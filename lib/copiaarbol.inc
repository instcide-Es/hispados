; =============================================================================
;  Hispa-DOS - motor de copia de arbol de ficheros (compartido)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Copia conjuntos de ficheros (patron "\CARPETA\*.*") de una unidad (y, si se
;  quiere, de una CARPETA base dentro de ella) a otra unidad, respetando los
;  datos y ajustes del usuario (*.INI, *.REC) en el modo "actualizar".
;
;  Lo usan INSTALAR (copiar desde una unidad: origen = "X:") y ACTUALIZAR
;  (aplicar una actualizacion bajada por la red: origen = "C:\ACTUALIZA").
;
;  La ruta de origen se forma asi:   [src]:  +  [src_pre]  +  patron
;    - [src]      letra de la unidad de origen (p.ej. 'A')
;    - [src_pre]  prefijo ASCIIZ dentro de esa unidad, VACIO por omision
;                 (p.ej. "\ACTUALIZA"; sin barra final)
;    - patron     "\HISPADOS\*.*" (el que pasa el llamante en SI)
;  El destino se forma:   [dst]:  +  carpeta destino (DI del llamante)  +  nombre
;
;  Senales del llamante (como en INSTALAR):
;    [keep]=1      respetar CUALQUIER fichero que ya exista en el destino
;    [keepdata]=1  respetar solo *.INI y *.REC que ya existan (actualizar)
;    [fix]=1       en el contenido copiado, cambiar las letras de unidad A:/B:/
;                  origen/destino por C: (para CONFIG.SYS y AUTOEXEC.BAT)
;  Resultados:  [ncopied] copiados, [nfailed] fallidos, [nkept] conservados.
;
;  Este fichero NO lleva acentos ni enies en cadenas: se incluye sin pasar por
;  cp437 (los mensajes en pantalla van en el .asm que lo incluye).
;  No imprime nada: solo trabaja con ficheros (INT 21h).
; =============================================================================

BUFSZ       equ 16384

; DI = carpeta de destino ("\HISPADOS\"): crearla en la unidad de destino
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

; SI = patron relativo a la raiz ("\DRIVERS\*.*"), DI = carpeta destino
; ("\" o "\DRIVERS\"): copiar del origen (unidad + prefijo) al destino
copy_set:
        push si
        push di
        mov [cs_dir], di
        ; patron completo en el origen: "X:" + [src_pre] + patron
        mov bx, pat
        mov al, [src]
        mov [bx], al
        mov byte [bx+1], ':'
        add bx, 2
        ; prefijo de origen (vacio por omision; p.ej. "\ACTUALIZA")
        push si
        mov si, src_pre
.pre:
        lodsb
        test al, al
        jz .prefin
        mov [bx], al
        inc bx
        jmp .pre
.prefin:
        pop si
.p:
        lodsb
        mov [bx], al
        inc bx
        test al, al
        jnz .p
        ; carpeta de origen = patron sin el nombre
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
        ; respetar el que ya existe?
        cmp byte [keep], 0
        jne .chk_exist          ; keep: respetar cualquier archivo que exista
        cmp byte [keepdata], 0
        je .cp                  ; ni keep ni keepdata: copiar (sobrescribir)
        ; keepdata (ACTUALIZAR): respetar solo *.INI y *.REC que ya existan
        call is_user_data       ; CF=0 si la fuente es *.INI/*.REC
        jc .cp                  ; no es dato de usuario: sobrescribir
        call dest_exists        ; CF=0 si ya esta en el destino
        jc .cp                  ; no existe aun: instalarlo
        inc word [nkept]        ; existe: se conserva el del usuario
        jmp .next
.chk_exist:
        call dest_exists
        jnc .next               ; existe: respetar
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

; existe ya path_d en el destino? CF=0 si existe. Usa dta2 para no estropear
; la busqueda que recorre copy_set (deja la DTA otra vez en dta)
dest_exists:
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
        ret

; el nombre de la fuente (dta+0x1E) acaba en .INI o .REC? CF=0 si si. Son los
; datos y ajustes del usuario que ACTUALIZAR conserva (RED.INI, VENTANAS.INI,
; SONIDO.INI, CENTRO.INI, SOLITARI.REC...)
is_user_data:
        push si
        mov si, dta + 0x1E
.find:
        lodsb
        test al, al
        jz .no
        cmp al, '.'
        jne .find
        ; SI -> extension; tiene que ser justo tres letras
        cmp byte [si+3], 0
        jne .no
        mov al, [si]
        and al, 0xDF
        mov ah, [si+1]
        and ah, 0xDF
        mov bl, [si+2]
        and bl, 0xDF
        cmp al, 'I'             ; INI?
        jne .rec
        cmp ah, 'N'
        jne .rec
        cmp bl, 'I'
        jne .rec
        jmp .yes
.rec:
        cmp al, 'R'             ; REC?
        jne .no
        cmp ah, 'E'
        jne .no
        cmp bl, 'C'
        jne .no
.yes:
        pop si
        clc
        ret
.no:
        pop si
        stc
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
; tendra el disco al arrancar desde el (CONFIG.SYS y AUTOEXEC.BAT)
fix_drive:
        push cx
        push si
        mov si, buf
        dec cx
        jle .r
        mov ah, ' '                 ; AH = el caracter anterior
.l:
        cmp byte [si+1], ':'
        jne .n
        mov al, [si]
        and al, 0xDF
        cmp al, 'A'                 ; A: y B: (el disquete arranca con A:\, y
        je .let                     ; en un disco duro nunca valen; hay BIOS
        cmp al, 'B'                 ; que arrancan el USB-disquete como disco
        je .let                     ; duro, asi que la de arranque es otra)
        cmp al, [src]               ; la unidad de origen (USB, disquete...)
        je .let
        cmp al, [dst]               ; (la que tiene ahora el disco duro)
        jne .n
.let:
        ; solo si es una letra suelta: "Fecha:" o "Nota:" no son la unidad A:
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

; ---- datos compartidos (inicializados: fiables sin poner a cero la .bss) ----
; Se emiten en la seccion donde se incluye el fichero (la de codigo/datos del
; .COM). Los BUFERES grandes (pat, sdir, path_s, path_d, dta, dta2, buf) los
; declara el fichero que incluye, en su propia "section .bss" (asi no abultan
; el .COM). BUFSZ queda definido arriba para esa declaracion.
src         db 0                ; letra de la unidad de origen
dst         db 0                ; letra de la unidad de destino
keep        db 0
fix         db 0
keepdata    db 0                ; 1: conservar *.INI/*.REC del usuario que ya existan
ncopied     dw 0
nfailed     dw 0
nkept       dw 0
hs          dw 0
hd          dw 0
cs_dir      dw 0
; prefijo de origen dentro de la unidad (ASCIIZ, vacio por omision). INSTALAR lo
; deja vacio; ACTUALIZAR escribe aqui "\ACTUALIZA".
src_pre     times 68 db 0
