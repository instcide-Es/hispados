# AUDICION: la prueba de sonido

`AUDICION` sirve para saber, en cada equipo de verdad, **por qué camino suena**
(o por qué no suena) y qué opciones necesita `HSPSND`. Recoge todos los datos
de sonido del equipo, toca sonidos por cada salida y pregunta si se oyen.
Todo queda en un informe de texto que se sube al FTP, a `HispaDOS/informes`.
Y **aprende**: lo que suena (se contesta «S») lo guarda en
`\HISPADOS\SONIDO.INI`, y `HSPSND` usa ese camino con preferencia, en el
arranque actual y en los siguientes (ver [Lo que aprende](#lo-que-aprende-sonidoini)).

Escrito desde cero (sala limpia) con la documentación pública: *High
Definition Audio Specification 1.0a* (Intel), *Audio Codec '97 2.3*, las hojas
de datos de los ICH (bus master del AC'97), *PCI Local Bus 2.2*, SMBIOS (DMTF
DSP0134), *Sound Blaster Series Hardware Programming Guide* (Creative) y
CEA-861 (trama de información de audio del HDMI). La forma del volcado de los
códecs imita la del «codec#0» de Linux, que es la referencia para comparar.

## Uso

    AUDICION               informe en X:\SONIDO01.TXT, SONIDO02.TXT...
    AUDICION carpeta       informe en carpeta\SONIDO.TXT
    AUDICION /RAPIDA [carpeta] [/INI:ruta]
                           la versión corta (la de INSTALAR, ver abajo)
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
- Código de salida: **2** si algo ha sonado y HSPSND puede usarlo ya (lo
  aprendido está guardado en `SONIDO.INI`, o es una Sound Blaster, que HSPSND
  encuentra sola); 0 si no (también con Esc sin que haya sonado nada, o si
  HSPSND estaba cargado); 1 si no hay memoria (128 KB libres) o si se
  contesta N a «¿Continuar?» con HSPSND cargado.

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

## Lo que aprende: SONIDO.INI

Cuando una combinación **suena**, AUDICION escribe en la unidad de arranque
`\HISPADOS\SONIDO.INI` (texto CP437 con CR+LF, comentado) una sección para
la tarjeta de ese equipo. HSPSND la lee al cargarse y, si es la de su
tarjeta, pone exactamente ese camino en vez de buscarlo. Lo pidió Carlos: «si
el usuario consigue hacer sonar su equipo en la Prueba, que sea dicha
configuración la que tenga preferencia en el arranque actual y en el
próximo».

- **La cabecera identifica la tarjeta**, por números que no cambian: la
  controladora PCI (fabricante:dispositivo y subsistema) y el códec
  (fabricante:dispositivo y subsistema):
  `[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]` (HD Audio) o
  `[AC97 8086:2415 8086:0000 8384:7600]` (AC'97). El fabricante y el modelo
  SMBIOS (`Equipo=`) van dentro, como ayuda; bus:disp.fun (`Posicion=`) y la
  dirección del códec (`Codec=`), solo como pista: pueden cambiar.
- **Contenido** (HD Audio): `Estado`, `Fecha`, `Equipo`, `Tipo`, los
  identificadores, el pin que sonó (`Altavoz`) y su `Ruta` hasta el DAC con la
  entrada elegida en cada mezclador o selector (`14h(0) 0Ch(0) 02h`: el pin
  14h toma su entrada 0, que es el nodo 0Ch; este, su entrada 0, que es el DAC
  02h), el `Control` del pin, los auriculares si se probaron y sonaron
  (`Auriculares`, `RutaAuriculares`, `ControlAuriculares`), `EAPD`, `GPIO`
  (máscara, dirección y datos, o «no hay»), `Snoop`, `Frecuencia`, `Formato`
  y `Volumen` (el amplificador que lo hace, o «por programa» si en la prueba
  no bajaba o no lo hay). AC'97: `EAPD=normal` o `invertido`.
- **Una prueba nueva en el mismo equipo reemplaza su sección** (la de la
  misma cabecera, y también la que tenga el mismo `Equipo=` y la misma
  controladora con otro códec); las de los demás equipos no se tocan. La
  sección nueva va la primera, detrás de los comentarios del principio.
- **Si no suena nada**, el códec queda como `Estado=NO SUENA` (y `SOLO CON
  TODO ABIERTO` si solo sonó con todo abierto, que HSPSND no sabe hacer):
  HSPSND no usa esas secciones y busca él solo.
- Sound Blaster de verdad (ISA): no se apunta nada; HSPSND la encuentra sola.
- Si el fichero pasa de 32 KB, AUDICION solo conserva los primeros; HSPSND
  lee los primeros 8 KB (por eso la sección nueva va al principio).
- [MEZCLADOR] En el mismo fichero va la sección `[Mezclador]`: el volumen de
  cada fuente, sus casillas y los ajustes, que guarda Hispa Ventanas y pone
  HSPSND al cargarse (docs/MEZCLADOR.md). AUDICION no la toca: no es la
  sección de ninguna tarjeta.

Ejemplo (QEMU con `AUDICION /SIM`: suena con las GPIO a 02h y el pin con el
amplificador de auriculares, que no es lo que haría HSPSND sin más):

    [HDA 8086:2668 1AF4:1100 1AF4:0022 1AF4:0022]
    ; Prueba de sonido del 13/11/2026 13:51: SUENA.
    Estado=SUENA
    Fecha=13/11/2026 13:51
    Equipo=Prueba | Portatil | 1
    Tipo=HDA
    Controladora=8086:2668
    Subsistema=1AF4:1100
    Posicion=00:04.0
    Codec=0
    IdCodec=1AF4:0022
    SubCodec=1AF4:0022
    Altavoz=03h (salida de línea)
    Ruta=03h(0) 02h
    Control=C0h
    Auriculares=03h
    RutaAuriculares=03h(0) 02h
    ControlAuriculares=C0h
    EAPD=encendido
    GPIO=07h 07h 02h
    Snoop=no
    Frecuencia=48000
    Formato=16 bits estéreo
    Volumen=amplificador 02h salida

### Cómo lo usa HSPSND

Al cargarse, antes de la búsqueda automática, HSPSND lee `\HISPADOS\SONIDO.INI`
de la unidad de la que se carga (o de la de arranque) y, por cada
controladora HD Audio y cada códec, busca la sección con su cabecera. Si la
hay y dice `SUENA`, primero **comprueba** que la ruta existe en ese códec
(un pin con salida, mezcladores o selectores, un DAC analógico al final y
cada nodo unido al siguiente por la entrada que dice la ruta) y después pone
justo eso: la ruta y el control del pin (y los auriculares), los demás pines
de salida apagados, las GPIO, el EAPD, el «snoop» de AMD/ATI, 44,1 kHz si fue
así como sonó y el volumen por programa si hace falta. Lo dice al cargarse
(«Configuración aprendida en la prueba (fecha).») y en `HSPSND /INFO`, con la
ruta (`ruta 03h(0) 02h, control del pin C0h`), EAPD y GPIO. Si `Equipo=` no
es el de este equipo (la misma tarjeta en otro modelo), la usa y lo avisa.

Si no hay sección de su tarjeta («es de otra tarjeta»), si dice que no sonó o
si la ruta no cuadra con el códec, HSPSND busca la salida él solo, como
siempre, y lo dice. `HSPSND /NOAPRENDIDO` no lo usa; `/GPIO:hh`, `/NOGPIO` y
`/EAPD:0` mandan sobre lo aprendido. Lo del fichero se analiza en la parte de
instalación, que se descarta: en el residente quedan unos 50 octetos para
`/INFO`.

### En el arranque actual: la opción «Prueba»

En la opción «Prueba» del menú de arranque, `DIAGNOSTICO` (`DIAGNOST.BAT`)
llama a `EQUIPO /INFORME`, que ofrece AUDICION al final. Si AUDICION sale con
el código 2, EQUIPO sale también con 2 (y ya no dice «reinicie y elija
Normal o Juegos») y `DIAGNOST.BAT` carga `HSPSND /VOL:80` en ese momento, con
lo aprendido: «Sonido configurado con lo aprendido: ya puede usarlo, también
en Hispa Ventanas». Si HSPSND no se pudiera cargar, sale el aviso de
reiniciar.

## En la instalación: AUDICION /RAPIDA

Idea de Carlos: «Si ponemos la prueba de sonido en la instalación de
Hispa-DOS, obligamos al usuario final a configurarlo bien desde un principio
de forma involuntaria». `INSTALAR unidad:` (y `INSTALL` desde la memoria
USB, que lo llama) termina con la **comprobación del sonido**:

    AUDICION /RAPIDA D:\HISPADOS /INI:D:\HISPADOS\SONIDO.INI

(con la unidad de destino, ejecutando el `AUDICION.COM` recién copiado). La
versión corta:

- **No vuelca nada en pantalla**: los datos del equipo, los intentos y el
  resumen van enteros al informe, como siempre. En pantalla solo salen tres
  líneas de explicación, las preguntas y una frase final.
- Pitido de control por el altavoz del PC; el **barrido** del HD Audio igual
  que la prueba completa, pero solo de las salidas analógicas y **hasta el
  primer camino que suene** (después el AC'97 y la Sound Blaster, si no ha
  sonado nada antes); y con ese camino, solo **el volumen** («¿El segundo ha
  sonado más bajo...?») y **izquierda y derecha**. Nada de frecuencias, 8
  bits, sonido largo, auriculares ni salidas digitales.
- Lo aprendido va al `SONIDO.INI` de `/INI:` (el del disco instalado, no el
  de la unidad de arranque: ni el disquete ni la memoria USB se tocan); el
  informe, a `carpeta\SONIDO.TXT` (si ya está, `SONIDO02.TXT`...). Como
  INSTALAR copia antes `\HISPADOS` entero, un `SONIDO.INI` que viniera en la
  memoria pasa al disco y la sección nueva se mezcla con él como siempre.
- **Esc** (en «Pulse una tecla para empezar» o en cualquier pregunta) salta
  el paso: si todavía no había sonado nada **no se toca `SONIDO.INI`**; el
  informe lo apunta. Si ya había sonado, lo que sonó se guarda.
- Sin tarjeta de sonido que se pueda probar (o solo HD Audio en modo V86):
  lo dice y no pregunta nada.
- Si HSPSND está cargado (arranque «Normal» desde la memoria USB) lo para
  sin preguntar y lo dice: «HSPSND se ha detenido para la prueba: volverá a
  sonar al reiniciar» (la instalación termina con un reinicio).
- Código de salida: **2** si ha sonado y queda guardado (o es una Sound
  Blaster), 0 si no o si se ha saltado, 1 si no hay memoria.

Así se ve la instalación (en QEMU):

    Hispa-DOS instalado en la unidad C: (57 archivos copiados).

    Por último, la comprobación del sonido
    --------------------------------------
    (HSPSND está cargado: se detiene durante la prueba; volverá al reiniciar.)
    Recogiendo los datos del equipo (no toque nada)...

    Vamos a comprobar que su equipo suena. Pulse S si oye el sonido,
    N si no lo oye (R lo repite). Suba el volumen si tiene rueda o teclas.
    Si no tiene altavoces ni auriculares, pulse Esc para saltar este paso.
    Pulse una tecla para empezar.

    Primero, el altavoz interno del PC (un pitido).
    P01. ¿Ha oído el pitido? (S/N/R) S

    Ahora, la tarjeta de sonido del equipo (HD Audio).
      Salida: salida de línea
    P02. ¿Lo oye? (S/N/R) S

    Bien. Ahora unas pruebas más con ese sonido.

    Volumen: sonará dos veces seguidas.
    P03. ¿El segundo ha sonado más bajo que el primero? (S/N/R) S

    Canales: izquierda y derecha.
    P04. ¿Suena solo por la izquierda? (S/N/R) S
    P05. ¿Suena solo por la derecha? (S/N/R) S

    El sonido de su equipo ha quedado configurado.
    (HSPSND se ha detenido para la prueba: volverá a sonar al reiniciar.)

    Ya puede arrancar desde ella.

Si no suena nada: «No se ha podido comprobar el sonido. Puede hacerlo
después con la opción «Prueba» del menú de arranque o con la orden
AUDICION.» (con Esc: «Se ha saltado la comprobación del sonido...»). La
instalación termina igual en todos los casos. `INSTALAR unidad: /NOSONIDO`
no hace la comprobación (para instalar sin nadie delante: INSTALAR no tiene
otro modo desatendido).

En el primer arranque desde el disco, HSPSND (en `AUTOEXEC.BAT`) encuentra
`C:\HISPADOS\SONIDO.INI` con la sección de la tarjeta y pone ese camino:
«Configuración aprendida en la prueba (fecha).».

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

## Los Cirrus Logic de los Mac [CIRRUS]

Los MacBook, MacBook Air, MacBook Pro e iMac con códec Cirrus Logic CS4206,
CS4207 o CS4208 (subsistema del códec `106Bxxxx`) no suenan con lo que deja
el firmware de Apple: los amplificadores de los altavoces y de los
auriculares están fuera del códec y se encienden con sus GPIO, y el widget de
proceso del fabricante (nodo 11h; 24h en el CS4208) tiene que estar en marcha
con sus coeficientes. AUDICION y HSPSND lo saben (`lib/cirrus.inc`):

| Códec | Altavoces | Auriculares | Widget |
|---|---|---|---|
| CS4206/CS4207 (MacBook Air 4,x y 5,x, MacBook Pro 8,x...) | GPIO 3 (08h) | GPIO 1 (02h) | 11h |
| CS4206/CS4207 del MacBook Pro 10,1 y los iMac 12,x y 14,1 | GPIO 3 (08h) | GPIO 2 (04h) | 11h |
| CS4208 (MacBook Air 6,x en adelante) | GPIO 0 (01h) | - | 24h |

AUDICION pone en marcha el widget al empezar con ese códec (lo dice en el
informe, con el estado y el coeficiente del DAC leídos después) y el primer
intento de cada salida es con las GPIO de Apple («GPIO de Apple (los
amplificadores): máscara 0Ah dirección 0Ah datos 0Ah»); después, las
variantes de siempre. Si suena así, la línea sugerida es `HSPSND` a secas.
HSPSND, sin opciones, hace lo mismo (las dos GPIO encendidas: suenan los
altavoces y los auriculares); `/GPIO:hh`, `/NOGPIO` y lo aprendido mandan
sobre eso, pero el widget de proceso se pone en marcha siempre. `/INFO` lo
dice: «Cirrus CS4206 de Apple (subsistema del códec 106B:5600): amplificador
de los altavoces por la GPIO 3 y el de los auriculares por la GPIO 1; widget
de proceso 11h puesto en marcha.» y de dónde han salido las GPIO.

## El arranque del flujo [CIRRUS]

Cada sonido para el flujo como pide la norma (RUN a 0 y esperar a que la
controladora lo lea 0), lo reinicia (SRST a 1 hasta leerlo 1, SRST a 0 hasta
leerlo 0, con una pausa en cada paso), lo programa, lo suelta de SSYNC y lo
arranca; si en un cuarto de segundo la posición no se mueve, apunta los
registros del flujo y lo repite una vez desde el reinicio. El informe dice
«a la primera no arrancó; se ha reiniciado el flujo y a la segunda sí» con
los registros, o, si tampoco, «NO AVANZA» con los registros de las dos veces.
(En el MacBook Air 2012 la mitad de los intentos, alternos, no arrancaban;
antes no se esperaba a que el flujo estuviera parado, no había pausas en el
reinicio ni segundo intento, y el informe no decía cómo estaban los
registros.) HSPSND para y reinicia el flujo de la misma manera: antes, al
quedarse sin datos o cambiar de frecuencia, volvía a arrancar justo después
de escribir RUN a 0, sin esperar a que parase.

## Pruebas

`tests/run_audicion_tests.sh`: en QEMU, con las respuestas simuladas por la
consola serie (`tests/tools/audicion_drv.py` contesta cuando ve la pregunta)
y la salida de audio grabada. Intel HD Audio (hda-duplex, hda-output,
hda-micro y un ICH9 con el códec en la dirección 2), AC'97, Sound Blaster 16
y sin tarjeta. La opción `/SIM` (solo para las pruebas) hace como si el códec
tuviera tres GPIO, EAPD, «snoop» y una salida de auriculares, para recorrer
todas las variantes y comprobar la línea sugerida. `/SIM:MAC` contesta por
el códec 0 como el Cirrus CS4206 del MacBook Air 2012 (con su volcado real;
el formato y el flujo de sus DAC van también al DAC de QEMU, para que suene)
y hace que el flujo no arranque a la primera, para probar el segundo
intento; `HSPSND /SIMMAC /INFO` (solo para las pruebas: no se queda
residente) usa el mismo códec simulado.

`tests/run_aprendido_tests.sh`: lo aprendido (`SONIDO.INI`). AUDICION /SIM
que suena por un camino que HSPSND no elegiría solo (GPIO 02h, control
C0h), con un SONIDO.INI previo de otros equipos que debe quedar igual; el
disquete de uso en «Normal» con ese fichero (HSPSND dice «aprendida», /INFO
enseña la ruta y suena); la misma tarjeta en otro modelo (SMBIOS); otra
tarjeta (hda-output y AC'97: no lo usa); `/NOAPRENDIDO`; una segunda prueba
que reemplaza la sección; AUDICION y HSPSND con AC'97; y la opción «Prueba»
del menú, con y sin sonido: HSPSND queda cargado y `SONAR` suena sin
reiniciar.

`tests/run_instalar_sonido_tests.sh`: la comprobación del sonido de
INSTALAR en un disco vacío de 32 MB con hda-duplex. Desde el disquete de uso
en «Normal» (HSPSND cargado) y desde la memoria USB (disco D:, con un
`SONIDO.INI` de otro equipo en la memoria): las cinco preguntas y nada más,
sin el volcado en pantalla, `SONIDO.INI` y `SONIDO.TXT` en el `\HISPADOS` del
disco (la memoria y el disquete, sin tocar); después, arrancando solo desde
el disco, HSPSND dice «aprendida», `/INFO` enseña la ruta y `SONAR` suena
(grabación de QEMU). Y Esc en la primera pregunta (el `SONIDO.INI` copiado
queda igual y la instalación acaba), `/NOSONIDO` y sin tarjeta de sonido.

**Falta probarlo en hardware real**; en QEMU no hay GPIO, EAPD, detección de
presencia ni HDMI de verdad.
