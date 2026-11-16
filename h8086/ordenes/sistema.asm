; =============================================================================
;  SISTEMA.COM (SYS) - deja un disco de arranque
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       SISTEMA unidad: [/A]
;
;  Escribe en el disco (que ya tiene formato) el sector de arranque de
;  Hispa-8086, conservando sus medidas (la BPB), y copia a su raíz
;  NUCLEO86.SYS y COMMAND.COM, que se toman de la unidad en la que está esta
;  orden. Los archivos que hubiera en el disco no se tocan.
;
;  En el disco duro (C:), además: en el sector de arranque quedan dichos los
;  sectores que hay delante de la partición («ocultos») y las medidas del
;  disco según la BIOS; la partición se marca como activa; y si el sector de
;  particiones no tiene código, se le pone el nuestro (mbr.asm).
;
;  Los dos archivos se leen a la memoria antes de empezar: con una sola
;  disquetera se puede cambiar de disquete cuando lo pide. /A: no pregunta
;  nada (para los lotes). Devuelve 0 si ha ido bien, 4 si no y 5 si se
;  contesta que no. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define CON_SISTEMA

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        call pide_nucleo
        call xfer_ini
        mov byte [unidad], 0xFF
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        je .opcion
        cmp byte [si+1], ':'
        jne .uso
        call mayus
        sub al, 'A'
        mov [unidad], al
        add si, 2
        jmp .arg
.opcion:
        inc si
        lodsb
        call mayus
        cmp al, 'A'
        jne .uso
        mov byte [sin_preguntar], 1
        jmp .arg
.uso:
        mov si, m_uso
        jmp error
.fin_args:
        mov al, [unidad]
        cmp al, 0xFF
        je .uso
        add al, 'A'
        mov [m_aviso.u], al
        mov [m_cambia.u], al
        mov [m_hecho.u], al
        cmp byte [unidad], 2
        ja .no_unidad
        je .lee
        cmp byte [unidad], 0
        je .lee
        jmp short .lee              ; (B: existe siempre: con una sola
.no_unidad:                         ; disquetera es la misma, con otro disquete)
        mov si, m_e_unidad
        jmp error
.lee:
        call sis_lee                ; el sistema, a la memoria
        jnc .leido
        mov si, m_sin_sistema
        jmp error
.leido:
        mov si, m_aviso
        call confirma
        je .va
        mov si, m_nada
        call pon_cad
        mov ax, 0x4C05
        int 0x21
.va:
        ; de un disquete a otro en la misma unidad: hay que cambiarlo
        cmp byte [unidad], 2
        jae .destino
        mov al, [t_sis_nucleo]
        sub al, 'A'
        cmp al, [unidad]
        je .cambia                  ; del mismo disquete... o de A: a B: con
        cmp al, 2                   ; una sola disquetera
        jae .destino
        int 0x11
        test al, 1
        jz .cambia
        and al, 0xC0
        jnz .destino
.cambia:
        cmp byte [sin_preguntar], 0
        jne .destino
        mov si, m_cambia
        call pon_cad
        DOS 0x08
        call pon_nl
.destino:
        cmp byte [unidad], 2
        je .duro
        mov al, [unidad]            ; un disquete: su primer sector se lee con
        mov [d_bios], al            ; cualquier medida
        int 0x11                    ; (con una sola disquetera, A: y B: son
        test al, 1                  ; la misma: la 0 de la BIOS)
        jz .una
        and al, 0xC0
        jnz .dos
.una:
        mov byte [d_bios], 0
.dos:
        mov word [d_spp], 9
        mov word [d_caras], 2
        jmp short .arranque
.duro:
        call duro_lee
        jnc .d1
        mov si, m_e_sin_duro
        jmp error
.d1:
        call duro_particion
        jnc .d2
        mov si, m_e_sin_part
        jmp error
.d2:
        mov [fila], si
        mov ax, [si+8]
        mov [d_base], ax
        mov ax, [si+10]
        mov [d_base+2], ax
.arranque:
        xor ax, ax
        mov bx, sec
        call disco_lee
        jnc .a1
        mov si, m_e_leer
        jmp error
.a1:
        ; ¿tiene formato? Sus medidas tienen que ser de las que entendemos
        cmp word [sec+510], 0xAA55
        jne .sin_formato
        cmp word [sec+11], 512
        jne .sin_formato
        cmp byte [sec+13], 0
        je .sin_formato
        cmp byte [sec+16], 0
        je .sin_formato
        cmp word [sec+22], 0
        jne .formato
.sin_formato:
        mov si, m_e_sin_formato
        jmp error
.formato:
        ; ¿FAT12 o FAT16? Por los grupos que tiene, como lo decide el núcleo
        mov al, [sec+16]
        xor ah, ah
        mul word [sec+22]
        add ax, [sec+14]
        mov bx, [sec+17]
        add bx, 15
        mov cl, 4
        shr bx, cl
        add ax, bx                  ; AX = el primer sector de los datos
        mov bx, [sec+19]
        test bx, bx
        jnz .tot
        cmp word [sec+34], 0
        jne .sin_formato
        mov bx, [sec+32]
.tot:
        sub bx, ax
        jbe .sin_formato
        mov ax, bx
        xor dx, dx
        mov bl, [sec+13]
        xor bh, bh
        div bx
        inc ax
        xor bl, bl
        cmp ax, 0x0FF6
        jb .tipo
        inc bl
.tipo:
        mov [arr+509], bl
        ; nuestro sector, con las medidas del disco
        mov si, sec + 11
        mov di, arr + 11
        mov cx, 51
        rep movsb
        cmp byte [unidad], 2
        je .bpb_duro
        xor ax, ax                  ; un disquete no tiene nada delante
        mov [arr+28], ax
        mov [arr+30], ax
        mov [arr+36], al
        cmp word [arr+19], 0
        je .escribe
        mov [arr+32], ax
        mov [arr+34], ax
        jmp short .escribe
.bpb_duro:
        mov ax, [d_base]
        mov [arr+28], ax
        mov ax, [d_base+2]
        mov [arr+30], ax
        mov ax, [d_spp]
        mov [arr+24], ax
        mov ax, [d_caras]
        mov [arr+26], ax
        mov byte [arr+36], 0x80
.escribe:
        xor ax, ax
        mov bx, arr
        call disco_escribe
        jc .mal_escribir
        cmp byte [unidad], 2
        jne .copia
        mov si, [fila]              ; la partición, activa; y un sector de
        call duro_codigo            ; particiones que la arranque
        call duro_activa
        call duro_escribe
        jnc .copia
.mal_escribir:
        mov si, m_e_escribir
        cmp byte [d_err], 3
        jne .me
        mov si, m_e_protegido
.me:
        jmp error
.copia:
        call avisa_nucleo
        cmp byte [unidad], 2
        jae .c0
        mov ax, 0xE803              ; (el disquete puesto es el de esta unidad)
        mov dl, [unidad]
        int 0x21
.c0:
        mov al, [unidad]
        add al, 'A'
        call sis_escribe
        jnc .hecho
        mov si, m_e_copiar
        jmp error
.hecho:
        mov si, m_hecho
        call pon_cad
        mov ax, 0x4C00
        int 0x21

; escribe el mensaje de SI y termina con el código 4
error:
        call pon_cad
        call avisa_nucleo
        mov ax, 0x4C04
        int 0x21

m_uso       db "SISTEMA - deja un disco de arranque", 10
            db "  SISTEMA unidad: [/A]", 10
            db "Escribe el sector de arranque de Hispa-8086 en el disco (que ya tiene", 10
            db "formato) y le copia NUCLEO86.SYS y COMMAND.COM. /A: no pregunta.", 10, 0
m_aviso     db "Se va a escribir el arranque de Hispa-8086 en "
.u          db "A: (sus archivos no se tocan).", 10
            db "Pulse S para seguir (otra tecla lo deja): ", 0
m_cambia    db "Ponga el disquete de destino en "
.u          db "A: y pulse una tecla...", 0
m_nada      db "No se ha tocado nada.", 10, 0
m_hecho     db "Sistema transferido a "
.u          db "A:.", 10, 0
m_e_unidad  db "No existe esa unidad.", 10, 0
m_sin_sistema db "No se encuentran NUCLEO86.SYS y COMMAND.COM junto a esta orden.", 10, 0
m_e_sin_duro db "No hay disco duro.", 10, 0
m_e_sin_part db "El disco duro no tiene partición de DOS: créela con PARTIR.", 10, 0
m_e_leer    db "No se puede leer el disco.", 10, 0
m_e_sin_formato db "El disco no tiene formato: déselo antes con FORMATEA.", 10, 0
m_e_escribir db "Error al escribir en el disco.", 10, 0
m_e_protegido db "El disco está protegido contra escritura.", 10, 0
m_e_copiar  db "No se ha podido copiar el sistema: ¿está lleno el disco?", 10, 0

; el sector de particiones y el de arranque, tal como salen de ensamblar
mbr_codigo:
        incbin "../mbr.bin"
arr:
        incbin "../arranque.bin"

        absolute $
            alignb 2
bss_ini:
unidad      resb 1                  ; 0 = A:
            alignb 2
fila        resw 1                  ; la fila de la tabla de particiones
            UTIL_BSS
sec         resb 512
bss_fin:
sis_mem:                            ; aquí se leen los archivos del sistema
