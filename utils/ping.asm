; =============================================================================
;  Hispa-DOS PING.COM - ¿contesta otro equipo? (eco de ICMP)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  PING nombre|a.b.c.d [/N:n]   (n ecos; 4 si no se dice; Esc para)
;  Necesita HSPRED (u otro packet driver). Ver docs/RED.md.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        cld
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        mov word [n_ecos], 4
        ; el destino y /N:n
        mov si, 0x81
.o:
        lodsb
        cmp al, ' '
        je .o
        cmp al, 9
        je .o
        cmp al, 13
        je .fin
        cmp al, '/'
        je .op
        cmp word [destino], 0
        jne uso
        dec si
        mov [destino], si
.sal:
        lodsb
        cmp al, ' '
        ja .sal
        dec si
        mov byte [si], 0            ; (el nombre acaba aquí)
        cmp al, 13
        je .fin
        inc si
        jmp .o
.op:
        lodsb
        cmp al, '?'
        je ayuda
        call upcase
        cmp al, 'N'
        je .n
        cmp al, 'T'                 ; /T: hasta que se pulse Esc
        jne uso
        mov word [n_ecos], 0xFFFF
        jmp .o
.n:
        lodsb
        cmp al, ':'
        je .nn
        cmp al, ' '
        jne uso
.nn:
        call parse_num
        jc uso
        test eax, eax
        jz uso
        cmp eax, 0xFFFE
        ja uso
        mov [n_ecos], ax
        jmp .o
.fin:
        cmp word [destino], 0
        je uso
        call red_abrir
        jc error
        xor al, al
        call red_config
        jc error
        mov si, [destino]
        call dns_buscar
        jc error
        mov [dst_ip], eax
        mov si, m_haciendo
        call puts
        mov si, [destino]
        call puts
        mov eax, [dst_ip]
        mov si, [destino]
        push eax
        call ip_texto               ; ¿era ya una dirección?
        pop eax
        jnc .sinip
        mov si, m_abre
        call puts
        call ip_poner
        mov al, ']'
        call putc
.sinip:
        mov si, m_con32
        call puts
.eco:
        inc word [seq]
        inc word [enviados]
        mov byte [ping_ok], 0
        call tic
        mov [t0], eax
        mov eax, [dst_ip]
        mov bx, [seq]
        call icmp_eco
        jc .nores
        mov bx, ping_ok
        mov cx, 37                  ; 2 s
        call red_esperar
        jc .tiempo
        call tic
        sub eax, [t0]
        mov [t1], eax
        inc word [recibidos]
        mov si, m_resp
        call puts
        mov eax, [dst_ip]
        call ip_poner
        mov si, m_oct
        call puts
        movzx eax, word [ping_oct]
        call putnum
        mov eax, [t1]
        test eax, eax
        jnz .ms
        mov si, m_menos55
        call puts
        jmp .ttl
.ms:
        mov si, m_tiempo
        call puts
        imul eax, eax, 55
        call putnum
        mov si, m_ms
        call puts
.ttl:
        mov si, m_ttl
        call puts
        movzx eax, byte [ping_ttl]
        call putnum
        call crlf
        jmp .sig
.nores:
        mov si, m_noarp
        call puts
        call crlf
        jmp .sig
.tiempo:
        cmp byte [red_esc], 0
        jne .resumen
        mov si, m_agotado
        call puts
        call crlf
.sig:
        cmp byte [red_esc], 0
        jne .resumen
        mov ax, [seq]
        cmp ax, [n_ecos]
        jae .resumen
        ; un segundo entre uno y otro
        call tic
        mov [t0], eax
.w:
        call red_sondear
        call esc_pulsado
        jc .resumen
        call tic
        sub eax, [t0]
        cmp eax, 18
        jb .w
        jmp .eco
.resumen:
        mov si, m_est1
        call puts
        mov eax, [dst_ip]
        call ip_poner
        mov si, m_est2
        call puts
        movzx eax, word [enviados]
        call putnum
        mov si, m_est3
        call puts
        movzx eax, word [recibidos]
        call putnum
        mov si, m_est4
        call puts
        movzx eax, word [enviados]
        sub ax, [recibidos]
        call putnum
        mov si, m_est5
        call puts
        movzx eax, word [enviados]
        sub ax, [recibidos]
        imul eax, eax, 100
        xor edx, edx
        movzx ecx, word [enviados]
        div ecx
        call putnum
        mov si, m_est6
        call puts
        cmp word [recibidos], 0
        mov al, 0
        jne .s
        mov al, 1
.s:
        jmp red_salir

error:
        call puts
        call crlf
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

%include "util.inc"
%include "red.inc"

m_uso       db "Comprueba si otro equipo contesta por la red (eco de ICMP).", 13, 10
            db 13, 10
            db "PING nombre|dirección [/N:n] [/T]", 13, 10
            db 13, 10
            db "  nombre     un nombre (se busca por DNS) o una dirección IP (10.0.2.2)", 13, 10
            db "  /N:n       n ecos (4 si no se dice)", 13, 10
            db "  /T         hasta que se pulse Esc", 13, 10
            db "Esc para en cualquier momento. Necesita HSPRED (ver CONFRED).", 13, 10, 0
m_haciendo  db 13, 10, "Haciendo ping a ", 0
m_abre      db " [", 0
m_con32     db " con 32 octetos de datos:", 13, 10, 0
m_resp      db "Respuesta desde ", 0
m_oct       db ": octetos=", 0
m_tiempo    db " tiempo=", 0
m_menos55   db " tiempo<55ms", 0
m_ms        db "ms", 0
m_ttl       db " TTL=", 0
m_agotado   db "Tiempo de espera agotado.", 0
m_noarp     db "No se llega a la dirección (nadie contesta en la red local).", 0
m_est1      db 13, 10, "Estadísticas de ping para ", 0
m_est2      db ":", 13, 10, "    Paquetes: enviados = ", 0
m_est3      db ", recibidos = ", 0
m_est4      db ", perdidos = ", 0
m_est5      db 13, 10, "    (", 0
m_est6      db "% perdidos)", 13, 10, 0

        section .bss
destino     resw 1
n_ecos      resw 1
seq         resw 1
enviados    resw 1
recibidos   resw 1
dst_ip      resd 1
t0          resd 1
t1          resd 1
u_numbuf    resb 16
u_lbuf      resb 84
bss_fin:
