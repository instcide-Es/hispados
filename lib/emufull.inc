; =============================================================================
;  lib/emufull.inc - pantalla completa para los emuladores de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo comparten Hispa-Nes (programas/hispanes.asm, NES) y Hispa-System
;  (programas/hispasms.asm, Master System): el modo de pantalla completa,
;  idéntico en los dos. Así se juega sin la ventana alrededor: el cuadro ocupa
;  toda la pantalla, centrado y a la mayor escala entera que quepa, con el
;  fondo negro (C_TINTA) alrededor, sin marco ni título ni menú ni escritorio.
;
;  Se entra y se sale con Alt+Intro (el estándar de los emuladores) o con la
;  orden «Pantalla completa» del menú Tamaño; Esc también sale (a ventana, sin
;  cerrar el emulador). El reloj (cuadros por segundo) y el teclado del juego
;  siguen yendo igual: solo cambia cómo se coloca y se dibuja el cuadro.
;
;  No se toca el núcleo. En pantalla completa a la ficha de la propia ventana
;  se le quitan de V_ESTILO el título, el marco y los botones, y se le pone
;  V_MENU a cero; ven_cli y nc_pintar del núcleo YA saben, con eso, dar toda
;  la ventana como zona de cliente y no pintar nada alrededor (igual que una
;  ventana sin E_TITULO ni E_EMERGENTE). Escribir en la ficha de la ventana es
;  lo mismo que ya hace WM_CREATE al poner V_MENU y V_ICONO. Al salir se
;  devuelve la ventana a como estaba (estilo, menú, posición y tamaño).
;
;  El emulador define antes de incluirlo:
;      JUEGO_AN   el ancho del cuadro de la consola (256)
;      JUEGO_AL   su alto (240 en la NES, 192 en la Master System)
;      FULL_MAX   la mayor escala entera (3; «pant» ha de ser de al menos
;                 JUEGO_AN*FULL_MAX por JUEGO_AL*FULL_MAX octetos)
;  y tiene en su .bss (como el otro emulador): ven, escala, cli_an, cli_al,
;  img_x, img_y, imagen, col_host, pant y pantc, pc_n y pc_estilo...pc_al.
;  En el menú Tamaño va la orden 113 «Pantalla completa», con la etiqueta
;  «pc_marca» en su primer carácter (una marca cuando está activa).
; =============================================================================

; ---- entrar o salir de pantalla completa (desde el menú o con Alt+Intro) ----
pc_alterna:
        cmp byte [pantc], 0
        jne pc_salir
        ; (si está en ventana, cae en pc_entrar)

; ---- entrar: guarda la ventana y la pone a cubrir toda la pantalla ----
pc_entrar:
        pushad
        cmp byte [pantc], 0
        jne .x                      ; ya estaba
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov eax, [ebp+V_ESTILO]     ; guardar estilo, menú y colocación
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
        ; quitarle el título, el marco, los botones y el menú: toda la ventana
        ; pasa a ser cliente y el núcleo no le pinta nada alrededor
        and dword [ebp+V_ESTILO], ~(E_TITULO | E_MAXIM | E_MINIM | E_TAMANO)
        mov dword [ebp+V_MENU], 0
        mov byte [pantc], 1
        mov byte [pc_marca], '*'    ; la orden del menú, marcada
        HV ven_activar              ; al frente y con el foco: tapa el escritorio
        HV pantalla                 ; -> EAX=ancho EBX=alto de la pantalla
        mov ecx, eax
        mov edx, ebx
        xor eax, eax
        xor ebx, ebx
        HV ven_mover                ; a 0,0 y toda la pantalla
        mov ebp, [ven]
        HV ven_invalidar
.x:
        popad
        ret

; ---- salir: devuelve la ventana a como estaba (misma posición y tamaño) ----
pc_salir:
        pushad
        cmp byte [pantc], 0
        je .x
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov byte [pantc], 0
        mov byte [pc_marca], ' '    ; la orden del menú, sin marca
        mov eax, [pc_estilo]        ; devolver el estilo y el menú
        mov [ebp+V_ESTILO], eax
        mov eax, [pc_menu]
        mov [ebp+V_MENU], eax
        mov eax, [pc_x]             ; y la colocación de antes
        mov ebx, [pc_y]
        mov ecx, [pc_an]
        mov edx, [pc_al]
        HV ven_mover
        mov ebp, [ven]
        HV ven_activar
        HV ven_invalidar
.x:
        popad
        ret

; ---- (interna) las medidas en pantalla completa: toda la ventana es cliente;
;      la escala es la mayor entera que quepa (hasta FULL_MAX) y la imagen va
;      centrada, con lo que sobra en negro alrededor ----
medidas_full:
        pushad
        mov ebp, [ven]
        HV ven_cli                  ; -> ECX=ancho EDX=alto del cliente
        mov [cli_an], ecx
        mov [cli_al], edx
        mov eax, ecx                ; cuántas veces cabe de ancho
        xor edx, edx
        mov ecx, JUEGO_AN
        div ecx
        mov ebx, eax
        mov eax, [cli_al]           ; y de alto
        xor edx, edx
        mov ecx, JUEGO_AL
        div ecx
        cmp eax, ebx                ; la menor de las dos
        jbe .min
        mov eax, ebx
.min:
        test eax, eax               ; al menos x1
        jnz .n1
        mov eax, 1
.n1:
        cmp eax, FULL_MAX           ; y como mucho FULL_MAX (lo que cabe en pant)
        jbe .n2
        mov eax, FULL_MAX
.n2:
        mov [escala], eax
        mov ebx, eax                ; centrar en horizontal
        imul ebx, JUEGO_AN
        mov ecx, [cli_an]
        sub ecx, ebx
        sar ecx, 1
        jns .cx
        xor ecx, ecx
.cx:
        mov [img_x], ecx
        mov ebx, eax                ; y en vertical
        imul ebx, JUEGO_AL
        mov ecx, [cli_al]
        sub ecx, ebx
        sar ecx, 1
        jns .cy
        xor ecx, ecx
.cy:
        mov [img_y], ecx
        popad
        ret

; ---- (interna, con el contexto abierto) pasa «imagen» a los colores de Hispa
;      Ventanas en «pant», repitiendo cada punto [escala] veces de ancho y de
;      alto (escala entera, cualquiera >= 1), y lo dibuja centrado (img_x,img_y)
img_copia_full:
        pushad
        mov esi, imagen
        mov edi, pant
        mov eax, [escala]
        mov [pc_n], eax
        mov ebx, JUEGO_AL           ; filas del cuadro de origen
.fila:
        mov edx, edi                ; principio de esta fila ya expandida
        mov ecx, JUEGO_AN
.px:
        movzx eax, byte [esi]
        mov al, [col_host+eax]      ; el color anfitrión de ese punto
        push ecx
        mov ecx, [pc_n]             ; repetido N veces de ancho
.rep:
        mov [edi], al
        inc edi
        dec ecx
        jnz .rep
        pop ecx
        inc esi
        dec ecx
        jnz .px
        mov eax, [pc_n]             ; la fila entera, N-1 veces más abajo
        dec eax
        jz .nofila
        push esi
        mov ecx, edi
        sub ecx, edx                ; longitud de la fila (JUEGO_AN*N octetos)
.dup:
        push ecx
        push esi
        mov esi, edx
        rep movsb                   ; copia edx->edi (edi sigue hacia abajo)
        pop esi
        pop ecx
        dec eax
        jnz .dup
        pop esi
.nofila:
        dec ebx
        jnz .fila
        mov eax, [img_x]            ; a la ventana, centrado
        mov ebx, [img_y]
        mov ecx, JUEGO_AN
        imul ecx, [pc_n]
        mov edx, JUEGO_AL
        imul edx, [pc_n]
        mov esi, pant
        HV dc_copiar
        popad
        ret
