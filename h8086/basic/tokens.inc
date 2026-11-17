; =============================================================================
;  Hispa-BASIC - las palabras: tokenizar una línea y listarla
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los códigos son los de GW-BASIC (comprobados guardando programas en
;  binario): las sentencias y los operadores, un octeto de 81h a F4h; las
;  funciones, FFh/FEh/FDh y un octeto. Como en GW-BASIC, una palabra solo es
;  palabra clave si va ENTERA (FORI es una variable; FOR I no).
;
;  Las palabras en español dan el mismo código que la inglesa, con una regla
;  de dónde valen (ver docs/8086/BASIC.md):
;    C_SENT   al empezar una sentencia, y no si detrás va «=»
;    C_TRAS   detrás de una expresión (HASTA, PASO, ENTONCES)
;    C_OPER   operador entre dos expresiones (Y, O)
;    C_NO     delante de una expresión (NO)
; =============================================================================

C_SENT      equ 1
C_TRAS      equ 2
C_OPER      equ 4
C_NO        equ 8
C_SIEMPRE   equ 16

; una palabra: longitud, el texto, y los dos octetos del código (el segundo
; es 0 si el código es de uno)
%macro P 2-3 0
        db %strlen(%1), %1, %2, %3
%endmacro
; una española: lo mismo y su clase
%macro E 3-4 0
        db %strlen(%1), %1, %2, %4, %3
%endmacro

pal_ing:
        P 'END', 0x81
        P 'FOR', 0x82
        P 'NEXT', 0x83
        P 'DATA', 0x84
        P 'INPUT', 0x85
        P 'DIM', 0x86
        P 'READ', 0x87
        P 'LET', 0x88
        P 'GOTO', 0x89
        P 'RUN', 0x8A
        P 'IF', 0x8B
        P 'RESTORE', 0x8C
        P 'GOSUB', 0x8D
        P 'RETURN', 0x8E
        P 'REM', 0x8F
        P 'STOP', 0x90
        P 'PRINT', 0x91
        P 'CLEAR', 0x92
        P 'LIST', 0x93
        P 'NEW', 0x94
        P 'ON', 0x95
        P 'WAIT', 0x96
        P 'DEF', 0x97
        P 'POKE', 0x98
        P 'CONT', 0x99
        P 'OUT', 0x9C
        P 'LPRINT', 0x9D
        P 'LLIST', 0x9E
        P 'WIDTH', 0xA0
        P 'ELSE', 0xA1
        P 'TRON', 0xA2
        P 'TROFF', 0xA3
        P 'SWAP', 0xA4
        P 'ERASE', 0xA5
        P 'EDIT', 0xA6
        P 'ERROR', 0xA7
        P 'RESUME', 0xA8
        P 'DELETE', 0xA9
        P 'AUTO', 0xAA
        P 'RENUM', 0xAB
        P 'DEFSTR', 0xAC
        P 'DEFINT', 0xAD
        P 'DEFSNG', 0xAE
        P 'DEFDBL', 0xAF
        P 'LINE', 0xB0
        P 'WHILE', 0xB1, 0xE9
        P 'WEND', 0xB2
        P 'CALL', 0xB3
        P 'WRITE', 0xB7
        P 'OPTION', 0xB8
        P 'RANDOMIZE', 0xB9
        P 'OPEN', 0xBA
        P 'CLOSE', 0xBB
        P 'LOAD', 0xBC
        P 'MERGE', 0xBD
        P 'SAVE', 0xBE
        P 'COLOR', 0xBF
        P 'CLS', 0xC0
        P 'MOTOR', 0xC1
        P 'BSAVE', 0xC2
        P 'BLOAD', 0xC3
        P 'SOUND', 0xC4
        P 'BEEP', 0xC5
        P 'PSET', 0xC6
        P 'PRESET', 0xC7
        P 'SCREEN', 0xC8
        P 'KEY', 0xC9
        P 'LOCATE', 0xCA
        P 'TO', 0xCC
        P 'THEN', 0xCD
        P 'TAB(', 0xCE
        P 'STEP', 0xCF
        P 'USR', 0xD0
        P 'FN', 0xD1
        P 'SPC(', 0xD2
        P 'NOT', 0xD3
        P 'ERL', 0xD4
        P 'ERR', 0xD5
        P 'STRING$', 0xD6
        P 'USING', 0xD7
        P 'INSTR', 0xD8
        P 'VARPTR', 0xDA
        P 'CSRLIN', 0xDB
        P 'POINT', 0xDC
        P 'OFF', 0xDD
        P 'INKEY$', 0xDE
        P 'AND', 0xEE
        P 'OR', 0xEF
        P 'XOR', 0xF0
        P 'EQV', 0xF1
        P 'IMP', 0xF2
        P 'MOD', 0xF3
        P 'LEFT$', 0xFF, 0x81
        P 'RIGHT$', 0xFF, 0x82
        P 'MID$', 0xFF, 0x83
        P 'SGN', 0xFF, 0x84
        P 'INT', 0xFF, 0x85
        P 'ABS', 0xFF, 0x86
        P 'SQR', 0xFF, 0x87
        P 'RND', 0xFF, 0x88
        P 'SIN', 0xFF, 0x89
        P 'LOG', 0xFF, 0x8A
        P 'EXP', 0xFF, 0x8B
        P 'COS', 0xFF, 0x8C
        P 'TAN', 0xFF, 0x8D
        P 'ATN', 0xFF, 0x8E
        P 'FRE', 0xFF, 0x8F
        P 'INP', 0xFF, 0x90
        P 'POS', 0xFF, 0x91
        P 'LEN', 0xFF, 0x92
        P 'STR$', 0xFF, 0x93
        P 'VAL', 0xFF, 0x94
        P 'ASC', 0xFF, 0x95
        P 'CHR$', 0xFF, 0x96
        P 'PEEK', 0xFF, 0x97
        P 'SPACE$', 0xFF, 0x98
        P 'OCT$', 0xFF, 0x99
        P 'HEX$', 0xFF, 0x9A
        P 'LPOS', 0xFF, 0x9B
        P 'CINT', 0xFF, 0x9C
        P 'CSNG', 0xFF, 0x9D
        P 'CDBL', 0xFF, 0x9E
        P 'FIX', 0xFF, 0x9F
        P 'PEN', 0xFF, 0xA0
        P 'STICK', 0xFF, 0xA1
        P 'STRIG', 0xFF, 0xA2
        P 'EOF', 0xFF, 0xA3
        P 'LOC', 0xFF, 0xA4
        P 'LOF', 0xFF, 0xA5
        P 'FILES', 0xFE, 0x81
        P 'FIELD', 0xFE, 0x82
        P 'SYSTEM', 0xFE, 0x83
        P 'NAME', 0xFE, 0x84
        P 'LSET', 0xFE, 0x85
        P 'RSET', 0xFE, 0x86
        P 'KILL', 0xFE, 0x87
        P 'PUT', 0xFE, 0x88
        P 'GET', 0xFE, 0x89
        P 'RESET', 0xFE, 0x8A
        P 'COMMON', 0xFE, 0x8B
        P 'CHAIN', 0xFE, 0x8C
        P 'DATE$', 0xFE, 0x8D
        P 'TIME$', 0xFE, 0x8E
        P 'PAINT', 0xFE, 0x8F
        P 'COM', 0xFE, 0x90
        P 'CIRCLE', 0xFE, 0x91
        P 'DRAW', 0xFE, 0x92
        P 'PLAY', 0xFE, 0x93
        P 'TIMER', 0xFE, 0x94
        P 'ERDEV', 0xFE, 0x95
        P 'IOCTL', 0xFE, 0x96
        P 'CHDIR', 0xFE, 0x97
        P 'MKDIR', 0xFE, 0x98
        P 'RMDIR', 0xFE, 0x99
        P 'SHELL', 0xFE, 0x9A
        P 'ENVIRON', 0xFE, 0x9B
        P 'VIEW', 0xFE, 0x9C
        P 'WINDOW', 0xFE, 0x9D
        P 'PMAP', 0xFE, 0x9E
        P 'PALETTE', 0xFE, 0x9F
        P 'LCOPY', 0xFE, 0xA0
        P 'CALLS', 0xFE, 0xA1
        P 'PCOPY', 0xFE, 0xA5
        P 'LOCK', 0xFE, 0xA7
        P 'UNLOCK', 0xFE, 0xA8
        P 'CVI', 0xFD, 0x81
        P 'CVS', 0xFD, 0x82
        P 'CVD', 0xFD, 0x83
        P 'MKI$', 0xFD, 0x84
        P 'MKS$', 0xFD, 0x85
        P 'MKD$', 0xFD, 0x86
        P 'EXTERR', 0xFD, 0x8B
        db 0

; las españolas (el mismo código que la inglesa). LIST en español usa la
; primera que encuentre para cada código: por eso «IR A» va antes que «IRA».
pal_esp:
        E 'IMPRIMIR', 0x91, C_SENT
        E 'PREGUNTAR', 0x85, C_SENT
        E 'SEA', 0x88, C_SENT
        E 'SI', 0x8B, C_SENT
        E 'ENTONCES', 0xCD, C_TRAS
        E 'SINO', 0xA1, C_SIEMPRE
        E 'PARA', 0x82, C_SENT
        E 'HASTA', 0xCC, C_TRAS
        E 'PASO', 0xCF, C_TRAS
        E 'SIGUIENTE', 0x83, C_SENT
        E 'IR A', 0x89, C_SENT | C_TRAS
        E 'IRA', 0x89, C_SENT | C_TRAS
        E 'LLAMAR', 0x8D, C_SENT | C_TRAS
        E 'VOLVER', 0x8E, C_SENT
        E 'MIENTRAS', 0xB1, C_SENT, 0xE9
        E 'FINMIENTRAS', 0xB2, C_SENT
        E 'DIMENSION', 0x86, C_SENT
        E 'DATOS', 0x84, C_SENT
        E 'LEER', 0x87, C_SENT
        E 'RELEER', 0x8C, C_SENT
        E 'FIN', 0x81, C_SENT
        E 'PARAR', 0x90, C_SENT
        E 'SEGUIR', 0x99, C_SENT
        E 'NOTA', 0x8F, C_SENT
        E 'EJECUTAR', 0x8A, C_SENT
        E 'LISTAR', 0x93, C_SENT
        E 'NUEVO', 0x94, C_SENT
        E 'CARGAR', 0xBC, C_SENT
        E 'GUARDAR', 0xBE, C_SENT
        E 'LIMPIAR', 0xC0, C_SENT
        E 'EN', 0x95, C_SENT
        E 'Y', 0xEE, C_OPER
        E 'O', 0xEF, C_OPER
        E 'NO', 0xD3, C_NO
        E 'SALIR', 0xFE, C_SENT, 0x83
        E 'ALEATORIO', 0xB9, C_SENT
        E 'VACIAR', 0x92, C_SENT
        E 'CAMBIAR', 0xA4, C_SENT
        db 0

; los operadores de un signo
ops_sig     db '>', 0xE6, '=', 0xE7, '<', 0xE8, '+', 0xE9, '-', 0xEA
            db '*', 0xEB, '/', 0xEC, '^', 0xED, '\', 0xF4, 0

; los códigos detrás de los que va un número de línea
tras_linea  db 0x89, 0x8D, 0xCD, 0xA1, 0x8C, 0x8A, 0x93, 0x9E, 0xA9, 0xAB
            db 0xAA, 0xA6, 0xA8, 0

; ---- ¿AL es letra (A-Z, ya en mayúsculas)? CF=1 si lo es ----
es_letra:
        cmp al, 'A'
        jb .no
        cmp al, 'Z' + 1
        ret                         ; CF=1 si es menor que 'Z'+1
.no:
        clc
        ret
; ---- ¿AL es letra, cifra o punto? (lo que sigue en un nombre) CF=1 ----
es_alfanum:
        call es_letra
        jc .si
        cmp al, '.'
        jne es_cifra
.si:
        stc
        ret
es_cifra:
        cmp al, '0'
        jb .no
        cmp al, '9' + 1
        ret
.no:
        clc
        ret
; ---- AL en mayúsculas (solo a-z) ----
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret

; =============================================================================
;  tokeniza: la línea de DS:SI (ASCIZ, ya sin el número) a b_tok.
;  Sale con CX = la longitud (con el 0 final).
; =============================================================================
tokeniza:
        mov byte [v_listar_es], 0
        mov di, b_tok
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 1
        mov byte [v_lineas], 0
.sig:
        cmp di, b_tok + LIN_TAM + 40
        jb .cabe
        mov al, 23                  ; Línea demasiado larga
        jmp error
.cabe:
        lodsb
        test al, al
        jz .fin
        cmp al, ' '
        jne .no_esp
        stosb
        jmp .sig
.no_esp:
        cmp al, '"'
        je .comillas
        cmp al, "'"
        je .apostrofo
        cmp al, '?'
        je .interrog
        cmp al, ':'
        je .dos_puntos
        cmp al, '&'
        je .hex
        cmp al, '.'
        je .punto
        call es_cifra
        jc .numero
        call mayus
        call es_letra
        jc .palabra
        ; un operador de un signo
        mov bx, ops_sig
.op:
        mov ah, [cs:bx]
        test ah, ah
        jz .otro
        cmp ah, al
        je .es_op
        add bx, 2
        jmp .op
.es_op:
        mov al, [cs:bx + 1]
        stosb
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 0
        cmp al, 0xEA                ; «-» entre números de línea (LIST 10-20)
        je .sig
        mov byte [v_lineas], 0
        jmp .sig
.otro:
        stosb
        mov byte [v_ini_sent], 0
        cmp al, ','
        je .coma
        mov byte [v_lineas], 0
        cmp al, ')'
        je .cierra
        mov byte [v_opnd], 0
        jmp .sig
.coma:
        mov byte [v_opnd], 0
        jmp .sig
.cierra:
        mov byte [v_opnd], 1
        jmp .sig
.fin:
        stosb
        mov cx, di
        sub cx, b_tok
        ret

.comillas:
        stosb
.com1:
        lodsb
        test al, al
        jz .fin
        stosb
        cmp al, '"'
        jne .com1
        mov byte [v_opnd], 1
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
        jmp .sig
.apostrofo:
        mov ax, 0x8F3A              ; «:» REM
        stosw
        mov al, 0xD9
        stosb
.crudo:                             ; lo que queda, tal cual
        lodsb
        stosb
        test al, al
        jnz .crudo
        dec di
        jmp .fin_sin
.fin_sin:
        xor al, al
        jmp .fin
.interrog:
        mov al, 0x91
        stosb
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
        jmp .sig
.dos_puntos:
        stosb
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 1
        mov byte [v_lineas], 0
        jmp .sig
.punto:
        mov al, [si]
        call es_cifra
        jc .numero_p
        mov al, '.'
        jmp .otro
.numero_p:
        mov al, '.'
.numero:
        dec si
        call tok_numero
        mov byte [v_opnd], 1
        mov byte [v_ini_sent], 0
        jmp .sig
.hex:                               ; &H1F, &O17 o &17 (octal)
        mov al, [si]
        call mayus
        mov bl, 16
        mov ah, 0x0C
        cmp al, 'H'
        je .hex1
        mov bl, 8
        mov ah, 0x0B
        cmp al, 'O'
        je .hex1
        call es_cifra
        jc .hex2
        mov al, '&'
        jmp .otro
.hex1:
        inc si
.hex2:
        mov al, ah
        stosb
        call lee_base               ; AX = el valor
        stosw
        mov byte [v_opnd], 1
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
        jmp .sig

        ; ---- una palabra ----
.palabra:
        dec si
        call tok_palabra
        jmp .sig

; ---- lee un número en base BL (8 o 16) de DS:SI: AX ----
lee_base:
        xor dx, dx
.c:
        mov al, [si]
        call mayus
        sub al, '0'
        jb .x
        cmp al, 9
        jbe .d
        sub al, 'A' - '0' - 10
        cmp al, 10
        jb .x
.d:
        cmp al, bl
        jae .x
        xor ah, ah
        xchg ax, dx
        push dx
        xor bh, bh
        mul bx
        pop dx
        jc .desb
        add dx, ax
        jc .desb
        inc si
        jmp .c
.x:
        mov ax, dx
        ret
.desb:
        mov al, 6                   ; Desbordamiento
        jmp error

; ---- un número: lo convierte y escribe su código ----
tok_numero:
        cmp byte [v_lineas], 0
        je .normal
        ; un número de línea: solo cifras
        push si
        xor dx, dx
.l:
        mov al, [si]
        call es_cifra
        jnc .l_fin
        sub al, '0'
        cbw
        xchg ax, dx
        push dx
        mov cx, 10
        mul cx
        pop dx
        jc .l_no
        add dx, ax
        jc .l_no
        inc si
        jmp .l
.l_fin:
        mov al, [si]                ; si sigue un punto o una E, no lo es
        cmp al, '.'
        je .l_no
        pop ax
        mov al, 0x0E
        stosb
        mov ax, dx
        stosw
        ret
.l_no:
        pop si
.normal:
        mov byte [v_lineas], 0
        call lee_numero             ; ac = el número; SI detrás
        mov al, [ac + AC_TIPO]
        cmp al, T_ENT
        je .ent
        cmp al, T_SIM
        je .sim
        mov al, 0x1F
        stosb
        call ac_a_mbf8              ; escribe 8 octetos en ES:DI
        ret
.sim:
        mov al, 0x1D
        stosb
        call ac_a_mbf4
        ret
.ent:
        mov ax, [ac + AC_M0]
        cmp ax, 10
        ja .ent1
        add al, 0x11
        stosb
        ret
.ent1:
        cmp ax, 255
        ja .ent2
        mov ah, al
        mov al, 0x0F
        stosw
        ret
.ent2:
        push ax
        mov al, 0x1C
        stosb
        pop ax
        stosw
        ret

; ---- una palabra (DS:SI en su primera letra) ----
tok_palabra:
        ; copia la palabra en mayúsculas a b_num (con un «$» o un «(» detrás,
        ; si los hay: así se encuentran LEFT$ y TAB()
        push di
        mov di, b_num
        mov bx, si                  ; el principio
.c:
        lodsb
        call mayus
        call es_alfanum
        jnc .c_fin
        cmp di, b_num + 40
        jae .c
        stosb
        jmp .c
.c_fin:
        dec si
        mov cx, di
        sub cx, b_num               ; la longitud sin «$»
        cmp al, '$'
        je .pon_extra
        cmp al, '('
        jne .sin_extra
.pon_extra:
        stosb
.sin_extra:
        xor al, al
        stosb
        pop di
        ; ¿FN y un nombre?
        cmp cx, 2
        jbe .no_fn
        cmp word [b_num], 'FN'
        jne .no_fn
        mov al, 0xD1
        stosb
        mov si, bx
        add si, 2
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
        ret
.no_fn:
        ; con el «$» o «(» de detrás
        push cx
        push bx                     ; (busca_pal deja la clase en BL)
        inc cx
        mov dx, pal_ing
        call busca_pal              ; CF=1 si la encuentra: AX = el código
        pop bx
        pop cx
        jnc .sin
        inc cx                      ; se come también el «$» o el «(»
        jmp .encontrada
.sin:
        push cx
        push bx
        mov dx, pal_ing
        call busca_pal
        pop bx
        pop cx
        jc .encontrada
        ; ¿una española?
        call tok_espanola
        jc .esp_hecha
        ; un nombre de variable: tal cual (en mayúsculas)
        mov si, bx
.var:
        lodsb
        call mayus
        call es_alfanum
        jnc .var_fin
        stosb
        jmp .var
.var_fin:
        cmp al, '$'
        je .suf
        cmp al, '%'
        je .suf
        cmp al, '!'
        je .suf
        cmp al, '#'
        jne .var_x
.suf:
        stosb
        inc si
.var_x:
        dec si
        mov byte [v_opnd], 1
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
.esp_hecha:
        ret

.encontrada:                        ; AX = el código; CX = lo que ocupa
        mov si, bx
        add si, cx
        jmp pon_codigo

; ---- escribe el código AX (AL el primero, AH el segundo o 0) y pone el
; estado según cuál sea. DS:SI detrás de la palabra ----
pon_codigo:
        cmp al, 0xA1                ; ELSE va con «:» delante
        jne .no_else
        push ax
        mov al, ':'
        stosb
        pop ax
.no_else:
        stosb
        test ah, ah
        jz .uno
        xchg al, ah
        stosb
        xchg al, ah
.uno:
        mov byte [v_opnd], 0
        mov byte [v_ini_sent], 0
        mov byte [v_lineas], 0
        test ah, ah
        jnz .func
        cmp al, 0x8F                ; REM: lo que queda, tal cual
        je .rem
        cmp al, 0x84                ; DATA: tal cual hasta «:»
        je .data
        cmp al, 0xCD                ; THEN y ELSE: empieza otra sentencia
        je .ini
        cmp al, 0xA1
        je .ini
        cmp al, 0xD4                ; ERL, ERR, CSRLIN, INKEY$: son valores
        je .valor
        cmp al, 0xD5
        je .valor
        cmp al, 0xDB
        je .valor
        cmp al, 0xDE
        je .valor
        ; ¿lleva números de línea detrás?
        mov bx, tras_linea
.tl:
        mov ah, [cs:bx]
        test ah, ah
        jz .x
        inc bx
        cmp ah, al
        jne .tl
        mov byte [v_lineas], 1
.x:
        ret
.ini:
        mov byte [v_ini_sent], 1
        mov byte [v_lineas], 1
        ret
.valor:
        mov byte [v_opnd], 1
        ret
.func:
        cmp ax, 0x94FE              ; TIMER, DATE$, TIME$, RND y ERDEV: valores
        je .valor
        cmp ax, 0x8DFE
        je .valor
        cmp ax, 0x8EFE
        je .valor
        cmp ax, 0x88FF
        je .valor
        ret
.rem:
        lodsb
        stosb
        test al, al
        jnz .rem
        dec si
        dec di
        ret
.data:
        xor ah, ah                  ; AH = 1 dentro de comillas
.d1:
        lodsb
        test al, al
        jz .d_fin
        cmp al, '"'
        jne .d2
        xor ah, 1
.d2:
        test ah, ah
        jnz .d3
        cmp al, ':'
        je .d_fin
.d3:
        stosb
        jmp .d1
.d_fin:
        dec si
        ret

; ---- busca en la tabla CS:DX la palabra de b_num de CX letras. CF=1 si
; está: AX = el código. Las españolas (pal_esp) tienen además la clase en BL
busca_pal:
        push si
        push di
        mov si, dx
.e:
        mov al, [cs:si]
        test al, al
        jz .no
        xor ah, ah
        cmp ax, cx
        jne .salta
        push si
        push cx
        inc si
        mov di, b_num
.cmp:
        mov al, [cs:si]
        cmp al, [di]
        jne .dist
        inc si
        inc di
        loop .cmp
        pop cx
        pop si
        add si, cx
        mov ax, [cs:si + 1]
        mov bl, [cs:si + 3]
        pop di
        pop si
        stc
        ret
.dist:
        pop cx
        pop si
.salta:
        mov al, [cs:si]
        xor ah, ah
        add si, ax
        add si, 3
        cmp dx, pal_esp
        jne .e
        inc si
        jmp .e
.no:
        pop di
        pop si
        clc
        ret

; ---- ¿la palabra de b_num (CX letras, empieza en BX del texto) es una
; española que vale aquí? CF=1 si lo es y ya está escrita ----
tok_espanola:
        ; «IR A»: IR y, tras espacios, una A sola
        cmp cx, 2
        jne .no_ir
        cmp word [b_num], 'IR'
        jne .no_ir
        mov si, bx
        add si, 2
.ir1:
        lodsb
        cmp al, ' '
        je .ir1
        call mayus
        cmp al, 'A'
        jne .no_ir_r
        mov al, [si]
        call mayus
        call es_alfanum
        jc .no_ir_r
        test byte [v_ini_sent], 1
        jnz .ir_vale
        test byte [v_opnd], 1
        jz .no_ir_r
.ir_vale:
        mov ax, 0x0089
        jmp pon_esp
.no_ir_r:
        mov si, bx
.no_ir:
        mov dx, pal_esp
        push bx
        call busca_pal              ; AX = el código, BL = la clase
        mov dl, bl
        pop bx
        jnc .no
        ; detrás de la palabra, ¿qué hay? (saltando espacios)
        mov si, bx
        add si, cx
        push si
.esp:
        lodsb
        cmp al, ' '
        je .esp
        pop si
        mov dh, al                  ; DH = lo siguiente
        test dl, C_SIEMPRE
        jnz pon_esp
        test dl, C_SENT
        jz .no_sent
        cmp byte [v_ini_sent], 0
        je .no_sent
        cmp dh, '='
        jne pon_esp
.no_sent:
        test dl, C_TRAS | C_OPER
        jz .no_tras
        cmp byte [v_opnd], 0
        jne pon_esp
.no_tras:
        test dl, C_NO
        jz .no
        cmp byte [v_opnd], 0
        jne .no
        cmp dh, '='
        je .no
        cmp dh, ')'
        je .no
        cmp dh, ':'
        je .no
        cmp dh, ','
        je .no
        cmp dh, ';'
        je .no
        test dh, dh
        jz .no
        jmp pon_esp
.no:
        mov si, bx
        clc
        ret
pon_esp:                            ; AX = el código; SI detrás de la palabra
        cmp ax, 0x0093
        jne .no_listar
        mov byte [v_listar_es], 1
.no_listar:
        call pon_codigo
        stc
        ret

; =============================================================================
;  lista_linea: escribe la línea de DS:BX (la del enlace) como se lista
; =============================================================================
lista_linea:
        push bx
        push dx
        push si
        mov ax, [bx + 2]
        call pon_entero_sin
        mov al, ' '
        call pon_car
        lea si, [bx + 4]
        xor dx, dx                  ; DL: 1 entre comillas; DH: 1 tras REM
.c:
        lodsb
        test al, al
        jz .fin
        cmp dh, 1
        je .tal_cual
        jb .normal
        cmp al, '"'                 ; DATA: tal cual hasta un «:» sin comillas
        jne .dat1
        xor dl, 1
.dat1:
        test dl, dl
        jnz .tal_cual
        cmp al, ':'
        jne .tal_cual
        xor dh, dh
.normal:
        cmp al, '"'
        jne .no_com
        xor dl, 1
        jmp .tal_cual
.no_com:
        test dl, dl
        jnz .tal_cual
        cmp al, ':'
        jne .no_dp
        cmp byte [si], 0xA1         ; «:ELSE»: el «:» no se ve
        je .c
        cmp word [si], 0xD98F       ; «:REM'»: se ve «'»
        jne .tal_cual
        add si, 2
        mov al, "'"
        call pon_car
        mov dh, 1
        jmp .c
.no_dp:
        cmp al, 0x80
        jae .codigo
        cmp al, 0x20
        jae .tal_cual
        ; una constante
        dec si
        call lista_constante
        jmp .c
.tal_cual:
        call pon_car
        jmp .c
.codigo:
        cmp al, 0xB1                ; WHILE (B1 E9)
        jne .no_wh
        cmp byte [si], 0xE9
        jne .no_wh
        inc si
.no_wh:
        xor ah, ah
        cmp al, 0xFD
        jb .uno
        mov ah, al
        lodsb
        xchg al, ah
.uno:
        call lista_palabra
        cmp ax, 0x008F
        je .rem
        cmp ax, 0x0084
        jne .c
        mov dh, 2                   ; DATA: tal cual hasta «:»
        jmp .c
.rem:
        mov dh, 1
        jmp .c
.fin:
        call pon_nl
        pop si
        pop dx
        pop bx
        ret

; ---- escribe el código AX como palabra. En español, con espacios si
; hace falta para que se pueda volver a leer ----
lista_palabra:
        push bx
        push cx
        push si
        push dx
        cmp byte [v_idioma], 0
        je .ing
        mov dx, pal_esp
        call busca_codigo
        jnc .ing
        ; un espacio delante si lo anterior era letra o cifra
        push ax
        mov al, [v_ult_car]
        call es_alfanum
        pop ax
        jnc .sin_antes
        push ax
        mov al, ' '
        call pon_car
        pop ax
.sin_antes:
        call pon_pal_tabla
        ; y detrás, si lo que sigue es letra, cifra o una constante
        pop dx
        pop si
        push si
        push dx
        mov al, [si]
        cmp al, 0x20
        jb .espacio
        cmp al, 0x80
        jb .no_cod
        cmp al, 0xE6                ; otra palabra (no un signo): espacio
        jb .espacio
        cmp al, 0xEE
        jb .x
        cmp al, 0xF4
        jb .espacio
        jmp .x
.no_cod:
        call es_alfanum
        jnc .x
.espacio:
        test al, al
        jz .x
        mov al, ' '
        call pon_car
        jmp .x
.ing:
        mov dx, pal_ing
        call busca_codigo
        jnc .desconocido
        call pon_pal_tabla
        jmp .x
.desconocido:
        mov al, '?'
        call pon_car
.x:
        pop dx
        pop si
        pop cx
        pop bx
        ret

; ---- busca el código AX en la tabla CS:DX: CF=1 y CS:SI = la entrada ----
busca_codigo:
        mov si, dx
.e:
        mov cl, [cs:si]
        test cl, cl
        jz .no
        xor ch, ch
        mov bx, si
        add bx, cx
        cmp [cs:bx + 1], ax
        je .si
        add si, cx
        add si, 3
        cmp dx, pal_esp
        jne .e
        inc si
        jmp .e
.si:
        stc
        ret
.no:
        clc
        ret
; escribe el texto de la entrada CS:SI
pon_pal_tabla:
        mov cl, [cs:si]
        xor ch, ch
        inc si
.c:
        mov al, [cs:si]
        inc si
        call pon_car
        loop .c
        ret

; ---- una constante del programa (DS:SI en su código) ----
lista_constante:
        push bx
        push cx
        push dx
        call .una
        pop dx
        pop cx
        pop bx
        ret
.una:
        lodsb
        cmp al, 0x0E
        je .linea
        cmp al, 0x0C
        je .hex
        cmp al, 0x0B
        je .oct
        cmp al, 0x0F
        je .byte
        cmp al, 0x1C
        je .ent
        cmp al, 0x1D
        je .sim
        cmp al, 0x1F
        je .dob
        cmp al, 0x11
        jb .raro
        cmp al, 0x1B
        ja .raro
        sub al, 0x11
        xor ah, ah
        jmp pon_entero_sin
.raro:
        mov al, '?'
        jmp pon_car
.linea:
        lodsw
        jmp pon_natural
.byte:
        lodsb
        xor ah, ah
        jmp pon_entero_sin
.ent:
        lodsw
        jmp pon_entero_sin
.hex:
        mov al, '&'
        call pon_car
        mov al, 'H'
        call pon_car
        lodsw
        mov bx, 16
        jmp pon_en_base
.oct:
        mov al, '&'
        call pon_car
        mov al, 'O'
        call pon_car
        lodsw
        mov bx, 8
        jmp pon_en_base
.sim:
        call mbf4_a_ac
        add si, 4
        push si
        call num_a_texto            ; b_num, sin espacios
        mov si, b_num
        call lista_num_txt
        ; si se leería como entero, «!» detrás
        mov si, b_num
        call parece_entero
        jnc .s_x
        mov al, '!'
        call pon_car
.s_x:
        pop si
        ret
.dob:
        call mbf8_a_ac
        add si, 8
        push si
        call num_a_texto
        mov si, b_num
        call lista_num_txt
        mov si, b_num
        call parece_doble
        jc .d_x
        mov al, '#'
        call pon_car
.d_x:
        pop si
        ret

; escribe el número de b_num quitándole el espacio de delante
lista_num_txt:
        cmp byte [si], ' '
        jne .c
        inc si
.c:
        lodsb
        test al, al
        jz .x
        call pon_car
        jmp .c
.x:
        ret
; ¿el texto de DS:SI se leería como entero? (solo cifras, y menos de 32768)
parece_entero:
        cmp byte [si], ' '
        jne .c0
        inc si
.c0:
        xor dx, dx
.c:
        lodsb
        test al, al
        jz .si
        call es_cifra
        jnc .no
        sub al, '0'
        cbw
        xchg ax, dx
        mov cx, 10
        push dx
        mul cx
        pop dx
        jc .no
        add dx, ax
        jc .no
        js .no
        jmp .c
.si:
        stc
        ret
.no:
        clc
        ret
; ¿el texto de DS:SI ya se leería como doble? (lleva D o más de 7 cifras)
parece_doble:
        xor cx, cx
.c:
        lodsb
        test al, al
        jz .fin
        cmp al, 'D'
        je .si
        call es_cifra
        jnc .c
        inc cx
        jmp .c
.fin:
        cmp cx, 7
        ja .si
        clc
        ret
.si:
        stc
        ret
