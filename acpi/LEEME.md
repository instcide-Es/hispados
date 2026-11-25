# acpi/ — ACPI en Hispa-486 (la batería)

Lo explica [docs/ACPI.md](../docs/ACPI.md). Aquí, qué hay en cada carpeta y
de dónde sale.

| Carpeta | Qué | De quién | Licencia |
|---|---|---|---|
| `nucleo/` | Las llamadas del residente, la tarea, la batería, el controlador embebido, lo que ACPICA pide al sistema (`osl.c`) y el entorno (`achispa.h`) | Hispa-DOS | GPL-2.0 o posterior |
| `pentium/` | El mismo núcleo para Hispa-Pentium: `ACPI.BIN`, un programa del anillo 3 (su entrada, su «máquina» con las llamadas al núcleo, el bucle que apaga y da la batería, y `hacer.sh`, que lo enlaza con los objetos de `build/acpi/`). Ver docs/PENTIUM.md, «Apagar de verdad y la batería» | Hispa-DOS | GPL-2.0 o posterior |
| `hispa64/` | El mismo núcleo para Hispa-64: `ACPICA64.BIN`, en el núcleo de 64 bits (anillo 0, enlazado para 0xFFFFFFFFC0000000): su entrada y cabecera, su «máquina», sus llamadas (la batería, apagar por `_S5`, las rutas de los `_PRT`, el brillo) y `hacer.sh`, que compila ACPICA y `nucleo/` para 64 bits en `build/acpi64/`. Ver docs/64/HISPA-64.md, sección 18 | Hispa-DOS | GPL-2.0 o posterior |
| `acpica/` | ACPICA: las tablas, el árbol de nombres y el intérprete de AML | Intel y contribuidores | Triple: la de Intel, BSD de 3 cláusulas o GPL-2.0, a elegir (el texto, al principio de cada archivo y en `acpica/LICENSE`). Aquí se usa bajo **GPL-2.0** |

## Los fuentes de ACPICA

Son los de la versión **R2025_04_04** (`20250404`), de
`https://github.com/acpica/acpica` (el archivo
`archive/refs/tags/R2025_04_04.tar.gz`, SHA-256
`9991ec103b3660d17715780406ee7409f705cf87ac55e9a32374affe1a6f275a`):

- `source/components/` → `acpica/`: `dispatcher`, `events`, `executer`,
  `hardware`, `namespace`, `parser`, `resources`, `tables` y `utilities`,
  enteros. No se han traído `debugger` ni `disassembler`.
- `source/include/*.h` → `acpica/include/`, y de `source/include/platform/`
  solo `acenv.h`, `acenvex.h`, `acgcc.h` y `acgccex.h`.
- `LICENSE` → `acpica/LICENSE`.

**Sin modificar, salvo dos archivos** (el cambio está marcado en ellos con
«Hispa-DOS (2026-11-18)»):

- `acpica/include/platform/acenv.h`: una rama más en la lista de sistemas,
  `#if defined(__HISPA__)` → `#include "achispa.h"` (que está en `nucleo/`).
- `acpica/include/platform/acenvex.h`: con `__HISPA__` no incluye nada.

No se compila `acpica/resources/rsdump.c` (solo sirve con el depurador de
ACPICA y sin él no compila).

Para pasar a otra versión de ACPICA: copiar las mismas carpetas encima,
repetir esos dos cambios y `make build/HSPACPI.BIN build/acpi_sim &&
tests/run_acpi_tests.sh`.
