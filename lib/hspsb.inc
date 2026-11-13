; =============================================================================
;  hspsb.inc - Sound Blaster 16 emulada de HSPSND (HSPSND /SB)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Para los equipos sin Sound Blaster (solo HD Audio o AC'97): con HSPEMM
;  cargado (modo V86), el monitor desvia a e_io cada IN/OUT de los programas
;  a los puertos 220h-22Fh, 388h-38Bh y 20h (docs/HSPSB.md):
;    - DSP 4.05: reproduccion por DMA de 8 bits (14h, 1Ch, 90h, 91h, 48h),
;      de 8 y 16 bits mono y estereo de la SB16 (Bxh, Cxh), frecuencia (40h,
;      41h), pausa, altavoz, identificacion (E1h, E0h, E4h/E8h) e IRQ (F2h).
;    - Mezclador: maestro, voz y FM (registros de la SB Pro y de la SB16),
;      IRQ y DMA (80h-82h).
;    - FM OPL2 (AdLib) por sintesis en lib/hspopl.inc.
;  El canal de DMA lo «lleva» HSPSND: el monitor anota en d8/d16 lo que el
;  programa escribe en el 8237 y contesta con esas estructuras sus lecturas.
;  La IRQ de la tarjeta (5 o 7) se entrega saltando al vector del programa
;  con el marco de la interrupcion en curso, como si acabara de llegar; su
;  EOI (puerto 20h) no llega al 8259.
;
;  Salida: un anillo de RING_FR tramas (16 bits estereo, a hw_rate) que el
;  HD Audio o el AC'97 tocan sin parar. e_pump lo va llenando por delante de
;  lo que suena (e_lead tramas): lo llaman la interrupcion de la tarjeta
;  (cada 256 tramas) y el reloj (INT 08h). Cada voz (la de la SB, la de la
;  API de HSPSND) lleva su propio cursor y suma sus muestras; la FM se suma
;  al avanzar el anillo.
;
;  Escrito desde cero con la documentacion publica de la Sound Blaster
;  (Hardware Programming Guide), del 8237/8259 y del AC'97/HD Audio.
;
;  Todo esto corre con DS = CS, GS = bloque de estado (e_seg) y las
;  interrupciones cerradas, en una pila propia (en el bloque de estado).
; =============================================================================

RING_FR     equ NBUF * BUFSZ / 4    ; tramas del anillo (8192)
E_CHUNK     equ 1024                ; bytes de cada entrada de la lista
E_LEAD_IRQ  equ 1536                ; adelanto con interrupciones de la tarjeta
E_LEAD_TMR  equ 4608                ; ... y solo con el reloj (18,2 Hz)
E_GUARD     equ 128                 ; margen sobre lo que suena

; estructura de un canal de DMA cedido (la misma que en hspemm.asm)
SBD_BASE    equ 0
SBD_COUNT   equ 4
SBD_MODE    equ 6
SBD_CURADDR equ 8
SBD_CURCNT  equ 10
SBD_MASK    equ 12

; una voz
V_ACTIVE    equ 0
V_FLAGS     equ 1                   ; 1: 16 bits, 2: estereo, 4: con signo,
                                    ; 8: silencio (no lee del DMA)
V_POS       equ 2                   ; dd 16.16: tramas de origen pendientes
V_STEP      equ 6                   ; dd 16.16: origen por trama de salida
V_CUR       equ 10                  ; dd cursor (trama absoluta)
V_L         equ 14
V_R         equ 16
V_GL        equ 18                  ; ganancia (256 = 1)
V_GR        equ 20
V_FETCH     equ 22                  ; rutina: siguiente trama -> V_L, V_R
V_PL        equ 24                  ; la trama anterior (se interpola)
V_PR        equ 26
V_SIZE      equ 28

; ---- estado: en un bloque aparte, alineado a pagina (segmento e_seg, en GS
; dentro de las rutinas) y tambien la pila. No va junto al codigo: escribir a
; cada muestra en una pagina con codigo es lentisimo en los emuladores (QEMU
; revisa su cache de traduccion en cada escritura) y en los procesadores
; modernos (vaciado de la cola por «codigo que se modifica»).
        absolute 0
e_irqpend   resb 1                  ; bit 0: 8 bits, bit 1: 16 bits
e_irqedge   resb 1                  ; IRQ por entregar (cuantas)
e_virq      resb 1                  ; entregada y sin EOI
e_virqage   resb 1
e_ocw3      resb 1                  ; el programa lee el ISR del 8259
a_fmt       resb 1                  ; voz de la API: formato,
a_left      resw 1                  ; octetos que quedan
a_src       resd 1                  ; y de donde
e_irqcnt    resw 1
e_lead      resw 1
p_last      resw 1
p_abs       resd 1                  ; tramas que han sonado
w_abs       resd 1                  ; tramas escritas en el anillo
w_old       resd 1
d8          resb 16                 ; canal de 8 bits (DMA 1)
d16         resb 16                 ; canal de 16 bits (DMA 5)
v_pcm       resb V_SIZE
v_api       resb V_SIZE
; DSP
d_dma       resw 1
d_rate      resw 1
d_blk       resw 1
d_left      resw 1
d_auto      resb 1
d_paused    resb 1
d_rst       resb 1
d_cmd       resb 1
d_need      resb 1
d_argn      resb 1
d_args      resb 4
d_spk       resb 1
d_test      resb 1
d_last      resb 1
d_wbusy     resb 1                  ; [SB-EOI] lecturas de 22Ch (el bit de ocupado)
e_irqqh     resb 1                  ; [SB-EOI] primera de la cola de IRQ
e_dirq      resb 1                  ; [SB-EOI] IRQ en la que se ha entregado
            alignb 4
e_irqq      resd 8                  ; [SB-EOI] cuándo suena el final de cada
                                    ; bloque (trama absoluta; 0 = ya)
d_qh        resb 1
d_qn        resb 1
d_q         resb 16
; mezclador
m_idx       resb 1
m_reg       resb 0x48
fm_gl       resw 1
fm_gr       resw 1
            alignb 4
%include "hspopl_st.inc"
            alignb 4
            resb 512                ; pila
e_stack_top:
ST_SIZE     equ $
ST_MAX      equ 3072                ; el bloque de estado (y detras, en la
                                    ; misma pagina, la lista de 1 KB)
%if ST_SIZE > ST_MAX
%error "el estado no cabe"
%endif
        section .text

; ---- datos que no cambian mientras suena (junto al codigo) ----
e_seg       dw 0                    ; segmento del bloque de estado
e_irq       db 5                    ; IRQ de la tarjeta emulada
e_hwirq     db 0                    ; IRQ del HD Audio / AC'97 (0 = ninguna)
e_rtc       db 0                    ; [SB-EOI] 1: sin IRQ de la tarjeta, el RTC
e_doinj     db 0
e_wason     db 0                    ; parada por Windows
e_tsrpar    dw 0
e_sp        dw 0                    ; (seguidos: se cargan con LES)
e_ss        dw 0
e_ax        dw 0
e_vec       dd 0
e_old08     dd 0
e_oldhw     dd 0
io_info     dw 0
io_port     dw 0
io_eax      dd 0
m_lut       db 0, 1, 2, 2, 3, 3, 4, 5, 6, 8, 10, 13, 16, 20, 25, 32
            db 40, 51, 64, 81, 102, 128, 161, 203
; puertos que se piden al monitor
e_ranges    dw 0x220, 0x22F, 0x388, 0x38B, 0x20, 0x20
.none       dw 0xFFFF

%macro E_ENTER 0
        mov [cs:e_ss], ss
        mov [cs:e_sp], sp
        mov [cs:e_ax], ax
        mov ax, [cs:e_seg]
        mov ss, ax
        mov sp, e_stack_top
        mov ax, [cs:e_ax]
        pushad
        push ds
        push es
        push fs
        push gs
        push cs
        pop ds
        push ss
        pop gs
        mov fs, [hda_wseg]
        cld
%endmacro
; (antes de salir se decide si hay que entregar la IRQ de la tarjeta)
%macro E_LEAVE 0
        call e_decide
        pop gs
        pop fs
        pop es
        pop ds
        popad
        mov ss, [cs:e_ss]
        mov sp, [cs:e_sp]
%endmacro

; =============================================================================
;  Entradas
; =============================================================================
; ---- E/S de un programa a un puerto vigilado (la deja aqui el monitor) ----
e_io:
        pop word [cs:io_info]
        pop word [cs:io_port]
        mov [cs:io_eax], eax
        E_ENTER
        mov dx, [io_port]
        test byte [io_info], 1
        jnz .out
        call e_in
        mov [io_eax], al
        test byte [io_info], 6
        jz .done
        mov dx, [io_port]
        inc dx
        call e_in
        mov [io_eax+1], al
        jmp .done
.out:
        mov al, [io_eax]
        call e_out
        test byte [io_info], 6
        jz .done
        mov dx, [io_port]
        inc dx
        mov al, [io_eax+1]
        call e_out
.done:
        E_LEAVE
        mov eax, [cs:io_eax]
; ---- salida comun: en la pila, el marco de la interrupcion (IP, CS,
; indicadores). Si e_decide ha visto que hay una IRQ de la tarjeta por
; entregar y que el programa interrumpido tenia las interrupciones abiertas,
; se sigue en su rutina, como si la IRQ acabara de llegar ----
e_exit:
        cmp byte [cs:e_doinj], 0
        je .iret
        jmp far [cs:e_vec]
.iret:
        iret

e_decide:
        mov byte [e_doinj], 0
        cmp byte [gs:e_irqedge], 0
        je .r
        cmp byte [gs:e_virq], 0
        jne .r
        ; [SB-EOI] la IRQ de un bloque, cuando suena su final (no cuando se
        ; ha leído del DMA, que es hasta 100 ms antes): si no, al leer de
        ; golpe salían varias seguidas y, si el manejador del programa abre
        ; las interrupciones, se anidaban (con DOS/4GW: «pila de modo real
        ; agotada» en HSPDPMI)
        movzx bx, byte [gs:e_irqqh]
        shl bx, 2
        mov eax, [gs:e_irqq+bx]
        test eax, eax
        jz .now
        sub eax, [gs:p_abs]
        jg .r
.now:
        les bx, [e_sp]
        test byte [es:bx+5], 2      ; IF del programa
        jz .r
        in al, 0x21
        mov ah, al
        mov cl, [e_irq]
        shr al, cl
        test al, 1                  ; tapada en el 8259
        jz .use
        ; [SB-EOI] tapada: si el programa tiene abierta la otra de siempre
        ; de la SB (la 7, o la 5 si la nuestra es la 7), la espera allí (Doom
        ; y Duke Nukem 3D vienen configurados con la 7): se entrega en esa
        mov cl, 7
        cmp byte [e_irq], 7
        jne .alt
        mov cl, 5
.alt:
        cmp cl, [e_hwirq]
        je .r
        mov al, ah
        shr al, cl
        test al, 1
        jnz .r
.use:
        mov [gs:e_dirq], cl         ; [SB-EOI]
        dec byte [gs:e_irqedge]
        inc byte [gs:e_irqqh]       ; [SB-EOI]
        and byte [gs:e_irqqh], 7    ; [SB-EOI]
        mov byte [gs:e_virq], 1
        mov byte [gs:e_virqage], 0
        xor ax, ax
        mov es, ax
        movzx bx, cl                ; [SB-EOI] (antes, siempre e_irq)
        add bx, 8
        shl bx, 2
        mov eax, [es:bx]
        mov [e_vec], eax
        mov byte [e_doinj], 1
.r:
        ret

; ---- reloj ----
e_int08:
        pushf
        call far [cs:e_old08]
        cmp byte [cs:e_on], 0
        je .iret
        E_ENTER
        ; con interrupciones de la tarjeta, menos adelanto
        mov ax, E_LEAD_TMR
        cmp word [gs:e_irqcnt], 2
        jb .l
        mov ax, E_LEAD_IRQ
.l:
        mov [gs:e_lead], ax
        mov word [gs:e_irqcnt], 0
        ; una IRQ entregada que nadie cierra en medio segundo: se olvida
        cmp byte [gs:e_virq], 0
        je .nv
        inc byte [gs:e_virqage]
        cmp byte [gs:e_virqage], 10
        jb .nv
        mov byte [gs:e_virq], 0
.nv:
        call e_rtc_arm              ; [SB-EOI]
        call e_gains
        call e_pump
        E_LEAVE
        jmp e_exit
.iret:
        iret

; ---- interrupcion del HD Audio / AC'97 ----
e_hwisr:
        cmp byte [cs:e_on], 0
        je .chain
        E_ENTER
        call e_hw_ack
        jc .no
        mov al, 0x20
        cmp byte [e_hwirq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        inc word [gs:e_irqcnt]
        call e_pump
        E_LEAVE
        jmp e_exit
.no:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        mov ss, [cs:e_ss]
        mov sp, [cs:e_sp]
.chain:
        jmp far [cs:e_oldhw]

; ---- [SB-EOI] el reloj de tiempo real (IRQ 8, interrupción periódica a
; 256 Hz), cuando la tarjeta no tiene IRQ (BIOS que no se la dan: el HD
; Audio del ThinkPad A275). Sin él, HSPSND solo tiene el control 18 veces
; por segundo, en el reloj, y si el programa lo engancha y llama al de antes
; desde su manejador (con las interrupciones cerradas), la IRQ de la tarjeta
; emulada no se podía entregar hasta que tocara un puerto de la SB con las
; interrupciones abiertas: los sonidos acababan tarde o nunca. Así llega
; con las interrupciones abiertas cada 3,9 ms: se entrega enseguida.
; Si la BIOS está esperando con el RTC (INT 15h AH=83h/86h), se le deja
; (y a 1024 Hz) ----
e_rtcisr:
        cmp byte [cs:e_on], 0
        je .chain
        push ax
        push ds
        xor ax, ax
        mov ds, ax
        test byte [0x4A0], 1        ; espera de la BIOS en curso
        pop ds
        jnz .bios
        mov al, 0x0C
        out 0x70, al
        in al, 0x71                 ; (leer C quita el aviso)
        mov al, 0x20
        out 0xA0, al
        out 0x20, al
        pop ax
        E_ENTER
        inc word [gs:e_irqcnt]
        call e_pump
        E_LEAVE
        jmp e_exit
.bios:
        mov al, 6                   ; la BIOS cuenta con 1024 Hz
        call e_rtc_rate
        pop ax
.chain:
        jmp far [cs:e_oldhw]

; [SB-EOI] frecuencia del RTC: AL = 6 (1024 Hz) u 8 (256 Hz). Usa AX.
e_rtc_rate:
        mov ah, al
        mov al, 0x0A
        out 0x70, al
        in al, 0x71
        and al, 0x0F
        cmp al, ah
        je .r
        mov al, 0x0A
        out 0x70, al
        in al, 0x71
        and al, 0xF0
        or ah, al
        mov al, 0x0A
        out 0x70, al
        mov al, ah
        out 0x71, al
.r:
        ret

; [SB-EOI] (re)encender la interrupción periódica del RTC, a 256 Hz (cada
; 3,9 ms: más deprisa se anidan las interrupciones en los equipos lentos)
; si no hay una espera de la BIOS (INT 15h AH=83h/86h, que la quiere a
; 1024 Hz y la apaga al acabar). Desde el reloj (INT 08h) y al arrancar,
; con las interrupciones cerradas.
e_rtc_arm:
        cmp byte [e_rtc], 0
        je .r
        push ds
        xor ax, ax
        mov ds, ax
        test byte [0x4A0], 1
        pop ds
        jnz .r
        mov al, 8
        call e_rtc_rate
        mov al, 0x0B
        out 0x70, al
        in al, 0x71
        test al, 0x40
        jnz .r
        or al, 0x40
        mov ah, al
        mov al, 0x0B
        out 0x70, al
        mov al, ah
        out 0x71, al
        mov al, 0x0C
        out 0x70, al
        in al, 0x71
        in al, 0xA1                 ; IRQ 8 y la cascada (IRQ 2), abiertas
        and al, 0xFE
        out 0xA1, al
        in al, 0x21
        and al, 0xFB
        out 0x21, al
.r:
        ret
; [SB-EOI] apagarla (al parar la emulación)
e_rtc_off:
        cmp byte [e_rtc], 0
        je .r
        push ds
        xor ax, ax
        mov ds, ax
        test byte [0x4A0], 1
        pop ds
        jnz .r
        mov al, 0x0B
        out 0x70, al
        in al, 0x71
        and al, 0xBF
        mov ah, al
        mov al, 0x0B
        out 0x70, al
        mov al, ah
        out 0x71, al
        mov al, 0x0C
        out 0x70, al
        in al, 0x71
        mov al, 6                   ; como la deja la BIOS
        call e_rtc_rate
.r:
        ret

; CF = 1 si la interrupcion no es del flujo de salida; si lo es, la apaga
e_hw_ack:
        cmp byte [dev_type], 4
        jne .ac
        mov dx, [hda_sd]
        add dx, 3
        call hrd8
        test al, 0x1C
        jz .no
        call hwr8
        clc
        ret
.ac:
        mov dx, [nabm]
        add dx, 0x16
        in ax, dx
        test al, 0x1C
        jz .no
        out dx, ax
        clc
        ret
.no:
        stc
        ret

; ---- API de HSPSND (AH = 1 reproducir, 2 esperar, 3 parar) con la tarjeta
; emulada en marcha: una voz mas del anillo ----
e_api:
        push gs
        mov gs, [cs:e_seg]
        cmp ah, 1
        je .play
        cmp ah, 2
        je .wait
        mov word [gs:a_left], 0
        mov byte [gs:v_api+V_ACTIVE], 0
        jmp .ret
.play:
        pushad
        pushf
        cli
        mov [gs:a_fmt], al
        mov [gs:a_left], cx
        mov [gs:a_src], si
        mov [gs:a_src+2], ds
        movzx eax, dx
        shl eax, 16
        xor edx, edx
        movzx ecx, word [cs:hw_rate]
        div ecx
        mov [gs:v_api+V_STEP], eax
        mov ax, [cs:soft_mul]
        mov [gs:v_api+V_GL], ax
        mov [gs:v_api+V_GR], ax
        mov byte [gs:v_api+V_ACTIVE], 1
        popf
        sti
        call tmo_start
.pw:
        cmp word [gs:a_left], 0
        je .pd
        cmp byte [gs:v_api+V_ACTIVE], 0
        je .pd
        call tmo_check
        jnc .pw
        mov byte [gs:v_api+V_ACTIVE], 0
.pd:
        popad
        jmp .ret
.wait:
        push eax
        sti
        call tmo_start
.ww:
        cmp byte [gs:v_api+V_ACTIVE], 0
        jne .wn
        mov eax, [gs:v_api+V_CUR]
        sub eax, [gs:p_abs]
        jle .wd
.wn:
        call tmo_check
        jnc .ww
.wd:
        pop eax
.ret:
        pop gs
        clc
        retf

; siguiente trama de la voz de la API (BX = v_api)
api_fetch:
        mov cx, 1
        mov dl, [gs:a_fmt]
        test dl, 1
        jz .b
        shl cx, 1
.b:
        test dl, 2
        jz .c
        shl cx, 1
.c:
        cmp [gs:a_left], cx
        jb .end
        push ds
        lds si, [gs:a_src]
        test dl, 1
        jnz .s16
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        mov di, ax
        test dl, 2
        jz .st
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        jmp .st
.s16:
        lodsw
        mov di, ax
        test dl, 2
        jz .st
        lodsw
.st:
        ; DI = izquierdo, AX = derecho (o el mismo)
        mov dx, ds
        pop ds
        mov [gs:bx+V_L], di
        mov [gs:bx+V_R], ax
        mov ax, si
        shr ax, 4
        add dx, ax
        and si, 15
        mov [gs:a_src], si
        mov [gs:a_src+2], dx
        sub [gs:a_left], cx
        clc
        ret
.end:
        mov word [gs:a_left], 0
        mov byte [gs:bx+V_ACTIVE], 0
        stc
        ret

; =============================================================================
;  El anillo
; =============================================================================
; posicion de la tarjeta en el anillo -> AX (tramas)
e_hwpos:
        cmp byte [dev_type], 4
        jne .ac
        mov dx, [hda_sd]
        add dx, 4
        call hrd32
        shr eax, 2
        and ax, RING_FR - 1
        ret
.ac:
        mov dx, [nabm]
        add dx, 0x14
        in al, dx
        mov bl, al
        add dx, 4
        in ax, dx
        mov cx, ax
        sub dx, 4
        in al, dx
        cmp al, bl
        jne .ac
        ; que no se pare: la ultima entrada valida, la anterior a la actual
        dec al
        and al, 31
        inc dx
        out dx, al
        mov ax, E_CHUNK / 2
        sub ax, cx
        jnc .p
        xor ax, ax
.p:
        shr ax, 1
        and bx, 31
        shl bx, 8
        add ax, bx
        and ax, RING_FR - 1
        ret

; llenar el anillo hasta e_lead tramas por delante de lo que suena
e_pump:
        call e_hwpos
        mov dx, ax
        sub ax, [gs:p_last]
        and ax, RING_FR - 1
        mov [gs:p_last], dx
        movzx eax, ax
        add [gs:p_abs], eax
        ; ¿se ha quedado atras lo escrito?
        mov eax, [gs:w_abs]
        sub eax, [gs:p_abs]
        cmp eax, E_GUARD
        jge .ok
        mov eax, [gs:p_abs]
        add eax, E_GUARD
        mov [gs:w_abs], eax
.ok:
        mov eax, [gs:p_abs]
        movzx edx, word [gs:e_lead]
        add eax, edx
        sub eax, [gs:w_abs]
        jle .voices
        ; AX = tramas nuevas: a cero
        mov cx, ax
        mov eax, [gs:w_abs]
        mov [gs:w_old], eax
        movzx edx, cx
        add [gs:w_abs], edx
        push cs
        pop es
        mov di, ax
        and di, RING_FR - 1
        mov dx, RING_FR
        sub dx, di                  ; tramas hasta el final del anillo
        shl di, 2
        add di, [buf_off]
        xor eax, eax
        cmp cx, dx
        jbe .z1
        push cx
        mov cx, dx
        rep stosd
        pop cx
        sub cx, dx
        mov di, [buf_off]
.z1:
        rep stosd
        call fm_render
.voices:
        mov bx, v_pcm
        call v_render
        mov bx, v_api
        call v_render
        ret

; sumar al anillo la voz BX, de su cursor a w_abs
v_render:
        cmp byte [gs:bx+V_ACTIVE], 0
        je .r
        mov eax, [gs:p_abs]
        add eax, E_GUARD
        mov edx, [gs:bx+V_CUR]
        sub edx, eax
        jns .loop
        mov [gs:bx+V_CUR], eax         ; llega tarde: lo antes posible
.loop:
        mov eax, [gs:w_abs]
        sub eax, [gs:bx+V_CUR]
        jle .r
.f:
        cmp word [gs:bx+V_POS+2], 0
        je .out
        mov eax, [gs:bx+V_L]        ; (izquierda y derecha)
        mov [gs:bx+V_PL], eax
        call [gs:bx+V_FETCH]
        jc .r
        dec word [gs:bx+V_POS+2]
        jmp .f
.out:
        mov di, [gs:bx+V_CUR]
        and di, RING_FR - 1
        shl di, 2
        add di, [buf_off]
        ; entre la trama anterior y la actual, segun la fraccion de V_POS
        ; (interpolacion lineal), y por la ganancia
        movzx ecx, word [gs:bx+V_POS]
        movsx eax, word [gs:bx+V_L]
        movsx edx, word [gs:bx+V_PL]
        sub eax, edx
        imul eax, ecx
        sar eax, 16
        add eax, edx
        movzx edx, word [gs:bx+V_GL]
        imul eax, edx
        sar eax, 8
        add [di], ax
        jno .l1
        mov word [di], 0x7FFF
        test ah, ah
        jns .l1
        mov word [di], 0x8000
.l1:
        movsx eax, word [gs:bx+V_R]
        movsx edx, word [gs:bx+V_PR]
        sub eax, edx
        imul eax, ecx
        sar eax, 16
        add eax, edx
        movzx edx, word [gs:bx+V_GR]
        imul eax, edx
        sar eax, 8
        add [di+2], ax
        jno .r1
        mov word [di+2], 0x7FFF
        test ah, ah
        jns .r1
        mov word [di+2], 0x8000
.r1:
        inc dword [gs:bx+V_CUR]
        mov eax, [gs:bx+V_STEP]
        add [gs:bx+V_POS], eax
        jmp .loop
.r:
        ret

; ganancias de la voz de la SB y de la FM: maestro x voz (o FM) del
; mezclador x volumen por programa de HSPSND
e_gains:
        mov al, [gs:m_reg+0x30]
        mov ah, [gs:m_reg+0x32]
        call .g
        mov [gs:v_pcm+V_GL], ax
        mov al, [gs:m_reg+0x31]
        mov ah, [gs:m_reg+0x33]
        call .g
        mov [gs:v_pcm+V_GR], ax
        mov al, [gs:m_reg+0x30]
        mov ah, [gs:m_reg+0x34]
        call .g
        mov [gs:fm_gl], ax
        mov al, [gs:m_reg+0x31]
        mov ah, [gs:m_reg+0x35]
        call .g
        mov [gs:fm_gr], ax
        ret
.g:
        call .lut
        xchg al, ah
        mov cx, ax                  ; CH = factor del primero
        call .lut
        mul ch                      ; 0..65025
        add ax, 255
        mov al, ah
        xor ah, ah                  ; 0..255
        cmp al, 255
        jb .g1
        inc ax
.g1:
        mul word [soft_mul]
        mov al, ah
        mov ah, dl
        ret
; AL = registro de volumen (5 bits altos) -> AL = factor (0..255)
.lut:
        push bx
        shr al, 3
        cmp al, 24
        jb .t
        mov al, 255
        pop bx
        ret
.t:
        movzx bx, al
        mov al, [m_lut+bx]
        pop bx
        ret

; =============================================================================
;  Puertos
; =============================================================================
; DX = puerto -> AL
e_in:
        cmp dx, 0x20
        je .pic
        cmp dx, 0x388
        jae .adlib
        and dx, 0x0F
        cmp dl, 0x0A
        je .data
        cmp dl, 0x0C
        je .wst
        cmp dl, 0x0E
        je .rst
        cmp dl, 0x0F
        je .ack16
        cmp dl, 0x04
        je .midx
        cmp dl, 0x05
        je mix_read
        cmp dl, 0x00
        je .fm
        cmp dl, 0x08
        je .fm
        mov al, 0xFF
        ret
.adlib:
        cmp dx, 0x388
        jne .ff
.fm:
        jmp fm_status
.ff:
        mov al, 0xFF
        ret
.midx:
        mov al, [gs:m_idx]
        ret
.data:
        cmp byte [gs:d_qn], 0
        je .last
        movzx bx, byte [gs:d_qh]
        mov al, [gs:d_q+bx]
        mov [gs:d_last], al
        inc bl
        and bl, 15
        mov [gs:d_qh], bl
        dec byte [gs:d_qn]
.last:
        mov al, [gs:d_last]
        ret
.wst:
        ; [SB-EOI] listo para recibir, pero el bit 7 (ocupado) se enciende una
        ; de cada cuatro lecturas, como en la tarjeta de verdad, donde parpadea
        ; aunque esté libre: hay juegos que esperan a verlo encendido y luego
        ; apagado antes de escribir (Prince of Persia, para parar un sonido; con
        ; el bit siempre apagado se quedaba esperando a que el sonido acabara)
        inc byte [gs:d_wbusy]
        mov al, [gs:d_wbusy]
        and al, 3
        cmp al, 3
        mov al, 0x7F
        jne .r
        mov al, 0xFF
        ret
.rst:
        and byte [gs:e_irqpend], ~1
        mov al, 0x7F
        cmp byte [gs:d_qn], 0
        je .r
        mov al, 0xFF
.r:
        ret
.ack16:
        and byte [gs:e_irqpend], ~2
        mov al, 0xFF
        ret
.pic:
        in al, 0x20
        cmp byte [gs:e_ocw3], 0
        je .r
        cmp byte [gs:e_virq], 0
        je .r
        push cx
        mov ah, 1
        mov cl, [gs:e_dirq]         ; [SB-EOI]
        shl ah, cl
        or al, ah
        pop cx
        ret

; DX = puerto, AL = valor
e_out:
        cmp dx, 0x20
        je .pic
        cmp dx, 0x388
        jae .adlib
        and dx, 0x0F
        cmp dl, 0x0C
        je dsp_write
        cmp dl, 0x06
        je .reset
        cmp dl, 0x04
        je .midx
        cmp dl, 0x05
        je mix_write
        cmp dl, 0x00
        je .fma
        cmp dl, 0x08
        je .fma
        cmp dl, 0x01
        je .fmd
        cmp dl, 0x09
        je .fmd
        ret
.adlib:
        cmp dx, 0x388
        je .fma
        cmp dx, 0x389
        jne .r
.fmd:
        jmp fm_write
.fma:
        mov [gs:fm_addr], al
.r:
        ret
.midx:
        mov [gs:m_idx], al
        ret
.reset:
        test al, 1
        jz .rs0
        mov byte [gs:d_rst], 1
        ret
.rs0:
        cmp byte [gs:d_rst], 0
        je .r
        mov byte [gs:d_rst], 0
        jmp dsp_reset
; 8259: el EOI de la IRQ emulada no llega al controlador de verdad
.pic:
        mov ah, al
        and ah, 0x18
        cmp ah, 0x08                ; OCW3: ¿leer el ISR?
        jne .eoi
        mov ah, al
        and ah, 3
        cmp ah, 2
        jb .pass
        and ah, 1
        mov [gs:e_ocw3], ah
        jmp .pass
.eoi:
        cmp byte [gs:e_virq], 0
        je .pass
        mov ah, [gs:e_dirq]         ; [SB-EOI]
        or ah, 0x60
        cmp al, ah                  ; EOI especifico de la nuestra
        je .eat
        cmp al, 0x20
        jne .pass
        ; EOI corriente: es de la nuestra si en el 8259 no hay en servicio
        ; ninguna de mas prioridad
        push ax
        mov al, 0x0B
        out 0x20, al
        in al, 0x20
        mov ah, al
        mov al, 0x0A
        add al, [gs:e_ocw3]
        out 0x20, al
        mov cl, [gs:e_dirq]         ; [SB-EOI]
        mov al, 1
        shl al, cl
        dec al
        test ah, al
        pop ax
        jnz .pass
.eat:
        mov byte [gs:e_virq], 0
        ret
.pass:
        out 0x20, al
        ret

; =============================================================================
;  DSP
; =============================================================================
dsp_reset:
        xor ax, ax
        mov [gs:v_pcm+V_ACTIVE], al
        mov [gs:d_need], al
        mov [gs:d_auto], al
        mov [gs:d_paused], al
        mov [gs:d_spk], al
        mov [gs:e_irqpend], al
        mov [gs:e_irqedge], al
        mov [gs:e_irqqh], al        ; [SB-EOI]
        mov [gs:d_qh], al
        mov [gs:d_qn], al
        mov al, 0xAA
; AL -> cola de lectura del DSP
dsp_queue:
        push bx
        mov bl, [gs:d_qh]
        add bl, [gs:d_qn]
        and bx, 15
        mov [gs:d_q+bx], al
        cmp byte [gs:d_qn], 16
        jae .f
        inc byte [gs:d_qn]
.f:
        pop bx
        ret

; octetos que siguen a cada orden (las que no estan: ninguno)
dsp_lens    db 0x10, 1, 0x14, 2, 0x16, 2, 0x17, 2, 0x24, 2, 0x38, 1
            db 0x40, 1, 0x41, 2, 0x42, 2, 0x48, 2, 0x74, 2, 0x75, 2
            db 0x76, 2, 0x77, 2, 0x80, 2, 0xE0, 1, 0xE2, 1, 0xE4, 1
            db 0x05, 2, 0x0E, 2, 0x0F, 1, 0

; AL = octeto escrito en 22Ch
dsp_write:
        cmp byte [gs:d_need], 0
        jne .arg
        mov [gs:d_cmd], al
        mov byte [gs:d_argn], 0
        cmp al, 0xB0
        jb .tab
        cmp al, 0xCF
        ja .tab
        mov byte [gs:d_need], 3
        ret
.tab:
        mov si, dsp_lens
.t:
        mov ah, [si]
        test ah, ah
        jz dsp_exec
        cmp ah, al
        je .tf
        add si, 2
        jmp .t
.tf:
        mov al, [si+1]
        mov [gs:d_need], al
        ret
.arg:
        movzx bx, byte [gs:d_argn]
        mov [gs:d_args+bx], al
        inc byte [gs:d_argn]
        dec byte [gs:d_need]
        jz dsp_exec
        ret

dsp_exec:
        mov al, [gs:d_cmd]
        cmp al, 0xB0
        jb .old
        cmp al, 0xCF
        ja .old
        ; ---- SB16: Bxh 16 bits, Cxh 8 bits; args: modo, longitud - 1 ----
        test al, 0x08
        jnz .r                      ; grabacion: no hay
        mov ah, [gs:d_args]            ; modo: bit 4 con signo, bit 5 estereo
        xor bl, bl
        cmp al, 0xC0
        jae .n16
        or bl, 1
.n16:
        test ah, 0x20
        jz .nst
        or bl, 2
.nst:
        test ah, 0x10
        jz .nsg
        or bl, 4
.nsg:
        mov cx, [gs:d_args+1]
        mov [gs:d_blk], cx
        mov ah, al
        shr ah, 2
        and ah, 1                   ; autoinicio
        mov al, bl
        movzx ecx, word [gs:d_rate]
        jmp v_start
.old:
        cmp al, 0x14
        je .p14
        cmp al, 0x91
        je .p91
        cmp al, 0x1C
        je .p1c
        cmp al, 0x90
        je .p1c
        cmp al, 0x40
        je .tc
        cmp al, 0x41
        je .rate
        cmp al, 0x42
        je .rate
        cmp al, 0x48
        je .blk
        cmp al, 0x80
        je .sil
        cmp al, 0x16
        je .sil
        cmp al, 0x17
        je .sil
        cmp al, 0x74
        jb .o2
        cmp al, 0x77
        jbe .sil                    ; ADPCM: silencio de esa duracion
.o2:
        cmp al, 0xD0
        je .pause
        cmp al, 0xD5
        je .pause
        cmp al, 0xD4
        je .cont
        cmp al, 0xD6
        je .cont
        cmp al, 0xD1
        je .spk1
        cmp al, 0xD3
        je .spk0
        cmp al, 0xD8
        je .spkq
        cmp al, 0xD9
        je .xauto
        cmp al, 0xDA
        je .xauto
        cmp al, 0xE0
        je .e0
        cmp al, 0xE1
        je .e1
        cmp al, 0xE3
        je .e3
        cmp al, 0xE4
        je .e4
        cmp al, 0xE8
        je .e8
        cmp al, 0xF2
        je .f2
        cmp al, 0xF3
        je .f3
        cmp al, 0xF8
        je .q0
        cmp al, 0x20
        je .adc
.r:
        ret
.p14:
        mov cx, [gs:d_args]
        mov [gs:d_blk], cx
.p91:
        xor ah, ah
        jmp .p8
.p1c:
        mov ah, 1
.p8:
        ; 8 bits sin signo; estereo si lo dice el mezclador de la SB Pro (y
        ; entonces la frecuencia programada es la de las dos canales juntas)
        xor al, al
        movzx ecx, word [gs:d_rate]
        test byte [gs:m_reg+0x0E], 2
        jz v_start
        or al, 2
        shr ecx, 1
        jmp v_start
.tc:
        movzx cx, byte [gs:d_args]
        mov ax, 256
        sub ax, cx
        mov cx, ax
        cmp cx, 16                  ; (el cociente tiene que caber)
        jae .tcd
        mov cx, 16
.tcd:
        mov dx, 0x000F
        mov ax, 0x4240              ; 1000000
        div cx
        mov [gs:d_rate], ax
        ret
.rate:
        mov ax, [gs:d_args]
        xchg al, ah
        mov [gs:d_rate], ax
        ret
.blk:
        mov ax, [gs:d_args]
        mov [gs:d_blk], ax
        ret
.sil:
        mov cx, [gs:d_args]
        mov [gs:d_blk], cx
        mov ax, 0x0008
        movzx ecx, word [gs:d_rate]
        jmp v_start
.pause:
        mov byte [gs:d_paused], 1
        ret
.cont:
        mov byte [gs:d_paused], 0
        jmp e_pump
.spk1:
        mov byte [gs:d_spk], 0xFF
        ret
.spk0:
        mov byte [gs:d_spk], 0
        ret
.spkq:
        mov al, [gs:d_spk]
        jmp dsp_queue
.xauto:
        mov byte [gs:d_auto], 0
        ; [SB-EOI] si ya hay bloques leídos del DMA cuya IRQ aún no se ha
        ; entregado (no han sonado), para el programa el bloque en curso es
        ; el primero de ellos: su IRQ será la última (como en la tarjeta)
        cmp byte [gs:e_irqedge], 0
        je .xr
        mov byte [gs:e_irqedge], 1
        mov byte [gs:v_pcm+V_ACTIVE], 0
.xr:
        ret
.e0:
        mov al, [gs:d_args]
        not al
        jmp dsp_queue
.e1:
        mov al, 4
        call dsp_queue
        mov al, 5
        jmp dsp_queue
.e3:
.q0:
        xor al, al
        jmp dsp_queue
.e4:
        mov al, [gs:d_args]
        mov [gs:d_test], al
        ret
.e8:
        mov al, [gs:d_test]
        jmp dsp_queue
.adc:
        mov al, 0x80
        jmp dsp_queue
.f2:
        or byte [gs:e_irqpend], 1
        jmp irq_raise
.f3:
        or byte [gs:e_irqpend], 2
; una IRQ mas por entregar (si se acumulan, se entregan una tras otra).
; [SB-EOI] irq_raise: ya; irq_raise_v: cuando suene la trama V_CUR de la voz
; BX. Conservan todo.
irq_raise_v:
        push eax
        mov eax, [gs:bx+V_CUR]
        test eax, eax
        jnz irq_raise.st
        inc eax
        jmp irq_raise.st
irq_raise:
        push eax
        xor eax, eax
.st:
        cmp byte [gs:e_irqedge], 8
        jae .r
        push bx
        movzx bx, byte [gs:e_irqqh]
        add bl, [gs:e_irqedge]
        and bx, 7
        shl bx, 2
        mov [gs:e_irqq+bx], eax
        pop bx
        inc byte [gs:e_irqedge]
.r:
        pop eax
        ret

; arrancar la voz de la SB: AL = V_FLAGS, AH = autoinicio, ECX = Hz,
; d_blk = longitud - 1
v_start:
        mov [gs:v_pcm+V_FLAGS], al
        mov [gs:d_auto], ah
        mov word [gs:d_dma], d8
        test al, 1
        jz .d8
        mov word [gs:d_dma], d16
.d8:
        mov ax, [gs:d_blk]
        mov [gs:d_left], ax
        test ecx, ecx
        jnz .hz
        mov cx, 11025
.hz:
        mov eax, ecx
        shl eax, 16
        xor edx, edx
        movzx ecx, word [hw_rate]
        div ecx
        mov [gs:v_pcm+V_STEP], eax
        mov byte [gs:d_paused], 0
        mov byte [gs:v_pcm+V_ACTIVE], 1
        call e_gains
        jmp e_pump

; siguiente trama de la voz de la SB (BX = v_pcm); CF = 1 si no hay
pcm_fetch:
        cmp byte [gs:d_paused], 0
        jne .no
        cmp byte [gs:bx+V_ACTIVE], 0
        je .no
        mov si, [gs:d_dma]
        test byte [gs:bx+V_FLAGS], 8
        jnz .sil
        call .unit
        jc .no
        mov [gs:bx+V_L], ax
        mov [gs:bx+V_R], ax
        test byte [gs:bx+V_FLAGS], 2
        jz .done
        cmp byte [gs:bx+V_ACTIVE], 0
        je .done
        call .unit
        jc .done
        mov [gs:bx+V_R], ax
.done:
        clc
        ret
.no:
        stc
        ret
.sil:
        xor ax, ax
        mov [gs:bx+V_L], ax
        mov [gs:bx+V_R], ax
        call .count
        clc
        ret
; una muestra del bufer del programa -> AX (16 bits con signo)
.unit:
        cmp byte [gs:si+SBD_MASK], 0
        jne .stall
        mov eax, [gs:si+SBD_BASE]
        test byte [gs:bx+V_FLAGS], 1
        jnz .u16
        and eax, 0x00FF0000
        mov ax, [gs:si+SBD_CURADDR]
        call .ptr
        mov al, 0x80
        jc .g8
        mov al, [es:di]
.g8:
        test byte [gs:bx+V_FLAGS], 4
        jnz .s8
        sub al, 0x80
.s8:
        mov ah, al
        xor al, al
        jmp .adv
.u16:
        and eax, 0x00FE0000
        movzx edx, word [gs:si+SBD_CURADDR]
        shl edx, 1
        add eax, edx
        call .ptr
        mov ax, 0x8000
        jc .g16
        mov ax, [es:di]
        test byte [gs:bx+V_FLAGS], 4
        jnz .adv
.g16:
        xor ax, 0x8000
.adv:
        inc word [gs:si+SBD_CURADDR]
        sub word [gs:si+SBD_CURCNT], 1
        jnc .cnt
        ; fin de la cuenta del 8237: autoinicio o canal tapado
        test byte [gs:si+SBD_MODE], 0x10
        jz .tcm
        mov dx, [gs:si+SBD_COUNT]
        mov [gs:si+SBD_CURCNT], dx
        mov edx, [gs:si+SBD_BASE]
        test byte [gs:bx+V_FLAGS], 1
        jz .rl
        shr edx, 1
.rl:
        mov [gs:si+SBD_CURADDR], dx
        jmp .cnt
.tcm:
        mov byte [gs:si+SBD_MASK], 1
.cnt:
        call .count
        clc
        ret
.stall:
        stc
        ret
; EAX = direccion lineal -> ES:DI (CF = 1 si no esta en el primer mega)
.ptr:
        cmp eax, 0x100000
        jae .far
        mov di, ax
        and di, 15
        shr eax, 4
        mov es, ax
        clc
        ret
.far:
        stc
        ret
; una muestra menos del bloque del DSP; al acabar, IRQ. Conserva AX.
.count:
        sub word [gs:d_left], 1
        jnc .cr
        mov dl, 1
        test byte [gs:bx+V_FLAGS], 1
        jz .i8
        mov dl, 2
.i8:
        or [gs:e_irqpend], dl
        call irq_raise_v            ; [SB-EOI]
        cmp byte [gs:d_auto], 0
        je .stop
        mov dx, [gs:d_blk]
        mov [gs:d_left], dx
        ret
.stop:
        mov byte [gs:bx+V_ACTIVE], 0
.cr:
        ret

; =============================================================================
;  Mezclador
; =============================================================================
mix_reset:
        push gs
        pop es
        mov di, m_reg
        mov cx, 0x48
        xor al, al
        rep stosb
        mov ax, 0xC0C0
        mov [gs:m_reg+0x30], ax
        mov [gs:m_reg+0x32], ax
        mov [gs:m_reg+0x34], ax
        jmp e_gains

; AL = dato para el registro m_idx
mix_write:
        movzx bx, byte [gs:m_idx]
        test bl, bl
        jz mix_reset
        cmp bl, 0x04                ; SB Pro: voz
        je .voz
        cmp bl, 0x22                ; ... maestro
        je .mas
        cmp bl, 0x26                ; ... FM
        je .fm
        cmp bl, 0x48
        jae .r
        mov [gs:m_reg+bx], al
        jmp e_gains
.voz:
        mov bx, 0x32
        jmp .pair
.mas:
        mov bx, 0x30
        jmp .pair
.fm:
        mov bx, 0x34
.pair:
        mov ah, al
        and al, 0xF0
        or al, 0x06
        mov [gs:m_reg+bx], al
        shl ah, 4
        or ah, 0x08
        mov [gs:m_reg+bx+1], ah
        jmp e_gains
.r:
        ret

mix_read:
        movzx bx, byte [gs:m_idx]
        cmp bl, 0x04
        je .voz
        cmp bl, 0x22
        je .mas
        cmp bl, 0x26
        je .fm
        cmp bl, 0x80
        je .irq
        cmp bl, 0x81
        je .dma
        cmp bl, 0x82
        je .ist
        mov al, 0xFF
        cmp bl, 0x48
        jae .r
        mov al, [gs:m_reg+bx]
.r:
        ret
.voz:
        mov bx, 0x32
        jmp .pair
.mas:
        mov bx, 0x30
        jmp .pair
.fm:
        mov bx, 0x34
.pair:
        mov al, [gs:m_reg+bx]
        and al, 0xF0
        mov ah, [gs:m_reg+bx+1]
        shr ah, 4
        or al, ah
        ret
.irq:
        mov al, 0x02                ; IRQ 5
        cmp byte [e_irq], 7
        jne .r
        mov al, 0x04
        ret
.dma:
        mov al, 0x22                ; DMA 1 y 5
        ret
.ist:
        mov al, [gs:e_irqpend]
        ret

; =============================================================================
;  Arranque (lo llama la instalacion al final: de aqui no se vuelve)
; =============================================================================
e_go:
        call e_start
        sti
        mov dx, [e_tsrpar]
        mov ax, 0x3100
        int 0x21

; Windows en modo 386 mejorado va a apagar el modo V86 (INT 2Fh AX=1605h):
; sin el monitor no hay puertos atrapados ni ventana para los registros del
; HD Audio, y una interrupcion de la tarjeta sin atender colgaria el equipo.
; Se para todo; al salir Windows (1606h), si el modo V86 ha vuelto, se
; arranca otra vez. Conservan todo.
e_winoff:
        pushad
        push ds
        push es
        push fs
        push gs
        push cs
        pop ds
        pushf
        cli
        cld
        call e_stop
        mov byte [e_wason], 1
        popf
        pop gs
        pop fs
        pop es
        pop ds
        popad
        ret
e_winon:
        pushad
        push ds
        push es
        push fs
        push gs
        push cs
        pop ds
        pushf
        cli
        cld
        smsw ax
        test al, 1
        jz .x
        mov byte [e_wason], 0
        call e_start
.x:
        popf
        pop gs
        pop fs
        pop es
        pop ds
        popad
        ret

; parar la tarjeta emulada: flujo, interrupciones, puertos y canales de DMA
e_stop:
        mov byte [e_on], 0
        call e_rtc_off              ; [SB-EOI]
        cmp byte [dev_type], 4
        jne .ac
        push fs
        call unreal
        mov dx, 0x20
        xor eax, eax
        call hwr32
        mov dx, [hda_sd]
        xor al, al
        call hwr8
        mov dx, [hda_sd]
        add dx, 3
        mov al, 0x1C
        call hwr8
        pop fs
        jmp .off
.ac:
        mov dx, [nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
.off:
        mov byte [started], 0
        xor bx, bx
        mov es, bx
        mov si, e_ranges.none
        mov ax, 0xDD01
        int 0x67
        xor di, di
        mov es, di
        mov bl, 1
        mov ax, 0xDD03
        int 0x67
        xor di, di
        mov es, di
        mov bl, 5
        mov ax, 0xDD03
        int 0x67
        ret

; poner en marcha: estado, anillo, tarjeta, canales de DMA y puertos.
; Vuelve con las interrupciones cerradas.
e_start:
        ; el anillo, en silencio
        push cs
        pop es
        mov di, [buf_off]
        mov cx, NBUF * BUFSZ / 2
        xor ax, ax
        cld
        rep stosw
        ; el bloque de estado, a cero y con sus valores iniciales
        cli
        mov es, [e_seg]
        mov gs, [e_seg]
        xor di, di
        mov cx, 4096                ; (la pagina entera, con la lista)
        xor al, al
        rep stosb
        mov word [gs:e_lead], E_LEAD_TMR
        mov word [gs:d_dma], d8
        mov word [gs:d_rate], 11025
        mov bx, v_pcm
        mov ax, pcm_fetch
        call .voice
        mov bx, v_api
        mov ax, api_fetch
        call .voice
        call mix_reset
        mov byte [e_on], 1
        cmp byte [dev_type], 4
        jne .ac
        push fs
        call unreal
        mov dx, 48000
        mov [cur_rate], dx
        call hda_rate_fmt
        call hda_setfmt
        call hda_start
        ; interrupciones del flujo
        mov ax, [hda_sd]
        sub ax, 0x80
        shr ax, 5
        mov cl, al
        mov eax, 1
        shl eax, cl
        or eax, 0x80000000
        cmp byte [e_hwirq], 0
        jne .ie
        xor eax, eax
.ie:
        mov dx, 0x20
        call hwr32
        pop fs
        jmp .on
.ac:
        mov word [cur_rate], 48000
        call ac_set_rate
        mov dx, [nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
        mov al, 2
        out dx, al
        mov cx, 0xFFFF
.rw:
        in al, dx
        test al, 2
        loopnz .rw
        mov di, [bdl_off]
        xor si, si
.bd:
        mov bx, si
        shl bx, 10
        add bx, [buf_off]
        call phys
        mov [di], eax
        mov word [di+4], E_CHUNK / 2
        mov word [di+6], 0x8000     ; interrupcion al acabarla
        add di, 8
        inc si
        cmp si, 32
        jb .bd
        mov bx, [bdl_off]
        call phys
        mov dx, [nabm]
        add dx, 0x10
        out dx, eax
        mov dx, [nabm]
        add dx, 0x15
        mov al, 31
        out dx, al
        inc dx
        mov ax, 0x1C
        out dx, ax
        mov dx, [nabm]
        add dx, 0x1B
        mov al, 0x01
        cmp byte [e_hwirq], 0
        je .ar
        mov al, 0x11
.ar:
        out dx, al
.on:
        mov byte [started], 1
        call fm_reset               ; (ya se sabe la frecuencia de salida)
        ; los canales de DMA y los puertos, al monitor
        mov es, [e_seg]
        mov di, d8
        mov bl, 1
        mov ax, 0xDD03
        int 0x67
        mov es, [e_seg]
        mov di, d16
        mov bl, 5
        mov ax, 0xDD03
        int 0x67
        push cs
        pop es
        mov bx, e_io
        mov si, e_ranges
        mov ax, 0xDD01
        int 0x67
        ; [SB-PM] el estado de la FM, para que HSPDPMI conteste él mismo las
        ; lecturas del estado de la AdLib de los programas en modo protegido
        ; (los juegos leen el 388h decenas de veces por cada escritura)
        movzx edx, word [e_seg]
        shl edx, 4
        add edx, fm_stat
        mov ax, 0xDD05
        int 0x67
        call e_rtc_arm              ; [SB-EOI] (sin IRQ de la tarjeta)
        ; abrir la IRQ de la tarjeta de verdad
        mov cl, [e_hwirq]
        test cl, cl
        jz .tsr
        cmp cl, 8
        jb .m1
        sub cl, 8
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0xA1
        and al, ah
        out 0xA1, al
        mov cl, 2
.m1:
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0x21
        and al, ah
        out 0x21, al
.tsr:
        ret
.voice:
        mov dword [gs:bx+V_POS], 0x10000
        mov dword [gs:bx+V_STEP], 0x10000
        mov word [gs:bx+V_GL], 256
        mov word [gs:bx+V_GR], 256
        mov [gs:bx+V_FETCH], ax
        ret
