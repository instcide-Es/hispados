; =============================================================================
;  Cuentas - los archivos (.HOJ y .CSV) y la impresora
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El formato propio, .HOJ, es de texto (se puede leer con cualquier editor)
;  y lo guarda todo. Una línea por cosa, acabada en CR+LF:
;       HOJA 1                  la primera: qué es el archivo y su versión
;       M EUR                   lo que va detrás de los números con moneda
;       A C1 120                la columna C mide 120 puntos de ancho (solo
;                               las que no tienen el ancho normal)
;       C B7 2176 =A1*2         la celda B7: su formato (la palabra de bits
;                               F_*, en decimal) y, detrás de un espacio, su
;                               contenido tal como se tecleó (solo las celdas
;                               que tienen contenido o formato)
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
IMP_AN      equ 80                  ; letras de una línea de la impresora

; ---- escribir ----
; abre para escribir el archivo ESI; CF=1 si no se puede
esc_abre:
        push eax
        HV dos_crear
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
        push ebx
        call esc_vuelca
        mov ebx, [esc_h]
        HV dos_cerrar
        pop ebx
        cmp byte [esc_mal], 1
        cmc                         ; (CF=1 si esc_mal vale 1)
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
        popad
        jmp guarda_escribe
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
        mov edi, cel_fmt
        mov ecx, NCEL
        xor eax, eax
        repe scasw
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
        xor ebx, ebx                ; las celdas con algo
.ce:
        movzx eax, word [cel_fmt+ebx*2]
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
        movzx eax, word [cel_fmt+ebx*2]
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
        call hoja_vacia
        mov esi, [car_buf]
        cmp dword [esi], 'HOJA'
        jne .csv
        call lee_hoj
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
        call al_principio
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
        mov [cel_fmt+ebx*2], ax
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
        jae .ct1
        stosb
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
        jae .pl
        stosb
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
        jae .cx
        cmp edx, NFIL
        jae .cx
        push esi
        mov esi, aj_buf
        call cel_pon
        call fecha_auto
        pop esi
.cx:
        ret

; la celda ECX, EDX acaba de recibir el texto ESI: si es una fecha
; (16/11/2026) y la celda no tiene formato de número, se le pone el de
; fecha, para que se vea como se tecleó. Conserva los registros
fecha_auto:
        pushad
        mov ebx, edx
        shl ebx, 6
        add ebx, ecx
        test word [cel_fmt+ebx*2], F_NUM
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
        or word [cel_fmt+ebx*2], FN_FECHA << F_NUM_D
.x:
        popad
        ret

; =============================================================================
;  Imprimir: por el puerto paralelo (LPT1) con la BIOS, INT 17h, como texto.
;  Sale lo elegido (si es más de una celda) o toda la hoja escrita: una fila
;  por línea, cada columna con el ancho que tiene en la pantalla (en letras)
;  y un espacio entre columnas, cada celda alineada como se ve. El texto
;  largo sigue sobre las celdas vacías de su derecha. Lo que pasa de IMP_AN
;  letras no sale.
; =============================================================================
imprimir:
        pushad
        call sel_rect
        mov eax, [r_c1]
        xor eax, [r_c2]
        mov ebx, [r_f1]
        xor ebx, [r_f2]
        or eax, ebx
        jnz .rango
        xor eax, eax                ; una celda sola: toda la hoja
        mov [r_c1], eax
        mov [r_f1], eax
        mov dword [r_c2], NCOL - 1
        mov dword [r_f2], NFIL - 1
.rango:
        call rect_recorta
        cmp dword [lim_f], 0
        jne .hay
        mov esi, t_imp_vacia
        call aviso
        popad
        ret
.hay:
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
        mov edx, [r_f1]
.fila:
        mov edi, imp_lin            ; la línea, en blanco
        mov ecx, IMP_AN
        mov al, ' '
        rep stosb
        xor ebx, ebx                ; EBX=la letra de la línea donde empieza la columna
        mov ecx, [r_c1]
.col:
        movzx ebp, word [col_an+ecx*2]
        shr ebp, 3                  ; EBP=el ancho de la columna, en letras
        jnz .c1
        inc ebp
.c1:
        call celda_ver
        xor eax, eax
.lg:
        cmp byte [disp+eax], 0
        je .lgf
        inc eax
        jmp .lg
.lgf:
        test eax, eax               ; (vacía: se queda lo que haya llegado de
        jz .sig                     ; la izquierda)
        push ecx
        mov edi, ebx                ; limpia su sitio
        mov ecx, ebp
.lim:
        cmp edi, IMP_AN
        jae .limf
        mov byte [imp_lin+edi], ' '
        inc edi
        dec ecx
        jnz .lim
.limf:
        pop ecx
        mov edi, ebx                ; dónde empieza, según la alineación
        mov esi, ebp
        sub esi, eax
        jle .pon
        cmp dword [disp_alin], 1
        je .pon
        cmp dword [disp_alin], 2
        jne .der
        shr esi, 1
.der:
        add edi, esi
.pon:
        mov esi, disp
.p:
        lodsb
        test al, al
        jz .sig
        cmp edi, IMP_AN
        jae .sig
        mov [imp_lin+edi], al
        inc edi
        jmp .p
.sig:
        lea ebx, [ebx+ebp+1]
        inc ecx
        cmp ecx, [r_c2]
        jbe .col
        mov ecx, IMP_AN             ; la línea, sin los espacios del final
.fin:
        cmp byte [imp_lin+ecx-1], ' '
        jne .manda
        dec ecx
        jnz .fin
.manda:
        mov esi, imp_lin
        jecxz .nl
.m:
        lodsb
        call imp_car
        jc .mal
        dec ecx
        jnz .m
.nl:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        inc edx
        cmp edx, [r_f2]
        jbe .fila
        mov al, 12                  ; y la hoja, fuera
        call imp_car
        jc .mal
        call imp_lista
        jc .mal
        mov esi, t_impreso
        call aviso
        popad
        ret
.mal:
        mov esi, t_e_imprimir
        call aviso
        popad
        ret

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
car_h       resd 1
rl          resb RL_TAM             ; los registros de una interrupción
esc_mal     resb 1                  ; algo no se ha podido escribir
csv_avisado resb 1                  ; ya se ha dicho que el .CSV pierde los formatos
car_com     resb 1                  ; al abrir un CSV: dentro de unas comillas
car_ini     resb 1                  ; (detrás de car_com) al principio de una celda
            alignb 4
imp_lin     resb IMP_AN + 4
esc_buf     resb ESC_TAM + 4
        section .text
