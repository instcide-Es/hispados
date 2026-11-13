; =============================================================================
;  CPUTEST.NES - prueba propia de Hispa-Nes: procesador, imagen, sonido y mando
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Una ROM de prueba hecha aquí (se ensambla con tools/asm6502.py), para que
;  las pruebas del emulador no dependan de nada descargado. Hace 24 pruebas
;  seguidas; si una falla, se para y apunta su número. El resultado queda:
;   - en 6000h: 0 si todo va bien, o el número de la prueba que falla (y
;     80h mientras corre); en 6001h-6003h, DEh B0h 61h; desde 6004h, un texto
;     («CPUTEST BIEN» o «CPUTEST FALLA nn»);
;   - en la pantalla: toda verde si va bien, toda roja si falla.
;  Cartucho: 16 KB de programa, dibujos en RAM, tablas de nombres una encima
;  de otra (2000h = 2400h).
;
;   1 LDA y las banderas Z y N        13 SED no cambia la suma
;   2 ADC: acarreo y desbordamiento   14 no oficiales: LAX, SAX, DCP, ISB, SLO
;   3 SBC                             15 2002h: leerlo quita el aviso
;   4 CMP, CPX, CPY                   16 2007h: la lectura va un octeto atrás
;   5 ASL, LSR, ROL, ROR              17 la paleta y sus espejos
;   6 INC, DEC, INX, DEY              18 dibujos en RAM
;   7 la pila: PHA, PLA, PHP, PLP     19 objetos: 2004h y la copia de 4014h
;   8 JSR, RTS                        20 el aviso del objeto 0
;   9 JMP () al final de página       21 NMI: una por cuadro, cada 29.780 ciclos
;  10 indexados y páginas             22 sonido: 4015h y la interrupción
;  11 BIT                             23 el mando
;  12 BRK, RTI                        24 las tablas de nombres y su espejo
; =============================================================================
num         = $00                   ; la prueba que se está haciendo
t0          = $01
t1          = $02
nmis        = $03
irqs        = $04
cnt         = $05                   ; (2 octetos)
ptr         = $10                   ; (2 octetos)

        .byte "NES", $1A, 1, 0, $00, 0
        .res 8, 0

        .org $C000
inicio:
        sei
        cld
        ldx #$FF
        txs
        inx
        stx $2000
        stx $2001
        lda #$40
        sta $4017
        lda #$80
        sta $6000
        lda #$DE
        sta $6001
        lda #$B0
        sta $6002
        lda #$61
        sta $6003
        lda #0
        sta $6004
        sta nmis
        sta irqs
        bit $2002
@v1:
        bit $2002
        bpl @v1
@v2:
        bit $2002
        bpl @v2

; ---- 1: LDA y las banderas ----
p1:
        lda #1
        sta num
        lda #0
        bne mal1
        bmi mal1
        lda #$80
        beq mal1
        bpl mal1
        lda #$7F
        bmi mal1
        beq mal1
; ---- 2: ADC ----
p2:
        inc num
        clc
        lda #$7F
        adc #$01
        bvc mal1
        bpl mal1
        bcs mal1
        cmp #$80
        bne mal1
        sec
        lda #$FF
        adc #$00
        bne mal1
        bcc mal1
        bvs mal1
        clc
        lda #$80
        adc #$80
        bvc mal1
        bcc mal1
        bne mal1
; ---- 3: SBC ----
p3:
        inc num
        sec
        lda #$50
        sbc #$70
        bcs mal1
        bvs mal1
        cmp #$E0
        bne mal1
        sec
        lda #$50
        sbc #$B0
        bvc mal1
        cmp #$A0
        bne mal1
        clc
        lda #$10
        sbc #$0F
        bne mal1
        bcc mal1
        jmp p4
mal1:
        jmp falla
; ---- 4: CMP, CPX, CPY ----
p4:
        inc num
        lda #5
        cmp #5
        bne mal1
        bcc mal1
        cmp #6
        bcs mal1
        bpl mal1
        ldx #$80
        cpx #$7F
        bcc mal1
        beq mal1
        ldy #0
        cpy #1
        bcs mal1
        bpl mal1
; ---- 5: desplazamientos ----
p5:
        inc num
        lda #$81
        asl a
        bcc mal1
        cmp #$02
        bne mal1
        lsr a
        bcs mal1
        cmp #$01
        bne mal1
        sec
        lda #$40
        sta t0
        rol t0
        bcs mal1
        lda t0
        cmp #$81
        bne mal1
        clc
        ror t0
        bcc mal1
        lda t0
        cmp #$40
        bne mal1
        lda #$01
        sta $0300
        lsr $0300
        bcc mal1
        bne mal1
; ---- 6: INC, DEC ----
p6:
        inc num
        lda #$FF
        sta t0
        inc t0
        bne mal1
        dec t0
        bpl mal1
        ldx #$FF
        inx
        bne mal1
        ldy #0
        dey
        cpy #$FF
        bne mal2
        lda #$7F
        sta $0301
        ldx #1
        inc $0300,x
        bpl mal2
; ---- 7: la pila ----
p7:
        inc num
        ldx #$F0
        txs
        lda #$5A
        pha
        lda #0
        pla
        cmp #$5A
        bne mal2
        tsx
        cpx #$F0
        bne mal2
        sec
        lda #0                      ; Z y C puestas
        php
        clc
        lda #1
        plp
        bcc mal2
        bne mal2
        php
        pla
        and #$30
        cmp #$30                    ; en la pila, B y el bit 5 van puestos
        bne mal2
        ldx #$FF
        txs
; ---- 8: JSR, RTS ----
p8:
        inc num
        lda #0
        sta t0
        jsr sub8
        jsr sub8
        lda t0
        cmp #2
        bne mal2
        tsx
        cpx #$FF
        bne mal2
        jmp p9
sub8:
        inc t0
        rts
mal2:
        jmp falla
; ---- 9: JMP () con el puntero al final de la página ----
p9:
        inc num
        lda #<bien9
        sta $03FF
        lda #>bien9
        sta $0300                   ; (el octeto alto sale de 0300h, no de 0400h)
        lda #>mal2
        sta $0400
        jmp ($03FF)
bien9:
; ---- 10: indexados ----
p10:
        inc num
        lda #$11
        sta $0380
        lda #$22
        sta $0480
        ldx #$81
        lda $03FF,x                 ; 03FFh + 81h = 0480h
        cmp #$22
        bne mal2
        ldy #$81
        lda $02FF,y                 ; 0380h
        cmp #$11
        bne mal2
        lda num                     ; (página cero,X): el puntero da la vuelta
        pha                         ; en la página (su octeto alto cae en 00h,
        lda #$80                    ; donde está el número de la prueba)
        sta $FF
        lda #$03
        sta $00
        ldx #$FF
        lda #$33
        sta $0380
        lda ($00,x)                 ; puntero en FFh y 00h -> 0380h
        tay
        pla
        sta num
        cpy #$33
        bne mal2
        lda #$F0                    ; (página cero),Y con cambio de página
        sta ptr
        lda #$03
        sta ptr+1
        ldy #$90
        lda #$44
        sta $0480
        lda (ptr),y                 ; 03F0h + 90h = 0480h
        cmp #$44
        bne mal2
        lda #$55
        sta (ptr),y
        lda $0480
        cmp #$55
        bne mal2
        ldx #$10                    ; página cero,X da la vuelta
        lda #$66
        sta $05
        lda $F5,x
        cmp #$66
        bne mal3
        lda #0
        sta $05
; ---- 11: BIT ----
p11:
        inc num
        lda #$C0
        sta t0
        lda #$3F
        bit t0
        bne mal3
        bpl mal3
        bvc mal3
        lda #$01
        sta t0
        bit t0
        beq mal3
        bmi mal3
        bvs mal3
; ---- 12: BRK, RTI ----
p12:
        inc num
        lda #0
        sta irqs
        brk
        .byte $EA                   ; (el octeto que BRK se salta)
        lda irqs
        cmp #1
        bne mal3
        lda t1                      ; lo que había en la pila: con B
        and #$10
        beq mal3
; ---- 13: no hay modo decimal ----
p13:
        inc num
        sed
        clc
        lda #$09
        adc #$01
        cld
        cmp #$0A
        bne mal3
; ---- 14: no oficiales ----
p14:
        inc num
        lda #$3C
        sta t0
        .byte $A7, t0               ; LAX: A y X a la vez
        cmp #$3C
        bne mal3
        cpx #$3C
        bne mal3
        lda #$F0
        ldx #$3C
        .byte $87, t0               ; SAX: guarda A y X
        lda t0
        cmp #$30
        bne mal3
        lda #$2F
        .byte $C7, t0               ; DCP: resta 1 y compara
        bne mal3
        bcc mal3
        sec
        lda #$40
        .byte $E7, t0               ; ISB: suma 1 y resta de A
        cmp #$10
        bne mal3
        lda #$01
        .byte $07, t0               ; SLO: desplaza y OR
        cmp #$61
        bne mal3
        jmp p15
mal3:
        jmp falla
; ---- 15: leer 2002h quita el aviso de retorno vertical ----
p15:
        inc num
        bit $2002
@v:
        bit $2002
        bpl @v
        bit $2002
        bmi mal3
; ---- 16: la lectura de 2007h va un octeto atrás ----
p16:
        inc num
        lda #$24
        sta $2006
        lda #$00
        sta $2006
        lda #$AB
        sta $2007
        lda #$CD
        sta $2007
        lda #$24
        sta $2006
        lda #$00
        sta $2006
        lda $2007
        lda $2007
        cmp #$AB
        bne mal3
        lda $2007
        cmp #$CD
        bne mal3
; ---- 17: la paleta ----
p17:
        inc num
        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        lda #$21
        sta $2007
        lda #$3F
        sta $2006
        lda #$10
        sta $2006
        lda #$15                    ; 3F10h es el mismo que 3F00h
        sta $2007
        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        lda $2007                   ; (la paleta se lee al momento)
        and #$3F
        cmp #$15
        bne mal3
; ---- 18: dibujos en RAM ----
p18:
        inc num
        lda #$00
        sta $2006
        lda #$10
        sta $2006
        ldx #16
        lda #$FF                    ; el dibujo 1, entero del color 3
@d:
        sta $2007
        dex
        bne @d
        lda #$00
        sta $2006
        lda #$10
        sta $2006
        lda $2007
        lda $2007
        cmp #$FF
        bne mal4
; ---- 19: los objetos ----
p19:
        inc num
        lda #5
        sta $2003
        lda #$77
        sta $2004
        lda #5
        sta $2003
        lda $2004
        cmp #$77
        bne mal4
        ldx #0
@o:
        txa
        sta $0300,x
        inx
        bne @o
        lda #0
        sta $2003
        lda #$03
        sta $4014
        lda #9
        sta $2003
        lda $2004
        cmp #9
        bne mal4
        jmp p20
mal4:
        jmp falla
; ---- 20: el aviso del objeto 0 ----
p20:
        inc num
        lda #$20                    ; el dibujo 1 en la casilla (2, 2)
        sta $2006
        lda #$42
        sta $2006
        lda #1
        sta $2007
        ldx #0                      ; los objetos: fuera, menos el 0, encima
        lda #$FF
@f:
        sta $0300,x
        inx
        bne @f
        lda #15
        sta $0300
        lda #1
        sta $0301
        lda #0
        sta $0302
        lda #16
        sta $0303
        lda #0
        sta $2003
        lda #$03
        sta $4014
        bit $2002
@v:
        bit $2002
        bpl @v
        lda #0
        sta $2000
        sta $2005
        sta $2005
        lda #$1E
        sta $2001
        ; durante el retorno vertical no hay aviso; en la imagen, sí
        bit $2002
        bvs mal4
        ldx #0
        ldy #0
@e:
        bit $2002
        bvs @si
        bmi mal4                    ; ha llegado el retorno siguiente sin aviso
        dex
        bne @e
        dey
        bne @e
        jmp falla
@si:
        lda #0
        sta $2001
; ---- 21: la NMI ----
p21:
        inc num
        bit $2002
        lda #0
        sta nmis
        lda #$80
        sta $2000
@n1:
        lda nmis
        beq @n1
        sta t0
        lda #0
        sta cnt
        sta cnt+1
@n2:
        inc cnt                     ; 17 ciclos por vuelta (21 cada 256)
        bne @n3
        inc cnt+1
@n3:
        lda nmis
        cmp t0
        beq @n2
        lda #0
        sta $2000
        lda nmis
        cmp #2
        bne mal5
        lda cnt+1                   ; 29.780,5 ciclos: unas 1.750 vueltas
        cmp #$06
        bne mal5
        lda cnt
        cmp #$C0
        bcc mal5
        cmp #$E4
        bcs mal5
; ---- 22: el sonido ----
p22:
        inc num
        lda #$01
        sta $4015
        lda #$08
        sta $4003
        lda $4015
        and #$01
        beq mal5
        lda #$00
        sta $4015
        lda $4015
        and #$01
        bne mal5
        lda #$00                    ; secuencia de cuatro pasos con interrupción
        sta $4017
        ldx #3
@a:
        bit $2002
@v:
        bit $2002
        bpl @v
        dex
        bne @a
        lda $4015
        and #$40
        beq mal5
        lda $4015                   ; (leerlo la había quitado)
        and #$40
        bne mal5
        lda #$40
        sta $4017
; ---- 23: el mando (sin tocar nada) ----
p23:
        inc num
        lda #1
        sta $4016
        lda #0
        sta $4016
        ldx #8
@m:
        lda $4016
        and #$01
        bne mal5
        dex
        bne @m
        lda $4016                   ; después de los ocho, unos
        and #$01
        beq mal5
        jmp p24
mal5:
        jmp falla
; ---- 24: las tablas de nombres ----
p24:
        inc num
        lda #$20
        ldx #$11
        jsr pon_nt
        lda #$28
        ldx #$22
        jsr pon_nt
        lda #$24                    ; 2400h es la misma que 2000h
        jsr lee_nt
        cmp #$11
        bne mal5
        lda #$2C                    ; y 2C00h, la misma que 2800h
        jsr lee_nt
        cmp #$22
        bne mal5

; ---- todo bien: pantalla verde ----
        lda #0
        sta $6000
        ldx #0
@t:
        lda txt_bien,x
        sta $6004,x
        beq @f
        inx
        bne @t
@f:
        lda #$2A
        jmp color

; ---- una ha fallado: su número y pantalla roja ----
falla:
        ldx #0
@t:
        lda txt_falla,x
        beq @n
        sta $6004,x
        inx
        bne @t
@n:
        lda num
        lsr a
        lsr a
        lsr a
        lsr a
        jsr cifra
        lda num
        jsr cifra
        lda #0
        sta $6004,x
        lda num
        sta $6000
        lda #$16
color:
        pha
        bit $2002
@v:
        bit $2002
        bpl @v
        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        pla
        sta $2007
        lda #$00
        sta $2006
        sta $2006
        sta $2000
        sta $2001
@fin:
        jmp @fin

cifra:
        and #$0F
        cmp #10
        bcc @d
        adc #6
@d:
        adc #$30
        sta $6004,x
        inx
        rts

; A=octeto alto de la dirección, X=valor: lo escribe al principio de la tabla
pon_nt:
        sta $2006
        lda #$05
        sta $2006
        stx $2007
        rts
; A=octeto alto -> A=lo que hay
lee_nt:
        sta $2006
        lda #$05
        sta $2006
        lda $2007
        lda $2007
        rts

txt_bien:
        .byte "CPUTEST BIEN", 0
txt_falla:
        .byte "CPUTEST FALLA ", 0

nmi:
        inc nmis
        rti
irq:
        pha                         ; (BRK): apunta el registro P de la pila
        txa
        pha
        tsx
        lda $0103,x
        sta t1
        inc irqs
        pla
        tax
        pla
        rti

        .pad $FFFA
        .word nmi, inicio, irq
