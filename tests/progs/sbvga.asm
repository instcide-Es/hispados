; SBVGA.COM - [SB-VGA] dibujar en modo X (320x200 sin encadenar, como Doom)
; desde un programa en modo protegido de 32 bits por DPMI mientras suena la
; Sound Blaster (emulada por HSPSND /SB, o de verdad): IRQ 5 en autoinicio
; con bloques cortos, FM sonando y el reloj (IRQ 0) enganchado en modo
; protegido, como Doom con DOS/4GW. En cada vuelta pinta las 320 columnas
; eligiendo el plano con la máscara del secuenciador (OUT 3C4h/3C5h) y
; después lee los cuatro planos (3CEh/3CFh, registro 4) para ver que ninguno
; se ha mezclado con otro. Por el camino hace lo que las bibliotecas de
; sonido para escribir en la tarjeta: PUSHFD / CLI / OUT (o IN) / POPFD, y
; mira que después las interrupciones vuelven a estar abiertas (con IOPL 0
; HSPDPMI emula CLI y va paso a paso hasta el POPFD: antes no lo veía venir
; tras un OUT atrapado, el programa se quedaba con las interrupciones
; cerradas, el reloj se paraba y Doom se colgaba en el HP Mini 5103).
; tests/run_sbvga_tests.sh mira lo que escribe por la consola (y la serie).
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
        mov edi, dbuf
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
        mov ax, 0x0002              ; y el de la memoria de vídeo
        mov bx, 0xA000
        int 0x31
        jc .no2
        mov [vgasel], ax
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
        mov es, [dssel]
        SAY m_hello
        pushfd
        pop eax
        shr eax, 12
        and al, 3
        add al, '0'
        mov [m_iopl_n], al
        SAY m_iopl
        call dsp_reset
        jnc .rok
        SAY m_rst_ko
        inc byte [nfail]
        jmp fin
.rok:
        call hook
        jc fin
        ; ---- la SB: autoinicio, 8 bits mono, 22222 Hz, bloques de 256 ----
        call pick_buf
        mov ebx, 149 * 16
        mov ecx, 4096
        call fill8
        mov ecx, 4096 - 1
        mov ah, 0x58
        call dma8
        mov al, 0xD1
        call dsp_out
        mov al, 0x40
        call dsp_out
        mov al, 256 - 45
        call dsp_out
        mov al, 0x48
        call dsp_out
        mov al, (256 - 1) & 0xFF
        call dsp_out
        mov al, (256 - 1) >> 8
        call dsp_out
        mov al, 0x1C
        call dsp_out
        ; ---- la FM: un tono que suena todo el rato ----
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

        ; ---- modo X: 13h por la BIOS y sin encadenar ----
        mov ax, 0x0013
        int 0x10
        mov dx, 0x3C4
        mov al, 4
        out dx, al
        inc dx
        in al, dx
        and al, 0xF7                ; sin «chain 4»
        or al, 0x04
        out dx, al
        mov dx, 0x3D4
        mov al, 0x14
        out dx, al
        inc dx
        in al, dx
        and al, 0xBF
        out dx, al
        dec dx
        mov al, 0x17
        out dx, al
        inc dx
        in al, dx
        or al, 0x40
        out dx, al
        mov dword [irqs], 1         ; (la primera, sin manejador lento)
        mov dword [t8], 0
        mov byte [maxdepth], 0
        call ticks
        mov [t0], eax

        ; ---- las vueltas: 2 s de reloj (o 3000 vueltas) ----
.round:
        mov fs, [vgasel]
        mov dx, 0x3C4
        mov al, 2                   ; índice: máscara de planos (una vez)
        out dx, al
        xor ebx, ebx                ; columna
.col:
        mov ecx, ebx
        and cl, 3
        mov al, 1
        shl al, cl
        mov dx, 0x3C5
        out dx, al                  ; el plano de la columna
        call pval                   ; AL = valor de ese plano en esta vuelta
        mov edi, ebx
        shr edi, 2
        mov ecx, ROWS
.row:
        mov [fs:edi], al
        add edi, 80
        loop .row
        ; cada 8 columnas, lo de las bibliotecas de sonido
        test bl, 7
        jnz .nx
        call snd_crit
.nx:
        inc ebx
        cmp ebx, 320
        jb .col
        ; comprobar los cuatro planos
        xor ecx, ecx
.pl:
        mov dx, 0x3CE
        mov al, 4
        out dx, al
        inc dx
        mov al, cl
        out dx, al
        push ecx
        mov ebx, ecx                ; (pval: plano = columna & 3)
        call pval
        xor edi, edi
        mov ecx, ROWS * 80
.rd:
        cmp [fs:edi], al
        je .rdok
        inc dword [mixed]
.rdok:
        inc edi
        loop .rd
        pop ecx
        inc ecx
        cmp ecx, 4
        jb .pl
        inc dword [rounds]
        cmp dword [rounds], 3000
        jae .done
        call ticks
        sub eax, [t0]
        and eax, 0xFFFF
        cmp eax, 36
        jb .round
.done:
        call ticks
        sub eax, [t0]
        and eax, 0xFFFF
        mov [tdraw], eax
        mov eax, [irqs]
        mov [irqdraw], eax
        mov eax, [t8]
        mov [t8draw], eax
        ; ---- fin: texto, la SB callada, todo como estaba ----
        mov al, 0xDA
        call dsp_out
        mov ecx, 3
        call wait_ticks
        call dsp_reset
        mov ax, 0xB013
        call opl
        call unhook
        mov ax, 0x0003
        int 0x10
        SAY m_hello
        SAY m_modex
        SAY m_rounds
        mov eax, [rounds]
        call pdec
        SAY m_mixed
        mov eax, [mixed]
        call pdec
        SAY m_nl
        SAY m_pmix
        mov esi, m_okl
        cmp dword [mixed], 0
        je .m1
        inc byte [nfail]
        mov esi, m_badl
.m1:
        call puts
        ; el reloj ha seguido (36 tics; si se para, se acaba por las vueltas)
        SAY m_clk
        mov eax, [tdraw]
        call pdec
        SAY m_ticks
        mov esi, m_ok
        cmp dword [tdraw], 30
        jae .c1
        inc byte [nfail]
        mov esi, m_bad
.c1:
        call puts
        ; la IRQ 0 en el manejador de modo protegido
        SAY m_t8
        mov eax, [t8draw]
        call pdec
        mov esi, m_ok
        cmp dword [t8draw], 25
        jae .c2
        inc byte [nfail]
        mov esi, m_bad
.c2:
        call puts
        ; la IRQ 5 de la SB
        SAY m_irq
        mov eax, [irqdraw]
        call pdec
        mov esi, m_ok
        cmp dword [irqdraw], 40
        jae .c3
        inc byte [nfail]
        mov esi, m_bad
.c3:
        call puts
        ; PUSHFD / CLI / OUT|IN / POPFD: interrupciones abiertas después
        SAY m_crit
        mov eax, [ncrit]
        call pdec
        SAY m_crit2
        mov eax, [badif]
        call pdec
        mov esi, m_ok
        cmp dword [badif], 0
        jne .c4b
        cmp dword [ncrit], 0
        jne .c4
.c4b:
        inc byte [nfail]
        mov esi, m_bad
.c4:
        call puts
        ; IRQ 5 anidadas con el manejador lento
        ; (lo mira run_sbvga_tests.sh: con la emulada, 1; una SB de verdad
        ; sí la anida, como el 8259)
        SAY m_nest
        movzx eax, byte [maxdepth]
        call pdec
        SAY m_nl
        SAY m_isr
        mov eax, [badisr]
        call pdec
        mov esi, m_ok
        cmp dword [badisr], 0
        je .c5
        inc byte [nfail]
        mov esi, m_bad
.c5:
        call puts
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

ROWS        equ 50

; AL = valor del plano (EBX & 3) en esta vuelta: distinto en cada plano
pval:
        push ecx
        mov ecx, ebx
        and ecx, 3
        imul ecx, ecx, 0x41
        mov eax, [rounds]
        imul eax, eax, 7
        add eax, ecx
        inc eax
        pop ecx
        ret

; lo que hacen las bibliotecas de sonido (DMX, Apogee...) para tocar la
; tarjeta sin que las interrumpan; después, las interrupciones abiertas.
; Conserva EAX, EBX, ECX, EDX.
snd_crit:
        push eax
        push edx
        inc dword [ncrit]
        ; PUSHFD / CLI / OUT (atrapado) / POPFD
        pushfd
        cli
        mov dx, 0x388
        mov al, 0x20
        out dx, al
        popfd
        call chk_if
        ; PUSHFD / CLI / IN (atrapado) / POPFD
        pushfd
        cli
        mov dx, 0x22C
        in al, dx
        popfd
        call chk_if
        ; PUSHFD / CLI / OUT / IN / OUT / POPFD (el mezclador)
        pushfd
        cli
        mov dx, 0x224
        mov al, 0x22
        out dx, al
        inc dx
        in al, dx
        out dx, al
        popfd
        call chk_if
        ; CLI / OUT / STI
        cli
        mov dx, 0x388
        mov al, 0x20
        out dx, al
        sti
        call chk_if
        pop edx
        pop eax
        ret
chk_if:
        pushfd
        test byte [esp+1], 2
        lea esp, [esp+4]
        jnz .r
        inc dword [badif]
        sti                         ; (para seguir: si no, el reloj se para)
.r:
        ret

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

; ---- IRQ 5 y IRQ 0: manejadores de modo protegido ----
hook:
        mov ax, 0x0204
        mov bl, 0x0D
        int 0x31
        mov [oldpm], edx
        mov [oldpm+4], cx
        mov ax, 0x0204
        mov bl, 0x08
        int 0x31
        mov [old8], edx
        mov [old8+4], cx
        mov ax, 0x0205
        mov bl, 0x0D
        mov cx, cs
        mov edx, isr5
        int 0x31
        jc .ko
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, cs
        mov edx, isr8
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
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, [old8+4]
        mov edx, [old8]
        int 0x31
        ret
; la de la SB: como la de un juego (cierra el bloque, EOI, abre las
; interrupciones y toca la FM con PUSHFD / CLI / ... / POPFD)
isr5:
        push eax
        push edx
        push ds
        mov ds, [cs:dssel]
        inc dword [irqs]
        inc byte [depth]
        mov al, [depth]
        cmp al, [maxdepth]
        jbe .d
        mov [maxdepth], al
.d:
        mov dx, 0x22E
        in al, dx
        mov al, 0x20
        out 0x20, al
        sti
        pushfd
        cli
        mov dx, 0x388
        mov al, 0x20
        out dx, al
        popfd
        pushfd                      ; (abiertas otra vez: la pila es la
        test byte [esp+1], 2        ; bloqueada, de la GDT)
        lea esp, [esp+4]
        jnz .ok
        inc dword [badisr]
        sti
.ok:
        ; una de cada 8, un manejador lento (hasta el tic siguiente del
        ; reloj, unos 55 ms: más que un bloque) con las interrupciones
        ; abiertas: la IRQ siguiente no tiene que entrar mientras tanto
        test byte [irqs], 7
        jnz .nos
        push gs
        mov gs, [lowsel]
        mov ax, [gs:0x46C]
        mov edx, 50000000
.sp:
        cmp ax, [gs:0x46C]
        jne .spx
        dec edx
        jnz .sp
.spx:
        pop gs
.nos:
        cli
        dec byte [depth]
        pop ds
        pop edx
        pop eax
        iretd
; la del reloj: cuenta y sigue con la de antes
isr8:
        push ds
        mov ds, [cs:dssel]
        inc dword [t8]
        pop ds
        jmp far [cs:old8]

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

; ---- búfer (dirección lineal = segmento del programa * 16 + desp.) ----
lin:
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

s_nodpmi    db "SBVGA: no hay servidor DPMI de 32 bits", 13, 10, "$"
m_hello     db "SBVGA: en modo protegido de 32 bits (DPMI)", 13, 10, 0
m_iopl      db "SBVGA: IOPL "
m_iopl_n    db "?", 13, 10, 0
m_nl        db 13, 10, 0
m_rst_ko    db "Reinicio del DSP: no responde", 13, 10, 0
m_hook_ko   db "INT 31h 0205h: MAL", 13, 10, 0
m_modex     db "Modo X (320x200 sin encadenar) con la SB y la FM sonando", 13, 10, 0
m_rounds    db "Vueltas: ", 0
m_mixed     db ", octetos de otro plano: ", 0
m_pmix      db "Planos de la VGA", 0
m_okl       db ": sin mezclar, correcto", 13, 10, 0
m_badl      db ": MEZCLADOS, MAL", 13, 10, 0
m_clk       db "Reloj durante el dibujo: ", 0
m_ticks     db " tics", 0
m_t8        db "IRQ 0 en el manejador de modo protegido: ", 0
m_irq       db "IRQ 5 de la SB durante el dibujo: ", 0
m_crit      db "PUSHFD/CLI/OUT/POPFD: ", 0
m_crit2     db " vueltas, interrupciones cerradas después: ", 0
m_nest      db "IRQ 5 con un manejador lento: anidadas como mucho ", 0
m_isr       db "PUSHFD/CLI/OUT/POPFD en el manejador de la IRQ 5, cerradas después: ", 0
m_ok        db ", correcto", 13, 10, 0
m_bad       db ", MAL", 13, 10, 0
m_fin       db "SBVGA terminado, fallos: ", 0

            align 4
entry       dd 0
rmseg       dw 0
dssel       dw 0
sssel       dw 0
cs32sel     dw 0
lowsel      dw 0
vgasel      dw 0
oldmask     db 0
nfail       db 0
depth       db 0
maxdepth    db 0
            align 4
irqs        dd 0
t8          dd 0
t0          dd 0
bufp        dd 0
rounds      dd 0
mixed       dd 0
ncrit       dd 0
badif       dd 0
badisr      dd 0
tdraw       dd 0
irqdraw     dd 0
t8draw      dd 0
oldpm       dd 0
            dw 0
old8        dd 0
            dw 0
dbuf        times 8 db 0
            align 16
            times 0x800 db 0
stack_top:
buf:        times 16384 + 2 db 0
prog_end:
