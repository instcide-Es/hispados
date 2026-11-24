; =============================================================================
;  PLUMA - preguntar, avisar, los menús y la ayuda
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Todo se pregunta en la línea de abajo, como en WordStar y en Word 1.0: así
;  el texto sigue a la vista. El menú (Esc o F10) es una línea con las
;  órdenes; se elige con las flechas e Intro o con la letra que se ve más
;  clara (la inicial, casi siempre).
; =============================================================================

; ---- un texto en la línea de abajo: pregunta SI, respuesta en DI (hasta CL
; letras; lo que ya hay se ve y se puede aprovechar: la primera letra que se
; escriba lo sustituye). CF=1 si se deja con Esc ----
pide:
        mov [p_preg], si
        mov [p_buf], di
        mov [p_max], cl
        mov byte [p_virgen], 1
.pinta:
        mov di, lin_abajo
        mov si, [p_preg]
        call copia_z
        mov si, [p_buf]
        call copia_z
        mov byte [di], 0
        mov si, lin_abajo
        call pinta_abajo
        mov dx, [abajo_col]
        sub dx, fila
        shr dx, 1
        mov dh, 24
        call pon_cursor
        call lee_tecla
        mov di, [p_buf]
        call largo_z                ; CX = lo que mide
        cmp al, 13
        je .si
        cmp al, 27
        je .no
        cmp al, 8
        je .borra
        test al, al
        jz .ext
        cmp al, 32
        jb .pinta
        cmp byte [p_virgen], 0
        je .pon
        xor cx, cx
.pon:
        mov byte [p_virgen], 0
        cmp cl, [p_max]
        jae .pinta
        mov bx, cx
        mov [di+bx], al
        mov byte [di+bx+1], 0
        jmp .pinta
.ext:
        cmp ah, 0x12                ; Alt+E: €
        jne .pinta
        mov al, C_EURO
        jmp .pon
.borra:
        mov byte [p_virgen], 0
        jcxz .pinta
        mov bx, cx
        mov byte [di+bx-1], 0
        jmp .pinta
.si:
        clc
        ret
.no:
        stc
        ret
; copia el texto de SI (acabado en 0) a DI (sin el 0)
copia_z:
        lodsb
        test al, al
        jz .x
        stosb
        jmp copia_z
.x:
        ret
; lo que mide el texto de DI: CX
largo_z:
        push di
        push ax
        xor cx, cx
.c:
        cmp byte [di], 0
        je .x
        inc di
        inc cx
        jmp .c
.x:
        pop ax
        pop di
        ret

; ---- un número de un octeto: pregunta SI, el número en [BX], entre DL y DH ----
pide_num:
        mov [n_ptr], bx
        mov [n_lim], dx
        push si
        mov al, [bx]
        mov ah, 0
        mov di, n_txt
        call num_a_texto
        mov byte [di], 0
        pop si
.otra:
        push si
        mov di, n_txt
        mov cl, 3
        call pide
        pop si
        jc .x
        mov di, n_txt               ; a número
        xor ax, ax
        mov cl, 10
.c:
        mov bl, [di]
        test bl, bl
        jz .fin
        sub bl, '0'
        cmp bl, 9
        ja .mal
        mul cl
        add al, bl
        adc ah, 0
        inc di
        jmp .c
.fin:
        test ah, ah
        jnz .mal
        mov dx, [n_lim]
        cmp al, dl
        jb .mal
        cmp al, dh
        ja .mal
        mov bx, [n_ptr]
        mov [bx], al
        mov byte [cambiado], 1
        mov byte [filas_ok], 0
        clc
.x:
        ret
.mal:
        call pita
        jmp .otra
; AX en decimal, en DI (que avanza)
num_a_texto:
        xor dx, dx
        mov byte [puntos], 0
        jmp short num_comun
num32_a_texto:                      ; (DX:AX, con puntos de miles)
        mov byte [puntos], 1
num_comun:
        push bx
        push cx
        xor cx, cx                  ; CX = cifras
.c:
        push ax                     ; DX:AX / 10, de 16 en 16 bits
        mov ax, dx
        xor dx, dx
        mov bx, 10
        div bx
        mov bx, ax
        pop ax
        push bx
        mov bx, 10
        div bx
        pop bx
        push dx                     ; la cifra
        mov dx, bx
        inc cx
        mov bx, ax
        or bx, dx
        jnz .c
.p:
        pop ax
        add al, '0'
        stosb
        dec cx
        jz .x
        cmp byte [puntos], 0
        je .p
        mov ax, cx                  ; un punto cada tres, contando desde el
        mov bl, 3                   ; final
        div bl
        test ah, ah
        jnz .p
        mov al, '.'
        stosb
        jmp .p
.x:
        pop cx
        pop bx
        ret

; ---- una pregunta de una tecla: AL, en mayúscula ----
pregunta:
        call pinta_abajo
        mov dh, 24
        mov dl, 79
        call pon_cursor
        call lee_tecla
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret
; ---- un aviso, hasta que se pulse una tecla ----
aviso:
        call pinta_abajo
        call pita
        call lee_tecla
        mov byte [filas_ok], 0
        ret

; =============================================================================
;  El menú: una línea con las órdenes. Cada entrada: el tipo (0 una orden, 1
;  otro menú), adónde va, y el texto (con & delante de su letra); 0FFh acaba
; =============================================================================
%macro ORDEN 2
        db 0
        dw %1
        db %2, 0
%endmacro
%macro SUBMENU 2
        db 1
        dw %1
        db %2, 0
%endmacro

menu_principal:
        mov si, mn_raiz
        call menu
        jc .x
        jmp bx
.x:
        ret
; el menú de SI: BX = la orden elegida; CF=1 si se deja con Esc
menu:
        mov [mn_def], si
        mov byte [mn_sel], 0
.pinta:
        ; la línea: «Pluma:» y las entradas, la elegida al revés
        mov di, fila
        mov cx, 80
        mov ax, 0x7020
        rep stosw
        mov di, fila + 2
        mov si, m_menu
.t:
        lodsb
        test al, al
        jz .t1
        mov ah, 0x70
        stosw
        jmp .t
.t1:
        mov si, [mn_def]
        xor cx, cx                  ; CL = la entrada
.e:
        cmp byte [si], 0xFF
        je .vuelca
        add si, 3
        mov ah, 0x70
        cmp cl, [mn_sel]
        jne .e1
        mov ah, 0x07
.e1:
        mov al, ' '
        stosw
.l:
        lodsb
        test al, al
        jz .e2
        cmp al, '&'
        jne .l1
        lodsb                       ; la letra de la tecla, más clara
        push ax
        or ah, 0x08
        stosw
        pop ax
        jmp .l
.l1:
        stosw
        jmp .l
.e2:
        mov al, ' '
        stosw
        inc cl
        cmp di, fila + 158
        jb .e
.vuelca:
        mov [mn_n], cl
        push es
        mov es, [vseg]
        mov di, 24 * 160
        mov si, fila
        mov cx, 80
        rep movsw
        pop es
        mov dx, 0x184F
        call pon_cursor
        call lee_tecla
        cmp al, 27
        je .no
        cmp al, 13
        je .elige
        test al, al
        jz .flecha
        cmp al, 'a'                 ; una letra: la entrada que la tiene
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 32
.m:
        mov si, [mn_def]
        xor cx, cx
.b:
        cmp byte [si], 0xFF
        je .pinta
        lea bx, [si+3]
.b1:
        cmp byte [bx], 0
        je .b3
        cmp byte [bx], '&'
        je .b2
        inc bx
        jmp .b1
.b2:
        mov ah, [bx+1]
        cmp ah, 'a'
        jb .b21
        sub ah, 32
.b21:
        cmp ah, al
        jne .b3
        mov [mn_sel], cl
        jmp short .elige
.b3:
        push ax
        call .salta
        pop ax
        inc cx
        jmp .b
.flecha:
        mov al, [mn_sel]
        cmp ah, 0x4B
        jne .f1
        dec al
        jns .f2
        mov al, [mn_n]
        dec al
        jmp short .f2
.f1:
        cmp ah, 0x4D
        jne .pinta
        inc al
        cmp al, [mn_n]
        jb .f2
        mov al, 0
.f2:
        mov [mn_sel], al
        jmp .pinta
.elige:
        mov si, [mn_def]
        mov cl, [mn_sel]
        mov ch, 0
        jcxz .el
.el0:
        call .salta
        loop .el0
.el:
        mov bx, [si+1]
        cmp byte [si], 0
        je .orden
        push word [mn_def]          ; otro menú; con Esc, se vuelve a este
        push word [mn_sel]
        mov si, bx
        call menu
        pop word [mn_sel]
        pop word [mn_def]
        jc .pinta
.orden:
        clc
        ret
.no:
        stc
        ret
; (interna) SI, a la entrada siguiente
.salta:
        add si, 3
.s:
        lodsb
        test al, al
        jnz .s
        ret

mn_raiz:
        SUBMENU mn_archivo, "&Archivo"
        SUBMENU mn_edicion, "&Edición"
        SUBMENU mn_buscar, "&Buscar"
        SUBMENU mn_formato, "&Formato"
        SUBMENU mn_pagina, "&Página"
        SUBMENU mn_imprimir, "&Imprimir"
        ORDEN ayuda, "A&yuda"
        ORDEN salir, "&Salir"
        db 0xFF
mn_archivo:
        ORDEN abrir, "&Abrir"
        ORDEN guardar, "&Guardar"
        ORDEN guardar_como, "Guardar &como"
        ORDEN nuevo, "&Nuevo"
        ORDEN inserta_archivo, "&Insertar archivo"
        SUBMENU mn_exportar, "&Exportar"
        ORDEN salir, "&Salir"
        db 0xFF
mn_exportar:
        ORDEN exp_dos, "&Texto de DOS (líneas partidas)"
        ORDEN exp_hoy, "Texto de &hoy (UTF-8)"
        ORDEN exp_rtf, "&RTF"
        db 0xFF
mn_edicion:
        ORDEN deshacer, "&Deshacer"
        ORDEN marcar, "&Marcar"
        ORDEN copiar, "&Copiar"
        ORDEN cortar, "Cor&tar"
        ORDEN pegar, "&Pegar"
        ORDEN borra_bloque_si, "&Borrar"
        ORDEN escribe_bloque, "&Escribir bloque"
        ORDEN contar, "Contar pa&labras"
        db 0xFF
mn_buscar:
        ORDEN buscar, "&Buscar"
        ORDEN buscar_sig, "&Siguiente"
        ORDEN reemplazar, "&Reemplazar"
        db 0xFF
mn_formato:
        ORDEN negrita, "&Negrita"
        ORDEN subrayado, "&Subrayado"
        ORDEN cursiva, "&Cursiva"
        ORDEN quita_formato, "&Quitar"
        ORDEN al_izq, "&Izquierda"
        ORDEN al_centro, "Cen&trado"
        ORDEN al_der, "De&recha"
        ORDEN al_just, "&Justificado"
        ORDEN sangria, "San&gría"
        ORDEN salto_pagina, "Salto de &página"
        ORDEN cambia_codigos, "&Ver códigos"
        db 0xFF
mn_pagina:
        ORDEN pide_margenes, "&Márgenes"
        ORDEN pide_ancho, "&Ancho"
        ORDEN pide_sang1, "&Sangría 1ª línea"
        ORDEN pide_largo, "&Largo"
        ORDEN pide_enc, "&Encabezado"
        ORDEN pide_pie, "&Pie"
        ORDEN pide_pag1, "&Número de la primera"
        SUBMENU mn_impresora, "&Impresora"
        ORDEN pide_hojas, "&Hojas sueltas"
        db 0xFF
mn_impresora:
        ORDEN imp_fx, "Epson &FX (7 bits)"
        ORDEN imp_lx, "Epson &LX o moderna"
        ORDEN imp_ibm, "&IBM Proprinter"
        ORDEN imp_txt, "Solo &texto"
        db 0xFF
mn_imprimir:
        ORDEN imprimir, "&Imprimir"
        ORDEN vista, "&Vista de la página"
        db 0xFF

borra_bloque_si:
        call bloque
        jc .x
        call fija_bloque
        jmp borra_bloque
.x:
        jmp pita

; ---- lo de la página ----
pide_margenes:
        call pide_mizq
        jc .x
        mov si, m_p_msup
        mov bx, cab + CAB_MSUP
        mov dx, 0x1400
        call pide_num
        jc .x
        mov si, m_p_minf
        mov bx, cab + CAB_MINF
        mov dx, 0x1400
        jmp pide_num
.x:
        ret
pide_mizq:
        mov si, m_p_mizq
        mov bx, cab + CAB_MIZQ
        mov dx, 0x3C00
        jmp pide_num
pide_ancho:
        mov si, m_p_ancho
        mov bx, cab + CAB_ANCHO
        mov dx, 0x4E14              ; de 20 a 78
        jmp pide_num
pide_sang1:
        mov si, m_p_sang1
        mov bx, cab + CAB_SANG1
        mov dx, 0x1400
        jmp pide_num
pide_largo:
        mov si, m_p_largo
        mov bx, cab + CAB_LARGO
        mov dx, 0xFA14
        jmp pide_num
pide_pag1:
        mov si, m_p_pag1
        mov bx, cab + CAB_PAG1
        mov dx, 0xFA00
        jmp pide_num
pide_enc:
        mov si, m_p_enc
        mov di, cab + CAB_ENC
        jmp short pide_texto_cab
pide_pie:
        mov si, m_p_pie
        mov di, cab + CAB_PIE
pide_texto_cab:
        mov cl, 46
        call pide
        jc .x
        mov byte [cambiado], 1
.x:
        ret
pide_hojas:
        mov si, m_p_hojas
        call pregunta
        mov ah, 1
        cmp al, 'S'
        je .p
        mov ah, 0
        cmp al, 'N'
        jne .x
.p:
        mov [cab+CAB_HOJAS], ah
        mov byte [cambiado], 1
.x:
        ret
imp_fx:
        mov al, 0
        jmp short pon_imp
imp_lx:
        mov al, 1
        jmp short pon_imp
imp_ibm:
        mov al, 2
        jmp short pon_imp
imp_txt:
        mov al, 3
pon_imp:
        mov [cab+CAB_IMPR], al
        mov byte [cambiado], 1
        ret

; ---- salir ----
salir:
        cmp byte [cambiado], 0
        je fuera
        mov si, m_cambios
        call pregunta
        cmp al, 'N'
        je fuera
        cmp al, 'S'
        jne .x
        call guardar
        jnc fuera
.x:
        ret
salir_sin:
        cmp byte [cambiado], 0
        je fuera
        mov si, m_salir_sin
        call pregunta
        cmp al, 'S'
        je fuera
        ret
guardar_salir:
        call guardar
        jnc fuera
        ret
fuera:
        call cierra_tmps
        mov al, [modo]              ; la pantalla como estaba (y sus letras)
        xor ah, ah
        int 0x10
        mov ax, 0x4C00
        int 0x21

; ---- el cursor: una raya al insertar y un bloque al sobrescribir ----
forma_cursor:
        mov cx, 0x0607
        cmp byte [modo], 7
        jne .c
        mov cx, 0x0B0C
.c:
        cmp byte [insertar], 0
        jne .x
        mov ch, 0
.x:
        mov ah, 1
        int 0x10
        ret

; =============================================================================
;  La ayuda (F1): páginas de texto; las líneas que empiezan por # van al revés
; =============================================================================
ayuda:
        mov si, t_ayuda
.pag:
        push es
        mov es, [vseg]
        xor di, di
        mov cx, 25 * 80
        mov ax, 0x0720
        rep stosw
        xor di, di
.lin:
        mov ah, 0x07
        cmp byte [si], '#'
        jne .c
        inc si
        mov ah, 0x70
.c:
        lodsb
        test al, al
        jz .fin
        cmp al, 12
        je .otra
        cmp al, 10
        je .nl
        stosw
        jmp .c
.nl:
        mov ax, di                  ; a la fila siguiente
        mov cl, 160
        div cl
        inc al
        mul cl
        mov di, ax
        jmp .lin
.otra:
        pop es
        push si
        mov dx, 0x184F
        call pon_cursor
        call lee_tecla
        pop si
        cmp al, 27
        jne .pag
        jmp short .x
.fin:
        pop es
        mov dx, 0x184F
        call pon_cursor
        call lee_tecla
.x:
        mov byte [filas_ok], 0
        ret
