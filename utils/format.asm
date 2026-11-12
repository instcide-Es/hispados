; =============================================================================
;  Hispa-DOS FORMAT.COM - dar formato FAT12/FAT16 a una unidad
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  FORMAT unidad: [/S] [/V:etiqueta] [/F:tamaño] [/Q] [/U] [/AUTOTEST]
;     /S         copiar el sistema (KERNEL.SYS y COMMAND.COM): disco de arranque
;     /V:nombre  etiqueta de volumen (11 caracteres como máximo)
;     /F:tamaño  capacidad del disquete: 360, 720, 1200 (1.2), 1440 (1.44)
;                o 2880 (2.88); hace falta para un disquete nuevo de menos
;                capacidad que la unidad
;     /Q /U      aceptadas por compatibilidad (el formato siempre es rápido)
;     /AUTOTEST  no pedir confirmación (para guiones y pruebas)
;
;  Disquetes: FAT12 con la geometría estándar. Discos duros: FAT16.
;  Escribe con INT 26h y avisa al núcleo con INT 21h AH=0Dh.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

ZCHUNK      equ 16                  ; sectores por escritura de ceros

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, (stack_top - bss_start) / 2
        xor ax, ax
        rep stosw
        mov bx, 0x1000              ; dejarnos 64 KB y liberar el resto
        mov ah, 0x4A
        int 0x21
        mov si, msg_title
        call puts
        call parse_args
        jc usage
        ; parámetros de la unidad
        mov ax, 0x440D
        mov bl, [drv]
        inc bl
        mov cx, 0x0860
        mov dx, params
        int 0x21
        jc bad_drive
        mov al, [params+2]
        and al, 1
        mov [fixed], al
        movzx eax, word [params+7+8]
        test eax, eax
        jnz .t
        mov eax, [params+7+21]
.t:
        mov [tot], eax
        mov eax, [params+7+17]
        mov [hidden], eax
        mov ax, [params+7+13]
        mov [spt], ax
        mov ax, [params+7+15]
        mov [heads], ax
        ; en un disco duro, el tamaño que cuenta es el de la partición, no el
        ; que diga el sector de arranque que haya ahora (puede ser de otra
        ; partición mayor que estuvo en el mismo sitio)
        cmp byte [fixed], 0
        je .lay
        call real_size
.lay:
        call layout
        jc fail
        ; leer el sistema antes de escribir nada
        cmp byte [opt_s], 0
        je .nosys
        call load_system
        jc fail
.nosys:
        call confirm
        jc abort
        mov si, msg_formatting
        call puts
        mov eax, [tot]
        imul eax, eax, 100
        shr eax, 11
        xor edx, edx
        mov ecx, 100
        div ecx
        push edx
        call putnum
        mov al, '.'
        call putc
        pop eax
        cmp eax, 10
        jae .two
        push ax
        mov al, '0'
        call putc
        pop ax
.two:
        call putnum
        mov al, 'M'
        call putc
        call crlf
        call make_serial
        call write_boot
        jc ioerr
        ; disquete: que el núcleo tome ya la geometría nueva (sectores por
        ; pista) del sector de arranque; si no, con un disquete de menos
        ; capacidad que la unidad el resto se escribiría en otro sitio
        cmp byte [fixed], 0
        jne .zs
        mov ah, 0x0D
        int 0x21
        mov ax, 0x440D
        mov bl, [drv]
        inc bl
        mov cx, 0x0860
        mov dx, params
        int 0x21
.zs:
        call zero_system_area
        jc ioerr
        call init_tables
        cmp byte [opt_s], 0
        je .nos2
        call install_system
        jc ioerr
.nos2:
        call write_tables
        jc ioerr
        mov ah, 0x0D                ; el núcleo vuelve a montar la unidad
        int 0x21
        mov si, msg_done
        call puts
        cmp byte [opt_s], 0
        je .nos3
        mov si, msg_systr
        call puts
.nos3:
        call report
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
bad_drive:
        mov si, msg_baddrv
        call puts
        mov ax, 0x4C01
        int 0x21
ioerr:
        mov si, msg_ioerr
        call puts
fail:
        mov ax, 0x4C04
        int 0x21
abort:
        mov ax, 0x4C05
        int 0x21

; -----------------------------------------------------------------------------
parse_args:
        mov byte [drv], 0xFF
        mov si, 0x81
.l:
        lodsb
        cmp al, 13
        je .end
        cmp al, ' '
        je .l
        cmp al, 9
        je .l
        cmp al, '/'
        je .opt
        call upcase
        cmp byte [si], ':'
        jne .bad
        sub al, 'A'
        jb .bad
        cmp al, 26
        jae .bad
        mov [drv], al
        inc si
        jmp .l
.opt:
        lodsb
        call upcase
        cmp al, 'S'
        je .s
        cmp al, 'Q'
        je .skip
        cmp al, 'U'
        je .skip
        cmp al, 'A'
        je .auto
        cmp al, 'V'
        je .v
        cmp al, 'F'
        je .f
        jmp .bad
.f:
        ; /F:tamaño en KB (360, 720, 1200, 1440, 2880) o en MB (1.2, 1.44, 2.88)
        cmp byte [si], ':'
        jne .bad
        inc si
        call parse_num
        jc .bad
        cmp byte [si], '.'
        jne .fk
        cmp byte [si-1], '0'
        jb .bad
        inc si
        mov bx, ax                  ; la parte entera
        call parse_num
        jc .bad
        imul bx, bx, 100
        cmp ax, 10                  ; «.2» son 20 centésimas
        jae .fc
        imul ax, ax, 10
.fc:
        add ax, bx                  ; centésimas de MB: 120, 144, 288
        mov bx, 1200
        cmp ax, 120
        je .fs
        mov bx, 1440
        cmp ax, 144
        je .fs
        mov bx, 2880
        cmp ax, 288
        jne .bad
.fs:
        mov ax, bx
.fk:
        mov di, fsz_tab
.ft:
        cmp word [di], 0
        je .bad
        cmp ax, [di]
        je .fok
        add di, 2
        jmp .ft
.fok:
        shl ax, 1                   ; KB -> sectores
        mov [fsz], ax
        jmp .skip                   ; (salta «K», «KB», «M», «MB»)
.s:
        mov byte [opt_s], 1
        jmp .skip
.auto:
        mov byte [opt_auto], 1
        jmp .skip
.v:
        cmp byte [si], ':'
        jne .bad
        inc si
        mov di, label
        mov cx, 11
        mov byte [opt_v], 1
.vl:
        mov al, [si]
        cmp al, 13
        je .vpad
        cmp al, ' '
        je .vpad
        cmp al, '/'
        je .vpad
        inc si
        jcxz .vl
        call upcase
        mov [di], al
        inc di
        dec cx
        jmp .vl
.vpad:
        jcxz .vend
        mov byte [di], ' '
        inc di
        loop .vpad
.vend:
        jmp .l
.skip:
        mov al, [si]
        cmp al, 13
        je .l
        cmp al, ' '
        je .l
        cmp al, '/'
        je .l
        inc si
        jmp .skip
.end:
        cmp byte [drv], 0xFF
        je .bad
        clc
        ret
.bad:
        stc
        ret

; -----------------------------------------------------------------------------
;  Cálculo del formato
; -----------------------------------------------------------------------------
layout:
        mov word [rsv], 1
        mov byte [nfats], 2
        cmp byte [fixed], 0
        jne .hd
        ; /F:tamaño: vale si cabe en la unidad (cilindros x cabezas x sectores)
        cmp word [fsz], 0
        je .nfsz
        movzx eax, word [params+4]
        movzx ecx, word [spt]
        imul eax, ecx
        movzx ecx, word [heads]
        imul eax, ecx
        movzx ecx, word [fsz]
        cmp ecx, eax
        ja .badfl
        mov [tot], ecx
.nfsz:
        ; disquete: buscar el formato estándar
        mov si, floppy_tab
.f:
        mov ax, [si]
        test ax, ax
        jz .badfl
        movzx eax, ax
        cmp eax, [tot]
        je .ff
        add si, 10
        jmp .f
.ff:
        movzx ax, byte [si+2]
        mov [spc], ax
        mov ax, [si+3]
        mov [rootent], ax
        mov ax, [si+5]
        mov [spf], ax
        mov al, [si+7]
        mov [media], al
        mov ax, [si+8]
        mov [spt], ax
        mov word [heads], 2
        mov byte [fat16], 0
        jmp .common
.badfl:
        mov si, msg_badfl
        call puts
        stc
        ret
.hd:
        mov byte [media], 0xF8
        mov word [rootent], 512
        mov byte [fat16], 1
        ; clúster más pequeño que deje como mucho 65524 clústeres
        mov word [spc], 1
.sp:
        mov eax, [tot]
        xor edx, edx
        movzx ecx, word [spc]
        div ecx
        cmp eax, 65524
        jbe .spok
        shl word [spc], 1
        cmp word [spc], 64
        jbe .sp
        mov si, msg_toobig
        call puts
        stc
        ret
.spok:
        ; sectores por FAT (cota superior)
        add eax, 2
        shl eax, 1
        add eax, 511
        shr eax, 9
        mov [spf], ax
.common:
        movzx eax, word [rootent]
        add eax, 15
        shr eax, 4
        mov [rootsecs], ax
        movzx eax, word [spf]
        movzx ecx, byte [nfats]
        imul eax, ecx
        movzx ecx, word [rsv]
        add eax, ecx
        mov [rootlba], eax
        movzx ecx, word [rootsecs]
        add eax, ecx
        mov [datalba], eax
        mov edx, [tot]
        sub edx, eax
        jbe .small
        mov eax, edx
        xor edx, edx
        movzx ecx, word [spc]
        div ecx
        mov [nclus], eax
        cmp byte [fat16], 0
        je .ok
        cmp eax, 4085
        jb .small
.ok:
        movzx eax, word [spc]
        shl eax, 9
        mov [bpc], eax
        clc
        ret
.small:
        mov si, msg_small
        call puts
        stc
        ret

; tamaños que admite /F (en KB)
fsz_tab     dw 360, 720, 1200, 1440, 2880, 0
; sectores, sec/clúster, entradas raíz, sec/FAT, medio, sec/pista
floppy_tab:
        dw 2880
        db 1
        dw 224, 9
        db 0xF0
        dw 18
        dw 1440
        db 2
        dw 112, 3
        db 0xF9
        dw 9
        dw 2400
        db 1
        dw 224, 7
        db 0xF9
        dw 15
        dw 5760
        db 2
        dw 240, 9
        db 0xF0
        dw 36
        dw 720
        db 2
        dw 112, 2
        db 0xFD
        dw 9
        dw 0

; -----------------------------------------------------------------------------
confirm:
        cmp byte [opt_auto], 0
        jne .yes
        cmp byte [fixed], 0
        je .floppy
        mov si, msg_warn1
        call puts
        mov al, [drv]
        add al, 'A'
        call putc
        mov si, msg_warn2
        call puts
        jmp ask_yes
.floppy:
        mov si, msg_insert
        call puts
        mov al, [drv]
        add al, 'A'
        call putc
        mov si, msg_insert2
        call puts
        call readline
.yes:
        clc
        ret

make_serial:
        mov ah, 0x2A
        int 0x21
        push cx
        push dx
        mov ah, 0x2C
        int 0x21
        pop ax
        add ax, dx
        mov [serial], ax
        pop ax
        add ax, cx
        mov [serial+2], ax
        ret

; -----------------------------------------------------------------------------
;  Escritura en el disco (INT 26h, paquete de 32 bits)
; -----------------------------------------------------------------------------
; EAX = sector, CX = número, DX:BX = búfer -> CF
abs_write:
        pushad
        mov [pk_sec], eax
        mov [pk_cnt], cx
        mov [pk_off], bx
        mov [pk_seg], dx
        mov al, [drv]
        mov cx, 0xFFFF
        mov bx, packet
        int 0x26
        pop word [junk]             ; INT 26h deja los indicadores en la pila
        popad
        ret

; EAX = sector -> CF si no se puede leer (búfer: secb)
abs_read:
        pushad
        mov [pk_sec], eax
        mov word [pk_cnt], 1
        mov word [pk_off], secb
        mov [pk_seg], ds
        mov al, [drv]
        mov cx, 0xFFFF
        mov bx, packet
        int 0x25
        pop word [junk]
        popad
        ret

; tamaño real de la unidad -> [tot]. INT 25h no deja leer más allá del final
; de la partición: se busca el primer sector que no se puede leer, entre el
; último cilindro entero que dice el núcleo y el siguiente.
real_size:
        xor esi, esi                    ; ESI = seguro que existe hasta aquí
        mov edi, 0xFFFFFFFF             ; EDI = seguro que no existe
        movzx eax, word [params+4]      ; cilindros
        test eax, eax
        jz .l
        cmp eax, 0xFFFF
        je .l
        movzx ecx, word [spt]
        movzx edx, word [heads]
        imul ecx, edx
        jecxz .l
        imul eax, ecx
        mov esi, eax
        add eax, ecx
        mov edi, eax
.l:
        cmp esi, edi
        jae .d
        mov eax, edi
        sub eax, esi
        shr eax, 1
        add eax, esi                    ; el del medio
        call abs_read
        jc .no
        lea esi, [eax+1]
        jmp .l
.no:
        mov edi, eax
        jmp .l
.d:
        mov [tot], esi
        ret

write_boot:
        mov si, boot12
        cmp byte [fat16], 0
        je .c
        mov si, boot16
.c:
        mov di, secb
        mov cx, 512
        rep movsb
        mov word [secb+0x0B], 512
        mov al, [spc]
        mov [secb+0x0D], al
        mov ax, [rsv]
        mov [secb+0x0E], ax
        mov al, [nfats]
        mov [secb+0x10], al
        mov ax, [rootent]
        mov [secb+0x11], ax
        mov word [secb+0x13], 0
        mov dword [secb+0x20], 0
        mov eax, [tot]
        cmp eax, 0xFFFF
        ja .t32
        mov [secb+0x13], ax
        jmp .t
.t32:
        mov [secb+0x20], eax
.t:
        mov al, [media]
        mov [secb+0x15], al
        mov ax, [spf]
        mov [secb+0x16], ax
        mov ax, [spt]
        mov [secb+0x18], ax
        mov ax, [heads]
        mov [secb+0x1A], ax
        mov eax, [hidden]
        mov [secb+0x1C], eax
        mov al, 0
        cmp byte [fixed], 0
        je .dn
        mov al, 0x80
.dn:
        mov [secb+0x24], al
        mov byte [secb+0x26], 0x29
        mov eax, [serial]
        mov [secb+0x27], eax
        mov si, label
        cmp byte [opt_v], 0
        jne .lb
        mov si, noname
.lb:
        mov di, secb+0x2B
        mov cx, 11
        rep movsb
        mov si, fs12
        cmp byte [fat16], 0
        je .fs
        mov si, fs16
.fs:
        mov di, secb+0x36
        mov cx, 8
        rep movsb
        xor eax, eax
        mov cx, 1
        mov dx, ds
        mov bx, secb
        jmp abs_write

; poner a cero las FAT y el directorio raíz
zero_system_area:
        movzx eax, word [rsv]
        mov ecx, [datalba]
        sub ecx, eax
.l:
        jecxz .ok
        mov edx, ecx
        cmp edx, ZCHUNK
        jbe .n
        mov edx, ZCHUNK
.n:
        push ecx
        push edx
        mov cx, dx
        mov dx, ds
        mov bx, zbuf
        call abs_write
        pop edx
        pop ecx
        jc .r
        add eax, edx
        sub ecx, edx
        jmp .l
.ok:
        clc
.r:
        ret

; primer sector de la FAT y del directorio raíz en memoria
init_tables:
        mov di, fatb
        mov cx, 256
        xor ax, ax
        rep stosw
        mov di, rootb
        mov cx, 256
        rep stosw
        mov al, [media]
        mov [fatb], al
        mov word [fatb+1], 0xFFFF
        cmp byte [fat16], 0
        je .f12
        mov byte [fatb+3], 0xFF
.f12:
        mov word [next_clus], 2
        mov word [dirpos], rootb
        mov dword [used], 0
        cmp byte [opt_v], 0
        je .r
        ; etiqueta de volumen en el directorio raíz
        mov di, [dirpos]
        mov si, label
        mov cx, 11
        rep movsb
        mov di, [dirpos]
        mov byte [di+11], 0x08
        call now_dos
        mov [di+22], ax
        mov [di+24], dx
        add word [dirpos], 32
.r:
        ret

; -> AX = hora DOS, DX = fecha DOS
now_dos:
        push cx
        push bx
        mov ah, 0x2C
        int 0x21
        movzx ax, ch
        shl ax, 6
        or al, cl
        shl ax, 5
        mov bl, dh
        shr bl, 1
        or al, bl
        push ax
        mov ah, 0x2A
        int 0x21
        sub cx, 1980
        mov ax, cx
        shl ax, 4
        or al, dh
        shl ax, 5
        or al, dl
        mov dx, ax
        pop ax
        pop bx
        pop cx
        ret

; AX = clúster, DX = valor -> entrada de la FAT en fatb
set_fat:
        push ax
        push bx
        push dx
        cmp byte [fat16], 0
        je .f12
        mov bx, ax
        shl bx, 1
        mov [fatb+bx], dx
        jmp .r
.f12:
        and dx, 0x0FFF
        mov bx, ax
        shr bx, 1
        add bx, ax
        test al, 1
        jz .even
        shl dx, 4
        and word [fatb+bx], 0x000F
        or [fatb+bx], dx
        jmp .r
.even:
        and word [fatb+bx], 0xF000
        or [fatb+bx], dx
.r:
        pop dx
        pop bx
        pop ax
        ret

write_tables:
        movzx eax, word [rsv]
        movzx ecx, byte [nfats]
.l:
        push ecx
        mov cx, 1
        mov dx, ds
        mov bx, fatb
        call abs_write
        pop ecx
        jc .r
        movzx edx, word [spf]
        add eax, edx
        loop .l
        mov eax, [rootlba]
        mov cx, 1
        mov dx, ds
        mov bx, rootb
        jmp abs_write
.r:
        ret

; -----------------------------------------------------------------------------
;  Copia del sistema (/S)
; -----------------------------------------------------------------------------
load_system:
        mov ax, 0x3305              ; unidad de arranque
        int 0x21
        add dl, 'A' - 1
        mov [sysname1], dl
        mov [sysname2], dl
        mov si, sysname1
        mov di, sysf1
        call load_file
        jc .r
        mov si, sysname2
        mov di, sysf2
        call load_file
.r:
        ret

; SI = ruta, DI = estructura {seg, tamaño dd, hora, fecha, nombre11}
load_file:
        mov [lf_name], si
        mov dx, si
        mov ax, 0x3D00
        int 0x21
        jc .nf
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        test dx, dx
        jnz .big
        cmp ax, 0xF000
        ja .big
        mov [di+2], ax
        mov word [di+4], 0
        push ax
        mov ax, 0x4200
        xor cx, cx
        xor dx, dx
        int 0x21
        mov ax, 0x5700
        int 0x21
        mov [di+6], cx
        mov [di+8], dx
        pop ax
        ; memoria: tamaño + un clúster de margen (máx. 64 KB)
        add ax, 15
        shr ax, 4
        add ax, 0x800
        cmp ax, 0x1000
        jbe .sz
        mov ax, 0x1000
.sz:
        push bx
        mov bx, ax
        mov [lf_paras], ax
        mov ah, 0x48
        int 0x21
        pop bx
        jc .nomem
        mov [di], ax
        ; poner el bloque a cero
        push di
        push es
        mov es, ax
        xor di, di
        mov cx, [lf_paras]
        shl cx, 3
        xor ax, ax
        rep stosw
        pop es
        pop di
        ; leer
        push ds
        mov cx, [di+2]
        mov ax, [di]
        mov ds, ax
        xor dx, dx
        mov ah, 0x3F
        int 0x21
        pop ds
        jc .rerr
        mov ah, 0x3E
        int 0x21
        clc
        ret
.nf:
        mov si, msg_nosys
        call puts
        mov si, [lf_name]
        call puts
        call crlf
        stc
        ret
.big:
.nomem:
        mov ah, 0x3E
        int 0x21
        mov si, msg_sysbig
        call puts
        stc
        ret
.rerr:
        mov si, msg_ioerr
        call puts
        stc
        ret

install_system:
        mov si, sysf1
        call put_file
        jc .r
        mov si, sysf2
        call put_file
.r:
        ret

; SI = estructura del fichero -> clústeres, cadena FAT y entrada de directorio
put_file:
        ; clústeres necesarios
        movzx eax, word [si+2]
        add eax, [bpc]
        dec eax
        xor edx, edx
        div dword [bpc]
        test eax, eax
        jnz .nz
        inc eax
.nz:
        mov [pf_n], ax
        ; ¿cabe la cadena en el primer sector de la FAT?
        movzx ecx, word [next_clus]
        add ecx, eax
        cmp byte [fat16], 0
        je .c12
        cmp ecx, 255
        ja .toolong
        jmp .cok
.c12:
        cmp ecx, 340
        ja .toolong
.cok:
        ; escribir los datos
        movzx eax, word [next_clus]
        sub eax, 2
        movzx ecx, word [spc]
        imul eax, ecx
        add eax, [datalba]
        movzx ecx, word [pf_n]
        movzx edx, word [spc]
        imul ecx, edx
        mov dx, [si]
        xor bx, bx
        call abs_write
        jc .r
        ; cadena
        mov ax, [next_clus]
        mov cx, [pf_n]
.ch:
        mov dx, ax
        inc dx
        cmp cx, 1
        jne .sf
        mov dx, 0xFFFF
.sf:
        call set_fat
        inc ax
        loop .ch
        ; entrada de directorio
        mov di, [dirpos]
        push si
        lea si, [si+10]
        mov cx, 11
        rep movsb
        pop si
        mov di, [dirpos]
        mov al, [si+21]
        mov [di+11], al
        mov ax, [si+6]
        mov [di+22], ax
        mov ax, [si+8]
        mov [di+24], ax
        mov ax, [next_clus]
        mov [di+26], ax
        movzx eax, word [si+2]
        mov [di+28], eax
        add word [dirpos], 32
        mov ax, [pf_n]
        add [next_clus], ax
        movzx eax, ax
        imul eax, [bpc]
        add [used], eax
        clc
.r:
        ret
.toolong:
        mov si, msg_sysbig
        call puts
        stc
        ret

; -----------------------------------------------------------------------------
report:
        call crlf
        mov eax, [nclus]
        imul eax, [bpc]
        mov [totb], eax
        mov cl, 15
        call putnum_w
        mov si, msg_total
        call puts
        cmp byte [opt_s], 0
        je .nos
        mov eax, [used]
        mov cl, 15
        call putnum_w
        mov si, msg_sysused
        call puts
.nos:
        mov eax, [totb]
        sub eax, [used]
        mov cl, 15
        call putnum_w
        mov si, msg_avail
        call puts
        call crlf
        mov eax, [bpc]
        mov cl, 15
        call putnum_w
        mov si, msg_unit
        call puts
        mov eax, [totb]
        sub eax, [used]
        xor edx, edx
        div dword [bpc]
        mov cl, 15
        call putnum_w
        mov si, msg_units
        call puts
        call crlf
        mov si, msg_serial
        call puts
        mov ax, [serial+2]
        call puthex4
        mov al, '-'
        call putc
        mov ax, [serial]
        call puthex4
        call crlf
        ret

%include "util.inc"

; estructuras de los ficheros de sistema: seg, tamaño(dd), hora, fecha,
; nombre 8.3 (11), atributos
sysf1:      dw 0
            dd 0
            dw 0, 0
            db "KERNEL  SYS"
            db 0x07
sysf2:      dw 0
            dd 0
            dw 0, 0
            db "COMMAND COM"
            db 0x20
sysname1    db "A:\KERNEL.SYS", 0
sysname2    db "A:\COMMAND.COM", 0
noname      db "NO NAME    "
fs12        db "FAT12   "
fs16        db "FAT16   "
label       db "           "

msg_title   db 13, 10, "Hispa-DOS FORMAT 0.9", 13, 10, 0
msg_usage   db "Uso: FORMAT unidad: [/S] [/V:etiqueta] [/F:tamaño] [/Q] [/U] [/AUTOTEST]", 13, 10
            db "  /F:tamaño  capacidad del disquete: 360, 720, 1200, 1440 o 2880 (KB)", 13, 10, 0
msg_baddrv  db "Especificación de unidad no válida", 13, 10, 0
msg_ioerr   db "Error de escritura en el disco: formato interrumpido", 13, 10, 0
msg_badfl   db "Tamaño de disquete no admitido", 13, 10, 0
msg_toobig  db "Partición demasiado grande para FAT16 (máximo 2 GB)", 13, 10, 0
msg_small   db "Unidad demasiado pequeña para darle formato", 13, 10, 0
msg_warn1   db 13, 10, "Atención: se perderán todos los datos del", 13, 10, "disco fijo de la unidad ", 0
msg_warn2   db ":", 13, 10, "¿Desea continuar con el formato (S/N)? ", 0
msg_insert  db 13, 10, "Inserte un disquete nuevo en la unidad ", 0
msg_insert2 db ":", 13, 10, "y pulse INTRO cuando esté listo...", 0
msg_formatting db 13, 10, "Formateando ", 0
msg_done    db "Formato completado.", 13, 10, 0
msg_systr   db "Sistema transferido", 13, 10, 0
msg_nosys   db "No se encuentra el archivo de sistema ", 0
msg_sysbig  db "Los archivos de sistema son demasiado grandes", 13, 10, 0
msg_total   db " bytes de espacio total en disco", 13, 10, 0
msg_sysused db " bytes usados por el sistema", 13, 10, 0
msg_avail   db " bytes disponibles en el disco", 13, 10, 0
msg_unit    db " bytes en cada unidad de asignación.", 13, 10, 0
msg_units   db " unidades de asignación disponibles en el disco.", 13, 10, 0
msg_serial  db "El número de serie del volumen es ", 0

boot12:     incbin "build/boot.bin"
boot16:     incbin "build/boot16.bin"

        section .bss
bss_start:
drv         resb 1
fixed       resb 1
fat16       resb 1
opt_s       resb 1
opt_v       resb 1
opt_auto    resb 1
nfats       resb 1
media       resb 1
            alignb 4
tot         resd 1
hidden      resd 1
rootlba     resd 1
datalba     resd 1
nclus       resd 1
bpc         resd 1
used        resd 1
totb        resd 1
serial      resd 1
spt         resw 1
heads       resw 1
spc         resw 1
rsv         resw 1
rootent     resw 1
rootsecs    resw 1
spf         resw 1
next_clus   resw 1
fsz         resw 1
dirpos      resw 1
pf_n        resw 1
lf_name     resw 1
lf_paras    resw 1
junk        resw 1
packet:
pk_sec      resd 1
pk_cnt      resw 1
pk_off      resw 1
pk_seg      resw 1
params      resb 64
u_numbuf    resb 16
u_lbuf      resb 84
            alignb 16
secb        resb 512
fatb        resb 512
rootb       resb 512
zbuf        resb 512*ZCHUNK
            resb 1024
stack_top:
