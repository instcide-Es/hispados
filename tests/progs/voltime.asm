; VOLTIME.COM - cuanto tarda un cambio de volumen en HSPSND (pruebas:
; tests/run_volumen_tests.sh). Copyright (C) 2026 Hispa-DOS contributors.
; GPL-2.0-or-later.
;   VOLTIME [n] [T]   n llamadas a la función 05h (volumen en %), cada una
;                     con un volumen distinto (20..79 %); por defecto 50.
;                     Con T, en vez de 05h se pulsan las teclas de volumen
;                     (INT 15h AH=4Fh con Ctrl+Alt+flecha, como la BIOS)
; Mide cada llamada con el TSC (calibrado con el reloj de la BIOS) y escribe
;   VOLTIME llamadas=50 total_ms=12 media_us=240 max_us=900 ordenes=200
;           fallos=0 saltadas=0
; (ordenes, fallos y saltadas: los contadores de HD Audio de HSPSND, si los
; tiene: lo que ha mandado al códec, las que no han contestado y las que no
; se han mandado porque ese nodo ya no contestaba). Deja el volumen como
; estaba.
        cpu 586
        bits 16
        org 0x100

start:
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        je .hay
        mov si, m_no
        call puts
        mov ax, 0x4C01
        int 0x21
.hay:
        mov [drv], bx
        mov [drv+2], es
        mov [dseg], es
        push cs
        pop es
        ; argumentos
        mov si, 0x81
.a:
        lodsb
        cmp al, 13
        je .fa
        cmp al, ' '
        je .a
        and al, 0xDF
        cmp al, 'T'
        jne .an
        mov byte [teclas], 1
        jmp .a
.an:
        or al, 0x20
        cmp al, '0'
        jb .a
        cmp al, '9'
        ja .a
        dec si
        xor bx, bx
.d:
        lodsb
        sub al, '0'
        jb .fd
        cmp al, 9
        ja .fd
        movzx ax, al
        imul bx, bx, 10
        add bx, ax
        jmp .d
.fd:
        dec si
        test bx, bx
        jz .a
        mov [n], bx
        jmp .a
.fa:
        ; el volumen de ahora
        mov ah, 6
        call far [drv]
        mov [v0], ax
        call cnt_leer
        mov [c0], eax
        mov [f0], ebx
        mov [s0], edx
        call calibrar
        xor ebp, ebp                ; EBP = total en us
        mov dword [vmax], 0
        xor cx, cx
.l:
        cmp cx, [n]
        jae .fin
        push cx
        mov ax, cx
        mov bl, 60
        div bl
        mov al, ah
        add al, 20
        mov [vv], al
        rdtsc
        mov [t0], eax
        mov [t0+4], edx
        cmp byte [teclas], 0
        jne .tec
        mov al, [vv]
        mov ah, 5
        call far [drv]
        jmp .med
.tec:
        ; Ctrl+Alt pulsadas (en la BIOS) y flecha arriba o abajo
        push ds
        push 0x40
        pop ds
        mov bl, [0x17]
        mov [cs:k17], bl
        or byte [0x17], 0x0C
        pop ds
        mov al, 0x48
        test cx, 1
        jz .t1
        mov al, 0x50
.t1:
        mov ah, 0x4F
        stc
        int 0x15
        push ds
        push 0x40
        pop ds
        mov bl, [cs:k17]
        mov [0x17], bl
        pop ds
.med:
        rdtsc
        sub eax, [t0]
        sbb edx, [t0+4]
        mov ebx, [tus]
        cmp edx, ebx                ; (que el cociente quepa)
        jb .dv
        mov eax, 0xFFFFFFFF
        jmp .dd
.dv:
        div ebx                     ; EAX = us
.dd:
        add ebp, eax
        cmp eax, [vmax]
        jbe .nm
        mov [vmax], eax
.nm:
        pop cx
        inc cx
        jmp .l
.fin:
        ; el volumen como estaba
        mov ax, [v0]
        mov ah, 5
        call far [drv]
        cmp byte [v0+1], 0
        je .nmu
        mov ax, 0x0701
        call far [drv]
.nmu:
        mov si, m_r1
        call puts
        movzx eax, word [n]
        call pnum
        mov si, m_r2
        call puts
        mov eax, ebp
        xor edx, edx
        mov ebx, 1000
        div ebx
        call pnum
        mov si, m_r3
        call puts
        mov eax, ebp
        xor edx, edx
        movzx ebx, word [n]
        test ebx, ebx
        jz .z
        div ebx
.z:
        call pnum
        mov si, m_r4
        call puts
        mov eax, [vmax]
        call pnum
        call cnt_leer
        cmp byte [hay_cnt], 0
        je .sc
        push edx
        push ebx
        sub eax, [c0]
        mov si, m_r5
        push eax
        call puts
        pop eax
        call pnum
        mov si, m_r6
        call puts
        pop eax
        sub eax, [f0]
        call pnum
        mov si, m_r7
        call puts
        pop eax
        sub eax, [s0]
        call pnum
.sc:
        mov si, m_crlf
        call puts
        mov ax, 0x4C00
        int 0x21

; los contadores de HSPSND («HDACNT» y tres dobles palabras) ->
; EAX órdenes, EBX fallos, EDX saltadas (0 si no los tiene)
cnt_leer:
        push es
        push di
        push si
        push cx
        mov es, [dseg]
        xor di, di
.b:
        cmp di, 0xFFF0
        jae .no
        cmp dword [es:di], 'HDAC'
        jne .s
        cmp word [es:di+4], 'NT'
        je .si
.s:
        inc di
        jmp .b
.si:
        mov byte [hay_cnt], 1
        mov eax, [es:di+6]
        mov ebx, [es:di+10]
        mov edx, [es:di+14]
        jmp .x
.no:
        xor eax, eax
        xor ebx, ebx
        xor edx, edx
.x:
        pop cx
        pop si
        pop di
        pop es
        ret

; el TSC por microsegundo (4 vueltas del reloj de la BIOS: 219.700 us)
calibrar:
        push es
        push 0
        pop es
        mov ax, [es:0x46C]
.e:
        cmp ax, [es:0x46C]
        je .e
        rdtsc
        mov [t0], eax
        mov [t0+4], edx
        mov ax, [es:0x46C]
        add ax, 4
.e2:
        cmp ax, [es:0x46C]
        jne .e2
        rdtsc
        sub eax, [t0]
        sbb edx, [t0+4]
        mov ebx, 219700
        div ebx
        test eax, eax
        jnz .k
        inc eax
.k:
        mov [tus], eax
        pop es
        ret

pnum:
        push eax
        push ebx
        push ecx
        push edx
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
        pop edx
        pop ecx
        pop ebx
        pop eax
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

m_no    db "VOLTIME: HSPSND no está cargado", 13, 10, 0
m_r1    db "VOLTIME llamadas=", 0
m_r2    db " total_ms=", 0
m_r3    db " media_us=", 0
m_r4    db " max_us=", 0
m_r5    db " ordenes=", 0
m_r6    db " fallos=", 0
m_r7    db " saltadas=", 0
m_crlf  db 13, 10, 0
n       dw 50
teclas  db 0
hay_cnt db 0
k17     db 0
vv      db 0
drv     dd 0
dseg    dw 0
v0      dw 0
c0      dd 0
f0      dd 0
s0      dd 0
tus     dd 1
vmax    dd 0
t0      dd 0, 0
