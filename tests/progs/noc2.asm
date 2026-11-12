; NOC2.COM - para las pruebas del ratón: deja residente un INT 15h que rechaza
; los servicios del ratón PS/2 de la BIOS (AH=C2h -> CF=1, AH=86h), como hacen
; las BIOS que no los ofrecen. Así HSPRATON tiene que ir por la vía directa.
        cpu 386
        bits 16
        org 0x100

        jmp init

old15   dd 0
int15:
        cmp ah, 0xC2
        je .no
        jmp far [cs:old15]
.no:
        mov ah, 0x86
        push bp
        mov bp, sp
        or byte [bp+6], 1           ; CF en los indicadores guardados
        pop bp
        iret
fin:

init:
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        mov dx, msg
        mov ah, 9
        int 0x21
        mov dx, fin + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21
msg     db "NOC2: la BIOS ya no ofrece el ratón PS/2.", 13, 10, "$"
