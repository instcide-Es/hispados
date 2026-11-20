; =============================================================================
;  Hispa-DOS WINCONF.COM - aplicar los cambios que propone el programa de
;  instalación de Windows 3.1x
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  WINCONF [carpeta de Windows] [/N] [/AUTO]
;     (sin carpeta)  \WINDOWS de la unidad de arranque
;     /N             no reiniciar al acabar (para las pruebas)
;     /AUTO          sin mensajes si no hay nada que hacer (AUTOEXEC.BAT lo
;                    llama así en cada arranque si hay C:\WINDOWS\WIN.COM)
;
;  El programa de instalación de Windows para Trabajo en Grupo 3.11 no toca
;  un CONFIG.SYS con menú (varias configuraciones, como el de Hispa-DOS): deja
;  lo que quería cambiar en CONFIG.WIN y AUTOEXEC.WIN, en la carpeta de
;  Windows («para preservar la integridad de la configuración del usuario»,
;  dice su INSTALAR.TXT). WINCONF lo pone donde va, como lo haría él con
;  MS-DOS 6.22 sin menú, y además:
;   - CONFIG.SYS: una opción «Windows» en el menú (la de omisión) con las
;     líneas del bloque [normal] (INCLUDE=NORMAL) y las que añade Windows
;     (SMARTDRV /DOUBLE_BUFFER, IFSHLP.SYS, STACKS...).
;   - AUTOEXEC.BAT: la línea PATH de Windows (con su carpeta) en lugar de la
;     nuestra, para todas las opciones; y un bloque :WINDOWS con el teclado,
;     el ratón (que se aparta solo mientras Windows está en marcha) y lo
;     demás que añade Windows (SMARTDRV, SET TEMP...). Sin HSPSND, HSPJOY ni
;     emuladores: con Windows, el sonido y lo demás son de sus controladores.
;   - SYSTEM.INI: la línea «device=vshare.386» queda comentada (VSHARE.386
;     escribe en posiciones fijas de los datos internos de MS-DOS: con
;     Hispa-DOS no puede cargarse y Windows avisaba en cada arranque).
;  Los anteriores quedan como CONFIG.SIN y AUTOEXEC.SIN; los .WIN pasan a
;  .APL (aplicados). Después reinicia: el AUTOEXEC.BAT en marcha ha cambiado
;  y el CONFIG.SYS nuevo vale desde el arranque.
;  Si ya no hay .WIN pero sí .APL (ya se aplicaron) y faltan sus líneas
;  (ACTUALIZAR pone un CONFIG.SYS y un AUTOEXEC.BAT nuevos), se vuelven a
;  poner. Una línea cuenta si es de Windows (lleva su carpeta, «:\WINDOWS», o
;  es STACKS=), no está en el archivo (sin mirar mayúsculas ni los espacios
;  de los extremos) y tampoco está desactivada con REM.
;  ERRORLEVEL: 0 hecho (o nada que hacer), 1 error.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

BMAX        equ 12288               ; tope de cada archivo leído
OMAX        equ 20480               ; tope del archivo nuevo
NVMAX       equ 32                  ; líneas nuevas como mucho

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        ; unidad de arranque
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [drv], dl
        ; argumentos: carpeta y /N
        mov si, 0x81
.arg:
        lodsb
        cmp al, ' '
        je .arg
        cmp al, 9
        je .arg
        cmp al, 13
        je .argd
        cmp al, '/'
        jne .dir
        lodsb
        call upcase
        cmp al, 'A'                 ; /AUTO: en silencio si no hay nada
        jne .on
        mov byte [opt_auto], 1
.as:
        lodsb                       ; (el resto de la palabra)
        cmp al, ' '
        ja .as
        dec si
        jmp .arg
.on:
        cmp al, 'N'
        jne usage
        mov byte [opt_n], 1
        jmp .arg
.dir:
        mov di, wdir
        mov cx, 64
.dc:
        stosb
        lodsb
        cmp al, ' '
        je .dd
        cmp al, 9
        je .dd
        cmp al, 13
        je .dd
        loop .dc
        jmp usage
.dd:
        dec si
        cmp byte [di-1], '\'        ; sin barra final
        jne .dz
        dec di
.dz:
        mov byte [di], 0
        jmp .arg
.argd:
        cmp byte [wdir], 0
        jne .hay
        mov di, wdir
        mov al, [drv]
        stosb
        mov si, s_windows
        call strcpy
.hay:
        cmp byte [opt_auto], 0
        jne .cfg
        call titulo
.cfg:
        ; ---- CONFIG.SYS ----
        mov si, s_cfgwin
        call win_path               ; -> path_w
        mov si, s_cfgsys
        mov di, path_a
        call root_path
        mov si, s_cfgsin
        mov di, path_b
        call root_path
        call load_pair
        jc .nocfg
        mov byte [cfg_menu], 0
        mov si, s_menu
        call find_hdr
        jc .cnm
        mov byte [cfg_menu], 1
.cnm:
        call collect
        call make_cfg
        call apply
        jc fail
.nocfg:
        jnc .aut
        ; sin CONFIG.WIN: ¿hay ya opción «Windows»?
        mov dx, path_a
        mov di, bufA
        call read_file
        jc .aut
        add ax, bufA
        mov [a_end], ax
        call mira_win
.aut:
        ; ---- AUTOEXEC.BAT ----
        mov si, s_autwin
        call win_path
        mov si, s_autbat
        mov di, path_a
        call root_path
        mov si, s_autsin
        mov di, path_b
        call root_path
        call load_pair
        jc .noaut
        call collect
        call make_aut
        call apply
        jc fail
.noaut:
        ; ---- SYSTEM.INI ----
        call vshare_off
        cmp byte [changed], 0
        jne .rein
        cmp byte [ini_mod], 0
        jne .fin0
        cmp byte [opt_auto], 0
        jne .fin0
        mov si, msg_nada
        call puts
.fin0:
        mov ax, 0x4C00
        int 0x21
.rein:
        mov ah, 0x0D                ; que todo quede escrito
        int 0x21
        cmp byte [opt_n], 0
        je .boot
        mov si, msg_hecho
        call puts
        mov ax, 0x4C00
        int 0x21
.boot:
        mov si, msg_reinicia
        call puts
        xor ax, ax                  ; 3 segundos
        mov es, ax
        mov bx, [es:0x46C]
.w:
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 55
        jb .w
        mov ah, 0x0D
        int 0x21
        mov ax, 0x4A10              ; SMARTDRV (si estuviera): vaciar
        mov bx, 1
        int 0x2F
        cli
        mov word [es:0x472], 0x1234 ; reinicio en caliente
        mov cx, 0xFFFF
.kw:
        in al, 0x64
        test al, 2
        loopnz .kw
        mov al, 0xFE                ; por el controlador del teclado
        out 0x64, al
        jmp 0xFFFF:0x0000

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
fail:
        mov si, msg_fail
        call puts
        mov si, path_a
        call puts
        call crlf
        mov ax, 0x4C01
        int 0x21

; -----------------------------------------------------------------------------
;  Rutas
; -----------------------------------------------------------------------------
; path_w = wdir + "\" + SI
win_path:
        push si
        mov si, wdir
        mov di, path_w
        call strcpy
        mov al, '\'
        stosb
        pop si
        jmp strcpy
; DI = "X:\" + SI
root_path:
        mov al, [drv]
        stosb
        mov ax, ':\'
        stosw
; SI -> DI (con el cero); DI queda en el cero
strcpy:
        lodsb
        stosb
        test al, al
        jnz strcpy
        dec di
        ret

; -----------------------------------------------------------------------------
;  Leer path_w (bufW) y path_a (bufA). CF si no hay .WIN (o no se puede leer
;  alguno: se dice).
; -----------------------------------------------------------------------------
load_pair:
        mov byte [de_apl], 0
        mov dx, path_w
        mov di, bufW
        call read_file
        jnc .w
        ; sin .WIN: el .APL de una vez anterior (una actualización de
        ; Hispa-DOS pone CONFIG.SYS y AUTOEXEC.BAT nuevos: se vuelve a poner
        ; lo de Windows)
        mov si, path_w
        mov di, path_x
        call strcpy
        mov dword [di-3], 'APL'
        mov dx, path_x
        mov di, bufW
        call read_file
        jc .no
        mov byte [de_apl], 1
.w:
        add ax, bufW
        mov [w_end], ax
        mov dx, path_a
        mov di, bufA
        call read_file
        jnc .ok
        mov word [a_end], bufA      ; (sin CONFIG.SYS o AUTOEXEC.BAT: vacío)
        cmp ax, 2
        je .r0
        cmp ax, 3
        je .r0
        mov si, msg_noleo
        call puts
        mov si, path_a
        call puts
        call crlf
.no:
        stc
        ret
.ok:
        add ax, bufA
        mov [a_end], ax
.r0:
        clc
        ret

; el título, una vez
titulo:
        cmp byte [tit_ya], 0
        jne .r
        mov byte [tit_ya], 1
        push si
        mov si, msg_title
        call puts
        pop si
.r:
        ret

; DX = ruta, DI = búfer -> AX = octetos (CF y AX = error si no se puede, o
; AX = 0FFFFh si no cabe)
read_file:
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov dx, di
        mov cx, BMAX
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        mov ah, 0x3E
        int 0x21
        pop ax
        popf
        jc .r
        cmp ax, BMAX
        jae .big
        clc
        ret
.big:
        mov ax, 0xFFFF
        stc
.r:
        ret

; DX = ruta, SI = datos, CX = octetos -> CF si falla
write_file:
        push cx
        push si
        mov ax, 0x4301              ; sin solo lectura
        xor cx, cx
        int 0x21
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        pop dx
        pop cx
        jc .r
        mov bx, ax
        mov ah, 0x40
        int 0x21
        jc .c
        cmp ax, cx
        jne .e
        mov ah, 0x3E
        int 0x21
        ret
.e:
        stc
.c:
        pushf
        mov ah, 0x3E
        int 0x21
        popf
.r:
        ret

; -----------------------------------------------------------------------------
;  Guardar: path_a -> path_b (la copia), out -> path_a, path_w -> .APL
; -----------------------------------------------------------------------------
apply:
        cmp byte [mod], 0
        jne .si
        clc
        ret
.si:
        call titulo
        mov si, msg_aplica
        call puts
        mov si, path_w
        cmp byte [de_apl], 0
        je .m
        mov si, path_x
.m:
        call puts
        call crlf
        mov byte [changed], 1
        cmp word [a_end], bufA
        je .nob
        mov dx, path_b
        mov si, bufA
        mov cx, [a_end]
        sub cx, bufA
        call write_file
        jc .r
.nob:
        mov dx, path_a
        mov si, obuf
        mov cx, [o_ptr]
        sub cx, obuf
        call write_file
        jc .r
        mov si, msg_puesto
        call puts
        mov si, path_a
        call puts
        mov si, msg_copia
        call puts
        mov si, path_b
        call puts
        mov si, msg_par
        call puts
        call crlf
; path_w (.WIN) -> .APL
.apl:
        cmp byte [de_apl], 0
        jne .ya
        mov si, path_w
        mov di, path_x
        call strcpy
        mov dword [di-3], 'APL'
        mov dx, path_x
        mov ah, 0x41
        int 0x21
        mov dx, path_w
        push ds
        pop es
        mov di, path_x
        mov ah, 0x56
        int 0x21
.ya:
        clc
.r:
        ret

; -----------------------------------------------------------------------------
;  Líneas. Una línea: SI = principio, CX = longitud sin CR/LF; next_line
;  pasa a la siguiente.
; -----------------------------------------------------------------------------
; SI = principio, BX = fin del búfer -> CX = longitud, DI = siguiente línea
next_line:
        mov di, si
.l:
        cmp di, bx
        jae .e
        mov al, [di]
        cmp al, 10
        je .lf
        cmp al, 13
        je .cr
        cmp al, 26                  ; ^Z: fin del texto
        je .z
        inc di
        jmp .l
.z:
        mov cx, di
        sub cx, si
        mov di, bx
        ret
.cr:
        mov cx, di
        sub cx, si
        inc di
        cmp di, bx
        jae .r
        cmp byte [di], 10
        jne .r
        inc di
.r:
        ret
.lf:
        mov cx, di
        sub cx, si
        inc di
        ret
.e:
        mov cx, di
        sub cx, si
        ret

; SI, CX -> sin espacios ni tabuladores en los extremos
trim:
        jcxz .r
.a:
        mov al, [si]
        cmp al, ' '
        je .ia
        cmp al, 9
        jne .b
.ia:
        inc si
        loop .a
        ret
.b:
        mov bx, si
        add bx, cx
        mov al, [bx-1]
        cmp al, ' '
        je .ib
        cmp al, 9
        jne .r
.ib:
        dec cx
        jnz .b
.r:
        ret

; ¿línea (SI, CX) igual a (DI, DX), sin mirar mayúsculas? ZF = 1 si sí
eq_line:
        cmp cx, dx
        jne .r
        jcxz .r
        push cx
        push si
        push di
.l:
        mov al, [si]
        call upcase
        mov ah, al
        mov al, [di]
        call upcase
        cmp al, ah
        jne .x
        inc si
        inc di
        loop .l
        cmp al, al                  ; (INC ha cambiado ZF) iguales
.x:
        pop di
        pop si
        pop cx
.r:
        ret

; ¿empieza la línea (SI, CX) por la cadena DI (en mayúsculas, con cero)?
; ZF = 1 si sí; DI queda detrás de lo comparado en la línea (en BX)
starts:
        push cx
        push si
.l:
        mov ah, [di]
        test ah, ah
        jz .si
        jcxz .no
        mov al, [si]
        call upcase
        cmp al, ah
        jne .no
        inc si
        inc di
        dec cx
        jmp .l
.si:
        mov bx, si
        pop si
        pop cx
        cmp al, al
        ret
.no:
        pop si
        pop cx
        or sp, sp                   ; ZF = 0
        ret

; ¿está la línea (SI, CX; ya sin espacios) en [BX = principio, BP = fin]?
; ZF = 1 si sí, tal cual o detrás de un REM (desactivada por el usuario).
; Conserva SI, CX, BX, DI y DX.
in_buf:
        push bx
        push di
        push dx
        mov [ib_s], si
        mov [ib_c], cx
        mov [ib_e], bp
        mov [ib_p], bx
.l:
        mov si, [ib_p]
        cmp si, [ib_e]
        jae .no
        mov bx, [ib_e]
        call next_line
        mov [ib_p], di
        call trim
        mov di, si
        mov dx, cx
        mov si, [ib_s]
        mov cx, [ib_c]
        call eq_line
        je .si
        ; ¿«REM línea»?
        mov si, di
        mov cx, dx
        mov di, s_rem
        call starts
        jne .l
        cmp cx, 3
        jbe .l
        mov al, [bx]
        cmp al, ' '
        je .rem
        cmp al, 9
        jne .l
.rem:
        sub cx, 3
        mov si, bx
        call trim
        mov di, si
        mov dx, cx
        mov si, [ib_s]
        mov cx, [ib_c]
        call eq_line
        je .si
        jmp .l
.si:
        mov si, [ib_s]
        mov cx, [ib_c]
        pop dx
        pop di
        pop bx
        cmp al, al
        ret
.no:
        mov si, [ib_s]
        mov cx, [ib_c]
        pop dx
        pop di
        pop bx
        or sp, sp
        ret

; -----------------------------------------------------------------------------
;  collect: las líneas de bufW que no están en bufA -> nv_ptr/nv_len (nv_n).
;  No cuentan las vacías, los REM ni las cabeceras de bloque [..].
; -----------------------------------------------------------------------------
collect:
        mov word [nv_n], 0
        mov byte [mod], 0
        mov si, bufW
.l:
        cmp si, [w_end]
        jae .r
        mov bx, [w_end]
        call next_line
        push di
        call trim
        jcxz .sig
        cmp byte [si], '['
        je .sig
        mov di, s_rem
        call starts
        jne .norem
        cmp cx, 3                   ; «REM» solo o «REM ...»
        je .sig
        mov al, [bx]
        cmp al, ' '
        je .sig
        cmp al, 9
        je .sig
.norem:
        call de_win
        jne .sig
        push si
        mov bx, bufA
        mov bp, [a_end]
        call in_buf
        pop si
        je .sig
        ; (repetida en el .WIN: una vez)
        mov bx, [nv_n]
        cmp bx, NVMAX
        jae .sig
        shl bx, 1
        mov [nv_ptr+bx], si
        mov [nv_len+bx], cx
        inc word [nv_n]
.sig:
        pop si
        jmp .l
.r:
        ret

; ¿es (SI, CX) una línea de Windows? ZF = 1 si lleva su carpeta («:\WINDOWS»,
; la de wdir sin la letra) o es STACKS=. Así no vuelve lo que el usuario haya
; quitado o cambiado después ni, desde un .APL, lo de una versión anterior
; de Hispa-DOS.
de_win:
        push si
        push cx
        mov di, s_stacks
        call starts
        je .r
        mov di, wdir + 1
.l:
        cmp cx, 2
        jb .no
        call starts_i
        je .r
        inc si
        dec cx
        jmp .l
.no:
        or sp, sp
.r:
        pop cx
        pop si
        ret
; como starts, con la cadena DI en mayúsculas o minúsculas (se pasan las dos
; por upcase)
starts_i:
        push cx
        push si
        push di
.l:
        mov ah, [di]
        test ah, ah
        jz .si
        jcxz .no
        mov al, ah
        call upcase
        mov ah, al
        mov al, [si]
        call upcase
        cmp al, ah
        jne .no
        inc si
        inc di
        dec cx
        jmp .l
.si:
        pop di
        pop si
        pop cx
        cmp al, al
        ret
.no:
        pop di
        pop si
        pop cx
        or sp, sp
        ret

; -----------------------------------------------------------------------------
;  Salida (out)
; -----------------------------------------------------------------------------
o_init:
        mov word [o_ptr], obuf
        ret
; SI, CX -> out
emit:
        push cx
        push si
        push di
        mov di, [o_ptr]
        mov ax, obuf + OMAX - 2
        sub ax, di
        cmp cx, ax
        jbe .c
        mov cx, ax
.c:
        rep movsb
        mov [o_ptr], di
        pop di
        pop si
        pop cx
        ret
; SI = cadena con cero -> out
emit_z:
        push cx
        push di
        mov di, si
        xor al, al
        mov cx, 0xFFFF
        repne scasb
        mov cx, di
        sub cx, si
        dec cx
        call emit
        pop di
        pop cx
        ret
emit_crlf:
        push si
        mov si, s_crlf
        call emit_z
        pop si
        ret
; SI, CX + CR LF
emit_line:
        call emit
        jmp emit_crlf
; las líneas nuevas (menos la de PATH ya puesta, nv_skip)
emit_new:
        pusha
        xor bx, bx
.l:
        cmp bx, [nv_n]
        jae .r
        push bx
        shl bx, 1
        mov si, [nv_ptr+bx]
        mov cx, [nv_len+bx]
        pop bx
        cmp si, [nv_skip]
        je .n
        push bx
        call emit_line
        pop bx
.n:
        inc bx
        jmp .l
.r:
        popa
        ret
; ¿la última de out acaba en salto de línea? si no, se pone
o_eol:
        mov di, [o_ptr]
        cmp di, obuf
        je .r
        cmp byte [di-1], 10
        je .r
        call emit_crlf
.r:
        ret

; ¿hay en bufA una línea «[nombre]» (SI = cadena con los corchetes, en
; mayúsculas)? CF = 0 si sí, con DI = su principio
find_hdr:
        mov [fh_s], si
        mov si, bufA
.l:
        cmp si, [a_end]
        jae .no
        mov bx, [a_end]
        call next_line
        push di
        push si
        call trim
        mov di, [fh_s]
        call starts
        pop di                      ; DI = principio de la línea
        pop si                      ; SI = la siguiente
        je .si
        jmp .l
.si:
        clc
        ret
.no:
        stc
        ret

; ¿tiene ya CONFIG.SYS (bufA) un bloque [windows]? -> cfg_win
mira_win:
        mov si, s_hwin
        call find_hdr
        jc .r
        mov byte [cfg_win], 1
.r:
        ret

; -----------------------------------------------------------------------------
;  CONFIG.SYS nuevo en out
; -----------------------------------------------------------------------------
make_cfg:
        call o_init
        mov word [nv_skip], 0
        call mira_win
        cmp word [nv_n], 0
        je .r
        mov byte [mod], 1
        cmp byte [cfg_menu], 0
        jne .menu
        ; sin menú: como Windows, al final
        mov si, bufA
        mov cx, [a_end]
        sub cx, si
        call emit
        call o_eol
        call emit_new
.r:
        ret
.menu:
        mov si, s_hwin
        call find_hdr
        jnc .yahay
        ; ---- una opción nueva: «Windows» ----
        ; la última MENUITEM del bloque [menu] y su MENUDEFAULT
        mov word [last_item], 0
        mov si, bufA
        mov byte [in_menu], 0
.b1:
        cmp si, [a_end]
        jae .b2
        mov bx, [a_end]
        call next_line
        push di
        push si
        call trim
        jcxz .b1n
        cmp byte [si], '['
        jne .b1i
        mov byte [in_menu], 0
        mov di, s_menu
        call starts
        jne .b1n
        mov byte [in_menu], 1
        jmp .b1n
.b1i:
        cmp byte [in_menu], 0
        je .b1n
        mov di, s_mitem
        call starts
        jne .b1n
        pop ax
        push ax
        mov [last_item], ax
.b1n:
        pop si
        pop si
        jmp .b1
.b2:
        ; copiar con los cambios
        mov si, bufA
        mov byte [in_menu], 0
.c1:
        cmp si, [a_end]
        jae .c9
        mov bx, [a_end]
        call next_line
        push di
        push si
        push cx
        call trim
        jcxz .ccopy
        cmp byte [si], '['
        jne .cm
        mov byte [in_menu], 0
        mov di, s_menu
        call starts
        jne .ccopy
        mov byte [in_menu], 1
        jmp .ccopy
.cm:
        cmp byte [in_menu], 0
        je .ccopy
        mov di, s_mdef
        call starts
        jne .ccopy
        ; MENUDEFAULT=WINDOWS y lo de detrás de la coma (los segundos)
        push si
        mov si, s_mdefw
        call emit_z
        pop si
        mov dx, si
        add dx, cx                  ; DX = fin de la línea recortada
.cs:
        cmp bx, dx
        jae .cfin
        cmp byte [bx], ','
        je .cc
        inc bx
        jmp .cs
.cc:
        mov si, bx
        mov cx, dx
        sub cx, bx
        call emit
.cfin:
        call emit_crlf
        pop cx
        pop si
        jmp .cnext
.ccopy:
        pop cx
        pop si
        call emit_line
        cmp si, [last_item]
        jne .cnext
        mov si, s_mitemw
        call emit_z
.cnext:
        pop si
        jmp .c1
.c9:
        call o_eol
        mov byte [cfg_win], 1
        mov si, s_cfgblk
        call emit_z
        mov si, s_hnormal
        call find_hdr
        jc .noinc
        mov si, s_incnorm
        call emit_z
.noinc:
        call emit_new
        ret
.yahay:
        ; ya hay [WINDOWS]: las nuevas, al final de ese bloque
        mov si, bufA
        mov byte [in_menu], 0       ; (aquí: 1 = dentro de [windows])
.y1:
        cmp si, [a_end]
        jae .y9
        mov bx, [a_end]
        call next_line
        push di
        push si
        push cx
        call trim
        jcxz .ycopy
        cmp byte [si], '['
        jne .ycopy
        cmp byte [in_menu], 1
        jne .yh
        call emit_new               ; acaba el bloque: aquí van
        mov byte [in_menu], 2
.yh:
        mov di, s_hwin
        call starts
        jne .ycopy
        cmp byte [in_menu], 0
        jne .ycopy
        mov byte [in_menu], 1
.ycopy:
        pop cx
        pop si
        call emit_line
        pop si
        jmp .y1
.y9:
        cmp byte [in_menu], 1
        jne .yr
        call o_eol
        call emit_new
.yr:
        ret

; -----------------------------------------------------------------------------
;  AUTOEXEC.BAT nuevo en out
; -----------------------------------------------------------------------------
; ¿es (SI, CX, ya recortada) una línea PATH? ZF = 1 si sí
is_path:
        mov di, s_setpath
        call starts
        je .r
        mov di, s_path
        call starts
        jne .r
        cmp cx, 4
        je .r
        mov al, [bx]
        cmp al, ' '
        je .r
        cmp al, '='
        je .r
        cmp al, ';'
        je .r
        cmp al, 9
.r:
        ret
; ¿línea con la orden AL (DI = su nombre)? para TECLADO y HSPRATON: la línea
; no es un REM y lleva el nombre
has_word:
        push si
        push cx
        mov [hw_w], di
        mov di, s_rem
        call starts
        je .no
.l:
        jcxz .no
        mov di, [hw_w]
        call starts
        je .si
        inc si
        dec cx
        jmp .l
.si:
        pop cx
        pop si
        cmp al, al
        ret
.no:
        pop cx
        pop si
        or sp, sp
        ret

make_aut:
        call o_init
        mov word [nv_skip], 0
        cmp word [nv_n], 0
        jne .hay
        ret
.hay:
        mov byte [mod], 1
        ; ¿alguna nueva es PATH? -> nv_skip (va en lugar de la nuestra)
        xor bx, bx
.p1:
        cmp bx, [nv_n]
        jae .p2
        push bx
        shl bx, 1
        mov si, [nv_ptr+bx]
        mov cx, [nv_len+bx]
        pop bx
        push bx
        call is_path
        pop bx
        jne .p1n
        mov [nv_skip], si
        mov [nv_skipl], cx
        jmp .p2
.p1n:
        inc bx
        jmp .p1
.p2:
        ; las líneas del teclado y del ratón (para el bloque :WINDOWS), la
        ; última IF "%CONFIG%"==, :WINDOWS y :COMUN
        mov word [l_tecl], 0
        mov word [l_raton], 0
        mov word [last_if], 0
        mov byte [has_lbl], 0
        mov byte [has_comun], 0
        mov si, bufA
.s1:
        cmp si, [a_end]
        jae .s2
        mov bx, [a_end]
        call next_line
        push di
        call trim
        jcxz .s1n
        mov di, s_ifcfg
        call starts
        jne .s1a
        mov [last_if], si
.s1a:
        mov di, s_lblwin
        call starts
        jne .s1b
        cmp cx, 8
        jne .s1b
        mov byte [has_lbl], 1
.s1b:
        mov di, s_lblcom
        call starts
        jne .s1c
        cmp cx, 6
        jne .s1c
        mov byte [has_comun], 1
.s1c:
        cmp word [l_tecl], 0
        jne .s1d
        mov di, s_teclado
        call has_word
        jne .s1d
        call quita_lh
        mov [l_tecl], si
        mov [l_teclc], cx
.s1d:
        cmp word [l_raton], 0
        jne .s1n
        mov di, s_hspraton
        call has_word
        jne .s1n
        call quita_lh
        mov [l_raton], si
        mov [l_ratonc], cx
.s1n:
        pop si
        jmp .s1
.s2:
        ; con opción «Windows» en CONFIG.SYS (y el AUTOEXEC.BAT por opciones)
        mov byte [conblk], 0
        cmp byte [cfg_win], 0
        je .s3
        cmp word [last_if], 0
        je .s3
        mov byte [conblk], 1
.s3:
        mov byte [puesto], 0        ; 1 = PATH ya cambiada
        mov byte [en_lbl], 0        ; 1 = dentro de :WINDOWS (ya existía)
        mov si, bufA
.c1:
        cmp si, [a_end]
        jae .c9
        mov bx, [a_end]
        call next_line
        push di
        push si
        push cx
        call trim
        test cx, cx
        jz .copy
        ; PATH: la de Windows, una vez
        cmp word [nv_skip], 0
        je .np
        cmp byte [puesto], 0
        jne .np
        call is_path
        jne .np
        mov byte [puesto], 1
        pop cx
        pop si
        push si
        push cx
        mov si, [nv_skip]
        mov cx, [nv_skipl]
        call emit_line
        cmp byte [conblk], 0
        jne .pskip
        call emit_new               ; sin opciones: las demás, detrás
.pskip:
        pop cx
        pop si
        jmp .next
.np:
        cmp byte [conblk], 0
        je .copy
        ; :COMUN sin :WINDOWS: el bloque, antes
        cmp byte [has_lbl], 0
        jne .nl
        mov di, s_lblcom
        call starts
        jne .copy
        cmp cx, 6
        jne .copy
        call blk_win
        jmp .copy
.nl:
        ; ya hay :WINDOWS: las nuevas, antes de su primer GOTO
        mov di, s_lblwin
        call starts
        jne .nl2
        cmp cx, 8
        jne .nl2
        mov byte [en_lbl], 1
        jmp .copy
.nl2:
        cmp byte [en_lbl], 1
        jne .copy
        mov di, s_goto
        call starts
        jne .copy
        call emit_new
        mov byte [en_lbl], 2
.copy:
        pop cx
        pop si
        call emit_line
        ; tras la última IF "%CONFIG%"==, la de WINDOWS
        cmp byte [conblk], 0
        je .next
        cmp byte [has_lbl], 0
        jne .next
        push si
        push cx
        call trim
        cmp si, [last_if]
        pop cx
        pop si
        jne .next
        mov si, s_ifwin
        call emit_z
.next:
        pop si
        jmp .c1
.c9:
        call o_eol
        ; sin PATH en el archivo: la de Windows, al final
        cmp word [nv_skip], 0
        je .c91
        cmp byte [puesto], 0
        jne .c91
        mov si, [nv_skip]
        mov cx, [nv_skipl]
        call emit_line
        mov byte [puesto], 1
        cmp byte [conblk], 0
        jne .c91
        call emit_new
.c91:
        cmp byte [conblk], 0
        jne .c92
        cmp byte [puesto], 0
        jne .c9r
        call emit_new               ; sin opciones ni PATH: al final
.c9r:
        ret
.c92:
        cmp byte [has_lbl], 0
        jne .c93
        cmp byte [has_comun], 0
        jne .c9r
        call blk_win                ; sin :COMUN: el bloque, al final
        ret
.c93:
        cmp byte [en_lbl], 1        ; :WINDOWS sin GOTO: al final
        jne .c9r
        call emit_new
        ret

; quitar «LH »/«LOADHIGH » del principio de (SI, CX)
quita_lh:
        mov di, s_lh
        call starts
        jne .r
        sub cx, 3
        mov si, bx
        call trim
.r:
        ret

; el bloque :WINDOWS
blk_win:
        push si
        push cx
        mov si, s_blk1
        call emit_z
        mov si, [l_tecl]
        test si, si
        jz .b1
        mov cx, [l_teclc]
        call emit_line
.b1:
        mov si, [l_raton]
        test si, si
        jz .b2
        mov cx, [l_ratonc]
        call emit_line
.b2:
        call emit_new
        mov si, s_blk2
        call emit_z
        pop cx
        pop si
        ret

; -----------------------------------------------------------------------------
;  SYSTEM.INI: «device=vshare.386» -> «;device=vshare.386» (con una nota)
; -----------------------------------------------------------------------------
vshare_off:
        mov si, s_sysini
        call win_path
        mov dx, path_w
        mov di, bufA
        call read_file
        jc .r
        add ax, bufA
        mov [a_end], ax
        call o_init
        mov byte [mod], 0
        mov si, bufA
.l:
        cmp si, [a_end]
        jae .e
        mov bx, [a_end]
        call next_line
        push di
        push si
        push cx
        call trim
        mov di, s_vshare
        call starts
        jne .c
        ; «device=vshare.386» y nada más (salvo espacios)
        push si
        mov si, bx
        mov ax, cx
        sub ax, 17
        mov cx, ax
        call trim
        pop si
        jcxz .x
        jmp .c
.x:
        mov byte [mod], 1
        mov si, s_vshoff
        call emit_z
        pop cx
        pop si
        push si
        push cx
        mov al, ';'
        mov di, [o_ptr]
        stosb
        mov [o_ptr], di
.c:
        pop cx
        pop si
        call emit_line
        pop si
        jmp .l
.e:
        cmp byte [mod], 0
        je .r
        mov byte [ini_mod], 1
        mov dx, path_w
        mov si, obuf
        mov cx, [o_ptr]
        sub cx, obuf
        call write_file
        jc .r
        call titulo
        mov si, msg_vshare
        call puts
.r:
        ret

%include "util.inc"

s_windows   db ":\WINDOWS", 0
s_cfgwin    db "CONFIG.WIN", 0
s_autwin    db "AUTOEXEC.WIN", 0
s_sysini    db "SYSTEM.INI", 0
s_cfgsys    db "CONFIG.SYS", 0
s_cfgsin    db "CONFIG.SIN", 0
s_autbat    db "AUTOEXEC.BAT", 0
s_autsin    db "AUTOEXEC.SIN", 0
s_menu      db "[MENU]", 0
s_hwin      db "[WINDOWS]", 0
s_hnormal   db "[NORMAL]", 0
s_mitem     db "MENUITEM", 0
s_mdef      db "MENUDEFAULT", 0
s_mdefw     db "MENUDEFAULT=WINDOWS", 0
s_mitemw    db "MENUITEM=WINDOWS,Windows 3.1x (sin los emuladores de Hispa-DOS)", 13, 10, 0
s_cfgblk    db 13, 10, "[windows]", 13, 10
            db "REM ---- Windows 3.1x (lo ha puesto WINCONF con lo que pedía su programa", 13, 10
            db "REM de instalación, en \WINDOWS\CONFIG.WIN): lo de «Normal», sin", 13, 10
            db "REM emuladores, y lo que añade Windows. ----", 13, 10, 0
s_incnorm   db "INCLUDE=NORMAL", 13, 10, 0
s_rem       db "REM", 0
s_stacks    db "STACKS", 0
s_lh        db "LH ", 0
s_path      db "PATH", 0
s_setpath   db "SET PATH", 0
s_ifcfg     db 'IF "%CONFIG%"==', 0
s_ifwin     db 'IF "%CONFIG%"=="WINDOWS" GOTO WINDOWS', 13, 10, 0
s_lblwin    db ":WINDOWS", 0
s_lblcom    db ":COMUN", 0
s_goto      db "GOTO", 0
s_teclado   db "TECLADO", 0
s_hspraton  db "HSPRATON", 0
s_blk1      db ":WINDOWS", 13, 10
            db "REM ---- Windows 3.1x (lo ha puesto WINCONF con lo que pedía su programa de", 13, 10
            db "REM instalación, en \WINDOWS\AUTOEXEC.WIN). De Hispa-DOS, solo el teclado y", 13, 10
            db "REM el ratón (se aparta solo mientras Windows está en marcha): ni sonido, ni", 13, 10
            db "REM mandos, ni emuladores; el hardware es de los controladores de Windows. ----", 13, 10, 0
s_blk2      db "GOTO COMUN", 13, 10, 13, 10, 0
s_vshare    db "DEVICE=VSHARE.386", 0
s_vshoff    db "; Hispa-DOS (WINCONF): VSHARE.386 no puede cargarse con este DOS (escribe en", 13, 10
            db "; los datos internos de MS-DOS); sin él, Windows funciona igual.", 13, 10, 0
s_crlf      db 13, 10, 0

msg_title   db "WINCONF - cambios de Windows 3.1x para Hispa-DOS", 13, 10, 0
msg_usage   db "Uso: WINCONF [carpeta de Windows] [/N] [/AUTO]", 13, 10
            db "  Pone en CONFIG.SYS y AUTOEXEC.BAT lo que dejó el programa de instalación", 13, 10
            db "  de Windows en CONFIG.WIN y AUTOEXEC.WIN, y reinicia (/N: sin reiniciar).", 13, 10
            db "  Si ya los aplicó (CONFIG.APL...), los vuelve a poner si faltan (tras una", 13, 10
            db "  actualización de Hispa-DOS). /AUTO: sin mensajes si no hay nada que hacer.", 13, 10, 0
msg_aplica  db "Cambios propuestos por Windows: ", 0
msg_noleo   db "No se puede leer ", 0
msg_puesto  db "Puesto al día: ", 0
msg_copia   db " (el anterior queda como ", 0
msg_par     db ")", 0
msg_vshare  db "SYSTEM.INI: desactivada la línea device=vshare.386.", 13, 10, 0
msg_nada    db "No hay cambios de Windows pendientes.", 13, 10, 0
msg_hecho   db "Hecho. Los cambios valen desde el próximo arranque.", 13, 10, 0
msg_reinicia db "Hecho. Hispa-DOS se reinicia para usarlos (en el menú, «Windows»)...", 13, 10, 0
msg_fail    db "Error: no se puede escribir ", 0

        section .bss
bss_start:
drv         resb 1
opt_n       resb 1
opt_auto    resb 1
de_apl      resb 1
tit_ya      resb 1
cfg_menu    resb 1
cfg_win     resb 1
changed     resb 1
ini_mod     resb 1
mod         resb 1
in_menu     resb 1
has_lbl     resb 1
has_comun   resb 1
conblk      resb 1
puesto      resb 1
en_lbl      resb 1
            resb 1
a_end       resw 1
w_end       resw 1
o_ptr       resw 1
nv_n        resw 1
nv_skip     resw 1
nv_skipl    resw 1
last_item   resw 1
last_if     resw 1
l_tecl      resw 1
l_teclc     resw 1
l_raton     resw 1
l_ratonc    resw 1
ib_s        resw 1
ib_c        resw 1
ib_e        resw 1
ib_p        resw 1
fh_s        resw 1
hw_w        resw 1
nv_ptr      resw NVMAX
nv_len      resw NVMAX
wdir        resb 80
path_w      resb 96
path_a      resb 16
path_b      resb 16
path_x      resb 96
u_numbuf    resb 16
u_lbuf      resb 84
bufA        resb BMAX
bufW        resb BMAX
obuf         resb OMAX
            resb 1024
stack_top:
