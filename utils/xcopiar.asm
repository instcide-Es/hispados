; =============================================================================
;  Hispa-DOS XCOPIAR.COM (XCOPY) - copiar archivos y árboles de carpetas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  XCOPIAR origen [destino] [/A | /M] [/D:fecha] [/P] [/S [/E]] [/V] [/W]
;          [/Y | /-Y]
;     origen   archivo(s) (admite comodines) o una carpeta (todos sus archivos)
;     destino  carpeta o archivo; si se omite, la carpeta actual
;     /A       solo los que tienen el atributo de archivo (no se cambia)
;     /M       igual, pero se quita el atributo de archivo en el origen
;     /D:fecha solo los modificados en esa fecha o después
;     /P       pregunta antes de copiar cada archivo
;     /S       también las subcarpetas (no las vacías); /E también las vacías
;     /V       comprueba cada copia (se vuelve a leer y se compara)
;     /W       espera una tecla antes de empezar
;     /Y       no pregunta al sobrescribir; /-Y sí pregunta (lo normal, salvo
;              que la variable COPYCMD tenga /Y)
;  Los archivos ocultos y de sistema no se copian. Las copias quedan con el
;  atributo de archivo y sin el de solo lectura, con la fecha del original.
;  Para ir deprisa se leen muchos archivos seguidos en un búfer grande (hasta
;  60 KB) y luego se escriben todos.
;  ERRORLEVEL: 0 bien, 1 no había archivos, 2 Ctrl+C, 4 error al empezar
;  (sintaxis, memoria, unidad, copia cíclica), 5 error al escribir.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXENT  equ 24                      ; archivos pendientes de escribir
ENT     equ 272                     ; tamaño de cada entrada
E_SRC   equ 0                       ; ruta de origen (128)
E_DST   equ 128                     ; ruta de destino (128)
E_TIME  equ 256
E_DATE  equ 258
E_LEN   equ 260
E_OFF   equ 262
VBUF    equ 2048                    ; búfer de cada lado al verificar

start:
        mov sp, stack_top
        ; las variables sin valor inicial a cero (DOS no las limpia)
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; Ctrl+C -> ERRORLEVEL 2
        mov dx, on_break
        mov ax, 0x2523
        int 0x21
        call env_copycmd
        call parse_args
        cmp byte [src], 0
        jne .havesrc
        mov si, msg_nosrc
        jmp die4
.havesrc:
        cmp byte [opt_m], 0
        je .nm
        mov byte [opt_a], 1
.nm:
        cmp byte [opt_s], 0
        jne .se
        mov byte [opt_e], 0         ; /E solo vale con /S
.se:
        ; búfer grande: 60 KB o lo que haya (al menos 4 KB)
        mov bx, 0x0F00
        mov ah, 0x48
        int 0x21
        jnc .mem
        cmp bx, 0x100
        jb .nomem
        mov ah, 0x48
        int 0x21
        jnc .mem
.nomem:
        mov si, msg_nomem
        jmp die4
.mem:
        mov [bufseg], ax
        shl bx, 4
        mov [bufsize], bx

        ; ---- origen ----
        mov si, src
        mov di, spath
        call truename
        jc bad_drive
        mov si, src
        call has_wild
        test al, al
        jnz .split
        mov si, src
        call last_char
        cmp al, '\'
        je .sdir
        cmp al, ':'
        je .sdir
        mov si, spath
        call str_end
        cmp di, spath + 3
        jbe .sdir
        mov dx, spath
        mov ax, 0x4300
        int 0x21
        jc .split
        test cl, 0x10
        jz .split
.sdir:
        ; una carpeta: todos sus archivos
        mov si, spath
        call str_end
        cmp byte [di-1], '\'
        je .sd2
        mov byte [di], '\'
        inc di
        mov byte [di], 0
.sd2:
        mov [pend], di
        mov dword [mask], '*.*'
        jmp .dest
.split:
        ; carpeta + máscara
        mov si, spath
        mov bx, si
.sp:
        lodsb
        test al, al
        jz .spd
        cmp al, '\'
        jne .sp
        mov bx, si
        jmp .sp
.spd:
        mov [pend], bx
        mov si, bx
        mov di, mask
        call strcpy

        ; ---- destino ----
.dest:
        cmp byte [have_dst], 0
        jne .hd
        mov word [dst], '.'
.hd:
        mov si, dst
        call has_wild
        test al, al
        jz .nw
        mov si, msg_badparam
        call puts
        mov si, dst
        call puts
        call crlf
        mov al, 4
        jmp exit_al
.nw:
        mov si, dst
        mov di, dpath
        call truename
        jc bad_drive
        ; con /S, ¿el destino está dentro del origen? (antes de preguntar nada)
        cmp byte [opt_s], 0
        je .nocyc
        mov si, dpath
        mov di, cur_dst
        call strcpy
        cmp byte [di-1], '\'
        je .cy1
        mov byte [di], '\'
        inc di
        mov byte [di], 0
.cy1:
        mov si, spath
        mov di, cur_dst
        mov cx, [pend]
        sub cx, spath
        push ds
        pop es
        repe cmpsb
        jne .nocyc
        cmp byte [di], 0
        je .nocyc
        mov si, msg_cyclic
        jmp die4
.nocyc:
        cmp byte [opt_w], 0
        je .nowait
        mov si, msg_wait
        call puts
        call getkey
        call crlf
.nowait:
        mov si, dpath
        call str_end
        cmp di, dpath + 3
        jbe .dirmode
        mov si, dst
        call last_char
        cmp al, '\'
        je .dirmode
        cmp al, ':'
        je .dirmode
        mov dx, dpath
        mov ax, 0x4300
        int 0x21
        jc .ask
        test cl, 0x10
        jnz .dirmode
        jmp .filemode
.ask:
        ; no existe: ¿archivo o directorio?
        mov si, msg_fd1
        call puts
        mov si, dst
        call puts
        mov si, msg_fd2
        call puts
        call readline
        mov al, [u_line]
        call upcase
        cmp al, 'D'
        je .dirmode
        cmp al, 'A'
        jne .ask
.filemode:
        mov byte [filemode], 1
        mov si, dpath
        mov di, dfile
        call strcpy
        ; la carpeta del archivo
        mov si, dpath
        mov bx, si
.fp:
        lodsb
        test al, al
        jz .fpd
        cmp al, '\'
        jne .fp
        mov bx, si
        jmp .fp
.fpd:
        mov byte [bx], 0
        mov [dend], bx
        mov byte [opt_s], 0         ; a un archivo no se copian carpetas
        mov byte [opt_e], 0
        jmp .go
.dirmode:
        mov si, dpath
        call str_end
        cmp byte [di-1], '\'
        je .dm2
        mov byte [di], '\'
        inc di
        mov byte [di], 0
.dm2:
        mov [dend], di
        ; ¿la carpeta de destino es la de origen o está dentro?
        mov si, spath
        mov di, dpath
        mov cx, [pend]
        sub cx, spath
        push ds
        pop es
        repe cmpsb
        jne .go
        cmp byte [di], 0
        jne .inside
        mov si, msg_self
        jmp die4
.inside:
        cmp byte [opt_s], 0
        je .go
        mov si, msg_cyclic
        jmp die4

.go:
        mov si, msg_reading
        call puts
        call walk
        call flush
        cmp word [found], 0
        jne .sum
        mov si, msg_nf
        call puts
        mov si, src
        call puts
        call crlf
        mov byte [exitcode], 1
.sum:
        movzx eax, word [copied]
        mov cl, 9
        call putnum_w
        mov si, msg_copied
        call puts
        mov al, [exitcode]
exit_al:
        mov ah, 0x4C
        int 0x21

bad_drive:
        mov si, msg_baddrv
        cmp ax, 15
        je die4
        mov si, msg_badpath
die4:
        call puts
        mov ax, 0x4C04
        int 0x21

on_break:
        mov ax, 0x4C02
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- argumentos ----
parse_args:
        mov si, 0x81
.next:
        lodsb
        cmp al, ' '
        je .next
        cmp al, 9
        je .next
        cmp al, 13
        je .done
        cmp al, '/'
        je .sw
        dec si
        mov di, src
        cmp byte [src], 0
        je .tok
        mov di, dst
        cmp byte [have_dst], 0
        jne .toomany
        mov byte [have_dst], 1
.tok:
        mov cx, 120
.tc:
        lodsb
        cmp al, ' '
        je .te
        cmp al, 9
        je .te
        cmp al, 13
        je .te
        cmp al, '/'
        je .te
        call upcase
        stosb
        loop .tc
.te:
        dec si
        mov byte [di], 0
        jmp .next
.done:
        ret
.toomany:
        mov si, msg_toomany
        jmp die4
.sw:
        lea bx, [si-1]              ; para el mensaje de error
        lodsb
        call upcase
        mov di, opt_a
        cmp al, 'A'
        je .set
        mov di, opt_m
        cmp al, 'M'
        je .set
        mov di, opt_p
        cmp al, 'P'
        je .set
        mov di, opt_s
        cmp al, 'S'
        je .set
        mov di, opt_e
        cmp al, 'E'
        je .set
        mov di, opt_v
        cmp al, 'V'
        je .set
        mov di, opt_w
        cmp al, 'W'
        je .set
        mov di, opt_y
        cmp al, 'Y'
        je .set
        cmp al, '?'
        je usage
        cmp al, 'D'
        je .date
        cmp al, '-'
        jne .bad
        lodsb
        call upcase
        cmp al, 'Y'
        jne .bad
        mov byte [opt_y], 0
        jmp .end
.set:
        mov byte [di], 1
.end:
        mov al, [si]
        cmp al, ' '
        je .next
        cmp al, 9
        je .next
        cmp al, 13
        je .next
        cmp al, '/'
        je .next
.bad:
        mov si, msg_badparam
        call puts
        mov si, bx
.bp:
        lodsb
        cmp al, ' '
        jbe .bpe
        call putc
        jmp .bp
.bpe:
        call crlf
        mov ax, 0x4C04
        int 0x21
.date:
        cmp byte [si], ':'
        jne .bad
        inc si
        call parse_num
        jc .bdate
        mov [dn1], ax
        call date_sep
        jc .bdate
        call parse_num
        jc .bdate
        mov [dn2], ax
        call date_sep
        jc .bdate
        call parse_num
        jc .bdate
        mov [dn3], ax
        mov al, [si]
        cmp al, ' '
        je .dok
        cmp al, 9
        je .dok
        cmp al, 13
        jne .bdate
.dok:
        call build_date
        jc .bdate
        mov byte [opt_d], 1
        jmp .next
.bdate:
        mov si, msg_baddate
        jmp die4

; [SI] es un separador de fecha (/ - .)? -> avanza; CF si no
date_sep:
        mov al, [si]
        cmp al, '/'
        je .ok
        cmp al, '-'
        je .ok
        cmp al, '.'
        je .ok
        stc
        ret
.ok:
        inc si
        clc
        ret

; dn1/dn2/dn3 en el orden del país -> d_date (fecha empaquetada); CF si mal
build_date:
        mov dx, cbuf
        mov ax, 0x3800
        int 0x21
        mov ax, [dn1]               ; 1: día-mes-año (España)
        mov bx, [dn2]
        mov cx, [dn3]
        jc .dmy
        cmp word [cbuf], 0
        jne .n0
        xchg ax, bx                 ; 0: mes-día-año
        jmp .dmy
.n0:
        cmp word [cbuf], 2
        jne .dmy
        xchg ax, cx                 ; 2: año-mes-día
.dmy:
        ; AX = día, BX = mes, CX = año
        cmp cx, 100
        jae .y4
        add cx, 1900
        cmp cx, 1980
        jae .y4
        add cx, 100
.y4:
        sub cx, 1980
        jb .bad
        cmp cx, 127
        ja .bad
        cmp bx, 1
        jb .bad
        cmp bx, 12
        ja .bad
        cmp ax, 1
        jb .bad
        cmp ax, 31
        ja .bad
        shl cx, 9
        shl bx, 5
        or cx, bx
        or cx, ax
        mov [d_date], cx
        clc
        ret
.bad:
        stc
        ret

; COPYCMD=/Y en el entorno -> no preguntar al sobrescribir
env_copycmd:
        mov ax, [0x2C]
        test ax, ax
        jz .r
        mov es, ax
        xor di, di
.var:
        cmp byte [es:di], 0
        je .r
        push di
        mov si, s_copycmd
        mov cx, 8
        repe cmpsb
        jne .skip
.val:
        mov al, [es:di]
        inc di
        test al, al
        jz .vend
        cmp al, '/'
        jne .val
        mov al, [es:di]
        cmp al, '-'
        jne .y1
        mov al, [es:di+1]
        call upcase
        cmp al, 'Y'
        jne .val
        mov byte [opt_y], 0
        jmp .val
.y1:
        call upcase
        cmp al, 'Y'
        jne .val
        mov byte [opt_y], 1
        jmp .val
.vend:
        pop di
        jmp .r
.skip:
        pop di
.s2:
        mov al, [es:di]
        inc di
        test al, al
        jnz .s2
        jmp .var
.r:
        push ds
        pop es
        ret

; ---- recorrer la carpeta spath (hasta pend) con la máscara ----
walk:
        push bp
        sub sp, 44
        mov bp, sp
        mov dx, bp
        mov ah, 0x1A
        int 0x21
        mov byte [dir_ok], 0
        cmp byte [opt_e], 0
        je .files
        call ensure_dir             ; /E: también las carpetas vacías
.files:
        mov di, [pend]
        mov si, mask
        call strcpy
        mov dx, spath
        mov cx, 0x01                ; normales y de solo lectura
        mov ah, 0x4E
        int 0x21
        jc .subs
.f:
        test byte [bp+0x15], 0x1E   ; ni ocultos, ni de sistema, ni carpetas
        jnz .fn
        call do_file
.fn:
        mov ah, 0x4F
        int 0x21
        jnc .f
.subs:
        cmp byte [opt_s], 0
        je .r
        mov di, [pend]
        mov dword [di], '*.*'
        mov dx, spath
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jc .r
.d:
        test byte [bp+0x15], 0x10
        jz .dn
        test byte [bp+0x15], 0x06
        jnz .dn
        cmp byte [bp+0x1E], '.'
        je .dn
        push word [pend]
        push word [dend]
        mov di, [pend]
        lea si, [bp+0x1E]
        call strcpy
        mov byte [di], '\'
        inc di
        mov byte [di], 0
        mov [pend], di
        cmp di, spath + 114
        jae .back
        mov di, [dend]
        lea si, [bp+0x1E]
        call strcpy
        mov byte [di], '\'
        inc di
        mov byte [di], 0
        mov [dend], di
        cmp di, dpath + 110
        jae .back
        call walk
.back:
        pop word [dend]
        pop word [pend]
        mov di, [dend]
        mov byte [di], 0
        mov byte [dir_ok], 0
        mov dx, bp                  ; la DTA de este nivel
        mov ah, 0x1A
        int 0x21
.dn:
        mov ah, 0x4F
        int 0x21
        jnc .d
.r:
        add sp, 44
        pop bp
        ret

; ---- el archivo de la DTA [bp] ----
do_file:
        mov al, [bp+0x15]
        cmp byte [opt_a], 0
        je .f1
        test al, 0x20
        jz .ret
.f1:
        cmp byte [opt_d], 0
        je .f2
        mov ax, [bp+0x18]
        cmp ax, [d_date]
        jb .ret
.f2:
        inc word [found]
        ; ruta completa del origen y del destino
        mov di, [pend]
        lea si, [bp+0x1E]
        call strcpy
        mov di, cur_dst
        mov si, dfile
        cmp byte [filemode], 0
        jne .fm
        mov si, dpath
        call strcpy
        lea si, [bp+0x1E]
.fm:
        call strcpy
        ; ¿sobre sí mismo?
        mov si, spath
        mov di, cur_dst
        call strcmp
        jne .notself
        call flush
        mov si, msg_self
        jmp die4
.notself:
        cmp byte [opt_p], 0
        je .np
.askp:
        mov si, spath
        call puts
        mov si, msg_yn
        call puts
        call readline
        mov al, [u_line]
        call upcase
        cmp al, 'N'
        je .ret
        cmp al, 'S'
        je .np
        cmp al, 'Y'
        jne .askp
.np:
        ; ¿ya existe el destino?
        mov dx, cur_dst
        mov ax, 0x4300
        int 0x21
        jc .new
        test cl, 0x10
        jz .exists
        mov si, msg_cant
        call puts
        mov si, cur_dst
        call puts
        call crlf
        mov byte [exitcode], 5
        ret
.exists:
        cmp byte [opt_y], 0
        jne .new
.ow:
        mov si, msg_ow1
        call puts
        mov si, cur_dst
        call puts
        mov si, msg_ow2
        call puts
        call readline
        mov al, [u_line]
        call upcase
        cmp al, 'N'
        je .ret
        cmp al, 'S'
        je .new
        cmp al, 'Y'
        je .new
        cmp al, 'T'
        jne .ow
        mov byte [opt_y], 1         ; todos
.new:
        cmp byte [opt_p], 0
        jne .shown
        mov si, spath
        call puts
        call crlf
.shown:
        cmp byte [dir_ok], 0
        jne queue_file
        call ensure_dir
        jmp queue_file
.ret:
        ret

; ---- leer el archivo al búfer (o copiarlo a trozos si no cabe) ----
queue_file:
        mov eax, [bp+0x1A]
        movzx edx, word [bufsize]
        sub dx, [bufused]
        cmp eax, edx
        jbe .fits
        call flush
        mov eax, [bp+0x1A]
        movzx edx, word [bufsize]
        cmp eax, edx
        ja big_copy
.fits:
        cmp word [nent], MAXENT
        jb .room
        call flush
.room:
        mov ax, [nent]
        mov dx, ENT
        mul dx
        add ax, entries
        mov bx, ax
        mov si, spath
        lea di, [bx+E_SRC]
        call strcpy
        mov si, cur_dst
        lea di, [bx+E_DST]
        call strcpy
        mov ax, [bp+0x16]
        mov [bx+E_TIME], ax
        mov ax, [bp+0x18]
        mov [bx+E_DATE], ax
        mov dx, spath
        mov ax, 0x3D00
        int 0x21
        jc open_err
        mov [hs], ax
        push bx
        mov bx, ax
        mov cx, [bp+0x1A]
        mov dx, [bufused]
        push ds
        mov ds, [bufseg]
        mov ah, 0x3F
        int 0x21
        pop ds
        pop bx
        jc .rerr
        mov [bx+E_LEN], ax
        mov cx, [bufused]
        mov [bx+E_OFF], cx
        add [bufused], ax
        inc word [nent]
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        ret
.rerr:
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
read_err:
        mov si, msg_read
        jmp file_err
open_err:
        mov si, msg_open
file_err:
        call puts
        mov si, spath
        call puts
        call crlf
        mov byte [exitcode], 5
        ret

; un archivo más grande que el búfer: a trozos, directamente
big_copy:
        mov dx, spath
        mov ax, 0x3D00
        int 0x21
        jc open_err
        mov [hs], ax
        mov dx, cur_dst
        mov cx, 0x20
        mov ah, 0x3C
        int 0x21
        jnc .cr
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        mov si, cur_dst
        jmp create_err
.cr:
        mov [hd], ax
.l:
        mov bx, [hs]
        mov cx, [bufsize]
        xor dx, dx
        push ds
        mov ds, [bufseg]
        mov ah, 0x3F
        int 0x21
        pop ds
        jc .rerr
        test ax, ax
        jz .eof
        mov cx, ax
        mov bx, [hd]
        xor dx, dx
        push ds
        mov ds, [bufseg]
        mov ah, 0x40
        int 0x21
        pop ds
        jc .full
        cmp ax, cx
        jne .full
        jmp .l
.eof:
        mov cx, [bp+0x16]
        mov dx, [bp+0x18]
        mov bx, [hd]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        mov si, spath
        mov di, cur_dst
        jmp after_copy
.rerr:
        mov bx, [hd]
        mov ah, 0x3E
        int 0x21
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        mov dx, cur_dst
        mov ah, 0x41
        int 0x21
        jmp read_err
.full:
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        mov dx, cur_dst
        jmp disk_full

; ---- escribir todos los archivos pendientes ----
flush:
        mov word [fi], 0
.l:
        mov ax, [fi]
        cmp ax, [nent]
        jae .done
        mov dx, ENT
        mul dx
        add ax, entries
        mov bx, ax
        lea dx, [bx+E_DST]
        mov cx, 0x20                ; con el atributo de archivo
        mov ah, 0x3C
        int 0x21
        jnc .cr
        lea si, [bx+E_DST]
        call create_err
        jmp .nx
.cr:
        mov [hd], ax
        mov cx, [bx+E_LEN]
        jcxz .nowr
        mov dx, [bx+E_OFF]
        push bx
        mov bx, [hd]
        push ds
        mov ds, [bufseg]
        mov ah, 0x40
        int 0x21
        pop ds
        pop bx
        jc .full
        cmp ax, cx
        jne .full
.nowr:
        mov cx, [bx+E_TIME]
        mov dx, [bx+E_DATE]
        push bx
        mov bx, [hd]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        pop bx
        lea si, [bx+E_SRC]
        lea di, [bx+E_DST]
        call after_copy
.nx:
        inc word [fi]
        jmp .l
.done:
        mov word [nent], 0
        mov word [bufused], 0
        ret
.full:
        lea dx, [bx+E_DST]
disk_full:
        ; DX = destino a medias: se cierra y se borra
        push dx
        mov bx, [hd]
        mov ah, 0x3E
        int 0x21
        pop dx
        mov ah, 0x41
        int 0x21
        call crlf
        mov si, msg_full
        call puts
        mov ax, 0x4C05
        int 0x21

; SI = destino que no se pudo crear
create_err:
        push si
        mov si, msg_cant
        call puts
        pop si
        call puts
        call crlf
        mov byte [exitcode], 5
        ret

; copia hecha (SI = origen, DI = destino): /V, /M y contarla
after_copy:
        cmp byte [opt_v], 0
        je .m
        call verify
.m:
        cmp byte [opt_m], 0
        je .c
        mov dx, si
        mov ax, 0x4300
        int 0x21
        jc .c
        and cx, 0x07                ; quitar el atributo de archivo
        mov ax, 0x4301
        int 0x21
.c:
        inc word [copied]
        ret

; releer origen (SI) y destino (DI) y compararlos; si no son iguales, salir
verify:
        pusha
        mov [vdst], di
        mov dx, si
        mov ax, 0x3D00
        int 0x21
        jc .bad
        mov [vh1], ax
        mov dx, di
        mov ax, 0x3D00
        int 0x21
        jc .bad1
        mov [vh2], ax
.l:
        mov bx, [vh1]
        mov dx, vbuf1
        mov cx, VBUF
        mov ah, 0x3F
        int 0x21
        jc .bad2
        mov [vlen], ax
        mov bx, [vh2]
        mov dx, vbuf2
        mov cx, VBUF
        mov ah, 0x3F
        int 0x21
        jc .bad2
        cmp ax, [vlen]
        jne .bad2
        test ax, ax
        jz .same
        mov cx, ax
        mov si, vbuf1
        mov di, vbuf2
        push ds
        pop es
        repe cmpsb
        jne .bad2
        jmp .l
.same:
        mov bx, [vh2]
        mov ah, 0x3E
        int 0x21
        mov bx, [vh1]
        mov ah, 0x3E
        int 0x21
        popa
        ret
.bad2:
        mov bx, [vh2]
        mov ah, 0x3E
        int 0x21
.bad1:
        mov bx, [vh1]
        mov ah, 0x3E
        int 0x21
.bad:
        mov si, msg_verify
        call puts
        mov si, [vdst]
        call puts
        call crlf
        mov ax, 0x4C05
        int 0x21

; crear (si faltan) las carpetas de dpath; si no se puede, salir
ensure_dir:
        mov si, dpath + 3
.l:
        mov al, [si]
        test al, al
        jz .chk
        cmp al, '\'
        jne .n
        mov byte [si], 0
        mov dx, dpath
        mov ah, 0x39
        int 0x21
        mov byte [si], '\'
.n:
        inc si
        jmp .l
.chk:
        mov si, [dend]
        cmp si, dpath + 3
        jbe .ok
        mov byte [si-1], 0
        mov dx, dpath
        mov ax, 0x4300
        int 0x21
        mov byte [si-1], '\'
        jc .fail
        test cl, 0x10
        jz .fail
.ok:
        mov byte [dir_ok], 1
        ret
.fail:
        call flush
        mov si, msg_mderr
        call puts
        mov si, dpath
        call puts
        call crlf
        mov ax, 0x4C04
        int 0x21

; SI = nombre -> DI = ruta completa (INT 21h 60h); CF y AX si hay error
truename:
        push es
        push ds
        pop es
        mov ah, 0x60
        int 0x21
        pop es
        ret

; SI = texto -> AL = 1 si tiene * o ?
has_wild:
        xor al, al
.l:
        mov ah, [si]
        inc si
        test ah, ah
        jz .r
        cmp ah, '*'
        je .y
        cmp ah, '?'
        jne .l
.y:
        mov al, 1
.r:
        ret

; SI = texto -> AL = último carácter (0 si está vacío)
last_char:
        xor al, al
.l:
        mov ah, [si]
        inc si
        test ah, ah
        jz .r
        mov al, ah
        jmp .l
.r:
        ret

; SI = texto -> DI = su cero final
str_end:
        mov di, si
.l:
        cmp byte [di], 0
        je .r
        inc di
        jmp .l
.r:
        ret

; SI y DI iguales? -> ZF
strcmp:
        push si
        push di
.l:
        mov al, [si]
        cmp al, [di]
        jne .r
        inc si
        inc di
        test al, al
        jnz .l
.r:
        pop di
        pop si
        ret

; DI <- SI (ASCIIZ); DI queda sobre el cero
strcpy:
        lodsb
        mov [di], al
        test al, al
        jz .e
        inc di
        jmp strcpy
.e:
        ret

%include "util.inc"

msg_usage   db "Uso: XCOPIAR origen [destino] [/A | /M] [/D:fecha] [/P] [/S [/E]] [/V] [/W]", 13, 10
            db "               [/Y | /-Y]", 13, 10
            db "Copia archivos (admite comodines) y árboles de carpetas.", 13, 10
            db "  /A        solo los que tienen el atributo de archivo (no se cambia)", 13, 10
            db "  /M        igual que /A, pero quita el atributo de archivo del origen", 13, 10
            db "  /D:fecha  solo los modificados en esa fecha o después", 13, 10
            db "  /P        pregunta antes de copiar cada archivo", 13, 10
            db "  /S        también las subcarpetas que no estén vacías", 13, 10
            db "  /E        con /S, también las subcarpetas vacías", 13, 10
            db "  /V        comprueba cada copia", 13, 10
            db "  /W        espera una tecla antes de empezar", 13, 10
            db "  /Y        no pregunta al sobrescribir; /-Y sí pregunta", 13, 10
            db "La variable COPYCMD puede llevar /Y o /-Y.", 13, 10, 0
msg_nosrc   db "Falta el origen. Escriba XCOPIAR /? para ver la ayuda.", 13, 10, 0
msg_badparam db "Parámetro no válido - ", 0
msg_toomany db "Demasiados parámetros", 13, 10, 0
msg_nomem   db "Memoria insuficiente", 13, 10, 0
msg_baddrv  db "Especificación de unidad no válida", 13, 10, 0
msg_badpath db "Ruta no válida", 13, 10, 0
msg_baddate db "Fecha no válida", 13, 10, 0
msg_wait    db "Pulse una tecla para empezar a copiar los archivos", 0
msg_fd1     db "¿Es ", 0
msg_fd2     db " un archivo o un directorio (A = archivo, D = directorio)? ", 0
msg_reading db "Leyendo los archivos de origen...", 13, 10, 0
msg_nf      db "No se encontró el archivo - ", 0
msg_self    db "No se puede copiar un archivo sobre sí mismo", 13, 10, 0
msg_cyclic  db "No se puede ejecutar XCOPIAR de forma cíclica", 13, 10, 0
msg_mderr   db "No se puede crear el directorio ", 0
msg_cant    db "No se puede crear el archivo ", 0
msg_open    db "No se puede abrir el archivo ", 0
msg_read    db "Error al leer el archivo ", 0
msg_full    db "Disco lleno", 13, 10, 0
msg_verify  db "Error de verificación: ", 0
msg_yn      db " (S/N)? ", 0
msg_ow1     db "¿Sobrescribir ", 0
msg_ow2     db " (S/N/T)? ", 0
msg_copied  db " archivo(s) copiado(s)", 13, 10, 0
s_copycmd   db "COPYCMD="

        section .bss
bss_start:
opt_a       resb 1
opt_m       resb 1
opt_d       resb 1
opt_p       resb 1
opt_s       resb 1
opt_e       resb 1
opt_v       resb 1
opt_w       resb 1
opt_y       resb 1
have_dst    resb 1
filemode    resb 1
dir_ok      resb 1
exitcode    resb 1
            resb 1
d_date      resw 1
dn1         resw 1
dn2         resw 1
dn3         resw 1
bufseg      resw 1
bufsize     resw 1
bufused     resw 1
nent        resw 1
fi          resw 1
found       resw 1
copied      resw 1
pend        resw 1
dend        resw 1
hs          resw 1
hd          resw 1
vh1         resw 1
vh2         resw 1
vlen        resw 1
vdst        resw 1
cbuf        resb 40
src         resb 128
dst         resb 128
mask        resb 16
spath       resb 260
dpath       resb 160
dfile       resb 128
cur_dst     resb 160
u_numbuf    resb 16
u_lbuf      resb 84
entries     resb MAXENT * ENT
vbuf1       resb VBUF
vbuf2       resb VBUF
            resb 4096
stack_top:
prog_end:
