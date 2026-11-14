; =============================================================================
;  Hispa-DOS FDISK.COM (en el disco PARTIR.COM) - particiones de los discos
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  PARTIR                 menú interactivo (como FDISK de MS-DOS 6.22)
;  PARTIR [n] /STATUS     tabla de particiones del disco n (1 = primero)
;  PARTIR [n] /PRI[:MB]   crear partición primaria DOS (FAT16), activa si no
;                         hay otra; sin tamaño usa todo el espacio libre
;  PARTIR [n] /EXT[:MB]   crear la partición extendida (sin tamaño, todo lo
;                         libre)
;  PARTIR [n] /LOG[:MB]   crear una unidad lógica en la partición extendida
;  PARTIR [n] /ACT:p      marcar activa la partición p
;  PARTIR [n] /DEL:p      borrar la partición p (la extendida, si está vacía)
;  PARTIR [n] /DELLOG:u   borrar la unidad lógica u (1 = la primera)
;  PARTIR [n] /MBR        escribir el código de arranque de Hispa-DOS en el MBR
;  PARTIR [n] /EMPTY      nivel de error 0 si el disco no tiene particiones,
;                         1 si tiene alguna, 2 si no existe (para instaladores)
;  PARTIR /DISCO:n ...    lo mismo que PARTIR n ...
;  PARTIR [n] /INSTALAR   (para INSTALAR) elige el disco duro donde instalar
;                         (nunca el de arranque), ofrece crear la partición y
;                         sale con el nivel de error = código ASCII de la letra
;                         de la unidad (67 = C:...); 0 si no se hace nada, 1 si
;                         no se puede
;  PARTIR /ARRANQUE:X:    (para INSTALAR) deja arrancable la partición de X:
;                         (activa y, si el disco no tiene, con el MBR propio)
;
;  Varios discos: el 1 es el 80h de la BIOS, el 2 el 81h... Arrancando desde
;  una memoria USB, la BIOS la pone como disco 1 y el disco duro como 2:
;  PARTIR empieza entonces por el disco duro y enseña la memoria como
;  «Memoria USB de arranque (no tocar)». Cualquier cambio en el disco de
;  arranque pide escribir SI.
;
;  Tras cambiar la tabla llama a INT 21h AH=0Dh: el núcleo vuelve a leer las
;  particiones y asigna las letras de unidad sin reiniciar.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAX_FAT16   equ 2047 * 2048         ; 2047 MB en sectores
MIN_PART    equ 4200                ; ~2 MB: mínimo para FAT16
MAXDISK     equ 8
MAXLOG      equ 24
MODELEN     equ 41

IF_ATA      equ 1
IF_SATA     equ 2
IF_USB      equ 3
IF_SCSI     equ 4
IF_OTHER    equ 5

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
        ; (la línea de órdenes se copia antes: la DTA del PSP la pisa)
        mov si, 0x80
        mov di, cmdline
        mov cx, 128
        rep movsb
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        call find_boot
        call info_all
        mov byte [disk], 0x80
        call default_disk
        ; analizar la línea de órdenes
        mov si, cmdline + 1
.arg:
        lodsb
        cmp al, 13
        je .args_done
        cmp al, 0
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
        call set_disk_n
        jc .baddisk
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
        cmp dword [di], 'DELL'
        je .o_dellog
        cmp dword [di], 'DISC'
        je .o_disco
        cmp dword [di], 'INST'
        je .o_inst
        cmp dword [di], 'ARRA'
        je .o_arr
        cmp word [di], 'PR'
        jne .n1
        cmp byte [di+2], 'I'
        je .o_pri
.n1:
        cmp word [di], 'EX'
        jne .n3
        cmp byte [di+2], 'T'
        je .o_ext
.n3:
        cmp word [di], 'LO'
        jne .n4
        cmp byte [di+2], 'G'
        je .o_log
.n4:
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
.o_inst:
        mov byte [mode], 9
        jmp .arg
.o_pri:
        mov byte [mode], 2
        jmp .size
.o_ext:
        mov byte [mode], 7
        jmp .size
.o_log:
        mov byte [mode], 8
.size:
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
.o_dellog:
        mov byte [mode], 11
        push si
        call opt_colon
        jc .badopt_p
        call parse_num
        jc .badopt_p
        mov [req_part], al
        pop si
        jmp .arg
.o_disco:
        push si
        call opt_colon
        jc .badopt_p
        call parse_num
        jc .badopt_p
        pop si
        test eax, eax
        jz .baddisk
        cmp eax, 9
        ja .baddisk
        dec al
        call set_disk_n
        jc .baddisk
        jmp .arg
.o_arr:
        mov byte [mode], 10
        push si
        call opt_colon
        jc .badopt_p
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .badopt_p
        cmp al, 'Z'
        ja .badopt_p
        mov [arr_letter], al
        pop si
        jmp .arg
.badopt_p:
        pop si
        jmp .n2
.o_mbr:
        mov byte [mode], 5
        jmp .arg
.args_done:
        cmp byte [mode], 10
        je c_arr                    ; busca él mismo el disco
        cmp byte [mode], 9
        jne .ld
        cmp byte [explicit], 0
        jne .ld
        call inst_pick_disk
        jnc .ld
        mov si, msg_noother
        call puts
        jmp exit_err
.ld:
        call load_disk
        jc .ioerr
        movzx bx, byte [mode]
        shl bx, 1
        jmp [modetab+bx]
.ioerr:
        mov si, msg_ioerr
        call puts
exit_err:
        mov ax, 0x4C01
        int 0x21
exit_ok:
        mov ax, 0x4C00
        int 0x21

modetab dw menu, c_status, c_pri, c_act, c_del, c_mbr, c_empty, c_ext, c_log
        dw c_inst, c_arr, c_dellog

c_mbr:
        call install_mbr
        jc exit_err
        jmp exit_ok
c_empty:
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
c_status:
        call show_table
        jmp exit_ok
c_pri:
        mov eax, [req_mb]
        call create_primary
        jc exit_err
        call show_table
        jmp exit_ok
c_ext:
        mov eax, [req_mb]
        call create_ext
        jc exit_err
        call show_table
        jmp exit_ok
c_log:
        mov eax, [req_mb]
        call create_log
        jc exit_err
        call show_table
        jmp exit_ok
c_act:
        mov al, [req_part]
        call set_active
        jc exit_err
        call show_table
        jmp exit_ok
c_del:
        mov al, [req_part]
        call delete_part
        jc exit_err
        call show_table
        jmp exit_ok
c_dellog:
        mov al, [req_part]
        call delete_log
        jc exit_err
        call show_table
        jmp exit_ok

; AL = disco (0 = el primero) -> [disk]; CF si no existe
set_disk_n:
        cmp al, [ndisks]
        jae .bad
        add al, 0x80
        mov [disk], al
        mov byte [explicit], 1
        mov byte [autosw], 0
        clc
        ret
.bad:
        stc
        ret

; en optbuf, lo que va tras los dos puntos -> SI; CF si no hay
opt_colon:
        mov si, optbuf
.l:
        lodsb
        test al, al
        jz .no
        cmp al, ':'
        jne .l
        clc
        ret
.no:
        stc
        ret

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
        cmp al, 0
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
        cmp al, 0
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
;  Los discos: cuántos hay, cuál es el de arranque, tamaño y modelo
; -----------------------------------------------------------------------------
count_disks:
        mov byte [ndisks], 0
        push es
        mov ah, 8
        mov dl, 0x80
        xor di, di
        mov es, di
        int 0x13
        pop es
        jc .probe
        cmp dl, MAXDISK
        jbe .n
        mov dl, MAXDISK
.n:
        mov [ndisks], dl
        ; algunas BIOS, arrancando desde una memoria USB, cuentan solo esa
        ; memoria: se pregunta a los siguientes con AH=15h (AH=3: disco duro)
.probe:
        mov al, [ndisks]
        cmp al, MAXDISK
        jae .r
        mov dl, al
        add dl, 0x80
        mov ah, 0x15
        int 0x13
        jc .r
        cmp ah, 3
        jne .r
        inc byte [ndisks]
        jmp .probe
.r:
        ret

; el disco de arranque: el que tiene en el sector «ocultos» de la unidad de
; arranque su mismo sector de arranque -> [bootdisk] (0 = no es un disco
; duro), [bootusb] = 1 si es una memoria USB (la BIOS lo dice, EDD 3.0, o la
; unidad tiene el \INSTALL.BAT de la imagen USB)
find_boot:
        mov ax, 0x3305
        int 0x21
        cmp dl, 3
        jb .r                       ; disquete
        dec dl
        mov [bootltr], dl
        mov al, dl
        mov dword [pk_sec], 0
        mov word [pk_cnt], 1
        mov word [pk_off], bsbuf
        mov [pk_seg], ds
        mov cx, 0xFFFF
        mov bx, packet
        push bp
        int 0x25
        pop ax                      ; (las banderas que deja INT 25h)
        pop bp
        jc .r
        cmp word [bsbuf+0x0B], 512
        jne .r
        mov al, 0x80
.d:
        mov ah, al
        sub ah, 0x80
        cmp ah, [ndisks]
        jae .r
        mov [disk], al
        push ax
        call disk_geom
        jc .n
        mov eax, [bsbuf+0x1C]
        mov bx, iobuf
        mov byte [io_cmd], 2
        call disk_io
        jc .n
        mov si, bsbuf
        mov di, iobuf
        mov cx, 256
        repe cmpsw
        jne .n
        pop ax
        mov [bootdisk], al
        jmp .usb
.n:
        pop ax
        inc al
        jmp .d
.usb:
        call edd_info
        cmp byte [iface], IF_USB
        je .isusb
        mov al, [bootltr]
        add al, 'A'
        mov [s_instbat], al
        mov dx, s_instbat
        mov cx, 0x07
        mov ah, 0x4E
        int 0x21
        jc .r
.isusb:
        mov byte [bootusb], 1
.r:
        ret

; arrancando desde una memoria USB con otro disco: empezar por el disco duro
default_disk:
        cmp byte [bootdisk], 0
        je .r
        cmp byte [bootusb], 0
        je .r
        cmp byte [ndisks], 2
        jb .r
        call other_disk
        mov [disk], al
        mov byte [autosw], 1
.r:
        ret

; -> AL = el primer disco que no es el de arranque
other_disk:
        mov al, 0x80
        cmp al, [bootdisk]
        jne .r
        inc al
.r:
        ret

; /INSTALAR sin disco: el disco duro que no es el de arranque; CF si no hay
inst_pick_disk:
        mov byte [disk], 0x80
        cmp byte [bootdisk], 0
        je .ok
        cmp byte [ndisks], 2
        jb .no
        call other_disk
        mov [disk], al
.ok:
        clc
        ret
.no:
        stc
        ret

; tamaño, interfaz y modelo de cada disco
info_all:
        mov al, 0x80
.l:
        mov ah, al
        sub ah, 0x80
        cmp ah, [ndisks]
        jae .r
        mov [disk], al
        push ax
        movzx bx, ah
        shl bx, 2
        mov dword [d_size+bx], 0
        call disk_geom
        jc .n
        pop ax
        push ax
        movzx bx, ah
        shl bx, 2
        mov eax, [total]
        mov [d_size+bx], eax
        call edd_info
        pop ax
        push ax
        movzx bx, ah
        mov cl, [iface]
        mov [d_iface+bx], cl
        imul di, bx, MODELEN
        add di, d_model
        cmp cl, IF_USB
        je .n
        cmp cl, IF_SCSI
        je .n
        cmp cl, IF_OTHER
        je .n
        cmp byte [dpte_ok], 0
        je .n
        call ata_identify
.n:
        pop ax
        inc al
        jmp .l
.r:
        ret

; EDD 3.0 del disco [disk] -> [iface], [dpte_ok] y at_base/at_ctl/at_dev
edd_info:
        mov byte [iface], 0
        mov byte [dpte_ok], 0
        cmp byte [lba], 0
        je .r
        mov di, edd3
        mov cx, 0x4A / 2
        xor ax, ax
        rep stosw
        mov word [edd3], 0x42
        mov ah, 0x48
        mov dl, [disk]
        mov si, edd3
        int 0x13
        jc .r
        cmp word [edd3], 0x1E
        jb .r
        ; tabla de parámetros (DPTE): puertos del disco IDE
        les bx, [edd3+0x1A]
        mov ax, es
        push ds
        pop es
        cmp ax, 0xFFFF
        je .nod
        mov cx, ax
        or cx, bx
        jz .nod
        push ds
        mov ds, ax
        mov ax, [bx]
        mov cx, [bx+2]
        mov dl, [bx+4]
        pop ds
        test ax, ax
        jz .nod
        cmp ax, 0xFFFF
        je .nod
        mov [at_base], ax
        mov [at_ctl], cx
        mov [at_dev], dl
        mov byte [dpte_ok], 1
.nod:
        cmp word [edd3], 0x42
        jb .r
        cmp word [edd3+0x1E], 0xBEDD
        jne .r
        mov eax, [edd3+0x28]
        mov byte [iface], IF_USB
        cmp eax, 'USB '
        je .r
        and eax, 0x00FFFFFF
        cmp eax, 'USB'
        je .r
        mov eax, [edd3+0x28]
        mov byte [iface], IF_SATA
        cmp eax, 'SATA'
        je .r
        mov byte [iface], IF_ATA
        cmp eax, 'ATA '
        je .r
        mov byte [iface], IF_SCSI
        cmp eax, 'SCSI'
        je .r
        mov byte [iface], IF_OTHER
.r:
        ret

; IDENTIFY DEVICE por los puertos de la DPTE -> modelo en DS:DI (ASCIIZ);
; sin interrupción mientras se pregunta y con un tope de tiempo
ata_identify:
        push di
        mov dx, [at_ctl]
        mov al, 2
        out dx, al
        mov dx, [at_base]
        add dx, 6
        mov al, [at_dev]
        and al, 0x10
        or al, 0xA0
        out dx, al
        call ata_delay
        call ata_notbusy
        jc .fail
        mov dx, [at_base]
        add dx, 7
        mov al, 0xEC
        out dx, al
        call ata_delay
        call ata_drq
        jc .fail
        mov dx, [at_base]
        mov di, idbuf
        mov cx, 256
        rep insw
        add dx, 7
        in al, dx
        call ata_ien
        pop di
        ; palabras 27-46: el modelo, con los bytes de cada palabra al revés
        mov si, idbuf + 54
        mov cx, 20
        push di
.c:
        lodsw
        xchg al, ah
        stosw
        loop .c
        mov byte [di], 0
        ; sin los espacios del final
.t:
        cmp di, [esp]
        je .e
        cmp byte [di-1], ' '
        ja .e
        dec di
        mov byte [di], 0
        jmp .t
.e:
        pop di
        ; si el modelo no es texto, no se enseña
        mov si, di
.v:
        lodsb
        test al, al
        jz .ok
        cmp al, ' '
        jb .bad
        cmp al, 0x7E
        jbe .v
.bad:
        mov byte [di], 0
.ok:
        ret
.fail:
        call ata_ien
        pop di
        mov byte [di], 0
        ret

ata_ien:
        mov dx, [at_ctl]
        xor al, al
        out dx, al
        ret

ata_delay:
        push cx
        push dx
        mov dx, [at_ctl]
        mov cx, 5
.l:
        in al, dx
        loop .l
        pop dx
        pop cx
        ret

; esperar a que el disco no esté ocupado (medio segundo como mucho)
ata_notbusy:
        call tick0
.l:
        mov dx, [at_base]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        je .no
        test al, 0x80
        jz .ok
        call tick_over
        jnc .l
.no:
        stc
        ret
.ok:
        clc
        ret

; esperar a los datos (DRQ) o al error
ata_drq:
        call tick0
.l:
        mov dx, [at_base]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        je .no
        test al, 0x80
        jnz .w
        test al, 1
        jnz .no
        test al, 8
        jnz .ok
.w:
        call tick_over
        jnc .l
.no:
        stc
        ret
.ok:
        clc
        ret

tick0:
        push es
        push ax
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
        mov [t_start], ax
        pop ax
        pop es
        ret

; CF si han pasado más de 10 tics (medio segundo)
tick_over:
        push es
        push ax
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
        pop es
        sub ax, [t_start]
        cmp ax, 10
        cmc
        pop ax
        ret

; -----------------------------------------------------------------------------
;  Menú interactivo
; -----------------------------------------------------------------------------
menu:
        cmp byte [ndisks], 1
        jbe menu_loop
        call crlf
        call list_disks
        cmp byte [autosw], 0
        je menu_loop
        mov si, msg_auto1
        call puts
        movzx eax, byte [bootdisk]
        sub al, 0x7F
        call putnum
        mov si, msg_auto2
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov si, msg_auto3
        call puts
menu_loop:
        call crlf
        mov si, msg_menu1
        call puts
        call put_curdisk
        call crlf
        call put_bootline
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
        jbe menu_loop
        ; cambiar de disco
        call list_disks
        mov si, msg_whichdisk
        call puts
        movzx eax, byte [ndisks]
        call putnum
        mov si, msg_whichdisk2
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu_loop
        test eax, eax
        jz menu_loop
        cmp al, [ndisks]
        ja menu_loop
        add al, 0x7F
        mov [disk], al
        call load_disk
        jnc menu_loop
        mov si, msg_ioerr
        call puts
        jmp menu_loop
.display:
        call show_table
        call press_esc
        jmp menu_loop
.create:
        mov si, msg_cmenu
        call puts
        call submenu
        jc menu_loop
        mov [subopt], al
        call show_table
        mov al, [subopt]
        cmp al, '1'
        jne .c2
        call max_free_mb
        jmp .cask
.c2:
        cmp al, '2'
        jne .c3
        call free_space
        mov eax, edx
        shr eax, 11
        jmp .cask
.c3:
        call log_place
        mov eax, edx
        jnc .c3b
        xor eax, eax
.c3b:
        cmp eax, MAX_FAT16
        jbe .c3c
        mov eax, MAX_FAT16
.c3c:
        shr eax, 11
.cask:
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
        jc menu_loop
.cr:
        cmp byte [subopt], '1'
        jne .cr2
        call create_primary
        jmp .cdone
.cr2:
        cmp byte [subopt], '2'
        jne .cr3
        call create_ext
        jmp .cdone
.cr3:
        call create_log
.cdone:
        call press_esc
        jmp menu_loop
.active:
        call show_table
        mov si, msg_askact
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu_loop
        call set_active
        call press_esc
        jmp menu_loop
.delete:
        mov si, msg_dmenu
        call puts
        call submenu
        jc menu_loop
        cmp al, '2'
        ja .delete
        mov [subopt], al
        call show_table
        mov si, msg_askdel
        cmp byte [subopt], '1'
        je .dq
        mov si, msg_askdellog
.dq:
        call puts
        call readline
        mov si, u_line
        call parse_num
        jc menu_loop
        push ax
        mov si, msg_sure
        call puts
        call ask_yes
        pop ax
        jc menu_loop
        cmp byte [subopt], '1'
        jne .dl
        call delete_part
        jmp .dd
.dl:
        call delete_log
.dd:
        call press_esc
        jmp menu_loop

; tecla 1-3 de un submenú -> AL; CF con Esc
submenu:
        mov si, msg_subchoice
        call puts
.k:
        call getkey
        cmp al, 27
        je .esc
        cmp al, '1'
        jb .k
        cmp al, '3'
        ja .k
        call putc
        call crlf
        clc
        ret
.esc:
        call crlf
        stc
        ret

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

; la lista de los discos
list_disks:
        mov si, msg_disks
        call puts
        mov al, 0x80
.l:
        mov ah, al
        sub ah, 0x80
        cmp ah, [ndisks]
        jae .r
        push ax
        mov bl, ' '
        cmp al, [disk]
        jne .nc
        mov bl, '*'
.nc:
        push ax
        mov al, bl
        call putc
        mov al, ' '
        call putc
        pop ax
        movzx eax, ah
        inc eax
        mov cl, 3
        call putnum_w
        pop ax
        push ax
        movzx bx, ah
        shl bx, 2
        mov eax, [d_size+bx]
        shr eax, 11
        mov cl, 10
        call putnum_w
        mov si, msg_mbytes
        call puts
        mov si, msg_sp2
        call puts
        pop ax
        push ax
        call put_desc
        pop ax
        push ax
        cmp al, [bootdisk]
        jne .nb
        mov si, msg_dash
        call puts
        call put_bootlabel
.nb:
        call crlf
        pop ax
        inc al
        jmp .l
.r:
        ret

; AL = disco BIOS: modelo o tipo de conexión
put_desc:
        push ax
        sub al, 0x80
        movzx bx, al
        imul si, bx, MODELEN
        add si, d_model
        cmp byte [si], 0
        jne .p
        mov al, [d_iface+bx]
        mov si, msg_if_usb
        cmp al, IF_USB
        je .p
        mov si, msg_if_sata
        cmp al, IF_SATA
        je .p
        mov si, msg_if_ata
        cmp al, IF_ATA
        je .p
        mov si, msg_if_scsi
        cmp al, IF_SCSI
        je .p
        mov si, msg_if_hd
.p:
        call puts
        pop ax
        ret

put_bootlabel:
        mov si, msg_lbl_usb
        cmp byte [bootusb], 0
        jne .p
        mov si, msg_lbl_hd
.p:
        jmp puts

; «Disco actual: 2/2 (200 MB, modelo)»
put_curdisk:
        mov si, msg_curdisk
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov al, '/'
        call putc
        movzx eax, byte [ndisks]
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
        mov al, [disk]
        call put_desc
        mov al, ')'
        call putc
        ret

put_bootline:
        mov al, [disk]
        cmp al, [bootdisk]
        jne .r
        mov si, msg_isboot
        call puts
        call put_bootlabel
        call crlf
.r:
        ret

; Antes de cambiar el disco de arranque: pedir que se escriba SI. CF = no.
guard:
        mov al, [disk]
        cmp al, [bootdisk]
        jne .ok
        call crlf
        mov si, msg_g1
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov si, msg_g_usb
        cmp byte [bootusb], 0
        jne .p
        mov si, msg_g_hd
.p:
        call puts
        call readline
        mov al, [u_line]
        call upcase
        cmp al, 'S'
        jne .no
        mov al, [u_line+1]
        cmp al, 0xA1                ; í (CP437)
        je .i
        call upcase
        cmp al, 'I'
        jne .no
.i:
        cmp byte [u_line+2], 0
        jne .no
.ok:
        clc
        ret
.no:
        mov si, msg_untouched
        call puts
        stc
        ret

; -----------------------------------------------------------------------------
;  Acceso al disco
; -----------------------------------------------------------------------------
; geometría, tamaño y LBA del disco [disk]
disk_geom:
        push es
        mov ah, 8
        mov dl, [disk]
        xor di, di
        mov es, di
        int 0x13
        pop es
        jc .err
        movzx ax, cl
        and al, 0xC0
        shl ax, 2
        mov al, ch
        inc ax
        mov [cyls], ax
        and cx, 0x3F
        jz .err
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
        jc .ok
        cmp bx, 0xAA55
        jne .ok
        test cl, 1
        jz .ok
        mov byte [lba], 1
        mov word [edd], 0x1A
        mov ah, 0x48
        mov dl, [disk]
        mov si, edd
        int 0x13
        jc .ok
        mov eax, [edd+0x10]
        cmp dword [edd+0x14], 0
        je .tot
        mov eax, 0xFFFFFFFF
.tot:
        test eax, eax
        jz .ok
        mov [total], eax
.ok:
        clc
        ret
.err:
        stc
        ret

; leer geometría, tamaño, MBR y unidades lógicas del disco [disk]
load_disk:
        call disk_geom
        jc .err
        xor eax, eax
        mov bx, mbrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .err
        mov byte [newmbr], 0
        mov byte [superfl], 0
        mov byte [nocode], 0
        cmp word [mbrbuf+510], 0xAA55
        je .sf
        ; sin tabla válida: empezar de cero
        mov di, mbrbuf
        mov cx, 256
        xor ax, ax
        rep stosw
        mov byte [newmbr], 1
.ok:
        call scan_ext
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
        mov bx, mbrbuf
        call is_superfl
        jc .code
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
.code:
        ; tabla hecha por otro programa sin código de arranque (todo ceros):
        ; al escribir se pone el de Hispa-DOS, si no el disco no arrancaría
        mov si, mbrbuf
        mov cx, 440
.z:
        lodsb
        test al, al
        jnz .ok
        loop .z
        mov byte [nocode], 1
        jmp .ok

; BX = sector 0 -> CF=0 si es un arranque FAT (disco sin particiones)
is_superfl:
        cmp word [bx+0x0B], 512
        jne .no
        cmp byte [bx+0x0D], 0
        je .no
        cmp byte [bx+0x15], 0xF0
        jb .no
        mov al, [bx+0x10]
        dec al
        cmp al, 1
        ja .no
        clc
        ret
.no:
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

; EAX = LBA -> escribir un sector a cero ahí
zero_sector:
        push di
        push cx
        push eax
        mov di, zbuf
        mov cx, 256
        xor ax, ax
        rep stosw
        pop eax
        mov bx, zbuf
        mov byte [io_cmd], 3
        call disk_io
        pop cx
        pop di
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
        jne .code
        cmp byte [nocode], 0
        je .w
.code:
        call copy_mbr_code
.w:
        xor eax, eax
        mov bx, mbrbuf
        mov byte [io_cmd], 3
        call disk_io
        jc .err
        mov byte [newmbr], 0
        mov byte [superfl], 0
        mov byte [nocode], 0
        call reread
        call scan_ext
        clc
        ret
.err:
        mov si, msg_ioerr
        call puts
        stc
        ret

reread:
        mov ah, 0x0D                ; el núcleo relee las particiones
        int 0x21
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
        call guard
        jc .r
        call copy_mbr_code
        call write_mbr
        jc .r
        mov si, msg_mbrok
        call puts
.r:
        ret

; -----------------------------------------------------------------------------
;  Partición extendida y unidades lógicas
; -----------------------------------------------------------------------------
; AL = tipo -> ZF si es una partición extendida
is_ext_type:
        cmp al, 0x05
        je .r
        cmp al, 0x0F
        je .r
        cmp al, 0x85
.r:
        ret

; la partición extendida del disco actual y sus unidades lógicas
scan_ext:
        mov word [ext_slot], 0
        mov dword [ext_start], 0
        mov dword [ext_size], 0
        mov byte [nlogs], 0
        cmp byte [superfl], 0
        jne .r
        mov si, mbrbuf+0x1BE
        mov cx, 4
.l:
        mov al, [si]
        and al, 0x7F
        jnz .n
        mov al, [si+4]
        call is_ext_type
        jne .n
        cmp dword [si+8], 0
        je .n
        mov [ext_slot], si
        mov eax, [si+8]
        mov [ext_start], eax
        mov [wb_base], eax
        mov eax, [si+12]
        mov [ext_size], eax
        mov byte [rec], 1
        call walk_ebr
        mov byte [rec], 0
        mov eax, [wl_tail]
        mov [ext_tail], eax
        mov eax, [wl_end]
        mov [ext_used], eax
        mov al, [wl_fe]
        mov [ext_fe], al
        ret
.n:
        add si, 16
        loop .l
.r:
        ret

; recorrer la cadena de EBR desde [wb_base] -> [wl_count] unidades DOS,
; [wl_tail] último EBR (-1 si no hay ninguno válido), [wl_end] primer sector
; tras lo usado, [wl_fe] = 1 si el primer EBR no tiene unidad. Con [rec] = 1
; guarda las unidades en log_*.
walk_ebr:
        mov byte [wl_count], 0
        mov byte [wl_fe], 0
        mov dword [wl_tail], 0xFFFFFFFF
        mov dword [wl_prev], 0xFFFFFFFF
        mov eax, [wb_base]
        mov [wl_end], eax
        mov [wl_cur], eax
        mov byte [wl_guard], 64
.l:
        mov eax, [wl_cur]
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .r
        cmp word [ebrbuf+510], 0xAA55
        jne .r
        mov eax, [wl_cur]
        mov [wl_tail], eax
        inc eax
        cmp eax, [wl_end]
        jbe .e1
        mov [wl_end], eax
.e1:
        mov si, ebrbuf+0x1BE
        mov cl, [si+4]
        test cl, cl
        jz .empty
        cmp dword [si+8], 0
        je .empty
        cmp dword [si+12], 0
        je .empty
        mov eax, [si+8]
        add eax, [wl_cur]
        mov edx, eax
        add edx, [si+12]
        cmp edx, [wl_end]
        jbe .e2
        mov [wl_end], edx
.e2:
        push ax
        mov al, cl
        call is_dos
        pop ax
        jc .nd
        inc byte [wl_count]
.nd:
        cmp byte [rec], 0
        je .nx
        movzx bx, byte [nlogs]
        cmp bx, MAXLOG
        jae .nx
        mov [log_type+bx], cl
        shl bx, 2
        mov [log_start+bx], eax
        mov edx, [si+12]
        mov [log_size+bx], edx
        mov edx, [wl_cur]
        mov [log_ebr+bx], edx
        mov edx, [wl_prev]
        mov [log_prev+bx], edx
        inc byte [nlogs]
        jmp .nx
.empty:
        mov eax, [wl_cur]
        cmp eax, [wb_base]
        jne .nx
        mov byte [wl_fe], 1
.nx:
        mov eax, [wl_cur]
        mov [wl_prev], eax
        mov si, ebrbuf+0x1CE
        mov al, [si+4]
        call is_ext_type
        jne .r
        mov eax, [si+8]
        test eax, eax
        jz .r
        add eax, [wb_base]
        cmp eax, [wl_cur]
        je .r
        mov [wl_cur], eax
        dec byte [wl_guard]
        jnz .l
.r:
        ret

; dónde iría una unidad lógica nueva -> [n_ebr], [n_prev] (-1: ninguno),
; [p_start], EDX = sectores libres; CF si no hay partición extendida o sitio
log_place:
        cmp word [ext_slot], 0
        je .no
        mov dword [n_prev], 0xFFFFFFFF
        mov eax, [ext_start]
        mov [n_ebr], eax
        cmp dword [ext_tail], 0xFFFFFFFF
        je .pl                          ; aún sin EBR
        cmp byte [ext_fe], 0
        je .after
        cmp byte [nlogs], 0
        jne .gap
        cmp eax, [ext_tail]
        je .pl                          ; un primer EBR vacío y nada más
        jmp .after
.gap:
        ; el primer EBR vacío (se borró su unidad) y otras detrás: el hueco
        ; hasta el EBR siguiente, si cabe una unidad
        movzx edx, word [spt]
        add eax, edx
        mov [p_start], eax
        mov edx, [log_ebr]
        sub edx, eax
        jbe .after
        cmp edx, MIN_PART
        jb .after
        clc
        ret
.after:
        mov eax, [ext_tail]
        mov [n_prev], eax
        mov eax, [ext_used]
        call align_cyl
        mov [n_ebr], eax
.pl:
        mov eax, [n_ebr]
        movzx edx, word [spt]
        add eax, edx
        mov [p_start], eax
        mov edx, [ext_start]
        add edx, [ext_size]
        sub edx, eax
        jbe .no
        cmp edx, MIN_PART
        jb .no
        clc
        ret
.no:
        xor edx, edx
        stc
        ret

; EAX = sector -> EAX redondeado al cilindro siguiente
align_cyl:
        push ebx
        push edx
        movzx ebx, word [spt]
        movzx edx, word [heads]
        imul ebx, edx
        add eax, ebx
        dec eax
        xor edx, edx
        div ebx
        imul eax, ebx
        pop edx
        pop ebx
        ret

; ¿pasa [p_start]+[p_size] del cilindro 1024 (fuera del alcance de CHS)?
; -> CF=1 si pasa
beyond_chs:
        push eax
        push ecx
        push edx
        mov eax, [p_start]
        add eax, [p_size]
        movzx ecx, word [spt]
        movzx edx, word [heads]
        imul ecx, edx
        imul ecx, ecx, 1024
        cmp ecx, eax
        pop edx
        pop ecx
        pop eax
        ret

; tipo de una unidad DOS de [p_size] sectores en [p_start] -> BL
dos_type:
        mov bl, 0x04
        cmp dword [p_size], 65536
        jb .r
        mov bl, 0x06
        call beyond_chs
        jnc .r
        mov bl, 0x0E
.r:
        ret

; DI = entrada de la tabla, BL = tipo: rellenarla con [p_start] (relativo a
; ECX) y [p_size]; el estado, a 0
fill_entry:
        push eax
        mov byte [di], 0
        mov [di+4], bl
        mov eax, [p_start]
        sub eax, ecx
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
        pop eax
        ret

; EAX = MB pedidos (0 = todo), EDX = sectores libres, ECX = tope -> [p_size]
pick_size:
        cmp edx, ecx
        jbe .c1
        mov edx, ecx
.c1:
        test eax, eax
        jz .c2
        cmp eax, 0x1FFFFF
        jae .c2
        shl eax, 11
        cmp eax, edx
        ja .c2
        mov edx, eax
.c2:
        mov [p_size], edx
        ret

; EAX = MB (0 = todo lo libre) -> crear la partición extendida
create_ext:
        mov [req_mb], eax
        call no_superfl
        jc .r
        cmp word [ext_slot], 0
        je .e
        mov si, msg_hasext
        call puts
        stc
        ret
.e:
        call free_slot
        jc .r
        call free_space
        mov [p_start], eax
        mov eax, [req_mb]
        mov ecx, 0xFFFFFFFF
        call pick_size
        cmp dword [p_size], MIN_PART
        jae .big
        mov si, msg_nospace
        call puts
        stc
        ret
.big:
        call guard
        jc .r
        mov bl, 0x05
        call beyond_chs
        jnc .t
        mov bl, 0x0F
.t:
        mov di, [slot]
        xor ecx, ecx
        call fill_entry
        ; el primer sector, a cero: la cadena de unidades lógicas, vacía
        mov eax, [p_start]
        call zero_sector
        jc .ioerr
        call write_mbr
        jc .r
        mov si, msg_extok
        call puts
        clc
.r:
        ret
.ioerr:
        mov si, msg_ioerr
        call puts
        stc
        ret

; EAX = MB (0 = lo máximo) -> crear una unidad lógica en la extendida
create_log:
        mov [req_mb], eax
        cmp word [ext_slot], 0
        jne .e
        mov si, msg_noext
        call puts
        stc
        ret
.e:
        cmp byte [nlogs], MAXLOG
        jb .e2
        mov si, msg_full
        call puts
        stc
        ret
.e2:
        call log_place
        jnc .sz
        mov si, msg_nospace
        call puts
        stc
        ret
.sz:
        mov eax, [req_mb]
        mov ecx, MAX_FAT16
        call pick_size
        cmp dword [p_size], MIN_PART
        jae .big
        mov si, msg_nospace
        call puts
        stc
        ret
.big:
        call guard
        jc .r
        ; el EBR nuevo, en iobuf (si se aprovecha el primero, con su enlace)
        mov di, iobuf
        mov cx, 256
        xor ax, ax
        rep stosw
        cmp dword [n_prev], 0xFFFFFFFF
        jne .nolink
        cmp dword [ext_tail], 0xFFFFFFFF
        je .nolink
        mov eax, [n_ebr]
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ioerr
        cmp word [ebrbuf+510], 0xAA55
        jne .nolink
        mov si, ebrbuf+0x1CE
        mov di, iobuf+0x1CE
        mov cx, 16
        rep movsb
.nolink:
        call dos_type
        mov di, iobuf+0x1BE
        mov ecx, [n_ebr]
        call fill_entry
        mov word [iobuf+510], 0xAA55
        ; el primer sector de la unidad, a cero (como con las primarias)
        mov eax, [p_start]
        call zero_sector
        jc .ioerr
        mov eax, [n_ebr]
        mov bx, iobuf
        mov byte [io_cmd], 3
        call disk_io
        jc .ioerr
        ; y el enlace desde el EBR anterior, lo último: hasta aquí la unidad
        ; nueva no está en la cadena
        mov eax, [n_prev]
        cmp eax, 0xFFFFFFFF
        je .done
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ioerr
        mov eax, [p_start]
        add eax, [p_size]
        mov ecx, [n_ebr]
        sub eax, ecx
        mov [p_size], eax
        mov [p_start], ecx
        mov ecx, [ext_start]
        mov di, ebrbuf+0x1CE
        mov bl, 0x05
        call fill_entry
        mov eax, [n_prev]
        mov bx, ebrbuf
        mov byte [io_cmd], 3
        call disk_io
        jc .ioerr
.done:
        call reread
        call scan_ext
        mov si, msg_logok
        call puts
        clc
.r:
        ret
.ioerr:
        call reread
        call scan_ext
        mov si, msg_ioerr
        call puts
        stc
        ret

; AL = unidad lógica (1 = la primera) -> borrarla
delete_log:
        cmp al, 1
        jb .bad
        cmp al, [nlogs]
        jbe .ok
.bad:
        mov si, msg_nolog
        call puts
        stc
        ret
.ok:
        movzx bx, al
        dec bx
        shl bx, 2
        mov [slot], bx
        call guard
        jc .r
        mov bx, [slot]
        mov eax, [log_ebr+bx]
        cmp eax, [ext_start]
        jne .mid
        ; la del primer EBR: se vacía su entrada (el enlace se queda)
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ioerr
        mov di, ebrbuf+0x1BE
        mov cx, 16
        xor al, al
        rep stosb
        mov eax, [ext_start]
        mov bx, ebrbuf
        mov byte [io_cmd], 3
        call disk_io
        jc .ioerr
        jmp .done
.mid:
        ; las demás: el EBR anterior pasa a enlazar con el siguiente
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ioerr
        mov si, ebrbuf+0x1CE
        mov di, link16
        mov cx, 16
        rep movsb
        mov bx, [slot]
        mov eax, [log_prev+bx]
        mov bx, ebrbuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ioerr
        mov si, link16
        mov di, ebrbuf+0x1CE
        mov cx, 16
        rep movsb
        mov bx, [slot]
        mov eax, [log_prev+bx]
        mov bx, ebrbuf
        mov byte [io_cmd], 3
        call disk_io
        jc .ioerr
        mov bx, [slot]
        mov eax, [log_ebr+bx]
        call zero_sector
        jc .ioerr
.done:
        call reread
        call scan_ext
        mov si, msg_logdel
        call puts
        clc
.r:
        ret
.ioerr:
        call reread
        call scan_ext
        mov si, msg_ioerr
        call puts
        stc
        ret

; -----------------------------------------------------------------------------
;  Particiones primarias
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
        call align_cyl
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

; -> [slot] = una entrada libre de la tabla; CF (y mensaje) si no hay
free_slot:
        mov si, mbrbuf+0x1BE
        mov cx, 4
.slot:
        cmp byte [si+4], 0
        je .got
        add si, 16
        loop .slot
        mov si, msg_full
        call puts
        stc
        ret
.got:
        mov [slot], si
        clc
        ret

; EAX = MB (0 = máximo) -> crear partición primaria DOS
create_primary:
        mov [req_mb], eax
        call free_slot
        jc .r
        call free_space
        mov [p_start], eax
        mov eax, [req_mb]
        cmp eax, 2048                   ; más que el máximo = el máximo
        jb .c1
        xor eax, eax
.c1:
        mov ecx, MAX_FAT16
        call pick_size
        cmp dword [p_size], MIN_PART
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
        call guard
        jc .r
        call dos_type
        mov di, [slot]
        xor ecx, ecx
        call fill_entry
        ; activa si no hay ninguna
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
        mov eax, [p_start]
        call zero_sector
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
        mov al, [si+4]
        call is_ext_type
        jne .ne
        mov si, msg_actext
        call puts
        stc
        ret
.ne:
        push si
        call guard
        pop si
        jc .r
        call activate
        call write_mbr
        jc .r
        mov si, msg_actok
        call puts
        clc
.r:
        ret

; SI = entrada -> la única activa
activate:
        mov di, mbrbuf+0x1BE
        mov cx, 4
.l:
        mov byte [di], 0
        add di, 16
        loop .l
        mov byte [si], 0x80
        ret

delete_part:
        call part_entry
        jc .r
        mov al, [si+4]
        call is_ext_type
        jne .ne
        cmp byte [nlogs], 0
        je .ne
        mov si, msg_extfull
        call puts
        stc
        ret
.ne:
        push si
        call guard
        pop si
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
;  /INSTALAR y /ARRANQUE:X: (los usa INSTALAR)
; -----------------------------------------------------------------------------
; primera partición DOS primaria -> AL = su letra, [req_part]; CF si no hay
first_dos_prim:
        call calc_letters
        mov si, mbrbuf+0x1BE
        mov cx, 1
.l:
        mov al, [si]
        and al, 0x7F
        jnz .n
        cmp dword [si+12], 0
        je .n
        mov al, [si+4]
        call is_dos
        jc .n
        mov [req_part], cl
        mov [slot], si
        mov al, [pletter]
        clc
        ret
.n:
        add si, 16
        inc cx
        cmp cx, 4
        jbe .l
        stc
        ret

c_inst:
        call show_table
        call first_dos_prim
        jnc .have
        ; ¿sin ninguna partición?
        mov si, mbrbuf+0x1BE
        mov cx, 4
.ce:
        cmp byte [si+4], 0
        jne .some
        add si, 16
        loop .ce
        mov si, msg_isempty
        call puts
        mov si, msg_inst_all
        call puts
        call ask_yes
        jc .cancel
        jmp .create
.some:
        mov si, msg_inst_nodos
        call puts
        call max_free_mb
        cmp eax, 3
        jae .free
        mov si, msg_inst_full
        call puts
        jmp exit_err
.free:
        push eax
        mov si, msg_inst_free1
        call puts
        pop eax
        call putnum
        mov si, msg_inst_free2
        call puts
        call ask_yes
        jc .cancel
.create:
        xor eax, eax
        call create_primary
        jc exit_err
        call load_disk
        jc exit_err
        call first_dos_prim
        jc exit_err
.have:
        push ax
        mov [letter], al
        mov si, msg_inst_drv1
        call puts
        mov al, [letter]
        call putc
        mov si, msg_inst_drv2
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov si, msg_inst_drv3
        call puts
        movzx eax, byte [req_part]
        call putnum
        mov si, msg_sectors
        call puts
        mov si, [slot]
        mov eax, [si+12]
        shr eax, 11
        call putnum
        mov si, msg_mbytes
        call puts
        mov si, msg_inst_drv4
        call puts
        pop ax
        mov ah, 0x4C
        int 0x21
.cancel:
        mov si, msg_untouched
        call puts
        jmp exit_ok

; /ARRANQUE:X: la partición de la unidad X: (la del sector de arranque que se
; lee por el núcleo), activa y con el MBR de Hispa-DOS si el disco no tiene
c_arr:
        mov al, [arr_letter]
        sub al, 'A'
        mov dword [pk_sec], 0
        mov word [pk_cnt], 1
        mov word [pk_off], bsbuf
        mov [pk_seg], ds
        mov cx, 0xFFFF
        mov bx, packet
        push bp
        int 0x25
        pop ax
        pop bp
        jc .notpart
        cmp word [bsbuf+0x0B], 512
        jne .notpart
        mov al, 0x80
.d:
        mov ah, al
        sub ah, 0x80
        cmp ah, [ndisks]
        jae .notpart
        mov [disk], al
        push ax
        call load_disk
        jc .nd
        cmp byte [superfl], 0
        jne .nd
        mov si, mbrbuf+0x1BE
        mov cx, 4
.e:
        mov al, [si+4]
        call is_dos
        jc .ne
        mov eax, [si+8]
        cmp eax, [bsbuf+0x1C]
        jne .ne
        mov bx, iobuf
        mov byte [io_cmd], 2
        call disk_io
        jc .ne
        push si
        push cx
        mov si, bsbuf
        mov di, iobuf
        mov cx, 256
        repe cmpsw
        pop cx
        pop si
        je .found
.ne:
        add si, 16
        loop .e
.nd:
        pop ax
        inc al
        jmp .d
.found:
        pop ax
        mov [slot], si
        mov al, 5
        sub al, cl
        mov [req_part], al
        ; ¿ya está? (la única activa y con código de arranque)
        mov byte [letter], 0
        cmp byte [newmbr], 0
        jne .need
        cmp byte [nocode], 0
        jne .need
        mov di, mbrbuf+0x1BE
        mov cx, 4
.a:
        mov al, [di]
        cmp di, si
        je .self
        test al, 0x80
        jnz .need
        jmp .an
.self:
        test al, 0x80
        jz .need
.an:
        add di, 16
        loop .a
        jmp .ready
.need:
        mov al, [newmbr]
        or al, [nocode]
        mov [letter], al            ; (1 = se pone el MBR de Hispa-DOS)
        push si
        call guard
        pop si
        jc exit_err
        call activate
        call write_mbr
        jc exit_err
.ready:
        mov si, msg_arr1
        call puts
        movzx eax, byte [req_part]
        call putnum
        mov si, msg_arr2
        call puts
        movzx eax, byte [disk]
        sub al, 0x7F
        call putnum
        mov si, msg_arr3
        call puts
        cmp byte [letter], 0
        je .nm
        mov si, msg_arr4
        call puts
.nm:
        call crlf
        jmp exit_ok
.notpart:
        mov si, msg_arr_no1
        call puts
        mov al, [arr_letter]
        call putc
        mov si, msg_arr_no2
        call puts
        jmp exit_err

; -----------------------------------------------------------------------------
;  Letras de unidad (como las da el núcleo: primero las primarias de cada
;  disco y después las lógicas de cada disco)
; -----------------------------------------------------------------------------
; -> [pletter] = letra de la primera primaria DOS del disco actual,
;    [lletter] = la de su primera unidad lógica
calc_letters:
        mov byte [pletter], 'C'
        mov byte [lletter], 'C'
        mov byte [cl_tmp], 'C'      ; lógicas de los discos anteriores
        mov al, 0x80
.l:
        mov ah, al
        sub ah, 0x80
        cmp ah, [ndisks]
        jae .e
        push ax
        call count_disk             ; BL primarias, BH lógicas
        pop ax
        add [lletter], bl
        cmp al, [disk]
        jae .cur
        add [pletter], bl
        add [cl_tmp], bh
.cur:
        inc al
        jmp .l
.e:
        mov al, [cl_tmp]
        sub al, 'C'
        add [lletter], al
        ret

; AL = disco BIOS -> BL = unidades primarias DOS, BH = unidades lógicas DOS
; (sin cambiar los datos del disco actual)
count_disk:
        push word [disk]
        push word [lba]
        push dword [total]
        push word [spt]
        push word [heads]
        push word [cyls]
        mov [disk], al
        mov word [cd_cnt], 0
        call disk_geom
        jc .r
        xor eax, eax
        mov bx, iobuf
        mov byte [io_cmd], 2
        call disk_io
        jc .r
        cmp word [iobuf+510], 0xAA55
        jne .r
        mov bx, iobuf
        call is_superfl
        jc .pt
        mov byte [cd_cnt], 1
        jmp .r
.pt:
        mov dword [wb_base], 0
        mov si, iobuf+0x1BE
        mov cx, 4
.l:
        mov al, [si]
        and al, 0x7F
        jnz .n
        cmp dword [si+8], 0
        je .n
        cmp dword [si+12], 0
        je .n
        mov al, [si+4]
        call is_ext_type
        jne .nx
        cmp dword [wb_base], 0
        jne .n
        mov eax, [si+8]
        mov [wb_base], eax
        jmp .n
.nx:
        call is_dos
        jc .n
        inc byte [cd_cnt]
.n:
        add si, 16
        loop .l
        cmp dword [wb_base], 0
        je .r
        mov byte [rec], 0
        call walk_ebr
        mov al, [wl_count]
        mov [cd_cnt+1], al
.r:
        pop word [cyls]
        pop word [heads]
        pop word [spt]
        pop dword [total]
        pop word [lba]
        pop word [disk]
        mov bx, [cd_cnt]
        ret

; -----------------------------------------------------------------------------
;  Mostrar la tabla
; -----------------------------------------------------------------------------
show_table:
        call calc_letters
        mov al, [pletter]
        mov [letter], al
        call crlf
        call put_curdisk
        call crlf
        call put_bootline
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
        ; las unidades lógicas de la partición extendida
        cmp word [ext_slot], 0
        je .r
        call crlf
        mov si, msg_loghead
        call puts
        cmp byte [nlogs], 0
        jne .lh
        mov si, msg_nologs
        call puts
        jmp .lfree
.lh:
        mov si, msg_lthead
        call puts
        mov al, [lletter]
        mov [letter], al
        xor bx, bx
.lr:
        cmp bl, [nlogs]
        jae .lfree
        push bx
        movzx eax, bl
        inc eax
        mov cl, 6
        call putnum_w
        mov si, msg_sp8
        call puts
        mov al, [log_type+bx]
        push ax
        call is_dos
        pop ax
        jc .lnd
        mov si, tn_log16
        cmp al, 0x01
        jne .lp
        mov si, tn_log12
        jmp .lp
.lnd:
        call type_name
        mov si, di
.lp:
        call puts
        shl bx, 2
        mov eax, [log_start+bx]
        shr eax, 11
        mov cl, 10
        call putnum_w
        mov eax, [log_size+bx]
        shr eax, 11
        mov cl, 10
        call putnum_w
        shr bx, 2
        mov al, [log_type+bx]
        call is_dos
        jc .lnl
        mov si, msg_sp4
        call puts
        mov al, [letter]
        call putc
        mov al, ':'
        call putc
        inc byte [letter]
.lnl:
        call crlf
        pop bx
        inc bx
        jmp .lr
.lfree:
        mov si, msg_extfree
        call puts
        call log_place
        mov eax, edx
        jnc .lf
        xor eax, eax
.lf:
        shr eax, 11
        call putnum
        mov si, msg_mbytes
        call puts
        call crlf
.r:
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
        db 0x85, "EXT Linux           ", 0
        db 0x0B, "No DOS   FAT32      ", 0
        db 0x0C, "No DOS   FAT32 LBA  ", 0
        db 0x07, "No DOS   NTFS/exFAT ", 0
        db 0x82, "No DOS   Linux swap ", 0
        db 0x83, "No DOS   Linux      ", 0
        db 0xEE, "No DOS   GPT        ", 0
        db 0xFF
tn_other    db "No DOS              ", 0
tn_log16    db "LOG DOS  FAT16      ", 0
tn_log12    db "LOG DOS  FAT12      ", 0

s_instbat   db "C:\INSTALL.BAT", 0

msg_title   db 13, 10, "Programa de configuración del disco duro de Hispa-DOS 0.9", 13, 10
            db "(C) 2026 colaboradores de Hispa-DOS - GPL-2.0 o posterior", 13, 10, 0
msg_nodisk  db "No hay discos duros.", 13, 10, 0
msg_baddisk db "Número de disco duro no válido.", 13, 10, 0
msg_badopt  db "Opción no válida. Use /STATUS /PRI[:MB] /EXT[:MB] /LOG[:MB] /ACT:n /DEL:n", 13, 10
            db "/DELLOG:n /MBR /EMPTY /DISCO:n", 13, 10, 0
msg_isempty db "El disco duro no tiene particiones.", 13, 10, 0
msg_notempty db "El disco duro ya tiene particiones.", 13, 10, 0
msg_ioerr   db "Error al leer o escribir el disco duro. No se ha terminado el cambio.", 13, 10, 0
msg_menu1   db "                  Opciones de PARTIR (FDISK)", 13, 10, 13, 10, 0
msg_menu2   db 13, 10, "Elija una de las siguientes opciones:", 13, 10, 13, 10
            db "1. Crear una partición o una unidad lógica DOS", 13, 10
            db "2. Establecer la partición activa", 13, 10
            db "3. Eliminar una partición o una unidad lógica DOS", 13, 10
            db "4. Mostrar la información de las particiones", 13, 10, 0
msg_menu5   db "5. Cambiar la unidad de disco actual", 13, 10, 0
msg_choice  db 13, 10, "Escriba su opción (Esc para salir): ", 0
msg_subchoice db 13, 10, "Escriba su opción (Esc para volver): ", 0
msg_cmenu   db "Crear una partición o una unidad lógica DOS", 13, 10, 13, 10
            db "1. Crear una partición DOS primaria", 13, 10
            db "2. Crear una partición DOS extendida", 13, 10
            db "3. Crear unidades lógicas DOS en la partición extendida", 13, 10, 0
msg_dmenu   db "Eliminar una partición o una unidad lógica DOS", 13, 10, 13, 10
            db "1. Eliminar una partición (primaria, extendida o de otro sistema)", 13, 10
            db "2. Eliminar una unidad lógica de la partición extendida", 13, 10, 0
msg_disks   db "Discos duros que ve la BIOS:", 13, 10
            db "  Disco    Tamaño", 13, 10, 0
msg_dash    db "  - ", 0
msg_sp2     db "  ", 0
msg_sp8     db "        ", 0
msg_whichdisk db "Escriba el número de disco duro (1-", 0
msg_whichdisk2 db "): ", 0
msg_auto1   db 13, 10, "Se ha arrancado desde una memoria USB (el disco ", 0
msg_auto2   db "): se empieza por el disco ", 0
msg_auto3   db ",", 13, 10, "el disco duro. Con la opción 5 se cambia de disco.", 13, 10, 0
msg_askmb   db "Tamaño en MB (Intro = máximo, ", 0
msg_askmb2  db " MB): ", 0
msg_askact  db "Número de la partición que quiere activar: ", 0
msg_askdel  db "Número de la partición que quiere eliminar: ", 0
msg_askdellog db "Número de la unidad lógica que quiere eliminar: ", 0
msg_sure    db "¡Atención! Se perderán los datos de la partición eliminada.", 13, 10
            db "¿Desea continuar (S/N)? ", 0
msg_esc     db "Pulse Intro o Esc para continuar", 0
msg_mbrok   db "Código de arranque principal actualizado.", 13, 10, 0
msg_full    db "No queda ninguna entrada libre en la tabla de particiones.", 13, 10, 0
msg_nospace db "No hay espacio libre suficiente en el disco.", 13, 10, 0
msg_created db "Partición DOS primaria creada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_extok   db "Partición DOS extendida creada. Ahora se pueden crear unidades lógicas en ella.", 13, 10, 0
msg_logok   db "Unidad lógica DOS creada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_logdel  db "Unidad lógica eliminada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_hasext  db "Este disco ya tiene una partición extendida.", 13, 10, 0
msg_noext   db "Este disco no tiene partición extendida: créela antes.", 13, 10, 0
msg_extfull db "La partición extendida tiene unidades lógicas: elimínelas antes.", 13, 10, 0
msg_nolog   db "No existe esa unidad lógica.", 13, 10, 0
msg_actext  db "Una partición extendida no puede ser la activa.", 13, 10, 0
msg_nopart  db "No existe esa partición.", 13, 10, 0
msg_actok   db "Partición activa establecida.", 13, 10, 0
msg_delok   db "Partición eliminada. Se han actualizado las letras de unidad.", 13, 10, 0
msg_curdisk db "Disco actual: ", 0
msg_isboot  db "  Atención: ", 0
msg_lbl_usb db "Memoria USB de arranque (no tocar)", 0
msg_lbl_hd  db "Disco de arranque (el sistema en uso)", 0
msg_if_usb  db "Memoria o disco USB", 0
msg_if_sata db "Disco SATA", 0
msg_if_ata  db "Disco IDE (ATA)", 0
msg_if_scsi db "Disco SCSI", 0
msg_if_hd   db "Disco duro", 0
msg_open    db "  (", 0
msg_mbytes  db " MB", 0
msg_sectors db ", ", 0
msg_noparts db "No hay particiones definidas.", 13, 10, 0
msg_thead   db "Partición Estado  Tipo                 Inicio MB Tamaño MB  Unidad", 13, 10, 0
msg_loghead db "Unidades lógicas de la partición extendida:", 13, 10, 0
msg_nologs  db "  (ninguna)", 13, 10, 0
msg_lthead  db "Unidad lóg.     Tipo                 Inicio MB Tamaño MB  Unidad", 13, 10, 0
msg_extfree db "Espacio libre en la partición extendida: ", 0
msg_sp4     db "    ", 0
msg_freesp  db "Espacio libre: ", 0
msg_superfl db "Este disco no tiene tabla de particiones: todo él es una unidad FAT.", 13, 10, 0
msg_sfdel   db "Para particionarlo hay que eliminar antes esa unidad (partición 1).", 13, 10, 0
msg_g1      db "¡Atención! El disco ", 0
msg_g_usb   db " es la MEMORIA USB DE ARRANQUE: desde ella funciona", 13, 10
            db "Hispa-DOS ahora mismo. Si la cambia puede perder lo que tiene y dejar", 13, 10
            db "de arrancar. El disco duro del ordenador es otro (opción 5 o PARTIR n).", 13, 10
            db "Para cambiar la memoria USB de todos modos escriba SI y pulse Intro: ", 0
msg_g_hd    db " es el DISCO DE ARRANQUE: desde él funciona Hispa-DOS", 13, 10
            db "ahora mismo. Si lo cambia puede perder lo que tiene y dejar de arrancar.", 13, 10
            db "Para cambiarlo de todos modos escriba SI y pulse Intro: ", 0
msg_untouched db "No se ha cambiado nada.", 13, 10, 0
msg_noother db "No hay ningún disco duro aparte del de arranque: no hay dónde instalar.", 13, 10, 0
msg_inst_all db "Se puede crear una partición para Hispa-DOS con todo el disco (hasta", 13, 10
            db "2.047 MB). ¡Se borrará todo lo que haya en este disco duro!", 13, 10
            db "¿Crear la partición (S/N)? ", 0
msg_inst_nodos db "El disco duro ya tiene particiones, pero ninguna primaria de DOS (FAT16).", 13, 10, 0
msg_inst_full db "No queda espacio libre en él. Para hacer sitio, elimine alguna partición con", 13, 10
            db "PARTIR (opción 3; se pierde lo que tenga) y vuelva a escribir INSTALAR.", 13, 10, 0
msg_inst_free1 db "Hay ", 0
msg_inst_free2 db " MB libres sin usar. ¿Crear ahí una partición DOS para Hispa-DOS?", 13, 10
            db "No se borra nada de lo que ya hay (S/N): ", 0
msg_inst_drv1 db 13, 10, "Unidad para Hispa-DOS: ", 0
msg_inst_drv2 db ": (disco ", 0
msg_inst_drv3 db ", partición ", 0
msg_inst_drv4 db ").", 13, 10, 0
msg_arr1    db "La partición ", 0
msg_arr2    db " del disco ", 0
msg_arr3    db " es la activa: el ordenador arrancará desde ella", 0
msg_arr4    db 13, 10, "(con el código de arranque principal, MBR, de Hispa-DOS)", 0
msg_arr_no1 db "La unidad ", 0
msg_arr_no2 db ": no es una partición primaria de un disco duro.", 13, 10, 0

mbr_code:
        incbin "build/mbr.bin", 0, 446

        section .bss
bss_start:
ndisks      resb 1
disk        resb 1
mode        resb 1
req_part    resb 1
lba         resb 1
            resb 1
io_cmd      resb 1
newmbr      resb 1
superfl     resb 1
nocode      resb 1
letter      resb 1
pletter     resb 1
lletter     resb 1
cl_tmp      resb 1
explicit    resb 1
autosw      resb 1
bootdisk    resb 1
bootusb     resb 1
bootltr     resb 1
arr_letter  resb 1
iface       resb 1
dpte_ok     resb 1
at_dev      resb 1
subopt      resb 1
rec         resb 1
nlogs       resb 1
ext_fe      resb 1
wl_count    resb 1
wl_fe       resb 1
wl_guard    resb 1
            alignb 4
req_mb      resd 1
total       resd 1
p_start     resd 1
p_size      resd 1
ext_start   resd 1
ext_size    resd 1
ext_tail    resd 1
ext_used    resd 1
wb_base     resd 1
wl_tail     resd 1
wl_prev     resd 1
wl_end      resd 1
wl_cur      resd 1
n_ebr       resd 1
n_prev      resd 1
packet:
pk_sec      resd 1
pk_cnt      resw 1
pk_off      resw 1
pk_seg      resw 1
cyls        resw 1
spt         resw 1
heads       resw 1
slot        resw 1
ext_slot    resw 1
cd_cnt      resw 1
at_base     resw 1
at_ctl      resw 1
t_start     resw 1
chstmp      resb 4
link16      resb 16
dap         resb 16
edd         resb 0x42
edd3        resb 0x4A
optbuf      resb 20
u_numbuf    resb 16
u_lbuf      resb 84
cmdline     resb 128
dta         resb 128
            alignb 4
d_size      resd MAXDISK
log_start   resd MAXLOG
log_size    resd MAXLOG
log_ebr     resd MAXLOG
log_prev    resd MAXLOG
log_type    resb MAXLOG
d_iface     resb MAXDISK
d_model     resb MAXDISK * MODELEN
            alignb 16
mbrbuf      resb 512
iobuf       resb 512
ebrbuf      resb 512
zbuf        resb 512
bsbuf       resb 512
idbuf       resb 512
            resb 1024
stack_top:
