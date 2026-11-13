; =============================================================================
;  BLOQUES.HVP - Bloques, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Por un pozo de 11 x 18 casillas caen piezas de azulejos, de una en una; se
;  mueven y se giran para encajarlas abajo. Cada fila que queda completa
;  desaparece y lo de encima baja. La partida acaba cuando una pieza nueva ya
;  no cabe. Hay once piezas: dos de tres casillas (Barrita y Codo), siete de
;  cuatro (Losa, Viga, Gancho, Garfio, Peldaño, Escalón y Pico) y dos de
;  cinco (Cruz y Cazo). Una sombra marca dónde caería la pieza.
;
;  Puntos: uno por cada casilla de la pieza encajada y uno por cada fila que
;  baja al soltarla de golpe; 20, 50, 90 o 140 por quitar una, dos, tres o
;  cuatro filas a la vez, multiplicados por el nivel. Cada ocho filas sube el
;  nivel (hasta el 12) y las piezas caen más deprisa. Desde el nivel 5, cada
;  diez piezas sube del fondo una fila de piedra con un hueco (se quita como
;  las demás, completándola).
;
;  Teclado: flechas izquierda y derecha mueven, arriba gira, abajo baja una
;  casilla, Espacio la suelta de golpe; P pausa (también se para sola al
;  minimizar la ventana); F2 otra partida; Intro empieza (y, acabada, otra).
;  Ratón: clic gira, botón derecho suelta. Menú Juego: nivel inicial (1, 4 o
;  7) y las cinco mejores puntuaciones, que se guardan en BLOQUES.REC (al
;  lado de BLOQUES.HVP; texto).
;
;  Todo está hecho aquí: las piezas, los azulejos (cada pieza con su color y
;  un rombo en el centro), el pozo punteado, el panel, el icono y los textos.
;  Es un módulo .HVP (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /S=n   semilla de los números aleatorios (salen siempre las mismas piezas)
;    /P     paso a paso: la pieza no cae sola (solo con la flecha abajo o
;           soltándola)
; =============================================================================
%include "hvp.inc"

COLS        equ 11
FILAS       equ 18
CEL         equ 20
POZ_X       equ 14
POZ_Y       equ 12
POZ_AN      equ COLS * CEL
POZ_AL      equ FILAS * CEL
PAN_X       equ POZ_X + POZ_AN + 14 ; el panel de la derecha
PAN_AN      equ 148
CLI_AN      equ PAN_X + PAN_AN + 12
CLI_AL      equ POZ_Y + POZ_AL + 12
SIG_Y       equ 32                  ; la caja de la pieza siguiente
SIG_AL      equ 92
DAT_Y       equ 136                 ; el primer dato (puntos) y lo que hay
DAT_PASO    equ 50                  ; de uno a otro
CAR_AN      equ 204                 ; el cartel del centro del pozo
CAR_AL      equ 58
CAR_X       equ POZ_X + (POZ_AN - CAR_AN) / 2
CAR_Y       equ POZ_Y + 120
N_PIEZAS    equ 11
V_PIEDRA    equ 12                  ; (en el mapa) casilla de una fila de piedra
NIVEL_MAX   equ 12
NIVEL_PIEDRA equ 5                  ; desde este nivel suben filas de piedra
CADA_PIEDRA equ 10                  ; cada tantas piezas
FILAS_NIVEL equ 8                   ; filas para subir de nivel
E_LISTO     equ 0                   ; esperando para empezar
E_JUEGA     equ 1
E_PAUSA     equ 2
E_FIN       equ 3

HVP_CABECERA "Bloques", entrada
        db "................................"
        db "1..............................1"
        db "1........0000000...............1"
        db "1........0444440...............1"
        db "1........0444440...............1"
        db "1........044a440...............1"
        db "1........0444440...............1"
        db "1........0444440...............1"
        db "1........0000000...............1"
        db "1.000000000000000000000........1"
        db "1.044444004444400444440........1"
        db "1.044444004444400444440........1"
        db "1.044a440044a440044a440........1"
        db "1.044444004444400444440........1"
        db "1.044444004444400444440........1"
        db "1.000000000000000000000........1"
        db "1.0000000..............0000000.1"
        db "1.0aaaaa0..............0222220.1"
        db "1.0aaaaa0..............0222220.1"
        db "1.0aa4aa0..............022e220.1"
        db "1.0aaaaa0..............0222220.1"
        db "1.0aaaaa0..............0222220.1"
        db "1.0000000..............0000000.1"
        db "1.0000000000000000000000000000.1"
        db "1.022222002222200bbbbb00222220.1"
        db "1.022222002222200bbbbb00222220.1"
        db "1.022e220022e2200bbabb0022e220.1"
        db "1.022222002222200bbbbb00222220.1"
        db "1.022222002222200bbbbb00222220.1"
        db "1.0000000000000000000000000000.1"
        db "11111111111111111111111111111111"
        db "11111111111111111111111111111111"
HVP_CODIGO

c_bloques   dd blo_proc
            db SIN_FONDO, 0, 0, 0   ; (se pinta todo aquí: no parpadea)
blo_plant:
        PLANTILLA c_bloques, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 150, 60, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

blo_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida", 0
        dw 102
        db "&Pausa", 0
        dw 0
        db 0
        dw 103
        db "Empezar en el nivel &1", 0
        dw 104
        db "Empezar en el nivel &4", 0
        dw 105
        db "Empezar en el nivel &7", 0
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

; las piezas: lado del cuadro en el que giran, casillas, y cinco pares
; columna, fila (dentro del cuadro). Doce octetos cada una.
formas:
        db 3, 3,  0,1, 1,1, 2,1, 0,0, 0,0       ;  1 Barrita
        db 2, 3,  0,0, 0,1, 1,1, 0,0, 0,0       ;  2 Codo
        db 2, 4,  0,0, 1,0, 0,1, 1,1, 0,0       ;  3 Losa
        db 4, 4,  0,1, 1,1, 2,1, 3,1, 0,0       ;  4 Viga
        db 3, 4,  0,1, 1,1, 2,1, 2,0, 0,0       ;  5 Gancho
        db 3, 4,  0,0, 0,1, 1,1, 2,1, 0,0       ;  6 Garfio
        db 3, 4,  1,0, 2,0, 0,1, 1,1, 0,0       ;  7 Peldaño
        db 3, 4,  0,0, 1,0, 1,1, 2,1, 0,0       ;  8 Escalón
        db 3, 4,  1,0, 0,1, 1,1, 2,1, 0,0       ;  9 Pico
        db 3, 5,  1,0, 0,1, 1,1, 2,1, 1,2       ; 10 Cruz
        db 3, 5,  0,0, 2,0, 0,1, 1,1, 2,1       ; 11 Cazo
; los azulejos: color, canto claro, canto oscuro y rombo del centro
estilo:
        db C_TURQUESA, C_BLANCO, C_PETROLEO, C_PETROLEO
        db C_OCRE, C_CREMA, C_MARRON, C_TERRACOTA
        db C_TERRACOTA, C_TERRACOTA_CL, C_GRANATE, C_OCRE
        db C_PETROLEO, C_PETROLEO_CL, C_TINTA, C_TURQUESA
        db C_OLIVA, C_PIEDRA, C_TINTA, C_OCRE
        db C_ANIL, C_PETROLEO_CL, C_TINTA, C_BLANCO
        db C_GRANATE, C_TERRACOTA_CL, C_TINTA, C_OCRE
        db C_ARENA_OSC, C_ARENA, C_MARRON, C_MARRON
        db C_PETROLEO_CL, C_BLANCO, C_PETROLEO, C_ANIL
        db C_TERRACOTA_CL, C_BLANCO, C_TERRACOTA, C_GRANATE
        db C_MARRON, C_ARENA_OSC, C_TINTA, C_OCRE
        db C_PIEDRA, C_BLANCO, C_ARENA_OSC, C_ARENA_OSC     ; la piedra
premio      db 20, 50, 90, 140, 200 ; por una, dos, tres... filas a la vez
piedra_pt   db 5, 6,  12, 5,  8, 12,  13, 13    ; las motas de la piedra

t_titulo    db "Bloques", 0
t_sig       db "Siguiente", 0
t_puntos    db "Puntos", 0
t_filas     db "Filas", 0
t_nivel     db "Nivel", 0
t_record    db "Récord", 0
t_pie1      db "P: pausa", 0
t_pie2      db "F2: otra partida", 0
t_listo     db "Intro: empezar", 0
t_pausa     db "Pausa", 0
t_pausa2    db "P para seguir", 0
t_fin       db "Fin de la partida", 0
t_nuevo     db "¡Nuevo récord!", 0
t_fin2      db "Intro: otra partida", 0
t_rec_tit   db "Mejores puntuaciones", 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Encaje las piezas que caen: cada fila completa desaparece.", 10
            db "Flechas izquierda y derecha: mover.  Arriba o clic: girar.", 10
            db "Abajo: bajar.  Espacio o botón derecho: soltar de golpe.", 10
            db 10
            db "Puntos: uno por casilla encajada y por fila que baja al", 10
            db "soltar; 20, 50, 90 o 140 por una, dos, tres o cuatro filas", 10
            db "a la vez, por el nivel. Cada ocho filas sube el nivel.", 10
            db "Desde el nivel 5, cada diez piezas sube una fila de piedra.", 10
            db 10
            db "P: pausa.    F2: otra partida.", 0
k_cab       db "Bloques - mejores puntuaciones (Hispa Ventanas)", 13, 10, 0
k_puntos    db "Puntos=", 0
k_nivel     db "Nivel=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; BLOQUES.REC, al lado del módulo
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
        mov byte [nivel_ini], 1
        call rec_leer
        mov esi, blo_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
blo_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .letra
        cmp eax, WM_LBUTTONDOWN
        je .clic
        cmp eax, WM_LBUTTONDBLCLK
        je .clic
        cmp eax, WM_RBUTTONDOWN
        je .derecho
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], blo_menu
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
        call bajar
        jmp .hecho
.clic:
        cmp dword [estado], E_LISTO
        je .empezar
        cmp dword [estado], E_JUEGA
        jne .hecho
        call girar
        jmp .hecho
.derecho:
        cmp dword [estado], E_JUEGA
        jne .hecho
        call soltar
        jmp .hecho
.letra:
        cmp ecx, ' '
        jne .l1
        cmp dword [estado], E_LISTO
        je .empezar
        cmp dword [estado], E_JUEGA
        jne .hecho
        call soltar
        jmp .hecho
.l1:
        and ecx, 0xDF
        cmp ecx, 'P'
        je .pausa
        jmp .hecho
.tecla:
        cmp ecx, TC_F1 + 1          ; F2
        je .nueva
        mov eax, [estado]
        cmp ecx, TC_INTRO
        jne .t1
        cmp eax, E_FIN
        je .nueva
        cmp eax, E_LISTO
        je .empezar
        jmp .hecho
.t1:
        cmp ecx, TC_IZQ
        je .flecha
        cmp ecx, TC_DER
        je .flecha
        cmp ecx, TC_ARRIBA
        je .flecha
        cmp ecx, TC_ABAJO
        jne .hecho
.flecha:
        cmp eax, E_LISTO            ; la primera flecha solo empieza
        je .empezar
        cmp eax, E_JUEGA
        jne .hecho
        cmp ecx, TC_ARRIBA
        jne .t2
        call girar
        jmp .hecho
.t2:
        cmp ecx, TC_ABAJO
        jne .t3
        call bajar
        jmp .hecho
.t3:
        mov eax, 1
        cmp ecx, TC_DER
        je .t4
        neg eax
.t4:
        call mover
        jmp .hecho
.empezar:
        mov dword [estado], E_JUEGA
        HV ven_invalidar
        call poner_reloj
        jmp .hecho
.orden:
        cmp ecx, 101
        jne .o2
.nueva:
        call nueva
        jmp .hecho
.o2:
        cmp ecx, 102
        jne .o3
.pausa:
        call pausa
        jmp .hecho
.o3:
        cmp ecx, 107
        je .records
        ja .o8
        sub ecx, 103                ; 103-105: el nivel inicial (1, 4, 7)
        jb .hecho
        lea ecx, [ecx+ecx*2+1]
        mov [nivel_ini], cl
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
        mov edi, txt                ; la lista: «1.  nnn»
        mov ebx, rec
        mov ecx, 1
.r1:
        mov eax, ecx
        call hvl_num
        mov eax, '.   '
        stosd
        mov eax, [ebx]
        add ebx, 4
        call hvl_num
        mov al, 10
        stosb
        inc ecx
        cmp ecx, 5
        jbe .r1
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

; el reloj de la caída, según el nivel (no en paso a paso)
poner_reloj:
        cmp byte [a_pasos], 0
        jne .x
        pushad
        mov ebp, [ven]
        imul edx, [nivel], 50
        neg edx
        add edx, 660
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
        xor eax, eax
        rep stosb
        mov [puntos], eax
        mov [filas], eax
        mov [puestas], eax
        mov [nuevo_rec], al
        movzx eax, byte [nivel_ini]
        mov [nivel], eax
        mov dword [estado], E_LISTO
        mov ecx, 1
        HV tmp_quitar
        mov eax, N_PIEZAS
        HV azar
        mov [sig_tipo], eax
        call pieza_nueva
        HV ven_invalidar
        popad
        ret

; la pieza siguiente entra por arriba y se sortea otra; si no cabe, se acabó
pieza_nueva:
        pushad
        mov eax, [sig_tipo]
        mov [pz_tipo], eax
        imul esi, eax, 12
        add esi, formas
        mov eax, N_PIEZAS
        HV azar
        mov [sig_tipo], eax
        movzx eax, byte [esi]
        mov [pz_lado], eax
        movzx ecx, byte [esi+1]
        mov [pz_n], ecx
        add esi, 2
        mov edi, pz_cel
        mov ecx, 10
        rep movsb
        mov edx, COLS
        sub edx, eax
        shr edx, 1
        mov [pz_x], edx
        xor eax, eax
        mov [pz_y], eax
        mov [som_y], eax
        call choca
        jnc .ok
        mov dword [estado], E_FIN   ; no cabe: fin de la partida
        mov ebp, [ven]
        mov ecx, 1
        HV tmp_quitar
        call rec_apuntar
        HV ven_invalidar
        jmp .x
.ok:
        call calc_sombra
        call inv_pieza
.x:
        call inv_panel
        popad
        ret

; -> CF=1 si la pieza, donde está, se sale del pozo o pisa algo
choca:
        pushad
        mov ecx, [pz_n]
        mov esi, pz_cel
.b:
        movzx eax, byte [esi]
        add eax, [pz_x]
        movzx ebx, byte [esi+1]
        add ebx, [pz_y]
        cmp eax, COLS               ; (sin signo: también por la izquierda)
        jae .si
        cmp ebx, FILAS
        jae .si
        imul ebx, COLS
        cmp byte [mapa+ebx+eax], 0
        jne .si
        add esi, 2
        dec ecx
        jnz .b
        popad
        clc
        ret
.si:
        popad
        stc
        ret

; la sombra: la fila hasta la que caería la pieza
calc_sombra:
        push eax
        push dword [pz_y]
.b:
        inc dword [pz_y]
        call choca
        jnc .b
        mov eax, [pz_y]
        dec eax
        mov [som_y], eax
        pop dword [pz_y]
        pop eax
        ret

; hay que repintar las casillas de la pieza y las de su sombra
inv_pieza:
        pushad
        mov edi, [pz_n]
        mov esi, pz_cel
.b:
        movzx ecx, byte [esi]
        add ecx, [pz_x]
        movzx edx, byte [esi+1]
        push edx
        add edx, [pz_y]
        call inv_celda
        pop edx
        add edx, [som_y]
        call inv_celda
        add esi, 2
        dec edi
        jnz .b
        popad
        ret

; la casilla ECX (columna), EDX (fila) hay que repintarla
inv_celda:
        cmp ecx, COLS
        jae .x
        cmp edx, FILAS
        jae .x
        pushad
        imul eax, ecx, CEL
        add eax, POZ_X
        imul ebx, edx, CEL
        add ebx, POZ_Y
        mov ecx, CEL
        mov edx, CEL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
.x:
        ret

; el panel de la derecha hay que repintarlo
inv_panel:
        pushad
        mov eax, PAN_X
        xor ebx, ebx
        mov ecx, PAN_AN
        mov edx, CLI_AL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; ---- los movimientos ----
; EAX=-1 a la izquierda, 1 a la derecha (si cabe)
mover:
        call inv_pieza
        add [pz_x], eax
        call choca
        jnc .ok
        sub [pz_x], eax
.ok:
        call calc_sombra
        jmp inv_pieza

; un cuarto de vuelta a la derecha dentro de su cuadro; si no cabe, prueba
; una casilla a la derecha y una a la izquierda; si tampoco, no gira
girar:
        pushad
        call inv_pieza
        mov esi, pz_cel
        mov edi, pz_ant
        mov ecx, 10
        rep movsb
        mov ecx, [pz_n]
        mov esi, pz_cel
        mov edx, [pz_lado]
        dec edx
.g:
        mov al, [esi]               ; (columna, fila) -> (lado-1-fila, columna)
        mov ah, [esi+1]
        mov bl, dl
        sub bl, ah
        mov [esi], bl
        mov [esi+1], al
        add esi, 2
        dec ecx
        jnz .g
        call choca
        jnc .ok
        inc dword [pz_x]
        call choca
        jnc .ok
        sub dword [pz_x], 2
        call choca
        jnc .ok
        inc dword [pz_x]
        mov esi, pz_ant
        mov edi, pz_cel
        mov ecx, 10
        rep movsb
.ok:
        call calc_sombra
        call inv_pieza
        popad
        ret

; una casilla abajo; si no puede, se encaja
bajar:
        call inv_pieza
        inc dword [pz_y]
        call choca
        jc .enc
        jmp inv_pieza
.enc:
        dec dword [pz_y]
        jmp encajar

; de golpe hasta la sombra: un punto por fila
soltar:
        push eax
        call inv_pieza
        mov eax, [som_y]
        sub eax, [pz_y]
        add [puntos], eax
        mov eax, [som_y]
        mov [pz_y], eax
        pop eax
; la pieza se queda donde está: al mapa, filas completas y pieza nueva
encajar:
        pushad
        mov ebp, [ven]
        mov ecx, [pz_n]
        add [puntos], ecx           ; un punto por casilla
        mov esi, pz_cel
        mov edx, [pz_tipo]
        inc edx
.b:
        movzx eax, byte [esi]
        add eax, [pz_x]
        movzx ebx, byte [esi+1]
        add ebx, [pz_y]
        imul ebx, COLS
        mov [mapa+ebx+eax], dl
        add esi, 2
        dec ecx
        jnz .b
        call inv_pieza
        call quitar_filas           ; EAX=cuántas
        test eax, eax
        jz .sin
        add [filas], eax
        movzx eax, byte [premio+eax-1]
        imul eax, [nivel]
        add [puntos], eax
        mov eax, [filas]            ; el nivel: el inicial y uno por cada ocho
        xor edx, edx
        mov ecx, FILAS_NIVEL
        div ecx
        movzx ecx, byte [nivel_ini]
        add eax, ecx
        cmp eax, NIVEL_MAX
        jbe .n1
        mov eax, NIVEL_MAX
.n1:
        cmp eax, [nivel]
        je .n2
        mov [nivel], eax
        call poner_reloj
.n2:
        HV ven_invalidar
.sin:
        inc dword [puestas]
        cmp dword [nivel], NIVEL_PIEDRA
        jb .p
        mov eax, [puestas]
        xor edx, edx
        mov ecx, CADA_PIEDRA
        div ecx
        test edx, edx
        jnz .p
        call fila_piedra
.p:
        call pieza_nueva
        popad
        ret

; quita las filas completas y baja lo de encima -> EAX=cuántas ha quitado
quitar_filas:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        xor eax, eax
        mov edx, FILAS - 1          ; la fila que se mira
        mov ebx, FILAS - 1          ; y dónde se queda
.f:
        imul esi, edx, COLS
        add esi, mapa
        xor ecx, ecx
.c:
        cmp byte [esi+ecx], 0
        je .no
        inc ecx
        cmp ecx, COLS
        jb .c
        inc eax                     ; completa: no se copia
        jmp .sig
.no:
        cmp edx, ebx
        je .mismo
        imul edi, ebx, COLS
        add edi, mapa
        mov ecx, COLS
        rep movsb
.mismo:
        dec ebx
.sig:
        dec edx
        jns .f
.v:
        test ebx, ebx               ; arriba quedan tantas vacías
        js .x
        imul edi, ebx, COLS
        add edi, mapa
        mov ecx, COLS
        push eax
        xor al, al
        rep stosb
        pop eax
        dec ebx
        jmp .v
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; todo sube una fila y por abajo entra una de piedra con un hueco
fila_piedra:
        pushad
        mov esi, mapa + COLS
        mov edi, mapa
        mov ecx, COLS * (FILAS - 1)
        rep movsb
        mov al, V_PIEDRA
        mov ecx, COLS
        rep stosb
        mov eax, COLS
        HV azar
        mov byte [mapa+COLS*(FILAS-1)+eax], 0
        mov ebp, [ven]
        HV ven_invalidar
        popad
        ret

; ---- las mejores puntuaciones: cinco ----
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
        mov ebx, rec
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
        mov ax, 0x0A0D
        stosw
        mov esi, k_nivel
        call hvl_cad
        movzx eax, byte [nivel_ini]
        call hvl_num
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
        jc .n
        mov ebx, rec
        mov ecx, 5
.f1:
        call hvl_lee_num
        mov [ebx], eax
        add ebx, 4
        cmp byte [esi], ','
        jne .n
        inc esi
        dec ecx
        jnz .f1
.n:
        mov esi, txt
        mov edi, k_nivel
        call hvl_clave
        jc .x
        call hvl_lee_num
        cmp eax, 1
        je .n1
        cmp eax, 4
        je .n1
        cmp eax, 7
        jne .x
.n1:
        mov [nivel_ini], al
.x:
        popad
        ret

; ---- el dibujo ----
; (interna) de puntos a casillas: ECX=primer punto EDX=último+1 (ya sin el
; origen del pozo) ESI=casillas que hay -> ECX=primera EDX=última
rango:
        push eax
        push ebx
        mov ebx, CEL
        test ecx, ecx
        jns .a
        xor ecx, ecx
.a:
        mov eax, ecx
        push edx
        xor edx, edx
        div ebx
        mov ecx, eax
        pop edx
        dec edx
        js .neg
        mov eax, edx
        xor edx, edx
        div ebx
        mov edx, eax
        cmp edx, esi
        jl .b
        lea edx, [esi-1]
        jmp .b
.neg:
        mov edx, -1
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
        sub ecx, POZ_X
        sub edx, POZ_X
        mov esi, COLS
        call rango
        mov [p_c0], ecx
        mov [p_c1], edx
        mov ecx, [ebp+V_UY0]
        mov edx, [ebp+V_UY1]
        sub ecx, ebx
        sub edx, ebx
        sub ecx, POZ_Y
        sub edx, POZ_Y
        mov esi, FILAS
        call rango
        mov [p_f0], ecx
        mov [p_f1], edx
        HV dc_pintar
        ; el fondo, alrededor del pozo
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, CLI_AN
        mov edx, POZ_Y - 2
        HV dc_rect
        mov ebx, POZ_Y + POZ_AL + 2
        mov edx, CLI_AL - (POZ_Y + POZ_AL + 2)
        HV dc_rect
        mov ebx, POZ_Y - 2
        mov ecx, POZ_X - 2
        mov edx, POZ_AL + 4
        HV dc_rect
        mov eax, POZ_X + POZ_AN + 2
        mov ecx, CLI_AN - (POZ_X + POZ_AN + 2)
        HV dc_rect
        ; el marco del pozo
        COLOR C_MARRON
        mov eax, POZ_X - 2
        mov ebx, POZ_Y - 2
        mov ecx, POZ_AN + 4
        mov edx, POZ_AL + 4
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        ; el panel: la pieza siguiente
        COLOR C_TINTA
        mov eax, PAN_X
        mov ebx, 12
        mov esi, t_sig
        HV dc_texto
        COLOR C_CREMA
        mov ebx, SIG_Y
        mov ecx, PAN_AN
        mov edx, SIG_AL
        HV dc_rect
        COLOR C_MARRON
        HV dc_marco
        cmp dword [estado], E_PAUSA ; (en pausa no se enseña)
        je .datos
        mov edi, [sig_tipo]
        imul edi, 12
        add edi, formas
        movzx edx, byte [edi]       ; centrada según el lado de su cuadro
        imul edx, CEL
        mov eax, PAN_AN
        sub eax, edx
        shr eax, 1
        add eax, PAN_X
        mov ebx, SIG_AL
        sub ebx, edx
        shr ebx, 1
        add ebx, SIG_Y
        cmp byte [edi+1], 4         ; (las de cuatro en un cuadro de tres o
        jne .s1                     ; de cuatro están arriba: media más abajo)
        cmp byte [edi], 3
        jb .s1
        add ebx, CEL / 2
.s1:
        movzx ecx, byte [edi+1]
        add edi, 2
        mov esi, [sig_tipo]
        inc esi
.s:
        push eax
        push ebx
        movzx edx, byte [edi]
        imul edx, CEL
        add eax, edx
        movzx edx, byte [edi+1]
        imul edx, CEL
        add ebx, edx
        call azulejo
        pop ebx
        pop eax
        add edi, 2
        dec ecx
        jnz .s
.datos:
        mov esi, t_puntos
        mov edx, [puntos]
        mov ebx, DAT_Y
        call .dato
        mov esi, t_filas
        mov edx, [filas]
        mov ebx, DAT_Y + DAT_PASO
        call .dato
        mov esi, t_nivel
        mov edx, [nivel]
        mov ebx, DAT_Y + 2*DAT_PASO
        call .dato
        mov esi, t_record
        mov edx, [rec]
        mov ebx, DAT_Y + 3*DAT_PASO
        call .dato
        COLOR C_MARRON
        mov eax, PAN_X
        mov ebx, DAT_Y + 4*DAT_PASO + 2
        mov esi, t_pie1
        HV dc_texto
        add ebx, 18
        mov esi, t_pie2
        HV dc_texto
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
; (interna) un dato del panel: ESI=rótulo EDX=valor EBX=altura
.dato:
        COLOR C_TINTA
        mov eax, PAN_X
        HV dc_texto
        add ebx, 18
        push edx
        COLOR C_BLANCO
        mov ecx, PAN_AN
        mov edx, 22
        HV dc_rect
        COLOR C_ARENA_OSC
        HV dc_marco
        pop eax
        mov edi, rot
        call hvl_num
        mov byte [edi], 0
        COLOR C_TINTA
        NEGRITA 1
        mov eax, PAN_X + 8
        add ebx, 3
        mov esi, rot
        HV dc_texto
        NEGRITA 0
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

; -> CF=1 si la casilla ECX (columna), EDX (fila) es de la pieza puesta en
; la fila EDI (la suya o la de la sombra)
en_pieza:
        push eax
        push esi
        push ebp
        mov ebp, [pz_n]
        mov esi, pz_cel
.b:
        movzx eax, byte [esi]
        add eax, [pz_x]
        cmp eax, ecx
        jne .s
        movzx eax, byte [esi+1]
        add eax, edi
        cmp eax, edx
        je .si
.s:
        add esi, 2
        dec ebp
        jnz .b
        clc
        jmp .x
.si:
        stc
.x:
        pop ebp
        pop esi
        pop eax
        ret

; dibuja la casilla ECX (columna), EDX (fila) del pozo
celda:
        pushad
        imul eax, ecx, CEL
        add eax, POZ_X
        imul ebx, edx, CEL
        add ebx, POZ_Y
        mov edi, [pz_y]
        call en_pieza
        jnc .mapa
        mov esi, [pz_tipo]
        inc esi
        jmp .az
.mapa:
        imul edi, edx, COLS
        add edi, ecx
        movzx esi, byte [mapa+edi]
        test esi, esi
        jz .vacia
.az:
        call azulejo
        jmp .x
.vacia:
        mov edi, [som_y]
        call en_pieza
        setc byte [p_som]
        COLOR C_CREMA               ; vacía: crema con un punto en la esquina
        mov ecx, CEL
        mov edx, CEL
        HV dc_rect
        COLOR C_PIEDRA
        mov ecx, 2
        mov edx, 2
        HV dc_rect
        cmp byte [p_som], 0
        je .x
        cmp dword [estado], E_JUEGA
        ja .x
        COLOR C_ARENA_OSC           ; la sombra de la pieza: un recuadro
        add eax, 3
        add ebx, 3
        mov ecx, CEL - 6
        mov edx, CEL - 6
        HV dc_marco
.x:
        popad
        ret

; dibuja un azulejo en EAX, EBX; ESI=cuál (1-11 el de cada pieza, 12 piedra)
azulejo:
        pushad
        lea edi, [estilo+esi*4-4]
        movzx ecx, byte [edi]
        COLOR ecx
        mov ecx, CEL
        mov edx, CEL
        HV dc_rect
        movzx ecx, byte [edi+1]     ; el canto claro: arriba y a la izquierda
        COLOR ecx
        mov ecx, CEL - 1
        mov edx, 2
        HV dc_rect
        mov ecx, 2
        mov edx, CEL - 1
        HV dc_rect
        movzx ecx, byte [edi+2]     ; el oscuro: abajo y a la derecha
        COLOR ecx
        push eax
        push ebx
        inc eax
        add ebx, CEL - 2
        mov ecx, CEL - 1
        mov edx, 2
        HV dc_rect
        add eax, CEL - 3
        sub ebx, CEL - 3
        mov ecx, 2
        mov edx, CEL - 1
        HV dc_rect
        pop ebx
        pop eax
        movzx ecx, byte [edi+3]
        COLOR ecx
        cmp esi, V_PIEDRA
        je .piedra
        add eax, CEL / 2            ; el rombo del centro
        add ebx, CEL / 2
        mov ecx, 3
        HV dc_rombo
        jmp .x
.piedra:
        mov esi, piedra_pt          ; la piedra: cuatro motas
        mov edi, 4
        mov ecx, 2
        mov edx, 2
.m:
        push eax
        push ebx
        push ecx
        movzx ecx, byte [esi]
        add eax, ecx
        movzx ecx, byte [esi+1]
        add ebx, ecx
        pop ecx
        HV dc_rect
        pop ebx
        pop eax
        add esi, 2
        dec edi
        jnz .m
.x:
        popad
        ret

%include "hvplib.inc"

        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
pz_tipo     resd 1                  ; la pieza que cae (0-10)
pz_lado     resd 1                  ; el lado de su cuadro
pz_n        resd 1                  ; sus casillas
pz_x        resd 1                  ; dónde está su cuadro en el pozo
pz_y        resd 1
som_y       resd 1                  ; la fila hasta la que caería
sig_tipo    resd 1                  ; la siguiente
puntos      resd 1
filas       resd 1
nivel       resd 1
puestas     resd 1                  ; piezas encajadas
p_c0        resd 1
p_c1        resd 1
p_f0        resd 1
p_f1        resd 1
rec         resd 5                  ; las cinco mejores
nivel_ini   resb 1                  ; 1, 4 o 7
a_pasos     resb 1
nuevo_rec   resb 1
p_som       resb 1
pz_cel      resb 10                 ; columna y fila de cada casilla (girada)
pz_ant      resb 10
ruta_rec    resb 80
rot         resb 32                 ; el número que se está pintando
txt         resb 304
mapa        resb COLS * FILAS       ; 0 vacía, 1-11 la pieza, 12 piedra
HVP_FIN
