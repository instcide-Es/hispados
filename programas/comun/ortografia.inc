; =============================================================================
;  ortografia.inc - el corrector ortográfico (español) que comparten los
;  programas de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El diccionario está en DICCIO.DAT, al lado de los módulos (\HISPADOS\PROGS).
;  Lo hace tools/hazdiccio.py, que explica el archivo; de dónde sale y su
;  licencia (que no es la de este código), en docs/ORTOGRAFIA.md.
;
;  DICCIO.DAT es un autómata: todas las formas del español (las conjugaciones,
;  los plurales, los verbos con sus pronombres pegados...) ya desplegadas y
;  guardadas compartiendo los principios y los finales. Son «arcos» de 4
;  octetos: la letra (bits 0-7), «aquí se acaba una palabra» (bit 8), «último
;  arco de este nodo» (bit 9) y el primer arco del nodo al que lleva (bits
;  10-31; 0, a ninguno). Mirar una palabra es seguirla letra a letra desde el
;  arco 0: no se descomprime nada y la memoria que hace falta es el archivo.
;
;  Las letras, en la página de códigos 437. Como ahí no hay Á, Í, Ó ni Ú:
;    - tal cual: «árbol», «Madrid», «ADSL»;
;    - con la primera en mayúscula: «Casa»; y «Arbol» vale por «árbol»;
;    - toda en mayúsculas: «CASA», «MADRID»; y «ARBOL», «CAMION» o «MALAGA»
;      valen por «árbol», «camión» y «Málaga» (A, I, O y U valen también por
;      su acentuada; la É y la Ü sí existen, y por eso se piden: «CAFÉ»);
;    - los nombres propios, solo con su mayúscula: «madrid» no vale.
;  Lo que tiene cifras, lo de una letra y lo de más de 63 se da por bueno (no
;  se juzga). Si no hay diccionario, todo se da por bueno.
;
;  Se incluye en el código del programa y, en su «section .bss», se pone la
;  macro ORTO_BSS (unos 520 K: lo que puede ocupar el diccionario). ort_cargar
;  usa la API de Hispa Ventanas (HV dos_abrir, dos_leer y dos_cerrar); lo
;  demás no llama a nada de fuera: trabaja con lo que hay en memoria. Lo más
;  hondo que llegan a usar la pila, unos 3 K (lo normal, menos de 1 K).
;
;    ort_cargar   ESI=ruta del módulo (la que recibe en su entrada): lee
;                 DICCIO.DAT de su carpeta y lo comprueba entero. Deja
;                 [ort_hay] = 1 si hay diccionario, y vacía la lista de
;                 palabras del usuario. Conserva los registros
;    ort_palabra  ESI=palabra ECX=su largo -> CF=0 si está bien (o no se
;                 juzga), CF=1 si no se conoce. Conserva los registros
;    ort_sugerir  ESI=palabra ECX=su largo EDI=dónde poner las propuestas
;                 EDX=cuántas a lo más (hasta ORT_SUG, 8) -> EAX=cuántas ha
;                 puesto: cada una en 64 octetos y acabada en 0, la más
;                 probable primero, y con las mayúsculas de la que se dio
;                 (puede escribir en los EDX x 64 octetos aunque ponga menos).
;                 Conserva los demás registros. Propone las palabras del
;                 diccionario a UNA corrección: una letra cambiada, de más,
;                 de menos, o dos vecinas trocadas; o «ll» por «y», «k» por
;                 «qu» y «x» por «cc» (y al revés). Los acentos no cuentan
;                 como corrección: pueden faltar o sobrar además («tanbien»
;                 -> «también»). Delante van las faltas de siempre: el acento,
;                 b/v, g/j, la h, c/z/s, ll/y, r/rr, m/n, q/k
;    ort_propia   ESI=palabra ECX=su largo (1..63, sin espacios): la apunta en
;                 la lista del usuario y ort_palabra la da por buena (también
;                 con la primera en mayúscula o toda en mayúsculas, si se
;                 apuntó en minúsculas). CF=1 si no cabe o no vale
;    ort_propias_ptr    -> ESI=la lista del usuario, ECX=sus octetos: es un
;                 texto, una palabra por línea (CR LF), listo para guardarlo
;    ort_propias_poner  ESI=un texto ECX=sus octetos: la lista pasa a ser las
;                 palabras de ese texto (separadas por espacios o saltos de
;                 línea; lo que no vale o no cabe se deja). Con ECX=0, la vacía
;    ort_comprobar      (la usa ort_cargar) EAX=octetos que hay en ort_dic:
;                 pone [ort_hay] si son un diccionario entero y sano
; =============================================================================
%ifndef ORT_MAX
ORT_MAX     equ 512 * 1024          ; lo más que puede ocupar DICCIO.DAT (hoy,
%endif                              ; 454 K; quien quiera otro tope lo define antes)
ORT_CAB     equ 32                  ; su cabecera
ORT_LARGO   equ 63                  ; la palabra más larga que se mira
ORT_SUG     equ 8                   ; las propuestas, a lo más
ORT_PROPIAS equ 4096                ; lo que ocupa la lista del usuario
ORT_TOPE    equ 400000              ; pasos de una búsqueda, a lo más (con un
                                    ; diccionario sano no se llega ni de lejos)

%macro ORTO_BSS 0
            alignb 4
ort_hay     resd 1                  ; 1: hay diccionario
ort_largo   resd 1                  ; su palabra más larga
ort_n       resd 1                  ; la palabra que se mira: su largo,
ort_modo    resd 1                  ; 0 tal cual, 1 Primera mayúscula, 2 TODA
ort_tope    resd 1
ort_ent_n   resd 1
ort_s_n     resd 1                  ; las propuestas que hay
ort_propias_n resd 1
ort_ruta_fin resd 1
ort_alt     resb 64 * 4             ; por cada letra, las que valen en su sitio
ort_ent     resb 68                 ; la palabra en minúsculas, desde ort_ent+1
ort_sal     resb 68                 ; la candidata que se va formando
ort_s_pts   resb ORT_SUG            ; la nota de cada propuesta (menos, mejor)
ort_s_pal   resb ORT_SUG * 64
ort_ruta    resb 128
ort_propias resb ORT_PROPIAS
            alignb 4
ort_dic     resb ORT_MAX
%endmacro

; ---- las letras (página 437) ------------------------------------------------
; una tabla de 256: cada código vale él mismo salvo los pares «código, valor»
; que se dan; el primer dato: 1 = además A-Z pasan a a-z, 2 = a-z a A-Z
%macro ORT_TABLA 1-*
%assign %%letras %1
%rotate 1
%assign %%i 0
%rep 256
  %assign %%v %%i
  %if %%letras = 1 && %%i >= 'A' && %%i <= 'Z'
    %assign %%v %%i + 32
  %elif %%letras = 2 && %%i >= 'a' && %%i <= 'z'
    %assign %%v %%i - 32
  %endif
  %rep (%0 - 1) / 2
    %if %1 = %%i
      %assign %%v %2
    %endif
    %rotate 2
  %endrep
  %rotate 1                         ; (el primer dato, que también da la vuelta)
        db %%v
  %assign %%i %%i + 1
%endrep
%endmacro

; a minúscula:  Ñ É Ü Ç Ä Å Æ Ö
ort_min:    ORT_TABLA 1, 165,164, 144,130, 154,129, 128,135, 142,132, 143,134, 146,145, 153,148
; a mayúscula: ñ é ü, y á í ó ú a la letra sin acento (no hay otra); ç ä å æ ö
ort_may:    ORT_TABLA 2, 164,165, 130,144, 129,154, 160,'A', 161,'I', 162,'O', 163,'U', 135,128, 132,142, 134,143, 145,146, 148,153
; sin acento: á é í ó ú ü
ort_base:   ORT_TABLA 0, 160,'a', 130,'e', 161,'i', 162,'o', 163,'u', 129,'u'

; las letras que se confunden al escribir (en cualquier orden): b/v g/j c/z
; c/s s/z s/x m/n q/k c/k c/q y/i n/ñ
ort_pares   db "bv", "gj", "cz", "cs", "sz", "sx", "mn", "qk", "ck", "cq", "yi", "n", 164, 0, 0
; grupos que se confunden: lo tecleado (dos letras; la segunda, 0 si es una) y
; lo que se prueba en su lugar (igual)
ort_grupos  db "ll", "y", 0,  "y", 0, "ll",  "k", 0, "qu",  "qu", "k", 0,  "x", 0, "cc",  "cc", "x", 0,  0
ort_archivo db "DICCIO.DAT", 0

; ---- leer el diccionario ----------------------------------------------------
ort_cargar:
        pushad
        mov dword [ort_hay], 0
        mov dword [ort_propias_n], 0
        mov dword [ort_s_n], 0
        mov edi, ort_ruta           ; la carpeta del módulo
        mov ebx, edi
        mov ecx, 110
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c1
        mov ebx, edi
.c1:
        test al, al
        jz .c2
        dec ecx
        jnz .c
.c2:
        mov [ort_ruta_fin], ebx
        mov edi, ebx
        mov esi, ort_archivo
.n:
        lodsb
        stosb
        test al, al
        jnz .n
        mov esi, ort_ruta           ; se lee a trozos (de una vez, DOS no pasa
        xor al, al                  ; de 64 K)
        HV dos_abrir
        jc .x
        mov ebx, eax
        mov edi, ort_dic
.lee:
        mov ecx, ort_dic + ORT_MAX
        sub ecx, edi
        jz .leido
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .leido
        test eax, eax
        jz .leido
        cmp eax, ecx                ; (por si acaso: nunca más de lo pedido)
        ja .leido
        add edi, eax
        jmp .lee
.leido:
        HV dos_cerrar
        mov eax, edi
        sub eax, ort_dic            ; EAX=lo leído
        call ort_comprobar
.x:
        popad
        ret

; EAX=octetos que hay en ort_dic. Se mira TODO antes de fiarse: la cabecera,
; que el archivo esté entero (ni cortado ni con algo detrás), que cada arco
; lleve a un arco que existe, que el último cierre su nodo (así recorrer un
; nodo nunca se sale) y la suma de todo. Después ya no hace falta comprobar
; nada al consultar.
ort_comprobar:
        pushad
        mov dword [ort_hay], 0
        cmp eax, ORT_CAB + 4
        jb .x
        cmp eax, ORT_MAX
        ja .x
        cmp dword [ort_dic], 'DICC'
        jne .x
        cmp dword [ort_dic+4], 'IO1' | (26 << 24)
        jne .x
        mov ecx, [ort_dic+8]        ; cuántos arcos
        test ecx, ecx
        jz .x
        cmp ecx, (ORT_MAX - ORT_CAB) / 4
        ja .x
        lea edx, [ecx*4+ORT_CAB]
        cmp edx, eax
        jne .x
        movzx edx, byte [ort_dic+20]        ; la palabra más larga
        test edx, edx
        jz .x
        cmp edx, ORT_LARGO
        ja .x
        mov [ort_largo], edx
        mov esi, ort_dic + ORT_CAB
        mov edi, ecx
        xor ebx, ebx
.a:
        mov eax, [esi]
        add ebx, eax
        test al, al                 ; (una letra 0 no puede ser)
        jz .x
        shr eax, 10
        cmp eax, edi
        jae .x
        add esi, 4
        dec ecx
        jnz .a
        test byte [esi-3], 2
        jz .x
        cmp ebx, [ort_dic+16]
        jne .x
        mov dword [ort_hay], 1
.x:
        popad
        ret

; ---- ¿está bien escrita? ----------------------------------------------------
ort_palabra:
        pushad
        cmp dword [ort_hay], 0
        je .bien
        cmp ecx, 2
        jb .bien
        cmp ecx, ORT_LARGO
        ja .bien
        call ort_prepara
        jc .bien                    ; (con cifras)
        call ort_en_propias
        jnc .bien
        cmp ecx, [ort_largo]        ; más larga que la más larga: no está
        ja .mal
        mov dword [ort_tope], ORT_TOPE
        mov ebx, ort_dic + ORT_CAB
        xor edx, edx
        call ort_casa
        jnc .bien
.mal:
        popad
        stc
        ret
.bien:
        popad
        clc
        ret

; (interna) ESI=palabra ECX=largo (1..63): apunta su largo, cómo está escrita
; ([ort_modo]) y, en ort_alt, las letras que valen en cada sitio (hasta tres;
; 0, ninguna): la tecleada y, donde una mayúscula puede ser por la norma y no
; por la palabra, su minúscula y su minúscula con acento. CF=1 si hay cifras
ort_prepara:
        pushad
        mov [ort_n], ecx
        xor ebx, ebx                ; mayúsculas
        xor ebp, ebp                ; minúsculas
        xor edx, edx
        xor edi, edi                ; 1: hay cifras
.l:
        movzx eax, byte [esi+edx]
        mov [ort_alt+edx*4], eax
        cmp al, '0'
        jb .l1
        cmp al, '9'
        ja .l1
        mov edi, 1
.l1:
        cmp al, [ort_min+eax]
        je .l2
        inc ebx
.l2:
        cmp al, [ort_may+eax]
        je .l3
        inc ebp
.l3:
        inc edx
        cmp edx, ecx
        jb .l
        mov dword [ort_modo], 0
        cmp ebx, ecx
        jne .p
        mov dword [ort_modo], 2     ; TODA en mayúsculas: se abre cada letra
        xor edx, edx
.t:
        call ort_abre
        inc edx
        cmp edx, ecx
        jb .t
        jmp short .x
.p:
        cmp ebx, 1                  ; una mayúscula, la primera, y lo demás en
        jne .x                      ; minúsculas
        lea eax, [ebp+1]
        cmp eax, ecx
        jne .x
        movzx eax, byte [esi]
        cmp al, [ort_min+eax]
        je .x
        mov dword [ort_modo], 1
        xor edx, edx
        call ort_abre
.x:
        shr edi, 1                  ; CF=hay cifras
        popad
        ret

; (interna) la letra EDX de la palabra es una mayúscula: vale también por su
; minúscula y, si es A, I, O o U, por la acentuada (que en mayúscula no existe)
ort_abre:
        push eax
        push ebx
        movzx eax, byte [ort_alt+edx*4]
        mov bl, [ort_min+eax]
        mov [ort_alt+edx*4+1], bl
        xor bl, bl
        cmp al, 'A'
        jne .i
        mov bl, 160
.i:
        cmp al, 'I'
        jne .o
        mov bl, 161
.o:
        cmp al, 'O'
        jne .u
        mov bl, 162
.u:
        cmp al, 'U'
        jne .p
        mov bl, 163
.p:
        mov [ort_alt+edx*4+2], bl
        pop ebx
        pop eax
        ret

; (interna) EBX=nodo (la dirección de su primer arco) AL=letra -> CF=0 y
; EAX=el arco de esa letra; CF=1 si no lo hay. Los arcos van por orden
ort_arco:
        push ebx
.l:
        cmp [ebx], al
        je .si
        ja .no
        test byte [ebx+1], 2
        jnz .no
        add ebx, 4
        jmp .l
.si:
        mov eax, [ebx]
        pop ebx
        clc
        ret
.no:
        pop ebx
        stc
        ret

; (interna) EBX=nodo (0, ninguno) EDX=por qué letra se va -> CF=0 si desde ahí
; se llega justo al final de la palabra por alguna de las letras que valen
ort_casa:
        push eax
        push ecx
        push esi
        test ebx, ebx
        jz .no
        dec dword [ort_tope]
        js .no
        lea esi, [ort_alt+edx*4]
        mov ecx, 3
.alt:
        mov al, [esi]
        inc esi
        test al, al
        jz .sig
        call ort_arco
        jc .sig
        push edx
        inc edx
        cmp edx, [ort_n]
        jne .baja
        pop edx
        test ah, 1                  ; la última letra: ¿se acaba ahí una palabra?
        jnz .si
        jmp short .sig
.baja:
        push ebx
        mov ebx, eax
        shr ebx, 10
        jz .b1
        lea ebx, [ort_dic+ORT_CAB+ebx*4]
.b1:
        call ort_casa               ; (sin nodo, contesta que no)
        pop ebx
        pop edx
        jnc .si
.sig:
        dec ecx
        jnz .alt
.no:
        stc
        jmp short .x
.si:
        clc
.x:
        pop esi
        pop ecx
        pop eax
        ret

; ---- las palabras del usuario -------------------------------------------------
; (interna) ¿está la palabra preparada (ort_alt, ort_n) en la lista? CF=0 si sí
ort_en_propias:
        pushad
        mov esi, ort_propias
        mov ebp, esi
        add ebp, [ort_propias_n]
.pal:
        cmp esi, ebp
        jae .no
        xor edx, edx
.c:
        mov al, [esi+edx]
        cmp al, 13
        je .fin
        cmp edx, [ort_n]
        jae .otra
        cmp al, [ort_alt+edx*4]
        je .vale
        cmp al, [ort_alt+edx*4+1]
        je .vale
        cmp al, [ort_alt+edx*4+2]
        jne .otra
.vale:
        inc edx
        jmp .c
.fin:
        cmp edx, [ort_n]
        je .si
.otra:
        cmp esi, ebp                ; a la línea siguiente
        jae .no
        lodsb
        cmp al, 10
        jne .otra
        jmp .pal
.no:
        popad
        stc
        ret
.si:
        popad
        clc
        ret

ort_propia:
        pushad
        cmp ecx, 1
        jb .mal
        cmp ecx, ORT_LARGO
        ja .mal
        xor edx, edx
.v:
        cmp byte [esi+edx], 32      ; ni espacios ni saltos: es UNA palabra
        jbe .mal
        inc edx
        cmp edx, ecx
        jb .v
        call ort_prepara
        call ort_en_propias         ; (si ya vale, no se repite)
        jnc .bien
        mov eax, [ort_propias_n]
        lea edx, [eax+ecx+2]
        cmp edx, ORT_PROPIAS
        ja .mal
        lea edi, [ort_propias+eax]
        rep movsb
        mov word [edi], 0x0A0D
        mov [ort_propias_n], edx
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

ort_propias_ptr:
        mov esi, ort_propias
        mov ecx, [ort_propias_n]
        ret

ort_propias_poner:
        pushad
        mov dword [ort_propias_n], 0
        mov ebp, esi
        add ebp, ecx                ; EBP=el final del texto
.salta:
        cmp esi, ebp
        jae .x
        cmp byte [esi], 32
        ja .pal
        inc esi
        jmp .salta
.pal:
        mov edi, esi
.p1:
        inc esi
        cmp esi, ebp
        jae .p2
        cmp byte [esi], 32
        ja .p1
.p2:
        mov ecx, esi
        sub ecx, edi
        push esi
        mov esi, edi
        call ort_propia             ; (si es larga o no cabe, se deja)
        pop esi
        jmp .salta
.x:
        popad
        ret

; ---- las propuestas ---------------------------------------------------------
; Se recorre el autómata al lado de la palabra tecleada, dejando hacer UNA
; corrección por el camino. Así solo se andan los trozos del diccionario que
; se parecen a la palabra (no hay que fabricar candidatas y mirarlas una a
; una) y se sabe qué corrección ha hecho falta, que es lo que da la nota:
;     +1  por cada acento que falta o sobra (no gasta la corrección), y por
;         la mayúscula de un nombre propio
;     +2  una falta de siempre: letras que se confunden (ort_pares, ort_grupos),
;         h de más o de menos, letra doblada o sin doblar (r/rr), la c de «xc»
;         y «sc», la u de «gu» y «qu»
;     +4  dos letras vecinas trocadas
;     +5  una letra de más o de menos        (+1 si es la primera letra: en
;     +6  una letra por otra                  la primera se falla menos)
ort_sugerir:
        pushad
        mov dword [ort_s_n], 0
        mov dword [esp+28], 0       ; (el EAX que se devuelve)
        cmp dword [ort_hay], 0
        je .x
        cmp ecx, 2
        jb .x
        cmp ecx, ORT_LARGO
        ja .x
        mov eax, [ort_largo]        ; a una corrección de una palabra del
        inc eax                     ; diccionario no puede ser más larga
        cmp ecx, eax
        ja .x
        test edx, edx
        jz .x
        call ort_prepara
        jc .x
        xor ebx, ebx                ; la palabra, en minúsculas, a partir de
.m:                                 ; ort_ent+1 (el porqué, en ort_s_rec)
        movzx eax, byte [esi+ebx]
        mov al, [ort_min+eax]
        mov [ort_ent+ebx+1], al
        inc ebx
        cmp ebx, ecx
        jb .m
        inc ecx
        mov [ort_ent_n], ecx
        mov dword [ort_tope], ORT_TOPE
        mov ebx, ort_dic + ORT_CAB
        mov edx, 1
        xor ecx, ecx
        xor ebp, ebp
        call ort_s_rec
        ; se copian, con las mayúsculas de la tecleada
        mov ebp, [esp+20]           ; cuántas se piden
        cmp ebp, ORT_SUG
        jbe .c0
        mov ebp, ORT_SUG
.c0:
        xor ebx, ebx                ; EBX=la que se copia
        xor edx, edx                ; EDX=las puestas
.copia:
        cmp ebx, [ort_s_n]
        jae .hecho
        cmp edx, ebp
        jae .hecho
        mov esi, ebx
        shl esi, 6
        add esi, ort_s_pal
        mov edi, edx
        shl edi, 6
        add edi, [esp]              ; (el EDI que se dio)
        push edi
        xor ecx, ecx                ; ECX=letras copiadas
.cl:
        movzx eax, byte [esi+ecx]
        test al, al
        jz .cf
        cmp dword [ort_modo], 2
        je .cm
        cmp dword [ort_modo], 1
        jne .cp
        test ecx, ecx
        jnz .cp
.cm:
        mov al, [ort_may+eax]
.cp:
        mov [edi+ecx], al
        inc ecx
        jmp .cl
.cf:
        mov byte [edi+ecx], 0
        pop edi
        ; que no sea la tecleada, ni una que ya está puesta (al pasar a
        ; mayúsculas se pierden acentos y dos distintas pueden quedar iguales)
        cmp ecx, [esp+24]
        jne .c2
        mov esi, [esp+4]
        push edi
        repe cmpsb
        pop edi
        je .sig
.c2:
        mov eax, [esp]              ; EAX=cada una de las puestas
        xor ecx, ecx
.c3:
        cmp ecx, edx
        jae .vale
        push ecx
        push edi
        mov esi, eax
.c4:
        mov cl, [esi]
        cmp cl, [edi]
        jne .c5
        inc esi
        inc edi
        test cl, cl
        jnz .c4
.c5:
        pop edi
        pop ecx
        je .sig
        add eax, 64
        inc ecx
        jmp .c3
.vale:
        inc edx
.sig:
        inc ebx
        jmp .copia
.hecho:
        mov [esp+28], edx
.x:
        popad
        ret

; (interna) EAX=arco -> EBX=el nodo al que lleva (0, ninguno) y, en el bit 9
; de EBP, si con él se acaba una palabra
ort_s_hijo:
        and ebp, ~0x200
        test ah, 1
        jz .n
        or ebp, 0x200
.n:
        mov ebx, eax
        shr ebx, 10
        jz .x
        lea ebx, [ort_dic+ORT_CAB+ebx*4]
.x:
        ret

; (interna) AL, AH = dos letras -> CF=0 si son de las que se confunden
ort_s_par:
        push esi
        mov esi, ort_pares
.l:
        cmp byte [esi], 0
        je .no
        cmp ax, [esi]
        je .si
        xchg al, ah
        cmp ax, [esi]
        xchg al, ah
        je .si
        add esi, 2
        jmp .l
.no:
        pop esi
        stc
        ret
.si:
        pop esi
        clc
        ret

; (interna) el paso de la búsqueda: EBX=nodo del diccionario (0, ninguno)
; EDX=por qué letra de la tecleada se va (la primera es la 1; [ort_ent_n], la
; de detrás de la última)  ECX=letras de la candidata (ort_sal)
; EBP=la nota hasta aquí (octeto bajo), bit 8 = la corrección ya está gastada,
; bit 9 = la candidata, tal como está, es una palabra.
; Cada paso gasta una letra de la tecleada o la corrección: no baja más de
; (largo + 2) niveles, 36 octetos de pila cada uno.
ort_s_rec:
        dec dword [ort_tope]
        js .fuera
        pushad                      ; [esp+8]=EBP [esp+16]=EBX [esp+20]=EDX [esp+24]=ECX
        cmp edx, [ort_ent_n]
        jne .arcos
        test ebp, 0x200
        jz .arcos
        call ort_s_apunta           ; se acabaron las dos a la vez: es candidata
.arcos:
        test ebx, ebx
        jz .sin
        mov esi, ebx
.arco:
        mov ebp, [esp+8]
        mov eax, [esi]
        mov [ort_sal+ecx], al
        call ort_s_hijo
        cmp edx, [ort_ent_n]
        jae .pon
        mov al, [esi]               ; AL=la del diccionario
        mov ah, [ort_ent+edx]       ; AH=la tecleada
        cmp al, ah
        je .sigue
        movzx edi, al               ; ¿la misma con otro acento?
        mov al, [ort_base+edi]
        movzx edi, ah
        cmp al, [ort_base+edi]
        je .acento
        test ecx, ecx               ; ¿la mayúscula de un nombre propio?
        jnz .cambia
        movzx edi, byte [esi]
        movzx edi, byte [ort_min+edi]
        mov al, [ort_base+edi]
        movzx edi, ah
        cmp al, [ort_base+edi]
        je .acento
.cambia:
        test ebp, 0x100             ; una letra por otra
        jnz .otro
        mov al, [esi]
        call ort_s_par
        jnc .c2
        add ebp, 6
        cmp edx, 1
        jne .c3
        inc ebp
        jmp short .c3
.c2:
        add ebp, 2
.c3:
        or ebp, 0x100
        jmp short .sigue
.acento:
        inc ebp
.sigue:
        inc edx
        inc ecx
        call ort_s_rec
        dec ecx
        dec edx
.pon:
        mov ebp, [esp+8]            ; falta una letra: la del diccionario
        test ebp, 0x100
        jnz .otro
        mov eax, [esi]
        call ort_s_hijo
        call ort_s_cuesta_poner
        inc ecx
        call ort_s_rec
        dec ecx
.otro:
        test byte [esi+1], 2
        jnz .sin
        add esi, 4
        jmp .arco
.sin:
        mov ebp, [esp+8]
        mov ebx, [esp+16]
        test ebp, 0x100
        jnz .x
        cmp edx, [ort_ent_n]
        jae .x
        call ort_s_cuesta_quitar    ; sobra una letra: la tecleada
        inc edx
        call ort_s_rec
        dec edx
        test ebx, ebx
        jz .x
        ; dos vecinas trocadas: se truecan en la tecleada y se sigue (así
        ; valen también con un acento de más o de menos)
        mov ebp, [esp+8]
        lea eax, [edx+2]
        cmp eax, [ort_ent_n]
        ja .grupos
        mov ax, [ort_ent+edx]
        cmp al, ah
        je .grupos
        xchg al, ah
        mov [ort_ent+edx], ax
        add ebp, 4
        or ebp, 0x100
        call ort_s_rec
        xchg al, ah
        mov [ort_ent+edx], ax
        ; los grupos que se confunden («ll» por «y»...): igual, se cambian en
        ; la tecleada. Donde una letra pasa a ser dos, se usa el sitio de la
        ; letra de antes, que ya está gastada (por eso la palabra empieza en
        ; ort_ent+1: para que la primera también tenga una delante)
.grupos:
        mov esi, ort_grupos - 4
.gsig:
        add esi, 4
        mov ebp, [esp+8]
        mov edx, [esp+20]
        mov eax, [esi]              ; AL, AH = lo tecleado; detrás, lo que se prueba
        test al, al
        jz .x
        cmp al, [ort_ent+edx]
        jne .gsig
        add ebp, 2
        or ebp, 0x100
        test ah, ah
        jz .g1
        lea edi, [edx+2]            ; dos por una
        cmp edi, [ort_ent_n]
        ja .gsig
        cmp ah, [ort_ent+edx+1]
        jne .gsig
        inc edx
        mov al, [esi+2]
        xchg al, [ort_ent+edx]
        call ort_s_rec
        mov [ort_ent+edx], al
        jmp .gsig
.g1:
        dec edx                     ; una por dos
        mov ax, [ort_ent+edx]
        mov di, [esi+2]
        mov [ort_ent+edx], di
        call ort_s_rec
        mov [ort_ent+edx], ax
        jmp .gsig
.x:
        popad
.fuera:
        ret

; (interna) lo que cuesta poner la letra que falta, ort_sal[ECX], yendo por la
; letra EDX de la tecleada: lo suma a EBP y da la corrección por gastada
ort_s_cuesta_poner:
        push eax
        mov al, [ort_sal+ecx]
        cmp al, 'h'
        je .poco
        test ecx, ecx
        jz .mucho
        mov ah, [ort_sal+ecx-1]
        cmp al, ah                  ; doblada: «caro» por «carro»
        je .poco
        cmp ax, 'cx'                ; (AL='c', AH='x'): «exelente»
        je .poco
        cmp ax, 'cs'                ; «picina»
        je .poco
        cmp ax, 'ug'                ; «gerra»
        je .poco
        cmp ax, 'uq'
        je .poco
.mucho:
        add ebp, 5
        cmp edx, 1
        jne .x
        inc ebp
        jmp short .x
.poco:
        add ebp, 2
.x:
        or ebp, 0x100
        pop eax
        ret

; (interna) lo que cuesta quitar la letra EDX de la tecleada, que sobra
ort_s_cuesta_quitar:
        push eax
        mov al, [ort_ent+edx]
        cmp al, 'h'
        je .poco
        cmp edx, 1
        jbe .mucho
        cmp al, [ort_ent+edx-1]     ; doblada: «carro» por «caro»
        je .poco
.mucho:
        add ebp, 5
        cmp edx, 1
        jne .x
        inc ebp
        jmp short .x
.poco:
        add ebp, 2
.x:
        or ebp, 0x100
        pop eax
        ret

; (interna) la candidata ort_sal, de ECX letras y con la nota de EBP, es una
; palabra: se apunta entre las propuestas si su nota es de las ORT_SUG mejores
; (a igual nota, la que se encontró antes). A la misma palabra se puede llegar
; por dos caminos: se queda con la mejor nota.
ort_s_apunta:
        pushad
        mov eax, ebp
        and eax, 0xFF
        jz .x                       ; nota 0: es la tecleada, tal cual
        test ecx, ecx
        jz .x
        cmp ecx, ORT_LARGO
        ja .x
        mov byte [ort_sal+ecx], 0
        inc ecx                     ; (con su 0)
        xor edx, edx                ; ¿está ya?
.b:
        cmp edx, [ort_s_n]
        jae .mete
        mov edi, edx
        shl edi, 6
        add edi, ort_s_pal
        mov esi, ort_sal
        push ecx
        repe cmpsb
        pop ecx
        je .esta
        inc edx
        jmp .b
.esta:
        cmp al, [ort_s_pts+edx]
        jae .x
.quita:                             ; estaba con peor nota: se quita de ahí
        lea ebx, [edx+1]
        cmp ebx, [ort_s_n]
        jae .q1
        mov bl, [ort_s_pts+edx+1]
        mov [ort_s_pts+edx], bl
        mov edi, edx
        shl edi, 6
        add edi, ort_s_pal
        lea esi, [edi+64]
        push ecx
        mov ecx, 16
        rep movsd
        pop ecx
        inc edx
        jmp .quita
.q1:
        dec dword [ort_s_n]
.mete:
        xor edx, edx                ; EDX=su sitio: delante de la primera peor
.p:
        cmp edx, [ort_s_n]
        jae .aqui
        cmp [ort_s_pts+edx], al
        ja .aqui
        inc edx
        jmp .p
.aqui:
        cmp edx, ORT_SUG
        jae .x
        mov ebx, [ort_s_n]          ; se le hace sitio (la última, si no cabe,
        cmp ebx, ORT_SUG            ; se pierde)
        jb .d
        mov ebx, ORT_SUG - 1
.d:
        cmp ebx, edx
        jbe .pon
        mov ah, [ort_s_pts+ebx-1]
        mov [ort_s_pts+ebx], ah
        mov edi, ebx
        shl edi, 6
        add edi, ort_s_pal
        lea esi, [edi-64]
        push ecx
        mov ecx, 16
        rep movsd
        pop ecx
        dec ebx
        jmp .d
.pon:
        mov [ort_s_pts+edx], al
        mov edi, edx
        shl edi, 6
        add edi, ort_s_pal
        mov esi, ort_sal
        rep movsb
        cmp dword [ort_s_n], ORT_SUG
        jae .x
        inc dword [ort_s_n]
.x:
        popad
        ret
