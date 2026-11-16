; =============================================================================
;  MAS.COM (MÁS, MORE) - enseña un texto pantalla a pantalla
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       orden | MAS          lo que escribe la orden, por páginas
;       MAS < archivo        el archivo, por páginas
;       MAS archivo          lo mismo
;
;  Es un filtro: lee de la entrada (que una tubería o «<» cambian por un
;  archivo) y escribe en la salida. Cada vez que se llena la pantalla espera
;  una tecla; Esc lo deja. El aviso «-- Más --» y la tecla van directamente
;  por la BIOS: la entrada está ocupada con el texto, y si la salida se ha
;  mandado a un archivo el aviso no tiene que acabar en él.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        ; cuántas líneas caben: las de la pantalla menos una (25 si la BIOS
        ; no lo dice: las tarjetas anteriores a la EGA)
        mov byte [filas], 24
        push es
        mov ax, 0x40
        mov es, ax
        mov al, [es:0x84]
        pop es
        cmp al, 24
        jb .f
        cmp al, 60
        ja .f
        mov [filas], al
.f:
        ; ¿un archivo? Si no, la entrada
        mov si, 0x81
.e:
        lodsb
        cmp al, ' '
        je .e
        jb lee
        dec si
        mov dx, si
.n:
        lodsb
        cmp al, ' '
        ja .n
        mov byte [si-1], 0
        mov al, 0
        DOS 0x3D
        jnc .abierto
        mov dx, m_no_esta
        DOS 0x09
        mov ax, 0x4C01
        int 0x21
.abierto:
        mov [ent], ax
lee:
        mov bx, [ent]
        mov cx, 512
        mov dx, buf
        DOS 0x3F
        jc fin
        test ax, ax
        jz fin
        mov cx, ax
        mov si, buf
.c:
        lodsb
        cmp al, 0x1A                ; el fin de archivo de los textos
        je fin
        cmp al, 9
        je .tab
        call sale
        cmp al, 10
        je .linea
        cmp al, 13
        jne .letra
        mov byte [col], 0
        jmp short .sig
.tab:
        mov al, ' '                 ; el tabulador: espacios hasta la columna
        call sale                   ; siguiente que sea múltiplo de ocho
        inc byte [col]
        cmp byte [col], 80
        jae .llena
        test byte [col], 7
        jnz .tab
        jmp short .sig
.letra:
        cmp al, ' '
        jb .sig
        inc byte [col]
        cmp byte [col], 80          ; una línea más larga que la pantalla
        jb .sig                     ; ocupa otra
.llena:
        mov byte [col], 0
.linea:
        inc byte [lin]
        mov al, [lin]
        cmp al, [filas]
        jb .sig
        call pausa
.sig:
        loop .c
        jmp lee
fin:
        mov ax, 0x4C00
        int 0x21

; escribe AL en la salida. Conserva los registros
sale:
        push ax
        push dx
        mov dl, al
        DOS 0x02
        pop dx
        pop ax
        ret

; «-- Más --»: espera una tecla y borra el aviso. Con Esc, se acaba
pausa:
        push si
        push cx
        mov byte [lin], 0
        mov si, m_mas
        call bios_cad
        xor ah, ah
        int 0x16
        push ax
        mov si, m_borra
        call bios_cad
        pop ax
        pop cx
        pop si
        cmp al, 27
        je fin
        ret
; escribe el texto de SI (acabado en 0) con la BIOS
bios_cad:
        lodsb
        test al, al
        jz .x
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp bios_cad
.x:
        ret

m_mas       db "-- Más --", 0
m_borra     db 13, "         ", 13, 0
m_no_esta   db "No existe ese archivo.", 13, 10, "$"
ent         dw 0                    ; de dónde se lee (0: la entrada)
col         db 0
lin         db 0
filas       db 24

        absolute $
buf         resb 512
