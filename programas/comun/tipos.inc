; =============================================================================
;  tipos.inc - los tipos de letra que comparten los programas de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los tipos están en cuatro archivos, al lado de los módulos (\HISPADOS\PROGS):
;  TIPOS.DAT (la letra normal, en casilla de 16 puntos de pantalla) y
;  TIPOS12.DAT, TIPOS24.DAT y TIPOS32.DAT (los otros tamaños). Los hace
;  tools/hazfuentes.py, que explica el archivo; de dónde sale cada tipo y su
;  licencia, en docs/TIPOS-DE-LETRA.md.
;
;  Los caracteres, en la página de Ventanas: la 437 con Á, Ó, €... en códigos
;  que no se pintaban (tools/hispa437.py).
;
;  El tipo 0 es «Hispa», la letra de ancho fijo de Hispa Ventanas (8x16). Si
;  falta un archivo, o un tipo no trae un carácter, se pone esa letra, a la
;  escala que más se acerque: todo sigue funcionando.
;
;  Los tamaños (de 0 a TL_TAMANOS-1; el 0 es el normal, para que un documento
;  sin tamaños salga como siempre):
;
;      tamaño   0    1    2    3    4    5
;      puntos   12   9    18   24   36   48
;      casilla  16   12   24   32   48   64   (36 y 48: el doble de 18 y 24)
;
;  tl_orden dice en qué orden enseñarlos (de menor a mayor) y vale también al
;  revés: tl_orden[tl_orden[i]] = i.
;
;  Se incluye en el código del programa y, en su «section .bss», se pone la
;  macro TIPOS_BSS. Usa la API de Hispa Ventanas. La memoria de cada archivo
;  se pide al leerlo (mem_bloque), justo la que ocupa: un programa que solo
;  usa la letra normal gasta unos 86 K, no los 640 K de todos los tamaños.
;  Al cerrarse, el programa llama a tl_soltar para devolverla.
;
;    tl_cargar   ESI=ruta del módulo (la que recibe en su entrada): lee los
;                tipos de la letra normal; los otros tamaños se leen la primera
;                vez que hacen falta. Deja en [tl_n] cuántos tipos hay (con
;                «Hispa»)
;    tl_soltar   devuelve la memoria de los tipos (al cerrar el programa);
;                después hay que volver a llamar a tl_cargar para usarlos
;    tl_nombre   EAX=tipo -> ESI=su nombre
;    tl_puntos   EDX=tamaño -> ESI=sus puntos, en letra («12»)
;    tl_alto_de  EDX=tamaño -> EAX=el alto de su casilla (el de la línea)
;    tl_letra    EAX=tipo ECX=carácter EDX=tamaño -> ESI=las filas de su dibujo
;                (el punto de la izquierda, el bit más alto del primer octeto)
;                y, en [tl_g_filas], cuántas filas; [tl_g_bpr], octetos por
;                fila; [tl_g_esc], a qué escala se pinta (cada punto, un
;                cuadrado de ese lado); [tl_g_an], lo que avanza (ya a escala);
;                [tl_g_alto], el alto de su casilla (ya a escala). La línea
;                base está a tres cuartos de la casilla
;    tl_ancho    EAX=tipo ECX=carácter EDX=tamaño -> EAX=lo que avanza
; =============================================================================
TL_TIPOS    equ 32                  ; tipos, a lo más (con «Hispa»)
TL_TAMANOS  equ 6
TL_TOPE     equ 512 * 1024          ; lo más que puede ocupar un archivo de tipos

%macro TIPOS_BSS 0
tl_n        resd 1
tl_tam      resd 1
tl_g_filas  resd 1
tl_g_bpr    resd 1
tl_g_esc    resd 1
tl_g_an     resd 1
tl_g_alto   resd 1
tl_est      resb 4                  ; de cada archivo: 0 sin leer, 1 leído, 2 no está
tl_ruta     resb 128
tl_ruta_fin resd 1                  ; dónde va el nombre del archivo en tl_ruta
tl_buf      resd 4                  ; de cada archivo: dónde está en memoria...
tl_mango    resd 4                  ; ...y el mango de su bloque (para soltarlo)
%endmacro

; los tamaños: el alto de la casilla, de qué archivo sale y a qué escala
tl_alto     db 16, 12, 24, 32, 48, 64
tl_fich     db 1, 0, 2, 3, 2, 3
tl_esc      db 1, 1, 1, 1, 2, 2
tl_orden    db 1, 0, 2, 3, 4, 5
tl_pt0      db "12", 0
tl_pt1      db "9", 0
tl_pt2      db "18", 0
tl_pt3      db "24", 0
tl_pt4      db "36", 0
tl_pt5      db "48", 0
tl_hispa    db "Hispa", 0
tl_a0       db "TIPOS12.DAT", 0
tl_a1       db "TIPOS.DAT", 0
tl_a2       db "TIPOS24.DAT", 0
tl_a3       db "TIPOS32.DAT", 0
            align 4
tl_pts      dd tl_pt0, tl_pt1, tl_pt2, tl_pt3, tl_pt4, tl_pt5
; los archivos: su nombre, dónde se guarda, lo que cabe y el alto de su casilla
tl_arch     dd tl_a0, tl_a1, tl_a2, tl_a3
tl_altos    dd 12, 16, 24, 32

tl_cargar:
        pushad
        call tl_soltar              ; (por si se llama dos veces)
        mov dword [tl_n], 1
        mov dword [tl_est], 0
        mov edi, tl_ruta            ; la carpeta del módulo
        mov ebx, edi
        mov ecx, 110
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c1
        mov ebx, edi
.c1:
        test al, al
        jz .c2
        dec ecx
        jnz .c
.c2:
        mov [tl_ruta_fin], ebx
        mov eax, 1                  ; la letra normal
        call tl_carga_f
        cmp byte [tl_est+1], 1
        jne .x
        mov eax, [tl_buf+4]
        movzx eax, byte [eax+8]
        mov [tl_n], eax
.x:
        popad
        ret

tl_soltar:
        pushad
        xor ebx, ebx
.b:
        cmp dword [tl_buf+ebx*4], 0
        je .s
        mov edx, [tl_mango+ebx*4]
        HV mem_bloque_soltar
        mov dword [tl_buf+ebx*4], 0
.s:
        mov byte [tl_est+ebx], 0
        inc ebx
        cmp ebx, 4
        jb .b
        mov dword [tl_n], 1
        popad
        ret

; (interna) lee el archivo EAX (0-3) y lo comprueba; deja su estado en tl_est
tl_carga_f:
        pushad
        mov ebp, eax
        mov byte [tl_est+ebp], 2
        mov edi, [tl_ruta_fin]
        test edi, edi
        jz .x
        mov esi, [tl_arch+ebp*4]
.n:
        lodsb
        stosb
        test al, al
        jnz .n
        mov esi, tl_ruta
        xor al, al
        HV dos_abrir
        jc .x
        mov ebx, eax                ; EBX=el manejador
        mov al, 2                   ; cuánto ocupa: el puntero, al final...
        xor edx, edx
        HV dos_puntero
        jc .cierra
        mov [tl_tam], eax
        xor al, al                  ; ...y otra vez al principio
        xor edx, edx
        HV dos_puntero
        jc .cierra
        mov ecx, [tl_tam]
        cmp ecx, 12 + 32
        jb .cierra
        cmp ecx, TL_TOPE
        ja .cierra
        push ebx                    ; la memoria justa para él
        HV mem_bloque               ; -> EAX=dónde EDX=su mango
        pop ebx
        jc .cierra
        mov [tl_buf+ebp*4], eax
        mov [tl_mango+ebp*4], edx
        mov edi, eax                ; se lee a trozos (de una vez, DOS no pasa
.lee:                               ; de 64 K)
        mov ecx, [tl_buf+ebp*4]
        add ecx, [tl_tam]
        sub ecx, edi
        jz .leido
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .leido
        test eax, eax
        jz .leido
        add edi, eax
        jmp .lee
.leido:
        HV dos_cerrar
        mov ebx, [tl_buf+ebp*4]
        mov eax, edi
        sub eax, ebx                ; EAX=lo leído
        call .mira
        cmp byte [tl_est+ebp], 1    ; si no vale, su memoria se devuelve
        je .x
        mov edx, [tl_mango+ebp*4]
        HV mem_bloque_soltar
        mov dword [tl_buf+ebp*4], 0
        jmp .x
.cierra:
        HV dos_cerrar
        jmp .x
; (interna) EBX=el archivo en memoria EAX=octetos: lo comprueba y, si vale,
; pone su estado a 1
.mira:
        pushad
        cmp eax, 12 + 32
        jb .mx
        cmp dword [ebx], 'TIPO'
        jne .mx
        cmp dword [ebx+4], 'S2' | (26 << 16)
        jne .mx
        movzx ecx, byte [ebx+8]     ; cuántos tipos
        test ecx, ecx
        jz .mx
        cmp ecx, TL_TIPOS
        ja .mx
        movzx edx, byte [ebx+9]     ; el alto de la casilla: el de este archivo
        cmp edx, [tl_altos+ebp*4]
        jne .mx
        mov edx, ecx
        shl edx, 5
        add edx, 12
        cmp edx, eax
        ja .mx
        lea esi, [ebx+12]           ; que cada tipo esté entero en lo leído
.v:
        mov byte [esi+23], 0
        mov edi, [esi+28]
        test edi, edi
        jz .v1                      ; (0: ese tipo no está en este archivo)
        cmp edi, 448
        jb .mx
        mov edx, [esi+24]
        cmp edx, eax
        jae .mx
        add edx, edi
        jc .mx
        cmp edx, eax
        ja .mx
.v1:
        add esi, 32
        dec ecx
        jnz .v
        mov byte [tl_est+ebp], 1
.mx:
        popad
        ret
.x:
        popad
        ret

tl_nombre:
        mov esi, tl_hispa
        test eax, eax
        jz .x
        cmp eax, [tl_n]
        jae .x
        mov esi, eax
        shl esi, 5
        add esi, [tl_buf+4]
        add esi, 12
.x:
        ret

tl_puntos:
        mov esi, tl_pt0
        cmp edx, TL_TAMANOS
        jae .x
        mov esi, [tl_pts+edx*4]
.x:
        ret

tl_alto_de:
        mov eax, 16
        cmp edx, TL_TAMANOS
        jae .x
        movzx eax, byte [tl_alto+edx]
.x:
        ret

tl_letra:
        push eax
        push ebx
        push ecx
        push edx
        push edi
        cmp edx, TL_TAMANOS
        jb .t
        xor edx, edx
.t:
        movzx ebx, byte [tl_alto+edx]
        mov [tl_g_alto], ebx
        movzx ebx, byte [tl_esc+edx]
        mov [tl_g_esc], ebx
        movzx edi, byte [tl_fich+edx]       ; EDI=de qué archivo
        cmp ecx, 32
        jb .hispa
        cmp ecx, 255
        ja .hispa
        cmp byte [tl_est+edi], 0    ; (se lee la primera vez que hace falta)
        jne .leido
        push eax
        mov eax, edi
        call tl_carga_f
        pop eax
.leido:
        cmp byte [tl_est+edi], 1
        jne .hispa
        mov ebx, [tl_buf+edi*4]
        movzx esi, byte [ebx+8]
        cmp eax, esi
        jae .hispa
        shl eax, 5
        mov esi, [ebx+12+eax+28]    ; ESI=lo que ocupa el tipo
        test esi, esi
        jz .hispa
        add ebx, [ebx+12+eax+24]    ; EBX=donde empieza
        movzx eax, word [ebx+ecx*2-64]
        test eax, eax
        jz .hispa
        lea edx, [eax+2]
        cmp edx, esi
        ja .hispa
        movzx edx, byte [ebx+eax+1] ; octetos por fila
        mov [tl_g_bpr], edx
        imul edx, [tl_altos+edi*4]  ; que el dibujo esté entero en su tipo
        lea edx, [edx+eax+2]
        cmp edx, esi
        ja .hispa
        mov edx, [tl_altos+edi*4]
        mov [tl_g_filas], edx
        lea esi, [ebx+eax+2]
        movzx eax, byte [esi-2]
        imul eax, [tl_g_esc]
        mov [tl_g_an], eax
        jmp short .x
.hispa:
        ; la letra de Hispa Ventanas: 8x16, a la escala que más se acerque
        mov eax, [tl_g_alto]
        shr eax, 4
        jnz .h1
        inc eax
.h1:
        mov [tl_g_esc], eax
        shl eax, 3
        mov [tl_g_an], eax
        shl eax, 1
        mov [tl_g_alto], eax
        mov dword [tl_g_bpr], 1
        mov dword [tl_g_filas], 16
        mov esi, [esp+8]            ; (el carácter)
        and esi, 0xFF
        shl esi, 4
        add esi, [hv_api + HV_fuente8x16 * 4]
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

tl_ancho:
        push esi
        call tl_letra
        mov eax, [tl_g_an]
        pop esi
        ret
