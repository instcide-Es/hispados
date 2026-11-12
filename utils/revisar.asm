; =============================================================================
;  Hispa-DOS REVISAR.COM (CHKDSK) - comprobar el estado de un disco FAT
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  REVISAR [unidad:][[ruta]archivo] [/F] [/V]
;     /F       corrige los errores (sin /F solo informa y no escribe nada)
;     /V       muestra la ruta completa de cada archivo y carpeta
;     archivo  (admite comodines) informa de los archivos no contiguos
;
;  Lee el sector de arranque y la FAT con INT 25h (sectores absolutos), recorre
;  el árbol de carpetas desde el raíz con una pila propia (no la del
;  procesador) y sigue la cadena de clústeres de cada archivo y carpeta.
;  Para cada clúster guarda qué archivo lo usa (un número por entrada), y así
;  encuentra los vínculos cruzados y las unidades de asignación perdidas.
;  Discos FAT12 y FAT16 con sectores de 512 bytes, con o sin particiones.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

MAXDEPTH equ 40
; una carpeta en la pila del recorrido
F_FIRST  equ 0          ; primer clúster (0 = raíz)
F_CLUS   equ 2          ; clúster que se está leyendo (FFFFh = terminado)
F_SEC    equ 4          ; sector dentro del clúster (o del raíz)
F_ENT    equ 6          ; entrada dentro del sector
F_PLEN   equ 8          ; longitud de su ruta en path
F_NUM    equ 10         ; su número de entrada
F_IDX    equ 12         ; entradas leídas
F_DOTS   equ 14         ; bit 0: «.» correcta, bit 1: «..» correcta
F_MATCH  equ 16         ; es la carpeta del patrón de archivos
F_PARENT equ 18         ; primer clúster de la carpeta madre
FRSZ     equ 20
; vínculos cruzados que se recuerdan para nombrarlos
XLMAX    equ 8
XLP      equ 100
XLSZ     equ 4 + 2*XLP
; marcas de propietario de los clústeres perdidos
OWN_LOST equ 0xFFFF     ; perdido, primero de su cadena
OWN_MID  equ 0xFFFE     ; perdido, apuntado por otro perdido
OWN_REC  equ 0xFFFD     ; recuperado en FILEnnnn.CHK
EOC      equ 0xFFF8
BADC     equ 0xFFF7

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov dword [dirbuf_lba], 0xFFFFFFFF
        mov ah, 0x19
        int 0x21
        mov [drive], al
        ; argumentos
        mov si, 0x81
.arg:
        lodsb
        cmp al, ' '
        je .arg
        cmp al, 9
        je .arg
        cmp al, 13
        je .argd
        test al, al
        jz .argd
        cmp al, '/'
        je .opt
        cmp byte [have_arg], 0
        jne bad_opt
        mov byte [have_arg], 1
        mov di, argbuf
.tk:
        call upcase
        stosb
        lodsb
        cmp al, ' '
        jbe .tke
        cmp al, '/'
        jne .tk
.tke:
        dec si
        mov byte [di], 0
        jmp .arg
.opt:
        lodsb
        call upcase
        cmp al, '?'
        je usage
        cmp al, 'F'
        jne .ov
        mov byte [opt_f], 1
        jmp .arg
.ov:
        cmp al, 'V'
        jne bad_opt
        mov byte [opt_v], 1
        jmp .arg
.argd:
        cmp byte [have_arg], 0
        je .drv
        call parse_spec
.drv:
        call check_drive
        mov ah, 0x0D                    ; que el núcleo escriba lo pendiente
        int 0x21
        call read_boot
        call alloc_mem
        ; la FAT entera (la primera copia)
        mov eax, [fat_lba]
        mov cx, [spf]
        xor bx, bx
        mov dx, [fatseg]
        call disk_rd
        jc rd_err
        call find_label
        call show_head
        ; primera pasada: comprobar
        mov byte [pass], 1
        call walk
        ; segunda pasada (solo si hay vínculos cruzados): nombrar al primero
        cmp word [xl_n], 0
        je .noxl
        mov byte [pass], 2
        call walk
        call show_xl
.noxl:
        cmp byte [have_spec], 0
        je .nsp
        cmp byte [noncontig], 0
        jne .nsp
        mov si, msg_contig
        call puts
.nsp:
        call lost
        ; escribir todas las copias de la FAT
        cmp byte [opt_f], 0
        je .nowr
        cmp byte [fat_dirty], 0
        je .nowr
        xor bp, bp
        mov eax, [fat_lba]
.wf:
        movzx cx, byte [nfats]
        cmp bp, cx
        jae .nowr
        mov cx, [spf]
        xor bx, bx
        mov dx, [fatseg]
        call disk_wr
        jc wr_err
        movzx ecx, word [spf]
        add eax, ecx
        inc bp
        jmp .wf
.nowr:
        call summary
        ; como CHKDSK de MS-DOS 6.2x: ERRORLEVEL 255 si se encontraron errores
        mov ax, 0x4C00
        cmp byte [errors], 0
        je .ex
        mov al, 255
.ex:
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
bad_opt:
        mov si, msg_badopt
        jmp fail
bad_drive:
        mov si, msg_baddrv
        jmp fail
bad_path:
        mov si, msg_badpath
        jmp fail
not_fat:
        mov si, msg_notfat
        jmp fail
no_mem:
        mov si, msg_mem
        jmp fail
too_many:
        mov si, msg_many
        jmp fail
rd_err:
        mov si, msg_rderr
        jmp fail
wr_err:
        mov si, msg_wrerr
fail:
        push cs
        pop ds
        call puts
        mov ax, 0x4C01
        int 0x21

; ---- [unidad:][ruta]archivo -> unidad, carpeta (completa) y patrón ----
parse_spec:
        mov si, argbuf
        cmp byte [si+1], ':'
        jne .split
        cmp byte [si+2], 0
        jne .split
        mov al, [si]                    ; solo la unidad
        sub al, 'A'
        jb bad_drive
        cmp al, 26
        jae bad_drive
        mov [drive], al
        ret
.split:
        ; el nombre empieza tras la última \ o :
        mov di, si
.f:
        lodsb
        test al, al
        jz .fe
        cmp al, '\'
        je .fs
        cmp al, ':'
        jne .f
.fs:
        mov di, si
        jmp .f
.fe:
        mov [name_ptr], di
        ; la carpeta: lo que hay antes del nombre
        mov si, argbuf
        mov cx, di
        sub cx, si
        mov di, dirstr
        jcxz .dot
        rep movsb
        cmp byte [di-1], ':'
        je .dot
        cmp byte [di-1], '\'
        jne .dz
        ; «\» final: se quita salvo en «\» y «X:\»
        lea ax, [dirstr+1]
        cmp di, ax
        je .dz
        cmp byte [di-2], ':'
        je .dz
        dec di
        jmp .dz
.dot:
        mov al, '.'
        stosb
.dz:
        mov byte [di], 0
        push ds
        pop es
        mov si, dirstr
        mov di, spec_dir
        mov ah, 0x60
        int 0x21
        jc bad_path
        mov al, [spec_dir]
        sub al, 'A'
        mov [drive], al
        ; sin «\» al final (salvo en el raíz)
        mov di, spec_dir
        xor al, al
        mov cx, 128
        repne scasb
        dec di
        lea ax, [spec_dir+3]
        cmp di, ax
        jbe .pat
        cmp byte [di-1], '\'
        jne .pat
        mov byte [di-1], 0
.pat:
        mov si, [name_ptr]
        cmp byte [si], 0
        je .r                           ; sin nombre: solo la unidad
        mov byte [have_spec], 1
        ; patrón al estilo de las FCB: 11 caracteres, «?» = cualquiera
        mov di, spec_pat
        mov cx, 11
        mov al, ' '
        rep stosb
        mov di, spec_pat
        mov cx, 8
        call .part
        cmp byte [si-1], '.'
        jne .r
        mov di, spec_pat + 8
        mov cx, 3
        call .part
.r:
        ret
; copia hasta «.» o el final; «*» rellena con «?»
.part:
        lodsb
        test al, al
        jz .pe
        cmp al, '.'
        je .pr
        cmp al, '*'
        je .star
        jcxz .part
        stosb
        dec cx
        jmp .part
.star:
        mov al, '?'
        rep stosb
        jmp .part
.pe:
        dec si
.pr:
        ret

; ---- la unidad tiene que ser un disco FAT local ----
check_drive:
        mov bl, [drive]
        inc bl
        mov ax, 0x4409
        int 0x21
        jc .nr
        test dh, 0x10
        jz .nr
        mov si, msg_net
        jmp fail
.nr:
        mov bl, [drive]
        inc bl
        mov dx, devparm
        mov ax, 0x440D
        mov cx, 0x0860
        int 0x21
        jnc .ok
        cmp ax, 1
        jne bad_drive
        mov si, msg_cd
        jmp fail
.ok:
        ret

; ---- sectores absolutos: EAX = sector, CX = cuántos, DX:BX = búfer ----
disk_rd:
        mov byte [io_fn], 0x25
        jmp disk_io
disk_wr:
        mov byte [io_fn], 0x26
disk_io:
        pushad
        mov [pk_lba], eax
        mov [pk_cnt], cx
        mov [pk_buf], bx
        mov [pk_buf+2], dx
        mov al, [drive]
        mov bx, packet
        mov cx, 0xFFFF
        cmp byte [io_fn], 0x26
        je .w
        int 0x25
        jmp .f
.w:
        int 0x26
.f:
        pop dx                          ; los indicadores que deja en la pila
        popad
        ret

; sector EAX del directorio -> dirbuf (si no estaba ya)
load_dir:
        cmp eax, [dirbuf_lba]
        je .ok
        push bx
        push cx
        push dx
        mov bx, dirbuf
        mov dx, ds
        mov cx, 1
        call disk_rd
        pop dx
        pop cx
        pop bx
        jc rd_err
        mov [dirbuf_lba], eax
.ok:
        ret

; dirbuf -> disco
save_dir:
        pushad
        mov eax, [dirbuf_lba]
        mov bx, dirbuf
        mov dx, ds
        mov cx, 1
        call disk_wr
        jc wr_err
        popad
        ret

; ---- sector de arranque y geometría ----
read_boot:
        xor eax, eax
        mov cx, 1
        mov bx, boot
        mov dx, ds
        call disk_rd
        jc rd_err
        cmp word [boot+0x0B], 512
        jne not_fat
        mov al, [boot+0x0D]             ; sectores por clúster: potencia de 2
        test al, al
        jz not_fat
        mov ah, al
        dec ah
        test al, ah
        jnz not_fat
        movzx eax, al
        shl eax, 9
        mov [bpc], eax
        movzx eax, word [boot+0x0E]     ; reservados
        test eax, eax
        jz not_fat
        mov [fat_lba], eax
        movzx ecx, byte [boot+0x10]     ; número de FAT
        test ecx, ecx
        jz not_fat
        cmp cl, 4
        ja not_fat
        mov [nfats], cl
        movzx edx, word [boot+0x16]     ; sectores por FAT (0 = FAT32)
        test edx, edx
        jz not_fat
        mov [spf], dx
        imul edx, ecx
        add eax, edx
        mov [root_lba], eax
        movzx edx, word [boot+0x11]
        test edx, edx
        jz not_fat
        add edx, 15
        shr edx, 4
        mov [root_secs], dx
        add eax, edx
        mov [data_lba], eax
        movzx edx, word [boot+0x13]
        test edx, edx
        jnz .t
        mov edx, [boot+0x20]
.t:
        sub edx, eax
        jbe not_fat
        mov eax, edx
        xor edx, edx
        movzx ecx, byte [boot+0x0D]
        div ecx
        test eax, eax
        jz not_fat
        cmp eax, 65524
        ja not_fat                      ; FAT32
        mov [nclus], ax
        inc ax
        mov [maxc], ax
        ; FAT12 o FAT16 según el número de clústeres
        movzx eax, ax
        inc eax                         ; entradas de la FAT
        cmp word [nclus], 4085
        jb .f12
        mov byte [fat16], 1
        shl eax, 1
        jmp .fs
.f12:
        lea eax, [eax*3+1]
        shr eax, 1
.fs:
        movzx edx, word [spf]
        shl edx, 9
        cmp eax, edx
        ja not_fat                      ; la FAT no cabe en sus sectores
        ret

; ---- memoria para la FAT y para los propietarios de los clústeres ----
alloc_mem:
        mov bx, [spf]
        cmp bx, 2047
        ja no_mem
        shl bx, 5
        mov ah, 0x48
        int 0x21
        jc no_mem
        mov [fatseg], ax
        movzx eax, word [maxc]
        inc eax
        shl eax, 1
        add eax, 15
        shr eax, 4
        mov bx, ax
        mov bp, ax
        mov ah, 0x48
        int 0x21
        jc no_mem
        mov [ownseg], ax
        ; a cero, por trozos de 32 KB
        mov dx, ax
.z:
        mov es, dx
        xor di, di
        mov cx, bp
        cmp cx, 0x0800
        jbe .z1
        mov cx, 0x0800
.z1:
        sub bp, cx
        add dx, cx
        shl cx, 3
        xor ax, ax
        rep stosw
        test bp, bp
        jnz .z
        push ds
        pop es
        ret

; ---- la FAT y los propietarios en memoria (por segmentos) ----
; EDX = desplazamiento lineal en la FAT -> ES:BX
fat_ptr:
        mov bx, dx
        and bx, 15
        shr edx, 4
        add dx, [fatseg]
        mov es, dx
        ret

; AX = clúster -> EAX = valor (en FAT12, FF0h..FFFh pasan a FFF0h..FFFFh)
get_fat:
        push ebx
        push edx
        push es
        movzx eax, ax
        cmp byte [fat16], 0
        je .f12
        lea edx, [eax*2]
        call fat_ptr
        movzx eax, word [es:bx]
        jmp .r
.f12:
        mov edx, eax
        shr edx, 1
        add edx, eax
        push eax
        call fat_ptr
        pop eax
        movzx edx, word [es:bx]
        test al, 1
        jz .ev
        shr edx, 4
.ev:
        and edx, 0xFFF
        cmp edx, 0xFF0
        jb .s
        or edx, 0xF000
.s:
        mov eax, edx
.r:
        pop es
        pop edx
        pop ebx
        ret

; AX = clúster, CX = valor nuevo
set_fat:
        push eax
        push ebx
        push ecx
        push edx
        push es
        mov byte [fat_dirty], 1
        movzx eax, ax
        cmp byte [fat16], 0
        je .f12
        lea edx, [eax*2]
        call fat_ptr
        mov [es:bx], cx
        jmp .r
.f12:
        mov edx, eax
        shr edx, 1
        add edx, eax
        push eax
        call fat_ptr
        pop eax
        and cx, 0x0FFF
        mov dx, [es:bx]
        test al, 1
        jz .ev
        shl cx, 4
        and dx, 0x000F
        jmp .st
.ev:
        and dx, 0xF000
.st:
        or dx, cx
        mov [es:bx], dx
.r:
        pop es
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; AX = clúster -> EAX = número de la entrada que lo usa (0 = ninguna)
get_own:
        push ebx
        push edx
        push es
        movzx edx, ax
        shl edx, 1
        mov bx, dx
        and bx, 15
        shr edx, 4
        add dx, [ownseg]
        mov es, dx
        movzx eax, word [es:bx]
        pop es
        pop edx
        pop ebx
        ret

; AX = clúster, CX = propietario
set_own:
        push ebx
        push edx
        push es
        movzx edx, ax
        shl edx, 1
        mov bx, dx
        and bx, 15
        shr edx, 4
        add dx, [ownseg]
        mov es, dx
        mov [es:bx], cx
        pop es
        pop edx
        pop ebx
        ret

; AX = clúster; CF si no es un clúster de datos válido
in_range:
        cmp ax, 2
        jb .no
        cmp ax, [maxc]
        ja .no
        clc
        ret
.no:
        stc
        ret

; ---- etiqueta del volumen (en el raíz) ----
find_label:
        xor cx, cx
.s:
        cmp cx, [root_secs]
        jae .r
        movzx eax, cx
        add eax, [root_lba]
        call load_dir
        mov si, dirbuf
.e:
        mov al, [si]
        test al, al
        jz .r
        cmp al, 0xE5
        je .n
        mov al, [si+11]
        cmp al, 0x0F
        je .n
        test al, 0x08
        jz .n
        mov byte [have_lab], 1
        mov di, curlab
        push cx
        mov cx, 11
        push si
        rep movsb
        pop si
        pop cx
        mov ax, [si+0x16]
        mov [lab_time], ax
        mov ax, [si+0x18]
        mov [lab_date], ax
        ret
.n:
        add si, 32
        cmp si, dirbuf + 512
        jb .e
        inc cx
        jmp .s
.r:
        ret

; «El volumen X creado el ...» y el número de serie
show_head:
        call crlf
        cmp byte [have_lab], 0
        je .ser
        mov si, msg_vol
        call puts
        mov si, curlab
        mov cx, 11
.tr:
        mov bx, cx
        cmp byte [curlab+bx-1], ' '
        jne .p
        loop .tr
        jmp .pe
.p:
        lodsb
        call putc
        loop .p
.pe:
        mov ax, [lab_date]
        test ax, ax
        jz .nd
        mov si, msg_created
        call puts
        mov al, [lab_date]
        and al, 31
        call put2
        mov al, '-'
        call putc
        mov ax, [lab_date]
        shr ax, 5
        and al, 15
        call put2
        mov al, '-'
        call putc
        mov ax, [lab_date]
        shr ax, 9
        add ax, 1980
        mov bl, 100
        div bl
        push ax
        call put2
        pop ax
        mov al, ah
        call put2
        mov al, ' '
        call putc
        mov ax, [lab_time]
        shr ax, 11
        call put2
        mov al, ':'
        call putc
        mov ax, [lab_time]
        shr ax, 5
        and al, 63
        call put2
.nd:
        call crlf
.ser:
        cmp byte [boot+0x26], 0x29
        jne .r
        mov si, msg_ser
        call puts
        mov ax, [boot+0x29]
        call puthex4
        mov al, '-'
        call putc
        mov ax, [boot+0x27]
        call puthex4
        call crlf
.r:
        call crlf
        ret

; AL (0..99) con dos cifras
put2:
        push ax
        aam
        add ax, '00'
        xchg al, ah
        call putc
        mov al, ah
        call putc
        pop ax
        ret

; ---- recorrido del árbol ----
; BX -> carpeta de arriba de la pila
top:
        mov bx, [depth]
        dec bx
        imul bx, bx, FRSZ
        add bx, frames
        ret

walk:
        mov word [num], 0
        mov word [last_key], 0xFFFF
        mov al, [drive]
        add al, 'A'
        mov [path], al
        mov word [path+1], ':\'
        mov byte [path+3], 0
        mov di, frames
        mov cx, FRSZ
        xor al, al
        rep stosb
        mov word [frames+F_PLEN], 3
        mov word [depth], 1
        mov bx, frames
        call set_match
.l:
        call next_entry
        jc .pop
        call do_entry
        jmp .l
.pop:
        call end_frame
        dec word [depth]
        jnz .l
        ret

; BX = carpeta (con su ruta en path): ¿es la del patrón?
set_match:
        push si
        push di
        mov word [bx+F_MATCH], 0
        cmp byte [have_spec], 0
        je .r
        mov si, path
        mov di, spec_dir
.c:
        mov al, [si]
        cmp al, [di]
        jne .r
        inc si
        inc di
        test al, al
        jnz .c
        mov word [bx+F_MATCH], 1
.r:
        pop di
        pop si
        ret

; siguiente entrada de la carpeta de arriba -> SI (en dirbuf), BX = carpeta;
; CF si se ha terminado
next_entry:
        call top
        cmp word [bx+F_FIRST], 0
        jne .sub
        mov ax, [bx+F_SEC]
        cmp ax, [root_secs]
        jae .end
        movzx eax, ax
        add eax, [root_lba]
        jmp .rd
.sub:
        mov ax, [bx+F_CLUS]
        cmp ax, 0xFFFF
        je .end
        movzx eax, ax
        sub eax, 2
        movzx ecx, byte [boot+0x0D]
        imul eax, ecx
        add eax, [data_lba]
        movzx ecx, word [bx+F_SEC]
        add eax, ecx
.rd:
        call load_dir
        mov si, [bx+F_ENT]
        shl si, 5
        add si, dirbuf
        cmp byte [si], 0
        je .end                         ; fin de la carpeta
        mov ax, [bx+F_IDX]
        mov [cur_idx], ax
        inc word [bx+F_IDX]
        inc word [bx+F_ENT]
        cmp word [bx+F_ENT], 16
        jb .ok
        mov word [bx+F_ENT], 0
        inc word [bx+F_SEC]
        cmp word [bx+F_FIRST], 0
        je .ok
        movzx ax, byte [boot+0x0D]
        cmp [bx+F_SEC], ax
        jb .ok
        mov word [bx+F_SEC], 0
        ; el clúster siguiente, si sigue siendo de esta carpeta
        mov ax, [bx+F_CLUS]
        call get_fat
        call in_range
        jc .last
        push eax
        call get_own
        cmp ax, [bx+F_NUM]
        pop eax
        jne .last
        mov [bx+F_CLUS], ax
        jmp .ok
.last:
        mov word [bx+F_CLUS], 0xFFFF
.ok:
        clc
        ret
.end:
        mov ax, [root_secs]
        mov [bx+F_SEC], ax
        mov word [bx+F_CLUS], 0xFFFF
        stc
        ret

; una entrada del directorio: SI = entrada, BX = su carpeta
do_entry:
        mov byte [ent_dirty], 0
        mov di, [bx+F_PLEN]
        mov byte [path+di], 0
        mov al, [si]
        cmp al, 0xE5
        je .r
        mov al, [si+11]
        cmp al, 0x0F                    ; nombre largo
        je .r
        test al, 0x08                   ; etiqueta
        jnz .r
        cmp byte [si], '.'
        jne .norm
        cmp word [bx+F_FIRST], 0
        je .r
        cmp byte [pass], 1
        jne .r
        call check_dot
        jmp .fl
.norm:
        inc word [num]
        ; los números de propietario son de 16 bits y los tres últimos son
        ; marcas: con más entradas no se puede revisar (ni tocar) el disco
        cmp word [num], OWN_REC
        jae too_many
        call append_name
        cmp byte [pass], 1
        je .p1
        ; segunda pasada: solo los nombres de los vínculos cruzados
        call xl_resolve
        test byte [si+11], 0x10
        jz .r
        movzx eax, word [si+0x1A]
        call in_range
        jc .r
        call get_own
        cmp ax, [num]
        jne .r
        call dir_valid
        jc .r
        jmp push_frame
.p1:
        test byte [si+11], 0x10
        jnz .dir
        cmp byte [opt_v], 0
        je .nv
        push si
        mov si, path
        call puts
        call crlf
        pop si
        mov ax, [num]                   ; ya está escrita su ruta
        mov [last_key], ax
.nv:
        call chk_file
        jmp .fl
.dir:
        cmp byte [opt_v], 0
        je .nv2
        push si
        mov si, msg_vdir
        call puts
        mov si, path
        call puts
        call crlf
        pop si
.nv2:
        call chk_dir
        pushf
        call flush_ent
        popf
        jc .r
        ; si su primer sector no parece el de una carpeta («.» y «..»), no se
        ; entra: serían datos de otro archivo y «corregirlos» los estropearía
        call dir_valid
        jnc push_frame
        mov dx, msg_notdir
        mov ax, [num]
        call report
        ret
.fl:
        call flush_ent
.r:
        ret

; si se ha cambiado la entrada (y hay /F), escribir su sector
flush_ent:
        cmp byte [ent_dirty], 0
        je .r
        mov byte [ent_dirty], 0
        cmp byte [opt_f], 0
        je .r
        call save_dir
.r:
        ret

; nombre de la entrada SI al final de la ruta de la carpeta BX
append_name:
        push si
        mov di, [bx+F_PLEN]
        add di, path
        cmp word [bx+F_PLEN], 3
        jbe .ns
        mov al, '\'
        stosb
.ns:
        mov dx, di
        mov cx, 8
        lodsb
        cmp al, 5
        jne .n1
        mov al, 0xE5
.n1:
        stosb
        dec cx
        rep movsb
        call .trim
        cmp word [si], '  '
        jne .ext
        cmp byte [si+2], ' '
        je .d
.ext:
        mov al, '.'
        stosb
        mov dx, di
        movsw
        movsb
        call .trim
.d:
        mov byte [di], 0
        mov [cur_end], di
        pop si
        ret
.trim:
        cmp di, dx
        jbe .tr
        cmp byte [di-1], ' '
        jne .tr
        dec di
        jmp .trim
.tr:
        ret

; ¿la carpeta de la entrada SI empieza por «.» o tiene «..» en segundo lugar?
; CF si no (no es una carpeta). Deja dirbuf como estaba.
dir_valid:
        pushad
        mov ebp, [dirbuf_lba]
        movzx eax, word [si+0x1A]
        sub eax, 2
        movzx ecx, byte [boot+0x0D]
        imul eax, ecx
        add eax, [data_lba]
        call load_dir
        mov dl, 0                       ; DL = 1 si vale
        mov si, dirbuf
        mov di, s_dot
        mov cx, 11
        repe cmpsb
        jne .d2
        test byte [dirbuf+11], 0x10
        jz .d2
        mov dl, 1
.d2:
        mov si, dirbuf + 32
        mov di, s_dotdot
        mov cx, 11
        repe cmpsb
        jne .dd
        test byte [dirbuf+32+11], 0x10
        jz .dd
        mov dl, 1
.dd:
        mov eax, ebp
        call load_dir
        cmp dl, 1                       ; CF si DL = 0
        popad
        ret

; nueva carpeta en la pila: la entrada SI (ruta en path hasta cur_end)
push_frame:
        cmp word [depth], MAXDEPTH
        jae .deep
        mov ax, [bx+F_FIRST]
        mov cx, [num]
        mov dx, [si+0x1A]
        inc word [depth]
        call top
        mov [bx+F_PARENT], ax
        mov [bx+F_FIRST], dx
        mov [bx+F_CLUS], dx
        mov [bx+F_NUM], cx
        xor ax, ax
        mov [bx+F_SEC], ax
        mov [bx+F_ENT], ax
        mov [bx+F_IDX], ax
        mov [bx+F_DOTS], ax
        mov ax, [cur_end]
        sub ax, path
        mov [bx+F_PLEN], ax
        call set_match
        ret
.deep:
        cmp byte [pass], 1
        jne .r
        mov dx, msg_deep
        mov ax, [num]
        call report
        mov byte [partial], 1
.r:
        ret

; «.» y «..»: las dos primeras entradas de una carpeta
check_dot:
        mov ax, [cur_idx]
        cmp ax, 1
        ja .r
        push si
        mov di, s_dot
        test ax, ax
        jz .d0
        mov di, s_dotdot
.d0:
        mov cx, 11
        repe cmpsb
        pop si
        jne .r
        mov cx, [bx+F_FIRST]
        mov dx, msg_dot
        test ax, ax
        jz .c
        mov cx, [bx+F_PARENT]
        mov dx, msg_dotdot
.c:
        inc ax
        or [bx+F_DOTS], ax
        cmp [si+0x1A], cx
        je .r
        mov ax, [bx+F_NUM]
        call report
        mov [si+0x1A], cx
        mov byte [ent_dirty], 1
.r:
        ret

; al terminar una carpeta: ¿faltaban «.» o «..»?
end_frame:
        cmp byte [pass], 1
        jne .r
        call top
        cmp word [bx+F_FIRST], 0
        je .r
        mov di, [bx+F_PLEN]
        mov byte [path+di], 0
        mov ax, [bx+F_NUM]
        test byte [bx+F_DOTS], 1
        jnz .d1
        mov dx, msg_dot
        call report
.d1:
        test byte [bx+F_DOTS], 2
        jnz .r
        mov dx, msg_dotdot
        call report
.r:
        ret

; ---- errores ----
; DX = mensaje, AX = número de la entrada (para escribir su ruta una vez)
report:
        pushad
        call warn
        cmp ax, [last_key]
        je .np
        mov [last_key], ax
        mov si, path
        call puts
        call crlf
.np:
        mov si, dx
        call puts
        popad
        ret

; el primer error sin /F: avisar de que no se corregirá nada
warn:
        mov byte [errors], 1
        cmp byte [opt_f], 0
        jne .r
        cmp byte [warned], 0
        jne .r
        mov byte [warned], 1
        push si
        mov si, msg_warn
        call puts
        pop si
        mov word [last_key], 0xFFFF     ; la ruta, otra vez tras el aviso
.r:
        ret

; ---- cadenas de clústeres ----
; sigue la cadena de la entrada SI (número [num]) y marca sus clústeres.
; -> fl_n clústeres, fl_blocks bloques, fl_xl si choca con otra entrada.
; Una cadena rota (clúster fuera de rango, libre, defectuoso o un bucle) se
; corta en el último clúster bueno.
follow:
        xor eax, eax
        mov [fl_n], ax
        mov [fl_blocks], ax
        mov [fl_prev], ax
        mov [fl_xl], al
        mov [fl_rep], al
        mov ax, [si+0x1A]
        test ax, ax
        jz .done
.l:
        call in_range
        jc .bad
        push eax
        call get_fat
        mov edx, eax                    ; el siguiente
        pop eax
        test dx, dx
        jz .bad                         ; libre
        cmp dx, BADC
        je .bad                         ; defectuoso
        push eax
        call get_own
        mov cx, ax
        pop eax
        test cx, cx
        jz .claim
        cmp cx, [num]
        je .bad                         ; vuelve sobre sí misma
        call xl_add
        mov byte [fl_xl], 1
        jmp .done
.claim:
        mov cx, [num]
        call set_own
        inc word [fl_n]
        mov cx, [fl_prev]
        inc cx
        cmp cx, ax
        je .cont
        inc word [fl_blocks]
.cont:
        mov [fl_prev], ax
        cmp dx, EOC
        jae .done
        movzx eax, dx
        jmp .l
.bad:
        mov dx, msg_badch
        mov ax, [num]
        call report
        mov byte [fl_rep], 1
        mov ax, [fl_prev]
        test ax, ax
        jnz .cut
        mov word [si+0x1A], 0
        mov byte [ent_dirty], 1
        jmp .done
.cut:
        mov cx, 0xFFFF
        call set_fat
.done:
        ret

; un archivo: cadena y tamaño
chk_file:
        call follow
        cmp byte [fl_xl], 0
        jne .acc
        mov eax, [si+0x1C]
        xor edx, edx
        div dword [bpc]
        test edx, edx
        jz .e
        inc eax
.e:
        movzx ecx, word [fl_n]
        cmp ecx, eax
        je .acc
        jb .short
        ; más larga que el archivo: se corta y se libera lo que sobra
        cmp byte [fl_rep], 0
        jne .t
        push eax
        mov dx, msg_badch
        mov ax, [num]
        call report
        pop eax
.t:
        call truncate
        jmp .acc
.short:
        ; más corta: el tamaño pasa a ser el de la cadena
        mov dx, msg_size
        mov ax, [num]
        call report
        movzx eax, word [fl_n]
        imul eax, [bpc]
        mov [si+0x1C], eax
        mov byte [ent_dirty], 1
.acc:
        movzx eax, word [fl_n]
        imul eax, [bpc]
        test byte [si+11], 0x02
        jz .user
        inc dword [n_hid]
        add [b_hid], eax
        jmp .ctg
.user:
        inc dword [n_usr]
        add [b_usr], eax
.ctg:
        ; ¿archivo del patrón con varios bloques?
        cmp word [bx+F_MATCH], 0
        je .r
        cmp word [fl_blocks], 1
        jbe .r
        push si
        mov di, spec_pat
        mov cx, 11
.m:
        mov al, [di]
        cmp al, '?'
        je .mn
        cmp al, [si]
        jne .nm
.mn:
        inc si
        inc di
        loop .m
        pop si
        mov byte [noncontig], 1
        push si
        mov si, path
        call puts
        mov si, msg_frag1
        call puts
        movzx eax, word [fl_blocks]
        call putnum
        mov si, msg_frag2
        call puts
.nm:
        pop si
.r:
        ret

; deja EAX clústeres en la cadena de la entrada SI y libera el resto
truncate:
        mov [fl_n], ax
        movzx edx, word [si+0x1A]
        test eax, eax
        jnz .k
        mov word [si+0x1A], 0
        mov byte [ent_dirty], 1
        mov eax, edx
        jmp .free
.k:
        mov ecx, eax
        mov eax, edx
.w:
        dec ecx
        jz .cut
        call get_fat
        jmp .w
.cut:
        push eax
        call get_fat
        mov edx, eax
        pop eax
        mov cx, 0xFFFF
        call set_fat
        mov eax, edx
.free:
        call in_range
        jc .r
        push eax
        call get_own
        cmp ax, [num]
        pop eax
        jne .r
        push eax
        call get_fat
        mov edx, eax
        pop eax
        xor cx, cx
        call set_fat
        call set_own
        mov eax, edx
        jmp .free
.r:
        ret

; una carpeta: CF si no se puede entrar en ella
chk_dir:
        call follow
        cmp word [fl_n], 0
        jne .ok
        cmp byte [fl_xl], 0
        jne .no
        cmp byte [fl_rep], 0
        jne .cv
        mov dx, msg_badch
        mov ax, [num]
        call report
.cv:
        ; sin ningún clúster válido: se convierte en un archivo vacío. Dejarla
        ; como carpeta con el clúster 0 sería un segundo nombre del raíz (y
        ; borrarla después con DELTREE borraría el raíz entero)
        mov dx, msg_dir2f
        mov ax, [num]
        call report
        and byte [si+11], 0xEF
        mov word [si+0x1A], 0
        mov dword [si+0x1C], 0
        mov byte [ent_dirty], 1
        inc dword [n_usr]
.no:
        stc
        ret
.ok:
        movzx eax, word [fl_n]
        imul eax, [bpc]
        add [b_dir], eax
        inc dword [n_dir]
        clc
        ret

; ---- vínculos cruzados ----
; AX = clúster, CX = la entrada que ya lo tenía; la ruta actual es la otra
xl_add:
        pushad
        call warn
        mov bx, [xl_n]
        cmp bx, XLMAX
        jae .r
        inc word [xl_n]
        imul bx, bx, XLSZ
        add bx, xl_tab
        mov [bx], ax
        mov [bx+2], cx
        mov byte [bx+4], 0
        lea di, [bx+4+XLP]
        mov si, path
        call copy_path
.r:
        popad
        ret

; SI -> DI, hasta XLP-1 caracteres
copy_path:
        mov cx, XLP-1
.c:
        lodsb
        stosb
        test al, al
        jz .r
        loop .c
        mov byte [di], 0
.r:
        ret

; segunda pasada: ¿es esta entrada la primera de algún vínculo cruzado?
xl_resolve:
        pushad
        mov bx, xl_tab
        mov dx, [xl_n]
.l:
        test dx, dx
        jz .r
        mov ax, [bx+2]
        cmp ax, [num]
        jne .n
        cmp byte [bx+4], 0
        jne .n
        lea di, [bx+4]
        mov si, path
        call copy_path
.n:
        add bx, XLSZ
        dec dx
        jmp .l
.r:
        popad
        ret

show_xl:
        mov bx, xl_tab
        mov dx, [xl_n]
.l:
        test dx, dx
        jz .r
        lea si, [bx+4]
        cmp byte [si], 0
        jne .p1
        mov si, s_unknown
.p1:
        call puts
        mov si, msg_xl1
        call puts
        lea si, [bx+4+XLP]
        call puts
        mov si, msg_xl2
        call puts
        movzx eax, word [bx]
        call putnum
        mov al, '.'
        call putc
        call crlf
        add bx, XLSZ
        dec dx
        jmp .l
.r:
        ret

; ---- unidades de asignación perdidas ----
lost:
        ; ocupadas en la FAT y sin dueño
        mov eax, 2
.m:
        cmp ax, [maxc]
        ja .m2
        push eax
        call get_fat
        mov edx, eax
        pop eax
        test dx, dx
        jz .mn
        cmp dx, BADC
        je .mn
        push eax
        call get_own
        test ax, ax
        pop eax
        jnz .mn
        mov cx, OWN_LOST
        call set_own
        inc dword [nlost]
.mn:
        inc eax
        jmp .m
.m2:
        cmp dword [nlost], 0
        je .ret
        ; si alguna carpeta no se ha recorrido (demasiados niveles), sus
        ; archivos parecen perdidos sin serlo: ni se convierten ni se liberan
        cmp byte [partial], 0
        je .full_walk
        mov si, msg_partial
        call puts
        jmp .ret
.full_walk:
        call warn
        ; las apuntadas por otra perdida no empiezan cadena
        mov eax, 2
.h:
        cmp ax, [maxc]
        ja .h2
        push eax
        call get_own
        cmp ax, OWN_MID
        pop eax
        jb .hn
        push eax
        call get_fat
        call in_range
        jc .hp
        push eax
        call get_own
        cmp ax, OWN_MID
        pop eax
        jb .hp
        mov cx, OWN_MID
        call set_own
.hp:
        pop eax
.hn:
        inc eax
        jmp .h
.h2:
        ; cadenas = primeras de cadena
        mov eax, 2
.c:
        cmp ax, [maxc]
        ja .c2
        push eax
        call get_own
        cmp ax, OWN_LOST
        pop eax
        jne .cn
        inc dword [nchains]
.cn:
        inc eax
        jmp .c
.c2:
        mov si, msg_lost1
        call puts
        mov eax, [nlost]
        call putnum
        mov si, msg_lost2
        call puts
        mov eax, [nchains]
        call putnum
        mov si, msg_lost3
        call puts
        mov si, msg_conv
        call puts
        mov eax, [nlost]
        imul eax, [bpc]
        mov [b_lost], eax
        call ask_yes
        jnc .yes
        ; no: se liberan
        cmp byte [opt_f], 0
        je .nof
        call free_lost
        mov eax, [b_lost]
        call putnum
        mov si, msg_freed
        call puts
        jmp .ret
.nof:
        mov eax, [b_lost]
        call putnum
        mov si, msg_wfreed
        call puts
        jmp .ret
.yes:
        cmp byte [opt_f], 0
        jne .conv
        mov si, msg_wrec1
        call puts
        mov eax, [b_lost]
        call putnum
        mov si, msg_wrec2
        call puts
        mov eax, [nchains]
        call putnum
        mov si, msg_wrec3
        call puts
        jmp .ret
.conv:
        mov eax, 2
.v:
        cmp ax, [maxc]
        ja .v2
        push eax
        call get_own
        cmp ax, OWN_LOST
        pop eax
        jne .vn
        call make_chk
        jc .v3                          ; raíz lleno: el resto se deja como está
.vn:
        inc eax
        jmp .v
.v2:
        call free_lost                  ; restos (bucles sin principio)
.v3:
        mov eax, [b_rec]
        call putnum
        mov si, msg_rec1
        call puts
        mov eax, [n_rec]
        call putnum
        mov si, msg_rec2
        call puts
.ret:
        ret

; liberar los clústeres perdidos que queden
free_lost:
        pushad
        mov eax, 2
.l:
        cmp ax, [maxc]
        ja .r
        push eax
        call get_own
        cmp ax, OWN_MID
        pop eax
        jb .n
        xor cx, cx
        call set_fat
        call set_own
.n:
        inc eax
        jmp .l
.r:
        popad
        ret

; AX = primer clúster de una cadena perdida -> FILEnnnn.CHK en el raíz
make_chk:
        pushad
.nm:
        mov ax, [chk_next]
        cmp ax, 10000
        jae .full
        mov di, chkname + 7
        mov bx, 10
        mov cx, 4
.dg:
        xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .dg
        mov di, chkname
        call root_has
        jnc .free
        inc word [chk_next]
        jmp .nm
.free:
        call root_free
        jc .full
        popad
        pushad
        ; la cadena: hasta el final o hasta algo que no sea perdido
        xor ebp, ebp
        mov [chk_first], ax
.f:
        mov cx, OWN_REC
        call set_own
        inc ebp
        push eax
        call get_fat
        mov edx, eax
        pop eax
        cmp dx, EOC
        jae .end
        push eax
        mov ax, dx
        call in_range
        jc .fix
        call get_own
        cmp ax, OWN_MID
        jne .fix
        pop eax
        movzx eax, dx
        jmp .f
.fix:
        pop eax
        mov cx, 0xFFFF
        call set_fat
.end:
        ; la entrada, escrita a mano
        mov di, [chk_slot]
        push di
        mov si, chkname
        mov cx, 11
        rep movsb
        mov al, 0x20
        stosb
        mov cx, 10
        xor al, al
        rep stosb
        call now
        pop di
        mov ax, [dir_time]
        mov [di+0x16], ax
        mov ax, [dir_date]
        mov [di+0x18], ax
        mov ax, [chk_first]
        mov [di+0x1A], ax
        mov eax, ebp
        imul eax, [bpc]
        mov [di+0x1C], eax
        add [b_rec], eax
        add [b_usr], eax
        inc dword [n_rec]
        inc dword [n_usr]
        call save_dir
        inc word [chk_next]
        popad
        clc
        ret
.full:
        mov si, msg_full
        call puts
        popad
        stc
        ret

; DI = nombre (11): CF si ya está en el raíz
root_has:
        xor cx, cx
.s:
        cmp cx, [root_secs]
        jae .no
        movzx eax, cx
        add eax, [root_lba]
        call load_dir
        mov si, dirbuf
.e:
        cmp byte [si], 0
        je .no
        cmp byte [si], 0xE5
        je .n
        push si
        push di
        push cx
        mov cx, 11
        repe cmpsb
        pop cx
        pop di
        pop si
        je .yes
.n:
        add si, 32
        cmp si, dirbuf + 512
        jb .e
        inc cx
        jmp .s
.no:
        clc
        ret
.yes:
        stc
        ret

; un hueco en el raíz -> chk_slot (en dirbuf, que queda con su sector); CF si no hay
root_free:
        xor cx, cx
.s:
        cmp cx, [root_secs]
        jae .no
        movzx eax, cx
        add eax, [root_lba]
        call load_dir
        mov si, dirbuf
.e:
        cmp byte [si], 0
        je .yes
        cmp byte [si], 0xE5
        je .yes
        add si, 32
        cmp si, dirbuf + 512
        jb .e
        inc cx
        jmp .s
.no:
        stc
        ret
.yes:
        mov [chk_slot], si
        clc
        ret

; hora y fecha de DOS -> dir_time, dir_date
now:
        pushad
        mov ah, 0x2C
        int 0x21
        movzx ax, ch
        shl ax, 6
        or al, cl
        shl ax, 5
        shr dh, 1
        or al, dh
        mov [dir_time], ax
        mov ah, 0x2A
        int 0x21
        sub cx, 1980
        mov ax, cx
        shl ax, 4
        or al, dh
        shl ax, 5
        or al, dl
        mov [dir_date], ax
        popad
        ret

; ---- informe final ----
; EAX con 13 columnas
num13:
        push cx
        mov cl, 13
        call putnum_w
        pop cx
        ret

summary:
        ; libres y defectuosas
        xor esi, esi
        xor edi, edi
        mov eax, 2
.c:
        cmp ax, [maxc]
        ja .c2
        push eax
        call get_fat
        test ax, ax
        jnz .nf
        inc esi
.nf:
        cmp ax, BADC
        jne .nb
        inc edi
.nb:
        pop eax
        inc eax
        jmp .c
.c2:
        mov [n_free], esi
        mov [n_bad], edi
        call crlf
        movzx eax, word [nclus]
        imul eax, [bpc]
        call num13
        mov si, msg_total
        call puts
        cmp dword [n_hid], 0
        je .nh
        mov eax, [b_hid]
        call num13
        mov si, msg_en
        call puts
        mov eax, [n_hid]
        call putnum
        mov si, msg_hid
        call puts
.nh:
        cmp dword [n_dir], 0
        je .nd
        mov eax, [b_dir]
        call num13
        mov si, msg_en
        call puts
        mov eax, [n_dir]
        call putnum
        mov si, msg_dirs
        call puts
.nd:
        mov eax, [b_usr]
        call num13
        mov si, msg_en
        call puts
        mov eax, [n_usr]
        call putnum
        mov si, msg_usr
        call puts
        cmp dword [n_bad], 0
        je .nbd
        mov eax, [n_bad]
        imul eax, [bpc]
        call num13
        mov si, msg_bad
        call puts
.nbd:
        mov eax, [n_free]
        imul eax, [bpc]
        call num13
        mov si, msg_avail
        call puts
        call crlf
        mov eax, [bpc]
        call num13
        mov si, msg_bpc
        call puts
        movzx eax, word [nclus]
        call num13
        mov si, msg_units
        call puts
        mov eax, [n_free]
        call num13
        mov si, msg_ufree
        call puts
        call crlf
        ; memoria: la FAT y los propietarios ya no hacen falta
        mov es, [fatseg]
        mov ah, 0x49
        int 0x21
        mov es, [ownseg]
        mov ah, 0x49
        int 0x21
        push ds
        pop es
        int 0x12
        movzx eax, ax
        shl eax, 10
        call num13
        mov si, msg_mem1
        call puts
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        movzx eax, bx
        shl eax, 4
        call num13
        mov si, msg_mem2
        call puts
        ret

%include "util.inc"

s_dot       db ".          "
s_dotdot    db "..         "
s_unknown   db "?", 0
msg_usage   db "Comprueba un disco y muestra un informe de su estado.", 13, 10, 13, 10
            db "REVISAR [unidad:][[ruta]archivo] [/F] [/V]", 13, 10, 13, 10
            db "  [unidad:][ruta]  Unidad (y carpeta) que se va a comprobar.", 13, 10
            db "  archivo          Archivos cuya fragmentación se comprueba", 13, 10
            db "                   (admite comodines).", 13, 10
            db "  /F               Corrige los errores del disco.", 13, 10
            db "  /V               Muestra la ruta de cada archivo y carpeta.", 13, 10, 0
msg_badopt  db "Parámetro no válido", 13, 10, 0
msg_baddrv  db "Especificación de unidad no válida", 13, 10, 0
msg_badpath db "Ruta no válida", 13, 10, 0
msg_net     db "REVISAR no se puede usar en una unidad de red", 13, 10, 0
msg_cd      db "REVISAR no se puede usar en una unidad de CD/DVD", 13, 10, 0
msg_notfat  db "El disco no tiene un sistema de archivos FAT12 o FAT16 válido", 13, 10, 0
msg_mem     db "No hay memoria suficiente", 13, 10, 0
msg_rderr   db "No se puede leer el disco", 13, 10, 0
msg_wrerr   db "No se puede escribir en el disco", 13, 10, 0
msg_vol     db "El volumen ", 0
msg_created db " se creó el ", 0
msg_ser     db "El número de serie del volumen es ", 0
msg_warn    db "Se encontraron errores y no se ha indicado /F.", 13, 10
            db "No se escribirán las correcciones en el disco.", 13, 10, 13, 10, 0
msg_vdir    db "Directorio ", 0
msg_badch   db "   La cadena de asignación no es válida.", 13, 10, 0
msg_size    db "   El tamaño se ha ajustado.", 13, 10, 0
msg_dot     db "   La entrada «.» del directorio no es válida.", 13, 10, 0
msg_dotdot  db "   La entrada «..» del directorio no es válida.", 13, 10, 0
msg_deep    db "   Hay demasiados niveles de directorios: no se revisa su contenido.", 13, 10, 0
msg_xl1     db " y ", 0
msg_xl2     db " tienen vínculos cruzados en la unidad de asignación ", 0
msg_frag1   db " contiene ", 0
msg_frag2   db " bloques no contiguos", 13, 10, 0
msg_contig  db "Todos los archivos especificados son contiguos", 13, 10, 0
msg_lost1   db 13, 10, "Se encontraron ", 0
msg_lost2   db " unidades de asignación perdidas en ", 0
msg_lost3   db " cadenas.", 13, 10, 0
msg_conv    db "¿Convertir las cadenas perdidas en archivos (S/N)? ", 0
msg_freed   db " bytes de espacio en disco liberados.", 13, 10, 0
msg_wfreed  db " bytes de espacio en disco se liberarían.", 13, 10, 0
msg_wrec1   db "Se recuperarían ", 0
msg_wrec2   db " bytes en ", 0
msg_wrec3   db " archivos.", 13, 10, 0
msg_rec1    db " bytes recuperados en ", 0
msg_rec2    db " archivos.", 13, 10, 0
msg_full    db "No queda sitio en el directorio raíz: las cadenas que faltan", 13, 10
            db "se dejan como están. Mueva archivos del raíz y repita REVISAR /F.", 13, 10, 0
msg_many    db "Hay demasiados archivos y carpetas para revisar este disco", 13, 10, 0
msg_notdir  db "   No es un directorio válido: no se revisa su contenido.", 13, 10, 0
msg_dir2f   db "   Directorio no válido: convertido en un archivo vacío.", 13, 10, 0
msg_partial db 13, 10, "Hay carpetas que no se han podido revisar: las unidades de asignación", 13, 10
            db "que parecen perdidas no se tocan.", 13, 10, 0
msg_total   db " bytes de espacio total en disco", 13, 10, 0
msg_en      db " bytes en ", 0
msg_hid     db " archivos ocultos", 13, 10, 0
msg_dirs    db " directorios", 13, 10, 0
msg_usr     db " archivos de usuario", 13, 10, 0
msg_bad     db " bytes en sectores defectuosos", 13, 10, 0
msg_avail   db " bytes disponibles en el disco", 13, 10, 0
msg_bpc     db " bytes en cada unidad de asignación", 13, 10, 0
msg_units   db " unidades de asignación en total en el disco", 13, 10, 0
msg_ufree   db " unidades de asignación disponibles en el disco", 13, 10, 0
msg_mem1    db " bytes de memoria total", 13, 10, 0
msg_mem2    db " bytes libres", 13, 10, 0
chkname     db "FILE0000CHK"

        section .bss
            alignb 4
bss_start:
packet:
pk_lba      resd 1
pk_cnt      resw 1
pk_buf      resd 1
io_fn       resb 1
drive       resb 1
opt_f       resb 1
opt_v       resb 1
have_arg    resb 1
have_spec   resb 1
have_lab    resb 1
pass        resb 1
fat16       resb 1
nfats       resb 1
fat_dirty   resb 1
ent_dirty   resb 1
errors      resb 1
warned      resb 1
noncontig   resb 1
fl_xl       resb 1
fl_rep      resb 1
partial     resb 1
            alignb 4
bpc         resd 1
fat_lba     resd 1
root_lba    resd 1
data_lba    resd 1
dirbuf_lba  resd 1
n_hid       resd 1
b_hid       resd 1
n_dir       resd 1
b_dir       resd 1
n_usr       resd 1
b_usr       resd 1
n_free      resd 1
n_bad       resd 1
nlost       resd 1
nchains     resd 1
b_lost      resd 1
n_rec       resd 1
b_rec       resd 1
spf         resw 1
root_secs   resw 1
nclus       resw 1
maxc        resw 1
fatseg      resw 1
ownseg      resw 1
num         resw 1
last_key    resw 1
depth       resw 1
cur_idx     resw 1
cur_end     resw 1
name_ptr    resw 1
fl_n        resw 1
fl_blocks   resw 1
fl_prev     resw 1
xl_n        resw 1
chk_next    resw 1
chk_first   resw 1
chk_slot    resw 1
lab_time    resw 1
lab_date    resw 1
dir_time    resw 1
dir_date    resw 1
curlab      resb 12
spec_pat    resb 12
devparm     resb 64
argbuf      resb 128
dirstr      resb 132
spec_dir    resb 132
path        resb 600
frames      resb FRSZ*MAXDEPTH
xl_tab      resb XLSZ*XLMAX
u_numbuf    resb 16
u_lbuf      resb 84
boot        resb 512
dirbuf      resb 512
            resb 2048
stack_top:
prog_end:
