; =============================================================================
;  HSPJOY.COM - mandos de juego de Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPJOY [/R] [/D] [/I:fichero]                    (en AUTOEXEC.BAT)
;
;    (sin opciones)  lee MANDO.INI (el que está junto a HSPJOY.COM), busca los
;                    mandos y, si hay alguno (o MANDO.INI pone uno en el
;                    puerto de impresora), queda residente. Si ya estaba
;                    cargado, vuelve a leer MANDO.INI y a buscar los mandos,
;                    sin cargar otro.
;    /R              quedar residente aunque no haya ningún mando
;    /I:fichero      otro fichero de configuración
;    /D              descargarlo de la memoria
;
;  Junta los mandos de todas las procedencias en una tabla y se los da a los
;  programas (docs/MANDOS.md):
;    - USB (HID genéricos y tipo Xbox 360): los lleva HSPUSB.SYS, que entrega
;      el estado de cada uno por INT 2Fh AX=D903h;
;    - puerto de juegos (201h) analógico, 2 o 4 ejes y 4 botones: se mide el
;      tiempo de cada eje (vueltas de un bucle con las interrupciones
;      cortadas) y se pasa a 0-255 con la calibración (mínimo, centro, máximo);
;    - puerto de impresora con cable casero: joystick Atari/Commodore y mando
;      de Mega Drive (puerto bidireccional), mandos de NES, Super Nintendo y
;      PlayStation (hasta dos), con los cables de docs/MANDOS.md.
;  Y los entrega así:
;    - INT 15h AH=84h (DX=0 botones, DX=1 ejes), como el mando clásico de la
;      BIOS: jugador 1 = mando A, jugador 2 (o la segunda palanca del 1) = B;
;    - INT 2Fh AH=D9h, la interfaz propia (para MANDO, Hispa Ventanas y el
;      puerto 201h virtual de HSPEMM): AL=00h ¿está? (ES:DI = datos), 01h el
;      mando clásico, 02h un mando de la tabla, 03h estado de un mando USB
;      (lo llama HSPUSB), 04h la configuración ha cambiado;
;    - mando a teclado: MANDO.INI asigna la cruceta y los botones a teclas,
;      que se entregan como si vinieran del teclado (orden D2h del 8042; sin
;      8042, al búfer de la BIOS), para los juegos sin mando.
;  Los mandos que hay que sondear (201h, impresora) se leen cuando un programa
;  los pide y, si hay mando a teclado, en cada tic del reloj (18,2 veces/s).
;
;  Con -DPRUEBA (HSPJOYT.COM, solo para tests/run_joy_tests.sh) las entradas
;  y salidas del puerto de juegos y del de impresora van a un simulador
;  (tests/progs/joysim.inc): QEMU no tiene 201h ni deja mover las entradas
;  del puerto de impresora.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

%define HJ_SOLO_DEFS
%include "hidjoyx.inc"

JOY_MAX     equ 6                   ; 0-1 USB, 2-3 puerto de juegos A y B, 4-5 impresora
M_SZ        equ 32                  ; bytes por mando: HJS_* y:
M_RET       equ 18                  ;   dw botones pulsados desde el último tic
M_RETC      equ 20                  ;   db cruceta pulsada desde el último tic
; tipos de mando (HJS_TIPO): 1 USB HID, 2 USB tipo Xbox 360 y
T_JUEGOS    equ 3                   ; puerto de juegos analógico
T_ATARI     equ 4                   ; joystick Atari/Commodore (impresora)
T_MEGA      equ 5                   ; Mega Drive (impresora)
T_NES       equ 6
T_SNES      equ 7
T_PSX       equ 8                   ; PlayStation
NTECLAS     equ 20                  ; arriba, abajo, izquierda, derecha, botones 1-16

%ifdef PRUEBA
 %define PIN  call sim_in
 %define POUT call sim_out
%else
 %define PIN  in al, dx
 %define POUT out dx, al
%endif

start:
        jmp init

; ---- datos: lo que da INT 2Fh AX=D900h en ES:DI (docs/MANDOS.md) ----
        align 2
datos:
            db "HJ"                 ; +00 firma
            db 1                    ; +02 versión de esta estructura
            db JOY_MAX              ; +03 mandos de la tabla
            db M_SZ                 ; +04 bytes de cada uno
            db 0
            dw mandos               ; +06 la tabla de mandos
; el mando clásico (lo que se da a los juegos)
d_cbot      db 0xF0                 ; +08 bits 4-7 como el puerto 201h (0 = pulsado);
                                    ;     bits 0-3: hay eje X1, Y1, X2, Y2
d_cejes     db 128, 128, 128, 128   ; +09 X1 Y1 X2 Y2 (0-255)
d_j1e       db 0xFF                 ; +0D mando que hace de jugador 1 (FFh ninguno)
d_j2e       db 0xFF                 ; +0E y de jugador 2
            db 0
; ---- configuración (se copia al residente al volver a ejecutar HSPJOY) ----
cfg:
d_j1        db 0xFF                 ; +10 jugador 1: mando, o FFh = el primero que haya
d_j2        db 0xFF                 ; +11 jugador 2: mando, FFh = el siguiente, FEh ninguno
d_opc       db 0                    ; +12 bit 0: sondear en cada tic; bit 1: teclas del
                                    ;     jugador 1; bit 2: teclas del jugador 2
d_lptt      db 0                    ; +13 qué hay en el cable de impresora (T_*; 0 nada)
d_lpt       dw 0                    ; +14 puerto de impresora
d_gport     dw 0x201                ; +16 puerto de juegos (0 = no se mira)
d_gtmo      dw 2000                 ; +18 vueltas de espera como mucho
d_cal       dw 0, 200, 400          ; +1A calibración: mínimo, centro y máximo de X1,
            dw 0, 200, 400          ;     Y1, X2 e Y2 (en vueltas)
            dw 0, 200, 400
            dw 0, 200, 400
d_teclas    times 2 * NTECLAS db 0  ; +32 teclas de cada jugador (bit 7 = prefijo E0h)
gp_mask     db 0                    ; +5A ejes del puerto de juegos que existen
lpt_n       db 1                    ; +5B mandos en el cable de impresora (1 o 2)
kbc_ok      db 1                    ; +5C hay controlador de teclado (8042)
            db 0
gp_raw      dw 0, 0, 0, 0           ; +5E última medida del puerto de juegos (vueltas)
mandos      times JOY_MAX * M_SZ db 0
cfg_end:
%if (d_cbot - datos) != HJD_CBOT || (cfg - datos) != HJD_J1 || (d_cal - datos) != HJD_CAL || \
    (d_teclas - datos) != HJD_TECLAS || (gp_mask - datos) != HJD_GMASK || (gp_raw - datos) != HJD_GRAW
 %error "el bloque de datos de HSPJOY no coincide con HJD_* (lib/hidjoyx.inc, docs/MANDOS.md)"
%endif

old2f       dd 0
old15       dd 0
old08       dd 0
busy        db 0
rep_key     db 0
rep_cnt     db 0
            db 0
k_prev      dd 0, 0                 ; teclas pulsadas de cada jugador
sv_ss       dw 0
sv_sp       dw 0
gp_btn      db 0

; =============================================================================
;  INT 2Fh AH=D9h
; =============================================================================
int2f:
        cmp ah, 0xD9
        je .ours
        jmp far [cs:old2f]
.ours:
        cmp al, 3
        je .usb
        cmp al, 0
        je .check
        cmp al, 1
        je .clas
        cmp al, 2
        je .uno
        cmp al, 4
        je .cambio
%ifdef PRUEBA
        cmp al, 0x7F                ; ES:DI = el simulador
        jne .no
        push cs
        pop es
        mov di, sim
        iret
.no:
%endif
        xor al, al
        iret
.check:
        mov al, 0xFF
        mov bx, 0x484A              ; 'HJ'
        mov cx, 0x0100              ; versión 1.0
        push cs
        pop es
        mov di, datos
        iret
.clas:
        ; el mando clásico: AL = botones y ejes que hay, BL = X1, BH = Y1,
        ; CL = X2, CH = Y2
        push ds
        push cs
        pop ds
        call refresh
        mov al, [d_cbot]
        mov bx, [d_cejes]
        mov cx, [d_cejes+2]
        pop ds
        iret
.uno:
        ; DL = mando -> ES:DI = su entrada, AL = FFh; CF si no existe
        push bp
        mov bp, sp
        or byte [bp+6], 1
        cmp dl, JOY_MAX
        jae .ur
        and byte [bp+6], 0xFE
        push ds
        push cs
        pop ds
        call refresh
        pop ds
        push cs
        pop es
        push ax
        mov al, M_SZ
        mul dl
        mov di, ax
        add di, mandos
        pop ax
        mov al, 0xFF
.ur:
        pop bp
        iret
.usb:
        ; de HSPUSB: DL = mando USB (0 o 1), ES:DI = su estado (HJS_SZ bytes)
        cmp dl, 2
        jae .ack
        push si
        push di
        push cx
        push ds
        push es
        push ax
        push es
        pop ds
        mov si, di                  ; DS:SI = estado
        push cs
        pop es
        mov al, M_SZ
        mul dl
        mov di, ax
        add di, mandos              ; ES:DI = la entrada
        pushf
        cli
        mov ax, [si+HJS_BTN]
        or [es:di+M_RET], ax
        mov al, [si+HJS_CRUZ]
        or [es:di+M_RETC], al
        mov cx, HJS_SZ
        cld
        rep movsb
        popf
        pop ax
        pop es
        pop ds
        pop cx
        pop di
        pop si
.ack:
        mov al, 0xFF
        iret
.cambio:
        ; la configuración ha cambiado: soltar las teclas y recalcular
        push ds
        push cs
        pop ds
        mov byte [busy], 1
        pushad
        mov byte [d_j1e], 0xFF
        mov byte [d_j2e], 0xFF
        call keys_run               ; (sin jugadores: suelta lo que hubiera)
        popad
        mov byte [busy], 0
        call refresh
        pop ds
        mov al, 0xFF
        iret

; =============================================================================
;  INT 15h AH=84h: el mando de la BIOS
; =============================================================================
int15:
        cmp ah, 0x84
        je .j
.chain:
        jmp far [cs:old15]
.j:
        push ds
        push cs
        pop ds
        call refresh
        cmp byte [d_j1e], 0xFF
        jne .go
        pop ds                      ; sin mandos: lo que diga la BIOS
        jmp .chain
.go:
        push bp
        mov bp, sp
        and byte [bp+8], 0xFE       ; CF = 0
        test dx, dx
        jnz .ejes
        mov al, [d_cbot]
        and al, 0xF0
        jmp .r
.ejes:
        cmp dx, 1
        jne .mal
        mov dl, 1
        mov al, [d_cejes]
        call .esc
        push ax
        mov al, [d_cejes+1]
        call .esc
        mov bx, ax
        mov al, [d_cejes+2]
        call .esc
        mov cx, ax
        mov al, [d_cejes+3]
        call .esc
        mov dx, ax
        pop ax
        jmp .r
.mal:
        or byte [bp+8], 1           ; subfunción desconocida
        mov ah, 0x86
.r:
        pop bp
        pop ds
        iret
; eje AL (0-255) -> AX como lo daría la BIOS (0 si no hay ese eje; si lo
; hay, 1-383: una vez y media el valor, y nunca 0); DL = bit del eje
.esc:
        test [d_cbot], dl
        jz .e0
        xor ah, ah
        push bx
        mov bx, ax
        shr bx, 1
        add ax, bx
        inc ax
        pop bx
        shl dl, 1
        ret
.e0:
        xor ax, ax
        shl dl, 1
        ret

; =============================================================================
;  INT 08h: el reloj
; =============================================================================
int08:
        pushf
        call far [cs:old08]
        cmp byte [cs:busy], 0
        jne .ret
        mov byte [cs:busy], 1
        mov [cs:sv_ss], ss
        mov [cs:sv_sp], sp
        push cs
        pop ss
        mov sp, pila_fin
        sti
        pushad
        push ds
        push es
        push cs
        pop ds
        test byte [d_opc], 7
        jz .np
        call poll_all
.np:
        call clasico
        test byte [d_opc], 6
        jz .nk
        call keys_run
.nk:
        pop es
        pop ds
        popad
        cli
        mov ss, [cs:sv_ss]
        mov sp, [cs:sv_sp]
        mov byte [cs:busy], 0
.ret:
        iret

; leer los mandos que hay que sondear y rehacer el mando clásico (DS = CS);
; si ya se está en ello (un tic ha interrumpido) se dejan los últimos valores
refresh:
        cmp byte [busy], 0
        jne .r
        mov byte [busy], 1
        pushad
        push es
        call poll_all
        call clasico
        pop es
        popad
        mov byte [busy], 0
.r:
        ret

poll_all:
        cmp byte [gp_mask], 0
        je .l
        call gp_poll
.l:
        mov al, [d_lptt]
        cmp al, T_ATARI
        jb .r
        cmp al, T_MEGA
        ja .n
        call db9_poll
        ret
.n:
        cmp al, T_SNES
        ja .p
        call nes_poll
        ret
.p:
        cmp al, T_PSX
        jne .r
        call psx_poll
.r:
        ret

; ---- quién es cada jugador y el mando clásico ----
clasico:
        ; jugador 1
        mov al, [d_j1]
        cmp al, 0xFF
        jne .f1
        mov ah, 0xFF                ; el primero que haya
        call .sig
.f1:
        call .hay
        mov [d_j1e], al
        ; jugador 2
        mov al, [d_j2]
        cmp al, 0xFF
        jne .f2
        mov al, 0xFF
        cmp byte [d_j1e], 0xFF
        je .f2
        mov ah, 0xFF                ; el primero que haya y no sea el jugador 1
        call .sig
        cmp al, [d_j1e]
        jne .f2
        mov ah, al
        call .sig
.f2:
        call .hay
        cmp al, [d_j1e]
        jne .f3
        mov al, 0xFF
.f3:
        mov [d_j2e], al
        ; mando A
        mov dl, 0xF0                ; botones sueltos, sin ejes
        mov dword [d_cejes], 0x80808080
        mov al, [d_j1e]
        cmp al, 0xFF
        je .fin
        call .ptr                   ; SI = el mando
        call .xy
        mov [d_cejes], ax
        or dl, 3
        mov ax, [si+HJS_BTN]
        test al, 1
        jz .a1
        and dl, ~0x10
.a1:
        test al, 2
        jz .a2
        and dl, ~0x20
.a2:
        cmp byte [d_j2e], 0xFF
        jne .dos
        ; un solo mando: su segunda palanca y sus botones 3 y 4 hacen de B
        test al, 4
        jz .a3
        and dl, ~0x40
.a3:
        test al, 8
        jz .a4
        and dl, ~0x80
.a4:
        cmp byte [si+HJS_NAX], 4
        jb .fin
        mov ax, [si+HJS_EJES+2]
        mov [d_cejes+2], ax
        or dl, 0x0C
        jmp .fin
.dos:
        mov al, [d_j2e]
        call .ptr
        call .xy
        mov [d_cejes+2], ax
        or dl, 0x0C
        mov ax, [si+HJS_BTN]
        test al, 1
        jz .b1
        and dl, ~0x40
.b1:
        test al, 2
        jz .fin
        and dl, ~0x80
.fin:
        mov [d_cbot], dl
        ret
; AL = mando -> AL (FFh si no existe o no está presente)
.hay:
        cmp al, JOY_MAX
        jae .hn
        push si
        call .ptr
        test byte [si+HJS_FLAGS], 1
        pop si
        jnz .hr
.hn:
        mov al, 0xFF
.hr:
        ret
; el primer mando presente después de AH (FFh = desde el principio) -> AL
.sig:
        mov al, ah
.sl:
        inc al
        cmp al, JOY_MAX
        jae .sn
        push si
        call .ptr
        test byte [si+HJS_FLAGS], 1
        pop si
        jz .sl
        ret
.sn:
        mov al, 0xFF
        ret
; AL = mando -> SI
.ptr:
        push ax
        mov ah, M_SZ
        mul ah
        mov si, ax
        add si, mandos
        pop ax
        ret
; X e Y del mando SI -> AL, AH: la palanca, o la cruceta si está pulsada
.xy:
        mov ax, [si+HJS_EJES]
        mov bl, [si+HJS_CRUZ]
        test bl, 4
        jz .x1
        xor al, al
.x1:
        test bl, 8
        jz .x2
        mov al, 255
.x2:
        test bl, 1
        jz .x3
        xor ah, ah
.x3:
        test bl, 2
        jz .x4
        mov ah, 255
.x4:
        ret

; =============================================================================
;  Mando a teclado
; =============================================================================
; comparar lo pulsado por cada jugador con lo del tic anterior y mandar las
; teclas que cambian; repetir la última mantenida (0,5 s y unas 9 por segundo)
keys_run:
        xor bp, bp                  ; BP = jugador
.j:
        xor edx, edx                ; EDX = lo pulsado: bits 0-3 cruceta, 4-19 botones
        mov bx, bp
        mov al, [d_j1e+bx]
        cmp al, 0xFF
        je .cmp
        mov cx, bp
        mov ah, 2
        shl ah, cl
        test [d_opc], ah
        jz .cmp
        call clasico.ptr
        pushf
        cli
        movzx edx, word [si+HJS_BTN]
        or dx, [si+M_RET]
        mov word [si+M_RET], 0
        shl edx, 4
        mov al, [si+HJS_CRUZ]
        or al, [si+M_RETC]
        mov byte [si+M_RETC], 0
        popf
        and al, 15
        or dl, al
        cmp byte [si+HJS_NAX], 2
        jb .cmp
        mov ax, [si+HJS_EJES]       ; la palanca también hace de cruceta
        cmp ah, 64
        jae .p1
        or dl, 1
.p1:
        cmp ah, 192
        jbe .p2
        or dl, 2
.p2:
        cmp al, 64
        jae .p3
        or dl, 4
.p3:
        cmp al, 192
        jbe .cmp
        or dl, 8
.cmp:
        mov bx, bp
        shl bx, 2
        mov eax, [k_prev+bx]
        mov [k_prev+bx], edx
        xor eax, edx                ; EAX = lo que cambia
        jz .next
        mov di, bp
        imul di, NTECLAS
        add di, d_teclas
        xor ecx, ecx
.k:
        bt eax, ecx
        jnc .kn
        push eax
        mov bx, cx
        mov al, [di+bx]
        test al, al
        jz .kp
        mov ah, 0x80                ; soltada
        bt edx, ecx
        jnc .ks
        xor ah, ah                  ; pulsada
        mov [rep_key], al
        mov byte [rep_cnt], 9
        jmp .kq
.ks:
        cmp al, [rep_key]
        jne .kq
        mov byte [rep_key], 0
.kq:
        call key_send
.kp:
        pop eax
.kn:
        inc cx
        cmp cx, NTECLAS
        jb .k
.next:
        inc bp
        cmp bp, 2
        jb .j
        ; repetición
        cmp byte [rep_key], 0
        je .r
        dec byte [rep_cnt]
        jnz .r
        mov byte [rep_cnt], 2
        mov al, [rep_key]
        xor ah, ah
        call key_send
.r:
        ret

; mandar la tecla AL (bit 7 = prefijo E0h) pulsada (AH = 0) o soltada (80h)
key_send:
        cmp byte [kbc_ok], 0
        je key_soft
        push ax
        test al, 0x80
        jz .n
        push ax
        mov al, 0xE0
        call kbc_put
        pop ax
.n:
        and al, 0x7F
        or al, ah
        call kbc_put
        pop ax
        ret

; el byte AL, como si lo mandara el teclado: orden D2h del 8042 (la BIOS y
; los juegos lo reciben por la IRQ 1 y el puerto 60h). Se espera a que se
; haya leído el anterior; la orden y su dato van con las interrupciones
; cortadas, para que no se cuele en medio otra orden al 8042 (HSPUSB).
kbc_put:
        push cx
        push ax
        mov ah, al
        xor cx, cx
.w:
        sti
        jmp short $+2
        cli
        in al, 0x64
        test al, 3
        jz .go
        loop .w
        jmp .out                    ; no se vacía: se pierde la tecla
.go:
        mov al, 0xD2
        out 0x64, al
        xor cx, cx
.w2:
        in al, 0x64
        test al, 2
        loopnz .w2
        mov al, ah
        out 0x60, al
.out:
        sti
        pop ax
        pop cx
        ret

; Sin 8042 (los Mac): la tecla va al búfer de la BIOS (INT 16h AH=05h) con
; su carácter, y Ctrl, Alt y Mayús a los indicadores de 0040:0017. Los
; juegos que leen el puerto 60h no la ven.
key_soft:
        pushad
        push es
        xor bx, bx
        mov es, bx
        mov bl, 4                   ; Ctrl
        cmp al, 0x1D
        je .mod
        mov bl, 8                   ; Alt
        cmp al, 0x38
        je .mod
        mov bl, 2                   ; Mayús
        cmp al, 0x2A
        je .mod
        test ah, ah
        jnz .r                      ; soltada: nada
        mov ch, al
        and ch, 0x7F
        xor cl, cl
        test al, 0x80
        jnz .put
        cmp al, 0x39
        ja .put
        movzx bx, al
        mov cl, [soft_ascii+bx]
.put:
        mov ah, 5
        int 0x16
        jmp .r
.mod:
        test ah, ah
        jnz .mo
        or [es:0x417], bl
        jmp .r
.mo:
        not bl
        and [es:0x417], bl
.r:
        pop es
        popad
        ret
soft_ascii  db 0, 27, "1234567890-=", 8, 9, "qwertyuiop[]", 13, 0, "asdfghjkl;'`", 0, "\zxcvbnm,./", 0, "*", 0, " "

; =============================================================================
;  Puerto de juegos (201h) analógico
; =============================================================================
; Al escribir en el puerto, los bits 0-3 se ponen a 1 y cada uno vuelve a 0
; al cabo de un tiempo proporcional a la resistencia del potenciómetro de su
; eje (X1, Y1, X2, Y2); los bits 4-7 son los botones (0 = pulsado). Se
; cuentan las vueltas del bucle hasta que baja cada bit: gp_raw.
gp_read:
        push ax
        push bx
        push cx
        push dx
        push di
        mov dx, [d_gport]
        mov bh, [gp_mask]
        mov cx, [d_gtmo]
        xor di, di
        pushf
        cli
        POUT
.l:
        PIN
        and al, bh
        cmp al, bh
        jne .chg
.c:
        inc di
        dec cx
        jnz .l
        mov al, bh                  ; se acabó la espera: los que quedan, al máximo
        xor bh, bh
        jmp .set
.chg:
        xor al, bh                  ; AL = los que acaban de bajar
        xor bh, al
.set:
        test al, 1
        jz .s1
        mov [gp_raw], di
.s1:
        test al, 2
        jz .s2
        mov [gp_raw+2], di
.s2:
        test al, 4
        jz .s3
        mov [gp_raw+4], di
.s3:
        test al, 8
        jz .s4
        mov [gp_raw+6], di
.s4:
        test bh, bh
        jz .done
        test cx, cx
        jnz .c
.done:
        popf
        PIN
        not al
        shr al, 4
        mov [gp_btn], al
        pop di
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; medir y pasar a las entradas 2 (mando A) y 3 (mando B)
gp_poll:
        call gp_read
        mov di, mandos + 2 * M_SZ
        mov si, d_cal
        mov bx, gp_raw
        mov cx, 4
.e:
        mov ax, [bx]
        call gp_norm
        push bx
        mov bx, 4
        sub bx, cx                  ; eje 0-3
        cmp bx, 2
        jb .ea
        mov [di+M_SZ+HJS_EJES+bx-2], al
        jmp .eb
.ea:
        mov [di+HJS_EJES+bx], al
.eb:
        pop bx
        add bx, 2
        add si, 6
        loop .e
        mov al, [gp_btn]
        test byte [gp_mask], 0x0C
        jnz .dos
        and ax, 0x0F                ; un solo mando: los cuatro botones
        mov [di+HJS_BTN], ax
        or [di+M_RET], ax
        ret
.dos:
        mov ah, al
        and ax, 0x0C03
        shr ah, 2
        push ax
        xor ah, ah
        mov [di+HJS_BTN], ax
        or [di+M_RET], ax
        pop ax
        mov al, ah
        xor ah, ah
        mov [di+M_SZ+HJS_BTN], ax
        or [di+M_SZ+M_RET], ax
        ret

; AX = vueltas, SI = calibración (mínimo, centro, máximo) -> AL = 0-255
gp_norm:
        push bx
        push cx
        push dx
        mov bx, [si+2]              ; centro
        cmp ax, bx
        ja .alto
        sub ax, [si]
        jbe .cero
        mov cx, bx
        sub cx, [si]
        jbe .cero
        mov dx, 128
        mul dx
        div cx
        cmp ax, 128
        jbe .r
        mov al, 128
        jmp .r
.cero:
        xor al, al
        jmp .r
.alto:
        sub ax, bx
        mov cx, [si+4]
        sub cx, bx
        jbe .tope
        cmp ax, cx
        jae .tope
        mov dx, 127
        mul dx
        div cx
        add al, 128
        jmp .r
.tope:
        mov al, 255
.r:
        pop dx
        pop cx
        pop bx
        ret

; =============================================================================
;  Puerto de impresora (cables de docs/MANDOS.md)
; =============================================================================
; pausa de unos microsegundos: CX lecturas del puerto de estado (cada acceso
; a un puerto ISA tarda alrededor de 1 µs, sea cual sea el procesador)
lpt_espera:
        push ax
        push dx
        mov dx, [d_lpt]
        inc dx
.l:
        in al, dx
        loop .l
        pop dx
        pop ax
        ret
%macro ESPERA 1
        mov cx, %1
        call lpt_espera
%endmacro

; ---- NES y Super Nintendo: registro de desplazamiento ----
; patilla 2 (D0) reloj, 3 (D1) carga, 4-9 (D2-D7) alimentación; datos del
; mando 1 en la 10 (ACK, bit 6 del estado) y del 2 en la 11 (BUSY, bit 7,
; que el puerto da invertido). Un 0 en la línea = botón pulsado.
NES_PWR     equ 0xFC
nes_poll:
        mov dx, [d_lpt]
        mov al, NES_PWR | 3         ; carga arriba
        POUT
        ESPERA 12
        mov al, NES_PWR | 1         ; carga abajo, reloj arriba
        POUT
        ESPERA 6
        xor si, si                  ; SI, DI = botones de los mandos 1 y 2
        xor di, di
        mov bx, 1
        mov bp, 8
        cmp byte [d_lptt], T_SNES
        jne .b
        mov bp, 12
.b:
        inc dx
        PIN
        dec dx
        test al, 0x40
        jnz .n1
        or si, bx
.n1:
        test al, 0x80
        jz .n2
        or di, bx
.n2:
        mov al, NES_PWR             ; reloj abajo...
        POUT
        ESPERA 6
        mov al, NES_PWR | 1         ; ...y arriba: el registro avanza
        POUT
        ESPERA 6
        shl bx, 1
        dec bp
        jnz .b
        mov bx, nes_tab
        cmp byte [d_lptt], T_SNES
        jne .t
        mov bx, snes_tab
.t:
        mov ax, si
        mov si, mandos + 4 * M_SZ
        call .pon
        cmp byte [lpt_n], 2
        jb .r
        mov ax, di
        mov si, mandos + 5 * M_SZ
        call .pon
.r:
        ret
; AX = bits leídos, BX = tabla, SI = mando
.pon:
        push di
        mov dx, 0x0FFF
        cmp bx, snes_tab
        je .p0
        mov dx, 0x00FF
.p0:
        cmp ax, dx                  ; todo pulsado: no hay mando (línea a 0)
        jne .p1
        xor ax, ax
.p1:
        xor dx, dx                  ; DL = cruceta, botones en DI
        xor di, di
        xor cx, cx
.pl:
        shr ax, 1
        jnc .pn
        push bx
        add bx, cx
        add bx, cx
        or di, [bx]
        pop bx
.pn:
        inc cx
        cmp cx, 12
        jb .pl
        mov ax, di
        shr ax, 12                  ; la cruceta va en los 4 bits altos de la tabla
        and di, 0x0FFF
        mov [si+HJS_CRUZ], al
        or [si+M_RETC], al
        mov [si+HJS_BTN], di
        or [si+M_RET], di
        pop di
        ret
; qué es cada bit, en el orden en que sale del mando: botón (bits 0-11) o
; cruceta (bit 12 arriba, 13 abajo, 14 izquierda, 15 derecha)
nes_tab     dw 0x0001, 0x0002, 0x0004, 0x0008      ; A B Select Start
            dw 0x1000, 0x2000, 0x4000, 0x8000, 0, 0, 0, 0
snes_tab    dw 0x0001, 0x0004, 0x0040, 0x0080      ; B Y Select Start
            dw 0x1000, 0x2000, 0x4000, 0x8000
            dw 0x0002, 0x0008, 0x0010, 0x0020      ; A X L R

; ---- PlayStation ----
; patilla 2 (D0) órdenes, 3 (D1) selección, 4 (D2) reloj, 5-9 alimentación;
; datos del mando 1 en la 10 y del 2 en la 11. Con la selección abajo, por
; cada byte: 8 bits, el menos significativo primero; se pone el bit de la
; orden con el reloj abajo y se lee el del mando al subirlo. Se manda 01h,
; 42h y ceros; el mando contesta su tipo (41h digital, 73h o 53h analógico:
; el dígito bajo son las palabras de 16 bits que siguen), 5Ah, dos bytes de
; botones (0 = pulsado) y, los analógicos, X e Y de la palanca derecha y de
; la izquierda.
PSX_PWR     equ 0xF8
psx_poll:
        mov dx, [d_lpt]
        mov al, PSX_PWR | 7         ; reposo: todo arriba
        POUT
        ESPERA 10
        mov al, PSX_PWR | 5         ; selección abajo
        POUT
        ESPERA 30
        xor si, si                  ; SI = byte
.by:
        mov bl, 0x01
        test si, si
        jz .c
        mov bl, 0x42
        cmp si, 1
        je .c
        xor bl, bl
.c:
        mov bp, 8
.bit:
        mov al, PSX_PWR             ; reloj abajo, con el bit de la orden
        shr bl, 1
        adc al, 0
        mov ah, al
        POUT
        ESPERA 8
        mov al, ah
        or al, 4                    ; reloj arriba: el mando ha puesto su bit
        POUT
        ESPERA 4
        inc dx
        PIN
        dec dx
        shr byte [psx_r1+si], 1     ; mando 1: patilla 10 tal cual
        shr byte [psx_r2+si], 1     ; mando 2: patilla 11, invertida
        test al, 0x40
        jz .d1
        or byte [psx_r1+si], 0x80
.d1:
        test al, 0x80
        jnz .d2
        or byte [psx_r2+si], 0x80
.d2:
        ESPERA 4
        dec bp
        jnz .bit
        ESPERA 30                   ; el mando avisa (ACK) entre byte y byte
        inc si
        cmp si, 5
        jb .by
        ; ¿siguen más? (los analógicos)
        cmp si, 9
        jae .fin
        mov al, [psx_r1+1]
        call .largo
        mov ah, al
        mov al, [psx_r2+1]
        call .largo
        cmp al, ah
        jae .m
        mov al, ah
.m:
        xor ah, ah
        cmp si, ax
        jb .by
.fin:
        mov al, PSX_PWR | 7
        POUT
        mov bx, psx_r1
        mov si, mandos + 4 * M_SZ
        call .pon
        cmp byte [lpt_n], 2
        jb .r
        mov bx, psx_r2
        mov si, mandos + 5 * M_SZ
        call .pon
.r:
        ret
; tipo AL -> AL = bytes de la respuesta (3 + 2 × palabras; 5 si no hay mando)
.largo:
        cmp al, 0xFF
        je .l5
        and al, 15
        shl al, 1
        add al, 3
        cmp al, 5
        jb .l5
        cmp al, 9
        jbe .lr
        mov al, 9
.lr:
        ret
.l5:
        mov al, 5
        ret
; respuesta BX -> mando SI
.pon:
        and byte [si+HJS_FLAGS], 0xFE
        cmp byte [bx+2], 0x5A
        jne .pr
        or byte [si+HJS_FLAGS], 1
        mov ax, [bx+3]
        not ax                      ; 1 = pulsado
        ; AL: Select, L3, R3, Start, arriba, derecha, abajo, izquierda
        ; AH: L2, R2, L1, R1, Triángulo, Círculo, Cruz, Cuadrado
        xor dl, dl
        test al, 0x10
        jz .u
        or dl, 1
.u:
        test al, 0x40
        jz .d
        or dl, 2
.d:
        test al, 0x80
        jz .l
        or dl, 4
.l:
        test al, 0x20
        jz .ri
        or dl, 8
.ri:
        mov [si+HJS_CRUZ], dl
        or [si+M_RETC], dl
        xor cx, cx                  ; botones 1-12
        mov di, psx_tab
.pb:
        shr ax, 1
        jnc .pn
        or cx, [di]
.pn:
        add di, 2
        cmp di, psx_tab + 32
        jb .pb
        mov [si+HJS_BTN], cx
        or [si+M_RET], cx
        mov byte [si+HJS_NAX], 0
        mov dword [si+HJS_EJES], 0x80808080
        mov al, [bx+1]
        and al, 15
        cmp al, 3
        jb .pr
        mov byte [si+HJS_NAX], 4
        mov ax, [bx+7]              ; palanca izquierda
        mov [si+HJS_EJES], ax
        mov ax, [bx+5]              ; y derecha
        mov [si+HJS_EJES+2], ax
.pr:
        ret
; botón de cada bit de los dos bytes: 1 Cruz, 2 Círculo, 3 Cuadrado,
; 4 Triángulo, 5 L1, 6 R1, 7 L2, 8 R2, 9 Select, 10 Start, 11 L3, 12 R3
psx_tab     dw 0x0100, 0x0400, 0x0800, 0x0200, 0, 0, 0, 0
            dw 0x0040, 0x0080, 0x0010, 0x0020, 0x0008, 0x0002, 0x0001, 0x0004
psx_r1      times 9 db 0xFF
psx_r2      times 9 db 0xFF

; ---- joystick Atari/Commodore y mando de Mega Drive ----
; Puerto bidireccional (bit 5 del registro de control = entrada): patillas
; 2-5 (D0-D3) arriba, abajo, izquierda y derecha, 6 (D4) el botón (patilla 6
; del DB9) y 7 (D5) el segundo (patilla 9); un 0 = pulsado. La patilla 1
; (STROBE) da los 5 V y la 14 (AUTOFD) es la selección de la Mega Drive
; (patilla 7 del DB9): arriba se leen B y C; abajo, A y Start, y las líneas
; de izquierda y derecha van a 0 (así se sabe que es un mando de Mega Drive).
db9_poll:
        mov dx, [d_lpt]
        add dx, 2
        mov al, 0x24                ; entrada; STROBE y AUTOFD arriba
        POUT
        ESPERA 8
        sub dx, 2
        PIN
        not al
        mov si, mandos + 4 * M_SZ
        mov bl, al
        and bl, 15
        mov [si+HJS_CRUZ], bl
        or [si+M_RETC], bl
        shr al, 4
        and ax, 3
        cmp byte [d_lptt], T_MEGA
        jne .pon
        push ax
        add dx, 2
        mov al, 0x26                ; selección abajo
        POUT
        ESPERA 8
        sub dx, 2
        PIN
        not al
        mov bl, al
        add dx, 2
        mov al, 0x24
        POUT
        pop ax
        mov bh, bl
        and bh, 0x0C
        cmp bh, 0x0C                ; ¿izquierda y derecha a 0? es de Mega Drive
        jne .pon
        shr bl, 2
        and bl, 0x0C
        or al, bl                   ; botones: 1 B, 2 C, 3 A, 4 Start
.pon:
        mov [si+HJS_BTN], ax
        or [si+M_RET], ax
        ret

%ifdef PRUEBA
%include "tests/progs/joysim.inc"
%endif

            align 2
pila        times 512 db 0
pila_fin:
resident_end:

; =============================================================================
;  Instalación (se descarta)
; =============================================================================
init:
        cld
        mov dx, msg_title
        call print
        ; ---- opciones ----
        mov si, 0x81
.o:
        lodsb
        cmp al, 13
        je .od
        cmp al, '/'
        jne .o
        lodsb
        cmp al, '?'
        je .uso
        and al, 0xDF
        cmp al, 'D'
        jne .o0
        mov byte [opt_d], 1
        jmp .o
.o0:
        cmp al, 'R'
        jne .o1
        mov byte [opt_r], 1
        jmp .o
.o1:
        cmp al, 'I'
        jne .uso
        lodsb
        cmp al, ':'
        jne .uso
        mov di, ini_path
.o2:
        lodsb
        cmp al, ' '
        jbe .o3
        stosb
        cmp di, ini_path + 78
        jb .o2
.o3:
        mov byte [di], 0
        mov byte [opt_i], 1
        dec si
        jmp .o
.uso:
        mov dx, msg_uso
        call print
        mov ax, 0x4C01
        int 0x21
.od:
        ; ---- ¿ya está? ----
        mov ax, 0xD900
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .nueva
        cmp bx, 0x484A
        jne .nueva
        mov [res_seg], es
        mov byte [instalado], 1
        cmp word [es:di+2], (JOY_MAX << 8) | 1
        je .mismo
        mov dx, msg_otra
        call print
        mov ax, 0x4C01
        int 0x21
.mismo:
        push cs
        pop es
        cmp byte [opt_d], 0
        jne descargar
        jmp .conf
.nueva:
        push cs
        pop es
        cmp byte [opt_d], 0
        je .conf
        mov dx, msg_noesta
        call print
        mov ax, 0x4C01
        int 0x21
.conf:
%ifdef PRUEBA
        ; lo que haya puesto la prueba en el simulador del residente vale
        ; también para buscar los mandos desde aquí
        cmp byte [instalado], 0
        je .ps
        push ds
        mov ds, [res_seg]
        mov si, sim
        mov di, sim
        mov cx, sim_fin - sim
        rep movsb
        pop ds
.ps:
%endif
        ; ---- configuración y mandos ----
        call ini_leer
        call detectar
        cmp byte [instalado], 0
        je .instalar
        ; ya residente: pasarle la configuración y los mandos sondeados
        ; (las entradas de los USB son suyas: las mantiene HSPUSB)
        mov es, [res_seg]
        cli
        mov si, cfg
        mov di, cfg
        mov cx, mandos - cfg
        rep movsb
        mov si, mandos + 2 * M_SZ
        mov di, si
        mov cx, cfg_end - (mandos + 2 * M_SZ)
        rep movsb
        sti
        mov ax, 0xD904
        int 0x2F
        mov dx, msg_actual
        call print
        call listar
        mov ax, 0x4C00
        int 0x21
.instalar:
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        mov dx, int08
        mov ax, 0x2508
        int 0x21
        ; los mandos USB se anuncian solos (HSPUSB, cuatro veces por segundo)
        push cs
        pop es
        xor ax, ax
        mov ds, ax
        mov bx, [0x46C]
.esp:
        mov ax, [0x46C]
        sub ax, bx
        cmp ax, 6
        jb .esp
        push cs
        pop ds
        mov [res_seg], cs
        call refresh
        call listar
        ; sin mandos (y sin /R ni cable de impresora en MANDO.INI) no se queda
        test bp, bp
        jnz .queda
        cmp byte [opt_r], 0
        jne .queda
        cmp byte [d_lptt], 0
        jne .queda
        push ds
        lds dx, [cs:old08]
        mov ax, 0x2508
        int 0x21
        lds dx, [cs:old15]
        mov ax, 0x2515
        int 0x21
        lds dx, [cs:old2f]
        mov ax, 0x252F
        int 0x21
        pop ds
        mov ax, 0x4C01
        int 0x21
.queda:
        cmp byte [opt_r], 0
        je .q1
        test bp, bp
        jnz .q1
        mov dx, msg_queda
        call print
.q1:
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, resident_end + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21

; ---- /D: quitar el residente ----
descargar:
        mov es, [res_seg]
        ; los tres vectores tienen que seguir siendo suyos
        xor ax, ax
        mov ds, ax
        mov ax, es
        cmp [0x2F*4+2], ax
        jne .no
        cmp [0x15*4+2], ax
        jne .no
        cmp [0x08*4+2], ax
        jne .no
        push cs
        pop ds
        ; soltar las teclas que tuviera pulsadas
        mov byte [es:d_opc], 0
        mov ax, 0xD904
        int 0x2F
        push ds
        lds dx, [es:old08]
        mov ax, 0x2508
        int 0x21
        lds dx, [es:old15]
        mov ax, 0x2515
        int 0x21
        lds dx, [es:old2f]
        mov ax, 0x252F
        int 0x21
        pop ds
        ; el puerto de impresora, como estaba (salida)
        cmp byte [es:d_lptt], 0
        je .lp
        mov dx, [es:d_lpt]
        add dx, 2
        mov al, 0x0C
        out dx, al
.lp:
        mov ah, 0x49
        int 0x21
        mov dx, msg_fuera
        call print
        mov ax, 0x4C00
        int 0x21
.no:
        push cs
        pop ds
        mov dx, msg_nofuera
        call print
        mov ax, 0x4C01
        int 0x21

; ---- buscar los mandos que se sondean ----
detectar:
        ; teclado: ¿hay 8042?
        in al, 0x64
        cmp al, 0xFF
        jne .k
        mov byte [kbc_ok], 0
.k:
        ; puerto de impresora
        cmp byte [d_lptt], 0
        je .gp
        xor ax, ax
        mov es, ax
        movzx bx, byte [lpt_num]
        dec bx
        shl bx, 1
        mov ax, [es:0x408+bx]
        push cs
        pop es
%ifdef PRUEBA
        mov ax, 0x378
%endif
        test ax, ax
        jnz .lp
        mov dx, msg_nolpt
        call print
        mov byte [d_lptt], 0
        jmp .gp
.lp:
        mov [d_lpt], ax
        mov si, mandos + 4 * M_SZ
        call .lpon
        cmp byte [lpt_n], 2
        jb .lb
        cmp byte [d_lptt], T_NES
        jb .lb                      ; (Atari y Mega Drive: uno solo)
        mov si, mandos + 5 * M_SZ
        call .lpon
.lb:
        cmp byte [d_lptt], T_MEGA
        ja .lq
        ; Atari y Mega Drive: el puerto tiene que ser bidireccional. Con las
        ; patillas de datos en entrada no se debe leer lo que se escribe.
        mov dx, [d_lpt]
        add dx, 2
        mov al, 0x24
        POUT
        sub dx, 2
        mov al, 0x55
        POUT
        PIN
        mov ah, al
        mov al, 0xAA
        POUT
        PIN
        cmp ax, 0x55AA
        jne .lq
        mov dx, msg_nobidi
        call print
.lq:
        call poll_all               ; (deja el puerto en reposo)
.gp:
        ; puerto de juegos
        mov byte [gp_mask], 0
        cmp word [d_gport], 0
        je .r
        ; espera máxima: las vueltas que caben en unos 3,5 ms (1/16 de tic)
        call .vueltas
        mov [d_gtmo], ax
        mov byte [gp_mask], 0x0F
        call gp_read
        xor bl, bl                  ; los ejes que han bajado a tiempo existen
        mov si, gp_raw
        mov cl, 1
.ge:
        mov ax, [si]
        cmp ax, [d_gtmo]
        jae .gn
        cmp ax, 2
        jb .gn
        or bl, cl
.gn:
        add si, 2
        shl cl, 1
        cmp cl, 16
        jb .ge
        ; un mando necesita sus dos ejes
        mov al, bl
        and al, 3
        cmp al, 3
        je .g1
        and bl, 0x0C
.g1:
        mov al, bl
        and al, 0x0C
        cmp al, 0x0C
        je .g2
        and bl, 3
.g2:
        test bl, 3
        jnz .g3
        xor bl, bl                  ; sin mando A no se usa el B
.g3:
        mov [gp_mask], bl
        test bl, bl
        jz .r
        ; calibración: la de MANDO.INI o, si no hay, el centro es donde está
        ; ahora la palanca y los extremos, un octavo y el doble menos eso
        cmp byte [cal_ini], 0
        jne .gc
        mov si, gp_raw
        mov di, d_cal
        mov cx, 4
.gd:
        lodsw
        mov bx, ax
        shr bx, 3
        mov [di], bx
        mov [di+2], ax
        shl ax, 1
        sub ax, bx
        mov [di+4], ax
        add di, 6
        loop .gd
.gc:
        mov si, mandos + 2 * M_SZ
        mov byte [si+HJS_TIPO], T_JUEGOS
        mov byte [si+HJS_FLAGS], 1
        mov word [si+HJS_NAX], 0x0402       ; 2 ejes, 4 botones
        mov ax, [d_gport]
        mov [si+HJS_VID], ax
        mov dword [si+HJS_EJES], 0x80808080
        test byte [gp_mask], 0x0C
        jz .gf
        mov byte [si+HJS_NBTN], 2
        add si, M_SZ
        mov byte [si+HJS_TIPO], T_JUEGOS
        mov byte [si+HJS_FLAGS], 1
        mov word [si+HJS_NAX], 0x0202
        mov [si+HJS_VID], ax
        mov dword [si+HJS_EJES], 0x80808080
.gf:
        call gp_poll
.r:
        ret
; entrada SI: un mando del cable de impresora
.lpon:
        mov al, [d_lptt]
        mov [si+HJS_TIPO], al
        mov byte [si+HJS_FLAGS], 1
        movzx bx, al
        mov al, [lpt_btn+bx-T_ATARI]
        mov byte [si+HJS_NAX], 0
        mov [si+HJS_NBTN], al
        mov ax, [d_lpt]
        mov [si+HJS_VID], ax
        mov dword [si+HJS_EJES], 0x80808080
        ret
; vueltas del bucle de medida en 1/16 de tic -> AX
.vueltas:
        push es
        xor ax, ax
        mov es, ax
        mov dx, [d_gport]
        xor edi, edi
        mov bx, [es:0x46C]
.v0:
        cmp bx, [es:0x46C]
        je .v0
        mov bx, [es:0x46C]
.v1:
        PIN
        and al, 0x0F
        cmp al, 0x1F                ; (como en gp_read: nunca es igual)
        je .v1
        inc edi
        cmp bx, [es:0x46C]
        je .v1
        shr edi, 4
        cmp edi, 0xFFF0
        jbe .v2
        mov edi, 0xFFF0
.v2:
        cmp edi, 16
        jae .v3
        mov edi, 16
.v3:
        mov ax, di
        pop es
        ret
lpt_btn     db 2, 4, 4, 8, 12       ; botones de Atari, Mega Drive, NES, SNES, PlayStation

; ---- escribir la lista de mandos del residente ----
listar:
        push es
        mov es, [res_seg]
        xor bx, bx
        xor bp, bp                  ; BP = cuántos
        mov si, mandos
.l:
        test byte [es:si+HJS_FLAGS], 1
        jz .n
        inc bp
        mov dx, msg_mando
        call print
        mov al, bl
        add al, '1'
        call putc
        mov dx, msg_dosp
        call print
        push bx
        movzx bx, byte [es:si+HJS_TIPO]
        cmp bx, T_PSX
        jbe .t
        xor bx, bx
.t:
        shl bx, 1
        mov dx, [tipos+bx]
        call print
        pop bx
        cmp byte [es:si+HJS_TIPO], 2
        ja .nu
        mov al, ' '
        call putc
        mov ax, [es:si+HJS_VID]
        call hex4
        mov al, ':'
        call putc
        mov ax, [es:si+HJS_PID]
        call hex4
.nu:
        mov dx, msg_par
        call print
        mov al, [es:si+HJS_NAX]
        call dec8
        mov dx, msg_ejes
        call print
        mov al, [es:si+HJS_NBTN]
        call dec8
        mov dx, msg_bot
        call print
.n:
        add si, M_SZ
        inc bx
        cmp bx, JOY_MAX
        jb .l
        test bp, bp
        jnz .hay
        mov dx, msg_ninguno
        call print
.hay:
        ; mando a teclado
        test byte [es:d_opc], 6
        jz .r
        mov dx, msg_teclas
        call print
.r:
        pop es
        ret                         ; BP = cuántos mandos hay

print:
        mov ah, 9
        int 0x21
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
hex4:
        push cx
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        ret
; AL en decimal
dec8:
        push ax
        push bx
        xor ah, ah
        mov bl, 10
        div bl
        test al, al
        jz .u
        add al, '0'
        call putc
.u:
        mov al, ah
        add al, '0'
        call putc
        pop bx
        pop ax
        ret

; =============================================================================
;  MANDO.INI
; =============================================================================
; [mandos]   jugador1= jugador2=  (auto, ninguno, usb1, usb2, juegos1,
;            juegos2, lpt1, lpt2)   lpt= (ninguno, atari, megadrive, nes,
;            snes, psx)   puerto= (1-3: LPT1-LPT3)   lpt_mandos= (1, 2)
;            juegos= (si, no)   sondeo= (si, no)
; [teclas1] y [teclas2]   arriba= abajo= izquierda= derecha= boton1= ...
;            boton16= (nombre de tecla)   activo= (si, no)
; calibrar=  doce números (mínimo, centro y máximo de X1, Y1, X2 e Y2); lo
;            escribe MANDO /CALIBRAR
ini_leer:
        cmp byte [opt_i], 0
        jne .abrir
        ; junto al programa: su ruta está detrás del entorno
        mov es, [0x2C]
        xor di, di
        xor al, al
        mov cx, 0x7FFF
.e:
        repne scasb
        scasb
        jne .e
        add di, 2                   ; ES:DI = la ruta del programa
        mov si, di
        mov di, ini_path
        mov bx, di
.c:
        mov al, [es:si]
        inc si
        mov [di], al
        inc di
        cmp al, '\'
        jne .c1
        mov bx, di                  ; tras la última barra
.c1:
        test al, al
        jz .c2
        cmp di, ini_path + 64
        jb .c
.c2:
        push cs
        pop es
        mov di, bx
        mov si, ini_nombre
.c3:
        lodsb
        stosb
        test al, al
        jnz .c3
.abrir:
        push cs
        pop es
        mov ax, 0x3D00
        mov dx, ini_path
        int 0x21
        jnc .ok
        cmp byte [opt_i], 0
        je .r                       ; sin MANDO.INI: todo automático
        mov dx, msg_noini
        call print
        ret
.ok:
        mov bx, ax
        ; [SB-VGA] lo que quepa en el bloque de memoria: con LH en un hueco
        ; de memoria superior pequeño (12 KB en QEMU con HSPSND y HSPDPMI
        ; arriba), los 6000 octetos de detrás del programa pasaban del final
        ; del bloque, MANDO.INI pisaba la pila (que DOS pone al final) y el
        ; arranque de «Juegos» se colgaba al cargar HSPJOY
        mov cx, INI_MAX
        mov ax, [2]                 ; fin del bloque (PSP)
        mov dx, cs
        sub ax, dx                  ; párrafos del bloque
        cmp ax, 0x1000
        jae .cabe
        shl ax, 4                   ; octetos
        sub ax, ini_buf + 0x301     ; (la pila, al final, y el 0 de detrás)
        jnc .hay
        xor ax, ax
.hay:
        cmp ax, cx
        jae .cabe
        mov cx, ax
.cabe:
        mov ah, 0x3F
        mov dx, ini_buf
        int 0x21
        jnc .rd
        xor ax, ax
.rd:
        mov si, ax
        mov byte [ini_buf+si], 0
        mov ah, 0x3E
        int 0x21
        ; línea a línea
        mov si, ini_buf
        mov word [ini_linea], 0
        mov byte [ini_sec], 0
.l:
        inc word [ini_linea]
        call .blancos
        mov al, [si]
        test al, al
        jz .fin
        cmp al, 26
        je .fin
        cmp al, ';'
        je .sig
        cmp al, '#'
        je .sig
        cmp al, 13
        je .sig
        cmp al, 10
        je .sig
        cmp al, '['
        jne .kv
        inc si
        mov di, t_sec
        call buscar
        mov byte [ini_sec], 0
        jc .sig                     ; sección desconocida: se salta
        mov [ini_sec], al
        cmp al, 2
        jb .sig
        ; una sección de teclas activa ese jugador (salvo activo=no)
        mov cl, al
        dec cl
        mov ah, 1
        shl ah, cl
        or [d_opc], ah
        jmp .sig
.kv:
        call clave
        jnc .sig
        mov dx, msg_inierr
        call print
        mov ax, [ini_linea]
        call dec8w
        mov dx, msg_crlf
        call print
.sig:
        lodsb                       ; hasta el final de la línea
        test al, al
        jz .fin
        cmp al, 10
        jne .sig
        jmp .l
.fin:
.r:
        ret
.blancos:
        cmp byte [si], ' '
        je .b1
        cmp byte [si], 9
        jne .b2
.b1:
        inc si
        jmp .blancos
.b2:
        ret

; una línea clave=valor (SI); CF si no se entiende
clave:
        ; boton<n>
        mov di, t_boton
        push si
        call prefijo
        jc .nb
        add sp, 2
        call numero
        jc .mal
        cmp ax, 1
        jb .mal
        cmp ax, 16
        ja .mal
        add al, 3
        jmp .tecla
.nb:
        pop si
        mov di, t_claves
        call buscar
        jc .mal
        cmp al, 4
        jb .tecla
        ; ---- las demás ----
        mov bl, al
        call igual
        jc .mal
        cmp bl, 20
        je .calibrar
        cmp bl, 10
        je .j1
        cmp bl, 11
        je .j2
        cmp bl, 12
        je .lpt
        cmp bl, 13
        je .puerto
        cmp bl, 14
        je .lptn
        cmp bl, 15
        je .juegos
        cmp bl, 16
        je .sondeo
        cmp bl, 17
        je .activo
        jmp .mal
.j1:
        mov di, t_mandos
        call buscar
        jc .mal
        mov [d_j1], al
        jmp .bien
.j2:
        mov di, t_mandos
        call buscar
        jc .mal
        mov [d_j2], al
        jmp .bien
.lpt:
        mov di, t_lpt
        call buscar
        jc .mal
        mov [d_lptt], al
        jmp .bien
.puerto:
        call numero
        jc .mal
        cmp ax, 1
        jb .mal
        cmp ax, 3
        ja .mal
        mov [lpt_num], al
        jmp .bien
.lptn:
        call numero
        jc .mal
        cmp ax, 1
        jb .mal
        cmp ax, 2
        ja .mal
        mov [lpt_n], al
        jmp .bien
.juegos:
        mov di, t_sino
        call buscar
        jc .mal
        test al, al
        jnz .bien
        mov word [d_gport], 0
        jmp .bien
.sondeo:
        mov di, t_sino
        call buscar
        jc .mal
        and byte [d_opc], 0xFE
        or [d_opc], al
        jmp .bien
.activo:
        mov di, t_sino
        call buscar
        jc .mal
        test al, al
        jnz .bien
        mov cl, [ini_sec]
        cmp cl, 2
        jb .mal
        dec cl
        mov ah, 1
        shl ah, cl
        not ah
        and [d_opc], ah
        jmp .bien
.calibrar:
        mov di, d_cal
        mov cx, 12
.cl:
        call numero
        jc .mal
        mov [di], ax
        add di, 2
        cmp byte [si], ','
        jne .cn
        inc si
.cn:
        loop .cl
        mov byte [cal_ini], 1
        jmp .bien
.tecla:
        ; AL = índice (0-3 cruceta, 4-19 botones) en la sección de teclas
        mov bl, al
        mov cl, [ini_sec]
        cmp cl, 2
        jb .mal
        call igual
        jc .mal
        push bx
        mov di, t_teclas
        call buscar
        pop bx
        jnc .tk
        ; una letra o una cifra
        mov al, [si]
        call minus
        mov ah, [si+1]
        cmp ah, ' '
        ja .mal
        cmp al, 'a'
        jb .dig
        cmp al, 'z'
        ja .mal
        sub al, 'a'
        push bx
        movzx bx, al
        mov al, [sc_letras+bx]
        pop bx
        jmp .tk
.dig:
        cmp al, '0'
        jb .mal
        cmp al, '9'
        ja .mal
        sub al, '0'
        jnz .d1
        mov al, 10
.d1:
        inc al                      ; 1-9 = 02h-0Ah, 0 = 0Bh
.tk:
        xor bh, bh
        cmp byte [ini_sec], 3
        jne .t1
        add bx, NTECLAS
.t1:
        mov [d_teclas+bx], al
.bien:
        clc
        ret
.mal:
        stc
        ret

; saltar blancos y el «=»; CF si no lo hay
igual:
        call ini_leer.blancos
        cmp byte [si], '='
        jne .no
        inc si
        call ini_leer.blancos
        clc
        ret
.no:
        stc
        ret

; AL en minúscula (y sin acento la ó de «botón»)
minus:
        cmp al, 0xA2                ; ó en CP437
        jne .a
        mov al, 'o'
.a:
        cmp al, 'A'
        jb .r
        cmp al, 'Z'
        ja .r
        add al, 32
.r:
        ret

; ¿empieza SI por la cadena DI (ASCIIZ, en minúsculas)? Si sí, SI la pasa
prefijo:
        mov al, [di]
        test al, al
        jz .si
        mov ah, al
        mov al, [si]
        call minus
        cmp al, ah
        jne .no
        inc si
        inc di
        jmp prefijo
.si:
        clc
        ret
.no:
        stc
        ret

; buscar la palabra de SI en la tabla DI (cadena ASCIIZ, valor; fin con 0)
; -> AL = valor y SI detrás de la palabra; CF si no está (SI no cambia)
buscar:
        push bx
        push dx
.e:
        cmp byte [di], 0
        je .no
        mov bx, si
.c:
        mov ah, [di]
        test ah, ah
        jz .fin
        mov al, [bx]
        call minus
        cmp al, ah
        jne .otra
        inc bx
        inc di
        jmp .c
.fin:
        ; la palabra tiene que acabar aquí
        mov al, [bx]
        call minus
        cmp al, 'a'
        jb .ok0
        cmp al, 'z'
        jbe .otra
.ok0:
        cmp al, '0'
        jb .ok
        cmp al, '9'
        jbe .otra
.ok:
        mov al, [di+1]
        mov si, bx
        pop dx
        pop bx
        clc
        ret
.otra:
        cmp byte [di], 0            ; a la entrada siguiente
        je .o1
        inc di
        jmp .otra
.o1:
        add di, 2
        jmp .e
.no:
        pop dx
        pop bx
        stc
        ret

; número decimal en SI -> AX; CF si no hay
numero:
        push bx
        push dx
        xor ax, ax
        xor dx, dx
        mov bl, [si]
        cmp bl, '0'
        jb .no
        cmp bl, '9'
        ja .no
.l:
        mov bl, [si]
        cmp bl, '0'
        jb .ok
        cmp bl, '9'
        ja .ok
        sub bl, '0'
        xor bh, bh
        mov dx, 10
        mul dx
        add ax, bx
        inc si
        jmp .l
.ok:
        pop dx
        pop bx
        clc
        ret
.no:
        pop dx
        pop bx
        stc
        ret

; AX en decimal
dec8w:
        push ax
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop dx
        pop cx
        pop bx
        pop ax
        ret

%macro PAL 2
        db %1, 0, %2
%endmacro
t_sec:
        PAL "mandos]", 1
        PAL "teclas1]", 2
        PAL "teclas2]", 3
        db 0
t_boton     db "boton", 0
t_claves:
        PAL "arriba", 0
        PAL "abajo", 1
        PAL "izquierda", 2
        PAL "derecha", 3
        PAL "jugador1", 10
        PAL "jugador2", 11
        PAL "lpt_mandos", 14
        PAL "lpt", 12
        PAL "puerto", 13
        PAL "juegos", 15
        PAL "sondeo", 16
        PAL "activo", 17
        PAL "calibrar", 20
        db 0
t_mandos:
        PAL "auto", 0xFF
        PAL "ninguno", 0xFE
        PAL "usb1", 0
        PAL "usb2", 1
        PAL "juegos1", 2
        PAL "juegos2", 3
        PAL "lpt1", 4
        PAL "lpt2", 5
        db 0
t_lpt:
        PAL "ninguno", 0
        PAL "atari", T_ATARI
        PAL "megadrive", T_MEGA
        PAL "snes", T_SNES
        PAL "nes", T_NES
        PAL "psx", T_PSX
        db 0
t_sino:
        PAL "si", 1
        PAL "no", 0
        db 0
; nombres de tecla -> código (bit 7 = prefijo E0h)
t_teclas:
        PAL "arriba", 0xC8
        PAL "abajo", 0xD0
        PAL "izquierda", 0xCB
        PAL "derecha", 0xCD
        PAL "intro", 0x1C
        PAL "esc", 0x01
        PAL "espacio", 0x39
        PAL "ctrl", 0x1D
        PAL "alt", 0x38
        PAL "mayus", 0x2A
        PAL "tab", 0x0F
        PAL "retroceso", 0x0E
        PAL "insert", 0xD2
        PAL "supr", 0xD3
        PAL "inicio", 0xC7
        PAL "fin", 0xCF
        PAL "repag", 0xC9
        PAL "avpag", 0xD1
        PAL "f10", 0x44
        PAL "f11", 0x57
        PAL "f12", 0x58
        PAL "f1", 0x3B
        PAL "f2", 0x3C
        PAL "f3", 0x3D
        PAL "f4", 0x3E
        PAL "f5", 0x3F
        PAL "f6", 0x40
        PAL "f7", 0x41
        PAL "f8", 0x42
        PAL "f9", 0x43
        PAL "nada", 0
        db 0
sc_letras   db 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32
            db 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C

tipos       dw tp0, tp1, tp2, tp3, tp4, tp5, tp6, tp7, tp8
tp0         db "?$"
tp1         db "USB$"
tp2         db "USB tipo Xbox 360$"
tp3         db "puerto de juegos$"
tp4         db "joystick Atari/Commodore (impresora)$"
tp5         db "Mega Drive (impresora)$"
tp6         db "NES (impresora)$"
tp7         db "Super Nintendo (impresora)$"
tp8         db "PlayStation (impresora)$"
%ifdef PRUEBA
msg_title   db "HSPJOY 0.1 - mandos de juego de Hispa-DOS (PRUEBA: puertos simulados)", 13, 10, "$"
%else
msg_title   db "HSPJOY 0.1 - mandos de juego de Hispa-DOS", 13, 10, "$"
%endif
msg_uso     db "Uso: HSPJOY [/R] [/D] [/I:fichero]", 13, 10
            db "  Lee MANDO.INI (el que está junto a HSPJOY.COM, o el de /I:), busca los", 13, 10
            db "  mandos de juego (USB con HSPUSB, puerto de juegos, cable del puerto de", 13, 10
            db "  impresora) y los entrega a los juegos (INT 15h) o como teclas.", 13, 10
            db "  Si no hay ninguno no se queda en la memoria (con /R, sí).", 13, 10
            db "  Si ya está cargado, vuelve a leer MANDO.INI. /D lo descarga.", 13, 10
            db "  MANDO enseña los mandos, los prueba y calibra el del puerto de juegos.", 13, 10, "$"
msg_otra    db "Hay cargada otra versión de HSPJOY: descárguela con su HSPJOY /D.", 13, 10, "$"
msg_noesta  db "HSPJOY no está cargado.", 13, 10, "$"
msg_actual  db "HSPJOY ya estaba cargado: configuración actualizada.", 13, 10, "$"
msg_fuera   db "HSPJOY descargado.", 13, 10, "$"
msg_nofuera db "No se puede descargar: hay otro programa residente cargado después.", 13, 10, "$"
msg_nolpt   db "MANDO.INI pide un mando en un puerto de impresora que no existe.", 13, 10, "$"
msg_nobidi  db "AVISO: el puerto de impresora no parece bidireccional; el joystick Atari y", 13, 10
            db "el mando de Mega Drive lo necesitan (modo PS/2, EPP o ECP en la BIOS).", 13, 10, "$"
msg_noini   db "No se puede abrir el fichero de configuración.", 13, 10, "$"
msg_inierr  db "MANDO.INI: no se entiende la línea $"
msg_crlf    db 13, 10, "$"
msg_mando   db "Mando $"
msg_dosp    db ": $"
msg_par     db " ($"
msg_ejes    db " ejes, $"
msg_bot     db " botones)", 13, 10, "$"
msg_ninguno db "No se ha encontrado ningún mando de juego.", 13, 10, "$"
msg_queda   db "HSPJOY queda cargado (/R).", 13, 10, "$"
msg_teclas  db "Mando a teclado activado (MANDO.INI).", 13, 10, "$"
ini_nombre  db "MANDO.INI", 0
opt_d       db 0
opt_r       db 0
opt_i       db 0
instalado   db 0
cal_ini     db 0
lpt_num     db 1
ini_sec     db 0
ini_linea   dw 0
res_seg     dw 0
ini_path    times 80 db 0
INI_MAX     equ 6000
ini_buf:
