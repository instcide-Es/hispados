; VELTEST.COM - prueba de VELOCIDAD (tests/run_velocidad_tests.sh): mide la
; velocidad con un bucle propio contra el reloj de la BIOS y escribe
;     VEL=n [TICS=t [ISR=i]] V08=ssss:oooo CD=c
;   VEL   vueltas (de 100) del bucle en 8 tics
;   TICS  tics de la BIOS en 2 segundos del reloj de tiempo real (36 o 37
;         si la INT 08h de siempre sigue a 18,2 Hz)
;   ISR   veces que se ha llamado a la INT 08h de esta prueba en esos 2 s
;   V08   el vector de la INT 08h;  CD = bit CD de CR0 (cachés)
; Argumentos:  T  medir también TICS
;              H  tomar la INT 08h sin reprogramar el temporizador (como un
;                 juego sencillo): ISR tiene que ser 36 o 37
;              R  reprogramar el temporizador a 200 Hz y tomar la INT 08h
;                 pasando el tic a la anterior a 18,2 Hz (como un juego con
;                 música): ISR tiene que ser unos 400
        cpu 386
        bits 16
        org 0x100

DIVR    equ 5966                ; 1193182 / 200 Hz
BASURA  equ 0x8000              ; una palabra de memoria para el bucle

start:
        mov si, 0x81
.a:
        lodsb
        cmp al, 13
        je .ae
        and al, 0xDF
        cmp al, 'T'
        jne .a1
        mov byte [con_tics], 1
.a1:
        cmp al, 'H'
        jne .a2
        mov byte [con_tics], 1
        mov byte [gancho], 1
.a2:
        cmp al, 'R'
        jne .a
        mov byte [con_tics], 1
        mov byte [gancho], 2
        jmp .a
.ae:
        cmp byte [gancho], 0
        je .sing
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        push cs
        pop es
        cli
        mov dx, isr
        mov ax, 0x2508
        int 0x21
        cmp byte [gancho], 2
        jne .g1
        mov al, 0x36
        out 0x43, al
        mov al, DIVR & 0xFF
        out 0x40, al
        mov al, DIVR >> 8
        out 0x40, al
.g1:
        sti
.sing:
        cmp byte [con_tics], 0
        je .vel
        call seg_nuevo
        call tics
        mov [tic0], ax
        mov eax, [isr_n]
        mov [isr0], eax
.vel:
        call medir
        mov [vel], ebx
        cmp byte [con_tics], 0
        je .fin
        call seg_nuevo
        call seg_nuevo
        call tics
        sub ax, [tic0]
        mov [tic0], ax
        mov eax, [isr_n]
        sub eax, [isr0]
        mov [isr0], eax
.fin:
        cmp byte [gancho], 0
        je .sing2
        cli
        cmp byte [gancho], 2
        jne .g2
        mov al, 0x36
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
.g2:
        push ds
        lds dx, [old08]
        mov ax, 0x2508
        int 0x21
        pop ds
        sti
.sing2:
        mov si, s_vel
        call puts
        mov eax, [vel]
        call num
        cmp byte [con_tics], 0
        je .p1
        mov si, s_tics
        call puts
        movzx eax, word [tic0]
        call num
        cmp byte [gancho], 0
        je .p1
        mov si, s_isr
        call puts
        mov eax, [isr0]
        call num
.p1:
        mov si, s_v08
        call puts
        xor ax, ax
        mov es, ax
        mov ax, [es:0x22]
        call puthex4
        mov al, ':'
        call putc
        mov ax, [es:0x20]
        call puthex4
        mov si, s_cd
        call puts
        mov al, '?'
        smsw bx
        test bl, 1
        jnz .cd
        mov eax, cr0
        shr eax, 30
        and al, 1
        add al, '0'
.cd:
        call putc
        call crlf
        mov ax, 0x4C00
        int 0x21

isr:
        inc dword [cs:isr_n]
        cmp byte [cs:gancho], 2
        je .r
        jmp far [cs:old08]
.r:
        add word [cs:acc], DIVR
        jc .pasa
        push ax
        mov al, 0x20
        out 0x20, al
        pop ax
        iret
.pasa:
        jmp far [cs:old08]

; AX = tics de la BIOS (palabra baja)
tics:
        push es
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
        pop es
        ret

; esperar a que el reloj de tiempo real cambie de segundo
seg_nuevo:
        mov ah, 2
        int 0x1A
        mov bl, dh
.l:
        mov ah, 2
        int 0x1A
        cmp dh, bl
        je .l
        ret

; EBX = vueltas de 100 en 8 tics
medir:
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
.w:
        cmp ax, [es:0x6C]
        je .w
        mov dx, [es:0x6C]
        xor ebx, ebx
.o:
        mov cx, 100
.i:
        add [BASURA], cx            ; (lejos del código: otra página)
        dec cx
        jnz .i
        inc ebx
        mov ax, [es:0x6C]
        sub ax, dx
        cmp ax, 8
        jb .o
        push cs
        pop es
        ret

; EAX en decimal, sin separadores
num:
        push eax
        push ebx
        push edx
        push cx
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop cx
        pop edx
        pop ebx
        pop eax
        ret

%include "util.inc"

s_vel       db "VEL=", 0
s_tics      db " TICS=", 0
s_isr       db " ISR=", 0
s_v08       db " V08=", 0
s_cd        db " CD=", 0
con_tics    db 0
gancho      db 0
acc         dw 0
tic0        dw 0
isr_n       dd 0
isr0        dd 0
vel         dd 0
old08       dd 0
u_lbuf      times 84 db 0
u_numbuf    times 16 db 0
