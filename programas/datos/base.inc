; =============================================================================
;  DATOS.HVP - la base en memoria y su archivo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La base vive en «la arena»: un trozo de memoria con la misma forma que el
;  archivo .BDD (cabecera, y las tablas una tras otra con sus campos y sus
;  registros; ver la cabecera de datos.asm). [fin] es donde acaba. Todo cambio
;  de tamaño (un registro nuevo, uno que crece, uno que se borra) es abrir o
;  cerrar un hueco con «hueco», que corre lo que hay detrás.
;
;  De la tabla que se ve se lleva un índice ([idx]: dónde empieza cada
;  registro) y la vista ([vis]: los registros que pasan el filtro, en el orden
;  pedido). Los hace «indexa», a la que se llama después de cada cambio.
;
;  Las rutinas conservan los registros salvo los que devuelven algo.
; =============================================================================

%macro BASE_BSS 0
nv_n        resd 1                  ; la tabla que se va a hacer: sus campos,
nv_cam      resb MAX_CAM * C_TAM    ;   cómo son
nv_org      resb MAX_CAM            ;   y de qué campo de ahora sale cada uno
rh_reg      resd 1
rh_fin      resd 1
rh_delta    resd 1
rh_tam      resd 1
va_fin      resd 1
%endmacro

; ---- la memoria: un bloque con los búferes de trabajo y, detrás, la arena;
;      se pide lo más que haya de 2 MB a 448 K. CF=1 si no hay ----
M_FIJO      equ MAX_CAM * EB_TAM + RB_TAM + 2 * MAX_REG * 4
            align 4
mem_tam     dd 2048 * 1024, 1024 * 1024, 448 * 1024, 0
memoria:
        pushad
        mov esi, mem_tam
.b:
        mov ecx, [esi]
        test ecx, ecx
        jz .no
        add ecx, M_FIJO
        push esi
        push ecx
        HV mem_bloque
        pop ecx
        pop esi
        jnc .si
        add esi, 4
        jmp .b
.si:
        mov [blq_mango], edx
        mov [ebufs], eax
        add eax, MAX_CAM * EB_TAM
        mov [rbuf], eax
        add eax, RB_TAM
        mov [idx], eax
        add eax, MAX_REG * 4
        mov [vis], eax
        add eax, MAX_REG * 4
        mov [arena], eax
        add eax, [esi]
        mov [arena_fin], eax
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; ---- la base vacía ----
base_vacia:
        pushad
        mov edi, [arena]
        mov ecx, H_CAB / 4
        xor eax, eax
        rep stosd
        mov [fin], edi
        mov edi, [arena]
        mov dword [edi+H_FIRMA], 'HBDD'
        mov dword [edi+H_VER], 1
        xor eax, eax
        call tab_pon
        popad
        ret

; EAX=tabla -> EBX=su bloque
tab_ptr:
        push ecx
        mov ebx, [arena]
        add ebx, H_CAB
        mov ecx, eax
.b:
        test ecx, ecx
        jz .x
        add ebx, [ebx+T_TAM]
        dec ecx
        jmp .b
.x:
        pop ecx
        ret

; pone a la vista la tabla EAX (si no hay tablas, ninguna): sin filtro y
; desde el principio
tab_pon:
        pushad
        mov ebx, [arena]
        mov ecx, [ebx+H_NTAB]
        mov dword [tab], 0
        mov dword [tab_i], 0
        test ecx, ecx
        jz .v
        cmp eax, ecx
        jb .a
        xor eax, eax
.a:
        mov [tab_i], eax
        cmp [ebx+H_ACT], eax
        je .a1
        mov [ebx+H_ACT], eax
        mov byte [sucio], 1
.a1:
        call tab_ptr
        mov [tab], ebx
        mov al, [ebx+T_VISTA]
        and al, 1
        mov [vista], al
.v:
        xor eax, eax
        mov [f_on], al
        mov [sel], eax
        mov [top], eax
        mov [col], eax
        mov [xdesp], eax
        mov [ftop], eax
        call indexa
        call coloca
        popad
        ret

; EAX=campo de la tabla que se ve -> EBX=su ficha
cam_ptr:
        mov ebx, eax
        shl ebx, 5
        add ebx, [tab]
        add ebx, T_CAB
        ret

; EAX=campo -> EAX=la x donde empieza su columna (sin correr la rejilla); con
; el número de campos, el ancho de toda la tabla
col_x:
        push ebx
        push ecx
        push edx
        mov ecx, eax
        xor eax, eax
        mov ebx, [tab]
        test ebx, ebx
        jz .x
        add ebx, T_CAB
.b:
        test ecx, ecx
        jz .x
        movzx edx, word [ebx+C_ANCHO]
        add eax, edx
        add ebx, C_TAM
        dec ecx
        jmp .b
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; el índice de la tabla que se ve: los tipos de sus campos y dónde empieza
; cada registro; y, detrás, la vista
indexa:
        pushad
        mov dword [n_reg], 0
        mov dword [n_cam], 0
        mov ebx, [tab]
        test ebx, ebx
        jz .fin
        movzx ecx, byte [ebx+T_NCAM]
        mov [n_cam], ecx
        lea esi, [ebx+T_CAB]
        xor edx, edx
.t:
        cmp edx, ecx
        jae .t1
        mov al, [esi+C_TIPO]
        mov [tipos+edx], al
        mov al, [esi+C_DEC]
        mov [decs+edx], al
        add esi, C_TAM
        inc edx
        jmp .t
.t1:
        mov ecx, [ebx+T_NREG]       ; ESI=el primer registro
        mov [n_reg], ecx
        mov edi, [idx]
.r:
        test ecx, ecx
        jz .fin
        mov [edi], esi
        add edi, 4
        movzx eax, word [esi]
        add esi, eax
        dec ecx
        jmp .r
.fin:
        mov eax, [col]              ; (que el campo elegido exista)
        cmp eax, [n_cam]
        jb .c
        mov dword [col], 0
.c:
        call filtra
        popad
        ret

; la vista: los registros que pasan el filtro, y en su orden
filtra:
        pushad
        mov edi, [vis]
        xor ecx, ecx
        xor edx, edx
.b:
        cmp ecx, [n_reg]
        jae .f
        mov eax, ecx
        call pasa
        jc .s
        mov [edi+edx*4], ecx
        inc edx
.s:
        inc ecx
        jmp .b
.f:
        mov [n_vis], edx
        call ordena
        mov eax, [sel]              ; (y que el elegido exista)
        cmp eax, edx
        jb .x
        mov eax, edx
        dec eax
        jns .x1
        xor eax, eax
.x1:
        mov [sel], eax
.x:
        mov byte [suma_ok], 0
        popad
        ret

; ESI=registro EAX=campo -> ESI=donde está el valor de ese campo
val_ptr:
        push ecx
        push edx
        add esi, 2
        xor ecx, ecx
.b:
        cmp ecx, eax
        jae .x
        mov dl, [tipos+ecx]
        cmp dl, T_NUM
        je .c4
        cmp dl, T_FECHA
        je .c4
        cmp dl, T_SINO
        je .c1
        movzx edx, word [esi]
        lea esi, [esi+edx+2]
        jmp .s
.c4:
        add esi, 4
        jmp .s
.c1:
        inc esi
.s:
        inc ecx
        jmp .b
.x:
        pop edx
        pop ecx
        ret

; ESI=valor BL=tipo: lo copia tal cual a EDI, que avanza
val_copia:
        push ecx
        push esi
        mov ecx, 4
        cmp bl, T_NUM
        je .c
        cmp bl, T_FECHA
        je .c
        mov ecx, 1
        cmp bl, T_SINO
        je .c
        movzx ecx, word [esi]
        add ecx, 2
.c:
        rep movsb
        pop esi
        pop ecx
        ret

; ESI=valor AL=tipo DL=decimales -> su texto en EDI, acabado en 0 (EDI queda
; en el 0). Los números, con puntos de millares si [fmt_mil]
val_txt:
        push eax
        push ecx
        push esi
        cmp al, T_NUM
        je .num
        cmp al, T_FECHA
        je .fec
        cmp al, T_SINO
        je .sn
        movzx ecx, word [esi]
        add esi, 2
        rep movsb
        jmp .x
.num:
        mov eax, [esi]
        cmp eax, VACIO
        je .x
        call fmt_num
        jmp .x
.fec:
        mov eax, [esi]
        test eax, eax
        jz .x
        call fmt_fecha
        jmp .x
.sn:
        cmp byte [esi], 0
        mov esi, t_no
        je .sn1
        mov esi, t_si
.sn1:
        call hvl_cad
.x:
        mov byte [edi], 0
        pop esi
        pop ecx
        pop eax
        ret

; EAX=registro (de la tabla que se ve) ECX=campo -> ESI=su texto (en vbuf)
reg_txt:
        push eax
        push edx
        push edi
        mov esi, [idx]
        mov esi, [esi+eax*4]
        mov eax, ecx
        call val_ptr
        mov al, [tipos+ecx]
        mov dl, [decs+ecx]
        mov edi, vbuf
        call val_txt
        mov esi, vbuf
        pop edi
        pop edx
        pop eax
        ret

; ESI=texto DL=tipo DH=decimales: pone su valor en EDI, que avanza. CF=1 si
; el texto no vale para ese tipo (y entonces pone el valor vacío)
txt_val:
        push eax
        push ecx
        push esi
        cmp dl, T_NUM
        je .num
        cmp dl, T_FECHA
        je .fec
        cmp dl, T_SINO
        je .sn
        call largo
        mov eax, MEMO_MAX
        cmp dl, T_MEMO
        je .t1
        mov eax, TXT_MAX
.t1:
        cmp ecx, eax
        jbe .t2
        mov ecx, eax
.t2:
        mov [edi], cx
        add edi, 2
.t3:
        test ecx, ecx
        jz .t9
        lodsb
        cmp dl, T_MEMO              ; un texto corto va en una línea
        je .t4
        cmp al, ' '
        jae .t4
        mov al, ' '
.t4:
        stosb
        dec ecx
        jmp .t3
.t9:
        clc
        jmp .x
.num:
        push edx
        mov dl, dh
        call lee_num
        pop edx
        jnc .v4
        mov eax, VACIO
        stosd
        stc
        jmp .x
.fec:
        call lee_fecha
        jnc .v4
        xor eax, eax
        stosd
        stc
        jmp .x
.v4:
        stosd
        clc
        jmp .x
.sn:
        call lee_sino
        stosb                       ; (no toca el acarreo)
.x:
        pop esi
        pop ecx
        pop eax
        ret

; ESI=texto -> ECX=lo que mide
largo:
        push esi
        xor ecx, ecx
.b:
        cmp byte [esi], 0
        je .x
        inc esi
        inc ecx
        jmp .b
.x:
        pop esi
        ret
; copia ESI (acabado en 0) a EDI, con su 0
copia:
        push eax
        push esi
        push edi
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop edi
        pop esi
        pop eax
        ret
; ESI=texto -> ZF=1 si está vacío (o solo tiene espacios)
vacio:
        push esi
.b:
        cmp byte [esi], ' '
        jne .f
        inc esi
        jmp .b
.f:
        cmp byte [esi], 0
        pop esi
        ret

; =============================================================================
;  Números, fechas y sí o no
; =============================================================================
; EAX=número DL=decimales -> su texto en EDI, que avanza: «-1.234,50» (los
; puntos de millares, solo si [fmt_mil] y el campo lleva decimales: un año o
; una cantidad se leen mejor sin ellos)
fmt_num:
        pushad
        movzx ebp, dl
        test eax, eax
        jns .p
        neg eax
        mov byte [edi], '-'
        inc edi
.p:
        mov esi, num_tmp            ; las cifras, de la última a la primera
        xor ecx, ecx
        mov ebx, 10
.d:
        xor edx, edx
        div ebx
        add dl, '0'
        mov [esi+ecx], dl
        inc ecx
        test eax, eax
        jnz .d
.pad:
        cmp ecx, ebp
        ja .e
        mov byte [esi+ecx], '0'
        inc ecx
        jmp .pad
.e:
        dec ecx
        mov al, [esi+ecx]
        stosb
        test ecx, ecx
        jz .f
        cmp ecx, ebp
        jb .e
        jne .e1
        mov al, ','
        stosb
        jmp .e
.e1:
        cmp byte [fmt_mil], 0
        je .e
        test ebp, ebp
        jz .e
        mov eax, ecx
        sub eax, ebp
        xor edx, edx
        push ecx
        mov ecx, 3
        div ecx
        pop ecx
        test edx, edx
        jnz .e
        mov al, '.'
        stosb
        jmp .e
.f:
        mov [esp], edi
        popad
        ret

; ESI=texto DL=decimales -> EAX=el número (multiplicado por 10^decimales);
; vacío: EAX=VACIO. CF=1 si no es un número o tiene más de nueve cifras.
; Vale «1234,5», «1.234,5» y «1234.5»; «1.500» son mil quinientos. Lo que
; sobra de decimales se redondea
lee_num:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        movzx ebp, dl
.sp:
        cmp byte [esi], ' '
        jne .s0
        inc esi
        jmp .sp
.s0:
        cmp byte [esi], 0
        jne .s1
        mov eax, VACIO
        jmp .ok
.s1:
        push esi                    ; ¿cuál es el separador de los decimales?
        xor ecx, ecx                ; CL=comas CH=puntos
        xor edx, edx                ; EDX=lo que hay detrás del último punto
.c:
        lodsb
        test al, al
        jz .c9
        cmp al, ','
        jne .c1
        inc cl
        jmp .c
.c1:
        cmp al, '.'
        jne .c2
        inc ch
        xor edx, edx
        jmp .c
.c2:
        inc edx
        jmp .c
.c9:
        pop esi
        mov bl, ','
        test cl, cl
        jnz .sep
        xor bl, bl
        cmp ch, 1
        jne .sep
        cmp edx, 3                  ; «1.500»: el punto es de millares
        je .sep
        mov bl, '.'
.sep:
        cmp cl, 1
        ja .err
        xor edi, edi                ; el signo
        cmp byte [esi], '-'
        jne .i
        inc edi
        inc esi
.i:
        xor eax, eax
        xor ecx, ecx                ; cuántas cifras van
.il:
        movzx edx, byte [esi]
        test edx, edx
        jz .esc
        cmp dl, bl
        je .fr
        cmp dl, '.'
        je .salta
        sub edx, '0'
        cmp edx, 9
        ja .esc
        cmp eax, 99999999
        ja .err
        imul eax, 10
        add eax, edx
        inc ecx
.salta:
        inc esi
        jmp .il
.fr:
        inc esi
.fl:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .esc
        inc esi
        inc ecx
        test ebp, ebp
        jz .extra
        cmp eax, 99999999
        ja .err
        imul eax, 10
        add eax, edx
        dec ebp
        jmp .fl
.extra:
        cmp edx, 5                  ; la primera que sobra redondea
        jb .sk
        inc eax
.sk:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .t
        inc esi
        jmp .sk
.esc:
        test ebp, ebp               ; los decimales que no se han escrito
        jz .t
        cmp eax, 99999999
        ja .err
        imul eax, 10
        dec ebp
        jmp .esc
.t:
        cmp byte [esi], ' '
        jne .t1
        inc esi
        jmp .t
.t1:
        cmp byte [esi], 0
        jne .err
        test ecx, ecx
        jz .err
        test edi, edi
        jz .ok
        neg eax
.ok:
        clc
        jmp .x
.err:
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=aaaammdd -> «dd/mm/aaaa» en EDI, que avanza
fmt_fecha:
        push eax
        push ebx
        push ecx
        push edx
        xor edx, edx
        mov ecx, 100
        div ecx
        mov ebx, edx                ; día
        xor edx, edx
        div ecx                     ; EAX=año EDX=mes
        push eax
        push edx
        mov eax, ebx
        call .dos
        mov al, '/'
        stosb
        pop eax
        call .dos
        mov al, '/'
        stosb
        pop eax
        call hvl_num
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
.dos:
        cmp eax, 10
        jae .d1
        mov byte [edi], '0'
        inc edi
.d1:
        jmp hvl_num

; ESI=«d/m/aaaa» (o con guiones o puntos; el año, también con dos cifras:
; hasta 29 es 20xx y desde 30, 19xx) -> EAX=aaaammdd; vacío: EAX=0. CF=1 si
; no es una fecha que exista
lee_fecha:
        push ebx
        push ecx
        push edx
        push esi
        call vacio
        jne .hay
        xor eax, eax
        jmp .x                      ; (CF=0)
.hay:
        call .num
        jc .err
        mov ebx, eax                ; día
        call .sep
        jc .err
        call .num
        jc .err
        mov ecx, eax                ; mes
        call .sep
        jc .err
        call .num
        jc .err
        cmp eax, 100
        jae .a
        add eax, 1900
        cmp eax, 1930
        jae .a
        add eax, 100
.a:
        cmp eax, 1000
        jb .err
        cmp eax, 9999
        ja .err
        cmp ecx, 1
        jb .err
        cmp ecx, 12
        ja .err
        cmp ebx, 1
        jb .err
        movzx edx, byte [dias_mes+ecx-1]
        cmp ecx, 2                  ; febrero de un año bisiesto
        jne .d
        push eax
        push ebx
        push edx
        test al, 3
        jnz .nb
        xor edx, edx
        mov ebx, 100
        div ebx
        test edx, edx
        jnz .sb                     ; divisible por 4 y no por 100
        test al, 3                  ; los siglos, solo uno de cada cuatro
        jnz .nb
.sb:
        pop edx
        inc edx
        jmp .b1
.nb:
        pop edx
.b1:
        pop ebx
        pop eax
.d:
        cmp ebx, edx
        ja .err
.f:
        cmp byte [esi], ' '
        jne .f1
        inc esi
        jmp .f
.f1:
        cmp byte [esi], 0
        jne .err
        imul eax, 100
        add eax, ecx
        imul eax, 100
        add eax, ebx
        clc
        jmp .x
.err:
        stc
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
.num:
        cmp byte [esi], ' '
        jne .n0
        inc esi
        jmp .num
.n0:
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .nerr
        call hvl_lee_num
        cmp eax, 9999
        ja .nerr
        clc
        ret
.nerr:
        stc
        ret
.sep:
        mov al, [esi]
        inc esi
        cmp al, '/'
        je .sok
        cmp al, '-'
        je .sok
        cmp al, '.'
        je .sok
        stc
        ret
.sok:
        clc
        ret
dias_mes    db 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31

; ESI=texto -> AL=1 si dice que sí («sí», «s», «x», «1», «verdadero»), 0 si
; dice que no o está vacío; CF=1 si no dice ni una cosa ni la otra
lee_sino:
        push ebx
        push esi
.sp:
        cmp byte [esi], ' '
        jne .s
        inc esi
        jmp .sp
.s:
        mov bl, [esi]
        xor al, al
        test bl, bl
        jz .ok
        or bl, 0x20                 ; (en minúscula)
        cmp bl, 'n'
        je .ok
        cmp bl, '0'
        je .ok
        cmp bl, 'f'
        je .ok
        inc al
        cmp bl, 's'
        je .ok
        cmp bl, 'x'
        je .ok
        cmp bl, '1'
        je .ok
        cmp bl, 'v'
        je .ok
        xor al, al
        stc
        jmp .x
.ok:
        clc
.x:
        pop esi
        pop ebx
        ret

; la fecha de hoy (INT 21h, 2Ah) -> [hoy]
hoy_leer:
        pushad
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [rl+RL_EAX], 0x2A00
        mov byte [rl+RL_INT], 0x21
        mov esi, rl
        HV rm_interrup
        movzx eax, word [rl+RL_ECX]
        imul eax, 100
        movzx ecx, byte [rl+RL_EDX+1]
        add eax, ecx
        imul eax, 100
        movzx ecx, byte [rl+RL_EDX]
        add eax, ecx
        mov [hoy], eax
        popad
        ret

; =============================================================================
;  Comparar textos sin mirar mayúsculas ni acentos
; =============================================================================
; la tabla [pliega]: de cada carácter, con qué se compara. Las minúsculas y
; las vocales con acento, como su mayúscula; la eñe, entre la N y la O (por
; eso de la O en adelante todo va un sitio más arriba)
pl_pares    db "áAéEíIóOúUüUÜUÉEàAèEìIòOùUçCÇCäAëEïIöOâAêEîIôOûUñÑ", 0
haz_pliega:
        pushad
        xor eax, eax
.a:
        mov [pliega+eax], al
        inc eax
        cmp eax, 256
        jb .a
        mov eax, 'a'
.b:
        mov bl, al
        sub bl, 0x20
        mov [pliega+eax], bl
        inc eax
        cmp eax, 'z'
        jbe .b
        mov esi, pl_pares
.c:
        movzx eax, byte [esi]
        test eax, eax
        jz .d
        mov bl, [esi+1]
        mov [pliega+eax], bl
        add esi, 2
        jmp .c
.d:
        xor eax, eax
.e:
        mov bl, [pliega+eax]
        cmp bl, 0xA5                ; la Ñ, donde estaba la O
        jne .e1
        mov bl, 'O'
        jmp .e2
.e1:
        cmp bl, 'O'
        jb .e2
        cmp bl, 0xFF
        je .e2
        inc bl
.e2:
        mov [pliega+eax], bl
        inc eax
        cmp eax, 256
        jb .e
        popad
        ret

; ESI=texto (ECX letras) y EDI=otro (EDX letras) -> EAX menor, igual o mayor
; que cero según el primero vaya antes, igual o después
cmp_txt:
        push ebx
        push ecx
        push edx
        push esi
        push edi
.b:
        test ecx, ecx
        jz .fa
        test edx, edx
        jz .may
        movzx eax, byte [esi]
        movzx ebx, byte [edi]
        mov al, [pliega+eax]
        mov bl, [pliega+ebx]
        sub eax, ebx
        jnz .x
        inc esi
        inc edi
        dec ecx
        dec edx
        jmp .b
.fa:
        xor eax, eax                ; se acaba el primero: igual o menor
        test edx, edx
        jz .x
        dec eax
        jmp .x
.may:
        mov eax, 1
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ESI=texto (ECX letras): ¿contiene el de EDI (EDX letras)? -> CF=0 si lo
; contiene
contiene:
        pushad
        mov ebp, edx                ; lo que mide lo buscado
.b:
        cmp ecx, ebp
        jb .no
        xor edx, edx
.c:
        cmp edx, ebp
        jae .si
        movzx eax, byte [esi+edx]
        movzx ebx, byte [edi+edx]
        mov al, [pliega+eax]
        cmp al, [pliega+ebx]
        jne .s
        inc edx
        jmp .c
.s:
        inc esi
        dec ecx
        jmp .b
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; =============================================================================
;  El filtro y el orden
; =============================================================================
; EAX=registro: ¿pasa el filtro? -> CF=0 si pasa
pasa:
        cmp byte [f_on], 0
        jne .hay
        clc
        ret
.hay:
        pushad
        mov esi, [idx]
        mov esi, [esi+eax*4]
        mov eax, [f_campo]
        call val_ptr                ; ESI=el valor
        movzx ebx, byte [tipos+eax]
        mov ecx, [f_op]
        cmp ecx, 4
        je .vacio
        test ecx, ecx
        jz .contiene
        cmp bl, T_NUM
        je .num
        cmp bl, T_FECHA
        je .fec
        cmp bl, T_SINO
        je .sn
        movzx ecx, word [esi]       ; texto
        add esi, 2
        mov edi, f_txt
        mov edx, [f_len]
        call cmp_txt
        jmp .mira
.num:
        mov eax, [esi]
        cmp eax, VACIO              ; lo que está vacío no es mayor ni menor
        jne .n1
        cmp dword [f_val], VACIO
        jne .no
        cmp dword [f_op], 1
        je .si
        jmp .no
.fec:
        mov eax, [esi]
        test eax, eax
        jnz .n1
        cmp dword [f_val], 0
        jne .no
        cmp dword [f_op], 1
        je .si
        jmp .no
.sn:
        movzx eax, byte [esi]
.n1:
        cmp eax, [f_val]
        mov eax, 0
        je .mira
        mov eax, 1
        jg .mira
        mov eax, -1
.mira:                              ; EAX: el valor frente al del filtro
        mov ecx, [f_op]
        cmp ecx, 1
        jne .m2
        test eax, eax
        jz .si
        jmp .no
.m2:
        cmp ecx, 2
        jne .m3
        test eax, eax
        jg .si
        jmp .no
.m3:
        test eax, eax
        jl .si
        jmp .no
.contiene:
        mov al, bl                  ; en su texto, como se ve
        mov ecx, [f_campo]
        mov dl, [decs+ecx]
        mov edi, vbuf
        mov byte [fmt_mil], 0
        call val_txt
        mov ecx, edi
        sub ecx, vbuf
        mov esi, vbuf
        mov edi, f_txt
        mov edx, [f_len]
        call contiene
        jc .no
        jmp .si
.vacio:
        cmp bl, T_NUM
        jne .v1
        cmp dword [esi], VACIO
        je .si
        jmp .no
.v1:
        cmp bl, T_FECHA
        jne .v2
        cmp dword [esi], 0
        je .si
        jmp .no
.v2:
        cmp bl, T_SINO
        jne .v3
        cmp byte [esi], 0
        je .si
        jmp .no
.v3:
        cmp word [esi], 0
        je .si
.no:
        popad
        stc
        ret
.si:
        popad
        clc
        ret

; EAX=un registro EDX=otro -> EAX menor, igual o mayor que cero según el
; primero vaya antes o después por el campo [ord_c] (al revés si [ord_d]); a
; igualdad, el que se escribió antes
cmp_reg:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov ebp, eax
        mov ebx, edx
        mov ecx, [ord_c]
        mov esi, [idx]
        mov esi, [esi+ebp*4]
        mov eax, ecx
        call val_ptr
        push esi
        mov esi, [idx]
        mov esi, [esi+ebx*4]
        call val_ptr
        mov edi, esi
        pop esi
        mov al, [tipos+ecx]
        cmp al, T_NUM
        je .num
        cmp al, T_FECHA
        je .num
        cmp al, T_SINO
        je .sn
        movzx ecx, word [esi]
        add esi, 2
        movzx edx, word [edi]
        add edi, 2
        call cmp_txt
        jmp .r
.num:
        mov eax, [esi]
        cmp eax, [edi]
        mov eax, 0
        je .r
        mov eax, 1
        jg .r
        mov eax, -1
        jmp .r
.sn:
        movzx eax, byte [esi]
        movzx edx, byte [edi]
        sub eax, edx
.r:
        test eax, eax
        jnz .d
        mov eax, ebp
        sub eax, ebx
        jmp .x
.d:
        cmp byte [ord_d], 0
        je .x
        neg eax
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ordena la vista por el campo de orden de la tabla (ordenación de Shell: con
; miles de registros, la de inserción directa se haría eterna en un 386)
ordena:
        pushad
        mov ebx, [tab]
        test ebx, ebx
        jz .x
        movzx eax, byte [ebx+T_ORDC]
        cmp eax, [n_cam]
        jae .x
        mov [ord_c], eax
        mov al, [ebx+T_ORDD]
        mov [ord_d], al
        mov ecx, [n_vis]
        cmp ecx, 2
        jb .x
        mov ebp, 1                  ; el salto: 1, 4, 13, 40...
.g:
        lea eax, [ebp+ebp*2+1]
        cmp eax, ecx
        jae .pas
        mov ebp, eax
        jmp .g
.pas:
        mov esi, ebp
.i:
        cmp esi, [n_vis]
        jae .sig
        mov edi, [vis]
        mov ebx, [edi+esi*4]        ; el que se coloca
        mov ecx, esi
.j:
        cmp ecx, ebp
        jb .pon
        mov edx, ecx
        sub edx, ebp
        mov eax, [edi+edx*4]
        push edx
        mov edx, ebx
        call cmp_reg
        pop edx
        test eax, eax
        jle .pon
        mov eax, [edi+edx*4]
        mov [edi+ecx*4], eax
        mov ecx, edx
        jmp .j
.pon:
        mov [edi+ecx*4], ebx
        inc esi
        jmp .i
.sig:
        mov eax, ebp
        xor edx, edx
        mov ecx, 3
        div ecx
        mov ebp, eax
        test ebp, ebp
        jnz .pas
.x:
        popad
        ret

; =============================================================================
;  Cambiar la arena
; =============================================================================
; EDI=un sitio de la tabla que se ve, EAX=octetos: abre (o, si es negativo,
; cierra) un hueco ahí; lo que hay de EDI en adelante se corre. CF=1 si no
; cabe (y no cambia nada)
hueco:
        pushad
        mov ebx, [fin]
        mov edx, ebx
        add edx, eax
        cmp edx, [arena_fin]
        ja .no
        mov ecx, ebx
        sub ecx, edi                ; lo que se corre
        test eax, eax
        jz .ok
        js .menos
        lea esi, [ebx-1]            ; hacia arriba: de atrás adelante
        lea edi, [esi+eax]
        std
        rep movsb
        cld
        jmp .aj
.menos:
        mov esi, edi
        add edi, eax
        rep movsb
.aj:
        mov eax, [esp+28]
        add [fin], eax
        mov ebx, [tab]
        add [ebx+T_TAM], eax
.ok:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; pone el registro montado en [rbuf] en la tabla: EAX=el registro que cambia
; (-1: uno nuevo, al final). CF=1 si no cabe
reg_pon:
        pushad
        mov esi, [rbuf]
        movzx edx, word [esi]       ; lo que mide
        mov ebx, [tab]
        cmp eax, -1
        jne .cam
        cmp dword [ebx+T_NREG], MAX_REG
        jae .no
        mov edi, ebx
        add edi, [ebx+T_TAM]        ; donde acaba la tabla
        mov eax, edx
        call hueco
        jc .no
        inc dword [ebx+T_NREG]
        jmp .pon
.cam:
        mov edi, [idx]
        mov edi, [edi+eax*4]
        movzx ecx, word [edi]
        push edi
        add edi, ecx
        mov eax, edx
        sub eax, ecx
        call hueco
        pop edi
        jc .no
.pon:
        mov ecx, edx
        rep movsb
        call indexa
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; quita el registro EAX
reg_quita:
        pushad
        mov edi, [idx]
        mov edi, [edi+eax*4]
        movzx eax, word [edi]
        add edi, eax
        neg eax
        call hueco
        mov ebx, [tab]
        dec dword [ebx+T_NREG]
        call indexa
        popad
        ret

; una tabla nueva, vacía, al final de la base y a la vista: ESI=nombre, sus
; campos en nv_cam y nv_n. CF=1 si no cabe
tabla_nueva:
        pushad
        mov ebx, [arena]
        cmp dword [ebx+H_NTAB], MAX_TAB
        jae .no
        mov edi, [fin]
        mov eax, [nv_n]
        shl eax, 5
        add eax, T_CAB
        lea edx, [edi+eax]
        cmp edx, [arena_fin]
        ja .no
        push edi
        push eax
        mov ecx, eax
        xor al, al
        rep stosb
        pop eax
        pop edi
        mov [edi+T_TAM], eax
        mov eax, [nv_n]
        mov [edi+T_NCAM], al
        mov byte [edi+T_ORDC], 0xFF
        push edi
        add edi, T_NOM
        mov ecx, 23
.n:
        lodsb
        test al, al
        jz .n1
        stosb
        dec ecx
        jnz .n
.n1:
        pop edi
        add edi, T_CAB
        mov esi, nv_cam
        mov ecx, [nv_n]
        shl ecx, 3
        rep movsd
        mov [fin], edi
        mov eax, [ebx+H_NTAB]
        inc dword [ebx+H_NTAB]
        call tab_pon
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; borra la tabla que se ve
tabla_borra:
        pushad
        mov edi, [tab]
        mov eax, [edi+T_TAM]
        lea esi, [edi+eax]
        mov ecx, [fin]
        sub ecx, esi
        rep movsb
        sub [fin], eax
        mov ebx, [arena]
        dec dword [ebx+H_NTAB]
        ; los campos de otras tablas que venían de esta ya no vienen de
        ; ninguna, y los que venían de una de detrás, de una menos
        mov edx, [tab_i]
        inc edx                     ; (C_REFT lleva la tabla + 1)
        xor eax, eax
.t:
        cmp eax, [ebx+H_NTAB]
        jae .t9
        push ebx
        call tab_ptr
        movzx ecx, byte [ebx+T_NCAM]
        lea esi, [ebx+T_CAB]
        pop ebx
.c:
        test ecx, ecx
        jz .t1
        cmp [esi+C_REFT], dl
        jb .c1
        jne .c0
        mov byte [esi+C_REFT], 1    ; (y se queda en 0 con el dec)
.c0:
        dec byte [esi+C_REFT]
.c1:
        add esi, C_TAM
        dec ecx
        jmp .c
.t1:
        inc eax
        jmp .t
.t9:
        mov eax, [tab_i]
        cmp eax, [ebx+H_NTAB]
        jb .p
        dec eax
        jns .p
        xor eax, eax
.p:
        mov byte [sucio], 1
        call tab_pon
        popad
        ret

; rehace la tabla que se ve con los campos de nv_cam (nv_n; cada uno sale del
; campo nv_org de ahora, o de ninguno si es 255): así se añade, se quita, se
; cambia y se mueve un campo. Los valores que cambian de tipo pasan por su
; texto; el que no vale para el tipo nuevo se queda vacío. La tabla nueva se
; monta en el sitio libre de detrás y luego se pone en lugar de la vieja.
; CF=1 si no cabe (y no cambia nada)
rehaz:
        pushad
        mov ebx, [tab]
        mov edi, [fin]
        mov eax, [nv_n]
        shl eax, 5
        add eax, T_CAB + 16
        add eax, edi
        cmp eax, [arena_fin]
        ja .no
        mov esi, ebx
        mov ecx, T_CAB
        rep movsb
        mov esi, nv_cam
        mov ecx, [nv_n]
        shl ecx, 5
        rep movsb
        mov edx, [fin]
        mov eax, [nv_n]
        mov [edx+T_NCAM], al
        mov al, [ebx+T_ORDC]        ; el campo de orden, donde quede
        mov byte [edx+T_ORDC], 0xFF
        cmp al, 0xFF                ; (sin orden: sigue sin él)
        je .o1
        xor ecx, ecx
.o:
        cmp ecx, [nv_n]
        jae .o1
        cmp [nv_org+ecx], al
        jne .o0
        mov [edx+T_ORDC], cl
.o0:
        inc ecx
        jmp .o
.o1:
        xor ebp, ebp                ; EBP=el registro
.r:
        cmp ebp, [n_reg]
        jae .hechos
        lea eax, [edi+REG_MAX]
        cmp eax, [arena_fin]
        ja .no
        mov [rh_reg], edi
        add edi, 2
        xor ecx, ecx                ; ECX=el campo nuevo
.c:
        cmp ecx, [nv_n]
        jae .rc
        mov eax, ecx
        shl eax, 5
        mov dl, [nv_cam+eax+C_TIPO]
        mov dh, [nv_cam+eax+C_DEC]
        movzx eax, byte [nv_org+ecx]
        cmp al, 0xFF
        je .vacio
        mov esi, [idx]
        mov esi, [esi+ebp*4]
        call val_ptr
        mov bl, [tipos+eax]
        mov bh, [decs+eax]
        cmp bl, dl
        jne .conv
        cmp bl, T_NUM
        jne .igual
        cmp bh, dh
        jne .conv
.igual:
        call val_copia
        jmp .cs
.conv:
        push edx
        push edi
        mov al, bl
        mov dl, bh
        mov edi, vbuf
        mov byte [fmt_mil], 0
        call val_txt
        pop edi
        pop edx
        mov esi, vbuf
        call txt_val
        jmp .cs
.vacio:
        mov esi, t_nada
        call txt_val
.cs:
        inc ecx
        jmp .c
.rc:
        mov eax, edi
        mov esi, [rh_reg]
        sub eax, esi
        mov [esi], ax
        inc ebp
        jmp .r
.hechos:
        mov eax, edi
        sub eax, [fin]
        mov [rh_tam], eax           ; lo que mide la tabla nueva
        mov edx, [fin]
        mov [edx+T_TAM], eax
        mov [rh_fin], edx
        mov ebx, [tab]
        sub eax, [ebx+T_TAM]
        mov [rh_delta], eax         ; lo que crece
        ; se corre todo lo de detrás de la tabla vieja (con la nueva, que
        ; está al final) y la nueva se copia a su sitio
        mov [fin], edi
        mov edi, ebx
        add edi, [ebx+T_TAM]
        call hueco
        jnc .cabe
        mov eax, [rh_fin]
        mov [fin], eax
        jmp .no
.cabe:
        mov esi, [rh_fin]
        add esi, [rh_delta]
        mov edi, [tab]
        mov ecx, [rh_tam]
        rep movsb
        mov eax, [rh_fin]
        add eax, [rh_delta]
        mov [fin], eax
        mov byte [f_on], 0          ; (el filtro hablaba de los campos de antes)
        call indexa
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; =============================================================================
;  El archivo
; =============================================================================
; pone el nombre de la base: ESI=nombre (con ruta o sin ella; sin extensión,
; .BDD) -> ruta_bdd, completa, y las del temporal y la copia anterior
pon_ruta:
        pushad
        mov edi, ruta_bdd
        cmp byte [esi+1], ':'
        je .tal
        push esi
        HV dos_dir_actual           ; «A:\CARPETA»
        pop esi
        mov edi, ruta_bdd
        cmp byte [esi], '\'         ; desde la raíz: solo la unidad
        jne .rel
        add edi, 2
        jmp .tal
.rel:
        cmp byte [edi], 0
        je .r1
        inc edi
        jmp .rel
.r1:
        cmp byte [edi-1], '\'
        je .tal
        mov byte [edi], '\'
        inc edi
.tal:
        xor edx, edx                ; EDX=1 si lleva extensión
        mov ecx, 100
.c:
        lodsb
        test al, al
        jz .c9
        cmp al, 'a'
        jb .c0
        cmp al, 'z'
        ja .c0
        sub al, 0x20
.c0:
        cmp al, '.'
        jne .c1
        mov edx, 1
.c1:
        cmp al, '\'
        jne .c2
        xor edx, edx
.c2:
        stosb
        dec ecx
        jnz .c
.c9:
        test edx, edx
        jnz .e
        mov esi, t_ext_bdd
        call hvl_cad
.e:
        mov byte [edi], 0
        mov esi, ruta_bdd
        mov edi, ruta_tmp
        mov eax, '$$$'
        call hvl_ruta
        mov edi, ruta_ant
        mov eax, 'ANT'
        call hvl_ruta
        popad
        ret

; ESI=nombre de antes EDI=el nuevo: cambia de nombre un archivo (INT 21h,
; 56h, con los dos nombres en el búfer de paso). CF=1 si no se puede
renombra:
        pushad
        push edi
        mov edi, [dbuf]
        call copia
        pop esi
        mov edi, [dbuf]
        add edi, 128
        call copia
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [rl+RL_EAX], 0x5600
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        add eax, 128
        mov [rl+RL_EDI], eax
        mov byte [rl+RL_INT], 0x21
        mov esi, rl
        HV rm_interrup
        popad
        ret

; guarda la base. Primero entera en el temporal; después el archivo de antes
; pasa a ser la copia .ANT y el temporal toma su nombre: si se va la luz en
; cualquier momento, queda en el disco una base entera
guarda:
        pushad
        mov ebx, [arena]
        mov eax, [fin]
        sub eax, ebx
        mov [ebx+H_TAM], eax
        call suma_haz
        mov [ebx+H_SUMA], edx
        mov esi, ruta_tmp
        call escribe_base
        jc .mal
        mov esi, ruta_ant
        HV dos_borrar
        mov esi, ruta_bdd
        mov edi, ruta_ant
        call renombra               ; (la primera vez no hay de antes)
        mov esi, ruta_tmp
        mov edi, ruta_bdd
        call renombra
        jc .mal
        mov byte [sucio], 0
        popad
        ret
.mal:
        mov esi, t_e_guardar
        call aviso
        popad
        ret
; (interna) escribe la arena entera en el archivo ESI, de 16 K en 16 K (DOS
; no pasa de 64 K de una vez). CF=1 si no se puede (y no deja el archivo)
escribe_base:
        pushad
        mov ebp, esi
        HV dos_crear
        jc .no
        mov ebx, eax
        mov esi, [arena]
        mov edx, [fin]
        sub edx, esi                ; lo que falta
.w:
        test edx, edx
        jz .w1
        mov ecx, edx
        cmp ecx, 16384
        jbe .w0
        mov ecx, 16384
.w0:
        push ecx
        push edx
        push esi
        HV dos_escribir
        pop esi
        pop edx
        pop ecx
        jc .wmal
        add esi, ecx
        sub edx, ecx
        jmp .w
.w1:
        HV dos_cerrar
        jc .borra
        popad
        clc
        ret
.wmal:
        HV dos_cerrar
.borra:
        mov esi, ebp
        HV dos_borrar
.no:
        popad
        stc
        ret
; guarda si hay algo pendiente (el ancho de una columna, el orden, la vista)
guarda_si:
        cmp byte [sucio], 0
        je .x
        cmp dword [arena], 0
        je .x
        call guarda
.x:
        ret

; (interna) EBX=arena -> EDX=la suma de comprobación de lo que hay detrás de
; la cabecera, hasta [EBX+H_TAM]
suma_haz:
        push eax
        push ecx
        push esi
        lea esi, [ebx+H_CAB]
        mov ecx, [ebx+H_TAM]
        sub ecx, H_CAB
        xor edx, edx
        xor eax, eax
.s:
        test ecx, ecx
        jle .s1
        lodsb
        add edx, eax
        rol edx, 1
        dec ecx
        jmp .s
.s1:
        pop esi
        pop ecx
        pop eax
        ret

; (interna) lee el archivo ESI en la arena -> EAX=octetos leídos; CF=1 si no
; está. Si no cabe, EAX=-1
lee_arch:
        push ebx
        push ecx
        push edi
        xor al, al
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, [arena]
.l:
        mov ecx, [arena_fin]
        sub ecx, edi
        jz .lleno
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .f
        test eax, eax
        jz .f
        add edi, eax
        jmp .l
.lleno:
        HV dos_cerrar
        mov eax, -1
        clc
        jmp .x
.f:
        HV dos_cerrar
        mov eax, edi
        sub eax, [arena]
        clc
.x:
        pop edi
        pop ecx
        pop ebx
        ret

; (interna) ¿lo que hay en la arena (EAX octetos) es una base entera y sana?
; Lo repasa todo: los tamaños tienen que cuadrar hasta el último registro,
; para que después nada se salga de su sitio. CF=1 si no vale
valida:
        pushad
        mov ebx, [arena]
        cmp eax, H_CAB
        jb .no
        cmp dword [ebx+H_FIRMA], 'HBDD'
        jne .no
        cmp dword [ebx+H_VER], 1
        jne .no
        cmp [ebx+H_TAM], eax
        jne .no
        call suma_haz
        cmp [ebx+H_SUMA], edx
        jne .no
        cmp dword [ebx+H_NTAB], MAX_TAB
        ja .no
        add eax, ebx
        mov [va_fin], eax           ; donde acaba el archivo
        lea esi, [ebx+H_CAB]        ; ESI=la tabla
        mov ebp, [ebx+H_NTAB]
.t:
        test ebp, ebp
        jz .fin
        lea eax, [esi+T_CAB]
        cmp eax, [va_fin]
        ja .no
        mov edx, [esi+T_TAM]
        mov eax, [va_fin]
        sub eax, esi
        cmp edx, eax                ; (un tamaño que se sale del archivo)
        ja .no
        add edx, esi                ; EDX=donde acaba la tabla
        movzx ecx, byte [esi+T_NCAM]
        test ecx, ecx
        jz .no
        cmp ecx, MAX_CAM
        ja .no
        cmp dword [esi+T_NREG], MAX_REG
        ja .no
        mov byte [esi+T_NOM+23], 0
        cmp [esi+T_ORDC], cl
        jb .t0
        mov byte [esi+T_ORDC], 0xFF
.t0:
        lea edi, [esi+T_CAB]        ; EDI=los campos
        mov eax, ecx
        shl eax, 5
        add eax, edi
        cmp eax, edx
        ja .no
        push ecx
        xor ebx, ebx
.c:
        cmp byte [edi+C_TIPO], T_MEMO
        ja .noc
        cmp byte [edi+C_DEC], 3
        ja .noc
        mov byte [edi+C_NOM+19], 0
        mov al, [edi+C_TIPO]
        mov [tipos+ebx], al
        cmp word [edi+C_ANCHO], AN_MIN
        jae .c1
        mov word [edi+C_ANCHO], AN_MIN
.c1:
        cmp word [edi+C_ANCHO], AN_MAX
        jbe .c2
        mov word [edi+C_ANCHO], AN_MAX
.c2:
        add edi, C_TAM
        inc ebx
        dec ecx
        jnz .c
        pop ecx                     ; ECX=campos; EDI=el primer registro
        mov ebx, [esi+T_NREG]
.r:
        test ebx, ebx
        jz .rf
        lea eax, [edi+2]
        cmp eax, edx
        ja .no
        movzx eax, word [edi]
        add eax, edi                ; EAX=donde acaba el registro
        cmp eax, edx
        ja .no
        push esi
        push ebx
        lea esi, [edi+2]
        xor ebx, ebx
.v:
        cmp ebx, ecx
        jae .vf
        cmp byte [tipos+ebx], T_NUM
        je .v4
        cmp byte [tipos+ebx], T_FECHA
        je .v4
        cmp byte [tipos+ebx], T_SINO
        je .v1
        lea edi, [esi+2]
        cmp edi, eax
        ja .nov
        movzx edi, word [esi]
        cmp edi, MEMO_MAX
        ja .nov
        lea esi, [esi+edi+2]
        jmp .vs
.v4:
        add esi, 4
        jmp .vs
.v1:
        inc esi
.vs:
        cmp esi, eax
        ja .nov
        inc ebx
        jmp .v
.vf:
        cmp esi, eax                ; el registro acaba donde dice
        jne .nov
        mov edi, eax
        pop ebx
        pop esi
        dec ebx
        jmp .r
.rf:
        cmp edi, edx                ; y la tabla, también
        jne .no
        mov esi, edx
        dec ebp
        jmp .t
.fin:
        cmp esi, [va_fin]
        jne .no
        popad
        clc
        ret
.nov:
        pop ebx
        pop esi
        jmp .no
.noc:
        pop ecx
.no:
        popad
        stc
        ret

; abre la base de ruta_bdd. Si el archivo no está entero, prueba con el
; temporal y con la copia anterior. -> EAX=0 abierta, 1 no hay archivo
; (queda una base vacía con ese nombre), 2 no vale o no cabe (y queda vacía)
abre:
        push ebx
        push esi
        xor ebx, ebx                ; EBX=1 si hay algún archivo
        mov esi, ruta_bdd
        call .prueba
        jnc .ok
        mov esi, ruta_tmp
        call .prueba
        jnc .rec
        mov esi, ruta_ant
        call .prueba
        jnc .rec
        call base_vacia
        lea eax, [ebx+1]
        jmp .x
.rec:
        mov byte [aviso_ini], 1     ; se ha recuperado una copia: se dice y
        mov byte [sucio], 1         ; se guarda con su nombre
.ok:
        mov esi, [arena]
        mov eax, [esi+H_TAM]
        add eax, esi
        mov [fin], eax
        mov eax, [esi+H_ACT]
        call tab_pon
        xor eax, eax
.x:
        pop esi
        pop ebx
        ret
.prueba:
        call lee_arch
        jc .p0
        mov ebx, 1
        cmp eax, -1
        je .p0
        call valida
        ret
.p0:
        stc
        ret

; la base con la que se empieza: la que venga en los parámetros; si no, la
; última que se abrió (apuntada en DATOS.INI); si no, DATOS.BDD, al lado del
; programa
base_inicial:
        pushad
        mov esi, [param]
        test esi, esi
        jz .ini
.sp:
        cmp byte [esi], ' '
        jne .p1
        inc esi
        jmp .sp
.p1:
        cmp byte [esi], 0
        je .ini
        call pon_ruta
        call abre
        test eax, eax
        jz .x
.ini:
        mov esi, ruta_ini
        mov edi, nom_tmp
        mov ecx, 120
        call hvl_leer
        cmp eax, 3
        jb .def
        mov esi, nom_tmp            ; (hasta el fin de la línea)
.l:
        cmp byte [esi], ' '
        jb .l1
        inc esi
        jmp .l
.l1:
        mov byte [esi], 0
        mov esi, nom_tmp
        call pon_ruta
        call abre
        test eax, eax
        jz .x
.def:
        mov esi, ruta_def
        call pon_ruta
        call abre
.x:
        cmp byte [aviso_ini], 0
        je .x1
        call guarda
.x1:
        popad
        ret

; apunta en DATOS.INI la base que está abierta, para la próxima vez
apunta_ini:
        pushad
        mov esi, ruta_bdd
        call largo
        mov edx, ruta_bdd
        mov esi, ruta_ini
        call hvl_escribir
        popad
        ret
