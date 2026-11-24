# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_termo_disco.sh - (TERMO) los discos del Termómetro (termo/disco.c,
# nucleo32/tdisco.cas y nvme.cas), dentro de la parte 46 (SOLO=termo,
# pruebas_termo.sh, que lee este archivo): sus ayudantes (ok, ko, mira,
# termo_qemu) y sus carpetas ($AQUI, $Y).
#  a. En el anfitrión: disco.c con datos simulados (pruebas/disco_prueba.c):
#     SMART de un disco sano, de uno con 12 sectores pendientes (el veredicto:
#     «puede ser la causa de los parones»), de un SSD gastado, sin SMART y
#     con SMART apagado; NVMe sano y con aviso crítico; eMMC con PRE_EOL;
#     el informe (sin números de serie enteros) y el detalle.
#  b. En QEMU, la memoria del Termómetro con tres discos de dentro: IDE
#     (el canal primario), SATA (una AHCI, -device ahci) y NVMe: los tres
#     salen en INFORME.TXT («DISCOS») con su salud, su SMART (QEMU da el
#     básico en IDE y AHCI) o el registro de salud de NVMe (Get Log Page
#     02h), la prueba de lectura (y su pausa durante la prueba de esfuerzo),
#     la serie recortada a las 4 últimas letras, 0 sectores escritos en los
#     discos de dentro, y los tres discos sin un solo octeto cambiado.

termo_disco_anfitrion() {
  local T=$AQUI/nucleo32/termo B=$Y/anf
  mkdir -p $B
  if ! gcc -O1 -Wall -Wextra -DDI_ANFITRION -I$T $T/pruebas/disco_prueba.c $T/disco.c -o $B/disco_prueba 2> $B/gccdi.txt; then
    ko "disco.c no compila en el anfitrión: $(head -n 2 $B/gccdi.txt)"; return
  fi
  if $B/disco_prueba > $B/disco.txt; then
    ok "(DISCOS) disco.c con datos simulados: $(grep -c '^bien' $B/disco.txt) comprobaciones (SMART de SATA sano, con 12 pendientes y de un SSD gastado; sin SMART; NVMe; eMMC; la lectura; el informe y el veredicto)"
  else ko "(DISCOS) disco.c con datos simulados:"; grep -v '^bien' $B/disco.txt | sed 's/^/        /' | head -n 12; fi
}

# (para TQ_HASTA) ¿está ya escrito el INFORME.TXT con los discos? Al acabar
# la lectura de los discos («Discos: Hecho»), si la prueba ya había dado su
# resultado, el informe se vuelve a escribir («informe para el técnico»): hay
# que esperar a ese, no a la hora (la lectura del IDE de QEMU, en PIO, tarda
# más o menos según lo ocupado que esté el anfitrión)
td_informe_hecho() {
  grep -a 'termo: suceso: ' $1 | awk '/Discos: Hecho: / { d = NR } /informe para el t/ { i = NR } END { exit !(d && i > d) }'
}

termo_discos() {
  local D=$Y/discos f s0 s1 I L
  rm -rf $D; mkdir -p $D
  # tres discos de 64 MB con algo dentro (para ver que no cambia nada)
  for f in ide sata nvme; do
    head -c 67108864 /dev/zero > $D/$f.img
    printf 'HISPA-TERMO-DISCO-%s' $f | dd of=$D/$f.img bs=1 seek=1048576 conv=notrunc status=none
  done
  s0=$(cat $D/ide.img $D/sata.img $D/nvme.img | sha256sum)
  cp $Y/termo.img $Y/d.img
  mdel -i $Y/d.img@@1M ::TERMO/INFORME.TXT > /dev/null 2>&1
  TQ_HASTA=td_informe_hecho TQ_TOPE=600 TQ_ACPI="-drive file=$D/ide.img,format=raw,if=ide,index=0 -device ahci,id=ah -drive file=$D/sata.img,format=raw,if=none,id=sa -device ide-hd,drive=sa,bus=ah.0,serial=SATA1234ABCD -drive file=$D/nvme.img,format=raw,if=none,id=nv -device nvme,drive=nv,serial=NVME5678WXYZ" \
    termo_qemu $Y/d.img td1 300 "-cpu Haswell" "44:sendkey 1" "46:sendkey c" "60:$C" "60:o /b 0x71 91" "72:$C" "72:o /b 0x71 50"
  s1=$(cat $D/ide.img $D/sata.img $D/nvme.img | sha256sum)
  if [ "$s0" = "$s1" ]; then ok "(DISCOS) los tres discos de dentro (IDE, SATA y NVMe), sin un solo octeto cambiado"; else ko "(DISCOS) ¡se ha escrito en un disco de dentro!"; fi
  mira "(DISCOS) la tarea hija TERMO.BIN /DISCO mira los tres discos (el NVMe lo pone en marcha nvme.cas, solo para leer) y acaba" \
    "termo: discos: Mirando los discos de dentro (solo leer)" "termo: discos: disco 3 de 3 (QEMU NVMe Ctrl): su salud" \
    "termo: discos: Hecho: 3 discos: 1 BIEN, 2 BIEN, 3 BIEN"
  mira "(DISCOS) con la prueba de esfuerzo, la lectura de los discos espera (y sigue al acabar)" \
    "termo: discos: en pausa mientras dura la prueba de esfuerzo" "termo: suceso: Discos: Hecho: 3 discos"
  I=$D/INFORME.TXT
  mtype -i $Y/d.img@@1M ::TERMO/INFORME.TXT 2> /dev/null | tr -d '\r' > $I
  # (td_hay N REGEX: ¿hay N líneas así en INFORME.TXT (al menos N, con «+N»)?
  # si no, se apunta en $f lo que falta, para decirlo en el FAIL)
  td_hay() {
    local c; c=$(grep -c -- "$2" $I)
    case $1 in +*) [ "$c" -ge "${1#+}" ] && return;; *) [ "$c" = "$1" ] && return;; esac
    f="$f «$2» ($c de $1);"
  }
  td_falta() { [ -s $I ] || f=" (INFORME.TXT vacío o no está)$f"; if grep -q '^DISCOS' $I; then sed -n '/^DISCOS/,/^Sectores\|^  Sectores/p' $I; else echo "(sin «DISCOS»; lo que hay:)"; head -n 8 $I; fi | head -n 20 | sed 's/^/        /'; }
  local f=
  td_hay +1 '^DISCOS (solo se leen'; td_hay 1 '^  Disco 1: QEMU HARDDISK (IDE, 67 MB)'; td_hay 1 '^  Disco 2: QEMU HARDDISK (SATA, 67 MB), serie \.\.\.ABCD'
  td_hay 1 '^  Disco 3: QEMU NVMe Ctrl (NVMe, SSD, 67 MB), serie \.\.\.WXYZ'; td_hay 3 '^    Salud: BIEN\.'
  if [ -z "$f" ]; then ok "(DISCOS) INFORME.TXT, «DISCOS»: los tres, con su clase, su tamaño, la serie recortada y «Salud: BIEN» en palabras"
  else ko "(DISCOS) INFORME.TXT, «DISCOS»: falta$f"; td_falta; fi
  f=
  td_hay 2 '^    SMART: .*horas encendido; .*encendidos; .*reasignados 0, pendientes 0, incorregibles 0, CRC del cable 0; el disco dice de sí: bien\.'
  td_hay 1 '^    Salud NVMe: [0-9]* °C; usado 0 % de su vida; reserva [0-9]* %'
  if [ -z "$f" ]; then ok "(DISCOS) el SMART de IDE y de SATA (IDENTIFY, READ DATA, THRESHOLDS, RETURN STATUS) y la salud de NVMe (Get Log Page 02h)"
  else ko "(DISCOS) SMART y NVMe: falta$f"; grep 'SMART\|NVMe' $I | head -n 6 | sed 's/^/        /'; fi
  f=
  td_hay 3 '^    Prueba de lectura: [0-9]* MB repartidos por todo el disco a [0-9,]* MB/s y [0-9]* MB seguidos a [0-9,]* MB/s; cada lectura de 64 K: media [0-9]* ms, p99 [0-9]* ms, la peor [0-9]* ms; 0 errores, 0 reintentos'
  td_hay +2 '^    Prueba de lectura: 256 MB repartidos.* y 64 MB seguidos'; td_hay +1 'Sectores escritos en los discos de dentro desde que arrancó la memoria: 0 (como debe ser)'
  if [ -z "$f" ]; then ok "(DISCOS) la prueba de lectura de los tres (MB/s a saltos y seguido, media, p99 y la peor; SATA y NVMe enteros, el IDE de QEMU, en PIO y lento, puede quedarse en el minuto) y 0 sectores escritos en los discos de dentro"
  else ko "(DISCOS) la prueba de lectura: falta$f"; grep 'Prueba de lectura\|Sectores escritos' $I | head -n 4 | sed 's/^/        /'; fi
  if ! grep -q 'SATA1234\|NVME5678\|QM0000' $I && ! mtype -i $Y/d.img@@1M ::TERMO/DISCOS.LOG 2> /dev/null | grep -q 'SATA1234\|NVME5678\|QM0000' && ! grep -q 'SATA1234\|NVME5678' $S; then
    ok "(DISCOS) ningún número de serie entero, ni en INFORME.TXT, ni en DISCOS.LOG, ni por el puerto serie"
  else ko "(DISCOS) sale un número de serie entero"; fi
  L=$D/DISCOS.LOG
  mtype -i $Y/d.img@@1M ::TERMO/DISCOS.LOG 2> /dev/null | tr -d '\r' > $L
  if grep -q '^  ATRIB   9 ' $L && grep -q '^  NVME critico=00 ' $L && grep -q '^nvme: disco QEMU NVMe Ctrl, 64 MB, sectores de 512' $L && grep -q '^ESCRITOS_DENTRO 0$' $L; then
    ok "(DISCOS) DISCOS.LOG: los atributos uno a uno, el registro de NVMe, lo que ha visto nvme.cas y 0 escritos"
  else ko "(DISCOS) DISCOS.LOG:"; head -n 12 $L | sed 's/^/        /'; fi
  if ! grep -q "excepción\|\*\*\*" $S; then ok "(DISCOS) con los tres discos, ni una excepción ni una queja del núcleo"; else ko "(DISCOS) el núcleo se queja: $(grep "excepción\|\*\*\*" $S | head -n 2)"; fi
}
