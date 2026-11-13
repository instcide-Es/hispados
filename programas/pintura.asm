; =============================================================================
;  PINTURA.HVP - Pintura, el programa de dibujo de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un lienzo de puntos (un octeto por punto: los 256 colores de Hispa
;  Ventanas) sobre el que se dibuja con el ratón. Arriba, la barra de
;  herramientas (lápiz, pincel, goma, línea, rectángulo, elipse, relleno,
;  cuentagotas, texto y selección), los cuatro grosores y las tres formas
;  (contorno, lleno, contorno con fondo). A la derecha, los dos colores en
;  dos rombos (el primario se usa con el botón izquierdo y el secundario con
;  el derecho) y la paleta. Abajo, la barra de estado. Si el lienzo no cabe
;  en la ventana salen barras de desplazamiento.
;
;  Archivo: Nuevo, Abrir, Guardar y Guardar como, en BMP. Se leen los BMP de
;  1, 4, 8 y 24 bits sin compresión (cada color pasa al más parecido de los
;  256) y se guardan de 8 bits con la paleta de Hispa Ventanas. Edición:
;  deshacer y rehacer (hasta 8 pasos, según el tamaño), cortar, copiar,
;  pegar, borrar y seleccionar todo. Pregunta antes de perder cambios.
;
;  Todo el diseño es propio: la disposición, los dibujos de las herramientas,
;  el icono y los textos. Es un módulo .HVP (ventanas/hvp.inc) que usa la
;  versión 2 de la interfaz: dc_copiar, dlg_archivo, mem_bloque, pal_rgb y
;  fuente8x16 (docs/VENTANAS.md).
;
;  Parámetros del acceso directo: el nombre de un BMP, que se abre al entrar.
;
;  Memoria: un bloque de memoria extendida con el lienzo, la zona flotante
;  (lo seleccionado que se mueve o se pega; también es la pila del relleno) y
;  los pasos de deshacer, cada uno del tamaño del lienzo.
; =============================================================================
%include "hvp.inc"

BARRA       equ 34                  ; alto de la barra de herramientas
PANEL       equ 76                  ; ancho del panel de colores
ESTADO      equ 20                  ; alto de la barra de estado
MARG        equ 6                   ; margen alrededor del lienzo
BD          equ 14                  ; grueso de las barras de desplazamiento
PAL_Y       equ 50                  ; la paleta, dentro del panel
MAXAN       equ 1024
MAXAL       equ 768
MEM_TOPE    equ 2560 * 1024         ; más pasos de deshacer no, si pasan de aquí
H_SELEC     equ 9
A_TRAZO     equ 1                   ; lo que se está haciendo con el botón apretado
A_FORMA     equ 2
A_MARCO     equ 3
A_MOVER     equ 4
A_BH        equ 5
A_BV        equ 6

HVP_CABECERA "Pintura", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db "..111111111111111111111111......"
        db "..188888888888888888888881......"
        db "..1888888888888888888888817....."
        db "..188888888888444488a888817....."
        db "..1888888888844444aaaaa8817....."
        db "..1884488888444884aaaaa8817....."
        db "..188444888444888aaaaaaa817....."
        db "..1888444444488888aaaaa8817....."
        db "..1888844444888888aaaaa8817....."
        db "..188888888840888888a888817....."
        db "..1888888888040008888888817....."
        db "..1888888888804408888888817....."
        db "..1888888888804408888888817....."
        db "..188cccccccc000000b8888817....."
        db "..188ceeeeeeec880700bb88817....."
        db "..188ceeeeeeec8800700b88817....."
        db "..188ceeeeeeec88b00700b8817....."
        db "..188ceeeeeeec888b007008817....."
        db "..188ccccccccc888bb00000817....."
        db "..18888888888888888b00a0017....."
        db "..188888888888888888800a007....."
        db "..1111111111111111111100a00....."
        db "....777777777777777777700a00...."
        db "........................00a00..."
        db ".........................00a00.."
        db "..........................00a00."
        db "...........................00a00"
        db "............................00a0"
        db ".............................000"
HVP_CODIGO

c_pintura   dd pin_proc
            db SIN_FONDO, 0, 0, 0
c_colores   dd col_proc
            db C_ARENA, 0, 0, 0
pin_plant:
        PLANTILLA c_pintura, titulo, E_NORMAL, 100, 60, 600, 440, 0
col_plant:
        PLANTILLA c_colores, t_col_tit, E_DIALOGO, 214, 150, 372, 163, 0

pin_menu:
        db "&Archivo", 0
        dw 101
        db "&Nuevo...", 0
        dw 102
        db "&Abrir...", 0
        dw 103
        db "&Guardar          Ctrl+G", 0
        dw 104
        db "Guardar &como...", 0
        dw 0
        db 0
        dw 105
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw 111
        db "&Deshacer          Ctrl+Z", 0
        dw 112
        db "&Rehacer           Ctrl+Y", 0
        dw 0
        db 0
        dw 113
        db "Cor&tar            Ctrl+X", 0
        dw 114
        db "&Copiar            Ctrl+C", 0
        dw 115
        db "&Pegar             Ctrl+V", 0
        dw 116
        db "&Borrar            Supr", 0
        dw 0
        db 0
        dw 117
        db "&Seleccionar todo  Ctrl+A", 0
        dw 0xFFFF
        db "&Colores", 0
        dw 121
        db "&Todos los colores...", 0
        dw 122
        db "&Intercambiar primario y secundario", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw 131
        db "&Cómo se usa...", 0
        dw 132
        db "&Acerca de Pintura...", 0
        dw 0xFFFF
        db 0

            align 4
; las órdenes del menú y quién las hace
tab_orden   dd 101, o_nuevo, 102, o_abrir, 103, guardar, 104, guardar_como, 105, o_salir
            dd 111, deshacer, 112, rehacer, 113, cortar, 114, copiar, 115, pegar
            dd 116, borrar, 117, todo, 121, abrir_colores, 122, intercambiar
            dd 131, o_ayuda, 132, o_acerca, 140, sel_quitar, 0
; teclas que son una orden
tab_tecla   dd TC_CTRL + 26, 111, TC_CTRL + 25, 112, TC_CTRL + 24, 113, TC_CTRL + 3, 114
            dd TC_CTRL + 22, 115, TC_SUPR, 116, TC_CTRL + 1, 117, TC_CTRL + 7, 103
            dd TC_ESC, 140, 0
tab_pulsa   dd p_trazo, p_trazo, p_trazo, p_forma, p_forma, p_forma
            dd p_relleno, p_gotas, p_texto, p_selec
tab_mueve   dd hecho, m_trazo, m_forma, m_marco, m_mover, m_bh, m_bv
nom_herr    dd t_h0, t_h1, t_h2, t_h3, t_h4, t_h5, t_h6, t_h7, t_h8, t_h9
letras      db "LPGNREBCTS", 0      ; la tecla de cada herramienta
gros_tab    db 1, 2, 4, 8           ; los grosores
esc_tab     db 1, 1, 2, 3           ; el tamaño del texto con cada grosor
; la paleta del panel: los 16 colores de Hispa Ventanas y 40 del cubo y de
; los grises (rojos, naranjas, amarillos, verdes, turquesas, azules,
; violetas, malvas, rosas y grises: de oscuro a claro)
tabla_col   db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
            db 88, 160, 196, 217,  130, 172, 214, 222,  142, 184, 226, 229
            db 28, 40, 46, 157,  30, 44, 51, 159,  18, 20, 21, 147
            db 91, 129, 177, 183,  90, 164, 201, 219,  198, 211, 218, 224
            db 232, 239, 246, 252

t_pintura   db "Pintura", 0
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_col_tit   db "Todos los colores", 0
t_h0        db "Lápiz", 0
t_h1        db "Pincel", 0
t_h2        db "Goma", 0
t_h3        db "Línea", 0
t_h4        db "Rectángulo", 0
t_h5        db "Elipse", 0
t_h6        db "Relleno", 0
t_h7        db "Cuentagotas", 0
t_h8        db "Texto", 0
t_h9        db "Selección", 0
t_sep       db "   ", 0
t_por       db " x ", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_ext_bmp   db ".BMP", 0
t_nuevo_tit db "Lienzo nuevo", 0
t_nuevo_preg db "Ancho x alto (hasta 1024 x 768):", 0
t_txt_tit   db "Texto", 0
t_txt_preg  db "Texto que se escribe en el lienzo:", 0
t_cambios   db "La imagen tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_bmp    db "El archivo no es una imagen BMP de 1, 4, 8 o 24 bits", 10
            db "sin compresión.", 0
t_grande    db "La imagen es demasiado grande: Pintura llega", 10
            db "hasta 1024 x 768 puntos.", 0
t_corto     db "El archivo está incompleto: falta parte de la imagen.", 0
t_sin_mem   db "No hay memoria bastante para un lienzo de ese tamaño.", 0
t_sin_mem_c db "No hay memoria bastante para copiar la selección.", 0
t_no_guardar db "No se ha podido guardar la imagen.", 0
t_ayuda_tit db "Cómo se usa Pintura", 0
t_ayuda     db "Elija una herramienta en la barra de arriba (o con su tecla):", 10
            db "L lápiz, P pincel, G goma, N línea, R rectángulo, E elipse,", 10
            db "B relleno, C cuentagotas, T texto y S selección.", 10
            db 10
            db "El botón izquierdo dibuja con el color primario y el derecho", 10
            db "con el secundario (los dos rombos; un clic en ellos los cambia).", 10
            db "En la paleta, cada botón elige su color.", 10
            db 10
            db "Al lado de las herramientas están los grosores (teclas + y -)", 10
            db "y la forma: contorno, lleno o contorno con fondo.", 10
            db 10
            db "Selección: se marca un recuadro y se arrastra para moverlo;", 10
            db "Ctrl+X, Ctrl+C y Ctrl+V cortan, copian y pegan; Supr borra;", 10
            db "Esc la deja donde está. Ctrl+Z deshace y Ctrl+Y rehace.", 10
            db "Las flechas mueven el lienzo cuando no cabe en la ventana.", 0
t_acerca    db "Pintura", 10, "El programa de dibujo de Hispa Ventanas.", 10
            db "Lee y guarda imágenes BMP.", 0

; los dibujos de las herramientas: 16 x 16, «.» transparente y 0-9, a-f los
; colores (se pasan a números al empezar)
iconos:
        ; lápiz
        db "............00.."
        db "...........0440."
        db "..........04440."
        db ".........0aa40.."
        db "........0aaa0..."
        db ".......0aaa0...."
        db "......0aaa0....."
        db ".....0aaa0......"
        db "....0aaa0......."
        db "...0aaa0........"
        db "..06aa0........."
        db "..0660.........."
        db ".00060.........."
        db ".0000..........."
        db ".00............."
        db "................"
        ; pincel
        db ".............00."
        db "............0110"
        db "...........01110"
        db "..........01110."
        db ".........01110.."
        db "........01110..."
        db ".......07710...."
        db "......07770....."
        db ".....044470....."
        db "....0444440....."
        db "...04444440....."
        db "..0444440......."
        db ".0444400........"
        db ".04400.........."
        db ".000............"
        db "................"
        ; goma
        db "................"
        db "................"
        db ".......00000000."
        db "......055555550."
        db ".....0555555500."
        db "....05555555050."
        db "...055555550550."
        db "..0000000005500."
        db "..099999990550.."
        db "..09999999050..."
        db "..0999999900...."
        db "..099999990....."
        db "..000000000....."
        db "................"
        db "................"
        db "................"
        ; línea
        db "................"
        db "............ccc."
        db "............ccc."
        db "...........0ccc."
        db "..........00...."
        db ".........00....."
        db "........00......"
        db ".......00......."
        db "......00........"
        db ".....00........."
        db "....00.........."
        db ".ccc0..........."
        db ".ccc............"
        db ".ccc............"
        db "................"
        db "................"
        ; rectángulo
        db "................"
        db "................"
        db ".00000000000000."
        db ".00000000000000."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00eeeeeeeeee00."
        db ".00000000000000."
        db ".00000000000000."
        db "................"
        db "................"
        ; elipse
        db "................"
        db "................"
        db ".....000000....."
        db "...0000000000..."
        db "..000aaaaaa000.."
        db ".000aaaaaaaa000."
        db ".00aaaaaaaaaa00."
        db ".00aaaaaaaaaa00."
        db ".00aaaaaaaaaa00."
        db ".00aaaaaaaaaa00."
        db ".000aaaaaaaa000."
        db "..000aaaaaa000.."
        db "...0000000000..."
        db ".....000000....."
        db "................"
        db "................"
        ; relleno: un cubo que se vuelca
        db "................"
        db "......00........"
        db ".....0..0......."
        db "....0.0..0......"
        db "...0...0..0....."
        db "..0.....0..0...."
        db ".0444444440.0..."
        db ".04444444440c..."
        db "..044444440.cc.."
        db "...0444440..cc.."
        db "....04440...ccc."
        db ".....040....ccc."
        db "......0......c.."
        db "................"
        db "................"
        db "................"
        ; cuentagotas
        db "............00.."
        db "...........0000."
        db "..........00000."
        db ".........0.000.."
        db "........0990...."
        db ".......09990...."
        db "......0999.0...."
        db ".....0999.0....."
        db "....0999.0......"
        db "...0999.0......."
        db "..0c99.0........"
        db "..0cc00........."
        db "..000..........."
        db ".c.............."
        db ".c.............."
        db "................"
        ; texto
        db "................"
        db ".......00......."
        db "......0000......"
        db "......0000......"
        db ".....00..00....."
        db ".....00..00....."
        db "....00....00...."
        db "....00....00...."
        db "...0000000000..."
        db "...0000000000..."
        db "..00........00.."
        db "..00........00.."
        db ".0000......0000."
        db "................"
        db "................"
        db "................"
        ; selección
        db "................"
        db "................"
        db ".00.00.00.00.00."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".00.00.00.00.00."
        db "................"
        db "................"
        db "................"

; ---- la entrada: EBX=base ESI=ruta del módulo EDI=parámetros ----
entrada:
        mov [param], edi
        HV pantalla                 ; en una pantalla pequeña, más arriba
        cmp eax, 800
        jae .p
        mov dword [pin_plant+P_X], 16
        mov dword [pin_plant+P_Y], 32
        mov dword [col_plant+P_X], 134
        mov dword [col_plant+P_Y], 120
.p:
        mov dword [col1], C_TINTA
        mov dword [col2], C_BLANCO
        mov esi, iconos             ; los dibujos, de letras a colores
        mov ecx, 10 * 256
.i:
        mov al, [esi]
        cmp al, '.'
        jne .i1
        mov al, CLAVE
        jmp .i3
.i1:
        cmp al, 'a'
        jb .i2
        sub al, 'a' - '9' - 1
.i2:
        sub al, '0'
.i3:
        mov [esi], al
        inc esi
        dec ecx
        jnz .i
        call pon_titulo
        mov esi, pin_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov eax, 480
        mov ebx, 320
        call lienzo_nuevo
        jc .sinmem
        mov esi, [param]            ; ¿un archivo en los parámetros?
.e:
        cmp byte [esi], ' '
        jne .e1
        inc esi
        jmp .e
.e1:
        cmp byte [esi], 0
        je .b
        cmp byte [esi], '/'
        je .b
        mov edi, nom_tmp
        mov ecx, 79
.e2:
        lodsb
        cmp al, ' '
        jbe .e3
        stosb
        dec ecx
        jnz .e2
.e3:
        mov byte [edi], 0
        mov esi, nom_tmp
        call cargar
.b:
        call medidas
        call estado_txt
        call inv_todo
        HV app_bucle
.x:
        ret
.sinmem:
        mov ebp, [ven]
        mov esi, t_sin_mem
        mov edi, t_pintura
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; (interna) el título: «Pintura - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_pintura
        call hvl_cad
        mov esi, t_guion
        call hvl_cad
        mov esi, nombre
        cmp byte [esi], 0
        jne .n
        mov esi, t_sin_nombre
.n:
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, titulo
        HV ven_texto
.x:
        popad
        ret

; ---- el procedimiento de la ventana ----
pin_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONDOWN
        je .izq
        cmp eax, WM_LBUTTONDBLCLK
        je .izq
        cmp eax, WM_RBUTTONDOWN
        je .der
        cmp eax, WM_LBUTTONUP
        je raton_suelta
        cmp eax, WM_RBUTTONUP
        je raton_suelta
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_GETMINMAXINFO   ; [TAMAÑO] lo más pequeño: herramientas y colores
        jne .nmin
        mov eax, 320 + 260 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], pin_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call medidas
        jmp hecho
.tam:
        call medidas
        HV ven_invalidar
        mov eax, WM_SIZE
        HVJ ven_defecto
.cerrar:
        cmp dword [arr], 0
        jne hecho
        call preguntar
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        cmp dword [lz], 0
        je .d1
        mov edx, [lz_mango]
        HV mem_bloque_soltar
        mov dword [lz], 0
.d1:
        call pp_soltar
        HV msg_salir
        jmp hecho
.izq:
        xor esi, esi
        jmp raton_pulsa
.der:
        mov esi, 1
        jmp raton_pulsa

hecho:
        xor eax, eax
        ret

; ---- medidas: la zona de la ventana y lo que se ve del lienzo ----
medidas:
        pushad
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        sub ecx, PANEL              ; lo que queda para el lienzo
        sub edx, BARRA + ESTADO
        mov esi, [lz_an]            ; lo que hace falta
        add esi, 2*MARG
        mov edi, [lz_al]
        add edi, 2*MARG
        xor eax, eax                ; AL=barra horizontal AH=barra vertical
        cmp esi, ecx
        jle .m1
        mov al, 1
        sub edx, BD
.m1:
        cmp edi, edx
        jle .m2
        mov ah, 1
        sub ecx, BD
        test al, al
        jnz .m2
        cmp esi, ecx
        jle .m2
        mov al, 1
        sub edx, BD
.m2:
        test ecx, ecx
        jns .m3
        xor ecx, ecx
.m3:
        test edx, edx
        jns .m4
        xor edx, edx
.m4:
        mov [hay_bh], al
        mov [hay_bv], ah
        mov [vis_an], ecx
        mov [vis_al], edx
        sub esi, ecx                ; hasta dónde se puede desplazar
        jns .m5
        xor esi, esi
.m5:
        sub edi, edx
        jns .m6
        xor edi, edi
.m6:
        mov [max_dx], esi
        mov [max_dy], edi
        mov eax, [des_x]
        test eax, eax
        jns .m7
        xor eax, eax
.m7:
        cmp eax, esi
        jle .m8
        mov eax, esi
.m8:
        mov [des_x], eax
        mov eax, [des_y]
        test eax, eax
        jns .m9
        xor eax, eax
.m9:
        cmp eax, edi
        jle .m10
        mov eax, edi
.m10:
        mov [des_y], eax
        popad
        ret

; EAX=largo de la pista EBX=lo que hace falta ECX=desplazamiento EDX=su máximo
; -> EAX=sitio del tirador ECX=su largo; [tir_rec]=lo que puede recorrer
tirador:
        push ebx
        push edx
        push esi
        push edi
        mov esi, eax
        mov edi, edx
        push ecx
        imul eax, eax
        xor edx, edx
        div ebx
        cmp eax, 16
        jae .a
        mov eax, 16
.a:
        cmp eax, esi
        jbe .b
        mov eax, esi
.b:
        mov ecx, eax
        pop eax
        mov ebx, esi
        sub ebx, ecx
        mov [tir_rec], ebx
        imul eax, ebx
        xor edx, edx
        test edi, edi
        jz .z
        div edi
        jmp .x
.z:
        xor eax, eax
.x:
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

; EAX=x EBX=y de la ventana -> EAX, EBX del lienzo
a_lienzo:
        sub eax, MARG
        add eax, [des_x]
        sub ebx, BARRA + MARG
        add ebx, [des_y]
        ret

; EAX, EBX -> dentro del lienzo
acota:
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp eax, [lz_an]
        jl .b
        mov eax, [lz_an]
        dec eax
.b:
        test ebx, ebx
        jns .c
        xor ebx, ebx
.c:
        cmp ebx, [lz_al]
        jl .d
        mov ebx, [lz_al]
        dec ebx
.d:
        ret

; ---- lo que hay que repintar ----
inv_todo:
        push ebp
        mov ebp, [ven]
        HV ven_invalidar
        pop ebp
        ret
inv_vista:
        pushad
        xor eax, eax
        mov ebx, BARRA
        mov ecx, [vis_an]
        mov edx, [vis_al]
        jmp inv_rect
inv_barra:
        pushad
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, BARRA
        jmp inv_rect
inv_panel:
        pushad
        mov eax, [cli_an]
        sub eax, PANEL
        mov ebx, BARRA
        mov ecx, PANEL
        mov edx, [cli_al]
        jmp inv_rect
inv_estado:
        pushad
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, ESTADO
        mov ecx, [cli_an]
        mov edx, ESTADO
        jmp inv_rect
; lo que se ha dibujado en el lienzo (d_x0, d_y0)-(d_x1, d_y1)
inv_sucio:
        pushad
        mov eax, [d_x0]
        mov ecx, [d_x1]
        sub ecx, eax
        jle inv_rect.x
        mov ebx, [d_y0]
        mov edx, [d_y1]
        sub edx, ebx
        jle inv_rect.x
        add eax, MARG
        sub eax, [des_x]
        add ebx, BARRA + MARG
        sub ebx, [des_y]
inv_rect:
        mov ebp, [ven]
        HV ven_invalidar_rect
.x:
        popad
        ret

sucio_cero:
        mov dword [d_x0], 0x7FFFFFFF
        mov dword [d_y0], 0x7FFFFFFF
        mov dword [d_x1], 0
        mov dword [d_y1], 0
        ret

; (interna) la barra de estado: herramienta, tamaño del lienzo y punto
estado_txt:
        pushad
        mov edi, est_txt
        mov eax, [herr]
        mov esi, [nom_herr+eax*4]
        call hvl_cad
        mov esi, t_sep
        call hvl_cad
        mov eax, [lz_an]
        call hvl_num
        mov esi, t_por
        call hvl_cad
        mov eax, [lz_al]
        call hvl_num
        mov eax, [est_x]
        cmp eax, [lz_an]
        jae .fuera
        mov ebx, [est_y]
        cmp ebx, [lz_al]
        jae .fuera
        mov esi, t_sep
        call hvl_cad
        call hvl_num
        mov ax, ', '
        stosw
        mov eax, ebx
        call hvl_num
.fuera:
        mov byte [edi], 0
        popad
        jmp inv_estado

; ---- pintar ----
; EAX=x ECX=ancho ESI=1 si está elegido: el recuadro de un botón de la barra
boton:
        push ebx
        push edx
        mov ebx, 3
        mov edx, 28
        test esi, esi
        jz .no
        COLOR C_CREMA
        HV dc_rect
        COLOR C_TERRACOTA
        HV dc_marco
        push eax
        push ecx
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        pop ecx
        pop eax
        jmp .x
.no:
        COLOR C_ARENA_OSC
        HV dc_marco
.x:
        pop edx
        pop ebx
        ret

pintar:
        call medidas
        HV dc_pintar
        ; ---- la barra de herramientas ----
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, BARRA - 1
        HV dc_rect
        COLOR C_MARRON
        mov ebx, BARRA - 1
        mov edx, 1
        HV dc_rect
        xor edi, edi                ; las herramientas
.h:
        imul eax, edi, 30
        add eax, 4
        mov ecx, 28
        xor esi, esi
        cmp edi, [herr]
        jne .h1
        inc esi
.h1:
        call boton
        add eax, 6
        mov ebx, 9
        mov ecx, 16
        mov edx, 16
        mov esi, edi
        shl esi, 8
        add esi, iconos
        HV dc_copiar_clave
        inc edi
        cmp edi, 10
        jb .h
        xor edi, edi                ; los grosores
.g:
        imul eax, edi, 24
        add eax, 312
        mov ecx, 22
        xor esi, esi
        cmp edi, [gros]
        jne .g1
        inc esi
.g1:
        call boton
        COLOR C_TINTA
        add eax, 4
        mov ecx, 14
        movzx edx, byte [gros_tab+edi]
        mov ebx, edx
        shr ebx, 1
        neg ebx
        add ebx, 17
        HV dc_rect
        inc edi
        cmp edi, 4
        jb .g
        xor edi, edi                ; las formas
.f:
        imul eax, edi, 28
        add eax, 416
        mov ecx, 26
        xor esi, esi
        cmp edi, [forma]
        jne .f1
        inc esi
.f1:
        call boton
        add eax, 5
        mov ebx, 10
        mov ecx, 16
        mov edx, 14
        cmp edi, 1
        jb .f3
        je .f2
        COLOR C_OCRE
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        jmp .fs
.f2:
        COLOR C_TINTA
        HV dc_rect
        jmp .fs
.f3:
        COLOR C_TINTA
        HV dc_marco
.fs:
        inc edi
        cmp edi, 3
        jb .f
        ; ---- el panel de colores ----
        mov eax, [cli_an]
        sub eax, PANEL
        mov [pan_x], eax
        COLOR C_ARENA
        mov ebx, BARRA
        mov ecx, PANEL
        mov edx, [cli_al]
        sub edx, BARRA + ESTADO
        HV dc_rect
        COLOR C_MARRON
        mov ecx, 1
        HV dc_rect
        mov eax, [pan_x]            ; el color secundario, detrás
        add eax, 50
        mov ebx, BARRA + 28
        mov ecx, 14
        COLOR C_TINTA
        HV dc_rombo
        sub ecx, 2
        COLOR [col2]
        HV dc_rombo
        mov eax, [pan_x]            ; y el primario, delante
        add eax, 28
        mov ebx, BARRA + 22
        mov ecx, 18
        COLOR C_TINTA
        HV dc_rombo
        sub ecx, 2
        COLOR [col1]
        HV dc_rombo
        xor edi, edi                ; la paleta
.p:
        mov eax, edi
        and eax, 3
        imul eax, 17
        add eax, [pan_x]
        add eax, 5
        mov ebx, edi
        shr ebx, 2
        imul ebx, 17
        add ebx, BARRA + PAL_Y
        mov ecx, 16
        mov edx, 16
        COLOR C_MARRON
        HV dc_marco
        inc eax
        inc ebx
        mov ecx, 14
        mov edx, 14
        push eax
        movzx eax, byte [tabla_col+edi]
        HV dc_color
        pop eax
        HV dc_rect
        inc edi
        cmp edi, 56
        jb .p
        ; ---- la barra de estado ----
        COLOR C_PIEDRA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, ESTADO
        mov ecx, [cli_an]
        mov edx, ESTADO
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        COLOR C_TINTA
        mov eax, 8
        add ebx, 2
        mov esi, est_txt
        HV dc_texto
        cmp dword [lz], 0
        je .fin
        ; ---- las barras de desplazamiento ----
        cmp byte [hay_bh], 0
        je .sbh
        xor eax, eax
        mov ebx, BARRA
        add ebx, [vis_al]
        mov ecx, [vis_an]
        mov edx, BD
        COLOR C_PIEDRA
        HV dc_rect
        push ebx
        mov eax, [vis_an]
        mov ebx, [lz_an]
        add ebx, 2*MARG
        mov ecx, [des_x]
        mov edx, [max_dx]
        call tirador
        pop ebx
        inc ebx
        mov edx, BD - 2
        COLOR C_ARENA
        HV dc_rect
        COLOR C_MARRON
        HV dc_marco
.sbh:
        cmp byte [hay_bv], 0
        je .sbv
        mov eax, [vis_an]
        mov ebx, BARRA
        mov ecx, BD
        mov edx, [cli_al]
        sub edx, BARRA + ESTADO
        COLOR C_PIEDRA
        HV dc_rect
        mov eax, [vis_al]
        mov ebx, [lz_al]
        add ebx, 2*MARG
        mov ecx, [des_y]
        mov edx, [max_dy]
        call tirador
        lea ebx, [eax+BARRA]
        mov edx, ecx
        mov eax, [vis_an]
        inc eax
        mov ecx, BD - 2
        COLOR C_ARENA
        HV dc_rect
        COLOR C_MARRON
        HV dc_marco
.sbv:
        ; ---- el lienzo ----
        xor eax, eax
        mov ebx, BARRA
        mov ecx, [vis_an]
        mov edx, [vis_al]
        HV dc_cortar
        COLOR C_ARENA_OSC
        HV dc_rect
        mov eax, MARG - 1
        sub eax, [des_x]
        mov ebx, BARRA + MARG - 1
        sub ebx, [des_y]
        mov ecx, [lz_an]
        add ecx, 2
        mov edx, [lz_al]
        add edx, 2
        COLOR C_TINTA
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        mov esi, [lz]
        HV dc_copiar
        cmp dword [sel_hay], 0
        je .fin
        HV dc_cortar                ; la selección no se sale del lienzo
        add eax, [sel_x]
        add ebx, [sel_y]
        mov ecx, [sel_an]
        mov edx, [sel_al]
        cmp dword [sel_hay], 2
        jne .marco
        mov esi, [flo]              ; lo que flota, encima
        HV dc_copiar
.marco:
        call marquesina
.fin:
        HV dc_cerrar
        jmp hecho

; EAX=x EBX=y ECX=ancho EDX=alto: el recuadro de la selección, a rayas
marquesina:
        pushad
        COLOR C_BLANCO
        HV dc_marco
        COLOR C_TINTA
        mov esi, ecx
        mov edi, edx
        xor ebp, ebp                ; las rayas de arriba y de abajo
.h:
        cmp ebp, esi
        jge .v
        mov ecx, esi
        sub ecx, ebp
        cmp ecx, 4
        jle .h1
        mov ecx, 4
.h1:
        mov edx, 1
        push eax
        add eax, ebp
        HV dc_rect
        push ebx
        add ebx, edi
        dec ebx
        HV dc_rect
        pop ebx
        pop eax
        add ebp, 8
        jmp .h
.v:
        xor ebp, ebp                ; y las de los lados
.v0:
        cmp ebp, edi
        jge .x
        mov edx, edi
        sub edx, ebp
        cmp edx, 4
        jle .v1
        mov edx, 4
.v1:
        mov ecx, 1
        push ebx
        add ebx, ebp
        HV dc_rect
        push eax
        add eax, esi
        dec eax
        HV dc_rect
        pop eax
        pop ebx
        add ebp, 8
        jmp .v0
.x:
        popad
        ret

; ---- el ratón ----
; ESI=botón (0 izquierdo, 1 derecho) EDX=x + y*65536
raton_pulsa:
        cmp dword [arr], 0
        jne hecho
        mov [arr_bot], esi
        call medidas
        movsx eax, dx
        sar edx, 16
        mov ebx, edx
        cmp ebx, BARRA
        jl en_barra
        mov ecx, [cli_al]
        sub ecx, ESTADO
        cmp ebx, ecx
        jge hecho
        mov ecx, [cli_an]
        sub ecx, PANEL
        cmp eax, ecx
        jge en_panel
        cmp dword [lz], 0
        je hecho
        cmp eax, [vis_an]
        jge en_bv
        mov ecx, [vis_al]
        add ecx, BARRA
        cmp ebx, ecx
        jge en_bh
        call a_lienzo               ; en el lienzo: la herramienta
        mov ecx, [col1]
        mov edx, [col2]
        test esi, esi
        jz .c
        xchg ecx, edx
.c:
        mov [col_a], ecx
        mov [col_b], edx
        mov [ux], eax
        mov [uy], ebx
        mov [ax0], eax
        mov [ay0], ebx
        mov ecx, [herr]
        jmp [tab_pulsa+ecx*4]

; en la barra de herramientas: EAX=x
en_barra:
        sub eax, 4
        jl hecho
        cmp eax, 300
        jae .g
        xor edx, edx
        mov ecx, 30
        div ecx
        cmp edx, 28
        jae hecho
        call poner_herr
        jmp hecho
.g:
        sub eax, 308
        jl hecho
        cmp eax, 96
        jae .f
        xor edx, edx
        mov ecx, 24
        div ecx
        mov [gros], eax
        jmp .inv
.f:
        sub eax, 104
        jl hecho
        cmp eax, 84
        jae hecho
        xor edx, edx
        mov ecx, 28
        div ecx
        mov [forma], eax
.inv:
        call inv_barra
        jmp hecho

; EAX=herramienta: la elige
poner_herr:
        cmp eax, H_SELEC
        je .s
        call sel_quitar
.s:
        mov [herr], eax
        call inv_barra
        jmp estado_txt

; en el panel: EAX=x EBX=y ESI=botón
en_panel:
        mov ecx, [cli_an]
        sub ecx, PANEL
        sub eax, ecx
        sub ebx, BARRA
        cmp ebx, PAL_Y
        jl .rombos
        sub ebx, PAL_Y
        sub eax, 5
        jl hecho
        cmp eax, 68
        jae hecho
        push eax
        mov eax, ebx
        xor edx, edx
        mov ecx, 17
        div ecx
        mov ebx, eax                ; la fila
        pop eax
        cmp ebx, 14
        jae hecho
        xor edx, edx
        div ecx
        lea eax, [eax+ebx*4]
        movzx eax, byte [tabla_col+eax]
        call poner_color
        jmp hecho
.rombos:
        test esi, esi
        jnz hecho
        call intercambiar
        jmp hecho

; EAX=color ESI=botón: el primario o el secundario
poner_color:
        mov [col1+esi*4], eax
        jmp inv_panel

intercambiar:
        mov eax, [col1]
        xchg eax, [col2]
        mov [col1], eax
        jmp inv_panel

; en la barra vertical: EBX=y
en_bv:
        test esi, esi
        jnz hecho
        mov ecx, [vis_al]
        add ecx, BARRA
        cmp ebx, ecx
        jge hecho
        sub ebx, BARRA
        push ebx
        mov eax, [vis_al]
        mov ebx, [lz_al]
        add ebx, 2*MARG
        mov ecx, [des_y]
        mov edx, [max_dy]
        call tirador
        pop ebx
        cmp ebx, eax
        jl .antes
        add ecx, eax
        cmp ebx, ecx
        jge .despues
        sub ebx, eax
        mov [arr_off], ebx
        mov dword [arr], A_BV
        jmp hecho
.antes:
        mov eax, [vis_al]
        neg eax
        jmp .pag
.despues:
        mov eax, [vis_al]
.pag:
        add [des_y], eax
        jmp desplazado

; en la barra horizontal: EAX=x
en_bh:
        test esi, esi
        jnz hecho
        cmp byte [hay_bh], 0
        je hecho
        push eax
        mov eax, [vis_an]
        mov ebx, [lz_an]
        add ebx, 2*MARG
        mov ecx, [des_x]
        mov edx, [max_dx]
        call tirador
        pop ebx
        cmp ebx, eax
        jl .antes
        add ecx, eax
        cmp ebx, ecx
        jge .despues
        sub ebx, eax
        mov [arr_off], ebx
        mov dword [arr], A_BH
        jmp hecho
.antes:
        mov eax, [vis_an]
        neg eax
        jmp .pag
.despues:
        mov eax, [vis_an]
.pag:
        add [des_x], eax
desplazado:
        call medidas
        call inv_todo
        jmp hecho

; ---- las herramientas, al apretar: EAX, EBX=punto del lienzo ESI=botón ----
; lápiz, pincel y goma
p_trazo:
        call sel_quitar
        call des_guardar
        call prepara
        call sucio_cero
        call sello
        mov dword [arr], A_TRAZO
        mov byte [modif], 1
        call inv_sucio
        jmp hecho

; (interna) el sello y la tinta de la herramienta elegida
prepara:
        push eax
        push ecx
        mov ecx, [gros]
        movzx ecx, byte [gros_tab+ecx]
        mov byte [s_cuad], 0
        mov al, [col_a]
        mov [tinta], al
        mov eax, [herr]
        test eax, eax
        jnz .a
        mov ecx, 1                  ; el lápiz: un punto
.a:
        cmp eax, 2
        jne .b
        lea ecx, [ecx*2+2]          ; la goma: cuadrada y con el secundario
        mov byte [s_cuad], 1
        mov al, [col2]
        mov [tinta], al
.b:
        mov [s_d], ecx
        pop ecx
        pop eax
        ret

; línea, rectángulo y elipse
p_forma:
        call sel_quitar
        call des_guardar
        mov dword [arr], A_FORMA
        mov byte [modif], 1
        call sucio_cero
        call forma_dibuja
        call p_guarda
        call inv_sucio
        jmp hecho

; (interna) lo dibujado pasa a ser «lo de antes» (p_*)
p_guarda:
        push eax
        mov eax, [d_x0]
        mov [p_x0], eax
        mov eax, [d_y0]
        mov [p_y0], eax
        mov eax, [d_x1]
        mov [p_x1], eax
        mov eax, [d_y1]
        mov [p_y1], eax
        pop eax
        ret

p_relleno:
        call sel_quitar
        cmp eax, [lz_an]
        jae hecho
        cmp ebx, [lz_al]
        jae hecho
        mov esi, ebx
        imul esi, [lz_an]
        add esi, [lz]
        mov dl, [esi+eax]
        cmp dl, [col_a]
        je hecho
        call des_guardar
        call rellenar
        mov byte [modif], 1
        call inv_vista
        jmp hecho

p_gotas:
        cmp eax, [lz_an]
        jae hecho
        cmp ebx, [lz_al]
        jae hecho
        imul ebx, [lz_an]
        add ebx, [lz]
        movzx eax, byte [ebx+eax]
        call poner_color
        jmp hecho

p_texto:
        call sel_quitar
        mov byte [txt], 0
        mov ebp, [ven]
        mov esi, t_txt_preg
        mov edi, t_txt_tit
        mov ebx, txt
        mov ecx, 60
        HV dlg_pedir
        cmp eax, 1
        jne hecho
        cmp byte [txt], 0
        je hecho
        call des_guardar
        call sucio_cero
        mov al, [col_a]
        mov [tinta], al
        call texto_dibuja
        mov byte [modif], 1
        call inv_sucio
        jmp hecho

p_selec:
        test esi, esi
        jnz .quita
        cmp dword [sel_hay], 0
        je .nueva
        mov ecx, eax                ; ¿dentro de lo seleccionado? Se mueve
        sub ecx, [sel_x]
        cmp ecx, [sel_an]
        jae .nueva
        mov edx, ebx
        sub edx, [sel_y]
        cmp edx, [sel_al]
        jae .nueva
        mov [mov_dx], ecx
        mov [mov_dy], edx
        cmp dword [sel_hay], 2
        je .m
        call levantar
.m:
        mov dword [arr], A_MOVER
        call inv_vista
        jmp hecho
.nueva:
        call sel_quitar
        call acota
        mov [ax0], eax
        mov [ay0], ebx
        mov dword [arr], A_MARCO
        jmp hecho
.quita:
        call sel_quitar
        jmp hecho

; ---- el ratón se mueve ----
raton_mueve:
        movsx eax, dx
        sar edx, 16
        mov ebx, edx
        mov [rx], eax
        mov [ry], ebx
        call a_lienzo
        cmp eax, [est_x]
        jne .cambia
        cmp ebx, [est_y]
        je .igual
.cambia:
        mov [est_x], eax
        mov [est_y], ebx
        call estado_txt
.igual:
        mov ecx, [arr]
        jmp [tab_mueve+ecx*4]

m_trazo:
        call sucio_cero
        mov ecx, eax
        mov edx, ebx
        xchg eax, [ux]
        xchg ebx, [uy]
        call linea
        call inv_sucio
        jmp hecho

m_forma:
        call restaura_p             ; fuera la de antes
        push dword [p_x0]
        push dword [p_y0]
        push dword [p_x1]
        push dword [p_y1]
        mov [ux], eax
        mov [uy], ebx
        call sucio_cero
        call forma_dibuja
        call p_guarda
        pop eax                     ; se repinta lo de antes y lo nuevo
        cmp eax, [d_y1]
        jle .a
        mov [d_y1], eax
.a:
        pop eax
        cmp eax, [d_x1]
        jle .b
        mov [d_x1], eax
.b:
        pop eax
        cmp eax, [d_y0]
        jge .c
        mov [d_y0], eax
.c:
        pop eax
        cmp eax, [d_x0]
        jge .d
        mov [d_x0], eax
.d:
        call inv_sucio
        jmp hecho

m_marco:
        call acota
        mov ecx, [ax0]
        cmp eax, ecx
        jge .a
        xchg eax, ecx
.a:
        sub eax, ecx
        inc eax
        mov [sel_x], ecx
        mov [sel_an], eax
        mov ecx, [ay0]
        cmp ebx, ecx
        jge .b
        xchg ebx, ecx
.b:
        sub ebx, ecx
        inc ebx
        mov [sel_y], ecx
        mov [sel_al], ebx
        mov dword [sel_hay], 1
        call inv_vista
        jmp hecho

m_mover:
        sub eax, [mov_dx]
        sub ebx, [mov_dy]
        mov [sel_x], eax
        mov [sel_y], ebx
        call inv_vista
        jmp hecho

m_bh:
        mov eax, [vis_an]
        mov ebx, [lz_an]
        add ebx, 2*MARG
        mov ecx, [des_x]
        mov edx, [max_dx]
        call tirador
        mov eax, [rx]
        sub eax, [arr_off]
        jns .a
        xor eax, eax
.a:
        imul eax, [max_dx]
        xor edx, edx
        mov ecx, [tir_rec]
        test ecx, ecx
        jz hecho
        div ecx
        mov [des_x], eax
        jmp desplazado

m_bv:
        mov eax, [vis_al]
        mov ebx, [lz_al]
        add ebx, 2*MARG
        mov ecx, [des_y]
        mov edx, [max_dy]
        call tirador
        mov eax, [ry]
        sub eax, BARRA
        sub eax, [arr_off]
        jns .a
        xor eax, eax
.a:
        imul eax, [max_dy]
        xor edx, edx
        mov ecx, [tir_rec]
        test ecx, ecx
        jz hecho
        div ecx
        mov [des_y], eax
        jmp desplazado

; ---- se suelta el botón ----
raton_suelta:
        mov ecx, [arr]
        mov dword [arr], 0
        cmp ecx, A_MARCO
        jne hecho
        cmp dword [sel_hay], 1      ; un recuadro de un punto no es nada
        jne hecho
        cmp dword [sel_an], 2
        jae hecho
        cmp dword [sel_al], 2
        jae hecho
        mov dword [sel_hay], 0
        call inv_vista
        jmp hecho

; ---- el teclado ----
tecla:
        mov esi, tab_tecla
.b:
        mov eax, [esi]
        test eax, eax
        jz .flechas
        cmp eax, ecx
        je .si
        add esi, 8
        jmp .b
.si:
        mov ecx, [esi+4]
        jmp orden
.flechas:
        xor eax, eax
        xor ebx, ebx
        cmp ecx, TC_IZQ
        jne .f1
        mov eax, -16
.f1:
        cmp ecx, TC_DER
        jne .f2
        mov eax, 16
.f2:
        cmp ecx, TC_ARRIBA
        jne .f3
        mov ebx, -16
.f3:
        cmp ecx, TC_ABAJO
        jne .f4
        mov ebx, 16
.f4:
        cmp ecx, TC_REPAG
        jne .f5
        mov ebx, [vis_al]
        neg ebx
.f5:
        cmp ecx, TC_AVPAG
        jne .f6
        mov ebx, [vis_al]
.f6:
        mov esi, eax
        or esi, ebx
        jz .no
        add [des_x], eax
        add [des_y], ebx
        jmp desplazado
.no:
        mov eax, WM_KEYDOWN
        HVJ ven_defecto

letra:
        mov eax, ecx
        cmp al, '+'
        je .mas
        cmp al, '-'
        je .menos
        cmp al, 'a'
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 0x20
.m:
        xor ebx, ebx
.b:
        mov dl, [letras+ebx]
        test dl, dl
        jz hecho
        cmp dl, al
        je .si
        inc ebx
        jmp .b
.si:
        cmp dword [arr], 0
        jne hecho
        mov eax, ebx
        call poner_herr
        jmp hecho
.mas:
        cmp dword [gros], 3
        jae hecho
        inc dword [gros]
        jmp .g
.menos:
        cmp dword [gros], 0
        je hecho
        dec dword [gros]
.g:
        call inv_barra
        jmp hecho

; ---- las órdenes del menú: ECX=número ----
orden:
        mov esi, tab_orden
.b:
        mov eax, [esi]
        test eax, eax
        jz hecho
        cmp eax, ecx
        je .si
        add esi, 8
        jmp .b
.si:
        cmp dword [arr], 0          ; (con el botón apretado, no)
        jne hecho
        call [esi+4]
        jmp hecho

o_salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        ret

o_ayuda:
        mov esi, t_ayuda
        mov edi, t_ayuda_tit
        jmp mensaje.t
o_acerca:
        mov esi, t_acerca
; ESI=texto: un aviso con «Aceptar»
mensaje:
        mov edi, t_pintura
.t:
        push eax
        push ebp
        mov ebp, [ven]
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        pop ebp
        pop eax
        ret

o_nuevo:
        call preguntar
        jc .x
        mov edi, num                ; por omisión, el tamaño que hay
        mov eax, [lz_an]
        call hvl_num
        mov al, 'x'
        stosb
        mov eax, [lz_al]
        call hvl_num
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, t_nuevo_preg
        mov edi, t_nuevo_tit
        mov ebx, num
        mov ecx, 12
        HV dlg_pedir
        cmp eax, 1
        jne .x
        mov esi, num
        call hvl_lee_num
        mov ebx, eax
.s:
        mov al, [esi]               ; lo que separa los dos números
        test al, al
        jz .s2
        sub al, '0'
        cmp al, 9
        jbe .s2
        inc esi
        jmp .s
.s2:
        call hvl_lee_num
        xchg eax, ebx               ; EAX=ancho EBX=alto, entre 8 y el máximo
        cmp eax, 8
        jae .a
        mov eax, 8
.a:
        cmp eax, MAXAN
        jbe .b
        mov eax, MAXAN
.b:
        cmp ebx, 8
        jae .c
        mov ebx, 8
.c:
        cmp ebx, MAXAL
        jbe .d
        mov ebx, MAXAL
.d:
        call lienzo_nuevo
        jc .sinmem
        mov byte [nombre], 0
        mov byte [modif], 0
        call pon_titulo
        call medidas
        call estado_txt
        call inv_todo
.x:
        ret
.sinmem:
        mov esi, t_sin_mem
        jmp mensaje

o_abrir:
        call preguntar
        jc .x
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, nom_tmp
        call cargar
.x:
        ret

; ---- el lienzo y su memoria ----
; EAX=ancho EBX=alto: un lienzo nuevo, en blanco. CF=1 si no hay memoria (y
; sigue el que había)
lienzo_nuevo:
        pushad
        mov esi, eax
        mov edi, ebx
        imul eax, ebx
        add eax, 3
        and eax, ~3
        mov ebp, eax                ; octetos del lienzo
        mov ebx, 8                  ; pasos de deshacer
.n:
        lea ecx, [ebx+2]
        imul ecx, ebp
        cmp ebx, 1
        je .pide
        cmp ecx, MEM_TOPE
        ja .menos
.pide:
        HV mem_bloque
        jnc .hay
        cmp ebx, 1
        je .no
.menos:
        dec ebx
        jmp .n
.hay:
        push eax
        push edx
        cmp dword [lz], 0
        je .h1
        mov edx, [lz_mango]
        HV mem_bloque_soltar
.h1:
        pop edx
        pop eax
        mov [lz_mango], edx
        mov [lz], eax
        add eax, ebp
        mov [flo], eax
        add eax, ebp
        mov [des_buf], eax
        mov [des_n], ebx
        mov [lz_an], esi
        mov [lz_al], edi
        mov [lz_tam], ebp
        xor eax, eax
        mov [des_cab], eax
        mov [des_cuenta], eax
        mov [des_re], eax
        mov [sel_hay], eax
        mov [des_x], eax
        mov [des_y], eax
        mov [arr], eax
        mov edi, [lz]
        mov ecx, ebp
        mov al, C_BLANCO
        rep stosb
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; ---- deshacer ----
; guarda el lienzo antes de cambiarlo
des_guardar:
        pushad
        mov eax, [des_cab]
        imul eax, [lz_tam]
        add eax, [des_buf]
        mov edi, eax
        mov esi, [lz]
        mov ecx, [lz_tam]
        shr ecx, 2
        rep movsd
        mov eax, [des_cab]
        inc eax
        cmp eax, [des_n]
        jb .a
        xor eax, eax
.a:
        mov [des_cab], eax
        mov eax, [des_cuenta]
        cmp eax, [des_n]
        jae .b
        inc eax
        mov [des_cuenta], eax
.b:
        mov dword [des_re], 0
        popad
        ret

; (interna) EAX=paso: cambia el lienzo por lo guardado en él
des_cambia:
        pushad
        imul eax, [lz_tam]
        add eax, [des_buf]
        mov esi, eax
        mov edi, [lz]
        mov ecx, [lz_tam]
        shr ecx, 2
.c:
        mov edx, [esi]
        xchg edx, [edi]
        mov [esi], edx
        add esi, 4
        add edi, 4
        dec ecx
        jnz .c
        popad
        ret

deshacer:
        cmp dword [sel_hay], 2      ; lo que flotaba se descarta
        jne .a
        mov dword [sel_hay], 0
.a:
        cmp dword [des_cuenta], 0
        je .x
        mov eax, [des_cab]
        test eax, eax
        jnz .b
        mov eax, [des_n]
.b:
        dec eax
        mov [des_cab], eax
        call des_cambia
        dec dword [des_cuenta]
        inc dword [des_re]
        mov byte [modif], 1
.x:
        jmp inv_vista

rehacer:
        cmp dword [des_re], 0
        je .x
        mov eax, [des_cab]
        call des_cambia
        inc eax
        cmp eax, [des_n]
        jb .a
        xor eax, eax
.a:
        mov [des_cab], eax
        inc dword [des_cuenta]
        dec dword [des_re]
        mov byte [modif], 1
        call inv_vista
.x:
        ret

; (interna) repone del último paso guardado el rectángulo p_*
restaura_p:
        pushad
        mov eax, [p_x0]
        mov ecx, [p_x1]
        sub ecx, eax
        jle .x
        mov ebx, [p_y0]
        mov edx, [des_cab]
        test edx, edx
        jnz .a
        mov edx, [des_n]
.a:
        dec edx
        imul edx, [lz_tam]
        add edx, [des_buf]
.f:
        cmp ebx, [p_y1]
        jge .x
        mov esi, ebx
        imul esi, [lz_an]
        add esi, eax
        mov edi, esi
        add esi, edx
        add edi, [lz]
        push ecx
        rep movsb
        pop ecx
        inc ebx
        jmp .f
.x:
        popad
        ret

; ---- dibujar en el lienzo ----
; EAX=x0 ECX=x1 EBX=y: una raya horizontal con [tinta] (recortada)
hlin:
        pushad
        cmp ebx, [lz_al]
        jae .x
        test eax, eax
        jns .a
        xor eax, eax
.a:
        mov edx, [lz_an]
        cmp ecx, edx
        jl .b
        lea ecx, [edx-1]
.b:
        cmp eax, ecx
        jg .x
        cmp eax, [d_x0]
        jge .c
        mov [d_x0], eax
.c:
        inc ecx
        cmp ecx, [d_x1]
        jle .d
        mov [d_x1], ecx
.d:
        cmp ebx, [d_y0]
        jge .e
        mov [d_y0], ebx
.e:
        lea edx, [ebx+1]
        cmp edx, [d_y1]
        jle .f
        mov [d_y1], edx
.f:
        sub ecx, eax
        mov edi, ebx
        imul edi, [lz_an]
        add edi, eax
        add edi, [lz]
        mov al, [tinta]
        rep stosb
.x:
        popad
        ret

; EAX=x EBX=y: el sello, de [s_d] puntos de ancho (redondo, o cuadrado si
; [s_cuad])
sello:
        pushad
        mov edx, [s_d]
        cmp edx, 1
        ja .g
        mov ecx, eax
        call hlin
        jmp .x
.g:
        mov esi, edx
        shr esi, 1
        sub eax, esi                ; la esquina
        sub ebx, esi
        xor edi, edi                ; la fila
.f:
        xor esi, esi                ; cuántos puntos se quitan de cada lado
        cmp byte [s_cuad], 0
        jne .raya
        lea ebp, [edi*2+1]
        sub ebp, edx
        imul ebp, ebp
.b:
        lea ecx, [esi*2+1]
        sub ecx, edx
        imul ecx, ecx
        add ecx, ebp
        push edx
        imul edx, edx
        cmp ecx, edx
        pop edx
        jbe .raya
        inc esi
        lea ecx, [esi*2]
        cmp ecx, edx
        jb .b
        jmp .sig
.raya:
        push eax
        push ebx
        lea ecx, [eax+edx-1]
        sub ecx, esi
        add eax, esi
        add ebx, edi
        call hlin
        pop ebx
        pop eax
.sig:
        inc edi
        cmp edi, edx
        jb .f
.x:
        popad
        ret

; EAX=x0 EBX=y0 ECX=x1 EDX=y1: una línea de sellos
linea:
        pushad
        mov dword [l_sx], 1
        mov dword [l_sy], 1
        mov esi, ecx
        sub esi, eax
        jns .a
        neg esi
        neg dword [l_sx]
.a:
        mov edi, edx
        sub edi, ebx
        jns .b
        neg edi
        neg dword [l_sy]
.b:
        mov ebp, esi                ; el error
        sub ebp, edi
.l:
        call sello
        cmp eax, ecx
        jne .p
        cmp ebx, edx
        je .x
.p:
        push edx
        lea edx, [ebp*2]
        push eax
        mov eax, edi
        neg eax
        cmp edx, eax
        pop eax
        jle .q
        sub ebp, edi
        add eax, [l_sx]
.q:
        cmp edx, esi
        jge .r
        add ebp, esi
        add ebx, [l_sy]
.r:
        pop edx
        jmp .l
.x:
        popad
        ret

; la forma que se está estirando, de (ax0, ay0) a (ux, uy)
forma_dibuja:
        pushad
        mov eax, [ax0]
        mov ebx, [ay0]
        mov ecx, [ux]
        mov edx, [uy]
        cmp dword [herr], 3
        jne .caja
        call prepara
        call linea
        jmp .x
.caja:
        cmp eax, ecx
        jle .c1
        xchg eax, ecx
.c1:
        cmp ebx, edx
        jle .c2
        xchg ebx, edx
.c2:
        mov [r_x0], eax
        mov [r_y0], ebx
        mov [r_x1], ecx
        mov [r_y1], edx
        mov esi, [gros]
        movzx esi, byte [gros_tab+esi]
        add eax, esi                ; la caja de dentro
        add ebx, esi
        sub ecx, esi
        sub edx, esi
        mov [i_x0], eax
        mov [i_y0], ebx
        mov [i_x1], ecx
        mov [i_y1], edx
        mov byte [hay_int], 0
        cmp eax, ecx
        jg .c3
        cmp ebx, edx
        jg .c3
        mov byte [hay_int], 1
.c3:
        cmp dword [herr], 4
        jne .elipse
        call f_rect
        jmp .x
.elipse:
        call f_elipse
.x:
        popad
        ret

; (interna) AL=color: la tinta
%macro TINTA 1
        mov dl, [%1]
        mov [tinta], dl
%endmacro

; el rectángulo r_*: ESI=grosor
f_rect:
        mov ebx, [r_y0]
.f:
        cmp ebx, [r_y1]
        jg .x
        mov eax, [r_x0]
        mov ecx, [r_x1]
        TINTA col_a
        cmp dword [forma], 1
        je .llena
        cmp byte [hay_int], 0       ; tan fino que todo es contorno
        je .llena
        cmp ebx, [i_y0]
        jl .llena
        cmp ebx, [i_y1]
        jg .llena
        mov ecx, [i_x0]             ; los dos lados
        dec ecx
        call hlin
        mov eax, [i_x1]
        inc eax
        mov ecx, [r_x1]
        call hlin
        cmp dword [forma], 2
        jne .s
        TINTA col_b                 ; y el fondo
        mov eax, [i_x0]
        mov ecx, [i_x1]
.llena:
        call hlin
.s:
        inc ebx
        jmp .f
.x:
        ret

; la elipse metida en la caja r_*
f_elipse:
        mov ebx, [r_y0]
.f:
        cmp ebx, [r_y1]
        jg .x
        mov esi, r_x0
        call elip_fila              ; EAX, ECX=de dónde a dónde
        TINTA col_a
        cmp dword [forma], 1
        je .llena
        cmp byte [hay_int], 0
        je .llena
        cmp ebx, [i_y0]
        jl .llena
        cmp ebx, [i_y1]
        jg .llena
        push eax
        push ecx
        mov esi, i_x0
        call elip_fila              ; lo de dentro
        mov [t_l], eax
        mov [t_r], ecx
        cmp dword [forma], 2
        jne .n2
        TINTA col_b
        call hlin
        TINTA col_a
.n2:
        pop ecx
        pop eax
        push ecx
        mov ecx, [t_l]
        dec ecx
        call hlin
        pop ecx
        mov eax, [t_r]
        inc eax
.llena:
        call hlin
        inc ebx
        jmp .f
.x:
        ret

; ESI=caja (x0, y0, x1, y1) EBX=fila -> EAX=primer punto ECX=último de la
; elipse en esa fila. (Con los centros de los puntos: medidas dobladas.)
elip_fila:
        push edx
        push edi
        push ebp
        mov edi, [esi+8]
        sub edi, [esi]              ; ancho - 1
        mov ebp, [esi+12]
        sub ebp, [esi+4]            ; alto - 1
        mov eax, ebx
        sub eax, [esi+4]
        add eax, eax
        sub eax, ebp                ; la fila, desde el centro
        imul eax, eax
        inc ebp
        imul ebp, ebp
        mov edx, ebp
        sub edx, eax
        lea eax, [edi+1]
        imul eax, eax
        mul edx
        div ebp
        call raiz                   ; medio ancho (doblado) en esa fila
        mov edx, edi
        sub edx, eax
        inc edx
        sar edx, 1
        mov eax, [esi]
        add eax, edx
        mov ecx, [esi+8]
        sub ecx, edx
        pop ebp
        pop edi
        pop edx
        ret

; EAX -> EAX=raíz cuadrada (entera)
raiz:
        push ebx
        push ecx
        push edx
        mov ecx, eax
        xor eax, eax
        mov ebx, 0x40000000
.a:
        cmp ebx, ecx
        jbe .l
        shr ebx, 2
        jnz .a
.l:
        test ebx, ebx
        jz .x
        lea edx, [eax+ebx]
        shr eax, 1
        cmp ecx, edx
        jb .n
        sub ecx, edx
        add eax, ebx
.n:
        shr ebx, 2
        jmp .l
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=x EBX=y: cambia por [col_a] la mancha del color que hay ahí. Va por
; tramos de fila; lo pendiente se apunta (x, y) en la zona flotante
rellenar:
        pushad
        mov edi, [lz_an]
        mov esi, ebx
        imul esi, edi
        add esi, [lz]
        mov dl, [esi+eax]
        mov dh, [col_a]
        cmp dl, dh
        je .x
        mov [r_viejo], dl
        mov [r_nuevo], dh
        mov ebp, [flo]
        mov [r_pila], ebp
        mov ecx, [lz_tam]
        add ecx, ebp
        mov [r_tope], ecx
        call .mete
.saca:
        cmp ebp, [r_pila]
        je .x
        sub ebp, 4
        movzx eax, word [ebp]
        movzx ebx, word [ebp+2]
        mov esi, ebx
        imul esi, edi
        add esi, [lz]
        mov dl, [r_viejo]
        cmp [esi+eax], dl
        jne .saca
        mov ecx, eax                ; hasta dónde llega a la izquierda
.izq:
        test ecx, ecx
        jz .der
        cmp [esi+ecx-1], dl
        jne .der
        dec ecx
        jmp .izq
.der:
        inc eax                     ; y a la derecha
        cmp eax, edi
        jae .d2
        cmp [esi+eax], dl
        je .der
.d2:
        push ecx
        mov dh, [r_nuevo]
.ll:
        mov [esi+ecx], dh
        inc ecx
        cmp ecx, eax
        jb .ll
        pop ecx
        dec ebx                     ; la fila de arriba y la de abajo
        call .mira
        add ebx, 2
        call .mira
        jmp .saca
; EBX=fila, de ECX a EAX (fuera): apunta el principio de cada tramo del color
.mira:
        cmp ebx, [lz_al]
        jae .m9
        push ecx
        push esi
        mov esi, ebx
        imul esi, edi
        add esi, [lz]
        xor dh, dh
.m1:
        cmp [esi+ecx], dl
        jne .m3
        test dh, dh
        jnz .m4
        mov dh, 1
        push eax
        mov eax, ecx
        call .mete
        pop eax
        jmp .m4
.m3:
        xor dh, dh
.m4:
        inc ecx
        cmp ecx, eax
        jb .m1
        pop esi
        pop ecx
.m9:
        ret
.mete:
        cmp ebp, [r_tope]
        jae .mt9
        mov [ebp], ax
        mov [ebp+2], bx
        add ebp, 4
.mt9:
        ret
.x:
        popad
        ret

; escribe txt en (ax0, ay0) con [tinta] y la fuente de Hispa Ventanas (más
; grande con los grosores gordos)
texto_dibuja:
        pushad
        mov eax, [gros]
        movzx eax, byte [esc_tab+eax]
        mov [t_esc], eax
        mov esi, txt
        mov edi, [ax0]
.c:
        movzx eax, byte [esi]
        inc esi
        test eax, eax
        jz .x
        shl eax, 4
        add eax, [hv_api + HV_fuente8x16 * 4]
        mov ebp, eax
        xor edx, edx                ; la fila del carácter
.f:
        movzx eax, byte [ebp+edx]
        mov [t_bits], eax
        mov dword [t_col], 0
.b:
        test byte [t_bits], 0x80
        jz .n
        push edx
        mov eax, [t_col]
        imul eax, [t_esc]
        add eax, edi
        mov ecx, [t_esc]
        lea ecx, [eax+ecx-1]
        mov ebx, edx
        imul ebx, [t_esc]
        add ebx, [ay0]
        mov edx, [t_esc]
.r:
        call hlin
        inc ebx
        dec edx
        jnz .r
        pop edx
.n:
        shl byte [t_bits], 1
        inc dword [t_col]
        cmp dword [t_col], 8
        jb .b
        inc edx
        cmp edx, 16
        jb .f
        mov eax, [t_esc]
        shl eax, 3
        add edi, eax
        jmp .c
.x:
        popad
        ret

; ---- la selección ----
; la deja donde está (si flota, se pega en el lienzo) y la quita
sel_quitar:
        cmp dword [sel_hay], 0
        je .x
        cmp dword [sel_hay], 2
        jne .q
        call fijar
.q:
        mov dword [sel_hay], 0
        call inv_vista
.x:
        ret

; (interna) lo que flota, al lienzo (lo que cae fuera se pierde)
fijar:
        pushad
        xor edx, edx
.f:
        cmp edx, [sel_al]
        jae .x
        mov ebx, [sel_y]
        add ebx, edx
        cmp ebx, [lz_al]
        jae .s
        mov eax, [sel_x]
        xor esi, esi
        mov ecx, [sel_an]
        test eax, eax
        jns .a
        sub esi, eax
        add ecx, eax
        xor eax, eax
.a:
        mov edi, [lz_an]
        sub edi, eax
        jle .s
        cmp ecx, edi
        jle .b
        mov ecx, edi
.b:
        test ecx, ecx
        jle .s
        mov edi, ebx
        imul edi, [lz_an]
        add edi, eax
        add edi, [lz]
        mov eax, edx
        imul eax, [sel_an]
        add esi, eax
        add esi, [flo]
        rep movsb
.s:
        inc edx
        jmp .f
.x:
        mov byte [modif], 1
        popad
        ret

; (interna) lo seleccionado sale del lienzo y pasa a flotar; el hueco, con
; el color secundario
levantar:
        pushad
        call des_guardar
        mov edi, [flo]
        mov edx, [sel_y]
        mov ebx, [sel_al]
.f:
        mov esi, edx
        imul esi, [lz_an]
        add esi, [sel_x]
        add esi, [lz]
        mov ecx, [sel_an]
        push esi
        rep movsb
        pop esi
        push edi
        mov edi, esi
        mov ecx, [sel_an]
        mov al, [col2]
        rep stosb
        pop edi
        inc edx
        dec ebx
        jnz .f
        mov dword [sel_hay], 2
        mov byte [modif], 1
        popad
        ret

pp_soltar:
        cmp dword [pp], 0
        je .x
        push edx
        mov edx, [pp_mango]
        HV mem_bloque_soltar
        pop edx
        mov dword [pp], 0
.x:
        ret

; lo seleccionado, al portapapeles de Pintura. CF=1 si no hay nada
copiar:
        pushad
        cmp dword [sel_hay], 0
        je .no
        call pp_soltar
        mov ecx, [sel_an]
        imul ecx, [sel_al]
        HV mem_bloque
        jc .nomem
        mov [pp], eax
        mov [pp_mango], edx
        mov edi, eax
        mov eax, [sel_an]
        mov [pp_an], eax
        mov eax, [sel_al]
        mov [pp_al], eax
        cmp dword [sel_hay], 2
        jne .lz
        mov esi, [flo]
        rep movsb
        jmp .ok
.lz:
        mov edx, [sel_y]
        mov ebx, [sel_al]
.f:
        mov esi, edx
        imul esi, [lz_an]
        add esi, [sel_x]
        add esi, [lz]
        mov ecx, [sel_an]
        rep movsb
        inc edx
        dec ebx
        jnz .f
.ok:
        popad
        clc
        ret
.nomem:
        mov esi, t_sin_mem_c
        call mensaje
.no:
        popad
        stc
        ret

cortar:
        call copiar
        jc borrar.x
; borra lo seleccionado (queda el color secundario)
borrar:
        cmp dword [sel_hay], 0
        je .x
        cmp dword [sel_hay], 2
        je .q
        pushad
        call des_guardar
        TINTA col2
        mov ebx, [sel_y]
        mov edx, [sel_al]
        mov eax, [sel_x]
        mov ecx, [sel_an]
        lea ecx, [eax+ecx-1]
.f:
        call hlin
        inc ebx
        dec edx
        jnz .f
        mov byte [modif], 1
        popad
.q:
        mov dword [sel_hay], 0
        call inv_vista
.x:
        ret

; lo del portapapeles, flotando en la esquina de lo que se ve
pegar:
        cmp dword [pp], 0
        je .x
        pushad
        call sel_quitar
        call des_guardar
        mov eax, [pp_an]
        cmp eax, [lz_an]
        jbe .a
        mov eax, [lz_an]
.a:
        mov [sel_an], eax
        mov ebx, [pp_al]
        cmp ebx, [lz_al]
        jbe .b
        mov ebx, [lz_al]
.b:
        mov [sel_al], ebx
        mov ecx, [lz_an]            ; dentro del lienzo
        sub ecx, eax
        mov eax, [des_x]
        cmp eax, ecx
        jbe .c
        mov eax, ecx
.c:
        mov [sel_x], eax
        mov ecx, [lz_al]
        sub ecx, ebx
        mov eax, [des_y]
        cmp eax, ecx
        jbe .d
        mov eax, ecx
.d:
        mov [sel_y], eax
        mov edi, [flo]
        mov edx, [pp]
.f:
        mov esi, edx
        mov ecx, [sel_an]
        rep movsb
        add edx, [pp_an]
        dec ebx
        jnz .f
        mov dword [sel_hay], 2
        mov byte [modif], 1
        mov dword [herr], H_SELEC
        call estado_txt
        call inv_todo
        popad
.x:
        ret

; selecciona todo el lienzo
todo:
        call sel_quitar
        xor eax, eax
        mov [sel_x], eax
        mov [sel_y], eax
        mov eax, [lz_an]
        mov [sel_an], eax
        mov eax, [lz_al]
        mov [sel_al], eax
        mov dword [sel_hay], 1
        mov dword [herr], H_SELEC
        call estado_txt
        jmp inv_todo

; ---- la ventana de todos los colores ----
abrir_colores:
        mov ebp, [col_ven]
        test ebp, ebp
        jz .crea
        HV ven_activar
        ret
.crea:
        mov esi, col_plant
        xor ebp, ebp
        HV ven_crear
        mov [col_ven], eax
        ret

col_proc:
        cmp eax, WM_PAINT
        je .pinta
        cmp eax, WM_LBUTTONDOWN
        je .izq
        cmp eax, WM_RBUTTONDOWN
        je .der
        cmp eax, WM_DESTROY
        je .dest
        HVJ ven_defecto
.dest:
        mov dword [col_ven], 0
        jmp hecho
.izq:
        xor esi, esi
        jmp .elige
.der:
        mov esi, 1
.elige:
        movsx eax, dx
        sar edx, 16
        sub eax, 4
        jl hecho
        sub edx, 4
        jl hecho
        mov ebx, edx
        xor edx, edx
        mov ecx, 10
        div ecx
        xchg eax, ebx               ; EBX=columna
        xor edx, edx
        mov ecx, 16
        div ecx                     ; EAX=fila
        mov ecx, 16                 ; fila 0: los 16 propios
        test eax, eax
        jz .e1
        mov ecx, 24                 ; fila 7: los 24 grises
        mov edx, 232
        cmp eax, 7
        je .e2
        ja hecho
        mov ecx, 36                 ; filas 1 a 6: el cubo, una por nivel de rojo
        dec eax
        imul edx, eax, 36
        add edx, 16
        jmp .e2
.e1:
        xor edx, edx
.e2:
        cmp ebx, ecx
        jae hecho
        lea eax, [edx+ebx]
        call poner_color
        jmp hecho
.pinta:
        HV dc_pintar
        xor edi, edi
.p:
        mov eax, edi                ; el color EDI: su fila y su columna
        xor ebx, ebx
        cmp edi, 16
        jb .p1
        mov ebx, 7
        sub eax, 232
        jae .p1
        lea eax, [edi-16]
        xor edx, edx
        mov ecx, 36
        div ecx
        lea ebx, [eax+1]
        mov eax, edx
.p1:
        imul eax, 10
        add eax, 4
        shl ebx, 4
        add ebx, 4
        mov ecx, 9
        mov edx, 15
        COLOR edi
        HV dc_rect
        inc edi
        cmp edi, 256
        jb .p
        HV dc_cerrar
        jmp hecho

; ---- archivos ----
; si hay cambios sin guardar, pregunta. CF=1 si no hay que seguir
preguntar:
        cmp byte [modif], 0
        je .si
        push eax
        push ebp
        push esi
        push edi
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_pintura
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        pop edi
        pop esi
        pop ebp
        cmp eax, ID_NO
        je .si0
        cmp eax, ID_SI
        jne .no
        call guardar
        jc .no
.si0:
        pop eax
.si:
        clc
        ret
.no:
        pop eax
        stc
        ret

; guarda con el nombre que tiene (o lo pide). CF=1 si no se guarda
guardar:
        cmp byte [nombre], 0
        jne escribir
guardar_como:
        pushad
        mov edi, nom_tmp
        mov esi, nombre
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .no
        mov esi, nom_tmp            ; sin extensión: .BMP
        mov edi, nombre
        xor edx, edx
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '.'
        jne .c1
        inc edx
.c1:
        cmp al, '\'
        jne .c2
        xor edx, edx
.c2:
        stosb
        jmp .c
.f:
        test edx, edx
        jnz .f2
        mov esi, t_ext_bmp
        call hvl_cad
.f2:
        mov byte [edi], 0
        popad
        jmp escribir
.no:
        popad
        stc
        ret

; escribe el lienzo en «nombre»: BMP de 8 bits con la paleta de Hispa
; Ventanas, las filas de abajo arriba. CF=1 si falla
escribir:
        pushad
        call sel_quitar
        mov esi, nombre
        HV dos_crear
        jc .falla0
        mov ebx, eax
        mov edi, cab
        xor eax, eax
        mov ecx, 14
        rep stosd
        mov word [cab], 'BM'
        mov eax, [lz_an]
        add eax, 3
        and eax, ~3
        mov [c_fila], eax
        imul eax, [lz_al]
        mov [cab+34], eax           ; octetos de la imagen
        add eax, 1078
        mov [cab+2], eax            ; y del archivo
        mov dword [cab+10], 1078    ; dónde empieza la imagen
        mov dword [cab+14], 40
        mov eax, [lz_an]
        mov [cab+18], eax
        mov eax, [lz_al]
        mov [cab+22], eax
        mov word [cab+26], 1
        mov word [cab+28], 8
        mov dword [cab+38], 2835    ; 72 puntos por pulgada
        mov dword [cab+42], 2835
        mov esi, cab
        mov ecx, 54
        HV dos_escribir
        jc .falla
        mov esi, [hv_api + HV_pal_rgb * 4]  ; la paleta: azul, verde, rojo, 0
        mov edi, fila
        mov ecx, 256
.p:
        mov eax, [esi]
        add esi, 4
        mov edx, eax
        shr edx, 16
        mov [edi], dl
        mov [edi+1], ah
        mov [edi+2], al
        mov byte [edi+3], 0
        add edi, 4
        dec ecx
        jnz .p
        mov esi, fila
        mov ecx, 1024
        HV dos_escribir
        jc .falla
        cmp eax, ecx
        jb .falla
        mov edx, [lz_al]
.f:
        dec edx
        js .fin
        mov esi, edx
        imul esi, [lz_an]
        add esi, [lz]
        mov edi, fila
        mov ecx, [lz_an]
        rep movsb
        xor eax, eax
        stosd
        mov esi, fila
        mov ecx, [c_fila]
        HV dos_escribir
        jc .falla
        cmp eax, ecx                ; (disco lleno)
        jb .falla
        jmp .f
.fin:
        HV dos_cerrar
        mov byte [modif], 0
        call pon_titulo
        popad
        clc
        ret
.falla:
        HV dos_cerrar
.falla0:
        mov esi, t_no_guardar
        call mensaje
        popad
        stc
        ret

; EAX=color (rojo en el octeto bajo, verde, azul) -> EAX=el más parecido de
; los 256: uno de los 16 propios si es igual; si no, del cubo o de los grises
rgb_a_ind:
        cmp eax, [rgb_ult]
        jne .b
        mov eax, [rgb_ult_i]
        ret
.b:
        push ebx
        push ecx
        push edx
        push esi
        mov [rgb_ult], eax
        mov esi, [hv_api + HV_pal_rgb * 4]
        xor ecx, ecx
.p:
        cmp eax, [esi+ecx*4]
        je .hecho
        inc ecx
        cmp ecx, 16
        jb .p
        movzx ebx, al               ; rojo
        movzx ecx, ah               ; verde
        mov edx, eax
        shr edx, 16                 ; azul
        cmp ebx, ecx
        jne .cubo
        cmp ebx, edx
        jne .cubo
        lea eax, [ebx+5]            ; un gris: el de la escala (de 11 en 11)...
        xor edx, edx
        mov ecx, 11
        div ecx
        cmp eax, 23
        jbe .g1
        mov eax, 23
.g1:
        imul ecx, eax, 11
        sub ecx, ebx
        jns .g2
        neg ecx
.g2:
        push eax
        lea eax, [ebx+25]           ; ...o el del cubo (de 51 en 51)
        xor edx, edx
        mov esi, 51
        div esi
        imul edx, eax, 51
        sub edx, ebx
        jns .g3
        neg edx
.g3:
        cmp edx, ecx
        jae .rampa
        add esp, 4
        imul ecx, eax, 43
        add ecx, 16
        jmp .hecho
.rampa:
        pop ecx
        add ecx, 232
        jmp .hecho
.cubo:
        push edx
        mov esi, 51
        lea eax, [ebx+25]
        xor edx, edx
        div esi
        imul ebx, eax, 36
        lea eax, [ecx+25]
        xor edx, edx
        div esi
        imul eax, eax, 6
        add ebx, eax
        pop eax
        add eax, 25
        xor edx, edx
        div esi
        lea ecx, [ebx+eax+16]
.hecho:
        mov eax, ecx
        mov [rgb_ult_i], eax
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ESI=nombre: abre un BMP en un lienzo nuevo de su tamaño
cargar:
        pushad
        mov [c_nom], esi
        xor al, al
        HV dos_abrir
        jc .noabre
        mov [c_arch], eax
        mov ebx, eax
        mov edi, cab
        mov ecx, 54
        HV dos_leer
        jc .malo
        cmp eax, 54
        jne .malo
        cmp word [cab], 'BM'
        jne .malo
        cmp dword [cab+14], 40
        jb .malo
        cmp dword [cab+30], 0       ; sin compresión
        jne .malo
        movzx eax, word [cab+28]
        mov [c_bpp], eax
        cmp eax, 1
        je .bien
        cmp eax, 4
        je .bien
        cmp eax, 8
        je .bien
        cmp eax, 24
        jne .malo
.bien:
        mov eax, [cab+18]
        mov edx, [cab+22]
        mov byte [c_arriba], 0
        test edx, edx
        jns .h
        neg edx                     ; alto negativo: la primera fila, arriba
        mov byte [c_arriba], 1
.h:
        test eax, eax
        jle .malo
        test edx, edx
        jle .malo
        cmp eax, MAXAN
        ja .grande
        cmp edx, MAXAL
        ja .grande
        mov [c_an], eax
        mov [c_al], edx
        mov dword [rgb_ult], -1
        cmp dword [c_bpp], 8
        ja .sinpal
        mov ecx, [c_bpp]            ; la paleta: cada color, al más parecido
        mov eax, 1
        shl eax, cl
        mov ecx, [cab+46]
        test ecx, ecx
        jz .p1
        cmp ecx, eax
        jbe .p2
.p1:
        mov ecx, eax
.p2:
        mov [c_ncol], ecx
        mov edx, [cab+14]
        add edx, 14
        xor al, al
        HV dos_puntero
        mov edi, fila
        shl ecx, 2
        HV dos_leer
        jc .malo
        cmp eax, ecx
        jne .malo
        xor ecx, ecx
.m:
        xor eax, eax
        cmp ecx, [c_ncol]
        jae .m1
        movzx eax, byte [fila+ecx*4+2]
        mov ah, [fila+ecx*4+1]
        movzx edx, byte [fila+ecx*4]
        shl edx, 16
        or eax, edx
        call rgb_a_ind
.m1:
        mov [mapa+ecx], al
        inc ecx
        cmp ecx, 256
        jb .m
.sinpal:
        mov ebx, [c_arch]
        mov edx, [cab+10]
        xor al, al
        HV dos_puntero
        mov eax, [c_an]
        mov ebx, [c_al]
        call lienzo_nuevo
        jc .sinmem
        mov eax, [c_an]             ; octetos de una fila del archivo
        imul eax, [c_bpp]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [c_fila], eax
        xor edx, edx
.fila:
        cmp edx, [c_al]
        jae .fin
        mov ebx, [c_arch]
        mov edi, fila
        mov ecx, [c_fila]
        HV dos_leer
        jc .corto
        cmp eax, ecx
        jb .corto
        mov eax, edx
        cmp byte [c_arriba], 0
        jne .y
        mov eax, [c_al]
        dec eax
        sub eax, edx
.y:
        imul eax, [lz_an]
        add eax, [lz]
        mov edi, eax
        mov esi, fila
        mov ecx, [c_an]
        push edx
        mov eax, [c_bpp]
        cmp eax, 8
        je .b8
        cmp eax, 24
        je .b24
        cmp eax, 4
        je .b4
        xor edx, edx                ; 1 bit: el de más peso, a la izquierda
.b1:
        test edx, 7
        jnz .b1n
        mov ah, [esi]
        inc esi
.b1n:
        xor ebx, ebx
        shl ah, 1
        adc ebx, 0
        mov al, [mapa+ebx]
        stosb
        inc edx
        dec ecx
        jnz .b1
        jmp .sigf
.b4:
        xor edx, edx                ; 4 bits: primero los cuatro altos
.b4l:
        test edx, 1
        jnz .b4n
        mov ah, [esi]
        inc esi
        movzx ebx, ah
        shr ebx, 4
        jmp .b4p
.b4n:
        movzx ebx, ah
        and ebx, 15
.b4p:
        mov al, [mapa+ebx]
        stosb
        inc edx
        dec ecx
        jnz .b4l
        jmp .sigf
.b8:
        movzx ebx, byte [esi]
        inc esi
        mov al, [mapa+ebx]
        stosb
        dec ecx
        jnz .b8
        jmp .sigf
.b24:
        movzx eax, byte [esi+2]     ; azul, verde, rojo
        mov ah, [esi+1]
        movzx edx, byte [esi]
        shl edx, 16
        or eax, edx
        add esi, 3
        call rgb_a_ind
        stosb
        dec ecx
        jnz .b24
.sigf:
        pop edx
        inc edx
        jmp .fila
.corto:
        mov ebx, [c_arch]
        HV dos_cerrar
        mov byte [nombre], 0
        mov esi, t_corto
        call mensaje
        jmp .listo
.fin:
        mov ebx, [c_arch]
        HV dos_cerrar
        mov esi, [c_nom]
        mov edi, nombre
        mov ecx, 79
.n:
        lodsb
        stosb
        test al, al
        jz .listo
        dec ecx
        jnz .n
        mov byte [edi], 0
.listo:
        mov byte [modif], 0
        call pon_titulo
        call medidas
        call estado_txt
        call inv_todo
        popad
        ret
.grande:
        mov esi, t_grande
        jmp .cierra
.sinmem:
        mov esi, t_sin_mem
        jmp .cierra
.malo:
        mov esi, t_no_bmp
.cierra:
        mov ebx, [c_arch]
        HV dos_cerrar
        jmp .aviso
.noabre:
        mov esi, t_no_abrir
.aviso:
        call mensaje
        popad
        ret

%include "hvplib.inc"

        section .bss
            alignb 4
ven         resd 1                  ; la ventana
col_ven     resd 1                  ; la de todos los colores
param       resd 1
cli_an      resd 1                  ; la zona de la ventana
cli_al      resd 1
vis_an      resd 1                  ; lo que se ve del lienzo
vis_al      resd 1
max_dx      resd 1
max_dy      resd 1
des_x       resd 1                  ; el desplazamiento
des_y       resd 1
pan_x       resd 1
tir_rec     resd 1
lz          resd 1                  ; el lienzo
lz_mango    resd 1
lz_an       resd 1
lz_al       resd 1
lz_tam      resd 1
flo         resd 1                  ; la zona flotante
des_buf     resd 1                  ; los pasos de deshacer
des_n       resd 1                  ;   cuántos caben
des_cab     resd 1                  ;   dónde va el siguiente
des_cuenta  resd 1                  ;   cuántos se pueden deshacer
des_re      resd 1                  ;   y rehacer
herr        resd 1                  ; la herramienta
gros        resd 1
forma       resd 1
col1        resd 1                  ; el color primario
col2        resd 1                  ; y el secundario (detrás de col1)
col_a       resd 1                  ; el del botón con el que se dibuja
col_b       resd 1                  ; y el otro
arr         resd 1                  ; qué se hace con el botón apretado
arr_bot     resd 1
arr_off     resd 1
ax0         resd 1                  ; dónde se apretó (en el lienzo)
ay0         resd 1
ux          resd 1                  ; el último punto
uy          resd 1
rx          resd 1                  ; el ratón, en la ventana
ry          resd 1
est_x       resd 1                  ; el punto de la barra de estado
est_y       resd 1
sel_hay     resd 1                  ; 0 no, 1 un recuadro, 2 flota
sel_x       resd 1
sel_y       resd 1
sel_an      resd 1
sel_al      resd 1
mov_dx      resd 1
mov_dy      resd 1
pp          resd 1                  ; el portapapeles de Pintura
pp_mango    resd 1
pp_an       resd 1
pp_al       resd 1
d_x0        resd 1                  ; lo dibujado
d_y0        resd 1
d_x1        resd 1
d_y1        resd 1
p_x0        resd 1                  ; la forma de antes
p_y0        resd 1
p_x1        resd 1
p_y1        resd 1
r_x0        resd 1                  ; la caja de la forma (seguidos)
r_y0        resd 1
r_x1        resd 1
r_y1        resd 1
i_x0        resd 1                  ; y la de dentro (seguidos)
i_y0        resd 1
i_x1        resd 1
i_y1        resd 1
t_l         resd 1
t_r         resd 1
s_d         resd 1                  ; el sello
l_sx        resd 1
l_sy        resd 1
r_pila      resd 1
r_tope      resd 1
t_esc       resd 1
t_bits      resd 1
t_col       resd 1
rgb_ult     resd 1
rgb_ult_i   resd 1
c_nom       resd 1                  ; abrir y guardar
c_arch      resd 1
c_bpp       resd 1
c_an        resd 1
c_al        resd 1
c_ncol      resd 1
c_fila      resd 1
cab         resb 56
tinta       resb 1
s_cuad      resb 1
hay_int     resb 1
hay_bh      resb 1
hay_bv      resb 1
modif       resb 1
r_viejo     resb 1
r_nuevo     resb 1
c_arriba    resb 1
nombre      resb 80
nom_tmp     resb 80
titulo      resb 100
est_txt     resb 64
txt         resb 64
num         resb 16
mapa        resb 256
fila        resb 3084
HVP_FIN
