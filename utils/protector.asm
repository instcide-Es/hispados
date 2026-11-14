; =============================================================================
;  Hispa-DOS PROTECTOR.COM (en el disco PROTECTO.COM: 8 letras) - protector
;  de pantalla en modo texto, residente: la «Lluvia verde»
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  PROTECTOR minutos   se queda en la memoria y salta tras esos minutos (1 a
;                      60) sin teclado ni ratón; si ya está, cambia los minutos
;  PROTECTOR /YA       lo enseña ahora          PROTECTOR /NO   lo quita
;  PROTECTOR           dice cómo está           PROTECTOR /?    la ayuda
;  (/S:n, sin documentar en la ayuda: n segundos en vez de minutos, para las
;  pruebas, tests/run_protector_tests.sh)
;
;  Cómo:
;  - INT 08h cuenta los tics sin hacer nada. Lo que es «hacer algo»: una
;    tecla pulsada (INT 09h, solo los códigos de pulsar; también las que
;    llegan por INT 2Fh AX=AD81h de HSPUSB o se meten en el búfer de la BIOS
;    sin pasar por INT 09h: se mira la cola del búfer) y mover el ratón o
;    pulsar un botón (INT 33h AX=0003h, preguntado desde INT 08h solo si
;    nadie está dentro de INT 33h: por eso se engancha a INT 33h; hace falta
;    cargarlo después del driver del ratón, HSPRATON).
;  - Pasado el tiempo, salta solo si la pantalla está en modo de texto de
;    80x25 (03h o 07h en la BIOS y el controlador de gráficos de la VGA en
;    modo texto), Hispa-DOS no está a medias (InDOS a cero o dentro de INT
;    28h, que es Hispa-DOS esperando una tecla), sin error crítico y sin
;    ningún motor de disquetera en marcha (escribiendo en el disco). Así no
;    salta con gráficos ni dentro de Hispa Ventanas (que tiene el suyo).
;  - No toca la pantalla del programa: dibuja en la última página de la
;    memoria de texto (la 8.ª; la 7.ª si el programa usa la 8.ª) y le dice
;    a la VGA que enseñe esa (registros 0Ch/0Dh del CRTC). El cursor, que está
;    en la página del programa, no se ve. Lo que el programa escriba mientras
;    tanto va a su página, como siempre. En color, los verdes oscuros se
;    consiguen cambiando un momento cuatro colores del DAC (1, 3, 4 y 5).
;  - Al pulsar una tecla (que se quita del búfer: no le llega al programa)
;    o mover el ratón, se vuelve a enseñar la página del programa y se dejan
;    los colores como estaban: la pantalla queda exactamente igual.
;  - Nunca llama a DOS ni a la BIOS desde las interrupciones (solo a INT 33h
;    para saber dónde está el ratón); al tocar los registros de la VGA
;    deja el índice como estaba, por si el programa estaba a medias.
;  INT 2Fh AH=D8h: AL=00h -> AL=FFh, BX='PR', ES=su segmento.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

NCOL    equ 80
NFIL    equ 25
NIV     equ 6                       ; colores: la cabeza y cinco de estela
MULTI   equ 0xD8                    ; INT 2Fh AH=D8h

start:
        jmp init

; =============================================================================
;  La parte residente
; =============================================================================
old08       dd 0
old09       dd 0
old16       dd 0
old28       dd 0
old2f       dd 0
old33       dd 0
indos       dd 0                    ; InDOS (y delante, el error crítico)
limite      dd 1092                 ; tics sin hacer nada antes de saltar
cuenta      dd 0
valor       dw 5                    ; lo que se ha pedido (minutos o segundos)
en_seg      db 0                    ; 1: «valor» son segundos (/S:n)
apagado     db 0                    ; /NO sin poder quitarlo de la memoria
activo      db 0                    ; se está enseñando
pendiente   db 0                    ; toca enseñarlo en cuanto se pueda
dentro      db 0                    ; ya se está en INT 08h o INT 28h
ocup33      db 0                    ; alguien está dentro de INT 33h
raton       db 0                    ; INT 33h enganchada: se mira el ratón
codigo      db 0                    ; la tecla que llega por INT 09h
cola        dw 0                    ; la cola del búfer del teclado
rat_x       dw 0
rat_y       dw 0
rat_b       dw 0
semilla     dw 0x5EED
; lo de la pantalla mientras se enseña
modo        db 0
dac_ok      db 0
crtc        dw 0x3D4
vseg        dw 0xB800
pagina      dw 0x7000               ; dónde se dibuja
ini_ant     dw 0                    ; la dirección de inicio que había
dac_ant     times 12 db 0           ; los colores 1, 3, 4 y 5 del DAC
; los atributos de la cabeza y la estela: en color (con el DAC cambiado) y
; en la pantalla monocroma
atr_color   db 0x0F, 0x0A, 0x01, 0x03, 0x04, 0x05
atr_mono    db 0x0F, 0x0F, 0x07, 0x07, 0x07, 0x07
atr         dw atr_color
dac_idx     db 1, 3, 4, 5           ; (con la paleta de la BIOS: los
dac_verde   db 0, 48, 0             ; atributos 1, 3, 4 y 5 son esos
            db 0, 34, 0             ; colores del DAC)
            db 0, 22, 0
            db 0, 12, 0
; las letras (página 437)
letras:
        db "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        db "ÑÑñÉáéíóúüÇç¿¡«»ºª½$%&@#*+=<>?!:"
        db "αßΓπΣσµτΦΘΩδ∞φε≡±÷≈√"
        db "│║░▒┼╬"
NLET        equ $ - letras
; las columnas
c_cab       times NCOL db 0         ; fila de la cabeza (con signo)
c_acc       times NCOL db 0         ; dieciseisavos de fila acumulados
c_vel       times NCOL db 0         ; velocidad: dieciseisavos por tic
c_lon       times NCOL db 0         ; largo de la estela
c_esp       times NCOL db 0         ; tics de espera antes de empezar

; ---- INT 2Fh: ¿está? y las teclas de HSPUSB (AX=AD81h, BL=código) ----
int2f:
        cmp ah, MULTI
        jne .k
        test al, al
        jne .k
        mov al, 0xFF
        mov bx, 'PR'
        push cs
        pop es
        iret
.k:
        cmp ax, 0xAD81
        jne .pasa
        cmp bl, 0x80                ; una tecla pulsada
        jae .k1
        mov dword [cs:cuenta], 0
        mov byte [cs:pendiente], 0
.k1:
        cmp byte [cs:activo], 0
        je .pasa
        cmp bl, 0x80
        jae .k2
        call apagar
.k2:
        mov ax, 0x4B01              ; atendida: no se mete en el búfer
        iret
.pasa:
        jmp far [cs:old2f]

; ---- INT 09h: el teclado ----
int09:
        push ax
        in al, 0x60
        mov [cs:codigo], al
        cmp al, 0xE0
        jae .t                      ; (E0h, E1h y respuestas: no son teclas)
        test al, al
        js .t
        mov dword [cs:cuenta], 0    ; una tecla pulsada: alguien está
        mov byte [cs:pendiente], 0
.t:
        cmp byte [cs:activo], 0
        jne .tragar
        pop ax
        jmp far [cs:old09]
.tragar:
        push ds
        mov ax, 0x40
        mov ds, ax
        push word [0x1C]            ; la cola antes de la tecla
        pushf
        call far [cs:old09]
        cli
        pop word [0x1C]             ; lo que haya metido la BIOS, fuera
        mov al, [cs:codigo]
        cmp al, 0xE0
        jae .x
        test al, al
        js .x
        call apagar
.x:
        pop ds
        pop ax
        iret

; ---- INT 16h: mientras se enseña, lo que entra por AH=05h no llega ----
int16:
        cmp byte [cs:activo], 0
        je .pasa
        cmp ah, 0x05
        jne .mira
        call apagar
        mov al, 0
        iret
.mira:
        call cola_mira              ; ¿ha entrado algo en el búfer?
.pasa:
        jmp far [cs:old16]

; ---- INT 33h: se cuenta quién está dentro ----
int33:
        inc byte [cs:ocup33]
        pushf
        call far [cs:old33]
        dec byte [cs:ocup33]
        iret

; ---- INT 28h: Hispa-DOS espera una tecla; si toca, se enseña ----
int28:
        cmp byte [cs:pendiente], 0
        je .pasa
        cli
        cmp byte [cs:dentro], 0
        jne .pasa
        mov byte [cs:dentro], 1
        push ax
        push ds
        push es
        push bx
        mov ax, 0x40
        mov ds, ax
        les bx, [cs:indos]
        cmp byte [es:bx-1], 0       ; ni con un error crítico
        jne .no
        call disco_quieto
        jc .no
        call es_texto
        jc .no
        call encender
.no:
        pop bx
        pop es
        pop ds
        pop ax
        mov byte [cs:dentro], 0
.pasa:
        jmp far [cs:old28]

; ---- INT 08h: el reloj ----
int08:
        pushf
        call far [cs:old08]
        cli
        cmp byte [cs:dentro], 0
        je .entra
        iret
.entra:
        mov byte [cs:dentro], 1
        pushad                      ; (los registros de 32 bits, también)
        push ds
        push es
        cmp byte [cs:apagado], 0
        jne .fin
        mov ax, 0x40
        mov ds, ax
        call es_texto
        jnc .texto
        mov byte [cs:activo], 0     ; con gráficos no se cuenta (y si el
        mov dword [cs:cuenta], 0    ; programa ha cambiado el modo mientras
        mov byte [cs:pendiente], 0  ; se enseñaba, ya no se enseña)
        mov ax, [0x1C]
        mov [cs:cola], ax
        jmp .fin
.texto:
        cmp byte [cs:activo], 0
        je .espera
        ; ---- se está enseñando ----
        call cola_mira
        cmp byte [cs:activo], 0
        je .fin
        call raton_mira
        jnc .r1
        call apagar
        jmp .fin
.r1:
        mov dx, [cs:crtc]           ; ¿sigue en nuestra página? (si el
        call inicio_lee             ; programa ha puesto el modo otra vez,
        mov bx, [cs:pagina]         ; ya no: se deja)
        shr bx, 1
        cmp ax, bx
        je .marco
        mov byte [cs:activo], 0
        mov dword [cs:cuenta], 0
        jmp .fin
.marco:
        push cs
        pop ds
        call marco
        jmp .fin
        ; ---- esperando ----
.espera:
        mov ax, [0x1C]              ; ¿ha entrado una tecla en el búfer?
        cmp ax, [cs:cola]
        je .e1
        mov [cs:cola], ax
        mov dword [cs:cuenta], 0
        mov byte [cs:pendiente], 0
.e1:
        call raton_mira
        jnc .e2
        mov dword [cs:cuenta], 0
        mov byte [cs:pendiente], 0
.e2:
        inc dword [cs:cuenta]
        mov eax, [cs:cuenta]
        cmp eax, [cs:limite]
        jb .e3
        mov byte [cs:pendiente], 1
        mov dword [cs:cuenta], 0
.e3:
        cmp byte [cs:pendiente], 0
        je .fin
        les bx, [cs:indos]          ; Hispa-DOS no puede estar a medias (si
        cmp word [es:bx-1], 0       ; espera una tecla, se hace en INT 28h)
        jne .fin
        call disco_quieto
        jc .fin
        call encender
.fin:
        pop es
        pop ds
        popad
        mov byte [cs:dentro], 0
        iret

; DS=40h -> CF=1 si la pantalla no es de texto de 80x25
es_texto:
        push ax
        push dx
        mov al, [0x49]
        cmp al, 3
        je .m
        cmp al, 7
        jne .no
.m:
        cmp word [0x4A], 80
        jne .no
        mov al, [0x84]              ; filas - 1 (0: una BIOS antigua, 25)
        test al, al
        jz .f
        cmp al, NFIL - 1
        jne .no
.f:
        mov dx, 0x3CE               ; el controlador de gráficos: registro 6,
        mov al, 6                   ; bit 0 = modo gráfico
        call vga_lee
        test al, 1
        jnz .no
        pop dx
        pop ax
        clc
        ret
.no:
        pop dx
        pop ax
        stc
        ret

; DS=40h -> CF=1 si alguna disquetera tiene el motor en marcha
disco_quieto:
        test byte [0x3F], 0x0F
        jz .s
        stc
        ret
.s:
        clc
        ret

; ¿ha entrado algo en el búfer del teclado mientras se enseña? Se quita y se
; acaba (las teclas que no pasan por INT 09h: HSPUSB sin 8042)
cola_mira:
        push ax
        push ds
        mov ax, 0x40
        mov ds, ax
        mov ax, [cs:cola]
        cmp ax, [0x1C]
        je .x
        mov [0x1C], ax
        call apagar
.x:
        pop ds
        pop ax
        ret

; -> CF=1 si el ratón se ha movido o ha cambiado un botón (solo si INT 33h
; es nuestra y nadie está dentro)
raton_mira:
        cmp byte [cs:raton], 0
        je .no
        cmp byte [cs:ocup33], 0
        jne .no
        push es
        push ax
        push bx
        push cx
        push dx
        xor ax, ax
        mov es, ax
        cmp word [es:0x33*4], int33
        jne .nada
        mov ax, cs
        cmp [es:0x33*4+2], ax
        jne .nada
        mov ax, 3
        int 0x33
        cmp cx, [cs:rat_x]
        jne .si
        cmp dx, [cs:rat_y]
        jne .si
        cmp bx, [cs:rat_b]
        jne .si
.nada:
        pop dx
        pop cx
        pop bx
        pop ax
        pop es
.no:
        clc
        ret
.si:
        mov [cs:rat_x], cx
        mov [cs:rat_y], dx
        mov [cs:rat_b], bx
        pop dx
        pop cx
        pop bx
        pop ax
        pop es
        stc
        ret

; ---- registros de la VGA, dejando el índice como estaba ----
; DX=puerto del índice AL=registro -> AL=valor
vga_lee:
        push bx
        mov bl, al
        in al, dx
        mov bh, al
        mov al, bl
        out dx, al
        inc dx
        in al, dx
        dec dx
        xchg al, bh
        out dx, al
        mov al, bh
        pop bx
        ret
; DX=puerto del índice AL=registro AH=valor
vga_pon:
        push ax
        push bx
        mov bx, ax
        in al, dx
        push ax
        mov al, bl
        out dx, al
        inc dx
        mov al, bh
        out dx, al
        dec dx
        pop ax
        out dx, al
        pop bx
        pop ax
        ret
; DX=CRTC -> AX=dirección de inicio (en caracteres)
inicio_lee:
        mov al, 0x0D
        call vga_lee
        mov ah, al
        mov al, 0x0C
        call vga_lee
        xchg al, ah
        ret
; DX=CRTC AX=dirección de inicio
inicio_pon:
        push ax
        push ax
        mov ah, al
        mov al, 0x0D
        call vga_pon
        pop ax
        mov al, 0x0C
        call vga_pon
        pop ax
        ret
; los cuatro colores del DAC: SI=de dónde DI=a dónde (DS=ES=CS)
dac_leer:
        push ax
        push bx
        push cx
        push dx
        xor bx, bx
.c:
        mov al, [dac_idx+bx]
        mov dx, 0x3C7
        out dx, al
        mov dx, 0x3C9
        mov cx, 3
.r:
        in al, dx
        stosb
        loop .r
        inc bx
        cmp bx, 4
        jb .c
        pop dx
        pop cx
        pop bx
        pop ax
        ret
dac_escribir:
        push ax
        push bx
        push cx
        push dx
        xor bx, bx
.c:
        mov al, [dac_idx+bx]
        mov dx, 0x3C8
        out dx, al
        inc dx
        mov cx, 3
.w:
        lodsb
        out dx, al
        loop .w
        inc bx
        cmp bx, 4
        jb .c
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; ---- números al azar (xorshift de 16 bits) ----
; AX=n -> AX=de 0 a n-1
azar:
        push bx
        push dx
        mov bx, ax
        mov ax, [cs:semilla]
        mov dx, ax
        shl dx, 7
        xor ax, dx
        mov dx, ax
        shr dx, 9
        xor ax, dx
        mov dx, ax
        shl dx, 8
        xor ax, dx
        mov [cs:semilla], ax
        mul bx
        mov ax, dx
        pop dx
        pop bx
        ret
; -> AL=una letra
letra:
        mov ax, NLET
        call azar
        mov bx, ax
        mov al, [cs:letras+bx]
        ret

; ---- enseñarlo (DS=40h, interrupciones cerradas) ----
encender:
        pusha
        push ds
        push es
        mov al, [0x49]
        mov [cs:modo], al
        mov dx, [0x63]
        mov [cs:crtc], dx
        mov ax, 0x7000              ; la 8.ª página (o la 7.ª si es la suya)
        cmp byte [0x62], 7
        jne .p
        mov ax, 0x6000
.p:
        mov [cs:pagina], ax
        mov ax, [0x1C]
        mov [cs:cola], ax
        mov ax, [0x6C]              ; la semilla, del reloj
        xor ax, [cs:semilla]
        or al, 1                    ; (nunca cero)
        mov [cs:semilla], ax
        push cs
        pop ds
        mov ax, 0xB800
        mov word [atr], atr_color
        cmp byte [modo], 7
        jne .s
        mov ax, 0xB000
        mov word [atr], atr_mono
.s:
        mov [vseg], ax
        mov es, ax
        mov di, [pagina]            ; la página, vacía
        mov cx, NCOL * NFIL
        mov ax, 0x0020
        cld
        rep stosw
        xor bx, bx                  ; las gotas
.g:
        mov al, 1
        call nueva
        inc bx
        cmp bx, NCOL
        jb .g
        mov byte [dac_ok], 0        ; los verdes oscuros (en color)
        cmp byte [modo], 3
        jne .d
        push cs
        pop es
        mov di, dac_ant
        call dac_leer
        mov si, dac_verde
        call dac_escribir
        mov byte [dac_ok], 1
.d:
        mov dx, [crtc]
        call inicio_lee
        mov [ini_ant], ax
        mov ax, [pagina]
        shr ax, 1
        call inicio_pon
        mov byte [activo], 1
        mov byte [pendiente], 0
        mov dword [cuenta], 0
        pop es
        pop ds
        popa
        ret

; ---- quitarlo: la pantalla del programa, como estaba ----
apagar:
        cmp byte [cs:activo], 0
        je .x
        pusha
        push ds
        push es
        push cs
        pop ds
        push cs
        pop es
        mov byte [activo], 0
        mov byte [pendiente], 0
        mov dword [cuenta], 0
        mov dx, [crtc]
        call inicio_lee             ; (si el programa la ha cambiado, se deja)
        mov bx, [pagina]
        shr bx, 1
        cmp ax, bx
        jne .y
        mov ax, [ini_ant]
        call inicio_pon
        cmp byte [dac_ok], 0
        je .y
        mov si, dac_ant
        call dac_escribir
.y:
        mov byte [dac_ok], 0
        pop es
        pop ds
        popa
.x:
        ret

; ---- una gota nueva en la columna BX (AL=1: la primera vez) ----
nueva:
        push ax
        mov ax, 15
        call azar
        add al, 6
        mov [c_lon+bx], al          ; de 6 a 20 letras
        mov ax, 12
        call azar
        add al, 3
        mov [c_vel+bx], al          ; de 3/16 a 14/16 de fila por tic
        mov byte [c_acc+bx], 0
        pop ax
        push ax
        test al, al
        jz .otra
        mov ax, NFIL
        call azar
        not al
        mov [c_cab+bx], al          ; de -1 a -25: escalonadas
        mov ax, 12
        call azar
        mov [c_esp+bx], al
        pop ax
        ret
.otra:
        mov ax, 5
        call azar
        not al
        mov [c_cab+bx], al
        mov ax, 30
        call azar
        mov [c_esp+bx], al
        pop ax
        ret

; ---- un tic: las gotas bajan y cambian algunas letras (DS=CS) ----
marco:
        mov es, [vseg]
        cld
        xor bx, bx
.c:
        cmp byte [c_esp+bx], 0
        je .baja
        dec byte [c_esp+bx]
        jmp .sig
.baja:
        mov al, [c_acc+bx]
        add al, [c_vel+bx]
        cmp al, 16
        jb .a
        sub al, 16
        mov [c_acc+bx], al
        call paso
        jmp .sig
.a:
        mov [c_acc+bx], al
.sig:
        inc bx
        cmp bx, NCOL
        jb .c
        mov cx, 6                   ; seis letras de las estelas cambian
.m:
        mov ax, NCOL
        call azar
        mov bx, ax
        cmp byte [c_esp+bx], 0
        jne .ms
        movzx ax, byte [c_lon+bx]
        dec ax
        call azar
        inc ax                      ; distancia a la cabeza: 1 a largo-1
        movsx dx, byte [c_cab+bx]
        sub dx, ax
        js .ms
        cmp dx, NFIL
        jae .ms
        call celda
        call letra
        mov [es:di], al
.ms:
        loop .m
        ret

; BX=columna DX=fila -> DI=su sitio en la página
celda:
        push ax
        push dx
        mov ax, NCOL * 2
        mul dx
        mov di, ax
        pop dx
        add di, bx
        add di, bx
        add di, [pagina]
        pop ax
        ret

; la cabeza de la columna BX baja una fila
paso:
        pusha
        inc byte [c_cab+bx]
        movzx cx, byte [c_lon+bx]
        xor si, si                  ; distancia a la cabeza
.d:
        movsx dx, byte [c_cab+bx]
        sub dx, si
        js .fin                     ; de aquí arriba, fuera de la pantalla
        cmp dx, NFIL
        jae .s
        mov ax, NCOL * 2            ; (mul cambia DX)
        push dx
        mul dx
        pop dx
        mov di, ax
        add di, bx
        add di, bx
        add di, [pagina]
        test si, si
        jnz .estela
        push bx
        call letra                  ; la cabeza: letra nueva, blanca
        pop bx
        mov bp, [atr]
        mov ah, [ds:bp]
        mov [es:di], ax
        jmp .s
.estela:
        cmp si, cx
        jb .e1
        mov word [es:di], 0x0020    ; el final de la estela: se borra
        jmp .s
.e1:
        lea ax, [si-1]              ; nivel = 1 + (d-1)*5/largo
        mov dx, NIV - 1
        mul dx
        div cx
        inc ax
        mov bp, [atr]
        add bp, ax
        mov al, [ds:bp]
        mov [es:di+1], al
.s:
        inc si
        cmp si, cx
        jbe .d
.fin:
        movsx ax, byte [c_cab+bx]   ; ¿la estela ha salido por abajo?
        sub ax, cx
        cmp ax, NFIL
        jl .x
        mov al, 0
        call nueva
.x:
        popa
        ret

fin_res:

; =============================================================================
;  La parte que no se queda
; =============================================================================
init:
        cld
        mov si, 0x81
        call salta
        cmp word [si], '/?'
        je ayuda
        call buscar                 ; ES=el residente (CF=1 si no está)
        mov byte [ya_esta], 0
        jc .a
        mov byte [ya_esta], 1
.a:
        mov al, [si]
        cmp al, 13
        je estado
        cmp al, '/'
        jne .num
        mov ax, [si+1]
        and ax, 0xDFDF
        cmp ax, 'NO'
        je quitar
        cmp ax, 'YA'
        je ahora
        cmp ax, 'S:' & 0xDFDF
        jne mal
        cmp byte [si+2], ':'
        jne mal
        add si, 3
        call parse_num
        jc mal
        test eax, eax
        jz mal
        cmp eax, 3600
        ja mal
        mov [n_valor], ax
        mov byte [n_seg], 1
        imul eax, 182               ; 18,2 tics por segundo
        add eax, 5
        xor edx, edx
        mov ecx, 10
        div ecx
        jmp poner
.num:
        call parse_num
        jc mal
        cmp eax, 1
        jb malmin
        cmp eax, 60
        ja malmin
        mov [n_valor], ax
        mov byte [n_seg], 0
        imul eax, 1092              ; tics por minuto
        jmp poner

mal:
        mov si, m_mal
        call puts
        mov si, m_uso
        call puts
        mov ax, 0x4C01
        int 0x21
malmin:
        mov si, m_malmin
        call puts
        mov ax, 0x4C01
        int 0x21

ayuda:
        mov si, m_ayuda
        call puts
        mov si, m_uso
        call puts
        mov si, m_ayuda2
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- PROTECTOR: cómo está ----
estado:
        cmp byte [ya_esta], 0
        jne .si
        mov si, m_no_esta
        call puts
        mov ax, 0x4C00
        int 0x21
.si:
        cmp byte [es:apagado], 0
        je .act
        mov si, m_desact
        call puts
        mov ax, 0x4C00
        int 0x21
.act:
        mov si, m_esta
        call puts
        call tiempo_es
        mov si, m_esta2
        call puts
        mov si, m_rat_si
        cmp byte [es:raton], 0
        jne .r
        mov si, m_rat_no
.r:
        call puts
        mov ax, 0x4C00
        int 0x21

; escribe «5 minutos» o «1 minuto» (o segundos) con lo del residente ES
tiempo_es:
        movzx eax, word [es:valor]
        call putnum
        mov si, m_minuto
        cmp byte [es:en_seg], 0
        je .m
        mov si, m_segundo
.m:
        call puts
        cmp word [es:valor], 1
        je .x
        mov al, 's'
        call putc
.x:
        ret

; ---- PROTECTOR /YA ----
ahora:
        cmp byte [ya_esta], 0
        je .no
        cmp byte [es:apagado], 0
        jne .des
        mov byte [es:pendiente], 1
        mov ax, 0x4C00
        int 0x21
.no:
        mov si, m_no_esta
        call puts
        mov ax, 0x4C01
        int 0x21
.des:
        mov si, m_desact
        call puts
        mov ax, 0x4C01
        int 0x21

; ---- PROTECTOR /NO ----
quitar:
        cmp byte [ya_esta], 0
        jne .si
        mov si, m_no_esta
        call puts
        mov ax, 0x4C00
        int 0x21
.si:
        cli
        mov byte [es:apagado], 1
        mov byte [es:pendiente], 0
        sti
        ; los vectores tienen que seguir siendo suyos para soltarlos
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, es
        mov bx, int08
        cmp [0x08*4], bx
        jne .queda
        cmp [0x08*4+2], ax
        jne .queda
        mov bx, int09
        cmp [0x09*4], bx
        jne .queda
        cmp [0x09*4+2], ax
        jne .queda
        mov bx, int16
        cmp [0x16*4], bx
        jne .queda
        cmp [0x16*4+2], ax
        jne .queda
        mov bx, int28
        cmp [0x28*4], bx
        jne .queda
        cmp [0x28*4+2], ax
        jne .queda
        mov bx, int2f
        cmp [0x2F*4], bx
        jne .queda
        cmp [0x2F*4+2], ax
        jne .queda
        cmp byte [es:raton], 0
        je .r
        mov bx, int33
        cmp [0x33*4], bx
        jne .queda
        cmp [0x33*4+2], ax
        jne .queda
.r:
        cli
        mov si, old08
        mov di, 0x08*4
        call vec_vuelve
        mov si, old09
        mov di, 0x09*4
        call vec_vuelve
        mov si, old16
        mov di, 0x16*4
        call vec_vuelve
        mov si, old28
        mov di, 0x28*4
        call vec_vuelve
        mov si, old2f
        mov di, 0x2F*4
        call vec_vuelve
        cmp byte [es:raton], 0
        je .r2
        mov si, old33
        mov di, 0x33*4
        call vec_vuelve
.r2:
        sti
        pop ds
        mov ah, 0x49                ; ES: su memoria
        int 0x21
        mov si, m_quitado
        call puts
        mov ax, 0x4C00
        int 0x21
.queda:
        pop ds
        mov si, m_no_quita
        call puts
        mov ax, 0x4C00
        int 0x21

; DS=0 ES=residente SI=su vector viejo DI=sitio en la tabla
vec_vuelve:
        mov ax, [es:si]
        mov [di], ax
        mov ax, [es:si+2]
        mov [di+2], ax
        ret

; ---- PROTECTOR n (EAX=tics) ----
poner:
        mov [n_tics], eax
        cmp byte [ya_esta], 0
        je instalar
        cli                         ; ya está: los minutos nuevos
        mov eax, [n_tics]
        mov [es:limite], eax
        mov dword [es:cuenta], 0
        mov ax, [n_valor]
        mov [es:valor], ax
        mov al, [n_seg]
        mov [es:en_seg], al
        mov byte [es:apagado], 0
        sti
        call raton_enganchar        ; (por si el driver del ratón es de después)
        mov si, m_cambiado
        call puts
        call tiempo_es
        mov si, m_punto
        call puts
        mov ax, 0x4C00
        int 0x21

instalar:
        mov ax, 0x1A00              ; hace falta una VGA
        int 0x10
        cmp al, 0x1A
        je .vga
        mov si, m_novga
        call puts
        mov ax, 0x4C01
        int 0x21
.vga:
        mov eax, [n_tics]
        mov [limite], eax
        mov ax, [n_valor]
        mov [valor], ax
        mov al, [n_seg]
        mov [en_seg], al
        push es
        mov ah, 0x34
        int 0x21
        mov [indos], bx
        mov [indos+2], es
        pop es
        push ds
        mov ax, 0x40
        mov ds, ax
        mov ax, [0x1C]
        mov [cs:cola], ax
        pop ds
        mov al, 0x08
        mov bx, old08
        mov dx, int08
        call vec_poner
        mov al, 0x09
        mov bx, old09
        mov dx, int09
        call vec_poner
        mov al, 0x16
        mov bx, old16
        mov dx, int16
        call vec_poner
        mov al, 0x28
        mov bx, old28
        mov dx, int28
        call vec_poner
        mov al, 0x2F
        mov bx, old2f
        mov dx, int2f
        call vec_poner
        push cs
        pop es
        call raton_enganchar
        mov si, m_puesto
        call puts
        call tiempo_es
        mov si, m_puesto2
        call puts
        mov es, [0x2C]              ; el entorno no hace falta
        mov ah, 0x49
        int 0x21
        mov word [0x2C], 0
        mov dx, (fin_res - start + 0x100 + 15) / 16
        mov ax, 0x3100
        int 0x21

; AL=interrupción BX=dónde guardar la vieja DX=la nuestra (DS=CS)
vec_poner:
        push es
        push bx
        push ax
        mov ah, 0x35
        int 0x21                    ; ES:BX = la vieja
        mov ax, bx
        pop cx                      ; (AL = la interrupción)
        pop bx
        mov [bx], ax
        mov [bx+2], es
        mov ax, cx
        mov ah, 0x25
        int 0x21
        pop es
        ret

; ES=el residente: engancha INT 33h si hay driver del ratón y no lo está ya
raton_enganchar:
        cmp byte [es:raton], 0
        jne .x
        push es
        mov ax, 0x3533
        int 0x21
        mov ax, es
        or ax, bx
        jz .no
        cmp byte [es:bx], 0xCF      ; (un IRET solo: no hay driver)
        je .no
        mov cx, es
        pop es
        mov [es:old33], bx
        mov [es:old33+2], cx
        mov ax, 3                   ; dónde está ahora
        int 0x33
        mov [es:rat_x], cx
        mov [es:rat_y], dx
        mov [es:rat_b], bx
        push ds
        push es
        pop ds
        mov dx, int33
        mov ax, 0x2533
        int 0x21
        pop ds
        mov byte [es:raton], 1
        ret
.no:
        pop es
.x:
        ret

; -> ES=segmento del residente, CF=1 si no está
buscar:
        mov ax, MULTI << 8
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .no
        cmp bx, 'PR'
        jne .no
        clc
        ret
.no:
        push cs
        pop es
        stc
        ret

salta:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .x
.s:
        inc si
        jmp salta
.x:
        ret

%include "util.inc"

m_ayuda     db "Protector de pantalla en modo texto: la «Lluvia verde», columnas de", 13, 10
            db "letras verdes que caen.", 13, 10, 13, 10, 0
m_uso       db "PROTECTOR minutos   se queda en la memoria y salta tras esos minutos (de 1", 13, 10
            db "                    a 60) sin tocar el teclado ni el ratón; si ya está,", 13, 10
            db "                    cambia los minutos", 13, 10
            db "PROTECTOR /YA       lo enseña ahora", 13, 10
            db "PROTECTOR /NO       lo quita", 13, 10
            db "PROTECTOR           dice cómo está", 13, 10, 0
m_ayuda2    db 13, 10
            db "Una tecla o mover el ratón lo quita y la pantalla queda como estaba (la", 13, 10
            db "tecla no le llega al programa). Solo salta con la pantalla de texto de 80x25:", 13, 10
            db "nunca con gráficos ni dentro de Hispa Ventanas (que tiene el suyo), ni", 13, 10
            db "mientras se escribe en el disco. Ocupa 2,4 KB y se puede cargar con LH;", 13, 10
            db "para que el ratón lo quite, cárguelo después de HSPRATON. Ejemplo para", 13, 10
            db "AUTOEXEC.BAT:  LH PROTECTOR 5", 13, 10, 0
m_mal       db "PROTECTOR: parámetro no válido.", 13, 10, 0
m_malmin    db "PROTECTOR: los minutos van de 1 a 60.", 13, 10, 0
m_novga     db "PROTECTOR necesita una tarjeta VGA.", 13, 10, 0
m_no_esta   db "PROTECTOR no está en la memoria. Para ponerlo: PROTECTOR minutos (de 1 a 60).", 13, 10, 0
m_desact    db "PROTECTOR está desactivado. Para activarlo: PROTECTOR minutos (de 1 a 60).", 13, 10, 0
m_esta      db "PROTECTOR está en la memoria: salta tras ", 0
m_esta2     db " sin tocar el teclado ni el ratón.", 13, 10, 0
m_rat_si    db "El ratón también lo quita.", 13, 10, 0
m_rat_no    db "Sin ratón (no había driver al cargarlo): solo lo quita el teclado.", 13, 10, 0
m_puesto    db "PROTECTOR puesto: la «Lluvia verde» sale tras ", 0
m_puesto2   db " sin tocar el teclado", 13, 10
            db "ni el ratón. PROTECTOR /YA la enseña ahora y PROTECTOR /NO lo quita.", 13, 10, 0
m_cambiado  db "PROTECTOR: ahora salta tras ", 0
m_punto     db ".", 13, 10, 0
m_quitado   db "PROTECTOR quitado de la memoria.", 13, 10, 0
m_no_quita  db "PROTECTOR desactivado. No se puede quitar de la memoria porque otro programa", 13, 10
            db "residente se ha cargado después.", 13, 10, 0
m_minuto    db " minuto", 0
m_segundo   db " segundo", 0

ya_esta     db 0
n_seg       db 0
n_valor     dw 0
n_tics      dd 0
u_numbuf    times 16 db 0
u_lbuf      times 84 db 0
