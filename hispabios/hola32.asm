; HispaBIOS - paso 1: aplicación UEFI mínima (IA32) que escribe un letrero
; con el servicio de texto del firmware y espera.
%define PE_ARQ 32
%include "pe.inc"

; desplazamientos en las tablas de UEFI (IA32: punteros de 4 octetos)
ST_CONOUT       equ 44          ; EFI_SYSTEM_TABLE.ConOut
ST_BOOTSERV     equ 60          ; EFI_SYSTEM_TABLE.BootServices
TXT_OUTSTR      equ 4           ; SIMPLE_TEXT_OUTPUT.OutputString
BS_STALL        equ 136         ; BOOT_SERVICES.Stall

pe_entrada:
        push ebp
        mov ebp, esp
        push ebx
        push esi
        call .aqui                      ; ESI = dirección real de .aqui
.aqui:  pop esi
        mov ebx, [ebp + 12]             ; EBX = tabla del sistema
        lea eax, [esi + letrero - .aqui]
        push eax
        mov eax, [ebx + ST_CONOUT]
        push eax
        call [eax + TXT_OUTSTR]
        add esp, 8
        push dword 60000000             ; 60 s (para poder verlo)
        mov eax, [ebx + ST_BOOTSERV]
        call [eax + BS_STALL]
        add esp, 4
        xor eax, eax                    ; EFI_SUCCESS
        pop esi
        pop ebx
        pop ebp
        ret

letrero:
        dw __utf16__(`HispaBIOS 0.1 (IA32): el firmware UEFI la ha cargado.\r\n`), 0

PE_FIN
