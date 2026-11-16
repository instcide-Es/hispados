; =============================================================================
;  MIRADOR.HVP - el formato GIF (87a y 89a)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La paleta global o la propia de la imagen, el color transparente (de la
;  extensión de control gráfico), las imágenes entrelazadas y las que no
;  ocupan toda la «pantalla lógica» (lo de alrededor queda transparente, o
;  del color de fondo si no hay transparente). De un GIF animado se enseña
;  el primer cuadro.
;
;  Los puntos van comprimidos con LZW en trozos de hasta 255 octetos: se
;  descomprime todo de una vez a la memoria de trabajo (un octeto por punto:
;  entrelazada, las filas no vienen en orden). El diccionario (4.096 códigos:
;  de cada uno, el código del que viene y su último color) está en GIF_BSS.
; =============================================================================
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

gif_abre:
        lea eax, [esi+ecx]
        mov [gif_fin], eax
        movzx eax, word [esi+6]
        mov [im_an], eax
        movzx eax, word [esi+8]
        mov [im_al], eax
        movzx eax, byte [esi+11]
        mov [gif_relleno], eax
        mov dword [im_bits], 8
        mov dword [im_clase], IC_PALETA
        ; sin paleta ninguna (los hay): 256 grises
        xor eax, eax
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
        push edx
        call gif_paleta
        pop edx
.bloques:
        mov ebp, -1                 ; EBP=el índice transparente
.b:
        cmp edx, [gif_fin]
        jae .roto
        mov al, [edx]
        inc edx
        cmp al, 0x21
        je .ext
        cmp al, 0x2C
        je .imagen
        jmp .roto                   ; (ni una imagen antes del final)
.ext:
        lea eax, [edx+7]
        cmp eax, [gif_fin]
        ja .roto
        cmp byte [edx], 0xF9        ; el control gráfico: el color transparente
        jne .e1
        cmp byte [edx+1], 4
        jne .e1
        mov ebp, -1
        test byte [edx+2], 1
        jz .e1
        movzx ebp, byte [edx+5]
.e1:
        inc edx
        call gif_salta
        jc .roto
        jmp .b
.imagen:
        lea eax, [edx+10]
        cmp eax, [gif_fin]
        ja .roto
        movzx eax, word [edx]
        mov [gif_fx], eax
        movzx eax, word [edx+2]
        mov [gif_fy], eax
        movzx eax, word [edx+4]
        mov [gif_fw], eax
        test eax, eax
        jz .roto
        movzx eax, word [edx+6]
        mov [gif_fh], eax
        test eax, eax
        jz .roto
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
        mov [im_bits], ecx
        mov eax, 1
        shl eax, cl
        mov ecx, eax
        lea eax, [eax+eax*2]
        mov ebx, edx
        add edx, eax
        cmp edx, [gif_fin]
        jae .roto
        push edx
        call gif_paleta
        pop edx
.i1:
        cmp edx, [gif_fin]
        jae .roto
        movzx eax, byte [edx]
        inc edx
        mov [gif_min], eax
        dec eax
        cmp eax, 10                 ; de 1 a 11 bits
        ja .roto
        mov [gif_p], edx
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
        test ebp, ebp
        js .i2
        mov byte [im_pal+ebp*4+3], 0
        mov dword [im_hay_alfa], 1
        mov [gif_relleno], ebp
.i2:
        mov eax, [im_an]
        imul eax, [im_al]
        mov [im_alm], eax
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
        add ecx, [im_almp]
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
        mov ecx, [im_alm]
        mov al, [gif_relleno]
        rep stosb
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
        cmp dword [gif_hecha], 0    ; ¿se ha quedado a medias?
        jne .bien
        mov dword [im_aviso], 1
.bien:
        xor eax, eax
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
