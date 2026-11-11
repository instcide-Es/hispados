; DRVLIST.COM - lista las unidades A:-H: con su DPB (INT 21h 32h)
        cpu 386
        bits 16
        org 0x100
        mov byte [drv], 1
.l:
        mov al, [drv]
        add al, 'A' - 1
        mov [txt], al
        mov dl, [drv]
        mov ah, 0x32
        int 0x21
        push cs
        pop ds
        mov si, txt_no
        cmp al, 0
        jne .p
        mov si, txt_ok
.p:
        mov dx, txt
        mov ah, 9
        int 0x21
        mov dx, si
        mov ah, 9
        int 0x21
        inc byte [drv]
        cmp byte [drv], 9
        jb .l
        mov ax, 0x4C00
        int 0x21
drv     db 0
txt     db "?: $"
txt_ok  db "ok", 13, 10, "$"
txt_no  db "no", 13, 10, "$"
