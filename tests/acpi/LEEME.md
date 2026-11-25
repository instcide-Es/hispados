# tests/acpi/ — pruebas de la batería por ACPI

Las usa `tests/run_acpi_tests.sh` (ver [docs/ACPI.md](../../docs/ACPI.md),
«Las pruebas»).

| Qué | De quién |
|---|---|
| `anfitrion.c`, `maquina_sim.c`, `sim.h`: el núcleo ACPI en el equipo de compilación, con un portátil de mentira (`build/acpi_sim`) | Hispa-DOS, GPL-2.0 o posterior |
| `bateria-qemu.asl` y su compilado `bateria-qemu.aml` (con `iasl` 20230628): la batería de mentira para QEMU | Hispa-DOS, GPL-2.0 o posterior |
| `brillo-qemu.asl`, `haz_brillo.py` y su `brillo-qemu.aml` (hecho por `haz_brillo.py`, sin iasl): el brillo de mentira para QEMU (Hispa-64, etapa 11 b) | Hispa-DOS, GPL-2.0 o posterior |
| `toshiba-nb250/*.BIN`: FACP, FACS, DSDT y SSDT1-3 del Toshiba NB250-10N de Carlos, volcadas por EQUIPO el 15/11/2026 | Tablas del firmware del equipo (Toshiba / Phoenix): datos de prueba, sin modificar |
| `hp-mini-5103/*.BIN`: FACP, FACS, DSDT y SSDT1-4 del HP Mini 5103, volcadas por EQUIPO el 18/11/2026 | Tablas del firmware del equipo (Hewlett-Packard): datos de prueba, sin modificar |
| `*/escenario.txt`, `toshiba-nb250/mudo.txt`: qué hay en el controlador embebido simulado en cada lectura | Hispa-DOS |

Las tablas de los dos equipos son lo que su firmware entrega a cualquier
sistema operativo al arrancar; no llevan datos personales (no se han traído la
SLIC ni la SMBIOS, que sí llevan números de serie o de licencia). Están aquí
solo para probar el intérprete contra el AML de verdad de esos fabricantes.
No forman parte de lo que se instala.
