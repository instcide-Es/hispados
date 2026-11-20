; =============================================================================
;  Hispa-Drive - el procesador principal: 68000 (a 7,67 MHz)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del 68000 (el manual del
;  programador del fabricante: instrucciones, banderas, excepciones y
;  ciclos). Las instrucciones las hace tools/gen68k.py: una rutina por cada
;  forma (instrucción, tamaño y modos de dirección; los números de registro
;  se sacan del código en la propia rutina) y una tabla de 65.536 saltos,
;  uno por código, que se rellena al empezar (m68k_tablas) desde la lista
;  comprimida que sale del generador (m68k_gen.inc).
;
;    m68k_tablas     (una vez) la tabla de saltos y las de banderas
;    m68k_reiniciar  como al encender: el SSP y el PC, de los vectores 0 y 1
;    m68k_correr     EAX=ciclos que se añaden: ejecuta hasta gastarlos (lo
;                    que sobra o falta queda en m_resto para la siguiente)
;    m_ipl           (lo pone el VDP, con md_irq_pon) el nivel de
;                    interrupción que se pide (0-7); al aceptarla se llama a
;                    md_irq_ack (EAX=nivel)
;
;  Mientras corre:  ESI=puntero (del equipo) a la siguiente palabra del
;  programa; el PC del 68000 es ESI-[m_pcbase].  EDI=ciclos que quedan.
;  EBX=el código de la instrucción.  EAX, ECX, EDX, EBP=de paso.
;
;  La memoria del 68000 se guarda por palabras al revés (cada palabra en el
;  orden del x86): así una palabra se lee de una vez, un octeto está en la
;  dirección XOR 1 y un largo es la palabra alta y la baja (ROL 16). Se ve
;  por trozos de 64 KB: m_pl[n] (leer), m_pe[n] (escribir) y m_px[n]
;  (ejecutar) son punteros tales que [puntero+dirección] es el dato; si
;  m_pl o m_pe es 0, lo hace la rutina lenta (md_lee8, md_esc16...: los
;  puertos, el Z80, la RAM del cartucho). Las direcciones son de 24 bits.
;
;  Las banderas: m_f es el AH de LAHF tras la operación del x86 del mismo
;  tamaño (N en el bit 7, Z en el 6 y C en el 0: la resta del x86 deja el
;  acarreo como el 68000), m_v el desbordamiento (0 o 1) y m_x la X. Para
;  una condición: AH=m_f, OF=m_v (ADD AL,7Fh) y SAHF: las 14 condiciones son
;  las del x86 (HI=JA, GE=JGE, GT=JG...).
; =============================================================================

; ---- la memoria ----
; EDX=dirección (24 bits) -> AL / AX / EAX (gasta EBP; EDX queda)
%macro LEE8 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pl+ebp*4]
        test ebp, ebp
        jz %%l
        xor edx, 1
        movzx eax, byte [ebp+edx]
        xor edx, 1
        jmp short %%x
%%l:
        call md_lee8
%%x:
%endmacro
%macro LEE16 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pl+ebp*4]
        test ebp, ebp
        jz %%l
        movzx eax, word [ebp+edx]
        jmp short %%x
%%l:
        call md_lee16
%%x:
%endmacro
%macro LEE32 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pl+ebp*4]
        test ebp, ebp
        jz %%l
        mov eax, [ebp+edx]
        rol eax, 16
        jmp short %%x
%%l:
        call md_lee32
%%x:
%endmacro
; AL / AX / EAX -> la dirección EDX (gasta EBP; EAX y EDX quedan)
%macro ESC8 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pe+ebp*4]
        test ebp, ebp
        jz %%l
        xor edx, 1
        mov [ebp+edx], al
        xor edx, 1
        jmp short %%x
%%l:
        call md_esc8
%%x:
%endmacro
%macro ESC16 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pe+ebp*4]
        test ebp, ebp
        jz %%l
        mov [ebp+edx], ax
        jmp short %%x
%%l:
        call md_esc16
%%x:
%endmacro
%macro ESC32 0
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pe+ebp*4]
        test ebp, ebp
        jz %%l
        rol eax, 16
        mov [ebp+edx], eax
        rol eax, 16
        jmp short %%x
%%l:
        call md_esc32
%%x:
%endmacro

; ---- las banderas ----
; tras una operación lógica (o un MOVE): N y Z del x86, V=0, C=0
%macro FL_LOG 0
        lahf
        mov byte [m_v], 0
        mov [m_f], ah
%endmacro
; tras una suma o una resta: N Z V C, y X=C
%macro FL_ARI 0
        lahf
        seto byte [m_v]
        setc byte [m_x]
        mov [m_f], ah
%endmacro
; tras una comparación: N Z V C (la X no cambia)
%macro FL_CMP 0
        lahf
        seto byte [m_v]
        mov [m_f], ah
%endmacro
; las banderas del 68000 en las del x86 (para un Jcc): gasta EAX
%macro COND 0
        mov ax, [m_vf]
        add al, 0x7F
        sahf
%endmacro

; ---- la siguiente instrucción ----
%macro MDESPACHA 0
        movzx ebx, word [esi]
        add esi, 2
        jmp [m_tab+ebx*4]
%endmacro
; fin de una instrucción de %1 ciclos
%macro MFIN 1
        sub edi, %1
        jle near m_fuera
        MDESPACHA
%endmacro

; =============================================================================
;  Las tablas
; =============================================================================
m68k_tablas:
        pushad
        mov esi, m_rle              ; la tabla de saltos: parejas (cuántos, cuál)
        mov edi, m_tab
.r:
        movzx ecx, word [esi]
        test ecx, ecx
        jz .r9
        movzx eax, word [esi+2]
        add esi, 4
        mov eax, [m_rutinas+eax*4]
        rep stosd
        jmp .r
.r9:
        xor ebx, ebx                ; m_f -> N, Z y C del CCR
.f:
        xor eax, eax
        test bl, 0x80
        jz .f1
        or al, 8
.f1:
        test bl, 0x40
        jz .f2
        or al, 4
.f2:
        test bl, 0x01
        jz .f3
        or al, 1
.f3:
        mov [m_f2ccr+ebx], al
        inc ebx
        cmp ebx, 256
        jb .f
        xor ebx, ebx                ; y al revés (N, Z y C del CCR -> m_f)
.g:
        xor eax, eax
        test bl, 8
        jz .g1
        or al, 0x80
.g1:
        test bl, 4
        jz .g2
        or al, 0x40
.g2:
        test bl, 1
        jz .g3
        or al, 0x01
.g3:
        mov [m_ccr2f+ebx], al
        inc ebx
        cmp ebx, 32
        jb .g
        xor ebx, ebx                ; cuántos unos tiene cada octeto
.u:
        xor eax, eax
        mov ecx, ebx
.u1:
        shr ecx, 1
        adc eax, 0
        test ecx, ecx
        jnz .u1
        mov [m_unos+ebx], al
        inc ebx
        cmp ebx, 256
        jb .u
        popad
        ret

; como al encender: supervisor, interrupciones tapadas, SSP y PC de los
; vectores (las tablas de memoria ya tienen que estar puestas)
m68k_reiniciar:
        pushad
        mov edi, m_regs
        mov ecx, m_regs_fin - m_regs
        xor eax, eax
        rep stosb
        mov byte [m_srh], 0x27
        xor edx, edx
        LEE32
        mov [m_a+7*4], eax
        mov edx, 4
        LEE32
        mov edx, eax
        and edx, 0xFFFFFF
        call m_salta
        mov [m_esi], esi
        popad
        ret

; EAX=ciclos: ejecuta hasta gastarlos
m68k_correr:
        pushad
        add eax, [m_resto]
        mov edi, eax
        mov [m_presup], eax
        mov esi, [m_esi]
        mov dword [m_aparte], 0
        mov byte [m_corre], 1
        call m_mira_irq
        cmp byte [m_parado], 0
        jne .parado
        test edi, edi
        jle .fin
        MDESPACHA
.parado:
        xor edi, edi                ; (STOP: hasta una interrupción)
.fin:
        mov [m_resto], edi
        mov [m_esi], esi
        mov byte [m_corre], 0
        popad
        ret

; se han gastado los ciclos (o una rutina ha pedido mirar las interrupciones
; ya: m_aparte guarda lo que quedaba y EDI se puso a 0)
        align 4
m_fuera:
        mov eax, [m_aparte]
        test eax, eax
        jz m68k_correr.fin
        mov dword [m_aparte], 0
        add edi, eax
        call m_mira_irq
        cmp byte [m_parado], 0
        jne m68k_correr.parado
        test edi, edi
        jle m68k_correr.fin
        MDESPACHA

; (dentro de una instrucción) que se miren las interrupciones al acabarla
m_pide_mirar:
        add [m_aparte], edi
        xor edi, edi
        ret

; si hay una interrupción de más nivel que la máscara, se acepta
m_mira_irq:
        movzx eax, byte [m_ipl]
        test eax, eax
        jz .x
        mov ah, [m_srh]
        and ah, 7
        cmp al, 7
        je .si
        cmp al, ah
        jbe .x
.si:
        pushad
        movzx eax, al
        push eax
        call m_sr_lee               ; EAX=SR (de antes)
        mov ecx, eax
        pop eax
        push eax
        mov byte [m_parado], 0
        call m_a_super
        mov ah, [m_srh]             ; la máscara, al nivel aceptado
        and ah, 0xF8
        or ah, al
        and ah, 0x7F                ; (sin traza)
        mov [m_srh], ah
        mov eax, ecx
        call m_marco                ; el PC y el SR, a la pila
        pop eax
        push eax
        add eax, 24                 ; los vectores automáticos: 25-31
        shl eax, 2
        mov edx, eax
        LEE32
        mov edx, eax
        and edx, 0xFFFFFF
        call m_salta
        pop eax
        mov cl, [m_corre]           ; (al quitarla no hay que mirar nada)
        mov byte [m_corre], 0
        call md_irq_ack
        mov [m_corre], cl
        mov [esp+4], esi            ; (ESI y EDI de vuelta, por POPAD)
        sub dword [esp], 44
        popad
.x:
        ret

; =============================================================================
;  El PC, el SR y las excepciones
; =============================================================================
; EDX=dirección (24 bits): el programa sigue ahí (ESI y m_pcbase)
m_salta:
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_px+ebp*4]
        mov [m_pcbase], ebp
        lea esi, [ebp+edx]
        ret

; -> EAX=el SR entero
m_sr_lee:
        movzx eax, byte [m_f]
        movzx eax, byte [m_f2ccr+eax]
        cmp byte [m_v], 0
        je .v
        or al, 2
.v:
        cmp byte [m_x], 0
        je .x
        or al, 0x10
.x:
        mov ah, [m_srh]
        ret

; AL=el CCR -> las banderas (gasta EAX)
m_ccr_pon:
        push ebx
        movzx ebx, al
        and ebx, 0x1D
        mov ah, [m_ccr2f+ebx]
        mov [m_f], ah
        mov ah, al
        shr ah, 1
        and ah, 1
        mov [m_v], ah
        shr al, 4
        and al, 1
        mov [m_x], al
        pop ebx
        ret

; AX=el SR nuevo (gasta EAX): cambia de pila si cambia el bit S y pide
; mirar las interrupciones (la máscara puede haber bajado)
m_sr_pon:
        push eax
        call m_ccr_pon
        pop eax
        mov al, ah
        and al, 0xA7
        mov ah, [m_srh]
        mov [m_srh], al
        xor ah, al
        test ah, 0x20
        jz .s
        mov eax, [m_a+7*4]          ; la otra pila
        xchg eax, [m_osp]
        mov [m_a+7*4], eax
.s:
        jmp m_pide_mirar

; pasa a supervisor (con su pila), sin traza (gasta EAX)
m_a_super:
        test byte [m_srh], 0x20
        jnz .x
        or byte [m_srh], 0x20
        mov eax, [m_a+7*4]
        xchg eax, [m_osp]
        mov [m_a+7*4], eax
.x:
        and byte [m_srh], 0x7F
        ret

; AX=SR: a la pila del supervisor el PC (ESI) y el SR (gasta EAX, EDX, EBP)
m_marco:
        push eax
        mov eax, esi
        sub eax, [m_pcbase]
        sub dword [m_a+7*4], 4
        mov edx, [m_a+7*4]
        and edx, 0xFFFFFF
        ESC32
        pop eax
        sub dword [m_a+7*4], 2
        mov edx, [m_a+7*4]
        and edx, 0xFFFFFF
        ESC16
        ret

; una excepción: EAX=vector (el PC que se guarda es ESI). Gasta EAX, ECX,
; EDX, EBP
m_excepcion:
        push eax
        call m_sr_lee
        mov ecx, eax
        call m_a_super
        mov eax, ecx
        call m_marco
        pop edx
        shl edx, 2
        LEE32
        mov edx, eax
        and edx, 0xFFFFFF
        jmp m_salta

; las excepciones de las rutinas generadas (saltan aquí en vez de FIN)
m_ex_ilegal:                        ; (el PC guardado: el de la instrucción)
        sub esi, 2
        mov eax, 4
        call m_excepcion
        MFIN 34
m_ex_lineaa:
        sub esi, 2
        mov eax, 10
        call m_excepcion
        MFIN 34
m_ex_lineaf:
        sub esi, 2
        mov eax, 11
        call m_excepcion
        MFIN 34
m_ex_privilegio:
        sub esi, 2
        mov eax, 8
        call m_excepcion
        MFIN 34
; división entre 0 (el PC: la siguiente; N Z V C a 0); con los ciclos de
; la dirección de la fuente
%assign i 0
%rep 13
m_ex_cero_%[i]:
        sub edi, i
        jmp m_ex_cero
%assign i i+1
%endrep
m_ex_cero:
        mov word [m_vf], 0
        mov eax, 5
        call m_excepcion
        MFIN 38
m_ex_chk:
        mov eax, 6
        call m_excepcion
        MFIN 40
m_ex_trapv:
        mov eax, 7
        call m_excepcion
        MFIN 34

; =============================================================================
;  Lo que es largo para hacerlo en cada rutina
; =============================================================================
; MOVEM de registros a memoria. ECX=máscara EDX=dirección EAX=1 largos
; (si no, palabras). -> EDX=la dirección después
m_movem_rm:
        push ebx
        xor ebx, ebx
        test eax, eax
        jnz .l
.w:
        shr ecx, 1
        jnc .w1
        mov eax, [m_d+ebx*4]
        push ebp
        ESC16
        pop ebp
        add edx, 2
        and edx, 0xFFFFFF
        sub edi, 4
.w1:
        inc ebx
        test ecx, ecx
        jnz .w
        pop ebx
        ret
.l:
        shr ecx, 1
        jnc .l1
        mov eax, [m_d+ebx*4]
        ESC32
        add edx, 4
        and edx, 0xFFFFFF
        sub edi, 8
.l1:
        inc ebx
        test ecx, ecx
        jnz .l
        pop ebx
        ret

; lo mismo hacia abajo (-(An)): la máscara al revés (bit 0 = A7). EDX=An
; (sin restar) -> EDX=lo que queda en An
m_movem_rm_pre:
        push ebx
        mov ebx, 15
        test eax, eax
        jnz .l
.w:
        shr ecx, 1
        jnc .w1
        sub edx, 2
        and edx, 0xFFFFFF
        mov eax, [m_d+ebx*4]
        ESC16
        sub edi, 4
.w1:
        dec ebx
        test ecx, ecx
        jnz .w
        pop ebx
        ret
.l:
        shr ecx, 1
        jnc .l1
        sub edx, 4
        and edx, 0xFFFFFF
        mov eax, [m_d+ebx*4]
        ESC32
        sub edi, 8
.l1:
        dec ebx
        test ecx, ecx
        jnz .l
        pop ebx
        ret

; MOVEM de memoria a registros (las palabras, con su signo, a 32 bits).
; ECX=máscara EDX=dirección EAX=1 largos -> EDX=la dirección después
m_movem_mr:
        push ebx
        xor ebx, ebx
        test eax, eax
        jnz .l
.w:
        shr ecx, 1
        jnc .w1
        LEE16
        cwde
        mov [m_d+ebx*4], eax
        add edx, 2
        and edx, 0xFFFFFF
        sub edi, 4
.w1:
        inc ebx
        test ecx, ecx
        jnz .w
        pop ebx
        ret
.l:
        shr ecx, 1
        jnc .l1
        LEE32
        mov [m_d+ebx*4], eax
        add edx, 4
        and edx, 0xFFFFFF
        sub edi, 8
.l1:
        inc ebx
        test ecx, ecx
        jnz .l
        pop ebx
        ret

; ABCD: AL=destino CL=fuente -> AL=resultado y las banderas (Z solo se
; quita). La X entra y sale
m_abcd:
        push edx
        push ebx
        movzx edx, al
        movzx ecx, cl
        movzx ebx, byte [m_x]
        mov eax, edx                ; las unidades
        and eax, 0x0F
        push ecx
        and ecx, 0x0F
        add eax, ecx
        add eax, ebx
        pop ecx
        push eax
        lea eax, [edx+ecx]          ; en binario
        add eax, ebx
        mov ebx, eax                ; EBX = binario
        pop eax
        xor edx, edx                ; EDX = la corrección
        cmp eax, 9
        jbe .a1
        mov edx, 6
.a1:
        xor ecx, ecx                ; CL = acarreo
        cmp ebx, 0x99
        jbe .a2
        add edx, 0x60
        inc ecx
.a2:
        lea eax, [ebx+edx]
        mov [m_x], cl
        ; V: el bit 7 pasa de 0 (binario) a 1 (corregido)
        mov edx, ebx
        not edx
        and edx, eax
        shr edx, 7
        and edx, 1
        mov [m_v], dl
        jmp m_bcd_fin

; SBCD: AL=destino CL=fuente (destino - fuente - X)
m_sbcd:
        push edx
        push ebx
        movzx edx, al
        movzx ecx, cl
m_sbcd_c:
        movzx ebx, byte [m_x]
        mov eax, edx
        and eax, 0x0F
        push ecx
        and ecx, 0x0F
        sub eax, ecx
        sub eax, ebx                ; EAX = las unidades (con signo)
        pop ecx
        push eax
        mov eax, edx
        sub eax, ecx
        sub eax, ebx
        mov ebx, eax                ; EBX = binario (con signo)
        pop eax
        xor edx, edx
        test eax, eax
        jns .s1
        mov edx, 6
.s1:
        xor ecx, ecx
        test ebx, ebx
        jns .s2
        add edx, 0x60
        inc ecx
.s2:
        mov eax, ebx
        sub eax, edx
        test ebx, ebx               ; (las unidades corregidas pueden pedir
        js .s3                      ; prestado aunque el binario no)
        test eax, eax
        jns .s3
        mov ecx, 1
.s3:
        mov [m_x], cl
        mov edx, ebx                ; V: el bit 7 pasa de 1 a 0
        not eax
        and edx, eax
        not eax
        shr edx, 7
        and edx, 1
        mov [m_v], dl
m_bcd_fin:
        ; N del bit 7, Z se quita si no es 0, C=X
        mov ch, [m_f]
        and ch, 0x40
        test al, al
        jz .z
        xor ch, ch
.z:
        test al, 0x80
        jz .n
        or ch, 0x80
.n:
        or ch, cl
        mov [m_f], ch
        movzx eax, al
        pop ebx
        pop edx
        ret

; NBCD: AL=el octeto -> 0 - AL - X
m_nbcd:
        push edx
        push ebx
        movzx ecx, al
        xor edx, edx
        jmp m_sbcd_c

; DIVU: EAX=dividendo (32) ECX=divisor (16, sin signo, no 0) -> EAX=resto:
; cociente (o el dividendo si desborda) y las banderas. Resta de EDI los
; ciclos, como el 68000: el cociente sale bit a bit, y cada paso cuesta según
; si se resta o no
m_divu:
        push edx
        push ebx
        mov ebx, eax
        mov edx, eax
        shr edx, 16
        cmp edx, ecx
        jae .desb
        push ecx                    ; los ciclos: 76 y, por cada uno de los
        push esi                    ; 15 bits altos del cociente, 0, 2 o 4
        mov esi, ecx
        shl esi, 16
        mov edx, eax
        mov ecx, 15
        sub edi, 76
.c:
        shl edx, 1
        jnc .c1
        sub edx, esi
        jmp .c2
.c1:
        sub edi, 4
        cmp edx, esi
        jb .c2
        sub edx, esi
        add edi, 2
.c2:
        dec ecx
        jnz .c
        pop esi
        pop ecx
        xor edx, edx
        div ecx
        shl edx, 16
        or edx, eax
        test ax, ax
        lahf
        and ah, 0xC0
        mov [m_f], ah
        mov byte [m_v], 0
        mov eax, edx
        pop ebx
        pop edx
        ret
.desb:
        sub edi, 10
        mov byte [m_v], 1           ; desborda: V=1 C=0 (N y Z, como
        and byte [m_f], 0xC0        ; estaban), el registro igual
        mov eax, ebx
        pop ebx
        pop edx
        ret

; DIVS: EAX=dividendo ECX=divisor (con signo, no 0); los ciclos, como el
; 68000 (según los signos y los 15 bits altos del cociente sin signo)
m_divs:
        push edx
        push ebx
        mov ebx, eax
        push esi
        push ecx
        mov esi, 12                 ; ESI = los ciclos
        test eax, eax
        jns .s1
        add esi, 2
        neg eax
.s1:
        test ecx, ecx
        jns .s2
        neg ecx
.s2:
        mov edx, eax                ; ¿desborda sin mirar el signo?
        shr edx, 15
        cmp edx, ecx
        jb .s3
        add esi, 4
        sub edi, esi
        pop ecx
        pop esi
        jmp .desb
.s3:
        xor edx, edx
        div ecx                     ; EAX = cociente sin signo
        add esi, 110
        mov ecx, [esp]
        test ecx, ecx
        js .s5
        sub esi, 2
        test ebx, ebx
        jns .s5
        add esi, 4
.s5:
        mov ecx, 15
.s6:
        test ax, ax
        js .s7
        add esi, 2
.s7:
        shl eax, 1
        dec ecx
        jnz .s6
        sub edi, esi
        pop ecx
        pop esi
        mov eax, ebx
        cmp eax, 0x80000000         ; (-2^31 / -1: el x86 no puede)
        jne .d
        cmp ecx, -1
        je .desb
.d:
        cdq
        idiv ecx
        movsx ecx, ax
        cmp ecx, eax
        jne .desb
        shl edx, 16
        mov dx, ax
        test ax, ax
        lahf
        and ah, 0xC0
        mov [m_f], ah
        mov byte [m_v], 0
        mov eax, edx
        pop ebx
        pop edx
        ret
.desb:
        mov byte [m_v], 1
        and byte [m_f], 0xC0
        mov eax, ebx
        pop ebx
        pop edx
        ret

; tras ADDX, SUBX o NEGX: N V C y X; la Z solo se quita (si el resultado
; no es 0). Conserva EAX
m_fl_x:
        push eax
        lahf
        seto byte [m_v]
        setc byte [m_x]
        mov al, [m_f]
        or al, 0xBF
        and ah, al
        mov [m_f], ah
        pop eax
        ret
