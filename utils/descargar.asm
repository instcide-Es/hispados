; =============================================================================
;  Hispa-DOS DESCARGAR (en el disco DESCARGA.COM) - bajar un archivo por HTTP
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  DESCARGAR [http://]servidor[:puerto]/ruta [archivo]
;  HTTP/1.0 sin cifrar, con barra de progreso; sigue hasta 4 redirecciones a
;  otra dirección http://. HTTPS (cifrado) todavía no. Esc para.
;  Si no se dice el archivo, se usa el último trozo de la ruta (en 8.3).
;  Necesita HSPRED (u otro packet driver). Ver docs/RED.md.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

HDR_MAX     equ 2048

start:
        cld
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        mov word [t_rutina], http_datos
        ; la dirección y el archivo
        mov si, 0x81
        call blancos
        cmp word [si], '/?'
        je ayuda
        cmp byte [si], 13
        je uso
        mov di, url
        call palabra
        call blancos
        cmp byte [si], 13
        je .sinarch
        mov di, archivo
        call palabra
        mov byte [arch_dado], 1
.sinarch:
        call red_abrir
        jc error
        xor al, al
        call red_config
        jc error
.otra_url:
        call url_partir
        jc error
        cmp byte [arch_dado], 0
        jne .ad
        call nombre_de_ruta
.ad:
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
        mov si, m_tcprst
        jmp .falla
.corta:
        mov si, [abortar_msg]
.falla:
        push si
        call tcp_cerrar
        call crlf
        pop si
        call puts
        call crlf
        call borrar_arch
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
        ; empezar otra vez con la dirección nueva
        mov byte [h_estado], 0
        mov word [h_len], 0
        mov dword [recibidos], 0
        mov dword [total], 0
        mov byte [t_fin], 0
        jmp .otra_url
.muchas:
        mov si, m_muchas
        jmp .falla
.cuerpo:
        ; ¿llegó todo?
        mov eax, [total]
        test eax, eax
        jz .ok
        cmp eax, [recibidos]
        je .ok
        call crlf
        mov si, m_incompleto
        jmp .falla
.ok:
        mov byte [pct_ult], 0xFF
        mov byte [forzar], 1
        call progreso
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        mov word [fh], 0
        call crlf
        mov si, m_guardado
        call puts
        mov si, archivo
        call puts
        mov si, m_parent
        call puts
        mov eax, [recibidos]
        call putnum
        mov si, m_octp
        call puts
        xor al, al
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
        call borrar_arch
        mov al, 1
        jmp red_salir

uso:
        mov si, m_uso
        call puts
        mov ax, 0x4C01
        int 0x21
ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

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
        ; ¿«https://»?
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
        ; la ruta (con «/» delante)
        mov di, ruta
        mov byte [di], '/'
        inc di
        test al, al
        jz .rf
        cmp al, '/'
        je .r
        stosb                       ; «?...» directamente tras el servidor
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

; el nombre del archivo: el último trozo de la ruta, en 8.3 (lo de antes del
; último punto, 8 letras; lo de después, 3; las letras que no valen en DOS,
; fuera). Si no queda nada: INDEX.HTM
nombre_de_ruta:
        mov si, ruta
        mov bx, si                  ; principio del último trozo
.b:
        lodsb
        test al, al
        jz .e
        cmp al, '?'
        je .e
        cmp al, '#'
        je .e
        cmp al, '/'
        jne .b
        mov bx, si
        jmp .b
.e:
        dec si
        mov dx, si                  ; fin del trozo
        ; el último punto
        mov bp, dx
        mov si, bx
.p:
        cmp si, dx
        jae .pf
        cmp byte [si], '.'
        jne .pn
        mov bp, si
.pn:
        inc si
        jmp .p
.pf:
        mov si, bx
        mov di, archivo
        mov cx, 8
.n:
        cmp si, bp
        jae .nf
        lodsb
        call letra_dos
        jc .n
        jcxz .n
        stosb
        dec cx
        jmp .n
.nf:
        cmp di, archivo
        je .idx
        cmp bp, dx
        jae .z
        mov al, '.'
        stosb
        mov bx, di
        lea si, [bp+1]
        mov cx, 3
.x:
        cmp si, dx
        jae .xf
        lodsb
        call letra_dos
        jc .x
        jcxz .x
        stosb
        dec cx
        jmp .x
.xf:
        cmp di, bx
        jne .z
        dec di                      ; sin extensión: sin el punto
.z:
        mov byte [di], 0
        ret
.idx:
        mov si, s_index
        mov di, archivo
        mov cx, 10
        rep movsb
        ret

; AL = letra -> en mayúsculas si vale en un nombre de DOS; CF = 1 si no vale
letra_dos:
        call upcase
        cmp al, '0'
        jb .raro
        cmp al, '9'
        jbe .ok
        cmp al, 'A'
        jb .raro
        cmp al, 'Z'
        jbe .ok
        cmp al, '_'
        je .ok
.raro:
        cmp al, '-'
        je .ok
        cmp al, '~'
        je .ok
        cmp al, '!'
        je .ok
        stc
        ret
.ok:
        clc
        ret

; ---- la petición: GET ruta HTTP/1.0 ... -> peti, CX = octetos ----
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
        mov cx, di
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
;  Lo que llega por la conexión (lo llama tcp_in): SI = datos, CX = octetos
;  h_estado: 0 cabeceras, 1 cuerpo (al archivo), 2 error (se tira),
;  3 redirección (se tira)
; =============================================================================
http_datos:
        cmp byte [abortar], 0
        jne .r
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
        jcxz .r
        mov dx, si
        mov bx, [fh]
        mov ah, 0x40
        int 0x21
        jc .disco
        cmp ax, cx
        jne .disco
        movzx eax, cx
        add [recibidos], eax
        call progreso
.r:
        ret
.larga:
        mov word [abortar_msg], m_nohttp
        mov byte [abortar], 1
        ret
.disco:
        mov word [abortar_msg], m_disco
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
        ; Content-Length y Location
        mov si, hdr
.lin:
        lodsb
        test al, al
        jz .hecho
        cmp al, 10
        jne .lin
        mov di, s_clen
        call prefijo
        jc .nl
        add si, 15
        call blancos
        call parse_num
        jc .lin
        mov [total], eax
        jmp .lin
.nl:
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
        ; crear el archivo
        mov dx, archivo
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .nocrea
        mov [fh], ax
        mov byte [creado], 1
        mov byte [h_estado], 1
        mov si, m_guardando
        call puts
        mov si, archivo
        call puts
        mov si, m_puntos2
        call puts
        mov byte [pct_ult], 0xFF
        mov byte [forzar], 1
        call progreso
        ret
.nocrea:
        mov word [abortar_msg], m_nocrea
        mov byte [abortar], 1
        mov byte [h_estado], 2
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

; ---- barra de progreso ----
; [██████████░░░░░░░░░░░░░░░]  40%   12.345 de 30.000 octetos
progreso:
        mov eax, [total]
        test eax, eax
        jz .sin
        ; porcentaje
        mov eax, [recibidos]
        cmp eax, [total]
        jbe .p
        mov eax, [total]
.p:
        mov ecx, 100
        mul ecx
        div dword [total]
        cmp byte [forzar], 0
        jne .pinta
        cmp al, [pct_ult]
        je .r
.pinta:
        mov byte [forzar], 0
        mov [pct_ult], al
        push ax
        mov al, 13
        call putc
        mov al, '['
        call putc
        pop ax
        push ax
        movzx cx, al
        shr cx, 2                   ; 25 casillas
        mov dx, 25
        sub dx, cx
        jcxz .v
.ll:
        mov si, s_lleno
        call puts
        loop .ll
.v:
        mov cx, dx
        jcxz .ce
.vv:
        mov si, s_vacio
        call puts
        loop .vv
.ce:
        mov al, ']'
        call putc
        pop ax
        movzx eax, al
        mov cl, 4
        call putnum_w
        mov si, m_pct
        call puts
        mov eax, [recibidos]
        mov cl, 11
        call putnum_w
        mov si, m_de
        call puts
        mov eax, [total]
        call putnum
        mov si, m_octetos
        call puts
.r:
        ret
.sin:
        ; sin Content-Length: solo los octetos, cada 4 KB
        mov eax, [recibidos]
        shr eax, 12
        cmp byte [forzar], 0
        jne .ps
        cmp al, [pct_ult]
        je .r
.ps:
        mov byte [forzar], 0
        mov [pct_ult], al
        mov al, 13
        call putc
        mov si, m_llevo
        call puts
        mov eax, [recibidos]
        call putnum
        mov si, m_octetos
        call puts
        ret

borrar_arch:
        cmp byte [creado], 0
        je .r
        mov bx, [fh]
        test bx, bx
        jz .b
        mov ah, 0x3E
        int 0x21
.b:
        mov dx, archivo
        mov ah, 0x41
        int 0x21
        mov byte [creado], 0
.r:
        ret

%include "util.inc"
%include "red.inc"

m_uso       db "Baja un archivo de un servidor web (HTTP) y lo guarda en el disco.", 13, 10
            db 13, 10
            db "DESCARGAR [http://]servidor[:puerto]/ruta [archivo]", 13, 10
            db 13, 10
            db "  archivo   dónde guardarlo (si no se dice: el nombre de la ruta, o", 13, 10
            db "            INDEX.HTM)", 13, 10
            db 13, 10
            db "Solo HTTP sin cifrar: las direcciones https:// (cifradas) todavía no", 13, 10
            db "funcionan, y hoy casi todas las páginas lo son. Sirve para servidores de", 13, 10
            db "la red de casa o de archivos antiguos. Esc para la descarga.", 13, 10
            db "Necesita HSPRED (ver CONFRED).", 13, 10, 0
m_buscando  db "Servidor ", 0
m_puntos    db ": ", 0
m_guardando db "Guardando en ", 0
m_puntos2   db ":", 13, 10, 0
m_guardado  db "Guardado en ", 0
m_parent    db " (", 0
m_octp      db " octetos).", 13, 10, 0
m_pct       db "%", 0
m_de        db " de ", 0
m_octetos   db " octetos", 0
m_llevo     db "Recibidos ", 0
m_redir     db "El servidor manda a otra dirección: ", 0
m_muchas    db "Demasiadas redirecciones.", 0
m_https     db "Las direcciones https:// (cifradas) todavía no funcionan en Hispa-DOS;", 13, 10
            db "pruebe con http://, si el servidor lo admite.", 0
m_redircifr db "El servidor manda a una dirección https:// (cifrada), que todavía no", 13, 10
            db "funciona en Hispa-DOS.", 0
m_urlmal    db "Esa dirección no es válida (ejemplo: http://servidor/archivo.zip).", 0
m_nohttp    db "El servidor no contesta como un servidor web (HTTP).", 0
m_servidor  db "El servidor contesta: "
h_linea     times 72 db 0
m_nocrea    db "No se puede crear el archivo.", 0
m_disco     db "No se puede escribir en el disco (¿está lleno?).", 0
m_parado    db "El servidor ha dejado de enviar (30 segundos sin nada).", 0
m_esc       db "Descarga interrumpida (Esc).", 0
m_incompleto db "La descarga ha quedado incompleta.", 0
s_https     db "HTTPS://", 0
s_http      db "HTTP://", 0
s_httpm     db "http://", 0
s_http_     db "HTTP/", 0
s_clen      db "CONTENT-LENGTH:", 0
s_loc       db "LOCATION:", 0
s_index     db "INDEX.HTM", 0
s_get       db "GET ", 0
s_http10    db " HTTP/1.0", 13, 10, "Host: ", 0
s_cabeceras db 13, 10, "User-Agent: Hispa-DOS/0.9 DESCARGAR", 13, 10
            db "Accept: */*", 13, 10
            db "Connection: close", 13, 10, 13, 10, 0
s_lleno     db "█", 0
s_vacio     db "░", 0

        section .bss
url         resb 256
nueva_url   resb 256
host        resb 128
ruta        resb 256
archivo     resb 260
arch_dado   resb 1
puerto      resw 1
srv_ip      resd 1
peti        resb 700
hdr         resb HDR_MAX + 4
h_len       resw 1
h_estado    resb 1
h_codigo    resw 1
n_redir     resb 1
fh          resw 1
creado      resb 1
abortar     resb 1
abortar_msg resw 1
recibidos   resd 1
total       resd 1
rec_ult     resd 1
t_ult       resd 1
pct_ult     resb 1
forzar      resb 1
num_txt     resb 8
u_numbuf    resb 16
u_lbuf      resb 84
bss_fin:
