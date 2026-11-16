; =============================================================================
;  desinfla.inc - descompresor Deflate (RFC 1951) con el envoltorio zlib
;  (RFC 1950), para cualquier programa de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo que casi todo el mundo usa para comprimir: los flujos de un PDF
;  (/FlateDecode), las imágenes PNG, los .ZIP y los .GZ llevan dentro datos
;  «Deflate». Aquí está entero: bloques sin comprimir, con los códigos de
;  Huffman fijos y con códigos propios (dinámicos), y la ventana de 32 K (que
;  es el propio destino: todo se descomprime de una vez, de memoria a memoria).
;
;  No depende de nada: ni de Hispa Ventanas ni de quien lo incluye. Se pone
;  dentro del código (%include "desinfla.inc") y, en la «section .bss», la
;  macro DESINFLA_BSS (unos 1.200 octetos). No es reentrante.
;
;    desinfla       ESI=datos comprimidos (Deflate a secas)  ECX=cuántos octetos
;                   EDI=destino                              EDX=lo que cabe
;                   -> EAX=octetos escritos en el destino
;                      CF=0: ha llegado al final de los datos comprimidos
;                      CF=1: no ha podido; en [dsf_error] el porqué (DSF_E_*).
;                            Lo escrito hasta ahí (EAX) es bueno: quien quiera
;                            puede aprovecharlo
;                   Conserva los demás registros.
;    desinfla_zlib  lo mismo, con el envoltorio zlib: dos octetos delante y la
;                   suma Adler-32 detrás. Una cabecera que no es de zlib, o que
;                   pide un diccionario, es DSF_E_DATOS. La suma no es un error
;                   (hay muchos archivos con la suma cortada que por lo demás
;                   están bien): queda en [dsf_suma] 0 si coincide, 1 si no
;                   coincide y 2 si falta
;    [dsf_usado]    cuántos octetos de la entrada ha gastado (con la cabecera
;                   y la suma, si las había)
;
;  Cómo está hecho (como «puff», de Mark Adler: lo más sencillo que es
;  correcto). Los bits se sacan de uno en uno o a puñados de un registro que
;  se rellena por octetos. De cada código de Huffman se guarda cuántos
;  símbolos hay de cada largo y los símbolos en orden; se descifra bit a bit,
;  comparando con el primer código de cada largo (códigos canónicos). No
;  hacen falta tablas grandes. Todo se comprueba: que queden datos, que
;  quepa lo que sale, que las distancias no apunten antes del principio y que
;  los códigos estén bien hechos; con datos rotos da error, nunca escribe
;  fuera del destino ni se queda dando vueltas.
; =============================================================================
DSF_E_DATOS equ 1                   ; los datos no son Deflate o están rotos
DSF_E_CORTO equ 2                   ; se acaban antes del final (archivo cortado)
DSF_E_LLENO equ 3                   ; no cabe en el destino

DSF_MAXL    equ 286                 ; códigos de letra o largo
DSF_MAXD    equ 30                  ; códigos de distancia

%macro DESINFLA_BSS 0
            alignb 4
dsf_error   resd 1
dsf_usado   resd 1
dsf_suma    resd 1
dsf_esp     resd 1                  ; la pila de la entrada, para salir de un error
dsf_in_fin  resd 1
dsf_out_ini resd 1
dsf_out_fin resd 1
dsf_out_p   resd 1                  ; hasta dónde se ha escrito (para salir de un error)
dsf_n       resd 1
dsf_nl      resd 1                  ; bloque dinámico: cuántos códigos de cada clase
dsf_nd      resd 1
dsf_fijos   resd 1                  ; 1: las tablas tienen ahora los códigos fijos
dsf_lcnt    resw 16                 ; letras y largos: cuántos códigos de cada largo
dsf_lsim    resw 288                ; (pegado detrás) y sus símbolos, en orden
dsf_dcnt    resw 16                 ; distancias: lo mismo
dsf_dsim    resw 32
dsf_ccnt    resw 16                 ; el código de los largos de código
dsf_csim    resw 20
dsf_offs    resw 16
dsf_largos  resb 320                ; el largo del código de cada símbolo
%endmacro

; el largo que dice cada símbolo 257-285 y cuántos bits más lleva
dsf_lbase   dw 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31
            dw 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
dsf_lext    db 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2
            db 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
; y la distancia de cada símbolo 0-29
dsf_dbase   dw 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193
            dw 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145
            dw 8193, 12289, 16385, 24577
dsf_dext    db 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6
            db 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
; en qué orden vienen los largos del código de largos
dsf_orden   db 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15

; -----------------------------------------------------------------------------
desinfla_zlib:
        pushad
        mov dword [dsf_suma], 2
        mov dword [dsf_usado], 0
        mov dword [dsf_error], DSF_E_CORTO
        cmp ecx, 2
        jb .mal0
        mov dword [dsf_error], DSF_E_DATOS
        movzx eax, byte [esi]       ; CMF: método 8 (Deflate), ventana de hasta 32 K
        mov ebx, eax
        and ebx, 0x0F
        cmp ebx, 8
        jne .mal0
        cmp eax, 0x80
        jae .mal0
        movzx ebx, byte [esi+1]     ; FLG: sin diccionario, y los dos octetos
        test bl, 0x20               ; juntos, múltiplo de 31
        jnz .mal0
        shl eax, 8
        or eax, ebx
        xor edx, edx
        mov ebx, 31
        div ebx
        test edx, edx
        jnz .mal0
        mov edx, [esp+20]           ; (EDX de la entrada)
        add esi, 2
        sub ecx, 2
        call desinfla
        mov [esp+28], eax
        pushf
        add dword [dsf_usado], 2
        popf
        jc .mal
        ; la suma Adler-32 de lo que ha salido, contra los cuatro octetos de detrás
        mov ebx, [esp+24]           ; (ECX de la entrada)
        sub ebx, [dsf_usado]
        cmp ebx, 4
        jb .bien                    ; falta: [dsf_suma] se queda en 2
        mov esi, [esp+4]            ; (ESI de la entrada)
        add esi, [dsf_usado]
        mov ebx, [esi]              ; va con el octeto alto primero
        xchg bl, bh
        rol ebx, 16
        xchg bl, bh
        add dword [dsf_usado], 4
        mov esi, [esp]              ; (EDI de la entrada: el destino)
        mov ecx, eax
        call dsf_adler
        cmp eax, ebx
        setne al
        movzx eax, al
        mov [dsf_suma], eax
.bien:
        popad
        clc
        ret
.mal0:
        mov dword [esp+28], 0
.mal:
        popad
        stc
        ret

; (interna) ESI=datos ECX=cuántos -> EAX=su suma Adler-32
dsf_adler:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov edi, 1                  ; a
        xor ebp, ebp                ; b
.tanda:
        test ecx, ecx
        jz .fin
        mov edx, ecx                ; de 5.552 en 5.552 no se desborda
        cmp edx, 5552
        jbe .t1
        mov edx, 5552
.t1:
        sub ecx, edx
        xor eax, eax
.o:
        lodsb
        add edi, eax
        add ebp, edi
        dec edx
        jnz .o
        mov ebx, 65521
        mov eax, edi
        xor edx, edx
        div ebx
        mov edi, edx
        mov eax, ebp
        xor edx, edx
        div ebx
        mov ebp, edx
        jmp .tanda
.fin:
        mov eax, ebp
        shl eax, 16
        or eax, edi
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; -----------------------------------------------------------------------------
;  Dentro: ESI=entrada EDI=salida EBX=los bits que quedan del último octeto
;  leído (y de los siguientes) EDX=cuántos son
; -----------------------------------------------------------------------------
desinfla:
        pushad
        mov [dsf_esp], esp
        mov dword [dsf_error], 0
        mov dword [dsf_fijos], 0
        mov [dsf_out_ini], edi
        mov [dsf_out_p], edi
        add edx, edi
        jnc .d1
        mov edx, 0xFFFFFFFF         ; (no puede dar la vuelta a la memoria)
.d1:
        mov [dsf_out_fin], edx
        add ecx, esi
        jnc .d2
        mov ecx, 0xFFFFFFFF
.d2:
        mov [dsf_in_fin], ecx
        xor ebx, ebx
        xor edx, edx
.bloque:
        mov ecx, 1                  ; ¿es el último?
        call dsf_bits
        push eax
        mov ecx, 2                  ; la clase del bloque
        call dsf_bits
        cmp eax, 1
        jb .guardado
        je .fijo
        cmp eax, 2
        jne dsf_mal_datos           ; (la clase 3 no existe)
        call dsf_dinamico
        jmp .codigos
.fijo:
        call dsf_pon_fijos
.codigos:
        call dsf_codigos
        jmp .sig
.guardado:
        xor ebx, ebx                ; lo que queda del octeto a medias no vale
        xor edx, edx
        mov eax, [dsf_in_fin]
        sub eax, esi
        cmp eax, 4
        jb dsf_mal_corto
        movzx ecx, word [esi]       ; el largo y, para comprobar, su complemento
        movzx eax, word [esi+2]
        xor eax, 0xFFFF
        cmp eax, ecx
        jne dsf_mal_datos
        add esi, 4
        mov eax, [dsf_in_fin]
        sub eax, esi
        cmp eax, ecx
        jb .g_corto
        mov eax, [dsf_out_fin]
        sub eax, edi
        cmp eax, ecx
        jb .g_lleno
        rep movsb
        mov [dsf_out_p], edi
.sig:
        pop eax
        test eax, eax
        jz .bloque
        clc
        jmp dsf_sale
.g_corto:
        mov ecx, eax                ; lo que haya, se aprovecha
        mov eax, [dsf_out_fin]
        sub eax, edi
        cmp eax, ecx
        jae .g1
        mov ecx, eax
.g1:
        rep movsb
        mov [dsf_out_p], edi
        jmp dsf_mal_corto
.g_lleno:
        mov ecx, eax
        rep movsb
        mov [dsf_out_p], edi
        jmp dsf_mal_lleno

; las salidas con error: desde cualquier profundidad
dsf_mal_datos:
        mov dword [dsf_error], DSF_E_DATOS
        jmp dsf_mal
dsf_mal_corto:
        mov dword [dsf_error], DSF_E_CORTO
        jmp dsf_mal
dsf_mal_lleno:
        mov dword [dsf_error], DSF_E_LLENO
dsf_mal:
        mov edi, [dsf_out_p]        ; (EDI puede estar en otro uso)
        stc
dsf_sale:
        mov esp, [dsf_esp]
        pushf
        mov eax, edi
        sub eax, [dsf_out_ini]
        mov [esp+4+28], eax
        mov eax, esi
        sub eax, [esp+4+4]
        mov [dsf_usado], eax
        popf
        popad
        ret

; ECX=cuántos bits (de 0 a 16) -> EAX=su valor (el primero leído, el de menos peso)
dsf_bits:
.mas:
        cmp edx, ecx
        jae .hay
        cmp esi, [dsf_in_fin]
        jae dsf_mal_corto
        movzx eax, byte [esi]
        inc esi
        xchg ecx, edx
        shl eax, cl
        xchg ecx, edx
        or ebx, eax
        add edx, 8
        jmp .mas
.hay:
        mov eax, 1
        shl eax, cl
        dec eax
        and eax, ebx
        shr ebx, cl
        sub edx, ecx
        ret

; EBP=un código (16 cuentas y, detrás, los símbolos) -> EAX=el símbolo que viene
dsf_simbolo:
        push ecx
        push edi
        push ebp
        xor eax, eax                ; el código leído hasta ahora
        xor ecx, ecx                ; el primer código de este largo
        xor edi, edi                ; cuántos símbolos hay de los largos menores
        mov dword [dsf_n], 15
.b:
        add ebp, 2
        test edx, edx
        jnz .hay
        cmp esi, [dsf_in_fin]
        jae dsf_mal_corto
        movzx ebx, byte [esi]
        inc esi
        mov edx, 8
.hay:
        shr ebx, 1
        adc eax, eax
        dec edx
        push edx
        movzx edx, word [ebp]       ; cuántos hay de este largo
        push eax
        sub eax, edx
        cmp eax, ecx
        pop eax
        jl .es
        add edi, edx
        add ecx, edx
        shl ecx, 1
        pop edx
        dec dword [dsf_n]
        jnz .b
        jmp dsf_mal_datos           ; se han acabado los largos: no es un código
.es:
        pop edx
        sub eax, ecx
        add eax, edi
        pop ebp
        movzx eax, word [ebp+32+eax*2]
        pop edi
        pop ecx
        ret

; Hace un código: ESI=los largos (un octeto por símbolo) ECX=cuántos símbolos
; EBP=dónde (16 cuentas + símbolos) -> EAX: 0 si el código está completo,
; negativo si sobran códigos (imposible), positivo si faltan (incompleto)
dsf_construye:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edi, ebp
        push ecx
        xor eax, eax
        mov ecx, 16
        rep stosw
        pop ecx
        xor ebx, ebx
.cuenta:
        movzx eax, byte [esi+ebx]
        inc word [ebp+eax*2]
        inc ebx
        cmp ebx, ecx
        jb .cuenta
        xor eax, eax
        movzx edx, word [ebp]
        cmp edx, ecx
        je .x                       ; ningún código: vale (no se usará)
        mov eax, 1                  ; los códigos que quedan libres
        mov ebx, 1
.libres:
        shl eax, 1
        movzx edx, word [ebp+ebx*2]
        sub eax, edx
        js .x
        inc ebx
        cmp ebx, 16
        jb .libres
        push eax
        mov word [dsf_offs+2], 0    ; dónde empiezan los símbolos de cada largo
        mov ebx, 1
.offs:
        mov ax, [dsf_offs+ebx*2]
        add ax, [ebp+ebx*2]
        mov [dsf_offs+ebx*2+2], ax
        inc ebx
        cmp ebx, 15
        jb .offs
        xor ebx, ebx
.pon:
        movzx eax, byte [esi+ebx]
        test eax, eax
        jz .pon1
        movzx edx, word [dsf_offs+eax*2]
        inc word [dsf_offs+eax*2]
        mov [ebp+32+edx*2], bx
.pon1:
        inc ebx
        cmp ebx, ecx
        jb .pon
        pop eax
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; los códigos fijos: letras 0-143 de 8 bits, 144-255 de 9, 256-279 de 7 y
; 280-287 de 8; las 30 distancias, de 5
dsf_pon_fijos:
        cmp dword [dsf_fijos], 0
        jne .x
        push esi
        push edi
        push ecx
        push eax
        push ebp
        mov edi, dsf_largos
        mov al, 8
        mov ecx, 144
        rep stosb
        mov al, 9
        mov ecx, 112
        rep stosb
        mov al, 7
        mov ecx, 24
        rep stosb
        mov al, 8
        mov ecx, 8
        rep stosb
        mov esi, dsf_largos
        mov ecx, 288
        mov ebp, dsf_lcnt
        call dsf_construye
        mov edi, dsf_largos
        mov al, 5
        mov ecx, DSF_MAXD
        rep stosb
        mov esi, dsf_largos
        mov ecx, DSF_MAXD
        mov ebp, dsf_dcnt
        call dsf_construye
        mov dword [dsf_fijos], 1
        pop ebp
        pop eax
        pop ecx
        pop edi
        pop esi
.x:
        ret

; un bloque con códigos propios: lee su descripción y hace las dos tablas
dsf_dinamico:
        mov dword [dsf_fijos], 0
        mov ecx, 5
        call dsf_bits
        add eax, 257
        mov [dsf_nl], eax
        mov ecx, 5
        call dsf_bits
        inc eax
        mov [dsf_nd], eax
        mov ecx, 4
        call dsf_bits
        add eax, 4
        cmp dword [dsf_nl], DSF_MAXL
        ja dsf_mal_datos
        cmp dword [dsf_nd], DSF_MAXD
        ja dsf_mal_datos
        push edi
        push eax
        mov edi, dsf_largos         ; el código de los largos: 19 símbolos, de
        xor eax, eax                ; hasta 7 bits, en su orden raro
        mov ecx, 19
        rep stosb
        pop ebp                     ; EBP=cuántos vienen
        xor edi, edi
.cl:
        mov ecx, 3
        call dsf_bits
        movzx ecx, byte [dsf_orden+edi]
        mov [dsf_largos+ecx], al
        inc edi
        cmp edi, ebp
        jb .cl
        push esi
        mov esi, dsf_largos
        mov ecx, 19
        mov ebp, dsf_ccnt
        call dsf_construye
        pop esi
        test eax, eax
        jnz dsf_mal_datos           ; tiene que estar completo
        ; los largos de las letras y de las distancias, seguidos
        xor edi, edi                ; EDI=cuántos van
.lee:
        mov eax, [dsf_nl]
        add eax, [dsf_nd]
        cmp edi, eax
        jae .leidos
        mov ebp, dsf_ccnt
        call dsf_simbolo
        cmp eax, 16
        jae .repite
        mov [dsf_largos+edi], al
        inc edi
        jmp .lee
.repite:
        jne .r17
        test edi, edi               ; 16: el anterior, de 3 a 6 veces
        jz dsf_mal_datos
        mov ecx, 2
        call dsf_bits
        lea ecx, [eax+3]
        mov al, [dsf_largos+edi-1]
        jmp .rep
.r17:
        cmp eax, 17
        jne .r18
        mov ecx, 3                  ; 17: de 3 a 10 ceros
        call dsf_bits
        lea ecx, [eax+3]
        xor al, al
        jmp .rep
.r18:
        mov ecx, 7                  ; 18: de 11 a 138 ceros
        call dsf_bits
        lea ecx, [eax+11]
        xor al, al
.rep:
        mov ebp, [dsf_nl]
        add ebp, [dsf_nd]
        sub ebp, edi
        cmp ecx, ebp
        ja dsf_mal_datos            ; más de los que hay
.rep1:
        mov [dsf_largos+edi], al
        inc edi
        dec ecx
        jnz .rep1
        jmp .lee
.leidos:
        cmp byte [dsf_largos+256], 0
        je dsf_mal_datos            ; sin el símbolo de fin no se acabaría nunca
        push esi
        mov esi, dsf_largos
        mov ecx, [dsf_nl]
        mov ebp, dsf_lcnt
        call dsf_construye
        pop esi
        test eax, eax               ; incompleto solo vale con un único código
        jz .l_ok
        js dsf_mal_datos
        movzx eax, word [dsf_lcnt]
        add ax, [dsf_lcnt+2]
        cmp eax, [dsf_nl]
        jne dsf_mal_datos
.l_ok:
        push esi
        mov esi, dsf_largos
        add esi, [dsf_nl]
        mov ecx, [dsf_nd]
        mov ebp, dsf_dcnt
        call dsf_construye
        pop esi
        test eax, eax
        jz .d_ok
        js dsf_mal_datos
        movzx eax, word [dsf_dcnt]
        add ax, [dsf_dcnt+2]
        cmp eax, [dsf_nd]
        jne dsf_mal_datos
.d_ok:
        pop edi
        ret

; los datos de un bloque comprimido, con las tablas que haya puestas
dsf_codigos:
.b:
        mov ebp, dsf_lcnt
        call dsf_simbolo
        cmp eax, 256
        jae .largo
        cmp edi, [dsf_out_fin]      ; una letra
        jae dsf_mal_lleno
        stosb
        mov [dsf_out_p], edi
        jmp .b
.largo:
        je .fin
        sub eax, 257                ; un trozo que ya ha salido: largo y distancia
        cmp eax, 29
        jae dsf_mal_datos
        movzx ecx, byte [dsf_lext+eax]
        movzx ebp, word [dsf_lbase+eax*2]
        call dsf_bits
        add ebp, eax
        push ebp
        mov ebp, dsf_dcnt
        call dsf_simbolo
        cmp eax, DSF_MAXD
        jae dsf_mal_datos
        movzx ecx, byte [dsf_dext+eax]
        movzx ebp, word [dsf_dbase+eax*2]
        call dsf_bits
        add ebp, eax                ; EBP=la distancia
        pop ecx                     ; ECX=el largo
        mov eax, edi
        sub eax, [dsf_out_ini]
        cmp ebp, eax
        ja dsf_mal_datos            ; antes del principio de lo que ha salido
        mov eax, [dsf_out_fin]
        sub eax, edi
        cmp ecx, eax
        jbe .cabe
        mov ecx, eax                ; no cabe entero: lo que quepa, y error
        push dword 1
        jmp .copia
.cabe:
        push dword 0
.copia:
        push esi
        mov esi, edi
        sub esi, ebp
        rep movsb                   ; (de octeto en octeto: pueden solaparse)
        mov [dsf_out_p], edi
        pop esi
        pop eax
        test eax, eax
        jnz dsf_mal_lleno
        jmp .b
.fin:
        ret
