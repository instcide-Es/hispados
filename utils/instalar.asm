; =============================================================================
;  Hispa-DOS INSTALAR.COM - instalar Hispa-DOS en una unidad ya formateada
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  INSTALAR unidad:   hace la unidad arrancable (SYS) y copia desde la unidad
;                     de arranque la carpeta del sistema (\HISPADOS:
;                     programas, drivers y ALIAS.INI; \HISPADOS\JUEGOS:
;                     los juegos de Hispa Ventanas, con los cartuchos de
;                     sus emuladores en JUEGOS\NES y JUEGOS\SMS; y
;                     \HISPADOS\PROGS: sus accesorios), AUTOEXEC.BAT,
;                     CONFIG.SYS y LEEME.TXT. Si la unidad ya tiene un
;                     AUTOEXEC.BAT o un CONFIG.SYS, se respetan. En los que
;                     se copian, la letra de origen (A: o la USB) pasa a C:,
;                     la que tendrá el disco al arrancar desde él.
;  Para preparar un disco nuevo: PARTIR, reiniciar si hace falta, FORMATEA
;  unidad: y después INSTALAR unidad:.
;  Último paso, la comprobación del sonido: AUDICION /RAPIDA (la versión
;  corta de la prueba de sonido) guarda lo que suena en el SONIDO.INI del
;  disco instalado (unidad:\HISPADOS\SONIDO.INI, con su informe SONIDO.TXT
;  al lado); así HSPSND lo usa desde el primer arranque. /NOSONIDO no la hace.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        mov sp, stack_top
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov si, msg_title
        call puts
        ; unidad de destino
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 13
        je usage
        and al, 0xDF
        cmp al, 'A'
        jb usage
        cmp al, 'Z'
        ja usage
        cmp byte [si], ':'
        jne usage
        mov [dst], al
        mov [tail_drv], al
        mov [msg_drv1], al
        mov [msg_drv2], al
        mov [aud_path], al
        mov [aud_d1], al
        mov [aud_d2], al
        ; opciones: /NOSONIDO
        inc si
.op:
        lodsb
        cmp al, 13
        je .ope
        cmp al, '/'
        jne .op
        mov eax, [si]
        and eax, 0xDFDFDFDF
        cmp eax, 'NOSO'
        jne .op
        mov eax, [si+4]
        and eax, 0xDFDFDFDF
        cmp eax, 'NIDO'
        jne .op
        mov byte [nosnd], 1
        add si, 8
        jmp .op
.ope:
        ; unidad de origen: la de arranque
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [src], dl
        cmp dl, [dst]
        jne .ok
        mov si, msg_same
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        ; ¿existe y tiene formato?
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .fmt
        mov si, msg_nofmt
        call puts
        mov ax, 0x4C02
        int 0x21
.fmt:
        ; 1) sistema: SYS unidad:
        mov al, [src]
        mov [sys_path], al
        mov word [pblock], 0
        mov word [pblock+2], sys_tail
        mov [pblock+4], cs
        mov dword [pblock+6], 0
        mov dword [pblock+10], 0
        mov dx, sys_path
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        jc .syserr
        mov ah, 0x4D
        int 0x21
        test al, al
        jz .copy
.syserr:
        mov si, msg_syserr
        call puts
        mov ax, 0x4C03
        int 0x21
.copy:
        ; 2) archivos
        mov si, msg_copying
        call puts
        ; el sistema: \HISPADOS entero
        mov byte [keep], 0
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
        mov si, s_hspj              ; y los juegos de Hispa Ventanas
        mov di, s_hjdir
        call make_dir
        call copy_set
        mov si, s_hspn              ; con los cartuchos de los emuladores,
        mov di, s_hndir             ; cada uno en su carpeta
        call make_dir
        call copy_set
        mov si, s_hsps
        mov di, s_hsdir
        call make_dir
        call copy_set
        mov si, s_hspp              ; y sus accesorios (Pintura)
        mov di, s_hpdir
        call make_dir
        call copy_set
        mov si, s_autoexec
        mov di, s_root
        mov byte [keep], 1
        mov byte [fix], 1
        call copy_set
        mov si, s_config
        mov di, s_root
        call copy_set
        mov byte [keep], 0
        mov byte [fix], 0
        mov si, s_leeme
        mov di, s_root
        call copy_set
        cmp word [nfailed], 0
        je .allok
        ; alguno no se ha copiado (disco lleno...): no decir que está instalado
        mov si, msg_fail
        call puts
        movzx eax, word [nfailed]
        call putnum
        mov si, msg_fail2
        call puts
        mov ax, 0x4C04
        int 0x21
.allok:
        mov si, msg_done
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_done2
        call puts
        ; 3) la comprobación del sonido, con el disco recién instalado
        cmp byte [nosnd], 0
        jne .end
        call sound_check
.end:
        mov si, msg_boot
        call puts
        mov ax, 0x4C00
        int 0x21

; AUDICION /RAPIDA del disco instalado: lo aprendido, en su SONIDO.INI
; (AUDICION dice ella misma cómo ha ido; aquí, solo si no ha podido empezar)
sound_check:
        mov si, msg_snd
        call puts
        mov word [pblock], 0
        mov word [pblock+2], aud_tail
        mov [pblock+4], cs
        mov dword [pblock+6], 0
        mov dword [pblock+10], 0
        mov dx, aud_path
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        jc .fail
        mov ah, 0x4D
        int 0x21
        cmp al, 1                   ; 1: sin memoria (0 y 2 los explica ella)
        jne .r
.fail:
        mov si, msg_sndko
        call puts
.r:
        mov si, msg_crlf
        jmp puts

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; DI = carpeta de destino ("\DRIVERS\"): crearla en la unidad de destino
make_dir:
        push si
        push di
        mov bx, path_d
        mov al, [dst]
        mov [bx], al
        mov byte [bx+1], ':'
        add bx, 2
        mov si, di
.c:
        lodsb
        mov [bx], al
        inc bx
        test al, al
        jnz .c
        mov byte [bx-2], 0          ; sin la barra final
        mov dx, path_d
        mov ah, 0x39
        int 0x21                    ; si ya existe, no pasa nada
        pop di
        pop si
        ret

; SI = patrón relativo a la raíz ("\DRIVERS\*.*"), DI = carpeta destino
; ("\" o "\DRIVERS\"): copiar de la unidad de origen a la de destino
copy_set:
        push si
        push di
        mov [cs_dir], di
        ; patrón completo en el origen
        mov bx, pat
        mov al, [src]
        mov [bx], al
        mov byte [bx+1], ':'
        add bx, 2
.p:
        lodsb
        mov [bx], al
        inc bx
        test al, al
        jnz .p
        ; carpeta de origen = patrón sin el nombre
        mov si, pat
        mov di, sdir
        mov bx, di
.d:
        lodsb
        mov [di], al
        inc di
        test al, al
        jz .de
        cmp al, '\'
        jne .d
        mov bx, di
        jmp .d
.de:
        mov byte [bx], 0
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, pat
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jc .r
.f:
        ; origen = sdir + nombre ; destino = unidad + carpeta + nombre
        mov si, sdir
        mov di, path_s
        call strcpy
        mov si, dta + 0x1E
        call strcpy
        mov di, path_d
        mov al, [dst]
        mov [di], al
        mov byte [di+1], ':'
        add di, 2
        mov si, [cs_dir]
        call strcpy
        mov si, dta + 0x1E
        call strcpy
        ; ¿respetar el que ya existe?
        cmp byte [keep], 0
        je .cp
        mov dx, dta2
        mov ah, 0x1A
        int 0x21
        mov dx, path_d
        mov cx, 0x07
        mov ah, 0x4E
        int 0x21
        pushf
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        popf
        jnc .next
.cp:
        call copy_file
        jnc .cok
        inc word [nfailed]
        jmp .next
.cok:
        inc word [ncopied]
.next:
        mov ah, 0x4F
        int 0x21
        jnc .f
.r:
        pop di
        pop si
        ret

; DI <- SI (copia ASCIIZ, DI queda sobre el cero final)
strcpy:
        lodsb
        mov [di], al
        test al, al
        jz .e
        inc di
        jmp strcpy
.e:
        ret

; path_s -> path_d
copy_file:
        mov dx, path_d
        mov ax, 0x4301
        xor cx, cx
        int 0x21                    ; quitar solo lectura de una copia anterior
        mov dx, path_s
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [hs], ax
        mov dx, path_d
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cl
        mov [hd], ax
.l:
        mov bx, [hs]
        mov dx, buf
        mov cx, BUFSZ
        mov ah, 0x3F
        int 0x21
        jc .both
        test ax, ax
        jz .eof
        mov cx, ax
        cmp byte [fix], 0
        je .w
        call fix_drive
.w:
        mov bx, [hd]
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .both
        cmp ax, cx
        jne .both
        jmp .l
.eof:
        mov bx, [hs]
        mov ax, 0x5700
        int 0x21
        mov bx, [hd]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        clc
        ret
.both:
        mov bx, [hd]
        mov ah, 0x3E
        int 0x21
        mov dx, path_d              ; no dejar un archivo a medias
        mov ah, 0x41
        int 0x21
.cl:
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        stc
.r:
        ret

; en buf (CX bytes): "X:" de la unidad de origen -> "C:", la letra que
; tendrá el disco al arrancar desde él (CONFIG.SYS y AUTOEXEC.BAT)
fix_drive:
        push cx
        push si
        mov si, buf
        dec cx
        jle .r
        mov ah, ' '                 ; AH = el carácter anterior
.l:
        cmp byte [si+1], ':'
        jne .n
        mov al, [si]
        and al, 0xDF
        cmp al, [src]
        jne .n
        ; solo si es una letra suelta: «Fecha:» o «Nota:» no son la unidad A:
        cmp ah, '0'
        jb .fx
        cmp ah, '9'
        jbe .n
        cmp ah, 'A'
        jb .fx
        or ah, 0x20
        cmp ah, 'z'
        jbe .n2
        jmp .n                      ; letras acentuadas (CP437)
.n2:
        cmp ah, 'a'
        jae .n
.fx:
        mov byte [si], 'C'
.n:
        mov ah, [si]
        inc si
        loop .l
.r:
        pop si
        pop cx
        ret

putnum:
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
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

BUFSZ       equ 16384
src         db 0
dst         db 0
keep        db 0
fix         db 0
ncopied     dw 0
nfailed     dw 0
hs          dw 0
hd          dw 0
cs_dir      dw 0
nosnd       db 0
aud_path    db "C:\HISPADOS\AUDICION.COM", 0
aud_tail    db aud_te - aud_tail - 1, " /RAPIDA "
aud_d1      db "C:\HISPADOS /INI:"
aud_d2      db "C:\HISPADOS\SONIDO.INI"
aud_te      db 13
sys_path    db "A:\HISPADOS\SISTEMA.COM", 0
sys_tail    db 3, " "
tail_drv    db "C:", 13
s_root      db "\", 0
s_hdir      db "\HISPADOS\", 0
s_hsp       db "\HISPADOS\*.*", 0
s_hjdir     db "\HISPADOS\JUEGOS\", 0
s_hspj      db "\HISPADOS\JUEGOS\*.*", 0
s_hndir     db "\HISPADOS\JUEGOS\NES\", 0
s_hspn      db "\HISPADOS\JUEGOS\NES\*.*", 0
s_hsdir     db "\HISPADOS\JUEGOS\SMS\", 0
s_hsps      db "\HISPADOS\JUEGOS\SMS\*.*", 0
s_hpdir     db "\HISPADOS\PROGS\", 0
s_hspp      db "\HISPADOS\PROGS\*.*", 0
s_autoexec  db "\AUTOEXEC.BAT", 0
s_config    db "\CONFIG.SYS", 0
s_leeme     db "\LEEME.TXT", 0
msg_title   db "Instalador de Hispa-DOS", 13, 10, 0
msg_usage   db "Uso: INSTALAR unidad: [/NOSONIDO]", 13, 10
            db "Instala Hispa-DOS en una unidad que ya tiene formato.", 13, 10
            db "Para un disco nuevo: PARTIR, FORMATEA unidad: y después INSTALAR unidad:", 13, 10
            db "Al final comprueba el sonido (AUDICION /RAPIDA): lo que suena queda en", 13, 10
            db "unidad:\HISPADOS\SONIDO.INI y HSPSND lo usa desde el primer arranque.", 13, 10
            db "Esc salta ese paso; /NOSONIDO no lo hace.", 13, 10, 0
msg_same    db "No se puede instalar sobre la unidad de arranque.", 13, 10, 0
msg_nofmt   db "La unidad "
msg_drv1    db "C: no existe o no tiene formato.", 13, 10
            db "Prepárela con PARTIR y FORMATEA y vuelva a ejecutar INSTALAR.", 13, 10, 0
msg_syserr  db "No se ha podido transferir el sistema (SYS).", 13, 10, 0
msg_copying db "Copiando los archivos de Hispa-DOS...", 13, 10, 0
msg_fail    db "La instalación NO está completa: no se han podido copiar ", 0
msg_fail2   db " archivos", 13, 10, "(¿disco lleno?).", 13, 10, 0
msg_done    db "Hispa-DOS instalado en la unidad "
msg_drv2    db "C: (", 0
msg_done2   db " archivos copiados).", 13, 10, 0
msg_crlf    db 13, 10, 0
msg_boot    db "Ya puede arrancar desde ella.", 13, 10, 0
msg_snd     db 13, 10, "Por último, la comprobación del sonido", 13, 10
            db "--------------------------------------", 13, 10, 0
msg_sndko   db "No se ha podido comprobar el sonido. Puede hacerlo después con la", 13, 10
            db "opción «Prueba» del menú de arranque o con la orden AUDICION.", 13, 10, 0

        section .bss
pblock      resb 14
pat         resb 80
sdir        resb 80
path_s      resb 128
path_d      resb 128
dta         resb 128
dta2        resb 128
buf         resb BUFSZ
            resb 512
stack_top:
prog_end:
