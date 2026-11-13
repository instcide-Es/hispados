; =============================================================================
;  hspopl.inc - sintesis FM de la Sound Blaster emulada: OPL2 (YM3812, AdLib)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Nueve canales de dos operadores (en serie -FM- o sumados), cuatro formas
;  de onda, envolvente (ataque, caida, sostenido, suelta) con escala por
;  altura, nivel y atenuacion por altura (KSL), realimentacion, tremolo y
;  vibrato, y el modo de percusion simplificado (bombo de dos operadores;
;  caja, timbal, platillo y charles con un operador cada uno, los que llevan
;  ruido con la fase al azar).
;
;  Escrito desde cero con la documentacion publica del chip (hoja de datos y
;  manual de aplicacion del YM3812, y la descripcion de sus tablas de seno
;  en logaritmo y de exponencial); las tablas salen de sus formulas
;  (tools/mkopl.py). Se sintetiza a la frecuencia de la tarjeta (48 kHz), no
;  a los 49716 Hz del chip: la fase se corrige (fm_k); las envolventes van un
;  3 % mas lentas.
;
;  Simplificaciones: los pasos de envolvente se reparten de forma uniforme
;  (el chip sigue un patron de ocho); los temporizadores vencen en cuanto se
;  arrancan (basta para que los programas detecten la tarjeta); sin modo
;  CSM; el ruido de la percusion no sale de las fases de los operadores.
;
;  DS = CS (tablas), GS = bloque de estado.
; =============================================================================
%include "hspopl_tab.inc"

; operador de cada registro (los 5 bits bajos de 20h-35h, 40h-55h...)
fm_slot     db 0, 2, 4, 1, 3, 5, 0xFF, 0xFF, 6, 8, 10, 7, 9, 11, 0xFF, 0xFF
            db 12, 14, 16, 13, 15, 17, 0xFF, 0xFF
            times 8 db 0xFF
; percusion: bit del registro BDh, operador
fm_rhy      db 0x10, 12, 0x10, 13, 0x08, 15, 0x04, 16, 0x02, 17, 0x01, 14, 0

fm_reset:
        mov eax, 49716 << 16
        xor edx, edx
        movzx ecx, word [hw_rate]
        div ecx
        mov [gs:fm_k], eax
        mov dword [gs:fm_noise], 1
        mov si, fm_op
        xor cx, cx
.l:
        mov word [gs:si+O_ENV], 511
        mov al, cl
        shr al, 1
        mov [gs:si+O_CH], al
        add si, O_SIZE
        inc cx
        cmp cx, 18
        jb .l
        ret

; -> AL = registro de estado (los bits 1 y 2 a uno: es un OPL2)
fm_status:
        mov al, [gs:fm_stat]
        or al, 0x06
        ret

; AL = dato para el registro fm_addr
fm_write:
        movzx bx, byte [gs:fm_addr]
        cmp bl, 0x01
        je .r01
        cmp bl, 0x04
        je .timer
        cmp bl, 0xBD
        je .rbd
        cmp bl, 0x20
        jb .r
        cmp bl, 0xA0
        jb .oper
        cmp bl, 0xC9
        jb .chan
        cmp bl, 0xE0
        jb .r
        cmp bl, 0xF6
        jb .oper
.r:
        ret
.r01:
        mov [gs:fm_r01], al
        ret
; temporizadores: vencen al arrancar (si no estan tapados)
.timer:
        test al, 0x80
        jz .t
        mov byte [gs:fm_stat], 0
        ret
.t:
        test al, 0x01
        jz .t2
        test al, 0x40
        jnz .t2
        or byte [gs:fm_stat], 0xC0
.t2:
        test al, 0x02
        jz .r
        test al, 0x20
        jnz .r
        or byte [gs:fm_stat], 0xA0
        ret
; registros de un operador
.oper:
        mov di, bx
        and di, 0x1F
        movzx di, byte [fm_slot+di]
        cmp di, 0xFF
        je .r
        imul si, di, O_SIZE
        add si, fm_op
        shr bl, 5
        cmp bl, 1
        jne .o2
        mov [gs:si+O_R20], al
        jmp fm_opupd
.o2:
        cmp bl, 2
        jne .o3
        mov [gs:si+O_R40], al
        jmp fm_opupd
.o3:
        cmp bl, 3
        jne .o4
        mov [gs:si+O_R60], al
        jmp fm_opupd
.o4:
        cmp bl, 4
        jne .o7
        mov [gs:si+O_R80], al
        jmp fm_opupd
.o7:
        cmp bl, 7
        jne .r
        and al, 3
        mov [gs:si+O_WS], al
        ret
; registros de un canal: A0h frecuencia, B0h tecla y octava, C0h conexion
.chan:
        mov di, bx
        and di, 0x0F
        cmp di, 8
        ja .r
        imul si, di, 2 * O_SIZE
        add si, fm_op               ; su primer operador
        shl di, 2
        add di, fm_ch
        cmp bl, 0xB0
        jb .a0
        cmp bl, 0xC0
        jb .b0
        mov [gs:di+C_C0], al
        ret
.a0:
        mov [gs:di+C_FNUM], al
        jmp .both
.b0:
        mov ah, [gs:di+C_B0]
        mov [gs:di+C_B0], al
        xor ah, al                  ; AH bit 5: la tecla ha cambiado
        push ax
        and al, 3
        mov [gs:di+C_FNUM+1], al
        call .both
        pop ax
        test ah, 0x20
        jz .r
        test al, 0x20
        jz .koff
        call fm_keyon
        sub si, O_SIZE
        jmp fm_keyon
.koff:
        call fm_keyoff
        sub si, O_SIZE
        jmp fm_keyoff
.both:
        push si
        call fm_opupd
        pop si
        add si, O_SIZE
        push si
        call fm_opupd
        pop si
        ret
; BDh: profundidad de tremolo y vibrato, y percusion
.rbd:
        mov ah, [gs:fm_rbd]
        mov [gs:fm_rbd], al
        ; las teclas de percusion solo cuentan con el modo activo (bit 5)
        test al, 0x20
        jnz .rb1
        xor al, al
.rb1:
        test ah, 0x20
        jnz .rb2
        xor ah, ah
.rb2:
        xor ah, al                  ; las que cambian
        mov di, fm_rhy
.rbl:
        mov dl, [di]
        test dl, dl
        jz .r
        test ah, dl
        jz .rbn
        movzx si, byte [di+1]
        imul si, si, O_SIZE
        add si, fm_op
        test al, dl
        jz .rbo
        call fm_keyon
        jmp .rbn
.rbo:
        call fm_keyoff
.rbn:
        add di, 2
        jmp .rbl

; pulsar / soltar la tecla del operador SI
fm_keyon:
        mov dword [gs:si+O_PHASE], 0
        mov byte [gs:si+O_STATE], 1
        mov byte [gs:fm_any], 1
        ret
fm_keyoff:
        cmp byte [gs:si+O_STATE], 0
        je .r
        mov byte [gs:si+O_STATE], 4
.r:
        ret

; recalcular el operador SI a partir de sus registros y de la frecuencia de
; su canal: nivel, sostenido, velocidades de envolvente y avance de fase
fm_opupd:
        movzx bx, byte [gs:si+O_CH]
        shl bx, 2
        add bx, fm_ch
        mov ax, [gs:bx+C_FNUM]
        mov cl, [gs:bx+C_B0]
        shr cl, 2
        and cl, 7                   ; octava
        ; atenuacion por altura: 6 dB por octava, entera, la mitad o un cuarto
        push ax
        shr ax, 6
        movzx di, al
        movzx ax, byte [fm_kslrom+di]
        shl ax, 2
        mov dx, 8
        sub dl, cl
        shl dx, 5
        sub ax, dx
        jns .k1
        xor ax, ax
.k1:
        mov dl, [gs:si+O_R40]
        shr dl, 6
        jnz .k2
        xor ax, ax
        jmp .k4
.k2:
        cmp dl, 3
        je .k4
        push cx
        mov cl, dl
        shr ax, cl
        pop cx
.k4:
        movzx dx, byte [gs:si+O_R40]
        and dx, 0x3F
        shl dx, 2
        add ax, dx
        mov [gs:si+O_TLL], ax
        pop ax
        ; sostenido: 3 dB por paso (el 15, 93 dB)
        movzx dx, byte [gs:si+O_R80]
        shr dx, 4
        cmp dl, 15
        jne .s1
        mov dl, 31
.s1:
        shl dx, 4
        mov [gs:si+O_SL], dx
        ; escala de las velocidades por la altura
        mov dl, cl
        shl dl, 1
        test ah, 2                  ; bit 9 de la frecuencia
        jz .r1
        inc dl
.r1:
        test byte [gs:si+O_R20], 0x10
        jnz .r2
        shr dl, 2
.r2:
        mov al, [gs:si+O_R60]
        shr al, 4
        call .rate
        mov [gs:si+O_ARI], eax
        mov al, [gs:si+O_R60]
        and al, 15
        call .rate
        mov [gs:si+O_DRI], eax
        mov al, [gs:si+O_R80]
        and al, 15
        call .rate
        mov [gs:si+O_RRI], eax
        jmp fm_opinc
; AL = velocidad (0..15), DL = escala -> EAX = pasos por muestra (16.16):
; (4 + r mod 4) * 2^(r / 4) / 32768, con r = 4 * velocidad + escala
.rate:
        test al, al
        jz .rz
        shl al, 2
        add al, dl
        cmp al, 63
        jbe .ra
        mov al, 63
.ra:
        push cx
        mov ch, al
        and ch, 3
        add ch, 4
        shr al, 2
        inc al
        mov cl, al
        movzx eax, ch
        shl eax, cl
        pop cx
        ret
.rz:
        xor eax, eax
        ret

; avance de fase del operador SI (con el vibrato de ahora)
fm_opinc:
        movzx bx, byte [gs:si+O_CH]
        shl bx, 2
        add bx, fm_ch
        movzx eax, word [gs:bx+C_FNUM]
        mov cl, [gs:bx+C_B0]
        shr cl, 2
        and cl, 7
        test byte [gs:si+O_R20], 0x40
        jz .nv
        mov dx, ax
        shr dx, 7
        and dx, 7
        test byte [gs:fm_rbd], 0x40
        jnz .dv
        shr dx, 1
.dv:
        mov ch, [gs:fm_vibs]
        test ch, 3
        jz .nv
        test ch, 1
        jz .full
        shr dx, 1
.full:
        test ch, 4
        jz .add
        neg dx
.add:
        add ax, dx
.nv:
        shl eax, cl
        movzx di, byte [gs:si+O_R20]
        and di, 15
        movzx edx, byte [fm_mult+di]
        mul edx
        mul dword [gs:fm_k]
        shrd eax, edx, 5
        mov [gs:si+O_INC], eax
        ret

; cada 64 muestras: tremolo (triangulo de 3,7 Hz, 4,8 o 1 dB) y vibrato
; (ocho pasos, 6,1 Hz). Conserva SI, DI y BP.
fm_lfo_step:
        mov al, [gs:fm_tstep]
        inc al
        cmp al, 210
        jb .t
        xor al, al
.t:
        mov [gs:fm_tstep], al
        cmp al, 105
        jb .u
        mov ah, 209
        sub ah, al
        mov al, ah
.u:
        shr al, 2
        test byte [gs:fm_rbd], 0x80
        jnz .d
        shr al, 2
.d:
        mov [gs:fm_trem], al
        inc byte [gs:fm_vcnt]
        test byte [gs:fm_vcnt], 15
        jnz .r
        mov al, [gs:fm_vibs]
        inc al
        and al, 7
        mov [gs:fm_vibs], al
        push si
        push di
        mov si, fm_op
        mov cx, 18
.v:
        test byte [gs:si+O_R20], 0x40
        jz .vn
        cmp byte [gs:si+O_STATE], 0
        je .vn
        push cx
        call fm_opinc
        pop cx
.vn:
        add si, O_SIZE
        loop .v
        pop di
        pop si
.r:
        ret

; una muestra del operador SI. BX = modulacion de fase (1024 = una onda)
; -> AX = salida (13 bits con signo). Conserva SI, DI y BP.
fm_oper:
        ; ---- envolvente ----
        mov al, [gs:si+O_STATE]
        cmp al, 2
        jb .att
        je .dec
        cmp al, 3
        je .sus
.rel:
        mov eax, [gs:si+O_RRI]
        call .steps
        add cx, [gs:si+O_ENV]
        cmp cx, 511
        jb .store
        mov cx, 511
        mov byte [gs:si+O_STATE], 0
        jmp .store
.sus:
        test byte [gs:si+O_R20], 0x20
        jz .rel                     ; sin sostenido: sigue cayendo
        jmp .eg9
.dec:
        mov eax, [gs:si+O_DRI]
        call .steps
        add cx, [gs:si+O_ENV]
        cmp cx, [gs:si+O_SL]
        jb .store
        mov cx, [gs:si+O_SL]
        mov byte [gs:si+O_STATE], 3
        jmp .store
.att:
        test al, al
        jz .eg9
        mov eax, [gs:si+O_ARI]
        call .steps
        jcxz .eg9
        mov ax, [gs:si+O_ENV]
.al:
        mov dx, ax
        shr dx, 3
        inc dx
        sub ax, dx
        ja .an
        xor ax, ax
        mov byte [gs:si+O_STATE], 2
        jmp .ast
.an:
        loop .al
.ast:
        mov cx, ax
.store:
        mov [gs:si+O_ENV], cx
.eg9:
        ; ---- atenuacion total ----
        mov cx, [gs:si+O_ENV]
        add cx, [gs:si+O_TLL]
        test byte [gs:si+O_R20], 0x80
        jz .noam
        movzx ax, byte [gs:fm_trem]
        add cx, ax
.noam:
        ; ---- fase ----
        mov edx, [gs:si+O_PHASE]
        mov eax, edx
        add eax, [gs:si+O_INC]
        mov [gs:si+O_PHASE], eax
        cmp cx, 480
        jae .zero
        shl cx, 3
        shr edx, 22
        add dx, bx
        ; ---- forma de onda: AH bit 0 = segundo cuarto, bit 1 = negativa ----
        mov al, [gs:si+O_WS]
        test byte [gs:fm_r01], 0x20
        jnz .ws
        xor al, al
.ws:
        mov ah, dh
        cmp al, 1
        jb .sine
        je .half
        cmp al, 2
        je .abs
        test ah, 1                  ; 3: solo los cuartos que suben
        jnz .zero
        and ah, ~2
        jmp .sine
.half:
        test ah, 2                  ; 1: media onda
        jnz .zero
        jmp .sine
.abs:
        and ah, ~2                  ; 2: valor absoluto
.sine:
        test ah, 1
        jz .q
        not dl
.q:
        movzx bx, dl
        shl bx, 1
        mov dx, [fm_logsin+bx]
        add dx, cx
        cmp dh, 13
        jae .zero
        mov cl, dh
        not dl
        movzx bx, dl
        shl bx, 1
        mov dx, [fm_exp+bx]
        shr dx, cl
        test ah, 2
        jz .pos
        neg dx
.pos:
        mov ax, dx
        ret
.zero:
        xor ax, ax
        ret
; EAX = pasos por muestra (16.16) -> CX = pasos enteros de ahora
.steps:
        movzx edx, word [gs:si+O_EGACC]
        add eax, edx
        mov [gs:si+O_EGACC], ax
        shr eax, 16
        mov cx, ax
        ret

; sumar la FM a las tramas nuevas del anillo (de w_old a w_abs)
fm_render:
        cmp byte [gs:fm_any], 0
        je .ret
        mov eax, [gs:w_abs]
        sub eax, [gs:w_old]
        mov bp, ax
        mov di, [gs:w_old]
        and di, RING_FR - 1
        shl di, 2
        add di, [buf_off]
.frame:
        inc word [gs:fm_lfo]
        test byte [gs:fm_lfo], 63
        jnz .nolfo
        call fm_lfo_step
.nolfo:
        mov dword [gs:fm_sum], 0
        mov word [gs:fm_nact], 0
        mov si, fm_op
        xor cx, cx
.ch:
        push cx
        test byte [gs:fm_rbd], 0x20
        jz .mel
        cmp cl, 7
        jae .perc
.mel:
        mov bx, cx
        shl bx, 2
        mov al, [gs:fm_ch+bx+C_C0]
        mov ah, [gs:si+O_SIZE+O_STATE]
        test al, 1
        jz .fmc
        or ah, [gs:si+O_STATE]
.fmc:
        test ah, ah
        jz .next
        inc word [gs:fm_nact]
        ; el modulador, con su realimentacion
        cmp byte [gs:si+O_STATE], 0
        je .nomod
        xor bx, bx
        shr al, 1
        and al, 7
        jz .nofb
        mov cl, 9
        sub cl, al
        mov bx, [gs:si+O_OUT]
        add bx, [gs:si+O_OUT2]
        sar bx, cl
.nofb:
        mov ax, [gs:si+O_OUT]
        mov [gs:si+O_OUT2], ax
        call fm_oper
        mov [gs:si+O_OUT], ax
        jmp .car
.nomod:
        xor ax, ax
.car:
        pop cx
        push cx
        mov bx, cx
        shl bx, 2
        test byte [gs:fm_ch+bx+C_C0], 1
        jnz .addv
        mov bx, ax                  ; en serie: modula al portador
        add si, O_SIZE
        call fm_oper
        sub si, O_SIZE
        jmp .acc
.addv:
        push ax                     ; sumados
        xor bx, bx
        add si, O_SIZE
        call fm_oper
        sub si, O_SIZE
        pop dx
        add ax, dx
.acc:
        cwde
        pop cx
        push cx
        cmp cl, 6                   ; el bombo, al doble
        jne .a1
        test byte [gs:fm_rbd], 0x20
        jz .a1
        shl eax, 1
.a1:
        add [gs:fm_sum], eax
.next:
        pop cx
        add si, 2 * O_SIZE
        inc cx
        cmp cx, 9
        jb .ch
        cmp word [gs:fm_nact], 0
        jne .mix
        mov byte [gs:fm_any], 0     ; todo callado: hasta la proxima tecla
        jmp .ret
.mix:
        mov eax, [gs:fm_sum]
        shl eax, 1
        cmp eax, 32767
        jle .c1
        mov eax, 32767
.c1:
        cmp eax, -32768
        jge .c2
        mov eax, -32768
.c2:
        mov cx, ax
        imul word [gs:fm_gl]
        mov al, ah
        mov ah, dl
        add [di], ax
        jno .l1
        mov word [di], 0x7FFF
        test ah, ah
        jns .l1
        mov word [di], 0x8000
.l1:
        mov ax, cx
        imul word [gs:fm_gr]
        mov al, ah
        mov ah, dl
        add [di+2], ax
        jno .r1
        mov word [di+2], 0x7FFF
        test ah, ah
        jns .r1
        mov word [di+2], 0x8000
.r1:
        add di, 4
        mov ax, [buf_off]
        add ax, NBUF * BUFSZ
        cmp di, ax
        jb .nw
        mov di, [buf_off]
.nw:
        dec bp
        jnz .frame
.ret:
        ret
; percusion: canales 7 (charles y caja) y 8 (timbal y platillo), un operador
; cada instrumento; los de ruido, con la fase al azar
.perc:
        mov eax, [gs:fm_noise]
        shr eax, 1
        jnc .n1
        xor eax, 0x400181
.n1:
        mov [gs:fm_noise], eax
        mov bx, ax
        and bx, 0x3FF
        cmp cl, 8
        jne .p7
        push bx
        xor bx, bx
        call .one                   ; timbal: tono limpio
        pop bx
        add si, O_SIZE
        call .one                   ; platillo
        sub si, O_SIZE
        jmp .next
.p7:
        push bx
        call .one                   ; charles
        pop bx
        xor bx, 0x155
        add si, O_SIZE
        call .one                   ; caja
        sub si, O_SIZE
        jmp .next
.one:
        cmp byte [gs:si+O_STATE], 0
        je .oret
        inc word [gs:fm_nact]
        call fm_oper
        cwde
        shl eax, 1
        add [gs:fm_sum], eax
.oret:
        ret
