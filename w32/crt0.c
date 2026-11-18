/* crt0.c - el arranque de los programas Hispa32 de consola hechos en C con
 * main(argc, argv): pide los argumentos a MSVCRT.DLL y sale con lo que
 * devuelva main. Se enlaza con el programa (tools/elf2pe.py; entrada «inicio»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
int main(int, char **);
void __getmainargs(int *, char ***, char ***, int, void *);
void exit(int);
void inicio(void) {
  int argc;
  char **argv, **env;
  __getmainargs(&argc, &argv, &env, 0, 0);
  exit(main(argc, argv));
}
