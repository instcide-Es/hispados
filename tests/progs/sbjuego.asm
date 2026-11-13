; SBJUEGO.COM - [SONIDO-JUEGOS] usa la Sound Blaster directamente, como un
; juego de DOS (tests/run_sbjuego_tests.sh), sin pasar por HSPSND:
;   - lee la variable BLASTER (A220 I5 D1 H5);
;   - reinicia el DSP, enciende el altavoz (D1h), toma el vector de la IRQ y
;     la destapa en el PIC;
;   - 8 bits: 4 bloques de ciclo único (orden 14h, constante de tiempo 40h)
;     por el canal de DMA de 8 bits; la interrupción de cada bloque lanza el
;     siguiente. Tono de unos 440 Hz durante 0,73 s;
;   - 16 bits (DSP 4.x con DMA alta): 8 bloques de ciclo único (41h + B0h) por
;     el canal de 16 bits. Tono de 882 Hz durante 0,73 s;
;   - mira si hay FM (AdLib/OPL en 388h), que es lo que usa la música;
;   - deja el vector y la máscara del PIC como estaban.
; Escribe, además del estado que se ha encontrado (IRQ tapada o no, volúmenes
; del mezclador), una línea por prueba:
;     SBJUEGO: 8 bits: OK, 4 interrupciones
;     SBJUEGO: 16 bits: OK, 8 interrupciones
; y sale con nivel de error 1 si algo falla. Sirve también para probar una
; máquina (VirtualBox...): si aquí suenan los dos tonos, la tarjeta funciona
; con los programas que la usan directamente.
; Argumento: N = sin destapar la IRQ en el PIC (un juego que se fía de que ya
; lo está); M = sin tocar el mezclador (lo normal: tampoco se toca; con M no
; se leen siquiera los volúmenes).
        cpu 386
        bits 16
        org 0x100

BLK     equ 2000                ; muestras por bloque (80 periodos de 25)
NB8     equ 4
NB16    equ 8

start:
        cld
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .argf
        and al, 0xDF
        cmp al, 'N'
        jne .a1
        mov byte [o_nomask], 1
.a1:
        cmp al, 'M'
        jne .arg
        mov byte [o_nomix], 1
        jmp .arg
.argf:
        call lee_blaster
        jnc .hay
        mov si, m_nobl
        call puts
        jmp fallo
.hay:
        mov si, m_bl
        call puts
        mov ax, [base]
        call puthex4
        mov si, m_i
        call puts
        movzx eax, byte [irq]
        call putnum
        mov si, m_d
        call puts
        movzx eax, byte [dma]
        call putnum
        mov si, m_h
        call puts
        movzx eax, byte [hdma]
        call putnum
        call crlf
        ; --- estado al entrar: máscara del PIC ---
        call pic_bit                    ; -> DX puerto, AH bit
        in al, dx
        mov [pic_old], al
        mov si, m_tap
        test al, ah
        jnz .pt
        mov si, m_dest
.pt:
        push si
        mov si, m_pic
        call puts
        pop si
        call puts
        call crlf
        ; --- DSP ---
        call dsp_reset
        jnc .dsp
        mov si, m_nodsp
        call puts
        jmp fallo
.dsp:
        mov al, 0xE1
        call dsp_out
        call dsp_in
        mov [dsp_maj], al
        call dsp_in
        mov [dsp_min], al
        mov si, m_dsp
        call puts
        movzx eax, byte [dsp_maj]
        call putnum
        mov al, '.'
        call putc
        mov al, [dsp_min]
        aam
        add ax, 0x3030
        xchg al, ah
        call putc
        mov al, ah
        call putc
        call crlf
        ; --- mezclador (SB16): maestro y voz ---
        cmp byte [dsp_maj], 4
        jb .nomix
        cmp byte [o_nomix], 0
        jne .nomix
        mov si, m_mix
        call puts
        mov bx, mixregs
.mx:
        mov al, [bx]
        test al, al
        jz .mxf
        mov dx, [base]
        add dx, 4
        out dx, al
        inc dx
        in al, dx
        call puthex2
        mov al, ' '
        call putc
        inc bx
        jmp .mx
.mxf:
        call crlf
.nomix:
        ; --- FM ---
        call fm_detecta
        mov si, m_fmsi
        jnc .fm
        mov si, m_fmno
.fm:
        call puts
        ; --- bloque de sonido: dentro de una página de 64 KB ---
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, buf
        mov ebx, eax
        and ebx, 0xFFFF
        add ebx, 2 * BLK
        cmp ebx, 0x10000
        jbe .cabe
        add eax, 2 * BLK                ; la segunda mitad sí cabe
.cabe:
        mov [fis], eax
        ; --- vector e IRQ ---
        mov al, [irq]
        add al, 8
        cmp byte [irq], 8
        jb .v
        add al, 0x70 - 16
.v:
        mov [vec], al
        mov ah, 0x35
        int 0x21
        mov [old], bx
        mov [old+2], es
        push cs
        pop es
        mov al, [vec]
        mov dx, isr
        mov ah, 0x25
        int 0x21
        cmp byte [o_nomask], 0
        jne .nm
        cli
        call pic_bit
        in al, dx
        not ah
        and al, ah
        out dx, al
        cmp byte [irq], 8
        jb .nc
        in al, 0x21
        and al, 0xFB
        out 0x21, al
.nc:
        sti
.nm:
        mov al, 0xD1                    ; altavoz encendido
        call dsp_out

        ; ================= 8 bits =================
        ; onda cuadrada sin signo, periodo de 25 muestras
        mov di, [fis]
        mov ax, cs
        shl ax, 4
        sub di, ax                      ; desplazamiento en CS
        mov cx, BLK
        xor bx, bx
.g8:
        mov al, 0x80 + 0x50
        cmp bl, 12
        jb .g8a
        mov al, 0x80 - 0x50
.g8a:
        stosb
        inc bl
        cmp bl, 25
        jb .g8b
        xor bl, bl
.g8b:
        loop .g8
        mov byte [bits16], 0
        mov word [nblk], NB8
        mov word [nirq], 0
        mov al, 0x40                    ; constante de tiempo: 10989 Hz
        call dsp_out
        mov al, 165
        call dsp_out
        cli
        call bloque
        sti
        call espera
        mov si, m_8
        call resultado

        ; ================= 16 bits =================
        cmp byte [dsp_maj], 4
        jb .fin
        cmp byte [hdma], 5
        jb .fin
        cmp byte [hdma], 7
        ja .fin
        call dsp_reset
        mov al, 0xD1
        call dsp_out
        mov di, [fis]
        mov ax, cs
        shl ax, 4
        sub di, ax
        mov cx, BLK
        xor bx, bx
.g16:
        mov ax, 12000
        cmp bl, 12
        jb .g16a
        mov ax, -12000
.g16a:
        stosw
        inc bl
        cmp bl, 25
        jb .g16b
        xor bl, bl
.g16b:
        loop .g16
        mov byte [bits16], 1
        mov word [nblk], NB16
        mov word [nirq], 0
        cli
        call bloque
        sti
        call espera
        mov si, m_16
        call resultado
.fin:
        call restaura
        mov al, [mal]
        mov ah, 0x4C
        int 0x21

fallo:
        mov ax, 0x4C01
        int 0x21

; «SBJUEGO: N bits: OK|FALLO, n interrupciones»
resultado:
        call puts
        mov ax, [nirq]
        cmp ax, [nblk]
        mov si, m_ok
        je .r
        mov si, m_fallo
        mov byte [mal], 1
.r:
        call puts
        movzx eax, word [nirq]
        call putnum
        mov si, m_int
        call puts
        ret

; esperar a que lleguen todas las interrupciones (3 s como mucho)
espera:
        push es
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.l:
        mov ax, [nirq]
        cmp ax, [nblk]
        jae .r
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 55
        jb .l
.r:
        ; que acabe de sonar lo último antes de seguir
        mov bx, [es:0x46C]
.w:
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 4
        jb .w
        pop es
        ret

; vector y máscara del PIC como estaban; altavoz apagado
restaura:
        mov al, 0xD3
        call dsp_out
        call dsp_reset
        cli
        call pic_bit
        in al, dx
        not ah
        and al, ah
        not ah
        and ah, [pic_old]
        or al, ah
        out dx, al
        sti
        push ds
        lds dx, [old]
        mov al, [cs:vec]
        mov ah, 0x25
        int 0x21
        pop ds
        ret

; puerto de máscara (DX) y bit (AH) de la IRQ
pic_bit:
        push cx
        mov cl, [irq]
        mov dx, 0x21
        cmp cl, 8
        jb .m
        mov dx, 0xA1
        sub cl, 8
.m:
        mov ah, 1
        shl ah, cl
        pop cx
        ret

; lanzar un bloque: DMA de ciclo único + orden del DSP (DS = CS)
bloque:
        push eax
        push bx
        push dx
        cmp byte [bits16], 0
        jne .b16
        mov bl, [dma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0x0A, al                    ; tapar el canal
        out 0x0C, al                    ; biestable
        mov al, 0x48                    ; ciclo único, memoria -> tarjeta
        or al, bl
        out 0x0B, al
        movzx dx, bl
        shl dx, 1
        mov eax, [fis]
        out dx, al
        mov al, ah
        out dx, al
        inc dx
        mov ax, BLK - 1
        out dx, al
        mov al, ah
        out dx, al
        movzx bx, bl
        mov dl, [pg8+bx]
        xor dh, dh
        mov eax, [fis]
        shr eax, 16
        out dx, al
        mov al, bl
        out 0x0A, al                    ; destapar
        mov al, 0x14                    ; DAC de 8 bits, ciclo único
        call dsp_out
        mov al, (BLK - 1) & 0xFF
        call dsp_out
        mov al, (BLK - 1) >> 8
        call dsp_out
        jmp .r
.b16:
        mov bl, [hdma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0xD4, al
        out 0xD8, al
        mov al, 0x48
        or al, bl
        out 0xD6, al
        movzx dx, bl
        shl dx, 2
        add dx, 0xC0
        mov eax, [fis]
        shr eax, 1                      ; en palabras
        out dx, al
        mov al, ah
        out dx, al
        add dx, 2
        mov ax, BLK - 1
        out dx, al
        mov al, ah
        out dx, al
        movzx bx, bl
        mov dl, [pg16+bx]
        xor dh, dh
        mov eax, [fis]
        shr eax, 16
        out dx, al
        mov al, bl
        out 0xD4, al
        mov al, 0x41                    ; frecuencia de salida: 22050 Hz
        call dsp_out
        mov al, 22050 >> 8
        call dsp_out
        mov al, 22050 & 0xFF
        call dsp_out
        mov al, 0xB0                    ; 16 bits, salida, ciclo único
        call dsp_out
        mov al, 0x10                    ; mono con signo
        call dsp_out
        mov al, (BLK - 1) & 0xFF
        call dsp_out
        mov al, (BLK - 1) >> 8
        call dsp_out
.r:
        pop dx
        pop bx
        pop eax
        ret
pg8     db 0x87, 0x83, 0x81, 0x82
pg16    db 0x8F, 0x8B, 0x89, 0x8A

; interrupción de la tarjeta: acuse y bloque siguiente
isr:
        push ax
        push dx
        push ds
        push cs
        pop ds
        mov dx, [base]
        add dx, 0x0E
        cmp byte [bits16], 0
        je .a
        inc dx
.a:
        in al, dx
        inc word [nirq]
        mov ax, [nirq]
        cmp ax, [nblk]
        jae .eoi
        call bloque
.eoi:
        mov al, 0x20
        cmp byte [irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop ds
        pop dx
        pop ax
        iret

; ---- DSP ----
dsp_out:
        push cx
        push dx
        push ax
        mov dx, [base]
        add dx, 0x0C
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .ok
        loop .w
.ok:
        pop ax
        out dx, al
        pop dx
        pop cx
        ret
dsp_in:
        push cx
        push dx
        mov dx, [base]
        add dx, 0x0E
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .ok
        loop .w
        stc
        jmp .r
.ok:
        sub dx, 4
        in al, dx
        clc
.r:
        pop dx
        pop cx
        ret
dsp_reset:
        push ax
        push cx
        push dx
        mov dx, [base]
        add dx, 6
        mov al, 1
        out dx, al
        mov cx, 64
.d:
        in al, dx
        loop .d
        xor al, al
        out dx, al
        mov cx, 8
.t:
        call dsp_in
        jc .n
        cmp al, 0xAA
        je .ok
.n:
        loop .t
        stc
        jmp .r
.ok:
        clc
.r:
        pop dx
        pop cx
        pop ax
        ret

; ¿hay un OPL en 388h? (la detección de siempre, con sus dos temporizadores)
; -> CF si no
fm_detecta:
        mov ax, 0x0460
        call fm_out
        mov ax, 0x0480
        call fm_out
        mov dx, 0x388
        in al, dx
        mov bl, al
        mov ax, 0x02FF
        call fm_out
        mov ax, 0x0421
        call fm_out
        ; más de 80 microsegundos: dos tics del reloj
        push es
        xor ax, ax
        mov es, ax
        mov cx, [es:0x46C]
.w:
        mov ax, [es:0x46C]
        sub ax, cx
        cmp ax, 2
        jb .w
        pop es
        mov dx, 0x388
        in al, dx
        mov bh, al
        mov ax, 0x0460
        call fm_out
        mov ax, 0x0480
        call fm_out
        and bx, 0xE0E0
        cmp bx, 0xC000
        je .si
        stc
        ret
.si:
        clc
        ret
; AH = registro, AL = valor
fm_out:
        push cx
        push dx
        push ax
        mov dx, 0x388
        mov al, ah
        out dx, al
        mov cx, 6
.a:
        in al, dx
        loop .a
        inc dx
        pop ax
        out dx, al
        dec dx
        mov cx, 36
.b:
        in al, dx
        loop .b
        pop dx
        pop cx
        ret

; BLASTER=A220 I5 D1 H5 del entorno -> base, irq, dma, hdma (CF si no está)
lee_blaster:
        push es
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .no
        mov si, s_bl
        push di
.c:
        lodsb
        test al, al
        jz .si
        cmp al, [es:di]
        jne .nx
        inc di
        jmp .c
.nx:
        pop di
.sk:
        cmp byte [es:di], 0
        je .n2
        inc di
        jmp .sk
.n2:
        inc di
        jmp .v
.si:
        pop ax
.t:
        mov al, [es:di]
        test al, al
        jz .ok
        inc di
        and al, 0xDF
        cmp al, 'A'
        je .a
        cmp al, 'I'
        je .i
        cmp al, 'D'
        je .d
        cmp al, 'H'
        je .h
        jmp .t
.a:
        xor dx, dx
.al:
        mov al, [es:di]
        sub al, '0'
        jb .af
        cmp al, 9
        ja .af
        shl dx, 4
        or dl, al
        inc di
        jmp .al
.af:
        mov [base], dx
        jmp .t
.i:
        call .dec
        mov [irq], dl
        jmp .t
.d:
        call .dec
        mov [dma], dl
        jmp .t
.h:
        call .dec
        mov [hdma], dl
        jmp .t
.ok:
        pop es
        cmp word [base], 0
        je .no2
        clc
        ret
.no:
        pop es
.no2:
        stc
        ret
.dec:
        xor dx, dx
.dl:
        mov al, [es:di]
        sub al, '0'
        jb .dr
        cmp al, 9
        ja .dr
        imul dx, dx, 10
        add dl, al
        inc di
        jmp .dl
.dr:
        ret

; AL en hexadecimal (dos cifras)
puthex2:
        push ax
        push ax
        shr al, 4
        call .n
        pop ax
        call .n
        pop ax
        ret
.n:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        jmp putc

%include "util.inc"

s_bl    db "BLASTER=", 0
m_nobl  db "SBJUEGO: no hay variable BLASTER", 13, 10, 0
m_bl    db "SBJUEGO: BLASTER: puerto ", 0
m_i     db "h, IRQ ", 0
m_d     db ", DMA ", 0
m_h     db ", DMA alta ", 0
m_pic   db "SBJUEGO: al entrar, IRQ ", 0
m_tap   db "tapada en el PIC", 0
m_dest  db "destapada en el PIC", 0
m_nodsp db "SBJUEGO: el DSP no contesta", 13, 10, 0
m_dsp   db "SBJUEGO: DSP ", 0
m_mix   db "SBJUEGO: mezclador 30h 31h 32h 33h 22h 04h = ", 0
m_fmsi  db "SBJUEGO: FM (OPL en 388h): hay", 13, 10, 0
m_fmno  db "SBJUEGO: FM (OPL en 388h): no hay", 13, 10, 0
m_8     db "SBJUEGO: 8 bits: ", 0
m_16    db "SBJUEGO: 16 bits: ", 0
m_ok    db "OK, ", 0
m_fallo db "FALLO, ", 0
m_int   db " interrupciones", 13, 10, 0
mixregs db 0x30, 0x31, 0x32, 0x33, 0x22, 0x04, 0

base    dw 0
irq     db 5
dma     db 1
hdma    db 0
dsp_maj db 0
dsp_min db 0
vec     db 0
pic_old db 0
bits16  db 0
mal     db 0
o_nomask db 0
o_nomix db 0
nblk    dw 0
nirq    dw 0
old     dd 0
fis     dd 0
u_lbuf      times 84 db 0
u_numbuf    times 16 db 0
        align 2
buf:                            ; 2 x (2 * BLK) octetos, sin inicializar
