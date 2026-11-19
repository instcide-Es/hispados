# Casos de HispaBIOS: el teclado USB del firmware y los tics de su reloj (0.6).
# (Lo incluye tests/run_hispabios_tests.sh, con sus ayudas: arranca, hb_variante,
# hb_log, ok, ko. A solas: HB_SOLO=teclado tests/run_hispabios_tests.sh.)
#
# El MEDION S2217 (Atom Bay Trail, firmware de 32 bits, SIN 8042: el teclado de
# la base es USB) tenía teclado en el menú de CONFIG.SYS y lo perdía después.
# El teclado USB del firmware solo recibe teclas en los sucesos de temporizador
# del firmware, y estos solo andan con la interrupción de su reloj, que con DOS
# en marcha no le llega (docs/HISPABIOS.md, «El teclado USB del firmware se
# quedaba sin reloj»). Aquí: QEMU sin 8042 (-machine q35,i8042=off) y un
# usb-kbd en el xHCI de la memoria USB, y HispaBIOS ensamblada con
# -DPRUEBA_FW_SIN_RELOJ: el firmware no recibe NINGUNA interrupción en sus
# viajes (en OVMF su reloj es el del APIC local, que no va acompasado con el
# 8254 y por eso allí el fallo no se veía). Con eso y sin dar tics
# (-DPRUEBA_FWT=0) el teclado se queda mudo en C:\>: es el fallo. Con los tics
# dados en cada viaje (GenerateSoftInterrupt, o la rutina de la interrupción
# del firmware con -DPRUEBA_FWT=2) el teclado responde.
TEC_MAQ="-machine q35,i8042=off"
nasm -f bin -o $T/TECLAPRU.COM tests/progs/teclapru.asm || ko "no se ensambla TECLAPRU.COM"

# tec_caso arq variante "opciones" "título"  -> arranca y deja S y L
tec_caso() {
  hb_variante $2 "$3" $( [ $1 = 64 ] || echo 32 ) || return 1
  mcopy -o -i $T/$2.img@@1048576 $T/TECLAPRU.COM ::TECLAPRU.COM
  HB_MEM=1G HB_MAQ="$TEC_MAQ" HB_EXTRA="-device usb-kbd" arranca $1 $T/$2.img $2 \
    "d i r ret w3 t e c l a p r u spc 4 ret w1 a b c w6 h b l o g ret" "${4:-fin ---}" || return 1
  S=$T/serie_$2.txt
  L=$(hb_log $2)
}
# tec_bien: lo común cuando el teclado tiene que ir
tec_bien() {
  grep -aq 'bytes libres' $S && ok "el teclado USB responde en C:\\> (DIR)" || ko "el teclado USB no responde en C:\\>"
  grep -aq 'TECLAPRU: teclas recibidas: 00003' $S && ok "TECLAPRU (espera con HLT, como el núcleo) recibe las 3 teclas" || ko "TECLAPRU no recibe las 3 teclas ($(grep -a 'teclas recibidas' $S | tr -d '\r'))"
  echo "$L" | grep -q 'USB: interfaz HID de teclado 0627:0001, interfaz 0 (clase/subclase/protocolo 03/01/01): la lleva el firmware' && ok "HISPABIOS.LOG: el teclado USB y quién lo lleva (el firmware)" || ko "HISPABIOS.LOG sin el teclado USB"
  echo "$L" | grep -q 'teclado del firmware (Simple Text Input) en el manejador [0-9A-F]*: es USB, 0627:0001' && ok "HISPABIOS.LOG: los teclados del firmware (Simple Text Input), el USB entre ellos" || ko "HISPABIOS.LOG sin los teclados del firmware"
  echo "$L" | grep -q 'reloj del firmware (Timer Arch): periodo 10000 microsegundos' && ok "HISPABIOS.LOG: el reloj del firmware y su periodo (10 ms en OVMF)" || ko "HISPABIOS.LOG sin el reloj del firmware"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
}

echo "== HispaBIOS: teclado solo USB (sin 8042) y el firmware sin interrupciones: SIN dar tics, el teclado se queda mudo (el fallo del Medion) =="
if HB_SEG=90 tec_caso a32 tecno "-DREGISTRO_PASOS -DPRUEBA_FW_SIN_RELOJ -DPRUEBA_FWT=0" "bytes libres"; then
  grep -aq 'C:\\>' $S && ok "arranca hasta C:\\> (el fallo no es del arranque)" || ko "no llega a C:\\>"
  grep -aq 'bytes libres' $S && ko "el teclado responde sin tics: la prueba no reproduce el fallo" || ok "sin tics al firmware, el teclado USB no responde (se reproduce el fallo)"
  echo "$L" | grep -q '(prueba: no se dan tics al firmware)' && ok "HISPABIOS.LOG dice que no se dan tics" || ko "HISPABIOS.LOG no dice que no se dan tics"
else
  ko "no se ha podido ensamblar o arrancar la variante sin tics"
fi

echo "== HispaBIOS (32 bits): teclado solo USB y el firmware sin interrupciones: tics por GenerateSoftInterrupt =="
if tec_caso a32 tec32 "-DREGISTRO_PASOS -DPRUEBA_FW_SIN_RELOJ"; then
  tec_bien
  echo "$L" | grep -q 'GenerateSoftInterrupt (un tic a mano): resultado 00000000' && echo "$L" | grep -q 'Tics al firmware en cada viaje: por GenerateSoftInterrupt' && ok "HISPABIOS.LOG: los tics se dan por GenerateSoftInterrupt" || ko "HISPABIOS.LOG sin el camino de los tics"
  grep -aq 'Teclado: teclas dadas por el firmware: [1-9][0-9]*; tics dados al firmware: [1-9][0-9]* (por GenerateSoftInterrupt de su reloj)' $S && ok "HBLOG: las teclas y los tics dados al firmware" || ko "HBLOG sin el teclado ($(grep -a 'Teclado: teclas' $S | tr -d '\r'))"
else
  ko "no se ha podido ensamblar o arrancar la variante de 32 bits"
fi

echo "== HispaBIOS (32 bits): lo mismo con los tics por la rutina de la interrupción del firmware (su IDT) =="
if tec_caso a32 tec32r "-DREGISTRO_PASOS -DPRUEBA_FW_SIN_RELOJ -DPRUEBA_FWT=2"; then
  tec_bien
  echo "$L" | grep -q 'Tics al firmware en cada viaje: por su rutina de la interrupcion del reloj (vector [0-9A-F]*h de su IDT)' && ok "HISPABIOS.LOG: los tics se dan por la rutina de la interrupción" || ko "HISPABIOS.LOG sin el camino de los tics"
  grep -aq 'tics dados al firmware: [1-9][0-9]* (por su rutina de la interrupcion del reloj)' $S && ok "HBLOG: los tics dados por la rutina" || ko "HBLOG sin los tics por la rutina"
else
  ko "no se ha podido ensamblar o arrancar la variante de 32 bits con la rutina"
fi

echo "== HispaBIOS (32 bits, procesador de 32): teclado solo USB y el firmware sin interrupciones =="
if tec_caso 32 tec32p "-DREGISTRO_PASOS -DPRUEBA_FW_SIN_RELOJ"; then
  tec_bien
else
  ko "no se ha podido ensamblar o arrancar la variante con procesador de 32 bits"
fi

echo "== HispaBIOS (64 bits): teclado solo USB y el firmware sin interrupciones: tics por GenerateSoftInterrupt =="
if tec_caso 64 tec64 "-DREGISTRO_PASOS -DPRUEBA_PILA -DPRUEBA_FW_SIN_RELOJ"; then
  tec_bien
  echo "$L" | grep -q 'Tics al firmware en cada viaje: por GenerateSoftInterrupt' && ok "HISPABIOS.LOG: los tics se dan por GenerateSoftInterrupt" || ko "HISPABIOS.LOG sin el camino de los tics"
  grep -aq 'tics dados al firmware: [1-9][0-9]* (por GenerateSoftInterrupt de su reloj)' $S && ok "HBLOG: los tics dados al firmware" || ko "HBLOG sin los tics"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear ($(grep -ac 'PILA-MAL' $S))" || ok "la pila, alineada (también en las llamadas del teclado)"
else
  ko "no se ha podido ensamblar o arrancar la variante de 64 bits"
fi

echo "== HispaBIOS (64 bits): lo mismo con los tics por la rutina de la interrupción del firmware =="
if tec_caso 64 tec64r "-DREGISTRO_PASOS -DPRUEBA_PILA -DPRUEBA_FW_SIN_RELOJ -DPRUEBA_FWT=2"; then
  tec_bien
  echo "$L" | grep -q 'Tics al firmware en cada viaje: por su rutina de la interrupcion del reloj' && ok "HISPABIOS.LOG: los tics se dan por la rutina de la interrupción" || ko "HISPABIOS.LOG sin el camino de los tics"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
else
  ko "no se ha podido ensamblar o arrancar la variante de 64 bits con la rutina"
fi

# El modo seguro (S): no se dan tics; INT 16h pregunta siempre al firmware y la
# espera de tecla no usa HLT (como la 0.2). Aquí el firmware SÍ recibe sus
# interrupciones (sin PRUEBA_FW_SIN_RELOJ): lo que se ve es que el teclado USB
# va y que queda dicho.
echo "== HispaBIOS (32 bits): el modo seguro con teclado solo USB: sondeo continuo, sin tics =="
if tec_caso a32 tecseg "-DREGISTRO_PASOS -DPRUEBA_SEGURO"; then
  grep -aq 'bytes libres' $S && ok "el teclado USB responde en el modo seguro" || ko "el teclado USB no responde en el modo seguro"
  grep -aq 'TECLAPRU: teclas recibidas: 00003' $S && ok "TECLAPRU recibe las 3 teclas" || ko "TECLAPRU no recibe las 3 teclas"
  echo "$L" | grep -q 'Modo seguro: no se dan tics al firmware; INT 16h le pregunta siempre' && ok "HISPABIOS.LOG: modo seguro, sondeo continuo" || ko "HISPABIOS.LOG no dice lo del modo seguro"
  grep -aq 'tics dados al firmware: 0 (no se le dan); modo seguro: sondeo continuo, sin HLT' $S && ok "HBLOG: sin tics, sondeo continuo" || ko "HBLOG no dice lo del modo seguro ($(grep -a 'Teclado: teclas' $S | tr -d '\r'))"
else
  ko "no se ha podido ensamblar o arrancar la variante del modo seguro"
fi
