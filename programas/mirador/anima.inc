; =============================================================================
;  MIRADOR.HVP - las animaciones (GIF): tenerlas en memoria y pasarlas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Al abrir un GIF con más de un cuadro, y con el archivo todavía en memoria,
;  se le piden a gif.inc todos los cuadros ya compuestos y cada uno se lleva
;  a la paleta en un bloque: «cuadros x ancho x alto» octetos y, detrás, los
;  milisegundos de cada uno. Pasar la animación es entonces cambiar el mapa
;  que se pinta (mapa_p apunta al cuadro de ahora): no se descomprime nada
;  mientras se ve, así que va igual de bien en un 386. Si el bloque no cabe
;  (o la imagen se redujo al leerla), se queda en el primer cuadro y la
;  franja del aviso lo dice.
;
;  Los cuadros se pasan a la paleta con el tramado ordenado aunque esté
;  puesta la difusión de error: la difusión cambia de un cuadro a otro en las
;  zonas que no se han movido (hormiguea); el ordenado es siempre el mismo.
;  No se guarda el original en color verdadero: una animación ocupa ya mucho.
;
;  El tiempo lo lleva un temporizador (cada tic de reloj, 55 ms) que mira
;  cuánto ha pasado de verdad: si el equipo va lento se salta cuadros en vez
;  de retrasarse. Repite las veces que diga el archivo (NETSCAPE2.0: 0, sin
;  fin; N, N veces más; si no lo dice, se ve una vez) y se queda en el último
;  cuadro. Espacio la para y la sigue (acabada, la empieza otra vez); en el
;  pase de diapositivas, la pausa del pase la para también.
;
;  Girar, voltear o retocar una animación la deja en el cuadro que se ve
;  («an_para»), como una imagen normal. Guardar e imprimir toman ese cuadro.
; =============================================================================
ID_RELOJ_ANIMA equ 3
AN_TIC_MS   equ 55                  ; lo que dura un tic del reloj

%macro ANIMA_BSS 0
            alignb 4
an_n        resd 1                  ; cuadros en memoria (0: no hay animación)
an_k        resd 1                  ; el que se ve
an_p        resd 1                  ; el bloque de los cuadros
an_h        resd 1
an_tam      resd 1                  ; octetos de un cuadro
an_ms       resd 1                  ; los milisegundos de cada uno
an_tic      resd 1                  ; el reloj la última vez que se miró
an_debe     resd 1                  ; milisegundos que lleva a la vista el cuadro de ahora
an_vueltas  resd 1                  ; vueltas que quedan (-1: sin fin)
an_trab_h   resd 1
an_fila     resd 1
f_cuadros   resd 1                  ; lo que dice el archivo: cuadros, duración de una vuelta
f_dura      resd 1                  ; (milisegundos) y repeticiones
f_bucle     resd 1
an_pausa    resb 1
an_fin      resb 1                  ; ha dado todas sus vueltas
an_no       resb 1                  ; es una animación, pero no se ha podido cargar
            alignb 4
an_buf      resb 100
%endmacro

t_an_aviso1 db "Aviso: animación de ", 0
t_an_aviso2 db " cuadros; no hay memoria para todos y se ve solo el primero.", 0
t_an_pie    db "animación (Espacio: pausa)", 0
t_an_pausa  db "en pausa: cuadro ", 0
t_an_pausa2 db " (Espacio)", 0
t_an_fin    db "animación acabada (Espacio: otra vez)", 0
t_i_anima   db "Animación: ", 0
t_i_cuadros db " cuadros; una vuelta dura ", 0
t_i_seg     db " segundos;", 10, 0
t_i_sinfin  db "se repite sin fin.", 0
t_i_unavez  db "se ve una sola vez.", 0
t_i_veces1  db "se ve ", 0
t_i_veces2  db " veces.", 0
t_i_an_no   db " No cabe en la memoria: se ve el primer cuadro.", 0

; recién leída una imagen (con el archivo aún en memoria): si es una
; animación, carga sus cuadros y la pone en marcha
an_carga:
        pushad
        mov eax, [im_cuadros]
        mov [f_cuadros], eax
        mov eax, [im_dura]
        mov [f_dura], eax
        mov eax, [im_bucle]
        mov [f_bucle], eax
        mov byte [an_no], 0
        cmp dword [im_cuadros], 2
        jb .x
        cmp dword [f_red], 1        ; reducida al leerla: es demasiado grande
        ja .no
        mov eax, [ma_an]
        mul dword [ma_al]
        mov [an_tam], eax
        mov ecx, [im_cuadros]
        add eax, 4                  ; (y sus milisegundos)
        mul ecx
        test edx, edx
        jnz .no
        add eax, 64
        jc .no
        mov ecx, eax
        HV mem_bloque
        jc .no
        mov [an_p], eax
        mov [an_h], edx
        mov eax, [an_tam]
        mul dword [im_cuadros]
        add eax, [an_p]
        add eax, 3
        and eax, ~3
        mov [an_ms], eax
        ; la memoria de trabajo: la de gif.inc y una fila
        call gif_an_mem
        mov ecx, [ma_an]
        lea ecx, [ecx+ecx*2]
        lea ecx, [ecx+eax+128]
        HV mem_bloque
        jc .no_cuadros
        mov [an_trab_h], edx
        mov edi, eax
        mov ebx, eax
        call gif_an_mem
        lea ebx, [ebx+eax+16]
        and ebx, ~3
        mov [an_fila], ebx
        ; con todo eso pedido tiene que quedar sitio para enseñarla
        mov ecx, [vb_an]
        imul ecx, [vb_al]
        add ecx, 4096
        HV mem_bloque
        jc .no_trab
        HV mem_bloque_soltar
        ; (las rutinas de gif.inc cambian todos los registros: lo que dan, por la memoria)
        pushad
        mov edx, gif_an_empieza     ; (EDI=la memoria de trabajo)
        call nuc
        popad
        xor ebp, ebp                ; EBP=el cuadro
.cuadro:
        cmp ebp, [im_cuadros]
        jae .hechos
        pushad
        mov edx, gif_an_cuadro
        call nuc
        mov [an_debe], eax
        popad                       ; (no toca el acarreo)
        jc .hechos
        mov eax, [an_debe]
        mov ebx, [an_ms]
        mov [ebx+ebp*4], eax
        xor edi, edi                ; sin difusión: no hace falta memoria para los errores
        mov ecx, [ma_an]
        call ap_empieza
        mov eax, [trama]
        cmp eax, AP_DIFUSION
        jne .modo
        mov eax, AP_ORDENADO
.modo:
        mov [ap_modo], eax
        xor ebx, ebx                ; EBX=la fila
.fila:
        cmp ebx, [ma_al]
        jae .otro
        pushad
        mov eax, ebx
        mov edi, [an_fila]
        mov edx, gif_an_fila
        call nuc
        popad
        mov eax, ebp
        mul dword [an_tam]
        mov edi, eax
        mov eax, ebx
        mul dword [ma_an]
        add edi, eax
        add edi, [an_p]
        mov esi, [an_fila]
        mov ecx, [ma_an]
        mov eax, ebx
        call ap_fila
        inc ebx
        jmp .fila
.otro:
        inc ebp
        jmp .cuadro
.hechos:
        mov edx, [an_trab_h]
        HV mem_bloque_soltar
        cmp ebp, 2                  ; (el segundo estaba dañado: es una imagen sin más)
        jae .vale
        mov edx, [an_h]
        HV mem_bloque_soltar
        jmp .x
.vale:
        ; ---- la imagen que se ve pasa a ser el primer cuadro del bloque ----
        mov edx, [mapa_h]
        HV mem_bloque_soltar
        cmp byte [hay_orig], 0
        je .v1
        mov edx, [orig_h]
        HV mem_bloque_soltar
        mov byte [hay_orig], 0
.v1:
        mov eax, [an_p]
        mov [mapa_p], eax
        mov eax, [an_h]
        mov [mapa_h], eax
        mov [an_n], ebp
        call an_desde_cero
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov ecx, ID_RELOJ_ANIMA
        mov edx, AN_TIC_MS
        HV tmp_poner
        jmp .x
.no_trab:
        mov edx, [an_trab_h]
        HV mem_bloque_soltar
.no_cuadros:
        mov edx, [an_h]
        HV mem_bloque_soltar
.no:
        mov byte [an_no], 1
        cmp dword [av_txt], 0       ; (si ya hay otro aviso, ese)
        jne .x
        mov edi, an_buf
        mov esi, t_an_aviso1
        call hvl_cad
        mov eax, [im_cuadros]
        call hvl_num
        mov esi, t_an_aviso2
        call hvl_cad
        mov byte [edi], 0
        mov dword [av_txt], an_buf
.x:
        popad
        ret

; (interna) la animación, en su primer cuadro y con todas sus vueltas por dar
an_desde_cero:
        push eax
        xor eax, eax
        mov [an_k], eax
        mov [an_debe], eax
        mov [an_pausa], al
        mov [an_fin], al
        mov eax, [f_bucle]          ; -1 (no lo dice): una vez; 0: sin fin; N: N veces más
        inc eax
        jnz .v1
        mov eax, 1
        jmp .v2
.v1:
        cmp eax, 1
        jne .v2
        mov eax, -1
.v2:
        mov [an_vueltas], eax
        HV tics
        mov [an_tic], eax
        pop eax
        ret

; la imagen se suelta (su bloque lo suelta quien llama: es el del mapa)
an_olvida:
        cmp dword [an_n], 0
        je .x
        mov dword [an_n], 0
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .o1
        mov ecx, ID_RELOJ_ANIMA
        HV tmp_quitar
.o1:
        popad
.x:
        mov byte [an_no], 0
        mov dword [f_cuadros], 0
        ret

; deja la animación en el cuadro que se ve, como una imagen normal: el
; cuadro pasa a un bloque suyo (si no hay memoria ni para eso, se queda al
; principio del bloque grande)
an_para:
        cmp dword [an_n], 0
        je .x
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .p0
        mov ecx, ID_RELOJ_ANIMA
        HV tmp_quitar
.p0:
        mov dword [an_n], 0
        mov ecx, [an_tam]
        add ecx, 64
        HV mem_bloque
        jc .sitio
        mov edi, eax
        mov ebx, edx
        mov esi, [mapa_p]
        mov ecx, [an_tam]
        push edi
        rep movsb
        pop dword [mapa_p]
        mov edx, [an_h]
        HV mem_bloque_soltar
        mov [mapa_h], ebx
        jmp .p9
.sitio:
        mov esi, [mapa_p]
        mov edi, [an_p]
        mov ecx, [an_tam]
        rep movsb                   ; (hacia delante: el destino está antes)
        mov eax, [an_p]
        mov [mapa_p], eax
.p9:
        call suelta_vista
        mov byte [sucio], 1
        call inval
        popad
.x:
        ret

; Espacio: la para o la sigue; acabada, otra vez desde el principio
an_espacio:
        pushad
        cmp byte [an_fin], 0
        je .e1
        call an_desde_cero
        call an_pon
        jmp .e9
.e1:
        xor byte [an_pausa], 1
        HV tics
        mov [an_tic], eax
.e9:
        call inval_pie
        popad
        ret

; (interna) el cuadro [an_k], a la vista: solo se repinta la imagen
an_pon:
        pushad
        mov eax, [an_k]
        mul dword [an_tam]
        add eax, [an_p]
        mov [mapa_p], eax
        mov byte [sucio], 1
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov eax, [ox]
        mov ebx, [vy]
        add ebx, [oy]
        mov ecx, [vis_an]
        mov edx, [vis_al]
        HV ven_invalidar_rect
.x:
        popad
        ret

; el temporizador: ¿toca otro cuadro?
an_reloj:
        pushad
        cmp dword [an_n], 2
        jb .x
        HV tics
        mov ebx, eax
        sub eax, [an_tic]
        mov [an_tic], ebx
        cmp byte [an_pausa], 0
        jne .x
        cmp byte [an_fin], 0
        jne .x
        cmp byte [mini], 0          ; (con las miniaturas delante no se ve)
        jne .x
        cmp byte [pase], 0          ; en el pase, su pausa la para también
        je .r0
        cmp byte [pase_pausa], 0
        jne .x
.r0:
        cmp eax, 40                 ; (medianoche, o un parón largo: como si nada)
        jbe .r1
        mov eax, 1
.r1:
        imul eax, AN_TIC_MS
        add eax, [an_debe]
        mov [an_debe], eax
        xor edi, edi                ; EDI=1: ha cambiado el cuadro
        mov esi, [an_ms]
.sig:
        mov ecx, [an_k]
        mov eax, [an_debe]
        sub eax, [esi+ecx*4]
        jb .listo
        mov [an_debe], eax
        inc ecx
        cmp ecx, [an_n]
        jb .pon
        cmp dword [an_vueltas], -1  ; otra vuelta, si queda
        je .vuelta
        dec dword [an_vueltas]
        jnz .vuelta
        mov byte [an_fin], 1        ; se acabó: se queda en el último
        call inval_pie
        jmp .listo
.vuelta:
        xor ecx, ecx
.pon:
        mov [an_k], ecx
        mov edi, 1
        jmp .sig
.listo:
        test edi, edi
        jz .x
        call an_pon
.x:
        popad
        ret

; lo que dice el pie de una animación (EDI avanza). En el pase no: ya dice
; que es el pase, y no cabe todo
an_pie:
        cmp dword [an_n], 2
        jb .x
        cmp byte [pase], 0
        jne .x
        push eax
        push esi
        mov esi, t_sep
        call hvl_cad
        cmp byte [an_pausa], 0
        jne .pausa
        mov esi, t_an_pie
        cmp byte [an_fin], 0
        je .p1
        mov esi, t_an_fin
.p1:
        call hvl_cad
        jmp .p9
.pausa:
        mov esi, t_an_pausa
        call hvl_cad
        mov eax, [an_k]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [an_n]
        call hvl_num
        mov esi, t_an_pausa2
        call hvl_cad
.p9:
        pop esi
        pop eax
.x:
        ret

; lo que dice la información de una animación (EDI avanza; deja un salto de línea)
an_info:
        cmp dword [f_cuadros], 2
        jb .x
        push eax
        push edx
        push ecx
        push esi
        mov esi, t_i_anima
        call hvl_cad
        mov eax, [f_cuadros]
        call hvl_num
        mov esi, t_i_cuadros
        call hvl_cad
        mov eax, [f_dura]           ; en segundos, con dos decimales
        add eax, 5
        xor edx, edx
        mov ecx, 1000
        div ecx
        call hvl_num
        mov al, ','
        stosb
        mov eax, edx
        xor edx, edx
        mov ecx, 10
        div ecx
        call num_2
        mov esi, t_i_seg
        call hvl_cad
        mov eax, [f_bucle]
        mov esi, t_i_unavez
        cmp eax, -1
        je .dice
        mov esi, t_i_sinfin
        test eax, eax
        jz .dice
        mov esi, t_i_veces1
        call hvl_cad
        inc eax
        call hvl_num
        mov esi, t_i_veces2
.dice:
        call hvl_cad
        cmp byte [an_no], 0
        je .i1
        mov esi, t_i_an_no
        call hvl_cad
.i1:
        mov al, 10
        stosb
        pop esi
        pop ecx
        pop edx
        pop eax
.x:
        ret
