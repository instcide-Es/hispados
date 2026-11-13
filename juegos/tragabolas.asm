; =============================================================================
;  TRAGABOL.HVP - Tragabolas, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un jardín de setos lleno de bolitas. El Tragón (una bola azul petróleo con
;  ojos) se lleva con las flechas y se las traga todas para pasar al jardín
;  siguiente; tres erizos lo persiguen, cada uno a su manera: el granate va
;  derecho a por él, el añil da vueltas sin rumbo y el marrón le sale al paso
;  por delante. Si un erizo lo pincha pierde una vida (tiene tres). En las
;  esquinas hay campanas: al coger una, los erizos se duermen un rato (se
;  ponen grises, con los ojos cerrados) y no pinchan. Hay tres jardines que
;  se van turnando; cada vuelta va todo más deprisa y el sueño dura menos.
;
;  Puntos: 10 por bolita, 50 por campana y 100 por el número del jardín al
;  dejarlo limpio. Las flechas se recuerdan: el Tragón gira en cuanto puede.
;  P o Espacio: pausa (también se para solo al minimizar). F2: partida nueva.
;  Intro: otra partida al acabar. Las cinco mejores puntuaciones se guardan
;  en TRAGABOL.REC (al lado de TRAGABOL.HVP; texto).
;
;  Todo está hecho aquí: los jardines, el Tragón, los erizos, las campanas,
;  el icono y los textos. Es un módulo .HVP (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (los erizos hacen siempre lo
;           mismo ante los mismos movimientos)
;    /P     paso a paso: todo avanza un paso solo con cada flecha
;    /N=n   empezar por el jardín n (1 a 3)
; =============================================================================
%include "hvp.inc"

COLS        equ 25
FILAS       equ 17
CEL         equ 20
TAB_X       equ 8
TAB_Y       equ 34
TAB_AN      equ COLS * CEL
TAB_AL      equ FILAS * CEL
PIE_Y       equ TAB_Y + TAB_AL + 7  ; el renglón de avisos, debajo
CLI_AN      equ TAB_X * 2 + TAB_AN
CLI_AL      equ PIE_Y + 16 + 5
CAR_AN      equ 320                 ; el cartel del centro
CAR_AL      equ 58
CAR_X       equ TAB_X + (TAB_AN - CAR_AN) / 2
CAR_Y       equ TAB_Y + (TAB_AL - CAR_AL) / 2
N_LAB       equ 3                   ; jardines
N_PER       equ 3                   ; erizos
JUG_X0      equ 12                  ; de dónde sale el Tragón
JUG_Y0      equ 14
PER_Y0      equ 8                   ; y los erizos (columnas 10, 12 y 14)
E_LISTO     equ 0                   ; esperando una flecha
E_JUEGA     equ 1
E_PAUSA     equ 2
E_FIN       equ 3
M_SUELO     equ 0
M_SETO      equ 1
M_BOLA      equ 2
M_CAMPANA   equ 3
A_EMPEZAR   equ 0                   ; el aviso del renglón de abajo
A_PINCHAZO  equ 1
A_LIMPIO    equ 2

; rectángulo dentro de la casilla que se está dibujando: x, y, ancho, alto
%macro RC 4
        mov eax, [c_x]
        add eax, %1
        mov ebx, [c_y]
        add ebx, %2
        mov ecx, %3
        mov edx, %4
        HV dc_rect
%endmacro

HVP_CABECERA "Tragabolas", entrada
        db "................................"
        db "................................"
        db ".bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb."
        db ".bbbbbbbbbb2bbbbbbbbbbbbbb2bbbb."
        db ".bbb2bbbbbbbbbbbbbbbbbbbbbbbbbb."
        db ".bbbbbbbbbbbbbbbbbb2bbbbbbbbbbb."
        db ".bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb."
        db "................................"
        db "........00000000................"
        db ".......0022222200..............."
        db "......ee2222222220.............."
        db ".....0e222222222220............."
        db "....0029992299922200............"
        db "....0229902299022220............"
        db "....0229992299922220..aa...aa..."
        db "....0222222222222220.aaaa.aaaa.."
        db "....0222222222222220.aaaa.aaaa.."
        db "....0222222222222220..aa...aa..."
        db "....0222255555522220............"
        db "....0022255555522200............"
        db ".....02222222222220............."
        db "......022222222220.............."
        db ".......0022222200..............."
        db "........00000000................"
        db "................................"
        db ".bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb."
        db ".bbbbbbbbbbbbb2bbbbbbbbbbbbb2bb."
        db ".bbbbb2bbbbbbbbbbbbbbbbbbbbbbbb."
        db ".bbbbbbbbbbbbbbbbbbbbb2bbbbbbbb."
        db ".bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb."
        db "................................"
        db "................................"
HVP_CODIGO

c_traga     dd tra_proc
            db C_CREMA, 0, 0, 0
tra_plant:
        PLANTILLA c_traga, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 40, 30, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

tra_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida      F2", 0
        dw 102
        db "&Pausa", 0
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

; los jardines: «#» seto, «.» bolita, «o» campana
labs:
        ; laberinto 1
        db "#########################"
        db "#o....#...........#....o#"
        db "#.###.#.####.####.#.###.#"
        db "#.#...................#.#"
        db "#.#.##.###.###.###.##.#.#"
        db "#....#...#.....#...#....#"
        db "####.#.#.#.#.#.#.#.#.####"
        db "#....#.#...#.#...#.#....#"
        db "#.####.##       ##.####.#"
        db "#......#.#######.#......#"
        db "#.####.#.........#.####.#"
        db "#.#....####.#.####....#.#"
        db "#.#.##.............##.#.#"
        db "#...#..###.###.###..#...#"
        db "#.#.#.##.........##.#.#.#"
        db "#o#.........#.........#o#"
        db "#########################"
        ; laberinto 2
        db "#########################"
        db "#o.....................o#"
        db "#.##.##.##.###.##.##.##.#"
        db "#.##.##.##.###.##.##.##.#"
        db "#.......................#"
        db "#.##.#.####.#.####.#.##.#"
        db "#.#..#.............#..#.#"
        db "#...##.##.#####.##.##...#"
        db "#.#......       ......#.#"
        db "#.#.##.###########.##.#.#"
        db "#.......................#"
        db "#.##.#.##.##.##.##.#.##.#"
        db "#.#..#.#...#.#...#.#..#.#"
        db "#...##.#.#.#.#.#.#.##...#"
        db "#.#......#.....#......#.#"
        db "#o#.####...#.#...####.#o#"
        db "#########################"
        ; laberinto 3
        db "#########################"
        db "#.....#...........#.....#"
        db "#.###.#.###.#.###.#.###.#"
        db "#.#o#...#...#...#...#o#.#"
        db "#.#.#####.#.#.#.#####.#.#"
        db "#.#.......#...#.......#.#"
        db "#.#######.#####.#######.#"
        db "#.........#...#.........#"
        db "#.#####.#       #.#####.#"
        db "#.....#.####.####.#.....#"
        db "####.##...........##.####"
        db "#....#..####.####..#....#"
        db "#.####.##.......##.####.#"
        db "#......#..##.##..#......#"
        db "#.####.#.#.....#.#.####.#"
        db "#o.....#...#.#...#.....o#"
        db "#########################"
; las cuatro direcciones (x, y): derecha, izquierda, abajo, arriba
dirs        db 1, 0,   -1, 0,   0, 1,   0, -1
per_col     db 10, 12, 14           ; la columna de salida de cada erizo
per_color   db C_GRANATE, C_ANIL, C_MARRON

t_titulo    db "Tragabolas", 0
t_puntos    db "Puntos: ", 0
t_nivel     db "Jardín: ", 0
t_vidas     db "Vidas: ", 0
t_record    db "Récord: ", 0
t_listo     db "Pulse una flecha para empezar", 0
t_pinchazo  db "¡Pinchazo! Pulse una flecha para seguir", 0
t_limpio    db "¡Jardín limpio! Pulse una flecha para seguir", 0
t_duermen   db "Los erizos duermen: ", 0
t_pausa     db "Pausa", 0
t_pausa2    db "P o Espacio para seguir", 0
t_fin       db "Fin de la partida", 0
t_nuevo     db "¡Nuevo récord!", 0
t_fin2      db "Intro: otra partida", 0
t_rec_tit   db "Mejores puntuaciones", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Lleve al Tragón con las flechas y tráguese todas las", 10
            db "bolitas del jardín. Que no lo pinchen los erizos: tiene", 10
            db "tres vidas. Con una campana, los erizos se duermen un", 10
            db "rato (se ponen grises) y no pinchan.", 10
            db 10
            db "P o Espacio: pausa.   F2: partida nueva.", 0
k_cab       db "Tragabolas - mejores puntuaciones (Hispa Ventanas)", 13, 10, 0
k_puntos    db "Puntos=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; TRAGABOL.REC, al lado del módulo
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
        mov esi, edi
        mov al, 'N'
        call hvl_opcion
        jc .n
        call hvl_lee_num
        dec eax
        cmp eax, N_LAB
        jae .n
        mov [lab_ini], eax
.n:
        call rec_leer
        mov esi, tra_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
tra_proc:
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
        mov dword [ebp+V_MENU], tra_menu
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
        cmp ecx, TC_F1 + 1          ; F2
        jne .t0
        call nueva
        jmp .hecho
.t0:
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
        mov [pend_x], eax
        mov [pend_y], ebx
        cmp dword [estado], E_LISTO
        jne .d2
        mov dword [estado], E_JUEGA
        call inv_pie
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
        jne .o7
.pausa:
        call pausa
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
        call parar
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp .dlg
.records:
        call parar
        mov edi, txt                ; la lista: «1.  120», una por renglón
        xor ecx, ecx
.r1:
        lea eax, [ecx+1]
        call hvl_num
        mov eax, '.   '
        stosd
        mov eax, [rec+ecx*4]
        call hvl_num
        mov al, 10
        stosb
        inc ecx
        cmp ecx, 5
        jb .r1
        mov byte [edi-1], 0
        mov esi, txt
        mov edi, t_rec_tit
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
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

; el reloj de los pasos: cuatro tics de reloj por paso en la primera vuelta
; a los tres jardines, tres en la segunda y dos desde la tercera (no en
; paso a paso)
poner_reloj:
        cmp byte [a_pasos], 0
        jne .x
        pushad
        call vuelta
        mov edx, 4
        sub edx, eax
        imul edx, 55
        mov ecx, 1
        mov ebp, [ven]
        HV tmp_poner
        popad
.x:
        ret

; -> EAX=la vuelta en la que se está: 0, 1 o 2 (de la tercera no pasa)
vuelta:
        push edx
        push ecx
        mov eax, [nivel]
        dec eax
        xor edx, edx
        mov ecx, N_LAB
        div ecx
        cmp eax, 2
        jbe .x
        mov eax, 2
.x:
        pop ecx
        pop edx
        ret

; ---- partida nueva ----
nueva:
        pushad
        mov ebp, [ven]
        xor eax, eax
        mov [puntos], eax
        mov [paso_n], eax
        mov [nuevo_rec], al
        mov byte [aviso], A_EMPEZAR
        mov dword [nivel], 1
        mov dword [vidas], 3
        call jardin
        mov dword [estado], E_LISTO
        mov ecx, 1
        HV tmp_quitar
        HV ven_invalidar
        popad
        ret

; pone el jardín que toca según el nivel, con todas sus bolitas, y a cada
; uno en su sitio
jardin:
        pushad
        mov eax, [nivel]
        dec eax
        add eax, [lab_ini]
        xor edx, edx
        mov ecx, N_LAB
        div ecx
        imul esi, edx, COLS * FILAS
        add esi, labs
        mov edi, mapa
        mov ecx, COLS * FILAS
.b:
        lodsb
        mov ah, M_SETO
        cmp al, '#'
        je .p
        mov ah, M_BOLA
        cmp al, '.'
        je .p
        mov ah, M_CAMPANA
        cmp al, 'o'
        je .p
        mov ah, M_SUELO
.p:
        mov [edi], ah
        inc edi
        dec ecx
        jnz .b
        mov byte [mapa + JUG_Y0*COLS + JUG_X0], M_SUELO
        xor eax, eax                ; las bolitas que hay
        mov ecx, COLS * FILAS
        mov esi, mapa
.c:
        cmp byte [esi], M_BOLA
        jne .c1
        inc eax
.c1:
        inc esi
        dec ecx
        jnz .c
        mov [quedan], eax
        call colocar
        popad
        ret

; el Tragón y los erizos, a su sitio de salida, parados y despiertos
colocar:
        pushad
        xor eax, eax
        mov dword [jug_x], JUG_X0
        mov dword [jug_y], JUG_Y0
        mov [dir_x], eax
        mov [dir_y], eax
        mov [pend_x], eax
        mov [pend_y], eax
        mov [dormidos], eax
        xor esi, esi
.b:
        movzx edx, byte [per_col+esi]
        mov [per_x+esi*4], edx
        mov dword [per_y+esi*4], PER_Y0
        mov [per_dx+esi*4], eax
        mov [per_dy+esi*4], eax
        inc esi
        cmp esi, N_PER
        jb .b
        popad
        ret

; EAX=columna EDX=fila -> ZF=1 si es seto (el borde lo es siempre)
es_seto:
        push edi
        imul edi, edx, COLS
        cmp byte [mapa+edi+eax], M_SETO
        pop edi
        ret

; la casilla EAX=columna EDX=fila hay que repintarla
inv_xy:
        pushad
        imul eax, CEL
        add eax, TAB_X
        imul ebx, edx, CEL
        add ebx, TAB_Y
        mov ecx, CEL
        mov edx, CEL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; los rótulos de arriba / el renglón de abajo / las casillas de los erizos
inv_arriba:
        pushad
        xor eax, eax
        xor ebx, ebx
        mov edx, TAB_Y - 4
        jmp inv_pie.inv
inv_pie:
        pushad
        xor eax, eax
        mov ebx, PIE_Y - 2
        mov edx, 20
.inv:
        mov ecx, CLI_AN
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret
inv_erizos:
        pushad
        xor esi, esi
.b:
        mov eax, [per_x+esi*4]
        mov edx, [per_y+esi*4]
        call inv_xy
        inc esi
        cmp esi, N_PER
        jb .b
        popad
        ret

; ---- un paso: el Tragón y, tres pasos de cada cuatro, los erizos ----
paso:
        pushad
        mov ebp, [ven]
        mov eax, [jug_x]
        mov [ant_x], eax
        mov edx, [jug_y]
        mov [ant_y], edx
        add eax, [pend_x]           ; ¿puede girar ya hacia donde se pidió?
        add edx, [pend_y]
        call es_seto
        je .p1
        mov ecx, [pend_x]
        mov [dir_x], ecx
        mov ecx, [pend_y]
        mov [dir_y], ecx
.p1:
        mov eax, [jug_x]
        mov edx, [jug_y]
        add eax, [dir_x]
        add edx, [dir_y]
        call es_seto
        je .p2                      ; un seto: se queda parado
        mov [jug_x], eax
        mov [jug_y], edx
        call inv_xy
        imul edi, edx, COLS
        add edi, eax
        mov eax, [ant_x]
        mov edx, [ant_y]
        call inv_xy
        mov cl, [mapa+edi]          ; lo que se traga
        mov byte [mapa+edi], M_SUELO
        cmp cl, M_BOLA
        jne .c1
        add dword [puntos], 10
        dec dword [quedan]
        call inv_arriba
        jmp .p2
.c1:
        cmp cl, M_CAMPANA
        jne .p2
        add dword [puntos], 50
        call vuelta                 ; el sueño: 32, 24 o 16 pasos
        shl eax, 3
        neg eax
        add eax, 33                 ; (uno más: este mismo paso ya lo gasta)
        mov [dormidos], eax
        call inv_arriba
        call inv_erizos
.p2:
        cmp dword [dormidos], 0
        jne .p3
        xor esi, esi                ; ¿se ha metido en la casilla de un erizo?
.ch:
        mov eax, [per_x+esi*4]
        cmp eax, [jug_x]
        jne .ch1
        mov eax, [per_y+esi*4]
        cmp eax, [jug_y]
        je .cogido
.ch1:
        inc esi
        cmp esi, N_PER
        jb .ch
.p3:
        cmp dword [quedan], 0
        je .limpio
        cmp dword [dormidos], 0     ; los erizos
        je .mueve
        dec dword [dormidos]
        call inv_pie
        cmp dword [dormidos], 0
        jne .fin
        call inv_erizos             ; se despiertan
        jmp .fin
.mueve:
        mov eax, [paso_n]
        and eax, 3
        cmp eax, 3
        je .fin
        xor esi, esi
.m1:
        call erizo
        mov eax, [per_x+esi*4]      ; ¿lo pincha? (en su casilla, o se han
        mov edx, [per_y+esi*4]      ; cruzado de frente)
        cmp eax, [jug_x]
        jne .m2
        cmp edx, [jug_y]
        je .cogido
.m2:
        cmp eax, [ant_x]
        jne .m3
        cmp edx, [ant_y]
        jne .m3
        mov eax, [pa_x]
        cmp eax, [jug_x]
        jne .m3
        mov eax, [pa_y]
        cmp eax, [jug_y]
        je .cogido
.m3:
        inc esi
        cmp esi, N_PER
        jb .m1
.fin:
        inc dword [paso_n]
        popad
        ret
.cogido:
        dec dword [vidas]
        jnz .otra
        mov dword [estado], E_FIN
        call rec_apuntar
        jmp .para
.otra:
        mov byte [aviso], A_PINCHAZO
        call colocar
        jmp .listo
.limpio:
        mov eax, [nivel]
        imul eax, 100
        add [puntos], eax
        inc dword [nivel]
        mov byte [aviso], A_LIMPIO
        call jardin
.listo:
        mov dword [estado], E_LISTO
.para:
        mov ecx, 1
        HV tmp_quitar
        HV ven_invalidar
        jmp .fin

; mueve el erizo ESI una casilla. No da media vuelta si puede seguir o
; girar. El granate (0) elige la salida que más lo acerca al Tragón (una de
; cada seis veces, cualquiera); el añil (1), cualquiera; el marrón (2), la
; que más lo acerca a cuatro casillas por delante del Tragón.
erizo:
        pushad
        mov eax, [per_x+esi*4]
        mov [pa_x], eax
        mov eax, [per_y+esi*4]
        mov [pa_y], eax
        mov eax, [jug_x]
        mov edx, [jug_y]
        cmp esi, 2
        jne .o1
        mov ecx, [dir_x]
        lea eax, [eax+ecx*4]
        mov ecx, [dir_y]
        lea edx, [edx+ecx*4]
.o1:
        mov [obj_x], eax
        mov [obj_y], edx
        mov byte [al_azar], 0
        cmp esi, 1
        je .az
        test esi, esi
        jnz .o2
        mov eax, 6
        HV azar
        test eax, eax
        jnz .o2
.az:
        mov byte [al_azar], 1
.o2:
        mov dword [mejor], -1
        mov dword [mejor_p], 0x7FFFFFFF
        xor edi, edi
.d:
        movsx ebx, byte [dirs+edi*2]
        movsx ecx, byte [dirs+edi*2+1]
        mov eax, ebx                ; media vuelta, no
        add eax, [per_dx+esi*4]
        mov edx, ecx
        add edx, [per_dy+esi*4]
        or eax, edx
        jz .sig
        mov eax, [pa_x]
        add eax, ebx
        mov edx, [pa_y]
        add edx, ecx
        call es_seto
        je .sig
        cmp byte [al_azar], 0
        je .dist
        mov eax, 1000
        HV azar
        jmp .cmp
.dist:
        sub eax, [obj_x]
        jns .d1
        neg eax
.d1:
        sub edx, [obj_y]
        jns .d2
        neg edx
.d2:
        add eax, edx
.cmp:
        cmp eax, [mejor_p]
        jge .sig
        mov [mejor_p], eax
        mov [mejor], edi
.sig:
        inc edi
        cmp edi, 4
        jb .d
        mov edi, [mejor]
        cmp edi, -1
        jne .va
        neg dword [per_dx+esi*4]    ; sin salida: media vuelta
        neg dword [per_dy+esi*4]
        jmp .mv
.va:
        movsx ebx, byte [dirs+edi*2]
        movsx ecx, byte [dirs+edi*2+1]
        mov [per_dx+esi*4], ebx
        mov [per_dy+esi*4], ecx
.mv:
        mov eax, [pa_x]
        add eax, [per_dx+esi*4]
        mov edx, [pa_y]
        add edx, [per_dy+esi*4]
        call es_seto
        je .x
        mov [per_x+esi*4], eax
        mov [per_y+esi*4], edx
        call inv_xy
        mov eax, [pa_x]
        mov edx, [pa_y]
        call inv_xy
.x:
        popad
        ret

; ---- las cinco mejores puntuaciones ----
rec_apuntar:
        pushad
        mov eax, [puntos]
        test eax, eax
        jz .x
        xor ecx, ecx
.b:
        cmp eax, [rec+ecx*4]
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
        mov esi, [rec+edx*4-4]
        mov [rec+edx*4], esi
        dec edx
        jmp .d
.p:
        mov [rec+ecx*4], eax
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
        mov esi, k_puntos
        call hvl_cad
        xor ecx, ecx
.f1:
        mov eax, [rec+ecx*4]
        call hvl_num
        mov al, ','
        stosb
        inc ecx
        cmp ecx, 5
        jb .f1
        dec edi
        mov ax, 0x0A0D
        stosw
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
        mov ecx, 300
        call hvl_leer
        test eax, eax
        jz .x
        mov esi, txt
        mov edi, k_puntos
        call hvl_clave
        jc .x
        xor ecx, ecx
.f1:
        call hvl_lee_num
        mov [rec+ecx*4], eax
        cmp byte [esi], ','
        jne .x
        inc esi
        inc ecx
        cmp ecx, 5
        jb .f1
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
        mov esi, t_puntos
        mov eax, [puntos]
        mov ebx, 8
        call .rotulo
        mov esi, t_nivel
        mov eax, [nivel]
        mov ebx, 144
        call .rotulo
        mov esi, t_vidas
        mov eax, [vidas]
        mov ebx, 248
        call .rotulo
        mov esi, t_record
        mov eax, [rec]
        mov ebx, 344
        call .rotulo
        ; el marco del jardín
        COLOR C_MARRON
        mov eax, TAB_X - 2
        mov ebx, TAB_Y - 2
        mov ecx, TAB_AN + 4
        mov edx, TAB_AL + 4
        HV dc_marco
        COLOR C_OLIVA
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
        ; el renglón de abajo
        mov eax, [estado]
        cmp eax, E_LISTO
        jne .pj
        mov esi, t_listo
        cmp byte [aviso], A_EMPEZAR
        je .pie
        mov esi, t_pinchazo
        cmp byte [aviso], A_PINCHAZO
        je .pie
        mov esi, t_limpio
        jmp .pie
.pj:
        cmp eax, E_JUEGA
        jne .cart
        cmp dword [dormidos], 0
        je .x
        mov edi, rot
        mov esi, t_duermen
        call hvl_cad
        mov eax, [dormidos]
        call hvl_num
        mov byte [edi], 0
        mov esi, rot
.pie:
        COLOR C_MARRON
        mov eax, TAB_X
        mov ebx, PIE_Y
        HV dc_texto
        jmp .x
.cart:
        ; el cartel
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
        COLOR C_OLIVA
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
; (interna) el rótulo ESI con el número EAX, en EBX, 8
.rotulo:
        mov edi, rot
        call hvl_cad
        call hvl_num
        mov byte [edi], 0
        mov eax, ebx
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

; dibuja la casilla ECX (columna), EDX (fila) con lo que tenga encima
celda:
        pushad
        mov [c_col], ecx
        mov [c_fil], edx
        imul eax, ecx, CEL
        add eax, TAB_X
        mov [c_x], eax
        imul ebx, edx, CEL
        add ebx, TAB_Y
        mov [c_y], ebx
        imul edi, edx, COLS
        add edi, ecx
        movzx esi, byte [mapa+edi]
        cmp esi, M_SETO
        je .seto
        COLOR C_CREMA               ; el suelo
        RC 0, 0, CEL, CEL
        cmp esi, M_BOLA
        jne .s1
        COLOR C_OCRE                ; la bolita
        RC 8, 8, 4, 4
        RC 9, 7, 2, 6
        RC 7, 9, 6, 2
        jmp .gente
.s1:
        cmp esi, M_CAMPANA
        jne .gente
        COLOR C_OCRE                ; la campana
        RC 8, 3, 4, 2
        RC 6, 5, 8, 8
        RC 4, 13, 12, 2
        COLOR C_BLANCO
        RC 7, 6, 1, 5
        COLOR C_MARRON
        RC 9, 15, 2, 2
        RC 9, 2, 2, 1
        jmp .gente
.seto:
        COLOR C_OLIVA               ; el seto, con sus hojas
        RC 0, 0, CEL, CEL
        COLOR C_PETROLEO
        RC 3, 4, 2, 1
        RC 13, 2, 2, 1
        RC 8, 9, 2, 1
        RC 16, 12, 2, 1
        RC 3, 15, 2, 1
        RC 11, 17, 2, 1
        COLOR C_TURQUESA
        RC 4, 3, 1, 1
        RC 14, 1, 1, 1
        RC 9, 8, 1, 1
        RC 17, 11, 1, 1
        RC 4, 14, 1, 1
        RC 12, 16, 1, 1
        jmp .x
.gente:
        xor esi, esi                ; los erizos que haya en la casilla
.g1:
        mov eax, [per_x+esi*4]
        cmp eax, [c_col]
        jne .g2
        mov eax, [per_y+esi*4]
        cmp eax, [c_fil]
        jne .g2
        call .erizo
.g2:
        inc esi
        cmp esi, N_PER
        jb .g1
        mov eax, [jug_x]            ; y el Tragón, encima
        cmp eax, [c_col]
        jne .x
        mov eax, [jug_y]
        cmp eax, [c_fil]
        jne .x
        COLOR C_PETROLEO
        RC 5, 2, 10, 16
        RC 2, 5, 16, 10
        RC 3, 3, 14, 14
        COLOR C_TURQUESA            ; el brillo
        RC 4, 4, 2, 1
        RC 4, 5, 1, 1
        COLOR C_BLANCO              ; los ojos, que miran hacia donde va
        RC 5, 6, 4, 4
        RC 11, 6, 4, 4
        COLOR C_TINTA
        mov esi, [dir_x]
        mov edi, [dir_y]
        lea eax, [esi+6]
        lea ebx, [edi+7]
        call .punto2
        lea eax, [esi+12]
        lea ebx, [edi+7]
        call .punto2
        COLOR C_TERRACOTA_CL        ; la boca
        RC 7, 13, 6, 2
.x:
        popad
        ret
; (interna) un cuadrito de 2x2 en EAX, EBX de la casilla
.punto2:
        add eax, [c_x]
        add ebx, [c_y]
        mov ecx, 2
        mov edx, 2
        HV dc_rect
        ret
; (interna) el erizo ESI: de su color, o gris y con los ojos cerrados
.erizo:
        movzx eax, byte [per_color+esi]
        cmp dword [dormidos], 0
        je .e1
        mov eax, C_ARENA_OSC
.e1:
        HV dc_color
        RC 2, 9, 16, 8              ; el cuerpo
        RC 4, 7, 12, 2
        RC 4, 17, 12, 1
        RC 3, 6, 2, 3               ; las púas
        RC 7, 3, 2, 4
        RC 11, 3, 2, 4
        RC 15, 6, 2, 3
        COLOR C_TINTA
        RC 4, 18, 3, 1              ; las patas
        RC 13, 18, 3, 1
        RC 7, 2, 2, 1               ; las puntas de las púas
        RC 11, 2, 2, 1
        RC 3, 5, 2, 1
        RC 15, 5, 2, 1
        cmp dword [dormidos], 0
        jne .e2
        COLOR C_BLANCO
        RC 5, 11, 3, 3
        RC 12, 11, 3, 3
        COLOR C_TINTA
        RC 6, 12, 1, 1
        RC 13, 12, 1, 1
        ret
.e2:
        RC 5, 12, 3, 1              ; dormido
        RC 12, 12, 3, 1
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
jug_x       resd 1                  ; el Tragón: dónde está
jug_y       resd 1
dir_x       resd 1                  ; hacia dónde va
dir_y       resd 1
pend_x      resd 1                  ; hacia dónde se le ha pedido que vaya
pend_y      resd 1
ant_x       resd 1                  ; dónde estaba antes de este paso
ant_y       resd 1
per_x       resd N_PER              ; los erizos
per_y       resd N_PER
per_dx      resd N_PER
per_dy      resd N_PER
pa_x        resd 1                  ; el erizo que se mueve: dónde estaba,
pa_y        resd 1
obj_x       resd 1                  ; a dónde quiere ir
obj_y       resd 1
mejor       resd 1                  ; y la mejor salida encontrada
mejor_p     resd 1
dormidos    resd 1                  ; pasos de sueño que les quedan
quedan      resd 1                  ; bolitas que quedan
puntos      resd 1
nivel       resd 1
vidas       resd 1
paso_n      resd 1
lab_ini     resd 1                  ; el jardín por el que se empieza (/N)
p_c0        resd 1
p_c1        resd 1
p_f0        resd 1
p_f1        resd 1
c_col       resd 1                  ; la casilla que se dibuja
c_fil       resd 1
c_x         resd 1
c_y         resd 1
rec         resd 5                  ; las cinco mejores puntuaciones
a_pasos     resb 1
nuevo_rec   resb 1
aviso       resb 1
al_azar     resb 1
ruta_rec    resb 80
rot         resb 64                 ; el rótulo que se está pintando
txt         resb 304
mapa        resb COLS * FILAS
HVP_FIN
