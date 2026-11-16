; =============================================================================
;  Cuentas - los gráficos: barras, barras apiladas, líneas, sectores y
;  dispersión (X-Y)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La hoja tiene UN gráfico, que se guarda con ella: su tipo, el rango del
;  que sale, si las series van por columnas o por filas, su título y los
;  rótulos de sus ejes. Se ve en una ventana aparte, con su menú (el tipo,
;  las series, los títulos, imprimirlo y guardarlo como imagen .BMP).
;
;  De dónde salen los datos (con las series POR COLUMNAS, lo normal):
;    - cada columna del rango es una SERIE y cada fila, un PUNTO;
;    - si la primera columna no tiene ningún número, son los RÓTULOS de los
;      puntos (si no, se rotulan con el número de su fila);
;    - si la primera fila no tiene ningún número, son los NOMBRES de las
;      series (si no, «Serie 1», «Serie 2»...).
;  Por filas es lo mismo, cambiando filas por columnas (lo elegido de una
;  sola fila se toma así). Como mucho, GR_SER series de GR_MAX puntos. En el
;  de sectores cuenta solo la primera serie; en el de dispersión, la primera
;  serie son las X y las demás, las Y.
;
;  La ventana guarda el rango, no los números: cada vez que se pinta los
;  vuelve a leer, así que el gráfico sigue a la hoja cuando cambia.
;
;  El gráfico se dibuja siempre en el LIENZO (un mapa de 576 puntos de ancho,
;  un octeto por punto, de imprime.inc) con las rutinas ib_*, y de ahí va a
;  la ventana, a la impresora (como una página de «Imprimir como se ve») o a
;  un archivo .BMP de 256 colores. Los rellenos (barras, sectores) llevan la
;  marca IB_RAYADO: en el papel cada serie sale con un rayado distinto.
;
;  Sin coma flotante. La escala: de la menor a la mayor de las cantidades se
;  busca un PASO «redondo» (1, 2 o 5 por una potencia de diez) que dé seis
;  divisiones como mucho; la escala va del múltiplo del paso que queda por
;  debajo de la menor al que queda por encima de la mayor. Para pasar una
;  cantidad a puntos, a ella y al tramo de la escala se les quitan por abajo
;  los bits que sobran para que el tramo quepa en 20 (gr_pos).
;  Los sectores se pintan punto a punto: de cada punto del círculo se saca
;  su ángulo (en 1.024 partes de vuelta, con una tabla de arcotangentes de un
;  octavo de vuelta) y se mira en qué sector cae.
; =============================================================================
GR_SER      equ 8                   ; series, como mucho
GR_MAX      equ 24                  ; puntos de cada serie, como mucho
GR_DIV      equ 6                   ; divisiones de la escala, como mucho
GT_BARRAS   equ 0                   ; los tipos
GT_LINEAS   equ 1
GT_SECTORES equ 2
GT_APILADAS equ 3
GT_XY       equ 4
; una escala
E_LO        equ 0                   ; dq donde empieza
E_PASO      equ 8                   ; dq lo que hay de una raya a la siguiente
E_N         equ 16                  ; dd cuántas divisiones
E_R         equ 20                  ; dd lo que mide (sin los bits que sobran)
E_K         equ 24                  ; dd y cuántos bits se le han quitado
E_TAM       equ 28
; las órdenes del menú de la ventana del gráfico
O_G_TIPO    equ 220                 ; 220-224: el tipo (GT_*)
O_G_COLS    equ 230
O_G_FILAS   equ 231
O_G_TITULOS equ 232
O_G_IMPRIMIR equ 233
O_G_BMP     equ 234
O_G_CERRAR  equ 235

c_grafico   dd gr_proc
            db C_BLANCO, 0, 0, 0
gr_plant:
        PLANTILLA c_grafico, t_gr_barras, E_NORMAL, 130, 100, 500, 384, 0
gr_menu:
        db "&Archivo", 0
        dw O_G_IMPRIMIR
        db "&Imprimir", 0
        dw O_G_BMP
        db "&Guardar como imagen (.BMP)...", 0
        dw 0
        db 0
        dw O_G_CERRAR
        db "&Cerrar", 0
        dw 0xFFFF
        db "&Gráfico", 0
        dw O_G_TIPO + GT_BARRAS
        db "&Barras", 0
        dw O_G_TIPO + GT_APILADAS
        db "Barras a&piladas", 0
        dw O_G_TIPO + GT_LINEAS
        db "&Líneas", 0
        dw O_G_TIPO + GT_SECTORES
        db "&Sectores", 0
        dw O_G_TIPO + GT_XY
        db "&Dispersión (X-Y)", 0
        dw 0
        db 0
        dw O_G_COLS
        db "Series por &columnas", 0
        dw O_G_FILAS
        db "Series por &filas", 0
        dw 0
        db 0
        dw O_G_TITULOS
        db "&Título y rótulos de los ejes...", 0
        dw 0xFFFF
        db 0
; ---- diálogo: el título y los rótulos de los ejes ----
dlg_gr:
        PLANTILLA HV_c_dialogo, t_d_gr, E_DIALOGO, 0, 0, 420, 196, 0
        PLANTILLA HV_c_estatico, t_g_tit, E_HIJA, 20, 18, 140, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 164, 14, 236, 24, 50
        PLANTILLA HV_c_estatico, t_g_ejex, E_HIJA, 20, 50, 140, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 164, 46, 236, 24, 51
        PLANTILLA HV_c_estatico, t_g_ejey, E_HIJA, 20, 82, 140, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 164, 78, 236, 24, 52
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 115, 122, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 215, 122, 90, 26, ID_CANCELAR
        dd 0
t_d_gr      db "Título y rótulos del gráfico", 0
t_g_tit     db "Título:", 0
t_g_ejex    db "Eje horizontal:", 0
t_g_ejey    db "Eje vertical:", 0
t_gr_barras db "Gráfico de barras", 0
t_gr_lineas db "Gráfico de líneas", 0
t_gr_sectores db "Gráfico de sectores", 0
t_gr_apiladas db "Gráfico de barras apiladas", 0
t_gr_xy     db "Gráfico de dispersión", 0
t_gr_nada   db "Para hacer un gráfico, elija antes las celdas con las cantidades", 10
            db "(y, si quiere, la columna de los nombres, a su izquierda, y la", 10
            db "fila de los títulos, encima).", 0
t_gr_no_hay db "Esta hoja no tiene gráfico. Para hacer uno, elija las celdas", 10
            db "con las cantidades y use Insertar > Gráfico.", 0
t_gr_sin    db "No hay cantidades que dibujar.", 0
t_gr_xy_falta db "Hacen falta dos series: las X y las Y.", 0
t_gr_serie  db "Serie ", 0
t_gr_bmp    db "Guardar el gráfico como imagen", 0
t_gr_bmp_nom db "GRAFICO.BMP", 0
t_gr_bmp_ext db ".BMP", 0
t_gr_guardado db "El gráfico se ha guardado.", 0
            align 4
gr_titulos  dd t_gr_barras, t_gr_lineas, t_gr_sectores, t_gr_apiladas, t_gr_xy
gr_formas   dd gr_barras, gr_lineas, 0, gr_barras, gr_xy
gr_colores  db C_PETROLEO, C_TERRACOTA, C_OCRE, C_OLIVA, C_ANIL, C_GRANATE, C_TURQUESA, C_MARRON
gr_mult     db 1, 2, 5
; el ángulo cuya tangente es n/64, en 1.024 partes de vuelta (de 0 a 45 grados)
gr_atan     db 0, 3, 5, 8, 10, 13, 15, 18, 20, 23, 25, 28, 30
            db 33, 35, 38, 40, 42, 45, 47, 49, 52, 54, 56, 58, 61
            db 63, 65, 67, 69, 71, 74, 76, 78, 80, 82, 84, 85, 87
            db 89, 91, 93, 95, 96, 98, 100, 102, 103, 105, 106, 108, 110
            db 111, 113, 114, 116, 117, 119, 120, 121, 123, 124, 125, 127, 128
; la cabecera de un .BMP de 576 x 400 puntos y 256 colores
            align 4
gr_bmp_cab  db "BM"
            dd 54 + 1024 + IB_AN * LZ_AL, 0, 54 + 1024
            dd 40, IB_AN, LZ_AL
            dw 1, 8
            dd 0, IB_AN * LZ_AL, 2835, 2835, 256, 0
GR_BMP_CAB  equ $ - gr_bmp_cab

; =============================================================================
;  Las órdenes de Cuentas
; =============================================================================
; el botón de la barra: con una sola celda elegida, enseña el gráfico que ya
; tiene la hoja (si lo tiene); si no, hace uno de barras de lo elegido
grafico_boton:
        call sel_una
        jne .nuevo
        cmp byte [gr_hay], 0
        jne gr_muestra
.nuevo:
        xor eax, eax
; Insertar > Gráfico de...: EAX=el tipo (GT_*). De lo elegido; si lo elegido
; es una sola celda y la hoja ya tiene gráfico, se le cambia el tipo
grafico:
        pushad
        mov ebp, eax
        call sel_rect
        call sel_una
        jne .nuevo
        cmp byte [gr_hay], 0
        je .nada
        mov [gr_tipo], ebp
        jmp .ver
.nuevo:
        call rect_recorta
        mov esi, gr_c1              ; (por si no vale: el de antes)
        mov edi, gr_ant
        mov ecx, 4
        rep movsd
        mov al, [gr_filas]
        mov [gr_ant+16], al
        mov esi, r_c1
        mov edi, gr_c1
        mov ecx, 4
        rep movsd
        mov eax, [gr_f1]            ; una sola fila: las series, por filas
        cmp eax, [gr_f2]
        sete [gr_filas]
        call gr_lee
        cmp dword [gr_n], 0
        jne .vale
        mov esi, gr_ant
        mov edi, gr_c1
        mov ecx, 4
        rep movsd
        mov al, [gr_ant+16]
        mov [gr_filas], al
.nada:
        popad
        mov esi, t_gr_nada
        jmp aviso
.vale:
        mov [gr_tipo], ebp
        xor eax, eax                ; un gráfico nuevo: sin títulos
        mov [gr_tit], al
        mov [gr_ejex], al
        mov [gr_ejey], al
        mov byte [gr_hay], 1
.ver:
        mov byte [modif], 1
        popad
; enseña la ventana del gráfico (Ver > Gráfico)
gr_muestra:
        cmp byte [gr_hay], 0
        jne .hay
        mov esi, t_gr_no_hay
        jmp aviso
.hay:
        pushad
        mov eax, [gr_tipo]
        mov esi, [gr_titulos+eax*4]
        mov [gr_plant+P_TEXTO], esi
        mov ebp, [gr_ven]
        test ebp, ebp
        jz .nueva
        HV ven_texto                ; ya hay una ventana: la misma, con lo nuevo
        HV ven_invalidar
        HV ven_activar
        popad
        ret
.nueva:
        mov esi, gr_plant
        xor ebp, ebp
        HV ven_crear
        popad
        ret

; la hoja se queda sin gráfico (una hoja nueva, o su rango se ha eliminado)
gr_olvida:
        pushad
        xor eax, eax
        mov [gr_hay], al
        mov [gr_filas], al
        mov [gr_tit], al
        mov [gr_ejex], al
        mov [gr_ejey], al
        mov [gr_tipo], eax
        mov ebp, [gr_ven]
        test ebp, ebp
        jz .x
        HV ven_destruir
.x:
        popad
        ret

; la ventana del gráfico, a repintar (ha cambiado la hoja)
gr_inval:
        push ebp
        mov ebp, [gr_ven]
        test ebp, ebp
        jz .x
        pushad
        HV ven_invalidar
        popad
.x:
        pop ebp
        ret

; se han insertado o eliminado filas o columnas ([aj_modo], [aj_pos],
; [aj_n]): el rango del gráfico se corrige como el de una fórmula; si se ha
; eliminado entero, la hoja se queda sin gráfico
gr_ajusta:
        cmp byte [gr_hay], 0
        je .x
        pushad
        call gr_rango_txt
        mov esi, gr_txt
        mov edi, aj_buf
        call ajusta_formula
        mov esi, aj_buf
        call gr_rango_lee
        popad
        jnc .x
        call gr_olvida
.x:
        ret
; (interna) el rango, como texto («A2:C5»), en gr_txt
gr_rango_txt:
        push ecx
        push edx
        push edi
        mov edi, gr_txt
        mov ecx, [gr_c1]
        mov edx, [gr_f1]
        call ref_nombre
        mov al, ':'
        stosb
        mov ecx, [gr_c2]
        mov edx, [gr_f2]
        call ref_nombre
        mov byte [edi], 0
        pop edi
        pop edx
        pop ecx
        ret
; (interna) ESI=«A2:C5» -> el rango del gráfico (ESI queda detrás); CF=1 si
; no es un rango de la hoja
gr_rango_lee:
        call lee_ref
        jc .no
        cmp ecx, NCOL
        jae .no
        cmp edx, NFIL
        jae .no
        cmp byte [esi], ':'
        jne .no
        inc esi
        push ecx
        push edx
        call lee_ref
        mov eax, ecx
        mov ebx, edx
        pop edx
        pop ecx
        jc .no
        cmp eax, NCOL
        jae .no
        cmp ebx, NFIL
        jae .no
        cmp eax, ecx
        jb .no
        cmp ebx, edx
        jb .no
        mov [gr_c1], ecx
        mov [gr_f1], edx
        mov [gr_c2], eax
        mov [gr_f2], ebx
        clc
        ret
.no:
        stc
        ret

; ---- en el archivo .HOJ: «G tipo rango filas», «T título», «X rótulo» e
;      «Y rótulo» ----
gr_guarda:
        cmp byte [gr_hay], 0
        je .x
        pushad
        mov al, 'G'
        call esc_car
        mov al, ' '
        call esc_car
        mov eax, [gr_tipo]
        call esc_num
        mov al, ' '
        call esc_car
        call gr_rango_txt
        mov esi, gr_txt
        call esc_cad
        mov al, ' '
        call esc_car
        movzx eax, byte [gr_filas]
        call esc_num
        call esc_nl
        mov al, 'T'
        mov esi, gr_tit
        call .linea
        mov al, 'X'
        mov esi, gr_ejex
        call .linea
        mov al, 'Y'
        mov esi, gr_ejey
        call .linea
        popad
.x:
        ret
.linea:
        cmp byte [esi], 0
        je .lx
        call esc_car
        mov al, ' '
        call esc_car
        call esc_cad
        call esc_nl
.lx:
        ret
; ESI=una línea del .HOJ que empieza por una letra y un espacio: si es del
; gráfico, la lee. Puede cambiar todos los registros
gr_hoj_linea:
        mov al, [esi]
        add esi, 2
        mov edi, gr_tit
        mov ecx, 40
        cmp al, 'T'
        je .txt
        mov ecx, 24
        mov edi, gr_ejex
        cmp al, 'X'
        je .txt
        mov edi, gr_ejey
        cmp al, 'Y'
        je .txt
        cmp al, 'G'
        jne .x
        call hvl_lee_num
        cmp eax, GT_XY
        ja .x
        mov ebp, eax
.sp:
        cmp byte [esi], ' '
        jne .r
        inc esi
        jmp .sp
.r:
        call gr_rango_lee
        jc .x
        call hvl_lee_num
        and al, 1
        mov [gr_filas], al
        mov [gr_tipo], ebp
        mov byte [gr_hay], 1
.x:
        ret
.txt:
        mov al, [esi]
        cmp al, 32
        jb .tf
        inc esi
        jecxz .txt
        stosb
        dec ecx
        jmp .txt
.tf:
        mov byte [edi], 0
        ret

; =============================================================================
;  La ventana del gráfico
; =============================================================================
gr_proc:
        cmp eax, WM_PAINT
        je gr_pintar
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [gr_ven], ebp
        mov dword [ebp+V_ICONO], hvp_icono
        mov dword [ebp+V_MENU], gr_menu
        jmp .x
.tam:
        HV ven_invalidar
        jmp .x
.destruir:
        mov dword [gr_ven], 0
        jmp .x
.orden:
        mov [aviso_ven], ebp        ; (lo que haya que decir, sobre esta ventana)
        call .haz
        mov dword [aviso_ven], 0
        xor eax, eax
        ret
.haz:
        mov eax, ecx
        sub eax, O_G_TIPO
        cmp eax, GT_XY
        ja .o1
        mov [gr_tipo], eax
        mov byte [modif], 1
        call gr_muestra             ; (el título de la ventana, y a repintar)
        jmp .x
.o1:
        cmp ecx, O_G_COLS
        je .series
        cmp ecx, O_G_FILAS
        jne .o2
.series:
        sub ecx, O_G_COLS
        mov [gr_filas], cl
        mov byte [modif], 1
        HV ven_invalidar
        jmp .x
.o2:
        cmp ecx, O_G_TITULOS
        jne .o3
        call gr_dlg_titulos
        jmp .x
.o3:
        cmp ecx, O_G_IMPRIMIR
        jne .o4
        call gr_imprime
        jmp .x
.o4:
        cmp ecx, O_G_BMP
        jne .o5
        call gr_bmp
        jmp .x
.o5:
        cmp ecx, O_G_CERRAR
        jne .x
        HV ven_destruir
.x:
        xor eax, eax
        ret

gr_pintar:
        mov ebp, [gr_ven]
        HV dc_pintar
        HV ven_cli
        cmp ecx, IB_AN
        jbe .a
        mov ecx, IB_AN
.a:
        cmp edx, LZ_AL
        jbe .b
        mov edx, LZ_AL
.b:
        cmp ecx, 8
        jl .fin
        cmp edx, 8
        jl .fin
        call gr_dibuja
        ; del lienzo (576 de ancho, con sus marcas) a un mapa del ancho de
        ; la ventana, en el mismo sitio (cada fila queda antes de donde estaba)
        push ecx
        push edx
        mov esi, lienzo
        mov edi, lienzo
        mov ebx, edx
.f:
        push ecx
        push esi
.p:
        lodsb
        and al, 15
        stosb
        dec ecx
        jnz .p
        pop esi
        pop ecx
        add esi, IB_AN
        dec ebx
        jnz .f
        pop edx
        pop ecx
        xor eax, eax
        xor ebx, ebx
        mov esi, lienzo
        HV dc_copiar
.fin:
        HV dc_cerrar
        xor eax, eax
        ret

; el diálogo del título y los rótulos de los ejes
gr_dlg_titulos:
        pushad
        mov ebp, [gr_ven]
        mov esi, dlg_gr
        mov edi, dlg_gr + P_TAM
        mov ebx, .gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov byte [modif], 1
        call gr_inval
.x:
        popad
        ret
.gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov eax, WM_GETTEXT
        mov ecx, 50
        mov ebx, 41
        mov edx, gr_tit
        call hija_msg
        mov eax, WM_GETTEXT
        mov ecx, 51
        mov ebx, 25
        mov edx, gr_ejex
        call hija_msg
        mov eax, WM_GETTEXT
        mov ecx, 52
        mov edx, gr_ejey
        call hija_msg
        popad
        jmp .no
.ini:
        pushad
        mov eax, WM_SETTEXT
        mov ecx, 50
        mov edx, gr_tit
        call hija_msg
        mov eax, WM_SETTEXT
        mov ecx, 51
        mov edx, gr_ejex
        call hija_msg
        mov eax, WM_SETTEXT
        mov ecx, 52
        mov edx, gr_ejey
        call hija_msg
        mov eax, EM_SEL_TODO        ; (lo que se teclee cambia el título)
        mov ecx, 50
        call hija_msg
        popad
.no:
        xor eax, eax
        ret

; ---- imprimir el gráfico: una página, con la cabecera de las de la hoja ----
gr_imprime:
        pushad
        call imp_hay
        jc .mal
        mov esi, ig_ini
        mov ecx, 5
        call imp_manda
        jc .mal
        mov dword [ip_n], 1
        mov dword [ipb_pag], 0
        call ig_empieza
        mov dword [ib_base], ib_buf
        call ipg_cabecera
        call ig_come
        jc .mal
        mov ecx, IB_AN
        mov edx, LZ_AL
        call gr_dibuja              ; (deja el lienzo como el trozo que toca)
        call ig_come
        jc .mal
        call ig_acaba
        jc .mal
        call imp_lista
        jc .mal
        popad
        jmp imp_dicho
.mal:
        popad
        jmp imp_mal

; ---- guardar el gráfico como imagen: un .BMP de 576 x 400 y 256 colores
;      (los de la pantalla) ----
gr_bmp:
        pushad
        mov esi, nombre             ; el nombre que se propone: el de la hoja,
        mov edi, nom_tmp            ; acabado en .BMP
        cmp byte [esi], 0
        jne .n0
        mov esi, t_gr_bmp_nom
        call hvl_cad
        jmp .n3
.n0:
        xor edx, edx                ; EDX=dónde está su último punto
.n1:
        lodsb
        test al, al
        jz .n2
        cmp al, '.'
        jne .n11
        mov edx, edi
.n11:
        cmp al, '\'
        jne .n12
        xor edx, edx
.n12:
        stosb
        jmp .n1
.n2:
        test edx, edx
        jz .n21
        mov edi, edx
.n21:
        mov esi, t_gr_bmp_ext
        call hvl_cad
.n3:
        mov byte [edi], 0
        mov ebp, [gr_ven]
        mov ebx, nom_tmp
        mov edi, t_gr_bmp
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, nom_tmp            ; sin extensión: .BMP
        xor edx, edx
.e1:
        lodsb
        test al, al
        jz .e2
        cmp al, '.'
        jne .e11
        inc edx
.e11:
        cmp al, '\'
        jne .e1
        xor edx, edx
        jmp .e1
.e2:
        test edx, edx
        jnz .e3
        lea edi, [esi-1]
        mov esi, t_gr_bmp_ext
        call hvl_cad
        mov byte [edi], 0
.e3:
        mov ecx, IB_AN
        mov edx, LZ_AL
        call gr_dibuja
        mov esi, nom_tmp
        call esc_abre
        jc .falla
        mov esi, gr_bmp_cab
        mov ecx, GR_BMP_CAB
.c:
        lodsb
        call esc_car
        dec ecx
        jnz .c
        mov esi, [hv_api+HV_pal_rgb*4]      ; la paleta: aquí, rojo, verde y azul;
        mov ecx, 256                ; en el .BMP, azul, verde y rojo
.p:
        mov al, [esi+2]
        call esc_car
        mov al, [esi+1]
        call esc_car
        mov al, [esi]
        call esc_car
        xor al, al
        call esc_car
        add esi, 4
        dec ecx
        jnz .p
        mov ebx, LZ_AL              ; las filas, de la de abajo a la de arriba
.f:
        dec ebx
        imul esi, ebx, IB_AN
        add esi, lienzo
        mov ecx, IB_AN
.f1:
        lodsb
        and al, 15
        call esc_car
        dec ecx
        jnz .f1
        test ebx, ebx
        jnz .f
        call esc_cierra
        jc .falla
        mov esi, t_gr_guardado
        call aviso
.x:
        popad
        ret
.falla:
        mov esi, t_no_guardar
        call aviso
        popad
        ret

; =============================================================================
;  Leer los datos de la hoja
; =============================================================================
; (interna) la celda que hace EAX en el sentido de las series y EBX en el de
; los puntos (contando los rótulos y los nombres) -> ECX=columna EDX=fila
gr_cel:
        cmp byte [gr_filas], 0
        jne .f
        mov ecx, [gr_c1]
        add ecx, eax
        mov edx, [gr_f1]
        add edx, ebx
        ret
.f:
        mov ecx, [gr_c1]
        add ecx, ebx
        mov edx, [gr_f1]
        add edx, eax
        ret
; (interna) ¿esa celda tiene un número? ZF=1 si sí. Conserva los registros
gr_es_num:
        push ecx
        push edx
        call gr_cel
        call celda_ver
        cmp byte [disp_tipo], 1
        pop edx
        pop ecx
        ret
; (interna) copia [disp] a EDI: 15 letras como mucho, y el 0
gr_copia15:
        push eax
        push ecx
        push esi
        push edi
        mov esi, disp
        mov ecx, 15
.c:
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        pop edi
        pop esi
        pop ecx
        pop eax
        ret
; (interna) ESI=texto -> ECX=sus letras
gr_largo:
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .x
        inc ecx
        jmp .l
.x:
        ret

; lee de la hoja el rango del gráfico: [gr_ns] series de [gr_np] puntos;
; las cantidades en gr_v (y en gr_hayv, si la celda tiene número), los
; nombres de las series en gr_nser, los rótulos de los puntos en gr_nom y
; cuántas cantidades hay en total en [gr_n]
gr_lee:
        pushad
        mov eax, [gr_c2]
        sub eax, [gr_c1]
        inc eax
        mov ebx, [gr_f2]
        sub ebx, [gr_f1]
        inc ebx
        cmp byte [gr_filas], 0
        je .uv
        xchg eax, ebx
.uv:
        mov [gl_u], eax             ; lo que hay en el sentido de las series
        mov [gl_v], ebx             ; y en el de los puntos
        mov byte [gr_et], 0         ; ¿la primera son rótulos? Ningún número
        cmp eax, 2
        jb .cab
        xor ebx, ebx
.e1:
        xor eax, eax
        call gr_es_num
        je .cab
        inc ebx
        cmp ebx, [gl_v]
        jb .e1
        mov byte [gr_et], 1
.cab:
        mov byte [gr_cab], 0        ; ¿y la primera de cada serie, su nombre?
        cmp dword [gl_v], 2
        jb .cuenta
        movzx eax, byte [gr_et]
.c1:
        cmp eax, [gl_u]
        jae .c2
        xor ebx, ebx
        call gr_es_num
        je .cuenta
        inc eax
        jmp .c1
.c2:
        mov byte [gr_cab], 1
.cuenta:
        movzx ecx, byte [gr_et]
        mov eax, [gl_u]
        sub eax, ecx
        cmp eax, GR_SER
        jbe .n1
        mov eax, GR_SER
.n1:
        mov [gr_ns], eax
        movzx ecx, byte [gr_cab]
        mov eax, [gl_v]
        sub eax, ecx
        cmp eax, GR_MAX
        jbe .n2
        mov eax, GR_MAX
.n2:
        mov [gr_np], eax
        xor ebp, ebp                ; los nombres de las series
.ns:
        cmp ebp, [gr_ns]
        jae .nsf
        mov edi, ebp
        shl edi, 4
        add edi, gr_nser
        mov byte [edi], 0
        cmp byte [gr_cab], 0
        je .nsd
        movzx eax, byte [gr_et]
        add eax, ebp
        xor ebx, ebx
        call gr_cel
        call celda_ver
        call gr_copia15
.nsd:
        cmp byte [edi], 0
        jne .nss
        push edi
        mov esi, t_gr_serie
        call hvl_cad
        lea eax, [ebp+1]
        call hvl_num
        mov byte [edi], 0
        pop edi
.nss:
        inc ebp
        jmp .ns
.nsf:
        xor ebp, ebp                ; los rótulos de los puntos
.np:
        cmp ebp, [gr_np]
        jae .npf
        mov edi, ebp
        shl edi, 4
        add edi, gr_nom
        mov byte [edi], 0
        movzx ebx, byte [gr_cab]
        add ebx, ebp
        xor eax, eax
        call gr_cel
        cmp byte [gr_et], 0
        je .npd
        call celda_ver
        call gr_copia15
.npd:
        cmp byte [edi], 0
        jne .nps
        push edi                    ; sin rótulo: el número de su fila (o la
        cmp byte [gr_filas], 0      ; letra de su columna)
        jne .npc
        lea eax, [edx+1]
        call hvl_num
        jmp .npz
.npc:
        call col_nombre
.npz:
        mov byte [edi], 0
        pop edi
.nps:
        inc ebp
        jmp .np
.npf:
        mov dword [gr_n], 0         ; y las cantidades
        xor ebp, ebp
.vs:
        cmp ebp, [gr_ns]
        jae .vf
        xor esi, esi
.vp:
        cmp esi, [gr_np]
        jae .vsf
        movzx eax, byte [gr_et]
        add eax, ebp
        movzx ebx, byte [gr_cab]
        add ebx, esi
        call gr_cel
        imul edi, ebp, GR_MAX
        add edi, esi
        mov byte [gr_hayv+edi], 0
        mov dword [gr_v+edi*8], 0
        mov dword [gr_v+edi*8+4], 0
        call celda_ver
        cmp byte [disp_tipo], 1
        jne .vn
        call valor_celda
        mov [gr_v+edi*8], eax
        mov [gr_v+edi*8+4], edx
        mov byte [gr_hayv+edi], 1
        inc dword [gr_n]
        test ebp, ebp               ; (las de la primera serie, también como
        jnz .vn                     ; se ven: se escriben sobre las barras)
        push edi
        mov edi, esi
        shl edi, 4
        add edi, gr_vtxt
        call gr_copia15
        pop edi
.vn:
        inc esi
        jmp .vp
.vsf:
        inc ebp
        jmp .vs
.vf:
        popad
        ret

; =============================================================================
;  Las escalas
; =============================================================================
; (internas) la menor y la mayor: se empieza, y se cuenta EDX:EAX
gm_ini:
        mov dword [gm_min], 0xFFFFFFFF
        mov dword [gm_min+4], 0x7FFFFFFF
        mov dword [gm_max], 0
        mov dword [gm_max+4], 0x80000000
        ret
gm_pon:
        cmp edx, [gm_min+4]
        jg .a
        jl .pmin
        cmp eax, [gm_min]
        jae .a
.pmin:
        mov [gm_min], eax
        mov [gm_min+4], edx
.a:
        cmp edx, [gm_max+4]
        jl .x
        jg .pmax
        cmp eax, [gm_max]
        jbe .x
.pmax:
        mov [gm_max], eax
        mov [gm_max+4], edx
.x:
        ret
; (interna) EDX:EAX (sin signo) por ECX -> EDX:EAX; CF=1 si no cabe
gr_por:
        push ebx
        push esi
        mov ebx, edx
        mul ecx
        mov esi, edx
        xchg eax, ebx               ; EBX=lo de abajo del resultado
        mul ecx
        add eax, esi
        adc edx, 0
        neg edx                     ; (CF=1 si lo que sobra no es cero)
        mov edx, eax
        mov eax, ebx
        pop esi
        pop ebx
        ret

; las escalas del gráfico: la vertical en gr_ey y, en el de dispersión, la
; horizontal en gr_ex. [gp_s0]=la primera serie que se dibuja
gr_escalas:
        pushad
        call gm_ini
        cmp dword [gr_tipo], GT_APILADAS
        je .apil
        mov ebx, [gp_s0]
.s:
        cmp ebx, [gr_ns]
        jae .sf
        call .serie
        inc ebx
        jmp .s
.sf:
        mov edi, gr_ey
        cmp dword [gr_tipo], GT_BARRAS      ; las barras salen del cero
        sete bl
        call gr_escala
        cmp dword [gr_tipo], GT_XY
        jne .x
        call gm_ini
        xor ebx, ebx
        call .serie
        mov edi, gr_ex
        xor bl, bl
        call gr_escala
.x:
        popad
        ret
; apiladas: de cada punto, la suma de las positivas y la de las negativas
.apil:
        xor ecx, ecx
.ap:
        cmp ecx, [gr_np]
        jae .apf
        xor eax, eax
        mov [gp_acp], eax
        mov [gp_acp+4], eax
        mov [gp_acn], eax
        mov [gp_acn+4], eax
        xor ebx, ebx
.as:
        cmp ebx, [gr_ns]
        jae .asf
        imul esi, ebx, GR_MAX
        add esi, ecx
        mov eax, [gr_v+esi*8]
        mov edx, [gr_v+esi*8+4]
        mov edi, gp_acp
        test edx, edx
        jns .a1
        mov edi, gp_acn
.a1:
        add [edi], eax
        adc [edi+4], edx
        inc ebx
        jmp .as
.asf:
        mov eax, [gp_acp]
        mov edx, [gp_acp+4]
        call gm_pon
        mov eax, [gp_acn]
        mov edx, [gp_acn+4]
        call gm_pon
        inc ecx
        jmp .ap
.apf:
        mov edi, gr_ey
        mov bl, 1
        call gr_escala
        popad
        ret
; (interna) cuenta las cantidades de la serie EBX
.serie:
        xor ecx, ecx
.p:
        cmp ecx, [gr_np]
        jae .pf
        imul esi, ebx, GR_MAX
        add esi, ecx
        cmp byte [gr_hayv+esi], 0
        je .pn
        mov eax, [gr_v+esi*8]
        mov edx, [gr_v+esi*8+4]
        call gm_pon
.pn:
        inc ecx
        jmp .p
.pf:
        ret

; hace en EDI la escala que abarca de [gm_min] a [gm_max] (y el cero, si BL
; no es 0)
gr_escala:
        pushad
        test bl, bl
        jz .a
        xor eax, eax
        xor edx, edx
        call gm_pon
.a:
        mov eax, [gm_min]
        mov edx, [gm_min+4]
        cmp edx, [gm_max+4]
        jg .vacio
        jl .b
        cmp eax, [gm_max]
        ja .vacio
        jb .b
        mov ecx, eax                ; una sola cantidad: del cero a ella
        or ecx, edx
        jz .vacio
        xor ecx, ecx
        test edx, edx
        js .neg
        mov [gm_min], ecx
        mov [gm_min+4], ecx
        jmp .b
.neg:
        mov [gm_max], ecx
        mov [gm_max+4], ecx
        jmp .b
.vacio:
        xor ecx, ecx                ; nada, o solo ceros: de 0 a 1
        mov [gm_min], ecx
        mov [gm_min+4], ecx
        mov dword [gm_max], CU_ESC
        mov [gm_max+4], ecx
.b:
        mov eax, [gm_max]           ; lo que hay de la menor a la mayor
        mov edx, [gm_max+4]
        sub eax, [gm_min]
        sbb edx, [gm_min+4]
        mov [gp_r], eax
        mov [gp_r+4], edx
        mov dword [gp_base], 10000  ; el paso: desde una diezmilésima, 1, 2, 5,
        mov dword [gp_base+4], 0    ; 10, 20, 50... hasta que basten GR_DIV
        xor ebp, ebp
.paso:
        mov eax, [gp_base]
        mov edx, [gp_base+4]
        movzx ecx, byte [gr_mult+ebp]
        call gr_por
        jc .pf
        mov [edi+E_PASO], eax
        mov [edi+E_PASO+4], edx
        mov ecx, GR_DIV
        call gr_por
        jc .pf
        cmp edx, [gp_r+4]
        ja .pf
        jb .otro
        cmp eax, [gp_r]
        jae .pf
.otro:
        inc ebp
        cmp ebp, 3
        jb .paso
        xor ebp, ebp
        mov eax, [gp_base]
        mov edx, [gp_base+4]
        mov ecx, 10
        call gr_por
        jc .pf
        mov [gp_base], eax
        mov [gp_base+4], edx
        jmp .paso
.pf:
        mov eax, [gm_min]           ; empieza en el múltiplo del paso de debajo
        mov edx, [gm_min+4]         ; de la menor
        call .resto
        jz .lo_ok
        test edx, edx
        js .lo_neg
        sub eax, [gp_t]
        sbb edx, [gp_t+4]
        jmp .lo_ok
.lo_neg:
        sub eax, [edi+E_PASO]
        sbb edx, [edi+E_PASO+4]
        add eax, [gp_t]
        adc edx, [gp_t+4]
.lo_ok:
        mov [edi+E_LO], eax
        mov [edi+E_LO+4], edx
        mov eax, [gm_max]           ; y acaba en el de encima de la mayor
        mov edx, [gm_max+4]
        call .resto
        jz .hi_ok
        test edx, edx
        js .hi_neg
        add eax, [edi+E_PASO]
        adc edx, [edi+E_PASO+4]
        sub eax, [gp_t]
        sbb edx, [gp_t+4]
        jmp .hi_ok
.hi_neg:
        add eax, [gp_t]
        adc edx, [gp_t+4]
.hi_ok:
        mov [gp_hi], eax
        mov [gp_hi+4], edx
        xor ebx, ebx                ; cuántos pasos hay
        mov eax, [edi+E_LO]
        mov edx, [edi+E_LO+4]
.n:
        cmp edx, [gp_hi+4]
        jg .nf
        jl .n1
        cmp eax, [gp_hi]
        jae .nf
.n1:
        add eax, [edi+E_PASO]
        adc edx, [edi+E_PASO+4]
        inc ebx
        cmp ebx, 40
        jb .n
.nf:
        test ebx, ebx
        jnz .n2
        inc ebx
.n2:
        mov [edi+E_N], ebx
        mov eax, [edi+E_PASO]       ; lo que mide, sin los bits que sobran
        mov edx, [edi+E_PASO+4]
        mov ecx, ebx
        call gr_por
        xor ecx, ecx
.k:
        test edx, edx
        jnz .k1
        cmp eax, 1 << 20
        jb .kf
.k1:
        shrd eax, edx, 1
        shr edx, 1
        inc ecx
        jmp .k
.kf:
        test eax, eax
        jnz .kg
        inc eax
.kg:
        mov [edi+E_R], eax
        mov [edi+E_K], ecx
        popad
        ret
; (interna) [gp_t]=lo que le sobra a EDX:EAX (sin su signo) de un múltiplo
; del paso; ZF=1 si nada. Conserva EDX:EAX
.resto:
        push eax
        push edx
        call abs64
        mov ebx, [edi+E_PASO]
        mov ecx, [edi+E_PASO+4]
        call mod64
        mov [gp_t], eax
        mov [gp_t+4], edx
        or eax, edx
        pop edx
        pop eax
        ret

; dónde cae la cantidad EDX:EAX en la escala ESI, que mide ECX puntos ->
; EAX, de 0 a ECX. Conserva los demás registros
gr_pos:
        push ebx
        push edx
        sub eax, [esi+E_LO]
        sbb edx, [esi+E_LO+4]
        js .cero
        mov ebx, [esi+E_K]
.s:
        test ebx, ebx
        jz .sf
        shrd eax, edx, 1
        shr edx, 1
        dec ebx
        jmp .s
.sf:
        test edx, edx
        jnz .tope
        cmp eax, [esi+E_R]
        ja .tope
        mul ecx
        div dword [esi+E_R]
        jmp .x
.tope:
        mov eax, ecx
        jmp .x
.cero:
        xor eax, eax
.x:
        pop edx
        pop ebx
        ret
; ...y la y del lienzo de esa cantidad, en la escala vertical
gr_y:
        push ecx
        push esi
        mov esi, gr_ey
        mov ecx, [gp_ph]
        call gr_pos
        neg eax
        add eax, [gp_py0]
        add eax, [gp_ph]
        pop esi
        pop ecx
        ret
; la raya EAX de la escala ESI, como texto, en gr_txt -> ECX=sus letras
gr_marca_txt:
        push eax
        push edx
        push edi
        mov ecx, eax
        mov eax, [esi+E_LO]
        mov edx, [esi+E_LO+4]
        jecxz .f
.s:
        add eax, [esi+E_PASO]
        adc edx, [esi+E_PASO+4]
        dec ecx
        jnz .s
.f:
        mov dword [nf_cars], 12
        mov edi, gr_txt
        call num_fmt
        pop edi
        pop edx
        pop eax
        push esi
        mov esi, gr_txt
        call gr_largo
        pop esi
        ret

; =============================================================================
;  Dibujar el gráfico en el lienzo
; =============================================================================
; ECX=ancho EDX=alto (como mucho, IB_AN x LZ_AL). Deja [ib_base], [ib_al] y
; [ib_cx1] puestos al lienzo. Conserva los registros
gr_dibuja:
        pushad
        mov [gp_an], ecx
        mov [gp_al], edx
        mov dword [ib_base], lienzo
        mov [ib_al], edx
        mov [ib_cx1], ecx
        call ib_limpia
        mov byte [tc_fg], C_TINTA
        mov byte [tc_neg], 0
        mov byte [ib_cur], 0
        call gr_lee
        mov dword [gp_py0], 10
        mov esi, gr_tit             ; el título, centrado y en negrita
        call gr_largo
        jecxz .st
        mov eax, [gp_an]
        lea ebx, [ecx*8]
        sub eax, ebx
        sar eax, 1
        mov ebx, 6
        mov byte [tc_neg], 1
        call ib_texto
        mov byte [tc_neg], 0
        mov dword [gp_py0], 30
.st:
        mov esi, t_gr_sin
        cmp dword [gr_n], 0
        je .dice
        cmp dword [gr_tipo], GT_SECTORES
        je gr_sectores
        mov dword [gp_s0], 0
        cmp dword [gr_tipo], GT_XY
        jne .esc
        mov dword [gp_s0], 1
        mov esi, t_gr_xy_falta
        cmp dword [gr_ns], 2
        jb .dice
.esc:
        cmp dword [gr_tipo], GT_BARRAS      ; (sitio para la cantidad de encima
        jne .e1                     ; de cada barra)
        cmp dword [gr_ns], 1
        jne .e1
        add dword [gp_py0], 16
.e1:
        call gr_escalas
        call gr_marco
        jc .fin
        mov eax, [gr_tipo]
        jmp [gr_formas+eax*4]
.dice:
        call gr_largo
        mov eax, 16
        mov ebx, [gp_py0]
        add ebx, 6
        call ib_texto
.fin:
        popad
        ret

; (interna) lo que rodea al dibujo: reparte el sitio ([gp_px0], [gp_py0],
; [gp_pw], [gp_ph]: donde van las barras o las líneas) y pinta la escala con
; sus rayas guía, los ejes, la leyenda y los rótulos de los ejes. CF=1 si la
; ventana es tan pequeña que no cabe
gr_marco:
        pushad
        mov dword [gp_lab], 1       ; lo que miden los números de la escala
        xor eax, eax
.l:
        cmp eax, [gr_ey+E_N]
        ja .lf
        mov esi, gr_ey
        call gr_marca_txt
        cmp ecx, [gp_lab]
        jbe .l1
        mov [gp_lab], ecx
.l1:
        inc eax
        jmp .l
.lf:
        mov eax, [gp_lab]
        lea eax, [eax*8+12]
        cmp byte [gr_ejey], 0
        je .i1
        add eax, 20
.i1:
        mov [gp_px0], eax
        mov dword [gp_ley], 0       ; la leyenda, si hay más de una serie
        mov eax, [gr_ns]
        sub eax, [gp_s0]
        cmp eax, 2
        jb .sinley
        xor edx, edx
        mov ebx, [gp_s0]
.ll:
        cmp ebx, [gr_ns]
        jae .llf
        mov esi, ebx
        shl esi, 4
        add esi, gr_nser
        call gr_largo
        cmp ecx, edx
        jbe .ll1
        mov edx, ecx
.ll1:
        inc ebx
        jmp .ll
.llf:
        lea eax, [edx*8+30]
        mov [gp_ley], eax
.sinley:
        mov eax, [gp_an]
        sub eax, [gp_px0]
        sub eax, [gp_ley]
        sub eax, 14
        mov [gp_pw], eax
        cmp eax, 40
        jl .no
        mov ebx, 24
        cmp byte [gr_ejex], 0
        je .a1
        add ebx, 18
.a1:
        mov eax, [gp_al]
        sub eax, [gp_py0]
        sub eax, ebx
        mov [gp_ph], eax
        cmp eax, 30
        jl .no
        ; la escala vertical: de cada raya, su número y una guía punteada
        mov dword [gp_i], 0
.g:
        mov eax, [gp_i]
        cmp eax, [gr_ey+E_N]
        ja .gf
        mul dword [gp_ph]
        div dword [gr_ey+E_N]
        neg eax
        add eax, [gp_py0]
        add eax, [gp_ph]
        mov [gp_y], eax
        mov byte [ib_c], C_ARENA_OSC
        mov ebx, eax
        mov eax, [gp_px0]
        mov ecx, [gp_pw]
        call ib_puntos_h
        mov eax, [gp_i]
        mov esi, gr_ey
        call gr_marca_txt
        mov eax, ecx
        shl eax, 3
        neg eax
        add eax, [gp_px0]
        sub eax, 6
        mov ebx, [gp_y]
        sub ebx, 8
        mov esi, gr_txt
        call ib_texto
        inc dword [gp_i]
        jmp .g
.gf:
        cmp dword [gr_tipo], GT_XY
        jne .ejes
        mov dword [gp_i], 0         ; dispersión: la escala horizontal
.h:
        mov eax, [gp_i]
        cmp eax, [gr_ex+E_N]
        ja .ejes
        mul dword [gp_pw]
        div dword [gr_ex+E_N]
        add eax, [gp_px0]
        mov [gp_x], eax
        mov byte [ib_c], C_ARENA_OSC
        mov ebx, [gp_py0]
        mov edx, [gp_ph]
        call ib_puntos_v
        mov eax, [gp_i]
        mov esi, gr_ex
        call gr_marca_txt
        mov eax, ecx
        shl eax, 2
        neg eax
        add eax, [gp_x]
        mov ebx, [gp_py0]
        add ebx, [gp_ph]
        add ebx, 5
        mov esi, gr_txt
        call ib_texto
        inc dword [gp_i]
        jmp .h
.ejes:
        mov byte [ib_c], C_ARENA_OSC        ; el eje vertical
        mov eax, [gp_px0]
        dec eax
        mov ebx, [gp_py0]
        mov ecx, 1
        mov edx, [gp_ph]
        inc edx
        call ib_rect
        xor eax, eax                ; y el horizontal, a la altura del cero (o
        xor edx, edx                ; abajo, si la escala no llega al cero)
        call gr_y
        mov [gp_cero], eax
        mov ebx, eax
        mov eax, [gp_px0]
        sub eax, 4
        mov ecx, [gp_pw]
        add ecx, 8
        mov edx, 1
        call ib_rect
        cmp dword [gp_ley], 0       ; la leyenda: una muestra y el nombre
        je .tit
        mov ebp, [gp_s0]
.ley:
        cmp ebp, [gr_ns]
        jae .tit
        mov eax, ebp
        sub eax, [gp_s0]
        mov ecx, eax
        imul eax, 18
        add eax, [gp_py0]
        mov [gp_y], eax
        mov ebx, eax
        add ebx, 2
        mov eax, [gp_an]
        sub eax, [gp_ley]
        add eax, 2
        call gr_muestra_c
        mov esi, ebp
        shl esi, 4
        add esi, gr_nser
        call gr_largo
        add eax, 18
        mov ebx, [gp_y]
        call ib_texto
        inc ebp
        jmp .ley
.tit:
        mov esi, gr_ejex            ; los rótulos de los ejes: el horizontal,
        call gr_largo               ; centrado debajo
        jecxz .ty
        mov eax, [gp_pw]
        lea ebx, [ecx*8]
        sub eax, ebx
        sar eax, 1
        add eax, [gp_px0]
        mov ebx, [gp_al]
        sub ebx, 19
        call ib_texto
.ty:
        mov esi, gr_ejey            ; y el vertical, de abajo arriba
        call gr_largo
        jecxz .si
        mov ebx, [gp_ph]
        lea eax, [ecx*8]
        add ebx, eax
        sar ebx, 1
        add ebx, [gp_py0]
        mov eax, 3
        mov byte [ib_c], C_TINTA
        call ib_texto_v
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; (interna) la muestra del color ECX (de 0 a 7...) en EAX=x EBX=y: un
; cuadrito con su marco. Conserva los registros
gr_muestra_c:
        pushad
        and ecx, 7
        mov dl, [gr_colores+ecx]
        or dl, IB_RAYADO
        mov [ib_c], dl
        mov ecx, 12
        mov edx, 12
        call ib_rect
        mov byte [ib_c], C_TINTA
        call ib_marco
        popad
        ret

; (interna) los rótulos de los puntos, debajo de cada uno (lo que quepa)
gr_rotulos:
        pushad
        xor ebp, ebp
.p:
        cmp ebp, [gr_np]
        jae .x
        mov esi, ebp
        shl esi, 4
        add esi, gr_nom
        call gr_largo
        mov eax, [gp_gw]
        shr eax, 3
        cmp ecx, eax
        jbe .r
        mov ecx, eax
.r:
        mov eax, ebp
        imul eax, [gp_gw]
        add eax, [gp_px0]
        mov ebx, [gp_py0]
        add ebx, [gp_ph]
        add ebx, 5
        call gr_centra
        inc ebp
        jmp .p
.x:
        popad
        ret
; (interna) escribe ESI (ECX letras) centrado en el sitio de un punto, que
; empieza en la x EAX, a la altura EBX; si no cabe, nada
gr_centra:
        push eax
        push edx
        lea edx, [ecx*8]
        neg edx
        add edx, [gp_gw]
        js .x
        sar edx, 1
        add eax, edx
        call ib_texto
.x:
        pop edx
        pop eax
        ret

; ---- barras, juntas o apiladas (dentro de gr_dibuja) ----
gr_barras:
        mov eax, [gp_pw]            ; el sitio de cada punto,
        xor edx, edx
        div dword [gr_np]
        mov [gp_gw], eax
        cmp eax, 4
        jl gr_dibuja.fin
        mov ecx, eax                ; con un margen a cada lado,
        shr ecx, 3
        inc ecx
        mov [gp_m], ecx
        sub eax, ecx
        sub eax, ecx
        cmp dword [gr_tipo], GT_APILADAS
        je .bw
        xor edx, edx                ; y el ancho de cada barra
        div dword [gr_ns]
.bw:
        test eax, eax
        jnz .bw1
        inc eax
.bw1:
        mov [gp_bw], eax
        xor ebp, ebp                ; EBP=el punto
.p:
        cmp ebp, [gr_np]
        jae .rot
        xor eax, eax
        mov [gp_acp], eax
        mov [gp_acp+4], eax
        mov [gp_acn], eax
        mov [gp_acn+4], eax
        xor edi, edi                ; EDI=la serie
.s:
        cmp edi, [gr_ns]
        jae .sf
        imul esi, edi, GR_MAX
        add esi, ebp
        cmp byte [gr_hayv+esi], 0
        je .ss
        mov eax, [gr_v+esi*8]
        mov edx, [gr_v+esi*8+4]
        cmp dword [gr_tipo], GT_APILADAS
        je .apila
        call gr_y                   ; juntas: del cero a la cantidad
        mov [gp_y], eax
        mov ebx, [gp_cero]
        mov eax, edi
        imul eax, [gp_bw]
        jmp .pinta
.apila:
        mov ebx, gp_acp             ; apiladas: desde donde llegaban las de
        test edx, edx               ; antes (las positivas hacia arriba y las
        jns .ap1                    ; negativas hacia abajo)
        mov ebx, gp_acn
.ap1:
        push eax
        push edx
        mov eax, [ebx]
        mov edx, [ebx+4]
        call gr_y
        mov ecx, eax                ; ECX=la y de donde sale
        pop edx
        pop eax
        add eax, [ebx]
        adc edx, [ebx+4]
        mov [ebx], eax
        mov [ebx+4], edx
        call gr_y
        mov [gp_y], eax
        mov ebx, ecx
        xor eax, eax
.pinta:
        add eax, [gp_m]             ; EAX=la x de la barra; va de [gp_y] a EBX
        mov ecx, ebp
        imul ecx, [gp_gw]
        add eax, ecx
        add eax, [gp_px0]
        mov edx, ebx
        mov ebx, [gp_y]
        sub edx, ebx
        jns .b1
        add ebx, edx                ; (hacia abajo)
        neg edx
.b1:
        inc edx
        mov ecx, edi
        and ecx, 7
        mov cl, [gr_colores+ecx]
        or cl, IB_RAYADO
        mov [ib_c], cl
        mov ecx, [gp_bw]
        call ib_rect
        mov byte [ib_c], C_TINTA
        call ib_marco
.ss:
        inc edi
        jmp .s
.sf:
        cmp dword [gr_tipo], GT_BARRAS      ; una sola serie: la cantidad, encima
        jne .pn                     ; de su barra (o debajo), si cabe
        cmp dword [gr_ns], 1
        jne .pn
        cmp byte [gr_hayv+ebp], 0
        je .pn
        mov esi, ebp
        shl esi, 4
        add esi, gr_vtxt
        call gr_largo
        mov ebx, [gp_y]
        sub ebx, 18
        test dword [gr_v+ebp*8+4], 0x80000000
        jz .v1
        add ebx, 22
.v1:
        mov eax, ebp
        imul eax, [gp_gw]
        add eax, [gp_px0]
        call gr_centra
.pn:
        inc ebp
        jmp .p
.rot:
        call gr_rotulos
        jmp gr_dibuja.fin

; ---- líneas: cada serie, sus puntos unidos (donde falta una cantidad, la
;      línea se corta) ----
gr_lineas:
        mov eax, [gp_pw]
        xor edx, edx
        div dword [gr_np]
        mov [gp_gw], eax
        cmp eax, 4
        jl gr_dibuja.fin
        xor edi, edi                ; EDI=la serie
.s:
        cmp edi, [gr_ns]
        jae .rot
        mov ecx, edi
        and ecx, 7
        mov cl, [gr_colores+ecx]
        mov [ib_c], cl
        mov byte [gp_hay], 0        ; (no hay punto anterior)
        xor ebp, ebp
.p:
        cmp ebp, [gr_np]
        jae .sf
        imul esi, edi, GR_MAX
        add esi, ebp
        cmp byte [gr_hayv+esi], 0
        jne .p1
        mov byte [gp_hay], 0
        jmp .pn
.p1:
        mov eax, [gr_v+esi*8]
        mov edx, [gr_v+esi*8+4]
        call gr_y
        mov ebx, eax
        mov eax, ebp
        imul eax, [gp_gw]
        add eax, [gp_px0]
        mov ecx, [gp_gw]
        shr ecx, 1
        add eax, ecx
        cmp byte [gp_hay], 0
        je .p2
        mov ecx, [gp_px]            ; dos puntos de gruesa
        mov edx, [gp_py]
        call ib_linea
        inc ebx
        inc edx
        call ib_linea
        dec ebx
.p2:
        mov [gp_px], eax
        mov [gp_py], ebx
        mov byte [gp_hay], 1
        sub eax, 2
        sub ebx, 2
        mov ecx, 5
        mov edx, 5
        call ib_rect
.pn:
        inc ebp
        jmp .p
.sf:
        inc edi
        jmp .s
.rot:
        call gr_rotulos
        jmp gr_dibuja.fin

; ---- dispersión: la primera serie son las X; de cada una de las demás, un
;      punto por cada par (X, Y) ----
gr_xy:
        mov edi, 1
.s:
        cmp edi, [gr_ns]
        jae gr_dibuja.fin
        lea ecx, [edi-1]
        and ecx, 7
        mov cl, [gr_colores+ecx]
        mov [ib_c], cl
        xor ebp, ebp
.p:
        cmp ebp, [gr_np]
        jae .sf
        imul esi, edi, GR_MAX
        add esi, ebp
        cmp byte [gr_hayv+esi], 0
        je .pn
        cmp byte [gr_hayv+ebp], 0
        je .pn
        mov eax, [gr_v+esi*8]
        mov edx, [gr_v+esi*8+4]
        call gr_y
        mov ebx, eax
        mov eax, [gr_v+ebp*8]
        mov edx, [gr_v+ebp*8+4]
        mov esi, gr_ex
        mov ecx, [gp_pw]
        call gr_pos
        add eax, [gp_px0]
        sub eax, 2
        sub ebx, 2
        mov ecx, 5
        mov edx, 5
        call ib_rect
.pn:
        inc ebp
        jmp .p
.sf:
        inc edi
        jmp .s

; ---- los sectores: cada cantidad positiva de la primera serie, su parte
;      del círculo; al lado, la leyenda con los tantos por ciento ----
gr_sectores:
        ; las cantidades, a enteros pequeños: los bits que le sobran a la
        ; mayor para caber en 24, fuera
        xor esi, esi
        xor edi, edi                ; EDI:ESI=la mayor
        xor ebx, ebx
.m:
        cmp ebx, [gr_np]
        jae .mf
        mov eax, [gr_v+ebx*8]
        mov edx, [gr_v+ebx*8+4]
        call abs64
        cmp edx, edi
        jb .m1
        ja .m0
        cmp eax, esi
        jbe .m1
.m0:
        mov esi, eax
        mov edi, edx
.m1:
        inc ebx
        jmp .m
.mf:
        xor ecx, ecx                ; ECX=cuántos bits se quitan
.k:
        test edi, edi
        jnz .k1
        cmp esi, 1 << 24
        jb .kf
.k1:
        shrd esi, edi, 1
        shr edi, 1
        inc ecx
        jmp .k
.kf:
        xor ebx, ebx
.v:
        cmp ebx, [gr_np]
        jae .vf
        mov eax, [gr_v+ebx*8]
        mov edx, [gr_v+ebx*8+4]
        mov esi, ecx
.v1:
        test esi, esi
        jz .v2
        shrd eax, edx, 1
        sar edx, 1
        dec esi
        jmp .v1
.v2:
        mov [gr_val+ebx*4], eax
        inc ebx
        jmp .v
.vf:
        xor eax, eax                ; el total, y hasta dónde llega cada sector
        xor ecx, ecx
.t:
        mov edx, [gr_val+ecx*4]
        test edx, edx
        jle .t1
        add eax, edx
.t1:
        mov [gr_lim+ecx*4], eax
        inc ecx
        cmp ecx, [gr_np]
        jb .t
        mov [gp_r], eax
        mov esi, t_gr_sin
        test eax, eax
        jz gr_dibuja.dice
        xor ecx, ecx
.e:
        mov eax, [gr_lim+ecx*4]
        mov ebx, 1024
        imul ebx
        idiv dword [gp_r]
        mov [gr_lim+ecx*4], eax
        inc ecx
        cmp ecx, [gr_np]
        jb .e
        xor edx, edx                ; la leyenda: lo que mide el nombre más largo
        xor ebx, ebx
.ll:
        cmp ebx, [gr_np]
        jae .llf
        mov esi, ebx
        shl esi, 4
        add esi, gr_nom
        call gr_largo
        cmp ecx, edx
        jbe .ll1
        mov edx, ecx
.ll1:
        inc ebx
        jmp .ll
.llf:
        lea eax, [edx*8+6*8+30]
        mov [gp_ley], eax
        mov eax, [gp_an]            ; el radio: lo que quepa
        sub eax, [gp_ley]
        sub eax, 28
        mov ebx, [gp_al]
        sub ebx, [gp_py0]
        sub ebx, 14
        cmp eax, ebx
        jle .r0
        mov eax, ebx
.r0:
        sar eax, 1
        cmp eax, 16
        jl gr_dibuja.fin
        mov [gp_ph], eax            ; [gp_ph]=el radio
        lea ebx, [eax+eax+1]
        mov [gp_pw], ebx            ; [gp_pw]=el lado del cuadro
        imul eax, eax
        mov [gp_bw], eax            ; [gp_bw]=el radio al cuadrado
        xor ebx, ebx                ; EBX=la fila
.fy:
        mov edi, [gp_py0]
        add edi, ebx
        imul edi, IB_AN
        add edi, lienzo + 20
        xor ecx, ecx                ; ECX=la columna
.fx:
        mov eax, ecx
        sub eax, [gp_ph]            ; EAX=dx
        mov edx, ebx
        sub edx, [gp_ph]            ; EDX=dy
        push ebx
        push ecx
        mov esi, eax
        imul esi, eax
        push edx
        imul edx, edx
        add esi, edx
        pop edx
        cmp esi, [gp_bw]            ; fuera del círculo: como está
        ja .sig
        call .angulo                ; EAX=el ángulo, de 0 a 1023
        xor ecx, ecx
.s:
        cmp eax, [gr_lim+ecx*4]
        jb .sf
        inc ecx
        cmp ecx, [gr_np]
        jb .s
        dec ecx
.sf:
        and ecx, 7
        mov bl, [gr_colores+ecx]
        or bl, IB_RAYADO
        mov [edi], bl
.sig:
        inc edi
        pop ecx
        pop ebx
        inc ecx
        cmp ecx, [gp_pw]
        jb .fx
        inc ebx
        cmp ebx, [gp_pw]
        jb .fy
        ; la leyenda: el color, el nombre y el tanto por ciento
        xor ebp, ebp
        mov eax, [gp_py0]
        mov [gp_y], eax
.ley:
        cmp dword [gr_val+ebp*4], 0
        jle .ls
        mov eax, [gp_pw]
        add eax, 40
        mov ebx, [gp_y]
        add ebx, 2
        mov ecx, ebp
        call gr_muestra_c
        mov esi, ebp
        shl esi, 4
        add esi, gr_nom
        mov edi, gr_txt
        call hvl_cad
        mov ax, ': '
        stosw
        mov eax, [gr_val+ebp*4]     ; el tanto por ciento, redondeado
        mov ebx, 200
        imul ebx
        idiv dword [gp_r]
        inc eax
        shr eax, 1
        call hvl_num
        mov ax, '%'
        stosw
        mov esi, gr_txt
        call gr_largo
        mov eax, [gp_pw]
        add eax, 58
        mov ebx, [gp_y]
        call ib_texto
        add dword [gp_y], 18
.ls:
        inc ebp
        cmp ebp, [gr_np]
        jb .ley
        jmp gr_dibuja.fin
; (interna) el ángulo del punto EAX=dx EDX=dy, contado desde arriba y en el
; sentido de las agujas del reloj, en 1.024 partes de vuelta
.angulo:
        push ebx
        push ecx
        push esi
        mov esi, eax                ; ESI=dx, ECX=dy (con signo)
        mov ecx, edx
        mov eax, esi
        test eax, eax
        jns .a1
        neg eax
.a1:
        mov ebx, ecx
        test ebx, ebx
        jns .a2
        neg ebx
.a2:
        ; EAX=|dx| EBX=|dy|: el ángulo con la vertical, de 0 a 256
        cmp ebx, eax
        jae .a3
        xchg eax, ebx               ; más tumbado que de pie: 256 menos el otro
        shl eax, 6
        xor edx, edx
        div ebx
        movzx eax, byte [gr_atan+eax]
        neg eax
        add eax, 256
        jmp .a4
.a3:
        test ebx, ebx
        jz .a5                      ; (el centro)
        shl eax, 6
        xor edx, edx
        div ebx
        movzx eax, byte [gr_atan+eax]
.a4:
        test ecx, ecx               ; hacia abajo: 512 menos eso
        js .a6
        neg eax
        add eax, 512
.a6:
        test esi, esi               ; hacia la izquierda: la vuelta menos eso
        jns .a5
        neg eax
        add eax, 1024
.a5:
        and eax, 1023
        pop esi
        pop ecx
        pop ebx
        ret

        section .bss
            alignb 4
gr_ven      resd 1                  ; la ventana del gráfico (0: no hay)
gr_tipo     resd 1                  ; GT_*
gr_c1       resd 1                  ; el rango del que sale (en el orden de r_c1...)
gr_f1       resd 1
gr_c2       resd 1
gr_f2       resd 1
gr_ant      resd 5                  ; (el de antes, mientras se prueba uno nuevo)
gr_n        resd 1                  ; cuántas cantidades hay
gr_ns       resd 1                  ; cuántas series
gr_np       resd 1                  ; de cuántos puntos
gl_u        resd 1                  ; gr_lee: lo que mide el rango en el sentido
gl_v        resd 1                  ; de las series y en el de los puntos
gm_min      resd 2                  ; la menor y la mayor
gm_max      resd 2
gr_ey       resb E_TAM              ; la escala vertical
gr_ex       resb E_TAM              ; y la horizontal (dispersión)
gp_an       resd 1                  ; al dibujar: el ancho y el alto,
gp_al       resd 1
gp_px0      resd 1                  ; el sitio del dibujo,
gp_py0      resd 1
gp_pw       resd 1
gp_ph       resd 1
gp_s0       resd 1                  ; la primera serie que se dibuja
gp_lab      resd 1                  ; lo que miden los números de la escala
gp_ley      resd 1                  ; el ancho de la leyenda (0: no hay)
gp_gw       resd 1                  ; el sitio de cada punto,
gp_m        resd 1                  ; su margen
gp_bw       resd 1                  ; y el ancho de una barra
gp_cero     resd 1                  ; la y del cero
gp_i        resd 1
gp_x        resd 1
gp_y        resd 1
gp_px       resd 1
gp_py       resd 1
gp_r        resd 2
gp_t        resd 2
gp_hi       resd 2
gp_base     resd 2
gp_acp      resd 2                  ; apiladas: hasta dónde llegan las positivas
gp_acn      resd 2                  ; y las negativas
gr_val      resd GR_MAX             ; sectores: las cantidades, como enteros pequeños
gr_lim      resd GR_MAX             ; y hasta qué ángulo llega cada uno
gr_v        resd 2 * GR_SER * GR_MAX        ; las cantidades (la serie por GR_MAX, más el punto)
gr_hayv     resb GR_SER * GR_MAX    ; 1: esa celda tiene un número
gr_nser     resb 16 * GR_SER        ; los nombres de las series
gr_nom      resb 16 * GR_MAX        ; los rótulos de los puntos
gr_vtxt     resb 16 * GR_MAX        ; las cantidades de la primera serie, como se ven
gr_txt      resb 48
gr_tit      resb 44                 ; el título
gr_ejex     resb 28                 ; y los rótulos de los ejes
gr_ejey     resb 28
gr_hay      resb 1                  ; la hoja tiene gráfico
gr_filas    resb 1                  ; las series van por filas
gr_et       resb 1                  ; gr_lee: hay rótulos de los puntos
gr_cab      resb 1                  ; y nombres de las series
gp_hay      resb 1
        section .text
