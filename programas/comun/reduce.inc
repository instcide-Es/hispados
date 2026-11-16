; =============================================================================
;  reduce.inc - cambia el tamaño de una imagen de color verdadero fila a fila:
;  al reducir, cada punto nuevo es la media de los que tapa (suavizado); al
;  ampliar, se repite el más cercano. Para cualquier programa que enseñe
;  imágenes más grandes (o más pequeñas) que el sitio que tiene.
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  No guarda la imagen: pide las filas de origen que le hacen falta a una
;  rutina de quien lo usa, siempre hacia delante, y devuelve una fila del
;  tamaño nuevo. Así una imagen enorme se reduce mientras se lee, sin tenerla
;  nunca entera en memoria.
;
;  No depende de nada. Se pone dentro del código (%include "reduce.inc") y,
;  en la «section .bss», la macro REDUCE_BSS. No es reentrante.
;
;    red_prepara  EAX=ancho y EBX=alto de origen  ECX=ancho y EDX=alto nuevos
;                 EDI=memoria de trabajo: RED_MEM(ancho de origen, ancho
;                 nuevo) octetos   ESI=la rutina que da las filas de origen:
;                     EAX=fila (desde 0)  EDI=destino (tres octetos por punto)
;                     -> CF=1 si esa fila no existe (imagen cortada)
;                 (puede cambiar todos los registros). Conserva los registros.
;    red_fila     EAX=fila nueva (desde 0; se piden en orden)  EDI=destino
;                 (tres octetos por punto, el ancho nuevo) -> CF=1 si no ha
;                 podido leer ninguna de las filas de origen que le tocan.
;                 Conserva los registros.
;
;  La fila nueva «y» tapa las de origen desde y·alto/nuevo hasta la anterior
;  a (y+1)·alto/nuevo (una al menos); y lo mismo las columnas. No hay pesos:
;  cada punto de origen cae entero en uno nuevo.
; =============================================================================
%define RED_MEM(ao, an) ((ao) * 3 + (an) * 16 + 32)

%macro REDUCE_BSS 0
            alignb 4
red_fuente  resd 1
red_an_o    resd 1
red_al_o    resd 1
red_an_n    resd 1
red_al_n    resd 1
red_fo      resd 1                  ; una fila de origen
red_ac      resd 1                  ; las sumas (tres de 32 bits por punto nuevo)
red_xb      resd 1                  ; dónde empieza cada columna nueva en el origen
red_n       resd 1                  ; filas sumadas
red_y1      resd 1
red_dest    resd 1
%endmacro

red_prepara:
        pushad
        mov [red_an_o], eax
        mov [red_al_o], ebx
        mov [red_an_n], ecx
        mov [red_al_n], edx
        mov [red_fuente], esi
        mov [red_fo], edi
        lea eax, [eax+eax*2]
        add eax, 7
        and eax, ~3
        add edi, eax
        mov [red_ac], edi
        lea eax, [ecx+ecx*2]
        lea edi, [edi+eax*4]
        mov [red_xb], edi
        xor ebx, ebx                ; cada frontera: i·ancho/nuevo
.x:
        mov eax, ebx
        mul dword [red_an_o]
        div dword [red_an_n]
        stosd
        inc ebx
        cmp ebx, [red_an_n]
        jbe .x
        popad
        ret

red_fila:
        pushad
        mov [red_dest], edi
        mov ebx, eax
        mul dword [red_al_o]        ; las filas de origen que le tocan
        div dword [red_al_n]
        mov esi, eax                ; ESI=la primera
        lea eax, [ebx+1]
        mul dword [red_al_o]
        div dword [red_al_n]
        cmp eax, esi
        ja .y1
        lea eax, [esi+1]
.y1:
        mov [red_y1], eax
        ; lo corriente al ver una imagen a su tamaño: tal cual
        mov ecx, [red_an_o]
        cmp ecx, [red_an_n]
        jne .suma
        dec eax
        cmp eax, esi
        jne .suma
        mov eax, esi
        mov edi, [red_dest]
        call [red_fuente]
        jc .mal
        jmp .bien
.suma:
        mov edi, [red_ac]
        mov ecx, [red_an_n]
        lea ecx, [ecx+ecx*2]
        xor eax, eax
        rep stosd
        mov dword [red_n], 0
.fila:
        cmp esi, [red_y1]
        jae .media
        push esi
        mov eax, esi
        mov edi, [red_fo]
        call [red_fuente]
        pop esi
        jc .media                   ; (cortada: vale con las que haya)
        inc dword [red_n]
        push esi
        mov edi, [red_ac]
        mov ebx, [red_xb]
        mov ebp, [red_an_n]
.col:
        mov esi, [ebx]              ; de la columna ESI a la anterior a ECX
        mov ecx, [ebx+4]
        cmp ecx, esi
        ja .c1
        lea ecx, [esi+1]
.c1:
        sub ecx, esi
        lea esi, [esi+esi*2]
        add esi, [red_fo]
.c2:
        movzx eax, byte [esi]
        add [edi], eax
        movzx eax, byte [esi+1]
        add [edi+4], eax
        movzx eax, byte [esi+2]
        add [edi+8], eax
        add esi, 3
        dec ecx
        jnz .c2
        add edi, 12
        add ebx, 4
        dec ebp
        jnz .col
        pop esi
        inc esi
        jmp .fila
.media:
        cmp dword [red_n], 0
        je .mal
        mov esi, [red_ac]
        mov edi, [red_dest]
        mov ebx, [red_xb]
        mov ebp, [red_an_n]
.m1:
        mov ecx, [ebx+4]            ; entre cuántos: columnas por filas
        sub ecx, [ebx]
        ja .m2
        mov ecx, 1
.m2:
        imul ecx, [red_n]
        push ebx
        mov ebx, ecx
        shr ebx, 1                  ; (redondeando)
        mov eax, [esi]
        add eax, ebx
        xor edx, edx
        div ecx
        stosb
        mov eax, [esi+4]
        add eax, ebx
        xor edx, edx
        div ecx
        stosb
        mov eax, [esi+8]
        add eax, ebx
        xor edx, edx
        div ecx
        stosb
        pop ebx
        add esi, 12
        add ebx, 4
        dec ebp
        jnz .m1
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
