; =============================================================================
;  texto.inc - Lector: el texto de una página, en orden de lectura
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  En un PDF el texto no viene por líneas: son trozos sueltos, cada uno en su
;  sitio y en el orden que quiso el programa que lo hizo. Para buscar, guardar
;  e imprimir hace falta el texto seguido. Aquí se sacan los trozos de la
;  lista de la página (contenido.inc), se ordenan de arriba abajo, se juntan
;  en líneas los que comparten línea base (dentro de cada una, de izquierda a
;  derecha) y se pone un espacio donde hay un hueco entre trozos (tres, si el
;  hueco es grande: columnas de una tabla) y una línea en blanco donde el
;  salto es de párrafo.
;
;    tx_extrae   deja el texto de la página en tx_buf (página 437, líneas
;                acabadas en 13, 10) -> EAX=octetos. De cada letra, en tx_mapa
;                (dw), de qué elemento de la lista sale (FFFFh: es añadida)
;    tx_busca    ESI=lo que se busca (acabado en 0) EAX=desde dónde -> EAX=
;                dónde está en tx_buf (-1: no está), ECX=su largo. No distingue
;                mayúsculas ni acentos (la «ñ» sí es otra letra que la «n»)
;    pdf_dato    EDI=clave de la información del documento ("Title"...)
;                EBX=búfer ECX=tamaño -> EAX=largo (0: no está); en la 437
; =============================================================================
TX_MAX      equ 96 * 1024

%macro TEXTO_BSS 0
            alignb 4
tx_n        resd 1                  ; octetos en tx_buf
tx_nord     resd 1
tx_tol      resd 1
tx_fin_ant  resd 1
tx_tam_ant  resd 1
tx_y_ant    resd 1
tx_ord      resw MAX_IT + 4
tx_mapa     resw TX_MAX + 8
tx_buf      resb TX_MAX + 8
%endmacro

k_title     db "Title", 0
k_author    db "Author", 0
k_subject   db "Subject", 0
k_creator   db "Creator", 0
k_producer  db "Producer", 0

; (interna) AL=letra, EDX=de qué elemento (FFFFh: ninguno): a tx_buf, si cabe
tx_pon:
        push ecx
        mov ecx, [tx_n]
        cmp ecx, TX_MAX
        jae .x
        mov [tx_buf+ecx], al
        mov [tx_mapa+ecx*2], dx
        inc dword [tx_n]
.x:
        pop ecx
        ret

tx_extrae:
        pushad
        mov dword [tx_n], 0
        ; ---- los trozos de texto, por su número ----
        xor ecx, ecx
        xor ebx, ebx
        mov esi, it_tab
.junta:
        cmp ebx, [it_n]
        jae .ordena
        cmp byte [esi+IT_TIPO], ITT_TEXTO
        jne .j1
        mov [tx_ord+ecx*2], bx
        inc ecx
.j1:
        add esi, IT_TAM
        inc ebx
        jmp .junta
.ordena:
        mov [tx_nord], ecx
        ; ---- de arriba abajo (por inserción: casi siempre ya vienen así) ----
        mov ebx, 1
.o:
        cmp ebx, [tx_nord]
        jae .lineas
        movzx edx, word [tx_ord+ebx*2]
        mov eax, edx
        shl eax, 5
        mov eax, [it_tab+eax+IT_Y]  ; EAX=la y del que se coloca
        mov ecx, ebx
.o1:
        test ecx, ecx
        jz .o2
        movzx esi, word [tx_ord+ecx*2-2]
        mov edi, esi
        shl edi, 5
        cmp [it_tab+edi+IT_Y], eax
        jle .o2
        mov [tx_ord+ecx*2], si
        dec ecx
        jmp .o1
.o2:
        mov [tx_ord+ecx*2], dx
        inc ebx
        jmp .o
        ; ---- línea a línea ----
.lineas:
        xor ebx, ebx                ; EBX=el primero de la línea
        mov dword [tx_y_ant], 0x80000000
.l:
        cmp ebx, [tx_nord]
        jae .fin
        movzx eax, word [tx_ord+ebx*2]
        shl eax, 5
        lea esi, [it_tab+eax]
        mov eax, [esi+IT_AL]        ; la holgura: un tercio del tamaño (al menos
        mov ecx, 3                  ; un punto)
        cdq
        idiv ecx
        cmp eax, 0x10000
        jge .l0
        mov eax, 0x10000
.l0:
        mov [tx_tol], eax
        mov ebp, [esi+IT_Y]         ; EBP=la línea base
        ; un salto grande desde la línea anterior: párrafo nuevo
        cmp dword [tx_y_ant], 0x80000000
        je .l1
        mov eax, [esi+IT_AL]        ; más de 1,8 veces el tamaño
        lea eax, [eax+eax*8]
        cdq
        mov ecx, 5
        idiv ecx
        mov ecx, eax
        mov eax, ebp
        sub eax, [tx_y_ant]
        cmp eax, ecx
        jle .l1
        mov edx, 0xFFFF
        mov al, 13
        call tx_pon
        mov al, 10
        call tx_pon
.l1:
        mov [tx_y_ant], ebp
        mov ecx, ebx                ; ECX=uno más que el último de la línea
.l2:
        inc ecx
        cmp ecx, [tx_nord]
        jae .l3
        movzx eax, word [tx_ord+ecx*2]
        shl eax, 5
        mov eax, [it_tab+eax+IT_Y]
        sub eax, ebp
        cmp eax, [tx_tol]
        jle .l2
.l3:
        ; la línea, de izquierda a derecha (por inserción)
        lea edi, [ebx+1]
.x1:
        cmp edi, ecx
        jae .escribe
        movzx edx, word [tx_ord+edi*2]
        mov eax, edx
        shl eax, 5
        mov eax, [it_tab+eax+IT_X]
        mov esi, edi
.x2:
        cmp esi, ebx
        jbe .x3
        push ebp
        movzx ebp, word [tx_ord+esi*2-2]
        push ebp
        shl ebp, 5
        cmp [it_tab+ebp+IT_X], eax
        pop ebp
        jle .x3b
        mov [tx_ord+esi*2], bp
        pop ebp
        dec esi
        jmp .x2
.x3b:
        pop ebp
.x3:
        mov [tx_ord+esi*2], dx
        inc edi
        jmp .x1
.escribe:
        mov dword [tx_fin_ant], 0x80000000
.e:
        cmp ebx, ecx
        jae .e_fin
        movzx edx, word [tx_ord+ebx*2]
        mov eax, edx
        shl eax, 5
        lea esi, [it_tab+eax]
        ; el hueco desde el trozo anterior
        cmp dword [tx_fin_ant], 0x80000000
        je .e2
        mov eax, [esi+IT_X]
        sub eax, [tx_fin_ant]
        mov edi, [tx_tam_ant]
        sar edi, 3                  ; más de un octavo del tamaño: un espacio
        cmp eax, edi
        jle .e2
        push eax
        push edx
        mov edi, [tx_n]             ; (si no lo hay ya)
        test edi, edi
        jz .e1
        cmp byte [tx_buf+edi-1], ' '
        je .e1
        mov edi, [esi+IT_TXT]
        cmp byte [edi], ' '
        je .e1
        mov al, ' '
        mov edx, 0xFFFF
        call tx_pon
.e1:
        pop edx
        pop eax
        mov edi, [tx_tam_ant]       ; más de tres veces: otra columna
        lea edi, [edi+edi*2]
        cmp eax, edi
        jle .e2
        push edx
        mov al, ' '
        mov edx, 0xFFFF
        call tx_pon
        call tx_pon
        pop edx
.e2:
        mov eax, [esi+IT_X]
        add eax, [esi+IT_AN]
        mov [tx_fin_ant], eax
        mov eax, [esi+IT_AL]
        mov [tx_tam_ant], eax
        push ecx
        movzx ecx, word [esi+IT_N]
        mov esi, [esi+IT_TXT]
.e3:
        test ecx, ecx
        jz .e4
        lodsb
        call tx_pon
        dec ecx
        jmp .e3
.e4:
        pop ecx
        inc ebx
        jmp .e
.e_fin:
        mov edx, 0xFFFF
        mov al, 13
        call tx_pon
        mov al, 10
        call tx_pon
        jmp .l
.fin:
        mov eax, [tx_n]
        mov byte [tx_buf+eax], 0
        mov [esp+28], eax
        popad
        ret

; (interna) AL=letra -> AL sin acento y en mayúscula (la «ñ», en «Ñ»)
tx_may:
        cmp al, 0xA4
        je .enie
        cmp al, 0xA5
        je .x
        cmp al, 128
        jb .a
        push ebx
        movzx ebx, al
        mov al, [sin_acento+ebx-128]
        pop ebx
.a:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret
.enie:
        mov al, 0xA5
        ret

tx_busca:
        push ebx
        push edx
        push esi
        push edi
        xor ecx, ecx                ; el largo de lo que se busca
.l:
        cmp byte [esi+ecx], 0
        je .l1
        inc ecx
        jmp .l
.l1:
        test ecx, ecx
        jz .no
        mov edi, eax
.b:
        mov eax, [tx_n]
        sub eax, edi
        jb .no
        cmp eax, ecx
        jb .no
        xor ebx, ebx
.c:
        mov al, [tx_buf+edi+ebx]
        call tx_may
        mov dl, al
        mov al, [esi+ebx]
        call tx_may
        cmp al, dl
        jne .sig
        inc ebx
        cmp ebx, ecx
        jb .c
        mov eax, edi
        jmp .x
.sig:
        inc edi
        jmp .b
.no:
        mov eax, -1
.x:
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

pdf_dato:
        pushad
        mov dword [esp+28], 0
        mov esi, [pdf_info_p]
        mov ebp, [pdf_info_p+4]
        test esi, esi
        jz .x
        call px_dic
        jc .x
        call px_ref
        cmp esi, ebp
        jae .x
        cmp byte [esi], '('
        je .cad
        cmp byte [esi], '<'
        jne .x
.cad:
        mov edi, ci_cad
        mov ecx, CAD_TAM
        call px_cadena
        mov ecx, eax                ; ECX=octetos
        mov esi, ci_cad
        mov edi, [esp+16]           ; (EBX: el búfer)
        mov ebp, [esp+24]           ; (ECX: lo que cabe)
        lea ebp, [edi+ebp-4]        ; EBP=hasta dónde (sitio para «...» y el 0)
        xor ebx, ebx                ; BL=1: UTF-16
        cmp ecx, 2
        jb .c
        cmp word [esi], 0xFFFE      ; (FE FF: va en UTF-16, el de más peso primero)
        jne .c
        add esi, 2
        sub ecx, 2
        inc ebx
.c:
        test ecx, ecx
        jle .fin
        cmp edi, ebp
        jae .fin
        movzx eax, byte [esi]
        inc esi
        dec ecx
        test ebx, ebx
        jz .c1
        test ecx, ecx
        jz .fin
        shl eax, 8
        mov al, [esi]
        inc esi
        dec ecx
.c1:
        call uni_437
        test al, al
        jz .c
        cmp al, 16
        jae .c2
        movzx eax, al               ; varias letras
        lea edx, [exp_tab+eax*4]
.cv:
        mov al, [edx]
        test al, al
        jz .c
        cmp edi, ebp
        jae .c
        stosb
        inc edx
        jmp .cv
.c2:
        stosb
        jmp .c
.fin:
        mov byte [edi], 0
        sub edi, [esp+16]
        mov [esp+28], edi
.x:
        popad
        ret
