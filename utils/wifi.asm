; =============================================================================
;  Hispa-486 WIFI.COM - elegir la red inalámbrica
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  WIFI                       cómo está la conexión
;  WIFI REDES                 busca las redes y las enseña, con su señal
;  WIFI CONECTAR red [clave]  conecta (red: su nombre, o su número en la
;                             lista de WIFI REDES); si la red pide contraseña
;                             y no se da, la pregunta sin enseñarla
;  WIFI DESCONECTAR           corta la conexión
;  WIFI OLVIDAR               además, la quita de RED.INI
;  WIFI /AUTO                 conecta con la red guardada en RED.INI (para
;                             AUTOEXEC.BAT; si no hay ninguna, no dice nada)
;  WIFI REGISTRO [archivo]    guarda el registro del controlador
;
;  Necesita HSPWIFI cargado (INT 2Fh AH=DCh). Al conectar guarda en
;  \HISPADOS\RED.INI de la unidad de arranque el nombre (WIFI=) y la clave ya
;  preparada (WIFICLAVE=, 64 cifras: la PMK de WPA2, no la contraseña).
;  Ver docs/WIFI.md.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

LL_BARRE    equ 5
LL_REDES    equ 6
LL_CONECTA  equ 7
LL_DESCON   equ 8
LL_ESTADO   equ 9
LL_REGISTRO equ 10
LL_PMK_E    equ 11
LL_PMK_S    equ 12

; struct w_info (wifi/nucleo/nucleo.h)
I_ESTADO    equ 0
I_FALLO     equ 1
I_CANAL     equ 2
I_SENAL     equ 3
I_BSSID     equ 4
I_SSID      equ 10
I_SSID_N    equ 43
I_SEG       equ 44
I_NREDES    equ 45
I_BARRE     equ 46
I_MAC       equ 47
I_TX        equ 53
I_RX        equ 57
I_TXERR     equ 61
I_RXDESC    equ 65
I_RECON     equ 69
I_BIEN      equ 73                  ; detrás: aciertos, fallos y velocidad
I_MAL       equ 77
I_VEL       equ 81
I_LEN       equ 85

; struct ll_red (42 octetos)
R_BSSID     equ 0
R_CANAL     equ 6
R_SENAL     equ 7
R_SEG       equ 8
R_SSID_N    equ 9
R_SSID      equ 10
R_LEN       equ 42
MAXREDES    equ 24

start:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        ; ¿está HSPWIFI?
        mov ax, 0xDC00
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .no
        cmp bx, 0x5746
        je .si
.no:
        ; con /AUTO y sin controlador, callado (AUTOEXEC.BAT de un equipo sin Wi-Fi)
        call palabra
        cmp word [si], '/A'
        je salir0
        cmp word [si], '/?'
        je ayuda
        mov si, m_nores
        call puts
        mov ax, 0x4C01
        int 0x21
.si:
        call palabra
        mov al, [si]
        cmp al, 13
        je cmd_estado
        cmp word [si], '/?'
        je ayuda
        mov di, t_ordenes
.busca:
        mov bx, [di]
        test bx, bx
        jz uso
        add di, 2
        push si
.c:
        mov al, [di]
        test al, al
        jz .fin_palabra
        mov ah, [si]
        call upcase_ah
        cmp ah, al
        jne .otra
        inc si
        inc di
        jmp .c
.fin_palabra:
        ; vale con las primeras letras (CON, DES, RED...) si después hay un blanco
        mov ah, [si]
        cmp ah, ' '
        jbe .hecho
        ; (palabra más larga que la de la tabla: se salta el resto de letras)
.resto:
        inc si
        cmp byte [si], ' '
        ja .resto
.hecho:
        add sp, 2
        call blancos
        jmp bx
.otra:
        pop si
.sal:
        inc di
        cmp byte [di-1], 0
        jne .sal
        jmp .busca

uso:
        mov si, m_uso
        call puts
        mov ax, 0x4C01
        int 0x21
ayuda:
        mov si, m_uso
        call puts
salir0:
        mov ax, 0x4C00
        int 0x21
salir1:
        call registro_auto
        mov ax, 0x4C01
        int 0x21
salir_bien:
        call registro_auto
        mov ax, 0x4C00
        int 0x21

; ---- SI = después del nombre del programa: salta los blancos ----
palabra:
        mov si, 0x81
blancos:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        je .s
        ret
.s:
        inc si
        jmp blancos

upcase_ah:
        cmp ah, 'a'
        jb .r
        cmp ah, 'z'
        ja .r
        sub ah, 32
.r:
        ret

; ---- llamar al núcleo: CX = función, la petición en «pet» -> DX:AX ----
nucleo:
        push si
        mov si, pet
        mov ax, 0xDC01
        int 0x2F
        pop si
        ret

; BX = desplazamiento -> EAX = dirección lineal
lineal:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ebx, bx
        add eax, ebx
        ret

; pedir el estado a «info»
estado:
        push bx
        push cx
        push dx
        mov bx, info
        call lineal
        mov [pet], eax
        mov dword [pet+4], I_LEN
        mov dword [pet+8], 0
        mov cx, LL_ESTADO
        call nucleo
        pop dx
        pop cx
        pop bx
        ret

; pedir la lista a «redes» -> [nredes]
lista:
        mov bx, redes
        call lineal
        mov [pet], eax
        mov dword [pet+4], MAXREDES
        mov cx, LL_REDES
        call nucleo
        mov [nredes], ax
        ret

; esperar un tic (dejando que el controlador mire la tarjeta); CF si Esc
espera_tic:
        push ax
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
.l:
        push ax
        mov ah, 1
        int 0x16
        jz .nk
        xor ah, ah
        int 0x16
        cmp al, 27
        jne .nk
        pop ax
        pop es
        pop ax
        stc
        ret
.nk:
        pop ax
        cmp ax, [es:0x46C]
        je .l
        pop es
        pop ax
        clc
        ret

; =============================================================================
;  WIFI REDES
; =============================================================================
cmd_redes:
        mov si, m_buscando
        call puts
        call buscar
        call lista
        cmp word [nredes], 0
        jne .hay
        mov si, m_ninguna
        call puts
        jmp salir_bien
.hay:
        mov si, m_cab
        call puts
        xor bp, bp
        mov di, redes
.l:
        ; número
        mov ax, bp
        inc ax
        mov cl, 3
        call num_ancho
        mov al, ' '
        call putc
        call putc
        ; nombre, en 32 columnas
        lea si, [di + R_SSID]
        movzx cx, byte [di + R_SSID_N]
        mov bx, 33
        jcxz .oculta
.n:
        lodsb
        call letra
        dec bx
        loop .n
        jmp .rell
.oculta:
        mov si, m_oculta
        call puts
        sub bx, 12
.rell:
        mov al, ' '
.r2:
        call putc
        dec bx
        jnz .r2
        ; canal
        movzx ax, byte [di + R_CANAL]
        mov cl, 3
        call num_ancho
        mov al, ' '
        call putc
        call putc
        ; señal: barras y dBm
        mov al, [di + R_SENAL]
        call barras
        mov al, ' '
        call putc
        movsx ax, byte [di + R_SENAL]
        call num_signo
        mov si, m_dbm
        call puts
        ; seguridad
        movzx bx, byte [di + R_SEG]
        cmp bx, 5
        jbe .s
        mov bx, 4
.s:
        shl bx, 1
        mov si, [t_seg + bx]
        call puts
        call crlf
        add di, R_LEN
        inc bp
        cmp bp, [nredes]
        jb .l
        mov si, m_pie
        call puts
        jmp salir_bien

; lanzar la búsqueda y esperar a que acabe (unos 2 segundos)
buscar:
        mov cx, LL_BARRE
        call nucleo
espera_busqueda:
        mov bp, 110                 ; 6 s como mucho
.l:
        call espera_tic
        jc .r
        call estado
        cmp byte [info + I_BARRE], 0
        je .r
        dec bp
        jnz .l
.r:
        ret

; AL = señal en dBm -> cuatro barras
barras:
        push bx
        push cx
        mov bl, 1
        cmp al, -75
        jl .p
        inc bl
        cmp al, -65
        jl .p
        inc bl
        cmp al, -55
        jl .p
        inc bl
.p:
        mov cx, 4
.l:
        mov al, 0xDB                ; bloque lleno
        test bl, bl
        jnz .b
        mov al, 0xB0                ; trama clara
        jmp .o
.b:
        dec bl
.o:
        call putc
        loop .l
        pop cx
        pop bx
        ret

; una letra del nombre de la red (lo que no es ASCII, «?»)
letra:
        cmp al, 32
        jb .q
        cmp al, 127
        jb .ok
.q:
        mov al, '?'
.ok:
        jmp putc

; =============================================================================
;  WIFI (estado)
; =============================================================================
cmd_estado:
        call estado
        call pinta_estado
        jmp salir_bien

pinta_estado:
        mov si, m_wifi
        call puts
        movzx bx, byte [info + I_ESTADO]
        cmp bl, 5
        je .con
        test bl, bl
        jnz .enello
        cmp byte [info + I_SSID_N], 0
        jne .quiere
        mov si, m_sin
        call puts
        jmp .mac
.quiere:
        ; quiere una red y no está conectado: el porqué
        mov si, m_noconec
        call puts
        call pon_ssid
        mov si, m_cierra
        call puts
        movzx bx, byte [info + I_FALLO]
        cmp bx, 9
        jbe .f
        xor bx, bx
.f:
        shl bx, 1
        mov si, [t_fallo + bx]
        call puts
        call crlf
        jmp .mac
.enello:
        mov si, m_conectando
        call puts
        call pon_ssid
        mov si, m_cierra3
        call puts
        jmp .mac
.con:
        mov si, m_conectado
        call puts
        call pon_ssid
        mov si, m_canal
        call puts
        movzx ax, byte [info + I_CANAL]
        call num
        mov si, m_senal
        call puts
        movsx ax, byte [info + I_SENAL]
        call num_signo
        mov si, m_dbm2
        call puts
        mov al, [info + I_SENAL]
        call barras
        mov si, m_coma
        call puts
        movzx bx, byte [info + I_SEG]
        cmp bx, 5
        jbe .s
        mov bx, 4
.s:
        shl bx, 1
        mov si, [t_seg + bx]
        call puts
        mov si, m_cierra2
        call puts
        mov si, m_ap
        call puts
        mov si, info + I_BSSID
        call pon_mac
        call crlf
.mac:
        mov si, m_mimac
        call puts
        mov si, info + I_MAC
        call pon_mac
        call crlf
        mov si, m_paq
        call puts
        mov eax, [info + I_TX]
        call num32
        mov si, m_paq2
        call puts
        mov eax, [info + I_RX]
        call num32
        mov si, m_paq3
        call puts
        mov eax, [info + I_TXERR]
        call num32
        mov si, m_paq4
        call puts
        mov eax, [info + I_RECON]
        call num32
        cmp dword [info + I_VEL], 0
        je .nv
        mov si, m_vel
        call puts
        mov eax, [info + I_VEL]
        shr eax, 1
        call num32
        test byte [info + I_VEL], 1
        jz .ent
        mov si, m_coma5
        call puts
.ent:
        mov si, m_mbit
        call puts
.nv:
        call crlf
        ret

pon_ssid:
        mov si, info + I_SSID
        movzx cx, byte [info + I_SSID_N]
        jcxz .r
.l:
        lodsb
        call letra
        loop .l
.r:
        ret

pon_mac:
        mov cx, 6
.l:
        lodsb
        call hex2
        dec cx
        jz .r
        mov al, ':'
        call putc
        jmp .l
.r:
        ret

; =============================================================================
;  WIFI CONECTAR red [clave]
; =============================================================================
cmd_conectar:
        cmp byte [si], 13
        je uso
        ; el nombre: entre comillas o hasta el blanco
        mov di, ssid
        xor cx, cx
        mov ah, ' '
        cmp byte [si], '"'
        jne .n
        mov ah, '"'
        inc si
.n:
        mov al, [si]
        cmp al, 13
        je .nf
        cmp al, ah
        je .nfc
        cmp cx, 32
        jae .nsig
        stosb
        inc cx
.nsig:
        inc si
        jmp .n
.nfc:
        inc si
.nf:
        mov [ssid_n], cl
        test cx, cx
        jz near uso
        call blancos
        ; la clave, si viene
        mov di, clave
        xor cx, cx
.k:
        mov al, [si]
        cmp al, 13
        je .kf
        cmp cx, 64
        jae .kf
        stosb
        inc cx
        inc si
        jmp .k
.kf:
        ; (sin los blancos del final)
        jcxz .kz
        cmp byte [di-1], ' '
        jne .kz
        dec di
        dec cx
        jmp .kf
.kz:
        mov byte [di], 0
        mov [clave_n], cl
        ; ¿un número de la lista? (si hay una búsqueda a medias, se espera)
        call espera_busqueda
        call lista
        call es_numero
        jc .nombre
        cmp ax, 1
        jb .nombre
        cmp ax, [nredes]
        jbe .num
        ; no hay tantas: si la lista está vacía se busca ahora
        cmp word [nredes], 0
        jne .nonum
        push ax
        mov si, m_buscando
        call puts
        call buscar
        call lista
        pop ax
        cmp ax, [nredes]
        jbe .num
.nonum:
        mov si, m_nonum
        call puts
        jmp salir1
.num:
        dec ax
        imul bx, ax, R_LEN
        add bx, redes
        jmp .tengo
.nombre:
        call busca_red
        jnc .tengo
        ; no está en la lista: buscar ahora
        mov si, m_buscando
        call puts
        call buscar
        call lista
        call busca_red
        jnc .tengo
        mov si, m_noaparece
        call puts
        jmp salir1
.tengo:
        ; BX = la red de la lista
        lea si, [bx + R_SSID]
        mov di, ssid
        movzx cx, byte [bx + R_SSID_N]
        mov [ssid_n], cl
        rep movsb
        mov al, [bx + R_SEG]
        mov byte [con_clave], 0
        test al, al
        jz .conecta                 ; abierta
        cmp al, 3
        je .wpa2
        mov si, m_noseg
        call puts
        movzx bx, al
        cmp bx, 5
        jbe .ts
        mov bx, 4
.ts:
        shl bx, 1
        mov si, [t_seg + bx]
        call puts
        mov si, m_noseg2
        call puts
        jmp salir1
.wpa2:
        mov byte [con_clave], 1
        cmp byte [clave_n], 0
        jne .hayclave
        call pide_clave
        jc salir1
.hayclave:
        ; 64 cifras hexadecimales: es la clave ya preparada
        cmp byte [clave_n], 64
        jne .frase
        mov si, clave
        mov di, pmk
        call hex_a_pmk
        jnc .conecta
.frase:
        mov al, [clave_n]
        cmp al, 8
        jb .corta
        cmp al, 63
        jbe .prepara
.corta:
        mov si, m_corta
        call puts
        jmp salir1
.prepara:
        mov si, m_prepara
        call puts
        mov bx, clave
        call lineal
        mov [pet], eax
        mov bx, ssid
        call lineal
        mov [pet+4], eax
        movzx eax, byte [ssid_n]
        mov [pet+8], eax
        mov cx, LL_PMK_E
        call nucleo
        mov bx, pmk
        call lineal
        mov [pet], eax
        xor bp, bp
.pm:
        mov cx, LL_PMK_S
        call nucleo
        cmp ax, 1
        je .pmf
        inc bp
        test bp, 3
        jnz .pm
        mov al, '.'
        call putc
        jmp .pm
.pmf:
        call crlf
        ; la contraseña, fuera de la memoria
        mov di, clave
        mov cx, 65
        xor al, al
        rep stosb
.conecta:
        mov byte [callado], 0
        call conecta
        jc salir1
        call guarda_ini
        mov si, m_guardada
        call puts
        jmp salir_bien

; conectar con ssid/pmk y esperar el resultado. CF si no conecta.
conecta:
        mov bx, ssid
        call lineal
        mov [pet], eax
        movzx eax, byte [ssid_n]
        mov [pet+4], eax
        mov dword [pet+8], 0
        cmp byte [con_clave], 0
        je .sin
        mov bx, pmk
        call lineal
        mov [pet+8], eax
.sin:
        mov cx, LL_CONECTA
        call nucleo
        mov si, m_conectando2
        call puts
        mov si, ssid
        movzx cx, byte [ssid_n]
.nl:
        lodsb
        call letra
        loop .nl
        mov si, m_cierra3
        call puts
        ; hasta 25 segundos; un fallo que no se arregla solo, antes
        mov bp, 455
.l:
        call espera_tic
        jc .esc
        call estado
        cmp byte [info + I_ESTADO], 5
        je .bien
        mov al, [info + I_FALLO]
        cmp al, 6                   ; contraseña
        je .mal
        cmp al, 7                   ; seguridad
        je .mal
        cmp al, 3                   ; autenticación rechazada
        je .mal
        cmp al, 5                   ; asociación rechazada
        je .mal
        ; «no aparece», tras dos búsquedas (unos 9 s)
        cmp al, 1
        jne .sig
        cmp bp, 455 - 200
        jb .mal
.sig:
        dec bp
        jnz .l
.mal:
        mov si, m_nocon
        call puts
        movzx bx, byte [info + I_FALLO]
        cmp bx, 9
        jbe .f
        xor bx, bx
.f:
        test bx, bx
        jnz .f2
        mov bx, 10                  ; se acabó el tiempo sin motivo claro
.f2:
        shl bx, 1
        mov si, [t_fallo + bx]
        call puts
        call crlf
        ; que no lo siga intentando solo con una clave mala
        cmp byte [callado], 0
        jne .deja
        mov cx, LL_DESCON
        call nucleo
.deja:
        stc
        ret
.esc:
        mov si, m_esc
        call puts
        mov cx, LL_DESCON
        call nucleo
        stc
        ret
.bien:
        mov si, m_hecho
        call puts
        call pon_ssid
        mov si, m_hecho2
        call puts
        movsx ax, byte [info + I_SENAL]
        call num_signo
        mov si, m_dbm3
        call puts
        ; la configuración IP que hubiera es de otra red: que se pida otra vez
        ; (salvo con HSPWIFI /JUNTO: esa configuración es la de HSPRED)
        mov ax, 0xDC02
        int 0x2F
        mov al, [es:bx+64]
        push cs
        pop es
        test al, 8
        jnz .r
        mov ax, 0xDA00
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .r
        mov word [es:di], 0
        push cs
        pop es
.r:
        clc
        ret

; ¿el nombre es un número? -> AX; CF si no
es_numero:
        mov si, ssid
        movzx cx, byte [ssid_n]
        xor ax, ax
        cmp cx, 2
        ja .no
.l:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .no
        imul ax, ax, 10
        xor bh, bh
        add ax, bx
        inc si
        loop .l
        clc
        ret
.no:
        stc
        ret

; buscar ssid en la lista -> BX = su entrada (la de más señal); CF si no está
busca_red:
        xor dx, dx
        mov bx, redes
        mov bp, [nredes]
        test bp, bp
        jz .fin
.l:
        mov al, [ssid_n]
        cmp al, [bx + R_SSID_N]
        jne .n
        lea si, [bx + R_SSID]
        mov di, ssid
        movzx cx, al
        repe cmpsb
        jne .n
        test dx, dx
        jz .mejor
        mov al, [bx + R_SENAL]
        xchg bx, dx
        cmp al, [bx + R_SENAL]
        xchg bx, dx
        jle .n
.mejor:
        mov dx, bx
.n:
        add bx, R_LEN
        dec bp
        jnz .l
.fin:
        mov bx, dx
        test dx, dx
        jz .no
        clc
        ret
.no:
        stc
        ret

; pedir la contraseña sin enseñarla -> clave, clave_n; CF si se deja vacía
pide_clave:
        mov si, m_pideclave
        call puts
        mov di, clave
        xor cx, cx
.l:
        mov ah, 0x08
        int 0x21
        cmp al, 13
        je .fin
        cmp al, 27
        je .vacia
        cmp al, 8
        je .borra
        test al, al
        jnz .letra
        mov ah, 0x08                ; tecla de función: se salta su código
        int 0x21
        jmp .l
.letra:
        cmp al, 32
        jb .l
        cmp cx, 64
        jae .l
        stosb
        inc cx
        mov al, '*'
        call putc
        jmp .l
.borra:
        jcxz .l
        dec di
        dec cx
        mov al, 8
        call putc
        mov al, ' '
        call putc
        mov al, 8
        call putc
        jmp .l
.vacia:
        xor cx, cx
.fin:
        call crlf
        mov byte [di], 0
        mov [clave_n], cl
        jcxz .no
        clc
        ret
.no:
        stc
        ret

; SI = 64 cifras hexadecimales -> DI = 32 octetos; CF si no lo son
hex_a_pmk:
        mov cx, 32
.l:
        call .cifra
        jc .r
        shl al, 4
        mov ah, al
        call .cifra
        jc .r
        or al, ah
        stosb
        loop .l
        clc
.r:
        ret
.cifra:
        lodsb
        cmp al, '0'
        jb .mal
        cmp al, '9'
        jbe .d
        or al, 0x20
        cmp al, 'a'
        jb .mal
        cmp al, 'f'
        ja .mal
        sub al, 'a' - 10
        clc
        ret
.d:
        sub al, '0'
        clc
        ret
.mal:
        stc
        ret

; =============================================================================
;  WIFI DESCONECTAR / OLVIDAR / /AUTO
; =============================================================================
cmd_olvidar:
        mov byte [ssid_n], 0
        call guarda_ini
        mov si, m_olvidada
        call puts
cmd_desconectar:
        mov cx, LL_DESCON
        call nucleo
        mov si, m_descon
        call puts
        jmp salir_bien

cmd_auto:
        call lee_ini
        jc salir0                   ; no hay red guardada: nada que decir
        mov byte [callado], 1       ; si no conecta, que lo siga intentando solo
        call conecta
        jc salir1
        jmp salir_bien

; =============================================================================
;  RED.INI: WIFI= y WIFICLAVE=
; =============================================================================
ini_abre:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [ini_ruta], dl
        mov [reg_def], dl
        ret

; leer el archivo entero a ini_buf -> [ini_n]
ini_carga:
        call ini_abre
        mov word [ini_n], 0
        mov dx, ini_ruta
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov dx, ini_buf
        mov cx, INI_MAX
        mov ah, 0x3F
        int 0x21
        jc .c
        mov [ini_n], ax
.c:
        mov ah, 0x3E
        int 0x21
.r:
        ret

; SI = principio de una línea -> AL = 1 si es WIFI=, 2 si WIFICLAVE=, 0 otra;
; BX = donde empieza el valor
ini_clave:
        push si
        mov di, k_wificlave
        call .cmp
        mov al, 2
        jnc .r
        pop si
        push si
        mov di, k_wifi
        call .cmp
        mov al, 1
        jnc .r
        xor al, al
.r:
        mov bx, si
        pop si
        ret
.cmp:
        mov ah, [si]
        call upcase_ah
        cmp byte [di], 0
        je .fin
        cmp ah, [di]
        jne .no
        inc si
        inc di
        jmp .cmp
.fin:
        cmp ah, '='
        jne .no
        inc si
        clc
        ret
.no:
        stc
        ret

; -> ssid/ssid_n, pmk, con_clave; CF si no hay red guardada
lee_ini:
        call ini_carga
        mov byte [ssid_n], 0
        mov byte [con_clave], 0
        mov si, ini_buf
        mov bp, ini_buf
        add bp, [ini_n]
.linea:
        cmp si, bp
        jae .fin
        call ini_clave
        cmp al, 1
        jne .n2
        ; WIFI=nombre (hasta el fin de la línea)
        push si
        mov si, bx
        mov di, ssid
        xor cx, cx
.c:
        cmp si, bp
        jae .cf
        mov al, [si]
        cmp al, 13
        je .cf
        cmp al, 10
        je .cf
        cmp cx, 32
        jae .cf
        stosb
        inc cx
        inc si
        jmp .c
.cf:
        mov [ssid_n], cl
        pop si
        jmp .salta
.n2:
        cmp al, 2
        jne .salta
        push si
        mov si, bx
        mov di, pmk
        call hex_a_pmk
        pop si
        jc .salta
        mov byte [con_clave], 1
.salta:
        cmp si, bp
        jae .fin
        lodsb
        cmp al, 10
        jne .salta
        jmp .linea
.fin:
        cmp byte [ssid_n], 1
        ret                         ; CF si ssid_n = 0

; escribir RED.INI sin las líneas del Wi-Fi y, si hay red (ssid_n), con las
; nuevas al final
guarda_ini:
        call ini_carga
        cmp word [ini_n], INI_MAX   ; demasiado grande para rehacerlo: no se toca
        jae .mal
        mov dx, ini_ruta
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .mal
        mov [fh], ax
        mov si, ini_buf
        mov bp, ini_buf
        add bp, [ini_n]
.linea:
        cmp si, bp
        jae .fin
        call ini_clave
        mov dl, al
        ; el final de la línea
        mov di, si
.e:
        cmp di, bp
        jae .ef
        inc di
        cmp byte [di-1], 10
        jne .e
.ef:
        test dl, dl
        jnz .sig                    ; las del Wi-Fi no se copian
        mov cx, di
        sub cx, si
        mov dx, si
        mov bx, [fh]
        mov ah, 0x40
        int 0x21
.sig:
        mov si, di
        jmp .linea
.fin:
        ; que el archivo acabe en fin de línea antes de añadir
        cmp word [ini_n], 0
        je .nuevas
        cmp byte [bp-1], 10
        je .nuevas
        mov dx, k_nl
        mov cx, 2
        call .escribe
.nuevas:
        cmp byte [ssid_n], 0
        je .cierra
        mov dx, k_wifi_l
        mov cx, 5
        call .escribe
        mov dx, ssid
        movzx cx, byte [ssid_n]
        call .escribe
        mov dx, k_nl
        mov cx, 2
        call .escribe
        cmp byte [con_clave], 0
        je .cierra
        mov dx, k_clave_l
        mov cx, 10
        call .escribe
        ; la PMK en hexadecimal
        mov si, pmk
        mov di, hexbuf
        mov cx, 32
.h:
        lodsb
        mov ah, al
        shr al, 4
        call .cifra
        mov al, ah
        and al, 15
        call .cifra
        loop .h
        mov dx, hexbuf
        mov cx, 64
        call .escribe
        mov dx, k_nl
        mov cx, 2
        call .escribe
.cierra:
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        ret
.mal:
        mov si, m_noini
        call puts
        ret
.escribe:
        mov bx, [fh]
        mov ah, 0x40
        int 0x21
        ret
.cifra:
        add al, '0'
        cmp al, '9'
        jbe .cp
        add al, 39
.cp:
        stosb
        ret

; =============================================================================
;  WIFI REGISTRO [archivo]
; =============================================================================
cmd_registro:
        call ini_abre
        mov di, reg_ruta
        cmp byte [si], 13
        je .def
.c:
        lodsb
        cmp al, ' '
        jbe .cf
        stosb
        cmp di, reg_ruta + 63
        jb .c
.cf:
        mov byte [di], 0
        jmp .va
.def:
        call ruta_residente
        cmp byte [reg_ruta], 0
        jne .va
        mov si, reg_def
.d:
        lodsb
        stosb
        test al, al
        jnz .d
.va:
        ; el estado, también al registro (petición [8] = 1)
        mov bx, info
        call lineal
        mov [pet], eax
        mov dword [pet+4], I_LEN
        mov dword [pet+8], 1
        mov cx, LL_ESTADO
        call nucleo
        call vuelca
        jc .mal
        mov si, m_reg
        call puts
        mov si, reg_ruta
        call puts
        call crlf
        mov ax, 0x4C00
        int 0x21
.mal:
        mov si, m_noreg
        call puts
        mov si, reg_ruta
        call puts
        call crlf
        mov ax, 0x4C01
        int 0x21

; el archivo de registro que se dio a HSPWIFI (/REG:) -> reg_ruta
ruta_residente:
        push es
        push si
        push di
        mov ax, 0xDC02
        int 0x2F
        mov si, bx
        mov di, reg_ruta
        mov cx, 64
.l:
        mov al, [es:si]
        mov [di], al
        inc si
        inc di
        loop .l
        pop di
        pop si
        pop es
        ret

; al acabar cualquier orden: si HSPWIFI se cargó con /REG, lo nuevo al archivo
registro_auto:
        call ruta_residente
        cmp byte [reg_ruta], 0
        je .r
        call vuelca
.r:
        ret

; añadir a reg_ruta lo que haya en el registro del núcleo. CF si no se puede.
vuelca:
        mov dx, reg_ruta
        mov ax, 0x3D02
        int 0x21
        jnc .ab
        mov dx, reg_ruta
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .r
.ab:
        mov [fh], ax
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov ax, 0x4202
        int 0x21
.l:
        mov bx, ini_buf
        call lineal
        mov [pet], eax
        mov dword [pet+4], INI_MAX
        mov cx, LL_REGISTRO
        call nucleo
        test ax, ax
        jle .fin
        mov cx, ax
        mov dx, ini_buf
        mov bx, [fh]
        mov ah, 0x40
        int 0x21
        jmp .l
.fin:
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        clc
.r:
        ret

; =============================================================================
;  Escribir
; =============================================================================
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
        push si
.l:
        lodsb
        test al, al
        jz .d
        call putc
        jmp .l
.d:
        pop si
        pop ax
        ret

crlf:
        push ax
        mov al, 13
        call putc
        mov al, 10
        call putc
        pop ax
        ret

hex2:
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 15
.n:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 39
.p:
        jmp putc

; AX con signo
num_signo:
        test ax, ax
        jns num
        push ax
        mov al, '-'
        call putc
        pop ax
        neg ax
; AX sin signo
num:
        push eax
        movzx eax, ax
        call num32
        pop eax
        ret

; EAX sin signo
num32:
        push eax
        push ebx
        push cx
        push edx
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop edx
        pop cx
        pop ebx
        pop eax
        ret

; AX sin signo, alineado a la derecha en CL columnas (hasta 999)
num_ancho:
        push ax
        push cx
        cmp ax, 100
        jae .n
        dec cl
        push ax
        mov al, ' '
        call putc
        pop ax
        cmp ax, 10
        jae .n
        push ax
        mov al, ' '
        call putc
        pop ax
.n:
        call num
        pop cx
        pop ax
        ret

; =============================================================================
;  Datos
; =============================================================================
t_ordenes:
        dw cmd_redes
        db "RED", 0
        dw cmd_redes
        db "LIS", 0
        dw cmd_redes
        db "BUS", 0
        dw cmd_redes
        db "/L", 0
        dw cmd_conectar
        db "CON", 0
        dw cmd_desconectar
        db "DES", 0
        dw cmd_olvidar
        db "OLV", 0
        dw cmd_estado
        db "EST", 0
        dw cmd_registro
        db "REG", 0
        dw cmd_registro
        db "/R", 0
        dw cmd_auto
        db "/A", 0
        dw 0

t_seg       dw s_abierta, s_wep, s_wpa, s_wpa2, s_otra, s_wpa2t
s_abierta   db "abierta", 0
s_wep       db "WEP (no se admite)", 0
s_wpa       db "WPA antigua (no se admite)", 0
s_wpa2      db "WPA2", 0
s_otra      db "otra (no se admite)", 0
s_wpa2t     db "WPA2 con TKIP (no se admite)", 0

t_fallo     dw f_0, f_1, f_2, f_3, f_4, f_5, f_6, f_7, f_8, f_9, f_10
f_0         db "todavía se está intentando.", 0
f_1         db "la red no aparece (¿está lejos o apagado el router?).", 0
f_2         db "el punto de acceso no contesta (señal débil).", 0
f_3         db "el punto de acceso rechaza la conexión (¿filtro de equipos?).", 0
f_4         db "el punto de acceso no contesta a la asociación (señal débil).", 0
f_5         db "el punto de acceso rechaza la asociación.", 0
f_6         db "la contraseña no es la de esta red.", 0
f_7         db "la seguridad de esa red no se admite (hace falta WPA2-Personal con AES).", 0
f_8         db "el punto de acceso ha cortado la conexión.", 0
f_9         db "se ha perdido la señal.", 0
f_10        db "no ha contestado a tiempo.", 0

m_nores     db "HSPWIFI no está cargado. Cárguelo antes: HSPWIFI", 13, 10, 0
m_uso       db "WIFI: la red inalámbrica (Wi-Fi) de Hispa-486", 13, 10, 13, 10
            db "  WIFI                       cómo está la conexión", 13, 10
            db "  WIFI REDES                 busca las redes y las enseña con su señal", 13, 10
            db "  WIFI CONECTAR red [clave]  conecta. «red» es el nombre (entre comillas si", 13, 10
            db "                             lleva espacios) o su número en WIFI REDES.", 13, 10
            db "                             Si no se da la clave, la pide sin enseñarla.", 13, 10
            db "  WIFI DESCONECTAR           corta la conexión", 13, 10
            db "  WIFI OLVIDAR               la corta y la quita de RED.INI", 13, 10
            db "  WIFI /AUTO                 conecta con la red guardada (para AUTOEXEC.BAT)", 13, 10
            db "  WIFI REGISTRO [archivo]    guarda el registro del controlador", 13, 10, 13, 10
            db "Admite redes abiertas y con WPA2-Personal (AES). La red y su clave ya", 13, 10
            db "preparada se guardan en \HISPADOS\RED.INI. Después: CONFRED, PING...", 13, 10, 0
m_buscando  db "Buscando redes...", 13, 10, 0
m_ninguna   db "No se ve ninguna red.", 13, 10, 0
m_cab       db 13, 10, "N.º  Red                              Canal  Señal         Seguridad", 13, 10, 0
m_oculta    db "(sin nombre)", 0
m_dbm       db " dBm  ", 0
m_pie       db 13, 10, "Para conectar: WIFI CONECTAR número  (o el nombre)", 13, 10, 0
m_wifi      db "Wi-Fi: ", 0
m_sin       db "sin conectar. Para ver las redes: WIFI REDES", 13, 10, 0
m_noconec   db "sin conexión con «", 0
m_cierra    db "»: ", 0
m_conectando db "conectando con «", 0
m_cierra3   db "»...", 13, 10, 0
m_conectado db "conectado a «", 0
m_canal     db "» (canal ", 0
m_senal     db ", señal ", 0
m_dbm2      db " dBm ", 0
m_coma      db ", ", 0
m_cierra2   db ")", 13, 10, 0
m_ap        db "  Punto de acceso: ", 0
m_mimac     db "  Dirección MAC:   ", 0
m_paq       db "  Paquetes: ", 0
m_paq2      db " enviados, ", 0
m_paq3      db " recibidos, ", 0
m_paq4      db " sin acuse; reconexiones: ", 0
m_vel       db "; velocidad: ", 0
m_coma5     db ",5", 0
m_mbit      db " Mbit/s", 0
m_noaparece db "Esa red no aparece. Mire las que hay con WIFI REDES.", 13, 10, 0
m_nonum     db "Ese número no está en la lista de WIFI REDES.", 13, 10, 0
m_noseg     db "Esa red no se puede usar: su seguridad es ", 0
m_noseg2    db ".", 13, 10, "Hace falta una red abierta o con WPA2-Personal (AES).", 13, 10, 0
m_pideclave db "Contraseña de la red: ", 0
m_corta     db "La contraseña de una red WPA2 tiene de 8 a 63 letras.", 13, 10, 0
m_prepara   db "Preparando la clave", 0
m_conectando2 db "Conectando con «", 0
m_nocon     db "No se ha podido conectar: ", 0
m_esc       db "Cancelado.", 13, 10, 0
m_hecho     db "Conectado a «", 0
m_hecho2    db "» (señal ", 0
m_dbm3      db " dBm).", 13, 10, 0
m_guardada  db "La red queda guardada en RED.INI. Ahora: CONFRED", 13, 10, 0
m_olvidada  db "Red quitada de RED.INI.", 13, 10, 0
m_descon    db "Wi-Fi desconectado.", 13, 10, 0
m_noini     db "No se ha podido escribir RED.INI.", 13, 10, 0
m_reg       db "Registro guardado en ", 0
m_noreg     db "No se puede escribir ", 0

k_wifi      db "WIFI", 0
k_wificlave db "WIFICLAVE", 0
k_wifi_l    db "WIFI="
k_clave_l   db "WIFICLAVE="
k_nl        db 13, 10
ini_ruta    db "A:\HISPADOS\RED.INI", 0
reg_def     db "A:\WIFI.LOG", 0
callado     db 0
con_clave   db 0
ssid_n      db 0
clave_n     db 0
nredes      dw 0
fh          dw 0
ini_n       dw 0

INI_MAX     equ 4096
        section .bss
bss_ini:
pet         resd 4
ssid        resb 34
clave       resb 66
pmk         resb 32
hexbuf      resb 64
reg_ruta    resb 64
info        resb I_LEN + 8
redes       resb MAXREDES * R_LEN
ini_buf     resb INI_MAX + 2
bss_fin:
