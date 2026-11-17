; =============================================================================
;  desinfla_t.inc - descompresor Deflate (RFC 1951) y zlib (RFC 1950) POR
;  TRAMOS, para cualquier programa de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hace lo mismo que desinfla.inc (bloques sin comprimir, con códigos fijos y
;  con códigos propios), pero sin tener a la vez en memoria ni todos los datos
;  comprimidos ni todo lo que sale: la entrada se le da a trozos, según se va
;  leyendo del disco, y la salida se le pide a trozos, según hace falta (una
;  fila de un PNG cada vez). Entre trozo y trozo se queda donde iba: con los
;  bits que le sobran, a medio bloque o a medio copiar. Así una imagen PNG de
;  muchos megas se ve con unos pocos K.
;
;  No depende de nada: ni de Hispa Ventanas ni de quien lo incluye (tampoco
;  de desinfla.inc: pueden ir los dos en el mismo programa). Se pone dentro
;  del código (%include "desinfla_t.inc") y, en la «section .bss», la macro
;  DESINFLA_T_BSS (unos 1.300 octetos). La memoria de trabajo (DST_MEM
;  octetos: la ventana de 32 K y una pila propia) la da quien lo usa. No es
;  reentrante: un flujo cada vez.
;
;    dst_empieza   EDI=memoria de trabajo (DST_MEM octetos)
;                  EAX=0: Deflate a secas; 1: con el envoltorio zlib (dos
;                  octetos delante y la suma Adler-32 detrás)
;                  Conserva los registros.
;    dst_entrada   ESI=datos comprimidos  ECX=cuántos octetos (0: no hay más)
;                  El trozo siguiente de la entrada. Se llama al principio y
;                  cada vez que dst_sigue pide más (DST_HAMBRE): no antes. Los
;                  datos tienen que seguir en su sitio hasta que se pidan más.
;                  Conserva los registros.
;    dst_sigue     EDI=destino  ECX=cuántos octetos se quieren
;                  -> EAX=cuántos ha escrito   EDX=por qué ha parado:
;                     DST_HECHO   ha escrito todos los que se pedían
;                     DST_HAMBRE  se ha quedado sin entrada: dst_entrada con el
;                                 trozo siguiente y otra vez dst_sigue, con lo
;                                 que falte (EDI más adelante, ECX menor)
;                     DST_FIN     los datos comprimidos se han acabado, bien
;                     DST_ERROR   no puede seguir; en [dst_error] el porqué:
;                                 DST_E_DATOS (no son Deflate o están rotos) o
;                                 DST_E_CORTO (se acaban antes del final)
;                  Lo escrito (EAX) es siempre bueno. Tras DST_FIN o DST_ERROR
;                  ya no da más: devuelve lo mismo, con EAX=0.
;                  Conserva los demás registros.
;    [dst_suma]    con zlib, tras DST_FIN: 0 si la suma Adler-32 coincide, 1
;                  si no coincide y 2 si falta (no es un error: hay muchos
;                  archivos con la suma cortada que por lo demás están bien)
;    [dst_usado]   tras DST_FIN: octetos de la entrada que ha gastado (con la
;                  cabecera y la suma, si las había)
;
;  Cómo está hecho. El descompresor es el de desinfla.inc (como «puff», de
;  Mark Adler: bit a bit, con códigos canónicos y sin tablas grandes), escrito
;  de corrido, como si tuviera todos los datos delante. Lo que lo hace «por
;  tramos» es que corre en su propia pila, como una corrutina: cuando le
;  falta un octeto de entrada, o cuando ha llenado lo que se le pidió, guarda
;  sus registros en su pila y vuelve a la de quien llamó («dst_cede»); la
;  llamada siguiente a dst_sigue lo deja seguir justo donde estaba. Su estado
;  a medias son, pues, sus registros y su pila: no hay que apuntar a mano en
;  qué paso iba. Lo que sale se escribe en el destino y, a la vez, en una
;  ventana circular de 32 K, de la que salen las copias («repite los N de
;  hace D»), porque el destino de antes ya no está.
;
;  Todo se comprueba, como en desinfla.inc: que las distancias no apunten
;  antes del principio, que los códigos estén bien hechos; con datos rotos da
;  error, nunca lee fuera del trozo que se le ha dado, ni escribe fuera de lo
;  que se le pide, ni se queda dando vueltas. La pila propia (4 K) es mucho
;  más de lo que gasta: lo demás es para las interrupciones que caigan
;  mientras corre.
; =============================================================================
DST_HECHO   equ 0
DST_HAMBRE  equ 1
DST_FIN     equ 2
DST_ERROR   equ 3

DST_E_DATOS equ 1                   ; (los mismos números que en desinfla.inc)
DST_E_CORTO equ 2

DST_VENTANA equ 32768
DST_PILA    equ 4096
DST_MEM     equ DST_VENTANA + DST_PILA

DST_MAXL    equ 286                 ; códigos de letra o largo
DST_MAXD    equ 30                  ; códigos de distancia

%macro DESINFLA_T_BSS 0
            alignb 4
dst_error   resd 1
dst_usado   resd 1
dst_suma    resd 1
dst_final   resd 1                  ; DST_FIN o DST_ERROR cuando ha acabado (0: sigue)
dst_zlib    resd 1
dst_sin_mas resd 1                  ; se ha dicho que no hay más entrada
dst_esp_fuera resd 1                ; la pila de quien llama, mientras corre
dst_esp_dentro resd 1               ; y la suya, mientras espera
dst_pila_tope resd 1
dst_in      resd 1                  ; el trozo de entrada de ahora: por dónde va
dst_in_fin  resd 1
dst_in_ini  resd 1
dst_gastado resd 1                  ; octetos de los trozos anteriores
dst_out     resd 1                  ; el destino de ahora: por dónde va
dst_out_fin resd 1
dst_out_ini resd 1
dst_ven     resd 1                  ; la ventana, su final y por dónde se va
dst_ven_fin resd 1
dst_vp      resd 1
dst_total   resd 1                  ; octetos que han salido (para las distancias: hasta 32 K)
dst_ad_a    resd 1                  ; la suma Adler-32 de lo que ha salido
dst_ad_b    resd 1
dst_ad_dice resd 1                  ; y la que dicen los datos
dst_n       resd 1
dst_nl      resd 1                  ; bloque dinámico: cuántos códigos de cada clase
dst_nd      resd 1
dst_fijos   resd 1                  ; 1: las tablas tienen ahora los códigos fijos
dst_lcnt    resw 16                 ; letras y largos: cuántos códigos de cada largo
dst_lsim    resw 288                ; (pegado detrás) y sus símbolos, en orden
dst_dcnt    resw 16                 ; distancias: lo mismo
dst_dsim    resw 32
dst_ccnt    resw 16                 ; el código de los largos de código
dst_csim    resw 20
dst_offs    resw 16
dst_largos  resb 320                ; el largo del código de cada símbolo
%endmacro

; el largo que dice cada símbolo 257-285 y cuántos bits más lleva
dst_lbase   dw 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31
            dw 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
dst_lext    db 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2
            db 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
; y la distancia de cada símbolo 0-29
dst_dbase   dw 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193
            dw 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145
            dw 8193, 12289, 16385, 24577
dst_dext    db 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6
            db 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
; en qué orden vienen los largos del código de largos
dst_orden   db 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15

; -----------------------------------------------------------------------------
;  Lo que llama quien lo usa
; -----------------------------------------------------------------------------
dst_empieza:
        pushad
        mov [dst_zlib], eax
        xor eax, eax
        mov [dst_error], eax
        mov [dst_usado], eax
        mov [dst_final], eax
        mov [dst_sin_mas], eax
        mov [dst_in], eax
        mov [dst_in_fin], eax
        mov [dst_in_ini], eax
        mov [dst_gastado], eax
        mov [dst_total], eax
        mov [dst_fijos], eax
        mov [dst_ad_b], eax
        mov dword [dst_ad_a], 1
        mov dword [dst_suma], 2
        mov [dst_ven], edi
        mov [dst_vp], edi
        add edi, DST_VENTANA
        mov [dst_ven_fin], edi
        ; su pila, preparada para que la primera vez «vuelva» al principio
        ; del descompresor: ocho registros (los que sean) y esa dirección
        add edi, DST_PILA
        and edi, ~3
        mov [dst_pila_tope], edi
        sub edi, 4
        mov dword [edi], dst_cuerpo
        sub edi, 32
        mov [dst_esp_dentro], edi
        popad
        ret

dst_entrada:
        push eax
        mov eax, [dst_in]           ; lo del trozo anterior, a la cuenta
        sub eax, [dst_in_ini]
        add [dst_gastado], eax
        mov [dst_in], esi
        mov [dst_in_ini], esi
        lea eax, [esi+ecx]
        mov [dst_in_fin], eax
        test ecx, ecx
        jnz .x
        mov dword [dst_sin_mas], 1
.x:
        pop eax
        ret

dst_sigue:
        pushad
        mov edx, [dst_final]        ; ya ha acabado: lo mismo, y nada más
        test edx, edx
        jnz .nada
        test ecx, ecx
        jz .nada                    ; (no se pide nada: hecho)
        mov [dst_out], edi
        mov [dst_out_ini], edi
        add ecx, edi
        mov [dst_out_fin], ecx
        mov [dst_esp_fuera], esp    ; a su pila: sigue donde estaba
        mov esp, [dst_esp_dentro]
        popad
        ret
.nada:
        mov dword [esp+28], 0
        mov [esp+20], edx
        popad
        ret

; (interna; la llama el descompresor, en su pila) EAX=por qué para: guarda
; cómo va y vuelve a quien llamó a dst_sigue. Cuando se le deja seguir,
; vuelve de aquí con todos sus registros como estaban
dst_cede:
        pushad
        mov [dst_esp_dentro], esp
        mov esp, [dst_esp_fuera]
        ; ---- otra vez en la pila de quien llamó, dentro de dst_sigue ----
        mov [esp+20], eax           ; (su EDX)
        mov esi, [dst_out_ini]
        mov ecx, [dst_out]
        sub ecx, esi
        mov [esp+28], ecx           ; (su EAX: lo escrito)
        cmp dword [dst_zlib], 0
        je .sale
        call dst_adler              ; la suma, con lo que ha salido esta vez
        cmp eax, DST_FIN
        jne .sale
        cmp dword [dst_suma], 3     ; (3: los datos traían la suya; hay que compararla)
        jne .sale
        mov eax, [dst_ad_b]
        shl eax, 16
        or eax, [dst_ad_a]
        cmp eax, [dst_ad_dice]
        setne al
        movzx eax, al
        mov [dst_suma], eax
.sale:
        popad
        ret

; (interna) ESI=datos ECX=cuántos: los suma a la Adler-32 que se lleva
dst_adler:
        pushad
        mov edi, [dst_ad_a]
        mov ebp, [dst_ad_b]
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
        mov [dst_ad_a], edi
        mov [dst_ad_b], ebp
        popad
        ret

; -----------------------------------------------------------------------------
;  El descompresor (corre en su pila). ESI=entrada EBX=los bits que quedan
;  del último octeto leído (y de los siguientes) EDX=cuántos son
; -----------------------------------------------------------------------------
dst_cuerpo:
        xor ebx, ebx
        xor edx, edx
        mov esi, [dst_in]
        cmp dword [dst_zlib], 0
        je .bloque
        ; la cabecera de zlib. CMF: método 8 (Deflate), ventana de hasta 32 K.
        ; FLG: sin diccionario, y los dos octetos juntos, múltiplo de 31
        call dst_octeto
        mov ecx, eax
        call dst_octeto
        mov edi, eax
        mov eax, ecx
        and eax, 0x0F
        cmp eax, 8
        jne dst_mal_datos
        cmp ecx, 0x80
        jae dst_mal_datos
        test edi, 0x20
        jnz dst_mal_datos
        mov eax, ecx
        shl eax, 8
        or eax, edi
        push edx
        xor edx, edx
        mov ecx, 31
        div ecx
        test edx, edx
        pop edx
        jnz dst_mal_datos
.bloque:
        mov ecx, 1                  ; ¿es el último?
        call dst_bits
        push eax
        mov ecx, 2                  ; la clase del bloque
        call dst_bits
        cmp eax, 1
        jb .guardado
        je .fijo
        cmp eax, 2
        jne dst_mal_datos           ; (la clase 3 no existe)
        call dst_dinamico
        jmp .codigos
.fijo:
        call dst_pon_fijos
.codigos:
        call dst_codigos
        jmp .sig
.guardado:
        xor ebx, ebx                ; lo que queda del octeto a medias no vale
        xor edx, edx
        call dst_octeto             ; el largo y, para comprobar, su complemento
        mov ecx, eax
        call dst_octeto
        mov ch, al
        call dst_octeto
        mov edi, eax
        call dst_octeto
        shl eax, 8
        or eax, edi
        xor eax, 0xFFFF
        cmp eax, ecx
        jne dst_mal_datos
        test ecx, ecx
        jz .sig
.g1:
        call dst_octeto
        call dst_pon
        dec ecx
        jnz .g1
.sig:
        pop eax
        test eax, eax
        jz .bloque
        ; ---- el final: lo gastado y, con zlib, la suma que traen los datos ----
        mov eax, esi
        sub eax, [dst_in_ini]
        add eax, [dst_gastado]
        mov [dst_usado], eax
        cmp dword [dst_zlib], 0
        je .fin
        xor edi, edi                ; va con el octeto alto primero
        mov ecx, 4
.suma:
        call dst_octeto_si
        jc .fin                     ; falta (entera o un trozo): [dst_suma] se queda en 2
        shl edi, 8
        or edi, eax
        dec ecx
        jnz .suma
        mov [dst_ad_dice], edi
        mov dword [dst_suma], 3     ; (se compara al salir, con todo lo que ha salido sumado)
        add dword [dst_usado], 4
.fin:
        mov dword [dst_final], DST_FIN
        mov eax, DST_FIN
        call dst_cede               ; (de aquí ya no se sigue)

; las salidas con error: desde cualquier profundidad
dst_mal_datos:
        mov dword [dst_error], DST_E_DATOS
        jmp dst_mal
dst_mal_corto:
        mov dword [dst_error], DST_E_CORTO
dst_mal:
        mov esp, [dst_pila_tope]
        mov dword [dst_final], DST_ERROR
        mov eax, DST_ERROR
        call dst_cede               ; (ni de aquí)
        jmp dst_mal

; -> EAX=el octeto siguiente de la entrada. Si el trozo se ha acabado, pide
; otro (cede el turno); si no hay más, es que los datos están cortados.
; Conserva los demás registros (ESI avanza)
dst_octeto:
        cmp esi, [dst_in_fin]
        jae .pide
        movzx eax, byte [esi]
        inc esi
        ret
.pide:
        mov [dst_in], esi
        cmp dword [dst_sin_mas], 0
        jne dst_mal_corto
        mov eax, DST_HAMBRE
        call dst_cede
        mov esi, [dst_in]
        jmp dst_octeto
; lo mismo, pero si no hay más vuelve con CF=1 (para la suma del final)
dst_octeto_si:
        cmp esi, [dst_in_fin]
        jae .pide
        movzx eax, byte [esi]
        inc esi
        clc
        ret
.pide:
        mov [dst_in], esi
        cmp dword [dst_sin_mas], 0
        jne .no
        mov eax, DST_HAMBRE
        call dst_cede
        mov esi, [dst_in]
        jmp dst_octeto_si
.no:
        stc
        ret

; AL: un octeto que sale. Al destino y a la ventana; si con él se llena lo
; que se pidió, cede el turno. Conserva los registros
dst_pon:
        push edi
        mov edi, [dst_vp]
        mov [edi], al
        inc edi
        cmp edi, [dst_ven_fin]
        jb .v
        mov edi, [dst_ven]
.v:
        mov [dst_vp], edi
        add dword [dst_total], 1    ; (la cuenta no da la vuelta: se queda en el tope)
        sbb dword [dst_total], 0
        mov edi, [dst_out]
        mov [edi], al
        inc edi
        mov [dst_out], edi
        cmp edi, [dst_out_fin]
        pop edi
        jb .x
        push eax
        mov eax, DST_HECHO
        call dst_cede
        pop eax
.x:
        ret

; ECX=cuántos bits (de 0 a 16) -> EAX=su valor (el primero leído, el de menos peso)
dst_bits:
.mas:
        cmp edx, ecx
        jae .hay
        call dst_octeto
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
dst_simbolo:
        push ecx
        push edi
        push ebp
        xor eax, eax                ; el código leído hasta ahora
        xor ecx, ecx                ; el primer código de este largo
        xor edi, edi                ; cuántos símbolos hay de los largos menores
        mov dword [dst_n], 15
.b:
        add ebp, 2
        test edx, edx
        jnz .hay
        push eax
        call dst_octeto
        mov ebx, eax
        pop eax
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
        dec dword [dst_n]
        jnz .b
        jmp dst_mal_datos           ; se han acabado los largos: no es un código
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
dst_construye:
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
        mov word [dst_offs+2], 0    ; dónde empiezan los símbolos de cada largo
        mov ebx, 1
.offs:
        mov ax, [dst_offs+ebx*2]
        add ax, [ebp+ebx*2]
        mov [dst_offs+ebx*2+2], ax
        inc ebx
        cmp ebx, 15
        jb .offs
        xor ebx, ebx
.pon:
        movzx eax, byte [esi+ebx]
        test eax, eax
        jz .pon1
        movzx edx, word [dst_offs+eax*2]
        inc word [dst_offs+eax*2]
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
dst_pon_fijos:
        cmp dword [dst_fijos], 0
        jne .x
        push esi
        push edi
        push ecx
        push eax
        push ebp
        mov edi, dst_largos
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
        mov esi, dst_largos
        mov ecx, 288
        mov ebp, dst_lcnt
        call dst_construye
        mov edi, dst_largos
        mov al, 5
        mov ecx, DST_MAXD
        rep stosb
        mov esi, dst_largos
        mov ecx, DST_MAXD
        mov ebp, dst_dcnt
        call dst_construye
        mov dword [dst_fijos], 1
        pop ebp
        pop eax
        pop ecx
        pop edi
        pop esi
.x:
        ret

; un bloque con códigos propios: lee su descripción y hace las dos tablas
dst_dinamico:
        mov dword [dst_fijos], 0
        mov ecx, 5
        call dst_bits
        add eax, 257
        mov [dst_nl], eax
        mov ecx, 5
        call dst_bits
        inc eax
        mov [dst_nd], eax
        mov ecx, 4
        call dst_bits
        add eax, 4
        cmp dword [dst_nl], DST_MAXL
        ja dst_mal_datos
        cmp dword [dst_nd], DST_MAXD
        ja dst_mal_datos
        push eax
        mov edi, dst_largos         ; el código de los largos: 19 símbolos, de
        xor eax, eax                ; hasta 7 bits, en su orden raro
        mov ecx, 19
        rep stosb
        pop ebp                     ; EBP=cuántos vienen
        xor edi, edi
.cl:
        mov ecx, 3
        call dst_bits
        movzx ecx, byte [dst_orden+edi]
        mov [dst_largos+ecx], al
        inc edi
        cmp edi, ebp
        jb .cl
        push esi
        mov esi, dst_largos
        mov ecx, 19
        mov ebp, dst_ccnt
        call dst_construye
        pop esi
        test eax, eax
        jnz dst_mal_datos           ; tiene que estar completo
        ; los largos de las letras y de las distancias, seguidos
        xor edi, edi                ; EDI=cuántos van
.lee:
        mov eax, [dst_nl]
        add eax, [dst_nd]
        cmp edi, eax
        jae .leidos
        mov ebp, dst_ccnt
        call dst_simbolo
        cmp eax, 16
        jae .repite
        mov [dst_largos+edi], al
        inc edi
        jmp .lee
.repite:
        jne .r17
        test edi, edi               ; 16: el anterior, de 3 a 6 veces
        jz dst_mal_datos
        mov ecx, 2
        call dst_bits
        lea ecx, [eax+3]
        mov al, [dst_largos+edi-1]
        jmp .rep
.r17:
        cmp eax, 17
        jne .r18
        mov ecx, 3                  ; 17: de 3 a 10 ceros
        call dst_bits
        lea ecx, [eax+3]
        xor al, al
        jmp .rep
.r18:
        mov ecx, 7                  ; 18: de 11 a 138 ceros
        call dst_bits
        lea ecx, [eax+11]
        xor al, al
.rep:
        mov ebp, [dst_nl]
        add ebp, [dst_nd]
        sub ebp, edi
        cmp ecx, ebp
        ja dst_mal_datos            ; más de los que hay
.rep1:
        mov [dst_largos+edi], al
        inc edi
        dec ecx
        jnz .rep1
        jmp .lee
.leidos:
        cmp byte [dst_largos+256], 0
        je dst_mal_datos            ; sin el símbolo de fin no se acabaría nunca
        push esi
        mov esi, dst_largos
        mov ecx, [dst_nl]
        mov ebp, dst_lcnt
        call dst_construye
        pop esi
        test eax, eax               ; incompleto solo vale con un único código
        jz .l_ok
        js dst_mal_datos
        movzx eax, word [dst_lcnt]
        add ax, [dst_lcnt+2]
        cmp eax, [dst_nl]
        jne dst_mal_datos
.l_ok:
        push esi
        mov esi, dst_largos
        add esi, [dst_nl]
        mov ecx, [dst_nd]
        mov ebp, dst_dcnt
        call dst_construye
        pop esi
        test eax, eax
        jz .d_ok
        js dst_mal_datos
        movzx eax, word [dst_dcnt]
        add ax, [dst_dcnt+2]
        cmp eax, [dst_nd]
        jne dst_mal_datos
.d_ok:
        ret

; los datos de un bloque comprimido, con las tablas que haya puestas
dst_codigos:
.b:
        mov ebp, dst_lcnt
        call dst_simbolo
        cmp eax, 256
        jae .largo
        call dst_pon                ; una letra
        jmp .b
.largo:
        je .fin
        sub eax, 257                ; un trozo que ya ha salido: largo y distancia
        cmp eax, 29
        jae dst_mal_datos
        movzx ecx, byte [dst_lext+eax]
        movzx ebp, word [dst_lbase+eax*2]
        call dst_bits
        add ebp, eax
        push ebp
        mov ebp, dst_dcnt
        call dst_simbolo
        cmp eax, DST_MAXD
        jae dst_mal_datos
        movzx ecx, byte [dst_dext+eax]
        movzx ebp, word [dst_dbase+eax*2]
        call dst_bits
        add ebp, eax                ; EBP=la distancia
        pop ecx                     ; ECX=el largo
        cmp ebp, [dst_total]
        ja dst_mal_datos            ; antes del principio de lo que ha salido
        mov edi, [dst_vp]           ; de la ventana, EBP octetos atrás (dando la vuelta)
        sub edi, ebp
        cmp edi, [dst_ven]
        jae .copia
        add edi, DST_VENTANA
.copia:
        mov al, [edi]               ; (de octeto en octeto: pueden solaparse)
        inc edi
        cmp edi, [dst_ven_fin]
        jb .c1
        mov edi, [dst_ven]
.c1:
        call dst_pon
        dec ecx
        jnz .copia
        jmp .b
.fin:
        ret
