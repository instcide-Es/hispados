; =============================================================================
;  contenido.inc - Lector: el contenido de una página (lo que hay que dibujar)
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El contenido de una página de un PDF es un programita: números y nombres
;  que se van apilando y órdenes de una a tres letras que los gastan («BT»,
;  «/F1 12 Tf», «72 700 Td», «(Hola) Tj», «0 0 100 50 re f»...). Aquí se
;  ejecuta y de él sale una lista de cosas que pintar (it_tab, IT_*), ya en
;  puntos de la página con el origen arriba a la izquierda y la «y» hacia
;  abajo, en coma fija 16.16 y en el orden en que hay que pintarlas:
;
;    ITT_TEXTO     un trozo de texto (en la página 437): dónde empieza su
;                  línea base, cuánto ocupa de ancho según el PDF, el tamaño,
;                  el tipo nuestro y el color
;    ITT_RECT      un rectángulo relleno
;    ITT_LINEA     una raya, con su grosor
;    ITT_POLI      una figura rellena que no es un rectángulo (sus vértices)
;    ITT_IMAGEN    el sitio de una imagen (no se sabe pintar: un recuadro)
;    ITT_ILEGIBLE  el sitio de un texto que no se puede leer
;
;  y, en [pg_avisos], lo que no se ha podido hacer (AV_*).
;
;    pdf_pagina   EAX=la página (desde 0): deja su lista, [pg_an] y [pg_al]
;
;  Órdenes que entiende: q Q cm w; g G rg RG k K sc scn SC SCN; m l c v y h re;
;  S s f F f* B B* b b* n; BT ET Tf Td TD Tm T* Tj TJ ' " Tc Tw Tz TL Ts Tr; Do
;  (imágenes y formularios, que son contenido dentro del contenido), BI (una
;  imagen metida en el contenido) y W, W* (el recorte, si es un rectángulo
;  derecho: cada elemento lleva en IT_CLIP el suyo, de cl_tab). De cada imagen
;  se hace una miniatura (imagen.inc) que va en IT_TXT (0: no se sabe pintar);
;  en IT_BAND, cómo cae (II_*). Los degradados (sh) y lo demás se pasa por
;  alto. Las curvas se hacen con ocho rayas.
;
;  Los trozos de texto seguidos de una misma línea se juntan en uno (así un
;  texto escrito letra a letra queda entero, para buscar en él y guardarlo).
; =============================================================================
MAX_IT      equ 6000
IT_TAM      equ 32
IT_TIPO     equ 0
IT_COLOR    equ 1
IT_LETRA    equ 2                   ; el tipo nuestro (tipos.inc)
IT_BAND     equ 3                   ; IB_* y, en los bits 4-5, hacia dónde va
IT_X        equ 4
IT_Y        equ 8
IT_AN       equ 12                  ; ancho (raya: la x del otro extremo)
IT_AL       equ 16                  ; alto; texto: tamaño (raya: la y del otro extremo)
IT_TXT      equ 20                  ; texto: sus letras (raya: grosor; figura: vértices)
IT_N        equ 24                  ; (dw) cuántas letras (figura: cuántos trozos)
IT_FU       equ 26                  ; (dw) la fuente del PDF de donde sale
IT_CLIP     equ 28                  ; (dw) su recorte: un rectángulo de cl_tab (0: ninguno)
II_ESPEJO   equ 1                   ; imagen (en IT_BAND): columnas al revés
II_VUELTA   equ 2                   ; filas al revés
II_GIRADA   equ 4                   ; las columnas van de arriba abajo
MAX_CLIP    equ 255
ITT_TEXTO   equ 1
ITT_RECT    equ 2
ITT_LINEA   equ 3
ITT_IMAGEN  equ 4
ITT_ILEGIBLE equ 5
ITT_POLI    equ 6
IB_NEGRA    equ 1
IB_CURSIVA  equ 2
IB_GIRO     equ 0x30                ; 0 normal, 1 hacia abajo, 2 cabeza abajo, 3 hacia arriba

AV_ILEGIBLE equ 1                   ; hay texto con letra de codificación propia
AV_DESCON   equ 2                   ; hay letras que no existen en la 437 («?»)
AV_FILTRO   equ 4                   ; el contenido usa un filtro que no se sabe
AV_ROTO     equ 8                   ; el contenido está estropeado o cortado
AV_LLENA    equ 16                  ; no cabe todo: se enseña una parte
AV_IMAGEN   equ 32                  ; hay imágenes (van como recuadros)
AV_MEMORIA  equ 64                  ; no hay memoria para descomprimirlo

POOL_TXT    equ 128 * 1024          ; letras de una página
MAX_PT      equ 6000                ; vértices del trazado en curso
PT_TAM      equ 12                  ; dd x, y, clase (0 empieza trozo, 1 raya)
POOL_POLI   equ 160 * 1024          ; vértices de las figuras rellenas
CAD_TAM     equ 4096                ; una cadena de texto, a lo más
GS_PILA     equ 40
MAX_OPS     equ 400000              ; órdenes de una página (una de verdad no llega a 100.000)
MAX_FORMAS  equ 600                 ; formularios pintados en una página

; el estado de dibujo (lo que guardan «q» y «Q»)
G_CTM       equ 0                   ; 6 dd
G_REL       equ 24                  ; color de relleno
G_TRZ       equ 25                  ; color de trazo
G_GROS      equ 28
G_TC        equ 32
G_TW        equ 36
G_TZ        equ 40
G_TL        equ 44
G_TS        equ 48
G_FU        equ 52                  ; la ficha de la fuente (0: ninguna)
G_TF        equ 56                  ; el tamaño
G_CLIP      equ 60                  ; el recorte en vigor (número en cl_tab)
G_TAM       equ 64

%macro CONTENIDO_BSS 0
            alignb 4
pg_an       resd 1                  ; el tamaño de la página, en puntos (16.16)
pg_al       resd 1
pg_avisos   resd 1
it_n        resd 1
tx_tope     resd 1
po_tope     resd 1
ci_res      resd 2                  ; los recursos en vigor
ci_recorta  resd 1                  ; se ha pedido «W»: el trazado, al acabar, recorta
cl_n        resd 1
cl_tab      resd (MAX_CLIP + 1) * 4 ; los recortes: x0, y0, x1, y1 (en la página)
ci_ops      resd 1                  ; órdenes que aún se pueden ejecutar en esta página
ci_formas   resd 1                  ; y formularios
pg_filtro   resb 32                 ; el filtro desconocido de AV_FILTRO
ci_prof     resd 1
ci_lim      resd 1
ci_salto    resd 1
ci_bi       resd 1
ci_mt       resd 6
ci_m        resd 6
ci_tm       resd 6                  ; la matriz del texto
ci_tlm      resd 6                  ; y la del principio de la línea
ci_x        resd 1
ci_y        resd 1
ci_xs       resd 1
ci_tam      resd 1
ci_av       resd 1
ci_giro     resd 1
ci_nsal     resd 1
ci_hay      resd 1
ci_tj       resd 2
op_n        resd 1
gs_n        resd 1
pt_n        resd 1
pt_ini      resd 2                  ; donde empieza el trozo (para cerrarlo)
pt_act      resd 2                  ; el último punto (ya en la página)
bz          resd 8
eg          resb G_TAM
gs_pila     resb G_TAM * GS_PILA
op_t        resb 36
            alignb 4
op_v        resd 36
ci_nom      resb 68
ci_cad      resb CAD_TAM + 4
ci_sal      resb CAD_TAM * 3 + 8
            alignb 4
pt_tab      resb MAX_PT * PT_TAM
it_tab      resb MAX_IT * IT_TAM
tx_pool     resb POOL_TXT
po_pool     resb POOL_POLI
%endmacro

k_contents  db "Contents", 0
k_font      db "Font", 0
k_xobject   db "XObject", 0
k_matrix    db "Matrix", 0
n_image     db "Image", 0
n_form      db "Form", 0
p_id        db "ID", 0
f_f1        db "F", 0
; los pesos de una curva de Bézier en t = 1/8 ... 8/8 (suman 512)
bz_pesos    dw 343, 147, 21, 1,   216, 216, 72, 8,   125, 225, 135, 27,   64, 192, 192, 64
            dw 27, 135, 225, 125,   8, 72, 216, 216,   1, 21, 147, 343,   0, 0, 0, 512
            align 4
; las órdenes: su nombre (hasta cuatro letras) y quién la hace
ci_ordenes:
        dd 'q', o_q,   'Q', o_Q,   'cm', o_cm,   'w', o_w
        dd 'g', o_g,   'G', o_G,   'rg', o_rg,   'RG', o_RG,   'k', o_k,   'K', o_K
        dd 'sc', o_sc,   'scn', o_sc,   'SC', o_SC,   'SCN', o_SC
        dd 'm', o_m,   'l', o_l,   'c', o_c,   'v', o_v,   'y', o_y,   'h', o_h,   're', o_re
        dd 'S', o_S,   's', o_s,   'f', o_f,   'F', o_f,   'f*', o_f
        dd 'B', o_B,   'B*', o_B,   'b', o_b,   'b*', o_b,   'n', o_n
        dd 'BT', o_BT,   'Tf', o_Tf,   'Td', o_Td,   'TD', o_TD,   'Tm', o_Tm,   'T*', o_Tast
        dd 'Tj', o_Tj,   'TJ', o_TJ,   0x27, o_comilla,   0x22, o_comillas
        dd 'Tc', o_Tc,   'Tw', o_Tw,   'Tz', o_Tz,   'TL', o_TL,   'Ts', o_Ts
        dd 'Do', o_Do,   'BI', o_BI,   'W', o_W,   'W*', o_W
        dd 0

; EAX x EDX (16.16) -> EAX
fmul:
        push edx
        imul edx
        shrd eax, edx, 16
        pop edx
        ret

%macro FM 2
        mov eax, %1
        mov edx, %2
        call fmul
%endmacro

; ESI=matriz A, EDI=matriz B, EBX=dónde: A por B (primero A, luego B)
mat_mul:
        push eax
        push ecx
        push edx
        FM [esi], [edi]
        mov ecx, eax
        FM [esi+4], [edi+8]
        add eax, ecx
        mov [ci_mt], eax
        FM [esi], [edi+4]
        mov ecx, eax
        FM [esi+4], [edi+12]
        add eax, ecx
        mov [ci_mt+4], eax
        FM [esi+8], [edi]
        mov ecx, eax
        FM [esi+12], [edi+8]
        add eax, ecx
        mov [ci_mt+8], eax
        FM [esi+8], [edi+4]
        mov ecx, eax
        FM [esi+12], [edi+12]
        add eax, ecx
        mov [ci_mt+12], eax
        FM [esi+16], [edi]
        mov ecx, eax
        FM [esi+20], [edi+8]
        add eax, ecx
        add eax, [edi+16]
        mov [ci_mt+16], eax
        FM [esi+16], [edi+4]
        mov ecx, eax
        FM [esi+20], [edi+12]
        add eax, ecx
        add eax, [edi+20]
        mov [ci_mt+20], eax
        mov ecx, 5
.c:
        mov eax, [ci_mt+ecx*4]
        mov [ebx+ecx*4], eax
        dec ecx
        jns .c
        pop edx
        pop ecx
        pop eax
        ret

; EAX=x EBX=y (del usuario) -> EAX, EBX en la página (con la matriz en vigor)
ci_punto:
        push ecx
        push edx
        push esi
        push edi
        mov esi, eax
        mov edi, ebx
        FM esi, [eg+G_CTM]
        mov ecx, eax
        FM edi, [eg+G_CTM+8]
        add ecx, eax
        add ecx, [eg+G_CTM+16]
        push ecx
        FM esi, [eg+G_CTM+4]
        mov ecx, eax
        FM edi, [eg+G_CTM+12]
        add ecx, eax
        add ecx, [eg+G_CTM+20]
        mov ebx, ecx
        pop eax
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

; EAX=rojo EBX=verde ECX=azul (16.16, de 0 a 1) -> AL=el color más cercano de
; la paleta de Hispa Ventanas: el cubo de 6x6x6 (16-231) o, si es un gris, la
; escala de grises (232-255); el negro es el 16 y el blanco el 231
col_rgb:
        push ebx
        push ecx
        push edx
        push esi
        push edi
%macro TOPE 1
        test %1, %1
        jns %%a
        xor %1, %1
%%a:
        cmp %1, 0x10000
        jle %%b
        mov %1, 0x10000
%%b:
%endmacro
        TOPE eax
        TOPE ebx
        TOPE ecx
        mov esi, eax                ; el mayor y el menor
        mov edi, eax
        cmp ebx, esi
        jle .m1
        mov esi, ebx
.m1:
        cmp ecx, esi
        jle .m2
        mov esi, ecx
.m2:
        cmp ebx, edi
        jge .m3
        mov edi, ebx
.m3:
        cmp ecx, edi
        jge .m4
        mov edi, ecx
.m4:
        sub esi, edi
        cmp esi, 0x0A00
        ja .cubo
        add eax, ebx                ; un gris: de 0 a 255
        add eax, ecx
        xor edx, edx
        mov ebx, 3
        div ebx
        imul eax, 255
        shr eax, 16
        cmp eax, 248
        jae .blanco
        cmp eax, 5
        jbe .negro
        add eax, 5
        xor edx, edx
        mov ebx, 11
        div ebx
        cmp eax, 23
        jbe .g1
        mov eax, 23
.g1:
        add eax, 232
        jmp .x
.blanco:
        mov eax, 231
        jmp .x
.negro:
        mov eax, 16
        jmp .x
.cubo:
        lea eax, [eax+eax*4]        ; cada uno, de 0 a 5
        add eax, 0x8000
        shr eax, 16
        lea ebx, [ebx+ebx*4]
        add ebx, 0x8000
        shr ebx, 16
        lea ecx, [ecx+ecx*4]
        add ecx, 0x8000
        shr ecx, 16
        imul eax, 36
        imul ebx, 6
        add eax, ebx
        add eax, ecx
        add eax, 16
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; -----------------------------------------------------------------------------
;  La lista de lo que hay que pintar
; -----------------------------------------------------------------------------
; -> EDI=un elemento nuevo, a cero; CF=1 si no caben más (y queda avisado)
it_nuevo:
        push eax
        push ecx
        mov eax, [it_n]
        cmp eax, MAX_IT
        jae .no
        inc dword [it_n]
        shl eax, 5
        lea edi, [it_tab+eax]
        push edi
        xor eax, eax
        mov ecx, IT_TAM / 4
        rep stosd
        pop edi
        mov eax, [eg+G_CLIP]
        mov [edi+IT_CLIP], ax
        pop ecx
        pop eax
        clc
        ret
.no:
        or dword [pg_avisos], AV_LLENA
        pop ecx
        pop eax
        stc
        ret

; -----------------------------------------------------------------------------
;  Una página
; -----------------------------------------------------------------------------
pdf_pagina:
        pushad
        cld
        xor ebx, ebx
        mov [it_n], ebx
        mov [pg_avisos], ebx
        mov [ci_prof], ebx
        mov [gs_n], ebx
        mov [op_n], ebx
        mov [pt_n], ebx
        mov [px_prof], ebx
        mov [ci_salto], ebx
        mov [pg_filtro], bl
        mov [ci_recorta], ebx
        mov [cl_n], ebx
        mov eax, [ar_fin0]          ; (las imágenes de la página anterior, fuera)
        mov [ar_fin], eax
        mov eax, [esp+28]
        mov dword [ci_ops], MAX_OPS
        mov dword [ci_formas], MAX_FORMAS
        mov dword [tx_tope], tx_pool
        mov dword [po_tope], po_pool
        mov dword [pg_an], 612 << 16
        mov dword [pg_al], 792 << 16
        call fu_vacia
        push eax
        mov eax, [ar_fijo]
        call ar_suelta
        pop eax
        cmp eax, [pdf_npag]
        jae .x
        imul eax, PG_TAM
        lea ebx, [pg_tab+eax]       ; EBX=la ficha de la página
        ; la caja y el giro: la matriz que lleva del PDF (origen abajo a la
        ; izquierda de la caja, y hacia arriba) a la página (arriba, y hacia abajo)
        mov edi, eg
        mov ecx, G_TAM / 4
        xor eax, eax
        rep stosd
        mov eax, [ebx+PG_CAJA+8]
        sub eax, [ebx+PG_CAJA]
        mov [pg_an], eax
        mov eax, [ebx+PG_CAJA+12]
        sub eax, [ebx+PG_CAJA+4]
        mov [pg_al], eax
        mov eax, [ebx+PG_GIRO]      ; el giro, en cuartos de vuelta (0-3)
        cdq
        mov ecx, 90
        idiv ecx
        and eax, 3
        cmp eax, 1
        je .g90
        cmp eax, 2
        je .g180
        cmp eax, 3
        je .g270
        mov dword [eg+G_CTM], 0x10000       ; x' = x - x0;  y' = y1 - y
        mov dword [eg+G_CTM+12], -0x10000
        mov eax, [ebx+PG_CAJA]
        neg eax
        mov [eg+G_CTM+16], eax
        mov eax, [ebx+PG_CAJA+12]
        mov [eg+G_CTM+20], eax
        jmp .ctm
.g90:
        mov dword [eg+G_CTM+4], 0x10000     ; x' = y - y0;  y' = x - x0
        mov dword [eg+G_CTM+8], 0x10000
        mov eax, [ebx+PG_CAJA+4]
        neg eax
        mov [eg+G_CTM+16], eax
        mov eax, [ebx+PG_CAJA]
        neg eax
        mov [eg+G_CTM+20], eax
        jmp .gira
.g180:
        mov dword [eg+G_CTM], -0x10000      ; x' = x1 - x;  y' = y - y0
        mov dword [eg+G_CTM+12], 0x10000
        mov eax, [ebx+PG_CAJA+8]
        mov [eg+G_CTM+16], eax
        mov eax, [ebx+PG_CAJA+4]
        neg eax
        mov [eg+G_CTM+20], eax
        jmp .ctm
.g270:
        mov dword [eg+G_CTM+4], -0x10000    ; x' = y1 - y;  y' = x1 - x
        mov dword [eg+G_CTM+8], -0x10000
        mov eax, [ebx+PG_CAJA+12]
        mov [eg+G_CTM+16], eax
        mov eax, [ebx+PG_CAJA+8]
        mov [eg+G_CTM+20], eax
.gira:
        mov eax, [pg_an]
        xchg eax, [pg_al]
        mov [pg_an], eax
.ctm:
        mov byte [eg+G_REL], 16     ; negro
        mov byte [eg+G_TRZ], 16
        mov dword [eg+G_GROS], 0x10000
        mov dword [eg+G_TZ], 0x10000
        call o_BT                   ; (las matrices del texto, a la unidad)
        mov eax, [ebx+PG_RES]
        mov [ci_res], eax
        mov eax, [ebx+PG_RES+4]
        mov [ci_res+4], eax
        ; el contenido: un flujo o una lista de flujos, que van seguidos
        mov esi, [ebx+PG_DIC]
        mov ebp, [ebx+PG_DIC+4]
        mov edi, k_contents
        call px_dic
        jc .x
        call px_ref
        cmp esi, ebp
        jae .x
        cmp byte [esi], '['
        je .lista
        call .flujo
        jmp .x
.lista:
        inc esi
.l:
        call px_blancos
        cmp esi, ebp
        jae .x
        cmp byte [esi], ']'
        je .x
        push esi
        push ebp
        call px_ref
        call .flujo
        pop ebp
        pop esi
        call px_salta_valor
        jmp .l
.x:
        popad
        ret
; (interna) ESI, EBP=un flujo de contenido: lo descomprime y lo ejecuta
.flujo:
        call pdf_flujo
        jc .f_mal
        cmp dword [fl_parcial], 0
        je .f1
        or dword [pg_avisos], AV_ROTO
.f1:
        mov esi, eax
        lea ebp, [eax+ecx]
        call ci_ejecuta
        ret
.f_mal:
        call ci_aviso_flujo
        ret

; (interna) un flujo de contenido que no se ha podido leer: el aviso que toca
ci_aviso_flujo:
        push eax
        push ecx
        mov eax, [fl_error]
        cmp eax, FE_NOFLUJO         ; (un contenido que no es un flujo: nada)
        je .x
        mov ecx, AV_FILTRO
        cmp eax, FE_FILTRO
        jne .otro
        cmp byte [pg_filtro], 0     ; su nombre, para decirlo
        jne .pon
        push esi
        push edi
        push ecx
        mov esi, fl_filtro
        mov edi, pg_filtro
        mov ecx, 32
        rep movsb
        pop ecx
        pop edi
        pop esi
        jmp .pon
.otro:
        mov ecx, AV_MEMORIA
        cmp eax, FE_MEMORIA
        je .pon
        mov ecx, AV_ROTO
.pon:
        or [pg_avisos], ecx
.x:
        pop ecx
        pop eax
        ret

; -----------------------------------------------------------------------------
;  El intérprete: ESI, EBP=el contenido
; -----------------------------------------------------------------------------
ci_ejecuta:
        pushad
.b:
        call px_blancos
        cmp esi, ebp
        jae .fin
        movzx eax, byte [esi]
        cmp al, '/'
        je .nombre
        cmp al, '('
        je .cadena
        cmp al, '<'
        je .menor
        cmp al, '['
        je .lista
        cmp byte [px_clase+eax], 2  ; un delimitador suelto
        jne .b1
        inc esi
        jmp .b
.b1:
        cmp al, '.'
        je .numero
        cmp al, '-'
        je .numero
        cmp al, '+'
        je .numero
        sub al, '0'
        cmp al, 9
        ja .orden
.numero:
        call px_num
        jc .raro
        xor ebx, ebx
        jmp .apila
.raro:
        call px_salta
        jmp .b
.nombre:
        mov eax, esi
        mov bl, 1
        call px_salta
        jmp .apila
.cadena:
        mov eax, esi
        mov bl, 2
        call px_salta
        jmp .apila
.menor:
        mov eax, esi
        mov bl, 2
        lea ecx, [esi+1]
        cmp ecx, ebp
        jae .m1
        cmp byte [ecx], '<'
        jne .m1
        mov bl, 4
.m1:
        call px_salta
        jmp .apila
.lista:
        mov eax, esi
        mov bl, 3
        call px_salta
.apila:
        mov ecx, [op_n]
        cmp ecx, 32                 ; demasiadas cosas sin orden: basura
        jb .a1
        xor ecx, ecx
.a1:
        mov [op_t+ecx], bl
        mov [op_v+ecx*4], eax
        inc ecx
        mov [op_n], ecx
        jmp .b
.orden:
        ; una palabra: hasta cuatro letras, empaquetadas
        xor eax, eax
        xor ecx, ecx
.o1:
        cmp esi, ebp
        jae .o3
        movzx ebx, byte [esi]
        cmp byte [px_clase+ebx], 0
        jne .o3
        inc esi
        cmp ecx, 32
        jae .o2
        shl ebx, cl
        or eax, ebx
.o2:
        add ecx, 8
        jmp .o1
.o3:
        cmp ecx, 32
        ja .hecha                   ; (más de cuatro letras: ninguna de las nuestras)
        mov edi, ci_ordenes
.o4:
        cmp dword [edi], 0
        je .hecha
        cmp eax, [edi]
        je .o5
        add edi, 8
        jmp .o4
.o5:
        dec dword [ci_ops]          ; un contenido sin fin (formularios que se
        js .agotado                 ; llaman unos a otros): hasta aquí
        push esi
        push ebp
        mov [ci_lim], ebp
        call [edi+4]
        pop ebp
        pop esi
        mov [ci_lim], ebp
        mov eax, [ci_salto]         ; (BI: la imagen va detrás, en crudo)
        test eax, eax
        jz .hecha
        mov esi, eax
        mov dword [ci_salto], 0
.hecha:
        mov dword [op_n], 0
        jmp .b
.agotado:
        or dword [pg_avisos], AV_LLENA
.fin:
        popad
        ret

; (interna) ECX=cuántos números quiere la orden -> EBX=dónde están (dd cada
; uno); CF=1 si no hay tantos
ci_args:
        mov ebx, [op_n]
        sub ebx, ecx
        jb .no
        lea ebx, [op_v+ebx*4]
        clc
        ret
.no:
        stc
        ret

o_q:
        mov eax, [gs_n]
        cmp eax, GS_PILA
        jae .x
        inc dword [gs_n]
        imul eax, G_TAM
        lea edi, [gs_pila+eax]
        mov esi, eg
        mov ecx, G_TAM / 4
        rep movsd
.x:
        ret
o_Q:
        mov eax, [gs_n]
        test eax, eax
        jz .x
        dec eax
        mov [gs_n], eax
        imul eax, G_TAM
        lea esi, [gs_pila+eax]
        mov edi, eg
        mov ecx, G_TAM / 4
        rep movsd
.x:
        ret
o_cm:
        mov ecx, 6
        call ci_args
        jc .x
        mov esi, ebx
        mov edi, eg+G_CTM
        mov ebx, edi
        call mat_mul
.x:
        ret
o_w:
        mov ecx, 1
        call ci_args
        jc .x
        mov eax, [ebx]
        mov [eg+G_GROS], eax
.x:
        ret

; ---- colores ----
; (interna) ECX=cuántos componentes (1 gris, 3 RGB, 4 CMYK) -> AL=color; CF=1 si faltan
ci_color:
        call ci_args
        jc .x
        cmp ecx, 3
        je .rgb
        ja .cmyk
        mov eax, [ebx]
        mov ecx, eax
        mov ebx, eax
        jmp .pon
.rgb:
        mov eax, [ebx]
        mov ecx, [ebx+8]
        mov ebx, [ebx+4]
        jmp .pon
.cmyk:
        ; rojo = (1 - cian)(1 - negro), y así los otros
        mov esi, 0x10000
        sub esi, [ebx+12]
        mov eax, 0x10000
        sub eax, [ebx]
        mov edx, esi
        call fmul
        push eax
        mov eax, 0x10000
        sub eax, [ebx+4]
        mov edx, esi
        call fmul
        push eax
        mov eax, 0x10000
        sub eax, [ebx+8]
        mov edx, esi
        call fmul
        mov ecx, eax
        pop ebx
        pop eax
.pon:
        call col_rgb
        clc
.x:
        ret
o_g:
        mov ecx, 1
        jmp o_rel
o_rg:
        mov ecx, 3
        jmp o_rel
o_k:
        mov ecx, 4
o_rel:
        call ci_color
        jc .x
        mov [eg+G_REL], al
.x:
        ret
o_G:
        mov ecx, 1
        jmp o_trz
o_RG:
        mov ecx, 3
        jmp o_trz
o_K:
        mov ecx, 4
o_trz:
        call ci_color
        jc .x
        mov [eg+G_TRZ], al
.x:
        ret
; sc, scn: no se sabe en qué espacio de color están; se adivina por cuántos
; números traen (1 gris, 3 RGB, 4 CMYK). Con un nombre detrás (un motivo), nada
ci_cuantos:
        mov ecx, [op_n]
        test ecx, ecx
        jz .no
        cmp byte [op_t+ecx-1], 0
        jne .no
        cmp ecx, 1
        je .si
        cmp ecx, 3
        je .si
        cmp ecx, 4
        je .si
.no:
        stc
        ret
.si:
        clc
        ret
o_sc:
        call ci_cuantos
        jnc o_rel
        ret
o_SC:
        call ci_cuantos
        jnc o_trz
        ret

; ---- el trazado ----
; (interna) añade el punto EAX, EBX (ya en la página), de la clase ECX
pt_pon:
        push edi
        mov [pt_act], eax
        mov [pt_act+4], ebx
        mov edi, [pt_n]
        cmp edi, MAX_PT
        jae .no
        inc dword [pt_n]
        imul edi, PT_TAM
        add edi, pt_tab
        mov [edi], eax
        mov [edi+4], ebx
        mov [edi+8], ecx
        pop edi
        ret
.no:
        or dword [pg_avisos], AV_LLENA
        pop edi
        ret
o_m:
        mov ecx, 2
        call ci_args
        jc .x
        mov eax, [ebx]
        mov ebx, [ebx+4]
        call ci_punto
        mov [pt_ini], eax
        mov [pt_ini+4], ebx
        xor ecx, ecx
        call pt_pon
.x:
        ret
o_l:
        mov ecx, 2
        call ci_args
        jc .x
        mov eax, [ebx]
        mov ebx, [ebx+4]
        call ci_punto
        mov ecx, 1
        cmp dword [pt_n], 0         ; (una raya sin «m» delante: empieza ahí)
        jne .l1
        mov [pt_ini], eax
        mov [pt_ini+4], ebx
        xor ecx, ecx
.l1:
        call pt_pon
.x:
        ret
o_h:
        cmp dword [pt_n], 0
        je .x
        mov eax, [pt_ini]
        mov ebx, [pt_ini+4]
        cmp eax, [pt_act]
        jne .h1
        cmp ebx, [pt_act+4]
        je .x
.h1:
        mov ecx, 1
        call pt_pon
.x:
        ret
o_re:
        mov ecx, 4
        call ci_args
        jc .x
        mov esi, ebx
        mov eax, [esi]              ; x, y
        mov ebx, [esi+4]
        call ci_punto
        mov [pt_ini], eax
        mov [pt_ini+4], ebx
        xor ecx, ecx
        call pt_pon
        mov eax, [esi]              ; x + ancho, y
        add eax, [esi+8]
        mov ebx, [esi+4]
        call ci_punto
        mov ecx, 1
        call pt_pon
        mov eax, [esi]              ; x + ancho, y + alto
        add eax, [esi+8]
        mov ebx, [esi+4]
        add ebx, [esi+12]
        call ci_punto
        mov ecx, 1
        call pt_pon
        mov eax, [esi]              ; x, y + alto
        mov ebx, [esi+4]
        add ebx, [esi+12]
        call ci_punto
        mov ecx, 1
        call pt_pon
        mov eax, [pt_ini]           ; y se cierra
        mov ebx, [pt_ini+4]
        mov ecx, 1
        call pt_pon
.x:
        ret
; las curvas: c (dos puntos de control y el final), v (el primero es el punto
; donde se está) e y (el segundo es el final)
o_c:
        mov ecx, 6
        call ci_args
        jc o_curva.x
        mov esi, ebx
        mov eax, [esi]
        mov ebx, [esi+4]
        call ci_punto
        mov [bz+8], eax
        mov [bz+12], ebx
        mov eax, [esi+8]
        mov ebx, [esi+12]
        call ci_punto
        mov [bz+16], eax
        mov [bz+20], ebx
        mov eax, [esi+16]
        mov ebx, [esi+20]
        jmp o_curva
o_v:
        mov ecx, 4
        call ci_args
        jc o_curva.x
        mov esi, ebx
        mov eax, [pt_act]
        mov [bz+8], eax
        mov eax, [pt_act+4]
        mov [bz+12], eax
        mov eax, [esi]
        mov ebx, [esi+4]
        call ci_punto
        mov [bz+16], eax
        mov [bz+20], ebx
        mov eax, [esi+8]
        mov ebx, [esi+12]
        jmp o_curva
o_y:
        mov ecx, 4
        call ci_args
        jc o_curva.x
        mov esi, ebx
        mov eax, [esi]
        mov ebx, [esi+4]
        call ci_punto
        mov [bz+8], eax
        mov [bz+12], ebx
        mov eax, [esi+8]
        mov ebx, [esi+12]
        call ci_punto
        mov [bz+16], eax
        mov [bz+20], ebx
        mov eax, [esi+8]
        mov ebx, [esi+12]
; (interna) EAX, EBX=el punto final (del usuario); bz+8...: los de control (ya
; en la página). Ocho rayas desde el punto donde se está
o_curva:
        call ci_punto
        mov [bz+24], eax
        mov [bz+28], ebx
        cmp dword [pt_n], 0
        je .x
        mov eax, [pt_act]
        mov [bz], eax
        mov eax, [pt_act+4]
        mov [bz+4], eax
        mov ecx, 8                  ; (se quitan 9 bits a cada número para que
.q:                                 ; quepa la cuenta: queda 1/128 de punto)
        sar dword [bz+ecx*4-4], 9
        dec ecx
        jnz .q
        xor esi, esi                ; ESI=el paso (0-7)
.p:
        xor edi, edi                ; EDI=0 la x, 1 la y
.e:
        xor ebx, ebx
        xor ecx, ecx                ; ECX=el punto de control
.s:
        lea edx, [ecx+esi*4]
        movzx eax, word [bz_pesos+edx*2]
        lea edx, [edi+ecx*2]
        imul eax, [bz+edx*4]
        add ebx, eax
        inc ecx
        cmp ecx, 4
        jb .s
        push ebx
        inc edi
        cmp edi, 2
        jb .e
        pop ebx
        pop eax
        mov ecx, 1
        call pt_pon
        inc esi
        cmp esi, 8
        jb .p
.x:
        ret

o_n:
        cmp dword [ci_recorta], 0
        je .n
        mov dword [ci_recorta], 0
        call ci_recorte
.n:
        mov dword [pt_n], 0
        ret
o_W:
        mov dword [ci_recorta], 1
        ret

; (interna) el trazado pasa a ser el recorte, si es un rectángulo derecho (lo
; que hacen casi todos los programas para recortar una imagen o una celda);
; con otra forma no se recorta
ci_recorte:
        pushad
        mov esi, pt_tab
        mov ecx, [pt_n]
        cmp ecx, 4
        jb .x
        call ci_rellena.trozo
        cmp edx, ecx
        jne .x
        call ci_rellena.es_rect
        jc .x
        mov eax, [esi]              ; dos esquinas opuestas
        mov ecx, [esi+2*PT_TAM]
        cmp eax, ecx
        jle .o1
        xchg eax, ecx
.o1:
        mov ebx, [esi+4]
        mov edx, [esi+2*PT_TAM+4]
        cmp ebx, edx
        jle .o2
        xchg ebx, edx
.o2:
        mov esi, [eg+G_CLIP]        ; dentro del que ya hubiera
        test esi, esi
        jz .nuevo
        shl esi, 4
        add esi, cl_tab
        cmp eax, [esi]
        jge .i1
        mov eax, [esi]
.i1:
        cmp ebx, [esi+4]
        jge .i2
        mov ebx, [esi+4]
.i2:
        cmp ecx, [esi+8]
        jle .i3
        mov ecx, [esi+8]
.i3:
        cmp edx, [esi+12]
        jle .nuevo
        mov edx, [esi+12]
.nuevo:
        mov esi, [cl_n]
        cmp esi, MAX_CLIP
        jae .x
        inc esi
        mov [cl_n], esi
        mov [eg+G_CLIP], esi
        shl esi, 4
        add esi, cl_tab
        mov [esi], eax
        mov [esi+4], ebx
        mov [esi+8], ecx
        mov [esi+12], edx
.x:
        popad
        ret
o_S:
        call ci_traza
        jmp o_n
o_s:
        call ci_cierra
        call ci_traza
        jmp o_n
o_f:
        call ci_rellena
        jmp o_n
o_B:
        call ci_rellena
        call ci_traza
        jmp o_n
o_b:
        call ci_cierra
        call ci_rellena
        call ci_traza
        jmp o_n

; (interna) cierra el último trozo del trazado
ci_cierra:
        jmp o_h

; (interna) las rayas del trazado, a la lista
ci_traza:
        pushad
        ; el grosor, en la página: lo que estire la matriz
        xor ecx, ecx
        xor ebx, ebx
.g:
        mov eax, [eg+G_CTM+ebx*4]
        cdq
        xor eax, edx
        sub eax, edx
        add ecx, eax
        inc ebx
        cmp ebx, 4
        jb .g
        shr ecx, 1
        mov eax, [eg+G_GROS]
        mov edx, ecx
        call fmul
        mov ebp, eax                ; EBP=el grosor
        mov esi, pt_tab
        mov ecx, [pt_n]
.b:
        test ecx, ecx
        jz .x
        cmp dword [esi+8], 0
        je .sig                     ; (empieza un trozo)
        call it_nuevo
        jc .x
        mov byte [edi+IT_TIPO], ITT_LINEA
        mov al, [eg+G_TRZ]
        mov [edi+IT_COLOR], al
        mov eax, [esi-PT_TAM]
        mov [edi+IT_X], eax
        mov eax, [esi-PT_TAM+4]
        mov [edi+IT_Y], eax
        mov eax, [esi]
        mov [edi+IT_AN], eax
        mov eax, [esi+4]
        mov [edi+IT_AL], eax
        mov [edi+IT_TXT], ebp
.sig:
        add esi, PT_TAM
        dec ecx
        jmp .b
.x:
        popad
        ret

; (interna) ¿son |EAX - EBX| menos de un treintavo de punto? ZF... CF=1: sí
ci_cerca:
        push eax
        sub eax, ebx
        jns .p
        neg eax
.p:
        cmp eax, 0x800
        pop eax
        ret

; (interna) el relleno del trazado, a la lista: si todos sus trozos son
; rectángulos derechos, un ITT_RECT por cada uno; si no, una figura
ci_rellena:
        pushad
        cmp dword [pt_n], 3
        jb .x
        ; ---- primera pasada: ¿todo rectángulos? ----
        mov esi, pt_tab
        mov ecx, [pt_n]
.r:
        test ecx, ecx
        jz .rects
        call .trozo                 ; EDX=cuántos puntos tiene el trozo de ESI
        call .es_rect
        jc .figura
        imul eax, edx, PT_TAM
        add esi, eax
        sub ecx, edx
        jmp .r
.rects:
        mov esi, pt_tab
        mov ecx, [pt_n]
.r2:
        test ecx, ecx
        jz .x
        call .trozo
        cmp edx, 4
        jb .r3                      ; (un punto suelto: nada)
        call it_nuevo
        jc .x
        mov byte [edi+IT_TIPO], ITT_RECT
        mov al, [eg+G_REL]
        mov [edi+IT_COLOR], al
        mov eax, [esi]              ; dos esquinas opuestas: la 0 y la 2
        mov ebx, [esi+2*PT_TAM]
        cmp eax, ebx
        jle .o1
        xchg eax, ebx
.o1:
        mov [edi+IT_X], eax
        sub ebx, eax
        mov [edi+IT_AN], ebx
        mov eax, [esi+4]
        mov ebx, [esi+2*PT_TAM+4]
        cmp eax, ebx
        jle .o2
        xchg eax, ebx
.o2:
        mov [edi+IT_Y], eax
        sub ebx, eax
        mov [edi+IT_AL], ebx
.r3:
        imul eax, edx, PT_TAM
        add esi, eax
        sub ecx, edx
        jmp .r2
        ; ---- una figura: los vértices de cada trozo, al almacén de figuras ----
.figura:
        mov eax, [pt_n]
        lea eax, [eax*8+eax]        ; (de sobra: 8 por punto y 4 por trozo)
        add eax, [po_tope]
        cmp eax, po_pool + POOL_POLI
        ja .llena
        call it_nuevo
        jc .x
        mov byte [edi+IT_TIPO], ITT_POLI
        mov al, [eg+G_REL]
        mov [edi+IT_COLOR], al
        mov eax, [po_tope]
        mov [edi+IT_TXT], eax
        mov dword [edi+IT_X], 0x7FFFFFFF    ; su caja: se va haciendo
        mov dword [edi+IT_Y], 0x7FFFFFFF
        mov dword [edi+IT_AN], 0x80000000   ; (de momento, la x y la y mayores)
        mov dword [edi+IT_AL], 0x80000000
        mov esi, pt_tab
        mov ecx, [pt_n]
.f:
        test ecx, ecx
        jz .f_fin
        call .trozo
        cmp edx, 3
        jb .f_sig
        inc word [edi+IT_N]
        mov ebx, [po_tope]
        mov [ebx], edx
        add ebx, 4
        push ecx
        push esi
        mov ecx, edx
.fp:
        mov eax, [esi]
        mov [ebx], eax
        cmp eax, [edi+IT_X]
        jge .c1
        mov [edi+IT_X], eax
.c1:
        cmp eax, [edi+IT_AN]
        jle .c2
        mov [edi+IT_AN], eax
.c2:
        mov eax, [esi+4]
        mov [ebx+4], eax
        cmp eax, [edi+IT_Y]
        jge .c3
        mov [edi+IT_Y], eax
.c3:
        cmp eax, [edi+IT_AL]
        jle .c4
        mov [edi+IT_AL], eax
.c4:
        add ebx, 8
        add esi, PT_TAM
        dec ecx
        jnz .fp
        mov [po_tope], ebx
        pop esi
        pop ecx
.f_sig:
        imul eax, edx, PT_TAM
        add esi, eax
        sub ecx, edx
        jmp .f
.f_fin:
        cmp word [edi+IT_N], 0
        jne .f1
        dec dword [it_n]            ; (no había nada que rellenar)
        jmp .x
.f1:
        mov eax, [edi+IT_X]         ; de «x mayor» a «ancho»
        sub [edi+IT_AN], eax
        mov eax, [edi+IT_Y]
        sub [edi+IT_AL], eax
        jmp .x
.llena:
        or dword [pg_avisos], AV_LLENA
.x:
        popad
        ret
; (interna) ESI=un punto que empieza trozo, ECX=los que quedan -> EDX=cuántos
; puntos tiene el trozo
.trozo:
        push esi
        mov edx, 1
.t:
        cmp edx, ecx
        jae .tx
        add esi, PT_TAM
        cmp dword [esi+8], 0
        je .tx
        inc edx
        jmp .t
.tx:
        pop esi
        ret
; (interna) ¿es el trozo de ESI (EDX puntos) un rectángulo derecho? CF=0: sí
; (o no llega a tres puntos: no pinta nada)
.es_rect:
        push eax
        push ebx
        cmp edx, 3
        jb .si
        cmp edx, 4
        jb .no
        cmp edx, 5
        ja .no
        jb .cuatro
        mov eax, [esi+4*PT_TAM]     ; cinco: el último, donde el primero
        mov ebx, [esi]
        call ci_cerca
        jae .no
        mov eax, [esi+4*PT_TAM+4]
        mov ebx, [esi+4]
        call ci_cerca
        jae .no
.cuatro:
        ; lados: vertical, horizontal, vertical, horizontal...
        mov eax, [esi]
        mov ebx, [esi+PT_TAM]
        call ci_cerca
        jae .otro
        mov eax, [esi+PT_TAM+4]
        mov ebx, [esi+2*PT_TAM+4]
        call ci_cerca
        jae .no
        mov eax, [esi+2*PT_TAM]
        mov ebx, [esi+3*PT_TAM]
        call ci_cerca
        jae .no
        mov eax, [esi+3*PT_TAM+4]
        mov ebx, [esi+4]
        call ci_cerca
        jae .no
        jmp .si
.otro:
        ; ...o al revés
        mov eax, [esi+4]
        mov ebx, [esi+PT_TAM+4]
        call ci_cerca
        jae .no
        mov eax, [esi+PT_TAM]
        mov ebx, [esi+2*PT_TAM]
        call ci_cerca
        jae .no
        mov eax, [esi+2*PT_TAM+4]
        mov ebx, [esi+3*PT_TAM+4]
        call ci_cerca
        jae .no
        mov eax, [esi+3*PT_TAM]
        mov ebx, [esi]
        call ci_cerca
        jae .no
.si:
        pop ebx
        pop eax
        clc
        ret
.no:
        pop ebx
        pop eax
        stc
        ret

; -----------------------------------------------------------------------------
;  Imágenes y formularios
; -----------------------------------------------------------------------------
; (interna) el recurso de la clase EDI que se llama [ci_nom] -> ESI, EBP; CF=1
; si no está
ci_recurso:
        mov esi, [ci_res]
        mov ebp, [ci_res+4]
        test esi, esi
        jz .no
        call px_dic
        jc .no
        call px_ref
        mov edi, ci_nom
        call px_dic
        jc .no
        call px_ref
        clc
        ret
.no:
        stc
        ret

; (interna) el nombre del último operando, a ci_nom; CF=1 si no es un nombre
ci_nombre_op:
        mov ecx, [op_n]
        test ecx, ecx
        jz .no
        cmp byte [op_t+ecx-1], 1
        jne .no
        mov esi, [op_v+ecx*4-4]
        mov ebp, [ci_lim]
        mov edi, ci_nom
        mov ecx, 64
        call px_copia_nombre
        ; (px_copia_nombre lo deja sin la barra)
        clc
        ret
.no:
        stc
        ret

; (interna) el sitio de una imagen: el cuadrado de lado 1, con la matriz en vigor
ci_imagen:
        pushad
        test eax, eax               ; EAX=su miniatura (0: no se sabe pintar)
        jnz .hay
        or dword [pg_avisos], AV_IMAGEN
.hay:
        call it_nuevo
        jc .x
        mov [edi+IT_TXT], eax
        mov byte [edi+IT_TIPO], ITT_IMAGEN
        ; cómo cae: la fila de arriba de la imagen va de (0,1) a (1,1)
        xor eax, eax
        mov ebx, 0x10000
        call ci_punto
        mov esi, eax                ; ESI, EBP=arriba a la izquierda
        mov ebp, ebx
        mov eax, 0x10000
        mov ebx, 0x10000
        call ci_punto
        sub eax, esi                ; EAX, EBX=hacia dónde van las columnas
        sub ebx, ebp
        mov ecx, eax
        mov edx, ebx
        test ecx, ecx
        jns .a1
        neg ecx
.a1:
        test edx, edx
        jns .a2
        neg edx
.a2:
        cmp edx, ecx
        ja .girada
        shr eax, 31                 ; II_ESPEJO si van hacia la izquierda
        push eax
        xor eax, eax
        xor ebx, ebx
        call ci_punto
        pop eax
        cmp ebx, ebp                ; II_VUELTA si la fila de abajo queda arriba
        jge .pon_b
        or al, II_VUELTA
        jmp .pon_b
.girada:
        mov eax, ebx
        shr eax, 31
        or al, II_GIRADA
        push eax
        xor eax, eax
        xor ebx, ebx
        call ci_punto
        mov ebx, eax
        pop eax
        cmp ebx, esi
        jge .pon_b
        or al, II_VUELTA
.pon_b:
        mov [edi+IT_BAND], al
        mov dword [edi+IT_X], 0x7FFFFFFF
        mov dword [edi+IT_Y], 0x7FFFFFFF
        mov dword [edi+IT_AN], 0x80000000
        mov dword [edi+IT_AL], 0x80000000
        xor ecx, ecx                ; las cuatro esquinas
.e:
        mov eax, ecx
        and eax, 1
        shl eax, 16
        mov ebx, ecx
        shr ebx, 1
        shl ebx, 16
        call ci_punto
        cmp eax, [edi+IT_X]
        jge .c1
        mov [edi+IT_X], eax
.c1:
        cmp eax, [edi+IT_AN]
        jle .c2
        mov [edi+IT_AN], eax
.c2:
        cmp ebx, [edi+IT_Y]
        jge .c3
        mov [edi+IT_Y], ebx
.c3:
        cmp ebx, [edi+IT_AL]
        jle .c4
        mov [edi+IT_AL], ebx
.c4:
        inc ecx
        cmp ecx, 4
        jb .e
        mov eax, [edi+IT_X]
        sub [edi+IT_AN], eax
        mov eax, [edi+IT_Y]
        sub [edi+IT_AL], eax
.x:
        popad
        ret

o_Do:
        call ci_nombre_op
        jc .x
        mov edi, k_xobject
        call ci_recurso
        jc .x
        push esi
        push ebp
        mov edi, k_subtype
        call px_dic
        jc .nada
        call px_ref
        mov edi, n_image
        call px_es_nombre
        je .imagen
        mov edi, n_form
        call px_es_nombre
        je .forma
.nada:
        pop ebp
        pop esi
.x:
        ret
.imagen:
        pop ebp
        pop esi
        call ar_marca
        push eax
        push esi
        push ebp
        call pdf_flujo
        pop ebp
        pop esi
        jc .im_no
        mov [im_dat], eax
        mov [im_ndat], ecx
        mov eax, [ci_res]
        mov [im_res], eax
        mov eax, [ci_res+4]
        mov [im_res+4], eax
        mov al, [eg+G_REL]
        call im_hace
        jnc .im_si
.im_no:
        xor eax, eax
.im_si:
        mov ebx, eax
        pop eax
        call ar_suelta
        mov eax, ebx
        jmp ci_imagen
.forma:
        ; un formulario: contenido con su matriz y, si los trae, sus recursos
        pop ebp
        pop esi
        cmp dword [ci_prof], 8      ; (formularios que se llaman a sí mismos)
        jae .x
        dec dword [ci_formas]
        jns .f0
        or dword [pg_avisos], AV_LLENA
        ret
.f0:
        push dword [ci_res]
        push dword [ci_res+4]
        push dword [gs_n]
        push esi
        push ebp
        call o_q
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_matrix
        call px_dic
        jc .f1
        call px_ref
        cmp esi, ebp
        jae .f1
        cmp byte [esi], '['
        jne .f1
        inc esi
        sub esp, 24
        xor ebx, ebx
.fm:
        call px_blancos
        call px_num
        jc .fm_mal
        mov [esp+ebx*4], eax
        inc ebx
        cmp ebx, 6
        jb .fm
        mov esi, esp
        mov edi, eg+G_CTM
        mov ebx, edi
        call mat_mul
.fm_mal:
        add esp, 24
.f1:
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_resources
        call px_dic
        jc .f2
        call px_ref
        mov [ci_res], esi
        mov [ci_res+4], ebp
.f2:
        pop ebp
        pop esi
        call ar_marca
        push eax
        call pdf_flujo
        jc .f_mal
        cmp dword [fl_parcial], 0
        je .f3
        or dword [pg_avisos], AV_ROTO
.f3:
        mov esi, eax
        lea ebp, [eax+ecx]
        mov dword [op_n], 0
        mov dword [pt_n], 0
        inc dword [ci_prof]
        call ci_ejecuta
        dec dword [ci_prof]
        mov dword [pt_n], 0
        jmp .f_fin
.f_mal:
        call ci_aviso_flujo
.f_fin:
        pop eax
        call ar_suelta
        pop eax                     ; el estado, como estaba antes de entrar
        inc eax
        cmp [gs_n], eax
        jb .f4
        mov [gs_n], eax
        call o_Q
.f4:
        pop dword [ci_res+4]
        pop dword [ci_res]
        ret

; BI ... ID datos EI: una imagen dentro del contenido. Se salta entera
o_BI:
        mov dword [ci_bi], 0
        mov esi, [esp+8]            ; (el cursor que guardó ci_ejecuta)
        mov ebp, [esp+4]
.b:
        call px_blancos
        cmp esi, ebp
        jae .fin
        mov edi, p_id
        call px_es_palabra
        je .id
        call px_salta
        jmp .b
.id:
        ; lo de entre «BI» e «ID» es su diccionario, sin los «<<»: se copia con ellos
        push esi
        mov ecx, esi
        mov esi, [esp+12]           ; (el cursor de ci_ejecuta: detrás de «BI»)
        sub ecx, esi
        cmp ecx, CAD_TAM - 8
        ja .id_no
        mov edi, ci_cad
        mov ax, '<<'
        stosw
        rep movsb
        mov ax, '>>'
        stosw
        pop esi
        add esi, 3                  ; «ID» y un blanco
        mov [im_dat], esi
        mov eax, ebp
        sub eax, esi
        jbe .e
        mov [im_ndat], eax
        push esi
        push ebp
        mov esi, ci_cad
        mov ebp, edi
        mov edi, k_filter           ; con filtros dentro del contenido, no
        call px_dic
        jnc .id_filtro
        mov esi, ci_cad
        mov edi, f_f1
        call px_dic
        jnc .id_filtro
        mov eax, [ci_res]
        mov [im_res], eax
        mov eax, [ci_res+4]
        mov [im_res+4], eax
        mov esi, ci_cad
        mov al, [eg+G_REL]
        call im_hace
        jc .id_filtro
        mov [ci_bi], eax
        pop ebp
        pop esi
        mov eax, [im_fila]          ; los datos miden lo que dicen: «EI», detrás
        mul dword [im_h]
        add eax, esi
        jc .e
        cmp eax, ebp
        ja .e
        mov esi, eax
        jmp .e
.id_filtro:
        pop ebp
        pop esi
        jmp .e
.id_no:
        pop esi
        add esi, 3
.e:
        ; «EI» entre blancos (o al final)
        lea eax, [esi+2]
        cmp eax, ebp
        ja .fin0
        cmp word [esi], 'EI'
        jne .e1
        movzx eax, byte [esi-1]
        cmp byte [px_clase+eax], 1
        jne .e1
        lea eax, [esi+2]
        cmp eax, ebp
        jae .e_si
        movzx eax, byte [eax]
        cmp byte [px_clase+eax], 0
        je .e1
.e_si:
        add esi, 2
        jmp .fin
.e1:
        inc esi
        jmp .e
.fin0:
        mov esi, ebp
.fin:
        mov [ci_salto], esi
        mov eax, [ci_bi]
        jmp ci_imagen

; -----------------------------------------------------------------------------
;  Texto
; -----------------------------------------------------------------------------
o_BT:
        push eax
        push ecx
        push edi
        mov edi, ci_tm
        mov ecx, 12
        xor eax, eax
        rep stosd
        mov eax, 0x10000
        mov [ci_tm], eax
        mov [ci_tm+12], eax
        mov [ci_tlm], eax
        mov [ci_tlm+12], eax
        pop edi
        pop ecx
        pop eax
        ret
%macro PON_UNO 2
%1:
        mov ecx, 1
        call ci_args
        jc .x
        mov eax, [ebx]
        mov [eg+%2], eax
.x:
        ret
%endmacro
        PON_UNO o_Tc, G_TC
        PON_UNO o_Tw, G_TW
        PON_UNO o_TL, G_TL
        PON_UNO o_Ts, G_TS
o_Tz:
        mov ecx, 1
        call ci_args
        jc .x
        mov eax, [ebx]              ; en tantos por ciento
        cdq
        mov ecx, 100
        idiv ecx
        mov [eg+G_TZ], eax
.x:
        ret
o_Tf:
        mov ecx, 1
        call ci_args
        jc .x
        mov eax, [ebx]
        cmp eax, 2000 << 16         ; (un tamaño disparatado desbordaría las cuentas)
        jle .t1
        mov eax, 2000 << 16
.t1:
        cmp eax, -(2000 << 16)
        jge .t2
        mov eax, -(2000 << 16)
.t2:
        mov [eg+G_TF], eax
        mov dword [eg+G_FU], 0
        dec dword [op_n]            ; el nombre es el operando de antes
        call ci_nombre_op
        jc .x
        mov edi, k_font
        call ci_recurso
        jc .x
        cmp esi, ebp
        jae .x
        cmp byte [esi], '<'
        jne .x
        call fu_busca
        mov [eg+G_FU], edi
.x:
        ret
; (interna) mueve el principio de la línea EAX, EBX (en unidades del texto)
ci_td:
        push ecx
        push edx
        push esi
        push edi
        mov esi, eax
        mov edi, ebx
        FM esi, [ci_tlm]
        mov ecx, eax
        FM edi, [ci_tlm+8]
        add ecx, eax
        add [ci_tlm+16], ecx
        FM esi, [ci_tlm+4]
        mov ecx, eax
        FM edi, [ci_tlm+12]
        add ecx, eax
        add [ci_tlm+20], ecx
        mov esi, ci_tlm
        mov edi, ci_tm
        mov ecx, 6
        rep movsd
        pop edi
        pop esi
        pop edx
        pop ecx
        ret
o_Td:
        mov ecx, 2
        call ci_args
        jc .x
        mov eax, [ebx]
        mov ebx, [ebx+4]
        call ci_td
.x:
        ret
o_TD:
        mov ecx, 2
        call ci_args
        jc .x
        mov eax, [ebx+4]
        neg eax
        mov [eg+G_TL], eax
        mov eax, [ebx]
        mov ebx, [ebx+4]
        call ci_td
.x:
        ret
o_Tm:
        mov ecx, 6
        call ci_args
        jc .x
        mov esi, ebx
        mov edi, ci_tm
        mov ecx, 6
        rep movsd
        mov esi, ebx
        mov edi, ci_tlm
        mov ecx, 6
        rep movsd
.x:
        ret
o_Tast:
        xor eax, eax
        mov ebx, [eg+G_TL]
        neg ebx
        jmp ci_td
o_Tj:
        mov ecx, [op_n]
        test ecx, ecx
        jz .x
        cmp byte [op_t+ecx-1], 2
        jne .x
        mov esi, [op_v+ecx*4-4]
        call ci_cadena
.x:
        ret
o_comilla:
        call o_Tast
        jmp o_Tj
o_comillas:
        mov ecx, [op_n]
        cmp ecx, 3
        jb o_comilla
        mov eax, [op_v+ecx*4-12]
        mov [eg+G_TW], eax
        mov eax, [op_v+ecx*4-8]
        mov [eg+G_TC], eax
        jmp o_comilla
o_TJ:
        mov ecx, [op_n]
        test ecx, ecx
        jz .x
        cmp byte [op_t+ecx-1], 3
        jne .x
        mov esi, [op_v+ecx*4-4]
        mov ebp, [ci_lim]
        inc esi
.b:
        call px_blancos
        cmp esi, ebp
        jae .x
        mov al, [esi]
        cmp al, ']'
        je .x
        cmp al, '('
        je .cad
        cmp al, '<'
        je .cad
        call px_num
        jc .raro
        ; un número: el texto retrocede tantas milésimas del tamaño
        mov edx, [eg+G_TF]
        call fmul
        cdq
        mov ecx, 1000
        idiv ecx
        mov edx, [eg+G_TZ]
        call fmul
        neg eax
        call ci_avanza
        jmp .b
.raro:
        call px_salta
        jmp .b
.cad:
        mov [ci_tj], esi
        mov [ci_tj+4], ebp
        call ci_cadena
        mov esi, [ci_tj]
        mov ebp, [ci_tj+4]
        call px_salta
        jmp .b
.x:
        ret

; (interna) la matriz del texto avanza EAX (en unidades del texto)
ci_avanza:
        push eax
        push ecx
        push edx
        mov ecx, eax
        FM ecx, [ci_tm]
        add [ci_tm+16], eax
        FM ecx, [ci_tm+4]
        add [ci_tm+20], eax
        pop edx
        pop ecx
        pop eax
        ret

; (interna) ESI=una cadena del contenido («(...)» o «<...>»): la enseña
ci_cadena:
        pushad
        mov ebp, [ci_lim]
        mov edi, ci_cad
        mov ecx, CAD_TAM
        call px_cadena
        mov ecx, eax
        mov esi, ci_cad
        call ci_muestra
        popad
        ret

; (interna) ESI=los códigos de un trozo de texto, ECX=cuántos octetos: lo pone
; en la lista, donde diga la matriz del texto, y avanza
ci_muestra:
        pushad
        mov ebp, [eg+G_FU]
        test ebp, ebp
        jz .x
        test ecx, ecx
        jz .x
        ; la matriz que lleva del texto a la página
        push esi
        mov esi, ci_tm
        mov edi, eg+G_CTM
        mov ebx, ci_m
        call mat_mul
        pop esi
        ; dónde empieza (con la subida «Ts»)
        FM [eg+G_TS], [ci_m+8]
        add eax, [ci_m+16]
        mov [ci_x], eax
        FM [eg+G_TS], [ci_m+12]
        add eax, [ci_m+20]
        mov [ci_y], eax
        ; hacia dónde va y cuánto estira a lo ancho y a lo alto
        mov eax, [ci_m]
        cdq
        xor eax, edx
        sub eax, edx
        mov ebx, eax                ; |a|
        mov eax, [ci_m+4]
        cdq
        xor eax, edx
        sub eax, edx                ; |b|
        cmp ebx, eax
        jb .vertical
        mov [ci_xs], ebx
        mov eax, [ci_m+12]
        xor ebx, ebx
        cmp dword [ci_m], 0
        jge .giro
        mov ebx, 2
        jmp .giro
.vertical:
        mov [ci_xs], eax
        mov eax, [ci_m+8]
        mov ebx, 1
        cmp dword [ci_m+4], 0
        jg .giro
        mov ebx, 3
.giro:
        mov [ci_giro], ebx
        cdq
        xor eax, edx
        sub eax, edx
        mov edx, [eg+G_TF]
        test edx, edx
        jns .t1
        neg edx
.t1:
        call fmul
        mov [ci_tam], eax
        ; las letras: a ci_sal; y lo que avanzan, sumado
        mov dword [ci_av], 0
        mov dword [ci_hay], 0       ; ¿hay algo que no sea un espacio?
        mov edi, ci_sal
        lea edx, [esi+ecx]          ; EDX=el final de los códigos
.c:
        cmp esi, edx
        jae .c_fin
        movzx eax, byte [esi]
        inc esi
        test byte [ebp+F_BAND], FB_DOS
        jz .una
        cmp esi, edx
        jae .c_fin
        shl eax, 8
        mov al, [esi]
        inc esi
        movzx ecx, word [ebp+F_DW]  ; el ancho y la letra de un código doble
        xor ebx, ebx
        cmp eax, T0_COD
        jb .c_tabla
        test byte [ebp+F_BAND], FB_IDENT
        jz .c_an
        push eax
        call uni_437
        mov ebx, eax
        pop eax
        jmp .c_an
.c_tabla:
        mov ebx, [ebp+F_T0AN]
        test ebx, ebx
        jz .c_dw
        movzx ecx, byte [ebx+eax]
        shl ecx, 3
.c_dw:
        mov ebx, [ebp+F_T0MAP]
        test ebx, ebx
        jz .c_an
        movzx ebx, byte [ebx+eax]
        jmp .c_an
.una:
        movzx ecx, word [ebp+F_ANCHO+eax*2]
        movzx ebx, byte [ebp+F_COD+eax]
        test ebx, ebx
        jnz .u1
        cmp eax, 32                 ; (un código de control sin letra: nada)
        jae .u1
        mov ebx, -1
.u1:
        cmp eax, 32                 ; el espacio de verdad lleva el «Tw»
        jne .c_an
        push eax
        mov eax, [eg+G_TW]
        add [ci_av], eax
        pop eax
.c_an:
        ; ECX=el ancho (milésimas) EBX=la letra (0: no se sabe; -1: ninguna)
        push edx
        mov eax, [eg+G_TF]
        imul ecx
        mov ecx, 1000
        idiv ecx
        add eax, [eg+G_TC]
        add [ci_av], eax
        pop edx
        test byte [ebp+F_BAND], FB_ILEGIBLE
        jnz .c
        cmp ebx, -1
        je .c
        cmp edi, ci_sal + CAD_TAM * 3
        jae .c
        test ebx, ebx
        jnz .l1
        or dword [pg_avisos], AV_DESCON
        mov bl, '?'
.l1:
        cmp ebx, 16
        jb .varias
        cmp bl, ' '
        je .l2
        mov dword [ci_hay], 1
.l2:
        mov [edi], bl
        inc edi
        jmp .c
.varias:
        mov dword [ci_hay], 1
        lea ebx, [exp_tab+ebx*4]
.v:
        mov al, [ebx]
        test al, al
        jz .c
        stosb
        inc ebx
        jmp .v
.c_fin:
        sub edi, ci_sal
        mov [ci_nsal], edi
        ; lo que avanza: por el estirado horizontal «Tz»
        FM [ci_av], [eg+G_TZ]
        mov [ci_av], eax
        mov ecx, eax                ; ECX=el ancho en la página
        FM ecx, [ci_xs]
        mov ecx, eax
        test ecx, ecx
        jns .pos
        neg ecx                     ; (texto que va hacia atrás: empieza antes)
        cmp dword [ci_giro], 0
        jne .pos
        sub [ci_x], ecx
.pos:
        mov eax, [ci_av]
        call ci_avanza
        test byte [ebp+F_BAND], FB_ILEGIBLE
        jnz .ilegible
        cmp dword [ci_nsal], 0
        je .x
        ; ¿sigue al trozo anterior, en su misma línea? Se junta con él
        mov eax, [it_n]
        test eax, eax
        jz .nuevo
        dec eax
        shl eax, 5
        lea edi, [it_tab+eax]
        cmp byte [edi+IT_TIPO], ITT_TEXTO
        jne .nuevo
        call .mismo
        jne .nuevo
        mov esi, [edi+IT_TXT]       ; (sus letras son las últimas del almacén)
        movzx eax, word [edi+IT_N]
        add esi, eax
        cmp esi, [tx_tope]
        jne .nuevo
        add eax, [ci_nsal]
        cmp eax, 60000
        ja .nuevo
        mov ebx, [tx_tope]
        add ebx, [ci_nsal]
        cmp ebx, tx_pool + POOL_TXT
        ja .llena
        mov [edi+IT_N], ax
        mov eax, [ci_x]             ; su ancho llega ahora hasta el final de este
        add eax, ecx
        sub eax, [edi+IT_X]
        mov [edi+IT_AN], eax
        jmp .copia
.nuevo:
        cmp dword [ci_hay], 0       ; solo espacios, y sueltos: no hace falta
        je .x
        mov ebx, [tx_tope]
        add ebx, [ci_nsal]
        cmp ebx, tx_pool + POOL_TXT
        ja .llena
        call it_nuevo
        jc .x
        mov byte [edi+IT_TIPO], ITT_TEXTO
        call .comun
        mov eax, [tx_tope]
        mov [edi+IT_TXT], eax
        mov eax, [ci_nsal]
        mov [edi+IT_N], ax
.copia:
        mov esi, ci_sal
        mov edi, [tx_tope]
        mov ecx, [ci_nsal]
        rep movsb
        mov [tx_tope], edi
        jmp .x
.ilegible:
        or dword [pg_avisos], AV_ILEGIBLE
        mov eax, [it_n]             ; un trozo ilegible detrás de otro: uno solo
        test eax, eax
        jz .i_nuevo
        dec eax
        shl eax, 5
        lea edi, [it_tab+eax]
        cmp byte [edi+IT_TIPO], ITT_ILEGIBLE
        jne .i_nuevo
        call .mismo
        jne .i_nuevo
        mov eax, [ci_x]
        add eax, ecx
        sub eax, [edi+IT_X]
        mov [edi+IT_AN], eax
        jmp .x
.i_nuevo:
        call it_nuevo
        jc .x
        mov byte [edi+IT_TIPO], ITT_ILEGIBLE
        call .comun
        jmp .x
.llena:
        or dword [pg_avisos], AV_LLENA
.x:
        popad
        ret
; (interna) lo que llevan igual un texto y un ilegible. ECX=el ancho
.comun:
        mov al, [eg+G_REL]
        mov [edi+IT_COLOR], al
        mov al, [ebp+F_TIPO]
        mov [edi+IT_LETRA], al
        mov al, [ebp+F_BAND]
        and al, FB_NEGRA | FB_CURSIVA
        mov ebx, [ci_giro]
        shl ebx, 4
        or al, bl
        mov [edi+IT_BAND], al
        mov eax, [ci_x]
        mov [edi+IT_X], eax
        mov eax, [ci_y]
        mov [edi+IT_Y], eax
        mov [edi+IT_AN], ecx
        mov eax, [ci_tam]
        mov [edi+IT_AL], eax
        mov eax, ebp
        sub eax, fu_tab
        xor edx, edx
        mov ebx, F_TAM
        div ebx
        mov [edi+IT_FU], ax
        ret
; (interna) ¿sigue este trozo al elemento EDI? ZF=1: sí (misma fuente, tamaño,
; color y línea, sin girar, y empieza donde acaba el otro, más o menos)
.mismo:
        push eax
        push ebx
        push edx
        cmp dword [ci_giro], 0
        jne .m_no
        test byte [edi+IT_BAND], IB_GIRO
        jnz .m_no
        mov eax, [eg+G_CLIP]
        cmp ax, [edi+IT_CLIP]
        jne .m_no
        mov al, [eg+G_REL]
        cmp al, [edi+IT_COLOR]
        jne .m_no
        mov eax, ebp
        sub eax, fu_tab
        xor edx, edx
        mov ebx, F_TAM
        div ebx
        cmp ax, [edi+IT_FU]
        jne .m_no
        mov eax, [ci_tam]
        mov ebx, [edi+IT_AL]
        call ci_cerca
        jae .m_no
        mov eax, [ci_y]
        sub eax, [edi+IT_Y]
        jns .m1
        neg eax
.m1:
        cmp eax, 0x4000             ; un cuarto de punto
        ja .m_no
        mov eax, [ci_x]             ; el hueco: de -0,3 a +0,1 del tamaño (un
        sub eax, [edi+IT_X]         ; espacio de TeX apretado mide 0,17)
        sub eax, [edi+IT_AN]
        mov ebx, [ci_tam]
        mov edx, ebx
        imul edx, 26
        sar edx, 8
        cmp eax, edx
        jg .m_no
        imul ebx, 5
        sar ebx, 4
        neg ebx
        cmp eax, ebx
        jl .m_no
        xor eax, eax                ; ZF=1
        jmp .m_x
.m_no:
        or eax, 1
.m_x:
        pop edx
        pop ebx
        pop eax
        ret
