; =============================================================================
;  MIRADOR.HVP - el formato JPEG: el puente con programas/comun/jpeg.inc
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El descodificador es la pieza común (jpeg.inc: lea su cabecera para saber
;  qué JPEG admite); aquí solo se le da la forma de un formato de Mirador. Es
;  el único que sabe entregar la imagen ya reducida a la mitad, un cuarto o
;  un octavo sin hacerla entera (mi_reduce): así una foto de muchos
;  megapuntos se abre con poca memoria y deprisa.
;
;  Si se ensambla sin MI_CON_JPEG, los JPEG se reconocen y se dice que
;  todavía no se pueden ver.
; =============================================================================
%macro JPG_BSS 0
            alignb 4
jpg_dat     resd 1
jpg_tam     resd 1
jpg_op      resd 1                  ; las opciones (la reducción)
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
