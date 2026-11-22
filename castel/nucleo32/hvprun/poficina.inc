; =============================================================================
;  HVPRUN - (OFICINA2) lo que le hacía falta a la oficina 1.1 (Datos, el
;  Lector, Publicaciones, Presentaciones y Pintura, además de Facturas,
;  Pluma, Cuentas y Mirador): dónde se trabaja, la rueda y Alt+letra
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  - La carpeta de trabajo (la que enseñan los cuadros de Abrir y Guardar
;    como, y donde va lo que se guarda sin ruta): si se abre un documento con
;    su ruta, la suya; si no, los documentos del usuario: con usuarios (la
;    llamada 200: la ficha que es la suya), \USUARIOS\<SU CARPETA>\DOCUMENT;
;    sin ellos, \DOCUMENT si está; y si no, la de siempre (la raíz). Nunca
;    \HISPA, la zona protegida: ahí no se puede escribir.
;  - La rueda del ratón (el suceso 13 de Hispa-Pentium) se le da a Ventanas
;    (ges_rueda: WM_MOUSEWHEEL a la ventana de debajo del cursor).
;  - Alt+letra (Hispa-Pentium da TECLA_ALT + la letra) llega como en DOS: AL
;    = 0 y AH = el código de la tecla: así abre el menú que la tiene marcada.
; =============================================================================

PS_USUARIOS     equ 200             ; (rama usuarios) 0: cuántos; 1: la ficha i
FU_ES_YO        equ 7               ; en la ficha (64 octetos): ¿es el de este programa?
FU_CARPETA      equ 36              ; su carpeta (12 octetos), en \USUARIOS
EVP_RUEDA       equ 13              ; a, b: dónde; c: los pasos (octeto bajo, con signo: arriba, positivos)
TECLA_ALT_A     equ 0x180           ; Alt+letra: 0x180 + (la letra - «a»)

; la carpeta de trabajo, según lo de arriba (con hvp_param ya leído)
p_carpeta_docs:
        pushad
        mov esi, hvp_param
.e:
        cmp byte [esi], ' '
        jne .p
        inc esi
        jmp .e
.p:
        cmp byte [esi], 0
        jne .con_doc
        ; los documentos del usuario (si el núcleo sabe de usuarios)
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        PSIS PS_USUARIOS
        test eax, eax
        jz .sin_usuarios
        cmp eax, 64
        ja .sin_usuarios            ; (un error, o un núcleo sin usuarios)
        mov ebp, eax
        xor edi, edi
.u:
        mov ebx, 1
        mov ecx, edi
        mov edx, p_ficha
        PSIS PS_USUARIOS
        cmp eax, 1
        jne .u_sig
        cmp byte [p_ficha+FU_ES_YO], 0
        je .u_sig
        cmp byte [p_ficha+FU_CARPETA], 0
        je .sin_usuarios
        push edi
        mov edi, p_aux2
        mov esi, t_usuarios
        call .copia
        mov esi, p_ficha+FU_CARPETA
        mov ecx, 11
.cc:
        lodsb
        test al, al
        jz .cf
        stosb
        loop .cc
.cf:
        mov esi, t_su_docs
        call .copia
        mov byte [edi], 0
        pop edi
        mov esi, p_aux2
        call dos_ir_carpeta
        jnc .x
        jmp .sin_usuarios
.u_sig:
        inc edi
        cmp edi, ebp
        jb .u
.sin_usuarios:
        mov esi, t_document
        call dos_ir_carpeta         ; (si no está, se queda donde estaba)
        jmp .x
.con_doc:
        ; un documento con su ruta entera («\A\B.PLU», «C:\A\B.PLU»): a su carpeta
        cmp byte [esi], '\'
        je .r
        cmp byte [esi+1], ':'
        jne .x
.r:
        mov edi, p_aux2
        xor edx, edx                ; EDX=dónde va la última «\» (en p_aux2)
        mov ecx, 120
.rc:
        lodsb
        cmp al, ' '
        jbe .rf
        stosb
        cmp al, '\'
        jne .rs
        mov edx, edi
.rs:
        loop .rc
.rf:
        test edx, edx
        jz .x
        mov byte [edx], 0           ; hasta la «\» (incluida)
        lea eax, [edx-1]
        cmp eax, p_aux2
        je .va                      ; «\»: la raíz
        cmp byte [edx-2], ':'
        je .va                      ; «C:\»
        mov byte [edx-1], 0         ; «\A\B» (sin la última «\»)
.va:
        mov esi, p_aux2
        call dos_ir_carpeta
.x:
        popad
        ret
; (interna) ESI -> EDI, sin el 0
.copia:
        lodsb
        test al, al
        jz .copia_x
        stosb
        jmp .copia
.copia_x:
        ret

t_usuarios  db "\USUARIOS\", 0
t_su_docs   db "\DOCUMENT", 0
t_document  db "\DOCUMENT", 0

; la rueda: la flecha, donde dice el suceso; y sus pasos, a Ventanas
; (positivos, hacia abajo)
pan_rueda:
        cmp dword [pan_ppal], 0
        je .x
        mov ecx, [pan_suceso+12]
        call pan_cambio
        mov eax, [pan_suceso+4]
        mov ebx, [pan_suceso+8]
        cmp eax, [rat_x]
        jne .m
        cmp ebx, [rat_y]
        je .r
.m:
        call rat_mover
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_raton_mueve
.r:
        movsx eax, byte [pan_suceso+12]
        neg eax
        call ges_rueda
        mov byte [tec_mayus], 0
.x:
        ret

; Alt+letra -> EAX como lo daría INT 16h (AL = 0, AH = el código de la
; tecla) y CF=0; CF=1 si EAX no es una de ellas
pan_alt_letra:
        cmp eax, TECLA_ALT_A
        jb .no
        cmp eax, TECLA_ALT_A + 25
        ja .no
        movzx eax, byte [alt_codigos+eax-TECLA_ALT_A]
        shl eax, 8
        or byte [tec_mayus], TM_ALT
        clc
        ret
.no:
        stc
        ret

; los códigos de las teclas de la A a la Z (los del teclado del PC)
alt_codigos db 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32
            db 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C

; el módulo (\OFICINA\PLUMA.HVP), si se ha dado sin la letra y no está en la
; unidad de trabajo, en la del sistema: Mi equipo abre un documento de una
; memoria USB (D:\CARTA.PLU) con la unidad de trabajo en la suya, y la
; oficina está en la del sistema (C:\OFICINA)
p_modulo_del_sistema:
        pushad
        cmp byte [p_aux+1], ':'
        je .x                       ; (con su letra: donde se dice)
        mov esi, hvp_ruta
        xor al, al
        call dos_abrir
        jc .busca
        mov ebx, eax
        call dos_cerrar
        jmp .x
.busca:
        xor esi, esi
.b:
        mov ebx, esi
        mov ecx, p_info
        mov eax, 43
        int 0x80
        cmp eax, 1
        jne .x
        cmp dword [p_info+12], 1    ; del_sistema
        je .si
        inc esi
        cmp esi, 32
        jb .b
        jmp .x
.si:
        mov al, [p_info]
        test al, al
        jz .x
        mov [hvp_ruta], al          ; «C:\OFICINA\PLUMA.HVP»
.x:
        popad
        ret

; el título de la ventana, que en Hispa-Pentium cabe en 63 letras (MAX_TÍTULO
; de ventanas.cas; con más, el núcleo no lo pone): ESI=el del programa ->
; ESI=él mismo o, si es más largo, en p_titulo, lo de antes de « - » (el
; nombre del programa), «...» y el final de la ruta («Lector -
; ...\2026\PROGRAMA\MUESTRA.PDF»)
TIT_MAX     equ 63
p_titulo_corto:
        push eax
        push ecx
        push edx
        push edi
        mov edi, esi
        xor ecx, ecx
.l:
        cmp byte [edi+ecx], 0
        je .lf
        inc ecx
        jmp .l
.lf:
        cmp ecx, TIT_MAX
        jbe .x                      ; (cabe: tal cual)
        xor edx, edx                ; EDX=lo de delante: hasta « - » (incluido)
.p:
        cmp edx, 32
        jae .sin_p
        cmp byte [esi+edx], 0
        je .sin_p
        cmp word [esi+edx], ' -'
        jne .p1
        cmp byte [esi+edx+2], ' '
        jne .p1
        add edx, 3
        jmp .copia
.p1:
        inc edx
        jmp .p
.sin_p:
        xor edx, edx
.copia:
        push esi
        mov edi, p_titulo
        push ecx
        mov ecx, edx
        rep movsb                   ; lo de delante
        pop ecx
        pop esi
        mov eax, '...'
        stosd
        dec edi                     ; (los tres puntos, sin el 0)
        mov eax, TIT_MAX - 3
        sub eax, edx                ; lo que cabe del final
        sub ecx, eax
        push esi
        add esi, ecx
        mov ecx, eax
.lim:
        cmp byte [esi], '\'        ; que empiece en una «\» (sin media carpeta)
        je .cola
        cmp ecx, 13
        jbe .cola
        inc esi
        dec ecx
        jmp .lim
.cola:
        rep movsb
        mov byte [edi], 0
        pop esi
        mov esi, p_titulo
.x:
        pop edi
        pop edx
        pop ecx
        pop eax
        ret
