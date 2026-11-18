; =============================================================================
;  HBLOG.COM - enseña y guarda el registro de la BIOS de HispaBIOS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
;  HispaBIOS (docs/HISPABIOS.md) apunta, mientras DOS funciona, lo que sirve
;  para depurar la pantalla y el reloj: cómo es la pantalla del firmware y lo
;  que cuesta pintarla, cuántas veces se ha dibujado y cuánto ha tardado,
;  si llega la IRQ 0, y cada llamada de VBE (INT 10h AX = 4Fxxh) con lo que
;  se ha contestado. Este programa lo pide por INT 15h AX = 4844h ('HD'), lo
;  enseña y lo guarda en \HBLOG.TXT de la unidad actual.
;  (Texto sin acentos: sale igual en cualquier página de códigos.)
;  uso: HBLOG
; =============================================================================
        org 100h
        cpu 386

; la tabla de traspaso de la BIOS (bios64.asm)
T_TSC_TICK      equ 36
T_VBE_LFB       equ 184
T_GRAF          equ 192
T_SIN_IRQ       equ 193
T_DIB_N         equ 196
T_DIB_ULT       equ 200
T_DIB_MAX       equ 204
T_N_IRQ         equ 208
T_N_SIN         equ 212
T_N_TECLA       equ 216
T_N_FW          equ 220
T_DIB_TOT       equ 224

inicio:
        cld
        mov ax, 4842h                   ; ¿HispaBIOS?
        xor bx, bx
        stc
        int 15h
        jc .no
        cmp ax, 6862h
        jne .no
        mov [version], bx
        mov ax, 4844h
        stc
        int 15h
        jc .vieja
        cmp ax, 6864h
        jne .vieja
        mov [tab_seg], es
        mov [tab], bx
        mov [reg], si
        mov [reg_n], cx
        push cs
        pop es
        jmp .texto
.no:    mov dx, m_no
        jmp .aviso
.vieja: mov dx, m_vieja
.aviso: mov ah, 9
        int 21h
        mov ax, 4C01h
        int 21h

.texto:
        mov di, buf
        mov si, m_titulo
        call txt
        movzx eax, byte [version + 1]
        call num
        mov al, '.'
        stosb
        movzx eax, byte [version]
        call num
        mov si, m_tics
        call txt
        push ds
        xor ax, ax
        mov ds, ax
        mov eax, [46Ch]
        pop ds
        call num
        mov si, m_irq
        call txt
        mov bx, T_N_IRQ
        call num_tab
        mov si, m_sin
        call txt
        mov bx, T_N_SIN
        call num_tab
        mov si, m_irq_si
        mov bx, T_SIN_IRQ
        call leer_tab
        test al, al
        jz .irq
        mov si, m_irq_no
.irq:   call txt
        mov si, m_fw
        call txt
        mov bx, T_N_FW
        call num_tab
        mov si, m_tecla
        call txt
        mov bx, T_N_TECLA
        call num_tab
        mov si, m_dib
        call txt
        mov bx, T_DIB_N
        call num_tab
        mov si, m_ult
        call txt
        mov bx, T_DIB_ULT
        call num_tab
        mov si, m_max
        call txt
        mov bx, T_DIB_MAX
        call num_tab
        mov si, m_tot
        call txt
        mov bx, T_DIB_TOT
        call num_tab
        mov si, m_modo_t
        mov bx, T_GRAF
        call leer_tab
        test al, al
        jz .modo
        mov si, m_modo_g
.modo:  call txt
        mov si, m_reg
        call txt
        ; el registro de la BIOS, tal cual
        mov cx, [reg_n]
        cmp cx, 1000h
        jbe .cabe
        mov cx, 1000h
.cabe:  mov si, [reg]
        push ds
        mov ds, [tab_seg]
        rep movsb
        pop ds
        mov si, m_fin
        call txt
        sub di, buf
        mov [largo], di

        mov ah, 40h                     ; a la pantalla
        mov bx, 1
        mov cx, [largo]
        mov dx, buf
        int 21h
        mov ah, 3Ch                     ; y al archivo
        xor cx, cx
        mov dx, nombre
        int 21h
        jc .sin_archivo
        mov bx, ax
        mov ah, 40h
        mov cx, [largo]
        mov dx, buf
        int 21h
        pushf
        mov ah, 3Eh
        int 21h
        popf
        jc .sin_archivo
        mov dx, m_guardado
        mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h
.sin_archivo:
        mov dx, m_no_guardado
        mov ah, 9
        int 21h
        mov ax, 4C02h
        int 21h

; txt: DS:SI = texto terminado en 0 -> ES:DI
txt:    lodsb
        test al, al
        jz .f
        stosb
        jmp txt
.f:     ret

; leer_tab: BX = desplazamiento en la tabla -> EAX
leer_tab:
        push es
        mov es, [tab_seg]
        add bx, [tab]
        mov eax, [es:bx]
        pop es
        ret
; num_tab: el número de la tabla en BX, en decimal, a ES:DI
num_tab:
        call leer_tab
; num: EAX en decimal -> ES:DI
num:    push ebx
        push cx
        push edx
        mov ebx, 10
        xor cx, cx
.d:     xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.e:     pop ax
        add al, '0'
        stosb
        loop .e
        pop edx
        pop cx
        pop ebx
        ret

m_no            db 'HBLOG: este equipo no ha arrancado con HispaBIOS (arranque por UEFI).', 13, 10, '$'
m_vieja         db 'HBLOG: esta HispaBIOS es anterior a la 0.3 y no lleva registro.', 13, 10, '$'
m_guardado      db 13, 10, 'Guardado en \HBLOG.TXT', 13, 10, '$'
m_no_guardado   db 13, 10, 'HBLOG: no se ha podido guardar \HBLOG.TXT', 13, 10, '$'
nombre          db '\HBLOG.TXT', 0
m_titulo        db '[HBLOG: registro de la BIOS de HispaBIOS ', 0
m_tics          db ']', 13, 10, 'Reloj: contador de tics de DOS (la hora): ', 0
m_irq           db '; INT 08h recibidas: ', 0
m_sin           db '; tics dados sin IRQ: ', 0
m_irq_si        db ' (ahora la IRQ 0 llega)', 13, 10, 0
m_irq_no        db ' (AHORA LA IRQ 0 NO LLEGA: los tics se dan al llamar a la BIOS)', 13, 10, 0
m_fw            db 'Viajes al firmware: ', 0
m_tecla         db '; preguntas por el teclado: ', 0
m_dib           db 13, 10, 'Pantalla: dibujada ', 0
m_ult           db ' veces; el ultimo dibujo, ', 0
m_max           db ' us; el mas lento, ', 0
m_tot           db ' us; en total, ', 0
m_modo_t        db ' us. Ahora: modo texto', 13, 10, 0
m_modo_g        db ' us. Ahora: modo grafico (VBE)', 13, 10, 0
m_reg           db '--- lo apuntado por HispaBIOS (pantalla y llamadas de VBE) ---', 13, 10, 0
m_fin           db '--- fin ---', 13, 10, 0

version         dw 0
tab_seg         dw 0
tab             dw 0
reg             dw 0
reg_n           dw 0
largo           dw 0
buf:
