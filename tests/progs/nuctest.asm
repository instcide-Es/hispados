; NUCTEST.COM - pruebas del núcleo (revisión): memoria, EXEC, país, PSP...
; Cada prueba escribe "NUC <nombre> OK" o "NUC <nombre> FALLO".
; Kernel self-tests: memory, EXEC, country info, PSP...
        cpu 386
        bits 16
        org 0x100

%macro PRUEBA 1
        mov word [nombre], %%n
        jmp %%s
%%n:    db %1, "$"
%%s:
%endmacro

inicio:
        ; dejar memoria libre: el bloque del programa, solo lo necesario
        mov sp, pila_fin
        mov bx, fin + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov [mipsp], cs

; ---- 1Fh no debe tocar DL ----
        PRUEBA "1F-DL"
        push ds
        mov dl, 0x77
        mov ah, 0x1F
        int 0x21
        pop ds
        cmp dl, 0x77
        call resultado

; ---- 3001h: BH = indicador de versión (no el OEM) ----
        PRUEBA "3001"
        mov ax, 0x3001
        int 0x21
        test bh, bh
        call resultado

; ---- 38h con DX=FFFFh (fijar país) no debe escribir en DS:FFFFh ----
        PRUEBA "38-FIJAR"
        mov si, 0
        mov di, copia
        mov cx, 40
        rep movsb
        mov ax, 0x3822              ; España (34)
        mov dx, 0xFFFF
        int 0x21
        jc .f38
        mov si, 0
        mov di, copia
        mov cx, 40
        repe cmpsb
.f38:
        call resultado_cf
        PRUEBA "38-FIJAR-OTRO"
        mov ax, 0x3801              ; un país que no está
        mov dx, 0xFFFF
        int 0x21
        jnc .f38b
        cmp ax, 2
        jne .f38b
        mov si, 0
        mov di, copia
        mov cx, 40
        repe cmpsb
        jmp .f38c
.f38b:
        or al, 1                    ; ZF=0
.f38c:
        call resultado

; ---- 38h: reloj de 24 h y rutina de mayúsculas llamable ----
        PRUEBA "38-24H"
        mov ax, 0x3800
        mov dx, pais
        int 0x21
        cmp bx, 34
        jne .f24
        cmp byte [pais+0x11], 1
.f24:
        call resultado
        PRUEBA "38-MAYUS"
        mov ax, [pais+0x12]
        or ax, [pais+0x14]
        jz .fmay                    ; puntero nulo: llamarlo colgaría
        mov al, 0xA4                ; ñ -> Ñ
        call far [pais+0x12]
        cmp al, 0xA5
        jne .fmay2
        mov al, 0x81                ; ü -> Ü
        mov bx, 0x1234
        call far [pais+0x12]
        cmp bx, 0x1234
        jne .fmay2
        cmp al, 0x9A
        jmp .fmay2
.fmay:
        or al, 1
.fmay2:
        call resultado

; ---- 6501h con un búfer de 7 octetos: no debe escribir más ----
        PRUEBA "6501-CORTO"
        mov di, corto
        mov cx, 7
        mov bx, 0xFFFF
        mov dx, 0xFFFF
        mov ax, 0x6501
        int 0x21
        jc .f65
        cmp cx, 7
        jne .f65
        cmp byte [corto], 1
        jne .f65
        cmp word [corto+3], 34
        jne .f65
        cmp word [corto+5], 437
        jne .f65
        mov di, corto + 7
        mov cx, 40
        mov al, 0x5A
        repe scasb                  ; la guarda, intacta
        jmp .f65b
.f65:
        or al, 1
.f65b:
        call resultado
        PRUEBA "6501-ENTERO"
        mov di, pais
        mov cx, 41
        mov bx, 0xFFFF
        mov dx, 0xFFFF
        mov ax, 0x6501
        int 0x21
        jc .f65c
        cmp cx, 41
        jne .f65c
        cmp word [pais+1], 38
        jne .f65c
        cmp byte [pais+7+0x11], 1   ; 24 h
        jmp .f65d
.f65c:
        or al, 1
.f65d:
        call resultado

; ---- 4Ah que no cabe: el bloque queda con el máximo (como MS-DOS) ----
        PRUEBA "4A-MAXIMO"
        mov bx, 0x10
        mov ah, 0x48
        int 0x21
        jc .f4a
        mov [blk], ax
        mov es, ax
        mov bx, 0xFFFF
        mov ah, 0x4A
        int 0x21
        jnc .f4a
        cmp ax, 8
        jne .f4a
        mov ax, [blk]
        dec ax
        mov es, ax
        cmp [es:3], bx              ; BX = máximo; el bloque ya lo ocupa
        pushf
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        popf
        jmp .f4ab
.f4a:
        or al, 1
.f4ab:
        push cs
        pop es
        call resultado

; ---- 4Ah: el bloque pasa a ser del proceso actual ----
        PRUEBA "4A-DUENO"
        mov bx, 0x20
        mov ah, 0x48
        int 0x21
        mov [blk], ax               ; hará de PSP ficticio
        mov bx, ax
        mov ah, 0x50
        int 0x21
        mov bx, 0x10
        mov ah, 0x48
        int 0x21                    ; bloque de otro dueño
        mov [blk2], ax
        mov bx, [mipsp]
        mov ah, 0x50
        int 0x21
        mov es, [blk2]
        mov bx, 0x08
        mov ah, 0x4A
        int 0x21
        mov ax, [blk2]
        dec ax
        mov es, ax
        mov ax, [es:1]
        cmp ax, [mipsp]
        pushf
        mov es, [blk2]
        mov ah, 0x49
        int 0x21
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        popf
        push cs
        pop es
        call resultado

; ---- 4Bh con subfunción que no existe: error 1 ----
        PRUEBA "4B-SUBF"
        mov dx, hijo
        mov bx, pb
        mov ax, 0x4B07
        int 0x21
        jnc .f4b
        cmp ax, 1
        jmp .f4b2
.f4b:
        or al, 1
.f4b2:
        call resultado

; ---- entorno vacío: dos ceros, como lo deja MS-DOS ----
        PRUEBA "ENTORNO-VACIO"
        mov bx, 1
        mov ah, 0x48
        int 0x21
        mov [blk], ax
        mov es, ax
        xor di, di
        xor ax, ax
        mov cx, 8
        rep stosw
        push cs
        pop es
        mov ax, [blk]
        mov [pb], ax
        mov word [pb+2], cola_e
        mov dx, hijo
        call lanzar
        mov word [pb], 0
        pushf
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        push cs
        pop es
        popf
        call resultado

; ---- entorno normal: el hijo encuentra su nombre tras el doble cero ----
        PRUEBA "ENTORNO-NOMBRE"
        mov word [pb+2], cola_n
        mov dx, hijo
        call lanzar
        call resultado

; ---- EXE con MAXALLOC menor que MINALLOC: se respeta MINALLOC ----
        PRUEBA "EXE-MINALLOC"
        mov word [pb+2], cola_0
        mov dx, exe
        call lanzar
        call resultado

; ---- .COM de casi 64 KB sin memoria: error 8 y cadena de MCB intacta ----
        PRUEBA "COM-SIN-MEMORIA"
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21                    ; BX = bloque libre mayor
        sub bx, 0x101
        mov ah, 0x48
        int 0x21
        jc .fcom
        mov [blk], ax
        mov word [pb+2], cola_0
        mov dx, grande
        mov bx, pb
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        cli
        push cs
        pop ss
        mov sp, pila_fin
        sti
        jnc .fcom_m                 ; se ha ejecutado: mal
        cmp ax, 8
        jne .fcom_l
        call cadena                 ; ZF=1 si la cadena de MCB está bien
        jmp .fcom_l
.fcom_m:
        or al, 1
.fcom_l:
        pushf
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        push cs
        pop es
        popf
        jmp .fcom2
.fcom:
        or al, 1
.fcom2:
        call resultado

; ---- EXE que pide más memoria (MINALLOC) de la que queda: error 8 ----
        PRUEBA "EXE-SIN-MEMORIA"
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        sub bx, 0x101
        mov ah, 0x48
        int 0x21
        jc .fexe
        mov [blk], ax
        mov word [pb+2], cola_0
        mov dx, exe
        mov bx, pb
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        cli
        push cs
        pop ss
        mov sp, pila_fin
        sti
        jnc .fexe_m                 ; se ha ejecutado: mal
        cmp ax, 8
        jne .fexe_l
        call cadena                 ; ZF=1 si la cadena de MCB está bien
        jmp .fexe_l
.fexe_m:
        or al, 1
.fexe_l:
        pushf
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        push cs
        pop es
        popf
        jmp .fexe2
.fexe:
        or al, 1
.fexe2:
        call resultado

; ---- 2Ch: las centésimas avanzan (no siempre 0) ----
        PRUEBA "2C-CENTESIMAS"
        push es
        xor ax, ax
        mov es, ax
        mov si, [es:0x46C]
.t2c:
        mov ah, 0x2C
        int 0x21
        test dl, dl
        jnz .t2ok
        mov ax, [es:0x46C]
        sub ax, si
        cmp ax, 40                  ; ~2 s
        jb .t2c
        pop es
        or al, 1
        jmp .t2f
.t2ok:
        pop es
        cmp al, al
.t2f:
        call resultado

; ---- 2Ch coherente con el reloj (INT 1Ah AH=02h): misma hora y minuto ----
        PRUEBA "2C-HORA"
        mov ah, 2
        int 0x1A
        mov [rtc_hm], cx
        mov ah, 0x2C
        int 0x21
        mov [dos_hm], cx
        mov al, [rtc_hm+1]
        call bcd
        mov bl, 60
        mul bl
        mov bx, ax
        mov al, [rtc_hm]
        call bcd
        xor ah, ah
        add bx, ax                  ; minutos del día según el RTC
        mov al, [dos_hm+1]
        mov cl, 60
        mul cl
        movzx cx, byte [dos_hm]
        add ax, cx                  ; según DOS
        sub ax, bx
        jns .hp
        neg ax
.hp:
        cmp ax, 2                   ; como mucho 2 minutos de diferencia
        jbe .hok
        cmp ax, 1438                ; (o alrededor de medianoche)
        jae .hok
        or al, 1
        jmp .hf
.hok:
        cmp al, al
.hf:
        call resultado

; ---- PSP: palabra 6 (tamaño del segmento, de CP/M) distinta de cero ----
        PRUEBA "PSP-0006"
        cmp byte [5], 0x9A
        jne .p6
        cmp word [6], 0x100
        jb .p6n
        cmp al, al
        jmp .p6
.p6n:
        or al, 1
.p6:
        call resultado

        mov dx, m_fin
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

; ZF=1 -> OK
resultado:
        jz resultado_ok
        jmp resultado_mal
; CF=0 y ZF=1 -> OK
resultado_cf:
        jc resultado_mal
        jnz resultado_mal
resultado_ok:
        mov bx, m_ok
        jmp short res
resultado_mal:
        mov bx, m_mal
res:
        push cs
        pop ds
        push cs
        pop es
        mov dx, m_nuc
        mov ah, 9
        int 0x21
        mov dx, [nombre]
        mov ah, 9
        int 0x21
        mov dx, bx
        mov ah, 9
        int 0x21
        ret

; ejecutar DS:DX con pb -> ZF=1 si el hijo devuelve 55h
lanzar:
        mov [pb+4], cs
        mov word [pb+6], 0x5C
        mov [pb+8], cs
        mov word [pb+10], 0x6C
        mov [pb+12], cs
        mov bx, pb
        mov [sv_sp], sp
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        cli
        push cs
        pop ss
        mov sp, [sv_sp]
        sti
        jc .mal
        mov ah, 0x4D
        int 0x21
        cmp ax, 0x0055
        ret
.mal:
        or al, 1
        ret

; recorrer la cadena de MCB: ZF=1 si acaba en 'Z' justo en el tope de memoria
cadena:
        push es
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        mov cx, 500
.l:
        mov es, ax
        cmp byte [es:0], 'Z'
        je .z
        cmp byte [es:0], 'M'
        jne .mal
        add ax, [es:3]
        inc ax
        loop .l
.mal:
        pop es
        or al, 1
        ret
.z:
        add ax, [es:3]
        inc ax
        pop es
        push ax
        int 0x12
        shl ax, 6
        pop bx
        cmp ax, bx
        ret

bcd:
        mov ah, al
        shr ah, 4
        and al, 15
        aad
        ret

m_nuc   db "NUC $"
m_ok    db " OK", 13, 10, "$"
m_mal   db " FALLO", 13, 10, "$"
m_fin   db "NUC fin", 13, 10, "$"
hijo    db "A:\BIN\NUCHIJO.COM", 0
exe     db "A:\BIN\NUCEXE.EXE", 0
grande  db "A:\BIN\NUCBIG.COM", 0
cola_0  db 0, 13
cola_e  db 2, " E", 13
cola_n  db 2, " N", 13
pb      dw 0, cola_0, 0, 0x5C, 0, 0x6C, 0
nombre  dw 0
mipsp   dw 0
blk     dw 0
blk2    dw 0
sv_sp   dw 0
rtc_hm  dw 0
dos_hm  dw 0
pais    times 48 db 0
corto   times 7 db 0
        times 40 db 0x5A
copia   times 40 db 0
        times 512 db 0
pila_fin:
fin:
