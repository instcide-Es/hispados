; =============================================================================
;  VDPTEST.SMS - la ROM de prueba propia de Hispa-System
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Prueba lo que las de libre distribución del procesador (zexdoc, zexall)
;  no miran: la imagen, las interrupciones, el mapeador, la RAM del
;  cartucho y los puertos de los mandos. Cada prueba que falla deja su
;  nombre en el texto del resultado, que va a la RAM del cartucho (banco 0):
;    8000h: cuántas han fallado; 8001h-8003h: DEh B0h 61h (hay resultado);
;    8004h: el texto, una línea por fallo y al final «VDPTEST BIEN» o
;    «VDPTEST MAL», terminado en 0.
;  Luego deja una pantalla del modo 4 que usa casi todo (dibujos girados,
;  las dos paletas, prioridad, desplazamiento con las filas y columnas
;  fijas, la columna tapada, objetos de 8x16), con el fondo verde si todo ha
;  ido bien y rojo si no. 64 KB: cuatro bancos, cada uno con su número.
;  Se ensambla con tools/asmz80.py.
; =============================================================================
VDATOS      = $BE
VCTRL       = $BF
VCONT       = $7E
MANDO1      = $DC
MANDO2      = $DD
PUERTO_ES   = $3F
NOMBRES     = $3800
OBJETOS     = $3F00
; ---- variables (RAM) ----
fallos      = $C000
txt_p       = $C002
cnt_cuadro  = $C004
cnt_linea   = $C005
v_primera   = $C006
visto       = $C007
bandera     = $C008
modo_int    = $C009
texto       = $C100
LETRAS      = $00                   ; (las letras van en el dibujo de su código)

        .org $0000
        di
        im 1
        ld sp,$DFF0
        jp inicio
        .pad $0038
        jp irq
        .pad $0066
        retn

; ---- el banco de 0000h-3FFFh cambia mientras se corre aquí: en el primer
;      KB, que es siempre el del banco 0 ----
banco0:
        ld a,1
        ld ($FFFD),a
        ld a,($0400)                ; el banco 1 desde 0400h
        ld b,a
        ld a,($0000)                ; y el primer KB, el de siempre
        ld c,a
        xor a
        ld ($FFFD),a
        ld a,b
        cp $11
        ret nz
        ld a,c
        cp $F3                      ; (di)
        ret

irq:
        push af
        push hl
        in a,(VCTRL)
        ld l,a
        ld a,(modo_int)
        cp 1
        jr z,@cuenta
        cp 2
        jr z,@tras_ei
        jr @fin
@cuenta:
        bit 7,l
        jr z,@lin
        ld hl,cnt_cuadro
        inc (hl)
        jr @fin
@lin:
        ld a,(cnt_linea)
        or a
        jr nz,@l2
        in a,(VCONT)
        ld (v_primera),a
@l2:
        ld hl,cnt_linea
        inc (hl)
        jr @fin
@tras_ei:
        ld a,(bandera)
        ld (visto),a
@fin:
        pop hl
        pop af
        ei
        reti

; ---- los registros del VDP ----
; B=registro C=valor
vdp_reg:
        ld a,c
        out (VCTRL),a
        ld a,b
        or $80
        out (VCTRL),a
        ret
; HL=dirección para escribir en la VRAM
vdp_esc:
        ld a,l
        out (VCTRL),a
        ld a,h
        or $40
        out (VCTRL),a
        ret
; HL=dirección para leer
vdp_lee:
        ld a,l
        out (VCTRL),a
        ld a,h
        and $3F
        out (VCTRL),a
        ret
; A=color de la paleta (0-31) para escribir
vdp_pal:
        out (VCTRL),a
        ld a,$C0
        out (VCTRL),a
        ret
; espera a la bandera de cuadro -> A = el estado
espera_cuadro:
        in a,(VCTRL)
        bit 7,a
        jr z,espera_cuadro
        ret
; espera a la línea A del contador de líneas
espera_linea:
        ld b,a
@e:
        in a,(VCONT)
        cp b
        jr nz,@e
        ret
; HL=texto (terminado en 0): lo añade al resultado
pon_texto:
        ld de,(txt_p)
@c:
        ld a,(hl)
        ld (de),a
        or a
        jr z,@x
        inc hl
        inc de
        jr @c
@x:
        ld (txt_p),de
        ret
; Z=1: la prueba ha ido bien; si no, HL=su nombre
resultado:
        ret z
        ld a,(fallos)
        inc a
        ld (fallos),a
        jp pon_texto

; los registros del principio: modo 4, imagen apagada
regs_ini:
        .byte $04, $80, $FF, $FF, $FF, $FF, $FB, $00, $00, $00, $FF
regs_fin:

inicio:
        ld hl,$C000                 ; la RAM a cero
        ld de,$C001
        ld bc,$1FF0
        ld (hl),0
        ldir
        ld hl,texto
        ld (txt_p),hl
        ld hl,regs_ini
        ld b,0
@r:
        ld c,(hl)
        push hl
        push bc
        call vdp_reg
        pop bc
        pop hl
        inc hl
        inc b
        ld a,b
        cp regs_fin - regs_ini
        jr nz,@r
        call t_vram
        call t_espejo
        call t_bancos
        call t_ramcart
        call t_mandos
        call t_cuadro
        call t_lineas
        call t_ei
        call t_sobran
        call t_choque
        ; ---- el resultado ----
        ld a,(fallos)
        or a
        ld hl,t_bien
        jr z,@b
        ld hl,t_mal
@b:
        call pon_texto
        ld a,$08                    ; a la RAM del cartucho
        ld ($FFFC),a
        ld a,(fallos)
        ld ($8000),a
        ld a,$DE
        ld ($8001),a
        ld a,$B0
        ld ($8002),a
        ld a,$61
        ld ($8003),a
        ld hl,texto
        ld de,$8004
        ld bc,$200
        ldir
        xor a
        ld ($FFFC),a
        call pantalla
@fin:
        di
        halt
        jr @fin

t_bien:     .byte "VDPTEST BIEN", 0
t_mal:      .byte "VDPTEST MAL", 0

; ---- 1. la VRAM: 256 octetos y leerlos (con el búfer de lectura) ----
t_vram:
        ld hl,$1000
        call vdp_esc
        ld b,0
@w:
        ld a,b
        xor $5A
        out (VDATOS),a
        inc b
        jr nz,@w
        ld hl,$1000
        call vdp_lee
        ld b,0
        ld c,0                      ; C: los que no salen
@r:
        in a,(VDATOS)
        xor $5A
        cp b
        jr z,@r1
        inc c
@r1:
        inc b
        jr nz,@r
        ld a,c
        or a
        ld hl,@n
        jp resultado
@n:     .byte "VRAM\n", 0

; ---- 2. la RAM, dos veces (C000h y E000h) ----
t_espejo:
        ld a,$77
        ld ($C010),a
        ld a,($E010)
        cp $77
        ld hl,@n
        jp resultado
@n:     .byte "ESPEJO DE LA RAM\n", 0

; ---- 3. el mapeador: bancos 1, 2 y 3; el primer KB fijo; DFFFh ----
t_bancos:
        ld a,3
        ld ($FFFF),a
        ld a,($8000)
        cp 3
        jr nz,@mal
        ld a,($DFFF)                ; los registros son también RAM
        cp 3
        jr nz,@mal
        ld a,2
        ld ($FFFE),a
        ld a,($4000)
        cp 2
        jr nz,@mal
        ld a,1
        ld ($FFFE),a
        ld a,($4000)
        cp 1
        jr nz,@mal
        ld a,2
        ld ($FFFF),a
        ld a,($8000)
        cp 2
        jr nz,@mal
        call banco0
@mal:
        ld hl,@n
        jp resultado
@n:     .byte "MAPEADOR\n", 0

; ---- 4. la RAM del cartucho: dos bancos ----
t_ramcart:
        ld a,$08
        ld ($FFFC),a
        ld a,$5A
        ld ($8000),a
        ld a,$0C
        ld ($FFFC),a
        ld a,$A5
        ld ($8000),a
        ld a,$08
        ld ($FFFC),a
        ld a,($8000)
        cp $5A
        jr nz,@mal
        ld a,$0C
        ld ($FFFC),a
        ld a,($8000)
        cp $A5
        jr nz,@mal
        xor a                       ; apagada: otra vez la ROM
        ld ($FFFC),a
        ld a,($8000)
        cp 2
@mal:
        push af
        xor a
        ld ($FFFC),a
        pop af
        ld hl,@n
        jp resultado
@n:     .byte "RAM DEL CARTUCHO\n", 0

; ---- 5. los mandos: nada pulsado; TH como salida (consola de fuera de Japón) ----
t_mandos:
        in a,(MANDO1)
        cp $FF
        jr nz,@mal
        ld a,$F5                    ; TH de los dos, salidas a 1
        out (PUERTO_ES),a
        in a,(MANDO2)
        and $C0
        cp $C0
        jr nz,@mal
        ld a,$55                    ; y a 0
        out (PUERTO_ES),a
        in a,(MANDO2)
        and $C0
@mal:
        push af
        ld a,$FF
        out (PUERTO_ES),a
        pop af
        ld hl,@n
        jp resultado
@n:     .byte "MANDOS\n", 0

; ---- 6. la bandera de cuadro: en la línea C1h; leerla la borra ----
t_cuadro:
        in a,(VCTRL)                ; (la de antes, fuera)
        call espera_cuadro
        in a,(VCONT)
        ld b,a
        in a,(VCTRL)
        bit 7,a                     ; (ya borrada)
        jr nz,@mal
        ld a,b
        cp $C1
        jr c,@mal
        cp $C4
        ccf
        jr c,@mal
        xor a
@mal:
        ld hl,@n
        jp resultado
@n:     .byte "BANDERA DE CUADRO\n", 0

; ---- 7. interrupciones de línea (cada 50) y de cuadro, con IM 1 y HALT ----
t_lineas:
        ld bc,$0A31                 ; cada 49+1 líneas
        call vdp_reg
        ld bc,$0014                 ; modo 4 y la interrupción de línea
        call vdp_reg
        ld bc,$01A0                 ; y la de cuadro
        call vdp_reg
        call espera_cuadro
        xor a
        ld (cnt_cuadro),a
        ld (cnt_linea),a
        ld (v_primera),a
        ld a,1
        ld (modo_int),a
        in a,(VCTRL)
        ei
@h:
        halt
        ld a,(cnt_cuadro)
        or a
        jr z,@h
        di
        xor a
        ld (modo_int),a
        ld bc,$0004
        call vdp_reg
        ld bc,$0180
        call vdp_reg
        ld a,(cnt_linea)
        cp 3
        jr nz,@mal
        ld a,(v_primera)
        cp 49
        jr z,@mal
        cp 50
@mal:
        ld hl,@n
        jp resultado
@n:     .byte "INTERRUPCION DE LINEA\n", 0

; ---- 8. tras EI va una instrucción más antes de la interrupción ----
t_ei:
        ld a,2
        ld (modo_int),a
        call espera_cuadro
        ld bc,$01A0
        call vdp_reg
        ld a,$10                    ; al cuadro siguiente, tras la bandera
        call espera_linea
        ld a,$C8
        call espera_linea
        xor a
        ld (bandera),a
        ld (visto),a
        ld hl,bandera
        ei
        ld (hl),1                   ; esta va antes de la interrupción
        nop
        nop
        di
        ld bc,$0180
        call vdp_reg
        xor a
        ld (modo_int),a
        ld a,(visto)
        cp 1
        ld hl,@n
        jp resultado
@n:     .byte "RETRASO DE EI\n", 0

; ---- objetos: N en la tabla, todos en la misma línea y separados C en x ----
; B=cuántos C=separación
pon_objetos:
        push bc
        ld hl,OBJETOS
        call vdp_esc
        pop bc
        push bc
@y:
        ld a,$40
        out (VDATOS),a
        djnz @y
        ld a,$D0
        out (VDATOS),a
        ld hl,OBJETOS + $80
        call vdp_esc
        pop bc
        xor a
@x:
        out (VDATOS),a              ; x
        push af
        ld a,1                      ; el dibujo 1 (lleno)
        out (VDATOS),a
        pop af
        add a,c
        djnz @x
        ret
; un cuadro entero con la imagen encendida -> A = el estado (leído una
; sola vez al final: cada lectura borra las banderas)
dos_cuadros:
        ld bc,$01C0
        call vdp_reg
        ld a,$C8
        call espera_linea
        in a,(VCTRL)
        ld a,$10
        call espera_linea
        ld a,$C8
        call espera_linea
        in a,(VCTRL)
        push af
        ld bc,$0180
        call vdp_reg
        pop af
        ret
dibujo_lleno:
        ld hl,$0020                 ; el dibujo 1, de color 1
        call vdp_esc
        ld b,8
@d:
        ld a,$FF
        out (VDATOS),a
        xor a
        out (VDATOS),a
        out (VDATOS),a
        out (VDATOS),a
        djnz @d
        ret

; ---- 9. más de ocho objetos en una línea ----
t_sobran:
        call dibujo_lleno
        ld bc,$0910
        call pon_objetos
        call dos_cuadros
        bit 6,a
        jr z,@mal
        ld bc,$0810                 ; con ocho, no
        call pon_objetos
        call dos_cuadros
        bit 6,a
        jr nz,@mal
        xor a
@mal:
        ld hl,@n
        jp resultado
@n:     .byte "MAS DE OCHO OBJETOS\n", 0

; ---- 10. el choque de dos objetos ----
t_choque:
        ld bc,$0204                 ; dos, montados
        call pon_objetos
        call dos_cuadros
        bit 5,a
        jr z,@mal
        ld bc,$0240                 ; separados
        call pon_objetos
        call dos_cuadros
        bit 5,a
        jr nz,@mal
        xor a
@mal:
        ld hl,@n
        jp resultado
@n:     .byte "CHOQUE DE OBJETOS\n", 0

; =============================================================================
;  La pantalla del final
; =============================================================================
paleta:
        .byte $08, $3F, $0F, $30, $03, $0C, $15, $2A    ; 0 (verde: cambia) blanco amarillo azul rojo verde gris gris claro
        .byte $00, $00, $00, $00, $00, $00, $00, $00
        .byte $00, $33, $3C, $0B, $3F, $00, $00, $00    ; los objetos: magenta, celeste, naranja, blanco
        .byte $00, $00, $00, $00, $00, $00, $00, $10    ; (y el 31, el borde: azul oscuro)

pantalla:
        ld bc,$0180
        call vdp_reg
        ld a,0                      ; la paleta
        call vdp_pal
        ld hl,paleta
        ld b,32
@p:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @p
        ld a,(fallos)               ; el fondo: verde o rojo
        or a
        jr z,@v
        xor a
        call vdp_pal
        ld a,$03
        out (VDATOS),a
@v:
        ld hl,$0000                 ; los dibujos
        call vdp_esc
        ld hl,dibujos
        ld bc,dibujos_fin - dibujos
@d:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,@d
        ld hl,$0400                 ; las letras, en el dibujo de su código
        call vdp_esc
        ld hl,letras
        ld bc,letras_fin - letras
@l:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,@l
        ld hl,NOMBRES               ; la tabla de nombres: 28 filas de 32
        call vdp_esc
        ld hl,mapa
        ld c,28
@f:
        ld b,32
@c:
        ld a,(hl)
        cp $FF                      ; FFh: la última, hasta el final de la fila
        jr z,@rep
        ld d,a
        inc hl
        ld e,(hl)
        inc hl
        ld a,d
        out (VDATOS),a
        ld a,e
        out (VDATOS),a
        djnz @c
        ld a,(hl)                   ; (una fila entera: su FFh, si lo lleva)
        cp $FF
        jr nz,@sig
        inc hl
        jr @sig
@rep:
        ld a,d
        out (VDATOS),a
        ld a,e
        out (VDATOS),a
        djnz @rep
        inc hl
@sig:
        dec c
        jr nz,@f
        ld hl,OBJETOS               ; los objetos
        call vdp_esc
        ld hl,obj_y
        ld b,obj_n + 1
@oy:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @oy
        ld hl,OBJETOS + $80
        call vdp_esc
        ld hl,obj_xn
        ld b,obj_n * 2
@ox:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @ox
        ld bc,$0804                 ; desplazamientos: 4 a la derecha, 8 arriba
        call vdp_reg
        ld bc,$0908
        call vdp_reg
        ld bc,$07F0                 ; el borde: el color 31 (16 + 15)
        call vdp_reg
        ld bc,$00E4                 ; columnas y filas fijas y la columna tapada
        call vdp_reg
        ld bc,$01C2                 ; imagen, objetos de 8x16
        call vdp_reg
        ret

; los objetos de la pantalla
obj_y:      .byte 47, 47, 95, 95, 127, $D0
obj_n       = 5
obj_xn:     .byte 64, 4,  40, 6,  40, 4,  120, 4,  200, 6

; el mapa: por casilla, dibujo y atributos (bit 1 girado izquierda-derecha,
; 2 arriba-abajo, 3 la paleta de los objetos, 4 delante); FFh repite la
; última casilla hasta el final de la fila. Fila a fila:
mapa:
        .byte 'V',0, 'D',0, 'P',0, 'T',0, 'E',0, 'S',0, 'T',0, ' ',0, 'H',0, 'I',0, 'S',0, 'P',0, 'A',0
        .byte '-',0, 'S',0, 'Y',0, 'S',0, 'T',0, 'E',0, 'M',0, ' ',0, $FF
        .byte '0',0, '1',0, '2',0, '3',0, '4',0, '5',0, '6',0, '7',0, '8',0, '9',0, ' ',0, $FF
        .byte 2,0, $FF                          ; 2: cuadros
        .byte 3,0, 3,2, 3,4, 3,6, 3,8, 3,10, 3,12, 3,14, 2,0, $FF   ; la flecha en todos los giros
        .byte 2,0, $FF
        .byte 4,0, 4,8, $FF                     ; 4: un marco, con las dos paletas
        .byte 2,0, $FF
        .byte 1,0, 1,0, 1,0, 1,0, 1,16, 1,16, 1,16, 1,16, 1,0, 1,0, 1,0, 1,0, 1,16, 1,16, 1,16, 1,16, 4,0, $FF
        .byte 2,0, $FF                          ; (encima, los objetos 0 y 1: el 1 queda detrás)
        .byte 5,0, $FF                          ; 5: rayas (para ver el desplazamiento)
        .byte 5,0, $FF
        .byte 6,0, $FF
        .byte 6,0, $FF
        .byte 5,0, $FF
        .byte 2,0, $FF
        .byte 'A',0, 'B',0, 'C',0, 'D',0, 'E',0, 'F',0, 'G',0, 'H',0, 'I',0, 'J',0, 'K',0, 'L',0, 'M',0
        .byte 'N',0, 'O',0, 'P',0, 'Q',0, 'R',0, 'S',0, 'T',0, 'U',0, 'V',0, 'W',0, 'X',0, 'Y',0, 'Z',0, ' ',0, $FF
        .byte 2,0, $FF
        .byte 6,0, $FF
        .byte 7,0, $FF
        .byte 6,0, $FF
        .byte 5,0, $FF
        .byte 2,0, $FF
        .byte 'F',0, 'I',0, 'N',0, ' ',0, $FF
        .byte 1,0, $FF
        .byte 1,8, $FF
        .byte 2,0, $FF
        .byte 7,6, $FF
        .byte 2,0, $FF

dibujos:
.tiles
; 0: vacío
........
........
........
........
........
........
........
........

; 1: lleno (color 1)
11111111
11111111
11111111
11111111
11111111
11111111
11111111
11111111

; 2: cuadros
66667777
66667777
66667777
66667777
77776666
77776666
77776666
77776666

; 3: una flecha que no es simétrica
4.......
44......
444.....
4444....
44444...
444.....
44......
4......2

; 4: un marco de colores
12345671
2......2
3......3
4..55..4
5..55..5
6......6
7......7
17654321

; 5: rayas
22222222
........
33333333
........
44444444
........
55555555
........

; 6: columnas
2.3.4.5.
2.3.4.5.
2.3.4.5.
2.3.4.5.
2.3.4.5.
2.3.4.5.
2.3.4.5.
2.3.4.5.

; 7: diagonal
1.......
.1......
..1.....
...1....
....1...
.....1..
......1.
.......1
.endtiles
dibujos_fin:
letras:
        .include "../../sms/comun/letras.s"
letras_fin:

        .pad $4000, 0
; ---- los bancos 1, 2 y 3: su número al principio (y el 1, $11 en 0400h) ----
        .byte 1
        .pad $4400, 0
        .byte $11
        .pad $7FF0, 0
        .cabecera_sega 0, 0, $4C
        .byte 2
        .pad $C000, 0
        .byte 3
        .pad $10000, 0
