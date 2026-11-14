; =============================================================================
;  alcsim.asm - el códec Realtek ALC269 (10EC:0269 rev. 00100100h, el ALC269VB)
;  del Toshiba NB250-10N de Carlos (subsistema 1179:FF6C), simulado a partir
;  de su volcado real (informe SONIDO.TXT de AUDICION, nodo a nodo: capacidades,
;  amplificadores, conexiones, pines y cómo los dejó la BIOS).
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Solo para las pruebas en QEMU (que no tiene ese códec): AUDICION /SIM:ALC269
;  y HSPSND /SIMALC. Se ensambla aparte (build/gen/lib/alcsim.bin, ORG 0) y se
;  mete entero en el programa (incbin); se llama con CALL FAR, así funciona en
;  cualquier segmento (HSPSND lo copia detrás de lo residente). Todo lo suyo lo
;  lee y escribe con CS:; no toca DS ni ES.
;
;  Entradas (CALL FAR a desplazamiento fijo):
;    00h  alc_reset   el códec como lo dejó la BIOS
;    04h  alc_cmd     EAX = orden completa al códec 0 -> EAX = respuesta
;    08h  alc_tsr     (HSPSND, JMP FAR) se copia a ES:DI (CX octetos) y se
;                     queda residente con DX párrafos (INT 21h AX=3100h)
;    0Ch  dd          (lo pone quien lo usa) CALL FAR para mandar una orden
;                     al códec de QEMU (EAX, la respuesta no importa)
;    10h  db          modo: bit 0 = la hipótesis «el DAC vuelve a 0 dB cuando
;                     se le programa el formato o el flujo»; bit 1 = apuntar
;                     por COM1 cada orden de ganancia («ALCSIM xxxxxxxxh»)
;
;  Lo que suena en QEMU: el formato y el flujo de los DAC 02h y 03h se pasan
;  a su DAC 02h; y la ganancia del camino del altavoz (pin 14h, su mezclador
;  elegido 0Ch/0Dh, la entrada 0 de ese mezclador y su DAC 02h/03h) se pasa
;  a su amplificador: la atenuación del DAC (pasos de 0,75 dB, 0 dB en el 87)
;  convertida en dB a su escala (lineal, 0 dB en el paso 74); el silencio del
;  pin, del mezclador o el pin sin salida, en silencio.
;  Como el de verdad (hoja de datos pública del ALC269): el DAC no tiene
;  silencio (el bit 7 se lee 0), los mezcladores 0Ch/0Dh solo silencio, las
;  ganancias por encima del número de pasos se quedan en el máximo.
; =============================================================================
        bits 16
        org 0

        jmp near alc_reset
        nop
        jmp near alc_cmd
        nop
        jmp near alc_tsr            ; 08h
        nop
alc_fwd dd 0                        ; 0Ch
alc_modo db 0                       ; 10h
        db 0, 0, 0

; ---- la tabla (del volcado) ----
%macro AROW 29
        dd %1, %2, %3, %4, %5, %6, %7
        db %8
        db %9, %10, %11, %12, %13, %14, %15, %16
        db %17, %18, %19, %20
        db %21, %22, %23, %24, %25, %26, %27, %28
        dw %29
        db 0
%endmacro
R_CAPS      equ 0
R_PINC      equ 4
R_IAMP      equ 8
R_OAMP      equ 12
R_PCM       equ 16
R_FMTS      equ 20
R_CFG       equ 24
R_NC        equ 28
R_CONN      equ 29
R_CTL       equ 37
R_EAPD      equ 38
R_AOL       equ 39
R_AOR       equ 40
R_AI        equ 41
R_FMT       equ 49
ROW         equ 52
; estado por nodo
S_CTL       equ 0
S_EAPD      equ 1
S_SEL       equ 2
S_AOL       equ 3
S_AOR       equ 4
S_AI        equ 5                   ; 8 entradas x (izquierda, derecha)
S_PWR       equ 21
S_FMT       equ 22                  ; (palabra)
S_STR       equ 24
S_UNS       equ 25
S_DIG       equ 26                  ; (palabra)
SSZ         equ 28

ALC_N0      equ 0x02
ALC_NN      equ 34
alc_rows:
;         caps        pin         amp-ent     amp-sal     PCM         formatos    config      n  conexiones (8)                                  ctl   eapd  aoI   aoD   entradas (8)                                    formato
        AROW 0x0000001D, 0x00000000, 0x00000000, 0x00025757, 0x000E0560, 0x00000001, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0031 ; 02h
        AROW 0x0000001D, 0x00000000, 0x00000000, 0x00025757, 0x000E0560, 0x00000001, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0031 ; 03h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 04h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 05h
        AROW 0x00000211, 0x00000000, 0x00000000, 0x00000000, 0x000E05E0, 0x00000001, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0031 ; 06h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 07h
        AROW 0x0010011B, 0x00000000, 0x80051F0B, 0x00000000, 0x000E0560, 0x00000001, 0x00000000, 1, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0031 ; 08h
        AROW 0x0010011B, 0x00000000, 0x80051F0B, 0x00000000, 0x000E0560, 0x00000001, 0x00000000, 1, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0031 ; 09h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 0Ah
        AROW 0x0020010B, 0x00000000, 0x80051F17, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 5, 0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x97, 0x97, 0x97, 0x15, 0x00, 0x00, 0x00, 0x0000 ; 0Bh
        AROW 0x0020010B, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 2, 0x02, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 0Ch
        AROW 0x0020010B, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 2, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 0Dh
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 0Eh
        AROW 0x0020010A, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 2, 0x02, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 0Fh
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 10h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 11h
        AROW 0x0040000B, 0x00000020, 0x002F0300, 0x00000000, 0x00000000, 0x00000000, 0x411111F0, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 12h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 13h
        AROW 0x0040018D, 0x00010014, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x99131110, 2, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 14h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 15h
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 16h
        AROW 0x0040010C, 0x00000010, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x411111F0, 1, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 17h
        AROW 0x0040018F, 0x00001734, 0x002F0300, 0x80000000, 0x00000000, 0x00000000, 0x03A11C20, 1, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 18h
        AROW 0x0040008B, 0x00001724, 0x002F0300, 0x00000000, 0x00000000, 0x00000000, 0x99A3192F, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 19h
        AROW 0x0040018F, 0x0000003C, 0x002F0300, 0x80000000, 0x00000000, 0x00000000, 0x411111F0, 2, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Ah
        AROW 0x0040018F, 0x00000034, 0x002F0300, 0x80000000, 0x00000000, 0x00000000, 0x411111F0, 2, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Bh
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Ch
        AROW 0x00400000, 0x00000020, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x4017992D, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Dh
        AROW 0x00400381, 0x00000014, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x411111F0, 1, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Eh
        AROW 0x00F00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 1Fh
        AROW 0x00F00040, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 20h
        AROW 0x0040018D, 0x0000001C, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x0321141F, 2, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 21h
        AROW 0x0030010B, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 7, 0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x0B, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000 ; 22h
        AROW 0x0020010B, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 6, 0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x0000 ; 23h
%if ($ - alc_rows) != ALC_NN * ROW
%error "la tabla del ALC269 no cuadra"
%endif
; atenuación (en pasos de 0,75 dB) -> paso del amplificador de QEMU (lineal: 74 = 0 dB)
alc_qtab    db 74, 68, 62, 57, 52, 48, 44, 40, 37, 34, 31, 29, 26, 24, 22, 20, 19, 17, 16, 14, 13, 12
            db 11, 10, 9, 9, 8, 7, 7, 6, 6, 5, 5, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2
            db 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0
            db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
; los coeficientes del widget de proceso (20h), como los dejó la BIOS
alc_coef0   dw 0x2016, 0xAD6B, 0xAAB8, 0x4001, 0x9827, 0x0080, 0x8000, 0x00C0
            dw 0x0310, 0x0000, 0xBBCC, 0xBBCC, 0x0210, 0x4040, 0x8814, 0xB60B
            dw 0x0000, 0x0710, 0x1901, 0xC332, 0x143A, 0x258D, 0x0234, 0x002F
            dw 0x0000
ALC_NCOEF   equ 32

; ---- el estado ----
alc_st      times ALC_NN * SSZ db 0
alc_coef    times ALC_NCOEF dw 0
alc_cidx    dw 0
alc_proc    db 0
alc_gd      db 0                    ; GPIO: datos, máscara, dirección
alc_gm      db 0
alc_gdir    db 0
alc_afgp    dw 0
alc_qL      db 0                    ; lo último que se mandó a QEMU (FFh: nada)
alc_qR      db 0

; =============================================================================
alc_tsr:
        push cs
        pop ds
        xor si, si
        cld
        rep movsb
        sti
        mov ax, 0x3100
        int 0x21

alc_reset:
        pushad
        mov si, alc_rows
        mov di, alc_st
        mov cx, ALC_NN
.n:
        mov al, [cs:si+R_CTL]
        mov [cs:di+S_CTL], al
        mov al, [cs:si+R_EAPD]
        mov [cs:di+S_EAPD], al
        mov byte [cs:di+S_SEL], 0
        mov al, [cs:si+R_AOL]
        mov [cs:di+S_AOL], al
        mov al, [cs:si+R_AOR]
        mov [cs:di+S_AOR], al
        xor bx, bx
.ai:
        mov al, [cs:si+R_AI+bx]
        push bx
        shl bx, 1
        mov [cs:di+S_AI+bx], al
        mov [cs:di+S_AI+bx+1], al
        pop bx
        inc bx
        cmp bx, 8
        jb .ai
        mov byte [cs:di+S_PWR], 0
        mov ax, [cs:si+R_FMT]
        mov [cs:di+S_FMT], ax
        mov byte [cs:di+S_STR], 0
        mov byte [cs:di+S_UNS], 0
        mov word [cs:di+S_DIG], 0
        add si, ROW
        add di, SSZ
        loop .n
        xor bx, bx
.c:
        xor ax, ax
        cmp bx, 25 * 2
        jae .c2
        mov ax, [cs:alc_coef0+bx]
.c2:
        mov [cs:alc_coef+bx], ax
        add bx, 2
        cmp bx, ALC_NCOEF * 2
        jb .c
        mov word [cs:alc_cidx], 0
        mov byte [cs:alc_proc], 0
        mov byte [cs:alc_gd], 0
        mov byte [cs:alc_gm], 0
        mov byte [cs:alc_gdir], 0
        mov word [cs:alc_afgp], 0x400
        mov byte [cs:alc_qL], 0xFF
        mov byte [cs:alc_qR], 0xFF
        popad
        retf

; =============================================================================
; EAX = orden completa (códec 0) -> EAX = respuesta. Conserva lo demás.
alc_cmd:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx, eax                ; ECX = la orden
        mov edx, eax
        shr edx, 20
        and dx, 0xFF                ; DL = nodo
        ; SI -> fila, DI -> estado (nodos 02h-23h), o 0
        xor si, si
        xor di, di
        cmp dl, ALC_N0
        jb .nr
        cmp dl, ALC_N0 + ALC_NN - 1
        ja .nr
        movzx si, dl
        sub si, ALC_N0
        mov di, si
        imul si, si, ROW
        add si, alc_rows
        imul di, di, SSZ
        add di, alc_st
.nr:
        mov eax, ecx
        shr eax, 8
        and ax, 0xFFF               ; verbo de 12 bits
        mov bx, ax
        and bh, 0x0F
        cmp bh, 0x07
        je .v12
        cmp bh, 0x0F
        je .v12
        ; verbos de 4 bits (dato de 16 bits en CX)
        mov eax, ecx
        shr eax, 16
        and al, 0x0F
        cmp al, 0x02
        je .sfmt
        cmp al, 0x0A
        je .gfmt
        cmp al, 0x03
        je .samp
        cmp al, 0x0B
        je .gamp
        cmp al, 0x05
        je .sidx
        cmp al, 0x0D
        je .gidx
        cmp al, 0x04
        je .scoef
        cmp al, 0x0C
        je .gcoef
        jmp .z
.v12:
        cmp ax, 0xF00
        je .param
        cmp ax, 0xF20
        je .ssid
        cmp ax, 0x7FF
        je .freset
        cmp ax, 0xF15
        je .ggd
        cmp ax, 0xF16
        je .ggm
        cmp ax, 0xF17
        je .ggdir
        cmp ax, 0x715
        je .sgd
        cmp ax, 0x716
        je .sgm
        cmp ax, 0x717
        je .sgdir
        cmp ax, 0xF03
        je .gproc
        cmp ax, 0x703
        je .sproc
        cmp ax, 0xF05
        je .gpwr
        cmp ax, 0x705
        je .spwr
        test di, di
        jz .z
        cmp ax, 0xF02
        je .conn
        cmp ax, 0xF1C
        je .cfg
        cmp ax, 0xF09
        je .z                       ; nada enchufado
        cmp ax, 0xF01
        je .gsel
        cmp ax, 0x701
        je .ssel
        cmp ax, 0xF07
        je .gctl
        cmp ax, 0x707
        je .sctl
        cmp ax, 0xF0C
        je .geapd
        cmp ax, 0x70C
        je .seapd
        cmp ax, 0xF06
        je .gstr
        cmp ax, 0x706
        je .sstr
        cmp ax, 0xF08
        je .guns
        cmp ax, 0x708
        je .suns
        cmp ax, 0xF0D
        je .gdig
        cmp ax, 0x70D
        je .sdig1
        cmp ax, 0x70E
        je .sdig2
        jmp .z                      ; lo demás: se acepta y contesta 0
; ---- parámetros ----
.param:
        movzx bx, cl
        test dl, dl
        jnz .p1
        mov eax, 0x10EC0269
        cmp bl, 0x00
        je .r
        mov eax, 0x00100100
        cmp bl, 0x02
        je .r
        mov eax, 0x00010001
        cmp bl, 0x04
        je .r
        jmp .z
.p1:
        cmp dl, 1
        jne .pw
        mov eax, (ALC_N0 << 16) | ALC_NN
        cmp bl, 0x04
        je .r
        mov eax, 0x00000001
        cmp bl, 0x05
        je .r
        mov eax, 0x00010F0F
        cmp bl, 0x08
        je .r
        mov eax, 0x000E0560
        cmp bl, 0x0A
        je .r
        mov eax, 1
        cmp bl, 0x0B
        je .r
        mov eax, 0xC000000F
        cmp bl, 0x0F
        je .r
        mov eax, 0x40000002
        cmp bl, 0x11
        je .r
        jmp .z
.pw:
        test di, di
        jz .z
        mov eax, [cs:si+R_CAPS]
        cmp bl, 0x09
        je .r
        mov eax, [cs:si+R_PINC]
        cmp bl, 0x0C
        je .r
        mov eax, [cs:si+R_IAMP]
        cmp bl, 0x0D
        je .r
        mov eax, [cs:si+R_OAMP]
        cmp bl, 0x12
        je .r
        mov eax, [cs:si+R_PCM]
        cmp bl, 0x0A
        je .r
        mov eax, [cs:si+R_FMTS]
        cmp bl, 0x0B
        je .r
        movzx eax, byte [cs:si+R_NC]
        cmp bl, 0x0E
        je .r
        mov eax, 0x00001900         ; proceso: capacidades
        cmp bl, 0x10
        jne .z
        cmp dl, 0x20
        je .r
        jmp .z
.ssid:
        mov eax, 0x1179FF6C
        cmp dl, 1
        je .r
        jmp .z
.freset:
        cmp dl, 1
        jne .z
        push cs
        call alc_reset
        call alc_upd
        jmp .z
.conn:
        movzx bx, cl
        xor eax, eax
        mov cx, 4
        add bx, 3
.cn:
        shl eax, 8
        cmp bx, 8
        jae .cn2
        cmp bl, [cs:si+R_NC]
        jae .cn2
        mov al, [cs:si+R_CONN+bx]
.cn2:
        dec bx
        loop .cn
        jmp .r
.cfg:
        mov eax, [cs:si+R_CFG]
        jmp .r
; ---- GPIO ----
.ggd:
        movzx eax, byte [cs:alc_gd]
        jmp .r
.ggm:
        movzx eax, byte [cs:alc_gm]
        jmp .r
.ggdir:
        movzx eax, byte [cs:alc_gdir]
        jmp .r
.sgd:
        and cl, 3
        mov [cs:alc_gd], cl
        jmp .z
.sgm:
        and cl, 3
        mov [cs:alc_gm], cl
        jmp .z
.sgdir:
        and cl, 3
        mov [cs:alc_gdir], cl
        jmp .z
; ---- widget de proceso (20h): índice y coeficientes ----
.gproc:
        cmp dl, 0x20
        jne .z
        movzx eax, byte [cs:alc_proc]
        jmp .r
.sproc:
        cmp dl, 0x20
        jne .z
        mov [cs:alc_proc], cl
        jmp .z
.sidx:
        cmp dl, 0x20
        jne .z
        mov [cs:alc_cidx], cx
        jmp .z
.gidx:
        cmp dl, 0x20
        jne .z
        movzx eax, word [cs:alc_cidx]
        jmp .r
.scoef:
        cmp dl, 0x20
        jne .z
        movzx bx, byte [cs:alc_cidx]
        cmp bx, ALC_NCOEF
        jae .z
        shl bx, 1
        mov [cs:alc_coef+bx], cx
        jmp .z
.gcoef:
        cmp dl, 0x20
        jne .z
        movzx bx, byte [cs:alc_cidx]
        cmp bx, ALC_NCOEF
        jae .z
        shl bx, 1
        movzx eax, word [cs:alc_coef+bx]
        jmp .r
; ---- energía ----
.gpwr:
        cmp dl, 1
        jne .gp2
        movzx eax, word [cs:alc_afgp]
        jmp .r
.gp2:
        test di, di
        jz .z
        movzx eax, byte [cs:di+S_PWR]
        jmp .r
.spwr:
        and cl, 0x0F
        mov al, cl
        shl al, 4
        or al, cl
        movzx ax, al
        cmp dl, 1
        jne .sp2
        mov [cs:alc_afgp], ax
        jmp .z
.sp2:
        test di, di
        jz .z
        mov [cs:di+S_PWR], al
        jmp .z
; ---- conexión elegida, pin, EAPD, flujo, no pedidas, digital ----
.gsel:
        movzx eax, byte [cs:di+S_SEL]
        jmp .r
.ssel:
        mov [cs:di+S_SEL], cl
        call alc_upd
        jmp .z
.gctl:
        movzx eax, byte [cs:di+S_CTL]
        jmp .r
.sctl:
        mov [cs:di+S_CTL], cl
        call alc_upd
        jmp .z
.geapd:
        movzx eax, byte [cs:di+S_EAPD]
        jmp .r
.seapd:
        and cl, 7
        mov [cs:di+S_EAPD], cl
        jmp .z
.gstr:
        movzx eax, byte [cs:di+S_STR]
        jmp .r
.sstr:
        mov [cs:di+S_STR], cl
        call .hip
        call .dac
        jnz .z
        mov eax, ecx
        call alc_send
        call alc_upd
        jmp .z
.guns:
        movzx eax, byte [cs:di+S_UNS]
        jmp .r
.suns:
        mov [cs:di+S_UNS], cl
        jmp .z
.gdig:
        movzx eax, word [cs:di+S_DIG]
        jmp .r
.sdig1:
        mov [cs:di+S_DIG], cl
        jmp .z
.sdig2:
        mov [cs:di+S_DIG+1], cl
        jmp .z
; ---- formato del convertidor ----
.sfmt:
        test di, di
        jz .z
        mov [cs:di+S_FMT], cx
        call .hip
        call .dac
        jnz .z
        mov eax, ecx
        call alc_send
        call alc_upd
        jmp .z
.gfmt:
        test di, di
        jz .z
        movzx eax, word [cs:di+S_FMT]
        jmp .r
; ---- amplificadores ----
; 3h: bit 15 salida, 14 entrada, 13 izquierdo, 12 derecho, 11-8 índice,
; 7 silencio, 6-0 ganancia
.samp:
        call alc_log
        test di, di
        jz .z
        mov al, cl                  ; AL = el valor
        test ch, 0x80
        jz .sai
        mov ebx, [cs:si+R_OAMP]
        call .ajusta
        test ch, 0x20
        jz .so2
        mov [cs:di+S_AOL], al
.so2:
        test ch, 0x10
        jz .sae
        mov [cs:di+S_AOR], al
        jmp .sae
.sai:
        test ch, 0x40
        jz .sae
        mov ebx, [cs:si+R_IAMP]
        call .ajusta
        movzx bx, ch
        and bx, 0x0F
        cmp bx, 8
        jae .sae
        shl bx, 1
        add bx, di
        test ch, 0x20
        jz .si2
        mov [cs:bx+S_AI], al
.si2:
        test ch, 0x10
        jz .sae
        mov [cs:bx+S_AI+1], al
.sae:
        call alc_upd
        jmp .z
; AL = valor, EBX = capacidades del amplificador -> AL como lo guarda el
; códec: sin silencio si no lo tiene; la ganancia, como mucho el último paso
.ajusta:
        push cx
        test ebx, ebx
        js .aj1
        and al, 0x7F
.aj1:
        mov cl, al
        and cl, 0x7F
        mov ch, bh                  ; pasos
        and ch, 0x7F
        cmp cl, ch
        jbe .aj2
        and al, 0x80
        or al, ch
.aj2:
        pop cx
        ret
; Bh: bit 15 salida, 13 izquierdo, 3-0 índice
.gamp:
        test di, di
        jz .z
        test ch, 0x80
        jz .gai
        movzx eax, byte [cs:di+S_AOR]
        test ch, 0x20
        jz .r
        movzx eax, byte [cs:di+S_AOL]
        jmp .r
.gai:
        movzx bx, cl
        and bx, 0x0F
        xor eax, eax
        cmp bx, 8
        jae .r
        shl bx, 1
        add bx, di
        movzx eax, byte [cs:bx+S_AI+1]
        test ch, 0x20
        jz .r
        movzx eax, byte [cs:bx+S_AI]
        jmp .r
.z:
        xor eax, eax
.r:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        retf
; ZF = 1 si el nodo DL es uno de los DAC analógicos (02h, 03h)
.dac:
        cmp dl, 2
        je .dr
        cmp dl, 3
.dr:
        ret
; la hipótesis (modo bit 0): programar el formato o el flujo de un DAC lo
; deja en 0 dB (57h)
.hip:
        test byte [cs:alc_modo], 1
        jz .hr
        call .dac
        jnz .hr
        mov byte [cs:di+S_AOL], 0x57
        mov byte [cs:di+S_AOR], 0x57
.hr:
        ret

; ---- a QEMU: EAX = la orden (se manda a su DAC 02h) ----
alc_send:
        pushad
        and eax, 0x000FFFFF
        or eax, 0x00200000
        cmp dword [cs:alc_fwd], 0
        je .r
        call far [cs:alc_fwd]
.r:
        popad
        ret

; el camino del altavoz (14h) -> la ganancia de QEMU, por lados, si cambia
alc_upd:
        pushad
        mov si, alc_st + (0x14 - ALC_N0) * SSZ
        mov dl, [cs:si+S_SEL]       ; DL = la conexión del pin
        mov di, alc_st + (0x0C - ALC_N0) * SSZ   ; el mezclador
        mov bp, alc_st + (0x02 - ALC_N0) * SSZ   ; y su DAC
        cmp dl, 1
        jne .m
        mov di, alc_st + (0x0D - ALC_N0) * SSZ
        mov bp, alc_st + (0x03 - ALC_N0) * SSZ
.m:
        xor bx, bx                  ; BX = 0 izquierdo, 1 derecho
.lado:
        mov al, 0x80                ; silencio
        cmp dl, 2
        jae .w
        test byte [cs:si+S_CTL], 0x40
        jz .w
        test byte [cs:si+S_AOL+bx], 0x80
        jnz .w
        test byte [cs:di+S_AI+bx], 0x80
        jnz .w
        push si
        mov si, bp
        mov ah, [cs:si+S_AOL+bx]
        pop si
        and ah, 0x7F
        cmp ah, 87
        jbe .g
        mov ah, 87
.g:
        mov al, 87
        sub al, ah
        push bx
        movzx bx, al
        mov al, [cs:alc_qtab+bx]
        pop bx
.w:
        cmp al, [cs:alc_qL+bx]
        je .n
        mov [cs:alc_qL+bx], al
        movzx eax, al
        or eax, 0x0023A000          ; salida, izquierdo
        test bx, bx
        jz .s
        xor ax, 0x3000              ; salida, derecho
.s:
        call alc_send
.n:
        inc bx
        cmp bx, 2
        jb .lado
        popad
        ret

; modo bit 1: «ALCSIM xxxxxxxx» por COM1 (ECX = la orden)
alc_log:
        test byte [cs:alc_modo], 2
        jz .r
        pushad
        mov si, .t
.p:
        mov al, [cs:si]
        test al, al
        jz .h
        call .ch
        inc si
        jmp .p
.h:
        mov bx, 8
.d:
        rol ecx, 4
        mov al, cl
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .d2
        add al, 7
.d2:
        call .ch
        dec bx
        jnz .d
        mov al, 13
        call .ch
        mov al, 10
        call .ch
        popad
.r:
        ret
.ch:
        push cx
        push ax
        mov dx, 0x3FD
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x20
        loopz .w
        pop ax
        mov dx, 0x3F8
        out dx, al
        pop cx
        ret
.t      db "ALCSIM ", 0
