; formas64.asm - [CASTEL64] una muestra de cada forma de instrucción de 64 bits
; que entienden ensambla.py y ENSAMBLA (pruebas64.sh las compara con NASM,
; octeto a octeto): lo que saca el compilador para «modelo linux64» y
; «núcleo64» (CASTEL64.md) y lo corriente de un bloque «ensamblador».
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
cpu x64
bits 32
org 0x100000
; lo de 32 bits que va delante del modo largo
inicio:
mov [dato], eax
mov edi, 0x70000
mov dword [0x71000], 0x72003
mov [edi+4], edx
adc edx, 0
mov eax, cr4
mov cr4, eax
mov cr3, eax
rdmsr
wrmsr
lgdt [dato]
x0: loop x0
jmp 0x08:largo
bits 64
largo:
; los registros de 64 bits y los nuevos
mov rax, rbx
mov rbx, rax
mov r8, rax
mov rax, r8
mov r15, r9
mov r10d, eax
mov eax, r11d
mov ax, r12w
mov al, r13b
mov r14b, cl
add rax, rcx
sub rcx, rax
and rdx, rsi
or rdi, rbx
xor r8, r9
cmp rcx, rax
cmp rax, rcx
test rax, rax
test al, al
neg rax
not rax
inc rax
dec rax
inc eax
dec ecx
inc qword [rbx]
dec dword [rbp-8]
imul rax, rcx
imul rax, rax, 10
imul rax, rax, 1000
imul rsi, rsi, 12
imul rax, [rbp-16]
cqo
idiv rcx
div rcx
cdq
idiv ecx
cdqe
cwde
shl rax, 4
shl rax, 1
shr rax, 63
sar rax, cl
shl rcx, 4
shl eax, 4
; los inmediatos
add rax, 5
add rax, 1000
add rax, -1
add rax, -200
sub rax, 70000
and rax, 7
add eax, 5
cmp rax, 0
cmp rax, -1
cmp rax, 100000
cmp qword [rbp-8], 0
cmp qword [rbp-8], 1000
cmp dword [rbp-8], 5
cmp byte [rbx+rcx], 0
test qword [rbp-8], 1
mov qword [rbp-8], 0
mov qword [rbp-8], -1
mov qword [rbp-8], 1000
mov dword [rbp-8], 1000
mov word [rbp-8], 3
mov byte [rbp-8], 3
add qword [rbp-8], 8
sub qword [rbp-8], 1000
; mov de números a registros: como NASM
mov rax, 5
mov rax, 0xFFFFFFFF
mov rax, -1
mov rax, -2147483648
mov rax, 0x123456789
mov rcx, -9223372036854775807
mov r9, 7
mov r9, -7
mov eax, 5
mov r8d, 5
mov ecx, 4294967295
mov rax, dato
mov ebx, dato
; la memoria
mov rax, [rbp+16]
mov rax, [rbp-8]
mov rax, [rbx]
mov rax, [rbx+8]
mov rax, [rbx+rsi*8]
mov rax, [rbx+rsi*8+16]
mov eax, [rbx+rsi*4]
mov eax, [rsi*4+dato]
mov eax, [rsi*2+8]
mov ax, [rdx*2+0xB8000]
mov [rdx*2+0xB8000], ax
mov rax, [rsp]
mov rax, [rsp+8]
mov rax, [r12]
mov rax, [r13]
mov rax, [r13+8]
mov rax, [rbx+r12*2]
mov rax, [r8+r9*4+100]
mov rax, [dato]
mov [dato], rax
mov eax, [dato]
mov [dato], eax
mov al, [dato]
mov [dato+3], al
mov rax, [dato+rsi*8]
mov [rbp-8], rax
mov [rbp-8], eax
mov [rbp-8], ax
mov [rbp-8], al
mov [rbx+rsi], al
lea rax, [rbp-8]
lea rbx, [dato+16]
lea rax, [r8+r8*2]
lea rbx, [rbx+rsi*4]
movzx eax, byte [rbp-8]
movzx eax, word [rbx+rsi*2]
movzx esi, byte [dato]
movsx eax, word [rbp-8]
movsx rsi, word [rbp-8]
movsx rcx, word [dato]
movsxd rsi, dword [rbp-8]
movsxd rcx, dword [dato]
; la pila
push rax
push rbx
push rbp
push r8
push r15
pop rax
pop rcx
pop rbp
pop r9
pop r15
push 5
push 200
push -1
push dato
push qword [rbp-8]
push qword [dato]
pop qword [rbp-8]
; las llamadas, los saltos y la vuelta
call largo
call rax
call [dato]
jmp rax
jmp r8
jmp [dato]
x1: jmp x1
x2: jz x2
x3: jne strict near x3
ret
ret 16
iretq
syscall
cpuid
rdtsc
; las sueltas y las de cadenas
cld
cli
hlt
lodsb
stosb
movsb
rep stosb
rep movsd
rep stosd
stosq
movsq
lodsq
rep movsq
; los puertos y los registros de segmento
in al, dx
out dx, al
mov ax, 0x10
mov ds, ax
mov ss, ax
mov ax, ss
mov cr3, rax
mov rax, cr2
xchg rax, rcx
xchg rcx, rax
xchg r8, rax
; los datos
dato:
dq 0
dq 18446744073709551615
dq dato
dq -1, 5
dd 0x100000000-0xE85250D6
dw 2, 0
db 'Hola', 0
align 8, db 0
dq 0, 0
