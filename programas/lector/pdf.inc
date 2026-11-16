; =============================================================================
;  pdf.inc - Lector: la estructura de un PDF (objetos, índice, flujos, páginas)
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  No sabe nada de Hispa Ventanas: trabaja sobre el archivo entero en memoria
;  ([pdf_dat], [pdf_tam]) y sobre un «almacén» ([ar_base] a [ar_fin]) que le
;  da quien lo usa, donde descomprime lo que hace falta. Así se prueba fuera,
;  en Linux (tests/tools/lector_nucleo.py), con los mismos octetos.
;
;  Un PDF es un montón de objetos numerados («12 0 obj ... endobj») y, al
;  final, un índice (xref) que dice dónde empieza cada uno. Aquí no se monta
;  ningún árbol: un valor es un «cursor», ESI=dónde está y EBP=hasta dónde se
;  puede leer, y se busca en él sobre la marcha:
;
;    px_blancos    salta blancos y comentarios
;    px_salta      salta un elemento (número, nombre, cadena, lista, diccionario)
;    px_salta_valor  lo mismo, y una referencia «12 0 R» entera
;    px_dic        ESI=diccionario EDI=clave ("Type", acabada en 0) -> CF=0 y
;                  ESI en su valor; CF=1 si no está (ESI, como estaba)
;    px_ref        si el valor es una referencia «n g R», va al objeto (hasta
;                  20 saltos); si el objeto no existe, a un «null»
;    px_objeto     EAX=número de objeto -> ESI, EBP; CF=1 si no existe
;    px_entero     -> EAX=entero (con signo), CF=1 si no lo es (ESI no se mueve)
;    px_num        -> EAX=número en coma fija 16.16, CF=1 si no lo es
;    px_es_nombre  ESI en «/Nombre», EDI="Nombre" -> ZF=1 si es ese
;    px_es_palabra ESI en una palabra, EDI="palabra" -> ZF=1 si es esa
;    px_copia_nombre  ESI en «/Nombre», EDI=búfer ECX=tamaño: lo copia (con
;                  los «#xx» deshechos y un 0 detrás) -> EAX=largo; ESI detrás
;    px_cadena     ESI en «(...)» o «<...>», EDI=búfer ECX=lo que cabe: la
;                  cadena, descifrada -> EAX=octetos; ESI detrás
;
;    pdf_abrir     -> EAX=0 si se ha abierto (y [pdf_npag] páginas); si no,
;                  PE_*: por qué no. [pdf_muchos]=1 si tiene más objetos
;                  (MAX_OBJ) o páginas (MAX_PAG) de los que caben: se ve una parte
;    pdf_flujo     ESI, EBP=un objeto con flujo -> EAX=sus datos ECX=cuántos,
;                  ya sin filtros (en el almacén); CF=1: [fl_error]=FE_*
;    ar_marca / ar_suelta   EAX=la marca del almacén: lo pedido después de la
;                  marca se devuelve entero al soltarla
;
;  Todo lo que se lee se comprueba contra el límite del cursor: un archivo
;  roto o cortado da un error o un «no está», nunca una lectura fuera.
;  Si el índice está mal (o no está), se rehace buscando los «n g obj» por
;  todo el archivo ([pdf_reparado]=1).
; =============================================================================
MAX_OBJ     equ 32768               ; objetos, a lo más
MAX_PAG     equ 4000                ; páginas
PG_TAM      equ 40                  ; de cada página: diccionario (p, lím), recursos
PG_DIC      equ 0                   ; (p, lím), caja (x0, y0, x1, y1) y giro
PG_RES      equ 8
PG_CAJA     equ 16
PG_GIRO     equ 32
PG_NIVEL    equ 44                  ; lo que se hereda al bajar por el árbol (11 dd)

PE_NOPDF    equ 1                   ; no es un PDF
PE_ROTO     equ 2                   ; no se encuentra ni se puede rehacer su índice
PE_CIFRADO  equ 3                   ; está cifrado (con contraseña o permisos)
PE_MEMORIA  equ 4                   ; no cabe en la memoria que hay
PE_SINPAG   equ 5                   ; no tiene páginas

FE_NOFLUJO  equ 1                   ; el objeto no lleva flujo
FE_FILTRO   equ 2                   ; un filtro que no se sabe deshacer ([fl_filtro])
FE_ROTO     equ 3                   ; los datos están estropeados
FE_MEMORIA  equ 4                   ; no cabe en el almacén

%macro PDF_BSS 0
            alignb 4
pdf_dat     resd 1                  ; el archivo, entero
pdf_tam     resd 1
ar_base     resd 1                  ; el almacén
ar_fin      resd 1
ar_tope     resd 1                  ; lo que hay ocupado
ar_fijo     resd 1                  ; hasta aquí, lo que dura mientras el documento
pdf_npag    resd 1
pdf_ver     resd 1                  ; la versión: mayor * 16 + menor
pdf_reparado resd 1
pdf_muchos  resd 1                  ; tiene más objetos o páginas de los que caben
pdf_raiz    resd 2                  ; el catálogo (p, lím)
pdf_info_p  resd 2                  ; el diccionario de información (0: no hay)
pdf_cifrado resd 1
px_prof     resd 1
xr_saltos   resd 1
xr_prev     resd 1
xr_mixto    resd 1
xr_w        resd 3
xr_n        resd 1
xr_dic      resd 2
xr_dat      resd 2
xr_ind      resd 2
os_num      resd 1
os_n        resd 1
os_first    resd 1
os_dat      resd 2
os_cab      resd 1
os_ant      resd 1
os_todos    resd 1
pg_prof     resd 1
pg_vistos   resd 1
pg_her      resd 7                  ; lo que hereda una página: recursos, caja, giro
pg_pila     resd 44                 ; los nodos por los que se ha bajado hasta el de ahora
fl_dic      resd 2
fl_p        resd 1
fl_n        resd 1
fl_error    resd 1
fl_parcial  resd 1
fl_fa       resd 2                  ; la lista de filtros
fl_pa       resd 2                  ; y la de sus parámetros (0: no hay)
fl_parm     resd 2                  ; los parámetros del filtro de ahora (0: no hay)
fl_pred     resd 1
fl_cols     resd 1
fl_bpp      resd 1
fl_fila     resd 1
fl_ant      resd 1
fl_tmp      resd 4
fl_filtro   resb 32                 ; el nombre del filtro que no se sabe
xr_t        resb MAX_OBJ            ; de cada objeto: 0 no se sabe, 1 en el archivo
            alignb 4                ; (a=dónde), 2 en un flujo de objetos (a=cuál,
xr_a        resd MAX_OBJ            ; b=el cuánto), 3 ya en memoria (a=p, b=lím),
xr_b        resd MAX_OBJ            ; 4 libre
pg_tab      resb MAX_PAG * PG_TAM
%endmacro

; las clases de carácter: 1 blanco, 2 delimitador, 0 corriente
px_clase:
%assign i 0
%rep 256
  %if i = 0 || i = 9 || i = 10 || i = 12 || i = 13 || i = 32
        db 1
  %elif i = '(' || i = ')' || i = '<' || i = '>' || i = '[' || i = ']' || i = '{' || i = '}' || i = '/' || i = '%'
        db 2
  %else
        db 0
  %endif
  %assign i i + 1
%endrep
px_nulo     db "null", 0, 0, 0, 0

k_type      db "Type", 0
k_length    db "Length", 0
k_filter    db "Filter", 0
k_parms     db "DecodeParms", 0
k_predictor db "Predictor", 0
k_columns   db "Columns", 0
k_colors    db "Colors", 0
k_bpc       db "BitsPerComponent", 0
k_root      db "Root", 0
k_info      db "Info", 0
k_encrypt   db "Encrypt", 0
k_prev      db "Prev", 0
k_xrefstm   db "XRefStm", 0
k_size      db "Size", 0
k_w         db "W", 0
k_index     db "Index", 0
k_n         db "N", 0
k_first     db "First", 0
k_pages     db "Pages", 0
k_kids      db "Kids", 0
k_resources db "Resources", 0
k_mediabox  db "MediaBox", 0
k_cropbox   db "CropBox", 0
k_rotate    db "Rotate", 0
k_catalog   db "Catalog", 0
k_objstm    db "ObjStm", 0
k_xref_t    db "XRef", 0
p_obj       db "obj", 0
p_stream    db "stream", 0
p_xref      db "xref", 0
p_trailer   db "trailer", 0
p_startxref db "startxref"
p_endstream db "endstream"
f_flate     db "FlateDecode", 0
f_fl        db "Fl", 0
f_ahx       db "ASCIIHexDecode", 0
f_ahx2      db "AHx", 0
f_a85       db "ASCII85Decode", 0
f_a852      db "A85", 0
f_rl        db "RunLengthDecode", 0
f_rl2       db "RL", 0

; -----------------------------------------------------------------------------
;  El almacén
; -----------------------------------------------------------------------------
ar_marca:
        mov eax, [ar_tope]
        ret
ar_suelta:
        mov [ar_tope], eax
        ret
; ECX=octetos -> EAX=sitio (sin poner a cero); CF=1 si no cabe
ar_pide:
        push ecx
        add ecx, 3
        and ecx, ~3
        mov eax, [ar_fin]
        sub eax, [ar_tope]
        cmp eax, ecx
        jb .no
        mov eax, [ar_tope]
        add [ar_tope], ecx
        pop ecx
        clc
        ret
.no:
        pop ecx
        stc
        ret

; -----------------------------------------------------------------------------
;  Leer elementos
; -----------------------------------------------------------------------------
px_blancos:
        push eax
.b:
        cmp esi, ebp
        jae .x
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 1
        jne .com
        inc esi
        jmp .b
.com:
        cmp al, '%'
        jne .x
.c:
        inc esi
        cmp esi, ebp
        jae .x
        mov al, [esi]
        cmp al, 10
        je .b
        cmp al, 13
        je .b
        jmp .c
.x:
        pop eax
        ret

; (interna) salta los caracteres corrientes que haya (puede que ninguno)
px_corrientes:
        push eax
.b:
        cmp esi, ebp
        jae .x
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 0
        jne .x
        inc esi
        jmp .b
.x:
        pop eax
        ret

; (interna) ESI en «(»: salta la cadena, con sus paréntesis de dentro
px_salta_cad:
        push eax
        push ecx
        xor ecx, ecx
.b:
        cmp esi, ebp
        jae .x
        mov al, [esi]
        inc esi
        cmp al, '\'
        jne .b1
        cmp esi, ebp
        jae .x
        inc esi
        jmp .b
.b1:
        cmp al, '('
        jne .b2
        inc ecx
        jmp .b
.b2:
        cmp al, ')'
        jne .b
        dec ecx
        jnz .b
.x:
        pop ecx
        pop eax
        ret

; salta un elemento. Siempre avanza (salvo en el límite): nunca da vueltas
px_salta:
        push eax
        call px_blancos
        cmp esi, ebp
        jae .x
        inc dword [px_prof]
        cmp dword [px_prof], 100    ; listas dentro de listas sin fin: se deja
        ja .abandona
        mov al, [esi]
        cmp al, '/'
        je .nombre
        cmp al, '('
        je .cadena
        cmp al, '<'
        je .menor
        cmp al, '['
        je .lista
        mov eax, esi
        call px_corrientes
        cmp eax, esi
        jne .fin
        inc esi                     ; un delimitador suelto
        jmp .fin
.nombre:
        inc esi
        call px_corrientes
        jmp .fin
.cadena:
        call px_salta_cad
        jmp .fin
.menor:
        lea eax, [esi+1]
        cmp eax, ebp
        jae .hex
        cmp byte [eax], '<'
        je .dic
.hex:
        inc esi
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '>'
        jne .hex
        inc esi
        jmp .fin
.dic:
        add esi, 2
.d:
        call px_blancos
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '>'
        jne .d2
        inc esi
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '>'
        jne .d
        inc esi
        jmp .fin
.d2:
        call px_salta
        jmp .d
.lista:
        inc esi
.l:
        call px_blancos
        cmp esi, ebp
        jae .fin
        cmp byte [esi], ']'
        jne .l2
        inc esi
        jmp .fin
.l2:
        call px_salta
        jmp .l
.abandona:
        mov esi, ebp
.fin:
        dec dword [px_prof]
.x:
        pop eax
        ret

; (interna) ¿hay en ESI una referencia «n g R»? CF=0: sí, EAX=n y ESI detrás;
; CF=1: no (ESI no se mueve)
px_es_ref:
        push ebx
        push esi
        call px_blancos
        cmp esi, ebp
        jae .no
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .no
        call px_entero
        jc .no
        mov ebx, eax
        call px_blancos
        cmp esi, ebp
        jae .no
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .no
        call px_entero
        jc .no
        call px_blancos
        cmp esi, ebp
        jae .no
        cmp byte [esi], 'R'
        jne .no
        inc esi
        cmp esi, ebp
        jae .si
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 0
        je .no
.si:
        mov eax, ebx
        add esp, 4
        pop ebx
        clc
        ret
.no:
        pop esi
        pop ebx
        stc
        ret

px_salta_valor:
        push eax
        call px_es_ref
        jnc .x
        call px_salta
.x:
        pop eax
        ret

px_dic:
        push eax
        push esi
        call px_blancos
        lea eax, [esi+2]
        cmp eax, ebp
        ja .no
        cmp word [esi], '<<'
        jne .no
        add esi, 2
.b:
        call px_blancos
        cmp esi, ebp
        jae .no
        mov al, [esi]
        cmp al, '>'
        je .no
        cmp al, '/'
        je .clave
        call px_salta               ; (algo que no es una clave: se pasa)
        jmp .b
.clave:
        inc esi
        push edi
.c:
        mov al, [edi]
        test al, al
        jz .kfin
        cmp esi, ebp
        jae .dist
        cmp al, [esi]
        jne .dist
        inc esi
        inc edi
        jmp .c
.kfin:
        cmp esi, ebp
        jae .igual
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 0
        jne .igual
.dist:
        pop edi
        call px_corrientes
        call px_salta_valor
        jmp .b
.igual:
        pop edi
        call px_blancos
        add esp, 4
        pop eax
        clc
        ret
.no:
        pop esi
        pop eax
        stc
        ret

px_ref:
        push eax
        push ecx
        mov ecx, 20
.b:
        call px_es_ref
        jc .x
        call px_objeto
        jc .nulo
        call px_blancos
        dec ecx
        jnz .b
.nulo:
        mov esi, px_nulo
        lea ebp, [esi+4]
.x:
        pop ecx
        pop eax
        ret

px_objeto:
        push eax
        push ebx
        cmp eax, MAX_OBJ
        jae .no
        movzx ebx, byte [xr_t+eax]
        cmp bl, 3
        je .directo
        cmp bl, 1
        jne .no
        mov esi, [xr_a+eax*4]
        cmp esi, [pdf_tam]
        jae .no
        add esi, [pdf_dat]
        mov ebp, [pdf_dat]
        add ebp, [pdf_tam]
        call px_blancos
        call px_entero              ; «n g obj»
        jc .no
        call px_blancos
        call px_entero
        jc .no
        call px_blancos
        push edi
        mov edi, p_obj
        call px_es_palabra
        pop edi
        jne .no
        add esi, 3
        call px_blancos
        jmp .si
.directo:
        mov esi, [xr_a+eax*4]
        mov ebp, [xr_b+eax*4]
.si:
        pop ebx
        pop eax
        clc
        ret
.no:
        pop ebx
        pop eax
        stc
        ret

px_entero:
        push ebx
        push ecx
        push edx
        push esi
        xor ecx, ecx                ; el signo
        cmp esi, ebp
        jae .no
        mov al, [esi]
        cmp al, '+'
        je .signo
        cmp al, '-'
        jne .dig
        inc ecx
.signo:
        inc esi
.dig:
        xor eax, eax
        xor edx, edx                ; cuántas cifras
.d:
        cmp esi, ebp
        jae .fin
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .fin
        inc esi
        inc edx
        cmp eax, 214748363          ; (no pasa de 2^31 - 1)
        ja .tope
        imul eax, 10
        add eax, ebx
        jmp .d
.tope:
        mov eax, 0x7FFFFFFF
        jmp .d
.fin:
        test edx, edx
        jz .no
        test ecx, ecx
        jz .pos
        neg eax
.pos:
        add esp, 4
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.no:
        pop esi
        pop edx
        pop ecx
        pop ebx
        stc
        ret

; un número, a coma fija 16.16 (de -32768 a 32767,99998: lo que pase, al tope)
px_num:
        push ebx
        push ecx
        push edx
        push edi
        push esi
        xor ecx, ecx                ; el signo
        cmp esi, ebp
        jae .no
        mov al, [esi]
        cmp al, '+'
        je .signo
        cmp al, '-'
        jne .ent
        inc ecx
.signo:
        inc esi
.ent:
        xor eax, eax                ; la parte entera
        xor edi, edi                ; cuántas cifras, en total
.e:
        cmp esi, ebp
        jae .fin_e
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .fin_e
        inc esi
        inc edi
        cmp eax, 32767
        ja .e
        imul eax, 10
        add eax, ebx
        jmp .e
.fin_e:
        cmp eax, 32767
        jbe .e1
        mov eax, 32767
.e1:
        shl eax, 16
        cmp esi, ebp
        jae .hecho
        cmp byte [esi], '.'
        jne .hecho
        inc esi
        push eax
        xor eax, eax                ; los decimales: hasta seis cifras
        mov edx, 1
.f:
        cmp esi, ebp
        jae .fin_f
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .fin_f
        inc esi
        inc edi
        cmp edx, 1000000
        jae .f
        imul eax, 10
        add eax, ebx
        imul edx, 10
        jmp .f
.fin_f:
        mov ebx, edx
        mov edx, 65536
        mul edx
        div ebx
        pop edx
        add eax, edx
.hecho:
        test edi, edi
        jz .no
        test ecx, ecx
        jz .pos
        neg eax
.pos:
        add esp, 4
        pop edi
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.no:
        pop esi
        pop edi
        pop edx
        pop ecx
        pop ebx
        stc
        ret

px_es_nombre:
        cmp esi, ebp
        jae .no
        cmp byte [esi], '/'
        jne .no
        inc esi
        call px_es_palabra
        lea esi, [esi-1]
        ret
.no:
        cmp esi, 0                  ; ZF=0 (ESI nunca es 0)
        ret

px_es_palabra:
        push eax
        push esi
        push edi
.c:
        mov al, [edi]
        test al, al
        jz .fin
        cmp esi, ebp
        jae .no
        cmp al, [esi]
        jne .no
        inc esi
        inc edi
        jmp .c
.fin:
        cmp esi, ebp
        jae .si
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 0
        je .no
.si:
        xor eax, eax                ; ZF=1
        jmp .x
.no:
        or eax, 1                   ; ZF=0
.x:
        pop edi
        pop esi
        pop eax
        ret

; (interna) AL=una cifra hexadecimal -> AL=su valor; CF=1 si no lo es
px_hex:
        cmp al, '0'
        jb .no
        cmp al, '9'
        jbe .d
        or al, 0x20
        cmp al, 'a'
        jb .no
        cmp al, 'f'
        ja .no
        sub al, 'a' - 10
        clc
        ret
.d:
        sub al, '0'
        clc
        ret
.no:
        stc
        ret

px_copia_nombre:
        push ebx
        push ecx
        push edx
        push edi
        mov edx, edi
        dec ecx
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '/'
        jne .fin
        inc esi
.c:
        cmp esi, ebp
        jae .fin
        movzx eax, byte [esi]
        cmp byte [px_clase+eax], 0
        jne .fin
        inc esi
        cmp al, '#'
        jne .pon
        lea ebx, [esi+2]
        cmp ebx, ebp
        ja .pon
        mov al, [esi]
        call px_hex
        jc .almo
        mov bl, al
        mov al, [esi+1]
        call px_hex
        jc .almo
        shl bl, 4
        or al, bl
        add esi, 2
        jmp .pon
.almo:
        mov al, '#'
.pon:
        test ecx, ecx
        jle .c
        stosb
        dec ecx
        jmp .c
.fin:
        mov byte [edi], 0
        mov eax, edi
        sub eax, edx
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

px_cadena:
        push ebx
        push ecx
        push edx
        push edi
        mov edx, edi                ; EDX=el principio
        add ecx, edi                ; ECX=el final de lo que cabe
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '('
        je .literal
        cmp byte [esi], '<'
        jne .fin
        ; hexadecimal: de dos en dos cifras; una suelta al final, con un 0
        inc esi
        xor ebx, ebx                ; BH=1: hay media
.h:
        cmp esi, ebp
        jae .h_fin
        mov al, [esi]
        inc esi
        cmp al, '>'
        je .h_fin
        call px_hex
        jc .h                       ; (blancos y lo que no sea cifra)
        test bh, bh
        jnz .h2
        mov bl, al
        shl bl, 4
        mov bh, 1
        jmp .h
.h2:
        or al, bl
        xor ebx, ebx
        cmp edi, ecx
        jae .h
        stosb
        jmp .h
.h_fin:
        test bh, bh
        jz .fin
        cmp edi, ecx
        jae .fin
        mov al, bl
        stosb
        jmp .fin
.literal:
        inc esi
        xor ebx, ebx                ; paréntesis abiertos dentro
.l:
        cmp esi, ebp
        jae .fin
        mov al, [esi]
        inc esi
        cmp al, '\'
        je .barra
        cmp al, '('
        jne .l1
        inc ebx
        jmp .pon
.l1:
        cmp al, ')'
        jne .l2
        test ebx, ebx
        jz .fin
        dec ebx
        jmp .pon
.l2:
        cmp al, 13                  ; un fin de línea, del modo que sea, es un 10
        jne .pon
        mov al, 10
        cmp esi, ebp
        jae .pon
        cmp byte [esi], 10
        jne .pon
        inc esi
.pon:
        cmp edi, ecx
        jae .l
        stosb
        jmp .l
.barra:
        cmp esi, ebp
        jae .fin
        mov al, [esi]
        inc esi
        cmp al, 'n'
        jne .b1
        mov al, 10
        jmp .pon
.b1:
        cmp al, 'r'
        jne .b2
        mov al, 13
        jmp .pon
.b2:
        cmp al, 't'
        jne .b3
        mov al, 9
        jmp .pon
.b3:
        cmp al, 'b'
        jne .b4
        mov al, 8
        jmp .pon
.b4:
        cmp al, 'f'
        jne .b5
        mov al, 12
        jmp .pon
.b5:
        cmp al, 13                  ; barra y fin de línea: la cadena sigue
        jne .b6
        cmp esi, ebp
        jae .l
        cmp byte [esi], 10
        jne .l
        inc esi
        jmp .l
.b6:
        cmp al, 10
        je .l
        cmp al, '0'                 ; hasta tres cifras en octal
        jb .pon
        cmp al, '7'
        ja .pon
        sub al, '0'
        mov ah, 2
        push ebx
.oct:
        cmp esi, ebp
        jae .pon_o
        mov bl, [esi]
        cmp bl, '0'
        jb .pon_o
        cmp bl, '7'
        ja .pon_o
        inc esi
        shl al, 3
        sub bl, '0'
        or al, bl
        dec ah
        jnz .oct
.pon_o:
        pop ebx
        jmp .pon
.fin:
        mov eax, edi
        sub eax, edx
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

; busca ECX octetos de EDI entre ESI y EBP -> CF=0 y EAX=dónde; CF=1 si no están
mem_busca:
        push ebx
        push edx
        push esi
        mov edx, ebp
        sub edx, ecx                ; el último sitio donde cabe
.b:
        cmp esi, edx
        ja .no
        mov al, [edi]
        cmp al, [esi]
        jne .sig
        xor ebx, ebx
.c:
        inc ebx
        cmp ebx, ecx
        jae .si
        mov al, [edi+ebx]
        cmp al, [esi+ebx]
        je .c
.sig:
        inc esi
        jmp .b
.si:
        mov eax, esi
        pop esi
        pop edx
        pop ebx
        clc
        ret
.no:
        pop esi
        pop edx
        pop ebx
        stc
        ret

; -----------------------------------------------------------------------------
;  Flujos
; -----------------------------------------------------------------------------
; (interna) ESI, EBP=un objeto con flujo -> [fl_p], [fl_n]: sus datos tal como
; están en el archivo; CF=1 si no lleva flujo. Deja el diccionario en [fl_dic]
fl_crudo:
        pushad
        call px_blancos
        mov [fl_dic], esi
        mov [fl_dic+4], ebp
        lea eax, [esi+2]
        cmp eax, ebp
        ja .no
        cmp word [esi], '<<'
        jne .no
        call px_salta
        call px_blancos
        mov edi, p_stream
        call px_es_palabra
        jne .no
        add esi, 6
        cmp esi, ebp                ; detrás de «stream» va un fin de línea
        jae .no
        cmp byte [esi], 13
        jne .s1
        inc esi
        cmp esi, ebp
        jae .no
.s1:
        cmp byte [esi], 10
        jne .s2
        inc esi
.s2:
        mov [fl_p], esi
        ; el largo que dice; vale si detrás está «endstream»
        push esi
        push ebp
        mov esi, [fl_dic]
        mov edi, k_length
        call px_dic
        jc .sin_largo
        call px_ref
        call px_entero
        jc .sin_largo
        pop ebp
        pop esi
        test eax, eax
        js .busca
        mov ebx, ebp
        sub ebx, esi
        cmp eax, ebx
        ja .busca
        mov [fl_n], eax
        push esi
        add esi, eax
        call px_blancos
        lea ebx, [esi+9]
        cmp ebx, ebp
        ja .mal_largo
        cmp dword [esi], 'ends'
        jne .mal_largo
        cmp dword [esi+4], 'trea'
        jne .mal_largo
        pop esi
        jmp .si
.mal_largo:
        pop esi
        jmp .busca
.sin_largo:
        pop ebp
        pop esi
.busca:
        ; el largo no vale: hasta el primer «endstream» (o hasta el final)
        mov edi, p_endstream
        mov ecx, 9
        call mem_busca
        jnc .b1
        mov eax, ebp
.b1:
        cmp eax, esi                ; sin el fin de línea de delante
        jbe .b3
        cmp byte [eax-1], 10
        jne .b2
        dec eax
        cmp eax, esi
        jbe .b3
.b2:
        cmp byte [eax-1], 13
        jne .b3
        dec eax
.b3:
        sub eax, esi
        mov [fl_n], eax
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

pdf_flujo:
        push ebx
        push edx
        push esi
        push edi
        push ebp
        mov dword [fl_parcial], 0
        mov dword [fl_error], FE_NOFLUJO
        mov byte [fl_filtro], 0
        call fl_crudo
        jc .mal
        mov esi, [fl_dic]
        mov ebp, [fl_dic+4]
        mov edi, k_filter
        call px_dic
        jc .bien                    ; sin filtros: los datos, donde están
        call px_ref
        ; los parámetros: un diccionario, o una lista pareja a la de filtros
        mov dword [fl_pa], 0
        mov dword [fl_parm], 0
        push esi
        push ebp
        mov esi, [fl_dic]
        mov ebp, [fl_dic+4]
        mov edi, k_parms
        call px_dic
        jc .sin_parm
        call px_ref
        cmp esi, ebp
        jae .sin_parm
        cmp byte [esi], '['
        jne .parm_dic
        inc esi
        mov [fl_pa], esi
        mov [fl_pa+4], ebp
        jmp .sin_parm
.parm_dic:
        mov [fl_parm], esi
        mov [fl_parm+4], ebp
.sin_parm:
        pop ebp
        pop esi
        cmp esi, ebp
        jae .bien
        cmp byte [esi], '['
        je .lista
        cmp byte [esi], '/'
        jne .bien                   ; («/Filter null»: sin filtros)
        call fl_aplica              ; un solo filtro
        jc .mal
        jmp .bien
.lista:
        inc esi
        mov [fl_fa], esi
        mov [fl_fa+4], ebp
.l:
        mov esi, [fl_fa]
        mov ebp, [fl_fa+4]
        call px_blancos
        cmp esi, ebp
        jae .bien
        cmp byte [esi], ']'
        je .bien
        push esi
        call px_salta_valor
        mov [fl_fa], esi
        pop esi
        cmp dword [fl_pa], 0        ; sus parámetros, si los hay
        je .l1
        push esi
        push ebp
        mov dword [fl_parm], 0
        mov esi, [fl_pa]
        mov ebp, [fl_pa+4]
        call px_blancos
        cmp esi, ebp
        jae .l0
        cmp byte [esi], ']'
        je .l0
        push esi
        call px_salta_valor
        mov [fl_pa], esi
        pop esi
        call px_ref
        mov [fl_parm], esi
        mov [fl_parm+4], ebp
.l0:
        pop ebp
        pop esi
.l1:
        call px_ref
        call fl_aplica
        jc .mal
        jmp .l
.bien:
        mov eax, [fl_p]
        mov ecx, [fl_n]
        clc
        jmp .x
.mal:
        xor eax, eax
        xor ecx, ecx
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ebx
        ret

; (interna) ESI=el nombre de un filtro: lo aplica a [fl_p], [fl_n]. CF=1: [fl_error]
fl_aplica:
        pushad
        mov edi, f_flate
        call px_es_nombre
        je .flate
        mov edi, f_fl
        call px_es_nombre
        je .flate
        mov edi, f_ahx
        call px_es_nombre
        je .ahx
        mov edi, f_ahx2
        call px_es_nombre
        je .ahx
        mov edi, f_a85
        call px_es_nombre
        je .a85
        mov edi, f_a852
        call px_es_nombre
        je .a85
        mov edi, f_rl
        call px_es_nombre
        je .rl
        mov edi, f_rl2
        call px_es_nombre
        je .rl
        mov edi, fl_filtro          ; no se sabe: su nombre, para decirlo
        mov ecx, 32
        call px_copia_nombre
        mov dword [fl_error], FE_FILTRO
        jmp .mal
.flate:
        mov esi, [fl_p]
        mov ecx, [fl_n]
        mov edi, [ar_tope]
        mov edx, [ar_fin]
        sub edx, edi
        call desinfla_zlib
        jnc .f_ok
        cmp dword [dsf_error], DSF_E_LLENO
        je .sin_sitio
        test eax, eax               ; roto o cortado: lo que haya salido vale
        jz .roto
        mov dword [fl_parcial], 1
.f_ok:
        mov [fl_p], edi
        mov [fl_n], eax
        call fl_predictor
        jc .roto
        mov ecx, [fl_n]
        call ar_pide
        jmp .bien
.ahx:
        mov ecx, [fl_n]             ; (lo que sale no pasa de la mitad)
        shr ecx, 1
        inc ecx
        call ar_pide
        jc .sin_sitio
        mov edi, eax
        mov esi, [fl_p]
        mov ebp, esi
        add ebp, [fl_n]
        mov [fl_p], edi
        xor ebx, ebx
.x1:
        cmp esi, ebp
        jae .x_fin
        mov al, [esi]
        inc esi
        cmp al, '>'
        je .x_fin
        call px_hex
        jc .x1
        test bh, bh
        jnz .x2
        mov bl, al
        shl bl, 4
        mov bh, 1
        jmp .x1
.x2:
        or al, bl
        xor ebx, ebx
        stosb
        jmp .x1
.x_fin:
        test bh, bh
        jz .x3
        mov al, bl
        stosb
.x3:
        sub edi, [fl_p]
        mov [fl_n], edi
        jmp .bien
.a85:
        mov ecx, [fl_n]             ; (de cada 5 salen 4; una «z», 4)
        shl ecx, 2
        add ecx, 8
        call ar_pide
        jc .sin_sitio
        mov edi, eax
        mov esi, [fl_p]
        mov ebp, esi
        add ebp, [fl_n]
        mov [fl_p], edi
        xor ebx, ebx                ; EBX=el grupo  ECX=cuántas letras lleva
        xor ecx, ecx
.a1:
        cmp esi, ebp
        jae .a_fin
        movzx eax, byte [esi]
        inc esi
        cmp al, '~'
        je .a_fin
        cmp al, 'z'
        jne .a2
        test ecx, ecx
        jnz .a1
        xor eax, eax
        stosd
        jmp .a1
.a2:
        sub eax, '!'
        cmp eax, 84
        ja .a1                      ; blancos y lo demás
        imul ebx, 85
        add ebx, eax
        inc ecx
        cmp ecx, 5
        jb .a1
        mov eax, ebx                ; cuatro octetos, el de más peso primero
        xchg al, ah
        rol eax, 16
        xchg al, ah
        stosd
        xor ebx, ebx
        xor ecx, ecx
        jmp .a1
.a_fin:
        cmp ecx, 2                  ; el último grupo, a medias: n letras dan n-1
        jb .a4
        push ecx
        mov edx, 5
        sub edx, ecx
.a3:
        imul ebx, 85
        add ebx, 84
        dec edx
        jnz .a3
        pop ecx
        dec ecx
.a5:
        rol ebx, 8
        mov al, bl
        stosb
        dec ecx
        jnz .a5
.a4:
        sub edi, [fl_p]
        mov [fl_n], edi
        jmp .bien
.rl:
        mov esi, [fl_p]
        mov ebp, esi
        add ebp, [fl_n]
        mov edi, [ar_tope]
        mov edx, [ar_fin]
        mov [fl_p], edi
.r1:
        cmp esi, ebp
        jae .r_fin
        movzx ecx, byte [esi]
        inc esi
        cmp ecx, 128
        je .r_fin
        ja .r_rep
        inc ecx                     ; de 0 a 127: tantos + 1 octetos, tal cual
        mov eax, ebp
        sub eax, esi
        cmp ecx, eax
        jbe .r2
        mov ecx, eax
.r2:
        mov eax, edx
        sub eax, edi
        cmp ecx, eax
        ja .sin_sitio
        rep movsb
        jmp .r1
.r_rep:
        neg ecx                     ; de 129 a 255: 257 - n veces el siguiente
        add ecx, 257
        cmp esi, ebp
        jae .r_fin
        lodsb
        mov ebx, edx
        sub ebx, edi
        cmp ecx, ebx
        ja .sin_sitio
        rep stosb
        jmp .r1
.r_fin:
        sub edi, [fl_p]
        mov [fl_n], edi
        mov ecx, edi
        call ar_pide
.bien:
        popad
        clc
        ret
.sin_sitio:
        mov dword [fl_error], FE_MEMORIA
        jmp .mal
.roto:
        mov dword [fl_error], FE_ROTO
.mal:
        popad
        stc
        ret

; (interna) el parámetro entero EDI de [fl_parm] -> EAX (si no está, EAX no cambia)
fl_parametro:
        push esi
        push ebp
        push ebx
        mov ebx, eax
        mov esi, [fl_parm]
        mov ebp, [fl_parm+4]
        call px_dic
        jc .x
        call px_ref
        call px_entero
        jc .x
        mov ebx, eax
.x:
        mov eax, ebx
        pop ebx
        pop ebp
        pop esi
        ret

; (interna) deshace el «predictor» de [fl_p], [fl_n], en el sitio: los de PNG
; (10 a 15: cada fila lleva delante cómo está hecha) y el de TIFF (2). CF=1 si
; es uno que no se sabe
fl_predictor:
        pushad
        cmp dword [fl_parm], 0
        je .bien
        mov eax, 1
        mov edi, k_predictor
        call fl_parametro
        cmp eax, 1
        jbe .bien
        mov [fl_pred], eax
        mov eax, 1
        mov edi, k_columns
        call fl_parametro
        mov [fl_cols], eax
        mov eax, 1
        mov edi, k_colors
        call fl_parametro
        mov ebx, eax
        mov eax, 8
        mov edi, k_bpc
        call fl_parametro
        cmp ebx, 1                  ; que no salgan cuentas disparatadas
        jb .mal
        cmp ebx, 32
        ja .mal
        cmp eax, 1
        jb .mal
        cmp eax, 16
        ja .mal
        cmp dword [fl_cols], 1
        jb .mal
        cmp dword [fl_cols], 100000
        ja .mal
        imul ebx, eax               ; bits de un punto
        mov eax, ebx
        add eax, 7
        shr eax, 3
        mov [fl_bpp], eax           ; octetos de un punto (al menos 1)
        imul ebx, [fl_cols]
        add ebx, 7
        shr ebx, 3
        mov [fl_fila], ebx          ; octetos de una fila
        cmp dword [fl_pred], 2
        je .tiff
        cmp dword [fl_pred], 10
        jb .mal
        mov esi, [fl_p]             ; ESI=de dónde se lee  EDI=dónde queda
        mov edi, esi
        mov ebp, esi
        add ebp, [fl_n]
        mov dword [fl_ant], 0       ; la fila de arriba (0: no hay)
.fila:
        cmp esi, ebp
        jae .png_fin
        movzx edx, byte [esi]       ; cómo está hecha la fila
        inc esi
        mov ecx, ebp
        sub ecx, esi
        cmp ecx, [fl_fila]
        jbe .f1
        mov ecx, [fl_fila]
.f1:
        test ecx, ecx
        jz .png_fin
        mov [fl_tmp], edi           ; el principio de esta fila
        xor ebx, ebx                ; EBX=el octeto de la fila
.oct:
        ; AL=izquierda, AH=arriba, [fl_tmp+4]=arriba a la izquierda
        push ecx
        xor eax, eax
        mov dword [fl_tmp+4], 0
        cmp ebx, [fl_bpp]
        jb .o1
        mov ecx, edi
        sub ecx, [fl_bpp]
        mov al, [ecx]
.o1:
        mov ecx, [fl_ant]
        test ecx, ecx
        jz .o2
        mov ah, [ecx+ebx]
        cmp ebx, [fl_bpp]
        jb .o2
        add ecx, ebx
        sub ecx, [fl_bpp]
        movzx ecx, byte [ecx]
        mov [fl_tmp+4], ecx
.o2:
        cmp edx, 1
        jb .pon0
        je .pon_a
        cmp edx, 3
        jb .pon_b
        je .pon_media
        cmp edx, 4
        jne .pon0
        ; Paeth: de izquierda, arriba y arriba-izquierda, el que más se acerca
        ; a izquierda + arriba - arriba-izquierda
        push edx
        push ebx
        movzx ebx, al               ; a
        movzx edx, ah               ; b
        mov ecx, ebx
        add ecx, edx
        sub ecx, [fl_tmp+4]         ; p
        push ebp
        mov ebp, ecx
        sub ebp, ebx
        jns .p1
        neg ebp
.p1:
        mov [fl_tmp+8], ebp         ; pa
        mov ebp, ecx
        sub ebp, edx
        jns .p2
        neg ebp
.p2:
        mov [fl_tmp+12], ebp        ; pb
        sub ecx, [fl_tmp+4]
        jns .p3
        neg ecx                     ; pc
.p3:
        pop ebp
        mov eax, ebx                ; ¿a?
        mov ebx, [fl_tmp+8]
        cmp ebx, [fl_tmp+12]
        ja .p4
        cmp ebx, ecx
        jbe .p5
.p4:
        mov eax, edx                ; ¿b?
        cmp [fl_tmp+12], ecx
        jbe .p5
        mov eax, [fl_tmp+4]         ; c
.p5:
        pop ebx
        pop edx
        jmp .suma
.pon0:
        xor eax, eax
        jmp .suma
.pon_a:
        and eax, 0xFF
        jmp .suma
.pon_b:
        movzx eax, ah
        jmp .suma
.pon_media:
        movzx ecx, ah
        and eax, 0xFF
        add eax, ecx
        shr eax, 1
.suma:
        add al, [esi]
        inc esi
        stosb
        pop ecx
        inc ebx
        dec ecx
        jnz .oct
        mov eax, [fl_tmp]
        mov [fl_ant], eax
        jmp .fila
.png_fin:
        sub edi, [fl_p]
        mov [fl_n], edi
        jmp .bien
.tiff:
        ; cada octeto, más el del punto de su izquierda (solo con 8 bits)
        mov esi, [fl_p]
        mov ebp, esi
        add ebp, [fl_n]
.t_fila:
        mov ecx, [fl_fila]
        mov ebx, [fl_bpp]
        mov eax, ebp
        sub eax, esi
        cmp eax, ecx
        jb .bien
        test ecx, ecx
        jz .bien
.t_oct:
        cmp ebx, 0
        jg .t1
        mov edx, esi
        sub edx, [fl_bpp]
        mov al, [edx]
        add [esi], al
.t1:
        dec ebx
        inc esi
        dec ecx
        jnz .t_oct
        jmp .t_fila
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; -----------------------------------------------------------------------------
;  Abrir: el índice
; -----------------------------------------------------------------------------
pdf_abrir:
        pushad
        cld
        mov eax, [ar_base]
        mov [ar_tope], eax
        mov [ar_fijo], eax
        xor eax, eax
        mov [pdf_npag], eax
        mov [pdf_reparado], eax
        mov [pdf_muchos], eax
        mov [pdf_info_p], eax
        mov [pdf_raiz], eax
        mov [pdf_cifrado], eax
        mov [px_prof], eax
        mov [pdf_ver], eax
        call xr_vacia
        ; ---- «%PDF-1.4», en los primeros 1.024 octetos ----
        mov esi, [pdf_dat]
        mov ebp, esi
        add ebp, [pdf_tam]
        mov eax, esi
        add eax, 1024
        cmp eax, ebp
        ja .c1
        mov ebp, eax
.c1:
        mov edi, t_pdf_cab
        mov ecx, 5
        call mem_busca
        mov dword [esp+28], PE_NOPDF
        jc .x
        lea esi, [eax+5]
        mov ebp, [pdf_dat]
        add ebp, [pdf_tam]
        lea eax, [esi+3]
        cmp eax, ebp
        ja .sin_ver
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .sin_ver
        movzx ebx, byte [esi+2]
        sub ebx, '0'
        cmp ebx, 9
        ja .sin_ver
        shl eax, 4
        or eax, ebx
        mov [pdf_ver], eax
.sin_ver:
        ; ---- el índice: desde el último «startxref» ----
        mov esi, [pdf_dat]
        mov ebp, esi
        add ebp, [pdf_tam]
        mov eax, ebp
        sub eax, 2048
        cmp eax, esi
        jbe .s1
        mov esi, eax
.s1:
        mov edi, p_startxref
        mov ecx, 9
        xor ebx, ebx                ; el último que haya
.s2:
        call mem_busca
        jc .s3
        mov ebx, eax
        lea esi, [eax+9]
        jmp .s2
.s3:
        test ebx, ebx
        jz .repara
        lea esi, [ebx+9]
        call px_blancos
        call px_entero
        jc .repara
        mov dword [xr_saltos], 0
        mov dword [xr_mixto], 0
.cadena:
        call xr_lee                 ; EAX=dónde está esta parte -> EAX=la anterior
        jc .repara
        test eax, eax
        jz .indice_ok
        inc dword [xr_saltos]
        cmp dword [xr_saltos], 200  ; (una cadena de «Prev» que da vueltas)
        jb .cadena
.indice_ok:
        cmp dword [pdf_raiz], 0
        jne .abierto
.repara:
        mov eax, [ar_base]
        mov [ar_tope], eax
        mov dword [pdf_npag], 0
        call xr_repara
        mov dword [esp+28], PE_ROTO
        jc .mira_cifrado
.abierto:
        mov dword [esp+28], PE_CIFRADO
        cmp dword [pdf_cifrado], 0
        jne .x
        call os_carga_todos         ; los objetos que van dentro de flujos
        mov dword [esp+28], PE_MEMORIA
        jc .x
        mov dword [px_prof], 0
        mov eax, [ar_tope]
        mov [ar_fijo], eax
        ; ---- las páginas ----
        mov esi, [pdf_raiz]
        mov ebp, [pdf_raiz+4]
        mov edi, k_pages
        call px_dic
        jc .sin_pag
        call px_ref
        mov edi, pg_her
        mov ecx, 7
        xor eax, eax
        rep stosd
        mov dword [pg_her+16], 612 << 16    ; si nadie dice la caja: una carta
        mov dword [pg_her+20], 792 << 16
        mov dword [pg_prof], 0
        mov dword [pg_vistos], 0
        call pg_nodo
.sin_pag:
        cmp dword [pdf_npag], 0
        jne .hay_pag
        cmp dword [pdf_reparado], 0 ; el índice estaba, pero no lleva a ninguna
        je .repara                  ; página: se rehace
        mov dword [esp+28], PE_SINPAG
        jmp .x
.hay_pag:
        mov dword [esp+28], 0
.x:
        popad
        ret
.mira_cifrado:
        cmp dword [pdf_cifrado], 0
        je .x
        mov dword [esp+28], PE_CIFRADO
        jmp .x
t_pdf_cab   db "%PDF-"

xr_vacia:
        push eax
        push ecx
        push edi
        mov edi, xr_t
        mov ecx, MAX_OBJ / 4
        xor eax, eax
        rep stosd
        pop edi
        pop ecx
        pop eax
        ret

; (interna) apunta un objeto si no se sabe ya de él (manda lo más nuevo, que
; se lee primero): EAX=número BL=clase ECX=a EDX=b
xr_apunta:
        cmp eax, MAX_OBJ
        jae .muchos
        cmp byte [xr_t+eax], 0
        je .pon
        cmp dword [xr_mixto], 0     ; (índice mixto: su flujo manda sobre los
        je .x                       ; «libres» que la tabla acaba de apuntar)
        cmp byte [xr_t+eax], 4
        jne .x
.pon:
        mov [xr_t+eax], bl
        mov [xr_a+eax*4], ecx
        mov [xr_b+eax*4], edx
.x:
        ret
.muchos:
        cmp bl, 4                   ; (uno libre de más no importa)
        je .x
        mov dword [pdf_muchos], 1
        ret

; (interna) lo que interesa de un «trailer» (ESI, EBP=su diccionario): la
; raíz, la información y si está cifrado; y el índice anterior -> [xr_prev]
xr_trailer:
        pushad
        mov [xr_dic], esi
        mov [xr_dic+4], ebp
        mov edi, k_encrypt
        call px_dic
        jc .e1
        cmp esi, ebp                ; (sin ir al objeto: puede que el índice aún
        jae .e1                     ; no se haya leído)
        cmp byte [esi], 'n'         ; («/Encrypt null» no cifra nada)
        je .e1
        mov dword [pdf_cifrado], 1
.e1:
        cmp dword [pdf_raiz], 0
        jne .r1
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_root
        call px_dic
        jc .r1
        call px_es_ref              ; se guarda el número: los objetos que van
        jc .r1                      ; en flujos aún no están en memoria
        mov [pdf_raiz+4], eax
        mov dword [pdf_raiz], 1
.r1:
        cmp dword [pdf_info_p], 0
        jne .i1
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_info
        call px_dic
        jc .i1
        call px_es_ref
        jc .i1
        mov [pdf_info_p+4], eax
        mov dword [pdf_info_p], 1
.i1:
        mov dword [xr_prev], 0
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_prev
        call px_dic
        jc .x
        call px_entero
        jc .x
        test eax, eax
        jle .x
        mov [xr_prev], eax
.x:
        popad
        ret

; (interna) como xr_trailer, pero lo que diga este manda sobre lo anterior
; (rehaciendo el índice se leen de principio a fin: el último es el bueno)
xr_trailer_ultimo:
        push eax
        push edx
        push dword [pdf_raiz]
        push dword [pdf_raiz+4]
        push dword [pdf_info_p]
        push dword [pdf_info_p+4]
        mov dword [pdf_raiz], 0
        mov dword [pdf_info_p], 0
        call xr_trailer
        pop edx
        pop eax
        cmp dword [pdf_info_p], 0
        jne .i
        mov [pdf_info_p], eax
        mov [pdf_info_p+4], edx
.i:
        pop edx
        pop eax
        cmp dword [pdf_raiz], 0
        jne .r
        mov [pdf_raiz], eax
        mov [pdf_raiz+4], edx
.r:
        pop edx
        pop eax
        ret

; (interna) lee la parte del índice que está en EAX (desde el principio del
; archivo): una tabla «xref» o un flujo /XRef -> EAX=dónde está la anterior
; (0: no hay más); CF=1 si ahí no hay un índice
xr_lee:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        inc dword [xr_saltos]       ; (índices que se llaman unos a otros sin fin)
        cmp dword [xr_saltos], 400
        ja .mal
        test eax, eax
        js .mal
        cmp eax, [pdf_tam]
        jae .mal
        mov esi, [pdf_dat]
        mov ebp, esi
        add ebp, [pdf_tam]
        add esi, eax
        call px_blancos
        mov edi, p_xref
        call px_es_palabra
        jne .flujo
        add esi, 4
        ; ---- la tabla de siempre: «primero cuántos» y una línea por objeto ----
.seccion:
        call px_blancos
        mov edi, p_trailer
        call px_es_palabra
        je .trailer
        call px_entero
        jc .mal
        mov edi, eax                ; EDI=el número del objeto
        call px_blancos
        call px_entero
        jc .mal
        mov ecx, eax                ; ECX=cuántos
        test edi, edi
        js .mal
        test ecx, ecx
        js .mal
.linea:
        test ecx, ecx
        jz .seccion
        call px_blancos
        call px_entero              ; dónde
        jc .mal
        mov edx, eax
        call px_blancos
        call px_entero              ; la generación
        jc .mal
        call px_blancos
        cmp esi, ebp
        jae .mal
        mov al, [esi]
        inc esi
        push ecx
        mov bl, 4
        cmp al, 'f'
        je .l1
        cmp al, 'n'
        jne .l_mal
        mov bl, 1
        test edx, edx               ; (un «n» en el 0 es un libre mal escrito)
        jnz .l1
        mov bl, 4
.l1:
        mov eax, edi
        mov ecx, edx
        xor edx, edx
        call xr_apunta
        pop ecx
        inc edi
        dec ecx
        jmp .linea
.l_mal:
        pop ecx
        jmp .mal
.trailer:
        add esi, 7
        call px_blancos
        call xr_trailer
        push dword [xr_prev]
        ; un índice mixto: los objetos nuevos están en un flujo aparte
        mov edi, k_xrefstm
        call px_dic
        jc .t1
        call px_entero
        jc .t1
        mov dword [xr_mixto], 1
        call xr_lee
        mov dword [xr_mixto], 0
.t1:
        pop eax
        jmp .bien
        ; ---- un flujo /XRef (PDF 1.5): campos binarios de anchos /W ----
.flujo:
        call px_entero              ; «n g obj»
        jc .mal
        call px_blancos
        call px_entero
        jc .mal
        call px_blancos
        mov edi, p_obj
        call px_es_palabra
        jne .mal
        add esi, 3
        call px_blancos
        mov [xr_dic], esi
        mov [xr_dic+4], ebp
        mov edi, k_type
        call px_dic
        jc .mal
        mov edi, k_xref_t
        call px_es_nombre
        jne .mal
        mov esi, [xr_dic]
        call xr_trailer
        push dword [xr_prev]
        call ar_marca
        push eax
        call xr_flujo
        pop eax
        call ar_suelta
        pop eax
        jc .mal
.bien:
        clc
        jmp .x
.mal:
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) las entradas de un flujo /XRef ([xr_dic]); CF=1 si está mal
xr_flujo:
        pushad
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        call pdf_flujo
        jc .mal
        mov [xr_dat], eax
        add ecx, eax
        mov [xr_dat+4], ecx         ; (el final)
        ; /W [a b c]: los octetos de cada campo
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_w
        call px_dic
        jc .mal
        call px_ref
        cmp esi, ebp
        jae .mal
        cmp byte [esi], '['
        jne .mal
        inc esi
        xor ebx, ebx
.w:
        call px_blancos
        call px_entero
        jc .mal
        cmp eax, 4
        ja .mal
        mov [xr_w+ebx*4], eax
        inc ebx
        cmp ebx, 3
        jb .w
        mov eax, [xr_w]
        add eax, [xr_w+4]
        add eax, [xr_w+8]
        jz .mal
        ; /Index [primero cuántos ...]; si no está, de 0 a /Size
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_size
        call px_dic
        jc .mal
        call px_ref
        call px_entero
        jc .mal
        mov [xr_n], eax
        mov dword [xr_ind], 0
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_index
        call px_dic
        jc .tramo0
        call px_ref
        cmp esi, ebp
        jae .tramo0
        cmp byte [esi], '['
        jne .tramo0
        inc esi
        mov [xr_ind], esi
        mov [xr_ind+4], ebp
.tramo:
        mov esi, [xr_ind]
        mov ebp, [xr_ind+4]
        call px_blancos
        call px_entero
        jc .bien                    ; (se acaba la lista)
        mov edi, eax
        call px_blancos
        call px_entero
        jc .bien
        mov ecx, eax
        mov [xr_ind], esi
        jmp .entradas
.tramo0:
        xor edi, edi
        mov ecx, [xr_n]
.entradas:
        ; EDI=número del objeto  ECX=cuántos
        test edi, edi
        js .mal
.ent:
        test ecx, ecx
        jle .ent_fin
        mov esi, [xr_dat]
        mov eax, [xr_w]
        add eax, [xr_w+4]
        add eax, [xr_w+8]
        add eax, esi
        cmp eax, [xr_dat+4]
        ja .bien                    ; (se acaban los datos)
        mov [xr_dat], eax
        push ecx
        mov ecx, [xr_w]
        mov eax, 1                  ; sin el primer campo, es de la clase 1
        test ecx, ecx
        jz .c1
        call .campo
.c1:
        mov ebx, eax
        mov ecx, [xr_w+4]
        call .campo
        mov edx, eax
        mov ecx, [xr_w+8]
        call .campo
        xchg eax, edx               ; EAX=campo 2  EDX=campo 3
        mov ecx, eax
        cmp ebx, 1
        jb .libre
        je .apunta
        cmp ebx, 2
        jne .sig                    ; (una clase que no se conoce: como si no estuviera)
        jmp .apunta
.libre:
        mov bl, 4
.apunta:
        mov eax, edi
        call xr_apunta
.sig:
        pop ecx
        inc edi
        dec ecx
        jmp .ent
.ent_fin:
        cmp dword [xr_ind], 0
        jne .tramo
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; (interna) ESI=datos ECX=octetos -> EAX=el campo (el de más peso, primero)
.campo:
        xor eax, eax
        test ecx, ecx
        jz .cx
.cb:
        shl eax, 8
        mov al, [esi]
        inc esi
        dec ecx
        jnz .cb
.cx:
        ret

; -----------------------------------------------------------------------------
;  Rehacer el índice de un archivo estropeado: se buscan los «n g obj» (el
;  último de cada número manda) y los «trailer»; si no hay ninguno con /Root,
;  se busca el objeto /Type /Catalog. CF=1 si no hay manera
; -----------------------------------------------------------------------------
xr_repara:
        pushad
        call xr_vacia
        mov dword [pdf_reparado], 1
        mov dword [pdf_raiz], 0
        mov dword [pdf_info_p], 0
        mov esi, [pdf_dat]
        mov ebp, esi
        add ebp, [pdf_tam]
.b:
        cmp esi, ebp
        jae .fin
        mov al, [esi]
        cmp al, 't'
        je .trailer
        sub al, '0'
        cmp al, 9
        ja .sig
        cmp esi, [pdf_dat]          ; un número al principio de una palabra
        je .num
        movzx eax, byte [esi-1]
        cmp byte [px_clase+eax], 1
        jne .sig
.num:
        mov ebx, esi
        call px_entero
        jc .sig1
        mov edx, eax
        call px_blancos
        cmp esi, ebp
        jae .no_obj
        mov al, [esi]
        sub al, '0'
        cmp al, 9
        ja .no_obj
        call px_entero
        jc .no_obj
        call px_blancos
        mov edi, p_obj
        call px_es_palabra
        jne .no_obj
        cmp edx, MAX_OBJ
        jae .no_obj
        mov byte [xr_t+edx], 1      ; (el último que se vea)
        mov eax, ebx
        sub eax, [pdf_dat]
        mov [xr_a+edx*4], eax
        add esi, 3
        jmp .b
.no_obj:
        lea esi, [ebx+1]            ; no lo era: se sigue por la cifra siguiente
        jmp .b
.trailer:
        mov edi, p_trailer
        call px_es_palabra
        jne .sig
        add esi, 7
        push esi
        call px_blancos
        call xr_trailer_ultimo
        pop esi
        jmp .b
.sig1:
        mov esi, ebx
.sig:
        inc esi
        jmp .b
.fin:
        ; los objetos que son flujos de objetos, índices en flujo o el catálogo
        mov dword [os_todos], 1
        xor ebx, ebx
.o:
        cmp byte [xr_t+ebx], 1
        jne .o_sig
        mov eax, ebx
        call px_objeto
        jc .o_sig
        mov [xr_dic], esi
        mov [xr_dic+4], ebp
        mov edi, k_type
        call px_dic
        jc .o_sig
        mov edi, k_catalog
        call px_es_nombre
        jne .o1
        cmp dword [pdf_raiz], 0
        jne .o_sig
        mov dword [pdf_raiz], 2     ; (2: hallado a mano)
        mov [pdf_raiz+4], ebx
        jmp .o_sig
.o1:
        mov edi, k_xref_t
        call px_es_nombre
        jne .o2
        mov esi, [xr_dic]
        call xr_trailer_ultimo
        jmp .o_sig
.o2:
        mov edi, k_objstm
        call px_es_nombre
        jne .o_sig
        mov eax, ebx
        call os_carga               ; (apunta todo lo que lleve y no se conozca)
.o_sig:
        inc ebx
        cmp ebx, MAX_OBJ
        jb .o
        mov dword [os_todos], 0
        cmp dword [pdf_raiz], 0
        jne .bien
        ; sin «trailer»: el catálogo puede estar dentro de un flujo de objetos
        xor ebx, ebx
.c:
        cmp byte [xr_t+ebx], 3
        jne .c_sig
        mov eax, ebx
        call px_objeto
        jc .c_sig
        mov edi, k_type
        call px_dic
        jc .c_sig
        mov edi, k_catalog
        call px_es_nombre
        jne .c_sig
        mov dword [pdf_raiz], 2
        mov [pdf_raiz+4], ebx
        jmp .bien
.c_sig:
        inc ebx
        cmp ebx, MAX_OBJ
        jb .c
        popad
        stc
        ret
.bien:
        popad
        clc
        ret

; -----------------------------------------------------------------------------
;  Los flujos de objetos (PDF 1.5): muchos objetos pequeños comprimidos juntos.
;  Se descomprimen todos al abrir y se quedan en el almacén: así un objeto es
;  siempre un puntero que no se mueve. CF=1 si no caben
; -----------------------------------------------------------------------------
os_carga_todos:
        pushad
        xor ebx, ebx
.b:
        cmp byte [xr_t+ebx], 2
        jne .sig
        mov eax, [xr_a+ebx*4]
        call os_carga
        jc .mal
        cmp byte [xr_t+ebx], 2      ; el flujo no lo traía: no existe
        jne .sig
        mov byte [xr_t+ebx], 0
.sig:
        inc ebx
        cmp ebx, MAX_OBJ
        jb .b
        ; la raíz y la información, ahora que todo está en memoria
        cmp dword [pdf_raiz], 0
        je .mal_raiz
        mov eax, [pdf_raiz+4]
        call px_objeto
        jc .mal_raiz
        mov [pdf_raiz], esi
        mov [pdf_raiz+4], ebp
        cmp dword [pdf_info_p], 0
        je .bien
        mov eax, [pdf_info_p+4]
        mov dword [pdf_info_p], 0
        call px_objeto
        jc .bien
        mov [pdf_info_p], esi
        mov [pdf_info_p+4], ebp
.bien:
        popad
        clc
        ret
.mal_raiz:
        mov dword [pdf_raiz], px_nulo   ; (sin catálogo: no saldrán páginas)
        mov dword [pdf_raiz+4], px_nulo + 4
        jmp .bien
.mal:
        popad
        stc
        ret

; (interna) descomprime el flujo de objetos EAX y apunta los objetos que
; lleva: los que el índice dice que están en él o, rehaciendo el índice
; ([os_todos]), los que no se conozcan. CF=1 solo si no cabe en el almacén
os_carga:
        pushad
        mov [os_num], eax
        cmp eax, MAX_OBJ
        jae .nada
        cmp byte [xr_t+eax], 1      ; un flujo está siempre en el archivo
        jne .nada
        call px_objeto
        jc .nada
        mov [xr_dic], esi
        mov [xr_dic+4], ebp
        mov edi, k_n
        call px_dic
        jc .nada
        call px_ref
        call px_entero
        jc .nada
        test eax, eax
        jle .nada
        mov [os_n], eax
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        mov edi, k_first
        call px_dic
        jc .nada
        call px_ref
        call px_entero
        jc .nada
        test eax, eax
        js .nada
        mov [os_first], eax
        mov esi, [xr_dic]
        mov ebp, [xr_dic+4]
        call ar_marca
        mov edx, eax
        call pdf_flujo
        jnc .hay
        mov eax, edx
        call ar_suelta
        cmp dword [fl_error], FE_MEMORIA
        je .mal
        jmp .nada
.hay:
        ; (si no llevaba filtros, los datos siguen en el archivo: ahí se quedan)
        mov [os_dat], eax
        add ecx, eax
        mov [os_dat+4], ecx         ; el final
        mov edx, [os_first]
        add edx, eax
        cmp edx, ecx
        ja .nada
        ; la cabecera: N parejas «número dónde»
        mov esi, eax
        mov ebp, edx                ; (la cabecera acaba donde empieza el primero)
        mov dword [os_ant], 0       ; el objeto anterior: hasta saber dónde acaba
        xor edi, edi                ; EDI=cuál
.par:
        cmp edi, [os_n]
        jae .ultimo
        call px_blancos
        call px_entero
        jc .ultimo
        mov ebx, eax                ; EBX=número
        call px_blancos
        call px_entero
        jc .ultimo
        test eax, eax
        js .ultimo
        add eax, [os_first]
        add eax, [os_dat]
        jc .ultimo
        cmp eax, [os_dat+4]
        ja .ultimo
        call .cierra                ; el anterior acaba donde empieza este
        cmp ebx, MAX_OBJ
        jae .par_sig
        mov cl, [xr_t+ebx]
        cmp dword [os_todos], 0
        je .p1
        test cl, cl                 ; rehaciendo: los que no se conozcan
        jz .mio
        jmp .par_sig
.p1:
        cmp cl, 2
        jne .par_sig
        mov ecx, [os_num]
        cmp [xr_a+ebx*4], ecx
        jne .par_sig
.mio:
        mov byte [xr_t+ebx], 3
        mov [xr_a+ebx*4], eax
        mov ecx, [os_dat+4]
        mov [xr_b+ebx*4], ecx
        mov [os_ant], ebx
        or dword [os_ant], 0x80000000
.par_sig:
        inc edi
        jmp .par
.ultimo:
.nada:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
; (interna) el objeto apuntado antes acaba en EAX (si es que empieza antes)
.cierra:
        push ecx
        mov ecx, [os_ant]
        test ecx, ecx
        jns .cx
        and ecx, 0x7FFFFFFF
        cmp eax, [xr_a+ecx*4]
        jbe .cx0
        mov [xr_b+ecx*4], eax
.cx0:
        mov dword [os_ant], 0
.cx:
        pop ecx
        ret

; -----------------------------------------------------------------------------
;  El árbol de páginas. ESI, EBP=un nodo; lo heredado, en pg_her
; -----------------------------------------------------------------------------
pg_nodo:
        pushad
        inc dword [pg_prof]
        cmp dword [pg_prof], 40     ; un árbol que no se acaba
        ja .x0
        inc dword [pg_vistos]
        cmp dword [pg_vistos], 4 * MAX_PAG
        ja .x0
        call px_blancos
        lea eax, [esi+2]
        cmp eax, ebp
        ja .x0
        cmp word [esi], '<<'
        jne .x0
        mov ecx, [pg_prof]          ; un nodo que es su propio abuelo: no se entra
        dec ecx                     ; (un árbol que se muerde la cola)
        mov [pg_pila+ecx*4], esi
.cola:
        dec ecx
        js .nuevo
        cmp [pg_pila+ecx*4], esi
        je .x0
        jmp .cola
.nuevo:
        mov ecx, 7                  ; lo heredado se guarda: vale para este nodo
.g:
        push dword [pg_her+ecx*4-4] ; y los de debajo
        dec ecx
        jnz .g
        push esi
        push ebp
        mov edi, k_resources
        call px_dic
        jc .r1
        call px_ref
        mov [pg_her], esi
        mov [pg_her+4], ebp
.r1:
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_mediabox
        call pg_caja
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_cropbox
        call pg_caja
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_rotate
        call px_dic
        jc .g1
        call px_ref
        call px_entero
        jc .g1
        mov [pg_her+24], eax
.g1:
        mov ebp, [esp]
        mov esi, [esp+4]
        mov edi, k_kids
        call px_dic
        jc .hoja
        call px_ref
        cmp esi, ebp
        jae .fin
        cmp byte [esi], '['
        jne .fin
        inc esi
.hijo:
        call px_blancos
        cmp esi, ebp
        jae .fin
        cmp byte [esi], ']'
        je .fin
        push esi
        push ebp
        call px_ref
        call pg_nodo
        pop ebp
        pop esi
        call px_salta_valor
        jmp .hijo
.hoja:
        mov eax, [pdf_npag]
        cmp eax, MAX_PAG
        jae .llena
        imul eax, PG_TAM
        lea edi, [pg_tab+eax]
        mov eax, [esp+4]
        stosd
        mov eax, [esp]
        stosd
        mov esi, pg_her
        mov ecx, 7
        rep movsd
        inc dword [pdf_npag]
.fin:
        add esp, 8
        mov ecx, 7
        xor ebx, ebx
        jmp .d
.llena:
        mov dword [pdf_muchos], 1
        jmp .fin
.d:
        pop dword [pg_her+ebx*4]
        inc ebx
        dec ecx
        jnz .d
.x0:
        dec dword [pg_prof]
        popad
        ret

; (interna) si el nodo tiene la caja EDI («[x0 y0 x1 y1]»), a lo heredado
pg_caja:
        pushad
        call px_dic
        jc .x
        call px_ref
        cmp esi, ebp
        jae .x
        cmp byte [esi], '['
        jne .x
        inc esi
        sub esp, 16
        xor ebx, ebx
.n:
        call px_blancos
        push esi
        push ebp
        call px_ref                 ; (cada número puede ser una referencia)
        call px_num
        pop ebp
        pop esi
        jc .mal
        mov [esp+ebx*4], eax
        call px_salta_valor
        inc ebx
        cmp ebx, 4
        jb .n
        mov eax, [esp]              ; en orden: x0 < x1, y0 < y1
        mov ecx, [esp+8]
        cmp eax, ecx
        jle .o1
        xchg eax, ecx
.o1:
        mov ebx, [esp+4]
        mov edx, [esp+12]
        cmp ebx, edx
        jle .o2
        xchg ebx, edx
.o2:
        mov esi, ecx                ; una caja sin ancho o sin alto no vale
        sub esi, eax
        cmp esi, 0x10000
        jl .mal
        mov esi, edx
        sub esi, ebx
        cmp esi, 0x10000
        jl .mal
        mov [pg_her+8], eax
        mov [pg_her+12], ebx
        mov [pg_her+16], ecx
        mov [pg_her+20], edx
.mal:
        add esp, 16
.x:
        popad
        ret
