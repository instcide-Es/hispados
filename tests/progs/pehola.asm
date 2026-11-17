; PEHOLA.EXE - un programa Win32 de consola, con la cabecera PE escrita a mano
; (para probar HW32, el cargador de Hispa-486, sin compiladores de fuera).
; Escribe «Hola desde Win32» y la línea de órdenes, y acaba con el código 42.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;   nasm -f bin -o PEHOLA.EXE pehola.asm
        bits 32
BASE    equ 0x400000
SECALIN equ 0x1000                  ; alineamiento en memoria
ARCHALIN equ 0x200                  ; y en el archivo
%define RVA(x) ((x) - $$ - CAB_TAM + SECALIN)

; ---- cabecera MZ (lo mínimo) ----
mz:     db 'MZ'
        times 0x3C - ($ - mz) db 0
        dd pe - mz                  ; e_lfanew
; ---- cabecera PE ----
pe:     db 'PE', 0, 0
        dw 0x014C                   ; i386
        dw 1                        ; una sección
        dd 0                        ; fecha
        dd 0, 0                     ; símbolos
        dw opc_fin - opc            ; tamaño de la cabecera opcional
        dw 0x0103                   ; ejecutable, sin reubicados, 32 bits
opc:    dw 0x010B                   ; PE32
        db 1, 0                     ; versión del enlazador
        dd TEXTO_TAM                ; código
        dd 0, 0
        dd SECALIN + (inicio - texto) ; punto de entrada (RVA)
        dd SECALIN                  ; base del código
        dd SECALIN                  ; base de los datos
        dd BASE                     ; ImageBase
        dd SECALIN, ARCHALIN
        dw 4, 0, 0, 0, 4, 0         ; versiones (SO, imagen, subsistema)
        dd 0
        dd SECALIN + ((TEXTO_TAM + SECALIN - 1) / SECALIN) * SECALIN  ; SizeOfImage
        dd CAB_TAM                  ; SizeOfHeaders
        dd 0                        ; suma
        dw 3                        ; subsistema: consola
        dw 0
        dd 0x100000, 0x1000         ; pila: reservada, comprometida
        dd 0x100000, 0x1000         ; montón
        dd 0
        dd 16                       ; directorios
        dd 0, 0                     ; 0 exportaciones
        dd SECALIN + (imports - texto), imp_fin - imports   ; 1 importaciones
        times 14 dd 0, 0
opc_fin:
; ---- la sección ----
        db '.text', 0, 0, 0
        dd TEXTO_TAM                ; tamaño en memoria
        dd SECALIN                  ; RVA
        dd ARCH_TAM                 ; tamaño en el archivo
        dd CAB_TAM                  ; dónde está en el archivo
        dd 0, 0
        dw 0, 0
        dd 0xE0000060               ; código, datos, leer, escribir, ejecutar
        times ARCHALIN - ($ - mz) db 0
CAB_TAM equ $ - mz

; ================= la sección .text (RVA 0x1000) =================
texto:
%define A(x) (BASE + SECALIN + ((x) - texto))
inicio:
        push -11                    ; STD_OUTPUT_HANDLE
        call [A(iat_getstd)]
        mov ebx, eax
        push 0
        push A(escritos)
        push msg_tam
        push A(msg)
        push ebx
        call [A(iat_write)]
        ; la línea de órdenes
        call [A(iat_cmdline)]
        mov esi, eax
        xor ecx, ecx
.l:     cmp byte [esi + ecx], 0
        je .ll
        inc ecx
        jmp .l
.ll:
        push 0
        push A(escritos)
        push ecx
        push esi
        push ebx
        call [A(iat_write)]
        push 0
        push A(escritos)
        push 2
        push A(nl)
        push ebx
        call [A(iat_write)]
        push 42
        call [A(iat_exit)]
        hlt

msg     db 'Hola desde Win32', 13, 10, 'Linea: '
msg_tam equ $ - msg
nl      db 13, 10
escritos dd 0

; ---- las importaciones ----
        align 4
imports:
        dd A(ilt) - BASE, 0, 0, A(dllname) - BASE, A(iat) - BASE
        dd 0, 0, 0, 0, 0
imp_fin:
ilt:    dd A(h_getstd) - BASE, A(h_write) - BASE, A(h_cmdline) - BASE, A(h_exit) - BASE, 0
iat:
iat_getstd  dd A(h_getstd) - BASE
iat_write   dd A(h_write) - BASE
iat_cmdline dd A(h_cmdline) - BASE
iat_exit    dd A(h_exit) - BASE
        dd 0
dllname db 'KERNEL32.dll', 0
        align 2
h_getstd  dw 0
        db 'GetStdHandle', 0
        align 2
h_write   dw 0
        db 'WriteFile', 0
        align 2
h_cmdline dw 0
        db 'GetCommandLineA', 0
        align 2
h_exit    dw 0
        db 'ExitProcess', 0
TEXTO_TAM equ $ - texto
        times (ARCHALIN - (($ - texto) % ARCHALIN)) % ARCHALIN db 0
ARCH_TAM equ $ - texto
