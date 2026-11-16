; =============================================================================
;  MIRADOR.HVP - el formato JPEG: el puente con programas/comun/jpeg.inc
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El descodificador es la pieza común (jpeg.inc: lea su cabecera para saber
;  qué JPEG admite); aquí solo se le da la forma de un formato de Mirador. Es
;  el único que sabe entregar la imagen ya reducida a la mitad, un cuarto o
;  un octavo sin hacerla entera (mi_reduce): así una foto de muchos
;  megapuntos se abre con poca memoria y deprisa. También lee aquí lo que
;  apunta la cámara (EXIF): la orientación, la marca y el modelo, y la fecha.
;
;  Si se ensambla sin MI_CON_JPEG, los JPEG se reconocen y se dice que
;  todavía no se pueden ver.
; =============================================================================
%macro JPG_BSS 0
            alignb 4
jpg_dat     resd 1
jpg_tam     resd 1
jpg_op      resd 1                  ; las opciones (la reducción)
jx_len      resd 1                  ; EXIF: lo que mide su bloque
jx_mm       resd 1                  ; 1: los números van con el octeto de más peso delante
jx_camara   resb 68
jx_tomada   resb 24
%endmacro

jpg_detecta:
        cmp ecx, 4
        jb .no
        cmp word [esi], 0xD8FF
        jne .no
        cmp byte [esi+2], 0xFF
        jne .no
        clc
        ret
.no:
        stc
        ret

%ifdef MI_CON_JPEG
jpg_t_prec  db "un JPEG de 12 o 16 bits por muestra", 0
jpg_t_arit  db "un JPEG con codificación aritmética", 0
jpg_t_sinp  db "un JPEG sin pérdidas", 0
jpg_t_jera  db "un JPEG jerárquico", 0
jpg_t_prog  db "progresivo", 0

jpg_abre:
        mov [jpg_dat], esi
        mov [jpg_tam], ecx
        mov dword [jpg_op], JPEG_R1
        call jpg_exif
; (interna) las medidas, con las opciones de ahora
jpg_mide:
        mov esi, [jpg_dat]
        mov ecx, [jpg_tam]
        mov eax, [jpg_op]
        call jpeg_medidas
        jc .mal
        mov dword [im_clase], IC_RGB
        mov dword [im_bits], 24
        cmp ecx, 1
        jne .m1
        mov dword [im_clase], IC_GRIS
        mov dword [im_bits], 8
.m1:
        test edx, JPEG_B_PROG
        jz .m2
        mov dword [im_extra], jpg_t_prog
.m2:
        mov eax, [jp_sal_an]
        mov [im_an], eax
        mov eax, [jp_sal_al]
        mov [im_al], eax
        mov eax, [jp_pide]
        cmp eax, 0x7FFF0000         ; (tanta no la hay en ninguna parte)
        ja .grande
        mov [im_alm], eax
        xor eax, eax
        ret
.grande:
        mov eax, ME_GRANDE
        ret
.mal:
        mov dword [im_var], jpg_t_prec
        cmp eax, JPEG_E_PRECISION
        je .var
        mov dword [im_var], jpg_t_arit
        cmp eax, JPEG_E_ARITMETICO
        je .var
        mov dword [im_var], jpg_t_sinp
        cmp eax, JPEG_E_SINPERDIDAS
        je .var
        mov dword [im_var], jpg_t_jera
        cmp eax, JPEG_E_JERARQUICO
        je .var
        mov dword [im_var], 0
        mov eax, ME_ROTO
        ret
.var:
        mov eax, ME_VARIANTE
        ret

; -----------------------------------------------------------------------------
;  Lo que apunta la cámara (EXIF, en el segmento APP1): cómo hay que girar la
;  foto para verla derecha (un móvil guarda siempre el sensor tal cual y dice
;  aparte cómo se sujetaba), la marca y el modelo, y cuándo se hizo. Es un
;  pequeño archivo TIFF: una lista de datos con su número, y dentro otra (la
;  de EXIF) con la fecha de la toma. Todo se lee comprobando que cae dentro
; -----------------------------------------------------------------------------
jpg_exif:
        pushad
        mov byte [jx_camara], 0
        mov byte [jx_tomada], 0
        lea edi, [esi+ecx]          ; EDI=el final del archivo
        add esi, 2
.seg:
        lea eax, [esi+4]
        cmp eax, edi
        ja .x
        cmp byte [esi], 0xFF
        jne .x
        movzx eax, byte [esi+1]
        cmp al, 0xDA                ; (empieza la imagen: ya no hay más)
        je .x
        movzx ecx, word [esi+2]
        xchg cl, ch                 ; ECX=lo que mide el segmento
        cmp ecx, 2
        jb .x
        lea edx, [esi+ecx+2]
        cmp edx, edi
        ja .x
        cmp al, 0xE1
        jne .sig
        cmp ecx, 16
        jb .sig
        cmp dword [esi+4], 'Exif'
        jne .sig
        cmp word [esi+8], 0
        jne .sig
        lea ebx, [esi+10]           ; EBX=el TIFF
        sub ecx, 8
        mov [jx_len], ecx
        mov dword [jx_mm], 0
        cmp word [ebx], 'II'
        je .orden
        cmp word [ebx], 'MM'
        jne .x
        mov dword [jx_mm], 1
.orden:
        mov eax, 4
        call jx_32
        jc .x
        xor edx, edx
        call jx_lista
        cmp byte [jx_camara], 0
        je .t
        mov dword [im_camara], jx_camara
.t:
        cmp byte [jx_tomada], 0
        je .x
        mov dword [im_tomada], jx_tomada
        jmp .x
.sig:
        mov esi, edx
        jmp .seg
.x:
        popad
        ret
; (internas) el número de 16 o de 32 bits que hay en EAX (desde el principio
; del TIFF) -> EAX; CF=1 si cae fuera
jx_16:
        cmp eax, [jx_len]
        jae .no
        push ecx
        lea ecx, [eax+2]
        cmp ecx, [jx_len]
        pop ecx
        ja .no
        movzx eax, word [ebx+eax]
        cmp dword [jx_mm], 0
        je .ok
        xchg al, ah
.ok:
        clc
        ret
.no:
        stc
        ret
jx_32:
        cmp eax, [jx_len]
        jae .no
        push ecx
        lea ecx, [eax+4]
        cmp ecx, [jx_len]
        pop ecx
        ja .no
        mov eax, [ebx+eax]
        cmp dword [jx_mm], 0
        je .ok
        call mi_be32
.ok:
        clc
        ret
.no:
        stc
        ret
; (interna) la lista de datos que empieza en EAX; EDX=0 la principal, 1 la de EXIF
jx_lista:
        pushad
        mov esi, eax
        call jx_16
        jc .x
        mov ecx, eax                ; ECX=cuántos datos
        cmp ecx, 300
        ja .x
        add esi, 2
.dato:
        test ecx, ecx
        jz .x
        mov eax, esi
        call jx_16
        jc .x
        test edx, edx
        jnz .exif
        cmp eax, 0x0112             ; la orientación
        jne .d1
        lea eax, [esi+8]
        call jx_16
        jc .x
        dec eax
        cmp eax, 7
        ja .sig
        inc eax
        mov [im_orienta], eax
        jmp .sig
.d1:
        cmp eax, 0x010F             ; la marca
        je .camara
        cmp eax, 0x0110             ; y el modelo, detrás
        jne .d2
.camara:
        mov edi, jx_camara
        mov ebp, 30
        call jx_texto
        jmp .sig
.d2:
        cmp eax, 0x0132             ; la fecha del archivo (si no hay otra mejor)
        jne .d3
        cmp byte [jx_tomada], 0
        jne .sig
        jmp .fecha
.d3:
        cmp eax, 0x8769             ; la lista de EXIF
        jne .sig
        lea eax, [esi+8]
        call jx_32
        jc .sig
        push edx
        mov edx, 1
        call jx_lista
        pop edx
        jmp .sig
.exif:
        cmp eax, 0x9003             ; cuándo se hizo la foto
        jne .sig
        mov byte [jx_tomada], 0
.fecha:
        mov edi, jx_tomada
        mov ebp, 20
        call jx_texto
.sig:
        add esi, 12
        dec ecx
        jmp .dato
.x:
        popad
        ret
; (interna) el texto del dato ESI, añadido a EDI (tras un espacio, si ya
; había algo), EBP letras como mucho
jx_texto:
        pushad
        lea eax, [esi+4]
        call jx_32
        jc .x
        mov ecx, eax                ; ECX=cuántas letras dice
        lea edx, [esi+8]            ; hasta cuatro van en el propio dato
        cmp ecx, 4
        jbe .aqui
        mov eax, edx
        call jx_32
        jc .x
        mov edx, eax
.aqui:
        cmp edx, [jx_len]
        jae .x
        mov eax, [jx_len]
        sub eax, edx
        cmp ecx, eax
        jbe .c0
        mov ecx, eax
.c0:
        cmp byte [edi], 0           ; al final de lo que haya
        je .c1
.f:
        inc edi
        cmp byte [edi], 0
        jne .f
        mov byte [edi], ' '
        inc edi
.c1:
        test ecx, ecx
        jz .c9
        test ebp, ebp
        jz .c9
        mov al, [ebx+edx]
        test al, al
        jz .c9
        cmp al, ' '
        jb .raro
        cmp al, 126
        jbe .pon
.raro:
        mov al, '?'
.pon:
        mov [edi], al
        inc edi
        inc edx
        dec ecx
        dec ebp
        jmp .c1
.c9:
        mov byte [edi], 0
.x:
        popad
        ret

; EAX=1, 2 o 3: a la mitad, un cuarto o un octavo -> CF=1 si no se puede
jpg_reduce:
        mov [jpg_op], eax
        call jpg_mide
        test eax, eax
        jnz .no
        clc
        ret
.no:
        stc
        ret

jpg_prepara:
        mov esi, [jpg_dat]
        mov ecx, [jpg_tam]
        mov eax, [jpg_op]
        mov edi, [im_almp]
        mov edx, [im_alm]
        call jpeg_abre
        jc .mal
        xor eax, eax
        ret
.mal:
        mov eax, ME_ROTO
        ret

jpg_fila:
        push edi
        call jpeg_fila
        pop edi
        jc .falta
        mov ecx, [jp_sal_an]
        imul ecx, [jp_sal_nc]
        rep movsb
        clc
        ret
.falta:
        stc
        ret
%else
jpg_t_no    db "un JPEG: los JPEG todavía no se pueden ver", 0
jpg_abre:
        mov dword [im_var], jpg_t_no
        mov eax, ME_VARIANTE
        ret
jpg_prepara:
jpg_fila:
jpg_reduce:
        stc
        ret
%endif
