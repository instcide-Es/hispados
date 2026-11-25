; =============================================================================
;  CUENTAS - los archivos: el propio (.CTA), CSV, DIF e imprimir
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  .CTA, el propio: "HCTA", la versión (1), los anchos que no son los de
;  siempre (cuántos, y columna y ancho de cada uno) y las celdas por orden:
;  fila (palabra), columna, tipo, formato y, para un número, sus 8 octetos;
;  para un texto o una fórmula, el largo y las letras. Acaba en FFFFh. Un
;  número ocupa 13 octetos; «=SUMA(A1:A9)», 18.
;
;  .CSV: una fila por línea, las celdas separadas por «;» y lo que tienen tal
;  cual se escribe (las fórmulas, con su «=»), como Cuentas de Hispa Ventanas
;  y las hojas de hoy en español. Al leer, el separador sale de la primera
;  línea («;», tabulador o «,»), y las comillas se respetan.
;
;  .DIF: el formato de intercambio de VisiCalc (Software Arts, 1980): valores,
;  no fórmulas, con punto decimal.
; =============================================================================

; ---- leer: abre DX; CF si no ----
abre_lee:
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [fh], ax
        xor ax, ax
        mov [rb_n], ax
        mov [rb_p], ax
.r:
        ret
; AL = la letra siguiente; CF al acabar
lee_c:
        push bx
        push cx
        push dx
        push si
        mov si, [rb_p]
        cmp si, [rb_n]
        jb .hay
        mov bx, [fh]
        mov cx, 512
        mov dx, iobuf
        mov ah, 0x3F
        int 0x21
        jc .fin
        test ax, ax
        jz .fin
        mov [rb_n], ax
        xor si, si
.hay:
        mov al, [iobuf + si]
        inc si
        mov [rb_p], si
        clc
        jmp short .r
.fin:
        stc
.r:
        pop si
        pop dx
        pop cx
        pop bx
        ret
; CX octetos a DS:DI; CF si se acaba antes
lee_n:
        clc
        jcxz .r
.l:
        call lee_c
        jc .r
        mov [di], al
        inc di
        loop .l
.r:
        ret
cierra:
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        ret

; ---- escribir: crea DX (o abre un dispositivo); CF si no ----
crea_esc:
        mov word [wb_n], 0
        mov byte [w_mal], 0
        push dx
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        pop dx
        jnc .ok
        mov ax, 0x3D01
        int 0x21
        jc .r
.ok:
        mov [fh], ax
.r:
        ret
esc_c:
        push bx
        mov bx, [wb_n]
        mov [iobuf + bx], al
        inc bx
        mov [wb_n], bx
        cmp bx, 512
        pop bx
        jb .r
        call vuelca
.r:
        ret
vuelca:
        push ax
        push bx
        push cx
        push dx
        mov cx, [wb_n]
        jcxz .x
        mov bx, [fh]
        mov dx, iobuf
        mov ah, 0x40
        int 0x21
        jc .mal
        cmp ax, cx
        je .x
.mal:
        mov byte [w_mal], 1
.x:
        mov word [wb_n], 0
        pop dx
        pop cx
        pop bx
        pop ax
        ret
esc_z:
        lodsb
        test al, al
        jz .r
        call esc_c
        jmp esc_z
.r:
        ret
esc_nl:
        mov al, 13
        call esc_c
        mov al, 10
        jmp esc_c
; AX en decimal
esc_dec:
        push ds
        pop es
        mov di, avbuf
        call pon_dec
        mov byte [di], 0
        mov si, avbuf
        jmp esc_z
cierra_esc:
        call vuelca
        call cierra
        cmp byte [w_mal], 1
        cmc
        ret

; ---- la extensión de `nombre`: SI detrás del punto (CF si no hay) y DI al
; final del nombre ----
extension:
        push ds
        pop es
        mov di, nombre
        xor si, si
.l:
        mov al, [di]
        test al, al
        jz .f
        cmp al, '.'
        jne .a
        mov si, di
        inc si
.a:
        cmp al, '\'
        jne .b
        xor si, si
.b:
        inc di
        jmp .l
.f:
        test si, si
        jnz .r
        stc
.r:
        ret
; AL = 0 propio, 1 CSV, 2 DIF
que_formato:
        call extension
        mov al, 0
        jc .r
        cmp word [si], 'CS'
        jne .d
        cmp byte [si+2], 'V'
        jne .r
        mov al, 1
.d:
        cmp word [si], 'DI'
        jne .r
        cmp byte [si+2], 'F'
        jne .r
        mov al, 2
.r:
        ret

; =============================================================================
;  Abrir: la hoja de `nombre` (la que había ya se ha vaciado). CF si no
; =============================================================================
carga:
        mov dx, nombre
        call abre_lee
        jc .r
        mov byte [ud_graba], 0
        mov byte [sin_calc], 1
        call que_formato
        cmp al, 1
        jb .cta
        je .csv
        call carga_dif
        jmp short .fin
.csv:
        call carga_csv
        jmp short .fin
.cta:
        call carga_cta
.fin:
        pushf
        call cierra
        mov byte [sin_calc], 0
        call recalcula_todo
        mov byte [ud_graba], 1
        mov byte [ud_hay], 0
        mov byte [cambios], 0
        mov byte [sucio], 2
        popf
        jnc .r
        mov byte [nombre], 0        ; a medias: sin nombre y con cambios, para
        mov byte [cambios], 1       ; que no se guarde encima sin querer (y
        stc                         ; que Salir pregunte)
.r:
        ret

carga_cta:
        mov di, ebuf
        mov cx, 6
        call lee_n
        jc .r
        cmp word [ebuf], 'HC'
        jne .mal
        cmp word [ebuf+2], 'TA'
        jne .mal
        mov cl, [ebuf+5]            ; los anchos
        xor ch, ch
        jcxz .celdas
.a:
        call lee_c
        jc .r
        mov bl, al
        call lee_c
        jc .r
        xor bh, bh
        mov [anch + bx], al
        loop .a
.celdas:
        mov di, nr
        mov cx, 2
        call lee_n
        jc .r
        cmp word [nr], 0xFFFF
        je .bien
        mov di, nr + 2
        mov cx, 3
        call lee_n
        jc .r
        mov al, [nr + R_TIPO]
        and al, 3
        mov [nr + R_TIPO], al
        cmp al, T_NUM
        jne .t
        mov byte [nr + R_LEN], 0
        mov di, nr + R_VAL
        mov cx, 8
        call lee_n
        jc .r
        mov cx, 14
        jmp short .pon
.t:
        call lee_c
        jc .r
        mov [nr + R_LEN], al
        mov cl, al
        xor ch, ch
        mov di, nr + 6
        cmp byte [nr + R_TIPO], T_FORM
        jne .t1
        mov di, nr + 20
.t1:
        push cx
        call lee_n
        pop cx
        jc .r
        add cx, 6
        cmp byte [nr + R_TIPO], T_FORM
        jne .pon
        push cx
        or byte [nr + R_TIPO], PENDIENTE
        call caja_nr
        pop cx
        add cx, 14
.pon:
        mov dx, [nr]
        mov bl, [nr + 2]
        cmp dx, NFIL
        jae .mal
        mov si, nr
        call pon_celda
        jc .r
        jmp .celdas
.bien:
        clc
.r:
        ret
.mal:
        stc
        ret

; ---- CSV ----
carga_csv:
        call lee_c                  ; el separador, de la primera línea
        jc .vacio
        dec word [rb_p]
        mov byte [csv_sep], ','
        xor si, si
.s:
        cmp si, [rb_n]
        jae .s3
        mov al, [iobuf + si]
        inc si
        cmp al, 10
        je .s3
        cmp al, 9
        jne .s1
        mov byte [csv_sep], 9
.s1:
        cmp al, ';'
        jne .s
        mov byte [csv_sep], ';'
.s3:
        xor ax, ax
        mov [en_f2], ax
        mov [en_c2], al
.campo:
        xor bx, bx                  ; BX = letras del campo
        mov byte [csv_com], 0
        mov byte [csv_par], 0       ; (paréntesis abiertos de una fórmula)
.c:
        call lee_c
        jc .ultimo
        cmp byte [csv_com], 1
        je .dentro
        cmp al, '"'
        jne .c1
        test bx, bx
        jnz .c1
        mov byte [csv_com], 1
        jmp .c
.c1:
        cmp al, 13
        je .c
        cmp byte [ebuf], '='        ; en una fórmula, el «;» de dentro de
        jne .c2                     ; un paréntesis no separa
        test bx, bx
        jz .c2
        cmp al, '('
        jne .c3
        inc byte [csv_par]
.c3:
        cmp al, ')'
        jne .c4
        dec byte [csv_par]
.c4:
        cmp byte [csv_par], 0
        jg .mete
.c2:
        cmp al, [csv_sep]
        je .fin_campo
        cmp al, 10
        je .fin_linea
.mete:
        cmp bx, LIN_MAX
        jae .c
        mov [ebuf + bx], al
        inc bx
        jmp .c
.dentro:
        cmp al, '"'
        jne .mete
        call lee_c                  ; «""» es una comilla; si no, se acaban
        jc .ultimo
        cmp al, '"'
        je .mete
        mov byte [csv_com], 2
        dec word [rb_p]
        jmp .c
.fin_campo:
        call pon_campo
        inc byte [en_c2]
        jmp .campo
.fin_linea:
        call pon_campo
        mov byte [en_c2], 0
        inc word [en_f2]
        cmp word [en_f2], NFIL
        jb .campo
        clc
        ret
.ultimo:
        call pon_campo
.vacio:
        clc
        ret
pon_campo:
        mov byte [ebuf + bx], 0
        test bx, bx
        jz .r
        mov dx, [en_f2]
        mov bl, [en_c2]
        call entra
        jnc .r
        mov byte [en_texto], 1      ; una fórmula que no entendemos: texto
        mov dx, [en_f2]
        mov bl, [en_c2]
        call entra
        mov byte [en_texto], 0
.r:
        ret

guarda_csv:
        xor ax, ax
        mov [en_f2], ax             ; la fila y la columna por donde va
        mov [en_c2], al
        xor si, si
.l:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .fin
        push si
        call del_indice
        mov dx, [es:di]
        mov bl, [es:di+2]
.f:
        cmp dx, [en_f2]
        je .c
        call esc_nl
        inc word [en_f2]
        mov byte [en_c2], 0
        jmp .f
.c:
        cmp bl, [en_c2]
        je .campo
        mov al, ';'
        call esc_c
        inc byte [en_c2]
        jmp .c
.campo:
        call texto_edicion
        mov si, ebuf3               ; ¿hace falta entrecomillarlo?
.q:
        lodsb
        test al, al
        jz .tal
        cmp al, ';'
        je .comillas
        cmp al, '"'
        jne .q
.comillas:
        mov al, '"'
        call esc_c
        mov si, ebuf3
.q1:
        lodsb
        test al, al
        jz .q2
        call esc_c
        cmp al, '"'
        jne .q1
        call esc_c
        jmp .q1
.q2:
        mov al, '"'
        call esc_c
        jmp short .sig
.tal:
        mov si, ebuf3
        call esc_z
.sig:
        pop si
        inc si
        inc si
        jmp .l
.fin:
        cmp word [n_cel], 0
        je .r
        call esc_nl
.r:
        ret

; ---- DIF ----
guarda_dif:
        call rango_usado
        mov si, d_tabla
        call esc_z
        mov al, [q_c2]
        xor ah, ah
        inc ax
        call esc_dec
        mov si, d_tuplas
        call esc_z
        mov ax, [q_f2]
        inc ax
        call esc_dec
        mov si, d_datos
        call esc_z
        xor dx, dx
.f:
        push dx
        mov si, d_bot
        call esc_z
        pop dx
        xor bl, bl
.c:
        push dx
        push bx
        call busca
        jc .vacia
        mov al, [es:di+R_TIPO]
        and al, 3
        jz .texto
        call valor_a_acc
        cmp word [acc+6], 0x8000
        je .error
        mov si, d_num
        call esc_z
        push ds
        pop es
        mov di, ebuf3
        mov cx, 0x00FF
        call num_txt
        mov byte [di], 0
        mov si, ebuf3
.n:
        lodsb
        test al, al
        jz .n2
        cmp al, ','
        jne .n1
        mov al, '.'
.n1:
        call esc_c
        jmp .n
.n2:
        mov si, d_v
        call esc_z
        jmp short .sig
.error:
        mov si, d_err
        call esc_z
        jmp short .sig
.texto:
        call celda_txt0             ; el texto, sin el prefijo
        jmp short .t
.vacia:
        mov byte [dtx], 0
        xor cx, cx
.t:
        mov si, d_txt
        call esc_z
        mov al, '"'
        call esc_c
        mov si, dtx
        jcxz .t2
.t1:
        lodsb
        call esc_c
        cmp al, '"'
        jne .t1b
        call esc_c
.t1b:
        loop .t1
.t2:
        mov al, '"'
        call esc_c
        call esc_nl
.sig:
        pop bx
        pop dx
        cmp bl, [q_c2]
        je .fs
        inc bl
        jmp .c
.fs:
        cmp dx, [q_f2]
        je .fin
        inc dx
        jmp .f
.fin:
        mov si, d_eod
        jmp esc_z
; el texto del registro ES:DI en dtx (CX letras), como lo ve celda_txt
celda_txt0:
        mov dx, [es:di]
        mov bl, [es:di+2]
        call celda_txt
        ret

carga_dif:
.cab:
        call lee_linea              ; la cabecera, hasta DATA
        jc .r
        cmp word [ebuf], 'DA'
        jne .cab
        cmp word [ebuf+2], 'TA'
        jne .cab
        call lee_linea
        call lee_linea
        mov word [en_f2], 0xFFFF
        mov byte [en_c2], 0
.l:
        call lee_linea              ; «tipo,número» y luego el texto
        jc .bien
        push ds
        pop es
        mov si, ebuf
        mov di, ebuf2
        call copia_cad
        call lee_linea
        jc .bien
        mov si, ebuf2
        call blancos
        cmp al, '-'
        je .especial
        cmp al, '1'
        je .texto
        ; un número: 0,valor y V (o TRUE, FALSE, NA, ERROR)
        cmp word [ebuf], 'TR'
        je .uno
        cmp word [ebuf], 'FA'
        je .cero
        cmp byte [ebuf], 'V'
        jne .otro
        mov si, ebuf2
.coma:
        lodsb
        test al, al
        jz .otro
        cmp al, ','
        jne .coma
        call lee_num_dif
        jc .otro
        jmp short .num
.uno:
        mov ax, 1
        call pon_ent
        jmp short .num
.cero:
        call acc_cero
.num:
        mov ax, [en_f2]
        mov [en_f], ax
        mov al, [en_c2]
        mov [en_c], al
        mov byte [en_fmt], 0
        mov al, T_NUM
        call nr_cabeza
        mov si, acc
        mov di, nr + R_VAL
        call copia8
        mov cx, 14
        call pon_dif
.otro:
        inc byte [en_c2]
        jmp .l
.texto:
        mov ax, [en_f2]
        mov [en_f], ax
        mov al, [en_c2]
        mov [en_c], al
        mov byte [en_fmt], 0
        mov al, T_TEXTO
        call nr_cabeza
        mov si, ebuf                ; sin las comillas («""» es una)
        cmp byte [si], '"'
        jne .t0
        inc si
.t0:
        mov di, nr + 6
        xor cx, cx
.t1:
        lodsb
        test al, al
        jz .t3
        cmp al, '"'
        jne .t2
        lodsb
        cmp al, '"'
        jne .t3
.t2:
        stosb
        inc cx
        cmp cx, LIN_MAX
        jb .t1
.t3:
        jcxz .otro0
        mov [nr + R_LEN], cl
        add cx, 6
        call pon_dif
.otro0:
        jmp .otro
.especial:
        cmp word [ebuf], 'BO'       ; BOT: otra fila
        jne .eod
        inc word [en_f2]
        mov byte [en_c2], 0
        cmp word [en_f2], NFIL
        jb .l
.eod:
.bien:
        clc
.r:
        ret
pon_dif:
        mov dx, [en_f]
        cmp dx, NFIL
        jae .r
        mov bl, [en_c]
        mov si, nr
        call pon_celda
.r:
        ret
; un número de DIF: punto decimal, y quizá un exponente (1.5E+03)
lee_num_dif:
        call blancos
        mov byte [en_neg], 0
        cmp al, '-'
        jne .p
        mov byte [en_neg], 1
        inc si
.p:
        cmp byte [si], '+'
        jne .n
        inc si
.n:
        mov byte [nmodo], 2
        call lee_num
        jc .r
        mov al, [si]
        and al, 0xDF
        cmp al, 'E'
        jne .s
        inc si
        mov dl, 0
        cmp byte [si], '+'
        je .e1
        cmp byte [si], '-'
        jne .e2
        mov dl, 1
.e1:
        inc si
.e2:
        push dx
        call ent_corto
        pop dx
        jc .s
        mov cx, ax
        jcxz .s
        cmp cx, 20
        ja .s
.e3:
        push cx
        push dx
        mov bx, 10
        test dl, dl
        jz .por
        call divr
        jmp short .e4
.por:
        mov si, acc
        mov cx, 4
        call muln
.e4:
        pop dx
        pop cx
        loop .e3
.s:
        cmp byte [en_neg], 0
        je .ok
        mov si, acc
        call neg64
.ok:
        clc
.r:
        ret

; una línea a ebuf (sin el fin de línea); CF si ya no hay nada
lee_linea:
        xor bx, bx
.l:
        call lee_c
        jc .fin
        cmp al, 13
        je .l
        cmp al, 10
        je .f
        cmp bx, 254
        jae .l
        mov [ebuf + bx], al
        inc bx
        jmp .l
.fin:
        test bx, bx
        jnz .f
        stc
        ret
.f:
        mov byte [ebuf + bx], 0
        clc
        ret

d_tabla     db "TABLE", 13, 10, "0,1", 13, 10, '""', 13, 10, "VECTORS", 13, 10, "0,", 0
d_tuplas    db 13, 10, '""', 13, 10, "TUPLES", 13, 10, "0,", 0
d_datos     db 13, 10, '""', 13, 10, "DATA", 13, 10, "0,0", 13, 10, '""', 13, 10, 0
d_bot       db "-1,0", 13, 10, "BOT", 13, 10, 0
d_eod       db "-1,0", 13, 10, "EOD", 13, 10, 0
d_num       db "0,", 0
d_v         db 13, 10, "V", 13, 10, 0
d_err       db "0,0", 13, 10, "ERROR", 13, 10, 0
d_txt       db "1,0", 13, 10, 0

; =============================================================================
;  Guardar `nombre` (en el formato de su extensión). CF si no se ha podido
; =============================================================================
; Si el archivo ya existe, se escribe en NOMBRE.$$$ y solo cuando está
; entero se borra el viejo y el temporal pasa a llamarse como él: con el disco
; lleno, el viejo queda como estaba. Si no existe (o es un dispositivo), se
; escribe directamente, y si falla se borra lo escrito
guarda:
        mov si, nombre              ; nombre3: el temporal (la extensión, $$$)
        mov di, nombre3
        call copia_cad
        call extension
        jnc .e
        lea si, [di+1]              ; (sin extensión: se le pone)
        mov byte [di + nombre3 - nombre], '.'
.e:
        add si, nombre3 - nombre
        mov word [si], '$$'
        mov word [si+2], '$'
        mov dx, nombre
        mov ax, 0x4300              ; ¿existe ya (y es un archivo)?
        int 0x21
        jnc .tmp
        mov word [g_nom], nombre
        jmp short .crea
.tmp:
        mov word [g_nom], nombre3
.crea:
        mov dx, [g_nom]
        call crea_esc
        jc .r
        call .escribe
        mov dx, [g_nom]
        jc .borra                   ; no ha salido: fuera lo escrito
        cmp dx, nombre
        je .r                       ; (era nuevo: ya está)
        mov dx, nombre              ; el viejo, fuera, y el nuevo, en su sitio
        mov ah, 0x41
        int 0x21
        jc .borra_tmp
        mov dx, nombre3
        mov di, nombre
        mov ah, 0x56
        int 0x21
        ret
.borra_tmp:
        mov dx, nombre3
.borra:
        mov ah, 0x41
        int 0x21
        stc
.r:
        ret
.escribe:
        call que_formato
        cmp al, 1
        jb .cta
        je .csv
        call guarda_dif
        jmp short .fin
.csv:
        call guarda_csv
        jmp short .fin
.cta:
        call guarda_cta
.fin:
        jmp cierra_esc

guarda_cta:
        mov si, c_cab
        mov cx, 5
.h:
        lodsb
        call esc_c
        loop .h
        xor bx, bx                  ; los anchos que no son los de siempre
        xor cx, cx
.a:
        cmp byte [anch + bx], ANCHO0
        je .a1
        inc cx
.a1:
        inc bl
        jnz .a
        cmp cx, 255
        jbe .a2
        mov cx, 255
.a2:
        mov al, cl
        call esc_c
        xor bx, bx
.a3:
        jcxz .celdas
        cmp byte [anch + bx], ANCHO0
        je .a4
        mov al, bl
        call esc_c
        mov al, [anch + bx]
        call esc_c
        dec cx
.a4:
        inc bl
        jnz .a3
.celdas:
        xor si, si
.l:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .fin
        push si
        call del_indice             ; el registro, a nr
        call tam_rec
        push ds
        mov si, di
        mov di, nr
        push es
        push ds
        pop es
        pop ds
        rep movsb
        pop ds
        mov si, nr                  ; fila, columna, tipo, formato
        mov cx, 5
        mov al, [nr + R_TIPO]
        and al, 3
        mov [nr + R_TIPO], al
.c:
        lodsb
        call esc_c
        loop .c
        cmp byte [nr + R_TIPO], T_NUM
        jne .t
        mov si, nr + R_VAL
        mov cx, 8
        jmp short .e
.t:
        mov al, [nr + R_LEN]
        call esc_c
        mov cl, al
        xor ch, ch
        mov si, nr + 6
        cmp byte [nr + R_TIPO], T_FORM
        jne .e
        mov si, nr + 20
.e:
        jcxz .sig
.e1:
        lodsb
        call esc_c
        loop .e1
.sig:
        pop si
        inc si
        inc si
        jmp .l
.fin:
        mov al, 0xFF
        call esc_c
        jmp esc_c
c_cab       db "HCTA", 1

; =============================================================================
;  Imprimir el rango q_ en el archivo (o dispositivo) DX, como se ve
; =============================================================================
imprime:
        call crea_esc
        jc .r
        mov byte [en_papel], 1
        mov dx, [q_f1]
.f:
        push dx
        mov bl, [q_c1]
        mov bh, [q_c2]
        mov cx, 1024                ; (una impresora ancha, o un archivo)
        call construye
        mov cx, [ltx_n]             ; sin los blancos del final
        mov si, ltx
        add si, cx
.b:
        jcxz .e
        dec si
        cmp byte [si], ' '
        jne .e
        dec cx
        jmp .b
.e:
        mov si, ltx
        jcxz .nl
.e1:
        lodsb
        call esc_c
        loop .e1
.nl:
        call esc_nl
        pop dx
        cmp dx, [q_f2]
        je .fin
        inc dx
        jmp .f
.fin:
        mov byte [en_papel], 0
        jmp cierra_esc
.r:
        ret
