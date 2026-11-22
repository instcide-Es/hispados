/* HLMM.EXE - (HL) prueba de lo de WINMM que pedía Half-Life 1: mmio (hacer
 * un WAV con mmioCreateChunk y mmioAscend, y leerlo con mmioDescend como
 * los juegos: FINDRIFF, FINDCHUNK «fmt » y «data», mmioRead, mmioSeek; el
 * acceso directo de mmioGetInfo y mmioAdvance; un archivo en memoria con
 * FOURCC_MEM), el mezclador (mixerOpen, sus líneas, sus controles, poner y
 * leer el volumen), mciGetDeviceID, y lo que no hay (waveIn, midiOut, aux)
 * dicho limpio. Una línea por cosa.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hl.h"

static void hace_wav(void)
{
	HANDLE h = mmioOpenA("HLMM.WAV", 0, 0x1000 | 1);    /* MMIO_CREATE | MMIO_WRITE */
	MMCKINFO riff, ck;
	WAVEFORMATEX f = { 1, 1, 11025, 22050, 2, 16, 0 };
	short m[101];
	int k;
	u32 r1, r2, r3, r4;
	for (k = 0; k < 101; k++)
		m[k] = (short)(k * 100);
	riff.tipo = FCC('W', 'A', 'V', 'E');
	riff.tam = 0;
	r1 = mmioCreateChunk(h, &riff, 0x20);              /* MMIO_CREATERIFF */
	ck.id = FCC('f', 'm', 't', ' ');
	ck.tam = 16;
	mmioCreateChunk(h, &ck, 0);
	mmioWrite(h, (const char *)&f, 16);
	r2 = mmioAscend(h, &ck, 0);
	ck.id = FCC('d', 'a', 't', 'a');
	ck.tam = 0;
	mmioCreateChunk(h, &ck, 0);
	mmioWrite(h, (const char *)m, 201);                  /* (impar: el relleno al subir) */
	r3 = mmioAscend(h, &ck, 0);
	r4 = mmioAscend(h, &riff, 0);
	printf("mmio_escribe: %s %u %u %u %u %u\n", h ? "si" : "no", r1, r2, r3, r4, ck.tam);
	printf("mmio_cierra: %u\n", mmioClose(h, 0));
}

int main(void)
{
	HANDLE h, mx = 0;
	MMCKINFO riff, ck;
	WAVEFORMATEX f;
	char b[64];
	u32 r;
	hace_wav();
	/* leerlo como un juego */
	h = mmioOpenA("HLMM.WAV", 0, 0x10000);                 /* MMIO_READ | MMIO_ALLOCBUF */
	riff.tipo = FCC('W', 'A', 'V', 'E');
	r = mmioDescend(h, &riff, 0, 0x20);                    /* MMIO_FINDRIFF */
	printf("mmio_riff: %u %u %u\n", r, riff.tam, riff.datos);
	ck.id = FCC('f', 'm', 't', ' ');
	r = mmioDescend(h, &ck, &riff, 0x10);                  /* MMIO_FINDCHUNK */
	memset(&f, 0, sizeof f);
	mmioRead(h, (char *)&f, ck.tam);
	printf("mmio_fmt: %u %u %u %u %u\n", r, ck.tam, f.hz, f.bits, f.canales);
	mmioAscend(h, &ck, 0);
	ck.id = FCC('d', 'a', 't', 'a');
	r = mmioDescend(h, &ck, &riff, 0x10);
	{
		short m[4];
		i32 l = mmioRead(h, (char *)m, 8), p;
		printf("mmio_data: %u %u %d %d %d\n", r, ck.tam, l, m[1], m[3]);
		p = mmioSeek(h, 0, 1);
		printf("mmio_seek: %d %d\n", p - (i32)ck.datos, mmioSeek(h, 0, 2));
	}
	/* lo que falta: CHUNKNOTFOUND (265) */
	mmioSeek(h, riff.datos + 4, 0);
	ck.id = FCC('L', 'I', 'S', 'T');
	printf("mmio_nohay: %u\n", mmioDescend(h, &ck, &riff, 0x10));
	/* el acceso directo: pchNext y mmioAdvance */
	{
		MMIOINFO i;
		mmioSeek(h, 0, 0);
		mmioGetInfo(h, &i, 0);
		printf("mmio_getinfo: %c%c%c%c %d\n", i.sig[0], i.sig[1], i.sig[2], i.sig[3], (int)(i.fin_l - i.sig));
		i.sig += 8;
		mmioSetInfo(h, &i, 0);
		printf("mmio_setinfo: %d\n", mmioSeek(h, 0, 1));
		printf("mmio_advance: %u\n", mmioAdvance(h, &i, 0));
	}
	mmioClose(h, 0);
	printf("mmio_nohay_archivo: %s\n", mmioOpenA("NOHAY.WAV", 0, 0) ? "MAL" : "nulo");
	printf("mmio_existe: %d %d\n", (int)mmioOpenA("HLMM.WAV", 0, 0x4000), (int)mmioOpenA("NOHAY.WAV", 0, 0x4000));
	/* en memoria (FOURCC_MEM) */
	{
		static char mem[64] = "RIFF\x0c\0\0\0WAVEabcd\x00\0\0\0";
		MMIOINFO i;
		memset(&i, 0, sizeof i);
		i.fcc = FCC('M', 'E', 'M', ' ');
		i.buf = (u8 *)mem;
		i.cb = 20;
		h = mmioOpenA(0, &i, 0);
		riff.tipo = FCC('W', 'A', 'V', 'E');
		r = mmioDescend(h, &riff, 0, 0x20);
		ck.id = FCC('a', 'b', 'c', 'd');
		printf("mmio_memoria: %s %u %u\n", h ? "si" : "no", r, mmioDescend(h, &ck, &riff, 0x10));
		mmioClose(h, 0);
	}
	printf("fourcc: %08x\n", mmioStringToFOURCCA("wave", 0x10));
	DeleteFileA("HLMM.WAV");
	/* el mezclador */
	{
		MIXERCAPSA c;
		MIXERLINEA l;
		MIXERCONTROLA ctl[2];
		MIXERLINECONTROLSA lc;
		MIXERCONTROLDETAILS d;
		u32 v[2], volid;
		printf("mixer: %u %u", mixerGetNumDevs(), mixerOpen(&mx, 0, 0, 0, 0));
		memset(&c, 0, sizeof c);
		r = mixerGetDevCapsA(0, &c, sizeof c);
		printf(" %u %u %s\n", r, c.ndest, c.nombre);
		memset(&l, 0, sizeof l);
		l.tam = sizeof l;
		l.tipo = 4;                                  /* los altavoces */
		r = mixerGetLineInfoA(mx, &l, 0x80000000 | 3);
		printf("mixer_linea: %u %u %u %u %s\n", r, l.linea, l.controles, l.conexiones, l.corto);
		memset(&l, 0, sizeof l);
		l.tam = sizeof l;
		l.tipo = 0x1008;                             /* la onda */
		r = mixerGetLineInfoA(mx, &l, 0x80000000 | 3);
		printf("mixer_onda: %u %x %x\n", r, l.linea, l.fl);
		memset(&l, 0, sizeof l);
		l.tam = sizeof l;
		l.tipo = 0x2003;                             /* el micrófono: no hay */
		printf("mixer_micro: %u\n", mixerGetLineInfoA(mx, &l, 0x80000000 | 3));
		memset(&lc, 0, sizeof lc);
		lc.tam = sizeof lc;
		lc.linea = 0;
		lc.idtipo = 0x50030001;                      /* el volumen */
		lc.n = 1;
		lc.tamc = sizeof ctl[0];
		lc.c = ctl;
		r = mixerGetLineControlsA(mx, &lc, 0x80000000 | 2);
		volid = ctl[0].id;
		printf("mixer_control: %u %u %d %d %s\n", r, volid, ctl[0].min, ctl[0].max, ctl[0].nombre);
		memset(&d, 0, sizeof d);
		d.tam = sizeof d;
		d.id = volid;
		d.canales = 1;
		d.tamd = 4;
		d.d = v;
		v[0] = 32768;
		r = mixerSetControlDetails(mx, &d, 0x80000000);
		v[0] = 0;
		printf("mixer_volumen: %u %u", r, mixerGetControlDetailsA(mx, &d, 0x80000000));
		printf(" %s\n", v[0] >= 32000 && v[0] <= 33500 ? "la mitad" : "MAL");
		v[0] = 65535;
		mixerSetControlDetails(mx, &d, 0x80000000);
		d.id = 9;
		printf("mixer_control_nohay: %u\n", mixerGetControlDetailsA(mx, &d, 0x80000000));
		printf("mixer_cierra: %u\n", mixerClose(mx));
	}
	printf("mcigetdeviceid: %u\n", mciGetDeviceIDA("cdaudio"));
	/* lo que no hay */
	{
		HANDLE wi = (HANDLE)1;
		WAVEFORMATEX wf = { 1, 1, 8000, 16000, 2, 16, 0 };
		r = waveInOpen(&wi, 0xFFFFFFFF, &wf, 0, 0, 0);
		printf("wavein: %u %u %s %u\n", waveInGetNumDevs(), r, wi ? "MAL" : "nulo", waveInGetDevCapsA(0, b, sizeof b));
		waveInGetErrorTextA(6, b, sizeof b);
		printf("wavein_texto: %s\n", b);
		wi = (HANDLE)1;
		r = midiOutOpen(&wi, 0xFFFFFFFF, 0, 0, 0);
		printf("midiout: %u %u %s %u\n", midiOutGetNumDevs(), r, wi ? "MAL" : "nulo", midiOutGetDevCapsA(0, b, sizeof b));
		printf("aux: %u %u %u\n", auxGetNumDevs(), auxGetDevCapsA(0, b, sizeof b), auxSetVolume(0, 0xFFFF));
	}
	printf("FIN HLMM\n");
	return 0;
}
