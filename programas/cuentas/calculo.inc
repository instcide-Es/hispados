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
;  también redondeados (num_fmt), o los que diga el formato (fmt_celda).
;
;  Las fórmulas (lo que va detrás del «=»), de menos a más prioridad:
;       expresión := unión { (= | <> | < | > | <= | >=) unión }    -> 1 o 0
;       unión     := suma { & suma }                    (junta dos textos)
;       suma      := término { (+|-) término }
;       término   := potencia { (*|/) potencia }
;       potencia  := factor { ^ factor }
;       factor    := (+|-) factor | primario { % }
;       primario  := número | "texto" | celda | ( expresión ) | función
;       celda     := [$] letra [letra] [$] fila          (A1, $B$7, AB12)
;  «%» divide entre cien (=A1*21%). Las mayúsculas dan igual. Como en las
;  hojas de cálculo de siempre, -2^2 es 4 (el signo va antes que la potencia).
;
;  Un valor es un número, un texto o «vacío» (una celda sin nada). En una
;  cuenta, el vacío y el texto que no es un número valen 0. Al comparar, el
;  vacío es igual que 0 y que el texto vacío; dos textos se comparan sin
;  mirar las mayúsculas, y un número es menor que cualquier texto.
;
;  Las funciones (los argumentos, separados por «;»):
;       SUMA PROMEDIO MAX MIN CONTAR CONTARA    de rangos (A1:B7), celdas y
;                   expresiones; en los rangos solo cuentan las celdas con
;                   número (CONTARA: las que no están vacías)
;       SI(condición; si sí [; si no])          solo calcula la rama que toca
;       Y(...) O(...) NO(x) VERDADERO FALSO
;       ENTERO(x)   el entero de abajo          REDONDEAR(x; decimales)
;       ABS(x)      RAIZ(x)                     RESIDUO(a; b), con el signo de b
;       POTENCIA(x; y) y x^y: exponente entero o acabado en ,5 (con la raíz)
;       CONTAR.SI(rango; criterio)  SUMAR.SI(rango; criterio [; rango a sumar])
;                   el criterio: un número, un texto, o ">5", "<>0", "=pan"...
;       HOY()       el día de hoy, en días desde el 30/12/1899 (como las
;                   demás hojas de cálculo: el formato de fecha lo enseña)
;       CONCATENAR(a; b; ...)  LARGO(x)  MAYUSC(x)  MINUSC(x)
;       IZQUIERDA(t [; n])  DERECHA(t [; n])  EXTRAE(t; desde; n)
;       ENCONTRAR(lo buscado; t [; desde])  ESPACIOS(t)  VALOR(t)
;       TEXTO(x; "0,00")    con el formato: «0», «0,00», «#.##0,00», «0%»,
;                   «dd/mm/aaaa»
;       FECHA(año; mes; día)  AÑO(f) (o ANO)  MES(f)  DIA(f)  DIASEM(f [; 2])
;       BUSCARV(valor; rango; columna [; 0])  BUSCARH  INDICE(rango; f [; c])
;       COINCIDIR(valor; rango [; 0])
;       ESNUMERO(x)  ESTEXTO(x)  ESBLANCO(x)  SI.ERROR(x; si falla)
;       TRUNCAR(x [; d])  REDONDEAR.MAS(x; d)  REDONDEAR.MENOS(x; d)  SIGNO(x)
;       PRODUCTO  MEDIANA  VAR  DESVEST (de una muestra; hasta 512 números)
;       PAGO(tasa; plazos; capital)   la cuota de un préstamo
;       PI()  ALEATORIO()
;
;  Lo que pide a quien lo incluye:
;       NCOL, NFIL                          constantes de la hoja
;       cel_puntero     ECX=columna EDX=fila -> ESI=el texto de la celda
;                       (conserva los demás registros)
;       cel_cache       ECX=columna EDX=fila ESI=su texto (una fórmula) ->
;                       EDI=su ficha (CV_TAM octetos); conserva los demás
;       hoy_serie       -> EAX=el día de hoy, en días desde el 30/12/1899
;       azar_num        -> EAX=un número al azar, de 0 a CU_ESC - 1
;  y lo que da:
;       calc_celda      ECX=columna EDX=fila -> EDX:EAX; [cv_tipo]: 0 vacía,
;                       1 número, 2 texto (y entonces EAX=el texto, acabado
;                       en 0); [ev_err]: 0 o EV_ERROR, EV_CIRC, EV_REF
;       calc_olvida     hay que llamarla cuando cambia una celda: cada
;                       fórmula calculada se apunta, para no repetirla (sin
;                       eso, una hoja donde cada celda suma las dos de antes
;                       tardaría una eternidad)
;       [ev_pila_min]   si no es 0, la pila no baja de ahí: da error antes
;       cad_a_num       ESI=texto -> EDX:EAX; CF=1 si no es un número (ni
;                       una fecha, d/m/aaaa)
;       num_fmt         EDX:EAX -> EDI=texto como se ve en una celda sin
;                       formato, en [nf_cars] caracteres como mucho
;       fmt_celda       EDX:EAX EBX=formato (F_*) -> EDI=texto con ese formato
;       num_fijo        EDX:EAX ECX=decimales BL=1 sin ceros de más, 2 con
;                       puntos de millares -> EDI=texto (EDI queda en el 0)
;       lee_ref         ESI=texto -> ECX=columna EDX=fila AL=los «$», ESI
;                       detrás; CF=1 si no es una celda
;       [moneda_txt]    lo que va detrás de un número con formato de moneda
;  Todas las rutinas ev_* dejan el resultado en EDX:EAX y su clase en [ev_tx]
;  (0 número, 1 texto: EAX=puntero, 2 vacío), y pueden cambiar los demás
;  registros, menos EBP.
; =============================================================================
CU_ESC      equ 100000000           ; ocho decimales por dentro
CU_DEC      equ 8
VER_DEC     equ 4                   ; los que se ven, como mucho
EV_PROF     equ 400                 ; fórmulas metidas unas en otras, como mucho
EV_ERROR    equ 1                   ; «#ERROR»
EV_CIRC     equ 2                   ; «#CIRC»: la celda se pide a sí misma
EV_REF      equ 3                   ; «#REF»: una celda que no existe
EV_CADS     equ 8192                ; sitio para los textos a medio calcular
; la ficha de una fórmula, donde se apunta lo que vale (la guarda quien
; incluye esto, junto a la celda: cel_cache)
CV_GEN      equ 0                   ; dd la tanda en que se apuntó
CV_EST      equ 4                   ; db 0 nada, 1 calculándose, 2 apuntada
CV_ERR      equ 5                   ; db su error
CV_VAL      equ 6                   ; dq su valor
CV_TAM      equ 14
CAD_MAX     equ 255                 ; letras de un texto calculado, como mucho
; el formato de una celda (una palabra)
F_NEG       equ 0x0001              ; negrita
F_CUR       equ 0x0002              ; cursiva
F_ALIN      equ 0x000C              ; alineación: 0 la normal, 1 izquierda,
F_ALIN_D    equ 2                   ;   2 centro, 3 derecha
F_COLOR     equ 0x0070              ; color de la letra (de 0 a 7)
F_COLOR_D   equ 4
F_NUM       equ 0x0380              ; formato del número (FN_*)
F_NUM_D     equ 7
F_DEC       equ 0x3C00              ; decimales (de 0 a 8)
F_DEC_D     equ 10
F_BABA      equ 0x4000              ; una raya (borde) debajo de la celda
F_BDER      equ 0x8000              ; y otra a su derecha
FN_GENERAL  equ 0
FN_FIJO     equ 1                   ; con tantos decimales
FN_MIL      equ 2                   ; y con puntos de millares
FN_MONEDA   equ 3                   ; millares y la moneda detrás
FN_PORC     equ 4                   ; por cien y «%»
FN_FECHA    equ 5                   ; dd/mm/aaaa
FECHA_MAX   equ 2958465             ; el 31/12/9999

; =============================================================================
;  El valor de una celda
; =============================================================================
; el valor de una celda, desde fuera: ECX=columna EDX=fila
calc_celda:
        mov byte [ev_err], 0
        mov byte [ev_hondo], 0
        mov dword [ev_prof], 0
        mov dword [ev_cad_p], ev_cads
; (y desde dentro de una fórmula)
celda_valor:
        cmp ecx, NCOL
        jae .ref
        cmp edx, NFIL
        jae .ref
        call cel_puntero
        mov al, [esi]
        test al, al
        jz .vacia
        cmp al, '='
        je .formula
        call cad_a_num
        jc .texto
        mov byte [cv_tipo], 1
        mov byte [ev_tx], 0
        ret
.texto:
        mov byte [cv_tipo], 2
        mov byte [ev_tx], 1
        mov eax, esi
        xor edx, edx
        ret
.vacia:
        mov byte [cv_tipo], 0
        mov byte [ev_tx], 2
.cero:
        xor eax, eax
        xor edx, edx
        ret
.apuntada:
        mov byte [cv_tipo], 1
        mov byte [ev_tx], 0
        mov al, [edi+CV_ERR]
        test al, al
        jz .ap1
        call ev_pon_err
        jmp .cero
.ap1:
        mov eax, [edi+CV_VAL]
        mov edx, [edi+CV_VAL+4]
        ret
.formula:
        call cel_cache              ; EDI=la ficha donde se apunta su valor
        mov eax, [cv_gen]           ; (lo apuntado antes del último cambio no vale)
        cmp [edi+CV_GEN], eax
        jne .calc
        mov al, [edi+CV_EST]
        cmp al, 1                   ; ¿se está calculando ya? Se pide a sí misma
        je .circ
        cmp al, 2                   ; ¿calculada ya? Lo apuntado
        je .apuntada
.calc:
        cmp dword [ev_prof], EV_PROF        ; demasiadas, unas dentro de otras
        jae .honda
        cmp esp, [ev_pila_min]              ; (o la pila se acaba)
        jb .honda
        inc dword [ev_prof]
        mov eax, [cv_gen]
        mov [edi+CV_GEN], eax
        mov byte [edi+CV_EST], 1
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
        mov cl, [ev_err]
        test cl, cl
        jnz .f0
        cmp byte [ev_tx], 1         ; ha salido un texto: no se apunta (vive en
        jne .f1                     ; el sitio de los textos a medio calcular)
        mov byte [cv_tipo], 2
        mov byte [edi+CV_EST], 0
        ret
.f0:
        xor eax, eax
        xor edx, edx
.f1:
        mov byte [cv_tipo], 1       ; (una celda vacía, pedida con =A1, vale 0)
        mov byte [ev_tx], 0
        mov byte [edi+CV_EST], 0
        test ebx, ebx               ; se apunta si lo que ha salido es todo
        jnz .f2                     ; suyo: sin un error de antes y sin haber
        cmp byte [ev_hondo], 0      ; tocado fondo
        jne .f2
        mov [edi+CV_VAL], eax
        mov [edi+CV_VAL+4], edx
        mov [edi+CV_ERR], cl
        mov byte [edi+CV_EST], 2
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
        mov byte [ev_tx], 0
        jmp .cero

; olvida lo apuntado: hay que llamarla cada vez que cambia una celda. No
; recorre nada: cada ficha lleva el número de la tanda en que se apuntó, y
; con cambiar de tanda dejan de valer todas
calc_olvida:
        inc dword [cv_gen]
        jnz .x
        inc dword [cv_gen]          ; (el 0 es el de las fichas recién hechas)
.x:
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
;  Los valores: número, texto o vacío
; =============================================================================
; lo que hay en EDX:EAX, como número: el vacío y el texto que no es un número
; valen 0. Conserva los demás registros
ev_num:
        cmp byte [ev_tx], 0
        je .x
        cmp byte [ev_tx], 1
        mov byte [ev_tx], 0
        jne .cero
        push esi
        mov esi, eax
        call cad_a_num
        pop esi
        jnc .x
.cero:
        xor eax, eax
        xor edx, edx
.x:
        ret

; lo que hay en EDX:EAX, como texto -> EAX=puntero: el vacío es el texto
; vacío y el número se escribe con todos sus decimales. Conserva los demás
ev_a_cad:
        cmp byte [ev_tx], 1
        je .x
        cmp byte [ev_tx], 2
        mov byte [ev_tx], 1
        jne .num
        mov eax, ev_vacia
        xor edx, edx
        ret
.num:
        push ebx
        push ecx
        push edi
        call ev_cad_nueva
        push edi
        mov ecx, CU_DEC
        mov bl, 1
        call num_fijo
        inc edi
        call ev_cad_fin
        pop eax
        xor edx, edx
        pop edi
        pop ecx
        pop ebx
.x:
        ret

; sitio para un texto nuevo (CAD_MAX letras y el 0) -> EDI. Si no queda,
; error, y EDI apunta a un sitio de sobra. Conserva los demás registros
ev_cad_nueva:
        mov edi, [ev_cad_p]
        cmp edi, ev_cads + EV_CADS - CAD_MAX - 1
        jbe .x
        push eax
        call ev_mal
        pop eax
        mov edi, ev_cads + EV_CADS
.x:
        ret
; el texto nuevo acaba en EDI (detrás de su 0)
ev_cad_fin:
        cmp edi, ev_cads + EV_CADS
        ja .x
        mov [ev_cad_p], edi
.x:
        ret
; el texto EAX, si está en el sitio de los textos, baja a EBX (el sitio vuelve
; a estar como al empezar la expresión, con ese texto solo) -> EAX
ev_cad_baja:
        cmp eax, ev_cads
        jb .fuera
        cmp eax, ev_cads + EV_CADS
        jae .fuera
        push esi
        push edi
        mov esi, eax
        mov edi, ebx
        mov eax, ebx
.c:
        movsb
        cmp byte [edi-1], 0
        jne .c
        mov [ev_cad_p], edi
        pop edi
        pop esi
        ret
.fuera:
        mov [ev_cad_p], ebx
        ret

; junta dos textos: ESI=el de la izquierda EAX=el de la derecha -> EAX=el
; nuevo (lo que pasa de CAD_MAX letras se pierde)
ev_une:
        mov ebx, eax
        call ev_cad_nueva
        push edi
        mov ecx, CAD_MAX
.a:
        lodsb
        test al, al
        jz .b0
        stosb
        dec ecx
        jnz .a
.b0:
        mov esi, ebx
        jecxz .f
.b:
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jnz .b
.f:
        mov byte [edi], 0
        inc edi
        call ev_cad_fin
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        ret

; EAX distinto de 0 -> 1; si no, 0 (como número)
ev_bool:
        xor edx, edx
        mov byte [ev_tx], 0
        test eax, eax
        jz .x
        mov eax, CU_ESC
.x:
        ret

; AL -> en mayúscula (y may_min: en minúscula), con las letras de la página
; 437 que tienen las dos formas: ñ, é, ü, ç
may_car:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .otras
        sub al, 0x20
.x:
        ret
.otras:
        push ebx
        mov ebx, may_pares
.b:
        cmp byte [ebx], 0
        je .f
        cmp al, [ebx]
        je .es
        add ebx, 2
        jmp .b
.es:
        mov al, [ebx+1]
.f:
        pop ebx
        ret
min_car:
        cmp al, 'A'
        jb .x
        cmp al, 'Z'
        ja .otras
        add al, 0x20
.x:
        ret
.otras:
        push ebx
        mov ebx, may_pares
.b:
        cmp byte [ebx], 0
        je .f
        cmp al, [ebx+1]
        je .es
        add ebx, 2
        jmp .b
.es:
        mov al, [ebx]
.f:
        pop ebx
        ret
may_pares   db 0xA4, 0xA5, 0x82, 0x90, 0x81, 0x9A, 0x87, 0x80, 0     ; ñÑ éÉ üÜ çÇ
ev_vacia    db 0

; compara dos textos, ESI y EDI, sin mirar las mayúsculas -> EAX: -1, 0 o 1
cad_cmp:
        push esi
        push edi
.c:
        lodsb
        call may_car
        mov ah, al
        mov al, [edi]
        inc edi
        call may_car
        cmp ah, al
        jne .d
        test al, al
        jnz .c
        xor eax, eax
        jmp .x
.d:
        mov eax, 1
        ja .x
        mov eax, -1
.x:
        pop edi
        pop esi
        ret

; compara lo de la izquierda ([cmp_l], de la clase ECX) con lo que hay ahora
; en EDX:EAX -> EAX: -1, 0 o 1
ev_compara:
        movzx ebx, byte [ev_tx]
        cmp ecx, 2
        jne .a
        mov ecx, ebx                ; el vacío toma la clase del otro
        cmp ecx, 2
        je .igual
        mov dword [cmp_l], 0
        mov dword [cmp_l+4], 0
        cmp ecx, 1
        jne .a
        mov dword [cmp_l], ev_vacia
.a:
        cmp ebx, 2
        jne .b
        mov ebx, ecx
        xor eax, eax
        xor edx, edx
        cmp ebx, 1
        jne .b
        mov eax, ev_vacia
.b:
        cmp ecx, ebx
        jne .dist
        cmp ecx, 1
        je .txt
        cmp [cmp_l+4], edx          ; dos números (con signo)
        jg .may
        jl .men
        cmp [cmp_l], eax
        ja .may
        jb .men
.igual:
        xor eax, eax
        ret
.txt:
        mov esi, [cmp_l]
        mov edi, eax
        jmp cad_cmp
.dist:
        cmp ecx, 1                  ; un número es menor que un texto
        je .may
.men:
        mov eax, -1
        ret
.may:
        mov eax, 1
        ret

; EAX=cómo ha salido la comparación (-1, 0, 1) EBX=el operador (0 «=», 1 «<>»,
; 2 «<», 3 «>», 4 «<=», 5 «>=») -> EAX=1 si se cumple
ev_op:
        lea ecx, [eax+1]
        movzx eax, byte [ev_op_t+ebx]
        shr eax, cl
        and eax, 1
        ret
ev_op_t     db 010b, 101b, 001b, 100b, 011b, 110b

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

; expresión := unión { (= | <> | < | > | <= | >=) unión }
; Al acabar, el sitio de los textos queda como estaba, más el texto que haya
; salido (si ha salido uno)
ev_expr:
        push dword [ev_cad_p]
        call ev_union
.b:
        push eax
        call ev_car
        mov cl, al
        pop eax
        mov esi, [ev_ptr]
        xor ebx, ebx
        cmp cl, '='
        je .op1
        cmp cl, '<'
        je .menor
        cmp cl, '>'
        jne .fin
        mov bl, 3
        cmp byte [esi+1], '='
        jne .op1
        mov bl, 5
        jmp .op2
.menor:
        mov bl, 2
        cmp byte [esi+1], '>'
        jne .m1
        mov bl, 1
        jmp .op2
.m1:
        cmp byte [esi+1], '='
        jne .op1
        mov bl, 4
.op2:
        inc esi
.op1:
        inc esi
        mov [ev_ptr], esi
        push ebx
        movzx ecx, byte [ev_tx]
        push ecx
        push edx
        push eax
        call ev_union
        pop dword [cmp_l]
        pop dword [cmp_l+4]
        pop ecx
        call ev_compara
        pop ebx
        call ev_op
        call ev_bool
        jmp .b
.fin:
        pop ebx
        cmp byte [ev_tx], 1
        je ev_cad_baja
        mov [ev_cad_p], ebx
        ret

; unión := suma { & suma }
ev_union:
        call ev_suma
.b:
        push eax
        call ev_car
        mov cl, al
        pop eax
        cmp cl, '&'
        je .y
        ret
.y:
        inc dword [ev_ptr]
        call ev_a_cad
        push eax
        call ev_suma
        call ev_a_cad
        pop esi
        call ev_une
        jmp .b

; suma := término { (+|-) término }
ev_suma:
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
        call ev_num
        push edx
        push eax
        call ev_term
        call ev_num
        pop ebx
        pop ecx                     ; ECX:EBX=lo de la izquierda
        add eax, ebx
        adc edx, ecx
        jno .b
        jmp .desb
.menos:
        inc dword [ev_ptr]
        call ev_num
        push edx
        push eax
        call ev_term
        call ev_num
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

; término := potencia { (*|/) potencia }
ev_term:
        call ev_pot
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
        call ev_num
        push edx
        push eax
        call ev_pot
        call ev_num
        pop ebx
        pop ecx
        call ev_mul
        jmp .b
.entre:
        inc dword [ev_ptr]
        call ev_num
        push edx
        push eax
        call ev_pot
        call ev_num
        pop ebx
        pop ecx
        call ev_div
        jmp .b

; potencia := factor { ^ factor }
ev_pot:
        call ev_factor
.b:
        push eax
        call ev_car
        mov cl, al
        pop eax
        cmp cl, '^'
        je .a
        ret
.a:
        inc dword [ev_ptr]
        call ev_num
        push edx
        push eax
        call ev_factor
        call ev_num
        pop ebx
        pop ecx
        call ev_potencia
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
        call ev_num
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
        call ev_factor
        jmp ev_num
.neg:
        inc dword [ev_ptr]
        call ev_factor
        call ev_num
        neg edx
        neg eax
        sbb edx, 0
        ret

; primario := número | "texto" | celda | ( expresión ) | función
ev_primario:
        call ev_car
        cmp al, '('
        je .paren
        cmp al, '"'
        je .cadena
        cmp al, '$'
        je ev_nombre
        cmp al, '#'
        je .almo
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
.cero:
        xor eax, eax
        xor edx, edx
        mov byte [ev_tx], 0
        ret
.num:
        mov esi, [ev_ptr]
        call lee_num
        mov [ev_ptr], esi
        jc .mal
        mov byte [ev_tx], 0
        ret
.almo:
        mov esi, [ev_ptr]           ; «#REF»: lo que queda de una celda que se
        cmp dword [esi], '#REF'     ; ha eliminado
        jne .mal
        add esi, 4
        mov [ev_ptr], esi
        mov al, EV_REF
        call ev_pon_err
        jmp .cero
.cadena:
        mov esi, [ev_ptr]
        inc esi
        call ev_cad_nueva
        push edi
        mov ecx, CAD_MAX
.c:
        lodsb
        test al, al
        jz .cmal
        cmp al, '"'
        jne .cp
        cmp byte [esi], '"'         ; dos comillas seguidas son una
        jne .cf
        inc esi
.cp:
        jecxz .c
        stosb
        dec ecx
        jmp .c
.cf:
        mov byte [edi], 0
        inc edi
        call ev_cad_fin
        mov [ev_ptr], esi
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        ret
.cmal:
        dec esi                     ; sin las comillas de cerrar
        mov [ev_ptr], esi
        pop eax
        jmp .mal
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

; salta un argumento sin calcularlo (la rama de SI que no toca): hasta el
; «;» o el «)» que lo acaban. Conserva los registros
ev_salta:
        push eax
        push ecx
        push esi
        mov esi, [ev_ptr]
        xor ecx, ecx                ; paréntesis abiertos
.b:
        mov al, [esi]
        test al, al
        jz .x
        cmp al, '"'
        jne .n
.q:
        inc esi
        mov al, [esi]
        test al, al
        jz .x
        cmp al, '"'
        jne .q
        jmp .s
.n:
        cmp al, '('
        jne .n1
        inc ecx
        jmp .s
.n1:
        cmp al, ')'
        jne .n2
        test ecx, ecx
        jz .x
        dec ecx
        jmp .s
.n2:
        cmp al, ';'
        jne .s
        test ecx, ecx
        jz .x
.s:
        inc esi
        jmp .b
.x:
        mov [ev_ptr], esi
        pop esi
        pop ecx
        pop eax
        ret

; un nombre: una celda o una función
ev_nombre:
        mov esi, [ev_ptr]
        call lee_ref
        jc .fun
        mov [ev_ptr], esi
        cmp ecx, NCOL
        jae .ref
        cmp edx, NFIL
        jae .ref
        push ebp
        call celda_valor
        pop ebp
        ret
.ref:
        mov al, EV_REF
        call ev_pon_err
        jmp .cero
.fun:
        mov edi, ev_nom
        xor ecx, ecx
.l:
        mov al, [esi]
        cmp al, '.'                 ; (CONTAR.SI)
        je .pon
        cmp al, 0xA5                ; (la Ñ de AÑO, en la página 437)
        je .pon
        cmp al, 0xA4                ; (y la ñ)
        jne .l1
        mov al, 0xA5
        jmp .pon
.l1:
        and al, 0xDF
        cmp al, 'A'
        jb .f
        cmp al, 'Z'
        ja .f
.pon:
        cmp ecx, 15
        jae .s
        mov [edi+ecx], al
        inc ecx
.s:
        inc esi
        jmp .l
.f:
        mov byte [edi+ecx], 0
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
        mov byte [ev_tx], 0
        ret
%macro FUNCION 3
        db %1, 0
        dd %2
        db %3
%endmacro
ev_funciones:
        FUNCION "SUMA", ev_rangos, 0
        FUNCION "PROMEDIO", ev_rangos, 1
        FUNCION "MAX", ev_rangos, 2
        FUNCION "MIN", ev_rangos, 3
        FUNCION "CONTAR", ev_rangos, 4
        FUNCION "CONTARA", ev_rangos, 5
        FUNCION "REDONDEAR", ev_redondear, 0
        FUNCION "ABS", ev_abs, 0
        FUNCION "SI", ev_si, 0
        FUNCION "Y", ev_yo, 0
        FUNCION "O", ev_yo, 1
        FUNCION "NO", ev_no, 0
        FUNCION "VERDADERO", ev_bool, 1
        FUNCION "FALSO", ev_bool, 0
        FUNCION "ENTERO", ev_entero, 0
        FUNCION "RAIZ", ev_raiz, 0
        FUNCION "POTENCIA", ev_fpotencia, 0
        FUNCION "RESIDUO", ev_residuo, 0
        FUNCION "CONTAR.SI", ev_si_rango, 0
        FUNCION "SUMAR.SI", ev_si_rango, 1
        FUNCION "HOY", ev_hoy, 0
        FUNCION "CONCATENAR", ev_concatenar, 0
        FUNCION "LARGO", ev_largo, 0
        FUNCION "MAYUSC", ev_mayusc, 0
        FUNCION "MINUSC", ev_mayusc, 1
        FUNCION "FECHA", ev_fecha, 0
        FUNCION "AÑO", ev_parte_fecha, 0
        FUNCION "ANO", ev_parte_fecha, 0
        FUNCION "MES", ev_parte_fecha, 1
        FUNCION "DIA", ev_parte_fecha, 2
        FUNCION "DIASEM", ev_diasem, 0
        FUNCION "BUSCARV", ev_buscar, 0
        FUNCION "BUSCARH", ev_buscar, 1
        FUNCION "COINCIDIR", ev_buscar, 2
        FUNCION "INDICE", ev_indice, 0
        FUNCION "IZQUIERDA", ev_izqder, 0
        FUNCION "DERECHA", ev_izqder, 1
        FUNCION "EXTRAE", ev_extrae, 0
        FUNCION "ENCONTRAR", ev_encontrar, 0
        FUNCION "ESPACIOS", ev_espacios, 0
        FUNCION "TEXTO", ev_texto, 0
        FUNCION "VALOR", ev_valor, 0
        FUNCION "ESNUMERO", ev_es, 0
        FUNCION "ESTEXTO", ev_es, 1
        FUNCION "ESBLANCO", ev_es, 2
        FUNCION "SI.ERROR", ev_sierror, 0
        FUNCION "TRUNCAR", ev_trunca, 0
        FUNCION "REDONDEAR.MENOS", ev_trunca, 1
        FUNCION "REDONDEAR.MAS", ev_trunca, 2
        FUNCION "PRODUCTO", ev_rangos, 6
        FUNCION "MEDIANA", ev_rangos, 7
        FUNCION "VAR", ev_rangos, 8
        FUNCION "DESVEST", ev_rangos, 9
        FUNCION "PAGO", ev_pago, 0
        FUNCION "PI", ev_pi, 0
        FUNCION "ALEATORIO", ev_aleatorio, 0
        FUNCION "SIGNO", ev_signo_f, 0
        db 0

; ESI=texto -> ECX=columna EDX=fila (desde 0; pueden salirse de la hoja),
; AL=los «$» (1: el de la columna, 2: el de la fila) y ESI detrás; CF=1 si no
; es una celda (una o dos letras y la fila), y ESI como estaba
lee_ref:
        push ebx
        push edi
        push esi
        xor edi, edi
        cmp byte [esi], '$'
        jne .l
        or edi, 1
        inc esi
.l:
        movzx eax, byte [esi]
        and al, 0xDF
        sub al, 'A'
        cmp al, 25
        ja .no
        mov ecx, eax
        inc esi
        movzx eax, byte [esi]
        and al, 0xDF
        sub al, 'A'
        cmp al, 25
        ja .f
        inc ecx                     ; dos letras: AA es la 26
        imul ecx, 26
        add ecx, eax
        inc esi
.f:
        cmp byte [esi], '$'
        jne .d
        or edi, 2
        inc esi
.d:
        xor edx, edx
        xor ebx, ebx                ; cuántas cifras
.dl:
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .df
        inc ebx
        inc esi
        cmp edx, 100000             ; (que no desborde: ya es demasiado)
        jae .dl
        imul edx, 10
        add edx, eax
        jmp .dl
.df:
        test ebx, ebx
        jz .no
        dec edx                     ; la fila 1 es la 0
        mov eax, edi
        add esp, 4
        pop edi
        pop ebx
        clc
        ret
.no:
        pop esi
        pop edi
        pop ebx
        stc
        ret

; lee de la fórmula un rango (A1:B7) o una celda sola que sea un argumento
; entero (detrás, «;» o «)») -> ECX=primera columna EDX=primera fila
; ESI=última columna EDI=última fila; CF=1 si no lo es (y no gasta nada: será
; una expresión). Si se sale de la hoja, error «#REF»
ev_rango_lee:
        call ev_car
        mov esi, [ev_ptr]
        push esi
        call lee_ref
        jc .no
.e1:
        cmp byte [esi], ' '
        jne .e2
        inc esi
        jmp .e1
.e2:
        mov al, [esi]
        cmp al, ':'
        je .dos
        cmp al, ';'
        je .una
        cmp al, ')'
        je .una
.no:
        pop esi
        stc
        ret
.una:
        add esp, 4
        mov [ev_ptr], esi
        mov esi, ecx
        mov edi, edx
        jmp .val
.dos:
        add esp, 4
        inc esi
.e3:
        cmp byte [esi], ' '
        jne .e4
        inc esi
        jmp .e3
.e4:
        push ecx
        push edx
        call lee_ref
        mov [ev_ptr], esi
        mov esi, ecx
        mov edi, edx
        pop edx
        pop ecx
        jnc .ord
        call ev_mal
        jmp .nada
.ord:
        cmp ecx, esi                ; de la menor a la mayor
        jbe .o1
        xchg ecx, esi
.o1:
        cmp edx, edi
        jbe .val
        xchg edx, edi
.val:
        cmp esi, NCOL
        jae .ref
        cmp edi, NFIL
        jae .ref
        clc
        ret
.ref:
        mov al, EV_REF
        call ev_pon_err
.nada:
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        clc
        ret

; ---- SUMA, PROMEDIO, MAX, MIN, CONTAR y CONTARA: EAX=cuál (de 0 a 5) ----
%define R_MODO  dword [ebp-4]
%define R_C1    dword [ebp-8]
%define R_F1    dword [ebp-12]
%define R_C2    dword [ebp-16]
%define R_F2    dword [ebp-20]
%define R_C     dword [ebp-24]
%define R_F     dword [ebp-28]
%define R_N     dword [ebp-32]      ; cuántos números
%define R_SL    dword [ebp-36]      ; la suma
%define R_SH    dword [ebp-40]
%define R_EL    dword [ebp-44]      ; la mayor o la menor
%define R_EH    dword [ebp-48]
%define R_NA    dword [ebp-52]      ; cuántas cosas (números y textos)
%define R_LP    dword [ebp-56]      ; la lista de los números (MEDIANA, VAR, DESVEST)
EV_LISTA    equ 512                 ; números de una lista, como mucho
ev_rangos:
        push ebp
        mov ebp, esp
        sub esp, 56
        mov R_MODO, eax
        cmp eax, 7                  ; la lista va en la pila: una fórmula puede
        jb .sl                      ; pedir otra que también haga la suya
        sub esp, EV_LISTA * 8
        mov R_LP, esp
.sl:
        xor eax, eax
        mov R_N, eax
        mov R_NA, eax
        mov R_SL, eax
        mov R_SH, eax
        mov R_EL, eax
        mov R_EH, eax
        call ev_abre
.arg:
        call ev_rango_lee
        jc .expr
        cmp byte [ev_err], 0
        jne .fin
        mov R_C1, ecx
        mov R_F1, edx
        mov R_C2, esi
        mov R_F2, edi
        mov R_F, edx
.fila:
        mov eax, R_C1
        mov R_C, eax
.col:
        push dword [ev_cad_p]
        mov ecx, R_C
        mov edx, R_F
        push ebp
        call celda_valor
        pop ebp
        pop dword [ev_cad_p]
        cmp byte [ev_err], 0
        jne .fin
        cmp byte [cv_tipo], 0
        je .sig
        inc R_NA
        cmp byte [cv_tipo], 1       ; solo se suman los números
        jne .sig
        call .cuenta
        jc .fin
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
        jmp .otro
.expr:
        call ev_expr                ; una expresión: cuenta si da un número
        cmp byte [ev_err], 0
        jne .fin
        cmp byte [ev_tx], 2
        je .otro
        inc R_NA
        cmp byte [ev_tx], 0
        jne .otro
        call .cuenta
        jc .fin
.otro:
        call ev_car
        cmp al, ';'                 ; ¿otro argumento?
        jne .cierra
        inc dword [ev_ptr]
        jmp .arg
.cierra:
        call ev_cierra
.fin:
        mov byte [ev_tx], 0
        mov eax, R_SL
        mov edx, R_SH
        mov ecx, R_MODO
        test ecx, ecx
        jz .x                       ; SUMA
        cmp ecx, 1
        je .promedio
        cmp ecx, 6
        je .extremo                 ; PRODUCTO: lo que ha ido saliendo
        ja .lista
        cmp ecx, 4
        jae .contar
.extremo:
        mov eax, R_EL               ; MAX o MIN
        mov edx, R_EH
        jmp .x
.contar:
        mov eax, R_N
        cmp ecx, 4
        je .c1
        mov eax, R_NA
.c1:
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
; MEDIANA: la lista, ordenada, y el de en medio (o la media de los dos de en
; medio). VAR y DESVEST (de una muestra): la media de los cuadrados de lo que
; se aparta cada uno de la media, entre uno menos de los que son; y su raíz
.lista:
        cmp byte [ev_err], 0
        jne .lcero
        mov esi, R_LP
        mov ecx, R_N
        cmp R_MODO, 7
        jne .var
        test ecx, ecx
        jz .lmal
        mov ebx, 1                  ; por inserción
.o1:
        cmp ebx, ecx
        jae .o5
        mov eax, [esi+ebx*8]
        mov edx, [esi+ebx*8+4]
        mov edi, ebx
.o2:
        test edi, edi
        jz .o4
        cmp [esi+edi*8-4], edx
        jg .o3
        jl .o4
        cmp [esi+edi*8-8], eax
        jbe .o4
.o3:
        push eax
        mov eax, [esi+edi*8-8]
        mov [esi+edi*8], eax
        mov eax, [esi+edi*8-4]
        mov [esi+edi*8+4], eax
        pop eax
        dec edi
        jmp .o2
.o4:
        mov [esi+edi*8], eax
        mov [esi+edi*8+4], edx
        inc ebx
        jmp .o1
.o5:
        mov ebx, ecx
        shr ebx, 1
        mov eax, [esi+ebx*8]
        mov edx, [esi+ebx*8+4]
        test ecx, 1
        jnz .x
        add eax, [esi+ebx*8-8]
        adc edx, [esi+ebx*8-4]
        jo .lmal
        mov ebx, eax
        mov ecx, edx
        mov eax, 2 * CU_ESC
        xor edx, edx
        call ev_div
        jmp .x
.var:
        cmp ecx, 2
        jb .lmal
        xor eax, eax
        xor edx, edx
        xor ebx, ebx
.v1:
        add eax, [esi+ebx*8]
        adc edx, [esi+ebx*8+4]
        jo .lmal
        inc ebx
        cmp ebx, ecx
        jb .v1
        mov R_SL, eax
        mov R_SH, edx
        mov eax, R_N
        mov ecx, CU_ESC
        mul ecx
        mov ebx, R_SL
        mov ecx, R_SH
        call ev_div                 ; la media
        mov R_EL, eax
        mov R_EH, edx
        mov R_SL, 0
        mov R_SH, 0
        mov R_C, 0
.v2:
        mov esi, R_LP
        mov ecx, R_C
        mov eax, [esi+ecx*8]
        mov edx, [esi+ecx*8+4]
        sub eax, R_EL
        sbb edx, R_EH
        jo .lmal
        mov ebx, eax
        mov ecx, edx
        call ev_mul
        add R_SL, eax
        adc R_SH, edx
        jo .lmal
        inc R_C
        mov eax, R_C
        cmp eax, R_N
        jb .v2
        mov eax, R_N
        dec eax
        mov ecx, CU_ESC
        mul ecx
        mov ebx, R_SL
        mov ecx, R_SH
        call ev_div
        cmp R_MODO, 9
        jne .x
        cmp byte [ev_err], 0
        jne .x
        call raiz64
        jmp .x
.lmal:
        call ev_mal
.lcero:
        xor eax, eax
        xor edx, edx
        jmp .x
; (interna) un número más, EDX:EAX; CF=1 si la suma no cabe
.cuenta:
        inc R_N
        cmp R_MODO, 6
        jb .suma
        je .prod
        mov ecx, R_N                ; a la lista
        cmp ecx, EV_LISTA
        ja .cmal
        mov ebx, R_LP
        mov [ebx+ecx*8-8], eax
        mov [ebx+ecx*8-4], edx
        clc
        ret
.prod:
        cmp R_N, 1                  ; PRODUCTO: el primero, tal cual
        je .pon
        mov ebx, R_EL
        mov ecx, R_EH
        call ev_mul
        jmp .pon
.cmal:
        call ev_mal
        stc
        ret
.suma:
        add R_SL, eax
        adc R_SH, edx
        jno .ext
        call ev_mal
        stc
        ret
.ext:
        cmp R_N, 1                  ; el primero es el mayor y el menor
        je .pon
        cmp edx, R_EH               ; ¿mayor que el que hay? (con signo)
        jg .mayor
        jl .menor
        cmp eax, R_EL
        ja .mayor
        je .ok
.menor:
        cmp R_MODO, 3
        je .pon
        jmp .ok
.mayor:
        cmp R_MODO, 2
        jne .ok
.pon:
        mov R_EL, eax
        mov R_EH, edx
.ok:
        clc
        ret

; ---- CONTAR.SI(rango; criterio) y SUMAR.SI(rango; criterio [; rango]) ----
%define S_MODO  dword [ebp-4]       ; 0 contar, 1 sumar
%define S_C1    dword [ebp-8]
%define S_F1    dword [ebp-12]
%define S_C2    dword [ebp-16]
%define S_F2    dword [ebp-20]
%define S_SC    dword [ebp-24]      ; la esquina del rango que se suma
%define S_SF    dword [ebp-28]
%define S_OP    dword [ebp-32]      ; el operador del criterio
%define S_CT    dword [ebp-36]      ; el criterio es 0 un número, 1 un texto
%define S_VL    dword [ebp-40]      ; el número (o el puntero al texto)
%define S_VH    dword [ebp-44]
%define S_N     dword [ebp-48]
%define S_SL    dword [ebp-52]
%define S_SH    dword [ebp-56]
%define S_C     dword [ebp-60]
%define S_F     dword [ebp-64]
ev_si_rango:
        push ebp
        mov ebp, esp
        sub esp, 64
        mov S_MODO, eax
        xor eax, eax
        mov S_N, eax
        mov S_SL, eax
        mov S_SH, eax
        mov S_OP, eax
        mov S_CT, eax
        call ev_abre
        call ev_rango_lee
        jc .mal
        mov S_C1, ecx
        mov S_F1, edx
        mov S_C2, esi
        mov S_F2, edi
        mov S_SC, ecx
        mov S_SF, edx
        call ev_car
        cmp al, ';'
        jne .mal
        inc dword [ev_ptr]
        call ev_expr                ; el criterio
        cmp byte [ev_tx], 1
        jne .cnum
        mov esi, eax                ; un texto: puede llevar delante el operador
        xor ebx, ebx
        mov al, [esi]
        cmp al, '='
        jne .o1
        inc esi
        jmp .of
.o1:
        cmp al, '<'
        jne .o2
        inc esi
        mov bl, 2
        cmp byte [esi], '>'
        jne .o1b
        mov bl, 1
        inc esi
        jmp .of
.o1b:
        cmp byte [esi], '='
        jne .of
        mov bl, 4
        inc esi
        jmp .of
.o2:
        cmp al, '>'
        jne .of
        inc esi
        mov bl, 3
        cmp byte [esi], '='
        jne .of
        mov bl, 5
        inc esi
.of:
        mov S_OP, ebx
        call cad_a_num              ; lo que queda, ¿es un número?
        jnc .cnum2
        mov S_CT, 1
        mov S_VL, esi
        jmp .c3
.cnum:
        call ev_num
.cnum2:
        mov S_VL, eax
        mov S_VH, edx
.c3:
        cmp S_MODO, 1
        jne .cie
        call ev_car
        cmp al, ';'
        jne .cie
        inc dword [ev_ptr]
        call ev_rango_lee
        jc .mal
        mov S_SC, ecx
        mov S_SF, edx
.cie:
        call ev_cierra
        cmp byte [ev_err], 0
        jne .fin
        mov eax, S_F1
        mov S_F, eax
.fila:
        mov eax, S_C1
        mov S_C, eax
.col:
        push dword [ev_cad_p]
        mov ecx, S_C
        mov edx, S_F
        push ebp
        call celda_valor
        pop ebp
        cmp byte [ev_err], 0
        jne .finp
        movzx ecx, byte [cv_tipo]
        cmp S_CT, 0
        jne .ctx
        cmp ecx, 1
        jne .otro
        cmp edx, S_VH               ; número con número
        jg .may
        jl .men
        cmp eax, S_VL
        ja .may
        jb .men
        xor eax, eax
        jmp .ap
.may:
        mov eax, 1
        jmp .ap
.men:
        mov eax, -1
        jmp .ap
.ctx:
        cmp ecx, 2
        jne .otro
        mov esi, eax                ; texto con texto
        mov edi, S_VL
        call cad_cmp
.ap:
        mov ebx, S_OP
        call ev_op
        jmp .dec
.otro:
        xor eax, eax                ; de otra clase: solo cumple «distinto de»
        cmp S_OP, 1
        jne .dec
        inc eax
.dec:
        test eax, eax
        jz .sig
        cmp S_MODO, 0
        jne .sum
        inc S_N
        jmp .sig
.sum:
        mov ecx, S_C                ; la celda que hace pareja con esta
        sub ecx, S_C1
        add ecx, S_SC
        mov edx, S_F
        sub edx, S_F1
        add edx, S_SF
        cmp ecx, NCOL
        jae .sig
        cmp edx, NFIL
        jae .sig
        push ebp
        call celda_valor
        pop ebp
        cmp byte [ev_err], 0
        jne .finp
        cmp byte [cv_tipo], 1
        jne .sig
        add S_SL, eax
        adc S_SH, edx
        jno .sig
        call ev_mal
        jmp .finp
.sig:
        pop dword [ev_cad_p]
        mov eax, S_C
        inc eax
        mov S_C, eax
        cmp eax, S_C2
        jbe .col
        mov eax, S_F
        inc eax
        mov S_F, eax
        cmp eax, S_F2
        jbe .fila
        jmp .fin
.finp:
        pop dword [ev_cad_p]
        jmp .fin
.mal:
        call ev_mal
.fin:
        mov byte [ev_tx], 0
        mov eax, S_SL
        mov edx, S_SH
        cmp S_MODO, 0
        jne .x
        mov eax, S_N
        mov ecx, CU_ESC
        mul ecx
.x:
        mov esp, ebp
        pop ebp
        ret

; ---- un argumento, como número: (expresión) ----
ev_arg1:
        call ev_abre
        call ev_expr
        call ev_num
        jmp ev_cierra
; ---- dos: (a; b) -> ECX:EBX=a EDX:EAX=b ----
ev_arg2:
        call ev_abre
        call ev_expr
        call ev_num
        push edx
        push eax
        call ev_car
        cmp al, ';'
        jne .mal
        inc dword [ev_ptr]
        call ev_expr
        call ev_num
        call ev_cierra
        pop ebx
        pop ecx
        ret
.mal:
        call ev_mal
        pop ebx
        pop ecx
        xor eax, eax
        xor edx, edx
        ret

; ---- SI(condición; si sí [; si no]): la rama que no toca no se calcula ----
ev_si:
        call ev_abre
        call ev_expr
        call ev_num
        or eax, edx
        push eax                    ; distinto de 0: se cumple
        call ev_car
        cmp al, ';'
        jne .mal
        inc dword [ev_ptr]
        pop eax
        test eax, eax
        jz .no
        call ev_expr
        push eax
        call ev_car
        cmp al, ';'
        pop eax
        jne ev_cierra
        inc dword [ev_ptr]
        call ev_salta
        jmp ev_cierra
.no:
        call ev_salta
        call ev_car
        cmp al, ';'
        je .sino
        xor eax, eax                ; sin «si no»: 0
        xor edx, edx
        mov byte [ev_tx], 0
        jmp ev_cierra
.sino:
        inc dword [ev_ptr]
        call ev_expr
        jmp ev_cierra
.mal:
        pop eax
        call ev_mal
        xor eax, eax
        xor edx, edx
        mov byte [ev_tx], 0
        ret

; ---- Y(a; b; ...) (EAX=0) y O(a; b; ...) (EAX=1) ----
ev_yo:
        push ebp
        mov ebp, esp
        sub esp, 8
        mov [ebp-4], eax            ; cuál
        xor eax, 1
        mov [ebp-8], eax            ; lo que va saliendo: Y empieza en 1, O en 0
        call ev_abre
.a:
        call ev_expr
        call ev_num
        or eax, edx
        setnz al
        movzx eax, al
        cmp eax, [ebp-4]            ; a Y la cambia un 0; a O, un 1
        jne .s
        mov [ebp-8], eax
.s:
        call ev_car
        cmp al, ';'
        jne .f
        inc dword [ev_ptr]
        jmp .a
.f:
        call ev_cierra
        mov eax, [ebp-8]
        mov esp, ebp
        pop ebp
        jmp ev_bool
; ---- NO(x) ----
ev_no:
        call ev_arg1
        or eax, edx
        setz al
        movzx eax, al
        jmp ev_bool

; ---- HOY() ----
ev_hoy:
        call ev_abre
        call ev_cierra
        call hoy_serie
        mov ecx, CU_ESC
        mul ecx
        mov byte [ev_tx], 0
        ret

; ---- CONCATENAR(a; b; ...) ----
ev_concatenar:
        call ev_abre
        call ev_expr
        call ev_a_cad
.b:
        push eax
        call ev_car
        cmp al, ';'
        pop eax
        jne ev_cierra
        inc dword [ev_ptr]
        push eax
        call ev_expr
        call ev_a_cad
        pop esi
        call ev_une
        jmp .b
; ---- LARGO(x): cuántas letras ----
ev_largo:
        call ev_abre
        call ev_expr
        call ev_a_cad
        xor ecx, ecx
.l:
        cmp byte [eax+ecx], 0
        je .f
        inc ecx
        jmp .l
.f:
        mov eax, ecx
        mov ecx, CU_ESC
        mul ecx
        mov byte [ev_tx], 0
        jmp ev_cierra
; ---- MAYUSC(x) (EAX=0) y MINUSC(x) (EAX=1) ----
ev_mayusc:
        push eax
        call ev_abre
        call ev_expr
        call ev_a_cad
        mov esi, eax
        call ev_cad_nueva
        pop ebx
        push edi
.c:
        lodsb
        test ebx, ebx
        jnz .min
        call may_car
        jmp .p
.min:
        call min_car
.p:
        stosb
        test al, al
        jnz .c
        call ev_cad_fin
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        jmp ev_cierra

; ---- ENTERO(x): el entero de abajo (ENTERO(-2,5) es -3) ----
ev_entero:
        call ev_arg1
        mov esi, edx                ; (el signo)
        call abs64
        mov ebx, CU_ESC
        call div64
        test esi, esi
        jns .p
        test ecx, ecx
        jz .p
        add eax, 1
        adc edx, 0
.p:
        call por_esc
        jc .desb
        test esi, esi
        jns .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret
.desb:
        call ev_mal
        xor eax, eax
        xor edx, edx
        ret
; EDX:EAX (sin signo) por 10^8 -> EDX:EAX; CF=1 si no cabe en 63 bits
por_esc:
        push ebx
        push ecx
        mov ecx, edx
        mov ebx, CU_ESC
        mul ebx
        xchg eax, ecx               ; ECX=lo de abajo; ahora, lo de arriba
        push edx
        mul ebx
        pop ebx
        add eax, ebx
        adc edx, 0
        jnz .mal
        test eax, eax
        js .mal
        mov edx, eax
        mov eax, ecx
        pop ecx
        pop ebx
        clc
        ret
.mal:
        pop ecx
        pop ebx
        stc
        ret

; ---- RAIZ(x) ----
ev_raiz:
        call ev_arg1
        test edx, edx
        jns raiz64
        call ev_mal                 ; de un número negativo no hay
        xor eax, eax
        xor edx, edx
        ret
; la raíz cuadrada de EDX:EAX (no negativo), redondeada a la cienmillonésima
; más cercana: la raíz entera de x por 10^8 (96 bits), cifra a cifra (en
; binario), y uno más si lo que sobra pasa de la mitad
raiz64:
        push ebp
        mov edi, edx
        mov esi, CU_ESC
        mul esi
        mov [cu_m], eax
        mov [cu_m+4], edx
        mov eax, edi
        mul esi
        add [cu_m+4], eax
        adc edx, 0
        mov [cu_m+8], edx
        xor eax, eax                ; EDX:EAX=la raíz
        xor edx, edx
        xor esi, esi                ; EDI:ESI=lo que sobra
        xor edi, edi
        mov ebp, 48
.b:
        shl dword [cu_m], 1         ; dos cifras más del número
        rcl dword [cu_m+4], 1
        rcl dword [cu_m+8], 1
        rcl esi, 1
        rcl edi, 1
        shl dword [cu_m], 1
        rcl dword [cu_m+4], 1
        rcl dword [cu_m+8], 1
        rcl esi, 1
        rcl edi, 1
        shl eax, 1                  ; y una de la raíz: ¿cabe el doble más uno?
        rcl edx, 1
        mov ebx, eax
        mov ecx, edx
        shl ebx, 1
        rcl ecx, 1
        or ebx, 1
        cmp edi, ecx
        ja .r
        jb .s
        cmp esi, ebx
        jb .s
.r:
        sub esi, ebx
        sbb edi, ecx
        or eax, 1
.s:
        dec ebp
        jnz .b
        cmp edi, edx                ; lo que sobra, ¿pasa de la raíz? Uno más
        ja .mas
        jb .x
        cmp esi, eax
        jbe .x
.mas:
        add eax, 1
        adc edx, 0
.x:
        pop ebp
        ret

; ---- POTENCIA(x; y) y x^y: ECX:EBX=la base EDX:EAX=el exponente ----
; El exponente tiene que ser entero o acabar en ,5. Se hace con productos
; (elevando al cuadrado: pocos, aunque el exponente sea grande), cada uno
; redondeado; el medio, con la raíz; y el negativo, con una división al final
%define P_BL    dword [ebp-4]       ; la base, que se va elevando al cuadrado
%define P_BH    dword [ebp-8]
%define P_RL    dword [ebp-12]      ; lo que va saliendo
%define P_RH    dword [ebp-16]
%define P_N     dword [ebp-20]      ; lo que queda del exponente
%define P_NEG   dword [ebp-24]
%define P_MED   dword [ebp-28]      ; acaba en ,5
%define P_OL    dword [ebp-32]      ; la base, como vino
%define P_OH    dword [ebp-36]
ev_fpotencia:
        call ev_arg2
ev_potencia:
        push ebp
        mov ebp, esp
        sub esp, 36
        mov P_BL, ebx
        mov P_BH, ecx
        mov P_OL, ebx
        mov P_OH, ecx
        mov P_NEG, edx
        call abs64
        mov ebx, CU_ESC
        call div64                  ; EDX:EAX=la parte entera ECX=el resto
        test edx, edx
        jnz .mal
        mov P_N, eax
        mov P_MED, 0
        test ecx, ecx
        jz .ent
        cmp ecx, CU_ESC / 2
        jne .mal
        mov P_MED, 1
.ent:
        mov P_RL, CU_ESC
        mov P_RH, 0
.b:
        cmp P_N, 0
        je .fin
        test P_N, 1
        jz .c
        mov ebx, P_RL
        mov ecx, P_RH
        mov eax, P_BL
        mov edx, P_BH
        call ev_mul
        mov P_RL, eax
        mov P_RH, edx
.c:
        shr P_N, 1
        jz .fin
        mov ebx, P_BL
        mov ecx, P_BH
        mov eax, ebx
        mov edx, ecx
        call ev_mul
        mov P_BL, eax
        mov P_BH, edx
        jmp .b
.fin:
        cmp P_MED, 0
        je .f1
        mov eax, P_OL
        mov edx, P_OH
        test edx, edx
        js .mal
        call raiz64
        mov ebx, P_RL
        mov ecx, P_RH
        call ev_mul
        mov P_RL, eax
        mov P_RH, edx
.f1:
        mov eax, P_RL
        mov edx, P_RH
        cmp P_NEG, 0
        jge .x
        mov ebx, CU_ESC             ; exponente negativo: uno entre eso
        xor ecx, ecx
        call ev_div
        jmp .x
.mal:
        call ev_mal
        xor eax, eax
        xor edx, edx
.x:
        mov byte [ev_tx], 0
        mov esp, ebp
        pop ebp
        ret

; ---- RESIDUO(a; b): lo que sobra de a entre b, con el signo de b ----
ev_residuo:
        call ev_arg2                ; ECX:EBX=a EDX:EAX=b
        mov esi, eax
        or esi, edx
        jz .mal
        push ebp
        mov ebp, edx                ; el signo de b
        push ecx                    ; y el de a
        call abs64
        xchg eax, ebx
        xchg edx, ecx
        call abs64                  ; EDX:EAX=|a| ECX:EBX=|b|
        call mod64
        pop esi
        mov edi, eax
        or edi, edx
        jz .x
        xor esi, ebp                ; de distinto signo: lo que falta para b
        jns .mismo
        sub ebx, eax
        sbb ecx, edx
        mov eax, ebx
        mov edx, ecx
.mismo:
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
        ret
; EDX:EAX entre ECX:EBX (sin signo, y el divisor no es 0) -> EDX:EAX=el resto
mod64:
        push esi
        push edi
        push ebp
        xor esi, esi
        xor edi, edi
        mov ebp, 64
.b:
        shl eax, 1
        rcl edx, 1
        rcl esi, 1
        rcl edi, 1
        cmp edi, ecx
        ja .r
        jb .s
        cmp esi, ebx
        jb .s
.r:
        sub esi, ebx
        sbb edi, ecx
.s:
        dec ebp
        jnz .b
        mov eax, esi
        mov edx, edi
        pop ebp
        pop edi
        pop esi
        ret

; =============================================================================
;  Más funciones: de fecha, de texto, de búsqueda y de cuentas
; =============================================================================
; gasta un «;», si viene: CF=0 si venía. Conserva los registros
ev_puntoycoma:
        push eax
        call ev_car
        cmp al, ';'
        jne .no
        inc dword [ev_ptr]
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret
; una expresión, como número entero -> EAX (con signo; sin los decimales)
ev_ent:
        call ev_expr
        call ev_num
; EDX:EAX -> EAX=su parte entera; error si no cabe en 31 bits
ent64:
        push ebx
        push ecx
        push esi
        mov esi, edx
        call abs64
        mov ebx, CU_ESC
        call div64
        test edx, edx
        jnz .mal
        test eax, eax
        js .mal
        test esi, esi
        jns .x
        neg eax
.x:
        pop esi
        pop ecx
        pop ebx
        ret
.mal:
        call ev_mal
        xor eax, eax
        jmp .x
; error, y cero
ev_cero_mal:
        call ev_mal
        xor eax, eax
        xor edx, edx
        mov byte [ev_tx], 0
        ret
; EAX (sin signo) -> ese entero como número
ev_de_ent:
        mov ecx, CU_ESC
        mul ecx
        mov byte [ev_tx], 0
        ret

; ---- PI() ----
ev_pi:
        call ev_abre
        call ev_cierra
        mov eax, 314159265
        xor edx, edx
        mov byte [ev_tx], 0
        ret
; ---- ALEATORIO(): un número de 0 a 1 (sin llegar) ----
ev_aleatorio:
        call ev_abre
        call ev_cierra
        call azar_num
        xor edx, edx
        mov byte [ev_tx], 0
        ret
; ---- SIGNO(x): -1, 0 o 1 ----
ev_signo_f:
        call ev_arg1
        mov ecx, eax
        or ecx, edx
        jz .x
        test edx, edx
        mov eax, CU_ESC
        mov edx, 0
        jns .x
        mov eax, -CU_ESC
        mov edx, -1
.x:
        ret

; ---- TRUNCAR(x [; decimales]) (EAX=0), REDONDEAR.MENOS(x; d) (1: hacia el
;      cero, como TRUNCAR) y REDONDEAR.MAS(x; d) (2: alejándose del cero) ----
ev_trunca:
        push eax
        call ev_abre
        call ev_expr
        call ev_num
        push edx
        push eax
        call ev_puntoycoma
        jc .sin
        call ev_ent
        jmp .con
.sin:
        cmp dword [esp+8], 0        ; solo TRUNCAR puede ir sin los decimales
        jne .mal
        xor eax, eax
.con:
        call ev_cierra
        cmp eax, CU_DEC
        ja .mal
        mov ebx, CU_DEC
        sub ebx, eax
        mov ebx, [pot10+ebx*4]
        pop eax
        pop edx
        pop edi                     ; EDI=cuál
        mov esi, edx                ; (el signo)
        call abs64
        call div64
        cmp edi, 2
        jne .r
        test ecx, ecx               ; sobra algo: uno más
        jz .r
        add eax, 1
        adc edx, 0
.r:
        mov ecx, edx
        mul ebx
        mov edi, eax
        mov eax, ecx
        mov ecx, edx
        mul ebx
        add eax, ecx
        mov edx, eax
        mov eax, edi
        js ev_cero_mal
        mov byte [ev_tx], 0
        test esi, esi
        jns .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret
.mal:
        add esp, 12
        jmp ev_cero_mal

; ---- PAGO(tasa; plazos; capital): la cuota de un préstamo (sale negativa,
;      como en las demás hojas: es dinero que se paga) ----
; capital x tasa / (1 - (1 + tasa)^-plazos); sin interés, capital / plazos
ev_pago:
        call ev_abre
        call ev_expr
        call ev_num
        push edx
        push eax
        call ev_puntoycoma
        jc .mal1
        call ev_expr
        call ev_num
        push edx
        push eax
        call ev_puntoycoma
        jc .mal2
        call ev_expr
        call ev_num
        call ev_cierra
        push edx
        push eax
        push ebp
        mov ebp, esp                ; [ebp+4]=capital +12=plazos +20=tasa
        mov eax, [ebp+20]
        or eax, [ebp+24]
        jz .cero
        mov ebx, [ebp+4]
        mov ecx, [ebp+8]
        mov eax, [ebp+20]
        mov edx, [ebp+24]
        call ev_mul
        push edx
        push eax                    ; [ebp-8]=capital x tasa
        mov ebx, [ebp+20]
        mov ecx, [ebp+24]
        add ebx, CU_ESC
        adc ecx, 0
        jno .p1
        call ev_mal
.p1:
        mov eax, [ebp+12]
        mov edx, [ebp+16]
        neg edx
        neg eax
        sbb edx, 0
        call ev_potencia
        mov ebx, CU_ESC
        xor ecx, ecx
        sub ebx, eax
        sbb ecx, edx
        mov eax, ebx
        mov edx, ecx
        mov ebx, [ebp-8]
        mov ecx, [ebp-4]
        call ev_div
        jmp .neg
.cero:
        mov ebx, [ebp+4]
        mov ecx, [ebp+8]
        mov eax, [ebp+12]
        mov edx, [ebp+16]
        call ev_div
.neg:
        neg edx
        neg eax
        sbb edx, 0
        mov esp, ebp
        pop ebp
        add esp, 24
        mov byte [ev_tx], 0
        ret
.mal2:
        add esp, 8
.mal1:
        add esp, 8
        jmp ev_cero_mal

; ---- FECHA(año; mes; día) ----
ev_fecha:
        call ev_abre
        call ev_ent
        push eax
        call ev_puntoycoma
        jc .mal1
        call ev_ent
        push eax
        call ev_puntoycoma
        jc .mal2
        call ev_ent
        call ev_cierra
        mov ebx, eax
        pop ecx
        pop eax
        cmp eax, 1900
        jb ev_cero_mal
        cmp eax, 9999
        ja ev_cero_mal
        call fecha_serie
        jc ev_cero_mal
        jmp ev_de_ent
.mal2:
        add esp, 4
.mal1:
        add esp, 4
        jmp ev_cero_mal
; (interna) el argumento, como día -> EAX (error si no es una fecha)
ev_dia_arg:
        call ev_arg1
ev_a_dia:
        test edx, edx
        js .mal
        mov ebx, CU_ESC
        call div64
        test edx, edx
        jnz .mal
        cmp eax, FECHA_MAX
        ja .mal
        clc
        ret
.mal:
        call ev_mal
        xor eax, eax
        stc
        ret
; ---- AÑO(fecha) (EAX=0), MES(fecha) (1) y DIA(fecha) (2) ----
ev_parte_fecha:
        push eax
        call ev_dia_arg
        pop edi
        jc ev_cero_mal
        call serie_fecha            ; EAX=año ECX=mes EBX=día
        cmp edi, 1
        jb ev_de_ent
        mov eax, ecx
        je ev_de_ent
        mov eax, ebx
        jmp ev_de_ent
; ---- DIASEM(fecha [; tipo]): tipo 1 (o nada), del 1 domingo al 7 sábado;
;      tipo 2, del 1 lunes al 7 domingo ----
ev_diasem:
        call ev_abre
        call ev_expr
        call ev_num
        push edx
        push eax
        mov eax, 1
        call ev_puntoycoma
        jc .t
        call ev_ent
.t:
        call ev_cierra
        mov edi, eax
        pop eax
        pop edx
        call ev_a_dia
        jc ev_cero_mal
        add eax, 6                  ; (el día 1, el 31/12/1899, fue domingo)
        cmp edi, 1
        je .c
        dec eax
        cmp edi, 2
        jne ev_cero_mal
.c:
        xor edx, edx
        mov ecx, 7
        div ecx
        lea eax, [edx+1]
        jmp ev_de_ent

; ---- ESNUMERO(x) (EAX=0), ESTEXTO(x) (1) y ESBLANCO(x) (2) ----
ev_es:
        push eax
        call ev_abre
        call ev_expr
        call ev_cierra
        pop ecx
        movzx eax, byte [ev_tx]
        cmp eax, ecx
        sete al
        movzx eax, al
        jmp ev_bool
; ---- SI.ERROR(x; si falla): x, o lo otro si x da error ----
ev_sierror:
        call ev_abre
        cmp byte [ev_err], 0        ; (si ya había un error, ese manda)
        jne .normal
        push dword [ev_ptr]
        push dword [ev_cad_p]
        call ev_expr
        cmp byte [ev_err], 0
        jne .falla
        add esp, 8
        jmp .n1
.normal:
        call ev_expr
.n1:
        call ev_puntoycoma
        jc .mal
        call ev_salta
        jmp ev_cierra
.falla:
        mov byte [ev_err], 0        ; como si no se hubiera calculado: se salta
        pop dword [ev_cad_p]
        pop dword [ev_ptr]
        call ev_salta
        call ev_puntoycoma
        jc ev_cero_mal
        call ev_expr
        jmp ev_cierra
.mal:
        jmp ev_cero_mal

; ---- los textos ----
; (interna) un texto nuevo con EAX letras desde ESI -> EAX, como texto
ev_subcad:
        mov ecx, eax
        call ev_cad_nueva
        push edi
        rep movsb
        mov byte [edi], 0
        inc edi
        call ev_cad_fin
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        ret
; (interna) ESI=texto -> ECX=sus letras
ev_cad_largo:
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .x
        inc ecx
        jmp .l
.x:
        ret
; ---- IZQUIERDA(texto [; letras]) (EAX=0) y DERECHA(texto [; letras]) (1) ----
ev_izqder:
        push eax
        call ev_abre
        call ev_expr
        call ev_a_cad
        push eax
        mov eax, 1
        call ev_puntoycoma
        jc .n
        call ev_ent
.n:
        call ev_cierra
        pop esi
        pop ebx
        test eax, eax
        js ev_cero_mal
        call ev_cad_largo
        cmp eax, ecx
        jbe .a
        mov eax, ecx
.a:
        test ebx, ebx
        jz ev_subcad
        add esi, ecx
        sub esi, eax
        jmp ev_subcad
; ---- EXTRAE(texto; desde; letras): desde la letra «desde» (la primera es la 1) ----
ev_extrae:
        call ev_abre
        call ev_expr
        call ev_a_cad
        push eax
        call ev_puntoycoma
        jc .mal1
        call ev_ent
        push eax
        call ev_puntoycoma
        jc .mal2
        call ev_ent
        call ev_cierra
        pop ebx                     ; EBX=desde EAX=letras
        pop esi
        test eax, eax
        js ev_cero_mal
        dec ebx
        js ev_cero_mal
        call ev_cad_largo
        sub ecx, ebx                ; lo que queda desde ahí
        jns .a
        xor ecx, ecx
        xor ebx, ebx
        xor eax, eax
.a:
        cmp eax, ecx
        jbe .b
        mov eax, ecx
.b:
        add esi, ebx
        jmp ev_subcad
.mal2:
        add esp, 4
.mal1:
        add esp, 4
        jmp ev_cero_mal
; ---- ENCONTRAR(lo que se busca; texto [; desde]): en qué letra está (mirando
;      las mayúsculas); si no está, error ----
ev_encontrar:
        call ev_abre
        call ev_expr
        call ev_a_cad
        push eax
        call ev_puntoycoma
        jc .mal1
        call ev_expr
        call ev_a_cad
        push eax
        mov eax, 1
        call ev_puntoycoma
        jc .n
        call ev_ent
.n:
        call ev_cierra
        pop esi                     ; ESI=el texto
        pop edi                     ; EDI=lo que se busca
        mov ebx, eax
        dec ebx
        js ev_cero_mal
        call ev_cad_largo
        cmp ebx, ecx
        ja ev_cero_mal
.b:
        xor ecx, ecx                ; ¿está en la letra EBX?
.c:
        mov al, [edi+ecx]
        test al, al
        jz .esta
        cmp al, [esi+ebx]
        jne .sig
        inc ecx
        inc ebx
        jmp .c
.sig:
        sub ebx, ecx
        cmp byte [esi+ebx], 0
        je ev_cero_mal
        inc ebx
        jmp .b
.esta:
        sub ebx, ecx
        lea eax, [ebx+1]
        jmp ev_de_ent
.mal1:
        add esp, 4
        jmp ev_cero_mal
; ---- ESPACIOS(texto): sin espacios delante ni detrás, y uno solo entre palabras ----
ev_espacios:
        call ev_abre
        call ev_expr
        call ev_a_cad
        mov esi, eax
        call ev_cad_nueva
        push edi
        xor ecx, ecx                ; 1: hay un espacio pendiente
.c:
        lodsb
        test al, al
        jz .f
        cmp al, ' '
        jne .l
        cmp edi, [esp]              ; (delante de todo, no)
        je .c
        mov ecx, 1
        jmp .c
.l:
        jecxz .l1
        mov byte [edi], ' '
        inc edi
        xor ecx, ecx
.l1:
        stosb
        jmp .c
.f:
        mov byte [edi], 0
        inc edi
        call ev_cad_fin
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        jmp ev_cierra
; ---- VALOR(texto): el número que es ese texto; si no lo es, error ----
ev_valor:
        call ev_abre
        call ev_expr
        cmp byte [ev_tx], 1
        jne .num
        mov esi, eax
        call cad_a_num
        mov byte [ev_tx], 0
        jnc ev_cierra
        call ev_mal
        xor eax, eax
        xor edx, edx
        jmp ev_cierra
.num:
        call ev_num
        jmp ev_cierra
; ---- TEXTO(valor; formato): el número, escrito con ese formato. El formato:
;      «0», «0,00» (los decimales), «#.##0,00» (con puntos de millares),
;      «0%» (por cien) o «dd/mm/aaaa» (una fecha) ----
ev_texto:
        call ev_abre
        call ev_expr
        call ev_num
        push edx
        push eax
        call ev_puntoycoma
        jc .mal1
        call ev_expr
        call ev_a_cad
        call ev_cierra
        mov esi, eax
        mov al, [esi]
        call may_car
        cmp al, 'D'
        je .fecha
        xor ecx, ecx                ; ECX=decimales
        xor ebx, ebx                ; BL=2: millares; BH=1: por cien; EDI=1: tras la coma
        xor edi, edi
.l:
        lodsb
        test al, al
        jz .lf
        cmp al, '%'
        jne .l1
        mov bh, 1
        jmp .l
.l1:
        cmp al, ','
        jne .l2
        mov edi, 1
        jmp .l
.l2:
        test edi, edi
        jnz .l3
        cmp al, '.'
        jne .l
        mov bl, 2
        jmp .l
.l3:
        cmp ecx, CU_DEC
        jae .l
        inc ecx
        jmp .l
.lf:
        pop eax
        pop edx
        test bh, bh
        jz .n
        mov esi, edx                ; por cien, si cabe
        call abs64
        cmp edx, 0x0147AE14
        jae ev_cero_mal
        push ecx
        push ebx
        mov ecx, edx
        mov ebx, 100
        mul ebx
        imul ecx, 100
        add edx, ecx
        pop ebx
        pop ecx
        test esi, esi
        jns .n
        neg edx
        neg eax
        sbb edx, 0
.n:
        call ev_cad_nueva
        push edi
        push ebx
        mov bh, 0
        call num_fijo
        pop ebx
        test bh, bh
        jz .cierra
        mov byte [edi], '%'
        inc edi
.cierra:
        mov byte [edi], 0
        inc edi
        call ev_cad_fin
        pop eax
        xor edx, edx
        mov byte [ev_tx], 1
        ret
.fecha:
        pop eax
        pop edx
        call ev_a_dia
        jc ev_cero_mal
        call ev_cad_nueva
        push edi
        call fecha_txt
        jmp .cierra
.mal1:
        add esp, 8
        jmp ev_cero_mal

; ---- BUSCARV(valor; rango; columna [; ordenado]) (EAX=0): busca el valor en
;      la primera columna del rango y da lo que hay en esa fila, en la
;      columna que se dice (la 1 es la primera). BUSCARH (1): lo mismo, por
;      filas. COINCIDIR(valor; rango [; tipo]) (2): en qué puesto del rango
;      (una fila o una columna) está. Con «ordenado» 0 (o tipo 0) tiene que
;      estar tal cual; si no, vale el último que no pase del valor (el rango
;      tiene que estar ordenado de menor a mayor). Si no está, error ----
%define L_MODO  dword [ebp-4]
%define L_VT    dword [ebp-8]       ; lo que se busca: su clase
%define L_VL    dword [ebp-12]
%define L_VH    dword [ebp-16]
%define L_C1    dword [ebp-20]
%define L_F1    dword [ebp-24]
%define L_C2    dword [ebp-28]
%define L_F2    dword [ebp-32]
%define L_IDX   dword [ebp-36]      ; la columna (o la fila) que se pide
%define L_EX    dword [ebp-40]      ; 1: tiene que estar tal cual
%define L_I     dword [ebp-44]
%define L_ENC   dword [ebp-48]      ; dónde está (-1: no está)
%define L_N     dword [ebp-52]      ; cuántas celdas se miran
%define L_V     dword [ebp-56]      ; 1: se mira hacia abajo; 0: hacia la derecha
ev_buscar:
        push ebp
        mov ebp, esp
        sub esp, 56
        mov L_MODO, eax
        mov L_EX, 0
        mov L_IDX, 1
        mov L_ENC, -1
        call ev_abre
        call ev_expr
        movzx ecx, byte [ev_tx]
        mov L_VT, ecx
        mov L_VL, eax
        mov L_VH, edx
        call ev_puntoycoma
        jc .mal
        call ev_rango_lee
        jc .mal
        mov L_C1, ecx
        mov L_F1, edx
        mov L_C2, esi
        mov L_F2, edi
        cmp L_MODO, 2
        je .tipo
        call ev_puntoycoma
        jc .mal
        call ev_ent
        mov L_IDX, eax
        call ev_puntoycoma
        jc .cie
        call ev_expr
        call ev_num
        or eax, edx
        jnz .cie
        mov L_EX, 1
        jmp .cie
.tipo:
        call ev_puntoycoma
        jc .cie
        call ev_ent
        cmp eax, 1
        je .cie
        test eax, eax
        jnz .mal
        mov L_EX, 1
.cie:
        call ev_cierra
        cmp byte [ev_err], 0
        jne .cero
        mov eax, L_F2               ; hacia dónde se mira, y cuántas celdas
        sub eax, L_F1
        mov ebx, L_C2
        sub ebx, L_C1               ; EAX=filas-1 EBX=columnas-1
        mov ecx, 1
        cmp L_MODO, 0
        je .dir
        xor ecx, ecx
        cmp L_MODO, 1
        je .dir
        test ebx, ebx               ; COINCIDIR: según sea el rango
        jnz .dir
        inc ecx
.dir:
        mov L_V, ecx
        test ecx, ecx
        jnz .d1
        xchg eax, ebx               ; (EAX=lo que se recorre EBX=lo otro)
.d1:
        inc eax
        mov L_N, eax
        mov eax, L_IDX              ; la columna pedida, dentro del rango
        dec eax
        js .mal
        cmp eax, ebx
        jbe .d2
        cmp L_MODO, 2
        je .d2
        mov al, EV_REF
        call ev_pon_err
        jmp .cero
.d2:
        mov L_I, 0
.b:
        push dword [ev_cad_p]
        call .celda                 ; ECX, EDX=la celda del puesto L_I
        push ebp
        call celda_valor
        pop ebp
        cmp byte [ev_err], 0
        jne .ferr
        cmp byte [cv_tipo], 0
        je .sig
        mov ecx, L_VL
        mov [cmp_l], ecx
        mov ecx, L_VH
        mov [cmp_l+4], ecx
        mov ecx, L_VT
        call ev_compara             ; lo buscado con lo de la celda
        test eax, eax
        jz .esta
        jns .menor
        cmp L_EX, 0                 ; la celda ya pasa: en un rango ordenado
        je .fpop                    ; no hay que seguir
        jmp .sig
.menor:
        cmp L_EX, 0
        jne .sig
.esta:
        mov ecx, L_I
        mov L_ENC, ecx
        cmp L_EX, 0
        jne .fpop
        jmp .sig
.sig:
        pop dword [ev_cad_p]
        inc L_I
        mov eax, L_I
        cmp eax, L_N
        jb .b
        jmp .fin
.ferr:
        pop dword [ev_cad_p]
        jmp .cero
.fpop:
        pop dword [ev_cad_p]
.fin:
        mov eax, L_ENC
        cmp eax, -1
        je .mal
        cmp L_MODO, 2
        jne .dato
        inc eax
        call ev_de_ent
        jmp .x
.dato:
        mov L_I, eax                ; lo que hay en ese puesto, en la columna pedida
        call .celda
        mov eax, L_IDX
        dec eax
        cmp L_V, 0
        je .h
        add ecx, eax
        jmp .v
.h:
        add edx, eax
.v:
        push ebp
        call celda_valor
        pop ebp
        jmp .x
.mal:
        call ev_mal
.cero:
        xor eax, eax
        xor edx, edx
        mov byte [ev_tx], 0
.x:
        mov esp, ebp
        pop ebp
        ret
; (interna) ECX=columna EDX=fila de la celda del puesto L_I de lo que se mira
.celda:
        mov ecx, L_C1
        mov edx, L_F1
        cmp L_V, 0
        je .ch
        add edx, L_I
        ret
.ch:
        add ecx, L_I
        ret

; ---- INDICE(rango; fila [; columna]): lo que hay en ese sitio del rango (de
;      un rango de una sola fila, con un número solo, la columna) ----
ev_indice:
        call ev_abre
        call ev_rango_lee
        jc ev_cero_mal
        push ecx
        push edx
        push esi
        push edi
        call ev_puntoycoma
        jc .mal4
        call ev_ent
        push eax
        mov eax, -1
        call ev_puntoycoma
        jc .n
        call ev_ent
.n:
        call ev_cierra
        mov ebx, eax                ; EBX=columna (-1: no se ha dicho) EAX=fila
        pop eax
        pop edi
        pop esi
        pop edx
        pop ecx
        cmp byte [ev_err], 0
        jne ev_primario.cero
        cmp ebx, -1
        jne .a
        mov ebx, 1
        cmp edx, edi                ; una sola fila: el número es la columna
        jne .a
        xchg eax, ebx
.a:
        dec eax
        js ev_cero_mal
        dec ebx
        js ev_cero_mal
        add edx, eax
        add ecx, ebx
        cmp edx, edi
        ja .ref
        cmp ecx, esi
        ja .ref
        push ebp
        call celda_valor
        pop ebp
        ret
.ref:
        mov al, EV_REF
        call ev_pon_err
        jmp ev_primario.cero
.mal4:
        add esp, 16
        jmp ev_cero_mal

; ---- ABS(expresión) ----
ev_abs:
        call ev_arg1
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
        call ev_num
        push edx
        push eax
        call ev_car
        cmp al, ';'
        jne .mal
        inc dword [ev_ptr]
        call ev_expr
        call ev_num
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

; ESI=texto (acabado en 0) -> EDX:EAX; CF=1 si no es un número (o una fecha)
; y nada más (puede llevar signo, y espacios delante y detrás). Conserva los
; registros
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
        call lee_fecha              ; una fecha (16/11/2026) es el número de su día
        jnc .t
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
; de más. Si no cabe en [nf_cars] caracteres se le quitan decimales y, si ni
; el entero cabe, salen almohadillas: un número nunca se ve cortado. Conserva
; los registros
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
        cmp eax, [nf_cars]
        jbe .cabe
        test esi, esi
        jz .no
        dec esi
        jmp .otra
.no:
        mov edi, ebp
        mov ecx, [nf_cars]
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

; =============================================================================
;  Números con formato y fechas
; =============================================================================
; EDX:EAX -> texto en EDI, acabado en 0 (EDI queda en ese 0), con ECX
; decimales (de 0 a 8), redondeados (la mitad, alejándose de cero). BL: bit 0,
; sin los ceros de más del final; bit 1, con puntos de millares. Conserva los
; demás registros
num_fijo:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push ebp
        mov [nj_dec], ecx
        mov [nj_band], bl
        mov byte [nf_neg], 0
        test edx, edx
        jns .p
        neg edx
        neg eax
        sbb edx, 0
        mov byte [nf_neg], 1
.p:
        mov ebx, CU_DEC
        sub ebx, ecx
        mov ebx, [pot10+ebx*4]
        call div64
        shl ecx, 1
        cmp ecx, ebx
        jb .nr
        add eax, 1
        adc edx, 0
.nr:
        mov ecx, eax                ; el cero no lleva signo
        or ecx, edx
        jnz .sg
        mov byte [nf_neg], 0
.sg:
        mov ebx, [nj_dec]
        mov ebx, [pot10+ebx*4]
        call div64                  ; EDX:EAX=la parte entera ECX=los decimales
        mov ebp, ecx
        push edi
        mov edi, nf_ent
        call u64_txt
        mov byte [edi], 0
        mov ecx, edi
        sub ecx, nf_ent             ; ECX=cifras de la parte entera
        pop edi
        cmp byte [nf_neg], 0
        je .e
        mov al, '-'
        stosb
.e:
        mov esi, nf_ent
.ec:
        lodsb
        stosb
        dec ecx
        jz .ef
        test byte [nj_band], 2      ; un punto cada tres cifras
        jz .ec
        mov eax, ecx
        xor edx, edx
        mov ebx, 3
        div ebx
        test edx, edx
        jnz .ec
        mov al, '.'
        stosb
        jmp .ec
.ef:
        mov ecx, [nj_dec]
        test ecx, ecx
        jz .fin
        mov al, ','
        stosb
        mov eax, ebp                ; los decimales, de atrás adelante
        mov esi, ecx
        mov ebx, 10
.d:
        xor edx, edx
        div ebx
        add dl, '0'
        mov [edi+ecx-1], dl
        dec ecx
        jnz .d
        add edi, esi
        test byte [nj_band], 1
        jz .fin
.tz:
        cmp byte [edi-1], '0'
        jne .tz1
        dec edi
        jmp .tz
.tz1:
        cmp byte [edi-1], ','
        jne .fin
        dec edi
.fin:
        mov byte [edi], 0
        pop ebp
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; EDX:EAX -> texto en EDI, acabado en 0, con el formato EBX (los bits F_NUM y
; F_DEC de una celda). Si no cabe en [nf_cars] caracteres salen almohadillas:
; un número nunca se ve cortado. Conserva los registros
fmt_celda:
        test ebx, F_NUM
        jz num_fmt                  ; sin formato: como siempre
        pushad
        mov ebp, edi
        mov ecx, ebx
        shr ecx, F_DEC_D
        and ecx, 15
        cmp ecx, CU_DEC
        jbe .d
        mov ecx, CU_DEC
.d:
        shr ebx, F_NUM_D
        and ebx, 7
        mov [nj_tipo], bl
        cmp ebx, FN_FECHA
        je .fecha
        ja .almo
        cmp ebx, FN_PORC
        jne .n
        mov esi, edx                ; por cien, si cabe
        call abs64
        cmp edx, 0x0147AE14         ; (2^63 / 100, la parte de arriba)
        jae .almo
        push ecx
        mov ecx, edx
        mov ebx, 100
        mul ebx
        imul ecx, 100
        add edx, ecx
        pop ecx
        test esi, esi
        jns .n
        neg edx
        neg eax
        sbb edx, 0
.n:
        xor ebx, ebx
        cmp byte [nj_tipo], FN_MIL
        jb .n1
        cmp byte [nj_tipo], FN_MONEDA
        ja .n1
        mov bl, 2
.n1:
        call num_fijo
        cmp byte [nj_tipo], FN_PORC
        jne .n2
        mov al, '%'
        stosb
.n2:
        cmp byte [nj_tipo], FN_MONEDA
        jne .mide
        cmp byte [moneda_txt], 0
        je .mide
        mov al, ' '
        stosb
        mov esi, moneda_txt
.m:
        lodsb
        test al, al
        jz .mide
        stosb
        jmp .m
.fecha:
        test edx, edx
        js .almo
        mov ebx, CU_ESC
        call div64
        test edx, edx
        jnz .almo
        cmp eax, FECHA_MAX
        ja .almo
        call fecha_txt
.mide:
        mov byte [edi], 0
        sub edi, ebp
        cmp edi, [nf_cars]
        jbe .x
.almo:
        mov edi, ebp
        mov ecx, [nf_cars]
        mov al, '#'
        rep stosb
        mov byte [edi], 0
.x:
        popad
        ret

; EAX=día (en días desde el 30/12/1899) -> «dd/mm/aaaa» en EDI, que avanza
fecha_txt:
        push eax
        push ebx
        push ecx
        push edx
        call serie_fecha            ; EAX=año ECX=mes EBX=día
        push eax
        mov eax, ebx
        call .dos
        mov al, '/'
        stosb
        mov eax, ecx
        call .dos
        mov al, '/'
        stosb
        pop eax
        xor edx, edx
        mov ebx, 1000               ; el año, con sus cuatro cifras
.a:
        div ebx
        add al, '0'
        stosb
        mov eax, edx
        push eax
        mov eax, ebx
        xor edx, edx
        mov ebx, 10
        div ebx
        mov ebx, eax
        pop eax
        xor edx, edx
        test ebx, ebx
        jnz .a
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
.dos:
        xor edx, edx
        mov ebx, 10
        div ebx
        add al, '0'
        stosb
        mov al, dl
        add al, '0'
        stosb
        ret

; EAX=año ECX=mes EBX=día -> EAX=el día en días desde el 30/12/1899 (el día
; juliano menos 2.415.019); CF=1 si ese día no existe. Conserva los demás
fecha_serie:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        cmp ecx, 1
        jb .mal
        cmp ecx, 12
        ja .mal
        cmp ebx, 1
        jb .mal
        cmp ebx, 31
        ja .mal
        mov [fs_d], ebx
        mov [fs_m], ecx
        xor esi, esi                ; enero y febrero cuentan con el año de antes
        cmp ecx, 2
        ja .a
        inc esi
.a:
        add eax, 4800
        sub eax, esi
        lea edi, [esi+esi*2]
        shl edi, 2
        add edi, ecx
        sub edi, 3                  ; EDI=el mes, contando desde marzo
        mov ecx, eax                ; ECX=el año, desde el -4800
        imul eax, edi, 153
        add eax, 2
        xor edx, edx
        mov esi, 5
        div esi
        add ebx, eax
        imul eax, ecx, 365
        add ebx, eax
        mov eax, ecx
        shr eax, 2
        add ebx, eax
        mov eax, ecx
        xor edx, edx
        mov esi, 100
        div esi
        sub ebx, eax
        mov eax, ecx
        xor edx, edx
        mov esi, 400
        div esi
        add ebx, eax
        sub ebx, 32045 + 2415019
        mov eax, ebx
        push eax
        call serie_fecha            ; de vuelta: ¿sale el mismo día?
        cmp ebx, [fs_d]
        jne .mal2
        cmp ecx, [fs_m]
        jne .mal2
        pop eax
        clc
        jmp .x
.mal2:
        pop eax
.mal:
        stc
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=día (en días desde el 30/12/1899) -> EAX=año ECX=mes EBX=día
serie_fecha:
        push edx
        push esi
        push edi
        add eax, 2415019 + 32044
        mov esi, eax
        lea eax, [eax*4+3]
        xor edx, edx
        mov ecx, 146097
        div ecx
        mov edi, eax                ; EDI=el siglo (de 400 en 400 años)
        imul eax, edi, 146097
        shr eax, 2
        sub esi, eax
        lea eax, [esi*4+3]
        xor edx, edx
        mov ecx, 1461
        div ecx
        mov ecx, eax                ; ECX=el año dentro del siglo
        imul eax, ecx, 1461
        shr eax, 2
        sub esi, eax                ; ESI=el día del año, desde el 1 de marzo
        lea eax, [esi+esi*4+2]
        xor edx, edx
        mov ebx, 153
        div ebx                     ; EAX=el mes, desde marzo
        push eax
        imul eax, eax, 153
        add eax, 2
        xor edx, edx
        mov ebx, 5
        div ebx
        mov ebx, esi
        sub ebx, eax
        inc ebx
        pop eax
        imul edi, edi, 100
        add edi, ecx
        sub edi, 4800
        mov ecx, eax
        add ecx, 3
        cmp eax, 10
        jb .f
        sub ecx, 12
        inc edi
.f:
        mov eax, edi
        pop edi
        pop esi
        pop edx
        ret

; ESI=texto -> EDX:EAX=el día (d/m/aaaa, desde 1900) como número, y ESI
; detrás; CF=1 si no es una fecha (y ESI como estaba)
lee_fecha:
        push ebx
        push ecx
        push edi
        push esi
        call .num2
        jc .no
        mov ebx, eax
        cmp byte [esi], '/'
        jne .no
        inc esi
        call .num2
        jc .no
        mov ecx, eax
        cmp byte [esi], '/'
        jne .no
        inc esi
        xor eax, eax
        mov edi, 4
.y:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .no
        imul eax, 10
        add eax, edx
        inc esi
        dec edi
        jnz .y
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        jbe .no
        cmp eax, 1900
        jb .no
        call fecha_serie
        jc .no
        mov ecx, CU_ESC
        mul ecx
        add esp, 4
        pop edi
        pop ecx
        pop ebx
        clc
        ret
.no:
        pop esi
        pop edi
        pop ecx
        pop ebx
        stc
        ret
; (interna) una o dos cifras -> EAX; CF=1 si no las hay o son más
.num2:
        xor eax, eax
        xor edi, edi
.n:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .nf
        imul eax, 10
        add eax, edx
        inc esi
        inc edi
        cmp edi, 2
        jbe .n
        stc
        ret
.nf:
        cmp edi, 1
        ret

        section .bss
            alignb 4
ev_ptr      resd 1                  ; por dónde va la fórmula
ev_prof     resd 1                  ; fórmulas a medias (unas piden otras)
ev_cad_p    resd 1                  ; hasta dónde hay textos a medio calcular
cu_m        resd 4                  ; el producto o el dividendo, de 128 bits
cmp_l       resd 2                  ; lo de la izquierda de una comparación
nf_v        resd 2                  ; num_fmt: el valor sin signo
nf_cars     resd 1                  ; los caracteres que caben en la celda
nj_dec      resd 1                  ; num_fijo: los decimales
fs_d        resd 1                  ; fecha_serie: el día y el mes pedidos
fs_m        resd 1
nf_ent      resb 24                 ; la parte entera, en texto
ev_nom      resb 16                 ; el nombre leído
moneda_txt  resb 8                  ; lo que va detrás de la moneda («EUR»)
ev_pila_min resd 1                  ; la pila no puede bajar de aquí (0: da igual)
cv_gen      resd 1                  ; la tanda de cálculo (cambia con cada cambio)
ev_cads     resb EV_CADS + CAD_MAX + 1  ; los textos a medio calcular (y uno de sobra)
ev_hondo    resb 1                  ; se ha tocado fondo en este cálculo
ev_err      resb 1                  ; 0, o EV_ERROR, EV_CIRC, EV_REF
ev_tx       resb 1                  ; lo que hay en EDX:EAX: 0 número, 1 texto, 2 vacío
cv_tipo     resb 1                  ; lo que era la última celda pedida
nf_neg      resb 1
nj_band     resb 1
nj_tipo     resb 1
        section .text
