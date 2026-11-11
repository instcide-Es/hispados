; =============================================================================
;  Hispa-DOS GESTOR.COM - gestor de archivos a pantalla completa (estilo DOSSHELL)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Flechas, RePág, AvPág, Inicio, Fin: moverse.  Una letra: saltar a ella.
;  Intro: entrar en la carpeta / ejecutar el programa / editar el texto.
;  Retroceso: carpeta superior.   F4 Editar   F5 Copiar   F6 Renombrar o mover
;  F7 Crear carpeta   F8 Borrar   F9 Cambiar de unidad   Esc o F10 Salir.
;  Las operaciones se hacen con COMMAND.COM (COPY, REN, MD, DEL, DELTREE) y EDITAR.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXENT      equ 400
ENT_LEN     equ 24                  ; nombre(13) atrib(1) tamaño(4) fecha(2) hora(2) pad(2)
LIST_TOP    equ 3
LIST_ROWS   equ 19
LIST_W      equ 46
A_NORM      equ 0x1F
A_SEL       equ 0x70
A_BAR       equ 0x30
A_DIR       equ 0x1E
A_PANEL     equ 0x17

start:
        mov sp, stack_top
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; COMSPEC del entorno
        call find_comspec
        mov ax, 0xB800
        mov gs, ax
reload:
        call read_dir
        mov word [sel], 0
        mov word [first], 0
redraw:
        mov ax, 0x0003
        int 0x10
        mov ah, 1                   ; ocultar el cursor
        mov cx, 0x2000
        int 0x10
loop_:
        call draw
        xor ah, ah
        int 0x16
        cmp al, 0
        je .ext
        cmp al, 0xE0
        jne .asc
        test ah, ah
        jnz .ext
.asc:
        cmp al, 27
        je exit_
        cmp al, 13
        je k_enter
        cmp al, 8
        je k_parent
        call upcase
        cmp al, 'A'
        jb loop_
        cmp al, 'Z'
        jbe k_letter
        jmp loop_
.ext:
        mov al, ah
        cmp al, 0x48
        je k_up
        cmp al, 0x50
        je k_down
        cmp al, 0x49
        je k_pgup
        cmp al, 0x51
        je k_pgdn
        cmp al, 0x47
        je k_home
        cmp al, 0x4F
        je k_end
        cmp al, 0x3E
        je k_edit
        cmp al, 0x3F
        je k_copy
        cmp al, 0x40
        je k_ren
        cmp al, 0x41
        je k_mkdir
        cmp al, 0x42
        je k_del
        cmp al, 0x43
        je k_drive
        cmp al, 0x44
        je exit_
        jmp loop_

exit_:
        mov ax, 0x0003
        int 0x10
        mov ax, 0x4C00
        int 0x21

k_up:
        cmp word [sel], 0
        je loop_
        dec word [sel]
        jmp loop_
k_down:
        mov ax, [sel]
        inc ax
        cmp ax, [nent]
        jae loop_
        mov [sel], ax
        jmp loop_
k_pgup:
        mov ax, [sel]
        sub ax, LIST_ROWS - 1
        jae .s
        xor ax, ax
.s:
        mov [sel], ax
        jmp loop_
k_pgdn:
        mov ax, [sel]
        add ax, LIST_ROWS - 1
        cmp ax, [nent]
        jb .s
        mov ax, [nent]
        dec ax
.s:
        mov [sel], ax
        jmp loop_
k_home:
        mov word [sel], 0
        jmp loop_
k_end:
        mov ax, [nent]
        dec ax
        mov [sel], ax
        jmp loop_
k_letter:
        ; primer elemento (tras el actual) que empieza por la letra
        mov bl, al
        mov cx, [nent]
        mov ax, [sel]
.l:
        inc ax
        cmp ax, [nent]
        jb .c
        xor ax, ax
.c:
        call ent_ptr
        cmp [si], bl
        je .f
        loop .l
        jmp loop_
.f:
        mov [sel], ax
        jmp loop_

k_parent:
        mov dx, s_dotdot
        mov ah, 0x3B
        int 0x21
        jmp reload

k_enter:
        cmp word [nent], 0
        je loop_
        mov ax, [sel]
        call ent_ptr
        test byte [si+13], 0x10
        jz .file
        mov dx, si
        mov ah, 0x3B
        int 0x21
        jmp reload
.file:
        ; ¿programa?
        call ext_of
        cmp eax, 'COM'
        je .run
        cmp eax, 'EXE'
        je .run
        cmp eax, 'BAT'
        je .run
        jmp k_edit
.run:
        mov di, cmdline
        call cat_name
        call run_cmd
        jmp reload

k_edit:
        cmp word [nent], 0
        je loop_
        mov ax, [sel]
        call ent_ptr
        test byte [si+13], 0x10
        jnz loop_
        mov di, cmdline
        mov si, s_edit
        call cat
        mov ax, [sel]
        call ent_ptr
        call cat_name
        call run_cmd_quiet
        jmp reload

k_copy:
        mov si, s_copy
        mov bx, ask_copy
        jmp two_names
k_ren:
        mov si, s_ren
        mov bx, ask_ren
two_names:
        cmp word [nent], 0
        je loop_
        mov [op], si
        mov si, bx
        call ask
        jc redraw
        mov di, cmdline
        mov si, [op]
        call cat
        mov ax, [sel]
        call ent_ptr
        call cat_name
        mov al, ' '
        stosb
        mov si, inbuf
        call cat
        call run_cmd
        jmp reload

k_mkdir:
        mov si, ask_md
        call ask
        jc redraw
        mov di, cmdline
        mov si, s_md
        call cat
        mov si, inbuf
        call cat
        call run_cmd
        jmp reload

k_del:
        cmp word [nent], 0
        je loop_
        mov ax, [sel]
        call ent_ptr
        test byte [si+13], 0x10
        mov si, ask_del
        jz .ask
        mov si, ask_deltree         ; carpeta: con todo su contenido
.ask:
        call ask_yn
        jc redraw
        mov di, cmdline
        mov ax, [sel]
        call ent_ptr
        mov bx, s_del
        test byte [si+13], 0x10
        jz .f
        mov bx, s_rd
.f:
        push si
        mov si, bx
        call cat
        pop si
        call cat_name
        call run_cmd
        jmp reload

k_drive:
        mov si, ask_drv
        call ask
        jc redraw
        mov al, [inbuf]
        call upcase
        sub al, 'A'
        jb redraw
        mov dl, al
        mov ah, 0x0E
        int 0x21
        jmp reload

; ---- ejecutar con COMMAND /C cmdline (DI = final de la orden) ----
run_cmd:
        mov byte [quiet], 0
        jmp run_any
run_cmd_quiet:
        mov byte [quiet], 1
run_any:
        mov byte [di], 0
        ; cola: " /C orden" + CR
        mov di, tail + 1
        mov si, s_slashc
        call cat
        mov si, cmdline
        call cat
        mov ax, di
        sub ax, tail + 1
        mov [tail], al
        mov byte [di], 13
        mov ax, 0x0003
        int 0x10
        cmp byte [quiet], 0
        jne .q
        mov si, cmdline
        call puts
        call crlf
.q:
        mov word [pblock], 0
        mov word [pblock+2], tail
        mov [pblock+4], cs
        mov dword [pblock+6], 0
        mov dword [pblock+10], 0
        mov dx, comspec
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        cmp byte [quiet], 0
        jne .r
        mov si, msg_key
        call puts
        xor ah, ah
        int 0x16
.r:
        ret

; ---- preguntas en la última fila ----
; SI = pregunta -> inbuf; CF si se cancela con Esc o vacío
ask:
        mov dh, 24
        mov dl, 0
        mov ah, A_SEL
        call vputs
        mov [ask_col], dl
        call vfill
        xor cx, cx
.k:
        mov dh, 24
        mov dl, [ask_col]
        add dl, cl
        push cx
        mov ah, 2
        xor bh, bh
        push dx
        mov dx, dx
        pop dx
        int 0x10
        mov ah, 1
        mov cx, 0x0607
        int 0x10
        pop cx
        xor ah, ah
        int 0x16
        cmp al, 27
        je .cancel
        cmp al, 13
        je .done
        cmp al, 8
        je .bs
        cmp al, 32
        jb .k
        cmp cx, 60
        jae .k
        mov bx, cx
        mov [inbuf+bx], al
        inc cx
        mov dh, 24
        mov dl, [ask_col]
        add dl, bl
        mov ah, A_SEL
        call vput
        jmp .k
.bs:
        jcxz .k
        dec cx
        mov dh, 24
        mov dl, [ask_col]
        add dl, cl
        mov ax, (A_SEL << 8) | ' '
        call vput
        jmp .k
.done:
        mov bx, cx
        mov byte [inbuf+bx], 0
        call hide_cursor
        jcxz .c0
        clc
        ret
.cancel:
        call hide_cursor
.c0:
        stc
        ret
hide_cursor:
        push cx
        mov ah, 1
        mov cx, 0x2000
        int 0x10
        pop cx
        ret
; SI = pregunta S/N -> CF si no
ask_yn:
        mov dh, 24
        mov dl, 0
        mov ah, A_SEL
        call vputs
        call vfill
.k:
        xor ah, ah
        int 0x16
        call upcase
        cmp al, 'S'
        je .y
        cmp al, 'Y'
        je .y
        cmp al, 'N'
        je .n
        cmp al, 27
        je .n
        jmp .k
.y:
        clc
        ret
.n:
        stc
        ret

; ---- directorio ----
read_dir:
        mov word [nent], 0
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, s_all
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .sort
.l:
        cmp byte [dta+0x1E], '.'
        jne .add
        cmp byte [dta+0x1F], 0      ; "." no se muestra; ".." sí
        je .next
.add:
        mov ax, [nent]
        cmp ax, MAXENT
        jae .sort
        call ent_ptr
        mov di, si
        mov si, dta+0x1E
        mov cx, 13
        rep movsb
        mov al, [dta+0x15]
        stosb
        mov eax, [dta+0x1A]
        stosd
        mov ax, [dta+0x18]
        stosw
        mov ax, [dta+0x16]
        stosw
        inc word [nent]
.next:
        mov ah, 0x4F
        int 0x21
        jnc .l
.sort:
        ; inserción: carpetas primero, después por nombre
        mov cx, 1
.o:
        cmp cx, [nent]
        jae .done
        mov bx, cx
.i:
        test bx, bx
        jz .ni
        mov ax, bx
        call ent_ptr
        mov di, si
        mov ax, bx
        dec ax
        call ent_ptr
        call ent_less               ; ¿DI < SI?
        jnc .ni
        call ent_swap
        dec bx
        jmp .i
.ni:
        inc cx
        jmp .o
.done:
        ; ruta actual y espacio libre
        mov ah, 0x19
        int 0x21
        mov [drive], al
        add al, 'A'
        mov [path], al
        mov word [path+1], '\:'
        xchg al, al
        mov byte [path+1], ':'
        mov byte [path+2], '\'
        mov si, path+3
        mov dl, 0
        mov ah, 0x47
        int 0x21
        mov ah, 0x36
        mov dl, 0
        int 0x21
        movzx eax, ax
        movzx ebx, bx
        movzx ecx, cx
        mul ebx
        mul ecx
        mov [freeb], eax
        ret

; AX = índice -> SI = entrada
ent_ptr:
        push ax
        push dx
        mov si, ENT_LEN
        mul si
        mov si, ax
        add si, entries
        pop dx
        pop ax
        ret
; ¿entrada DI < entrada SI? (carpetas primero, luego nombre) -> CF
ent_less:
        push cx
        push si
        push di
        mov al, [di+13]
        and al, 0x10
        mov ah, [si+13]
        and ah, 0x10
        cmp al, ah
        je .name
        ; la carpeta va antes
        cmp al, ah
        ja .yes
        jmp .no
.name:
        mov cx, 13
.c:
        mov al, [di]
        mov ah, [si]
        cmp al, ah
        jb .yes
        ja .no
        test al, al
        jz .no
        inc si
        inc di
        loop .c
.no:
        pop di
        pop si
        pop cx
        clc
        ret
.yes:
        pop di
        pop si
        pop cx
        stc
        ret
ent_swap:
        push cx
        mov cx, ENT_LEN
.l:
        mov al, [si]
        xchg al, [di]
        mov [si], al
        inc si
        inc di
        loop .l
        pop cx
        ret
; SI = entrada -> EAX = extensión en mayúsculas ('COM')
ext_of:
        push si
        xor eax, eax
.f:
        mov bl, [si]
        test bl, bl
        jz .r
        inc si
        cmp bl, '.'
        jne .f
        mov eax, [si]
        and eax, 0x00FFFFFF
.r:
        pop si
        ret

; ---- dibujo ----
draw:
        ; título
        mov dh, 0
        mov dl, 0
        mov ah, A_BAR
        mov si, msg_title
        call vputs
        call vfill
        ; ruta
        mov dh, 1
        mov dl, 0
        mov ah, A_NORM
        mov si, msg_dir
        call vputs
        mov si, path
        call vputs
        call vfill
        mov dh, 2
        mov dl, 0
        mov ah, A_NORM
        call vfill
        ; ajustar la primera fila visible
        mov ax, [sel]
        cmp ax, [first]
        jae .a
        mov [first], ax
.a:
        mov bx, [first]
        add bx, LIST_ROWS
        cmp ax, bx
        jb .b
        sub ax, LIST_ROWS - 1
        mov [first], ax
.b:
        ; lista
        mov cx, 0
.row:
        cmp cx, LIST_ROWS
        jae .panel
        mov dh, cl
        add dh, LIST_TOP
        mov dl, 0
        mov ax, [first]
        add ax, cx
        cmp ax, [nent]
        jae .blank
        push cx
        mov bx, ax
        call ent_ptr
        mov ah, A_NORM
        test byte [si+13], 0x10
        jz .nd
        mov ah, A_DIR
.nd:
        cmp bx, [sel]
        jne .ns
        mov ah, A_SEL
.ns:
        mov al, ' '
        call vput
        inc dl
        push si
        call vputs                  ; nombre
        pop si
.pad:
        cmp dl, 16
        jae .sz
        mov al, ' '
        call vput
        inc dl
        jmp .pad
.sz:
        test byte [si+13], 0x10
        jz .file
        push si
        mov si, msg_dirtag
        call vputs
        pop si
        jmp .dt
.file:
        push ax
        mov eax, [si+14]
        mov [numv], eax
        pop ax
        mov byte [numw], 12
        call vnum
.dt:
        mov al, ' '
        call vput
        inc dl
        call vput
        inc dl
        mov bx, [si+18]             ; fecha
        push bx
        and bx, 31
        call v2
        mov al, '-'
        call vput
        inc dl
        pop bx
        push bx
        shr bx, 5
        and bx, 15
        call v2
        mov al, '-'
        call vput
        inc dl
        pop bx
        shr bx, 9
        add bx, 80
        cmp bx, 100
        jb .yy
        sub bx, 100
.yy:
        call v2
.fill:
        mov al, ' '
        cmp dl, LIST_W
        jae .rowd
        call vput
        inc dl
        jmp .fill
.rowd:
        pop cx
        inc cx
        jmp .row
.blank:
        mov ah, A_NORM
.bl:
        mov al, ' '
        cmp dl, LIST_W
        jae .bld
        call vput
        inc dl
        jmp .bl
.bld:
        inc cx
        jmp .row
.panel:
        ; panel derecho: información del elemento seleccionado
        mov dh, LIST_TOP
.pc:
        cmp dh, LIST_TOP + LIST_ROWS
        jae .pinfo
        mov dl, LIST_W
        mov ah, A_PANEL
        mov al, 0xB3
        call vput
        inc dl
        call vfill
        inc dh
        jmp .pc
.pinfo:
        mov ah, A_PANEL
        mov dh, LIST_TOP
        mov dl, LIST_W + 2
        mov si, msg_items
        mov ah, A_PANEL
        call vputs
        mov eax, 0
        mov ax, [nent]
        mov [numv], eax
        mov byte [numw], 0
        mov ah, A_PANEL
        call vnum
        mov dh, LIST_TOP + 1
        mov dl, LIST_W + 2
        mov si, msg_free
        mov ah, A_PANEL
        call vputs
        mov eax, [freeb]
        shr eax, 10
        mov [numv], eax
        mov byte [numw], 0
        mov ah, A_PANEL
        call vnum
        mov si, msg_kb
        mov ah, A_PANEL
        call vputs
        cmp word [nent], 0
        je .keys
        mov ax, [sel]
        call ent_ptr
        mov dh, LIST_TOP + 3
        mov dl, LIST_W + 2
        push si
        mov si, msg_sel
        mov ah, A_PANEL
        call vputs
        pop si
        mov dh, LIST_TOP + 4
        mov dl, LIST_W + 2
        push si
        mov ah, A_PANEL
        call vputs
        pop si
        mov dh, LIST_TOP + 5
        mov dl, LIST_W + 2
        test byte [si+13], 0x10
        jz .pf
        push si
        mov si, msg_isdir
        mov ah, A_PANEL
        call vputs
        pop si
        jmp .pa
.pf:
        push si
        mov eax, [si+14]
        mov [numv], eax
        mov byte [numw], 0
        mov ah, A_PANEL
        call vnum
        mov si, msg_bytes
        mov ah, A_PANEL
        call vputs
        pop si
.pa:
        mov dh, LIST_TOP + 6
        mov dl, LIST_W + 2
        mov bl, [si+13]
        mov si, msg_attr
        mov ah, A_PANEL
        call vputs
        mov al, '-'
        test bl, 1
        jz .a1
        mov al, 'L'                 ; solo lectura
.a1:
        mov ah, A_PANEL
        call vput
        inc dl
        mov al, '-'
        test bl, 2
        jz .a2
        mov al, 'O'                 ; oculto
.a2:
        mov ah, A_PANEL
        call vput
        inc dl
        mov al, '-'
        test bl, 4
        jz .a3
        mov al, 'S'                 ; sistema
.a3:
        mov ah, A_PANEL
        call vput
        inc dl
        mov al, '-'
        test bl, 0x20
        jz .a4
        mov al, 'A'                 ; archivo
.a4:
        mov ah, A_PANEL
        call vput
.keys:
        mov dh, 23
        mov dl, 0
        mov ah, A_BAR
        mov si, msg_keys1
        call vputs
        call vfill
        mov dh, 24
        mov dl, 0
        mov si, msg_keys2
        call vputs
        call vfill
        ret

; BX (0-99) con dos cifras en DH,DL (atributo [cur_attr])
v2:
        push ax
        push bx
        mov ax, bx
        mov bl, 10
        div bl
        push ax
        add al, '0'
        mov ah, [cur_attr]
        call vput
        inc dl
        pop ax
        mov al, ah
        add al, '0'
        mov ah, [cur_attr]
        call vput
        inc dl
        pop bx
        pop ax
        ret

; escribir AL (atributo AH) en DH,DL
vput:
        push di
        push ax
        mov [cur_attr], ah
        movzx di, dh
        imul di, di, 160
        push ax
        movzx ax, dl
        shl ax, 1
        add di, ax
        pop ax
        mov [gs:di], ax
        pop ax
        pop di
        ret
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
vfill:
        push ax
        mov al, ' '
.l:
        cmp dl, 80
        jae .r
        call vput
        inc dl
        jmp .l
.r:
        pop ax
        ret
; número [numv] con separador de miles, alineado a [numw] columnas
; (DH, DL) = posición; deja DL tras el número
vnum:
        push eax
        push ebx
        push ecx
        push si
        mov [vn_dx], dx             ; DIV usa EDX
        mov [cur_attr], ah
        mov eax, [numv]
        mov si, nbuf + 15
        mov byte [si], 0
        mov ebx, 10
        xor cl, cl
.l:
        xor edx, edx
        div ebx
        cmp cl, 3
        jne .nc
        dec si
        mov byte [si], '.'
        xor cl, cl
.nc:
        add dl, '0'
        dec si
        mov [si], dl
        inc cl
        test eax, eax
        jnz .l
        mov dx, [vn_dx]
        mov ax, nbuf + 15
        sub ax, si
        movzx cx, byte [numw]
        sub cx, ax
        jle .p
        mov ah, [cur_attr]
.pad:
        mov al, ' '
        call vput
        inc dl
        loop .pad
.p:
        mov ah, [cur_attr]
        call vputs
        pop si
        pop ecx
        pop ebx
        pop eax
        ret

; ---- cadenas ----
cat:
        lodsb
        test al, al
        jz .r
        stosb
        jmp cat
.r:
        ret
; nombre de la entrada SI en DI
cat_name:
        jmp cat

find_comspec:
        push es
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .def
        cmp dword [es:di], 'COMS'
        jne .s
        cmp dword [es:di+4], 'PEC='
        jne .s
        add di, 8
        mov si, comspec
.c:
        mov al, [es:di]
        mov [si], al
        inc di
        inc si
        test al, al
        jnz .c
        pop es
        ret
.s:
        inc di
        cmp byte [es:di-1], 0
        jne .s
        jmp .l
.def:
        pop es
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
crlf:
        mov si, s_crlf
        jmp puts

comspec     db "C:\COMMAND.COM", 0
            times 50 db 0
s_slashc    db " /C ", 0
s_all       db "*.*", 0
s_dotdot    db "..", 0
s_edit      db "EDITAR ", 0
s_copy      db "COPY ", 0
s_ren       db "REN ", 0
s_md        db "MD ", 0
s_del       db "DEL ", 0
s_rd        db "DELTREE /Y ", 0
s_crlf      db 13, 10, 0
msg_title   db " Hispa-DOS - Gestor de archivos", 0
msg_dir     db " Carpeta: ", 0
msg_dirtag  db "     <DIR>  ", 0
msg_items   db "Elementos: ", 0
msg_free    db "Libre: ", 0
msg_kb      db " KB", 0
msg_sel     db "Seleccionado:", 0
msg_isdir   db "Carpeta", 0
msg_bytes   db " bytes", 0
msg_attr    db "Atributos: ", 0
msg_keys1   db " Intro Abrir/Ejecutar  Retroceso Subir  Letra Ir a  F9 Unidad  Esc Salir", 0
msg_keys2   db " F4 Editar  F5 Copiar  F6 Renombrar/Mover  F7 Crear carpeta  F8 Borrar", 0
msg_key     db 13, 10, "Pulse una tecla para volver al gestor...", 0
ask_copy    db " Copiar en (carpeta o nombre): ", 0
ask_ren     db " Nuevo nombre: ", 0
ask_md      db " Nombre de la carpeta nueva: ", 0
ask_del     db " ¿Borrar el elemento seleccionado? (S/N) ", 0
ask_deltree db " ¿Borrar la carpeta y TODO su contenido? (S/N) ", 0
ask_drv     db " Unidad (letra): ", 0

        section .bss
nent        resw 1
sel         resw 1
first       resw 1
op          resw 1
drive       resb 1
quiet       resb 1
ask_col     resb 1
cur_attr    resb 1
numw        resb 1
            resb 1
numv        resd 1
vn_dx       resw 1
freeb       resd 1
path        resb 72
nbuf        resb 16
inbuf       resb 64
cmdline     resb 160
tail        resb 160
pblock      resb 14
dta         resb 128
entries     resb MAXENT*ENT_LEN
            resb 1024
stack_top:
prog_end:
