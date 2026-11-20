; =============================================================================
;  Hispa-Snes - el arranque del procesador de sonido (los 64 octetos que hay
;  en FFC0h del SPC700 al encender). Propio: hace lo mismo que el de la
;  consola por fuera (el mismo trato por los cuatro puertos, que es lo que
;  usan los juegos para mandarle su programa), con su propio código.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El trato (documentado): al encender, el procesador de sonido pone AAh y
;  BBh en los puertos 0 y 1 y espera CCh en el puerto 0. Entonces lee la
;  dirección (puertos 2 y 3) y la orden (puerto 1), y devuelve en el puerto 0
;  lo que le han puesto. Orden distinta de 0: un bloque de datos, octeto a
;  octeto (el dato en el puerto 1 y su número, desde 0, en el puerto 0; se
;  devuelve el número); un número «por delante» del que toca es otra orden.
;  Orden 0: salta a la dirección.
;  tools/asm816.py arranque.s arranque.bin; los octetos van en spc.inc.
; =============================================================================
        .cpu spc700
        .rom 64
        .org $FFC0
inicio: mov x, #$EF
        mov sp, x
        mov a, #$00
@borra: mov (x), a              ; la página 0, a cero (de EFh a 01h)
        dec x
        bne @borra
        mov $F4, #$AA           ; «listo»
        mov $F5, #$BB
@espera:
        cmp $F4, #$CC
        bne @espera
orden:  mov x, $F4              ; lo que ha puesto en el puerto 0
        movw ya, $F6            ; la dirección
        movw $00, ya
        mov a, $F5              ; la orden
        mov $F4, x              ; (se devuelve)
        beq salta
        mov y, #$00
@cero:  cmp y, $F4              ; el primer octeto: el número 0
        bne @cero
dato:   mov a, $F5
        mov [$00]+y, a
        mov $F4, y
        inc y
        bne @mira
        inc $01
@mira:  cmp y, $F4              ; ¿el siguiente, el mismo de antes u otra orden?
        beq dato
        bpl @mira
        bra orden
salta:  mov x, #$00
        jmp [!$0000+x]
        .org $FFFE
        .word inicio
