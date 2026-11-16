; =============================================================================
;  Cuentas - los gráficos: de barras, de líneas y de sectores
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El gráfico sale en una ventana aparte, de lo elegido al pedirlo (recortado
;  a lo escrito). Cada FILA de lo elegido es un punto: su cantidad es el
;  último número de la fila y su nombre, el primer texto (si no hay, el
;  número de la fila). Si lo elegido es una sola fila, cada celda con número
;  es un punto, con el nombre de su celda. Como mucho, GR_MAX puntos.
;
;  La ventana guarda el rango, no los números: cada vez que se pinta los
;  vuelve a leer, así que el gráfico sigue a la hoja cuando cambia.
;
;  Sin coma flotante: las cantidades se pasan a enteros pequeños (se les
;  quitan por abajo los bits que haga falta para que la mayor quepa en 24) y
;  las alturas salen de multiplicar y dividir enteros. Los sectores se pintan
;  punto a punto en un mapa aparte: de cada punto del círculo se saca su
;  ángulo (en 1.024 partes de vuelta, con una tabla de arcotangentes de un
;  octavo de vuelta) y se mira en qué sector cae.
; =============================================================================
GR_MAX      equ 24                  ; puntos de un gráfico, como mucho
GR_R        equ 100                 ; radio del gráfico de sectores, como mucho
GR_IZQ      equ 40                  ; márgenes del dibujo
GR_DER      equ 16
GR_ARR      equ 26
GR_ABA      equ 40

c_grafico   dd gr_proc
            db C_BLANCO, 0, 0, 0
gr_plant:
        PLANTILLA c_grafico, t_gr_barras, E_NORMAL, 130, 100, 500, 360, 0
t_gr_barras db "Gráfico de barras", 0
t_gr_lineas db "Gráfico de líneas", 0
t_gr_sectores db "Gráfico de sectores", 0
t_gr_nada   db "Para hacer un gráfico, elija antes las celdas con las cantidades", 10
            db "(y, si quiere, la columna de los nombres, a su izquierda).", 0
t_gr_sin    db "No hay cantidades que dibujar.", 0
            align 4
gr_titulos  dd t_gr_barras, t_gr_lineas, t_gr_sectores
gr_colores  db C_PETROLEO, C_TERRACOTA, C_OCRE, C_OLIVA, C_ANIL, C_GRANATE, C_TURQUESA, C_MARRON
; el ángulo cuya tangente es n/64, en 1.024 partes de vuelta (de 0 a 45 grados)
gr_atan     db 0, 3, 5, 8, 10, 13, 15, 18, 20, 23, 25, 28, 30
            db 33, 35, 38, 40, 42, 45, 47, 49, 52, 54, 56, 58, 61
            db 63, 65, 67, 69, 71, 74, 76, 78, 80, 82, 84, 85, 87
            db 89, 91, 93, 95, 96, 98, 100, 102, 103, 105, 106, 108, 110
            db 111, 113, 114, 116, 117, 119, 120, 121, 123, 124, 125, 127, 128

; la orden: EAX=0 barras, 1 líneas, 2 sectores
grafico:
        mov [gr_tipo], eax
        call sel_rect
        call rect_recorta
        mov esi, r_c1
        mov edi, gr_c1
        mov ecx, 4
        rep movsd
        call gr_datos
        cmp dword [gr_n], 0
        jne .hay
        mov esi, t_gr_nada
        jmp aviso
.hay:
        mov eax, [gr_tipo]
        mov esi, [gr_titulos+eax*4]
        mov [gr_plant+P_TEXTO], esi
        mov ebp, [gr_ven]
        test ebp, ebp
        jz .nueva
        HV ven_texto                ; ya hay una ventana: la misma, con lo nuevo
        HV ven_invalidar
        HV ven_activar
        ret
.nueva:
        mov esi, gr_plant
        xor ebp, ebp
        HV ven_crear
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

gr_proc:
        cmp eax, WM_PAINT
        je gr_pintar
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
        jmp .x
.tam:
        HV ven_invalidar
        jmp .x
.destruir:
        mov dword [gr_ven], 0
.x:
        xor eax, eax
        ret

; lee de la hoja los puntos del rango [gr_c1]...: [gr_n] puntos, con su
; cantidad en gr_val (ya como entero pequeño), como se ve en gr_txt y su
; nombre en gr_nom
gr_datos:
        pushad
        mov dword [gr_n], 0
        mov edx, [gr_f1]
.f:
        mov ebx, [gr_n]
        cmp ebx, GR_MAX
        jae .esc
        mov edi, ebx                ; el nombre, si no sale otro: el número de la fila
        shl edi, 4
        add edi, gr_nom
        push edi
        lea eax, [edx+1]
        call hvl_num
        mov byte [edi], 0
        pop edi
        xor ebp, ebp                ; bit 0: ya tiene número; bit 1: ya tiene nombre
        mov ecx, [gr_c1]
.c:
        call celda_ver
        cmp byte [disp_tipo], 1
        jne .t
        push edx
        call valor_celda
        mov [gr_v64+ebx*8], eax
        mov [gr_v64+ebx*8+4], edx
        pop edx
        push edi
        mov edi, ebx
        shl edi, 4
        add edi, gr_txt
        mov eax, 15
        call .copia
        pop edi
        or ebp, 1
        mov eax, [gr_f1]            ; una sola fila: cada número es un punto,
        cmp eax, [gr_f2]            ; con el nombre de su celda
        jne .s
        push edi
        call ref_nombre
        mov byte [edi], 0
        pop edi
        inc ebx
        mov [gr_n], ebx
        cmp ebx, GR_MAX
        jae .esc
        mov edi, ebx
        shl edi, 4
        add edi, gr_nom
        xor ebp, ebp
        jmp .s
.t:
        cmp byte [disp_tipo], 2
        jne .s
        test ebp, 2
        jnz .s
        mov eax, 15
        call .copia
        or ebp, 2
.s:
        inc ecx
        cmp ecx, [gr_c2]
        jbe .c
        test ebp, 1
        jz .sf
        inc dword [gr_n]
.sf:
        inc edx
        cmp edx, [gr_f2]
        jbe .f
.esc:
        ; a enteros pequeños: los bits que sobran a la mayor (sin signo), fuera
        xor esi, esi
        xor edi, edi                ; EDI:ESI=la mayor
        xor ebx, ebx
.m:
        cmp ebx, [gr_n]
        jae .mf
        mov eax, [gr_v64+ebx*8]
        mov edx, [gr_v64+ebx*8+4]
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
        cmp ebx, [gr_n]
        jae .x
        mov eax, [gr_v64+ebx*8]
        mov edx, [gr_v64+ebx*8+4]
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
.x:
        popad
        ret
; (interna) copia [disp] a EDI, EAX letras como mucho, y el 0
.copia:
        push esi
        push edi
        mov esi, disp
.cp:
        mov byte [edi], 0
        test eax, eax
        jz .cpf
        movsb
        dec eax
        cmp byte [edi-1], 0
        jne .cp
.cpf:
        pop edi
        pop esi
        ret

; EAX por EBX entre ECX (con signo; el divisor, positivo) -> EAX
gr_regla:
        push edx
        imul ebx
        idiv ecx
        pop edx
        ret

gr_pintar:
        HV dc_pintar
        call gr_datos
        HV ven_cli
        mov [gp_an], ecx
        mov [gp_al], edx
        cmp dword [gr_n], 0
        jne .hay
.sin:
        COLOR C_TINTA
        mov eax, 16
        mov ebx, 16
        mov esi, t_gr_sin
        HV dc_texto
        jmp .fin
.hay:
        cmp dword [gr_tipo], 2
        je gr_sectores
        mov eax, [gp_an]            ; el sitio del dibujo
        sub eax, GR_IZQ + GR_DER
        mov [gp_w], eax
        cmp eax, 8
        jl .fin
        mov ebx, [gp_al]
        sub ebx, GR_ARR + GR_ABA
        mov [gp_h], ebx
        cmp ebx, 20
        jl .fin
        xor edx, edx
        div dword [gr_n]
        mov [gp_bw], eax            ; el ancho que le toca a cada punto
        cmp eax, 4
        jl .fin
        xor eax, eax                ; de la menor a la mayor, contando el cero
        xor ebx, ebx
        xor ecx, ecx
.mm:
        mov edx, [gr_val+ecx*4]
        cmp edx, eax
        jge .mm1
        mov eax, edx
.mm1:
        cmp edx, ebx
        jle .mm2
        mov ebx, edx
.mm2:
        inc ecx
        cmp ecx, [gr_n]
        jb .mm
        mov ecx, ebx
        sub ecx, eax
        jnz .mm3
        inc ecx
.mm3:
        mov [gp_rango], ecx
        mov eax, ebx                ; la altura del cero
        mov ebx, [gp_h]
        call gr_regla
        add eax, GR_ARR
        mov [gp_cero], eax
        COLOR C_ARENA_OSC           ; los ejes
        mov eax, GR_IZQ - 1
        mov ebx, GR_ARR - 6
        mov ecx, 1
        mov edx, [gp_h]
        add edx, 6
        HV dc_rect
        mov eax, GR_IZQ - 4
        mov ebx, [gp_cero]
        mov ecx, [gp_w]
        add ecx, 8
        mov edx, 1
        HV dc_rect
        mov dword [gp_i], 0
.p:
        mov edi, [gp_i]
        mov eax, edi
        imul eax, [gp_bw]
        add eax, GR_IZQ
        mov [gp_x], eax             ; donde empieza el sitio de este punto
        mov eax, [gr_val+edi*4]     ; su altura sobre el cero (o bajo él)
        mov ebx, [gp_h]
        mov ecx, [gp_rango]
        call gr_regla
        mov ebx, [gp_cero]
        sub ebx, eax
        mov [gp_y], ebx
        cmp dword [gr_tipo], 0
        jne .linea
        mov ecx, edi                ; una barra, de su color
        and ecx, 7
        movzx eax, byte [gr_colores+ecx]
        HV dc_color
        mov ecx, [gp_bw]            ; con un margen a cada lado
        mov eax, ecx
        shr eax, 3
        inc eax
        sub ecx, eax
        sub ecx, eax
        add eax, [gp_x]
        mov ebx, [gp_y]
        mov edx, [gp_cero]
        sub edx, ebx
        jns .b1
        mov ebx, [gp_cero]
        neg edx
.b1:
        inc edx
        HV dc_rect
        jmp .rot
.linea:
        mov eax, [gp_bw]            ; un punto, unido al de antes
        shr eax, 1
        add eax, [gp_x]
        mov ebx, [gp_y]
        test edi, edi
        jz .l1
        COLOR C_PETROLEO
        push eax
        push ebx
        mov ecx, [gp_px]
        mov edx, [gp_py]
        HV dc_linea
        inc ebx
        inc edx
        HV dc_linea
        pop ebx
        pop eax
.l1:
        mov [gp_px], eax
        mov [gp_py], ebx
        COLOR C_TERRACOTA
        sub eax, 2
        sub ebx, 2
        mov ecx, 5
        mov edx, 5
        HV dc_rect
.rot:
        COLOR C_TINTA               ; la cantidad, encima (o debajo), si cabe
        mov esi, edi
        shl esi, 4
        add esi, gr_txt
        call .largo
        mov ebx, [gp_y]
        sub ebx, 18
        cmp dword [gr_val+edi*4], 0
        jge .r1
        add ebx, 22
.r1:
        call .centra
        mov esi, edi                ; y el nombre, abajo
        shl esi, 4
        add esi, gr_nom
        call .largo
        mov ebx, [gp_al]
        sub ebx, GR_ABA - 12
        mov eax, [gp_bw]
        shr eax, 3
        cmp ecx, eax
        jbe .r2
        mov ecx, eax                ; (lo que quepa)
.r2:
        call .centra
        inc dword [gp_i]
        mov eax, [gp_i]
        cmp eax, [gr_n]
        jb .p
.fin:
        HV dc_cerrar
        xor eax, eax
        ret
; (internas) ECX=las letras del texto ESI; y lo escribe centrado en el sitio
; del punto, en la y EBX, si cabe
.largo:
        xor ecx, ecx
.lg:
        cmp byte [esi+ecx], 0
        je .lgf
        inc ecx
        jmp .lg
.lgf:
        ret
.centra:
        lea eax, [ecx*8]
        mov edx, [gp_bw]
        sub edx, eax
        js .cx
        sar edx, 1
        mov eax, [gp_x]
        add eax, edx
        HV dc_texto_n
.cx:
        ret

; ---- los sectores: cada cantidad positiva, su parte del círculo ----
gr_sectores:
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
        cmp ecx, [gr_n]
        jb .t
        mov [gp_rango], eax
        test eax, eax
        jz gr_pintar.sin
        xor ecx, ecx
.e:
        mov eax, [gr_lim+ecx*4]
        mov ebx, 1024
        push ecx
        mov ecx, [gp_rango]
        call gr_regla
        pop ecx
        mov [gr_lim+ecx*4], eax
        inc ecx
        cmp ecx, [gr_n]
        jb .e
        mov eax, [gp_al]            ; el radio: lo que quepa, hasta GR_R
        sub eax, GR_ARR + 16
        sar eax, 1
        cmp eax, GR_R
        jle .r1
        mov eax, GR_R
.r1:
        cmp eax, 16
        jl gr_pintar.fin
        mov [gp_h], eax             ; [gp_h]=el radio
        lea ebx, [eax+eax+1]
        mov [gp_w], ebx             ; [gp_w]=el lado del mapa
        imul eax, eax
        mov [gp_bw], eax            ; [gp_bw]=el radio al cuadrado
        mov edi, gr_mapa
        xor ebx, ebx                ; EBX=la fila
.fy:
        xor ecx, ecx                ; ECX=la columna
.fx:
        mov eax, ecx
        sub eax, [gp_h]             ; EAX=dx
        mov edx, ebx
        sub edx, [gp_h]             ; EDX=dy
        push ebx
        push ecx
        mov esi, eax
        imul esi, eax
        push edx
        imul edx, edx
        add esi, edx
        pop edx
        mov bl, C_BLANCO
        cmp esi, [gp_bw]            ; fuera del círculo: en blanco
        ja .pon
        call .angulo                ; EAX=el ángulo, de 0 a 1023
        xor ecx, ecx
.s:
        cmp eax, [gr_lim+ecx*4]
        jb .sf
        inc ecx
        cmp ecx, [gr_n]
        jb .s
        dec ecx
.sf:
        and ecx, 7
        mov bl, [gr_colores+ecx]
.pon:
        mov [edi], bl
        inc edi
        pop ecx
        pop ebx
        inc ecx
        cmp ecx, [gp_w]
        jb .fx
        inc ebx
        cmp ebx, [gp_w]
        jb .fy
        mov eax, 20
        mov ebx, GR_ARR
        mov ecx, [gp_w]
        mov edx, ecx
        mov esi, gr_mapa
        HV dc_copiar
        ; la leyenda: el color, el nombre y el tanto por ciento
        mov dword [gp_i], 0
        mov dword [gp_y], GR_ARR
.ley:
        mov edi, [gp_i]
        cmp dword [gr_val+edi*4], 0
        jle .ls
        mov ecx, edi
        and ecx, 7
        movzx eax, byte [gr_colores+ecx]
        HV dc_color
        mov eax, [gp_w]
        add eax, 40
        mov ebx, [gp_y]
        add ebx, 2
        mov ecx, 12
        mov edx, 12
        HV dc_rect
        COLOR C_TINTA
        push edi
        mov esi, edi
        shl esi, 4
        add esi, gr_nom
        mov edi, pie_txt
        call hvl_cad
        mov ax, ': '
        stosw
        pop esi
        mov eax, [gr_val+esi*4]     ; el tanto por ciento, redondeado
        mov ebx, 200
        mov ecx, [gp_rango]
        call gr_regla
        inc eax
        shr eax, 1
        call hvl_num
        mov ax, '%'
        stosw
        mov eax, [gp_w]
        add eax, 58
        mov ebx, [gp_y]
        mov esi, pie_txt
        HV dc_texto
        add dword [gp_y], 18
.ls:
        inc dword [gp_i]
        mov eax, [gp_i]
        cmp eax, [gr_n]
        jb .ley
        jmp gr_pintar.fin
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
gr_tipo     resd 1                  ; 0 barras, 1 líneas, 2 sectores
gr_c1       resd 1                  ; el rango del que sale (en el orden de r_c1...)
gr_f1       resd 1
gr_c2       resd 1
gr_f2       resd 1
gr_n        resd 1                  ; cuántos puntos
gp_an       resd 1                  ; al pintar: la ventana,
gp_al       resd 1
gp_w        resd 1                  ; el sitio del dibujo,
gp_h        resd 1
gp_bw       resd 1
gp_rango    resd 1
gp_cero     resd 1
gp_i        resd 1
gp_x        resd 1
gp_y        resd 1
gp_px       resd 1
gp_py       resd 1
gr_val      resd GR_MAX             ; las cantidades, como enteros pequeños
gr_lim      resd GR_MAX             ; sectores: hasta qué ángulo llega cada uno
gr_v64      resd 2 * GR_MAX         ; las cantidades, como son
gr_nom      resb 16 * GR_MAX        ; los nombres
gr_txt      resb 16 * GR_MAX        ; las cantidades, como se ven
gr_mapa     resb (2 * GR_R + 1) * (2 * GR_R + 1)
        section .text
