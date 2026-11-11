; =============================================================================
;  HispaDOS kernel (KERNEL.SYS)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  Núcleo compatible con la API de MS-DOS 6.22 escrito desde cero a partir de
;  documentación pública (Ralf Brown's Interrupt List, especificación FAT,
;  formato MZ). No contiene código de Microsoft.
;
;  Carga: 0070:0000. Entrada: DL = unidad, DS:SI = sector de arranque (BPB).
; =============================================================================
        cpu 386
        bits 16
        org 0

KSEG            equ 0x0070
HISPA_MAJOR      equ 0
HISPA_MINOR      equ 1
DOS_MAJOR       equ 6
DOS_MINOR       equ 22

; --- marco de registros del llamante en INT 21h (SS:BP) ---
%define F_AX bp+0
%define F_AL bp+0
%define F_AH bp+1
%define F_BX bp+2
%define F_BL bp+2
%define F_BH bp+3
%define F_CX bp+4
%define F_CL bp+4
%define F_CH bp+5
%define F_DX bp+6
%define F_DL bp+6
%define F_DH bp+7
%define F_SI bp+8
%define F_DI bp+10
%define F_BP bp+12
%define F_DS bp+14
%define F_ES bp+16
%define F_IP bp+18
%define F_CS bp+20
%define F_FL bp+22

; --- tabla de ficheros del sistema (SFT) ---
SFT_REF         equ 0
SFT_TYPE        equ 1
SFT_MODE        equ 2
SFT_ATTR        equ 3
SFT_CLUS        equ 4
SFT_SIZE        equ 6
SFT_POS         equ 10
SFT_CCL         equ 14
SFT_CIX         equ 16
SFT_DIRC        equ 18
SFT_DIRI        equ 20
SFT_TIME        equ 22
SFT_DATE        equ 24
SFT_LEN         equ 32
NSFT            equ 20

T_FREE          equ 0
T_CON           equ 1
T_FILE          equ 2
T_NUL           equ 3

; --- errores DOS ---
E_FUNC          equ 1
E_NOFILE        equ 2
E_NOPATH        equ 3
E_NOHANDLES     equ 4
E_ACCESS        equ 5
E_HANDLE        equ 6
E_MCB           equ 7
E_NOMEM         equ 8
E_BLOCK         equ 9
E_FORMAT        equ 11
E_DRIVE         equ 15
E_NOMORE        equ 18

NPATH_MAX       equ 66

; =============================================================================
        section .text
start:
        cli
        cld
        mov ax, KSEG
        mov es, ax
        mov di, bootsec
        mov cx, 256
        rep movsw
        mov ds, ax
        mov ss, ax
        mov sp, kstack_top
        sti
        mov [boot_drive], dl

        call serial_init
        call geom_init

        mov si, msg_banner
        call kputs

        call fat_load
        jnc .fatok
        mov si, msg_fatbad
        call kputs
        jmp halt
.fatok:
        call vectors_init
        call mem_init
        call root_psp_init

.shell:
        mov ax, KSEG
        mov ds, ax
        mov es, ax
        mov dx, shell_path
        mov bx, shell_pb
        mov ax, 0x4B00
        int 0x21
        jc .noshell
        mov si, msg_reshell
        call kputs
        jmp .shell
.noshell:
        mov ax, KSEG
        mov ds, ax
        mov si, msg_noshell
        call kputs
halt:
        sti
        hlt
        jmp halt

; -----------------------------------------------------------------------------
;  Geometría del disco a partir del BPB
; -----------------------------------------------------------------------------
geom_init:
        mov ax, [bootsec+0x0B]
        mov [bps], ax
        movzx ax, byte [bootsec+0x0D]
        mov [spcw], ax
        shl ax, 4
        mov [epc], ax
        mov ax, [spcw]
        shl ax, 9
        mov [bpc], ax
        mov ax, [bootsec+0x0E]
        mov [fat_lba], ax
        mov ax, [bootsec+0x11]
        mov [rootent], ax
        mov ax, [bootsec+0x16]
        mov [spf], ax
        mov ax, [bootsec+0x18]
        mov [spt], ax
        mov ax, [bootsec+0x1A]
        mov [heads], ax
        movzx ax, byte [bootsec+0x10]
        mov [nfats], ax
        mul word [spf]
        add ax, [fat_lba]
        mov [root_lba], ax
        mov ax, [rootent]
        add ax, 15
        shr ax, 4
        mov [root_secs], ax
        add ax, [root_lba]
        mov [data_lba], ax
        mov ax, [bootsec+0x13]
        sub ax, [data_lba]
        xor dx, dx
        div word [spcw]
        mov [nclus], ax
        mov word [cache_lba], 0xFFFF
        ret

; cargar la FAT entera en memoria (FAT12: máximo 12 sectores)
fat_load:
        mov cx, [spf]
        cmp cx, 12
        ja .bad
        mov ax, [fat_lba]
        mov bx, fatbuf
        push ds
        pop es
.l:
        call read_sector
        jc .bad
        inc ax
        add bx, 512
        loop .l
        clc
        ret
.bad:
        stc
        ret

; -----------------------------------------------------------------------------
;  Vectores de interrupción
; -----------------------------------------------------------------------------
vectors_init:
        push es
        xor ax, ax
        mov es, ax
        cli
        mov si, vec_table
.l:
        lodsb
        cmp al, 0xFF
        je .done
        movzx bx, al
        shl bx, 2
        lodsw
        mov [es:bx], ax
        mov word [es:bx+2], KSEG
        jmp .l
.done:
        sti
        pop es
        ret

vec_table:
        db 0x20
        dw int20
        db 0x21
        dw int21
        db 0x22
        dw int22
        db 0x23
        dw int23
        db 0x24
        dw int24
        db 0x25
        dw int25
        db 0x26
        dw int25
        db 0x27
        dw int27
        db 0x28
        dw int_iret
        db 0x29
        dw int29
        db 0x2A
        dw int_iret
        db 0x2F
        dw int2f
        db 0xFF

; -----------------------------------------------------------------------------
;  Memoria: cadena de MCB (Memory Control Blocks)
; -----------------------------------------------------------------------------
mem_init:
        mov ax, kernel_end + 15
        shr ax, 4
        add ax, KSEG
        mov [first_mcb], ax
        int 0x12                    ; AX = KB de memoria convencional
        shl ax, 6
        mov [mem_top], ax
        mov bx, [first_mcb]
        mov es, bx
        mov byte [es:0], 'Z'
        mov word [es:1], 0
        sub ax, bx
        dec ax
        mov [es:3], ax
        ret

; BX = párrafos -> CF=0: AX = segmento | CF=1: AX=8, BX = mayor bloque
mcb_alloc:
        push cx
        push dx
        push si
        push es
        xor dx, dx
        mov ax, [first_mcb]
.l:
        mov es, ax
        cmp word [es:1], 0
        jne .next
        mov cx, [es:3]
        cmp cx, bx
        jae .fit
        cmp cx, dx
        jbe .next
        mov dx, cx
.next:
        cmp byte [es:0], 'Z'
        je .fail
        cmp byte [es:0], 'M'
        jne .corrupt
        add ax, [es:3]
        inc ax
        jmp .l
.fit:
        cmp cx, bx
        je .take
        push ax
        mov si, ax
        add si, bx
        inc si
        mov al, [es:0]
        push es
        mov es, si
        mov [es:0], al
        mov word [es:1], 0
        sub cx, bx
        dec cx
        mov [es:3], cx
        pop es
        mov byte [es:0], 'M'
        mov [es:3], bx
        pop ax
.take:
        mov si, [cur_psp]
        mov [es:1], si
        inc ax
        pop es
        pop si
        pop dx
        pop cx
        clc
        ret
.corrupt:
        mov ax, E_MCB
        xor bx, bx
        jmp .out
.fail:
        mov bx, dx
        mov ax, E_NOMEM
.out:
        pop es
        pop si
        pop dx
        pop cx
        stc
        ret

; unir bloques libres contiguos
mcb_coalesce:
        push ax
        push bx
        push cx
        push es
        mov ax, [first_mcb]
.l:
        mov es, ax
        cmp byte [es:0], 'M'
        jne .done
        mov bx, ax
        add bx, [es:3]
        inc bx
        cmp word [es:1], 0
        jne .adv
        push ds
        mov ds, bx
        cmp word [ds:1], 0
        jne .nm
        mov cl, [ds:0]
        mov bx, [ds:3]
        pop ds
        mov [es:0], cl
        inc bx
        add [es:3], bx
        jmp .l
.nm:
        pop ds
.adv:
        mov ax, bx
        jmp .l
.done:
        pop es
        pop cx
        pop bx
        pop ax
        ret

; ES = segmento de datos del bloque -> CF en error (AX)
mcb_free:
        push es
        push ax
        mov ax, es
        dec ax
        mov es, ax
        mov al, [es:0]
        cmp al, 'M'
        je .ok
        cmp al, 'Z'
        je .ok
        pop ax
        pop es
        mov ax, E_BLOCK
        stc
        ret
.ok:
        mov word [es:1], 0
        pop ax
        pop es
        call mcb_coalesce
        clc
        ret

; ES = bloque, BX = nuevo tamaño -> CF: AX=error, BX=máximo
mcb_resize:
        push cx
        push dx
        push si
        push es
        mov ax, es
        dec ax
        mov es, ax
        mov al, [es:0]
        cmp al, 'M'
        je .ok
        cmp al, 'Z'
        je .ok
        mov ax, E_BLOCK
        jmp .err
.ok:
        mov cx, [es:3]
        cmp bx, cx
        jbe .shrink
        ; crecer: ¿el siguiente está libre?
        cmp byte [es:0], 'Z'
        je .nogrow
        mov si, es
        add si, cx
        inc si
        push ds
        mov ds, si
        cmp word [ds:1], 0
        jne .nogrow_ds
        mov dx, [ds:3]
        mov al, [ds:0]
        pop ds
        add dx, cx
        inc dx                  ; tamaño unido
        cmp dx, bx
        jb .toosmall
        mov [es:0], al
        mov [es:3], dx
        mov cx, dx
        jmp .shrink
.nogrow_ds:
        pop ds
.nogrow:
        mov bx, cx
        mov ax, E_NOMEM
        jmp .err
.toosmall:
        mov bx, dx
        mov ax, E_NOMEM
        jmp .err
.shrink:
        cmp bx, cx
        je .done
        mov si, es
        add si, bx
        inc si
        mov al, [es:0]
        mov byte [es:0], 'M'
        mov [es:3], bx
        push es
        mov es, si
        mov [es:0], al
        mov word [es:1], 0
        sub cx, bx
        dec cx
        mov [es:3], cx
        pop es
        call mcb_coalesce
.done:
        pop es
        pop si
        pop dx
        pop cx
        clc
        ret
.err:
        pop es
        pop si
        pop dx
        pop cx
        stc
        ret

; liberar todos los bloques cuyo dueño es AX
mcb_free_owner:
        push bx
        push es
        mov bx, [first_mcb]
.l:
        mov es, bx
        cmp [es:1], ax
        jne .n
        mov word [es:1], 0
.n:
        cmp byte [es:0], 'M'
        jne .done
        add bx, [es:3]
        inc bx
        jmp .l
.done:
        pop es
        pop bx
        call mcb_coalesce
        ret

; -----------------------------------------------------------------------------
;  PSP raíz (el del propio núcleo) y entorno inicial
; -----------------------------------------------------------------------------
root_psp_init:
        mov word [cur_psp], 8
        mov bx, 16
        call mcb_alloc
        mov [root_psp], ax
        mov es, ax
        xor di, di
        mov cx, 128
        xor ax, ax
        rep stosw
        mov ax, es
        mov word [es:0], 0x20CD
        mov dx, [mem_top]
        mov [es:2], dx
        mov [es:0x16], ax
        mov di, 0x18
        mov cx, 20
        mov al, 0xFF
        rep stosb
        mov byte [es:0x18], 0
        mov byte [es:0x19], 0
        mov byte [es:0x1A], 0
        mov byte [es:0x1B], 1
        mov byte [es:0x1C], 2
        mov word [es:0x32], 20
        mov word [es:0x34], 0x18
        mov [es:0x36], es
        mov dword [es:0x38], 0xFFFFFFFF
        mov word [es:0x50], 0x21CD
        mov byte [es:0x52], 0xCB
        ; SFT estáticas: CON, AUX(NUL), PRN(NUL)
        mov byte [sft_table+SFT_REF], 3
        mov byte [sft_table+SFT_TYPE], T_CON
        mov byte [sft_table+SFT_LEN+SFT_REF], 1
        mov byte [sft_table+SFT_LEN+SFT_TYPE], T_NUL
        mov byte [sft_table+2*SFT_LEN+SFT_REF], 1
        mov byte [sft_table+2*SFT_LEN+SFT_TYPE], T_NUL
        ; entorno inicial
        mov bx, (root_env_end - root_env + 15) / 16
        call mcb_alloc
        push es
        mov es, ax
        xor di, di
        mov si, root_env
        mov cx, root_env_end - root_env
        rep movsb
        pop es
        mov [es:0x2C], ax
        mov ax, es
        mov [cur_psp], ax
        mov [dta_seg], ax
        mov word [dta_off], 0x80
        ; el entorno pertenece al PSP raíz
        mov ax, [es:0x2C]
        dec ax
        push es
        mov dx, es
        mov es, ax
        mov [es:1], dx
        pop es
        ret

root_env:
        db "COMSPEC=A:\COMMAND.COM", 0
        db "PATH=A:\", 0
        db "PROMPT=$P$G", 0
        db 0
root_env_end:

; -----------------------------------------------------------------------------
;  Consola: BIOS (INT 10h / INT 16h) + espejo en el puerto serie COM1
; -----------------------------------------------------------------------------
serial_init:
        mov dx, 0x3FF
        mov al, 0x5A
        out dx, al
        in al, dx
        cmp al, 0x5A
        jne .none
        mov dx, 0x3F9
        xor al, al
        out dx, al
        mov dx, 0x3FB
        mov al, 0x80
        out dx, al
        mov dx, 0x3F8
        mov al, 1
        out dx, al
        inc dx
        xor al, al
        out dx, al
        mov dx, 0x3FB
        mov al, 3
        out dx, al
        mov dx, 0x3FA
        mov al, 0xC7
        out dx, al
        mov dx, 0x3FC
        mov al, 3
        out dx, al
        mov byte [ser_ok], 1
.none:
        ret

ser_putc:
        cmp byte [ser_ok], 0
        je .r
        push ax
        push cx
        push dx
        mov ah, al
        mov dx, 0x3FD
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x20
        jnz .go
        loop .w
.go:
        mov al, ah
        mov dx, 0x3F8
        out dx, al
        pop dx
        pop cx
        pop ax
.r:
        ret

; AL -> pantalla y serie
con_out:
        cmp al, 9
        jne con_raw
        push ax
.tab:
        mov al, ' '
        call con_raw
        test byte [col], 7
        jnz .tab
        pop ax
        ret

con_raw:
        push ax
        push bx
        push bp
        call ser_putc
        mov ah, 0x0E
        mov bx, 0x0007
        int 0x10
        pop bp
        pop bx
        pop ax
        cmp al, 13
        je .cr
        cmp al, 8
        je .bs
        cmp al, 32
        jb .r
        inc byte [col]
.r:
        ret
.cr:
        mov byte [col], 0
        ret
.bs:
        cmp byte [col], 0
        je .r
        dec byte [col]
        ret

; mirar sin esperar: CF=1 si hay carácter, AL = carácter (lo consume)
con_poll:
        cmp byte [pend_scan], 0
        je .k
        mov al, [pend_scan]
        mov byte [pend_scan], 0
        stc
        ret
.k:
        push bp
        mov ah, 1
        int 0x16
        pop bp
        jz .ser
        push bp
        xor ah, ah
        int 0x16
        pop bp
        cmp al, 0xE0
        jne .k1
        test ah, ah
        jz .k1
        xor al, al
.k1:
        test al, al
        jnz .got
        mov [pend_scan], ah
        test ah, ah
        jnz .got
        mov byte [pend_scan], 3
.got:
        mov byte [ser_last], 0
        stc
        ret
.ser:
        cmp byte [ser_ok], 0
        je .none
        push dx
        mov dx, 0x3FD
        in al, dx
        test al, 1
        jz .nos
        mov dx, 0x3F8
        in al, dx
        pop dx
        cmp al, 10
        jne .notlf
        cmp byte [ser_last], 13
        mov byte [ser_last], 10
        je .none
        mov al, 13
        stc
        ret
.notlf:
        mov [ser_last], al
        cmp al, 127
        jne .sgot
        mov al, 8
.sgot:
        stc
        ret
.nos:
        pop dx
.none:
        clc
        ret

; ZF=0 si hay un carácter esperando (no lo consume)
con_check:
        cmp byte [peek_ok], 0
        jne .yes
        call con_poll
        jnc .no
        mov [peek_ch], al
        mov byte [peek_ok], 1
.yes:
        or al, 1
        ret
.no:
        xor al, al
        ret

; esperar un carácter -> AL
con_getc:
        cmp byte [peek_ok], 0
        je .wait
        mov byte [peek_ok], 0
        mov al, [peek_ch]
        ret
.wait:
        call con_poll
        jc .r
        jmp .wait
.r:
        ret

; ES:DI = búfer estilo 0Ah ([max][cuenta][datos...])
line_input:
        push ax
        push bx
        push cx
        push dx
        movzx cx, byte [es:di]
        test cx, cx
        jz .out
        xor bx, bx
.l:
        call con_getc
        test al, al
        jnz .nx
        call con_getc
        jmp .l
.nx:
        cmp al, 13
        je .cr
        cmp al, 8
        je .bs
        cmp al, 27
        je .esc
        cmp al, 32
        jb .l
        mov dx, bx
        inc dx
        cmp dx, cx
        jae .bell
        mov [es:di+2+bx], al
        inc bx
        call con_out
        jmp .l
.bell:
        mov al, 7
        call con_raw
        jmp .l
.bs:
        test bx, bx
        jz .l
        dec bx
        call .rub
        jmp .l
.esc:
        test bx, bx
        jz .l
        dec bx
        call .rub
        jmp .esc
.rub:
        push ax
        mov al, 8
        call con_raw
        mov al, ' '
        call con_raw
        mov al, 8
        call con_raw
        pop ax
        ret
.cr:
        mov byte [es:di+2+bx], 13
        mov [es:di+1], bl
        call con_out
.out:
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; imprimir cadena ASCIIZ DS:SI
kputs:
        push ax
        push si
.l:
        lodsb
        test al, al
        jz .d
        call con_out
        jmp .l
.d:
        pop si
        pop ax
        ret

; -----------------------------------------------------------------------------
;  Disco: lectura de sectores por BIOS INT 13h
; -----------------------------------------------------------------------------
; AX = LBA, ES:BX = destino -> CF
read_sector:
        pusha
        mov di, 3
.retry:
        push ax
        xor dx, dx
        div word [spt]
        mov cx, dx
        inc cx
        xor dx, dx
        div word [heads]
        mov dh, dl
        mov ch, al
        shl ah, 6
        or cl, ah
        mov dl, [boot_drive]
        mov ax, 0x0201
        int 0x13
        pop ax
        jnc .ok
        push ax
        xor ax, ax
        mov dl, [boot_drive]
        int 0x13
        pop ax
        dec di
        jnz .retry
        popa
        stc
        ret
.ok:
        popa
        clc
        ret

; AX = LBA -> sector en secbuf (con caché de un sector)
cache_read:
        cmp ax, [cache_lba]
        je .hit
        push es
        push bx
        push ds
        pop es
        mov bx, secbuf
        call read_sector
        pop bx
        pop es
        jc .err
        mov [cache_lba], ax
.hit:
        clc
        ret
.err:
        mov word [cache_lba], 0xFFFF
        stc
        ret

; -----------------------------------------------------------------------------
;  FAT12
; -----------------------------------------------------------------------------
; AX = clúster -> AX = siguiente
fat_next:
        push bx
        push dx
        mov bx, ax
        shr bx, 1
        add bx, ax
        mov dx, [fatbuf+bx]
        test al, 1
        jz .even
        shr dx, 4
        jmp .d
.even:
        and dx, 0x0FFF
.d:
        mov ax, dx
        pop dx
        pop bx
        ret

; AX = clúster -> AX = primer LBA (destruye DX)
clus_lba:
        sub ax, 2
        mul word [spcw]
        add ax, [data_lba]
        ret

; contar clústeres libres -> AX
fat_free_count:
        push bx
        push cx
        xor bx, bx
        mov cx, 2
.l:
        mov ax, cx
        dec ax
        cmp ax, [nclus]
        ja .d
        mov ax, cx
        call fat_next
        test ax, ax
        jnz .n
        inc bx
.n:
        inc cx
        jmp .l
.d:
        mov ax, bx
        pop cx
        pop bx
        ret

; -----------------------------------------------------------------------------
;  Directorios
; -----------------------------------------------------------------------------
; AX = clúster del directorio (0 = raíz), CX = índice -> SI = entrada en secbuf
; CF=1 si el índice sale del directorio
dir_get:
        push ax
        push bx
        push cx
        push dx
        mov [dg_idx], cx
        test ax, ax
        jnz .sub
        cmp cx, [rootent]
        jae .end
        mov ax, cx
        shr ax, 4
        add ax, [root_lba]
        jmp .read
.sub:
        mov bx, ax
        mov ax, cx
        xor dx, dx
        div word [epc]
        mov cx, ax
        mov ax, bx
        mov [dg_in], dx
.walk:
        jcxz .got
        call fat_next
        cmp ax, 0x0FF8
        jae .end
        cmp ax, 2
        jb .end
        dec cx
        jmp .walk
.got:
        call clus_lba
        mov dx, [dg_in]
        shr dx, 4
        add ax, dx
.read:
        call cache_read
        jc .end
        mov si, [dg_idx]
        and si, 15
        shl si, 5
        add si, secbuf
        pop dx
        pop cx
        pop bx
        pop ax
        clc
        ret
.end:
        pop dx
        pop cx
        pop bx
        pop ax
        stc
        ret

; AX = directorio, CX = índice inicial, SI = patrón (11), DL = atributos
; -> CF=0: CX = índice encontrado, fentry = copia de la entrada
find_entry:
        push ax
        push bx
        push si
        push di
        push es
        mov [fe_dir], ax
        mov [fe_pat], si
        push ds
        pop es
.next:
        mov ax, [fe_dir]
        call dir_get
        jc .nf
        mov bl, [si]
        test bl, bl
        jz .nf
        cmp bl, 0xE5
        je .skip
        mov bh, [si+11]
        cmp bh, 0x0F
        je .skip
        test bh, 0x08
        jz .notlab
        test dl, 0x08
        jz .skip
        jmp .cmp
.notlab:
        cmp dl, 0x08
        je .skip
        mov al, dl
        not al
        and al, bh
        test al, 0x16
        jnz .skip
.cmp:
        push cx
        push si
        mov di, [fe_pat]
        mov cx, 11
.c:
        mov al, [di]
        cmp al, '?'
        je .cok
        cmp al, [si]
        jne .bad
.cok:
        inc si
        inc di
        loop .c
        pop si
        pop cx
        mov di, fentry
        push cx
        mov cx, 16
        rep movsw
        pop cx
        pop es
        pop di
        pop si
        pop bx
        pop ax
        clc
        ret
.bad:
        pop si
        pop cx
.skip:
        inc cx
        jmp .next
.nf:
        pop es
        pop di
        pop si
        pop bx
        pop ax
        stc
        ret

; componente DS:SI -> nombre 8.3 en DS:DI (11 bytes). SI avanza.
to11:
        push ax
        push bx
        push cx
        push di
        mov bx, di
        mov cx, 11
.f:
        mov byte [di], ' '
        inc di
        loop .f
        mov di, bx
        mov cx, 8
.n:
        mov al, [si]
        test al, al
        jz .done
        cmp al, '\'
        je .done
        inc si
        cmp al, '.'
        je .ext
        cmp al, '*'
        jne .nc
.nf:
        jcxz .n
        mov byte [di], '?'
        inc di
        dec cx
        jmp .nf
.nc:
        jcxz .n
        mov [di], al
        inc di
        dec cx
        jmp .n
.ext:
        lea di, [bx+8]
        mov cx, 3
.e:
        mov al, [si]
        test al, al
        jz .done
        cmp al, '\'
        je .done
        inc si
        cmp al, '*'
        jne .ec
.ef:
        jcxz .e
        mov byte [di], '?'
        inc di
        dec cx
        jmp .ef
.ec:
        jcxz .e
        mov [di], al
        inc di
        dec cx
        jmp .e
.done:
        pop di
        pop cx
        pop bx
        pop ax
        ret

; nombre de 11 bytes DS:SI -> "NOMBRE.EXT",0 en ES:DI
from11:
        push ax
        push cx
        push si
        mov cx, 8
.n:
        lodsb
        cmp al, ' '
        je .ns
        stosb
.ns:
        loop .n
        cmp byte [si], ' '
        je .end
        mov al, '.'
        stosb
        mov cx, 3
.e:
        lodsb
        cmp al, ' '
        je .es
        stosb
.es:
        loop .e
.end:
        xor al, al
        stosb
        pop si
        pop cx
        pop ax
        ret

; ruta ES:SI del llamante -> npath absoluta (sin unidad ni "\" inicial)
; CF=1 y AX = error
normalize:
        push bx
        push cx
        push si
        push di
        mov di, tmpath
        mov cx, 127
.cp:
        mov al, [es:si]
        inc si
        cmp al, '/'
        jne .c1
        mov al, '\'
.c1:
        cmp al, 'a'
        jb .c2
        cmp al, 'z'
        ja .c2
        sub al, 32
.c2:
        mov [di], al
        inc di
        test al, al
        jz .cpd
        loop .cp
        mov byte [di], 0
.cpd:
        mov si, tmpath
        cmp byte [si], 0
        je .badpath
        cmp byte [si+1], ':'
        jne .nodrv
        cmp byte [si], 'A'
        jne .baddrv
        add si, 2
.nodrv:
        mov di, npath
        cmp byte [si], '\'
        jne .rel
        inc si
        mov byte [di], 0
        jmp .loop
.rel:
        push si
        mov si, cur_path
.rc:
        lodsb
        mov [di], al
        test al, al
        jz .rcd
        inc di
        jmp .rc
.rcd:
        pop si
.loop:
        mov al, [si]
        test al, al
        jz .ok
        cmp al, '\'
        jne .comp
        inc si
        jmp .loop
.comp:
        cmp al, '.'
        jne .app
        mov al, [si+1]
        test al, al
        jz .dot
        cmp al, '\'
        je .dot
        cmp al, '.'
        jne .app
        mov al, [si+2]
        test al, al
        jz .dotdot
        cmp al, '\'
        je .dotdot
        jmp .app
.dot:
        inc si
        jmp .loop
.dotdot:
        add si, 2
.rb:
        cmp di, npath
        je .rbd
        dec di
        cmp byte [di], '\'
        jne .rb
.rbd:
        mov byte [di], 0
        jmp .loop
.app:
        cmp di, npath
        je .ca
        mov byte [di], '\'
        inc di
.ca:
        mov al, [si]
        test al, al
        jz .ce
        cmp al, '\'
        je .ce
        mov [di], al
        inc di
        inc si
        cmp di, npath + NPATH_MAX
        jae .badpath
        jmp .ca
.ce:
        mov byte [di], 0
        jmp .loop
.ok:
        pop di
        pop si
        pop cx
        pop bx
        clc
        ret
.baddrv:
        mov ax, E_DRIVE
        jmp .err
.badpath:
        mov ax, E_NOPATH
.err:
        pop di
        pop si
        pop cx
        pop bx
        stc
        ret

; recorrer npath -> w_dir (directorio padre), w_name (último), w_has
walk:
        push bx
        push cx
        push dx
        push si
        push di
        mov word [w_dir], 0
        mov byte [w_has], 0
        mov si, npath
        cmp byte [si], 0
        je .ok
.comp:
        mov di, w_name
        call to11
        cmp byte [si], 0
        je .last
        inc si
        push si
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x16
        call find_entry
        pop si
        jc .nf
        test byte [fentry+11], 0x10
        jz .nf
        mov ax, [fentry+26]
        mov [w_dir], ax
        jmp .comp
.last:
        mov byte [w_has], 1
.ok:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        clc
        ret
.nf:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        mov ax, E_NOPATH
        stc
        ret

; ruta ES:SI -> AX = clúster del directorio (para CHDIR)
resolve_dir:
        call normalize
        jc .r
        call walk
        jc .r
        mov ax, [w_dir]
        cmp byte [w_has], 0
        je .ok
        push cx
        push dx
        push si
        mov si, w_name
        xor cx, cx
        mov dl, 0x16
        call find_entry
        pop si
        pop dx
        pop cx
        jc .nf
        test byte [fentry+11], 0x10
        jz .nf
        mov ax, [fentry+26]
.ok:
        clc
.r:
        ret
.nf:
        mov ax, E_NOPATH
        stc
        ret

; ¿w_name es un dispositivo? -> CF=1 y AL = tipo
check_device:
        push si
        push di
        push cx
        push es
        push ds
        pop es
        mov si, dev_names
.l:
        cmp byte [si], 0
        je .no
        mov di, w_name
        mov cx, 8
        push si
        repe cmpsb
        pop si
        je .yes
        add si, 9
        jmp .l
.yes:
        mov al, [si+8]
        pop es
        pop cx
        pop di
        pop si
        stc
        ret
.no:
        pop es
        pop cx
        pop di
        pop si
        clc
        ret

dev_names:
        db "CON     ", T_CON
        db "NUL     ", T_NUL
        db "AUX     ", T_NUL
        db "PRN     ", T_NUL
        db "LPT1    ", T_NUL
        db "COM1    ", T_NUL
        db "CLOCK$  ", T_NUL
        db 0

; -----------------------------------------------------------------------------
;  Ficheros: SFT y tabla de handles del PSP (JFT)
; -----------------------------------------------------------------------------
; BX = handle -> DI = SFT; CF=1 (AX=6) si no es válido
get_sft:
        cmp bx, 20
        jae .bad
        push es
        mov es, [cur_psp]
        les di, [es:0x34]
        movzx di, byte [es:di+bx]
        pop es
        cmp di, 0xFF
        je .bad
        cmp di, NSFT
        jae .bad
        shl di, 5
        add di, sft_table
        cmp byte [di+SFT_REF], 0
        je .bad
        clc
        ret
.bad:
        mov ax, E_HANDLE
        stc
        ret

; -> DI = SFT libre, AL = índice
sft_alloc:
        mov di, sft_table + 3*SFT_LEN
        mov al, 3
.l:
        cmp byte [di+SFT_REF], 0
        je .ok
        add di, SFT_LEN
        inc al
        cmp al, NSFT
        jb .l
        mov ax, E_NOHANDLES
        stc
        ret
.ok:
        push ax
        push cx
        push di
        push es
        push ds
        pop es
        mov cx, SFT_LEN
        xor al, al
        rep stosb
        pop es
        pop di
        pop cx
        pop ax
        clc
        ret

; -> BX = handle libre en el PSP actual
jft_alloc:
        push es
        push di
        mov es, [cur_psp]
        les di, [es:0x34]
        xor bx, bx
.l:
        cmp byte [es:di+bx], 0xFF
        je .ok
        inc bx
        cmp bx, 20
        jb .l
        pop di
        pop es
        mov ax, E_NOHANDLES
        stc
        ret
.ok:
        pop di
        pop es
        clc
        ret

; BX = handle, AL = índice SFT
jft_set:
        push es
        push di
        mov es, [cur_psp]
        les di, [es:0x34]
        mov [es:di+bx], al
        pop di
        pop es
        ret

; DI = SFT, ES:BX = destino, CX = bytes -> AX = leídos; CF en error de disco
file_read:
        push bx
        push cx
        push dx
        push si
        mov [fr_off], bx
        mov [fr_seg], es
        mov [fr_left], cx
        mov word [fr_done], 0
.loop:
        cmp word [fr_left], 0
        je .out
        mov eax, [di+SFT_SIZE]
        sub eax, [di+SFT_POS]
        jbe .out
        push eax
        mov eax, [di+SFT_POS]
        xor edx, edx
        movzx ecx, word [bpc]
        div ecx
        call sft_seek_clus
        jc .errp
        mov si, dx
        mov cx, si
        shr cx, 9
        call clus_lba
        add ax, cx
        call cache_read
        jc .errp
        and si, 511
        mov cx, 512
        sub cx, si
        pop eax
        movzx ecx, cx
        cmp eax, ecx
        jae .c1
        mov ecx, eax
.c1:
        cmp cx, [fr_left]
        jbe .c2
        mov cx, [fr_left]
.c2:
        push es
        push di
        les di, [fr_off]
        add si, secbuf
        push cx
        rep movsb
        pop cx
        mov [fr_off], di
        pop di
        pop es
        movzx ecx, cx
        add [di+SFT_POS], ecx
        sub [fr_left], cx
        add [fr_done], cx
        jmp .loop
.errp:
        pop eax
        mov ax, [fr_done]
        pop si
        pop dx
        pop cx
        pop bx
        stc
        ret
.out:
        mov ax, [fr_done]
        pop si
        pop dx
        pop cx
        pop bx
        clc
        ret

; DI = SFT, AX = índice de clúster dentro del fichero -> AX = clúster
sft_seek_clus:
        push bx
        push cx
        cmp ax, [di+SFT_CIX]
        jb .restart
        mov cx, [di+SFT_CCL]
        mov bx, [di+SFT_CIX]
        jmp .walk
.restart:
        mov cx, [di+SFT_CLUS]
        xor bx, bx
.walk:
        cmp cx, 2
        jb .fail
        cmp cx, 0x0FF8
        jae .fail
        cmp bx, ax
        je .done
        push ax
        mov ax, cx
        call fat_next
        mov cx, ax
        pop ax
        inc bx
        jmp .walk
.done:
        mov [di+SFT_CCL], cx
        mov [di+SFT_CIX], bx
        mov ax, cx
        pop cx
        pop bx
        clc
        ret
.fail:
        pop cx
        pop bx
        stc
        ret

; rellenar la SFT DI con fentry (índice CX en w_dir)
sft_fill_file:
        mov byte [di+SFT_REF], 1
        mov byte [di+SFT_TYPE], T_FILE
        mov al, [fentry+11]
        mov [di+SFT_ATTR], al
        mov ax, [fentry+26]
        mov [di+SFT_CLUS], ax
        mov eax, [fentry+28]
        mov [di+SFT_SIZE], eax
        mov dword [di+SFT_POS], 0
        mov word [di+SFT_CIX], 0xFFFF
        mov ax, [w_dir]
        mov [di+SFT_DIRC], ax
        mov [di+SFT_DIRI], cx
        mov ax, [fentry+22]
        mov [di+SFT_TIME], ax
        mov ax, [fentry+24]
        mov [di+SFT_DATE], ax
        ret

; abrir ES:SI -> DI = SFT rellena (no asigna handle). CF/AX
open_path:
        call normalize
        jc .r
        call walk
        jc .r
        cmp byte [w_has], 0
        je .nf
        call sft_alloc
        jc .r
        call check_device
        jnc .file
        mov byte [di+SFT_REF], 1
        mov [di+SFT_TYPE], al
        clc
        ret
.file:
        push si
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x06
        call find_entry
        pop si
        jc .nf
        call sft_fill_file
        clc
.r:
        ret
.nf:
        mov ax, E_NOFILE
        stc
        ret

; =============================================================================
;  INT 21h
; =============================================================================
int21:
        sti
        cld
        push es
        push ds
        push bp
        push di
        push si
        push dx
        push cx
        push bx
        push ax
        mov bp, sp
        mov ax, KSEG
        mov ds, ax
        inc byte [indos]
        and byte [F_FL], 0xFE
        movzx bx, byte [F_AH]
        cmp bx, MAXFN
        ja .bad
        shl bx, 1
        call [fntable+bx]
        jmp i21_ret
.bad:
        call fn_unsup
i21_ret:
        dec byte [indos]
        pop ax
        pop bx
        pop cx
        pop dx
        pop si
        pop di
        pop bp
        pop ds
        pop es
        iret

ret_err:
        mov [F_AX], ax
        mov [last_err], ax
        or byte [F_FL], 1
        ret

fn_unsup:
        mov byte [F_AL], 0
        mov ax, E_FUNC
        jmp ret_err

fn_nop:
        ret

; ---- 00h / 4Ch / 31h: terminar ----
fn_00:
        xor al, al
        xor ah, ah
        jmp do_term
fn_4c:
        mov al, [F_AL]
        xor ah, ah
        jmp do_term
fn_31:
        mov al, [F_AL]
        mov ah, 3
        jmp do_term

; ---- consola ----
fn_01:
        call con_getc
        mov [F_AL], al
        jmp con_out
fn_02:
        mov al, [F_DL]
        jmp con_out
fn_06:
        mov al, [F_DL]
        cmp al, 0xFF
        jne con_out
        call con_check
        jz .none
        call con_getc
        mov [F_AL], al
        and byte [F_FL], 0xBF
        ret
.none:
        mov byte [F_AL], 0
        or byte [F_FL], 0x40
        ret
fn_07:
fn_08:
        call con_getc
        mov [F_AL], al
        ret
fn_09:
        mov es, [F_DS]
        mov si, [F_DX]
.l:
        mov al, [es:si]
        cmp al, '$'
        je .d
        call con_out
        inc si
        jmp .l
.d:
        mov byte [F_AL], '$'
        ret
fn_0a:
        mov es, [F_DS]
        mov di, [F_DX]
        jmp line_input
fn_0b:
        call con_check
        mov byte [F_AL], 0
        jz .r
        mov byte [F_AL], 0xFF
.r:
        ret
fn_0c:
.fl:
        call con_check
        jz .e
        call con_getc
        jmp .fl
.e:
        mov al, [F_AL]
        cmp al, 0x01
        je fn_01
        cmp al, 0x06
        je .f6
        cmp al, 0x07
        je fn_07
        cmp al, 0x08
        je fn_08
        cmp al, 0x0A
        je fn_0a
        ret
.f6:
        jmp fn_06

; ---- unidades ----
fn_0e:
        mov byte [F_AL], 5
        ret
fn_19:
        mov byte [F_AL], 0
        ret
fn_1a:
        mov ax, [F_DX]
        mov [dta_off], ax
        mov ax, [F_DS]
        mov [dta_seg], ax
        ret
fn_2f:
        mov ax, [dta_off]
        mov [F_BX], ax
        mov ax, [dta_seg]
        mov [F_ES], ax
        ret
fn_36:
        mov al, [F_DL]
        cmp al, 1
        ja .bad
        call fat_free_count
        mov [F_BX], ax
        mov ax, [spcw]
        mov [F_AX], ax
        mov word [F_CX], 512
        mov ax, [nclus]
        mov [F_DX], ax
        ret
.bad:
        mov word [F_AX], 0xFFFF
        ret

; ---- vectores ----
fn_25:
        movzx bx, byte [F_AL]
        shl bx, 2
        xor ax, ax
        mov es, ax
        mov ax, [F_DX]
        mov dx, [F_DS]
        cli
        mov [es:bx], ax
        mov [es:bx+2], dx
        sti
        ret
fn_35:
        movzx bx, byte [F_AL]
        shl bx, 2
        xor ax, ax
        mov es, ax
        mov ax, [es:bx]
        mov [F_BX], ax
        mov ax, [es:bx+2]
        mov [F_ES], ax
        ret

; ---- fecha y hora (RTC por INT 1Ah) ----
bcd2bin:
        push cx
        mov cl, al
        shr al, 4
        mov ch, 10
        mul ch
        and cl, 0x0F
        add al, cl
        pop cx
        ret

fn_2a:
        push bp
        mov ah, 4
        int 0x1A
        pop bp
        jnc .ok
        mov cx, 0x2026
        mov dx, 0x0101
.ok:
        mov al, ch
        call bcd2bin
        mov bl, 100
        mul bl
        mov bx, ax
        mov al, cl
        call bcd2bin
        xor ah, ah
        add bx, ax
        mov [F_CX], bx
        mov al, dh
        call bcd2bin
        mov [F_DH], al
        mov al, dl
        call bcd2bin
        mov [F_DL], al
        ; día de la semana (Sakamoto)
        movzx cx, byte [F_DH]
        mov ax, bx
        cmp cx, 3
        jae .y
        dec ax
.y:
        mov bx, ax
        mov si, ax
        shr ax, 2
        add si, ax
        mov ax, bx
        xor dx, dx
        push cx
        mov cx, 100
        div cx
        sub si, ax
        mov ax, bx
        xor dx, dx
        mov cx, 400
        div cx
        add si, ax
        pop cx
        mov bx, cx
        dec bx
        movzx ax, byte [sak+bx]
        add si, ax
        movzx ax, byte [F_DL]
        add ax, si
        xor dx, dx
        mov cx, 7
        div cx
        mov [F_AL], dl
        ret
sak     db 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4

fn_2c:
        push bp
        mov ah, 2
        int 0x1A
        pop bp
        jnc .ok
        xor cx, cx
        xor dx, dx
.ok:
        mov al, ch
        call bcd2bin
        mov [F_CH], al
        mov al, cl
        call bcd2bin
        mov [F_CL], al
        mov al, dh
        call bcd2bin
        mov [F_DH], al
        mov byte [F_DL], 0
        ret
fn_set_ok:
        mov byte [F_AL], 0
        ret

; ---- versión y varios ----
fn_30:
        mov word [F_AX], DOS_MAJOR | (DOS_MINOR << 8)
        mov word [F_BX], 0xEE00
        mov word [F_CX], 0
        ret
fn_33:
        mov al, [F_AL]
        cmp al, 0
        je .get
        cmp al, 5
        je .boot
        cmp al, 6
        je .ver
        ret
.get:
        mov byte [F_DL], 0
        ret
.boot:
        mov byte [F_DL], 1
        ret
.ver:
        mov word [F_BX], DOS_MAJOR | (DOS_MINOR << 8)
        mov word [F_DX], 0
        ret
fn_34:
        mov word [F_ES], KSEG
        mov word [F_BX], indos
        ret
fn_37:
        cmp byte [F_AL], 0
        jne .r
        mov byte [F_DL], '/'
        mov byte [F_AL], 0
.r:
        ret
fn_38:
        mov es, [F_DS]
        mov di, [F_DX]
        mov si, country
        mov cx, 34
        rep movsb
        mov word [F_BX], 34
        mov word [F_AX], 34
        ret
country:
        dw 1                    ; fecha: dd/mm/aa
        db "EUR", 0, 0
        db ".", 0
        db ",", 0
        db "/", 0
        db ":", 0
        db 0                    ; símbolo delante
        db 2                    ; decimales
        db 0                    ; 24 h
        dd 0
        db ";", 0
        times 10 db 0

fn_54:
        mov byte [F_AL], 0
        ret
fn_58:
        mov al, [F_AL]
        test al, al
        jz .g
        cmp al, 2
        jne .r
        mov byte [F_AL], 0
.r:
        ret
.g:
        mov word [F_AX], 0
        ret
fn_59:
        mov ax, [last_err]
        mov [F_AX], ax
        mov word [F_BX], 0x0101
        mov byte [F_CH], 1
        ret
fn_62:
fn_51:
        mov ax, [cur_psp]
        mov [F_BX], ax
        ret
fn_50:
        mov ax, [F_BX]
        mov [cur_psp], ax
        ret
fn_52:
        mov word [F_ES], KSEG
        mov word [F_BX], lol
        mov ax, [first_mcb]
        mov [lol-2], ax
        ret
fn_66:
        mov word [F_BX], 437
        mov word [F_DX], 437
        ret
fn_67:
        ret

; ---- 29h: analizar nombre a FCB ----
fn_29:
        mov es, [F_DS]
        mov si, [F_SI]
        mov ax, [F_ES]
        mov gs, ax
        mov di, [F_DI]
        test byte [F_AL], 1
        jz .noskip
.sk:
        mov al, [es:si]
        cmp al, ' '
        je .sk1
        cmp al, 9
        je .sk1
        cmp al, ':'
        je .sk1
        cmp al, ';'
        je .sk1
        cmp al, ','
        je .sk1
        cmp al, '='
        je .sk1
        cmp al, '+'
        je .sk1
        jmp .noskip
.sk1:
        inc si
        jmp .sk
.noskip:
        mov byte [F_AL], 0
        test byte [F_AL+0], 0
        mov byte [gs:di], 0
        cmp byte [es:si+1], ':'
        jne .nodrv
        mov al, [es:si]
        and al, 0xDF
        sub al, 'A'-1
        mov [gs:di], al
        add si, 2
.nodrv:
        push di
        inc di
        mov cx, 8
        call .field
        pop di
        push di
        add di, 9
        cmp byte [es:si], '.'
        jne .noext
        inc si
        mov cx, 3
        call .field
        jmp .ed
.noext:
        mov cx, 3
.sp:
        mov byte [gs:di], ' '
        inc di
        loop .sp
.ed:
        pop di
        mov [F_SI], si
        ret
.field:
        mov al, [es:si]
        cmp al, 'a'
        jb .f1
        cmp al, 'z'
        ja .f1
        sub al, 32
.f1:
        cmp al, '*'
        je .star
        cmp al, '?'
        je .q
        cmp al, '!'
        jb .pad
        cmp al, '.'
        je .pad
        cmp al, '\'
        je .pad
        cmp al, ':'
        je .pad
        cmp al, '/'
        je .pad
        jcxz .skipc
        mov [gs:di], al
        inc di
        dec cx
.skipc:
        inc si
        jmp .field
.q:
        mov byte [F_AL], 1
        jcxz .skipc
        mov [gs:di], al
        inc di
        dec cx
        jmp .skipc
.star:
        mov byte [F_AL], 1
        inc si
.st:
        jcxz .field
        mov byte [gs:di], '?'
        inc di
        dec cx
        jmp .st
.pad:
        jcxz .pd
        mov byte [gs:di], ' '
        inc di
        loop .pad
.pd:
        ret

; ---- directorios ----
fn_3b:
        mov es, [F_DS]
        mov si, [F_DX]
        call resolve_dir
        jc ret_err
        mov [cur_clus], ax
        push ds
        pop es
        mov si, npath
        mov di, cur_path
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        ret
fn_47:
        mov al, [F_DL]
        cmp al, 1
        ja .bad
        mov es, [F_DS]
        mov di, [F_SI]
        mov si, cur_path
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        mov word [F_AX], 0x0100
        ret
.bad:
        mov ax, E_DRIVE
        jmp ret_err
fn_60:
        mov es, [F_DS]
        mov si, [F_SI]
        call normalize
        jc ret_err
        mov es, [F_ES]
        mov di, [F_DI]
        mov ax, ':\'
        mov byte [es:di], 'A'
        mov [es:di+1], ax
        add di, 3
        mov si, npath
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        ret

; ---- handles ----
fn_3d:
        mov es, [F_DS]
        mov si, [F_DX]
        test byte [F_AL], 3
        jz .rd
        call normalize
        jc ret_err
        call walk
        jc ret_err
        call check_device
        jc .dev
        mov ax, E_ACCESS
        jmp ret_err
.dev:
        mov es, [F_DS]
        mov si, [F_DX]
.rd:
        call open_path
        jc ret_err
        mov al, [F_AL]
        mov [di+SFT_MODE], al
        call jft_alloc
        jc .nohand
        mov ax, di
        sub ax, sft_table
        shr ax, 5
        call jft_set
        mov [F_AX], bx
        ret
.nohand:
        mov byte [di+SFT_REF], 0
        jmp ret_err

fn_3e:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        mov al, 0xFF
        call jft_set
        dec byte [di+SFT_REF]
        ret

fn_3f:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        mov al, [di+SFT_TYPE]
        cmp al, T_FILE
        je .file
        cmp al, T_CON
        je .con
        mov word [F_AX], 0
        ret
.file:
        mov es, [F_DS]
        mov bx, [F_DX]
        mov cx, [F_CX]
        call file_read
        mov [F_AX], ax
        ret
.con:
        mov es, [F_DS]
        mov bx, [F_DX]
        mov cx, [F_CX]
        call con_read
        mov [F_AX], ax
        ret

; lectura "cocinada" de la consola: una línea con CR LF
con_read:
        push bx
        push cx
        push di
        push es
        mov ax, [conq_pos]
        cmp ax, [conq_len]
        jb .have
        push es
        push ds
        pop es
        mov di, conline
        mov byte [di], 126
        call line_input
        pop es
        mov al, 10
        call con_out
        movzx ax, byte [conline+1]
        mov di, conline+2
        add di, ax
        mov byte [di], 13
        mov byte [di+1], 10
        add ax, 2
        mov [conq_len], ax
        mov word [conq_pos], 0
.have:
        pop es
        push es
        mov di, bx
        xor bx, bx
.cp:
        test cx, cx
        jz .d
        mov si, [conq_pos]
        cmp si, [conq_len]
        jae .d
        mov al, [conline+2+si]
        mov [es:di], al
        inc di
        inc word [conq_pos]
        inc bx
        dec cx
        jmp .cp
.d:
        mov ax, bx
        pop es
        pop di
        pop cx
        pop bx
        ret

fn_40:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        mov al, [di+SFT_TYPE]
        cmp al, T_CON
        je .con
        cmp al, T_NUL
        je .nul
        mov ax, E_ACCESS
        jmp ret_err
.con:
        mov es, [F_DS]
        mov si, [F_DX]
        mov cx, [F_CX]
        jcxz .nul
.l:
        mov al, [es:si]
        call con_out
        inc si
        loop .l
.nul:
        mov ax, [F_CX]
        mov [F_AX], ax
        ret

fn_42:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        cmp byte [di+SFT_TYPE], T_FILE
        jne .dev
        mov ax, [F_CX]
        shl eax, 16
        mov ax, [F_DX]
        mov bl, [F_AL]
        cmp bl, 0
        je .set
        cmp bl, 1
        je .cur
        cmp bl, 2
        je .end
        mov ax, E_FUNC
        jmp ret_err
.cur:
        add eax, [di+SFT_POS]
        jmp .set
.end:
        add eax, [di+SFT_SIZE]
.set:
        mov [di+SFT_POS], eax
        mov [F_AX], ax
        shr eax, 16
        mov [F_DX], ax
        ret
.dev:
        mov word [F_AX], 0
        mov word [F_DX], 0
        ret

fn_43:
        cmp byte [F_AL], 0
        jne .set
        mov es, [F_DS]
        mov si, [F_DX]
        call normalize
        jc ret_err
        call walk
        jc ret_err
        cmp byte [w_has], 0
        je .nf
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x16
        call find_entry
        jc .nf
        movzx ax, byte [fentry+11]
        mov [F_CX], ax
        mov [F_AX], ax
        ret
.nf:
        mov ax, E_NOFILE
        jmp ret_err
.set:
        mov ax, E_ACCESS
        jmp ret_err

fn_44:
        mov al, [F_AL]
        cmp al, 0
        je .info
        cmp al, 1
        je .ok
        cmp al, 6
        je .ready
        cmp al, 7
        je .ready
        cmp al, 8
        je .rem
        cmp al, 9
        je .remote
        jmp fn_unsup
.info:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        mov al, [di+SFT_TYPE]
        cmp al, T_FILE
        je .finfo
        cmp al, T_CON
        je .cinfo
        mov word [F_DX], 0x8084
        mov word [F_AX], 0x8084
        ret
.cinfo:
        mov word [F_DX], 0x80D3
        mov word [F_AX], 0x80D3
        ret
.finfo:
        mov word [F_DX], 0x0000
        mov word [F_AX], 0x0000
        ret
.ok:
        ret
.ready:
        mov byte [F_AL], 0xFF
        ret
.rem:
        mov word [F_AX], 0
        ret
.remote:
        mov word [F_DX], 0
        ret

fn_45:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        call jft_alloc
        jc ret_err
        mov ax, di
        sub ax, sft_table
        shr ax, 5
        call jft_set
        inc byte [di+SFT_REF]
        mov [F_AX], bx
        ret
fn_46:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        push di
        mov bx, [F_CX]
        cmp bx, 20
        jae .bad
        call get_sft
        jc .free
        dec byte [di+SFT_REF]
.free:
        pop di
        mov ax, di
        sub ax, sft_table
        shr ax, 5
        mov bx, [F_CX]
        call jft_set
        inc byte [di+SFT_REF]
        ret
.bad:
        pop di
        mov ax, E_HANDLE
        jmp ret_err

fn_57:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        cmp byte [F_AL], 0
        jne .r
        mov ax, [di+SFT_TIME]
        mov [F_CX], ax
        mov ax, [di+SFT_DATE]
        mov [F_DX], ax
.r:
        ret

fn_6c:
        mov al, [F_DL]
        and al, 0x0F
        cmp al, 1
        jne .acc
        mov ax, [F_SI]
        mov [F_DX], ax
        mov al, [F_BL]
        mov [F_AL], al
        call fn_3d
        test byte [F_FL], 1
        jnz .r
        mov word [F_CX], 1
.r:
        ret
.acc:
        mov ax, E_ACCESS
        jmp ret_err

fn_rdonly:
        mov ax, E_ACCESS
        jmp ret_err

; ---- memoria ----
fn_48:
        mov bx, [F_BX]
        call mcb_alloc
        jc .err
        mov [F_AX], ax
        ret
.err:
        mov [F_BX], bx
        jmp ret_err
fn_49:
        mov es, [F_ES]
        call mcb_free
        jc ret_err
        ret
fn_4a:
        mov es, [F_ES]
        mov bx, [F_BX]
        call mcb_resize
        jc .err
        ret
.err:
        mov [F_BX], bx
        jmp ret_err

; ---- 4Dh: código de retorno ----
fn_4d:
        mov ax, [last_ret]
        mov [F_AX], ax
        mov word [last_ret], 0
        ret

; ---- buscar ficheros ----
fn_4e:
        mov es, [F_DS]
        mov si, [F_DX]
        call normalize
        jc ret_err
        call walk
        jc ret_err
        les di, [dta_off]
        mov byte [es:di], 1
        cmp byte [w_has], 0
        jne .pat
        mov si, all11
        jmp .cpp
.pat:
        mov si, w_name
.cpp:
        push di
        inc di
        mov cx, 11
        rep movsb
        pop di
        mov al, [F_CL]
        mov [es:di+12], al
        mov word [es:di+13], 0
        mov ax, [w_dir]
        mov [es:di+15], ax
        mov word [es:di+17], 0x444E
        call find_step
        jnc .r
        mov ax, E_NOFILE
        jmp ret_err
.r:
        ret
fn_4f:
        les di, [dta_off]
        cmp word [es:di+17], 0x444E
        jne .nm
        call find_step
        jnc .r
.nm:
        mov ax, E_NOMORE
        jmp ret_err
.r:
        ret

; DTA en ES:DI -> busca desde el índice guardado y rellena el resultado
find_step:
        push es
        push di
        lea si, [di+1]
        push ds
        mov ax, es
        mov ds, ax
        mov bx, pat_tmp
        push es
        mov ax, KSEG
        mov es, ax
        mov di, bx
        mov cx, 11
        rep movsb
        pop es
        pop ds
        pop di
        push di
        mov ax, [es:di+15]
        mov cx, [es:di+13]
        mov dl, [es:di+12]
        mov si, pat_tmp
        call find_entry
        pop di
        pop es
        jc .nf
        inc cx
        mov [es:di+13], cx
        mov al, [fentry+11]
        mov [es:di+0x15], al
        mov ax, [fentry+22]
        mov [es:di+0x16], ax
        mov ax, [fentry+24]
        mov [es:di+0x18], ax
        mov eax, [fentry+28]
        mov [es:di+0x1A], eax
        push di
        add di, 0x1E
        mov si, fentry
        call from11
        pop di
        clc
        ret
.nf:
        stc
        ret

; ---- 4Bh: cargar y ejecutar ----
fn_4b:
        cmp byte [F_AL], 0
        je .go
        jmp fn_unsup
.go:
        mov ax, [F_BX]
        mov [ex_pb], ax
        mov ax, [F_ES]
        mov [ex_pb+2], ax
        mov es, [F_DS]
        mov si, [F_DX]
        call normalize
        jc ret_err
        ; ruta completa del programa para el entorno
        push ds
        pop es
        mov di, ex_path
        mov ax, ':A'
        xchg al, ah
        stosw
        mov al, '\'
        stosb
        mov si, npath
.cpn:
        lodsb
        stosb
        test al, al
        jnz .cpn
        mov ax, di
        sub ax, ex_path
        mov [ex_plen], ax
        call walk
        jc ret_err
        cmp byte [w_has], 0
        je .nf
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x06
        call find_entry
        jc .nf
        mov di, ex_sft
        call sft_fill_file
        ; cabecera
        push ds
        pop es
        mov bx, ex_hdr
        mov cx, 28
        call file_read
        mov word [ex_isexe], 0
        cmp ax, 28
        jb .com
        mov ax, [ex_hdr]
        cmp ax, 'MZ'
        je .exe
        cmp ax, 'ZM'
        je .exe
        jmp .com
.exe:
        mov word [ex_isexe], 1
.com:
        call build_env
        jc ret_err
        mov [ex_env], ax
        cmp word [ex_isexe], 0
        jne .exe_alloc
        ; .COM: toda la memoria disponible
        mov bx, 0xFFFF
        call mcb_alloc
        mov eax, [ex_sft+SFT_SIZE]
        cmp eax, 0xFF00
        ja .toobig
        add ax, 0x100 + 0x100 + 15
        shr ax, 4
        cmp bx, ax
        jb .nomem
        call mcb_alloc
        jc .nomem
        mov [ex_psp], ax
        mov [ex_size], bx
        jmp .allocd
.toobig:
        mov ax, E_FORMAT
        jmp .fail_env
.exe_alloc:
        ; tamaño de la imagen
        movzx eax, word [ex_hdr+4]
        shl eax, 9
        movzx ecx, word [ex_hdr+2]
        test ecx, ecx
        jz .full
        sub eax, 512
        add eax, ecx
.full:
        movzx ecx, word [ex_hdr+8]
        shl ecx, 4
        sub eax, ecx
        jb .bad_exe
        mov [ex_img], eax
        add eax, 15
        shr eax, 4
        add eax, 0x10
        mov [ex_need], eax
        movzx ecx, word [ex_hdr+0x0A]
        add eax, ecx
        cmp eax, 0xFFFF
        ja .nomem_e
        mov [ex_min], ax
        mov eax, [ex_need]
        movzx ecx, word [ex_hdr+0x0C]
        add eax, ecx
        cmp eax, 0xFFFF
        jbe .mx
        mov eax, 0xFFFF
.mx:
        mov bx, ax
        call mcb_alloc
        jnc .gotexe
        cmp bx, [ex_min]
        jb .nomem
        call mcb_alloc
        jc .nomem
.gotexe:
        mov [ex_psp], ax
        mov [ex_size], bx
        jmp .allocd
.bad_exe:
        mov ax, E_FORMAT
        jmp .fail_env
.nomem_e:
.nomem:
        mov ax, E_NOMEM
.fail_env:
        push ax
        mov es, [ex_env]
        call mcb_free
        pop ax
        jmp ret_err
.allocd:
        ; dueños: el nuevo PSP
        mov ax, [ex_psp]
        mov bx, ax
        dec bx
        mov es, bx
        mov [es:1], ax
        mov bx, [ex_env]
        dec bx
        mov es, bx
        mov [es:1], ax
        call build_psp
        ; cargar la imagen
        cmp word [ex_isexe], 0
        jne .load_exe
        mov dword [ex_sft+SFT_POS], 0
        mov es, [ex_psp]
        mov bx, 0x100
        mov cx, [ex_sft+SFT_SIZE]
        mov di, ex_sft
        call file_read
        jc .ioerr
        mov ax, [ex_psp]
        mov [ex_cs], ax
        mov [ex_ss], ax
        mov word [ex_ip], 0x100
        mov ax, 0xFFFE
        cmp word [ex_size], 0x1000
        jae .sp
        mov ax, [ex_size]
        shl ax, 4
        sub ax, 2
.sp:
        mov [ex_sp], ax
        mov es, [ex_psp]
        mov bx, ax
        mov word [es:bx], 0
        jmp .launch
.load_exe:
        movzx eax, word [ex_hdr+8]
        shl eax, 4
        mov [ex_sft+SFT_POS], eax
        mov ax, [ex_psp]
        add ax, 0x10
        mov [ex_load], ax
        mov [ex_cur], ax
.chunk:
        mov eax, [ex_img]
        test eax, eax
        jz .reloc
        cmp eax, 0x8000
        jbe .last
        mov eax, 0x8000
.last:
        mov cx, ax
        sub [ex_img], eax
        mov es, [ex_cur]
        xor bx, bx
        mov di, ex_sft
        call file_read
        jc .ioerr
        add word [ex_cur], 0x800
        jmp .chunk
.reloc:
        movzx eax, word [ex_hdr+0x18]
        mov [ex_sft+SFT_POS], eax
        mov cx, [ex_hdr+6]
.rl:
        jcxz .rdone
        push cx
        push ds
        pop es
        mov bx, ex_rel
        mov cx, 4
        mov di, ex_sft
        call file_read
        pop cx
        jc .ioerr
        mov ax, [ex_rel+2]
        add ax, [ex_load]
        mov es, ax
        mov bx, [ex_rel]
        mov ax, [ex_load]
        add [es:bx], ax
        dec cx
        jmp .rl
.rdone:
        mov ax, [ex_hdr+0x16]
        add ax, [ex_load]
        mov [ex_cs], ax
        mov ax, [ex_hdr+0x14]
        mov [ex_ip], ax
        mov ax, [ex_hdr+0x0E]
        add ax, [ex_load]
        mov [ex_ss], ax
        mov ax, [ex_hdr+0x10]
        mov [ex_sp], ax
.launch:
        ; guardar la pila del padre y su DTA
        mov es, [cur_psp]
        mov [es:0x2E], bp
        mov [es:0x30], ss
        mov es, [ex_psp]
        mov ax, [dta_off]
        mov [es:0x3C], ax
        mov ax, [dta_seg]
        mov [es:0x3E], ax
        mov ax, [ex_psp]
        mov [cur_psp], ax
        mov [dta_seg], ax
        mov word [dta_off], 0x80
        dec byte [indos]
        mov dx, [ex_psp]
        mov ax, [ex_ss]
        mov bx, [ex_sp]
        cli
        mov ss, ax
        mov sp, bx
        sti
        push word [ex_cs]
        push word [ex_ip]
        mov es, dx
        xor ax, ax
        xor bx, bx
        xor cx, cx
        xor si, si
        xor di, di
        xor bp, bp
        mov ds, dx
        retf
.ioerr:
        mov ax, [ex_psp]
        call mcb_free_owner
        mov ax, E_FORMAT
        jmp ret_err
.nf:
        mov ax, E_NOFILE
        jmp ret_err

; entorno para el hijo -> AX = segmento
build_env:
        push es
        les si, [ex_pb]
        mov ax, [es:si]
        test ax, ax
        jnz .src
        mov es, [cur_psp]
        mov ax, [es:0x2C]
.src:
        mov [ex_esrc], ax
        xor cx, cx
        test ax, ax
        jz .len0
        mov es, ax
        xor si, si
.s:
        cmp byte [es:si], 0
        je .se
.ss:
        inc si
        cmp si, 0x7F00
        jae .se
        cmp byte [es:si-1], 0
        jne .ss
        jmp .s
.se:
        mov cx, si
.len0:
        mov [ex_elen], cx
        mov bx, cx
        add bx, 3
        add bx, [ex_plen]
        add bx, 15
        shr bx, 4
        call mcb_alloc
        jc .err
        mov es, ax
        xor di, di
        mov cx, [ex_elen]
        push ds
        mov si, [ex_esrc]
        test si, si
        jz .noc
        mov ds, si
        xor si, si
        rep movsb
.noc:
        pop ds
        xor al, al
        stosb
        mov ax, 1
        stosw
        mov si, ex_path
        mov cx, [ex_plen]
        rep movsb
        mov ax, es
        pop es
        clc
        ret
.err:
        pop es
        stc
        ret

; construir el PSP en ex_psp (tamaño ex_size)
build_psp:
        mov es, [ex_psp]
        xor di, di
        xor ax, ax
        mov cx, 128
        rep stosw
        mov word [es:0], 0x20CD
        mov ax, [ex_psp]
        add ax, [ex_size]
        mov [es:2], ax
        push ds
        xor ax, ax
        mov ds, ax
        mov eax, [ds:0x22*4]
        mov [es:0x0A], eax
        mov eax, [ds:0x23*4]
        mov [es:0x0E], eax
        mov eax, [ds:0x24*4]
        mov [es:0x12], eax
        pop ds
        mov ax, [cur_psp]
        mov [es:0x16], ax
        ; heredar handles
        push ds
        mov ds, ax
        lds si, [ds:0x34]
        mov di, 0x18
        mov cx, 20
        rep movsb
        pop ds
        mov di, 0x18
        mov cx, 20
.inh:
        movzx bx, byte [es:di]
        cmp bx, NSFT
        jae .ni
        shl bx, 5
        inc byte [sft_table+bx+SFT_REF]
.ni:
        inc di
        loop .inh
        mov ax, [ex_env]
        mov [es:0x2C], ax
        mov word [es:0x32], 20
        mov word [es:0x34], 0x18
        mov [es:0x36], es
        mov dword [es:0x38], 0xFFFFFFFF
        mov word [es:0x50], 0x21CD
        mov byte [es:0x52], 0xCB
        ; FCB y cola de órdenes desde el bloque de parámetros
        push ds
        lds si, [ex_pb]
        mov bx, si
        lds si, [bx+6]
        mov ax, ds
        test ax, ax
        jz .nf1
        mov di, 0x5C
        mov cx, 16
        rep movsb
.nf1:
        pop ds
        push ds
        lds si, [ex_pb]
        mov bx, si
        lds si, [bx+10]
        mov ax, ds
        test ax, ax
        jz .nf2
        mov di, 0x6C
        mov cx, 16
        rep movsb
.nf2:
        pop ds
        push ds
        lds si, [ex_pb]
        mov bx, si
        lds si, [bx+2]
        mov ax, ds
        test ax, ax
        jz .nt
        movzx cx, byte [si]
        cmp cx, 126
        jbe .tl
        mov cx, 126
.tl:
        mov di, 0x80
        mov [es:di], cl
        inc si
        inc di
        rep movsb
        mov byte [es:di], 13
        jmp .td
.nt:
        mov byte [es:0x81], 13
.td:
        pop ds
        ret

; ---- terminar el programa actual (AL = código, AH = tipo) ----
do_term:
        mov [last_ret], ax
        mov bx, [cur_psp]
        mov es, bx
        mov cx, [es:0x16]
        cmp cx, bx
        je .root
        cmp ah, 3
        jne .free
        mov bx, [F_DX]
        cmp bx, 6
        jae .tsz
        mov bx, 6
.tsz:
        call mcb_resize
        jmp .rest
.free:
        ; cerrar sus handles
        push cx
        mov cx, 20
        les di, [es:0x34]
.cl:
        movzx bx, byte [es:di]
        cmp bx, NSFT
        jae .cn
        shl bx, 5
        cmp byte [sft_table+bx+SFT_REF], 0
        je .cn
        dec byte [sft_table+bx+SFT_REF]
.cn:
        mov byte [es:di], 0xFF
        inc di
        loop .cl
        pop cx
        mov ax, [cur_psp]
        call mcb_free_owner
.rest:
        mov es, [cur_psp]
        ; restaurar INT 22h-24h y la DTA del padre
        push ds
        xor ax, ax
        mov ds, ax
        mov eax, [es:0x0E]
        mov [ds:0x23*4], eax
        mov eax, [es:0x12]
        mov [ds:0x24*4], eax
        pop ds
        mov ax, [es:0x3C]
        mov [dta_off], ax
        mov ax, [es:0x3E]
        mov [dta_seg], ax
        mov [cur_psp], cx
        mov word [conq_len], 0
        mov word [conq_pos], 0
        mov es, cx
        cli
        mov ss, [es:0x32-2]
        mov sp, [es:0x2E]
        sti
        mov bp, sp
        and byte [F_FL], 0xFE
        jmp i21_ret
.root:
        mov si, msg_rootterm
        call kputs
        jmp halt

; =============================================================================
;  Otras interrupciones
; =============================================================================
int20:
        mov ax, 0x4C00
        jmp int21
int27:
        add dx, 15
        shr dx, 4
        mov ax, 0x3100
        jmp int21
int22:
        jmp int_iret
int23:
        mov ax, 0x4C03
        jmp int21
int24:
        mov al, 3
        iret
int25:
        stc
        retf 2
int29:
        push ds
        push ax
        mov ax, KSEG
        mov ds, ax
        pop ax
        call con_out
        pop ds
        iret
int2f:
int_iret:
        iret

; =============================================================================
;  Datos
; =============================================================================
MAXFN   equ 0x6C
fntable:
        dw fn_00, fn_01, fn_02, fn_unsup, fn_unsup, fn_unsup, fn_06, fn_07  ; 00
        dw fn_08, fn_09, fn_0a, fn_0b, fn_0c, fn_nop, fn_0e, fn_unsup       ; 08
        dw fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 10
        dw fn_unsup, fn_19, fn_1a, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 18
        dw fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_25, fn_unsup, fn_unsup ; 20
        dw fn_unsup, fn_29, fn_2a, fn_set_ok, fn_2c, fn_set_ok, fn_nop, fn_2f ; 28
        dw fn_30, fn_31, fn_unsup, fn_33, fn_34, fn_35, fn_36, fn_37     ; 30
        dw fn_38, fn_rdonly, fn_rdonly, fn_3b, fn_rdonly, fn_3d, fn_3e, fn_3f ; 38
        dw fn_40, fn_rdonly, fn_42, fn_43, fn_44, fn_45, fn_46, fn_47       ; 40
        dw fn_48, fn_49, fn_4a, fn_4b, fn_4c, fn_4d, fn_4e, fn_4f           ; 48
        dw fn_50, fn_51, fn_52, fn_unsup, fn_54, fn_unsup, fn_rdonly, fn_57 ; 50
        dw fn_58, fn_59, fn_rdonly, fn_rdonly, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 58
        dw fn_60, fn_unsup, fn_62, fn_unsup, fn_unsup, fn_unsup, fn_66, fn_67 ; 60
        dw fn_nop, fn_unsup, fn_unsup, fn_unsup, fn_6c                       ; 68

all11       db "???????????"
shell_path  db "A:\COMMAND.COM", 0
shell_tail  db 0, 13
shell_fcb   times 16 db 0
shell_pb    dw 0
            dw shell_tail, KSEG
            dw shell_fcb, KSEG
            dw shell_fcb, KSEG

msg_banner  db 13, 10, "HispaDOS 0.1 - MS-DOS 6.22 compatible kernel", 13, 10
            db "(C) 2026 HispaDOS contributors - GPL-2.0-or-later", 13, 10, 13, 10, 0
msg_fatbad  db "Disk error reading FAT", 13, 10, 0
msg_noshell db 13, 10, "Cannot load A:\COMMAND.COM - system halted", 13, 10, 0
msg_reshell db 13, 10, "Restarting command interpreter...", 13, 10, 0
msg_rootterm db 13, 10, "Root process terminated - system halted", 13, 10, 0

; lista de listas (INT 21h AH=52h); la palabra anterior es el primer MCB
            dw 0
lol:        times 0x40 db 0
lol_end:

indos       db 0
            db 0
cur_path    db 0
            times NPATH_MAX+2 db 0

; =============================================================================
        section .bss
bootsec     resb 512
boot_drive  resb 1
ser_ok      resb 1
ser_last    resb 1
col         resb 1
pend_scan   resb 1
peek_ok     resb 1
peek_ch     resb 1
            resb 1
bps         resw 1
spcw        resw 1
epc         resw 1
bpc         resw 1
fat_lba     resw 1
nfats       resw 1
spf         resw 1
rootent     resw 1
root_lba    resw 1
root_secs   resw 1
data_lba    resw 1
nclus       resw 1
spt         resw 1
heads       resw 1
cache_lba   resw 1
first_mcb   resw 1
mem_top     resw 1
cur_psp     resw 1
root_psp    resw 1
dta_off     resw 1
dta_seg     resw 1
last_err    resw 1
last_ret    resw 1
cur_clus    resw 1
dg_idx      resw 1
dg_in       resw 1
fe_dir      resw 1
fe_pat      resw 1
w_dir       resw 1
w_has       resb 1
w_name      resb 12
fentry      resb 32
pat_tmp     resb 12
fr_off      resw 1
fr_seg      resw 1
fr_left     resw 1
fr_done     resw 1
conq_len    resw 1
conq_pos    resw 1
conline     resb 132
tmpath      resb 130
npath       resb NPATH_MAX+4
ex_pb       resd 1
ex_path     resb NPATH_MAX+8
ex_plen     resw 1
ex_hdr      resb 28
ex_isexe    resw 1
ex_env      resw 1
ex_esrc     resw 1
ex_elen     resw 1
ex_psp      resw 1
ex_size     resw 1
ex_img      resd 1
ex_need     resd 1
ex_min      resw 1
ex_load     resw 1
ex_cur      resw 1
ex_cs       resw 1
ex_ip       resw 1
ex_ss       resw 1
ex_sp       resw 1
ex_rel      resd 1
            alignb 16
ex_sft      resb SFT_LEN
sft_table   resb SFT_LEN*NSFT
            alignb 16
secbuf      resb 512
fatbuf      resb 512*12
            alignb 2
kstack      resb 1024
kstack_top:
            alignb 16
kernel_end:
