; =============================================================================
;  Hispa-DOS ETIQUETA.COM (LABEL) - poner, cambiar o quitar la etiqueta de
;  volumen de un disco
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ETIQUETA [unidad:][etiqueta]
;     Sin etiqueta: muestra la actual y el número de serie y la pregunta
;     (ENTRAR sin escribir nada = ninguna; si había una, pregunta si se quita).
;  La etiqueta se guarda en la entrada de volumen del directorio raíz y, si
;  el disco tiene BPB extendido, también en el sector de arranque.
;  Usa INT 25h/26h (sectores absolutos): discos FAT12/FAT16.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        mov ah, 0x19
        int 0x21
        mov [drive], al
        ; argumentos
        mov si, 0x81
        call skip_sp
        cmp word [si], '/?'
        je usage
        cmp byte [si+1], ':'
        jne .lab
        mov al, [si]
        call upcase
        sub al, 'A'
        jb bad_drive
        cmp al, 26
        jae bad_drive
        mov [drive], al
        add si, 2
        call skip_sp
.lab:
        ; el resto de la línea es la etiqueta nueva (puede tener espacios)
        mov di, newlab
        xor cx, cx
.lc:
        lodsb
        cmp al, 13
        je .lce
        test al, al
        jz .lce
        cmp cx, 11
        jae too_long
        stosb
        inc cx
        jmp .lc
.lce:
        mov [newlen], cx
        ; la unidad tiene que existir (y quedar montada)
        mov dl, [drive]
        inc dl
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        je bad_drive
        call read_boot
        jc rd_err
        call find_label
        jc rd_err
        cmp word [newlen], 0
        jne .set
        ; sin etiqueta en la línea: mostrar y preguntar
        call show_info
        mov si, msg_ask
        call puts
        call readline
        cmp cx, 11
        ja too_long
        mov [newlen], cx
        mov si, u_line
        mov di, newlab
        rep movsb
        cmp word [newlen], 0
        jne .set
        cmp byte [have_lab], 0
        je done
        mov si, msg_del
        call puts
        call ask_yes
        jc done
        call del_label
        jmp done
.set:
        call check_label
        jc bad_chars
        call set_label
done:
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
bad_drive:
        mov si, msg_baddrv
        jmp fail
too_long:
        mov si, msg_long
        jmp fail
bad_chars:
        mov si, msg_chars
        jmp fail
rd_err:
        mov si, msg_rderr
        jmp fail
wr_err:
        mov si, msg_wrerr
fail:
        call puts
        mov ax, 0x4C01
        int 0x21

skip_sp:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp skip_sp
.r:
        ret

; ---- sectores absolutos: EAX = sector, BX = búfer; CF si error ----
read_sec:
        mov byte [io_fn], 0x25
        jmp abs_sec
write_sec:
        mov byte [io_fn], 0x26
abs_sec:
        pushad
        mov [pk_lba], eax
        mov word [pk_cnt], 1
        mov [pk_buf], bx
        mov [pk_buf+2], ds
        mov al, [drive]
        mov bx, packet
        mov cx, 0xFFFF
        cmp byte [io_fn], 0x26
        je .w
        int 0x25
        jmp .f
.w:
        int 0x26
.f:
        pop dx                      ; los indicadores que deja en la pila
        popad
        ret

; sector de arranque y geometría del directorio raíz
read_boot:
        xor eax, eax
        mov bx, boot
        call read_sec
        jc .r
        cmp word [boot+0x0B], 512
        jne .e
        movzx eax, word [boot+0x0E]     ; reservados
        movzx ecx, byte [boot+0x10]     ; número de FAT
        jecxz .e
        movzx edx, word [boot+0x16]     ; sectores por FAT
        test edx, edx
        jz .e                           ; FAT32: no se admite
        imul edx, ecx
        add eax, edx
        mov [root_lba], eax
        movzx eax, word [boot+0x11]
        test eax, eax
        jz .e
        add eax, 15
        shr eax, 4
        mov [root_secs], ax
        clc
.r:
        ret
.e:
        stc
        ret

; buscar la entrada de volumen (y un hueco libre) en el directorio raíz
find_label:
        mov word [free_off], 0xFFFF
        xor cx, cx
.s:
        cmp cx, [root_secs]
        jae .end
        movzx eax, cx
        add eax, [root_lba]
        mov bx, dirbuf
        call read_sec
        jc .r
        mov si, dirbuf
.e:
        mov al, [si]
        test al, al
        jz .free0
        cmp al, 0xE5
        je .free
        cmp byte [si+11], 0x0F          ; nombre largo
        je .n
        test byte [si+11], 0x08
        jz .n
        ; la etiqueta
        mov byte [have_lab], 1
        movzx eax, cx
        add eax, [root_lba]
        mov [lab_lba], eax
        sub si, dirbuf
        mov [lab_off], si
        add si, dirbuf
        push cx
        mov di, curlab
        mov cx, 11
        rep movsb
        pop cx
        jmp .end
.free:
        call note_free
.n:
        add si, 32
        cmp si, dirbuf + 512
        jb .e
        inc cx
        jmp .s
.free0:
        call note_free                  ; fin del directorio
.end:
        clc
.r:
        ret
note_free:
        cmp word [free_off], 0xFFFF
        jne .r
        movzx eax, cx
        add eax, [root_lba]
        mov [free_lba], eax
        mov ax, si
        sub ax, dirbuf
        mov [free_off], ax
.r:
        ret

; «El volumen de la unidad X es ...» y el número de serie
show_info:
        mov al, [drive]
        add al, 'A'
        mov [msg_vol_d], al
        mov [msg_nol_d], al
        cmp byte [have_lab], 0
        je .nol
        mov si, msg_vol
        call puts
        mov si, curlab
        mov cx, 11
.tr:
        mov bx, cx
        cmp byte [curlab+bx-1], ' '
        jne .p
        loop .tr
        jmp .pe
.p:
        lodsb
        call putc
        loop .p
.pe:
        call crlf
        jmp .ser
.nol:
        mov si, msg_nol
        call puts
.ser:
        cmp byte [boot+0x26], 0x29
        jne .r
        mov si, msg_ser
        call puts
        mov ax, [boot+0x29]
        call puthex4
        mov al, '-'
        call putc
        mov ax, [boot+0x27]
        call puthex4
        call crlf
.r:
        call crlf
        ret

; validar la etiqueta nueva y pasarla a mayúsculas, con espacios hasta 11
check_label:
        mov si, newlab
        mov cx, [newlen]
        ; los espacios del final no cuentan
.t:
        jcxz .e
        mov bx, cx
        cmp byte [si+bx-1], ' '
        jne .v
        dec cx
        jmp .t
.v:
        mov [newlen], cx
.c:
        mov al, [si]
        call upcase
        cmp al, 0xA4                    ; ñ -> Ñ
        jne .u
        mov al, 0xA5
.u:
        mov [si], al
        cmp al, ' '
        jb .e
        push cx
        push si
        mov di, badch
        mov cx, badch_n
        repne scasb
        pop si
        pop cx
        je .e
        inc si
        loop .c
        mov cx, 11
        sub cx, [newlen]
        mov di, si
        mov al, ' '
        rep stosb
        clc
        ret
.e:
        stc
        ret

; hora y fecha de DOS -> dir_time, dir_date
now:
        mov ah, 0x2C
        int 0x21
        movzx ax, ch
        shl ax, 6
        or al, cl
        shl ax, 5
        shr dh, 1
        or al, dh
        mov [dir_time], ax
        mov ah, 0x2A
        int 0x21
        sub cx, 1980
        mov ax, cx
        shl ax, 4
        or al, dh
        shl ax, 5
        or al, dl
        mov [dir_date], ax
        ret

; escribir la etiqueta newlab (11 caracteres)
set_label:
        call now
        cmp byte [have_lab], 0
        je .new
        mov eax, [lab_lba]
        mov bx, dirbuf
        call read_sec
        jc rd_err
        mov di, [lab_off]
        jmp .fill
.new:
        cmp word [free_off], 0xFFFF
        je .full
        mov eax, [free_lba]
        mov [lab_lba], eax
        mov bx, dirbuf
        call read_sec
        jc rd_err
        mov di, [free_off]
        add di, dirbuf
        push di
        mov cx, 32
        xor al, al
        rep stosb
        pop di
        mov byte [di+11], 0x08
        sub di, dirbuf
.fill:
        add di, dirbuf
        mov si, newlab
        mov cx, 11
        rep movsb
        sub di, 11
        mov ax, [dir_time]
        mov [di+0x16], ax
        mov ax, [dir_date]
        mov [di+0x18], ax
        mov eax, [lab_lba]
        mov bx, dirbuf
        call write_sec
        jc wr_err
        mov si, newlab
        jmp boot_label
.full:
        mov si, msg_full
        jmp fail

; quitar la etiqueta
del_label:
        mov eax, [lab_lba]
        mov bx, dirbuf
        call read_sec
        jc rd_err
        mov di, [lab_off]
        mov byte [dirbuf+di], 0xE5
        mov bx, dirbuf
        call write_sec
        jc wr_err
        mov si, s_noname
; la copia de la etiqueta en el sector de arranque (BPB extendido)
boot_label:
        cmp byte [boot+0x26], 0x29
        jne .r
        mov di, boot+0x2B
        mov cx, 11
        rep movsb
        xor eax, eax
        mov bx, boot
        call write_sec
        jc wr_err
.r:
        ret

%include "util.inc"

badch       db '*?/\|.,;:+=[]<>"'
badch_n     equ $ - badch
s_noname    db "NO NAME    "
msg_usage   db "Pone, cambia o quita la etiqueta de volumen de un disco.", 13, 10, 13, 10
            db "ETIQUETA [unidad:][etiqueta]", 13, 10, 13, 10
            db "Sin etiqueta, muestra la actual y la pregunta.", 13, 10, 0
msg_vol     db " El volumen de la unidad "
msg_vol_d   db "A es ", 0
msg_nol     db " El volumen de la unidad "
msg_nol_d   db "A no tiene etiqueta", 13, 10, 0
msg_ser     db " El número de serie del volumen es ", 0
msg_ask     db "Etiqueta del volumen (11 caracteres, ENTRAR para ninguna)? ", 0
msg_del     db 13, 10, "¿Quitar la etiqueta actual del volumen (S/N)? ", 0
msg_baddrv  db "Especificación de unidad no válida", 13, 10, 0
msg_long    db "La etiqueta tiene más de 11 caracteres", 13, 10, 0
msg_chars   db "Caracteres no válidos en la etiqueta del volumen", 13, 10, 0
msg_rderr   db "No se puede leer el disco (o no es FAT12/FAT16)", 13, 10, 0
msg_wrerr   db "No se puede escribir en el disco", 13, 10, 0
msg_full    db "No queda sitio en el directorio raíz", 13, 10, 0

        section .bss
            alignb 4
bss_start:
packet:
pk_lba      resd 1
pk_cnt      resw 1
pk_buf      resd 1
io_fn       resb 1
drive       resb 1
have_lab    resb 1
            resb 1
newlen      resw 1
root_secs   resw 1
lab_off     resw 1
free_off    resw 1
root_lba    resd 1
lab_lba     resd 1
free_lba    resd 1
dir_time    resw 1
dir_date    resw 1
newlab      resb 12
curlab      resb 12
u_numbuf    resb 16
u_lbuf      resb 84
boot        resb 512
dirbuf      resb 512
            resb 512
stack_top:
