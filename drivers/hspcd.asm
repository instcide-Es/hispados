; =============================================================================
;  HSPCD.SYS - lectores de CD/DVD (ATAPI) para Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\DRIVERS\HSPCD.SYS
;
;  Busca lectores ATAPI en los canales IDE clásicos (1F0h y 170h, maestro y
;  esclavo) y en las controladoras SATA en modo AHCI (las de los PCs actuales;
;  necesita modo real: no funciona con EMM386 cargado), y los ofrece como
;  unidades de bloque de sectores de 512 bytes
;  (cada sector de 2048 bytes del disco son cuatro). El núcleo de Hispa-DOS
;  reconoce el sistema de archivos ISO 9660 de los CD y DVD de datos.
; =============================================================================
        cpu 486
        bits 16
        org 0

MAXCD       equ 4

header:
        dw 0xFFFF, 0xFFFF
        dw 0x0802                   ; bloque, sectores de 32 bits
        dw strategy
        dw interrupt
        db 1, "HSPCD  "

req         dd 0
ncd         db 0
            db 0
cd_port     dw 0, 0, 0, 0           ; IDE: puerto; AHCI: número de puerto
cd_sel      db 0, 0, 0, 0
cd_type     db 0, 0, 0, 0           ; 0 = IDE, 1 = AHCI
cd_pbase    dd 0, 0, 0, 0           ; AHCI: registros del puerto
cd_mem      dw 0, 0, 0, 0           ; AHCI: zona de 1 KB (lista, FIS, tabla)
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
buf_unit    db 0xFF
            db 0
buf_lba     dd 0xFFFFFFFF
; BPB ficticio: el núcleo detecta ISO 9660 leyendo el sector 16
bpb:
            dw 512
            db 4
            dw 1
            db 0                    ; sin FAT: el núcleo prueba ISO 9660
            dw 16
            dw 0
            db 0xF8
            dw 1
            dw 1
            dw 1
            dd 0
            dd 0x7FFFFFFF
bpbarr      dw bpb, bpb, bpb, bpb

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        pushad
        push ds
        push es
        lds bx, [cs:req]
        mov al, [bx+2]
        cmp al, 0
        je init
        cmp al, 1
        je .media
        cmp al, 2
        je .bpb
        cmp al, 4
        je .read
        cmp al, 8
        je .wp
        cmp al, 9
        je .wp
.done:
        mov word [bx+3], 0x0100
.out:
        pop es
        pop ds
        popad
        retf
.media:
        mov byte [bx+0x0E], 0       ; "no se sabe": puede haber cambiado el disco
        jmp .done
.bpb:
        mov word [bx+0x12], bpb
        mov [bx+0x14], cs
        jmp .done
.wp:
        mov word [bx+3], 0x8100     ; protegido contra escritura
        jmp .out
.read:
        movzx eax, word [bx+0x14]
        cmp ax, 0xFFFF
        jne .s
        mov eax, [bx+0x1A]
.s:
        mov [cs:r_lba], eax
        mov cx, [bx+0x12]
        les di, [bx+0x0E]
        mov al, [bx+1]
        mov [cs:r_unit], al
        cld
.l:
        jcxz .done
        ; bloque de 2048 bytes que contiene el sector de 512
        mov eax, [cs:r_lba]
        shr eax, 2
        call get_block
        jc .err
        mov si, [cs:r_lba]
        and si, 3
        shl si, 9
        add si, cdbuf
        push ds
        push cs
        pop ds
        push cx
        mov cx, 256
        rep movsw
        pop cx
        pop ds
        ; normalizar ES:DI
        mov ax, di
        shr ax, 4
        mov dx, es
        add dx, ax
        mov es, dx
        and di, 15
        inc dword [cs:r_lba]
        dec cx
        jmp .l
.err:
        mov ax, [bx+0x12]
        sub ax, cx
        mov [bx+0x12], ax
        mov word [bx+3], 0x8102     ; unidad no preparada (sin disco)
        jmp .out
r_lba       dd 0
r_unit      db 0

; EAX = bloque de 2048 de la unidad r_unit -> cdbuf; CF si falla
get_block:
        mov dl, [cs:r_unit]
        cmp dl, [cs:buf_unit]
        jne .rd
        cmp eax, [cs:buf_lba]
        je .ok
.rd:
        push bx
        push cx
        push di
        push es
        mov [cs:buf_lba], eax
        mov [cs:buf_unit], dl
        mov cx, 3
.try:
        push cx
        call atapi_read
        pop cx
        jnc .good
        loop .try
        mov byte [cs:buf_unit], 0xFF
        pop es
        pop di
        pop cx
        pop bx
        stc
        ret
.good:
        pop es
        pop di
        pop cx
        pop bx
.ok:
        clc
        ret

; leer el bloque [buf_lba] de la unidad [buf_unit] con READ(10) por PIO
atapi_read:
        movzx bx, byte [cs:buf_unit]
        cmp byte [cs:cd_type+bx], 1
        je ahci_read
        mov al, [cs:cd_sel+bx]
        shl bx, 1
        mov dx, [cs:cd_port+bx]
        mov [cs:port], dx
        add dx, 6
        out dx, al
        call delay400
        call wait_nbsy
        jc .r
        mov dx, [cs:port]
        inc dx
        xor al, al
        out dx, al                  ; PIO
        add dx, 3
        out dx, al                  ; límite de bytes: 2048
        inc dx
        mov al, 0x08
        out dx, al
        add dx, 2
        mov al, 0xA0                ; PACKET
        out dx, al
        call delay400
        call wait_drq
        jc .r
        ; paquete READ(10) de un bloque
        mov eax, [cs:buf_lba]
        bswap eax
        mov [cs:pkt+2], eax
        mov word [cs:pkt+7], 0x0100
        mov dx, [cs:port]
        mov si, pkt
        mov cx, 6
.p:
        mov ax, [cs:si]
        out dx, ax
        add si, 2
        loop .p
        call delay400
        call wait_drq
        jc .r
        mov dx, [cs:port]
        push cs
        pop es
        mov di, cdbuf
        mov cx, 1024
        cld
        rep insw
        call delay400
        call wait_nbsy
.r:
        ret
pkt         db 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
port        dw 0

delay400:
        push dx
        push cx
        mov dx, [cs:port]
        add dx, 7
        mov cx, 4
.l:
        in al, dx
        loop .l
        pop cx
        pop dx
        ret
; esperar que no esté ocupado (CF si error o plazo)
wait_nbsy:
        push cx
        push dx
        mov dx, [cs:port]
        add dx, 7
        mov ecx, 0x200000
.l:
        in al, dx
        test al, 0x80
        jz .ok
        loop .l, ecx
        stc
        jmp .r
.ok:
        test al, 1
        jz .c
        stc
        jmp .r
.c:
        clc
.r:
        pop dx
        pop cx
        ret
; esperar datos (DRQ) (CF si error o plazo)
wait_drq:
        push cx
        push dx
        mov dx, [cs:port]
        add dx, 7
        mov ecx, 0x200000
.l:
        in al, dx
        test al, 0x80
        jnz .n
        test al, 1
        jnz .e
        test al, 8
        jnz .ok
.n:
        loop .l, ecx
.e:
        stc
        jmp .r
.ok:
        clc
.r:
        pop dx
        pop cx
        ret

; ---- AHCI ----
; modo irreal: FS con base 0 y límite de 4 GB (CF si estamos en modo V86)
unreal:
        push eax
        push bx
        smsw ax
        test al, 1
        jnz .v86
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
        pop bx
        pop eax
        clc
        ret
.v86:
        pop bx
        pop eax
        stc
        ret

; CS:AX -> EAX física
phys:
        push ebx
        movzx ebx, ax
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, ebx
        pop ebx
        ret

; orden ATAPI por AHCI: BX = unidad*1, AL = orden ATA (A0h PACKET o A1h
; IDENTIFY PACKET), ECX = bytes que se leen a cdbuf, paquete en pkt -> CF
ahci_cmd:
        push esi
        push edi
        push fs
        mov [cs:ac_cmd], al
        mov [cs:ac_len], ecx
        call unreal
        jc .fail
        mov di, bx
        shl di, 1
        mov di, [cs:cd_mem+di]      ; zona del puerto
        ; cabecera de orden 0: FIS de 5 dwords, 1 PRD, ATAPI si es PACKET
        mov eax, 0x00010005
        cmp byte [cs:ac_cmd], 0xA0
        jne .h
        or al, 0x20
.h:
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        lea ax, [di+0x200]
        call phys
        mov [cs:di+8], eax
        mov dword [cs:di+12], 0
        ; tabla: FIS H2D, paquete ATAPI y PRD
        push di
        add di, 0x200
        push cx
        push di
        xor eax, eax
        mov cx, 0x90 / 4
.z:
        mov [cs:di], eax
        add di, 4
        loop .z
        pop di
        pop cx
        mov byte [cs:di], 0x27      ; H2D
        mov byte [cs:di+1], 0x80    ; orden
        mov al, [cs:ac_cmd]
        mov [cs:di+2], al
        cmp al, 0xA0
        jne .nf
        mov byte [cs:di+3], 1       ; DMA
        mov byte [cs:di+5], 0x00    ; límite de bytes: 2048
        mov byte [cs:di+6], 0x08
        mov eax, [cs:pkt]
        mov [cs:di+0x40], eax
        mov eax, [cs:pkt+4]
        mov [cs:di+0x44], eax
        mov eax, [cs:pkt+8]
        mov [cs:di+0x48], eax
.nf:
        mov ax, cdbuf
        call phys
        mov [cs:di+0x80], eax
        mov dword [cs:di+0x84], 0
        mov eax, [cs:ac_len]
        dec eax
        mov [cs:di+0x8C], eax
        pop di
        ; registros del puerto
        mov si, bx
        shl si, 2
        mov esi, [cs:cd_pbase+si]
        ; esperar que no esté ocupado (TFD: BSY, DRQ)
        mov ecx, 0x400000
.nb:
        test byte [fs:esi+0x20], 0x88
        jz .go
        dec ecx
        jnz .nb
        jmp .fail
.go:
        mov dword [fs:esi+0x10], 0xFFFFFFFF ; borrar IS
        mov dword [fs:esi+0x30], 0xFFFFFFFF ; borrar SERR
        mov dword [fs:esi+0x38], 1          ; CI: orden 0
        mov ecx, 0x2000000
.w:
        test dword [fs:esi+0x10], 0x40000000    ; TFES: error
        jnz .fail
        test dword [fs:esi+0x38], 1
        jz .done
        dec ecx
        jnz .w
        jmp .fail
.done:
        test byte [fs:esi+0x20], 1          ; ERR
        jnz .fail
        pop fs
        pop edi
        pop esi
        clc
        ret
.fail:
        pop fs
        pop edi
        pop esi
        stc
        ret
ac_cmd      db 0
ac_len      dd 0

; READ(10) del bloque [buf_lba] por AHCI (BX = unidad)
ahci_read:
        mov eax, [cs:buf_lba]
        bswap eax
        mov dword [cs:pkt], 0x28
        mov [cs:pkt+2], eax
        mov dword [cs:pkt+6], 0
        mov word [cs:pkt+7], 0x0100
        mov word [cs:pkt+10], 0
        mov al, 0xA0
        mov ecx, 2048
        call ahci_cmd
        ret

            align 2
cdbuf       times 2048 db 0

resident_end:

; =============================================================================
init:
        push ds
        push cs
        pop ds
        mov si, msg_title
        call puts
        ; buscar lectores ATAPI
        mov si, ide_list
.l:
        mov dx, [si]
        test dx, dx
        jz .end
        mov al, [si+2]
        push si
        call probe
        pop si
        add si, 3
        jmp .l
.end:
        call ahci_probe
        pop ds
        lds bx, [cs:req]
        cmp byte [cs:ncd], 0
        jne .found
        push cs
        pop ds
        mov si, msg_none
        call puts
        lds bx, [cs:req]
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
        jmp interrupt.done
.found:
        mov al, [cs:ncd]
        mov [bx+0x0D], al
        mov ax, [cs:res_end]
        mov [bx+0x0E], ax
        mov [bx+0x10], cs
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        ; letras
        mov al, [bx+0x16]
        add al, 'A'
        mov [cs:msg_drv], al
        push cs
        pop ds
        mov si, msg_units
        call puts
        jmp interrupt.done

; DX = puerto, AL = A0h/B0h
probe:
        cmp byte [ncd], MAXCD
        jae .no
        mov [port], dx
        mov [tsel], al
        add dx, 6
        out dx, al
        call delay400
        mov dx, [port]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        je .no
        mov al, 0xA1                ; IDENTIFY PACKET DEVICE
        out dx, al
        call delay400
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .n
        test al, 1
        jnz .no
        test al, 8
        jnz .data
.n:
        loop .w
        jmp .no
.data:
        mov dx, [port]
        push cs
        pop es
        mov di, cdbuf
        mov cx, 256
        cld
        rep insw
        ; registrar la unidad
        movzx bx, byte [ncd]
        mov al, [tsel]
        mov [cd_sel+bx], al
        shl bx, 1
        mov ax, [port]
        mov [cd_port+bx], ax
        inc byte [ncd]
        call print_model
.no:
        ret

print_model:
        mov si, msg_model
        call puts
        mov si, cdbuf + 54
        mov cx, 20
.m:
        lodsw
        xchg al, ah
        push ax
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        mov dl, ah
        mov ah, 2
        int 0x21
        loop .m
        mov si, msg_crlf
        call puts
        ret
tsel        db 0
res_end     dw resident_end

; ---- lectores SATA (AHCI) ----
ahci_probe:
        smsw ax
        test al, 1
        jnz .r                      ; modo V86: no se puede
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .r
        cmp edx, 0x20494350
        jne .r
        mov word [hba_idx], 0
.hba:
        mov ax, 0xB103
        mov ecx, 0x010601
        mov si, [hba_idx]
        int 0x1A
        jc .r
        mov [bdf], bx
        inc word [hba_idx]
        ; memoria y bus master
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0006
        mov bx, [bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0x24                ; BAR5 = ABAR
        int 0x1A
        and ecx, 0xFFFFFFF0
        jz .hba
        mov [abar], ecx
        ; GDT del modo irreal
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        call unreal
        jc .r
        push fs
        mov esi, [abar]
        or dword [fs:esi+4], 0x80000000     ; GHC.AE: modo AHCI
        mov eax, [fs:esi+0x0C]              ; puertos implementados
        mov [pi], eax
        xor cx, cx
.port:
        cmp cx, 32
        jae .pd
        cmp byte [ncd], MAXCD
        jae .pd
        movzx eax, cx
        bt dword [pi], eax
        jnc .np
        movzx esi, cx
        shl esi, 7
        add esi, [abar]
        add esi, 0x100
        mov eax, [fs:esi+0x28]              ; SSTS
        and al, 0x0F
        cmp al, 3                           ; dispositivo presente
        jne .np
        cmp dword [fs:esi+0x24], 0xEB140101 ; firma ATAPI
        jne .np
        push cx
        call ahci_port
        pop cx
.np:
        inc cx
        jmp .port
.pd:
        pop fs
        jmp .hba
.r:
        ret
hba_idx     dw 0
bdf         dw 0
abar        dd 0
pi          dd 0

; preparar el puerto ESI (número CX) y registrar la unidad
ahci_port:
        ; zona de 1 KB alineada a 1 KB tras todo el fichero
        mov ax, [res_end]
        cmp ax, resident_end
        jne .have
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, file_end + 1023
        and eax, 0xFFFFFC00
        sub eax, ebx
        mov [res_end], ax
.have:
        mov di, [res_end]
        add word [res_end], 0x400
        push cx
        push di
        xor ax, ax
        mov cx, 0x200
        push es
        push cs
        pop es
        cld
        rep stosw
        pop es
        pop di
        pop cx
        ; parar el puerto (ST y FRE) y esperar
        and dword [fs:esi+0x18], ~0x11
        mov ecx, 0x400000
.st:
        test dword [fs:esi+0x18], 0xC000    ; CR | FR
        jz .stopped
        dec ecx
        jnz .st
        jmp .fail
.stopped:
        mov ax, di
        call phys
        mov [fs:esi+0x00], eax              ; CLB
        mov dword [fs:esi+0x04], 0
        add eax, 0x100
        mov [fs:esi+0x08], eax              ; FB
        mov dword [fs:esi+0x0C], 0
        mov dword [fs:esi+0x30], 0xFFFFFFFF
        mov dword [fs:esi+0x10], 0xFFFFFFFF
        or dword [fs:esi+0x18], 0x10        ; FRE
        or dword [fs:esi+0x18], 0x01        ; ST
        ; registrar
        movzx bx, byte [ncd]
        mov byte [cd_type+bx], 1
        shl bx, 1
        mov [cd_mem+bx], di
        shl bx, 1
        mov [cd_pbase+bx], esi
        movzx bx, byte [ncd]
        mov al, 0xA1                        ; IDENTIFY PACKET DEVICE
        mov ecx, 512
        call ahci_cmd
        jc .fail
        inc byte [ncd]
        call print_model
        ret
.fail:
        sub word [res_end], 0x400
        ret

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

ide_list    dw 0x1F0
            db 0xA0
            dw 0x1F0
            db 0xB0
            dw 0x170
            db 0xA0
            dw 0x170
            db 0xB0
            dw 0
msg_title   db "HSPCD 0.1 - lectores de CD/DVD de Hispa-DOS", 13, 10, 0
msg_model   db "Lector: ", 0
msg_units   db "Primera unidad de CD/DVD: "
msg_drv     db "?:", 13, 10, 0
msg_none    db "No se ha encontrado ningún lector de CD/DVD (ATAPI).", 13, 10, 0
msg_crlf    db 13, 10, 0
file_end:
