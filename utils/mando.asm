; =============================================================================
;  MANDO.COM - los mandos de juego de Hispa-DOS (HSPJOY): verlos, probarlos
;  y calibrar el del puerto de juegos
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  MANDO             lista de los mandos que hay y quién es cada jugador
;  MANDO /VER        ejes, cruceta y botones de cada mando, en vivo
;  MANDO /PROBAR     lo que ve un juego (INT 15h AH=84h), en vivo
;  MANDO /CALIBRAR   calibra el mando del puerto de juegos y lo guarda en
;                    MANDO.INI (línea calibrar=)
;  Todo sale de la interfaz de HSPJOY (INT 2Fh AH=D9h, docs/MANDOS.md).
; =============================================================================
        cpu 386
        bits 16
        org 0x100

%define HJ_SOLO_DEFS
%include "hidjoyx.inc"
M_SZ        equ 32
JOY_MAX     equ 6

        cld
        ; ¿está HSPJOY?
        mov ax, 0xD900
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .no
        cmp bx, 0x484A
        jne .no
        cmp word [es:di+HJD_VER], (JOY_MAX << 8) | 1
        jne .no
        mov [res_off], di
        mov [res_seg], es
        mov ax, [es:di+HJD_TABLA]
        mov [tabla], ax
        push cs
        pop es
        jmp opciones
.no:
        push cs
        pop es
        ; (la ayuda se da aunque no esté)
        mov si, 0x81
        call opcion
        cmp al, '?'
        je ayuda
        mov si, m_noesta
        call puts
        mov ax, 0x4C01
        int 0x21

opciones:
        mov si, 0x81
        call opcion
        cmp al, 0
        je lista
        cmp al, 'V'
        je ver
        cmp al, 'P'
        je probar
        cmp al, 'C'
        je calibrar
ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

; primera opción de la línea -> AL = su letra en mayúscula (0 si no hay)
opcion:
        lodsb
        cmp al, 13
        je .no
        cmp al, '/'
        jne opcion
        lodsb
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret
.no:
        xor al, al
        ret

; =============================================================================
;  MANDO: la lista
; =============================================================================
lista:
        mov si, m_titulo
        call puts
        xor dl, dl
        xor bp, bp
.l:
        call mando                  ; ES:DI = entrada del mando DL (sondeado)
        jc .n
        inc bp
        mov al, ' '
        call putc
        call putc
        mov al, dl
        add al, '1'
        call putc
        mov al, ' '
        call putc
        call putc
        call nombre
        mov si, m_coma
        call puts
        movzx eax, byte [es:di+HJS_NAX]
        call putnum
        mov si, m_ejes
        call puts
        movzx eax, byte [es:di+HJS_NBTN]
        call putnum
        mov si, m_botones
        call puts
        ; ¿es un jugador?
        push es
        push di
        les di, [res_off]
        mov si, m_j1
        cmp dl, [es:di+HJD_J1E]
        je .j
        mov si, m_j2
        cmp dl, [es:di+HJD_J2E]
        jne .nj
.j:
        call puts
.nj:
        pop di
        pop es
        call crlf
.n:
        inc dl
        cmp dl, JOY_MAX
        jb .l
        test bp, bp
        jnz .hay
        mov si, m_ninguno
        call puts
.hay:
        ; mando a teclado
        les di, [res_off]
        mov al, [es:di+HJD_OPC]
        push cs
        pop es
        test al, 6
        jz .nt
        mov si, m_tec
        call puts
        test al, 2
        jz .t2
        push ax
        mov si, m_tecj1
        call puts
        pop ax
.t2:
        test al, 4
        jz .t3
        mov si, m_tecj2
        call puts
.t3:
        call crlf
.nt:
        mov si, m_pie
        call puts
        mov ax, 0x4C00
        int 0x21

; mando DL -> ES:DI = su entrada (recién sondeada); CF si no está presente
mando:
        push ax
        mov ax, 0xD902
        int 0x2F
        jc .no
        test byte [es:di+HJS_FLAGS], 1
        jz .no
        pop ax
        clc
        ret
.no:
        pop ax
        stc
        ret

; escribir qué es el mando ES:DI
nombre:
        push bx
        movzx bx, byte [es:di+HJS_TIPO]
        cmp bx, 8
        jbe .t
        xor bx, bx
.t:
        shl bx, 1
        mov si, [tipos+bx]
        call puts
        cmp byte [es:di+HJS_TIPO], 2
        ja .r
        mov al, ' '
        call putc
        mov ax, [es:di+HJS_VID]
        call puthex4
        mov al, ':'
        call putc
        mov ax, [es:di+HJS_PID]
        call puthex4
.r:
        pop bx
        ret

; =============================================================================
;  MANDO /VER: los mandos en vivo
; =============================================================================
ver:
        mov si, m_ver
        call puts
        mov byte [primera], 1
.l:
        ; ¿ha cambiado algo? (y cuántos hay)
        xor dl, dl
        xor bp, bp                  ; BP = presentes
        xor bx, bx                  ; BL = 1 si algo cambia
.c:
        call mando
        jc .cn
        inc bp
        push dx
        mov al, HJS_SZ
        mul dl
        mov si, ax
        add si, copia
        mov cx, HJS_SZ
        push di
.cc:
        mov al, [es:di]
        cmp al, [si]
        je .ce
        mov [si], al
        mov bl, 1
.ce:
        inc di
        inc si
        loop .cc
        pop di
        pop dx
.cn:
        inc dl
        cmp dl, JOY_MAX
        jb .c
        push cs
        pop es
        cmp bp, [n_lineas]
        je .mismos
        mov bl, 1
        mov byte [primera], 1       ; otro número de mandos: líneas nuevas
        mov [n_lineas], bp
.mismos:
        test bl, bl
        jz .esp
        call al_principio
        xor dl, dl
.p:
        call mando
        jc .pn
        call linea
.pn:
        push cs
        pop es
        inc dl
        cmp dl, JOY_MAX
        jb .p
        cmp word [n_lineas], 0
        jne .hay
        cmp byte [primera], 0
        je .hay
        mov si, m_ninguno
        call puts
.hay:
        call fin_pintado
.esp:
        call tecla_o_tic
        jnc .l
        mov ax, 0x4C00
        int 0x21

; una línea con el estado del mando DL (ES:DI)
linea:
        push dx
        mov si, m_mando
        call puts
        mov al, dl
        add al, '1'
        call putc
        xor bx, bx
.e:
        cmp bl, [es:di+HJS_NAX]
        jae .ed
        mov si, bx
        shl si, 2
        add si, n_ejes
        call puts
        mov al, [es:di+HJS_EJES+bx]
        call num3
        inc bx
        cmp bx, 6
        jb .e
.ed:
        mov si, m_cruceta
        call puts
        mov ah, [es:di+HJS_CRUZ]
        mov si, n_cruz
        mov cx, 4
.cr:
        lodsb
        shr ah, 1
        jc .cp
        mov al, '.'
.cp:
        call putc
        loop .cr
        mov si, m_bot
        call puts
        mov bx, [es:di+HJS_BTN]
        movzx cx, byte [es:di+HJS_NBTN]
        jcxz .bd
        mov ah, '1'
.b:
        mov al, '.'
        shr bx, 1
        jnc .bp
        mov al, ah
.bp:
        call putc
        inc ah
        cmp ah, '9'
        jbe .bn
        mov ah, '0'
.bn:
        loop .b
.bd:
        mov si, m_borra
        call puts
        call crlf
        pop dx
        ret

; AL en 3 columnas
num3:
        push ax
        push bx
        push cx
        xor ah, ah
        mov bl, 100
        div bl
        mov ch, ah
        mov cl, ' '
        test al, al
        jz .c
        add al, '0'
        mov cl, al
.c:
        mov al, cl
        call putc
        mov al, ch
        xor ah, ah
        mov bl, 10
        div bl
        test al, al
        jnz .d
        cmp cl, ' '
        jne .d
        mov al, ' ' - '0'
.d:
        add al, '0'
        call putc
        mov al, ah
        add al, '0'
        call putc
        pop cx
        pop bx
        pop ax
        ret

; ---- repintar en el mismo sitio ----
; antes de escribir las líneas: la primera vez se anota dónde empiezan; las
; siguientes se vuelve allí
al_principio:
        push ax
        push bx
        push cx
        push dx
        cmp byte [primera], 0
        jne .r
        mov ah, 2
        xor bh, bh
        mov dx, [inicio]
        int 0x10
.r:
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; después: la primera vez se calcula la fila de inicio (la pantalla puede
; haber subido al escribir) y se cuenta cuántas líneas son
fin_pintado:
        push ax
        push bx
        push cx
        push dx
        cmp byte [primera], 0
        je .r
        mov byte [primera], 0
        mov ah, 3
        xor bh, bh
        int 0x10                    ; DH = fila del cursor
        mov ax, [n_lineas]
        test ax, ax
        jnz .n
        inc ax                      ; (la línea de «ningún mando»)
.n:
        sub dh, al
        jnc .k
        xor dh, dh
.k:
        xor dl, dl
        mov [inicio], dx
.r:
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; esperar al siguiente tic del reloj; CF si se ha pulsado una tecla (se lee)
tecla_o_tic:
        push ax
        push bx
        push es
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.l:
        mov ah, 0x0B
        int 0x21
        test al, al
        jnz .k
        cmp bx, [es:0x46C]
        je .l
        pop es
        pop bx
        pop ax
        clc
        ret
.k:
        mov ah, 0x07
        int 0x21
        test al, al
        jnz .k1
        mov ah, 0x07                ; tecla extendida: su segundo código
        int 0x21
        xor al, al
.k1:
        mov [ultima], al
        pop es
        pop bx
        pop ax
        stc
        ret

; =============================================================================
;  MANDO /PROBAR: lo que ve un juego
; =============================================================================
probar:
        mov si, m_probar
        call puts
        mov byte [primera], 1
        mov word [n_lineas], 1
.l:
        mov ah, 0x84
        xor dx, dx
        int 0x15
        jc .sin
        mov [p_bot], al
        mov ah, 0x84
        mov dx, 1
        int 0x15
        mov [p_ejes], ax
        mov [p_ejes+2], bx
        mov [p_ejes+4], cx
        mov [p_ejes+6], dx
        mov si, p_bot
        mov di, p_viejo
        mov cx, 9
        repe cmpsb
        je .esp
        mov si, p_bot
        mov di, p_viejo
        mov cx, 9
        rep movsb
        call al_principio
        mov si, m_juego
        call puts
        xor bx, bx
.e:
        mov si, bx
        shl si, 1
        add si, n_cejes
        call puts
        movzx eax, word [p_ejes+bx]
        push bx
        mov cl, 3
        call putnum_w
        pop bx
        add bx, 2
        cmp bx, 8
        jb .e
        mov si, m_bot
        call puts
        mov ah, [p_bot]
        shr ah, 4
        mov al, '1'
.b:
        push ax
        shr ah, 1
        jnc .bp
        mov al, '.'                 ; un 1 = suelto
.bp:
        call putc
        pop ax
        shr ah, 1
        inc al
        cmp al, '4'
        jbe .b
        mov si, m_borra
        call puts
        call crlf
        call fin_pintado
.esp:
        call tecla_o_tic
        jnc .l
        mov ax, 0x4C00
        int 0x21
.sin:
        mov si, m_sinjuego
        call puts
        mov ax, 0x4C01
        int 0x21

; =============================================================================
;  MANDO /CALIBRAR: el mando del puerto de juegos
; =============================================================================
calibrar:
        mov dl, 2
        call mando
        push cs
        pop es
        jnc .hay
        mov si, m_nocal
        call puts
        mov ax, 0x4C01
        int 0x21
.hay:
        mov si, m_cal1
        call puts
        call espera_boton
        jc .fuera
        ; el centro
        call medir
        mov cx, 4
        xor bx, bx
.c:
        mov ax, [medida+bx]
        mov [c_cen+bx], ax
        mov [c_min+bx], ax
        mov [c_max+bx], ax
        add bx, 2
        loop .c
        mov si, m_cal2
        call puts
        ; los extremos: mientras se mueve
.m:
        call medir
        mov cx, 4
        xor bx, bx
.mm:
        mov ax, [medida+bx]
        cmp ax, [tmo]               ; sin respuesta: no cuenta
        jae .mn
        cmp ax, [c_min+bx]
        jae .m1
        mov [c_min+bx], ax
.m1:
        cmp ax, [c_max+bx]
        jbe .mn
        mov [c_max+bx], ax
.mn:
        add bx, 2
        loop .mm
        call boton_o_tecla
        jnc .m
        cmp byte [ultima], 27
        je .fuera
        ; un margen del 6 % en cada extremo, para que se llegue seguro
        ; a 0 y a 255, y a la configuración del residente
        les di, [res_off]
        add di, HJD_CAL
        xor bx, bx
.g:
        mov ax, [c_cen+bx]
        sub ax, [c_min+bx]
        shr ax, 4
        add ax, [c_min+bx]
        cli
        stosw
        mov ax, [c_cen+bx]
        stosw
        mov ax, [c_max+bx]
        sub ax, [c_cen+bx]
        shr ax, 4
        neg ax
        add ax, [c_max+bx]
        stosw
        sti
        add bx, 2
        cmp bx, 8
        jb .g
        push cs
        pop es
        mov si, m_cal3
        call puts
        call guardar
        mov ax, 0x4C00
        int 0x21
.fuera:
        mov si, m_cancel
        call puts
        mov ax, 0x4C01
        int 0x21

; leer el puerto de juegos -> medida (vueltas de los 4 ejes) y botones
medir:
        push es
        push di
        push dx
        mov dl, 2
        mov ax, 0xD902
        int 0x2F
        mov ax, [es:di+HJS_BTN]
        mov [m_btn], al
        mov dl, 3
        mov ax, 0xD902
        int 0x2F
        mov ax, [es:di+HJS_BTN]
        or [m_btn], al
        les di, [res_off]
        mov ax, [es:di+HJD_GTMO]
        mov [tmo], ax
        add di, HJD_GRAW
        mov ax, [es:di]
        mov [medida], ax
        mov ax, [es:di+2]
        mov [medida+2], ax
        mov ax, [es:di+4]
        mov [medida+4], ax
        mov ax, [es:di+6]
        mov [medida+6], ax
        pop dx
        pop di
        pop es
        ret

; ¿botón del mando (pulsar y soltar) o tecla? CF si sí ([ultima] = la tecla,
; 0 si fue un botón)
boton_o_tecla:
        mov ah, 0x0B
        int 0x21
        test al, al
        jz .b
        mov ah, 0x07
        int 0x21
        mov [ultima], al
        stc
        ret
.b:
        cmp byte [m_btn], 0
        jne .suelta
        clc
        ret
.suelta:
        call medir
        cmp byte [m_btn], 0
        jne .suelta
        mov byte [ultima], 0
        stc
        ret
; esperar un botón del mando o una tecla; CF si es Esc
espera_boton:
        call medir
        call boton_o_tecla
        jnc espera_boton
        cmp byte [ultima], 27
        je .esc
        clc
        ret
.esc:
        stc
        ret

; ---- guardar la calibración en MANDO.INI (junto a MANDO.COM) ----
; Se copia el fichero sin las líneas «calibrar=» que tuviera y se añade la
; nueva al final (vale en cualquier sección).
guardar:
        ; la ruta: la del programa, con MANDO.INI
        mov es, [0x2C]
        xor di, di
        xor al, al
        mov cx, 0x7FFF
.e:
        repne scasb
        scasb
        jne .e
        add di, 2
        mov si, di
        mov di, ruta
        mov bx, di
.c:
        mov al, [es:si]
        inc si
        mov [di], al
        inc di
        cmp al, '\'
        jne .c1
        mov bx, di
.c1:
        test al, al
        jz .c2
        cmp di, ruta + 64
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
        ; lo que hubiera
        mov word [ini_len], 0
        mov ax, 0x3D00
        mov dx, ruta
        int 0x21
        jc .nuevo
        mov bx, ax
        mov ah, 0x3F
        mov cx, INI_MAX
        mov dx, bufer
        int 0x21
        jc .ce
        mov [ini_len], ax
.ce:
        mov ah, 0x3E
        int 0x21
.nuevo:
        mov ah, 0x3C
        xor cx, cx
        mov dx, ruta
        int 0x21
        jc .err
        mov [h], ax
        ; línea a línea, sin las de calibrar
        mov si, bufer
        mov bx, si
        add bx, [ini_len]           ; BX = fin
.l:
        cmp si, bx
        jae .fin
        mov di, si                  ; DI = principio de la línea
.ll:
        cmp si, bx
        jae .lf
        lodsb
        cmp al, 10
        jne .ll
.lf:
        ; ¿empieza por «calibrar»?
        push si
        push di
        mov si, di
.sp:
        cmp byte [si], ' '
        je .sp1
        cmp byte [si], 9
        jne .sp2
.sp1:
        inc si
        jmp .sp
.sp2:
        mov di, k_calibrar
        mov cx, 8
.cmp:
        lodsb
        or al, 0x20
        cmp al, [di]
        jne .dif
        inc di
        loop .cmp
.dif:
        pop di
        pop si
        jcxz .l                     ; lo era: no se copia
        cmp byte [di], 26           ; (ni el Ctrl+Z del final)
        je .l
        mov cx, si
        sub cx, di
        mov dx, di
        call .escribe
        ; una última línea sin salto: se le pone
        cmp byte [si-1], 10
        je .l
        mov dx, k_crlf
        mov cx, 2
        call .escribe
        jmp .l
.fin:
        ; la línea nueva
        mov di, bufer
        mov si, k_calibrar
        mov cx, 8
        rep movsb
        mov al, '='
        stosb
        push ds
        lds si, [res_off]
        add si, HJD_CAL
        mov cx, 12
.n:
        lodsw
        call .dec
        cmp cx, 1
        je .n1
        mov al, ','
        stosb
.n1:
        loop .n
        pop ds
        mov ax, 0x0A0D
        stosw
        mov cx, di
        sub cx, bufer
        mov dx, bufer
        call .escribe
        mov bx, [h]
        mov ah, 0x3E
        int 0x21
        mov si, m_guardado
        call puts
        mov si, ruta
        call puts
        call crlf
        ret
.err:
        mov si, m_noguarda
        call puts
        ret
.escribe:
        push bx
        push ax
        mov bx, [h]
        mov ah, 0x40
        int 0x21
        pop ax
        pop bx
        ret
; AX en decimal a ES:DI
.dec:
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.d1:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d1
.d2:
        pop ax
        add al, '0'
        stosb
        loop .d2
        pop dx
        pop cx
        pop bx
        ret

%include "util.inc"

tipos       dw tp0, tp1, tp2, tp3, tp4, tp5, tp6, tp7, tp8
tp0         db "?", 0
tp1         db "USB", 0
tp2         db "USB tipo Xbox 360", 0
tp3         db "puerto de juegos", 0
tp4         db "joystick Atari/Commodore (impresora)", 0
tp5         db "Mega Drive (impresora)", 0
tp6         db "NES (impresora)", 0
tp7         db "Super Nintendo (impresora)", 0
tp8         db "PlayStation (impresora)", 0
n_ejes      db " X=", 0, " Y=", 0, " Z=", 0, " R=", 0, " L=", 0, " D=", 0
n_cejes     db " X=", 0, " Y=", 0, " Z=", 0, " R=", 0
n_cruz      db "ABID"
m_noesta    db "HSPJOY no está cargado. Cárguelo antes con la orden HSPJOY", 13, 10
            db "(mejor en AUTOEXEC.BAT). MANDO /? da la ayuda.", 13, 10, 0
m_uso       db "MANDO - los mandos de juego (HSPJOY)", 13, 10
            db "  MANDO             lista de los mandos y quién es cada jugador", 13, 10
            db "  MANDO /VER        ejes, cruceta y botones de cada mando, en vivo", 13, 10
            db "  MANDO /PROBAR     lo que ve un juego (INT 15h), en vivo", 13, 10
            db "  MANDO /CALIBRAR   calibra el mando del puerto de juegos y lo guarda", 13, 10
            db "Los mandos, el cable del puerto de impresora y las teclas de cada botón", 13, 10
            db "se eligen en MANDO.INI (junto a HSPJOY.COM); después, HSPJOY otra vez.", 13, 10, 0
m_titulo    db "Mandos de juego:", 13, 10, 0
m_coma      db ", ", 0
m_ejes      db " ejes y ", 0
m_botones   db " botones", 0
m_j1        db "  <- jugador 1", 0
m_j2        db "  <- jugador 2", 0
m_ninguno   db "  (no hay ningún mando)", 13, 10, 0
m_tec       db "Mando a teclado:", 0
m_tecj1     db " jugador 1", 0
m_tecj2     db " jugador 2", 0
m_pie       db "MANDO /VER los enseña en vivo; MANDO /? da las demás opciones.", 13, 10, 0
m_ver       db "Mueva los mandos y pulse sus botones. Una tecla para terminar.", 13, 10
            db "(cruceta: Arriba, aBajo, Izquierda, Derecha)", 13, 10, 0
m_mando     db "Mando ", 0
m_cruceta   db "  cruceta=", 0
m_bot       db "  botones=", 0
m_borra     db "   ", 0
m_probar    db "Así ve el mando un juego (INT 15h). Una tecla para terminar.", 13, 10, 0
m_juego     db "Juego:", 0
m_sinjuego  db "No hay ningún mando que dar a los juegos.", 13, 10, 0
m_nocal     db "No hay mando en el puerto de juegos: los demás no se calibran.", 13, 10, 0
m_cal1      db "Calibrar el mando del puerto de juegos (Esc para dejarlo)", 13, 10
            db "1. Suelte la palanca, que quede en el centro, y pulse un botón del", 13, 10
            db "   mando (o una tecla).", 13, 10, 0
m_cal2      db "2. Lleve la palanca hasta los topes dando un par de vueltas completas", 13, 10
            db "   y pulse un botón del mando (o una tecla).", 13, 10, 0
m_cal3      db "Calibrado.", 13, 10, 0
m_cancel    db "Calibración cancelada.", 13, 10, 0
m_guardado  db "Guardado en ", 0
m_noguarda  db "No se ha podido guardar en MANDO.INI (vale hasta apagar).", 13, 10, 0
ini_nombre  db "MANDO.INI", 0
k_calibrar  db "calibrar"
k_crlf      db 13, 10
res_off     dw 0
res_seg     dw 0
tabla       dw 0
primera     db 1
ultima      db 0
n_lineas    dw 0xFFFF
inicio      dw 0
p_viejo     times 9 db 0xEE
p_bot       db 0
p_ejes      dw 0, 0, 0, 0
m_btn       db 0
            db 0
tmo         dw 0
medida      dw 0, 0, 0, 0
c_min       dw 0, 0, 0, 0
c_cen       dw 0, 0, 0, 0
c_max       dw 0, 0, 0, 0
h           dw 0
ini_len     dw 0
ruta        times 80 db 0
copia       times JOY_MAX * HJS_SZ db 0xEE
u_numbuf    times 16 db 0
u_lbuf      times 84 db 0
INI_MAX     equ 12000
bufer:
