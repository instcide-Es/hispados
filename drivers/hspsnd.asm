; =============================================================================
;  HSPSND.COM - driver de sonido de HispaDOS (residente)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPSND          (en AUTOEXEC.BAT o con INSTALL= en CONFIG.SYS)
;
;  Tarjetas: Intel AC'97 (ICH y compatibles, PCI). Próximamente Sound
;  Blaster 16 y HD Audio.
;
;  API: INT 2Fh AX=D700h -> AL=FFh si está instalado, ES:BX = punto de entrada
;  (llamada lejana, AH = función):
;    00h información  -> AL = tarjeta (0 ninguna, 3 AC'97), BX = versión,
;                        ES:DI = nombre
;    01h reproducir   DS:SI = datos PCM, CX = bytes, DX = Hz,
;                     AL = formato (bit 0: 16 bits, bit 1: estéreo).
;                     Vuelve cuando los datos están en cola; el sonido sigue.
;    02h esperar a que termine
;    03h parar
;    04h volumen      AL = 0 (silencio) .. 31 (máximo)
; =============================================================================
        cpu 386
        bits 16
        org 0x100

NBUF        equ 4                   ; búferes de DMA
BUFSZ       equ 8192                ; bytes por búfer (16 bits estéreo)
FRAMES      equ BUFSZ / 4

start:
        jmp init

; ---- datos residentes ----
old2f       dd 0
dev_type    db 0
started     db 0
wr_idx      db 0
vra         db 0
nam         dw 0
nabm        dw 0
cur_rate    dw 0
p_fmt       db 0
            db 0
p_left      dw 0
p_src       dd 0
name_ac97   db "Intel AC'97", 0
name_none   db "none", 0
bdl_off     dw 0
buf_off     dw 0

int2f:
        cmp ax, 0xD700
        jne .chain
        mov al, 0xFF
        push cs
        pop es
        mov bx, entry
        iret
.chain:
        jmp far [cs:old2f]

entry:
        cmp ah, 0
        je f_info
        cmp ah, 1
        je f_play
        cmp ah, 2
        je f_wait
        cmp ah, 3
        je f_stop
        cmp ah, 4
        je f_volume
        stc
        retf

f_info:
        mov al, [cs:dev_type]
        mov bx, 0x0001
        push cs
        pop es
        mov di, name_none
        cmp al, 3
        jne .r
        mov di, name_ac97
.r:
        clc
        retf

f_play:
        pushad
        push ds
        push es
        cmp byte [cs:dev_type], 3
        jne .done
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:cur_rate]
        je .rate_ok
        ; cambiar de frecuencia: esperar a que acabe lo que suena
        call ac_wait
        call ac_stop
        mov [cs:cur_rate], dx
        call ac_set_rate
.rate_ok:
        cmp byte [cs:started], 0
        jne .loop
        mov byte [cs:wr_idx], 0     ; al arrancar, CIV empieza en 0
.loop:
        cmp word [cs:p_left], 0
        je .done
        call ac_wait_slot
        call fill_buffer
        test bx, bx
        jz .done
        call ac_queue
        jmp .loop
.done:
        pop es
        pop ds
        popad
        clc
        retf

f_wait:
        cmp byte [cs:dev_type], 3
        jne .r
        call ac_wait
.r:
        clc
        retf

f_stop:
        cmp byte [cs:dev_type], 3
        jne .r
        call ac_stop
.r:
        clc
        retf

f_volume:
        cmp byte [cs:dev_type], 3
        jne .r
        push ax
        push dx
        and al, 31
        jz .mute
        mov ah, 31
        sub ah, al
        mov al, ah                  ; misma atenuación en los dos canales
        jmp .set
.mute:
        mov ax, 0x8000
.set:
        mov dx, [cs:nam]
        add dx, 0x02
        out dx, ax
        pop dx
        pop ax
.r:
        clc
        retf

; ---- conversión: p_src -> búfer de DMA wr_idx&3 (16 bits estéreo) ----
; -> BX = tramas escritas
fill_buffer:
        ; bytes por trama de entrada
        mov cl, 1
        test byte [cs:p_fmt], 1
        jz .b
        shl cl, 1
.b:
        test byte [cs:p_fmt], 2
        jz .c
        shl cl, 1
.c:
        movzx cx, cl
        mov ax, [cs:p_left]
        xor dx, dx
        div cx                      ; AX = tramas disponibles
        cmp ax, FRAMES
        jbe .n
        mov ax, FRAMES
.n:
        mov bx, ax
        test ax, ax
        jnz .go
        mov word [cs:p_left], 0
        ret
.go:
        push bx
        mul cx
        sub [cs:p_left], ax         ; bytes consumidos
        ; destino
        movzx di, byte [cs:wr_idx]
        and di, NBUF - 1
        shl di, 13                  ; * BUFSZ
        add di, [cs:buf_off]
        push cs
        pop es
        lds si, [cs:p_src]
        mov cx, bx
        mov dl, [cs:p_fmt]
        cld
.l:
        test dl, 1
        jnz .s16
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        mov bx, ax
        test dl, 2
        jz .mono
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        jmp .st
.s16:
        lodsw
        mov bx, ax
        test dl, 2
        jz .mono
        lodsw
        jmp .st
.mono:
        mov ax, bx
.st:
        xchg ax, bx
        stosw                       ; izquierdo
        mov ax, bx
        stosw                       ; derecho
        loop .l
        ; normalizar el puntero de origen
        mov ax, ds
        mov bx, si
        shr bx, 4
        add ax, bx
        and si, 15
        mov [cs:p_src], si
        mov [cs:p_src+2], ax
        pop bx
        ret

; ---- AC'97 ----
; EAX <- dirección física de CS:BX
phys:
        push ebx
        movzx ebx, bx
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, ebx
        pop ebx
        ret

; poner en cola el búfer wr_idx con BX tramas
ac_queue:
        push ax
        push bx
        push dx
        push di

        movzx di, byte [cs:wr_idx]
        shl di, 3
        add di, [cs:bdl_off]
        push bx
        movzx bx, byte [cs:wr_idx]
        and bx, NBUF - 1
        shl bx, 13
        add bx, [cs:buf_off]
        call phys
        pop bx
        mov [cs:di], eax
        shl bx, 1                   ; muestras de 16 bits (2 por trama)
        mov [cs:di+4], bx
        mov word [cs:di+6], 0
        cmp byte [cs:started], 0
        jne .lvi
        ; reiniciar el canal de salida y arrancar
        mov dx, [cs:nabm]
        add dx, 0x1B
        mov al, 2
        out dx, al
        mov cx, 0xFFFF
.rw:
        in al, dx
        test al, 2
        loopnz .rw
        mov bx, [cs:bdl_off]
        call phys
        mov dx, [cs:nabm]
        add dx, 0x10
        out dx, eax
        mov dx, [cs:nabm]
        add dx, 0x15
        mov al, [cs:wr_idx]
        out dx, al
        mov dx, [cs:nabm]
        add dx, 0x1B
        mov al, 1
        out dx, al
        mov byte [cs:started], 1
        jmp .next
.lvi:
        mov dx, [cs:nabm]
        add dx, 0x15
        mov al, [cs:wr_idx]
        out dx, al
.next:
        inc byte [cs:wr_idx]
        and byte [cs:wr_idx], 31
        pop di
        pop dx
        pop bx
        pop ax
        ret

; esperar a que haya un búfer libre
ac_wait_slot:
        push ax
        push dx
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        mov dx, [cs:nabm]
        add dx, 0x16
        in ax, dx
        test al, 1                  ; DMA parado: todo libre
        jnz .r
        mov dx, [cs:nabm]
        add dx, 0x14
        in al, dx
        mov ah, [cs:wr_idx]
        sub ah, al
        and ah, 31
        cmp ah, NBUF
        jb .r
        call tmo_check
        jnc .l
.r:
        pop dx
        pop ax
        ret

; esperar a que termine la reproducción
ac_wait:
        push ax
        push dx
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        mov dx, [cs:nabm]
        add dx, 0x16
        in ax, dx
        test al, 1
        jnz .r
        call tmo_check
        jnc .l
.r:
        pop dx
        pop ax
        ret

ac_stop:
        push ax
        push dx
        mov dx, [cs:nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
        mov al, 2
        out dx, al
        mov byte [cs:started], 0
        mov byte [cs:wr_idx], 0
        pop dx
        pop ax
        ret

ac_set_rate:
        push ax
        push dx
        cmp byte [cs:vra], 0
        je .r
        mov ax, [cs:cur_rate]
        mov dx, [cs:nam]
        add dx, 0x2C
        out dx, ax
.r:
        pop dx
        pop ax
        ret

; plazo máximo de unos 20 segundos (ticks de la BIOS)
tmo_start:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        mov [cs:tmo_t0], ax
        pop ax
        pop ds
        ret
tmo_check:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        sub ax, [cs:tmo_t0]
        cmp ax, 364
        pop ax
        pop ds
        cmc
        ret                         ; CF=1 si se agotó el plazo
tmo_t0      dw 0

resident_end:

; =============================================================================
;  Instalación (lo que sigue se descarta)
; =============================================================================
init:
        mov si, msg_title
        call puts
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne .notinst
        mov si, msg_already
        call puts
        mov ax, 0x4C01
        int 0x21
.notinst:
        ; búferes de DMA y lista de descriptores tras el código residente
        mov ax, resident_end + 15
        and ax, 0xFFF0
        mov [bdl_off], ax
        add ax, 32 * 8
        mov [buf_off], ax
        add ax, NBUF * BUFSZ
        mov [tsr_end], ax
        ; ¿hay BIOS PCI?
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .nopci
        cmp edx, 0x20494350
        jne .nopci
        ; AC'97: clase 04h, subclase 01h
        mov ax, 0xB103
        mov ecx, 0x040100
        xor si, si
        int 0x1A
        jc .noac97
        mov [pci_bdf], bx
        call ac97_init
        jc .fail
        mov byte [dev_type], 3
        mov si, msg_ac97
        call puts
        mov ax, [nam]
        call hex4
        mov al, '/'
        call putc
        mov ax, [nabm]
        call hex4
        mov si, msg_vra
        cmp byte [vra], 0
        jne .v
        mov si, msg_novra
.v:
        call puts
        jmp .install
.noac97:
        mov ax, 0xB103
        mov ecx, 0x040300
        xor si, si
        int 0x1A
        jc .nocard
        mov si, msg_hda
        call puts
        jmp .nocard2
.nopci:
.nocard:
        mov si, msg_nocard
        call puts
.nocard2:
        mov ax, 0x4C02
        int 0x21
.fail:
        mov si, msg_fail
        call puts
        mov ax, 0x4C03
        int 0x21
.install:
        ; enganchar INT 2Fh y quedar residente
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        ; liberar el entorno
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, [tsr_end]
        add dx, 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21

; poner en marcha el controlador AC'97 y el códec
ac97_init:
        ; BAR0 = mezclador (NAM), BAR1 = bus master (NABM); los dos de E/S
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        and cx, 0xFFFE
        mov [nam], cx
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x14
        int 0x1A
        and cx, 0xFFFE
        mov [nabm], cx
        ; activar E/S y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0005
        mov bx, [pci_bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        ; reinicio en frío
        mov dx, [nabm]
        add dx, 0x2C
        xor eax, eax
        out dx, eax
        call delay
        mov eax, 2
        out dx, eax
        call delay
        ; esperar al códec
        mov cx, 0xFFFF
        mov dx, [nabm]
        add dx, 0x30
.w:
        in eax, dx
        test ax, 0x0100
        jnz .ready
        loop .w
        stc
        ret
.ready:
        ; reiniciar el códec y abrir volúmenes
        mov dx, [nam]
        xor ax, ax
        out dx, ax
        call delay
        mov dx, [nam]
        add dx, 0x02                ; maestro: 0 dB
        xor ax, ax
        out dx, ax
        mov dx, [nam]
        add dx, 0x04                ; auriculares
        out dx, ax
        mov dx, [nam]
        add dx, 0x18                ; PCM
        mov ax, 0x0808
        out dx, ax
        ; frecuencia variable
        mov dx, [nam]
        add dx, 0x28
        in ax, dx
        test al, 1
        jz .novra
        mov dx, [nam]
        add dx, 0x2A
        in ax, dx
        or al, 1
        out dx, ax
        mov byte [vra], 1
.novra:
        mov word [cur_rate], 48000
        clc
        ret

delay:
        push cx
        mov cx, 0x4000
.l:
        in al, 0x80
        loop .l
        pop cx
        ret

hex4:
        push cx
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .h
        pop cx
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
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret

pci_bdf     dw 0
tsr_end     dw 0
msg_title   db "HSPSND 0.1 - HispaDOS sound driver", 13, 10, 0
msg_already db "Already installed.", 13, 10, 0
msg_ac97    db "Intel AC'97 audio at I/O ", 0
msg_vra     db ", variable rate", 13, 10, 0
msg_novra   db ", fixed 48 kHz", 13, 10, 0
msg_hda     db "HD Audio controller found: not supported yet.", 13, 10, 0
msg_nocard  db "No supported sound card found.", 13, 10, 0
msg_fail    db "The AC'97 codec does not answer.", 13, 10, 0
