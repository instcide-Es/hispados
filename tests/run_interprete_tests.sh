#!/bin/bash
# Prueba del intérprete (revisión): COPY /Y /-Y /V /A /B y COPYCMD, pregunta
# al sobrescribir (fuera de los lotes), COPY con «+» suelto y comodines en el
# destino, DEL /P, REN *.* en una subcarpeta, PATH=..., PROMPT=..., ECHO, ECHO;
# IF EXIST carpeta\NUL, FOR ... DO CALL, GOTO a una etiqueta que no existe,
# líneas que desbordaban los búferes, muchas tuberías, TEMP, DATE/TIME con
# parámetro y CD X:.
# Command interpreter review test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
bat() { printf '%s\r\n' "${@:2}" > $T/tmp.bat; mcopy -o -i $T/f.img $T/tmp.bat "::$1"; }
L100=$(printf 'x%.0s' $(seq 100))
LBL=$(printf 'a%.0s' $(seq 180))

# --- archivos por lotes de la prueba ---
bat REV.BAT \
  '@ECHO OFF' \
  'PATH=A:\HISPADOS;A:\BIN' \
  'ECHO [path] %PATH%' \
  'ECHO,' \
  'ECHO;punto y coma' \
  'ECHO=igual' \
  'ECHO [noexiste]%NOEXISTE%[fin]' \
  'IF "%9"=="" ECHO [vacia] OK' \
  'IF EXIST A:\DOCS\NUL ECHO [nul] carpeta existe' \
  'IF EXIST A:\NOHAY\NUL ECHO [nul] MAL' \
  'IF NOT EXIST A:\NOHAY\NUL ECHO [nul] no existe OK' \
  'COPY README.TXT L1.TXT' \
  'COPY README.TXT L1.TXT' \
  'ECHO [lote] copiado sin preguntar' \
  'FOR %%F IN (uno dos tres cuatro cinco) DO CALL A:\SUB.BAT %%F' \
  'ECHO [for-call] fin' \
  '@' \
  'IF EXIST A:\DOCS\*.TXT LLAMAR A:\SUB.BAT seis' \
  'ECHO tubos | UPPER | UPPER | UPPER | UPPER | UPPER | UPPER | UPPER | UPPER' \
  'dir/b l1.*' \
  'cd..' \
  'GOTO noexiste' \
  'ECHO [goto] MAL'
bat SUB.BAT \
  'ECHO [sub] %1' \
  'FOR %%G IN (x y) DO ECHO [anidado] %1-%%G'
# una línea que al sustituir %1 pasa de 800 caracteres (desbordaba eline)
bat OVR.BAT \
  '@ECHO OFF' \
  'ECHO %1%1%1%1%1%1%1%1' \
  "GOTO etiq$LBL" \
  ":etiq$LBL" \
  'ECHO [desborde] el lote sigue'
# doce variables de 100 caracteres: no caben en 1 KB de entorno
{ printf '@ECHO OFF\r\n'; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do printf 'SET V%s=%s\r\n' $i "$L100"; done; printf 'ECHO [env] fin\r\n'; } > $T/tmp.bat
mcopy -o -i $T/f.img $T/tmp.bat ::ENV.BAT
bat CPY.BAT \
  '@ECHO OFF' \
  'COPY README.TXT C1.TXT' \
  'ECHO [copycmd-lote] fin'
printf 'antes\032despues\r\n' > $T/z.txt
mcopy -o -i $T/f.img $T/z.txt ::Z.TXT

CMDS=(
  "REV"
  "ECHO"
  "OVR $L100"
  "ECHO [m1]"
  "COPY README.TXT C1.TXT"
  "COPY AUTOEXEC.BAT C1.TXT" "N"
  "TYPE C1.TXT"
  "ECHO [m2]"
  "COPY AUTOEXEC.BAT C1.TXT" "S"
  "TYPE C1.TXT"
  "ECHO [m3]"
  "COPY /Y README.TXT C1.TXT"
  "COPY README.TXT C1.TXT /Y"
  "copy/y readme.txt c1.txt"
  "ECHO [m4]"
  "MD D2"
  "COPY README.TXT D2\\A.TXT"
  "COPY README.TXT D2\\B.TXT"
  "COPY README.TXT D2\\C.TXT"
  "MD D3"
  "COPY D2\\*.* D3"
  "ECHO [m5]"
  "COPY D2\\*.* D3" "N" "N" "N"
  "ECHO [m6]"
  "COPY D2\\*.* D3" "T"
  "ECHO [m7]"
  "SET COPYCMD=/Y"
  "COPY README.TXT C1.TXT"
  "ECHO [m8]"
  "COPY /-Y AUTOEXEC.BAT C1.TXT" "N"
  "ECHO [m9]"
  "SET COPYCMD=/-Y"
  "CPY" "S"
  "SET COPYCMD="
  "ECHO [m10]"
  "COPY /B README.TXT + AUTOEXEC.BAT J.TXT /V"
  "TYPE J.TXT"
  "ECHO [m11]"
  "COPY /A Z.TXT ZA.TXT"
  "TYPE ZA.TXT"
  "ECHO."
  "ECHO [m12]"
  "COPY /B Z.TXT ZB.TXT"
  "TYPE ZB.TXT"
  "ECHO [m12b]"
  "COPY /A Z.TXT+README.TXT ZC.TXT"
  "TYPE ZC.TXT"
  "ECHO [m13]"
  "COPY README.TXT *.BAK"
  "COPY D2\\*.TXT D3\\*.CPA"
  "DIR /B D3"
  "DIR /B"
  "ECHO [m14]"
  "COPY /Q README.TXT Q.TXT"
  "COPY README.TXT README.TXT"
  "COPY README.TXT NUL"
  "ECHO [m15]"
  "DEL D3\\*.TXT /P" "S" "N" "S"
  "DIR /B D3\\*.TXT"
  "ECHO [m16]"
  "DEL /P D3\\B.TXT" "S"
  "DIR /B D3\\*.TXT"
  "ECHO [m17]"
  "CD D2"
  "REN *.* *.OLD"
  "DIR /B"
  "CD \\"
  "ECHO [m18]"
  "PROMPT=[\$N]"
  "PROMPT \$P\$G"
  "CD A:"
  "DATE 05-06-2027"
  "TIME 12:34:50"
  "DATE 45-13-2027"
  "TIME 25:00"
  "ECHO [m19]"
  "SET TEMP=A:\\D2"
  "ECHO con temp | UPPER"
  "ECHO a|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER|UPPER"
  "ECHO [m20]"
  "SET TEMP="
  "COMMAND /E:4096"
  "ENV"
  "EXIT"
  "ECHO [m21]"
  "ENV"
  "ECHO [fin]"
)
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; case "$c" in REV|ENV|OVR*|COMMAND*) sleep 2.5;; *) sleep 0.9;; esac; done; sleep 1; } | \
  timeout 170 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
ok()  { echo "  OK   $1"; }
bad() { echo "  FAIL $1"; fail=1; }
check()  { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else bad "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then bad "(no debía salir) $1"; else ok "sin «$1»"; fi; }
# trozo del registro entre dos marcas «ECHO [mN]»
sec() { awk -v a="ECHO [$1]" -v b="ECHO [$2]" 'index($0,b){f=0} f{print} index($0,a){f=1}' $T/log.txt; }
insec()  { if sec "$1" "$2" | grep -qF -- "$3"; then ok "$4"; else bad "$4"; fi; }
ninsec() { if sec "$1" "$2" | grep -qF -- "$3"; then bad "$4"; else ok "$4"; fi; }
cntsec() { n=$(sec "$1" "$2" | grep -cF -- "$3"); if [ "$n" = "$4" ]; then ok "$5"; else bad "$5 (salen $n)"; fi; }

echo "== Hispa-DOS: intérprete (revisión) =="
# --- lote REV.BAT ---
check  "[path] A:\\HISPADOS;A:\\BIN"
check  "punto y coma"
check  "igual"
checkn "Comando o nombre de archivo incorrecto"
check  "[noexiste][fin]"
check  "[vacia] OK"
check  "[nul] carpeta existe"
checkn "[nul] MAL"
check  "[nul] no existe OK"
check  "[lote] copiado sin preguntar"
if sed -n '1,/\[lote\]/p' $T/log.txt | grep -qF "Sobrescribir"; then bad "COPY preguntó dentro de un lote"; else ok "COPY no pregunta dentro de un lote"; fi
orden=$(grep -F -e "[sub] " -e "[for-call]" $T/log.txt | grep -v seis | tr '\n' ' ')
if [ "$orden" = "[sub] uno [sub] dos [sub] tres [sub] cuatro [sub] cinco [for-call] fin " ]; then ok "FOR ... DO CALL: en orden y antes de seguir"; else bad "FOR ... DO CALL: $orden"; fi
check  "[anidado] cinco-y"
check  "[sub] seis"
checkn "Demasiados archivos de proceso por lotes"
check  "TUBOS"
check  "L1.TXT"
check  "No se encontró la etiqueta"
checkn "[goto] MAL"
if sed -n '/No se encontró la etiqueta/,/\[m1\]/p' $T/log.txt | grep -qF "ECHO está activado"; then ok "tras GOTO fallido vuelve el eco"; else bad "tras GOTO fallido el eco sigue apagado"; fi
check  "[desborde] el lote sigue"
# --- COPY: pregunta, /Y, /-Y, COPYCMD ---
ninsec m1 m2 "SET HISPA" "respuesta N: no se sobrescribe"
insec  m1 m2 "¿Sobrescribir C1.TXT (Sí/No/Todos)?" "pregunta al sobrescribir en la línea de órdenes"
insec  m1 m2 "Welcome to Hispa-DOS" "el archivo sigue como estaba"
insec  m2 m3 "SET HISPA" "respuesta S: se sobrescribe"
ninsec m3 m4 "Sobrescribir" "/Y no pregunta (delante, detrás y pegado)"
cntsec m3 m4 "1 archivo(s) copiado(s)" 3 "/Y copia las tres veces"
ninsec m4 m5 "Sobrescribir" "sin archivo previo no pregunta"
cntsec m5 m6 "Sobrescribir" 3 "N, N, N: pregunta por cada uno"
insec  m5 m6 "0 archivo(s) copiado(s)" "N, N, N: no copia ninguno"
cntsec m6 m7 "Sobrescribir" 1 "T (Todos): pregunta una sola vez"
insec  m6 m7 "3 archivo(s) copiado(s)" "T (Todos): copia los tres"
ninsec m7 m8 "Sobrescribir" "COPYCMD=/Y no pregunta"
insec  m8 m9 "Sobrescribir" "/-Y en la línea manda sobre COPYCMD=/Y"
insec  m8 m9 "0 archivo(s) copiado(s)" "/-Y y N: no copia"
insec  m9 m10 "Sobrescribir" "COPYCMD=/-Y pregunta también en un lote"
insec  m9 m10 "[copycmd-lote] fin" "el lote sigue tras la pregunta"
# --- /A /B /V, «+» suelto, comodines en el destino ---
insec  m10 m11 "This file was read" "COPY /B a + b destino /V: primera parte"
insec  m10 m11 "PATH A:\\HISPADOS" "COPY /B a + b destino /V: segunda parte"
insec  m11 m12 "antes" "/A copia hasta Ctrl+Z"
ninsec m11 m12 "despues" "/A no copia lo que sigue a Ctrl+Z"
insec  m12 m12b "despues" "/B copia todo"
insec  m12b m13 "antesWelcome to Hispa-DOS" "COPY /A a+b: cada parte hasta su Ctrl+Z"
ninsec m12b m13 "despues" "COPY /A a+b: sin lo que sigue a Ctrl+Z"
insec  m13 m14 "README.BAK" "COPY README.TXT *.BAK"
insec  m13 m14 "C.CPA" "COPY D2\\*.TXT D3\\*.CPA"
if sec m13 m14 | grep -qx "+"; then bad "COPY a + b creó un archivo llamado «+»"; else ok "COPY a + b no crea un archivo «+»"; fi
insec  m14 m15 "Parámetro no válido" "opción desconocida: error"
insec  m14 m15 "sobre sí mismo" "copia sobre sí mismo: aviso"
ninsec m14 m15 "Sobrescribir" "a NUL no pregunta"
# --- DEL /P, REN ---
cntsec m15 m16 "¿Eliminar (S/N)?" 3 "DEL /P pregunta por cada archivo"
insec  m15 m16 "B.TXT" "DEL /P con N conserva el archivo"
if sec m15 m16 | grep -qx "A.TXT"; then bad "DEL /P con S no borró A.TXT"; else ok "DEL /P con S borra"; fi
insec  m16 m17 "No se encontró el archivo" "DEL /P archivo (opción delante)"
ninsec m17 m18 "duplicado" "REN *.* en una subcarpeta sin errores por «.» y «..»"
insec  m17 m18 "C.OLD" "REN *.* *.OLD"
# --- PROMPT=, CD X:, DATE, TIME, TEMP, tuberías ---
insec  m18 m19 "[A]PROMPT" "PROMPT=texto"
if sec m18 m19 | grep -qx 'A:\\'; then ok "CD A: muestra la carpeta"; else bad "CD A: muestra la carpeta"; fi
cntsec m18 m19 "no válida" 2 "DATE/TIME con parámetro: aceptan lo válido y rechazan lo imposible"
insec  m19 m20 "CON TEMP" "tubería con TEMP"
ninsec m19 m20 "Error al crear" "tubería con TEMP sin errores"
insec  m19 m20 "Demasiadas tuberías" "doce tuberías: aviso limpio"
insec  m20 m21 "[env] fin" "COMMAND /E:4096: el lote de 12 variables acaba"
ninsec m20 m21 "No queda espacio en el entorno" "COMMAND /E:4096: caben 1,2 KB de variables"
insec  m21 fin "No queda espacio en el entorno" "sin /E el entorno sigue siendo de 1 KB"
check "[fin]"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat $T/log.txt; fi
rm -rf $T
exit $fail
