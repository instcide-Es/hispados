; =============================================================================
;  HSPPMAN.SYS - gestor de protocolos NDIS 2 (PROTMAN$) de Hispa-DOS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  CONFIG.SYS:  DEVICE=C:\HISPADOS\HSPPMAN.SYS [/I:carpeta]
;  (antes de los drivers NDIS 2 de las tarjetas, los «MAC»)
;
;  Hecho con la especificación pública 3Com/Microsoft «LAN Manager Network
;  Driver Interface Specification 2.0.1» (1990), capítulo 5 y apéndice B, y la
;  Ralf Brown's Interrupt List (INT 21h 4402h): sin código de nadie.
;  - Al cargarse lee PROTOCOL.INI (de /I:carpeta o de \LANMAN en la unidad de
;    arranque) y deja la «imagen de la configuración»: una lista de módulos
;    (ModuleConfig) con sus claves (KeywordEntry) y sus valores (Param).
;  - Contesta por IOCTL (INT 21h 4402h sobre PROTMAN$, bloque de 14 octetos)
;    y por su entrada directa (ProtManEntry, Pascal lejano):
;      1 GetProtocolManagerInfo, 2 RegisterModule, 3 BindAndStart (solo por
;      la entrada directa), 4 GetProtocolManagerLinkage, 5 GetProtocolIniPath,
;      9 BindStatus (no), 0Ah RegisterStatus.
;  - BindAndStart: a cada protocolo registrado con su lista de uniones le pide
;    InitiateBind con la tabla de cada MAC de la lista (y el protocolo hace
;    el Bind con el MAC).
; =============================================================================
        cpu 386
        bits 16
        org 0

NMOD        equ 8                   ; módulos registrados a la vez
IMG_MAX     equ 6144                ; la imagen de PROTOCOL.INI, como mucho
INI_MAX     equ 8192                ; PROTOCOL.INI, como mucho (al cargar)

header:
        dw 0xFFFF, 0xFFFF
        dw 0xC000                   ; carácter, con IOCTL
        dw strategy
        dw interrupt
        db "PROTMAN$"

req         dd 0
img_ptr     dd 0                    ; lo que devuelve GetProtocolManagerInfo
ini_ruta    times 80 db 0           ; la ruta de PROTOCOL.INI
nmod        dw 0
mods        times NMOD*8 db 0       ; por módulo: su tabla común y sus uniones
arrancado   db 0

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        pushad
        push ds
        push es
        push cs
        pop ds
        les bx, [req]
        mov al, [es:bx+2]
        cmp al, 0
        je init
        cmp al, 3                   ; IOCTL de entrada
        jne .hecho
        push es
        push bx
        les di, [es:bx+0x0E]        ; el bloque de petición
        call peticion_ioctl
        pop bx
        pop es
.hecho:
        mov word [es:bx+3], 0x0100
        pop es
        pop ds
        popad
        retf

; ES:DI = bloque (Opcode, Status, Pointer1, Pointer2, Word1)
peticion_ioctl:
        mov ax, [es:di]
        cmp ax, 3                   ; BindAndStart: por IOCTL no (5.1)
        je .nofun
        call atender
        mov [es:di+2], ax
        ret
.nofun:
        mov word [es:di+2], 8       ; INVALID_FUNCTION
        ret

; ProtManEntry(ReqBlock dd, TargetDS dw) -> AX: [sp+4] DS, [sp+6] bloque
entrada:
        push bp
        mov bp, sp
        push ds
        push es
        push bx
        push cx
        push si
        push di
        push cs
        pop ds
        les di, [bp+8]
        call atender
        mov [es:di+2], ax
        pop di
        pop si
        pop cx
        pop bx
        pop es
        pop ds
        pop bp
        retf 6

; ES:DI = bloque -> AX = estado (DS = el nuestro)
atender:
        mov ax, [es:di]
        cmp ax, 1
        je .info
        cmp ax, 2
        je .registrar
        cmp ax, 3
        je .arrancar
        cmp ax, 4
        je .enlace
        cmp ax, 5
        je .ruta
        cmp ax, 0x0A
        je .regestado
        mov ax, 8                   ; INVALID_FUNCTION (también 9, BindStatus)
        ret
.info:
        mov eax, [img_ptr]
        mov [es:di+4], eax
        mov word [es:di+12], 0x0102 ; NDIS 2.01 (BCD: mayor abajo, menor arriba)
        xor ax, ax
        ret
.enlace:
        mov word [es:di+4], entrada
        mov [es:di+6], cs
        mov [es:di+12], cs
        xor ax, ax
        ret
.ruta:
        ; Pointer1 = búfer, Word1 = su tamaño
        push es
        push di
        mov cx, [es:di+12]
        les di, [es:di+4]
        mov si, ini_ruta
.rl:
        jcxz .rcorta
        lodsb
        stosb
        dec cx
        test al, al
        jnz .rl
        pop di
        pop es
        xor ax, ax
        ret
.rcorta:
        pop di
        pop es
        mov ax, 0x0D                ; BUFFER_TOO_SMALL
        ret
.regestado:
        ; ¿está registrado el módulo de nombre Pointer2? (0 sí, 2Fh no)
        push es
        push di
        lfs si, [es:di+8]
        call buscar_nombre
        pop di
        pop es
        mov ax, 0x2F                ; INFO_NOT_FOUND
        jc .rr
        xor ax, ax
.rr:
        ret
.registrar:
        ; Pointer1 = su tabla común, Pointer2 = sus uniones (o 0)
        push es
        push di
        lfs si, [es:di+4]
        lea si, [si+0x0C]
        call buscar_nombre          ; ¿ya está?
        pop di
        pop es
        jc .nuevo
        mov ax, 0x2C                ; ALREADY_REGISTERED
        ret
.nuevo:
        mov bx, [nmod]
        cmp bx, NMOD
        jb .cabe
        mov ax, 6                   ; OUT_OF_RESOURCE
        ret
.cabe:
        shl bx, 3
        mov eax, [es:di+4]
        mov [mods+bx], eax
        mov eax, [es:di+8]
        mov [mods+bx+4], eax
        inc word [nmod]
        ; el número de módulo (+20h de su tabla): 1, 2, 3...
        push es
        les si, [es:di+4]
        mov ax, [nmod]
        mov [es:si+0x20], ax
        pop es
        xor ax, ax
        ret
.arrancar:
        cmp byte [arrancado], 0
        je .ar1
        mov ax, 0x20                ; ALREADY_STARTED
        ret
.ar1:
        mov byte [arrancado], 1
        ; FailingModules (Pointer1): los dos nombres a 0 si sale bien
        push es
        push di
        les di, [es:di+4]
        mov cx, 32
        xor al, al
        rep stosb
        pop di
        pop es
        call unir_todo
        ret

; FS:SI = nombre (ASCIZ) -> BX = índice*8 del módulo con ese nombre; CF si no
buscar_nombre:
        xor bx, bx
.l:
        mov ax, bx
        shr ax, 3
        cmp ax, [nmod]
        jae .no
        push es
        push si
        les di, [mods+bx]
        lea di, [di+0x0C]
        mov cx, 16
.c:
        mov al, [fs:si]
        cmp al, [es:di]
        jne .dist
        test al, al
        jz .igual
        inc si
        inc di
        loop .c
.igual:
        pop si
        pop es
        clc
        ret
.dist:
        pop si
        pop es
        add bx, 8
        jmp .l
.no:
        stc
        ret

; ES:DI = bloque de BindAndStart -> AX. Para cada módulo con uniones, cada
; nombre de su lista: InitiateBind(0, tabla del MAC, ¿última?, 1, su DS)
unir_todo:
        push es
        push di
        xor bx, bx
        mov word [estado_u], 0
.mod:
        mov ax, bx
        shr ax, 3
        cmp ax, [nmod]
        jae .fin
        cmp dword [mods+bx+4], 0
        je .sig
        mov [u_prot], bx
        mov eax, [mods+bx+4]        ; NumBindings, y nombres de 16
        mov [u_lista], eax
        lfs si, [u_lista]
        mov cx, [fs:si]
        mov [u_n], cx
        add word [u_lista], 2
.un:
        cmp word [u_n], 0
        je .sigp
        lfs si, [u_lista]
        call buscar_nombre          ; -> BX = el MAC
        jc .nomac
        les di, [mods+bx]           ; la tabla del MAC
        mov bx, [u_prot]
        lfs si, [mods+bx]           ; la tabla del protocolo
        push dword 0                ; Param1: relleno
        push es                     ; Param2: la tabla del MAC
        push di
        push word 0                 ; Param3: LastBind
        push word 1                 ; InitiateBind
        push word [fs:si+0x22]      ; DS del protocolo
        call far [fs:si+0x24]
        push cs
        pop ds
        test ax, ax
        jz .bien
        mov [estado_u], ax
        jmp .bien
.nomac:
        mov word [estado_u], 0x21   ; INCOMPLETE_BINDING
.bien:
        add word [u_lista], 16
        dec word [u_n]
        jmp .un
.sigp:
        mov bx, [u_prot]
.sig:
        add bx, 8
        jmp .mod
.fin:
        pop di
        pop es
        mov ax, [estado_u]
        ret

u_lista     dd 0
u_n         dw 0
estado_u    dw 0
u_prot      dw 0

; la imagen de PROTOCOL.INI (se llena al cargar)
            align 4
imagen:     times IMG_MAX db 0

; =============================================================================
;  INIT: leer PROTOCOL.INI y montar la imagen (lo de aquí abajo se pisa con la
;  imagen; el búfer del archivo va detrás de ella, más allá del corte)
; =============================================================================
init:
        ; la ruta: /I:carpeta o \LANMAN de la unidad de arranque
        push es
        push bx
        lds si, [es:bx+0x12]        ; la línea del CONFIG.SYS
        push cs
        pop es
        mov di, ini_ruta
.bs:
        lodsb
        cmp al, 13
        je .sini
        cmp al, 10
        je .sini
        test al, al
        jz .sini
        cmp al, '/'
        jne .bs
        mov al, [si]
        and al, 0xDF
        cmp al, 'I'
        jne .bs
        cmp byte [si+1], ':'
        jne .bs
        add si, 2
.cp:
        lodsb
        cmp al, ' '
        jbe .cpf
        stosb
        cmp di, ini_ruta + 64
        jb .cp
.cpf:
        jmp .barra
.sini:
        push cs
        pop ds
        mov ax, 0x3305
        int 0x21
        mov al, dl
        add al, 'A' - 1
        stosb
        mov si, s_lanman
.cl:
        lodsb
        stosb
        test al, al
        jnz .cl
        dec di
.barra:
        push cs
        pop ds
        cmp byte [di-1], '\'
        je .nb
        mov al, '\'
        stosb
.nb:
        mov si, s_ini
.ci:
        lodsb
        stosb
        test al, al
        jnz .ci
        ; leer el archivo detrás de la imagen
        mov dx, ini_ruta
        mov ax, 0x3D00
        int 0x21
        jc .sinarchivo
        mov bx, ax
        mov dx, ini_buf
        mov cx, INI_MAX - 1
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        mov ah, 0x3E
        int 0x21
        pop cx
        popf
        jc .sinarchivo
        mov si, ini_buf
        add si, cx
        mov byte [si], 0
        call montar_imagen
        jc .malini
        mov dword [img_ptr], 0
        cmp word [img_primero], 0
        je .listo
        mov word [img_ptr], imagen
        mov [img_ptr+2], cs
        jmp .listo
.malini:
        mov dx, m_malini
        mov ah, 9
        int 0x21
        jmp .listo
.sinarchivo:
        mov dx, m_noini
        mov ah, 9
        int 0x21
.listo:
        mov dx, m_titulo
        mov ah, 9
        int 0x21
        pop bx
        pop es
        mov ax, [img_fin]
        mov word [es:bx+0x0E], ax   ; el corte: el final de la imagen
        mov [es:bx+0x10], cs
        mov word [es:bx+3], 0x0100
        pop es
        pop ds
        popad
        retf

; -----------------------------------------------------------------------------
;  De PROTOCOL.INI (en imagen+IMG_MAX, acabado en 0) a la imagen (en imagen).
;  [SECCIÓN] abre un módulo; CLAVE = valor, valor... una clave. ';' en la
;  primera columna: comentario. Números: decimales o 0x hexadecimales;
;  cadenas: lo demás (o entre comillas). Nombres en mayúsculas, de 15 letras.
;  -> CF si no cabe o hay una línea rota
; -----------------------------------------------------------------------------
montar_imagen:
        push cs
        pop es
        mov si, ini_buf
        mov di, imagen
        mov word [img_primero], 0
        mov word [mod_act], 0
        mov word [kw_ant], 0
.linea:
        call saltar_blancos
        lodsb
        test al, al
        jz .fin
        cmp al, 13
        je .linea
        cmp al, 10
        je .linea
        cmp al, ';'
        je .coment
        cmp al, '['
        je .seccion
        ; una clave
        cmp word [mod_act], 0
        je .roto                    ; clave sin sección
        dec si
        call nueva_clave
        jc .roto
        jmp .linea
.coment:
        call fin_de_linea
        jmp .linea
.seccion:
        ; ModuleConfig: siguiente, anterior, nombre[16]
        lea ax, [di+24]
        cmp ax, imagen + IMG_MAX - 64
        ja .roto
        mov dword [di], 0
        mov dword [di+4], 0
        mov bx, [mod_act]
        test bx, bx
        jz .prim
        mov [bx], di                ; el anterior apunta a este
        mov [bx+2], cs
        mov [di+4], bx
        mov [di+6], cs
        jmp .nom
.prim:
        mov [img_primero], di
.nom:
        mov [mod_act], di
        mov word [kw_ant], 0
        push di
        add di, 8
        mov cx, 15
.n:
        lodsb
        cmp al, ']'
        je .nf
        cmp al, 13
        jbe .rotop
        call mayus
        stosb
        loop .n
.nf:
        xor al, al
        rep stosb                   ; (CX que quede + el 0)
        stosb
        pop di
        add di, 24
        call fin_de_linea
        jmp .linea
.rotop:
        pop di
.roto:
        stc
        ret
.fin:
        mov [img_fin], di
        clc
        ret

; DS:SI = principio de «CLAVE = valores», ES:DI = donde va -> DI detrás
nueva_clave:
        lea ax, [di+26]
        cmp ax, imagen + IMG_MAX - 128
        ja .no
        mov dword [di], 0
        mov dword [di+4], 0
        mov bx, [kw_ant]
        test bx, bx
        jz .prim
        mov [bx], di
        mov [bx+2], cs
        mov [di+4], bx
        mov [di+6], cs
.prim:
        mov [kw_ant], di
        mov bp, di                  ; la entrada
        add di, 8
        mov cx, 15
.k:
        lodsb
        cmp al, '='
        je .kf
        cmp al, ' '
        je .kb
        cmp al, 9
        je .kb
        cmp al, 13
        jbe .no
        call mayus
        stosb
        loop .k
.kb:
        call saltar_blancos
        lodsb
        cmp al, '='
        jne .no
.kf:
        mov al, 0
        rep stosb
        stosb
        lea di, [ds:bp+24]
        mov word [di], 0            ; NumParams
        add di, 2
.val:
        call saltar_sep
        mov al, [si]
        cmp al, 13
        jbe .vfin
        cmp al, ';'
        je .vfin
        inc word [ds:bp+24]
        cmp di, imagen + IMG_MAX - 80
        ja .no
        cmp al, '0'
        jb .cad
        cmp al, '9'
        ja .cad
        call leer_numero            ; -> EAX
        jc .cad
        mov word [di], 0
        mov word [di+2], 4
        mov [di+4], eax
        add di, 8
        jmp .val
.cad:
        mov word [di], 1
        push di
        add di, 4
        xor cx, cx
        cmp byte [si], '"'
        jne .c1
        inc si
.cq:
        lodsb
        cmp al, '"'
        je .cfin
        cmp al, 13
        jbe .cfin0
        stosb
        inc cx
        cmp cx, 64
        jb .cq
        jmp .cfin
.c1:
        lodsb
        cmp al, ' '
        jbe .cfin0
        cmp al, ','
        je .cfin
        cmp al, ';'
        je .cfin0
        stosb
        inc cx
        cmp cx, 64
        jb .c1
        jmp .cfin
.cfin0:
        dec si
.cfin:
        xor al, al
        stosb
        inc cx
        pop bx
        mov [bx+2], cx
        jmp .val
.vfin:
        call fin_de_linea
        clc
        ret
.no:
        stc
        ret

; DS:SI -> EAX (decimal o 0x...); CF si no es un número (SI sin mover)
leer_numero:
        push si
        xor eax, eax
        xor edx, edx
        cmp word [si], '0x'
        je .hex
        cmp word [si], '0X'
        je .hex
.dec:
        mov dl, [si]
        sub dl, '0'
        cmp dl, 9
        ja .df
        imul eax, eax, 10
        add eax, edx
        inc si
        jmp .dec
.hex:
        add si, 2
.h:
        mov dl, [si]
        call mayus_dl
        sub dl, '0'
        cmp dl, 9
        jbe .hd
        sub dl, 'A' - '0' - 10
        cmp dl, 10
        jb .df
        cmp dl, 15
        ja .df
.hd:
        shl eax, 4
        add eax, edx
        inc si
        jmp .h
.df:
        ; detrás tiene que venir un separador
        mov dl, [si]
        cmp dl, ' '
        jbe .ok
        cmp dl, ','
        je .ok
        cmp dl, ';'
        je .ok
        pop si
        stc
        ret
.ok:
        add sp, 2
        clc
        ret

saltar_blancos:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp saltar_blancos
.r:
        ret
saltar_sep:
        call saltar_blancos
        cmp byte [si], ','
        jne .r
        inc si
        jmp saltar_sep
.r:
        ret
fin_de_linea:
        lodsb
        test al, al
        jz .cero
        cmp al, 10
        jne fin_de_linea
        ret
.cero:
        dec si
        ret
mayus:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret
mayus_dl:
        cmp dl, 'a'
        jb .r
        cmp dl, 'z'
        ja .r
        sub dl, 32
.r:
        ret

img_primero dw 0
img_fin     dw imagen
mod_act     dw 0
kw_ant      dw 0
s_lanman    db ":\LANMAN", 0
s_ini       db "PROTOCOL.INI", 0
m_titulo    db "HSPPMAN 0.1 - gestor de protocolos NDIS 2 de Hispa-DOS", 13, 10, "$"
m_noini     db "HSPPMAN: no se encuentra PROTOCOL.INI", 13, 10, "$"
m_malini    db "HSPPMAN: PROTOCOL.INI tiene una línea que no se entiende (o no cabe)", 13, 10, "$"

            align 16
ini_buf:                            ; (detrás del archivo: memoria del INIT)
