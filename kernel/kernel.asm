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
SFT_DRV         equ 26
SFT_FLAGS       equ 27
E_EXISTS        equ 80
E_CURDIR        equ 16
E_NOTSAME       equ 17
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
PATHLEN         equ 68

; --- unidades (A:, B:, C:...) ---
D_FLAGS         equ 0
D_BIOS          equ 1
D_PTYPE         equ 2
D_MEDIA         equ 3
D_START         equ 4
D_SIZE          equ 8
D_SPT           equ 12
D_HEADS         equ 14
D_SPCW          equ 16
D_EPC           equ 18
D_BPC           equ 20
D_FATLBA        equ 22
D_NFATS         equ 26
D_SPF           equ 28
D_ROOTENT       equ 30
D_ROOTLBA       equ 32
D_ROOTSECS      equ 36
D_DATALBA       equ 38
D_NCLUS         equ 42
D_HINT          equ 44
DRV_LEN         equ 64
NDRV            equ 8

DF_EXISTS       equ 1
DF_MOUNTED      equ 2
DF_LBA          equ 4
DF_FIXED        equ 8
DF_FAT16        equ 16

%define spcw    cur_drv+D_SPCW
%define epc     cur_drv+D_EPC
%define bpc     cur_drv+D_BPC
%define nclus   cur_drv+D_NCLUS
%define rootent cur_drv+D_ROOTENT

; =============================================================================
        section .text
start:
        cli
        cld
        mov ax, KSEG
        mov es, ax
        ; poner a cero la zona .bss (la BIOS no la deja limpia)
        push ax
        mov di, bss_start
        mov cx, (kernel_end - bss_start) / 2
        xor ax, ax
        rep stosw
        pop ax
        mov di, bootsec
        mov cx, 256
        rep movsw
        mov ds, ax
        mov ss, ax
        mov sp, kstack_top
        sti
        mov [boot_drive], dl

        call serial_init
        mov si, msg_banner
        call kputs
        call drives_scan
        call boot_select
        jnc .bootok
        mov si, msg_fatbad
        call kputs
        jmp halt
.bootok:
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
;  Unidades: disquetes (A:, B:) y particiones FAT de los discos duros (C:...)
; -----------------------------------------------------------------------------
; DL = unidad BIOS -> AX = cilindros, CX = sectores/pista, DX = cabezas; CF
bios_geom:
        push bx
        push di
        push es
        push bp
        mov ah, 8
        xor di, di
        mov es, di
        int 0x13
        pop bp
        pop es
        pop di
        pop bx
        jc .r
        mov [bg_count], dl
        movzx ax, cl
        and al, 0xC0
        shl ax, 2
        mov al, ch
        inc ax
        and cx, 0x3F
        movzx dx, dh
        inc dx
        clc
.r:
        ret

; DL = unidad BIOS: ¿admite las extensiones LBA de INT 13h?
check_ext:
        push ax
        push bx
        push cx
        push dx
        mov ah, 0x41
        mov bx, 0x55AA
        push bp
        int 0x13
        pop bp
        jc .no
        cmp bx, 0xAA55
        jne .no
        test cl, 1
        jz .no
        or byte [cur_drv+D_FLAGS], DF_LBA
.no:
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; recorrer disquetes y tablas de particiones -> drv_tab, ndrives
drives_scan:
        pushad
        push es
        push ds
        pop es
        mov di, drv_tab
        mov cx, NDRV*DRV_LEN/2
        xor ax, ax
        rep stosw
        mov dword [cache_lba], 0xFFFFFFFF
        mov dword [fatc_lba], 0xFFFFFFFF
        mov byte [ndrives], 2
        ; disquetes según la lista de equipo de la BIOS
        int 0x11
        test al, 1
        jz .hdd
        shr al, 6
        and al, 3
        inc al
        movzx si, al
        cmp si, 2
        jbe .fl
        mov si, 2
.fl:
        xor dl, dl
.flop:
        movzx bx, dl
        shl bx, 6
        add bx, drv_tab
        mov byte [bx+D_FLAGS], DF_EXISTS
        mov [bx+D_BIOS], dl
        mov word [bx+D_SPT], 18
        mov word [bx+D_HEADS], 2
        mov dword [bx+D_SIZE], 2880
        push dx
        call bios_geom
        jc .fdef
        test cx, cx
        jz .fdef
        mov [bx+D_SPT], cx
        mov [bx+D_HEADS], dx
        movzx eax, ax
        movzx ecx, cx
        imul eax, ecx
        movzx edx, dx
        imul eax, edx
        mov [bx+D_SIZE], eax
.fdef:
        pop dx
        inc dl
        dec si
        jnz .flop
.hdd:
        mov dl, 0x80
        call bios_geom
        jc .done
        mov al, [bg_count]
        test al, al
        jz .done
        mov [hd_count], al
        mov byte [hd_cur], 0x80
.disk:
        mov dl, [hd_cur]
        call bios_geom
        jc .nextdisk
        push ax
        push cx
        mov di, cur_drv
        mov cx, DRV_LEN/2
        xor ax, ax
        rep stosw
        pop cx
        pop ax
        mov bl, [hd_cur]
        mov [cur_drv+D_BIOS], bl
        mov [cur_drv+D_SPT], cx
        mov [cur_drv+D_HEADS], dx
        mov byte [cur_drv+D_FLAGS], DF_EXISTS | DF_FIXED
        movzx eax, ax
        movzx ecx, cx
        imul eax, ecx
        movzx edx, dx
        imul eax, edx
        mov [cur_drv+D_SIZE], eax
        mov dl, [hd_cur]
        call check_ext
        ; leer el MBR
        xor eax, eax
        mov bx, secbuf
        call read_sector
        mov dword [cache_lba], 0xFFFFFFFF
        jc .nextdisk
        cmp word [secbuf+510], 0xAA55
        jne .nextdisk
        mov si, secbuf+0x1BE
        mov cx, 4
.part:
        mov al, [si+4]
        cmp al, 0x01
        je .add
        cmp al, 0x04
        je .add
        cmp al, 0x06
        je .add
        cmp al, 0x0E
        je .add
        jmp .np
.add:
        movzx bx, byte [ndrives]
        cmp bx, NDRV
        jae .np
        shl bx, 6
        add bx, drv_tab
        push si
        push cx
        mov si, cur_drv
        mov di, bx
        mov cx, DRV_LEN/2
        rep movsw
        pop cx
        pop si
        mov [bx+D_PTYPE], al
        mov eax, [si+8]
        mov [bx+D_START], eax
        mov eax, [si+12]
        mov [bx+D_SIZE], eax
        inc byte [ndrives]
.np:
        add si, 16
        loop .part
.nextdisk:
        inc byte [hd_cur]
        dec byte [hd_count]
        jnz .disk
.done:
        pop es
        popad
        ret

; elegir la unidad de arranque como unidad actual
boot_select:
        mov al, [boot_drive]
        cmp al, 0x80
        jae .hd
        movzx bx, al
        cmp bx, 1
        jbe .fl
        xor bx, bx
.fl:
        mov ax, bx
        shl bx, 6
        add bx, drv_tab
        test byte [bx+D_FLAGS], DF_EXISTS
        jnz .set
        mov byte [bx+D_FLAGS], DF_EXISTS
        mov [bx+D_BIOS], al
        mov word [bx+D_SPT], 18
        mov word [bx+D_HEADS], 2
        mov dword [bx+D_SIZE], 2880
        jmp .set
.hd:
        mov ecx, [bootsec+0x1C]
        mov bx, drv_tab + 2*DRV_LEN
        mov ah, 2
.find:
        cmp ah, [ndrives]
        jae .first
        cmp [bx+D_BIOS], al
        jne .nx
        cmp [bx+D_START], ecx
        je .found
.nx:
        add bx, DRV_LEN
        inc ah
        jmp .find
.first:
        mov ah, 2
.found:
        mov al, ah
.set:
        mov [cur_drive], al
        add al, 'A'
        mov [shell_path], al
        mov [env_l1], al
        mov [env_l2], al
        sub al, 'A'
        jmp select_drive

; AL = unidad -> cur_drv = copia de su entrada (sin montarla); CF si no existe
select_raw:
        cmp al, NDRV
        jae .bad
        push cx
        push si
        push di
        push es
        movzx si, al
        shl si, 6
        add si, drv_tab
        test byte [si+D_FLAGS], DF_EXISTS
        jz .badp
        push ds
        pop es
        mov di, cur_drv
        mov cx, DRV_LEN/2
        rep movsw
        mov [sel_drv], al
        pop es
        pop di
        pop si
        pop cx
        clc
        ret
.badp:
        pop es
        pop di
        pop si
        pop cx
.bad:
        stc
        ret

; AL = unidad -> seleccionada y montada; CF si no tiene sistema de ficheros
select_drive:
        call select_raw
        jc .r
        test byte [cur_drv+D_FLAGS], DF_MOUNTED
        jnz .ok
        call mount
        jc .r
.ok:
        clc
.r:
        ret

; guardar cur_drv en su entrada de la tabla
drv_store:
        push cx
        push si
        push di
        push es
        push ds
        pop es
        movzx di, byte [sel_drv]
        shl di, 6
        add di, drv_tab
        mov si, cur_drv
        mov cx, DRV_LEN/2
        rep movsw
        pop es
        pop di
        pop si
        pop cx
        ret

; leer el BPB de la unidad seleccionada y calcular su geometría FAT
mount:
        pushad
        push es
        push ds
        pop es
        xor eax, eax
        mov bx, secbuf
        call read_sector
        mov dword [cache_lba], 0xFFFFFFFF
        jc .bad
        cmp word [secbuf+0x0B], 512
        jne .bad
        movzx ax, byte [secbuf+0x0D]
        test ax, ax
        jz .bad
        mov cx, ax
        dec cx
        test cx, ax
        jnz .bad
        mov [cur_drv+D_SPCW], ax
        mov cx, ax
        shl cx, 4
        mov [cur_drv+D_EPC], cx
        shl cx, 5
        mov [cur_drv+D_BPC], cx
        movzx eax, word [secbuf+0x0E]
        test eax, eax
        jz .bad
        mov [cur_drv+D_FATLBA], eax
        movzx ecx, byte [secbuf+0x10]
        test ecx, ecx
        jz .bad
        cmp ecx, 2
        ja .bad
        mov [cur_drv+D_NFATS], cx
        movzx edx, word [secbuf+0x16]
        test edx, edx
        jz .bad
        mov [cur_drv+D_SPF], dx
        imul edx, ecx
        add eax, edx
        mov [cur_drv+D_ROOTLBA], eax
        movzx ecx, word [secbuf+0x11]
        test ecx, ecx
        jz .bad
        mov [cur_drv+D_ROOTENT], cx
        add ecx, 15
        shr ecx, 4
        mov [cur_drv+D_ROOTSECS], cx
        add eax, ecx
        mov [cur_drv+D_DATALBA], eax
        movzx edx, word [secbuf+0x13]
        test edx, edx
        jnz .ts
        mov edx, [secbuf+0x20]
.ts:
        sub edx, eax
        jbe .bad
        mov eax, edx
        xor edx, edx
        movzx ecx, word [cur_drv+D_SPCW]
        div ecx
        test eax, eax
        jz .bad
        cmp eax, 65524
        ja .bad
        mov [cur_drv+D_NCLUS], ax
        and byte [cur_drv+D_FLAGS], 0xFF ^ DF_FAT16
        cmp eax, 4085
        jb .f12
        or byte [cur_drv+D_FLAGS], DF_FAT16
.f12:
        mov al, [secbuf+0x15]
        mov [cur_drv+D_MEDIA], al
        test byte [cur_drv+D_FLAGS], DF_FIXED
        jnz .fixed
        mov ax, [secbuf+0x18]
        test ax, ax
        jz .fixed
        mov [cur_drv+D_SPT], ax
        mov ax, [secbuf+0x1A]
        mov [cur_drv+D_HEADS], ax
.fixed:
        or byte [cur_drv+D_FLAGS], DF_MOUNTED
        call drv_store
        pop es
        popad
        clc
        ret
.bad:
        pop es
        popad
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
        dw int26
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
        db "COMSPEC="
env_l1: db "A:\COMMAND.COM", 0
        db "PATH="
env_l2: db "A:\", 0
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
        cmp al, 0x1A
        je .store
        cmp al, 32
        jb .l
.store:
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
;  Disco: lectura y escritura de sectores por BIOS INT 13h (CHS o LBA)
; -----------------------------------------------------------------------------
; EAX = sector relativo a la unidad seleccionada, ES:BX = búfer -> CF
read_sector:
        mov byte [io_cmd], 2
        jmp disk_io
write_sector:
        mov byte [io_cmd], 3
disk_io:
        pushad
        add eax, [cur_drv+D_START]
        mov di, 3
.retry:
        test byte [cur_drv+D_FLAGS], DF_LBA
        jz .chs
        mov byte [dap], 16
        mov byte [dap+1], 0
        mov word [dap+2], 1
        mov [dap+4], bx
        mov [dap+6], es
        mov [dap+8], eax
        mov dword [dap+12], 0
        push eax
        mov ah, [io_cmd]
        or ah, 0x40
        xor al, al
        mov dl, [cur_drv+D_BIOS]
        mov si, dap
        push bp
        int 0x13
        pop bp
        pop eax
        jnc .ok
        jmp .err
.chs:
        push eax
        xor edx, edx
        movzx ecx, word [cur_drv+D_SPT]
        jecxz .chsbad
        div ecx
        inc dx
        mov cx, dx
        xor edx, edx
        movzx esi, word [cur_drv+D_HEADS]
        test esi, esi
        jz .chsbad
        div esi
        cmp eax, 1023
        ja .chsbad
        mov dh, dl
        mov ch, al
        shl ah, 6
        or cl, ah
        mov dl, [cur_drv+D_BIOS]
        mov ah, [io_cmd]
        mov al, 1
        push bp
        int 0x13
        pop bp
        pop eax
        jnc .ok
.err:
        push eax
        xor ax, ax
        mov dl, [cur_drv+D_BIOS]
        push bp
        int 0x13
        pop bp
        pop eax
        dec di
        jnz .retry
        popad
        stc
        ret
.chsbad:
        pop eax
        popad
        stc
        ret
.ok:
        popad
        clc
        ret

; EAX = sector relativo -> copia en secbuf (caché de un sector)
cache_read:
        push edx
        mov edx, eax
        add edx, [cur_drv+D_START]
        cmp edx, [cache_lba]
        jne .miss
        push ax
        mov al, [cur_drv+D_BIOS]
        cmp al, [cache_bios]
        pop ax
        je .hit
.miss:
        push es
        push bx
        push ds
        pop es
        mov bx, secbuf
        call read_sector
        pop bx
        pop es
        jc .err
        mov [cache_lba], edx
        mov [cache_rel], eax
        push ax
        mov al, [cur_drv+D_BIOS]
        mov [cache_bios], al
        pop ax
.hit:
        mov [cache_rel], eax
        pop edx
        clc
        ret
.err:
        mov dword [cache_lba], 0xFFFFFFFF
        pop edx
        stc
        ret

; -----------------------------------------------------------------------------
;  FAT12 / FAT16 (la FAT se lee por sectores, con su propia caché)
; -----------------------------------------------------------------------------
; EAX = sector de FAT relativo -> fatsec
fat_load_sec:
        push edx
        mov edx, eax
        add edx, [cur_drv+D_START]
        cmp edx, [fatc_lba]
        jne .m
        push ax
        mov al, [cur_drv+D_BIOS]
        cmp al, [fatc_bios]
        pop ax
        je .h
.m:
        call fat_flush
        push es
        push bx
        push ds
        pop es
        mov bx, fatsec
        call read_sector
        pop bx
        pop es
        jc .e
        mov [fatc_lba], edx
        mov [fatc_rel], eax
        push ax
        mov al, [sel_drv]
        mov [fatc_drv], al
        pop ax
        push ax
        mov al, [cur_drv+D_BIOS]
        mov [fatc_bios], al
        pop ax
.h:
        pop edx
        clc
        ret
.e:
        mov dword [fatc_lba], 0xFFFFFFFF
        pop edx
        stc
        ret

; ECX = desplazamiento dentro de la FAT -> DL = byte; CF
fat_byte:
        push eax
        push ecx
        push bx
        mov eax, ecx
        shr eax, 9
        add eax, [cur_drv+D_FATLBA]
        call fat_load_sec
        jc .e
        mov bx, cx
        and bx, 511
        mov dl, [fatsec+bx]
        clc
.e:
        pop bx
        pop ecx
        pop eax
        ret

; AX = clúster -> AX = siguiente (0xFFFF = fin de cadena o error)
fat_next:
        push ecx
        push edx
        movzx ecx, ax
        test byte [cur_drv+D_FLAGS], DF_FAT16
        jnz .f16
        mov edx, ecx
        shr edx, 1
        add ecx, edx
        push ax
        call fat_byte
        jc .e12
        mov [fn_lo], dl
        inc ecx
        call fat_byte
        jc .e12
        mov [fn_lo+1], dl
        pop ax
        test al, 1
        mov ax, [fn_lo]
        jz .even
        shr ax, 4
        jmp .c12
.even:
        and ax, 0x0FFF
.c12:
        cmp ax, 0x0FF7
        jb .r
        mov ax, 0xFFFF
        jmp .r
.e12:
        pop ax
        mov ax, 0xFFFF
        jmp .r
.f16:
        shl ecx, 1
        call fat_byte
        jc .e16
        mov [fn_lo], dl
        inc ecx
        call fat_byte
        jc .e16
        mov [fn_lo+1], dl
        mov ax, [fn_lo]
        cmp ax, 0xFFF7
        jb .r
.e16:
        mov ax, 0xFFFF
.r:
        pop edx
        pop ecx
        ret

; AX = clúster -> EAX = primer sector relativo
clus_lba:
        push edx
        movzx eax, ax
        sub eax, 2
        movzx edx, word [cur_drv+D_SPCW]
        imul eax, edx
        add eax, [cur_drv+D_DATALBA]
        pop edx
        ret

; contar clústeres libres de la unidad seleccionada -> AX
fat_free_count:
        push bx
        push cx
        xor bx, bx
        mov cx, 2
.l:
        mov ax, cx
        sub ax, 2
        cmp ax, [cur_drv+D_NCLUS]
        jae .d
        mov ax, cx
        call fat_next
        test ax, ax
        jnz .n
        inc bx
.n:
        inc cx
        jnz .l
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
        push eax
        push bx
        push cx
        push dx
        mov [dg_idx], cx
        test ax, ax
        jnz .sub
        cmp cx, [cur_drv+D_ROOTENT]
        jae .end
        movzx eax, cx
        shr eax, 4
        add eax, [cur_drv+D_ROOTLBA]
        jmp .read
.sub:
        mov bx, ax
        mov ax, cx
        xor dx, dx
        div word [cur_drv+D_EPC]
        mov cx, ax
        mov ax, bx
        mov [dg_in], dx
.walk:
        jcxz .got
        call fat_next
        cmp ax, 0xFFF7
        jae .end
        cmp ax, 2
        jb .end
        dec cx
        jmp .walk
.got:
        call clus_lba
        movzx edx, word [dg_in]
        shr edx, 4
        add eax, edx
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
        pop eax
        clc
        ret
.end:
        pop dx
        pop cx
        pop bx
        pop eax
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
        mov al, [cur_drive]
        cmp byte [si+1], ':'
        jne .nodrv
        mov al, [si]
        sub al, 'A'
        jb .baddrv
        cmp al, NDRV
        jae .baddrv
        add si, 2
.nodrv:
        mov [op_drv], al
        call select_drive
        jc .baddrv
        mov di, npath
        cmp byte [si], '\'
        jne .rel
        inc si
        mov byte [di], 0
        jmp .loop
.rel:
        push si
        movzx si, byte [op_drv]
        imul si, si, PATHLEN
        add si, cur_paths
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
        push ax
        mov al, [di+SFT_DRV]
        call select_drive
        pop ax
        jc .out
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
        movzx ecx, si
        shr ecx, 9
        call clus_lba
        add eax, ecx
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
        cmp cx, 0xFFF7
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
        mov al, [sel_drv]
        mov [di+SFT_DRV], al
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

; -----------------------------------------------------------------------------
;  Escritura: FAT, directorios y ficheros
; -----------------------------------------------------------------------------
; escribir secbuf en su sector
cache_write:
        push eax
        push bx
        push es
        push ds
        pop es
        mov eax, [cache_rel]
        mov bx, secbuf
        call write_sector
        pop es
        pop bx
        pop eax
        ret

; EAX = sector relativo -> secbuf a cero y asociado a ese sector
cache_blank:
        push eax
        push cx
        push di
        push es
        push ds
        pop es
        mov di, secbuf
        mov cx, 256
        push ax
        xor ax, ax
        rep stosw
        pop ax
        mov [cache_rel], eax
        add eax, [cur_drv+D_START]
        mov [cache_lba], eax
        mov al, [cur_drv+D_BIOS]
        mov [cache_bios], al
        pop es
        pop di
        pop cx
        pop eax
        ret

; escribir el sector de FAT modificado en todas las copias
fat_flush:
        cmp byte [fat_dirty], 0
        jne .go
        ret
.go:
        pushad
        push es
        mov al, [sel_drv]
        push ax
        mov al, [fatc_drv]
        cmp al, [sel_drv]
        je .same
        call select_raw
.same:
        mov eax, [fatc_rel]
        movzx cx, byte [cur_drv+D_NFATS]
        push ds
        pop es
        mov bx, fatsec
.l:
        call write_sector
        movzx edx, word [cur_drv+D_SPF]
        add eax, edx
        loop .l
        mov byte [fat_dirty], 0
        pop ax
        cmp al, [sel_drv]
        je .ns
        call select_raw
.ns:
        pop es
        popad
        ret

; ECX = desplazamiento en la FAT, BL = máscara AND, BH = valor OR
fat_mod_byte:
        push eax
        push ecx
        push si
        mov eax, ecx
        shr eax, 9
        add eax, [cur_drv+D_FATLBA]
        call fat_load_sec
        jc .e
        mov si, cx
        and si, 511
        and [fatsec+si], bl
        or [fatsec+si], bh
        mov byte [fat_dirty], 1
.e:
        pop si
        pop ecx
        pop eax
        ret

; AX = clúster, DX = valor
fat_set:
        push bx
        push ecx
        push edx
        movzx ecx, ax
        test byte [cur_drv+D_FLAGS], DF_FAT16
        jnz .f16
        and dx, 0x0FFF
        shr ecx, 1
        add cx, ax
        test al, 1
        jnz .odd
        mov bl, 0
        mov bh, dl
        call fat_mod_byte
        inc ecx
        mov bl, 0xF0
        mov bh, dh
        call fat_mod_byte
        jmp .r
.odd:
        mov bl, 0x0F
        mov bh, dl
        shl bh, 4
        call fat_mod_byte
        inc ecx
        shr dx, 4
        mov bl, 0
        mov bh, dl
        call fat_mod_byte
        jmp .r
.f16:
        shl ecx, 1
        mov bl, 0
        mov bh, dl
        call fat_mod_byte
        inc ecx
        mov bh, dh
        call fat_mod_byte
.r:
        pop edx
        pop ecx
        pop bx
        ret

; AX = clúster anterior (0 = ninguno) -> AX = clúster nuevo (fin de cadena)
; CF = 1 si el disco está lleno
fat_alloc:
        push bx
        push cx
        push dx
        push ax
        mov cx, [cur_drv+D_NCLUS]
        mov bx, [cur_drv+D_HINT]
        mov dx, [cur_drv+D_NCLUS]
        inc dx
        cmp bx, 2
        jb .h0
        cmp bx, dx
        jbe .l
.h0:
        mov bx, 2
.l:
        mov ax, bx
        call fat_next
        test ax, ax
        jz .found
        inc bx
        cmp bx, dx
        jbe .n
        mov bx, 2
.n:
        loop .l
        pop ax
        pop dx
        pop cx
        pop bx
        stc
        ret
.found:
        mov ax, bx
        mov dx, 0xFFFF
        call fat_set
        pop ax
        test ax, ax
        jz .nolink
        mov dx, bx
        call fat_set
.nolink:
        mov ax, bx
        inc bx
        mov [cur_drv+D_HINT], bx
        call drv_store
        pop dx
        pop cx
        pop bx
        clc
        ret

; AX = primer clúster -> liberar toda la cadena
fat_free_chain:
        push ax
        push dx
.l:
        cmp ax, 2
        jb .d
        cmp ax, 0xFFF7
        jae .d
        push ax
        call fat_next
        mov dx, ax
        pop ax
        push dx
        xor dx, dx
        call fat_set
        pop ax
        jmp .l
.d:
        pop dx
        pop ax
        ret

; AX = clúster -> llenarlo de ceros
zero_cluster:
        push eax
        push cx
        call clus_lba
        mov cx, [cur_drv+D_SPCW]
.l:
        call cache_blank
        call cache_write
        inc eax
        loop .l
        pop cx
        pop eax
        ret

; -> AX = hora DOS, DX = fecha DOS
dos_now:
        push bx
        push cx
        push bp
        mov ah, 2
        int 0x1A
        jnc .t
        xor cx, cx
        xor dx, dx
.t:
        mov al, ch
        call bcd2bin
        movzx bx, al
        shl bx, 6
        mov al, cl
        call bcd2bin
        or bl, al
        shl bx, 5
        mov al, dh
        call bcd2bin
        shr al, 1
        or bl, al
        push bx
        mov ah, 4
        int 0x1A
        jnc .d
        mov cx, 0x2026
        mov dx, 0x0101
.d:
        mov al, ch
        call bcd2bin
        mov bl, 100
        mul bl
        mov bx, ax
        mov al, cl
        call bcd2bin
        xor ah, ah
        add bx, ax
        sub bx, 1980
        shl bx, 4
        mov al, dh
        call bcd2bin
        or bl, al
        shl bx, 5
        mov al, dl
        call bcd2bin
        or bl, al
        mov dx, bx
        pop ax
        pop bp
        pop cx
        pop bx
        ret

; AX = directorio -> CX = índice de una entrada libre (amplía subdirectorios)
; CF y AX = error si el directorio raíz está lleno o el disco lleno
dir_free_slot:
        push ax
        push si
        xor cx, cx
.l:
        call dir_get
        jc .end
        cmp byte [si], 0
        je .ok
        cmp byte [si], 0xE5
        je .ok
        inc cx
        jnz .l
.end:
        test ax, ax
        jz .full
        ; ampliar el subdirectorio con un clúster nuevo
        push cx
.last:
        mov si, ax
        call fat_next
        cmp ax, 0xFFF7
        jae .gotl
        cmp ax, 2
        jb .gotl
        jmp .last
.gotl:
        mov ax, si
        call fat_alloc
        pop cx
        jc .full
        call zero_cluster
.ok:
        pop si
        pop ax
        clc
        ret
.full:
        pop si
        pop ax
        mov ax, E_ACCESS
        stc
        ret

; AX = directorio, CX = índice, SI = nombre (11), BL = atributos,
; DX = primer clúster -> entrada nueva con la fecha y hora actuales
dir_make:
        push eax
        push di
        push es
        push si
        push dx
        call dir_get
        pop dx
        jc .r
        mov di, si
        pop si
        push si
        push ds
        pop es
        push cx
        mov cx, 11
        rep movsb
        pop cx
        mov [di], bl
        push dx
        push di
        push cx
        inc di
        xor ax, ax
        mov cx, 5
        rep stosw
        pop cx
        pop di
        call dos_now
        mov [di+11], ax
        mov [di+13], dx
        pop dx
        mov [di+15], dx
        mov dword [di+17], 0
        call cache_write
.r:
        pop si
        pop es
        pop di
        pop eax
        ret

; actualizar la entrada de directorio de la SFT DI
dir_update:
        pushad
        mov al, [di+SFT_DRV]
        call select_drive
        jc .r
        mov ax, [di+SFT_DIRC]
        mov cx, [di+SFT_DIRI]
        call dir_get
        jc .r
        mov ax, [di+SFT_CLUS]
        mov [si+26], ax
        mov eax, [di+SFT_SIZE]
        mov [si+28], eax
        mov ax, [di+SFT_TIME]
        mov [si+22], ax
        mov ax, [di+SFT_DATE]
        mov [si+24], ax
        or byte [si+11], 0x20
        call cache_write
        and byte [di+SFT_FLAGS], 0xFE
.r:
        popad
        ret

; soltar una referencia a la SFT DI (al llegar a cero, guardar la entrada)
sft_release:
        cmp byte [di+SFT_REF], 0
        je .r
        dec byte [di+SFT_REF]
        jnz .r
        cmp byte [di+SFT_TYPE], T_FILE
        jne .r
        test byte [di+SFT_FLAGS], 1
        jz .r
        call dir_update
.r:
        ret

; DI = SFT, AX = índice de clúster -> AX = clúster (amplía la cadena)
sft_seek_alloc:
        push bx
        push cx
        cmp word [di+SFT_CLUS], 0
        jne .have
        push ax
        xor ax, ax
        call fat_alloc
        jc .full0
        mov [di+SFT_CLUS], ax
        mov [di+SFT_CCL], ax
        mov word [di+SFT_CIX], 0
        pop ax
.have:
        cmp ax, [di+SFT_CIX]
        jb .restart
        mov cx, [di+SFT_CCL]
        mov bx, [di+SFT_CIX]
        jmp .walk
.restart:
        mov cx, [di+SFT_CLUS]
        xor bx, bx
.walk:
        cmp bx, ax
        je .done
        push ax
        mov ax, cx
        call fat_next
        cmp ax, 2
        jb .ext
        cmp ax, 0xFFF7
        jb .nx
.ext:
        mov ax, cx
        call fat_alloc
        jc .full1
.nx:
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
.full1:
        pop ax
        mov [di+SFT_CCL], cx
        mov [di+SFT_CIX], bx
        pop cx
        pop bx
        stc
        ret
.full0:
        pop ax
        pop cx
        pop bx
        stc
        ret

; marcar la SFT como modificada con la fecha y hora actuales
sft_touch:
        push ax
        push dx
        call dos_now
        mov [di+SFT_TIME], ax
        mov [di+SFT_DATE], dx
        or byte [di+SFT_FLAGS], 1
        pop dx
        pop ax
        ret

; DI = SFT, ES:BX = origen, CX = bytes -> AX = escritos
file_write:
        push bx
        push cx
        push dx
        push si
        mov [fr_off], bx
        mov [fr_seg], es
        mov [fr_left], cx
        mov word [fr_done], 0
        mov al, [di+SFT_DRV]
        call select_drive
        jc .out
        call sft_touch
.loop:
        cmp word [fr_left], 0
        je .out
        mov eax, [di+SFT_POS]
        xor edx, edx
        movzx ecx, word [cur_drv+D_BPC]
        div ecx
        call sft_seek_alloc
        jc .out
        mov si, dx
        movzx ecx, si
        shr ecx, 9
        call clus_lba
        add eax, ecx
        and si, 511
        mov cx, 512
        sub cx, si
        cmp cx, [fr_left]
        jbe .c1
        mov cx, [fr_left]
.c1:
        ; sector entero: no hace falta leerlo
        test si, si
        jnz .rd
        cmp cx, 512
        jne .rd
        call cache_blank
        jmp .cp
.rd:
        call cache_read
        jc .out
.cp:
        push ds
        push es
        push di
        push si
        mov ax, ds
        mov es, ax
        lea di, [secbuf+si]
        lds si, [fr_off]
        push cx
        rep movsb
        pop cx
        pop si
        pop di
        pop es
        pop ds
        add [fr_off], cx
        call cache_write
        movzx ecx, cx
        add [di+SFT_POS], ecx
        mov eax, [di+SFT_POS]
        cmp eax, [di+SFT_SIZE]
        jbe .ns
        mov [di+SFT_SIZE], eax
.ns:
        sub [fr_left], cx
        add [fr_done], cx
        jmp .loop
.out:
        mov ax, [fr_done]
        pop si
        pop dx
        pop cx
        pop bx
        ret

; DI = SFT -> el fichero termina en la posición actual
file_truncate:
        pushad
        mov al, [di+SFT_DRV]
        call select_drive
        jc .r
        call sft_touch
        mov eax, [di+SFT_POS]
        cmp eax, [di+SFT_SIZE]
        jae .grow
        test eax, eax
        jnz .part
        mov ax, [di+SFT_CLUS]
        call fat_free_chain
        mov word [di+SFT_CLUS], 0
        jmp .set
.part:
        dec eax
        xor edx, edx
        movzx ecx, word [cur_drv+D_BPC]
        div ecx
        call sft_seek_clus
        jc .set
        mov bx, ax
        call fat_next
        push ax
        mov ax, bx
        mov dx, 0xFFFF
        call fat_set
        pop ax
        call fat_free_chain
        jmp .set
.grow:
        je .set
        dec eax
        xor edx, edx
        movzx ecx, word [cur_drv+D_BPC]
        div ecx
        call sft_seek_alloc
.set:
        mov word [di+SFT_CIX], 0xFFFF
        mov eax, [di+SFT_POS]
        mov [di+SFT_SIZE], eax
.r:
        popad
        ret

; crear o vaciar el fichero de la ruta ES:SI, atributos CL
; AL = 1: fallar si ya existe -> DI = SFT abierta (lectura/escritura)
create_file:
        mov [cf_attr], cl
        mov [cf_new], al
        call normalize
        jc .r
        call walk
        jc .r
        cmp byte [w_has], 0
        je .nopath
        call check_device
        jnc .notdev
        push ax
        call sft_alloc
        pop dx
        jc .r
        mov byte [di+SFT_REF], 1
        mov [di+SFT_TYPE], dl
        mov byte [di+SFT_MODE], 2
        clc
        ret
.notdev:
        ; nombre válido (sin comodines)
        mov si, w_name
        cmp byte [si], ' '
        je .nopath
        mov cx, 11
.chk:
        cmp byte [si], '?'
        je .nopath
        inc si
        loop .chk
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x16
        call find_entry
        jc .new
        cmp byte [cf_new], 0
        jne .exists
        test byte [fentry+11], 0x19
        jnz .acc
        ; vaciar el fichero existente
        mov ax, [fentry+26]
        call fat_free_chain
        mov ax, [w_dir]
        call dir_get
        jc .acc
        mov word [si+26], 0
        mov dword [si+28], 0
        mov al, [cf_attr]
        and al, 0x27
        mov [si+11], al
        call cache_write
        jmp .open
.new:
        mov ax, [w_dir]
        call dir_free_slot
        jc .r
        mov si, w_name
        mov bl, [cf_attr]
        and bl, 0x27
        xor dx, dx
        call dir_make
        jc .acc
.open:
        ; releer la entrada creada en fentry
        push cx
        mov ax, [w_dir]
        call dir_get
        mov di, fentry
        push es
        push ds
        pop es
        push cx
        mov cx, 16
        rep movsw
        pop cx
        pop es
        pop cx
        call sft_alloc
        jc .r
        call sft_fill_file
        mov byte [di+SFT_MODE], 2
        call sft_touch
        clc
.r:
        ret
.exists:
        mov ax, E_EXISTS
        stc
        ret
.acc:
        mov ax, E_ACCESS
        stc
        ret
.nopath:
        mov ax, E_NOPATH
        stc
        ret

; ruta ES:SI -> fentry, CX = índice, [w_dir]; CF/AX (atributos DL)
find_path:
        call normalize
        jc .r
        call walk
        jc .r
        cmp byte [w_has], 0
        je .nf
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        call find_entry
        jc .nf
        clc
.r:
        ret
.nf:
        mov ax, E_NOFILE
        stc
        ret

; -----------------------------------------------------------------------------
;  Entrada y salida estándar (handles 0 y 1 del proceso: admiten redirección)
; -----------------------------------------------------------------------------
; BX = handle -> AL = tipo (T_CON si no es válido), DI = SFT
std_type:
        push ax
        call get_sft
        pop ax
        jc .con
        mov ah, [di+SFT_TYPE]
        ret
.con:
        mov ah, T_CON
        ret

; AL -> salida estándar
std_out:
        push ax
        push bx
        push cx
        push di
        push es
        mov bx, 1
        call std_type
        cmp ah, T_FILE
        je .file
        cmp ah, T_NUL
        je .r
        call con_out
        jmp .r
.file:
        test byte [di+SFT_MODE], 3
        jz .r
        mov [so_ch], al
        push ds
        pop es
        mov bx, so_ch
        mov cx, 1
        call file_write
.r:
        pop es
        pop di
        pop cx
        pop bx
        pop ax
        ret

; -> AL desde la entrada estándar (1Ah al final de un fichero)
std_in:
        push bx
        push cx
        push di
        push es
        mov bx, 0
        call std_type
        cmp ah, T_FILE
        je .file
        cmp ah, T_NUL
        je .eof
        call con_getc
        jmp .r
.file:
        push ds
        pop es
        mov bx, so_ch
        mov cx, 1
        call file_read
        test ax, ax
        jz .eof
        mov al, [so_ch]
        jmp .r
.eof:
        mov al, 0x1A
.r:
        pop es
        pop di
        pop cx
        pop bx
        ret

; ZF=0 si la entrada estándar tiene algo que leer
std_check:
        push bx
        push di
        mov bx, 0
        call std_type
        cmp ah, T_FILE
        je .file
        cmp ah, T_NUL
        je .no
        pop di
        pop bx
        jmp con_check
.file:
        mov eax, [di+SFT_POS]
        cmp eax, [di+SFT_SIZE]
        jae .no
        or al, 1
        pop di
        pop bx
        ret
.no:
        xor al, al
        pop di
        pop bx
        ret

; ¿la entrada estándar es la consola? ZF=1 si sí
std_in_is_con:
        push ax
        push bx
        push di
        mov bx, 0
        call std_type
        cmp ah, T_CON
        pop di
        pop bx
        pop ax
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
        call fat_flush
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
        call std_in
        mov [F_AL], al
        jmp std_out
fn_02:
        mov al, [F_DL]
        jmp std_out
fn_06:
        mov al, [F_DL]
        cmp al, 0xFF
        jne std_out
        call std_check
        jz .none
        call std_in
        mov [F_AL], al
        and byte [F_FL], 0xBF
        ret
.none:
        mov byte [F_AL], 0
        or byte [F_FL], 0x40
        ret
fn_07:
fn_08:
        call std_in
        mov [F_AL], al
        ret
fn_09:
        mov es, [F_DS]
        mov si, [F_DX]
.l:
        mov al, [es:si]
        cmp al, '$'
        je .d
        call std_out
        inc si
        jmp .l
.d:
        mov byte [F_AL], '$'
        ret
fn_0a:
        mov es, [F_DS]
        mov di, [F_DX]
        call std_in_is_con
        je line_input
        ; entrada redirigida: leer hasta CR
        movzx cx, byte [es:di]
        xor bx, bx
.l:
        call std_in
        cmp al, 0x1A
        je .end
        cmp al, 10
        je .l
        cmp al, 13
        je .end
        inc bx
        cmp bx, cx
        jae .full
        mov [es:di+1+bx], al
        jmp .l
.full:
        dec bx
        jmp .l
.end:
        mov byte [es:di+2+bx], 13
        mov [es:di+1], bl
        ret
fn_0b:
        call std_check
        mov byte [F_AL], 0
        jz .r
        mov byte [F_AL], 0xFF
.r:
        ret
fn_0c:
        call std_in_is_con
        jne .e
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
fn_0d:
        call drives_scan
        push es
        push ds
        pop es
        mov di, cur_paths + 2*PATHLEN
        mov cx, (NDRV-2)*PATHLEN
        xor al, al
        rep stosb
        pop es
        ret
fn_0e:
        mov al, [F_DL]
        call select_raw
        jc .r
        mov [cur_drive], al
.r:
        mov al, [ndrives]
        cmp al, 5
        jae .s
        mov al, 5
.s:
        mov [F_AL], al
        ret
fn_19:
        mov al, [cur_drive]
        mov [F_AL], al
        ret

; BL del llamante (0 = actual, 1 = A:...) -> AL = unidad
bl_drive:
        mov al, [F_BL]
        test al, al
        jnz .d
        mov al, [cur_drive]
        ret
.d:
        dec al
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
        test al, al
        jnz .d
        mov al, [cur_drive]
        inc al
.d:
        dec al
        call select_drive
        jc .bad
        call fat_free_count
        mov [F_BX], ax
        mov ax, [cur_drv+D_SPCW]
        mov [F_AX], ax
        mov word [F_CX], 512
        mov ax, [cur_drv+D_NCLUS]
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
        movzx di, byte [op_drv]
        imul di, di, PATHLEN
        add di, cur_paths
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        ret
fn_47:
        mov al, [F_DL]
        test al, al
        jnz .d
        mov al, [cur_drive]
        inc al
.d:
        dec al
        call select_raw
        jc .bad
        movzx si, al
        imul si, si, PATHLEN
        add si, cur_paths
        mov es, [F_DS]
        mov di, [F_SI]
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
        mov [es:di+1], ax
        mov al, [op_drv]
        add al, 'A'
        mov [es:di], al
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
        call open_path
        jc ret_err
        mov al, [F_AL]
        mov [di+SFT_MODE], al
        test al, 3
        jz .ok
        cmp byte [di+SFT_TYPE], T_FILE
        jne .ok
        test byte [di+SFT_ATTR], 1
        jz .ok
        mov byte [di+SFT_REF], 0
        mov ax, E_ACCESS
        jmp ret_err
.ok:
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
        call sft_release
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
        test byte [di+SFT_MODE], 3
        jz .acc
        mov cx, [F_CX]
        jcxz .trunc
        mov es, [F_DS]
        mov bx, [F_DX]
        call file_write
        mov [F_AX], ax
        ret
.trunc:
        call file_truncate
        mov word [F_AX], 0
        ret
.acc:
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
        mov es, [F_DS]
        mov si, [F_DX]
        mov dl, 0x16
        call find_path
        jc ret_err
        test byte [fentry+11], 0x18
        jnz .sacc
        mov ax, [w_dir]
        call dir_get
        jc .sacc
        mov al, [F_CL]
        and al, 0x27
        mov [si+11], al
        call cache_write
        ret
.sacc:
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
        cmp al, 0x0D
        je .gen
        jmp fn_unsup
.baddrv:
        mov ax, E_DRIVE
        jmp ret_err
.gen:
        cmp byte [F_CL], 0x60
        je .getp
        cmp byte [F_CL], 0x40
        je .ok
        jmp fn_unsup
.getp:
        call bl_drive
        call select_raw
        jc .baddrv
        test byte [cur_drv+D_FLAGS], DF_MOUNTED
        jnz .gm
        call mount
.gm:
        mov es, [F_DS]
        mov di, [F_DX]
        push di
        add di, 1
        mov cx, 6 + 25
        xor al, al
        rep stosb
        pop di
        mov byte [es:di+1], 7
        mov word [es:di+2], 0
        test byte [cur_drv+D_FLAGS], DF_FIXED
        jz .gfl
        mov byte [es:di+1], 5
        mov word [es:di+2], 1
.gfl:
        test byte [cur_drv+D_FLAGS], DF_MOUNTED
        jz .gnobpb
        xor eax, eax
        call cache_read
        jc .gnobpb
        push di
        add di, 7
        mov si, secbuf+0x0B
        mov cx, 25
        rep movsb
        pop di
        jmp .gcommon
.gnobpb:
        mov word [es:di+7], 512
        mov byte [es:di+7+10], 0xF8
        test byte [cur_drv+D_FLAGS], DF_FIXED
        jnz .gtot
        mov byte [es:di+7+10], 0xF0
.gtot:
        mov eax, [cur_drv+D_SIZE]
        cmp eax, 0xFFFF
        ja .g32
        mov [es:di+7+8], ax
        jmp .gcommon
.g32:
        mov [es:di+7+21], eax
.gcommon:
        mov ax, [cur_drv+D_SPT]
        mov [es:di+7+13], ax
        mov ax, [cur_drv+D_HEADS]
        mov [es:di+7+15], ax
        mov eax, [cur_drv+D_START]
        mov [es:di+7+17], eax
        ; cilindros
        mov eax, [cur_drv+D_SIZE]
        movzx ecx, word [cur_drv+D_SPT]
        movzx edx, word [cur_drv+D_HEADS]
        imul ecx, edx
        jecxz .gnc
        xor edx, edx
        div ecx
        cmp eax, 0xFFFF
        jbe .gc
        mov ax, 0xFFFF
.gc:
        mov [es:di+4], ax
.gnc:
        ret
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
        call bl_drive
        call select_raw
        jc .baddrv
        mov ax, 1
        test byte [cur_drv+D_FLAGS], DF_FIXED
        jnz .rr
        xor ax, ax
.rr:
        mov [F_AX], ax
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
        call sft_release
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
        jne .set
        mov ax, [di+SFT_TIME]
        mov [F_CX], ax
        mov ax, [di+SFT_DATE]
        mov [F_DX], ax
        ret
.set:
        cmp byte [F_AL], 1
        jne .r
        mov ax, [F_CX]
        mov [di+SFT_TIME], ax
        mov ax, [F_DX]
        mov [di+SFT_DATE], ax
        or byte [di+SFT_FLAGS], 1
.r:
        ret

fn_6c:
        mov ax, [F_SI]
        mov [F_DX], ax
        mov al, [F_BL]
        mov [F_AL], al
        mov al, [F_DL]
        mov [x6c_act], al
        call fn_3d
        test byte [F_FL], 1
        jnz .notopen
        mov al, [x6c_act]
        and al, 0x0F
        jz .exists
        cmp al, 2
        jne .opened
        ; sustituir: vaciar
        mov bx, [F_AX]
        call get_sft
        jc .opened
        mov dword [di+SFT_POS], 0
        call file_truncate
        mov word [F_CX], 3
        ret
.opened:
        mov word [F_CX], 1
        ret
.exists:
        mov bx, [F_AX]
        call get_sft
        jc .e80
        mov al, 0xFF
        call jft_set
        call sft_release
.e80:
        mov ax, E_EXISTS
        jmp ret_err
.notopen:
        cmp word [F_AX], E_NOFILE
        jne .r
        test byte [x6c_act], 0x10
        jz .r
        and byte [F_FL], 0xFE
        call fn_3c
        test byte [F_FL], 1
        jnz .r
        mov word [F_CX], 2
.r:
        ret

fn_rdonly:
        mov ax, E_ACCESS
        jmp ret_err

; ---- 3Ch / 5Bh: crear fichero ----
fn_3c:
        xor al, al
        jmp fn_create
fn_5b:
        mov al, 1
fn_create:
        mov es, [F_DS]
        mov si, [F_DX]
        mov cl, [F_CL]
        call create_file
        jc ret_err
        call jft_alloc
        jc .noh
        mov ax, di
        sub ax, sft_table
        shr ax, 5
        call jft_set
        mov [F_AX], bx
        ret
.noh:
        mov byte [di+SFT_REF], 0
        jmp ret_err

; ---- 41h: borrar fichero ----
fn_41:
        mov es, [F_DS]
        mov si, [F_DX]
        mov dl, 0x06
        call find_path
        jc ret_err
        test byte [fentry+11], 0x01
        jnz .acc
        mov ax, [fentry+26]
        call fat_free_chain
        mov ax, [w_dir]
        call dir_get
        jc .acc
        mov byte [si], 0xE5
        call cache_write
        ret
.acc:
        mov ax, E_ACCESS
        jmp ret_err

; ---- 39h: crear directorio ----
fn_39:
        mov es, [F_DS]
        mov si, [F_DX]
        call normalize
        jc ret_err
        call walk
        jc ret_err
        cmp byte [w_has], 0
        je .acc
        mov si, w_name
        mov cx, 11
.chk:
        cmp byte [si], '?'
        je .acc
        inc si
        loop .chk
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x16
        call find_entry
        jnc .acc
        xor ax, ax
        call fat_alloc
        jc .acc
        mov [md_clus], ax
        call zero_cluster
        ; "." y ".."
        call clus_lba
        call cache_read
        mov di, secbuf
        push es
        push ds
        pop es
        mov al, ' '
        mov cx, 64
        push di
        rep stosb
        pop di
        mov byte [di], '.'
        mov byte [di+32], '.'
        mov byte [di+33], '.'
        mov byte [di+11], 0x10
        mov byte [di+32+11], 0x10
        xor ax, ax
        push di
        lea di, [di+12]
        mov cx, 10
        rep stosb
        pop di
        push di
        lea di, [di+32+12]
        mov cx, 10
        rep stosb
        pop di
        pop es
        call dos_now
        mov [di+22], ax
        mov [di+24], dx
        mov [di+32+22], ax
        mov [di+32+24], dx
        mov ax, [md_clus]
        mov [di+26], ax
        mov ax, [w_dir]
        mov [di+32+26], ax
        mov dword [di+28], 0
        mov dword [di+32+28], 0
        call cache_write
        ; entrada en el directorio padre
        mov ax, [w_dir]
        call dir_free_slot
        jc .free
        mov si, w_name
        mov bl, 0x10
        mov dx, [md_clus]
        call dir_make
        ret
.free:
        push ax
        mov ax, [md_clus]
        call fat_free_chain
        pop ax
        jmp ret_err
.acc:
        mov ax, E_ACCESS
        jmp ret_err

; ---- 3Ah: borrar directorio ----
fn_3a:
        mov es, [F_DS]
        mov si, [F_DX]
        mov dl, 0x16
        call find_path
        jc .np
        test byte [fentry+11], 0x10
        jz .np
        ; no puede ser el directorio actual de la unidad
        push cx
        movzx si, byte [op_drv]
        imul si, si, PATHLEN
        add si, cur_paths
        mov di, npath
.cmp:
        mov al, [di]
        test al, al
        jz .pref
        cmp al, [si]
        jne .notcur
        inc si
        inc di
        jmp .cmp
.pref:
        mov al, [si]
        test al, al
        jz .cur
        cmp al, '\'
        je .cur
.notcur:
        pop cx
        ; ¿vacío?
        mov [md_idx], cx
        mov ax, [fentry+26]
        mov [md_clus], ax
        mov cx, 2
.l:
        call dir_get
        jc .empty
        mov bl, [si]
        test bl, bl
        jz .empty
        cmp bl, 0xE5
        je .n
        cmp byte [si+11], 0x0F
        je .n
        mov ax, E_ACCESS
        jmp ret_err
.n:
        inc cx
        jmp .l
.empty:
        mov ax, [md_clus]
        call fat_free_chain
        mov ax, [w_dir]
        mov cx, [md_idx]
        call dir_get
        jc .np
        mov byte [si], 0xE5
        call cache_write
        ret
.cur:
        pop cx
        mov ax, E_CURDIR
        jmp ret_err
.np:
        mov ax, E_NOPATH
        jmp ret_err

; ---- 56h: renombrar / mover ----
fn_56:
        mov es, [F_DS]
        mov si, [F_DX]
        mov dl, 0x16
        call find_path
        jc ret_err
        test byte [fentry+11], 0x08
        jnz .acc
        mov [rn_idx], cx
        mov ax, [w_dir]
        mov [rn_dir], ax
        mov al, [op_drv]
        mov [rn_drv], al
        push es
        push ds
        pop es
        mov si, fentry
        mov di, rn_ent
        mov cx, 16
        rep movsw
        pop es
        ; destino
        mov es, [F_ES]
        mov si, [F_DI]
        call normalize
        jc ret_err
        mov al, [op_drv]
        cmp al, [rn_drv]
        jne .nsame
        call walk
        jc ret_err
        cmp byte [w_has], 0
        je .acc
        mov si, w_name
        mov cx, 11
.chk:
        cmp byte [si], '?'
        je .acc
        inc si
        loop .chk
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, 0x16
        call find_entry
        jnc .acc
        mov ax, [w_dir]
        cmp ax, [rn_dir]
        je .inplace
        test byte [rn_ent+11], 0x10
        jnz .acc
        ; mover a otro directorio
        call dir_free_slot
        jc ret_err
        call dir_get
        jc .acc
        push es
        push ds
        pop es
        mov di, si
        mov si, rn_ent
        push cx
        mov cx, 16
        rep movsw
        pop cx
        sub di, 32
        mov si, w_name
        mov cx, 11
        rep movsb
        pop es
        call cache_write
        mov ax, [rn_dir]
        mov cx, [rn_idx]
        call dir_get
        jc .acc
        mov byte [si], 0xE5
        call cache_write
        ret
.inplace:
        mov cx, [rn_idx]
        call dir_get
        jc .acc
        push es
        push ds
        pop es
        mov di, si
        mov si, w_name
        mov cx, 11
        rep movsb
        pop es
        call cache_write
        ret
.nsame:
        mov ax, E_NOTSAME
        jmp ret_err
.acc:
        mov ax, E_ACCESS
        jmp ret_err

; ---- 68h / 6Ah: confirmar escritura ----
fn_68:
        mov bx, [F_BX]
        call get_sft
        jc ret_err
        cmp byte [di+SFT_TYPE], T_FILE
        jne .r
        test byte [di+SFT_FLAGS], 1
        jz .r
        call dir_update
.r:
        ret

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
        mov al, [op_drv]
        inc al
        mov [es:di], al
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
        mov al, [es:di]
        dec al
        call select_drive
        jnc .sel
        ret
.sel:
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
        mov al, [op_drv]
        add al, 'A'
        mov ah, ':'
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
        push di
        lea di, [sft_table+bx]
        call sft_release
        pop di
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
; INT 25h / INT 26h: leer / escribir sectores absolutos de una unidad.
; AL = unidad, CX = sectores, DX = primer sector, DS:BX = búfer;
; CX = FFFFh: DS:BX -> paquete {dd sector, dw cuenta, dd búfer}.
; Vuelven con RETF dejando los indicadores en la pila (como en DOS).
int25:
        push ax
        mov al, 2
        jmp abs_io
int26:
        push ax
        mov al, 3
abs_io:
        sti
        cld
        pushad
        push ds
        push es
        mov bp, sp
        mov cx, KSEG
        mov ds, cx
        mov [abs_cmd], al
        mov al, [bp+36]
        call select_raw
        jc .bdrv
        mov cx, [bp+28]
        cmp cx, 0xFFFF
        jne .short
        mov es, [bp+2]
        mov bx, [bp+20]
        mov eax, [es:bx]
        mov cx, [es:bx+4]
        les bx, [es:bx+6]
        jmp .go
.short:
        movzx eax, word [bp+24]
        mov es, [bp+2]
        mov bx, [bp+20]
.go:
        jcxz .ok
.l:
        cmp eax, [cur_drv+D_SIZE]
        jae .bsec
        cmp byte [abs_cmd], 2
        jne .w
        call read_sector
        jmp .chk
.w:
        call write_sector
.chk:
        jc .bsec
        inc eax
        mov dx, es
        add dx, 0x20
        mov es, dx
        loop .l
.ok:
        cmp byte [abs_cmd], 3
        jne .ok2
        mov dword [cache_lba], 0xFFFFFFFF
        mov dword [fatc_lba], 0xFFFFFFFF
        mov byte [fat_dirty], 0
        and byte [cur_drv+D_FLAGS], 0xFF ^ DF_MOUNTED
        call drv_store
.ok2:
        pop es
        pop ds
        popad
        pop ax
        xor ax, ax
        clc
        retf
.bdrv:
        mov word [abs_err], 0x0201
        jmp .fail
.bsec:
        mov word [abs_err], 0x0408
.fail:
        pop es
        pop ds
        popad
        pop ax
        mov ax, [cs:abs_err]
        stc
        retf
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
        dw fn_08, fn_09, fn_0a, fn_0b, fn_0c, fn_0d, fn_0e, fn_unsup       ; 08
        dw fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 10
        dw fn_unsup, fn_19, fn_1a, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 18
        dw fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_25, fn_unsup, fn_unsup ; 20
        dw fn_unsup, fn_29, fn_2a, fn_set_ok, fn_2c, fn_set_ok, fn_nop, fn_2f ; 28
        dw fn_30, fn_31, fn_unsup, fn_33, fn_34, fn_35, fn_36, fn_37     ; 30
        dw fn_38, fn_39, fn_3a, fn_3b, fn_3c, fn_3d, fn_3e, fn_3f ; 38
        dw fn_40, fn_41, fn_42, fn_43, fn_44, fn_45, fn_46, fn_47       ; 40
        dw fn_48, fn_49, fn_4a, fn_4b, fn_4c, fn_4d, fn_4e, fn_4f           ; 48
        dw fn_50, fn_51, fn_52, fn_unsup, fn_54, fn_unsup, fn_56, fn_57 ; 50
        dw fn_58, fn_59, fn_unsup, fn_5b, fn_unsup, fn_unsup, fn_unsup, fn_unsup ; 58
        dw fn_60, fn_unsup, fn_62, fn_unsup, fn_unsup, fn_unsup, fn_66, fn_67 ; 60
        dw fn_68, fn_unsup, fn_68, fn_unsup, fn_6c                       ; 68

all11       db "???????????"
shell_path  db "A:\COMMAND.COM", 0
shell_tail  db 3, " /P", 13
shell_fcb   times 16 db 0
shell_pb    dw 0
            dw shell_tail, KSEG
            dw shell_fcb, KSEG
            dw shell_fcb, KSEG

msg_banner  db 13, 10, "HispaDOS 0.2 - MS-DOS 6.22 compatible kernel", 13, 10
            db "(C) 2026 HispaDOS contributors - GPL-2.0-or-later", 13, 10, 13, 10, 0
msg_fatbad  db "Cannot read the boot drive file system", 13, 10, 0
msg_noshell db 13, 10, "Cannot load A:\COMMAND.COM - system halted", 13, 10, 0
msg_reshell db 13, 10, "Restarting command interpreter...", 13, 10, 0
msg_rootterm db 13, 10, "Root process terminated - system halted", 13, 10, 0

; lista de listas (INT 21h AH=52h); la palabra anterior es el primer MCB
            dw 0
lol:        times 0x40 db 0
lol_end:

indos       db 0
            db 0

; =============================================================================
        section .bss
bss_start:
bootsec     resb 512
boot_drive  resb 1
cur_drive   resb 1
sel_drv     resb 1
op_drv      resb 1
ndrives     resb 1
bg_count    resb 1
hd_count    resb 1
hd_cur      resb 1
io_cmd      resb 1
abs_cmd     resb 1
cache_bios  resb 1
fatc_bios   resb 1
            alignb 4
cache_lba   resd 1
fatc_lba    resd 1
cache_rel   resd 1
so_ch       resb 2
fatc_rel    resd 1
fatc_drv    resb 1
fat_dirty   resb 1
cf_attr     resb 1
cf_new      resb 1
x6c_act     resb 1
rn_drv      resb 1
md_clus     resw 1
md_idx      resw 1
rn_idx      resw 1
rn_dir      resw 1
rn_ent      resb 32
abs_err     resw 1
fn_lo       resw 1
dap         resb 16
cur_drv     resb DRV_LEN
drv_tab     resb DRV_LEN*NDRV
cur_paths   resb PATHLEN*NDRV
ser_ok      resb 1
ser_last    resb 1
col         resb 1
pend_scan   resb 1
peek_ok     resb 1
peek_ch     resb 1
            resb 1
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
fatsec      resb 512
            alignb 2
kstack      resb 1024
kstack_top:
            alignb 16
kernel_end:
