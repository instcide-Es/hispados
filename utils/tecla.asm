; =============================================================================
;  Hispa-DOS TECLA.COM - diagnóstico del teclado: muestra el código BRUTO
;  (código de rastreo / scancode) de cada tecla que se pulsa.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Sirve para identificar teclas que mandan códigos no estándar en portátiles
;  raros (p. ej. un «Ctrl izquierdo» de un netbook que no actúa como Ctrl).
;
;  ¿Por qué no vale INT 16h? Las teclas MODIFICADORAS (Ctrl, Alt, Mayús) NO
;  ponen nada en el búfer de la BIOS, así que un programa normal (INT 16h) no
;  las ve. Para ver su código hay que mirar el código de rastreo en crudo, a
;  nivel de la interrupción del teclado (INT 09h). Aquí lo hacemos de la forma
;  probada y segura de KEYB: engancharse al aviso de la BIOS INT 15h AH=4Fh,
;  que la BIOS llama desde INT 09h con el código en AL y SIN que tengamos que
;  leer el puerto 60h (así no se duplican las teclas de dos códigos: flechas,
;  Inicio, Fin...). Encadenamos a la BIOS, de modo que el teclado sigue
;  funcionando y convive con KEYB si está cargado (los dos usan INT 15h 4Fh).
;
;  Por cada código escribe una línea, p. ej.:
;     Codigo: 1E     (make)   Estado: 00
;     Codigo: 9E     (break)  Estado: 00
;     Codigo: 1D     (make)   Estado: 04 [Ctrl]
;     Codigo: E0 4B  (make)   Estado: 00            (flecha izquierda: E0 + 4B)
;  y, si la BIOS lo convierte en carácter, el ASCII.
;
;  El «Estado» de cada línea es el de la BIOS (0040:0017h) JUSTO DESPUÉS de
;  que la BIOS haya tratado ese código, no el del momento de escribir la línea:
;  el aviso 4Fh llega ANTES de que la BIOS cambie el estado, así que también
;  nos enganchamos a INT 09h (encadenando primero a la BIOS) y, a la vuelta,
;  apuntamos el estado junto a los códigos que han entrado en esa interrupción.
;  Antes se leía al escribir la línea y, si el equipo iba lento (o la tecla
;  se soltaba deprisa), un Ctrl salía con «Estado: 00» porque ya estaba suelto.
;
;  Salida robusta (no depende de Esc ni de Ctrl, que pueden ser justo las
;  teclas que fallan): pulse F10 (código 44h) o escriba la palabra FIN.
;  Al salir, DESENGANCHA INT 15h e INT 09h (restaura los vectores anteriores).
; =============================================================================
        cpu 386
        bits 16
        org 0x100

RINGSZ      equ 64                  ; búfer circular de códigos (potencia de 2)
RINGMASK    equ RINGSZ-1

start:
        mov sp, stack_top
        cld
        ; limpiar la zona de datos sin inicializar
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        push cs
        pop ds
        push cs
        pop es

        ; cabecera
        mov si, msg_head
        call puts

        ; guardar el vector de INT 15h y poner el nuestro
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        push cs
        pop es                      ; INT 21h 35 nos cambió ES
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        ; y el de INT 09h (solo para apuntar el estado a la vuelta de la BIOS)
        mov ax, 0x3509
        int 0x21
        mov [old09], bx
        mov [old09+2], es
        push cs
        pop es
        mov dx, int09
        mov ax, 0x2509
        int 0x21

; -----------------------------------------------------------------------------
; bucle principal: vacía el búfer circular de códigos y el búfer de la BIOS
; -----------------------------------------------------------------------------
main:
.scan:
        cli
        mov bx, [ring_tail]
        cmp bx, [ring_head]
        je .noscan
        mov al, [ring+bx]
        mov ah, [ring_st+bx]        ; el estado apuntado a la vuelta de INT 09h
        cmp bx, [ring_samp]         ; ¿sin apuntar? (el aviso no vino de INT 09h)
        jne .conestado
        push ds                     ; entonces, el estado de ahora mismo
        push bx
        mov bx, 0x40
        mov ds, bx
        mov ah, [0x17]
        pop bx
        pop ds
        inc bx
        and bx, RINGMASK
        mov [ring_samp], bx
        mov [ring_tail], bx
        jmp .sacado
.conestado:
        inc bx
        and bx, RINGMASK
        mov [ring_tail], bx
.sacado:
        sti
        mov [cur_st], ah
        call show_scan
        cmp byte [done], 0
        jne .fin
        jmp .scan
.noscan:
        sti
; caracteres que la BIOS sí ha convertido (los vaciamos para que no se queden
; en el búfer ni pasen al intérprete al salir)
.ascii:
        mov ah, 0x01
        int 0x16
        jz .noascii
        xor ah, ah
        int 0x16
        call show_ascii
        jmp .ascii
.noascii:
        sti
        hlt                         ; esperar al siguiente aviso (teclado o reloj)
        jmp main

.fin:
        ; vaciar lo que quede en el búfer de la BIOS (F10, letras de FIN...)
.flush:
        mov ah, 0x01
        int 0x16
        jz .restore
        xor ah, ah
        int 0x16
        jmp .flush
.restore:
        ; DESENGANCHAR: restaurar los vectores de INT 09h e INT 15h anteriores
        cli
        push ds
        lds dx, [old09]
        mov ax, 0x2509
        int 0x21
        pop ds
        push ds
        lds dx, [old15]
        mov ax, 0x2515
        int 0x21
        pop ds
        sti
        mov si, msg_bye
        call puts
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  Nuestro INT 15h: si es el aviso de tecla (AH=4Fh, AL=código), guardamos el
;  código en el búfer circular y SIEMPRE encadenamos al handler anterior (la
;  BIOS, o KEYB), sin tocar el acarreo: así el teclado sigue como siempre.
; =============================================================================
int15:
        cmp ah, 0x4F
        jne .chain
        push bx
        mov bx, [cs:ring_head]
        mov [cs:ring+bx], al        ; el código en crudo (con el bit 7 si es soltar)
        inc bx
        and bx, RINGMASK
        cmp bx, [cs:ring_tail]      ; ¿lleno? entonces se descarta (no se pierde teclado)
        je .full
        mov [cs:ring_head], bx
.full:
        pop bx
.chain:
        jmp far [cs:old15]          ; el iret del anterior restaura el acarreo original

; =============================================================================
;  Nuestro INT 09h: primero la BIOS (que llama al aviso 4Fh de arriba y luego
;  cambia el estado de las modificadoras) y, a la vuelta, se apunta el estado
;  (0040:0017h) junto a cada código que haya entrado y aún no lo tenga. Así
;  «Estado» es el que dejó la BIOS al tratar ESA tecla, tarde lo que tarde el
;  bucle principal en escribir la línea.
; =============================================================================
int09:
        pushf
        call far [cs:old09]
        push ax
        push bx
        push ds
        cli                         ; (la BIOS puede haber vuelto a permitirlas)
        mov ax, 0x40
        mov ds, ax
        mov al, [0x17]
        mov bx, [cs:ring_samp]
.apunta:
        cmp bx, [cs:ring_head]
        je .hecho
        mov [cs:ring_st+bx], al
        inc bx
        and bx, RINGMASK
        jmp .apunta
.hecho:
        mov [cs:ring_samp], bx
        pop ds
        pop bx
        pop ax
        iret                        ; restaura los indicadores del interrumpido

; =============================================================================
;  show_scan: AL = un código en crudo del búfer circular -> una línea
; =============================================================================
show_scan:
        push ax
        push bx
        ; E0h: prefijo de tecla extendida. No es una línea; se recuerda y se
        ; enseña junto al siguiente código como «E0 xx».
        cmp al, 0xE0
        jne .notpfx
        mov byte [e0], 1
        jmp .ret
.notpfx:
        ; E1h: prefijo de la tecla Pausa (dos palabras). Lo marcamos igual.
        cmp al, 0xE1
        jne .line
        mov byte [e1], 1
        jmp .ret
.line:
        push ax
        mov si, msg_cod
        call puts
        cmp byte [e0], 0
        je .noe0
        mov al, 0xE0
        call puthex2
        mov al, ' '
        call putc
        jmp .showcode
.noe0:
        cmp byte [e1], 0
        je .pad
        mov al, 0xE1
        call puthex2
        mov al, ' '
        call putc
        jmp .showcode
.pad:
        mov al, ' '                 ; alinear con los que sí llevan «E0 »
        call putc
        call putc
        call putc
.showcode:
        pop ax
        push ax
        call puthex2                ; el código (con el bit 7 si es soltar)
        ; make / break
        mov si, msg_make
        test al, 0x80
        jz .mk
        mov si, msg_break
.mk:
        call puts
        ; estado de modificadores de la BIOS (0040:0017h)
        call show_state
        call crlf
        pop ax
        ; ---- salida y palabra FIN (solo con «make», no con «soltar») ----
        test al, 0x80
        jnz .clrpfx
        ; las teclas extendidas (E0/E1) no cuentan para F10 ni para FIN
        cmp byte [e0], 0
        jne .clrpfx
        cmp byte [e1], 0
        jne .clrpfx
        cmp al, 0x44                ; F10
        je .exit
        call fin_check
.clrpfx:
        mov byte [e0], 0
        mov byte [e1], 0
.ret:
        pop bx
        pop ax
        ret
.exit:
        mov byte [done], 1
        jmp .clrpfx

; -----------------------------------------------------------------------------
; show_state: imprime «   Estado: XX [Ctrl][Alt][May]» con el estado de la BIOS
; (0040:0017h) apuntado para este código (cur_st)
; -----------------------------------------------------------------------------
show_state:
        push ax
        mov si, msg_est
        call puts
        mov al, [cur_st]
        push ax
        call puthex2
        pop ax
        test al, 0x04               ; Ctrl
        jz .noc
        push ax
        mov si, msg_ctrl
        call puts
        pop ax
.noc:
        test al, 0x08               ; Alt
        jz .noa
        push ax
        mov si, msg_alt
        call puts
        pop ax
.noa:
        test al, 0x03               ; cualquiera de las dos Mayús
        jz .nos
        mov si, msg_may
        call puts
.nos:
        pop ax
        ret

; -----------------------------------------------------------------------------
; fin_check: AL = código «make» normal -> avanza la secuencia F-I-N
;   F=21h  I=17h  N=31h   ; las modificadoras no rompen la secuencia
; -----------------------------------------------------------------------------
fin_check:
        push ax
        push bx
        ; ignorar las propias modificadoras (Mayús, Ctrl, Alt, Bloq Mayús)
        cmp al, 0x2A
        je .ret
        cmp al, 0x36
        je .ret
        cmp al, 0x1D
        je .ret
        cmp al, 0x38
        je .ret
        cmp al, 0x3A
        je .ret
        movzx bx, byte [finpos]
        cmp al, [fin_seq+bx]        ; ¿la que tocaba?
        jne .other
        inc byte [finpos]
        cmp byte [finpos], 3
        jb .ret
        mov byte [done], 1          ; F, I, N completas
        mov byte [finpos], 0
        jmp .ret
.other:
        xor bx, bx                  ; reiniciar; si es «F» empezar de nuevo
        cmp al, [fin_seq]
        jne .set
        inc bx
.set:
        mov [finpos], bl
.ret:
        pop bx
        pop ax
        ret

; -----------------------------------------------------------------------------
; show_ascii: AL = ASCII, AH = código -> «   ASCII: hh 'c'»
; -----------------------------------------------------------------------------
show_ascii:
        push ax
        mov si, msg_asc
        call puts
        pop ax
        push ax
        call puthex2                ; valor ASCII
        pop ax
        cmp al, 0x20                 ; imprimible: enseñarlo entre comillas
        jb .nochar
        cmp al, 0xFF
        je .nochar
        push ax
        mov al, ' '
        call putc
        mov al, 0x27                 ; '
        call putc
        pop ax
        call putc
        mov al, 0x27
        call putc
.nochar:
        call crlf
        ret

; =============================================================================
;  rutinas de salida
; =============================================================================
%include "util.inc"

; AL -> dos dígitos hexadecimales
puthex2:
        push ax
        push ax
        shr al, 4
        call .nib
        pop ax
        and al, 0x0F
        call .nib
        pop ax
        ret
.nib:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        call putc
        ret

; -----------------------------------------------------------------------------
msg_head    db "TECLA - diagn", 0xA2, "stico del teclado de Hispa-DOS", 13, 10
            db "Pulse teclas para ver su c", 0xA2, "digo en crudo (c", 0xA2, "digo de rastreo).", 13, 10
            db "Las modificadoras (Ctrl, Alt, May", 0xA3, "s) NO las ve INT 16h; aqu", 0xA1, " s", 0xA1, ".", 13, 10
            db "Para salir: pulse F10 o escriba la palabra FIN.", 13, 10
            db "Convive con KEYB; si ve algo raro, pru", 0x82, "belo tambi", 0x82, "n sin KEYB.", 13, 10, 13, 10, 0
msg_bye     db "Fin del diagn", 0xA2, "stico. Teclado restaurado.", 13, 10, 0
msg_cod     db "Codigo: ", 0
msg_make    db "  (make) ", 0
msg_break   db "  (break)", 0
msg_est     db "   Estado: ", 0
msg_ctrl    db " [Ctrl]", 0
msg_alt     db " [Alt]", 0
msg_may     db " [May", 0xA3, "s]", 0
msg_asc     db "   ASCII: ", 0

fin_seq     db 0x21, 0x17, 0x31     ; F, I, N

        section .bss
            alignb 4
bss_start:
old15       resd 1
old09       resd 1
ring        resb RINGSZ
ring_st     resb RINGSZ             ; el estado de la BIOS tras cada código
ring_head   resw 1
ring_tail   resw 1
ring_samp   resw 1                  ; primer código aún sin su estado apuntado
cur_st      resb 1                  ; el estado del código que se está escribiendo
e0          resb 1
e1          resb 1
done        resb 1
finpos      resb 1
u_numbuf    resb 16
u_lbuf      resb 84
            resb 256                ; pila
stack_top:
