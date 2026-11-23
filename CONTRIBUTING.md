# Cómo colaborar con Hispa-DOS

¡Gracias por pasarte! La familia Hispa-DOS la hace hoy muy poca gente y **cualquier ayuda cuenta**, también si no sabes programar.
*English speakers: see [the short version below](#english).*

La familia, de más antiguo a más moderno: **Hispa-8086** (PC, XT y AT), **Hispa-386** e **Hispa-486** (DOS compatible con MS-DOS 6.22, con Hispa Ventanas), **Hispa-Pentium** (núcleo propio de 32 bits escrito en Castel, gama Windows XP) e **Hispa-64** (los PC de hoy, en sus primeros pasos). **Castel** es nuestro lenguaje de programación, en español. El estado de cada uno, al día: [hispados.es/estado.html](https://hispados.es/estado.html).

## Sin programar

- **Pruébalo en un equipo de verdad.** Es lo que más falta hace: casi todo pasa las pruebas en emuladores, pero cada portátil tiene sus manías (USB, touchpad, sonido, red, Wi-Fi, ACPI, UEFI). Sirve igual si **todo funciona**.
  - **Hispa-Pentium:** graba la memoria USB de la [última versión](https://github.com/instcide-Es/hispados/releases) con Rufus (modo «imagen DD») o balenaEtcher y arráncala por BIOS o por UEFI. Arranca con «registro» y adjunta `X:\ARRANQUE.TXT` (y `X:\ACPI.TXT` si lo hay).
  - **El Termómetro de Hispa-Pentium:** una memoria USB que arranca sola, carga todos los núcleos del procesador cinco minutos y apunta temperaturas, ventiladores, frecuencia y batería. Adjunta `X:\TERMO\INFORME.TXT` y el `.LOG`. La carpeta `X:\TERMO\EQUIPO` (la ficha técnica del equipo: tablas ACPI y lista PCI) ayuda mucho a hacer drivers nuevos; no lleva datos personales ni la clave de Windows.
  - **Hispa-386/486:** arranca la memoria USB o el disquete, ejecuta `DIAGNOSTICO` y adjunta la carpeta `\INFORMES\EQUIPOnn`.
  - Abre una *issue* con la plantilla **«Informe de un equipo real»**.
- **Pruébalo en el navegador**, en [hispados.es](https://hispados.es), y cuenta lo que no se entiende o falla.
- **Juegos y programas:** dinos cuáles van y cuáles no (de DOS en Hispa-386; de Windows de 32 bits en Hispa-Pentium), y en qué equipo o emulador.
- **Textos y documentación:** erratas, explicaciones confusas, traducciones (las pantallas están en español; `docs/` agradece una mano en inglés).
- **Difúndelo:** una ⭐ en el repositorio, o contarlo en foros de retroinformática y de sistemas operativos, ayuda a que lo encuentre más gente.

## Programando

### Preparar el entorno

En Debian, Ubuntu o WSL:

```sh
sudo apt install nasm mtools qemu-system-x86 ovmf python3 gcc
# Hispa-386/486
make          # crea la imagen de disquete en build/
make run      # la arranca en QEMU
make test     # pruebas de principio a fin (por el puerto serie)
# Castel e Hispa-Pentium
cd castel
./pruebas.sh                 # la batería entera (larga)
SOLO=usb ./pruebas.sh        # solo una parte: mira la cabecera de pruebas.sh
```

Las pruebas de Hispa-386/486 viven en [`tests/`](tests/) (un `run_*_tests.sh` por parte); las de Castel e Hispa-Pentium, en [`castel/pruebas.sh`](castel/pruebas.sh), por partes.

### Por dónde empezar

- Las *issues* marcadas **[`good first issue`](https://github.com/instcide-Es/hispados/labels/good%20first%20issue)** están pensadas para empezar; las de **[`help wanted`](https://github.com/instcide-Es/hispados/labels/help%20wanted)**, para quien ya se maneja.
- Lo que viene: [docs/ROADMAP.md](docs/ROADMAP.md) y, para Hispa-Pentium, [docs/pentium/METAS.md](docs/pentium/METAS.md). Hacia dónde va la familia: [docs/VISION.md](docs/VISION.md). Cómo está hecho Hispa-Pentium: [docs/PENTIUM.md](docs/PENTIUM.md). El lenguaje: [castel/README.md](castel/README.md).
- Si quieres meterte en algo grande, abre antes una *issue* contándolo: así nadie hace el mismo trabajo dos veces.

### Normas (importantes)

1. **Nada de código de Microsoft**: ni del MS-DOS que Microsoft publicó, ni de desensamblados, ni de filtraciones. Todo se escribe a partir de documentación pública (Ralf Brown's Interrupt List, manuales de Intel y AMD, especificaciones de USB, ACPI, UEFI, HD Audio...). El código de otros proyectos libres solo entra con su licencia compatible (GPL-2.0 o posterior) y dicho en la PR; si te has inspirado en algo, dilo también.
2. **Lo nuevo lleva su prueba**: en Hispa-386, una comprobación en `tests/` por cada función nueva de INT 21h; en Hispa-Pentium, una parte de `castel/pruebas.sh` o una comprobación en la que toque. La batería tiene que seguir pasando.
3. **Nada de programas de terceros con derechos** (Windows, juegos comerciales, ROM...) en el repositorio: cada uno usa su copia legal.
4. **En español**: mensajes en pantalla, nombres de órdenes, comentarios y textos de commit. Si no lo dominas, escribe como puedas; lo pulimos juntos.
5. **Sin datos personales** en informes, registros ni capturas (correos, contraseñas, claves de licencia): revísalos antes de subirlos.
6. Licencia: lo que aportas se publica con la **GPL-2.0 o posterior**, como el resto («Copyright (C) 2026 Carlos Sanz y contribuidores»).

### Enviar los cambios

1. Haz un *fork* y una rama a partir de la rama principal (`hispados-0.9`).
2. Cambios pequeños y con un propósito cada uno; mejor varias PR cortas que una enorme.
3. Explica en la PR qué cambia, por qué y **dónde lo has probado** (QEMU, VirtualBox, 86Box, equipo real...).
4. No hace falta que esté perfecto: una PR a medias, marcada como borrador, también vale para pedir opinión.

## Dudas

Abre una *issue* con la etiqueta `question`. No hay preguntas tontas: el PC tiene cuarenta años de rarezas.

Todos los que participan siguen el [código de conducta](CODE_OF_CONDUCT.md).

---

<a id="english"></a>
## English (short version)

Contributions are very welcome. The most useful thing right now is **testing on real hardware**: boot a [release](https://github.com/instcide-Es/hispados/releases) (Hispa-Pentium boots from USB by BIOS or UEFI; its "Termómetro" stick runs a 5-minute stress test and writes `X:\TERMO\INFORME.TXT`) and open an issue with the "Informe de un equipo real" template. To build: `sudo apt install nasm mtools qemu-system-x86 ovmf python3 gcc`; Hispa-386: `make`, `make run`, `make test`; Castel and Hispa-Pentium: `cd castel && ./pruebas.sh` (or `SOLO=<part>`). Rules: **never submit Microsoft code or disassembly**, add tests for new features, no personal data in reports, and keep user-facing text in Spanish (we'll help with wording). Issues and PRs may be written in English.
