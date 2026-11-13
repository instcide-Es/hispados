; =============================================================================
;  hspmezi.inc - el mezclador por fuentes de HSPSND: lo de la instalacion
;  (se descarta al quedar residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;    mz_iniciar   (al instalar, con la tarjeta ya elegida) mira que tiene la
;                 tarjeta, lee la seccion [Mezclador] de SONIDO.INI y, si se
;                 ha dicho /VOL:n, pone ese volumen en General
;    hda_inputs   (HD Audio, al final de setup_tail) busca en los mezcladores
;                 de los caminos de salida las entradas que vienen de un pin
;                 de CD, de linea o de microfono
;  El formato de [Mezclador], en docs/MEZCLADOR.md.
; =============================================================================

mz_iniciar:
        call ini_load               ; (si no se ha leido ya)
        call mz_detect
        call mz_caps_init
        call mz_ini
        cmp byte [have_vol], 0
        je .nv
        mov al, [opt_vol]
        mov [vol_pct], al
        mov byte [vol_mute], 0
.nv:
        mov byte [mz_chg], 0
        ret

; ---- que registros tiene la tarjeta ----
mz_detect:
        cmp byte [dev_type], 3
        je .ac
        cmp byte [dev_type], 1
        jne .r
        cmp byte [sb_major], 2      ; Sound Blaster 2.0: ¿con el CT1335?
        jne .r
        mov ax, 0x060A
        call sb_mix
        mov al, 0x06
        call sb_mixrd
        and al, 0x0E
        cmp al, 0x0A
        jne .r
        mov ax, 0x0604
        call sb_mix
        mov al, 0x06
        call sb_mixrd
        and al, 0x0E
        cmp al, 0x04
        jne .r
        mov byte [mz_ct1335], 1
        mov ax, 0x020E              ; el maestro al máximo (General va por
        call sb_mix                 ; programa) y la voz también
        mov ax, 0x0A06
        call sb_mix
.r:
        ret
.ac:
        ; AC'97: los registros que no existen se leen como 0
        xor bl, bl
        mov dl, 0x12                ; CD
        mov ax, 0x8808
        mov cx, 0x1F1F
        call .prueba
        jne .a1
        or bl, MZA_CD
.a1:
        mov dl, 0x10                ; línea
        mov ax, 0x8808
        call .prueba
        jne .a2
        or bl, MZA_LIN
.a2:
        mov dl, 0x0E                ; micrófono
        mov ax, 0x8008
        mov cx, 0x001F
        call .prueba
        jne .a3
        or bl, MZA_MIC
.a3:
        mov dl, 0x0A                ; altavoz del PC
        mov ax, 0x801E
        mov cx, 0x001E
        call .prueba
        jne .a4
        or bl, MZA_BEEP
.a4:
        mov dl, 0x08                ; tonos (y quedan sin efecto: 0F0Fh)
        mov ax, 0x0F0F
        mov cx, 0x0F0F
        call .prueba
        jne .a5
        or bl, MZA_TONO
.a5:
        mov [mz_ac], bl
        ret
; DL = registro, AX = valor, CX = bits que se miran -> ZF si se leen igual
.prueba:
        push ax
        push dx
        movzx dx, dl
        add dx, [nam]
        out dx, ax
        in ax, dx
        pop dx
        and ax, cx
        pop si
        and si, cx
        cmp ax, si
        jne .pr
        test ax, ax                 ; (que no sean todos ceros)
        jnz .pz
        or ax, 1                    ; ZF = 0
.pr:
        ret
.pz:
        cmp ax, ax
        ret

; ---- qué fuentes y ajustes hay en esta tarjeta ----
mz_caps_init:
        xor ax, ax                  ; AL fuentes, AH las de la tarjeta
        xor bx, bx                  ; ajustes
        xor cx, cx                  ; CL grabables, CH con «Sonar»
        mov dl, [dev_type]
        cmp dl, 2
        jne .n2
        mov ax, 0x7F7F              ; Sound Blaster 16: todo en el CT1745
        mov bx, MZC_BAL | MZC_GRAVES | MZC_AGUDOS | MZC_GENT | MZC_GSAL | MZC_AGC | MZC_ALT4
        mov cx, 0x383C
        jmp .fin
.n2:
        cmp dl, 1
        jne .n3
        cmp byte [sb_major], 3
        jb .sb2
        mov ax, 0x3F3F              ; SB Pro: el CT1345
        mov bx, MZC_BAL | MZC_UNA
        mov cx, 0x3838
        jmp .fin
.sb2:
        mov ax, 0x0003              ; SB 2.0: General y Sonidos por programa
        cmp byte [mz_ct1335], 0
        je .fin
        mov ax, 0x0C0F              ; ... y Música y CD en el CT1335
        mov ch, 0x08
        jmp .fin
.n3:
        cmp dl, 3
        jne .n4
        mov ax, 0x0303              ; AC'97
        mov bx, MZC_BAL | MZC_GENT | MZC_UNA
        mov dh, [mz_ac]
        test dh, MZA_CD
        jz .c1
        or ax, 0x0808
.c1:
        test dh, MZA_LIN
        jz .c2
        or ax, 0x1010
.c2:
        test dh, MZA_MIC
        jz .c3
        or ax, 0x2020
        or bx, MZC_REFUERZO
.c3:
        test dh, MZA_BEEP
        jz .c4
        or ax, 0x4040
.c4:
        test dh, MZA_TONO
        jz .c5
        or bx, MZC_GRAVES | MZC_AGUDOS
.c5:
        mov cl, al
        and cl, 0x38
        mov ch, cl
        jmp .fin
.n4:
        cmp dl, 4
        jne .fin
        mov ax, 0x0003              ; HD Audio: General (en el códec o por
        cmp byte [hw_vol], 0        ; programa) y Sonidos por programa
        je .h0
        mov ah, 0x01
.h0:
        mov bx, MZC_BAL
        mov si, hda_in              ; y las entradas del mezclador del códec
        movzx di, byte [hda_nin]
.h1:
        test di, di
        jz .h2
        push cx
        mov cl, [si+2]
        mov dh, 1
        shl dh, cl
        pop cx
        or al, dh
        or ah, dh
        or ch, dh
        add si, 6
        dec di
        jmp .h1
.h2:
.fin:
        ; con la Sound Blaster emulada, la Música (su FM, por programa)
        cmp byte [opt_sb], 0
        je .w
        cmp byte [dev_type], 3
        jb .w
        or al, 0x04
.w:
        mov [mz_src], al
        mov [mz_hwsrc], ah
        mov [mz_ajs], bx
        mov [mz_rec], cl
        mov [mz_son], ch
        ret

; ---- la sección [Mezclador] de SONIDO.INI ----
mz_ini:
        cmp byte [ini_lei], 0
        je .r
        push cs
        pop es
        mov si, i_mz_sec
        mov di, key_buf
.k:
        lodsb
        stosb
        test al, al
        jnz .k
        call ini_find
        jc .r
        ; volumen y balance: «General=80,50»
        xor bx, bx
.v:
        push bx
        shl bx, 1
        mov si, [i_mz_nom+bx]
        pop bx
        call ini_get
        jc .vn
        push bx
        call ini_dec
        pop bx
        jc .vn
        cmp ax, 100
        ja .vn
        mov [mz_vol+bx], al
        call ini_skipsp
        cmp byte [si], ','
        jne .vn
        inc si
        push bx
        call ini_dec
        pop bx
        jc .vn
        cmp ax, 100
        ja .vn
        mov [mz_bal+bx], al
.vn:
        inc bx
        cmp bx, MZ_N
        jb .v
        ; las casillas: «Silencio=Musica,CD», «Sonar=CD», «Grabar=Microfono»
        mov si, i_mz_sil
        mov ah, MZF_MUDO
        call .lista
        mov si, i_mz_son
        mov ah, MZF_SONAR
        call .lista
        mov si, i_mz_gra
        mov ah, MZF_GRABAR
        call .lista
        ; una sola fuente de grabación: la primera
        test byte [mz_ajs], MZC_UNA
        jz .aj
        xor bx, bx
        mov dl, 0
.u:
        test byte [mz_flg+bx], MZF_GRABAR
        jz .un
        inc dl
        cmp dl, 1
        je .un
        and byte [mz_flg+bx], ~MZF_GRABAR & 0xFF
.un:
        inc bx
        cmp bx, MZ_N
        jb .u
.aj:
        ; los ajustes
        xor bx, bx
.a:
        push bx
        shl bx, 1
        mov si, [i_mz_aj+bx]
        pop bx
        call ini_get
        jc .an
        push bx
        call ini_dec
        pop bx
        jc .an
        cmp al, [i_mz_lim+bx]
        ja .an
        test ah, ah
        jnz .an
        mov [mz_ajv+bx], al
.an:
        inc bx
        cmp bx, MZ_NAJ
        jb .a
.r:
        ret
; SI = clave, AH = casilla: si la clave está, la casilla queda puesta en
; las fuentes de la lista y quitada en las demás
.lista:
        push ax
        call ini_get
        pop ax
        jc .lr
        call ini_lista              ; -> DL = fuentes
        xor bx, bx
        mov al, ah
        not al
.ll:
        and [mz_flg+bx], al
        shr dl, 1
        jnc .l2
        or [mz_flg+bx], ah
.l2:
        inc bx
        cmp bx, MZ_N
        jb .ll
        and byte [mz_flg], MZF_MUDO ; (General: solo el silencio)
.lr:
        ret

; SI = lista de nombres de fuentes separados por comas -> DL = sus bits
ini_lista:
        xor dl, dl
.l:
        mov al, [si]
        cmp al, ' '
        je .s
        cmp al, 9
        je .s
        cmp al, ','
        jne .w
.s:
        inc si
        jmp .l
.w:
        call .fin
        je .r
        ; ¿qué fuente es?
        xor bx, bx
.n:
        push bx
        shl bx, 1
        mov di, [i_mz_nom+bx]
        pop bx
        push si
.c:
        mov al, [si]
        call ini_up
        cmp byte [di], '='
        je .finn
        cmp al, [di]
        jne .no
        inc si
        inc di
        jmp .c
.finn:
        call .fin                   ; el nombre acaba ahí
        je .si
        cmp al, ','
        je .si
        cmp al, ' '
        je .si
.no:
        pop si
        inc bx
        cmp bx, MZ_N
        jb .n
        ; no se conoce: se salta
.sk:
        mov al, [si]
        call .fin
        je .r
        cmp al, ','
        je .l
        inc si
        jmp .sk
.si:
        add sp, 2
        mov cl, bl
        mov al, 1
        shl al, cl
        or dl, al
        jmp .l
.r:
        ret
; AL = carácter -> ZF si acaba la línea
.fin:
        test al, al
        jz .f
        cmp al, 13
        je .f
        cmp al, 10
        je .f
        cmp al, ';'
.f:
        ret

i_mz_sec    db "[MEZCLADOR]", 0
i_mz_nom    dw i_n0, i_n1, i_n2, i_n3, i_n4, i_n5, i_n6
i_n0        db "GENERAL=", 0
i_n1        db "SONIDOS=", 0
i_n2        db "MUSICA=", 0
i_n3        db "CD=", 0
i_n4        db "LINEA=", 0
i_n5        db "MICROFONO=", 0
i_n6        db "ALTAVOZ=", 0
i_mz_sil    db "SILENCIO=", 0
i_mz_son    db "SONAR=", 0
i_mz_gra    db "GRABAR=", 0
i_mz_aj     dw i_a0, i_a1, i_a2, i_a3, i_a4, i_a5
i_a0        db "GRAVES=", 0
i_a1        db "AGUDOS=", 0
i_a2        db "GANANCIAENTRADA=", 0
i_a3        db "GANANCIASALIDA=", 0
i_a4        db "AJUSTEMICROFONO=", 0
i_a5        db "REFUERZOMICROFONO=", 0
i_mz_lim    db 100, 100, 3, 3, 1, 1

; =============================================================================
;  HD Audio: las entradas del mezclador del códec (CD, línea, micrófono)
; =============================================================================
; Para cada mezclador (tipo 2) de los caminos de salida elegidos, cada
; conexión que viene de un pin de entrada con dispositivo CD (3), entrada de
; línea (8) o micrófono (Ah), si el mezclador tiene amplificador de entrada:
; se apunta en hda_in (como mucho 6) y el pin se pone como entrada.
hda_inputs:
        mov byte [hda_nin], 0
        mov si, hda_pins
        movzx cx, byte [hda_npin]
.p:
        jcxz .r
        push cx
        push si
        mov cx, 4                   ; los nodos del camino detrás del pin
.n:
        inc si
        mov bl, [si]
        test bl, bl
        jz .pn
        push cx
        push si
        call w_type
        cmp al, 2
        jne .nn
        test byte [cur_caps], 0x02  ; con amplificador de entrada
        jz .nn
        mov [hi_mix], bl
        mov di, conn_bufs + 3 * CONNMAX
        call get_conn
        xor si, si
.c:
        cmp si, cx
        jae .nn
        cmp si, 15
        ja .nn
        push cx
        push si
        mov bl, [conn_bufs + 3 * CONNMAX + si]
        call hi_pin                 ; -> AL = fuente (FFh no vale)
        cmp al, 0xFF
        je .cn
        pop dx                      ; DX = la entrada
        push dx
        call hi_add
.cn:
        pop si
        pop cx
        inc si
        jmp .c
.nn:
        pop si
        pop cx
        loop .n
.pn:
        pop si
        pop cx
        add si, 5
        dec cx
        jmp .p
.r:
        ret

; BL = nodo -> AL = fuente (MZ_CD, MZ_LIN o MZ_MIC) si es un pin de entrada
; de esas clases con algo físico; FFh si no. Lo pone como entrada
hi_pin:
        call w_type
        cmp ax, 4
        jne .no
        mov cl, 0x0C
        call hda_param
        jc .no
        test al, 0x20               ; puede recibir señal
        jz .no
        mov [hi_caps], eax
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        jc .no
        mov ecx, eax
        shr ecx, 30
        cmp cl, 1                   ; «sin conexión física»
        je .no
        shr eax, 20
        and al, 0x0F
        mov ah, MZ_CD
        cmp al, 3
        je .si
        mov ah, MZ_LIN
        cmp al, 8
        je .si
        mov ah, MZ_MIC
        cmp al, 0x0A
        jne .no
.si:
        push ax
        mov ax, 0x705               ; encendido
        xor cl, cl
        call hda_verb
        mov cl, 0x20                ; entrada (y el micrófono, con su
        pop ax                      ; alimentación al 80 % o al 50 %)
        push ax
        cmp ah, MZ_MIC
        jne .w
        test byte [hi_caps+1], 0x10
        jz .w5
        mov cl, 0x24
        jmp .w
.w5:
        test byte [hi_caps+1], 0x02
        jz .w
        mov cl, 0x21
.w:
        mov ax, 0x707
        call hda_verb
        pop ax
        mov al, ah
        ret
.no:
        mov al, 0xFF
        ret

; AL = fuente, DL = entrada del mezclador hi_mix: a hda_in
hi_add:
        cmp byte [hda_nin], 6
        jae .r
        mov ah, al
        mov si, hda_in
        movzx cx, byte [hda_nin]
.d:
        jcxz .new
        mov al, [hi_mix]            ; (ya está)
        cmp [si], al
        jne .dx
        cmp [si+1], dl
        je .r
.dx:
        add si, 6
        dec cx
        jmp .d
.new:
        mov al, [hi_mix]
        mov [si], al
        mov [si+1], dl
        mov [si+2], ah
        push si
        mov bl, al
        call w_type                 ; (cur_caps del mezclador)
        mov cl, 0x0D
        call amp_caps
        pop si
        mov cl, al
        and cl, 0x7F
        mov [si+3], cl              ; paso de 0 dB
        shr eax, 8
        mov cl, al
        and cl, 0x7F
        mov [si+4], cl              ; pasos
        shr eax, 8
        and al, 0x7F
        inc al
        mov [si+5], al              ; tamaño del paso en cuartos de dB
        inc byte [hda_nin]
.r:
        ret

hi_mix      db 0
hi_caps     dd 0
