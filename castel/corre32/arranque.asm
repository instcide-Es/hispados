; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; arranque.asm - el trocito de 32 bits de corre32 (ver corre32.c): prepara en
; Linux la memoria de un programa de usuario de Hispa-Pentium (la zona del
; programa desde 0x40000000 y la pila, que acaba en 0xC0000000) y se para con
; «int3»; corre32 pone ahí el programa y le da el procesador.
        bits 32
        global _start
_start:
        ; mmap(0x40000000, 1,5 GB, leer+escribir+ejecutar, privada+anónima+fija+sin reservar)
        mov eax, 192
        mov ebx, 0x40000000
        mov ecx, 0x60000000
        mov edx, 7
        mov esi, 0x4032
        mov edi, -1
        xor ebp, ebp
        int 0x80
        cmp eax, 0x40000000
        jne .mal
        ; la pila: de 0xBFC00000 a 0xC0000000 (4 MB, como en Hispa-Pentium)
        mov eax, 192
        mov ebx, 0xBFC00000
        mov ecx, 0x400000
        mov edx, 3
        mov esi, 0x4032
        mov edi, -1
        xor ebp, ebp
        int 0x80
        cmp eax, 0xBFC00000
        jne .mal
        int3
.mal:
        mov eax, 1
        mov ebx, 99
        int 0x80
