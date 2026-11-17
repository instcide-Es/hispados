; =============================================================================
;  MIRADOR.HVP - sacar la imagen: guardarla como BMP, imprimirla, su
;  información y copiar su nombre
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Guardar: un BMP de Windows sin comprimir, de 24 bits si se guarda el
;  original en color verdadero; si no (o si se pide), de 8 bits con la paleta
;  de Hispa Ventanas: lo que se ve, punto por punto. Se guarda como está
;  ahora (girada, volteada, reducida si se redujo al leerla).
;
;  Imprimir: en gráficos de ESC/P (impresoras de agujas, por el puerto
;  paralelo), como Pluma: «ESC * 5» (72 puntos por pulgada a lo ancho, 576 en
;  la hoja) y tiras de 8 agujas con el avance a 8/72, así que los puntos son
;  cuadrados. La imagen se ajusta a la hoja (576 x 760), apaisada si así sale
;  mayor, sin agrandarla más de cuatro veces; se pasa a grises y de grises a
;  punto negro o blanco repartiendo el error (Floyd-Steinberg).
; =============================================================================
%macro SALIDA_BSS 0
            alignb 4
sal_h       resd 1                  ; un bloque de quita y pon
sal_p       resd 1
sal_fila    resd 1
sal_paso    resd 1
sal_24      resd 1
sal_man     resd 1                  ; el archivo
tr_orden    resd 1
tr_paso     resd 1
im_an_p     resd 1                  ; al imprimir: medidas en el papel
im_al_p     resd 1
im_margen   resd 1
im_e0       resd 1                  ; los errores de la fila y de la de debajo
im_e1       resd 1
im_nfila    resd 1                  ; filas que hay en la tira
im_gira     resb 1
            alignb 4
ruta2       resb 132
cab_bmp     resb 56
im_tira     resb 8 * PAG_AN         ; la tira de 8 filas: 1 = punto negro
%endmacro

t_ext_bmp   db ".BMP", 0
t_guardado  db "La imagen se ha guardado.", 0
t_guardado8 db "La imagen se ha guardado con los 256 colores de Hispa Ventanas.", 0
t_no_guardar db "No se ha podido guardar la imagen.", 10
            db "Compruebe que hay sitio en el disco y que no está protegido.", 0
t_encima    db "Va a escribir encima de la imagen que está viendo.", 10
            db "¿Quiere seguir?", 0
t_impreso   db "La imagen se ha enviado a la impresora.", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_e_mem_sal db "No hay memoria bastante para hacerlo.", 10
            db "Cierre otros programas y pruebe otra vez.", 0
t_info      db "Información de la imagen", 0
t_i_archivo db "Archivo: ", 0
t_i_formato db "Formato: ", 0
t_i_medidas db "Medidas: ", 0
t_i_puntos  db " puntos", 0
t_i_colores db "Colores: ", 0
t_i_pal     db " colores, con paleta", 0
t_i_byn     db "blanco y negro", 0
t_i_grises  db "grises de ", 0
t_i_verdad  db "color verdadero de ", 0
t_i_bits    db " bits", 0
t_i_alfa    db "; con partes transparentes", 0
t_i_tam     db "Tamaño del archivo: ", 0
t_i_octetos db " octetos", 0
t_i_fecha   db "Fecha: ", 0
t_i_camara  db "Cámara: ", 0
t_i_tomada  db "Foto hecha el: ", 0
t_i_mem     db "En memoria: ", 0
t_i_256     db " puntos a 256 colores", 0
t_i_orig    db ",", 10, "y además en color verdadero (se puede guardar en 24 bits)", 0
t_i_red     db " (reducida a 1/", 0
t_i_cortada db "El archivo está cortado o dañado: falta una parte.", 0
t_coma      db ", ", 0

; ---- el nombre del archivo, al portapapeles (por un cuadro de texto oculto) ----
copiar:
        pushad
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, nombre
        HV msg_enviar
        mov eax, EM_SEL_TODO
        HV msg_enviar
        mov eax, WM_COPY
        HV msg_enviar
        popad
        ret

; =============================================================================
;  Guardar: ECX=O_GUARDAR (BMP, de 24 bits si se puede), O_GUARDAR8 (BMP de
;  256 colores), O_GUARDA_PNG u O_GUARDA_PCX (guarda.inc)
; =============================================================================
;  Una imagen retocada, una vez guardada, pasa a ser la que se ve: se abre el
;  archivo que se acaba de escribir (así lo que se ve es lo que hay en el disco)
guardar:
        call guarda_haz
        jc .x
        cmp byte [retocada], 0
        je .x
        pushad
        mov byte [retocada], 0
        mov esi, ruta2
        mov edi, nom_tmp
        mov ecx, 132
        rep movsb
        call abre_archivo
        popad
.x:
        ret
; lo mismo, sin más: CF=1 si no se ha guardado (se ha cancelado o no se ha podido)
guarda_haz:
        pushad
        mov dword [sal_24], 0
        mov dword [sal_fmt], 0
        mov dword [sal_ext], t_ext_bmp
        cmp ecx, O_GUARDAR8
        je .g0
        movzx eax, byte [hay_orig]  ; (lo demás: en color verdadero si se guarda)
        mov [sal_24], eax
        cmp ecx, O_GUARDA_PNG
        jne .g00
        mov dword [sal_fmt], 1
        mov dword [sal_ext], t_ext_png
.g00:
        cmp ecx, O_GUARDA_PCX
        jne .g0
        mov dword [sal_fmt], 2
        mov dword [sal_ext], t_ext_pcx
.g0:
        ; se propone el nombre de la imagen, con .BMP
        mov esi, [nom_corto]
        mov edi, nom_tmp
.n:
        lodsb
        test al, al
        jz .n1
        cmp al, '.'
        je .n1
        stosb
        jmp .n
.n1:
        mov esi, [sal_ext]
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_guardar
        HV dlg_archivo
        test eax, eax
        jz .x
        cmp byte [nom_tmp], 0
        je .x
        mov esi, nom_tmp            ; sin extensión: .BMP
        xor ebx, ebx
.e:
        lodsb
        test al, al
        jz .e1
        cmp al, '.'
        jne .e0
        inc ebx
.e0:
        cmp al, '\'
        jne .e
        xor ebx, ebx
        jmp .e
.e1:
        test ebx, ebx
        jnz .e2
        lea edi, [esi-1]
        cmp edi, nom_tmp + 120
        jae .e2
        mov esi, [sal_ext]
        call hvl_cad
        mov byte [edi], 0
.e2:
        ; la ruta entera (sin perder la de la imagen que se ve)
        mov esi, ruta
        mov edi, ruta2
        mov ecx, 132
        rep movsb
        call pon_ruta
        mov esi, ruta
        mov edi, ruta2
        mov ecx, 132
.cam:
        mov al, [esi]
        xchg al, [edi]
        mov [esi], al
        inc esi
        inc edi
        dec ecx
        jnz .cam
        mov esi, ruta2              ; ¿encima de la que se ve?
        mov edi, nombre
.ig:
        mov al, [esi]
        cmp al, [edi]
        jne .dist
        inc esi
        inc edi
        test al, al
        jnz .ig
        mov ebp, [ven]
        mov esi, t_encima
        mov edi, t_mirador
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .x
.dist:
        cmp dword [sal_fmt], 1
        jb .bmp
        je .png
        call escribe_pcx
        jmp .escrita
.png:
        call escribe_png
        jmp .escrita
.bmp:
        call escribe_bmp
.escrita:
        jnc .bien
        mov esi, t_no_guardar
        call aviso
.x:
        popad
        stc
        ret
.bien:
        mov byte [lst_fuerza], 1    ; la carpeta ha cambiado
        call lista_carpeta
        call inval
        mov esi, t_guardado
        cmp dword [sal_24], 0
        jne .dice
        mov esi, t_guardado8
.dice:
        call aviso
        popad
        clc
        ret

; escribe la imagen en ruta2: de 24 bits si [sal_24]; si no, de 8. CF=1 si
; no se puede
escribe_bmp:
        pushad
        ; una fila, con su relleno hasta múltiplo de cuatro
        mov eax, [ma_an]
        cmp dword [sal_24], 0
        je .p8
        lea eax, [eax+eax*2]
.p8:
        add eax, 3
        and eax, ~3
        mov [sal_paso], eax
        lea ecx, [eax+64]
        HV mem_bloque
        jc .mal0
        mov [sal_p], eax
        mov [sal_h], edx
        mov esi, ruta2
        HV dos_crear
        jc .mal1
        mov [sal_man], eax
        mov ebx, eax
        ; la cabecera
        mov edi, cab_bmp
        xor eax, eax
        mov ecx, 14
        rep stosd
        mov word [cab_bmp], 'BM'
        mov eax, [sal_paso]
        mul dword [ma_al]
        mov [cab_bmp+34], eax       ; octetos de la imagen
        mov ecx, 54
        cmp dword [sal_24], 0
        jne .c1
        add ecx, 1024
.c1:
        mov [cab_bmp+10], ecx       ; dónde empieza
        add eax, ecx
        mov [cab_bmp+2], eax        ; y los del archivo
        mov dword [cab_bmp+14], 40
        mov eax, [ma_an]
        mov [cab_bmp+18], eax
        mov eax, [ma_al]
        mov [cab_bmp+22], eax
        mov word [cab_bmp+26], 1
        mov word [cab_bmp+28], 8
        cmp dword [sal_24], 0
        je .c2
        mov word [cab_bmp+28], 24
.c2:
        mov dword [cab_bmp+38], 2835    ; 72 puntos por pulgada
        mov dword [cab_bmp+42], 2835
        mov esi, cab_bmp
        mov ecx, 54
        call .escribe
        jc .mal2
        cmp dword [sal_24], 0
        jne .filas
        mov esi, [hv_api + HV_pal_rgb * 4]  ; la paleta: azul, verde, rojo, 0
        mov edi, linea
        mov ecx, 256
.pal:
        mov eax, [esi]
        add esi, 4
        mov edx, eax
        shr edx, 16
        mov [edi], dl
        mov [edi+1], ah
        mov [edi+2], al
        mov byte [edi+3], 0
        add edi, 4
        dec ecx
        jnz .pal
        mov esi, linea
        mov ecx, 1024
        call .escribe
        jc .mal2
.filas:
        mov ebp, [ma_al]            ; de abajo arriba
.f:
        dec ebp
        js .fin
        mov edi, [sal_p]
        mov ecx, [sal_paso]
        xor al, al
        push edi
        rep stosb
        pop edi
        mov eax, ebp
        mul dword [ma_an]
        mov ecx, [ma_an]
        cmp dword [sal_24], 0
        jne .f24
        mov esi, [mapa_p]
        add esi, eax
        rep movsb
        jmp .f1
.f24:
        lea esi, [eax+eax*2]
        add esi, [orig_p]
.f24b:
        mov al, [esi+2]             ; azul, verde, rojo
        stosb
        mov al, [esi+1]
        stosb
        mov al, [esi]
        stosb
        add esi, 3
        dec ecx
        jnz .f24b
.f1:
        mov esi, [sal_p]
        mov ecx, [sal_paso]
        call .escribe
        jc .mal2
        jmp .f
.fin:
        mov ebx, [sal_man]
        HV dos_cerrar
        mov edx, [sal_h]
        HV mem_bloque_soltar
        popad
        clc
        ret
.mal2:
        mov ebx, [sal_man]
        HV dos_cerrar
        mov esi, ruta2              ; (a medias no se deja)
        HV dos_borrar
.mal1:
        mov edx, [sal_h]
        HV mem_bloque_soltar
.mal0:
        popad
        stc
        ret
; (interna) ESI=datos ECX=octetos: al archivo, a trozos de 16 K; CF=1 si no
; cabe (disco lleno) o hay error
.escribe:
        mov ebx, [sal_man]
.e1:
        test ecx, ecx
        jz .e9
        push ecx
        cmp ecx, 16384
        jbe .e2
        mov ecx, 16384
.e2:
        HV dos_escribir
        jc .e8
        cmp eax, ecx
        jb .e8
        add esi, ecx
        mov eax, ecx
        pop ecx
        sub ecx, eax
        jmp .e1
.e8:
        pop ecx
        stc
        ret
.e9:
        clc
        ret

; =============================================================================
;  Información de la imagen
; =============================================================================
; (interna) EAX, con puntos de millares, en EDI (que avanza)
num_miles:
        push eax
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor ecx, ecx
.d:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .d
.e:
        pop eax
        add al, '0'
        stosb
        dec ecx
        jz .x
        mov eax, ecx                ; un punto cada tres cifras
        xor edx, edx
        push ecx
        mov ecx, 3
        div ecx
        pop ecx
        test edx, edx
        jnz .e
        mov al, '.'
        stosb
        jmp .e
.x:
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
; (interna) EAX, con dos cifras, en EDI
num_2:
        push eax
        push edx
        xor edx, edx
        push ecx
        mov ecx, 10
        div ecx
        pop ecx
        add al, '0'
        stosb
        mov al, dl
        add al, '0'
        stosb
        pop edx
        pop eax
        ret

informacion:
        pushad
        mov edi, linea
        mov esi, t_i_archivo
        call hvl_cad
        mov esi, nombre
        call hvl_cad
        mov al, 10
        stosb
        mov esi, t_i_formato
        call hvl_cad
        mov esi, [f_fmt]
        call hvl_cad
        mov esi, [f_extra]
        test esi, esi
        jz .f1
        push esi
        mov esi, t_coma
        call hvl_cad
        pop esi
        call hvl_cad
.f1:
        mov al, 10
        stosb
        mov esi, t_i_medidas
        call hvl_cad
        mov eax, [g_an]
        call num_miles
        mov esi, t_x
        call hvl_cad
        mov eax, [g_al]
        call num_miles
        mov esi, t_i_puntos
        call hvl_cad
        mov al, 10
        stosb
        ; los colores
        mov esi, t_i_colores
        call hvl_cad
        mov eax, [f_clase]
        cmp eax, IC_PALETA
        jne .c1
        mov eax, [f_ncol]
        test eax, eax
        jnz .c0
        mov ecx, [f_bits]
        mov eax, 1
        shl eax, cl
.c0:
        call hvl_num
        mov esi, t_i_pal
        call hvl_cad
        jmp .c9
.c1:
        cmp eax, IC_GRIS
        je .c2
        cmp eax, IC_GRISA
        jne .c3
.c2:
        mov esi, t_i_byn
        cmp dword [f_bits], 1
        je .c2b
        mov esi, t_i_grises
        call hvl_cad
        mov eax, [f_bits]
        call hvl_num
        mov esi, t_i_bits
.c2b:
        call hvl_cad
        jmp .c9
.c3:
        mov esi, t_i_verdad
        call hvl_cad
        mov eax, [f_bits]
        call hvl_num
        mov esi, t_i_bits
        call hvl_cad
.c9:
        cmp byte [f_alfa], 0
        je .c10
        mov esi, t_i_alfa
        call hvl_cad
.c10:
        mov al, 10
        stosb
        mov esi, t_i_tam
        call hvl_cad
        mov eax, [f_tam]
        call num_miles
        mov esi, t_i_octetos
        call hvl_cad
        mov al, 10
        stosb
        ; la fecha: la que dio la carpeta
        mov eax, [lst_i]
        cmp eax, [lst_n]
        jae .sin_fecha
        imul eax, LST_TAM
        lea ebx, [lst+eax]
        movzx edx, word [ebx+L_FECHA]
        test edx, edx
        jz .sin_fecha
        mov esi, t_i_fecha
        call hvl_cad
        mov eax, edx
        and eax, 31
        call num_2
        mov al, '/'
        stosb
        mov eax, edx
        shr eax, 5
        and eax, 15
        call num_2
        mov al, '/'
        stosb
        mov eax, edx
        shr eax, 9
        add eax, 1980
        call hvl_num
        mov al, ' '
        stosb
        movzx edx, word [ebx+L_HORA]
        mov eax, edx
        shr eax, 11
        call num_2
        mov al, ':'
        stosb
        mov eax, edx
        shr eax, 5
        and eax, 63
        call num_2
        mov al, 10
        stosb
.sin_fecha:
        ; lo que apuntó la cámara, si lo apuntó
        cmp byte [f_camara], 0
        je .sin_camara
        mov esi, t_i_camara
        call hvl_cad
        mov esi, f_camara
        call hvl_cad
        mov al, 10
        stosb
.sin_camara:
        cmp byte [f_tomada+15], 0   ; «AAAA:MM:DD HH:MM:SS»
        je .sin_tomada
        cmp byte [f_tomada+4], ':'
        jne .sin_tomada
        mov esi, t_i_tomada
        call hvl_cad
        mov ax, [f_tomada+8]
        stosw
        mov al, '/'
        stosb
        mov ax, [f_tomada+5]
        stosw
        mov al, '/'
        stosb
        mov eax, [f_tomada]
        stosd
        mov al, ' '
        stosb
        mov eax, [f_tomada+11]
        stosd
        mov al, [f_tomada+15]
        stosb
        mov al, 10
        stosb
.sin_tomada:
        call an_info
        mov esi, t_i_mem
        call hvl_cad
        mov eax, [ma_an]
        call num_miles
        mov esi, t_x
        call hvl_cad
        mov eax, [ma_al]
        call num_miles
        mov esi, t_i_256
        call hvl_cad
        cmp dword [f_red], 1
        jbe .m1
        mov esi, t_i_red
        call hvl_cad
        mov eax, [f_red]
        call hvl_num
        mov al, ')'
        stosb
.m1:
        cmp byte [hay_orig], 0
        je .m2
        mov esi, t_i_orig
        call hvl_cad
.m2:
        cmp byte [f_aviso], 0
        je .m3
        mov al, 10
        stosb
        mov esi, t_i_cortada
        call hvl_cad
.m3:
        cmp byte [retocada], 0
        je .m4
        mov al, 10
        stosb
        mov esi, t_i_retoc
        call hvl_cad
.m4:
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, linea
        mov edi, t_info
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; =============================================================================
;  Imprimir
; =============================================================================
im_ini      db 27, '@', 27, 'A', 8          ; la impresora, como de fábrica; avance de 8/72
im_graf     db 27, '*', 5, PAG_AN & 0xFF, PAG_AN >> 8

imprimir:
        pushad
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .mal
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .mal
        test ah, 0x10
        jz .mal
        ; ---- a qué tamaño: derecha o apaisada, la que salga mayor ----
        mov eax, [ma_an]
        mov ebx, [ma_al]
        call .cabe
        mov esi, eax                ; ESI=escala derecha (16.16)
        mov eax, [ma_al]
        mov ebx, [ma_an]
        call .cabe
        mov byte [im_gira], 0
        mov ecx, [ma_an]            ; ECX, EDX=medidas de origen como se van a leer
        mov edx, [ma_al]
        cmp eax, esi
        jbe .dcha
        mov byte [im_gira], 1
        mov esi, eax
        xchg ecx, edx
.dcha:
        mov eax, ecx                ; las medidas en el papel
        push edx
        mul esi
        shrd eax, edx, 16
        pop edx
        call .entre
        cmp eax, PAG_AN
        jbe .d1
        mov eax, PAG_AN
.d1:
        mov [im_an_p], eax
        mov ebx, PAG_AN
        sub ebx, eax
        shr ebx, 1
        mov [im_margen], ebx
        mov eax, edx
        push edx
        mul esi
        shrd eax, edx, 16
        pop edx
        call .entre
        cmp eax, PAG_AL
        jbe .d2
        mov eax, PAG_AL
.d2:
        mov [im_al_p], eax
        ; ---- la memoria: el reductor, una fila en color y los errores ----
        push ecx
        push edx
        lea eax, [ecx+ecx*2]
        add eax, PAG_AN * 16 + 32 + PAG_AN * 3 + (PAG_AN + 2) * 8 + 128
        mov ecx, eax
        HV mem_bloque
        mov [sal_h], edx
        mov edi, eax
        pop edx
        pop ecx
        jnc .hay
        mov esi, t_e_mem_sal
        call aviso
        popad
        ret
.hay:
        mov eax, ecx
        mov ebx, edx
        mov ecx, [im_an_p]
        mov edx, [im_al_p]
        mov esi, fuente_fila
        cmp byte [im_gira], 0
        je .h1
        mov esi, fuente_fila_g
.h1:
        call red_prepara
        lea eax, [eax+eax*2]
        lea edi, [edi+eax+PAG_AN*16+36]
        and edi, ~3
        mov [sal_fila], edi
        add edi, PAG_AN * 3 + 4
        mov [im_e0], edi
        add edi, (PAG_AN + 2) * 4
        mov [im_e1], edi
        mov edi, [im_e0]
        mov ecx, (PAG_AN + 2) * 2
        xor eax, eax
        rep stosd
        mov esi, t_imprimiendo
        call pie_dice
        mov esi, im_ini
        mov ecx, 5
        call im_manda
        jc .mal2
        mov dword [im_nfila], 0
        call im_limpia
        xor ebx, ebx                ; EBX=la fila del papel
.fila:
        cmp ebx, [im_al_p]
        jae .fin
        mov eax, ebx
        mov edi, [sal_fila]
        call red_fila
        call im_a_puntos
        inc dword [im_nfila]
        cmp dword [im_nfila], 8
        jb .f1
        call im_la_tira
        jc .mal2
.f1:
        inc ebx
        jmp .fila
.fin:
        call im_la_tira
        jc .mal2
        mov al, 12                  ; y la hoja, fuera
        call imp_car
        jc .mal2
        call imp_lista
        jc .mal2
        mov edx, [sal_h]
        HV mem_bloque_soltar
        mov esi, t_impreso
        call aviso
        popad
        ret
.mal2:
        mov edx, [sal_h]
        HV mem_bloque_soltar
.mal:
        mov esi, t_e_imprimir
        call aviso
        popad
        ret
; (interna) EAX=ancho EBX=alto de la imagen -> EAX=la mayor escala (16.16)
; con la que cabe en la hoja, sin pasar de cuatro
.cabe:
        push ecx
        push edx
        mov ecx, eax
        mov edx, PAG_AN
        xor eax, eax
        shrd eax, edx, 16
        shr edx, 16
        div ecx
        mov ecx, eax                ; ECX=a lo ancho
        mov edx, PAG_AL
        xor eax, eax
        shrd eax, edx, 16
        shr edx, 16
        div ebx
        cmp eax, ecx
        jbe .c1
        mov eax, ecx
.c1:
        cmp eax, 0x40000
        jbe .c2
        mov eax, 0x40000
.c2:
        pop edx
        pop ecx
        ret
; (interna) un punto al menos
.entre:
        test eax, eax
        jnz .e1
        inc eax
.e1:
        ret

; (interna) la fila de color de [sal_fila], a puntos negros en la fila
; [im_nfila] de la tira: a grises, y de grises a negro o blanco repartiendo
; el error entre los vecinos
im_a_puntos:
        pushad
        mov esi, [sal_fila]
        mov edi, [im_nfila]
        imul edi, PAG_AN
        add edi, im_tira
        add edi, [im_margen]
        mov ebx, [im_e0]
        add ebx, 4                  ; (una casilla de margen a cada lado)
        mov ebp, [im_e1]
        add ebp, 4
        mov ecx, [im_an_p]
.p:
        movzx eax, byte [esi]       ; el gris: 77 de rojo, 150 de verde y 29 de azul
        imul eax, 77
        movzx edx, byte [esi+1]
        imul edx, 150
        add eax, edx
        movzx edx, byte [esi+2]
        imul edx, 29
        add eax, edx
        shr eax, 8
        mov edx, [ebx]
        add edx, 8
        sar edx, 4
        add eax, edx                ; con el error que le ha tocado
        xor edx, edx                ; EDX=lo que se pone: 0 (negro) o 255
        cmp eax, 128
        jl .negro
        mov edx, 255
        jmp .pon
.negro:
        mov byte [edi], 1
.pon:
        sub eax, edx                ; el error
        cmp eax, 255                ; (acotado: una zona toda blanca o toda negra no arrastra)
        jle .a1
        mov eax, 255
.a1:
        cmp eax, -255
        jge .a2
        mov eax, -255
.a2:
        lea edx, [eax*8]
        sub edx, eax
        add [ebx+4], edx            ; 7/16 a la derecha
        lea edx, [eax+eax*2]
        add [ebp-4], edx            ; 3/16 abajo a la izquierda
        lea edx, [eax+eax*4]
        add [ebp], edx              ; 5/16 debajo
        add [ebp+4], eax            ; 1/16 abajo a la derecha
        add esi, 3
        inc edi
        add ebx, 4
        add ebp, 4
        dec ecx
        jnz .p
        ; la fila de debajo pasa a ser la de ahora
        mov eax, [im_e0]
        mov edi, eax
        xchg eax, [im_e1]
        mov [im_e0], eax
        mov ecx, PAG_AN + 2
        xor eax, eax
        rep stosd
        popad
        ret

; (internas) manda ECX octetos de ESI; CF=1 si falla
im_manda:
        lodsb
        call imp_car
        jc .x
        dec ecx
        jnz im_manda
.x:
        ret
; la tira, en blanco
im_limpia:
        pushad
        mov edi, im_tira
        mov ecx, 8 * PAG_AN / 4
        xor eax, eax
        rep stosd
        popad
        ret
; manda la tira (si tiene algún punto) y avanza el papel; la deja en blanco.
; CF=1 si falla
im_la_tira:
        pushad
        cmp dword [im_nfila], 0
        je .bien
        mov edi, im_tira            ; ¿tiene algo?
        mov ecx, 8 * PAG_AN
        xor al, al
        repe scasb
        je .avanza
        mov esi, im_graf            ; ESC * 5 y cuántas columnas
        mov ecx, 5
        call im_manda
        jc .mal
        xor ebx, ebx                ; EBX=la columna
.col:
        xor al, al                  ; sus ocho puntos: el de arriba, el bit 7
        mov ah, 0x80
        lea esi, [im_tira+ebx]
.pt:
        cmp byte [esi], 0
        je .p1
        or al, ah
.p1:
        add esi, PAG_AN
        shr ah, 1
        jnz .pt
        call imp_car
        jc .mal
        inc ebx
        cmp ebx, PAG_AN
        jb .col
.avanza:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        mov dword [im_nfila], 0
        call im_limpia
.bien:
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; AL: a la impresora. CF=1 si no se puede
imp_car:
        push eax
        push ebx
        call imp_lista
        jc .x
        movzx eax, al
        mov bl, 0x17
        call bios
        test ah, 0x28               ; error o sin papel
        jz .x                       ; (CF=0)
        stc
.x:
        pop ebx
        pop eax
        ret
; espera a que la impresora admita otro carácter, hasta veinte segundos (una
; de verdad se para a ratos, con la memoria llena); CF=1 si no contesta
imp_lista:
        push eax
        push ebx
        push edx
        HV tics
        mov edx, eax
.l:
        mov eax, 0x0200
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .no
        test ah, 0x80               ; no está ocupada
        jnz .si
        HV tics
        sub eax, edx
        cmp eax, 20 * 18
        jb .l
.no:
        stc
        jmp .x
.si:
        clc
.x:
        pop edx
        pop ebx
        pop eax
        ret
; la interrupción BL de la BIOS con EAX y DX=0 (LPT1) -> EAX
bios:
        push ecx
        push esi
        push edi
        push eax
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov [rl+RL_EAX], eax
        mov [rl+RL_INT], bl
        mov esi, rl
        HV rm_interrup
        mov eax, [rl+RL_EAX]
        pop edi
        pop esi
        pop ecx
        ret
