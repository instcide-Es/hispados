; KBSB.COM - [SB-EOI] el teclado mientras suena la Sound Blaster (de verdad
; o la emulada por HSPSND /SB), como en un juego: su propio INT 9 (lee el
; 60h y manda el EOI, sin pasar por la BIOS), la IRQ 5 de la SB en
; autoinicio (bloques cortos: unas 170 IRQ por segundo; el manejador abre
; las interrupciones y tarda un poco, como el que mezcla), la FM sonando en
; el bucle principal y el reloj (INT 8) enganchado. Cuenta las pulsaciones
; y las sueltas de las teclas durante unos 16 s (o hasta soltar Esc) y lo
; dice: si se pierde un código de soltar, la tecla «se queda pulsada».
;   KBSB          con sonido          KBSB N   sin sonido (solo teclado)
; Antes, como Prince of Persia: el bit de ocupado del DSP (22Ch) tiene que
; parpadear (el juego espera a verlo encendido y apagado) y la IRQ de un
; bloque único tiene que llegar a su hora aunque el programa no toque los
; puertos y llame al reloj de antes desde su INT 8 (con las interrupciones
; cerradas).
; tests/run_sbeoi_tests.sh pulsa y suelta teclas por QMP.
        cpu 386
        bits 16
        org 0x100

%macro SAY 1
        mov si, %1
        call puts
%endmacro

start:
        mov al, [0x82]
        and al, 0xDF
        mov [mode], al
        SAY m_hello
        ; INT 9 y INT 8
        mov ax, 0x3509
        int 0x21
        mov [old9], bx
        mov [old9+2], es
        mov ax, 0x3508
        int 0x21
        mov [old8], bx
        mov [old8+2], es
        push cs
        pop es
        mov dx, isr9
        mov ax, 0x2509
        int 0x21
        mov dx, isr8
        mov ax, 0x2508
        int 0x21
        cmp byte [mode], 'N'
        je .loop0
        ; ---- la Sound Blaster ----
        call dsp_reset
        jnc .rok
        SAY m_rst_ko
        jmp fin
.rok:
        mov ax, 0x350D
        int 0x21
        mov [old5], bx
        mov [old5+2], es
        push cs
        pop es
        mov dx, isr5
        mov ax, 0x250D
        int 0x21
        in al, 0x21
        and al, 0xDF
        out 0x21, al
        mov al, 0xD1
        call dsp_out
        ; el bit de ocupado del DSP parpadea
        mov dx, 0x22C
        mov cx, 64
.bz:
        in al, dx
        test al, 0x80
        jnz .bzy
        loop .bz
        SAY m_bz_ko
        jmp .bze
.bzy:
        SAY m_bz_ok
.bze:
        ; búfer: 4096 muestras de un tono, sin cruzar una página de DMA
        mov bx, buf
        call lin
        mov ecx, eax
        add ecx, 4095
        xor ecx, eax
        test ecx, 0xFFFF0000
        jz .bok
        add bx, 4096
.bok:
        mov [bufp], bx
        mov di, bx
        mov cx, 4096
        xor dx, dx
.f:
        mov al, 0xA0
        test dh, 0x80
        jz .s
        mov al, 0x60
.s:
        stosb
        add dx, 149 * 16
        loop .f
        ; DMA 1, autoinicio, 4096 muestras
        mov al, 5
        out 0x0A, al
        out 0x0C, al
        mov al, 0x59
        out 0x0B, al
        mov bx, [bufp]
        call lin
        out 0x02, al
        mov al, ah
        out 0x02, al
        mov al, 0xFF
        out 0x03, al
        mov al, 0x0F
        out 0x03, al
        shr eax, 16
        out 0x83, al
        mov al, 1
        out 0x0A, al
        mov al, 0x40
        call dsp_out
        mov al, 256 - 45            ; 22222 Hz
        call dsp_out
        ; un bloque único de 2222 muestras (0,1 s) sin tocar los puertos:
        ; ¿cuándo llega su IRQ?
        mov word [irqs], 0
        mov ax, [ticks]
        mov [t0], ax
        mov al, 0x14
        call dsp_out
        mov al, (2222 - 1) & 0xFF
        call dsp_out
        mov al, (2222 - 1) >> 8
        call dsp_out
        sti
.bw:
        cmp word [irqs], 0
        jne .bwd
        mov ax, [ticks]
        sub ax, [t0]
        cmp ax, 40
        jb .bw
.bwd:
        mov ax, [ticks]
        sub ax, [t0]
        push ax
        SAY m_blk
        pop ax
        call pdec
        SAY m_tics
        ; y ahora el autoinicio (el DMA sigue en autoinicio desde el principio)
        mov al, 0x48
        call dsp_out
        mov al, 127                 ; bloques de 128: 174 IRQ por segundo
        call dsp_out
        mov al, 0
        call dsp_out
        mov al, 0x1C
        call dsp_out
        ; FM: un tono en el canal 0
        mov ax, 0x2001
        call opl
        mov ax, 0x403F
        call opl
        mov ax, 0x2321
        call opl
        mov ax, 0x4310
        call opl
        mov ax, 0x63F0
        call opl
        mov ax, 0x830F
        call opl
        mov ax, 0xA017
        call opl
        mov ax, 0xB033
        call opl
.loop0:
        SAY m_ready
        sti
        mov ax, [ticks]
        mov [t0], ax
.loop:
        cmp byte [mode], 'N'
        je .nofm
        ; la «música»: escrituras de la FM sin parar
        mov ax, 0x4310
        test byte [ticks], 1
        jz .v
        mov al, 0x14
.v:
        call opl
.nofm:
        cmp byte [esc_up], 0
        jne .end
        mov ax, [ticks]
        sub ax, [t0]
        cmp ax, 18 * 16
        jb .loop
.end:
        cmp byte [mode], 'N'
        je .q
        mov al, 0xDA
        call dsp_out
        mov ax, 0xB013
        call opl
        mov cx, 4
        call wait_ticks
        call dsp_reset
        in al, 0x21
        or al, 0x20
        out 0x21, al
        push ds
        lds dx, [old5]
        mov ax, 0x250D
        int 0x21
        pop ds
.q:
        cli
        push ds
        lds dx, [old9]
        mov ax, 0x2509
        int 0x21
        pop ds
        push ds
        lds dx, [old8]
        mov ax, 0x2508
        int 0x21
        pop ds
        sti
        SAY m_mk
        mov ax, [makes]
        call pdec
        SAY m_br
        mov ax, [breaks]
        call pdec
        SAY m_ex
        mov ax, [ext]
        call pdec
        SAY m_ir
        mov ax, [irqs]
        call pdec
        SAY m_tk
        mov ax, [ticks]
        sub ax, [t0]
        call pdec
        SAY m_nl
        ; ¿alguna tecla se ha quedado pulsada?
        mov si, down
        mov cx, 128
        xor bx, bx
.d:
        lodsb
        add bl, al
        loop .d
        SAY m_held
        mov ax, bx
        call pdec
        SAY m_nl
fin:
        mov ax, 0x4C00
        int 0x21

; ---- teclado: como un juego, sin la BIOS ----
isr9:
        push ax
        push bx
        in al, 0x60
        cmp al, 0xE0
        jne .n
        inc word [cs:ext]
        jmp .eoi
.n:
        movzx bx, al
        and bl, 0x7F
        test al, 0x80
        jnz .up
        inc word [cs:makes]
        mov byte [cs:down+bx], 1
        jmp .eoi
.up:
        inc word [cs:breaks]
        mov byte [cs:down+bx], 0
        cmp bl, 1
        jne .eoi
        mov byte [cs:esc_up], 1
.eoi:
        mov al, 0x20
        out 0x20, al
        pop bx
        pop ax
        iret

; ---- reloj: cuenta y sigue en el de antes ----
isr8:
        inc word [cs:ticks]
        pushf                       ; como Prince of Persia: el de antes,
        call far [cs:old8]          ; desde aquí (sin interrupciones)
        iret

; ---- IRQ 5: abre las interrupciones y «mezcla» un rato antes del EOI ----
isr5:
        push ax
        push cx
        push dx
        inc word [cs:irqs]
        mov dx, 0x22E
        in al, dx
        sti
        mov cx, 3000
.w:
        loop .w
        cli
        mov al, 0x20
        out 0x20, al
        pop dx
        pop cx
        pop ax
        iret

; ---- DSP y FM ----
dsp_out:
        push cx
        push dx
        push ax
        mov dx, 0x22C
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
dsp_reset:
        mov dx, 0x226
        mov al, 1
        out dx, al
        mov cx, 100
.d:
        in al, 0x80
        loop .d
        xor al, al
        out dx, al
        mov cx, 1000
.t:
        mov dx, 0x22E
        in al, dx
        test al, 0x80
        jz .n
        mov dx, 0x22A
        in al, dx
        cmp al, 0xAA
        je .ok
.n:
        loop .t
        stc
        ret
.ok:
        clc
        ret
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
wait_ticks:
        mov bx, [ticks]
.l:
        mov ax, [ticks]
        sub ax, bx
        cmp ax, cx
        jb .l
        ret
lin:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ebx, bx
        add eax, ebx
        ret

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

m_hello     db "KBSB: teclado e IRQ 5 a la vez", 13, 10, 0
m_rst_ko    db "KBSB: el DSP no responde", 13, 10, 0
m_ready     db "KBSB: listo", 13, 10, 0
m_mk        db "KBSB: pulsadas ", 0
m_br        db ", soltadas ", 0
m_ex        db ", E0 ", 0
m_ir        db ", IRQ 5 ", 0
m_tk        db ", tics ", 0
m_held      db "KBSB: teclas que quedan pulsadas: ", 0
m_nl        db 13, 10, 0
m_bz_ok     db "KBSB: el bit de ocupado del DSP parpadea", 13, 10, 0
m_bz_ko     db "KBSB: el bit de ocupado del DSP NO parpadea", 13, 10, 0
m_blk       db "KBSB: IRQ del bloque único a los ", 0
m_tics      db " tics", 13, 10, 0
mode        db 0
esc_up      db 0
            align 2
makes       dw 0
breaks      dw 0
ext         dw 0
irqs        dw 0
ticks       dw 0
t0          dw 0
bufp        dw 0
old9        dd 0
old8        dd 0
old5        dd 0
down        times 128 db 0
buf:        times 8192 db 0
