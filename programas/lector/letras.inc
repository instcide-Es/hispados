; =============================================================================
;  letras.inc - Lector: las fuentes de un PDF (qué letra es cada código, cuánto
;  avanza y a cuál de nuestros tipos se parece)
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lector no dibuja con las fuentes que el PDF lleva dentro: de cada una saca
;  solo tres cosas y las guarda en una ficha (F_*), que vale mientras se
;  compone la página:
;
;   - a qué tipo de los nuestros se parece (por su nombre y sus banderas): con
;     remates, «Imprenta» (las Times) o «Roble»; palo seco, «Claro» (las
;     Arial y Helvetica) o «Llano»; ancho fijo, «Máquina»; y sus negritas;
;   - qué letra de la página 437 es cada código: por su /Encoding (WinAnsi,
;     MacRoman, Standard), sus /Differences (nombres: aacute, ntilde...) y su
;     /ToUnicode. Lo que no existe en la 437 va por su letra sin acento o se
;     queda en 0 (se verá «?»). Los códigos de 1 a 15 son varias letras (las
;     ligaduras «fi», el euro: exp_tab);
;   - cuánto avanza cada código (en milésimas del tamaño): /Widths o, en las
;     fuentes de siempre sin anchos, los de Helvetica, Times y Courier.
;
;  Una fuente compuesta (Type0: códigos de dos octetos) solo se lee si trae
;  /ToUnicode. La que no se puede leer (códigos propios sin /ToUnicode,
;  símbolos) queda marcada FB_ILEGIBLE: su texto no se escribe, se avisa.
;
;    fu_busca    ESI, EBP=el diccionario de la fuente -> EDI=su ficha
;    fu_vacia    olvida todas las fichas (al empezar una página)
;    uni_437     EAX=carácter Unicode -> AL=el de la 437 (0: no hay)
;    gl_busca    ESI=nombre de letra (acabado en 0) -> EAX=Unicode (0: no se sabe)
; =============================================================================
MAX_FU      equ 48                  ; fuentes distintas en una página
MAX_T0      equ 8                   ; de ellas, compuestas
T0_COD      equ 16384               ; códigos que se guardan de una compuesta
F_CLAVE     equ 0                   ; el diccionario de la fuente (para reconocerla)
F_TIPO      equ 4                   ; el tipo nuestro
F_BAND      equ 5                   ; FB_*
F_DW        equ 6                   ; el ancho de los códigos de más de T0_COD
F_T0MAP     equ 8                   ; compuesta: la letra de cada código
F_T0AN      equ 12                  ; y su ancho, en octavos de milésima... (x 8)
F_COD       equ 16                  ; sencilla: la letra de cada código
F_ANCHO     equ 272                 ; y su ancho (dw)
F_TAM       equ 784
FB_NEGRA    equ 1                   ; negrita que hay que fingir (el tipo no la tiene)
FB_CURSIVA  equ 2
FB_ILEGIBLE equ 4
FB_DOS      equ 8                   ; códigos de dos octetos
FB_FIJA     equ 16
FB_IDENT    equ 32                  ; compuesta cuyo código es ya el Unicode de la letra

TL_ROBLE    equ 1                   ; los tipos de programas/comun/tipos.inc
TL_ROBLE_N  equ 2
TL_IMPRENTA equ 3
TL_LLANO    equ 9
TL_LLANO_N  equ 10
TL_CLARO    equ 11
TL_MAQUINA  equ 17

%macro LETRAS_BSS 0
            alignb 4
fu_n        resd 1
fu_t0n      resd 1
fu_dic      resd 2
fu_cid      resd 2
fu_desc     resd 2
fu_enc      resd 2
fu_slot     resd 1
fu_flags    resd 1
fu_es_t0    resd 1
fu_tt       resd 1
fu_t3       resd 1
fu_base     resd 1
fu_hay_cod  resd 1
fu_hay_tu   resd 1
fu_conoc    resd 1
fu_descon   resd 1
fu_std      resd 1
fu_simb     resd 1
fu_tex      resd 1
fu_modo     resd 1
fu_lo       resd 1
fu_hi       resd 1
fu_nom      resb 68
fu_tmp      resb 80
            alignb 4
fu_tab      resb MAX_FU * F_TAM
fu_t0       resb MAX_T0 * T0_COD * 2
%endmacro

k_subtype   db "Subtype", 0
k_basefont  db "BaseFont", 0
k_descfonts db "DescendantFonts", 0
k_fontdesc  db "FontDescriptor", 0
k_flags     db "Flags", 0
k_encoding  db "Encoding", 0
k_baseenc   db "BaseEncoding", 0
k_differ    db "Differences", 0
k_tounicode db "ToUnicode", 0
k_firstchar db "FirstChar", 0
k_widths    db "Widths", 0
k_fontmat   db "FontMatrix", 0
k_dw        db "DW", 0
k_missing   db "MissingWidth", 0
n_type0     db "Type0", 0
n_truetype  db "TrueType", 0
n_type3     db "Type3", 0
n_winansi   db "WinAnsiEncoding", 0
n_macroman  db "MacRomanEncoding", 0
n_standard  db "StandardEncoding", 0
n_notdef    db ".notdef", 0
w_bfchar    db "beginbfchar", 0
w_bfrange   db "beginbfrange", 0
w_endbfc    db "endbfchar", 0
w_endbfr    db "endbfrange", 0
; trozos de nombre (en minúsculas) que dicen cómo es una fuente
l_fija      db "courier", 0, "mono", 0, "consol", 0, "typewriter", 0, "cmtt", 0, "lmtt", 0, "fixed", 0, "terminal", 0, 0
l_negra     db "bold", 0, "black", 0, "heavy", 0, "demi", 0, "negrita", 0, "cmbx", 0, "-bd", 0, 0
l_cursiva   db "italic", 0, "oblique", 0, "cursiva", 0, "cmti", 0, "cmsl", 0, "-it", 0, "kursiv", 0, 0
l_remate    db "times", 0, "serif", 0, "georgia", 0, "garamond", 0, "cambria", 0, "libertin", 0, "palatino", 0
            db "century", 0, "bookman", 0, "minion", 0, "cmr", 0, "cmbx", 0, "cmti", 0, "cmsl", 0, "lmroman", 0
            db "charter", 0, "utopia", 0, "didot", 0, "bodoni", 0, "baskerv", 0, "caslon", 0, "nimbusrom", 0
            db "constantia", 0, "antiqua", 0, "tinos", 0, "roman", 0, "schoolbook", 0, "sfrm", 0, "thorndale", 0, 0
l_times     db "times", 0, "nimbusrom", 0, "liberationserif", 0, "libertin", 0, "tinos", 0, "thorndale", 0
            db "cmr", 0, "cmti", 0, "cmsl", 0, "lmroman", 0, "sfrm", 0, 0
l_arial     db "arial", 0, "helvet", 0, "liberationsans", 0, "nimbussan", 0, "arimo", 0, "opensans", 0
            db "calibri", 0, "carlito", 0, "albany", 0, 0
l_simbolos  db "symbol", 0, "dingbat", 0, "wingding", 0, "webding", 0, "cmsy", 0, "cmmi", 0, "cmex", 0, "msam", 0, "msbm", 0, 0
l_tex       db "cmr", 0, "cmbx", 0, "cmti", 0, "cmsl", 0, "cmss", 0, "cmcsc", 0, "cmtt", 0, 0
t_sans      db "sans", 0
; lo que las fuentes de TeX (codificación OT1) tienen distinto del ASCII
ot1_tab     db 0x0B, 3, 0x0C, 1, 0x0D, 2, 0x0E, 4, 0x0F, 5, 0x10, 'i', 0x19, 0xE1
            db 0x3C, 0xAD, 0x3E, 0xA8, 0x5C, '"', 0x7B, '-', 0x7C, '-', 0x22, '"', 0

fu_vacia:
        mov dword [fu_n], 0
        mov dword [fu_t0n], 0
        ret

fu_busca:
        push eax
        push ecx
        mov edi, fu_tab
        mov ecx, [fu_n]
.b:
        test ecx, ecx
        jz .nueva
        cmp [edi+F_CLAVE], esi
        je .x
        add edi, F_TAM
        dec ecx
        jmp .b
.nueva:
        cmp dword [fu_n], MAX_FU    ; no caben más: se aprovecha la última
        jb .n1
        dec dword [fu_n]
        sub edi, F_TAM
.n1:
        inc dword [fu_n]
        call fu_prepara
.x:
        pop ecx
        pop eax
        ret

; (interna) ¿está el trozo EDI en el nombre de la fuente? ZF=1: sí
fu_tiene:
        push eax
        push ebx
        push esi
        mov esi, fu_nom
.b:
        cmp byte [esi], 0
        je .no
        xor ebx, ebx
.c:
        mov al, [edi+ebx]
        test al, al
        jz .si
        cmp al, [esi+ebx]
        jne .sig
        inc ebx
        jmp .c
.sig:
        inc esi
        jmp .b
.si:
        xor eax, eax
        jmp .x
.no:
        or eax, 1
.x:
        pop esi
        pop ebx
        pop eax
        ret

; (interna) ¿está alguno de los trozos de la lista EDI? ZF=1: sí
fu_alguna:
        push edi
.b:
        cmp byte [edi], 0
        je .no
        call fu_tiene
        je .x
.s:
        inc edi
        cmp byte [edi-1], 0
        jne .s
        jmp .b
.no:
        cmp edi, 0                  ; ZF=0
.x:
        pop edi
        ret

; (interna) hace la ficha EDI de la fuente ESI, EBP
fu_prepara:
        pushad
        mov [fu_slot], edi
        mov [fu_dic], esi
        mov [fu_dic+4], ebp
        push edi
        mov ecx, F_TAM / 4
        xor eax, eax
        rep stosd
        pop edi
        mov [edi+F_CLAVE], esi
        mov word [edi+F_DW], 500
        mov [fu_desc], eax
        mov [fu_cid], eax
        mov [fu_enc], eax
        mov [fu_flags], eax
        mov [fu_es_t0], eax
        mov [fu_tt], eax
        mov [fu_t3], eax
        mov [fu_hay_cod], eax
        mov [fu_hay_tu], eax
        mov [fu_conoc], eax
        mov [fu_descon], eax
        mov [fu_nom], al
        mov edi, k_subtype
        call px_dic
        jc .sin_clase
        call px_ref
        mov edi, n_type0
        call px_es_nombre
        jne .c1
        inc dword [fu_es_t0]
.c1:
        mov edi, n_truetype
        call px_es_nombre
        jne .c2
        inc dword [fu_tt]
.c2:
        mov edi, n_type3
        call px_es_nombre
        jne .sin_clase
        inc dword [fu_t3]
.sin_clase:
        ; el nombre, en minúsculas y sin el prefijo de las fuentes recortadas
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_basefont
        call px_dic
        jc .sin_nombre
        call px_ref
        mov edi, fu_nom
        mov ecx, 64
        call px_copia_nombre
        cmp eax, 7
        jb .min
        cmp byte [fu_nom+6], '+'
        jne .min
        mov esi, fu_nom + 7
        mov edi, fu_nom
.quita:
        lodsb
        stosb
        test al, al
        jnz .quita
.min:
        mov esi, fu_nom
.m:
        mov al, [esi]
        test al, al
        jz .sin_nombre
        cmp al, 'A'
        jb .m1
        cmp al, 'Z'
        ja .m1
        add byte [esi], 32
.m1:
        inc esi
        jmp .m
.sin_nombre:
        ; una compuesta: lo demás está en su fuente «descendiente»
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        cmp dword [fu_es_t0], 0
        je .desc
        mov edi, k_descfonts
        call px_dic
        jc .sin_desc
        call px_ref
        cmp esi, ebp
        jae .sin_desc
        cmp byte [esi], '['
        jne .sin_desc
        inc esi
        call px_blancos
        call px_ref
        mov [fu_cid], esi
        mov [fu_cid+4], ebp
.desc:
        mov edi, k_fontdesc
        call px_dic
        jc .sin_desc
        call px_ref
        mov [fu_desc], esi
        mov [fu_desc+4], ebp
        mov edi, k_flags
        call px_dic
        jc .sin_desc
        call px_ref
        call px_entero
        jc .sin_desc
        mov [fu_flags], eax
.sin_desc:
        call fu_clasifica
        cmp dword [fu_es_t0], 0
        jne .t0
        call fu_sencilla
        jmp .x
.t0:
        call fu_compuesta
.x:
        popad
        ret

; (interna) el tipo nuestro que más se le parece, por el nombre y las banderas
fu_clasifica:
        pushad
        mov ebp, [fu_slot]
        mov ebx, [fu_flags]
        xor edx, edx                ; DL=negrita DH=cursiva
        mov edi, l_negra
        call fu_alguna
        jne .n1
        inc dl
.n1:
        test ebx, 0x40000
        jz .n2
        mov dl, 1
.n2:
        mov edi, l_cursiva
        call fu_alguna
        jne .k1
        inc dh
.k1:
        test bl, 0x40
        jz .k2
        mov dh, 1
.k2:
        test dh, dh
        jz .k3
        or byte [ebp+F_BAND], FB_CURSIVA
.k3:
        mov dword [fu_simb], 0
        mov edi, l_simbolos
        call fu_alguna
        jne .s1
        inc dword [fu_simb]
.s1:
        mov dword [fu_tex], 0
        mov edi, l_tex
        call fu_alguna
        jne .x1
        cmp word [fu_nom], 'cm'     ; (que empiece así: «cmr10»)
        jne .x1
        inc dword [fu_tex]
.x1:
        mov edi, l_fija
        call fu_alguna
        je .fija
        test bl, 1
        jnz .fija
        mov edi, l_remate
        call fu_alguna
        jne .no_remate
        mov edi, t_sans
        call fu_tiene
        jne .remate
.no_remate:
        mov edi, l_arial            ; palo seco: ¿de las que conocemos?
        call fu_alguna
        je .claro
        mov edi, t_sans
        call fu_tiene
        je .llano
        test bl, 2                  ; nada en el nombre: lo que digan las banderas
        jnz .remate
.llano:
        mov al, TL_LLANO
        jmp .palo
.claro:
        mov al, TL_CLARO
.palo:
        mov dword [fu_std], 0
        test dl, dl
        jz .pon
        mov al, TL_LLANO_N
        mov dword [fu_std], 1
        jmp .pon
.remate:
        mov al, TL_ROBLE
        mov edi, l_times
        call fu_alguna
        jne .r1
        mov al, TL_IMPRENTA
.r1:
        mov dword [fu_std], 2
        test dl, dl
        jz .pon
        mov al, TL_ROBLE_N
        mov dword [fu_std], 3
        jmp .pon
.fija:
        mov al, TL_MAQUINA
        mov dword [fu_std], 4
        or byte [ebp+F_BAND], FB_FIJA
        test dl, dl
        jz .pon
        or byte [ebp+F_BAND], FB_NEGRA
.pon:
        mov [ebp+F_TIPO], al
        popad
        ret

; (interna) AL=letra de la 437 (o código de varias) -> EAX=su ancho en las
; fuentes de siempre, según [fu_std]
fu_ancho_std:
        push ebx
        movzx eax, al
        mov ebx, [fu_std]
        cmp ebx, 4
        je .fijo
        cmp eax, 16
        jb .varias
        cmp eax, 128
        jb .a1
        movzx eax, byte [sin_acento+eax-128]
.a1:
        sub eax, 32
        cmp eax, 94
        ja .medio
        imul ebx, 95
        add ebx, eax
        movzx eax, word [an_std+ebx*2]
        pop ebx
        ret
.varias:
        test eax, eax
        jz .medio
        mov eax, 556
        pop ebx
        ret
.medio:
        mov eax, 500
        pop ebx
        ret
.fijo:
        mov eax, 600
        pop ebx
        ret

; (interna) una fuente sencilla: un octeto por letra
fu_sencilla:
        pushad
        mov eax, [fu_tt]            ; sin /Encoding: las TrueType, como en Windows;
        mov [fu_base], eax          ; las demás, la Standard
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_encoding
        call px_dic
        jc .base
        call px_ref
        cmp esi, ebp
        jae .base
        cmp byte [esi], '/'
        jne .enc_dic
        inc dword [fu_hay_cod]
        call .nombre_base
        jmp .base
.enc_dic:
        cmp byte [esi], '<'
        jne .base
        inc dword [fu_hay_cod]
        mov [fu_enc], esi
        mov [fu_enc+4], ebp
        mov edi, k_baseenc
        call px_dic
        jc .base
        call px_ref
        call .nombre_base
.base:
        ; la tabla de la codificación base
        mov edi, [fu_slot]
        xor ecx, ecx
.b:
        mov eax, ecx
        mov ebx, [fu_base]
        test ebx, ebx
        jnz .b1
        xor eax, eax                ; Standard
        cmp ecx, 0x20
        jb .b3
        movzx eax, word [std_tab+ecx*2-0x40]
        jmp .b3
.b1:
        cmp ecx, 0x80
        jb .b3
        cmp ebx, 1
        jne .b2
        cmp ecx, 0xA0               ; WinAnsi
        jae .b3
        movzx eax, word [win_tab+ecx*2-0x100]
        jmp .b3
.b2:
        movzx eax, word [mac_tab+ecx*2-0x100]
.b3:
        call uni_437
        mov [edi+F_COD+ecx], al
        inc ecx
        cmp ecx, 256
        jb .b
        ; las fuentes de TeX sin /Encoding: casi ASCII, con sus ligaduras
        cmp dword [fu_tex], 0
        je .dif
        cmp dword [fu_hay_cod], 0
        jne .dif
        mov esi, ot1_tab
.ot1:
        lodsb
        test al, al
        jz .ot1_fin
        movzx ebx, al
        lodsb
        mov [edi+F_COD+ebx], al
        jmp .ot1
.ot1_fin:
        inc dword [fu_hay_cod]      ; (se da por conocida)
.dif:
        ; /Differences [ código /nombre /nombre ... código /nombre ... ]
        cmp dword [fu_enc], 0
        je .tu
        mov esi, [fu_enc]
        mov ebp, [fu_enc+4]
        mov edi, k_differ
        call px_dic
        jc .tu
        call px_ref
        cmp esi, ebp
        jae .tu
        cmp byte [esi], '['
        jne .tu
        inc esi
        xor ebx, ebx                ; EBX=el código
.d:
        call px_blancos
        cmp esi, ebp
        jae .tu
        mov al, [esi]
        cmp al, ']'
        je .tu
        cmp al, '/'
        je .d_nom
        call px_entero
        jc .d_raro
        mov ebx, eax
        jmp .d
.d_raro:
        call px_salta
        jmp .d
.d_nom:
        mov edi, n_notdef
        call px_es_nombre
        pushf
        mov edi, fu_tmp
        mov ecx, 64
        call px_copia_nombre
        popf
        je .d_sig
        push esi
        mov esi, fu_tmp
        call gl_busca
        pop esi
        test eax, eax
        jnz .d_ok
        inc dword [fu_descon]
        jmp .d_pon
.d_ok:
        inc dword [fu_conoc]
        call uni_437
.d_pon:
        cmp ebx, 255
        ja .d_sig
        mov edi, [fu_slot]
        mov [edi+F_COD+ebx], al
.d_sig:
        inc ebx
        jmp .d
.tu:
        call fu_tounicode
        ; ¿se puede leer?
        cmp dword [fu_hay_tu], 0
        jne .anchos
        mov edi, [fu_slot]
        cmp dword [fu_simb], 0
        jne .ilegible
        mov eax, [fu_descon]        ; más nombres desconocidos que conocidos
        cmp eax, [fu_conoc]
        ja .ilegible
        cmp dword [fu_hay_cod], 0   ; simbólica, incrustada y sin decir cómo
        jne .anchos
        cmp dword [fu_desc], 0
        je .anchos
        mov eax, [fu_flags]
        and eax, 4 | 32
        cmp eax, 4
        jne .anchos
.ilegible:
        or byte [edi+F_BAND], FB_ILEGIBLE
.anchos:
        ; los anchos: los de siempre y, encima, los que diga /Widths
        mov edi, [fu_slot]
        xor ecx, ecx
.a:
        mov al, [edi+F_COD+ecx]
        call fu_ancho_std
        mov [edi+F_ANCHO+ecx*2], ax
        inc ecx
        cmp ecx, 256
        jb .a
        mov dword [fu_lo], 0x10000  ; la escala de los anchos: 1 (en las Type3,
        cmp dword [fu_t3], 0        ; mil veces lo que diga su matriz)
        je .w0
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_fontmat
        call px_dic
        jc .w0
        call px_ref
        cmp esi, ebp
        jae .w0
        cmp byte [esi], '['
        jne .w0
        inc esi
        call px_blancos
        call px_num
        jc .w0
        imul eax, 1000
        test eax, eax
        jle .w0
        mov [fu_lo], eax
.w0:
        ; el ancho de los códigos que /Widths no trae: /MissingWidth (o 0)
        mov dword [fu_hi], 0
        cmp dword [fu_desc], 0
        je .mw
        mov esi, [fu_desc]
        mov ebp, [fu_desc+4]
        mov edi, k_missing
        call px_dic
        jc .mw
        call px_ref
        call px_entero
        jc .mw
        cmp eax, 4000
        ja .mw
        mov [fu_hi], eax
.mw:
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_firstchar
        call px_dic
        jc .x
        call px_ref
        call px_entero
        jc .x
        mov ebx, eax
        test ebx, ebx
        js .x
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_widths
        call px_dic
        jc .x
        call px_ref
        cmp esi, ebp
        jae .x
        cmp byte [esi], '['
        jne .x
        inc esi
        mov edi, [fu_slot]
        push edi
        lea edi, [edi+F_ANCHO]
        mov eax, [fu_hi]
        mov ecx, 256
        rep stosw
        pop edi
.w:
        call px_blancos
        cmp esi, ebp
        jae .x
        cmp byte [esi], ']'
        je .x
        cmp ebx, 255
        ja .x
        push esi
        push ebp
        call px_ref
        call px_num
        pop ebp
        pop esi
        jc .w_sig
        imul dword [fu_lo]          ; (x escala; se queda la parte entera)
        shrd eax, edx, 16
        sar eax, 16
        jns .w1
        xor eax, eax
.w1:
        cmp eax, 4000
        jbe .w2
        mov eax, 4000
.w2:
        mov [edi+F_ANCHO+ebx*2], ax
.w_sig:
        call px_salta_valor
        inc ebx
        jmp .w
.x:
        popad
        ret
; (interna) ESI=el nombre de una codificación: si se conoce, a [fu_base]
.nombre_base:
        mov edi, n_winansi
        call px_es_nombre
        jne .nb1
        mov dword [fu_base], 1
.nb1:
        mov edi, n_macroman
        call px_es_nombre
        jne .nb2
        mov dword [fu_base], 2
.nb2:
        mov edi, n_standard
        call px_es_nombre
        jne .nb3
        mov dword [fu_base], 0
.nb3:
        ret

; (interna) una fuente compuesta: códigos de dos octetos. Solo se lee con /ToUnicode
fu_compuesta:
        pushad
        mov edi, [fu_slot]
        or byte [edi+F_BAND], FB_DOS
        mov word [edi+F_DW], 1000
        mov eax, [fu_t0n]
        cmp eax, MAX_T0
        jae .ilegible               ; (no caben más: como si no se pudiera leer)
        inc dword [fu_t0n]
        imul eax, T0_COD * 2
        add eax, fu_t0
        mov [edi+F_T0MAP], eax
        lea ebx, [eax+T0_COD]
        mov [edi+F_T0AN], ebx
        push edi
        mov edi, eax
        xor eax, eax
        mov ecx, T0_COD / 4
        rep stosd
        pop edi
        cmp dword [fu_cid], 0
        je .sin_anchos
        ; /DW: el ancho de los que no estén en /W
        mov esi, [fu_cid]
        mov ebp, [fu_cid+4]
        mov edi, k_dw
        call px_dic
        jc .dw
        call px_ref
        call px_entero
        jc .dw
        cmp eax, 0
        jl .dw
        cmp eax, 2000
        ja .dw
        mov edi, [fu_slot]
        mov [edi+F_DW], ax
.dw:
        mov edi, [fu_slot]
        movzx eax, word [edi+F_DW]
        add eax, 4
        shr eax, 3
        mov ah, al
        mov ecx, eax
        shl eax, 16
        mov ax, cx
        mov edi, [edi+F_T0AN]
        mov ecx, T0_COD / 4
        rep stosd
        ; /W [ c [w w ...]  c1 c2 w ... ]
        mov esi, [fu_cid]
        mov ebp, [fu_cid+4]
        mov edi, k_w
        call px_dic
        jc .sin_anchos
        call px_ref
        cmp esi, ebp
        jae .sin_anchos
        cmp byte [esi], '['
        jne .sin_anchos
        inc esi
.w:
        call px_blancos
        cmp esi, ebp
        jae .sin_anchos
        cmp byte [esi], ']'
        je .sin_anchos
        call px_entero
        jnc .w1
        call px_salta
        jmp .w
.w1:
        mov ebx, eax                ; EBX=el primer código
        call px_blancos
        cmp esi, ebp
        jae .sin_anchos
        cmp byte [esi], '['
        je .w_lista
        call px_es_ref              ; ¿la lista, por referencia?
        jc .w_tramo
        push esi
        push ebp
        call px_objeto
        jc .w_ref_no
        cmp esi, ebp
        jae .w_ref_no
        cmp byte [esi], '['
        jne .w_ref_no
        call .lista
.w_ref_no:
        pop ebp
        pop esi
        jmp .w
.w_lista:
        call .lista
        jmp .w
.w_tramo:
        call px_entero              ; c1 c2 w
        jc .w
        mov ecx, eax
        call px_blancos
        call px_num
        jc .w
        call .a_octavos
.wt:
        cmp ebx, ecx
        ja .w
        cmp ebx, T0_COD
        jae .w
        test ebx, ebx
        js .w
        mov edx, [fu_slot]
        mov edx, [edx+F_T0AN]
        mov [edx+ebx], al
        inc ebx
        jmp .wt
.sin_anchos:
        call fu_tounicode
        cmp dword [fu_hay_tu], 0
        jne .x
.ilegible:
        mov edi, [fu_slot]
        or byte [edi+F_BAND], FB_ILEGIBLE
.x:
        popad
        ret
; (interna) ESI en «[»: los anchos seguidos, desde el código EBX
.lista:
        inc esi
.l:
        call px_blancos
        cmp esi, ebp
        jae .lx
        cmp byte [esi], ']'
        jne .l1
        inc esi
.lx:
        ret
.l1:
        call px_num
        jnc .l2
        call px_salta
        jmp .l
.l2:
        call .a_octavos
        cmp ebx, T0_COD
        jae .l3
        test ebx, ebx
        js .l3
        mov edx, [fu_slot]
        mov edx, [edx+F_T0AN]
        mov [edx+ebx], al
.l3:
        inc ebx
        jmp .l
; (interna) EAX=ancho en 16.16 -> AL=en octavos (de 0 a 255)
.a_octavos:
        sar eax, 16
        jns .o1
        xor eax, eax
.o1:
        add eax, 4
        shr eax, 3
        cmp eax, 255
        jbe .o2
        mov eax, 255
.o2:
        ret

; (interna) EAX=código DL=letra: a la ficha [fu_slot]
fu_pon:
        push edi
        mov edi, [fu_slot]
        mov dword [fu_hay_tu], 1
        test byte [edi+F_BAND], FB_DOS
        jnz .dos
        cmp eax, 255
        ja .x
        mov [edi+F_COD+eax], dl
        jmp .x
.dos:
        cmp eax, T0_COD
        jae .x
        mov edi, [edi+F_T0MAP]
        mov [edi+eax], dl
.x:
        pop edi
        ret

; (interna) lee el /ToUnicode de la fuente [fu_dic] (un «CMap»): las tandas
;   n beginbfchar <código> <letra> ... endbfchar
;   n beginbfrange <primero> <último> <letra> | [<letra> <letra> ...] ... endbfrange
; Si hay alguna, [fu_hay_tu]=1
fu_tounicode:
        pushad
        mov esi, [fu_dic]
        mov ebp, [fu_dic+4]
        mov edi, k_tounicode
        call px_dic
        jc .x
        call px_ref
        call ar_marca
        push eax
        call pdf_flujo
        jc .fin
        mov esi, eax
        lea ebp, [eax+ecx]
        mov dword [fu_modo], 0
.b:
        call px_blancos
        cmp esi, ebp
        jae .fin
        mov al, [esi]
        cmp al, '<'
        je .menor
        mov edi, w_bfchar
        call px_es_palabra
        jne .p1
        mov dword [fu_modo], 1
.p1:
        mov edi, w_bfrange
        call px_es_palabra
        jne .p2
        mov dword [fu_modo], 2
.p2:
        mov edi, w_endbfc
        call px_es_palabra
        jne .p3
        mov dword [fu_modo], 0
.p3:
        mov edi, w_endbfr
        call px_es_palabra
        jne .p4
        mov dword [fu_modo], 0
.p4:
        call px_salta
        jmp .b
.menor:
        lea eax, [esi+1]
        cmp eax, ebp
        jae .fin
        cmp byte [eax], '<'
        je .p4                      ; (un diccionario)
        cmp dword [fu_modo], 1
        jb .p4
        ja .tramo
        call .codigo                ; <código> <letra>
        mov ebx, eax
        call px_blancos
        call .letra
        mov dl, al
        mov eax, ebx
        call fu_pon
        jmp .b
.tramo:
        call .codigo
        mov [fu_lo], eax
        call px_blancos
        call .codigo
        mov [fu_hi], eax
        call px_blancos
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '['
        je .t_lista
        cmp byte [esi], '<'
        jne .b
        call .letra                 ; (EAX=437, ECX=su Unicode, EDX=cuántas unidades)
        mov ebx, [fu_lo]
        cmp edx, 1
        je .t1
        mov dl, al                  ; varias letras: solo vale para el primero
        mov eax, ebx
        call fu_pon
        jmp .b
.t1:
        test ebx, ebx               ; un solo tramo de 0000 a FFFF sobre 0000: el
        jnz .t2                     ; código es el Unicode (lo hace mPDF); vale
        test ecx, ecx               ; también para los que no caben en la tabla
        jnz .t2
        cmp dword [fu_hi], T0_COD
        jb .t2
        mov edx, [fu_slot]
        or byte [edx+F_BAND], FB_IDENT
.t2:
        cmp ebx, [fu_hi]
        ja .b
        cmp ebx, T0_COD             ; (hay quien pone un solo tramo de 0000 a FFFF:
        jae .b                      ; hasta donde se guarda)
        mov eax, ecx
        call uni_437
        mov dl, al
        mov eax, ebx
        call fu_pon
        inc ebx
        inc ecx
        jmp .t2
.t_lista:
        inc esi
        mov ebx, [fu_lo]
.tl:
        call px_blancos
        cmp esi, ebp
        jae .fin
        cmp byte [esi], ']'
        jne .tl1
        inc esi
        jmp .b
.tl1:
        cmp byte [esi], '<'
        je .tl2
        call px_salta
        jmp .tl
.tl2:
        call .letra
        cmp ebx, [fu_hi]
        ja .tl
        mov dl, al
        mov eax, ebx
        call fu_pon
        inc ebx
        jmp .tl
.fin:
        pop eax
        call ar_suelta
.x:
        popad
        ret
; (interna) ESI en «<...>»: un código -> EAX (los octetos, el de más peso primero)
.codigo:
        push ecx
        push edx
        push edi
        mov edi, fu_tmp
        mov ecx, 8
        call px_cadena
        mov ecx, eax
        xor eax, eax
        xor edx, edx
.c1:
        cmp edx, ecx
        jae .c2
        shl eax, 8
        mov al, [fu_tmp+edx]
        inc edx
        jmp .c1
.c2:
        pop edi
        pop edx
        pop ecx
        ret
; (interna) ESI en «<...>»: una letra en UTF-16 -> AL=la de la 437 (0: no hay),
; ECX=su Unicode, EDX=cuántas unidades traía (con más de una, AL puede ser un
; código de varias letras: «fi»)
.letra:
        push ebx
        push edi
        mov edi, fu_tmp
        mov ecx, 16
        call px_cadena
        mov edx, eax
        xor ecx, ecx
        xor eax, eax
        test edx, edx
        jz .l_fin
        movzx ecx, byte [fu_tmp]
        cmp edx, 1
        je .l_una
        shl ecx, 8
        mov cl, [fu_tmp+1]
        shr edx, 1
        cmp edx, 1
        je .l_una
        ; varias: ¿son de las que se saben escribir juntas?
        mov ebx, 1
.lv:
        xor edi, edi
.lv1:
        mov al, [exp_tab+ebx*4+edi]
        test al, al
        jz .lv_fin
        cmp edi, edx
        jae .lv_no
        cmp byte [fu_tmp+edi*2], 0
        jne .lv_no
        cmp al, [fu_tmp+edi*2+1]
        jne .lv_no
        inc edi
        cmp edi, 4
        jb .lv1
.lv_fin:
        test edi, edi
        jz .lv_no
        cmp edi, edx
        jne .lv_no
        mov eax, ebx
        jmp .l_fin
.lv_no:
        inc ebx
        cmp ebx, 16
        jb .lv
        mov eax, ecx                ; no: la primera (salvo media letra de las
        sub eax, 0xD800             ; que van en dos unidades)
        cmp eax, 0x800
        jae .l_una
        xor eax, eax
        jmp .l_fin
.l_una:
        mov eax, ecx
        call uni_437
.l_fin:
        pop edi
        pop ebx
        ret

uni_437:
        push esi
        cmp eax, 0x20
        jb .control
        cmp eax, 0x7F
        jb .x
        cmp eax, 0xA0
        jb .no
        cmp eax, 0x100
        jae .tabla
        mov al, [lat_437+eax-0xA0]
        jmp .x
.control:
        cmp eax, 9                  ; el tabulador y los fines de línea, un espacio
        je .esp
        cmp eax, 10
        je .esp
        cmp eax, 13
        jne .no
.esp:
        mov al, ' '
        jmp .x
.tabla:
        mov esi, uni_tab
.t:
        cmp word [esi], 0
        je .no
        cmp ax, [esi]
        je .t1
        add esi, 3
        jmp .t
.t1:
        cmp eax, 0xFFFF
        ja .no
        mov al, [esi+2]
        jmp .x
.no:
        xor eax, eax
.x:
        movzx eax, al
        pop esi
        ret

gl_busca:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        xor ecx, ecx                ; el largo, hasta un punto («a.sc» es «a»)
.l:
        mov al, [esi+ecx]
        test al, al
        jz .l1
        cmp al, '.'
        jne .l0
        test ecx, ecx
        jnz .l1
.l0:
        inc ecx
        jmp .l
.l1:
        xor eax, eax
        test ecx, ecx
        jz .x
        cmp ecx, 1
        jne .uni
        mov al, [esi]
        jmp .x
.uni:
        ; «uniXXXX» y «uXXXX»
        cmp ecx, 7
        jne .u
        cmp word [esi], 'un'
        jne .u
        cmp byte [esi+2], 'i'
        jne .u
        lea edi, [esi+3]
        mov edx, 4
        call .hex
        jnc .x
.u:
        cmp byte [esi], 'u'
        jne .tabla
        cmp ecx, 5
        jb .tabla
        cmp ecx, 7
        ja .tabla
        lea edi, [esi+1]
        lea edx, [ecx-1]
        call .hex
        jnc .x
.tabla:
        mov edi, gl_tab
.t:
        movzx edx, byte [edi]
        test edx, edx
        jz .no
        cmp edx, ecx
        jne .t_sig
        xor ebx, ebx
.t1:
        mov al, [edi+1+ebx]
        cmp al, [esi+ebx]
        jne .t_sig
        inc ebx
        cmp ebx, ecx
        jb .t1
        movzx eax, word [edi+1+edx]
        jmp .x
.t_sig:
        lea edi, [edi+edx+3]
        jmp .t
.no:
        xor eax, eax
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; (interna) EDI=EDX cifras hexadecimales -> EAX y CF=0; CF=1 si no lo son
.hex:
        push ebx
        xor ebx, ebx
.h:
        mov al, [edi]
        call px_hex
        jc .hx
        shl ebx, 4
        or bl, al
        inc edi
        dec edx
        jnz .h
        mov eax, ebx
        clc
.hx:
        pop ebx
        ret
