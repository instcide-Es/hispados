; =============================================================================
;  MIRADOR.HVP - abrir un archivo y hacer su mapa de puntos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Del archivo se lee primero la cabecera (4 K) y se le enseña al núcleo
;  (nucleo.inc). Si es un PNG, o un BMP o un TGA sin comprimir, el archivo se
;  queda abierto y el formato va leyendo de él lo que necesita para cada fila
;  («c_lector»): no llega a estar entero en memoria, así que puede ser mucho
;  mayor que la memoria que hay. Los demás se leen enteros a un bloque. El
;  núcleo dice cuánta memoria de trabajo quiere; las filas salen de una en
;  una, en rojo, verde y azul, y cada una se pasa a la paleta (apaleta.inc)
;  en el «mapa», que es lo que se queda. Si la imagen es mayor que el tope (o
;  no hay memoria para su mapa), se reduce al vuelo (reduce.inc, o el propio
;  descodificador si es un JPEG). Si sobra memoria se guarda también el
;  original en color verdadero.
; =============================================================================
%macro CARGA_BSS 0
            alignb 4
arch_p      resd 1                  ; el archivo
arch_h      resd 1
arch_tam    resd 1
trab_p      resd 1                  ; la memoria de trabajo
trab_h      resd 1
n_mapa_p    resd 1                  ; el mapa que se está haciendo
n_mapa_h    resd 1
n_orig_p    resd 1
n_orig_h    resd 1
c_fila      resd 1                  ; una fila en rojo, verde y azul
c_err       resd 1
c_k         resd 1                  ; reducción que hace el descodificador (JPEG): 1/2, 1/4, 1/8
c_r         resd 1                  ; y la que se hace aquí
c_an        resd 1                  ; medidas del mapa nuevo
c_al        resd 1
c_y         resd 1
c_f_an      resd 1                  ; medidas de la imagen del archivo
c_f_al      resd 1
hay_arch    resb 1
hay_man     resb 1                  ; el archivo sigue abierto (se lee según hace falta)
hay_trab    resb 1
hay_n_mapa  resb 1
hay_n_orig  resb 1
c_directo   resb 1                  ; de paleta a paleta, color a color y sin tramar
c_cortada   resb 1
            alignb 4
f_orienta   resd 1
f_camara    resb 68
f_tomada    resb 24
pal_mapa    resb 256                ; cada color de la paleta de la imagen, en la nuestra
err_buf     resb 256
c_man       resd 1                  ; el manejador del archivo abierto
c_pos       resd 1                  ; por dónde va su puntero (-1: no se sabe)
c_cab_n     resd 1                  ; octetos de cabecera leídos
c_cab       resb MI_CAB             ; la cabecera
%endmacro

t_no_abrir  db "No se ha podido abrir el archivo.", 10
            db "Compruebe que el nombre está bien escrito y que el disco se lee.", 0
t_no_leer   db "No se ha podido leer el archivo entero: el disco da error.", 0
t_e_vacio   db "El archivo está vacío.", 0
t_e_grande  db "El archivo es demasiado grande para Mirador:", 10
            db "pasa de 1 GB, o de 64 MB y no es un PNG, un BMP o un TGA sin comprimir.", 0
t_e_memoria db "No hay memoria bastante para ver esta imagen.", 10
            db "Cierre otros programas y pruebe otra vez.", 0
t_e_descon  db "Este archivo no es una imagen de las que Mirador conoce:", 10
            db "BMP, PCX, GIF, PNG, TGA, ICO, PBM, PGM, PPM y JPEG.", 0
t_e_roto    db "La imagen está dañada y no se puede ver nada de ella.", 10
            db "Pida a quien se la dio que se la envíe otra vez.", 0
t_e_medidas db "La imagen dice tener unas medidas imposibles", 10
            db "(más de 32.767 puntos de lado o de 64 millones en total).", 0
t_e_var1    db "Mirador no sabe ver esta imagen: es ", 10, 0
t_e_var2    db ".", 0

; la orientación de EXIF (de 1 a 8): cuartos de vuelta a la derecha y, después,
; si se voltea en horizontal
or_tabla    db 0, 0,  0, 1,  2, 0,  2, 1,  1, 1,  1, 0,  3, 1,  3, 0

abrir:
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .x
        cmp byte [nom_tmp], 0
        je .x
        call abre_archivo
.x:
        ret

; nom_tmp (como lo ha escrito el usuario) -> «ruta»: con su unidad y su
; carpeta, y en mayúsculas
pon_ruta:
        pushad
        mov esi, nom_tmp
        mov edi, ruta
        cmp byte [esi], 0
        je .copia
        cmp byte [esi+1], ':'
        je .copia
        HV dos_dir_actual           ; «A:\CARPETA»
        cmp byte [esi], '\'
        jne .rel
        mov edi, ruta + 2           ; desde la raíz: solo hace falta la unidad
        jmp .copia
.rel:
        cmp byte [edi], 0
        je .r1
        inc edi
        jmp .rel
.r1:
        cmp byte [edi-1], '\'
        je .copia
        mov byte [edi], '\'
        inc edi
.copia:
        lodsb
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 0x20
.c1:
        cmp edi, ruta + 127
        jae .c2
        stosb
        test al, al
        jnz .copia
.c2:
        mov byte [edi], 0
        popad
        ret

; abre la imagen nom_tmp. Si no se puede, se queda como «la de ahora» con su
; porqué a la vista (nada de un diálogo: pasando imágenes estorbaría)
abre_archivo:
        pushad
        call confirma_retoque       ; la de ahora, retocada y sin guardar: se pregunta
        jc .x
        call pon_ruta
        call mini_sal
        mov dword [or_rot], 0       ; una imagen nueva: derecha
        mov dword [or_volt], 0
        mov esi, t_abriendo
        call pie_dice
        call carga                  ; EAX=0, o el texto del error
        cmp eax, t_e_memoria        ; sin memoria: las miniaturas se sueltan, y otra vez
        jne .leida
        cmp byte [mini_hay], 0
        je .leida
        call mini_suelta
        call carga
.leida:
        mov [err_txt], eax
        test eax, eax               ; una foto que dice cómo hay que girarla: se gira sola
        jnz .derecha
        mov eax, [f_orienta]
        dec eax
        cmp eax, 7
        ja .derecha
        movzx ebx, byte [or_tabla+eax*2]
        mov [or_rot], ebx
        movzx ebx, byte [or_tabla+eax*2+1]
        mov [or_volt], ebx
        call tr_repite
.derecha:
        mov esi, ruta
        mov edi, nombre
        mov ecx, 132
        rep movsb
        call lista_carpeta          ; (y pone nom_corto)
        xor eax, eax
        mov [sx], eax
        mov [sy], eax
        mov byte [zajuste], 1
        mov dword [zoom], 0x10000
        call pon_marcas
        call pon_titulo
        call dispone
        call inval
.x:
        popad
        ret

; la misma imagen, leída otra vez (han cambiado los colores o el fondo), y
; puesta como estaba de girada
recarga:
        pushad
        cmp byte [nombre], 0
        je .x
        mov esi, nombre
        mov edi, ruta
        mov ecx, 132
        rep movsb
        mov esi, t_abriendo
        call pie_dice
        call carga
        mov [err_txt], eax
        test eax, eax
        jnz .r1
        call tr_repite              ; girada y volteada como estaba
.r1:
        call dispone
        call inval
.x:
        popad
        ret

; suelta la imagen que haya
suelta_imagen:
        pushad
        cmp byte [hay_img], 0
        je .v
        mov edx, [mapa_h]
        HV mem_bloque_soltar
        mov byte [hay_img], 0
.v:
        cmp byte [hay_orig], 0
        je .w
        mov edx, [orig_h]
        HV mem_bloque_soltar
        mov byte [hay_orig], 0
.w:
        call an_olvida              ; (los cuadros de una animación son el bloque del mapa)
        call des_suelta             ; y lo que hubiera para deshacer
        mov byte [retocada], 0
        call el_sal
        call suelta_vista
        call suelta_vbuf
        mov dword [av_txt], 0
        mov byte [sucio], 1
        popad
        ret
suelta_vista:
        cmp byte [hay_vista], 0
        je .x
        pushad
        mov edx, [vista_h]
        HV mem_bloque_soltar
        popad
        mov byte [hay_vista], 0
.x:
        ret

; (interna) las medidas del mapa con las reducciones de ahora: EAX=ancho
; EDX=alto, y en c_an y c_al
c_medidas:
        mov eax, [im_al]
        xor edx, edx
        div dword [c_r]
        test eax, eax
        jnz .a
        inc eax
.a:
        mov [c_al], eax
        mov eax, [im_an]
        xor edx, edx
        div dword [c_r]
        test eax, eax
        jnz .b
        inc eax
.b:
        mov [c_an], eax
        mov edx, [c_al]
        ret

; (interna) hace la imagen más pequeña: primero, que la reduzca su
; descodificador (JPEG); si no sabe o ya no da más, aquí, a la mitad
c_reduce_mas:
        cmp dword [c_r], 1
        jne .aqui
        mov eax, [c_k]
        cmp eax, 3
        jae .aqui
        inc eax
        mov edx, mi_reduce
        call nuc
        jc .aqui
        inc dword [c_k]
        ret
.aqui:
        shl dword [c_r], 1
        ret

; lee la imagen «ruta» y hace su mapa -> EAX=0, o el texto que dice por qué
; no. La imagen de antes se suelta lo primero (así toda la memoria es para la
; nueva); si no se puede leer, no queda ninguna
carga:
        pushad
        call suelta_imagen
        xor eax, eax
        mov [hay_arch], al
        mov [hay_trab], al
        mov [hay_n_mapa], al
        mov [hay_n_orig], al
        mov [c_cortada], al
        mov [c_k], eax
        mov dword [c_r], 1
        mov [hay_man], al
        mov dword [c_err], t_no_abrir
        mov esi, ruta
        call c_abre_arch            ; EAX=lo que dice el núcleo (-1: no se puede leer)
        cmp eax, -1
        je .mal
        test eax, eax
        jnz .mal_formato
        mov eax, [im_an]
        mov [c_f_an], eax
        mov eax, [im_al]
        mov [c_f_al], eax
        ; ---- a qué tamaño: el mayor que no pase del tope y para el que haya memoria ----
.medidas:
        call c_medidas
        mul edx
        cmp eax, TOPE_PUNTOS
        jbe .mapa
        call c_reduce_mas
        jmp .medidas
.mapa:
        mov ecx, eax
        add ecx, 64
        HV mem_bloque
        jnc .hay_mapa
.sin_mem:
        mov dword [c_err], t_e_memoria
        call c_medidas              ; no cabe: más pequeña, hasta que no merezca la pena
        mul edx
        cmp eax, 64 * 64
        jb .mal
        call c_reduce_mas
        jmp .medidas
.hay_mapa:
        mov [n_mapa_p], eax
        mov [n_mapa_h], edx
        ; con el mapa hecho tiene que quedar sitio para enseñarlo (el mapa de
        ; ampliar, del tamaño de la pantalla): si no, más pequeña
        mov ecx, [vb_an]
        imul ecx, [vb_al]
        add ecx, 4096
        HV mem_bloque
        jnc .cabe_ver
        mov edx, [n_mapa_h]
        HV mem_bloque_soltar
        jmp .sin_mem
.cabe_ver:
        HV mem_bloque_soltar
        mov byte [hay_n_mapa], 1
        ; la memoria de trabajo: la del formato, los errores de la difusión,
        ; la del reductor y una fila
        mov ecx, [c_an]
        lea ecx, [ecx+2]
        imul ecx, 12                ; AP_MEM
        mov eax, [im_an]
        lea eax, [eax+eax*2]
        add ecx, eax
        mov eax, [c_an]
        shl eax, 4
        add ecx, eax
        add ecx, 32                 ; RED_MEM
        mov eax, [im_an]            ; la fila (del ancho del archivo, por si no se reduce)
        lea eax, [eax+eax*2]
        add ecx, eax
        add ecx, 128
        add ecx, [mi_mem]
        jc .sin_trab
        HV mem_bloque
        jnc .hay_trab
.sin_trab:
        mov edx, [n_mapa_h]         ; sin sitio para trabajar: con la imagen más pequeña,
        HV mem_bloque_soltar        ; un JPEG pide menos (los demás, lo mismo)
        mov byte [hay_n_mapa], 0
        cmp dword [c_k], 3
        jae .mal_mem
        cmp dword [c_r], 1
        jne .mal_mem
        mov eax, [c_k]
        inc eax
        mov edx, mi_reduce
        call nuc
        jc .mal_mem
        inc dword [c_k]
        jmp .medidas
.mal_mem:
        mov dword [c_err], t_e_memoria
        jmp .mal
.hay_trab:
        mov [trab_p], eax
        mov [trab_h], edx
        mov byte [hay_trab], 1
        mov edi, eax
        mov edx, mi_prepara
        call nuc
        test eax, eax
        jnz .mal_formato
        ; el original en color verdadero, si cabe y aún queda sitio de sobra
        mov eax, [c_an]
        mul dword [c_al]
        lea ecx, [eax+eax*2]
        cmp ecx, ORIG_MAX
        ja .sin_orig
        add ecx, 64
        HV mem_bloque
        jc .sin_orig
        mov [n_orig_p], eax
        mov [n_orig_h], edx
        mov eax, [c_an]             ; ¿queda para una vista reducida y para girarla?
        mul dword [c_al]
        lea ecx, [eax+RESERVA]
        HV mem_bloque
        jnc .hay_orig
        mov edx, [n_orig_h]
        HV mem_bloque_soltar
        jmp .sin_orig
.hay_orig:
        HV mem_bloque_soltar
        mov byte [hay_n_orig], 1
.sin_orig:
        ; el reparto de la memoria de trabajo
        mov edi, [trab_p]
        add edi, [mi_mem]
        add edi, 3
        and edi, ~3
        mov ecx, [c_an]
        push edi
        call ap_empieza
        pop edi
        lea eax, [ecx+2]
        imul eax, 12
        add edi, eax
        mov eax, [trama]
        mov [ap_modo], eax
        cmp dword [c_r], 1
        je .sin_red
        mov eax, [im_an]
        mov ebx, [im_al]
        mov ecx, [c_an]
        mov edx, [c_al]
        mov esi, mi_fila
        call red_prepara
.sin_red:
        mov eax, [im_an]
        lea eax, [eax+eax*2]
        add edi, eax
        mov eax, [c_an]
        shl eax, 4
        lea edi, [edi+eax+36]
        and edi, ~3
        mov [c_fila], edi
        call c_ve_directo
        ; ---- las filas ----
        mov dword [c_y], 0
.fila:
        mov eax, [c_y]
        cmp eax, [c_al]
        jae .hecha
        mov edx, c_una_fila
        call nuc
        jc .cortada
        inc dword [c_y]
        jmp .fila
.cortada:
        mov byte [c_cortada], 1     ; lo que falta, de un color liso
        mov eax, [c_al]
        sub eax, [c_y]
        mov ebx, eax                ; EBX=filas que faltan
        mul dword [c_an]
        mov ecx, eax
        mov eax, [c_y]
        mul dword [c_an]
        mov edi, [n_mapa_p]
        add edi, eax
        push eax
        mov al, C_PIEDRA
        rep stosb
        pop eax
        cmp byte [hay_n_orig], 0
        je .hecha
        lea edi, [eax+eax*2]
        add edi, [n_orig_p]
        mov eax, ebx
        mul dword [c_an]
        mov ecx, eax
.c1:
        mov word [edi], 0xD2D8      ; (el color «piedra» de la paleta)
        mov byte [edi+2], 0xC4
        add edi, 3
        dec ecx
        jnz .c1
.hecha:
        cmp dword [c_y], 0          ; ni una fila: no hay imagen
        jne .vale
        mov dword [c_err], t_e_roto
        jmp .mal
.vale:
        ; ---- ya está: la de antes se suelta y queda esta ----
        call suelta_imagen
        mov eax, [n_mapa_p]
        mov [mapa_p], eax
        mov eax, [n_mapa_h]
        mov [mapa_h], eax
        mov byte [hay_img], 1
        mov byte [hay_n_mapa], 0
        cmp byte [hay_n_orig], 0
        je .v1
        mov eax, [n_orig_p]
        mov [orig_p], eax
        mov eax, [n_orig_h]
        mov [orig_h], eax
        mov byte [hay_orig], 1
        mov byte [hay_n_orig], 0
.v1:
        mov eax, [c_an]
        mov [ma_an], eax
        mov eax, [c_al]
        mov [ma_al], eax
        mov eax, [c_f_an]
        mov [g_an], eax
        mov eax, [c_f_al]
        mov [g_al], eax
        mov eax, [c_r]
        mov ecx, [c_k]
        shl eax, cl
        mov [f_red], eax
        mov eax, [im_clase]
        mov [f_clase], eax
        mov eax, [im_bits]
        mov [f_bits], eax
        mov eax, [im_ncol]
        mov [f_ncol], eax
        mov eax, [im_extra]
        mov [f_extra], eax
        mov eax, [im_orienta]
        mov [f_orienta], eax
        mov edi, f_camara           ; (los textos se copian: el núcleo se usa también
        mov esi, [im_camara]        ; para las miniaturas)
        call .copia
        mov edi, f_tomada
        mov esi, [im_tomada]
        call .copia
        mov eax, [im_fmt]
        mov eax, [eax+F_NOMBRE]
        mov [f_fmt], eax
        mov eax, [arch_tam]
        mov [f_tam], eax
        mov al, [im_hay_alfa]
        mov [f_alfa], al
        mov al, [im_aviso]
        or al, [c_cortada]
        mov [f_aviso], al
        mov dword [av_txt], 0
        test al, al
        jz .v2
        mov dword [av_txt], t_av_cortado
.v2:
        cmp byte [hay_trab], 0      ; (la memoria de trabajo ya no hace falta)
        je .v3
        mov edx, [trab_h]
        HV mem_bloque_soltar
        mov byte [hay_trab], 0
.v3:
        call an_carga               ; una animación: sus cuadros, ahora que el archivo está aquí
        mov dword [c_err], 0
        jmp .limpia
.mal_formato:
        ; EAX=ME_*: lo que dice el núcleo, en palabras
        mov dword [c_err], t_e_descon
        cmp eax, ME_DESCONOCIDO
        je .mal
        mov dword [c_err], t_e_medidas
        cmp eax, ME_GRANDE
        je .mal
        mov dword [c_err], t_e_roto
        cmp eax, ME_VARIANTE
        jne .mal
        cmp dword [im_var], 0
        je .mal
        mov edi, err_buf
        mov esi, t_e_var1
        call hvl_cad
        mov esi, [im_var]
        call hvl_cad
        mov esi, t_e_var2
        call hvl_cad
        mov byte [edi], 0
        mov dword [c_err], err_buf
        jmp .mal
.mal:
        call suelta_imagen
.limpia:
        cmp byte [hay_n_orig], 0
        je .l2
        mov edx, [n_orig_h]
        HV mem_bloque_soltar
.l2:
        cmp byte [hay_n_mapa], 0
        je .l3
        mov edx, [n_mapa_h]
        HV mem_bloque_soltar
.l3:
        cmp byte [hay_trab], 0
        je .l4
        mov edx, [trab_h]
        HV mem_bloque_soltar
.l4:
        call c_suelta_arch
        mov eax, [c_err]
        mov [esp+28], eax
        popad
        ret

; (interna) el texto ESI (0: ninguno), a EDI
.copia:
        mov byte [edi], 0
        test esi, esi
        jz .cp9
.cp1:
        lodsb
        stosb
        test al, al
        jnz .cp1
.cp9:
        ret

; -----------------------------------------------------------------------------
;  El archivo: abrirlo, reconocerlo y leer de él
; -----------------------------------------------------------------------------
DISCO_MAX   equ 0x40000000          ; un archivo mayor no se intenta ni leyéndolo a trozos

; ESI=la ruta: abre el archivo y se lo enseña al núcleo -> EAX=lo que dice
; (ME_*; 0: se puede seguir con mi_prepara y las filas), o -1 si el archivo
; no se puede leer ([c_err] dice por qué). Con 0, o el archivo se queda
; abierto ([hay_man]) o está entero en un bloque ([hay_arch]): al acabar,
; c_suelta_arch. Lo usan la imagen y las miniaturas
c_abre_arch:
        pushad
        mov byte [hay_arch], 0
        mov byte [hay_man], 0
        xor al, al
        HV dos_abrir
        jc .mal
        mov [c_man], eax
        mov byte [hay_man], 1
        mov ebx, eax                ; EBX=el manejador
        mov dword [c_err], t_no_leer
        mov al, 2                   ; cuánto ocupa: el puntero, al final...
        xor edx, edx
        HV dos_puntero
        jc .mal
        mov [arch_tam], eax
        xor al, al                  ; ...y otra vez al principio
        xor edx, edx
        HV dos_puntero
        jc .mal
        mov dword [c_err], t_e_vacio
        mov ecx, [arch_tam]
        test ecx, ecx
        jz .mal
        mov dword [c_err], t_e_grande
        cmp ecx, DISCO_MAX
        ja .mal
        ; ---- la cabecera, y qué dice el núcleo de ella ----
        mov dword [c_err], t_no_leer
        cmp ecx, MI_CAB
        jbe .c1
        mov ecx, MI_CAB
.c1:
        mov edi, c_cab
        HV dos_leer
        jc .mal
        cmp eax, ecx
        jne .mal                    ; (se acaba antes de lo que dijo)
        mov [c_cab_n], eax
        mov [c_pos], eax
        movzx eax, byte [ajedrez]
        xor eax, 1
        mov [mi_fondo], eax
        mov dword [mi_fondo_rgb], 0x00FFFFFF
        mov dword [mi_lector], c_lector
        mov esi, c_cab
        mov ecx, [c_cab_n]
        mov edx, c_abre_d
        call nuc
        cmp eax, ME_ENTERO
        je .entero
        test eax, eax               ; (lo ve leyendo del disco: se queda abierto; o no vale)
        jz .dev
        call c_suelta_arch
        jmp .dev
.entero:
        ; ---- hay que tenerlo entero en memoria ----
        mov dword [c_err], t_e_grande
        mov ecx, [arch_tam]
        cmp ecx, ARCH_MAX
        ja .mal
        mov dword [c_err], t_e_memoria
        add ecx, 64
        HV mem_bloque
        jc .mal
        mov [arch_p], eax
        mov [arch_h], edx
        mov byte [hay_arch], 1
        mov dword [c_err], t_no_leer
        mov edi, eax                ; lo que ya se ha leído, y el resto
        mov esi, c_cab
        mov ecx, [c_cab_n]
        rep movsb
        mov ebx, [c_man]
        xor al, al
        mov edx, [c_cab_n]
        HV dos_puntero
        jc .mal
.lee:
        mov ecx, [arch_p]
        add ecx, [arch_tam]
        sub ecx, edi
        jz .leido
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .mal
        test eax, eax
        jz .mal                     ; (se acaba antes de lo que dijo)
        add edi, eax
        jmp .lee
.leido:
        call c_cierra
        mov esi, [arch_p]
        mov ecx, [arch_tam]
        mov edx, mi_abre
        call nuc
        test eax, eax
        jz .dev
        call c_suelta_arch
.dev:
        mov [esp+28], eax
        popad
        ret
.mal:
        call c_suelta_arch
        mov dword [esp+28], -1
        popad
        ret

; (interna, la llama «nuc») mi_abre_d, con el tamaño del archivo en EDX
c_abre_d:
        mov edx, [arch_tam]
        jmp mi_abre_d

; (interna) cierra el archivo, si sigue abierto. Conserva los registros
c_cierra:
        cmp byte [hay_man], 0
        je .x
        mov byte [hay_man], 0
        push ebx
        mov ebx, [c_man]
        HV dos_cerrar
        pop ebx
.x:
        ret

; cierra el archivo y suelta su bloque, lo que haya. Conserva los registros
c_suelta_arch:
        call c_cierra
        cmp byte [hay_arch], 0
        je .x
        mov byte [hay_arch], 0
        pushad
        mov edx, [arch_h]
        HV mem_bloque_soltar
        popad
.x:
        ret

; lo que el núcleo llama para leer del archivo abierto: EAX=posición
; EDI=destino ECX=cuántos -> EAX=los leídos
c_lector:
        push ebx
        push edx
        mov ebx, [c_man]
        cmp eax, [c_pos]            ; (si ya está ahí, no se mueve el puntero)
        je .l
        mov edx, eax
        xor al, al
        HV dos_puntero
        jc .mal
        mov [c_pos], eax
.l:
        HV dos_leer
        jc .mal
        add [c_pos], eax
        pop edx
        pop ebx
        ret
.mal:
        mov dword [c_pos], -1       ; (a saber dónde ha quedado)
        xor eax, eax
        pop edx
        pop ebx
        ret

; (interna, con la pila del núcleo) la fila EAX del mapa nuevo: se pide al
; formato (reducida, si toca), se guarda en el original y se pasa a la
; paleta. CF=1 si el archivo no da para más
c_una_fila:
        pushad
        mov edi, [c_fila]
        cmp dword [c_r], 1
        je .tal
        call red_fila
        jmp .f1
.tal:
        call mi_fila
.f1:
        jc .falta
        cmp byte [hay_n_orig], 0
        je .f2
        mov eax, [c_y]
        mul dword [c_an]
        lea edi, [eax+eax*2]
        add edi, [n_orig_p]
        mov esi, [c_fila]
        mov ecx, [c_an]
        lea ecx, [ecx+ecx*2]
        rep movsb
.f2:
        mov eax, [c_y]
        mul dword [c_an]
        mov edi, [n_mapa_p]
        add edi, eax
        mov ecx, [c_an]
        cmp byte [c_directo], 0
        je .trama
        mov esi, [mi_cruda]         ; los índices de la imagen, a los nuestros
        xor eax, eax
.d1:
        lodsb
        mov al, [pal_mapa+eax]
        stosb
        dec ecx
        jnz .d1
        jmp .bien
.trama:
        mov esi, [c_fila]
        mov eax, [c_y]
        call ap_fila
.bien:
        popad
        clc
        ret
.falta:
        popad
        stc
        ret

; (interna) ¿va de paleta a paleta, color a color? Sí, si la imagen es de
; paleta, sin transparencia ni reducción, y tiene 16 colores o menos, o todos
; sus colores están (o casi) en la paleta de Hispa Ventanas, o se ha pedido
; «el más cercano». Deja hecha la tabla pal_mapa
c_ve_directo:
        pushad
        mov byte [c_directo], 0
        cmp dword [im_clase], IC_PALETA
        jne .x
        cmp dword [im_hay_alfa], 0
        jne .x
        cmp dword [c_r], 1
        jne .x
        mov ebp, 1                  ; EBP=1 mientras todos estén en la nuestra
        xor ebx, ebx
.c:
        mov eax, [im_pal+ebx*4]
        and eax, 0x00FFFFFF
        mov edx, eax
        call ap_exacto
        mov [pal_mapa+ebx], al
        cmp ebx, [im_ncol]          ; (los que la imagen no dice tener no cuentan)
        jae .c2
        mov esi, [ap_pal]
        mov esi, [esi+eax*4]        ; ¿cuánto se parece? (5 niveles por componente, a lo más)
        mov ecx, 3
.c1:
        movzx eax, dl
        movzx edi, si
        and edi, 255
        sub eax, edi
        jns .c1b
        neg eax
.c1b:
        cmp eax, 5
        jbe .c1c
        xor ebp, ebp
.c1c:
        shr edx, 8
        shr esi, 8
        dec ecx
        jnz .c1
.c2:
        inc ebx
        cmp ebx, 256
        jb .c
        mov eax, [im_ncol]
        dec eax
        cmp eax, 16
        jb .si
        test ebp, ebp
        jnz .si
        cmp dword [trama], AP_CERCANO
        jne .x
.si:
        mov byte [c_directo], 1
.x:
        popad
        ret
