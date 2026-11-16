; =============================================================================
;  DATOS.HVP - sacar y meter datos: exportar a .CSV, importar un .CSV,
;  imprimir el listado y las etiquetas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo que sale va letra a letra por «sal_c», a un archivo (con un búfer de
;  4 K) o a la impresora del puerto paralelo (LPT1, por la BIOS, INT 17h: el
;  PRN de Hispa-DOS todavía no imprime). Si algo falla queda apuntado en
;  [sal_err] y lo demás se calla: se dice una sola vez, al final.
;
;  El .CSV es el de Cuentas y Facturas: «;» entre celdas, números con coma y
;  sin puntos de millares, fechas día/mes/año, una línea por registro y, la
;  primera, con los nombres de los campos.
; =============================================================================

SAL_BUF     equ 4096
PAG_FILAS   equ 54                  ; registros por página del listado
PAG_ANCHO   equ 79                  ; columnas del papel
ET_ANCHO    equ 38                  ; ancho de una etiqueta (van dos)
ET_LINEAS   equ 4                   ; sus líneas (y dos en blanco: 6 = 1 pulgada)
ET_PAG      equ 10                  ; filas de etiquetas por página
ET_CAMPOS   equ 6                   ; campos en una línea de la etiqueta

%macro SAL_BSS 0
sal_man     resd 1                  ; el archivo que se escribe
sal_n       resd 1                  ; lo que hay en el búfer
im_ini      resd 1                  ; el .CSV que se importa: dónde está,
im_fin      resd 1                  ;   dónde acaba
im_n        resd 1                  ;   cuántas columnas
im_regs     resd 1
imp_pag     resd 1
imp_n       resd 1                  ; columnas que se imprimen
imp_w       resd MAX_CAM            ; el ancho de cada una
imp_c       resd MAX_CAM            ; y qué campo es
inf_ancho   resd 1                  ; el informe: columnas del papel,
inf_grupo   resd 1                  ;   campo por el que se agrupa (-1: no),
inf_lin     resd 1                  ;   renglones que van en la página,
g_n         resd 1                  ;   registros del grupo,
inf_cta     resd 1
s_gru       resd 2 * MAX_CAM        ;   las sumas del grupo (64 bits por columna)
s_tot       resd 2 * MAX_CAM        ;   y las de todo
inf_ord     resw 1
inf_arch    resb 1                  ;   1: a un archivo .TXT
inf_comp    resb 1                  ;   1: en letra comprimida
inf_reord   resb 1
g_ini       resb 1
car_arch    resb 1
            alignb 4
inf_t1      resb 40
inf_t2      resb 8
g_txt       resb 104
carta       resb CARTA_MAX + 8
sal_modo    resb 1                  ; 0: a un archivo; 1: a la impresora
sal_err     resb 1
im_sep      resb 1
im_corta    resb 1
im_alguno   resb MAX_CAM            ; de cada columna: tiene algo,
im_num      resb MAX_CAM            ;   todo son números,
im_fecha    resb MAX_CAM            ;   todo fechas,
im_sino     resb MAX_CAM            ;   todo «sí» o «no»,
im_dec      resb MAX_CAM            ;   los decimales que hacen falta
im_largo    resb MAX_CAM            ;   y si algún texto pasa de 100 letras
et_def      resb ET_LINEAS * ET_CAMPOS  ; las etiquetas: campo + 1 (0: nada)
et_txt      resb ET_LINEAS * 24
            alignb 4
et_izq      resb 96
et_der      resb 96
linea       resb 200
sal_buf     resb SAL_BUF
%endmacro

t_csv_tit   db "Exportar a .CSV (para Cuentas)", 0
t_csv_hecho db "La tabla se ha exportado. El archivo se abre con Cuentas.", 0
t_e_csv     db "No se ha podido escribir el archivo.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_e_nada    db "No hay registros a la vista: no hay nada que sacar.", 0
t_imp_tit   db "Importar un .CSV como tabla nueva", 0
t_e_imp     db "No se ha podido leer ese archivo.", 0
t_e_impvacio db "Ese archivo no tiene datos: la primera línea tiene que", 10
            db "llevar los nombres de los campos.", 0
t_e_impcabe db "Ese archivo es demasiado grande para la memoria de Datos.", 0
t_imp_hecho db " registros importados.", 0
t_imp_hecho1 db " registro importado.", 0
t_imp_corta db 10, "No cabían más: una tabla no pasa de 8.000 registros.", 0
t_imp_campo db "Campo ", 0
t_imp_nom   db "IMPORTADA", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_impreso   db "El listado se ha enviado a la impresora.", 0
t_et_impreso db "Las etiquetas se han enviado a la impresora.", 0
t_e_etiq    db "Escriba en alguna línea los números de los campos, de 1 a ", 0
t_pagina    db "   Página ", 0
t_ext_txt   db ".TXT", 0
t_inf_arch  db "Guardar como texto (para Pluma)", 0
t_inf_hecho db "El archivo de texto se ha guardado. Se abre con Pluma.", 0
t_e_infnum  db "Escriba números de campo, de 1 a ", 0
t_car_leer  db "Leer el modelo de un archivo de texto", 0
t_car_sep   db "----------------------------------------", 0
t_car_impreso db "Las cartas se han enviado a la impresora.", 0
t_e_carta   db "Escriba el texto del modelo (o léalo de un archivo).", 0
t_i_filtro  db "Filtro: ", 0
t_et_def1   db "1 2", 0
t_et_def2   db "3", 0
t_et_def3   db "4", 0
t_sep_si    db "si", 0
t_sep_no    db "no", 0

; =============================================================================
;  Por dónde sale
; =============================================================================
; abre el archivo ESI para escribir; CF=1 si no se puede
sal_abre:
        push eax
        mov byte [sal_modo], 0
        mov byte [sal_err], 0
        mov dword [sal_n], 0
        HV dos_crear
        jc .x
        mov [sal_man], eax
        clc
.x:
        pop eax
        ret
; (interna) escribe lo que hay en el búfer
sal_vacia:
        pushad
        mov ecx, [sal_n]
        test ecx, ecx
        jz .x
        cmp byte [sal_err], 0
        jne .x0
        mov ebx, [sal_man]
        mov esi, sal_buf
        HV dos_escribir
        jnc .x0
        mov byte [sal_err], 1
.x0:
        mov dword [sal_n], 0
.x:
        popad
        ret
; cierra el archivo; CF=1 si algo ha fallado
sal_cierra:
        push eax
        push ebx
        call sal_vacia
        mov ebx, [sal_man]
        HV dos_cerrar
        jnc .a
        mov byte [sal_err], 1
.a:
        cmp byte [sal_err], 1
        cmc                         ; (CF=1 si [sal_err] es 1)
        pop ebx
        pop eax
        ret

; prepara la impresora; CF=1 si no hay o no está lista
imp_abre:
        push eax
        push ebx
        mov byte [sal_modo], 1
        mov byte [sal_err], 0
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h: los dos
        mov bl, 0x11                ; bits de arriba, cuántos)
        call bios
        test ah, 0xC0
        jz .no
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28               ; (error o sin papel)
        jnz .no
        test ah, 0x10               ; (en línea)
        jz .no
        clc
        jmp .x
.no:
        mov byte [sal_err], 1
        stc
.x:
        pop ebx
        pop eax
        ret
; (interna) la interrupción BL de la BIOS con EAX y DX=0 (LPT1) -> EAX
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
; (interna) espera a que la impresora no esté ocupada, hasta veinte segundos
; (una de verdad se para a ratos, con la memoria llena); CF=1 si no contesta
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

; saca el carácter AL
sal_c:
        cmp byte [sal_modo], 0
        jne .imp
        push ebx
        mov ebx, [sal_n]
        mov [sal_buf+ebx], al
        inc ebx
        mov [sal_n], ebx
        cmp ebx, SAL_BUF
        pop ebx
        jb .x
        call sal_vacia
.x:
        ret
.imp:
        cmp byte [sal_err], 0
        jne .x
        push eax
        push ebx
        call imp_lista
        jc .mal
        movzx eax, al               ; AH=0: imprimir el carácter AL
        mov bl, 0x17
        call bios
        test ah, 0x28
        jz .bien
.mal:
        mov byte [sal_err], 1
.bien:
        pop ebx
        pop eax
        ret
; saca el texto ESI (acabado en 0)
sal_txt:
        push eax
        push esi
.b:
        lodsb
        test al, al
        jz .x
        call sal_c
        jmp .b
.x:
        pop esi
        pop eax
        ret
; salto de línea
sal_nl:
        push eax
        mov al, 13
        call sal_c
        mov al, 10
        call sal_c
        pop eax
        ret

; =============================================================================
;  Exportar a .CSV lo que se ve
; =============================================================================
; (interna) el nombre de archivo que se propone: las letras y cifras del
; nombre de la tabla (hasta ocho) y la extensión ESI -> nom_tmp
nombre_de_tabla:
        pushad
        mov ebp, esi
        mov edi, nom_tmp
        mov esi, [tab]
        add esi, T_NOM
        xor ecx, ecx
.n:
        movzx eax, byte [esi]
        inc esi
        test al, al
        jz .n2
        mov al, [pliega+eax]        ; sin acentos y en mayúsculas
        cmp al, '0'
        jb .n
        cmp al, '9'
        jbe .n1
        cmp al, 'A'
        jb .n
        cmp al, 'Z' + 1
        ja .n
        cmp al, 'O'                 ; (la tabla de comparar corre un sitio
        jb .n1                      ; las letras desde la O, por la eñe)
        mov ah, 'N'
        je .n0
        dec al
        mov ah, al
.n0:
        mov al, ah
.n1:
        stosb
        inc ecx
        cmp ecx, 8
        jb .n
.n2:
        test ecx, ecx
        jnz .n3
        mov esi, t_tabla_def
        call hvl_cad
.n3:
        mov esi, ebp
        call hvl_cad
        mov byte [edi], 0
        popad
        ret

exportar:
        call hay_tabla
        jc .x
        cmp dword [n_vis], 0
        jne .hay
        mov esi, t_e_nada
        jmp aviso
.hay:
        mov esi, t_ext_csv
        call nombre_de_tabla
        mov edi, t_csv_tit
        call pide_archivo
        jz .x
        mov esi, t_ext_csv
        call pon_ext
        mov esi, nom_tmp
        call sal_abre
        jc .mal
        xor ecx, ecx                ; la primera línea: los nombres
.c:
        cmp ecx, [n_cam]
        jae .c9
        mov eax, ecx
        call cam_ptr
        mov esi, ebx
        call csv_celda_sal
        inc ecx
        jmp .c
.c9:
        call sal_nl
        xor ebp, ebp                ; EBP=la fila
.f:
        cmp ebp, [n_vis]
        jae .f9
        mov esi, [vis]
        mov eax, [esi+ebp*4]
        xor ecx, ecx
.v:
        cmp ecx, [n_cam]
        jae .v9
        mov byte [fmt_mil], 0
        call reg_txt
        call csv_celda_sal
        inc ecx
        jmp .v
.v9:
        call sal_nl
        inc ebp
        jmp .f
.f9:
        call sal_cierra
        jc .mal
        mov esi, t_csv_hecho
        jmp aviso
.mal:
        mov esi, t_e_csv
        call aviso
.x:
        ret
; (interna) una celda (ESI=texto) de la columna ECX: delante, el «;» si no es
; la primera. Los «;», las comillas y los saltos de línea del texto se cambian
; por una coma, un apóstrofo y un espacio: así no hacen falta comillas
csv_celda_sal:
        push eax
        push esi
        test ecx, ecx
        jz .b
        mov al, ';'
        call sal_c
.b:
        lodsb
        test al, al
        jz .x
        cmp al, ';'
        jne .c1
        mov al, ','
.c1:
        cmp al, '"'
        jne .c2
        mov al, "'"
.c2:
        cmp al, ' '
        jae .c3
        mov al, ' '
.c3:
        call sal_c
        jmp .b
.x:
        pop esi
        pop eax
        ret

; =============================================================================
;  Importar un .CSV como tabla nueva
; =============================================================================
; (interna) lee una celda del .CSV: ESI=donde empieza -> su texto en vbuf
; (acabado en 0; ECX=lo que mide) y ESI detrás de ella. CF=1 si era la última
; de su línea (y ESI queda en la línea siguiente). Entiende las comillas
csv_lee:
        push eax
        push edi
        mov edi, vbuf
        cmp esi, [im_fin]
        jae .eol
        cmp byte [esi], '"'
        jne .plano
        inc esi
.q:
        cmp esi, [im_fin]
        jae .eol
        lodsb
        cmp al, '"'
        jne .qc
        cmp esi, [im_fin]
        jae .eol
        cmp byte [esi], '"'         ; dos seguidas: una comilla
        jne .plano
        inc esi
.qc:
        call .pon
        jmp .q
.plano:
        cmp esi, [im_fin]
        jae .eol
        lodsb
        cmp al, [im_sep]
        je .mas
        cmp al, 10
        je .eol
        cmp al, 13
        je .cr
        cmp al, 26                  ; (la marca de fin de archivo de DOS)
        je .fin
        call .pon
        jmp .plano
.fin:
        mov [im_fin], esi
        jmp .eol
.cr:
        cmp esi, [im_fin]
        jae .eol
        cmp byte [esi], 10
        jne .eol
        inc esi
.eol:
        mov al, 1
        jmp .x
.mas:
        xor al, al
.x:
        mov byte [edi], 0
        mov ecx, edi
        sub ecx, vbuf
        cmp al, 1
        cmc                         ; CF=1 si era la última de la línea
        pop edi
        pop eax
        ret
.pon:
        cmp edi, vbuf + MEMO_MAX
        jae .p1
        stosb
.p1:
        ret

; (interna) ESI=texto de una celda: ¿puede ser un número? (los que empiezan
; por cero, como un código postal, no: perderían el cero) -> CF=0 si lo es y
; EAX=los decimales que lleva escritos
es_numero:
        push ebx
        push edx
        push esi
        mov dl, 3
        call lee_num
        jc .no
        cmp eax, VACIO
        je .no
.sp:
        cmp byte [esi], ' '
        jne .s1
        inc esi
        jmp .sp
.s1:
        cmp byte [esi], '-'
        jne .s2
        inc esi
.s2:
        cmp byte [esi], '0'
        jne .dec
        mov al, [esi+1]
        sub al, '0'
        cmp al, 9
        jbe .no
.dec:
        xor eax, eax                ; los decimales: lo que hay tras la coma
        xor ebx, ebx                ; (o tras el punto, si no es de millares)
        xor edx, edx                ; EDX=puntos
.d:
        mov bl, [esi]
        inc esi
        test bl, bl
        jz .d9
        cmp bl, ','
        je .d1
        cmp bl, '.'
        jne .d2
        inc edx
.d1:
        xor eax, eax
        mov bh, bl
        jmp .d
.d2:
        cmp bl, ' '
        je .d
        inc eax
        jmp .d
.d9:
        test bh, bh                 ; sin separador: entero
        jz .cero
        cmp bh, ','
        je .ok
        cmp edx, 1                  ; un solo punto y no tres cifras detrás
        jne .cero
        cmp eax, 3
        jne .ok
.cero:
        xor eax, eax
.ok:
        cmp eax, 3
        jbe .ok1
        mov eax, 3
.ok1:
        clc
        jmp .x
.no:
        stc
.x:
        pop esi
        pop edx
        pop ebx
        ret

importar:
        mov eax, [arena]
        cmp dword [eax+H_NTAB], MAX_TAB
        jb .cabe
        mov esi, t_e_maxtab
        jmp aviso
.cabe:
        mov byte [nom_tmp], 0
        mov edi, t_imp_tit
        call pide_archivo
        jz .x0
        pushad
        call des_vacia              ; (el sitio de las fotos hace falta)
        ; el archivo, entero, al final de la arena (la tabla se irá haciendo
        ; desde [fin], por delante)
        mov esi, nom_tmp
        xor al, al
        HV dos_abrir
        mov esi, t_e_imp
        jc .dice
        mov ebx, eax
        mov al, 2
        xor edx, edx
        HV dos_puntero              ; EAX=lo que mide
        mov ecx, eax
        push ecx
        xor al, al
        xor edx, edx
        HV dos_puntero
        pop ecx
        mov eax, [arena_fin]
        sub eax, [fin]
        sub eax, T_CAB + MAX_CAM * C_TAM + 256
        jl .nocabe
        cmp ecx, eax
        ja .nocabe
        mov edi, [arena_fin]
        sub edi, ecx
        mov [im_ini], edi
        mov eax, [arena_fin]
        mov [im_fin], eax
.l:
        mov ecx, [arena_fin]
        sub ecx, edi
        jz .l9
        cmp ecx, 16384
        jbe .l1
        mov ecx, 16384
.l1:
        HV dos_leer
        jc .l8
        test eax, eax
        jz .l8
        add edi, eax
        jmp .l
.l8:
        mov [im_fin], edi           ; (ha dado menos de lo que decía)
.l9:
        HV dos_cerrar
        call imp_haz                ; ESI=lo que hay que decir
        jmp .dice
.nocabe:
        HV dos_cerrar
        mov esi, t_e_impcabe
.dice:
        call inval
        call aviso
        popad
.x0:
        ret

; (interna) hace la tabla con el .CSV que está en [im_ini]..[im_fin]
; -> ESI=el mensaje del resultado
imp_haz:
        mov esi, [im_ini]
        cmp esi, [im_fin]
        jb .hay
        mov esi, t_e_impvacio
        ret
.hay:
        ; el separador: el que más haya en la primera línea
        mov esi, [im_ini]
        xor ebx, ebx                ; BL=«;» BH=comas
        xor edx, edx                ; DL=tabuladores
.s:
        cmp esi, [im_fin]
        jae .s9
        lodsb
        cmp al, 10
        je .s9
        cmp al, 13
        je .s9
        cmp al, ';'
        jne .s1
        inc bl
.s1:
        cmp al, ','
        jne .s2
        inc bh
.s2:
        cmp al, 9
        jne .s
        inc dl
        jmp .s
.s9:
        mov al, ';'
        cmp bh, bl
        jbe .s3
        mov al, ','
        mov bl, bh
.s3:
        cmp dl, bl
        jbe .s4
        mov al, 9
.s4:
        mov [im_sep], al
        ; la primera línea: los nombres de los campos
        mov edi, nv_cam
        mov ecx, MAX_CAM * C_TAM
        xor al, al
        rep stosb
        mov esi, [im_ini]
        xor ebp, ebp                ; EBP=la columna
.n:
        call csv_lee
        pushf
        cmp ebp, MAX_CAM
        jae .n3
        mov edi, ebp
        shl edi, 5
        add edi, nv_cam
        push esi
        mov esi, vbuf
.nsp:
        cmp byte [esi], ' '
        jne .n0
        inc esi
        jmp .nsp
.n0:
        cmp byte [esi], 0
        jne .n1
        mov esi, t_imp_campo        ; sin nombre: «Campo 3»
        push edi
        call hvl_cad
        lea eax, [ebp+1]
        call hvl_num
        pop edi
        jmp .n2
.n1:
        mov ecx, 19
        push edi
.nc:
        lodsb
        test al, al
        jz .nc1
        stosb
        dec ecx
        jnz .nc
.nc1:
        pop edi
.n2:
        pop esi
        mov word [edi+C_ANCHO], 160
        inc ebp
.n3:
        popf
        jnc .n
        mov [im_n], ebp
        cmp ebp, MAX_CAM
        jbe .n4
        mov dword [im_n], MAX_CAM
.n4:
        mov [im_ini], esi           ; desde aquí, los registros
        ; primera pasada: qué tipo le va a cada columna
        mov edi, im_alguno
        mov ecx, 6 * MAX_CAM
        xor al, al
        rep stosb
        mov edi, im_num
        mov ecx, 3 * MAX_CAM
        mov al, 1
        rep stosb
.p1:
        cmp esi, [im_fin]
        jae .p9
        xor ebp, ebp
.p2:
        call csv_lee
        pushf
        cmp ebp, [im_n]
        jae .p8
        push esi
        mov esi, vbuf
        call vacio
        je .p7
        mov byte [im_alguno+ebp], 1
        cmp ecx, TXT_MAX
        jbe .p3
        mov byte [im_largo+ebp], 1
.p3:
        call es_numero
        jnc .p4
        mov byte [im_num+ebp], 0
        jmp .p5
.p4:
        cmp al, [im_dec+ebp]
        jbe .p5
        mov [im_dec+ebp], al
.p5:
        call lee_fecha
        jnc .p6
        mov byte [im_fecha+ebp], 0
.p6:
        call es_sino
        jnc .p7
        mov byte [im_sino+ebp], 0
.p7:
        pop esi
.p8:
        inc ebp
        popf
        jnc .p2
        jmp .p1
.p9:
        xor ebp, ebp
.t:
        cmp ebp, [im_n]
        jae .t9
        mov edi, ebp
        shl edi, 5
        add edi, nv_cam
        cmp byte [im_alguno+ebp], 0
        je .ts                      ; (todo vacío: texto)
        mov al, T_NUM
        cmp byte [im_num+ebp], 0
        jne .tp
        mov al, T_FECHA
        cmp byte [im_fecha+ebp], 0
        jne .tp
        mov al, T_SINO
        cmp byte [im_sino+ebp], 0
        jne .tp
        mov al, T_MEMO
        cmp byte [im_largo+ebp], 0
        jne .tp
        mov al, T_TEXTO
.tp:
        mov [edi+C_TIPO], al
        movzx eax, al
        movzx ecx, word [an_tipo+eax*2]
        mov [edi+C_ANCHO], cx
        cmp al, T_NUM
        jne .ts
        mov al, [im_dec+ebp]
        mov [edi+C_DEC], al
.ts:
        inc ebp
        jmp .t
.t9:
        ; la tabla, con el nombre del archivo (sin ruta ni extensión)
        mov eax, [im_n]
        mov [nv_n], eax
        mov esi, nom_tmp
        mov edx, esi
.a:
        lodsb
        test al, al
        jz .a1
        cmp al, '\'
        je .a0
        cmp al, ':'
        jne .a
.a0:
        mov edx, esi
        jmp .a
.a1:
        mov esi, edx
        mov edi, tn_nom
        mov ecx, 23
.a2:
        lodsb
        test al, al
        jz .a3
        cmp al, '.'
        je .a3
        stosb
        dec ecx
        jnz .a2
.a3:
        mov byte [edi], 0
        mov esi, tn_nom
        cmp byte [esi], 0
        jne .a4
        mov esi, t_imp_nom
.a4:
        call tabla_nueva
        jnc .reg
        mov esi, t_e_impcabe
        ret
.reg:
        ; segunda pasada: los registros, uno tras otro al final de la tabla
        ; (que es el final de la base). Lo ya leído del .CSV se puede pisar
        mov dword [im_regs], 0
        mov byte [im_corta], 0
        mov esi, [im_ini]
.r1:
        cmp esi, [im_fin]
        jae .r9
        cmp dword [im_regs], MAX_REG
        jb .r1a
        mov byte [im_corta], 1
        jmp .r9
.r1a:
        mov edi, [rbuf]
        add edi, 2
        xor ebp, ebp
        xor ebx, ebx                ; EBX=1 si la línea trae algo
.r2:
        call csv_lee
        pushf
        cmp ebp, [im_n]
        jae .r4
        test ecx, ecx
        jz .r3
        mov ebx, 1
.r3:
        push esi
        mov eax, ebp
        shl eax, 5
        mov dl, [nv_cam+eax+C_TIPO]
        mov dh, [nv_cam+eax+C_DEC]
        mov esi, vbuf
        call txt_val
        pop esi
.r4:
        inc ebp
        popf
        jnc .r2
.r5:
        cmp ebp, [im_n]             ; las celdas que falten, vacías
        jae .r6
        push esi
        mov eax, ebp
        shl eax, 5
        mov dl, [nv_cam+eax+C_TIPO]
        mov dh, [nv_cam+eax+C_DEC]
        mov esi, t_nada
        call txt_val
        pop esi
        inc ebp
        jmp .r5
.r6:
        test ebx, ebx               ; (una línea en blanco no es un registro)
        jz .r1
        mov eax, edi
        mov edx, [rbuf]
        sub eax, edx
        mov [edx], ax
        mov edi, [fin]
        lea ecx, [edi+eax]
        cmp ecx, esi                ; ¿alcanza a lo que falta por leer?
        ja .lleno
        push esi
        mov esi, edx
        mov ecx, eax
        rep movsb
        pop esi
        mov [fin], edi
        mov edx, [tab]
        add [edx+T_TAM], eax
        inc dword [edx+T_NREG]
        inc dword [im_regs]
        jmp .r1
.lleno:
        call tabla_borra            ; no cabe: la tabla, fuera
        mov esi, t_e_impcabe
        ret
.r9:
        call indexa
        call guarda
        mov edi, res_buf
        mov eax, [im_regs]
        call hvl_num
        mov esi, t_imp_hecho
        cmp eax, 1
        jne .r9a
        mov esi, t_imp_hecho1
.r9a:
        call hvl_cad
        cmp byte [im_corta], 0
        je .r10
        mov esi, t_imp_corta
        call hvl_cad
.r10:
        mov byte [edi], 0
        mov esi, res_buf
        ret

; (interna) ESI=texto: ¿es «sí» o «no»? -> CF=0 si lo es
es_sino:
        push eax
        push ecx
        push edx
        push edi
        call largo
        mov edi, t_sep_si
        mov edx, 2
        call cmp_txt
        test eax, eax
        jz .si
        mov edi, t_sep_no
        call cmp_txt
        test eax, eax
        jz .si
        stc
        jmp .x
.si:
        clc
.x:
        pop edi
        pop edx
        pop ecx
        pop eax
        ret

; =============================================================================
;  Imprimir el listado de lo que se ve
; =============================================================================
; (interna) las columnas del listado: todos los campos, en su orden
inf_todas:
        push eax
        xor eax, eax
.b:
        cmp eax, [n_cam]
        jae .x
        mov [imp_c+eax*4], eax
        inc eax
        jmp .b
.x:
        mov [imp_n], eax
        pop eax
        ret
; (interna) el ancho de cada columna del listado (las de imp_c): lo que mide
; su nombre o el más largo de sus valores (hasta 40); si entre todas no caben
; en el papel ([inf_ancho]), se estrechan las más anchas y, si ni así, se
; dejan fuera las últimas
imp_anchos:
        pushad
        xor ebx, ebx                ; EBX=la columna
.c:
        cmp ebx, [imp_n]
        jae .cabe
        mov eax, [imp_c+ebx*4]
        push ebx
        call cam_ptr
        mov esi, ebx
        pop ebx
        call largo
        mov edx, ecx                ; EDX=el ancho
        xor ebp, ebp
.r:
        cmp ebp, [n_vis]
        jae .r9
        mov esi, [vis]
        mov eax, [esi+ebp*4]
        mov ecx, [imp_c+ebx*4]
        call celda
        cmp ecx, edx
        jbe .r1
        mov edx, ecx
.r1:
        inc ebp
        jmp .r
.r9:
        cmp edx, 40
        jbe .r2
        mov edx, 40
.r2:
        mov [imp_w+ebx*4], edx
        inc ebx
        jmp .c
.cabe:
        xor eax, eax                ; lo que ocupan, con dos espacios entre ellas
        xor ecx, ecx
        xor edx, edx                ; EDX=la más ancha
.s:
        cmp ecx, [imp_n]
        jae .s9
        mov ebx, [imp_w+ecx*4]
        add eax, ebx
        add eax, 2
        cmp ebx, [imp_w+edx*4]
        jbe .s1
        mov edx, ecx
.s1:
        inc ecx
        jmp .s
.s9:
        sub eax, 2
        cmp eax, [inf_ancho]
        jle .x
        cmp dword [imp_w+edx*4], 8
        jbe .quita
        dec dword [imp_w+edx*4]
        jmp .cabe
.quita:
        cmp dword [imp_n], 1
        jbe .x
        dec dword [imp_n]
        jmp .cabe
.x:
        popad
        ret

; (interna) la línea [linea], hasta EDI, a la salida, con su salto
lin_sale:
        mov byte [edi], 0
        push esi
.q:                                 ; (sin los espacios del final)
        cmp edi, linea
        jbe .q1
        cmp byte [edi-1], ' '
        jne .q1
        dec edi
        mov byte [edi], 0
        jmp .q
.q1:
        mov esi, linea
        call sal_txt
        call sal_nl
        pop esi
        ret
; (interna) ESI=texto ECX=lo que mide, en EDX columnas, en EDI: arrimado a la
; izquierda (o a la derecha si AL=1), cortado si no cabe; y dos espacios
lin_celda:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        cmp ecx, edx
        jbe .a
        mov ecx, edx
.a:
        mov ebx, edx
        sub ebx, ecx                ; EBX=los espacios
        test al, al
        jz .t
        push ecx
        mov ecx, ebx
        mov al, ' '
        rep stosb
        pop ecx
        xor ebx, ebx
.t:
        rep movsb
        mov ecx, ebx
        add ecx, 2
        mov al, ' '
        rep stosb
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; (interna) la cabecera de una página del listado: la tabla, la fecha y la
; página; los nombres de las columnas y una raya
imp_cabecera:
        pushad
        mov edi, linea
        mov ecx, [inf_ancho]
        mov al, ' '
        rep stosb
        mov edi, linea
        mov esi, [tab]
        add esi, T_NOM
        call hvl_cad
        mov edi, num_tmp            ; a la derecha: «16/11/2026   Página 2»
        mov eax, [hoy]
        call fmt_fecha
        cmp byte [inf_arch], 0      ; (en un archivo no hay páginas)
        jne .sinp
        mov esi, t_pagina
        call hvl_cad
        mov eax, [imp_pag]
        call hvl_num
.sinp:
        mov byte [edi], 0
        mov esi, num_tmp
        call largo
        mov edi, linea
        add edi, [inf_ancho]
        sub edi, ecx
        call hvl_cad
        call lin_sale
        mov edi, linea
        cmp byte [f_on], 0
        je .sinf
        mov esi, t_i_filtro
        call hvl_cad
        call filtro_txt
.sinf:
        call lin_sale
        mov edi, linea              ; los nombres
        xor ebp, ebp
.n:
        cmp ebp, [imp_n]
        jae .n9
        mov eax, [imp_c+ebp*4]
        mov ecx, eax
        call cam_ptr
        mov esi, ebx
        xor eax, eax
        cmp byte [tipos+ecx], T_NUM
        jne .n1
        inc eax
.n1:
        call largo
        mov edx, [imp_w+ebp*4]
        call lin_celda
        inc ebp
        jmp .n
.n9:
        call lin_sale
        call inf_raya
        popad
        ret
; (interna) una raya debajo de cada columna
inf_raya:
        pushad
        mov edi, linea
        xor ebp, ebp
.y:
        cmp ebp, [imp_n]
        jae .y9
        mov ecx, [imp_w+ebp*4]
        mov al, '-'
        rep stosb
        mov ax, '  '
        stosw
        inc ebp
        jmp .y
.y9:
        call lin_sale
        popad
        ret

; Imprimir el listado, tal cual: todas las columnas, en 79, sin grupos
imprimir:
        call hay_tabla
        jc .x
        call inf_todas
        mov dword [inf_ancho], PAG_ANCHO
        mov dword [inf_grupo], -1
        mov byte [inf_arch], 0
        mov byte [inf_comp], 0
        jmp inf_sale
.x:
        ret

; Informe a medida: qué columnas, agrupado o no por un campo (con la cuenta y
; las sumas de cada grupo y las del total), en letra comprimida (132
; columnas; la orden es la de las impresoras ESC/P: SI, y DC2 al acabar) y a
; la impresora o a un archivo .TXT, para Pluma
informe:
        call hay_tabla
        jc .x
        mov esi, dlg_inf
        mov ebx, inf_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        je inf_sale
.x:
        ret

; (interna) saca el informe con lo que dicen imp_c, imp_n, [inf_ancho],
; [inf_grupo], [inf_arch] e [inf_comp]
inf_sale:
        cmp dword [n_vis], 0
        jne .hay
        mov esi, t_e_nada
        jmp aviso
.hay:
        cmp byte [inf_arch], 0
        je .imp
        mov esi, t_ext_txt
        call nombre_de_tabla
        mov edi, t_inf_arch
        call pide_archivo
        jz .x
        mov esi, t_ext_txt
        call pon_ext
        mov esi, nom_tmp
        call sal_abre
        jc .malarch
        jmp .va
.imp:
        call imp_abre
        jc .mal
        cmp byte [inf_comp], 0
        je .va
        mov al, 0x0F
        call sal_c
.va:
        cmp dword [inf_grupo], -1   ; agrupado: mientras dura, la vista va
        je .va1                     ; ordenada por ese campo
        mov ebx, [tab]
        mov ax, [ebx+T_ORDC]
        mov [inf_ord], ax
        mov eax, [inf_grupo]
        mov [ebx+T_ORDC], al
        mov byte [ebx+T_ORDD], 0
        call ordena
        mov byte [inf_reord], 1
.va1:
        call imp_anchos
        mov edi, s_gru
        mov ecx, 4 * MAX_CAM
        xor eax, eax
        rep stosd
        mov [g_n], eax
        mov byte [g_ini], 0
        mov dword [imp_pag], 1
        xor ebx, ebx                ; EBX=la fila
.pag:
        call imp_cabecera
        mov dword [inf_lin], 0
.f:
        cmp ebx, [n_vis]
        jae .fin
        cmp byte [inf_arch], 0
        jne .grupo
        cmp dword [inf_lin], PAG_FILAS
        jae .otra
.grupo:
        cmp dword [inf_grupo], -1
        je .fila
        mov esi, [vis]              ; ¿empieza otro grupo?
        mov eax, [esi+ebx*4]
        mov ecx, [inf_grupo]
        call celda
        cmp byte [g_ini], 0
        je .nuevo
        push esi
        mov esi, g_txt
        call largo
        mov edx, ecx
        mov edi, esi
        pop esi
        call largo
        call cmp_txt
        test eax, eax
        jz .fila
        mov esi, s_gru              ; sí: las cuentas del que acaba
        mov eax, [g_n]
        call inf_sumas
        call inf_lin_sale
        mov esi, [vis]
        mov eax, [esi+ebx*4]
        mov ecx, [inf_grupo]
        call celda
.nuevo:
        mov byte [g_ini], 1
        mov edi, g_txt              ; su valor, y su renglón: «Población: Valls»
        mov ecx, 100
.gc:
        lodsb
        stosb
        test al, al
        jz .gc1
        dec ecx
        jnz .gc
        mov byte [edi], 0
.gc1:
        mov edi, linea
        mov eax, [inf_grupo]
        push ebx
        call cam_ptr
        mov esi, ebx
        pop ebx
        call hvl_cad
        mov ax, ': '
        stosw
        mov esi, g_txt
        mov ecx, 60
.gn:
        lodsb
        test al, al
        jz .gn1
        stosb
        dec ecx
        jnz .gn
.gn1:
        call inf_lin_sale
        mov edi, s_gru
        mov ecx, 2 * MAX_CAM
        xor eax, eax
        rep stosd
        mov [g_n], eax
.fila:
        mov edi, linea
        xor ebp, ebp
.c:
        cmp ebp, [imp_n]
        jae .c9
        mov esi, [vis]
        mov eax, [esi+ebx*4]
        mov ecx, [imp_c+ebp*4]
        cmp byte [tipos+ecx], T_NUM
        jne .c0
        push eax                    ; un número: a las sumas del grupo y del total
        mov esi, [idx]
        mov esi, [esi+eax*4]
        mov eax, ecx
        call val_ptr
        mov eax, [esi]
        cmp eax, VACIO
        je .c00
        cdq
        add [s_gru+ebp*8], eax
        adc [s_gru+ebp*8+4], edx
        add [s_tot+ebp*8], eax
        adc [s_tot+ebp*8+4], edx
.c00:
        pop eax
.c0:
        push ecx
        call celda
        mov edx, [imp_w+ebp*4]
        pop eax
        cmp byte [tipos+eax], T_NUM
        mov eax, 0
        jne .c1
        inc eax
.c1:
        call lin_celda
        inc ebp
        jmp .c
.c9:
        call inf_lin_sale
        inc dword [g_n]
        inc ebx
        cmp byte [sal_err], 0
        jne .acaba
        jmp .f
.otra:
        mov al, 12                  ; página nueva
        call sal_c
        inc dword [imp_pag]
        jmp .pag
.fin:
        cmp dword [inf_grupo], -1
        jne .ftot
        call sal_nl                 ; abajo, la cuenta
        mov edi, linea
        mov eax, [n_vis]
        call hvl_num
        mov esi, t_p_reg
        cmp eax, 1
        jne .r
        mov esi, t_p_reg1
.r:
        call hvl_cad
        call lin_sale
        jmp .acaba
.ftot:
        mov esi, s_gru              ; las cuentas del último grupo y, bajo una
        mov eax, [g_n]              ; raya, las de todo
        call inf_sumas
        call lin_sale
        call inf_raya
        mov esi, s_tot
        mov eax, [n_vis]
        call inf_sumas
        call lin_sale
.acaba:
        cmp byte [inf_reord], 0     ; el orden de la tabla, como estaba (la
        je .a1                      ; vista la rehace quien ha llamado)
        mov ebx, [tab]
        mov ax, [inf_ord]
        mov [ebx+T_ORDC], ax
.a1:
        cmp byte [inf_arch], 0
        jne .cierra
        mov al, 12
        call sal_c
        cmp byte [inf_comp], 0
        je .a2
        mov al, 0x12
        call sal_c
.a2:
        cmp byte [sal_err], 0
        jne .mal
        mov esi, t_impreso
        jmp aviso
.cierra:
        call sal_cierra
        jc .malarch
        mov esi, t_inf_hecho
        jmp aviso
.malarch:
        mov esi, t_e_csv
        jmp aviso
.mal:
        mov esi, t_e_imprimir
        call aviso
.x:
        ret
; (interna) un renglón del informe, a la salida; lleva la cuenta de los de la
; página
inf_lin_sale:
        call lin_sale
        inc dword [inf_lin]
        ret
; (interna) monta en [linea] (y deja EDI al final) el renglón de las cuentas:
; ESI=las sumas (64 bits por columna) EAX=cuántos registros. Bajo cada
; columna de números, su suma; y al final, entre paréntesis, la cuenta
inf_sumas:
        push eax
        push ebx
        push ecx
        push edx
        push ebp
        mov ebx, esi
        mov edi, linea
        xor ebp, ebp
        mov [inf_cta], eax
.c:
        cmp ebp, [imp_n]
        jae .x
        mov ecx, [imp_c+ebp*4]
        push edi
        mov edi, lin_buf
        cmp byte [tipos+ecx], T_NUM
        jne .t
        movzx ecx, byte [decs+ecx]
        mov eax, [ebx+ebp*8]
        mov edx, [ebx+ebp*8+4]
        call fmt_num64
        mov al, 1
        jmp .p
.t:
        xor al, al
.p:
        mov byte [edi], 0
        pop edi
        mov esi, lin_buf
        call largo
        mov edx, [imp_w+ebp*4]
        call lin_celda
        inc ebp
        jmp .c
.x:
        mov al, '('                 ; y detrás, la cuenta
        stosb
        mov eax, [inf_cta]
        call hvl_num
        mov esi, t_p_reg
        cmp eax, 1
        jne .x1
        mov esi, t_p_reg1
.x1:
        call hvl_cad
        mov al, ')'
        stosb
        pop ebp
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; el gancho del informe a medida
inf_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_PAINT
        je .pinta
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        je .acepta
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 110
        mov edx, inf_t1
        call g_pon_txt
        mov ecx, 111
        mov edx, inf_t2
        call g_pon_txt
        mov ecx, 120
        movzx edx, byte [inf_comp]
        call g_pon_marca
        mov ecx, 121
        movzx edx, byte [inf_arch]
        call g_pon_marca
        jmp .no
.pinta:
        HV dc_pintar
        mov edx, 146
        call pinta_numeros
        HV dc_cerrar
        mov eax, 1
        ret
.acepta:
        mov ecx, 110
        mov edx, inf_t1
        mov ebx, 40
        call g_lee_txt
        mov ecx, 111
        mov edx, inf_t2
        mov ebx, 8
        call g_lee_txt
        mov ecx, 120
        call g_marca
        mov [inf_comp], al
        mov dword [inf_ancho], PAG_ANCHO
        test al, al
        jz .a0
        mov dword [inf_ancho], 132
.a0:
        mov ecx, 121
        call g_marca
        mov [inf_arch], al
        mov esi, inf_t1             ; las columnas: sus números, o todas
        xor edi, edi
.n:
        mov al, [esi]
        test al, al
        jz .n9
        sub al, '0'
        cmp al, 9
        jbe .n1
        inc esi
        jmp .n
.n1:
        call hvl_lee_num
        mov ecx, 110
        test eax, eax
        jz .mal
        cmp eax, [n_cam]
        ja .mal
        dec eax
        cmp edi, MAX_CAM
        jae .n
        mov [imp_c+edi*4], eax
        inc edi
        jmp .n
.n9:
        mov [imp_n], edi
        test edi, edi
        jnz .g
        call inf_todas
.g:
        mov dword [inf_grupo], -1   ; y el campo por el que se agrupa
        mov esi, inf_t2
        call vacio
        je .no
        call hvl_lee_num
        mov ecx, 111
        test eax, eax
        jz .mal
        cmp eax, [n_cam]
        ja .mal
        dec eax
        mov [inf_grupo], eax
        jmp .no
.mal:
        push ecx
        mov edi, res_buf
        mov esi, t_e_infnum
        call hvl_cad
        mov eax, [n_cam]
        call hvl_num
        mov ax, '.'
        stosw
        mov esi, res_buf
        call g_aviso
        pop ecx
        call g_foco
        mov eax, 1
        ret

; (interna, para los ganchos; dentro de un dc) los números de los campos, en
; dos renglones desde la y EDX: «1 Nombre  2 Apellidos  3 NIF»
pinta_numeros:
        pushad
        COLOR C_TINTA
        lea eax, [edx+36]
        mov [inf_cta], eax          ; (hasta dónde: dos renglones)
        xor ecx, ecx
.p0:
        mov edi, linea
.p:
        cmp ecx, [n_cam]
        jae .p8
        mov eax, edi
        sub eax, linea
        cmp eax, 26
        ja .p8
        lea eax, [ecx+1]
        call hvl_num
        mov al, ' '
        stosb
        mov eax, ecx
        call cam_ptr
        mov esi, ebx
        call hvl_cad
        mov ax, '  '
        stosw
        inc ecx
        jmp .p
.p8:
        mov byte [edi], 0
        push ecx
        push edx
        mov eax, 16
        mov ebx, edx
        mov esi, linea
        HV dc_texto
        pop edx
        pop ecx
        add edx, 18
        cmp edx, [inf_cta]
        jb .p0
        popad
        ret

; =============================================================================
;  Cartas y recibos en serie: un texto modelo con campos entre « » (o entre
;  < >, que están en el teclado), una vez por registro de los que se ven
; =============================================================================
CARTA_MAX   equ 4000
cartas:
        call hay_tabla
        jc .x
        cmp dword [n_vis], 0
        jne .hay
        mov esi, t_e_nada
        jmp aviso
.hay:
        mov esi, dlg_carta
        mov ebx, carta_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .x
        cmp byte [car_arch], 0
        je .imp
        mov esi, t_ext_txt
        call nombre_de_tabla
        mov edi, t_inf_arch
        call pide_archivo
        jz .x
        mov esi, t_ext_txt
        call pon_ext
        mov esi, nom_tmp
        call sal_abre
        jc .malarch
        jmp .va
.imp:
        call imp_abre
        jc .mal
.va:
        xor ebx, ebx                ; EBX=el registro (su sitio en la vista)
.r:
        cmp ebx, [n_vis]
        jae .fin
        mov esi, [vis]
        mov edx, [esi+ebx*4]        ; EDX=el registro
        mov esi, carta
.c:
        lodsb
        test al, al
        jz .c9
        cmp al, 10
        jne .c1
        call sal_nl
        jmp .c
.c1:
        mov ah, '>'
        cmp al, '<'
        je .campo
        mov ah, '»'
        cmp al, '«'
        je .campo
.pon:
        call sal_c
        jmp .c
.campo:                             ; ¿es el nombre de un campo? AH=cómo acaba
        xor ecx, ecx
.cn:
        cmp byte [esi+ecx], ah
        je .cn1
        cmp byte [esi+ecx], ' '
        jb .pon
        inc ecx
        cmp ecx, 20
        jb .cn
        jmp .pon
.cn1:
        push eax
        push ebx
        push edx
        xor ebp, ebp                ; EBP=el campo
.cb:
        cmp ebp, [n_cam]
        jae .cno
        mov eax, ebp
        call cam_ptr
        mov edi, ebx
        push ecx
        push esi
        mov esi, ebx
        call largo
        mov edx, ecx
        pop esi
        pop ecx
        call cmp_txt
        test eax, eax
        jz .csi
        inc ebp
        jmp .cb
.cno:
        pop edx
        pop ebx
        pop eax
        jmp .pon
.csi:
        pop edx
        pop ebx
        pop eax
        lea esi, [esi+ecx+1]        ; detrás del nombre y su cierre
        push esi
        mov eax, edx
        mov ecx, ebp
        call celda
        call sal_txt
        pop esi
        jmp .c
.c9:
        cmp byte [car_arch], 0
        jne .sep
        mov al, 12                  ; cada carta, en su hoja
        call sal_c
        jmp .sig
.sep:
        call sal_nl
        mov esi, t_car_sep
        call sal_txt
        call sal_nl
.sig:
        inc ebx
        cmp byte [sal_err], 0
        je .r
.fin:
        cmp byte [car_arch], 0
        jne .cierra
        cmp byte [sal_err], 0
        jne .mal
        mov esi, t_car_impreso
        jmp aviso
.cierra:
        call sal_cierra
        jc .malarch
        mov esi, t_inf_hecho
        jmp aviso
.malarch:
        mov esi, t_e_csv
        jmp aviso
.mal:
        mov esi, t_e_imprimir
        call aviso
.x:
        ret

carta_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_PAINT
        je .pinta
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 130
        je .leer
        cmp ecx, ID_ACEPTAR
        je .acepta
.no:
        xor eax, eax
        ret
.ini:
        mov ecx, 110
        mov edx, carta
        call g_pon_txt
        mov ecx, 121
        movzx edx, byte [car_arch]
        call g_pon_marca
        jmp .no
.pinta:
        HV dc_pintar
        mov edx, 246
        call pinta_numeros
        HV dc_cerrar
        mov eax, 1
        ret
.leer:                              ; el modelo, de un archivo de texto
        mov byte [nom_tmp], 0
        mov ebx, nom_tmp
        mov edi, t_car_leer
        HV dlg_archivo
        test eax, eax
        jz .si
        mov esi, nom_tmp
        mov edi, [ebufs]
        mov ecx, CARTA_MAX
        call hvl_leer
        mov esi, [ebufs]            ; sin los retornos, y hasta el fin de archivo
        mov edi, carta
.l:
        lodsb
        cmp al, 13
        je .l
        cmp al, 26
        jne .l1
        xor al, al
.l1:
        stosb
        test al, al
        jnz .l
        mov ecx, 110
        mov edx, carta
        call g_pon_txt
.si:
        mov eax, 1
        ret
.acepta:
        mov ecx, 110
        mov edx, carta
        mov ebx, CARTA_MAX
        call g_lee_txt
        mov ecx, 121
        call g_marca
        mov [car_arch], al
        mov esi, carta
        call vacio
        jne .no
        mov esi, t_e_carta
        call g_aviso
        mov ecx, 110
        call g_foco
        mov eax, 1
        ret

; =============================================================================
;  Etiquetas: una por registro de los que se ven, en dos columnas
; =============================================================================
etiquetas:
        call hay_tabla
        jc .x
        cmp dword [n_vis], 0
        jne .hay
        mov esi, t_e_nada
        jmp aviso
.hay:
        cmp byte [et_txt], 0        ; la primera vez: los primeros campos
        jne .d
        cmp byte [et_txt+24], 0
        jne .d
        mov esi, t_et_def1
        mov edi, et_txt
        call copia
        mov esi, t_et_def2
        mov edi, et_txt + 24
        call copia
        mov esi, t_et_def3
        mov edi, et_txt + 48
        call copia
.d:
        mov esi, dlg_etiq
        mov ebx, etiq_gancho
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .x
        call imp_abre
        jc .mal
        xor ebx, ebx                ; EBX=el registro de la izquierda
        xor edx, edx                ; EDX=filas de etiquetas en la página
.f:
        cmp ebx, [n_vis]
        jae .fin
        cmp edx, ET_PAG
        jb .f1
        mov al, 12
        call sal_c
        xor edx, edx
.f1:
        xor ebp, ebp                ; EBP=la línea de la etiqueta
.l:
        mov eax, ebx
        mov edi, et_izq
        call etiq_linea
        lea eax, [ebx+1]
        mov edi, et_der
        call etiq_linea
        mov edi, linea
        mov esi, et_izq
        call largo
        push edx
        mov edx, ET_ANCHO
        xor eax, eax
        call lin_celda
        mov esi, et_der
        call largo
        call lin_celda
        pop edx
        call lin_sale
        inc ebp
        cmp ebp, ET_LINEAS
        jb .l
        call sal_nl
        call sal_nl
        add ebx, 2
        inc edx
        cmp byte [sal_err], 0
        jne .mal
        jmp .f
.fin:
        mov al, 12
        call sal_c
        cmp byte [sal_err], 0
        jne .mal
        mov esi, t_et_impreso
        jmp aviso
.mal:
        mov esi, t_e_imprimir
        call aviso
.x:
        ret
; (interna) la línea EBP de la etiqueta del registro que está en el sitio EAX
; de la vista (si lo hay), a EDI: sus campos, con un espacio entre ellos
etiq_linea:
        pushad
        mov byte [edi], 0
        cmp eax, [n_vis]
        jae .x
        mov esi, [vis]
        mov ebx, [esi+eax*4]        ; EBX=el registro
        imul ebp, ebp, ET_CAMPOS
        add ebp, et_def             ; EBP=los campos de la línea
        mov edx, ET_CAMPOS
        xor ecx, ecx                ; ECX=lo que va escrito
.c:
        movzx eax, byte [ebp]
        dec eax
        js .f
        push ecx
        mov ecx, eax
        mov eax, ebx
        call celda                  ; ESI=texto
        pop ecx
        cmp byte [esi], 0
        je .s
        test ecx, ecx
        jz .p
        cmp ecx, 90
        jae .f
        mov byte [edi], ' '
        inc edi
        inc ecx
.p:
        lodsb
        test al, al
        jz .s
        cmp ecx, 90
        jae .f
        stosb
        inc ecx
        jmp .p
.s:
        inc ebp
        dec edx
        jnz .c
.f:
        mov byte [edi], 0
.x:
        popad
        ret

; el gancho de las etiquetas: las cuatro líneas, con números de campo
etiq_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_PAINT
        je .pinta
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        je .acepta
.no:
        xor eax, eax
        ret
.ini:
        xor ebx, ebx
.i:
        imul edx, ebx, 24
        add edx, et_txt
        lea ecx, [ebx+110]
        call g_pon_txt
        inc ebx
        cmp ebx, ET_LINEAS
        jb .i
        jmp .no
.pinta:                             ; abajo, los números de los campos
        HV dc_pintar
        mov edx, 196
        call pinta_numeros
        HV dc_cerrar
        mov eax, 1
        ret
.acepta:
        mov edi, et_def
        mov ecx, ET_LINEAS * ET_CAMPOS
        xor al, al
        rep stosb
        mov dword [et_n], 0
        xor ebx, ebx
.a:
        imul edx, ebx, 24
        add edx, et_txt
        lea ecx, [ebx+110]
        push ebx
        mov ebx, 24
        call g_lee_txt
        pop ebx
        mov esi, edx                ; los números de la línea
        imul edi, ebx, ET_CAMPOS
        add edi, et_def
        mov ecx, ET_CAMPOS
.n:
        mov al, [esi]
        test al, al
        jz .n9
        sub al, '0'
        cmp al, 9
        jbe .n1
        inc esi                     ; (lo que no es una cifra separa)
        jmp .n
.n1:
        call hvl_lee_num
        test eax, eax
        jz .mal
        cmp eax, [n_cam]
        ja .mal
        stosb
        inc dword [et_n]
        dec ecx
        jnz .n
.n9:
        inc ebx
        cmp ebx, ET_LINEAS
        jb .a
        cmp dword [et_n], 0
        je .mal
        jmp .no
.mal:
        mov edi, res_buf
        mov esi, t_e_etiq
        call hvl_cad
        mov eax, [n_cam]
        call hvl_num
        mov ax, '.'
        stosw
        mov esi, res_buf
        call g_aviso
        mov ecx, 110
        call g_foco
        mov eax, 1
        ret
