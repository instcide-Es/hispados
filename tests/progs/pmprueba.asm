; PMPRUEBA.COM - prueba del gestor de protocolos NDIS 2 (PROTMAN$)
; Abre PROTMAN$, pide GetProtocolManagerInfo (1) y GetProtocolManagerLinkage (4)
; por IOCTL (INT 21h 4402h) y escribe lo que devuelve, en una línea por paso:
;   ABRIR=OK|ERR nnnn  INFO=estado VER=vvvv IMAGEN=ssss:oooo  ENLACE=estado DS=ssss
; y, si hay imagen, los nombres de los módulos de PROTOCOL.INI con sus claves.
; (Documentación pública: especificación NDIS 2.0.1 y la RBIL, INT 21h 4402h.)
        org 0x100
        cpu 386
start:
        ; la cadena de dispositivos (desde NUL, en la lista de listas + 22h)
        mov ah, 0x52
        int 0x21
        lea si, [bx+0x22]
        mov ax, es
        mov fs, ax
        mov cx, 40
.dv:
        mov dx, m_dev
        call puts
        mov ax, fs
        call hex16
        mov al, ':'
        call putc
        mov ax, si
        call hex16
        mov al, ' '
        call putc
        mov ax, [fs:si+4]
        call hex16
        mov al, ' '
        call putc
        mov ax, [fs:si+6]
        call hex16
        mov al, ' '
        call putc
        push cx
        mov cx, 8
        lea bx, [si+10]
.dn:
        mov al, [fs:bx]
        cmp al, ' '
        jb .dnx
        cmp al, 127
        jb .dnp
.dnx:
        mov al, '.'
.dnp:
        call putc
        inc bx
        loop .dn
        pop cx
        call crlf
        mov eax, [fs:si]
        cmp ax, 0xFFFF
        je .dve
        mov si, ax
        shr eax, 16
        mov fs, ax
        loop .dv
.dve:
        mov dx, nombre
        mov ax, 0x3D02
        int 0x21
        jnc .ab
        push ax
        mov dx, m_aberr
        call puts
        pop ax
        call hex16
        call crlf
        mov ax, 0x4C01
        int 0x21
.ab:
        mov [h], ax
        mov dx, m_abok
        call puts
        call crlf
        ; GetProtocolManagerInfo
        mov word [rb], 1
        call ioctl
        mov dx, m_info
        call puts
        mov ax, [rb+2]
        call hex16
        mov dx, m_ver
        call puts
        mov ax, [rb+12]
        call hex16
        mov dx, m_img
        call puts
        mov ax, [rb+6]
        call hex16
        mov al, ':'
        call putc
        mov ax, [rb+4]
        call hex16
        call crlf
        mov eax, [rb+4]
        mov [img], eax
        ; GetProtocolManagerLinkage
        mov word [rb], 4
        call ioctl
        mov dx, m_enl
        call puts
        mov ax, [rb+2]
        call hex16
        mov dx, m_ds
        call puts
        mov ax, [rb+12]
        call hex16
        call crlf
        ; recorrer la imagen (si la hay)
        cmp dword [img], 0
        je .fin
        lfs si, [img]
        mov cx, 16                      ; como mucho 16 módulos
.mod:
        push cx
        mov dx, m_mod
        call puts
        lea bx, [si+8]
        call putsfs
        call crlf
        ; claves: justo detrás del nombre (desplazamiento 24)
        lea di, [si+24]
        mov cx, 32
.kw:
        push cx
        mov dx, m_kw
        call puts
        lea bx, [di+8]
        call putsfs
        mov al, '='
        call putc
        mov cx, [fs:di+24]             ; número de parámetros
        lea bx, [di+26]
.pa:
        jcxz .pe
        cmp word [fs:bx], 0
        jne .str
        mov eax, [fs:bx+4]
        call hex32
        jmp .pn
.str:
        push bx
        add bx, 4
        call putsfs
        pop bx
.pn:
        mov al, ' '
        call putc
        mov ax, [fs:bx+2]
        add ax, 4
        add bx, ax
        dec cx
        jmp .pa
.pe:
        call crlf
        pop cx
        mov eax, [fs:di]
        test eax, eax
        jz .kwe
        mov di, ax
        dec cx
        jnz .kw
.kwe:
        pop cx
        mov eax, [fs:si]
        test eax, eax
        jz .fin
        mov si, ax
        shr eax, 16
        mov fs, ax
        dec cx
        jnz .mod
.fin:
        mov ah, 0x3E
        mov bx, [h]
        int 0x21
        mov ax, 0x4C00
        int 0x21

ioctl:
        mov word [rb+2], 0xFFFF
        mov ax, 0x4402
        mov bx, [h]
        mov cx, 14
        mov dx, rb
        int 0x21
        ret

; escribir la cadena ASCIZ de FS:BX (como mucho 64 letras)
putsfs:
        push cx
        mov cx, 64
.l:
        mov al, [fs:bx]
        test al, al
        jz .e
        call putc
        inc bx
        loop .l
.e:
        pop cx
        ret
puts:
        mov ah, 9
        int 0x21
        ret
putc:
        push dx
        push ax
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        pop dx
        ret
crlf:
        mov al, 13
        call putc
        mov al, 10
        jmp putc
hex32:
        push eax
        shr eax, 16
        call hex16
        pop eax
hex16:
        push ax
        mov al, ah
        call hex8
        pop ax
hex8:
        push ax
        shr al, 4
        call hex4
        pop ax
hex4:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

%ifdef DEVPATH
nombre  db "\DEV\PROTMAN$", 0
%else
nombre  db "PROTMAN$", 0
%endif
m_aberr db "ABRIR=ERR $"
m_abok  db "ABRIR=OK$"
m_info  db "INFO=$"
m_ver   db " VER=$"
m_img   db " IMAGEN=$"
m_enl   db "ENLACE=$"
m_ds    db " DS=$"
m_mod   db "MODULO $"
m_kw    db "  CLAVE $"
m_dev   db "DISP $"
h       dw 0
img     dd 0
rb      times 14 db 0
