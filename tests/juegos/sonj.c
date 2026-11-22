/* SONJ.EXE - prueba del sonido de un juego (JUEGOS): un búfer de
 * DirectSound en bucle, con GetCurrentPosition (lo que mira el mezclador de
 * un juego para escribir por delante) y sin que el programa llame a nada
 * durante dos segundos (la mezcla va en su hilo: no se corta); waveOut con
 * CALLBACK_EVENT (los búferes vuelven hechos y el evento se enciende); y el
 * CD de sonido por MCI (mciSendString y mciSendCommand; en QEMU, sin disco).
 * Lo que ve lo escribe en SONJ.TXT, en su carpeta; lo que suena lo graba
 * QEMU (-audiodev wav) y lo mira nucleo32/sonido/oye.py: 440 Hz (DirectSound,
 * 2 s seguidos) y 660 Hz (waveOut, 1 s).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
typedef long HRESULT;
typedef struct { void **vt; } COM;
#define LLAMA(o, n, ...) ((HRESULT(WINAPI *)())((COM *)(o))->vt[n])((o), ##__VA_ARGS__)
HRESULT WINAPI DirectSoundCreate(void *, void **, void *);
typedef struct { u32 tam, banderas, octetos, r; WAVEFORMATEX *f; } DSBUFFERDESC;
static FILE *sal;
#define DI(...) do { fprintf(sal, __VA_ARGS__); fflush(sal); } while (0)
static i16 seno[100];
static void tono(i16 *d, u32 tramas, u32 canales, u32 hz, u32 frec)
{
	/* (sin coma flotante: una tabla de un seno, a 1/100 de periodo) */
	u32 k, fase = 0, paso = frec * 65536u / hz * 100;   /* (sin pasarse de 32 bits) */
	for (k = 0; k < tramas; k++, fase += paso) {
		i16 v = seno[(fase >> 16) % 100];
		d[k * canales] = v;
		if (canales == 2)
			d[k * canales + 1] = v;
	}
}
int main(void)
{
	char b[260];
	void *ds = 0, *buf = 0;
	WAVEFORMATEX f;
	DSBUFFERDESC d;
	u32 k;
	{
		/* un seno a mano (Bhaskara), sin math.h */
		for (k = 0; k < 100; k++) {
			i32 x = k < 50 ? (i32)k : (i32)k - 50, y = 4 * x * (50 - x) * 16000 / (2500 * 5 - x * (50 - x));
			seno[k] = (i16)(k < 50 ? y : -y);
		}
	}
	GetModuleFileNameA(0, b, sizeof b);
	strcpy(strstr(b, "SONJ.EXE"), "SONJ.TXT");
	sal = fopen(b, "w");
	if (!sal)
		return 1;
	/* 1. DirectSound: un búfer de 1 s en bucle */
	DI("dsound: %ld\n", DirectSoundCreate(0, &ds, 0));
	if (ds) {
		HRESULT e;
		u32 j, antes = 0, vueltas = 0, pos = 0, esc = 0, adelante = 1, t0;
		void *p1, *p2;
		u32 n1, n2;
		LLAMA(ds, 6, 0, 2);                    /* SetCooperativeLevel(DSSCL_PRIORITY) */
		memset(&f, 0, sizeof f);
		f.fmt = 1; f.canales = 2; f.hz = 44100; f.bits = 16; f.bloque = 4; f.octetos_s = 44100 * 4;
		memset(&d, 0, sizeof d);
		d.tam = sizeof d;
		d.banderas = 0x10080;                  /* DSBCAPS_GETCURRENTPOSITION2 | CTRLVOLUME */
		d.octetos = 44100 * 4;
		d.f = &f;
		e = LLAMA(ds, 3, &d, &buf, 0);         /* CreateSoundBuffer */
		DI("crear_bufer: %ld\n", e);
		if (buf && LLAMA(buf, 11, 0, 44100 * 4, &p1, &n1, &p2, &n2, 0) == 0) {   /* Lock */
			tono((i16 *)p1, n1 / 4, 2, 44100, 440);
			LLAMA(buf, 19, p1, n1, p2, n2);    /* Unlock */
		}
		Sleep(400);                             /* (un silencio antes: oye.py separa las ráfagas) */
		if (buf)
			DI("play: %ld\n", LLAMA(buf, 12, 0, 0, 1));   /* Play(DSBPLAY_LOOPING) */
		/* 2 s sin llamar a nada: la mezcla, en su hilo */
		t0 = GetTickCount();
		while (GetTickCount() - t0 < 2000)
			for (j = 0; j < 100000; j++)
				__asm__ volatile("");
		/* y 1,5 s mirando por dónde va, como un mezclador */
		t0 = GetTickCount();
		while (GetTickCount() - t0 < 1500) {
			LLAMA(buf, 4, &pos, &esc);          /* GetCurrentPosition */
			if (pos < antes)
				vueltas++;
			if ((esc + 44100 * 4 - pos) % (44100 * 4) > 44100 * 2)
				adelante = 0;
			antes = pos;
			Sleep(10);
		}
		DI("vueltas_en_1500ms: %s %u\n", vueltas >= 1 && vueltas <= 2 ? "bien" : "mal", vueltas);
		DI("escribir_delante: %s\n", adelante ? "si" : "no");
		{
			u32 est = 0;
			LLAMA(buf, 9, &est);                /* GetStatus */
			DI("estado: %u\n", est);
		}
		LLAMA(buf, 18);                         /* Stop */
	}
	Sleep(500);
	/* 2. waveOut con CALLBACK_EVENT: cuatro trozos de 1/4 s */
	{
		static i16 datos[4][5512];
		WAVEHDR h[4];
		HANDLE wo = 0, ev = CreateEventA(0, 0, 0, 0);
		u32 e, hechos = 0, t0, vueltas = 0;
		memset(&f, 0, sizeof f);
		f.fmt = 1; f.canales = 1; f.hz = 22050; f.bits = 16; f.bloque = 2; f.octetos_s = 44100;
		e = waveOutOpen(&wo, 0xFFFFFFFF, &f, (u32)ev, 0, 0x50000);   /* WAVE_MAPPER, CALLBACK_EVENT */
		DI("waveout: %u %u\n", waveOutGetNumDevs() > 0, e);
		t0 = GetTickCount();
		for (k = 0; k < 4; k++) {
			tono(datos[k], 5512, 1, 22050, 660);
			memset(&h[k], 0, sizeof h[k]);
			h[k].datos = (char *)datos[k];
			h[k].largo = sizeof datos[k];
			waveOutPrepareHeader(wo, &h[k], sizeof h[k]);
			waveOutWrite(wo, &h[k], sizeof h[k]);
		}
		while (GetTickCount() - t0 < 4000) {
			WaitForSingleObject(ev, 500);
			vueltas++;
			for (k = 0, hechos = 0; k < 4; k++)
				hechos += h[k].flags & 1;
			if (hechos == 4)
				break;
		}
		DI("waveout_hechos: %u %s\n", hechos, GetTickCount() - t0 >= 800 && GetTickCount() - t0 < 2500 ? "a su tiempo" : "MAL");
		for (k = 0; k < 4; k++)
			waveOutUnprepareHeader(wo, &h[k], sizeof h[k]);
		waveOutClose(wo);
	}
	/* 3. el CD de sonido (sin disco en QEMU) */
	{
		char r[64], t[128];
		u32 e;
		r[0] = 0;
		e = mciSendStringA("open cdaudio alias cd wait", r, sizeof r, 0);
		DI("cd_open: %u\n", e);
		e = mciSendStringA("status cd media present", r, sizeof r, 0);
		DI("cd_disco: %u %s\n", e, r);
		mciSendStringA("set cd time format tmsf", r, sizeof r, 0);
		r[0] = 0;
		e = mciSendStringA("status cd number of tracks", r, sizeof r, 0);
		mciGetErrorStringA(e, t, sizeof t);
		DI("cd_pistas: %u [%s] %s\n", e, r, t);
		e = mciSendStringA("play cd from 2 notify", r, sizeof r, 0);
		DI("cd_play: %u\n", e);
		e = mciSendStringA("status cd mode", r, sizeof r, 0);
		DI("cd_modo: %u %s\n", e, r);
		mciSendStringA("close cd", r, sizeof r, 0);
		{
			struct { u32 cb, id; const char *tipo, *elem, *alias; } op = { 0, 0, "cdaudio", 0, 0 };
			struct { u32 cb, ret, item, pista; } st = { 0, 0, 5, 0 };
			e = mciSendCommandA(0, 0x803, 0x2000, (u32)&op);    /* MCI_OPEN, MCI_OPEN_TYPE */
			DI("cd_mci_open: %u %s\n", e, op.id ? "con numero" : "sin numero");
			e = mciSendCommandA(op.id, 0x814, 0x100, (u32)&st);  /* MCI_STATUS, MEDIA_PRESENT */
			DI("cd_mci_status: %u %u\n", e, st.ret);
			DI("cd_mci_close: %u\n", mciSendCommandA(op.id, 0x804, 0, 0));
		}
	}
	DI("FIN SONJ\n");
	fclose(sal);
	return 0;
}
