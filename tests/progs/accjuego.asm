; ACCJUEGO.COM - «juego» de prueba de los accesos directos con opciones de
; juego de Hispa Ventanas (tests/run_accesos_tests.sh). Escribe en la
; pantalla una línea
;   JUEGO PROG=ruta PSP=hhhh ARGS=[...] VEL=n FRENO=SI|NO RES=SI|NO BLASTER=SI|NO
;     PROG     su propia ruta (la del entorno)
;     PSP      su segmento (con LOADFIX, 1000h o más)
;     VEL      vueltas (de 100) de un bucle en 8 tics del reloj de la BIOS
;     FRENO    si hay un VELOCIDAD en la memoria frenando (se busca su firma
;              en la cadena de bloques, como hace VELOCIDAD, y se mira su
;              estado: freno del temporizador o cachés desactivadas)
;     RES      si hay un VELOCIDAD en la memoria, frene o no
;     BLASTER  si la variable BLASTER está en el entorno
; Con parámetros:
;   /C   después espera una tecla con la función 08h de DOS: se puede acabar
;        con Ctrl+C (escribe antes «JUEGO: espera»)
;   /T   toma la INT 08h (pasándola a la anterior) y se queda residente, como
;        un juego que deja un residente en la memoria
;   /E   termina con el código 7
        cpu 386
        bits 16
        org 0x100

BASURA  equ 0x8000

start:
        jmp near inicio

; ---- lo que se queda con /T ----
old08   dd 0
isr08:
        jmp far [cs:old08]
res_fin:

inicio:
        cld
        mov si, 0x81
.a:
        lodsb
        cmp al, 13
        je .ae
        cmp al, '/'
        jne .a
        lodsb
        and al, 0xDF
        cmp al, 'C'
        jne .a1
        mov byte [op_c], 1
.a1:
        cmp al, 'T'
        jne .a2
        mov byte [op_t], 1
.a2:
        cmp al, 'E'
        jne .a3
        mov byte [op_e], 1
.a3:
        cmp al, 13
        jne .a
.ae:
        mov si, s_juego
        call pon
        ; la ruta propia y BLASTER, del entorno
        mov es, [0x2C]
        xor di, di
.e1:
        cmp byte [es:di], 0
        je .e3
        push di
        mov si, s_blaster
        mov cx, 8
        repe cmpsb
        pop di
        jne .e2
        mov byte [hay_bl], 1
.e2:
        xor al, al
        mov cx, 0xFFFF
        repne scasb
        jmp .e1
.e3:
        add di, 3
.e4:
        mov dl, [es:di]
        test dl, dl
        jz .e5
        mov ah, 0x02
        int 0x21
        inc di
        jmp .e4
.e5:
        push cs
        pop es
        mov si, s_psp
        call pon
        mov ax, cs
        call hex4
        mov si, s_args
        call pon
        movzx cx, byte [0x80]
        mov si, 0x81
        jcxz .g2
.g1:
        lodsb
        mov dl, al
        mov ah, 0x02
        int 0x21
        loop .g1
.g2:
        mov si, s_vel
        call pon
        call medir
        mov eax, ebx
        call dec32
        call buscar_res
        mov si, s_freno
        call pon
        mov al, [frena]
        call si_no
        mov si, s_res
        call pon
        mov al, [hay_res]
        call si_no
        mov si, s_bl
        call pon
        mov al, [hay_bl]
        call si_no
        mov si, s_nl
        call pon
        cmp byte [op_c], 0
        je .sinc
        mov si, s_espera
        call pon
        mov ah, 0x08                ; (con Ctrl+C, DOS acaba aquí el programa)
        int 0x21
        mov si, s_tecla
        call pon
.sinc:
        cmp byte [op_t], 0
        je .sint
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov dx, isr08
        mov ax, 0x2508
        int 0x21
        mov si, s_tsr
        call pon
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, (res_fin - $$ + 0x100 + 15) / 16
        mov ax, 0x3100
        int 0x21
.sint:
        mov ax, 0x4C00
        cmp byte [op_e], 0
        je .fin
        mov al, 7
.fin:
        int 0x21

; EBX = vueltas (de 100) en 8 tics
medir:
        push es
        xor ax, ax
        mov es, ax
        sti
        mov ax, [es:0x46C]
.w:
        cmp ax, [es:0x46C]
        je .w
        mov dx, [es:0x46C]
        xor ebx, ebx
.o:
        mov cx, 100
.i:
        dec cx
        jnz .i
        inc ebx
        mov ax, [es:0x46C]
        sub ax, dx
        cmp ax, 8
        jb .o
        pop es
        ret

; ¿hay un VELOCIDAD en la memoria? ¿frena?
buscar_res:
        push es
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
.l:
        mov es, ax
        inc ax
        cmp [es:1], ax
        jne .nx
        cmp dword [es:0x113], 'HSPV'
        jne .nx
        cmp dword [es:0x117], 'EL1'
        jne .nx
        mov byte [hay_res], 1
        mov dl, [es:0x11B]          ; r_activo
        or dl, [es:0x13B]           ; r_cd
        jz .nx
        mov byte [frena], 1
.nx:
        cmp byte [es:0], 'M'
        jne .x
        add ax, [es:3]
        jmp .l
.x:
        pop es
        ret

si_no:
        mov si, s_si
        test al, al
        jnz pon
        mov si, s_no
pon:
        lodsb
        test al, al
        jz .x
        mov dl, al
        mov ah, 0x02
        int 0x21
        jmp pon
.x:
        ret

hex4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 0x02
        int 0x21
        pop ax
        loop .h
        ret

dec32:
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop dx
        add dl, '0'
        mov ah, 0x02
        int 0x21
        loop .p
        ret

s_juego     db "JUEGO PROG=", 0
s_psp       db " PSP=", 0
s_args      db " ARGS=[", 0
s_vel       db "] VEL=", 0
s_freno     db " FRENO=", 0
s_res       db " RES=", 0
s_bl        db " BLASTER=", 0
s_si        db "SI", 0
s_no        db "NO", 0
s_nl        db 13, 10, 0
s_espera    db "JUEGO: espera", 13, 10, 0
s_tecla     db "JUEGO: tecla", 13, 10, 0
s_tsr       db "JUEGO: residente", 13, 10, 0
s_blaster   db "BLASTER="
op_c        db 0
op_t        db 0
op_e        db 0
hay_bl      db 0
hay_res     db 0
frena       db 0
