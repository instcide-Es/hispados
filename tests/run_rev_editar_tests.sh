#!/bin/bash
# Revisión de EDITAR (EDIT), GESTOR y EXPANDIR (EXPAND): casos que perdían o
# estropeaban datos o colgaban el programa.
#   1) EDIT y GESTOR con teclas reales (monitor de QEMU); la pantalla de texto
#      se lee de la memoria de vídeo (pmemsave) para comprobar los mensajes.
#   2) EDIT al guardar en un disco casi lleno.
#   3) EXPANDIR por la consola serie, con un segundo disquete casi lleno en B:.
# Review tests for EDIT, GESTOR and EXPAND (data loss / hangs).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# texto de una captura de la memoria de vídeo (80x25, CP437)
scr() { python3 -c '
import sys
d = open(sys.argv[1], "rb").read()[0:4000:2]
for i in range(0, len(d), 80): print(d[i:i+80].decode("cp437").rstrip())' "$1" 2>/dev/null; }
# deja en la imagen $1 solo $2 bytes libres (con un archivo de relleno)
fill() { local free; free=$(mdir -i "$1" :: | sed -n 's/ //g;s/^\([0-9]*\)bytesfree.*/\1/p')
  head -c $((free - $2)) /dev/zero > $T/RELLENO.BIN; mcopy -i "$1" $T/RELLENO.BIN ::RELLENO.BIN; }

# ---------------------------------------------------------------- 1) EDIT y GESTOR
cp "$IMG" $T/f.img
python3 - $T <<'PYEOF'
import sys
d = sys.argv[1]
# mayor que el búfer de EDIT (61440): 70000 bytes de líneas numeradas
g = b''.join(b'linea %05d de un archivo grande\r\n' % i for i in range(3000))[:70000]
open(d + '/GRANDE.TXT', 'wb').write(g)
open(d + '/LFTAB.TXT', 'wb').write(b'a\tb\nc')          # solo LF, tabulador, sin salto final
open(d + '/LFTAB.OK', 'wb').write(b'a\tb\r\nc')
open(d + '/VACIO.TXT', 'wb').write(b'')
open(d + '/RO.TXT', 'wb').write(b'solo lectura\r\n')
open(d + '/CRZ.TXT', 'wb').write(b'uno\rdos\r\n\x1atres\r\n')   # CR suelto y Ctrl+Z en medio
open(d + '/LINEAS.TXT', 'wb').write(b'\n' * 61439)       # miles de líneas
open(d + '/LINEAS.OK', 'wb').write(b'\r\n' * 61439 + b'z')
t = b'abcdefg\n' * 7680                                  # justo 61440 bytes: texto lleno
open(d + '/TOPE.TXT', 'wb').write(t)
open(d + '/TOPE.OK', 'wb').write((t[:1] + t[2:]).replace(b'\n', b'\r\n'))
PYEOF
for f in GRANDE.TXT LFTAB.TXT VACIO.TXT RO.TXT TOPE.TXT CRZ.TXT LINEAS.TXT; do mcopy -i $T/f.img $T/$f ::$f; done
mattrib -i $T/f.img +r ::RO.TXT
mmd -i $T/f.img ::QQ ::QQ/SUB
mcopy -i $T/f.img $T/RO.TXT ::QQ/F.TXT
mcopy -i $T/f.img $T/RO.TXT ::QQ/SUB/G.TXT
printf '%s\n' \
  "type EDIT GRANDE.TXT" "key ret" "wait 3" "mon pmemsave 0xb8000 4000 \"$T/s_grande.bin\"" \
  "type x" "key esc" "wait 1" "type s" "wait 3" "key ret" "wait 1" \
  "type EDIT NODIR\\X.TXT" "key ret" "wait 2" "type ECHO HOLA>MARCA.TXT" "key ret" "wait 1" \
  "key esc" "type n" "key ret" "wait 1" \
  "type EDIT LFTAB.TXT" "key ret" "wait 2" "key f2" "wait 1" "key esc" "wait 1" \
  "type EDIT VACIO.TXT" "key ret" "wait 2" "key f2" "wait 1" "key esc" "wait 1" \
  "type EDIT CRZ.TXT" "key ret" "wait 2" "key f2" "wait 1" "key esc" "wait 1" \
  "type EDIT RO.TXT" "key ret" "wait 2" "type x" "key f2" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_ro.bin\"" \
  "key esc" "wait 1" "type n" "wait 1" \
  "type GESTOR" "key ret" "wait 2" "type h" "type h" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_hh.bin\"" \
  "key ret" "wait 3" "key ret" "wait 2" "mon pmemsave 0xb8000 4000 \"$T/s_gs.bin\"" \
  "type g" "wait 1" "key ret" "wait 3" \
  "mon pmemsave 0xb8000 4000 \"$T/s_gg.bin\"" "key ret" "wait 2" "type q" "wait 1" "key ret" "wait 2" \
  "key f8" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_f8.bin\"" "type s" "wait 3" "key ret" "wait 2" \
  "key f6" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_f6.bin\"" "type ZZ" "key ret" "wait 3" "key ret" "wait 2" \
  "key esc" "wait 1" \
  "type SET COMSPEC=A:\\NOHAY.COM" "key ret" "wait 1" \
  "type GESTOR" "key ret" "wait 2" "key f7" "wait 1" "type NUEVA" "key ret" "wait 2" \
  "mon pmemsave 0xb8000 4000 \"$T/s_exec.bin\"" "key ret" "wait 1" "key esc" "wait 1" \
  "type SET COMSPEC=A:\\COMMAND.COM" "key ret" "wait 1" \
  "type EDIT LINEAS.TXT" "key ret" "wait 3" "key ctrl-end" "wait 5" "type z" "key f2" "wait 8" "key esc" "wait 1" \
  "type EDIT TOPE.TXT" "key ret" "wait 3" "key right" "key tab" "wait 2" "key delete" "key f2" "wait 3" \
  "key esc" "wait 1" > $T/guion
tests/tools/screen.sh $T/f.img $T/o $T/guion 2>/dev/null
D=$T/o_disk.img
echo "== Hispa-DOS revisión de EDIT y GESTOR =="
if mtype -i $D ::GRANDE.TXT 2>/dev/null | cmp -s - $T/GRANDE.TXT; then ok "EDIT no trunca un archivo mayor que su búfer (70000 bytes)"; else ko "EDIT ha truncado GRANDE.TXT a $(mtype -i $D ::GRANDE.TXT 2>/dev/null | wc -c) bytes (eran 70000)"; fi
if scr $T/s_grande.bin | grep -q "demasiado grande"; then ok "EDIT avisa de que el archivo es demasiado grande"; else ko "EDIT no avisa del archivo demasiado grande"; fi
if mtype -i $D ::MARCA.TXT 2>/dev/null | grep -q HOLA; then ok "EDIT no abre un archivo de una carpeta que no existe (no se podría guardar)"; else ko "EDIT abre un texto que nunca se podrá guardar (carpeta inexistente)"; fi
if mtype -i $D ::LFTAB.TXT 2>/dev/null | cmp -s - $T/LFTAB.OK; then ok "EDIT conserva tabuladores y el final sin salto de línea (LF -> CR LF)"; else ko "EDIT LFTAB.TXT"; mtype -i $D ::LFTAB.TXT | od -c | head -3; fi
if [ "$(mtype -i $D ::VACIO.TXT 2>/dev/null | wc -c)" = 0 ] && mdir -i $D ::VACIO.TXT >/dev/null 2>&1; then ok "EDIT guarda un archivo de 0 bytes"; else ko "EDIT archivo de 0 bytes"; fi
if mtype -i $D ::CRZ.TXT 2>/dev/null | cmp -s - $T/CRZ.TXT; then ok "EDIT conserva un CR suelto y lo que hay tras un Ctrl+Z"; else ko "EDIT pierde datos con CR sueltos o Ctrl+Z: $(mtype -i $D ::CRZ.TXT 2>/dev/null | od -An -c | tr -s ' ' | head -2 | tr -d '\n')"; fi
if mtype -i $D ::RO.TXT 2>/dev/null | cmp -s - $T/RO.TXT; then ok "EDIT no toca un archivo de solo lectura"; else ko "EDIT solo lectura"; fi
if scr $T/s_ro.bin | grep -q "No se ha podido guardar"; then ok "EDIT avisa de que no ha podido guardar (solo lectura)"; else ko "EDIT aviso de solo lectura"; fi
if scr $T/s_hh.bin | grep -q "│ HELLOEXE.EXE" && scr $T/s_gs.bin | grep -q "Gestor de archivos"; then ok "GESTOR se vuelve a dibujar tras ejecutar un programa (GS)"; else ko "GESTOR: pantalla en blanco tras ejecutar un programa (dibuja en otro segmento)"; fi
if scr $T/s_gg.bin | grep -q "demasiado grande" && scr $T/s_gg.bin | grep -q "Pulse una tecla"; then ok "GESTOR: Intro sobre un archivo demasiado grande deja leer el aviso de EDITAR"; else ko "GESTOR: Intro sobre un archivo demasiado grande"; fi
if scr $T/s_f8.bin | grep -q "TODO su contenido"; then ko "GESTOR ofrece borrar «..» (la carpeta superior)"; else ok "GESTOR no ofrece borrar «..»"; fi
if scr $T/s_f6.bin | grep -q "Nuevo nombre"; then ko "GESTOR ofrece renombrar «..»"; else ok "GESTOR no ofrece renombrar «..»"; fi
if mtype -i $D ::QQ/F.TXT >/dev/null 2>&1 && mtype -i $D ::QQ/SUB/G.TXT >/dev/null 2>&1; then ok "GESTOR: F8 y F6 sobre «..» no borran ni mueven nada"; else ko "GESTOR: F8/F6 sobre «..» han estropeado QQ"; fi
if scr $T/s_exec.bin | grep -q "No se ha podido ejecutar"; then ok "GESTOR avisa si no puede ejecutar COMMAND.COM"; else ko "GESTOR no avisa de que la orden no se ha ejecutado"; fi
if mtype -i $D ::LINEAS.TXT 2>/dev/null | cmp -s - $T/LINEAS.OK; then ok "EDIT: Ctrl+Fin en un texto de 61439 líneas responde enseguida"; else ko "EDIT: Ctrl+Fin en un texto de 61439 líneas deja el editor parado (más de 15 s)"; fi
if mtype -i $D ::TOPE.TXT 2>/dev/null | cmp -s - $T/TOPE.OK; then ok "EDIT: Tab con el texto lleno no cuelga el editor"; else ko "EDIT: Tab con el texto lleno (colgado o mal guardado: $(mtype -i $D ::TOPE.TXT 2>/dev/null | wc -c) bytes)"; fi
if fsck.fat -n $D >/dev/null 2>&1; then ok "fsck.fat tras EDIT y GESTOR"; else ko "fsck.fat tras EDIT y GESTOR"; fi
if [ -n "${VERBOSE:-}" ]; then for s in grande ro hh gs gg f8 f6 exec; do echo "--- $s"; scr $T/s_$s.bin; done; fi

# ---------------------------------------------------------------- 2) EDIT, disco casi lleno
cp "$IMG" $T/g.img
python3 -c "open('$T/AL.TXT','wb').write(b'a\n'*15000)"      # 30000 bytes; con CR LF, 45000
mcopy -i $T/g.img $T/AL.TXT ::AL.TXT
# un nombre corto con un espacio dentro («WA WB.TXT»), junto a WA y WB.TXT
printf 'no me borres\r\n' > $T/WA; cp $T/WA $T/WB.TXT; cp $T/WA $T/ESPACIO.TXT
for f in WA WB.TXT ESPACIO.TXT; do mcopy -i $T/g.img $T/$f ::$f; done
python3 - $T/g.img <<'PYEOF'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
i = d.find(b'ESPACIO TXT', 19 * 512, 33 * 512)
assert i >= 0 and (i - 19 * 512) % 32 == 0
d[i:i+11] = b'WA WB   TXT'
open(sys.argv[1], 'wb').write(d)
PYEOF
# ¿está la entrada $2 (11 caracteres) en la carpeta raíz de la imagen $1?
inroot() { python3 -c '
import sys
d = open(sys.argv[1], "rb").read()[19*512:33*512]
sys.exit(0 if any(d[i:i+11] == sys.argv[2].encode() for i in range(0, len(d), 32)) else 1)' "$1" "$2"; }
fill $T/g.img 2048
printf '%s\n' \
  "type EDIT AL.TXT" "key ret" "wait 3" "type x" "key f2" "wait 4" "mon pmemsave 0xb8000 4000 \"$T/s_lleno.bin\"" \
  "key esc" "wait 1" "type n" "wait 2" \
  "type GESTOR" "key ret" "wait 2" "type w" "type w" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_w.bin\"" \
  "key f8" "wait 1" "mon pmemsave 0xb8000 4000 \"$T/s_sp.bin\"" "type s" "wait 3" "key down" "wait 1" \
  "key esc" "wait 1" > $T/guion2
tests/tools/screen.sh $T/g.img $T/p $T/guion2 2>/dev/null
D=$T/p_disk.img
if mtype -i $D ::AL.TXT 2>/dev/null | cmp -s - $T/AL.TXT; then ok "EDIT: si no cabe en el disco, el original queda intacto"; else ko "EDIT: disco lleno al guardar, el original queda con $(mtype -i $D ::AL.TXT 2>/dev/null | wc -c) bytes (eran 30000)"; fi
if scr $T/s_lleno.bin | grep -q "No cabe en el disco"; then ok "EDIT avisa de que no cabe en el disco"; else ko "EDIT aviso de disco lleno"; fi
if scr $T/s_w.bin | grep -q "│ WA WB.TXT" && inroot $D "WA         " && inroot $D "WB      TXT" && inroot $D "WA WB   TXT"; then ok "GESTOR: F8 sobre «WA WB.TXT» no borra WA ni WB.TXT"; else ko "GESTOR: F8 sobre «WA WB.TXT» ha borrado otros archivos (WA / WB.TXT)"; fi
if scr $T/s_sp.bin | grep -q "Nombre con espacios"; then ok "GESTOR avisa del nombre con espacios"; else ko "GESTOR no avisa del nombre con espacios"; fi
if fsck.fat -n $D >/dev/null 2>&1; then ok "fsck.fat tras el disco lleno"; else ko "fsck.fat tras el disco lleno"; fsck.fat -n $D | head -5; fi
if [ -n "${VERBOSE:-}" ]; then for s in lleno w sp; do echo "--- $s"; scr $T/s_$s.bin; done; fi

# ---------------------------------------------------------------- 3) EXPANDIR
cp "$IMG" $T/h.img
dd if=/dev/zero of=$T/b.img bs=1k count=1440 status=none
mformat -i $T/b.img -f 1440 ::
head -c 30000 /dev/urandom | base64 > $T/DATOS.TXT
python3 - $T <<'PYEOF'
import struct, sys
d = sys.argv[1]
data = open(d + '/DATOS.TXT', 'rb').read()
def kwaj(method, flags, extra, body):
    return b'KWAJ\x88\xf0\x27\xd1' + struct.pack('<HHH', method, 14 + len(extra), flags) + extra + body
def szdd(body):                      # SZDD solo con literales
    out = b'SZDD\x88\xf0\x27\x33A' + b'T' + struct.pack('<I', len(body))
    for i in range(0, len(body), 8):
        out += b'\xff' + body[i:i+8]
    return out
w = lambda n, b: open(d + '/' + n, 'wb').write(b)
w('D1.BIN', data[:100]); w('D2.BIN', data[:5000])
w('M1.TX_', kwaj(0, 1, struct.pack('<I', 100), data[:100]))      # con tamaño
w('M2.TX_', kwaj(0, 0, b'', data[:5000]))                        # sin tamaño
w('K0.TX_', kwaj(0, 1, struct.pack('<I', len(data)), data))
w('CORTO.TX_', szdd(data)[:15000])                               # SZDD cortado
w('CORTO2.TX_', kwaj(1, 1, struct.pack('<I', len(data)), bytes(b ^ 0xFF for b in data[:1000])))
w('ROTO.TX_', kwaj(0, 0x0C, struct.pack('<H', 0xF000) + b'NOMBRE\0', data[:200]))
w('PLANO.TXT', data[:20000])
w('MINI.TXT', b'hola\n')
w('SZ.TX_', szdd(data))
PYEOF
for f in M1.TX_ M2.TX_ K0.TX_ CORTO.TX_ CORTO2.TX_ ROTO.TX_ PLANO.TXT MINI.TXT SZ.TX_; do mcopy -i $T/h.img $T/$f ::$f; done
mmd -i $T/b.img ::DIR
fill $T/b.img 8192
CMDS=("MD SAL" "MD VAR"
      "EXPANDIR M?.TX_ VAR"
      "EXPANDIR CORTO.TX_ SAL\\CORTO.TXT" "IF ERRORLEVEL 1 ECHO NIVEL-CORTO"
      "EXPANDIR CORTO2.TX_ SAL\\CORTO2.TXT"
      "EXPANDIR M?.TX_ SAL\\UNO.TXT" "IF ERRORLEVEL 1 ECHO NIVEL-VARIOS"
      "EXPANDIR K0.TX_ .\\K0.TX_"
      "EXPANDIR K0.TX_ B:\\K0.TXT" "IF ERRORLEVEL 1 ECHO NIVEL-LLENO"
      "EXPANDIR PLANO.TXT SAL" "IF ERRORLEVEL 1 ECHO MAL-NIVEL-PLANO"
      "EXPANDIR MINI.TXT SAL\\MINI2.TXT"
      "EXPANDIR PLANO.TXT"
      "EXPANDIR ROTO.TX_ SAL\\ROTO.TXT"
      "EXPANDIR SZ.TX_ SAL\\"
      "EXPANDIR M1.TX_ B:\\DIR\\" "EXPANDIR M2.TX_ B:\\")
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; sleep 3; done; } | \
  timeout 150 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/h.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
  -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
echo "== Hispa-DOS revisión de EXPANDIR =="
D=$T/h.img
same() { if mtype -i $1 "::$2" 2>/dev/null | cmp -s - "$3"; then ok "$4"; else ko "$4 ($(mtype -i $1 "::$2" 2>/dev/null | wc -c) bytes)"; fi; }
nohave() { if mdir -b -i $1 "::$2" >/dev/null 2>&1; then ko "$3: queda $2 ($(mtype -i $1 "::$2" 2>/dev/null | wc -c) bytes)"; else ok "$3"; fi; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$2"; else ko "$2"; fi; }
same $D VAR/M1.TX_ $T/D1.BIN "varios archivos: el primero (con tamaño en la cabecera)"
same $D VAR/M2.TX_ $T/D2.BIN "varios archivos: el segundo (sin tamaño) no hereda el tamaño del primero"
check "CORTO.TX_: los datos comprimidos están dañados" "SZDD cortado: se avisa"
nohave $D SAL/CORTO.TXT "SZDD cortado: no deja una salida a medias"
if grep -qx "NIVEL-CORTO" $T/log.txt; then ok "SZDD cortado: ERRORLEVEL 1"; else ko "SZDD cortado: ERRORLEVEL"; fi
nohave $D SAL/CORTO2.TXT "KWAJ cortado: no deja una salida a medias"
nohave $D SAL/UNO.TXT "varios archivos a un solo nombre: no se pisan unos a otros"
if grep -qx "NIVEL-VARIOS" $T/log.txt; then ok "varios archivos a un solo nombre: ERRORLEVEL 1"; else ko "varios archivos a un solo nombre: ERRORLEVEL"; fi
same $D K0.TX_ $T/K0.TX_ "destino = origen escrito de otra forma (.\\K0.TX_): el origen no se pierde"
check "K0.TX_: no cabe en el disco de destino" "disco lleno: se avisa (y no «datos dañados»)"
nohave $T/b.img K0.TXT "disco lleno: no deja una salida a medias"
if grep -qx "NIVEL-LLENO" $T/log.txt; then ok "disco lleno: ERRORLEVEL 1"; else ko "disco lleno: ERRORLEVEL"; fi
same $D SAL/PLANO.TXT $T/PLANO.TXT "un archivo sin comprimir se copia tal cual (como EXPAND de MS-DOS)"
if grep -qx "MAL-NIVEL-PLANO" $T/log.txt; then ko "sin comprimir: ERRORLEVEL 0"; else ok "sin comprimir: ERRORLEVEL 0"; fi
same $D SAL/MINI2.TXT $T/MINI.TXT "un archivo sin comprimir más corto que la cabecera"
same $D PLANO.TXT $T/PLANO.TXT "sin comprimir y sin destino: el origen queda intacto"
check "ROTO.TX_: los datos comprimidos están dañados" "KWAJ con la cabecera rota: se avisa"
nohave $D SAL/ROTO.TXT "KWAJ con la cabecera rota: no crea nada"
same $D SAL/SZ.TXT $T/DATOS.TXT "SZDD con destino «SAL\\» (carpeta acabada en barra)"
same $T/b.img DIR/M1.TX_ $T/D1.BIN "destino «B:\\DIR\\»"
same $T/b.img M2.TX_ $T/D2.BIN "destino «B:\\» (carpeta raíz)"
if fsck.fat -n $D >/dev/null 2>&1 && fsck.fat -n $T/b.img >/dev/null 2>&1; then ok "fsck.fat"; else ko "fsck.fat"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/MD SAL/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
