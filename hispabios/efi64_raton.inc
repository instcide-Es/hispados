; =============================================================================
;  HispaBIOS (x64) - el ratón con UEFI (lo incluye efi64.asm)
; =============================================================================
;  Arrancando por UEFI no hay ratón PS/2 (no hay emulación del USB «heredado»,
;  y los portátiles llevan el panel táctil por USB, I2C o SPI): HSPRATON no
;  encontraba nada. Aquí se busca el ratón por dos caminos y la BIOS de modo
;  real lo da a DOS por INT 15h C2xxh (bios_int.inc, s_raton), como el ratón
;  PS/2 de una BIOS de las de siempre:
;
;   1. El que da el FIRMWARE: EFI_SIMPLE_POINTER_PROTOCOL (ratones; el panel
;      táctil de los Mac, si el firmware lo lleva) y EFI_ABSOLUTE_POINTER_
;      PROTOCOL (pantallas táctiles, tabletas). Antes se pide al firmware que
;      ponga en marcha sus controladoras USB (ConnectController recursivo):
;      arrancando «deprisa» solo enciende lo que hace falta para arrancar.
;   2. Un RATÓN USB que el firmware no sabe llevar (muchos firmware no tienen
;      controlador de ratón; OVMF tampoco): las interfaces HID de ratón de
;      arranque (clase 03h, subclase 01h, protocolo 02h) que no tenga ningún
;      controlador del firmware. Se le pide el protocolo de arranque
;      (SET_PROTOCOL: informes de botones, X, Y y rueda) y se le pone una
;      transferencia de interrupción asíncrona (UsbAsyncInterruptTransfer)
;      por EFI_USB_IO_PROTOCOL, el mismo camino que usa el teclado USB del
;      firmware: el firmware atiende el USB mientras HispaBIOS está en un
;      viaje y llama a raton_cb con cada informe.
;
;  En cada viaje al firmware con el ratón activado (T_RAT_ON: el tic de la BIOS
;  hace uno si no tenía ya que dibujar; el teclado, los suyos), raton_sondear
;  pregunta a los del firmware (GetState) y suma el movimiento en T_RAT_DX/DY
;  (cuentas del PS/2: X a la derecha, Y hacia arriba) y los botones en
;  T_RAT_BOT; raton_cb hace lo mismo con los informes del ratón USB. Lo que
;  hay al arrancar va al registro (HISPABIOS.LOG). En el modo seguro no se
;  conectan las controladoras USB ni se pone el ratón USB propio.
;
;  Las cuentas: un Simple Pointer dice sus cuentas por milímetro
;  (ResolutionX/Y); hasta 8 se pasan tal cual (lo que hace la emulación PS/2
;  de las BIOS con un ratón USB) y, si son más (paneles de alta resolución),
;  se llevan a 8 por milímetro. Un Absolute Pointer da la posición: se pasa a
;  movimiento, con todo el ancho (y todo el alto) = 1024 cuentas.
;  (Especificación UEFI 2.x, 12.5, 12.7 y 17.2; USB HID 1.11, apéndice B.)
; =============================================================================

RT_MAX          equ 8           ; fuentes (ratones) como mucho
RT_TAM          equ 96          ; octetos de cada una en rt_fuentes
; cada fuente:
RT_TIPO         equ 0           ; 1 Simple Pointer, 2 Absolute Pointer, 3 ratón USB propio
RT_PROT         equ 8           ; el protocolo (SIMPLE_POINTER, ABSOLUTE_POINTER o USB_IO)
RT_RESX         equ 16          ; Simple: resolución X; Absolute: mínimo X
RT_RESY         equ 24          ; Simple: resolución Y; Absolute: mínimo Y
RT_MAXX         equ 32          ; Absolute: máximo X e Y
RT_MAXY         equ 40
RT_ULTX         equ 48          ; Absolute: la última posición
RT_ULTY         equ 56
RT_BOT          equ 64          ; sus botones (bit 0 izquierdo, 1 derecho, 2 central)
RT_EP           equ 65          ; USB: el punto final de interrupción (entrada)
RT_FALLO        equ 66          ; USB: 1 = el último informe dio error (se rehace)
RT_LARGO        equ 67          ; USB: octetos del informe
RT_VISTO        equ 68          ; Absolute: 1 = ya hay una posición anterior
RT_RESTOX       equ 72          ; lo que sobra al pasar a cuentas (dd)
RT_RESTOY       equ 76
RT_INTERV       equ 80          ; USB: cada cuántos ms se pregunta (dd)
RT_IFAZ         equ 84          ; USB: el número de la interfaz
; EFI_SIMPLE_POINTER_PROTOCOL / EFI_ABSOLUTE_POINTER_PROTOCOL (x64)
SP_GETSTATE     equ 8
SP_MODE         equ 24
; EFI_USB_IO_PROTOCOL (x64)
UIO_CONTROL     equ 0
UIO_ASYNCINT    equ 16
UIO_IFAZ        equ 64
UIO_EP          equ 72
EFI_NO_LISTO    equ 8000000000000006h   ; EFI_NOT_READY
ABRIR_DRIVER    equ 10h                 ; EFI_OPEN_PROTOCOL_BY_DRIVER

; ---- raton_preparar: busca los ratones (al arrancar, antes de la BIOS) ----
raton_preparar:
        push rbp
        mov rbp, rsp
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        and rsp, -16
        sub rsp, 64
        mov byte [rel rt_n], 0
        DI t_rt_tit
        cmp byte [rel seguro], 0
        jne .sin_usb
        mov word [rel pc_clase], 0C03h  ; las controladoras USB (clase 0C03h)
        mov word [rel pc_clase2], 0C03h
        lea rax, [rel t_rt_usb]
        mov [rel pc_texto], rax
        call pci_conectar
.sin_usb:
        ; ---- 1. los del firmware: Simple Pointer y Absolute Pointer ----
        mov qword [rel rt_guid], 1
.prot:  mov rax, [rel bootserv]
        mov ecx, 2                      ; ByProtocol
        lea rdx, [rel guid_simple]
        cmp qword [rel rt_guid], 1
        je .g
        lea rdx, [rel guid_absoluto]
.g:     xor r8d, r8d
        lea r9, [rel pc_n]
        lea r10, [rel pc_man]
        mov [rsp + 32], r10
        FW [rax + BS_LOCATEHBUF]
        test rax, rax
        jnz .prot_sig
        xor r12d, r12d
.cada:  cmp r12, [rel pc_n]
        jae .prot_libera
        mov rax, [rel pc_man]
        mov r13, [rax + r12 * 8]        ; R13 = el manejador
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_simple]
        cmp qword [rel rt_guid], 1
        je .g2
        lea rdx, [rel guid_absoluto]
.g2:    lea r8, [rel rt_p]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .otro
        ; (el de la consola de entrada no: es el que reúne a los demás —en
        ; EDK II, ConSplitter—, y lo hay aunque no haya ningún ratón)
        mov rax, [rel sistema]
        cmp r13, [rax + ST_CONINH]
        jne .apunta
        DI t_rt_consola
        jmp .otro
.apunta:
        call rt_nueva                   ; RBX = la fuente (o CF: no caben)
        jc .otro
        mov rax, [rel rt_p]
        mov [rbx + RT_PROT], rax
        mov r14, [rax + SP_MODE]
        cmp qword [rel rt_guid], 1
        jne .abs
        mov byte [rbx + RT_TIPO], 1
        mov rax, [r14]                  ; ResolutionX, Y
        mov [rbx + RT_RESX], rax
        mov rax, [r14 + 8]
        mov [rbx + RT_RESY], rax
        DI t_rt_simple
        mov rax, [r14]
        call decimal
        DI t_rt_coma
        mov rax, [r14 + 8]
        call decimal
        DI t_rt_coma
        mov rax, [r14 + 16]
        call decimal
        DI t_rt_botones
        movzx eax, byte [r14 + 24]
        call decimal
        DI t_rt_coma
        movzx eax, byte [r14 + 25]
        call decimal
        DI t_nl
        or byte [rel rt_hay], 1
        jmp .otro
.abs:   mov byte [rbx + RT_TIPO], 2
        mov rax, [r14]                  ; AbsoluteMinX, MinY
        mov [rbx + RT_RESX], rax
        mov rax, [r14 + 8]
        mov [rbx + RT_RESY], rax
        mov rax, [r14 + 24]             ; AbsoluteMaxX, MaxY
        mov [rbx + RT_MAXX], rax
        mov rax, [r14 + 32]
        mov [rbx + RT_MAXY], rax
        DI t_rt_absoluto
        mov rax, [r14]
        call decimal
        DI t_rt_a
        mov rax, [r14 + 24]
        call decimal
        DI t_rt_y
        mov rax, [r14 + 8]
        call decimal
        DI t_rt_a
        mov rax, [r14 + 32]
        call decimal
        DI t_rt_atrib
        mov eax, [r14 + 48]
        mov ecx, 8
        call hex
        DI t_nl
        or byte [rel rt_hay], 2
.otro:  inc r12
        jmp .cada
.prot_libera:
        mov rax, [rel bootserv]
        mov rcx, [rel pc_man]
        FW [rax + BS_FREEPOOL]
.prot_sig:
        inc qword [rel rt_guid]
        cmp qword [rel rt_guid], 2
        jbe .prot
        ; ---- 2. los ratones USB que no lleva el firmware ----
        cmp byte [rel seguro], 0
        jne .fin
        mov rax, [rel bootserv]
        mov ecx, 2
        lea rdx, [rel guid_usbio]
        xor r8d, r8d
        lea r9, [rel pc_n]
        lea r10, [rel pc_man]
        mov [rsp + 32], r10
        FW [rax + BS_LOCATEHBUF]
        test rax, rax
        jnz .fin
        xor r12d, r12d
.u:     cmp r12, [rel pc_n]
        jae .u_libera
        mov rax, [rel pc_man]
        mov r13, [rax + r12 * 8]
        call rt_usb                     ; R13 = el manejador
        inc r12
        jmp .u
.u_libera:
        mov rax, [rel bootserv]
        mov rcx, [rel pc_man]
        FW [rax + BS_FREEPOOL]
.fin:   cmp byte [rel rt_n], 0
        jne .hay
        DI t_rt_nada
.hay:   mov rax, [rel bios_base]
        mov cl, [rel rt_hay]
        mov [rax + 284], cl             ; T_RAT_HAY
        lea rsp, [rbp - 48]
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        ret

; rt_nueva -> RBX = una fuente nueva, a cero (CF: ya hay RT_MAX)
rt_nueva:
        movzx eax, byte [rel rt_n]
        cmp eax, RT_MAX
        jae .no
        imul eax, eax, RT_TAM
        lea rbx, [rel rt_fuentes]
        add rbx, rax
        push rdi
        push rcx
        mov rdi, rbx
        mov ecx, RT_TAM
        xor eax, eax
        cld
        rep stosb
        pop rcx
        pop rdi
        inc byte [rel rt_n]
        clc
        ret
.no:    stc
        ret

; rt_usb: R13 = un manejador con EFI_USB_IO_PROTOCOL; si es una interfaz de
; ratón de arranque que no lleva el firmware, se pone en marcha
rt_usb:
        push rbp
        mov rbp, rsp
        push rbx
        push rsi
        push rdi
        push r14
        and rsp, -16
        sub rsp, 64
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_usbio]
        lea r8, [rel rt_p]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .f
        mov rcx, [rel rt_p]
        lea rdx, [rel rt_ifaz]
        FW [rcx + UIO_IFAZ]             ; UsbGetInterfaceDescriptor
        test rax, rax
        jnz .f
        cmp byte [rel rt_ifaz + 5], 03h ; HID
        jne .f
        cmp byte [rel rt_ifaz + 7], 02h ; protocolo: ratón
        jne .f
        DI t_rt_hid
        movzx eax, byte [rel rt_ifaz + 2]
        call decimal
        DI t_rt_hid2
        movzx eax, byte [rel rt_ifaz + 6]
        mov ecx, 2
        call hex
        ; ¿lo lleva ya el firmware? (su Simple Pointer ya se ha apuntado)
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_simple]
        lea r8, [rel rt_p2]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .sin_sp
        DI t_rt_ya_fw
        jmp .f
.sin_sp:
        ; ¿lo tiene abierto un controlador del firmware?
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_usbio]
        lea r8, [rel rt_info]
        lea r9, [rel rt_info_n]
        FW [rax + BS_OPENPROTINFO]
        test rax, rax
        jnz .libre
        xor ebx, ebx                    ; EBX = 1 si alguno lo abre como controlador
        mov rsi, [rel rt_info]
        mov rcx, [rel rt_info_n]
.oi:    test rcx, rcx
        jz .oi_f
        test dword [rsi + 16], ABRIR_DRIVER
        jz .oi_s
        mov ebx, 1
.oi_s:  add rsi, 24
        dec rcx
        jmp .oi
.oi_f:  mov rax, [rel bootserv]
        mov rcx, [rel rt_info]
        FW [rax + BS_FREEPOOL]
        test ebx, ebx
        jz .libre
        DI t_rt_ocupado
        jmp .f
.libre: cmp byte [rel rt_ifaz + 6], 01h ; los de arranque (subclase 01h); los
        je .arranque                    ; demás, si aceptan el protocolo de arranque
        mov word [rel rt_pet], 0B21h    ; (un ratón que no lo tiene debe negarse:
        mov word [rel rt_pet + 2], 0    ; HID 1.11, 7.2.6)
        movzx eax, byte [rel rt_ifaz + 2]
        mov [rel rt_pet + 4], ax
        mov word [rel rt_pet + 6], 0
        call rt_control
        test rax, rax
        jnz .no_arr
        cmp dword [rel rt_estado], 0
        je .arranque
.no_arr:
        DI t_rt_no_arr
        jmp .f
.arranque:
        ; el punto final de interrupción de entrada
        xor r14d, r14d
.ep:    movzx eax, byte [rel rt_ifaz + 4]    ; NumEndpoints
        cmp r14d, eax
        jae .sin_ep
        mov rcx, [rel rt_p]
        mov edx, r14d
        lea r8, [rel rt_epd]
        FW [rcx + UIO_EP]               ; UsbGetEndpointDescriptor
        test rax, rax
        jnz .ep_s
        mov al, [rel rt_epd + 3]        ; Attributes: 3 = interrupción
        and al, 3
        cmp al, 3
        jne .ep_s
        test byte [rel rt_epd + 2], 80h ; de entrada
        jnz .ep_si
.ep_s:  inc r14d
        jmp .ep
.sin_ep:
        DI t_rt_sin_ep
        jmp .f
.ep_si: call rt_nueva
        jc .f
        mov byte [rbx + RT_TIPO], 3
        mov rax, [rel rt_p]
        mov [rbx + RT_PROT], rax
        mov al, [rel rt_epd + 2]
        mov [rbx + RT_EP], al
        movzx eax, word [rel rt_epd + 4] ; MaxPacketSize: el informe, hasta 8
        and eax, 7FFh
        cmp eax, 8
        jbe .lg
        mov eax, 8
.lg:    cmp eax, 3
        jae .lg2
        mov eax, 3
.lg2:   mov [rbx + RT_LARGO], al
        movzx eax, byte [rel rt_epd + 6] ; Interval (ms; lo que usa el teclado USB)
        test eax, eax
        jnz .iv
        mov eax, 8
.iv:    mov [rbx + RT_INTERV], eax
        movzx eax, byte [rel rt_ifaz + 2]
        mov [rbx + RT_IFAZ], eax
        DI t_rt_ep
        movzx eax, byte [rbx + RT_EP]
        mov ecx, 2
        call hex
        DI t_rt_ep2
        movzx eax, byte [rbx + RT_LARGO]
        call decimal
        DI t_rt_ep3
        mov eax, [rbx + RT_INTERV]
        call decimal
        DI t_rt_ep4
        ; SET_PROTOCOL (arranque) y SET_IDLE (solo cuando cambia algo)
        mov word [rel rt_pet], 0B21h    ; tipo 21h (clase, interfaz), petición 0Bh
        mov word [rel rt_pet + 2], 0    ; wValue = 0: el protocolo de arranque
        mov eax, [rbx + RT_IFAZ]
        mov [rel rt_pet + 4], ax
        mov word [rel rt_pet + 6], 0
        call rt_control
        DI t_rt_proto
        mov eax, [rel rt_estado]
        mov ecx, 8
        call hex
        mov word [rel rt_pet], 0A21h    ; SET_IDLE: duración 0 (indefinida)
        call rt_control
        DI t_rt_idle
        mov eax, [rel rt_estado]
        mov ecx, 8
        call hex
        call rt_usb_poner               ; la transferencia asíncrona
        DI t_rt_async
        mov ecx, 8
        call hex
        DI t_nl
        test rax, rax
        jz .puesto
        dec byte [rel rt_n]             ; no se ha podido: fuera
        jmp .f
.puesto:
        or byte [rel rt_hay], 4
.f:     lea rsp, [rbp - 32]
        pop r14
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        ret

; rt_control: la petición rt_pet (sin datos) a la interfaz de rt_p -> RAX y rt_estado
rt_control:
        push rbp
        mov rbp, rsp
        and rsp, -16
        sub rsp, 64
        mov rcx, [rel rt_p]
        lea rdx, [rel rt_pet]
        mov r8d, 2                      ; EfiUsbNoData
        mov r9d, 100                    ; 100 ms
        mov qword [rsp + 32], 0
        mov qword [rsp + 40], 0
        lea rax, [rel rt_estado]
        mov [rsp + 48], rax
        mov dword [rel rt_estado], 0
        FW [rcx + UIO_CONTROL]
        mov rsp, rbp
        pop rbp
        ret

; rt_usb_poner: RBX = una fuente USB -> pone (o rehace) su transferencia de
; interrupción asíncrona; RAX = resultado
rt_usb_poner:
        push rbp
        mov rbp, rsp
        and rsp, -16
        sub rsp, 64
        mov rcx, [rbx + RT_PROT]
        movzx edx, byte [rbx + RT_EP]
        mov r8d, 1                      ; IsNewTransfer
        mov r9d, [rbx + RT_INTERV]
        movzx eax, byte [rbx + RT_LARGO]
        mov [rsp + 32], rax
        lea rax, [rel raton_cb]
        mov [rsp + 40], rax
        mov [rsp + 48], rbx             ; Context = la fuente
        FW [rcx + UIO_ASYNCINT]
        mov rsp, rbp
        pop rbp
        ret

; rt_usb_quitar: RBX = una fuente USB -> quita su transferencia asíncrona
rt_usb_quitar:
        push rbp
        mov rbp, rsp
        and rsp, -16
        sub rsp, 64
        mov rcx, [rbx + RT_PROT]
        movzx edx, byte [rbx + RT_EP]
        xor r8d, r8d                    ; IsNewTransfer = FALSE: quitarla
        xor r9d, r9d
        mov qword [rsp + 32], 0
        mov qword [rsp + 40], 0
        mov qword [rsp + 48], 0
        FW [rcx + UIO_ASYNCINT]
        mov rsp, rbp
        pop rbp
        ret

; ---- raton_cb: el firmware, con cada informe del ratón USB propio ----
; (EFI_ASYNC_USB_TRANSFER_CALLBACK: RCX = datos, RDX = octetos, R8 = la
; fuente, R9 = estado de la transferencia; vuelve con EFI_SUCCESS.) Informe
; de arranque: botones, X, Y (hacia abajo) y, si viene, la rueda.
raton_cb:
        push rbx
        mov rbx, r8
        test r9d, r9d                   ; error del USB: se rehace en raton_sondear
        jz .bien
        mov byte [rbx + RT_FALLO], 1
        jmp .f
.bien:  cmp rdx, 3
        jb .f
        test rcx, rcx
        jz .f
        pushfq
        cli
        mov r8, [rel bios_base]
        inc dword [r8 + 308]            ; T_RAT_CB
        mov al, [rcx]
        and al, 7
        mov [rbx + RT_BOT], al
        cmp byte [r8 + 285], 0          ; T_RAT_ON: sin activar, solo los botones
        je .bot
        movsx eax, byte [rcx + 1]
        add [r8 + 288], eax             ; T_RAT_DX
        movsx eax, byte [rcx + 2]
        sub [r8 + 292], eax             ; T_RAT_DY (el PS/2, hacia arriba)
        cmp rdx, 4
        jb .bot
        movsx eax, byte [rcx + 3]       ; la rueda (en el USB, positivo hacia fuera)
        sub [r8 + 296], eax
.bot:   call rt_botones
        mov byte [r8 + 287], 1          ; T_RAT_NUEVO
        popfq
.f:     xor eax, eax
        pop rbx
        ret

; rt_botones: T_RAT_BOT = los botones de todas las fuentes (R8 = la BIOS)
rt_botones:
        push rcx
        push rdx
        xor eax, eax
        movzx ecx, byte [rel rt_n]
        lea rdx, [rel rt_fuentes]
.b:     test ecx, ecx
        jz .f
        or al, [rdx + RT_BOT]
        add rdx, RT_TAM
        dec ecx
        jmp .b
.f:     mov [r8 + 286], al              ; T_RAT_BOT
        pop rdx
        pop rcx
        ret

; ---- raton_sondear: en cada viaje con el ratón activado, lo que digan los
;      del firmware (y los USB que hayan dado error, otra vez en marcha) ----
raton_sondear:
        push rbp
        mov rbp, rsp
        push rbx
        push rsi
        push rdi
        push r12
        and rsp, -16
        sub rsp, 64
        xor r12d, r12d
.c:     cmp r12b, [rel rt_n]
        jae .f
        movzx eax, r12b
        imul eax, eax, RT_TAM
        lea rbx, [rel rt_fuentes]
        add rbx, rax
        cmp byte [rbx + RT_TIPO], 1
        je .simple
        cmp byte [rbx + RT_TIPO], 2
        je .absoluto
        cmp byte [rbx + RT_FALLO], 0    ; USB: ¿hay que rehacerla?
        je .sig
        mov byte [rbx + RT_FALLO], 0
        sti
        call rt_usb_quitar
        call rt_usb_poner
        cli
        jmp .sig
.simple:
        mov rcx, [rbx + RT_PROT]
        lea rdx, [rel rt_estado_sp]
        sti
        FW [rcx + SP_GETSTATE]
        cli
        test rax, rax
        jnz .sig
        mov r8, [rel bios_base]
        inc dword [r8 + 300]            ; T_RAT_LEC
        mov eax, [rel rt_estado_sp]     ; RelativeMovementX
        mov rcx, [rbx + RT_RESX]
        lea rsi, [rbx + RT_RESTOX]
        call rt_escalar
        add [r8 + 288], eax
        mov eax, [rel rt_estado_sp + 4] ; RelativeMovementY (hacia abajo)
        mov rcx, [rbx + RT_RESY]
        lea rsi, [rbx + RT_RESTOY]
        call rt_escalar
        sub [r8 + 292], eax
        mov eax, [rel rt_estado_sp + 8] ; la rueda
        sub [r8 + 296], eax
        xor eax, eax
        cmp byte [rel rt_estado_sp + 12], 0     ; izquierdo
        je .s1
        or al, 1
.s1:    cmp byte [rel rt_estado_sp + 13], 0     ; derecho
        je .s2
        or al, 2
.s2:    mov [rbx + RT_BOT], al
        jmp .nuevo
.absoluto:
        mov rcx, [rbx + RT_PROT]
        lea rdx, [rel rt_estado_ap]
        sti
        FW [rcx + SP_GETSTATE]
        cli
        test rax, rax
        jnz .sig
        mov r8, [rel bios_base]
        inc dword [r8 + 300]            ; T_RAT_LEC
        mov rax, [rel rt_estado_ap]     ; CurrentX
        mov rdx, [rel rt_estado_ap + 8] ; CurrentY
        cmp byte [rbx + RT_VISTO], 0
        mov byte [rbx + RT_VISTO], 1
        xchg rax, [rbx + RT_ULTX]
        xchg rdx, [rbx + RT_ULTY]
        je .ab_bot                      ; (la primera: solo se apunta)
        neg rax
        add rax, [rbx + RT_ULTX]        ; RAX = lo que se ha movido en X
        neg rdx
        add rdx, [rbx + RT_ULTY]
        mov rdi, rdx
        mov rcx, [rbx + RT_MAXX]
        sub rcx, [rbx + RT_RESX]
        lea rsi, [rbx + RT_RESTOX]
        call rt_absoluto
        add [r8 + 288], eax
        mov rax, rdi
        mov rcx, [rbx + RT_MAXY]
        sub rcx, [rbx + RT_RESY]
        lea rsi, [rbx + RT_RESTOY]
        call rt_absoluto
        sub [r8 + 292], eax
.ab_bot:
        mov eax, [rel rt_estado_ap + 24] ; ActiveButtons: bit 0 (tocar), 1 (el otro)
        and al, 3
        mov [rbx + RT_BOT], al
.nuevo: call rt_botones
        mov byte [r8 + 287], 1          ; T_RAT_NUEVO
.sig:   inc r12d
        jmp .c
.f:     lea rsp, [rbp - 32]
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        ret

; rt_escalar: EAX = cuentas del firmware, RCX = sus cuentas por mm, RSI = el
; resto que se lleva -> EAX = cuentas para el PS/2 (hasta 8 por mm, tal cual)
rt_escalar:
        cmp rcx, 8
        jbe .tal
        push rdx
        movsxd rax, eax
        shl rax, 3                      ; a 8 por mm
        movsxd rdx, dword [rsi]
        add rax, rdx
        cqo
        idiv rcx
        mov [rsi], edx
        pop rdx
.tal:   ret

; rt_absoluto: RAX = lo que se ha movido, RCX = el ancho del Absolute
; Pointer, RSI = el resto -> EAX = cuentas (todo el ancho = 1024)
rt_absoluto:
        test rcx, rcx
        jle .cero
        push rdx
        shl rax, 10
        movsxd rdx, dword [rsi]
        add rax, rdx
        cqo
        idiv rcx
        mov [rsi], edx
        pop rdx
        ret
.cero:  xor eax, eax
        ret

; ---- textos y datos ----
t_rt_tit        db 'Raton:', 13, 10, 0
t_rt_usb        db '  Controladora USB ', 0
t_rt_consola    db '  (el de la consola, que reune a los demas, no se usa)', 13, 10, 0
t_rt_simple     db '  Del firmware (Simple Pointer): cuentas por mm X, Y, Z = ', 0
t_rt_coma       db ', ', 0
t_rt_botones    db '; botones izquierdo, derecho = ', 0
t_rt_absoluto   db '  Del firmware (Absolute Pointer): X de ', 0
t_rt_a          db ' a ', 0
t_rt_y          db ', Y de ', 0
t_rt_atrib      db '; atributos ', 0
t_rt_hid        db '  USB: interfaz HID de raton n.', 0
t_rt_hid2       db ', subclase ', 0
t_rt_ya_fw      db 'h: la lleva el firmware (su Simple Pointer)', 13, 10, 0
t_rt_ocupado    db 'h: la tiene un controlador del firmware sin Simple Pointer: no se toca', 13, 10, 0
t_rt_no_arr     db 'h: no es de arranque ni acepta su protocolo (informes de 3 octetos): no se usa', 13, 10, 0
t_rt_sin_ep     db 'h: sin punto final de interrupcion de entrada', 13, 10, 0
t_rt_ep         db 'h: la lleva HispaBIOS; punto final ', 0
t_rt_ep2        db 'h, informes de ', 0
t_rt_ep3        db ' octetos cada ', 0
t_rt_ep4        db ' ms', 0
t_rt_proto      db '; SET_PROTOCOL (arranque): ', 0
t_rt_idle       db ', SET_IDLE: ', 0
t_rt_async      db ', transferencia asincrona: ', 0
t_rt_nada       db '  No hay raton: ni el firmware da uno ni hay un raton USB de arranque', 13, 10, 0
        align 4
guid_simple     dd 31878C87h            ; EFI_SIMPLE_POINTER_PROTOCOL
                dw 0B75h, 11D5h
                db 9Ah, 4Fh, 00h, 90h, 27h, 3Fh, 0C1h, 4Dh
guid_absoluto   dd 8D59D32Bh            ; EFI_ABSOLUTE_POINTER_PROTOCOL
                dw 0C655h, 4AE9h
                db 9Bh, 15h, 0F2h, 59h, 04h, 99h, 2Ah, 43h
guid_usbio      dd 2B2F68D6h            ; EFI_USB_IO_PROTOCOL
                dw 0CD2h, 44CFh
                db 8Eh, 8Bh, 0BBh, 0A2h, 0Bh, 1Bh, 5Bh, 75h
        align 8
rt_guid         dq 0                    ; 1 = buscando Simple Pointer; 2 = Absolute
rt_p            dq 0
rt_p2           dq 0
rt_info         dq 0                    ; OpenProtocolInformation
rt_info_n       dq 0
rt_estado_sp    dd 0, 0, 0, 0           ; EFI_SIMPLE_POINTER_STATE
rt_estado_ap    dq 0, 0, 0, 0           ; EFI_ABSOLUTE_POINTER_STATE
rt_estado       dd 0
rt_pet          dw 0, 0, 0, 0           ; EFI_USB_DEVICE_REQUEST
rt_ifaz         times 10 db 0           ; EFI_USB_INTERFACE_DESCRIPTOR
rt_epd          times 8 db 0            ; EFI_USB_ENDPOINT_DESCRIPTOR
rt_n            db 0                    ; fuentes
rt_hay          db 0                    ; T_RAT_HAY
        align 8
rt_fuentes      times RT_MAX * RT_TAM db 0
