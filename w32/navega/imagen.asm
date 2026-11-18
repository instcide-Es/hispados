; =============================================================================
;  imagen.asm - el Navegador de Hispa-486: los formatos de imagen (PNG, GIF,
;  JPEG, BMP, ICO...) con el núcleo de Mirador TAL CUAL (programas/mirador/
;  nucleo.inc y un .inc por formato; programas/comun/desinfla_t.inc y
;  jpeg.inc), que no depende de Hispa Ventanas. Aquí solo está el puente para
;  llamarlo desde C (cdecl; el núcleo usa registros y el acarreo).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;    int img_abre(const u8 *datos, u32 n, u32 *medidas)
;        -> ME_* (0: bien); medidas[0]=ancho [1]=alto [2]=memoria de trabajo
;    int img_reduce(int k)            k=1, 2, 3: a 1/2, 1/4, 1/8 (JPEG)
;        -> 1 si se puede (cambian las medidas: img_medidas)
;    void img_medidas(u32 *medidas)
;    int img_prepara(void *memoria)   -> ME_*
;    int img_fila(int y, u8 *destino) -> 0, o -1 si esa fila ya no está
;        (rojo, verde, azul por punto; lo transparente, sobre blanco)
; =============================================================================
        bits 32
        cpu 386
%define MI_CON_JPEG
        section .text
        global img_abre, img_reduce, img_medidas, img_prepara, img_fila

%macro GUARDA 0
        push ebx
        push esi
        push edi
        push ebp
        cld
%endmacro
%macro VUELVE 0
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
%endmacro

img_abre:
        GUARDA
        mov dword [mi_fondo], 1            ; lo transparente, sobre el blanco de la página
        mov dword [mi_fondo_rgb], 0x00FFFFFF
        mov dword [mi_lector], 0
        mov esi, [esp+20]
        mov ecx, [esp+24]
        call mi_abre
        push eax
        mov edi, [esp+32]
        call medidas
        pop eax
        VUELVE
img_medidas:
        GUARDA
        mov edi, [esp+20]
        call medidas
        VUELVE
medidas:
        mov eax, [im_an]
        mov [edi], eax
        mov eax, [im_al]
        mov [edi+4], eax
        mov eax, [mi_mem]
        mov [edi+8], eax
        ret
img_reduce:
        GUARDA
        mov eax, [esp+20]
        call mi_reduce
        mov eax, 0
        jc .x
        inc eax
.x:
        VUELVE
img_prepara:
        GUARDA
        mov edi, [esp+20]
        call mi_prepara
        VUELVE
img_fila:
        GUARDA
        mov eax, [esp+20]
        mov edi, [esp+24]
        call mi_fila
        mov eax, 0
        jnc .x
        dec eax
.x:
        VUELVE

%include "desinfla_t.inc"
%include "jpeg.inc"
%include "nucleo.inc"
%include "bmp.inc"
%include "ico.inc"
%include "pcx.inc"
%include "gif.inc"
%include "png.inc"
%include "tga.inc"
%include "pnm.inc"
%include "jpg.inc"

        section .bss
        DESINFLA_T_BSS
        JPEG_BSS
        NUCLEO_BSS
        BMP_BSS
        ICO_BSS
        PCX_BSS
        GIF_BSS
        PNG_BSS
        TGA_BSS
        PNM_BSS
        JPG_BSS
