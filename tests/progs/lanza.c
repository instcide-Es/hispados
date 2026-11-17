/* LANZA.EXE - prueba de lanzar otros programas desde un programa Win32 (HW32,
 * Hispa-486): system() con una orden de DOS y _spawnvp con otro programa de
 * Windows (PEHOLA, que acaba con 42), por CreateProcessA de KERNEL32.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
int printf(const char *, ...); int system(const char *); int _spawnvp(int, const char *, const char *const *); void exit(int);
void inicio(void) {
  int r = system("ECHO desde system");
  printf("system: %d\n", r);
  const char *a[] = { "A:\\BIN\\HW32", "A:\\BIN\\PEHOLA.EXE", "dos palabras", 0 };
  r = _spawnvp(0, "A:\\BIN\\HW32", a);
  printf("spawn PEHOLA: %d\n", r);
  printf("fin de LANZA\n");
  exit(5);
}
