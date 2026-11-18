; formas32.asm - una muestra de cada forma de instrucción de 32 bits que entienden
; ensambla.py y ENSAMBLA (pruebas.sh las compara con NASM, octeto a octeto).
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
cpu 386
bits 32
org 0x100000
mov word [es:ebx], 1
mov ax, [es:ebx]
mov eax, [ebx+esi]
mov eax, [esi+ebx]
mov eax, [esi+ebp]
mov eax, [ebp+esi]
mov eax, [esp+esi]
mov eax, [esi+esp]
mov eax, [esi*2]
mov eax, [esi*2+8]
mov eax, [esi*4+0x1000]
mov eax, [ebp]
mov eax, [esp]
mov eax, [esp+4]
mov eax, [0x1000]
mov [0x1000], ax
push 5
push 200
push -1
push dword 5
imul eax, 10
imul eax, eax, 1000
shl eax, 1
shl eax, 4
x1: jecxz x1
x2: loop x2
iret
iretd
pushf
cwd
cdq
cbw
cwde
lodsw
lodsd
movsd
in ax, dx
in eax, dx
out dx, ax
movzx eax, byte [ebx]
movzx eax, ax
movsx eax, word [ebp-4]
movzx ax, byte [ebx]
test eax, 1
test byte [eax], 1
lgdt [ebx]
lidt [0x2000]
jmp 0x08:0x1234
mov ds, ax
mov ds, eax
mov eax, cr0
mov cr0, eax
mov fs, ax
ret 8
mov ax, ds
mov eax, ds
mov [ebx], ds
push fs
pop gs
x4: jcxz x4
pusha
popa
pushad
xchg eax, ebx
xchg ebx, eax
xchg ax, bx
inc ax
inc eax
push ax
pop esi
mov al, [0x10]
mov ax, 5
mov eax, -1
add eax, 200
add eax, 100
add ax, 100
add ax, 200
cmp byte [ebx], 5
sub dword [ebp-8], 1
and eax, 0xFFFF
mov dword [ebx+4], 7
call eax
call [ebx]
jmp dword [ebx+eax*4]
push dword [ebp+8]
push word [ebp+8]
pop dword [ebx]
lea eax, [ebp-8]
lea esi, [ebx+esi*2]
movzx ecx, byte [G]
rep stosd
rep movsb
in al, 0x60
out 0x20, al
out 0xf4, al
mov eax, [ebx+esi*8+4]
mov eax, [esi*8+ebx]
mov eax, [4+ebx]
mov eax, [ebx-4]
mov eax, [ebx+esi*1]
int3
int 0x30
std
push 4294967291
add eax, 4294967291
cmp eax, 4294967295
imul eax, eax, 4294967295
mov dword [ebx], 4294967295
and eax, 4294967040
push -5
adelante:
    jmp lejos
    je lejos
    jne cerca
    jmp short cerca
cerca:
    call lejos
    imul ecx, [ebp+8]
    imul edx, eax, 80
    imul esi, esi, 12
    imul eax, ecx
    shr eax, 3
    sar dword [ebx], 1
    shl word [esi+4], cl
    mov [edx*2+0xB8000], ax
    mov [esi+G], eax
    mov eax, [esi*4+G]
    mov ecx, [ebp+esi*4-20]
    movzx eax, byte [ebx+esi]
    movsx ecx, byte [ebp-1]
    movsx eax, ax
    neg eax
    not eax
    idiv ecx
    div dword [G]
    mul ecx
    cdq
    xor edx, edx
    test al, al
    test eax, eax
    cmp ecx, eax
    or al, al
    and eax, 255
    lea ebx, [G+8]
    lea ebx, [esi*2+G]
    mov ebx, G+4
    push G
    push dword [G]
    pop eax
    pushad
    popad
    iretd
    sgdt [G]
    sidt [ebx]
    mov cr3, eax
    mov ebx, cr2
    push es
    pop ds
    jmp 0x08:cerca
    call dword [G]
    jmp ebx
    stosb
    lodsb
    in eax, 0x40
    out dx, eax
    times 3 nop
    rep stosb
    align 4, db 0
    jmp adelante
    times 200 db 0
lejos:
    ret
    retf
    ret 12
G: dd 0, 0, 0
    dw G
    dd lejos-adelante
    db 'texto', 0
bits 16
    mov ax, [bx+si]
    mov eax, 5
    push eax
    inc eax
    cwde
