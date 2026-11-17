; =============================================================================
;  XMS.COM - prueba del gestor de memoria extendida, de A20 y de la HMA
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Pregunta por el gestor (INT 2Fh), y con él: mira la versión; abre A20 y
;  comprueba DE VERDAD que está abierta (cambia 0000:0000 y mira que
;  FFFF:0010 no cambia con él) y, si el núcleo no está en la HMA, que al
;  cerrarla vuelve a ser el mismo octeto; pide 256 K, los llena con un dibujo
;  distinto en cada trozo de 16 K, los lee y los compara; agranda el bloque a
;  384 K (se tiene que mudar o crecer sin perder lo que tenía), lo bloquea y
;  mira su dirección; pide todo lo que queda, comprueba que ya no hay más, lo
;  suelta todo y mira que queda lo mismo que al empezar; y pide la HMA.
;  Cada paso que falla dice «XMS: MAL» y su número.
; =============================================================================
        cpu 8086
        org 0x100
TROZO       equ 16384

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro
%macro XMS 1
        mov ah, %1
        call far [xms]
%endmacro
%macro DICE 1
        mov dx, %1
        DOS 0x09
%endmacro
%macro SI_NO 2                      ; si no se cumple (salto %1), mal con el número %2
        %1 %%bien
        mov al, %2
        jmp mal
%%bien:
%endmacro

inicio:
        cld
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        je .hay
        DICE m_no_hay
        mov ax, 0x4C00
        int 0x21
.hay:
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        mov ax, 0xE806              ; ¿está el núcleo en la HMA?
        int 0x21
        and al, 2
        mov [alto], al
        XMS 0x00
        cmp ax, 0x0200
        SI_NO je, '1'
        cmp dx, 1
        SI_NO je, '1'
        XMS 0x08                    ; lo que hay al empezar
        mov [libre0], dx
        mov ax, dx
        mov di, m_libres_n + 4
        call num
        DICE m_libres
        ; ---- A20 ----
        XMS 0x05
        cmp ax, 1
        SI_NO je, '2'
        XMS 0x07
        cmp ax, 1
        SI_NO je, '2'
        call a20_abierta
        SI_NO jnz, '2'
        XMS 0x06
        cmp byte [alto], 0
        jne .siempre
        XMS 0x07
        test ax, ax
        SI_NO jz, '3'
        call a20_abierta
        SI_NO jz, '3'
        DICE m_a20
        jmp short .mem
.siempre:
        call a20_abierta            ; con el núcleo arriba no se cierra nunca
        SI_NO jnz, '3'
        DICE m_a20_alto
.mem:
        ; ---- 256 K: llenar, leer y comparar ----
        mov dx, 256
        XMS 0x09
        cmp ax, 1
        SI_NO je, '4'
        mov [asa], dx
        mov word [tr], 0
.llena:
        call dibuja
        mov word [m_org], 0         ; de aquí (convencional)...
        mov word [m_org_off], buf
        mov [m_org_off+2], cs
        mov ax, [asa]               ; ...al bloque, en su trozo
        mov [m_dst], ax
        call pon_dst
        mov si, mueve
        XMS 0x0B
        cmp ax, 1
        SI_NO je, '5'
        inc word [tr]
        cmp word [tr], 16
        jb .llena
        mov word [tr], 0
.lee:
        call comprueba_trozo
        SI_NO je, '6'
        inc word [tr]
        cmp word [tr], 16
        jb .lee
        DICE m_movidos
        ; ---- agrandar a 384 K, sin perder nada; bloquear ----
        mov dx, 100                 ; (algo detrás, para que tenga que mudarse)
        XMS 0x09
        mov [asa2], dx
        mov dx, [asa]
        mov bx, 384
        XMS 0x0F
        cmp ax, 1
        SI_NO je, '7'
        mov word [tr], 0
        call comprueba_trozo
        SI_NO je, '7'
        mov word [tr], 15
        call comprueba_trozo
        SI_NO je, '7'
        mov dx, [asa]
        XMS 0x0E
        cmp dx, 384
        SI_NO je, '8'
        mov dx, [asa]
        XMS 0x0C
        cmp dx, 0x0011              ; (por encima de la HMA: 110000h o más)
        SI_NO jae, '8'
        mov dx, [asa]
        XMS 0x0A                    ; bloqueado no se puede soltar
        test ax, ax
        SI_NO jz, '8'
        mov dx, [asa]
        XMS 0x0D
        DICE m_crece
        ; ---- llenar y soltar ----
        XMS 0x08
        mov dx, ax                  ; el mayor trozo que queda
        XMS 0x09
        cmp ax, 1
        SI_NO je, '9'
        mov [asa3], dx
        mov dx, 1024                ; ya no cabe un mega más
        XMS 0x09
        test ax, ax
        SI_NO jz, '9'
        cmp bl, 0xA0
        SI_NO je, '9'
        mov dx, [asa3]
        XMS 0x0A
        mov dx, [asa2]
        XMS 0x0A
        mov dx, [asa]
        XMS 0x0A
        cmp ax, 1
        SI_NO je, 'A'
        mov dx, [asa]               ; soltado dos veces: error
        XMS 0x0A
        test ax, ax
        SI_NO jz, 'A'
        XMS 0x08
        cmp dx, [libre0]
        SI_NO je, 'A'
        cmp ax, dx
        SI_NO je, 'A'
        DICE m_soltar
        ; ---- la HMA ----
        mov dx, 0xFFFF
        XMS 0x01
        cmp byte [alto], 0
        jne .hma_alto
        cmp ax, 1
        SI_NO je, 'B'
        XMS 0x01                    ; (dos veces no)
        test ax, ax
        SI_NO jz, 'B'
        XMS 0x02
        cmp ax, 1
        SI_NO je, 'B'
        DICE m_hma
        jmp short .fin
.hma_alto:
        test ax, ax
        SI_NO jz, 'B'
        cmp bl, 0x91
        SI_NO je, 'B'
        DICE m_hma_alto
.fin:
        mov ax, 0x4C00
        int 0x21
mal:
        mov [m_mal_n], al
        DICE m_mal
        mov ax, 0x4C01
        int 0x21

; ¿está abierta A20 de verdad? ZF=0 si lo está
a20_abierta:
        push ds
        push es
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        cli
        mov bx, [0]
        not word [0]
        mov ax, [0]
        xor ax, [es:0x10]           ; iguales: es el mismo octeto (cerrada)
        mov [0], bx
        sti
        pop es
        pop ds
        test ax, ax
        ret
; llena buf con el dibujo del trozo [tr]
dibuja:
        mov di, buf
        mov cx, TROZO / 2
        mov ax, [tr]
        mov bx, 0x9E37
        mul bx
        add ax, 0x1234
.c:
        stosw
        add ax, 0x0101
        rol ax, 1
        loop .c
        ret
; el destino (o el origen) dentro del bloque: el trozo [tr]
pon_dst:
        mov ax, [tr]                ; x 16.384
        mov dx, ax
        mov cl, 14
        shl ax, cl
        mov cl, 2
        shr dx, cl
        mov [m_dst_off], ax
        mov [m_dst_off+2], dx
        ret
; lee el trozo [tr] del bloque y lo compara con su dibujo. ZF=1 si es igual
comprueba_trozo:
        mov ax, [asa]               ; del bloque a buf2
        mov [m_org], ax
        call pon_dst
        mov ax, [m_dst_off]
        mov [m_org_off], ax
        mov ax, [m_dst_off+2]
        mov [m_org_off+2], ax
        mov word [m_dst], 0
        mov word [m_dst_off], buf2
        mov [m_dst_off+2], cs
        mov si, mueve
        XMS 0x0B
        cmp ax, 1
        jne .x
        call dibuja
        mov si, buf
        mov di, buf2
        mov cx, TROZO / 2
        repe cmpsw
.x:
        ret
; AX en decimal, cinco cifras acabando en DI
num:
        mov bx, 10
        mov cx, 5
.c:
        xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .c
        ret

m_no_hay    db "XMS: no hay gestor de memoria extendida", 13, 10, "$"
m_libres    db "XMS: 2.0, libres "
m_libres_n  db "00000 K", 13, 10, "$"
m_a20       db "A20: se abre y se cierra de verdad", 13, 10, "$"
m_a20_alto  db "A20: abierta siempre (el nucleo esta arriba)", 13, 10, "$"
m_movidos   db "XMS: 256 K movidos y comparados", 13, 10, "$"
m_crece     db "XMS: crecer a 384 K, bloquear y soltar", 13, 10, "$"
m_soltar    db "XMS: llenar y soltar", 13, 10, "$"
m_hma       db "HMA: pedida y soltada", 13, 10, "$"
m_hma_alto  db "HMA: la tiene el sistema", 13, 10, "$"
m_mal       db "XMS: MAL "
m_mal_n     db "0", 13, 10, "$"
alto        db 0
; lo que pide «mover»
mueve:
            dd TROZO
m_org       dw 0
m_org_off   dd 0
m_dst       dw 0
m_dst_off   dd 0

        absolute $
            alignb 2
xms         resw 2
asa         resw 1
asa2        resw 1
asa3        resw 1
libre0      resw 1
tr          resw 1
buf         resb TROZO
buf2        resb TROZO
