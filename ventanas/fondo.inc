; =============================================================================
;  Hispa Ventanas - [FONDO] la imagen de fondo del escritorio
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Este archivo va en la «parte alta» de VENTANAS.COM (ventanas.asm): solo
;  código y datos de 32 bits. En los demás archivos solo hay ganchos de una o
;  dos líneas, marcados con [FONDO].
;
;  Pedido de Carlos: «igual ya tenemos imagen para el fondo de escritorio de
;  486». Detrás de las ventanas, en vez del dibujo de 16x16 (el mosaico), se
;  puede poner una imagen: ajustada a la pantalla (conserva la proporción; lo
;  que sobra se rellena con el color del escritorio), centrada (a su tamaño),
;  en mosaico (repetida), estirada (toda la pantalla, aunque se deforme) o
;  rellenando (toda la pantalla sin deformar: lo que sobra queda fuera). Vale para los dos escritorios (el de Hispa-386 y
;  el de Hispa-486) en todos los modos de VESA; en la VGA de 16 colores no
;  hay imagen: queda el dibujo. Sin la clave en VENTANAS.INI no hay imagen
;  (Hispa-386, de serie); la imagen de Hispa-486 la trae puesta.
;
;  En el disco es un BMP sin comprimir de 8 bits (con su paleta) o de 24. No
;  se descodifica nada más aquí: los demás formatos (JPEG, PNG, GIF...) los
;  convierte Mirador («Poner como fondo de escritorio») o, fuera,
;  tools/mkfondo.py, que además deja la imagen ya en los colores fijos de
;  Hispa Ventanas (el cubo de 6x6x6 y los grises: 16 a 255) con difuminado.
;  Un color del BMP que es exactamente uno de la paleta se queda tal cual
;  (una imagen preparada no se toca: se lee y basta); los demás van al cubo
;  con un difuminado ordenado (o a los grises, si es un gris).
;
;  Memoria: la imagen ya preparada (escalada o recortada: nunca más que la
;  pantalla) queda en un bloque de memoria extendida (mem_bloque), un octeto
;  por punto; nada en la convencional. Mientras se lee hay otro bloque con
;  el original (ancho x alto octetos), que se suelta al acabar (o pasa a ser
;  el definitivo, si no hay que escalar ni recortar). Pintar es copiar del
;  bloque a la superficie, solo en los rectángulos a la vista (dc_cada): tan
;  rápido como el mosaico, y sin parpadeo (la pantalla solo ve la superficie
;  ya compuesta).
;  De la memoria del programa (que es convencional) gasta unos 2,7 KB de
;  código y medio KB de variables (bssfondo.inc): lo que solo hace falta al
;  cargar (VENTANAS.INI, la paleta del BMP) usa el búfer de barra.inc.
;
;    fon_iniciar   (principal) lee «Fondo=» y «Ajuste=» de VENTANAS.INI y
;                  carga la imagen antes de pintar nada
;    fon_pintar    (escritorio) EAX=x EBX=y ECX=ancho EDX=alto, con el
;                  contexto abierto: la imagen o, si no hay, el mosaico
;    esc_imagen    (interfaz, entrada 97) leer lo puesto o poner otra:
;                  EAX=0 -> EAX=ajuste (FON_*) ESI=ruta («», sin fondo)
;                           ECX=0 sin imagen, 1 a la vista, 2 hay ruta pero
;                           no se ve (16 colores, o no se pudo cargar)
;                           EDX=un número que cambia cada vez que se pone
;                  EAX=1 ESI=ruta (0 o «»: sin fondo) EDX=ajuste
;                        -> EAX=0 y CF=0, o CF=1 y EAX=FON_E_*; si la nueva
;                           no vale, se queda la que había
;                  EAX=2 EDI=búfer de 80: la carpeta de VENTANAS.COM, con
;                        su «\» al final (donde Mirador deja MIFONDO.BMP)
;                  EAX=3 [ESTILO] -> EAX=0 el escritorio clásico, 1 el de la
;                        barra de tareas (barra.inc: bt_estilo_leer)
;                  EAX=4 [ESTILO] EDX=0/1: pasar a ese escritorio en marcha
;                        -> CF=1 si no se puede (barra.inc: bt_estilo_pedir)
;  La ruta sin unidad ni «\» delante es de la carpeta de VENTANAS.COM (así
;  la misma VENTANAS.INI vale en la memoria USB y en el disco duro).
; =============================================================================

FON_AJUSTADA equ 0                  ; a la pantalla, con su proporción
FON_CENTRADA equ 1                  ; a su tamaño, en el centro
FON_MOSAICO  equ 2                  ; repetida desde la esquina
FON_ESTIRADA equ 3                  ; a toda la pantalla, aunque se deforme
FON_RELLENAR equ 4                  ; a toda la pantalla, recortando lo que sobra
FON_E_ABRIR  equ 1                  ; no se puede abrir o leer
FON_E_FORMATO equ 2                 ; no es un BMP de 8 o 24 bits sin comprimir
FON_E_MEMORIA equ 3                 ; no hay memoria extendida
FON_E_COLORES equ 4                 ; VGA de 16 colores: se apunta, no se ve
FON_LADO    equ 4096                ; ancho y alto del BMP, como mucho
FON_INI     equ BT_BUF              ; VENTANAS.INI, como mucho (el búfer de barra.inc)

fon_k_fondo  db "fondo", 0
fon_k_ajuste db "ajuste", 0
fon_letras  db "a0c1m2e3r4"         ; «Ajuste=»: la primera letra (o el número)
; el difuminado ordenado (4x4)
fon_bayer   db 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5

; =============================================================================
;  Arranque: VENTANAS.INI
; =============================================================================
fon_iniciar:
        pushad
        cmp byte [op_demo], 0
        jne .x
        call dir_propia_poner
        mov edi, fon_tmp
        mov esi, dir_propia
        call pon_cad
        mov esi, bt_n_ini
        call pon_cad
        mov byte [edi], 0
        mov esi, fon_tmp
        xor al, al
        call dos_abrir
        jc .x
        mov ebx, eax
        mov edi, fon_buf
        mov ecx, FON_INI - 1
        call dos_leer
        jnc .l
        xor eax, eax
.l:
        call dos_cerrar
        mov byte [fon_buf+eax], 0
        mov esi, fon_buf
.linea:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, ' '
        ja .l1
        inc esi
        jmp .linea
.l1:
        mov edi, fon_k_fondo
        call fon_clave
        jc .l2
        mov edi, fon_ruta           ; el valor, hasta el fin de la línea
        mov ecx, 79
.v:
        mov al, [edx]
        cmp al, ' '
        jb .vf
        stosb
        inc edx
        dec ecx
        jnz .v
.vf:
        cmp edi, fon_ruta           ; (sin los espacios del final)
        je .vz
        cmp byte [edi-1], ' '
        jne .vz
        dec edi
        jmp .vf
.vz:
        mov byte [edi], 0
        jmp .salta
.l2:
        mov edi, fon_k_ajuste
        call fon_clave
        jc .salta
        mov al, [edx]               ; ajustada, centrada, mosaico, estirada o
        or al, 0x20                 ; rellenar (o 0 a 4)
        xor ah, ah
        mov edi, fon_letras
.ajb:
        cmp al, [edi]
        je .aj
        cmp al, [edi+1]
        je .aj
        add edi, 2
        inc ah
        cmp ah, FON_RELLENAR
        jbe .ajb
        jmp .salta
.aj:
        mov [fon_ajuste], ah
.salta:
        mov al, [esi]
        test al, al
        jz .fin
        inc esi
        cmp al, 10
        jne .salta
        jmp .linea
.fin:
        cmp byte [fon_ruta], 0
        je .x
        call fon_cargar             ; (si no se puede, queda el mosaico)
.x:
        popad
        ret

; (interna) ESI=principio de una línea EDI=clave en minúsculas -> CF=0 y
; EDX=su valor (detrás del «=» y de los espacios); CF=1 si es otra clave
fon_clave:
        mov edx, esi
.c:
        mov al, [edi]
        test al, al
        jz .fk
        mov ah, [edx]
        or ah, 0x20                 ; sin mirar mayúsculas (son letras)
        cmp al, ah
        jne .no
        inc edi
        inc edx
        jmp .c
.fk:
        cmp byte [edx], ' '
        jne .fk2
        inc edx
        jmp .fk
.fk2:
        cmp byte [edx], '='
        jne .no
.fk3:
        inc edx
        cmp byte [edx], ' '
        je .fk3
        clc
        ret
.no:
        stc
        ret

; =============================================================================
;  La interfaz (entrada 97): leer lo puesto o poner otra imagen
; =============================================================================
esc_imagen:
        cmp eax, 3                  ; [ESTILO] el estilo del escritorio (barra.inc):
        je bt_estilo_leer           ;   EAX=3 -> EAX=0 clásico, 1 barra de tareas
        cmp eax, 4                  ;   EAX=4 EDX=0/1: ponerlo -> CF=1 si no se puede
        je bt_estilo_pedir
        cmp eax, 2
        je .carpeta
        test eax, eax
        jnz .poner
        movzx eax, byte [fon_ajuste]
        mov esi, fon_ruta
        mov edx, [fon_serie]
        xor ecx, ecx
        cmp byte [esi], 0
        je .r
        inc ecx
        cmp byte [fon_hay], 0
        jne .r
        inc ecx
.r:
        clc
        ret
.carpeta:
        pushad
        call dir_propia_poner
        mov esi, dir_propia
        call pon_cad
        mov byte [edi], 0
        popad
        clc
        ret
.poner:
        pushad
        inc dword [fon_serie]
        push esi                    ; lo que había, por si la nueva no vale
        mov esi, fon_ruta
        mov edi, fon_ruta_ant
        mov ecx, 80 / 4
        rep movsd
        pop esi
        mov al, [fon_ajuste]
        mov [fon_ajuste_ant], al
        cmp edx, FON_RELLENAR
        jbe .a
        xor edx, edx
.a:
        mov [fon_ajuste], dl
        call fon_quitar
        mov edi, fon_ruta
        mov byte [edi], 0
        xor eax, eax
        test esi, esi
        jz .hecho
        mov ecx, 79
.c:
        lodsb
        test al, al
        jz .cf
        stosb
        dec ecx
        jnz .c
.cf:
        mov byte [edi], 0
        xor eax, eax
        cmp byte [fon_ruta], 0
        je .hecho
        call fon_cargar
        test eax, eax
        jz .hecho
        cmp eax, FON_E_COLORES      ; (16 colores: queda apuntada)
        je .hecho
        push eax                    ; no vale: vuelve la de antes
        mov esi, fon_ruta_ant
        mov edi, fon_ruta
        mov ecx, 80 / 4
        rep movsd
        mov al, [fon_ajuste_ant]
        mov [fon_ajuste], al
        cmp byte [fon_ruta], 0
        je .c2
        call fon_cargar
.c2:
        pop eax
.hecho:
        mov [esp+28], eax
        mov ebp, [escritorio]
        test ebp, ebp
        jz .h2
        call ven_invalidar
.h2:
        popad
        cmp eax, 1                  ; CF=1 si EAX es 0...
        cmc                         ; ...y aquí al revés: CF=1 si ha fallado
        ret

; (interna) suelta la imagen que haya
fon_quitar:
        cmp byte [fon_hay], 0
        je .x
        mov byte [fon_hay], 0
        push edx
        mov edx, [fon_mango]
        call mem_bloque_soltar
        pop edx
.x:
        ret

; =============================================================================
;  Pintar (con el contexto del escritorio abierto)
; =============================================================================
fon_pintar:
        cmp byte [fon_hay], 0
        jne .img
        mov esi, mosaico
        jmp dc_mosaico
.img:
        call fon_color              ; el color de lo que la imagen no tapa
        push dword fon_gdi          ; (como las primitivas de dc.inc: una
        jmp dc_cada                 ; vez por cada rectángulo a la vista)

; (interna) la primitiva: EAX=x EBX=y ECX=ancho EDX=alto, en la superficie y
; con el recorte ya puesto
fon_gdi:
        pushad
        call gdi_cortar
        cmp byte [fon_ajuste], FON_MOSAICO
        je .mos
        mov al, [fon_cbarras]
        mov [gdi_color], al
        xor eax, eax                ; arriba,
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [fon_y]
        call gdi_rect
        mov ebx, [fon_y]            ; abajo,
        add ebx, [fon_al]
        mov edx, [vid_alto]
        sub edx, ebx
        call gdi_rect
        mov ebx, [fon_y]            ; a la izquierda
        mov edx, [fon_al]
        mov ecx, [fon_x]
        call gdi_rect
        mov eax, [fon_x]            ; y a la derecha
        add eax, [fon_an]
        mov ecx, [vid_ancho]
        sub ecx, eax
        call gdi_rect
        mov eax, [fon_x]
        mov ebx, [fon_y]
        mov ecx, [fon_an]
        mov edx, [fon_al]
        mov esi, [fon_ptr]
        call gdi_copiar
        jmp .x
.mos:
        mov eax, [rec_y0]           ; solo las piezas que tocan el recorte
        xor edx, edx
        div dword [fon_al]
        imul eax, [fon_al]
        mov ebx, eax
        mov esi, [fon_ptr]
.my:
        cmp ebx, [rec_y1]
        jge .x
        mov eax, [rec_x0]
        xor edx, edx
        div dword [fon_an]
        imul eax, [fon_an]
.mx:
        cmp eax, [rec_x1]
        jge .mys
        mov ecx, [fon_an]
        mov edx, [fon_al]
        call gdi_copiar
        add eax, [fon_an]
        jmp .mx
.mys:
        add ebx, [fon_al]
        jmp .my
.x:
        popad
        ret

; (interna) fon_cbarras = el color del escritorio: el que más sale en el
; mosaico (el del fondo del dibujo que haya puesto Configuración)
fon_color:
        pushad
        mov edi, fon_hist
        xor eax, eax
        mov ecx, 256 / 2
        rep stosd
        mov esi, mosaico
        mov ecx, 256
.c:
        lodsb
        inc word [fon_hist+eax*2]
        loop .c
        xor ebx, ebx                ; el mejor
        xor edx, edx                ; y cuántas veces
.m:
        cmp [fon_hist+ecx*2], dx
        jbe .s
        mov dx, [fon_hist+ecx*2]
        mov ebx, ecx
.s:
        inc ecx
        cmp ecx, 256
        jb .m
        mov [fon_cbarras], bl
        popad
        ret

; =============================================================================
;  Cargar el BMP de fon_ruta -> EAX=0 (y fon_hay=1) o FON_E_*
; =============================================================================
fon_cargar:
        pushad
        mov dword [esp+28], FON_E_COLORES
        cmp byte [vid_tipo], 1
        jne .x
        mov esi, fon_ruta           ; la ruta: tal cual o en la carpeta propia
        mov edi, fon_tmp
        cmp byte [esi+1], ':'
        je .rc
        cmp byte [esi], '\'
        je .rc
        push esi
        call dir_propia_poner
        mov esi, dir_propia
        call pon_cad
        pop esi
.rc:
        call pon_cad
        mov byte [edi], 0
        mov dword [esp+28], FON_E_ABRIR
        mov esi, fon_tmp
        xor al, al
        call dos_abrir
        jc .x
        mov [fon_man], eax
        mov ebx, eax
        mov edi, fon_cab
        mov ecx, 54
        call dos_leer
        jc .cierra
        mov dword [esp+28], FON_E_FORMATO
        cmp eax, 54
        jne .cierra
        cmp word [fon_cab], 'BM'
        jne .cierra
        cmp dword [fon_cab+14], 40  ; la cabecera de Windows (o más nueva)
        jb .cierra
        cmp dword [fon_cab+30], 0   ; sin comprimir
        jne .cierra
        movzx eax, word [fon_cab+28]
        cmp eax, 8
        je .bits
        cmp eax, 24
        jne .cierra
.bits:
        mov [fon_bpp], eax
        mov eax, [fon_cab+18]
        cmp eax, 1
        jl .cierra
        cmp eax, FON_LADO
        jg .cierra
        mov [fon_san], eax
        mov eax, [fon_cab+22]       ; el alto (negativo: de arriba abajo)
        mov byte [fon_arriba], 0
        test eax, eax
        jns .alto
        neg eax
        mov byte [fon_arriba], 1
.alto:
        cmp eax, 1
        jl .cierra
        cmp eax, FON_LADO
        jg .cierra
        mov [fon_sal], eax
        mov eax, [fon_san]          ; lo que ocupa una fila en el archivo
        imul eax, [fon_bpp]
        add eax, 31
        shr eax, 5
        shl eax, 2
        mov [fon_fila], eax
        cmp dword [fon_bpp], 8
        jne .datos
        ; ---- la paleta: cada color, a rojo-verde-azul y, si es uno de los de
        ; Hispa Ventanas, su número ----
        mov edi, fon_pal
        xor eax, eax
        mov ecx, 256
        rep stosd
        mov edx, [fon_cab+14]
        add edx, 14
        xor al, al
        call dos_puntero
        jc .cierra
        mov ecx, [fon_cab+46]
        test ecx, ecx
        jnz .pn
        mov ecx, 256
.pn:
        cmp ecx, 256
        ja .cierra
        shl ecx, 2
        mov edi, fon_pal
        call dos_leer
        jc .cierra
        xor esi, esi
.pe:
        mov eax, [fon_pal+esi*4]    ; azul, verde, rojo, 0
        mov ecx, eax
        shr ecx, 16
        and ecx, 0xFF               ; rojo
        mov edx, eax
        and edx, 0xFF
        shl edx, 16                 ; azul
        and eax, 0xFF00
        or eax, ecx
        or eax, edx
        mov [fon_pal+esi*4], eax    ; como en pal_rgb: rojo, verde, azul, 0
        xor ecx, ecx
.pb:
        cmp eax, [pal_rgb+ecx*4]
        je .ph
        inc ecx
        cmp ecx, 256
        jb .pb
        mov ecx, 0xFFFF             ; no está: irá al cubo
.ph:
        mov [fon_exacto+esi*2], cx
        inc esi
        cmp esi, 256
        jb .pe
.datos:
        mov ebx, [fon_man]
        mov edx, [fon_cab+10]
        xor al, al
        call dos_puntero
        jc .cierra
        mov dword [esp+28], FON_E_MEMORIA
        mov ecx, [fon_san]          ; el original (un octeto por punto) y,
        imul ecx, [fon_sal]         ; detrás, una fila del archivo
        add ecx, [fon_fila]
        add ecx, 16
        call mem_bloque
        jc .cierra
        mov [fon_sptr], eax
        mov [fon_smango], edx
        mov ecx, [fon_san]
        imul ecx, [fon_sal]
        add eax, ecx
        mov [fon_lin], eax
        mov dword [esp+28], FON_E_ABRIR
        xor ebp, ebp                ; la fila del archivo
.fila:
        mov ebx, [fon_man]
        mov edi, [fon_lin]
        mov ecx, [fon_fila]
        call dos_leer
        jc .suelta
        cmp eax, ecx
        jne .suelta
        mov eax, ebp
        cmp byte [fon_arriba], 0
        jne .d
        mov eax, [fon_sal]          ; (lo normal: la última fila, la primera)
        dec eax
        sub eax, ebp
.d:
        mov [fon_ty], eax
        imul eax, [fon_san]
        add eax, [fon_sptr]
        mov edi, eax
        mov esi, [fon_lin]
        xor ecx, ecx
        cmp dword [fon_bpp], 8
        jne .f24
.f8:
        movzx eax, byte [esi]
        inc esi
        mov dx, [fon_exacto+eax*2]
        test dh, dh
        jnz .f8d
        mov [edi], dl
        jmp .f8s
.f8d:
        mov eax, [fon_pal+eax*4]
        call fon_rgb
        mov [edi], al
.f8s:
        inc edi
        inc ecx
        cmp ecx, [fon_san]
        jb .f8
        jmp .sig
.f24:
        mov eax, [esi]              ; azul, verde, rojo (y uno del siguiente)
        add esi, 3
        mov edx, eax
        shr edx, 16
        and edx, 0xFF
        mov ebx, eax
        and ebx, 0xFF
        shl ebx, 16
        and eax, 0xFF00
        or eax, edx
        or eax, ebx
        call fon_rgb
        mov [edi], al
        inc edi
        inc ecx
        cmp ecx, [fon_san]
        jb .f24
.sig:
        inc ebp
        cmp ebp, [fon_sal]
        jb .fila
        mov ebx, [fon_man]
        call dos_cerrar
        mov dword [esp+28], FON_E_MEMORIA
        call fon_montar
        jc .x
        mov dword [esp+28], 0
        jmp .x
.suelta:
        mov edx, [fon_smango]
        call mem_bloque_soltar
.cierra:
        mov ebx, [fon_man]
        call dos_cerrar
.x:
        popad
        ret

; (interna) EAX=color (rojo, verde, azul, 0) ECX=x [fon_ty]=y -> AL=su número
; en la paleta: un gris, a los grises (232-255); lo demás, al cubo (16-231),
; con el difuminado ordenado
fon_rgb:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ebx, [fon_ty]
        and ebx, 3
        and ecx, 3
        movzx esi, byte [fon_bayer+ebx*4+ecx]
        mov edi, eax
        mov edx, eax
        shr edx, 8
        cmp al, dl
        jne .color
        cmp dl, dh
        jne .color
        movzx eax, al               ; gris: de 11 en 11
        cmp eax, 253
        ja .blanco
        imul ecx, esi, 11
        shr ecx, 4
        add eax, ecx
        xor edx, edx
        mov ecx, 11
        div ecx
        cmp eax, 23
        jbe .g
        mov eax, 23
.g:
        add eax, 232
        jmp .x
.blanco:
        mov eax, 231                ; (el blanco del cubo)
        jmp .x
.color:
        shl esi, 4
        add esi, 7                  ; el umbral: 7 a 247
        xor ebx, ebx
        mov ecx, 3
.c:
        mov eax, edi
        and eax, 0xFF
        shr edi, 8
        lea eax, [eax+eax*4]        ; (5 c + umbral) / 255: de 0 a 5
        add eax, esi
        imul eax, 0x8081
        shr eax, 23
        imul ebx, 6
        add ebx, eax
        dec ecx
        jnz .c
        lea eax, [ebx+16]
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) del original (fon_sptr, fon_san x fon_sal) a la imagen lista
; (fon_ptr, fon_an x fon_al, en fon_x, fon_y) según fon_ajuste. El original
; se suelta (o se queda de imagen, si no hay que tocarlo). CF=1 sin memoria
fon_montar:
        pushad
        mov eax, [fon_san]
        mov ebx, [fon_sal]
        xor ecx, ecx
        mov [fon_rx], ecx           ; el trozo del original que se usa
        mov [fon_ry], ecx
        mov [fon_rw], eax
        mov [fon_rh], ebx
        cmp byte [fon_ajuste], FON_AJUSTADA
        je .aj
        cmp byte [fon_ajuste], FON_ESTIRADA
        je .est
        cmp byte [fon_ajuste], FON_RELLENAR
        jne .cm
        ; rellenar: del original, el trozo del centro con la proporción de la
        ; pantalla; y ese trozo, a toda la pantalla
        mov ecx, eax
        imul ecx, [vid_alto]
        mov edx, ebx
        imul edx, [vid_ancho]
        cmp ecx, edx
        jbe .ralta
        mov eax, edx                ; más ancha: sobran los lados
        xor edx, edx
        div dword [vid_alto]
        test eax, eax
        jnz .r1
        inc eax
.r1:
        mov [fon_rw], eax
        mov ecx, [fon_san]
        sub ecx, eax
        shr ecx, 1
        mov [fon_rx], ecx
        jmp .est
.ralta:
        mov eax, ecx                ; más alta: sobra por arriba y por abajo
        xor edx, edx
        div dword [vid_ancho]
        test eax, eax
        jnz .r2
        inc eax
.r2:
        mov [fon_rh], eax
        mov ecx, [fon_sal]
        sub ecx, eax
        shr ecx, 1
        mov [fon_ry], ecx
.est:
        mov eax, [vid_ancho]        ; (y estirada: todo el original)
        mov ebx, [vid_alto]
        jmp .dims
.cm:
        ; centrada y mosaico: a su tamaño; lo que no cabe, fuera (centrada:
        ; por los dos lados; mosaico: por la derecha y por abajo)
        cmp eax, [vid_ancho]
        jbe .c1
        mov ecx, eax
        sub ecx, [vid_ancho]
        shr ecx, 1
        mov [fon_rx], ecx
        mov eax, [vid_ancho]
        mov [fon_rw], eax
.c1:
        cmp ebx, [vid_alto]
        jbe .c2
        mov ecx, ebx
        sub ecx, [vid_alto]
        shr ecx, 1
        mov [fon_ry], ecx
        mov ebx, [vid_alto]
        mov [fon_rh], ebx
.c2:
        cmp byte [fon_ajuste], FON_MOSAICO
        jne .dims
        xor ecx, ecx
        mov [fon_rx], ecx
        mov [fon_ry], ecx
        jmp .dims
.aj:
        mov ecx, eax                ; ¿más ancha o más alta que la pantalla?
        imul ecx, [vid_alto]
        mov edx, ebx
        imul edx, [vid_ancho]
        cmp ecx, edx
        jae .ancha
        mov eax, ecx                ; más alta: todo el alto
        xor edx, edx
        div ebx
        mov ebx, [vid_alto]
        jmp .min
.ancha:
        mov eax, edx                ; más ancha: todo el ancho
        xor edx, edx
        div dword [fon_san]
        mov ebx, eax
        mov eax, [vid_ancho]
.min:
        test eax, eax
        jnz .m1
        inc eax
.m1:
        test ebx, ebx
        jnz .dims
        inc ebx
.dims:
        mov [fon_an], eax
        mov [fon_al], ebx
        mov ecx, [vid_ancho]
        sub ecx, eax
        sar ecx, 1
        mov edx, [vid_alto]
        sub edx, ebx
        sar edx, 1
        cmp byte [fon_ajuste], FON_MOSAICO
        jne .pos
        xor ecx, ecx
        xor edx, edx
.pos:
        mov [fon_x], ecx
        mov [fon_y], edx
        cmp eax, [fon_san]          ; ¿vale tal cual?
        jne .hacer
        cmp ebx, [fon_sal]
        jne .hacer
        mov eax, [fon_sptr]
        mov [fon_ptr], eax
        mov eax, [fon_smango]
        mov [fon_mango], eax
        jmp .ok
.hacer:
        mov ecx, eax                ; la imagen y, detrás, la tabla de las x
        imul ecx, ebx
        mov esi, ecx
        lea ecx, [ecx+eax*4+8]
        call mem_bloque
        jc .nomem
        mov [fon_ptr], eax
        mov [fon_mango], edx
        add esi, eax
        mov [fon_xt], esi
        xor ecx, ecx                ; dónde empieza cada punto en el original
.xt:
        mov eax, ecx
        mul dword [fon_rw]
        div dword [fon_an]
        add eax, [fon_rx]
        mov [esi+ecx*4], eax
        inc ecx
        cmp ecx, [fon_an]
        jbe .xt
        mov edi, [fon_ptr]
        xor ebp, ebp
.fy:
        mov [fon_ty], ebp
        mov eax, ebp
        mul dword [fon_rh]
        div dword [fon_al]
        add eax, [fon_ry]
        mov [fon_y0], eax
        lea eax, [ebp+1]
        mul dword [fon_rh]
        div dword [fon_al]
        add eax, [fon_ry]
        cmp eax, [fon_y0]
        ja .y1
        mov eax, [fon_y0]
        inc eax
.y1:
        mov [fon_y1], eax
        xor ecx, ecx
.fx:
        mov esi, [fon_xt]
        mov ebx, [esi+ecx*4]
        mov edx, [esi+ecx*4+4]
        cmp edx, ebx
        ja .x1
        lea edx, [ebx+1]
.x1:
        mov eax, [fon_y1]           ; un punto por punto: el mismo color (al
        sub eax, [fon_y0]           ; ampliar no se difumina otra vez)
        cmp eax, 1
        jne .caja
        lea eax, [ebx+1]
        cmp eax, edx
        jne .caja
        mov eax, [fon_y0]
        imul eax, [fon_san]
        add eax, ebx
        add eax, [fon_sptr]
        mov al, [eax]
        jmp .pon
.caja:
        call fon_caja               ; al reducir: la media de los que caen
        call fon_rgb                ; en el punto, otra vez a la paleta
.pon:
        stosb
        inc ecx
        cmp ecx, [fon_an]
        jb .fx
        inc ebp
        cmp ebp, [fon_al]
        jb .fy
        mov edx, [fon_smango]
        call mem_bloque_soltar
.ok:
        mov byte [fon_hay], 1
        popad
        clc
        ret
.nomem:
        mov edx, [fon_smango]
        call mem_bloque_soltar
        popad
        stc
        ret

; (interna) EBX=x0 EDX=x1 (fuera), fon_y0 a fon_y1: la media de los colores
; de esa caja del original -> EAX (rojo, verde, azul, 0)
fon_caja:
        push ecx
        push edx
        push esi
        push edi
        push ebp
        xor eax, eax
        mov [fon_sr], eax
        mov [fon_sg], eax
        mov [fon_sb], eax
        mov ebp, [fon_y0]
.y:
        mov esi, ebp
        imul esi, [fon_san]
        add esi, [fon_sptr]
        mov ecx, ebx
.x:
        movzx eax, byte [esi+ecx]
        mov eax, [pal_rgb+eax*4]
        movzx edi, al
        add [fon_sr], edi
        movzx edi, ah
        add [fon_sg], edi
        shr eax, 16
        add [fon_sb], eax
        inc ecx
        cmp ecx, edx
        jb .x
        inc ebp
        cmp ebp, [fon_y1]
        jb .y
        mov ecx, edx                ; cuántos son
        sub ecx, ebx
        mov eax, [fon_y1]
        sub eax, [fon_y0]
        imul ecx, eax
        mov edi, ecx
        shr edi, 1
        mov eax, [fon_sb]
        add eax, edi
        xor edx, edx
        div ecx
        mov ebp, eax
        shl ebp, 8
        mov eax, [fon_sg]
        add eax, edi
        xor edx, edx
        div ecx
        or ebp, eax
        shl ebp, 8
        mov eax, [fon_sr]
        add eax, edi
        xor edx, edx
        div ecx
        or eax, ebp
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        ret
