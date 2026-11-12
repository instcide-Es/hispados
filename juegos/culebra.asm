; =============================================================================
;  CULEBRA.HVP - Culebra, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  La culebra avanza sola por el tablero y se lleva con las flechas hasta la
;  fruta; cada fruta la alarga dos casillas y cada cinco frutas va más
;  deprisa. La partida acaba al tocar el borde, un muro o su propio cuerpo.
;  P o Espacio: pausa (también se para sola al minimizar la ventana). Intro:
;  otra partida. Menú Juego: con o sin muros, tres
;  velocidades y las mejores puntuaciones, que se guardan en CULEBRA.REC (al
;  lado de CULEBRA.HVP; texto).
;
;  Todo está hecho aquí: el dibujo (casillas en damero crema y piedra,
;  culebra verde oliva con collar ocre y cabeza azul petróleo, frutas en
;  rombo), el icono y los textos. Es un módulo .HVP (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (las frutas salen siempre en
;           los mismos sitios)
;    /P     paso a paso: la culebra solo avanza una casilla con cada flecha
; =============================================================================
%include "hvp.inc"

COLS        equ 28
FILAS       equ 18
CEL         equ 16
TAB_X       equ 8
TAB_Y       equ 34
TAB_AN      equ COLS * CEL
TAB_AL      equ FILAS * CEL
CLI_AN      equ TAB_X * 2 + TAB_AN
CLI_AL      equ TAB_Y + TAB_AL + 8
CUERPO_MAX  equ 512                 ; (potencia de 2, más que casillas hay)
CAR_AN      equ 320                 ; el cartel del centro
CAR_AL      equ 58
CAR_X       equ TAB_X + (TAB_AN - CAR_AN) / 2
CAR_Y       equ TAB_Y + (TAB_AL - CAR_AL) / 2
E_LISTO     equ 0                   ; esperando la primera flecha
E_JUEGA     equ 1
E_PAUSA     equ 2
E_FIN       equ 3
M_VACIO     equ 0
M_CUERPO    equ 1
M_MURO      equ 2
M_FRUTA     equ 3

HVP_CABECERA "Culebra", entrada
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db ".................000000........."
        db ".....0000000000002222220...bb..."
        db "....0bbbbbbbbbbbb2229220...4...."
        db "....0bbbbbbbbbbbb22222200.444..."
        db "....0bbabbbabbbab222222dd45444.."
        db "....0bbabbbbbbbbb22222204444444."
        db "....0bbbbbbbbbbbb2229220.44444.."
        db "....0bbbbb00000002222220..444..."
        db "....0bbbbb0......000000....4...."
        db "....0bbabb0....................."
        db "....0bbbbb000000000000000000...."
        db "....0bbbbbbbbbbbbbbbbbbbbbbb0..."
        db "....0bbbbbbbbbbbbbbbbbbbbbbb0..."
        db "....0bbababbbabbbabbbabbbabb0..."
        db "....0bbbbbbbbbbbbbbbbbbbbabb0..."
        db "....0bbbbbbbbbbbbbbbbbbbbbbb0..."
        db ".....000000000000000000bbbbb0..."
        db "......................0bbbbb0..."
        db "......................0bbabb0..."
        db "...00000000000000000000bbbbb0..."
        db "..0bbbbbbbbbbbbbbbbbbbbbbbbb0..."
        db "..0bbbbbbbbbbbbbbbbbbbbbbbbb0..."
        db "..0bbabbbabbbabbbabbbabbbabb0..."
        db "..0bbbbbbbbbbbbbbbbbbbbbbbbb0..."
        db "..0bbbbbbbbbbbbbbbbbbbbbbbbb0..."
        db "...0000000000000000000000000...."
        db "................................"
        db "................................"
HVP_CODIGO

c_culebra   dd cul_proc
            db C_CREMA, 0, 0, 0
cul_plant:
        PLANTILLA c_culebra, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 150, 70, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

cul_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida", 0
        dw 102
        db "&Pausa", 0
        dw 0
        db 0
        dw 103
        db "Poner o quitar los &muros", 0
        dw 104
        db "Velocidad &lenta", 0
        dw 105
        db "Velocidad n&ormal", 0
        dw 106
        db "Velocidad &rápida", 0
        dw 0
        db 0
        dw 107
        db "Mejores p&untuaciones...", 0
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

; los muros: x, y, largo, 0 horizontal / 1 vertical
muros       db 7, 4, 5, 0,   16, 4, 5, 0,   7, 13, 5, 0,   16, 13, 5, 0
            db 3, 7, 4, 1,   24, 7, 4, 1,   0xFF
; los ojos de la cabeza según a dónde mira (derecha, izquierda, arriba, abajo)
ojos        db 9, 2, 9, 9,   2, 2, 2, 9,   2, 2, 9, 2,   2, 9, 9, 9
vel_base    db 5, 4, 3              ; tics de reloj por paso (lenta, normal, rápida)

t_titulo    db "Culebra", 0
t_puntos    db "Puntos: ", 0
t_nivel     db "Nivel: ", 0
t_record    db "Récord: ", 0
t_sin       db "Sin muros", 0
t_con       db "Con muros", 0
t_listo     db "Pulse una flecha para empezar", 0
t_pausa     db "Pausa", 0
t_pausa2    db "P o Espacio para seguir", 0
t_fin       db "Fin de la partida", 0
t_nuevo     db "¡Nuevo récord!", 0
t_fin2      db "Intro: otra partida", 0
t_rec_tit   db "Mejores puntuaciones", 0
t_rec_sin   db "Sin muros:  ", 0
t_rec_con   db "Con muros:  ", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Lleve la culebra con las flechas hasta la fruta.", 10
            db "Cada fruta la alarga y cada cinco va más deprisa.", 10
            db "No toque el borde, los muros ni su propio cuerpo.", 10
            db 10
            db "P o Espacio: pausa.    Intro: otra partida.", 0
k_cab       db "Culebra - mejores puntuaciones (Hispa Ventanas)", 13, 10, 0
k_sin       db "Sin=", 0
k_con       db "Con=", 0
k_vel       db "Velocidad=", 0
k_muros     db "Muros=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; CULEBRA.REC, al lado del módulo
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
        setnc byte [a_pasos]
        mov byte [velocidad], 1
        call rec_leer
        mov esi, cul_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
cul_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .letra
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], cul_menu
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
        jnz .pausa                  ; minimizada no se juega: en pausa
        call paso
        jmp .hecho
.letra:
        cmp ecx, ' '
        je .pausa
        and ecx, 0xDF
        cmp ecx, 'P'
        je .pausa
        jmp .hecho
.tecla:
        cmp ecx, TC_INTRO
        jne .t1
        cmp dword [estado], E_FIN
        jne .hecho
        call nueva
        jmp .hecho
.t1:
        xor eax, eax                ; EAX, EBX = hacia dónde
        xor ebx, ebx
        cmp ecx, TC_DER
        jne .t2
        inc eax
        jmp .dir
.t2:
        cmp ecx, TC_IZQ
        jne .t3
        dec eax
        jmp .dir
.t3:
        cmp ecx, TC_ABAJO
        jne .t4
        inc ebx
        jmp .dir
.t4:
        cmp ecx, TC_ARRIBA
        jne .hecho
        dec ebx
.dir:
        cmp dword [estado], E_JUEGA
        ja .hecho                   ; en pausa o acabada
        mov ecx, eax                ; media vuelta no se puede dar
        add ecx, [dir_x]
        mov edx, ebx
        add edx, [dir_y]
        or ecx, edx
        jz .d1
        mov [pend_x], eax
        mov [pend_y], ebx
.d1:
        cmp dword [estado], E_LISTO
        jne .d2
        mov dword [estado], E_JUEGA
        HV ven_invalidar
        call poner_reloj
.d2:
        cmp byte [a_pasos], 0
        je .hecho
        call paso
        jmp .hecho
.orden:
        cmp ecx, 101
        jne .o2
        call nueva
        jmp .hecho
.o2:
        cmp ecx, 102
        jne .o3
.pausa:
        call pausa
        jmp .hecho
.o3:
        cmp ecx, 103
        jne .o4
        xor byte [muros_si], 1
        jmp .cambio
.o4:
        cmp ecx, 107
        je .records
        ja .o8
        sub ecx, 104                ; 104-106: la velocidad
        jb .hecho
        mov [velocidad], cl
.cambio:
        call rec_guardar
        call nueva
        jmp .hecho
.o8:
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
        call parar
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp .dlg
.records:
        call parar
        mov edi, txt                ; las dos listas
        mov esi, t_rec_sin
        mov ebx, rec
        call .lista
        mov al, 10
        stosb
        mov esi, t_rec_con
        call .lista
        mov byte [edi], 0
        mov esi, txt
        mov edi, t_rec_tit
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret
; (interna) ESI=rótulo EBX=cinco puntuaciones: a EDI; EBX avanza
.lista:
        call hvl_cad
        mov ecx, 5
.l1:
        mov eax, [ebx]
        add ebx, 4
        call hvl_num
        mov ax, '  '
        stosw
        dec ecx
        jnz .l1
        ret

; si se está jugando, pausa (antes de abrir un diálogo)
parar:
        cmp dword [estado], E_JUEGA
        jne pausa.x
; pausa o sigue
pausa:
        mov eax, [estado]
        cmp eax, E_JUEGA
        je .p
        cmp eax, E_PAUSA
        jne .x
        mov dword [estado], E_JUEGA
        call poner_reloj
        jmp .inv
.p:
        mov dword [estado], E_PAUSA
        mov ecx, 1
        HV tmp_quitar
.inv:
        HV ven_invalidar
.x:
        ret

; el reloj de los pasos, según la velocidad y el nivel (no en paso a paso)
poner_reloj:
        cmp byte [a_pasos], 0
        jne .x
        pushad
        movzx eax, byte [velocidad]
        movzx edx, byte [vel_base+eax]
        sub edx, [nivel]
        inc edx
        imul edx, 55
        mov ecx, 1
        HV tmp_poner
        popad
.x:
        ret

; ---- partida nueva ----
nueva:
        pushad
        mov ebp, [ven]
        mov edi, mapa
        mov ecx, COLS * FILAS
        xor al, al
        rep stosb
        cmp byte [muros_si], 0
        je .sin
        mov esi, muros
.m:
        lodsb
        cmp al, 0xFF
        je .sin
        movzx ebx, al
        lodsb
        movzx edx, al
        lodsb
        movzx ecx, al
        lodsb
.m1:
        imul edi, edx, COLS
        mov byte [mapa+edi+ebx], M_MURO
        test al, al
        jz .mh
        inc edx
        jmp .m2
.mh:
        inc ebx
.m2:
        dec ecx
        jnz .m1
        jmp .m
.sin:
        xor ecx, ecx                ; la culebra: cuatro casillas en la fila 2
.c:                                 ; (a la vista, por encima del cartel)
        lea eax, [ecx+4]
        mov ah, 2
        mov [cuerpo+ecx*2], ax
        movzx edi, al
        mov byte [mapa+2*COLS+edi], M_CUERPO
        inc ecx
        cmp ecx, 4
        jb .c
        xor eax, eax
        mov [cola], eax
        mov dword [cab], 3
        mov [dir_y], eax
        mov [pend_y], eax
        mov [puntos], eax
        mov [comidas], eax
        mov [crecer], eax
        mov [nuevo_rec], al
        inc eax
        mov [dir_x], eax
        mov [pend_x], eax
        mov [nivel], eax
        mov dword [estado], E_LISTO
        call fruta_nueva
        mov ecx, 1
        HV tmp_quitar
        HV ven_invalidar
        popad
        ret

; una fruta en una casilla libre (la primera libre a partir de una al azar)
fruta_nueva:
        pushad
        mov eax, COLS * FILAS
        HV azar
        mov ecx, COLS * FILAS
.b:
        cmp byte [mapa+eax], M_VACIO
        je .h
        inc eax
        cmp eax, COLS * FILAS
        jb .b1
        xor eax, eax
.b1:
        dec ecx
        jnz .b
        jmp .x                      ; (no queda sitio)
.h:
        mov byte [mapa+eax], M_FRUTA
        call inv_celda
.x:
        popad
        ret

; la casilla EAX (fila*COLS+columna) hay que repintarla
inv_celda:
        pushad
        xor edx, edx
        mov ecx, COLS
        div ecx
        imul ebx, eax, CEL
        add ebx, TAB_Y
        imul eax, edx, CEL
        add eax, TAB_X
        mov ecx, CEL
        mov edx, CEL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; -> EAX=la casilla del anillo ESI del cuerpo
casilla_de:
        movzx eax, byte [cuerpo+esi*2+1]
        imul eax, COLS
        push edx
        movzx edx, byte [cuerpo+esi*2]
        add eax, edx
        pop edx
        ret

; ---- un paso de la culebra ----
paso:
        pushad
        mov ebp, [ven]
        mov eax, [pend_x]
        mov [dir_x], eax
        mov eax, [pend_y]
        mov [dir_y], eax
        mov esi, [cab]
        movzx ebx, byte [cuerpo+esi*2]
        movzx edx, byte [cuerpo+esi*2+1]
        add ebx, [dir_x]
        add edx, [dir_y]
        cmp ebx, COLS               ; el borde
        jae .fin
        cmp edx, FILAS
        jae .fin
        imul edi, edx, COLS
        add edi, ebx                ; EDI=la casilla a la que va
        mov al, [mapa+edi]
        cmp al, M_MURO
        je .fin
        xor ecx, ecx                ; CL=1 come, CH=1 crece en este paso
        cmp al, M_FRUTA
        jne .p1
        mov ecx, 0x0101
        jmp .p2
.p1:
        cmp dword [crecer], 0
        je .p2
        dec dword [crecer]
        mov ch, 1
.p2:
        mov esi, [cola]
        call casilla_de
        mov esi, eax                ; ESI=la casilla de la cola
        cmp byte [mapa+edi], M_CUERPO
        jne .libre
        test ch, ch                 ; su cuerpo: solo vale la casilla que la
        jnz .fin                    ; cola deja en este mismo paso
        cmp edi, esi
        jne .fin
.libre:
        test ch, ch
        jnz .nq
        mov byte [mapa+esi], M_VACIO
        mov eax, esi
        call inv_celda
        mov eax, [cola]
        inc eax
        and eax, CUERPO_MAX - 1
        mov [cola], eax
.nq:
        mov esi, [cab]
        call casilla_de
        call inv_celda              ; (la cabeza vieja pasa a ser cuerpo)
        inc esi
        and esi, CUERPO_MAX - 1
        mov [cab], esi
        mov [cuerpo+esi*2], bl
        mov [cuerpo+esi*2+1], dl
        mov byte [mapa+edi], M_CUERPO
        mov eax, edi
        call inv_celda
        test cl, cl
        jz .x
        inc dword [crecer]          ; cada fruta: dos casillas más
        movzx eax, byte [velocidad]
        add eax, [nivel]
        movzx edx, byte [muros_si]
        add eax, edx
        lea eax, [eax+eax*4]
        add [puntos], eax
        inc dword [comidas]
        mov eax, [comidas]
        xor edx, edx
        mov ecx, 5
        div ecx
        test edx, edx
        jnz .p3
        movzx eax, byte [velocidad] ; cada cinco, un nivel (hasta un tic por paso)
        movzx eax, byte [vel_base+eax]
        cmp [nivel], eax
        jae .p3
        inc dword [nivel]
        call poner_reloj
.p3:
        call fruta_nueva
        xor eax, eax                ; los rótulos de arriba
        xor ebx, ebx
        mov ecx, CLI_AN
        mov edx, TAB_Y - 4
        HV ven_invalidar_rect
        jmp .x
.fin:
        mov dword [estado], E_FIN
        mov ecx, 1
        HV tmp_quitar
        call rec_apuntar
        HV ven_invalidar
.x:
        popad
        ret

; ---- las mejores puntuaciones: cinco sin muros y cinco con muros ----
rec_apuntar:
        pushad
        mov eax, [puntos]
        test eax, eax
        jz .x
        movzx ebx, byte [muros_si]
        imul ebx, 20
        add ebx, rec
        xor ecx, ecx
.b:
        cmp eax, [ebx+ecx*4]
        ja .ins
        inc ecx
        cmp ecx, 5
        jb .b
        jmp .x
.ins:
        mov edx, 4
.d:
        cmp edx, ecx
        jbe .p
        mov esi, [ebx+edx*4-4]
        mov [ebx+edx*4], esi
        dec edx
        jmp .d
.p:
        mov [ebx+ecx*4], eax
        test ecx, ecx
        setz byte [nuevo_rec]
        call rec_guardar
.x:
        popad
        ret

rec_guardar:
        pushad
        mov edi, txt
        mov esi, k_cab
        call hvl_cad
        mov esi, k_sin
        mov ebx, rec
        call .fila
        mov esi, k_con
        call .fila
        mov esi, k_vel
        call hvl_cad
        movzx eax, byte [velocidad]
        call .num
        mov esi, k_muros
        call hvl_cad
        movzx eax, byte [muros_si]
        call .num
        mov ecx, edi
        mov edx, txt
        sub ecx, edx
        mov esi, ruta_rec
        call hvl_escribir
        popad
        ret
.fila:
        call hvl_cad
        mov ecx, 5
.f1:
        mov eax, [ebx]
        add ebx, 4
        call hvl_num
        mov al, ','
        stosb
        dec ecx
        jnz .f1
        dec edi
        jmp .nl
.num:
        call hvl_num
.nl:
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
        mov edi, k_sin
        mov ebx, rec
        call .fila
        mov edi, k_con
        mov ebx, rec + 20
        call .fila
        mov esi, txt
        mov edi, k_vel
        call hvl_clave
        jc .v
        call hvl_lee_num
        cmp eax, 2
        ja .v
        mov [velocidad], al
.v:
        mov esi, txt
        mov edi, k_muros
        call hvl_clave
        jc .x
        call hvl_lee_num
        and al, 1
        mov [muros_si], al
.x:
        popad
        ret
.fila:
        mov esi, txt
        call hvl_clave
        jc .f2
        mov ecx, 5
.f1:
        call hvl_lee_num
        mov [ebx], eax
        add ebx, 4
        cmp byte [esi], ','
        jne .f2
        inc esi
        dec ecx
        jnz .f1
.f2:
        ret

; ---- el dibujo ----
; (interna) de puntos a casillas: ECX=primer punto EDX=último+1 (ya sin el
; origen del tablero) ESI=casillas que hay -> ECX=primera EDX=última
rango:
        sar ecx, 4
        jns .a
        xor ecx, ecx
.a:
        dec edx
        sar edx, 4
        cmp edx, esi
        jl .b
        lea edx, [esi-1]
.b:
        ret

pintar:
        HV ven_cli                  ; solo las casillas de la zona por repintar
        mov ecx, [ebp+V_UX0]
        mov edx, [ebp+V_UX1]
        sub ecx, eax
        sub edx, eax
        sub ecx, TAB_X
        sub edx, TAB_X
        mov esi, COLS
        call rango
        mov [p_c0], ecx
        mov [p_c1], edx
        mov ecx, [ebp+V_UY0]
        mov edx, [ebp+V_UY1]
        sub ecx, ebx
        sub edx, ebx
        sub ecx, TAB_Y
        sub edx, TAB_Y
        mov esi, FILAS
        call rango
        mov [p_f0], ecx
        mov [p_f1], edx
        HV dc_pintar
        ; los rótulos
        COLOR C_TINTA
        mov edi, rot
        mov esi, t_puntos
        call hvl_cad
        mov eax, [puntos]
        call hvl_num
        mov byte [edi], 0
        mov eax, 8
        call .rotulo
        mov edi, rot
        mov esi, t_nivel
        call hvl_cad
        mov eax, [nivel]
        call hvl_num
        mov byte [edi], 0
        mov eax, 136
        call .rotulo
        mov edi, rot
        mov esi, t_record
        call hvl_cad
        movzx eax, byte [muros_si]
        imul eax, 20
        mov eax, [rec+eax]
        call hvl_num
        mov byte [edi], 0
        mov eax, 224
        call .rotulo
        COLOR C_ARENA_OSC
        mov esi, t_sin
        cmp byte [muros_si], 0
        je .r1
        mov esi, t_con
.r1:
        mov eax, CLI_AN - 8 - 9*8
        mov ebx, 8
        HV dc_texto
        ; el marco del tablero
        COLOR C_MARRON
        mov eax, TAB_X - 2
        mov ebx, TAB_Y - 2
        mov ecx, TAB_AN + 4
        mov edx, TAB_AL + 4
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        ; las casillas
        mov edx, [p_f0]
.f:
        cmp edx, [p_f1]
        jg .ff
        mov ecx, [p_c0]
.c:
        cmp ecx, [p_c1]
        jg .cf
        call celda
        inc ecx
        jmp .c
.cf:
        inc edx
        jmp .f
.ff:
        ; el cartel
        mov eax, [estado]
        cmp eax, E_JUEGA
        je .x
        mov esi, t_titulo
        mov edi, t_listo
        cmp eax, E_LISTO
        je .cartel
        mov esi, t_pausa
        mov edi, t_pausa2
        cmp eax, E_PAUSA
        je .cartel
        mov esi, t_fin
        mov edi, t_fin2
        cmp byte [nuevo_rec], 0
        je .cartel
        mov esi, t_nuevo
.cartel:
        COLOR C_CREMA
        mov eax, CAR_X
        mov ebx, CAR_Y
        mov ecx, CAR_AN
        mov edx, CAR_AL
        HV dc_rect
        COLOR C_MARRON
        HV dc_marco
        add eax, 2
        add ebx, 2
        sub ecx, 4
        sub edx, 4
        COLOR C_OCRE
        HV dc_marco
        COLOR C_TERRACOTA
        NEGRITA 1
        mov ebx, CAR_Y + 10
        call .centro
        NEGRITA 0
        COLOR C_TINTA
        mov esi, edi
        mov ebx, CAR_Y + 32
        call .centro
.x:
        HV dc_cerrar
        xor eax, eax
        ret
; (interna) rot en EAX, 8
.rotulo:
        mov ebx, 8
        mov esi, rot
        HV dc_texto
        ret
; (interna) ESI centrado en el cartel, a la altura EBX
.centro:
        push esi
        xor eax, eax
.c1:
        cmp byte [esi], 0
        je .c2
        inc esi
        add eax, 4
        jmp .c1
.c2:
        pop esi
        neg eax
        add eax, CAR_X + CAR_AN / 2
        HV dc_texto
        ret

; dibuja la casilla ECX (columna), EDX (fila)
celda:
        pushad
        imul eax, ecx, CEL
        add eax, TAB_X
        imul ebx, edx, CEL
        add ebx, TAB_Y
        imul edi, edx, COLS
        add edi, ecx
        lea esi, [ecx+edx]
        and esi, 1                  ; el suelo: damero crema y piedra
        imul esi, C_PIEDRA - C_CREMA
        add esi, C_CREMA
        COLOR esi
        mov ecx, CEL
        mov edx, CEL
        HV dc_rect
        movzx esi, byte [mapa+edi]
        cmp esi, M_CUERPO
        je .cuerpo
        cmp esi, M_MURO
        je .muro
        cmp esi, M_FRUTA
        jne .x
        COLOR C_TERRACOTA           ; la fruta: un rombo con su rabito
        push eax
        push ebx
        add eax, 8
        add ebx, 9
        mov ecx, 5
        HV dc_rombo
        COLOR C_TERRACOTA_CL
        sub eax, 2
        sub ebx, 1
        HV dc_punto
        dec ebx
        inc eax
        HV dc_punto
        pop ebx
        pop eax
        COLOR C_OLIVA
        add eax, 8
        add ebx, 1
        mov ecx, 2
        mov edx, 3
        HV dc_rect
        add eax, 2
        mov ecx, 3
        mov edx, 2
        HV dc_rect
        jmp .x
.muro:
        COLOR C_MARRON              ; el muro: ladrillos
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        add ebx, 7
        HV dc_rect
        add ebx, 8
        HV dc_rect
        sub ebx, 15
        mov ecx, 1
        mov edx, 7
        add eax, 7
        HV dc_rect
        add ebx, 8
        sub eax, 4
        HV dc_rect
        add eax, 8
        HV dc_rect
        jmp .x
.cuerpo:
        inc eax
        inc ebx
        mov ecx, CEL - 2
        mov edx, CEL - 2
        mov esi, [cab]
        push eax
        call casilla_de
        cmp eax, edi
        pop eax
        je .cabeza
        COLOR C_OLIVA               ; el cuerpo, con su collar
        HV dc_rect
        COLOR C_OCRE
        add eax, 5
        add ebx, 5
        mov ecx, 4
        mov edx, 4
        HV dc_rect
        jmp .x
.cabeza:
        COLOR C_PETROLEO
        HV dc_rect
        xor esi, esi                ; los ojos, hacia donde mira
        cmp dword [dir_x], 1
        je .o
        inc esi
        cmp dword [dir_x], -1
        je .o
        inc esi
        cmp dword [dir_y], -1
        je .o
        inc esi
.o:
        lea esi, [ojos+esi*4]
        COLOR C_BLANCO
        mov ecx, 3
        mov edx, 3
        call .ojo
        call .ojo
.x:
        popad
        ret
.ojo:
        push eax
        push ebx
        movzx edi, byte [esi]
        add eax, edi
        movzx edi, byte [esi+1]
        add ebx, edi
        add esi, 2
        HV dc_rect
        COLOR C_TINTA
        inc eax
        inc ebx
        HV dc_punto
        COLOR C_BLANCO
        pop ebx
        pop eax
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
dir_x       resd 1                  ; hacia dónde va
dir_y       resd 1
pend_x      resd 1                  ; hacia dónde irá en el paso siguiente
pend_y      resd 1
cab         resd 1                  ; el cuerpo es un anillo: cabeza y cola
cola        resd 1
crecer      resd 1                  ; casillas que le quedan por crecer
puntos      resd 1
nivel       resd 1
comidas     resd 1
p_c0        resd 1
p_c1        resd 1
p_f0        resd 1
p_f1        resd 1
rec         resd 10                 ; cinco sin muros y cinco con muros
velocidad   resb 1                  ; 0 lenta, 1 normal, 2 rápida
muros_si    resb 1
a_pasos     resb 1
nuevo_rec   resb 1
ruta_rec    resb 80
rot         resb 32                 ; el rótulo que se está pintando
txt         resb 304
cuerpo      resw CUERPO_MAX         ; columna y fila de cada casilla
mapa        resb COLS * FILAS
HVP_FIN
