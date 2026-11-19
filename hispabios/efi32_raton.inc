; =============================================================================
;  HispaBIOS (IA32) - el ratón con UEFI (lo incluye efi32.asm)
; =============================================================================
;  Lo mismo que efi64_raton.inc (allí está explicado): los ratones del
;  firmware (Simple Pointer y Absolute Pointer, tras poner en marcha las
;  controladoras USB) y los ratones USB de arranque que no lleva el firmware
;  (transferencia de interrupción asíncrona por EFI_USB_IO_PROTOCOL). En cada
;  viaje con el ratón activado se suma lo que se ha movido en T_RAT_* de la
;  BIOS, que lo da a DOS por INT 15h C2xxh. Convención de C (efi32.asm): el
;  firmware llama a raton_cb con los argumentos en la pila y EBP suyo.
; =============================================================================

RT_MAX          equ 8
RT_TAM          equ 96
RT_TIPO         equ 0           ; 1 Simple Pointer, 2 Absolute Pointer, 3 ratón USB propio
RT_PROT         equ 8
RT_RESX         equ 16          ; Simple: resolución X; Absolute: mínimo X
RT_RESY         equ 24
RT_MAXX         equ 32
RT_MAXY         equ 40
RT_ULTX         equ 48
RT_ULTY         equ 56
RT_BOT          equ 64
RT_EP           equ 65
RT_FALLO        equ 66
RT_LARGO        equ 67
RT_VISTO        equ 68
RT_RESTOX       equ 72
RT_RESTOY       equ 76
RT_INTERV       equ 80
RT_IFAZ         equ 84
SP_GETSTATE     equ 4           ; EFI_SIMPLE_POINTER / EFI_ABSOLUTE_POINTER (IA32)
SP_MODE         equ 12
UIO_CONTROL     equ 0           ; EFI_USB_IO_PROTOCOL (IA32)
UIO_ASYNCINT    equ 8
UIO_IFAZ        equ 32
UIO_EP          equ 36
BS_OPENPROTINFO equ 160         ; OpenProtocolInformation
ABRIR_DRIVER    equ 10h

; ---- raton_preparar: busca los ratones (al arrancar, antes de la BIOS) ----
raton_preparar:
        pushad
        mov byte [V(rt_n)], 0
        DI t_rt_tit
        cmp byte [V(seguro)], 0
        jne .sin_usb
        call usb_conectar
.sin_usb:
        ; ---- 1. los del firmware: Simple Pointer y Absolute Pointer ----
        mov dword [V(rt_guid)], 1
.prot:  lea eax, [V(guid_simple)]
        cmp dword [V(rt_guid)], 1
        je .g
        lea eax, [V(guid_absoluto)]
.g:     mov [V(rt_g)], eax
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, [V(rt_g)], 0, &rt_hn, &rt_hman
        test eax, eax
        jnz .prot_sig
        mov dword [V(rt_i)], 0
.cada:  mov eax, [V(rt_i)]
        cmp eax, [V(rt_hn)]
        jae .prot_libera
        mov ecx, [V(rt_hman)]
        mov edi, [ecx + eax * 4]        ; EDI = el manejador
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], edi, [V(rt_g)], &rt_p
        test eax, eax
        jnz .otro
        ; (el de la consola de entrada no: reúne a los demás y lo hay siempre)
        mov eax, [V(sistema)]
        cmp edi, [eax + ST_CONINH]
        jne .apunta
        DI t_rt_consola
        jmp .otro
.apunta:
        call rt_nueva                   ; EBX = la fuente (o CF)
        jc .otro
        mov eax, [V(rt_p)]
        mov [ebx + RT_PROT], eax
        mov esi, [eax + SP_MODE]
        cmp dword [V(rt_guid)], 1
        jne .abs
        mov byte [ebx + RT_TIPO], 1
        mov eax, [esi]                  ; ResolutionX, Y (la parte baja)
        mov edx, [esi + 4]
        test edx, edx
        jz .rx
        mov eax, -1
.rx:    mov [ebx + RT_RESX], eax
        mov eax, [esi + 8]
        mov edx, [esi + 12]
        test edx, edx
        jz .ry
        mov eax, -1
.ry:    mov [ebx + RT_RESY], eax
        DI t_rt_simple
        mov eax, [esi]
        call decimal
        DI t_rt_coma
        mov eax, [esi + 8]
        call decimal
        DI t_rt_coma
        mov eax, [esi + 16]
        call decimal
        DI t_rt_botones
        movzx eax, byte [esi + 24]
        call decimal
        DI t_rt_coma
        movzx eax, byte [esi + 25]
        call decimal
        DI t_nl
        or byte [V(rt_hay)], 1
        jmp .otro
.abs:   mov byte [ebx + RT_TIPO], 2
        mov eax, [esi]                  ; AbsoluteMinX, MinY, MaxX, MaxY
        mov [ebx + RT_RESX], eax
        mov eax, [esi + 8]
        mov [ebx + RT_RESY], eax
        mov eax, [esi + 24]
        mov [ebx + RT_MAXX], eax
        mov eax, [esi + 32]
        mov [ebx + RT_MAXY], eax
        DI t_rt_absoluto
        mov eax, [esi]
        call decimal
        DI t_rt_a
        mov eax, [esi + 24]
        call decimal
        DI t_rt_y
        mov eax, [esi + 8]
        call decimal
        DI t_rt_a
        mov eax, [esi + 32]
        call decimal
        DI t_rt_atrib
        mov eax, [esi + 48]
        mov ecx, 8
        call hex
        DI t_nl
        or byte [V(rt_hay)], 2
.otro:  inc dword [V(rt_i)]
        jmp .cada
.prot_libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(rt_hman)]
.prot_sig:
        inc dword [V(rt_guid)]
        cmp dword [V(rt_guid)], 2
        jbe .prot
        ; ---- 2. los ratones USB que no lleva el firmware ----
        cmp byte [V(seguro)], 0
        jne .fin
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_usbio, 0, &rt_hn, &rt_hman
        test eax, eax
        jnz .fin
        mov dword [V(rt_i)], 0
.u:     mov eax, [V(rt_i)]
        cmp eax, [V(rt_hn)]
        jae .u_libera
        mov ecx, [V(rt_hman)]
        mov edi, [ecx + eax * 4]
        call rt_usb                     ; EDI = el manejador
        inc dword [V(rt_i)]
        jmp .u
.u_libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(rt_hman)]
.fin:   cmp byte [V(rt_n)], 0
        jne .hay
        DI t_rt_nada
.hay:   mov eax, [V(bios_base)]
        mov cl, [V(rt_hay)]
        mov [eax + 284], cl             ; T_RAT_HAY
        popad
        ret

; usb_conectar: que el firmware ponga en marcha sus controladoras USB (los
; dispositivos PCI de clase 0C03h): ConnectController, recursivo
usb_conectar:
        pushad
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_pciio, 0, &rt_hn, &rt_hman
        test eax, eax
        jnz .f
        xor esi, esi
.c:     cmp esi, [V(rt_hn)]
        jae .libera
        mov ecx, [V(rt_hman)]
        mov ebx, [ecx + esi * 4]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], ebx, &guid_pciio, &dc_pci
        test eax, eax
        jnz .sig
        mov ecx, [V(dc_pci)]
        FW [ecx + PCIIO_LEER], ecx, 2, 0, 4, &dc_cfg
        test eax, eax
        jnz .sig
        cmp word [V(dc_cfg) + 10], 0C03h
        jne .sig
        DI t_rt_usb
        movzx eax, word [V(dc_cfg)]
        mov ecx, 4
        call hex
        DI t_dc2
        movzx eax, word [V(dc_cfg) + 2]
        call hex
        DI t_dc3
        movzx eax, word [V(dc_cfg) + 10]
        call hex
        DI t_dc4
        call registro_paso
        mov eax, [V(bootserv)]
        FW [eax + BS_CONNECT], ebx, 0, 0, 1
        mov ecx, 8
        call hex
        DI t_nl
.sig:   inc esi
        jmp .c
.libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(rt_hman)]
.f:     popad
        ret

; rt_nueva -> EBX = una fuente nueva, a cero (CF: ya hay RT_MAX)
rt_nueva:
        movzx eax, byte [V(rt_n)]
        cmp eax, RT_MAX
        jae .no
        imul eax, eax, RT_TAM
        lea ebx, [V(rt_fuentes)]
        add ebx, eax
        push edi
        push ecx
        mov edi, ebx
        mov ecx, RT_TAM
        xor eax, eax
        cld
        rep stosb
        pop ecx
        pop edi
        inc byte [V(rt_n)]
        clc
        ret
.no:    stc
        ret

; rt_usb: EDI = un manejador con EFI_USB_IO_PROTOCOL; si es una interfaz de
; ratón de arranque que no lleva el firmware, se pone en marcha
rt_usb:
        pushad
        mov [V(rt_man)], edi
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], edi, &guid_usbio, &rt_p
        test eax, eax
        jnz .f
        mov ecx, [V(rt_p)]
        FW [ecx + UIO_IFAZ], ecx, &rt_ifaz
        test eax, eax
        jnz .f
        cmp byte [V(rt_ifaz) + 5], 03h  ; HID
        jne .f
        cmp byte [V(rt_ifaz) + 7], 02h  ; protocolo: ratón
        jne .f
        DI t_rt_hid
        movzx eax, byte [V(rt_ifaz) + 2]
        call decimal
        DI t_rt_hid2
        movzx eax, byte [V(rt_ifaz) + 6]
        mov ecx, 2
        call hex
        mov eax, [V(bootserv)]          ; ¿lo lleva ya el firmware?
        FW [eax + BS_HANDLEPROT], [V(rt_man)], &guid_simple, &rt_p2
        test eax, eax
        jnz .sin_sp
        DI t_rt_ya_fw
        jmp .f
.sin_sp:
        mov eax, [V(bootserv)]          ; ¿lo tiene abierto un controlador?
        FW [eax + BS_OPENPROTINFO], [V(rt_man)], &guid_usbio, &rt_info, &rt_info_n
        test eax, eax
        jnz .libre
        xor ebx, ebx
        mov esi, [V(rt_info)]
        mov ecx, [V(rt_info_n)]
.oi:    jecxz .oi_f
        test dword [esi + 8], ABRIR_DRIVER
        jz .oi_s
        mov ebx, 1
.oi_s:  add esi, 16
        dec ecx
        jmp .oi
.oi_f:  push ebx
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(rt_info)]
        pop ebx
        test ebx, ebx
        jz .libre
        DI t_rt_ocupado
        jmp .f
.libre: cmp byte [V(rt_ifaz) + 6], 01h  ; los de arranque (subclase 01h); los
        je .arranque                    ; demás, si aceptan el protocolo de arranque
        mov word [V(rt_pet)], 0B21h
        mov word [V(rt_pet) + 2], 0
        movzx eax, byte [V(rt_ifaz) + 2]
        mov [V(rt_pet) + 4], ax
        mov word [V(rt_pet) + 6], 0
        call rt_control
        cmp dword [V(rt_res)], 0
        jne .no_arr
        cmp dword [V(rt_estado)], 0
        je .arranque
.no_arr:
        DI t_rt_no_arr
        jmp .f
.arranque:
        xor esi, esi                    ; el punto final de interrupción de entrada
.ep:    movzx eax, byte [V(rt_ifaz) + 4]
        cmp esi, eax
        jae .sin_ep
        mov ecx, [V(rt_p)]
        FW [ecx + UIO_EP], ecx, esi, &rt_epd
        test eax, eax
        jnz .ep_s
        mov al, [V(rt_epd) + 3]
        and al, 3
        cmp al, 3
        jne .ep_s
        test byte [V(rt_epd) + 2], 80h
        jnz .ep_si
.ep_s:  inc esi
        jmp .ep
.sin_ep:
        DI t_rt_sin_ep
        jmp .f
.ep_si: call rt_nueva
        jc .f
        mov byte [ebx + RT_TIPO], 3
        mov eax, [V(rt_p)]
        mov [ebx + RT_PROT], eax
        mov al, [V(rt_epd) + 2]
        mov [ebx + RT_EP], al
        movzx eax, word [V(rt_epd) + 4]
        and eax, 7FFh
        cmp eax, 8
        jbe .lg
        mov eax, 8
.lg:    cmp eax, 3
        jae .lg2
        mov eax, 3
.lg2:   mov [ebx + RT_LARGO], al
        movzx eax, byte [V(rt_epd) + 6]
        test eax, eax
        jnz .iv
        mov eax, 8
.iv:    mov [ebx + RT_INTERV], eax
        movzx eax, byte [V(rt_ifaz) + 2]
        mov [ebx + RT_IFAZ], eax
        DI t_rt_ep
        movzx eax, byte [ebx + RT_EP]
        mov ecx, 2
        call hex
        DI t_rt_ep2
        movzx eax, byte [ebx + RT_LARGO]
        call decimal
        DI t_rt_ep3
        mov eax, [ebx + RT_INTERV]
        call decimal
        DI t_rt_ep4
        mov word [V(rt_pet)], 0B21h     ; SET_PROTOCOL: el de arranque
        mov word [V(rt_pet) + 2], 0
        mov eax, [ebx + RT_IFAZ]
        mov [V(rt_pet) + 4], ax
        mov word [V(rt_pet) + 6], 0
        call rt_control
        DI t_rt_proto
        mov eax, [V(rt_estado)]
        mov ecx, 8
        call hex
        mov word [V(rt_pet)], 0A21h     ; SET_IDLE: duración 0
        call rt_control
        DI t_rt_idle
        mov eax, [V(rt_estado)]
        mov ecx, 8
        call hex
        call rt_usb_poner
        DI t_rt_async
        mov ecx, 8
        call hex
        DI t_nl
        test eax, eax
        jz .puesto
        dec byte [V(rt_n)]
        jmp .f
.puesto:
        or byte [V(rt_hay)], 4
.f:     popad
        ret

; rt_control: la petición rt_pet (sin datos) a la interfaz de rt_p -> rt_estado
rt_control:
        pushad
        mov dword [V(rt_estado)], 0
        mov ecx, [V(rt_p)]
        FW [ecx + UIO_CONTROL], ecx, &rt_pet, 2, 100, 0, 0, &rt_estado
        mov [V(rt_res)], eax
        popad
        ret

; rt_usb_poner: EBX = una fuente USB -> pone su transferencia asíncrona; EAX = resultado
rt_usb_poner:
        push ecx
        push edx
        push esi
        movzx esi, byte [ebx + RT_EP]
        movzx edx, byte [ebx + RT_LARGO]
        lea eax, [V(raton_cb)]
        mov [V(rt_cbdir)], eax
        mov ecx, [ebx + RT_PROT]
        FW [ecx + UIO_ASYNCINT], ecx, esi, 1, [ebx + RT_INTERV], edx, [V(rt_cbdir)], ebx
        pop esi
        pop edx
        pop ecx
        ret

; rt_usb_quitar: EBX = una fuente USB -> quita su transferencia asíncrona
rt_usb_quitar:
        push ecx
        push edx
        push esi
        movzx esi, byte [ebx + RT_EP]
        mov ecx, [ebx + RT_PROT]
        FW [ecx + UIO_ASYNCINT], ecx, esi, 0, 0, 0, 0, 0
        pop esi
        pop edx
        pop ecx
        ret

; ---- raton_cb: el firmware, con cada informe del ratón USB propio ----
; (EFI_ASYNC_USB_TRANSFER_CALLBACK, convención de C: datos, octetos, la
; fuente y el estado en la pila; vuelve con EFI_SUCCESS en EAX)
raton_cb:
        push ebp
        push ebx
        push esi
        push edi
        BASE_EBP
        mov esi, [esp + 20]             ; datos
        mov ecx, [esp + 24]             ; octetos
        mov ebx, [esp + 28]             ; la fuente
        cmp dword [esp + 32], 0         ; error del USB: se rehace en raton_sondear
        je .bien
        mov byte [ebx + RT_FALLO], 1
        jmp .f
.bien:  cmp ecx, 3
        jb .f
        test esi, esi
        jz .f
        pushfd
        cli
        mov edi, [V(bios_base)]
        inc dword [edi + 308]           ; T_RAT_CB
        mov al, [esi]
        and al, 7
        mov [ebx + RT_BOT], al
        cmp byte [edi + 285], 0         ; T_RAT_ON: sin activar, solo los botones
        je .bot
        movsx eax, byte [esi + 1]
        add [edi + 288], eax            ; T_RAT_DX
        movsx eax, byte [esi + 2]
        sub [edi + 292], eax            ; T_RAT_DY (el PS/2, hacia arriba)
        cmp ecx, 4
        jb .bot
        movsx eax, byte [esi + 3]
        sub [edi + 296], eax
.bot:   call rt_botones
        mov byte [edi + 287], 1         ; T_RAT_NUEVO
        popfd
.f:     xor eax, eax
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret

; rt_botones: T_RAT_BOT = los botones de todas las fuentes (EDI = la BIOS)
rt_botones:
        push ecx
        push edx
        xor eax, eax
        movzx ecx, byte [V(rt_n)]
        lea edx, [V(rt_fuentes)]
.b:     jecxz .f
        or al, [edx + RT_BOT]
        add edx, RT_TAM
        dec ecx
        jmp .b
.f:     mov [edi + 286], al
        pop edx
        pop ecx
        ret

; ---- raton_sondear: en cada viaje con el ratón activado ----
raton_sondear:
        pushad
        mov dword [V(rt_i)], 0
.c:     mov eax, [V(rt_i)]
        cmp al, [V(rt_n)]
        jae .f
        imul eax, eax, RT_TAM
        lea ebx, [V(rt_fuentes)]
        add ebx, eax
        mov edi, [V(bios_base)]
        cmp byte [ebx + RT_TIPO], 1
        je .simple
        cmp byte [ebx + RT_TIPO], 2
        je .absoluto
        cmp byte [ebx + RT_FALLO], 0
        je .sig
        mov byte [ebx + RT_FALLO], 0
        sti
        call rt_usb_quitar
        call rt_usb_poner
        cli
        jmp .sig
.simple:
        mov ecx, [ebx + RT_PROT]
        sti
        FW [ecx + SP_GETSTATE], ecx, &rt_estado_sp
        cli
        test eax, eax
        jnz .sig
        mov edi, [V(bios_base)]
        inc dword [edi + 300]           ; T_RAT_LEC
        mov eax, [V(rt_estado_sp)]
        mov ecx, [ebx + RT_RESX]
        lea esi, [ebx + RT_RESTOX]
        call rt_escalar
        add [edi + 288], eax
        mov eax, [V(rt_estado_sp) + 4]
        mov ecx, [ebx + RT_RESY]
        lea esi, [ebx + RT_RESTOY]
        call rt_escalar
        sub [edi + 292], eax
        mov eax, [V(rt_estado_sp) + 8]
        sub [edi + 296], eax
        xor eax, eax
        cmp byte [V(rt_estado_sp) + 12], 0
        je .s1
        or al, 1
.s1:    cmp byte [V(rt_estado_sp) + 13], 0
        je .s2
        or al, 2
.s2:    mov [ebx + RT_BOT], al
        jmp .nuevo
.absoluto:
        mov ecx, [ebx + RT_PROT]
        sti
        FW [ecx + SP_GETSTATE], ecx, &rt_estado_ap
        cli
        test eax, eax
        jnz .sig
        mov edi, [V(bios_base)]
        inc dword [edi + 300]
        mov eax, [V(rt_estado_ap)]      ; CurrentX, CurrentY (la parte baja)
        mov edx, [V(rt_estado_ap) + 8]
        cmp byte [ebx + RT_VISTO], 0
        mov byte [ebx + RT_VISTO], 1
        xchg eax, [ebx + RT_ULTX]
        xchg edx, [ebx + RT_ULTY]
        je .ab_bot
        neg eax
        add eax, [ebx + RT_ULTX]
        neg edx
        add edx, [ebx + RT_ULTY]
        push edx
        mov ecx, [ebx + RT_MAXX]
        sub ecx, [ebx + RT_RESX]
        lea esi, [ebx + RT_RESTOX]
        call rt_absoluto
        add [edi + 288], eax
        pop eax
        mov ecx, [ebx + RT_MAXY]
        sub ecx, [ebx + RT_RESY]
        lea esi, [ebx + RT_RESTOY]
        call rt_absoluto
        sub [edi + 292], eax
.ab_bot:
        mov eax, [V(rt_estado_ap) + 24]
        and al, 3
        mov [ebx + RT_BOT], al
.nuevo: call rt_botones
        mov byte [edi + 287], 1
.sig:   inc dword [V(rt_i)]
        jmp .c
.f:     popad
        ret

; rt_escalar: EAX = cuentas, ECX = cuentas por mm, ESI = el resto -> EAX
rt_escalar:
        cmp ecx, 8
        jbe .tal
        push edx
        shl eax, 3
        add eax, [esi]
        cdq
        idiv ecx
        mov [esi], edx
        pop edx
.tal:   ret

; rt_absoluto: EAX = lo que se ha movido, ECX = el ancho, ESI = el resto -> EAX
rt_absoluto:
        test ecx, ecx
        jle .cero
        push edx
        shl eax, 10
        add eax, [esi]
        cdq
        idiv ecx
        mov [esi], edx
        pop edx
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
rt_guid         dd 0
rt_g            dd 0
rt_i            dd 0
rt_hn           dd 0
rt_hman         dd 0
rt_man          dd 0
rt_p            dd 0
rt_p2           dd 0
rt_info         dd 0
rt_info_n       dd 0
rt_cbdir        dd 0
rt_estado       dd 0
rt_res          dd 0                    ; lo que devuelve rt_control
rt_estado_sp    dd 0, 0, 0, 0
        align 8
rt_estado_ap    dd 0, 0, 0, 0, 0, 0, 0, 0
rt_pet          dw 0, 0, 0, 0
rt_ifaz         times 10 db 0
rt_epd          times 8 db 0
rt_n            db 0
rt_hay          db 0
        align 8
rt_fuentes      times RT_MAX * RT_TAM db 0
