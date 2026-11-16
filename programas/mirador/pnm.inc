; =============================================================================
;  MIRADOR.HVP - los formatos PBM, PGM y PPM (los «P1» a «P6» de Netpbm)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Blanco y negro (P1 en texto, P4 en bits), grises (P2, P5) y color (P3,
;  P6), con el valor máximo que digan hasta 65.535 (se lleva a 0-255,
;  redondeando) y con comentarios («#») en la cabecera. Se leen fila a fila,
;  según se piden: no necesitan memoria de trabajo.
; =============================================================================
%macro PNM_BSS 0
            alignb 4
pnm_p       resd 1                  ; por dónde se va leyendo
pnm_fin     resd 1
pnm_tipo    resd 1                  ; de 1 a 6
pnm_max     resd 1
pnm_n       resd 1                  ; muestras por fila
%endmacro

pnm_detecta:
        cmp ecx, 8
        jb .no
        cmp byte [esi], 'P'
        jne .no
        mov al, [esi+1]
        sub al, '1'
        cmp al, 5
        ja .no
        mov al, [esi+2]
        cmp al, ' '
        je .si
        cmp al, '#'
        je .si
        cmp al, 9
        jb .no
        cmp al, 13
        ja .no
.si:
        clc
        ret
.no:
        stc
        ret

; (interna) salta espacios y comentarios desde ESI; CF=1 si se acaba el archivo
pnm_blancos:
        cmp esi, [pnm_fin]
        jae .fin
        mov al, [esi]
        cmp al, '#'
        je .coment
        cmp al, ' '
        je .s
        cmp al, 9
        jb .x
        cmp al, 13
        ja .x
.s:
        inc esi
        jmp pnm_blancos
.coment:
        inc esi
        cmp esi, [pnm_fin]
        jae .fin
        cmp byte [esi], 10
        je .s
        cmp byte [esi], 13
        je .s
        jmp .coment
.x:
        clc
        ret
.fin:
        stc
        ret

; (interna) el número que viene en ESI (tras espacios y comentarios) -> EAX;
; CF=1 si no hay ninguno. Como mucho, 999.999
pnm_num:
        call pnm_blancos
        jc .no
        push edx
        xor eax, eax
        xor edx, edx                ; EDX=cifras
.c:
        cmp esi, [pnm_fin]
        jae .f
        push ebx
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .f1
        cmp eax, 99999
        ja .c1
        imul eax, 10
        add eax, ebx
.c1:
        pop ebx
        inc esi
        inc edx
        jmp .c
.f1:
        pop ebx
.f:
        test edx, edx
        pop edx
        jz .no
        clc
        ret
.no:
        stc
        ret

pnm_abre:
        lea eax, [esi+ecx]
        mov [pnm_fin], eax
        movzx eax, byte [esi+1]
        sub eax, '0'
        mov [pnm_tipo], eax
        add esi, 2
        call pnm_num
        jc .roto
        mov [im_an], eax
        call pnm_num
        jc .roto
        mov [im_al], eax
        call mi_medidas_ok
        jc .grande
        mov dword [pnm_max], 1
        mov eax, [pnm_tipo]
        cmp eax, 1
        je .sin_max
        cmp eax, 4
        je .sin_max
        call pnm_num
        jc .roto
        test eax, eax
        jz .roto
        cmp eax, 65535
        ja .roto
        mov [pnm_max], eax
.sin_max:
        cmp dword [pnm_tipo], 4     ; detrás de la cabecera de los binarios, un solo blanco
        jb .c1
        cmp esi, [pnm_fin]
        jae .c1
        inc esi
.c1:
        mov [pnm_p], esi
        mov eax, [im_an]
        mov dword [im_clase], IC_GRIS
        mov ebx, 1                  ; EBX=bits por punto, para la información
        mov ecx, [pnm_tipo]
        cmp ecx, 1
        je .c3
        cmp ecx, 4
        je .c3
        mov ebx, 8
        cmp dword [pnm_max], 255
        jbe .c2
        mov ebx, 16
.c2:
        cmp ecx, 3
        je .color
        cmp ecx, 6
        jne .c3
.color:
        mov dword [im_clase], IC_RGB
        lea eax, [eax+eax*2]
        lea ebx, [ebx+ebx*2]
.c3:
        mov [pnm_n], eax
        mov [im_bits], ebx
        xor eax, eax
        ret
.roto:
        mov eax, ME_ROTO
        ret
.grande:
        mov eax, ME_GRANDE
        ret

; (interna) EAX=una muestra, de 0 a [pnm_max] -> AL, de 0 a 255
pnm_escala:
        cmp eax, [pnm_max]          ; (uno mayor que el máximo: el máximo)
        jbe .a0
        mov eax, [pnm_max]
.a0:
        cmp dword [pnm_max], 255
        je .x
        push edx
        push ebx
        mov ebx, [pnm_max]
        imul eax, 255
        mov edx, ebx
        shr edx, 1
        add eax, edx
        xor edx, edx
        div ebx
        pop ebx
        pop edx
.x:
        ret

pnm_fila:
        mov esi, [pnm_p]
        mov ecx, [pnm_n]
        mov eax, [pnm_tipo]
        cmp eax, 4
        je .bits
        cmp eax, 1
        je .texto1
        cmp eax, 3
        jbe .texto
        ; ---- P5 y P6: muestras de uno o dos octetos ----
        mov edx, [pnm_fin]
        sub edx, esi
        cmp dword [pnm_max], 255
        ja .dos
        cmp edx, ecx
        jb .falta
.u1:
        movzx eax, byte [esi]
        inc esi
        call pnm_escala
        stosb
        dec ecx
        jnz .u1
        jmp .bien
.dos:
        shr edx, 1
        cmp edx, ecx
        jb .falta
.d1:
        movzx eax, word [esi]
        xchg al, ah
        add esi, 2
        call pnm_escala
        stosb
        dec ecx
        jnz .d1
        jmp .bien
.texto:
        call pnm_num
        jc .falta
        call pnm_escala
        stosb
        dec ecx
        jnz .texto
        jmp .bien
.texto1:
        call pnm_blancos            ; P1: unos y ceros, juntos o separados
        jc .falta
        mov al, [esi]
        inc esi
        sub al, '0'
        cmp al, 1
        ja .falta
        dec al                      ; el 1 es negro
        stosb
        dec ecx
        jnz .texto1
        jmp .bien
.bits:
        mov edx, ecx                ; P4: un bit por punto; el 1 es negro
        add edx, 7
        shr edx, 3
        mov eax, [pnm_fin]
        sub eax, esi
        cmp eax, edx
        jb .falta
        push edx
        push esi
        push edi
        mov edx, ecx
        mov cl, 1
        call mi_desempaqueta
        pop edi
        pop esi
        pop edx
        add esi, edx
        mov ecx, [pnm_n]
.b1:
        mov al, [edi]
        dec al
        stosb
        dec ecx
        jnz .b1
.bien:
        mov [pnm_p], esi
        clc
        ret
.falta:
        mov [pnm_p], esi
        stc
        ret
