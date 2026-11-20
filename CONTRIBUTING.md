# Cómo colaborar con Hispa-DOS

¡Gracias por pasarte! Hispa-DOS lo hace hoy muy poca gente y **cualquier ayuda cuenta**, también si no sabes ensamblador.
*English speakers: see [the short version below](#english).*

## Sin programar

- **Pruébalo en un PC de verdad.** Es lo que más falta hace: casi todo pasa las pruebas en emuladores, pero falta hardware real (USB, sonido, red, Wi-Fi, ACPI, portátiles). Arranca la memoria USB o el disquete de la [última versión](https://github.com/instcide-Es/hispados/releases), ejecuta `DIAGNOSTICO` y abre una *issue* con la plantilla **«Informe de un equipo real»** adjuntando la carpeta `\INFORMES\EQUIPOnn`. Sirve igual si **todo funciona**.
- **Pruébalo en el navegador**, en [hispados.es](https://hispados.es), y cuenta lo que no se entiende o falla.
- **Juegos y programas de DOS:** dinos cuáles van y cuáles no (y en qué equipo o emulador).
- **Textos y documentación:** erratas, explicaciones confusas, traducciones (las pantallas están en español; `docs/` agradece una mano en inglés).
- **Difúndelo:** una ⭐ en el repositorio, o contarlo en foros de retroinformática, ayuda a que lo encuentre más gente.

## Programando

### Preparar el entorno

En Debian, Ubuntu o WSL:

```sh
sudo apt install nasm mtools qemu-system-x86
make          # crea la imagen de disquete en build/
make run      # la arranca en QEMU
make test     # pruebas automáticas de principio a fin (por el puerto serie)
```

Las pruebas viven en [`tests/`](tests/); hay un `run_*_tests.sh` por cada parte, por si quieres lanzar solo la tuya.

### Por dónde empezar

- Las *issues* marcadas **[`good first issue`](https://github.com/instcide-Es/hispados/labels/good%20first%20issue)** están pensadas para empezar; las de **[`help wanted`](https://github.com/instcide-Es/hispados/labels/help%20wanted)**, para quien ya se maneja.
- [docs/ROADMAP.md](docs/ROADMAP.md) dice lo que viene; [docs/VISION.md](docs/VISION.md), hacia dónde va la familia.
- Si quieres meterte en algo grande, abre antes una *issue* contándolo: así nadie hace el mismo trabajo dos veces.

### Normas (importantes)

1. **Nada de código de Microsoft**: ni del MS-DOS que Microsoft publicó, ni de desensamblados, ni de filtraciones. Todo se escribe a partir de documentación pública (Ralf Brown's Interrupt List, manuales de Intel, especificaciones de USB, ACPI, HD Audio...). Si te has inspirado en algo, dilo en la PR.
2. **Cada función nueva de INT 21h lleva su comprobación en `tests/`**, y `make test` tiene que seguir pasando.
3. **Nada de programas de terceros con derechos** (Windows 3.1, juegos comerciales...) en el repositorio: cada uno usa su copia legal.
4. **En español**: mensajes en pantalla, nombres de órdenes, comentarios y textos de commit. Si no lo dominas, escribe como puedas; lo pulimos juntos.
5. Licencia: lo que aportas se publica con la **GPL-2.0 o posterior**, como el resto.

### Enviar los cambios

1. Haz un *fork* y una rama a partir de la rama principal.
2. Cambios pequeños y con un propósito cada uno; mejor varias PR cortas que una enorme.
3. Explica en la PR qué cambia, por qué y **dónde lo has probado** (QEMU, 86Box, VirtualBox, equipo real...).
4. No hace falta que esté perfecto: una PR a medias, marcada como borrador, también vale para pedir opinión.

## Dudas

Abre una *issue* con la etiqueta `question`. No hay preguntas tontas: el DOS tiene cuarenta años de rarezas.

Todos los que participan siguen el [código de conducta](CODE_OF_CONDUCT.md).

---

<a id="english"></a>
## English (short version)

Contributions are very welcome. The most useful thing right now is **testing on real hardware**: boot a [release](https://github.com/instcide-Es/hispados/releases), run `DIAGNOSTICO` and open an issue with the "Informe de un equipo real" template. To build: `sudo apt install nasm mtools qemu-system-x86`, then `make`, `make run`, `make test`. Rules: **never submit Microsoft code or disassembly**, add a test in `tests/` for every new INT 21h function, and keep user-facing text in Spanish (we'll help with wording). Issues and PRs may be written in English.
