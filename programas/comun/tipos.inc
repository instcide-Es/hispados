; =============================================================================
;  tipos.inc - los tipos de letra que comparten los programas de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los tipos están en TIPOS.DAT, al lado de los módulos (\HISPADOS\PROGS); lo
;  hace tools/hazfuentes.py, que explica el archivo; de dónde sale cada tipo y
;  su licencia, en docs/TIPOS-DE-LETRA.md. El tipo 0 es «Hispa», la letra de
;  ancho fijo de Hispa Ventanas (8x16), que no está en el archivo: si TIPOS.DAT
;  falta, es el único que hay y todo sigue funcionando.
;
;  Se incluye en el código del programa (detrás de hvplib.inc o delante, da
;  igual) y, en su «section .bss», se pone la macro TIPOS_BSS.
;
;    tl_cargar   ESI=ruta del módulo (la que recibe en su entrada): lee los
;                tipos. Deja en [tl_n] cuántos hay, contando «Hispa»
;    tl_nombre   EAX=tipo -> ESI=su nombre
;    tl_letra    EAX=tipo ECX=carácter -> ESI=su dibujo; ZF=1 (y ESI=0) si ese
;                tipo no lo trae: se pone el de Hispa. El dibujo: db ancho,
;                db octetos por fila (1 o 2), y 16 filas (el punto de la
;                izquierda, el bit más alto; con 2 octetos, primero el bajo)
;    tl_ancho    EAX=tipo ECX=carácter -> EAX=su ancho en puntos (8 en Hispa)
; =============================================================================
TL_MAX      equ 128 * 1024          ; lo más que puede ocupar TIPOS.DAT
TL_TIPOS    equ 32                  ; tipos, a lo más (con «Hispa»)

%macro TIPOS_BSS 0
tl_n        resd 1
tl_ruta     resb 128
tl_buf      resb TL_MAX
%endmacro

tl_archivo  db "TIPOS.DAT", 0
tl_hispa    db "Hispa", 0

tl_cargar:
        pushad
        mov dword [tl_n], 1
        mov edi, tl_ruta            ; la carpeta del módulo...
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
        mov edi, ebx                ; ...y el nombre del archivo
        mov esi, tl_archivo
.n:
        lodsb
        stosb
        test al, al
        jnz .n
        mov esi, tl_ruta            ; se lee a trozos (de una vez, DOS no pasa
        xor al, al                  ; de 64 K)
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, tl_buf
.lee:
        mov ecx, tl_buf + TL_MAX
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
        mov eax, edi
        sub eax, tl_buf             ; EAX=lo leído
        cmp eax, 12
        jb .x
        cmp dword [tl_buf], 'TIPO'
        jne .x
        cmp dword [tl_buf+4], 'S1' | (26 << 16)
        jne .x
        mov ecx, [tl_buf+8]
        test ecx, ecx
        jz .x
        cmp ecx, TL_TIPOS - 1
        ja .x
        mov edx, ecx                ; que cada tipo esté entero en lo leído
        mov esi, tl_buf + 12
.v:
        mov ebx, [esi+24]
        cmp ebx, eax
        jae .x
        mov edi, [esi+28]
        cmp edi, 448
        jb .x
        add ebx, edi
        jc .x
        cmp ebx, eax
        ja .x
        mov byte [esi+23], 0
        add esi, 32
        dec edx
        jnz .v
        inc ecx
        mov [tl_n], ecx
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
        add esi, tl_buf + 12 - 32
.x:
        ret

tl_letra:
        xor esi, esi
        test eax, eax
        jz .x
        cmp eax, [tl_n]
        jae .x
        cmp ecx, 32
        jb .x
        cmp ecx, 255
        ja .x
        push eax
        push ebx
        shl eax, 5
        mov ebx, [tl_buf+12-32+eax+24]      ; dónde empieza el tipo
        mov eax, [tl_buf+12-32+eax+28]      ; y lo que ocupa
        movzx esi, word [tl_buf+ebx+ecx*2-64]
        test esi, esi
        jz .p
        add eax, -34                ; (que el dibujo esté dentro de su tipo)
        cmp esi, eax
        ja .fuera
        lea esi, [tl_buf+ebx+esi]
        jmp short .p
.fuera:
        xor esi, esi
.p:
        pop ebx
        pop eax
.x:
        test esi, esi
        ret

tl_ancho:
        push esi
        call tl_letra
        mov eax, 8
        jz .x
        movzx eax, byte [esi]
.x:
        pop esi
        ret
