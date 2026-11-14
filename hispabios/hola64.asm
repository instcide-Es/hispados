; HispaBIOS - paso 1: aplicación UEFI mínima (x64) que escribe un letrero
; con el servicio de texto del firmware y espera unos segundos.
%define PE_ARQ 64
%include "pe.inc"

; desplazamientos en las tablas de UEFI (x64)
ST_CONOUT       equ 64          ; EFI_SYSTEM_TABLE.ConOut
ST_BOOTSERV     equ 96          ; EFI_SYSTEM_TABLE.BootServices
TXT_OUTSTR      equ 8           ; SIMPLE_TEXT_OUTPUT.OutputString
BS_STALL        equ 248         ; BOOT_SERVICES.Stall

pe_entrada:
        push rbx
        sub rsp, 32                     ; espacio de la convención de llamada
        mov rbx, rdx                    ; RBX = tabla del sistema
        mov rcx, [rbx + ST_CONOUT]
        lea rdx, [rel letrero]
        call [rcx + TXT_OUTSTR]
        mov rax, [rbx + ST_BOOTSERV]
        mov ecx, 60000000               ; 60 s (para poder verlo)
        call [rax + BS_STALL]
        xor eax, eax                    ; EFI_SUCCESS
        add rsp, 32
        pop rbx
        ret

letrero:
        dw __utf16__(`HispaBIOS 0.1 (x64): el firmware UEFI la ha cargado.\r\n`), 0

PE_FIN
