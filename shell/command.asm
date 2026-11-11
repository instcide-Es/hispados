; =============================================================================
;  HispaDOS COMMAND.COM - intérprete de órdenes
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;  Usa solo la API pública de INT 21h, así que también funciona sobre otros DOS.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, (stack_top - bss_start) / 2
        xor ax, ax
        rep stosw
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A                ; reducir nuestro bloque y dejar memoria
        int 0x21
        mov si, msg_hello
        call puts

main_loop:
        call prompt
        mov dx, inbuf
        mov byte [inbuf], 127
        mov ah, 0x0A
        int 0x21
        mov dl, 10
        mov ah, 2
        int 0x21
        movzx bx, byte [inbuf+1]
        mov byte [inbuf+2+bx], 0
        mov si, inbuf+2
        call execute_line
        jmp main_loop

; -----------------------------------------------------------------------------
prompt:
        call put_drive
        mov si, curdir
        mov dl, 0
        mov ah, 0x47
        int 0x21
        call puts
        mov dl, '>'
        mov ah, 2
        int 0x21
        ret

; DS:SI = línea
execute_line:
        call skip_sp
        cmp byte [si], 0
        je .r
        cmp byte [si], '@'
        jne .w
        inc si
        call skip_sp
.w:
        ; palabra de orden -> cmdword (mayúsculas)
        mov di, cmdword
        mov cx, 63
.cw:
        mov al, [si]
        test al, al
        jz .curdir
        cmp al, ' '
        je .curdir
        cmp al, 9
        je .curdir
        cmp al, '/'
        je .curdir
        cmp al, '.'
        jne .cw1
        ; "ECHO." / "CD.." : cortar si lo anterior es una orden interna
        push si
        push di
        mov byte [di], 0
        call find_internal
        pop di
        pop si
        jnc .curdir
        mov al, [si]
.cw1:
        call upcase
        mov [di], al
        inc di
        inc si
        loop .cw
.curdir:
        mov byte [di], 0
        mov [argp], si
        ; "A:" cambiar de unidad
        cmp byte [cmdword+1], ':'
        jne .ni
        cmp byte [cmdword+2], 0
        jne .ni
        mov dl, [cmdword]
        sub dl, 'A'
        mov ah, 0x0E
        int 0x21
        mov ah, 0x19
        int 0x21
        cmp al, dl
        je .r
        mov si, msg_baddrv
        jmp puts
.ni:
        call find_internal
        jc .ext
        mov si, [argp]
        call skip_sp
        jmp bx
.ext:
        jmp run_external
.r:
        ret

; cmdword -> BX = rutina; CF=1 si no es interna
find_internal:
        mov bx, cmd_table
.l:
        mov si, [bx]
        test si, si
        jz .no
        mov di, cmdword
.c:
        mov al, [si]
        cmp al, [di]
        jne .n
        test al, al
        jz .yes
        inc si
        inc di
        jmp .c
.n:
        add bx, 4
        jmp .l
.yes:
        mov bx, [bx+2]
        clc
        ret
.no:
        stc
        ret

cmd_table:
        dw n_ver, do_ver
        dw n_cls, do_cls
        dw n_dir, do_dir
        dw n_type, do_type
        dw n_cd, do_cd
        dw n_chdir, do_cd
        dw n_echo, do_echo
        dw n_set, do_set
        dw n_mem, do_mem
        dw n_date, do_date
        dw n_time, do_time
        dw n_vol, do_vol
        dw n_help, do_help
        dw n_rem, do_rem
        dw n_exit, do_exit
        dw 0, 0

n_ver   db "VER", 0
n_cls   db "CLS", 0
n_dir   db "DIR", 0
n_type  db "TYPE", 0
n_cd    db "CD", 0
n_chdir db "CHDIR", 0
n_echo  db "ECHO", 0
n_set   db "SET", 0
n_mem   db "MEM", 0
n_date  db "DATE", 0
n_time  db "TIME", 0
n_vol   db "VOL", 0
n_help  db "HELP", 0
n_rem   db "REM", 0
n_exit  db "EXIT", 0

; -----------------------------------------------------------------------------
;  Órdenes internas (SI = argumentos)
; -----------------------------------------------------------------------------
do_rem:
        ret

do_exit:
        mov ax, 0x4C00
        int 0x21

do_ver:
        mov si, msg_ver
        call puts
        mov ah, 0x30
        int 0x21
        push ax
        movzx eax, al
        call putnum
        mov dl, '.'
        mov ah, 2
        int 0x21
        pop ax
        movzx eax, ah
        cmp al, 10
        jae .two
        push ax
        mov dl, '0'
        mov ah, 2
        int 0x21
        pop ax
.two:
        call putnum
        jmp crlf

do_cls:
        mov ax, 0x0600
        mov bh, 0x07
        xor cx, cx
        mov dx, 0x184F
        int 0x10
        mov ah, 2
        xor bh, bh
        xor dx, dx
        int 0x10
        ret

do_echo:
        cmp byte [si], '.'
        jne .n
        inc si
.n:
        call puts
        jmp crlf

do_set:
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .d
.c:
        mov dl, [es:di]
        inc di
        test dl, dl
        jz .e
        mov ah, 2
        int 0x21
        jmp .c
.e:
        call crlf
        jmp .l
.d:
        push ds
        pop es
        ret

do_cd:
        cmp byte [si], 0
        jne .chg
        call put_drive
        mov si, curdir
        mov dl, 0
        mov ah, 0x47
        int 0x21
        call puts
        jmp crlf
.chg:
        call cut_arg
        mov dx, si
        mov ah, 0x3B
        int 0x21
        jnc .r
        mov si, msg_baddir
        call puts
.r:
        ret

do_type:
        cmp byte [si], 0
        jne .go
        mov si, msg_reqparam
        jmp puts
.go:
        call cut_arg
        mov dx, si
        mov ax, 0x3D00
        int 0x21
        jc .nf
        mov bx, ax
.l:
        mov dx, iobuf
        mov cx, 512
        mov ah, 0x3F
        int 0x21
        jc .cl
        test ax, ax
        jz .cl
        mov cx, ax
        push bx
        mov bx, 1
        mov ah, 0x40
        int 0x21
        pop bx
        jmp .l
.cl:
        mov ah, 0x3E
        int 0x21
        ret
.nf:
        mov si, msg_nofile
        jmp puts

do_vol:
        call arg_drive
        call crlf
        call print_label
        jmp crlf

; SI = argumento -> AL = letra de unidad ("X:" o la actual)
arg_drive:
        cmp byte [si], 0
        je .cur
        cmp byte [si+1], ':'
        jne .cur
        mov al, [si]
        call upcase
        ret
.cur:
        mov ah, 0x19
        int 0x21
        add al, 'A'
        ret

; escribir "X:\" de la unidad actual
put_drive:
        push ax
        mov ah, 0x19
        int 0x21
        add al, 'A'
        call putc
        mov al, ':'
        call putc
        mov al, '\'
        call putc
        pop ax
        ret

; AL = letra de la unidad
print_label:
        mov [s_lab], al
        mov [msg_vol1+17], al
        mov [msg_vol0+17], al
        call set_dta
        mov dx, s_lab
        mov cx, 8
        mov ah, 0x4E
        int 0x21
        jc .none
        mov si, msg_vol1
        call puts
        mov si, dta+0x1E
        mov di, lbl
.c:
        lodsb
        cmp al, '.'
        je .c
        mov [di], al
        inc di
        test al, al
        jnz .c
        mov si, lbl
        call puts
        jmp crlf
.none:
        mov si, msg_vol0
        call puts
        jmp crlf

set_dta:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        ret

do_dir:
        call cut_arg
        ; construir el patrón
        mov di, pattern
        cmp byte [si], 0
        jne .copy
        mov si, s_all
.copy:
        lodsb
        stosb
        test al, al
        jnz .copy
        dec di
        mov [pat_end], di
        mov al, [di-1]
        cmp al, '\'
        je .addall
        cmp al, ':'
        je .addall
        ; ¿es un directorio sin comodines?
        mov si, pattern
.wc:
        lodsb
        test al, al
        jz .nowc
        cmp al, '*'
        je .ready
        cmp al, '?'
        je .ready
        jmp .wc
.nowc:
        call set_dta
        mov dx, pattern
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .ready
        test byte [dta+0x15], 0x10
        jz .ready
.addall:
        mov di, [pat_end]
        cmp byte [di-1], '\'
        je .a2
        cmp byte [di-1], ':'
        je .a2
        mov al, '\'
        stosb
.a2:
        mov si, s_all
.a3:
        lodsb
        stosb
        test al, al
        jnz .a3
.ready:
        mov si, pattern
        mov di, truebuf
        mov ah, 0x60
        int 0x21
        jnc .tn
        mov si, msg_baddrv
        jmp puts
.tn:
        call crlf
        mov al, [truebuf]
        call print_label
        mov si, truebuf
        mov di, si
.ls:
        lodsb
        test al, al
        jz .lse
        cmp al, '\'
        jne .ls
        mov di, si
        jmp .ls
.lse:
        cmp di, truebuf+3
        jbe .root
        dec di
.root:
        mov byte [di], 0
        mov si, msg_dirof
        call puts
        mov si, truebuf
        call puts
        call crlf
        call crlf
        xor eax, eax
        mov [nfiles], eax
        mov [nbytes], eax
        call set_dta
        mov dx, pattern
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jc .empty
.entry:
        call dir_line
        mov ah, 0x4F
        int 0x21
        jnc .entry
        ; resumen
        mov eax, [nfiles]
        mov cl, 9
        call putnum_w
        mov si, msg_files
        call puts
        mov eax, [nbytes]
        mov cl, 15
        call putnum_w
        mov si, msg_bytes
        call puts
        mov ah, 0x36
        mov dl, [truebuf]
        sub dl, 'A' - 1
        int 0x21
        movzx eax, ax
        movzx ebx, bx
        movzx ecx, cx
        mul ebx
        mul ecx
        mov cl, 25
        call putnum_w
        mov si, msg_free
        jmp puts
.empty:
        mov si, msg_nofile
        jmp puts

dir_line:
        inc dword [nfiles]
        mov di, nm11
        mov cx, 11
        mov al, ' '
        rep stosb
        mov si, dta+0x1E
        mov di, nm11
        cmp byte [si], '.'
        je .dots
.n:
        lodsb
        test al, al
        jz .out
        cmp al, '.'
        je .x
        stosb
        jmp .n
.x:
        mov di, nm11+8
.e:
        lodsb
        test al, al
        jz .out
        stosb
        jmp .e
.dots:
        lodsb
        test al, al
        jz .out
        stosb
        jmp .dots
.out:
        mov si, nm11
        mov cx, 8
.p1:
        lodsb
        call putc
        loop .p1
        mov al, ' '
        call putc
        mov cx, 3
.p2:
        lodsb
        call putc
        loop .p2
.size:
        test byte [dta+0x15], 0x10
        jz .file
        mov si, s_dirtag
        call puts
        jmp .date
.file:
        mov eax, [dta+0x1A]
        add [nbytes], eax
        mov cl, 14
        call putnum_w
.date:
        mov al, ' '
        call putc
        mov ax, [dta+0x18]
        mov bx, ax
        shr ax, 5
        and ax, 15
        call put2
        mov al, '-'
        call putc
        mov ax, bx
        and ax, 31
        call put2
        mov al, '-'
        call putc
        mov ax, bx
        shr ax, 9
        add ax, 80
        xor dx, dx
        mov cx, 100
        div cx
        mov ax, dx
        call put2
        ; hora 12 h con a/p
        mov ax, [dta+0x16]
        shr ax, 11
        mov bl, 'a'
        cmp ax, 12
        jb .am
        mov bl, 'p'
        sub ax, 12
.am:
        test ax, ax
        jnz .h
        mov ax, 12
.h:
        movzx eax, ax
        mov cl, 4
        call putnum_w
        mov al, ':'
        call putc
        mov ax, [dta+0x16]
        shr ax, 5
        and ax, 63
        call put2
        mov al, bl
        call putc
        jmp crlf

do_mem:
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        push ds
        pop es
        xor ebx, ebx            ; libre total
        xor edx, edx            ; mayor libre
        xor edi, edi            ; fin
.l:
        push ds
        mov ds, ax
        mov cl, [0]
        movzx esi, word [3]
        cmp word [1], 0
        pop ds
        jne .used
        mov ebp, esi
        shl ebp, 4
        add ebx, ebp
        cmp ebp, edx
        jbe .used
        mov edx, ebp
.used:
        movzx edi, ax
        add edi, esi
        inc edi
        cmp cl, 'Z'
        je .done
        cmp cl, 'M'
        jne .done
        add ax, si
        inc ax
        jmp .l
.done:
        push edx
        push ebx
        shl edi, 4
        call crlf
        mov eax, edi
        mov cl, 12
        call putnum_w
        mov si, msg_mtotal
        call puts
        pop eax
        mov cl, 12
        call putnum_w
        mov si, msg_mfree
        call puts
        pop eax
        mov cl, 12
        call putnum_w
        mov si, msg_mlarge
        jmp puts

do_date:
        mov ah, 0x2A
        int 0x21
        push cx
        push dx
        mov si, msg_date
        call puts
        movzx bx, al
        shl bx, 2
        lea si, [days+bx]
        call puts
        mov al, ' '
        call putc
        pop dx
        push dx
        movzx ax, dl
        call put2
        mov al, '-'
        call putc
        pop dx
        movzx ax, dh
        call put2
        mov al, '-'
        call putc
        pop cx
        movzx eax, cx
        call putnum
        jmp crlf

do_time:
        mov ah, 0x2C
        int 0x21
        push dx
        push cx
        mov si, msg_time
        call puts
        pop cx
        push cx
        movzx eax, ch
        mov cl, 2
        call putnum_w
        mov al, ':'
        call putc
        pop cx
        movzx ax, cl
        call put2
        mov al, ':'
        call putc
        pop dx
        movzx ax, dh
        call put2
        jmp crlf

do_help:
        mov si, msg_help
        jmp puts

; -----------------------------------------------------------------------------
;  Programas externos (.COM / .EXE) con búsqueda en PATH
; -----------------------------------------------------------------------------
run_external:
        ; ¿lleva extensión o ruta?
        mov byte [hasext], 0
        mov byte [haspath], 0
        mov si, cmdword
.sc:
        lodsb
        test al, al
        jz .scd
        cmp al, '.'
        jne .s1
        mov byte [hasext], 1
.s1:
        cmp al, '\'
        je .hp
        cmp al, ':'
        jne .sc
.hp:
        mov byte [haspath], 1
        mov byte [hasext], 0
        jmp .sc
.scd:
        ; la extensión solo cuenta en el último componente
        mov si, cmdword
        mov di, si
.lc:
        lodsb
        test al, al
        jz .lcd
        cmp al, '\'
        je .lcs
        cmp al, ':'
        jne .lc
.lcs:
        mov di, si
        jmp .lc
.lcd:
        mov byte [hasext], 0
.le:
        mov al, [di]
        test al, al
        jz .lee
        inc di
        cmp al, '.'
        jne .le
        mov byte [hasext], 1
.lee:
        ; cola de órdenes: argumentos tal cual (con el espacio inicial)
        mov si, [argp]
        mov di, tail+1
        xor cx, cx
.t:
        lodsb
        test al, al
        jz .td
        cmp cx, 126
        jae .t
        stosb
        inc cx
        jmp .t
.td:
        mov [tail], cl
        mov byte [di], 13
        ; FCB 1 y 2
        mov si, [argp]
        mov di, fcb1
        mov ax, 0x2901
        int 0x21
        mov di, fcb2
        mov ax, 0x2901
        int 0x21
        ; 1) directorio actual / ruta dada
        mov word [pathptr], 0
        mov byte [prefix], 0
        call try_dir
        jnc .done
        cmp byte [haspath], 0
        jne .bad
        ; 2) cada entrada de PATH
        call find_path_var
        jc .bad
        mov [pathptr], si
.next:
        mov si, [pathptr]
        mov es, [0x2C]
        cmp byte [es:si], 0
        je .badr
        mov di, prefix
.pc:
        mov al, [es:si]
        test al, al
        jz .pce
        inc si
        cmp al, ';'
        je .pce2
        mov [di], al
        inc di
        jmp .pc
.pce:
.pce2:
        mov [pathptr], si
        push ds
        pop es
        cmp di, prefix
        je .next
        cmp byte [di-1], '\'
        je .pz
        mov byte [di], '\'
        inc di
.pz:
        mov byte [di], 0
        call try_dir
        jnc .done
        jmp .next
.badr:
        push ds
        pop es
.bad:
        mov si, msg_badcmd
        call puts
.done:
        ret

; prefix + cmdword (+ .COM/.EXE) -> ejecutar. CF=1 si no existe
try_dir:
        cmp byte [hasext], 0
        je .try2
        xor bx, bx
        jmp try_one
.try2:
        mov bx, s_com
        call try_one
        jnc .r
        mov bx, s_exe
        call try_one
.r:
        ret

; BX = extensión (0 = ninguna)
try_one:
        mov di, progpath
        mov si, prefix
.a:
        lodsb
        test al, al
        jz .b
        stosb
        jmp .a
.b:
        mov si, cmdword
.c:
        lodsb
        test al, al
        jz .d
        stosb
        jmp .c
.d:
        test bx, bx
        jz .e
        mov si, bx
.f:
        lodsb
        stosb
        test al, al
        jnz .f
        jmp .g
.e:
        stosb
.g:
        ; ¿existe?
        call set_dta
        mov dx, progpath
        mov cx, 0x06
        mov ah, 0x4E
        int 0x21
        jc .no
        mov word [pblock], 0
        mov word [pblock+2], tail
        mov [pblock+4], ds
        mov word [pblock+6], fcb1
        mov [pblock+8], ds
        mov word [pblock+10], fcb2
        mov [pblock+12], ds
        mov dx, progpath
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        jnc .ok
        mov si, msg_execerr
        call puts
.ok:
        clc
        ret
.no:
        stc
        ret

; -> SI = valor de PATH en el entorno (offset), CF si no hay
find_path_var:
        mov es, [0x2C]
        xor si, si
.l:
        cmp byte [es:si], 0
        je .no
        cmp dword [es:si], 'PATH'
        jne .s
        cmp byte [es:si+4], '='
        jne .s
        add si, 5
        push ds
        pop es
        clc
        ret
.s:
        inc si
        cmp byte [es:si-1], 0
        jne .s
        jmp .l
.no:
        push ds
        pop es
        stc
        ret

; -----------------------------------------------------------------------------
;  Utilidades
; -----------------------------------------------------------------------------
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

; termina el argumento en el primer espacio
cut_arg:
        push si
.l:
        mov al, [si]
        test al, al
        jz .d
        cmp al, ' '
        je .z
        inc si
        jmp .l
.z:
        mov byte [si], 0
.d:
        pop si
        ret

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret

puts:
        push ax
        push dx
.l:
        lodsb
        test al, al
        jz .d
        mov dl, al
        mov ah, 2
        int 0x21
        jmp .l
.d:
        pop dx
        pop ax
        ret

crlf:
        push si
        mov si, s_crlf
        call puts
        pop si
        ret

; AX (0..99) con dos cifras
put2:
        push ax
        push dx
        push cx
        xor dx, dx
        mov cx, 10
        div cx
        add al, '0'
        call putc
        mov al, dl
        add al, '0'
        call putc
        pop cx
        pop dx
        pop ax
        ret

; EAX sin relleno
putnum:
        mov cl, 0
; EAX con separador de miles, alineado a la derecha en CL columnas
putnum_w:
        push eax
        push ebx
        push ecx
        push edx
        push si
        push di
        mov di, numbuf+15
        mov byte [di], 0
        mov ebx, 10
        xor ch, ch
.l:
        xor edx, edx
        div ebx
        add dl, '0'
        cmp ch, 3
        jne .nc
        dec di
        mov byte [di], ','
        mov ch, 0
.nc:
        dec di
        mov [di], dl
        inc ch
        test eax, eax
        jnz .l
        ; relleno
        mov ax, numbuf+15
        sub ax, di
        movzx cx, cl
        sub cx, ax
        jle .p
.pad:
        mov al, ' '
        call putc
        loop .pad
.p:
        mov si, di
        call puts
        pop di
        pop si
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; -----------------------------------------------------------------------------
msg_hello   db 13, 10, "HispaDOS Command Interpreter 0.2", 13, 10
            db "Type HELP for a list of commands.", 13, 10, 13, 10, 0
msg_ver     db 13, 10, "HispaDOS Version 0.2 (MS-DOS 6.22 compatible)", 13, 10
            db "Reported DOS version: ", 0
msg_badcmd  db "Bad command or file name", 13, 10, 0
msg_execerr db "Cannot execute program", 13, 10, 0
msg_baddir  db "Invalid directory", 13, 10, 0
msg_baddrv  db "Invalid drive specification", 13, 10, 0
msg_nofile  db "File not found", 13, 10, 0
msg_reqparam db "Required parameter missing", 13, 10, 0
msg_vol1    db " Volume in drive A is ", 0
msg_vol0    db " Volume in drive A has no label", 0
msg_dirof   db " Directory of ", 0
msg_files   db " file(s)", 0
msg_bytes   db " bytes", 13, 10, 0
msg_free    db " bytes free", 13, 10, 0
msg_mtotal  db " bytes total conventional memory", 13, 10, 0
msg_mfree   db " bytes free", 13, 10, 0
msg_mlarge  db " bytes largest executable program size", 13, 10, 0
msg_date    db "Current date is ", 0
msg_time    db "Current time is ", 0
msg_help    db "Internal commands:", 13, 10
            db "  CD/CHDIR  CLS  DATE  DIR  ECHO  EXIT  HELP  MEM", 13, 10
            db "  REM  SET  TIME  TYPE  VER  VOL", 13, 10
            db "Programs (.COM, .EXE) are searched in the current", 13, 10
            db "directory and then in PATH.", 13, 10, 0
days        db "Sun", 0, "Mon", 0, "Tue", 0, "Wed", 0, "Thu", 0, "Fri", 0, "Sat", 0
s_drive     db "A:\", 0
s_all       db "*.*", 0
s_lab       db "A:\*.*", 0
s_com       db ".COM", 0
s_exe       db ".EXE", 0
s_dirtag    db "    <DIR>     ", 0
s_crlf      db 13, 10, 0

prog_end_data:
        section .bss
bss_start:
inbuf       resb 130
cmdword     resb 64
argp        resw 1
curdir         resb 68
dta         resb 128
lbl         resb 14
pattern     resb 132
pat_end     resw 1
truebuf     resb 132
nfiles      resd 1
nbytes      resd 1
numbuf      resb 16
nm11        resb 12
iobuf       resb 512
hasext      resb 1
haspath     resb 1
pathptr     resw 1
prefix      resb 80
progpath    resb 160
tail        resb 130
fcb1        resb 37
fcb2        resb 37
pblock      resb 14
            resb 2048
stack_top:
prog_end:
