; =============================================================================
;  Hispa-DOS SYSINFO.COM - propiedades del sistema (cuadro informativo)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Muestra: sistema, procesador, memoria, discos, almacenamiento, vídeo,
;  audio, USB, CD/DVD y red. Se ejecuta desde AUTOEXEC.BAT al arrancar.
; =============================================================================
        cpu 586
        bits 16
        org 0x100

WIDTH       equ 70                  ; ancho interior del cuadro

start:
        mov sp, stack_top
        call box_top
        ; ---- sistema ----
        mov si, l_os
        call ln_begin
        mov si, v_os
        call app
        mov ah, 0x30
        int 0x21
        push ax
        movzx eax, al
        call app_num
        mov al, '.'
        call app_ch
        pop ax
        movzx eax, ah
        cmp al, 10
        jae .v2
        push ax
        mov al, '0'
        call app_ch
        pop ax
.v2:
        call app_num
        mov si, v_os2
        call app
        call ln_end
        ; ---- procesador ----
        mov si, l_cpu
        call ln_begin
        call cpu_info
        call ln_end
        ; ---- memoria ----
        mov si, l_mem
        call ln_begin
        int 0x12
        movzx eax, ax
        call app_num
        mov si, v_kbconv
        call app
        call ext_mem                ; EAX = KB extendidos
        shr eax, 10
        call app_num
        mov si, v_mbext
        call app
        call ln_end
        ; ---- discos ----
        call disks
        ; ---- almacenamiento (PCI) ----
        mov si, l_stor
        call ln_begin
        mov byte [any], 0
        ; IDE: varias interfaces posibles (00h, 80h, 85h, 8Ah, 8Fh)
        mov bx, ide_ifs
.ide:
        mov cl, [bx]
        cmp cl, 0xFF
        je .ided
        movzx ecx, cl
        or ecx, 0x010100
        push bx
        mov si, v_ide
        call pci_count_named
        pop bx
        inc bx
        jmp .ide
.ided:
        mov ecx, 0x010601
        mov si, v_ahci
        call pci_count_named
        mov ecx, 0x010802
        mov si, v_nvme
        call pci_count_named
        call none_if_empty
        call ln_end
        ; ---- vídeo ----
        mov si, l_video
        call ln_begin
        call video_info
        call ln_end
        ; ---- audio ----
        mov si, l_audio
        call ln_begin
        mov byte [any], 0
        mov ecx, 0x040300
        mov si, v_hda
        call pci_count_named
        mov ecx, 0x040100
        mov si, v_ac97
        call pci_count_named
        ; las Sound Blaster (ISA) no están en el PCI: se le pregunta a HSPSND
        ; qué tarjeta usa (función 00h: AL = 1 Sound Blaster, 2 SB16)
        mov byte [snd_on], 0
        push es                     ; (devuelve en ES:BX la entrada de HSPSND:
        push di                     ; sin esto, el resto de las líneas se
        mov ax, 0xD700              ; escribían encima del driver residente)
        int 0x2F
        cmp al, 0xFF
        jne .nosb
        mov byte [snd_on], 1
        mov [snd_ent], bx
        mov [snd_ent+2], es
        mov ah, 0x00
        call far [snd_ent]          ; -> AL = tarjeta (ES:DI = nombre)
        mov [snd_card], al
.nosb:
        pop di
        pop es
        cmp byte [snd_on], 0
        je .nosb2
        mov si, v_sb16
        cmp byte [snd_card], 2
        je .sb
        mov si, v_sb
        cmp byte [snd_card], 1
        jne .nosb2
.sb:
        cmp byte [any], 0
        je .sb1
        push si
        mov si, v_sep
        call app
        pop si
.sb1:
        call app
        mov byte [any], 1
.nosb2:
        call none_if_empty
        cmp byte [snd_on], 0
        je .nosnd
        mov si, v_hspsnd
        call app
.nosnd:
        call ln_end
        ; ---- USB ----
        mov si, l_usb
        call ln_begin
        mov byte [any], 0
        mov ecx, 0x0C0330
        mov si, v_xhci
        call pci_count_named
        mov ecx, 0x0C0320
        mov si, v_ehci
        call pci_count_named
        mov ecx, 0x0C0310
        mov si, v_ohci
        call pci_count_named
        mov ecx, 0x0C0300
        mov si, v_uhci
        call pci_count_named
        call none_if_empty
        call ln_end
        ; ---- CD/DVD ----
        mov si, l_cd
        call ln_begin
        call atapi_info
        call ln_end
        ; ---- red ----
        mov si, l_net
        call ln_begin
        mov ecx, 0x020000
        call pci_vendor_name
        call ln_end
        call box_bottom
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  Detección
; =============================================================================
; procesador: CPUID (fabricante y nombre comercial)
cpu_info:
        ; ¿existe CPUID? (bit ID de EFLAGS)
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        jnz .cpuid
        mov si, v_cpu486
        jmp app
.cpuid:
        mov eax, 0x80000000
        cpuid
        cmp eax, 0x80000004
        jb .vendor
        ; nombre comercial (48 caracteres)
        push di
        mov di, cpu_brand
        mov eax, 0x80000002
.b:
        push eax
        cpuid
        mov [di], eax
        mov [di+4], ebx
        mov [di+8], ecx
        mov [di+12], edx
        add di, 16
        pop eax
        inc eax
        cmp eax, 0x80000004
        jbe .b
        mov byte [di], 0
        pop di
        mov si, cpu_brand
.sp:
        cmp byte [si], ' '
        jne .bn
        inc si
        jmp .sp
.bn:
        cmp byte [si], 0            ; nombre vacío: usar el fabricante
        je .vendor
        jmp app
.vendor:
        xor eax, eax
        cpuid
        mov [cpu_brand], ebx
        mov [cpu_brand+4], edx
        mov [cpu_brand+8], ecx
        mov byte [cpu_brand+12], 0
        mov si, cpu_brand
        jmp app

; -> EAX = KB de memoria extendida
ext_mem:
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
        ret
.old:
        mov ah, 0x88
        int 0x15
        movzx eax, ax
        ret

; discos duros (INT 13h) y disquetes
disks:
        mov si, l_disk
        call ln_begin
        mov byte [any], 0
        push di
        mov ah, 8
        mov dl, 0x80
        xor di, di
        mov es, di
        int 0x13
        pop di
        push cs
        pop es
        jc .nohd
        movzx cx, dl
        jcxz .nohd
        mov byte [dnum], 0x80
.d:
        push cx
        cmp byte [any], 0
        je .f
        mov si, v_sep
        call app
.f:
        mov byte [any], 1
        ; tamaño por INT 13h AH=48h
        mov word [edd], 0x1A
        mov ah, 0x48
        mov dl, [dnum]
        mov si, edd
        push di
        int 0x13
        pop di
        jc .chs
        mov eax, [edd+0x10]
        shr eax, 11
        call app_num
        mov si, v_mb
        call app
        jmp .n
.chs:
        mov si, v_hd
        call app
.n:
        inc byte [dnum]
        pop cx
        loop .d
.nohd:
        cmp byte [any], 0
        jne .fl
        mov si, v_none
        call app
.fl:
        int 0x11
        test al, 1
        jz .e
        shr al, 6
        and al, 3
        inc al
        push ax
        mov si, v_sep
        call app
        pop ax
        movzx eax, al
        call app_num
        mov si, v_floppy
        call app
.e:
        jmp ln_end

; ECX = clase PCI, SI = nombre -> "nombre" o "n x nombre" si hay alguno
pci_count_named:
        push si
        xor si, si
.l:
        push ecx
        push si
        push di
        mov ax, 0xB103
        int 0x1A
        pop di
        pop si
        pop ecx
        jc .done
        inc si
        jmp .l
.done:
        mov ax, si
        pop si
        test ax, ax
        jz .r
        push ax
        cmp byte [any], 0
        je .f
        push si
        mov si, v_sep
        call app
        pop si
.f:
        mov byte [any], 1
        pop ax
        cmp ax, 1
        je .one
        movzx eax, ax
        call app_num
        mov al, 'x'
        call app_ch
        mov al, ' '
        call app_ch
.one:
        call app
.r:
        ret

none_if_empty:
        cmp byte [any], 0
        jne .r
        mov si, v_none
        call app
.r:
        ret

; ECX = clase -> fabricante del primer dispositivo de esa clase (o ninguno)
pci_vendor_name:
        push di
        xor si, si
        mov ax, 0xB103
        int 0x1A
        jc .none0
        mov ax, 0xB10A
        xor di, di
        int 0x1A                    ; ECX = dispositivo:fabricante
        pop di
        mov [pci_id], ecx
        mov si, vendors
.l:
        mov ax, [si]
        test ax, ax
        jz .unk
        cmp ax, cx
        je .f
        add si, 2
.s:
        lodsb
        test al, al
        jnz .s
        jmp .l
.f:
        add si, 2
        jmp app
.unk:
        mov si, v_vendor
        call app
        mov ax, [pci_id]
        call app_hex4
        mov al, ':'
        call app_ch
        mov ax, [pci_id+2]
        jmp app_hex4
.none0:
        pop di
.none:
        mov si, v_none
        jmp app

; vídeo: fabricante PCI + VESA (versión, memoria)
video_info:
        mov ecx, 0x030000
        call pci_vendor_name
        mov dword [vbe], 'VBE2'
        push di
        mov ax, 0x4F00
        mov di, vbe
        int 0x10
        pop di
        cmp ax, 0x004F
        jne .r
        cmp dword [vbe], 'VESA'
        jne .r
        mov si, v_vesa
        call app
        movzx eax, byte [vbe+5]
        call app_num
        mov al, '.'
        call app_ch
        movzx eax, byte [vbe+4]
        call app_num
        mov al, ','
        call app_ch
        mov al, ' '
        call app_ch
        movzx eax, word [vbe+0x12]
        shr eax, 4                  ; bloques de 64 KB -> MB
        call app_num
        mov si, v_mb
        call app
.r:
        ret

; CD/DVD: IDENTIFY PACKET DEVICE en los canales IDE clásicos
atapi_info:
        mov byte [any], 0
        mov si, ide_list
.l:
        mov dx, [si]
        test dx, dx
        jz .end
        mov al, [si+2]
        push si
        call atapi_probe
        pop si
        add si, 3
        jmp .l
.end:
        cmp byte [any], 0
        jne .r
        mov si, v_none
        call app
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

; DX = puerto base, AL = A0h/B0h (maestro/esclavo)
atapi_probe:
        push di
        call .probe
        pop di
        jnc .r
        ret
.r:
        ; añadir el modelo a la línea
        cmp byte [any], 0
        je .f
        mov si, v_sep
        call app
.f:
        mov byte [any], 1
        mov si, cd_model
        jmp app
.probe:
        mov [ide_base], dx
        add dx, 6
        out dx, al
        call ide_delay
        ; ¿hay algo en el canal?
        mov dx, [ide_base]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        je .no
        ; IDENTIFY PACKET DEVICE
        mov al, 0xA1
        out dx, al
        call ide_delay
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80               ; ocupado
        jnz .n
        test al, 1                  ; error: no es ATAPI
        jnz .no
        test al, 8                  ; datos listos
        jnz .data
.n:
        loop .w
        jmp .no
.data:
        mov dx, [ide_base]
        mov di, ident
        mov cx, 256
        cld
        rep insw
        ; modelo: palabras 27-46, bytes intercambiados
        mov si, ident + 54
        mov di, cd_model
        mov cx, 20
.m:
        lodsw
        xchg al, ah
        stosw
        loop .m
        mov byte [di], 0
        ; quitar espacios finales
.t:
        dec di
        cmp di, cd_model
        jb .tt
        cmp byte [di], ' '
        jne .tt
        mov byte [di], 0
        jmp .t
.tt:
        clc
        ret
.no:
        stc
        ret
ide_delay:
        push dx
        push cx
        mov dx, [ide_base]
        add dx, 7
        mov cx, 4
.l:
        in al, dx
        loop .l
        pop cx
        pop dx
        ret

; =============================================================================
;  Cuadro de texto
; =============================================================================
box_top:
        call crlf
        mov al, '╔'
        call putc
        mov cx, WIDTH
.l:
        mov al, '═'
        call putc
        loop .l
        mov al, '╗'
        call putc
        call crlf
        ; logo
        mov bx, logo
.lg:
        mov si, [bx]
        test si, si
        jz .lgd
        push bx
        call ln_begin0
        call ln_end
        pop bx
        add bx, 2
        jmp .lg
.lgd:
        mov si, s_empty
        call ln_begin0
        call ln_end
        mov si, title
        call ln_begin0
        call ln_end
        mov al, '╟'
        call putc
        mov cx, WIDTH
.l2:
        mov al, '─'
        call putc
        loop .l2
        mov al, '╢'
        call putc
        jmp crlf
box_bottom:
        mov al, '╚'
        call putc
        mov cx, WIDTH
.l:
        mov al, '═'
        call putc
        loop .l
        mov al, '╝'
        call putc
        jmp crlf

; empezar una línea con la etiqueta SI (rellenada a 13 columnas)
ln_begin:
        mov di, line
        mov al, ' '
        stosb
.c:
        lodsb
        test al, al
        jz .p
        stosb
        jmp .c
.p:
        cmp di, line + 14
        jae .r
        mov al, ' '
        stosb
        jmp .p
.r:
        ret
; línea centrada con el texto SI
ln_begin0:
        mov di, line
        mov al, ' '
        stosb
        jmp ln_begin.c

app:
        lodsb
        test al, al
        jz .r
        cmp di, line + WIDTH - 1
        jae app
        stosb
        jmp app
.r:
        ret
app_ch:
        cmp di, line + WIDTH - 1
        jae .r
        stosb
.r:
        ret
app_num:
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
        call app_ch
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret
app_hex4:
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call app_ch
        pop ax
        loop .l
        ret
; cerrar la línea: rellenar y escribirla entre bordes
ln_end:
        mov al, ' '
.p:
        cmp di, line + WIDTH
        jae .w
        stosb
        jmp .p
.w:
        mov byte [di], 0
        mov al, '║'
        call putc
        mov si, line
        call puts
        mov al, '║'
        call putc
        jmp crlf

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
crlf:
        push ax
        mov al, 13
        call putc
        mov al, 10
        call putc
        pop ax
        ret

title       db "                     Propiedades del sistema", 0
logo        dw logo0, logo1, logo2, logo3, logo4, 0
logo0       db "  ██   ██ ██ ███████ ██████   █████        ██████   ██████  ███████", 0
logo1       db "  ██   ██ ██ ██      ██   ██ ██   ██       ██   ██ ██    ██ ██", 0
logo2       db "  ███████ ██ ███████ ██████  ███████ █████ ██   ██ ██    ██ ███████", 0
logo3       db "  ██   ██ ██      ██ ██      ██   ██       ██   ██ ██    ██      ██", 0
logo4       db "  ██   ██ ██ ███████ ██      ██   ██       ██████   ██████  ███████", 0
s_empty     db 0

l_os        db "Sistema", 0
l_cpu       db "Procesador", 0
l_mem       db "Memoria", 0
l_disk      db "Discos", 0
l_stor      db "Controlador", 0
l_video     db "Vídeo", 0
l_audio     db "Audio", 0
l_usb       db "USB", 0
l_cd        db "CD/DVD", 0
l_net       db "Red", 0
v_os        db "Hispa-DOS 0.9 (DOS ", 0
v_os2       db ")", 0
v_cpu486    db "386/486 (sin CPUID)", 0
v_kbconv    db " KB convencional, ", 0
v_mbext     db " MB extendida", 0
v_sep       db ", ", 0
v_mb        db " MB", 0
v_hd        db "disco duro", 0
v_floppy    db " disquetera(s)", 0
v_none      db "no detectado", 0
v_ide       db "IDE", 0
ide_ifs     db 0x00, 0x80, 0x85, 0x8A, 0x8F, 0xFF
v_ahci      db "SATA (AHCI)", 0
v_nvme      db "NVMe", 0
v_hda       db "HD Audio", 0
v_ac97      db "AC'97", 0
v_sb        db "Sound Blaster", 0
v_sb16      db "Sound Blaster 16", 0
v_hspsnd    db " - HSPSND cargado", 0
v_xhci      db "xHCI (USB 3)", 0
v_ehci      db "EHCI (USB 2)", 0
v_ohci      db "OHCI (USB 1.1)", 0
v_uhci      db "UHCI (USB 1.1)", 0
v_vendor    db "fabricante ", 0
v_vesa      db " - VESA ", 0
vendors     dw 0x8086
            db "Intel", 0
            dw 0x10DE
            db "NVIDIA", 0
            dw 0x1002
            db "AMD/ATI", 0
            dw 0x1022
            db "AMD", 0
            dw 0x1234
            db "QEMU/Bochs (VGA estándar)", 0
            dw 0x15AD
            db "VMware", 0
            dw 0x80EE
            db "VirtualBox", 0
            dw 0x1AF4
            db "Red Hat virtio", 0
            dw 0x10EC
            db "Realtek", 0
            dw 0x14E4
            db "Broadcom", 0
            dw 0x1B36
            db "Red Hat (QEMU)", 0
            dw 0x5333
            db "S3", 0
            dw 0x1013
            db "Cirrus Logic", 0
            dw 0x102B
            db "Matrox", 0
            dw 0x1106
            db "VIA", 0
            dw 0x10B7
            db "3Com", 0
            dw 0x1969
            db "Atheros/Qualcomm", 0
            dw 0

        section .bss
any         resb 1
snd_on      resb 1
snd_card    resb 1
snd_ent     resd 1
dnum        resb 1
ide_base    resw 1
pci_id      resd 1
edd         resb 0x42
cpu_brand   resb 52
cd_model    resb 44
line        resb WIDTH + 4
ident       resb 512
vbe         resb 512
            resb 1024
stack_top:
