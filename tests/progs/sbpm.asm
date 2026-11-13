; SBPM.COM - [SB-PM] la Sound Blaster (emulada por HSPSND /SB o de verdad)
; desde un programa en modo protegido de 32 bits por DPMI, como un juego con
; DOS/4GW: reinicio y versión del DSP (E1h), E0h, mezclador, IRQ (F2h),
; bloque único (14h, 500 Hz), autoinicio (1Ch, 802 Hz, 9 IRQ), SB16 de 16
; bits estéreo (B6h, 1206 Hz, 25 IRQ) por el DMA 5, AdLib (detección por los
; temporizadores y un tono de 600 Hz) y CLI/STI. La IRQ, con un manejador
; de modo protegido (INT 31h 0205h). Con HSPDPMI + HSPEMM + HSPSND /SB, cada
; IN/OUT a esos puertos falla (IOPL 0) y HSPDPMI la lleva a la emulación.
; tests/run_sbpm_tests.sh graba lo que suena y mide los tonos.
; Termina con ERRORLEVEL 0 si todo va bien (1 si algo falla).
        cpu 586
        bits 16
        org 0x100

%macro SAY 1
        mov esi, %1
        call puts
%endmacro

start:
        mov sp, stack_top
        mov [rmseg], cs
        mov bx, (prog_end - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz .no
        test bl, 1
        jz .no
        mov [entry], di
        mov [entry+2], es
        mov bx, si
        xor ax, ax
        mov es, ax
        test bx, bx
        jz .nopriv
        mov ah, 0x48
        int 0x21
        jc .no
        mov es, ax
.nopriv:
        mov ax, 1                   ; cliente de 32 bits
        call far [entry]
        jnc .pm
.no:
        mov dx, s_nodpmi
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C02
        int 0x21
.pm:
        mov [dssel], ds
        push ds
        pop es
        xor ax, ax
        mov cx, 2
        int 0x31
        jc .no2
        mov [cs32sel], ax
        add ax, 8
        mov [sssel], ax
        mov bx, cs
        mov ax, 0x000B
        mov edi, dbuf               ; (cliente de 32 bits: EDI entero)
        int 0x31
        jc .no2
        or byte [dbuf+6], 0x40      ; código de 32 bits
        mov bx, [cs32sel]
        mov ax, 0x000C
        int 0x31
        jc .no2
        mov bx, ds
        mov ax, 0x000B
        int 0x31
        jc .no2
        or byte [dbuf+6], 0x40      ; pila de 32 bits (misma base)
        mov bx, [sssel]
        mov ax, 0x000C
        int 0x31
        jc .no2
        mov ax, 0x0002              ; selector del segmento 0 (el reloj)
        xor bx, bx
        int 0x31
        jc .no2
        mov [lowsel], ax
        movzx esp, sp
        mov ss, [sssel]
        push word [cs32sel]
        push word main32
        retf
.no2:
        mov ax, 0x4C03
        int 0x21

; ================= 32 bits =================
        bits 32
main32:
        mov gs, [lowsel]
        SAY m_hello
        ; IOPL (0 si HSPDPMI atrapa la E/S) e IF
        pushfd
        pop eax
        shr eax, 12
        and al, 3
        add al, '0'
        mov [m_iopl_n], al
        SAY m_iopl
        SAY m_cfg

        ; ---- reinicio ----
        call dsp_reset
        jnc .rok
        SAY m_rst_ko
        jmp fin
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
        movzx eax, byte [ver+1]
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
        cmp word [ver], 0x0405
        je .vok
        inc byte [nfail]
.vok:
        ; ---- E0h ----
        mov al, 0xE0
        call dsp_out
        mov al, 0xA5
        call dsp_out
        call dsp_in
        mov esi, m_e0_ok
        cmp al, 0x5A
        je .e0
        inc byte [nfail]
        mov esi, m_e0_ko
.e0:
        call puts
        ; ---- mezclador ----
        SAY m_mix
        mov al, 0x80
        call mix_rd
        call hex2
        mov al, ' '
        call putc
        mov al, 0x81
        call mix_rd
        call hex2
        mov dx, 0x224
        mov al, 0x22
        out dx, al
        inc dx
        mov al, 0xEE
        out dx, al
        mov al, 0x22
        call mix_rd
        mov esi, m_mix_ok
        cmp al, 0xEE
        je .mx
        inc byte [nfail]
        mov esi, m_mix_ko
.mx:
        call puts
        mov dx, 0x224
        mov al, 0x22
        out dx, al
        inc dx
        mov al, 0xFF
        out dx, al

        ; ---- IRQ (F2h) con un manejador de modo protegido ----
        call hook
        jc fin
        mov dword [irqs], 0
        mov al, 0xF2
        call dsp_out
        mov ecx, 18
        call wait_irq
        mov esi, m_f2_ok
        jnc .f2
        inc byte [nfail]
        mov esi, m_f2_ko
.f2:
        call puts
        mov al, 0xD1
        call dsp_out

        ; ---- bloque único: 8 bits mono, 10989 Hz, 500 Hz, 6615 muestras ----
        call pick_buf
        mov ebx, 2972
        mov ecx, 6615
        call fill8
        mov ecx, 6615 - 1
        mov ah, 0x48
        call dma8
        mov al, 0x40
        call dsp_out
        mov al, 256 - 91
        call dsp_out
        mov dword [irqs], 0
        call ticks
        mov [t0], eax
        mov al, 0x14
        call dsp_out
        mov al, (6615 - 1) & 0xFF
        call dsp_out
        mov al, (6615 - 1) >> 8
        call dsp_out
        mov ecx, 54
        call wait_irq
        jc .s_ko
        call ticks
        sub eax, [t0]
        push eax
        SAY m_single
        pop eax
        push eax
        call pdec
        SAY m_ticks
        pop eax
        cmp eax, 8
        jb .s_bad
        cmp eax, 14
        ja .s_bad
        SAY m_ok
        jmp .s_end
.s_bad:
        inc byte [nfail]
        SAY m_bad
        jmp .s_end
.s_ko:
        inc byte [nfail]
        SAY m_single_ko
.s_end:
        call pause

        ; ---- autoinicio: 8 bits mono, 22222 Hz, 802 Hz, bloques de 2048 ----
        mov ebx, 149 * 16
        mov ecx, 4096
        call fill8
        mov ecx, 4096 - 1
        mov ah, 0x58
        call dma8
        mov al, 0x40
        call dsp_out
        mov al, 256 - 45
        call dsp_out
        mov al, 0x48
        call dsp_out
        mov al, (2048 - 1) & 0xFF
        call dsp_out
        mov al, (2048 - 1) >> 8
        call dsp_out
        mov dword [irqs], 0
        call ticks
        mov [t0], eax
        mov al, 0x1C
        call dsp_out
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
        cmp dword [irqs], 8
        jae .a_done
        call ticks
        sub eax, [t0]
        cmp eax, 54
        jb .a_w
.a_done:
        mov al, 0xDA
        call dsp_out
        mov ecx, 9
        call wait_irq2
        call ticks
        sub eax, [t0]
        push eax
        SAY m_auto
        mov eax, [irqs]
        call pdec
        SAY m_irqs
        pop eax
        push eax
        call pdec
        SAY m_ticks
        pop eax
        cmp dword [irqs], 9
        jne .a_bad
        cmp eax, 11
        jb .a_bad
        cmp eax, 19
        ja .a_bad
        SAY m_ok
        jmp .a_cnt
.a_bad:
        inc byte [nfail]
        SAY m_bad
.a_cnt:
        mov esi, m_cnt_ok
        cmp byte [moved], 0
        jne .a_c
        inc byte [nfail]
        mov esi, m_cnt_ko
.a_c:
        call puts
        call pause

        ; ---- SB16: 16 bits estéreo con signo, 44100 Hz, 1206 Hz, DMA 5 ----
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
        mov dword [irqs], 0
        call ticks
        mov [t0], eax
        mov al, 0xB6
        call dsp_out
        mov al, 0x30
        call dsp_out
        mov al, (2048 - 1) & 0xFF
        call dsp_out
        mov al, (2048 - 1) >> 8
        call dsp_out
.h_w:
        cmp dword [irqs], 24
        jae .h_done
        call ticks
        sub eax, [t0]
        cmp eax, 54
        jb .h_w
.h_done:
        mov al, 0xD9
        call dsp_out
        mov ecx, 9
        call wait_irq2
        call ticks
        sub eax, [t0]
        push eax
        SAY m_s16
        mov eax, [irqs]
        call pdec
        SAY m_irqs
        pop eax
        push eax
        call pdec
        SAY m_ticks
        pop eax
        mov byte [is16], 0
        cmp dword [irqs], 25
        jne .h_bad
        cmp eax, 8
        jb .h_bad
        cmp eax, 14
        ja .h_bad
        SAY m_ok
        jmp .h_end
.h_bad:
        inc byte [nfail]
        SAY m_bad
.h_end:
        call dsp_reset
        call unhook
        call pause

        ; ---- AdLib ----
        mov ax, 0x0460
        call opl
        mov ax, 0x0480
        call opl
        call opl_st
        mov bl, al
        mov ax, 0x02FF
        call opl
        mov ax, 0x0421
        call opl
        mov ecx, 400
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
        inc byte [nfail]
        SAY m_opl_ko
        jmp .clisti
.oplok:
        SAY m_opl_ok
        ; las escrituras de la FM, cronometradas (cada una son 2 OUT y 42 IN)
        call ticks
        mov [t0], eax
        mov ecx, 1000
.ot:
        mov ax, 0x4300
        call opl
        loop .ot
        call ticks
        sub eax, [t0]
        push eax
        SAY m_oplt
        pop eax
        call pdec
        SAY m_ticks
        SAY m_nl
        ; un tono: canal 0, el portador solo, 600 Hz
        mov ax, 0x0120
        call opl
        mov ax, 0x2001
        call opl
        mov ax, 0x403F
        call opl
        mov ax, 0x60F0
        call opl
        mov ax, 0x80FF
        call opl
        mov ax, 0x2321
        call opl
        mov ax, 0x4300
        call opl
        mov ax, 0x63F0
        call opl
        mov ax, 0x830F
        call opl
        mov ax, 0xC001
        call opl
        mov ax, 0xA017
        call opl
        mov ax, 0xB033
        call opl
        mov ecx, 13
        call wait_ticks
        mov ax, 0xB013
        call opl
        SAY m_fm
        call pause

.clisti:
        ; ---- CLI y STI (con IOPL 0 los emula HSPDPMI) ----
        ; (con IOPL 0, HSPDPMI va paso a paso tras un CLI: bucle corto)
        cli
        call ticks
        mov ebx, eax
        mov ecx, 20000
.cl:
        dec ecx
        jnz .cl
        call ticks
        sti
        mov esi, m_cli_ok
        cmp eax, ebx
        je .c1
        inc byte [nfail]
        mov esi, m_cli_ko
.c1:
        call puts
        mov ecx, 3
        call wait_ticks             ; (si el reloj no anda, no vuelve)
        SAY m_sti_ok
        ; PUSHFD / CLI / POPFD: con IOPL 0 POPFD no abre las interrupciones
        ; por sí solo (lo hace HSPDPMI); el reloj tiene que seguir
        pushfd
        cli
        nop
        popfd
        call ticks
        mov ebx, eax
        mov edx, 200000000          ; (sin reloj, se acaba por aquí)
.pf:
        call ticks
        sub eax, ebx
        and eax, 0xFFFF
        cmp eax, 3
        jae .pfo
        dec edx
        jnz .pf
        inc byte [nfail]
        sti
        SAY m_popf_ko
        jmp .pfe
.pfo:
        SAY m_popf_ok
.pfe:
        ; un puerto de encima de 3FFh (fuera del mapa de E/S): sin fallo
        mov dx, 0x4D0
        in al, dx
        SAY m_hi_ok
fin:
        SAY m_fin
        movzx eax, byte [nfail]
        call pdec
        SAY m_nl
        mov al, [nfail]
        test al, al
        jz .x
        mov al, 1
.x:
        mov ah, 0x4C
        int 0x21

; ---- DSP ----
dsp_out:
        push ecx
        push edx
        push eax
        mov dx, 0x22C
        mov ecx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .ok
        loop .w
.ok:
        pop eax
        out dx, al
        pop edx
        pop ecx
        ret
dsp_in:
        push ecx
        push edx
        mov dx, 0x22E
        mov ecx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .ok
        loop .w
        mov al, 0
        stc
        jmp .r
.ok:
        mov dx, 0x22A
        in al, dx
        clc
.r:
        pop edx
        pop ecx
        ret
dsp_reset:
        mov dx, 0x226
        mov al, 1
        out dx, al
        mov ecx, 100
.d:
        in al, 0x80
        loop .d
        xor al, al
        out dx, al
        mov ecx, 100
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
mix_rd:
        mov dx, 0x224
        out dx, al
        inc dx
        in al, dx
        ret

; ---- FM: AH = registro, AL = dato ----
opl:
        push edx
        push ecx
        mov dx, 0x388
        xchg al, ah
        out dx, al
        mov ecx, 6
.d1:
        in al, dx
        loop .d1
        inc dx
        xchg al, ah
        out dx, al
        dec dx
        mov ecx, 36
.d2:
        in al, dx
        loop .d2
        pop ecx
        pop edx
        ret
opl_st:
        mov dx, 0x388
        in al, dx
        ret

; ---- IRQ 5: manejador de modo protegido ----
hook:
        mov ax, 0x0204
        mov bl, 0x0D
        int 0x31
        mov [oldpm], edx
        mov [oldpm+4], cx
        mov ax, 0x0205
        mov bl, 0x0D
        mov cx, cs
        mov edx, isr32
        int 0x31
        jc .ko
        in al, 0x21
        mov [oldmask], al
        and al, 0xDF
        out 0x21, al
        clc
        ret
.ko:
        inc byte [nfail]
        SAY m_hook_ko
        stc
        ret
unhook:
        mov al, [oldmask]
        out 0x21, al
        mov ax, 0x0205
        mov bl, 0x0D
        mov cx, [oldpm+4]
        mov edx, [oldpm]
        int 0x31
        ret
isr32:
        push eax
        push edx
        push ds
        mov ds, [cs:dssel]
        inc dword [irqs]
        mov dx, 0x22E
        cmp byte [is16], 0
        je .a
        inc dx
.a:
        in al, dx
        mov al, 0x20
        out 0x20, al
        pop ds
        pop edx
        pop eax
        iretd

wait_irq:
        mov dword [want], 1
        jmp wait_n
wait_irq2:
        mov eax, [irqs]
        inc eax
        mov [want], eax
wait_n:
        call ticks
        mov ebx, eax
.l:
        mov eax, [irqs]
        cmp eax, [want]
        jae .ok
        call ticks
        sub eax, ebx
        cmp eax, ecx
        jb .l
        stc
        ret
.ok:
        clc
        ret
ticks:
        movzx eax, word [gs:0x46C]
        ret
wait_ticks:
        call ticks
        mov ebx, eax
.l:
        call ticks
        sub eax, ebx
        and eax, 0xFFFF
        cmp eax, ecx
        jb .l
        ret
pause:
        mov ecx, 8
        jmp wait_ticks

; ---- búferes (dirección lineal = segmento del programa * 16 + desp.) ----
lin:    ; EBX = desplazamiento -> EAX = lineal
        movzx eax, word [rmseg]
        shl eax, 4
        add eax, ebx
        ret
pick_buf:
        mov ebx, buf
        call lin
        mov ecx, eax
        add ecx, 8191
        xor ecx, eax
        test ecx, 0xFFFF0000
        jz .ok
        add ebx, 8192
.ok:
        mov [bufp], ebx
        ret
pick_buf16:
        mov ebx, buf
        call lin
        test al, 1
        jz .ev
        inc ebx
        inc eax
.ev:
        mov ecx, eax
        add ecx, 8191
        xor ecx, eax
        test ecx, 0xFFFE0000
        jz .ok
        add ebx, 8192
.ok:
        mov [bufp], ebx
        ret
fill8:
        mov edi, [bufp]
        xor edx, edx
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
fill16:
        mov edi, [bufp]
        xor edx, edx
        mov ecx, 2048
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
; canal 1: AH = modo (sin el canal), CX = cuenta - 1
dma8:
        mov al, 5
        out 0x0A, al
        out 0x0C, al
        mov al, ah
        or al, 1
        out 0x0B, al
        mov ebx, [bufp]
        call lin
        out 0x02, al
        mov al, ah
        out 0x02, al
        mov al, cl
        out 0x03, al
        mov al, ch
        out 0x03, al
        shr eax, 16
        out 0x83, al
        mov al, 1
        out 0x0A, al
        ret
dma_count:
        pushfd
        cli
        out 0x0C, al
        in al, 0x03
        mov ah, al
        in al, 0x03
        xchg al, ah
        popfd
        sti                         ; (con IOPL 0, POPFD no abre: STI sí)
        ret
; canal 5: 4096 palabras, autoinicio
dma16:
        mov al, 5
        out 0xD4, al
        out 0xD8, al
        mov al, 0x59
        out 0xD6, al
        mov ebx, [bufp]
        call lin
        push eax
        shr eax, 1
        out 0xC4, al
        mov al, ah
        out 0xC4, al
        mov al, (4096 - 1) & 0xFF
        out 0xC6, al
        mov al, (4096 - 1) >> 8
        out 0xC6, al
        pop eax
        shr eax, 16
        out 0x8B, al
        mov al, 1
        out 0xD4, al
        ret

; ---- consola ----
putc:
        push eax
        push edx
        mov dl, al
        mov ah, 0x02
        int 0x21
        pop edx
        pop eax
        ret
puts:
        lodsb
        test al, al
        jz .x
        call putc
        jmp puts
.x:
        ret
pdec:
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor ecx, ecx
.l:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .l
.p:
        pop eax
        add al, '0'
        call putc
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret
hex2:
        push eax
        shr al, 4
        call .n
        pop eax
.n:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

s_nodpmi    db "SBPM: no hay servidor DPMI de 32 bits", 13, 10, "$"
m_hello     db "SBPM: en modo protegido de 32 bits (DPMI)", 13, 10, 0
m_iopl      db "SBPM: IOPL "
m_iopl_n    db "?", 13, 10, 0
m_cfg       db "SBPM: tarjeta en 220h, IRQ 5, DMA 1 y 5", 13, 10, 0
m_nl        db 13, 10, 0
m_rst_ok    db "Reinicio del DSP: correcto", 13, 10, 0
m_rst_ko    db "Reinicio del DSP: no responde", 13, 10, 0
m_ver       db "DSP versión ", 0
m_e0_ok     db "Identificación (E0h): correcta", 13, 10, 0
m_e0_ko     db "Identificación (E0h): MAL", 13, 10, 0
m_mix       db "Mezclador: IRQ y DMA ", 0
m_mix_ok    db ", maestro correcto", 13, 10, 0
m_mix_ko    db ", maestro MAL", 13, 10, 0
m_hook_ko   db "INT 31h 0205h: MAL", 13, 10, 0
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
m_oplt      db "1000 escrituras FM: ", 0
m_fm        db "Tono FM tocado", 13, 10, 0
m_cli_ok    db "CLI: el reloj se para", 13, 10, 0
m_cli_ko    db "CLI: el reloj NO se para", 13, 10, 0
m_sti_ok    db "STI: el reloj sigue", 13, 10, 0
m_popf_ok   db "PUSHFD/CLI/POPFD: el reloj sigue", 13, 10, 0
m_popf_ko   db "PUSHFD/CLI/POPFD: el reloj NO sigue", 13, 10, 0
m_hi_ok     db "Puerto 4D0h: correcto", 13, 10, 0
m_fin       db "SBPM terminado, fallos: ", 0

            align 4
entry       dd 0
rmseg       dw 0
dssel       dw 0
sssel       dw 0
cs32sel     dw 0
lowsel      dw 0
is16        db 0
moved       db 0
oldmask     db 0
nfail       db 0
ver         dw 0
c0          dw 0
            align 4
irqs        dd 0
want        dd 0
t0          dd 0
bufp        dd 0
oldpm       dd 0
            dw 0
dbuf        times 8 db 0
            align 16
            times 0x800 db 0
stack_top:
buf:        times 16384 + 2 db 0
prog_end:
