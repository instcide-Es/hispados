; =============================================================================
;  HispaBIOS (IA32) - el teclado del firmware y su reloj (lo incluye efi32.asm)
; =============================================================================
;  El fallo del MEDION S2217 (Atom Bay Trail, sin 8042: el teclado de la base
;  es USB): el teclado iba en el menú de CONFIG.SYS y dejaba de ir después.
;
;  El teclado USB del firmware (UsbKbDxe de EDK II y los que salen de él) no
;  se lee al preguntarle: sus teclas llegan por una transferencia de
;  interrupción que la controladora (xHCI) repasa en un suceso de TEMPORIZADOR
;  del firmware. Y los temporizadores del firmware solo andan con la
;  interrupción de su reloj (el protocolo «Timer Arch»: en ese equipo, el 8254
;  por el 8259, vector 68h, a 18,2 Hz). Con DOS en marcha el 8254 y el 8259
;  son de DOS: el firmware solo ve un tic si el flanco del 8254 cae justo en
;  uno de sus viajes. En el menú de CONFIG.SYS el núcleo pregunta por el
;  teclado sin parar (INT 16h 01h en bucle) y algunos flancos caen dentro;
;  después, quien espera una tecla descansa con HLT hasta el tic siguiente y
;  pregunta justo DESPUÉS de cada flanco: el firmware no ve ni uno, sus
;  temporizadores no avanzan y el teclado USB se queda mudo. (Un teclado PS/2
;  no depende de eso: su controlador mira el 8042 al preguntarle. Y en OVMF el
;  reloj del firmware es el del APIC local, que no va acompasado con el 8254.)
;
;  El arreglo: en cada viaje al firmware se le dan los tics de su reloj que le
;  tocan por el tiempo pasado (medido con el TSC), sin esperar a su
;  interrupción:
;    1. GenerateSoftInterrupt() de su EFI_TIMER_ARCH_PROTOCOL (PI, vol. 2): la
;       manera prevista de darle un tic «a mano»;
;    2. si no lo tiene o no lo admite y usa el 8259: su propia rutina de la
;       IRQ 0, llamada como una interrupción (por su IDT, vector T_VEC0).
;  En el modo seguro no se hace ninguna de las dos: INT 16h pregunta siempre
;  al firmware y la espera de tecla no usa HLT (como la HispaBIOS 0.2, con la
;  que ese equipo tenía teclado), que es lento pero deja al firmware sus tics.
;
;  Pruebas: -DPRUEBA_FW_SIN_RELOJ (el firmware no recibe ninguna interrupción
;  en sus viajes: el caso del Medion), -DPRUEBA_FWT=0 (sin dar tics: el fallo),
;  -DPRUEBA_FWT=2 (el camino 2 con el vector del temporizador del APIC local,
;  que es el reloj de OVMF).
; =============================================================================

BS_CHECKEVENT   equ 72
TXT_ESPERA      equ 8           ; EFI_SIMPLE_TEXT_INPUT_PROTOCOL.WaitForKey
TMR_PERIODO     equ 8           ; EFI_TIMER_ARCH_PROTOCOL.GetTimerPeriod
TMR_SOFT        equ 12          ; ... GenerateSoftInterrupt
UIO_DISP        equ 24          ; EFI_USB_IO_PROTOCOL.UsbGetDeviceDescriptor
TIC_100NS       equ 549254      ; un tic de DOS (1/18,2 s) en unidades de 100 ns
FWT_MAX         equ 3           ; tics del firmware por viaje, como mucho

; ---- teclado_preparar: qué teclado da el firmware y cómo anda su reloj ----
; (al arrancar, tras raton_preparar, que ha conectado las controladoras USB)
teclado_preparar:
        pushad
        DI t_tc_tit
        ; ---- 1. la consola de entrada ----
        mov ebx, [V(sistema)]
        DI t_tc_con
        mov eax, [ebx + ST_CONIN]
        mov ecx, 8
        call hex
        DI t_tc_con2
        mov eax, [ebx + ST_CONINH]
        call hex
        DI t_tc_con3
        mov ecx, [ebx + ST_CONINH]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], ecx, &guid_inex, &tc_p
        call tc_si_no
        DI t_nl
        ; ---- 2. cada teclado del firmware (Simple Text Input) ----
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_txtin, 0, &tc_hn, &tc_hman
        test eax, eax
        jnz .sin_txt
        xor esi, esi
.t:     cmp esi, [V(tc_hn)]
        jae .t_libera
        mov ecx, [V(tc_hman)]
        mov edi, [ecx + esi * 4]
        DI t_tc_txt
        mov eax, edi
        mov ecx, 8
        call hex
        mov eax, [V(sistema)]
        cmp edi, [eax + ST_CONINH]
        jne .t_no_con
        DI t_tc_txt_con
        jmp .t_sig
.t_no_con:
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], edi, &guid_usbio, &tc_p
        test eax, eax
        jnz .t_otro
        DI t_tc_txt_usb
        call tc_usb_id
        inc byte [V(tc_fw_usb)]
        DI t_tc_par
        DI t_nl
        jmp .t_sig
.t_otro:
        DI t_tc_txt_otro
.t_sig: inc esi
        jmp .t
.t_libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(tc_hman)]
        jmp .usb
.sin_txt:
        DI t_tc_sin_txt
        ; ---- 3. las interfaces USB de teclado (HID, protocolo 01h) ----
.usb:   mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_usbio, 0, &tc_hn, &tc_hman
        test eax, eax
        jnz .sin_usb
        xor esi, esi
.u:     cmp esi, [V(tc_hn)]
        jae .u_libera
        mov ecx, [V(tc_hman)]
        mov edi, [ecx + esi * 4]
        call tc_usb
        inc esi
        jmp .u
.u_libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(tc_hman)]
.sin_usb:
        cmp byte [V(tc_usb_n)], 0
        jne .reloj
        DI t_tc_sin_usb
        ; ---- 4. el reloj del firmware ----
.reloj: mov dword [V(tc_periodo)], TIC_100NS
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEPROT], &guid_timer, 0, &tc_timer
        test eax, eax
        jz .hay_timer
        mov dword [V(tc_timer)], 0
        DI t_tc_sin_timer
        mov ecx, 8
        call hex
        DI t_nl
        jmp .elegir
.hay_timer:
        DI t_tc_timer
        mov ecx, [V(tc_timer)]
        FW [ecx + TMR_PERIODO], ecx, &tc_per64
        test eax, eax
        jnz .per_mal
        cmp dword [V(tc_per64) + 4], 0
        je .per_dec
        mov eax, [V(tc_per64)]          ; (un periodo raro: se toma el tic de DOS)
        mov edx, [V(tc_per64) + 4]
        call hex64
        DI t_tc_timer2h
        jmp .elegir
.per_dec:
        mov eax, [V(tc_per64)]
        xor edx, edx
        mov ecx, 10
        div ecx
        call decimal
        DI t_tc_timer2
        mov eax, [V(tc_per64)]
        test eax, eax
        jz .elegir                      ; (0: su reloj está parado)
        mov [V(tc_periodo)], eax
        jmp .elegir
.per_mal:
        mov ecx, 8
        call hex
        DI t_tc_timer3
.elegir:
        ; el periodo, entre 1 ms y 100 ms, y en cuentas del TSC
        mov eax, [V(tc_periodo)]
        cmp eax, 10000
        jae .p1
        mov eax, 10000
.p1:    cmp eax, 1000000
        jbe .p2
        mov eax, 1000000
.p2:    mul dword [V(tsc_tic)]
        mov ecx, TIC_100NS
        div ecx
        test eax, eax
        jnz .p3
        inc eax
.p3:    mov [V(tc_per_tsc)], eax
        ; ---- cómo se le dan los tics ----
        mov byte [V(tc_via)], 0
        cmp byte [V(seguro)], 0
        je .no_seguro
        mov eax, [V(bios_base)]
        or byte [eax + 312], 1          ; T_TEC_OPC: sondeo continuo
        DI t_tc_seguro
        jmp .dicho
.no_seguro:
%ifdef PRUEBA_FWT
  %if PRUEBA_FWT == 0
        DI t_tc_prueba0
        jmp .dicho
  %elif PRUEBA_FWT == 2
        jmp .via2
  %endif
%endif
        cmp dword [V(tc_timer)], 0
        je .via2
        sti                             ; se prueba una vez: si lo admite
        mov ecx, [V(tc_timer)]
        FW [ecx + TMR_SOFT], ecx
        mov [V(tc_res)], eax
        DI t_tc_soft
        mov eax, [V(tc_res)]
        mov ecx, 8
        call hex
        DI t_nl
        cmp dword [V(tc_res)], 0
        jne .via2
        mov byte [V(tc_via)], 1
        DI t_tc_via1
        jmp .dicho
.via2:  call fwt_vector                 ; -> tc_far (CF: no hay)
        jc .ninguna
        mov byte [V(tc_via)], 2
        DI t_tc_via2
        movzx eax, byte [V(tc_vec)]
        mov ecx, 2
        call hex
        DI t_tc_via2b
        jmp .dicho
.ninguna:
        DI t_tc_via0
.dicho: mov eax, [V(bios_base)]
        mov cl, [V(tc_via)]
        mov [eax + 313], cl             ; T_FWT_VIA
        rdtsc
        mov [V(tc_ult)], eax
        mov [V(tc_ult) + 4], edx
        popad
        ret

; tc_si_no: EAX = resultado de una llamada -> escribe «si» (0) o «no (código)»
tc_si_no:
        test eax, eax
        jnz .no
        DI t_tc_si
        ret
.no:    DI t_tc_no
        push ecx
        mov ecx, 8
        call hex
        pop ecx
        DI t_tc_par
        ret

; tc_usb_id: tc_p = EFI_USB_IO -> escribe «VVVV:PPPP, interfaz n»
tc_usb_id:
        pushad
        mov ecx, [V(tc_p)]
        FW [ecx + UIO_DISP], ecx, &tc_disp
        test eax, eax
        jnz .sin
        movzx eax, word [V(tc_disp) + 8]
        mov ecx, 4
        call hex
        DI t_dc2
        movzx eax, word [V(tc_disp) + 10]
        call hex
.sin:   mov ecx, [V(tc_p)]
        FW [ecx + UIO_IFAZ], ecx, &tc_ifaz
        test eax, eax
        jnz .f
        DI t_tc_ifaz
        movzx eax, byte [V(tc_ifaz) + 2]
        call decimal
        DI t_tc_clase
        movzx eax, byte [V(tc_ifaz) + 5]
        mov ecx, 2
        call hex
        DI t_tc_barra
        movzx eax, byte [V(tc_ifaz) + 6]
        call hex
        DI t_tc_barra
        movzx eax, byte [V(tc_ifaz) + 7]
        call hex
.f:     popad
        ret

; tc_usb: EDI = un manejador con EFI_USB_IO; si es una interfaz de teclado
; (HID, protocolo 01h) se apunta, con quién la lleva
tc_usb:
        pushad
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], edi, &guid_usbio, &tc_p
        test eax, eax
        jnz .f
        mov ecx, [V(tc_p)]
        FW [ecx + UIO_IFAZ], ecx, &tc_ifaz
        test eax, eax
        jnz .f
        cmp byte [V(tc_ifaz) + 5], 03h  ; HID
        jne .f
        cmp byte [V(tc_ifaz) + 7], 01h  ; protocolo: teclado
        jne .f
        inc byte [V(tc_usb_n)]
        DI t_tc_hid
        call tc_usb_id
        mov eax, [V(bootserv)]          ; ¿la lleva el firmware?
        FW [eax + BS_HANDLEPROT], edi, &guid_txtin, &tc_p2
        test eax, eax
        jnz .sin_txt
        DI t_tc_hid_fw
        jmp .f
.sin_txt:
        mov eax, [V(bootserv)]          ; ¿la tiene abierta un controlador?
        FW [eax + BS_OPENPROTINFO], edi, &guid_usbio, &tc_info, &tc_info_n
        test eax, eax
        jnz .libre
        xor ebx, ebx
        mov esi, [V(tc_info)]
        mov ecx, [V(tc_info_n)]
.oi:    jecxz .oi_f
        test dword [esi + 8], ABRIR_DRIVER
        jz .oi_s
        mov ebx, 1
.oi_s:  add esi, 16
        dec ecx
        jmp .oi
.oi_f:  push ebx
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(tc_info)]
        pop ebx
        test ebx, ebx
        jz .libre
        DI t_tc_hid_ocup
        jmp .f
.libre: DI t_tc_hid_libre
.f:     popad
        ret

; fwt_vector: la rutina con la que el firmware atiende su reloj, por su IDT
; -> tc_far (desplazamiento y selector), tc_vec; CF si no se puede. Solo si el
; firmware usa el 8259 con la IRQ 0 abierta (vector T_VEC0). (Pruebas, con
; PRUEBA_FWT=2 y sin 8259: el vector del temporizador del APIC local.)
fwt_vector:
        pushad
        mov eax, [V(m_vec)]
        test al, al
        jz .sin_8259
        mov [V(tc_vec)], al
        in al, 21h
        test al, 1                      ; ¿tiene la IRQ 0 abierta?
        jnz .no
        jmp .idt
.sin_8259:
%ifdef PRUEBA_FWT
        mov ecx, 1Bh                    ; IA32_APIC_BASE
        rdmsr
        test eax, 800h
        jz .no
        test eax, 400h                  ; (x2APIC: no se prueba)
        jnz .no
        and eax, 0FFFFF000h
        mov eax, [eax + 320h]           ; el temporizador del APIC local
        test eax, 10000h
        jnz .no
        cmp al, 20h
        jb .no
        mov [V(tc_vec)], al
%else
        jmp .no
%endif
.idt:   sidt [V(tc_idt)]
        movzx eax, byte [V(tc_vec)]
        lea ecx, [eax * 8 + 7]
        cmp cx, [V(tc_idt)]             ; ¿cabe en la IDT?
        ja .no
        mov esi, [V(tc_idt) + 2]
        lea esi, [esi + eax * 8]
        mov al, [esi + 5]
        test al, 80h                    ; presente
        jz .no
        and al, 1Eh                     ; puerta de interrupción o de excepción, de 32 bits
        cmp al, 0Eh
        jne .no
        movzx eax, word [esi + 6]
        shl eax, 16
        mov ax, [esi]
        test eax, eax
        jz .no
        mov [V(tc_far)], eax
        mov ax, [esi + 2]
        mov [V(tc_far) + 4], ax
        popad
        clc
        ret
.no:    popad
        stc
        ret

; ---- fwt_dar: en cada viaje, los tics del reloj del firmware que le tocan ----
; (llega con las interrupciones cerradas y las deja cerradas; conserva todo)
fwt_dar:
        cmp byte [V(tc_via)], 0
        je .nada
        pushad
        rdtsc
        mov esi, eax
        mov edi, edx
        sub eax, [V(tc_ult)]
        sbb edx, [V(tc_ult) + 4]
        jnz .muchos                     ; (más de 2^32 cuentas: mucho tiempo)
        xor edx, edx
        div dword [V(tc_per_tsc)]       ; EAX = tics que tocan
        test eax, eax
        jz .f
        cmp eax, FWT_MAX
        ja .muchos
        mov ebx, eax
        mul dword [V(tc_per_tsc)]       ; lo dado se descuenta; el resto queda
        add [V(tc_ult)], eax
        adc [V(tc_ult) + 4], edx
        jmp .dar
.muchos:
        mov [V(tc_ult)], esi            ; se ha quedado muy atrás: desde ahora
        mov [V(tc_ult) + 4], edi
        mov ebx, FWT_MAX
.dar:   call fwt_uno
        mov eax, [V(bios_base)]
        inc dword [eax + 316]           ; T_FWT_N
        dec ebx
        jnz .dar
.f:     popad
.nada:  ret

; fwt_uno: un tic del reloj del firmware (corren sus sucesos de temporizador:
; el repaso del USB, y con él las teclas y el ratón)
fwt_uno:
        cmp byte [V(tc_via)], 2
        je .rutina
        sti
        mov ecx, [V(tc_timer)]
        FW [ecx + TMR_SOFT], ecx
        cli
        ret
.rutina:
        ; como la interrupción: EFLAGS (con las interrupciones abiertas, que es
        ; como las tenía el firmware), CS y EIP en la pila, y a su rutina con
        ; ellas cerradas; su IRETD vuelve aquí
        sti
        pushfd
        cli
        call far [V(tc_far)]
        cli
        ret

; ---- tecla_fw: la parte de la petición «una tecla» que ayuda al firmware ----
; Tras la espera y antes de leer: CheckEvent(WaitForKey), que es como pregunta
; un programa de UEFI si hay tecla (hay firmware que atiende ahí su teclado)
tecla_esperada:
        pushad
        mov eax, [V(sistema)]
        mov ecx, [eax + ST_CONIN]
        test ecx, ecx
        jz .f
        mov ecx, [ecx + TXT_ESPERA]
        test ecx, ecx
        jz .f
        sti
        mov eax, [V(bootserv)]
        FW [eax + BS_CHECKEVENT], ecx
        cli
.f:     popad
        ret

; ---- textos y variables ----
t_tc_tit        db 'Teclado:', 13, 10, 0
t_tc_con        db '  consola de entrada (ConIn) en ', 0
t_tc_con2       db ', manejador ', 0
t_tc_con3       db '; con estado de Ctrl y Alt (SimpleTextInputEx): ', 0
t_tc_si         db 'si', 0
t_tc_no         db 'no (', 0
t_tc_par        db ')', 0
t_tc_txt        db '  teclado del firmware (Simple Text Input) en el manejador ', 0
t_tc_txt_con    db ': el de la consola, que reune a los demas', 13, 10, 0
t_tc_txt_usb    db ': es USB, ', 0
t_tc_txt_otro   db ': no es USB (PS/2, puerto serie u otro)', 13, 10, 0
t_tc_sin_txt    db '  el firmware no da ningun teclado (Simple Text Input)', 13, 10, 0
t_tc_ifaz       db ', interfaz ', 0
t_tc_clase      db ' (clase/subclase/protocolo ', 0
t_tc_barra      db '/', 0
t_tc_hid        db '  USB: interfaz HID de teclado ', 0
t_tc_hid_fw     db '): la lleva el firmware (da Simple Text Input)', 13, 10, 0
t_tc_hid_ocup   db '): la tiene abierta un controlador del firmware, sin Simple Text Input', 13, 10, 0
t_tc_hid_libre  db '): SIN controlador del firmware (ese teclado no llega a DOS)', 13, 10, 0
t_tc_sin_usb    db '  USB: ninguna interfaz HID de teclado a la vista', 13, 10, 0
t_tc_sin_timer  db '  reloj del firmware: no da el protocolo Timer Arch; resultado ', 0
t_tc_timer      db '  reloj del firmware (Timer Arch): periodo ', 0
t_tc_timer2     db ' microsegundos (0: parado)', 13, 10, 0
t_tc_timer2h    db 'h x 100 ns', 13, 10, 0
t_tc_timer3     db ' (GetTimerPeriod falla)', 13, 10, 0
t_tc_soft       db '  GenerateSoftInterrupt (un tic a mano): resultado ', 0
t_tc_via1       db '  Tics al firmware en cada viaje: por GenerateSoftInterrupt (sus temporizadores, y con ellos el teclado USB, no dependen de su interrupcion)', 13, 10, 0
t_tc_via2       db '  Tics al firmware en cada viaje: por su rutina de la interrupcion del reloj (vector ', 0
t_tc_via2b      db 'h de su IDT)', 13, 10, 0
t_tc_via0       db '  Tics al firmware: NO se le pueden dar (ni Timer Arch ni 8259 con la IRQ 0 abierta); un teclado USB puede ir a ratos. Pruebe el modo seguro (S)', 13, 10, 0
t_tc_seguro     db '  Modo seguro: no se dan tics al firmware; INT 16h le pregunta siempre y la espera de tecla no usa HLT', 13, 10, 0
%ifdef PRUEBA_FWT
t_tc_prueba0    db '  (prueba: no se dan tics al firmware)', 13, 10, 0
%endif
guid_txtin      dd 387477C1h            ; EFI_SIMPLE_TEXT_INPUT_PROTOCOL
                dw 69C7h, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_timer      dd 26BACCB3h            ; EFI_TIMER_ARCH_PROTOCOL
                dw 6F42h, 11D4h
                db 0BCh, 0E7h, 00h, 80h, 0C7h, 3Ch, 88h, 81h
                align 4, db 0
tc_p            dd 0
tc_p2           dd 0
tc_hn           dd 0
tc_hman         dd 0
tc_info         dd 0
tc_info_n       dd 0
tc_timer        dd 0                    ; EFI_TIMER_ARCH_PROTOCOL (0: no hay)
tc_per64        dd 0, 0                 ; su periodo (100 ns)
tc_periodo      dd 0
tc_per_tsc      dd 1                    ; ... en cuentas del TSC
tc_res          dd 0
tc_ult          dd 0, 0                 ; TSC hasta donde se han dado tics
tc_far          dd 0                    ; la rutina del reloj del firmware:
                dw 0, 0                 ; desplazamiento y selector
tc_idt          dw 0, 0, 0, 0
tc_disp         times 20 db 0
tc_ifaz         times 10 db 0
tc_vec          db 0
tc_via          db 0                    ; 0 no se dan; 1 GenerateSoftInterrupt; 2 su rutina
tc_usb_n        db 0
tc_fw_usb       db 0
