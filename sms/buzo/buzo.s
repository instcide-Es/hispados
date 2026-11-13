; =============================================================================
;  Buzo (BUZO.SMS) - el primer juego propio de Hispa-System
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un juego pequeño, de diseño propio (dibujos, música y reglas), escrito en
;  ensamblador de Z80 para el hardware de la consola: funciona en
;  Hispa-System, en otros emuladores y en una consola de verdad con un
;  cartucho de 32 KB con RAM y pila (mapeador de Sega). Se ensambla con
;  tools/asmz80.py.
;
;  El buzo nada por el fondo del mar, que corre hacia la izquierda, y le
;  salen al paso peces (que se le acercan) y medusas (que suben y bajan).
;  Cruceta: nadar. Botón 1 o 2: arpón (dos a la vez). Pausa: parar y seguir
;  (en el título, empezar). Pez: 10 puntos; medusa: 20; cada 100 puntos
;  salen más deprisa. Un choque quita una vida (hay tres) y el buzo
;  parpadea un rato sin que le pase nada. La mejor puntuación se guarda en
;  la RAM con pila del cartucho (en Hispa-System, BUZO.SAV).
;
;  Usa lo que hay que probar en el emulador:
;  - el desplazamiento horizontal, con las dos filas de arriba fijas (el
;    marcador) y, por la interrupción de LÍNEA, a otra velocidad desde la
;    línea 128 (el fondo del mar corre el doble que el agua: paralaje);
;  - la interrupción de cuadro (y HALT para esperarla) e IM 1;
;  - objetos de 8x16, dos por cada bicho de 16 de ancho;
;  - la NMI del botón de pausa;
;  - el sonido: la música en dos canales de tono, el arpón en el tercero y
;    los golpes en el de ruido;
;  - la RAM del cartucho, con pila.
;  Los números aleatorios solo avanzan al sacar un bicho: con los mismos
;  botones en los mismos cuadros, la partida es siempre igual.
; =============================================================================

; ---- los puertos ----
VDATOS      = $BE
VCTRL       = $BF
VCONT       = $7E
PSG         = $7F
MANDO1      = $DC

; ---- la memoria de imagen ----
NOMBRES     = $3800
OBJETOS     = $3F00
LINEA_FONDO = 127                   ; la interrupción de línea: en la 127

; ---- los botones (como los deja lee_mando: 1 = pulsado) ----
B_ARRIBA    = $01
B_ABAJO     = $02
B_IZQ       = $04
B_DER       = $08
B_1         = $10
B_2         = $20

; ---- estados ----
E_TITULO    = 0
E_JUEGO     = 1
E_FIN       = 2

; ---- bichos ----
T_LIBRE     = 0
T_PEZ       = 1
T_MEDUSA    = 2
T_NUBE      = 3
N_BICHOS    = 6
BICHO       = 8                     ; octetos de cada uno
N_ARPONES   = 2

; ---- la RAM ----
estado      = $C000
vblank      = $C001
cuadro      = $C002                 ; (palabra)
pad         = $C004
pad_ant     = $C005
pulsado     = $C006
nmi_pide    = $C007
pausa       = $C008
sc_agua     = $C009
sc_fondo    = $C00A
jx          = $C00B
jy          = $C00C
invul       = $C00D
vidas       = $C00E
rnd         = $C010                 ; (palabra)
spawn_t     = $C012
dific       = $C013
puntos      = $C014                 ; tres octetos en BCD, el alto primero
mejor       = $C017
hud_sucio   = $C01A
tiempo      = $C01B
sat_n       = $C01C
cool        = $C01D
record      = $C01E
mus_p       = $C020                 ; la melodía: por dónde va, cuánto le queda
mus_d       = $C022
baj_p       = $C023                 ; el bajo
baj_d       = $C025
sfx_t       = $C026                 ; el arpón (canal 2)
ru_t        = $C027                 ; el ruido (canal 3)
ru_largo    = $C028
mus_att     = $C029
baj_att     = $C02A
arp         = $C030                 ; los arpones: activo, x, y
bichos      = $C040                 ; tipo, x, y, velocidad, fase, cuenta, y0
sat_y       = $C100
sat_xn      = $C140

; =============================================================================
        .org $0000
        di
        im 1
        ld sp,$DFF0
        jp inicio

        .pad $0038
; ---- las interrupciones del VDP: la de cuadro y la de línea ----
irq:
        push af
        in a,(VCTRL)                ; (leer el estado las borra)
        rlca
        jr nc,@linea
        ld a,(sc_agua)              ; cuadro: el desplazamiento del agua para
        out (VCTRL),a               ; el cuadro siguiente
        ld a,$88
        out (VCTRL),a
        ld a,1
        ld (vblank),a
        pop af
        ei
        reti
@linea:
        ld a,(sc_fondo)             ; línea 127: desde aquí, el del fondo
        out (VCTRL),a
        ld a,$88
        out (VCTRL),a
        pop af
        ei
        reti

        .pad $0066
; ---- el botón de pausa (NMI) ----
nmi:
        push af
        ld a,1
        ld (nmi_pide),a
        pop af
        retn

; =============================================================================
;  El VDP
; =============================================================================
; B=registro C=valor
vdp_reg:
        ld a,c
        out (VCTRL),a
        ld a,b
        or $80
        out (VCTRL),a
        ret
; HL=dirección de la VRAM para escribir
vdp_esc:
        ld a,l
        out (VCTRL),a
        ld a,h
        or $40
        out (VCTRL),a
        ret
; HL=origen BC=octetos: a la VRAM
vdp_copia:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,vdp_copia
        ret
; la casilla (B=fila, C=columna) -> HL = su dirección en la tabla de nombres
casilla:
        ld l,b
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl                   ; fila * 32
        ld a,c
        add a,l
        ld l,a
        add hl,hl                   ; * 2
        ld a,h
        add a,>NOMBRES
        ld h,a
        ret
; HL=texto (0 al final) B=fila C=columna E=atributos: lo escribe
escribe:
        di
        push hl
        call casilla
        call vdp_esc
        pop hl
@c:
        ld a,(hl)
        or a
        jr z,@x
        out (VDATOS),a              ; (las letras están en el dibujo de su código)
        ld a,e
        out (VDATOS),a
        inc hl
        jr @c
@x:
        ei
        ret
; HL=número (3 octetos BCD) B=fila C=columna: seis cifras, en el marcador
cifras:
        di
        push hl
        call casilla
        call vdp_esc
        pop hl
        ld b,3
@c:
        ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        call @una
        ld a,(hl)
        call @una
        inc hl
        djnz @c
        ei
        ret
@una:
        and $0F
        add a,'0'
        out (VDATOS),a
        ld a,$08                    ; (con la paleta de los objetos: blanco sobre negro)
        out (VDATOS),a
        ret

; =============================================================================
;  El principio
; =============================================================================
regs_ini:
        .byte $74                   ; 0: modo 4, interrupción de línea, columna
                                    ;    izquierda tapada, dos filas de arriba fijas
        .byte $02                   ; 1: imagen apagada, objetos de 8x16
        .byte $FF                   ; 2: los nombres en 3800h
        .byte $FF, $FF
        .byte $FF                   ; 5: los objetos en 3F00h
        .byte $FF                   ; 6: sus dibujos, desde el 256
        .byte $00                   ; 7: el borde, el color 16
        .byte $00, $00
        .byte LINEA_FONDO           ; 10: el contador de líneas
regs_fin:

paleta:
        .byte $24, $3F, $39, $3E, $1B, $05, $1A, $04   ; el fondo
        .byte $08, $27, $07, $15, $2A, $00, $0F, $34
        .byte $00, $3F, $0F, $07, $03, $2A, $15, $3D   ; los objetos
        .byte $38, $33, $00, $0C, $30, $06, $15, $2A

firma:      .byte "BUZ"

inicio:
        ld hl,$C000                 ; la RAM a cero
        ld de,$C001
        ld bc,$1FEF
        ld (hl),0
        ldir
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
        call silencio
        ; la paleta
        xor a
        out (VCTRL),a
        ld a,$C0
        out (VCTRL),a
        ld hl,paleta
        ld b,32
@p:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @p
        ; los dibujos: los del fondo en el 0, las letras en el 20h y los
        ; objetos en el 256
        ld hl,$0000
        call vdp_esc
        ld hl,dib_fondo
        ld bc,dib_fondo_fin - dib_fondo
        call vdp_copia
        ld hl,$0400
        call vdp_esc
        ld hl,letras
        ld bc,letras_fin - letras
        call vdp_copia
        ld hl,$2000
        call vdp_esc
        ld hl,dib_objetos
        ld bc,dib_objetos_fin - dib_objetos
        call vdp_copia
        ld hl,$ACE1                 ; la semilla
        ld (rnd),hl
        call lee_mejor
        call pinta_mar
        call titulo_ini
        ld bc,$0162                 ; imagen, interrupción de cuadro, 8x16
        call vdp_reg
        ei

; =============================================================================
;  El bucle: un cuadro cada vez
; =============================================================================
bucle:
        call espera_cuadro
        di                          ; (lo de la VRAM, sin interrupciones)
        call sat_sube
        ei
        ld a,(hud_sucio)
        or a
        call nz,hud
        call lee_mando
        ld a,(nmi_pide)
        or a
        jr z,@n
        xor a
        ld (nmi_pide),a
        call boton_pausa
@n:
        ld a,(pausa)
        or a
        jr nz,bucle
        ld hl,(cuadro)
        inc hl
        ld (cuadro),hl
        ld a,(estado)
        cp E_JUEGO
        jr z,@j
        cp E_FIN
        jr z,@f
        call titulo_paso
        jr @s
@j:
        call juego_paso
        jr @s
@f:
        call fin_paso
@s:
        call musica_paso
        call sfx_paso
        jr bucle

espera_cuadro:
        halt
        ld a,(vblank)
        or a
        jr z,espera_cuadro
        xor a
        ld (vblank),a
        ret

; el mando: pad = lo pulsado, pulsado = lo que se acaba de pulsar
lee_mando:
        in a,(MANDO1)
        cpl
        and $3F
        ld b,a
        ld a,(pad)
        ld (pad_ant),a
        cpl
        and b
        ld (pulsado),a
        ld a,b
        ld (pad),a
        ret

; el botón de pausa: en el título empieza; jugando, para y sigue
boton_pausa:
        ld a,(estado)
        cp E_TITULO
        jp z,juego_ini
        cp E_JUEGO
        ret nz
        ld a,(pausa)
        xor 1
        ld (pausa),a
        call silencio
        ld a,1
        ld (hud_sucio),a
        jp hud

; =============================================================================
;  La pantalla: el mar (filas 2-23) y el marcador (0 y 1)
; =============================================================================
; las filas 2-15: agua, con burbujas y reflejos aquí y allá
pinta_mar:
        di
        ld bc,$0000
        call casilla
        call vdp_esc
        ld b,32                     ; fila 0: negra
@f0:
        ld a,' '
        out (VDATOS),a
        ld a,$08
        out (VDATOS),a
        djnz @f0
        ld b,32                     ; fila 1: la orilla
@f1:
        ld a,3
        out (VDATOS),a
        xor a
        out (VDATOS),a
        djnz @f1
        ld c,2                      ; filas 2-15: agua
@fa:
        ld b,32
@fa1:
        ld a,c                      ; (fila * 7 + columna * 3) mod 23: 0 es
        add a,a                     ; burbuja y 11, reflejo
        add a,a
        add a,a
        sub c
        ld d,a
        ld a,b
        add a,a
        add a,b
        add a,d
@m:
        sub 23
        jr nc,@m
        add a,23
        ld d,0
        or a
        jr nz,@r
        ld d,1
@r:
        cp 11
        jr nz,@r2
        ld d,2
@r2:
        ld a,d
        out (VDATOS),a
        xor a
        out (VDATOS),a
        djnz @fa1
        inc c
        ld a,c
        cp 16
        jr nz,@fa
        ld hl,mapa_fondo            ; filas 16-27: el fondo
        ld bc,12 * 32
@ff:
        ld a,(hl)
        out (VDATOS),a
        xor a
        out (VDATOS),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,@ff
        ei
        ret

; el marcador: puntos, vidas, la mejor y, si está parado, PAUSA
hud:
        xor a
        ld (hud_sucio),a
        ld hl,t_puntos
        ld bc,$0001
        ld e,$08
        call escribe
        ld hl,puntos
        ld bc,$0008
        call cifras
        ld hl,t_mejor
        ld bc,$0014
        ld e,$08
        call escribe
        ld hl,mejor
        ld bc,$001A
        call cifras
        di
        ld bc,$0010                 ; las vidas: el buzo pequeño y cuántas
        call casilla
        call vdp_esc
        ld a,13
        out (VDATOS),a
        ld a,$08
        out (VDATOS),a
        ld a,(vidas)
        add a,'0'
        out (VDATOS),a
        ld a,$08
        out (VDATOS),a
        ei
        ld a,(pausa)                ; fila 1: PAUSA o la orilla
        or a
        ld hl,t_pausa
        jr nz,@p
        ld hl,t_orilla
@p:
        ld bc,$010D
        ld e,$08
        ld a,(pausa)
        or a
        jr nz,@p2
        ld e,0
@p2:
        jp escribe

t_puntos:   .byte "PUNTOS", 0
t_mejor:    .byte "MEJOR", 0
t_pausa:    .byte "PAUSA", 0
t_orilla:   .byte 3, 3, 3, 3, 3, 0

; las filas 16-27 del fondo (la arena y lo que hay en ella)
mapa_fondo:
        .byte 0,0,0,4,0,0,0,0,0,12,0,0,0,0,4,4, 0,0,0,0,0,0,12,0,0,0,4,0,0,0,0,0
        .byte 0,1,0,14,0,0,0,0,0,12,0,0,0,0,14,14, 0,0,0,2,0,0,12,0,0,0,14,0,0,1,0,0
        .byte 5,5,5,5,5,8,9,5,5,5,5,5,5,5,5,5, 5,5,5,5,5,5,5,5,5,8,9,5,5,5,5,5
        .byte 6,6,6,6,6,10,11,6,7,6,6,6,6,15,6,6, 6,6,6,7,6,6,6,6,6,10,11,6,6,6,6,6
        .byte 6,7,6,6,6,6,6,6,6,6,6,6,6,6,6,7, 6,6,6,6,6,6,15,6,6,6,6,6,7,6,6,6
        .byte 6,6,6,6,15,6,6,6,7,6,6,6,6,6,6,6, 6,6,7,6,6,6,6,6,6,6,6,6,6,6,6,6
        .byte 6,6,6,6,6,6,6,6,6,6,6,7,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,15,6,6,6,6,6
        .byte 6,6,7,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,7,6,6,6,6,6,6,6
        .byte 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6
        .byte 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6
        .byte 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6
        .byte 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6

; B=fila: la deja otra vez de agua (para borrar un texto)
borra_fila:
        push bc
        ld c,0
        call casilla
        di
        call vdp_esc
        pop bc
        ld c,b
        ld b,32
@c:
        ld a,c                      ; (lo mismo que pinta_mar)
        add a,a
        add a,a
        add a,a
        sub c
        ld d,a
        ld a,b
        add a,a
        add a,b
        add a,d
@m:
        sub 23
        jr nc,@m
        add a,23
        ld d,0
        or a
        jr nz,@r
        ld d,1
@r:
        cp 11
        jr nz,@r2
        ld d,2
@r2:
        ld a,d
        out (VDATOS),a
        xor a
        out (VDATOS),a
        djnz @c
        ei
        ret

; =============================================================================
;  Los objetos: se apuntan en sat_y y sat_xn y se suben en el retorno vertical
; =============================================================================
sat_cero:
        xor a
        ld (sat_n),a
        ret
; B=y C=x D=dibujo: uno más (si caben)
sat_pon:
        ld a,(sat_n)
        cp 63
        ret nc
        ld l,a
        ld h,>sat_y
        ld a,b
        dec a                       ; (la consola lo pinta una línea más abajo)
        ld (hl),a
        ld a,l
        add a,a
        add a,<sat_xn
        ld l,a
        ld (hl),c
        inc l
        ld (hl),d
        ld hl,sat_n
        inc (hl)
        ret
; B=y C=x D=dibujo: uno de 16 de ancho (dos de 8x16)
sat_pon16:
        push bc
        push de
        call sat_pon
        pop de
        pop bc
        ld a,c
        add a,8
        ld c,a
        inc d
        inc d
        jp sat_pon
; a la VRAM: las alturas (y D0h al final) y las x con los dibujos
sat_sube:
        ld hl,OBJETOS
        call vdp_esc
        ld a,(sat_n)
        ld b,a
        ld hl,sat_y
        or a
        jr z,@fin
@y:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @y
@fin:
        ld a,$D0
        out (VDATOS),a
        ld hl,OBJETOS + $80
        call vdp_esc
        ld a,(sat_n)
        or a
        ret z
        add a,a
        ld b,a
        ld hl,sat_xn
@x:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        djnz @x
        ret

; =============================================================================
;  El título
; =============================================================================
titulo_ini:
        xor a
        ld (estado),a
        ld (pausa),a
        ld (sc_agua),a
        ld a,3
        ld (vidas),a
        call sat_cero
        ld b,6
        call borra_fila
        ld b,10
        call borra_fila
        ld hl,t_buzo
        ld bc,$040D
        ld e,0
        call escribe
        ld hl,t_pulsa
        ld bc,$0806
        ld e,0
        call escribe
        ld hl,t_gpl
        ld bc,$0C04
        ld e,0
        call escribe
        ld a,1
        ld (hud_sucio),a
        jp musica_ini

t_buzo:     .byte "B U Z O", 0
t_pulsa:    .byte "PULSA 1 PARA JUGAR", 0
t_gpl:      .byte "HISPA-DOS 2026 - GPL", 0

titulo_paso:
        call corre_mar
        call sat_cero
        ld a,(cuadro)               ; el buzo, nadando quieto en medio
        rrca
        rrca
        rrca
        and 3
        cp 2
        jr c,@a
        xor 3
@a:
        add a,88
        ld b,a
        ld c,116
        ld d,0
        ld a,(cuadro)
        and $10
        jr z,@b
        ld d,4
@b:
        call sat_pon16
        ld a,(pulsado)
        and B_1 | B_2
        ret z
        jp juego_ini

; el agua y el fondo corren (el fondo, el doble); fuera de la partida, solo el fondo
corre_mar:
        ld a,(estado)               ; (en el título y al final, el agua quieta:
        cp E_JUEGO                  ; ahí están los textos)
        jr nz,@f
        ld hl,sc_agua
        dec (hl)
@f:
        ld hl,sc_fondo
        dec (hl)
        dec (hl)
        ret

; =============================================================================
;  La partida
; =============================================================================
juego_ini:
        ld a,E_JUEGO
        ld (estado),a
        xor a
        ld (pausa),a
        ld (dific),a
        ld (cool),a
        ld (record),a
        ld hl,puntos
        ld (hl),a
        inc hl
        ld (hl),a
        inc hl
        ld (hl),a
        ld hl,arp                   ; sin arpones ni bichos
        ld b,$40
@c:
        ld (hl),a
        inc hl
        djnz @c
        ld a,3
        ld (vidas),a
        ld a,40
        ld (jx),a
        ld a,64
        ld (jy),a
        ld a,60
        ld (invul),a
        ld (spawn_t),a
        ld b,4                      ; fuera los textos del título
        call borra_fila
        ld b,8
        call borra_fila
        ld b,12
        call borra_fila
        ld a,1
        ld (hud_sucio),a
        jp musica_ini

juego_paso:
        call corre_mar
        ; ---- el buzo ----
        ld a,(pad)
        ld c,a
        ld hl,jy
        bit 0,c
        jr z,@ab
        ld a,(hl)
        cp 20
        jr c,@ab
        dec (hl)
        dec (hl)
@ab:
        bit 1,c
        jr z,@iz
        ld a,(hl)
        cp 110
        jr nc,@iz
        inc (hl)
        inc (hl)
@iz:
        ld hl,jx
        bit 2,c
        jr z,@de
        ld a,(hl)
        cp 12
        jr c,@de
        dec (hl)
        dec (hl)
@de:
        bit 3,c
        jr z,@ar
        ld a,(hl)
        cp 224
        jr nc,@ar
        inc (hl)
        inc (hl)
@ar:
        ; ---- el arpón ----
        ld a,(cool)
        or a
        jr z,@a1
        dec a
        ld (cool),a
        jr @mueve
@a1:
        ld a,(pulsado)
        and B_1 | B_2
        jr z,@mueve
        ld hl,arp
        ld b,N_ARPONES
@a2:
        ld a,(hl)
        or a
        jr z,@a3
        inc hl
        inc hl
        inc hl
        djnz @a2
        jr @mueve
@a3:
        ld (hl),1
        inc hl
        ld a,(jx)
        add a,12
        ld (hl),a
        inc hl
        ld a,(jy)
        ld (hl),a
        ld a,6
        ld (cool),a
        ld a,10
        ld (sfx_t),a
@mueve:
        ld hl,arp                   ; los arpones avanzan
        ld b,N_ARPONES
@m1:
        ld a,(hl)
        or a
        jr z,@m2
        inc hl
        ld a,(hl)
        add a,6
        ld (hl),a
        dec hl
        cp 244
        jr c,@m2
        ld (hl),0
@m2:
        inc hl
        inc hl
        inc hl
        djnz @m1
        call saca_bicho
        call mueve_bichos
        call choques
        ld a,(invul)
        or a
        jr z,@pinta
        dec a
        ld (invul),a
@pinta:
        ; ---- los objetos: el buzo, los arpones y los bichos ----
        call sat_cero
        ld a,(invul)                ; (parpadea mientras no le pasa nada)
        and 4
        jr nz,@sinbuzo
        ld a,(jy)
        ld b,a
        ld a,(jx)
        ld c,a
        ld d,0
        ld a,(cuadro)
        and 8
        jr z,@p1
        ld d,4
@p1:
        call sat_pon16
@sinbuzo:
        ld ix,arp
        ld b,N_ARPONES
@p2:
        push bc
        ld a,(ix+0)
        or a
        jr z,@p3
        ld b,(ix+2)
        ld c,(ix+1)
        ld d,8
        call sat_pon
@p3:
        pop bc
        inc ix
        inc ix
        inc ix
        djnz @p2
        ld ix,bichos
        ld b,N_BICHOS
@p4:
        push bc
        ld a,(ix+0)
        or a
        jr z,@p9
        ld b,(ix+2)
        ld c,(ix+1)
        ld e,(ix+4)
        cp T_PEZ
        jr nz,@p5
        ld d,10
        bit 3,e
        jr z,@p6
        ld d,14
@p6:
        call sat_pon16
        jr @p9
@p5:
        cp T_MEDUSA
        jr nz,@p7
        ld d,18
        bit 4,e
        jr z,@p8
        ld d,20
@p8:
        call sat_pon
        jr @p9
@p7:
        ld d,22                     ; la nube
        bit 2,e
        jr z,@p8
        ld d,24
        jr @p8
@p9:
        pop bc
        ld de,BICHO
        add ix,de
        djnz @p4
        ret

; un número aleatorio (registro de desplazamiento de 16 bits) -> A
aleatorio:
        ld hl,(rnd)
        srl h
        rr l
        jr nc,@x
        ld a,h
        xor $B4
        ld h,a
@x:
        ld (rnd),hl
        ld a,l
        ret

; sale un bicho nuevo cuando toca
saca_bicho:
        ld hl,spawn_t
        dec (hl)
        ret nz
        ld ix,bichos                ; uno libre
        ld b,N_BICHOS
@l:
        ld a,(ix+0)
        or a
        jr z,@hay
        ld de,BICHO
        add ix,de
        djnz @l
        ld a,10
        ld (spawn_t),a
        ret
@hay:
        call aleatorio              ; la altura: una de seis filas
        and $07
@seis:
        cp 6
        jr c,@y
        sub 6
        jr @seis
@y:
        add a,a
        add a,a
        add a,a
        add a,a
        add a,22
        ld (ix+2),a
        ld (ix+6),a
        ld (ix+1),240
        ld (ix+4),0
        call aleatorio
        and 3
        jr nz,@pez
        ld (ix+0),T_MEDUSA
        ld (ix+3),1
        jr @t
@pez:
        ld (ix+0),T_PEZ
        call aleatorio
        and 1
        inc a
        ld b,a
        ld a,(dific)
        cp 3
        jr c,@v
        inc b
@v:
        ld (ix+3),b
@t:
        ld a,(dific)                ; el siguiente: antes cuanto más difícil
        add a,a
        add a,a
        ld b,a
        ld a,50
        sub b
        cp 16
        jr nc,@t2
        ld a,16
@t2:
        ld b,a
        call aleatorio
        and 15
        add a,b
        ld (spawn_t),a
        ret

; los bichos se mueven
mueve_bichos:
        ld ix,bichos
        ld b,N_BICHOS
@l:
        push bc
        ld a,(ix+0)
        or a
        jr z,@sig
        inc (ix+4)                  ; la fase (para dibujar y para la medusa)
        cp T_NUBE
        jr nz,@nonube
        dec (ix+5)
        jr nz,@sig
        ld (ix+0),T_LIBRE
        jr @sig
@nonube:
        ld a,(ix+1)                 ; a la izquierda
        sub (ix+3)
        jr c,@fuera
        cp 4
        jr c,@fuera
        ld (ix+1),a
        ld a,(ix+0)
        cp T_PEZ
        jr nz,@medusa
        ld a,(ix+4)                 ; el pez se acerca al buzo, poco a poco
        and 3
        jr nz,@sig
        ld a,(jy)
        cp (ix+2)
        jr z,@sig
        jr c,@sube
        inc (ix+2)
        jr @sig
@sube:
        dec (ix+2)
        jr @sig
@medusa:
        ld a,(ix+4)                 ; la medusa sube y baja
        rrca
        and $1F
        ld e,a
        ld d,0
        ld hl,vaiven
        add hl,de
        ld a,(ix+6)
        add a,(hl)
        ld (ix+2),a
        jr @sig
@fuera:
        ld (ix+0),T_LIBRE
@sig:
        pop bc
        ld de,BICHO
        add ix,de
        djnz @l
        ret

vaiven:     .byte 0, 1, 3, 4, 6, 7, 8, 9, 10, 10, 11, 11, 11, 10, 10, 9
            .byte 8, 7, 6, 4, 3, 1, 0, -1, -2, -3, -4, -4, -4, -3, -2, -1

; ¿se tocan? B,C = centro de uno; D,E = centro del otro; H,L = la suma de
; las medias anchuras y alturas -> CF=1 si se tocan
toca:
        ld a,b
        sub d
        jr nc,@x
        neg
@x:
        cp h
        ret nc
        ld a,c
        sub e
        jr nc,@y
        neg
@y:
        cp l
        ret

; los arpones contra los bichos, y los bichos contra el buzo
choques:
        ld ix,bichos
        ld b,N_BICHOS
@l:
        push bc
        ld a,(ix+0)
        cp T_PEZ
        jr z,@vivo
        cp T_MEDUSA
        jr nz,@sig
@vivo:
        call centro_bicho           ; -> D,E (y H,L: sus medias medidas)
        ld iy,arp
        ld b,N_ARPONES
@a:
        ld a,(iy+0)
        or a
        jr z,@a2
        push bc
        push hl
        ld a,(iy+1)
        add a,4
        ld b,a
        ld a,(iy+2)
        add a,6
        ld c,a
        ld a,h
        add a,4
        ld h,a
        inc l
        call toca
        pop hl
        pop bc
        jr nc,@a2
        ld (iy+0),0                 ; ¡le ha dado!
        call deshecho
        jr @sig
@a2:
        inc iy
        inc iy
        inc iy
        djnz @a
        ld a,(invul)                ; contra el buzo
        or a
        jr nz,@sig
        ld a,(estado)
        cp E_JUEGO
        jr nz,@sig
        ld a,(jx)
        add a,8
        ld b,a
        ld a,(jy)
        add a,7
        ld c,a
        ld a,h
        add a,5
        ld h,a
        ld a,l
        add a,3
        ld l,a
        call toca
        jr nc,@sig
        call golpe
@sig:
        pop bc
        ld de,BICHO
        add ix,de
        djnz @l
        ret

; el centro del bicho IX -> D,E, y sus medias medidas -> H,L
centro_bicho:
        ld a,(ix+0)
        cp T_PEZ
        jr nz,@m
        ld a,(ix+1)
        add a,8
        ld d,a
        ld a,(ix+2)
        add a,7
        ld e,a
        ld hl,$0704
        ret
@m:
        ld a,(ix+1)
        add a,4
        ld d,a
        ld a,(ix+2)
        add a,5
        ld e,a
        ld hl,$0405
        ret

; un arpón ha dado al bicho IX: se deshace y da puntos
deshecho:
        ld a,(ix+0)
        cp T_MEDUSA
        ld a,$20
        jr z,@m
        ld a,$10
@m:
        call suma_puntos
        ld (ix+0),T_NUBE
        ld (ix+5),16
        ld (ix+4),0
        ld a,12                     ; el ruido del golpe
        ld (ru_t),a
        xor a
        ld (ru_largo),a
        ret

; A = puntos (BCD, menos de 100): se suman; cada 100, más difícil
suma_puntos:
        ld hl,puntos + 2
        add a,(hl)
        daa
        ld (hl),a
        dec hl
        ld a,(hl)
        adc a,0
        daa
        ld (hl),a
        dec hl
        ld a,(hl)
        adc a,0
        daa
        ld (hl),a
        jr nc,@d
        ld a,$99                    ; (tope)
        ld (puntos),a
        ld (puntos+1),a
        ld (puntos+2),a
@d:
        ld a,(puntos+1)             ; la dificultad: las centenas (hasta 8)
        and $0F
        ld b,a
        ld a,(puntos)
        or a
        jr nz,@max
        ld a,(puntos+1)
        and $F0
        jr nz,@max
        ld a,b
        cp 8
        jr c,@pon
@max:
        ld a,8
@pon:
        ld (dific),a
        ld a,1
        ld (hud_sucio),a
        ret

; el bicho IX ha chocado con el buzo
golpe:
        ld (ix+0),T_NUBE
        ld (ix+5),16
        ld (ix+4),0
        ld a,24                     ; el ruido largo
        ld (ru_t),a
        ld a,1
        ld (ru_largo),a
        ld a,1
        ld (hud_sucio),a
        ld a,(vidas)
        dec a
        ld (vidas),a
        jp z,fin_ini
        ld a,90
        ld (invul),a
        ret

; =============================================================================
;  El final de la partida
; =============================================================================
fin_ini:
        ld a,E_FIN
        ld (estado),a
        xor a
        ld (tiempo),a
        ld (sc_agua),a
        ld hl,arp                   ; fuera los arpones y los bichos
        ld b,$40
@c:
        ld (hl),a
        inc hl
        djnz @c
        call silencio_musica
        ld hl,t_fin
        ld bc,$0707
        ld e,0
        call escribe
        ld hl,mejor                 ; ¿la mejor?
        ld de,puntos
        ld b,3
@m:
        ld a,(de)
        cp (hl)
        jr c,@no
        jr nz,@si
        inc hl
        inc de
        djnz @m
        jr @no                      ; (igual: no)
@si:
        ld hl,puntos
        ld de,mejor
        ld bc,3
        ldir
        call guarda_mejor
        ld hl,t_record
        ld bc,$0A0A
        ld e,0
        call escribe
@no:
        ld a,1
        ld (hud_sucio),a
        ret

t_fin:      .byte "FIN DE LA PARTIDA", 0
t_record:   .byte "NUEVO RECORD!", 0

fin_paso:
        call corre_mar
        call sat_cero
        ld ix,bichos                ; (las nubes del último golpe)
        ld a,(tiempo)
        cp 255
        jr z,@t
        inc a
        ld (tiempo),a
@t:
        cp 60
        ret c
        ld a,(pulsado)
        and B_1 | B_2
        ret z
        ld b,7
        call borra_fila
        ld b,10
        call borra_fila
        jp titulo_ini

; =============================================================================
;  La RAM del cartucho: la mejor puntuación («BUZ» y tres octetos BCD)
; =============================================================================
lee_mejor:
        ld a,$08                    ; la RAM del cartucho, en 8000h
        ld ($FFFC),a
        ld hl,firma
        ld de,$8000
        ld b,3
@c:
        ld a,(de)
        cp (hl)
        jr nz,@x
        inc hl
        inc de
        djnz @c
        ld hl,$8003
        ld de,mejor
        ld bc,3
        ldir
@x:
        xor a
        ld ($FFFC),a
        ret

guarda_mejor:
        ld a,$08
        ld ($FFFC),a
        ld hl,firma
        ld de,$8000
        ld bc,3
        ldir
        ld hl,mejor
        ld bc,3
        ldir
        xor a
        ld ($FFFC),a
        ret

; =============================================================================
;  El sonido
; =============================================================================
silencio:
        ld a,$9F
        out (PSG),a
        ld a,$BF
        out (PSG),a
        ld a,$DF
        out (PSG),a
        ld a,$FF
        out (PSG),a
        ret
silencio_musica:
        xor a
        ld (mus_p+1),a              ; (sin música)
        ld a,$9F
        out (PSG),a
        ld a,$BF
        out (PSG),a
        ret

musica_ini:
        ld hl,melodia
        ld (mus_p),hl
        ld hl,bajo
        ld (baj_p),hl
        ld a,1
        ld (mus_d),a
        ld (baj_d),a
        ret

; un paso de la música: la melodía en el canal 0 y el bajo en el 1
musica_paso:
        ld a,(mus_p+1)
        or a
        ret z
        ld hl,mus_d
        dec (hl)
        jr nz,@env
        ld hl,(mus_p)
        ld a,(hl)
        cp $FF
        jr nz,@n
        ld hl,melodia
        ld a,(hl)
@n:
        inc hl
        ld b,(hl)
        inc hl
        ld (mus_p),hl
        ld hl,mus_d
        ld (hl),b
        ld c,$80                    ; canal 0
        call nota
        ld a,2
        jr nc,@v
        ld a,15
@v:
        ld (mus_att),a
@env:
        ld a,(mus_d)                ; se apaga un poco al final de cada nota
        cp 3
        jr nc,@e1
        ld a,(mus_att)
        cp 15
        jr z,@e1
        ld a,6
        ld (mus_att),a
@e1:
        ld a,(mus_att)
        or $90
        out (PSG),a
        ; el bajo
        ld hl,baj_d
        dec (hl)
        jr nz,@benv
        ld hl,(baj_p)
        ld a,(hl)
        cp $FF
        jr nz,@bn
        ld hl,bajo
        ld a,(hl)
@bn:
        inc hl
        ld b,(hl)
        inc hl
        ld (baj_p),hl
        ld hl,baj_d
        ld (hl),b
        ld c,$A0                    ; canal 1
        call nota
        ld a,4
        jr nc,@bv
        ld a,15
@bv:
        ld (baj_att),a
@benv:
        ld a,(baj_d)
        cp 4
        jr nc,@b1
        ld a,(baj_att)
        cp 15
        jr z,@b1
        ld a,9
        ld (baj_att),a
@b1:
        ld a,(baj_att)
        or $B0
        out (PSG),a
        ret

; A=nota (0: silencio) C=80h canal 0, A0h canal 1, C0h canal 2: el periodo
; -> CF=1 si es silencio
nota:
        or a
        scf
        ret z
        add a,a
        ld e,a
        ld d,0
        ld hl,periodos - 2
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
; DE=periodo C=canal
periodo:
        ld a,e
        and $0F
        or c
        out (PSG),a
        ld a,e
        srl d
        rra
        srl d
        rra
        srl d
        rra
        srl d
        rra
        and $3F
        out (PSG),a
        or a
        ret

; los efectos: el arpón (canal 2, un silbido que baja) y los golpes (ruido)
sfx_paso:
        ld a,(sfx_t)
        or a
        jr z,@s0
        dec a
        ld (sfx_t),a
        ld b,a                      ; el periodo sube: 90 + (10 - t) * 24
        ld a,10
        sub b
        ld e,a
        ld d,0
        ld h,d
        ld l,e
        add hl,hl
        add hl,de
        add hl,hl
        add hl,hl
        add hl,hl                   ; * 24
        ld de,90
        add hl,de
        ex de,hl
        ld c,$C0
        call periodo
        ld a,(sfx_t)                ; el volumen baja
        ld b,a
        ld a,12
        sub b
        srl a
        or $D0
        out (PSG),a
        jr @ruido
@s0:
        ld a,$DF
        out (PSG),a
@ruido:
        ld a,(ru_t)
        or a
        jr z,@r0
        dec a
        ld (ru_t),a
        ld b,a
        ld a,(ru_largo)
        or a
        ld a,$E5                    ; blanco, rápido
        jr z,@r1
        ld a,$E6                    ; blanco, más grave
@r1:
        out (PSG),a
        ld a,b                      ; el volumen baja con el tiempo
        ld c,a
        ld a,(ru_largo)
        or a
        ld a,c
        jr z,@r2
        srl a
@r2:
        cpl
        and $0F
        or $F0
        out (PSG),a
        ret
@r0:
        ld a,$FF
        out (PSG),a
        ret

; los periodos de las notas (el reloj del sonido entre 32 y la frecuencia)
periodos:
        .word 1017                  ; 1: la 2
        .word 855, 762, 678, 640, 571   ; 2-6: do 3, re 3, mi 3, fa 3, sol 3
        .word 508, 453, 427, 380, 339   ; 7-11: la 3, si 3, do 4, re 4, mi 4
        .word 320, 285, 254, 226, 214   ; 12-16: fa 4, sol 4, la 4, si 4, do 5
        .word 190, 170, 143             ; 17-19: re 5, mi 5, sol 5
LA2 = 1
DO3 = 2
RE3 = 3
MI3 = 4
FA3 = 5
SOL3 = 6
LA3 = 7
SI3 = 8
DO4 = 9
RE4 = 10
MI4 = 11
FA4 = 12
SOL4 = 13
LA4 = 14
SI4 = 15
DO5 = 16
RE5 = 17
MI5 = 18
SOL5 = 19
CORCHEA = 8                             ; una corchea: 8 cuadros

; la melodía (nota, duración); FFh vuelve a empezar. Propia, en la menor
melodia:
        .byte LA4,CORCHEA, MI4,CORCHEA, LA4,CORCHEA, DO5,CORCHEA, SI4,CORCHEA, SOL4,CORCHEA, MI4,CORCHEA, SOL4,CORCHEA
        .byte LA4,CORCHEA, MI4,CORCHEA, LA4,CORCHEA, DO5,CORCHEA, RE5,CORCHEA, DO5,CORCHEA, SI4,2*CORCHEA
        .byte DO5,CORCHEA, SI4,CORCHEA, LA4,CORCHEA, SOL4,CORCHEA, MI4,CORCHEA, SOL4,CORCHEA, LA4,CORCHEA, SI4,CORCHEA
        .byte DO5,CORCHEA, RE5,CORCHEA, MI5,CORCHEA, RE5,CORCHEA, DO5,CORCHEA, SI4,CORCHEA, LA4,2*CORCHEA
        .byte MI5,CORCHEA, 0,CORCHEA, MI5,CORCHEA, RE5,CORCHEA, DO5,2*CORCHEA, SI4,2*CORCHEA
        .byte LA4,CORCHEA, SI4,CORCHEA, DO5,CORCHEA, LA4,CORCHEA, SOL4,2*CORCHEA, MI4,2*CORCHEA
        .byte FA4,CORCHEA, LA4,CORCHEA, DO5,CORCHEA, FA4,CORCHEA, MI4,CORCHEA, SOL4,CORCHEA, SI4,CORCHEA, MI4,CORCHEA
        .byte LA4,2*CORCHEA, MI4,2*CORCHEA, LA4,3*CORCHEA, 0,CORCHEA
        .byte $FF
bajo:
        .byte LA2,2*CORCHEA, MI3,2*CORCHEA, LA2,2*CORCHEA, MI3,2*CORCHEA
        .byte LA2,2*CORCHEA, MI3,2*CORCHEA, SOL3,2*CORCHEA, MI3,2*CORCHEA
        .byte FA3,2*CORCHEA, DO3,2*CORCHEA, SOL3,2*CORCHEA, RE3,2*CORCHEA
        .byte LA2,2*CORCHEA, MI3,2*CORCHEA, LA2,2*CORCHEA, MI3,2*CORCHEA
        .byte DO3,2*CORCHEA, SOL3,2*CORCHEA, SOL3,2*CORCHEA, MI3,2*CORCHEA
        .byte FA3,2*CORCHEA, DO3,2*CORCHEA, SOL3,2*CORCHEA, MI3,2*CORCHEA
        .byte FA3,2*CORCHEA, FA3,2*CORCHEA, MI3,2*CORCHEA, MI3,2*CORCHEA
        .byte LA2,2*CORCHEA, MI3,2*CORCHEA, LA2,4*CORCHEA
        .byte $FF

; =============================================================================
;  Los dibujos
; =============================================================================
        .include "dibujos.s"
letras:
        .include "../comun/letras.s"
letras_fin:

        .pad $7FF0, 0
        .cabecera_sega 0, 1, $4C
