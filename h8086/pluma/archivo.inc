; =============================================================================
;  PLUMA - abrir, guardar y exportar
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El formato propio, .PLU, es casi un texto: una cabecera de 128 octetos
;  (que empieza por «PLUMA86» y un 1Ah, para que TYPE no enseñe más) con los
;  márgenes, el encabezado y el pie, y detrás el texto en la página 858, con
;  CR LF al final de cada párrafo y los códigos de WordStar para el formato
;  (02h negrita, 13h subrayado, 19h cursiva) más los nuestros de párrafo.
;  Está entero en docs/8086/PLUMA.md.
;
;  Al guardar se escribe primero NOMBRE.$$$; si sale bien, el documento de
;  antes pasa a NOMBRE.BAK y el nuevo ocupa su sitio (como WordStar): un disco
;  lleno o un disquete que se saca a mitad no estropean lo que había.
; =============================================================================

; ---- lo que se escribe, a través de obuf ----
sal_a:                              ; BX = el archivo
        mov [s_h], bx
        mov word [s_n], 0
        mov byte [s_err], 0
        ret
sal_letra:
        push bx
        mov bx, [s_n]
        mov [obuf+bx], al
        inc bx
        mov [s_n], bx
        cmp bx, 512
        jb .x
        call sal_vacia
.x:
        pop bx
        ret
sal_vacia:
        push ax
        push bx
        push cx
        push dx
        mov cx, [s_n]
        jcxz .x
        mov bx, [s_h]
        mov dx, obuf
        DOS 0x40
        jc .mal
        cmp ax, cx
        je .x
.mal:
        mov byte [s_err], 1
.x:
        mov word [s_n], 0
        pop dx
        pop cx
        pop bx
        pop ax
        ret
sal_fin:                            ; CF=1 si algo ha fallado
        call sal_vacia
        cmp byte [s_err], 1
        cmc
        ret
sal_txt:                            ; el texto de SI (acabado en 0)
        lodsb
        test al, al
        jz .x
        call sal_letra
        jmp sal_txt
.x:
        ret
sal_crlf:
        mov al, 13
        call sal_letra
        mov al, 10
        jmp sal_letra

; ---- los nombres ----
; a mayúsculas (el nombre del documento, o el de SI)
mayusculas_nombre:
        mov si, nombre
a_mayusculas:
.c:
        lodsb
        test al, al
        jz .x
        cmp al, 'a'
        jb .c
        cmp al, 'z'
        ja .c
        sub byte [si-1], 32
        jmp .c
.x:
        ret
; dónde empieza la extensión del nombre de SI: BX (en el 0 si no tiene)
extension:
        mov bx, 0
.c:
        lodsb
        cmp al, '.'
        jne .c1
        lea bx, [si-1]
.c1:
        cmp al, '\'
        je .s
        cmp al, ':'
        jne .c2
.s:
        xor bx, bx
.c2:
        test al, al
        jnz .c
        test bx, bx
        jnz .x
        lea bx, [si-1]
.x:
        ret
; si el nombre no tiene extensión, .PLU
con_extension:
        mov si, nombre
        call extension
        cmp byte [bx], 0
        jne .x
        mov di, bx
        mov si, e_plu
        call copia_z
        mov byte [di], 0
.x:
        ret
; DI = el nombre del documento con la extensión de SI (4 letras: «.BAK»)
otra_extension:
        push si
        mov si, nombre
        call extension
        mov si, nombre
.c:
        cmp si, bx
        je .e
        movsb
        jmp .c
.e:
        pop si
        call copia_z
        mov byte [di], 0
        ret
; ¿es un .TXT? (ZF=1 si lo es)
es_txt:
        mov si, nombre
        call extension
        mov si, bx
        mov di, e_txt
        mov cx, 5
        repe cmpsb
        ret

; =============================================================================
;  Un documento vacío
; =============================================================================
doc_vacio:
        call cierra_tmps
        xor ax, ax
        mov [pre], ax
        mov [post], ax
        mov [lenA], ax
        mov [lenA+2], ax
        mov [nB], ax
        mov [cpos], ax
        mov [cpos+2], ax
        mov [clen], ax
        mov [clen+2], ax
        mov [top_w], ax
        mov [top_k], ax
        mov [u_top], ax
        mov [clip_f], al
        call desmarca
        call pon_hueco
        mov bx, [bfin]
        mov byte [bx], 0            ; (el final del texto, para quien lo lea)
        mov byte [cambiado], 0
        mov si, cab_normal          ; la página de siempre
        mov di, cab
        mov cx, CAB_TAM
        rep movsb
        ret

; =============================================================================
;  Abrir: el archivo [nombre] (si no existe, es un documento nuevo)
; =============================================================================
carga:
        call doc_vacio
        mov byte [cg_ins], 0
        mov dx, nombre
        mov ax, 0x3D00
        int 0x21
        jnc .hay
        mov word [mensaje], m_nuevo
        ret
.hay:
        call carga_de
        jnc .entero
        mov byte [nombre], 0        ; a medias: sin nombre (Guardar pregunta
        mov byte [cambiado], 1      ; cuál, y no pisa el bueno) y con cambios
        jmp short .x
.entero:
        mov ax, 0                   ; el cursor al principio
        call mueve_hueco
        call asegura
        mov byte [cambiado], 0
        cmp byte [cg_modo], CG_WS
        jne .x
        mov si, e_plu               ; de WordStar: se guardará como .PLU
        mov di, tmp_nombre
        call otra_extension
        mov si, tmp_nombre
        mov di, nombre
        call copia_z
        mov byte [di], 0
        mov word [mensaje], m_de_ws
.x:
        mov word [u_top], 0
        ret

CG_TXT      equ 0
CG_PLU      equ 1
CG_WS       equ 2
; lee el archivo abierto AX y lo mete donde está el cursor, convirtiéndolo.
; Si [cg_ins] es 0 (abrir un documento), lo que no cabe en la memoria va a
; C, y la cabecera de un .PLU pasa a la del documento
carga_de:
        mov [cg_h], ax
        mov byte [u_apag], 1
        cmp byte [cg_ins], 0
        je .a
        mov byte [u_apag], 0
.a:
        mov byte [cg_aC], 0
        mov byte [cg_fin], 0
        mov byte [cg_corto], 0
        mov byte [cg_blando], 0
        mov byte [cg_ult], 10
        mov word [cv_n], 0
        call .lee
        jc .mal
        mov si, sec
        mov cx, ax
        ; ¿de qué es?
        mov byte [cg_modo], CG_PLU
        cmp cx, CAB_TAM
        jb .no_plu
        mov di, firma
        push si
        push cx
        mov cx, 8
        repe cmpsb
        pop cx
        pop si
        jne .no_plu
        cmp byte [cg_ins], 0
        jne .s
        push si
        push cx
        mov di, cab
        mov cx, CAB_TAM
        rep movsb
        call cab_valida
        pop cx
        pop si
.s:
        add si, CAB_TAM
        sub cx, CAB_TAM
        jmp short .conv
.no_plu:
        mov byte [cg_modo], CG_WS
        cmp byte [forzar_ws], 0
        jne .conv
        push si                     ; ¿retornos blandos (8Dh 8Ah)?
        push cx
        jcxz .ws1
        dec cx
        jcxz .ws1
.ws:
        cmp word [si], 0x8A8D
        je .ws2
        inc si
        loop .ws
.ws1:
        mov byte [cg_modo], CG_TXT
.ws2:
        pop cx
        pop si
.conv:
        jcxz .otro
.c:
        lodsb
        call convierte
        cmp byte [cg_fin], 0
        jne .acaba
        loop .c
.otro:
        call .lee
        jc .mal
        mov cx, ax
        mov si, sec
        test cx, cx
        jnz .c
.acaba:
        call entrega
        mov bx, [cg_h]
        DOS 0x3E
        mov byte [u_apag], 0
        mov si, m_e_corto
        cmp byte [cg_corto], 0
        jne .dice
        clc
        ret
.mal:
        mov bx, [cg_h]
        DOS 0x3E
        mov byte [u_apag], 0
        mov si, m_e_leer
.dice:
        call aviso
        stc
        ret
.lee:
        mov bx, [cg_h]
        mov cx, 512
        mov dx, sec
        DOS 0x3F
        ret
; la cabecera leída, con valores que se puedan usar
cab_valida:
        mov al, [cab+CAB_ANCHO]
        cmp al, 20
        jb .mal
        cmp al, 78
        ja .mal
        cmp byte [cab+CAB_LARGO], 20
        jb .mal
        cmp byte [cab+CAB_IMPR], 3
        ja .mal
        mov byte [cab+CAB_ENC+47], 0
        mov byte [cab+CAB_PIE+47], 0
        ret
.mal:
        mov si, cab_normal
        mov di, cab
        mov cx, CAB_TAM
        rep movsb
        ret
; un octeto del archivo (AL), a lo nuestro
convierte:
        cmp byte [cg_modo], CG_WS
        jne .nows
        cmp al, 0x8D                ; WordStar: el retorno blando es un espacio
        jne .w1
        mov byte [cg_blando], 1
        ret
.w1:
        cmp al, 0x8A
        jne .w2
        cmp byte [cg_blando], 0
        je .w2
        mov byte [cg_blando], 0
        cmp byte [cg_ult], ' '
        je .x
        cmp byte [cg_ult], 10
        je .x
        mov al, ' '
        jmp short .pon
.w2:
        mov byte [cg_blando], 0
        cmp al, 0xA0                ; el espacio blando (de justificar): uno
        jne .w3                     ; si hace falta, y nada si ya lo hay
        cmp byte [cg_ult], ' '
        je .x
        mov al, ' '
        jmp short .pon
.w3:
        and al, 0x7F                ; y el bit alto de las letras
.nows:
        cmp al, 13
        je .x
        cmp al, 0x1A
        je .fin
        cmp al, 32
        jae .pon
        cmp al, 9                   ; los de control que se quedan
        je .pon
        cmp al, 10
        je .pon
        cmp al, 12
        je .pon
        cmp byte [cg_modo], CG_TXT
        je .x
        cmp al, CODN
        je .pon
        cmp al, CODS
        je .pon
        cmp al, CODC
        je .pon
        cmp byte [cg_modo], CG_WS
        je .x
        call de_clase               ; (en un .PLU, también los de párrafo)
        test ah, 0x40
        jz .x
.pon:
        mov [cg_ult], al
        push bx
        mov bx, [cv_n]
        mov [cbuf+bx], al
        inc bx
        mov [cv_n], bx
        pop bx
        cmp word [cv_n], 512
        jb .x
        jmp entrega
.fin:
        mov byte [cg_fin], 1
.x:
        ret
; lo convertido (cbuf), al documento (o a C, si ya no cabe en la memoria)
entrega:
        push si
        push cx
        mov cx, [cv_n]
        jcxz .x
        cmp byte [cg_aC], 0
        jne .aC
        cmp byte [cg_ins], 0
        jne .mem
        mov ax, [pstart]
        sub ax, [g_ini]
        sub ax, cx
        jb .aC
        cmp ax, 2 * TROZO
        jb .aC
.mem:
        mov si, cbuf
        call inserta
        jc .no
        jmp short .x
.aC:
        mov byte [cg_aC], 1
        mov bx, hC
        call abre_tmp
        jc .no
        mov bx, [hC]
        mov dx, cbuf
        DOS 0x40
        jc .no
        add [clen], ax
        adc word [clen+2], 0
        cmp ax, cx
        je .x
.no:
        mov byte [cg_corto], 1      ; no ha cabido (ni en la memoria ni en C,
        mov byte [cg_fin], 1        ; con el disco lleno): no se lee más, y se
.x:                                 ; dirá
        mov word [cv_n], 0
        pop cx
        pop si
        ret

; ---- Archivo › Abrir y Nuevo ----
; si hay cambios, pregunta si se guardan. CF=1 si no hay que seguir
antes_de_dejar:
        cmp byte [cambiado], 0
        je .si
        mov si, m_cambios
        call pregunta
        cmp al, 'N'
        je .si
        cmp al, 'S'
        jne .no
        jmp guardar
.si:
        clc
        ret
.no:
        stc
        ret
abrir:
        call antes_de_dejar
        jc .x
        mov byte [tmp_nombre], 0
        mov si, m_abrir
        mov di, tmp_nombre
        mov cl, 64
        call pide
        jc .x
        cmp byte [tmp_nombre], 0
        je .x
        mov si, tmp_nombre
        mov di, nombre
        call copia_z
        mov byte [di], 0
        call mayusculas_nombre
        call con_extension
        call carga
.x:
        mov byte [filas_ok], 0
        ret
nuevo:
        call antes_de_dejar
        jc .x
        call doc_vacio
        mov byte [nombre], 0
.x:
        ret
; ^KR: un archivo, metido donde está el cursor
inserta_archivo:
        mov byte [tmp_nombre], 0
        mov si, m_leer
        mov di, tmp_nombre
        mov cl, 64
        call pide
        jc .x
        mov dx, tmp_nombre
        mov ax, 0x3D00
        int 0x21
        jc .no
        mov byte [cg_ins], 1
        call carga_de
        mov byte [cg_ins], 0
        mov byte [centrar], 1
.x:
        ret
.no:
        mov si, m_no_existe
        jmp aviso

; =============================================================================
;  Guardar
; =============================================================================
guardar:
        cmp byte [nombre], 0
        jne escribe_doc
guardar_como:
        mov si, nombre
        mov di, tmp_nombre
        call copia_z
        mov byte [di], 0
        mov si, m_guardar_como
        mov di, tmp_nombre
        mov cl, 64
        call pide
        jc .x
        cmp byte [tmp_nombre], 0
        je .no
        mov si, tmp_nombre
        mov di, nombre
        call copia_z
        mov byte [di], 0
        call mayusculas_nombre
        call con_extension
        jmp escribe_doc
.no:
        stc
.x:
        ret
; el documento entero a [nombre] (por NOMBRE.$$$ y con NOMBRE.BAK)
escribe_doc:
        xor ax, ax
        mov [e_ini], ax
        mov [e_ini+2], ax
        call total32
        mov [e_fin], ax
        mov [e_fin+2], dx
        mov si, e_tmp
        mov di, tmp_nombre
        call otra_extension
        call es_txt
        mov al, 0
        jne .p
        mov al, 1
.p:
        mov [e_txt_f], al
        mov dx, tmp_nombre
        call escribe_rango
        jc .x
        mov si, e_bak               ; el de antes, a .BAK
        mov di, n_bak
        call otra_extension
        mov dx, n_bak
        DOS 0x41
        mov dx, nombre
        mov di, n_bak
        DOS 0x56
        mov dx, tmp_nombre
        mov di, nombre
        DOS 0x56
        jc .mal
        mov byte [cambiado], 0
        mov word [mensaje], m_guardado
        clc
.x:
        ret
.mal:
        mov si, m_e_guardar
        call aviso
        stc
        ret
; ^KW (Edición › Escribir bloque): el bloque, a un archivo
escribe_bloque:
        call bloque
        jc .x
        call fija_bloque
        mov ax, [b_ini]
        mov [e_ini], ax
        mov ax, [b_ini+2]
        mov [e_ini+2], ax
        mov ax, [b_fin]
        mov [e_fin], ax
        mov ax, [b_fin+2]
        mov [e_fin+2], ax
        mov byte [tmp_nombre], 0
        mov si, m_escribir
        mov di, tmp_nombre
        mov cl, 64
        call pide
        jc .x
        mov si, tmp_nombre
        call a_mayusculas
        mov si, tmp_nombre          ; ¿.TXT?
        call extension
        mov si, bx
        mov di, e_txt
        mov cx, 5
        repe cmpsb
        mov al, 0
        jne .p
        mov al, 1
.p:
        mov [e_txt_f], al
        mov dx, tmp_nombre
        call escribe_rango
        jc .x
        mov word [mensaje], m_guardado
.x:
        ret
; escribe de [e_ini] a [e_fin] en el archivo DX (nuevo): con [e_txt_f], en
; texto de DOS (sin formato, página 437); si no, un .PLU. CF=1 si falla
escribe_rango:
        mov [e_nom], dx
        xor cx, cx
        DOS 0x3C
        jc .mal0
        mov bx, ax
        call sal_a
        cmp byte [e_txt_f], 0
        jne .t
        mov si, cab
        mov cx, CAB_TAM
.cab:
        lodsb
        call sal_letra
        loop .cab
.t:
        mov ax, [e_ini]
        mov dx, [e_ini+2]
        call lee_desde
.c:
        mov ax, [it_pos]            ; ¿hasta aquí?
        mov dx, [it_pos+2]
        cmp dx, [e_fin+2]
        jb .s
        ja .fin
        cmp ax, [e_fin]
        jae .fin
.s:
        call lee_letra
        jc .fin
        cmp al, 10
        je .nl
        cmp byte [e_txt_f], 0
        je .pon
        call es_codigo              ; en texto: sin códigos, y en la 437
        jc .c
        call a_dos
.pon:
        call sal_letra
        jmp .c
.nl:
        call sal_crlf
        jmp .c
.fin:
        call sal_fin
        pushf
        mov bx, [s_h]
        DOS 0x3E
        popf
        jnc .x
        mov dx, [e_nom]             ; no ha salido bien: fuera lo escrito
        DOS 0x41
.mal0:
        mov si, m_e_guardar
        call aviso
        stc
.x:
        ret
; la letra AL (de la 858) en la 437; si no está, sin tilde
a_dos:
        cmp al, 0x80
        jb .x
        push bx
        mov bx, a437 - 0x80
        mov ah, al
        xlat
        test al, al
        jnz .y
        mov al, ah
        mov bx, base - 0x80
        xlat
        test al, al
        jnz .y
        mov al, '?'
.y:
        pop bx
.x:
        ret

; =============================================================================
;  Exportar
; =============================================================================
; pregunta el nombre (el del documento con la extensión SI) y lo crea: el
; archivo, para sal_*. CF=1 si no
pide_export:
        mov di, tmp_nombre
        call otra_extension
        mov si, m_exportar
        mov di, tmp_nombre
        mov cl, 64
        call pide
        jc .x
        mov dx, tmp_nombre
        xor cx, cx
        DOS 0x3C
        jc .mal
        mov bx, ax
        call sal_a
        clc
.x:
        ret
.mal:
        mov si, m_e_guardar
        call aviso
        stc
        ret
fin_export:
        call sal_fin
        pushf
        mov bx, [s_h]
        DOS 0x3E
        popf
        mov word [mensaje], m_exportado
        jnc .x
        mov si, m_e_guardar
        call aviso
.x:
        mov byte [filas_ok], 0
        ret

; ---- texto de DOS: las líneas como salen en la página, en la 437 ----
exp_dos:
        mov si, e_txt
        call pide_export
        jc .x
        mov byte [c_modo], C_TEXTO
        call compone_doc
        jmp fin_export
.x:
        ret

; ---- texto de hoy: UTF-8, un párrafo por línea ----
exp_hoy:
        mov si, e_txt
        call pide_export
        jc .x
        xor ax, ax
        xor dx, dx
        call lee_desde
.c:
        call lee_letra
        jc .fin
        cmp al, 10
        je .nl
        cmp al, 12
        je .nl
        call es_codigo
        jc .c
        cmp al, 0x80
        jb .pon
        call unicode_de             ; AX = el carácter
        cmp ax, 0x800
        jb .dos
        push ax                     ; tres octetos: 1110xxxx 10xxxxxx 10xxxxxx
        mov cl, 12
        shr ax, cl
        or al, 0xE0
        call sal_letra
        pop ax
        push ax
        mov cl, 6
        shr ax, cl
        and al, 0x3F
        or al, 0x80
        call sal_letra
        pop ax
        jmp short .ult
.dos:
        push ax                     ; dos: 110xxxxx 10xxxxxx
        mov cl, 6
        shr ax, cl
        or al, 0xC0
        call sal_letra
        pop ax
.ult:
        and al, 0x3F
        or al, 0x80
.pon:
        call sal_letra
        jmp .c
.nl:
        call sal_crlf
        jmp .c
.fin:
        jmp fin_export
.x:
        ret
; AL (de la 858, 80h o más) a su número de Unicode: AX
unicode_de:
        push bx
        mov bl, al
        mov bh, 0
        sub bl, 0x80
        shl bx, 1
        mov ax, [unicode+bx]
        pop bx
        ret

; ---- RTF (el formato de Microsoft de 1987, que abre cualquier programa de
; hoy): A4, Courier de 12 puntos (10 letras por pulgada, como la impresora),
; los márgenes, la sangría, la alineación, el encabezado y el pie ----
exp_rtf:
        mov si, e_rtf
        call pide_export
        jc .x
        mov si, r_cab
        call sal_txt
        mov si, r_margl             ; los márgenes, en veinteavos de punto
        mov al, [cab+CAB_MIZQ]
        mov bx, 144
        call .medida
        mov si, r_margr             ; el derecho: lo que sobra del A4
        call sal_txt
        mov al, [cab+CAB_ANCHO]
        mov ah, 0
        mov cx, 144
        mul cx
        mov cx, 11906
        sub cx, [r_tmp]
        sub cx, ax
        jb .mr
        cmp cx, 360
        jae .mr1
.mr:
        mov cx, 360
.mr1:
        mov ax, cx
        call .valor
        mov si, r_margt
        mov al, [cab+CAB_MSUP]
        mov bx, 240
        call .medida
        mov si, r_margb
        mov al, [cab+CAB_MINF]
        mov bx, 240
        call .medida
        mov si, r_pgn
        mov al, [cab+CAB_PAG1]
        mov bx, 1
        call .medida
        mov si, r_encab             ; el encabezado y el pie
        mov bx, cab + CAB_ENC
        call .cabeza
        mov si, r_pie
        mov bx, cab + CAB_PIE
        call .cabeza
        xor ax, ax
        xor dx, dx
        call lee_desde
        mov byte [r_nuevo], 1
        mov byte [r_est], 0
.c:
        call lee_letra
        jc .fin
        cmp byte [r_nuevo], 0
        je .dentro
        call .parrafo
.dentro:
        cmp al, 10
        je .finp
        cmp al, 12
        je .pag
        cmp al, 9
        je .tab
        cmp al, 32
        jae .letra
        call de_clase
        test ah, 0x80
        jz .c
        and ah, 7                   ; negrita, subrayado o cursiva
        xor [r_est], ah
        mov bl, ah
        call .formato
        jmp .c
.tab:
        mov si, r_tab
        call sal_txt
        jmp .c
.letra:
        call rtf_letra
        jmp .c
.finp:
        call .cierra
        mov si, r_par
        call sal_txt
        mov byte [r_nuevo], 1
        jmp .c
.pag:
        call .cierra
        mov si, r_page
        call sal_txt
        mov byte [r_nuevo], 1
        jmp .c
.fin:
        call .cierra
        mov al, '}'
        call sal_letra
        jmp fin_export
.x:
        ret
; (internas) el principio de un párrafo: \pard y lo suyo
.parrafo:
        push ax
        mov byte [r_nuevo], 0
        mov si, r_pard
        call sal_txt
        ; los códigos de párrafo se leen aquí mismo (van al principio)
        pop ax
        mov byte [r_par_f], 0
.pp:
        cmp al, 32
        jae .pp_x
        call de_clase
        test ah, 0x40
        jz .pp_x
        cmp ah, 0x44
        jne .pp1
        add byte [r_par_f], 4
        jmp short .pp2
.pp1:
        and byte [r_par_f], 0xFC
        and ah, 3
        or [r_par_f], ah
.pp2:
        call lee_letra
        jnc .pp
        mov al, 10
.pp_x:
        push ax
        mov al, [r_par_f]
        and ax, 3
        jz .ps
        mov si, r_alin
        dec ax
        mov cl, 4
        mul cl
        add si, ax
        call sal_txt
.ps:
        mov al, [r_par_f]
        shr al, 1
        shr al, 1
        mov bx, 720
        mov si, r_li
        call .medida_si
        mov al, [cab+CAB_SANG1]
        mov bx, 144
        mov si, r_fi
        call .medida_si
        mov al, ' '
        call sal_letra
        pop ax
        ret
.medida_si:                         ; (solo si no es 0)
        test al, al
        jz .ms
        jmp .medida
.ms:
        ret
; cierra lo que quede puesto al final de un párrafo
.cierra:
        mov bl, [r_est]
        mov byte [r_est], 0
        test bl, 1
        jz .c1
        push bx
        mov bl, 1
        mov byte [r_est], 0
        call .formato
        pop bx
.c1:
        test bl, 2
        jz .c2
        push bx
        mov bl, 2
        call .formato
        pop bx
.c2:
        test bl, 4
        jz .c3
        mov bl, 4
        call .formato
.c3:
        ret
; el formato BL (1, 2 o 4), según esté ahora en [r_est]
.formato:
        mov si, r_b
        cmp bl, 1
        je .f1
        mov si, r_ul
        cmp bl, 2
        je .f1
        mov si, r_i
.f1:
        test [r_est], bl
        jnz .f2
        add si, 8                   ; (el de quitar, 8 octetos después)
.f2:
        jmp sal_txt
; «\palabra» (SI) y el número AL * BX
.medida:
        mov ah, 0
        mul bx
        mov [r_tmp], ax
        push ax
        call sal_txt
        pop ax
.valor:
        mov di, n_txt
        call num_a_texto
        mov byte [di], 0
        mov si, n_txt
        jmp sal_txt
; {\header \pard\qc texto\par}: el número de página donde haya «#»
.cabeza:
        cmp byte [bx], 0
        je .cb_x
        push bx
        call sal_txt
        pop si
.cb:
        lodsb
        test al, al
        jz .cb1
        cmp al, '#'
        jne .cb2
        push si
        mov si, r_campo
        call sal_txt
        pop si
        jmp .cb
.cb2:
        push si
        call rtf_letra
        pop si
        jmp .cb
.cb1:
        mov si, r_cierra
        jmp sal_txt
.cb_x:
        ret
; una letra en RTF: las tres especiales con \, y las de fuera del ASCII como
; \'hh (página 1252) o \uN? (lo que no esté en ella)
rtf_letra:
        cmp al, '\'
        je .esc
        cmp al, '{'
        je .esc
        cmp al, '}'
        je .esc
        cmp al, 0x80
        jae .alta
        jmp sal_letra
.esc:
        push ax
        mov al, '\'
        call sal_letra
        pop ax
        jmp sal_letra
.alta:
        call unicode_de
        cmp ax, 0x20AC              ; € es el 80h de la 1252
        jne .a1
        mov ax, 0x80
.a1:
        cmp ax, 0x100
        jae .u
        push ax
        mov al, '\'
        call sal_letra
        mov al, "'"
        call sal_letra
        pop ax
        push ax
        mov cl, 4
        shr al, cl
        call .hex
        pop ax
        and al, 15
.hex:
        add al, '0'
        cmp al, '9'
        jbe .h
        add al, 'a' - '9' - 1
.h:
        jmp sal_letra
.u:
        push ax
        mov al, '\'
        call sal_letra
        mov al, 'u'
        call sal_letra
        pop ax
        mov di, n_txt
        call num_a_texto
        mov byte [di], 0
        mov si, n_txt
        call sal_txt
        mov al, '?'
        jmp sal_letra
