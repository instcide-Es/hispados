; =============================================================================
;  CUENTAS.COM - la hoja de cálculo de Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       CUENTAS [archivo] [/G destino] [/I destino]
;
;  Una hoja de 256 columnas (A a IV) por 9.999 filas, de las que solo ocupan
;  memoria las celdas que tienen algo. Para un PC o un XT de 256 K con
;  cualquier tarjeta (MDA, Hercules, CGA, EGA, VGA) en 80 columnas; la celda
;  y lo marcado se ven en vídeo inverso, sin depender del color.
;
;  Números exactos en coma fija (0,1 + 0,2 da 0,3), con coma decimal, puntos
;  de los miles, euros (€) y pesetas (₧), tantos por ciento y fechas.
;  Fórmulas en español y a la manera de hoy (=SUMA(A1:A10)), y también las de
;  VisiCalc y 1-2-3 (+A1, @SUM(A1...A10)). Recálculo de solo lo que depende
;  de lo cambiado, en el orden de las dependencias, y aviso de las
;  referencias circulares. Deshacer (Ctrl+Z), copiar con referencias
;  relativas y fijas ($A$1), anchos de columna, ordenar filas, imprimir.
;  Archivos: el propio (.CTA, compacto, con formatos y anchos), .CSV (con «;»,
;  el de toda la familia y el de las hojas de hoy) y .DIF (el de VisiCalc):
;  se elige por la extensión, al abrir y al guardar.
;
;  Sin pantalla: /G destino guarda en otro formato y /I destino imprime en un
;  archivo (o en PRN), y sale. Sirve para convertir en un lote... y para la
;  prueba automática.
;
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

NFIL        equ 9999                ; filas
NCOL        equ 256                 ; columnas
VFILAS      equ 21                  ; filas de la rejilla en la pantalla
ROT         equ 5                   ; ancho de los números de fila
ANCHO0      equ 10                  ; ancho de las columnas, al empezar
LIN_MAX     equ 240                 ; lo más largo que se teclea
A_NORMAL    equ 0x07
A_INVERSO   equ 0x70
A_BRILLO    equ 0x0F
EURO        equ 0xEE                ; la letra del euro (se dibuja en EGA/VGA)
PESETA      equ 0x9E                ; ₧, que ya está en la página 437
; los formatos (bits 4-7 del octeto de formato)
F_GEN       equ 0x00
F_FIJO      equ 0x10
F_EURO      equ 0x20
F_PTA       equ 0x30
F_POR       equ 0x40
F_FECHA     equ 0x50

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        mov sp, pila_fin
        mov bx, (pila_fin - inicio + 0x100 + 15) / 16
        DOS 0x4A                    ; el programa se queda con lo suyo
        mov bx, 0xFFFF              ; y pide lo demás
        DOS 0x48
        cmp bx, 2048                ; (menos de 32 K: no merece la pena)
        jb sin_memoria
        push bx
        DOS 0x48
        pop bx
        jc sin_memoria
        ; un décimo para cada búfer de deshacer y otro para el índice (cada
        ; uno hasta 64 K), y lo demás para el montón (hasta 256 K)
        mov cx, ax
        mov ax, bx
        xor dx, dx
        mov si, 10
        div si
        cmp ax, 4096
        jbe .t
        mov ax, 4096
.t:
        mov [ud_a], cx
        add cx, ax
        mov [ud_b], cx
        add cx, ax
        mov [ix_seg], cx
        add cx, ax
        mov [hp_seg], cx
        sub bx, ax
        sub bx, ax
        sub bx, ax
        mov dx, ax
        mov cl, 4
        shl dx, cl
        dec dx
        mov [ud_tam], dx
        inc dx
        shr dx, 1
        mov [ix_max], dx
        cmp bx, 16383               ; el montón, en grupos de cuatro octetos
        jbe .h
        mov bx, 16383
.h:
        shl bx, 1
        shl bx, 1
        mov [hp_lim], bx
        call borra_hoja
        call args
        cmp byte [nombre], 0
        je .sin
        call carga
        jnc .sin
        cmp byte [lote], 0
        jne lote_mal
.sin:
        cmp byte [lote], 0
        jne en_lote
        call pantalla_ini
        jmp bucle

sin_memoria:
        mov dx, m_sin_mem
        jmp short fin_msg
lote_mal:
        mov dx, m_lote_mal
fin_msg:
        DOS 0x09
        mov ax, 0x4C01
        int 0x21

; ---- sin pantalla: guardar o imprimir y salir ----
en_lote:
        mov si, destino
        mov di, nombre
        call copia_cad
        cmp byte [lote], 'G'
        jne .imp
        call guarda
        jc lote_mal
        jmp short .ok
.imp:
        call rango_usado
        mov dx, destino
        call imprime
        jc lote_mal
.ok:
        mov ax, 0x4C00
        int 0x21

; ---- la línea de órdenes: el archivo, /G destino, /I destino ----
args:
        mov si, 0x81
.l:
        call blancos
        cmp al, 13
        je .r
        test al, al
        jz .r
        cmp al, '/'
        je .op
        mov di, nombre
        call palabra
        jmp .l
.op:
        mov al, [si+1]
        and al, 0xDF
        mov [lote], al
        add si, 2
        call blancos
        mov di, destino
        call palabra
        jmp .l
.r:
        ret
; copia hasta un blanco, en mayúsculas
palabra:
        push ds
        pop es
.l:
        lodsb
        cmp al, ' '
        jbe .f
        call mayus
        stosb
        jmp .l
.f:
        dec si
        mov byte [di], 0
        ret
mayus:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 0x20
.r:
        ret
copia_cad:
        push ds
        pop es
.l:
        lodsb
        stosb
        test al, al
        jnz .l
        ret

; ---- la hoja, vacía ----
borra_hoja:
        xor ax, ax
        mov [n_cel], ax
        mov [hp_top], ax
        mov [cf], ax
        mov [arr], ax
        mov [cc], al
        mov [izq], al
        mov [sel_on], al
        mov [marcar], al
        mov [cambios], al
        mov [ud_hay], al
        mov [hay_clip], al
        push ds
        pop es
        mov di, anch
        mov cx, NCOL
        mov al, ANCHO0
        rep stosb
        mov byte [sucio], 2
        ret

; =============================================================================
;  La pantalla
; =============================================================================
pantalla_ini:
        mov ah, 0x0F
        int 0x10
        mov [modo], al
        mov word [vseg], 0xB800
        cmp al, 7
        jne .c
        mov word [vseg], 0xB000
        jmp short .e
.c:
        cmp al, 2
        je .e
        cmp al, 3
        je .e
        mov ax, 3
        int 0x10
        mov byte [modo], 3
.e:
        mov ah, 0x12                ; ¿EGA o VGA? se les dibuja el euro
        mov bl, 0x10
        int 0x10
        cmp bl, 0x10
        je .r
        mov ax, 0x1130
        xor bh, bh
        push bp
        int 0x10
        pop bp
        push ds
        pop es
        mov bp, letra_euro
        cmp cx, 16
        je .d
        cmp cx, 14
        jne .r
        inc bp
.d:
        mov bh, cl
        xor bl, bl
        mov cx, 1
        mov dx, EURO
        mov ax, 0x1110
        int 0x10
.r:
        ret
letra_euro  db 0, 0, 0x1E, 0x33, 0x60, 0xFC, 0x60, 0xF8, 0x60, 0x33, 0x1E, 0, 0, 0, 0, 0

; ---- el cursor de la BIOS, fuera de la vista ----
esconde:
        mov dx, 0x1900
cursor_en:
        xor bh, bh
        mov ah, 2
        int 0x10
        ret

; ---- escribe el texto de SI (acabado en 0) en ES:DI con el color AH ----
vtexto:
        lodsb
        test al, al
        jz .r
        stosw
        jmp vtexto
.r:
        ret
; ---- hasta la columna 80 de la fila de DI, con blancos del color AH ----
vfin:
        mov al, ' '
.l:
        push ax
        mov ax, di
        xor dx, dx
        mov cx, 160
        div cx
        pop ax
        test dx, dx
        jz .r
        stosw
        jmp .l
.r:
        ret

; ---- pinta lo que haga falta ----
pinta:
        call ajusta_vista
        call rango_sel
        mov es, [vseg]
        cmp byte [sucio], 2
        jb .poco
        call pinta_cab
        xor cx, cx
.f:
        push cx
        call pinta_fila
        pop cx
        inc cx
        cmp cx, VFILAS
        jb .f
        mov es, [vseg]
        mov di, 24 * 160
        mov si, m_ayuda
        mov ah, A_INVERSO
        call vtexto
        call vfin
        jmp short .arriba
.poco:
        cmp byte [sucio], 1
        jb .arriba
        call pinta_cab
        mov cx, [f_ant]
        sub cx, [arr]
        cmp cx, VFILAS
        jae .p1
        call pinta_fila
.p1:
        mov cx, [cf]
        sub cx, [arr]
        call pinta_fila
.arriba:
        mov byte [sucio], 0
        mov ax, [cf]
        mov [f_ant], ax
        call pinta_arriba
        jmp esconde

; ---- la vista sigue al cursor ----
ajusta_vista:
        mov ax, [cf]
        cmp ax, [arr]
        jae .a1
        mov [arr], ax
        mov byte [sucio], 2
.a1:
        mov bx, [arr]
        add bx, VFILAS - 1
        cmp ax, bx
        jbe .a2
        sub ax, VFILAS - 1
        mov [arr], ax
        mov byte [sucio], 2
.a2:
        mov al, [cc]
        cmp al, [izq]
        jae .c1
        mov [izq], al
        mov byte [sucio], 2
.c1:
        mov bl, [izq]               ; ¿cabe entera desde izq?
        xor bh, bh
        xor dx, dx
.c2:
        mov al, [anch + bx]
        xor ah, ah
        add dx, ax
        cmp bl, [cc]
        je .c3
        inc bx
        jmp .c2
.c3:
        cmp dx, 80 - ROT
        jbe .r
        inc byte [izq]
        mov byte [sucio], 2
        jmp .c1
.r:
        ret

; ---- los rótulos de las columnas (fila 2) ----
pinta_cab:
        mov es, [vseg]
        mov di, 2 * 160
        mov ax, A_INVERSO * 256 + ' '
        mov cx, ROT
        rep stosw
        mov bl, [izq]
        mov dx, ROT
.c:
        xor bh, bh
        mov cl, [anch + bx]
        xor ch, ch
        mov ax, dx
        add ax, cx
        cmp ax, 80
        ja .fin
        push dx
        push cx
        mov ah, A_INVERSO
        cmp bl, [cc]
        jne .a
        mov ah, A_BRILLO
.a:
        mov [t_atr], ah
        push di                     ; el nombre, en medio
        mov di, dtx
        push ds
        pop es
        push bx
        call pon_col
        pop bx
        mov ax, di
        sub ax, dtx
        pop di
        mov es, [vseg]
        pop cx
        push cx
        cmp ax, cx
        jbe .n1
        mov ax, cx
.n1:
        mov si, dtx
        add si, ax
        mov byte [si], 0
        sub cx, ax
        mov dx, cx
        shr cx, 1
        sub dx, cx
        mov ah, [t_atr]
        mov al, ' '
        rep stosw
        mov si, dtx
        call vtexto
        mov cx, dx
        mov al, ' '
        rep stosw
        pop cx
        pop dx
        add dx, cx
        inc bl
        jnz .c
.fin:
        mov ah, A_INVERSO
        jmp vfin

; ---- la fila CX de la rejilla ----
pinta_fila:
        mov ax, cx
        add ax, [arr]
        mov [pf_fila], ax
        mov ax, 160
        add cx, 3
        mul cx
        mov [pf_di], ax
        mov dx, [pf_fila]           ; las celdas, a ltx
        mov bl, [izq]
        mov bh, 255
        mov cx, 80 - ROT
        call construye
        push ds
        pop es
        mov di, lbuf                ; el número de la fila
        mov ax, [pf_fila]
        inc ax
        mov ah, A_INVERSO
        mov cx, [pf_fila]
        cmp cx, [cf]
        jne .n0
        mov ah, A_BRILLO
.n0:
        mov [t_atr], ah
        mov ax, [pf_fila]
        inc ax
        push di
        mov di, dtx
        call pon_dec
        mov cx, di
        sub cx, dtx
        pop di
        mov dx, ROT - 1
        sub dx, cx
        mov ah, [t_atr]
        mov al, ' '
.n1:
        test dx, dx
        jz .n1b
        stosw
        dec dx
        jmp .n1
.n1b:
        mov si, dtx
.n2:
        lodsb
        stosw
        loop .n2
        mov al, ' '
        stosw
        ; las letras, con el color de cada columna
        mov si, ltx
        mov bl, [izq]
        mov dx, ROT
.c:
        xor bh, bh
        mov cl, [anch + bx]
        xor ch, ch
        mov ax, dx
        add ax, cx
        cmp ax, 80
        ja .resto
        add dx, cx
        push dx
        mov dx, [pf_fila]
        call atr_celda
        pop dx
.c1:
        lodsb
        stosw
        loop .c1
        inc bl
        jnz .c
.resto:
        mov ax, A_NORMAL * 256 + ' '
        mov cx, 80
        sub cx, dx
        rep stosw
        mov es, [vseg]
        mov di, [pf_di]
        mov si, lbuf
        mov cx, 80
        rep movsw
        ret

; ---- AH = el color de la celda DX, BL ----
atr_celda:
        mov ah, A_NORMAL
        cmp dx, [q_f1]
        jb .fuera
        cmp dx, [q_f2]
        ja .fuera
        cmp bl, [q_c1]
        jb .fuera
        cmp bl, [q_c2]
        ja .fuera
        cmp byte [sel_on], 0
        je .fuera
        mov ah, A_INVERSO           ; dentro de lo marcado
        cmp dx, [cf]
        jne .r
        cmp bl, [cc]
        jne .r
        mov ah, A_BRILLO            ; (y la celda del cursor, en hueco)
.r:
        ret
.fuera:
        cmp dx, [cf]
        jne .r
        cmp bl, [cc]
        jne .r
        mov ah, A_INVERSO
        ret

; ---- las dos filas de arriba ----
pinta_arriba:
        push ds
        pop es
        mov di, ebuf2               ; fila 0: la celda (o lo marcado)
        mov dx, [q_f1]
        mov bl, [q_c1]
        xor ah, ah
        call pon_ref
        cmp byte [sel_on], 0
        je .a1
        mov al, ':'
        stosb
        mov dx, [q_f2]
        mov bl, [q_c2]
        xor ah, ah
        call pon_ref
.a1:
        mov al, ' '
        stosb
        mov byte [di], 0
        ; (el formato)
        mov dx, [cf]
        mov bl, [cc]
        call busca
        mov al, 0
        jc .sinf
        mov al, [es:di+R_FMT]
.sinf:
        push ds
        pop es
        mov di, ebuf2
        call fin_cad
        mov dl, al
        shr al, 1
        shr al, 1
        shr al, 1
        shr al, 1
        cbw
        mov bx, ax
        shl bx, 1
        mov si, [nom_fmt + bx]
        test si, si
        jz .a4
        mov al, '('
        stosb
        call copia_z
        cmp bl, F_FIJO >> 3
        je .dec
        cmp bl, F_POR >> 3
        jne .a5
.dec:
        mov al, ' '
        stosb
        mov al, dl
        and al, 15
        add al, '0'
        stosb
.a5:
        mov ax, ') '
        stosw
.a4:
        mov dx, [cf]                ; y lo que tiene, tal cual
        mov bl, [cc]
        push di
        call texto_edicion
        pop di
        mov si, ebuf3
        call copia_z
        mov byte [di], 0
        mov es, [vseg]
        xor di, di
        mov si, ebuf2
        mov ah, A_NORMAL
        mov cx, 70
.a6:
        lodsb
        test al, al
        jz .a7
        stosw
        loop .a6
.a7:
        mov al, ' '
        rep stosw
        mov si, m_listo             ; y a la derecha, el modo
        cmp byte [marcar], 0
        je .a8
        mov si, m_marcar
.a8:
        mov ah, A_INVERSO
        call vtexto
        mov al, ' '
        cmp byte [cambios], 0
        je .a9
        mov al, '*'
.a9:
        stosw
        call vfin
        ; fila 1: un aviso, o el archivo
        mov di, 160
        mov si, [aviso]
        mov ah, A_BRILLO
        test si, si
        jnz .b1
        mov si, m_cuentas
        mov ah, A_NORMAL
        call vtexto
        mov si, nombre
        cmp byte [si], 0
        jne .b1
        mov si, m_sin_nombre
.b1:
        call vtexto
        mov ah, A_NORMAL
        jmp vfin

fin_cad:
        cmp byte [di], 0
        je .r
        inc di
        jmp fin_cad
.r:
        ret

; ---- el rango marcado (o la celda del cursor) en q_f1..q_c2 ----
rango_sel:
        mov ax, [cf]
        mov bx, ax
        mov cl, [cc]
        mov ch, cl
        cmp byte [sel_on], 0
        je .p
        mov bx, [ancla_f]
        mov ch, [ancla_c]
        cmp ax, bx
        jbe .a
        xchg ax, bx
.a:
        cmp cl, ch
        jbe .p
        xchg cl, ch
.p:
        mov [q_f1], ax
        mov [q_f2], bx
        mov [q_c1], cl
        mov [q_c2], ch
        ret

; =============================================================================
;  Lo que se ve de una celda
; =============================================================================
; ---- construye en ltx la fila DX desde la columna BL hasta la BH, sin pasar
; de CX letras. ltx_n = las que salen. Un texto largo sigue por las celdas
; vacías de su derecha, como en 1-2-3.
construye:
        mov [cs_fila], dx
        mov [cs_lim], cx
        mov [cs_c2], bh
        mov word [ltx_n], 0
        mov word [ov_n], 0
.c:
        xor bh, bh
        mov al, [anch + bx]
        xor ah, ah
        mov [ancho_c], ax
        add ax, [ltx_n]
        cmp ax, [cs_lim]
        ja .fin
        push bx
        mov dx, [cs_fila]
        call celda_txt              ; dtx, CX, AL = cómo se alinea
        pop bx
        pushf
        push ds
        pop es
        mov di, ltx
        add di, [ltx_n]
        popf
        jc .vacia
        mov word [ov_n], 0
        mov dx, [ancho_c]
        cmp al, 3
        je .repite
        cmp cx, dx
        jbe .cabe
        cmp al, 0                   ; texto a la izquierda que no cabe: sigue
        jne .corta
        push cx
        mov si, dtx
        mov cx, dx
        rep movsb
        pop cx
        sub cx, dx
        mov [ov_n], cx
        mov [ov_p], si
        jmp short .sig
.corta:
        mov cx, dx
.cabe:
        mov si, dtx                 ; DX - CX blancos, repartidos
        sub dx, cx
        push dx
        cmp al, 0
        je .izq
        cmp al, 1
        je .der
        shr dx, 1
.der:
        push cx
        mov cx, dx
        mov al, ' '
        rep stosb
        pop cx
        pop ax
        sub ax, dx
        mov dx, ax
        push dx
.izq:
        rep movsb
        pop cx
        mov al, ' '
        rep stosb
        jmp short .sig
.repite:
        mov si, dtx                 ; \- : lo que haya, repetido
        jcxz .vacia0
        mov ah, cl
.r1:
        mov cl, ah
        xor ch, ch
        mov si, dtx
.r2:
        movsb
        dec dx
        jz .sig
        loop .r2
        jmp .r1
.vacia0:
        mov cx, dx
        mov al, ' '
        rep stosb
        jmp short .sig
.vacia:
        mov cx, [ancho_c]
        mov ax, [ov_n]
        test ax, ax
        jz .blanca
        cmp ax, cx
        jbe .v1
        mov ax, cx
.v1:
        sub cx, ax
        sub [ov_n], ax
        mov si, [ov_p]
        xchg ax, cx
        rep movsb
        mov [ov_p], si
        mov cx, ax
.blanca:
        mov al, ' '
        rep stosb
.sig:
        mov ax, [ancho_c]
        add [ltx_n], ax
        cmp bl, [cs_c2]
        je .fin
        inc bl
        jmp .c
.fin:
        ret

; ---- el texto de la celda DX, BL tal como se ve, en dtx (CX letras) ----
; AL: 0 a la izquierda, 1 a la derecha, 2 en medio, 3 repetido. CF: vacía.
celda_txt:
        call busca
        jc .r
        mov al, [es:di+R_TIPO]
        and al, 3
        jnz .num
        mov cl, [es:di+R_LEN]       ; un texto
        xor ch, ch
        lea si, [di+6]
        xor ax, ax
        jcxz .copia
        mov ah, [es:si]
        mov al, 0
        cmp ah, "'"
        je .pref
        inc ax
        cmp ah, '"'
        je .pref
        inc ax
        cmp ah, '^'
        je .pref
        inc ax
        cmp ah, '\'
        je .pref
        xor al, al
        jmp short .copia
.pref:
        inc si
        dec cx
.copia:
        push ax
        push cx
        push ds
        mov di, dtx
        push ds
        push es
        pop ds
        pop es
        rep movsb
        pop ds
        pop cx
        pop ax
        clc
.r:
        ret
.num:
        mov al, [es:di+R_FMT]
        mov [n_fmt], al
        call valor_a_acc
        call num_celda
        mov al, 1
        clc
        ret

; ---- acc con el formato n_fmt, en dtx, sin pasar de ancho_c: CX letras ----
num_celda:
        push ds
        pop es
        mov byte [npunto], 12
        mov al, [n_fmt]
        mov ah, al
        and ah, 15
        and al, 0xF0
        cmp word [acc+6], 0x8000    ; (los errores, tal cual)
        je .gen
        cmp al, F_FECHA
        je .fecha
        cmp al, F_FIJO
        je .fijo
        cmp al, F_EURO
        je .euro
        cmp al, F_PTA
        je .pta
        cmp al, F_POR
        je .por
.gen:
        mov di, dtx                 ; general: los decimales que quepan
        mov cx, 0x00FE
        call num_txt
        call .mide
        jbe .r
        mov si, dtx                 ; ¿cuántos decimales tenía?
.g1:
        cmp si, di
        jae .no
        lodsb
        cmp al, ','
        jne .g1
        mov cx, di
        sub cx, si
        mov [t_dec], cl
.g2:
        dec byte [t_dec]
        js .no
        mov cl, [t_dec]
        xor ch, ch
        mov di, dtx
        call num_txt
        call .mide
        ja .g2
        ret
.fijo:
        mov cl, ah
        mov ch, 1
        jmp short .escribe
.euro:
        mov cx, 0x0102
        mov dx, EURO
        jmp short .sufijo
.pta:
        mov cx, 0x0100
        mov dx, PESETA
.sufijo:
        push dx
        mov di, dtx
        call num_txt
        pop dx
        cmp byte [en_papel], 0
        je .s1
        mov al, ' '
        stosb
        mov si, t_eur
        cmp dl, EURO
        je .s0
        mov si, t_pts
.s0:
        call copia_z
        jmp short .s2
.s1:
        mov al, dl
        stosb
.s2:
        call .mide
        jbe .r
        jmp short .no
.por:
        mov byte [npunto], 14
        mov cl, ah
        xor ch, ch
        mov di, dtx
        call num_txt
        mov al, '%'
        stosb
        mov byte [npunto], 12
        call .mide
        jbe .r
        jmp short .no
.fecha:
        mov di, dtx
        call serie_fecha
        jc .no
        call .mide
        jbe .r
        jmp short .no
.escribe:
        mov di, dtx
        call num_txt
        call .mide
        jbe .r
.no:
        mov cx, [ancho_c]           ; no cabe: almohadillas
        mov di, dtx
        mov al, '#'
        push cx
        rep stosb
        pop cx
.r:
        ret
.mide:
        mov cx, di
        sub cx, dtx
        cmp cx, [ancho_c]
        ret

; ---- en ebuf3, lo que tiene la celda DX, BL para escribirla de nuevo: el
; texto o la fórmula tal cual; un número, con todos sus decimales (una fecha,
; como fecha; un tanto por ciento, con «%») ----
texto_edicion:
        push ds
        pop es
        mov byte [ebuf3], 0
        call busca
        jc .r
        mov al, [es:di+R_TIPO]
        and al, 3
        cmp al, T_NUM
        je .num
        call txt_rec
        mov si, ax
        add si, di
        mov cl, [es:di+R_LEN]
        xor ch, ch
        push ds
        mov di, ebuf3
        push ds
        push es
        pop ds
        pop es
        rep movsb
        pop ds
        mov byte [di], 0
        ret
.num:
        mov al, [es:di+R_FMT]
        and al, 0xF0
        push ax
        call valor_a_acc
        push ds
        pop es
        pop ax
        mov di, ebuf3
        cmp al, F_FECHA
        jne .n1
        call serie_fecha
        jnc .fin
        mov di, ebuf3
        jmp short .n2
.n1:
        cmp al, F_POR
        jne .n2
        mov byte [npunto], 14
        mov cx, 0x00FF
        call num_txt
        mov byte [npunto], 12
        mov al, '%'
        stosb
        jmp short .fin
.n2:
        mov cx, 0x00FF
        call num_txt
.fin:
        mov byte [di], 0
.r:
        push ds
        pop es
        ret

; =============================================================================
;  El teclado
; =============================================================================
bucle:
        mov ah, 1
        int 0x16
        jnz .tecla
        call pinta
.tecla:
        xor ah, ah
        int 0x16
        mov word [aviso], 0
        mov [tecla], ax
        mov ah, 2
        int 0x16
        and al, 3
        mov [mayusc], al
        mov ax, [tecla]
        mov bx, teclas_mov          ; ¿moverse?
.m:
        cmp byte [bx], 0
        je .no_mov
        cmp ah, [bx]
        je .mov
        add bx, 3
        jmp .m
.mov:
        cmp al, 0
        je .mov1
        cmp al, 0xE0
        je .mov1
        cmp byte [mayusc], 0        ; (en un XT, con Mayúsculas salen cifras)
        je .no_mov
.mov1:
        mov dx, [bx+1]
        call mueve
        jmp bucle
.no_mov:
        cmp al, 0
        je .esp
        cmp al, 0xE0
        je .esp
        mov bx, teclas_a
        jmp short .busca
.esp:
        mov al, ah
        mov bx, teclas_e
.busca:
        cmp byte [bx], 0
        je .letra
        cmp al, [bx]
        je .esta
        add bx, 3
        jmp .busca
.esta:
        call [bx+1]
        jmp bucle
.letra:
        cmp bx, teclas_e
        jae bucle
        cmp al, ' '
        jb bucle
        call teclea
        jmp bucle

%macro TECLA 2
        db %1
        dw %2
%endmacro
%macro MOV1 3
        db %1
        db %2, %3
%endmacro
; las flechas: la tecla y cuánto se mueve (filas, columnas)
teclas_mov:
        MOV1 0x48, -1, 0
        MOV1 0x50, 1, 0
        MOV1 0x4B, 0, -1
        MOV1 0x4D, 0, 1
        MOV1 0x49, -VFILAS, 0
        MOV1 0x51, VFILAS, 0
        db 0
teclas_a:
        TECLA 13, abajo
        TECLA 9, derecha
        TECLA 27, escape
        TECLA 26, c_deshacer
        TECLA '/', c_menu
        db 0
teclas_e:
        TECLA 0x3B, c_ayuda
        TECLA 0x3C, c_editar
        TECLA 0x3D, c_copiar
        TECLA 0x3E, c_pegar
        TECLA 0x3F, c_ir
        TECLA 0x42, c_marcar
        TECLA 0x43, c_calcular
        TECLA 0x44, c_menu
        TECLA 0x53, c_borrar
        TECLA 0x47, c_inicio
        TECLA 0x4F, c_fin
        TECLA 0x0F, izquierda
        TECLA 0x73, c_estrecha
        TECLA 0x74, c_ensancha
        db 0

abajo:
        mov dx, 0x0001
        jmp short mueve_sin
derecha:
        mov dx, 0x0100
        jmp short mueve_sin
izquierda:
        mov dx, 0xFF00
mueve_sin:
        mov byte [mayusc], 0
; ---- mueve el cursor DL filas y DH columnas (con Mayúsculas o F8, marca) ----
mueve:
        mov al, [mayusc]
        or al, [marcar]
        jz .sin
        cmp byte [sel_on], 0
        jne .mueve
        mov byte [sel_on], 1
        mov ax, [cf]
        mov [ancla_f], ax
        mov al, [cc]
        mov [ancla_c], al
        jmp short .mueve
.sin:
        cmp byte [sel_on], 0
        je .mueve
        mov byte [sel_on], 0
        mov byte [sucio], 2
.mueve:
        mov al, dl
        cbw
        add ax, [cf]
        jns .f1
        xor ax, ax
.f1:
        cmp ax, NFIL
        jb .f2
        mov ax, NFIL - 1
.f2:
        mov [cf], ax
        mov al, dh
        cbw
        mov bl, [cc]
        xor bh, bh
        add ax, bx
        jns .c1
        xor ax, ax
.c1:
        cmp ax, 255
        jbe .c2
        mov ax, 255
.c2:
        mov [cc], al
        cmp byte [sel_on], 0
        je .poco
        mov byte [sucio], 2
.poco:
        cmp byte [sucio], 1
        jae .r
        mov byte [sucio], 1
.r:
        ret

escape:
        mov byte [marcar], 0
        cmp byte [sel_on], 0
        je .r
        mov byte [sel_on], 0
        mov byte [sucio], 2
.r:
        ret
c_marcar:
        xor byte [marcar], 1
        ret
c_inicio:
        xor ax, ax
        xor bl, bl
        jmp short salta
c_fin:
        call rango_usado
        mov ax, [q_f2]
        mov bl, [q_c2]
; ---- el cursor a AX, BL (con Mayúsculas o F8, marcando) ----
salta:
        push ax
        push bx
        xor dx, dx
        call mueve
        pop bx
        pop ax
        mov [cf], ax
        mov [cc], bl
        mov byte [sucio], 2
        ret
a_celda:
        mov byte [sel_on], 0
        mov byte [sucio], 2
        ret
c_calcular:
        call recalcula_todo
        mov byte [sucio], 2
        jmp aviso_circ
c_deshacer:
        call deshacer
        jmp aviso_circ0

; ---- Ctrl+← y Ctrl+→: la columna, más estrecha o más ancha ----
c_estrecha:
        mov al, -1
        jmp short anchura
c_ensancha:
        mov al, 1
anchura:
        mov bl, [cc]
        xor bh, bh
        add al, [anch + bx]
        cmp al, 1
        jb .r
        cmp al, 75
        ja .r
        mov [anch + bx], al
        mov byte [cambios], 1
        mov byte [sucio], 2
.r:
        ret

; =============================================================================
;  Escribir en una celda
; =============================================================================
c_editar:
        mov dx, [cf]
        mov bl, [cc]
        call texto_edicion
        mov si, ebuf3
        mov di, ebuf
        call copia_cad
        jmp short edita_celda
teclea:
        mov [ebuf], al
        mov byte [ebuf+1], 0
edita_celda:
        mov si, ebuf                ; el cursor, al final
        xor cx, cx
.l:
        lodsb
        test al, al
        jz .f
        inc cx
        jmp .l
.f:
        mov [el_n], cx
        mov [el_x], cx
.otra:
        mov word [el_pre], m_vacio
        call edita_linea
        cmp al, 27
        je .r
        push ax
        call abre_ud
        mov dx, [cf]
        mov bl, [cc]
        call entra
        pop ax
        jnc .bien
        mov word [aviso], m_mal_formula
        mov ax, [ev_pos]            ; el cursor, donde se ve el error
        sub ax, ftxt
        add ax, [en_ini]
        cmp ax, [el_n]
        jbe .x
        mov ax, [el_n]
.x:
        mov [el_x], ax
        call pinta_arriba
        jmp .otra
.bien:
        mov byte [sucio], 2
        mov word [aviso], 0
        call aviso_circ
        cmp al, 13                  ; y se mueve según la tecla
        je abajo
        cmp al, 9
        je derecha
        cmp ax, 0x0F00
        je izquierda
        mov dx, 0x00FF
        cmp ah, 0x48
        je mueve_sin
        mov dx, 0x0001
        cmp ah, 0x50
        je mueve_sin
.r:
        ret

; ---- si hubo una referencia circular, el aviso ----
aviso_circ:
        cmp word [circ_f], 0xFFFF
        je aviso_circ0
        push ax
        push ds
        pop es
        mov si, m_circ
        mov di, avbuf
        call copia_z
        mov dx, [circ_f]
        mov bl, [circ_c]
        xor ah, ah
        call pon_ref
        mov byte [di], 0
        mov word [aviso], avbuf
        mov word [circ_f], 0xFFFF
        pop ax
aviso_circ0:
        ret

; ---- entra: lo de ebuf, a la celda DX, BL. CF si es una fórmula mal escrita
entra:
        push ds
        pop es
        mov [en_f], dx
        mov [en_c], bl
        mov byte [en_fmt], 0        ; el formato que tenía, se conserva
        call busca
        jc .sinf
        mov al, [es:di+R_FMT]
        mov [en_fmt], al
.sinf:
        push ds
        pop es
        mov si, ebuf
        call blancos
        mov [en_ini], si
        sub word [en_ini], ebuf
        test al, al
        jnz .algo0
        mov dx, [en_f]              ; nada: la celda se vacía
        mov bl, [en_c]
        xor cx, cx
        call pon_celda
        jmp .hecho
.algo0:
        cmp byte [en_texto], 0
        je .algo
        push si
        jmp .texto0
.algo:
        push si
        call lee_fecha              ; ¿una fecha?
        jc .nofecha
        call blancos
        test al, al
        jnz .nofecha
        pop ax
        mov byte [en_fmt], F_FECHA
        jmp .numero
.nofecha:
        pop si
        push si
        mov byte [nmodo], 0         ; ¿un número?
        mov byte [en_neg], 0
        lodsb
        cmp al, '+'
        je .sg
        cmp al, '-'
        jne .s0
        mov byte [en_neg], 1
.sg:
        call blancos
        jmp short .s1
.s0:
        dec si
.s1:
        call lee_num
        jc .nonum
        call blancos
        cmp al, '%'
        jne .n1
        inc si
        mov al, [nd_dec]
        cmp al, 0xFF
        jne .p1
        xor al, al
.p1:
        cmp al, 6
        jbe .p2
        mov al, 6
.p2:
        or al, F_POR
        mov [en_fmt], al
        push si
        mov bx, 100
        call divr
        pop si
.n1:
        call blancos
        test al, al
        jnz .nonum
        pop ax
        cmp byte [en_neg], 0
        je .numero
        mov si, acc
        call neg64
.numero:
        mov al, T_NUM
        call nr_cabeza
        mov si, acc
        mov di, nr + R_VAL
        call copia8
        mov cx, 14
        jmp .pon
.nonum:
        pop si
        push ds
        pop es
        mov al, [si]                ; ¿una fórmula?
        mov di, signos_f
        mov cx, 5
        repne scasb
        jne .texto
        mov al, T_FORM + PENDIENTE
        call nr_cabeza
        mov di, nr + 20             ; el texto, en mayúsculas
        xor cx, cx
.f1:
        lodsb
        call mayus
        stosb
        inc cx
        test al, al
        jnz .f1
        dec cx
        mov [nr + R_LEN], cl
        call caja_nr
        jc .r
        mov cl, [nr + R_LEN]
        xor ch, ch
        add cx, 20
        jmp short .pon
.texto0:
        pop si
.texto:
        mov al, T_TEXTO
        call nr_cabeza
        mov di, nr + 6
        xor cx, cx
.t1:
        lodsb
        stosb
        inc cx
        test al, al
        jnz .t1
        dec cx
        mov [nr + R_LEN], cl
        add cx, 6
.pon:
        mov dx, [en_f]
        mov bl, [en_c]
        mov si, nr
        call pon_celda
.hecho:
        mov ax, [en_f]
        mov [d_f1], ax
        mov [d_f2], ax
        mov al, [en_c]
        mov [d_c1], al
        mov [d_c2], al
        cmp byte [sin_calc], 0
        jne .x
        call recalcula
.x:
        clc
.r:
        ret
signos_f    db "=+-@("

; ---- la cabecera de nr: la celda en_f, en_c, el tipo AL y el formato ----
nr_cabeza:
        mov [nr + R_TIPO], al
        mov ax, [en_f]
        mov [nr], ax
        mov al, [en_c]
        mov [nr + 2], al
        mov al, [en_fmt]
        mov [nr + R_FMT], al
        mov byte [nr + R_LEN], 0
        xor ax, ax
        mov [nr + R_VAL], ax
        mov [nr + R_VAL + 2], ax
        mov [nr + R_VAL + 4], ax
        mov [nr + R_VAL + 6], ax
        push ds
        pop es
        ret

; =============================================================================
;  Editar una línea (la de la celda y las preguntas), en la fila 1: ebuf,
;  el_n letras, el cursor en el_x, delante el texto de el_pre. Vuelve con la
;  tecla que la acaba en AX: Intro, Esc, Tab, Mayús+Tab, ↑ o ↓.
; =============================================================================
edita_linea:
        mov word [el_ini], 0
        mov si, [aviso]             ; un aviso (la fórmula mal escrita), arriba
        test si, si
        jz .pinta
        mov es, [vseg]
        xor di, di
        mov ah, A_INVERSO
        call vtexto
        call vfin
.pinta:
        mov si, [el_pre]            ; lo que mide la pregunta
        xor cx, cx
.p0:
        lodsb
        test al, al
        jz .p1
        inc cx
        jmp .p0
.p1:
        mov dx, 79
        sub dx, cx                  ; DX = lo que se ve del texto
        mov ax, [el_x]
        cmp ax, [el_ini]
        jae .v1
        mov [el_ini], ax
.v1:
        mov bx, [el_ini]
        add bx, dx
        cmp ax, bx
        jb .v2
        sub ax, dx
        inc ax
        mov [el_ini], ax
.v2:
        mov es, [vseg]
        mov di, 160
        mov si, [el_pre]
        mov ah, A_BRILLO
        call vtexto
        mov si, ebuf
        add si, [el_ini]
        mov cx, [el_n]
        sub cx, [el_ini]
        cmp cx, dx
        jbe .v3
        mov cx, dx
.v3:
        mov ah, A_NORMAL
        jcxz .v5
.v4:
        lodsb
        stosw
        loop .v4
.v5:
        call vfin
        mov dl, 79                  ; el cursor de la BIOS, en su sitio
        sub dl, dh
        mov dx, [el_x]
        sub dx, [el_ini]
        mov si, [el_pre]
.v6:
        lodsb
        test al, al
        jz .v7
        inc dx
        jmp .v6
.v7:
        mov dh, 1
        call cursor_en
        xor ah, ah
        int 0x16
        mov bx, [el_x]
        cmp al, ' '
        jae .letra
        mov byte [el_nuevo], 0
.letra:
        cmp al, 0
        je .esp
        cmp al, 0xE0
        je .esp
        cmp al, 13
        je .fin
        cmp al, 27
        je .fin
        cmp al, 9
        je .fin
        cmp al, 8
        je .retro
        cmp al, ' '
        jb .pinta
        cmp byte [el_nuevo], 0      ; en una pregunta, lo primero que se
        je .l0                      ; teclea sustituye a lo propuesto
        xor bx, bx
        mov [el_n], bx
        mov [el_x], bx
.l0:
        mov byte [el_nuevo], 0
        mov cx, [el_n]              ; una letra: se mete
        cmp cx, LIN_MAX
        jae .pinta
        push ds
        pop es
        mov di, ebuf
        add di, cx
        mov si, di
        dec si
        sub cx, bx
        std
        rep movsb
        cld
        stosb
        inc word [el_n]
        inc word [el_x]
        jmp .pinta
.retro:
        test bx, bx
        jz .pinta
        dec word [el_x]
        dec bx
        jmp short .quita
.esp:
        cmp ah, 0x4B
        je .izq
        cmp ah, 0x4D
        je .der
        cmp ah, 0x47
        je .ini
        cmp ah, 0x4F
        je .end
        cmp ah, 0x53
        je .supr
        cmp ah, 0x48
        je .fin
        cmp ah, 0x50
        je .fin
        cmp ah, 0x0F
        je .fin
        jmp .pinta
.izq:
        test bx, bx
        jz .pinta
        dec word [el_x]
        jmp .pinta
.der:
        cmp bx, [el_n]
        jae .pinta
        inc word [el_x]
        jmp .pinta
.ini:
        mov word [el_x], 0
        jmp .pinta
.end:
        mov cx, [el_n]
        mov [el_x], cx
        jmp .pinta
.supr:
        cmp bx, [el_n]
        jae .pinta
.quita:
        push ds                     ; quita la letra BX
        pop es
        mov di, ebuf
        add di, bx
        lea si, [di+1]
        mov cx, [el_n]
        sub cx, bx
        rep movsb
        dec word [el_n]
        jmp .pinta
.fin:
        push ax
        mov bx, [el_n]
        mov byte [ebuf + bx], 0
        call esconde
        pop ax
        ret

; ---- una pregunta (SI) con la respuesta por defecto en ebuf: CF si Esc ----
pregunta:
        mov [el_pre], si
        mov si, ebuf
        xor cx, cx
.l:
        lodsb
        test al, al
        jz .f
        inc cx
        jmp .l
.f:
        mov [el_n], cx
        mov [el_x], cx
        mov byte [el_nuevo], 1
        call edita_linea
        cmp al, 27
        je .no
        clc
        ret
.no:
        stc
        ret

; ---- pregunta un número (SI) de 0 a BX, con AX por defecto; AX; CF si no ----
pide_num:
        mov [pn_max], bx
        push si
        push ds
        pop es
        mov di, ebuf
        call pon_dec
        mov byte [di], 0
        pop si
        call pregunta
        jc .r
        mov si, ebuf
        call blancos
        call ent_corto
        jc .r
        cmp ax, [pn_max]
        ja .mal
        clc
.r:
        ret
.mal:
        stc
        ret

; ---- una pregunta de sí o no (SI): CF si no es «S» ----
si_no:
        mov es, [vseg]
        mov di, 160
        mov ah, A_BRILLO
        call vtexto
        call vfin
        xor ah, ah
        int 0x16
        mov byte [sucio], 2
        call mayus
        cmp al, 'S'
        je .si
        stc
        ret
.si:
        clc
        ret

; ---- un menú en la fila 1: SI = las opciones (textos acabados en 0, y un
; 0 al final). AL = la elegida; CF si Esc
menu:
        mov [mn_lista], si
        mov byte [mn_sel], 0
        mov byte [sucio], 2
.pinta:
        mov es, [vseg]
        mov di, 160
        mov si, [mn_lista]
        xor cl, cl
.o:
        cmp byte [si], 0
        je .fin_o
        mov ah, A_NORMAL
        cmp cl, [mn_sel]
        jne .o1
        mov ah, A_INVERSO
.o1:
        call vtexto
        mov ax, A_NORMAL * 256 + ' '
        stosw
        stosw
        inc cl
        jmp .o
.fin_o:
        mov [mn_n], cl
        mov ah, A_NORMAL
        call vfin
        call esconde
        xor ah, ah
        int 0x16
        cmp al, 27
        je .esc
        cmp al, 13
        je .elige
        cmp ah, 0x4B
        je .izq
        cmp ah, 0x4D
        je .der
        call mayus                  ; la inicial
        mov [tecla_m], al
        mov si, [mn_lista]
        xor cl, cl
.b:
        cmp byte [si], 0
        je .pinta
        mov al, [tecla_m]
        cmp al, [si]
        je .esta
.b1:
        lodsb
        test al, al
        jnz .b1
        inc cl
        jmp .b
.esta:
        mov [mn_sel], cl
.elige:
        mov al, [mn_sel]
        clc
        ret
.esc:
        stc
        ret
.izq:
        dec byte [mn_sel]
        jns .pinta
        mov al, [mn_n]
        dec al
        mov [mn_sel], al
        jmp .pinta
.der:
        inc byte [mn_sel]
        mov al, [mn_sel]
        cmp al, [mn_n]
        jb .pinta
        mov byte [mn_sel], 0
        jmp .pinta

; =============================================================================
;  Las órdenes
; =============================================================================
c_menu:
        mov si, menu_p
        call menu
        jc .r
        cbw
        mov bx, ax
        shl bx, 1
        jmp [tabla_menu + bx]
.r:
        ret
menu_p      db "Abrir", 0, "Guardar", 0, "Nuevo", 0, "Deshacer", 0, "Formato", 0
            db "Columna", 0, "Ordenar", 0, "Imprimir", 0, "Salir", 0, 0
tabla_menu  dw c_abrir, c_guardar, c_nuevo, c_deshacer, c_formato
            dw c_columna, c_ordenar, c_imprimir, c_salir

; ---- F1: la ayuda ----
c_ayuda:
        mov es, [vseg]
        xor di, di
        mov si, t_ayuda
        mov ah, A_NORMAL
.l:
        lodsb
        test al, al
        jz .f
        cmp al, 10
        je .nl
        stosw
        jmp .l
.nl:
        mov al, ' '
        stosw
        call vfin
        jmp .l
.f:
        mov cx, 4000
        sub cx, di
        jbe .t
        shr cx, 1
        mov ax, A_NORMAL * 256 + ' '
        rep stosw
.t:
        xor ah, ah
        int 0x16
        mov byte [sucio], 2
        ret

; ---- ¿perder los cambios? CF si no ----
perder:
        cmp byte [cambios], 0
        je .si
        mov si, m_perder
        jmp si_no
.si:
        clc
        ret

c_nuevo:
        call perder
        jc .r
        call borra_hoja
        mov byte [nombre], 0
.r:
        ret

c_abrir:
        call perder
        jc .r
        mov byte [ebuf], 0
        mov si, m_abrir
        call pregunta
        jc .r
        mov si, ebuf
        call blancos
        mov di, nombre2
        call palabra
        mov word [aviso], m_no_abre
        mov dx, nombre2             ; ¿existe? (si no, la hoja se queda)
        call abre_lee
        jc .r
        call cierra
        mov si, nombre2
        mov di, nombre
        call copia_cad
        call borra_hoja
        call carga
        mov word [aviso], m_abierto
        jnc .c
        mov word [aviso], m_no_abre
.c:
        call aviso_circ
.r:
        mov byte [sucio], 2
        ret

c_guardar:
        mov si, nombre
        mov di, ebuf
        call copia_cad
        mov si, m_guardar
        call pregunta
        jc .r
        mov si, ebuf
        call blancos
        mov di, nombre
        call palabra
        call extension              ; sin extensión, la propia
        jnc .g
        mov si, ext_cta
        call copia_cad
.g:
        call guarda
        mov word [aviso], m_no_guarda
        jc .r
        mov byte [cambios], 0
        mov word [aviso], m_guardado
.r:
        mov byte [sucio], 2
        ret

c_salir:
        cmp byte [cambios], 0
        je .fuera
        mov si, m_salir
        call si_no
        jnc .guarda
        cmp al, 'N'
        jne .r
        jmp short .fuera
.guarda:
        call c_guardar
        cmp byte [cambios], 0
        jne .r
.fuera:
        mov al, [modo]              ; la pantalla (y la letra) como estaba
        xor ah, ah
        int 0x10
        mov ax, 0x4C00
        int 0x21
.r:
        ret

; ---- F5: ir a una celda ----
c_ir:
        mov byte [ebuf], 0
        mov si, m_ir
        call pregunta
        jc .r
        mov si, ebuf
.m:
        mov al, [si]
        call mayus
        mov [si], al
        inc si
        test al, al
        jnz .m
        mov si, ebuf
        call blancos
        call lee_ref
        jc .r
        mov [cf], dx
        mov [cc], bl
        jmp a_celda
.r:
        ret

; ---- F3: copiar lo marcado ----
c_copiar:
        call rango_sel
        mov si, q_f1
        mov di, k_f1
        push ds
        pop es
        movsw
        movsw
        movsw
        mov byte [hay_clip], 1
        mov word [aviso], m_copiado
        jmp a_celda

; ---- F4: pegar en el cursor (o repetido en lo marcado) ----
c_pegar:
        mov word [aviso], m_nada_clip
        cmp byte [hay_clip], 0
        je .r
        mov word [aviso], 0
        call rango_sel
        mov ax, [k_f2]              ; lo que mide lo copiado
        sub ax, [k_f1]
        mov [p_alto], ax
        mov al, [k_c2]
        sub al, [k_c1]
        mov [p_ancho], al
        cmp byte [sel_on], 0
        jne .h
        mov ax, [q_f1]              ; sin marcar: del mismo tamaño
        add ax, [p_alto]
        cmp ax, NFIL
        jb .a1
        mov ax, NFIL - 1
.a1:
        mov [q_f2], ax
        mov al, [q_c1]
        add al, [p_ancho]
        jnc .a2
        mov al, 255
.a2:
        mov [q_c2], al
.h:
        inc word [p_alto]
        inc byte [p_ancho]
        call abre_ud
        mov ax, [q_f1]              ; ¿de atrás adelante? si el destino va
        cmp ax, [k_f1]              ; detrás del origen, para no pisarlo
        jne .d1
        mov al, [q_c1]
        cmp al, [k_c1]
.d1:
        mov byte [p_atras], 0
        jbe .d2
        mov byte [p_atras], 1
.d2:
        mov dx, [q_f1]
        cmp byte [p_atras], 0
        je .f
        mov dx, [q_f2]
.f:
        mov bl, [q_c1]
        cmp byte [p_atras], 0
        je .c
        mov bl, [q_c2]
.c:
        push dx
        push bx
        mov ax, dx                  ; el origen que le toca
        sub ax, [q_f1]
        xor dx, dx
        div word [p_alto]
        add dx, [k_f1]
        mov [o_f], dx
        mov al, bl
        sub al, [q_c1]
        xor ah, ah
        div byte [p_ancho]
        add ah, [k_c1]
        mov [o_c], ah
        pop bx
        pop dx
        push dx
        push bx
        call copia_celda
        pop bx
        pop dx
        cmp byte [p_atras], 0
        jne .atras
        cmp bl, [q_c2]
        je .fs
        inc bl
        jmp .c
.fs:
        cmp dx, [q_f2]
        je .fin
        inc dx
        jmp .f
.atras:
        cmp bl, [q_c1]
        je .fa
        dec bl
        jmp .c
.fa:
        cmp dx, [q_f1]
        je .fin
        dec dx
        jmp .f
.fin:
        call recalcula_rango
        mov byte [sucio], 2
.r:
        jmp aviso_circ

; ---- recalcula lo que depende del rango q_ ----
recalcula_rango:
        mov ax, [q_f1]
        mov [d_f1], ax
        mov ax, [q_f2]
        mov [d_f2], ax
        mov al, [q_c1]
        mov [d_c1], al
        mov al, [q_c2]
        mov [d_c2], al
        jmp recalcula

; ---- copia la celda o_f, o_c a DX, BL (moviendo sus referencias) ----
copia_celda:
        mov [en_f], dx
        mov [en_c], bl
        cmp dx, [o_f]
        jne .a
        cmp bl, [o_c]
        je .r
.a:
        mov dx, [o_f]
        mov bl, [o_c]
        call busca
        mov cx, 0
        jc .pon
        call tam_rec
        push cx
        push ds
        mov si, di
        mov di, nr
        push es
        push ds
        pop es
        pop ds
        rep movsb
        pop ds
        pop cx
        mov ax, [en_f]
        mov [nr], ax
        mov al, [en_c]
        mov [nr + 2], al
        mov al, [nr + R_TIPO]
        and al, 3
        cmp al, T_FORM
        jne .pon
        mov ax, [en_f]              ; una fórmula: sus referencias, movidas
        sub ax, [o_f]
        mov [dsp_f], ax
        mov al, [en_c]
        xor ah, ah
        mov dl, [o_c]
        xor dh, dh
        sub ax, dx
        mov [dsp_c], ax
        call mueve_nr
        jc .r
        or byte [nr + R_TIPO], PENDIENTE
.pon:
        mov dx, [en_f]
        mov bl, [en_c]
        mov si, nr
        jmp pon_celda
.r:
        ret
; las referencias de la fórmula de nr, movidas dsp_f, dsp_c; CX = su tamaño
mueve_nr:
        push ds
        pop es
        mov si, nr + 20
        mov di, ftxt2
        mov cl, [nr + R_LEN]
        xor ch, ch
        rep movsb
        mov byte [di], 0
        mov si, ftxt2
        mov di, nr + 20
        call desplaza
        jc .r
        sub di, nr + 20
        mov ax, di
        mov [nr + R_LEN], al
        call caja_nr
        mov cl, [nr + R_LEN]
        xor ch, ch
        add cx, 20
        clc
.r:
        ret

; ---- Supr: borra lo marcado ----
c_borrar:
        call rango_sel
        call abre_ud
        mov dx, [q_f1]
        mov bl, [q_c1]
.l:
        call sig_en_rango
        jc .fin
        push dx
        push bx
        xor cx, cx
        call pon_celda
        pop bx
        pop dx
        jmp .l
.fin:
        call recalcula_rango
        mov byte [sucio], 2
        jmp aviso_circ

; ---- la primera celda con algo del rango q_, desde DX, BL (en orden de
; filas): DX, BL y ES:DI; CF si ya no hay ----
sig_en_rango:
.busca:
        cmp dx, [q_f2]
        ja .no
        call busca
.anda:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .no
        call del_indice
        mov dx, [es:di]
        cmp dx, [q_f2]
        ja .no
        mov bl, [es:di+2]
        cmp bl, [q_c1]
        jae .a
        mov bl, [q_c1]
        jmp .busca
.a:
        cmp bl, [q_c2]
        jbe .si
        inc dx
        mov bl, [q_c1]
        jmp .busca
.si:
        clc
        ret
.no:
        stc
        ret

; ---- el rango con algo de toda la hoja, en q_ ----
rango_usado:
        xor ax, ax
        mov [q_f1], ax
        mov [q_f2], ax
        mov [q_c1], al
        mov [q_c2], al
        mov cx, [n_cel]
        jcxz .r
        xor si, si
.l:
        push si
        call del_indice
        pop si
        mov ax, [es:di]
        mov [q_f2], ax              ; (la última es la de más abajo)
        mov al, [es:di+2]
        cmp al, [q_c2]
        jbe .s
        mov [q_c2], al
.s:
        inc si
        inc si
        loop .l
.r:
        ret

; ---- Formato ----
c_formato:
        mov si, menu_fmt
        call menu
        jc .r
        mov cl, 4
        shl al, cl
        mov [en_fmt], al
        cmp al, F_FIJO
        je .dec
        cmp al, F_POR
        jne .aplica
.dec:
        mov si, m_decimales
        mov ax, 2
        mov bx, 8
        cmp byte [en_fmt], F_POR
        jne .d1
        mov bx, 6
        xor ax, ax
.d1:
        call pide_num
        jc .r
        or [en_fmt], al
.aplica:
        call rango_sel
        call abre_ud
        mov dx, [q_f1]
.f:
        mov bl, [q_c1]
.c:
        push dx
        push bx
        mov [en_f], dx
        mov [en_c], bl
        call busca
        jc .nueva
        call tam_rec                ; la que hay, con el formato nuevo
        push ds
        mov si, di
        mov di, nr
        push es
        push ds
        pop es
        pop ds
        push cx
        rep movsb
        pop cx
        pop ds
        jmp short .pon
.nueva:
        mov al, T_TEXTO             ; la vacía: un texto vacío con formato
        call nr_cabeza
        mov cx, 6
.pon:
        mov al, [en_fmt]
        mov [nr + R_FMT], al
        mov dx, [en_f]
        mov bl, [en_c]
        mov si, nr
        call pon_celda
        pop bx
        pop dx
        jc .fin
        cmp bl, [q_c2]
        je .fs
        inc bl
        jmp .c
.fs:
        cmp dx, [q_f2]
        je .fin
        inc dx
        jmp .f
.fin:
        mov byte [sucio], 2
.r:
        ret
menu_fmt    db "General", 0, "Decimales fijos", 0, "Euros", 0, "Pesetas", 0
            db "Tanto por ciento", 0, "Fecha", 0, 0

; ---- Columna: el ancho de las columnas marcadas ----
c_columna:
        call rango_sel
        mov bl, [cc]
        xor bh, bh
        mov al, [anch + bx]
        xor ah, ah
        mov si, m_ancho
        mov bx, 75
        call pide_num
        jc .r
        test ax, ax
        jz .r
        mov bl, [q_c1]
        xor bh, bh
.l:
        mov [anch + bx], al
        cmp bl, [q_c2]
        je .f
        inc bx
        jmp .l
.f:
        mov byte [cambios], 1
.r:
        mov byte [sucio], 2
        ret

; ---- Imprimir ----
c_imprimir:
        mov si, prn
        mov di, ebuf
        call copia_cad
        mov si, m_imprimir
        call pregunta
        jc .r
        mov si, ebuf
        call blancos
        mov di, nombre2
        call palabra
        call rango_sel
        cmp byte [sel_on], 0
        jne .i
        call rango_usado
.i:
        mov dx, nombre2
        call imprime
        mov word [aviso], m_impreso
        jnc .r
        mov word [aviso], m_no_imprime
.r:
        mov byte [sucio], 2
        ret

; ---- Ordenar las filas marcadas por la columna del cursor ----
c_ordenar:
        mov si, menu_ord
        call menu
        jc .r
        mov [o_desc], al
        call rango_sel
        push ds                     ; ¿por qué columna? (la del cursor)
        pop es
        mov di, ebuf
        mov bl, [cc]
        call pon_col
        mov byte [di], 0
        mov si, m_por_col
        call pregunta
        jc .r
        mov si, ebuf
        call blancos
        mov di, ebuf2
        call palabra
        mov ax, '1'
        stosw
        mov si, ebuf2
        call lee_ref
        jc .r
        mov [o_kc], bl
        call ordena
        mov byte [sucio], 2
.r:
        ret
menu_ord    db "Ascendente (de menor a mayor)", 0, "Descendente", 0, 0

%include "cuentas/numeros.inc"
%include "cuentas/hoja.inc"
%include "cuentas/formulas.inc"
%include "cuentas/ordenar.inc"
%include "cuentas/archivos.inc"

; =============================================================================
;  Los textos
; =============================================================================
m_sin_mem   db "CUENTAS: no hay memoria bastante.", 13, 10, "$"
m_lote_mal  db "CUENTAS: no se ha podido leer o escribir el archivo.", 13, 10, "$"
m_ayuda     db " F1 Ayuda F2 Editar F3 Copiar F4 Pegar F5 Ir a F8 Marcar F9 Calcular F10 Menú", 0
m_listo     db " LISTO ", 0
m_marcar    db " MARCAR ", 0
m_cuentas   db "CUENTAS  ", 0
m_sin_nombre db "(sin nombre)", 0
m_vacio     db 0
m_mal_formula db "Fórmula mal escrita (el cursor está donde falla; para un texto, empiece con ')", 0
m_circ      db "Referencia circular en ", 0
m_llena     db "No caben más celdas: no hay memoria.", 0
m_nada_ud   db "No hay nada que deshacer.", 0
m_no_ud     db "No se puede deshacer: el cambio era demasiado grande.", 0
m_deshecho  db "Deshecho (Ctrl+Z otra vez: rehacer).", 0
m_perder    db "Hay cambios sin guardar. ¿Seguir y perderlos? (S/N)", 0
m_salir     db "Hay cambios sin guardar. ¿Guardarlos antes de salir? (S: sí, N: no, Esc: seguir)", 0
m_abrir     db "Abrir el archivo (.CTA, .CSV o .DIF): ", 0
m_guardar   db "Guardar como (.CTA, .CSV o .DIF): ", 0
m_abierto   db "Abierto.", 0
m_no_abre   db "No se ha podido abrir (¿existe?, ¿es de este formato?).", 0
m_guardado  db "Guardado.", 0
m_no_guarda db "No se ha podido guardar (¿disco lleno o protegido?).", 0
m_ir        db "Ir a la celda: ", 0
m_copiado   db "Copiado. Vaya adonde quiera la copia y pulse F4 (si marca un rango, lo llena).", 0
m_nada_clip db "Primero marque lo que quiera copiar y pulse F3.", 0
m_decimales db "Decimales: ", 0
m_por_col   db "Ordenar por la columna: ", 0
m_ancho     db "Ancho de la columna (1 a 75): ", 0
m_imprimir  db "Imprimir en (PRN, la impresora, o un archivo): ", 0
m_impreso   db "Impreso.", 0
m_no_imprime db "No se ha podido imprimir.", 0
prn         db "PRN", 0
t_eur       db "EUR", 0
t_pts       db "pts", 0
m_grande    db "No se puede ordenar tanto de una vez (no cabe en la memoria de deshacer).", 0
ext_cta     db ".CTA", 0
nom_err     dw e_error, e_div0, e_circ, e_desb, e_ref, e_valor
e_error     db "#ERROR", 0
e_div0      db "#DIV/0", 0
e_circ      db "#CIRC", 0
e_desb      db "#DESB", 0
e_ref       db "#REF", 0
e_valor     db "#VALOR", 0
nom_fmt     dw 0, f_fijo, f_euro, f_pta, f_por, f_fecha
f_fijo      db "Fijo", 0
f_euro      db "Euros", 0
f_pta       db "Pesetas", 0
f_por       db "%", 0
f_fecha     db "Fecha", 0
t_ayuda:
        db " CUENTAS, la hoja de cálculo de Hispa-8086", 10
        db 10
        db " Flechas, RePág, AvPág  moverse      Inicio  a A1      Fin  a la última celda", 10
        db " Mayús+flechas o F8     marcar un rango (Esc lo deja)", 10
        db " Escribir               un texto, un número (1.234,5  21%  24/11/2026) o una", 10
        db "                        fórmula: =A1*2  =SUMA(A1:A9)  +B2-B3  @SUM(A1...A9)", 10
        db " Intro, Tab, flechas    acaban de escribir      Esc  deja la celda como estaba", 10
        db " F2  editar la celda    Supr  borrar lo marcado    Ctrl+Z  deshacer y rehacer", 10
        db " F3  copiar lo marcado  F4  pegarlo en el cursor (o repetirlo en lo marcado)", 10
        db " F5  ir a una celda     F9  calcularlo todo    Ctrl+←, Ctrl+→  ancho de columna", 10
        db " / o F10  el menú: Abrir, Guardar, Nuevo, Deshacer, Formato, Columna, Ordenar,", 10
        db "          Imprimir y Salir (por la inicial, o con las flechas e Intro)", 10
        db 10
        db " Funciones: SUMA  MEDIA  MAX  MIN  CONTAR  SI(cond;sí;no)  REDONDEAR(x;dec)", 10
        db "            ABS  ENTERO  HOY()   (y en inglés: SUM AVG IF ROUND INT TODAY...)", 10
        db " Operadores: + - * / ^ %   y para SI: = <> < > <= >=", 10
        db " Referencias: A1 se mueve al copiar; $A$1 no; $A1 y A$1, a medias", 10
        db " Rangos: A1:B9 (o A1..B9, A1...B9)   Argumentos: ; (o , en las de @)", 10
        db " Textos: 'texto a la izquierda  ", 34, "a la derecha  ^en medio  \- repetido", 10
        db " Errores: #DIV/0 #CIRC (referencia circular) #DESB (no cabe) #REF #ERROR", 10
        db 10
        db " Archivos: .CTA (el propio), .CSV (con ;) y .DIF (VisiCalc), por la extensión", 10
        db 10
        db "                                                  Pulse una tecla para volver", 0

        absolute $
alignb 2
acc         resb 8
opb         resb 8
base        resb 8
prod        resb 16                 ; (128 bits)
resto       resb 8                  ; (justo detrás de prod)
prod2       resb 8
cifras      resb 20
s_dias      resw 2
f_ano       resw 1
f_mes       resb 1                  ; (y detrás, el día)
f_dia       resb 1
f_mes2      resb 1
signo       resb 1
nd_ent      resb 1
nd_dec      resb 1
n_red       resb 1
n_desb      resb 1
nmodo       resb 1
npunto      resb 1
n_fmt       resb 1
en_papel    resb 1
alignb 2
hp_seg      resw 1
hp_top      resw 1
hp_lim      resw 1
ix_seg      resw 1
ix_max      resw 1
n_cel       resw 1
ud_a        resw 1
ud_b        resw 1
ud_tam      resw 1
ud_fin      resw 1
c_orig      resw 1
c_dest      resw 1
c_un        resw 1
pc_f        resw 1
pc_src      resw 1
pc_tam      resw 1
pc_c        resb 1
ud_ok       resb 1
ud_hay      resb 1
ud_graba    resb 1
alignb 2
ev_sp       resw 1
ev_pos      resw 1
caja        resw 3
rr_f2       resw 1
dsp_f       resw 1
dsp_c       resw 1
dsp_ini     resw 1
d_f1        resw 1
d_f2        resw 1
d_c1        resb 1
d_c2        resb 1
r_f         resw 1
circ_f      resw 1
circ_c      resb 1
r_c         resb 1
rr_c1       resb 1
rr_c2       resb 1
ev_modo     resb 1
ev_mal      resb 1
lr_fl       resb 1
r_otra      resb 1
r_avanza    resb 1
r_quedan    resb 1
nombre_f    resb 12
alignb 2
cf          resw 1                  ; la fila del cursor
arr         resw 1                  ; la primera fila que se ve
f_ant       resw 1
ancla_f     resw 1
q_f1        resw 1                  ; un rango: el marcado, el usado...
q_f2        resw 1
q_c1        resb 1
q_c2        resb 1
k_f1        resw 1                  ; lo copiado (F3)
k_f2        resw 1
k_c1        resb 1
k_c2        resb 1
o_f         resw 1
p_alto      resw 1
vseg        resw 1
aviso       resw 1
tecla       resw 1
el_n        resw 1
el_x        resw 1
el_ini      resw 1
el_pre      resw 1
en_f        resw 1
en_ini      resw 1
mn_lista    resw 1
pf_fila     resw 1
pf_di       resw 1
cs_fila     resw 1
cs_lim      resw 1
ltx_n       resw 1
ov_n        resw 1
ov_p        resw 1
ancho_c     resw 1
pa_di       resw 1
o_c         resb 1
p_ancho     resb 1
p_atras     resb 1
o_desc      resb 1
cc          resb 1                  ; la columna del cursor
izq         resb 1                  ; la primera columna que se ve
ancla_c     resb 1
sel_on      resb 1
marcar      resb 1
mayusc      resb 1
cambios     resb 1
sucio       resb 1                  ; 2: toda la pantalla; 1: dos filas
hay_clip    resb 1
modo        resb 1
lote        resb 1
en_c        resb 1
en_fmt      resb 1
en_neg      resb 1
mn_sel      resb 1
mn_n        resb 1
tecla_m     resb 1
t_atr       resb 1
el_nuevo    resb 1
cs_c2       resb 1
anch        resb NCOL
nombre      resb 80
nombre2     resb 80
nombre3     resb 80
destino     resb 80
nr          resb 20 + 256 + 4       ; el registro que se está haciendo
ftxt        resb 256
ftxt2       resb 256
ebuf        resb 256
ebuf2       resb 256
ebuf3       resb 256
avbuf       resb 80
dtx         resb 256
ltx         resb 1024 + 80
lbuf        resb 160
iobuf       resb 512
fh          resw 1
rb_n        resw 1
rb_p        resw 1
wb_n        resw 1
en_f2       resw 1
pn_max      resw 1
o_n         resw 1
o_t         resw 1
o_k         resw 1
o_o         resw 1
o_src       resw 1
o_pos       resw 1
o_w         resw 1
o_lo        resw 1
o_mid       resw 1
o_hi        resw 1
o_pa        resw 1
o_pb        resw 1
o_la        resw 1
o_kc        resb 1
o_ta        resb 1
en_c2       resb 1
w_mal       resb 1
csv_sep     resb 1
csv_com     resb 1
csv_par     resb 1
en_texto    resb 1
sin_calc    resb 1
t_dec       resb 1
alignb 2
            resb 2048
pila_fin:
