; =============================================================================
;  pase.inc - la presentación a pantalla completa de Presentaciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como los emuladores (lib/emufull.inc): a la ventana se le quitan el título,
;  el marco y el menú y se lleva a toda la pantalla; al acabar se deja como
;  estaba. La diapositiva se dibuja con el motor (sin los textos de ejemplo de
;  las cajas vacías) y se enseña centrada, con negro alrededor: punto por
;  punto si la pantalla es de 800 x 600; si no, reducida o ampliada hasta
;  llenarla (como mucho 1024 x 768).
;
;  Se avanza con espacio, Intro, flecha derecha o abajo, AvPág o un clic; se
;  retrocede con flecha izquierda o arriba, RePág o Retroceso; Inicio y Fin
;  van a la primera y a la última; un número e Intro, a esa diapositiva; Esc
;  sale. Pasada la última sale un aviso de que se ha acabado.
;
;  Con «puntos de uno en uno», cada avance enseña un punto más de las listas
;  de la diapositiva antes de pasar a la siguiente ([lam_parr_max] del motor).
;  Las transiciones (cortina, barrido, disolver) se hacen pintando la
;  diapositiva nueva sobre la que hay en pantalla, por partes, en unos pasos.
; =============================================================================
TR_PASOS    equ 12                  ; pasos de una cortina o de un barrido

pres_inicio:
        xor eax, eax
        jmp pres_empieza
pres_aqui:
        mov eax, [pag]
; EAX=diapositiva: empieza la presentación por ella
pres_empieza:
        cmp byte [pres], 0
        jne .x
        call fin_edicion
        mov dword [sel], -1
        mov dword [herr], 0
        mov byte [drag], 0
        mov [pres_pag], eax
        mov dword [pres_paso], 0
        mov dword [pres_num], 0
        mov byte [pres_fin], 0
        mov ebp, [ven]
        mov eax, [ebp+V_ESTILO]     ; la ventana, como está: para devolverla
        mov [pc_estilo], eax
        mov eax, [ebp+V_MENU]
        mov [pc_menu], eax
        mov eax, [ebp+V_X]
        mov [pc_x], eax
        mov eax, [ebp+V_Y]
        mov [pc_y], eax
        mov eax, [ebp+V_AN]
        mov [pc_an], eax
        mov eax, [ebp+V_AL]
        mov [pc_al], eax
        and dword [ebp+V_ESTILO], ~(E_TITULO | E_MAXIM | E_MINIM | E_TAMANO)
        mov dword [ebp+V_MENU], 0
        mov byte [pres], 1
        xor eax, eax                ; las barras de desplazamiento, escondidas
        mov ebp, [barra_v]
        HV ven_mostrar
        mov ebp, [barra_h]
        HV ven_mostrar
        HV pantalla                 ; EAX=ancho EBX=alto de la pantalla
        mov [pan_an], eax
        mov [pan_al], ebx
        call pres_medidas
        call pres_dibuja
        mov ebp, [ven]
        HV ven_activar
        xor eax, eax
        xor ebx, ebx
        mov ecx, [pan_an]
        mov edx, [pan_al]
        HV ven_mover
        mov ebp, [ven]
        HV ven_invalidar
.x:
        ret

; se acaba la presentación: la ventana, como estaba, en la diapositiva en la
; que se ha quedado
pres_acaba:
        cmp byte [pres], 0
        je .x
        pushad
        mov byte [pres], 0
        mov byte [pres_fin], 0
        mov ebp, [ven]
        mov eax, [pc_estilo]
        mov [ebp+V_ESTILO], eax
        mov eax, [pc_menu]
        mov [ebp+V_MENU], eax
        mov eax, 1
        mov ebp, [barra_v]
        HV ven_mostrar
        mov ebp, [barra_h]
        HV ven_mostrar
        mov ebp, [ven]
        mov eax, [pc_x]
        mov ebx, [pc_y]
        mov ecx, [pc_an]
        mov edx, [pc_al]
        HV ven_mover
        mov eax, [pres_pag]
        cmp eax, [lam_pags]
        jb .p
        mov eax, [lam_pags]
        dec eax
.p:
        call pag_ir                 ; (y la lámina se dibuja otra vez)
        mov ebp, [ven]
        HV ven_activar
        HV ven_invalidar
        popad
.x:
        ret

; (interna) cuánto mide la diapositiva en la pantalla y dónde cae
pres_medidas:
        pushad
        mov eax, [pan_an]           ; lo que se puede llenar
        cmp eax, PRES_AN
        jbe .a
        mov eax, PRES_AN
.a:
        mov ebx, [pan_al]
        cmp ebx, PRES_AL
        jbe .b
        mov ebx, PRES_AL
.b:
        mov ecx, eax                ; a 4:3: lo que dé el ancho...
        mov eax, ecx
        mov edx, [lam_al]
        mul edx
        div dword [lam_an]          ; EAX=el alto que le toca a ese ancho
        cmp eax, ebx
        jbe .c
        mov eax, ebx                ; ...o lo que dé el alto
        mov edx, [lam_an]
        mul edx
        div dword [lam_al]
        mov ecx, eax
        mov eax, ebx
.c:
        mov [pres_an], ecx
        mov [pres_al], eax
        mov edx, [pan_an]
        sub edx, ecx
        sar edx, 1
        mov [pres_x], edx
        mov edx, [pan_al]
        sub edx, eax
        sar edx, 1
        mov [pres_y], edx
        popad
        ret

; (interna) dibuja la diapositiva [pres_pag] (con los puntos que toquen) y la
; deja lista para pintarla: en lam_buf si va punto por punto, o en pres_buf
pres_dibuja:
        pushad
        push dword [lam_guia_fn]    ; (sin los textos de ejemplo)
        mov dword [lam_guia_fn], 0
        mov dword [lam_parr_max], 0
        test byte [lam_extra+2], 1
        jz .d
        mov eax, [pres_paso]
        inc eax
        mov [lam_parr_max], eax
.d:
        mov ecx, [pres_pag]
        call lam_pinta
        mov dword [lam_parr_max], 0
        pop dword [lam_guia_fn]
        mov byte [lam_sucio], 1     ; (lam_buf ya no es la lámina del editor)
        mov dword [pres_src], lam_buf
        mov ecx, [pres_an]
        mov edx, [pres_al]
        cmp ecx, [lam_an]
        jne .escala
        cmp edx, [lam_al]
        je .x
.escala:
        mov edi, pres_buf
        mov [pres_src], edi
        mov ebx, [suave]
        call lam_reduce
.x:
        popad
        ret

; -> EAX=los puntos que tienen las listas de la diapositiva [pres_pag] (la que
; más tenga): los pasos que hay que dar para verla entera
pres_puntos:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        xor edi, edi
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, [pres_pag]
        jne .s
        mov cl, [esi+LO_BAND]
        and cl, LB_VINETAS | LB_OCULTO
        cmp cl, LB_VINETAS
        jne .s
        mov ecx, [esi+LO_LARGO]
        test ecx, ecx
        jz .s
        mov ebx, [esi+LO_DAT]
        add ebx, lam_dat
        mov edx, 1                  ; los párrafos: uno, y otro por cada salto
.c:                                 ; de línea que no sea el último
        dec ecx
        jz .f
        cmp byte [ebx], 10
        jne .c1
        inc edx
.c1:
        inc ebx
        jmp .c
.f:
        cmp edx, edi
        jbe .s
        mov edi, edx
.s:
        inc eax
        jmp .b
.x:
        mov eax, edi
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ---- avanzar y retroceder ----
pres_sig:
        cmp byte [pres_fin], 0      ; pasada la última: se sale
        jne pres_acaba
        mov dword [pres_num], 0
        test byte [lam_extra+2], 1  ; ¿quedan puntos por salir?
        jz .otra
        call pres_puntos
        cmp [pres_paso], eax
        jae .otra
        inc dword [pres_paso]
        call pres_dibuja
        jmp pres_inval
.otra:
        mov eax, [pres_pag]
        inc eax
        cmp eax, [lam_pags]
        jb .va
        mov byte [pres_fin], 1
        jmp pres_inval
.va:
        mov [pres_pag], eax
        mov dword [pres_paso], 0
        jmp pres_cambia
pres_ant:
        mov dword [pres_num], 0
        cmp byte [pres_fin], 0
        je .a
        mov byte [pres_fin], 0      ; del aviso del final, a la última otra vez
        jmp pres_inval
.a:
        test byte [lam_extra+2], 1
        jz .otra
        cmp dword [pres_paso], 0
        je .otra
        dec dword [pres_paso]
        call pres_dibuja
        jmp pres_inval
.otra:
        mov eax, [pres_pag]
        test eax, eax
        jz pres_inval
        dec eax
        mov [pres_pag], eax
        call pres_puntos            ; (la anterior, con todos sus puntos)
        mov [pres_paso], eax
        jmp pres_cambia
; EAX=diapositiva (si no la hay, la última): va a ella
pres_ir:
        mov dword [pres_num], 0
        mov byte [pres_fin], 0
        cmp eax, [lam_pags]
        jb .a
        mov eax, [lam_pags]
        dec eax
.a:
        mov [pres_pag], eax
        mov dword [pres_paso], 0
; (interna) otra diapositiva: se dibuja y entra con su transición
pres_cambia:
        call pres_dibuja
        call transicion
pres_inval:
        mov ebp, [ven]
        HV ven_invalidar
        ret

; ---- teclado y ratón, presentando ----
pres_tecla:
        cmp ecx, TC_ESC
        jne .n0
        call pres_acaba
        jmp hecho
.n0:
        cmp ecx, TC_INTRO
        jne .n1
        mov eax, [pres_num]         ; un número e Intro: a esa diapositiva
        test eax, eax
        jz .sig
        dec eax
        call pres_ir
        jmp hecho
.n1:
        cmp ecx, TC_DER
        je .sig
        cmp ecx, TC_ABAJO
        je .sig
        cmp ecx, TC_AVPAG
        je .sig
        cmp ecx, TC_IZQ
        je .ant
        cmp ecx, TC_ARRIBA
        je .ant
        cmp ecx, TC_REPAG
        je .ant
        cmp ecx, TC_RETRO
        je .ant
        cmp ecx, TC_INICIO
        jne .n2
        xor eax, eax
        call pres_ir
        jmp hecho
.n2:
        cmp ecx, TC_FIN
        jne hecho
        mov eax, -1
        call pres_ir
        jmp hecho
.sig:
        call pres_sig
        jmp hecho
.ant:
        call pres_ant
        jmp hecho
pres_letra:
        cmp ecx, ' '
        jne .n
        call pres_sig
        jmp hecho
.n:
        sub ecx, '0'                ; las cifras: el número de diapositiva
        cmp ecx, 9
        ja hecho
        mov eax, [pres_num]
        imul eax, 10
        add eax, ecx
        cmp eax, 999
        ja hecho
        mov [pres_num], eax
        jmp hecho
pres_clic:
        call pres_sig
        jmp hecho

; ---- pintar, presentando: negro alrededor y la diapositiva en medio ----
; (interna) -> el color negro: el del cubo de colores; con menos de 256, tinta
pres_negro:
        push eax
        mov eax, 16
        cmp dword [suave], 0
        jne .a
        mov eax, C_TINTA
.a:
        HV dc_color
        pop eax
        ret
pres_pinta:
        HV dc_pintar
        call pres_negro
        cmp byte [pres_fin], 0
        je .dia
        xor eax, eax                ; el aviso del final, sobre negro
        xor ebx, ebx
        mov ecx, [pan_an]
        mov edx, [pan_al]
        HV dc_rect
        COLOR C_PIEDRA
        mov eax, [pan_an]
        sub eax, 52 * 8
        sar eax, 1
        mov ebx, [pan_al]
        sar ebx, 1
        mov esi, t_fin
        HV dc_texto
        HV dc_cerrar
        jmp hecho
.dia:
        xor eax, eax                ; por arriba y por abajo
        xor ebx, ebx
        mov ecx, [pan_an]
        mov edx, [pres_y]
        test edx, edx
        jle .n1
        HV dc_rect
.n1:
        mov ebx, [pres_y]
        add ebx, [pres_al]
        mov edx, [pan_al]
        sub edx, ebx
        jle .n2
        HV dc_rect
.n2:
        mov ebx, [pres_y]           ; y a los lados
        mov edx, [pres_al]
        mov ecx, [pres_x]
        test ecx, ecx
        jle .n3
        HV dc_rect
.n3:
        mov eax, [pres_x]
        add eax, [pres_an]
        mov ecx, [pan_an]
        sub ecx, eax
        jle .n4
        HV dc_rect
.n4:
        call pres_copia
        HV dc_cerrar
        jmp hecho
; (interna, con el contexto abierto) la diapositiva, a su sitio en la pantalla
pres_copia:
        pushad
        mov eax, [pres_x]
        mov ebx, [pres_y]
        mov ecx, [pres_an]
        mov edx, [pres_al]
        mov esi, [pres_src]
        HV dc_copiar
        popad
        ret

; ---- las transiciones: la diapositiva nueva (ya dibujada) va tapando por
;      partes la que hay en la pantalla ----
transicion:
        pushad
        movzx eax, byte [lam_extra+1]
        cmp eax, 1
        je .cortina
        cmp eax, 2
        je .barrido
        cmp eax, 3
        je .disolver
        popad
        ret
.cortina:
        mov dword [tr_k], 1
.c1:
        mov ebp, [ven]
        HV dc_abrir
        mov eax, [pres_al]          ; de arriba hasta aquí
        mul dword [tr_k]
        mov ecx, TR_PASOS
        div ecx
        mov edx, eax
        mov eax, [pres_x]
        mov ebx, [pres_y]
        mov ecx, [pres_an]
        HV dc_cortar
        call pres_copia
        HV dc_cerrar
        call tr_espera
        inc dword [tr_k]
        cmp dword [tr_k], TR_PASOS
        jbe .c1
        popad
        ret
.barrido:
        mov dword [tr_k], 1
.b1:
        mov ebp, [ven]
        HV dc_abrir
        mov eax, [pres_an]          ; de la izquierda hasta aquí
        mul dword [tr_k]
        mov ecx, TR_PASOS
        div ecx
        mov ecx, eax
        mov eax, [pres_x]
        mov ebx, [pres_y]
        mov edx, [pres_al]
        HV dc_cortar
        call pres_copia
        HV dc_cerrar
        call tr_espera
        inc dword [tr_k]
        cmp dword [tr_k], TR_PASOS
        jbe .b1
        popad
        ret
.disolver:
        ; en dieciséis pasos: en cada uno, los puntos a los que les toca según
        ; la trama de 4x4; los demás se dejan como están (CLAVE no se copia)
        mov dword [tr_k], 0
.d1:
        mov esi, [pres_src]
        mov edi, hoja_buf
        xor ebx, ebx                ; la fila
.df:
        cmp ebx, [pres_al]
        jae .dp
        mov edx, ebx
        and edx, 3
        shl edx, 2
        xor ecx, ecx
.dc:
        cmp ecx, [pres_an]
        jae .dsf
        mov eax, ecx
        and eax, 3
        movzx eax, byte [tramado+edx+eax]
        shr eax, 4
        cmp eax, [tr_k]
        mov al, CLAVE
        jne .dpon
        mov al, [esi]
.dpon:
        mov [edi], al
        inc esi
        inc edi
        inc ecx
        jmp .dc
.dsf:
        inc ebx
        jmp .df
.dp:
        mov ebp, [ven]
        HV dc_abrir
        mov eax, [pres_x]
        mov ebx, [pres_y]
        mov ecx, [pres_an]
        mov edx, [pres_al]
        mov esi, hoja_buf
        HV dc_copiar_clave
        HV dc_cerrar
        call tr_espera
        inc dword [tr_k]
        cmp dword [tr_k], 16
        jb .d1
        popad
        ret
; (interna) espera al tic de reloj siguiente, dejando el turno a los demás
tr_espera:
        push eax
        push edx
        HV tics
        mov edx, eax
.e:
        HV ceder
        HV tics
        cmp eax, edx
        je .e
        pop edx
        pop eax
        ret
