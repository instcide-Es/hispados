/* wingui.c - prueba de la fase 3a de Hispa-486 (docs/486/HISPA-486.md): un
 * programa Win32 de VENTANAS (subsistema 2) que corre dentro de Hispa
 * Ventanas. Pregunta con MessageBoxA (sí o no), enseña lo contestado y su
 * línea de órdenes en otro MessageBoxA y acaba con ExitProcess.
 * Con -DFALTA llama después a una función que Hispa Ventanas aún no tiene
 * (CreateWindowExA): tiene que decirlo y terminar el programa.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
typedef unsigned int u32;
#define WINAPI __attribute__((stdcall))
int WINAPI MessageBoxA(void *, const char *, const char *, u32);
void WINAPI ExitProcess(u32);
char *WINAPI GetCommandLineA(void);
void *WINAPI GetModuleHandleA(const char *);
char *WINAPI lstrcpyA(char *, const char *);
char *WINAPI lstrcatA(char *, const char *);
int WINAPI lstrlenA(const char *);
void *WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, void *, void *, void *, void *);
#define MB_OK 0
#define MB_YESNO 4
#define IDYES 6
#define IDNO 7

static char texto[300];
static int cuenta;              /* (en .bss: tiene que llegar a cero) */

void inicio(void) {
  int r = MessageBoxA(0, "\xBF" "Le gusta Hispa-486?\nConteste s\xED" " o no.", "Prueba de Win32", MB_YESNO);
  lstrcpyA(texto, r == IDYES ? "Ha dicho que s\xED" "." : r == IDNO ? "Ha dicho que no." : "Respuesta rara.");
  lstrcatA(texto, "\r\n");
  lstrcatA(texto, GetCommandLineA());
  /* el módulo es la ImageBase, la .bss llega a cero y lstrlenA cuenta bien */
  if (GetModuleHandleA(0) == (void *)0x400000 && cuenta == 0 && lstrlenA("cinco") == 5)
    lstrcatA(texto, "\nTodo bien.");
  else
    lstrcatA(texto, "\nALGO MAL.");
  cuenta++;
#ifdef FALTA
  MessageBoxA(0, "Ahora, una que falta.", 0, MB_OK);
  CreateWindowExA(0, "BUTTON", "x", 0, 0, 0, 10, 10, 0, 0, 0, 0);
  MessageBoxA(0, "ESTO NO DEBE SALIR", "Prueba de Win32", MB_OK);
#else
  MessageBoxA(0, texto, "Prueba de Win32", MB_OK);
#endif
  ExitProcess(r);
}
