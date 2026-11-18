; formas.asm - las formas de instrucción que entiende el ensamblador de
; Castel (ensambla.py y ensambla.cas), para compararlas con NASM: pruebas.sh
; lo ensambla con los dos y el archivo tiene que salir igual.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
cpu 8086
bits 16
org 0x100
inicio:
    mov ax, bx
    mov al, ch
    MOV AX, [dato]
    mov [dato], ax
    mov al, [dato]
    mov [dato], al
    mov bx, [dato]
    mov [dato], cx
    mov cl, [dato+1]
    mov [dato+1], dh
    mov ax, [bx]
    mov ax, [bx+si]
    mov ax, [bx+di+4]
    mov ax, [bp+si-2]
    mov ax, [bp+di+300]
    mov ax, [si]
    mov ax, [di-128]
    mov ax, [di-129]
    mov ax, [bp]
    mov ax, [bp+127]
    mov ax, [bp+128]
    mov ax, [bx+dato]
    mov ax, [bx+dato+6]
    mov ax, [si+bx]
    mov ax, [es:bx]
    mov al, [es:123]
    mov ax, [es:0x46C]
    mov [es:bx], al
    mov [es:di+2], ax
    mov word [es:10], 7
    mov byte [es:bx], 65
    mov ax, 1234
    mov ax, dato
    mov cx, fin-inicio
    mov bl, 200
    mov dh, 'A'
    mov dl, '-'
    mov word [dato], 5
    mov byte [dato], 5
    mov word [bp-2], 0xFFFF
    mov byte [bx+3], 0x80
    mov word [bx], dato
    mov es, ax
    mov ds, [dato]
    mov ax, ss
    mov [dato], es
    mov ax, 0FFh
    add ax, 5
    add ax, 127
    add ax, 128
    add ax, 0xFF80
    add ax, 0xFF7F
    add ax, 65535
    add ax, -1
    add ax, dato
    add al, 5
    add bx, 5
    add bx, 500
    add bl, 5
    add ax, bx
    add al, bl
    add ax, [dato]
    add [dato], ax
    add [bx], cl
    add cl, [bx]
    add word [dato], 1
    add word [dato], 1000
    add byte [dato], 1
    add word [bp-4], -3
    or ax, dx
    or al, al
    or ax, ax
    or word [dato], 0x8000
    adc dx, 0
    adc word [dato+2], 0
    sbb dx, [bp-6]
    sbb cx, dx
    and ax, 0xFFFE
    and ax, 15
    and ax, 0x0FF0
    and word [dato], 0xFF
    and byte [dato], 0xFE
    sub sp, 300
    sub sp, 4
    sub ax, cx
    xor ax, ax
    xor ah, ah
    xor al, 1
    xor word [dato], 0x55AA
    cmp ax, 0
    cmp ax, 300
    cmp al, 13
    cmp byte [bx], 32
    cmp word [bp+4], 10
    cmp word [dato], 0x1234
    cmp [dato], ax
    cmp ax, [bp-2]
    cmp cx, ax
    cmp di, cx
    cmp al, [dato]
    cmp [dato], al
    cmp dx, 0
    cmp bx, 0xFFFF
    test byte [dato], 1
    test word [dato], 0x100
    test ax, 1
    test al, 0x80
    test bx, 4
    test bl, 4
    test ax, bx
    test [dato], cx
    test cl, [dato]
    inc ax
    inc si
    dec di
    inc byte [dato]
    inc word [bp-2]
    dec word [dato]
    dec byte [bx+si]
    inc al
    dec ch
    neg ax
    neg dx
    not ax
    not word [dato]
    neg byte [dato]
    mul cx
    mul bx
    mul word [dato]
    mul byte [dato]
    imul cx
    div cx
    div bx
    div byte [dato]
    idiv cx
    idiv word [bp-2]
    shl ax, 1
    shl ax, cl
    shr ax, 1
    shr dx, cl
    sar ax, 1
    sar ax, cl
    rcl dx, 1
    rcr ax, 1
    rol al, 1
    ror byte [dato], cl
    sal bx, 1
    shl word [dato], 1
    push ax
    push bp
    push ds
    push es
    push cs
    push ss
    push word [dato]
    push word [bp+4]
    push word [bx]
    pop ax
    pop bp
    pop ds
    pop es
    pop ss
    pop word [dato]
    pop word [R_ban]
    pushf
    popf
    xchg ax, cx
    xchg cx, ax
    xchg ax, si
    xchg bx, cx
    xchg al, ah
    xchg ax, [dato]
    xchg [dato], bx
    xchg bl, [dato]
    lea ax, [bp-10]
    lea bx, [bp+si-20]
    lea ax, [bx+dato]
    lea si, [dato]
    int 0x21
    int 0x10
    int 3
    in al, dx
    in ax, dx
    in al, 0x60
    out dx, al
    out dx, ax
    out 0x20, al
    cld
    std
    cli
    sti
    clc
    stc
    cmc
    cwd
    cbw
    lodsb
    lodsw
    stosb
    stosw
    movsb
    movsw
    cmpsb
    scasb
    rep stosb
    rep movsw
    repe cmpsb
    repne scasb
    nop
    lahf
    sahf
    xlatb
.cerca:
    jmp .cerca
    jmp short .cerca
    jmp strict near .cerca
    je .cerca
    jne .cerca
    jz .lejos
    jnz .lejos
    jb .lejos
    jae .cerca
    jbe .cerca
    ja .lejos
    jl .cerca
    jge .cerca
    jle .cerca
    jg .lejos
    jc .cerca
    jnc .cerca
    jo .cerca
    jno .cerca
    js .cerca
    jns .cerca
    jna .cerca
    jnb .cerca
    jnl .cerca
    jng .cerca
    jp .cerca
    jnp .cerca
    jcxz .aqui
    loop .aqui
.aqui:
    loope .aqui
    loopne .aqui
    call inicio
    call .lejos
    call bx
    call [dato]
    call word [bx+2]
    jmp bx
    jmp [dato]
    jmp word [bx+si]
    jmp 0x1234:0x5678
    jmp ((otra-inicio)>>4):(otra-inicio)
    call 0x10:0x20
    ret
    ret 4
    retf
    retf 6
    iret
    hlt
    times 130 db 0
.lejos:
    jmp .cerca
    jmp inicio
    je inicio
    jmp .mas
    times 127 nop
.mas:
    jmp .mas2
    times 128 nop
.mas2:
    je .mas3
    times 127 nop
.mas3:
    jne .mas4
    times 128 nop
.mas4:
otra:
    ret
    align 16, db 0
tabla:
    db 1, 2, 3, 'a', "hola", 0, -1, 255
    db 'M', 'Z'
    dw 1, 2, 0x1234, otra, otra-inicio, (fin-inicio) % 512, (fin-inicio+511) / 512
    dw (otra-inicio)>>4, (3+4)*2, 10-2-3, 1<<4|1, 0xFF & 0x0F, 6 ^ 3, -5, ~0
    dd 100000, 0x12345678, otra
    dw 'A'
dato:   dw 0
R_ban:  dw 0
fin:
; ---- sin valor inicial ----
_vacio:
    absolute _vacio
G_a: resb 10
G_b: resw 3
G_c: resd 2
_vacio_fin:
