; =============================================================================
;  Hispa-DOS - motor de copia de arbol de ficheros (compartido)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
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
;                  origen/destino por C: (para CONFIG.SYS y AUTOEXEC.BAT) y
;                  quitar las lineas que solo valen en el medio de instalacion
;                  (de una con "[SOLO-MEDIO" a una con "SOLO-MEDIO]": el
;                  primer menu del disquete, la memoria USB o el CD, con
;                  «Instalar Hispa-DOS en el disco duro»)
;    [solo_contar]=1  no copiar nada: solo contar en [ntotal] los ficheros
;                  que se copiarian (para la barra de progreso de SETUP)
;    [cs_aviso]    si no es 0, se llama antes de cada fichero (path_s y
;                  path_d ya puestos; debe conservar los registros y la DTA)
;    [seguro]=1    (ACTUALIZAR) no tocar el archivo de destino hasta tener el
;                  nuevo entero: se escribe en NOMBRE.$$$ y, si todo ha ido
;                  bien, el viejo pasa a NOMBRE.$$V, el nuevo a NOMBRE y se
;                  borra el viejo. Si algo falla, el viejo sigue en su sitio
;                  (asi un disco lleno no deja el equipo sin KERNEL.SYS)
;  Resultados:  [ncopied] copiados, [nfailed] fallidos, [nkept] conservados.
;
;  Este fichero NO lleva acentos ni enies en cadenas: se incluye sin pasar por
;  cp437 (los mensajes en pantalla van en el .asm que lo incluye).
;  No imprime nada: solo trabaja con ficheros (INT 21h).
; =============================================================================

%ifndef BUFSZ
BUFSZ       equ 16384           ; (ACTUALIZAR lo pone mas chico: le falta sitio)
%endif

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
        cmp byte [solo_contar], 0   ; [SETUP] solo contar
        je .real
        inc word [ntotal]
        jmp .next
.real:
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
        cmp word [cs_aviso], 0      ; [SETUP] el nombre y la barra
        je .noav
        call [cs_aviso]
.noav:
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
        mov byte [en_medio], 0
        mov dx, path_d
        cmp byte [seguro], 0
        je .dst
        mov si, path_t              ; [seguro] el nuevo, primero en NOMBRE.$$$
        mov al, '$'
        call cf_otro_nombre
        mov si, path_v              ; y el viejo pasara a NOMBRE.$$V
        mov al, 'V'
        call cf_otro_nombre
        mov dx, path_t
.dst:
        mov [cf_dest], dx
        mov ax, 0x4301
        xor cx, cx
        int 0x21                    ; quitar solo lectura de una copia anterior
        mov dx, path_s
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [hs], ax
        mov dx, [cf_dest]
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
        call quita_medio
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
        jc .borra                   ; al cerrar no se ha podido escribir todo
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        cmp byte [seguro], 0
        je .bien
        jmp cf_cambia               ; [seguro] el nuevo, en su sitio
.bien:
        clc
        ret
.both:
        mov bx, [hd]
        mov ah, 0x3E
        int 0x21
.borra:
        mov dx, [cf_dest]           ; no dejar un archivo a medias
        mov ah, 0x41
        int 0x21
.cl:
        mov bx, [hs]
        mov ah, 0x3E
        int 0x21
        stc
.r:
        ret

; [seguro] SI = buffer: path_d con la extension cambiada por "$$" + AL
; (KERNEL.SYS -> KERNEL.$$$ o KERNEL.$$V; sin extension, se le pone)
cf_otro_nombre:
        push di
        mov di, si
        push si
        push ax
        mov si, path_d
        call strcpy                 ; DI queda sobre el cero
        pop ax
        pop si
        mov bx, di
.b:
        cmp bx, si                  ; hacia atras: el punto del nombre
        je .sinext
        dec bx
        cmp byte [bx], '.'
        je .ext
        cmp byte [bx], '\'
        je .sinext
        cmp byte [bx], ':'
        je .sinext
        jmp .b
.sinext:
        mov bx, di
        mov byte [bx], '.'
.ext:
        mov byte [bx+1], '$'
        mov byte [bx+2], '$'
        mov [bx+3], al
        mov byte [bx+4], 0
        pop di
        ret

; [seguro] path_t (el nuevo, entero) -> path_d; el viejo, a path_v y fuera.
; CF=1 si no se ha podido (y entonces el viejo sigue en path_d)
cf_cambia:
        mov dx, path_v
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov dx, path_v
        mov ah, 0x41
        int 0x21                    ; un .$$V de otra vez
        mov byte [cf_habia], 0
        mov dx, path_d
        mov di, path_v
        mov ah, 0x56
        int 0x21                    ; el viejo, aparte (si lo hay)
        jc .nuevo
        mov byte [cf_habia], 1
.nuevo:
        mov dx, path_t
        mov di, path_d
        mov ah, 0x56
        int 0x21
        jc .vuelve
        cmp byte [cf_habia], 0
        je .ok
        mov dx, path_v
        mov ah, 0x41
        int 0x21                    ; ya no hace falta
.ok:
        clc
        ret
.vuelve:
        cmp byte [cf_habia], 0
        je .quita
        mov dx, path_v              ; el viejo, otra vez a su sitio
        mov di, path_d
        mov ah, 0x56
        int 0x21
.quita:
        mov dx, path_t
        mov ah, 0x41
        int 0x21
        stc
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

; [SETUP] en buf (CX bytes): quitar las lineas desde una que lleve
; "[SOLO-MEDIO" hasta una que lleve "SOLO-MEDIO]" (las dos incluidas) -> CX =
; los bytes que quedan. CONFIG.SYS y AUTOEXEC.BAT caben en una sola lectura
; (BUFSZ); [en_medio] sigue de una lectura a la siguiente.
quita_medio:
        push ax
        push bx
        push dx
        push si
        push di
        push es
        push ds
        pop es
        cld
        mov si, buf
        mov di, buf
        mov bx, buf
        add bx, cx
.linea:
        cmp si, bx
        jae .fin
        mov dx, si                  ; DX = principio de la linea
.busca:
        cmp si, bx
        jae .fl
        lodsb
        cmp al, 10
        jne .busca
.fl:
        call marca_medio            ; -> AL: 1 empieza, 2 acaba, 0 nada
        cmp al, 1
        jne .no1
        mov byte [en_medio], 1
        jmp .linea
.no1:
        cmp al, 2
        jne .no2
        mov byte [en_medio], 0
        jmp .linea
.no2:
        cmp byte [en_medio], 0
        jne .linea                  ; dentro de la zona: fuera
        push si
        mov cx, si
        sub cx, dx
        mov si, dx
        rep movsb
        pop si
        jmp .linea
.fin:
        mov cx, di
        sub cx, buf
        pop es
        pop di
        pop si
        pop dx
        pop bx
        pop ax
        ret
; la linea [DX, SI) -> AL = 1 si lleva "[SOLO-MEDIO", 2 si "SOLO-MEDIO]"
marca_medio:
        push bx
        push di
        mov di, dx
.l:
        mov bx, si
        sub bx, di
        cmp bx, 11
        jb .no
        push si
        push di
        mov si, s_medio
        mov cx, 10
        repe cmpsb
        pop di
        pop si
        jne .n
        mov al, 1
        cmp di, dx
        je .t
        cmp byte [di-1], '['
        je .r
.t:
        mov al, 2
        cmp byte [di+10], ']'
        je .r
.n:
        inc di
        jmp .l
.no:
        xor al, al
.r:
        pop di
        pop bx
        ret
s_medio     db "SOLO-MEDIO"
en_medio    db 0
solo_contar db 0
ntotal      dw 0
cs_aviso    dw 0

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
seguro      db 0                ; [seguro]: ver la cabecera
cf_habia    db 0
cf_dest     dw 0
path_t      times 128 db 0
path_v      times 128 db 0
; prefijo de origen dentro de la unidad (ASCIIZ, vacio por omision). INSTALAR lo
; deja vacio; ACTUALIZAR escribe aqui "\ACTUALIZA".
src_pre     times 68 db 0
