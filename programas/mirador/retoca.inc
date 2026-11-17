; =============================================================================
;  MIRADOR.HVP - los retoques: recortar, cambiar de tamaño, a grises, invertir
;  y brillo y contraste; deshacer; y el modo «elegir» una zona con el ratón
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Mirador solo retoca (dibujar es cosa de Pintura). Un retoque se hace sobre
;  la imagen que hay en memoria (el mapa en la paleta y, si se guarda, el
;  original en color verdadero), no sobre el archivo: el resultado es una
;  imagen nueva, en bloques nuevos, y la de antes se queda guardada para
;  deshacer (un paso: Ctrl+Z la devuelve, y otra vez Ctrl+Z rehace el
;  retoque). La imagen queda «retocada» hasta que se guarda con «Guardar
;  como»; al pasar a otra o al cerrar se pregunta si se guarda.
;
;  Cómo: recortar copia los puntos tal cual (no se trama otra vez). Lo demás
;  pasa todas las filas por el mismo camino: reduce.inc las da al tamaño
;  nuevo (promediando al reducir) desde «fuente_fila» (el original, o los
;  colores del mapa si no se guarda el original); a cada punto se le aplica,
;  si toca, una tabla de 256 valores (invertir, brillo y contraste) o el paso
;  a gris; se guarda en el original nuevo y se lleva a la paleta con
;  apaleta.inc. Sin original, por tanto, un retoque parte de lo que se ve
;  (256 colores) y se trama otra vez.
;
;  A grises:   gris = (77·rojo + 150·verde + 29·azul) / 256
;  Invertir:   v' = 255 - v
;  Brillo (b) y contraste (c), los dos de -100 a 100:
;              v' = (v - 128)·(100 + c)/100 + 128 + b·255/100
;              (las divisiones, quitando decimales hacia el cero; el
;              resultado, entre 0 y 255)
;
;  El modo «elegir»: con el ratón se marca un rectángulo (un marco con
;  tiradores en las esquinas y en los lados). Arrastrando fuera se empieza
;  otro; por un borde o una esquina se cambia de tamaño; por dentro se mueve.
;  Intro recorta la imagen a lo elegido y Esc lo quita. El marco se guarda en
;  puntos de la imagen, así que vale con cualquier ampliación.
;
;  Una imagen que se redujo al leerla (más de 4 megapuntos) se retoca como
;  está en memoria: reducida. Desde el primer retoque sus medidas son esas.
; =============================================================================
DES_N       equ 11                  ; datos (de 32 bits) que se guardan para deshacer
EL_BORDE    equ 4                   ; a cuántos puntos de un borde del marco se coge
TAM_LADO    equ 32767               ; lo más que se admite al cambiar de tamaño
TAM_PUNTOS  equ 16 * 1024 * 1024

%macro RETOCA_BSS 0
            alignb 4
des_v       resd DES_N              ; la imagen de antes del último retoque (como rt_campos)
rt_mapa_p   resd 1                  ; la imagen que se está haciendo
rt_mapa_h   resd 1
rt_orig_p   resd 1
rt_orig_h   resd 1
rt_an       resd 1
rt_al       resd 1
rt_trab_h   resd 1
rt_brillo   resd 1
rt_contr    resd 1
el_x0       resd 1                  ; lo elegido, en puntos de la imagen (x1, y1: el siguiente al último)
el_y0       resd 1
el_x1       resd 1
el_y1       resd 1
el_ax       resd 1                  ; al arrastrar: el punto que no se mueve
el_ay       resd 1
el_dx       resd 1                  ; o, al mover el marco, de dónde se ha cogido
el_dy       resd 1
el_modo     resd 1                  ; 1: cambia la x; 2: la y; 4: se mueve entero; 8: es nuevo
el_cur      resd 1                  ; la forma del cursor puesta
el_sx0      resd 1                  ; el marco, en la ventana
el_sy0      resd 1
el_sx1      resd 1
el_sy1      resd 1
tam_v       resd 3                  ; lo escrito en el diálogo: tanto por ciento, ancho y alto
retocada    resb 1                  ; la imagen no es ya la del archivo
des_hay     resb 1                  ; hay algo que deshacer
des_orig    resb 1                  ; (de la imagen guardada) tiene original
des_retoc   resb 1                  ; (ídem) estaba retocada
rt_hay_orig resb 1
rt_con_lut  resb 1                  ; a cada punto se le aplica rt_lut
rt_gris     resb 1                  ; y antes se pasa a gris
elegir      resb 1                  ; el modo «elegir»
el_hay      resb 1                  ; hay un marco
tam_modo    resb 1                  ; 0: tanto por ciento; 1: medidas
tam_prop    resb 1                  ; conservar la proporción
dl_mal      resb 1                  ; lo escrito en un diálogo no vale
            alignb 4
rt_lut      resb 256
dl_t1       resb 16
dl_t2       resb 16
dl_t3       resb 16
tam_ahora   resb 64
rt_nom      resb 132
%endmacro

; lo que cambia de una imagen a otra (y se guarda para deshacer), en el orden de des_v
rt_campos   dd mapa_p, mapa_h, orig_p, orig_h, ma_an, ma_al, g_an, g_al, f_red, or_rot, or_volt
D_MAPA_P    equ 0
D_MAPA_H    equ 4
D_ORIG_P    equ 8
D_ORIG_H    equ 12
D_MA_AN     equ 16
D_MA_AL     equ 20
D_G_AN      equ 24
D_G_AL      equ 28
D_F_RED     equ 32
D_OR_ROT    equ 36
D_OR_VOLT   equ 40

t_retocada  db " (retocada)", 0
t_retocando db "Retocando la imagen...", 0
t_q_guardar db "La imagen está retocada y no se ha guardado.", 10
            db "¿Quiere guardarla?", 0
t_e_elegir  db "Primero elija la zona que quiere conservar:", 10
            db "Imagen > Elegir una zona (E) y arrastre el ratón sobre la imagen.", 0
t_el_pista  db "Elija la zona arrastrando el ratón sobre la imagen (Esc: dejarlo)", 0
t_el_hay    db "Elegido: ", 0
t_el_que    db "Intro recorta; Esc lo quita", 0
t_i_retoc   db "Retocada: los cambios aún no se han guardado.", 0
t_d_tam     db "Cambiar de tamaño", 0
t_tam_pct   db "A un tanto por ciento:", 0
t_tam_med   db "A estas medidas, en puntos:", 0
t_tam_an    db "Ancho:", 0
t_tam_al    db "Alto:", 0
t_tam_prop  db "Conservar la proporción", 0
t_tam_mide  db "Ahora mide ", 0
t_tam_p     db " puntos.", 0
t_e_tam_pct db "El tanto por ciento tiene que ser un número de 1 a 1000.", 0
t_e_tam_med db "Las medidas tienen que ser números de 1 a 32767.", 10
            db "(Si se conserva la proporción vale con escribir una de las dos.)", 0
t_e_tam_gr  db "Esas medidas son demasiado grandes para Mirador", 10
            db "(más de 16 millones de puntos).", 0
t_d_bri     db "Brillo y contraste", 0
t_bri_b     db "Brillo (de -100 a 100):", 0
t_bri_c     db "Contraste (de -100 a 100):", 0
t_bri_nota  db "Con 0 se queda como está.", 0
t_e_bri     db "El brillo y el contraste tienen que ser números de -100 a 100.", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_cien      db "100", 0
t_cero      db "0", 0

dlg_tam:
        PLANTILLA HV_c_dialogo, t_d_tam, E_DIALOGO, 0, 0, 320, 236, 0
        PLANTILLA HV_c_estatico, tam_ahora, E_HIJA, 16, 12, 290, 18, 100
        PLANTILLA HV_c_boton, t_tam_pct, E_HIJA | E_TABULA | BS_OPCION, 16, 40, 200, 20, 20
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 224, 38, 64, 24, 30
        PLANTILLA HV_c_boton, t_tam_med, E_HIJA | E_TABULA | BS_OPCION, 16, 72, 250, 20, 21
        PLANTILLA HV_c_estatico, t_tam_an, E_HIJA, 36, 102, 52, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 90, 98, 64, 24, 31
        PLANTILLA HV_c_estatico, t_tam_al, E_HIJA, 176, 102, 44, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 224, 98, 64, 24, 32
        PLANTILLA HV_c_boton, t_tam_prop, E_HIJA | E_TABULA | BS_CASILLA, 36, 132, 250, 20, 22
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 60, 168, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 160, 168, 90, 26, ID_CANCELAR
        dd 0
dlg_bri:
        PLANTILLA HV_c_dialogo, t_d_bri, E_DIALOGO, 0, 0, 340, 190, 0
        PLANTILLA HV_c_estatico, t_bri_b, E_HIJA, 16, 20, 220, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 244, 16, 64, 24, 30
        PLANTILLA HV_c_estatico, t_bri_c, E_HIJA, 16, 52, 220, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 244, 48, 64, 24, 31
        PLANTILLA HV_c_estatico, t_bri_nota, E_HIJA, 16, 86, 310, 18, 102
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 70, 120, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 170, 120, 90, 26, ID_CANCELAR
        dd 0

; =============================================================================
;  Deshacer
; =============================================================================
; (interna) cambia la imagen que se ve por la guardada, dato a dato
des_cambia:
        pushad
        xor ebx, ebx
.c:
        mov esi, [rt_campos+ebx*4]
        mov eax, [esi]
        xchg eax, [des_v+ebx*4]
        mov [esi], eax
        inc ebx
        cmp ebx, DES_N
        jb .c
        mov al, [hay_orig]
        xchg al, [des_orig]
        mov [hay_orig], al
        mov al, [retocada]
        xchg al, [des_retoc]
        mov [retocada], al
        popad
        ret

; suelta la imagen guardada para deshacer
des_suelta:
        cmp byte [des_hay], 0
        je .x
        pushad
        mov edx, [des_v+D_MAPA_H]
        HV mem_bloque_soltar
        cmp byte [des_orig], 0
        je .s1
        mov edx, [des_v+D_ORIG_H]
        HV mem_bloque_soltar
.s1:
        popad
        mov byte [des_hay], 0
.x:
        ret

; Ctrl+Z: la imagen de antes del último retoque (y otra vez, el retoque)
deshacer:
        cmp byte [des_hay], 0
        je .x
        pushad
        call des_cambia
        call rt_a_la_vista
        popad
.x:
        ret

; (interna) la imagen ha cambiado: fuera el marco y la vista reducida, y a pintar
rt_a_la_vista:
        call el_sal
        call suelta_vista
        mov dword [sx], 0
        mov dword [sy], 0
        call dispone
        jmp inval

; =============================================================================
;  El camino de un retoque
; =============================================================================
; (interna) ECX=ancho y EDX=alto de la imagen nueva: pide sus bloques (el
; original, solo si la de ahora lo tiene y cabe). Antes suelta lo que había
; para deshacer y la vista reducida: toda la memoria, para el retoque.
; CF=1 si no hay memoria (y lo dice)
rt_empieza:
        pushad
        mov [rt_an], ecx
        mov [rt_al], edx
        call an_para                ; (una animación se queda en el cuadro que se ve)
        call des_suelta
        call suelta_vista
        mov eax, ecx
        mul edx
        mov ebx, eax                ; EBX=puntos
        lea ecx, [eax+64]
        HV mem_bloque
        jc .mal
        mov [rt_mapa_p], eax
        mov [rt_mapa_h], edx
        mov byte [rt_hay_orig], 0
        cmp byte [hay_orig], 0
        je .bien
        lea ecx, [ebx+ebx*2]
        cmp ecx, ORIG_MAX
        ja .bien
        add ecx, 64
        HV mem_bloque
        jc .bien
        mov [rt_orig_p], eax
        mov [rt_orig_h], edx
        mov byte [rt_hay_orig], 1
.bien:
        popad
        clc
        ret
.mal:
        mov esi, t_e_mem_sal
        call aviso
        popad
        stc
        ret

; (interna) no ha podido ser: suelta los bloques de la imagen nueva
rt_suelta:
        pushad
        mov edx, [rt_mapa_h]
        HV mem_bloque_soltar
        cmp byte [rt_hay_orig], 0
        je .x
        mov edx, [rt_orig_h]
        HV mem_bloque_soltar
.x:
        popad
        ret

; (interna) la imagen nueva pasa a ser la que se ve; la de antes, la de deshacer
rt_acaba:
        pushad
        mov eax, [rt_mapa_p]
        mov [des_v+D_MAPA_P], eax
        mov eax, [rt_mapa_h]
        mov [des_v+D_MAPA_H], eax
        mov eax, [rt_orig_p]
        mov [des_v+D_ORIG_P], eax
        mov eax, [rt_orig_h]
        mov [des_v+D_ORIG_H], eax
        mov eax, [rt_an]
        mov [des_v+D_MA_AN], eax
        mov [des_v+D_G_AN], eax
        mov eax, [rt_al]
        mov [des_v+D_MA_AL], eax
        mov [des_v+D_G_AL], eax
        mov dword [des_v+D_F_RED], 1
        mov eax, [or_rot]
        mov [des_v+D_OR_ROT], eax
        mov eax, [or_volt]
        mov [des_v+D_OR_VOLT], eax
        mov al, [rt_hay_orig]
        mov [des_orig], al
        mov byte [des_retoc], 1
        call des_cambia
        mov byte [des_hay], 1
        call rt_a_la_vista
        popad
        ret

; ECX=ancho y EDX=alto nuevos (los de ahora, si solo cambian los colores):
; hace la imagen nueva pasando todas las filas por reduce.inc, por la tabla
; de colores (si [rt_con_lut]) y por apaleta.inc
rt_haz:
        pushad
        mov esi, t_retocando
        call pie_dice
        call rt_empieza
        jc .x
        ; la memoria de trabajo: los errores de la difusión, el reductor y una fila
        mov ecx, [rt_an]
        add ecx, 2
        imul ecx, 12
        mov eax, [ma_an]
        lea eax, [eax+eax*2]
        add ecx, eax
        mov eax, [rt_an]
        shl eax, 4
        add ecx, eax
        mov eax, [rt_an]
        lea eax, [eax+eax*2]
        lea ecx, [ecx+eax+160]
        HV mem_bloque
        jnc .hay
        call rt_suelta
        mov esi, t_e_mem_sal
        call aviso
        jmp .x
.hay:
        mov [rt_trab_h], edx
        mov edi, eax
        mov ecx, [rt_an]
        call ap_empieza
        mov eax, [trama]
        mov [ap_modo], eax
        lea eax, [ecx+2]
        imul eax, 12
        add edi, eax
        mov eax, [ma_an]
        mov ebx, [ma_al]
        mov edx, [rt_al]
        mov esi, fuente_fila
        call red_prepara
        mov eax, [ma_an]
        lea eax, [eax+eax*2]
        add edi, eax
        mov eax, [rt_an]
        shl eax, 4
        lea edi, [edi+eax+36]
        and edi, ~3                 ; EDI=una fila en rojo, verde y azul
        xor ebx, ebx                ; EBX=la fila
.f:
        cmp ebx, [rt_al]
        jae .f9
        mov eax, ebx
        call red_fila
        cmp byte [rt_con_lut], 0
        je .f1
        call rt_aplica
.f1:
        cmp byte [rt_hay_orig], 0
        je .f2
        push edi
        mov esi, edi
        mov eax, ebx
        mul dword [rt_an]
        lea edi, [eax+eax*2]
        add edi, [rt_orig_p]
        mov ecx, [rt_an]
        lea ecx, [ecx+ecx*2]
        rep movsb
        pop edi
.f2:
        push edi
        mov esi, edi
        mov eax, ebx
        mul dword [rt_an]
        mov edi, [rt_mapa_p]
        add edi, eax
        mov ecx, [rt_an]
        mov eax, ebx
        call ap_fila
        pop edi
        inc ebx
        jmp .f
.f9:
        mov edx, [rt_trab_h]
        HV mem_bloque_soltar
        call rt_acaba
.x:
        popad
        ret

; (interna) EDI=una fila de [rt_an] puntos (rojo, verde, azul): a gris si
; [rt_gris], y cada valor por la tabla rt_lut
rt_aplica:
        pushad
        mov ecx, [rt_an]
        xor eax, eax
        cmp byte [rt_gris], 0
        jne .gris
        lea ecx, [ecx+ecx*2]
.c:
        mov al, [edi]
        mov al, [rt_lut+eax]
        stosb
        dec ecx
        jnz .c
        popad
        ret
.gris:
        movzx eax, byte [edi]
        imul eax, 77
        movzx edx, byte [edi+1]
        imul edx, 150
        add eax, edx
        movzx edx, byte [edi+2]
        imul edx, 29
        add eax, edx
        shr eax, 8
        mov al, [rt_lut+eax]
        stosb
        stosb
        stosb
        dec ecx
        jnz .gris
        popad
        ret

; (interna) la tabla de colores: cada valor, él mismo
rt_lut_igual:
        push eax
        xor eax, eax
.c:
        mov [rt_lut+eax], al
        inc al
        jnz .c
        pop eax
        ret

; ---- Imagen > A grises ----
a_grises:
        call rt_lut_igual
        mov byte [rt_gris], 1
rt_colores:
        mov byte [rt_con_lut], 1
        mov ecx, [ma_an]
        mov edx, [ma_al]
        call rt_haz
        mov byte [rt_con_lut], 0
        mov byte [rt_gris], 0
        ret

; ---- Imagen > Invertir los colores ----
invertir:
        push eax
        xor eax, eax
.c:
        mov [rt_lut+eax], al
        not byte [rt_lut+eax]
        inc al
        jnz .c
        pop eax
        jmp rt_colores

; ---- Imagen > Brillo y contraste ----
brillo:
        pushad
.otra:
        mov ebp, [ven]
        mov esi, dlg_bri
        mov edi, dlg_bri + P_TAM
        mov ebx, bri_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov byte [dl_mal], 0
        mov esi, dl_t1
        call dl_numero
        mov [rt_brillo], eax
        call .vale
        mov esi, dl_t2
        call dl_numero
        mov [rt_contr], eax
        call .vale
        cmp byte [dl_mal], 0
        je .bien
        mov esi, t_e_bri
        call aviso
        jmp .otra
.bien:
        mov eax, [rt_brillo]        ; (cero y cero: nada que hacer)
        or eax, [rt_contr]
        jz .x
        call rt_lut_brillo
        call rt_colores
.x:
        popad
        ret
; (interna) EAX tiene que estar entre -100 y 100
.vale:
        add eax, 100
        cmp eax, 200
        jbe .v1
        mov byte [dl_mal], 1
.v1:
        ret

; (interna) la tabla de [rt_brillo] y [rt_contr]
rt_lut_brillo:
        pushad
        mov eax, [rt_brillo]
        imul eax, 255
        cdq
        mov ecx, 100
        idiv ecx
        mov edi, eax                ; EDI=lo que suma el brillo
        mov esi, [rt_contr]
        add esi, 100                ; ESI=el contraste, en centésimas
        xor ebx, ebx
.c:
        lea eax, [ebx-128]
        imul eax, esi
        cdq
        idiv ecx
        lea eax, [eax+edi+128]
        test eax, eax
        jns .c1
        xor eax, eax
.c1:
        cmp eax, 255
        jle .c2
        mov eax, 255
.c2:
        mov [rt_lut+ebx], al
        inc ebx
        cmp ebx, 256
        jb .c
        popad
        ret

bri_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 30
        mov edx, dl_t1
        call dl_lee
        mov ecx, 31
        mov edx, dl_t2
        call dl_lee
        popad
        jmp short .no
.ini:
        pushad
        mov ecx, 30
        mov edx, t_cero
        call dl_pon
        mov ecx, 31
        call dl_pon
        popad
.no:
        xor eax, eax
        ret

; (internas) el texto EDX del control ECX del diálogo EBP: ponerlo y leerlo
; (hasta 15 letras). Conservan los registros
dl_pon:
        pushad
        HV ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        HV msg_enviar
        mov eax, EM_SEL_TODO        ; (elegido entero: lo que se teclee lo sustituye)
        HV msg_enviar
        popad
        ret
dl_lee:
        pushad
        mov byte [edx], 0
        HV ven_hija_id
        mov ebp, eax
        mov eax, WM_GETTEXT
        mov ecx, 16
        HV msg_enviar
        popad
        ret
; la marca (EDX=0 o 1) del control ECX: ponerla; y leerla -> EAX
dl_marca:
        pushad
        HV ven_hija_id
        mov ebp, eax
        mov eax, BM_PONER_MARCA
        mov ecx, edx
        HV msg_enviar
        popad
        ret
dl_marcada:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        HV ven_hija_id
        mov ebp, eax
        mov eax, BM_MARCA
        HV msg_enviar
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; (interna) ESI=un texto: el número que dice (con su signo) -> EAX. Si no es
; un número (o está vacío) pone [dl_mal] y da 0
dl_numero:
        push ebx
        push ecx
        push esi
        xor eax, eax
        xor ecx, ecx                ; ECX=1: negativo
.e:
        cmp byte [esi], ' '
        jne .s
        inc esi
        jmp .e
.s:
        cmp byte [esi], '-'
        jne .s1
        inc ecx
        inc esi
.s1:
        cmp byte [esi], '+'
        jne .s2
        inc esi
.s2:
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .mal                     ; ni una cifra
.d:
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .fin
        cmp eax, 100000000          ; (un número absurdo no da la vuelta)
        jae .mal
        imul eax, 10
        add eax, ebx
        inc esi
        jmp .d
.fin:
        cmp byte [esi], ' '         ; detrás, solo espacios
        jne .f1
        inc esi
        jmp .fin
.f1:
        cmp byte [esi], 0
        jne .mal
        test ecx, ecx
        jz .x
        neg eax
        jmp .x
.mal:
        mov byte [dl_mal], 1
        xor eax, eax
.x:
        pop esi
        pop ecx
        pop ebx
        ret
; (interna) ESI=un texto: ¿está vacío (o son solo espacios)? -> ZF=1 si sí
dl_vacio:
        push esi
.e:
        cmp byte [esi], ' '
        jne .f
        inc esi
        jmp .e
.f:
        cmp byte [esi], 0
        pop esi
        ret

; ---- Imagen > Cambiar de tamaño ----
cambia_tamano:
        pushad
.otra:
        mov edi, tam_ahora
        mov esi, t_tam_mide
        call hvl_cad
        mov eax, [ma_an]
        call hvl_num
        mov esi, t_x
        call hvl_cad
        mov eax, [ma_al]
        call hvl_num
        mov esi, t_tam_p
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, dlg_tam
        mov edi, dlg_tam + P_TAM
        mov ebx, tam_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov byte [dl_mal], 0
        cmp byte [tam_modo], 0
        jne .medidas
        ; ---- a un tanto por ciento ----
        mov esi, dl_t1
        call dl_numero
        dec eax
        cmp eax, 1000
        jb .p1
        mov byte [dl_mal], 1
.p1:
        inc eax
        mov esi, t_e_tam_pct
        cmp byte [dl_mal], 0
        jne .dice
        mov ebx, eax                ; EBX=el tanto por ciento
        mov eax, [ma_an]
        call .pct
        mov ecx, eax
        mov eax, [ma_al]
        call .pct
        mov edx, eax
        jmp .hazlo
.medidas:
        ; ---- a unas medidas ----
        xor ecx, ecx                ; ECX=ancho EDX=alto (0: no se ha escrito)
        xor edx, edx
        mov esi, dl_t2
        call dl_vacio
        je .m1
        call dl_numero
        mov ecx, eax
        dec eax
        cmp eax, TAM_LADO
        jb .m1
        mov byte [dl_mal], 1
.m1:
        mov esi, dl_t3
        call dl_vacio
        je .m2
        call dl_numero
        mov edx, eax
        dec eax
        cmp eax, TAM_LADO
        jb .m2
        mov byte [dl_mal], 1
.m2:
        mov esi, t_e_tam_med
        cmp byte [dl_mal], 0
        jne .dice
        cmp byte [tam_prop], 0
        jne .prop
        test ecx, ecx               ; sin proporción hacen falta las dos
        jz .dice
        test edx, edx
        jz .dice
        jmp .hazlo
.prop:
        ; la mayor que quepa en esas medidas sin deformarse (con una sola
        ; medida, manda esa)
        mov eax, ecx
        or eax, edx
        jz .dice
        test edx, edx
        jz .ancho
        test ecx, ecx
        jz .alto
        mov eax, ecx                ; ¿cuál aprieta? ancho·alto de ahora <= alto·ancho de ahora
        push edx
        mul dword [ma_al]
        mov ebx, eax
        mov edi, edx                ; EDI:EBX
        pop edx
        push edx
        mov eax, edx
        mul dword [ma_an]           ; EDX:EAX
        cmp edi, edx
        jne .p2
        cmp ebx, eax
.p2:
        pop edx
        ja .alto
.ancho:
        mov eax, [ma_al]            ; alto = alto de ahora · ancho / ancho de ahora
        mov ebx, [ma_an]
        call .regla
        mov edx, eax
        jmp .hazlo
.alto:
        push edx
        mov eax, [ma_an]
        mov ecx, edx
        mov ebx, [ma_al]
        call .regla
        pop edx
        mov ecx, eax
.hazlo:
        ; ECX, EDX=las medidas nuevas
        mov esi, t_e_tam_gr
        cmp ecx, TAM_LADO
        ja .dice
        cmp edx, TAM_LADO
        ja .dice
        mov eax, ecx
        push edx
        mul edx
        test edx, edx
        pop edx
        jnz .dice
        cmp eax, TAM_PUNTOS
        ja .dice
        cmp ecx, [ma_an]            ; (las mismas: nada que hacer)
        jne .h1
        cmp edx, [ma_al]
        je .x
.h1:
        call rt_haz
        jmp .x
.dice:
        call aviso
        jmp .otra
.x:
        popad
        ret
; (interna) EAX=una medida EBX=tanto por ciento -> EAX (redondeada; una al menos)
.pct:
        push edx
        push ecx
        mul ebx
        add eax, 50
        adc edx, 0
        mov ecx, 100
        div ecx
        pop ecx
        pop edx
        jmp short .una
; (interna) EAX·ECX/EBX, redondeado; uno al menos
.regla:
        push edx
        mul ecx
        push ebx
        shr ebx, 1
        add eax, ebx
        adc edx, 0
        pop ebx
        div ebx
        pop edx
.una:
        test eax, eax
        jnz .u1
        inc eax
.u1:
        ret

tam_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 30
        mov edx, dl_t1
        call dl_lee
        mov ecx, 31
        mov edx, dl_t2
        call dl_lee
        mov ecx, 32
        mov edx, dl_t3
        call dl_lee
        mov ecx, 21
        call dl_marcada
        mov [tam_modo], al
        mov ecx, 22
        call dl_marcada
        mov [tam_prop], al
        popad
        jmp short .no
.ini:
        pushad
        mov ecx, 30
        mov edx, t_cien
        call dl_pon
        mov edi, dl_t2
        mov eax, [ma_an]
        call hvl_num
        mov byte [edi], 0
        mov ecx, 31
        mov edx, dl_t2
        call dl_pon
        mov edi, dl_t3
        mov eax, [ma_al]
        call hvl_num
        mov byte [edi], 0
        mov ecx, 32
        mov edx, dl_t3
        call dl_pon
        mov ecx, 20
        mov edx, 1
        call dl_marca
        mov ecx, 22
        call dl_marca
        popad
.no:
        xor eax, eax
        ret

; ---- Imagen > Recortar a lo elegido ----
recortar:
        pushad
        cmp byte [el_hay], 0
        jne .hay
        mov esi, t_e_elegir
        call aviso
        jmp .x
.hay:
        mov ecx, [el_x1]
        sub ecx, [el_x0]
        mov edx, [el_y1]
        sub edx, [el_y0]
        cmp ecx, [ma_an]            ; (toda la imagen: nada que recortar)
        jne .r1
        cmp edx, [ma_al]
        jne .r1
        call el_sal
        call inval
        jmp .x
.r1:
        call rt_empieza
        jc .x
        xor ebx, ebx                ; EBX=la fila nueva
.f:
        cmp ebx, [rt_al]
        jae .f9
        mov eax, [el_y0]
        add eax, ebx
        mul dword [ma_an]
        add eax, [el_x0]
        mov ebp, eax                ; EBP=su primer punto, en la imagen de ahora
        mov esi, [mapa_p]
        add esi, eax
        mov eax, ebx
        mul dword [rt_an]
        mov edx, eax
        mov edi, [rt_mapa_p]
        add edi, eax
        mov ecx, [rt_an]
        rep movsb
        cmp byte [rt_hay_orig], 0
        je .f1
        lea esi, [ebp+ebp*2]
        add esi, [orig_p]
        lea edi, [edx+edx*2]
        add edi, [rt_orig_p]
        mov ecx, [rt_an]
        lea ecx, [ecx+ecx*2]
        rep movsb
.f1:
        inc ebx
        jmp .f
.f9:
        call rt_acaba
.x:
        popad
        ret

; =============================================================================
;  Con la imagen retocada
; =============================================================================
; antes de perder la imagen (se abre otra, se cierra): si está retocada,
; pregunta si se guarda. CF=1 si no hay que seguir (se ha dicho «Cancelar» o
; no se ha podido guardar). Conserva nom_tmp
confirma_retoque:
        cmp byte [retocada], 0
        jne .hay
        clc
        ret
.hay:
        pushad
        call pase_para              ; (un diálogo: en la ventana y con el pase parado)
        cmp byte [pantc], 0
        je .d
        call completa_sal
.d:
        mov ebp, [ven]
        mov esi, t_q_guardar
        mov edi, t_mirador
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_NO
        je .bien
        cmp eax, ID_SI
        jne .no
        mov esi, nom_tmp
        mov edi, rt_nom
        mov ecx, 132
        rep movsb
        call rt_formato             ; ECX=con qué orden se guarda
        call guarda_haz
        pushf
        mov esi, rt_nom
        mov edi, nom_tmp
        mov ecx, 132
        rep movsb
        popf
        jc .no
.bien:
        mov byte [retocada], 0
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; (interna) -> ECX=la orden de guardar que le va al archivo que se ve: como
; BMP o PCX si lo es; si no, como PNG (que no pierde nada y ocupa menos)
rt_formato:
        push eax
        push esi
        mov esi, [nom_corto]
.b:
        lodsb
        test al, al
        jz .png
        cmp al, '.'
        jne .b
        mov eax, [esi]
        mov ecx, O_GUARDAR
        cmp eax, 'BMP'
        je .x
        mov ecx, O_GUARDA_PCX
        cmp eax, 'PCX'
        je .x
.png:
        mov ecx, O_GUARDA_PNG
.x:
        pop esi
        pop eax
        ret

; han cambiado los colores (menú Ver) con la imagen retocada: no se puede
; leer otra vez del archivo. Si se guarda el original, el mapa se hace otra
; vez desde él, en el sitio; si no, se queda como está
remapa:
        pushad
        cmp byte [hay_orig], 0
        je .x
        mov ecx, [ma_an]
        lea ecx, [ecx+2]
        imul ecx, 12
        add ecx, 64
        HV mem_bloque
        jc .x
        mov [rt_trab_h], edx
        mov edi, eax
        mov ecx, [ma_an]
        call ap_empieza
        mov eax, [trama]
        mov [ap_modo], eax
        mov esi, [orig_p]
        mov edi, [mapa_p]
        xor eax, eax
.f:
        cmp eax, [ma_al]
        jae .f9
        call ap_fila
        lea esi, [esi+ecx*2]
        add esi, ecx
        add edi, ecx
        inc eax
        jmp .f
.f9:
        mov edx, [rt_trab_h]
        HV mem_bloque_soltar
        call suelta_vista
        mov byte [sucio], 1
        call inval
.x:
        popad
        ret

; =============================================================================
;  El modo «elegir»
; =============================================================================
elegir_alterna:
        cmp byte [elegir], 0
        jne el_fuera
        mov byte [elegir], 1
        mov byte [el_hay], 0
        jmp inval
el_fuera:
        call el_sal
        jmp inval
; sale del modo (si estaba) y quita el marco
el_sal:
        cmp byte [elegir], 0
        je .x
        mov byte [elegir], 0
        push eax
        xor eax, eax
        call el_cursor
        pop eax
.x:
        mov byte [el_hay], 0
        ret
; Ctrl+E: toda la imagen, elegida
elegir_todo:
        push eax
        mov byte [elegir], 1
        xor eax, eax
        mov [el_x0], eax
        mov [el_y0], eax
        mov eax, [ma_an]
        mov [el_x1], eax
        mov eax, [ma_al]
        mov [el_y1], eax
        mov byte [el_hay], 1
        pop eax
        jmp inval

; (interna) EAX=CUR_*: la forma del cursor, si no es la que ya está
el_cursor:
        cmp eax, [el_cur]
        je .x
        mov [el_cur], eax
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .c1
        HV cur_forma
.c1:
        popad
.x:
        ret

; (interna) EAX=x EBX=y de la ventana -> EAX, EBX=el punto de la imagen más
; cercano (de 0 al ancho y de 0 al alto: es una frontera entre puntos)
el_a_imagen:
        push ecx
        push edx
        sub eax, [ox]
        add eax, [sx]
        mov ecx, [ma_an]
        call .uno
        xchg eax, ebx
        sub eax, [vy]
        sub eax, [oy]
        add eax, [sy]
        mov ecx, [ma_al]
        call .uno
        xchg eax, ebx
        pop edx
        pop ecx
        ret
.uno:
        test eax, eax
        jg .u1
        xor eax, eax
        ret
.u1:
        mov edx, eax                ; EDX:EAX = puntos de pantalla x 65536, más media ampliación
        shr edx, 16
        shl eax, 16
        push ebx
        mov ebx, [zoom]
        shr ebx, 1
        add eax, ebx
        adc edx, 0
        pop ebx
        cmp edx, [zoom]
        jae .tope
        div dword [zoom]
        cmp eax, ecx
        jbe .u2
.tope:
        mov eax, ecx
.u2:
        ret

; (interna) el marco, en puntos de la ventana: el_sx0, el_sy0 (el primero) y
; el_sx1, el_sy1 (el siguiente al último)
el_caja:
        pushad
        mov eax, [el_x0]
        call a_pantalla0
        add eax, [ox]
        sub eax, [sx]
        mov [el_sx0], eax
        mov ebx, eax
        mov eax, [el_x1]
        call a_pantalla0
        add eax, [ox]
        sub eax, [sx]
        cmp eax, ebx
        jg .a
        lea eax, [ebx+1]
.a:
        mov [el_sx1], eax
        mov eax, [el_y0]
        call a_pantalla0
        add eax, [vy]
        add eax, [oy]
        sub eax, [sy]
        mov [el_sy0], eax
        mov ebx, eax
        mov eax, [el_y1]
        call a_pantalla0
        add eax, [vy]
        add eax, [oy]
        sub eax, [sy]
        cmp eax, ebx
        jg .b
        lea eax, [ebx+1]
.b:
        mov [el_sy1], eax
        popad
        ret
; (interna) EAX=puntos de la imagen -> los de la pantalla con la ampliación de ahora
a_pantalla0:
        push edx
        mul dword [zoom]
        shrd eax, edx, 16
        pop edx
        ret

; (interna) EAX=x EBX=y de la ventana: ¿qué se toca del marco? -> EAX: 0,
; fuera; 16, dentro; o los bordes: 1 el izquierdo, 2 el derecho, 4 el de
; arriba, 8 el de abajo (una esquina, dos a la vez)
el_toca:
        push ecx
        push edx
        call el_caja
        xor ecx, ecx
        mov edx, [el_sx0]
        sub edx, EL_BORDE
        cmp eax, edx
        jl .fuera
        mov edx, [el_sx1]
        add edx, EL_BORDE - 1
        cmp eax, edx
        jg .fuera
        mov edx, [el_sy0]
        sub edx, EL_BORDE
        cmp ebx, edx
        jl .fuera
        mov edx, [el_sy1]
        add edx, EL_BORDE - 1
        cmp ebx, edx
        jg .fuera
        mov edx, [el_sx1]           ; el borde derecho (antes que el izquierdo: en un
        sub edx, EL_BORDE + 1       ; marco muy pequeño, que se pueda agrandar)
        cmp eax, edx
        jl .t1
        or ecx, 2
        jmp .t2
.t1:
        mov edx, [el_sx0]
        add edx, EL_BORDE
        cmp eax, edx
        jg .t2
        or ecx, 1
.t2:
        mov edx, [el_sy1]
        sub edx, EL_BORDE + 1
        cmp ebx, edx
        jl .t3
        or ecx, 8
        jmp .t4
.t3:
        mov edx, [el_sy0]
        add edx, EL_BORDE
        cmp ebx, edx
        jg .t4
        or ecx, 4
.t4:
        test ecx, ecx
        jnz .x
        mov ecx, 16
        jmp .x
.fuera:
        xor ecx, ecx
.x:
        mov eax, ecx
        pop edx
        pop ecx
        ret

; el ratón se mueve sin arrastrar (EDX=x + y·65536): el cursor dice qué se
; cogería. Conserva los registros
el_sobre:
        pushad
        xor ecx, ecx
        cmp byte [el_hay], 0
        je .pon
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, [vy]               ; (solo sobre la zona de la imagen)
        jl .pon
        mov esi, [vy]
        add esi, [vh]
        cmp ebx, esi
        jge .pon
        cmp eax, [vw]
        jge .pon
        call el_toca
        and eax, 15
        movzx ecx, byte [el_formas+eax]
.pon:
        mov eax, ecx
        call el_cursor
        popad
        ret
; la forma del cursor según los bordes que se tocan
el_formas   db CUR_FLECHA, CUR_HORIZ, CUR_HORIZ, CUR_HORIZ
            db CUR_VERT, CUR_DIAG1, CUR_DIAG2, CUR_VERT
            db CUR_VERT, CUR_DIAG2, CUR_DIAG1, CUR_VERT
            db CUR_VERT, CUR_VERT, CUR_VERT, CUR_VERT

; se aprieta el botón sobre la imagen (EAX=x EBX=y de la ventana)
el_raton:
        pushad
        mov dword [el_modo], 0
        cmp byte [el_hay], 0
        je .nuevo
        push eax
        call el_toca
        mov ecx, eax
        pop eax
        test ecx, ecx
        jz .nuevo
        cmp ecx, 16
        je .mueve
        test cl, 1                  ; un borde o una esquina: el lado de enfrente no se mueve
        jz .b1
        mov edx, [el_x1]
        mov [el_ax], edx
        or dword [el_modo], 1
.b1:
        test cl, 2
        jz .b2
        mov edx, [el_x0]
        mov [el_ax], edx
        or dword [el_modo], 1
.b2:
        test cl, 4
        jz .b3
        mov edx, [el_y1]
        mov [el_ay], edx
        or dword [el_modo], 2
.b3:
        test cl, 8
        jz .coge
        mov edx, [el_y0]
        mov [el_ay], edx
        or dword [el_modo], 2
        jmp .coge
.mueve:
        call el_a_imagen
        sub eax, [el_x0]
        mov [el_dx], eax
        sub ebx, [el_y0]
        mov [el_dy], ebx
        mov dword [el_modo], 4
        jmp .coge
.nuevo:
        call el_a_imagen
        mov [el_ax], eax
        mov [el_ay], ebx
        mov dword [el_modo], 1 | 2 | 8
        mov byte [el_hay], 0
        call inval
.coge:
        mov byte [arrastra], 2
        mov ebp, [ven]
        HV ven_capturar
        popad
        ret

; el ratón se mueve arrastrando (EDX=x + y·65536)
el_mueve:
        pushad
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        call el_a_imagen
        test dword [el_modo], 4
        jz .borde
        ; ---- el marco entero, sin salirse de la imagen ----
        mov ecx, [el_x1]
        sub ecx, [el_x0]            ; ECX=su ancho
        sub eax, [el_dx]
        jns .m1
        xor eax, eax
.m1:
        mov edx, [ma_an]
        sub edx, ecx
        cmp eax, edx
        jle .m2
        mov eax, edx
.m2:
        mov [el_x0], eax
        add eax, ecx
        mov [el_x1], eax
        mov ecx, [el_y1]
        sub ecx, [el_y0]
        sub ebx, [el_dy]
        jns .m3
        xor ebx, ebx
.m3:
        mov edx, [ma_al]
        sub edx, ecx
        cmp ebx, edx
        jle .m4
        mov ebx, edx
.m4:
        mov [el_y0], ebx
        add ebx, ecx
        mov [el_y1], ebx
        jmp .hecho
.borde:
        test dword [el_modo], 1
        jz .y
        mov ecx, [el_ax]            ; del punto fijo al ratón, en el orden que sea
        cmp eax, ecx
        jbe .x1
        xchg eax, ecx
.x1:
        mov [el_x0], eax
        mov [el_x1], ecx
.y:
        test dword [el_modo], 2
        jz .mide
        mov ecx, [el_ay]
        cmp ebx, ecx
        jbe .y1
        xchg ebx, ecx
.y1:
        mov [el_y0], ebx
        mov [el_y1], ecx
.mide:
        ; sin ancho o sin alto: uno nuevo aún no existe; uno que ya estaba
        ; se queda con un punto
        mov eax, [el_x1]
        cmp eax, [el_x0]
        jne .a1
        test dword [el_modo], 8
        jnz .nada
        cmp eax, [ma_an]
        jb .a0
        dec dword [el_x0]
        jmp .a1
.a0:
        inc dword [el_x1]
.a1:
        mov eax, [el_y1]
        cmp eax, [el_y0]
        jne .hecho
        test dword [el_modo], 8
        jnz .nada
        cmp eax, [ma_al]
        jb .a2
        dec dword [el_y0]
        jmp .hecho
.a2:
        inc dword [el_y1]
.hecho:
        mov byte [el_hay], 1
        call inval
        popad
        ret
.nada:
        mov byte [el_hay], 0
        call inval
        popad
        ret

; pinta el marco y sus tiradores encima de la imagen (con el contexto abierto)
el_pinta:
        cmp byte [el_hay], 0
        je .x
        pushad
        call el_caja
        mov eax, [ox]               ; solo sobre la imagen
        mov ebx, [vy]
        add ebx, [oy]
        mov ecx, [vis_an]
        mov edx, [vis_al]
        HV dc_cortar
        mov eax, [el_sx0]
        mov ebx, [el_sy0]
        mov ecx, [el_sx1]
        sub ecx, eax
        mov edx, [el_sy1]
        sub edx, ebx
        COLOR C_TINTA               ; una raya negra entre dos blancas: se ve sobre cualquier fondo
        HV dc_marco
        COLOR C_BLANCO
        dec eax
        dec ebx
        add ecx, 2
        add edx, 2
        HV dc_marco
        add eax, 2
        add ebx, 2
        sub ecx, 4
        sub edx, 4
        cmp ecx, 0
        jle .t
        cmp edx, 0
        jle .t
        HV dc_marco
.t:
        ; los tiradores: en las esquinas y en mitad de cada lado
        xor esi, esi                ; ESI=fila de tiradores (0 arriba, 1 en medio, 2 abajo)
.tf:
        xor edi, edi
.tc:
        cmp esi, 1                  ; (en el centro no hay)
        jne .t1
        cmp edi, 1
        je .t9
.t1:
        mov eax, [el_sx0]
        mov ecx, [el_sx1]
        dec ecx
        call .punto
        push eax
        mov eax, [el_sy0]
        mov ecx, [el_sy1]
        dec ecx
        xchg esi, edi
        call .punto
        xchg esi, edi
        mov ebx, eax
        pop eax
        sub eax, 3
        sub ebx, 3
        mov ecx, 7
        mov edx, 7
        COLOR C_BLANCO
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
.t9:
        inc edi
        cmp edi, 3
        jb .tc
        inc esi
        cmp esi, 3
        jb .tf
        popad
.x:
        ret
; (interna) EAX=el principio ECX=el final EDI=0, 1 o 2 -> EAX=el principio,
; la mitad o el final
.punto:
        cmp edi, 1
        jb .p9
        je .p1
        mov eax, ecx
        ret
.p1:
        add eax, ecx
        sar eax, 1
.p9:
        ret
