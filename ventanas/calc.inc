; =============================================================================
;  Hispa Ventanas - Calculadora
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Cuatro operaciones con coma decimal. Los números son enteros de 64 bits
;  con seis decimales implícitos (coma fija: no hace falta coprocesador, que
;  un 386 puede no tener): hasta doce cifras enteras y seis decimales.
;  Con el ratón (botones) o con el teclado: cifras, coma o punto, + - * /,
;  Intro o = (resultado), Retroceso (borra una cifra), Esc o C (borra todo).
;  Cada botón tiene como id el carácter de su tecla.
; =============================================================================

CALC_ESC    equ 1000000             ; seis decimales
CALC_DEC    equ 6
CALC_ENT    equ 12
CB_AN       equ 40
CB_AL       equ 32
%define CB_E E_HIJA | E_SINFOCO     ; (el foco sigue en la ventana)

c_calc      dd calc_proc
            db C_CREMA, 0, 0, 0

calc_plant:
        PLANTILLA c_calc, t_calc, E_TITULO | E_CERRAR | E_MINIM, 500, 60, 204, 269, 0
calc_hijos:
        PLANTILLA c_estatico, calc_pant, E_HIJA | SS_HUNDIDO | SS_DERECHA, 10, 10, 178, 28, 100
        PLANTILLA c_boton, t_cb_c, CB_E, 10, 48, CB_AN, CB_AL, 'C'
        PLANTILLA c_boton, t_cb_borra, CB_E, 56, 48, CB_AN, CB_AL, 8
        PLANTILLA c_boton, t_cb_signo, CB_E, 102, 48, CB_AN, CB_AL, 'N'
        PLANTILLA c_boton, t_cb_div, CB_E, 148, 48, CB_AN, CB_AL, '/'
        PLANTILLA c_boton, t_cb_7, CB_E, 10, 86, CB_AN, CB_AL, '7'
        PLANTILLA c_boton, t_cb_8, CB_E, 56, 86, CB_AN, CB_AL, '8'
        PLANTILLA c_boton, t_cb_9, CB_E, 102, 86, CB_AN, CB_AL, '9'
        PLANTILLA c_boton, t_cb_por, CB_E, 148, 86, CB_AN, CB_AL, '*'
        PLANTILLA c_boton, t_cb_4, CB_E, 10, 124, CB_AN, CB_AL, '4'
        PLANTILLA c_boton, t_cb_5, CB_E, 56, 124, CB_AN, CB_AL, '5'
        PLANTILLA c_boton, t_cb_6, CB_E, 102, 124, CB_AN, CB_AL, '6'
        PLANTILLA c_boton, t_cb_menos, CB_E, 148, 124, CB_AN, CB_AL, '-'
        PLANTILLA c_boton, t_cb_1, CB_E, 10, 162, CB_AN, CB_AL, '1'
        PLANTILLA c_boton, t_cb_2, CB_E, 56, 162, CB_AN, CB_AL, '2'
        PLANTILLA c_boton, t_cb_3, CB_E, 102, 162, CB_AN, CB_AL, '3'
        PLANTILLA c_boton, t_cb_mas, CB_E, 148, 162, CB_AN, CB_AL, '+'
        PLANTILLA c_boton, t_cb_0, CB_E, 10, 200, CB_AN, CB_AL, '0'
        PLANTILLA c_boton, t_cb_coma, CB_E, 56, 200, CB_AN, CB_AL, ','
        PLANTILLA c_boton, t_cb_igual, CB_E | BS_DEFECTO, 102, 200, 86, CB_AL, '='
        dd 0

t_cb_c      db "C", 0
t_cb_borra  db "<", 0
t_cb_signo  db "+/-", 0
t_cb_div    db "/", 0
t_cb_por    db "x", 0
t_cb_menos  db "-", 0
t_cb_mas    db "+", 0
t_cb_coma   db ",", 0
t_cb_igual  db "=", 0
t_cb_0      db "0", 0
t_cb_1      db "1", 0
t_cb_2      db "2", 0
t_cb_3      db "3", 0
t_cb_4      db "4", 0
t_cb_5      db "5", 0
t_cb_6      db "6", 0
t_cb_7      db "7", 0
t_cb_8      db "8", 0
t_cb_9      db "9", 0
t_error     db "Error", 0

calc_tarea:
        mov esi, calc_plant
        xor ebp, ebp
        call ven_crear
        mov [calc_ven], eax
        jmp app_bucle

calc_proc:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov dword [calc_ven], 0
        call msg_salir
        jmp .nada
.crear:
        mov dword [ebp+V_ICONO], ico_calc
        mov esi, calc_hijos
        call ven_lista
        mov al, 'C'
        call calc_tecla
        jmp .nada
.tecla:
        mov al, '='
        cmp ecx, TC_INTRO
        je .da
        mov al, 'C'
        cmp ecx, TC_ESC
        je .da
        mov al, 8
        cmp ecx, TC_RETRO
        je .da
        jmp .nada
.orden:
        test ecx, 0xFFFF0000
        jnz .nada
.car:
        mov al, cl
.da:
        call calc_tecla
        mov ecx, 100                ; la pantalla, a repintar
        call ven_hija_id
        mov ebp, eax
        call ven_invalidar
.nada:
        xor eax, eax
        ret

; ---- la cuenta ----
; AL=tecla: cifra, coma, + - * /, =, C, 8 (borrar), N (cambiar el signo)
calc_tecla:
        pushad
        cmp al, '.'
        jne .k0
        mov al, ','
.k0:
        cmp al, 'x'
        je .k1
        cmp al, 'X'
        jne .k2
.k1:
        mov al, '*'
.k2:
        cmp al, 'c'
        jne .k3
        mov al, 'C'
.k3:
        cmp al, 'C'
        je .borra_todo
        cmp byte [calc_error], 0    ; tras un error solo vale empezar de nuevo
        je .k4
        cmp al, '0'
        jb .x
        cmp al, '9'
        ja .x
        push eax
        call calc_reiniciar
        pop eax
.k4:
        cmp al, '0'
        jb .k5
        cmp al, '9'
        jbe .cifra
.k5:
        cmp al, ','
        je .coma
        cmp al, 8
        je .borra
        cmp al, 'N'
        je .signo
        cmp al, '='
        je .igual
        cmp al, '+'
        je .oper
        cmp al, '-'
        je .oper
        cmp al, '*'
        je .oper
        cmp al, '/'
        je .oper
        jmp .x
.borra_todo:
        call calc_reiniciar
        jmp .x
.cifra:
        call calc_empezar
        ; ¿caben más cifras?
        mov esi, calc_pant
        xor ecx, ecx                ; cifras enteras
        xor edx, edx                ; decimales (-1: no hay coma)
        dec edx
.c1:
        mov bl, [esi]
        inc esi
        test bl, bl
        jz .c3
        cmp bl, ','
        jne .c2
        xor edx, edx
        jmp .c1
.c2:
        cmp bl, '-'
        je .c1
        test edx, edx
        js .c2e
        inc edx
        jmp .c1
.c2e:
        inc ecx
        jmp .c1
.c3:
        test edx, edx
        js .c4
        cmp edx, CALC_DEC
        jae .x
        jmp .c5
.c4:
        cmp ecx, CALC_ENT
        jae .x
        cmp word [calc_pant], '0'   ; un cero solo se sustituye
        jne .c5
        mov [calc_pant], al
        jmp .x
.c5:
        mov [esi-1], al
        mov byte [esi], 0
        jmp .x
.coma:
        call calc_empezar
        mov esi, calc_pant
.m1:
        lodsb
        cmp al, ','
        je .x
        test al, al
        jnz .m1
        mov word [esi-1], ','
        jmp .x
.borra:
        cmp byte [calc_nuevo], 0
        jne .x
        mov esi, calc_pant
        call cad_largo
        mov byte [esi+eax-1], 0
        cmp eax, 1
        je .b1
        cmp word [calc_pant], '-'
        jne .x
.b1:
        mov word [calc_pant], '0'
        jmp .x
.signo:
        mov byte [calc_nuevo], 0
        cmp byte [calc_pant], '-'
        je .s1
        cmp word [calc_pant], '0'
        je .x
        mov esi, calc_pant          ; se abre sitio para el signo
        call cad_largo
        lea ecx, [eax+1]
        lea esi, [calc_pant+eax]
        lea edi, [esi+1]
        std
        rep movsb
        cld
        mov byte [calc_pant], '-'
        jmp .x
.s1:
        mov esi, calc_pant + 1
        mov edi, calc_pant
.s2:
        lodsb
        stosb
        test al, al
        jnz .s2
        jmp .x
.oper:
        push eax
        call calc_resolver
        pop eax
        mov [calc_op], al
        jmp .x
.igual:
        call calc_resolver
        mov byte [calc_op], 0
.x:
        popad
        ret

; (interna) todo a cero
calc_reiniciar:
        xor eax, eax
        mov [calc_acum], eax
        mov [calc_acum+4], eax
        mov [calc_op], al
        mov [calc_error], al
        mov byte [calc_nuevo], 1
        mov word [calc_pant], '0'
        ret

; (interna) si lo de la pantalla es un resultado, se empieza un número nuevo
calc_empezar:
        cmp byte [calc_nuevo], 0
        je .x
        mov byte [calc_nuevo], 0
        mov word [calc_pant], '0'
.x:
        ret

; (interna) hace la operación pendiente con lo que hay en la pantalla y
; enseña el resultado
calc_resolver:
        cmp byte [calc_nuevo], 0
        jne .x                      ; (dos operaciones seguidas: vale la última)
        call calc_leer              ; EDX:EAX = el número de la pantalla
        mov ebx, eax
        mov ecx, edx
        mov eax, [calc_acum]
        mov edx, [calc_acum+4]
        mov byte [calc_nuevo], 1
        cmp byte [calc_op], '+'
        je .suma
        cmp byte [calc_op], '-'
        je .resta
        cmp byte [calc_op], '*'
        je .por
        cmp byte [calc_op], '/'
        je .entre
        mov eax, ebx                ; sin operación: el número tal cual
        mov edx, ecx
        jmp .ya
.suma:
        add eax, ebx
        adc edx, ecx
        jo .error
        jmp .ya
.resta:
        sub eax, ebx
        sbb edx, ecx
        jo .error
        jmp .ya
.por:
        call calc_signos            ; a x b / 1.000.000
        call calc_mul
        mov ebx, CALC_ESC
        xor ecx, ecx
        call calc_div
        jc .error
        call calc_poner_signo
        jmp .ya
.entre:
        mov esi, ebx
        or esi, ecx
        jz .error                   ; entre cero
        call calc_signos            ; a x 1.000.000 / b
        push ebx
        push ecx
        mov ebx, CALC_ESC
        xor ecx, ecx
        call calc_mul
        pop ecx
        pop ebx
        call calc_div
        jc .error
        call calc_poner_signo
.ya:
        mov [calc_acum], eax
        mov [calc_acum+4], edx
        call calc_escribir
.x:
        ret
.error:
        mov byte [calc_error], 1
        mov esi, t_error
        mov edi, calc_pant
        call pon_cad
        mov byte [edi], 0
        ret

; (interna) EDX:EAX y ECX:EBX pasan a valor absoluto; calc_neg = signo del
; producto o del cociente
calc_signos:
        mov byte [calc_neg], 0
        test edx, edx
        jns .a
        not byte [calc_neg]
        neg edx
        neg eax
        sbb edx, 0
.a:
        test ecx, ecx
        jns .b
        not byte [calc_neg]
        neg ecx
        neg ebx
        sbb ecx, 0
.b:
        ret

calc_poner_signo:
        test edx, edx               ; (más de 63 bits: no cabe)
        js calc_resolver.error_pila
        cmp byte [calc_neg], 0
        je .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret

; (interna) EDX:EAX x ECX:EBX (sin signo) -> calc_m (128 bits)
calc_mul:
        pushad
        mov esi, eax                ; a bajo
        mov edi, edx                ; a alto
        mul ebx                     ; a bajo x b bajo
        mov [calc_m], eax
        mov [calc_m+4], edx
        mov eax, edi
        mul ecx                     ; a alto x b alto
        mov [calc_m+8], eax
        mov [calc_m+12], edx
        mov eax, esi
        mul ecx                     ; a bajo x b alto
        add [calc_m+4], eax
        adc [calc_m+8], edx
        adc dword [calc_m+12], 0
        mov eax, edi
        mul ebx                     ; a alto x b bajo
        add [calc_m+4], eax
        adc [calc_m+8], edx
        adc dword [calc_m+12], 0
        popad
        ret

; (interna) calc_m (128 bits) / ECX:EBX -> EDX:EAX; CF=1 si no cabe en 64
calc_div:
        push esi
        push edi
        push ebp
        xor esi, esi                ; resto
        xor edi, edi
        mov ebp, 128
.l:
        shl dword [calc_m], 1
        rcl dword [calc_m+4], 1
        rcl dword [calc_m+8], 1
        rcl dword [calc_m+12], 1
        rcl esi, 1
        rcl edi, 1
        jc .resta
        cmp edi, ecx
        ja .resta
        jb .sig
        cmp esi, ebx
        jb .sig
.resta:
        sub esi, ebx
        sbb edi, ecx
        or byte [calc_m], 1
.sig:
        dec ebp
        jnz .l
        mov eax, [calc_m]
        mov edx, [calc_m+4]
        mov esi, [calc_m+8]
        or esi, [calc_m+12]
        pop ebp
        pop edi
        pop esi
        jnz .no
        clc
        ret
.no:
        stc
        ret

; (interna) el número de la pantalla -> EDX:EAX (con seis decimales)
calc_leer:
        push ebx
        push ecx
        push esi
        push edi
        mov esi, calc_pant
        xor eax, eax
        xor edx, edx
        mov edi, -1                 ; decimales leídos (-1: aún no hay coma)
        cmp byte [esi], '-'
        jne .c
        inc esi
.c:
        movzx ebx, byte [esi]
        inc esi
        test bl, bl
        jz .fin
        cmp bl, ','
        jne .d
        xor edi, edi
        jmp .c
.d:
        sub bl, '0'
        call calc_por10
        add eax, ebx
        adc edx, 0
        test edi, edi
        js .c
        inc edi
        jmp .c
.fin:
        test edi, edi
        jns .f1
        xor edi, edi
.f1:
        cmp edi, CALC_DEC
        jae .f2
        call calc_por10
        inc edi
        jmp .f1
.f2:
        cmp byte [calc_pant], '-'
        jne .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

; (interna) EDX:EAX por 10
calc_por10:
        push ebx
        push ecx
        shld edx, eax, 1
        shl eax, 1
        mov ebx, eax
        mov ecx, edx
        shld edx, eax, 2
        shl eax, 2
        add eax, ebx
        adc edx, ecx
        pop ecx
        pop ebx
        ret

; (interna) escribe calc_acum en la pantalla: signo, enteros, coma y los
; decimales que no sean ceros finales
calc_escribir:
        pushad
        mov eax, [calc_acum]
        mov edx, [calc_acum+4]
        mov edi, calc_pant
        test edx, edx
        jns .p
        mov byte [edi], '-'
        inc edi
        neg edx
        neg eax
        sbb edx, 0
.p:
        mov ebx, CALC_ESC           ; entre un millón: enteros y decimales
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        div ebx
        mov esi, eax                ; enteros, parte alta
        mov eax, ecx
        div ebx
        mov ebp, edx                ; los decimales
        mov edx, esi                ; EDX:EAX = enteros
        cmp edx, 232                ; (más de un billón: no cabe en la pantalla)
        ja .error
        xor ecx, ecx
        mov ebx, 10
.e1:
        push eax                    ; EDX:EAX entre 10
        mov eax, edx
        xor edx, edx
        div ebx
        mov esi, eax
        pop eax
        div ebx
        push edx
        inc ecx
        mov edx, esi
        mov esi, eax
        or esi, edx
        jnz .e1
        cmp ecx, CALC_ENT + 1
        ja .error_pila
.e2:
        pop eax
        add al, '0'
        stosb
        dec ecx
        jnz .e2
        test ebp, ebp
        jz .fin
        mov al, ','
        stosb
        mov eax, ebp
        mov ecx, CALC_DEC
        mov esi, CALC_ESC / 10
.d1:
        xor edx, edx
        div esi
        add al, '0'
        stosb
        mov eax, edx
        push eax
        mov eax, esi
        xor edx, edx
        div ebx
        mov esi, eax
        pop eax
        test eax, eax
        jz .fin
        dec ecx
        jnz .d1
.fin:
        mov byte [edi], 0
        popad
        ret
.error_pila:
        lea esp, [esp+ecx*4]
.error:
        popad
        jmp calc_resolver.error

; (un producto que no cabe: se quita la vuelta de calc_poner_signo)
calc_resolver.error_pila:
        add esp, 4
        jmp calc_resolver.error
