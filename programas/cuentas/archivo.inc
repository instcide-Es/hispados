; =============================================================================
;  Cuentas - los archivos (.HOJ y .CSV) y la impresora
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El formato propio, .HOJ, es de texto (se puede leer con cualquier editor)
;  y lo guarda todo. Una línea por cosa, acabada en CR+LF:
;       HOJA 1                  la primera: qué es el archivo y su versión
;       M EUR                   lo que va detrás de los números con moneda
;       I 1                     al imprimir como se ve sale la rejilla (solo
;                               si se ha pedido)
;       G 0 A2:C5 0             el gráfico de la hoja: su tipo, el rango del
;                               que sale y si las series van por filas (1)
;       T Ventas del año        su título; X..., Y...: los rótulos de sus ejes
;       H 7 30                  la fila 7 mide 30 puntos de alto (lo mismo)
;       P 1 2                   están inmovilizadas la primera columna y las
;                               dos primeras filas
;       A C1 120                la columna C mide 120 puntos de ancho (solo
;                               las que no tienen el ancho normal)
;       C B7 2176 =A1*2         la celda B7: su formato (los bits
;                               F_*, en decimal) y, detrás de un espacio, su
;                               contenido tal como se tecleó (solo las celdas
;                               que tienen contenido o formato)
;       L 2                     (libros de varias hojas) la hoja que se veía
;       S Ventas                y aquí empieza la hoja «Ventas»: hasta la
;                               «S» siguiente, todo es suyo (libro.inc)
;       V 1 6 0 0               (solo en las hojas que no se veían) dónde
;                               estaban su cursor y su vista
;  Lo que no se entiende se salta: una versión más nueva puede añadir líneas.
;
;  .CSV (para llevar y traer datos de otros programas): una fila por línea,
;  celdas separadas por «;», el contenido tal cual (las fórmulas, con su
;  «=»); la celda que lleva «;» o comillas va entre comillas (y las suyas,
;  dobles). No guarda formatos ni anchos. Al abrir, lo que no empieza por
;  «HOJA» se lee como CSV; al guardar, es CSV si el nombre acaba en .CSV.
;
;  Los archivos se leen enteros a un bloque de memoria aparte (a trozos de
;  16 K, que DOS no da más de 64 K de una vez) y se escriben a través de un
;  búfer pequeño, sin tener nunca el archivo entero en memoria.
; =============================================================================
ESC_TAM     equ 8192                ; el búfer de escribir
TROZO       equ 16384               ; lo que se lee de una vez

; ---- escribir ----
; abre para escribir el archivo ESI; CF=1 si no se puede. [0.9.31] Se
; escribe en un temporal y, al cerrar, toma su nombre: si algo falla, el de
; antes queda entero (guardar.inc)
esc_abre:
        push eax
        call gua_crear
        jc .x
        mov [esc_h], eax
        mov dword [esc_n], 0
        mov byte [esc_mal], 0
        clc
.x:
        pop eax
        ret
; AL, al archivo. Conserva los registros
esc_car:
        push edi
        mov edi, [esc_n]
        mov [esc_buf+edi], al
        inc edi
        mov [esc_n], edi
        pop edi
        cmp dword [esc_n], ESC_TAM
        jb .x
        call esc_vuelca
.x:
        ret
; el texto ESI (acabado en 0)
esc_cad:
        push eax
        push esi
.b:
        lodsb
        test al, al
        jz .x
        call esc_car
        jmp .b
.x:
        pop esi
        pop eax
        ret
; EAX, en decimal
esc_num:
        push edi
        push esi
        mov edi, ref_txt
        call hvl_num
        mov byte [edi], 0
        mov esi, ref_txt
        call esc_cad
        pop esi
        pop edi
        ret
; fin de línea
esc_nl:
        push eax
        mov al, 13
        call esc_car
        mov al, 10
        call esc_car
        pop eax
        ret
esc_vuelca:
        pushad
        mov ecx, [esc_n]
        jecxz .x
        cmp byte [esc_modo], 0      ; (a la memoria, o solo contando: libro.inc)
        je .arch
        add [esc_total], ecx
        cmp byte [esc_modo], 1
        je .x
        mov esi, esc_buf
        mov edi, [esc_mem_p]
        rep movsb
        mov [esc_mem_p], edi
        jmp .x
.arch:
        mov ebx, [esc_h]
        mov esi, esc_buf
        HV dos_escribir
        jc .mal
        cmp eax, [esc_n]
        je .x
.mal:
        mov byte [esc_mal], 1
.x:
        mov dword [esc_n], 0
        popad
        ret
; cierra; CF=1 si algo no se ha podido escribir (el disco lleno...)
esc_cierra:
        push eax
        push ebx
        call esc_vuelca
        mov ebx, [esc_h]
        mov al, [esc_mal]
        call gua_cerrar             ; (CF=1 si no se ha guardado)
        pop ebx
        pop eax
        ret

; ¿el nombre del archivo acaba en .CSV? -> ZF=1 si sí
es_csv:
        push eax
        push esi
        mov esi, nombre
.e:
        cmp byte [esi], 0
        je .e1
        inc esi
        jmp .e
.e1:
        sub esi, 4
        cmp esi, nombre
        jb .no
        mov eax, [esi]
        and eax, 0xDFDFDFFF         ; (a mayúsculas)
        cmp eax, '.CSV'
        jmp .x
.no:
        or esi, esi                 ; (ZF=0)
.x:
        pop esi
        pop eax
        ret

; escribe las líneas de la hoja activa (lo que es suyo: la rejilla al
; imprimir, los anchos, los altos, los paneles, el gráfico y las celdas) con
; las rutinas esc_*: al archivo o, al plegarla, a la memoria (libro.inc)
hoja_escribe:
        pushad
        cmp byte [imp_rej], 0       ; la rejilla al imprimir, si se ha pedido
        je .an0
        mov esi, t_hoj_rej
        call esc_cad
        call esc_nl
.an0:
        xor ecx, ecx                ; los anchos que no son el normal
.an:
        movzx eax, word [col_an+ecx*2]
        cmp eax, AN_NORMAL
        je .an1
        push eax
        mov al, 'A'
        call esc_car
        mov al, ' '
        call esc_car
        xor edx, edx
        mov edi, ref_txt
        call ref_nombre
        mov byte [edi], 0
        mov esi, ref_txt
        call esc_cad
        mov al, ' '
        call esc_car
        pop eax
        call esc_num
        call esc_nl
.an1:
        inc ecx
        cmp ecx, NCOL
        jb .an
        xor ecx, ecx                ; los altos que no son el normal
.al:
        movzx eax, byte [fil_al+ecx]
        cmp eax, CEL_AL
        je .al1
        push eax
        mov al, 'H'
        call esc_car
        mov al, ' '
        call esc_car
        lea eax, [ecx+1]
        call esc_num
        mov al, ' '
        call esc_car
        pop eax
        call esc_num
        call esc_nl
.al1:
        inc ecx
        cmp ecx, NFIL
        jb .al
        mov eax, [inm_c]            ; lo inmovilizado: columnas y filas
        or eax, [inm_f]
        jz .inm1
        mov al, 'P'
        call esc_car
        mov al, ' '
        call esc_car
        mov eax, [inm_c]
        call esc_num
        mov al, ' '
        call esc_car
        mov eax, [inm_f]
        call esc_num
        call esc_nl
.inm1:
%ifdef CON_GRAFICOS
        call gr_guarda              ; el gráfico de la hoja, si lo tiene
%endif
        cmp byte [esc_modo], 0      ; al plegarla: dónde están el cursor y la vista
        je .ce0
        mov al, 'V'
        call esc_car
        mov ebx, cur_col
        call .num
        mov ebx, cur_fil
        call .num
        mov ebx, desp_col
        call .num
        mov ebx, desp_fil
        call .num
        call esc_nl
.ce0:
        xor ebx, ebx                ; las celdas con algo
.ce:
        mov eax, [cel_fmt+ebx*4]
        or eax, [cel_off+ebx*4]
        jz .ce1
        mov al, 'C'
        call esc_car
        mov al, ' '
        call esc_car
        mov ecx, ebx
        and ecx, NCOL - 1
        mov edx, ebx
        shr edx, 6
        mov edi, ref_txt
        call ref_nombre
        mov byte [edi], 0
        mov esi, ref_txt
        call esc_cad
        mov al, ' '
        call esc_car
        mov eax, [cel_fmt+ebx*4]
        call esc_num
        mov al, ' '
        call esc_car
        call cel_puntero
        call esc_cad
        call esc_nl
.ce1:
        inc ebx
        cmp ebx, NCEL
        jb .ce
        popad
        ret
; (interna) un espacio y el número que hay en [EBX]
.num:
        mov al, ' '
        call esc_car
        mov eax, [ebx]
        jmp esc_num

guardar:
        cmp byte [nombre], 0
        jne guarda_escribe
        ; (sin nombre todavía: se pregunta)
guarda_como:
        pushad
        mov ebp, [ven]
        mov edi, nom_tmp
        mov esi, nombre
        call hvl_cad
        mov byte [edi], 0
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .no
        mov esi, nombre             ; (OFICINA2) el de antes, por si no se puede
        mov edi, nom_ant
        mov ecx, 80
        rep movsb
        mov esi, nom_tmp            ; el nombre, con .HOJ si no trae extensión
        mov edi, nombre
        xor edx, edx
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '.'
        jne .c1
        inc edx
.c1:
        cmp al, '\'
        jne .c2
        xor edx, edx
.c2:
        stosb
        jmp .c
.f:
        test edx, edx
        jnz .f2
        mov esi, t_ext_hoj
        call hvl_cad
.f2:
        mov byte [edi], 0
        mov byte [csv_avisado], 0
        mov esi, nombre             ; [0.9.31] ¿hay ya otro archivo con ese
        mov edx, nom_ant            ; nombre? Se pregunta si se reemplaza
        mov edi, t_guardar_como
        call hvl_reemplazar
        jnc .escribe
        mov esi, nom_ant
        mov edi, nombre
        mov ecx, 80
        rep movsb
        popad                       ; (no: el nombre de antes, y otra vez el cuadro)
        jmp guarda_como
.escribe:
        popad
        call guarda_escribe
        jnc .x
        pushad                      ; (OFICINA2) no se ha podido (la zona
        mov esi, nom_ant            ; protegida de Hispa-Pentium, un disco
        mov edi, nombre             ; lleno): el nombre de antes, y Guardar
        mov ecx, 80                 ; preguntará otra vez dónde
        rep movsb
        popad
        stc
.x:
        ret
.no:
        popad
        stc
        ret

; escribe la hoja en [nombre]; CF=1 si no se guarda
guarda_escribe:
        pushad
        call es_csv                 ; un .CSV no guarda formatos ni anchos: si
        jnz .abre                   ; los hay, se avisa (una vez por archivo)
        cmp byte [csv_avisado], 0
        jne .abre
        cmp dword [lib_n], 1        ; (ni las demás hojas del libro)
        ja .avisa
        mov edi, cel_fmt
        mov ecx, NCEL
        xor eax, eax
        repe scasd
        jne .avisa
        mov edi, col_an
        mov ecx, NCOL
        mov ax, AN_NORMAL
        repe scasw
        je .abre
.avisa:
        mov ebp, [ven]
        mov esi, t_csv_pierde
        mov edi, t_cuentas
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        je .av1
        popad
        stc
        ret
.av1:
        mov byte [csv_avisado], 1
.abre:
        mov esi, nombre
        call esc_abre
        jc .falla
        call es_csv
        jz .csv
        mov esi, t_hoj_cab
        call esc_cad
        call esc_nl
        mov al, 'M'
        call esc_car
        mov al, ' '
        call esc_car
        mov esi, moneda_txt
        call esc_cad
        call esc_nl
        call libro_escribe          ; sus hojas (libro.inc)
        jmp .cierra
.csv:
        call limites
        xor edx, edx
.rl:
        cmp edx, [lim_f]
        jae .cierra
        xor ecx, ecx
.cl:
        cmp ecx, [lim_c]
        jae .eol
        call cel_puntero
        ; la celda que lleva «;» (los argumentos de una función) o comillas
        ; va entre comillas, y las suyas, dobles: como en cualquier CSV
        push esi
        xor ah, ah
.q:
        mov al, [esi]
        test al, al
        jz .qf
        inc esi
        cmp al, ';'
        je .qs
        cmp al, '"'
        jne .q
.qs:
        mov ah, 1
        jmp .q
.qf:
        pop esi
        mov bl, ah
        test bl, bl
        jz .cpy
        mov al, '"'
        call esc_car
.cpy:
        lodsb
        test al, al
        jz .cpyend
        call esc_car
        cmp al, '"'
        jne .cpy
        test bl, bl
        jz .cpy
        call esc_car
        jmp .cpy
.cpyend:
        test bl, bl
        jz .sincom
        mov al, '"'
        call esc_car
.sincom:
        inc ecx
        cmp ecx, [lim_c]
        jae .eol
        mov al, ';'
        call esc_car
        jmp .cl
.eol:
        call esc_nl
        inc edx
        jmp .rl
.cierra:
        call esc_cierra
        jc .falla
        mov byte [modif], 0
        call pon_titulo
        popad
        clc
        ret
.falla:
        mov esi, t_no_guardar
        call aviso
        popad
        stc
        ret

; ---- abrir: el archivo ESI (acabado en 0) ----
cargar:
        pushad
        mov byte [car_rec], 0
        mov [car_ruta], esi
        xor al, al
        HV dos_abrir
        jc .falla
        mov [car_h], eax
        mov ebx, eax
        mov al, 2                   ; cuánto mide: hasta el final y vuelta
        xor edx, edx
        HV dos_puntero
        mov [car_tam], eax
        mov ebx, [car_h]
        xor al, al
        xor edx, edx
        HV dos_puntero
        mov ecx, [car_tam]
        add ecx, 16
        HV mem_bloque
        jc .falla1
        mov [car_buf], eax
        mov [car_mango], edx
        mov edi, eax
        mov esi, [car_tam]
.l:
        test esi, esi
        jz .lf
        mov ecx, esi
        cmp ecx, TROZO
        jbe .l1
        mov ecx, TROZO
.l1:
        mov ebx, [car_h]
        HV dos_leer
        jc .lf
        test eax, eax
        jz .lf
        add edi, eax
        sub esi, eax
        jmp .l
.lf:
        mov byte [edi], 0
        mov ebx, [car_h]
        HV dos_cerrar
        call libro_vacio
        mov esi, [car_buf]
        cmp dword [esi], 'HOJA'
        jne .csv
        call libro_lee
        jmp .leida
.csv:
        call lee_csv
.leida:
        mov edx, [car_mango]
        HV mem_bloque_soltar
        call limites
        mov esi, [car_ruta]
        mov edi, nombre
        mov ecx, 79
.nm:
        mov al, [esi]
        test al, al
        jz .nmx
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .nm
.nmx:
        mov byte [edi], 0
        cmp byte [car_rec], 0       ; [0.9.31] no cabía entero: se dice, y sin
        je .entero                  ; nombre, para que Guardar no lo escriba
        mov byte [nombre], 0        ; recortado encima (pide uno nuevo)
        mov esi, t_recortado
        call aviso
.entero:
        call al_principio
        cmp dword [barra_v], 0
        je .sinv
        call pon_hijas              ; (las pestañas del libro)
.sinv:
        mov byte [modif], 0
        call pon_titulo
        popad
        jmp fin_cambio
.falla1:
        mov ebx, [car_h]
        HV dos_cerrar
.falla:
        mov esi, t_no_abrir
        call aviso
        popad
        ret

; (interna) lee un .HOJ: ESI=el texto, acabado en 0
lee_hoj:
.lin:
        mov al, [esi]
        test al, al
        jz .fin
        cmp byte [esi+1], ' '
        jne .salta
        cmp al, 'C'
        je .c
        cmp al, 'A'
        je .a
        cmp al, 'M'
        je .m
        cmp al, 'I'
        je .i
        cmp al, 'H'
        je .h
        cmp al, 'P'
        je .p
        cmp al, 'V'
        je .v
        cmp al, 'L'
        je .l
%ifdef CON_GRAFICOS
        call gr_hoj_linea           ; (G, T, X, Y: el gráfico)
%endif
.salta:
        lodsb
        test al, al
        jz .fin
        cmp al, 10
        jne .salta
        jmp .lin
.fin:
        ret
.m:
        add esi, 2
        mov dword [moneda_txt], 0
        mov dword [moneda_txt+4], 0
        mov edi, moneda_txt
.m1:
        mov al, [esi]
        cmp al, 32
        jb .salta
        cmp edi, moneda_txt + 7
        jae .salta
        stosb
        inc esi
        jmp .m1
.v:
        add esi, 2                  ; el cursor y la vista de una hoja plegada
        call hvl_lee_num
        mov [hv_c], eax
        call hvl_lee_num
        mov [hv_f], eax
        call hvl_lee_num
        mov [hv_dc], eax
        call hvl_lee_num
        mov [hv_df], eax
        mov byte [hv_hay], 1
        jmp .salta
.l:
        add esi, 2                  ; la hoja del libro que estaba a la vista
        call hvl_lee_num
        mov [lib_quiere], eax
        jmp .salta
.h:
        add esi, 2
        call hvl_lee_num
        lea ecx, [eax-1]
        call hvl_lee_num
        cmp ecx, NFIL
        jae .salta
        cmp eax, AL_MIN
        jb .salta
        cmp eax, AL_MAX
        ja .salta
        mov [fil_al+ecx], al
        jmp .salta
.p:
        add esi, 2
        call hvl_lee_num
        mov ecx, eax
        call hvl_lee_num
        cmp ecx, INM_MAX
        ja .salta
        cmp eax, INM_MAX
        ja .salta
        mov [inm_c], ecx
        mov [inm_f], eax
        jmp .salta
.i:
        add esi, 2
        call hvl_lee_num
        and al, 1
        mov [imp_rej], al
        call rej_pon
        jmp .salta
.a:
        add esi, 2
        call lee_ref
        jc .salta
        call hvl_lee_num
        cmp ecx, NCOL
        jae .salta
        cmp eax, AN_MIN
        jb .salta
        cmp eax, AN_MAX
        ja .salta
        mov [col_an+ecx*2], ax
        jmp .salta
.c:
        add esi, 2
        call lee_ref
        jc .salta
        cmp ecx, NCOL
        jae .salta
        cmp edx, NFIL
        jae .salta
        call hvl_lee_num
        mov ebx, edx
        shl ebx, 6
        add ebx, ecx
        mov [cel_fmt+ebx*4], eax
        cmp byte [esi], ' '
        jne .ct
        inc esi
.ct:
        mov edi, aj_buf
.ct1:
        mov al, [esi]
        cmp al, 13
        je .ct2
        cmp al, 10
        je .ct2
        test al, al
        jz .ct2
        inc esi
        cmp edi, aj_buf + CEL_MAX - 1
        jae .ct_rec
        stosb
        jmp .ct1
.ct_rec:
        mov byte [car_rec], 1       ; [0.9.31] la celda no cabe entera
        jmp .ct1
.ct2:
        mov byte [edi], 0
        push esi
        mov esi, aj_buf
        call cel_pon
        pop esi
        jmp .salta

; (interna) lee un .CSV: ESI=el texto, acabado en 0
lee_csv:
        xor ecx, ecx                ; la columna
        xor edx, edx                ; y la fila
        mov edi, aj_buf
        mov word [car_com], 0x0100  ; fuera de comillas, al principio de celda
.pl:
        lodsb
        test al, al
        jz .celda
        cmp byte [car_com], 0       ; entre comillas todo es de la celda, menos
        je .fuera                   ; las comillas: dos seguidas son una, y una
        cmp al, '"'                 ; sola las cierra
        jne .dentro
        cmp byte [esi], '"'
        jne .cierra
        inc esi
        jmp .pon
.cierra:
        mov byte [car_com], 0
        jmp .pl
.dentro:
        cmp al, 32                  ; (un salto de línea dentro: fuera)
        jb .pl
        jmp .pon
.fuera:
        cmp al, 13
        je .pl
        cmp al, 10
        je .nl
        cmp al, ';'
        je .sep
        cmp al, '"'                 ; unas comillas al principio: las abren
        jne .pon
        cmp byte [car_ini], 0
        je .pon
        mov word [car_com], 0x0001
        jmp .pl
.pon:
        mov byte [car_ini], 0
        cmp edi, aj_buf + CEL_MAX - 1
        jae .rec
        stosb
        jmp .pl
.rec:
        mov byte [car_rec], 1       ; [0.9.31] la celda no cabe entera
        jmp .pl
.sep:
        call .celda
        inc ecx
        mov byte [car_ini], 1
        jmp .pl
.nl:
        call .celda
        inc edx
        xor ecx, ecx
        mov byte [car_ini], 1
        jmp .pl
; (interna) lo leído, a su celda (y al acabar el archivo, la última)
.celda:
        mov byte [edi], 0
        mov edi, aj_buf
        cmp byte [edi], 0
        je .cx
        cmp ecx, NCOL
        jae .cxr
        cmp edx, NFIL
        jae .cxr
        push esi
        mov esi, aj_buf
        call cel_pon
        call fecha_auto
        pop esi
.cx:
        ret
.cxr:
        mov byte [car_rec], 1       ; [0.9.31] fuera de la hoja: se dirá
        ret

; la celda ECX, EDX acaba de recibir el texto ESI: si es una fecha
; (16/11/2026) y la celda no tiene formato de número, se le pone el de
; fecha, para que se vea como se tecleó. Conserva los registros
fecha_auto:
        pushad
        mov ebx, edx
        shl ebx, 6
        add ebx, ecx
        test dword [cel_fmt+ebx*4], F_NUM
        jnz .x
.sp:
        cmp byte [esi], ' '
        jne .f
        inc esi
        jmp .sp
.f:
        call lee_fecha
        jc .x
.sp2:
        lodsb                       ; (y nada más detrás)
        cmp al, ' '
        je .sp2
        test al, al
        jnz .x
        mov eax, ebx
        call des_celda
        or dword [cel_fmt+ebx*4], FN_FECHA << F_NUM_D
.x:
        popad
        ret

; =============================================================================
;  La impresora (lo que se imprime y cómo, en imprime.inc)
; =============================================================================
; manda AL a la impresora; CF=1 si no lo admite
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
; la interrupción BL de la BIOS (o de DOS) con EAX y DX=0 (LPT1) -> EAX (lo
; demás que devuelve queda en [rl])
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

; el día de hoy (el reloj de DOS), en días desde el 30/12/1899: para HOY()
hoy_serie:
        push ebx
        push ecx
        mov eax, 0x2A00
        mov bl, 0x21
        call bios
        movzx eax, word [rl+RL_ECX]
        movzx ecx, byte [rl+RL_EDX+1]
        movzx ebx, byte [rl+RL_EDX]
        call fecha_serie
        jnc .x
        xor eax, eax
.x:
        pop ecx
        pop ebx
        ret

; un número al azar de 0 a 1 (sin llegar), en cienmillonésimas: para ALEATORIO()
azar_num:
        mov eax, CU_ESC
        HV azar
        ret

        section .bss
            alignb 4
esc_h       resd 1                  ; el archivo que se escribe,
esc_n       resd 1                  ; lo que hay en su búfer
car_ruta    resd 1
car_tam     resd 1
car_buf     resd 1
car_mango   resd 1
car_rec     resb 1                  ; [0.9.31] al abrir, algo no cabía (filas, columnas o letras)
car_h       resd 1
rl          resb RL_TAM             ; los registros de una interrupción
esc_mal     resb 1                  ; algo no se ha podido escribir
csv_avisado resb 1                  ; ya se ha dicho que el .CSV pierde los formatos
car_com     resb 1                  ; al abrir un CSV: dentro de unas comillas
car_ini     resb 1                  ; (detrás de car_com) al principio de una celda
            alignb 4
esc_buf     resb ESC_TAM + 4
        section .text
