; =============================================================================
;  HSPSND.COM - driver de sonido de HispaDOS (residente)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPSND          (en AUTOEXEC.BAT o con INSTALL= en CONFIG.SYS)
;
;  Tarjetas: Intel HD Audio (HDA, PCs actuales) e Intel AC'97 (ICH y
;  compatibles). HD Audio necesita el modo real "irreal" (FS con límite de
;  4 GB) para llegar a sus registros; no funciona bajo EMM386 (modo V86).
;
;  API: INT 2Fh AX=D700h -> AL=FFh si está instalado, ES:BX = punto de entrada
;  (llamada lejana, AH = función):
;    00h información  -> AL = tarjeta (0 ninguna, 3 AC'97, 4 HD Audio),
;                        BX = versión,
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
name_hda    db "Intel HD Audio", 0
hda_base    dd 0
hda_sd      dw 0                    ; registros del primer flujo de salida
hda_cad     db 0
hda_dac     db 0
hda_fmt     dw 0
hda_written dd 0                    ; bytes encolados
hda_played  dd 0
hda_last    dd 0
hda_cycles  dd 0
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
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
        jne .h
        mov di, name_ac97
.h:
        cmp al, 4
        jne .r
        mov di, name_hda
.r:
        clc
        retf

f_play:
        pushad
        push ds
        push es
        cmp byte [cs:dev_type], 4
        je hda_play
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
        cmp byte [cs:dev_type], 4
        jne .a
        call unreal
        call hda_wait
        jmp .r
.a:
        cmp byte [cs:dev_type], 3
        jne .r
        call ac_wait
.r:
        clc
        retf

f_stop:
        cmp byte [cs:dev_type], 4
        jne .a
        call unreal
        call hda_stop
        jmp .r
.a:
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

; =============================================================================
;  Intel HD Audio
; =============================================================================
; FS = segmento plano de 4 GB (modo real "irreal")
unreal:
        push eax
        push bx
        smsw ax
        test al, 1                  ; en modo V86 no se puede
        jnz .r
        cli
        lgdt [cs:gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        sti
.r:
        pop bx
        pop eax
        ret

; registros: DX = desplazamiento
hrd32:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov eax, [fs:esi]
        pop esi
        ret
hwr32:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov [fs:esi], eax
        pop esi
        ret
hrd16:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov ax, [fs:esi]
        pop esi
        ret
hwr16:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov [fs:esi], ax
        pop esi
        ret
hrd8:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov al, [fs:esi]
        pop esi
        ret
hwr8:
        push esi
        movzx esi, dx
        add esi, [cs:hda_base]
        mov [fs:esi], al
        pop esi
        ret

; EAX = orden para el códec (sin la dirección) -> EAX = respuesta; CF si falla
hda_cmd:
        push ecx
        push edx
        push ebx
        movzx ebx, byte [cs:hda_cad]
        shl ebx, 28
        or eax, ebx
        mov ebx, eax
        mov ecx, 0x100000
.w1:
        mov dx, 0x68
        call hrd16
        test al, 1
        jz .free
        loop .w1, ecx
        jmp .fail
.free:
        mov dx, 0x68
        mov ax, 2
        call hwr16
        mov eax, ebx
        mov dx, 0x60
        call hwr32
        mov dx, 0x68
        mov ax, 1
        call hwr16
        mov ecx, 0x100000
.w2:
        mov dx, 0x68
        call hrd16
        test al, 2
        jnz .got
        loop .w2, ecx
        jmp .fail
.got:
        mov dx, 0x64
        call hrd32
        push eax
        mov dx, 0x68
        mov ax, 2
        call hwr16
        pop eax
        pop ebx
        pop edx
        pop ecx
        clc
        ret
.fail:
        pop ebx
        pop edx
        pop ecx
        stc
        ret

; BL = nodo, AX = verbo de 12 bits, CL = dato -> EAX = respuesta
hda_verb:
        push ebx
        push ecx
        movzx ebx, bl
        shl ebx, 20
        movzx eax, ax
        shl eax, 8
        or eax, ebx
        movzx ecx, cl
        or eax, ecx
        call hda_cmd
        pop ecx
        pop ebx
        ret
; BL = nodo, AL = verbo de 4 bits, CX = dato de 16 bits
hda_verb4:
        push ebx
        push ecx
        movzx ebx, bl
        shl ebx, 20
        movzx eax, al
        shl eax, 16
        or eax, ebx
        movzx ecx, cx
        or eax, ecx
        call hda_cmd
        pop ecx
        pop ebx
        ret
; BL = nodo, CL = parámetro -> EAX
hda_param:
        mov ax, 0xF00
        jmp hda_verb

; formato del flujo para la frecuencia DX (16 bits estéreo)
hda_rate_fmt:
        push si
        mov si, rate_tab
.l:
        mov ax, [cs:si]
        test ax, ax
        jz .last
        cmp dx, ax
        jbe .got
        add si, 4
        jmp .l
.last:
        sub si, 4
.got:
        mov ax, [cs:si+2]
        or ax, 0x0011
        mov [cs:hda_fmt], ax
        pop si
        ret
rate_tab    dw 8000, 0x0500, 11025, 0x4300, 16000, 0x0200, 22050, 0x4100
            dw 24000, 0x0100, 32000, 0x0A00, 44100, 0x4000, 48000, 0x0000
            dw 0

; posición reproducida (bytes, sin vueltas perdidas si se consulta a menudo)
hda_update:
        push eax
        push dx
        mov dx, [cs:hda_sd]
        add dx, 4
        call hrd32
        cmp eax, [cs:hda_last]
        jae .s
        add dword [cs:hda_cycles], NBUF * BUFSZ
.s:
        mov [cs:hda_last], eax
        add eax, [cs:hda_cycles]
        mov [cs:hda_played], eax
        pop dx
        pop eax
        ret

hda_play:
        call unreal
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:cur_rate]
        je .loop
        call hda_wait
        call hda_stop
        mov [cs:cur_rate], dx
        call hda_rate_fmt
        mov bl, [cs:hda_dac]
        mov al, 2
        mov cx, [cs:hda_fmt]
        call hda_verb4
.loop:
        cmp word [cs:p_left], 0
        je .done
        ; esperar a que quede un búfer libre en el anillo
        cmp byte [cs:started], 0
        je .fill
        call tmo_start
.ws:
        call hda_update
        mov eax, [cs:hda_written]
        sub eax, [cs:hda_played]
        cmp eax, (NBUF - 1) * BUFSZ
        jbe .fill
        call tmo_check
        jnc .ws
.fill:
        mov eax, [cs:hda_written]
        shr eax, 13
        mov [cs:wr_idx], al
        call fill_buffer
        test bx, bx
        jz .done
        ; rellenar con silencio hasta el final del búfer
        mov cx, FRAMES
        sub cx, bx
        shl cx, 1
        xor ax, ax
        rep stosw
        add dword [cs:hda_written], BUFSZ
        cmp byte [cs:started], 0
        jne .loop
        call hda_start
        jmp .loop
.done:
        pop es
        pop ds
        popad
        clc
        retf

; arrancar el flujo de salida sobre el anillo de NBUF búferes
hda_start:
        pushad
        mov dx, [cs:hda_sd]
        xor al, al
        call hwr8
        mov al, 1                   ; reinicio del flujo
        call hwr8
        mov cx, 0xFFFF
.r1:
        call hrd8
        test al, 1
        loopz .r1
        xor al, al
        call hwr8
        mov cx, 0xFFFF
.r2:
        call hrd8
        test al, 1
        loopnz .r2
        ; lista de descriptores: NBUF entradas de BUFSZ
        mov di, [cs:bdl_off]
        xor si, si
.bd:
        mov bx, si
        shl bx, 13
        add bx, [cs:buf_off]
        call phys
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        mov dword [cs:di+8], BUFSZ
        mov dword [cs:di+12], 0
        add di, 16
        inc si
        cmp si, NBUF
        jb .bd
        mov bx, [cs:bdl_off]
        call phys
        mov dx, [cs:hda_sd]
        add dx, 0x18
        call hwr32
        add dx, 4
        xor eax, eax
        call hwr32
        mov dx, [cs:hda_sd]
        add dx, 0x08
        mov eax, NBUF * BUFSZ
        call hwr32
        mov dx, [cs:hda_sd]
        add dx, 0x0C
        mov ax, NBUF - 1
        call hwr16
        mov dx, [cs:hda_sd]
        add dx, 0x12
        mov ax, [cs:hda_fmt]
        call hwr16
        mov dx, [cs:hda_sd]
        add dx, 2
        mov al, 0x10                ; flujo número 1
        call hwr8
        mov dx, [cs:hda_sd]
        mov al, 2                   ; RUN
        call hwr8
        mov byte [cs:started], 1
        xor eax, eax
        mov [cs:hda_played], eax
        mov [cs:hda_last], eax
        mov [cs:hda_cycles], eax
        popad
        ret

hda_wait:
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        call hda_update
        mov eax, [cs:hda_played]
        cmp eax, [cs:hda_written]
        jae .end
        call tmo_check
        jnc .l
.end:
        call hda_stop
.r:
        ret

hda_stop:
        push ax
        push dx
        mov dx, [cs:hda_sd]
        xor al, al
        call hwr8
        mov byte [cs:started], 0
        mov dword [cs:hda_written], 0
        ; vaciar el anillo para que no suene lo viejo al volver a empezar
        push cx
        push di
        push es
        push cs
        pop es
        mov di, [cs:buf_off]
        mov cx, NBUF * BUFSZ / 2
        xor ax, ax
        rep stosw
        pop es
        pop di
        pop cx
        pop dx
        pop ax
        ret

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
        ; HDA pide 128 bytes de alineación en la dirección física
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, resident_end + 127
        and eax, 0xFFFFFF80
        sub eax, ebx
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
        ; buscar primero HD Audio y después AC'97
        jmp .noac97
.try_ac97:
        ; AC'97: clase 04h, subclase 01h
        mov ax, 0xB103
        mov ecx, 0x040100
        xor si, si
        int 0x1A
        jc .nopci
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
        jc .try_ac97
        mov [pci_bdf], bx
        call hda_init
        jc .hfail
        mov byte [dev_type], 4
        mov si, msg_hda
        call puts
        mov eax, [hda_base]
        shr eax, 16
        call hex4
        mov eax, [hda_base]
        call hex4
        mov si, msg_codec
        call puts
        mov al, [hda_dac]
        movzx ax, al
        call hex4
        mov si, msg_pin
        call puts
        mov al, [pin_nid]
        movzx ax, al
        call hex4
        mov si, msg_crlf2
        call puts
        jmp .install
.hfail:
        mov si, msg_hdafail
        call puts
        jmp .nocard2
.nocard:
        mov ax, 0xB103
        mov ecx, 0x040100
        xor si, si
        int 0x1A
        jnc .try_ac97
.nopci:
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

; poner en marcha el controlador HD Audio y buscar una salida en el códec
hda_init:
        smsw ax
        test al, 1
        jz .rm
        stc                         ; modo V86 (EMM386): sin acceso a los registros
        ret
.rm:
        ; GDT del modo irreal
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        ; BAR0 (memoria)
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        and ecx, 0xFFFFFFF0
        mov [hda_base], ecx
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0006               ; memoria y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        call unreal
        ; reinicio del controlador (GCTL.CRST)
        mov dx, 0x08
        xor eax, eax
        call hwr32
        mov cx, 0xFFFF
.c0:
        call hrd32
        test al, 1
        loopnz .c0
        call delay
        mov eax, 1
        call hwr32
        mov cx, 0xFFFF
.c1:
        call hrd32
        test al, 1
        loopz .c1
        jz .fail
        ; dar tiempo a los códecs y ver cuáles responden
        call delay
        call delay
        mov dx, 0x0E
        call hrd16
        test ax, ax
        jz .fail
        xor cl, cl
.cad:
        test al, 1
        jnz .gotcad
        shr ax, 1
        inc cl
        jmp .cad
.gotcad:
        mov [hda_cad], cl
        ; primer flujo de salida
        mov dx, 0x00
        call hrd16
        shr ax, 8
        and ax, 0x0F
        shl ax, 5
        add ax, 0x80
        mov [hda_sd], ax
        ; grupo de función de audio
        xor bl, bl
        mov cl, 0x04
        call hda_param
        jc .fail
        mov bl, al                  ; (los grupos son pocos: primer nodo)
        shr eax, 16
        mov bl, al
        mov [afg_nid], bl
        mov ax, 0x705
        xor cl, cl
        call hda_verb               ; encendido D0
        mov bl, [afg_nid]
        mov cl, 0x04
        call hda_param
        mov [w_count], al
        shr eax, 16
        mov [w_first], al
        ; recorrer los widgets: primer DAC y primer pin de salida conectado
        mov byte [hda_dac], 0
        mov byte [pin_nid], 0
        mov bl, [w_first]
        movzx cx, byte [w_count]
.wl:
        push cx
        push bx
        mov cl, 0x09
        call hda_param
        shr eax, 20
        and al, 0x0F
        pop bx
        cmp al, 0
        jne .notdac
        cmp byte [hda_dac], 0
        jne .next
        mov [hda_dac], bl
        jmp .next
.notdac:
        cmp al, 4
        jne .next
        cmp byte [pin_nid], 0
        jne .next
        push bx
        mov cl, 0x0C
        call hda_param
        pop bx
        test al, 0x10               ; puede sacar señal
        jz .next
        push bx
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        pop bx
        shr eax, 30
        cmp al, 1                   ; sin conexión física
        je .next
        mov [pin_nid], bl
.next:
        pop cx
        inc bl
        loop .wl
        cmp byte [hda_dac], 0
        je .fail
        ; ruta pin -> DAC (directa o a través de un mezclador/selector)
        cmp byte [pin_nid], 0
        je .dacs
        call hda_route
.dacs:
        ; DAC: encendido, flujo 1 canal 0, formato y amplificador abierto
        mov bl, [hda_dac]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        mov bl, [hda_dac]
        mov ax, 0x706
        mov cl, 0x10
        call hda_verb
        mov dx, 48000
        mov [cur_rate], dx
        call hda_rate_fmt
        mov bl, [hda_dac]
        mov al, 2
        mov cx, [hda_fmt]
        call hda_verb4
        mov bl, [hda_dac]
        call hda_unmute_out
        mov bl, [pin_nid]
        test bl, bl
        jz .ok
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        mov bl, [pin_nid]
        mov ax, 0x707
        mov cl, 0xC0                ; salida activa (y de auriculares)
        call hda_verb
        mov bl, [pin_nid]
        mov ax, 0x70C
        mov cl, 0x02                ; EAPD
        call hda_verb
        mov bl, [pin_nid]
        call hda_unmute_out
.ok:
        clc
        ret
.fail:
        stc
        ret

; BL = nodo: abrir su amplificador de salida (ganancia 0 dB)
hda_unmute_out:
        push bx
        mov cl, 0x12
        call hda_param
        and ax, 0x7F                ; desplazamiento = 0 dB
        mov cx, 0xB000
        or cx, ax
        pop bx
        mov al, 3
        jmp hda_verb4

; buscar el DAC en la lista de conexiones del pin (o un nivel más abajo)
hda_route:
        mov bl, [pin_nid]
        mov ax, 0xF02
        xor cl, cl
        call hda_verb
        mov [conn], eax
        xor si, si
.e:
        cmp si, 4
        jae .r
        mov bl, [conn+si]
        test bl, bl
        jz .r
        push si
        mov cl, 0x09
        call hda_param
        pop si
        shr eax, 20
        and al, 0x0F
        test al, al
        jnz .mix
        ; conexión directa al DAC
        mov [hda_dac], bl
        mov bl, [pin_nid]
        mov ax, 0x701
        mov cx, si
        call hda_verb
        ret
.mix:
        cmp al, 2
        je .level2
        cmp al, 3
        jne .n
.level2:
        ; mezclador o selector: ¿lleva a un DAC?
        mov [mid], bl
        push si
        mov ax, 0xF02
        xor cl, cl
        call hda_verb
        mov [conn2], eax
        pop si
        xor di, di
.e2:
        cmp di, 4
        jae .n
        mov bl, [conn2+di]
        test bl, bl
        jz .n
        push si
        push di
        mov cl, 0x09
        call hda_param
        pop di
        pop si
        shr eax, 20
        and al, 0x0F
        jnz .n2
        mov [hda_dac], bl
        mov bl, [pin_nid]
        mov ax, 0x701
        mov cx, si
        call hda_verb
        mov bl, [mid]
        mov ax, 0x701
        mov cx, di
        call hda_verb
        mov bl, [mid]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        ; entrada del mezclador abierta y su salida también
        mov bl, [mid]
        mov al, 3
        mov cx, di
        shl cx, 8
        or cx, 0x7000
        call hda_verb4
        mov bl, [mid]
        call hda_unmute_out
        ret
.n2:
        inc di
        jmp .e2
.n:
        inc si
        jmp .e
.r:
        ret

afg_nid     db 0
pin_nid     db 0
w_first     db 0
w_count     db 0
mid         db 0
conn        dd 0
conn2       dd 0

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
msg_hda     db "Intel HD Audio at ", 0
msg_codec   db ", DAC node ", 0
msg_pin     db ", output pin ", 0
msg_crlf2   db 13, 10, 0
msg_hdafail db "HD Audio controller found, but it could not be started.", 13, 10, 0
msg_nocard  db "No supported sound card found.", 13, 10, 0
msg_fail    db "The AC'97 codec does not answer.", 13, 10, 0
