; =============================================================================
;  HispaDOS EDIT.COM (también EDITAR.COM) - editor de textos a pantalla completa
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  EDIT archivo       abre el archivo (o lo crea al guardar si no existe)
;
;  Teclas: flechas, Inicio, Fin, RePág, AvPág, Ctrl+Inicio, Ctrl+Fin,
;  Supr, Retroceso, Intro, Tab; F2 o Ctrl+S guardar; Esc, F10 o Alt+X salir.
;  El texto se guarda con saltos de línea CR LF (formato DOS). Máximo 60 KB.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXTEXT     equ 61440
ROWS        equ 23                  ; filas de texto (1-23)
A_TEXT      equ 0x1F                ; blanco sobre azul
A_BAR       equ 0x70                ; negro sobre gris

start:
        mov sp, stack_top
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; nombre del archivo
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 13
        jne .name
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.name:
        mov di, fname
.n:
        stosb
        lodsb
        cmp al, 13
        je .ne
        cmp al, ' '
        jne .n
.ne:
        mov byte [di], 0
        ; memoria para el texto
        mov bx, 0x1000
        mov ah, 0x48
        int 0x21
        jnc .mem
        mov si, msg_nomem
        call puts
        mov ax, 0x4C02
        int 0x21
.mem:
        mov [tseg], ax
        mov es, ax
        call load
        ; pantalla
        mov ax, 0x0003
        int 0x10
        mov ax, 0xB800
        mov gs, ax
        mov word [cur], 0
        mov word [top], 0
        mov word [leftcol], 0
        mov word [pref], 0
main:
        call render
        xor ah, ah
        int 0x16
        mov byte [msgflag], 0
        cmp al, 0
        je .ext
        cmp al, 0xE0
        jne .ascii
        test ah, ah
        jnz .ext
.ascii:
        cmp al, 27
        je k_exit
        cmp al, 13
        je k_enter
        cmp al, 8
        je k_back
        cmp al, 9
        je k_tab
        cmp al, 0x13                ; Ctrl+S
        je k_save
        cmp al, 32
        jb main
        cmp al, 127
        je main
        call ins_char
        call set_pref
        jmp main
.ext:
        mov al, ah
        cmp al, 0x48
        je k_up
        cmp al, 0x50
        je k_down
        cmp al, 0x4B
        je k_left
        cmp al, 0x4D
        je k_right
        cmp al, 0x47
        je k_home
        cmp al, 0x4F
        je k_end
        cmp al, 0x49
        je k_pgup
        cmp al, 0x51
        je k_pgdn
        cmp al, 0x53
        je k_del
        cmp al, 0x77
        je k_ctop
        cmp al, 0x75
        je k_cend
        cmp al, 0x3C                ; F2
        je k_save
        cmp al, 0x44                ; F10
        je k_exit
        cmp al, 0x2D                ; Alt+X
        je k_exit
        jmp main

k_left:
        cmp word [cur], 0
        je main
        dec word [cur]
        call set_pref
        jmp main
k_right:
        mov ax, [cur]
        cmp ax, [tlen]
        jae main
        inc word [cur]
        call set_pref
        jmp main
k_home:
        mov ax, [cur]
        call line_start
        mov [cur], ax
        call set_pref
        jmp main
k_end:
        mov ax, [cur]
        call line_end
        mov [cur], ax
        call set_pref
        jmp main
k_ctop:
        mov word [cur], 0
        call set_pref
        jmp main
k_cend:
        mov ax, [tlen]
        mov [cur], ax
        call set_pref
        jmp main
k_up:
        call move_up
        jmp main
k_down:
        call move_down
        jmp main
k_pgup:
        mov cx, ROWS - 1
.l:
        push cx
        call move_up
        pop cx
        loop .l
        jmp main
k_pgdn:
        mov cx, ROWS - 1
.l:
        push cx
        call move_down
        pop cx
        loop .l
        jmp main
k_enter:
        mov al, 10
        call ins_char
        call set_pref
        jmp main
k_tab:
        mov al, ' '
        call ins_char
        call set_pref
        mov ax, [pref]
        test ax, 7
        jnz k_tab
        jmp main
k_back:
        cmp word [cur], 0
        je main
        dec word [cur]
        call del_char
        call set_pref
        jmp main
k_del:
        call del_char
        call set_pref
        jmp main
k_save:
        call save
        jmp main
k_exit:
        cmp byte [modified], 0
        je quit
        mov si, msg_ask
        call status_msg
        call render_status
.k:
        xor ah, ah
        int 0x16
        call upcase
        cmp al, 'S'
        je .save
        cmp al, 'Y'
        je .save
        cmp al, 'N'
        je quit
        cmp al, 27
        je .cancel
        jmp .k
.save:
        call save
        cmp byte [modified], 0
        jne main
        jmp quit
.cancel:
        mov byte [msgflag], 0
        jmp main

quit:
        mov ax, 0x0003
        int 0x10
        mov ax, 0x4C00
        int 0x21

; ---- utilidades del texto (ES = segmento del texto) ----
; AX = posición -> AX = principio de su línea
line_start:
        push si
        mov si, ax
.l:
        test si, si
        jz .r
        cmp byte [es:si-1], 10
        je .r
        dec si
        jmp .l
.r:
        mov ax, si
        pop si
        ret
; AX = posición -> AX = posición del salto de línea (o fin del texto)
line_end:
        push si
        mov si, ax
.l:
        cmp si, [tlen]
        jae .r
        cmp byte [es:si], 10
        je .r
        inc si
        jmp .l
.r:
        mov ax, si
        pop si
        ret
; columna de cur -> pref
set_pref:
        mov ax, [cur]
        call line_start
        mov bx, [cur]
        sub bx, ax
        mov [pref], bx
        ret
move_up:
        mov ax, [cur]
        call line_start
        test ax, ax
        jz .r
        mov bx, ax
        dec ax                      ; salto de línea anterior
        mov dx, ax
        call line_start
        add ax, [pref]
        cmp ax, dx
        jbe .s
        mov ax, dx
.s:
        mov [cur], ax
.r:
        ret
move_down:
        mov ax, [cur]
        call line_end
        cmp ax, [tlen]
        jae .r
        inc ax
        mov bx, ax
        call line_end
        mov dx, ax
        mov ax, bx
        add ax, [pref]
        cmp ax, dx
        jbe .s
        mov ax, dx
.s:
        mov [cur], ax
.r:
        ret
; insertar AL en cur
ins_char:
        cmp word [tlen], MAXTEXT
        jae .full
        push ds
        mov bl, al
        mov cx, [tlen]
        sub cx, [cur]
        mov si, [tlen]
        dec si
        mov di, [tlen]
        push es
        pop ds
        std
        rep movsb
        cld
        pop ds
        mov di, [cur]
        mov [es:di], bl
        inc word [tlen]
        inc word [cur]
        mov byte [modified], 1
        ret
.full:
        mov si, msg_full
        jmp status_msg
; borrar el carácter de cur
del_char:
        mov ax, [cur]
        cmp ax, [tlen]
        jae .r
        push ds
        mov cx, [tlen]
        sub cx, ax
        dec cx
        mov di, ax
        mov si, ax
        inc si
        push es
        pop ds
        cld
        rep movsb
        pop ds
        dec word [tlen]
        mov byte [modified], 1
.r:
        ret

; ---- archivo ----
load:
        mov word [tlen], 0
        mov byte [modified], 0
        mov dx, fname
        mov ax, 0x3D00
        int 0x21
        jc .new
        mov bx, ax
        push ds
        mov ds, [tseg]
        xor dx, dx
        mov cx, MAXTEXT
        mov ah, 0x3F
        int 0x21
        pop ds
        jc .close
        mov cx, ax
        ; quitar los CR (y un Ctrl+Z final)
        xor si, si
        xor di, di
.l:
        jcxz .done
        mov al, [es:si]
        inc si
        dec cx
        cmp al, 13
        je .l
        cmp al, 0x1A
        je .done
        mov [es:di], al
        inc di
        jmp .l
.done:
        mov [tlen], di
.close:
        mov ah, 0x3E
        int 0x21
        ret
.new:
        mov byte [newfile], 1
        ret

save:
        mov dx, fname
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .err
        mov [fh], ax
        xor si, si
        xor di, di
.l:
        cmp si, [tlen]
        jae .flush
        mov al, [es:si]
        inc si
        cmp al, 10
        jne .c
        mov byte [obuf+di], 13
        inc di
.c:
        mov [obuf+di], al
        inc di
        cmp di, 1022
        jb .l
        call .write
        jc .err2
        jmp .l
.flush:
        call .write
        jc .err2
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        mov byte [modified], 0
        mov byte [newfile], 0
        mov si, msg_saved
        jmp status_msg
.write:
        mov cx, di
        jcxz .w0
        mov bx, [fh]
        mov dx, obuf
        mov ah, 0x40
        int 0x21
        jc .wr
        cmp ax, cx
        jne .wf
.w0:
        xor di, di
        clc
.wr:
        ret
.wf:
        stc
        ret
.err2:
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
.err:
        mov si, msg_saveerr
        jmp status_msg

status_msg:
        mov [msgptr], si
        mov byte [msgflag], 1
        ret

; ---- pantalla ----
; escribir AL con atributo AH en la fila DH, columna DL
vput:
        push di
        push ax
        movzx di, dh
        imul di, di, 160
        movzx ax, dl
        shl ax, 1
        add di, ax
        pop ax
        mov [gs:di], ax
        pop di
        ret
; cadena DS:SI en la fila DH desde la columna DL (atributo AH) -> DL avanza
vputs:
        push ax
.l:
        mov al, [si]
        test al, al
        jz .r
        cmp dl, 80
        jae .r
        call vput
        inc si
        inc dl
        jmp .l
.r:
        pop ax
        ret
; rellenar con espacios hasta la columna 80
vfill:
        mov al, ' '
.l:
        cmp dl, 80
        jae .r
        call vput
        inc dl
        jmp .l
.r:
        ret
; número AX en la posición [vn_pos] -> DX = posición siguiente
vnum:
        push bx
        push cx
        mov bx, 10
        xor cx, cx
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
        mov dx, [vn_pos]
.p:
        pop ax
        add al, '0'
        mov ah, A_BAR
        call vput
        inc dl
        loop .p
        mov [vn_pos], dx
        pop cx
        pop bx
        ret

render:
        ; mantener el cursor dentro de la ventana
        mov ax, [cur]
        call line_start
        mov [cls], ax
        cmp ax, [top]
        jae .below
        mov [top], ax
.below:
        ; ¿cuántas líneas hay entre top y la línea del cursor?
        mov si, [top]
        xor cx, cx
.cnt:
        cmp si, [cls]
        jae .cd
        mov ax, si
        call line_end
        inc ax
        mov si, ax
        inc cx
        jmp .cnt
.cd:
        cmp cx, ROWS
        jb .vis
        ; bajar top una línea
        mov ax, [top]
        call line_end
        inc ax
        mov [top], ax
        jmp .below
.vis:
        mov [crow], cl
        ; columna y desplazamiento horizontal
        mov ax, [cur]
        sub ax, [cls]
        mov [ccol], ax
        cmp ax, [leftcol]
        jae .h1
        mov [leftcol], ax
.h1:
        mov bx, [leftcol]
        add bx, 79
        cmp ax, bx
        jbe .h2
        sub ax, 79
        mov [leftcol], ax
.h2:
        ; barra de título
        mov dh, 0
        mov dl, 0
        mov ah, A_BAR
        mov si, msg_title
        call vputs
        mov si, fname
        call vputs
        call vfill
        ; texto
        mov si, [top]
        mov dh, 1
.row:
        cmp dh, ROWS + 1
        jae .status
        mov dl, 0
        mov ah, A_TEXT
        cmp si, [tlen]
        ja .empty
        ; saltar leftcol caracteres
        mov cx, [leftcol]
.skip:
        jcxz .draw
        cmp si, [tlen]
        jae .eol
        cmp byte [es:si], 10
        je .eol
        inc si
        dec cx
        jmp .skip
.draw:
        cmp si, [tlen]
        jae .eol
        mov al, [es:si]
        cmp al, 10
        je .eol
        cmp dl, 80
        jae .more
        cmp al, 9
        jne .ch
        mov al, ' '
.ch:
        call vput
        inc dl
.more:
        inc si
        jmp .draw
.eol:
        call vfill
        ; siguiente línea
        cmp si, [tlen]
        jae .last
        inc si                      ; saltar el salto de línea
        inc dh
        jmp .row
.last:
        mov si, 0xFFFF              ; marca: ya no hay más texto
        inc dh
        jmp .row
.empty:
        call vfill
        inc dh
        jmp .row
.status:
        call render_status
        ; cursor
        mov dh, [crow]
        inc dh
        mov ax, [ccol]
        sub ax, [leftcol]
        mov dl, al
        mov ah, 2
        xor bh, bh
        int 0x10
        ret

render_status:
        mov dh, 24
        mov dl, 0
        mov ah, A_BAR
        cmp byte [msgflag], 0
        je .info
        mov al, ' '
        call vput
        inc dl
        mov si, [msgptr]
        call vputs
        call vfill
        ret
.info:
        mov si, msg_line
        call vputs
        ; número de línea = líneas antes del cursor + 1
        push dx
        xor si, si
        mov bx, 1
.ln:
        cmp si, [cls]
        jae .lnd
        cmp byte [es:si], 10
        jne .nl
        inc bx
.nl:
        inc si
        jmp .ln
.lnd:
        pop dx
        mov [vn_pos], dx
        mov ax, bx
        call vnum
        mov si, msg_col
        call vputs
        mov [vn_pos], dx
        mov ax, [ccol]
        inc ax
        call vnum
        cmp byte [modified], 0
        je .nm
        mov si, msg_mod
        call vputs
.nm:
        mov si, msg_keys
        call vputs
        call vfill
        ret

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

msg_usage   db "Uso: EDIT archivo", 13, 10, 0
msg_nomem   db "No hay memoria suficiente.", 13, 10, 0
msg_title   db " HispaDOS EDIT - ", 0
msg_line    db " Línea ", 0
msg_col     db "  Col ", 0
msg_mod     db "  [modificado]", 0
msg_keys    db "    F2 Guardar   Esc Salir", 0
msg_saved   db "Archivo guardado.", 0
msg_saveerr db "No se ha podido guardar el archivo.", 0
msg_full    db "El texto ha llegado al tamaño máximo (60 KB).", 0
msg_ask     db "Hay cambios sin guardar. ¿Guardar los cambios? (S/N/Esc)", 0

        section .bss
tseg        resw 1
tlen        resw 1
cur         resw 1
top         resw 1
leftcol     resw 1
pref        resw 1
cls         resw 1
ccol        resw 1
crow        resb 1
modified    resb 1
newfile     resb 1
msgflag     resb 1
tmpdl       resb 1
            resb 1
msgptr      resw 1
vn_pos      resw 1
fh          resw 1
fname       resb 80
obuf        resb 1024
            resb 1024
stack_top:
prog_end:
