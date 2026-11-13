; SINIRQ.COM - [SB-EOI] para las pruebas: hace como las BIOS que no dan IRQ
; a la tarjeta de sonido (el ThinkPad A275: «IRQ sin asignar» en su HD Audio
; Realtek): pone FFh en el registro 3Ch (línea de interrupción) de las
; funciones PCI de audio (clase 04h, subclases 01h y 03h).
        cpu 386
        bits 16
        org 0x100
        mov ecx, 0x040300
        call find
        mov ecx, 0x040100
        call find
        mov dx, msg
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
find:
        xor si, si
.f:
        push ecx
        push si
        mov ax, 0xB103              ; buscar por clase
        int 0x1A
        pop si
        pop ecx
        jc .r
        push ecx
        push si
        mov ax, 0xB10B              ; escribir un octeto
        mov di, 0x3C
        mov cl, 0xFF
        int 0x1A
        pop si
        pop ecx
        inc si
        jmp .f
.r:
        ret
msg     db "SINIRQ: tarjetas de sonido sin IRQ", 13, 10, "$"
