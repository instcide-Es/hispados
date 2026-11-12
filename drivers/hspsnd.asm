; =============================================================================
;  HSPSND.COM - driver de sonido de Hispa-DOS (residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPSND [/NOSB]  (en AUTOEXEC.BAT o con INSTALL= en CONFIG.SYS)
;        /NOSB  no buscar una Sound Blaster
;
;  Tarjetas: Sound Blaster 2.0/Pro/16 (ISA; variable BLASTER o sondeo en
;  220h-280h; si no está BLASTER, la pone), Intel HD Audio (HDA, PCs
;  actuales) e Intel AC'97 (ICH y
;  compatibles). HD Audio necesita el modo real "irreal" (FS con límite de
;  4 GB) para llegar a sus registros; no funciona bajo EMM386 (modo V86).
;
;  API: INT 2Fh AX=D700h -> AL=FFh si está instalado, ES:BX = punto de entrada
;  (llamada lejana, AH = función):
;    00h información  -> AL = tarjeta (0 ninguna, 1 Sound Blaster,
;                        2 Sound Blaster 16, 3 AC'97, 4 HD Audio),
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
name_none   db "ninguna", 0
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
        cmp byte [cs:dev_type], 4
        jne .go
        cmp ah, 1
        jb .go
        cmp ah, 3
        ja .go
        ; HD Audio: las funciones usan FS (modo irreal) y hay que devolverlo
        ; como estaba; en modo V86 (EMM386 o Windows cargados después) no
        ; se puede llegar a los registros
        push ax
        smsw ax
        test al, 1
        pop ax
        jnz .v86
        push fs
        call a20_open
        push cs
        call .go
        call a20_restore
        pop fs
        retf
.v86:
        stc
        retf
.go:
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
        cmp al, 1
        jne .s16
        mov di, name_sb
.s16:
        cmp al, 2
        jne .s3
        mov di, name_sb16
.s3:
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
        cmp byte [cs:dev_type], 2
        je sb_play_f
        cmp byte [cs:dev_type], 1
        je sb_play_f
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
        cmp byte [cs:dev_type], 2
        ja .nsb
        cmp byte [cs:dev_type], 0
        je .r
        call sb_wait
        jmp .r
.nsb:
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
        cmp byte [cs:dev_type], 2
        ja .nsb
        cmp byte [cs:dev_type], 0
        je .r
        call sb_halt
        jmp .r
.nsb:
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
        cmp byte [cs:dev_type], 2
        ja .nsb
        cmp byte [cs:dev_type], 0
        je .r
        call sb_volume
        jmp .r
.nsb:
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


; ---- línea A20 ----
; Los registros del controlador están por encima de 1 MB, casi siempre en una
; dirección con el bit 20 a uno: con A20 cerrada (la deja así cualquier
; programa que la abra y la cierre por el XMS, y hay BIOS que arrancan con
; ella cerrada) se leería y se escribiría en otra dirección. Se abre mientras
; el driver trabaja y se deja como estaba. Conservan todos los registros y
; los indicadores.
a20_open:
        pushf
        push ax
        cli
        inc byte [cs:a20_depth]
        cmp byte [cs:a20_depth], 1
        jne .r
        call a20_test
        jnz .r
        mov byte [cs:a20_closed], 1
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
        call a20_test
        jnz .r
        mov ax, 0x2401
        int 0x15
.r:
        pop ax
        popf
        ret
a20_restore:
        pushf
        push ax
        cli
        dec byte [cs:a20_depth]
        jnz .r
        cmp byte [cs:a20_closed], 0
        je .r
        mov byte [cs:a20_closed], 0
        in al, 0x92
        and al, 0xFC
        out 0x92, al
        call a20_test
        jz .r
        mov ax, 0x2400
        int 0x15
.r:
        pop ax
        popf
        ret
; ZF = 1 si A20 está cerrada (0000:0500 y FFFF:0510 son el mismo octeto)
a20_test:
        push ds
        push es
        push ax
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov al, [0x500]
        mov ah, al
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        mov [0x500], ah
        jne .open
        cmp ah, [es:0x510]
        jne .open
        xor al, al                  ; ZF = 1
        jmp .x
.open:
        or al, 1                    ; ZF = 0
.x:
        pop ax
        pop es
        pop ds
        ret
a20_depth   db 0
a20_closed  db 0

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
        ; ticks de una vuelta: NBUF*BUFSZ/4 tramas / Hz * 18,2
        push dx
        mov dx, 2
        mov ax, 0x466D              ; 149101
        div word [cs:si]
        mov [cs:hda_lapt], ax
        pop dx
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
        ; si ha pasado una vuelta entera del anillo sin consultar, la cuenta
        ; de vueltas ya no vale: se da todo por reproducido
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        mov dx, ax
        sub dx, [cs:hda_tick]
        mov [cs:hda_tick], ax
        cmp dx, [cs:hda_lapt]
        jb .pos
        mov eax, [cs:hda_written]
        mov [cs:hda_played], eax
        jmp .r
.pos:
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
.r:
        pop dx
        pop eax
        ret
hda_tick    dw 0
hda_lapt    dw 3                    ; ticks de una vuelta del anillo

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
        jbe .under
        cmp eax, (NBUF - 1) * BUFSZ
        jbe .fill
        call tmo_check
        jnc .ws
        jmp .fill
.under:
        ; el flujo ya ha pasado por todo lo encolado (el programa ha tardado
        ; en traer más: un disquete, un CD): se para y se empieza de nuevo.
        ; Antes la resta salía «negativa» y se esperaba el plazo entero (20 s)
        call hda_stop
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
        push ds
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        mov [cs:hda_tick], ax
        popad
        ret

hda_wait:
        cmp byte [cs:started], 0
        je .r
        push eax
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
        pop eax
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

; =============================================================================
;  Sound Blaster (2.0, Pro y 16) - parte residente
; =============================================================================
; 4 bloques de SB_BLK octetos en una zona alineada a 16 KB (la DMA del ISA no
; puede cruzar 64 KB). DMA de autoinicio sobre los 4 bloques; la tarjeta
; avisa (IRQ) al acabar cada bloque: se llena de silencio y queda libre.
; Sin datos durante SB_IDLE bloques se para (reset del DSP), así la tarjeta
; queda libre para los juegos.
SB_BLK      equ 4096
SB_IDLE     equ 8

sb_base     dw 0
sb_irq      db 0
sb_dma      db 1
sb_hdma     db 0                    ; 0 = sin DMA de 16 bits
sb_minor    db 0                    ; (seguidos: se comparan como palabra)
sb_major    db 0
sb_vec      db 0
sb_play     db 0                    ; bloque que suena
sb_wr       db 0                    ; siguiente bloque que se llena
sb_queued   db 0                    ; bloques con datos sin acabar
sb_idle     db 0
sb_full     db 0, 0, 0, 0
sb_hs       db 0                    ; modo de alta velocidad (8 bits)
sb_old      dd 0
sb_req      dw 0
name_sb     db "Sound Blaster", 0
name_sb16   db "Sound Blaster 16", 0

; ---- DSP ----
; AL -> DSP (CF si no responde)
sb_out:
        push cx
        push dx
        push ax
        mov dx, [cs:sb_base]
        add dx, 0x0C
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .ok
        loop .w
        pop ax
        stc
        jmp .r
.ok:
        pop ax
        out dx, al
        clc
.r:
        pop dx
        pop cx
        ret
; DSP -> AL (CF si no responde)
sb_in:
        push cx
        push dx
        mov dx, [cs:sb_base]
        add dx, 0x0E
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .ok
        loop .w
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
; reiniciar el DSP (CF si no hay DSP en sb_base)
sb_reset:
        push ax
        push cx
        push dx
        mov dx, [cs:sb_base]
        add dx, 6
        mov al, 1
        out dx, al
        mov cx, 16                  ; > 3 microsegundos
.d:
        in al, dx
        loop .d
        xor al, al
        out dx, al
        mov cx, 8
.t:
        call sb_in
        jc .n
        cmp al, 0xAA
        je .ok
.n:
        loop .t
        stc
        jmp .r
.ok:
        clc
.r:
        pop dx
        pop cx
        pop ax
        ret
; escribir un registro del mezclador: AH = registro, AL = valor
sb_mix:
        push dx
        mov dx, [cs:sb_base]
        add dx, 4
        xchg al, ah
        out dx, al
        inc dx
        xchg al, ah
        out dx, al
        pop dx
        ret

; dirección del bloque AL -> DI (en CS)
sb_blk_di:
        movzx di, al
        and di, 3
        shl di, 12
        add di, [cs:buf_off]
        ret

; llenar de silencio el bloque AL
sb_silence:
        push ax
        push cx
        push di
        push es
        call sb_blk_di
        push cs
        pop es
        mov ax, 0x8080              ; 8 bits sin signo
        cmp byte [cs:dev_type], 2
        jne .f
        xor ax, ax                  ; 16 bits con signo
.f:
        mov cx, SB_BLK / 2
        cld
        rep stosw
        pop es
        pop di
        pop cx
        pop ax
        ret

; parar: reset del DSP, canal DMA tapado y bloques en silencio
sb_halt:
        push ax
        push dx
        pushf
        cli
        call sb_reset
        call sb_dma_mask
        mov byte [cs:started], 0
        mov byte [cs:sb_queued], 0
        mov dword [cs:sb_full], 0
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
        xor al, al
.s:
        call sb_silence
        inc al
        cmp al, 4
        jb .s
        popf
        pop dx
        pop ax
        ret

; tapar el canal de DMA que se usa
sb_dma_mask:
        push ax
        cmp byte [cs:dev_type], 2
        je .h
        mov al, [cs:sb_dma]
        or al, 4
        out 0x0A, al
        pop ax
        ret
.h:
        mov al, [cs:sb_hdma]
        and al, 3
        or al, 4
        out 0xD4, al
        pop ax
        ret

; poner en marcha la DMA y el DSP sobre los 4 bloques (cur_rate, dev_type)
sb_start:
        pushad
        call sb_reset
        jc .r
        ; la IRQ puede estar tapada: lo hacen los juegos al salir, y la rutina
        ; por omisión de la BIOS con una interrupción que llegue estando parados
        call sb_unmask
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ebx, word [cs:buf_off]
        add eax, ebx                ; dirección física de los bloques
        cmp byte [cs:dev_type], 2
        je .d16
        ; --- DMA de 8 bits (canales 0-3) ---
        mov bl, [cs:sb_dma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0x0A, al                ; tapar
        out 0x0C, al                ; biestable a cero
        mov al, 0x58                ; lectura, autoinicio, uno a uno
        or al, bl
        out 0x0B, al
        movzx dx, bl
        shl dx, 1                   ; puerto de dirección
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ecx, word [cs:buf_off]
        add eax, ecx
        out dx, al
        mov al, ah
        out dx, al
        shr eax, 16
        push ax
        inc dx                      ; puerto de cuenta
        mov ax, 4 * SB_BLK - 1
        out dx, al
        mov al, ah
        out dx, al
        pop ax
        movzx bx, bl
        mov dl, [cs:pg8+bx]
        xor dh, dh
        out dx, al                  ; página
        mov al, [cs:sb_dma]
        and al, 3
        out 0x0A, al                ; destapar
        ; --- DSP de 8 bits ---
        mov al, 0xD1                ; altavoz
        call sb_out
        ; constante de tiempo: 256 - 1000000 / Hz
        mov eax, 1000000
        xor edx, edx
        movzx ecx, word [cs:cur_rate]
        div ecx
        mov ah, 0
        neg al                      ; 256 - x (en 8 bits)
        push ax
        mov al, 0x40
        call sb_out
        pop ax
        call sb_out
        cmp byte [cs:sb_major], 2
        jae .auto
        call sb_single              ; DSP 1.x: no tiene autoinicio
        jmp .on
.auto:
        mov al, 0x48                ; tamaño del bloque
        call sb_out
        mov al, (SB_BLK - 1) & 0xFF
        call sb_out
        mov al, (SB_BLK - 1) >> 8
        call sb_out
        mov al, 0x1C                ; autoinicio, normal
        cmp byte [cs:sb_hs], 0
        je .g8
        mov al, 0x90                ; autoinicio, alta velocidad
.g8:
        call sb_out
        jmp .on
.d16:
        ; --- DMA de 16 bits (canales 5-7): en palabras ---
        mov bl, [cs:sb_hdma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0xD4, al
        out 0xD8, al
        mov al, 0x58
        or al, bl
        out 0xD6, al
        movzx dx, bl
        shl dx, 2
        add dx, 0xC0                ; dirección
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ecx, word [cs:buf_off]
        add eax, ecx
        mov ecx, eax
        shr eax, 1
        out dx, al
        mov al, ah
        out dx, al
        add dx, 2                   ; cuenta
        mov ax, 4 * SB_BLK / 2 - 1
        out dx, al
        mov al, ah
        out dx, al
        shr ecx, 16
        and cl, 0xFE
        movzx bx, bl
        mov dl, [cs:pg16+bx]
        xor dh, dh
        mov al, cl
        out dx, al
        mov al, [cs:sb_hdma]
        and al, 3
        out 0xD4, al
        ; --- DSP 4.x: frecuencia de salida y 16 bits estéreo ---
        mov al, 0x41
        call sb_out
        mov ax, [cs:cur_rate]
        xchg al, ah
        call sb_out
        xchg al, ah
        call sb_out
        mov al, 0xB6                ; 16 bits, salida, autoinicio, FIFO
        call sb_out
        mov al, 0x30                ; estéreo con signo
        call sb_out
        mov ax, SB_BLK / 2 - 1      ; muestras por bloque
        call sb_out
        mov al, ah
        call sb_out
.on:
        mov byte [cs:started], 1
        mov byte [cs:sb_idle], 0
.r:
        popad
        ret
pg8         db 0x87, 0x83, 0x81, 0x82
pg16        db 0x8F, 0x8B, 0x89, 0x8A

; DSP 1.x (SB 1.0/1.5): un bloque de ciclo único (orden 14h); la interrupción
; pide el siguiente. La DMA sí va en autoinicio sobre los 4 bloques.
sb_single:
        push ax
        mov al, 0x14
        call sb_out
        mov al, (SB_BLK - 1) & 0xFF
        call sb_out
        mov al, (SB_BLK - 1) >> 8
        call sb_out
        pop ax
        ret

; destapar la IRQ de la tarjeta en el PIC
sb_unmask:
        push ax
        push cx
        pushf
        cli
        mov cl, [cs:sb_irq]
        cmp cl, 8
        jae .slave
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0x21
        and al, ah
        out 0x21, al
        jmp .r
.slave:
        sub cl, 8
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0xA1
        and al, ah
        out 0xA1, al
        in al, 0x21
        and al, 0xFB                ; cascada
        out 0x21, al
.r:
        popf
        pop cx
        pop ax
        ret

; quedar residente (DX = párrafos). Los bloques de DMA pueden caer encima del
; código de instalación (se alinean a 16 KB desde resident_end): se llenan de
; silencio aquí, cuando ese código ya ha terminado, y no antes.
sb_tsr:
        call sb_halt
        mov ax, 0x3100
        int 0x21

; ---- interrupción de la tarjeta: ha acabado el bloque sb_play ----
sb_isr:
        cmp byte [cs:started], 0
        jne .ours
        jmp far [cs:sb_old]
.ours:
        push ax
        push dx
        ; IRQ 7 / 15 espuria (el PIC la inventa si la petición desaparece):
        ; no está «en servicio» y no lleva EOI (quitaría el de otra)
        cmp byte [cs:sb_irq], 7
        jne .n7
        mov al, 0x0B
        out 0x20, al
        in al, 0x20
        mov ah, al
        mov al, 0x0A                ; dejar el PIC leyendo IRR, como estaba
        out 0x20, al
        test ah, 0x80
        jnz .real
        jmp .spur
.n7:
        cmp byte [cs:sb_irq], 15
        jne .real
        mov al, 0x0B
        out 0xA0, al
        in al, 0xA0
        mov ah, al
        mov al, 0x0A
        out 0xA0, al
        test ah, 0x80
        jnz .real
        mov al, 0x20
        out 0x20, al                ; solo al maestro
.spur:
        pop dx
        pop ax
        iret
.real:
        mov dx, [cs:sb_base]
        add dx, 0x0E                ; acuse de 8 bits
        cmp byte [cs:dev_type], 2
        jne .ack
        inc dx                      ; acuse de 16 bits
.ack:
        in al, dx
        push bx
        movzx bx, byte [cs:sb_play]
        cmp byte [cs:sb_full+bx], 0
        je .e
        mov byte [cs:sb_full+bx], 0
        dec byte [cs:sb_queued]
        mov byte [cs:sb_idle], 0
        jmp .z
.e:
        inc byte [cs:sb_idle]
.z:
        mov al, bl
        call sb_silence
        inc bl
        and bl, 3
        mov [cs:sb_play], bl
        ; sin nada en cola: no escribir en el que suena
        cmp byte [cs:sb_queued], 0
        jne .k
        cmp [cs:sb_wr], bl
        jne .k
        inc bl
        and bl, 3
        mov [cs:sb_wr], bl
.k:
        pop bx
        cmp byte [cs:sb_idle], SB_IDLE
        jb .more
        call sb_halt                ; un rato sin datos: a descansar
        jmp .eoi
.more:
        cmp byte [cs:dev_type], 1
        jne .eoi
        cmp byte [cs:sb_major], 2
        jae .eoi
        call sb_single              ; DSP 1.x: pedir el bloque siguiente
.eoi:
        mov al, 0x20
        cmp byte [cs:sb_irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop dx
        pop ax
        iret

; ---- reproducir (desde f_play, con pushad/ds/es ya guardados) ----
sb_play_f:
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:sb_req]
        je .rok
        mov [cs:sb_req], dx
        call sb_wait
        call sb_halt
        ; 8 bits: hasta 22050 Hz modo normal, por encima alta velocidad
        cmp dx, 4000
        jae .lo
        mov dx, 4000
.lo:
        cmp dx, 44100
        jbe .hi
        mov dx, 44100
.hi:
        mov [cs:cur_rate], dx
        mov byte [cs:sb_hs], 0
        cmp byte [cs:dev_type], 2
        je .rok
        cmp dx, 22050
        jbe .rok
        cmp word [cs:sb_minor], 0x0201  ; (menor, mayor)
        jb .slow                    ; DSP 1.x y 2.00: sin alta velocidad
        mov byte [cs:sb_hs], 1
        jmp .rok
.slow:
        mov word [cs:cur_rate], 22050
.rok:
.loop:
        cmp word [cs:p_left], 0
        je .done
        call sb_wait_slot
        jc .done
        cli
        call sb_fill
        test bx, bx
        jz .dn
        movzx bx, byte [cs:sb_wr]
        mov byte [cs:sb_full+bx], 1
        inc byte [cs:sb_queued]
        inc bl
        and bl, 3
        mov [cs:sb_wr], bl
        cmp byte [cs:started], 0
        jne .go
        call sb_start
.go:
        sti
        jmp .loop
.dn:
        sti
.done:
        pop es
        pop ds
        popad
        clc
        retf

; esperar un bloque libre (CF si vence el plazo)
sb_wait_slot:
        push bx
        cmp byte [cs:started], 0
        jne .s
        ; parada: se empieza por el bloque 0
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
        clc
        jmp .r
.s:
        call tmo_start
.l:
        sti
        movzx bx, byte [cs:sb_wr]
        cmp byte [cs:sb_full+bx], 0
        jne .w
        cmp bl, [cs:sb_play]
        jne .ok
.w:
        cmp byte [cs:started], 0
        je .s0
        call tmo_check
        jnc .l
        jmp .r
.s0:
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
.ok:
        clc
.r:
        pop bx
        ret

; esperar a que acabe lo que hay en cola
sb_wait:
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        sti
        cmp byte [cs:sb_queued], 0
        je .r
        call tmo_check
        jnc .l
.r:
        ret

; p_src -> bloque sb_wr: 16 bits estéreo (SB16) u 8 bits mono -> BX tramas
sb_fill:
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
        div cx                      ; tramas disponibles
        mov bx, SB_BLK / 4
        cmp byte [cs:dev_type], 2
        je .m
        mov bx, SB_BLK
.m:
        cmp ax, bx
        jbe .n
        mov ax, bx
.n:
        mov bx, ax
        test ax, ax
        jnz .go
        mov word [cs:p_left], 0
        ret
.go:
        push bx
        mul cx
        sub [cs:p_left], ax
        mov al, [cs:sb_wr]
        call sb_blk_di
        push cs
        pop es
        lds si, [cs:p_src]
        mov cx, bx
        mov dl, [cs:p_fmt]
        mov dh, [cs:dev_type]
        cld
.l:
        ; una trama -> AX izquierdo, BX derecho (16 bits con signo)
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
        xchg ax, bx
        jmp .put
.s16:
        lodsw
        mov bx, ax
        test dl, 2
        jz .mono
        lodsw
        xchg ax, bx
        jmp .put
.mono:
        mov ax, bx
.put:
        cmp dh, 2
        jne .p8
        stosw
        mov ax, bx
        stosw
        loop .l
        jmp .end
.p8:
        sar ax, 1                   ; media de los dos canales
        sar bx, 1
        add ax, bx
        mov al, ah
        add al, 0x80
        stosb
        loop .l
.end:
        mov ax, ds
        mov bx, si
        shr bx, 4
        add ax, bx
        and si, 15
        mov [cs:p_src], si
        mov [cs:p_src+2], ax
        pop bx
        ret

; volumen AL = 0..31 (SB16: maestro 30h/31h; SB Pro: 22h)
sb_volume:
        push ax
        and al, 31
        cmp byte [cs:dev_type], 2
        jne .pro
        shl al, 3
        mov ah, 0x30
        call sb_mix
        mov ah, 0x31
        call sb_mix
        pop ax
        ret
.pro:
        cmp byte [cs:sb_major], 3
        jne .r
        shr al, 1
        mov ah, al
        shl ah, 4
        or al, ah
        mov ah, 0x22
        call sb_mix
.r:
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
        ; Sound Blaster (ISA) primero, salvo con /NOSB
        call opt_nosb
        jc .pci
        call sb_detect
        jc .pci
        call sb_install
        jmp .install
.pci:
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
        ; VIA y SiS tienen otros registros: no son «compatibles con ICH»
        mov ax, 0xB109
        xor di, di
        int 0x1A
        cmp cx, 0x1106
        je .nopci
        cmp cx, 0x1039
        je .nopci
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
        ; puede haber varios (el de la placa y el HDMI de la gráfica): se
        ; toma el primero que tenga un códec con salida
        mov ax, 0xB103
        mov ecx, 0x040300
        mov si, [hda_idx]
        int 0x1A
        jc .nohda
        mov [pci_bdf], bx
        call hda_init
        jnc .hok
        inc word [hda_idx]
        cmp word [hda_idx], 4
        jb .noac97
        jmp .hfail
.nohda:
        cmp word [hda_idx], 0
        je .try_ac97
        jmp .hfail
.hok:
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
        cmp byte [dev_type], 1
        je sb_tsr
        cmp byte [dev_type], 2
        je sb_tsr
        mov ax, 0x3100
        int 0x21

; =============================================================================
;  Sound Blaster - detección e instalación (se descarta)
; =============================================================================
; BLASTER=A220 I5 D1 H5 T6 del entorno -> bl_*
sb_parse_env:
        push es
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .none
        mov si, s_blaster
        push di
.c:
        lodsb
        test al, al
        jz .found
        mov ah, [es:di]
        cmp ah, 'a'
        jb .u
        sub ah, 32
.u:
        cmp al, ah
        jne .nx
        inc di
        jmp .c
.nx:
        pop di
.sk:
        cmp byte [es:di], 0
        je .n2
        inc di
        jmp .sk
.n2:
        inc di
        jmp .v
.found:
        pop ax
        mov byte [bl_have], 1
.t:
        mov al, [es:di]
        test al, al
        jz .none
        inc di
        and al, 0xDF
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
        call .hex
        mov [bl_a], dx
        jmp .t
.i:
        call .dec
        mov [bl_i], dl
        jmp .t
.d:
        call .dec
        mov [bl_d], dl
        jmp .t
.h:
        call .dec
        mov [bl_h], dl
        jmp .t
.none:
        pop es
        ret
.hex:
        xor dx, dx
.hl:
        mov al, [es:di]
        sub al, '0'
        jb .hr
        cmp al, 9
        jbe .hd
        and al, 0xDF
        sub al, 'A' - '0' - 10
        cmp al, 10
        jb .hr
        cmp al, 15
        ja .hr
.hd:
        shl dx, 4
        or dl, al
        inc di
        jmp .hl
.hr:
        ret
.dec:
        xor dx, dx
.dl:
        mov al, [es:di]
        sub al, '0'
        jb .dr
        cmp al, 9
        ja .dr
        imul dx, dx, 10
        add dl, al
        inc di
        jmp .dl
.dr:
        ret

; leer un registro del mezclador AL -> AL
sb_mixrd:
        push dx
        mov dx, [sb_base]
        add dx, 4
        out dx, al
        inc dx
        in al, dx
        pop dx
        ret

; buscar la tarjeta -> CF si no hay; dev_type, sb_* rellenos
sb_detect:
        mov byte [bl_have], 0
        mov word [bl_a], 0
        mov byte [bl_i], 0xFF
        mov byte [bl_d], 0xFF
        mov byte [bl_h], 0xFF
        call sb_parse_env
        mov ax, [bl_a]
        test ax, ax
        jz .probe
        mov [sb_base], ax
        call sb_reset
        jnc .found
        stc
        ret
.probe:
        mov si, sb_ports
.p:
        lodsw
        test ax, ax
        jz .no
        mov [sb_base], ax
        call sb_reset
        jc .p
.found:
        mov al, 0xE1                ; versión del DSP
        call sb_out
        call sb_in
        jc .no
        mov [sb_major], al
        call sb_in
        jc .no
        mov [sb_minor], al
        ; valores por omisión, la variable BLASTER manda
        mov byte [sb_irq], 5
        mov byte [sb_dma], 1
        mov byte [sb_hdma], 0
        cmp byte [sb_major], 4
        jb .user
        ; SB16: la configuración está en el mezclador (80h IRQ, 81h DMA)
        mov al, 0x80
        call sb_mixrd
        mov ah, 2
        test al, 1
        jnz .ir
        mov ah, 5
        test al, 2
        jnz .ir
        mov ah, 7
        test al, 4
        jnz .ir
        mov ah, 10
        test al, 8
        jnz .ir
        mov ah, 5
.ir:
        mov [sb_irq], ah
        mov al, 0x81
        call sb_mixrd
        mov bl, al
        mov ah, 0
        test bl, 1
        jnz .d8
        mov ah, 1
        test bl, 2
        jnz .d8
        mov ah, 3
        test bl, 8
        jnz .d8
        mov ah, 1
.d8:
        mov [sb_dma], ah
        mov ah, 5
        test bl, 0x20
        jnz .d16
        mov ah, 6
        test bl, 0x40
        jnz .d16
        mov ah, 7
        test bl, 0x80
        jnz .d16
        mov ah, 0
.d16:
        mov [sb_hdma], ah
.user:
        mov al, [bl_i]
        cmp al, 0xFF
        je .u1
        mov [sb_irq], al
.u1:
        mov al, [bl_d]
        cmp al, 0xFF
        je .u2
        mov [sb_dma], al
.u2:
        mov al, [bl_h]
        cmp al, 0xFF
        je .u3
        mov [sb_hdma], al
.u3:
        ; SB 1.x/2.0/Pro sin BLASTER: la IRQ está en puentes de la tarjeta
        ; (de fábrica la 7 en muchas, no la 5): se pregunta al DSP
        cmp byte [sb_major], 4
        jae .u4
        cmp byte [bl_i], 0xFF
        jne .u4
        call sb_probe_irq
.u4:
        cmp byte [sb_irq], 2
        jne .i9
        mov byte [sb_irq], 9        ; la IRQ 2 llega por la 9
.i9:
        ; comprobar lo que no se puede usar
        movzx bx, byte [sb_irq]
        cmp bx, 15
        ja .no
        cmp byte [sb_dma], 3
        ja .no
        cmp byte [sb_dma], 2        ; la 2 es de la disquetera
        je .no
        mov byte [dev_type], 1
        cmp byte [sb_major], 4
        jb .ok
        mov al, [sb_hdma]
        cmp al, 5
        jb .ok
        cmp al, 7
        ja .ok
        mov byte [dev_type], 2
.ok:
        clc
        ret
.no:
        mov byte [dev_type], 0
        stc
        ret

; ¿qué IRQ usa la tarjeta? Se pide al DSP una interrupción (orden F2h) con
; cada candidata destapada y una rutina de prueba en su vector.
; -> sb_irq (no cambia si ninguna contesta)
sb_probe_irq:
        mov si, pr_list
.n:
        lodsb
        test al, al
        jz .r
        mov [pr_irq], al
        add al, 8
        cmp byte [pr_irq], 8
        jb .v
        add al, 0x70 - 16
.v:
        mov [pr_vec], al
        push si
        push es
        mov ah, 0x35
        int 0x21
        mov [pr_old], bx
        mov [pr_old+2], es
        pop es
        mov al, [pr_vec]
        mov dx, sb_pisr
        mov ah, 0x25
        int 0x21
        ; destapar (guardando las máscaras)
        cli
        in al, 0x21
        mov [pr_m1], al
        in al, 0xA1
        mov [pr_m2], al
        mov al, [sb_irq]
        push ax
        mov al, [pr_irq]
        mov [sb_irq], al
        call sb_unmask
        pop ax
        mov [sb_irq], al
        sti
        call delay                  ; lo que hubiera pendiente no cuenta
        mov byte [pr_hit], 0
        mov al, 0xF2
        call sb_out
        mov cx, 8
.w:
        call delay
        cmp byte [pr_hit], 0
        jne .wd
        loop .w
.wd:
        cli
        mov al, [pr_m2]
        out 0xA1, al
        mov al, [pr_m1]
        out 0x21, al
        sti
        push ds
        lds dx, [pr_old]
        mov al, [cs:pr_vec]
        mov ah, 0x25
        int 0x21
        pop ds
        pop si
        cmp byte [pr_hit], 0
        je .n
        mov al, [pr_irq]
        mov [sb_irq], al
.r:
        ret
sb_pisr:
        push ax
        push dx
        mov byte [cs:pr_hit], 1
        mov dx, [cs:sb_base]
        add dx, 0x0E
        in al, dx
        mov al, 0x20
        cmp byte [cs:pr_irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop dx
        pop ax
        iret
pr_list     db 5, 7, 9, 10, 0
pr_irq      db 0
pr_vec      db 0
pr_hit      db 0
pr_m1       db 0
pr_m2       db 0
pr_old      dd 0

; instalar: bloques alineados, IRQ y BLASTER en el entorno
sb_install:
        ; bloques en una zona de 16 KB alineada (no cruza 64 KB)
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, resident_end + 16383
        and eax, 0xFFFFC000
        sub eax, ebx
        mov [buf_off], ax
        add ax, 4 * SB_BLK
        mov [tsr_end], ax
        ; (sin tocar aún los bloques: pueden estar encima de este código;
        ; sb_tsr los deja en silencio al final)
        call sb_reset
        call sb_dma_mask
        ; vector de la IRQ
        mov al, [sb_irq]
        add al, 8
        cmp byte [sb_irq], 8
        jb .v
        add al, 0x70 - 16
.v:
        mov [sb_vec], al
        mov ah, 0x35
        int 0x21
        mov [sb_old], bx
        mov [sb_old+2], es
        mov al, [sb_vec]
        mov dx, sb_isr
        mov ah, 0x25
        int 0x21
        call sb_unmask
        ; «Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)»
        mov si, name_sb
        cmp byte [dev_type], 2
        jne .n
        mov si, name_sb16
.n:
        call puts
        mov si, msg_sb_en
        call puts
        mov ax, [sb_base]
        call hex3
        mov si, msg_sb_irq
        call puts
        movzx ax, byte [sb_irq]
        call dec2
        mov si, msg_sb_dma
        call puts
        movzx ax, byte [sb_dma]
        call dec2
        cmp byte [dev_type], 2
        jne .nh
        mov si, msg_sb_y
        call puts
        movzx ax, byte [sb_hdma]
        call dec2
.nh:
        mov si, msg_sb_dsp
        call puts
        movzx ax, byte [sb_major]
        call dec2
        mov al, '.'
        call putc
        mov al, [sb_minor]
        aam
        add ax, 0x3030
        xchg al, ah
        call putc
        mov al, ah
        call putc
        mov si, msg_sb_end
        call puts
        cmp byte [bl_have], 0
        jne .r
        call sb_set_blaster
.r:
        ret

; BLASTER=A220 I5 D1 [H5] T6 en el entorno del intérprete que nos ejecuta
sb_set_blaster:
        ; montar el texto
        mov di, bl_text
        mov si, s_blaster
.c:
        lodsb
        test al, al
        jz .c1
        mov [di], al
        inc di
        jmp .c
.c1:
        mov byte [di], 'A'
        inc di
        mov ax, [sb_base]
        call hex3_di
        mov dword [di], ' I'
        add di, 2
        movzx ax, byte [sb_irq]
        call dec_di
        mov dword [di], ' D'
        add di, 2
        movzx ax, byte [sb_dma]
        call dec_di
        cmp byte [dev_type], 2
        jne .t
        mov dword [di], ' H'
        add di, 2
        movzx ax, byte [sb_hdma]
        call dec_di
.t:
        mov al, '1'
        cmp byte [sb_major], 2
        jb .tt
        mov al, '3'
        cmp byte [sb_major], 3
        jb .tt
        mov al, '4'
        cmp byte [sb_major], 4
        jb .tt
        mov al, '6'
.tt:
        mov byte [di], ' '
        mov byte [di+1], 'T'
        mov [di+2], al
        mov byte [di+3], 0
        add di, 4
        sub di, bl_text
        mov [bl_len], di            ; con el cero
        ; entorno del padre
        mov ax, [0x16]
        test ax, ax
        jz .fail
        mov es, ax
        mov ax, [es:0x2C]
        test ax, ax
        jz .fail
        mov es, ax
        dec ax
        push ds
        mov ds, ax
        mov cx, [3]                 ; tamaño del bloque en párrafos
        pop ds
        shl cx, 4
        mov [bl_size], cx
        ; fin de las variables: ES:BX = el cero que cierra la lista
        xor bx, bx
        cmp byte [es:0], 0
        je .end
.e:
        cmp word [es:bx], 0
        je .e1
        inc bx
        cmp bx, cx
        jb .e
        jmp .fail
.e1:
        inc bx
.end:
        ; lo que va detrás (DOS 3+: cuenta y nombre del programa)
        lea si, [bx+1]
        xor dx, dx
        cmp word [es:si], 1
        jne .tl
        mov dx, 2
        lea di, [si+2]
.tn:
        inc dx
        cmp byte [es:di], 0
        je .tl
        inc di
        cmp di, cx
        jb .tn
        jmp .fail
.tl:
        ; ¿cabe? bx (variables) + texto + 1 (cero final) + dx (cola)
        mov ax, bx
        add ax, [bl_len]
        inc ax
        add ax, dx
        cmp ax, cx
        ja .full
        ; mover la cola hacia delante
        push ds
        push es
        pop ds
        std
        lea si, [bx+1]
        add si, dx
        dec si                      ; último octeto de la cola
        mov di, si
        add di, [cs:bl_len]
        mov cx, dx
        inc cx                      ; y el cero final
        rep movsb
        cld
        pop ds
        ; copiar el texto en ES:BX
        mov di, bx
        mov si, bl_text
        mov cx, [bl_len]
        rep movsb
        mov byte [es:di], 0         ; fin de la lista
        mov si, msg_sb_set
        call puts
        mov si, bl_text
        call puts
        mov si, msg_crlf2
        call puts
        ret
.full:
        mov si, msg_sb_full
        call puts
        mov si, bl_text
        call puts
        mov si, msg_crlf2
        call puts
.fail:
        ret

; AX -> 3 cifras hexadecimales (puerto)
hex3:
        push ax
        push cx
        mov cx, 3
        shl ax, 4
.l:
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
        loop .l
        pop cx
        pop ax
        mov al, 'h'
        jmp putc
hex3_di:
        push cx
        mov cx, 3
        shl ax, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov [di], al
        inc di
        pop ax
        loop .l
        pop cx
        ret
; AX (0-99) en decimal
dec2:
        cmp ax, 10
        jb .u
        push ax
        mov cl, 10
        div cl
        add al, '0'
        call putc
        mov al, ah
        add al, '0'
        call putc
        pop ax
        ret
.u:
        add al, '0'
        jmp putc
dec_di:
        cmp ax, 10
        jb .u
        mov cl, 10
        div cl
        add al, '0'
        mov [di], al
        inc di
        mov al, ah
.u:
        add al, '0'
        mov [di], al
        inc di
        ret

sb_ports    dw 0x220, 0x240, 0x260, 0x280, 0
s_blaster   db "BLASTER=", 0
msg_sb_en   db " en ", 0
msg_sb_irq  db ", IRQ ", 0
msg_sb_dma  db ", DMA ", 0
msg_sb_y    db " y ", 0
msg_sb_dsp  db " (DSP ", 0
msg_sb_end  db ")", 13, 10, 0
msg_sb_set  db "Variable puesta: ", 0
msg_sb_full db "No cabe en el entorno; póngala en AUTOEXEC.BAT: SET ", 0
bl_have     db 0
bl_i        db 0
bl_d        db 0
bl_h        db 0
bl_a        dw 0
bl_len      dw 0
bl_size     dw 0
bl_text     times 40 db 0

; ¿/NOSB en la línea de órdenes? -> CF
opt_nosb:
        mov si, 0x81
.l:
        lodsb
        cmp al, 13
        je .no
        cmp al, '/'
        jne .l
        mov al, [si]
        and al, 0xDF
        cmp al, 'N'
        jne .l
        stc
        ret
.no:
        clc
        ret

; poner en marcha el controlador HD Audio y buscar una salida en el códec
hda_init:
        smsw ax
        test al, 1
        jz .rm
        stc                         ; modo V86 (EMM386): sin acceso a los registros
        ret
.rm:
        call a20_open
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
        ; Intel: clase de tráfico 0 (TCSEL), o el flujo puede no salir limpio
        mov bx, [pci_bdf]
        mov ax, 0xB109
        xor di, di
        int 0x1A
        cmp cx, 0x8086
        jne .ntc
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x44
        int 0x1A
        jc .ntc
        and cl, 0xF8
        mov bx, [pci_bdf]
        mov ax, 0xB10B
        mov di, 0x44
        int 0x1A
.ntc:
        cmp dword [hda_base], 0
        je .fail
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
        call delay                  ; el códec tarda en despertar
        call delay
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
        mov ecx, eax
        shr eax, 30
        cmp al, 1                   ; sin conexión física
        je .next
        ; solo línea (0), altavoz (1) o auriculares (2): el primer pin de
        ; muchos códecs es la salida digital (S/PDIF) o un conector de
        ; entrada que también puede sacar señal
        shr ecx, 20
        and cl, 0x0F
        cmp cl, 2
        ja .next
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
        call a20_restore
        clc
        ret
.fail:
        call a20_restore
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
        ; sin puertos asignados (ICH6/7 con solo los BAR de memoria): nada
        ; de escribir en los puertos 0..3Fh, que son del controlador de DMA
        jz .nobar
        cmp word [nam], 0
        jne .bars
.nobar:
        stc
        ret
.bars:
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
        ; esperar al códec: en frío puede tardar medio segundo largo
        mov cx, 64                  ; ~1 s
        mov dx, [nabm]
        add dx, 0x30
.w:
        in eax, dx
        test ax, 0x0100
        jnz .ready
        call delay
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
hda_idx     dw 0
tsr_end     dw 0
msg_title   db "HSPSND 0.1 - controlador de sonido de Hispa-DOS", 13, 10, 0
msg_already db "Ya está instalado.", 13, 10, 0
msg_ac97    db "Audio Intel AC'97 en E/S ", 0
msg_vra     db ", frecuencia variable", 13, 10, 0
msg_novra   db ", 48 kHz fijos", 13, 10, 0
msg_hda     db "Intel HD Audio en ", 0
msg_codec   db ", nodo DAC ", 0
msg_pin     db ", salida ", 0
msg_crlf2   db 13, 10, 0
msg_hdafail db "Hay un controlador HD Audio, pero no se ha podido poner en marcha.", 13, 10, 0
msg_nocard  db "No se ha encontrado ninguna tarjeta de sonido compatible.", 13, 10, 0
msg_fail    db "El códec AC'97 no responde.", 13, 10, 0
