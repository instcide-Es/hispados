; DPMILIM.COM - HSPDPMI: límites y restos de un cliente muerto.
;   DPMILIM S   0300h con SS:SP = 0 y CX = 0700h -> 8021h (antes pisaba la
;               tabla de callbacks); con CX = 4, bien
;   DPMILIM B   reserva un callback (0303h), deja su dirección de modo real
;               en 0040:00F0 y termina sin liberarlo
;   DPMILIM L   (modo real) llama a esa dirección: el cliente ya no está y
;               tiene que volver sin más
;   DPMILIM I   engancha INT 08h en modo protegido con un manejador que
;               divide por cero: el servidor lo termina en mitad de la IRQ 0
;   DPMILIM T   (modo real) ¿avanza el reloj de la BIOS? (sin el EOI de la
;               IRQ 0 que no acabó, no)
; HSPDPMI limits and dead-client leftovers.
        cpu 386
        bits 16
        org 0x100

start:
        mov sp, pila_fin
        mov al, [0x82]
        and al, 0xDF
        mov [mode], al
        cmp al, 'L'
        je rm_llama
        cmp al, 'T'
        je rm_reloj
        mov bx, (fin - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz .no
        mov [entry], di
        mov [entry+2], es
        xor ax, ax
        mov es, ax
        test si, si
        jz .np
        mov bx, si
        mov ah, 0x48
        int 0x21
        jc .no
        mov es, ax
.np:
        xor ax, ax
        call far [entry]
        jnc pm
.no:
        mov dx, m_nodpmi
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21

; ---- modo real: llamar al callback que quedó ----
rm_llama:
        push ds
        xor ax, ax
        mov ds, ax
        mov eax, [0x4F0]
        pop ds
        mov dx, m_nocb
        test eax, eax
        jz .p
        mov [cbptr], eax
        call far [cbptr]
        mov dx, m_vuelto
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

; ---- modo real: ¿avanza el reloj? ----
rm_reloj:
        sti
        push ds
        xor ax, ax
        mov ds, ax
        mov bx, [0x46C]
        mov ecx, 0x4000000
.l:
        cmp bx, [0x46C]
        jne .vivo
        a32 loop .l
        pop ds
        mov dx, m_muerto
        jmp .p
.vivo:
        pop ds
        mov dx, m_vivo
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

; ================= modo protegido =================
pm:
        push ds
        pop es
        mov al, [mode]
        cmp al, 'B'
        je pm_cb
        cmp al, 'I'
        je pm_irq
        ; ---- S: 0300h ----
        mov word [rg+0x2E], 0       ; SS:SP = 0: la pila del servidor
        mov word [rg+0x30], 0
        mov word [rg+0x20], 0x0202
        mov di, rg
        mov ax, 0x0300
        mov bx, 0x0012              ; INT 12h
        mov cx, 0x0700
        int 0x31
        mov si, m_s_mal
        jnc .s1
        cmp ax, 0x8021
        jne .s1
        mov si, m_s_ok
.s1:
        call puts
        mov word [rg+0x2E], 0
        mov word [rg+0x30], 0
        mov di, rg
        mov ax, 0x0300
        mov bx, 0x0012
        mov cx, 4
        int 0x31
        mov si, m_s4_mal
        jc .s2
        cmp word [rg+0x1C], 0       ; AX de INT 12h: KB de memoria
        je .s2
        mov si, m_s4_ok
.s2:
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- B: callback sin liberar ----
pm_cb:
        push ds
        mov ax, cs
        mov ds, ax
        mov si, cb_proc
        mov di, rg2
        mov ax, 0x0303
        int 0x31
        pop ds
        jc .mal
        push cx
        push dx
        mov ax, 0x0002              ; selector de 0040h
        mov bx, 0x0040
        int 0x31
        pop dx
        pop cx
        jc .mal
        push ds
        mov ds, ax
        mov [0xF0], dx
        mov [0xF2], cx
        pop ds
        mov si, m_cb
        call puts
        mov ax, 0x4C00
        int 0x21
.mal:
        mov si, m_cbmal
        call puts
        mov ax, 0x4C01
        int 0x21
cb_proc:                            ; (no se llega a usar con el cliente vivo)
        iret

; ---- I: muerte en mitad de la IRQ 0 ----
pm_irq:
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, cs
        mov dx, irq_div
        int 0x31
        mov si, m_irq
        call puts
        sti
.e:
        jmp .e
irq_div:
        xor cx, cx
        mov ax, 1
        xor dx, dx
        div cx
        iret

putc:
        push ax
        push dx
        mov dl, al
        mov ah, 0x02
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .x
        call putc
        jmp puts
.x:
        ret

entry   dd 0
cbptr   dd 0
mode    db 0
m_nodpmi db "DPMILIM: sin DPMI", 13, 10, "$"
m_nocb   db "DPMILIM: no hay callback en 0040:00F0", 13, 10, "$"
m_vuelto db "DPMILIM: callback muerto, vuelto", 13, 10, "$"
m_vivo   db "DPMILIM: reloj vivo", 13, 10, "$"
m_muerto db "DPMILIM: reloj MUERTO", 13, 10, "$"
m_s_ok   db "DPMILIM: 0300h CX=0700h -> 8021h", 13, 10, 0
m_s_mal  db "DPMILIM: 0300h CX=0700h aceptado", 13, 10, 0
m_s4_ok  db "DPMILIM: 0300h CX=4 bien", 13, 10, 0
m_s4_mal db "DPMILIM: 0300h CX=4 MAL", 13, 10, 0
m_cb     db "DPMILIM: callback dejado", 13, 10, 0
m_cbmal  db "DPMILIM: 0303h ha fallado", 13, 10, 0
m_irq    db "DPMILIM: INT 08h enganchada", 13, 10, 0
rg       times 0x32 db 0
rg2      times 0x32 db 0
pila     times 1024 db 0
pila_fin:
fin:
