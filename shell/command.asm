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
        mov word [pl_in], 0
        mov byte [pl_depth], 0
        call run_line
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
        dw n_copy, do_copy
        dw n_del, do_del
        dw n_erase, do_del
        dw n_ren, do_ren
        dw n_rename, do_ren
        dw n_md, do_md
        dw n_mkdir, do_md
        dw n_rd, do_rd
        dw n_rmdir, do_rd
        dw n_pause, do_pause
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
n_copy  db "COPY", 0
n_del   db "DEL", 0
n_erase db "ERASE", 0
n_ren   db "REN", 0
n_rename db "RENAME", 0
n_md    db "MD", 0
n_mkdir db "MKDIR", 0
n_rd    db "RD", 0
n_rmdir db "RMDIR", 0
n_pause db "PAUSE", 0

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
;  Tuberías ("|") y redirecciones ("<", ">", ">>")
; -----------------------------------------------------------------------------
; SI = línea; [pl_in] = fichero de entrada heredado de la tubería (0 = ninguno)
run_line:
        mov di, si
.f:
        mov al, [di]
        test al, al
        jz .simple
        cmp al, '|'
        je .pipe
        inc di
        jmp .f
.simple:
        mov word [pl_out], 0
        jmp run_simple
.pipe:
        mov byte [di], 0
        inc di
        push di
        ; nombre del fichero temporal según la profundidad
        movzx bx, byte [pl_depth]
        inc byte [pl_depth]
        imul bx, bx, 16
        add bx, pipe_names
        push bx
        mov ah, 0x19
        int 0x21
        add al, 'A'
        mov [bx], al
        mov [pl_out], bx
        call run_simple
        pop bx
        pop si
        push word [pl_in]
        mov [pl_in], bx
        push bx
        call run_line
        pop dx
        mov ah, 0x41
        int 0x21
        pop word [pl_in]
        dec byte [pl_depth]
        ret

; SI = orden sin tuberías
run_simple:
        mov word [r_in], 0
        mov word [r_out], 0
        mov byte [r_app], 0
        mov di, si
.scan:
        mov al, [di]
        test al, al
        jz .done
        cmp al, '<'
        je .in
        cmp al, '>'
        je .out
        inc di
        jmp .scan
.in:
        mov byte [di], ' '
        inc di
        mov bx, rin_name
        mov [r_in], bx
        call .name
        jmp .scan
.out:
        mov byte [di], ' '
        inc di
        cmp byte [di], '>'
        jne .o1
        mov byte [di], ' '
        inc di
        mov byte [r_app], 1
.o1:
        mov bx, rout_name
        mov [r_out], bx
        call .name
        jmp .scan
; copiar el nombre que sigue a DI en BX y borrarlo de la línea
.name:
        cmp byte [di], ' '
        jne .n1
        inc di
        jmp .name
.n1:
        mov al, [di]
        test al, al
        jz .ne
        cmp al, ' '
        je .ne
        cmp al, '<'
        je .ne
        cmp al, '>'
        je .ne
        mov [bx], al
        inc bx
        mov byte [di], ' '
        inc di
        jmp .n1
.ne:
        mov byte [bx], 0
        ret
.done:
        ; quitar los espacios que han dejado las redirecciones al final
.trim:
        cmp di, si
        jbe .trimd
        cmp byte [di-1], ' '
        jne .trimd
        dec di
        mov byte [di], 0
        jmp .trim
.trimd:
        ; la tubería manda si no hay redirección explícita
        cmp word [r_in], 0
        jne .ri
        mov ax, [pl_in]
        mov [r_in], ax
.ri:
        cmp word [r_out], 0
        jne .ro
        mov ax, [pl_out]
        mov [r_out], ax
.ro:
        push si
        call redir_setup
        pop si
        jc .bad
        call execute_line
        call redir_restore
        ret
.bad:
        call redir_restore
        mov si, msg_redir
        jmp puts

redir_setup:
        mov word [sv_in], 0xFFFF
        mov word [sv_out], 0xFFFF
        mov dx, [r_in]
        test dx, dx
        jz .noin
        mov ax, 0x3D00
        int 0x21
        jc .err
        mov bx, 0
        call swap_handle
        mov [sv_in], ax
.noin:
        mov dx, [r_out]
        test dx, dx
        jz .ok
        cmp byte [r_app], 0
        je .create
        mov ax, 0x3D01
        int 0x21
        jc .create
        push ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        pop ax
        jmp .gotout
.create:
        mov dx, [r_out]
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .err
.gotout:
        mov bx, 1
        call swap_handle
        mov [sv_out], ax
.ok:
        clc
        ret
.err:
        stc
        ret

; AX = handle nuevo, BX = handle estándar -> AX = copia del antiguo
swap_handle:
        push cx
        push dx
        mov dx, ax
        push bx
        mov ah, 0x45
        int 0x21
        pop bx
        push ax
        mov cx, bx
        mov bx, dx
        mov ah, 0x46
        int 0x21
        mov bx, dx
        mov ah, 0x3E
        int 0x21
        pop ax
        pop dx
        pop cx
        ret

redir_restore:
        mov bx, [sv_out]
        cmp bx, 0xFFFF
        je .i
        mov cx, 1
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov word [sv_out], 0xFFFF
.i:
        mov bx, [sv_in]
        cmp bx, 0xFFFF
        je .r
        xor cx, cx
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov word [sv_in], 0xFFFF
.r:
        ret

; -----------------------------------------------------------------------------
;  Utilidades de rutas
; -----------------------------------------------------------------------------
; SI -> palabra en DI (ASCIIZ), SI avanza; CF si no hay
get_tok:
        call skip_sp
        cmp byte [si], 0
        je .none
.l:
        lodsb
        test al, al
        jz .e0
        cmp al, ' '
        je .e
        cmp al, 9
        je .e
        stosb
        jmp .l
.e0:
        dec si
.e:
        mov byte [di], 0
        clc
        ret
.none:
        mov byte [di], 0
        stc
        ret

; SI = ruta -> CF=1 si tiene comodines
has_wild:
        push si
.l:
        lodsb
        test al, al
        jz .no
        cmp al, '*'
        je .yes
        cmp al, '?'
        je .yes
        jmp .l
.yes:
        pop si
        stc
        ret
.no:
        pop si
        clc
        ret

; SI = ruta -> DI = su parte de carpeta ("A:\DOCS\", "A:", "")
path_prefix:
        push si
        push di
        mov bx, di
.c:
        lodsb
        stosb
        test al, al
        jz .e
        cmp al, '\'
        je .m
        cmp al, ':'
        jne .c
.m:
        mov bx, di
        jmp .c
.e:
        mov byte [bx], 0
        pop di
        pop si
        ret

; DI = destino <- SI (prefijo) + BX (nombre)
join_path:
        push si
.a:
        lodsb
        test al, al
        jz .b
        stosb
        jmp .a
.b:
        mov si, bx
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop si
        ret

; SI = ruta: añadir "\*.*" (o "*.*" tras "\" o ":")
append_all:
        push si
.e:
        lodsb
        test al, al
        jnz .e
        dec si
        mov di, si
        mov al, [di-1]
        cmp al, '\'
        je .a
        cmp al, ':'
        je .a
        mov byte [di], '\'
        inc di
.a:
        mov si, s_all
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop si
        ret

; SI = ruta -> CF=0 si es un directorio existente
is_dir:
        push si
        push di
        mov di, t_buf
        mov ah, 0x60
        int 0x21
        jc .no
        mov si, t_buf
        call has_wild
        jc .no
.e:
        lodsb
        test al, al
        jnz .e
        cmp byte [si-2], '\'
        je .yes
        mov dx, dta2
        mov ah, 0x1A
        int 0x21
        mov dx, t_buf
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .no
        test byte [dta2+0x15], 0x10
        jz .no
.yes:
        pop di
        pop si
        clc
        ret
.no:
        pop di
        pop si
        stc
        ret

; SI, DI = rutas -> CF=1 (y aviso) si son el mismo fichero
same_file:
        push si
        push di
        push di
        mov di, t_buf
        mov ah, 0x60
        int 0x21
        pop si
        mov di, t_buf2
        mov ah, 0x60
        int 0x21
        mov si, t_buf
        mov di, t_buf2
.c:
        mov al, [si]
        cmp al, [di]
        jne .diff
        inc si
        inc di
        test al, al
        jnz .c
        mov si, msg_self
        call puts
        pop di
        pop si
        stc
        ret
.diff:
        pop di
        pop si
        clc
        ret

msg_required:
        mov si, msg_reqparam
        jmp puts

; -----------------------------------------------------------------------------
;  COPY origen [destino]
; -----------------------------------------------------------------------------
do_copy:
        mov di, c_src
        call get_tok
        jc msg_required
        mov byte [c_nodst], 0
        mov di, c_dst
        call get_tok
        jnc .hd
        mov word [c_dst], '.'
        mov byte [c_nodst], 1
.hd:
        mov dword [c_count], 0
        mov byte [c_open], 0
        ; ¿concatenación A+B[+C...]?
        mov si, c_src
.plus:
        lodsb
        test al, al
        jz .noplus
        cmp al, '+'
        jne .plus
        jmp do_concat
.noplus:
        ; ¿origen dispositivo CON?
        mov si, c_src
        mov ax, 0x3D00
        mov dx, si
        int 0x21
        jc .notdev
        mov bx, ax
        mov ax, 0x4400
        int 0x21
        push dx
        mov ah, 0x3E
        int 0x21
        pop dx
        test dl, 0x80
        jz .notdev
        jmp .device
.notdev:
        mov si, c_src
        call is_dir
        jc .nd
        mov si, c_src
        call append_all
.nd:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        mov byte [c_dstdir], 0
        mov si, c_dst
        call is_dir
        jc .nd2
        mov byte [c_dstdir], 1
.nd2:
        mov si, c_src
        call has_wild
        setc [c_wild]
        call set_dta
        mov dx, c_src
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jnc .loop
        mov si, msg_nofile
        call puts
        jmp .summary
.loop:
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        cmp byte [c_wild], 0
        je .nw
        mov si, c_spath
        call puts
        call crlf
.nw:
        cmp byte [c_dstdir], 0
        je .tofile
        ; destino carpeta: destino\nombre
        mov si, c_dst
        mov di, c_dpre
.dp:
        lodsb
        stosb
        test al, al
        jnz .dp
        dec di
        mov al, [di-1]
        cmp al, '\'
        je .dp2
        cmp al, ':'
        je .dp2
        mov byte [di], '\'
        inc di
        mov byte [di], 0
.dp2:
        mov si, c_dpre
        mov bx, dta+0x1E
        mov di, c_dpath
        call join_path
        mov si, c_spath
        mov di, c_dpath
        call same_file
        jc .next
        mov dx, c_dpath
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        call copy_one
        mov bx, [c_dh]
        mov ah, 0x3E
        int 0x21
        jmp .next
.tofile:
        cmp byte [c_open], 0
        jne .append
        mov si, c_spath
        mov di, c_dst
        call same_file
        jc .next
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        mov byte [c_open], 1
.append:
        call copy_one
.next:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .loop
        jmp .close
.cerr:
        mov si, msg_cantcreate
        call puts
.close:
        cmp byte [c_open], 0
        je .summary
        mov bx, [c_dh]
        mov ah, 0x3E
        int 0x21
.summary:
        mov eax, [c_count]
        mov cl, 9
        call putnum_w
        mov si, msg_copied
        jmp puts
.device:
        mov si, c_dst
        call is_dir
        jnc .devbad
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        mov byte [c_open], 1
        mov dx, c_src
        mov ax, 0x3D00
        int 0x21
        jc .close
        mov bx, ax
        mov byte [c_isdev], 1
        call copy_data
        mov byte [c_isdev], 0
        mov ah, 0x3E
        int 0x21
        inc dword [c_count]
        jmp .close
.devbad:
        mov si, msg_cantcreate
        call puts
        jmp .summary

; COPY A+B[+C] [destino]: unir ficheros (sin destino, se añade a A)
do_concat:
        mov si, c_src
        mov di, c_spath
        mov byte [c_first], 1
.part:
        ; copiar la parte siguiente en c_spath
        mov di, c_spath
.cp:
        lodsb
        cmp al, '+'
        je .end
        test al, al
        je .end
        stosb
        jmp .cp
.end:
        mov byte [di], 0
        mov [c_next], si
        mov [c_last], al
        cmp byte [c_first], 0
        je .add
        mov byte [c_first], 0
        cmp byte [c_nodst], 0
        je .newdst
        ; sin destino: abrir la primera parte y añadir al final
        mov dx, c_spath
        mov ax, 0x3D02
        int 0x21
        jc .err
        mov [c_dh], ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        mov byte [c_open], 1
        inc dword [c_count]
        jmp .more
.newdst:
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .err
        mov [c_dh], ax
        mov byte [c_open], 1
.add:
        call copy_one
.more:
        mov si, [c_next]
        cmp byte [c_last], 0
        jne .part
        mov bx, [c_dh]
        mov ah, 0x3E
        int 0x21
        mov eax, 1
        mov cl, 9
        call putnum_w
        mov si, msg_copied
        jmp puts
.err:
        mov si, msg_nofile
        jmp puts

; copiar c_spath en el handle c_dh (conserva fecha y hora si es copia simple)
copy_one:
        mov dx, c_spath
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        call copy_data
        jc .full
        mov ax, 0x5700
        int 0x21
        push cx
        push dx
        mov ah, 0x3E
        int 0x21
        pop dx
        pop cx
        mov bx, [c_dh]
        mov ax, 0x5701
        int 0x21
        inc dword [c_count]
.r:
        ret
.full:
        mov ah, 0x3E
        int 0x21
        mov si, msg_full
        jmp puts

; BX = origen -> c_dh; CF si el disco se llena
copy_data:
.l:
        mov dx, cbuf
        mov cx, CBUF_LEN
        mov ah, 0x3F
        int 0x21
        jc .ok
        test ax, ax
        jz .ok
        mov cx, ax
        cmp byte [c_isdev], 0
        je .w
        ; de un dispositivo: parar en Ctrl+Z
        push di
        mov di, cbuf
        push cx
        mov al, 0x1A
        repne scasb
        pop cx
        jne .noz
        mov cx, di
        sub cx, cbuf
        dec cx
        mov byte [c_eof], 1
.noz:
        pop di
.w:
        push bx
        push cx
        mov bx, [c_dh]
        mov dx, cbuf
        mov ah, 0x40
        int 0x21
        pop cx
        pop bx
        jc .err
        cmp ax, cx
        jb .err
        cmp byte [c_eof], 0
        je .l
        mov byte [c_eof], 0
.ok:
        clc
        ret
.err:
        stc
        ret

; -----------------------------------------------------------------------------
;  DEL / ERASE patrón
; -----------------------------------------------------------------------------
do_del:
        mov di, c_src
        call get_tok
        jc msg_required
        mov si, c_src
        call is_dir
        jc .nd
        mov si, c_src
        call append_all
.nd:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        ; ¿todo el directorio?
        mov si, c_src
.e:
        lodsb
        test al, al
        jnz .e
        cmp dword [si-4], '*.*'
        je .all
        cmp word [si-2], '*'
        jne .go
        cmp si, c_src+2
        je .all
        cmp byte [si-3], '\'
        je .all
        cmp byte [si-3], ':'
        je .all
        jmp .go
.all:
        mov si, msg_delall
        call puts
        call ask_yn
        jc .r
.go:
        call set_dta
        mov dx, c_src
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jnc .l
        mov si, msg_nofile
        jmp puts
.l:
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        mov dx, c_spath
        mov ah, 0x41
        int 0x21
        jnc .n
        mov si, msg_accden
        call puts
.n:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .l
.r:
        ret

; leer S/N -> CF=0 si sí
ask_yn:
        mov dx, ynbuf
        mov byte [ynbuf], 4
        mov ah, 0x0A
        int 0x21
        call crlf
        mov al, [ynbuf+2]
        call upcase
        cmp al, 'Y'
        je .y
        cmp al, 'S'
        je .y
        stc
        ret
.y:
        clc
        ret

; -----------------------------------------------------------------------------
;  REN / RENAME origen nombre (admite máscaras como *.BAK)
; -----------------------------------------------------------------------------
do_ren:
        mov di, c_src
        call get_tok
        jc msg_required
        mov di, c_dst
        call get_tok
        jc msg_required
        mov si, c_dst
.chk:
        lodsb
        test al, al
        jz .ok
        cmp al, '\'
        je .bad
        cmp al, ':'
        je .bad
        jmp .chk
.bad:
        mov si, msg_badparam
        jmp puts
.ok:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        mov si, c_dst
        mov di, m_mask
        call to11
        call set_dta
        mov dx, c_src
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jnc .l
        mov si, msg_dupnf
        jmp puts
.l:
        mov si, dta+0x1E
        mov di, m_name
        call to11
        ; aplicar la máscara
        mov si, m_mask
        mov di, m_name
        mov cx, 11
.m:
        mov al, [si]
        cmp al, '?'
        je .keep
        mov [di], al
.keep:
        inc si
        inc di
        loop .m
        mov si, m_name
        mov di, m_new
        call from11
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        mov si, c_pre
        mov bx, m_new
        mov di, c_dpath
        call join_path
        mov dx, c_spath
        mov di, c_dpath
        mov ah, 0x56
        int 0x21
        jnc .n
        mov si, msg_dupnf
        call puts
.n:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .l
        ret

; nombre "NOMBRE.EXT" o máscara en SI -> 11 caracteres en DI ('*' -> '?')
to11:
        push di
        push cx
        mov cx, 11
        mov al, ' '
        rep stosb
        pop cx
        pop di
        push di
        mov bx, di
        cmp byte [si], '.'
        je .dots
        mov cx, 8
.n:
        lodsb
        test al, al
        jz .d
        cmp al, '.'
        je .x
        call upcase
        cmp al, '*'
        jne .nc
.nf:
        jcxz .n
        mov byte [di], '?'
        inc di
        dec cx
        jmp .nf
.nc:
        jcxz .n
        stosb
        dec cx
        jmp .n
.x:
        lea di, [bx+8]
        mov cx, 3
.e:
        lodsb
        test al, al
        jz .d
        call upcase
        cmp al, '*'
        jne .ec
.ef:
        jcxz .e
        mov byte [di], '?'
        inc di
        dec cx
        jmp .ef
.ec:
        jcxz .e
        stosb
        dec cx
        jmp .e
.dots:
        lodsb
        test al, al
        jz .d
        stosb
        jmp .dots
.d:
        pop di
        ret

; 11 caracteres en SI -> "NOMBRE.EXT" en DI
from11:
        push si
        mov cx, 8
.n:
        lodsb
        cmp al, ' '
        je .s
        stosb
.s:
        loop .n
        cmp byte [si], ' '
        je .e
        mov al, '.'
        stosb
        mov cx, 3
.x:
        lodsb
        cmp al, ' '
        je .xs
        stosb
.xs:
        loop .x
.e:
        mov byte [di], 0
        pop si
        ret

; -----------------------------------------------------------------------------
do_md:
        mov di, c_src
        call get_tok
        jc msg_required
        mov dx, c_src
        mov ah, 0x39
        int 0x21
        jnc .r
        mov si, msg_mderr
        call puts
.r:
        ret

do_rd:
        mov di, c_src
        call get_tok
        jc msg_required
        mov dx, c_src
        mov ah, 0x3A
        int 0x21
        jnc .r
        mov si, msg_rderr
        call puts
.r:
        ret

do_pause:
        mov si, msg_pause
        call puts
        mov ah, 0x08
        int 0x21
        jmp crlf

msg_redir   db "File creation error", 13, 10, 0
msg_self    db "File cannot be copied onto itself", 13, 10, 0
msg_cantcreate db "Unable to create destination", 13, 10, 0
msg_copied  db " file(s) copied", 13, 10, 0
msg_full    db "Insufficient disk space", 13, 10, 0
msg_delall  db "All files in directory will be deleted!", 13, 10
            db "Are you sure (Y/N)?", 0
msg_accden  db "Access denied", 13, 10, 0
msg_badparam db "Invalid parameter", 13, 10, 0
msg_dupnf   db "Duplicate file name or file not found", 13, 10, 0
msg_mderr   db "Unable to create directory", 13, 10, 0
msg_rderr   db "Invalid path, not directory,", 13, 10, "or directory not empty", 13, 10, 0
msg_pause   db "Press any key to continue . . .", 0
pipe_names  db "A:\~PIPE0.TMP", 0, 0, 0
            db "A:\~PIPE1.TMP", 0, 0, 0
            db "A:\~PIPE2.TMP", 0, 0, 0
            db "A:\~PIPE3.TMP", 0, 0, 0
CBUF_LEN    equ 4096
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
pl_in       resw 1
pl_out      resw 1
pl_depth    resb 1
r_app       resb 1
r_in        resw 1
r_out       resw 1
sv_in       resw 1
sv_out      resw 1
rin_name    resb 80
rout_name   resb 80
t_buf       resb 132
t_buf2      resb 132
dta2        resb 128
c_src       resb 132
c_dst       resb 132
c_pre       resb 132
c_dpre      resb 132
c_spath     resb 160
c_dpath     resb 160
c_count     resd 1
c_dh        resw 1
c_open      resb 1
c_dstdir    resb 1
c_wild      resb 1
c_isdev     resb 1
c_eof       resb 1
c_nodst     resb 1
c_first     resb 1
c_last      resb 1
c_next      resw 1
ynbuf       resb 8
m_mask      resb 12
m_name      resb 12
m_new       resb 14
cbuf        resb CBUF_LEN
            resb 2048
stack_top:
prog_end:
