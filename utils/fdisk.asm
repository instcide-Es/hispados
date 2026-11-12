; =============================================================================
;  Hispa-DOS FDISK.COM - gestor de particiones de disco duro
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  FDISK                 menú interactivo
;  FDISK [n] /STATUS     tabla de particiones del disco n (1 = primero)
;  FDISK [n] /PRI[:MB]   crear partición primaria DOS (FAT16), activa si no
;                        hay otra; sin tamaño usa todo el espacio libre
;  FDISK [n] /ACT:p      marcar activa la partición p
;  FDISK [n] /DEL:p      borrar la partición p
;  FDISK [n] /MBR        escribir el código de arranque de Hispa-DOS en el MBR
;  FDISK [n] /EMPTY      nivel de error 0 si el disco no tiene particiones,
;                        1 si tiene alguna, 2 si no existe (para instaladores)
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
        mov ax, 0x4C02
        int 0x21
.opt:
        mov di, si
        call opt_word               ; -> optbuf
        call skip_word
        mov di, optbuf
        cmp dword [di], 'STAT'
        je .o_status
        cmp dword [di], 'EMPT'
        je .o_empty
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
.o_empty:
        mov byte [mode], 6
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
        cmp al, 6
        je .c_empty
        call install_mbr
        jc exit_err
        jmp exit_ok
.c_empty:
        mov si, mbrbuf+0x1BE
        mov cx, 4
.ce:
        cmp byte [si+4], 0
        jne .notempty
        add si, 16
        loop .ce
        mov si, msg_isempty
        call puts
        jmp exit_ok
.notempty:
        mov si, msg_notempty
        call puts
        jmp exit_err
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
        mov byte [superfl], 0
        cmp word [mbrbuf+510], 0xAA55
        je .sf
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
.sf:
        ; ¿disco sin tabla de particiones, con el sector de arranque FAT en el
        ; sector 0? (lo mismo que mira el núcleo). Lo que hay en el sitio de la
        ; tabla es código de arranque: se presenta como una sola partición que
        ; ocupa todo el disco; al eliminarla se escribe un MBR nuevo.
        cmp word [mbrbuf+0x0B], 512
        jne .ok
        cmp byte [mbrbuf+0x0D], 0
        je .ok
        cmp byte [mbrbuf+0x15], 0xF0
        jb .ok
        mov al, [mbrbuf+0x10]
        dec al
        cmp al, 1
        ja .ok
        mov di, mbrbuf
        mov cx, 256
        xor ax, ax
        rep stosw
        mov byte [mbrbuf+0x1BE+4], 0x06
        mov eax, [total]
        mov [mbrbuf+0x1BE+12], eax
        mov byte [newmbr], 1
        mov byte [superfl], 1
        jmp .ok

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
        mov byte [superfl], 0
        mov ah, 0x0D                ; el núcleo relee las particiones
        int 0x21
        clc
        ret
.err:
        mov si, msg_ioerr
        call puts
        stc
        ret

; CF (y mensaje) si el disco no tiene tabla de particiones
no_superfl:
        cmp byte [superfl], 0
        je .r                           ; CF = 0
        mov si, msg_superfl
        call puts
        mov si, msg_sfdel
        call puts
        stc
.r:
        ret

copy_mbr_code:
        mov si, mbr_code
        mov di, mbrbuf
        mov cx, 446
        rep movsb
        ret

install_mbr:
        call no_superfl                 ; ahí no hay MBR: es el arranque FAT
        jc .r
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
        cmp ecx, 2048                   ; más que el máximo = el máximo
        jae .c2
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
        cmp byte [superfl], 0
        je .nsp
        mov si, msg_sfdel
        call puts
.nsp:
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
        ; el primer sector de la partición, a cero: si ahí quedaba el arranque
        ; de una partición anterior (quizá mayor), el núcleo y FORMATEA lo
        ; tomarían por bueno y se escribiría fuera de la partición nueva
        push di
        mov di, iobuf
        mov cx, 256
        xor ax, ax
        rep stosw
        pop di
        mov eax, [p_start]
        mov bx, iobuf
        mov byte [io_cmd], 3
        call disk_io
        jnc .wm
        mov si, msg_ioerr
        call puts
        stc
        ret
.wm:
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
        call no_superfl
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
        cmp byte [superfl], 0
        je .nsf
        mov si, msg_superfl
        call puts
.nsf:
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
        db 0x0B, "No DOS   FAT32      ", 0
        db 0x0C, "No DOS   FAT32 LBA  ", 0
        db 0x07, "No DOS   NTFS/exFAT ", 0
        db 0x82, "No DOS   Linux swap ", 0
        db 0x83, "No DOS   Linux      ", 0
        db 0xEE, "No DOS   GPT        ", 0
        db 0xFF
tn_other    db "No DOS              ", 0

msg_title   db 13, 10, "Programa de configuración del disco duro de Hispa-DOS 0.9", 13, 10
            db "(C) 2026 colaboradores de Hispa-DOS - GPL-2.0 o posterior", 13, 10, 0
msg_nodisk  db "No hay discos duros.", 13, 10, 0
msg_baddisk db "Número de disco duro no válido.", 13, 10, 0
msg_badopt  db "Opción no válida. Use /STATUS /PRI[:MB] /ACT:n /DEL:n /MBR /EMPTY", 13, 10, 0
msg_isempty db "El disco duro no tiene particiones.", 13, 10, 0
msg_notempty db "El disco duro ya tiene particiones.", 13, 10, 0
msg_ioerr   db "Error al leer o escribir el disco duro.", 13, 10, 0
msg_menu1   db "                        Opciones de FDISK", 13, 10, 13, 10
            db "Disco duro actual: ", 0
msg_menu2   db 13, 10, "Elija una de las siguientes opciones:", 13, 10, 13, 10
            db "1. Crear una partición DOS", 13, 10
            db "2. Establecer la partición activa", 13, 10
            db "3. Eliminar una partición", 13, 10
            db "4. Mostrar la información de las particiones", 13, 10, 0
msg_menu5   db "5. Cambiar el disco duro actual", 13, 10, 0
msg_choice  db 13, 10, "Escriba su opción (Esc para salir): ", 0
msg_whichdisk db "Escriba el número de disco duro: ", 0
msg_askmb   db "Tamaño de la partición en MB (Intro = máximo, ", 0
msg_askmb2  db " MB): ", 0
msg_askact  db "Número de la partición que quiere activar: ", 0
msg_askdel  db "Número de la partición que quiere eliminar: ", 0
msg_sure    db "¡Atención! Se perderán los datos de la partición eliminada.", 13, 10
            db "¿Desea continuar (S/N)? ", 0
msg_esc     db "Pulse Intro o Esc para continuar", 0
msg_mbrok   db "Código de arranque principal actualizado.", 13, 10, 0
msg_full    db "No queda ninguna entrada libre en la tabla de particiones.", 13, 10, 0
msg_nospace db "No hay espacio libre suficiente en el disco.", 13, 10, 0
msg_created db "Partición DOS primaria creada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_nopart  db "No existe esa partición.", 13, 10, 0
msg_actok   db "Partición activa establecida.", 13, 10, 0
msg_delok   db "Partición eliminada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_curdisk db 13, 10, "Disco duro actual: ", 0
msg_open    db "  (", 0
msg_mbytes  db " MB", 0
msg_sectors db ", ", 0
msg_sects   db " sectores", 0
msg_lba     db ", LBA", 0
msg_close   db ")", 13, 10, 0
msg_noparts db "No hay particiones definidas.", 13, 10, 0
msg_thead   db "Partición Estado  Tipo                 Inicio MB Tamaño MB  Unidad", 13, 10, 0
msg_sp4     db "    ", 0
msg_freesp  db "Espacio libre: ", 0
msg_superfl db "Este disco no tiene tabla de particiones: todo él es una unidad FAT.", 13, 10, 0
msg_sfdel   db "Para particionarlo hay que eliminar antes esa unidad (partición 1).", 13, 10, 0

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
superfl     resb 1
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
