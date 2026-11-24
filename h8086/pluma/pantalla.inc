; =============================================================================
;  PLUMA - partir el texto en líneas y pintarlo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un párrafo es lo que hay entre dos marcas de fin de párrafo (0Ah); sus
;  primeros octetos pueden ser códigos de párrafo (alineación y sangría). Las
;  líneas no se guardan en ningún sitio: se sacan cada vez, partiendo por los
;  espacios lo que no cabe en el ancho (como hacemos al escribir a mano). Lo
;  mismo sirve para la pantalla, para la vista de la página, para imprimir y
;  para exportar: lo que se ve es lo que sale.
;
;  Cada línea se compone en `fila` (80 casillas: letra y formato) y `fpos`
;  (de dónde sale cada casilla en el texto). El formato de cada casilla:
;     bits 0-2  negrita, subrayado, cursiva       bit 3  sin traducir (es
;     de los dibujos de Pluma, no del texto)      bit 4  un código, a la vista
; =============================================================================

; si SI ha llegado al hueco, al otro lado (así ninguna dirección guardada es
; la del principio del hueco: o es de antes, o es de lo de después)
%macro SALTA_HUECO 0
        cmp si, [g_ini]
        jne %%n
        mov si, [g_fin]
%%n:
%endmacro

F_CRUDO     equ 8
F_COD       equ 16

; ---- qué es cada octeto de control (0-31) ----
;   80h+bit  cambia negrita (1), subrayado (2) o cursiva (4)
;   41h centrado, 42h a la derecha, 43h justificado, 44h un paso de sangría
;   20h fin de párrafo, 21h salto de página, 22h tabulador, 23h fin del texto
;   0   (otro) no se ve ni ocupa
K_FIN       equ 0x20
K_PAG       equ 0x21
K_TAB       equ 0x22
K_NADA      equ 0x23
clase:
        db K_NADA, 0, 0x81, 0, 0, 0, 0, 0, 0, K_TAB, K_FIN, 0, K_PAG, 0, 0, 0
        db 0, 0, 0, 0x82, 0x44, 0, 0, 0, 0, 0x84, 0, 0, 0x41, 0x42, 0x43, 0
; lo que se ve de cada código con «Ver códigos» (Alt+V)
vista_cod:
        db 'N', 'S', 0, 'C'         ; (por el bit: 1, 2, 4)
        db 'M', 'D', 'J', 0xAF      ; centrado, derecha, justificado, sangría

; AL = octeto: AH = su clase (AL no cambia)
de_clase:
        mov ah, 0
        cmp al, 32
        jae .x
        push bx
        mov bx, clase
        push ax
        xlat
        mov bl, al
        pop ax
        mov ah, bl
        pop bx
.x:
        ret

; ---- el principio de un párrafo, en SI: su alineación y su sangría ----
; (se miran sus códigos de párrafo sin pasar de ellos). Deja la línea lista
; para componer: l_p, l_par, l_est = 0, l_pri = 1
ctx_parrafo:
        mov [l_p], si
        mov byte [l_par], 0
        mov byte [l_est], 0
        mov byte [l_pri], 1
        push si
.c:
        cmp si, [g_ini]
        jne .1
        mov si, [g_fin]
.1:
        lodsb
        call de_clase
        test ah, 0x40
        jz .x
        cmp ah, 0x44
        je .sang
        and byte [l_par], 0xFC      ; la alineación: la última que haya
        and ah, 3
        or [l_par], ah
        jmp .c
.sang:
        mov al, [l_par]
        cmp al, 4 * 4
        jae .c
        add byte [l_par], 4
        jmp .c
.x:
        pop si
        ret

; la columna donde empieza la línea: la sangría (5 por paso) y, en la
; primera, la de primera línea. AL
base_linea:
        mov al, [l_par]
        shr al, 1
        shr al, 1
        mov ah, 5
        mul ah
        cmp byte [l_pri], 0
        je .x
        add al, [cab+CAB_SANG1]
.x:
        mov ah, [cab+CAB_ANCHO]
        sub ah, 10
        cmp al, ah
        jbe .y
        mov al, ah
.y:
        ret

; =============================================================================
;  Medir una línea: desde l_p, hasta dónde llega. Deja:
;     m_fin     el principio de la línea siguiente
;     m_ult     1 si es la última del párrafo (o del texto)
;     m_acaba   1 si el texto se acaba en ella
;     m_wv      la columna detrás de su última letra que no es espacio
;     m_nsp     los espacios de en medio (para justificar)
;     m_tab     1 si lleva tabuladores (entonces no se justifica)
;     m_pag     1 si es un salto de página
;     m_est     el formato al final de la línea
; =============================================================================
mide:
        push si
        push di
        push bx
        call base_linea
        mov dl, al                  ; DL = la columna
        mov dh, [cab+CAB_ANCHO]     ; DH = hasta dónde
        xor ax, ax
        mov [m_ult], ax             ; (m_ult y m_acaba)
        mov [m_tab], ax             ; (m_tab y m_pag)
        mov [m_nsp], ax
        mov [m_rup], ax
        mov [m_snsp], ax
        mov byte [m_wv], dl
        mov byte [m_swv], dl
        mov byte [m_vio], 0         ; ¿ha salido ya una letra?
        mov byte [m_esp], 0         ; espacios seguidos desde la última letra
        mov bl, [l_est]
        mov [m_est], bl
        mov si, [l_p]
        SALTA_HUECO
.c:
        mov di, si
        lodsb
        SALTA_HUECO
        cmp al, 32
        jae .letra
        call de_clase
        test ah, 0x80
        jnz .cambia
        test ah, 0x40
        jnz .codigo
        cmp ah, K_FIN
        je .finp
        cmp ah, K_PAG
        je .pag
        cmp ah, K_TAB
        je .tab
        cmp ah, K_NADA
        je .acaba
        jmp .c
.cambia:
        and ah, 7
        xor [m_est], ah
.codigo:
        cmp byte [ver_cod], 0
        je .c
        jmp short .visible          ; (a la vista, ocupan una casilla)
.tab:
        mov byte [m_tab], 1
        mov al, dl                  ; a la columna siguiente que sea
        mov ah, 0                   ; múltiplo de 5
        mov bl, 5
        div bl
        sub bl, ah
        add dl, bl
        cmp dl, dh
        ja .larga
        jmp short .letra2
.letra:
        cmp al, ' '
        jne .visible
        inc dl                      ; un espacio: aquí se puede partir
        cmp byte [m_esp], 0
        jne .e1
        mov al, [m_wv]
        mov [m_swv], al
        mov ax, [m_nsp]
        mov [m_snsp], ax
.e1:
        inc byte [m_esp]
        mov [m_rup], si
        jmp .c
.visible:
        inc dl
        cmp dl, dh
        ja .larga
.letra2:
        cmp byte [m_vio], 0
        je .v1
        mov al, [m_esp]
        mov ah, 0
        add [m_nsp], ax
.v1:
        mov byte [m_vio], 1
        mov byte [m_esp], 0
        mov [m_wv], dl
        jmp .c
.larga:
        ; no cabe: se parte detrás del último espacio (o aquí, si no hay)
        mov ax, [m_rup]
        test ax, ax
        jz .corta
        mov [m_fin], ax
        mov al, [m_swv]
        mov [m_wv], al
        mov ax, [m_snsp]
        mov [m_nsp], ax
        call .est_hasta            ; (el formato, hasta donde se parte)
        jmp short .x
.corta:
        cmp di, [l_p]               ; (al menos una letra por línea)
        jne .c2
        mov di, si
.c2:
        mov [m_fin], di
        jmp short .x
.pag:
        mov byte [m_ult], 1
        cmp di, [l_p]
        jne .antes                  ; (el salto va en su propia línea)
        mov byte [m_pag], 1
        mov [m_fin], si
        jmp short .x
.antes:
        mov [m_fin], di
        jmp short .x
.finp:
        mov byte [m_ult], 1
        mov [m_fin], si
        jmp short .x
.acaba:
        mov byte [m_ult], 1
        mov byte [m_acaba], 1
        mov [m_fin], di
.x:
        pop bx
        pop di
        pop si
        ret
; (interna) el formato al final, contando solo hasta m_fin
.est_hasta:
        mov bl, [l_est]
        mov si, [l_p]
        SALTA_HUECO
.eh:
        cmp si, [m_fin]
        je .eh2
        lodsb
        SALTA_HUECO
        cmp al, 32
        jae .eh
        call de_clase
        test ah, 0x80
        jz .eh
        and ah, 7
        xor bl, ah
        jmp .eh
.eh2:
        mov [m_est], bl
        ret

; =============================================================================
;  Componer la línea medida en `fila`/`fpos`, con su alineación. Si en ella
;  está el cursor, deja su columna en c_col (y su formato en c_est)
; =============================================================================
compon:
        push si
        push di
        push bx
        mov di, fila                ; en blanco
        mov cx, 80
        mov ax, ' '
        rep stosw
        mov di, fpos
        mov cx, 80
        mov ax, 0xFFFF
        rep stosw
        mov word [k_ultp], 0
        call base_linea
        mov dl, al                  ; DL = la columna
        mov al, [cab+CAB_ANCHO]
        sub al, [m_wv]
        jae .ex
        xor al, al
.ex:
        mov [k_extra], al           ; lo que sobra a la derecha
        mov word [k_cada], 0
        mov al, [l_par]
        and al, 3
        jz .izq
        cmp al, 3
        je .just
        mov ah, [k_extra]
        cmp al, 1
        jne .der
        shr ah, 1                   ; centrado: la mitad delante
.der:
        add dl, ah
        jmp short .izq
.just:
        cmp byte [m_ult], 0         ; justificado: lo que sobra, repartido
        jne .izq                    ; entre los espacios (salvo en la última
        cmp byte [m_tab], 0         ; línea, y si hay tabuladores)
        jne .izq
        mov cx, [m_nsp]
        jcxz .izq
        mov al, [k_extra]
        mov ah, 0
        div cl
        mov [k_cada], ax            ; AL = a cada uno, AH = uno más a estos
.izq:
        mov bl, [l_est]             ; BL = el formato
        mov byte [k_vio], 0
        mov byte [k_esp], 0
        mov si, [l_p]
        SALTA_HUECO
        cmp byte [m_pag], 0
        jne .salto
.c:
        cmp si, [m_fin]
        je .fin
        cmp si, [pstart]            ; ¿el cursor?
        jne .2
        call .cursor
.2:
        mov di, si
        lodsb
        SALTA_HUECO
        cmp al, 32
        jae .letra
        call de_clase
        test ah, 0x80
        jnz .cambia
        test ah, 0x40
        jnz .codigo
        cmp ah, K_FIN
        je .finp
        cmp ah, K_TAB
        je .tab
        jmp .c
.cambia:
        and ah, 7
        xor bl, ah
.codigo:
        cmp byte [ver_cod], 0
        je .c
        push bx                     ; su letra, al revés
        mov bl, ah
        and bx, 0x47
        cmp bl, 0x40
        jb .cd1
        sub bl, 0x40 - 4
.cd1:
        mov al, [vista_cod+bx-1]
        pop bx
        mov ah, F_COD | F_CRUDO
        call .pon
        jmp .c
.tab:
        mov al, dl                  ; espacios hasta el siguiente múltiplo de 5
        mov ah, 0
        mov cl, 5
        div cl
        sub cl, ah
        mov ch, 0
        mov al, ' '
        cmp byte [ver_cod], 0
        je .t1
        mov al, 0x1A                ; (→)
.t1:
        mov ah, F_CRUDO
        or ah, bl
        call .pon
        mov al, ' '
        loop .t1
        mov byte [k_vio], 1
        jmp .c
.letra:
        mov ah, bl
        cmp al, ' '
        jne .l1
        call .pon
        cmp byte [k_vio], 0         ; los espacios de en medio, al justificar,
        je .c                       ; crecen
        inc byte [k_esp]
        mov al, [k_esp]
        mov ah, 0
        cmp ax, [m_nsp]
        ja .c
        mov cl, [k_cada]
        cmp byte [k_cada+1], 0
        je .l0
        dec byte [k_cada+1]
        inc cl
.l0:
        mov ch, 0
        jcxz .l3
.l2:
        mov al, ' '
        mov ah, bl
        call .pon
        loop .l2
.l3:
        jmp .c
.l1:
        mov byte [k_vio], 1
        call .pon
        jmp .c
.finp:
        cmp byte [ver_cod], 0       ; la marca de fin de párrafo
        je .f1
        mov al, 0x14                ; (¶, sin ocupar)
        mov ah, F_COD | F_CRUDO
        call .pon
        dec dl
.f1:
        mov byte [fila+2*79], '<'
        mov byte [fila+2*79+1], F_CRUDO
        mov [k_ultp], di
        jmp short .x
.fin:
        cmp byte [m_acaba], 0       ; el final del texto: también puede estar
        je .x                       ; ahí el cursor
        mov [k_ultp], si
        cmp si, [pstart]
        jne .x
        call .cursor
        jmp short .x
.salto:
        mov cx, 79                  ; el salto de página, de lado a lado
        mov di, fila
        mov ax, F_CRUDO * 256 + 0xC4
        rep stosw
        mov byte [fila+2*79], 'P'
        mov ax, [l_p]
        mov [fpos], ax
        mov [k_ultp], ax
        cmp ax, [pstart]
        jne .x
        mov dl, 0
        call .cursor
.x:
        mov [k_col], dl
        pop bx
        pop di
        pop si
        ret
; (interna) pon la letra AL con el formato AH en la columna DL (que avanza)
.pon:
        cmp dl, 79
        jae .p1
        push bx
        mov bl, dl
        mov bh, 0
        shl bx, 1
        mov [fila+bx], ax
        mov [fpos+bx], di
        pop bx
.p1:
        inc dl
        ret
.cursor:
        mov [c_col], dl
        mov [c_est], bl
        mov byte [c_hay], 1
        ret

; ---- una línea entera: medirla y componerla; luego, la siguiente ----
; (con l_* de la línea; al volver, l_* es la de la siguiente y CF=1 si el
; texto se ha acabado)
linea_sig:
        mov al, [m_est]
        mov [l_est], al
        mov si, [m_fin]
        cmp byte [m_acaba], 0
        jne .fin
        cmp byte [m_ult], 0
        jne .par
        mov [l_p], si
        mov byte [l_pri], 0
        clc
        ret
.par:
        call ctx_parrafo
        clc
        ret
.fin:
        stc
        ret

; ---- cuántas líneas tiene el párrafo que empieza en la posición AX ----
lineas_de:
        call dir_de
        mov si, ax
        call ctx_parrafo
        xor cx, cx
.c:
        inc cx
        push cx
        call mide
        pop cx
        cmp byte [m_ult], 0
        jne .x
        push cx
        call linea_sig
        pop cx
        jmp .c
.x:
        ret

; la posición (w) donde empieza el párrafo en el que está la posición AX
inicio_parrafo:
        push si
.c:
        test ax, ax
        jz .x
        push ax
        dec ax
        call dir_de
        mov si, ax
        mov al, [si]
        cmp al, 0x0A
        je .si
        cmp al, 0x0C
        je .si
        pop ax
        dec ax
        jmp .c
.si:
        pop ax
.x:
        pop si
        ret

; =============================================================================
;  La pantalla: fila 0 el estado, fila 1 la regla, de la 2 a la 23 el texto,
;  la 24 la ayuda (o el menú, o la pregunta)
; =============================================================================
; la primera línea de la pantalla, una línea más arriba. CF=1 si no la hay
linea_atras:
        cmp word [top_k], 0
        je .par
        dec word [top_k]
        clc
        ret
.par:
        mov ax, [top_w]
        test ax, ax
        jz .no
        dec ax
        call inicio_parrafo
        mov [top_w], ax
        call lineas_de
        dec cx
        mov [top_k], cx
        clc
        ret
.no:
        stc
        ret
; y una más abajo. CF=1 si no la hay
linea_adelante:
        mov ax, [top_w]
        call lineas_de              ; (deja medida la última del párrafo)
        mov ax, [top_k]
        inc ax
        cmp ax, cx
        jae .par
        mov [top_k], ax
        clc
        ret
.par:
        cmp byte [m_acaba], 0
        jne .no
        mov ax, [m_fin]
        call w_de
        mov [top_w], ax
        mov word [top_k], 0
        clc
        ret
.no:
        stc
        ret

; que top_w sea el principio de un párrafo de la memoria
valida_top:
        mov ax, [top_w]
        mov cx, [pre]
        add cx, [post]
        cmp ax, cx
        jbe .v
        xor ax, ax
.v:
        mov bx, ax
        call inicio_parrafo
        cmp ax, bx
        je .x
        mov word [top_k], 0
.x:
        mov [top_w], ax
        ret

; ---- componer todas las filas (y, si [con_video], pintarlas) ----
compone_pantalla:
        mov byte [intentos], 3
.otra:
        call valida_top
        call prepara_sel
        mov byte [c_hay], 0
        mov ax, [top_w]
        call dir_de
        mov si, ax
        call ctx_parrafo
        mov cx, [top_k]             ; las líneas de arriba que no se ven
        jcxz .filas
.salta:
        push cx
        call mide
        call linea_sig
        pop cx
        jc .filas                   ; (no debería)
        loop .salta
.filas:
        mov byte [fin_texto], 0
        xor bx, bx                  ; BX = la fila
.f:
        push bx
        mov di, bx                  ; lo que hace falta para rehacerla
        mov cl, 3
        shl di, cl
        add di, filas
        cmp byte [fin_texto], 0
        je .hay
        mov byte [di+7], 0          ; (vacía)
        mov di, fila
        mov cx, 80
        mov ax, ' '
        rep stosw
        jmp short .pinta
.hay:
        mov ax, [l_p]
        call w_de
        mov [di], ax
        mov al, [l_est]
        mov [di+2], al
        mov al, [l_par]
        mov [di+3], al
        mov al, [l_pri]
        mov [di+4], al
        mov byte [di+7], 1
        mov byte [c_hay], 0
        call mide
        call compon
        pop bx
        push bx
        cmp byte [c_hay], 0
        je .nc
        mov [cur_fila], bl
        mov al, [c_col]
        mov [cur_col], al
        mov al, [c_est]
        mov [cur_est], al
        mov al, [l_par]
        mov [cur_par], al
        mov byte [cur_ok], 1
.nc:
        call linea_sig
        jnc .pinta
        mov byte [fin_texto], 1
.pinta:
        pop bx
        push bx
        cmp byte [con_video], 0
        je .s
        mov ax, bx
        add ax, FILA0
        call vuelca
.s:
        pop bx
        inc bx
        cmp bx, NFILAS
        jb .f
        ; ¿se ve el cursor? Si no, se mueve la pantalla
        cmp byte [cur_ok], 0
        jne .x
        cmp byte [sin_ajuste], 0
        jne .x
        dec byte [intentos]
        jz .x
        call ajusta_top
        jmp .otra
.x:
        mov byte [filas_ok], 1
        ret

; el cursor no se ve: la línea del cursor arriba del todo, y luego unas
; cuantas atrás (si estaba debajo, para que quede abajo; si se ha saltado
; lejos, para que quede hacia la mitad)
ajusta_top:
        mov cx, NFILAS - 1
        cmp byte [centrar], 0
        je .c0
        mov cx, 8
        jmp short .c1
.c0:
        mov ax, [pre]
        cmp ax, [filas]
        jae .c1
        xor cx, cx                  ; (estaba encima: se queda arriba)
.c1:
        mov byte [centrar], 0
        push cx
        mov ax, [pre]
        call inicio_parrafo
        mov [top_w], ax
        mov word [top_k], 0
        call dir_de
        mov si, ax
        call ctx_parrafo
.b:
        call mide
        mov ax, [m_fin]             ; ¿el cursor está en esta línea?
        cmp ax, [pstart]
        ja .esta
        jne .sig
        cmp byte [m_acaba], 0
        jne .esta
.sig:
        call linea_sig
        jc .esta
        inc word [top_k]
        jmp .b
.esta:
        pop cx
        jcxz .x
.a:
        push cx
        call linea_atras
        pop cx
        jc .x
        loop .a
.x:
        ret

; la fila de `fila` a la pantalla, en la fila AX
vuelca:
        push es
        mov es, [vseg]
        mov cx, 160
        mul cx
        mov di, ax
        mov si, fila
        xor bx, bx
.c:
        lodsw                       ; AL letra, AH formato
        test ah, F_CRUDO
        jnz .crudo
        cmp al, 0x80
        jb .crudo
        push bx
        mov bx, vis - 0x80
        xlat
        pop bx
.crudo:
        push ax
        mov al, ah
        and al, 7
        push bx
        mov bx, atributos
        xlat
        pop bx
        test ah, F_COD
        jz .nc
        mov al, 0x70
.nc:
        mov dx, [fpos+bx]           ; ¿dentro del bloque marcado?
        cmp dx, 0xFFFF
        je .ns
        push ax
        mov ax, dx
        call w_de
        cmp ax, [s_ini]
        jb .ns0
        cmp ax, [s_fin]
        jae .ns0
        pop ax
        test al, 0x70
        mov al, 0x70
        jz .ns
        mov al, 0x07
        jmp short .ns
.ns0:
        pop ax
.ns:
        mov dh, al
        pop ax
        mov ah, dh
        stosw
        add bx, 2
        cmp bx, 160
        jb .c
        pop es
        ret

; lo marcado como bloque, en posiciones de la memoria: s_ini y s_fin
prepara_sel:
        xor ax, ax
        mov [s_ini], ax
        mov [s_fin], ax
        call bloque                 ; DX:AX - CX:BX, CF=1 si no hay
        jc .x
        push cx
        push bx
        call .w
        mov [s_ini], ax
        pop ax
        pop dx
        call .w
        mov [s_fin], ax
.x:
        ret
; (interna) DX:AX, como posición de la memoria (recortada a ella)
.w:
        sub ax, [lenA]
        sbb dx, [lenA+2]
        jae .w1
        xor ax, ax
        ret
.w1:
        mov cx, [pre]
        add cx, [post]
        test dx, dx
        jnz .w2
        cmp ax, cx
        jbe .w3
.w2:
        mov ax, cx
.w3:
        ret

; ---- la línea de arriba: cómo se llama el documento y cómo está todo ----
pinta_estado:
        mov di, fila
        mov cx, 80
        mov ax, F_CRUDO * 256 + ' '
        rep stosw
        mov di, fila + 2
        mov si, m_pluma
        call .txt
        mov si, nombre
        cmp byte [si], 0
        jne .n
        mov si, m_sin_nombre
.n:
        call .txt
        mov al, ' '
        cmp byte [cambiado], 0
        je .n1
        mov al, '*'
.n1:
        call .letra
        mov di, fila + 2 * 28
        mov si, m_col
        call .txt
        mov al, [cur_col]
        mov ah, 0
        inc ax
        call .num
        mov di, fila + 2 * 38       ; el formato donde está el cursor
        mov al, [cur_est]
        mov si, m_neg
        call .si_bit
        mov si, m_sub
        call .si_bit
        mov si, m_cur
        call .si_bit
        mov di, fila + 2 * 56
        mov al, [cur_par]
        and ax, 3
        mov si, m_alin
        mov cl, 4
        mul cl
        add si, ax
        mov cx, 4
.a:
        lodsb
        call .letra
        loop .a
        mov di, fila + 2 * 62
        mov si, m_ins
        cmp byte [insertar], 0
        jne .i
        mov si, m_sob
.i:
        call .txt
        cmp byte [ver_cod], 0
        je .p
        mov si, m_codigos
        call .txt
.p:
        mov di, fila + 2 * 74       ; por dónde va, en tanto por ciento
        call cursor32
        mov bx, ax
        mov cx, dx
        push cx
        push bx
        call total32
        pop bx
        pop cx
.r:
        test dx, dx                 ; los dos, a 16 bits
        jz .r1
        shr dx, 1
        rcr ax, 1
        shr cx, 1
        rcr bx, 1
        jmp .r
.r1:
        test ax, ax
        jz .r2
        xchg ax, bx                 ; BX = el total
        mov cx, 100
        mul cx
        div bx
        jmp short .r3
.r2:
        mov ax, 100
.r3:
        call .num
        mov al, '%'
        call .letra
        mov ax, 0
        jmp vuelca_inverso
; (internas)
.si_bit:
        push ax
        test al, 1
        jz .sb
        call .txt
        jmp short .sb1
.sb:
        add di, 8 * 2
.sb1:
        pop ax
        shr al, 1
        ret
.txt:
        lodsb
        test al, al
        jz .t
        call .letra
        jmp .txt
.t:
        ret
.letra:
        mov ah, F_CRUDO
        stosw
        ret
.num:
        mov bx, 10
        xor cx, cx
.n2:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .n2
.n3:
        pop ax
        add al, '0'
        call .letra
        loop .n3
        ret
; `fila`, al revés (para la línea de estado), en la fila AX
vuelca_inverso:
        push es
        mov es, [vseg]
        mov cx, 160
        mul cx
        mov di, ax
        mov si, fila
        mov cx, 80
.c:
        lodsw
        mov ah, 0x70
        stosw
        loop .c
        pop es
        ret

; ---- la regla, como la de WordStar: L sangría, ! tabuladores, R el final ----
pinta_regla:
        mov di, fila
        mov cx, 80
        mov ax, F_CRUDO * 256 + ' '
        rep stosw
        mov di, fila
        mov cl, [cab+CAB_ANCHO]
        mov ch, 0
        xor dx, dx
.c:
        mov al, '-'
        inc dx
        push dx
        mov ax, dx
        mov bl, 5
        div bl
        pop dx
        mov al, '-'
        test ah, ah
        jnz .c1
        mov al, '!'
.c1:
        mov ah, F_CRUDO
        stosw
        loop .c
        mov byte [di-2], 'R'
        mov al, [cur_par]
        mov [l_par], al
        mov byte [l_pri], 0
        call base_linea
        mov bl, al
        mov bh, 0
        shl bx, 1
        mov byte [fila+bx], 'L'
        mov ax, 1
        jmp vuelca

; ---- la línea de abajo: un texto (SI) con F_CRUDO ----
pinta_abajo:
        mov di, fila
        mov cx, 80
        mov ax, F_CRUDO * 256 + ' '
        rep stosw
        mov di, fila
.c:
        lodsb
        test al, al
        jz .x
        cmp di, fila + 160
        jae .c
        mov ah, F_CRUDO
        stosw
        jmp .c
.x:
        mov [abajo_col], di
        mov ax, 24
        jmp vuelca_inverso

; ---- todo: el texto, el estado, la regla y la ayuda; y el cursor ----
pinta:
        mov byte [con_video], 1
        mov byte [cur_ok], 0
        call compone_pantalla
        call pinta_estado
        call pinta_regla
        mov si, [mensaje]
        test si, si
        jnz .m
        mov si, m_ayuda
        call bloque
        jc .m
        mov si, m_ayuda_bloque
.m:
        call pinta_abajo
        mov dh, [cur_fila]
        add dh, FILA0
        mov dl, [cur_col]
        jmp pon_cursor

; ---- solo componer las filas, sin pintar (para moverse de fila en fila) ----
filas_al_dia:
        cmp byte [filas_ok], 0
        jne .x
        mov byte [con_video], 0
        mov byte [cur_ok], 0
        call compone_pantalla
.x:
        ret

; ---- volver a componer la fila BX (0 a NFILAS-1), para saber qué hay en
; cada columna. CF=1 si es una fila vacía ----
rehace_fila:
        mov di, bx
        mov cl, 3
        shl di, cl
        add di, filas
        cmp byte [di+7], 0
        je .no
        mov ax, [di]
        push di
        call dir_de
        pop di
        mov [l_p], ax
        mov al, [di+2]
        mov [l_est], al
        mov al, [di+3]
        mov [l_par], al
        mov al, [di+4]
        mov [l_pri], al
        call mide
        call compon
        clc
        ret
.no:
        stc
        ret
