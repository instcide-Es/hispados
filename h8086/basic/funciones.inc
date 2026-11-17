; =============================================================================
;  Hispa-BASIC - las funciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Las de FFh (LEFT$, SIN…), las de FEh (TIMER…), las sueltas (INSTR,
;  STRING$, INKEY$, CSRLIN) y las DEF FN del usuario. Las de cálculo (SIN,
;  LOG…) se calculan con la mantisa de 64 bits y dan simple precisión, como
;  en GW-BASIC.
; =============================================================================

; ---- un argumento entre paréntesis: numérico / de cadena / entero ----
arg_num:
        ESPERA '('
        call eval_num
        ESPERA ')'
        ret
arg_cad:                            ; BX = el descriptor
        ESPERA '('
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        ESPERA ')'
        mov bx, [ac + AC_M0]
        ret
arg_ent:
        call arg_num
        mov di, ac
        jmp a_entero

; ---- FFh xx: AL = xx ----
funcion_ff:
        cmp al, 0x81
        jb err_sintaxis
        cmp al, 0xA5
        ja err_sintaxis
        sub al, 0x81
        xor ah, ah
        shl ax, 1
        mov bx, ax
        jmp [cs:tabla_ff + bx]
tabla_ff:
        dw fn_left, fn_right, fn_mid, fn_sgn, fn_int, fn_abs, fn_sqr, fn_rnd
        dw fn_sin, fn_log, fn_exp, fn_cos, fn_tan, fn_atn, fn_fre, fn_inp
        dw fn_pos, fn_len, fn_str, fn_val, fn_asc, fn_chr, fn_peek, fn_space
        dw fn_oct, fn_hex, fn_pos, fn_cint, fn_csng, fn_cdbl, fn_fix, fn_mal
        dw fn_mal, fn_mal, fn_mal, fn_mal, fn_mal
fn_mal:
        jmp err_funcion

; ---- FEh xx ----
funcion_fe:
        cmp al, 0x94
        je fn_timer
        cmp al, 0x8D
        je fn_date
        cmp al, 0x8E
        je fn_time
        jmp err_sintaxis

; =============================================================================
;  Las de números
; =============================================================================
fn_abs:
        call arg_num
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [ac + AC_M0]
        test ax, ax
        jns .x
        cmp ax, 0x8000
        je op_cambia_signo          ; (32768: pasa a simple)
        neg ax
        mov [ac + AC_M0], ax
.x:
        ret
.flot:
        mov byte [ac + AC_SIG], 0
        ret
fn_sgn:
        call arg_num
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [ac + AC_M0]
        test ax, ax
        jz .pon
        mov ax, 1
        jns .pon
        neg ax
        jmp .pon
.flot:
        xor ax, ax
        cmp word [ac + AC_M3], 0
        je .pon
        inc ax
        test byte [ac + AC_SIG], 0x80
        jz .pon
        neg ax
.pon:
        jmp ac_entero
fn_int:
        call arg_num
        cmp byte [ac + AC_TIPO], T_ENT
        je .x
        mov di, ac
        call f_int
.x:
        ret
fn_fix:
        call arg_num
        cmp byte [ac + AC_TIPO], T_ENT
        je .x
        mov di, ac
        call f_trunca
.x:
        ret
fn_cint:
        call arg_ent
        jmp ac_entero
fn_csng:
        call arg_num
        mov di, ac
        call a_flot
        mov byte [ac + AC_TIPO], T_SIM
        mov di, ac
        jmp f_redondea
fn_cdbl:
        call arg_num
        mov di, ac
        call a_flot
        mov byte [ac + AC_TIPO], T_DOB
        ret

; el acumulador a decimales, de simple (los de cálculo)
ac_a_simple:
        mov di, ac
        call a_flot
        mov byte [ac + AC_TIPO], T_SIM
        ret
fin_simple:
        mov byte [ac + AC_TIPO], T_SIM
        mov di, ac
        jmp f_redondea

; ---- SQR ----
fn_sqr:
        call arg_num
        call ac_a_simple
        call fn_sqr_ac
        jmp fin_simple
fn_sqr_ac:
        cmp word [ac + AC_M3], 0
        je .x
        test byte [ac + AC_SIG], 0x80
        jnz err_funcion
        push si
        ; ac2 = x; ac3 = la aproximación (x con la mitad de exponente)
        mov si, ac
        mov di, ac2
        call f_copia
        mov di, ac3
        call f_copia
        mov ax, [ac3 + AC_EXP]
        sar ax, 1
        mov [ac3 + AC_EXP], ax
        mov cx, 7
.n:
        ; ac3 = (ac3 + ac2/ac3) / 2
        mov si, ac2
        mov di, ac4
        call f_copia
        mov si, ac3
        call f_div
        mov di, ac3
        mov si, ac4
        call f_suma
        dec word [ac3 + AC_EXP]
        loop .n
        mov si, ac3
        mov di, ac
        mov al, [di + AC_TIPO]
        call f_copia
        mov [di + AC_TIPO], al
        pop si
.x:
        ret

; ---- EXP ----
fn_exp:
        call arg_num
        call ac_a_simple
        call fn_exp_ac
        jmp fin_simple
; e^ac: x = n·ln2 + r, e^r por la serie
fn_exp_ac:
        push si
        mov al, [ac + AC_TIPO]
        push ax
        ; ¿demasiado grande? (más de 88,03: desbordamiento)
        mov si, c_ln_max
        mov di, ac4
        call f_carga_cte
        mov di, ac
        mov si, ac4
        call f_cmp
        cmp ax, 1
        je .desb
        ; n = INT(x / ln2 + 0,5)
        mov si, c_ln2
        mov di, ac3
        call f_carga_cte
        mov si, ac
        mov di, ac2
        call f_copia
        mov si, ac3
        call f_div                  ; ac2 = x / ln2
        mov si, f_medio_cte
        mov di, ac4
        call f_carga_cte
        mov di, ac2
        mov si, ac4
        call f_suma
        call f_int
        call f_a_entero             ; AX = n
        jc .cero_o_desb
        push ax
        mov si, c_ln2               ; (f_int usa ac3)
        mov di, ac3
        call f_carga_cte
        ; r = x - n·ln2   (ac2 = n como número)
        mov di, ac2
        call f_de_entero
        mov si, ac3
        call f_mul                  ; ac2 = n·ln2
        mov di, ac
        mov si, ac2
        call f_resta                ; ac = r
        ; serie: suma = 1 + r + r²/2 + …   (ac2 = el término, ac3 = la suma)
        mov si, f_uno_cte
        mov di, ac2
        call f_carga_cte
        mov di, ac3
        call f_carga_cte
        mov cx, 1
.t:
        ; término = término · r / k
        mov di, ac2
        mov si, ac
        call f_mul
        mov ax, cx
        mov di, ac4
        call f_de_entero
        mov di, ac2
        mov si, ac4
        call f_div
        mov di, ac3
        mov si, ac2
        call f_suma
        inc cx
        cmp cx, 20
        jbe .t
        pop ax
        add [ac3 + AC_EXP], ax      ; · 2^n
        mov si, ac3
        mov di, ac
        call f_copia
        pop ax
        mov [ac + AC_TIPO], al
        pop si
        ret
.cero_o_desb:
        test byte [ac + AC_SIG], 0x80
        jz .desb
        mov di, ac
        call f_cero
        pop ax
        mov [ac + AC_TIPO], al
        pop si
        ret
.desb:
        pop ax
        pop si
        jmp desborda_aviso

; ---- LOG ----
fn_log:
        call arg_num
        call ac_a_simple
        call fn_log_ac
        jmp fin_simple
; ln(ac) = e·ln2 + ln(m); ln(m) = 2·(z + z³/3 + z⁵/5 …), z = (m-1)/(m+1)
fn_log_ac:
        cmp word [ac + AC_M3], 0
        je .mal
        test byte [ac + AC_SIG], 0x80
        jnz .mal
        push si
        mov al, [ac + AC_TIPO]
        push ax
        mov ax, [ac + AC_EXP]
        mov word [ac + AC_EXP], 0   ; m entre 0,5 y 1
        mov byte [ac + AC_TIPO], T_DOB
        ; si m < raíz de 0,5: m·2, e-1
        push ax
        mov si, c_raiz_medio
        mov di, ac4
        call f_carga_cte
        mov di, ac
        mov si, ac4
        call f_cmp
        pop dx
        cmp ax, -1
        jne .m_ok
        inc word [ac + AC_EXP]
        dec dx
.m_ok:
        push dx                     ; e
        ; z = (m-1)/(m+1) en ac2
        mov si, f_uno_cte
        mov di, ac4
        call f_carga_cte
        mov si, ac
        mov di, ac2
        call f_copia
        mov si, ac4
        call f_resta                ; ac2 = m - 1
        mov di, ac
        call f_suma                 ; ac = m + 1
        mov di, ac2
        mov si, ac
        call f_div                  ; ac2 = z
        ; z² en ac3; la suma en ac; el término (z^k) en ac2
        mov si, ac2
        mov di, ac3
        call f_copia
        mov si, ac3
        call f_mul                  ; ac3 = z²
        mov si, ac2
        mov di, ac
        call f_copia                ; ac = z
        mov cx, 3
.t:
        mov di, ac2
        mov si, ac3
        call f_mul                  ; z^k
        mov si, ac2
        mov di, ac4
        call f_copia
        mov ax, cx
        mov di, ac5
        call f_de_entero
        mov di, ac4
        mov si, ac5
        call f_div                  ; z^k / k
        mov di, ac
        mov si, ac4
        call f_suma
        add cx, 2
        cmp cx, 31
        jbe .t
        inc word [ac + AC_EXP]      ; · 2
        ; + e·ln2
        pop ax
        mov di, ac2
        call f_de_entero
        mov si, c_ln2
        mov di, ac3
        call f_carga_cte
        mov di, ac2
        mov si, ac3
        call f_mul
        mov di, ac
        mov si, ac2
        call f_suma
        pop ax
        mov [ac + AC_TIPO], al
        pop si
        ret
.mal:
        jmp err_funcion

; ---- SIN, COS, TAN ----
fn_sin:
        call arg_num
        call ac_a_simple
        xor cx, cx
        call seno_ac
        jmp fin_simple
fn_cos:
        call arg_num
        call ac_a_simple
        mov cx, 1
        call seno_ac
        jmp fin_simple
fn_tan:
        call arg_num
        call ac_a_simple
        ; tan = sen / cos
        mov si, ac
        mov di, ac_tan
        call f_copia
        xor cx, cx
        call seno_ac
        push si
        mov si, ac
        mov di, ac_tan2
        call f_copia                ; el seno
        mov si, ac_tan
        mov di, ac
        call f_copia
        pop si
        mov cx, 1
        call seno_ac                ; el coseno
        cmp word [ac + AC_M3], 0
        je .inf
        push si
        mov di, ac_tan2
        mov si, ac
        call f_div
        mov si, ac_tan2
        mov di, ac
        call f_copia
        pop si
        jmp fin_simple
.inf:
        jmp desborda_aviso
; sen(ac + CX·π/2): k = redondeo(x / (π/2)); r = x - k·π/2; el cuadrante
; (k + CX) mod 4 dice qué serie y qué signo
seno_ac:
        push si
        mov al, [ac + AC_TIPO]
        push ax
        push cx
        mov byte [ac + AC_TIPO], T_DOB
        mov si, c_pi_medios
        mov di, ac3
        call f_carga_cte
        mov si, ac
        mov di, ac2
        call f_copia
        mov si, ac3
        call f_div                  ; ac2 = x / (π/2)
        mov si, f_medio_cte
        mov di, ac4
        call f_carga_cte
        mov di, ac2
        mov si, ac4
        call f_suma
        call f_int                  ; k
        ; k mod 4 (de la mantisa: solo hacen falta los dos bits de abajo)
        call f_bajos                ; AX = k mod 65536 (con signo bien)
        pop cx
        add ax, cx
        and ax, 3
        push ax                     ; el cuadrante
        mov si, c_pi_medios         ; (f_int y f_bajos usan ac3 y ac4)
        mov di, ac3
        call f_carga_cte
        mov di, ac2
        mov si, ac3
        call f_mul                  ; k·π/2
        mov di, ac
        mov si, ac2
        call f_resta                ; ac = r
        pop ax
        ; cuadrante 0: sen r; 1: cos r; 2: -sen r; 3: -cos r
        push ax
        test al, 1
        jnz .coseno
        call serie_seno
        jmp .signo
.coseno:
        call serie_coseno
.signo:
        pop ax
        test al, 2
        jz .x
        cmp word [ac + AC_M3], 0
        je .x
        xor byte [ac + AC_SIG], 0x80
.x:
        pop ax
        mov [ac + AC_TIPO], al
        pop si
        ret
; los 16 bits de abajo de la parte entera del de DI (que ya es entero): AX
f_bajos:
        push cx
        push di
        mov si, di
        mov di, ac4
        call f_copia
        mov byte [ac4 + AC_SIG], 0
        mov di, ac4
        xor ax, ax
        cmp word [di + AC_M3], 0
        je .x
        mov cx, 64
        sub cx, [di + AC_EXP]
        jle .x
        call f_dcha                 ; ahora la mantisa ES el entero
        mov ax, [di + AC_M0]
        pop di
        push di
        test byte [di + AC_SIG], 0x80
        jz .x
        neg ax
.x:
        pop di
        pop cx
        ret
; sen(r) = r - r³/3! + r⁵/5! …   (r en ac; usa ac2-ac4)
serie_seno:
        mov si, ac
        mov di, ac2
        call f_copia                ; el término
        mov di, ac3
        call f_copia
        mov si, ac3
        call f_mul                  ; ac3 = r²
        mov cx, 2
.t:
        ; término = -término · r² / (cx·(cx+1))
        mov di, ac2
        mov si, ac3
        call f_mul
        mov ax, cx
        inc ax
        mul cx
        mov di, ac4
        call f_de_entero
        mov di, ac2
        mov si, ac4
        call f_div
        xor byte [ac2 + AC_SIG], 0x80
        mov di, ac
        mov si, ac2
        call f_suma
        add cx, 2
        cmp cx, 24
        jbe .t
        ret
; cos(r) = 1 - r²/2! + r⁴/4! …
serie_coseno:
        mov si, ac
        mov di, ac3
        call f_copia
        mov si, ac3
        call f_mul                  ; ac3 = r²
        mov si, f_uno_cte
        mov di, ac2
        call f_carga_cte            ; el término
        mov di, ac
        call f_carga_cte            ; la suma
        mov cx, 1
.t:
        mov di, ac2
        mov si, ac3
        call f_mul
        mov ax, cx
        inc ax
        mul cx
        mov di, ac4
        call f_de_entero
        mov di, ac2
        mov si, ac4
        call f_div
        xor byte [ac2 + AC_SIG], 0x80
        mov di, ac
        mov si, ac2
        call f_suma
        add cx, 2
        cmp cx, 23
        jbe .t
        ret

; ---- ATN ----
fn_atn:
        call arg_num
        call ac_a_simple
        push si
        mov byte [ac + AC_TIPO], T_DOB
        mov al, [ac + AC_SIG]
        push ax                     ; el signo
        mov byte [ac + AC_SIG], 0
        xor dx, dx                  ; DL: 1 si se invirtió; DH: 1 si se restó π/6
        ; |x| > 1: atn = π/2 - atn(1/x)
        mov si, f_uno_cte
        mov di, ac4
        call f_carga_cte
        mov di, ac
        mov si, ac4
        call f_cmp
        cmp ax, 1
        jne .no_inv
        mov si, ac
        mov di, ac2
        call f_copia
        mov si, f_uno_cte
        mov di, ac
        call f_carga_cte
        mov si, ac2
        call f_div
        mov dl, 1
.no_inv:
        ; x > tan 15° (0,2679): atn = π/6 + atn((x·√3 - 1)/(√3 + x))
        cmp word [ac + AC_EXP], -1
        jl .serie                   ; menos de 0,25
        push dx
        mov si, c_raiz3
        mov di, ac3
        call f_carga_cte
        mov si, ac
        mov di, ac2
        call f_copia
        mov si, ac3
        call f_mul                  ; ac2 = x·√3
        mov si, f_uno_cte
        mov di, ac4
        call f_carga_cte
        mov di, ac2
        mov si, ac4
        call f_resta                ; x·√3 - 1
        mov di, ac3
        mov si, ac
        call f_suma                 ; √3 + x
        mov di, ac2
        mov si, ac3
        call f_div
        mov si, ac2
        mov di, ac
        call f_copia
        pop dx
        mov dh, 1
.serie:
        ; t - t³/3 + t⁵/5 …   (ac2 = t^k, ac3 = t², ac = la suma)
        push dx
        mov si, ac
        mov di, ac2
        call f_copia
        mov di, ac3
        call f_copia
        mov si, ac3
        call f_mul
        mov cx, 3
        mov bl, 0x80                ; el signo del siguiente término
.t:
        mov di, ac2
        mov si, ac3
        call f_mul
        mov si, ac2
        mov di, ac4
        call f_copia
        mov ax, cx
        mov di, ac5
        call f_de_entero
        mov di, ac4
        mov si, ac5
        call f_div
        xor [ac4 + AC_SIG], bl
        mov di, ac
        mov si, ac4
        call f_suma
        xor bl, 0x80
        add cx, 2
        cmp cx, 35
        jbe .t
        pop dx
        test dh, dh
        jz .sin_sexto
        mov si, c_pi_sextos
        mov di, ac4
        call f_carga_cte
        mov di, ac
        mov si, ac4
        call f_suma
.sin_sexto:
        test dl, dl
        jz .sin_inv
        mov si, c_pi_medios
        mov di, ac4
        call f_carga_cte
        mov di, ac4
        mov si, ac
        call f_resta
        mov si, ac4
        mov di, ac
        call f_copia
.sin_inv:
        pop ax
        cmp word [ac + AC_M3], 0
        je .x
        mov [ac + AC_SIG], al
.x:
        pop si
        jmp fin_simple

; ---- un desbordamiento que no para (como GW-BASIC): avisa y da el máximo
desborda_aviso:
        push si
        call col_cero
        mov si, m_desb
        call pon_cad_cs
        call pon_nl
        pop si
        mov di, ac
        mov al, [ac + AC_SIG]
        call f_cero
        mov [ac + AC_SIG], al
        mov byte [ac + AC_TIPO], T_SIM
        mov word [ac + AC_M3], 0xFFFF
        mov word [ac + AC_M2], 0xFF00
        mov word [ac + AC_EXP], 127
        ret

; ---- RND ----
fn_rnd:
        call salta_esp
        cmp al, '('
        jne .sig
        call arg_num
        call ac_a_simple
        cmp word [ac + AC_M3], 0
        je .repite
        test byte [ac + AC_SIG], 0x80
        jz .sig
        ; negativo: la semilla es su mantisa de 24 bits con el signo cambiado
        push si
        mov di, v_aux2
        mov bx, ac
        call a_mbf4
        pop si
        mov ax, [v_aux2]
        mov dl, [v_aux2 + 2]
        xor dl, 0x80
        mov [v_azar], ax
        mov [v_azar + 2], dl
.sig:
        ; x = (x · 214013 + 2531011) mod 2^24
        mov ax, [v_azar]            ; los 16 de abajo
        mov bl, [v_azar + 2]        ; los 8 de arriba
        mov cx, ax
        mov dx, 0x43FD
        mul dx                      ; DX:AX = abajo · 43FDh
        push ax
        push dx
        mov al, bl
        mov ah, 0
        mov dx, 0x43FD
        mul dx                      ; arriba · 43FDh (cuenta su octeto bajo)
        mov bh, al
        mov ax, cx
        mov dx, 3
        mul dx                      ; abajo · 3 (ídem)
        pop dx
        add dl, al
        add dl, bh
        pop ax
        add ax, 0x9EC3
        adc dl, 0x26
        mov [v_azar], ax
        mov [v_azar + 2], dl
.repite:
        ; el valor: (x xor 800000h) / 2^24
        mov ax, [v_azar]
        mov dl, [v_azar + 2]
        xor dl, 0x80
        mov di, ac
        call f_cero
        mov byte [di + AC_TIPO], T_SIM
        mov [di + AC_M3 + 1], dl
        mov [di + AC_M3], ah
        mov [di + AC_M2 + 1], al
        call f_normaliza
        ret

; ---- FRE ----
fn_fre:
        call salta_esp
        cmp al, '('
        jne .x
        call arg_cad_o_num
        call recoge_cadenas
.x:
        mov ax, [v_cads]
        sub ax, [v_libre]
        jmp ac_natural
arg_cad_o_num:
        ESPERA '('
        call evalua
        ESPERA ')'
        cmp byte [ac + AC_TIPO], T_CAD
        jne .x
        mov bx, [ac + AC_M0]
        call libera_temp
.x:
        ret

; ---- INP, PEEK, POS, CSRLIN ----
fn_inp:
        call arg_num
        mov di, ac
        call a_palabra
        mov dx, ax
        in al, dx
        xor ah, ah
        jmp ac_entero
fn_peek:
        call arg_num
        mov di, ac
        call a_palabra
        mov bx, ax
        push es
        mov es, [v_defseg]
        mov al, [es:bx]
        pop es
        xor ah, ah
        jmp ac_entero
fn_pos:
        call arg_num
        mov ax, [v_col]
        inc ax
        jmp ac_entero
fn_csrlin:
        mov ah, 3
        xor bh, bh
        int 0x10
        mov al, dh
        xor ah, ah
        inc ax
        jmp ac_entero

; ---- TIMER: segundos desde medianoche ----
fn_timer:
        push si
        xor ah, ah
        int 0x1A                    ; CX:DX = los tics
        mov di, ac
        call f_cero
        mov byte [di + AC_TIPO], T_SIM
        mov [di + AC_M3], cx
        mov [di + AC_M2], dx
        mov word [di + AC_EXP], 32
        call f_normaliza
        mov si, c_seg_tic
        mov di, ac2
        call f_carga_cte
        mov di, ac
        mov si, ac2
        call f_mul
        mov byte [ac + AC_TIPO], T_SIM
        mov di, ac
        call f_redondea
        pop si
        ret

; ---- DATE$ y TIME$ (solo para leer) ----
fn_date:
        push si
        mov ah, 0x2A
        int 0x21                    ; CX año, DH mes, DL día
        mov di, b_num
        mov al, dh
        call dos_cifras
        mov al, '-'
        stosb
        mov al, dl
        call dos_cifras
        mov al, '-'
        stosb
        mov ax, cx
        call texto_natural
        jmp fin_texto
fn_time:
        push si
        mov ah, 0x2C
        int 0x21                    ; CH hora, CL minuto, DH segundo
        mov di, b_num
        mov al, ch
        call dos_cifras
        mov al, ':'
        stosb
        mov al, cl
        call dos_cifras
        mov al, ':'
        stosb
        mov al, dh
        call dos_cifras
        mov byte [di], 0
fin_texto:
        mov di, b_num
        call largo_asciz
        mov si, b_num
        call cadena_de
        pop si
        ret
dos_cifras:
        push dx
        xor ah, ah
        mov dl, 10
        div dl
        add ax, '00'
        stosw
        pop dx
        ret

; =============================================================================
;  Las de cadenas
; =============================================================================
fn_len:
        call arg_cad
        call libera_temp
        mov al, [bx]
        xor ah, ah
        jmp ac_entero
fn_asc:
        call arg_cad
        call libera_temp
        cmp byte [bx], 0
        je .mal
        mov bx, [bx + 1]
        mov al, [bx]
        xor ah, ah
        jmp ac_entero
.mal:
        jmp err_funcion
fn_chr:
        call arg_ent
        test ah, ah
        jnz .mal
        mov [v_aux], al
        push si
        mov si, v_aux
        mov cx, 1
        call cadena_de
        pop si
        ret
.mal:
        jmp err_funcion
fn_str:
        call arg_num
        call num_a_texto
        push si
        mov di, b_num
        call largo_asciz
        mov si, b_num
        call cadena_de
        pop si
        ret
fn_val:
        call arg_cad
        call libera_temp
        ; una copia ASCIZ en b_num, y se lee el número que haya delante
        push si
        mov cl, [bx]
        xor ch, ch
        cmp cx, 40
        jbe .c
        mov cx, 40
.c:
        mov si, [bx + 1]
        mov di, b_num
        rep movsb
        mov byte [di], 0
        mov si, b_num
.e:
        lodsb
        cmp al, ' '
        je .e
        xor dx, dx                  ; DL: negativo
        cmp al, '-'
        jne .mas
        mov dl, 1
        lodsb
        jmp .num
.mas:
        cmp al, '+'
        jne .num
        lodsb
.num:
        dec si
        cmp al, '&'
        je .hex
        call es_cifra
        jc .lee
        cmp al, '.'
        je .lee
        xor ax, ax
        call ac_entero
        jmp .x
.lee:
        push dx
        call lee_numero
        pop dx
        jmp .signo
.hex:
        inc si
        mov al, [si]
        call mayus
        mov bl, 16
        cmp al, 'H'
        je .h1
        mov bl, 8
        cmp al, 'O'
        jne .h2
.h1:
        inc si
.h2:
        push dx
        call lee_base
        pop dx
        call ac_entero
.signo:
        test dl, dl
        jz .x
        call op_cambia_signo
.x:
        pop si
        ret
fn_space:
        call arg_ent
        mov dl, ' '
        jmp cadena_repetida
; ---- STRING$(n, código o cadena) ----
fn_string:
        ESPERA '('
        call eval_ent
        push ax
        ESPERA ','
        call evalua
        ESPERA ')'
        cmp byte [ac + AC_TIPO], T_CAD
        jne .codigo
        mov bx, [ac + AC_M0]
        call libera_temp
        cmp byte [bx], 0
        je .mal
        mov bx, [bx + 1]
        mov dl, [bx]
        jmp .ya
.codigo:
        mov di, ac
        call a_entero
        test ah, ah
        jnz .mal
        mov dl, al
.ya:
        pop ax
; AX veces el carácter DL, como cadena de paso
cadena_repetida:
        test ax, ax
        js .mal
        cmp ax, 255
        ja .mal
        mov cx, ax
        push si
        push cx
        push dx
        call pide_cad               ; DX
        pop ax
        pop cx
        mov di, dx
        push cx
        rep stosb
        pop cx
        call nuevo_temp
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        pop si
        ret
.mal:
fn_string.mal:
        jmp err_funcion
; ---- LEFT$, RIGHT$, MID$ ----
fn_left:
        call cad_y_ent              ; BX = la cadena, AX = n
        mov cl, [bx]
        xor ch, ch
        cmp ax, cx
        jbe .n
        mov ax, cx
.n:
        mov cx, ax
        xor ax, ax
        jmp cadena_de_desc
fn_right:
        call cad_y_ent
        mov cl, [bx]
        xor ch, ch
        cmp ax, cx
        jbe .n
        mov ax, cx
.n:
        xchg ax, cx                 ; CX = n; AX = largo
        sub ax, cx                  ; desde
        jmp cadena_de_desc
fn_mid:
        ESPERA '('
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        push word [ac + AC_M0]
        ESPERA ','
        call eval_ent
        dec ax
        js .mal
        cmp ax, 255
        jae .mal
        push ax
        mov cx, 255
        call salta_esp
        cmp al, ','
        jne .sin
        inc si
        call eval_ent
        test ax, ax
        js .mal
        mov cx, ax
.sin:
        ESPERA ')'
        pop ax                      ; desde (desde 0)
        pop bx
        ; cuántos: min(CX, largo - desde)
        mov dl, [bx]
        xor dh, dh
        sub dx, ax
        jg .hay
        xor cx, cx
        xor ax, ax
        jmp cadena_de_desc
.hay:
        cmp cx, dx
        jbe .y
        mov cx, dx
.y:
        jmp cadena_de_desc
.mal:
        jmp err_funcion
; (cadena, entero): BX = el descriptor, AX = el entero (0 a 255)
cad_y_ent:
        ESPERA '('
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        push word [ac + AC_M0]
        ESPERA ','
        call eval_ent
        test ax, ax
        js .mal
        cmp ax, 255
        ja .mal
        push ax
        ESPERA ')'
        pop ax
        pop bx
        ret
.mal:
        jmp err_funcion
; ---- INSTR([n,] a$, b$) ----
fn_instr:
        ESPERA '('
        call evalua
        mov ax, 1
        cmp byte [ac + AC_TIPO], T_CAD
        je .sin_n
        mov di, ac
        call a_entero
        test ax, ax
        jle .mal
        cmp ax, 255
        ja .mal
        push ax
        ESPERA ','
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        pop ax
.sin_n:
        push ax
        push word [ac + AC_M0]
        ESPERA ','
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        ESPERA ')'
        mov di, [ac + AC_M0]        ; b$
        pop bx                      ; a$
        pop ax                      ; desde (desde 1)
        push si
        ; si n > largo de a$: 0; si b$ es "": n
        mov cl, [bx]
        xor ch, ch
        cmp ax, cx
        ja .cero
        cmp byte [di], 0
        je .res
        mov dx, ax                  ; la posición que se prueba
.p:
        ; ¿cabe b$ desde dx?
        mov cl, [bx]
        xor ch, ch
        sub cx, dx
        inc cx                      ; lo que queda de a$
        mov al, [di]
        xor ah, ah
        cmp cx, ax
        jb .cero
        push di
        mov si, [bx + 1]
        add si, dx
        dec si
        mov cx, ax
        mov di, [di + 1]
        repe cmpsb
        pop di
        je .encontrado
        inc dx
        jmp .p
.encontrado:
        mov ax, dx
        jmp .res
.cero:
        xor ax, ax
.res:
        pop si
        jmp ac_entero
.mal:
        jmp err_funcion
; ---- HEX$ y OCT$ ----
fn_hex:
        mov bx, 16
        jmp en_base
fn_oct:
        mov bx, 8
en_base:
        push bx
        call arg_num
        mov di, ac
        cmp byte [di + AC_TIPO], T_ENT
        je .ent
        call a_flot
        test byte [di + AC_SIG], 0x80
        jz .pal
        call a_entero
        jmp .ya
.pal:
        call a_palabra
        jmp .ya
.ent:
        mov ax, [di + AC_M0]
.ya:
        pop bx
        push si
        mov di, b_num + 20
        mov byte [di], 0
.d:
        xor dx, dx
        div bx
        add dl, '0'
        cmp dl, '9'
        jbe .e
        add dl, 'A' - '9' - 1
.e:
        dec di
        mov [di], dl
        test ax, ax
        jnz .d
        mov si, di
        call largo_asciz
        call cadena_de
        pop si
        ret

; ---- INKEY$ ----
fn_inkey:
        push si
        mov ah, 0x06
        mov dl, 0xFF
        int 0x21
        jz .nada
        mov [v_aux], al
        mov cx, 1
        test al, al
        jnz .uno
        mov ah, 0x07
        int 0x21
        mov [v_aux + 1], al
        mov cx, 2
.uno:
        mov si, v_aux
        call cadena_de
        pop si
        ret
.nada:
        xor cx, cx
        xor dx, dx
        call nuevo_temp
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        pop si
        ret

; =============================================================================
;  DEF FN: una entrada en las variables simples con el nombre 01h, tipo y
;  nombre (no se puede confundir con una variable), de tipo entero, cuyo
;  valor es dónde sigue el texto de la DEF (los parámetros y la expresión)
; =============================================================================
; el nombre de detrás de FN, a b_nom como clave; DH = el tipo
nombre_fn:
        call salta_esp
        call lee_nombre             ; AL = el tipo
        mov dh, al
        mov [v_fn_tipo], al
        ; b_nom: largo, letras -> largo+2, 01h, tipo, letras
        push si
        push cx
        mov cl, [b_nom]
        xor ch, ch
        mov si, b_nom
        add si, cx
        mov di, si
        add di, 2
        std
        rep movsb
        cld
        mov al, [b_nom]
        add al, 2
        mov [b_nom], al
        mov byte [b_nom + 1], 1
        mov [b_nom + 2], dh
        pop cx
        pop si
        mov dh, T_ENT
        ret
; la de b_nom: BX = su valor, CF=1 si no está
busca_fn:
        push dx
        xor dl, dl
        mov dh, T_ENT
        call busca_simple
        pop dx
        cmp bx, v_cero_val
        je .no
        clc
        ret
.no:
        stc
        ret
crea_fn:
        push dx
        mov dl, 1
        mov dh, T_ENT
        call busca_simple
        pop dx
        ret

; ---- FNnombre[(args)] en una expresión (SI tras el código FN) ----
llama_fn:
        call nombre_fn
        call busca_fn
        jc .no_def
        push word [v_fn_tipo]
        push word [bx]              ; dónde está la definición
        ; los argumentos, a la pila (12 octetos cada uno)
        xor cx, cx
        call salta_esp
        cmp al, '('
        jne .sin_args
        inc si
.a:
        push cx
        call evalua
        pop cx
        call empuja_ac
        inc cx
        call salta_esp
        inc si
        cmp al, ','
        je .a
        cmp al, ')'
        jne err_sintaxis
.sin_args:
        push cx                     ; cuántos
        push si                     ; el texto de quien llama
        mov bp, sp
        ; [bp] SI, [bp+2] n, [bp+4..] los argumentos (el último primero),
        ; luego la definición y el tipo
        mov ax, cx
        mov dx, AC_TAM
        mul dx
        mov bx, bp
        add bx, 4
        add bx, ax                  ; BX tras los argumentos
        mov si, [bx]                ; la definición
        ; los parámetros: cada uno toma su argumento
        xor cx, cx                  ; el número de parámetro
        call salta_esp
        cmp al, '('
        jne .sin_par
        inc si
.p:
        push cx
        push bp
        mov al, 1
        call busca_var              ; BX, AL
        pop bp
        pop cx
        cmp cx, [bp + 2]
        jae .mal_args
        ; guarda lo de antes en la pila de FN
        mov di, [v_fnp]
        cmp di, v_fn_pila + FN_TAM * (N_FNP - 1)
        ja .honda
        add word [v_fnp], FN_TAM
        mov [di], bx
        mov [di + 2], al
        push si
        push cx
        lea si, [bx]
        add di, 4
        mov cl, al
        xor ch, ch
        rep movsb
        pop cx
        pop si
        ; el argumento cx-ésimo: está en [bp+4 + 12·(n-1-cx)]
        push bx
        push ax
        mov ax, [bp + 2]
        dec ax
        sub ax, cx
        mov dx, AC_TAM
        mul dx
        mov di, bp
        add di, 4
        add di, ax
        push si
        push cx
        mov si, di
        mov di, ac
        mov cx, AC_TAM / 2
        rep movsw
        pop cx
        pop si
        pop ax
        pop bx
        push cx
        push bp
        call guarda_valor
        pop bp
        pop cx
        inc cx
        call salta_esp
        inc si
        cmp al, ','
        je .p
        cmp al, ')'
        jne err_sintaxis
.sin_par:
        cmp cx, [bp + 2]
        jne .mal_args
        push cx                     ; cuántos parámetros (para soltarlos)
        ESPERA 0xE7
        call evalua
        pop cx
        ; el resultado, a su tipo
        mov bp, sp
        call fn_marco               ; BX tras los argumentos
        mov al, [bx + 2]            ; el tipo de la función
        push cx
        cmp al, T_CAD
        je .cad
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        cmp al, T_ENT
        jne .flot
        mov di, ac
        call a_entero
        mov [ac + AC_M0], ax
        mov byte [ac + AC_TIPO], T_ENT
        jmp .repon
.flot:
        push ax
        mov di, ac
        call a_flot
        pop ax
        mov [ac + AC_TIPO], al
        mov di, ac
        call f_redondea
        jmp .repon
.cad:
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        ; se copia (puede ser la de un parámetro, que se va a reponer)
        mov bx, [ac + AC_M0]
        mov cl, [bx]
        xor ch, ch
        xor ax, ax
        call cadena_de_desc
.repon:
        pop cx
        ; reponer los parámetros (del último al primero)
        jcxz .fin
.r:
        sub word [v_fnp], FN_TAM
        mov si, [v_fnp]
        mov di, [si]
        mov al, [si + 2]
        push cx
        mov cl, al
        xor ch, ch
        add si, 4
        rep movsb
        pop cx
        loop .r
.fin:
        mov si, [bp]                ; el texto de quien llama
        call fn_marco
        lea sp, [bx + 4]            ; fuera todo
        ret
.no_def:
        mov al, 18
        jmp error
.mal_args:
        jmp err_sintaxis
.honda:
        mov al, 7
        jmp error

; BX = BP + 4 + 12·n (n en [BP+2]): donde están la definición y el tipo
fn_marco:
        push ax
        push dx
        mov ax, [bp + 2]
        mov dx, AC_TAM
        mul dx
        mov bx, bp
        add bx, 4
        add bx, ax
        pop dx
        pop ax
        ret

m_desb      db "Desbordamiento", 0

; ---- las constantes (MBF doble) ----
c_pi_medios  db 0xC0, 0x68, 0x21, 0xA2, 0xDA, 0x0F, 0x49, 0x81
c_pi_sextos  db 0x28, 0x9B, 0x6B, 0xC1, 0x91, 0x0A, 0x06, 0x80
c_raiz3      db 0x50, 0x65, 0xC2, 0x42, 0xD7, 0xB3, 0x5D, 0x81
c_ln2        db 0x78, 0xCF, 0xD1, 0xF7, 0x17, 0x72, 0x31, 0x80
c_seg_tic    db 0x58, 0x04, 0x88, 0xD1, 0x8D, 0xF9, 0x60, 0x7C
c_raiz_medio db 0x68, 0xDE, 0xF9, 0x33, 0xF3, 0x04, 0x35, 0x80
c_ln_max     db 0x58, 0x09, 0x16, 0x87, 0x33, 0x0F, 0x30, 0x87
