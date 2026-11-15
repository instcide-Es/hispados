; =============================================================================
;  Hispa-DOS ACTUALIZAR (en el disco ACTUALIZ.COM) - buscar actualizaciones
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  ACTUALIZAR [http://servidor[:puerto]/ruta]
;  Mira por la red (HTTP, sin cifrar) si hay una versión más nueva de
;  Hispa-DOS que la instalada. Lee un archivo de texto con la última versión
;  (una cosa por línea):
;      version=0.9.12
;      descarga=http://ejemplo/hispados/
;      nota=Lo nuevo de esta versión
;  Compara «version=» con la versión del sistema (lib/version.inc) y avisa si
;  hay una más nueva. NO descarga ni instala nada: solo avisa.
;
;  La dirección del archivo sale, por este orden:
;    1. la que se escriba en la línea de órdenes (si se escribe),
;    2. la clave ACTUALIZAR= de \HISPADOS\RED.INI (si existe),
;    3. la de por omisión compilada (ver s_url_def, más abajo).
;
;  HTTPS (cifrado) todavía no: se rechaza con un mensaje claro, igual que
;  DESCARGAR. GitHub es todo HTTPS, así que el archivo de versión tiene que
;  estar en un servidor propio por HTTP.
;
;  Necesita HSPRED (u otro packet driver) y la red configurada (CONFRED).
;  Ver docs/RED.md.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

%include "version.inc"

HDR_MAX     equ 2048
BODY_MAX    equ 2048
INI_MAX     equ 2048

start:
        cld
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        mov word [t_rutina], act_datos
        ; ---- la dirección: ¿viene en la línea de órdenes? ----
        mov si, 0x81
        call blancos
        cmp word [si], '/?'
        je ayuda
        cmp byte [si], 13
        je .sin_arg
        mov di, url
        call palabra
        mov byte [url_dado], 1
.sin_arg:
        ; si no se escribió, la de RED.INI (clave ACTUALIZAR=)
        cmp byte [url_dado], 0
        jne .con_url
        call ini_actualizar
.con_url:
        ; si tampoco, la de por omisión compilada
        cmp byte [url_dado], 0
        jne .ya
        mov si, s_url_def
        mov di, url
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
.ya:
        ; ---- la red: HSPRED cargado y con IP ----
        call red_abrir
        jc no_red
        xor al, al
        call red_config
        jc no_red
        cmp dword [my_ip], 0
        je no_red
        ; ---- aviso ----
        mov si, m_mirando
        call puts
.otra_url:
        call url_partir
        jc error
        ; el servidor
        mov si, m_buscando
        call puts
        mov si, host
        call puts
        mov si, m_puntos
        call puts
        mov si, host
        call dns_buscar
        jc error_nl
        mov [srv_ip], eax
        call ip_poner
        mov al, ':'
        call putc
        mov di, num_txt
        movzx eax, word [puerto]
        call num_a_texto
        mov byte [di], 0
        mov si, num_txt
        call puts
        call crlf
        ; la conexión y la petición
        mov eax, [srv_ip]
        mov bx, [puerto]
        call tcp_abrir
        jc error
        call peticion
        mov si, peti
        call tcp_enviar
        jc error_cierra
        ; recibir hasta que el servidor cierre
        call tic
        mov [t_ult], eax
        mov eax, [recibidos]
        mov [rec_ult], eax
.l:
        call red_sondear
        cmp byte [abortar], 0
        jne .corta
        cmp byte [t_rst], 0
        jne .cortada
        cmp byte [t_fin], 0
        jne .acabo
        call esc_pulsado
        jc .esc
        mov eax, [recibidos]
        cmp eax, [rec_ult]
        je .quieto
        mov [rec_ult], eax
        call tic
        mov [t_ult], eax
        jmp .l
.quieto:
        call tic
        sub eax, [t_ult]
        cmp eax, 30 * 18            ; 30 s sin nada
        jb .l
        mov si, m_parado
        jmp .falla
.esc:
        mov si, m_esc
        jmp .falla
.cortada:
        mov si, m_tcprst           ; (está en red.inc)
        jmp .falla
.corta:
        mov si, [abortar_msg]
.falla:
        push si
        call tcp_cerrar
        pop si
        call puts
        call crlf
        mov al, 1
        jmp red_salir
.acabo:
        call tcp_cerrar
        cmp byte [h_estado], 1
        je .cuerpo
        cmp byte [h_estado], 3
        je .redir
        ; el servidor cerró sin terminar las cabeceras
        mov si, m_nohttp
        jmp .falla
.redir:
        inc byte [n_redir]
        cmp byte [n_redir], 4
        ja .muchas
        mov si, m_redir
        call puts
        mov si, url
        call puts
        call crlf
        mov byte [h_estado], 0
        mov word [h_len], 0
        mov dword [recibidos], 0
        mov word [body_len], 0
        mov byte [t_fin], 0
        jmp .otra_url
.muchas:
        mov si, m_muchas
        jmp .falla
.cuerpo:
        ; ya tenemos el archivo de versión en memoria: a mirarlo
        mov byte [body_buf + BODY_MAX - 1], 0
        call parse_cuerpo
        cmp byte [v_version], 0
        je .sin_version
        ; el número de la versión de fuera
        mov si, v_version
        call calc_num
        jc .sin_version
        mov [rem_num], eax
        ; comparar con la del sistema
        cmp eax, VER_NUM
        ja .nueva
        ; igual o menor: al día
        mov si, m_aldia
        call puts
        xor al, al
        jmp red_salir
.nueva:
        mov si, m_nueva
        call puts
        mov si, v_version
        call puts
        mov si, m_punto
        call puts
        ; la nota, si vino
        cmp byte [v_nota], 0
        je .nd
        mov si, v_nota
        call puts
        call crlf
.nd:
        ; dónde descargarla
        mov si, m_desc
        call puts
        cmp byte [v_descarga], 0
        je .du
        mov si, v_descarga
        call puts
        call crlf
        jmp .fin_nueva
.du:
        mov si, url                 ; si no hay «descarga=», la propia URL
        call puts
        call crlf
.fin_nueva:
        mov si, m_comoact
        call puts
        xor al, al
        jmp red_salir
.sin_version:
        mov si, m_sinver
        call puts
        mov al, 1
        jmp red_salir

no_red:
        mov si, m_sinred
        call puts
        call crlf
        mov al, 4
        jmp red_salir

error_cierra:
        push si
        call tcp_cerrar
        pop si
        jmp error
error_nl:
        call crlf
error:
        call puts
        call crlf
        mov al, 1
        jmp red_salir

ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  La dirección de RED.INI (clave ACTUALIZAR=)
; =============================================================================
; Lee \HISPADOS\RED.INI de la unidad de arranque y, si trae una línea
; ACTUALIZAR=<dirección>, la copia a «url» y pone [url_dado]=1.
ini_actualizar:
        mov ax, 0x3305
        int 0x21                    ; DL = unidad de arranque (1 = A)
        add dl, 'A' - 1
        mov [ini_ruta], dl
        mov dx, ini_ruta
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov dx, ini_buf
        mov cx, INI_MAX - 1
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        mov ah, 0x3E                ; cerrar (BX sigue siendo el manejador)
        int 0x21
        pop ax
        popf
        jc .r
        mov si, ini_buf
        add si, ax
        mov byte [si], 0
        mov si, ini_buf
.linea:
        ; saltar los espacios y tabuladores del principio
.bl:
        mov al, [si]
        cmp al, ' '
        je .bi
        cmp al, 9
        je .bi
        cmp al, 13
        je .bi
        cmp al, 10
        je .bi
        jmp .nobl
.bi:
        inc si
        jmp .bl
.nobl:
        test al, al
        jz .r
        cmp al, ';'
        je .salta
        cmp al, '#'
        je .salta
        cmp al, '['
        je .salta
        ; la clave, en mayúsculas, hasta «=»
        mov di, i_clave
        mov cx, 15
.k:
        mov al, [si]
        test al, al
        jz .r
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, '='
        je .ke
        inc si
        cmp al, ' '
        je .k
        cmp al, 9
        je .k
        call upcase
        stosb
        loop .k
        jmp .salta
.ke:
        mov byte [di], 0
        inc si                      ; saltar «=»
        ; ¿la clave es ACTUALIZAR?
        mov di, i_clave
        mov bp, s_clave_act
.cmp:
        mov al, [bp]
        cmp al, [di]
        jne .salta
        test al, al
        jz .match
        inc di
        inc bp
        jmp .cmp
.match:
        ; saltar los espacios tras «=»
.sp:
        mov al, [si]
        cmp al, ' '
        je .spi
        cmp al, 9
        jne .copia
.spi:
        inc si
        jmp .sp
.copia:
        mov di, url
        mov cx, 250
.c:
        mov al, [si]
        cmp al, ' '                 ; espacio, tab, CR, LF o 0: fin del valor
        jbe .ce
        inc si
        stosb
        loop .c
.ce:
        mov byte [di], 0
        cmp di, url
        je .salta                   ; valor vacío: como si no estuviera
        mov byte [url_dado], 1
        ret
.salta:
        ; hasta el fin de la línea
        mov al, [si]
        test al, al
        jz .r
        inc si
        cmp al, 10
        jne .salta
        jmp .linea
.r:
        ret

; =============================================================================
;  El cuerpo (el archivo de versión) ya está en body_buf (acaba en 0).
;  Saca version=, descarga= y nota= a v_version, v_descarga y v_nota.
; =============================================================================
parse_cuerpo:
        mov si, body_buf
.linea:
.bl:
        mov al, [si]
        cmp al, ' '
        je .bi
        cmp al, 9
        je .bi
        jmp .nobl
.bi:
        inc si
        jmp .bl
.nobl:
        test al, al
        jz .r
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, ';'
        je .salta
        cmp al, '#'
        je .salta
        ; la clave, en mayúsculas, hasta «=»
        mov di, p_clave
        mov cx, 14
.k:
        mov al, [si]
        test al, al
        jz .r
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, '='
        je .ke
        inc si
        call upcase
        stosb
        loop .k
        jmp .salta
.ke:
        mov byte [di], 0
        inc si
        ; saltar espacios tras «=»
.sp:
        mov al, [si]
        cmp al, ' '
        je .spi
        cmp al, 9
        jne .val
.spi:
        inc si
        jmp .sp
.val:
        ; el valor, hasta CR/LF/0, a p_valor
        mov di, p_valor
        mov cx, 200
.vl:
        mov al, [si]
        test al, al
        jz .vend
        cmp al, 13
        je .vend
        cmp al, 10
        je .vend
        inc si
        stosb
        loop .vl
.vend:
        mov byte [di], 0
        mov [p_si], si              ; guardar la posición en el cuerpo
        ; recortar los espacios del final
        call recorta
        ; ¿qué clave es?
        mov di, p_clave
        mov bp, k_version
        call kcmp
        jnc .es_version
        mov di, p_clave
        mov bp, k_descarga
        call kcmp
        jnc .es_descarga
        mov di, p_clave
        mov bp, k_nota
        call kcmp
        jnc .es_nota
        jmp .salta
.es_version:
        mov di, v_version
        jmp .guarda
.es_descarga:
        mov di, v_descarga
        jmp .guarda
.es_nota:
        mov di, v_nota
.guarda:
        mov si, p_valor
.g:
        lodsb
        stosb
        test al, al
        jnz .g
        mov si, [p_si]              ; recuperar la posición en el cuerpo
.salta:
        mov al, [si]
        test al, al
        jz .r
        inc si
        cmp al, 10
        jne .salta
        jmp .linea
.r:
        ret

; recorta los espacios y tabuladores del final de p_valor
recorta:
        mov di, p_valor
.f:
        cmp byte [di], 0
        je .fin
        inc di
        jmp .f
.fin:
        cmp di, p_valor
        je .r
        dec di
        cmp byte [di], ' '
        je .q
        cmp byte [di], 9
        je .q
        ret
.q:
        mov byte [di], 0
        jmp .fin
.r:
        ret

; DI = clave leída (mayúsculas, acaba en 0), BP = clave buscada; CF=0 si igual
kcmp:
        push di
        push bp
.l:
        mov al, [bp]
        cmp al, [di]
        jne .no
        test al, al
        jz .si
        inc di
        inc bp
        jmp .l
.si:
        pop bp
        pop di
        clc
        ret
.no:
        pop bp
        pop di
        stc
        ret

; SI = «mayor.menor[.parche]» -> EAX = mayor*10000 + menor*100 + parche
; CF = 1 si no es un número de versión
calc_num:
        push ebx
        call parse_num
        jc .mal
        imul ebx, eax, 10000        ; mayor
        cmp byte [si], '.'
        jne .mal
        inc si
        call parse_num
        jc .mal
        imul eax, eax, 100          ; menor
        add ebx, eax
        cmp byte [si], '.'
        jne .ok
        inc si
        call parse_num
        jc .ok                      ; «.» sin cifras: parche 0
        add ebx, eax                ; parche
.ok:
        mov eax, ebx
        pop ebx
        clc
        ret
.mal:
        pop ebx
        stc
        ret

; =============================================================================
;  Utilidades de la línea de órdenes (como en DESCARGAR)
; =============================================================================
blancos:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        je .s
        ret
.s:
        inc si
        jmp blancos

; copiar una palabra de SI a DI (hasta 250)
palabra:
        mov cx, 250
.l:
        lodsb
        cmp al, ' '
        jbe .f
        stosb
        loop .l
.f:
        dec si
        mov byte [di], 0
        ret

; ---- url -> host, puerto, ruta ----
url_partir:
        mov si, url
        mov di, s_https
        call prefijo
        jnc .cifr
        mov di, s_http
        call prefijo
        jc .sin
        add si, 7
.sin:
        mov di, host
        mov cx, 120
.h:
        lodsb
        test al, al
        jz .hf
        cmp al, '/'
        je .hf
        cmp al, ':'
        je .hf
        cmp al, '?'
        je .hf
        stosb
        loop .h
.hf:
        mov byte [di], 0
        cmp di, host
        je .mal
        mov word [puerto], 80
        cmp al, ':'
        jne .np
        call parse_num
        jc .mal
        test eax, eax
        jz .mal
        cmp eax, 65535
        ja .mal
        mov [puerto], ax
        lodsb
.np:
        mov di, ruta
        mov byte [di], '/'
        inc di
        test al, al
        jz .rf
        cmp al, '/'
        je .r
        stosb
.r:
        mov cx, 240
.rl:
        lodsb
        test al, al
        jz .rf
        stosb
        loop .rl
.rf:
        mov byte [di], 0
        clc
        ret
.cifr:
        mov si, m_https
        stc
        ret
.mal:
        mov si, m_urlmal
        stc
        ret

; ¿empieza SI por DI (sin mirar mayúsculas)? CF = 0 si sí
prefijo:
        push si
.l:
        mov ah, [di]
        test ah, ah
        jz .si
        lodsb
        call upcase
        inc di
        cmp al, ah
        je .l
        pop si
        stc
        ret
.si:
        pop si
        clc
        ret

; ---- la petición: GET ruta HTTP/1.0 ... -> peti ----
peticion:
        mov di, peti
        mov si, s_get
        call copia
        mov si, ruta
        call copia
        mov si, s_http10
        call copia
        mov si, host
        call copia
        cmp word [puerto], 80
        je .np
        mov al, ':'
        stosb
        movzx eax, word [puerto]
        call num_a_texto
.np:
        mov si, s_cabeceras
        call copia
        mov cx, di                  ; CX = longitud de la petición (para tcp_enviar)
        sub cx, peti
        ret

copia:
        lodsb
        test al, al
        jz .r
        stosb
        jmp copia
.r:
        ret

; EAX -> texto en DI
num_a_texto:
        push ebx
        push edx
        push cx
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
        pop ax
        add al, '0'
        stosb
        loop .p
        pop cx
        pop edx
        pop ebx
        ret

; =============================================================================
;  Lo que llega por la conexión (lo llama tcp_in): SI = datos, CX = octetos.
;  h_estado: 0 cabeceras, 1 cuerpo (a body_buf), 2 error (se tira),
;  3 redirección (se tira)
; =============================================================================
act_datos:
        cmp byte [abortar], 0
        jne .r
        movzx eax, cx
        add [recibidos], eax        ; para notar el progreso
        mov al, [h_estado]
        cmp al, 1
        je .cuerpo
        test al, al
        jnz .r
.cab:
        ; acumular las cabeceras hasta la línea en blanco
        jcxz .r
        lodsb
        dec cx
        mov bx, [h_len]
        cmp bx, HDR_MAX - 1
        jae .larga
        mov [hdr+bx], al
        inc word [h_len]
        cmp al, 10
        jne .cab
        cmp bx, 3
        jb .cab
        cmp dword [hdr+bx-3], 0x0A0D0A0D
        je .fin_cab
        cmp word [hdr+bx-1], 0x0A0A
        jne .cab
.fin_cab:
        mov bx, [h_len]
        mov byte [hdr+bx], 0
        push si
        push cx
        call cabeceras
        pop cx
        pop si
        cmp byte [h_estado], 1
        jne .r
.cuerpo:
        ; copiar el cuerpo a body_buf (lo que pase de BODY_MAX se descarta)
        jcxz .r
.cc:
        jcxz .r
        mov bx, [body_len]
        cmp bx, BODY_MAX - 1
        jae .r
        lodsb
        mov [body_buf+bx], al
        inc word [body_len]
        dec cx
        jmp .cc
.r:
        ret
.larga:
        mov word [abortar_msg], m_nohttp
        mov byte [abortar], 1
        ret

; las cabeceras ya están en hdr (acaban en 0)
cabeceras:
        ; «HTTP/1.x NNN texto»
        mov si, hdr
        mov di, s_http_
        call prefijo
        jc .nohttp
.sp:
        lodsb
        test al, al
        jz .nohttp
        cmp al, ' '
        jne .sp
        call parse_num
        jc .nohttp
        mov [h_codigo], ax
        ; Location (para las redirecciones)
        mov si, hdr
.lin:
        lodsb
        test al, al
        jz .hecho
        cmp al, 10
        jne .lin
        mov di, s_loc
        call prefijo
        jc .lin
        add si, 9
        call blancos
        mov di, nueva_url
        mov cx, 250
.lc:
        lodsb
        cmp al, ' '
        jbe .lf
        stosb
        loop .lc
.lf:
        mov byte [di], 0
        dec si
        jmp .lin
.hecho:
        mov ax, [h_codigo]
        cmp ax, 200
        je .bien
        cmp ax, 301
        jb .mal
        cmp ax, 308
        ja .mal
        cmp ax, 304
        je .mal
        cmp ax, 305
        je .mal
        cmp ax, 306
        je .mal
        cmp byte [nueva_url], 0
        je .mal
        ; ¿a una dirección https://?
        mov si, nueva_url
        mov di, s_https
        call prefijo
        jnc .acifr
        ; ¿relativa («/otra»)? se le pone el servidor y el puerto
        cmp byte [nueva_url], '/'
        jne .abs
        mov di, url
        mov si, s_httpm
        call copia
        mov si, host
        call copia
        cmp word [puerto], 80
        je .rp
        mov al, ':'
        stosb
        movzx eax, word [puerto]
        call num_a_texto
.rp:
        mov si, nueva_url
        call copia
        mov byte [di], 0
        jmp .redir
.abs:
        mov si, nueva_url
        mov di, url
        mov cx, 251
        rep movsb
.redir:
        mov byte [h_estado], 3
        ret
.acifr:
        mov word [abortar_msg], m_redircifr
        mov byte [abortar], 1
        mov byte [h_estado], 2
        ret
.bien:
        ; 200: el cuerpo que viene es el archivo de versión
        mov byte [h_estado], 1
        mov word [body_len], 0
        ret
.nohttp:
        mov word [abortar_msg], m_nohttp
        mov byte [abortar], 1
        mov byte [h_estado], 2
        ret
.mal:
        ; el servidor dice que no: su primera línea
        mov byte [h_estado], 2
        mov si, hdr
        mov di, h_linea
        mov cx, 70
.ml:
        lodsb
        cmp al, 13
        je .mf
        cmp al, 10
        je .mf
        test al, al
        jz .mf
        stosb
        loop .ml
.mf:
        mov byte [di], 0
        mov word [abortar_msg], m_servidor
        mov byte [abortar], 1
        ret

%include "util.inc"
%include "red.inc"

m_uso       db "Mira por la red si hay una versión más nueva de Hispa-DOS que la", 13, 10
            db "instalada (", VER_STR, "). Solo avisa: no descarga ni instala nada.", 13, 10
            db 13, 10
            db "ACTUALIZAR [http://servidor[:puerto]/ruta]", 13, 10
            db 13, 10
            db "Sin dirección, usa la clave ACTUALIZAR= de \HISPADOS\RED.INI y, si no", 13, 10
            db "está, una de por omisión. El servidor da un archivo de texto con la", 13, 10
            db "última versión:", 13, 10
            db "  version=0.9.12", 13, 10
            db "  descarga=http://ejemplo/hispados/   (opcional)", 13, 10
            db "  nota=Lo nuevo de esta versión        (opcional)", 13, 10
            db 13, 10
            db "Solo HTTP sin cifrar: las direcciones https:// todavía no funcionan, y", 13, 10
            db "GitHub es todo https, así que hace falta un servidor propio por HTTP.", 13, 10
            db "Necesita HSPRED y la red configurada (ver CONFRED).", 13, 10, 0
m_mirando   db "Buscando actualizaciones de Hispa-DOS ", VER_STR, "...", 13, 10, 0
m_buscando  db "Servidor ", 0
m_puntos    db ": ", 0
m_aldia     db "Hispa-DOS ", VER_STR, " está al día.", 13, 10, 0
m_nueva     db "Hay una versión nueva: ", 0
m_punto     db ".", 13, 10, 0
m_desc      db "Descárgala en: ", 0
m_comoact   db "Para actualizar: arranca desde el USB nuevo y ejecuta INSTALAR.", 13, 10, 0
m_sinver    db "El servidor contestó, pero el archivo no trae la versión (version=).", 13, 10, 0
m_sinred    db "Cargue HSPRED y configure la red (CONFRED) antes de buscar", 13, 10
            db "actualizaciones.", 0
m_redir     db "El servidor manda a otra dirección: ", 0
m_muchas    db "Demasiadas redirecciones.", 0
m_https     db "Las direcciones https:// (cifradas) todavía no funcionan en Hispa-DOS;", 13, 10
            db "pruebe con http://, si el servidor lo admite.", 0
m_redircifr db "El servidor manda a una dirección https:// (cifrada), que todavía no", 13, 10
            db "funciona en Hispa-DOS.", 0
m_urlmal    db "Esa dirección no es válida (ejemplo: http://servidor/ultima.txt).", 0
m_nohttp    db "El servidor no contesta como un servidor web (HTTP).", 0
m_servidor  db "El servidor contesta: "
h_linea     times 72 db 0
m_parado    db "El servidor ha dejado de enviar (30 segundos sin nada).", 0
m_esc       db "Búsqueda interrumpida (Esc).", 0

; --- la dirección de por omisión (PROVISIONAL): se puede cambiar con la clave
;     ACTUALIZAR= de \HISPADOS\RED.INI ---
s_url_def   db "http://actualiza.clicgest.es/ultima.txt", 0

s_clave_act db "ACTUALIZAR", 0
k_version   db "VERSION", 0
k_descarga  db "DESCARGA", 0
k_nota      db "NOTA", 0
s_https     db "HTTPS://", 0
s_http      db "HTTP://", 0
s_httpm     db "http://", 0
s_http_     db "HTTP/", 0
s_loc       db "LOCATION:", 0
s_get       db "GET ", 0
s_http10    db " HTTP/1.0", 13, 10, "Host: ", 0
s_cabeceras db 13, 10, "User-Agent: Hispa-DOS/", VER_STR, " ACTUALIZAR", 13, 10
            db "Accept: */*", 13, 10
            db "Connection: close", 13, 10, 13, 10, 0

        section .bss
url         resb 256
nueva_url   resb 256
host        resb 128
ruta        resb 256
url_dado    resb 1
puerto      resw 1
srv_ip      resd 1
peti        resb 700
hdr         resb HDR_MAX + 4
h_len       resw 1
h_estado    resb 1
h_codigo    resw 1
n_redir     resb 1
abortar     resb 1
abortar_msg resw 1
recibidos   resd 1
rec_ult     resd 1
t_ult       resd 1
body_buf    resb BODY_MAX
body_len    resw 1
v_version   resb 32
v_descarga  resb 200
v_nota      resb 200
rem_num     resd 1
p_clave     resb 16
p_valor     resb 208
p_si        resw 1
i_clave     resb 16
; ini_ruta e ini_buf se reutilizan de red.inc (ya los declara)
num_txt     resb 8
u_numbuf    resb 16
u_lbuf      resb 84
bss_fin:
