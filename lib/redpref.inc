; =============================================================================
;  Hispa-DOS - la red preferida (RED.INI: Preferir=AUTO|CABLE|WIFI) y las
;  tarjetas Wi-Fi que sabe usar HSPWIFI. Lo usan HSPRED /AUTO y HSPWIFI /AUTO
;  en la parte de instalación (16 bits, modo real). docs/RED.md, «Cable o
;  Wi-Fi».
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Quien lo incluye define RP_BUF (un búfer en DS, para leer RED.INI) y
;  RP_TAM (su tamaño); [rp_tam] (lo que se lee) se puede bajar antes de
;  llamar si el bloque de memoria del programa es más corto.
;
;    rp_leer       lee \HISPADOS\RED.INI de la unidad de arranque:
;                  [rp_pref] = 0 AUTO (también sin archivo o sin la clave),
;                  1 CABLE, 2 WIFI; [rp_haywifi] = 1 con HAYWIFI=SI (solo
;                  para las pruebas: como si hubiera una tarjeta Wi-Fi)
;    rp_wifi_pci   ¿hay una tarjeta Wi-Fi de las de rp_ids? (BIOS del PCI)
;                  CF=0 sí; CF=1 no, o no hay BIOS del PCI
;  Las claves, sin distinguir mayúsculas; se aceptan espacios alrededor del
;  «=»; las líneas que empiezan por «;», «#» o «[» (secciones) no cuentan.
; =============================================================================

rp_leer:
        pusha
        push es
        push ds
        pop es
        mov byte [rp_pref], 0
        mov byte [rp_haywifi], 0
        mov ax, 0x3305              ; la unidad de arranque
        int 0x21
        add dl, 'A' - 1
        mov [rp_ruta], dl
        mov dx, rp_ruta
        mov ax, 0x3D00
        int 0x21
        jc .x
        mov bx, ax
        mov dx, RP_BUF
        mov cx, [rp_tam]
        mov ah, 0x3F
        int 0x21
        jnc .lei
        xor ax, ax
.lei:
        mov bp, RP_BUF
        add bp, ax                  ; BP = el final
        mov ah, 0x3E
        int 0x21
        mov si, RP_BUF
.linea:
        call .blancos
        cmp si, bp
        jae .x
        mov di, rp_k_pref
        call .clave
        jc .l2
        call .letra
        mov ah, 1
        cmp al, 'C'
        je .pon
        mov ah, 2
        cmp al, 'W'
        je .pon
        mov ah, 0
.pon:
        mov [rp_pref], ah
        jmp .salta
.l2:
        mov di, rp_k_hay
        call .clave
        jc .salta
        call .letra
        cmp al, 'S'
        jne .salta
        mov byte [rp_haywifi], 1
.salta:
        cmp si, bp
        jae .x
        lodsb
        cmp al, 10
        jne .salta
        jmp .linea
.x:
        pop es
        popa
        ret
; (internas) saltar espacios y tabuladores
.blancos:
        cmp si, bp
        jae .br
        cmp byte [si], ' '
        je .b1
        cmp byte [si], 9
        jne .br
.b1:
        inc si
        jmp .blancos
.br:
        ret
; ¿empieza SI por la clave DI (en mayúsculas, acabada en 0) y un «=»? CF=0 sí
; (SI pasa al valor, sin los espacios); CF=1 no (SI no se mueve)
.clave:
        push si
.c1:
        cmp byte [di], 0
        je .c2
        cmp si, bp
        jae .cno
        lodsb
        cmp al, 'a'
        jb .c3
        cmp al, 'z'
        ja .c3
        sub al, 32
.c3:
        cmp al, [di]
        jne .cno
        inc di
        jmp .c1
.c2:
        call .blancos
        cmp si, bp
        jae .cno
        cmp byte [si], '='
        jne .cno
        inc si
        call .blancos
        add sp, 2
        clc
        ret
.cno:
        pop si
        stc
        ret
; la primera letra del valor, en mayúsculas (0 si no hay)
.letra:
        xor al, al
        cmp si, bp
        jae .lr
        mov al, [si]
        cmp al, 'a'
        jb .lr
        cmp al, 'z'
        ja .lr
        sub al, 32
.lr:
        ret

rp_wifi_pci:
        pushad
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .no
        test ah, ah
        jnz .no
        cmp edx, 0x20494350         ; «PCI »
        jne .no
        mov si, rp_ids
.otra:
        mov dx, [si]                ; fabricante (0 = fin de la tabla)
        test dx, dx
        jz .no
        mov cx, [si+2]              ; aparato
        push si
        mov ax, 0xB102
        xor si, si
        int 0x1A
        pop si
        jc .sig
        test ah, ah
        jz .si
.sig:
        add si, 4
        jmp .otra
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; las tarjetas Wi-Fi que sabe usar HSPWIFI (lib/wifi_ids.inc)
rp_ids:
%include "wifi_ids.inc"
rp_ruta     db "A:\HISPADOS\RED.INI", 0
rp_k_pref   db "PREFERIR", 0
rp_k_hay    db "HAYWIFI", 0
rp_tam      dw RP_TAM - 1
rp_pref     db 0
rp_haywifi  db 0
