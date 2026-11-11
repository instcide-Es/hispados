; =============================================================================
;  Hispa-DOS kernel (KERNEL.SYS)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
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

; --- tabla de ficheros del sistema (SFT), formato de DOS 4+ ---
; (Windows la recorre desde la lista de listas y la modifica directamente)
SFT_REF         equ 0x00            ; handles que la usan (palabra)
SFT_MODE        equ 0x02            ; modo de apertura (palabra)
SFT_ATTR        equ 0x04
SFT_DEVI        equ 0x05            ; información de dispositivo (palabra)
SFT_DPTR        equ 0x07            ; DPB (ficheros) o driver (dispositivos)
SFT_CLUS        equ 0x0B            ; primer cluster
SFT_TIME        equ 0x0D
SFT_DATE        equ 0x0F
SFT_SIZE        equ 0x11            ; (dispositivos: también el driver)
SFT_POS         equ 0x15
SFT_CIX         equ 0x19            ; cluster relativo en caché
SFT_DIRC        equ 0x1B            ; propio: cluster del directorio
SFT_DIRI        equ 0x1D            ; propio: índice en el directorio
SFT_NAME        equ 0x20            ; nombre en formato FCB (11)
SFT_TYPE        equ 0x2B            ; propio (zona de SHARE): tipo
SFT_DRV         equ 0x2C            ; propio: unidad
SFT_FLAGS       equ 0x2D            ; propio
SFT_PSP         equ 0x31            ; PSP propietario
SFT_CCL         equ 0x35            ; cluster absoluto en caché
E_EXISTS        equ 80
E_CURDIR        equ 16
E_NOTSAME       equ 17
SFT_LEN         equ 0x3B
NSFT            equ 40

T_FREE          equ 0
T_CON           equ 1
T_FILE          equ 2
T_NUL           equ 3
T_DEV           equ 4

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
HIST_N          equ 10
HIST_LEN        equ 128
CDS_LEN         equ 0x58
NCDS            equ 26
DPB_LEN         equ 0x21
SDA_LEN         equ 0x80
%define indos     sda+0x01
%define last_err  sda+0x04
%define dta_off   sda+0x0C
%define dta_seg   sda+0x0E
%define cur_psp   sda+0x10
%define last_ret  sda+0x14
%define cur_drive sda+0x16
CFG_MAX         equ 4096
SET_MAX         equ 512
INST_MAX        equ 256
ENV_ROOT        equ 768

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
D_DRVHDR        equ 48
D_UNIT          equ 52
D_BPBPTR        equ 56
DF_DRIVER       equ 32
NEXT            equ 6
DRV_LEN         equ 64
NDRV            equ 8

DF_EXISTS       equ 1
DF_MOUNTED      equ 2
DF_LBA          equ 4
DF_FIXED        equ 8
DF_FAT16        equ 16
DF_ISO          equ 64
ISO_ND          equ 24

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
        call config_run
        call build_root_env
        call install_run

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
        ; ¿unidad sin particiones (superfloppy)? sector 0 = arranque FAT
        cmp word [secbuf+0x0B], 512
        jne .ptab
        mov al, [secbuf+0x0D]
        test al, al
        jz .ptab
        cmp byte [secbuf+0x15], 0xF0
        jb .ptab
        cmp byte [secbuf+0x10], 1
        jb .ptab
        cmp byte [secbuf+0x10], 2
        ja .ptab
        ; todo el disco es una unidad FAT
        mov si, sf_entry
        mov byte [si+4], 0x06
        mov dword [si+8], 0
        mov eax, [cur_drv+D_SIZE]
        mov [si+12], eax
        mov cx, 1
        mov al, 0x06
        jmp .add
.ptab:
        mov si, secbuf+0x1BE
        mov cx, 4
.part:
        ; entrada coherente: estado 00h/80h, inicio y tamaño distintos de 0
        mov al, [si]
        and al, 0x7F
        jnz .np
        cmp dword [si+8], 0
        je .np
        cmp dword [si+12], 0
        je .np
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
        mov al, [ndrives]
        mov [bios_nd], al
        call append_ext
        pop es
        popad
        ret

; añadir las unidades de los drivers de bloque tras las de la BIOS
append_ext:
        pushad
        push es
        ; unidades de los drivers de bloque de CONFIG.SYS
        mov si, ext_tab
        movzx cx, byte [ext_count]
        jcxz .noext
.ext:
        movzx bx, byte [ndrives]
        cmp bx, NDRV
        jae .noext
        shl bx, 6
        add bx, drv_tab
        mov byte [bx+D_FLAGS], DF_EXISTS | DF_DRIVER | DF_FIXED
        ; identificador propio para las cachés (no es una unidad de la BIOS)
        mov al, [ndrives]
        or al, 0xE0
        mov [bx+D_BIOS], al
        mov eax, [si]
        mov [bx+D_DRVHDR], eax
        mov al, [si+4]
        mov [bx+D_UNIT], al
        mov eax, [si+5]
        mov [bx+D_BPBPTR], eax
        push es
        push di
        les di, [si+5]
        movzx eax, word [es:di+8]
        test eax, eax
        jnz .t16
        mov eax, [es:di+21]
.t16:
        mov [bx+D_SIZE], eax
        mov ax, [es:di+13]
        mov [bx+D_SPT], ax
        mov ax, [es:di+15]
        mov [bx+D_HEADS], ax
        mov al, [es:di+10]
        mov [bx+D_MEDIA], al
        pop di
        pop es
        inc byte [ndrives]
        add si, 9
        loop .ext
.noext:
        mov al, [ndrives]
        mov [lol+0x20], al
        call cds_all
        pop es
        popad
        ret

; tras cargar un driver de bloque: solo sus unidades, sin volver a pedir
; nada a la BIOS (un driver USB nativo puede haber tomado el controlador)
drives_ext:
        pushad
        push es
        push ds
        pop es
        movzx ax, byte [bios_nd]
        mov [ndrives], al
        mov di, ax
        shl di, 6
        add di, drv_tab
        mov cx, NDRV
        sub cx, ax
        shl cx, 5
        xor ax, ax
        rep stosw
        pop es
        call append_ext
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
        mov [boot_letter], al
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
        mov byte [mnt_try], 0
        xor eax, eax
        mov bx, secbuf
        call read_sector
        mov dword [cache_lba], 0xFFFFFFFF
        jc .bad
.parse:
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
        mov al, [sel_drv]
        call cds_update
        pop es
        popad
        clc
        ret
.bad:
        ; drivers de bloque: usar el BPB que dio el driver
        test byte [cur_drv+D_FLAGS], DF_DRIVER
        jz .fail
        cmp byte [mnt_try], 0
        jne .fail
        mov byte [mnt_try], 1
        push ds
        lds si, [cur_drv+D_BPBPTR]
        mov di, secbuf+0x0B
        mov cx, 25
        rep movsb
        pop ds
        jmp .parse
.fail:
        test byte [cur_drv+D_FLAGS], DF_DRIVER
        jz .fail2
        call iso_mount
        jc .fail2
        pop es
        popad
        clc
        ret
.fail2:
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
        mov dword [sft_hdr], 0xFFFFFFFF ; un solo bloque
        mov word [sft_hdr+4], NSFT
        mov byte [sft_table+SFT_REF], 3
        mov byte [sft_table+SFT_TYPE], T_CON
        mov byte [sft_table+SFT_LEN+SFT_REF], 1
        mov byte [sft_table+SFT_LEN+SFT_TYPE], T_NUL
        mov byte [sft_table+2*SFT_LEN+SFT_REF], 1
        mov byte [sft_table+2*SFT_LEN+SFT_TYPE], T_NUL
        push di
        mov di, sft_table
        mov word [di+SFT_DPTR], dev_con
        mov word [di+SFT_DPTR+2], KSEG
        call sft_dev_fill
        mov di, sft_table + SFT_LEN
        mov word [di+SFT_DPTR], dev_nul
        mov word [di+SFT_DPTR+2], KSEG
        call sft_dev_fill
        mov di, sft_table + 2*SFT_LEN
        mov word [di+SFT_DPTR], dev_nul
        mov word [di+SFT_DPTR+2], KSEG
        call sft_dev_fill
        pop di
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
        cmp word [con_hdr+2], 0
        je con_out_bios
        push eax
        push bx
        push cx
        push dx
        mov [so_ch+1], al
        mov eax, [dc_hdr]
        push eax
        mov eax, [con_hdr]
        mov [dc_hdr], eax
        mov al, 8
        mov dx, ds
        mov bx, so_ch+1
        mov cx, 1
        call dev_rw
        pop eax
        mov [dc_hdr], eax
        pop dx
        pop cx
        pop bx
        pop eax
        ret
con_out_bios:
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

; esperar un carácter -> AL (driver CON si lo hay)
con_getc:
        cmp word [con_hdr+2], 0
        je con_getc_bios
        push bx
        push cx
        push dx
        mov eax, [con_hdr]
        mov [dc_hdr], eax
        mov al, 4
        mov dx, ds
        mov bx, so_ch+1
        mov cx, 1
        call dev_rw
        mov al, [so_ch+1]
        pop dx
        pop cx
        pop bx
        ret
con_getc_bios:
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
        mov byte [hist_pos], 0
.l:
        call con_getc
        test al, al
        jnz .nx
        ; tecla extendida: historial de órdenes (como DOSKEY)
        call con_getc
        cmp al, 0x48                ; flecha arriba
        je .up
        cmp al, 0x50                ; flecha abajo
        je .down
        cmp al, 0x3D                ; F3: repetir la última
        je .f3
        jmp .l
.up:
        mov al, [hist_pos]
        cmp al, [hist_count]
        jae .l
        inc byte [hist_pos]
        call .recall
        jmp .l
.down:
        cmp byte [hist_pos], 1
        jbe .dclr
        dec byte [hist_pos]
        call .recall
        jmp .l
.dclr:
        mov byte [hist_pos], 0
        call .clear
        jmp .l
.f3:
        cmp byte [hist_count], 0
        je .l
        mov byte [hist_pos], 1
        call .recall
        jmp .l
.nx:
        cmp al, 13
        je .cr
        cmp al, 8
        je .bs
        cmp al, 27
        je .esc
        cmp al, 3
        jne .nc
        cmp byte [indos], 0
        jne do_ctrlc
.nc:
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
; borrar la línea escrita
.clear:
        test bx, bx
        jz .cr0
        dec bx
        call .rub
        jmp .clear
.cr0:
        ret
; poner en la línea la entrada del historial hist_pos (1 = la última)
.recall:
        call .clear
        push si
        movzx si, byte [hist_head]
        movzx ax, byte [hist_pos]
        sub si, ax
        jns .rpos
        add si, HIST_N
.rpos:
        shl si, 7
        add si, hist_buf
        movzx dx, byte [si]         ; longitud
        inc si
.rc:
        test dx, dx
        jz .rd
        mov ax, bx
        inc ax
        cmp ax, cx
        jae .rd
        mov al, [si]
        mov [es:di+2+bx], al
        call con_out
        inc bx
        inc si
        dec dx
        jmp .rc
.rd:
        pop si
        ret
.cr:
        mov byte [es:di+2+bx], 13
        mov [es:di+1], bl
        call con_out
        call hist_save
.out:
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; guardar la línea ES:DI+2 (BX bytes) en el historial si no está vacía
; y no es igual que la última
hist_save:
        test bx, bx
        jz .r
        cmp bx, HIST_LEN - 1
        jbe .len
        mov bx, HIST_LEN - 1
.len:
        pusha
        ; ¿igual que la última?
        cmp byte [hist_count], 0
        je .new
        movzx si, byte [hist_head]
        dec si
        jns .p
        add si, HIST_N
.p:
        shl si, 7
        add si, hist_buf
        cmp [si], bl
        jne .new
        mov cx, bx
        push di
        add di, 2
        inc si
.cmp:
        mov al, [si]
        cmp al, [es:di]
        jne .diff
        inc si
        inc di
        loop .cmp
        pop di
        jmp .done
.diff:
        pop di
.new:
        movzx si, byte [hist_head]
        shl si, 7
        add si, hist_buf
        mov [si], bl
        inc si
        mov cx, bx
        push di
        add di, 2
.cp:
        mov al, [es:di]
        mov [si], al
        inc si
        inc di
        loop .cp
        pop di
        inc byte [hist_head]
        cmp byte [hist_head], HIST_N
        jb .h
        mov byte [hist_head], 0
.h:
        cmp byte [hist_count], HIST_N
        jae .done
        inc byte [hist_count]
.done:
        popa
.r:
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
;  Estructuras internas de DOS 4+: CDS (carpeta actual por unidad) y DPB
;  (parámetros de disco), enlazadas desde la lista de listas
; -----------------------------------------------------------------------------
; AL = unidad -> rellenar su DPB (si está montada) y su CDS
cds_update:
        pushad
        push es
        push ds
        pop es
        movzx bx, al
        mov [cu_drv], al
        ; DPB
        mov di, bx
        imul di, di, DPB_LEN
        add di, dpb_tab
        mov [di], al
        cmp bx, NDRV
        jae .nodpb
        push bx
        shl bx, 6
        add bx, drv_tab
        test byte [bx+D_FLAGS], DF_MOUNTED
        jz .dpbpart
        mov word [di+0x02], 512
        mov ax, [bx+D_SPCW]
        dec al
        mov [di+0x04], al
        ; desplazamiento = log2(sectores por clúster)
        inc al
        xor cl, cl
.lg:
        shr al, 1
        jz .lgd
        inc cl
        jmp .lg
.lgd:
        mov [di+0x05], cl
        mov ax, [bx+D_FATLBA]
        mov [di+0x06], ax
        mov ax, [bx+D_NFATS]
        mov [di+0x08], al
        mov ax, [bx+D_ROOTENT]
        mov [di+0x09], ax
        mov ax, [bx+D_DATALBA]
        mov [di+0x0B], ax
        mov ax, [bx+D_NCLUS]
        inc ax
        mov [di+0x0D], ax
        mov ax, [bx+D_SPF]
        mov [di+0x0F], ax
        mov ax, [bx+D_ROOTLBA]
        mov [di+0x11], ax
        mov word [di+0x1F], 0xFFFF
        mov ax, [bx+D_HINT]
        mov [di+0x1D], ax
.dpbpart:
        mov al, [bx+D_UNIT]
        mov [di+0x01], al
        mov word [di+0x13], dev_disk
        mov word [di+0x15], KSEG
        mov al, [bx+D_MEDIA]
        mov [di+0x17], al
        mov byte [di+0x18], 0
        pop bx
.nodpb:
        ; CDS
        mov di, bx
        imul di, di, CDS_LEN
        add di, cds_tab
        push di
        mov cx, CDS_LEN
        xor al, al
        rep stosb
        pop di
        cmp bx, NDRV
        jae .r
        push bx
        shl bx, 6
        add bx, drv_tab
        test byte [bx+D_FLAGS], DF_EXISTS
        pop bx
        jz .r
        mov al, bl
        add al, 'A'
        mov [di], al
        mov byte [di+1], ':'
        mov byte [di+2], '\'
        push di
        add di, 3
        mov si, bx
        imul si, si, PATHLEN
        add si, cur_paths
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        pop di
        mov word [di+0x43], 0x4000          ; unidad física
        mov ax, bx
        imul ax, ax, DPB_LEN
        add ax, dpb_tab
        mov [di+0x45], ax
        mov word [di+0x47], KSEG
        mov word [di+0x49], 0xFFFF
        mov dword [di+0x4B], 0xFFFFFFFF
        mov word [di+0x51], 2
.r:
        pop es
        popad
        ret

; rehacer todas las CDS y DPB y enlazar los DPB
cds_all:
        pushad
        xor al, al
.l:
        call cds_update
        inc al
        cmp al, NCDS
        jb .l
        ; cadena de DPB: solo las unidades que existen
        mov word [lol], 0xFFFF
        mov word [lol+2], 0xFFFF
        mov si, lol
        xor bx, bx
.c:
        cmp bx, NDRV
        jae .ce
        push bx
        shl bx, 6
        test byte [drv_tab+bx+D_FLAGS], DF_EXISTS
        pop bx
        jz .cn
        mov ax, bx
        imul ax, ax, DPB_LEN
        add ax, dpb_tab
        mov [si], ax
        mov word [si+2], KSEG
        mov si, ax
        add si, 0x19
        mov word [si], 0xFFFF
        mov word [si+2], 0xFFFF
.cn:
        inc bx
        jmp .c
.ce:
        popad
        ret

; ---- 1Fh / 32h: DPB de la unidad por defecto / de DL ----
fn_1f:
        mov byte [F_DL], 0
fn_32:
        mov al, [F_DL]
        test al, al
        jnz .d
        mov al, [cur_drive]
        inc al
.d:
        dec al
        call select_drive
        jc .bad
        call cds_update
        movzx bx, al
        imul bx, bx, DPB_LEN
        add bx, dpb_tab
        mov word [F_DS], KSEG
        mov [F_BX], bx
        mov byte [F_AL], 0
        ret
.bad:
        mov byte [F_AL], 0xFF
        ret

; -----------------------------------------------------------------------------
;  ISO 9660 (CD y DVD de datos), solo lectura
;  Los sectores de 2048 bytes llegan del driver como cuatro de 512.
;  Carpetas: "manejador" de 16 bits (0 = raíz) en una tabla de extensiones.
; -----------------------------------------------------------------------------
iso_deny:
        test byte [cur_drv+D_FLAGS], DF_ISO
        jz .ok
        mov ax, E_ACCESS
        stc
        ret
.ok:
        clc
        ret

; ¿la unidad seleccionada lleva ISO 9660? -> CF si no
iso_mount:
        pushad
        mov eax, 16 * 4             ; descriptor primario de volumen
        call cache_read
        jc .no
        cmp byte [secbuf], 1
        jne .no
        cmp dword [secbuf+1], 'CD00'
        jne .no
        cmp byte [secbuf+5], '1'
        jne .no
        mov eax, [secbuf+156+2]
        mov [cur_drv+D_ROOTLBA], eax
        mov eax, [secbuf+156+10]
        mov [cur_drv+D_DATALBA], eax
        mov eax, [secbuf+80]        ; bloques del volumen
        shl eax, 2
        mov [cur_drv+D_SIZE], eax
        and byte [cur_drv+D_FLAGS], 0xFF ^ DF_FAT16
        or byte [cur_drv+D_FLAGS], DF_ISO | DF_MOUNTED
        mov word [cur_drv+D_SPCW], 4
        mov word [cur_drv+D_BPC], 2048
        mov byte [isob_drv], 0xFF
        call drv_store
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; bloque de 2048 EAX de la unidad seleccionada -> isobuf
iso_block:
        mov dl, [sel_drv]
        cmp dl, [isob_drv]
        jne .rd
        cmp eax, [isob_lba]
        je .ok
.rd:
        pushad
        mov [isob_lba], eax
        mov [isob_drv], dl
        shl eax, 2
        mov di, isobuf
        mov cx, 4
.l:
        call cache_read
        jc .err
        push cx
        push es
        push ds
        pop es
        mov si, secbuf
        mov cx, 256
        rep movsw
        pop es
        pop cx
        inc eax
        loop .l
        popad
.ok:
        clc
        ret
.err:
        mov byte [isob_drv], 0xFF
        popad
        stc
        ret

; AX = manejador -> [ife_ext], [ife_size]
iso_dir:
        test ax, ax
        jnz .t
        mov eax, [cur_drv+D_ROOTLBA]
        mov [ife_ext], eax
        mov eax, [cur_drv+D_DATALBA]
        mov [ife_size], eax
        ret
.t:
        push bx
        movzx bx, al
        dec bx
        imul bx, bx, 9
        add bx, iso_dirs
        mov eax, [bx]
        mov [ife_ext], eax
        mov eax, [bx+4]
        mov [ife_size], eax
        pop bx
        ret

; EAX = extensión, EDX = tamaño -> AX = manejador
iso_handle:
        cmp eax, [cur_drv+D_ROOTLBA]
        jne .s
        xor ax, ax
        ret
.s:
        push bx
        push cx
        push si
        mov si, iso_dirs
        mov cx, 1
.l:
        cmp cx, ISO_ND
        ja .new
        cmp [si], eax
        jne .n
        push ax
        mov al, [sel_drv]
        cmp [si+8], al
        pop ax
        jne .n
        mov ax, cx
        jmp .r
.n:
        add si, 9
        inc cx
        jmp .l
.new:
        movzx cx, byte [iso_next]
        inc byte [iso_next]
        cmp byte [iso_next], ISO_ND
        jb .w
        mov byte [iso_next], 0
.w:
        mov si, cx
        imul si, si, 9
        add si, iso_dirs
        mov [si], eax
        mov [si+4], edx
        mov al, [sel_drv]
        mov [si+8], al
        mov ax, cx
        inc ax
.r:
        pop si
        pop cx
        pop bx
        ret

; como find_entry: AX = carpeta, CX = índice inicial, SI = patrón, DL = atrib.
iso_find_entry:
        push eax
        push bx
        push si
        push di
        push dx
        push es
        push ds
        pop es
        mov [ife_pat], si
        mov [ife_attr], dl
        mov [ife_start], cx
        ; etiqueta de volumen (solo con atributo 08h)
        cmp dl, 0x08
        jne .dir
        test cx, cx
        jnz .nf
        mov eax, 16 * 4
        call cache_read
        jc .nf
        mov si, secbuf+40
        mov di, fentry
        mov cx, 11
        rep movsb
        mov byte [fentry+11], 0x08
        xor cx, cx
        jmp .found
.dir:
        call iso_dir
        xor ebx, ebx                ; posición dentro de la carpeta
        mov word [ife_n], 0
.loop:
        cmp ebx, [ife_size]
        jae .nf
        mov eax, ebx
        shr eax, 11
        add eax, [ife_ext]
        call iso_block
        jc .nf
        mov si, bx
        and si, 2047
        add si, isobuf
        movzx ax, byte [si]
        test ax, ax
        jnz .rec
        ; relleno al final del bloque: saltar al siguiente
        or ebx, 2047
        inc ebx
        jmp .loop
.rec:
        movzx eax, ax
        add ebx, eax
        mov cx, [ife_n]
        inc word [ife_n]
        cmp cx, [ife_start]
        jb .loop
        call iso_record             ; SI = registro -> fentry
        ; filtro de atributos (como en FAT)
        mov al, [ife_attr]
        not al
        and al, [fentry+11]
        test al, 0x16
        jnz .loop
        ; nombre
        mov si, [ife_pat]
        mov di, fentry
        push cx
        mov cx, 11
.c:
        mov al, [si]
        cmp al, '?'
        je .ok
        cmp al, [di]
        jne .bad
.ok:
        inc si
        inc di
        loop .c
        pop cx
.found:
        pop es
        pop dx
        pop di
        pop si
        pop bx
        pop eax
        clc
        ret
.bad:
        pop cx
        jmp .loop
.nf:
        pop es
        pop dx
        pop di
        pop si
        pop bx
        pop eax
        stc
        ret

; registro de directorio ISO en SI -> fentry (nombre 8.3, atributos, fecha)
iso_record:
        push eax
        push bx
        push cx
        push si
        push di
        mov di, fentry
        mov cx, 16
        xor ax, ax
        rep stosw
        mov di, fentry
        mov cx, 11
        mov al, ' '
        rep stosb
        movzx cx, byte [si+32]
        lea bx, [si+33]
        cmp cx, 1
        jne .name
        cmp byte [bx], 1
        ja .name
        mov byte [fentry], '.'
        cmp byte [bx], 0
        je .attr
        mov byte [fentry+1], '.'
        jmp .attr
.name:
        mov di, fentry
        xor dx, dx                  ; columna dentro de la parte
        mov ah, 8
.n:
        jcxz .attr
        mov al, [bx]
        inc bx
        dec cx
        cmp al, ';'
        je .attr
        cmp al, '.'
        jne .ch
        mov di, fentry+8
        xor dx, dx
        mov ah, 3
        jmp .n
.ch:
        cmp al, 'a'
        jb .up
        cmp al, 'z'
        ja .up
        sub al, 32
.up:
        cmp dl, ah
        jae .n
        mov [di], al
        inc di
        inc dx
        jmp .n
.attr:
        mov al, 0x01                ; solo lectura
        test byte [si+25], 2
        jz .f
        mov al, 0x11                ; carpeta
.f:
        test byte [si+25], 1
        jz .h
        or al, 0x02                 ; oculto
.h:
        mov [fentry+11], al
        ; fecha y hora
        movzx ax, byte [si+18]      ; años desde 1900
        sub ax, 80
        jae .y
        xor ax, ax
.y:
        shl ax, 4
        or al, [si+19]
        shl ax, 5
        or al, [si+20]
        mov [fentry+24], ax
        movzx ax, byte [si+21]
        shl ax, 6
        or al, [si+22]
        shl ax, 5
        mov cl, [si+23]
        shr cl, 1
        or al, cl
        mov [fentry+22], ax
        ; extensión y tamaño
        mov eax, [si+2]
        mov edx, [si+10]
        mov [fentry+28], edx
        test byte [fentry+11], 0x10
        jz .file
        call iso_handle
        mov [fentry+26], ax
        mov dword [fentry+28], 0
        jmp .r
.file:
        mov [fentry+26], ax
        shr eax, 16
        mov [fentry+20], ax
.r:
        pop di
        pop si
        pop cx
        pop bx
        pop eax
        ret

; leer de un fichero ISO (contiguo); mismo contrato que el bucle de file_read
iso_file_read:
.loop:
        cmp word [fr_left], 0
        je .out
        mov eax, [di+SFT_SIZE]
        sub eax, [di+SFT_POS]
        jbe .out
        push eax
        movzx eax, word [di+SFT_CCL]
        shl eax, 16
        mov ax, [di+SFT_CLUS]
        shl eax, 2                  ; bloques de 2048 -> sectores de 512
        mov edx, [di+SFT_POS]
        shr edx, 9
        add eax, edx
        call cache_read
        pop eax
        jc .out
        mov si, [di+SFT_POS]
        and si, 511
        mov cx, 512
        sub cx, si
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
.out:
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
        test byte [cur_drv+D_FLAGS], DF_ISO
        jz disk_io
        stc                         ; un CD/DVD no se puede escribir
        ret
disk_io:
        test byte [cur_drv+D_FLAGS], DF_DRIVER
        jz .bios
        jmp drv_block_io
.bios:
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
        test byte [cur_drv+D_FLAGS], DF_ISO
        jz .fat
        jmp iso_find_entry
.fat:
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
        imul di, di, SFT_LEN
        add di, sft_table
        cmp byte [di+SFT_REF], 0
        je .bad
        clc
        ret
.bad:
        mov ax, E_HANDLE
        stc
        ret

; formato DOS de una entrada de dispositivo DI (tipo en SFT_TYPE, driver en
; SFT_DPTR): información de dispositivo, nombre y propietario
sft_dev_fill:
        push ax
        push si
        push cx
        push es
        mov ax, [cur_psp]
        mov [di+SFT_PSP], ax
        mov ax, 0x80D3              ; CON
        cmp byte [di+SFT_TYPE], T_CON
        je .s
        mov ax, 0x8084              ; NUL
        cmp byte [di+SFT_TYPE], T_NUL
        je .s
        les si, [di+SFT_DPTR]
        mov ax, [es:si+4]
        and ax, 0x001F
        or ax, 0x00C0
.s:
        mov [di+SFT_DEVI], ax
        ; nombre del driver (8) + 3 espacios
        les si, [di+SFT_DPTR]
        add si, 10
        mov cx, 8
        push di
        add di, SFT_NAME
.n:
        mov al, [es:si]
        mov [di], al
        inc si
        inc di
        loop .n
        mov word [di], '  '
        mov byte [di+2], ' '
        pop di
        pop es
        pop cx
        pop si
        pop ax
        ret

; DI = entrada de la SFT -> AX = índice
sft_index:
        push dx
        push cx
        mov ax, di
        sub ax, sft_table
        xor dx, dx
        mov cx, SFT_LEN
        div cx
        pop cx
        pop dx
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
        test byte [cur_drv+D_FLAGS], DF_ISO
        jz .loop
        call iso_file_read
        jmp .out
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
        ; formato DOS: unidad + "no escrito", DPB, nombre y propietario
        movzx ax, al
        or al, 0x40
        mov [di+SFT_DEVI], ax
        movzx ax, byte [sel_drv]
        imul ax, ax, DPB_LEN
        add ax, dpb_tab
        mov [di+SFT_DPTR], ax
        mov word [di+SFT_DPTR+2], KSEG
        mov ax, [cur_psp]
        mov [di+SFT_PSP], ax
        push si
        push di
        push cx
        push es
        push ds
        pop es
        mov si, fentry
        add di, SFT_NAME
        mov cx, 11
        rep movsb
        pop es
        pop cx
        pop di
        pop si
        mov byte [di+SFT_TYPE], T_FILE
        mov al, [fentry+11]
        mov [di+SFT_ATTR], al
        mov ax, [fentry+26]
        mov [di+SFT_CLUS], ax
        mov ax, [fentry+20]         ; ISO 9660: parte alta del sector inicial
        mov [di+SFT_CCL], ax
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
        mov eax, [cd_hdr]
        mov [di+SFT_SIZE], eax
        mov [di+SFT_DPTR], eax
        call sft_dev_fill
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
        call iso_deny
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
        mov eax, [cd_hdr]
        mov [di+SFT_SIZE], eax
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
        cmp ah, T_DEV
        je .dev
        call con_out
        jmp .r
.dev:
        mov [so_ch], al
        mov eax, [di+SFT_SIZE]
        mov [dc_hdr], eax
        mov al, 8
        mov dx, ds
        mov bx, so_ch
        mov cx, 1
        call dev_rw
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

; -----------------------------------------------------------------------------
;  Drivers de dispositivo: cabeceras, peticiones y cadena
; -----------------------------------------------------------------------------
; llamar al driver [dc_hdr] con la petición KSEG:dreq
dev_call:
        pushad
        push ds
        push es
        les bx, [dc_hdr]
        mov ax, [es:bx+6]
        mov [dc_far], ax
        mov [dc_far+2], es
        mov ax, [es:bx+8]
        mov [dc_far2], ax
        mov [dc_far2+2], es
        mov ax, KSEG
        mov es, ax
        mov bx, dreq
        call far [dc_far]
        mov ax, KSEG
        mov ds, ax
        mov es, ax
        mov bx, dreq
        call far [dc_far2]
        mov ax, KSEG
        mov ds, ax
        pop es
        pop ds
        popad
        ret

; AL = orden, DX:BX = búfer, CX = cuenta -> AX = transferido; CF si error
dev_rw:
        push di
        push es
        push ds
        pop es
        push ax
        push cx
        mov di, dreq
        mov cx, 15
        xor ax, ax
        rep stosw
        pop cx
        pop ax
        mov byte [dreq], 0x16
        mov [dreq+2], al
        mov [dreq+0x0E], bx
        mov [dreq+0x10], dx
        mov [dreq+0x12], cx
        call dev_call
        pop es
        pop di
        mov ax, [dreq+0x12]
        test word [dreq+3], 0x8000
        jz .ok
        stc
        ret
.ok:
        clc
        ret

; E/S de sector por un driver de bloque (EAX, ES:BX como disk_io)
drv_block_io:
        pushad
        mov ecx, [cur_drv+D_DRVHDR]
        mov [dc_hdr], ecx
        push es
        push ds
        pop es
        push eax
        mov di, dreq
        mov cx, 15
        xor ax, ax
        rep stosw
        pop eax
        pop es
        mov byte [dreq], 0x1E
        mov cl, [cur_drv+D_UNIT]
        mov [dreq+1], cl
        mov cl, [io_cmd]
        cmp cl, 2
        mov cl, 4
        je .rd
        mov cl, 8
.rd:
        mov [dreq+2], cl
        mov cl, [cur_drv+D_MEDIA]
        mov [dreq+0x0D], cl
        mov [dreq+0x0E], bx
        mov [dreq+0x10], es
        mov word [dreq+0x12], 1
        cmp eax, 0xFFFF
        jae .big
        mov [dreq+0x14], ax
        jmp .go
.big:
        mov word [dreq+0x14], 0xFFFF
        mov [dreq+0x1A], eax
.go:
        call dev_call
        test word [dreq+3], 0x8000
        jnz .err
        popad
        clc
        ret
.err:
        popad
        stc
        ret

; rutinas de los dispositivos propios del núcleo
dev_strat:
        mov [cs:dev_reqp], bx
        mov [cs:dev_reqp+2], es
        retf
dev_intr_null:
        push ds
        push bx
        lds bx, [cs:dev_reqp]
        cmp byte [bx+2], 4
        jne .d
        mov word [bx+0x12], 0
.d:
        mov word [bx+3], 0x0100
        pop bx
        pop ds
        retf
dev_intr_con:
        pushad
        push ds
        push es
        mov ax, KSEG
        mov ds, ax
        les bx, [dev_reqp]
        mov al, [es:bx+2]
        mov word [es:bx+3], 0x0100
        cmp al, 4
        je .read
        cmp al, 5
        je .peek
        cmp al, 8
        je .write
        cmp al, 9
        je .write
        jmp .r
.read:
        mov cx, [es:bx+0x12]
        push es
        push bx
        les di, [es:bx+0x0E]
.rl:
        jcxz .rd
        call con_getc_bios
        stosb
        dec cx
        jmp .rl
.rd:
        pop bx
        pop es
        jmp .r
.peek:
        call con_check
        jz .busy
        mov al, [peek_ch]
        mov [es:bx+0x0D], al
        jmp .r
.busy:
        mov word [es:bx+3], 0x0300
        jmp .r
.write:
        mov cx, [es:bx+0x12]
        push ds
        lds si, [es:bx+0x0E]
.wl:
        jcxz .wd
        lodsb
        push ds
        push ax
        mov ax, KSEG
        mov ds, ax
        pop ax
        call con_out_bios
        pop ds
        dec cx
        jmp .wl
.wd:
        pop ds
.r:
        pop es
        pop ds
        popad
        retf

; ¿w_name es un dispositivo de la cadena? CF=1: AL = tipo, cd_hdr = cabecera
check_device:
        push si
        push di
        push cx
        push es
        mov word [cd_hdr], dev_nul
        mov word [cd_hdr+2], KSEG
.l:
        les di, [cd_hdr]
        lea di, [di+10]
        mov si, w_name
        mov cx, 8
        repe cmpsb
        je .yes
        les di, [cd_hdr]
        mov eax, [es:di]
        cmp ax, 0xFFFF
        je .no
        mov [cd_hdr], eax
        jmp .l
.yes:
        les di, [cd_hdr]
        mov ax, es
        cmp ax, KSEG
        jne .ext
        mov al, T_NUL
        cmp di, dev_con
        jne .t
        mov al, T_CON
        jmp .t
.ext:
        mov al, T_DEV
.t:
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

; -----------------------------------------------------------------------------
;  CONFIG.SYS
; -----------------------------------------------------------------------------
config_run:
        mov byte [lastdrive], 5
        mov al, [cur_drive]
        add al, 'A'
        mov [cfg_path], al
        push ds
        pop es
        mov byte [cfgbuf], 0
        mov si, cfg_path
        call open_path
        jc .choose
        mov byte [di+SFT_REF], 0
        cmp byte [di+SFT_TYPE], T_FILE
        jne .choose
        mov bx, cfgbuf
        mov cx, CFG_MAX - 1
        call file_read
        mov bx, ax
        mov byte [cfgbuf+bx], 0
.choose:
        call boot_choice            ; F5 / F8
        cmp byte [boot_clean], 0
        jne .none
        mov word [cfg_line], 0
        mov byte [cfg_inmenu], 0
        mov si, cfgbuf
.line:
        cmp byte [si], 0
        je .done
        inc word [cfg_line]
        ; copiar la línea en cfgl (terminada en CR LF 0)
        mov di, cfgl
.cl:
        lodsb
        test al, al
        jz .ce0
        cmp al, 10
        je .ce
        cmp al, 13
        je .cl
        cmp al, 0x1A
        je .ce0
        cmp di, cfgl + 160
        jae .cl
        mov [di], al
        inc di
        jmp .cl
.ce0:
        dec si
.ce:
        mov word [di], 0x0A0D
        mov byte [di+2], 0
        ; ¿desactivada en el menú de arranque?
        mov bx, [cfg_line]
        cmp bx, MAXCL
        jae .on
        cmp byte [cfg_dis+bx], 0
        jne .line
.on:
        push si
        call config_line
        pop si
        jmp .line
.done:
.none:
        ret


; =============================================================================
;  Arranque: F5 = limpio (sin CONFIG.SYS ni AUTOEXEC.BAT), F8 = menú propio.
;  SWITCHES=/N las desactiva y SWITCHES=/F quita la espera, como en MS-DOS.
; =============================================================================
MAXCL       equ 64                  ; líneas de CONFIG.SYS que se pueden desactivar
MAXIT       equ 16                  ; elementos visibles del menú
BOOT_WAIT   equ 27                  ; ~1,5 s

boot_choice:
        mov byte [boot_clean], 0
        mov byte [boot_noauto], 0
        call cfg_switches
        test byte [sw_flags], 2     ; /N
        jnz .r
        mov si, msg_bootkeys
        call kputs
        mov cx, BOOT_WAIT
        test byte [sw_flags], 1     ; /F
        jz .w
        mov cx, 1
.w:
        push es
        xor ax, ax
        mov es, ax
        mov dx, [es:0x46C]
        pop es
.l:
        mov ah, 1
        int 0x16
        jnz .key
        ; ¿llega algo por el puerto serie? (no esperar más)
        cmp byte [ser_ok], 0
        je .t
        push dx
        mov dx, 0x3FD
        in al, dx
        pop dx
        test al, 1
        jnz .r
.t:
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        pop es
        sub ax, dx
        cmp ax, cx
        jb .l
.r:
        ret
.key:
        cmp ah, 0x3F                ; F5
        je .f5
        cmp ah, 0x42                ; F8
        jne .r                      ; otra tecla: se queda para después
        xor ah, ah
        int 0x16
        jmp boot_menu
.f5:
        xor ah, ah
        int 0x16
        mov byte [boot_clean], 1
        call no_autoexec
        mov si, msg_clean
        jmp kputs

; añadir /D a la línea del intérprete: no ejecutará AUTOEXEC.BAT
no_autoexec:
        cmp byte [boot_noauto], 0
        jne .r
        mov byte [boot_noauto], 1
        movzx bx, byte [shell_tail]
        cmp bx, 120
        ja .r
        mov dword [shell_tail+1+bx], 0x0D442F20     ; " /D", CR
        add byte [shell_tail], 3
.r:
        ret

; buscar SWITCHES= en CONFIG.SYS: /F -> bit 0, /N -> bit 1
cfg_switches:
        mov byte [sw_flags], 0
        mov si, cfgbuf
.line:
        call cfg_skip
        mov eax, [si]
        and eax, 0xDFDFDFDF
        cmp eax, 'SWIT'
        jne .next
        mov eax, [si+4]
        and eax, 0xDFDFDFDF
        cmp eax, 'CHES'
        jne .next
.o:
        lodsb
        cmp al, 13
        je .line
        cmp al, 10
        je .line
        test al, al
        jz .r
        cmp al, '/'
        jne .o
        mov al, [si]
        and al, 0xDF
        cmp al, 'F'
        jne .on
        or byte [sw_flags], 1
.on:
        cmp al, 'N'
        jne .o
        or byte [sw_flags], 2
        jmp .o
.next:
        lodsb
        test al, al
        jz .r
        cmp al, 10
        jne .next
        jmp .line
.r:
        ret

; ---- menú F8 ----
boot_menu:
        ; líneas de CONFIG.SYS que se muestran (ni vacías, ni REM, ni secciones)
        xor cx, cx                  ; elementos
        mov si, cfgbuf
        mov word [bm_num], 0
.scan:
        cmp byte [si], 0
        je .sd
        inc word [bm_num]
        push si
        call cfg_skip
        mov al, [si]
        cmp al, 13
        je .skip
        cmp al, 10
        je .skip
        cmp al, ';'
        je .skip
        cmp al, '['
        je .skip
        mov eax, [si]
        and eax, 0x00DFDFDF
        cmp eax, 'REM'
        je .skip
        cmp cx, MAXIT - 1
        jae .skip
        cmp word [bm_num], MAXCL
        jae .skip
        mov bx, cx
        shl bx, 1
        mov [bm_off+bx], si
        mov ax, [bm_num]
        mov [bm_line+bx], ax
        inc cx
.skip:
        pop si
.eol:
        lodsb
        test al, al
        jz .sd0
        cmp al, 10
        jne .eol
        jmp .scan
.sd0:
        dec si
.sd:
        mov [bm_n], cx              ; + AUTOEXEC.BAT al final
        mov word [bm_sel], 0
        mov ax, 0x0003
        int 0x10
        mov ah, 1                   ; sin cursor
        mov cx, 0x2000
        int 0x10
.draw:
        call bm_draw
        xor ah, ah
        int 0x16
        cmp ah, 0x48
        je .up
        cmp ah, 0x50
        je .down
        cmp al, ' '
        je .toggle
        cmp al, 13
        je .go
        cmp ah, 0x3F                ; F5: limpio
        je .clean
        cmp al, 27                  ; Esc: normal (todo activado)
        je .normal
        jmp .draw
.up:
        cmp word [bm_sel], 0
        je .draw
        dec word [bm_sel]
        jmp .draw
.down:
        mov ax, [bm_sel]
        cmp ax, [bm_n]
        jae .draw
        inc word [bm_sel]
        jmp .draw
.toggle:
        mov bx, [bm_sel]
        cmp bx, [bm_n]
        je .ta
        shl bx, 1
        mov bx, [bm_line+bx]
        xor byte [cfg_dis+bx], 1
        jmp .draw
.ta:
        xor byte [bm_noauto], 1
        jmp .draw
.clean:
        mov byte [boot_clean], 1
        call no_autoexec
        jmp .end
.normal:
        push di
        push es
        push ds
        pop es
        mov di, cfg_dis
        mov cx, MAXCL
        xor al, al
        rep stosb
        pop es
        pop di
        mov byte [bm_noauto], 0
.go:
        cmp byte [bm_noauto], 0
        je .end
        call no_autoexec
.end:
        mov ax, 0x0003
        int 0x10
        cmp byte [boot_clean], 0
        je .r
        mov si, msg_clean
        call kputs
.r:
        ret

; dibujar el menú directamente en la memoria de vídeo
BM_TITLE    equ 0x4E                ; amarillo sobre rojo
BM_TEXT     equ 0x07
BM_ON       equ 0x0F
BM_OFF      equ 0x08
BM_HEAD     equ 0x0E
BM_SEL      equ 0x1F
BM_INFO     equ 0x0B
BM_KEYS     equ 0x70
bm_draw:
        push es
        mov ax, 0xB800
        mov es, ax
        ; fondo
        xor di, di
        mov cx, 80*25
        mov ax, (BM_TEXT << 8) | ' '
        rep stosw
        ; título
        xor di, di
        mov cx, 80
        mov ax, (BM_TITLE << 8) | ' '
        rep stosw
        mov di, 2
        mov ah, BM_TITLE
        mov si, bm_t1
        call bm_puts
        ; cabeceras
        mov di, (2*80 + 2) * 2
        mov ah, BM_HEAD
        mov si, bm_hcfg
        call bm_puts
        mov di, (2*80 + 56) * 2
        mov si, bm_hmem
        call bm_puts
        ; panel derecho: memoria
        mov di, (4*80 + 56) * 2
        mov ah, BM_TEXT
        mov si, bm_conv
        call bm_puts
        int 0x12
        movzx eax, ax
        mov di, (4*80 + 69) * 2
        call bm_kb
        mov di, (5*80 + 56) * 2
        mov si, bm_ext
        call bm_puts
        call bm_extkb
        mov di, (5*80 + 69) * 2
        call bm_kb
        mov di, (6*80 + 56) * 2
        mov si, bm_upp
        call bm_puts
        xor eax, eax
        mov di, (6*80 + 69) * 2
        call bm_kb
        ; línea vertical
        mov di, (2*80 + 53) * 2
        mov cx, 21
.vl:
        mov word [es:di], (BM_OFF << 8) | 0xB3
        add di, 160
        loop .vl
        ; elementos
        xor cx, cx
        mov word [bm_count], 0
.it:
        cmp cx, [bm_n]
        ja .ite
        mov ax, cx
        add ax, 4
        cmp cx, [bm_n]
        jne .row
        inc ax                      ; AUTOEXEC.BAT: una fila más abajo
        push ax
        mov di, ax
        dec di
        imul di, di, 160
        add di, 4
        push cx
        mov ah, BM_HEAD
        mov si, bm_hauto
        call bm_puts
        pop cx
        pop ax
.row:
        imul di, ax, 160
        add di, 4
        ; activado / desactivado
        mov bl, 1
        cmp cx, [bm_n]
        je .ia
        mov bx, cx
        shl bx, 1
        mov bx, [bm_line+bx]
        mov bl, [cfg_dis+bx]
        xor bl, 1
        jmp .ib
.ia:
        mov bl, [bm_noauto]
        xor bl, 1
.ib:
        mov ah, BM_ON
        test bl, bl
        jnz .ic
        mov ah, BM_OFF
.ic:
        test bl, bl
        jz .id
        inc word [bm_count]
.id:
        cmp cx, [bm_sel]
        jne .ie
        mov ah, BM_SEL
        ; barra de selección
        push di
        push cx
        push ax
        sub di, 2
        mov cx, 50
        mov al, ' '
        rep stosw
        pop ax
        pop cx
        pop di
.ie:
        mov al, '['
        stosw
        mov al, 0xFE                ; ■
        test bl, bl
        jnz .if
        mov al, ' '
.if:
        stosw
        mov al, ']'
        stosw
        mov al, ' '
        stosw
        ; texto
        push cx
        cmp cx, [bm_n]
        je .ta
        mov bx, cx
        shl bx, 1
        mov si, [bm_off+bx]
        mov cx, 45
.tl:
        mov al, [si]
        cmp al, 13
        je .tle
        cmp al, 10
        je .tle
        test al, al
        jz .tle
        cmp al, 9
        jne .tc
        mov al, ' '
.tc:
        stosw
        inc si
        loop .tl
.tle:
        pop cx
        jmp .nx
.ta:
        mov si, bm_tauto
        call bm_puts
        pop cx
.nx:
        inc cx
        jmp .it
.ite:
        ; descripción de la línea elegida
        mov di, (9*80 + 56) * 2
        mov ah, BM_HEAD
        mov si, bm_hdesc
        call bm_puts
        call bm_desc                ; SI = descripción
        mov di, (11*80 + 56) * 2
        mov ah, BM_INFO
        call bm_wrap
        ; resumen
        mov di, (22*80 + 2) * 2
        mov ah, 0x0A
        mov si, bm_s1
        call bm_puts
        mov ax, [bm_count]
        add al, '0'
        cmp ax, '9'
        jbe .n1
        mov al, '+'
.n1:
        mov ah, 0x0A
        stosw
        mov si, bm_s2
        call bm_puts
        mov ax, [bm_n]
        inc ax
        add al, '0'
        cmp al, '9'
        jbe .n2
        mov al, '+'
.n2:
        mov ah, 0x0A
        stosw
        mov si, bm_s3
        call bm_puts
        ; teclas
        mov di, 24*160
        mov cx, 80
        mov ax, (BM_KEYS << 8) | ' '
        rep stosw
        mov di, 24*160 + 2
        mov ah, BM_KEYS
        mov si, bm_keys
        call bm_puts
        pop es
        ret

; SI = texto (0) -> ES:DI con atributo AH
bm_puts:
        lodsb
        test al, al
        jz .r
        stosw
        jmp bm_puts
.r:
        ret
; SI = texto -> líneas de 23 columnas desde ES:DI (columna 56)
bm_wrap:
        mov cx, 23
.l:
        lodsb
        test al, al
        jz .r
        cmp al, '|'                 ; salto de línea
        je .nl
        stosw
        loop .l
.nl:
        add di, 160 - 46
        add di, cx
        add di, cx
        cmp byte [si], '|'          ; línea llena justo antes de un salto
        jne bm_wrap
        cmp cx, 0
        jne bm_wrap
        inc si
        jmp bm_wrap
.r:
        ret

; EAX KB -> "31.552 KB" alineado a la derecha (8 columnas) en ES:DI
bm_kb:
        push ebx
        push ecx
        push edx
        push si
        mov si, bm_num_e
        mov ebx, 10
        xor cl, cl
.d:
        xor edx, edx
        div ebx
        cmp cl, 3
        jne .nc
        dec si
        mov byte [si], '.'
        xor cl, cl
.nc:
        add dl, '0'
        dec si
        mov [si], dl
        inc cl
        test eax, eax
        jnz .d
        ; relleno
        mov cx, bm_num_e
        sub cx, si
        neg cx
        add cx, 7
        jle .p
        mov ax, (BM_TEXT << 8) | ' '
        rep stosw
.p:
        mov ah, BM_ON
        call bm_puts
        mov ah, BM_TEXT
        mov si, bm_kbs
        call bm_puts
        pop si
        pop edx
        pop ecx
        pop ebx
        ret

; memoria extendida de la BIOS (KB) -> EAX
bm_extkb:
        push ebx
        push ecx
        push edx
        mov ax, 0xE801
        int 0x15
        jc .old
        test ax, ax
        jnz .e
        mov ax, cx
        mov bx, dx
.e:
        movzx eax, ax
        movzx ebx, bx
        shl ebx, 6
        add eax, ebx
        jmp .r
.old:
        mov ah, 0x88
        int 0x15
        movzx eax, ax
.r:
        pop edx
        pop ecx
        pop ebx
        ret

; descripción de la línea elegida -> SI
bm_desc:
        push es
        push di
        push ds
        pop es
        call .d
        pop di
        pop es
        ret
.d:
        mov bx, [bm_sel]
        cmp bx, [bm_n]
        jne .c
        mov si, d_auto
        ret
.c:
        shl bx, 1
        mov si, [bm_off+bx]
        call cfg_skip
        ; palabra clave en mayúsculas en bm_key (8)
        mov di, bm_key
        mov cx, 8
.k:
        mov al, [si]
        cmp al, 'a'
        jb .k1
        cmp al, 'z'
        ja .k1
        sub al, 32
.k1:
        mov [di], al
        inc si
        inc di
        loop .k
        mov bx, d_tab
.f:
        mov si, [bx]
        test si, si
        jz .other
        mov di, bm_key
        mov cx, [bx+2]
        push si
        repe cmpsb
        pop si
        je .hit
        add bx, 6
        jmp .f
.hit:
        mov si, [bx+4]
        cmp si, d_device
        jne .r
        call bm_drvdesc
.r:
        ret
.other:
        mov si, d_other
        ret

; DEVICE=: según el nombre del driver
bm_drvdesc:
        mov bx, [bm_sel]
        shl bx, 1
        mov si, [bm_off+bx]
.l:
        mov al, [si]
        cmp al, 13
        je .gen
        test al, al
        jz .gen
        mov eax, [si]
        and eax, 0xDFDFDFDF
        cmp eax, 'HSPX'
        je .xms
        cmp eax, 'HSPC'
        je .cd
        cmp eax, 'HSPU'
        je .usb
        cmp eax, 'HSPR'
        je .ram
        inc si
        jmp .l
.xms:
        mov si, d_xms
        ret
.cd:
        mov si, d_cd
        ret
.usb:
        mov si, d_usb
        ret
.ram:
        mov si, d_ram
        ret
.gen:
        mov si, d_device
        ret

; una línea de CONFIG.SYS en cfgl
config_line:
        mov si, cfgl
        call cfg_skip
        mov al, [si]
        cmp al, 13
        je .r
        cmp al, ';'
        je .r
        cmp al, '['
        je .section
        cmp byte [cfg_inmenu], 0
        jne .r
        ; palabra clave
        mov di, cfg_key
        mov cx, 15
.k:
        mov al, [si]
        cmp al, '='
        je .ke
        cmp al, ' '
        je .ke
        cmp al, 9
        je .ke
        cmp al, 13
        je .ke
        cmp al, 'a'
        jb .k1
        cmp al, 'z'
        ja .k1
        sub al, 32
.k1:
        mov [di], al
        inc di
        inc si
        loop .k
.ke:
        mov byte [di], 0
        call cfg_skip
        cmp byte [si], '='
        jne .nv
        inc si
.nv:
        call cfg_skip
        mov [cfg_val], si
        ; buscar en la tabla
        mov bx, cfg_cmds
.find:
        mov di, [bx]
        test di, di
        jz .unk
        push si
        mov si, cfg_key
.c:
        mov al, [si]
        cmp al, [di]
        jne .n
        test al, al
        jz .hit
        inc si
        inc di
        jmp .c
.n:
        pop si
        add bx, 4
        jmp .find
.hit:
        pop si
        jmp [bx+2]
.unk:
        mov si, msg_cfgunk
        call kputs
        call cfg_lineerr
.r:
        ret
.section:
        ; [MENU] no se admite: sus líneas se ignoran; las demás secciones se procesan
        mov byte [cfg_inmenu], 0
        cmp dword [si+1], 'MENU'
        je .menu
        cmp dword [si+1], 'menu'
        jne .r
.menu:
        mov byte [cfg_inmenu], 1
        ret

cfg_skip:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp cfg_skip
.r:
        ret

cfg_lineerr:
        mov si, msg_cfgline
        call kputs
        movzx eax, word [cfg_line]
        call kputnum
        mov si, msg_crlf
        jmp kputs

; EAX -> decimal por la consola
kputnum:
        push eax
        push ebx
        push edx
        push cx
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
        call con_out
        loop .p
        pop cx
        pop edx
        pop ebx
        pop eax
        ret

cfg_cmds:
        dw k_device, cfg_device
        dw k_devhigh, cfg_device
        dw k_install, cfg_install
        dw k_insthigh, cfg_install
        dw k_shell, cfg_shell
        dw k_set, cfg_set
        dw k_lastdrive, cfg_lastdrive
        dw k_rem, cfg_ignore
        dw k_files, cfg_ignore
        dw k_buffers, cfg_ignore
        dw k_stacks, cfg_ignore
        dw k_fcbs, cfg_ignore
        dw k_break, cfg_ignore
        dw k_dos, cfg_ignore
        dw k_country, cfg_ignore
        dw k_numlock, cfg_numlock
        dw k_switches, cfg_ignore
        dw k_menuitem, cfg_ignore
        dw k_menudef, cfg_ignore
        dw k_include, cfg_ignore
        dw 0, 0
k_device    db "DEVICE", 0
k_devhigh   db "DEVICEHIGH", 0
k_install   db "INSTALL", 0
k_insthigh  db "INSTALLHIGH", 0
k_shell     db "SHELL", 0
k_set       db "SET", 0
k_lastdrive db "LASTDRIVE", 0
k_rem       db "REM", 0
k_files     db "FILES", 0
k_buffers   db "BUFFERS", 0
k_stacks    db "STACKS", 0
k_fcbs      db "FCBS", 0
k_break     db "BREAK", 0
k_dos       db "DOS", 0
k_country   db "COUNTRY", 0
k_numlock   db "NUMLOCK", 0
k_switches  db "SWITCHES", 0
k_menuitem  db "MENUITEM", 0
k_menudef   db "MENUDEFAULT", 0
k_include   db "INCLUDE", 0

cfg_ignore:
        ret

cfg_lastdrive:
        mov si, [cfg_val]
        mov al, [si]
        and al, 0xDF
        sub al, 'A' - 1
        cmp al, 1
        jb .r
        cmp al, 26
        ja .r
        mov [lastdrive], al
.r:
        ret

; NUMLOCK=ON|OFF: Bloq Num al arrancar (la BIOS enciende la luz en la
; siguiente llamada a INT 16h)
cfg_numlock:
        mov si, [cfg_val]
        mov al, [si+1]
        and al, 0xDF
        push es
        push word 0x40
        pop es
        cmp al, 'N'                 ; ON
        jne .off
        or byte [es:0x17], 0x20
        jmp .r
.off:
        cmp al, 'F'                 ; OFF
        jne .r
        and byte [es:0x17], 0xDF
.r:
        pop es
        ret

; SET NOMBRE=valor -> setbuf
cfg_set:
        mov si, [cfg_val]
        mov di, [set_end]
        test di, di
        jnz .h
        mov di, setbuf
.h:
        mov bx, di
.n:
        lodsb
        cmp al, 13
        je .bad
        cmp al, '='
        je .eq
        cmp al, 'a'
        jb .n1
        cmp al, 'z'
        ja .n1
        sub al, 32
.n1:
        cmp di, setbuf + SET_MAX - 2
        jae .bad
        mov [di], al
        inc di
        jmp .n
.eq:
        mov byte [di], '='
        inc di
.v:
        lodsb
        cmp al, 13
        je .ve
        cmp di, setbuf + SET_MAX - 2
        jae .bad
        mov [di], al
        inc di
        jmp .v
.ve:
        mov byte [di], 0
        inc di
        mov [set_end], di
        ret
.bad:
        mov [set_end], bx
        ret

; SHELL=ruta argumentos
cfg_shell:
        mov si, [cfg_val]
        mov di, shell_path
        mov cx, 63
.p:
        lodsb
        cmp al, ' '
        je .pe
        cmp al, 13
        je .pe0
        mov [di], al
        inc di
        loop .p
.pe0:
        dec si
.pe:
        mov byte [di], 0
        ; argumentos
        mov di, shell_tail + 1
        xor cx, cx
.a:
        lodsb
        cmp al, 13
        je .ae
        cmp cx, 120
        jae .a
        mov [di], al
        inc di
        inc cx
        jmp .a
.ae:
        cmp cx, 0
        je .noargs
        ; DOS pasa la cola con un espacio delante
        mov byte [di], 13
        mov [shell_tail], cl
        ret
.noargs:
        mov byte [shell_tail], 0
        mov byte [shell_tail+1], 13
        ret

; INSTALL=programa argumentos -> se ejecutan tras los drivers
cfg_install:
        mov si, [cfg_val]
        mov di, [inst_end]
        test di, di
        jnz .h
        mov di, instbuf
.h:
.c:
        lodsb
        cmp al, 13
        je .e
        cmp di, instbuf + INST_MAX - 2
        jae .e
        mov [di], al
        inc di
        jmp .c
.e:
        mov byte [di], 0
        inc di
        mov [inst_end], di
        ret

; DEVICE=ruta argumentos
cfg_device:
        mov si, [cfg_val]
        mov di, dpath
        mov cx, 79
.p:
        lodsb
        cmp al, ' '
        je .pe
        cmp al, 13
        je .pe
        cmp al, 9
        je .pe
        mov [di], al
        inc di
        loop .p
.pe:
        mov byte [di], 0
        push ds
        pop es
        mov si, dpath
        call open_path
        jc .missing
        mov byte [di+SFT_REF], 0
        cmp byte [di+SFT_TYPE], T_FILE
        jne .missing
        mov eax, [di+SFT_SIZE]
        cmp eax, 0xFFF0
        ja .missing
        mov [ld_size], ax
        ; memoria: el bloque libre más grande
        mov bx, 0xFFFF
        call mcb_alloc
        call mcb_alloc
        jc .missing
        mov [ld_seg], ax
        mov [ld_paras], bx
        push es
        dec ax
        mov es, ax
        mov word [es:1], 8
        ; nombre del bloque = nombre del fichero
        push di
        mov di, 8
        mov cx, 8
        xor al, al
        rep stosb
        pop di
        mov si, dpath
        mov bx, si
.bn:
        lodsb
        test al, al
        jz .bne
        cmp al, '\'
        je .bs
        cmp al, ':'
        jne .bn
.bs:
        mov bx, si
        jmp .bn
.bne:
        mov si, bx
        mov di, 8
.bc:
        lodsb
        test al, al
        jz .bce
        cmp al, '.'
        je .bce
        cmp di, 16
        jae .bce
        mov [es:di], al
        inc di
        jmp .bc
.bce:
        pop es
        ; leer el fichero
        push ds
        pop es
        mov si, dpath
        call open_path
        mov byte [di+SFT_REF], 0
        mov es, [ld_seg]
        xor bx, bx
        mov cx, [ld_size]
        call file_read
        ; ¿formato MZ?
        mov es, [ld_seg]
        mov ax, [ld_seg]
        mov [ld_img], ax
        cmp word [es:0], 'MZ'
        jne .raw
        mov ax, [es:8]
        add ax, [ld_seg]
        mov [ld_img], ax
        mov cx, [es:6]
        mov si, [es:0x18]
.rl:
        jcxz .raw
        push es
        mov bx, [es:si]
        mov ax, [es:si+2]
        add ax, [ld_img]
        mov es, ax
        mov ax, [ld_img]
        add [es:bx], ax
        pop es
        add si, 4
        dec cx
        jmp .rl
.raw:
        ; inicializar cada driver del fichero
        mov ax, [ld_seg]
        add ax, [ld_paras]
        mov [ld_end], ax
        mov word [ld_used], 0
        mov word [dc_hdr], 0
        mov ax, [ld_img]
        mov [dc_hdr+2], ax
.drv:
        les di, [dc_hdr]
        mov ax, [es:di]
        mov [ld_next], ax
        ; petición INIT
        push ds
        pop es
        mov di, dreq
        mov cx, 15
        xor ax, ax
        rep stosw
        mov byte [dreq], 0x17
        mov byte [dreq+2], 0
        mov word [dreq+0x0E], 0
        mov ax, [ld_end]
        mov [dreq+0x10], ax
        mov ax, [cfg_val]
        mov [dreq+0x12], ax
        mov [dreq+0x14], ds
        mov al, [ndrives]
        mov [dreq+0x16], al
        call dev_call
        test word [dreq+3], 0x8000
        jnz .initerr
        ; memoria usada hasta la dirección de corte
        mov ax, [dreq+0x0E]
        add ax, 15
        shr ax, 4
        add ax, [dreq+0x10]
        sub ax, [ld_seg]
        jbe .decline
        cmp ax, [ld_paras]
        ja .initerr
        cmp ax, [ld_used]
        jbe .nu
        mov [ld_used], ax
.nu:
        les di, [dc_hdr]
        test word [es:di+4], 0x8000
        jz .block
        ; carácter: enlazar tras NUL
        mov eax, [dev_nul]
        mov [es:di], eax
        mov eax, [dc_hdr]
        mov [dev_nul], eax
        cmp dword [es:di+10], 'CON '
        jne .nc
        mov [con_hdr], eax
.nc:
        test word [es:di+4], 0x0008
        jz .nextdrv
        mov [lol+8], eax
        jmp .nextdrv
.block:
        mov dword [es:di], 0xFFFFFFFF
        movzx cx, byte [dreq+0x0D]
        les si, [dreq+0x12]
.unit:
        jcxz .bdone
        movzx bx, byte [ext_count]
        cmp bx, 4
        jae .bdone
        imul bx, bx, 9
        add bx, ext_tab
        mov eax, [dc_hdr]
        mov [bx], eax
        mov al, [dreq+0x0D]
        sub al, cl
        mov [bx+4], al
        mov ax, [es:si]
        mov [bx+5], ax
        mov [bx+7], es
        inc byte [ext_count]
        add si, 2
        dec cx
        jmp .unit
.bdone:
        call drives_ext
.nextdrv:
        mov ax, [ld_next]
        cmp ax, 0xFFFF
        je .fin
        mov [dc_hdr], ax
        jmp .drv
.fin:
        ; dejar solo la memoria que el driver necesita
        mov es, [ld_seg]
        mov bx, [ld_used]
        call mcb_resize
        ret
.decline:
        ; el driver no quiere quedarse residente: se descarga sin mensaje
        cmp word [ld_used], 0
        jne .fin
        mov es, [ld_seg]
        call mcb_free
        ret
.initerr:
        mov es, [ld_seg]
        call mcb_free
        mov si, msg_drverr
        call kputs
        mov si, dpath
        call kputs
        mov si, msg_crlf
        jmp kputs
.missing:
        mov si, msg_missing
        call kputs
        mov si, dpath
        call kputs
        mov si, msg_crlf
        call kputs
        jmp cfg_lineerr

; entorno inicial: COMSPEC, PATH y PROMPT (si CONFIG.SYS no los fija) + SET
build_root_env:
        mov es, [root_psp]
        push es
        mov es, [es:0x2C]
        call mcb_free
        pop es
        mov bx, (ENV_ROOT + 15) / 16
        call mcb_alloc
        jc .r
        mov es, [root_psp]
        mov [es:0x2C], ax
        push ax
        dec ax
        mov es, ax
        mov ax, [root_psp]
        mov [es:1], ax
        pop es
        xor di, di
        mov si, s_comspec
        call .str
        mov si, shell_path
        call .strz
        mov si, s_path
        call .has
        jnc .np
        mov si, s_path
        call .str
        mov al, [cur_drive]
        add al, 'A'
        stosb
        mov ax, '\:'
        xchg al, ah
        stosw
        xor al, al
        stosb
.np:
        mov si, s_prompt
        call .has
        jnc .npr
        mov si, s_prompt
        call .str
        mov si, s_pg
        call .strz
.npr:
        ; variables de SET
        mov si, setbuf
.sv:
        cmp si, [set_end]
        jae .end
        call .strz
        jmp .sv
.end:
        xor al, al
        stosb
.r:
        push ds
        pop es
        ret
; copiar SI sin el cero final / con el cero final
.str:
        lodsb
        test al, al
        jz .sr
        stosb
        jmp .str
.sr:
        ret
.strz:
        lodsb
        stosb
        test al, al
        jnz .strz
        ret
; ¿setbuf define la variable SI (con "=")? CF=0 si sí
.has:
        push di
        mov bx, setbuf
.hl:
        cmp bx, [set_end]
        jae .hno
        push si
        mov di, bx
.hc:
        mov al, [si]
        test al, al
        jz .hyes
        cmp al, [di]
        jne .hn
        inc si
        inc di
        jmp .hc
.hn:
        pop si
.hs:
        inc bx
        cmp byte [bx-1], 0
        jne .hs
        jmp .hl
.hyes:
        pop si
        pop di
        clc
        ret
.hno:
        pop di
        stc
        ret

; ejecutar las líneas INSTALL=
install_run:
        mov si, instbuf
.l:
        cmp si, [inst_end]
        jae .r
        ; ruta
        mov di, ipath
.p:
        lodsb
        test al, al
        jz .pe0
        cmp al, ' '
        je .pe
        mov [di], al
        inc di
        jmp .p
.pe0:
        dec si
.pe:
        mov byte [di], 0
        mov di, itail + 1
        xor cx, cx
        mov byte [di], ' '
        inc di
        inc cx
.a:
        lodsb
        test al, al
        jz .ae
        mov [di], al
        inc di
        inc cx
        jmp .a
.ae:
        mov byte [di], 13
        mov [itail], cl
        push si
        mov ax, KSEG
        mov ds, ax
        mov es, ax
        mov dx, ipath
        mov bx, inst_pb
        mov ax, 0x4B00
        int 0x21
        mov ax, KSEG
        mov ds, ax
        mov es, ax
        pop si
        jnc .l
        push si
        mov si, msg_missing
        call kputs
        mov si, ipath
        call kputs
        mov si, msg_crlf
        call kputs
        pop si
        jmp .l
.r:
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
        mov ax, [cur_psp]
        cmp ax, 0x40
        jb .nosave
        push es
        mov es, ax
        mov [es:0x2E], bp
        mov [es:0x30], ss
        pop es
.nosave:
        and byte [F_FL], 0xFE
%ifdef TRACE
        mov al, '['
        call ser_putc
        mov al, [F_AH]
        call trace_hex
        mov al, [F_AL]
        call trace_hex
        ; nombre de fichero en DS:DX
        mov al, [F_AH]
        cmp al, 0x09
        jne .t9
        push es
        push si
        push cx
        mov es, [F_DS]
        mov si, [F_DX]
        mov cx, 300
.t9l:
        mov al, [es:si]
        cmp al, '$'
        je .t9e
        call ser_putc
        inc si
        loop .t9l
.t9e:
        pop cx
        pop si
        pop es
        jmp .tnd
.t9:
        cmp al, 0x3D
        je .tn
        cmp al, 0x3C
        je .tn
        cmp al, 0x4B
        je .tn
        cmp al, 0x4E
        je .tn
        cmp al, 0x41
        je .tn
        cmp al, 0x43
        je .tn
        cmp al, 0x5B
        je .tn
        jmp .tnd
.tn:
        push es
        push si
        push cx
        mov es, [F_DS]
        mov si, [F_DX]
        mov al, ' '
        call ser_putc
        mov cx, 60
.tl:
        mov al, [es:si]
        test al, al
        jz .te
        call ser_putc
        inc si
        loop .tl
.te:
        ; 4Bh: también la línea de órdenes (ES:BX -> bloque, +2 = cola)
        cmp byte [F_AH], 0x4B
        jne .tt
        mov al, '|'
        call ser_putc
        mov es, [F_ES]
        mov si, [F_BX]
        les si, [es:si+2]
        movzx cx, byte [es:si]
        jcxz .tt
        inc si
.tq:
        mov al, [es:si]
        call ser_putc
        inc si
        loop .tq
.tt:
        pop cx
        pop si
        pop es
.tnd:
%endif
        movzx bx, byte [F_AH]
        cmp bx, MAXFN
        ja .bad
        shl bx, 1
        call [fntable+bx]
        jmp i21_ret
.bad:
        call fn_unsup
i21_ret:
%ifdef TRACE
        mov al, '='
        call ser_putc
        mov al, [F_AH]
        call trace_hex
        mov al, [F_AL]
        call trace_hex
        test byte [F_FL], 1
        jz .tnc
        mov al, 'C'
        call ser_putc
.tnc:
        mov al, ']'
        call ser_putc
%endif
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

%ifdef TRACE
; AL -> dos cifras hexadecimales por el puerto serie
trace_hex:
        push ax
        shr al, 4
        call .d
        pop ax
        and al, 15
.d:
        add al, '0'
        cmp al, '9'
        jbe .o
        add al, 7
.o:
        jmp ser_putc
%endif

ret_err:
        mov [F_AX], ax
        mov [last_err], ax
        or byte [F_FL], 1
        ret

fn_unsup:
        ; como MS-DOS: AL = 0, AH sin tocar (7100h para las funciones LFN)
        mov byte [F_AL], 0
        mov word [last_err], E_FUNC
        or byte [F_FL], 1
        ret

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
        cmp al, 3
        je do_ctrlc
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
fn_08:
        call std_in
        cmp al, 3
        je do_ctrlc
        mov [F_AL], al
        ret
fn_07:
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
        cmp al, [lastdrive]
        jae .s
        mov al, [lastdrive]
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
        test byte [cur_drv+D_FLAGS], DF_ISO
        jz .fat
        mov word [F_AX], 4
        mov word [F_BX], 0
        mov word [F_CX], 512
        mov eax, [cur_drv+D_SIZE]
        shr eax, 2
        cmp eax, 0xFFFF
        jbe .t
        mov ax, 0xFFFF
.t:
        mov [F_DX], ax
        ret
.fat:
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
        cmp al, 1
        je .set
        cmp al, 5
        je .boot
        cmp al, 6
        je .ver
        ret
.get:
        mov al, [break_flag]
        mov [F_DL], al
        ret
.set:
        mov al, [F_DL]
        and al, 1
        mov [break_flag], al
        ret
.boot:
        mov al, [boot_letter]
        inc al
        mov [F_DL], al
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
        je .link
        cmp al, 3
        je .nolink
        ret
.link:
        mov byte [F_AL], 0
        ret
.nolink:
        ; sin memoria superior (UMB) no se puede enlazar
        mov ax, E_FUNC
        jmp ret_err
.g:
        mov word [F_AX], 0
.r:
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
        mov al, [op_drv]
        call cds_update
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
        call sft_index
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
        cmp al, T_DEV
        je .dev
        mov word [F_AX], 0
        ret
.dev:
        mov eax, [di+SFT_SIZE]
        mov [dc_hdr], eax
        mov al, 4
        mov dx, [F_DS]
        mov bx, [F_DX]
        mov cx, [F_CX]
        call dev_rw
        mov [F_AX], ax
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
        cmp al, T_DEV
        jne .notdev
        mov eax, [di+SFT_SIZE]
        mov [dc_hdr], eax
        mov al, 8
        mov dx, [F_DS]
        mov bx, [F_DX]
        mov cx, [F_CX]
        call dev_rw
        mov [F_AX], ax
        ret
.notdev:
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
        test byte [fentry+11], 0x08
        jnz .sacc
        mov ax, [w_dir]
        call dir_get
        jc .sacc
        mov al, [F_CL]
        and al, 0x27
        mov ah, [si+11]
        and ah, 0x10
        or al, ah
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
        cmp byte [F_CL], 0x66
        je .getid
        cmp byte [F_CL], 0x46
        je .setid
        jmp fn_unsup
; número de serie, etiqueta y sistema de ficheros del sector de arranque
.getid:
        call bl_drive
        call select_drive
        jc .baddrv
        xor eax, eax
        call cache_read
        jc .baddrv
        mov es, [F_DS]
        mov di, [F_DX]
        mov word [es:di], 0
        add di, 2
        mov si, secbuf+0x27
        mov cx, 4 + 11 + 8
        rep movsb
        ret
.setid:
        call bl_drive
        call select_drive
        jc .baddrv
        xor eax, eax
        call cache_read
        jc .baddrv
        cmp byte [secbuf+0x26], 0x29
        jne .baddrv
        push ds
        mov ds, [F_DS]
        mov si, [F_DX]
        add si, 2
        push ds
        pop ax
        mov di, secbuf+0x27
        push es
        push ds
        mov ax, KSEG
        mov es, ax
        pop ds
        mov cx, 4 + 11 + 8
        rep movsb
        pop es
        pop ds
        call cache_write
        ret
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
        cmp al, T_DEV
        jne .ninfo
        push es
        les di, [di+SFT_SIZE]
        mov ax, [es:di+4]
        pop es
        and ax, 0x001F
        or ax, 0x0080
        mov [F_DX], ax
        mov [F_AX], ax
        ret
.ninfo:
        mov word [F_DX], 0x8084
        mov word [F_AX], 0x8084
        ret
.cinfo:
        mov word [F_DX], 0x80D3
        mov word [F_AX], 0x80D3
        ret
.finfo:
        mov ax, [di+SFT_DEVI]
        mov [F_DX], ax
        mov [F_AX], ax
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
        call sft_index
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
        call sft_index
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
        call sft_index
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
        call iso_deny
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
        call iso_deny
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
        call iso_deny
        jc ret_err
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
        call iso_deny
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

; ---- 26h / 55h: crear PSP ----
fn_26:
        mov es, [F_DX]
        push ds
        mov ds, [cur_psp]
        xor si, si
        xor di, di
        mov cx, 0x80
        rep movsw
        pop ds
        mov word [es:0x34], 0x18
        mov [es:0x36], es
        ret
fn_55:
        mov es, [F_DX]
        push ds
        mov ds, [cur_psp]
        xor si, si
        xor di, di
        mov cx, 0x80
        rep movsw
        pop ds
        mov ax, [F_SI]
        mov [es:2], ax
        mov ax, [cur_psp]
        mov [es:0x16], ax
        mov word [es:0x32], 20
        mov word [es:0x34], 0x18
        mov [es:0x36], es
        ; los handles heredados cuentan una referencia más
        mov di, 0x18
        mov cx, 20
.inh:
        movzx bx, byte [es:di]
        cmp bx, NSFT
        jae .ni
        imul bx, bx, SFT_LEN
        inc byte [sft_table+bx+SFT_REF]
.ni:
        inc di
        loop .inh
        mov ax, es
        mov [cur_psp], ax
        mov byte [F_AL], 0xF0
        ret

; ---- 65h: información de país extendida ----
fn_65:
        mov al, [F_AL]
        cmp al, 0x20
        jae .caps
        mov es, [F_ES]
        mov di, [F_DI]
        cmp al, 1
        je .ext
        mov bx, upper_tbl
        cmp al, 2
        je .ptr
        cmp al, 4
        je .ptr
        mov bx, fnterm_tbl
        cmp al, 5
        je .ptr
        mov bx, coll_tbl
        cmp al, 6
        je .ptr
        mov bx, dbcs_tbl
        cmp al, 7
        je .ptr
        jmp fn_unsup
.ptr:
        mov [es:di], al
        mov [es:di+1], bx
        mov word [es:di+3], KSEG
        mov word [F_CX], 5
        ret
.ext:
        mov byte [es:di], 1
        mov word [es:di+1], 38
        mov word [es:di+3], 34
        mov word [es:di+5], 437
        add di, 7
        mov si, country
        mov cx, 34
        rep movsb
        mov word [F_CX], 41
        ret
.caps:
        cmp al, 0x20
        je .c1
        cmp al, 0x21
        je .cs
        cmp al, 0x22
        je .cz
        cmp al, 0x23
        je .yn
        jmp fn_unsup
.c1:
        mov al, [F_DL]
        call upcase437
        mov [F_DL], al
        ret
.cs:
        mov es, [F_DS]
        mov di, [F_DX]
        mov cx, [F_CX]
.csl:
        jcxz .r
        mov al, [es:di]
        call upcase437
        mov [es:di], al
        inc di
        dec cx
        jmp .csl
.cz:
        mov es, [F_DS]
        mov di, [F_DX]
.czl:
        mov al, [es:di]
        test al, al
        jz .r
        call upcase437
        mov [es:di], al
        inc di
        jmp .czl
.yn:
        mov al, [F_DL]
        call upcase437
        mov word [F_AX], 1
        cmp al, 'Y'
        je .r
        cmp al, 'S'
        je .r
        mov word [F_AX], 0
        cmp al, 'N'
        je .r
        mov word [F_AX], 2
.r:
        ret

; AL -> mayúscula (página 437)
upcase437:
        cmp al, 'a'
        jb .hi
        cmp al, 'z'
        ja .hi
        sub al, 32
        ret
.hi:
        cmp al, 0x80
        jb .r
        push bx
        movzx bx, al
        mov al, [upper_tbl+2+bx-0x80]
        pop bx
.r:
        ret

; ---- Ctrl+C dentro de INT 21h: salir con los registros del usuario,
;      llamar a INT 23h y, si vuelve con IRET, repetir la función ----
do_ctrlc:
        mov al, '^'
        call con_out
        mov al, 'C'
        call con_out
        mov al, 13
        call con_out
        mov al, 10
        call con_out
        mov word [conq_len], 0
        mov word [conq_pos], 0
        call fat_flush
        mov sp, bp
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
        int 0x23
        jmp int21

; ---- 5Dh: SDA (06h) y lista de zonas de la SDA (0Bh) ----
fn_5d:
        mov al, [F_AL]
        cmp al, 0x06
        je .sda
        cmp al, 0x0B
        je .list
        jmp fn_unsup
.sda:
        mov word [F_DS], KSEG
        mov word [F_SI], sda
        mov word [F_CX], SDA_LEN
        mov word [F_DX], 0x18
        ret
.list:
        mov word [F_DS], KSEG
        mov word [F_SI], sda_list
        ret

; ---- 5Ah: crear un fichero temporal con nombre único en DS:DX ----
fn_5a:
        mov es, [F_DS]
        mov di, [F_DX]
        ; final de la ruta
.e:
        cmp byte [es:di], 0
        je .end
        inc di
        jmp .e
.end:
        mov [tmp_end], di
.try:
        mov di, [tmp_end]
        mov eax, 'HSP0'
        mov [es:di], eax
        inc word [tmp_count]
        mov ax, [tmp_count]
        ; cuatro cifras hexadecimales del contador
        mov cx, 4
        add di, 3
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, 'A'
        mov [es:di], al
        inc di
        pop ax
        loop .h
        mov dword [es:di], '.TMP'
        mov byte [es:di+4], 0
        and byte [F_FL], 0xFE
        mov byte [F_AL], 1
        mov si, [F_DX]
        mov cl, [F_CL]
        mov al, 1
        call create_file
        jnc .ok
        cmp ax, E_EXISTS
        je .try
        jmp ret_err
.ok:
        call jft_alloc
        jc .noh
        call sft_index
        call jft_set
        mov [F_AX], bx
        ret
.noh:
        mov byte [di+SFT_REF], 0
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
        mov al, [F_AL]
        mov [ex_mode], al
        cmp al, 0
        je .go
        cmp al, 1
        je .go
        cmp al, 3
        jne .bad
        jmp exec_overlay
.bad:
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
        ; MINALLOC = MAXALLOC = 0: como MS-DOS, el programa recibe el bloque
        ; libre mayor entero y la imagen se carga al final (el PSP queda al
        ; principio). KRNL386.EXE de Windows 3.1x depende de ello: da por
        ; libre la memoria que sigue al PSP.
        mov byte [ex_high], 0
        mov ax, [ex_hdr+0x0A]
        or ax, [ex_hdr+0x0C]
        jnz .exe_low
        mov bx, 0xFFFF
        call mcb_alloc                  ; falla: BX = bloque mayor
        cmp bx, [ex_min]
        jb .nomem
        call mcb_alloc
        jc .nomem
        mov byte [ex_high], 1
        jmp .gotexe
.exe_low:
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
        ; nombre del programa en el MCB de su PSP
        mov ax, [ex_psp]
        dec ax
        mov es, ax
        mov di, 8
        mov si, w_name
        mov cx, 8
.mn:
        lodsb
        cmp al, ' '
        jne .mn1
        xor al, al
.mn1:
        stosb
        loop .mn
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
        cmp byte [ex_high], 0
        je .ld_low
        mov ax, [ex_psp]                ; al final del bloque
        add ax, [ex_size]
        sub ax, [ex_need]
        add ax, 0x10
.ld_low:
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
        ; dirección de terminación (INT 22h) = vuelta del EXEC
        mov ax, [F_IP]
        mov [es:0x0A], ax
        mov ax, [F_CS]
        mov [es:0x0C], ax
        cmp byte [ex_mode], 1
        jne .run
        ; 4B01h: devolver SS:SP y CS:IP iniciales sin ejecutar
        ; DOS deja el AX inicial en la pila y devuelve SP-2
        push es
        mov es, [ex_ss]
        mov bx, [ex_sp]
        sub bx, 2
        mov word [es:bx], 0
        mov [ex_sp], bx
        pop es
        les di, [ex_pb]
        mov ax, [ex_sp]
        mov [es:di+0x0E], ax
        mov ax, [ex_ss]
        mov [es:di+0x10], ax
        mov ax, [ex_ip]
        mov [es:di+0x12], ax
        mov ax, [ex_cs]
        mov [es:di+0x14], ax
        mov ax, [ex_psp]
        mov [cur_psp], ax
        mov [dta_seg], ax
        mov word [dta_off], 0x80
        ret
.run:
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

; 4B03h: cargar un overlay en ES:BX -> {seg de carga, factor de reubicación}
exec_overlay:
        mov es, [F_ES]
        mov bx, [F_BX]
        mov ax, [es:bx]
        mov [ov_seg], ax
        mov ax, [es:bx+2]
        mov [ov_fact], ax
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
        mov dl, 0x06
        call find_entry
        jc .nf
        mov di, ex_sft
        call sft_fill_file
        push ds
        pop es
        mov bx, ex_hdr
        mov cx, 28
        call file_read
        cmp ax, 28
        jb .com
        mov ax, [ex_hdr]
        cmp ax, 'MZ'
        je .exe
        cmp ax, 'ZM'
        je .exe
.com:
        mov dword [ex_sft+SFT_POS], 0
        mov eax, [ex_sft+SFT_SIZE]
        mov [ex_img], eax
        jmp .load
.exe:
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
        mov [ex_img], eax
        mov [ex_sft+SFT_POS], ecx
.load:
        mov ax, [ov_seg]
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
        add word [ex_cur], 0x800
        jmp .chunk
.reloc:
        cmp word [ex_hdr], 'MZ'
        je .rl0
        cmp word [ex_hdr], 'ZM'
        jne .done
.rl0:
        movzx eax, word [ex_hdr+0x18]
        mov [ex_sft+SFT_POS], eax
        mov cx, [ex_hdr+6]
.rl:
        jcxz .done
        push cx
        push ds
        pop es
        mov bx, ex_rel
        mov cx, 4
        mov di, ex_sft
        call file_read
        pop cx
        mov ax, [ex_rel+2]
        add ax, [ov_seg]
        mov es, ax
        mov bx, [ex_rel]
        mov ax, [ov_fact]
        add [es:bx], ax
        dec cx
        jmp .rl
.done:
        ret
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
        imul bx, bx, SFT_LEN
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
        imul bx, bx, SFT_LEN
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
        mov eax, [es:0x0A]
        mov [term_addr], eax
        ; restaurar INT 22h-24h y la DTA del padre
        push ds
        xor ax, ax
        mov ds, ax
        mov eax, [es:0x0A]
        mov [ds:0x22*4], eax
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
        mov eax, [term_addr]
        test eax, eax
        jz .keep
        mov [F_IP], eax
.keep:
        ; volver con interrupciones activas y sin paso a paso
        mov word [F_FL], 0x0202
        jmp i21_ret
.root:
        mov si, msg_rootterm
        call kputs
        jmp halt

; =============================================================================
;  FCB (bloques de control de fichero, API de DOS 1.x)
;  FCB: +00 unidad, +01 nombre(8), +09 ext(3), +0C bloque, +0E tamaño de
;  registro, +10 tamaño, +14 fecha, +16 hora, +18 reservado (SFT+1, 'H'),
;  +20 registro actual, +21 registro aleatorio. Extendido: FF,5 x 0,atrib.
; =============================================================================
; -> [fcb_off] = parte normal del FCB del llamante (ES = su segmento)
fcb_setup:
        mov es, [F_DS]
        mov di, [F_DX]
        mov byte [fcb_ext], 0
        mov byte [fcb_attr], 0
        cmp byte [es:di], 0xFF
        jne .n
        mov byte [fcb_ext], 1
        mov al, [es:di+6]
        mov [fcb_attr], al
        add di, 7
.n:
        mov [fcb_off], di
        ret

; nombre del FCB -> fcb_name ("X:NOMBRE.EXT"), normalize + walk
fcb_resolve:
        mov es, [F_DS]
        mov si, [fcb_off]
        mov al, [es:si]
        test al, al
        jnz .d
        mov al, [cur_drive]
        inc al
.d:
        add al, 'A' - 1
        mov [fcb_name], al
        mov byte [fcb_name+1], ':'
        push ds
        push es
        pop ds
        inc si
        mov di, fcb_n11
        push es
        mov ax, KSEG
        mov es, ax
        mov cx, 11
        rep movsb
        pop es
        pop ds
        push es
        push ds
        pop es
        mov si, fcb_n11
        mov di, fcb_name+2
        call from11
        mov si, fcb_name
        call normalize
        pop es
        jc .r
        call walk
        jc .r
        cmp byte [w_has], 0
        je .bad
        ; el nombre conserva los '?' del FCB
        push es
        push ds
        pop es
        mov si, fcb_n11
        mov di, w_name
        mov cx, 11
        rep movsb
        pop es
        clc
.r:
        ret
.bad:
        stc
        ret

; -> DI = SFT abierta por este FCB; CF si no está abierto
fcb_sft:
        mov es, [F_DS]
        mov bx, [fcb_off]
        cmp byte [es:bx+0x19], 'H'
        jne .bad
        movzx di, byte [es:bx+0x18]
        test di, di
        jz .bad
        dec di
        cmp di, NSFT
        jae .bad
        imul di, di, SFT_LEN
        add di, sft_table
        cmp byte [di+SFT_REF], 0
        je .bad
        clc
        ret
.bad:
        stc
        ret

; rellenar el FCB abierto con la SFT DI
fcb_fill:
        mov es, [F_DS]
        mov bx, [fcb_off]
        call sft_index
        inc al
        mov [es:bx+0x18], al
        mov byte [es:bx+0x19], 'H'
        cmp byte [es:bx], 0
        jne .dk
        mov al, [op_drv]
        inc al
        mov [es:bx], al
.dk:
        mov word [es:bx+0x0C], 0
        mov word [es:bx+0x0E], 128
        mov eax, [di+SFT_SIZE]
        mov [es:bx+0x10], eax
        mov ax, [di+SFT_DATE]
        mov [es:bx+0x14], ax
        mov ax, [di+SFT_TIME]
        mov [es:bx+0x16], ax
        mov byte [es:bx+0x20], 0
        ret

; -> ECX = tamaño de registro (128 si es 0)
fcb_recsize:
        mov es, [F_DS]
        mov bx, [fcb_off]
        movzx ecx, word [es:bx+0x0E]
        test ecx, ecx
        jnz .r
        mov cx, 128
        mov [es:bx+0x0E], cx
.r:
        ret

; registro secuencial actual -> EAX
fcb_seqrec:
        mov es, [F_DS]
        mov bx, [fcb_off]
        movzx eax, word [es:bx+0x0C]
        shl eax, 7
        movzx edx, byte [es:bx+0x20]
        add eax, edx
        ret

; EAX = registro -> bloque y registro actual del FCB
fcb_setrec:
        mov es, [F_DS]
        mov bx, [fcb_off]
        push eax
        mov dl, al
        and dl, 0x7F
        mov [es:bx+0x20], dl
        shr eax, 7
        mov [es:bx+0x0C], ax
        pop eax
        ret

; registro aleatorio -> EAX (3 bytes si el registro es de 64 o más)
fcb_randrec:
        call fcb_recsize
        mov eax, [es:bx+0x21]
        cmp ecx, 64
        jb .r
        and eax, 0x00FFFFFF
.r:
        ret

; DI = SFT, EAX = registro, CX = número de registros -> leer en la DTA
; -> AL = 0 bien, 1 fin, 3 parcial; DX = registros leídos
fcb_read_recs:
        mov [fcb_n], cx
        push eax
        call fcb_recsize
        pop eax
        mov [fcb_rs], ecx
        mul ecx
        mov [di+SFT_POS], eax
        movzx eax, word [fcb_n]
        mul dword [fcb_rs]
        cmp eax, 0xFFFF
        jbe .c
        mov eax, 0xFFFF
.c:
        mov cx, ax
        mov [fcb_want], cx
        les bx, [dta_off]
        call file_read
        mov [fcb_got], ax
        ; registros completos y relleno del último
        xor dx, dx
        div word [fcb_rs]
        test dx, dx
        jz .full
        ; rellenar con ceros hasta el final del registro
        push ax
        mov cx, [fcb_rs]
        sub cx, dx
        les di, [dta_off]
        add di, [fcb_got]
        xor al, al
        rep stosb
        pop ax
        inc ax
        mov dx, ax
        mov al, 3
        ret
.full:
        mov dx, ax
        mov al, 0
        test dx, dx
        jnz .r
        mov al, 1
.r:
        ret

; DI = SFT, EAX = registro, CX = registros -> escribir desde la DTA
; -> AL = 0 bien, 1 disco lleno; DX = registros escritos
fcb_write_recs:
        mov [fcb_n], cx
        push eax
        call fcb_recsize
        pop eax
        mov [fcb_rs], ecx
        mul ecx
        mov [di+SFT_POS], eax
        movzx eax, word [fcb_n]
        mul dword [fcb_rs]
        mov cx, ax
        les bx, [dta_off]
        push cx
        call file_write
        pop cx
        mov [fcb_got], ax
        ; tamaño en el FCB
        push es
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov edx, [di+SFT_SIZE]
        mov [es:bx+0x10], edx
        pop es
        xor dx, dx
        div word [fcb_rs]
        mov dx, ax
        mov al, 0
        cmp [fcb_got], cx
        jae .r
        mov al, 1
.r:
        ret

; SI = etiqueta (11) -> copiarla en el sector de arranque si tiene BPB extendido
boot_label:
        pushad
        push es
        xor eax, eax
        call cache_read
        jc .r
        cmp byte [secbuf+0x26], 0x29
        jne .r
        push ds
        pop es
        mov di, secbuf+0x2B
        mov cx, 11
        rep movsb
        call cache_write
.r:
        pop es
        popad
        ret
noname11    db "NO NAME    "

fn_0f:
        call fcb_setup
        call fcb_resolve
        jc .fail
        ; ¿dispositivo (CON, NUL...)?
        call check_device
        jnc .file
        push ax
        call sft_alloc
        pop dx
        jc .fail
        mov byte [di+SFT_REF], 1
        mov [di+SFT_TYPE], dl
        mov byte [di+SFT_MODE], 2
        mov eax, [cd_hdr]
        mov [di+SFT_SIZE], eax
        call fcb_fill
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov dword [es:bx+0x10], 0
        mov byte [F_AL], 0
        ret
.file:
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, [fcb_attr]
        call find_entry
        jc .fail
        test byte [fentry+11], 0x18
        jnz .fail
        call sft_alloc
        jc .fail
        call sft_fill_file
        mov byte [di+SFT_MODE], 2
        call fcb_fill
        mov byte [F_AL], 0
        ret
.fail:
        mov byte [F_AL], 0xFF
        ret

fn_10:
        call fcb_setup
        call fcb_sft
        jc .r
        call sft_release
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov byte [es:bx+0x18], 0
.r:
        mov byte [F_AL], 0
        ret

fn_16:
        call fcb_setup
        test byte [fcb_attr], 0x08
        jnz .label
        call fcb_resolve
        jc .fail
        push ds
        pop es
        mov si, fcb_name
        mov cl, [fcb_attr]
        xor al, al
        call create_file
        jc .fail
        cmp byte [di+SFT_TYPE], T_FILE
        jne .dev
        call fcb_fill
.dev:
        mov byte [F_AL], 0
        ret
.label:
        ; etiqueta de volumen: entrada con atributo 08h en la raíz
        call fcb_resolve
        jc .fail
        mov ax, [w_dir]
        call dir_free_slot
        jc .fail
        mov si, w_name
        mov bl, 0x08
        xor dx, dx
        call dir_make
        mov si, w_name
        call boot_label
        mov byte [F_AL], 0
        ret
.fail:
        mov byte [F_AL], 0xFF
        ret

; 13h: borrar (admite comodines y etiquetas con FCB extendido)
fn_13:
        call fcb_setup
        call fcb_resolve
        jc .fail
        mov word [fcb_cnt], 0
        xor cx, cx
.l:
        mov ax, [w_dir]
        mov si, w_name
        mov dl, [fcb_attr]
        call find_entry
        jc .end
        test byte [fentry+11], 0x10
        jnz .n
        test byte [fentry+11], 0x08
        jnz .del
        test byte [fentry+11], 0x01
        jnz .n
        mov ax, [fentry+26]
        call fat_free_chain
.del:
        mov ax, [w_dir]
        call dir_get
        jc .end
        test byte [si+11], 0x08
        mov byte [si], 0xE5
        call cache_write
        jz .nolab
        push si
        mov si, noname11
        call boot_label
        pop si
.nolab:
        inc word [fcb_cnt]
.n:
        inc cx
        jmp .l
.end:
        cmp word [fcb_cnt], 0
        je .fail
        mov byte [F_AL], 0
        ret
.fail:
        mov byte [F_AL], 0xFF
        ret

; 17h: renombrar; nombre nuevo en FCB+11h ('?' conserva el carácter)
fn_17:
        call fcb_setup
        mov es, [F_DS]
        mov si, [fcb_off]
        add si, 0x11
        push ds
        push es
        pop ds
        mov di, fcb_new11
        push es
        mov ax, KSEG
        mov es, ax
        mov cx, 11
        rep movsb
        pop es
        pop ds
        call fcb_resolve
        jc .fail
        mov word [fcb_cnt], 0
        xor cx, cx
.l:
        mov ax, [w_dir]
        mov si, w_name
        mov dl, [fcb_attr]
        call find_entry
        jc .end
        ; nombre resultante
        push cx
        mov si, fcb_new11
        mov di, fcb_res11
        mov bx, fentry
        mov cx, 11
.m:
        mov al, [si]
        cmp al, '?'
        jne .k
        mov al, [bx]
.k:
        mov [di], al
        inc si
        inc di
        inc bx
        loop .m
        ; ¿ya existe?
        mov ax, [w_dir]
        xor cx, cx
        mov si, fcb_res11
        mov dl, 0x16
        call find_entry
        pop cx
        jnc .fail
        mov ax, [w_dir]
        call dir_get
        jc .fail
        push es
        push ds
        pop es
        mov di, si
        mov si, fcb_res11
        push cx
        mov cx, 11
        rep movsb
        pop cx
        pop es
        call cache_write
        inc word [fcb_cnt]
        inc cx
        jmp .l
.end:
        cmp word [fcb_cnt], 0
        je .fail
        mov byte [F_AL], 0
        ret
.fail:
        mov byte [F_AL], 0xFF
        ret

; 11h / 12h: buscar con FCB; el resultado (FCB sin abrir) va a la DTA
fn_11:
        call fcb_setup
        call fcb_resolve
        jc .fail
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov ax, [w_dir]
        mov [es:bx+0x0C], ax
        mov word [es:bx+0x0E], 0
        mov al, [op_drv]
        mov [es:bx+0x18], al
        mov byte [es:bx+0x19], 'S'
        jmp fcb_find_step
.fail:
        mov byte [F_AL], 0xFF
        ret
fn_12:
        call fcb_setup
        mov es, [F_DS]
        mov bx, [fcb_off]
        cmp byte [es:bx+0x19], 'S'
        jne fn_11.fail
        mov al, [es:bx+0x18]
        mov [op_drv], al
        call select_drive
        jc fn_11.fail
        ; nombre buscado desde el FCB
        push ds
        push es
        pop ds
        lea si, [bx+1]
        mov di, w_name
        push es
        mov ax, KSEG
        mov es, ax
        mov cx, 11
        rep movsb
        pop es
        pop ds
        mov ax, [es:bx+0x0C]
        mov [w_dir], ax
fcb_find_step:
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov cx, [es:bx+0x0E]
        mov ax, [w_dir]
        mov si, w_name
        mov dl, [fcb_attr]
        call find_entry
        jc fn_11.fail
        inc cx
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov [es:bx+0x0E], cx
        ; DTA: [FF 0 0 0 0 0 atrib] unidad + entrada de directorio
        les di, [dta_off]
        cmp byte [fcb_ext], 0
        je .noext
        mov byte [es:di], 0xFF
        mov dword [es:di+1], 0
        mov byte [es:di+5], 0
        mov al, [fcb_attr]
        mov [es:di+6], al
        add di, 7
.noext:
        mov al, [op_drv]
        inc al
        stosb
        mov si, fentry
        mov cx, 16
        rep movsw
        mov byte [F_AL], 0
        ret

fn_14:
        call fcb_setup
        call fcb_sft
        jc .eof
        call fcb_seqrec
        mov cx, 1
        call fcb_read_recs
        mov [F_AL], al
        cmp al, 1
        je .r
        call fcb_seqrec
        inc eax
        call fcb_setrec
.r:
        ret
.eof:
        mov byte [F_AL], 1
        ret

fn_15:
        call fcb_setup
        call fcb_sft
        jc .full
        call fcb_seqrec
        mov cx, 1
        call fcb_write_recs
        mov [F_AL], al
        test al, al
        jnz .r
        call fcb_seqrec
        inc eax
        call fcb_setrec
.r:
        ret
.full:
        mov byte [F_AL], 1
        ret

fn_21:
        call fcb_setup
        call fcb_sft
        jc fn_14.eof
        call fcb_randrec
        push eax
        call fcb_setrec
        pop eax
        mov cx, 1
        call fcb_read_recs
        mov [F_AL], al
        ret

fn_22:
        call fcb_setup
        call fcb_sft
        jc fn_15.full
        call fcb_randrec
        push eax
        call fcb_setrec
        pop eax
        mov cx, 1
        call fcb_write_recs
        mov [F_AL], al
        ret

fn_27:
        call fcb_setup
        call fcb_sft
        jc fn_14.eof
        call fcb_randrec
        mov cx, [F_CX]
        push eax
        call fcb_read_recs
        mov [F_AL], al
        mov [F_CX], dx
        pop eax
        movzx edx, dx
        add eax, edx
        call fcb_store_rand
        ret

fn_28:
        call fcb_setup
        call fcb_sft
        jc fn_15.full
        call fcb_randrec
        mov cx, [F_CX]
        test cx, cx
        jnz .w
        ; CX = 0: fijar el tamaño en el registro aleatorio
        push eax
        call fcb_recsize
        pop eax
        mul ecx
        mov [di+SFT_POS], eax
        call file_truncate
        mov es, [F_DS]
        mov bx, [fcb_off]
        mov eax, [di+SFT_SIZE]
        mov [es:bx+0x10], eax
        mov byte [F_AL], 0
        ret
.w:
        push eax
        call fcb_write_recs
        mov [F_AL], al
        mov [F_CX], dx
        pop eax
        movzx edx, dx
        add eax, edx
        call fcb_store_rand
        ret

; EAX -> registro aleatorio del FCB (y bloque/registro actual)
fcb_store_rand:
        push eax
        call fcb_recsize
        pop eax
        cmp ecx, 64
        jb .four
        mov [es:bx+0x21], ax
        shr eax, 16
        mov [es:bx+0x23], al
        shl eax, 16
        mov ax, [es:bx+0x21]
        jmp fcb_setrec
.four:
        mov [es:bx+0x21], eax
        jmp fcb_setrec

fn_23:
        call fcb_setup
        call fcb_resolve
        jc .fail
        mov ax, [w_dir]
        xor cx, cx
        mov si, w_name
        mov dl, [fcb_attr]
        call find_entry
        jc .fail
        call fcb_recsize
        mov eax, [fentry+28]
        add eax, ecx
        dec eax
        xor edx, edx
        div ecx
        mov [es:bx+0x21], eax
        mov byte [F_AL], 0
        ret
.fail:
        mov byte [F_AL], 0xFF
        ret

fn_24:
        call fcb_setup
        call fcb_seqrec
        call fcb_store_rand
        ret

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
        dw fn_08, fn_09, fn_0a, fn_0b, fn_0c, fn_0d, fn_0e, fn_0f       ; 08
        dw fn_10, fn_11, fn_12, fn_13, fn_14, fn_15, fn_16, fn_17 ; 10
        dw fn_unsup, fn_19, fn_1a, fn_unsup, fn_unsup, fn_unsup, fn_unsup, fn_1f ; 18
        dw fn_unsup, fn_21, fn_22, fn_23, fn_24, fn_25, fn_26, fn_27 ; 20
        dw fn_28, fn_29, fn_2a, fn_set_ok, fn_2c, fn_set_ok, fn_nop, fn_2f ; 28
        dw fn_30, fn_31, fn_32, fn_33, fn_34, fn_35, fn_36, fn_37     ; 30
        dw fn_38, fn_39, fn_3a, fn_3b, fn_3c, fn_3d, fn_3e, fn_3f ; 38
        dw fn_40, fn_41, fn_42, fn_43, fn_44, fn_45, fn_46, fn_47       ; 40
        dw fn_48, fn_49, fn_4a, fn_4b, fn_4c, fn_4d, fn_4e, fn_4f           ; 48
        dw fn_50, fn_51, fn_52, fn_unsup, fn_54, fn_55, fn_56, fn_57 ; 50
        dw fn_58, fn_59, fn_5a, fn_5b, fn_unsup, fn_5d, fn_unsup, fn_unsup ; 58
        dw fn_60, fn_unsup, fn_62, fn_unsup, fn_unsup, fn_65, fn_66, fn_67 ; 60
        dw fn_68, fn_unsup, fn_68, fn_unsup, fn_6c                       ; 68

; tablas de país (página de códigos 437) para INT 21h AH=65h
upper_tbl   dw 128
            db 0x80, 0x9A, 0x90, 0x41, 0x8E, 0x41, 0x8F, 0x80, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x8E, 0x8F
            db 0x90, 0x92, 0x92, 0x4F, 0x99, 0x4F, 0x55, 0x55, 0x59, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
            db 0x41, 0x49, 0x4F, 0x55, 0xA5, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
            db 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF
            db 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF
            db 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
            db 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF
            db 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
coll_tbl    dw 256
            db 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
            db 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
            db 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F
            db 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
            db 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
            db 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F
            db 0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
            db 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F
            db 0x43, 0x55, 0x45, 0x41, 0x41, 0x41, 0x41, 0x43, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x41, 0x41
            db 0x45, 0x91, 0x92, 0x4F, 0x4F, 0x4F, 0x55, 0x55, 0x59, 0x4F, 0x55, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
            db 0x41, 0x49, 0x4F, 0x55, 0x4E, 0x4E, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
            db 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF
            db 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF
            db 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
            db 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF
            db 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
fnterm_tbl  dw 22
            db 1, 0x00, 0xFF, 0x00, 0x00, 0x20, 0x02, 14
            db '.', '"', '/', 0x5C, '[', ']', ':', '|', '<', '>', '+', '=', ';', ','
dbcs_tbl    dw 0
all11       db "???????????"
shell_path  db "A:\COMMAND.COM", 0
            times 64 db 0
shell_tail  db 3, " /P", 13
            times 130 db 0      ; sitio para SHELL= y /D
msg_bootkeys db "Iniciando Hispa-DOS...  F5 = arranque limpio  ", 0xFA, "  F8 = men", 0xA3, " de arranque", 13, 10, 0
msg_clean   db "Arranque limpio: sin CONFIG.SYS ni AUTOEXEC.BAT.", 13, 10, 0
bm_t1       db 0xDD, " Hispa-DOS ", 0xDE, "  Men", 0xA3, " de arranque", 0
bm_hcfg     db "CONFIG.SYS", 0
bm_hauto    db "AUTOEXEC.BAT", 0
bm_tauto    db "Ejecutar AUTOEXEC.BAT", 0
bm_hmem     db "Memoria", 0
bm_conv     db "Convencional", 0
bm_ext      db "Extendida", 0
bm_upp      db "Superior", 0
bm_kbs      db " KB", 0
bm_hdesc    db "L", 0xA1, "nea elegida", 0
bm_s1       db "Se cargar", 0xA0, "n ", 0
bm_s2       db " de ", 0
bm_s3       db " elementos.", 0
bm_keys     db 0x18, 0x19, " Elegir   Espacio Marcar   Intro Arrancar   F5 Limpio   Esc Todo normal", 0
            db "            "
bm_num_e    db 0
d_tab       dw dk_device, 6, d_device
            dw dk_devhigh, 8, d_device
            dw dk_files, 5, d_files
            dw dk_buffers, 7, d_buffers
            dw dk_lastdrv, 8, d_lastdrv
            dw dk_numlock, 7, d_numlock
            dw dk_shell, 5, d_shell
            dw dk_set, 3, d_set
            dw dk_install, 7, d_install
            dw dk_switches, 8, d_switches
            dw 0
dk_device   db "DEVICE"
dk_devhigh  db "DEVICEHI"
dk_files    db "FILES"
dk_buffers  db "BUFFERS"
dk_lastdrv  db "LASTDRIV"
dk_numlock  db "NUMLOCK"
dk_shell    db "SHELL"
dk_set      db "SET"
dk_install  db "INSTALL"
dk_switches db "SWITCHES"
d_device    db "Controlador de|dispositivo.", 0
d_xms       db "Memoria extendida|(XMS 3.0): la usan|Windows y muchos|programas.", 0
d_cd        db "Lectores de CD y DVD|(IDE y SATA).", 0
d_usb       db "USB propio: memorias,|teclado y rat", 0xA2, "n USB.", 0
d_ram       db "Disco en memoria RAM.", 0
d_files     db "Ficheros abiertos a|la vez.", 0
d_buffers   db "B", 0xA3, "feres de disco.", 0
d_lastdrv   db 0xA3, "ltima letra de|unidad.", 0
d_numlock   db "Bloq Num al arrancar.", 0
d_shell     db "Int", 0x82, "rprete de", 0xA2, "rdenes.", 0
d_set       db "Variable de entorno.", 0
d_install   db "Programa residente.", 0
d_switches  db "Opciones de arranque.", 0
d_other     db "Ajuste del sistema.", 0
d_auto      db "Ordenes de inicio:|teclado, rat", 0xA2, "n,|sonido, ruta...", 0

            times 124 db 0
shell_fcb   times 16 db 0
shell_pb    dw 0
            dw shell_tail, KSEG
            dw shell_fcb, KSEG
            dw shell_fcb, KSEG

msg_banner  db 13, 10, "Hispa-DOS 0.2 - núcleo compatible con MS-DOS 6.22", 13, 10
            db "(C) 2026 colaboradores de Hispa-DOS - GPL-2.0 o posterior", 13, 10, 13, 10, 0
msg_fatbad  db "No se puede leer el sistema de archivos de la unidad de arranque", 13, 10, 0
msg_noshell db 13, 10, "No se puede cargar el intérprete de órdenes: sistema detenido", 13, 10, 0
msg_reshell db 13, 10, "Reiniciando el intérprete de órdenes...", 13, 10, 0
msg_rootterm db 13, 10, "El proceso raíz ha terminado: sistema detenido", 13, 10, 0

; lista de listas (INT 21h AH=52h); la palabra anterior es el primer MCB
            dw 0
lol:        dd 0                    ; +00 DPB
            dw sft_hdr, KSEG        ; +04 SFT
            dw dev_clock, KSEG      ; +08 CLOCK$
            dw dev_con, KSEG        ; +0C CON
            dw 512                  ; +10 sector máximo
            dd 0                    ; +12 búferes
            dw cds_tab, KSEG        ; +16 CDS
            dd 0                    ; +1A FCB
            dw 0                    ; +1E
            db 0                    ; +20 unidades de bloque
            db 26                   ; +21 LASTDRIVE (entradas de la CDS)
dev_nul:    dw dev_con, KSEG        ; +22 cabecera del dispositivo NUL
            dw 0x8004
            dw dev_strat, dev_intr_null
            db "NUL     "
            times 0x40 db 0
lol_end:
dev_con:    dw dev_aux, KSEG
            dw 0x8013
            dw dev_strat, dev_intr_con
            db "CON     "
dev_aux:    dw dev_prn, KSEG
            dw 0x8000
            dw dev_strat, dev_intr_null
            db "AUX     "
dev_prn:    dw dev_clock, KSEG
            dw 0xA0C0
            dw dev_strat, dev_intr_null
            db "PRN     "
dev_clock:  dw dev_com1, KSEG
            dw 0x8008
            dw dev_strat, dev_intr_null
            db "CLOCK$  "
dev_com1:   dw dev_lpt1, KSEG
            dw 0x8000
            dw dev_strat, dev_intr_null
            db "COM1    "
dev_disk:   dw 0xFFFF, 0xFFFF
            dw 0x0802
            dw dev_strat, dev_intr_null
            db 26, "HSPDSK"
            db 0
dev_lpt1:   dw 0xFFFF, 0xFFFF
            dw 0xA0C0
            dw dev_strat, dev_intr_null
            db "LPT1    "

cfg_path    db "A:\CONFIG.SYS", 0
s_comspec   db "COMSPEC=", 0
s_path      db "PATH=", 0
s_prompt    db "PROMPT=", 0
s_pg        db "$P$G", 0
msg_cfgunk  db "Orden no reconocida en CONFIG.SYS", 13, 10, 0
msg_cfgline db "Error en CONFIG.SYS, línea ", 0
msg_missing db "Falta o no es válido ", 0
msg_drverr  db "Error al iniciar el controlador de dispositivo ", 0
msg_crlf    db 13, 10, 0
inst_pb     dw 0
            dw itail, KSEG
            dw shell_fcb, KSEG
            dw shell_fcb, KSEG

; Swappable Data Area (SDA): estado del núcleo en los desplazamientos de
; MS-DOS 4+; 5D06h/5D0Bh la entregan a quien la necesite (DEBUG, Windows)
sda:
            db 0                    ; +00 ErrorMode
            db 0                    ; +01 InDOS
            db 0                    ; +02 unidad del error crítico
            db 0                    ; +03 lugar del último error
            dw 0                    ; +04 último error extendido
            db 0                    ; +06 acción sugerida
            db 0                    ; +07 clase de error
            dd 0                    ; +08 ES:DI del último error
            dd 0                    ; +0C DTA actual
            dw 0                    ; +10 PSP actual
            dw 0                    ; +12 SP en INT 23h
            dw 0                    ; +14 código de retorno del último proceso
            db 0                    ; +16 unidad actual
            db 0                    ; +17 indicador de BREAK extendido
            times SDA_LEN - 0x18 db 0
sda_list:   dw 2                    ; 5D0Bh: zonas de la SDA
            dw sda, KSEG, 0x0018
            dw sda + 0x18, KSEG, 0x8000 | (SDA_LEN - 0x18)
tmp_count   dw 0

; =============================================================================
        section .bss
bss_start:
bootsec     resb 512
boot_drive  resb 1
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
mnt_try     resb 1
isob_drv    resb 1
iso_next    resb 1
ife_attr    resb 1
            alignb 4
isob_lba    resd 1
ife_ext     resd 1
ife_size    resd 1
ife_pat     resw 1
ife_start   resw 1
ife_n       resw 1
iso_dirs    resb 9*ISO_ND
hist_pos    resb 1
hist_head   resb 1
hist_count  resb 1
bios_nd     resb 1
sf_entry    resb 16
boot_letter resb 1
cu_drv      resb 1
fcb_ext     resb 1
fcb_attr    resb 1
fcb_off     resw 1
fcb_n       resw 1
fcb_want    resw 1
fcb_got     resw 1
fcb_cnt     resw 1
            alignb 4
fcb_rs      resd 1
fcb_name    resb 16
fcb_n11     resb 12
fcb_new11   resb 12
fcb_res11   resb 12
tmp_end     resw 1
ex_mode     resb 1
            alignb 4
term_addr   resd 1
ov_seg      resw 1
ov_fact     resw 1
break_flag  resb 1
cfg_inmenu  resb 1
ext_count   resb 1
lastdrive   resb 1
            alignb 4
dc_hdr      resd 1
dc_far      resd 1
dc_far2     resd 1
cd_hdr      resd 1
con_hdr     resd 1
dev_reqp    resd 1
ld_seg      resw 1
ld_paras    resw 1
ld_size     resw 1
ld_img      resw 1
ld_end      resw 1
ld_used     resw 1
ld_next     resw 1
cfg_line    resw 1
boot_clean  resb 1
boot_noauto resb 1
bm_noauto   resb 1
sw_flags    resb 1
cfg_dis     resb MAXCL
bm_n        resw 1
bm_sel      resw 1
bm_num      resw 1
bm_count    resw 1
bm_off      resw MAXIT
bm_line     resw MAXIT
bm_key      resb 8
cfg_val     resw 1
set_end     resw 1
inst_end    resw 1
ext_tab     resb 9*4
dreq        resb 32
cfg_key     resb 16
dpath       resb 84
ipath       resb 84
itail       resb 130
cfgl        resb 168
setbuf      resb SET_MAX
instbuf     resb INST_MAX
cfgbuf      resb CFG_MAX
isobuf      resb 2048
hist_buf    resb HIST_N*HIST_LEN
cds_tab     resb CDS_LEN*NCDS
dpb_tab     resb DPB_LEN*NCDS
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
root_psp    resw 1
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
ex_high     resb 1
ex_cur      resw 1
ex_cs       resw 1
ex_ip       resw 1
ex_ss       resw 1
ex_sp       resw 1
ex_rel      resd 1
            alignb 16
ex_sft      resb SFT_LEN
sft_hdr     resb 6                  ; siguiente bloque (dd) y entradas (dw)
sft_table   resb SFT_LEN*NSFT
            alignb 16
secbuf      resb 512
fatsec      resb 512
            alignb 2
kstack      resb 1024
kstack_top:
            alignb 16
kernel_end:
