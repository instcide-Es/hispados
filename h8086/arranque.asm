; =============================================================================
;  Hispa-8086 - sector de arranque (FAT12 y FAT16, disquetes y disco duro)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La BIOS (o el sector de particiones, mbr.asm) lo carga en 0000:7C00. Busca
;  NUCLEO86.SYS en el directorio raíz, lo carga entero en 0070:0000 y salta a
;  él con DL = la unidad de arranque.
;
;  El mismo sector vale para todo:
;    - cualquier disquete FAT12 (360 K, 720 K, 1,2 M, 1,44 M): las medidas las
;      lee de su propia tabla (la BPB), que rellena quien da formato;
;    - una partición FAT12 o FAT16 de disco duro: a cada sector se le suman
;      los «sectores ocultos» de la BPB (donde empieza la partición), y las
;      medidas del disco (sectores por pista y caras) se le preguntan a la
;      BIOS, que es la que manda: las de la BPB son las de la máquina en que
;      se dio formato.
;  La FAT no se carga entera (una FAT16 son hasta 64 K): se traen dos sectores
;  seguidos cada vez, los del grupo por el que se va.
;
;  Quien pone este sector en un disco (FORMATEA, SISTEMA, pon_arranque.py)
;  conserva la BPB del disco (octetos 11 a 61) y deja dicho en el octeto 509
;  si la FAT es de 16 bits (1) o de 12 (0).
;  Solo instrucciones del 8086 (el ensamblador lo vigila: «cpu 8086»).
; =============================================================================
        cpu 8086
        bits 16
        org 0x7C00

NUC_SEG     equ 0x0070              ; donde se carga el núcleo
FAT_DIR     equ 0x7E00              ; dos sectores de la FAT, detrás de este
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
b_ocultos   dd 0                    ; dónde empieza la partición
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
        test dl, dl
        jns .medidas                ; un disquete: sus medidas son las de la BPB
        mov ah, 8                   ; un disco duro: las que diga la BIOS
        int 0x13
        jc .medidas
        and cx, 63
        mov [b_spp], cx
        mov cl, dh
        inc cx
        mov [b_caras], cx
.medidas:
        xor ax, ax                  ; (la BIOS puede haber cambiado ES)
        mov es, ax
        ; dónde empiezan el directorio raíz y los datos
        mov al, [b_nfat]
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
        dec ax                      ; el sector del grupo
        dec ax
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
        ; el grupo siguiente, en la FAT
        push es
        push bx
        push ds
        pop es
        mov si, ax
        cmp byte [es_fat16], 0
        je .doce
        mov al, ah                  ; FAT16: una palabra por grupo; 256 por sector
        xor ah, ah
        and si, 0x00FF
        shl si, 1
        call fat_trae
        mov ax, [FAT_DIR+si]
        cmp ax, 0xFFF8
        jmp short .sig
.doce:
        shr si, 1                   ; FAT12: doce bits, en el octeto n*3/2
        add si, ax
        push ax
        mov ax, si
        mov cl, 9
        shr ax, cl
        and si, 511
        call fat_trae
        pop ax
        mov dx, [FAT_DIR+si]
        test al, 1
        jz .par
        mov cl, 4
        shr dx, cl
.par:
        and dx, 0x0FFF
        mov ax, dx
        cmp ax, 0x0FF8
.sig:
        pop bx
        pop es
        jb .grupo
        mov dl, [b_unidad]
        jmp NUC_SEG:0

; trae a FAT_DIR el sector AX de la FAT y el que le sigue (un grupo de FAT12
; puede caer entre dos), si no son los que ya están
fat_trae:
        add ax, [b_res]
        cmp ax, [fat_act]
        je .x
        mov [fat_act], ax
        mov bx, FAT_DIR
        call lee
        inc ax
        add bx, 512
        call lee
.x:
        ret

; lee el sector AX (contado desde el principio de la partición) en ES:BX; tres
; intentos: un disquete tarda en arrancar. Conserva los registros
lee:
        push ax
        push cx
        push dx
        push si
        mov si, 3
.otra:
        push ax
        xor dx, dx
        add ax, [b_ocultos]
        adc dx, [b_ocultos+2]
        div word [b_spp]            ; DX = sector - 1, AX = pista x caras + cara
        mov cx, dx
        inc cx
        xor dx, dx
        div word [b_caras]          ; AX = cilindro, DX = cara
        mov ch, al
        ror ah, 1                   ; (los dos bits altos del cilindro van con
        ror ah, 1                   ; el sector)
        or cl, ah
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
        jmp short error
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
fat_act     dw 0

        times 509 - ($ - $$) db 0
es_fat16    db 0                    ; 1: la FAT es de 16 bits (lo pone quien instala)
        dw 0xAA55
