; =============================================================================
;  SOLITARI.HVP - Solitario, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un solitario de columnas (de la familia del «Klondike», de dominio
;  público) con la baraja española de 40 cartas: oros, copas, espadas y
;  bastos; as, 2 a 7, sota (10), caballo (11) y rey (12).
;
;  Se reparten seis columnas de 1 a 6 cartas (21), tapadas menos la de
;  encima; las otras 19 son el mazo, arriba a la izquierda. Un clic en el
;  mazo saca una carta al descarte (a su derecha); cuando se acaba, otro clic
;  le da la vuelta al descarte, las veces que haga falta. En las columnas se
;  baja de una en una (rey, caballo, sota, 7, 6... as) alternando palos
;  claros (oros y copas, con el número en terracota) y oscuros (espadas y
;  bastos, en tinta); se puede llevar una carta destapada con todas las que
;  tiene encima. A una columna vacía solo va un rey. La carta tapada que
;  queda encima de una columna se destapa sola. Arriba a la derecha, los
;  cuatro montones de salida, uno por palo: se sube del as al rey. Se gana
;  con las 40 cartas arriba.
;
;  Ratón: arrastrar las cartas; doble clic (o botón derecho) en una carta la
;  sube a su montón si puede. Teclado: F2 partida nueva, Retroceso o Ctrl+Z
;  deshacer (todos los pasos), Espacio sacar carta del mazo, S subir todas
;  las que se puedan. El tiempo empieza con el primer movimiento. Los
;  récords (partidas jugadas y ganadas, mejor tiempo y menos movimientos) se
;  guardan en SOLITARI.REC (al lado de SOLITARI.HVP; texto).
;
;  Todo está hecho aquí: las cartas (blancas, con el número en la esquina,
;  las pintas de cada palo y las tres figuras), el dorso añil con rombos, el
;  tapete azul petróleo, el icono y los textos. Es un módulo .HVP
;  (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (siempre el mismo reparto)
;    /P     partida de prueba, casi acabada: faltan por subir el rey de oros
;           (en el mazo), el caballo y el rey de copas (en la segunda
;           columna, el rey tapado) y el rey de espadas (en la primera)
; =============================================================================
%include "hvp.inc"

CAR_AN      equ 60                  ; una carta
CAR_AL      equ 80
GAP         equ 14                  ; entre cartas
PASO        equ CAR_AN + GAP
FILA_Y      equ 28                  ; mazo, descarte y montones de salida
COL_Y       equ FILA_Y + CAR_AL + 12 ; las columnas
D_TAP       equ 6                   ; lo que asoma una carta tapada
D_VIS       equ 16                  ; y una destapada
CLI_AN      equ 6 * PASO + GAP
CLI_AL      equ COL_Y + 5 * D_TAP + 9 * D_VIS + CAR_AL + 8
PILA_T      equ 48                  ; una pila: cuántas y hasta 40 cartas
P_MAZO      equ 0
P_DESC      equ 1
P_SAL       equ 2                   ; 2-5: oros, copas, espadas, bastos
P_COL       equ 6                   ; 6-11: las columnas
TAPADA      equ 0x40                ; la carta es palo*10+orden (0-39) + esto
FIG_T       equ 32 * 44             ; una figura ampliada
DES_MAX     equ 512                 ; pasos que se pueden deshacer
E_LISTO     equ 0                   ; repartida, sin tocar (el reloj, parado)
E_JUEGA     equ 1
E_GANADA    equ 2
WM_FINAL    equ WM_USER + 100       ; (propio) decir que se ha ganado

HVP_CABECERA "Solitario", entrada
        db "................................"
        db "................................"
        db "....000000000000000............."
        db "...0ccccccccccccccc0............"
        db "...0ccccccccccccccc0............"
        db "...0ccacccacccacccc0............"
        db "...0ccc2ccc2ccc2ccc0............"
        db "...0ccccccc.0000000000000000...."
        db "...0ccccccc099999999999999990..."
        db "...0ccaccca094999999999999990..."
        db "...0ccc2ccc044999999999999990..."
        db "...0ccccccc094999999999999990..."
        db "...0ccccccc094999999999999990..."
        db "...0ccaccca099999999999999990..."
        db "...0ccc2ccc099999911119999990..."
        db "...0ccccccc0999911aaaa1199990..."
        db "...0ccccccc099911aaaaaa119990..."
        db "...0ccaccca09991aaaaaaaa19990..."
        db "...0ccc2ccc0991aaa1111aaa1990..."
        db "...0ccccccc0991aaa1991aaa1990..."
        db "...0ccccccc0991aaa1991aaa1990..."
        db "...0ccaccca0991aaa1111aaa1990..."
        db "...0ccc2ccc09991aaaaaaaa19990..."
        db "...0ccccccc099911aaaaaa119990..."
        db "....00000000999911aaaa1199990..."
        db "...........099999911119999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "...........099999999999999990..."
        db "............0000000000000000...."
        db "................................"
HVP_CODIGO

c_sol       dd sol_proc
            db C_PETROLEO, 0, 0, 0
sol_plant:
        PLANTILLA c_sol, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 90, 40, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

sol_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida    F2", 0
        dw 102
        db "&Deshacer    Retroceso", 0
        dw 103
        db "S&ubir las que se puedan    S", 0
        dw 0
        db 0
        dw 107
        db "&Récords...", 0
        dw 0
        db 0
        dw 108
        db "&Salir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 110
        db "&Cómo se juega...", 0
        dw 0xFFFF
        db 0

; ---- los dibujos: «.» transparente, 0-9 a-f colores, x/y los del palo ----
; los cuatro palos (13x13): oros, copas, espadas, bastos
d_sim:
        db "....11111...."
        db "..11aaaaa11.."
        db ".1aaaaaaaaa1."
        db ".1aaa111aaa1."
        db "1aaa1aaa1aaa1"
        db "1aa1aa9aa1aa1"
        db "1aa1a9a9a1aa1"
        db "1aa1aa9aa1aa1"
        db "1aaa1aaa1aaa1"
        db ".1aaa111aaa1."
        db ".1aaaaaaaaa1."
        db "..11aaaaa11.."
        db "....11111...."

        db ".ddddddddddd."
        db ".d444444444d."
        db ".d459444444d."
        db ".d459444444d."
        db ".d454444444d."
        db "..d4444444d.."
        db "...d44444d..."
        db "....ddddd...."
        db ".....d4d....."
        db ".....d4d....."
        db "....dd4dd...."
        db "..dd44444dd.."
        db "..ddddddddd.."

        db "......2......"
        db ".....222....."
        db "....22e22...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2e9e2...."
        db "....2eee2...."
        db ".ccccccccccc."
        db ".caacccccaac."
        db ".ccccccccccc."
        db ".....1a1....."
        db ".....111....."

        db "....11111...."
        db "...1bbbbb1..."
        db "..1bbbbbbb1.."
        db "..1bb1bbbb1.."
        db "..1bbbbbbb1.."
        db "..1bbbb1bb1.."
        db "...1bbbbb1..."
        db "...1bbbbb1..."
        db "....1bbb1...."
        db "....1bbb1...."
        db "....1b1b1...."
        db ".....1b1....."
        db ".....111....."
%if ($ - d_sim) != 4 * 169
%error "los palos no miden 13x13"
%endif

; las tres figuras (16x22): sota, caballo y rey
d_fig:
        db "................"
        db ".....yyyyyy....."
        db "....yyyyyyyy...."
        db ".....666666....."
        db ".....606606....."
        db ".....666666....."
        db "......6666......"
        db "....xxxxxxxx...."
        db "...xxxxxxxxxx..."
        db "..xxxxyxxyxxxx.."
        db "..xx.xxxxxx.xx.."
        db "..66.xxyyxx.66.."
        db ".....xxxxxx....."
        db ".....yyyyyy....."
        db ".....xxxxxx....."
        db "....xxxxxxxx...."
        db "....xxxxxxxx...."
        db ".....11..11....."
        db ".....11..11....."
        db ".....11..11....."
        db "....111..111...."
        db "................"

        db "................"
        db ".........1.1...."
        db "........11111..."
        db ".......1xxxxx1.."
        db "......1xxxxxxx1."
        db ".....1xxx0xxxx1."
        db "....1xxxxxxxxx1."
        db "...1xxxxxxxxxx1."
        db "..1xxxxxxxyxxx1."
        db "..1xxxx1xxyyxx1."
        db "..1x0x1.1xyyxx1."
        db "...111..1xxyyx1."
        db ".......1xxxyyx1."
        db "......1xxxxxyx1."
        db ".....1xxxxxxxx1."
        db "....1xxxxxxxxx1."
        db "....1xxxxxxxxx1."
        db "...1xxxxxxxxxx1."
        db "...111111111111."
        db "..yyyyyyyyyyyyy."
        db "..yyyyyyyyyyyyy."
        db "................"

        db "................"
        db "..a...a..a...a.."
        db "..aa.aaaaaa.aa.."
        db "..aaaaaaaaaaaa.."
        db "..a4aaa44aaa4a.."
        db "..aaaaaaaaaaaa.."
        db "...6666666666..."
        db "...6606666066..."
        db "...6666666666..."
        db "...1166666611..."
        db "...1111111111..."
        db "....11111111...."
        db ".....111111....."
        db "..xxxxx11xxxxx.."
        db ".xxxxxxxxxxxxxx."
        db ".xxyxxxxxxxxyxx."
        db ".xxyxxxyyxxxyxx."
        db ".xxyxxyyyyxxyxx."
        db ".xxyxxxyyxxxyxx."
        db ".xxyxxxxxxxxyxx."
        db ".xxxxxxxxxxxxxx."
        db "................"
%if ($ - d_fig) != 3 * 352
%error "las figuras no miden 16x22"
%endif
; los dos colores de las figuras de cada palo
fig_col     db C_OCRE, C_MARRON,   C_TERRACOTA, C_GRANATE
            db C_PETROLEO, C_TURQUESA,   C_OLIVA, C_ARENA_OSC

; dónde van las pintas del as al 7 (cuántas; x, y de cada una en la carta)
t_pintas    dd pin1, pin2, pin3, pin4, pin5, pin6, pin7
pin1        db 1
pin2        db 2,  23, 26,  23, 57
pin3        db 3,  23, 22,  23, 42,  23, 62
pin4        db 4,  10, 26,  37, 26,  10, 57,  37, 57
pin5        db 5,  10, 24,  37, 24,  23, 42,  10, 60,  37, 60
pin6        db 6,  10, 22,  37, 22,  10, 42,  37, 42,  10, 62,  37, 62
pin7        db 7,  10, 22,  37, 22,  23, 32,  10, 42,  37, 42,  10, 62,  37, 62
; el número de la esquina (tres octetos cada uno)
t_num       db "1", 0, 0, "2", 0, 0, "3", 0, 0, "4", 0, 0, "5", 0, 0
            db "6", 0, 0, "7", 0, 0, "10", 0, "11", 0, "12", 0
; los rombos del dorso: x, y, radio, color
dorso       db 17, 18, 9, C_PETROLEO,   43, 18, 9, C_PETROLEO
            db 17, 62, 9, C_PETROLEO,   43, 62, 9, C_PETROLEO
            db 30, 40, 13, C_PETROLEO_CL,   30, 40, 8, C_OCRE,   30, 40, 3, C_ANIL
            db 0xFF
; la partida de prueba: pila, cuántas y sus cartas
d_prueba    db P_SAL, 9,      0, 1, 2, 3, 4, 5, 6, 7, 8
            db P_SAL + 1, 8,  10, 11, 12, 13, 14, 15, 16, 17
            db P_SAL + 2, 9,  20, 21, 22, 23, 24, 25, 26, 27, 28
            db P_SAL + 3, 10, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39
            db P_COL, 1,      29
            db P_COL + 1, 2,  19 | TAPADA, 18
            db P_MAZO, 1,     9
            db 0xFF

t_titulo    db "Solitario", 0
t_movs      db "Movimientos: ", 0
t_tiempo    db "Tiempo: ", 0
t_mazo      db "Mazo: ", 0
t_gana_tit  db "Solitario resuelto", 0
t_gana      db "¡Las cuarenta arriba!", 10, "Tiempo: ", 0
t_gana2     db "    Movimientos: ", 0
t_gana3     db 10, "Es el mejor tiempo.", 0
t_gana4     db 10, "Nunca se había hecho con menos movimientos.", 0
t_rec_tit   db "Récords", 0
t_rec_jug   db "Partidas jugadas:  ", 0
t_rec_gan   db "Partidas ganadas:  ", 0
t_rec_tie   db "Mejor tiempo:  ", 0
t_rec_mov   db "Menos movimientos:  ", 0
t_rec_no    db "sin apuntar", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Baraja española de 40 cartas. Hay que subir los cuatro", 10
            db "palos, del as al rey, a los montones de arriba.", 10
            db 10
            db "En las columnas se baja de una en una (rey, caballo,", 10
            db "sota, 7... as) alternando palos claros (oros y copas)", 10
            db "y oscuros (espadas y bastos). A una columna vacía solo", 10
            db "va un rey. El mazo se puede pasar las veces que se quiera.", 10
            db 10
            db "Clic en el mazo: saca carta. Las cartas se arrastran;", 10
            db "doble clic o botón derecho: la carta sube a su montón.", 10
            db "F2: nueva.  Retroceso: deshacer.  Espacio: sacar carta.", 0
k_cab       db "Solitario - récords (Hispa Ventanas)", 13, 10, 0
k_jug       db "Jugadas=", 0
k_gan       db "Ganadas=", 0
k_tie       db "Tiempo=", 0
k_mov       db "Movimientos=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; SOLITARI.REC, al lado del módulo
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
        mov esi, edi
        mov al, 'P'
        call hvl_opcion
        setnc byte [a_prueba]
        call preparar
        call rec_leer
        mov esi, sol_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
sol_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONDOWN
        je .aprieta
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
        cmp eax, WM_RBUTTONDOWN
        je .doble
        cmp eax, WM_TIMER
        je .reloj
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
        mov dword [ebp+V_MENU], sol_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call nueva
        jmp .hecho
.destruir:
        HV msg_salir
        jmp .hecho
.reloj:
        cmp dword [estado], E_JUEGA
        jne .hecho
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .hecho                  ; minimizada, el tiempo no corre
        cmp dword [segundos], 5999
        jae .hecho
        inc dword [segundos]
        call inv_arriba
        jmp .hecho
.aprieta:
        cmp dword [estado], E_GANADA
        je .hecho
        cmp byte [arr_on], 0
        jne .hecho
        call donde                  ; EAX=pila ECX=carta
        test eax, eax
        js .hecho
        jnz .a1
        call pasar                  ; el mazo
        jmp .hecho
.a1:
        test ecx, ecx
        js .hecho
        imul esi, eax, PILA_T
        add esi, pilas
        test byte [esi+1+ecx], TAPADA
        jnz .hecho
        mov [arr_pila], eax         ; se lleva esa carta y las de encima
        mov [arr_desde], ecx
        movzx ebx, byte [esi]
        sub ebx, ecx
        mov [arr_n], ebx
        mov eax, [g_x]
        mov ebx, [g_y]
        mov [arr_x], eax
        mov [arr_y], ebx
        movsx ecx, dx
        sar edx, 16
        sub ecx, eax
        sub edx, ebx
        mov [arr_dx], ecx
        mov [arr_dy], edx
        mov byte [arr_on], 1
        HV ven_capturar
        jmp .hecho
.mueve:
        cmp byte [arr_on], 0
        je .hecho
        call inv_arr
        movsx eax, dx
        sar edx, 16
        sub eax, [arr_dx]
        sub edx, [arr_dy]
        mov [arr_x], eax
        mov [arr_y], edx
        call inv_arr
        jmp .hecho
.suelta:
        cmp byte [arr_on], 0
        je .hecho
        mov byte [arr_on], 0
        HV ven_soltar
        HV ven_invalidar
        call soltar
        jmp .hecho
.doble:
        cmp dword [estado], E_GANADA
        je .hecho
        cmp byte [arr_on], 0
        jne .hecho
        call donde
        cmp eax, P_DESC
        jl .hecho
        je .d1
        cmp eax, P_COL
        jb .hecho
.d1:
        imul esi, eax, PILA_T       ; solo la de encima
        movzx ebx, byte [pilas+esi]
        dec ebx
        cmp ebx, ecx
        jne .hecho
        call subir
        jmp .hecho
.letra:
        cmp ecx, ' '
        jne .l1
        call pasar
        jmp .hecho
.l1:
        and ecx, 0xDF
        cmp ecx, 'S'
        jne .hecho
        call subir_todas
        jmp .hecho
.tecla:
        cmp ecx, TC_F1 + 1          ; F2
        jne .t1
        call nueva
        jmp .hecho
.t1:
        cmp ecx, TC_RETRO
        je .t2
        cmp ecx, TC_CTRL + 26       ; Ctrl+Z
        jne .hecho
.t2:
        call deshacer
        jmp .hecho
.orden:
        cmp ecx, 101
        jne .o2
        call nueva
        jmp .hecho
.o2:
        cmp ecx, 102
        je .t2
        cmp ecx, 103
        jne .o7
        call subir_todas
        jmp .hecho
.o7:
        cmp ecx, 107
        je .records
        cmp ecx, 108
        jne .o10
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.o10:
        cmp ecx, 110
        jne .hecho
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp .dlg
.records:
        mov edi, txt
        mov esi, t_rec_jug
        call hvl_cad
        mov eax, [rec_jug]
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_rec_gan
        call hvl_cad
        mov eax, [rec_gan]
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_rec_tie
        call hvl_cad
        mov eax, [rec_tiempo]
        test eax, eax
        jz .r1
        call pon_tiempo
        jmp .r2
.r1:
        mov esi, t_rec_no
        call hvl_cad
.r2:
        mov al, 10
        stosb
        mov esi, t_rec_mov
        call hvl_cad
        mov eax, [rec_movs]
        test eax, eax
        jz .r3
        call hvl_num
        jmp .r4
.r3:
        mov esi, t_rec_no
        call hvl_cad
.r4:
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_rec_tit
        jmp .dlg
.final:
        mov edi, txt                ; «¡Las cuarenta arriba! Tiempo...»
        mov esi, t_gana
        call hvl_cad
        mov eax, [segundos]
        call pon_tiempo
        mov esi, t_gana2
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        test byte [nuevo_rec], 1
        jz .f1
        mov esi, t_gana3
        call hvl_cad
.f1:
        test byte [nuevo_rec], 2
        jz .f2
        mov esi, t_gana4
        call hvl_cad
.f2:
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_gana_tit
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret

; EAX=segundos: «M:SS» en EDI, que avanza
pon_tiempo:
        push eax
        push ecx
        push edx
        xor edx, edx
        mov ecx, 60
        div ecx
        call hvl_num
        mov al, ':'
        stosb
        mov eax, edx
        cmp eax, 10
        jae .d
        mov byte [edi], '0'
        inc edi
.d:
        call hvl_num
        pop edx
        pop ecx
        pop eax
        ret

; los rótulos de arriba hay que repintarlos
inv_arriba:
        pushad
        mov ebp, [ven]
        xor eax, eax
        xor ebx, ebx
        mov ecx, CLI_AN
        mov edx, FILA_Y - 4
        HV ven_invalidar_rect
        popad
        ret

; las cartas que se llevan con el ratón hay que repintarlas
inv_arr:
        pushad
        mov ebp, [ven]
        mov eax, [arr_x]
        mov ebx, [arr_y]
        mov ecx, CAR_AN
        mov edx, [arr_n]
        dec edx
        imul edx, D_VIS
        add edx, CAR_AL
        HV ven_invalidar_rect
        popad
        ret

; EAX=pila -> EAX=x EBX=y de su sitio
pila_pos:
        mov ebx, FILA_Y
        cmp eax, P_COL
        jb .a
        sub eax, P_COL
        mov ebx, COL_Y
.a:
        imul eax, PASO
        add eax, GAP
        ret

; ESI=una columna ECX=carta -> EBX=lo que baja desde COL_Y
carta_y:
        push edx
        xor ebx, ebx
        xor edx, edx
.b:
        cmp edx, ecx
        jae .x
        add ebx, D_VIS
        test byte [esi+1+edx], TAPADA
        jz .s
        sub ebx, D_VIS - D_TAP
.s:
        inc edx
        jmp .b
.x:
        pop edx
        ret

; EDX=x + y*65536 (de la zona) -> EAX=pila (-1: ninguna) ECX=carta (-1: no
; hay; en las de arriba, la de encima); g_x, g_y = dónde está esa carta
donde:
        push ebx
        push edx
        push esi
        push edi
        movsx eax, dx
        sar edx, 16                 ; EAX=x EDX=y
        sub eax, GAP
        js .no
        push edx
        xor edx, edx
        mov ecx, PASO
        div ecx
        mov ebx, edx
        pop edx
        cmp ebx, CAR_AN             ; (entre dos cartas)
        jae .no
        cmp eax, 5
        ja .no
        imul ebx, eax, PASO
        add ebx, GAP
        mov [g_x], ebx
        cmp edx, FILA_Y
        jl .no
        cmp edx, FILA_Y + CAR_AL
        jge .cols
        mov dword [g_y], FILA_Y
        imul esi, eax, PILA_T
        movzx ecx, byte [pilas+esi]
        dec ecx
        jmp .x
.cols:
        sub edx, COL_Y
        js .no
        add eax, P_COL
        imul esi, eax, PILA_T
        add esi, pilas
        mov dword [g_y], COL_Y
        movzx ecx, byte [esi]
        dec ecx
        js .x                       ; vacía
        call carta_y                ; la de encima, entera
        lea edi, [ebx+CAR_AL]
        cmp edx, edi
        jge .no
.b:
        cmp edx, ebx
        jge .esa
        dec ecx
        call carta_y
        jmp .b
.esa:
        add ebx, COL_Y
        mov [g_y], ebx
        jmp .x
.no:
        mov eax, -1
        mov ecx, -1
.x:
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

; AL=carta -> EBX=su montón de salida; CF=0 si le toca subir
puede_sal:
        push eax
        push ecx
        push edx
        movzx eax, al
        and eax, 0x3F
        xor edx, edx
        mov ecx, 10
        div ecx                     ; EAX=palo EDX=orden
        lea ebx, [eax+P_SAL]
        imul ecx, ebx, PILA_T
        movzx ecx, byte [pilas+ecx]
        cmp ecx, edx
        jne .no
        clc
        jmp .x
.no:
        stc
.x:
        pop edx
        pop ecx
        pop eax
        ret

; AL=carta EBX=columna (su pila) -> CF=0 si se puede poner encima
puede_col:
        pushad
        movzx eax, al
        and eax, 0x3F
        xor edx, edx
        mov ecx, 10
        div ecx                     ; EAX=palo EDX=orden
        imul esi, ebx, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jnz .hay
        cmp edx, 9                  ; a un hueco, solo un rey
        jne .no
        jmp .si
.hay:
        movzx ebx, byte [esi+ecx]   ; la de encima
        test bl, TAPADA
        jnz .no
        push eax
        push edx
        mov eax, ebx
        xor edx, edx
        mov ecx, 10
        div ecx
        mov ebx, eax                ; EBX=su palo ECX=su orden
        mov ecx, edx
        pop edx
        pop eax
        inc edx                     ; una menos
        cmp edx, ecx
        jne .no
        xor eax, ebx                ; y un palo claro sobre uno oscuro o al revés
        test al, 2
        jz .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; EAX=de qué pila EBX=a cuál ECX=cuántas (las de encima, en su orden)
mover:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        imul edi, ebx, PILA_T
        add edi, pilas
        movzx eax, byte [esi]
        sub eax, ecx
        mov [esi], al
        lea esi, [esi+1+eax]
        movzx eax, byte [edi]
        add [edi], cl
        lea edi, [edi+1+eax]
        rep movsb
        popad
        ret

; EAX=de qué pila EBX=a cuál: todas, dándoles la vuelta (mazo y descarte)
volcar:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        imul edi, ebx, PILA_T
        add edi, pilas
.b:
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .x
        mov al, [esi+ecx]
        dec byte [esi]
        movzx edx, byte [edi]
        mov [edi+1+edx], al
        inc byte [edi]
        jmp .b
.x:
        popad
        ret

; un movimiento: EAX=de qué pila EBX=a cuál ECX=cuántas. Destapa la carta que
; queda encima, lo apunta para deshacer y mira si se ha ganado
hacer_mov:
        pushad
        call mover
        mov edx, ecx
        imul esi, eax, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .a
        test byte [esi+ecx], TAPADA
        jz .a
        and byte [esi+ecx], ~TAPADA & 0xFF
        or dl, 0x80                 ; (al deshacer se vuelve a tapar)
.a:
        shl edx, 8                  ; el apunte: 1, de, a, cuántas
        or dl, bl
        shl edx, 8
        or dl, al
        shl edx, 8
        or dl, 1
        call des_apuntar
        call tras_mov
        popad
        ret

; EDX=un paso, a la lista de deshacer (si está llena, se pierde el más viejo)
des_apuntar:
        push eax
        push ecx
        push esi
        push edi
        mov eax, [des_n]
        cmp eax, DES_MAX
        jb .p
        mov esi, des + 4
        mov edi, des
        mov ecx, DES_MAX - 1
        rep movsd
        dec eax
.p:
        mov [des+eax*4], edx
        inc eax
        mov [des_n], eax
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; después de un movimiento: la cuenta, el reloj y el final
tras_mov:
        pushad
        mov ebp, [ven]
        inc dword [movs]
        cmp dword [estado], E_LISTO
        jne .j
        mov dword [estado], E_JUEGA ; el primero: empieza el tiempo
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
        inc dword [rec_jug]
        call rec_guardar
.j:
        mov al, [pilas+P_SAL*PILA_T]
        add al, [pilas+(P_SAL+1)*PILA_T]
        add al, [pilas+(P_SAL+2)*PILA_T]
        add al, [pilas+(P_SAL+3)*PILA_T]
        cmp al, 40
        jne .inv
        mov dword [estado], E_GANADA
        mov ecx, 1
        HV tmp_quitar
        inc dword [rec_gan]
        mov byte [nuevo_rec], 0
        mov eax, [segundos]
        test eax, eax
        jnz .t0
        inc eax
        mov [segundos], eax
.t0:
        cmp dword [rec_tiempo], 0
        je .t1
        cmp eax, [rec_tiempo]
        jae .m0
.t1:
        mov [rec_tiempo], eax
        or byte [nuevo_rec], 1
.m0:
        mov eax, [movs]
        cmp dword [rec_movs], 0
        je .m1
        cmp eax, [rec_movs]
        jae .g
.m1:
        mov [rec_movs], eax
        or byte [nuevo_rec], 2
.g:
        call rec_guardar
        mov eax, WM_FINAL           ; el aviso, cuando se haya pintado
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
.inv:
        HV ven_invalidar
        popad
        ret

; deshace el último paso
deshacer:
        pushad
        cmp dword [estado], E_GANADA
        je .x
        cmp byte [arr_on], 0
        jne .x
        mov eax, [des_n]
        test eax, eax
        jz .x
        dec eax
        mov [des_n], eax
        mov edx, [des+eax*4]
        cmp dl, 3
        jne .m
        mov eax, P_MAZO             ; la vuelta del descarte
        mov ebx, P_DESC
        call volcar
        jmp .f
.m:
        movzx eax, dh               ; de dónde salieron
        shr edx, 16
        movzx ebx, dl               ; dónde están
        movzx ecx, dh               ; cuántas (y si se destapó una)
        test cl, 0x80
        jz .m1
        and ecx, 0x7F
        imul esi, eax, PILA_T
        movzx edi, byte [pilas+esi]
        or byte [pilas+esi+edi], TAPADA
.m1:
        xchg eax, ebx
        call mover
.f:
        dec dword [movs]
        mov ebp, [ven]
        HV ven_invalidar
.x:
        popad
        ret

; clic en el mazo: una carta al descarte o, si no quedan, el descarte al mazo
pasar:
        pushad
        cmp dword [estado], E_GANADA
        je .x
        cmp byte [arr_on], 0
        jne .x
        cmp byte [pilas+P_MAZO*PILA_T], 0
        je .vuelta
        mov eax, P_MAZO
        mov ebx, P_DESC
        mov ecx, 1
        call hacer_mov
        jmp .x
.vuelta:
        cmp byte [pilas+P_DESC*PILA_T], 0
        je .x
        mov eax, P_DESC
        mov ebx, P_MAZO
        call volcar
        mov edx, 3
        call des_apuntar
        call tras_mov
.x:
        popad
        ret

; EAX=pila: su carta de encima sube a su montón, si le toca; CF=1 si no
subir:
        pushad
        imul esi, eax, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        test ecx, ecx
        jz .no
        mov dl, [esi+ecx]
        push eax
        mov al, dl
        call puede_sal              ; EBX=el montón
        pop eax
        jc .no
        mov ecx, 1
        call hacer_mov
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; sube todas las que se puedan, del descarte y de las columnas
subir_todas:
        pushad
        cmp byte [arr_on], 0
        jne .x
.otra:
        cmp dword [estado], E_GANADA
        je .x
        xor edi, edi
        mov eax, P_DESC
        call subir
        jc .c
        inc edi
.c:
        mov eax, P_COL
.c1:
        cmp dword [estado], E_GANADA
        je .x
        call subir
        jc .c2
        inc edi
.c2:
        inc eax
        cmp eax, P_COL + 6
        jb .c1
        test edi, edi
        jnz .otra
.x:
        popad
        ret

; se sueltan las cartas que se llevaban: a la columna o al montón donde caen
soltar:
        pushad
        mov eax, [arr_x]
        add eax, CAR_AN / 2 - GAP / 2
        js .x
        xor edx, edx
        mov ecx, PASO
        div ecx
        cmp eax, 5
        ja .x
        mov edi, eax                ; EDI=sobre qué columna (de 0 a 5)
        imul esi, [arr_pila], PILA_T
        add esi, pilas
        mov ecx, [arr_desde]
        mov al, [esi+1+ecx]         ; la de abajo de las que se llevan
        mov edx, [arr_y]
        add edx, CAR_AL / 2
        cmp edx, COL_Y - 6
        jge .col
        cmp edi, 2                  ; arriba: a su montón de salida
        jb .x
        cmp dword [arr_n], 1
        jne .x
        call puede_sal
        jc .x
        jmp .mov
.col:
        lea ebx, [edi+P_COL]
        cmp ebx, [arr_pila]
        je .x
        call puede_col
        jc .x
.mov:
        mov eax, [arr_pila]
        mov ecx, [arr_n]
        call hacer_mov
.x:
        popad
        ret

; ---- partida nueva ----
nueva:
        pushad
        mov ebp, [ven]
        mov edi, pilas
        mov ecx, 12 * PILA_T
        xor al, al
        rep stosb
        xor eax, eax
        mov [movs], eax
        mov [segundos], eax
        mov [des_n], eax
        mov [arr_on], al
        mov [estado], eax
        mov ecx, 1
        HV tmp_quitar
        cmp byte [a_prueba], 0
        jne .prueba
        xor ecx, ecx                ; se baraja
.i:
        mov [baraja+ecx], cl
        inc ecx
        cmp ecx, 40
        jb .i
        mov ecx, 39
.s:
        lea eax, [ecx+1]
        HV azar
        mov dl, [baraja+ecx]
        xchg dl, [baraja+eax]
        mov [baraja+ecx], dl
        dec ecx
        jnz .s
        mov esi, baraja             ; y se reparte: 1, 2... 6 cartas
        xor ebx, ebx
.c:
        imul edi, ebx, PILA_T
        add edi, pilas + P_COL * PILA_T
        lea ecx, [ebx+1]
        mov [edi], cl
        inc edi
.c1:
        lodsb
        cmp ecx, 1
        je .c2
        or al, TAPADA
.c2:
        stosb
        dec ecx
        jnz .c1
        inc ebx
        cmp ebx, 6
        jb .c
        mov edi, pilas + P_MAZO * PILA_T
        mov byte [edi], 19
        inc edi
        mov ecx, 19
        rep movsb
        jmp .f
.prueba:
        mov esi, d_prueba
.p:
        lodsb
        cmp al, 0xFF
        je .f
        movzx edi, al
        imul edi, PILA_T
        add edi, pilas
        lodsb
        movzx ecx, al
        stosb
        rep movsb
        jmp .p
.f:
        HV ven_invalidar
        popad
        ret

; ---- los récords ----
rec_guardar:
        pushad
        mov edi, txt
        mov esi, k_cab
        call hvl_cad
        mov esi, k_jug
        mov eax, [rec_jug]
        call .fila
        mov esi, k_gan
        mov eax, [rec_gan]
        call .fila
        mov esi, k_tie
        mov eax, [rec_tiempo]
        call .fila
        mov esi, k_mov
        mov eax, [rec_movs]
        call .fila
        mov ecx, edi
        mov edx, txt
        sub ecx, edx
        mov esi, ruta_rec
        call hvl_escribir
        popad
        ret
.fila:
        call hvl_cad
        call hvl_num
        mov ax, 0x0A0D
        stosw
        ret

rec_leer:
        pushad
        mov esi, ruta_rec
        mov edi, txt
        mov ecx, 300
        call hvl_leer
        test eax, eax
        jz .x
        mov edi, k_jug
        mov ebx, rec_jug
        call .fila
        mov edi, k_gan
        mov ebx, rec_gan
        call .fila
        mov edi, k_tie
        mov ebx, rec_tiempo
        call .fila
        mov edi, k_mov
        mov ebx, rec_movs
        call .fila
.x:
        popad
        ret
.fila:
        mov esi, txt
        call hvl_clave
        jc .f1
        call hvl_lee_num
        mov [ebx], eax
.f1:
        ret

; ---- los dibujos, de texto a mapas de bits (una vez, al empezar) ----
preparar:
        pushad
        mov esi, d_sim              ; los palos, y cada uno al doble
        mov edi, sim13
        mov ecx, 4 * 169
        xor edx, edx
        call convertir
        mov esi, sim13
        mov edi, sim26
        mov edx, 4
.s:
        mov ebx, 13
        mov ecx, 13
        call ampliar
        add esi, 169
        add edi, 676
        dec edx
        jnz .s
        mov edi, figs               ; las figuras, con los colores de cada palo
        xor ebp, ebp                ; EBP=palo
.p:
        mov esi, d_fig
.f:
        push edi
        mov edi, fig_tmp
        mov ecx, 352
        mov dx, [fig_col+ebp*2]
        call convertir              ; (ESI avanza)
        pop edi
        push esi
        mov esi, fig_tmp
        mov ebx, 16
        mov ecx, 22
        call ampliar
        pop esi
        add edi, FIG_T
        cmp esi, d_fig + 3 * 352
        jb .f
        inc ebp
        cmp ebp, 4
        jb .p
        popad
        ret

; ESI=dibujo en texto EDI=mapa ECX=puntos DL, DH=los colores «x» e «y»
; (ESI avanza)
convertir:
        push eax
        push ecx
        push edi
.b:
        lodsb
        cmp al, '.'
        jne .c1
        mov al, CLAVE
        jmp .p
.c1:
        cmp al, 'x'
        jne .c2
        mov al, dl
        jmp .p
.c2:
        cmp al, 'y'
        jne .c3
        mov al, dh
        jmp .p
.c3:
        cmp al, 'a'
        jb .c4
        sub al, 'a' - 10
        jmp .p
.c4:
        sub al, '0'
.p:
        stosb
        dec ecx
        jnz .b
        pop edi
        pop ecx
        pop eax
        ret

; ESI=mapa de EBX x ECX -> EDI=el mismo al doble
ampliar:
        pushad
.f:
        mov edx, 2                  ; cada fila, dos veces
.r:
        push esi
        push ecx
        mov ecx, ebx
.p:
        lodsb
        stosb
        stosb
        dec ecx
        jnz .p
        pop ecx
        pop esi
        dec edx
        jnz .r
        add esi, ebx
        dec ecx
        jnz .f
        popad
        ret

; ---- el dibujo ----
pintar:
        HV ven_cli                  ; solo las cartas de la zona por repintar
        mov ecx, [ebp+V_UX0]
        sub ecx, eax
        mov [p_x0], ecx
        mov ecx, [ebp+V_UX1]
        sub ecx, eax
        mov [p_x1], ecx
        mov ecx, [ebp+V_UY0]
        sub ecx, ebx
        mov [p_y0], ecx
        mov ecx, [ebp+V_UY1]
        sub ecx, ebx
        mov [p_y1], ecx
        HV dc_pintar
        cmp dword [p_y0], FILA_Y - 4
        jge .pilas
        COLOR C_CREMA               ; los rótulos
        mov edi, rot
        mov esi, t_movs
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        mov eax, GAP
        call .rotulo
        mov edi, rot
        mov esi, t_tiempo
        call hvl_cad
        mov eax, [segundos]
        call pon_tiempo
        mov eax, GAP + 2 * PASO
        call .rotulo
        mov edi, rot
        mov esi, t_mazo
        call hvl_cad
        movzx eax, byte [pilas+P_MAZO*PILA_T]
        call hvl_num
        mov eax, CLI_AN - GAP - 8 * 8
        call .rotulo
.pilas:
        xor edi, edi
.p:
        call dib_pila
        inc edi
        cmp edi, 12
        jb .p
        cmp byte [arr_on], 0        ; y, encima de todo, las que se llevan
        je .x
        imul esi, [arr_pila], PILA_T
        add esi, pilas + 1
        add esi, [arr_desde]
        mov eax, [arr_x]
        mov ebx, [arr_y]
        mov edi, [arr_n]
.a:
        mov cl, [esi]
        mov edx, D_VIS
        cmp edi, 1
        jne .a1
        mov edx, CAR_AL
.a1:
        call dib_carta
        inc esi
        add ebx, D_VIS
        dec edi
        jnz .a
.x:
        HV dc_cerrar
        xor eax, eax
        ret
; (interna) acaba rot y lo pinta en EAX, 6
.rotulo:
        mov byte [edi], 0
        mov ebx, 6
        mov esi, rot
        HV dc_texto
        ret

; dibuja la pila EDI (sin las cartas que se llevan con el ratón)
dib_pila:
        pushad
        mov eax, edi
        call pila_pos               ; EAX, EBX=su sitio
        imul esi, edi, PILA_T
        add esi, pilas
        movzx ecx, byte [esi]
        cmp byte [arr_on], 0
        je .n
        cmp edi, [arr_pila]
        jne .n
        sub ecx, [arr_n]
.n:
        mov [d_n], ecx
        test ecx, ecx
        jnz .hay
        call dib_hueco
        jmp .x
.hay:
        cmp edi, P_COL
        jae .col
        mov cl, [esi+ecx]           ; arriba solo se ve la de encima
        mov edx, CAR_AL
        test edi, edi
        jnz .c1
        or cl, TAPADA               ; (el mazo, boca abajo)
.c1:
        call dib_carta
        jmp .x
.col:
        xor edi, edi
.cb:
        mov cl, [esi+1+edi]
        mov edx, CAR_AL
        inc edi
        cmp edi, [d_n]
        jae .cd                     ; la de encima, entera
        mov edx, D_VIS
        test cl, TAPADA
        jz .cd
        mov edx, D_TAP
.cd:
        call dib_carta
        add ebx, edx
        cmp edi, [d_n]
        jb .cb
.x:
        popad
        ret

; el sitio vacío de la pila EDI, en EAX, EBX
dib_hueco:
        pushad
        COLOR C_PETROLEO_CL
        mov ecx, CAR_AN
        mov edx, CAR_AL
        HV dc_marco
        test edi, edi
        jnz .sal
        add eax, CAR_AN / 2         ; el mazo: un rombo hueco (darle la vuelta)
        add ebx, CAR_AL / 2
        mov ecx, 11
        HV dc_rombo
        COLOR C_PETROLEO
        mov ecx, 7
        HV dc_rombo
        jmp .x
.sal:
        cmp edi, P_SAL
        jb .x
        cmp edi, P_COL
        jae .x
        sub edi, P_SAL              ; un montón de salida: su palo
        imul esi, edi, 676
        add esi, sim26
        add eax, (CAR_AN - 26) / 2
        add ebx, (CAR_AL - 26) / 2
        mov ecx, 26
        mov edx, 26
        HV dc_copiar_clave
.x:
        popad
        ret

; dibuja una carta: EAX=x EBX=y CL=carta EDX=el alto que se ve (CAR_AL: entera)
dib_carta:
        pushad
        cmp eax, [p_x1]             ; ¿cae en lo que hay que repintar?
        jge .x
        cmp ebx, [p_y1]
        jge .x
        lea esi, [eax+CAR_AN]
        cmp esi, [p_x0]
        jle .x
        lea esi, [ebx+edx]
        cmp esi, [p_y0]
        jle .x
        mov [c_x], eax
        mov [c_y], ebx
        mov [c_h], edx
        movzx edi, cl
        mov esi, C_BLANCO
        test edi, TAPADA
        jz .f
        mov esi, C_ANIL
.f:
        COLOR esi
        mov ecx, CAR_AN
        HV dc_rect
        COLOR C_TINTA               ; el filete, con las esquinas matadas
        mov edx, 1
        HV dc_rect
        mov ecx, 1
        mov edx, [c_h]
        HV dc_rect
        add eax, CAR_AN - 1
        HV dc_rect
        COLOR C_PETROLEO
        HV dc_punto
        mov eax, [c_x]
        HV dc_punto
        cmp dword [c_h], CAR_AL
        jne .cara
        add ebx, CAR_AL - 1
        HV dc_punto
        add eax, CAR_AN - 1
        HV dc_punto
        COLOR C_TINTA
        mov eax, [c_x]
        inc eax
        mov ecx, CAR_AN - 2
        mov edx, 1
        HV dc_rect
.cara:
        mov eax, [c_x]
        mov ebx, [c_y]
        test edi, TAPADA
        jnz .dorso
        and edi, 0x3F
        mov eax, edi
        xor edx, edx
        mov ecx, 10
        div ecx                     ; EAX=palo EDX=orden
        mov [c_palo], eax
        mov [c_ord], edx
        mov esi, C_TINTA            ; el número: oscuros en tinta, claros en
        test al, 2                  ; terracota
        jnz .n1
        mov esi, C_TERRACOTA
.n1:
        COLOR esi
        NEGRITA 1
        lea esi, [t_num+edx*2+edx]
        mov eax, [c_x]
        add eax, 4
        mov ebx, [c_y]
        inc ebx
        HV dc_texto
        NEGRITA 0
        imul esi, [c_palo], 169     ; y el palo, en la otra esquina
        add esi, sim13
        mov eax, [c_x]
        add eax, CAR_AN - 17
        mov ebx, [c_y]
        add ebx, 2
        mov ecx, 13
        mov edx, 13
        HV dc_copiar_clave
        cmp dword [c_h], CAR_AL
        jne .x
        mov edx, [c_ord]
        cmp edx, 7
        jae .figura
        test edx, edx
        jnz .pintas
        imul esi, [c_palo], 676     ; el as: el palo, grande
        add esi, sim26
        mov eax, [c_x]
        add eax, (CAR_AN - 26) / 2
        mov ebx, [c_y]
        add ebx, 34
        mov ecx, 26
        mov edx, 26
        HV dc_copiar_clave
        jmp .x
.pintas:
        mov esi, [t_pintas+edx*4]
        movzx edi, byte [esi]
        inc esi
        imul ecx, [c_palo], 169
        add ecx, sim13
        mov [c_sim], ecx
.pb:
        movzx eax, byte [esi]
        movzx ebx, byte [esi+1]
        add esi, 2
        add eax, [c_x]
        add ebx, [c_y]
        push esi
        mov esi, [c_sim]
        mov ecx, 13
        mov edx, 13
        HV dc_copiar_clave
        pop esi
        dec edi
        jnz .pb
        jmp .x
.figura:
        sub edx, 7
        mov eax, [c_palo]
        lea eax, [eax+eax*2]
        add eax, edx
        imul esi, eax, FIG_T
        add esi, figs
        mov eax, [c_x]
        add eax, (CAR_AN - 32) / 2
        mov ebx, [c_y]
        add ebx, 26
        mov ecx, 32
        mov edx, 44
        HV dc_copiar_clave
        jmp .x
.dorso:
        COLOR C_PETROLEO_CL
        add eax, 3
        add ebx, 3
        mov ecx, CAR_AN - 6
        cmp dword [c_h], CAR_AL
        je .d1
        mov edx, 1                  ; tapada debajo de otra: solo asoma el borde
        HV dc_rect
        jmp .x
.d1:
        mov edx, CAR_AL - 6
        HV dc_marco
        mov esi, dorso
.d2:
        movzx eax, byte [esi]
        cmp al, 0xFF
        je .x
        movzx ebx, byte [esi+1]
        movzx ecx, byte [esi+2]
        movzx edx, byte [esi+3]
        add esi, 4
        COLOR edx
        add eax, [c_x]
        add ebx, [c_y]
        HV dc_rombo
        jmp .d2
.x:
        popad
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
movs        resd 1
segundos    resd 1
des_n       resd 1                  ; pasos apuntados para deshacer
arr_pila    resd 1                  ; lo que se lleva con el ratón: de qué pila,
arr_desde   resd 1                  ; desde qué carta, cuántas,
arr_n       resd 1
arr_x       resd 1                  ; dónde están ahora
arr_y       resd 1
arr_dx      resd 1                  ; y por dónde se cogieron
arr_dy      resd 1
g_x         resd 1                  ; (donde) el sitio de la carta
g_y         resd 1
p_x0        resd 1                  ; lo que hay que repintar
p_y0        resd 1
p_x1        resd 1
p_y1        resd 1
d_n         resd 1
c_x         resd 1                  ; la carta que se está dibujando
c_y         resd 1
c_h         resd 1
c_palo      resd 1
c_ord       resd 1
c_sim       resd 1
rec_jug     resd 1                  ; los récords
rec_gan     resd 1
rec_tiempo  resd 1
rec_movs    resd 1
arr_on      resb 1
a_prueba    resb 1
nuevo_rec   resb 1                  ; 1 el tiempo, 2 los movimientos
            resb 1
ruta_rec    resb 80
rot         resb 32                 ; el rótulo que se está pintando
txt         resb 304
baraja      resb 40
pilas       resb 12 * PILA_T
des         resd DES_MAX
sim13       resb 4 * 169            ; los palos
sim26       resb 4 * 676            ; y al doble
fig_tmp     resb 352
figs        resb 12 * FIG_T         ; las figuras de cada palo, al doble
HVP_FIN
