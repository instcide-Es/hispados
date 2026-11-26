; =============================================================================
;  FIRMATST.COM - prueba de las cuentas de ACTUALIZAR en el equipo (modo real o
;  V86): utils/firma/cripto16.c, metido como en ACTUALIZAR (incbin en 0x120,
;  llamado con «call dword»). Dice «SHA-256 abc: BIEN|MAL», «firma RFC 8032:
;  BIEN|MAL» y «firma cambiada: BIEN|MAL» (tests/run_firma_tests.sh).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        cpu 386
        bits 16
        org 0x100
        jmp inicio
        times 0x20 - ($ - $$) db 0
cripto16:
        incbin "cripto16.bin"
%include "cripto16.inc"
inicio:
        cld
        mov ax, cs
        mov ds, ax
        mov es, ax
        ; SHA-256 de «abc»
        movzx esp, sp
        call dword CR_SHA_INI
        push dword 3
        push dword abc
        call dword CR_SHA_MAS
        add sp, 8
        push dword resumen
        call dword CR_SHA_FIN
        add sp, 4
        mov si, resumen
        mov di, sha_abc
        mov cx, 32
        repe cmpsb
        mov dx, m_sha
        call di_bien
        ; la firma de la RFC 8032 (prueba 2: el mensaje es 0x72), con la
        ; etiqueta vacía; y luego con la firma cambiada
        call firma
        cmp eax, 1
        mov dx, m_fir
        call di_bien
        xor byte [firma_v + 5], 1
        call firma
        cmp eax, 0
        mov dx, m_mal
        call di_bien
        mov ax, 0x4C00
        int 0x21
firma:
        mov [sv], sp
        mov sp, pila_c + 4096
        movzx esp, sp
        push dword clave
        push dword 1
        push dword msj
        push dword 0
        push dword msj
        push dword firma_v
        call dword CR_FIRMA
        mov sp, [sv]
        ret
; ZF = 1 -> «BIEN»; DX = el texto
di_bien:
        pushf
        mov ah, 9
        int 0x21
        popf
        mov dx, m_bien
        je .e
        mov dx, m_malo
.e:
        mov ah, 9
        int 0x21
        ret
abc     db "abc"
msj     db 0x72
sha_abc:
        db 0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23
        db 0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD
clave:
        db 0x3D, 0x40, 0x17, 0xC3, 0xE8, 0x43, 0x89, 0x5A, 0x92, 0xB7, 0x0A, 0xA7, 0x4D, 0x1B, 0x7E, 0xBC
        db 0x9C, 0x98, 0x2C, 0xCF, 0x2E, 0xC4, 0x96, 0x8C, 0xC0, 0xCD, 0x55, 0xF1, 0x2A, 0xF4, 0x66, 0x0C
firma_v:
        db 0x92, 0xA0, 0x09, 0xA9, 0xF0, 0xD4, 0xCA, 0xB8, 0x72, 0x0E, 0x82, 0x0B, 0x5F, 0x64, 0x25, 0x40
        db 0xA2, 0xB2, 0x7B, 0x54, 0x16, 0x50, 0x3F, 0x8F, 0xB3, 0x76, 0x22, 0x23, 0xEB, 0xDB, 0x69, 0xDA
        db 0x08, 0x5A, 0xC1, 0xE4, 0x3E, 0x15, 0x99, 0x6E, 0x45, 0x8F, 0x36, 0x13, 0xD0, 0xF1, 0x1D, 0x8C
        db 0x38, 0x7B, 0x2E, 0xAE, 0xB4, 0x30, 0x2A, 0xEE, 0xB0, 0x0D, 0x29, 0x16, 0x12, 0xBB, 0x0C, 0x00
m_sha   db "SHA-256 abc: $"
m_fir   db "firma RFC 8032: $"
m_mal   db "firma cambiada: $"
m_bien  db "BIEN", 13, 10, "$"
m_malo  db "MAL", 13, 10, "$"
        section .bss
resumen resb 32
sv      resw 1
pila_c  resb 4096
