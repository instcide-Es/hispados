#!/bin/bash
# Fecha y hora de verdad: INT 21h 2Bh/2Dh (FECHA y HORA del intérprete)
# cambian la fecha del núcleo, los tics de la BIOS y el reloj de tiempo real
# (tras reiniciar sigue la fecha puesta); al pasar la medianoche la fecha
# avanza; las fechas imposibles se rechazan; los archivos nuevos llevan la
# fecha y la hora puestas.
#
# Y el Reloj de Hispa Ventanas (ventanas/reloj.inc), en sus dos formas (esfera
# con agujas y solo las cifras), a dos tamaños (la ventana de siempre y
# maximizada) y con la hora fijada: un residente de prueba (FIJAHORA, que se
# ensambla aquí mismo) contesta a INT 21h 2Ah/2Ch con una fecha y una hora
# fijas de una tabla de ocho, que se elige desde fuera con las luces del
# teclado (Bloq Despl, Bloq Num y Bloq Mayús: 0040:0017). Así las capturas
# son repetibles y se puede mirar punto a punto dónde está cada aguja y qué
# dicen las cifras (tests/tools/ventanas_check.py y unas comprobaciones
# propias: esfera redonda, sesenta marcas, lectura de los siete segmentos).
# Deja capturas en build/reloj_*.png.
# Real date and time: 2Bh/2Dh set the kernel date, the BIOS ticks and the RTC.
# And the Hispa Ventanas clock: both faces, two sizes, with a fixed time.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
# guion: una orden por línea; "@N" espera N segundos; "!orden" va al monitor
{ sleep 5
  while IFS= read -r c; do
    case "$c" in
      @*) sleep "${c#@}";;
      !*) mon "${c#!}";;
      *) printf '%s\r' "$c";;
    esac
    sleep 0.7
  done <<'EOF'
ECHO uno
FECHA
HORA
FECHA 30-02-2024
FECHA 29-02-2023
FECHA 31-04-2026
FECHA 01-01-1979
HORA 24:00
HORA 12:60
ECHO dos
FECHA 29-02-2024
FECHA
HORA 9:08:07
HORA
ECHO hola > RELOJ.TXT
DIR RELOJ.TXT
ECHO tres
FECHA 31-12-2027
HORA 23:59:57
@5
FECHA
HORA
ECHO cuatro
HORA 23:59:58
@3
HORA 23:59:58
@3
FECHA
ECHO cinco
FECHA 15-08-2031
HORA 6:07:08
!system_reset
@7
FECHA
HORA
ECHO fin
EOF
  sleep 2
  mon quit
} | timeout 90 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait -parallel none \
      -rtc base=2026-03-10T10:20:30 -drive file=$T/f.img,format=raw,if=floppy -boot a > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log
sec() { sed -n "/^$1\$/,/^$2\$/p" $T/log; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if sec "$1" "$2" | grep -qE -- "$3"; then ok "$4"; else ko "$4"; fi; }
echo "== Hispa-DOS: fecha y hora (2Ah-2Dh, reloj de tiempo real, medianoche) =="
has uno dos "La fecha actual es Mar 10-03-2026" "al arrancar, la fecha del reloj de tiempo real (Mar 10-03-2026)"
has uno dos "La hora actual es 10:2[0-9]:[0-9][0-9]" "y su hora (10:2x)"
n=$(sec uno dos | grep -c "Fecha no válida")
if [ "$n" = 4 ]; then ok "fechas imposibles rechazadas (30-02-2024, 29-02-2023, 31-04-2026, 1979)"; else ko "fechas imposibles rechazadas: $n de 4"; fi
n=$(sec uno dos | grep -c "Hora no válida")
if [ "$n" = 2 ]; then ok "horas imposibles rechazadas (24:00, 12:60)"; else ko "horas imposibles rechazadas: $n de 2"; fi
has dos tres "La fecha actual es Jue 29-02-2024" "FECHA 29-02-2024 -> Jue 29-02-2024"
has dos tres "La hora actual es  9:08:(0[7-9]|10)" "HORA 9:08:07 -> 9:08:07"
has dos tres "RELOJ +TXT.* 29-02-24 +9:08" "el archivo creado después lleva esa fecha y esa hora"
has tres cuatro "La fecha actual es Sáb 01-01-2028" "medianoche: del 31-12-2027 23:59:57 al Sáb 01-01-2028"
has tres cuatro "La hora actual es  0:00:0[0-9]" "y la hora vuelve a 0:00"
has cuatro cinco "La fecha actual es Lun 03-01-2028" "dos medianoches más (hora puesta dos veces a 23:59:58): Lun 03-01-2028"
sec cinco fin | sed -n '/system_reset/,$p;/Hispa-DOS 0/,$p' > $T/tras
if grep -q "La fecha actual es Vie 15-08-2031" $T/tras; then ok "tras reiniciar, el reloj de tiempo real da la fecha puesta (Vie 15-08-2031)"; else ko "tras reiniciar, la fecha puesta (Vie 15-08-2031)"; fi
if grep -qE "La hora actual es  6:07:[1-5][0-9]" $T/tras; then ok "y la hora puesta sigue corriendo (6:07)"; else ko "y la hora puesta (6:07)"; fi
check=$(grep -c "^fin$" $T/log)
[ "$check" -ge 1 ] && ok "fin" || ko "fin"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log; fi

# =============================================================================
#  El Reloj de Hispa Ventanas
# =============================================================================
echo "== Hispa Ventanas: el Reloj (esfera con agujas y solo las cifras) =="
B=build
CHK="python3 tests/tools/ventanas_check.py"
chk() { $CHK "$@" || fail=1; }
rchk() { python3 $T/reloj_chk.py "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/reloj_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
aparca() { ir 16 320; }                     # el cursor, donde no tape nada
# la hora que da FIJAHORA cambia con las luces del teclado; el Reloj la mira
# cada medio segundo
luz() { tecla "$1"; sleep 1.3; }

# -- FIJAHORA.COM: fecha y hora fijas, según las luces del teclado --
cat > $T/fijahora.asm <<'EOF'
        org 0x100
        jmp inicio
viejo   dd 0
; hora, minuto, segundo, día de la semana (0 = domingo), día, mes, año; el
; número de la fila son los bits 4 a 6 de 0040:0017 (Despl = 1, Num = 2,
; Mayús = 4)
tabla:  db 10, 10, 30, 1, 16, 11    ; 0: lunes 16-11-2026, 10:10:30
        dw 2026
        db 12, 0, 0, 3, 30, 9       ; 1: miércoles 30-09-2026, 12:00:00
        dw 2026
        db 3, 45, 15, 6, 3, 1       ; 2: sábado 3-01-2026, 3:45:15
        dw 2026
        db 23, 59, 59, 4, 31, 12    ; 3: jueves 31-12-2026, 23:59:59
        dw 2026
        db 18, 30, 45, 1, 6, 7      ; 4: lunes 6-07-2026, 18:30:45
        dw 2026
        db 0, 7, 8, 2, 10, 2        ; 5: martes 10-02-2026, 0:07:08
        dw 2026
        db 15, 15, 0, 0, 1, 3       ; 6: domingo 1-03-2026, 15:15:00
        dw 2026
        db 9, 45, 0, 5, 15, 5       ; 7: viernes 15-05-2026, 9:45:00
        dw 2026
nuevo:
        cmp ah, 0x2C
        je .hora
        cmp ah, 0x2A
        je .fecha
        jmp far [cs:viejo]
.hora:
        push bx
        call indice
        mov ch, [cs:bx]
        mov cl, [cs:bx+1]
        mov dh, [cs:bx+2]
        xor dl, dl
        pop bx
        iret
.fecha:
        push bx
        call indice
        mov al, [cs:bx+3]
        mov dl, [cs:bx+4]
        mov dh, [cs:bx+5]
        mov cx, [cs:bx+6]
        pop bx
        iret
indice:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov bl, [0x417]
        pop ax
        pop ds
        mov bh, 0
        shr bx, 1
        shr bx, 1
        shr bx, 1
        shr bx, 1
        and bx, 7
        shl bx, 1
        shl bx, 1
        shl bx, 1
        add bx, tabla
        ret
fin:
inicio:
        mov ax, 0x3521
        int 0x21
        mov [viejo], bx
        mov [viejo+2], es
        mov dx, nuevo
        mov ax, 0x2521
        int 0x21
        mov dx, fin
        add dx, 15
        mov cl, 4
        shr dx, cl
        mov ax, 0x3100
        int 0x21
EOF
nasm -f bin -o $T/FIJAHORA.COM $T/fijahora.asm || ko "no se ha podido ensamblar FIJAHORA"

# -- comprobaciones propias del Reloj sobre una captura --
cat > $T/reloj_chk.py <<'EOF'
"""uso: reloj_chk.py CAPTURA.ppm COMPROBACION...
  redonda CX CY R COLOR NOMBRE      el borde de la esfera es un círculo: todo
                                    alrededor, a R - 0,5 hay COLOR y a R + 1,5 no
  marcas CX CY R1 R2 CARA N         entre los radios R1 y R2 hay N marcas (tramos
                                    que no son del color de la cara) en la vuelta
  cifras X Y AN AL ENC APAG TEXTO   lee los siete segmentos (encendidos de color
                                    ENC, apagados de APAG) que hay en la zona:
                                    «HHMM|:|SS» (_ = cifra apagada; «.» = los dos
                                    puntos apagados; sin segundos, nada tras la raya)
  texto_g X0 Y0 X1 Y1 ESC FG BG TEXTO   el texto, a tamaño ESC (cada punto de la
                                    fuente, un cuadrado de ESC), con la esquina
                                    en la zona
  texto_gn ...                      lo mismo, en negrita
  cuenta X Y AN AL COLOR MIN MAX NOMBRE   puntos de ese color en la zona
"""
import math, os, sys
sys.path.insert(0, os.environ['HERRAMIENTAS'])
import ventanas_check as vc

SEG = {0x3F: '0', 0x06: '1', 0x5B: '2', 0x4F: '3', 0x66: '4', 0x6D: '5', 0x7D: '6',
       0x07: '7', 0x7F: '8', 0x6F: '9', 0: '_'}


def leer_cifras(cap, x, y, an, al, enc, apag):
    def seg(c):
        return vc.es(c, enc) or vc.es(c, apag)
    cols = [[yy for yy in range(y, y + al) if seg(cap.px(xx, yy))] for xx in range(x, x + an)]
    tramos, ini = [], None
    for i, c in enumerate(cols + [[]]):
        if c and ini is None:
            ini = i
        elif not c and ini is not None:
            ys = [v for cc in cols[ini:i] for v in cc]
            tramos.append((x + ini, min(ys), i - ini, max(ys) - min(ys) + 1))
            ini = None
    if not tramos:
        return '(nada)'
    alto = max(t[3] for t in tramos)

    def cifra(t):
        tx, ty, w, h = t
        m = max(1, w // 10)
        pts = [(w // 2, m), (w - 1 - m, h // 4), (w - 1 - m, 3 * h // 4), (w // 2, h - 1 - m),
               (m, 3 * h // 4), (m, h // 4), (w // 2, h // 2)]
        bits = sum(1 << i for i, (dx, dy) in enumerate(pts) if vc.es(cap.px(tx + dx, ty + dy), enc))
        return SEG.get(bits, '?')
    grandes = [t for t in tramos if t[3] >= alto * 9 // 10]
    hhmm = ''.join(cifra(t) for t in grandes)
    puntos, ss = '', ''
    if len(grandes) == 4:
        entre = [t for t in tramos if grandes[1][0] < t[0] < grandes[2][0]]
        if entre:
            tx, ty, w, h = entre[0]
            lit = any(vc.es(cap.px(tx + w // 2, yy), enc) for yy in range(ty, ty + h))
            puntos = ':' if lit else '.'
        ss = ''.join(cifra(t) for t in tramos if t[0] > grandes[3][0])
    return '%s|%s|%s' % (hhmm, puntos, ss)


def texto_g(cap, x0, y0, x1, y1, esc, fg, bg, texto, negrita=False):
    F = vc.fuente()
    pts = []
    for i, ch in enumerate(texto.encode('cp437')):
        for fila in range(16):
            bits = F[ch][fila] << 1
            if negrita:
                bits |= F[ch][fila]
            for col in range(8):
                pts.append(((i * 8 + col) * esc + esc // 2, fila * esc + esc // 2, bool(bits & (0x100 >> col))))
    pts.sort(key=lambda p: not p[2])
    for yy in range(y0, y1 + 1):
        for xx in range(x0, x1 + 1):
            if all(vc.es(cap.px(xx + dx, yy + dy), fg if f else bg) for dx, dy, f in pts):
                return (xx, yy)
    return None


def main():
    cap = vc.Captura(sys.argv[1])
    nombre = os.path.basename(sys.argv[1]).replace('.ppm', '')
    a = sys.argv[2:]
    i = 0
    while i < len(a):
        op = a[i]
        if op == 'redonda':
            cx, cy, r, col = (int(v) for v in a[i + 1:i + 5])
            mal = 0
            for g in range(360):
                s, c = math.sin(math.radians(g)), math.cos(math.radians(g))
                dentro = cap.px(round(cx + (r - 0.5) * s), round(cy - (r - 0.5) * c))
                fuera = cap.px(round(cx + (r + 1.5) * s), round(cy - (r + 1.5) * c))
                if not vc.es(dentro, col) or vc.es(fuera, col):
                    mal += 1
            vc.res(mal == 0, '%s: %s%s' % (nombre, a[i + 5], '' if mal == 0 else ' (%d de 360 grados mal)' % mal))
            i += 6
        elif op == 'marcas':
            cx, cy, r1, r2, cara, n = (int(v) for v in a[i + 1:i + 7])
            pasos = 2880
            hay = []
            for k in range(pasos):
                g = 2 * math.pi * (k + 0.5) / pasos
                hay.append(any(not vc.es(cap.px(round(cx + r * math.sin(g)), round(cy - r * math.cos(g))), cara)
                               for r in range(r1, r2 + 1)))
            tramos = sum(1 for k in range(pasos) if hay[k] and not hay[k - 1])
            vc.res(tramos == n, '%s: %d marcas en la esfera%s' % (nombre, n, '' if tramos == n else ' (salen %d)' % tramos))
            i += 7
        elif op == 'cifras':
            x, y, an, al, enc, apag = (int(v) for v in a[i + 1:i + 7])
            lee = leer_cifras(cap, x, y, an, al, enc, apag)
            vc.res(lee == a[i + 7], '%s: las cifras dicen «%s»%s' % (nombre, a[i + 7], '' if lee == a[i + 7] else ' (se lee «%s»)' % lee))
            i += 8
        elif op in ('texto_g', 'texto_gn'):
            x0, y0, x1, y1, esc, fg, bg = (int(v) for v in a[i + 1:i + 8])
            donde = texto_g(cap, x0, y0, x1, y1, esc, fg, bg, a[i + 8], op == 'texto_gn')
            vc.res(donde is not None, '%s: «%s» a tamaño %d%s' % (nombre, a[i + 8], esc, ' (en %d,%d)' % donde if donde else
                                                             ' (no está entre %d,%d y %d,%d)' % (x0, y0, x1, y1)))
            i += 9
        elif op == 'cuenta':
            x, y, an, al, col, mn, mx = (int(v) for v in a[i + 1:i + 8])
            n = sum(1 for yy in range(y, y + al) for xx in range(x, x + an) if vc.es(cap.px(xx, yy), col))
            vc.res(mn <= n <= mx, '%s: %s (%d puntos)' % (nombre, a[i + 8], n))
            i += 9
        else:
            raise SystemExit('comprobación desconocida: ' + op)
    sys.exit(1 if vc.fallos else 0)


main()
EOF
export HERRAMIENTAS="$PWD/tests/tools"

cp "$IMG" $T/v.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/v.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/v.img $T/FIJAHORA.COM ::FIJAHORA.COM
rm -f $T/mon
{ sleep 6
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'FIJAHORA' 1.5
  send 'HORA' 1
  send 'FECHA' 1
  send '' 1
  send 'VENTANAS' 5
  doble 206 129; sleep 1                    # el grupo «Accesorios»
  tecla right right ret; sleep 1.5          # su tercer icono: el Reloj
  aparca
  # ---- esfera con agujas, en la ventana de siempre (200 x 252) ----
  shot e1                                   # lunes 16-11-2026, 10:10:30
  luz scroll_lock
  shot e2                                   # miércoles 30-09-2026, 12:00:00
  luz num_lock
  shot e3                                   # jueves 31-12-2026, 23:59:59
  luz scroll_lock                           # sábado 3-01-2026, 3:45:15
  # ---- maximizada ----
  clic 728 343; sleep 1; aparca
  shot g1
  luz caps_lock
  shot g2                                   # domingo 1-03-2026, 15:15:00
  luz scroll_lock
  shot g3                                   # viernes 15-05-2026, 9:45:00
  # ---- opciones: solo las cifras ----
  tecla alt-r ret; sleep 1                  # Reloj > Opciones...
  shot o1
  clic 297 313                              # opción: solo las cifras
  aparca
  tecla ret; sleep 1.2
  shot c1                                   # 09:45 y 00, maximizada
  luz num_lock
  shot c2                                   # martes 10-02-2026, 0:07:08
  clic 767 41; sleep 1; aparca              # restaurar: la ventana de siempre
  shot c3
  tecla alt-r ret; sleep 1
  clic 297 259                              # sin segundos
  clic 297 339                              # formato de 12 horas
  aparca
  shot o2
  tecla ret; sleep 1.2
  shot c4                                   # 12:07 AM
  luz scroll_lock
  shot c5                                   # lunes 6-07-2026, 18:30:45 -> 6:30 PM
  # ---- los temas ----
  tecla alt-t; sleep 0.6
  shot t0                                   # el menú, con la marca en «Rescoldo»
  tecla down ret; sleep 1                   # Claro
  shot t1
  tecla alt-t; sleep 0.6
  shot t1m                                  # el menú: la marca, en «Claro»
  tecla down down ret; sleep 1              # Noche
  shot t2
  tecla alt-r ret; sleep 1
  clic 297 289; aparca                      # otra vez la esfera (sin segundero)
  tecla ret; sleep 1.2
  shot t3                                   # esfera de noche, 18:30:45
  clic 747 343; sleep 0.8                   # cerrar el Reloj
  shot z1
  clic 756 13; sleep 1; tecla ret; sleep 2.5    # salir de Hispa Ventanas
  send 'ECHO SIGUE-VIVO'
  mon quit
} | timeout 200 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 16 -drive file=$T/v.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw2 2>&1
tr -d '\r' < $T/raw2 | iconv -f CP437 -t UTF-8 > $T/log2

# -- el residente de prueba manda en la hora y en la fecha --
if grep -q "La hora actual es 10:10:30" $T/log2 && grep -q "La fecha actual es Lun 16-11-2026" $T/log2; then
  ok "hora y fecha fijadas para la prueba (Lun 16-11-2026, 10:10:30)"
else
  ko "hora y fecha fijadas para la prueba"
fi
check2() { if grep -qF -- "$1" $T/log2; then ok "$1"; else ko "$1"; fi; }
# La ventana de siempre está en (560,330) y su zona empieza en (562,375), de
# 196 x 205; la esfera tiene el centro en (660,464) y 81 de radio. Maximizada,
# la zona empieza en (2,73) y es de 796 x 525; centro (400,314), radio 233.
# -- esfera pequeña: 10:10:30 --
chk $T/e1.ppm tam 800 600 marco 560 330 200 252 activa titulo 560 330 activa "Reloj" \
  px 660 383 1 "el aro de la esfera, arriba" px 690 490 8 "la cara de la esfera" \
  px 696 446 0 "10:10:30, el minutero pasadas las y diez" \
  px 640 450 0 "la aguja de las horas, pasadas las 10" \
  px 660 494 4 "el segundero (terracota) en el 30" \
  texto 652 407 1 8 negrita "12" texto 592 557 1 8 fina "lunes 16 nov 2026"
rchk $T/e1.ppm redonda 660 464 81 1 "la esfera es redonda (radio 81)" marcas 660 464 69 70 8 60
# -- 12:00:00 y 23:59:59: el temporizador repinta solo --
chk $T/e2.ppm cambia 562 375 196 205 $T/e1.ppm 20 \
  px 660 414 4 "12:00:00, el segundero sobre las dos agujas" px 662 440 0 "las agujas, en las 12" \
  texto 576 557 1 8 fina "miércoles 30 sep 2026"
chk $T/e3.ppm px 662 440 0 "23:59:59, la aguja de las horas casi en las 12" \
  px 646 440 8 "y no en las 11" texto 588 557 1 8 fina "jueves 31 dic 2026"
# -- esfera maximizada: 3:45:15, 15:15:00 y 9:45:00 --
chk $T/g1.ppm px 400 82 1 "el aro de la esfera grande" px 280 311 0 "3:45:15, el minutero en las :45" \
  px 225 309 0 "el minutero ha avanzado con los segundos (un grado y medio)" \
  px 550 314 4 "el segundero en el 15" px 465 341 0 "la aguja de las horas, entre las 3 y las 4"
rchk $T/g1.ppm redonda 400 314 233 1 "la esfera grande es redonda (radio 233)" marcas 400 314 203 205 8 60 \
  texto_gn 381 136 387 142 2 1 8 "12" texto_gn 309 435 315 441 2 1 8 "7" \
  texto_g 188 555 196 563 2 1 8 "sábado, 3 de enero de 2026"
chk $T/g2.ppm px 520 314 0 "15:15:00, el minutero a la derecha (las :15)" px 280 314 8 "y nada a la izquierda" \
  px 469 323 0 "la aguja de las horas, pasadas las 3"
chk $T/g3.ppm px 280 314 0 "9:45:00, el minutero a la izquierda (las :45)" px 520 314 8 "y nada a la derecha" \
  px 335 287 0 "la aguja de las horas, entre las 9 y las 10"
# -- opciones --
chk $T/o1.ppm titulo 268 210 activa "Opciones del reloj" px 297 259 4 "casilla del segundero marcada" \
  texto 312 251 0 8 fina "Aguja de los segundos" px 297 289 4 "opción «Esfera con agujas» elegida" \
  px 297 313 9 "opción «Solo las cifras» sin elegir" px 297 339 9 "casilla de 12 horas sin marcar" \
  texto 312 331 0 8 fina "Formato de 12 horas"
chk $T/o2.ppm px 297 259 9 "casilla del segundero sin marcar" px 297 289 9 "opción «Esfera con agujas» sin elegir" \
  px 297 313 4 "opción «Solo las cifras» elegida" px 297 339 4 "casilla de 12 horas marcada"
# -- solo las cifras, maximizada: 09:45 00 y 00:07 08 --
chk $T/c1.ppm px 6 78 8 "fondo crema alrededor del panel" px 40 110 0 "el panel, de tinta"
rchk $T/c1.ppm cifras 16 87 768 497 10 1 "0945|:|00" texto_g 70 380 90 520 3 6 0 "viernes, 15 de mayo de 2026"
rchk $T/c2.ppm cifras 16 87 768 497 10 1 "0007|:|08" texto_g 40 380 70 520 3 6 0 "martes, 10 de febrero de 2026"
# -- y en la ventana de siempre --
chk $T/c3.ppm marco 560 330 200 252 activa px 660 383 8 "ya no hay esfera: fondo crema" px 590 400 0 "el panel" \
  busca 572 385 660 570 6 0 "martes 10 feb 2026"
rchk $T/c3.ppm cifras 576 389 168 177 10 1 "0007|:|08"
# -- 12 horas, sin segundos --
rchk $T/c4.ppm cifras 576 389 168 177 10 1 "1207|:|" cuenta 576 389 168 177 5 20 400 "«AM», del color del acento"
rchk $T/c5.ppm cifras 576 389 168 177 10 1 "_630|:|"
# -- temas --
# (el menú «Tema»: la primera orden sale elegida, en blanco sobre añil; la
# letra de cada orden que va subrayada no se mira)
chk $T/t0.ppm texto 641 379 9 12 fina "»" texto 665 379 9 12 fina "escoldo" \
  texto 665 399 0 8 fina "laro" texto 665 419 0 8 fina "oche" nobusca 636 395 650 425 0 8 "»"
chk $T/t1.ppm px 590 400 15 "tema Claro: el panel, de piedra"
rchk $T/t1.ppm cifras 576 389 168 177 0 6 "_630|:|"
chk $T/t1m.ppm texto 641 399 0 8 fina "»" texto 665 379 9 12 fina "escoldo" nobusca 636 375 650 385 9 12 "»"
chk $T/t2.ppm px 566 380 0 "tema Noche: el fondo, de tinta"
rchk $T/t2.ppm cifras 576 389 168 177 5 1 "_630|:|"
chk $T/t3.ppm px 660 383 7 "tema Noche: el aro" px 657 504 8 "18:30:45, el minutero (crema) en la media" \
  px 653 488 8 "la aguja de las horas, pasadas las 6" px 630 464 1 "sin segundero: en el 45 no hay aguja"
rchk $T/t3.ppm marcas 660 464 69 70 1 60
chk $T/z1.ppm px 707 471 2 "fondo donde estaba el Reloj"
check2 "SIGUE-VIVO"
if grep -qF -- "HSPDPMI: excepción" $T/log2; then ko "(no debía salir) HSPDPMI: excepción"; else ok "sin \"HSPDPMI: excepción\""; fi
muestra e1 esfera_pequena; muestra g1 esfera_grande; muestra c3 cifras_pequenas; muestra c2 cifras_grandes
muestra o2 opciones; muestra c5 cifras_12h; muestra t0 menu_tema; muestra t1 tema_claro; muestra t2 tema_noche
muestra t3 esfera_noche; muestra e3 esfera_2359; muestra g3 esfera_grande_0945
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/FIJAHORA/,$p' $T/log2; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
[ $fail = 0 ] && echo "TODO OK" || echo "HAY FALLOS"
exit $fail
