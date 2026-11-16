; =============================================================================
;  apaleta.inc - de color verdadero a la paleta de Hispa Ventanas: una fila de
;  puntos de rojo, verde y azul pasa a ser una fila de índices de la paleta
;  (un octeto por punto), con difusión de error, con tramado ordenado o al
;  color más cercano. Para cualquier programa que enseñe fotos.
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La paleta de Hispa Ventanas (ventanas/video.inc) son 256 colores: los 16
;  con nombre (0-15), un cubo de 6x6x6 (16-231: 16 + 36·rojo + 6·verde + azul,
;  cada uno de 0 a 5, de 51 en 51) y 24 grises (232-255, de 11 en 11). Con eso
;  una foto se ve bien si se reparte el error: lo que le falta o le sobra a
;  cada punto al redondearlo se les da a los vecinos que aún no se han hecho.
;
;  No depende de Hispa Ventanas (la paleta se le da) ni de quien lo incluye.
;  Se pone dentro del código (%include "apaleta.inc") y, en la «section
;  .bss», la macro APALETA_BSS (unos 37 K: la memoria de colores ya buscados).
;  No es reentrante: una imagen cada vez.
;
;    ap_inicia   ESI=la paleta (256 colores de cuatro octetos: rojo, verde,
;                azul, 0; la de «pal_rgb»). Una vez, antes de nada (y otra si
;                la paleta cambiara). Conserva los registros.
;    ap_empieza  ECX=ancho de la imagen  EDI=memoria para los errores de la
;                difusión: AP_MEM(ancho) octetos (0 si no se va a difundir).
;                Antes de la primera fila de cada imagen.
;    ap_fila     ESI=fila (tres octetos por punto: rojo, verde, azul)
;                EDI=destino (un octeto por punto)  ECX=puntos  EAX=el número
;                de la fila (para el tramado ordenado). Las filas, seguidas y
;                de arriba abajo. Como dice [ap_modo]:
;                   AP_CERCANO   el color más parecido, sin más
;                   AP_ORDENADO  tramado ordenado de 4x4 sobre el cubo
;                   AP_DIFUSION  difusión de error (Floyd-Steinberg) sobre
;                                los 256 colores
;                Conserva los registros.
;    ap_cercano  EAX=color (rojo en el octeto bajo, verde, azul) -> EAX=el
;                índice del más parecido de los 256. Deprisa: lo busca una vez
;                por cada casilla de 8x8x8 niveles (con el color de su centro)
;                y lo recuerda; por eso puede dar un color vecino del mejor,
;                a cuatro niveles como mucho.
;    ap_exacto   lo mismo, buscando con el color tal cual (despacio: para las
;                paletas propias de una imagen, que son 256 colores a lo más).
;
;  El más parecido es el de menor distancia (la suma de los cuadrados de las
;  diferencias de rojo, verde y azul) entre el del cubo que toca por redondeo,
;  los 16 con nombre y los 24 grises: 41 candidatos, no 256.
; =============================================================================
AP_CERCANO  equ 0
AP_ORDENADO equ 1
AP_DIFUSION equ 2
%define AP_MEM(an) (((an) + 2) * 12)

%macro APALETA_BSS 0
            alignb 4
ap_pal      resd 1
ap_modo     resd 1
ap_e0       resd 1                  ; los errores de la fila que se hace (palabras)
ap_e1       resd 1                  ; y los que van cayendo en la de debajo
ap_an       resd 1
ap_v        resd 3                  ; el punto, con el error que le toca
ap_c        resd 3
ap_mejor    resd 1
ap_mejor_d  resd 1
ap_y        resd 1
ap_niv      resb 256 + 64           ; nivel del cubo de cada valor (de -24 a 279)
ap_visto    resb 4096               ; un bit por casilla: ya se ha buscado
ap_cache    resb 32768              ; y el color que le tocó
%endmacro

; el tramado ordenado de 4x4 (matriz de Bayer)
ap_bayer    db 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5
; el valor que representa a cada casilla de 8 niveles
ap_rep      db 0
%assign i 1
%rep 30
            db i * 8 + 4
%assign i i + 1
%endrep
            db 255

ap_inicia:
        pushad
        mov [ap_pal], esi
        mov edi, ap_visto
        mov ecx, 4096 / 4
        xor eax, eax
        rep stosd
        ; el nivel del cubo (0 a 5) de cada valor con el umbral sumado: el
        ; valor v (de -24 a 279) está en ap_niv[v+24]
        mov edi, ap_niv
        mov ecx, -24
.n:
        mov eax, ecx
        test eax, eax
        jns .n1
        xor eax, eax
.n1:
        cmp eax, 255
        jbe .n2
        mov eax, 255
.n2:
        add eax, 25
        mov bl, 51
        div bl
        stosb
        inc ecx
        cmp ecx, 280
        jl .n
        popad
        ret

ap_empieza:
        pushad
        mov [ap_an], ecx
        mov [ap_e0], edi
        test edi, edi
        jz .x
        lea eax, [ecx+2]
        imul eax, 6
        add eax, edi
        mov [ap_e1], eax
        lea ecx, [ecx+2]
        imul ecx, 12
        xor al, al
        rep stosb
.x:
        popad
        ret

ap_cercano:
        push ebx
        push ecx
        mov ebx, eax                ; la casilla: 5 bits de cada componente
        shr ebx, 3
        and ebx, 31
        mov ecx, eax
        shr ecx, 11
        and ecx, 31
        shl ecx, 5
        or ebx, ecx
        mov ecx, eax
        shr ecx, 19
        and ecx, 31
        shl ecx, 10
        or ebx, ecx
        bt [ap_visto], ebx
        jnc .busca
        movzx eax, byte [ap_cache+ebx]
        pop ecx
        pop ebx
        ret
.busca:
        ; el color de la casilla: el de su centro (y en las dos de los
        ; extremos, el 0 y el 255: así el negro y el blanco siguen siéndolo)
        mov ecx, ebx
        shr ecx, 10
        movzx eax, byte [ap_rep+ecx]
        shl eax, 8
        mov ecx, ebx
        shr ecx, 5
        and ecx, 31
        mov al, [ap_rep+ecx]
        shl eax, 8
        mov ecx, ebx
        and ecx, 31
        mov al, [ap_rep+ecx]
        call ap_exacto
        mov [ap_cache+ebx], al
        bts [ap_visto], ebx
        pop ecx
        pop ebx
        ret

ap_exacto:
        pushad
        movzx ebx, al
        mov [ap_c], ebx
        movzx ebx, ah
        mov [ap_c+4], ebx
        shr eax, 16
        movzx eax, al
        mov [ap_c+8], eax
        mov dword [ap_mejor_d], 0x7FFFFFFF
        ; el del cubo, por redondeo de cada componente
        mov eax, [ap_c]
        movzx esi, byte [ap_niv+eax+24]
        imul esi, 36
        mov eax, [ap_c+4]
        movzx eax, byte [ap_niv+eax+24]
        lea eax, [eax+eax*2]
        lea esi, [esi+eax*2]
        mov eax, [ap_c+8]
        movzx eax, byte [ap_niv+eax+24]
        lea esi, [esi+eax+16]
        call .prueba
        xor esi, esi                ; los 16 con nombre
.p16:
        call .prueba
        inc esi
        cmp esi, 16
        jb .p16
        mov esi, 232                ; los grises
.pg:
        call .prueba
        inc esi
        cmp esi, 256
        jb .pg
        mov eax, [ap_mejor]
        mov [esp+28], eax
        popad
        ret
; (interna) ESI=un color de la paleta: ¿es el más parecido hasta ahora?
.prueba:
        mov edi, [ap_pal]
        lea edi, [edi+esi*4]
        movzx eax, byte [edi]
        sub eax, [ap_c]
        imul eax, eax
        mov ecx, eax
        movzx eax, byte [edi+1]
        sub eax, [ap_c+4]
        imul eax, eax
        add ecx, eax
        movzx eax, byte [edi+2]
        sub eax, [ap_c+8]
        imul eax, eax
        add ecx, eax
        cmp ecx, [ap_mejor_d]
        jae .p9
        mov [ap_mejor_d], ecx
        mov [ap_mejor], esi
.p9:
        ret

ap_fila:
        pushad
        mov [ap_y], eax
        test ecx, ecx
        jz .x
        mov eax, [ap_modo]
        cmp eax, AP_ORDENADO
        je .ordenado
        cmp eax, AP_DIFUSION
        jne .cercano
        cmp dword [ap_e0], 0        ; (sin memoria para los errores: al más cercano)
        jne .difusion
.cercano:
        mov eax, [esi]
        and eax, 0x00FFFFFF
        call ap_cercano
        stosb
        add esi, 3
        dec ecx
        jnz .cercano
.x:
        popad
        ret

; ---- tramado ordenado: a cada punto se le suma un umbral que depende de
;      dónde está (de -24 a 23: un escalón del cubo son 51) y se redondea ----
.ordenado:
        mov ebp, [ap_y]
        and ebp, 3
        shl ebp, 2
        xor edx, edx                ; EDX=x
.o1:
        mov ebx, edx
        and ebx, 3
        movzx ebx, byte [ap_bayer+ebp+ebx]
        imul ebx, 51
        shr ebx, 4                  ; EBX=umbral, de 0 a 47 (va con el +24 de la tabla)
        movzx eax, byte [esi]
        movzx eax, byte [ap_niv+eax+ebx]
        imul eax, 36
        push eax
        movzx eax, byte [esi+1]
        movzx eax, byte [ap_niv+eax+ebx]
        lea eax, [eax+eax*2]
        add eax, eax
        add [esp], eax
        movzx eax, byte [esi+2]
        movzx eax, byte [ap_niv+eax+ebx]
        add eax, 16
        add eax, [esp]
        add esp, 4
        stosb
        add esi, 3
        inc edx
        dec ecx
        jnz .o1
        popad
        ret

; ---- difusión de error: de lo que le falta a cada punto, 7/16 al de su
;      derecha y 3/16, 5/16 y 1/16 a los tres de debajo ----
%macro AP_CANAL 1
        movsx eax, word [ebx+%1*2]
        add eax, 8
        sar eax, 4
        movzx edx, byte [esi+%1]
        add eax, edx
        jns %%a
        xor eax, eax
%%a:
        cmp eax, 255
        jle %%b
        mov eax, 255
%%b:
        mov [ap_v+%1*4], eax
%endmacro
%macro AP_REPARTE 1
        mov edx, [ap_v+%1*4]
        movzx eax, byte [ebp+%1]
        sub edx, eax                ; EDX=el error
        lea eax, [edx*8]
        sub eax, edx
        add [ebx+6+%1*2], ax        ; 7/16 a la derecha
        mov eax, [ap_e1]
        add eax, [ap_y]             ; (aquí ap_y es el desplazamiento de la casilla)
        add [eax+6+%1*2], dx        ; 1/16 abajo a la derecha
        push ecx
        lea ecx, [edx+edx*2]
        add [eax-6+%1*2], cx        ; 3/16 abajo a la izquierda
        lea ecx, [edx+edx*4]
        add [eax+%1*2], cx          ; 5/16 debajo
        pop ecx
%endmacro
.difusion:
        mov ebx, [ap_e0]
        add ebx, 6                  ; (una casilla de margen a cada lado)
        mov dword [ap_y], 6
.d1:
        AP_CANAL 0
        AP_CANAL 1
        AP_CANAL 2
        mov eax, [ap_v+8]
        shl eax, 8
        or eax, [ap_v+4]
        shl eax, 8
        or eax, [ap_v]
        call ap_cercano
        stosb
        mov ebp, [ap_pal]
        lea ebp, [ebp+eax*4]
        AP_REPARTE 0
        AP_REPARTE 1
        AP_REPARTE 2
        add esi, 3
        add ebx, 6
        add dword [ap_y], 6
        dec ecx
        jnz .d1
        ; la fila de debajo pasa a ser la de ahora, y la nueva, a cero
        mov eax, [ap_e0]
        mov edi, eax
        xchg eax, [ap_e1]
        mov [ap_e0], eax
        mov ecx, [ap_an]
        add ecx, 2
        imul ecx, 6
        xor al, al
        rep stosb
        popad
        ret
