; =============================================================================
;  temas.inc - los temas, los diseños de diapositiva y las plantillas de
;  presentación de Presentaciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  UN TEMA es el fondo de todas las diapositivas (un color liso o un
;  degradado tramado), un adorno geométrico sencillo y los colores y tipos de
;  letra del título y del texto, a juego; todo con la paleta de Hispa
;  Ventanas. El número del tema va en el primer octeto de [lam_extra] (el
;  segundo es la transición y el tercero, si los puntos de las listas salen de
;  uno en uno). El fondo lo pinta fondo_tema, que el motor llama antes de los
;  objetos ([lam_fondo_fn]); los colores y las letras se ponen en las cajas
;  según lo que hacen en la diapositiva (LO_PAPEL), con tema_colores: por eso
;  cambiar de tema cambia todas las diapositivas, y lo que el usuario ha
;  puesto por su cuenta (cajas sin papel) no se toca.
;
;  UN DISEÑO es una plantilla del editor de láminas (macro PLX): las cajas de
;  una diapositiva nueva, con su papel. Vacías, enseñan un texto que dice para
;  qué son (guia_texto, que el motor llama por [lam_guia_fn]).
;
;  UNA PLANTILLA DE PRESENTACIÓN («mazo») son varias diapositivas ya puestas:
;  de cada una, su diseño y los textos del título y de sus cajas.
; =============================================================================
; de cada tema (T_TAM octetos): el fondo, el adorno, los colores del título,
; del texto y del texto de ejemplo, los tipos de letra del título y del texto,
; y su nombre
T_FONDO     equ 0
T_ADORNO    equ 1
T_C_TIT     equ 2
T_C_TXT     equ 3
T_C_GUIA    equ 4
T_L_TIT     equ 5
T_L_TXT     equ 6
T_NOMBRE    equ 8
T_TAM       equ 12
N_TEMAS     equ 6
            align 4
temas       db C_BLANCO, 0, C_PETROLEO, C_TINTA, C_ARENA_OSC, 10, 9, 0
            dd t_tm0
            db C_TINTA, 1, C_CREMA, C_PIEDRA, C_ARENA_OSC, 10, 9, 0
            dd t_tm1
            db C_PETROLEO, 2, C_BLANCO, C_CREMA, C_TURQUESA, 10, 9, 0
            dd t_tm2
            db C_CREMA, 3, C_TERRACOTA, C_MARRON, C_ARENA_OSC, 2, 1, 0
            dd t_tm3
            db C_ARENA, 4, C_GRANATE, C_TINTA, C_ARENA_OSC, 2, 3, 0
            dd t_tm4
            db C_ANIL, 5, C_BLANCO, C_CREMA, C_TURQUESA, 10, 11, 0
            dd t_tm5
adornos     dd ad_claro, ad_oscuro, ad_petroleo, ad_terracota, ad_arena, ad_atardecer
t_tm0       db "Claro", 0
t_tm1       db "Oscuro", 0
t_tm2       db "Petróleo", 0
t_tm3       db "Terracota", 0
t_tm4       db "Arena", 0
t_tm5       db "Atardecer", 0

; -> ESI=el tema de la presentación
tema_ptr:
        movzx esi, byte [lam_extra]
        cmp esi, N_TEMAS
        jb .a
        xor esi, esi
.a:
        imul esi, T_TAM
        add esi, temas
        ret

; un rectángulo del fondo: color, x, y, ancho, alto (en puntos de la diapositiva)
%macro CAJA 5
        mov byte [lam_color], %1
        mov eax, %2
        mov ebx, %3
        mov ecx, %4
        mov edx, %5
        call lam_caja
%endmacro

; el fondo de una diapositiva (lo llama el motor: ECX=diapositiva)
fondo_tema:
        pushad
        call tema_ptr
        mov al, [esi+T_C_GUIA]      ; (y el color de los textos de ejemplo)
        mov [lam_guia_color], al
        mov al, [esi+T_FONDO]
        cmp al, C_BLANCO            ; (el blanco ya está puesto)
        je .ad
        mov [lam_color], al
        xor eax, eax
        xor ebx, ebx
        mov ecx, [lam_dst_an]
        mov edx, [lam_dst_al]
        call lam_caja
.ad:
        movzx eax, byte [esi+T_ADORNO]
        call [adornos+eax*4]
        popad
        ret
; ---- los adornos: en las orillas, para no estorbar a las cajas ----
ad_claro:                           ; una franja abajo, con un tramo ocre
        CAJA C_PETROLEO, 0, 572, 800, 28
        CAJA C_OCRE, 0, 572, 120, 28
        ret
ad_oscuro:                          ; una banda terracota a la izquierda, con su filo
        CAJA C_TERRACOTA, 0, 0, 22, 600
        CAJA C_OCRE, 22, 0, 4, 600
        ret
ad_petroleo:                        ; una franja más clara abajo, y un bloque ocre
        CAJA C_PETROLEO_CL, 0, 560, 800, 40
        CAJA C_OCRE, 640, 560, 160, 40
        CAJA C_TURQUESA, 620, 560, 12, 40
        ret
ad_terracota:                       ; una banda a la izquierda y una raya abajo
        CAJA C_TERRACOTA, 0, 0, 26, 600
        CAJA C_TERRACOTA_CL, 26, 578, 774, 6
        CAJA C_OCRE, 0, 540, 26, 60
        ret
ad_arena:                           ; un marco doble
        CAJA C_MARRON, 10, 10, 780, 3
        CAJA C_MARRON, 10, 587, 780, 3
        CAJA C_MARRON, 10, 10, 3, 580
        CAJA C_MARRON, 787, 10, 3, 580
        CAJA C_GRANATE, 17, 17, 766, 1
        CAJA C_GRANATE, 17, 582, 766, 1
        CAJA C_GRANATE, 17, 17, 1, 566
        CAJA C_GRANATE, 782, 17, 1, 566
        ret
ad_atardecer:
        ; un degradado tramado, de añil arriba a petróleo abajo: en cada fila,
        ; más puntos del segundo color (la trama de 4x4 de la impresora)
        pushad
        xor ebx, ebx                ; la fila
.f:
        cmp ebx, [lam_dst_al]
        jae .raya
        mov eax, ebx                ; cuánto del segundo color: de 0 a 16
        imul eax, 17
        xor edx, edx
        div dword [lam_dst_al]
        mov ebp, eax
        mov edi, ebx
        imul edi, [lam_dst_an]
        add edi, [lam_dst]
        mov esi, ebx
        and esi, 3
        shl esi, 2
        xor ecx, ecx
.c:
        cmp ecx, [lam_dst_an]
        jae .sf
        mov eax, ecx
        and eax, 3
        movzx eax, byte [tramado+esi+eax]
        shr eax, 4
        cmp eax, ebp
        mov al, C_ANIL
        jae .p
        mov al, C_PETROLEO
.p:
        mov [edi+ecx], al
        inc ecx
        jmp .c
.sf:
        inc ebx
        jmp .f
.raya:
        popad
        CAJA C_OCRE, 0, 586, 800, 4
        ret

; el texto de ejemplo de una caja vacía (lo llama el motor: EAX=su papel)
guia_texto:
        xor esi, esi
        cmp eax, LP_IMAGEN
        ja .x
        mov esi, [t_guias+eax*4]
.x:
        ret

; pone a las cajas de todas las diapositivas los colores y las letras del
; tema, según su papel (las que no tienen papel son del usuario: no se tocan)
tema_colores:
        pushad
        call tema_ptr
        mov ebx, esi
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        movzx ecx, byte [esi+LO_PAPEL]
        cmp ecx, LP_TITULO
        jne .n1
        mov dl, [ebx+T_C_TIT]
        mov [esi+LO_TINTA], dl
        mov dl, [ebx+T_L_TIT]
        mov [esi+LO_LETRA], dl
        jmp .s
.n1:
        cmp ecx, LP_IMAGEN
        jne .n2
        mov dl, [ebx+T_C_GUIA]      ; el hueco de una imagen: su marco
        mov [esi+LO_BORDE], dl
        jmp .s
.n2:
        cmp ecx, LP_CUERPO
        jb .s
        cmp ecx, LP_SUBTIT
        ja .s
        mov dl, [ebx+T_C_TXT]
        mov [esi+LO_TINTA], dl
        mov dl, [ebx+T_L_TXT]
        mov [esi+LO_LETRA], dl
.s:
        inc eax
        jmp .b
.x:
        popad
        ret

; ---- Formato > Tema de la presentación ----
tema_elige:
        mov dword [le_titulo], t_d_tema
        mov dword [le_n], N_TEMAS
        mov dword [le_texto], .texto
        movzx eax, byte [lam_extra]
        mov [le_sel], eax
        call lista_elige
        jne .x
        call fin_edicion
        call instantanea
        mov al, [le_sel]
        mov [lam_extra], al
        call tema_colores
        mov byte [ed_todo], 1
        call cambio
.x:
        ret
.texto:
        imul eax, T_TAM
        mov esi, [temas+eax+T_NOMBRE]
        ret
; ---- Presentación > Transición ----
trans_elige:
        mov dword [le_titulo], t_d_trans
        mov dword [le_n], N_TRANS
        mov dword [le_texto], .texto
        movzx eax, byte [lam_extra+1]
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov al, [le_sel]
        mov [lam_extra+1], al
        mov byte [modif], 1
        call inval
.x:
        ret
.texto:
        mov esi, [t_trans+eax*4]
        ret
; ---- Presentación > Puntos de las listas de uno en uno ----
pasos_orden:
        xor byte [lam_extra+2], 1
        mov byte [modif], 1
        jmp inval

; =============================================================================
;  Los diseños (las medidas, en milésimas de la diapositiva; los colores y
;  las letras los pone luego el tema). PLX: tipo, x, y, ancho, alto, estilo,
;  alineación, letra, relleno, borde, grosor, tipo de letra, tamaño (dos
;  veces), texto, papel y banderas
; =============================================================================
V           equ LB_VINETAS
ds_portada:
        PLX LT_TEXTO, 80, 290, 840, 190, 0, 1, 0, N, N, 1, 10, 5, 5, 0, LP_TITULO, 0
        PLX LT_TEXTO, 80, 500, 840, 120, 0, 1, 0, N, N, 1, 9, 3, 3, 0, LP_SUBTIT, 0
        db 0
ds_texto:
        PLX LT_TEXTO, 50, 35, 900, 170, 0, 0, 0, N, N, 1, 10, 4, 4, 0, LP_TITULO, 0
        PLX LT_TEXTO, 50, 220, 900, 700, 0, 0, 0, N, N, 1, 9, 3, 3, 0, LP_CUERPO, V
        db 0
ds_columnas:
        PLX LT_TEXTO, 50, 35, 900, 170, 0, 0, 0, N, N, 1, 10, 4, 4, 0, LP_TITULO, 0
        PLX LT_TEXTO, 50, 220, 440, 700, 0, 0, 0, N, N, 1, 9, 2, 2, 0, LP_CUERPO, V
        PLX LT_TEXTO, 510, 220, 440, 700, 0, 0, 0, N, N, 1, 9, 2, 2, 0, LP_CUERPO2, V
        db 0
ds_titulo:
        PLX LT_TEXTO, 50, 35, 900, 170, 0, 0, 0, N, N, 1, 10, 4, 4, 0, LP_TITULO, 0
ds_blanco:
        db 0
ds_imagen:
        PLX LT_TEXTO, 50, 35, 900, 170, 0, 0, 0, N, N, 1, 10, 4, 4, 0, LP_TITULO, 0
        PLX LT_TEXTO, 50, 220, 440, 700, 0, 0, 0, N, N, 1, 9, 2, 2, 0, LP_CUERPO, V
        PLX LT_TEXTO, 510, 220, 440, 700, 0, 1, C_ARENA_OSC, N, C_ARENA_OSC, 1, 9, 0, 0, 0, LP_IMAGEN, LB_HUECO
        db 0

; =============================================================================
;  Las plantillas de presentación. De cada diapositiva: su diseño (FFh: se
;  acabó) y los textos del título, de la primera caja (el texto o el
;  subtítulo) y de la segunda columna (0: se deja vacía)
; =============================================================================
%macro DIA 4
        db %1
        dd %2, %3, %4
%endmacro
DIA_TAM     equ 13
mz_blanco:
        DIA 0, 0, 0, 0
        db 0xFF
mz_asoc:
        DIA 0, ma_1t, ma_1s, 0
        DIA 1, ma_2t, ma_2c, 0
        DIA 2, ma_3t, ma_3a, ma_3b
        DIA 1, ma_4t, ma_4c, 0
        DIA 0, ma_5t, ma_5s, 0
        db 0xFF
ma_1t       db "Asociación de vecinos", 0
ma_1s       db "Memoria del año", 0
ma_2t       db "Quiénes somos", 0
ma_2c       db "Una asociación sin ánimo de lucro", 10
            db "Nacida en el barrio hace veinte años", 10
            db "Abierta a todo el que quiera participar", 0
ma_3t       db "Lo que hemos hecho", 0
ma_3a       db "Talleres y cursos", 10, 9, "Pintura", 10, 9, "Informática", 10
            db "Excursiones", 10, "Fiestas del barrio", 0
ma_3b       db "Charlas", 10, "Biblioteca", 10, 9, "Préstamo de libros", 10
            db "Banco de tiempo", 0
ma_4t       db "Las cuentas", 0
ma_4c       db "Ingresos: cuotas y subvenciones", 10
            db "Gastos: actividades y local", 10
            db "El año se cierra sin deudas", 0
ma_5t       db "Gracias", 0
ma_5s       db "¿Alguna pregunta?", 0
mz_informe:
        DIA 0, mi_1t, mi_1s, 0
        DIA 1, mi_2t, mi_2c, 0
        DIA 2, mi_3t, mi_3a, mi_3b
        DIA 1, mi_4t, mi_4c, 0
        db 0xFF
mi_1t       db "Informe del trimestre", 0
mi_1s       db "Resultados y previsiones", 0
mi_2t       db "Resumen", 0
mi_2c       db "Lo más importante, en una frase", 10
            db "El dato que lo demuestra", 10
            db "Lo que hay que decidir hoy", 0
mi_3t       db "Los datos", 0
mi_3a       db "Lo que ha ido bien", 10, 9, "Primer logro", 10, 9, "Segundo logro", 0
mi_3b       db "Lo que hay que mejorar", 10, 9, "Primer problema", 10, 9, "Segundo problema", 0
mi_4t       db "Conclusiones", 0
mi_4c       db "Qué proponemos", 10, "Quién se encarga", 10, "Para cuándo", 0
mz_clase:
        DIA 0, mc_1t, mc_1s, 0
        DIA 1, mc_2t, mc_2c, 0
        DIA 2, mc_3t, mc_3a, mc_3b
        DIA 1, mc_4t, mc_4c, 0
        db 0xFF
mc_1t       db "El ciclo del agua", 0
mc_1s       db "Ciencias de la naturaleza", 0
mc_2t       db "Qué vamos a aprender", 0
mc_2c       db "De dónde viene la lluvia", 10
            db "Por qué los ríos no se acaban", 10
            db "Qué podemos hacer para cuidar el agua", 0
mc_3t       db "Las fases", 0
mc_3a       db "Evaporación", 10, 9, "El sol calienta el mar", 10, "Condensación", 10, 9, "Se forman las nubes", 0
mc_3b       db "Precipitación", 10, 9, "Lluvia, nieve o granizo", 10, "Escorrentía", 10, 9, "El agua vuelve al mar", 0
mc_4t       db "Actividades", 0
mc_4c       db "Dibuja el ciclo en tu cuaderno", 10
            db "Explica una fase con tus palabras", 10
            db "Trae una noticia sobre el agua", 0

; ESI=plantilla de presentación: pone sus diapositivas en el documento recién
; hecho (que tiene una, vacía) con los colores del tema
mazo_pon:
        pushad
        mov ebx, esi
        xor edi, edi                ; EDI=la diapositiva
.b:
        movzx eax, byte [ebx]
        cmp eax, 0xFF
        je .x
        test edi, edi
        jz .p
        lea ecx, [edi-1]
        call lam_pag_nueva
        jc .x
.p:
        mov esi, [disenos+eax*4]
        mov ecx, edi
        call plantilla_pon
        mov al, LP_TITULO
        mov esi, [ebx+1]
        call .texto
        mov al, LP_CUERPO           ; el segundo texto es del cuerpo o, en una
        mov esi, [ebx+5]            ; portada, del subtítulo
        call .texto
        mov al, LP_SUBTIT
        call .texto
        mov al, LP_CUERPO2
        mov esi, [ebx+9]
        call .texto
        add ebx, DIA_TAM
        inc edi
        jmp .b
.x:
        call tema_colores
        popad
        ret
; (interna) pone el texto ESI (0: ninguno) a la caja de papel AL de la
; diapositiva EDI, si la tiene
.texto:
        pushad
        test esi, esi
        jz .tx
        mov ebp, esi
        mov dl, al
        xor eax, eax
.t1:
        cmp eax, [lam_n]
        jae .tx
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, edi
        jne .t2
        cmp [esi+LO_PAPEL], dl
        je .t3
.t2:
        inc eax
        jmp .t1
.t3:
        xor edx, edx
.t4:
        cmp byte [ebp+edx], 0
        je .t5
        inc edx
        jmp .t4
.t5:
        mov esi, ebp
        xor ebx, ebx
        xor ecx, ecx
        call lam_dat_cambia
.tx:
        popad
        ret

; ---- Archivo > Nuevo: se elige la plantilla y el tema ----
nuevo:
        call preguntar
        jc .x
        mov ebp, [ven]
        mov esi, dlg_nuevo
        mov edi, dlg_nuevo + P_TAM
        mov ebx, nuevo_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, [dl_plant]
        mov ebx, [dl_papel]
        call pre_nueva
.x:
        ret
nuevo_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 60                  ; una de las listas: doble clic o Intro
        je .lista
        cmp cx, 61
        jne .boton
.lista:
        shr ecx, 16
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        HV msg_enviar
        jmp .no
.boton:
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 60
        mov edi, dl_plant
        call .lee
        mov ecx, 61
        mov edi, dl_papel
        call .lee
        popad
        jmp .no
.ini:
        pushad
        mov dword [dl_plant], 0
        mov dword [dl_papel], 0
        mov ecx, 60
        mov esi, t_mazos
        mov edi, N_MAZOS
        mov ebx, 4
        call .llena
        mov ecx, 61
        mov esi, temas + T_NOMBRE
        mov edi, N_TEMAS
        mov ebx, T_TAM
        call .llena
        popad
.no:
        xor eax, eax
        ret
; (internas) la lista ECX del diálogo EBP: su elegido, a [EDI]; y llenarla
; con EDI textos cuyos punteros están en ESI (cada EBX octetos)
.lee:
        push ebp
        HV ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        HV msg_enviar
        pop ebp
        test eax, eax
        js .l1
        mov [edi], eax
.l1:
        ret
.llena:
        push ebp
        HV ven_hija_id
        mov ebp, eax
.ll1:
        mov edx, [esi]
        mov eax, LM_ANADIR
        HV msg_enviar
        add esi, ebx
        dec edi
        jnz .ll1
        mov eax, LM_PONER_SELEC
        xor ecx, ecx
        HV msg_enviar
        pop ebp
        ret
