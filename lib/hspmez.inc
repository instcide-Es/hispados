; =============================================================================
;  hspmez.inc - el mezclador por fuentes de HSPSND (parte residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Siete fuentes con una escala unica de 0 a 100 % (docs/MEZCLADOR.md):
;  General, Sonidos (la onda), Musica (FM), CD, Linea, Microfono y Altavoz
;  del PC, cada una con su silencio, su balance y, las de entrada, las
;  casillas «Sonar» (pasa a los altavoces) y «Grabar». Y los ajustes: graves,
;  agudos, ganancias, ajuste automatico y refuerzo del microfono. Cada
;  tarjeta lo traduce a lo suyo (vol_apply):
;    Sound Blaster 16   el mezclador CT1745, registros 30h-47h (5 bits para
;                       los volumenes, en pasos de 2 dB; 2 para el altavoz;
;                       4 para los tonos), con la curva en dB
;    Sound Blaster Pro  el CT1345 (04h, 0Ah, 0Ch, 22h, 26h, 28h, 2Eh)
;    Sound Blaster 2.0  General y Sonidos por programa; Musica y CD en el
;                       CT1335 si lo tiene (06h, 08h)
;    AC'97              sus registros de atenuacion (02h, 04h, 0Ah, 0Eh,
;                       10h, 12h, 18h), la fuente de grabacion (1Ah), su
;                       ganancia (1Ch) y los tonos (08h) si los hay
;    HD Audio           General en los amplificadores del codec (o por
;                       programa), CD, Linea y Microfono en las entradas
;                       del mezclador del codec si las hay; Sonidos y Musica
;                       por programa, en la mezcla de HSPSND
;    /SB (emulada)      lo que escribe el juego en el mezclador emulado se
;                       multiplica por lo del usuario (e_gains)
;  La curva es la de siempre: 100 % = 0 dB y 46,5 dB menos en el 1 %
;  ((100 - %) * 15 / 8 cuartos de dB); el 0 % es silencio.
;  Todo esto queda residente. Escrito desde cero con la documentacion
;  publica del CT1745/CT1345 (Sound Blaster Hardware Programming Guide), del
;  AC'97 2.3 y del HD Audio 1.0a.
; =============================================================================

; ---- la interfaz (funciones 09h a 0Fh; ver la cabecera de hspsnd.asm) ----
f_mzcaps:
        mov al, [cs:mz_src]
        mov ah, [cs:mz_hwsrc]
        mov bx, [cs:mz_ajs]
        movzx cx, byte [cs:mz_rec]
        movzx dx, byte [cs:mz_son]
        clc
        retf

f_mzget:
        cmp al, MZ_N
        jae mz_mal
        push si
        movzx si, al
mz_lee:
        mov al, [cs:mz_vol+si]
        mov ah, [cs:mz_flg+si]
        mov bl, [cs:mz_bal+si]
        pop si
        clc
        retf
mz_mal:
        stc
        retf

f_mzset:
        cmp al, MZ_N
        jae mz_mal
        push si
        movzx si, al
        cmp bl, 0xFF
        je .f
        cmp bl, 100
        jbe .v
        mov bl, 100
.v:
        mov [cs:mz_vol+si], bl
.f:
        cmp bh, 0xFF
        je .b
        and bh, MZF_MUDO | MZF_SONAR | MZF_GRABAR
        test si, si
        jnz .f2
        and bh, MZF_MUDO            ; (General: solo el silencio)
.f2:
        test bh, MZF_GRABAR
        jz .f3
        test byte [cs:mz_ajs], MZC_UNA
        jz .f3
        push di                     ; una sola fuente de grabacion: las
        xor di, di                  ; demas, sin «Grabar»
.c:
        and byte [cs:mz_flg+di], ~MZF_GRABAR & 0xFF
        inc di
        cmp di, MZ_N
        jb .c
        pop di
.f3:
        mov [cs:mz_flg+si], bh
.b:
        cmp cl, 0xFF
        je .x
        cmp cl, 100
        jbe .b2
        mov cl, 100
.b2:
        mov [cs:mz_bal+si], cl
.x:
        mov byte [cs:vol_dirty], 1  ; (se aplica al salir de la API)
        mov byte [cs:mz_todo], 1    ; (también lo que no es General)
        mov byte [cs:mz_chg], 1
        jmp mz_lee

f_mzaget:
        cmp al, MZ_NAJ
        jae mz_mal
        push si
        movzx si, al
        mov al, [cs:mz_ajv+si]
        pop si
        clc
        retf

f_mzaset:
        cmp al, MZ_NAJ
        jae mz_mal
        push si
        movzx si, al
        mov al, bl
        mov ah, 100                 ; los limites: tonos 0..100, ganancias
        cmp si, 2                   ; 0..3, casillas 0/1
        jb .l
        mov ah, 3
        cmp si, 4
        jb .l
        mov ah, 1
.l:
        cmp al, ah
        jbe .p
        mov al, ah
.p:
        mov [cs:mz_ajv+si], al
        mov byte [cs:vol_dirty], 1
        mov byte [cs:mz_todo], 1    ; (también lo que no es General)
        mov byte [cs:mz_chg], 1
        pop si
        clc
        retf

f_mzfab:
        test al, al
        jnz .ap
        push si
        push di
        push cx
        xor si, si
.c:
        mov cl, [cs:mz_fab+si]
        mov [cs:mz_vol+si], cl
        inc si
        cmp si, MZ_EST
        jb .c
        pop cx
        pop di
        pop si
        mov byte [cs:mz_chg], 1
        jmp .todo
.ap:
        ; volver a escribirlo todo: también a los nodos de HD Audio que
        ; habían dejado de contestar (hda_cmd)
        push di
        push cx
        push es
        push cs
        pop es
        mov di, hda_fallo1
        mov cx, 64
        xor al, al
        cld
        rep stosb
        pop es
        pop cx
        pop di
        cmp byte [cs:dev_type], 4   ; y General otra vez por el amplificador
        jne .todo                   ; si lo había
        cmp byte [cs:hda_nvol], 0
        je .todo
        mov byte [cs:hw_vol], 1
.todo:
        mov byte [cs:vol_dirty], 1
        mov byte [cs:mz_todo], 1
        clc
        retf

f_mzchg:
        cmp al, 1
        jb .lee
        mov ah, 0
        je .p
        mov ah, 1
.p:
        mov [cs:mz_chg], ah
.lee:
        mov al, [cs:mz_chg]
        clc
        retf

; ---- BL = fuente, BH = lado (0 izquierdo, 1 derecho) -> AL = atenuacion en
; cuartos de dB (0..187), con el balance; CF = 1 (y AL = FFh) si no suena
; (silencio o 0 %). El Microfono y el Altavoz son de un canal: sin balance.
mz_q:
        push si
        push dx
        movzx si, bl
        test byte [cs:mz_flg+si], MZF_MUDO
        jnz .zero
        mov al, [cs:mz_vol+si]
        cmp bl, MZ_MIC
        jae .nb
        mov dl, [cs:mz_bal+si]
        test bh, bh
        jnz .der
        cmp dl, 50                  ; izquierdo: baja si va a la derecha
        jbe .nb
        mov dh, 100
        sub dh, dl
        jmp .esc
.der:
        cmp dl, 50                  ; derecho: baja si va a la izquierda
        jae .nb
        mov dh, dl
.esc:
        mul dh                      ; AL * DH / 50
        mov dl, 50
        div dl
.nb:
        test al, al
        jz .zero
        mov ah, 100
        sub ah, al
        mov al, ah
        mov ah, 15
        mul ah
        shr ax, 3
        clc
        jmp .r
.zero:
        mov al, 0xFF
        stc
.r:
        pop dx
        pop si
        ret

; AL = atenuacion en cuartos de dB -> AX = factor por programa (256 = 0 dB;
; la mitad cada 6 dB, y en medio en linea recta)
q_mul:
        push bx
        push cx
        push dx
        xor ah, ah
        mov cl, 24
        div cl
        mov cl, al
        mov bx, 256
        shr bx, cl
        mov al, ah
        xor ah, ah
        mul bx
        mov cx, 48
        div cx
        sub bx, ax
        mov ax, bx
        pop dx
        pop cx
        pop bx
        ret

; BL = fuente, BH = lado -> AX = factor por programa de esa fuente, con
; General si la tarjeta no tiene volumen propio (hw_vol = 0) y sin la fuente
; si la lleva la tarjeta (mz_hwsrc)
mz_fsoft:
        push cx
        push dx
        xor dx, dx                  ; DX = atenuacion total
        cmp byte [cs:vol_zero], 0
        jne .z
        push bx
        mov bl, MZ_GEN
        call mz_q
        pop bx
        jc .z
        cmp byte [cs:hw_vol], 0
        jne .s
        movzx dx, al
.s:
        mov cl, bl
        mov al, 1
        shl al, cl
        test [cs:mz_hwsrc], al
        jnz .t
        call mz_q
        jc .z
        movzx ax, al
        add dx, ax
.t:
        cmp dx, 240
        jbe .m
        mov dx, 240
.m:
        mov al, dl
        call q_mul
        jmp .r
.z:
        xor ax, ax
.r:
        pop dx
        pop cx
        ret

; llevar el volumen y el mezclador a la tarjeta. Conserva todo.
vol_apply:
        pushad
.otra:
        mov byte [cs:vol_dirty], 0
        ; General, por lados: att_q (izquierdo), att_r (derecho), vol_zero
        mov byte [cs:vol_zero], 0
        mov bx, MZ_GEN
        call mz_q
        mov [cs:att_q], al
        mov bh, 1
        call mz_q
        mov [cs:att_r], al
        cmp byte [cs:vol_mute], 0
        jne .cero
        cmp byte [cs:vol_pct], 0
        jne .nc
.cero:
        mov byte [cs:vol_zero], 1
.nc:
        ; factores por programa: Sonidos y Musica
        mov bx, MZ_SON
        call mz_fsoft
        mov [cs:soft_l], ax
        mov [cs:soft_mul], ax
        mov bh, 1
        call mz_fsoft
        mov [cs:soft_r], ax
        mov bx, MZ_MUS
        call mz_fsoft
        mov [cs:mus_l], ax
        mov bh, 1
        call mz_fsoft
        mov [cs:mus_r], ax
        ; y la tarjeta
        mov al, [cs:dev_type]
        test al, al
        jz .r
        cmp al, 2
        je .sb16
        jb .sb
        cmp al, 3
        je .ac
        ; HD Audio: hace falta el modo irreal; en modo protegido o V86 (sin
        ; la ventana de HSPEMM) queda pendiente para la próxima llamada
        call hda_reach
        jnc .h
        mov byte [cs:vol_dirty], 1
        jmp .r
.h:
        push fs
        call a20_open
        call unreal
        mov byte [cs:hda_vfail], 0
        call hda_volume
        cmp byte [cs:mz_todo], 0    ; las entradas del mezclador, solo si
        je .hx                      ; han cambiado (no con 05h-07h ni con
        call hda_mzin               ; las teclas de volumen)
        mov byte [cs:mz_todo], 0
.hx:
        call a20_restore
        pop fs
        ; si el amplificador no contesta, General va por programa (en la
        ; mezcla de HSPSND) y se vuelven a hacer las cuentas
        cmp byte [cs:hda_vfail], 0
        je .r
        mov byte [cs:hw_vol], 0
        jmp .otra
.sb16:
        call mz_sb16
        jmp .r
.sb:
        call mz_sbpro
        jmp .r
.ac:
        call mz_ac97
.r:
        popad
        ret

; AX = muestra de 16 bits -> por el factor por programa de los Sonidos
; (vol_scale: izquierdo; vol_scale_r: derecho)
vol_scale:
        push si
        mov si, soft_l
        jmp vol_sc
vol_scale_r:
        push si
        mov si, soft_r
vol_sc:
        cmp word [cs:si], 256
        je .r
        push dx
        imul word [cs:si]
        mov al, ah
        mov ah, dl
        pop dx
.r:
        pop si
        ret

; -> AL = las casillas de la fuente BL, solo si no esta en silencio
; (para «Sonar» y «Grabar»)
mz_flags:
        push si
        movzx si, bl
        mov al, [cs:mz_flg+si]
        test al, MZF_MUDO
        jz .r
        xor al, al
.r:
        pop si
        ret

; =============================================================================
;  Sound Blaster 16: CT1745
; =============================================================================
; AL = atenuacion (CF = silencio) -> AL = registro de 5 bits (bits 7-3)
ct5:
        jc .z
        add al, 4
        shr al, 3
        mov ah, 31
        sub ah, al
        jnc .k
        xor ah, ah
.k:
        mov al, ah
        shl al, 3
        ret
.z:
        xor al, al
        ret
; AL = 0..100 -> AL = tono de 4 bits (bits 7-4; 8 = 0 dB)
ct_tono:
        push dx
        mov ah, 15
        mul ah
        add ax, 50
        mov dl, 100
        div dl
        shl al, 4
        pop dx
        ret

mz_sb16:
        mov cx, 0x3000              ; CH = registro, CL = fuente
.f:
        mov bl, cl
        xor bh, bh
        call mz_q
        call ct5
        mov ah, ch
        call sb_mix
        inc bh
        call mz_q
        call ct5
        mov ah, ch
        inc ah
        call sb_mix
        add ch, 2
        inc cl
        cmp cl, MZ_MIC
        jb .f
        mov bx, MZ_MIC              ; 3Ah micrófono
        call mz_q
        call ct5
        mov ah, 0x3A
        call sb_mix
        mov bx, MZ_ALT              ; 3Bh altavoz del PC: 2 bits (-18, -12,
        call mz_q                   ; -6 y 0 dB), repartidos por igual en
        jc .az                      ; la escala (50 % = -6 dB)
        mov al, [cs:mz_vol+MZ_ALT]
        mov ah, 3
        mul ah
        add ax, 50
        mov dl, 100
        div dl
        shl al, 6
        jmp .aw
.az:
        xor al, al
.aw:
        mov ah, 0x3B
        call sb_mix
        ; 3Ch: lo que pasa a los altavoces (micrófono, CD y línea con «Sonar»)
        xor dl, dl
        mov bl, MZ_MIC
        call mz_flags
        test al, MZF_SONAR
        jz .o1
        or dl, 0x01
.o1:
        mov bl, MZ_CD
        call mz_flags
        test al, MZF_SONAR
        jz .o2
        or dl, 0x06
.o2:
        mov bl, MZ_LIN
        call mz_flags
        test al, MZF_SONAR
        jz .o3
        or dl, 0x18
.o3:
        mov al, dl
        mov ah, 0x3C
        call sb_mix
        ; 3Dh y 3Eh: lo que se graba en el canal izquierdo y en el derecho
        xor dx, dx                  ; DL izquierdo, DH derecho
        mov bl, MZ_MIC
        call mz_flags
        test al, MZF_GRABAR
        jz .i1
        or dx, 0x0101
.i1:
        mov bl, MZ_CD
        call mz_flags
        test al, MZF_GRABAR
        jz .i2
        or dx, 0x0204
.i2:
        mov bl, MZ_LIN
        call mz_flags
        test al, MZF_GRABAR
        jz .i3
        or dx, 0x0810
.i3:
        mov bl, MZ_MUS
        call mz_flags
        test al, MZF_GRABAR
        jz .i4
        or dx, 0x2040
.i4:
        mov al, dl
        mov ah, 0x3D
        call sb_mix
        mov al, dh
        mov ah, 0x3E
        call sb_mix
        ; ganancias (bits 7-6), ajuste automático del micrófono y tonos
        mov al, [cs:mz_ajv+2]
        shl al, 6
        mov ah, 0x3F
        call sb_mix
        inc ah
        call sb_mix
        mov al, [cs:mz_ajv+3]
        shl al, 6
        mov ah, 0x41
        call sb_mix
        inc ah
        call sb_mix
        mov al, [cs:mz_ajv+4]       ; 43h bit 0: 0 = con el ajuste automático
        xor al, 1
        and al, 1
        mov ah, 0x43
        call sb_mix
        mov al, [cs:mz_ajv+1]       ; agudos 44h/45h
        call ct_tono
        mov ah, 0x44
        call sb_mix
        inc ah
        call sb_mix
        mov al, [cs:mz_ajv]         ; graves 46h/47h
        call ct_tono
        mov ah, 0x46
        call sb_mix
        inc ah
        call sb_mix
        ret

; =============================================================================
;  Sound Blaster Pro (CT1345) y 2.0 (CT1335)
; =============================================================================
; AL = atenuacion (CF = silencio) -> AL = valor de 4 bits (unos 3 dB por paso)
sp4:
        jc .z
        xor ah, ah
        add al, 6
        mov dl, 12
        div dl
        mov ah, 15
        sub ah, al
        jnc .k
        xor ah, ah
.k:
        mov al, ah
        ret
.z:
        xor al, al
        ret
; BL = fuente, CL = 0 o MZF_SONAR (si hace falta «Sonar») -> AL = (izquierdo
; << 4) | derecho
sp_par:
        test cl, cl
        jz .s
        call mz_flags
        test al, cl
        jz .cero
.s:
        xor bh, bh
        call mz_q
        call sp4
        mov ch, al
        shl ch, 4
        mov bh, 1
        call mz_q
        call sp4
        or al, ch
        ret
.cero:
        xor al, al
        ret

mz_sbpro:
        cmp byte [cs:sb_major], 3
        jb mz_sb2
        mov si, sp_regs
.r:
        mov bl, [cs:si]
        cmp bl, 0xFF
        je .mic
        mov cl, [cs:si+2]
        call sp_par
        mov ah, [cs:si+1]
        call sb_mix
        add si, 3
        jmp .r
.mic:
        ; 0Ah micrófono: bits 2-1 (con «Sonar»)
        xor dl, dl
        mov bl, MZ_MIC
        call mz_flags
        test al, MZF_SONAR
        jz .mw
        xor bh, bh
        call mz_q
        jc .mw
        xor ah, ah
        add al, 24
        mov dh, 48
        div dh
        mov dl, 3
        sub dl, al
        jnc .mk
        xor dl, dl
.mk:
        shl dl, 1
.mw:
        mov al, dl
        mov ah, 0x0A
        call sb_mix
        ; 0Ch: de dónde se graba (bits 2-1: 0 micrófono, 1 CD, 3 línea)
        xor dl, dl
        mov bl, MZ_CD
        call mz_flags
        test al, MZF_GRABAR
        jz .g1
        mov dl, 1 << 1
.g1:
        mov bl, MZ_LIN
        call mz_flags
        test al, MZF_GRABAR
        jz .g2
        mov dl, 3 << 1
.g2:
        mov al, dl
        mov ah, 0x0C
        call sb_mix
        ret
; fuente, registro, casilla que hace falta
sp_regs     db MZ_GEN, 0x22, 0, MZ_SON, 0x04, 0, MZ_MUS, 0x26, 0
            db MZ_CD, 0x28, MZF_SONAR, MZ_LIN, 0x2E, MZF_SONAR, 0xFF

; Sound Blaster 2.0: si tiene el CT1335, la Música (06h) y el CD (08h), con
; General dentro (la voz y General van por programa: hw_vol = 0)
mz_sb2:
        cmp byte [cs:mz_ct1335], 0
        je .r
        mov bl, MZ_MUS
        mov ah, 0x06
        call .uno
        mov bl, MZ_CD
        mov ah, 0x08
.uno:
        push ax
        call mz_flags
        cmp bl, MZ_CD
        jne .s
        test al, MZF_SONAR
        jz .cero
.s:
        xor bh, bh
        call mz_q
        jc .cero
        movzx dx, al
        push bx
        mov bx, MZ_GEN
        call mz_q
        pop bx
        jc .cero
        movzx ax, al
        add ax, dx                  ; 3 bits: unos 4 dB por paso
        add ax, 8
        mov dl, 16
        div dl
        mov ah, 7
        sub ah, al
        jnc .k
        xor ah, ah
.k:
        mov al, ah
        shl al, 1
        jmp .w
.cero:
        xor al, al
.w:
        pop dx
        mov ah, dh
        jmp sb_mix
.r:
        ret

; =============================================================================
;  AC'97
; =============================================================================
; AL = atenuacion (CF = silencio) -> AL = pasos de 1,5 dB (como mucho 31)
ac_pasos:
        jc .z
        xor ah, ah
        add al, 3
        mov dl, 6
        div dl
        cmp al, 31
        jbe .r
.z:
        mov al, 31
.r:
        ret
; BL = fuente, CL = lo que hay que sumar a los pasos (0 o 8: los registros
; que tienen ganancia, con el 0 dB en 08h) -> AX = registro de dos canales
; (izquierdo en los bits 12-8), con el silencio (bit 15) si no suena ninguno
ac_par:
        xor bh, bh
        call mz_q
        pushf
        call ac_pasos
        add al, cl
        cmp al, 31
        jbe .l
        mov al, 31
.l:
        mov ch, al
        mov bh, 1
        call mz_q
        pushf
        call ac_pasos
        add al, cl
        cmp al, 31
        jbe .d
        mov al, 31
.d:
        mov ah, ch
        popf
        jnc .s
        popf
        jnc .s2
        or ah, 0x80                 ; ninguno de los dos suena
        ret
.s:
        popf
.s2:
        ret
; AX = valor, DL = registro
ac_out:
        push dx
        movzx dx, dl
        add dx, [cs:nam]
        out dx, ax
        pop dx
        ret

mz_ac97:
        ; maestro y auriculares (0 dB en 00h)
        mov bl, MZ_GEN
        xor cl, cl
        call ac_par
        mov dl, 0x02
        call ac_out
        mov dl, 0x04
        call ac_out
        ; PCM: los Sonidos (0 dB en 08h)
        mov bl, MZ_SON
        mov cl, 8
        call ac_par
        mov dl, 0x18
        call ac_out
        ; CD y línea: con «Sonar»
        test byte [cs:mz_ac], MZA_CD
        jz .l
        mov bl, MZ_CD
        mov dl, 0x12
        call .entrada
.l:
        test byte [cs:mz_ac], MZA_LIN
        jz .m
        mov bl, MZ_LIN
        mov dl, 0x10
        call .entrada
.m:
        ; micrófono (0Eh): 5 bits, 0 dB en 08h, refuerzo de 20 dB en el bit 6
        test byte [cs:mz_ac], MZA_MIC
        jz .b
        mov bl, MZ_MIC
        mov cl, 8
        call ac_par
        and ax, 0x801F
        test byte [cs:mz_ajv+5], 1
        jz .m1
        or al, 0x40
.m1:
        push ax
        call mz_flags
        test al, MZF_SONAR
        pop ax
        jnz .m2
        or ah, 0x80
.m2:
        mov dl, 0x0E
        call ac_out
.b:
        ; altavoz del PC (0Ah): bits 4-1, 3 dB por paso
        test byte [cs:mz_ac], MZA_BEEP
        jz .g
        mov bx, MZ_ALT
        call mz_q
        jc .bm
        xor ah, ah
        add al, 6
        mov dl, 12
        div dl
        cmp al, 15
        jbe .bk
        mov al, 15
.bk:
        shl al, 1
        xor ah, ah
        jmp .bw
.bm:
        mov ax, 0x8000
.bw:
        mov dl, 0x0A
        call ac_out
.g:
        ; de dónde se graba (1Ah): 0 micrófono, 1 CD, 4 línea
        xor cx, cx
        mov bl, MZ_CD
        call mz_flags
        test al, MZF_GRABAR
        jz .g1
        mov cl, 1
.g1:
        mov bl, MZ_LIN
        call mz_flags
        test al, MZF_GRABAR
        jz .g2
        mov cl, 4
.g2:
        mov ch, cl
        mov ax, cx
        mov dl, 0x1A
        call ac_out
        ; ganancia de grabación (1Ch): x1 x2 x4 x8 = 0, 6, 12, 18 dB
        mov al, [cs:mz_ajv+2]
        shl al, 2
        mov ah, al
        mov dl, 0x1C
        call ac_out
        ; tonos (08h): 4 bits, 0 = +10,5 dB, 7 = 0 dB, 14 = -10,5 dB
        test byte [cs:mz_ac], MZA_TONO
        jz .r
        mov al, [cs:mz_ajv]
        call .tono
        mov ch, al
        mov al, [cs:mz_ajv+1]
        call .tono
        mov ah, ch
        mov dl, 0x08
        call ac_out
.r:
        ret
.tono:
        mov ah, 14
        mul ah
        add ax, 50
        mov cl, 100
        div cl
        mov ah, 14
        sub ah, al
        mov al, ah
        ret
; BL = fuente (CD o línea), DL = registro
.entrada:
        push dx
        mov cl, 8
        call ac_par
        push ax
        call mz_flags
        test al, MZF_SONAR
        pop ax
        jnz .e1
        or ah, 0x80
.e1:
        pop dx
        jmp ac_out

; =============================================================================
;  HD Audio
; =============================================================================
; General a los amplificadores elegidos (hda_vols), cada lado con lo suyo
hda_volume:
        pushad
        cmp byte [cs:hw_vol], 0     ; (por programa: nada que mandar)
        je .r
        mov si, hda_vols
        movzx di, byte [cs:hda_nvol]
.l:
        test di, di
        jz .r
        xor bp, bp                  ; BP = 0 izquierdo, 1 derecho
.lado:
        movzx ax, byte [cs:att_q+bp]
        mov bl, [cs:si+4]           ; tamaño del paso en cuartos de dB
        mov bh, bl
        shr bh, 1
        add al, bh
        adc ah, 0
        div bl                      ; pasos por debajo de 0 dB
        mov ah, [cs:si+2]
        sub ah, al
        jnc .g
        xor ah, ah
.g:
        mov al, ah
        cmp byte [cs:vol_zero], 0
        jne .mu
        cmp byte [cs:att_q+bp], 0xFF
        jne .nm
.mu:
        mov al, 0x80                ; silencio (y ganancia mínima)
.nm:
        movzx cx, al
        mov al, [cs:si+1]
        test al, 0x80
        jz .in
        or ch, 0x80                 ; salida
        jmp .w
.in:
        mov ch, al
        and ch, 0x0F
        or ch, 0x40                 ; entrada n
.w:
        or ch, 0x20                 ; izquierdo (bit 13) o derecho (bit 12)
        test bp, bp
        jz .w2
        xor ch, 0x30
.w2:
        mov bl, [cs:si]
        mov al, 3
        call hda_verb4
        jnc .ok
        mov byte [cs:hda_vfail], 1  ; (no contesta: vol_apply)
.ok:
        inc bp
        cmp bp, 2
        jb .lado
        add si, 6
        dec di
        jmp .l
.r:
        popad
        ret
hda_vfail   db 0

; CD, Línea y Micrófono: las entradas del mezclador del códec que llevan a
; ellos (hda_in; las busca hda_inputs al instalarse). Sin «Sonar», o en
; silencio, la entrada se calla
hda_mzin:
        pushad
        mov si, hda_in
        movzx di, byte [cs:hda_nin]
.l:
        test di, di
        jz .r
        mov bl, [cs:si+2]
        call mz_flags
        mov dl, al                  ; DL = casillas
        xor bp, bp
.lado:
        mov bl, [cs:si+2]
        mov bh, 0
        test bp, bp
        jz .q
        mov bh, 1
.q:
        call mz_q
        mov cl, 0x80
        jc .w
        test dl, MZF_SONAR
        jz .w
        movzx ax, al
        cmp byte [cs:si+4], 0
        je .w0
        mov bl, [cs:si+5]
        mov bh, bl
        shr bh, 1
        add al, bh
        adc ah, 0
        div bl
        mov ah, [cs:si+3]
        sub ah, al
        jnc .g
        xor ah, ah
.g:
        mov cl, ah
        jmp .w
.w0:
        mov cl, [cs:si+3]           ; sin pasos: solo se quita el silencio
.w:
        movzx cx, cl
        mov ch, [cs:si+1]
        and ch, 0x0F
        or ch, 0x60                 ; entrada n, izquierdo
        test bp, bp
        jz .w2
        xor ch, 0x30                ; derecho
.w2:
        mov bl, [cs:si]
        mov al, 3
        call hda_verb4
        inc bp
        cmp bp, 2
        jb .lado
        add si, 6
        dec di
        jmp .l
.r:
        popad
        ret
