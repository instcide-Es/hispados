; =============================================================================
;  HispaBIOS - interrupciones de la BIOS de modo real (las incluye bios64.asm)
; =============================================================================
;  Comportamiento según la lista de interrupciones de Ralf Brown y las
;  especificaciones de Phoenix (EDD 3.0 para INT 13h extendida) y PCI BIOS 2.1.
;  Disco, teclado y espera los hace el firmware a través de llamar_fw; el
;  reloj de DOS se lleva con el contador del procesador (TSC).
; =============================================================================
        bits 16

PET_LEER        equ 3
PET_ESCRIBIR    equ 4
PET_TECLA       equ 5
PET_MICROSEG    equ 6
PET_REINICIAR   equ 7
TROZO           equ 16              ; sectores por viaje al firmware (8 KB)
SEG_PASO        equ SEG_BIOS + (0C000h >> 4)

; =============================================================================
;  Reloj: el temporizador del firmware llega en modo real por su vector
;  (VEC_FW_IRQ0); con el TSC se cuentan los tics de DOS (18,2 por segundo)
;  y por cada uno se llama a INT 08h. INT 08h (la nuestra) lleva 40:6C y
;  llama a INT 1Ch. De paso, si la pantalla ha cambiado, se dibuja.
; =============================================================================
irq_fw0:
        push eax
        push edx
        push ds
        push es
        mov ax, SEG_BIOS
        mov ds, ax
        cmp byte [dbg_t], 0             ; [depuración] la primera vez, 'T'
        jne .ya
        mov byte [dbg_t], 1
        mov al, 'T'
        call dbg
.ya:
        mov al, 20h                     ; fin de interrupción (maestro)
        out 20h, al
        cmp dword [T_TSC_TICK], 0
        je .dibujar
        rdtsc
        sub eax, [tsc0]
        sbb edx, [tsc0 + 4]
        cmp edx, [T_TSC_TICK]           ; (si no cabe, se deja)
        jae .dibujar
        div dword [T_TSC_TICK]          ; EAX = tics desde el arranque
.tic:   cmp eax, [tics_dados]
        jbe .dibujar
        inc dword [tics_dados]
        push eax
        int 08h
        pop eax
        jmp .tic
.dibujar:
        cmp byte [sucia], 0
        je .fin
        cmp byte [en_fw], 0
        jne .fin
        mov byte [sucia], 0
        push ax
        mov ax, PET_DIBUJAR
        call llamar_fw
        pop ax
.fin:   pop es
        pop ds
        pop edx
        pop eax
        iret

irq_fw_maestro:                         ; otras IRQ del firmware: solo el fin
        push ax
        mov al, 20h
        out 20h, al
        pop ax
        iret
irq_esclavo:                            ; IRQ 8-15 sin dueño: fin en los dos
        push ax
        mov al, 20h
        out 0A0h, al
        out 20h, al
        pop ax
        iret

int08:                                  ; IRQ 0: el tic de DOS
        push ds
        push ax
        mov ax, 40h
        mov ds, ax
        add word [6Ch], 1
        adc word [6Eh], 0
        cmp word [6Eh], 18h             ; medianoche: 1800B0h tics
        jb .s
        cmp word [6Ch], 0B0h
        jb .s
        mov word [6Ch], 0
        mov word [6Eh], 0
        mov byte [70h], 1
.s:     pop ax
        pop ds
        int 1Ch
        push eax
        push ds
        mov al, 20h                     ; fin de interrupción
        out 20h, al
        mov ax, SEG_BIOS                ; si la pantalla ha cambiado, se dibuja
        mov ds, ax
        cmp byte [sucia], 0
        je .fin
        cmp byte [en_fw], 0
        jne .fin
        mov byte [sucia], 0
        mov ax, PET_DIBUJAR
        call llamar_fw
.fin:   pop ds
        pop eax
        iret

; =============================================================================
;  INT 13h - discos (80h = el de arranque, 81h... los demás)
; =============================================================================
int13:
        sti
        push ax                         ; [depuración] 'D' y la función por COM1
        mov al, 'D'
        call dbg
        mov al, ah
        call dbg_hex
        pop ax
        cmp dl, 80h
        jb .nada
        push ax
        mov al, dl
        sub al, 80h
        cmp al, [cs:T_NDISCOS]
        pop ax
        jae .nada
        cmp ah, 00h
        je .bien
        cmp ah, 01h
        je .estado
        cmp ah, 02h
        je d_chs_leer
        cmp ah, 03h
        je d_chs_escribir
        cmp ah, 04h
        je .bien                        ; verificar: se da por bueno
        cmp ah, 08h
        je d_parametros
        cmp ah, 0Ch
        je .bien
        cmp ah, 0Dh
        je .bien
        cmp ah, 10h
        je .bien
        cmp ah, 15h
        je d_tipo
        cmp ah, 41h
        je d_ext_hay
        cmp ah, 42h
        je d_ext_leer
        cmp ah, 43h
        je d_ext_escribir
        cmp ah, 44h
        je .bien
        cmp ah, 47h
        je .bien
        cmp ah, 48h
        je d_ext_param
.nada:  mov ah, 01h                     ; función o disco que no hay
        jmp d_error
.estado:
        xor ax, ax
.bien:  xor ah, ah
d_bien:
        push bp
        mov bp, sp
        and byte [bp + 6], 0FEh         ; CF = 0 en las banderas guardadas
        pop bp
        iret
d_error:
        push bp
        mov bp, sp
        or byte [bp + 6], 1             ; CF = 1
        pop bp
        iret

; d_disco: DL -> SI = desplazamiento de su entrada en T_DISCOS
d_disco:
        push ax
        movzx si, dl
        sub si, 80h
        shl si, 4
        add si, T_DISCOS
        pop ax
        ret

; geometría fija: 255 cabezas, 63 sectores; cilindros según el tamaño
d_parametros:                           ; 08h
        push si
        push eax
        push ebx
        push edx
        call d_disco
        mov eax, [cs:si]                ; sectores (bajo)
        cmp dword [cs:si + 4], 0
        je .c
        mov eax, 0FFFFFFFFh
.c:     xor edx, edx
        mov ebx, 255 * 63
        div ebx
        cmp eax, 1024
        jb .k
        mov eax, 1024
.k:     dec ax                          ; último cilindro
        mov ch, al
        mov cl, ah
        shl cl, 6
        or cl, 63
        pop edx
        mov dh, 254
        mov dl, [cs:T_NDISCOS]
        pop ebx
        xor bl, bl
        pop eax
        xor ax, ax
        pop si
        jmp d_bien

d_tipo:                                 ; 15h -> AH = 3 (disco duro), CX:DX sectores
        push si
        call d_disco
        mov dx, [cs:si]
        mov cx, [cs:si + 2]
        cmp dword [cs:si + 4], 0
        je .ok
        mov cx, 0FFFFh
        mov dx, cx
.ok:    pop si
        mov ah, 3
        jmp d_bien

d_ext_hay:                              ; 41h
        cmp bx, 55AAh
        jne int13.nada
        mov bx, 0AA55h
        mov ah, 21h                     ; EDD 1.1
        mov cx, 1                       ; acceso por paquete
        jmp d_bien

d_ext_param:                            ; 48h: DS:SI = búfer
        push di
        push eax
        push es
        push ds
        pop es
        mov di, si
        push si
        call d_disco
        mov ax, [es:di]
        cmp ax, 26
        jb .corto
        mov word [es:di], 26
        mov word [es:di + 2], 2         ; geometría válida... no la usamos
        mov dword [es:di + 4], 1024
        mov dword [es:di + 8], 255
        mov dword [es:di + 12], 63
        mov eax, [cs:si]
        mov [es:di + 16], eax
        mov eax, [cs:si + 4]
        mov [es:di + 20], eax
        mov word [es:di + 24], 512
        pop si
        pop es
        pop eax
        pop di
        xor ah, ah
        jmp d_bien
.corto: pop si
        pop es
        pop eax
        pop di
        mov ah, 01h
        jmp d_error

; ---- CHS: AL sectores, CH/CL cilindro y sector, DH cabeza, ES:BX búfer ----
d_chs_leer:
        mov byte [cs:d_op], PET_LEER
        jmp d_chs
d_chs_escribir:
        mov byte [cs:d_op], PET_ESCRIBIR
d_chs:
        push eax
        push ebx
        push ecx
        push edx
        push edi
        push es
        mov [cs:d_n], al
        mov byte [cs:d_n + 1], 0
        mov [cs:d_buf], bx
        mov [cs:d_buf + 2], es
        ; LBA = (cilindro * 255 + cabeza) * 63 + sector - 1
        movzx eax, cl
        and al, 0C0h
        shl eax, 2
        mov al, ch                      ; EAX = cilindro (10 bits)
        imul eax, eax, 255
        movzx edi, dh
        add eax, edi
        imul eax, eax, 63
        movzx edi, cl
        and edi, 3Fh
        dec edi
        add eax, edi
        mov [cs:d_lba], eax
        mov dword [cs:d_lba + 4], 0
        call d_transferir
        pop es
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        mov ah, [cs:d_estado]
        mov al, [cs:d_hechos]
        test ah, ah
        jnz d_error
        jmp d_bien

; ---- extendida: DS:SI = paquete (tamaño, -, n, desplazamiento, segmento, LBA) ----
d_ext_leer:
        mov byte [cs:d_op], PET_LEER
        jmp d_ext
d_ext_escribir:
        mov byte [cs:d_op], PET_ESCRIBIR
d_ext:
        push eax
        mov ax, [si + 2]
        mov [cs:d_n], ax
        mov eax, [si + 4]
        mov [cs:d_buf], eax
        mov eax, [si + 8]
        mov [cs:d_lba], eax
        mov eax, [si + 12]
        mov [cs:d_lba + 4], eax
        call d_transferir
        mov ax, [cs:d_hechos]
        mov [si + 2], ax
        pop eax
        mov ah, [cs:d_estado]
        test ah, ah
        jnz d_error
        jmp d_bien

; d_transferir: d_op, DL (disco), d_lba, d_n, d_buf -> d_estado, d_hechos
d_transferir:
        pushad
        push ds
        push es
        mov ax, SEG_BIOS
        mov ds, ax
        mov byte [d_estado], 0
        mov word [d_hechos], 0
        sub dl, 80h
        mov [P_DISCO], dl
.trozo:
        mov cx, [d_n]
        sub cx, [d_hechos]
        jz .fin
        cmp cx, TROZO
        jbe .n
        mov cx, TROZO
.n:     mov [P_N], cx
        mov eax, [d_lba]
        mov [P_LBA], eax
        mov eax, [d_lba + 4]
        mov [P_LBA + 4], eax
        cmp byte [d_op], PET_ESCRIBIR
        jne .pide
        call d_copiar_a_paso
.pide:  movzx ax, byte [d_op]
        call llamar_fw
        test eax, eax
        jnz .mal
        cmp byte [d_op], PET_LEER
        jne .sigue
        call d_copiar_de_paso
.sigue: movzx eax, word [P_N]
        add [d_hechos], ax
        add [d_lba], eax
        adc dword [d_lba + 4], 0
        shl ax, 5                       ; sectores -> párrafos
        add [d_buf + 2], ax
        jmp .trozo
.mal:   mov byte [d_estado], 04h        ; sector no encontrado
        mov al, '!'
        call dbg
.fin:   pop es
        pop ds
        popad
        ret

; copias entre el búfer de la llamada (d_buf) y el de paso (P_N sectores)
d_copiar_de_paso:
        push ds
        push es
        push si
        push di
        push cx
        mov cx, [P_N]
        shl cx, 8                       ; palabras
        les di, [d_buf]
        mov si, SEG_PASO
        mov ds, si
        xor si, si
        cld
        rep movsw
        pop cx
        pop di
        pop si
        pop es
        pop ds
        ret
d_copiar_a_paso:
        push ds
        push es
        push si
        push di
        push cx
        mov cx, [P_N]
        shl cx, 8
        lds si, [d_buf]
        mov di, SEG_PASO
        mov es, di
        xor di, di
        cld
        rep movsw
        pop cx
        pop di
        pop si
        pop es
        pop ds
        ret

; =============================================================================
;  INT 16h - teclado (las teclas las da el firmware; búfer de la BIOS 40:1E)
; =============================================================================
int16:
        sti
        cmp ah, 00h
        je k_leer
        cmp ah, 10h
        je k_leer
        cmp ah, 01h
        je k_mirar
        cmp ah, 11h
        je k_mirar
        cmp ah, 02h
        je k_estado
        cmp ah, 12h
        je k_estado
        cmp ah, 05h
        je k_meter
        iret

k_estado:
        push ds
        mov ax, 40h
        mov ds, ax
        mov al, [17h]
        mov ah, [18h]
        pop ds
        iret

k_meter:                                ; 05h: CX = tecla -> AL = 0 si cabe
        push bx
        mov bx, cx
        call k_poner
        pop bx
        mov al, 0
        jnc .ok
        mov al, 1
.ok:    iret

k_leer:                                 ; 00h/10h: espera una tecla -> AX
.otra:  call k_sacar
        jnc .hay
        call k_sondear
        call k_sacar
        jnc .hay
        push ds                         ; nada: se dibuja si hace falta y se
        push ax                         ; espera al siguiente tic
        mov ax, SEG_BIOS
        mov ds, ax
        cmp byte [sucia], 0
        je .espera
        mov byte [sucia], 0
        mov ax, PET_DIBUJAR
        call llamar_fw
.espera:
        pop ax
        pop ds
        hlt
        jmp .otra
.hay:   iret

k_mirar:                                ; 01h/11h: ¿hay tecla? -> ZF, AX
        call k_mirar_buf
        jnc .hay
        call k_sondear
        call k_mirar_buf
        jnc .hay
        push bp
        mov bp, sp
        or byte [bp + 6], 40h           ; ZF = 1: no hay
        pop bp
        iret
.hay:   push bp
        mov bp, sp
        and byte [bp + 6], 0BFh         ; ZF = 0
        pop bp
        iret

; búfer circular 40:1E-3D (cabeza 40:1A, cola 40:1C)
k_mirar_buf:                            ; -> AX = la primera, CF si vacío
        push ds
        push bx
        mov bx, 40h
        mov ds, bx
        mov bx, [1Ah]
        cmp bx, [1Ch]
        stc
        je .f
        mov ax, [bx]
        clc
.f:     pop bx
        pop ds
        ret
k_sacar:                                ; -> AX, CF si vacío
        push ds
        push bx
        mov bx, 40h
        mov ds, bx
        mov bx, [1Ah]
        cmp bx, [1Ch]
        stc
        je .f
        mov ax, [bx]
        add bx, 2
        cmp bx, 3Eh
        jb .g
        mov bx, 1Eh
.g:     mov [1Ah], bx
        clc
.f:     pop bx
        pop ds
        ret
k_poner:                                ; BX = tecla -> CF si lleno
        push ds
        push ax
        push di
        mov ax, 40h
        mov ds, ax
        mov di, [1Ch]
        mov ax, di
        add ax, 2
        cmp ax, 3Eh
        jb .g
        mov ax, 1Eh
.g:     cmp ax, [1Ah]
        stc
        je .f
        mov [di], bx
        mov [1Ch], ax
        clc
.f:     pop di
        pop ax
        pop ds
        ret

; k_sondear: pregunta al firmware por una tecla y la deja en el búfer
k_sondear:
        push eax
        push bx
        push si
        mov ax, PET_TECLA
        call llamar_fw
        cmp eax, -1
        je .f
        ; EAX: bajo = código de exploración de UEFI, alto = carácter Unicode
        test ax, ax
        jnz .especial
        shr eax, 16                     ; AX = carácter
        cmp ax, 0Dh
        je .intro
        cmp ax, 08h
        je .retro
        cmp ax, 09h
        je .tab
        cmp ax, 80h
        jb .ascii
        ; letras del español (Unicode -> CP437)
        mov si, k_uni
.u:     cmp word [cs:si], 0
        je .f
        cmp [cs:si], ax
        je .u_si
        add si, 3
        jmp .u
.u_si:  movzx bx, byte [cs:si + 2]
        jmp .poner
.ascii: movzx si, al
        mov bl, al
        mov bh, 0
        cmp al, 20h
        jb .ctrl
        mov bh, [cs:k_scan + si - 20h]
        jmp .poner
.ctrl:  mov bh, 0                       ; Ctrl+letra: sin código de exploración
        jmp .poner
.intro: mov bx, 1C0Dh
        jmp .poner
.retro: mov bx, 0E08h
        jmp .poner
.tab:   mov bx, 0F09h
        jmp .poner
.especial:                              ; teclas sin carácter
        cmp ax, 17h
        je .esc
        cmp ax, 18h
        ja .f
        movzx si, al
        mov bh, [cs:k_esp + si]
        test bh, bh
        jz .f
        mov bl, 0
        jmp .poner
.esc:   mov bx, 011Bh
.poner: call k_poner
.f:     pop si
        pop bx
        pop eax
        ret

; =============================================================================
;  INT 15h - sistema
; =============================================================================
int15:
        sti
        cmp ah, 88h
        je s_ext
        cmp ax, 0E801h
        je s_e801
        cmp ax, 0E820h
        je s_e820
        cmp ah, 87h
        je s_mover
        cmp ah, 86h
        je s_esperar
        cmp ah, 24h
        je s_a20
        cmp ah, 0C0h
        je s_config
        cmp ah, 4Fh
        je s_teclado
        cmp ah, 90h
        je s_cero
        cmp ah, 91h
        je s_cero
        mov ah, 86h                     ; no se admite
        jmp d_error
s_cero: xor ah, ah
        jmp d_bien
s_teclado:                              ; la tecla sigue su camino
        jmp d_error

s_ext:                                  ; 88h -> AX = KB por encima de 1 MB
        mov eax, [cs:T_EXTKB]
        cmp eax, 0FFFFh
        jbe .k
        mov ax, 0FFFFh
.k:     jmp d_bien

s_e801:                                 ; E801h -> AX/CX hasta 16 MB, BX/DX encima
        push eax
        mov eax, [cs:T_EXTKB]
        cmp eax, 15360
        jbe .bajo
        sub eax, 15360
        shr eax, 6
        mov bx, ax
        mov dx, ax
        mov cx, 15360
        jmp .f
.bajo:  mov cx, ax
        xor bx, bx
        xor dx, dx
.f:     pop eax
        mov ax, cx
        jmp d_bien

s_e820:                                 ; E820h: EBX = índice, ES:DI = búfer
        cmp edx, 534D4150h
        jne .mal
        cmp ebx, 3
        jae .mal
        push si
        push ecx
        mov si, bx
        imul si, si, 20
        add si, e820
        mov ecx, 20
.c:     mov al, [cs:si]
        mov [es:di], al
        inc si
        inc di
        loop .c
        sub di, 20
        pop ecx
        pop si
        inc ebx
        cmp ebx, 3
        jb .sigue
        xor ebx, ebx
.sigue: mov eax, 534D4150h
        mov ecx, 20
        jmp d_bien
.mal:   mov ah, 86h
        jmp d_error

s_esperar:                              ; 86h: CX:DX microsegundos
        push eax
        push ebx
        mov bx, cx
        shl ebx, 16
        mov bx, dx
        mov ax, PET_MICROSEG
        call llamar_fw
        pop ebx
        pop eax
        xor ah, ah
        jmp d_bien

s_a20:                                  ; 24h: la A20 está siempre abierta
        cmp al, 2
        jne .ok
        mov al, 1                       ; 2402h: abierta
.ok:    cmp al, 3
        jne .z
        mov bx, 3                       ; 2403h: por teclado y puerto 92h
.z:     xor ah, ah
        jmp d_bien

s_config:                               ; C0h -> ES:BX = tabla de configuración
        mov bx, SEG_BIOS
        mov es, bx
        mov bx, tabla_config
        xor ah, ah
        jmp d_bien

; 87h: mover CX palabras; ES:SI = GDT con origen (+10h) y destino (+18h)
s_mover:
        pushad
        push ds
        push es
        cli
        mov ebx, [es:si + 12h]          ; base del origen (24 bits) + octeto 7
        and ebx, 00FFFFFFh
        movzx eax, byte [es:si + 17h]
        shl eax, 24
        or ebx, eax
        mov edx, [es:si + 1Ah]          ; base del destino
        and edx, 00FFFFFFh
        movzx eax, byte [es:si + 1Fh]
        shl eax, 24
        or edx, eax
        movzx ecx, cx
        mov esi, ebx
        mov edi, edx
        ; a modo protegido con segmentos planos, copiar, y volver
        mov ax, SEG_BIOS
        mov ds, ax
        o32 lgdt [gdtr_real]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp SEL_C16:.p16
.p16:   mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        cld
        a32 rep movsw
        mov ax, SEL_D16
        mov ds, ax
        mov es, ax
        mov eax, cr0
        and al, 0FEh
        mov cr0, eax
        jmp SEG_BIOS:.r16
.r16:   pop es
        pop ds
        popad
        sti
        xor ah, ah
        jmp d_bien

; =============================================================================
;  INT 1Ah - reloj (tics de 40:6C y el reloj CMOS) y BIOS PCI
; =============================================================================
int1a:
        sti
        cmp ah, 00h
        je r_tics
        cmp ah, 01h
        je r_poner_tics
        cmp ah, 02h
        je r_hora
        cmp ah, 04h
        je r_fecha
        cmp ah, 03h
        je r_poner_hora
        cmp ah, 05h
        je r_poner_fecha
        cmp ah, 0B1h
        je pci
        jmp d_error

r_tics:
        push ds
        mov ax, 40h
        mov ds, ax
        cli
        mov dx, [6Ch]
        mov cx, [6Eh]
        mov al, [70h]
        mov byte [70h], 0
        sti
        pop ds
        jmp d_bien
r_poner_tics:
        push ds
        push ax
        mov ax, 40h
        mov ds, ax
        mov [6Ch], dx
        mov [6Eh], cx
        mov byte [70h], 0
        pop ax
        pop ds
        jmp d_bien

; cmos_leer: AL = registro -> AL = valor
cmos_leer:
        out 70h, al
        jmp $ + 2
        in al, 71h
        ret
cmos_escribir:                          ; AL = registro, AH = valor
        out 70h, al
        jmp $ + 2
        mov al, ah
        out 71h, al
        ret
r_espera_cmos:                          ; espera a que no se esté actualizando
        push cx
        mov cx, 0FFFFh
.l:     mov al, 0Ah
        call cmos_leer
        test al, 80h
        loopnz .l
        pop cx
        ret
r_hora:
        call r_espera_cmos
        mov al, 4
        call cmos_leer
        mov ch, al
        mov al, 2
        call cmos_leer
        mov cl, al
        mov al, 0
        call cmos_leer
        mov dh, al
        mov dl, 0
        jmp d_bien
r_fecha:
        call r_espera_cmos
        mov al, 32h
        call cmos_leer
        mov ch, al
        cmp ch, 19h
        jae .s
        mov ch, 20h                     ; sin registro de siglo
.s:     mov al, 9
        call cmos_leer
        mov cl, al
        mov al, 8
        call cmos_leer
        mov dh, al
        mov al, 7
        call cmos_leer
        mov dl, al
        jmp d_bien
r_poner_hora:
        mov al, 4
        mov ah, ch
        call cmos_escribir
        mov al, 2
        mov ah, cl
        call cmos_escribir
        mov al, 0
        mov ah, dh
        call cmos_escribir
        jmp d_bien
r_poner_fecha:
        mov al, 32h
        mov ah, ch
        call cmos_escribir
        mov al, 9
        mov ah, cl
        call cmos_escribir
        mov al, 8
        mov ah, dh
        call cmos_escribir
        mov al, 7
        mov ah, dl
        call cmos_escribir
        jmp d_bien

; ---- BIOS PCI (mecanismo 1: puertos CF8h/CFCh) ----
pci:
        cmp al, 01h
        je .hay
        cmp al, 02h
        je .buscar_disp
        cmp al, 03h
        je .buscar_clase
        cmp al, 08h
        je .leer8
        cmp al, 09h
        je .leer16
        cmp al, 0Ah
        je .leer32
        cmp al, 0Bh
        je .escr8
        cmp al, 0Ch
        je .escr16
        cmp al, 0Dh
        je .escr32
        mov ah, 81h                     ; función no admitida
        jmp d_error
.hay:   mov edx, 20494350h              ; 'PCI '
        mov ax, 0001h                   ; AL = mecanismo 1
        mov bx, 0210h                   ; versión 2.10
        mov cl, 0FFh                    ; último bus
        jmp d_bien
.leer8: call pci_dir
        mov dx, di
        and dx, 3
        add dx, 0CFCh
        in al, dx
        mov cl, al
        jmp .ok
.leer16:
        call pci_dir
        mov dx, di
        and dx, 2
        add dx, 0CFCh
        in ax, dx
        mov cx, ax
        jmp .ok
.leer32:
        call pci_dir
        mov dx, 0CFCh
        in eax, dx
        mov ecx, eax
        jmp .ok
.escr8: call pci_dir
        mov dx, di
        and dx, 3
        add dx, 0CFCh
        mov al, cl
        out dx, al
        jmp .ok
.escr16:
        call pci_dir
        mov dx, di
        and dx, 2
        add dx, 0CFCh
        mov ax, cx
        out dx, ax
        jmp .ok
.escr32:
        call pci_dir
        mov dx, 0CFCh
        mov eax, ecx
        out dx, eax
.ok:    xor ah, ah
        jmp d_bien
; buscar un dispositivo: CX = dispositivo, DX = fabricante, SI = número -> BX
.buscar_disp:
        push eax
        push ecx
        push edx
        push di
        mov [cs:pci_buscar], dx
        mov [cs:pci_buscar + 2], cx
        mov [cs:pci_veces], si
        mov byte [cs:pci_modo], 0
        jmp .recorrer
; buscar por clase: ECX = clase (24 bits), SI = número -> BX
.buscar_clase:
        push eax
        push ecx
        push edx
        push di
        and ecx, 00FFFFFFh
        mov [cs:pci_buscar], ecx
        mov [cs:pci_veces], si
        mov byte [cs:pci_modo], 1
.recorrer:
        xor bx, bx                      ; BH = bus, BL = dispositivo << 3 | función
.cada:  mov di, 0
        call pci_dir
        mov dx, 0CFCh
        in eax, dx
        cmp ax, 0FFFFh
        je .siguiente
        cmp byte [cs:pci_modo], 0
        jne .clase
        cmp eax, [cs:pci_buscar]
        jne .siguiente
        jmp .coincide
.clase: mov di, 8
        call pci_dir
        mov dx, 0CFCh
        in eax, dx
        shr eax, 8
        cmp eax, [cs:pci_buscar]
        jne .siguiente
.coincide:
        cmp word [cs:pci_veces], 0
        je .encontrado
        dec word [cs:pci_veces]
.siguiente:
        inc bx
        jnz .cada
        pop di
        pop edx
        pop ecx
        pop eax
        mov ah, 86h                     ; no encontrado
        jmp d_error
.encontrado:
        pop di
        pop edx
        pop ecx
        pop eax
        xor ah, ah
        jmp d_bien

; pci_dir: BH = bus, BL = disp/función, DI = registro -> dirección en CF8h
pci_dir:
        push eax
        push dx
        movzx eax, bx
        shl eax, 8
        or ax, di
        and al, 0FCh
        or eax, 80000000h
        mov dx, 0CF8h
        out dx, eax
        pop dx
        pop eax
        ret

; =============================================================================
;  INT 11h, 12h, 18h, 19h y las de impresora y serie (sin aparatos)
; =============================================================================
int11:  mov ax, 0022h                   ; coprocesador y vídeo en color 80x25
        iret
int12:  push ds
        mov ax, 40h
        mov ds, ax
        mov ax, [13h]
        pop ds
        iret
int14:  mov ax, 8000h                   ; serie: no responde
        iret
int17:  mov ah, 0                       ; impresora: nada
        iret
int18:  push cs
        pop ds
        mov si, t_sin_sistema
        call escribir_bios
.p:     hlt
        jmp .p
int19:                                  ; carga el MBR del disco 80h y salta
        mov al, 'B'
        call dbg
        xor ax, ax
        mov es, ax
        mov bx, 7C00h
        mov ax, 0201h
        mov cx, 0001h
        mov dx, 0080h
        int 13h
        jc int18
        cmp word [es:7DFEh], 0AA55h
        jne int18
        xor ax, ax
        mov ds, ax
        mov es, ax
        cli
        mov ss, ax
        mov sp, 7C00h
        sti
        mov al, 'J'
        call dbg
        mov dl, 80h
        jmp 0:7C00h

; =============================================================================
;  Tablas del teclado
; =============================================================================
; código de exploración de cada carácter ASCII 20h-7Eh (teclado de EE. UU.,
; que es el que da el firmware; KEYB de Hispa-DOS no interviene aquí)
k_scan:
        db 39h, 02h, 28h, 04h, 05h, 06h, 08h, 28h, 0Ah, 0Bh, 09h, 0Dh, 33h, 0Ch, 34h, 35h   ; 20-2F
        db 0Bh, 02h, 03h, 04h, 05h, 06h, 07h, 08h, 09h, 0Ah, 27h, 27h, 33h, 0Dh, 34h, 35h   ; 30-3F
        db 03h, 1Eh, 30h, 2Eh, 20h, 12h, 21h, 22h, 23h, 17h, 24h, 25h, 26h, 32h, 31h, 18h   ; 40-4F
        db 19h, 10h, 13h, 1Fh, 14h, 16h, 2Fh, 11h, 2Dh, 15h, 2Ch, 1Ah, 2Bh, 1Bh, 07h, 0Ch   ; 50-5F
        db 29h, 1Eh, 30h, 2Eh, 20h, 12h, 21h, 22h, 23h, 17h, 24h, 25h, 26h, 32h, 31h, 18h   ; 60-6F
        db 19h, 10h, 13h, 1Fh, 14h, 16h, 2Fh, 11h, 2Dh, 15h, 2Ch, 1Ah, 2Bh, 1Bh, 29h        ; 70-7E
; códigos de exploración de UEFI (0-18h) -> los de la BIOS (0 = ninguno)
k_esp:  db 0, 48h, 50h, 4Dh, 4Bh, 47h, 4Fh, 52h, 53h, 49h, 51h          ; 0-0Ah
        db 3Bh, 3Ch, 3Dh, 3Eh, 3Fh, 40h, 41h, 42h, 43h, 44h             ; 0Bh-14h F1-F10
        db 85h, 86h, 01h, 0                                             ; 15h-18h
; Unicode -> CP437 para el español (y algunos más)
k_uni:  dw 00E1h
        db 0A0h
        dw 00E9h
        db 82h
        dw 00EDh
        db 0A1h
        dw 00F3h
        db 0A2h
        dw 00FAh
        db 0A3h
        dw 00F1h
        db 0A4h
        dw 00D1h
        db 0A5h
        dw 00FCh
        db 81h
        dw 00DCh
        db 9Ah
        dw 00E7h
        db 87h
        dw 00C7h
        db 80h
        dw 00BFh
        db 0A8h
        dw 00A1h
        db 0ADh
        dw 00BAh
        db 0A7h
        dw 00AAh
        db 0A6h
        dw 00C1h
        db 41h
        dw 00C9h
        db 90h
        dw 00B7h
        db 0FAh
        dw 0

tabla_config:
        dw 8                            ; tamaño
        db 0FCh, 01h, 00h               ; modelo AT, submodelo, revisión
        db 70h, 0, 0, 0, 0              ; RTC, INT 15h 4Fh, EBDA no

t_sin_sistema   db 13, 10, 'HispaBIOS: no hay sistema que arrancar en el disco.', 13, 10, 0

; variables de las interrupciones
d_op            db 0
d_estado        db 0
d_hechos        dw 0
d_n             dw 0
d_buf           dd 0
d_lba           dq 0
tics_dados      dd 0
tsc0            dq 0
en_fw           db 0
pci_buscar      dd 0
pci_veces       dw 0
pci_modo        db 0
        align 4
e820:   times 3 * 20 db 0

; [depuración] dbg: AL por COM1 (3F8h); dbg_hex: AL en hexadecimal
dbg:
        push dx
        push ax
        mov dx, 3FDh
.e:     in al, dx
        test al, 20h
        jz .e
        pop ax
        mov dx, 3F8h
        out dx, al
        pop dx
        ret
dbg_hex:
        push ax
        shr al, 4
        call .d
        pop ax
        push ax
        and al, 0Fh
        call .d
        pop ax
        ret
.d:     add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:     jmp dbg
dbg_t   db 0
