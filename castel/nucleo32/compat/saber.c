/* saber.c - (COMPAT) lo que se sabe de cada DLL de Windows (para qué sirve,
 * en llano) y de los archivos que no hacen falta para usar un programa
 * (instaladores, servidores dedicados, lo de las tarjetas 3dfx...). Así el
 * Asistente dice «faltan 3, de sonido» en vez de una lista de nombres.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "texto.h"

static const c_saber SABER[] = {
	{ "KERNEL32.DLL", "Sistema", "lo básico de Windows: archivos, memoria, hilos y la hora", 0, NULL },
	{ "NTDLL.DLL", "Sistema", "lo de dentro de Windows NT, 2000 y XP", 0,
	  "Un programa que la llama directamente suele ser solo para Windows NT o XP." },
	{ "USER32.DLL", "Ventanas", "las ventanas, los menús, el teclado y el ratón", 0, NULL },
	{ "GDI32.DLL", "Dibujo", "dibujar en la pantalla: letras, rayas e imágenes", 0, NULL },
	{ "ADVAPI32.DLL", "Sistema", "el registro de Windows, los usuarios y la seguridad", 0, NULL },
	{ "SHELL32.DLL", "Escritorio", "las carpetas especiales y abrir archivos con su programa", 0, NULL },
	{ "SHLWAPI.DLL", "Escritorio", "ayudas para rutas y textos del explorador", 0, NULL },
	{ "COMDLG32.DLL", "Ventanas", "los cuadros de abrir, guardar e imprimir", 0, NULL },
	{ "COMCTL32.DLL", "Ventanas", "los controles comunes: listas, árboles, barras de progreso", 0, NULL },
	{ "WSOCK32.DLL", "Red", "la red (Internet y las partidas en red)", 0, NULL },
	{ "WS2_32.DLL", "Red", "la red, la versión 2 (Internet y las partidas en red)", 0, NULL },
	{ "WININET.DLL", "Red", "páginas web y descargas", 0, NULL },
	{ "URLMON.DLL", "Red", "abrir y bajar direcciones de Internet", 0, NULL },
	{ "IPHLPAPI.DLL", "Red", "los datos de la red (la dirección, las tarjetas)", 0, NULL },
	{ "MPR.DLL", "Red", "las unidades de red de Windows", 1, NULL },
	{ "NETAPI32.DLL", "Red", "la red de Windows (dominios y carpetas compartidas)", 1, NULL },
	{ "WINMM.DLL", "Sonido", "el sonido, la música MIDI, el CD de música y el reloj fino", 0, NULL },
	{ "MSACM32.DLL", "Sonido", "comprimir y descomprimir sonido (ADPCM, MP3...)", 0, NULL },
	{ "DSOUND.DLL", "Sonido", "el sonido de DirectX (DirectSound)", 0, NULL },
	{ "A3D.DLL", "Sonido", "el sonido 3D de las tarjetas Aureal (de los 90)", 1, "Es opcional: el juego suena sin ella." },
	{ "A3DAPI.DLL", "Sonido", "el sonido 3D de las tarjetas Aureal (de los 90)", 1, "Es opcional: el juego suena sin ella." },
	{ "EAX.DLL", "Sonido", "los efectos de sonido de Creative (EAX)", 1, "Es opcional." },
	{ "MSS32.DLL", "Sonido", "Miles Sound System (el sonido de muchos juegos)", 0, "Va con el juego: tiene que estar en su carpeta." },
	{ "XAUDIO2_7.DLL", "Sonido", "el sonido de DirectX de 2010 (XAudio2)", 0, NULL },
	{ "X3DAUDIO1_7.DLL", "Sonido", "el sonido en 3D de DirectX (las cuentas)", 0, NULL },
	{ "DDRAW.DLL", "Gráficos", "los gráficos de DirectX en 2D (DirectDraw)", 0, NULL },
	{ "D3DIM.DLL", "Gráficos 3D", "Direct3D (los gráficos 3D de DirectX)", 0, "Hispa-Pentium aún no tiene Direct3D." },
	{ "D3DIM700.DLL", "Gráficos 3D", "Direct3D 7 (los gráficos 3D de DirectX)", 0, "Hispa-Pentium aún no tiene Direct3D." },
	{ "D3D8.DLL", "Gráficos 3D", "Direct3D 8 (los gráficos 3D de DirectX)", 0, "Hispa-Pentium aún no tiene Direct3D." },
	{ "D3D9.DLL", "Gráficos 3D", "Direct3D 9 (los gráficos 3D de DirectX)", 0, "Hispa-Pentium aún no tiene Direct3D." },
	{ "D3DRM.DLL", "Gráficos 3D", "Direct3D en modo retenido (de 1996)", 0, "Hispa-Pentium aún no tiene Direct3D." },
	{ "OPENGL32.DLL", "Gráficos 3D", "los gráficos 3D de OpenGL", 0, NULL },
	{ "GLU32.DLL", "Gráficos 3D", "ayudas de OpenGL", 0, NULL },
	{ "GLIDE.DLL", "Gráficos 3D", "Glide, de las tarjetas 3dfx Voodoo (de los 90)", 1,
	  "Solo para las tarjetas Voodoo: el juego suele traer también OpenGL o dibujo por programa." },
	{ "GLIDE2X.DLL", "Gráficos 3D", "Glide 2, de las tarjetas 3dfx Voodoo (de los 90)", 1,
	  "Solo para las tarjetas Voodoo: el juego suele traer también OpenGL o dibujo por programa." },
	{ "GLIDE3X.DLL", "Gráficos 3D", "Glide 3, de las tarjetas 3dfx Voodoo (de los 90)", 1,
	  "Solo para las tarjetas Voodoo: el juego suele traer también OpenGL o dibujo por programa." },
	{ "3DFXGL.DLL", "Gráficos 3D", "el OpenGL de las tarjetas 3dfx Voodoo", 1, "Solo para las tarjetas Voodoo." },
	{ "DINPUT.DLL", "Mandos", "el teclado, el ratón y los mandos de DirectX (DirectInput)", 0, NULL },
	{ "DINPUT8.DLL", "Mandos", "el teclado, el ratón y los mandos de DirectX 8 (DirectInput 8)", 0, NULL },
	{ "XINPUT1_3.DLL", "Mandos", "los mandos de la Xbox 360", 0, NULL },
	{ "HID.DLL", "Mandos", "los mandos y aparatos USB", 0, NULL },
	{ "DPLAY.DLL", "Red", "las partidas en red de DirectX (DirectPlay)", 0, NULL },
	{ "DPLAYX.DLL", "Red", "las partidas en red de DirectX (DirectPlay)", 0, NULL },
	{ "DPNET.DLL", "Red", "las partidas en red de DirectX 8 (DirectPlay 8)", 0, NULL },
	{ "WINSPOOL.DRV", "Impresora", "la impresora", 0, NULL },
	{ "WINSPOOL.DLL", "Impresora", "la impresora", 0, NULL },
	{ "VERSION.DLL", "Sistema", "leer la versión de los archivos", 0, NULL },
	{ "OLE32.DLL", "Objetos", "los objetos de Windows (COM): DirectX, arrastrar y soltar...", 0, NULL },
	{ "OLEAUT32.DLL", "Objetos", "la automatización (COM): textos y números de Visual Basic y de las macros", 0, NULL },
	{ "OLEPRO32.DLL", "Objetos", "los objetos de Visual Basic (imágenes y letras)", 0, NULL },
	{ "RPCRT4.DLL", "Objetos", "las llamadas entre programas de Windows (RPC)", 0, NULL },
	{ "MSVCRT.DLL", "C y C++", "la biblioteca de C de Visual C++ 6", 0, NULL },
	{ "MSVCRT20.DLL", "C y C++", "la biblioteca de C de Visual C++ 2", 0, NULL },
	{ "MSVCRT40.DLL", "C y C++", "la biblioteca de C de Visual C++ 4", 0, NULL },
	{ "MSVCP60.DLL", "C y C++", "la biblioteca de C++ de Visual C++ 6", 0, NULL },
	{ "MSVCR70.DLL", "C y C++", "la biblioteca de C de Visual C++ 2002", 0, NULL },
	{ "MSVCR71.DLL", "C y C++", "la biblioteca de C de Visual C++ 2003", 0, "Suele ir con el programa, en su carpeta." },
	{ "MSVCP71.DLL", "C y C++", "la biblioteca de C++ de Visual C++ 2003", 0, "Suele ir con el programa, en su carpeta." },
	{ "MSVCR80.DLL", "C y C++", "la biblioteca de C de Visual C++ 2005", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MSVCP80.DLL", "C y C++", "la biblioteca de C++ de Visual C++ 2005", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MSVCR90.DLL", "C y C++", "la biblioteca de C de Visual C++ 2008", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MSVCP90.DLL", "C y C++", "la biblioteca de C++ de Visual C++ 2008", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MSVCR100.DLL", "C y C++", "la biblioteca de C de Visual C++ 2010", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MSVCP100.DLL", "C y C++", "la biblioteca de C++ de Visual C++ 2010", 0, "Es de los componentes de Microsoft (botón «Componentes de Microsoft»)." },
	{ "MFC42.DLL", "C y C++", "las ventanas de Visual C++ 6 (MFC)", 0, NULL },
	{ "MFC80.DLL", "C y C++", "las ventanas de Visual C++ 2005 (MFC)", 0, NULL },
	{ "MFC90.DLL", "C y C++", "las ventanas de Visual C++ 2008 (MFC)", 0, NULL },
	{ "MFC100.DLL", "C y C++", "las ventanas de Visual C++ 2010 (MFC)", 0, NULL },
	{ "MSVBVM60.DLL", "Visual Basic", "lo que necesitan los programas de Visual Basic 6", 0, NULL },
	{ "IMM32.DLL", "Escritura", "escribir en chino, japonés o coreano (métodos de entrada)", 1, "En español no hace falta." },
	{ "AVIFIL32.DLL", "Vídeo", "los vídeos AVI", 0, NULL },
	{ "MSVFW32.DLL", "Vídeo", "los vídeos de Video for Windows", 0, NULL },
	{ "BINKW32.DLL", "Vídeo", "los vídeos Bink de los juegos", 0, "Va con el juego: tiene que estar en su carpeta." },
	{ "SMACKW32.DLL", "Vídeo", "los vídeos Smacker de los juegos", 0, "Va con el juego: tiene que estar en su carpeta." },
	{ "SETUPAPI.DLL", "Instalación", "instalar aparatos y controladores", 1, "Lo usan los instaladores, no los programas." },
	{ "CFGMGR32.DLL", "Instalación", "los aparatos del equipo", 1, NULL },
	{ "MSI.DLL", "Instalación", "Windows Installer (los .MSI)", 1, "Lo usan los instaladores, no los programas." },
	{ "DSETUP.DLL", "Instalación", "el instalador de DirectX", 1, "Solo para instalar DirectX: no hace falta." },
	{ "SFC.DLL", "Instalación", "la protección de archivos de Windows", 1, NULL },
	{ "SECUR32.DLL", "Seguridad", "la seguridad de la red de Windows", 0, NULL },
	{ "CRYPT32.DLL", "Seguridad", "los certificados y las firmas", 0, NULL },
	{ "WINTRUST.DLL", "Seguridad", "comprobar la firma de los programas", 1, NULL },
	{ "DBGHELP.DLL", "Depuración", "los informes de los fallos", 1, "Solo se usa si el programa falla." },
	{ "IMAGEHLP.DLL", "Depuración", "mirar los programas por dentro", 1, NULL },
	{ "PSAPI.DLL", "Sistema", "la lista de los programas en marcha", 0, NULL },
	{ "POWRPROF.DLL", "Sistema", "el ahorro de energía", 1, NULL },
	{ "STEAM_API.DLL", "Steam", "Steam (la tienda de Valve)", 1, "Es de la versión de Steam: la del CD no la usa." },
	{ "STEAM.DLL", "Steam", "Steam (la tienda de Valve)", 1, "Es de la versión de Steam: la del CD no la usa." },
	{ "IFC22.DLL", "Mandos", "la vibración de los mandos Immersion", 1, "Es opcional." },
	{ "IFC23.DLL", "Mandos", "la vibración de los mandos Immersion", 1, "Es opcional." },
};

const c_saber *saber_dll(const char *dll)
{
	u32 k;
	char m[48];

	copia(m, dll, sizeof m);
	a_mayusculas(m);
	if (!strchr(m, '.'))
		anade(m, ".DLL", sizeof m);
	for (k = 0; k < sizeof SABER / sizeof SABER[0]; k++)
		if (iguales(SABER[k].dll, m))
			return &SABER[k];
	/* las de DirectX 9 de ayuda (D3DX9_24 ... D3DX9_43) */
	if (empieza(m, "D3DX9_") || empieza(m, "D3DX8") || empieza(m, "D3DX10") || empieza(m, "D3DX11")) {
		static const c_saber d3dx = { "D3DX9", "Gráficos 3D", "ayudas de Direct3D (D3DX)", 0, "Sin Direct3D no sirven todavía." };
		return &d3dx;
	}
	return NULL;
}

/* ---- los archivos que no hacen falta ---- */
static const char *const MOTIVOS[] = {
	"",
	"es un instalador o un desinstalador: no hace falta para usar el programa",
	"es el servidor dedicado (para montar partidas en red sin jugar): no hace falta para jugar",
	"es para las tarjetas 3dfx Voodoo (Glide): el juego tiene otras maneras de dibujar",
	"es un actualizador: no hace falta para usar el programa",
	"es el que informa de los fallos: no hace falta para usar el programa",
	"es de Steam: la versión del CD no lo usa",
	"es un editor o una herramienta aparte: no hace falta para usar el programa",
};

const char *saber_motivo(int si) { return si > 0 && si < (int)(sizeof MOTIVOS / sizeof MOTIVOS[0]) ? MOTIVOS[si] : ""; }

/* ¿casa «n» (en mayúsculas) con el patrón? (con «*» al principio, al final o los dos) */
static int casa(const char *n, const char *pat)
{
	u32 ln = largo(n), lp = largo(pat);
	int ini = pat[0] == '*', fin = lp && pat[lp - 1] == '*';
	char p[40];

	copia(p, pat + ini, sizeof p);
	if (fin)
		p[largo(p) - 1] = 0;
	lp = largo(p);
	if (ini && fin)
		return busca(n, p) != NULL;
	if (ini)
		return ln >= lp && iguales(n + ln - lp, p);
	if (fin)
		return empieza(n, p);
	return iguales(n, p);
}

int saber_archivo(const char *nombre, const char **por_que)
{
	static const struct { const char *pat; int si; } T[] = {
		{ "SETUP*.EXE", SI_INSTALADOR }, { "*SETUP.EXE", SI_INSTALADOR }, { "INSTALL*.EXE", SI_INSTALADOR },
		{ "UNINST*.EXE", SI_INSTALADOR }, { "UNINS0*.EXE", SI_INSTALADOR }, { "UNWISE*.EXE", SI_INSTALADOR },
		{ "_ISDEL.EXE", SI_INSTALADOR }, { "ISUNINST.EXE", SI_INSTALADOR }, { "_SETUP.DLL", SI_INSTALADOR },
		{ "_INST*.EXE", SI_INSTALADOR }, { "IKERNEL.EXE", SI_INSTALADOR }, { "DXSETUP.EXE", SI_INSTALADOR },
		{ "DSETUP*.DLL", SI_INSTALADOR }, { "AUTORUN.EXE", SI_INSTALADOR }, { "VCREDIST*.EXE", SI_INSTALADOR },
		{ "DIRECTX*.EXE", SI_INSTALADOR }, { "UNINSTAL.EXE", SI_INSTALADOR },
		{ "HLDS.EXE", SI_SERVIDOR }, { "*DEDICATED*", SI_SERVIDOR },
		{ "GLIDE*.DLL", SI_GLIDE }, { "3DFX*.DLL", SI_GLIDE }, { "*GLIDE*", SI_GLIDE },
		{ "*UPDATE*.EXE", SI_ACTUALIZA }, { "*PATCH*.EXE", SI_ACTUALIZA }, { "*UPDAT.EXE", SI_ACTUALIZA },
		{ "*CRASH*", SI_FALLOS }, { "*BUGREP*", SI_FALLOS }, { "DBGHELP.DLL", SI_FALLOS },
		{ "STEAM*", SI_STEAM },
		{ "*EDITOR*.EXE", SI_EDITOR }, { "WORLDCRAFT*.EXE", SI_EDITOR }, { "HAMMER.EXE", SI_EDITOR },
	};
	char m[64];
	u32 k;

	copia(m, sin_carpeta(nombre), sizeof m);
	a_mayusculas(m);
	for (k = 0; k < sizeof T / sizeof T[0]; k++)
		if (casa(m, T[k].pat)) {
			if (por_que)
				*por_que = MOTIVOS[T[k].si];
			return T[k].si;
		}
	if (por_que)
		*por_que = "";
	return SI_NADA;
}

void saber_marca(c_informe *i)
{
	int k;

	for (k = 0; k < i->n_a; k++)
		i->a[k].sin_importancia = saber_archivo(i->a[k].nombre, NULL);
}
