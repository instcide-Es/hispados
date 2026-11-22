/* REG.EXE - prueba del registro de WIN32.BIN (JUEGOS): lo que hace un juego
 * con él (guardar la resolución, el .DLL del motor, la clave del CD...):
 * crear y abrir claves de HKCU y HKLM, poner y leer valores (DWORD, texto,
 * binario, el sin nombre), lo que se pide con el búfer corto, enumerar,
 * RegQueryInfoKey, borrar, las de Unicode, HKCR, y que siga ahí al volver
 * a correr (se guarda en \USUARIOS\<usuario>\REGISTRO.REG).
 *   REG         todo, y lo deja guardado
 *   REG /LEE    lo que dejó la otra vez
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
i32 WINAPI RegQueryValueExW(HKEY, const u16 *, u32 *, u32 *, u8 *, u32 *);
static const char *CLAVE = "Software\\Valve\\Half-Life\\Settings";
int main(int argc, char **argv)
{
	HKEY k, s;
	u32 disp = 9, tipo = 0, tam, v, n;
	i32 r;
	char b[64];
	if (argc > 1 && !strcmp(argv[1], "/LEE")) {
		r = RegOpenKeyExA(HKEY_CURRENT_USER, CLAVE, 0, KEY_READ, &k);
		tam = 4;
		v = 0;
		RegQueryValueExA(k, "ScreenWidth", 0, &tipo, (u8 *)&v, &tam);
		printf("lee_otra_vez: %d %u %u\n", r, tipo, v);
		tam = sizeof b;
		r = RegQueryValueExA(k, "EngineDLL", 0, &tipo, (u8 *)b, &tam);
		printf("lee_texto: %d %s %u\n", r, b, tam);
		tam = sizeof b;
		r = RegQueryValueExA(k, "Borrado", 0, &tipo, (u8 *)b, &tam);
		printf("lee_borrado: %d\n", r);
		RegCloseKey(k);
		r = RegCreateKeyExA(HKEY_CURRENT_USER, CLAVE, 0, 0, 0, KEY_ALL_ACCESS, 0, &k, &disp);
		printf("crear_existente: %d %u\n", r, disp);
		RegCloseKey(k);
		printf("FIN REG\n");
		return 0;
	}
	r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\NoExiste", 0, KEY_READ, &k);
	printf("abrir_nohay: %d\n", r);
	r = RegCreateKeyExA(HKEY_CURRENT_USER, CLAVE, 0, 0, 0, KEY_ALL_ACCESS, 0, &k, &disp);
	printf("crear: %d %u\n", r, disp);
	v = 640;
	r = RegSetValueExA(k, "ScreenWidth", 0, REG_DWORD, (u8 *)&v, 4);
	v = 480;
	RegSetValueExA(k, "ScreenHeight", 0, REG_DWORD, (u8 *)&v, 4);
	RegSetValueExA(k, "EngineDLL", 0, REG_SZ, (const u8 *)"hw.dll", 7);
	RegSetValueExA(k, "Ruta", 0, REG_SZ, (const u8 *)"C:\\JUEGOS\\HL \"1\" / \xF1", 0);
	RegSetValueExA(k, "Clave", 0, REG_BINARY, (const u8 *)"\x01\x02\xFE\xFF", 4);
	RegSetValueExA(k, "Borrado", 0, REG_SZ, (const u8 *)"x", 2);
	RegSetValueA(k, "Sub", REG_SZ, "por defecto", 11);
	printf("poner: %d\n", r);
	tam = 0;
	r = RegQueryValueExA(k, "EngineDLL", 0, &tipo, 0, &tam);
	printf("tam_sin_bufer: %d %u %u\n", r, tipo, tam);
	tam = 3;
	r = RegQueryValueExA(k, "EngineDLL", 0, &tipo, (u8 *)b, &tam);
	printf("bufer_corto: %d %u\n", r, tam);
	tam = sizeof b;
	r = RegQueryValueExA(k, "EngineDLL", 0, &tipo, (u8 *)b, &tam);
	printf("texto: %d %s %u\n", r, b, tam);
	tam = sizeof b;
	RegQueryValueExA(k, "Ruta", 0, &tipo, (u8 *)b, &tam);
	printf("ruta: %s %u\n", b, tam);
	tam = sizeof b;
	r = RegQueryValueExA(k, "Clave", 0, &tipo, (u8 *)b, &tam);
	printf("binario: %d %u %u %02x%02x%02x%02x\n", r, tipo, tam, (u8)b[0], (u8)b[1], (u8)b[2], (u8)b[3]);
	{
		i32 l = sizeof b;
		r = RegQueryValueA(k, "Sub", b, &l);
		printf("sin_nombre: %d %s\n", r, b);
	}
	r = RegDeleteValueA(k, "Borrado");
	tam = sizeof b;
	printf("borrar_valor: %d %d\n", r, RegQueryValueExA(k, "Borrado", 0, &tipo, (u8 *)b, &tam));
	{
		u32 nsub, nval, maxn, maxd;
		RegQueryInfoKeyA(k, 0, 0, 0, &nsub, 0, 0, &nval, &maxn, &maxd, 0, 0);
		printf("info: %u %u %u %u\n", nsub, nval, maxn, maxd);
	}
	for (n = 0;; n++) {
		u32 nl = sizeof b;
		if (RegEnumValueA(k, n, b, &nl, 0, &tipo, 0, 0))
			break;
		printf("valor %u: %s %u\n", n, b, tipo);
	}
	{
		u16 w[32];
		static const u16 nw[] = { 'E', 'n', 'g', 'i', 'n', 'e', 'D', 'L', 'L', 0 };   /* (wchar_t de Windows: 16 bits) */
		tam = sizeof w;
		r = RegQueryValueExW(k, nw, 0, &tipo, (u8 *)w, &tam);
		printf("unicode: %d %u %c%c%c\n", r, tam, w[0], w[1], w[2]);
	}
	RegCloseKey(k);
	r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve", 0, KEY_READ, &k);
	{
		u32 nl = sizeof b;
		r = RegEnumKeyExA(k, 0, b, &nl, 0, 0, 0, 0);
		printf("enum_claves: %d %s %d\n", r, b, RegEnumKeyExA(k, 1, b, &nl, 0, 0, 0, 0));
	}
	RegCloseKey(k);
	r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\DirectX", 0, KEY_READ, &k);
	tam = sizeof b;
	RegQueryValueExA(k, "Version", 0, &tipo, (u8 *)b, &tam);
	printf("directx: %d %s\n", r, b);
	RegCloseKey(k);
	r = RegCreateKeyA(HKEY_CLASSES_ROOT, ".hlj", &k);
	RegSetValueExA(k, 0, 0, REG_SZ, (const u8 *)"HispaJuego", 11);
	RegCloseKey(k);
	r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Classes\\.hlj", 0, KEY_READ, &k);
	tam = sizeof b;
	RegQueryValueExA(k, "", 0, &tipo, (u8 *)b, &tam);
	printf("hkcr: %d %s\n", r, b);
	RegCloseKey(k);
	r = RegCreateKeyA(HKEY_CURRENT_USER, "Software\\Borrame\\Dentro", &k);
	RegCloseKey(k);
	r = RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Borrame");
	printf("borrar_clave: %d %d\n", r, RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Borrame\\Dentro", 0, KEY_READ, &k));
	printf("FIN REG\n");
	return 0;
}
