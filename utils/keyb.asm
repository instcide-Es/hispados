; =============================================================================
;  Hispa-DOS KEYB.COM - distribución de teclado española (residente)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  KEYB SP | KEYB ES   teclado español (ñ, ç, ¡, ¿, º, ª, acentos con teclas
;                      muertas ´ ` ^ ¨, AltGr: @ # | \ [ ] { } ~ ¬)
;  KEYB US             teclado estadounidense (el de la BIOS)
;  KEYB                muestra el teclado activo
;  KEYB ... /MAC       teclas que no tienen los portátiles de Apple:
;                        Ctrl+Retroceso = Supr      Alt+Retroceso = Insertar
;                        Alt+izquierda  = Inicio    Alt+derecha   = Fin
;                        Alt+arriba     = RePág     Alt+abajo     = AvPág
;                      (Alt es la tecla Opción). Se activa sola si el equipo
;                      es de Apple (fabricante de la tabla SMBIOS tipo 1).
;  KEYB ... /NOMAC     las quita
;
;  INT 2Fh AX=AD82h BL=1 (lo pide Hispa Ventanas al arrancar; BL=0 al salir o
;  al abrir un programa de DOS): las mayúsculas con acento que la 437 no tiene
;  (Á Í Ó Ú À È Ì Ò Ù Â Ê Î Ô Û Ë Ï) y el euro (AltGr+E) salen con los códigos
;  de la letra de Ventanas (tools/hispa437.py). Sin eso, como siempre: Á = A.
;
;  Se engancha al aviso de teclado de la BIOS (INT 15h, AH=4Fh: la BIOS lo
;  llama desde INT 09h con cada código de tecla): traduce las teclas que
;  cambian y pone el carácter (página de códigos 437) en el búfer de la BIOS
;  con INT 16h AH=05h; las demás las deja pasar. No lee el puerto del teclado:
;  si lo leyera antes que la BIOS, las teclas de dos códigos (flechas, Inicio,
;  Fin...) podían llegar repetidas.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

D_ACUTE     equ 0xF1
D_GRAVE     equ 0xF2
D_CIRC      equ 0xF3
D_DIAER     equ 0xF4

start:
        jmp init

old09       dd 0
old2f       dd 0
soft        db 0                    ; la tecla llegó por INT 2Fh AX=AD81h
active      db 1                    ; distribución española activa
mac         db 0                    ; (active+1) teclas de sustitución del Mac
dead        db 0
insdn       db 0                    ; Alt+Retroceso sigue pulsado (no repetir)
e0          db 0                    ; último código fue E0h
altgr       db 0                    ; Alt derecho pulsado
remap_n     db 0                    ; remapeos activos de \HISPADOS\TECLAS.INI
vent        db 0                    ; [GLIFOS] Hispa Ventanas: Á, Í, Ó, Ú, €...

; código de tecla, normal, Mayús, AltGr (0 = lo que dé la BIOS)
keytab:
        db 0x29, 0xA7, 0xA6, '\'        ; º ª \
        db 0x02, 0,    0,    '|'        ; 1 ! |
        db 0x03, 0,    '"',  '@'        ; 2 " @
        db 0x04, 0,    0xFA, '#'        ; 3 · #
        db 0x05, 0,    0,    '~'        ; 4 $ ~
        db 0x07, 0,    '&',  0xAA       ; 6 & ¬
        db 0x08, 0,    '/',  0          ; 7 /
        db 0x09, 0,    '(',  0          ; 8 (
        db 0x0A, 0,    ')',  0          ; 9 )
        db 0x0B, 0,    '=',  0          ; 0 =
        db 0x0C, 0x27, '?',  0          ; ' ?
        db 0x0D, 0xAD, 0xA8, 0          ; ¡ ¿
        db 0x1A, D_GRAVE, D_CIRC, '['   ; ` ^ [
        db 0x1B, '+',  '*',  ']'        ; + * ]
        db 0x27, 0xA4, 0xA5, 0          ; ñ Ñ
        db 0x28, D_ACUTE, D_DIAER, '{'  ; ´ ¨ {
        db 0x2B, 0x87, 0x80, '}'        ; ç Ç }
        db 0x56, '<',  '>',  0          ; < >
        db 0x33, ',',  ';',  0
        db 0x34, '.',  ':',  0
        db 0x35, '-',  '_',  0
        db 0

; vocales: código de tecla y, por acento, minúscula y mayúscula
;              a     e     i     o     u
vowel_scan  db 0x1E, 0x12, 0x17, 0x18, 0x16
acute_lo    db 0xA0, 0x82, 0xA1, 0xA2, 0xA3
acute_up    db 'A',  0x90, 'I',  'O',  'U'
grave_lo    db 0x85, 0x8A, 0x8D, 0x95, 0x97
grave_up    db 'A',  'E',  'I',  'O',  'U'
circ_lo     db 0x83, 0x88, 0x8C, 0x93, 0x96
circ_up     db 'A',  'E',  'I',  'O',  'U'
diaer_lo    db 0x84, 0x89, 0x8B, 0x94, 0x81
diaer_up    db 0x8E, 'E',  'I',  0x99, 0x9A
accent_ch   db 0x27, '`', '^', '"'
; [GLIFOS] las mayúsculas con acento en Hispa Ventanas, que tiene esas letras
; en códigos de la 437 que no pinta (tools/hispa437.py); solo mientras
; Ventanas lo pide (INT 2Fh AX=AD82h BL=1): en modo texto serían recuadros
acute_upv   db 0xB6, 0x90, 0xCF, 0xD3, 0xD7     ; Á É Í Ó Ú
grave_upv   db 0xB5, 0xBD, 0xC7, 0xD2, 0xD6     ; À È Ì Ò Ù
circ_upv    db 0xB7, 0xBE, 0xD0, 0xD4, 0xD8     ; Â Ê Î Ô Û
diaer_upv   db 0x8E, 0xC6, 0xD1, 0x99, 0x9A     ; Ä Ë Ï Ö Ü
VENT_EURO   equ 0xF4                            ; € (AltGr+E)

int2f:
        cmp ax, 0xAD80
        jne .chain
        mov al, 0xFF
        mov bx, 'HS'
        push cs
        pop es
        mov di, active
        iret
.chain:
        cmp ax, 0xAD81
        je .soft
        cmp ax, 0xAD82              ; [GLIFOS] BL=1: letras de Hispa Ventanas
        jne .ch2                    ; (Á, €...); BL=0: las de modo texto
        mov [cs:vent], bl
        mov al, 0xFF
        iret
.ch2:
        jmp far [cs:old2f]
; código de tecla que no viene del 8042 (HSPUSB en equipos sin controlador
; de teclado): BL = código -> AX = 4B01h atendida, 4B00h sin cambios
.soft:
        push ax
        push bx
        push cx
        push si
        push ds
        mov byte [cs:soft], 1
        mov al, bl
        jmp int09.have

int09:
        cmp ah, 0x4F                ; ¿aviso de tecla? AL=código
        je .tecla
        jmp far [cs:old09]
.tecla:
        push ax
        push bx
        push cx
        push si
        push ds
.have:
        call remap_apply            ; TECLAS.INI: traducir el código entrante
        mov ah, al
        ; seguir AltGr (E0 38 / E0 B8) por nuestra cuenta
        cmp al, 0xE0
        jne .ne0
        mov byte [cs:e0], 1
        jmp .chain
.ne0:
        cmp byte [cs:e0], 0
        je .noe0
        mov byte [cs:e0], 0
        mov bl, al
        and bl, 0x7F
        cmp bl, 0x38
        jne .macnav
        xor bl, bl
        test al, 0x80
        jnz .ag
        mov bl, 0x08
.ag:
        mov [cs:altgr], bl
        jmp .chain
; Mac: Alt + flecha gris (E0 4B/4D/48/50) = Inicio / Fin / RePág / AvPág
.macnav:
        cmp byte [cs:mac], 0
        je .chain
        test al, 0x80
        jnz .chain
        mov bx, 0x40
        mov ds, bx
        mov bl, [0x17]
        and bl, 0x0C
        cmp bl, 0x08                ; Alt (cualquiera de los dos) sin Ctrl
        jne .chain
        mov ah, 0x47                ; izquierda -> Inicio
        cmp al, 0x4B
        je .macput
        mov ah, 0x4F                ; derecha -> Fin
        cmp al, 0x4D
        je .macput
        mov ah, 0x49                ; arriba -> RePág
        cmp al, 0x48
        je .macput
        mov ah, 0x51                ; abajo -> AvPág
        cmp al, 0x50
        je .macput
        jmp .chain
; la tecla gris de código AH al búfer (AX = xxE0h: INT 16h AH=10h la da así
; y AH=00h como xx00h, igual que la tecla de verdad); la original no pasa
.macput:
        and byte [0x96], 0xFD       ; la BIOS ya vio el E0h: que lo olvide
        mov al, 0xE0
        call flush_dead
        call emit
        jmp .eat
.noe0:
        cmp byte [cs:mac], 0
        je .nomac
        mov bl, al
        and bl, 0x7F
        cmp bl, 0x0E                ; Retroceso
        jne .nomac
        mov bx, 0x40
        mov ds, bx
        test al, 0x80
        jz .bsdn
        mov byte [cs:insdn], 0
        jmp .chain
.bsdn:
        mov bl, [0x17]
        and bl, 0x0C
        mov ah, 0x53                ; Ctrl+Retroceso = Supr
        cmp bl, 0x04
        je .macput
        cmp bl, 0x08                ; Alt+Retroceso = Insertar
        jne .bsno
        mov ah, 0x52
        cmp byte [cs:insdn], 0      ; el indicador cambia una vez por
        jne .macput                 ; pulsación, no con la repetición
        mov byte [cs:insdn], 1
        xor byte [0x17], 0x80
        jmp .macput
.bsno:
        mov ah, al
.nomac:
        cmp byte [cs:active], 0     ; distribución de EE. UU.: nada más
        je .chain
        test al, 0x80               ; soltar tecla
        jnz .chain
        mov bx, 0x40
        mov ds, bx
        mov bl, [0x17]              ; Mayús, Ctrl, Alt, Bloq Mayús
        mov bh, [cs:altgr]
        test bl, 0x04               ; Ctrl sin AltGr: lo que diga la BIOS
        jz .noctrl
        test bh, bh
        jz .chain
.noctrl:
        test bl, 0x08               ; Alt izquierdo
        jz .noalt
        test bh, bh
        jz .chain
.noalt:
        ; [GLIFOS] AltGr+E = € (solo en Hispa Ventanas: la 437 no lo tiene)
        cmp ah, 0x12
        jne .noeuro
        test bh, bh
        jz .noeuro
        cmp byte [cs:vent], 0
        je .noeuro
        mov al, VENT_EURO
        jmp .plain
.noeuro:
        ; buscar la tecla en la tabla
        mov si, keytab
.find:
        mov al, [cs:si]
        test al, al
        jz .notfound
        cmp al, ah
        je .found
        add si, 4
        jmp .find
.found:
        mov al, [cs:si+3]
        test bh, bh
        jnz .got
        mov al, [cs:si+1]
        mov cl, bl
        and cl, 3
        ; ñ y ç: Bloq Mayús también cuenta
        cmp ah, 0x27
        je .caps
        cmp ah, 0x2B
        jne .shift
.caps:
        test bl, 0x40
        jz .shift
        xor cl, 1
.shift:
        test cl, cl
        jz .got
        mov al, [cs:si+2]
.got:
        test al, al
        jz .notfound
        cmp al, D_ACUTE
        jb .plain
        ; tecla muerta: la siguiente tecla lleva el acento
        call flush_dead
        sub al, 0xF0
        mov [cs:dead], al
        jmp .eat
.plain:
        call flush_dead
        call emit
        jmp .eat
.notfound:
        cmp byte [cs:dead], 0
        je .chain
        ; Mayús, Ctrl, Alt y Bloq Mayús no deshacen la tecla muerta
        ; (´ y luego Mayús+E = É)
        cmp ah, 0x2A
        je .chain
        cmp ah, 0x36
        je .chain
        cmp ah, 0x1D
        je .chain
        cmp ah, 0x38
        je .chain
        cmp ah, 0x3A
        je .chain
        ; ¿vocal o espacio tras una tecla muerta?
        cmp ah, 0x39
        jne .vow
        call flush_dead
        jmp .eat
.vow:
        test bh, bh
        jnz .other
        mov si, vowel_scan
        xor cx, cx
.vl:
        cmp [cs:si], ah
        je .vowel
        inc si
        inc cx
        cmp cx, 5
        jb .vl
.other:
        call flush_dead
        jmp .chain
.vowel:
        ; mayúscula si Mayús xor Bloq Mayús
        mov al, bl
        and al, 3
        jz .nsh
        mov al, 1
.nsh:
        test bl, 0x40
        jz .ncaps
        xor al, 1
.ncaps:
        movzx ax, al
        movzx si, byte [cs:dead]
        dec si
        shl si, 1
        add si, ax                  ; (acento-1)*2 + mayúscula
        shl si, 1                   ; entradas de 2 bytes
        cmp byte [cs:vent], 0       ; [GLIFOS] en Ventanas, las mayúsculas
        je .vt                      ; con su acento
        add si, vtabv - vtab
.vt:
        mov si, [cs:vtab+si]        ; tabla de esa forma
        mov bx, cx
        mov al, [cs:si+bx]
        mov byte [cs:dead], 0
        call emit
        jmp .eat
.eat:
        cmp byte [cs:soft], 0
        jne .seat
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        push bp                     ; acarreo a 0: la BIOS no hace nada más
        mov bp, sp
        and byte [bp+6], 0xFE
        pop bp
        iret
.chain:
        cmp byte [cs:soft], 0
        jne .schain
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        jmp far [cs:old09]
.seat:
        mov byte [cs:soft], 0
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        mov ax, 0x4B01              ; atendida
        iret
.schain:
        mov byte [cs:soft], 0
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        mov ax, 0x4B00              ; sin cambios: la pone quien la envía
        iret

; AL = carácter, AH = código de tecla -> búfer de la BIOS
emit:
        push ax
        push cx
        mov cl, al
        mov ch, ah
        mov ah, 0x05
        int 0x16
        pop cx
        pop ax
        ret

; si hay una tecla muerta pendiente, escribir su acento
flush_dead:
        cmp byte [cs:dead], 0
        je .r
        push ax
        push bx
        movzx bx, byte [cs:dead]
        mov al, [cs:accent_ch+bx-1]
        mov ah, 0x28
        mov byte [cs:dead], 0
        call emit
        pop bx
        pop ax
.r:
        ret

vtab        dw acute_lo, acute_up, grave_lo, grave_up
            dw circ_lo, circ_up, diaer_lo, diaer_up
vtabv       dw acute_lo, acute_upv, grave_lo, grave_upv     ; [GLIFOS]
            dw circ_lo, circ_upv, diaer_lo, diaer_upv

; =============================================================================
;  Remapeo de teclas (lo configura \HISPADOS\TECLAS.INI; lo lee load_teclas al
;  cargar KEYB). Traduce el código de rastreo ENTRANTE antes del procesado
;  propio de KEYB, así la BIOS procesa el código de DESTINO y, si el destino es
;  una tecla modificadora (1D Ctrl, 2A/36 Mayús, 38 Alt), actualiza sus
;  banderas: una tecla rara se puede convertir en un Ctrl de verdad.
;
;  Entrada: AL = código de rastreo (make, o break = make|80h). [cs:e0] = 1 si
;  el código anterior fue el prefijo E0h (tecla extendida), 0 si no.
;  Cambia AL y la copia de AX guardada en la pila del gancho (la que .chain
;  devolverá a la BIOS). No toca nada si no hay remapeos o la tecla no está.
;
;  Cada entrada de remap_tab ocupa 3 octetos: origen (make 00-7Fh),
;  extendida (0 = normal, 1 = necesita el prefijo E0h) y destino (make 00-7Fh).
; =============================================================================
MAXREMAP    equ 32
remap_apply:
        cmp byte [cs:remap_n], 0
        je .r                       ; sin remapeos: no se hace nada
        cmp al, 0xE0                ; el propio prefijo E0h no se traduce
        je .r
        push bp
        push bx
        push cx
        push si
        mov bp, sp                  ; [bp+18] = octeto bajo del AX del gancho
        mov bl, al
        and bl, 0x7F                ; make del código entrante
        mov bh, al
        and bh, 0x80                ; 80h = soltar (break)
        mov cl, [cs:e0]             ; 0 = normal, 1 = extendida (E0 xx)
        mov si, remap_tab
        mov ch, [cs:remap_n]
.find:
        cmp [cs:si], bl             ; ¿coincide el origen?
        jne .next
        mov al, [cs:si+1]           ; ¿y la condición de extendida?
        cmp al, cl
        jne .next
        mov al, [cs:si+2]           ; destino (make)
        or al, bh                   ; conservar el bit de make/break
        mov [bp+18], al             ; que la BIOS reciba el destino al encadenar
        jmp .done
.next:
        add si, 3
        dec ch
        jnz .find
        mov al, [bp+18]             ; sin coincidencia: AL se queda igual
.done:
        pop si
        pop cx
        pop bx
        pop bp
.r:
        ret

remap_tab   times MAXREMAP*3 db 0   ; origen, extendida, destino (x MAXREMAP)

resident_end:

; =============================================================================
init:
        ; argumentos: [SP|ES|US] [/MAC|/NOMAC]
        mov si, 0x81
.tok:
        lodsb
        cmp al, ' '
        je .tok
        cmp al, 9
        je .tok
        cmp al, 13
        je .parsed
        cmp al, '/'
        je .sw
        and al, 0xDF
        mov ah, [si]
        and ah, 0xDF
        inc si
        mov bl, 1
        cmp ax, 'SP'
        je .lay
        cmp ax, 'ES'
        je .lay
        mov bl, 2
        cmp ax, 'US'
        jne .usage
.lay:
        mov [lay], bl
        jmp .delim
.sw:
        mov bl, 1
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'NO'
        jne .sw1
        add si, 2
        mov bl, 2
.sw1:
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'MA'
        jne .usage
        mov al, [si+2]
        and al, 0xDF
        cmp al, 'C'
        jne .usage
        add si, 3
        mov [macopt], bl
.delim:
        mov al, [si]                ; detrás, espacio, otra opción o el final
        cmp al, ' '
        je .tok
        cmp al, 9
        je .tok
        cmp al, 13
        je .tok
        cmp al, '/'
        je .tok
.usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.parsed:
        mov ax, 0xAD80              ; ¿ya residente? -> ES:DI = active, mac
        int 0x2F
        cmp al, 0xFF
        jne .nores
        ; ---- residente: cambiar lo que se pida y decir cómo queda ----
        mov al, [lay]
        test al, al
        jz .r1
        and al, 1                   ; SP = 1, US = 0
        mov [es:di], al
.r1:
        mov al, [macopt]
        test al, al
        jz .r2
        and al, 1                   ; /MAC = 1, /NOMAC = 0
        mov [es:di+1], al
.r2:
        mov si, msg_cur_us
        cmp byte [es:di], 0
        je .r3
        mov si, msg_cur_es
.r3:
        call puts
        mov si, msg_mac
        cmp byte [es:di+1], 0
        jne .r4
        cmp byte [macopt], 2
        jne .exit
        mov si, msg_nomac
.r4:
        call puts
.exit:
        mov ax, 0x4C00
        int 0x21
        ; ---- sin instalar ----
.nores:
        ; TECLAS.INI y la tabla de SMBIOS se leen en una zona de trabajo de
        ; detrás del programa (smbuf y teclas_buf: 3 KB que no están en el
        ; fichero). Con LH en un hueco pequeño de la memoria superior (el HP
        ; Mini: 4 KB al final de B000-B7FF, con HSPDPMI y HSPRATON delante)
        ; esa zona pasa del final del bloque: se pisaba la pila (que DOS pone
        ; al final del bloque) y lo de detrás del bloque, y el arranque de
        ; «Juegos» se colgaba aquí según lo que hubiera en SMBIOS. Si no
        ; cabe, la preparación se hace en una copia del programa en un bloque
        ; de paso de la memoria convencional y se trae el resultado.
        mov ax, [2]                 ; fin del bloque (PSP)
        mov dx, cs
        sub ax, dx                  ; párrafos del bloque
        cmp ax, TRABAJO_PARR
        jae .cabe
        mov ax, 0x5800              ; (LH deja «solo arriba»: se pide abajo)
        int 0x21
        push ax
        mov ax, 0x5801
        xor bx, bx
        int 0x21
        mov ah, 0x48
        mov bx, TRABAJO_PARR
        int 0x21
        pop bx
        pushf
        push ax
        xor bh, bh
        mov ax, 0x5801              ; la estrategia, como estaba
        int 0x21
        pop ax
        popf
        jc .sinmem
        mov [seg_orig], cs
        mov es, ax
        xor si, si
        xor di, di
        mov cx, smbuf               ; PSP y programa
        cld
        rep movsb
        push es
        push word .encopia
        retf
.encopia:                           ; (CS = la copia; la pila, la de antes)
        push cs
        pop ds
        call prepara
        mov es, [seg_orig]          ; el resultado (variables), a su sitio
        mov si, 0x100
        mov di, si
        mov cx, smbuf - 0x100
        cld
        rep movsb
        push es
        push word .vuelta
        retf
.vuelta:
        push ds
        pop es                      ; el bloque de paso ya no hace falta
        push cs
        pop ds
        mov ah, 0x49
        int 0x21
        push cs
        pop es
        jmp .nomac
.sinmem:
        mov si, msg_sinmem
        call puts
        mov ax, 0x4C01
        int 0x21
.cabe:
        call prepara
.nomac:
        mov al, [lay]
        and al, 1
        mov [active], al
        or al, [mac]
        or al, [remap_n]            ; con remapeos de TECLAS.INI también se instala
        jnz .install
        mov si, msg_cur_us          ; nada que dejar residente
        call puts
        cmp byte [macopt], 2
        jne .exit
        mov si, msg_nomac
        call puts
        jmp .exit
.install:
        push cs
        pop es
        mov ax, 0x3515
        int 0x21
        mov [old09], bx
        mov [old09+2], es
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int09
        mov ax, 0x2515
        int 0x21
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov si, msg_inst
        cmp byte [active], 0
        jne .i1
        mov si, msg_cur_us
.i1:
        call puts
        cmp byte [mac], 0
        je .i2
        mov si, msg_mac
        call puts
.i2:
        cmp byte [remap_n], 0
        je .i3
        mov si, msg_remap
        call puts
.i3:
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, resident_end + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21

; Lo que se lee antes de instalarse: los remapeos de TECLAS.INI y si el
; equipo es de Apple. Usa la zona de trabajo de detrás del programa (smbuf y
; teclas_buf); todo con CS = DS (también vale en una copia del programa).
prepara:
        call load_teclas            ; leer \HISPADOS\TECLAS.INI (remapeos)
        mov al, [macopt]
        cmp al, 2
        je .r
        cmp al, 1
        je .simac
        cmp byte [lay], 0           ; «KEYB» a secas solo informa
        je .r
        call detect_mac
        mov [mac], al
        ret
.simac:
        mov byte [mac], 1
.r:
        ret

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

; ---- ¿es un equipo de Apple? ----
; Busca la cabecera de SMBIOS («_SM_», «_SM3_» o la antigua «_DMI_») en
; F0000h-FFFFFh, recorre la tabla hasta la estructura de tipo 1 (información
; del sistema) y mira si el fabricante contiene «Apple». La tabla puede estar
; por encima del primer megabyte: se lee con INT 15h AH=87h.
; -> AL = 1 si lo es
SM_BUF      equ 1024
detect_mac:
        push es
        mov ax, 0xF000
        mov es, ax
        xor di, di
.scan:
        mov eax, [es:di]
        cmp eax, '_SM_'
        je .sm2
        cmp byte [es:di+4], '_'
        jne .next
        cmp eax, '_SM3'
        je .sm3
        cmp eax, '_DMI'
        je .dmi
.next:
        add di, 16
        jnz .scan
        jmp .no
.sm2:
        mov eax, [es:di+0x18]       ; dirección de la tabla
        movzx ecx, word [es:di+0x16] ; y lo que ocupa
        jmp .walk
.sm3:
        cmp dword [es:di+0x14], 0   ; por encima de 4 GB: no se alcanza
        jne .next
        mov eax, [es:di+0x10]
        mov ecx, [es:di+0x0C]
        jmp .walk
.dmi:
        mov eax, [es:di+8]
        movzx ecx, word [es:di+6]
.walk:
        mov [sm_addr], eax
        add ecx, eax
        mov [sm_end], ecx
        mov word [sm_n], 500        ; tope de estructuras
.st:
        mov eax, [sm_addr]
        cmp eax, [sm_end]
        jae .no
        dec word [sm_n]
        jz .no
        call sm_read
        jc .no
        mov word [smbuf+SM_BUF], 0  ; tope: las búsquedas acaban aquí
        mov si, smbuf
        mov al, [si]                ; tipo
        cmp al, 127                 ; fin de la tabla
        je .no
        movzx bx, byte [si+1]       ; longitud de la parte fija
        cmp bx, 4
        jb .no
        cmp bx, SM_BUF-2
        jae .no
        cmp al, 1
        je .type1
        ; saltar la estructura: tras la parte fija, cadenas hasta dos ceros
        add si, bx
.e1:
        cmp si, smbuf+SM_BUF-1
        jae .no
        cmp word [si], 0
        je .e2
        inc si
        jmp .e1
.e2:
        add si, 2
        sub si, smbuf
        movzx eax, si
        add [sm_addr], eax
        jmp .st
.type1:
        cmp bx, 5
        jb .no
        movzx cx, byte [si+4]       ; n.º de la cadena del fabricante
        jcxz .no
        add si, bx
.s1:
        dec cx
        jz .s3
.s2:
        lodsb                       ; saltar una cadena
        test al, al
        jnz .s2
        cmp byte [si], 0            ; no hay tantas
        je .no
        jmp .s1
.s3:
        cmp byte [si], 0
        je .no
        cmp dword [si], 'Appl'
        jne .s4
        cmp byte [si+4], 'e'
        je .yes
.s4:
        inc si
        jmp .s3
.yes:
        mov al, 1
        pop es
        ret
.no:
        xor al, al
        pop es
        ret

; copiar SM_BUF octetos de la memoria física [sm_addr] a smbuf -> CF si falla
sm_read:
        push es
        push si
        push di
        push cx
        mov eax, [sm_addr]
        cmp eax, 0x100000 - SM_BUF
        ja .high
        ; por debajo del megabyte: se copia sin más
        push ds
        mov si, ax
        and si, 0x000F
        shr eax, 4
        push ds
        pop es
        mov ds, ax
        mov di, smbuf
        mov cx, SM_BUF
        cld
        rep movsb
        pop ds
        clc
        jmp .r
        ; por encima (los equipos con EFI): movimiento de bloques de la BIOS
.high:
        call a20_test
        mov [sm_a20], al
        mov eax, [sm_addr]
        mov [sm_gdt+0x12], ax
        shr eax, 16
        mov [sm_gdt+0x14], al
        mov [sm_gdt+0x17], ah
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, smbuf
        mov [sm_gdt+0x1A], ax
        shr eax, 16
        mov [sm_gdt+0x1C], al
        mov [sm_gdt+0x1F], ah
        push cs
        pop es
        mov si, sm_gdt
        mov cx, SM_BUF/2
        mov ah, 0x87
        int 0x15
        pushf
        ; hay BIOS (las del AT) que dejan la línea A20 cerrada al volver: si
        ; estaba abierta (HSPXMS, Windows) se vuelve a abrir
        cmp byte [sm_a20], 0
        je .a2
        call a20_test
        test al, al
        jnz .a2
        call a20_reopen
.a2:
        popf
.r:
        pop cx
        pop di
        pop si
        pop es
        ret

; -> AL = 1 si la línea A20 está abierta (0000:0500 y FFFF:0510 son sitios
; distintos), 0 si está cerrada
a20_test:
        push ds
        push es
        push si
        push di
        pushf
        cli
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov si, 0x0500
        mov di, 0x0510
        mov al, [si]
        mov ah, [es:di]
        push ax
        mov byte [si], 0x55
        mov byte [es:di], 0xAA
        cmp byte [si], 0x55
        pop ax
        mov [es:di], ah
        mov [si], al
        mov al, 0
        jne .c
        mov al, 1
.c:
        popf
        pop di
        pop si
        pop es
        pop ds
        ret

; abrir la línea A20: por el driver XMS si lo hay; si no (o si no basta),
; por el puerto 92h
a20_reopen:
        push bx
        push es
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .port
        mov ax, 0x4310
        int 0x2F
        mov [sm_xms], bx
        mov [sm_xms+2], es
        mov ah, 3
        call far [sm_xms]
        call a20_test
        test al, al
        jnz .r
.port:
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
.r:
        pop es
        pop bx
        ret

; =============================================================================
;  Leer \HISPADOS\TECLAS.INI (y, si no, TECLAS.INI de la carpeta actual) y
;  llenar la tabla de remapeos antes de quedarse residente. Si no existe, no
;  hay remapeos y KEYB funciona igual que siempre.
;
;  Formato de TECLAS.INI (el mismo que escribirá la ventana «Teclado»):
;    - Una línea por remapeo: «origen=destino».
;    - Origen y destino son códigos de rastreo «make» en HEXADECIMAL.
;      Origen: 2 cifras (p. ej. 5B) o, para teclas extendidas, 4 cifras con el
;      prefijo E0 (p. ej. E05B). Destino: 2 cifras (p. ej. 1D = Ctrl).
;    - Ejemplo: 5B=1D  -> la tecla cuyo make es 5B actúa como Ctrl izquierdo.
;    - Líneas en blanco y las que empiezan por ; o # se ignoran.
;    - Hasta MAXREMAP (32) remapeos; los de más se descartan.
;  TECLA.COM ayuda a averiguar el código de rastreo de una tecla.
; =============================================================================
TECLAS_BUF  equ 2048
load_teclas:
        push cs
        pop es
        mov dx, teclas_path         ; primero \HISPADOS\TECLAS.INI
        mov ax, 0x3D00
        int 0x21
        jnc .ok
        mov dx, teclas_name         ; si no, TECLAS.INI (carpeta actual)
        mov ax, 0x3D00
        int 0x21
        jc .r                       ; no hay fichero: sin remapeos
.ok:
        mov bx, ax
        mov ah, 0x3F
        mov cx, TECLAS_BUF
        mov dx, teclas_buf
        int 0x21
        jnc .rd
        xor ax, ax
.rd:
        mov si, ax
        mov byte [teclas_buf+si], 0 ; cerrar el texto
        mov ah, 0x3E
        int 0x21
        mov si, teclas_buf
.line:
        mov al, [si]                ; saltar espacios y tabuladores iniciales
        cmp al, ' '
        je .sp
        cmp al, 9
        jne .first
.sp:
        inc si
        jmp .line
.first:
        test al, al
        jz .r
        cmp al, 26                  ; fin de fichero
        je .r
        cmp al, ';'
        je .skip
        cmp al, '#'
        je .skip
        cmp al, 13
        je .skip
        cmp al, 10
        je .skip
        call get_hex                ; origen (2 o 4 cifras)
        jc .skip
        xor bh, bh                  ; bh = extendida (0/1)
        cmp cx, 4
        jne .noext
        cmp dh, 0xE0                ; 4 cifras: el byte alto ha de ser E0
        jne .skip
        mov bh, 1
.noext:
        mov bl, dl                  ; make del origen
        and bl, 0x7F
.eq:
        mov al, [si]                ; el separador «=» (admite espacios antes)
        cmp al, '='
        je .eqok
        cmp al, ' '
        je .eqs
        cmp al, 9
        je .eqs
        jmp .skip
.eqs:
        inc si
        jmp .eq
.eqok:
        inc si
.ws:
        mov al, [si]                ; espacios tras el «=»
        cmp al, ' '
        je .wss
        cmp al, 9
        jne .dest
.wss:
        inc si
        jmp .ws
.dest:
        call get_hex                ; destino (2 cifras)
        jc .skip
        mov al, dl
        and al, 0x7F                ; destino make
        mov cl, [remap_n]
        cmp cl, MAXREMAP            ; ¿cabe otro?
        jae .skip
        movzx di, cl
        mov cx, di
        add di, di
        add di, cx                  ; di = n * 3
        add di, remap_tab
        mov [di], bl                ; origen
        mov [di+1], bh              ; extendida
        mov [di+2], al              ; destino
        inc byte [remap_n]
.skip:
        mov al, [si]                ; consumir hasta el fin de la línea
        test al, al
        jz .r
        cmp al, 26
        je .r
        inc si
        cmp al, 10
        jne .skip
        jmp .line
.r:
        ret

; Leer cifras hexadecimales desde SI -> DX = valor (hasta 4 cifras), CX = nº de
; cifras, SI avanza tras ellas. CF = 1 si no había ninguna cifra.
get_hex:
        xor dx, dx
        xor cx, cx
.g:
        mov al, [si]
        cmp al, 'a'
        jb .noup
        cmp al, 'f'
        ja .noup
        sub al, 32                  ; a mayúscula
.noup:
        cmp al, '0'
        jb .end
        cmp al, '9'
        jbe .dig
        cmp al, 'A'
        jb .end
        cmp al, 'F'
        ja .end
        sub al, 'A' - 10
        jmp .put
.dig:
        sub al, '0'
.put:
        shl dx, 4
        and al, 0x0F
        or dl, al
        inc si
        inc cx
        cmp cx, 4                   ; como mucho 4 cifras (E0 xx)
        jb .g
.end:
        test cx, cx
        jz .nc
        clc
        ret
.nc:
        stc
        ret

sm_a20      db 0
sm_xms      dd 0
lay         db 0                    ; 1 = SP/ES, 2 = US
macopt      db 0                    ; 1 = /MAC, 2 = /NOMAC
sm_addr     dd 0
sm_end      dd 0
sm_n        dw 0
sm_gdt      times 16 db 0
            dw 0xFFFF, 0            ; origen
            db 0, 0x93, 0, 0
            dw 0xFFFF, 0            ; destino
            db 0, 0x93, 0, 0
            times 16 db 0

msg_usage   db "Uso: TECLADO [SP|ES|US] [/MAC|/NOMAC]", 13, 10
            db "  SP o ES  teclado espa", 0xA4, "ol        US  teclado estadounidense", 13, 10
            db "  /MAC     teclas que faltan en los port", 0xA0, "tiles de Apple (Alt = Opci", 0xA2, "n):", 13, 10
            db "           Ctrl+Retroceso = Supr, Alt+Retroceso = Insertar,", 13, 10
            db "           Alt+izquierda/derecha = Inicio/Fin, Alt+arriba/abajo = RePág/AvPág", 13, 10
            db "           (se activan solas si el equipo es de Apple)", 13, 10
            db "  /NOMAC   las quita", 13, 10, 0
msg_inst    db "Teclado espa", 0xA4, "ol instalado.", 13, 10, 0
msg_cur_es  db "Teclado activo: espa", 0xA4, "ol (SP).", 13, 10, 0
msg_cur_us  db "Teclado activo: estadounidense (US).", 13, 10, 0
msg_mac     db "Teclado de Mac: Ctrl+Retroceso = Supr, Alt+flechas = Inicio/Fin/RePág/AvPág,", 13, 10
            db "Alt+Retroceso = Insertar", 13, 10, 0
msg_nomac   db "Teclado de Mac: desactivado.", 13, 10, 0
msg_remap   db "Remapeo de teclas de TECLAS.INI activo.", 13, 10, 0
msg_sinmem  db "TECLADO: no hay memoria suficiente.", 13, 10, 0
seg_orig    dw 0                    ; segmento del programa (desde su copia)

teclas_path db "\HISPADOS\TECLAS.INI", 0
teclas_name db "TECLAS.INI", 0

smbuf:                              ; SM_BUF + 2 octetos, detrás del programa
teclas_buf  equ smbuf + SM_BUF + 2  ; TECLAS_BUF octetos para TECLAS.INI (solo
                                    ; al arrancar; detrás de smbuf, no residente)
; párrafos que hacen falta para la zona de trabajo y 512 octetos de pila
TRABAJO_PARR equ (teclas_buf - $$ + 0x100 + TECLAS_BUF + 1 + 512 + 15) / 16
