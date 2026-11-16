; =============================================================================
;  Cuentas - el cálculo: valor de las celdas, fórmulas y números
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los números son de COMA FIJA: enteros de 64 bits con signo que cuentan
;  cienmillonésimas (CU_ESC = 10^8: ocho decimales). No hay coma flotante,
;  así que los decimales son exactos: 0,1 + 0,2 es 0,3. Caben hasta unos
;  92.000 millones (2^63 / 10^8); lo que no cabe da error, nunca un número
;  falso.
;       suma y resta     exactas
;       multiplicación   el producto entero (128 bits) entre 10^8
;       división         el dividendo por 10^8 (96 bits) entre el divisor
;  Las dos últimas redondean a la cienmillonésima más cercana (la mitad,
;  alejándose de cero). En la celda se ven, como mucho, VER_DEC decimales,
;  también redondeados (num_fmt).
;
;  Las fórmulas (lo que va detrás del «=»):
;       expresión := término { (+|-) término }
;       término   := factor { (*|/) factor }
;       factor    := (+|-) factor | primario { % }
;       primario  := número | celda | ( expresión ) | función
;       función   := SUMA(rangos) | PROMEDIO(rangos) | MAX(rangos)
;                    | MIN(rangos) | CONTAR(rangos)
;                    | REDONDEAR(expresión; decimales) | ABS(expresión)
;       rangos    := rango { ; rango }       rango := celda [ : celda ]
;  «%» divide entre cien (=A1*21%). Las mayúsculas dan igual. En los rangos
;  solo cuentan las celdas con número o fórmula: las vacías y las de texto no
;  entran en PROMEDIO, MAX, MIN ni CONTAR (y en una cuenta valen 0).
;
;  Lo que pide a quien lo incluye:
;       NCOL, NFIL, CEL_CARS                constantes de la hoja
;       cel_puntero     ECX=columna EDX=fila -> ESI=el texto de la celda
;                       (conserva los demás registros)
;  y lo que da:
;       calc_celda      ECX=columna EDX=fila -> EDX:EAX; [cv_tipo]: 0 vacía,
;                       1 número, 2 texto; [ev_err]: 0 o EV_ERROR, EV_CIRC,
;                       EV_REF
;       calc_olvida     hay que llamarla cuando cambia una celda: cada
;                       fórmula calculada se apunta, para no repetirla (sin
;                       eso, una hoja donde cada celda suma las dos de antes
;                       tardaría una eternidad)
;       [ev_pila_min]   si no es 0, la pila no baja de ahí: da error antes
;       cad_a_num       ESI=texto -> EDX:EAX; CF=1 si no es un número
;       num_fmt         EDX:EAX -> EDI=texto como se ve en la celda
;  Todas las rutinas ev_* dejan el resultado en EDX:EAX y pueden cambiar los
;  demás registros, menos EBP.
; =============================================================================
CU_ESC      equ 100000000           ; ocho decimales por dentro
CU_DEC      equ 8
VER_DEC     equ 4                   ; los que se ven, como mucho
EV_PROF     equ 400                 ; fórmulas metidas unas en otras, como mucho
EV_ERROR    equ 1                   ; «#ERROR»
EV_CIRC     equ 2                   ; «#CIRC»: la celda se pide a sí misma
EV_REF      equ 3                   ; «#REF»: una celda que no existe

; =============================================================================
;  El valor de una celda
; =============================================================================
; el valor de una celda, desde fuera: ECX=columna EDX=fila
calc_celda:
        mov byte [ev_err], 0
        mov byte [ev_hondo], 0
        mov dword [ev_prof], 0
; (y desde dentro de una fórmula)
celda_valor:
        cmp ecx, NCOL
        jae .ref
        cmp edx, NFIL
        jae .ref
        imul edi, edx, NCOL
        add edi, ecx                ; EDI=el número de la celda
        mov al, [cv_est+edi]
        cmp al, 1                   ; ¿se está calculando ya? Se pide a sí misma
        je .circ
        cmp al, 2                   ; ¿calculada ya? Lo apuntado
        je .apuntada
        call cel_puntero
        mov al, [esi]
        test al, al
        jz .vacia
        cmp al, '='
        je .formula
        call cad_a_num
        jc .texto
        mov byte [cv_tipo], 1
        ret
.texto:
        mov byte [cv_tipo], 2
        jmp .cero
.vacia:
        mov byte [cv_tipo], 0
.cero:
        xor eax, eax
        xor edx, edx
        ret
.apuntada:
        mov byte [cv_tipo], 1
        mov al, [cv_err+edi]
        test al, al
        jz .ap1
        call ev_pon_err
        jmp .cero
.ap1:
        mov eax, [cv_val+edi*8]
        mov edx, [cv_val+edi*8+4]
        ret
.formula:
        cmp dword [ev_prof], EV_PROF        ; demasiadas, unas dentro de otras
        jae .honda
        cmp esp, [ev_pila_min]              ; (o la pila se acaba)
        jb .honda
        inc dword [ev_prof]
        mov byte [cv_est+edi], 1
        push edi
        push dword [ev_ptr]
        movzx eax, byte [ev_err]
        push eax                    ; ¿había ya un error antes de esta celda?
        inc esi
        mov [ev_ptr], esi
        call ev_expr
        push eax
        call ev_car                 ; detrás no puede quedar nada
        test al, al
        pop eax
        jz .fin
        call ev_mal
.fin:
        pop ebx
        pop dword [ev_ptr]
        pop edi
        dec dword [ev_prof]
        mov byte [cv_tipo], 1
        mov cl, [ev_err]
        test cl, cl
        jz .f1
        xor eax, eax
        xor edx, edx
.f1:
        mov byte [cv_est+edi], 0
        test ebx, ebx               ; se apunta si lo que ha salido es todo
        jnz .f2                     ; suyo: sin un error de antes y sin haber
        cmp byte [ev_hondo], 0      ; tocado fondo
        jne .f2
        mov [cv_val+edi*8], eax
        mov [cv_val+edi*8+4], edx
        mov [cv_err+edi], cl
        mov byte [cv_est+edi], 2
.f2:
        ret
.honda:
        mov byte [ev_hondo], 1
        call ev_mal
        jmp .tipo1
.circ:
        mov al, EV_CIRC
        call ev_pon_err
        jmp .tipo1
.ref:
        mov al, EV_REF
        call ev_pon_err
.tipo1:
        mov byte [cv_tipo], 1
        jmp .cero

; olvida lo apuntado: hay que llamarla cada vez que cambia una celda
calc_olvida:
        push eax
        push ecx
        push edi
        mov edi, cv_est
        mov ecx, NCOL * NFIL
        xor al, al
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret

; apunta el error AL (ev_mal: EV_ERROR), si no hay ya otro: el primero manda
ev_mal:
        mov al, EV_ERROR
ev_pon_err:
        cmp byte [ev_err], 0
        jne .x
        mov [ev_err], al
.x:
        ret

; =============================================================================
;  Las fórmulas
; =============================================================================
; salta los espacios -> AL=el carácter que viene (sin gastarlo)
ev_car:
        push esi
        mov esi, [ev_ptr]
.b:
        mov al, [esi]
        cmp al, ' '
        jne .x
        inc esi
        jmp .b
.x:
        mov [ev_ptr], esi
        pop esi
        ret

; expresión := término { (+|-) término }
ev_expr:
        call ev_term
.b:
        push eax
        call ev_car
        mov cl, al
        pop eax
        cmp cl, '+'
        je .mas
        cmp cl, '-'
        je .menos
        ret
.mas:
        inc dword [ev_ptr]
        push edx
        push eax
        call ev_term
        pop ebx
        pop ecx                     ; ECX:EBX=lo de la izquierda
        add eax, ebx
        adc edx, ecx
        jno .b
        jmp .desb
.menos:
        inc dword [ev_ptr]
        push edx
        push eax
        call ev_term
        pop ebx
        pop ecx
        sub ebx, eax
        sbb ecx, edx
        mov eax, ebx
        mov edx, ecx
        jno .b
.desb:
        call ev_mal                 ; no cabe
        xor eax, eax
        xor edx, edx
        jmp .b

; término := factor { (*|/) factor }
ev_term:
        call ev_factor
.b:
        push eax
        call ev_car
        mov cl, al
        pop eax
        cmp cl, '*'
        je .por
        cmp cl, '/'
        je .entre
        ret
.por:
        inc dword [ev_ptr]
        push edx
        push eax
        call ev_factor
        pop ebx
        pop ecx
        call ev_mul
        jmp .b
.entre:
        inc dword [ev_ptr]
        push edx
        push eax
        call ev_factor
        pop ebx
        pop ecx
        call ev_div
        jmp .b

; factor := (+|-) factor | primario { % }
ev_factor:
        call ev_car
        cmp al, '-'
        je .neg
        cmp al, '+'
        je .pos
        call ev_primario
.pct:
        push eax
        call ev_car
        mov cl, al
        pop eax
        cmp cl, '%'
        jne .x
        inc dword [ev_ptr]
        mov ebx, eax                ; entre cien
        mov ecx, edx
        mov eax, 100 * CU_ESC & 0xFFFFFFFF
        mov edx, 100 * CU_ESC >> 32
        call ev_div
        jmp .pct
.x:
        ret
.pos:
        inc dword [ev_ptr]
        jmp ev_factor
.neg:
        inc dword [ev_ptr]
        call ev_factor
        neg edx
        neg eax
        sbb edx, 0
        ret

; primario := número | celda | ( expresión ) | función
ev_primario:
        call ev_car
        cmp al, '('
        je .paren
        cmp al, ','
        je .num
        cmp al, '.'
        je .num
        cmp al, '0'
        jb .mal
        cmp al, '9'
        jbe .num
        and al, 0xDF                ; (a mayúscula)
        cmp al, 'A'
        jb .mal
        cmp al, 'Z'
        jbe ev_nombre
.mal:
        call ev_mal
        xor eax, eax
        xor edx, edx
        ret
.num:
        mov esi, [ev_ptr]
        call lee_num
        mov [ev_ptr], esi
        jc .mal
        ret
.paren:
        inc dword [ev_ptr]
        call ev_expr
        ; (sigue en ev_cierra)
; gasta el «)» que tiene que venir; si no viene, error. Conserva EDX:EAX
ev_cierra:
        push eax
        call ev_car
        cmp al, ')'
        jne .mal
        inc dword [ev_ptr]
        pop eax
        ret
.mal:
        call ev_mal
        pop eax
        ret
; gasta el «(» que tiene que venir
ev_abre:
        push eax
        call ev_car
        cmp al, '('
        jne ev_cierra.mal
        inc dword [ev_ptr]
        pop eax
        ret

; un nombre: una celda (una letra y su fila) o una función
ev_nombre:
        mov esi, [ev_ptr]
        mov edi, ev_nom
        xor ecx, ecx
.l:
        mov al, [esi]
        and al, 0xDF
        cmp al, 'A'
        jb .f
        cmp al, 'Z'
        ja .f
        cmp ecx, 11
        jae .s
        mov [edi+ecx], al
        inc ecx
.s:
        inc esi
        jmp .l
.f:
        mov byte [edi+ecx], 0
        cmp ecx, 1                  ; una letra y una cifra: una celda
        jne .fun
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .fun
        call ev_ref
        jc .cero
        push ebp
        call celda_valor
        pop ebp
        ret
.fun:
        mov [ev_ptr], esi
        mov ebx, ev_funciones       ; la tabla: nombre, 0, dd rutina, db dato
.b:
        cmp byte [ebx], 0
        je .mal
        mov esi, ebx
        mov edi, ev_nom
.c:
        mov al, [esi]
        cmp al, [edi]
        jne .otra
        inc esi
        inc edi
        test al, al
        jnz .c
        movzx eax, byte [esi+4]
        jmp [esi]
.otra:
        mov al, [esi]               ; a la siguiente
        inc esi
        test al, al
        jnz .otra
        lea ebx, [esi+5]
        jmp .b
.mal:
        call ev_mal
.cero:
        xor eax, eax
        xor edx, edx
        ret
ev_funciones:
        db "SUMA", 0
        dd ev_rangos
        db 0
        db "PROMEDIO", 0
        dd ev_rangos
        db 1
        db "MAX", 0
        dd ev_rangos
        db 2
        db "MIN", 0
        dd ev_rangos
        db 3
        db "CONTAR", 0
        dd ev_rangos
        db 4
        db "REDONDEAR", 0
        dd ev_redondear
        db 0
        db "ABS", 0
        dd ev_abs
        db 0
        db 0

; lee una celda (letra y fila) de [ev_ptr] -> ECX=columna EDX=fila; CF=1 (y el
; error apuntado) si no es una celda o no existe
ev_ref:
        call ev_car
        mov esi, [ev_ptr]
        and al, 0xDF
        sub al, 'A'
        cmp al, 25
        ja .mal
        movzx ecx, al
        inc esi
        xor edx, edx
        xor ebx, ebx                ; cuántas cifras
.l:
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .f
        inc ebx
        inc esi
        cmp edx, 100000             ; (que no desborde: ya es demasiado)
        jae .l
        imul edx, 10
        add edx, eax
        jmp .l
.f:
        mov [ev_ptr], esi
        test ebx, ebx
        jz .mal
        dec edx                     ; la fila 1 es la 0
        cmp edx, NFIL
        jae .ref
        cmp ecx, NCOL
        jae .ref
        clc
        ret
.ref:
        mov al, EV_REF
        call ev_pon_err
        stc
        ret
.mal:
        call ev_mal
        stc
        ret

; ---- SUMA, PROMEDIO, MAX, MIN y CONTAR: EAX=cuál (de 0 a 4) ----
%define R_MODO  dword [ebp-4]
%define R_C1    dword [ebp-8]
%define R_F1    dword [ebp-12]
%define R_C2    dword [ebp-16]
%define R_F2    dword [ebp-20]
%define R_C     dword [ebp-24]
%define R_F     dword [ebp-28]
%define R_N     dword [ebp-32]      ; cuántas celdas con número
%define R_SL    dword [ebp-36]      ; la suma
%define R_SH    dword [ebp-40]
%define R_EL    dword [ebp-44]      ; la mayor o la menor
%define R_EH    dword [ebp-48]
ev_rangos:
        push ebp
        mov ebp, esp
        sub esp, 48
        mov R_MODO, eax
        xor eax, eax
        mov R_N, eax
        mov R_SL, eax
        mov R_SH, eax
        mov R_EL, eax
        mov R_EH, eax
        call ev_abre
.rango:
        call ev_ref
        jc .fin
        mov R_C1, ecx
        mov R_F1, edx
        mov R_C2, ecx
        mov R_F2, edx
        call ev_car
        cmp al, ':'
        jne .orden
        inc dword [ev_ptr]
        call ev_ref
        jc .fin
        mov R_C2, ecx
        mov R_F2, edx
.orden:
        mov eax, R_C1               ; de la menor a la mayor
        cmp eax, R_C2
        jbe .o1
        xchg eax, R_C2
        mov R_C1, eax
.o1:
        mov eax, R_F1
        cmp eax, R_F2
        jbe .o2
        xchg eax, R_F2
        mov R_F1, eax
.o2:
        mov eax, R_F1
        mov R_F, eax
.fila:
        mov eax, R_C1
        mov R_C, eax
.col:
        mov ecx, R_C
        mov edx, R_F
        push ebp
        call celda_valor
        pop ebp
        cmp byte [ev_err], 0
        jne .fin
        cmp byte [cv_tipo], 1       ; solo cuentan los números
        jne .sig
        inc R_N
        add R_SL, eax
        adc R_SH, edx
        jno .ext
        call ev_mal
        jmp .fin
.ext:
        cmp R_N, 1                  ; el primero es el mayor y el menor
        je .pon
        cmp edx, R_EH               ; ¿mayor que el que hay? (con signo)
        jg .mayor
        jl .menor
        cmp eax, R_EL
        ja .mayor
        je .sig
.menor:
        cmp R_MODO, 3
        je .pon
        jmp .sig
.mayor:
        cmp R_MODO, 2
        jne .sig
.pon:
        mov R_EL, eax
        mov R_EH, edx
.sig:
        mov eax, R_C
        inc eax
        mov R_C, eax
        cmp eax, R_C2
        jbe .col
        mov eax, R_F
        inc eax
        mov R_F, eax
        cmp eax, R_F2
        jbe .fila
        call ev_car
        cmp al, ';'                 ; ¿otro rango?
        jne .cierra
        inc dword [ev_ptr]
        jmp .rango
.cierra:
        call ev_cierra
.fin:
        mov eax, R_SL
        mov edx, R_SH
        mov ecx, R_MODO
        test ecx, ecx
        jz .x                       ; SUMA
        cmp ecx, 1
        je .promedio
        cmp ecx, 4
        je .contar
        mov eax, R_EL               ; MAX o MIN
        mov edx, R_EH
        jmp .x
.contar:
        mov eax, R_N
        mov ecx, CU_ESC
        mul ecx
        jmp .x
.promedio:
        mov eax, R_N                ; la suma entre cuántos (sin ninguno: error)
        mov ecx, CU_ESC
        mul ecx
        mov ebx, R_SL
        mov ecx, R_SH
        call ev_div
.x:
        mov esp, ebp
        pop ebp
        ret

; ---- ABS(expresión) ----
ev_abs:
        call ev_abre
        call ev_expr
        call ev_cierra
; el valor absoluto de EDX:EAX
abs64:
        test edx, edx
        jns .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret

; ---- REDONDEAR(expresión; decimales): a tantos decimales, de 0 a 8 ----
ev_redondear:
        call ev_abre
        call ev_expr
        push edx
        push eax
        call ev_car
        cmp al, ';'
        jne .mal
        inc dword [ev_ptr]
        call ev_expr
        call ev_cierra
        mov ebx, CU_ESC             ; los decimales: un entero de 0 a 8
        call div64
        test ecx, ecx
        jnz .mal
        test edx, edx
        jnz .mal
        cmp eax, CU_DEC
        ja .mal
        mov ebx, CU_DEC
        sub ebx, eax
        mov ebx, [pot10+ebx*4]      ; EBX=lo que vale la última cifra que queda
        pop eax
        pop edx
        mov esi, edx                ; (el signo)
        call abs64
        call div64                  ; entre eso, redondeando...
        shl ecx, 1
        cmp ecx, ebx
        jb .r
        add eax, 1
        adc edx, 0
.r:
        mov ecx, edx                ; ...y otra vez por eso
        mul ebx
        mov edi, eax
        mov eax, ecx
        mov ecx, edx
        mul ebx
        add eax, ecx
        mov edx, eax
        mov eax, edi
        js .desb                    ; (el redondeo lo ha sacado de los 63 bits)
        test esi, esi
        jns .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret
.mal:
        add esp, 8
.desb:
        call ev_mal
        xor eax, eax
        xor edx, edx
        ret

; =============================================================================
;  La aritmética de coma fija
; =============================================================================
; ECX:EBX por EDX:EAX -> EDX:EAX, redondeado; si no cabe, error y cero
ev_mul:
        push ebp
        mov ebp, edx                ; el signo del resultado: el bit de arriba
        xor ebp, ecx
        call abs64
        xchg eax, ebx
        xchg edx, ecx
        call abs64
        mov esi, eax                ; EDI:ESI por ECX:EBX, en cu_m (128 bits)
        mov edi, edx
        mul ebx
        mov [cu_m], eax
        mov [cu_m+4], edx
        mov eax, edi
        mul ecx
        mov [cu_m+8], eax
        mov [cu_m+12], edx
        mov eax, esi
        mul ecx
        add [cu_m+4], eax
        adc [cu_m+8], edx
        adc dword [cu_m+12], 0
        mov eax, edi
        mul ebx
        add [cu_m+4], eax
        adc [cu_m+8], edx
        adc dword [cu_m+12], 0
        mov ebx, CU_ESC             ; entre 10^8: de la palabra de más peso a la
        xor edx, edx                ; de menos, arrastrando el resto
        mov ecx, 12
.d:
        mov eax, [cu_m+ecx]
        div ebx
        mov [cu_m+ecx], eax
        sub ecx, 4
        jns .d
        shl edx, 1                  ; el resto: de la mitad arriba, uno más
        cmp edx, ebx
        jb .r
        add dword [cu_m], 1
        adc dword [cu_m+4], 0
        adc dword [cu_m+8], 0
        adc dword [cu_m+12], 0
.r:
        mov eax, [cu_m]
        mov edx, [cu_m+4]
        mov ecx, [cu_m+8]
        or ecx, [cu_m+12]
        jmp ev_signo

; ECX:EBX entre EDX:EAX -> EDX:EAX, redondeado; entre cero o si no cabe,
; error y cero
ev_div:
        push ebp
        mov ebp, edx
        xor ebp, ecx
        call abs64
        xchg eax, ebx
        xchg edx, ecx
        call abs64                  ; EDX:EAX=dividendo ECX:EBX=divisor
        mov esi, ecx
        or esi, ebx
        jz ev_signo.mal             ; entre cero
        mov edi, edx                ; el dividendo por 10^8, en cu_m (96 bits)
        mov esi, CU_ESC
        mul esi
        mov [cu_m], eax
        mov [cu_m+4], edx
        mov eax, edi
        mul esi
        add [cu_m+4], eax
        adc edx, 0
        mov [cu_m+8], edx
        xor esi, esi                ; EDI:ESI=el resto
        xor edi, edi
        mov edx, 96                 ; cifra a cifra (en binario): el cociente
.b:                                 ; va entrando por abajo en cu_m
        shl dword [cu_m], 1
        rcl dword [cu_m+4], 1
        rcl dword [cu_m+8], 1
        rcl esi, 1
        rcl edi, 1
        jc .resta                   ; (el resto no cabe en 64 bits: es mayor)
        cmp edi, ecx
        ja .resta
        jb .sig
        cmp esi, ebx
        jb .sig
.resta:
        sub esi, ebx
        sbb edi, ecx
        or byte [cu_m], 1
.sig:
        dec edx
        jnz .b
        shl esi, 1                  ; el doble del resto, ¿llega al divisor?
        rcl edi, 1
        jc .mas
        cmp edi, ecx
        ja .mas
        jb .nomas
        cmp esi, ebx
        jb .nomas
.mas:
        add dword [cu_m], 1
        adc dword [cu_m+4], 0
        adc dword [cu_m+8], 0
.nomas:
        mov eax, [cu_m]
        mov edx, [cu_m+4]
        mov ecx, [cu_m+8]
        ; (sigue en ev_signo)
; el final de ev_mul y ev_div: EDX:EAX sin signo, ECX=lo que pasa de 64 bits
; (tiene que ser cero), EBP=el signo en el bit de arriba, y en la pila el EBP
; de quien llamó
ev_signo:
        test ecx, ecx
        jnz .mal
        test edx, edx
        js .mal                     ; (más de 63 bits)
        test ebp, ebp
        jns .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        pop ebp
        ret
.mal:
        call ev_mal
        xor eax, eax
        xor edx, edx
        pop ebp
        ret

; EDX:EAX (sin signo) entre EBX -> EDX:EAX y ECX=el resto
div64:
        push esi
        mov esi, eax
        mov eax, edx
        xor edx, edx
        div ebx
        xchg eax, esi
        div ebx
        mov ecx, edx
        mov edx, esi
        pop esi
        ret

            align 4
pot10       dd 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000

; =============================================================================
;  Leer y escribir números
; =============================================================================
; lee un número sin signo de ESI (cifras, con coma o punto decimal) ->
; EDX:EAX, y ESI detrás; CF=1 si no hay cifras o no cabe. Los decimales que
; pasan de ocho redondean: el primero que sobra, si es 5 o más, hacia arriba
lee_num:
        push ebx
        push ecx
        push edi
        xor eax, eax
        xor edx, edx
        xor ecx, ecx                ; CH=1: ya ha salido la coma; CL=decimales
        xor edi, edi                ; bit 0: hay cifras; 1: redondear; 2: ya
.l:                                 ; se ha mirado la primera que sobra
        movzx ebx, byte [esi]
        cmp bl, ','
        je .coma
        cmp bl, '.'
        je .coma
        sub ebx, '0'
        cmp ebx, 9
        ja .fin
        or edi, 1
        test ch, ch
        jz .cifra
        cmp cl, CU_DEC
        jb .dec
        test edi, 4
        jnz .sig
        or edi, 4
        cmp ebx, 5
        jb .sig
        or edi, 2
        jmp .sig
.dec:
        inc cl
.cifra:
        call .por10
        jc .mal
        add eax, ebx
        adc edx, 0
        js .mal
.sig:
        inc esi
        jmp .l
.coma:
        test ch, ch
        jnz .fin
        mov ch, 1
        inc esi
        jmp .l
.fin:
        test edi, 1
        jz .mal
.esc:
        cmp cl, CU_DEC              ; a cienmillonésimas
        jae .red
        call .por10
        jc .mal
        inc cl
        jmp .esc
.red:
        test edi, 2
        jz .ok
        add eax, 1
        adc edx, 0
.ok:
        test edx, edx
        js .mal
        clc
        jmp .x
.mal:
        stc
.x:
        pop edi
        pop ecx
        pop ebx
        ret
; (interna) EDX:EAX por 10; CF=1 si pasaría de 63 bits
.por10:
        cmp edx, 0x0CCCCCCC
        jae .pmal
        push ebx
        push ecx
        mov ecx, edx
        mov ebx, 10
        mul ebx
        lea ecx, [ecx+ecx*4]
        lea edx, [edx+ecx*2]
        pop ecx
        pop ebx
        clc
        ret
.pmal:
        stc
        ret

; ESI=texto (acabado en 0) -> EDX:EAX; CF=1 si no es un número y nada más
; (puede llevar signo, y espacios delante y detrás). Conserva los registros
cad_a_num:
        push esi
        push ebx
        xor ebx, ebx
.sp:
        cmp byte [esi], ' '
        jne .s0
        inc esi
        jmp .sp
.s0:
        cmp byte [esi], '-'
        jne .s1
        inc ebx
        inc esi
        jmp .s2
.s1:
        cmp byte [esi], '+'
        jne .s2
        inc esi
.s2:
        call lee_num
        jc .x
.t:
        cmp byte [esi], ' '
        jne .t1
        inc esi
        jmp .t
.t1:
        cmp byte [esi], 0
        jne .bad
        test ebx, ebx
        jz .ok
        neg edx
        neg eax
        sbb edx, 0
.ok:
        clc
        jmp .x
.bad:
        stc
.x:
        pop ebx
        pop esi
        ret

; EDX:EAX (sin signo) -> sus cifras en EDI, que avanza
u64_txt:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        xor esi, esi
        mov ebx, 10
.l:
        call div64
        push ecx
        inc esi
        mov ecx, eax
        or ecx, edx
        jnz .l
.e:
        pop eax
        add al, '0'
        stosb
        dec esi
        jnz .e
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; EDX:EAX -> texto en EDI, acabado en 0, como se ve en la celda: hasta
; VER_DEC decimales, redondeados (la mitad, alejándose de cero) y sin ceros
; de más. Si no cabe en CEL_CARS se le quitan decimales y, si ni el entero
; cabe, salen almohadillas: un número nunca se ve cortado. Conserva los
; registros
num_fmt:
        pushad
        mov ebp, edi
        mov byte [nf_neg], 0
        test edx, edx
        jns .p
        neg edx
        neg eax
        sbb edx, 0
        mov byte [nf_neg], 1
.p:
        mov [nf_v], eax
        mov [nf_v+4], edx
        mov esi, VER_DEC            ; ESI=los decimales que se intentan
.otra:
        mov eax, [nf_v]             ; redondeado a esos decimales
        mov edx, [nf_v+4]
        mov ebx, CU_DEC
        sub ebx, esi
        mov ebx, [pot10+ebx*4]
        call div64
        shl ecx, 1
        cmp ecx, ebx
        jb .nr
        add eax, 1
        adc edx, 0
.nr:
        mov ebx, [pot10+esi*4]      ; la parte entera, y ECX=los decimales
        call div64
        mov edi, nf_ent
        call u64_txt
        mov byte [edi], 0
        sub edi, nf_ent             ; EDI=cifras de la parte entera
        mov edx, esi                ; EDX=decimales que quedan al quitar los
        mov ebx, 10                 ; ceros del final, y ECX sin ellos
.tz:
        test edx, edx
        jz .tzf
        mov eax, ecx
        push edx
        xor edx, edx
        div ebx
        test edx, edx
        pop edx
        jnz .tzf
        mov ecx, eax
        dec edx
        jmp .tz
.tzf:
        mov bl, [nf_neg]            ; el cero no lleva signo
        cmp byte [nf_ent], '0'
        jne .sg
        test ecx, ecx
        jnz .sg
        xor bl, bl
.sg:
        movzx eax, bl               ; ¿cabe? signo + entero + coma + decimales
        add eax, edi
        test edx, edx
        jz .cb
        lea eax, [eax+edx+1]
.cb:
        cmp eax, CEL_CARS
        jbe .cabe
        test esi, esi
        jz .no
        dec esi
        jmp .otra
.no:
        mov edi, ebp
        mov ecx, CEL_CARS
        mov al, '#'
        rep stosb
        jmp .fin
.cabe:
        mov edi, ebp
        test bl, bl
        jz .e
        mov al, '-'
        stosb
.e:
        mov esi, nf_ent
.ec:
        lodsb
        test al, al
        jz .ef
        stosb
        jmp .ec
.ef:
        test edx, edx
        jz .fin
        mov al, ','
        stosb
        mov eax, ecx                ; los decimales: EDX cifras, de atrás adelante
        mov ecx, edx
        mov esi, edx
        mov ebx, 10
.d:
        xor edx, edx
        div ebx
        add dl, '0'
        mov [edi+ecx-1], dl
        dec ecx
        jnz .d
        add edi, esi
.fin:
        mov byte [edi], 0
        popad
        ret

        section .bss
            alignb 4
ev_ptr      resd 1                  ; por dónde va la fórmula
ev_prof     resd 1                  ; fórmulas a medias (unas piden otras)
cu_m        resd 4                  ; el producto o el dividendo, de 128 bits
nf_v        resd 2                  ; num_fmt: el valor sin signo
nf_ent      resb 24                 ; y su parte entera, en texto
ev_nom      resb 12                 ; el nombre leído
ev_pila_min resd 1                  ; la pila no puede bajar de aquí (0: da igual)
cv_val      resd 2 * NCOL * NFIL    ; lo apuntado de cada celda: su valor,
cv_err      resb NCOL * NFIL        ; su error
cv_est      resb NCOL * NFIL        ; y su estado: 0 nada, 1 calculándose, 2 apuntada
ev_hondo    resb 1                  ; se ha tocado fondo en este cálculo
ev_err      resb 1                  ; 0, o EV_ERROR, EV_CIRC, EV_REF
cv_tipo     resb 1                  ; lo que era la última celda pedida
nf_neg      resb 1
        section .text
