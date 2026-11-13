# AUDICION: la prueba de sonido

`AUDICION` sirve para saber, en cada equipo de verdad, **por qué camino suena**
(o por qué no suena) y qué opciones necesita `HSPSND`. Recoge todos los datos
de sonido del equipo, toca sonidos por cada salida y pregunta si se oyen.
Todo queda en un informe de texto que se sube al FTP, a `HispaDOS/informes`.

Escrito desde cero (sala limpia) con la documentación pública: *High
Definition Audio Specification 1.0a* (Intel), *Audio Codec '97 2.3*, las hojas
de datos de los ICH (bus master del AC'97), *PCI Local Bus 2.2*, SMBIOS (DMTF
DSP0134), *Sound Blaster Series Hardware Programming Guide* (Creative) y
CEA-861 (trama de información de audio del HDMI). La forma del volcado de los
códecs imita la del «codec#0» de Linux, que es la referencia para comparar.

## Uso

    AUDICION               informe en X:\SONIDO01.TXT, SONIDO02.TXT...
    AUDICION carpeta       informe en carpeta\SONIDO.TXT
    AUDICION /?            ayuda (también AYUDA AUDICION)

- **Teclas**: `S` = sí, `N` = no, `R` = repetir el sonido, `Esc` = terminar y
  guardar lo que haya. No hay más preguntas que las de sí o no.
- **No usa HSPSND**: maneja la tarjeta directamente. Lo mejor es arrancar con
  la opción «Prueba de sonido» del menú de arranque (sin residentes). Si
  HSPSND está cargado, le pide parar (función 03h), avisa y pregunta si se
  sigue; al acabar hay que reiniciar para volver a usar HSPSND, porque
  AUDICION reinicia la controladora.
- Funciona sin HSPXMS ni nada más cargado (abre y cierra él la línea A20).
- Con HSPEMM u otro EMM386 (modo V86) no puede llegar al HD Audio ni al AC'97:
  lo dice en el informe y solo prueba el altavoz del PC y la Sound Blaster.
- **El informe nunca se sobrescribe.** Sin parámetro va a la raíz de la unidad
  de arranque (si no se puede escribir en ella, a la unidad actual) como
  `SONIDO01.TXT`, `SONIDO02.TXT`... Con una carpeta (que ya tiene que existir)
  va a `carpeta\SONIDO.TXT`; si ya existe, `SONIDO02.TXT`, `SONIDO03.TXT`...
  Es texto CP437 con CR+LF. Se guarda cada dos respuestas, por si el equipo
  se cuelga.
- Código de salida: 0 (también con Esc); 1 si no hay memoria (128 KB libres) o
  si se contesta N a «¿Continuar?» con HSPSND cargado.

Para `DIAGNOSTICO`: `AUDICION X:\INFORMES\EQUIPO01` deja `SONIDO.TXT` junto al
resto del volcado del equipo. No escribe más archivos: el volcado de los
códecs va entero dentro del informe, con todos los valores en hexadecimal.

## Qué hace, por orden

1. **Recoge sin preguntar** (apartados `=== ... ===` del informe):
   - `EQUIPO`: SMBIOS (BIOS, fabricante y modelo del equipo, placa),
     procesador (CPUID) y memoria. No se guardan números de serie.
   - `CONTROLADORAS DE SONIDO (PCI)`: todas las de clase 04h con subclase
     01h (audio/AC'97), 03h (HD Audio) y 80h: bus:disp.fun, fabricante y
     dispositivo, subsistema, orden/estado, IRQ, los seis BAR, los registros
     propios (Intel: TCSEL 44h; AMD/ATI: 42h «snoop»; Nvidia: 4Eh) y los 256
     octetos de configuración.
   - `HD AUDIO bb:dd.f`: registros de la controladora como los dejó la BIOS
     (GCTL, STATESTS, INTCTL, CORBCTL, RIRBCTL), GCAP, versión, tamaños de
     CORB y RIRB, y si el códec contesta por CORB/RIRB y por órdenes
     inmediatas (lo que usa HSPSND). Si la BIOS dejó el enlace en marcha **no
     se reinicia** en esta fase, para leer las GPIO y el EAPD tal como los
     dejó la BIOS.
   - `Códec n`: identificador, revisión, grupos de función, y del grupo de
     audio: subsistema, PCM, amplificadores por defecto, GPIO (cuántas y su
     máscara, dirección y datos) y **todos los nodos**: tipo, capacidades,
     conexiones (con `*` la elegida), amplificadores de entrada y de salida
     (paso de 0 dB, número de pasos, tamaño del paso, silencio y valor actual
     de cada canal), convertidores (flujo, formato, PCM), pines (capacidades,
     configuración por defecto descifrada, control, EAPD/BTL y presencia),
     energía, mando de volumen y coeficientes de proceso (los de Realtek).
   - `AC'97`: puertos, identificador del códec y los 64 registros del
     mezclador. `SOUND BLASTER (ISA)`: puerto, versión del DSP, DMA, la
     variable `BLASTER` y el mezclador. `ALTAVOZ DEL PC`.
2. **Prueba de control**: un pitido por el altavoz del PC (temporizador 2 y
   puerto 61h).
3. **Barrido del HD Audio**, controladora por controladora y códec por códec.
   Los pines que pueden sacar señal se prueban por este orden: altavoz
   interno, altavoz, auriculares y línea. Los que tienen detección de
   presencia y nada enchufado no se preguntan (salvo los altavoces). Para
   cada intento: ruta desde el pin hasta un DAC por mezcladores y selectores,
   todos los nodos del camino a D0, amplificadores a 0 dB y sin silencio, pin
   como salida, los demás pines de salida apagados, flujo 1 y tono de 440 Hz
   de un segundo. En el primer pin de cada códec, si no suena, se prueban
   las variantes:
   1. la de partida, igual que HSPSND sin opciones: EAPD encendido, todas
      las GPIO como salidas y a 1, 48 kHz;
   2. AMD/ATI: «snoop» activado (registro PCI 42h);
   3. GPIO como las dejó la BIOS; GPIO todas a 0; cada GPIO a 1 por separado;
   4. EAPD apagado (con las GPIO a 1 y como la BIOS);
   5. 44,1 kHz;
   6. otra ruta (otro DAC u otro mezclador), si la hay;
   7. sin el amplificador de auriculares del pin;
   y al final, una vez por códec, **todo abierto**: todos los DAC, todas las
   salidas y todos los amplificadores a la vez. Si así suena, se prueban una
   a una las salidas sin declarar (pines «sin conexión» o de otro tipo) para
   saber cuál es; si así tampoco suena, no se pregunta más por ese códec.
   En cuanto algo suena se deja de buscar. Cada intento se apunta con la
   combinación exacta (línea `Intento:`) y la respuesta.
4. **Con el camino que suena**: bajar el volumen (unos 20 dB), subirlo al
   máximo, silencio, solo izquierda, solo derecha, las frecuencias 8000,
   11025, 22050, 44100 y 48000 Hz que admita el DAC (se compara cada una con
   la del camino: «¿suena igual de agudo?»), 8 bits si los admite, un sonido
   de 5 segundos y los auriculares (detección de presencia antes y después de
   enchufarlos, si suenan y si el altavoz calla al apagar su pin).
5. Las **salidas digitales** (HDMI, DisplayPort, SPDIF) solo si su detección
   dice que hay algo enchufado: sonaría por la pantalla externa. Es un intento
   sencillo (trama de información de audio de dos canales).
6. **AC'97** y **Sound Blaster ISA**, si las hay: un tono y la pregunta (y una
   segunda variante si no suena: EAPD invertido en el AC'97, mezclador al
   máximo en la Sound Blaster).

No se cuelga: todas las órdenes al códec tienen un plazo de un cuarto de
segundo (`[sin respuesta a la orden ...]` en el informe); al cuarto fallo
seguido el códec se da por perdido y se sigue con lo demás.

## Cómo leer el informe

- **`=== RESUMEN ===`** (al final) es lo primero que hay que mirar:
  - `Altavoz del PC (pitido): sí` — la persona oye y contesta bien.
  - `SUENA: HD Audio ... pin 14h (altavoz interno), ruta 14h 0Ch 02h ...,
    EAPD encendido, GPIO máscara 03h dirección 03h datos 01h, 48000 Hz` — la
    combinación exacta que sonó. `NO suena: ... (n intentos)` si ninguna.
  - `Línea sugerida: HSPSND /GPIO:01` — las opciones de HSPSND que reproducen
    esa combinación (`/NOGPIO`, `/GPIO:hh`, `/EAPD:0`).
  - `FALTA en HSPSND: ...` — el camino que suena necesita algo que HSPSND
    todavía no hace («snoop», CORB/RIRB, otra ruta, solo 44,1 kHz, todo
    abierto...). `AVISO: ...` — cosas que conviene mirar.
  - Las pruebas de volumen, canales, frecuencias y auriculares, con `sí` o
    `NO`.
- **`=== PRUEBAS ===`**: cada `Intento:` con su combinación, los
  amplificadores abiertos, si la posición del flujo avanza (si **no avanza**,
  el problema está en la controladora o en la DMA, no en el códec) y la
  pregunta `Pnn` con la respuesta. `(repetido n)` si se pidió repetir.
- Los volcados sirven para comparar con el «codec#0» de Linux del mismo
  modelo y para decidir el «pin quirk» que haga falta. Lo más útil suele
  ser: la `Configuración por defecto` de cada pin (qué es altavoz y qué
  auriculares), las `Conexiones`, las `GPIO ahora` como las dejó la BIOS y
  el `EAPD/BTL`.

## Pruebas

`tests/run_audicion_tests.sh`: en QEMU, con las respuestas simuladas por la
consola serie (`tests/tools/audicion_drv.py` contesta cuando ve la pregunta)
y la salida de audio grabada. Intel HD Audio (hda-duplex, hda-output,
hda-micro y un ICH9 con el códec en la dirección 2), AC'97, Sound Blaster 16
y sin tarjeta. La opción `/SIM` (solo para las pruebas) hace como si el códec
tuviera tres GPIO, EAPD, «snoop» y una salida de auriculares, para recorrer
todas las variantes y comprobar la línea sugerida.

**Falta probarlo en hardware real**; en QEMU no hay GPIO, EAPD, detección de
presencia ni HDMI de verdad.
