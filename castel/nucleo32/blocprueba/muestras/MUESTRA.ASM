; un programa de NASM
%define LARGO 10
%macro saca 1
    mov dx, %1
%endmacro
section .text
inicio:
    mov eax, 0FFh       ; un número
    mov ebx, [esi+4]
.otra:
    db "hola", 0
    jmp .otra
