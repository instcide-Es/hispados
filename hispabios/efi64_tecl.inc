; =============================================================================
;  HispaBIOS (x64) - el teclado del firmware y su reloj (lo incluye efi64.asm)
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
;  (Lo mismo que efi32_tecl.inc, con la convención de llamadas de x64.)
;
;  Pruebas: -DPRUEBA_FW_SIN_RELOJ (el firmware no recibe ninguna interrupción
;  en sus viajes: el caso del Medion), -DPRUEBA_FWT=0 (sin dar tics: el fallo),
;  -DPRUEBA_FWT=2 (el camino 2 con el vector del temporizador del APIC local,
;  que es el reloj de OVMF).
; =============================================================================

BS_CHECKEVENT   equ 120
TXT_ESPERA      equ 16          ; EFI_SIMPLE_TEXT_INPUT_PROTOCOL.WaitForKey
TMR_PERIODO     equ 16          ; EFI_TIMER_ARCH_PROTOCOL.GetTimerPeriod
TMR_SOFT        equ 24          ; ... GenerateSoftInterrupt
UIO_DISP        equ 48          ; EFI_USB_IO_PROTOCOL.UsbGetDeviceDescriptor
TIC_100NS       equ 549254      ; un tic de DOS (1/18,2 s) en unidades de 100 ns
FWT_MAX         equ 3           ; tics del firmware por viaje, como mucho

; prólogo y epílogo: se guardan RBX, RSI, RDI, R12 y R13, y la pila queda alineada
%macro TC_ENTRA 0
        push rbp
        mov rbp, rsp
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        and rsp, -16
        sub rsp, 64
%endmacro
%macro TC_SALE 0
        lea rsp, [rbp - 40]
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        ret
%endmacro

; ---- teclado_preparar: qué teclado da el firmware y cómo anda su reloj ----
; (al arrancar, tras raton_preparar, que ha conectado las controladoras USB)
teclado_preparar:
        TC_ENTRA
        DI t_tc_tit
        ; ---- 1. la consola de entrada ----
        mov rbx, [rel sistema]
        DI t_tc_con
        mov rax, [rbx + ST_CONIN]
        call hex64
        DI t_tc_con2
        mov rax, [rbx + ST_CONINH]
        call hex64
        DI t_tc_con3
        mov rcx, [rbx + ST_CONINH]
        lea rdx, [rel guid_inex]
        lea r8, [rel tc_p]
        mov rax, [rel bootserv]
        FW [rax + BS_HANDLEPROT]
        call tc_si_no
        DI t_nl
        ; ---- 2. cada teclado del firmware (Simple Text Input) ----
        mov rax, [rel bootserv]
        mov ecx, 2                      ; ByProtocol
        lea rdx, [rel guid_txtin]
        xor r8d, r8d
        lea r9, [rel tc_hn]
        lea r10, [rel tc_hman]
        mov [rsp + 32], r10
        FW [rax + BS_LOCATEHBUF]
        test rax, rax
        jnz .sin_txt
        xor esi, esi
.t:     cmp rsi, [rel tc_hn]
        jae .t_libera
        mov rcx, [rel tc_hman]
        mov rdi, [rcx + rsi * 8]
        DI t_tc_txt
        mov rax, rdi
        call hex64
        mov rax, [rel sistema]
        cmp rdi, [rax + ST_CONINH]
        jne .t_no_con
        DI t_tc_txt_con
        jmp .t_sig
.t_no_con:
        mov rax, [rel bootserv]
        mov rcx, rdi
        lea rdx, [rel guid_usbio]
        lea r8, [rel tc_p]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .t_otro
        DI t_tc_txt_usb
        call tc_usb_id
        DI t_tc_par
        DI t_nl
        jmp .t_sig
.t_otro:
        DI t_tc_txt_otro
.t_sig: inc rsi
        jmp .t
.t_libera:
        mov rax, [rel bootserv]
        mov rcx, [rel tc_hman]
        FW [rax + BS_FREEPOOL]
        jmp .usb
.sin_txt:
        DI t_tc_sin_txt
        ; ---- 3. las interfaces USB de teclado (HID, protocolo 01h) ----
.usb:   mov rax, [rel bootserv]
        mov ecx, 2
        lea rdx, [rel guid_usbio]
        xor r8d, r8d
        lea r9, [rel tc_hn]
        lea r10, [rel tc_hman]
        mov [rsp + 32], r10
        FW [rax + BS_LOCATEHBUF]
        test rax, rax
        jnz .sin_usb
        xor esi, esi
.u:     cmp rsi, [rel tc_hn]
        jae .u_libera
        mov rcx, [rel tc_hman]
        mov rdi, [rcx + rsi * 8]
        call tc_usb
        inc rsi
        jmp .u
.u_libera:
        mov rax, [rel bootserv]
        mov rcx, [rel tc_hman]
        FW [rax + BS_FREEPOOL]
.sin_usb:
        cmp byte [rel tc_usb_n], 0
        jne .reloj
        DI t_tc_sin_usb
        ; ---- 4. el reloj del firmware ----
.reloj: mov dword [rel tc_periodo], TIC_100NS
        mov rax, [rel bootserv]
        lea rcx, [rel guid_timer]
        xor edx, edx
        lea r8, [rel tc_timer]
        FW [rax + BS_LOCATEPROT]
        test rax, rax
        jz .hay_timer
        mov qword [rel tc_timer], 0
        DI t_tc_sin_timer
        call hex64
        DI t_nl
        jmp .elegir
.hay_timer:
        DI t_tc_timer
        mov rcx, [rel tc_timer]
        lea rdx, [rel tc_per64]
        FW [rcx + TMR_PERIODO]
        test rax, rax
        jnz .per_mal
        mov rax, [rel tc_per64]
        mov rdx, rax
        shr rdx, 32
        jz .per_dec
        call hex64                      ; (un periodo raro: se toma el tic de DOS)
        DI t_tc_timer2h
        jmp .elegir
.per_dec:
        xor edx, edx
        mov ecx, 10
        div rcx
        call decimal
        DI t_tc_timer2
        mov eax, [rel tc_per64]
        test eax, eax
        jz .elegir                      ; (0: su reloj está parado)
        mov [rel tc_periodo], eax
        jmp .elegir
.per_mal:
        call hex64
        DI t_tc_timer3
.elegir:
        ; el periodo, entre 1 ms y 100 ms, y en cuentas del TSC
        mov eax, [rel tc_periodo]
        cmp eax, 10000
        jae .p1
        mov eax, 10000
.p1:    cmp eax, 1000000
        jbe .p2
        mov eax, 1000000
.p2:    mov ecx, [rel tsc_tic]
        mul rcx
        mov ecx, TIC_100NS
        div rcx
        test rax, rax
        jnz .p3
        inc rax
.p3:    mov [rel tc_per_tsc], rax
        ; ---- cómo se le dan los tics ----
        mov byte [rel tc_via], 0
        cmp byte [rel seguro], 0
        je .no_seguro
        mov rax, [rel bios_base]
        or byte [rax + 312], 1          ; T_TEC_OPC: sondeo continuo
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
        cmp qword [rel tc_timer], 0
        je .via2
        sti                             ; se prueba una vez: si lo admite
        mov rcx, [rel tc_timer]
        FW [rcx + TMR_SOFT]
        mov rbx, rax
        DI t_tc_soft
        mov rax, rbx
        call hex64
        DI t_nl
        test rbx, rbx
        jnz .via2
        mov byte [rel tc_via], 1
        DI t_tc_via1
        jmp .dicho
.via2:  call fwt_vector                 ; -> tc_rut (CF: no hay)
        jc .ninguna
        mov byte [rel tc_via], 2
        DI t_tc_via2
        movzx eax, byte [rel tc_vec]
        mov ecx, 2
        call hex
        DI t_tc_via2b
        jmp .dicho
.ninguna:
        DI t_tc_via0
.dicho: mov rax, [rel bios_base]
        mov cl, [rel tc_via]
        mov [rax + 313], cl             ; T_FWT_VIA
        rdtsc
        shl rdx, 32
        or rax, rdx
        mov [rel tc_ult], rax
        TC_SALE

; tc_si_no: RAX = resultado de una llamada -> escribe «si» (0) o «no (código)»
tc_si_no:
        test rax, rax
        jnz .no
        DI t_tc_si
        ret
.no:    DI t_tc_no
        call hex64
        DI t_tc_par
        ret

; tc_usb_id: tc_p = EFI_USB_IO -> escribe «VVVV:PPPP, interfaz n (clase...»
tc_usb_id:
        TC_ENTRA
        mov rcx, [rel tc_p]
        lea rdx, [rel tc_disp]
        FW [rcx + UIO_DISP]
        test rax, rax
        jnz .sin
        movzx eax, word [rel tc_disp + 8]
        mov ecx, 4
        call hex
        DI t_dc2
        movzx eax, word [rel tc_disp + 10]
        mov ecx, 4
        call hex
.sin:   mov rcx, [rel tc_p]
        lea rdx, [rel tc_ifaz]
        FW [rcx + UIO_IFAZ]
        test rax, rax
        jnz .f
        DI t_tc_ifaz
        movzx eax, byte [rel tc_ifaz + 2]
        call decimal
        DI t_tc_clase
        movzx eax, byte [rel tc_ifaz + 5]
        mov ecx, 2
        call hex
        DI t_tc_barra
        movzx eax, byte [rel tc_ifaz + 6]
        call hex
        DI t_tc_barra
        movzx eax, byte [rel tc_ifaz + 7]
        call hex
.f:     TC_SALE

; tc_usb: RDI = un manejador con EFI_USB_IO; si es una interfaz de teclado
; (HID, protocolo 01h) se apunta, con quién la lleva
tc_usb:
        TC_ENTRA
        mov r12, rdi
        mov rax, [rel bootserv]
        mov rcx, r12
        lea rdx, [rel guid_usbio]
        lea r8, [rel tc_p]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .f
        mov rcx, [rel tc_p]
        lea rdx, [rel tc_ifaz]
        FW [rcx + UIO_IFAZ]
        test rax, rax
        jnz .f
        cmp byte [rel tc_ifaz + 5], 03h ; HID
        jne .f
        cmp byte [rel tc_ifaz + 7], 01h ; protocolo: teclado
        jne .f
        inc byte [rel tc_usb_n]
        DI t_tc_hid
        call tc_usb_id
        mov rax, [rel bootserv]         ; ¿la lleva el firmware?
        mov rcx, r12
        lea rdx, [rel guid_txtin]
        lea r8, [rel tc_p2]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .sin_txt
        DI t_tc_hid_fw
        jmp .f
.sin_txt:
        mov rax, [rel bootserv]         ; ¿la tiene abierta un controlador?
        mov rcx, r12
        lea rdx, [rel guid_usbio]
        lea r8, [rel tc_info]
        lea r9, [rel tc_info_n]
        FW [rax + BS_OPENPROTINFO]
        test rax, rax
        jnz .libre
        xor ebx, ebx
        mov rsi, [rel tc_info]
        mov rcx, [rel tc_info_n]
.oi:    test rcx, rcx
        jz .oi_f
        test dword [rsi + 16], ABRIR_DRIVER
        jz .oi_s
        mov ebx, 1
.oi_s:  add rsi, 24
        dec rcx
        jmp .oi
.oi_f:  mov rax, [rel bootserv]
        mov rcx, [rel tc_info]
        FW [rax + BS_FREEPOOL]
        test ebx, ebx
        jz .libre
        DI t_tc_hid_ocup
        jmp .f
.libre: DI t_tc_hid_libre
.f:     TC_SALE

; fwt_vector: la rutina con la que el firmware atiende su reloj, por su IDT
; -> tc_rut, tc_vec; CF si no se puede. Solo si el firmware usa el 8259 con la
; IRQ 0 abierta (vector T_VEC0). (Pruebas, con PRUEBA_FWT=2 y sin 8259: el
; vector del temporizador del APIC local.)
fwt_vector:
        push rbx
        push rsi
        mov rax, [rel bios_base]
        mov al, [rax + 182]             ; T_VEC0
        test al, al
        jz .sin_8259
        mov [rel tc_vec], al
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
        mov eax, [rax + 320h]           ; el temporizador del APIC local
        test eax, 10000h
        jnz .no
        cmp al, 20h
        jb .no
        mov [rel tc_vec], al
%else
        jmp .no
%endif
.idt:   sidt [rel tc_idt]
        movzx eax, byte [rel tc_vec]
        shl eax, 4
        lea ecx, [eax + 15]
        cmp cx, [rel tc_idt]            ; ¿cabe en la IDT?
        ja .no
        mov rsi, [rel tc_idt + 2]
        add rsi, rax
        mov al, [rsi + 5]
        test al, 80h                    ; presente
        jz .no
        and al, 1Eh                     ; puerta de interrupción o de excepción
        cmp al, 0Eh
        jne .no
        mov eax, [rsi + 8]
        shl rax, 32
        movzx ecx, word [rsi + 6]
        shl ecx, 16
        mov cx, [rsi]
        or rax, rcx
        jz .no
        mov [rel tc_rut], rax
        pop rsi
        pop rbx
        clc
        ret
.no:    pop rsi
        pop rbx
        stc
        ret

; ---- fwt_dar: en cada viaje, los tics del reloj del firmware que le tocan ----
; (llega con las interrupciones cerradas y las deja cerradas; conserva todo)
fwt_dar:
        cmp byte [rel tc_via], 0
        je .nada
        push rax
        push rcx
        push rdx
        push r8
        push r9
        push r10
        push r11
        TC_ENTRA
        rdtsc
        shl rdx, 32
        or rax, rdx
        mov rsi, rax                    ; RSI = ahora
        sub rax, [rel tc_ult]
        xor edx, edx
        div qword [rel tc_per_tsc]      ; RAX = tics que tocan
        test rax, rax
        jz .f
        cmp rax, FWT_MAX
        ja .muchos
        mov rbx, rax
        mul qword [rel tc_per_tsc]      ; lo dado se descuenta; el resto queda
        add [rel tc_ult], rax
        jmp .dar
.muchos:
        mov [rel tc_ult], rsi           ; se ha quedado muy atrás: desde ahora
        mov ebx, FWT_MAX
.dar:   call fwt_uno
        mov rax, [rel bios_base]
        inc dword [rax + 316]           ; T_FWT_N
        dec rbx
        jnz .dar
.f:     lea rsp, [rbp - 40]
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdx
        pop rcx
        pop rax
.nada:  ret

; fwt_uno: un tic del reloj del firmware (corren sus sucesos de temporizador:
; el repaso del USB, y con él las teclas y el ratón). La pila llega alineada.
fwt_uno:
        cmp byte [rel tc_via], 2
        je .rutina
        sub rsp, 40
        sti
        mov rcx, [rel tc_timer]
        FW [rcx + TMR_SOFT]
        cli
        add rsp, 40
        ret
.rutina:
        ; como la interrupción (modo largo): SS, RSP, RFLAGS (con las
        ; interrupciones abiertas, que es como las tenía el firmware), CS y
        ; RIP en la pila alineada, y a su rutina con ellas cerradas; su IRETQ
        ; vuelve aquí con la pila de antes
        mov rax, rsp
        and rsp, -16
        mov ecx, ss
        push rcx
        push rax
        pushfq
        or qword [rsp], 200h
        mov ecx, cs
        push rcx
        lea rcx, [rel .vuelta]
        push rcx
        cli
        jmp [rel tc_rut]
.vuelta:
        cli
        ret

; ---- tecla_esperada: antes de leer la tecla, CheckEvent(WaitForKey), que es
; como pregunta un programa de UEFI si hay tecla (hay firmware que atiende ahí
; su teclado). Conserva todo; la pila llega como sea.
tecla_esperada:
        push rax
        push rcx
        push rdx
        push r8
        push r9
        push r10
        push r11
        TC_ENTRA
        mov rax, [rel sistema]
        mov rcx, [rax + ST_CONIN]
        test rcx, rcx
        jz .f
        mov rcx, [rcx + TXT_ESPERA]
        test rcx, rcx
        jz .f
        sti
        mov rax, [rel bootserv]
        FW [rax + BS_CHECKEVENT]
        cli
.f:     lea rsp, [rbp - 40]
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdx
        pop rcx
        pop rax
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
                align 8, db 0
tc_p            dq 0
tc_p2           dq 0
tc_hn           dq 0
tc_hman         dq 0
tc_info         dq 0
tc_info_n       dq 0
tc_timer        dq 0                    ; EFI_TIMER_ARCH_PROTOCOL (0: no hay)
tc_per64        dq 0                    ; su periodo (100 ns)
tc_per_tsc      dq 1                    ; ... en cuentas del TSC
tc_ult          dq 0                    ; TSC hasta donde se han dado tics
tc_rut          dq 0                    ; la rutina del reloj del firmware (de su IDT)
tc_periodo      dd 0
tc_idt          dw 0, 0, 0, 0, 0, 0
tc_disp         times 20 db 0
tc_ifaz         times 10 db 0
tc_vec          db 0
tc_via          db 0                    ; 0 no se dan; 1 GenerateSoftInterrupt; 2 su rutina
tc_usb_n        db 0
