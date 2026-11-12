; =============================================================================
;  MINAS.HVP - Campo de minas, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un campo de losas con minas escondidas. Al levantar una losa sale cuántas
;  minas tiene alrededor (si no tiene ninguna, se levantan solas las de al
;  lado); se gana al levantar todas las que no tienen mina. La primera losa
;  nunca tiene mina. Tableros: Pequeño (9x9, 10 minas), Mediano (16x16, 40),
;  Grande (30x16, 99) y Personalizado. Los mejores tiempos se guardan en
;  MINAS.REC (al lado de MINAS.HVP; texto).
;
;  Ratón: clic levanta una losa; botón derecho (o Mayús+clic) pone o quita un
;  banderín; doble clic en un número levanta las de alrededor si ya tiene
;  puestos tantos banderines como dice. Teclado: flechas, Intro o Espacio
;  levantan, B pone el banderín, F2 empieza otra partida.
;
;  Todo está hecho aquí: las losas verde oliva, los banderines ocre, las
;  minas, el rombo de arriba (añil mientras se juega, verde con una marca al
;  ganar, granate con un aspa al perder; un clic en él empieza otra partida),
;  los contadores, el icono y los textos. Es un módulo .HVP (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (las minas salen siempre en
;           los mismos sitios para la misma primera losa)
;    /P     tablero de prueba: 9x9 con dos minas, en las esquinas de arriba
;           a la izquierda y de abajo a la derecha (su tiempo se guarda
;           aparte, como «Prueba»)
; =============================================================================
%include "hvp.inc"

CEL         equ 20
MAXC        equ 30
MAXF        equ 24
PASO        equ 32                  ; una fila de las tablas
TAB_X       equ 10
TAB_Y       equ 46
CAJA_AN     equ 58                  ; los contadores de arriba
CAJA_AL     equ 26
CAJA_Y      equ 8
ROMBO_Y     equ 21
ROMBO_R     equ 13
MARCO_AL    equ 2*BORDE + TIT_H + 1 + MENU_H
E_LISTO     equ 0                   ; sin minas todavía (se ponen con la primera losa)
E_JUEGA     equ 1
E_GANADA    equ 2
E_PERDIDA   equ 3
V_TAPADA    equ 0
V_ABIERTA   equ 1
V_BANDERIN  equ 2
N_PERSONAL  equ 3
N_PRUEBA    equ 4
WM_FINAL    equ WM_USER + 100       ; (propio) decir que se ha ganado

HVP_CABECERA "Campo de minas", entrada
        db "................................"
        db ".......................0........"
        db ".......................0aaaaaa.."
        db ".......................0aaaaa..."
        db "...bbbbbbb0.66666667.bb0aaaa0..."
        db "...bbbbbbb0.66666667.bb0aabb0..."
        db "...bbbbbbb0.66666667.bb0bbbb0..."
        db "...bbbbbbb0.66666667.bb0bbbb0..."
        db "...bbbbbbb0.66666667.bb0bbbb0..."
        db "...bbbbbbb0.66666667.bb0bbbb0..."
        db "...bbbbbbb0.66666667.bbbbbbb0..."
        db "...00000000.77777777.00000000..."
        db "................................"
        db "...66666667.88888888.66666667..."
        db "...66666667.88ccc888.66666667..."
        db "...66666667.88888c88.66666667..."
        db "...66666667.88888c88.66666667..."
        db "...66666667.888cc888.66666667..."
        db "...66666667.88c88888.66666667..."
        db "...66666667.88cccc88.66666667..."
        db "...77777777.88888888.77777777..."
        db "................................"
        db "...88808888.66666667.bbbbbbb0..."
        db "...80ddd088.66666667.bbbbbbb0..."
        db "...8ddddd88.66666667.bbbbbbb0..."
        db "...0d9dddd0.66666667.bbbbbbb0..."
        db "...8ddddd88.66666667.bbbbbbb0..."
        db "...80ddd088.66666667.bbbbbbb0..."
        db "...88808888.66666667.bbbbbbb0..."
        db "...88888888.77777777.00000000..."
        db "................................"
        db "................................"
HVP_CODIGO

c_minas     dd min_proc
            db C_CREMA, 0, 0, 0
min_plant:
        PLANTILLA c_minas, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 120, 60, 204, 290, 0

min_menu:
        db "&Juego", 0
        dw 201
        db "&Nueva partida      F2", 0
        dw 0
        db 0
        dw 202
        db "&Pequeño (9 x 9, 10 minas)", 0
        dw 203
        db "&Mediano (16 x 16, 40 minas)", 0
        dw 204
        db "&Grande (30 x 16, 99 minas)", 0
        dw 205
        db "P&ersonalizado...", 0
        dw 0
        db 0
        dw 206
        db "Mejores &tiempos...", 0
        dw 0
        db 0
        dw 207
        db "&Salir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 210
        db "&Cómo se juega...", 0
        dw 0xFFFF
        db 0

; los tableros: columnas, filas y minas
niveles     dw 9, 9, 10,   16, 16, 40,   30, 16, 99
; el color de cada número (1 a 8): propio, de los fríos a los cálidos
col_num     db C_PETROLEO, C_ANIL, C_TERRACOTA, C_GRANATE, C_MARRON, C_OLIVA, C_TINTA, C_ARENA_OSC

t_titulo    db "Campo de minas", 0
t_pers_tit  db "Tablero personalizado", 0
t_pers_an   db "Casillas de ancho (8 a ", 0
t_pers_al   db "Casillas de alto (8 a ", 0
t_pers_min  db "Minas (1 a ", 0
t_pers_fin  db "):", 0
t_gana_tit  db "Campo despejado", 0
t_gana      db "¡Campo despejado en ", 0
t_gana2     db " segundos!", 0
t_gana3     db 10, "Es el mejor tiempo de este tablero.", 0
t_rec_tit   db "Mejores tiempos", 0
t_rec0      db "Pequeño (9 x 9, 10 minas):    ", 0
t_rec1      db "Mediano (16 x 16, 40 minas):  ", 0
t_rec2      db "Grande (30 x 16, 99 minas):   ", 0
t_rec_s     db " segundos", 0
t_rec_no    db "sin tiempo todavía", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Levante todas las losas que no tienen mina.", 10
            db "El número de una losa dice cuántas minas la rodean.", 10
            db 10
            db "Clic: levantar una losa (la primera nunca tiene mina).", 10
            db "Botón derecho o Mayús+clic: poner o quitar un banderín.", 10
            db "Doble clic en un número: levantar las de alrededor.", 10
            db "Teclado: flechas, Intro o Espacio, B (banderín) y F2.", 10
            db "Un clic en el rombo de arriba empieza otra partida.", 0
k_cab       db "Campo de minas - mejores tiempos, en segundos (Hispa Ventanas)", 13, 10, 0
k_rec0      db "Pequeño=", 0
k_rec1      db "Mediano=", 0
k_rec2      db "Grande=", 0
k_rec3      db "Prueba=", 0
k_tablero   db "Tablero=", 0
k_ancho     db "Ancho=", 0
k_alto      db "Alto=", 0
k_minas     db "Minas=", 0
; las claves, con dónde se guarda cada número
claves      dd k_rec0, rec, k_rec1, rec+4, k_rec2, rec+8, k_rec3, rec+12
            dd k_tablero, nivel_g, k_ancho, p_cols, k_alto, p_filas, k_minas, p_minas, 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; MINAS.REC, al lado del módulo
        mov eax, 'REC'
        call hvl_ruta
        pop edi
        mov esi, edi
        mov al, 'S'
        call hvl_opcion
        jc .reloj
        call hvl_lee_num
        jmp .sem
.reloj:
        HV tics
.sem:
        HV azar_semilla
        mov dword [p_cols], 12
        mov dword [p_filas], 12
        mov dword [p_minas], 20
        call rec_leer
        mov eax, [nivel_g]
        mov [nivel], eax
        mov esi, edi
        mov al, 'P'
        call hvl_opcion
        jc .abrir
        mov dword [nivel], N_PRUEBA
.abrir:
        mov esi, min_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
min_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_LBUTTONDOWN
        je .clic
        cmp eax, WM_LBUTTONDBLCLK
        je .clic
        cmp eax, WM_RBUTTONDOWN
        je .derecho
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .letra
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_FINAL
        je .final
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], min_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call tablero
        jmp .hecho
.destruir:
        HV msg_salir
        jmp .hecho
.reloj:
        cmp dword [estado], E_JUEGA
        jne .hecho
        cmp dword [segundos], 999
        jae .hecho
        inc dword [segundos]
        call inv_arriba
        jmp .hecho
.derecho:
        call punto_a_losa
        jc .hecho
        call banderin
        jmp .hecho
.clic:
        mov byte [cur_ve], 0
        push eax
        push ecx
        call punto_a_losa
        pop ebx                     ; EBX=mayúsculas
        pop eax
        jc .rombo
        test bl, TM_MAYUS
        jz .c1
        call banderin
        jmp .hecho
.c1:
        cmp eax, WM_LBUTTONDBLCLK
        jne .c2
        call alrededor
        jmp .hecho
.c2:
        call levantar
        jmp .hecho
.rombo:
        movsx eax, dx               ; ¿en el rombo de arriba?
        sar edx, 16
        sub eax, [cli_an2]
        jns .r1
        neg eax
.r1:
        sub edx, ROMBO_Y
        jns .r2
        neg edx
.r2:
        add eax, edx
        cmp eax, ROMBO_R + 1
        ja .hecho
        call nueva
        jmp .hecho
.letra:
        cmp ecx, ' '
        je .intro
        and ecx, 0xDF
        cmp ecx, 'B'
        jne .hecho
        mov ecx, [cur_x]
        mov edx, [cur_y]
        mov byte [cur_ve], 1
        call banderin
        jmp .hecho
.tecla:
        cmp ecx, TC_F1 + 1          ; F2
        jne .t0
        call nueva
        jmp .hecho
.t0:
        cmp ecx, TC_INTRO
        jne .t1
.intro:
        mov ecx, [cur_x]
        mov edx, [cur_y]
        mov byte [cur_ve], 1
        call levantar
        jmp .hecho
.t1:
        mov eax, [cur_x]
        mov ebx, [cur_y]
        cmp ecx, TC_DER
        jne .t2
        inc eax
.t2:
        cmp ecx, TC_IZQ
        jne .t3
        dec eax
.t3:
        cmp ecx, TC_ABAJO
        jne .t4
        inc ebx
.t4:
        cmp ecx, TC_ARRIBA
        jne .t5
        dec ebx
.t5:
        cmp eax, [cols]
        jae .hecho
        cmp ebx, [filas]
        jae .hecho
        mov ecx, [cur_x]            ; la losa de antes y la nueva, a repintar
        mov edx, [cur_y]
        call inv_losa
        mov [cur_x], eax
        mov [cur_y], ebx
        mov byte [cur_ve], 1
        mov ecx, eax
        mov edx, ebx
        call inv_losa
        jmp .hecho
.orden:
        cmp ecx, 201
        jne .o2
        call nueva
        jmp .hecho
.o2:
        cmp ecx, 205
        je .personal
        ja .o6
        sub ecx, 202                ; 202-204: los tres tableros
        jb .hecho
        mov [nivel], ecx
.cambio:
        mov eax, [nivel]
        mov [nivel_g], eax
        call rec_guardar
        call tablero
        jmp .hecho
.personal:
        call personal
        jc .hecho
        mov dword [nivel], N_PERSONAL
        jmp .cambio
.o6:
        cmp ecx, 206
        je .records
        cmp ecx, 207
        jne .o10
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.o10:
        cmp ecx, 210
        jne .hecho
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp .dlg
.records:
        mov edi, txt
        mov esi, t_rec0
        mov ebx, rec
        call .fila
        mov esi, t_rec1
        call .fila
        mov esi, t_rec2
        call .fila
        mov byte [edi-1], 0
        mov esi, txt
        mov edi, t_rec_tit
        jmp .dlg
.final:
        mov edi, txt                ; «¡Campo despejado en N segundos!»
        mov esi, t_gana
        call hvl_cad
        mov eax, [tiempo]
        call hvl_num
        mov esi, t_gana2
        call hvl_cad
        cmp byte [nuevo_rec], 0
        je .f1
        mov esi, t_gana3
        call hvl_cad
.f1:
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_gana_tit
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret
; (interna) ESI=rótulo EBX=el tiempo: una línea en EDI; EBX avanza
.fila:
        call hvl_cad
        mov eax, [ebx]
        add ebx, 4
        mov esi, t_rec_no
        test eax, eax
        jz .fi1
        call hvl_num
        mov esi, t_rec_s
.fi1:
        call hvl_cad
        mov al, 10
        stosb
        ret

; EDX=x + y*65536 (de la zona) -> ECX, EDX=la losa; CF=1 si no cae en
; ninguna (y EDX se queda como estaba)
punto_a_losa:
        push eax
        push ebx
        push edx
        movsx eax, dx
        sar edx, 16
        sub eax, TAB_X
        js .no
        sub edx, TAB_Y
        js .no
        mov ebx, edx
        xor edx, edx
        mov ecx, CEL
        div ecx
        xchg eax, ebx               ; EBX=columna
        xor edx, edx
        div ecx                     ; EAX=fila
        cmp ebx, [cols]
        jae .no
        cmp eax, [filas]
        jae .no
        mov ecx, ebx
        mov edx, eax
        add esp, 4
        pop ebx
        pop eax
        clc
        ret
.no:
        pop edx
        pop ebx
        pop eax
        stc
        ret

; ---- el tablero según el nivel: medidas, ventana y partida nueva ----
tablero:
        pushad
        mov ebp, [ven]
        mov eax, [nivel]
        cmp eax, N_PERSONAL
        je .pers
        ja .prueba
        lea eax, [eax+eax*2]
        movzx ecx, word [niveles+eax*2]
        movzx edx, word [niveles+eax*2+2]
        movzx ebx, word [niveles+eax*2+4]
        jmp .pon
.pers:
        mov ecx, [p_cols]
        mov edx, [p_filas]
        mov ebx, [p_minas]
        jmp .pon
.prueba:
        mov ecx, 9
        mov edx, 9
        mov ebx, 2
.pon:
        cmp ecx, 8                  ; (por si MINAS.REC trae algo raro)
        jae .p0
        mov ecx, 8
.p0:
        cmp edx, 8
        jae .p00
        mov edx, 8
.p00:
        test ebx, ebx
        jnz .p01
        inc ebx
.p01:
        call limites                ; (que quepa en la pantalla)
        cmp ecx, [max_c]
        jbe .p1
        mov ecx, [max_c]
.p1:
        cmp edx, [max_f]
        jbe .p2
        mov edx, [max_f]
.p2:
        mov [cols], ecx
        mov [filas], edx
        mov eax, ecx
        imul eax, edx
        sub eax, 10
        cmp ebx, eax
        jbe .p3
        mov ebx, eax
.p3:
        mov [nminas], ebx
        imul ecx, CEL               ; la ventana, a la medida del tablero
        add ecx, 2*TAB_X
        mov eax, ecx
        shr eax, 1
        mov [cli_an2], eax
        mov [cli_an], ecx
        add ecx, 2*BORDE
        imul edx, CEL
        add edx, TAB_Y + TAB_X + MARCO_AL
        HV pantalla                 ; EAX=ancho EBX=alto
        sub eax, ecx
        sub ebx, edx
        cmp eax, [ebp+V_X]
        jle .x1
        mov eax, [ebp+V_X]
.x1:
        test eax, eax
        jns .x2
        xor eax, eax
.x2:
        cmp ebx, [ebp+V_Y]
        jle .y1
        mov ebx, [ebp+V_Y]
.y1:
        cmp ebx, BARRA_H
        jge .y2
        mov ebx, BARRA_H
.y2:
        HV ven_mover
        popad
        ; (sigue: partida nueva)

nueva:
        pushad
        mov ebp, [ven]
        mov edi, minas
        mov ecx, 3 * PASO * MAXF
        xor al, al
        rep stosb
        xor eax, eax
        mov [estado], eax
        mov [abiertas], eax
        mov [banderines], eax
        mov [segundos], eax
        mov [nuevo_rec], al
        mov [cur_ve], al
        mov eax, [cols]
        shr eax, 1
        mov [cur_x], eax
        mov eax, [filas]
        shr eax, 1
        mov [cur_y], eax
        mov dword [explota], -1
        mov ecx, 1
        HV tmp_quitar
        HV ven_invalidar
        popad
        ret

; -> max_c, max_f: las casillas que caben en la pantalla
limites:
        pushad
        HV pantalla
        sub eax, 2*BORDE + 2*TAB_X
        xor edx, edx
        mov ecx, CEL
        div ecx
        cmp eax, MAXC
        jbe .a
        mov eax, MAXC
.a:
        mov [max_c], eax
        lea eax, [ebx - BARRA_H - MARCO_AL - TAB_Y - TAB_X]
        xor edx, edx
        div ecx
        cmp eax, MAXF
        jbe .b
        mov eax, MAXF
.b:
        mov [max_f], eax
        popad
        ret

; ---- el tablero personalizado: tres preguntas -> CF=1 si se cancela ----
personal:
        pushad
        call limites
        mov esi, t_pers_an
        mov eax, [max_c]
        mov ebx, [p_cols]
        mov edx, 8
        call .pide
        jc .x
        mov [p_cols], eax
        mov esi, t_pers_al
        mov eax, [max_f]
        mov ebx, [p_filas]
        mov edx, 8
        call .pide
        jc .x
        mov [p_filas], eax
        imul eax, [p_cols]
        sub eax, 10
        mov esi, t_pers_min
        mov ebx, [p_minas]
        mov edx, 1
        call .pide
        jc .x
        mov [p_minas], eax
        clc
.x:
        popad
        ret
; (interna) ESI=pregunta EAX=máximo EDX=mínimo EBX=valor de ahora -> EAX
.pide:
        push ecx
        push edx
        push eax
        mov edi, txt
        call hvl_cad
        call hvl_num
        mov esi, t_pers_fin
        call hvl_cad
        mov byte [edi], 0
        mov edi, num
        mov eax, ebx
        call hvl_num
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_pers_tit
        mov ebx, num
        mov ecx, 6
        HV dlg_pedir
        cmp eax, 1
        jne .no
        mov esi, num
        call hvl_lee_num
        pop edx                     ; entre el mínimo y el máximo
        cmp eax, edx
        jbe .p1
        mov eax, edx
.p1:
        pop edx
        cmp eax, edx
        jae .p2
        mov eax, edx
.p2:
        pop ecx
        clc
        ret
.no:
        pop eax
        pop edx
        pop ecx
        stc
        ret

; ---- las losas ----
; ECX=columna EDX=fila -> EDI=su sitio en las tablas
sitio:
        mov edi, edx
        shl edi, 5
        add edi, ecx
        ret

; la losa ECX, EDX hay que repintarla
inv_losa:
        pushad
        imul eax, ecx, CEL
        add eax, TAB_X
        imul ebx, edx, CEL
        add ebx, TAB_Y
        mov ecx, CEL
        mov edx, CEL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; los contadores y el rombo de arriba, a repintar
inv_arriba:
        pushad
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, TAB_Y - 4
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; llama a ESI con ECX, EDX = cada una de las losas que rodean a ECX, EDX
cada_vecina:
        pushad
        mov eax, ecx
        mov ebx, edx
        mov edx, -1
.f:
        mov ecx, -1
.c:
        mov edi, ecx
        or edi, edx
        jz .s                       ; (ella misma)
        push ecx
        push edx
        add ecx, eax
        add edx, ebx
        cmp ecx, [cols]
        jae .n
        cmp edx, [filas]
        jae .n
        call esi
.n:
        pop edx
        pop ecx
.s:
        inc ecx
        cmp ecx, 1
        jle .c
        inc edx
        cmp edx, 1
        jle .f
        popad
        ret

; (para cada_vecina) cuenta en «cuenta» las minas / los banderines
cuenta_mina:
        call sitio
        movzx edi, byte [minas+edi]
        add [cuenta], edi
        ret
cuenta_band:
        call sitio
        cmp byte [vista+edi], V_BANDERIN
        jne .x
        inc dword [cuenta]
.x:
        ret

; pone las minas: nunca en la losa ECX, EDX (la primera) ni, si caben, en
; las que la rodean
poner_minas:
        pushad
        cmp dword [nivel], N_PRUEBA
        jne .azar
        mov byte [minas], 1         ; el tablero de prueba: dos esquinas
        mov eax, [filas]
        dec eax
        shl eax, 5
        add eax, [cols]
        mov byte [minas+eax-1], 1
        jmp .cerca
.azar:
        mov eax, [cols]
        imul eax, [filas]
        sub eax, 9
        xor ebp, ebp                ; EBP=1: se respetan también las de alrededor
        cmp eax, [nminas]
        setae al
        movzx ebp, al
        mov esi, ecx
        mov edi, edx
        mov ebx, [nminas]
.m:
        test ebx, ebx
        jz .cerca
        mov eax, [cols]
        HV azar
        mov ecx, eax
        mov eax, [filas]
        HV azar
        mov edx, eax
        mov eax, ecx                ; ¿demasiado cerca de la primera?
        sub eax, esi
        jns .a1
        neg eax
.a1:
        push edx
        sub edx, edi
        jns .a2
        neg edx
.a2:
        cmp eax, edx
        jae .a3
        mov eax, edx
.a3:
        pop edx
        cmp eax, ebp
        jbe .m
        push edi
        call sitio
        mov eax, edi
        pop edi
        cmp byte [minas+eax], 0
        jne .m
        mov byte [minas+eax], 1
        dec ebx
        jmp .m
.cerca:
        xor edx, edx                ; cuántas minas rodean a cada losa
.f:
        xor ecx, ecx
.c:
        mov dword [cuenta], 0
        mov esi, cuenta_mina
        call cada_vecina
        call sitio
        mov al, [cuenta]
        mov [cerca+edi], al
        inc ecx
        cmp ecx, [cols]
        jb .c
        inc edx
        cmp edx, [filas]
        jb .f
        popad
        ret

; levanta la losa ECX, EDX (y, si no tiene minas cerca, las de alrededor)
levantar:
        pushad
        cmp dword [estado], E_JUEGA
        ja .x
        call sitio
        cmp byte [vista+edi], V_TAPADA
        jne .x
        cmp dword [estado], E_LISTO
        jne .l1
        call poner_minas
        mov dword [estado], E_JUEGA
        push ecx
        push edx
        mov ebp, [ven]
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
        pop edx
        pop ecx
        call inv_arriba
.l1:
        cmp byte [minas+edi], 0
        je .l2
        mov [explota], edi          ; una mina: se acabó
        mov dword [estado], E_PERDIDA
        jmp .fin
.l2:
        mov dword [pila_n], 0
        call abrir
.l3:
        mov eax, [pila_n]
        test eax, eax
        jz .l4
        dec eax
        mov [pila_n], eax
        movzx ecx, byte [pila+eax*2]
        movzx edx, byte [pila+eax*2+1]
        call sitio
        cmp byte [cerca+edi], 0
        jne .l3
        mov esi, abrir
        call cada_vecina
        jmp .l3
.l4:
        mov eax, [cols]             ; ¿están todas las que no tienen mina?
        imul eax, [filas]
        sub eax, [nminas]
        cmp eax, [abiertas]
        jne .x
        mov dword [estado], E_GANADA
        call apuntar
        mov ebp, [ven]
        mov eax, WM_FINAL
        HV msg_poner
.fin:
        mov ebp, [ven]
        mov ecx, 1
        HV tmp_quitar
        HV ven_invalidar
.x:
        popad
        ret

; (interna) abre la losa ECX, EDX si está tapada y la apunta en la pila
abrir:
        push edi
        push eax
        call sitio
        cmp byte [vista+edi], V_TAPADA
        jne .x
        cmp byte [minas+edi], 0
        jne .x
        mov byte [vista+edi], V_ABIERTA
        inc dword [abiertas]
        call inv_losa
        mov eax, [pila_n]
        mov [pila+eax*2], cl
        mov [pila+eax*2+1], dl
        inc dword [pila_n]
.x:
        pop eax
        pop edi
        ret

; doble clic en un número: si tiene tantos banderines como dice, levanta
; las demás losas de alrededor
alrededor:
        pushad
        call sitio
        cmp byte [vista+edi], V_ABIERTA
        jne .x
        mov dword [cuenta], 0
        mov esi, cuenta_band
        call cada_vecina
        movzx eax, byte [cerca+edi]
        test eax, eax
        jz .x
        cmp eax, [cuenta]
        jne .x
        mov esi, levantar
        call cada_vecina
.x:
        popad
        ret

; pone o quita el banderín de la losa ECX, EDX
banderin:
        pushad
        cmp dword [estado], E_JUEGA
        ja .x
        call sitio
        mov al, [vista+edi]
        cmp al, V_ABIERTA
        je .x
        xor al, V_BANDERIN
        mov [vista+edi], al
        shr al, 1                   ; 1 si se ha puesto
        movzx eax, al
        lea eax, [eax*2-1]
        add [banderines], eax
        call inv_losa
        call inv_arriba
.x:
        popad
        ret

; ---- los mejores tiempos ----
; se ha ganado: apunta el tiempo si es el mejor de su tablero
apuntar:
        pushad
        mov eax, [segundos]
        test eax, eax
        jnz .a
        inc eax
.a:
        mov [tiempo], eax
        mov ebx, [nivel]
        cmp ebx, N_PERSONAL
        je .x                       ; (el personalizado no tiene)
        jb .b
        dec ebx                     ; «Prueba» es el cuarto
.b:
        mov edx, [rec+ebx*4]
        test edx, edx
        jz .mejor
        cmp eax, edx
        jae .x
.mejor:
        mov [rec+ebx*4], eax
        mov byte [nuevo_rec], 1
        call rec_guardar
.x:
        popad
        ret

rec_guardar:
        pushad
        mov edi, txt
        mov esi, k_cab
        call hvl_cad
        mov ebx, claves
.b:
        mov esi, [ebx]
        test esi, esi
        jz .f
        call hvl_cad
        mov eax, [ebx+4]
        mov eax, [eax]
        call hvl_num
        mov ax, 0x0A0D
        stosw
        add ebx, 8
        jmp .b
.f:
        mov ecx, edi
        mov edx, txt
        sub ecx, edx
        mov esi, ruta_rec
        call hvl_escribir
        popad
        ret

rec_leer:
        pushad
        mov esi, ruta_rec
        mov edi, txt
        mov ecx, 500
        call hvl_leer
        test eax, eax
        jz .x
        mov ebx, claves
.b:
        mov edi, [ebx]
        test edi, edi
        jz .f
        mov esi, txt
        call hvl_clave
        jc .s
        call hvl_lee_num
        mov edx, [ebx+4]
        mov [edx], eax
.s:
        add ebx, 8
        jmp .b
.f:
        cmp dword [nivel_g], N_PERSONAL
        jbe .x
        mov dword [nivel_g], 0
.x:
        popad
        ret

; ---- el dibujo ----
; (interna) de puntos a casillas: ECX=primer punto EDX=último+1 (ya sin el
; origen del tablero) ESI=casillas que hay -> ECX=primera EDX=última
rango:
        push eax
        push ebx
        mov ebx, CEL
        mov eax, ecx
        xor ecx, ecx
        test eax, eax
        js .a
        push edx
        xor edx, edx
        div ebx
        pop edx
        mov ecx, eax
.a:
        mov eax, edx
        dec eax
        mov edx, -1
        js .b
        xor edx, edx
        div ebx
        mov edx, eax
        cmp edx, esi
        jl .b
        lea edx, [esi-1]
.b:
        pop ebx
        pop eax
        ret

pintar:
        HV ven_cli                  ; solo las losas de la zona por repintar
        mov ecx, [ebp+V_UX0]
        mov edx, [ebp+V_UX1]
        sub ecx, eax
        sub edx, eax
        sub ecx, TAB_X
        sub edx, TAB_X
        mov esi, [cols]
        call rango
        mov [p_c0], ecx
        mov [p_c1], edx
        mov ecx, [ebp+V_UY0]
        mov edx, [ebp+V_UY1]
        sub ecx, ebx
        sub edx, ebx
        sub ecx, TAB_Y
        sub edx, TAB_Y
        mov esi, [filas]
        call rango
        mov [p_f0], ecx
        mov [p_f1], edx
        HV dc_pintar
        ; ---- arriba: banderines que faltan, el rombo y el tiempo ----
        mov eax, TAB_X
        call .caja
        COLOR C_TINTA               ; un banderín pequeño
        mov eax, TAB_X + 9
        mov ebx, CAJA_Y + 5
        mov ecx, 1
        mov edx, 15
        HV dc_rect
        COLOR C_OCRE
        inc eax
        mov edx, 1
        mov ecx, 8
.b1:
        HV dc_rect
        inc ebx
        sub ecx, 1
        cmp ecx, 1
        ja .b1
        mov eax, [nminas]
        sub eax, [banderines]
        cmp dword [estado], E_GANADA
        jne .b2
        xor eax, eax
.b2:
        mov edi, rot
        test eax, eax
        jns .b3
        neg eax
        mov byte [edi], '-'
        inc edi
.b3:
        call hvl_num
        mov byte [edi], 0
        mov eax, TAB_X + 24
        call .numero
        mov eax, [cli_an]
        sub eax, TAB_X + CAJA_AN
        call .caja
        push eax
        COLOR C_ANIL                ; un reloj pequeño
        add eax, 6
        mov ebx, CAJA_Y + 6
        mov ecx, 13
        mov edx, 13
        HV dc_marco
        add eax, 6
        add ebx, 3
        mov ecx, 1
        mov edx, 4
        HV dc_rect
        add ebx, 3
        mov ecx, 4
        mov edx, 1
        HV dc_rect
        mov edi, rot
        mov eax, [segundos]
        call hvl_num
        mov byte [edi], 0
        pop eax
        add eax, 24
        call .numero
        ; el rombo
        mov esi, C_ANIL
        mov eax, [estado]
        cmp eax, E_GANADA
        jb .r1
        mov esi, C_OLIVA
        je .r1
        mov esi, C_GRANATE
.r1:
        COLOR C_MARRON
        mov eax, [cli_an2]
        mov ebx, ROMBO_Y
        mov ecx, ROMBO_R
        HV dc_rombo
        COLOR esi
        mov ecx, ROMBO_R - 2
        HV dc_rombo
        mov edx, [estado]
        cmp edx, E_GANADA
        je .r_gana
        ja .r_pierde
        COLOR C_OCRE                ; jugando: un rombo ocre dentro
        mov ecx, 4
        HV dc_rombo
        jmp .tab
.r_gana:
        COLOR C_BLANCO              ; ganada: una marca
        lea ecx, [eax-1]
        lea edx, [ebx+4]
        sub eax, 5
        HV dc_linea
        inc ebx
        inc edx
        HV dc_linea
        mov eax, ecx
        mov ebx, edx
        add ecx, 6
        sub edx, 9
        HV dc_linea
        dec ebx
        dec edx
        HV dc_linea
        jmp .tab
.r_pierde:
        COLOR C_TINTA               ; perdida: un aspa
        lea ecx, [eax+4]
        lea edx, [ebx+4]
        sub eax, 4
        sub ebx, 4
        HV dc_linea
        inc eax
        inc ecx
        HV dc_linea
        dec eax
        mov ecx, eax
        add eax, 8
        HV dc_linea
        inc eax
        inc ecx
        HV dc_linea
.tab:
        ; ---- el tablero ----
        COLOR C_MARRON
        mov eax, TAB_X - 2
        mov ebx, TAB_Y - 2
        mov ecx, [cols]
        imul ecx, CEL
        add ecx, 3
        mov edx, [filas]
        imul edx, CEL
        add edx, 3
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        mov edx, [p_f0]
.f:
        cmp edx, [p_f1]
        jg .x
        mov ecx, [p_c0]
.c:
        cmp ecx, [p_c1]
        jg .cf
        call losa
        inc ecx
        jmp .c
.cf:
        inc edx
        jmp .f
.x:
        HV dc_cerrar
        xor eax, eax
        ret
; (interna) la caja de un contador, en EAX
.caja:
        COLOR C_BLANCO
        mov ebx, CAJA_Y
        mov ecx, CAJA_AN
        mov edx, CAJA_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        HV dc_marco
        ret
; (interna) rot en negrita, en EAX
.numero:
        COLOR C_TINTA
        NEGRITA 1
        mov ebx, CAJA_Y + 5
        mov esi, rot
        HV dc_texto
        NEGRITA 0
        ret

; dibuja la losa ECX (columna), EDX (fila)
losa:
        pushad
        imul eax, ecx, CEL
        add eax, TAB_X
        imul ebx, edx, CEL
        add ebx, TAB_Y
        call sitio
        mov ebp, edi                ; EBP=su sitio en las tablas
        mov cl, [vista+ebp]         ; CL=cómo está CH=tiene mina
        mov ch, [minas+ebp]
        mov esi, [estado]
        cmp esi, E_GANADA
        jb .ver
        je .gana
        test ch, ch                 ; perdida: se ven las minas
        jz .ver
        cmp cl, V_BANDERIN
        je .ver
        mov cl, V_ABIERTA
        jmp .ver
.gana:
        test ch, ch                 ; ganada: un banderín en cada mina
        jz .ver
        mov cl, V_BANDERIN
.ver:
        COLOR C_MARRON              ; la rejilla: una raya a la derecha y abajo
        push ecx
        mov ecx, CEL
        mov edx, CEL
        HV dc_rect
        pop ecx
        cmp cl, V_ABIERTA
        je .abierta
        COLOR C_OLIVA               ; tapada: una losa verde con su canto
        push ecx
        mov ecx, CEL - 1
        mov edx, CEL - 1
        HV dc_rect
        COLOR C_TURQUESA
        mov edx, 1
        HV dc_rect
        mov ecx, 1
        mov edx, CEL - 1
        HV dc_rect
        pop ecx
        cmp cl, V_BANDERIN
        jne .cursor
        COLOR C_TINTA               ; el banderín
        push eax
        push ebx
        add eax, 6
        add ebx, 3
        mov ecx, 1
        mov edx, 13
        HV dc_rect
        sub eax, 2
        add ebx, 13
        mov ecx, 6
        mov edx, 1
        HV dc_rect
        COLOR C_OCRE
        add eax, 3
        sub ebx, 13
        mov ecx, 8
.b:
        HV dc_rect
        inc ebx
        dec ecx
        cmp ecx, 1
        ja .b
        pop ebx
        pop eax
        cmp esi, E_PERDIDA          ; perdida: el banderín sin mina, tachado
        jne .cursor
        test ch, ch
        jnz .cursor
        COLOR C_GRANATE
        push eax
        push ebx
        add eax, 3
        add ebx, 3
        lea ecx, [eax+12]
        lea edx, [ebx+12]
        HV dc_linea
        inc eax
        inc ecx
        HV dc_linea
        dec eax
        mov ecx, eax
        add eax, 12
        HV dc_linea
        inc eax
        inc ecx
        HV dc_linea
        pop ebx
        pop eax
        jmp .cursor
.abierta:
        mov edi, C_CREMA            ; levantada: crema (la que explotó, terracota)
        cmp ebp, [explota]
        jne .a1
        mov edi, C_TERRACOTA_CL
.a1:
        COLOR edi
        push ecx
        mov ecx, CEL - 1
        mov edx, CEL - 1
        HV dc_rect
        pop ecx
        test ch, ch
        jnz .mina
        movzx edi, byte [cerca+ebp]
        test edi, edi
        jz .cursor
        movzx ecx, byte [col_num+edi-1]
        COLOR ecx
        add edi, '0'
        mov [digito], edi
        NEGRITA 1
        push eax
        push ebx
        add eax, 5
        add ebx, 2
        mov esi, digito
        HV dc_texto
        pop ebx
        pop eax
        NEGRITA 0
        jmp .cursor
.mina:
        COLOR C_TINTA               ; la mina: un rombo con púas
        push eax
        push ebx
        add eax, 9
        add ebx, 9
        mov ecx, 6
        HV dc_rombo
        sub eax, 5
        sub ebx, 5
        mov ecx, 2
        mov edx, 2
        HV dc_rect
        add eax, 9
        HV dc_rect
        add ebx, 9
        HV dc_rect
        sub eax, 9
        HV dc_rect
        COLOR C_GRANATE
        add eax, 5
        sub ebx, 4
        mov ecx, 3
        HV dc_rombo
        COLOR C_BLANCO
        dec eax
        dec ebx
        HV dc_punto
        pop ebx
        pop eax
.cursor:
        cmp byte [cur_ve], 0        ; el recuadro del teclado
        je .x
        mov ecx, [esp+24]
        cmp ecx, [cur_x]
        jne .x
        mov edx, [esp+20]
        cmp edx, [cur_y]
        jne .x
        COLOR C_ANIL
        mov ecx, CEL - 1
        mov edx, CEL - 1
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.x:
        popad
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
nivel       resd 1                  ; 0-2 los tres tableros, 3 personalizado, 4 prueba
nivel_g     resd 1                  ; el que se guarda en MINAS.REC
cols        resd 1
filas       resd 1
nminas      resd 1
p_cols      resd 1                  ; el tablero personalizado
p_filas     resd 1
p_minas     resd 1
max_c       resd 1
max_f       resd 1
cli_an      resd 1                  ; ancho de la zona de la ventana y su mitad
cli_an2     resd 1
abiertas    resd 1
banderines  resd 1
segundos    resd 1
tiempo      resd 1
explota     resd 1                  ; la mina que se ha levantado (-1: ninguna)
cur_x       resd 1                  ; la losa del teclado
cur_y       resd 1
cuenta      resd 1
pila_n      resd 1
p_c0        resd 1
p_c1        resd 1
p_f0        resd 1
p_f1        resd 1
digito      resd 1
rec         resd 4                  ; pequeño, mediano, grande y prueba (0: no hay)
nuevo_rec   resb 1
cur_ve      resb 1
num         resb 8
ruta_rec    resb 80
rot         resb 32                 ; el rótulo que se está pintando
txt         resb 512
minas       resb PASO * MAXF        ; 1: hay mina
vista       resb PASO * MAXF        ; tapada, abierta o con banderín
cerca       resb PASO * MAXF        ; minas alrededor
pila        resw PASO * MAXF
HVP_FIN
