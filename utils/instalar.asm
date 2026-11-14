; =============================================================================
;  Hispa-DOS INSTALAR.COM - instalar Hispa-DOS en un disco duro (o disquete)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  INSTALAR           sin unidad, guiado: PARTIR /INSTALAR propone el disco
;                     duro (nunca la memoria USB ni el disco de arranque) y,
;                     si no tiene una partición DOS, ofrece crearla (con
;                     todo el disco si está vacío, o en el espacio libre).
;                     Después pregunta antes de seguir.
;  INSTALAR unidad:   en esa unidad. Si no tiene formato, pregunta y se lo da
;                     (FORMATEA unidad: /V:HISPADOS). Hace la unidad
;                     arrancable (SYS) y copia desde la unidad
;                     de arranque la carpeta del sistema (\HISPADOS:
;                     programas, drivers y ALIAS.INI; \HISPADOS\JUEGOS:
;                     los juegos de Hispa Ventanas, con los cartuchos de
;                     sus emuladores en JUEGOS\NES y JUEGOS\SMS; y
;                     \HISPADOS\PROGS: sus accesorios), AUTOEXEC.BAT,
;                     CONFIG.SYS y LEEME.TXT. Si la unidad ya tiene un
;                     AUTOEXEC.BAT o un CONFIG.SYS, se respetan. En los que
;                     se copian, la letra de origen (A: o la USB) y la que
;                     tiene ahora la unidad de destino (D: arrancando desde
;                     una memoria USB) pasan a C:, la que tendrá el disco al
;                     arrancar desde él.
;                     En un disco duro deja además la partición activa y,
;                     si el disco no tiene, el MBR de Hispa-DOS (PARTIR
;                     /ARRANQUE:unidad:). El sector de arranque lleva la
;                     unidad 80h y la geometría de ESE disco (SYS).
;  Último paso, la comprobación del sonido: AUDICION /RAPIDA (la versión
;  corta de la prueba de sonido) guarda lo que suena en el SONIDO.INI del
;  disco instalado (unidad:\HISPADOS\SONIDO.INI, con su informe SONIDO.TXT
;  al lado); así HSPSND lo usa desde el primer arranque. /NOSONIDO no la hace.
;  Nivel de error: 0 instalado, 1-5 error, 6 cancelado por el usuario.
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
        ; unidad de destino (sin ella: la propone PARTIR /INSTALAR)
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 9
        je .sk
        cmp al, 13
        je .guided
        cmp al, '/'
        jne .unit
        cmp byte [si], '?'
        je usage
        dec si
.guided:
        dec si
        mov byte [guided], 1
        jmp .op
.unit:
        and al, 0xDF
        cmp al, 'A'
        jb usage
        cmp al, 'Z'
        ja usage
        cmp byte [si], ':'
        jne usage
        call set_dst
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
        mov [sys_path], dl
        mov [part_path], dl
        mov [fmt_path], dl
        cmp byte [guided], 0
        je .chk
        call guided_pick
.chk:
        mov dl, [src]
        cmp dl, [dst]
        jne .ok
        mov si, msg_same
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        ; ¿existe? ¿es un disco duro?
        call drv_params
        jnc .ex
        mov si, msg_nofmt
        call puts
        mov ax, 0x4C02
        int 0x21
.ex:
        ; ¿tiene formato?
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .hasfmt
        ; sin formato: se le da (preguntando antes)
        mov si, msg_askfmt1
        call puts
        mov si, msg_askfmt2
        call puts
        call ask_yes
        jc cancel
        mov si, msg_crlf
        call puts
        mov dx, fmt_path
        mov bx, fmt_tail
        call run
        jc .fmterr
        test al, al
        jnz .fmterr
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .fmt
.fmterr:
        mov si, msg_fmterr
        call puts
        mov ax, 0x4C02
        int 0x21
.hasfmt:
        cmp byte [guided], 0
        je .fmt
        mov si, msg_askins1
        call puts
        mov si, msg_askins2
        call puts
        call ask_yes
        jc cancel
.fmt:
        ; 1) sistema: SYS unidad:
        mov dx, sys_path
        mov bx, sys_tail
        call run
        jc .syserr
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
        ; 3) disco duro: la partición, activa, y el MBR si no tiene
        ; (PARTIR /ARRANQUE:X:; en el disco de arranque pide escribir SI)
        cmp byte [fixed], 0
        je .snd
        mov dx, part_path
        mov bx, arr_tail
        call run
        jc .arrerr
        test al, al
        jz .snd
.arrerr:
        mov si, msg_arrerr
        call puts
        mov ax, 0x4C05
        int 0x21
.snd:
        ; 4) la comprobación del sonido, con el disco recién instalado
        cmp byte [nosnd], 0
        jne .end
        call sound_check
.end:
        mov si, msg_boot
        call puts
        cmp byte [guided], 0
        je .e2
        mov si, msg_reboot
        call puts
.e2:
        mov ax, 0x4C00
        int 0x21

cancel:
        mov si, msg_nothing
        call puts
        mov ax, 0x4C06
        int 0x21

; AL = letra de la unidad de destino
set_dst:
        mov [dst], al
        mov [tail_drv], al
        mov [msg_drv1], al
        mov [msg_drv2], al
        mov [msg_drv3], al
        mov [msg_drv4], al
        mov [aud_path], al
        mov [aud_d1], al
        mov [aud_d2], al
        mov [fmt_drv], al
        mov [arr_drv], al
        ret

; sin unidad: PARTIR /INSTALAR elige el disco duro (nunca el de arranque),
; ofrece crear la partición y devuelve su letra como nivel de error
guided_pick:
        mov dx, part_path
        mov bx, inst_tail
        call run
        jc .noprog
        cmp al, 'C'
        jb .none
        cmp al, 'Z'
        ja .none
        call set_dst
        ret
.none:
        test al, al
        jz cancel
        mov si, msg_nothing
        call puts
        mov ax, 0x4C01
        int 0x21
.noprog:
        mov si, msg_nopart
        call puts
        mov ax, 0x4C01
        int 0x21

; parámetros de la unidad de destino -> [fixed]; CF si no existe
drv_params:
        mov ax, 0x440D
        mov bl, [dst]
        sub bl, 'A' - 1
        mov cx, 0x0860
        mov dx, params
        int 0x21
        jc .r
        mov byte [fixed], 0
        cmp byte [params+1], 5
        jne .r
        mov byte [fixed], 1
        clc
.r:
        ret

; DX = programa, BX = su cola de órdenes -> AL = su nivel de error; CF si no
; se ha podido ejecutar
run:
        mov word [pblock], 0
        mov [pblock+2], bx
        mov [pblock+4], cs
        mov dword [pblock+6], 0
        mov dword [pblock+10], 0
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        jc .r
        mov ah, 0x4D
        int 0x21
        clc
.r:
        ret

; ¿S/N? (se escribe la respuesta y se pulsa Intro) -> CF=0 si es que sí
ask_yes:
        mov byte [lbuf], 8
        mov dx, lbuf
        mov ah, 0x0A
        int 0x21
        mov si, msg_crlf
        call puts
        cmp byte [lbuf+1], 0
        je .no
        mov al, [lbuf+2]
        and al, 0xDF
        cmp al, 'S'
        je .y
        cmp al, 'Y'
        je .y
.no:
        stc
        ret
.y:
        clc
        ret

; AUDICION /RAPIDA del disco instalado: lo aprendido, en su SONIDO.INI
; (AUDICION dice ella misma cómo ha ido; aquí, solo si no ha podido empezar)
sound_check:
        mov si, msg_snd
        call puts
        mov dx, aud_path
        mov bx, aud_tail
        call run
        jc .fail
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
        je .let
        cmp al, [dst]               ; (la que tiene ahora el disco duro)
        jne .n
.let:
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
guided      db 0
fixed       db 0
part_path   db "A:\HISPADOS\PARTIR.COM", 0
inst_tail   db 10, " /INSTALAR", 13
arr_tail    db 13, " /ARRANQUE:"
arr_drv     db "C:", 13
fmt_path    db "A:\HISPADOS\FORMATEA.COM", 0
fmt_tail    db fmt_te - fmt_tail - 1, " "
fmt_drv     db "C: /V:HISPADOS /AUTOTEST"
fmt_te      db 13
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
msg_usage   db "Uso: INSTALAR unidad: [/NOSONIDO]   o   INSTALAR [/NOSONIDO]", 13, 10
            db "Sin unidad busca el disco duro (nunca la memoria USB de arranque), ofrece", 13, 10
            db "crear la partición si no tiene y pregunta antes de seguir.", 13, 10
            db "Con unidad instala en esa (si no tiene formato, pregunta y se lo da).", 13, 10
            db "Al final comprueba el sonido (AUDICION /RAPIDA): lo que suena queda en", 13, 10
            db "unidad:\HISPADOS\SONIDO.INI y HSPSND lo usa desde el primer arranque.", 13, 10
            db "Esc salta ese paso; /NOSONIDO no lo hace.", 13, 10, 0
msg_same    db "No se puede instalar sobre la unidad de arranque.", 13, 10, 0
msg_nofmt   db "La unidad "
msg_drv1    db "C: no existe.", 13, 10
            db "Cree la partición con PARTIR y vuelva a escribir INSTALAR.", 13, 10, 0
msg_askfmt1 db 13, 10, "La unidad "
msg_drv3    db "C: no tiene formato: se le dará formato (se borra todo lo que", 13, 10
            db "tenga) y se instalará Hispa-DOS en ella.", 13, 10, 0
msg_askfmt2 db "¿Continuar (S/N)? ", 0
msg_askins1 db 13, 10, "Se va a instalar Hispa-DOS en la unidad "
msg_drv4    db "C:. Lo que ya tenga se conserva", 13, 10
            db "(solo se ponen o se cambian los archivos de Hispa-DOS).", 13, 10, 0
msg_askins2 equ msg_askfmt2
msg_fmterr  db "No se ha podido dar formato a la unidad: no se ha instalado nada.", 13, 10, 0
msg_nothing db "No se ha instalado nada.", 13, 10, 0
msg_nopart  db "No se encuentra \HISPADOS\PARTIR.COM en la unidad de arranque.", 13, 10, 0
msg_arrerr  db "Los archivos están copiados, pero el disco duro NO ha quedado listo para", 13, 10
            db "arrancar (partición activa y MBR). Pruebe con PARTIR: opción 2.", 13, 10, 0
msg_reboot  db "Quite la memoria USB (o el disquete) y reinicie el ordenador.", 13, 10, 0
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
params      resb 40
lbuf        resb 12
buf         resb BUFSZ
            resb 512
stack_top:
prog_end:
