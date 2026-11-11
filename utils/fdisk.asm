; =============================================================================
;  HispaDOS FDISK.COM - gestor de particiones de disco duro
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  FDISK                 menú interactivo
;  FDISK [n] /STATUS     tabla de particiones del disco n (1 = primero)
;  FDISK [n] /PRI[:MB]   crear partición primaria DOS (FAT16), activa si no
;                        hay otra; sin tamaño usa todo el espacio libre
;  FDISK [n] /ACT:p      marcar activa la partición p
;  FDISK [n] /DEL:p      borrar la partición p
;  FDISK [n] /MBR        escribir el código de arranque de HispaDOS en el MBR
;
;  Tras cambiar la tabla llama a INT 21h AH=0Dh: el núcleo vuelve a leer las
;  particiones y asigna las letras de unidad sin reiniciar.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAX_FAT16   equ 2047 * 2048         ; 2047 MB en sectores
MIN_PART    equ 4200                ; ~2 MB: mínimo para FAT16

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, (stack_top - bss_start) / 2
        xor ax, ax
        rep stosw
        mov si, msg_title
        call puts
        call count_disks
        cmp byte [ndisks], 0
        jne .have
        mov si, msg_nodisk
        call puts
        mov ax, 0x4C01
        int 0x21
.have:
        mov byte [disk], 0x80
        ; analizar la línea de órdenes
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .args_done
        cmp al, ' '
        je .arg
        cmp al, '/'
        je .opt
        cmp al, '1'
        jb .arg
        cmp al, '9'
        ja .arg
        sub al, '1'
        cmp al, [ndisks]
        jae .baddisk
        add al, 0x80
        mov [disk], al
        jmp .arg
.baddisk:
        mov si, msg_baddisk
        call puts
        mov ax, 0x4C01
        int 0x21
.opt:
        mov di, si
        call opt_word               ; -> optbuf
        call skip_word
        mov di, optbuf
        cmp dword [di], 'STAT'
        je .o_status
        cmp word [di], 'PR'
        jne .n1
        cmp byte [di+2], 'I'
        je .o_pri
.n1:
        cmp dword [di], 'ACT:'
        je .o_act
        cmp dword [di], 'DEL:'
        je .o_del
        cmp word [di], 'MB'
        jne .n2
        cmp byte [di+2], 'R'
        je .o_mbr
.n2:
        mov si, msg_badopt
        call puts
        mov ax, 0x4C01
        int 0x21
.o_status:
        mov byte [mode], 1
        jmp .arg
.o_pri:
        mov byte [mode], 2
        mov dword [req_mb], 0
        cmp byte [di+3], ':'
        jne .arg
        push si
        lea si, [di+4]
        call parse_num
        mov [req_mb], eax
        pop si
        jmp .arg
.o_act:
        mov byte [mode], 3
        mov al, [di+4]
        sub al, '0'
        mov [req_part], al
        jmp .arg
.o_del:
        mov byte [mode], 4
        mov al, [di+4]
        sub al, '0'
        mov [req_part], al
        jmp .arg
.o_mbr:
        mov byte [mode], 5
        jmp .arg
.args_done:
        call load_disk
        jc .ioerr
        mov al, [mode]
        cmp al, 0
        je menu
        cmp al, 1
        je .c_status
        cmp al, 2
        je .c_pri
        cmp al, 3
        je .c_act
        cmp al, 4
        je .c_del
        call install_mbr
        jmp exit_ok
.c_status:
        call show_table
        jmp exit_ok
.c_pri:
        mov eax, [req_mb]
        call create_primary
        jc exit_err
        call show_table
        jmp exit_ok
.c_act:
        mov al, [req_part]
        call set_active
        jc exit_err
        call show_table
        jmp exit_ok
.c_del:
        mov al, [req_part]
        call delete_part
        jc exit_err
        call show_table
        jmp exit_ok
.ioerr:
        mov si, msg_ioerr
        call puts
exit_err:
        mov ax, 0x4C01
        int 0x21
exit_ok:
        mov ax, 0x4C00
        int 0x21

; copiar la opción (mayúsculas) de DS:DI a optbuf
opt_word:
        push si
        push di
        mov si, di
        mov di, optbuf
        mov cx, 15
.l:
        lodsb
        cmp al, 13
        je .e
        cmp al, ' '
        je .e
        cmp al, '/'
        je .e
        call upcase
        stosb
        loop .l
.e:
        mov dword [di], 0
        pop di
        pop si
        ret

skip_word:
.l:
        mov al, [si]
        cmp al, 13
        je .e
        cmp al, ' '
        je .e
        cmp al, '/'
        je .e
        inc si
        jmp .l
.e:
        ret

; -----------------------------------------------------------------------------
;  Menú interactivo
; -----------------------------------------------------------------------------
menu:
        call crlf
        mov si, msg_menu1
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        call crlf
        mov si, msg_menu2
        call puts
        cmp byte [ndisks], 1
        jbe .one
        mov si, msg_menu5
        call puts
.one:
        mov si, msg_choice
        call puts
.key:
        call getkey
        cmp al, 27
        je exit_ok
        cmp al, '1'
        jb .key
        cmp al, '5'
        ja .key
        call putc
        call crlf
        call crlf
        cmp al, '1'
        je .create
        cmp al, '2'
        je .active
        cmp al, '3'
        je .delete
        cmp al, '4'
        je .display
        cmp byte [ndisks], 1
        jbe menu
        ; cambiar de disco
        mov si, msg_whichdisk
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu
        test eax, eax
        jz menu
        cmp al, [ndisks]
        ja menu
        add al, 0x7F
        mov [disk], al
        call load_disk
        jmp menu
.display:
        call show_table
        call press_esc
        jmp menu
.create:
        call show_table
        call max_free_mb
        mov si, msg_askmb
        call puts
        call putnum
        mov si, msg_askmb2
        call puts
        call readline
        xor eax, eax
        jcxz .cr
        mov si, u_line
        call parse_num
        jc menu
.cr:
        call create_primary
        call press_esc
        jmp menu
.active:
        call show_table
        mov si, msg_askact
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu
        call set_active
        call press_esc
        jmp menu
.delete:
        call show_table
        mov si, msg_askdel
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu
        push ax
        mov si, msg_sure
        call puts
        call ask_yes
        pop ax
        jc menu
        call delete_part
        call press_esc
        jmp menu

press_esc:
        mov si, msg_esc
        call puts
.k:
        call getkey
        cmp al, 27
        je .r
        cmp al, 13
        jne .k
.r:
        call crlf
        ret

; -----------------------------------------------------------------------------
;  Acceso al disco
; -----------------------------------------------------------------------------
count_disks:
        mov byte [ndisks], 0
        mov ah, 8
        mov dl, 0x80
        xor di, di
        mov es, di
        int 0x13
        push ds
        pop es
        jc .r
        mov [ndisks], dl
.r:
        ret

; leer geometría, tamaño y MBR del disco [disk]
load_disk:
        mov ah, 8
        mov dl, [disk]
        xor di, di
        mov es, di
        int 0x13
        push ds
        pop es
        jc .err
        movzx ax, cl
        and al, 0xC0
        shl ax, 2
        mov al, ch
        inc ax
        mov [cyls], ax
        and cx, 0x3F
        mov [spt], cx
        movzx dx, dh
        inc dx
        mov [heads], dx
        movzx eax, word [cyls]
        movzx ecx, word [spt]
        imul eax, ecx
        movzx ecx, word [heads]
        imul eax, ecx
        mov [total], eax
        mov byte [lba], 0
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [disk]
        int 0x13
        jc .nolba
        cmp bx, 0xAA55
        jne .nolba
        test cl, 1
        jz .nolba
        mov byte [lba], 1
        mov word [edd], 0x1A
        mov ah, 0x48
        mov dl, [disk]
        mov si, edd
        int 0x13
        jc .nolba
        mov eax, [edd+0x10]
        cmp dword [edd+0x14], 0
        je .tot
        mov eax, 0xFFFFFFFF
.tot:
        test eax, eax
        jz .nolba
        mov [total], eax
.nolba:
        xor eax, eax
        mov bx, mbrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .err
        mov byte [newmbr], 0
        cmp word [mbrbuf+510], 0xAA55
        je .ok
        ; sin tabla válida: empezar de cero
        mov di, mbrbuf
        mov cx, 256
        xor ax, ax
        rep stosw
        mov byte [newmbr], 1
.ok:
        clc
        ret
.err:
        stc
        ret

; EAX = LBA, ES:BX = búfer, [io_cmd] = 2 leer / 3 escribir
disk_io:
        pushad
        mov di, 3
.retry:
        cmp byte [lba], 0
        je .chs
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
        mov dl, [disk]
        mov si, dap
        int 0x13
        pop eax
        jnc .ok
        jmp .again
.chs:
        push eax
        call lba2chs_regs
        mov ah, [io_cmd]
        mov al, 1
        mov dl, [disk]
        int 0x13
        pop eax
        jnc .ok
.again:
        push eax
        xor ax, ax
        mov dl, [disk]
        int 0x13
        pop eax
        dec di
        jnz .retry
        popad
        stc
        ret
.ok:
        popad
        clc
        ret

; EAX = LBA -> CX/DH para INT 13h
lba2chs_regs:
        push di
        mov di, chstmp
        call lba2chs
        mov dh, [chstmp]
        mov cl, [chstmp+1]
        mov ch, [chstmp+2]
        pop di
        ret

; EAX = LBA -> 3 bytes CHS de la tabla en DS:DI (cabeza, sector|cil, cil)
lba2chs:
        push eax
        push ebx
        push ecx
        push edx
        xor edx, edx
        movzx ecx, word [spt]
        div ecx
        inc dl
        mov bl, dl
        xor edx, edx
        movzx ecx, word [heads]
        div ecx
        cmp eax, 1023
        jbe .ok
        mov eax, 1023
        mov dl, [heads]
        dec dl
        mov bl, [spt]
.ok:
        mov [di], dl
        mov cl, bl
        mov ch, al
        shl ah, 6
        or cl, ah
        mov [di+1], cl
        mov [di+2], ch
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; escribir el MBR y avisar al núcleo
write_mbr:
        mov word [mbrbuf+510], 0xAA55
        cmp byte [newmbr], 0
        je .w
        call copy_mbr_code
.w:
        xor eax, eax
        mov bx, mbrbuf
        mov byte [io_cmd], 3
        call disk_io
        jc .err
        mov byte [newmbr], 0
        mov ah, 0x0D                ; el núcleo relee las particiones
        int 0x21
        clc
        ret
.err:
        mov si, msg_ioerr
        call puts
        stc
        ret

copy_mbr_code:
        mov si, mbr_code
        mov di, mbrbuf
        mov cx, 446
        rep movsb
        ret

install_mbr:
        call copy_mbr_code
        call write_mbr
        jc .r
        mov si, msg_mbrok
        call puts
.r:
        ret

; -----------------------------------------------------------------------------
;  Operaciones con particiones
; -----------------------------------------------------------------------------
; AL = tipo -> CF=0 si es una partición DOS (FAT12/FAT16)
is_dos:
        cmp al, 0x01
        je .y
        cmp al, 0x04
        je .y
        cmp al, 0x06
        je .y
        cmp al, 0x0E
        je .y
        stc
        ret
.y:
        clc
        ret

; -> EAX = primer sector libre tras las particiones, EDX = sectores libres
free_space:
        push si
        push cx
        push ebx
        movzx eax, word [spt]
        mov si, mbrbuf+0x1BE
        mov cx, 4
.l:
        cmp byte [si+4], 0
        je .n
        mov ebx, [si+8]
        add ebx, [si+12]
        cmp ebx, eax
        jbe .n
        mov eax, ebx
.n:
        add si, 16
        loop .l
        ; alinear al cilindro si no es el principio del disco
        movzx ebx, word [spt]
        cmp eax, ebx
        je .al
        push eax
        movzx eax, word [heads]
        imul ebx, eax
        pop eax
        add eax, ebx
        dec eax
        xor edx, edx
        div ebx
        imul eax, ebx
.al:
        mov edx, [total]
        sub edx, eax
        jae .r
        xor edx, edx
.r:
        pop ebx
        pop cx
        pop si
        ret

; -> EAX = MB libres utilizables (máximo 2047)
max_free_mb:
        call free_space
        mov eax, edx
        cmp eax, MAX_FAT16
        jbe .s
        mov eax, MAX_FAT16
.s:
        shr eax, 11
        ret

; EAX = MB (0 = máximo) -> crear partición primaria DOS
create_primary:
        mov [req_mb], eax
        ; buscar una entrada libre
        mov si, mbrbuf+0x1BE
        mov cx, 4
.slot:
        cmp byte [si+4], 0
        je .gotslot
        add si, 16
        loop .slot
        mov si, msg_full
        call puts
        stc
        ret
.gotslot:
        mov [slot], si
        call free_space
        mov [p_start], eax
        mov eax, edx
        cmp eax, MAX_FAT16
        jbe .c1
        mov eax, MAX_FAT16
.c1:
        mov ecx, [req_mb]
        jecxz .c2
        shl ecx, 11
        cmp ecx, eax
        ja .c2
        mov eax, ecx
.c2:
        mov [p_size], eax
        cmp eax, MIN_PART
        jae .big
        mov si, msg_nospace
        call puts
        stc
        ret
.big:
        ; tipo según tamaño y posición
        mov bl, 0x04
        cmp eax, 65536
        jb .t
        mov bl, 0x06
        add eax, [p_start]
        movzx ecx, word [spt]
        movzx edx, word [heads]
        imul ecx, edx
        imul ecx, ecx, 1024
        cmp eax, ecx
        jbe .t
        mov bl, 0x0E
.t:
        mov di, [slot]
        mov [di+4], bl
        mov eax, [p_start]
        mov [di+8], eax
        mov eax, [p_size]
        mov [di+12], eax
        mov eax, [p_start]
        push di
        inc di
        call lba2chs
        pop di
        mov eax, [p_start]
        add eax, [p_size]
        dec eax
        push di
        add di, 5
        call lba2chs
        pop di
        ; activa si no hay ninguna
        mov byte [di], 0
        mov si, mbrbuf+0x1BE
        mov cx, 4
.act:
        test byte [si], 0x80
        jnz .hasact
        add si, 16
        loop .act
        mov byte [di], 0x80
.hasact:
        call write_mbr
        jc .r
        mov si, msg_created
        call puts
        clc
.r:
        ret

; AL = partición (1-4) -> SI = entrada; CF si no existe
part_entry:
        cmp al, 1
        jb .bad
        cmp al, 4
        ja .bad
        movzx si, al
        dec si
        shl si, 4
        add si, mbrbuf+0x1BE
        cmp byte [si+4], 0
        je .bad
        clc
        ret
.bad:
        push si
        mov si, msg_nopart
        call puts
        pop si
        stc
        ret

set_active:
        call part_entry
        jc .r
        mov di, mbrbuf+0x1BE
        mov cx, 4
.l:
        mov byte [di], 0
        add di, 16
        loop .l
        mov byte [si], 0x80
        call write_mbr
        jc .r
        mov si, msg_actok
        call puts
        clc
.r:
        ret

delete_part:
        call part_entry
        jc .r
        mov di, si
        mov cx, 16
        xor al, al
        rep stosb
        call write_mbr
        jc .r
        mov si, msg_delok
        call puts
        clc
.r:
        ret

; -----------------------------------------------------------------------------
;  Mostrar la tabla
; -----------------------------------------------------------------------------
; letra de la primera partición DOS de este disco
first_letter:
        mov byte [letter], 'C'
        mov al, 0x80
.d:
        cmp al, [disk]
        jae .r
        push ax
        mov [tmpdisk], al
        call count_dos_of
        add [letter], al
        pop ax
        inc al
        jmp .d
.r:
        ret

; contar particiones DOS del disco [tmpdisk] -> AL
count_dos_of:
        push word [disk]
        mov al, [tmpdisk]
        mov [disk], al
        push word [lba]
        mov byte [lba], 0
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [disk]
        int 0x13
        jc .nl
        cmp bx, 0xAA55
        jne .nl
        mov byte [lba], 1
.nl:
        xor eax, eax
        mov bx, iobuf
        mov byte [io_cmd], 2
        call disk_io
        pop word [lba]
        pop word [disk]
        mov al, 0
        jc .r
        cmp word [iobuf+510], 0xAA55
        jne .r
        mov si, iobuf+0x1BE
        mov cx, 4
.l:
        push ax
        mov al, [si+4]
        call is_dos
        pop ax
        jc .n
        inc al
.n:
        add si, 16
        loop .l
.r:
        ret

show_table:
        call first_letter
        mov si, msg_curdisk
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov si, msg_open
        call puts
        mov eax, [total]
        shr eax, 11
        call putnum
        mov si, msg_mbytes
        call puts
        mov si, msg_sectors
        call puts
        mov eax, [total]
        call putnum
        mov si, msg_sects
        call puts
        cmp byte [lba], 0
        je .nol
        mov si, msg_lba
        call puts
.nol:
        mov si, msg_close
        call puts
        call crlf
        mov si, mbrbuf+0x1BE
        mov cx, 4
        xor bx, bx
.cnt:
        cmp byte [si+4], 0
        je .cn
        inc bx
.cn:
        add si, 16
        loop .cnt
        test bx, bx
        jnz .hdr
        mov si, msg_noparts
        call puts
        jmp .free
.hdr:
        mov si, msg_thead
        call puts
        mov si, mbrbuf+0x1BE
        mov cx, 1
.row:
        cmp byte [si+4], 0
        je .next
        push cx
        push si
        movzx eax, cx
        push cx
        mov cl, 6
        call putnum_w
        pop cx
        mov al, ' '
        call putc
        call putc
        call putc
        call putc
        call putc
        mov al, ' '
        test byte [si], 0x80
        jz .na
        mov al, 'A'
.na:
        call putc
        mov al, ' '
        call putc
        call putc
        call putc
        call putc
        ; tipo
        mov al, [si+4]
        call type_name
        push si
        mov si, di
        call puts
        pop si
        ; inicio y tamaño en MB
        mov eax, [si+8]
        shr eax, 11
        mov cl, 10
        call putnum_w
        mov eax, [si+12]
        shr eax, 11
        mov cl, 10
        call putnum_w
        ; unidad
        mov al, [si+4]
        call is_dos
        jc .nodrv
        mov si, msg_sp4
        call puts
        mov al, [letter]
        call putc
        mov al, ':'
        call putc
        inc byte [letter]
.nodrv:
        call crlf
        pop si
        pop cx
.next:
        add si, 16
        inc cx
        cmp cx, 4
        jbe .row
.free:
        call crlf
        mov si, msg_freesp
        call puts
        call free_space
        mov eax, edx
        shr eax, 11
        call putnum
        mov si, msg_mbytes
        call puts
        call crlf
        ret

; AL = tipo -> DI = nombre (20 columnas)
type_name:
        mov di, types
.l:
        cmp byte [di], 0xFF
        je .other
        cmp [di], al
        je .f
        add di, 22
        jmp .l
.f:
        inc di
        ret
.other:
        mov di, tn_other
        ret

; -----------------------------------------------------------------------------
%include "util.inc"

types:
        db 0x01, "PRI DOS  FAT12      ", 0
        db 0x04, "PRI DOS  FAT16 <32M ", 0
        db 0x06, "PRI DOS  FAT16      ", 0
        db 0x0E, "PRI DOS  FAT16 LBA  ", 0
        db 0x05, "EXT DOS             ", 0
        db 0x0F, "EXT DOS  LBA        ", 0
        db 0x0B, "Non-DOS  FAT32      ", 0
        db 0x0C, "Non-DOS  FAT32 LBA  ", 0
        db 0x07, "Non-DOS  NTFS/exFAT ", 0
        db 0x82, "Non-DOS  Linux swap ", 0
        db 0x83, "Non-DOS  Linux      ", 0
        db 0xEE, "Non-DOS  GPT        ", 0
        db 0xFF
tn_other    db "Non-DOS             ", 0

msg_title   db 13, 10, "HispaDOS Fixed Disk Setup Program 0.2", 13, 10
            db "(C) 2026 HispaDOS contributors - GPL-2.0-or-later", 13, 10, 0
msg_nodisk  db "No fixed disks present.", 13, 10, 0
msg_baddisk db "Invalid fixed disk number.", 13, 10, 0
msg_badopt  db "Invalid option. Use /STATUS /PRI[:MB] /ACT:n /DEL:n /MBR", 13, 10, 0
msg_ioerr   db "Error reading or writing the fixed disk.", 13, 10, 0
msg_menu1   db "                         FDISK Options", 13, 10, 13, 10
            db "Current fixed disk drive: ", 0
msg_menu2   db 13, 10, "Choose one of the following:", 13, 10, 13, 10
            db "1. Create DOS partition", 13, 10
            db "2. Set active partition", 13, 10
            db "3. Delete partition", 13, 10
            db "4. Display partition information", 13, 10, 0
msg_menu5   db "5. Change current fixed disk drive", 13, 10, 0
msg_choice  db 13, 10, "Enter choice (Esc to exit): ", 0
msg_whichdisk db "Enter fixed disk drive number: ", 0
msg_askmb   db "Partition size in Mbytes (Enter = maximum, ", 0
msg_askmb2  db " MB): ", 0
msg_askact  db "Enter the number of the partition you want to make active: ", 0
msg_askdel  db "Enter the number of the partition you want to delete: ", 0
msg_sure    db "WARNING! Data in the deleted partition will be lost.", 13, 10
            db "Do you wish to continue (Y/N)? ", 0
msg_esc     db "Press Enter or Esc to continue", 0
msg_mbrok   db "Master boot code updated.", 13, 10, 0
msg_full    db "No free partition table entry.", 13, 10, 0
msg_nospace db "Not enough free space on the disk.", 13, 10, 0
msg_created db "Primary DOS partition created. Drive letters have been updated.", 13, 10, 0
msg_nopart  db "No such partition.", 13, 10, 0
msg_actok   db "Active partition set.", 13, 10, 0
msg_delok   db "Partition deleted. Drive letters have been updated.", 13, 10, 0
msg_curdisk db 13, 10, "Current fixed disk drive: ", 0
msg_open    db "  (", 0
msg_mbytes  db " MB", 0
msg_sectors db ", ", 0
msg_sects   db " sectors", 0
msg_lba     db ", LBA", 0
msg_close   db ")", 13, 10, 0
msg_noparts db "No partitions defined.", 13, 10, 0
msg_thead   db "Partition Status  Type                  Start MB   Size MB   Drive", 13, 10, 0
msg_sp4     db "    ", 0
msg_freesp  db "Free space: ", 0

mbr_code:
        incbin "build/mbr.bin", 0, 446

        section .bss
bss_start:
ndisks      resb 1
disk        resb 1
tmpdisk     resb 1
mode        resb 1
req_part    resb 1
lba         resb 1
            resb 1
io_cmd      resb 1
newmbr      resb 1
letter      resb 1
            alignb 4
req_mb      resd 1
total       resd 1
p_start     resd 1
p_size      resd 1
cyls        resw 1
spt         resw 1
heads       resw 1
slot        resw 1
chstmp      resb 4
dap         resb 16
edd         resb 0x42
optbuf      resb 20
u_numbuf    resb 16
u_lbuf      resb 84
            alignb 16
mbrbuf      resb 512
iobuf       resb 512
            resb 1024
stack_top:
