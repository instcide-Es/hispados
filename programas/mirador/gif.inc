; =============================================================================
;  MIRADOR.HVP - el formato GIF (87a y 89a), con sus animaciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La paleta global o la propia de la imagen, el color transparente (de la
;  extensión de control gráfico), las imágenes entrelazadas y las que no
;  ocupan toda la «pantalla lógica» (lo de alrededor queda transparente, o
;  del color de fondo si no hay transparente).
;
;  Los puntos van comprimidos con LZW en trozos de hasta 255 octetos: se
;  descomprime todo de una vez a la memoria de trabajo (un octeto por punto:
;  entrelazada, las filas no vienen en orden). El diccionario (4.096 códigos:
;  de cada uno, el código del que viene y su último color) está en GIF_BSS.
;
;  Como imagen (abre, prepara, fila) un GIF es su primer cuadro. Si tiene más
;  ([im_cuadros] > 1; [im_dura] dice cuántos milisegundos dura una vuelta e
;  [im_bucle] cuántas veces se repite: -1 no lo dice (una vez), 0 sin fin),
;  los cuadros de la animación se piden después, uno detrás de otro:
;    gif_an_mem      -> EAX=octetos de memoria de trabajo que hacen falta
;    gif_an_empieza  EDI=esa memoria: se pone delante del primer cuadro
;    gif_an_cuadro   compone el cuadro siguiente -> EAX=los milisegundos que
;                    se queda a la vista; CF=1 si no hay más (o el que viene
;                    está cortado o dañado: la animación acaba en el anterior)
;    gif_an_fila     EAX=fila EDI=destino: una fila del cuadro compuesto, en
;                    rojo, verde y azul, con lo transparente mezclado con el
;                    fondo (como mi_fila)
;  Pueden cambiar todos los registros. Hace falta que el archivo siga en
;  memoria y que entre tanto no se abra otro GIF.
;
;  Cómo se compone. Hay un «lienzo» del tamaño de la pantalla lógica, con
;  cuatro octetos por punto (rojo, verde, azul y opacidad). Empieza
;  transparente si el primer cuadro tiene color transparente y, si no, del
;  color de fondo (como se ve la imagen sola). Cada cuadro se pinta encima,
;  en su rectángulo, saltando sus puntos transparentes. Antes de pintar uno
;  se hace con el rectángulo del anterior lo que este pidió («disposal»):
;    0 y 1  nada: se queda como está
;    2      se borra: transparente si aquel cuadro tenía color transparente;
;           si no, del color de fondo (con la paleta de aquel cuadro)
;    3      vuelve a estar como antes de pintarlo (se guarda una copia del
;           lienzo; solo se pide su memoria si algún cuadro lo usa)
;  Un cuadro que se sale de la pantalla lógica se recorta. El tiempo de cada
;  cuadro va en centésimas; menos de dos centésimas (o ninguna) son diez,
;  como hacen los navegadores. Las repeticiones, de la extensión
;  «NETSCAPE2.0».
; =============================================================================
GIF_AN_MAX  equ 4096                ; cuadros de una animación, como mucho

%macro GIF_BSS 0
            alignb 4
gif_p       resd 1                  ; por dónde se va leyendo
gif_fin     resd 1
gif_blq     resd 1                  ; octetos que quedan del trozo de ahora
gif_se_acabo resd 1
gif_fx      resd 1                  ; el cuadro, dentro de la pantalla lógica
gif_fy      resd 1
gif_fw      resd 1
gif_fh      resd 1
gif_entre   resd 1                  ; entrelazada
gif_min     resd 1                  ; bits del código más corto
gif_relleno resd 1                  ; con qué índice se llena lo que no tapa el cuadro
gif_x       resd 1
gif_y       resd 1
gif_pasada  resd 1
gif_hecha   resd 1                  ; ya están todos los puntos
gif_borra   resd 1                  ; el código de «borrar el diccionario»
gif_sig     resd 1                  ; el siguiente código libre
gif_tam     resd 1                  ; bits de cada código, ahora
gif_ant     resd 1                  ; el código anterior (-1: ninguno)
gif_primero resd 1                  ; el primer color de la última cadena
gif_bits    resd 1
gif_nbits   resd 1
gif_pref    resw 4096
gif_suf     resb 4096
gif_pila    resb 4100
            alignb 4
gif_dest    resd 1                  ; dónde se dejan los puntos (un octeto cada uno)
gif_bg      resd 1                  ; el índice del color de fondo
gif_gpal    resd 1                  ; la paleta global (0: no hay) y sus colores
gif_gncol   resd 1
gif_lpal    resd 1                  ; la paleta del cuadro (0: no tiene) y sus colores
gif_lncol   resd 1
gif_lbits   resd 1
gif_transp  resd 1                  ; del cuadro: su índice transparente (-1: no tiene),
gif_cdisp   resd 1                  ; qué hacer con él después («disposal»)
gif_cret    resd 1                  ; y sus centésimas (-1: no lo dice)
gif_datos   resd 1                  ; dónde empiezan sus datos
gif_bloq0   resd 1                  ; dónde empiezan los bloques (tras la cabecera y la paleta global)
gif_bucle   resd 1
gif_hay_d3  resd 1                  ; algún cuadro pide «como antes» (disposal 3)
gif_an_p    resd 1                  ; la animación: dónde empieza el cuadro siguiente
gif_an_k    resd 1                  ; cuántos van
gif_an_idx  resd 1                  ; los índices del cuadro
gif_an_lienzo resd 1                ; el lienzo
gif_an_antes resd 1                 ; su copia (0: no hace falta)
gif_an_disp resd 1                  ; lo que pidió el cuadro anterior, y su rectángulo
gif_an_dx   resd 1
gif_an_dy   resd 1
gif_an_dw   resd 1
gif_an_dh   resd 1
gif_an_dfondo resd 1                ; con qué se borra
gif_cpal    resd 256                ; la paleta del cuadro: rojo, verde, azul, opacidad
%endmacro

gif_paso    dd 8, 8, 4, 2
gif_desde   dd 0, 4, 2, 1

gif_detecta:
        cmp ecx, 14
        jb .no
        cmp dword [esi], 'GIF8'
        jne .no
        cmp byte [esi+5], 'a'
        jne .no
        cmp byte [esi+4], '7'
        je .si
        cmp byte [esi+4], '9'
        jne .no
.si:
        clc
        ret
.no:
        stc
        ret

; (interna) salta los trozos de datos desde EDX (hasta el de largo 0);
; CF=1 si se sale del archivo
gif_salta:
        cmp edx, [gif_fin]
        jae .mal
        movzx eax, byte [edx]
        inc edx
        test eax, eax
        jz .bien
        add edx, eax
        jmp gif_salta
.bien:
        clc
        ret
.mal:
        stc
        ret

; (interna) la paleta de EBX (ECX colores, tres octetos cada uno) a im_pal
gif_paleta:
        mov [im_ncol], ecx
        xor eax, eax
.c:
        mov dl, [ebx]
        mov [im_pal+eax*4], dl
        mov dl, [ebx+1]
        mov [im_pal+eax*4+1], dl
        mov dl, [ebx+2]
        mov [im_pal+eax*4+2], dl
        mov byte [im_pal+eax*4+3], 255
        add ebx, 3
        inc eax
        dec ecx
        jnz .c
        ret

; (interna) EAX=centésimas de un cuadro (-1: no lo dice) -> EAX=milisegundos
gif_ms:
        cmp eax, 2
        jge .m
        mov eax, 10
.m:
        imul eax, 10
        ret

; (interna) lee los bloques desde EDX hasta la imagen siguiente: las
; extensiones (el control gráfico: transparente, «disposal» y tiempo; y las
; repeticiones de NETSCAPE2.0) y la cabecera de la imagen. Deja el cuadro en
; gif_fx, gif_fy, gif_fw, gif_fh, gif_entre, gif_lpal, gif_lncol, gif_min,
; gif_transp, gif_cdisp, gif_cret y gif_datos (y gif_p). CF=1 si no hay más
; imágenes o el archivo se acaba o está mal
gif_bloques:
        mov dword [gif_transp], -1
        mov dword [gif_cdisp], 0
        mov dword [gif_cret], -1
        mov dword [gif_lpal], 0
.b:
        cmp edx, [gif_fin]
        jae .no
        mov al, [edx]
        inc edx
        cmp al, 0x21
        je .ext
        cmp al, 0x2C
        je .imagen
        jmp .no                     ; (el final, o algo que no es de un GIF)
.ext:
        lea eax, [edx+7]
        cmp eax, [gif_fin]
        ja .no
        cmp byte [edx], 0xF9        ; el control gráfico
        jne .e1
        cmp byte [edx+1], 4
        jne .e2
        mov dword [gif_transp], -1
        movzx eax, byte [edx+2]
        test al, 1
        jz .e0
        movzx ecx, byte [edx+5]
        mov [gif_transp], ecx
.e0:
        shr eax, 2
        and eax, 7
        mov [gif_cdisp], eax
        movzx eax, word [edx+3]
        mov [gif_cret], eax
        jmp .e2
.e1:
        cmp byte [edx], 0xFF        ; de aplicación: «NETSCAPE2.0», 3, 1 y las repeticiones
        jne .e2
        cmp byte [edx+1], 11
        jne .e2
        lea eax, [edx+17]
        cmp eax, [gif_fin]
        ja .e2
        cmp dword [edx+2], 'NETS'
        jne .e2
        cmp dword [edx+6], 'CAPE'
        jne .e2
        cmp dword [edx+9], 'E2.0'
        jne .e2
        cmp byte [edx+13], 3
        jb .e2
        cmp byte [edx+14], 1
        jne .e2
        movzx eax, word [edx+15]
        mov [gif_bucle], eax
.e2:
        inc edx
        call gif_salta
        jc .no
        jmp .b
.imagen:
        lea eax, [edx+10]
        cmp eax, [gif_fin]
        ja .no
        movzx eax, word [edx]
        mov [gif_fx], eax
        movzx eax, word [edx+2]
        mov [gif_fy], eax
        movzx eax, word [edx+4]
        mov [gif_fw], eax
        test eax, eax
        jz .no
        movzx eax, word [edx+6]
        mov [gif_fh], eax
        test eax, eax
        jz .no
        movzx ecx, byte [edx+8]
        add edx, 9
        mov eax, ecx
        shr eax, 6
        and eax, 1
        mov [gif_entre], eax
        test cl, 0x80
        jz .i1
        and ecx, 7                  ; con su propia paleta
        inc ecx
        mov [gif_lbits], ecx
        mov eax, 1
        shl eax, cl
        mov [gif_lncol], eax
        mov [gif_lpal], edx
        lea eax, [eax+eax*2]
        add edx, eax
        cmp edx, [gif_fin]
        jae .no
.i1:
        cmp edx, [gif_fin]
        jae .no
        movzx eax, byte [edx]
        inc edx
        mov [gif_min], eax
        dec eax
        cmp eax, 10                 ; de 1 a 11 bits
        ja .no
        mov [gif_p], edx
        mov [gif_datos], edx
        clc
        ret
.no:
        stc
        ret

gif_abre:
        lea eax, [esi+ecx]
        mov [gif_fin], eax
        movzx eax, word [esi+6]
        mov [im_an], eax
        movzx eax, word [esi+8]
        mov [im_al], eax
        movzx eax, byte [esi+11]
        mov [gif_relleno], eax
        mov [gif_bg], eax
        mov dword [im_bits], 8
        mov dword [im_clase], IC_PALETA
        xor eax, eax
        mov [gif_gpal], eax
        mov [gif_gncol], eax
        mov [gif_hay_d3], eax
        mov dword [gif_bucle], -1
        ; sin paleta ninguna (los hay): 256 grises
.g:
        mov [im_pal+eax*4], al
        mov [im_pal+eax*4+1], al
        mov [im_pal+eax*4+2], al
        inc eax
        cmp eax, 256
        jb .g
        lea edx, [esi+13]           ; EDX=por dónde se va
        movzx ecx, byte [esi+10]
        test cl, 0x80
        jz .bloques
        and ecx, 7
        inc ecx
        mov [im_bits], ecx
        mov eax, 1
        shl eax, cl
        mov ecx, eax
        lea eax, [eax+eax*2]
        mov ebx, edx
        add edx, eax
        cmp edx, [gif_fin]
        ja .roto
        mov [gif_gpal], ebx
        mov [gif_gncol], ecx
        push edx
        call gif_paleta
        pop edx
.bloques:
        mov [gif_bloq0], edx
        call gif_bloques            ; el primer cuadro
        jc .roto
        cmp dword [gif_lpal], 0
        je .i1
        mov eax, [gif_lbits]
        mov [im_bits], eax
        mov ebx, [gif_lpal]
        mov ecx, [gif_lncol]
        call gif_paleta
.i1:
        ; una pantalla lógica sin medidas: las del cuadro
        cmp dword [im_an], 0
        je .sin
        cmp dword [im_al], 0
        jne .con
.sin:
        mov eax, [gif_fx]
        add eax, [gif_fw]
        mov [im_an], eax
        mov eax, [gif_fy]
        add eax, [gif_fh]
        mov [im_al], eax
.con:
        call mi_medidas_ok
        jc .grande
        mov ebp, [gif_transp]
        test ebp, ebp
        js .i2
        mov byte [im_pal+ebp*4+3], 0
        mov dword [im_hay_alfa], 1
        mov [gif_relleno], ebp
.i2:
        mov eax, [im_an]
        imul eax, [im_al]
        mov [im_alm], eax
        ; ---- ¿hay más cuadros? Se cuentan, con sus tiempos ----
        mov dword [im_cuadros], 1
        mov eax, [gif_cret]
        call gif_ms
        mov [im_dura], eax
.cuenta:
        cmp dword [gif_cdisp], 3
        jne .c1
        mov dword [gif_hay_d3], 1
.c1:
        cmp dword [im_cuadros], GIF_AN_MAX
        jae .contado
        mov edx, [gif_datos]
        call gif_salta
        jc .contado
        call gif_bloques
        jc .contado
        inc dword [im_cuadros]
        mov eax, [gif_cret]
        call gif_ms
        add [im_dura], eax
        jmp .cuenta
.contado:
        mov eax, [gif_bucle]
        mov [im_bucle], eax
        mov edx, [gif_bloq0]        ; y otra vez en el primero (que ya se sabe que está)
        call gif_bloques
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret
.grande:
        mov eax, ME_GRANDE
        ret

; (interna) el octeto siguiente de los datos -> AL; CF=1 si se han acabado
gif_octeto:
        cmp dword [gif_se_acabo], 0
        jne .no
        push esi
        mov esi, [gif_p]
        cmp dword [gif_blq], 0
        jne .o1
        cmp esi, [gif_fin]
        jae .fin
        movzx eax, byte [esi]       ; otro trozo: su largo
        inc esi
        test eax, eax
        jz .fin
        mov [gif_blq], eax
.o1:
        cmp esi, [gif_fin]
        jae .fin
        mov al, [esi]
        inc esi
        dec dword [gif_blq]
        mov [gif_p], esi
        pop esi
        clc
        ret
.fin:
        mov dword [gif_se_acabo], 1
        pop esi
.no:
        stc
        ret

; (interna) el punto AL, en su sitio del cuadro; después, al siguiente (con
; el orden de las filas entrelazadas, si lo es)
gif_pon:
        cmp dword [gif_hecha], 0
        jne .x
        push ebx
        push ecx
        mov ebx, [gif_fx]
        add ebx, [gif_x]
        cmp ebx, [im_an]
        jae .p1
        mov ecx, [gif_fy]
        add ecx, [gif_y]
        cmp ecx, [im_al]
        jae .p1
        imul ecx, [im_an]
        add ecx, ebx
        add ecx, [gif_dest]
        mov [ecx], al
.p1:
        mov ebx, [gif_x]
        inc ebx
        cmp ebx, [gif_fw]
        jb .p8
        xor ebx, ebx                ; otra fila
        mov ecx, [gif_y]
        cmp dword [gif_entre], 0
        jne .p2
        inc ecx
        cmp ecx, [gif_fh]
        jb .p7
        mov dword [gif_hecha], 1
        jmp .p7
.p2:
        push edx
        mov edx, [gif_pasada]
        add ecx, [gif_paso+edx*4]
.p3:
        cmp ecx, [gif_fh]
        jb .p6
        inc edx                     ; otra pasada
        cmp edx, 4
        jb .p4
        mov dword [gif_hecha], 1
        jmp .p6
.p4:
        mov ecx, [gif_desde+edx*4]
        jmp .p3
.p6:
        mov [gif_pasada], edx
        pop edx
.p7:
        mov [gif_y], ecx
.p8:
        mov [gif_x], ebx
        pop ecx
        pop ebx
.x:
        ret

gif_prepara:
        mov edi, [im_almp]          ; lo que no tape el cuadro
        mov [gif_dest], edi
        mov ecx, [im_alm]
        mov al, [gif_relleno]
        rep stosb
        call gif_descomprime
        cmp dword [gif_hecha], 0    ; ¿se ha quedado a medias?
        jne .bien
        mov dword [im_aviso], 1
.bien:
        xor eax, eax
        ret

; (interna) descomprime los puntos del cuadro (desde [gif_p]) a [gif_dest];
; [gif_hecha] queda a 1 si han salido todos
gif_descomprime:
        xor eax, eax
        mov [gif_blq], eax
        mov [gif_se_acabo], eax
        mov [gif_x], eax
        mov [gif_y], eax
        mov [gif_pasada], eax
        mov [gif_hecha], eax
        mov [gif_bits], eax
        mov [gif_nbits], eax
        mov ecx, [gif_min]
        inc eax
        shl eax, cl
        mov [gif_borra], eax
        call .borra
.codigo:
        ; ---- el código siguiente: [gif_tam] bits, del de menos peso ----
        mov ecx, [gif_nbits]
        cmp ecx, [gif_tam]
        jae .c1
        call gif_octeto
        jc .corto
        movzx eax, al
        shl eax, cl
        or [gif_bits], eax
        add dword [gif_nbits], 8
        jmp .codigo
.c1:
        mov ecx, [gif_tam]
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, [gif_bits]
        shr dword [gif_bits], cl
        sub [gif_nbits], ecx
        cmp eax, [gif_borra]
        jne .c2
        call .borra
        jmp .codigo
.c2:
        mov ebx, [gif_borra]
        inc ebx
        cmp eax, ebx                ; el de «fin»
        je .fin
        cmp dword [gif_ant], -1
        jne .c3
        cmp eax, [gif_borra]        ; el primero tras borrar: un color tal cual
        ja .roto
        mov [gif_ant], eax
        mov [gif_primero], eax
        call gif_pon
        jmp .seguir
.c3:
        mov ebp, eax                ; EBP=el código que ha venido
        xor edx, edx                ; EDX=lo que hay en la pila
        cmp eax, [gif_sig]
        jb .c4
        ja .roto                    ; (uno que aún no existe)
        mov ecx, [gif_primero]      ; el que se está haciendo: la cadena anterior
        mov [gif_pila], cl          ; y su primer color
        inc edx
        mov eax, [gif_ant]
.c4:
        cmp eax, [gif_borra]        ; la cadena, del final al principio
        jb .c5
        mov cl, [gif_suf+eax]
        mov [gif_pila+edx], cl
        inc edx
        movzx eax, word [gif_pref+eax*2]
        cmp edx, 4098
        jb .c4
        jmp .roto
.c5:
        mov [gif_primero], eax
        call gif_pon
.c6:
        test edx, edx
        jz .c7
        dec edx
        mov al, [gif_pila+edx]
        call gif_pon
        jmp .c6
.c7:
        mov eax, [gif_sig]          ; un código nuevo: el anterior y este primer color
        cmp eax, 4096
        jae .c8
        mov ecx, [gif_ant]
        mov [gif_pref+eax*2], cx
        mov ecx, [gif_primero]
        mov [gif_suf+eax], cl
        inc eax
        mov [gif_sig], eax
        mov ecx, [gif_tam]
        cmp ecx, 12
        jae .c8
        mov ebx, 1
        shl ebx, cl
        cmp eax, ebx
        jb .c8
        inc dword [gif_tam]
.c8:
        mov [gif_ant], ebp
.seguir:
        cmp dword [gif_hecha], 0
        je .codigo
        jmp .fin
.roto:
.corto:
.fin:
        ret
; (interna) el diccionario, vacío
.borra:
        mov eax, [gif_borra]
        add eax, 2
        mov [gif_sig], eax
        mov eax, [gif_min]
        inc eax
        mov [gif_tam], eax
        mov dword [gif_ant], -1
        ret

gif_fila:
        imul eax, [im_an]
        mov esi, [im_almp]
        add esi, eax
        mov ecx, [im_an]
        rep movsb
        clc
        ret

; =============================================================================
;  La animación
; =============================================================================
gif_an_mem:
        mov eax, [im_an]
        imul eax, [im_al]
        mov edx, eax
        lea eax, [eax+eax*4]        ; los índices y el lienzo
        cmp dword [gif_hay_d3], 0
        je .x
        lea eax, [eax+edx*4]        ; y su copia
.x:
        add eax, 64
        ret

gif_an_empieza:
        mov [gif_an_idx], edi
        mov eax, [im_an]
        imul eax, [im_al]
        lea edi, [edi+eax+3]
        and edi, ~3
        mov [gif_an_lienzo], edi
        lea edi, [edi+eax*4]
        mov dword [gif_an_antes], 0
        cmp dword [gif_hay_d3], 0
        je .e1
        mov [gif_an_antes], edi
.e1:
        mov eax, [gif_bloq0]
        mov [gif_an_p], eax
        xor eax, eax
        mov [gif_an_k], eax
        mov [gif_an_disp], eax
        ret

gif_an_cuadro:
        mov edx, [gif_an_p]
        call gif_bloques
        jc .no
        mov eax, [gif_an_idx]       ; sus puntos; si no salen todos, no vale
        mov [gif_dest], eax
        call gif_descomprime
        cmp dword [gif_hecha], 0
        je .no
        ; ---- su paleta: la suya, la global o grises ----
        mov edi, gif_cpal
        mov eax, 0xFF000000
        mov ecx, 256
        rep stosd
        mov esi, [gif_lpal]
        mov ecx, [gif_lncol]
        test esi, esi
        jnz .pal
        mov esi, [gif_gpal]
        mov ecx, [gif_gncol]
        test esi, esi
        jnz .pal
        xor eax, eax
.gris:
        mov [gif_cpal+eax*4], al
        mov [gif_cpal+eax*4+1], al
        mov [gif_cpal+eax*4+2], al
        inc eax
        cmp eax, 256
        jb .gris
        jmp .pal9
.pal:
        xor eax, eax
.pal1:
        mov dl, [esi]
        mov [gif_cpal+eax*4], dl
        mov dl, [esi+1]
        mov [gif_cpal+eax*4+1], dl
        mov dl, [esi+2]
        mov [gif_cpal+eax*4+2], dl
        add esi, 3
        inc eax
        dec ecx
        jnz .pal1
.pal9:
        ; con qué se borra, si hay que borrar: transparente si este cuadro
        ; tiene color transparente; si no, el color de fondo
        xor ebx, ebx
        cmp dword [gif_transp], 0
        jge .f1
        mov eax, [gif_bg]
        mov ebx, [gif_cpal+eax*4]
.f1:
        cmp dword [gif_an_k], 0
        jne .antes
        mov edi, [gif_an_lienzo]    ; el primero: el lienzo, de ese color
        mov ecx, [im_an]
        imul ecx, [im_al]
        mov eax, ebx
        rep stosd
        jmp .rect
.antes:
        ; ---- lo que pidió el cuadro anterior, en su rectángulo ----
        cmp dword [gif_an_disp], 2
        jne .a3
        mov eax, [gif_an_dfondo]
        call gif_an_llena
        jmp .rect
.a3:
        cmp dword [gif_an_disp], 3
        jne .rect
        mov esi, [gif_an_antes]
        mov edi, [gif_an_lienzo]
        call gif_an_copia
.rect:
        ; ---- el rectángulo de este, recortado a la pantalla lógica ----
        mov eax, [gif_fx]
        cmp eax, [im_an]
        jbe .r1
        mov eax, [im_an]
.r1:
        mov [gif_an_dx], eax
        mov ecx, [im_an]
        sub ecx, eax
        cmp ecx, [gif_fw]
        jbe .r2
        mov ecx, [gif_fw]
.r2:
        mov [gif_an_dw], ecx
        mov eax, [gif_fy]
        cmp eax, [im_al]
        jbe .r3
        mov eax, [im_al]
.r3:
        mov [gif_an_dy], eax
        mov ecx, [im_al]
        sub ecx, eax
        cmp ecx, [gif_fh]
        jbe .r4
        mov ecx, [gif_fh]
.r4:
        mov [gif_an_dh], ecx
        mov [gif_an_dfondo], ebx
        mov eax, [gif_cdisp]
        mov [gif_an_disp], eax
        cmp eax, 3                  ; «como antes»: se guarda cómo está
        jne .pinta
        mov esi, [gif_an_lienzo]
        mov edi, [gif_an_antes]
        test edi, edi
        jnz .r5
        mov dword [gif_an_disp], 1  ; (no puede pasar: se pidió su memoria si alguno lo usa)
        jmp .pinta
.r5:
        call gif_an_copia
.pinta:
        ; ---- sus puntos, encima (menos los transparentes) ----
        mov ebp, [gif_an_dh]
        test ebp, ebp
        jz .p9
        cmp dword [gif_an_dw], 0
        je .p9
        mov eax, [gif_an_dy]
        imul eax, [im_an]
        add eax, [gif_an_dx]
        mov esi, [gif_an_idx]
        add esi, eax
        mov edi, [gif_an_lienzo]
        lea edi, [edi+eax*4]
        mov ebx, [gif_transp]
.p1:
        push esi
        push edi
        mov ecx, [gif_an_dw]
.p2:
        movzx eax, byte [esi]
        inc esi
        cmp eax, ebx
        je .p3
        mov eax, [gif_cpal+eax*4]
        mov [edi], eax
.p3:
        add edi, 4
        dec ecx
        jnz .p2
        pop edi
        pop esi
        add esi, [im_an]
        mov eax, [im_an]
        lea edi, [edi+eax*4]
        dec ebp
        jnz .p1
.p9:
        ; ---- y a por el siguiente ----
        mov edx, [gif_datos]
        call gif_salta
        jnc .s1
        mov edx, [gif_fin]
.s1:
        mov [gif_an_p], edx
        inc dword [gif_an_k]
        mov eax, [gif_cret]
        call gif_ms
        clc
        ret
.no:
        mov eax, [gif_fin]          ; (y ya no hay más)
        mov [gif_an_p], eax
        stc
        ret

; (internas) el rectángulo del cuadro anterior: lleno del color EAX, o
; copiado del lienzo ESI al lienzo EDI
gif_an_llena:
        mov edx, [gif_an_dh]
        test edx, edx
        jz .x
        cmp dword [gif_an_dw], 0
        je .x
        push eax
        mov eax, [gif_an_dy]
        imul eax, [im_an]
        add eax, [gif_an_dx]
        mov edi, [gif_an_lienzo]
        lea edi, [edi+eax*4]
        pop eax
.f:
        push edi
        mov ecx, [gif_an_dw]
        rep stosd
        pop edi
        mov ecx, [im_an]
        lea edi, [edi+ecx*4]
        dec edx
        jnz .f
.x:
        ret
gif_an_copia:
        mov edx, [gif_an_dh]
        test edx, edx
        jz .x
        cmp dword [gif_an_dw], 0
        je .x
        mov eax, [gif_an_dy]
        imul eax, [im_an]
        add eax, [gif_an_dx]
        lea esi, [esi+eax*4]
        lea edi, [edi+eax*4]
        mov eax, [im_an]
        shl eax, 2
.f:
        push esi
        push edi
        mov ecx, [gif_an_dw]
        rep movsd
        pop edi
        pop esi
        add esi, eax
        add edi, eax
        dec edx
        jnz .f
.x:
        ret

gif_an_fila:
        mov [mi_y], eax
        imul eax, [im_an]
        mov esi, [gif_an_lienzo]
        lea esi, [esi+eax*4]
        xor ebx, ebx
        mov ecx, [im_an]
.p:
        mov eax, [esi+ebx*4]
        call mi_pon
        inc ebx
        cmp ebx, ecx
        jb .p
        clc
        ret
