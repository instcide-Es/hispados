; =============================================================================
;  Hispa-BASIC - el sonido por el altavoz del PC: SOUND y PLAY
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El temporizador 8253/8254 (canal 2) da la nota; el puerto 61h la deja
;  pasar al altavoz. Las duraciones se cuentan con el reloj de la BIOS
;  (0040:006C, 18,2 tics por segundo), como en GW-BASIC.
;    SOUND frecuencia, duración      (37 a 32767 Hz; la duración en tics)
;    PLAY "órdenes":  A-G (con #, + o -, un largo y puntos)  O n  < >
;                     L n (largo: 1 redonda, 4 negra...)  T n (negras por
;                     minuto)  P n o R n (silencio)  N n (nota 0-84)
;                     MN ML MS (normal, ligado, picado)  MF MB (se aceptan)
; =============================================================================

; ---- el altavoz a la frecuencia AX (Hz); AX = 0: callado ----
suena:
        push ax
        push dx
        push bx
        test ax, ax
        jz .calla
        cmp ax, 37
        jb .calla
        mov bx, ax
        mov dx, 0x0012              ; 1193180 / frecuencia
        mov ax, 0x34DC
        div bx
        push ax
        mov al, 0xB6
        out 0x43, al
        pop ax
        out 0x42, al
        mov al, ah
        out 0x42, al
        in al, 0x61
        or al, 3
        out 0x61, al
        jmp .x
.calla:
        in al, 0x61
        and al, 0xFC
        out 0x61, al
.x:
        pop bx
        pop dx
        pop ax
        ret
; ---- espera AX tics del reloj (y deja de esperar con Ctrl+Inter) ----
espera_tics:
        push es
        push bx
        push cx
        xor bx, bx
        mov es, bx
        mov cx, ax
        jcxz .x
.t:
        mov bx, [es:0x046C]
.e:
        cmp byte [v_corte], 0
        jne .x
        cmp bx, [es:0x046C]
        je .e
        loop .t
.x:
        pop cx
        pop bx
        pop es
        ret

; ---- SOUND f, d ----
sent_sound:
        call eval_ent
        push ax
        ESPERA ','
        call eval_num
        ; la duración puede llevar decimales: se redondea a tics
        mov di, ac
        call a_entero
        mov cx, ax
        pop ax
        test ax, ax
        jle .mal
        jcxz .calla
        call suena
        mov ax, cx
        call espera_tics
.calla:
        xor ax, ax
        jmp suena
.mal:
        jmp err_funcion

; ---- PLAY "música" ----
sent_play:
        call eval_cad               ; BX
        push si
        mov cl, [bx]
        xor ch, ch
        mov si, [bx + 1]
        add cx, si
        mov [v_p_fin], cx
        cmp byte [v_p_listo], 0
        jne .o
        mov byte [v_p_listo], 1     ; los de serie: O4, L4, T120, MN
        mov byte [v_p_oct], 4
        mov word [v_p_largo], 4
        mov word [v_p_tempo], 120
        mov byte [v_p_estilo], 7
.o:
        cmp si, [v_p_fin]
        jae .x
        cmp byte [v_corte], 0
        jne .x
        lodsb
        call mayus
        cmp al, 'A'
        jb .no_nota
        cmp al, 'G'
        ja .no_nota
        ; una nota: su semitono en la octava
        sub al, 'A'
        mov bl, al
        xor bh, bh
        mov al, [cs:p_semitono + bx]
        cbw
        mov dx, ax
        call p_mira
        cmp al, '#'
        je .sost
        cmp al, '+'
        je .sost
        cmp al, '-'
        jne .nota
        dec dx
        inc si
        jmp .nota
.sost:
        inc dx
        inc si
.nota:
        ; nota absoluta = octava*12 + semitono
        mov al, [v_p_oct]
        mov ah, 12
        mul ah
        add ax, dx
        mov dx, ax
        call p_num                  ; un largo propio (0: el de L)
        test ax, ax
        jnz .larg
        mov ax, [v_p_largo]
.larg:
        call p_toca                 ; DX = la nota, AX = el largo
        jmp .o
.no_nota:
        cmp al, 'O'
        jne .oct_mas
        call p_num
        cmp ax, 6
        ja .mal
        mov [v_p_oct], al
        jmp .o
.oct_mas:
        cmp al, '>'
        jne .oct_men
        cmp byte [v_p_oct], 6
        jae .o
        inc byte [v_p_oct]
        jmp .o
.oct_men:
        cmp al, '<'
        jne .l
        cmp byte [v_p_oct], 0
        je .o
        dec byte [v_p_oct]
        jmp .o
.l:
        cmp al, 'L'
        jne .t
        call p_num
        test ax, ax
        jz .mal
        cmp ax, 64
        ja .mal
        mov [v_p_largo], ax
        jmp .o
.t:
        cmp al, 'T'
        jne .p
        call p_num
        cmp ax, 32
        jb .mal
        cmp ax, 255
        ja .mal
        mov [v_p_tempo], ax
        jmp .o
.p:
        cmp al, 'P'
        je .silencio
        cmp al, 'R'
        jne .n
.silencio:
        call p_num
        test ax, ax
        jnz .s1
        mov ax, [v_p_largo]
.s1:
        mov dx, -1                  ; (sin nota)
        call p_toca
        jmp .o
.n:
        cmp al, 'N'
        jne .m
        call p_num
        cmp ax, 84
        ja .mal
        mov dx, ax
        dec dx                      ; (N1 = la primera C; N0 = silencio)
        test ax, ax
        jnz .n1
        mov dx, -1
.n1:
        mov ax, [v_p_largo]
        call p_toca
        jmp .o
.m:
        cmp al, 'M'
        jne .resto
        cmp si, [v_p_fin]
        jae .x
        lodsb
        call mayus
        mov ah, 7
        cmp al, 'N'
        je .m_e
        mov ah, 8
        cmp al, 'L'
        je .m_e
        mov ah, 6
        cmp al, 'S'
        je .m_e
        cmp al, 'F'                 ; MF / MB: siempre en primer plano
        je .o
        cmp al, 'B'
        je .o
        jmp .mal
.m_e:
        mov [v_p_estilo], ah
        jmp .o
.resto:
        cmp al, ' '
        je .o
        cmp al, ';'
        je .o
.mal:
        xor ax, ax
        call suena
        pop si
        jmp err_funcion
.x:
        xor ax, ax
        call suena
        pop si
        ret

; ---- toca la nota DX (0 = el primer do; -1 = silencio) con el largo AX
; (1 redonda, 4 negra...), con los puntos que sigan ----
p_toca:
        ; tics = 4 * 60 * 18,2 / (tempo * largo) = 4369 / (tempo*largo)
        push dx
        mov bx, ax
        mov ax, [v_p_tempo]
        mul bx
        mov bx, ax
        mov ax, 4369
        xor dx, dx
        div bx
        mov cx, ax                  ; los tics
        ; con resto, redondear
        shl dx, 1
        cmp dx, bx
        jb .r
        inc cx
.r:
        ; los puntillos: x 1,5 cada uno
.pt:
        call p_mira
        cmp al, '.'
        jne .pt_x
        inc si
        mov ax, cx
        shr ax, 1
        add cx, ax
        jmp .pt
.pt_x:
        test cx, cx
        jnz .c1
        inc cx
.c1:
        pop dx
        cmp dx, -1
        je .silencio
        ; la frecuencia de la nota
        mov ax, dx
        mov bl, 12
        div bl                      ; AL = octava, AH = semitono
        push ax
        mov bl, ah
        xor bh, bh
        shl bx, 1
        mov ax, [cs:p_frec6 + bx]   ; en la octava 6
        pop bx                      ; BL = octava
        mov dl, 6
        sub dl, bl
        jbe .f
        push cx
        mov cl, dl
        shr ax, cl
        pop cx
.f:
        ; suena el estilo (7/8, 8/8 o 3/4 del tiempo) y calla el resto
        push ax
        call suena
        mov ax, cx
        mov bl, [v_p_estilo]
        xor bh, bh
        mul bx
        mov bx, 8
        div bx
        test ax, ax
        jnz .f1
        inc ax
.f1:
        push ax
        call espera_tics
        pop ax
        sub cx, ax
        pop ax
        xor ax, ax
        call suena
        mov ax, cx
        jmp espera_tics
.silencio:
        xor ax, ax
        call suena
        mov ax, cx
        jmp espera_tics
; el carácter siguiente (sin pasarlo); 0 al final
p_mira:
        xor al, al
        cmp si, [v_p_fin]
        jae .x
        mov al, [si]
.x:
        ret
; un número de la cadena (0 si no hay)
p_num:
        xor ax, ax
.c:
        cmp si, [v_p_fin]
        jae .x
        mov bl, [si]
        sub bl, '0'
        jb .x
        cmp bl, 9
        ja .x
        inc si
        mov bh, 0
        push dx
        mov dx, 10
        mul dx
        pop dx
        add ax, bx
        jmp .c
.x:
        ret
; A B C D E F G -> semitono desde C
p_semitono  db 9, 11, 0, 2, 4, 5, 7
; las frecuencias de la octava 6 (C6 a B6), en Hz; la octava 4 tiene La a 440
p_frec6     dw 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976
