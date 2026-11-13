; SBTEST.COM - prueba de una Sound Blaster (de verdad o la emulada por
; HSPSND /SB) como la usa un juego: puertos del DSP, DMA del 8237 e IRQ.
;   SBTEST        todo: reinicio, versión (E1h), E0h, mezclador, IRQ (F2h),
;                 bloque único (14h, 500 Hz, 0,6 s), autoinicio (1Ch, 802 Hz,
;                 8 bloques), SB16 de 16 bits estéreo (B6h, 1206 Hz, 24
;                 bloques), AdLib (detección y un tono FM de 600 Hz)
; La tarjeta, de BLASTER (A, I, D, H); si no está, 220h, IRQ 5, DMA 1 y 5.
; tests/run_sbemu_tests.sh graba lo que suena y mide los tonos.
        cpu 386
        bits 16
        org 0x100

%macro SAY 1
        mov si, %1
        call puts
%endmacro

start:
        call parse_blaster
        SAY m_cfg
        mov ax, [base]
        call hex3
        SAY m_i
        movzx ax, byte [irq]
        call pdec
        SAY m_d
        movzx ax, byte [dma]
        call pdec
        SAY m_h
        movzx ax, byte [hdma]
        call pdec
        SAY m_nl
        ; ---- reinicio ----
        call dsp_reset
        jnc .rok
        SAY m_rst_ko
        mov ax, 0x4C01
        int 0x21
.rok:
        SAY m_rst_ok
        ; ---- versión ----
        mov al, 0xE1
        call dsp_out
        call dsp_in
        mov [ver+1], al
        call dsp_in
        mov [ver], al
        SAY m_ver
        movzx ax, byte [ver+1]
        call pdec
        mov al, '.'
        call putc
        mov al, [ver]
        aam
        add ax, 0x3030
        xchg al, ah
        call putc
        mov al, ah
        call putc
        SAY m_nl
        ; ---- E0h: devuelve el complemento ----
        mov al, 0xE0
        call dsp_out
        mov al, 0xA5
        call dsp_out
        call dsp_in
        cmp al, 0x5A
        mov si, m_e0_ok
        je .e0
        mov si, m_e0_ko
.e0:
        call puts
        ; ---- mezclador (SB16): IRQ y DMA ----
        cmp byte [ver+1], 4
        jb .nomix
        SAY m_mix
        mov al, 0x80
        call mix_rd
        call hex2
        mov al, ' '
        call putc
        mov al, 0x81
        call mix_rd
        call hex2
        ; maestro de la SB Pro: se escribe y se lee
        mov dx, [base]
        add dx, 4
        mov al, 0x22
        out dx, al
        inc dx
        mov al, 0xEE
        out dx, al
        mov al, 0x22
        call mix_rd
        cmp al, 0xEE
        mov si, m_mix_ok
        je .mx
        mov si, m_mix_ko
.mx:
        call puts
        ; (al máximo, que es como lo dejan los juegos)
        mov dx, [base]
        add dx, 4
        mov al, 0x22
        out dx, al
        inc dx
        mov al, 0xFF
        out dx, al
.nomix:
        ; ---- IRQ ----
        call hook
        mov word [irqs], 0
        mov al, 0xF2
        call dsp_out
        mov cx, 18
        call wait_irq
        mov si, m_f2_ok
        jnc .f2
        mov si, m_f2_ko
.f2:
        call puts
        mov al, 0xD1                ; altavoz
        call dsp_out

        ; ---- bloque único: 8 bits mono, 11025 Hz, 500 Hz, 6615 muestras ----
        call pick_buf
        mov bx, 2972                ; 500 * 65536 / 11025
        mov cx, 6615
        call fill8
        mov cx, 6615 - 1
        mov ah, 0x48                ; modo: un solo bloque, lectura
        call dma8
        mov al, 0x40
        call dsp_out
        mov al, 256 - 91            ; 1000000 / 91 = 10989 Hz
        call dsp_out
        mov word [irqs], 0
        call ticks
        mov [t0], ax
        mov al, 0x14
        call dsp_out
        mov al, (6615 - 1) & 0xFF
        call dsp_out
        mov al, (6615 - 1) >> 8
        call dsp_out
        mov cx, 54
        call wait_irq
        jc .s_ko
        call ticks
        sub ax, [t0]
        push ax
        SAY m_single
        pop ax
        push ax
        call pdec
        SAY m_ticks
        pop ax
        cmp ax, 8                   ; 0,6 s = 11 tics
        jb .s_bad
        cmp ax, 14
        ja .s_bad
        SAY m_ok
        jmp .s_end
.s_bad:
        SAY m_bad
        jmp .s_end
.s_ko:
        SAY m_single_ko
.s_end:
        call pause

        ; ---- autoinicio: 8 bits mono, 22050 Hz, 802 Hz (149 periodos en
        ; 4096 muestras), bloques de 2048: 8 IRQ ----
        mov bx, 149 * 16
        mov cx, 4096
        call fill8
        mov cx, 4096 - 1
        mov ah, 0x58                ; autoinicio
        call dma8
        mov al, 0x40
        call dsp_out
        mov al, 256 - 45            ; 22222 Hz
        call dsp_out
        mov al, 0x48
        call dsp_out
        mov al, (2048 - 1) & 0xFF
        call dsp_out
        mov al, (2048 - 1) >> 8
        call dsp_out
        mov word [irqs], 0
        call ticks
        mov [t0], ax
        mov al, 0x1C
        call dsp_out
        ; mientras suena, la cuenta del 8237 se mueve
        mov byte [moved], 0
        call dma_count
        mov [c0], ax
.a_w:
        call dma_count
        cmp ax, [c0]
        je .a_n
        cmp ax, 4096
        jae .a_n
        mov byte [moved], 1
.a_n:
        cmp word [irqs], 8
        jae .a_done
        call ticks
        sub ax, [t0]
        cmp ax, 54
        jb .a_w
.a_done:
        mov al, 0xDA                ; acabar al final de este bloque
        call dsp_out
        mov cx, 9
        call wait_irq2
        call ticks
        sub ax, [t0]
        push ax
        SAY m_auto
        mov ax, [irqs]
        call pdec
        SAY m_irqs
        pop ax
        push ax
        call pdec
        SAY m_ticks
        pop ax
        cmp word [irqs], 9
        jne .a_bad
        cmp ax, 11                  ; 9 x 2048 / 22222 = 0,83 s = 15 tics
        jb .a_bad
        cmp ax, 19
        ja .a_bad
        SAY m_ok
        jmp .a_cnt
.a_bad:
        SAY m_bad
.a_cnt:
        mov si, m_cnt_ok
        cmp byte [moved], 0
        jne .a_c
        mov si, m_cnt_ko
.a_c:
        call puts
        call pause

        ; ---- SB16: 16 bits estéreo con signo, 44100 Hz, 1206 Hz (56
        ; periodos en 2048 tramas), bloques de 1024 tramas: 24 IRQ ----
        cmp byte [ver+1], 4
        jb .no16
        call pick_buf16
        call fill16
        call dma16
        mov al, 0x41
        call dsp_out
        mov al, 44100 >> 8
        call dsp_out
        mov al, 44100 & 0xFF
        call dsp_out
        mov byte [is16], 1
        mov word [irqs], 0
        call ticks
        mov [t0], ax
        mov al, 0xB6                ; 16 bits, salida, autoinicio, FIFO
        call dsp_out
        mov al, 0x30                ; estéreo con signo
        call dsp_out
        mov al, (2048 - 1) & 0xFF
        call dsp_out
        mov al, (2048 - 1) >> 8
        call dsp_out
.h_w:
        cmp word [irqs], 24
        jae .h_done
        call ticks
        sub ax, [t0]
        cmp ax, 54
        jb .h_w
.h_done:
        mov al, 0xD9
        call dsp_out
        mov cx, 9
        call wait_irq2
        call ticks
        sub ax, [t0]
        push ax
        SAY m_s16
        mov ax, [irqs]
        call pdec
        SAY m_irqs
        pop ax
        push ax
        call pdec
        SAY m_ticks
        pop ax
        mov byte [is16], 0
        cmp word [irqs], 25
        jne .h_bad
        cmp ax, 8                   ; 25 x 1024 / 44100 = 0,58 s = 10,6 tics
        jb .h_bad
        cmp ax, 14
        ja .h_bad
        SAY m_ok
        jmp .no16
.h_bad:
        SAY m_bad
.no16:
        call dsp_reset
        call unhook
        call pause

        ; ---- AdLib ----
        mov ax, 0x0460              ; parar y tapar los dos temporizadores
        call opl
        mov ax, 0x0480              ; borrar los avisos
        call opl
        call opl_st
        mov bl, al
        mov ax, 0x02FF
        call opl
        mov ax, 0x0421              ; arrancar el temporizador 1
        call opl
        mov cx, 400
.od:
        call opl_st
        loop .od
        mov bh, al
        mov ax, 0x0460
        call opl
        mov ax, 0x0480
        call opl
        and bx, 0xE0E0
        cmp bx, 0xC000
        je .oplok
        SAY m_opl_ko
        jmp .end
.oplok:
        SAY m_opl_ok
        ; un tono: canal 0, el portador solo (modulador en silencio), 600 Hz
        mov ax, 0x0120              ; formas de onda
        call opl
        mov ax, 0x2001              ; modulador: x1
        call opl
        mov ax, 0x403F              ; ... en silencio
        call opl
        mov ax, 0x60F0
        call opl
        mov ax, 0x80FF
        call opl
        mov ax, 0x2321              ; portador: x1, sostenido
        call opl
        mov ax, 0x4300              ; volumen máximo
        call opl
        mov ax, 0x63F0              ; ataque rápido, sin caída
        call opl
        mov ax, 0x830F              ; sostenido al máximo, suelta rápida
        call opl
        mov ax, 0xC001              ; los dos operadores sumados
        call opl
        mov ax, 0xA017              ; 791 = 317h, bloque 4: 600 Hz
        call opl
        mov ax, 0xB033              ; tecla pulsada
        call opl
        mov cx, 13
        call wait_ticks
        mov ax, 0xB013              ; soltar
        call opl
        SAY m_fm
        call pause
.end:
        mov ax, 0x4C00
        int 0x21

; ---- BLASTER ----
parse_blaster:
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .r
        cmp dword [es:di], 'BLAS'
        jne .nx
        cmp dword [es:di+4], 'TER='
        je .f
.nx:
        inc di
        cmp byte [es:di-1], 0
        jne .nx
        jmp .v
.f:
        add di, 8
.t:
        mov al, [es:di]
        test al, al
        jz .r
        inc di
        cmp al, 'A'
        je .a
        cmp al, 'I'
        je .i
        cmp al, 'D'
        je .d
        cmp al, 'H'
        je .h
        jmp .t
.a:
        xor bx, bx
.al:
        mov al, [es:di]
        sub al, '0'
        jb .ae
        cmp al, 9
        ja .ae
        shl bx, 4
        or bl, al
        inc di
        jmp .al
.ae:
        mov [base], bx
        jmp .t
.i:
        call .num
        mov [irq], al
        jmp .t
.d:
        call .num
        mov [dma], al
        jmp .t
.h:
        call .num
        mov [hdma], al
        jmp .t
.num:
        xor ax, ax
.nl:
        mov cl, [es:di]
        sub cl, '0'
        jb .ne
        cmp cl, 9
        ja .ne
        mov ah, 10
        mul ah
        add al, cl
        inc di
        jmp .nl
.ne:
        ret
.r:
        push cs
        pop es
        ret

; ---- DSP ----
dsp_out:
        push cx
        push dx
        push ax
        mov dx, [base]
        add dx, 0x0C
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .ok
        loop .w
.ok:
        pop ax
        out dx, al
        pop dx
        pop cx
        ret
dsp_in:
        push cx
        push dx
        mov dx, [base]
        add dx, 0x0E
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .ok
        loop .w
        mov al, 0
        stc
        jmp .r
.ok:
        sub dx, 4
        in al, dx
        clc
.r:
        pop dx
        pop cx
        ret
dsp_reset:
        mov dx, [base]
        add dx, 6
        mov al, 1
        out dx, al
        mov cx, 100
.d:
        in al, 0x80
        loop .d
        xor al, al
        out dx, al
        mov cx, 100
.t:
        call dsp_in
        jc .n
        cmp al, 0xAA
        je .ok
.n:
        loop .t
        stc
        ret
.ok:
        clc
        ret
; AL = registro del mezclador -> AL
mix_rd:
        mov dx, [base]
        add dx, 4
        out dx, al
        inc dx
        in al, dx
        ret

; ---- FM ----
; AH = registro, AL = dato
opl:
        push dx
        push cx
        mov dx, 0x388
        xchg al, ah
        out dx, al
        mov cx, 6
.d1:
        in al, dx
        loop .d1
        inc dx
        xchg al, ah
        out dx, al
        dec dx
        mov cx, 36
.d2:
        in al, dx
        loop .d2
        pop cx
        pop dx
        ret
opl_st:
        mov dx, 0x388
        in al, dx
        ret

; ---- IRQ ----
hook:
        mov al, [irq]
        add al, 8
        mov ah, 0x35
        int 0x21
        mov [old], bx
        mov [old+2], es
        push cs
        pop es
        mov al, [irq]
        add al, 8
        mov dx, isr
        mov ah, 0x25
        int 0x21
        mov cl, [irq]
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0x21
        mov [oldmask], al
        and al, ah
        out 0x21, al
        ret
unhook:
        mov al, [oldmask]
        out 0x21, al
        push ds
        lds dx, [old]
        mov al, [cs:irq]
        add al, 8
        mov ah, 0x25
        int 0x21
        pop ds
        ret
isr:
        push ax
        push dx
        inc word [cs:irqs]
        mov dx, [cs:base]
        add dx, 0x0E
        cmp byte [cs:is16], 0
        je .a
        inc dx
.a:
        in al, dx
        mov al, 0x20
        out 0x20, al
        pop dx
        pop ax
        iret
; esperar una IRQ (CX tics como mucho) -> CF si no llega
wait_irq:
        mov word [want], 1
        jmp wait_n
; ... o una más de las que hay
wait_irq2:
        mov ax, [irqs]
        inc ax
        mov [want], ax
wait_n:
        call ticks
        mov bx, ax
.l:
        mov ax, [irqs]
        cmp ax, [want]
        jae .ok
        call ticks
        sub ax, bx
        cmp ax, cx
        jb .l
        stc
        ret
.ok:
        clc
        ret
ticks:
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        ret
; esperar CX tics
wait_ticks:
        call ticks
        mov bx, ax
.l:
        call ticks
        sub ax, bx
        cmp ax, cx
        jb .l
        ret
pause:
        mov cx, 8
        jmp wait_ticks

; ---- búferes: dentro de una página de DMA (64 KB / 128 KB) ----
pick_buf:
        mov bx, buf
        call .lin
        mov ecx, eax
        add ecx, 8191
        xor ecx, eax
        test ecx, 0xFFFF0000
        jz .ok
        add bx, 8192                ; el segundo trozo no la cruza
.ok:
        mov [bufp], bx
        ret
.lin:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ebx, bx
        add eax, ebx
        ret
pick_buf16:
        mov bx, buf
        call pick_buf.lin
        test al, 1
        jz .ev
        inc bx
        inc eax
.ev:
        mov ecx, eax
        add ecx, 8191
        xor ecx, eax
        test ecx, 0xFFFE0000
        jz .ok
        add bx, 8192
.ok:
        mov [bufp], bx
        ret

; onda cuadrada de 8 bits: CX muestras, BX = paso de fase (65536 = periodo)
fill8:
        mov di, [bufp]
        xor dx, dx
.l:
        mov al, 0xC0
        test dh, 0x80
        jz .s
        mov al, 0x40
.s:
        stosb
        add dx, bx
        loop .l
        ret
; 2048 tramas de 16 bits estéreo: 56 periodos
fill16:
        mov di, [bufp]
        xor dx, dx
        mov cx, 2048
.l:
        mov ax, 16000
        test dh, 0x80
        jz .s
        neg ax
.s:
        stosw
        stosw
        add dx, 56 * 32
        loop .l
        ret

; programar el canal de 8 bits: AH = modo (sin el canal), CX = cuenta - 1
dma8:
        push ax
        mov al, [dma]
        or al, 4
        out 0x0A, al                ; tapar
        out 0x0C, al                ; biestable
        pop ax
        mov al, ah
        or al, [dma]
        out 0x0B, al
        mov bx, [bufp]
        call pick_buf.lin
        movzx dx, byte [dma]
        shl dx, 1
        out dx, al
        mov al, ah
        out dx, al
        inc dx
        mov al, cl
        out dx, al
        mov al, ch
        out dx, al
        shr eax, 16
        movzx bx, byte [dma]
        mov dl, [pgport+bx]
        xor dh, dh
        out dx, al
        mov al, [dma]
        out 0x0A, al                ; destapar
        ret
; la cuenta que queda en el canal de 8 bits -> AX
dma_count:
        pushf
        cli
        out 0x0C, al
        movzx dx, byte [dma]
        shl dx, 1
        inc dx
        in al, dx
        mov ah, al
        in al, dx
        xchg al, ah
        popf
        ret
; canal de 16 bits: 4096 palabras, autoinicio
dma16:
        mov al, [hdma]
        out 0xD4, al                ; tapar (bit 2 = 1: 4 + canal - 4)
        out 0xD8, al
        mov al, [hdma]
        and al, 3
        or al, 0x58
        out 0xD6, al
        mov bx, [bufp]
        call pick_buf.lin
        push eax
        shr eax, 1
        movzx dx, byte [hdma]
        and dx, 3
        shl dx, 2
        add dx, 0xC0
        out dx, al
        mov al, ah
        out dx, al
        add dx, 2
        mov al, (4096 - 1) & 0xFF
        out dx, al
        mov al, (4096 - 1) >> 8
        out dx, al
        pop eax
        shr eax, 16
        movzx bx, byte [hdma]
        mov dl, [pgport+bx]
        xor dh, dh
        out dx, al
        mov al, [hdma]
        and al, 3
        out 0xD4, al                ; destapar
        ret
pgport      db 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A

; ---- consola ----
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
pdec:
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop dx
        pop cx
        pop bx
        ret
hex3:
        push ax
        mov al, ah
        call hex1
        pop ax
hex2:
        push ax
        shr al, 4
        call hex1
        pop ax
hex1:
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        call putc
        pop ax
        ret

base        dw 0x220
irq         db 5
dma         db 1
hdma        db 5
is16        db 0
moved       db 0
oldmask     db 0
ver         dw 0
irqs        dw 0
want        dw 0
t0          dw 0
c0          dw 0
bufp        dw 0
old         dd 0
m_cfg       db "SBTEST: tarjeta en ", 0
m_i         db "h, IRQ ", 0
m_d         db ", DMA ", 0
m_h         db " y ", 0
m_nl        db 13, 10, 0
m_rst_ok    db "Reinicio del DSP: correcto", 13, 10, 0
m_rst_ko    db "Reinicio del DSP: no responde", 13, 10, 0
m_ver       db "DSP versión ", 0
m_e0_ok     db "Identificación (E0h): correcta", 13, 10, 0
m_e0_ko     db "Identificación (E0h): MAL", 13, 10, 0
m_mix       db "Mezclador: IRQ y DMA ", 0
m_mix_ok    db ", maestro correcto", 13, 10, 0
m_mix_ko    db ", maestro MAL", 13, 10, 0
m_f2_ok     db "IRQ (F2h): llega", 13, 10, 0
m_f2_ko     db "IRQ (F2h): NO llega", 13, 10, 0
m_single    db "Bloque único (14h): IRQ a los ", 0
m_single_ko db "Bloque único (14h): NO llega la IRQ", 13, 10, 0
m_auto      db "Autoinicio (1Ch): ", 0
m_s16       db "SB16 16 bits estéreo (B6h): ", 0
m_irqs      db " IRQ en ", 0
m_ticks     db " tics", 0
m_ok        db ", correcto", 13, 10, 0
m_bad       db ", MAL", 13, 10, 0
m_cnt_ok    db "La cuenta del DMA avanza: correcto", 13, 10, 0
m_cnt_ko    db "La cuenta del DMA NO avanza", 13, 10, 0
m_opl_ok    db "AdLib (FM): detectado", 13, 10, 0
m_opl_ko    db "AdLib (FM): no está", 13, 10, 0
m_fm        db "Tono FM tocado", 13, 10, 0

buf:        times 16384 + 2 db 0
