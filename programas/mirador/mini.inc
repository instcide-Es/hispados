; =============================================================================
;  MIRADOR.HVP - las miniaturas de la carpeta («hoja de contactos»)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  En vez de la imagen, la zona enseña una rejilla con una miniatura de cada
;  imagen de la carpeta (las de la lista de carpeta.inc) y su nombre debajo.
;  Se elige con las flechas, AvPág, RePág, Inicio, Fin o un clic; Intro o un
;  doble clic la abren; Esc vuelve a la imagen que se estaba viendo.
;
;  Las miniaturas se hacen de una en una, con un temporizador: entre una y
;  otra se atienden el teclado y el ratón, así que la rejilla se va llenando
;  sin dejar la ventana parada (primero las que se ven). Cada una se lee como
;  cualquier imagen (el núcleo de formatos; un PNG, un BMP o un TGA grande,
;  del disco y sin cargarlo entero), pero reducida al vuelo a su casilla de
;  96x80 (reduce.inc; un JPEG, antes, por su propio descodificador) y pasada
;  a la paleta. La foto de un móvil se pone derecha (la orientación de su
;  EXIF), como al abrirla: se hace tumbada y se gira al guardarla.
;
;  Se guardan en memoria mientras no se cambie de carpeta (volver a la
;  rejilla es inmediato), cada una con lo que ocupa de verdad (ancho x alto:
;  una apaisada de 96x54, 5 K; un icono de 16x16, 256 octetos), seguidas, en
;  bloques de 64 K que se piden según hacen falta: no se aparta de entrada
;  sitio para todas las de la carpeta. Si la memoria se acaba, las que no
;  caben se quedan con su recuadro vacío; y si al abrir una imagen falta
;  memoria, las miniaturas se sueltan.
; =============================================================================
MINI_AN     equ 96                  ; la miniatura, como mucho
MINI_AL     equ 80
MINI_HUECO  equ MINI_AN * MINI_AL
CEL_AN      equ 112                 ; la casilla: la miniatura y su nombre
CEL_AL      equ 104
ME_PENDIENTE equ 0                  ; estado de cada miniatura
ME_HECHA    equ 1
ME_NO       equ 2                   ; no se puede ver
ME_SIN_SITIO equ 3                  ; no hay memoria para guardarla
ID_RELOJ_MINI equ 2
MINI_TROZO  equ 65536               ; los bloques en los que se guardan
MINI_TROZOS equ 128                 ; (para mil miniaturas de las mayores)

%macro MINI_BSS 0
            alignb 4
mini_tz_n   resd 1                  ; bloques pedidos
mini_tz_libre resd 1                ; por dónde va el último
mini_tmp    resd 1                  ; la miniatura como sale del archivo (sin girar)
mini_pan    resd 1                  ; sus medidas
mini_pal    resd 1
mini_rot    resd 1                  ; cómo hay que ponerla: cuartos de vuelta a la derecha
mini_volt   resd 1                  ; y, después, volteada o no
mini_sel    resd 1                  ; la elegida
mini_fila0  resd 1                  ; la primera fila de casillas que se ve
mini_cols   resd 1
mini_vis    resd 1                  ; filas que se ven
mini_x0     resd 1
mini_i      resd 1                  ; la que se está haciendo
mini_tan    resd 1
mini_tal    resd 1
mini_fila   resd 1
mini_y      resd 1
mini_th     resd 1                  ; la memoria de trabajo
mini        resb 1                  ; 1: se ven las miniaturas
mini_hay    resb 1                  ; hay bloque
            alignb 4
mini_ruta   resb 132
mini_est    resb LST_MAX
mini_ta     resb LST_MAX            ; ancho y alto de cada una
mini_tb     resb LST_MAX
            alignb 4
mini_ptr    resd LST_MAX            ; dónde está cada una
mini_tz_p   resd MINI_TROZOS        ; los bloques y sus mangos
mini_tz_h   resd MINI_TROZOS
%endmacro

t_mini_pie  db "Miniaturas: Intro abre la elegida; Esc vuelve", 0
t_mini_no   db "?", 0

; entra en las miniaturas, o sale si ya estaba
mini_alterna:
        cmp byte [mini], 0
        jne mini_sal
        cmp dword [lst_n], 0
        jne .hay
        mov esi, t_primero
        jmp aviso
.hay:
        pushad
        cmp byte [pantc], 0         ; (es cosa de la ventana)
        je .v
        call completa_sal
.v:
        mov byte [mini], 1
        mov eax, [lst_i]
        mov [mini_sel], eax
        cmp byte [mini_hay], 0
        jne .listo
        mov edi, mini_est           ; otra carpeta: todas por hacer
        mov ecx, LST_MAX
        xor al, al
        rep stosb
        mov byte [mini_hay], 1
.listo:
        mov ebp, [barra_v]          ; la barra, de fila en fila
        mov eax, BDM_LINEA
        mov ecx, 1
        HV msg_enviar
        call dispone
        call mini_a_la_vista
        mov ebp, [ven]
        mov ecx, ID_RELOJ_MINI
        mov edx, 60
        HV tmp_poner
        call inval
        popad
        ret

mini_sal:
        pushad
        cmp byte [mini], 0
        je .x
        mov byte [mini], 0
        call pon_marcas
        mov ebp, [ven]
        mov ecx, ID_RELOJ_MINI
        HV tmp_quitar
        mov ebp, [barra_v]
        mov eax, BDM_LINEA
        mov ecx, 32
        HV msg_enviar
        call dispone
        call inval
.x:
        popad
        ret

; suelta las miniaturas (otra carpeta, o hace falta la memoria)
mini_suelta:
        pushad
        mov ebx, [mini_tz_n]
.s:
        test ebx, ebx
        jz .s9
        dec ebx
        mov edx, [mini_tz_h+ebx*4]
        HV mem_bloque_soltar
        jmp .s
.s9:
        mov dword [mini_tz_n], 0
        mov byte [mini_hay], 0
        popad
        ret

; (interna) ECX=octetos (hasta MINI_TROZO) -> EAX=dónde guardarlos, a
; continuación de lo que ya hay; CF=1 si no hay memoria
mini_pide:
        push ebx
        push edx
        mov ebx, [mini_tz_n]
        test ebx, ebx
        jz .nuevo
        mov eax, [mini_tz_libre]
        add eax, ecx
        cmp eax, MINI_TROZO
        jbe .cabe
.nuevo:
        cmp ebx, MINI_TROZOS
        jae .no
        push ecx
        mov ecx, MINI_TROZO + 16
        HV mem_bloque
        pop ecx
        jc .no
        mov [mini_tz_p+ebx*4], eax
        mov [mini_tz_h+ebx*4], edx
        inc ebx
        mov [mini_tz_n], ebx
        mov dword [mini_tz_libre], 0
.cabe:
        mov eax, [mini_tz_p+ebx*4-4]
        add eax, [mini_tz_libre]
        add [mini_tz_libre], ecx
        pop edx
        pop ebx
        clc
        ret
.no:
        pop edx
        pop ebx
        stc
        ret

; la rejilla: columnas, filas que se ven y la barra (la llama «dispone»)
mini_mide:
        pushad
        mov eax, [vw]
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        test eax, eax
        jnz .c1
        inc eax
.c1:
        mov [mini_cols], eax
        imul eax, CEL_AN
        neg eax
        add eax, [vw]
        sar eax, 1
        jns .c2
        xor eax, eax
.c2:
        mov [mini_x0], eax
        mov eax, [vh]
        xor edx, edx
        mov ecx, CEL_AL
        div ecx
        test eax, eax
        jnz .c3
        inc eax
.c3:
        mov [mini_vis], eax
        mov eax, [lst_n]            ; filas en total
        add eax, [mini_cols]
        dec eax
        xor edx, edx
        div dword [mini_cols]
        sub eax, [mini_vis]
        jns .c4
        xor eax, eax
.c4:
        mov ebx, eax                ; EBX=la primera fila más baja que vale
        mov eax, [mini_fila0]
        cmp eax, ebx
        jle .c5
        mov eax, ebx
.c5:
        test eax, eax
        jns .c6
        xor eax, eax
.c6:
        mov [mini_fila0], eax
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, BDM_PONER
        mov ecx, ebx
        mov edx, [mini_vis]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [mini_fila0]
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_PONER
        xor ecx, ecx
        mov edx, [vw]
        HV msg_enviar
.x:
        popad
        ret

; que la elegida se vea
mini_a_la_vista:
        pushad
        mov eax, [mini_sel]
        xor edx, edx
        div dword [mini_cols]       ; EAX=su fila
        cmp eax, [mini_fila0]
        jae .a
        mov [mini_fila0], eax
.a:
        mov ebx, [mini_fila0]
        add ebx, [mini_vis]
        cmp eax, ebx
        jb .b
        sub eax, [mini_vis]
        inc eax
        mov [mini_fila0], eax
.b:
        call mini_mide
        popad
        ret

; una tecla (ECX), con las miniaturas a la vista
mini_tecla:
        cmp ecx, TC_ESC
        jne .t1
        call mini_sal
        jmp hecho
.t1:
        cmp ecx, TC_INTRO
        je mini_abre
        mov eax, [mini_sel]
        mov ebx, [mini_cols]
        cmp ecx, TC_DER
        jne .t2
        inc eax
        jmp .pon
.t2:
        cmp ecx, TC_IZQ
        jne .t3
        dec eax
        jmp .pon
.t3:
        cmp ecx, TC_ABAJO
        jne .t4
        add eax, ebx
        jmp .pon
.t4:
        cmp ecx, TC_ARRIBA
        jne .t5
        sub eax, ebx
        jmp .pon
.t5:
        imul ebx, [mini_vis]
        cmp ecx, TC_AVPAG
        jne .t6
        add eax, ebx
        jmp .tope
.t6:
        cmp ecx, TC_REPAG
        jne .t7
        sub eax, ebx
        jmp .tope
.t7:
        cmp ecx, TC_INICIO
        jne .t8
        xor eax, eax
        jmp .tope
.t8:
        cmp ecx, TC_FIN
        jne hecho
        mov eax, [lst_n]
        dec eax
.tope:
        test eax, eax               ; (de página en página: hasta el final, y basta)
        jns .tp1
        xor eax, eax
.tp1:
        cmp eax, [lst_n]
        jb .pon
        mov eax, [lst_n]
        dec eax
.pon:
        cmp eax, [lst_n]            ; (una flecha que se sale no hace nada)
        jae hecho
        mov [mini_sel], eax
        call mini_a_la_vista
        call inval
        jmp hecho

; abre la elegida
mini_abre:
        mov edi, nom_tmp
        mov esi, dir_ruta
        call hvl_cad
        mov eax, [mini_sel]
        imul eax, LST_TAM
        lea esi, [lst+eax]
        call hvl_cad
        mov byte [edi], 0
        call mini_sal
        call abre_archivo
        jmp hecho

; EDX=x + y*65536 de un clic: la casilla que hay ahí pasa a ser la elegida.
; CF=1 si no hay ninguna
mini_clic:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        sub ebx, [vy]
        js .no
        sub eax, [mini_x0]
        js .no
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        cmp eax, [mini_cols]
        jae .no
        mov ecx, eax                ; ECX=columna
        mov eax, ebx
        xor edx, edx
        mov ebx, CEL_AL
        div ebx
        cmp eax, [mini_vis]
        jae .no
        add eax, [mini_fila0]
        mul dword [mini_cols]
        add eax, ecx
        cmp eax, [lst_n]
        jae .no
        mov [mini_sel], eax
        call inval
        clc
        ret
.no:
        stc
        ret

; la rueda o la barra: EAX=la primera fila que se quiere ver
mini_desplaza:
        mov [mini_fila0], eax
        call mini_mide
        jmp inval

; -----------------------------------------------------------------------------
;  Pintar la rejilla (con el contexto abierto)
; -----------------------------------------------------------------------------
mini_pinta:
        pushad
        COLOR C_CREMA
        xor eax, eax
        mov ebx, [vy]
        mov ecx, [vw]
        mov edx, [vh]
        HV dc_rect
        mov eax, [mini_fila0]
        mul dword [mini_cols]
        mov ebp, eax                ; EBP=la imagen
        xor edi, edi                ; EDI=fila de la rejilla
.fila:
        cmp edi, [mini_vis]
        jae .x
        xor esi, esi                ; ESI=columna
.col:
        cmp esi, [mini_cols]
        jae .f9
        cmp ebp, [lst_n]
        jae .x
        mov eax, esi
        imul eax, CEL_AN
        add eax, [mini_x0]
        mov ebx, edi
        imul ebx, CEL_AL
        add ebx, [vy]
        call mini_celda
        inc ebp
        inc esi
        jmp .col
.f9:
        inc edi
        jmp .fila
.x:
        popad
        ret

; (interna) la casilla de la imagen EBP, en EAX, EBX
mini_celda:
        pushad
        cmp ebp, [mini_sel]
        jne .c1
        COLOR C_PETROLEO            ; la elegida, sobre petróleo
        mov ecx, CEL_AN
        mov edx, CEL_AL
        HV dc_rect
.c1:
        movzx ecx, byte [mini_est+ebp]
        cmp ecx, ME_HECHA
        jne .sin
        push eax
        push ebx
        movzx ecx, byte [mini_ta+ebp]
        movzx edx, byte [mini_tb+ebp]
        mov esi, MINI_AN
        sub esi, ecx
        shr esi, 1
        lea eax, [eax+esi+8]
        mov esi, MINI_AL
        sub esi, edx
        shr esi, 1
        lea ebx, [ebx+esi+4]
        mov esi, [mini_ptr+ebp*4]
        HV dc_copiar
        pop ebx
        pop eax
        jmp .nombre
.sin:
        ; sin hacer (un recuadro) o que no se puede ver (y un «?»)
        push eax
        push ebx
        COLOR C_ARENA_OSC
        add eax, 8
        add ebx, 4
        mov ecx, MINI_AN
        mov edx, MINI_AL
        HV dc_marco
        cmp byte [mini_est+ebp], ME_NO
        jne .s1
        COLOR C_GRANATE
        add eax, MINI_AN / 2 - 4
        add ebx, MINI_AL / 2 - 8
        mov esi, t_mini_no
        HV dc_texto
.s1:
        pop ebx
        pop eax
.nombre:
        COLOR C_TINTA
        cmp ebp, [mini_sel]
        jne .n1
        COLOR C_CREMA
.n1:
        mov esi, ebp
        imul esi, LST_TAM
        add esi, lst
        xor ecx, ecx                ; centrado
.n2:
        cmp byte [esi+ecx], 0
        je .n3
        inc ecx
        jmp .n2
.n3:
        shl ecx, 3
        neg ecx
        add ecx, CEL_AN
        sar ecx, 1
        add eax, ecx
        add ebx, MINI_AL + 6
        HV dc_texto
        popad
        ret

; -----------------------------------------------------------------------------
;  Hacer las miniaturas, de una en una (la llama el temporizador)
; -----------------------------------------------------------------------------
mini_paso:
        pushad
        cmp byte [mini], 0
        je .x
        ; la primera sin hacer: de las que se ven; si no, de todas
        mov eax, [mini_fila0]
        mul dword [mini_cols]
        mov ebx, eax
        mov ecx, [mini_vis]
        imul ecx, [mini_cols]
        add ecx, ebx
        call .busca
        jnc .hazla
        xor ebx, ebx
        mov ecx, [lst_n]
        call .busca
        jnc .hazla
        mov ebp, [ven]              ; ya están todas
        mov ecx, ID_RELOJ_MINI
        HV tmp_quitar
        jmp .x
.hazla:
        mov [mini_i], ebx
        call mini_haz
        ; su casilla, a repintar (si se ve)
        mov eax, [mini_i]
        xor edx, edx
        div dword [mini_cols]       ; EAX=fila EDX=columna
        sub eax, [mini_fila0]
        js .x
        cmp eax, [mini_vis]
        jae .x
        imul ebx, eax, CEL_AL
        add ebx, [vy]
        imul eax, edx, CEL_AN
        add eax, [mini_x0]
        mov ecx, CEL_AN
        mov edx, CEL_AL
        mov ebp, [ven]
        HV ven_invalidar_rect
.x:
        popad
        ret
; (interna) la primera pendiente de EBX a ECX (sin llegar) -> EBX; CF=1 si no hay
.busca:
        cmp ecx, [lst_n]
        jbe .b1
        mov ecx, [lst_n]
.b1:
        cmp ebx, ecx
        jae .b_no
        cmp byte [mini_est+ebx], ME_PENDIENTE
        je .b_si
        inc ebx
        jmp .b1
.b_si:
        clc
        ret
.b_no:
        stc
        ret

; hace la miniatura [mini_i]: deja su estado en ME_HECHA o ME_NO
mini_haz:
        pushad
        mov ebp, [mini_i]
        mov byte [mini_est+ebp], ME_NO
        mov edi, mini_ruta
        mov esi, dir_ruta
        call hvl_cad
        mov esi, ebp
        imul esi, LST_TAM
        add esi, lst
        call hvl_cad
        mov byte [edi], 0
        ; ---- el archivo: abierto y reconocido como el de una imagen cualquiera ----
        mov esi, mini_ruta
        call c_abre_arch
        test eax, eax
        jnz .x
        mov ebx, 1                  ; un JPEG grande: que lo reduzca él primero
.jr:
        cmp dword [im_an], MINI_AN * 4
        jb .jr9
        cmp ebx, 3
        ja .jr9
        mov eax, ebx
        mov edx, mi_reduce
        call nuc
        jc .jr9
        inc ebx
        jmp .jr
.jr9:
        ; cómo hay que ponerla (EXIF): cuartos de vuelta a la derecha y volteo
        mov eax, [im_orienta]
        dec eax
        cmp eax, 7
        jbe .o1
        xor eax, eax
.o1:
        movzx ebx, byte [or_tabla+eax*2]
        mov [mini_rot], ebx
        movzx eax, byte [or_tabla+eax*2+1]
        mov [mini_volt], eax
        mov esi, [im_an]            ; ESI, EDI=medidas de la imagen ya derecha
        mov edi, [im_al]
        test bl, 1
        jz .o2
        xchg esi, edi
.o2:
        ; la miniatura: que quepa en 96x80 sin deformarse (y sin agrandarse)
        mov eax, esi
        imul eax, MINI_AL
        mov ecx, edi
        imul ecx, MINI_AN
        cmp eax, ecx
        jb .alta
        mov eax, esi                ; manda el ancho
        cmp eax, MINI_AN
        jbe .a1
        mov eax, MINI_AN
.a1:
        mov [mini_tan], eax
        mul edi
        div esi
        jmp .a3
.alta:
        mov eax, edi                ; manda el alto
        cmp eax, MINI_AL
        jbe .a2
        mov eax, MINI_AL
.a2:
        mov [mini_tal], eax
        mul esi
        div edi
        test eax, eax
        jnz .a2b
        inc eax
.a2b:
        mov [mini_tan], eax
        jmp .a4
.a3:
        test eax, eax
        jnz .a3b
        inc eax
.a3b:
        mov [mini_tal], eax
.a4:
        mov eax, [mini_tan]         ; como sale del archivo: tumbada, si hay que girarla
        mov ecx, [mini_tal]
        test byte [mini_rot], 1
        jz .a5
        xchg eax, ecx
.a5:
        mov [mini_pan], eax
        mov [mini_pal], ecx
        ; ---- la memoria de trabajo: la del formato, los errores, el reductor,
        ;      una fila y la miniatura sin girar ----
        mov ecx, [mini_pan]
        add ecx, 2
        imul ecx, 12
        mov eax, [im_an]
        lea eax, [eax+eax*2]
        lea ecx, [ecx+eax*2]
        mov eax, [mini_pan]
        shl eax, 4
        lea ecx, [ecx+eax+256]
        mov eax, [mini_pan]
        imul eax, [mini_pal]
        lea ecx, [ecx+eax+16]
        add ecx, [mi_mem]
        jc .suelta_a
        HV mem_bloque
        jc .suelta_a
        mov [mini_th], edx
        mov edi, eax
        mov edx, mi_prepara
        call nuc
        test eax, eax
        jnz .suelta_t
        add edi, [mi_mem]
        add edi, 3
        and edi, ~3
        mov ecx, [mini_pan]
        call ap_empieza
        mov eax, [trama]
        mov [ap_modo], eax
        lea eax, [ecx+2]
        imul eax, 12
        add edi, eax
        mov eax, [im_an]
        mov ebx, [im_al]
        mov edx, [mini_pal]
        mov esi, mi_fila
        call red_prepara
        lea eax, [eax+eax*2]
        add edi, eax
        mov eax, [mini_pan]
        shl eax, 4
        lea edi, [edi+eax+36]
        and edi, ~3
        mov [mini_fila], edi
        mov eax, [mini_pan]
        lea eax, [eax+eax*2]
        lea edi, [edi+eax+8]
        mov [mini_tmp], edi
        ; ---- las filas ----
        mov dword [mini_y], 0
.fila:
        mov eax, [mini_y]
        cmp eax, [mini_pal]
        jae .hecha
        mov edx, mini_una_fila
        call nuc
        jc .corta
        inc dword [mini_y]
        jmp .fila
.corta:
        cmp dword [mini_y], 0       ; ni una fila: no se puede ver
        je .suelta_t
        mov eax, [mini_pal]         ; cortada: lo que falta, liso
        sub eax, [mini_y]
        mul dword [mini_pan]
        mov ecx, eax
        mov eax, [mini_y]
        mul dword [mini_pan]
        mov edi, [mini_tmp]
        add edi, eax
        mov al, C_PIEDRA
        rep stosb
.hecha:
        ; ---- a su sitio, ya derecha ----
        mov byte [mini_est+ebp], ME_SIN_SITIO
        mov ecx, [mini_tan]
        imul ecx, [mini_tal]
        add ecx, 3
        and ecx, ~3
        call mini_pide
        jc .suelta_t
        mov [mini_ptr+ebp*4], eax
        mov edi, eax
        call mini_endereza
        mov al, [mini_tan]
        mov [mini_ta+ebp], al
        mov al, [mini_tal]
        mov [mini_tb+ebp], al
        mov byte [mini_est+ebp], ME_HECHA
.suelta_t:
        mov edx, [mini_th]
        HV mem_bloque_soltar
.suelta_a:
        call c_suelta_arch
.x:
        popad
        ret

; (interna) la miniatura sin girar ([mini_tmp], de [mini_pan] x [mini_pal]),
; a EDI ([mini_tan] x [mini_tal]), girada [mini_rot] cuartos de vuelta a la
; derecha y, después, volteada en horizontal si [mini_volt]: de cada punto
; del destino, el que le toca del origen
mini_endereza:
        pushad
        xor ebp, ebp                ; EBP=y
.fy:
        cmp ebp, [mini_tal]
        jae .x
        xor ebx, ebx                ; EBX=x
.fx:
        cmp ebx, [mini_tan]
        jae .f9
        mov eax, ebx                ; EAX=x antes de voltear
        cmp dword [mini_volt], 0
        je .v
        mov eax, [mini_tan]
        dec eax
        sub eax, ebx
.v:
        mov edx, [mini_rot]         ; ECX, ESI=columna y fila del origen
        cmp edx, 1
        jb .r0
        je .r1
        cmp edx, 2
        je .r2
        mov ecx, [mini_pan]         ; a la izquierda: la columna «ancho-1-y», de arriba abajo
        dec ecx
        sub ecx, ebp
        mov esi, eax
        jmp .pon
.r0:
        mov ecx, eax
        mov esi, ebp
        jmp .pon
.r1:
        mov ecx, ebp                ; a la derecha: la columna «y», de abajo arriba
        mov esi, [mini_pal]
        dec esi
        sub esi, eax
        jmp .pon
.r2:
        mov ecx, [mini_pan]         ; media vuelta
        dec ecx
        sub ecx, eax
        mov esi, [mini_pal]
        dec esi
        sub esi, ebp
.pon:
        imul esi, [mini_pan]
        add esi, ecx
        add esi, [mini_tmp]
        mov al, [esi]
        stosb
        inc ebx
        jmp .fx
.f9:
        inc ebp
        jmp .fy
.x:
        popad
        ret

; (interna, con la pila del núcleo) la fila EAX de la miniatura
mini_una_fila:
        pushad
        mov edi, [mini_fila]
        call red_fila
        jc .falta
        mov ecx, [mini_pan]
        mul ecx
        mov edi, [mini_tmp]
        add edi, eax
        mov esi, [mini_fila]
        mov eax, [mini_y]
        call ap_fila
        popad
        clc
        ret
.falta:
        popad
        stc
        ret
