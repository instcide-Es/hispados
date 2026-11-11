; =============================================================================
;  PLAYWAV.COM - reproductor de sonido de HispaDOS (usa HSPSND)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  PLAYWAV fichero.wav           reproduce un WAV PCM (8/16 bits, mono/estéreo)
;  PLAYWAV /TONE hz [ms]         tono de prueba (onda cuadrada, 22050 Hz)
; =============================================================================
        cpu 386
        bits 16
        org 0x100

CHUNK       equ 32768

start:
        mov sp, stack_top
        mov si, msg_title
        call puts
        ; ¿driver instalado?
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne nodrv
        mov [drv], bx
        mov [drv+2], es
        push cs
        pop es
        mov ah, 0
        call far [drv]
        test al, al
        jz nocard
        push ds
        push es
        pop ds
        mov si, di
        push cs
        pop es
        mov di, cardname
.cn:
        lodsb
        stosb
        test al, al
        jnz .cn
        pop ds
        ; argumentos
        mov si, 0x81
        call skip_sp
        cmp byte [si], 13
        je usage
        cmp byte [si], '/'
        je tone
        ; nombre de fichero
        mov di, fname
.fn:
        lodsb
        cmp al, 13
        je .fe
        cmp al, ' '
        je .fe
        stosb
        jmp .fn
.fe:
        mov byte [di], 0
        jmp playfile

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
nodrv:
        mov si, msg_nodrv
        call puts
        mov ax, 0x4C02
        int 0x21
nocard:
        mov si, msg_nocard
        call puts
        mov ax, 0x4C03
        int 0x21

; ---- /TONE hz [ms] ----
tone:
        add si, 5
        call skip_sp
        call parse_num
        test eax, eax
        jz usage
        mov [freq], ax
        call skip_sp
        mov eax, 1000
        cmp byte [si], 13
        je .ms
        call parse_num
.ms:
        mov [ms], eax
        mov si, msg_tone
        call puts
        movzx eax, word [freq]
        call putnum
        mov si, msg_hz
        call puts
        call print_card
        ; muestras totales = 22050 * ms / 1000
        mov eax, [ms]
        imul eax, eax, 22050
        mov ecx, 1000
        xor edx, edx
        div ecx
        mov [total], eax
        ; medio periodo en muestras
        mov ax, 22050
        xor dx, dx
        mov cx, [freq]
        shl cx, 1
        div cx
        test ax, ax
        jnz .hp
        inc ax
.hp:
        mov [half], ax
        mov word [phase], 0
        mov word [level], 8000
.gen:
        mov eax, [total]
        test eax, eax
        jz .end
        mov ecx, CHUNK / 2
        cmp eax, ecx
        jae .n
        mov ecx, eax
.n:
        sub [total], ecx
        push cx
        mov di, buf
.s:
        mov ax, [level]
        stosw
        inc word [phase]
        mov ax, [phase]
        cmp ax, [half]
        jb .k
        mov word [phase], 0
        neg word [level]
.k:
        loop .s
        pop cx
        shl cx, 1
        mov si, buf
        mov dx, 22050
        mov al, 1                   ; 16 bits mono
        mov ah, 1
        call far [drv]
        jmp .gen
.end:
        jmp finish

; ---- fichero WAV ----
playfile:
        mov dx, fname
        mov ax, 0x3D00
        int 0x21
        jc .nf
        mov [handle], ax
        mov bx, ax
        mov dx, hdr
        mov cx, 12
        mov ah, 0x3F
        int 0x21
        cmp dword [hdr], 'RIFF'
        jne .bad
        cmp dword [hdr+8], 'WAVE'
        jne .bad
.chunk:
        mov bx, [handle]
        mov dx, hdr
        mov cx, 8
        mov ah, 0x3F
        int 0x21
        cmp ax, 8
        jb .bad
        cmp dword [hdr], 'fmt '
        je .fmt
        cmp dword [hdr], 'data'
        je .data
        ; saltar el bloque (tamaño par)
        mov edx, [hdr+4]
        inc edx
        and dl, 0xFE
        mov cx, [hdr+6+0]
        mov ecx, edx
        shr ecx, 16
        mov ax, 0x4201
        mov bx, [handle]
        int 0x21
        jmp .chunk
.fmt:
        mov bx, [handle]
        mov dx, fmt
        mov cx, 16
        mov ah, 0x3F
        int 0x21
        mov edx, [hdr+4]
        sub edx, 16
        jbe .chunk
        inc edx
        and dl, 0xFE
        mov ecx, edx
        shr ecx, 16
        mov ax, 0x4201
        mov bx, [handle]
        int 0x21
        jmp .chunk
.data:
        cmp word [fmt], 1
        jne .unsup
        mov al, 0
        cmp word [fmt+14], 16
        jne .b8
        or al, 1
.b8:
        cmp word [fmt+2], 2
        jne .m
        or al, 2
.m:
        mov [wfmt], al
        mov eax, [hdr+4]
        mov [total], eax
        mov si, msg_playing
        call puts
        mov si, fname
        call puts
        mov si, msg_open
        call puts
        mov eax, [fmt+4]
        call putnum
        mov si, msg_hzc
        call puts
        movzx eax, word [fmt+14]
        call putnum
        mov si, msg_bits
        call puts
        mov si, msg_mono
        test byte [wfmt], 2
        jz .mm
        mov si, msg_stereo
.mm:
        call puts
        call print_card
.loop:
        mov eax, [total]
        test eax, eax
        jz .done
        mov ecx, CHUNK
        cmp eax, ecx
        jae .r
        mov ecx, eax
.r:
        mov bx, [handle]
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .done
        test ax, ax
        jz .done
        movzx eax, ax
        sub [total], eax
        mov cx, ax
        mov si, buf
        mov dx, [fmt+4]
        mov al, [wfmt]
        mov ah, 1
        call far [drv]
        jmp .loop
.done:
        mov bx, [handle]
        mov ah, 0x3E
        int 0x21
        jmp finish
.nf:
        mov si, msg_nf
        call puts
        mov ax, 0x4C04
        int 0x21
.bad:
        mov si, msg_bad
        call puts
        mov ax, 0x4C05
        int 0x21
.unsup:
        mov si, msg_unsup
        call puts
        mov ax, 0x4C06
        int 0x21

finish:
        mov ah, 2
        call far [drv]
        mov si, msg_done
        call puts
        mov ax, 0x4C00
        int 0x21

print_card:
        mov si, msg_on
        call puts
        mov si, cardname
        call puts
        mov si, msg_crlf
        jmp puts

skip_sp:
        cmp byte [si], ' '
        jne .r
        inc si
        jmp skip_sp
.r:
        ret

; SI -> EAX
parse_num:
        xor eax, eax
        xor ebx, ebx
.l:
        mov bl, [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        imul eax, eax, 10
        add eax, ebx
        inc si
        jmp .l
.e:
        ret

putnum:
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop edx
        pop ecx
        pop ebx
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

msg_title   db "PLAYWAV 0.1 - HispaDOS", 13, 10, 0
msg_usage   db "Usage: PLAYWAV file.wav | PLAYWAV /TONE hz [ms]", 13, 10, 0
msg_nodrv   db "HSPSND is not loaded.", 13, 10, 0
msg_nocard  db "No sound card.", 13, 10, 0
msg_tone    db "Tone ", 0
msg_hz      db " Hz", 0
msg_playing db "Playing ", 0
msg_open    db " (", 0
msg_hzc     db " Hz, ", 0
msg_bits    db "-bit ", 0
msg_mono    db "mono)", 0
msg_stereo  db "stereo)", 0
msg_on      db " on ", 0
msg_done    db "Done.", 13, 10, 0
msg_nf      db "File not found.", 13, 10, 0
msg_bad     db "Not a WAV file.", 13, 10, 0
msg_unsup   db "Only PCM WAV files are supported.", 13, 10, 0
msg_crlf    db 13, 10, 0

        section .bss
drv         resd 1
handle      resw 1
freq        resw 1
half        resw 1
phase       resw 1
level       resw 1
wfmt        resb 1
            resb 1
ms          resd 1
total       resd 1
hdr         resb 12
fmt         resb 16
fname       resb 80
cardname    resb 32
buf         resb CHUNK
            resb 512
stack_top:
