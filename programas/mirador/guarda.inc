; =============================================================================
;  MIRADOR.HVP - guardar la imagen como PNG y como PCX
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como el BMP (salida.inc): de color verdadero si se guarda el original; si
;  no, con los 256 colores de Hispa Ventanas (lo que se ve).
;
;  PNG: de 8 bits con paleta o de 24 bits, sin entrelazar. La compresión es
;  Deflate de verdad pero sencilla: un solo bloque con los códigos de Huffman
;  fijos, y de lo que se repite solo se aprovechan las tiradas del mismo
;  octeto (una copia «a distancia 1»). A las filas de 24 bits se les pone el
;  filtro «Sub» (cada punto menos el de su izquierda), que convierte las
;  zonas lisas en ceros. Un dibujo (colores planos) queda en muy poco; una
;  foto ocupa más o menos como su BMP (en el peor caso, un 12 % más).
;  Los datos salen en trozos IDAT de 16 K, según se van haciendo.
;
;  PCX: versión 5, de 8 bits con la paleta al final o de 24 bits en tres
;  planos, con las tiradas de siempre (RLE).
;
;  Usa las tablas de largos de desinfla.inc (dsf_lbase, dsf_lext) y la del
;  CRC-32 de png.inc.
; =============================================================================
SAL_OB      equ 16384               ; lo que se junta antes de escribir en el disco

%macro GUARDA_BSS 0
            alignb 4
sal_fmt     resd 1                  ; 0 BMP, 1 PNG, 2 PCX
sal_ext     resd 1
sal_linea   resd 1                  ; un plano de una fila (PCX)
sal_ob      resd 1                  ; el búfer de salida
sal_on      resd 1                  ; octetos que tiene
sal_mal     resd 1                  ; ha fallado el disco
sal_idat    resd 1                  ; 1: lo que sale va en trozos IDAT
df_bits     resd 1                  ; Deflate: los bits que aún no llenan un octeto
df_nbits    resd 1
df_ult      resd 1                  ; el último octeto y cuántas veces más ha venido
df_rep      resd 1
df_hay      resd 1
df_a        resd 1                  ; la suma Adler-32
df_b        resd 1
sal_cab     resb 16
df_izq      resd 1                  ; el punto de la izquierda (filtro «Sub»)
%endmacro

t_ext_png   db ".PNG", 0
t_ext_pcx   db ".PCX", 0

; (interna) pide el bloque (una fila, un plano y el búfer) y crea ruta2.
; CF=1 si no se puede
sal_abre:
        pushad
        mov ecx, [ma_an]
        shl ecx, 2
        add ecx, SAL_OB + 64
        HV mem_bloque
        jc .mal
        mov [sal_p], eax
        mov [sal_h], edx
        mov ecx, [ma_an]
        lea ecx, [ecx+ecx*2+8]
        add eax, ecx
        mov [sal_linea], eax
        add eax, [ma_an]
        add eax, 8
        mov [sal_ob], eax
        xor eax, eax
        mov [sal_on], eax
        mov [sal_mal], eax
        mov [sal_idat], eax
        mov esi, ruta2
        HV dos_crear
        jc .mal1
        mov [sal_man], eax
        popad
        clc
        ret
.mal1:
        mov edx, [sal_h]
        HV mem_bloque_soltar
.mal:
        popad
        stc
        ret

; (interna) acaba: lo que quede, al disco; cierra y suelta. Si algo ha
; fallado, el archivo a medias se borra. CF=1 si ha fallado
sal_cierra:
        pushad
        call sal_vuelca
        mov ebx, [sal_man]
        HV dos_cerrar
        mov edx, [sal_h]
        HV mem_bloque_soltar
        cmp dword [sal_mal], 0
        je .bien
        mov esi, ruta2
        HV dos_borrar
        popad
        stc
        ret
.bien:
        popad
        clc
        ret

; (interna) ESI=datos ECX=octetos: al disco ya (CF=1 y [sal_mal] si falla)
sal_escribe:
        pushad
        cmp dword [sal_mal], 0
        jne .mal
        mov ebx, [sal_man]
        HV dos_escribir
        jc .mal
        cmp eax, ecx
        jb .mal
        popad
        clc
        ret
.mal:
        mov dword [sal_mal], 1
        popad
        stc
        ret

; (interna) AL: un octeto más de salida
sal_octeto:
        push ebx
        mov ebx, [sal_on]
        add ebx, [sal_ob]
        mov [ebx], al
        pop ebx
        inc dword [sal_on]
        cmp dword [sal_on], SAL_OB
        jb .x
        call sal_vuelca
.x:
        ret

; (interna) el búfer, al disco: tal cual, o como un trozo IDAT (su largo, su
; nombre, los datos y la suma de nombre y datos)
sal_vuelca:
        pushad
        mov ecx, [sal_on]
        test ecx, ecx
        jz .x
        cmp dword [sal_idat], 0
        je .datos
        mov eax, ecx
        call mi_be32
        mov [sal_cab], eax
        mov dword [sal_cab+4], 'IDAT'
        mov esi, sal_cab
        mov ecx, 8
        call sal_escribe
        mov eax, 0xFFFFFFFF
        mov esi, sal_cab + 4
        mov ecx, 4
        call png_crc
        mov esi, [sal_ob]
        mov ecx, [sal_on]
        call png_crc
        not eax
        call mi_be32
        mov [sal_cab+8], eax
        mov ecx, [sal_on]
.datos:
        mov esi, [sal_ob]
        call sal_escribe
        cmp dword [sal_idat], 0
        je .x
        mov esi, sal_cab + 8
        mov ecx, 4
        call sal_escribe
.x:
        mov dword [sal_on], 0
        popad
        ret

; (interna) la fila EBP de la imagen, en [sal_p]: un octeto por punto (el
; mapa) o tres (el original), según [sal_24]
sal_fila_haz:
        pushad
        mov eax, ebp
        mul dword [ma_an]
        mov ecx, [ma_an]
        mov edi, [sal_p]
        cmp dword [sal_24], 0
        jne .v24
        mov esi, [mapa_p]
        add esi, eax
        rep movsb
        popad
        ret
.v24:
        lea esi, [eax+eax*2]
        add esi, [orig_p]
        lea ecx, [ecx+ecx*2]
        rep movsb
        popad
        ret

; =============================================================================
;  PCX
; =============================================================================
escribe_pcx:
        pushad
        call sal_abre
        jc .mal
        mov edi, linea              ; la cabecera: 128 octetos
        xor eax, eax
        mov ecx, 32
        rep stosd
        mov dword [linea], 0x0801050A       ; PCX, versión 5, con tiradas, 8 bits
        mov eax, [ma_an]
        dec eax
        mov [linea+8], ax
        mov eax, [ma_al]
        dec eax
        mov [linea+10], ax
        mov dword [linea+12], 72 + 72 * 65536
        mov byte [linea+65], 1
        cmp dword [sal_24], 0
        je .c1
        mov byte [linea+65], 3
.c1:
        mov ebx, [ma_an]            ; octetos por fila de un plano: un número par
        inc ebx
        and ebx, ~1
        mov [linea+66], bx
        mov word [linea+68], 1
        mov esi, linea
        mov ecx, 128
        call sal_escribe
        xor ebp, ebp
.fila:
        cmp ebp, [ma_al]
        jae .pal
        call sal_fila_haz
        mov esi, [sal_p]
        cmp dword [sal_24], 0
        jne .planos
        mov ecx, [ma_an]
        mov edi, [sal_linea]
        rep movsb
        call .plano
        jmp .sig
.planos:
        xor edx, edx                ; EDX=el plano: rojo, verde, azul
.pl:
        mov esi, [sal_p]
        add esi, edx
        mov edi, [sal_linea]
        mov ecx, [ma_an]
.pl1:
        mov al, [esi]
        stosb
        add esi, 3
        dec ecx
        jnz .pl1
        call .plano
        inc edx
        cmp edx, 3
        jb .pl
.sig:
        cmp dword [sal_mal], 0
        jne .fin
        inc ebp
        jmp .fila
.pal:
        cmp dword [sal_24], 0       ; la paleta, al final, detrás de un 12
        jne .fin
        mov al, 12
        call sal_octeto
        mov esi, [hv_api + HV_pal_rgb * 4]
        mov ecx, 256
.p1:
        mov al, [esi]
        call sal_octeto
        mov al, [esi+1]
        call sal_octeto
        mov al, [esi+2]
        call sal_octeto
        add esi, 4
        dec ecx
        jnz .p1
.fin:
        call sal_cierra
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; (interna) el plano de [sal_linea] (EBX octetos, el último de relleno si el
; ancho es impar), con tiradas: hasta 63 iguales van como «0C0h + cuántos» y
; el octeto; uno suelto va tal cual si no se confunde con eso
.plano:
        pushad
        mov esi, [sal_linea]
        mov ecx, [ma_an]
        cmp ecx, ebx
        jae .t
        mov byte [esi+ecx], 0
.t:
        mov ecx, ebx                ; ECX=octetos que quedan
.t1:
        test ecx, ecx
        jz .t9
        mov al, [esi]
        xor edx, edx                ; EDX=cuántos iguales
.t2:
        inc edx
        cmp edx, ecx
        jae .t3
        cmp edx, 63
        jae .t3
        cmp al, [esi+edx]
        je .t2
.t3:
        add esi, edx
        sub ecx, edx
        cmp edx, 1
        ja .t4
        cmp al, 0xC0
        jb .t5
.t4:
        push eax
        mov al, dl
        or al, 0xC0
        call sal_octeto
        pop eax
.t5:
        call sal_octeto
        jmp .t1
.t9:
        popad
        ret

; =============================================================================
;  PNG
; =============================================================================
png_firma   db 0x89, "PNG", 13, 10, 26, 10

; (interna) un trozo entero: EDX=su nombre ESI=sus datos ECX=cuántos
png_e_trozo:
        pushad
        mov eax, ecx
        call mi_be32
        mov [sal_cab], eax
        mov [sal_cab+4], edx
        push esi
        push ecx
        mov esi, sal_cab
        mov ecx, 8
        call sal_escribe
        mov eax, 0xFFFFFFFF
        mov esi, sal_cab + 4
        mov ecx, 4
        call png_crc
        pop ecx
        pop esi
        test ecx, ecx
        jz .t1
        call sal_escribe
        call png_crc
.t1:
        not eax
        call mi_be32
        mov [sal_cab+8], eax
        mov esi, sal_cab + 8
        mov ecx, 4
        call sal_escribe
        popad
        ret

escribe_png:
        pushad
        call sal_abre
        jc .mal
        call png_crc_tabla
        mov esi, png_firma
        mov ecx, 8
        call sal_escribe
        mov eax, [ma_an]            ; IHDR: medidas, 8 bits, con paleta (3) o color (2)
        call mi_be32
        mov [linea], eax
        mov eax, [ma_al]
        call mi_be32
        mov [linea+4], eax
        mov dword [linea+8], 0x00000308
        mov byte [linea+12], 0
        cmp dword [sal_24], 0
        je .c1
        mov byte [linea+9], 2
.c1:
        mov edx, 'IHDR'
        mov esi, linea
        mov ecx, 13
        call png_e_trozo
        cmp dword [sal_24], 0
        jne .datos
        mov esi, [hv_api + HV_pal_rgb * 4]  ; PLTE: la paleta de Hispa Ventanas
        mov edi, linea
        mov ecx, 256
.p1:
        movsw
        movsb
        inc esi
        dec ecx
        jnz .p1
        mov edx, 'PLTE'
        mov esi, linea
        mov ecx, 768
        call png_e_trozo
.datos:
        ; ---- los datos: zlib (78h 01h), un bloque de Huffman fijo y la suma ----
        mov dword [sal_idat], 1
        xor eax, eax
        mov [df_bits], eax
        mov [df_nbits], eax
        mov [df_hay], eax
        mov [df_rep], eax
        mov [df_b], eax
        mov dword [df_a], 1
        mov al, 0x78
        call sal_octeto
        mov al, 0x01
        call sal_octeto
        mov eax, 3                  ; último bloque (1) y de códigos fijos (01)
        mov ecx, 3
        call df_pon
        xor ebp, ebp
.fila:
        cmp ebp, [ma_al]
        jae .fin
        call sal_fila_haz
        mov esi, [sal_p]
        mov ecx, [ma_an]
        cmp dword [sal_24], 0
        jne .f24
        xor al, al                  ; con paleta: sin filtro
        call df_dato
.f8:
        lodsb
        call df_dato
        dec ecx
        jnz .f8
        jmp .sig
.f24:
        mov al, 1                   ; color: cada punto, menos el de su izquierda
        call df_dato
        xor ebx, ebx                ; EBX=el punto de la izquierda
        lea ecx, [ecx+ecx*2]
        xor edx, edx                ; EDX=componente
.f24b:
        lodsb
        mov ah, al
        sub al, [df_izq+edx]
        mov [df_izq+edx], ah
        call df_dato
        inc edx
        cmp edx, 3
        jb .f24c
        xor edx, edx
.f24c:
        dec ecx
        jnz .f24b
        mov dword [df_izq], 0       ; (la fila siguiente empieza sin vecino)
.sig:
        cmp dword [sal_mal], 0
        jne .fin
        inc ebp
        jmp .fila
.fin:
        call df_tirada              ; lo que quedara repetido
        xor eax, eax                ; el código de fin de bloque (256): siete ceros
        mov ecx, 7
        call df_pon
        mov eax, 0                  ; hasta completar el octeto
        mov ecx, 7
        call df_pon
        mov eax, [df_b]             ; la suma Adler-32, con el octeto de más peso delante
        shl eax, 16
        or eax, [df_a]
        mov ecx, 4
.ad:
        rol eax, 8
        call sal_octeto
        dec ecx
        jnz .ad
        call sal_vuelca
        mov dword [sal_idat], 0
        mov edx, 'IEND'
        xor ecx, ecx
        call png_e_trozo
        call sal_cierra
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; (interna) EAX=bits (el primero que sale, el de menos peso) ECX=cuántos
df_pon:
        push eax
        push ecx
        push edx
        mov edx, ecx
        mov ecx, [df_nbits]
        shl eax, cl
        or [df_bits], eax
        add ecx, edx
.o:
        cmp ecx, 8
        jb .x
        mov al, [df_bits]
        call sal_octeto
        shr dword [df_bits], 8
        sub ecx, 8
        jmp .o
.x:
        mov [df_nbits], ecx
        pop edx
        pop ecx
        pop eax
        ret
; (interna) un código de Huffman: EAX=código ECX=bits. Estos van con el bit
; de más peso por delante: se le da la vuelta
df_codigo:
        push eax
        push ebx
        push edx
        mov edx, ecx
        xor ebx, ebx
.v:
        shr eax, 1
        rcl ebx, 1
        dec edx
        jnz .v
        mov eax, ebx
        call df_pon
        pop edx
        pop ebx
        pop eax
        ret
; (interna) AL=un octeto tal cual (un «literal» de los códigos fijos)
df_literal:
        push eax
        push ecx
        movzx eax, al
        mov ecx, 8
        cmp eax, 144
        jae .l9
        add eax, 0x30               ; de 0 a 143: ocho bits, desde 00110000
        jmp .l
.l9:
        add eax, 0x190 - 144        ; de 144 a 255: nueve, desde 110010000
        inc ecx
.l:
        call df_codigo
        pop ecx
        pop eax
        ret
; (interna) AL=el octeto siguiente de los datos sin comprimir: lo suma y lo
; codifica, juntando las tiradas del mismo octeto
df_dato:
        pushad
        movzx eax, al
        mov ebx, [df_a]             ; Adler-32
        add ebx, eax
        cmp ebx, 65521
        jb .a1
        sub ebx, 65521
.a1:
        mov [df_a], ebx
        add ebx, [df_b]
        cmp ebx, 65521
        jb .a2
        sub ebx, 65521
.a2:
        mov [df_b], ebx
        cmp dword [df_hay], 0
        je .nuevo
        cmp eax, [df_ult]
        jne .otro
        cmp dword [df_rep], 258
        jae .otro
        inc dword [df_rep]
        popad
        ret
.otro:
        call df_tirada
.nuevo:
        mov [df_ult], eax
        mov dword [df_hay], 1
        call df_literal
        popad
        ret
; (interna) la tirada pendiente: [df_rep] veces más el último octeto. De tres
; en adelante, una copia a distancia 1; menos, tal cual
df_tirada:
        pushad
        mov edx, [df_rep]
        mov dword [df_rep], 0
        cmp edx, 3
        jae .copia
.l:
        test edx, edx
        jz .x
        mov al, [df_ult]
        call df_literal
        dec edx
        jmp .l
.copia:
        ; el código del largo: el último de la tabla que no pasa de EDX
        mov ebx, 28
.b:
        movzx eax, word [dsf_lbase+ebx*2]
        cmp eax, edx
        jbe .es
        dec ebx
        jmp .b
.es:
        sub edx, eax                ; EDX=lo que se dice con los bits de más
        lea eax, [ebx+1]            ; el símbolo 257+EBX: de 257 a 279, siete bits
        mov ecx, 7                  ; (su número menos 256); de 280 a 287, ocho
        cmp ebx, 23                 ; desde 11000000
        jb .c1
        lea eax, [ebx-23+0xC0]
        inc ecx
.c1:
        call df_codigo
        movzx ecx, byte [dsf_lext+ebx]
        test ecx, ecx
        jz .c2
        mov eax, edx
        call df_pon
.c2:
        xor eax, eax                ; la distancia 1: cinco ceros
        mov ecx, 5
        call df_pon
.x:
        popad
        ret
