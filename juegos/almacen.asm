; =============================================================================
;  ALMACEN.HVP - Almacén, un juego de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El mozo del almacén tiene que dejar cada caja en una de las marcas del
;  suelo. Solo puede empujar (no tirar) y de una en una: una caja contra una
;  pared o contra otra caja no se mueve. Son 24 niveles, hechos aquí, cada
;  vez más difíciles; todos tienen solución (tools/almacen_niveles.py lo
;  comprueba y la busca).
;
;  Teclado: flechas, andar y empujar; Retroceso, U o Ctrl+Z, deshacer (todos
;  los pasos del nivel); R o F2, empezar el nivel otra vez; Av Pág y Re Pág,
;  nivel siguiente y anterior (se puede ir a los resueltos y al primero sin
;  resolver). Ratón: un clic en la fila o la columna del mozo lo hace dar un
;  paso hacia allí. Las mejores marcas (menos movimientos) y el nivel en que
;  se quedó se guardan en ALMACEN.REC (al lado de ALMACEN.HVP; texto).
;
;  Todo está hecho aquí: los niveles, el dibujo (baldosas de piedra,
;  estanterías, cajas de madera ocre que se ponen verdes en su marca, las
;  marcas en rombo, el mozo con mono añil y gorra granate), el icono y los
;  textos. Es un módulo .HVP (ventanas/hvp.inc).
;
;  Parámetros del acceso directo (para las pruebas):
;    /N=n   empieza en el nivel n y deja ir a cualquier nivel, esté o no
;           resuelto el anterior
; =============================================================================
%include "hvp.inc"

CEL         equ 28
MAXC        equ 16                  ; el nivel más grande que cabe
MAXF        equ 11
TAB_X       equ 8
TAB_Y       equ 34
TAB_AN      equ MAXC * CEL
TAB_AL      equ MAXF * CEL
CLI_AN      equ TAB_X * 2 + TAB_AN
CLI_AL      equ TAB_Y + TAB_AL + 8
MAX_NIV     equ 40
UNDO_MAX    equ 8192                ; pasos que se pueden deshacer
UNDO_TIRA   equ 1024                ; los que se olvidan si se llena
E_JUEGA     equ 0
E_HECHO     equ 1                   ; nivel resuelto
M_SUELO     equ 1                   ; el mapa: un octeto por casilla
M_MURO      equ 2
M_MARCA     equ 4
M_CAJA      equ 8
M_MOZO      equ 0x80                ; (solo en la tabla de letras)
D_DER       equ 0
D_IZQ       equ 1
D_ARRIBA    equ 2
D_ABAJO     equ 3
WM_FINAL    equ WM_USER + 100       ; (propio) decir que se ha resuelto

HVP_CABECERA "Almacén", entrada
        db "................................"
        db "................................"
        db "................................"
        db "...............11111111111111..."
        db "...............11111111111111..."
        db "...............11bbbbbbbbbb11..."
        db "...............11bbbbbbbbbb11..."
        db "...............11bbbbbbbbbb11..."
        db "...............11bbbbbbbbbb11..."
        db "...11111111111111111111911111..."
        db "...111111111111111111b9bbbb11..."
        db "...11aaaaaa77aaaaaa119bbbbb11..."
        db "...11aaaaaa77aaaaaa11bbbbbb11..."
        db "...11aaaaaa77aaaaaa11bbbbbb11..."
        db "...11aaaaaa77aaaaaa11bbbbbb11..."
        db "...11111111771111111111111111..."
        db "...11aaaaaa77aaaaaa1111111111..."
        db "...11aaaaaa77aaaaaa11.444......."
        db "...11aaaaaa77aaaaaa1144444......"
        db "...11aaaaaa77aaaaaa11444444....."
        db "...11111111771111111144f4444...."
        db "...11aaaaaa77aaaaaa114fff4444..."
        db "...11aaaaaa77aaaaaa11fffff4444.."
        db "...11aaaaaa77aaaaaa11ffffff4444."
        db "...11aaaaaa77aaaaaa11fffff4444.."
        db "...1111111111111111114fff4444..."
        db "...11111111111111111144f4444...."
        db "...000000000000000000444444....."
        db ".....................44444......"
        db "......................444......."
        db ".......................4........"
        db "................................"
HVP_CODIGO

c_almacen   dd alm_proc
            db C_CREMA, 0, 0, 0
alm_plant:
        PLANTILLA c_almacen, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 140, 60, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1 + MENU_H, 0

alm_menu:
        db "&Juego", 0
        dw 101
        db "&Nueva partida", 0
        dw 102
        db "&Reiniciar el nivel", 0
        dw 103
        db "&Deshacer", 0
        dw 0
        db 0
        dw 104
        db "Nivel &siguiente", 0
        dw 105
        db "Nivel &anterior", 0
        dw 106
        db "&Elegir nivel...", 0
        dw 0
        db 0
        dw 107
        db "Ré&cords...", 0
        dw 0
        db 0
        dw 108
        db "Sa&lir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 110
        db "&Cómo se juega...", 0
        dw 0xFFFF
        db 0

; ---- los niveles ----
; Cada fila es un texto acabado en 0; un 0 suelto acaba el nivel y un 0xFF,
; la lista. Las letras:
;    H muro (estantería)     . suelo        (espacio) fuera del almacén
;    c caja                  x marca        C caja que ya está en una marca
;    a el mozo               A el mozo, sobre una marca
; Como mucho MAXC x MAXF casillas. Se dibujan centrados. Para añadir o
; cambiar uno: python3 tools/almacen_niveles.py (comprueba que está cerrado,
; que hay tantas cajas como marcas y que tiene solución).
niveles:
; nivel 1
        db "HHHHHHHH", 0
        db "H......H", 0
        db "H.a.c.xH", 0
        db "H......H", 0
        db "HHHHHHHH", 0
        db 0
; nivel 2
        db "HHHHHH", 0
        db "Hx...H", 0
        db "H.c..H", 0
        db "H..a.H", 0
        db "HHHHHH", 0
        db 0
; nivel 3
        db "HHHHHHHHH", 0
        db "H...H...H", 0
        db "H.c...x.H", 0
        db "Ha..H...H", 0
        db "HHHHHHHHH", 0
        db 0
; nivel 4
        db "HHHHHHH", 0
        db "H..a..H", 0
        db "H.c.c.H", 0
        db "H.....H", 0
        db "Hx...xH", 0
        db "HHHHHHH", 0
        db 0
; nivel 5
        db "HHHHHHH", 0
        db "H.....H", 0
        db "H.HcH.H", 0
        db "H...c.H", 0
        db "HAx...H", 0
        db "HHHHHHH", 0
        db 0
; nivel 6
        db "  HHHHH", 0
        db "HHH...H", 0
        db "H.c...H", 0
        db "H.HH.cH", 0
        db "H..A.xH", 0
        db "HHHHHHH", 0
        db 0
; nivel 7
        db "HHHHHHHH", 0
        db "H..H...H", 0
        db "H..c...H", 0
        db "HH.HH..H", 0
        db " HAx.c.H", 0
        db " HHHHHHH", 0
        db 0
; nivel 8
        db "   HHHHH", 0
        db "HHHH..aH", 0
        db "H......H", 0
        db "H.HcHH.H", 0
        db "H.c.xc.H", 0
        db "HH.xx.HH", 0
        db " HHHHHH", 0
        db 0
; nivel 9
        db "HHHHHHHHH", 0
        db "H.......H", 0
        db "H.H.H.H.H", 0
        db "H....cc.H", 0
        db "HHHxCAHHH", 0
        db "  HHHHH", 0
        db 0
; nivel 10
        db "HHHHH", 0
        db "Ha..HHHHH", 0
        db "H.......H", 0
        db "HHH.H.H.H", 0
        db "  H.ccxxH", 0
        db "  H.c..xH", 0
        db "  HHHHHHH", 0
        db 0
; nivel 11
        db " HHHHHH", 0
        db "HH.a..HH", 0
        db "H.cHHc.H", 0
        db "H.c....H", 0
        db "H.x..x.H", 0
        db "HH..x.HH", 0
        db " HHHHHH", 0
        db 0
; nivel 12
        db "HHHHHHHHHH", 0
        db "H....H..aH", 0
        db "H.c.c....H", 0
        db "H.HH.H.x.H", 0
        db "H....c.x.H", 0
        db "H....H.x.H", 0
        db "HHHHHHHHHH", 0
        db 0
; nivel 13
        db "  HHHHHH", 0
        db "HHH....HHH", 0
        db "H.c...cc.H", 0
        db "H.H.xx.HaH", 0
        db "H...xx.c.H", 0
        db "HH......HH", 0
        db " HHHHHHHH", 0
        db 0
; nivel 14
        db "HHHHHHHHHH", 0
        db "H........H", 0
        db "H.HHc.HH.H", 0
        db "H.Hxc.xH.H", 0
        db "H.HC..xH.H", 0
        db "H.c......H", 0
        db "H.......aH", 0
        db "HHHHHHHHHH", 0
        db 0
; nivel 15
        db "HHHHHHH", 0
        db "H...a.HHHH", 0
        db "H.HH...c.H", 0
        db "H.c..HHc.H", 0
        db "HHHc.....H", 0
        db "  HxxxxHHH", 0
        db "  HHHHHH", 0
        db 0
; nivel 16
        db "HHHHHHHHH", 0
        db "H...H..aH", 0
        db "H.......H", 0
        db "H.c.H...H", 0
        db "HH.HHHcHH", 0
        db "H..c..c.H", 0
        db "H..xxxx.H", 0
        db "HHHHHHHHH", 0
        db 0
; nivel 17
        db "HHHHHHHHHHH", 0
        db "H....H....H", 0
        db "H...c...c.H", 0
        db "H....H..c.H", 0
        db "HHcHHHHH.HH", 0
        db "H.........H", 0
        db "H.x.xax.x.H", 0
        db "HHHHHHHHHHH", 0
        db 0
; nivel 18
        db "HHHHHHHHHHH", 0
        db "H.........H", 0
        db "H.H.H.HcH.H", 0
        db "H.......c.H", 0
        db "H.H.H.H.H.H", 0
        db "H......c..H", 0
        db "HHHxCxCAHHH", 0
        db "  HHHHHHH", 0
        db 0
; nivel 19
        db " HHHHHHHHH", 0
        db "HH...H...HH", 0
        db "H.c..cc.c.H", 0
        db "H..H.x.H..H", 0
        db "H...xxx.c.H", 0
        db "HH..ax...HH", 0
        db " HHHHHHHHH", 0
        db 0
; nivel 20
        db "HHHHHHHHHH", 0
        db "H...HH...H", 0
        db "H....c.c.H", 0
        db "HH...c..HH", 0
        db "H...xxc..H", 0
        db "Ha.xxxc..H", 0
        db "HHHHHHHHHH", 0
        db 0
; nivel 21
        db "HHHHHHHHHHHH", 0
        db "H...a.H....H", 0
        db "H.HH.......H", 0
        db "H.....Hcxx.H", 0
        db "HHcHHHH.xx.H", 0
        db "H.c...ccx..H", 0
        db "H.....H....H", 0
        db "HHHHHHHHHHHH", 0
        db 0
; nivel 22
        db "HHHHHHHHHHH", 0
        db "H.........H", 0
        db "H.xxcH.xx.H", 0
        db "H.C.c...x.H", 0
        db "H.cccH....H", 0
        db "H........aH", 0
        db "HHHHHHHHHHH", 0
        db 0
; nivel 23
        db "HHHHHHHHHHHHH", 0
        db "H...........H", 0
        db "H.HHcHHH.HH.H", 0
        db "H.cac.......H", 0
        db "H.HcCCxxxx..H", 0
        db "H...........H", 0
        db "HHHHHHHHHHHHH", 0
        db 0
; nivel 24
        db "HHHHHHHHHHHH", 0
        db "H....H.....H", 0
        db "H..c..c....H", 0
        db "H.H..Hc.H..H", 0
        db "H.......cc.H", 0
        db "H..HHHHHH..H", 0
        db "Ha.xxxxxC..H", 0
        db "HHHHHHHHHHHH", 0
        db 0
niveles_fin:
        db 0xFF

; lo que vale cada letra de un nivel
letras      db 'H', M_MURO
            db '.', M_SUELO
            db 'x', M_SUELO | M_MARCA
            db 'c', M_SUELO | M_CAJA
            db 'C', M_SUELO | M_MARCA | M_CAJA
            db 'a', M_SUELO | M_MOZO
            db 'A', M_SUELO | M_MARCA | M_MOZO
            db 0
; cada dirección (derecha, izquierda, arriba, abajo): columnas y filas
dir_tab     dd 1, 0,   -1, 0,   0, -1,   0, 1

t_titulo    db "Almacén", 0
t_nivel     db "Nivel: ", 0
t_movs      db "Movimientos: ", 0
t_emp       db "Empujones: ", 0
t_mejor     db "Mejor: ", 0
t_res_tit   db "Nivel resuelto", 0
t_res1      db "Nivel ", 0
t_res2      db " resuelto con ", 0
t_res3      db " movimientos y ", 0
t_res4      db " empujones.", 0
t_res_rec   db 10, "¡Es su mejor marca en este nivel!", 0
t_res_fin   db 10, 10, "¡Enhorabuena! Era el último: todo el almacén", 10
            db "está en orden.", 0
t_cerrado   db "Ese nivel todavía está cerrado: resuelva antes", 10
            db "el anterior.", 0
t_ele_tit   db "Elegir nivel", 0
t_ele1      db "Nivel (de 1 a ", 0
t_ele2      db "):", 0
t_rec_tit   db "Récords", 0
t_rec_cab   db "Mejor marca de cada nivel: movimientos (empujones)", 10, 10, 0
t_ayuda_tit db "Cómo se juega", 0
t_ayuda     db "Lleve cada caja a una marca del suelo. El mozo empuja", 10
            db "las cajas de una en una; no puede tirar de ellas.", 10
            db 10
            db "Flechas: andar y empujar.   Retroceso o U: deshacer.", 10
            db "R o F2: empezar el nivel otra vez.", 10
            db "Av Pág y Re Pág: nivel siguiente y anterior.", 0
k_cab       db "Almacén - mejores marcas (Hispa Ventanas)", 13, 10, 0
k_nivel     db "Nivel=", 0
k_mejor     db "Mejor=", 0
k_emp       db "Empujones=", 0

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        push edi
        mov edi, ruta_rec           ; ALMACEN.REC, al lado del módulo
        mov eax, 'REC'
        call hvl_ruta
        pop edi
        mov esi, niveles            ; dónde empieza cada nivel
        xor ecx, ecx
.n:
        cmp byte [esi], 0xFF
        je .n3
        cmp ecx, MAX_NIV
        jae .n3
        mov [niv_ptr+ecx*4], esi
        inc ecx
.n1:
        cmp byte [esi], 0           ; una fila vacía: fin del nivel
        je .n2
.n0:
        lodsb
        test al, al
        jnz .n0
        jmp .n1
.n2:
        inc esi
        jmp .n
.n3:
        mov [n_niv], ecx
        call rec_leer
        mov esi, edi
        mov al, 'N'
        call hvl_opcion
        jc .sin
        call hvl_lee_num
        mov byte [libre], 1
        dec eax
        cmp eax, [n_niv]
        jae .sin
        mov [nivel], eax
.sin:
        mov esi, alm_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; ---- el procedimiento de la ventana ----
alm_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .letra
        cmp eax, WM_LBUTTONDOWN
        je .clic
        cmp eax, WM_LBUTTONDBLCLK
        je .clic
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
        mov dword [ebp+V_MENU], alm_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call cargar
        jmp .hecho
.destruir:
        HV msg_salir
        jmp .hecho
.letra:
        cmp ecx, 8
        je .deshacer
        and ecx, 0xDF
        cmp ecx, 'U'
        je .deshacer
        cmp ecx, 'R'
        je .reiniciar
        jmp .hecho
.tecla:
        mov eax, D_DER
        cmp ecx, TC_DER
        je .mover
        mov eax, D_IZQ
        cmp ecx, TC_IZQ
        je .mover
        mov eax, D_ARRIBA
        cmp ecx, TC_ARRIBA
        je .mover
        mov eax, D_ABAJO
        cmp ecx, TC_ABAJO
        je .mover
        cmp ecx, TC_RETRO
        je .deshacer
        cmp ecx, TC_CTRL + 26       ; Ctrl+Z
        je .deshacer
        cmp ecx, TC_F1 + 1          ; F2
        je .reiniciar
        cmp ecx, TC_AVPAG
        je .siguiente
        cmp ecx, TC_REPAG
        je .anterior
        jmp .hecho
.mover:
        call mover
        jmp .hecho
.clic:
        movsx eax, dx               ; la casilla del clic
        sar edx, 16
        sub eax, [org_x]
        js .hecho
        sub edx, [org_y]
        js .hecho
        mov ebx, CEL
        mov ecx, edx
        xor edx, edx
        div ebx
        xchg eax, ecx               ; ECX=columna
        xor edx, edx
        div ebx                     ; EAX=fila
        sub ecx, [mozo_x]
        sub eax, [mozo_y]
        jnz .c1
        mov eax, D_DER              ; en su fila
        test ecx, ecx
        jz .hecho
        jns .mover
        mov eax, D_IZQ
        jmp .mover
.c1:
        test ecx, ecx
        jnz .hecho
        test eax, eax               ; en su columna
        mov eax, D_ABAJO
        jns .mover
        mov eax, D_ARRIBA
        jmp .mover
.orden:
        cmp ecx, 101
        jne .o2
        xor eax, eax                ; partida nueva: desde el nivel 1
        call ir_a
        jmp .hecho
.o2:
        cmp ecx, 102
        jne .o3
.reiniciar:
        call cargar
        jmp .hecho
.o3:
        cmp ecx, 103
        jne .o4
.deshacer:
        call deshacer
        jmp .hecho
.o4:
        cmp ecx, 104
        jne .o5
.siguiente:
        mov eax, [nivel]
        inc eax
        call ir_a
        jmp .hecho
.o5:
        cmp ecx, 105
        jne .o6
.anterior:
        mov eax, [nivel]
        dec eax
        call ir_a
        jmp .hecho
.o6:
        cmp ecx, 106
        jne .o7
        mov edi, txt2               ; «Nivel (de 1 a 24):»
        mov esi, t_ele1
        call hvl_cad
        mov eax, [n_niv]
        call hvl_num
        mov esi, t_ele2
        call hvl_cad
        mov byte [edi], 0
        mov edi, num
        mov eax, [nivel]
        inc eax
        call hvl_num
        mov byte [edi], 0
        mov esi, txt2
        mov edi, t_ele_tit
        mov ebx, num
        mov ecx, 4
        HV dlg_pedir
        cmp eax, 1
        jne .hecho
        mov esi, num
        call hvl_lee_num
        dec eax
        call ir_a
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
        mov edi, txt2               ; tres niveles por línea
        mov esi, t_rec_cab
        call hvl_cad
        xor ebx, ebx
.r1:
        cmp ebx, [n_niv]
        jae .r6
        mov edx, edi                ; (cada uno ocupa 17 letras)
        lea eax, [ebx+1]
        cmp eax, 10
        jae .r2
        mov al, ' '
        stosb
        lea eax, [ebx+1]
.r2:
        call hvl_num
        mov ax, ': '
        stosw
        mov eax, [mejor+ebx*4]
        test eax, eax
        jnz .r3
        mov ax, '--'
        stosw
        jmp .r4
.r3:
        call hvl_num
        mov ax, ' ('
        stosw
        mov eax, [mejor_e+ebx*4]
        call hvl_num
        mov al, ')'
        stosb
.r4:
        inc ebx
        mov eax, ebx
        push edx
        xor edx, edx
        mov ecx, 3
        div ecx
        mov eax, edx
        pop edx
        test eax, eax
        jnz .r5
        mov al, 10                  ; fin de la línea
        stosb
        jmp .r1
.r5:
        add edx, 17                 ; espacios hasta la columna siguiente
        mov al, ' '
.r50:
        cmp edi, edx
        jae .r1
        stosb
        jmp .r50
.r6:
        cmp byte [edi-1], 10        ; (sin una línea vacía al final)
        jne .r7
        dec edi
.r7:
        mov byte [edi], 0
        mov esi, txt2
        mov edi, t_rec_tit
        jmp .dlg
.final:
        mov edi, txt2               ; «Nivel N resuelto con M movimientos...»
        mov esi, t_res1
        call hvl_cad
        mov eax, [nivel]
        inc eax
        call hvl_num
        mov esi, t_res2
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        mov esi, t_res3
        call hvl_cad
        mov eax, [empujes]
        call hvl_num
        mov esi, t_res4
        call hvl_cad
        cmp byte [nuevo_rec], 0
        je .f1
        mov esi, t_res_rec
        call hvl_cad
.f1:
        mov ebx, [nivel]
        inc ebx
        cmp ebx, [n_niv]
        jb .f2
        mov esi, t_res_fin
        call hvl_cad
.f2:
        mov byte [edi], 0
        mov esi, txt2
        mov edi, t_res_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        cmp dword [estado], E_HECHO ; (si entretanto no se ha cambiado de nivel)
        jne .hecho
        cmp ebx, [n_niv]
        jae .hecho                  ; el último se queda a la vista
        mov eax, ebx
        call ir_a
        jmp .hecho
.dlg:
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret

; ---- va al nivel EAX (desde 0), si existe y está abierto ----
ir_a:
        pushad
        mov ebp, [ven]
        cmp eax, [n_niv]
        jae .x
        cmp byte [libre], 0
        jne .si
        test eax, eax               ; abiertos: el primero, los resueltos y
        jz .si                      ; el que sigue a uno resuelto
        cmp dword [mejor+eax*4], 0
        jne .si
        cmp dword [mejor+eax*4-4], 0
        jne .si
        mov esi, t_cerrado
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp .x
.si:
        mov [nivel], eax
        call cargar
        call rec_guardar
.x:
        popad
        ret

; ---- pone el nivel [nivel] como al principio ----
cargar:
        pushad
        mov edi, mapa
        mov ecx, MAXC * MAXF
        xor eax, eax
        rep stosb
        mov [niv_an], eax
        mov [quedan], eax
        mov [movs], eax
        mov [empujes], eax
        mov [n_undo], eax
        mov [nuevo_rec], al
        mov dword [mozo_dir], D_ABAJO
        mov dword [estado], E_JUEGA
        mov eax, [nivel]
        mov esi, [niv_ptr+eax*4]
        xor edx, edx                ; EDX=fila ECX=columna
.fila:
        cmp byte [esi], 0
        je .fin
        xor ecx, ecx
.car:
        lodsb
        test al, al
        jz .ff
        cmp ecx, MAXC               ; (lo que no quepa no se pone)
        jae .car
        cmp edx, MAXF
        jae .car
        mov ebx, letras
.l:
        cmp byte [ebx], 0
        je .sig
        cmp al, [ebx]
        je .es
        add ebx, 2
        jmp .l
.es:
        mov al, [ebx+1]
        test al, M_MOZO
        jz .e1
        mov [mozo_x], ecx
        mov [mozo_y], edx
        and al, ~M_MOZO
.e1:
        imul edi, edx, MAXC
        mov [mapa+edi+ecx], al
        and al, M_MARCA | M_CAJA    ; una caja fuera de su marca
        cmp al, M_CAJA
        jne .sig
        inc dword [quedan]
.sig:
        inc ecx
        jmp .car
.ff:
        cmp ecx, [niv_an]
        jbe .f1
        mov [niv_an], ecx
.f1:
        inc edx
        jmp .fila
.fin:
        mov [niv_al], edx
        mov eax, MAXC               ; centrado en el tablero
        sub eax, [niv_an]
        jns .c1
        xor eax, eax
.c1:
        imul eax, CEL / 2
        add eax, TAB_X
        mov [org_x], eax
        mov eax, MAXF
        sub eax, edx
        jns .c2
        xor eax, eax
.c2:
        imul eax, CEL / 2
        add eax, TAB_Y
        mov [org_y], eax
        mov ebp, [ven]
        HV ven_invalidar
        popad
        ret

; la casilla EAX (fila*MAXC+columna) hay que repintarla
inv_celda:
        pushad
        xor edx, edx
        mov ecx, MAXC
        div ecx
        imul ebx, eax, CEL
        add ebx, [org_y]
        imul eax, edx, CEL
        add eax, [org_x]
        mov ecx, CEL
        mov edx, CEL
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; los rótulos de arriba hay que repintarlos
inv_rotulos:
        pushad
        xor eax, eax
        xor ebx, ebx
        mov ecx, CLI_AN
        mov edx, TAB_Y - 4
        mov ebp, [ven]
        HV ven_invalidar_rect
        popad
        ret

; -> EAX=la casilla del mozo
casilla_mozo:
        imul eax, [mozo_y], MAXC
        add eax, [mozo_x]
        ret

; EAX=dirección -> EDX=lo que cambia la casilla al dar un paso hacia allí
paso_de:
        imul edx, [dir_tab+eax*8+4], MAXC
        add edx, [dir_tab+eax*8]
        ret

; ---- un paso del mozo hacia EAX (D_*): anda o empuja ----
mover:
        pushad
        cmp dword [estado], E_JUEGA
        jne .x
        mov [mozo_dir], eax
        mov ebx, eax                ; EBX=dirección EDX=paso
        call paso_de
        call casilla_mozo
        mov esi, eax                ; ESI=donde está EDI=a donde va
        lea edi, [eax+edx]
        call inv_celda              ; (aunque no ande, se vuelve hacia allí)
        mov al, [mapa+edi]
        test al, M_SUELO
        jz .x
        test al, M_CAJA
        jz .anda
        mov al, [mapa+edi+edx]      ; una caja: ¿tiene sitio detrás?
        test al, M_SUELO
        jz .x
        test al, M_CAJA
        jnz .x
        and byte [mapa+edi], ~M_CAJA
        test byte [mapa+edi], M_MARCA
        jz .e1
        inc dword [quedan]          ; sale de una marca
.e1:
        or byte [mapa+edi+edx], M_CAJA
        test al, M_MARCA
        jz .e2
        dec dword [quedan]          ; entra en una marca
.e2:
        lea eax, [edi+edx]
        call inv_celda
        inc dword [empujes]
        or bl, 4                    ; (en la lista de deshacer: con caja)
.anda:
        mov eax, edi
        call inv_celda
        mov eax, [mozo_dir]
        mov ecx, [dir_tab+eax*8]
        add [mozo_x], ecx
        mov ecx, [dir_tab+eax*8+4]
        add [mozo_y], ecx
        inc dword [movs]
        mov ecx, [n_undo]           ; se apunta para deshacer
        cmp ecx, UNDO_MAX
        jb .u1
        push esi                    ; llena: se olvidan los más viejos
        push edi
        mov esi, pila + UNDO_TIRA
        mov edi, pila
        mov ecx, UNDO_MAX - UNDO_TIRA
        rep movsb
        pop edi
        pop esi
        mov ecx, UNDO_MAX - UNDO_TIRA
.u1:
        mov [pila+ecx], bl
        inc ecx
        mov [n_undo], ecx
        call inv_rotulos
        cmp dword [quedan], 0
        jne .x
        test bl, 4
        jz .x
        call resuelto
.x:
        popad
        ret

; ---- deshace el último paso ----
deshacer:
        pushad
        cmp dword [estado], E_JUEGA
        jne .x
        mov ecx, [n_undo]
        test ecx, ecx
        jz .x
        dec ecx
        mov [n_undo], ecx
        movzx ebx, byte [pila+ecx]
        mov eax, ebx
        and eax, 3
        mov [mozo_dir], eax
        call paso_de
        call casilla_mozo
        mov esi, eax                ; ESI=donde está el mozo
        call inv_celda
        test bl, 4
        jz .d3
        and byte [mapa+esi+edx], ~M_CAJA    ; la caja vuelve a donde está el mozo
        test byte [mapa+esi+edx], M_MARCA
        jz .d1
        inc dword [quedan]
.d1:
        or byte [mapa+esi], M_CAJA
        test byte [mapa+esi], M_MARCA
        jz .d2
        dec dword [quedan]
.d2:
        lea eax, [esi+edx]
        call inv_celda
        dec dword [empujes]
.d3:
        mov eax, esi
        sub eax, edx
        call inv_celda
        mov eax, [mozo_dir]
        mov ecx, [dir_tab+eax*8]
        sub [mozo_x], ecx
        mov ecx, [dir_tab+eax*8+4]
        sub [mozo_y], ecx
        dec dword [movs]
        call inv_rotulos
.x:
        popad
        ret

; ---- todas las cajas están en su marca ----
resuelto:
        pushad
        mov dword [estado], E_HECHO
        mov ebx, [nivel]
        mov eax, [movs]
        mov edx, [mejor+ebx*4]
        test edx, edx
        jz .rec
        cmp eax, edx
        jae .x
.rec:
        mov [mejor+ebx*4], eax
        mov eax, [empujes]
        mov [mejor_e+ebx*4], eax
        mov byte [nuevo_rec], 1
        call rec_guardar
.x:
        mov ebp, [ven]
        mov eax, WM_FINAL
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        popad
        ret

; ---- ALMACEN.REC: el nivel en que se está y la mejor marca de cada uno ----
rec_guardar:
        pushad
        mov edi, txt
        mov esi, k_cab
        call hvl_cad
        mov esi, k_nivel
        call hvl_cad
        mov eax, [nivel]
        inc eax
        call hvl_num
        mov ax, 0x0A0D
        stosw
        mov esi, k_mejor
        mov ebx, mejor
        call .fila
        mov esi, k_emp
        mov ebx, mejor_e
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
        mov ecx, [n_niv]
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
        ret

rec_leer:
        pushad
        mov esi, ruta_rec
        mov edi, txt
        mov ecx, TXT_TAM
        call hvl_leer
        test eax, eax
        jz .x
        mov edi, k_mejor
        mov ebx, mejor
        call .fila
        mov edi, k_emp
        mov ebx, mejor_e
        call .fila
        mov esi, txt
        mov edi, k_nivel
        call hvl_clave
        jc .x
        call hvl_lee_num
        dec eax
        cmp eax, [n_niv]
        jae .x
        test eax, eax               ; (solo si está abierto)
        jz .x
        cmp dword [mejor+eax*4], 0
        jne .n
        cmp dword [mejor+eax*4-4], 0
        je .x
.n:
        mov [nivel], eax
.x:
        popad
        ret
.fila:
        mov esi, txt
        call hvl_clave
        jc .f2
        mov ecx, [n_niv]
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
; origen del nivel) ESI=casillas que hay -> ECX=primera EDX=última
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
        sub ecx, [org_x]
        sub edx, [org_x]
        mov esi, [niv_an]
        call rango
        mov [p_c0], ecx
        mov [p_c1], edx
        mov ecx, [ebp+V_UY0]
        mov edx, [ebp+V_UY1]
        sub ecx, ebx
        sub edx, ebx
        sub ecx, [org_y]
        sub edx, [org_y]
        mov esi, [niv_al]
        call rango
        mov [p_f0], ecx
        mov [p_f1], edx
        HV dc_pintar
        ; los rótulos
        COLOR C_TINTA
        mov edi, rot
        mov esi, t_nivel
        call hvl_cad
        mov eax, [nivel]
        inc eax
        call hvl_num
        mov al, '/'
        stosb
        mov eax, [n_niv]
        call hvl_num
        mov eax, 8
        call .rotulo
        mov edi, rot
        mov esi, t_movs
        call hvl_cad
        mov eax, [movs]
        call hvl_num
        mov eax, 112
        call .rotulo
        mov edi, rot
        mov esi, t_emp
        call hvl_cad
        mov eax, [empujes]
        call hvl_num
        mov eax, 248
        call .rotulo
        COLOR C_ARENA_OSC
        mov edi, rot
        mov esi, t_mejor
        call hvl_cad
        mov eax, [nivel]
        mov eax, [mejor+eax*4]
        test eax, eax
        jnz .r1
        mov al, '-'
        stosb
        jmp .r2
.r1:
        call hvl_num
.r2:
        mov eax, CLI_AN - 8 - 11*8
        call .rotulo
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
        HV dc_cerrar
        xor eax, eax
        ret
; (interna) acaba el texto de rot y lo pone en EAX, 8
.rotulo:
        mov byte [edi], 0
        mov ebx, 8
        mov esi, rot
        HV dc_texto
        ret

; dibuja la casilla ECX (columna), EDX (fila)
celda:
        pushad
        imul edi, edx, MAXC
        add edi, ecx
        movzx esi, byte [mapa+edi]  ; ESI=lo que hay
        test esi, esi
        jz .x                       ; fuera del almacén: el fondo
        imul eax, ecx, CEL
        add eax, [org_x]
        imul ebx, edx, CEL
        add ebx, [org_y]
        mov [c_x], eax
        mov [c_y], ebx
        test esi, M_MURO
        jnz .muro
        COLOR C_PIEDRA              ; el suelo: una baldosa
        mov edi, baldosa
        call rects
        test esi, M_MARCA
        jz .s1
        COLOR C_TERRACOTA           ; la marca: un rombo hueco
        add eax, CEL / 2
        add ebx, CEL / 2
        mov ecx, 8
        HV dc_rombo
        COLOR C_PIEDRA
        mov ecx, 5
        HV dc_rombo
        COLOR C_TERRACOTA
        mov ecx, 1
        HV dc_rombo
.s1:
        test esi, M_CAJA
        jnz .caja
        mov eax, [esp+24]           ; ¿está aquí el mozo?
        cmp eax, [mozo_x]
        jne .x
        mov eax, [esp+20]
        cmp eax, [mozo_y]
        jne .x
        mov edi, mozo
        call rects
        mov eax, [mozo_dir]         ; la cara, hacia donde va
        mov edi, [caras+eax*4]
        call rects
        jmp .x
.caja:
        mov edi, caja
        call rects
        test esi, M_MARCA
        jz .x
        mov edi, caja_ok            ; en su marca: verde, con una señal
        call rects
        COLOR C_CREMA
        mov eax, [c_x]
        mov ebx, [c_y]
        add eax, CEL / 2
        add ebx, CEL / 2
        mov ecx, 3
        HV dc_rombo
        jmp .x
.muro:
        mov edi, muro
        call rects
.x:
        popad
        ret

; dibuja la lista de rectángulos EDI en la casilla c_x, c_y. Cada uno:
; db x, y, ancho, alto, color; un color 0xFF deja el que hay; acaba en 0xFF
rects:
        pushad
.b:
        movzx eax, byte [edi]
        cmp al, 0xFF
        je .x
        movzx ebx, byte [edi+1]
        movzx ecx, byte [edi+2]
        movzx edx, byte [edi+3]
        movzx esi, byte [edi+4]
        add edi, 5
        add eax, [c_x]
        add ebx, [c_y]
        cmp esi, 0xFF
        je .r
        COLOR esi
.r:
        HV dc_rect
        jmp .b
.x:
        popad
        ret

; los dibujos de una casilla de 28 x 28
baldosa     db 0, 0, CEL, CEL, 0xFF         ; (el color ya está puesto)
            db 0, 0, CEL, 1, C_CREMA        ; la junta, arriba y a la izquierda
            db 0, 0, 1, CEL, C_CREMA
            db 0xFF
muro        db 0, 0, CEL, CEL, C_MARRON     ; una estantería con tres baldas
            db 0, 8, CEL, 2, C_ARENA_OSC
            db 0, 18, CEL, 2, C_ARENA_OSC
            db 0, 0, 1, CEL, C_TINTA        ; los largueros
            db CEL - 1, 0, 1, CEL, C_TINTA
            db 3, 2, 6, 6, C_PETROLEO       ; y lo que hay en ellas
            db 12, 3, 9, 5, C_TERRACOTA
            db 5, 12, 10, 6, C_OCRE
            db 18, 11, 6, 7, C_OLIVA
            db 3, 22, 8, 6, C_ANIL
            db 14, 23, 10, 5, C_ARENA
            db 0xFF
caja        db 2, 2, 24, 24, C_MARRON       ; de madera, con sus tablas
            db 4, 4, 20, 20, C_OCRE
            db 4, 10, 20, 1, C_MARRON
            db 4, 17, 20, 1, C_MARRON
            db 13, 4, 2, 20, C_ARENA_OSC
            db 5, 5, 1, 1, C_TINTA          ; los clavos
            db 22, 5, 1, 1, C_TINTA
            db 5, 22, 1, 1, C_TINTA
            db 22, 22, 1, 1, C_TINTA
            db 0xFF
caja_ok     db 4, 4, 20, 20, C_OLIVA
            db 4, 10, 20, 1, C_MARRON
            db 4, 17, 20, 1, C_MARRON
            db 0xFF
mozo        db 9, 21, 4, 5, C_TINTA         ; las botas
            db 15, 21, 4, 5, C_TINTA
            db 7, 12, 14, 10, C_ANIL        ; el mono
            db 4, 13, 3, 6, C_TERRACOTA_CL  ; los brazos
            db 21, 13, 3, 6, C_TERRACOTA_CL
            db 12, 14, 4, 3, C_OCRE         ; el peto
            db 9, 4, 10, 8, C_TERRACOTA_CL  ; la cabeza
            db 8, 2, 12, 3, C_GRANATE       ; y la gorra
            db 0xFF
caras       dd cara_der, cara_izq, cara_arr, cara_aba
cara_der    db 19, 4, 4, 2, C_GRANATE       ; la visera y un ojo
            db 16, 7, 2, 2, C_TINTA
            db 0xFF
cara_izq    db 5, 4, 4, 2, C_GRANATE
            db 10, 7, 2, 2, C_TINTA
            db 0xFF
cara_arr    db 8, 2, 12, 7, C_GRANATE       ; de espaldas: solo la gorra
            db 0xFF
cara_aba    db 8, 5, 12, 1, C_GRANATE
            db 10, 7, 2, 2, C_TINTA
            db 16, 7, 2, 2, C_TINTA
            db 12, 10, 4, 1, C_GRANATE      ; la boca
            db 0xFF

%include "hvplib.inc"

TXT_TAM     equ 640
        section .bss
ven         resd 1                  ; la ventana
estado      resd 1
nivel       resd 1                  ; desde 0
n_niv       resd 1                  ; cuántos hay
niv_an      resd 1                  ; casillas del nivel
niv_al      resd 1
org_x       resd 1                  ; dónde se dibuja su primera casilla
org_y       resd 1
mozo_x      resd 1
mozo_y      resd 1
mozo_dir    resd 1
quedan      resd 1                  ; cajas fuera de su marca
movs        resd 1
empujes     resd 1
n_undo      resd 1
p_c0        resd 1
p_c1        resd 1
p_f0        resd 1
p_f1        resd 1
c_x         resd 1                  ; la casilla que se está dibujando
c_y         resd 1
niv_ptr     resd MAX_NIV            ; dónde empieza cada nivel
mejor       resd MAX_NIV            ; menos movimientos (0: sin resolver)
mejor_e     resd MAX_NIV            ; y los empujones de esa vez
libre       resb 1                  ; /N: todos los niveles abiertos
nuevo_rec   resb 1
num         resb 8
ruta_rec    resb 80
rot         resb 32                 ; el rótulo que se está pintando
txt         resb TXT_TAM            ; el archivo de récords
txt2        resb TXT_TAM            ; el texto de un diálogo
mapa        resb MAXC * MAXF
pila        resb UNDO_MAX           ; los pasos: dirección (+4 con caja)
HVP_FIN
