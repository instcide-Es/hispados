; =============================================================================
;  Hispa Ventanas - capa de dibujo (el GDI propio)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Todo se dibuja en la superficie: un mapa de sup_ancho x sup_alto puntos de
;  un byte (índice de la paleta) en memoria extendida. Nada de aquí toca la
;  pantalla: eso lo hace el driver (vid_presentar).
;
;  Estado de dibujo (lo que en las etapas siguientes será el «contexto»):
;    gdi_color     color con el que se dibuja
;    gdi_negrita   texto en negrita (distinto de 0)
;    rec_x0, rec_y0, rec_x1, rec_y1   rectángulo de recorte (x1, y1 fuera)
;
;  Todas las funciones conservan los registros. Coordenadas con signo: lo que
;  cae fuera del recorte no se dibuja.
;    gdi_iniciar                       superficie = pantalla, recorte total
;    gdi_recorte_total
;    gdi_fijar_recorte  EAX=x0 EBX=y0 ECX=x1 EDX=y1
;    gdi_cortar         EAX=x EBX=y ECX=ancho EDX=alto  (recorte = recorte ∩ rect.)
;    gdi_punto          EAX=x EBX=y
;    gdi_linea          EAX=x0 EBX=y0 ECX=x1 EDX=y1
;    gdi_rect           EAX=x EBX=y ECX=ancho EDX=alto  (lleno)
;    gdi_marco          EAX=x EBX=y ECX=ancho EDX=alto  (borde de un punto)
;    gdi_mosaico        EAX=x EBX=y ECX=ancho EDX=alto ESI=mosaico de 16x16
;    gdi_copiar         EAX=x EBX=y ECX=ancho EDX=alto ESI=mapa (ancho x alto)
;    gdi_copiar_clave   igual, pero los puntos de valor CLAVE no se copian
;    gdi_leer           EAX=x EBX=y ECX=ancho EDX=alto EDI=mapa (de la superficie)
;    gdi_texto          EAX=x EBX=y ESI=cadena acabada en 0 (fuente de 8x16)
;    gdi_ancho_texto    ESI=cadena -> EAX=ancho en puntos
; =============================================================================

%macro COLOR 1
        mov byte [gdi_color], %1
%endmacro

gdi_iniciar:
        push eax
        mov eax, [vid_ancho]
        mov [sup_ancho], eax
        mov eax, [vid_alto]
        mov [sup_alto], eax
        pop eax
gdi_recorte_total:
        push eax
        xor eax, eax
        mov [rec_x0], eax
        mov [rec_y0], eax
        mov eax, [sup_ancho]
        mov [rec_x1], eax
        mov eax, [sup_alto]
        mov [rec_y1], eax
        pop eax
        ret

gdi_fijar_recorte:
        call gdi_recorte_total
        pushad
        sub ecx, eax
        sub edx, ebx
        jmp gdi_cortar.c
gdi_cortar:
        pushad
.c:
        call gdi_recortar
        jnc .hay
        xor ecx, ecx                ; vacío
        xor edx, edx
.hay:
        mov [rec_x0], eax
        mov [rec_y0], ebx
        add ecx, eax
        add edx, ebx
        mov [rec_x1], ecx
        mov [rec_y1], edx
        popad
        ret

; (interna) recorta EAX=x EBX=y ECX=ancho EDX=alto. CF=1 si no queda nada
gdi_recortar:
        add ecx, eax
        add edx, ebx
        cmp eax, [rec_x0]
        jge .a
        mov eax, [rec_x0]
.a:
        cmp ebx, [rec_y0]
        jge .b
        mov ebx, [rec_y0]
.b:
        cmp ecx, [rec_x1]
        jle .c
        mov ecx, [rec_x1]
.c:
        cmp edx, [rec_y1]
        jle .d
        mov edx, [rec_y1]
.d:
        sub ecx, eax
        jle .v
        sub edx, ebx
        jle .v
        clc
        ret
.v:
        stc
        ret

gdi_punto:
        cmp eax, [rec_x0]
        jl .x
        cmp eax, [rec_x1]
        jge .x
        cmp ebx, [rec_y0]
        jl .x
        cmp ebx, [rec_y1]
        jge .x
        push edi
        push edx
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
        mov dl, [gdi_color]
        mov [edi+eax], dl
        pop edx
        pop edi
.x:
        ret

; Línea (algoritmo del punto medio con error entero)
gdi_linea:
        pushad
        mov esi, ecx
        sub esi, eax
        mov dword [ln_sx], 1
        jns .1
        neg esi
        mov dword [ln_sx], -1
.1:
        mov edi, edx
        sub edi, ebx
        mov dword [ln_sy], 1
        jns .2
        neg edi
        mov dword [ln_sy], -1
.2:
        neg edi
        mov [ln_dx], esi
        mov [ln_dy], edi
        add esi, edi                ; error
.l:
        call gdi_punto
        cmp eax, ecx
        jne .n
        cmp ebx, edx
        je .x
.n:
        lea edi, [esi+esi]
        cmp edi, [ln_dy]
        jl .ny
        add esi, [ln_dy]
        add eax, [ln_sx]
.ny:
        cmp edi, [ln_dx]
        jg .l
        add esi, [ln_dx]
        add ebx, [ln_sy]
        jmp .l
.x:
        popad
        ret

gdi_rect:
        pushad
        call gdi_recortar
        jc .x
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, eax
        add edi, [sup_ptr]
        mov esi, [sup_ancho]
        sub esi, ecx
        mov al, [gdi_color]
        mov ah, al
        mov ebx, eax
        shl eax, 16
        mov ax, bx
        mov ebx, ecx
.f:
        mov ecx, ebx
        shr ecx, 2
        rep stosd
        mov ecx, ebx
        and ecx, 3
        rep stosb
        add edi, esi
        dec edx
        jnz .f
.x:
        popad
        ret

gdi_marco:
        pushad
        push edx
        mov edx, 1
        call gdi_rect               ; arriba
        pop edx
        push ebx
        lea ebx, [ebx+edx-1]
        push edx
        mov edx, 1
        call gdi_rect               ; abajo
        pop edx
        pop ebx
        push ecx
        mov ecx, 1
        call gdi_rect               ; izquierda
        pop ecx
        lea eax, [eax+ecx-1]
        mov ecx, 1
        call gdi_rect               ; derecha
        popad
        ret

; Rellena con un mosaico de 16x16 (alineado con el origen de la superficie)
gdi_mosaico:
        pushad
        call gdi_recortar
        jc .x
        mov [ms_ancho], ecx
.fila:
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
        mov ebp, ebx
        and ebp, 15
        shl ebp, 4
        add ebp, esi
        mov ecx, [ms_ancho]
        push eax
        push edx
.p:
        mov edx, eax
        and edx, 15
        mov dl, [ebp+edx]
        mov [edi+eax], dl
        inc eax
        dec ecx
        jnz .p
        pop edx
        pop eax
        inc ebx
        dec edx
        jnz .fila
.x:
        popad
        ret

; Copia de bloques entre un mapa en memoria y la superficie
gdi_copiar:
        mov byte [cp_modo], 0
        jmp gdi_bloque
gdi_copiar_clave:
        mov byte [cp_modo], 1
        jmp gdi_bloque
gdi_leer:
        mov byte [cp_modo], 2
        xchg esi, edi
        call gdi_bloque
        xchg esi, edi
        ret
gdi_bloque:
        pushad
        mov ebp, ecx                ; ancho del mapa
        mov [cp_x], eax
        mov [cp_y], ebx
        call gdi_recortar
        jc .x
        mov edi, ebx                ; lo recortado tampoco se lee del mapa
        sub edi, [cp_y]
        imul edi, ebp
        add esi, edi
        mov edi, eax
        sub edi, [cp_x]
        add esi, edi
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, eax
        add edi, [sup_ptr]
        mov eax, [sup_ancho]
        sub eax, ecx
        mov [cp_salto], eax
        sub ebp, ecx
        mov ebx, ecx
.fila:
        mov ecx, ebx
        cmp byte [cp_modo], 1
        je .clave
        ja .leer
        rep movsb
        jmp .sig
.leer:
        xchg esi, edi
        rep movsb
        xchg esi, edi
        jmp .sig
.clave:
        lodsb
        cmp al, CLAVE
        je .c2
        mov [edi], al
.c2:
        inc edi
        dec ecx
        jnz .clave
.sig:
        add esi, ebp
        add edi, [cp_salto]
        dec edx
        jnz .fila
.x:
        popad
        ret

; Texto con la fuente propia de 8x16 (fuente8x16, página 437), sin fondo.
; La negrita repite cada punto una columna a la derecha.
gdi_texto:
        pushad
.c:
        movzx edx, byte [esi]
        inc esi
        test edx, edx
        jz .x
        call gdi_caracter
        add eax, 8
        jmp .c
.x:
        popad
        ret

gdi_ancho_texto:
        push esi
        xor eax, eax
.c:
        cmp byte [esi], 0
        je .x
        inc esi
        add eax, 8
        jmp .c
.x:
        pop esi
        ret

; (interna) EDX=carácter EAX=x EBX=y
gdi_caracter:
        pushad
        cmp eax, [rec_x1]
        jge .x
        cmp ebx, [rec_y1]
        jge .x
        lea ecx, [eax+9]
        cmp ecx, [rec_x0]
        jle .x
        lea ecx, [ebx+16]
        cmp ecx, [rec_y0]
        jle .x
        shl edx, 4
        lea esi, [fuente8x16+edx]
        mov cl, [gdi_color]
        mov ebp, 16
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
.fila:
        movzx edx, byte [esi]
        inc esi
        mov ch, dl
        shl edx, 1                  ; 9 columnas: bit 8 = la primera
        cmp byte [gdi_negrita], 0
        je .fina
        or dl, ch
.fina:
        test edx, edx
        jz .sig
        cmp ebx, [rec_y0]
        jl .sig
        cmp ebx, [rec_y1]
        jge .sig
        push eax
.bit:
        test edx, 0x100
        jz .n
        cmp eax, [rec_x0]
        jl .n
        cmp eax, [rec_x1]
        jge .n
        mov [edi+eax], cl
.n:
        inc eax
        shl edx, 1
        test edx, 0x1FF
        jnz .bit
        pop eax
.sig:
        add edi, [sup_ancho]
        inc ebx
        dec ebp
        jnz .fila
.x:
        popad
        ret
