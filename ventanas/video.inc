; =============================================================================
;  Hispa Ventanas - driver de pantalla (modo protegido)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Interfaz del driver (lo único que usan las demás capas):
;    vid_detectar   (modo real, video16.inc) elige el modo
;    vid_abrir      pone el modo gráfico y la paleta. CF=1 si no se puede
;    vid_cerrar     vuelve al modo de texto que había
;    vid_presentar  lleva a la pantalla un rectángulo de la superficie
;                   EAX=x0 EBX=y0 ECX=x1 EDX=y1 (x1 e y1, fuera)
;    vid_ancho, vid_alto, vid_bits   lo que se ha puesto
;
;  Las capas de arriba dibujan siempre en la superficie (un byte por punto,
;  índices de la paleta de Hispa Ventanas); aquí se convierte al formato de la
;  pantalla: 8 bits (se copia), 15/16, 24 y 32 bits (por la tabla pal_nat) o
;  los cuatro planos de la VGA de 16 colores.
; =============================================================================

vid_abrir:
        cmp byte [vid_tipo], 1
        jne .vga
        mov eax, [vid_paso]         ; marco lineal: hacerlo accesible (0800h)
        imul eax, [vid_alto]
        mov edi, eax
        mov esi, eax
        shr esi, 16
        mov ecx, [vid_fis]
        mov ebx, ecx
        shr ebx, 16
        mov ax, 0x0800
        int 0x31
        jc .reserva
        shl ebx, 16
        mov bx, cx
        sub ebx, [base]
        mov [vid_ptr], ebx
        mov eax, 0x4F02
        movzx ebx, word [vid_modo]
        or bh, 0x40                 ; con marco lineal
        int 0x10
        cmp ax, 0x004F
        jne .reserva
        movzx eax, byte [vid_octetos]
        mov eax, [pres_tabla+eax*4-4]
        mov [vid_presenta], eax
        jmp .paleta
.reserva:
        mov byte [vid_tipo], 0      ; VESA ha fallado: VGA de 16 colores
        mov dword [vid_ancho], 640
        mov dword [vid_alto], 480
        mov dword [vid_paso], 80
        mov byte [vid_bits], 4
        mov byte [vid_octetos], 1
.vga:
        mov eax, 0x0012
        int 0x10
        mov eax, 0x0F00
        int 0x10
        cmp al, 0x12
        jne .mal
        mov eax, 0xA0000
        sub eax, [base]
        mov [vid_ptr], eax
        mov dword [vid_presenta], pres_vga
        xor ebx, ebx                ; los 16 colores, a los registros 0-15 del DAC
.atr:
        mov eax, 0x1000
        mov bh, bl
        int 0x10
        inc bl
        cmp bl, 16
        jb .atr
.paleta:
        call vid_paleta
        clc
        ret
.mal:
        stc
        ret

vid_cerrar:
        movzx eax, byte [modo_ant]
        cmp al, 7
        je .m
        cmp al, 3
        jbe .m
        mov al, 3
.m:
        int 0x10
        mov byte [en_grafico], 0
        ret

; La paleta: los 16 colores de Hispa Ventanas (paleta16), un cubo de 6x6x6
; (16-231) y una escala de grises (232-255) para las etapas siguientes.
vid_paleta:
        pushad
        mov esi, paleta16
        mov edi, pal_rgb
        mov ecx, 16
.p16:
        movsw
        movsb
        xor al, al
        stosb
        loop .p16
        xor ebx, ebx                ; cubo: índice 16 + 36r + 6g + b
.cubo:
        mov eax, ebx
        xor edx, edx
        mov ecx, 36
        div ecx
        imul eax, 51
        stosb
        mov eax, edx
        xor edx, edx
        mov ecx, 6
        div ecx
        imul eax, 51
        stosb
        imul eax, edx, 51
        stosb
        xor al, al
        stosb
        inc ebx
        cmp ebx, 216
        jb .cubo
        xor ebx, ebx
.gris:
        imul eax, ebx, 11
        stosb
        stosb
        stosb
        xor al, al
        stosb
        inc ebx
        cmp ebx, 24
        jb .gris

        cmp byte [vid_octetos], 1
        jne .directo
        mov ecx, 256                ; el DAC de la VGA (6 bits por componente)
        cmp byte [vid_tipo], 0
        jne .dac
        mov ecx, 16
.dac:
        mov dx, 0x3C8
        xor al, al
        out dx, al
        inc dx
        mov esi, pal_rgb
.d1:
        lodsb
        shr al, 2
        out dx, al
        lodsb
        shr al, 2
        out dx, al
        lodsb
        shr al, 2
        out dx, al
        inc esi
        loop .d1
        popad
        ret
.directo:
        mov esi, pal_rgb            ; cada color, en el formato de la pantalla
        mov edi, pal_nat
        mov ebp, 256
.n1:
        xor ebx, ebx
        xor edx, edx
.n2:
        movzx eax, byte [esi+edx]
        mov cl, 8
        sub cl, [vid_masc+edx*2]
        shr eax, cl
        mov cl, [vid_masc+edx*2+1]
        shl eax, cl
        or ebx, eax
        inc edx
        cmp edx, 3
        jb .n2
        mov [edi], ebx
        add esi, 4
        add edi, 4
        dec ebp
        jnz .n1
        popad
        ret

; EAX=x0 EBX=y0 ECX=x1 EDX=y1: de la superficie a la pantalla
vid_presentar:
        pushad
        test eax, eax
        jns .a
        xor eax, eax
.a:
        test ebx, ebx
        jns .b
        xor ebx, ebx
.b:
        cmp ecx, [vid_ancho]
        jle .c
        mov ecx, [vid_ancho]
.c:
        cmp edx, [vid_alto]
        jle .d
        mov edx, [vid_alto]
.d:
        sub ecx, eax
        jle .x
        sub edx, ebx
        jle .x
        call [vid_presenta]
.x:
        popad
        ret

; Los presentadores reciben EAX=x EBX=y ECX=ancho EDX=alto, ya dentro de la
; pantalla.
            align 4
pres_tabla  dd pres_8, pres_16, pres_24, pres_32

; prepara ESI (superficie), EDI (pantalla) y los saltos de fin de fila
pres_prep:
        mov esi, ebx
        imul esi, [sup_ancho]
        add esi, eax
        add esi, [sup_ptr]
        mov edi, ebx
        imul edi, [vid_paso]
        movzx ebx, byte [vid_octetos]
        imul eax, ebx
        add edi, eax
        add edi, [vid_ptr]
        mov eax, [sup_ancho]
        sub eax, ecx
        mov [pr_salto_s], eax
        imul ebx, ecx
        mov eax, [vid_paso]
        sub eax, ebx
        mov [pr_salto_d], eax
        mov ebx, ecx
        ret

pres_8:
        call pres_prep
.f:
        mov ecx, ebx
        shr ecx, 2
        rep movsd
        mov ecx, ebx
        and ecx, 3
        rep movsb
        add esi, [pr_salto_s]
        add edi, [pr_salto_d]
        dec edx
        jnz .f
        ret

pres_16:
        call pres_prep
.f:
        mov ecx, ebx
.p:
        movzx eax, byte [esi]
        inc esi
        mov eax, [pal_nat+eax*4]
        mov [edi], ax
        add edi, 2
        dec ecx
        jnz .p
        add esi, [pr_salto_s]
        add edi, [pr_salto_d]
        dec edx
        jnz .f
        ret

pres_24:
        call pres_prep
.f:
        mov ecx, ebx
.p:
        movzx eax, byte [esi]
        inc esi
        mov eax, [pal_nat+eax*4]
        mov [edi], ax
        shr eax, 16
        mov [edi+2], al
        add edi, 3
        dec ecx
        jnz .p
        add esi, [pr_salto_s]
        add edi, [pr_salto_d]
        dec edx
        jnz .f
        ret

pres_32:
        call pres_prep
.f:
        mov ecx, ebx
.p:
        movzx eax, byte [esi]
        inc esi
        mov eax, [pal_nat+eax*4]
        stosd
        dec ecx
        jnz .p
        add esi, [pr_salto_s]
        add edi, [pr_salto_d]
        dec edx
        jnz .f
        ret

; VGA de 16 colores: cuatro planos de un bit. Se escribe por octetos enteros
; (8 puntos), plano a plano, con el modo de escritura 0.
pres_vga:
        add ecx, eax
        add ecx, 7
        and ecx, ~7
        and eax, ~7
        sub ecx, eax
        shr ecx, 3
        mov [pr_n], ecx
        mov [pr_alto], edx
        mov esi, ebx
        imul esi, [sup_ancho]
        add esi, eax
        add esi, [sup_ptr]
        mov edi, ebx
        imul edi, 80
        shr eax, 3
        add edi, eax
        add edi, [vid_ptr]
.fila:
        xor ecx, ecx                ; CL = plano
.plano:
        mov ax, 0x0102
        shl ah, cl
        mov dx, 0x3C4               ; máscara de planos del secuenciador
        out dx, ax
        push esi
        push edi
        mov ebx, [pr_n]
.octeto:
        mov ch, 8
.bit:
        mov al, [esi]
        inc esi
        shr al, cl
        shr al, 1
        rcl ah, 1
        dec ch
        jnz .bit
        mov [edi], ah
        inc edi
        dec ebx
        jnz .octeto
        pop edi
        pop esi
        inc cl
        cmp cl, 4
        jb .plano
        add esi, [sup_ancho]
        add edi, 80
        dec dword [pr_alto]
        jnz .fila
        mov dx, 0x3C4
        mov ax, 0x0F02
        out dx, ax
        ret
