; =============================================================================
;  Hispa-8086 - sector de arranque (FAT12, disquetes)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La BIOS lo carga en 0000:7C00. Busca NUCLEO86.SYS en el directorio raíz, lo
;  carga entero en 0070:0000 y salta a él con DL = la unidad de arranque.
;  Vale para cualquier disquete FAT12 (360 K, 720 K, 1,2 M, 1,44 M): las
;  medidas las lee de su propia tabla (la BPB), que rellena quien da formato.
;  Solo instrucciones del 8086 (el ensamblador lo vigila: «cpu 8086»).
; =============================================================================
        cpu 8086
        bits 16
        org 0x7C00

NUC_SEG     equ 0x0070              ; donde se carga el núcleo
FAT_DIR     equ 0x7E00              ; la FAT, detrás de este sector
RAIZ_DIR    equ 0x0500              ; un sector del directorio raíz

        jmp short inicio
        nop
        db "HISPA86 "               ; quién dio formato
; ---- la BPB (la rellena el formateador; aquí, la de 1,44 M) ----
b_bps       dw 512                  ; octetos por sector
b_spc       db 1                    ; sectores por grupo
b_res       dw 1                    ; sectores reservados
b_nfat      db 2                    ; cuántas FAT
b_raiz      dw 224                  ; entradas del directorio raíz
b_tot       dw 2880                 ; sectores del disco
b_medio     db 0xF0
b_spf       dw 9                    ; sectores de cada FAT
b_spp       dw 18                   ; sectores por pista
b_caras     dw 2
b_ocultos   dd 0
b_tot32     dd 0
b_unidad    db 0
            db 0
            db 0x29
            dd 0x19861116
            db "HISPA-8086 "
            db "FAT12   "

inicio:
        cli
        xor ax, ax
        mov ss, ax
        mov sp, 0x7C00
        mov ds, ax
        mov es, ax
        sti
        cld
        mov [b_unidad], dl
        ; la FAT entera, a FAT_DIR
        mov ax, [b_res]
        mov cx, [b_spf]
        mov bx, FAT_DIR
.fat:
        call lee
        inc ax
        add bx, 512
        loop .fat
        ; dónde empiezan el directorio raíz y los datos
        mov al, [b_nfat]
        xor ah, ah
        mul word [b_spf]
        add ax, [b_res]             ; AX = primer sector de la raíz
        mov bx, [b_raiz]
        mov cl, 4
        shr bx, cl                  ; BX = sectores de la raíz (16 entradas cada uno)
        mov dx, ax
        add dx, bx
        mov [datos], dx
        ; buscar el núcleo en la raíz
        mov dx, bx                  ; DX = sectores que quedan por mirar
.sector:
        mov bx, RAIZ_DIR
        call lee
        mov di, bx
.entrada:
        push di
        mov si, nombre
        mov cx, 11
        repe cmpsb
        pop di
        je .esta
        add di, 32
        cmp di, RAIZ_DIR + 512
        jb .entrada
        inc ax
        dec dx
        jnz .sector
        mov si, m_falta
        jmp error
.esta:
        mov ax, [di+26]             ; su primer grupo
        mov bx, NUC_SEG
        mov es, bx
        xor bx, bx
.grupo:
        push ax
        sub ax, 2                   ; el sector del grupo
        mov cl, [b_spc]
        xor ch, ch
        mul cx
        add ax, [datos]
.s:
        call lee
        inc ax
        add bx, 512
        loop .s
        pop ax
        mov si, ax                  ; el siguiente: 12 bits en la FAT, en n*3/2
        shr si, 1
        add si, ax
        mov dx, [FAT_DIR+si]
        test al, 1
        jz .par
        mov cl, 4
        shr dx, cl
.par:
        and dx, 0x0FFF
        mov ax, dx
        cmp ax, 0x0FF8
        jb .grupo
        mov dl, [b_unidad]
        jmp NUC_SEG:0

; lee el sector AX en ES:BX (tres intentos: un disquete tarda en arrancar).
; Conserva los registros
lee:
        push ax
        push cx
        push dx
        push si
        mov si, 3
.otra:
        push ax
        xor dx, dx
        div word [b_spp]            ; DX = sector - 1, AX = pista x caras + cara
        mov cl, dl
        inc cl
        xor dx, dx
        div word [b_caras]          ; AX = pista, DX = cara
        mov ch, al
        mov dh, dl
        mov dl, [b_unidad]
        mov ax, 0x0201
        int 0x13
        pop ax
        jnc .bien
        push ax
        xor ax, ax                  ; reponer la unidad y otra vez
        int 0x13
        pop ax
        dec si
        jnz .otra
        mov si, m_disco
        jmp error
.bien:
        pop si
        pop dx
        pop cx
        pop ax
        ret

; escribe el mensaje de DS:SI, espera una tecla y vuelve a arrancar
error:
        lodsb
        test al, al
        jz .fin
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp error
.fin:
        xor ax, ax
        int 0x16
        int 0x19

nombre      db "NUCLEO86SYS"
m_falta     db "Falta NUCLEO86.SYS", 0
m_disco     db "Error de disco", 0
datos       dw 0

        times 510 - ($ - $$) db 0
        dw 0xAA55
