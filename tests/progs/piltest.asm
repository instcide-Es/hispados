; PILTEST.COM - INT 21h no debe trabajar sobre la pila del programa.
; El programa se pone una pila de 96 octetos con una zona de guarda debajo
; (1024 octetos con un dibujo conocido) y crea, escribe, lee, busca,
; renombra y borra archivos, hace directorios, usa FCB, INT 25h y EXEC.
; Tras cada prueba comprueba que la guarda sigue intacta; al final dice
; cuántos octetos de su pila se han llegado a usar.
; Cada prueba escribe "PIL <nombre> OK" o "PIL <nombre> FALLO".
; Con la cola " H" es el hijo del EXEC: saluda y devuelve 5Ah.
; INT 21h must not run on the caller's stack: 96-byte stack + guard area.
        cpu 386
        bits 16
        org 0x100

PILA    equ 96
GUARDA  equ 1024
NDATOS  equ 700

%macro PRUEBA 1
        mov word [nombre], %%n
        jmp %%s
%%n:    db %1, "$"
%%s:
%endmacro

inicio:
        cmp byte [0x82], 'H'
        jne padre
        ; el hijo también con una pila pequeña
        mov sp, pila_fin
        mov dx, m_hijo
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C5A
        int 0x21

padre:
        mov di, guarda
        mov cx, GUARDA
.g:
        mov ax, di
        xor al, 0x5A
        stosb
        loop .g
        mov di, pila
        mov cx, PILA
        mov al, 0xCC
        rep stosb
        mov sp, pila_fin
        mov bx, fin + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; datos de prueba
        mov di, datos
        mov cx, NDATOS
.d:
        mov ax, cx
        imul ax, ax, 7
        stosb
        loop .d

; ---- funciones que MS-DOS atiende sin cambiar de pila ----
        PRUEBA "LIGERAS"
        mov ah, 0x62
        int 0x21
        mov ax, cs
        cmp bx, ax
        jne .fl
        mov ah, 0x51
        xor bx, bx
        int 0x21
        mov ax, cs
        cmp bx, ax
        jne .fl
        mov ah, 0x50                ; fijar el mismo PSP
        int 0x21
        mov ah, 0x62
        xor bx, bx
        int 0x21
        mov ax, cs
        cmp bx, ax
        jne .fl
        mov ax, 0x3300
        mov dl, 0x77
        int 0x21
        cmp dl, 1
        ja .fl
        mov ax, 0x3306
        int 0x21
        cmp bx, 0x1606
.fl:
        call resultado

; ---- crear y escribir ----
        PRUEBA "CREAR-ESCRIBIR"
        mov dx, f_uno
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .fc
        mov bx, ax
        mov dx, datos
        mov cx, NDATOS
        mov ah, 0x40
        int 0x21
        jc .fc
        cmp ax, NDATOS
        jne .fc
        mov ah, 0x3E
        int 0x21
        jc .fc
        cmp ax, ax
.fc:
        call resultado_cf

; ---- abrir, leer y situarse ----
        PRUEBA "LEER"
        mov dx, f_uno
        mov ax, 0x3D00
        int 0x21
        jc .fr
        mov bx, ax
        mov dx, leido
        mov cx, NDATOS + 50
        mov ah, 0x3F
        int 0x21
        jc .fr
        cmp ax, NDATOS
        jne .fr
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        jc .fr
        cmp ax, NDATOS
        jne .fr
        mov ah, 0x3E
        int 0x21
        jc .fr
        mov si, datos
        mov di, leido
        mov cx, NDATOS
        repe cmpsb
.fr:
        call resultado_cf

; ---- buscar ----
        PRUEBA "BUSCAR"
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, f_patron
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jc .fb
        cmp dword [dta+0x1E], "PILA"
        jne .fb
        cmp dword [dta+0x1A], NDATOS
        jne .fb
        mov ah, 0x4F
        int 0x21
        jnc .fbm
        cmp ax, 18
        jmp .fb
.fbm:
        or al, 1
.fb:
        call resultado_cf

; ---- renombrar y borrar ----
        PRUEBA "RENOMBRAR-BORRAR"
        mov dx, f_uno
        mov di, f_dos
        mov ah, 0x56
        int 0x21
        jc .fd
        mov dx, f_dos
        mov ah, 0x41
        int 0x21
        jc .fd
        mov dx, f_dos
        mov ax, 0x3D00
        int 0x21
        jnc .fdm
        cmp ax, 2
        jmp .fd
.fdm:
        or al, 1
.fd:
        call resultado_cf

; ---- directorios ----
        PRUEBA "DIRECTORIOS"
        mov dx, f_dir
        mov ah, 0x39
        int 0x21
        jc .fm
        mov dx, f_dir
        mov ah, 0x3B
        int 0x21
        jc .fm
        mov si, leido
        xor dl, dl
        mov ah, 0x47
        int 0x21
        jc .fm
        mov dx, f_sube
        mov ah, 0x3B
        int 0x21
        jc .fm
        mov dx, f_dir
        mov ah, 0x3A
        int 0x21
        jc .fm
        ; la ruta acababa en PILADIR
        mov si, leido
.fz:
        lodsb
        test al, al
        jnz .fz
        cmp dword [si-8], "PILA"
.fm:
        call resultado_cf

; ---- FCB: crear, escribir, cerrar, borrar ----
        PRUEBA "FCB"
        mov dx, fcb
        mov ah, 0x16
        int 0x21
        test al, al
        jnz .ff
        mov dx, datos
        mov ah, 0x1A
        int 0x21
        mov dx, fcb
        mov ah, 0x15
        int 0x21
        test al, al
        jnz .ff
        mov dx, fcb
        mov ah, 0x10
        int 0x21
        test al, al
        jnz .ff
        mov dx, fcb
        mov ah, 0x13
        int 0x21
        test al, al
.ff:
        call resultado

; ---- espacio libre (recorre la FAT), fecha, hora, versión ----
        PRUEBA "VARIAS"
        xor dl, dl
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        je .fv
        mov ah, 0x2A
        int 0x21
        cmp cx, 1980
        jb .fv
        mov ah, 0x2C
        int 0x21
        cmp ch, 23
        ja .fv
        mov ah, 0x30
        int 0x21
        cmp al, 6
        jne .fv
        mov ah, 0x0B
        int 0x21
        mov dl, '.'
        mov ah, 0x02
        int 0x21
        mov dx, m_crlf
        mov cx, 2
        mov bx, 1
        mov ah, 0x40
        int 0x21
        jc .fv
        cmp ax, 2
.fv:
        call resultado

; ---- INT 25h: leer el sector 0 de la unidad actual ----
        PRUEBA "INT25"
        mov ah, 0x19
        int 0x21
        mov [sp_antes], sp
        mov cx, 1
        xor dx, dx
        mov bx, leido
        int 0x25
        jc .fi
        add sp, 2                   ; INT 25h deja los indicadores
        cmp sp, [sp_antes]
        jne .fi2
        cmp word [leido+510], 0xAA55
        jmp .fi2
.fi:
        mov sp, [sp_antes]
        or al, 1
.fi2:
        call resultado

; ---- EXEC: este mismo programa como hijo ----
        PRUEBA "EXEC"
        ; el nombre del programa está al final del entorno
        mov es, [0x2C]
        xor di, di
.fe:
        cmp word [es:di], 0
        je .fef
        inc di
        jmp .fe
.fef:
        add di, 4
        mov si, di
        mov di, ruta
.fec:
        mov al, [es:si]
        mov [di], al
        inc si
        inc di
        test al, al
        jnz .fec
        push ds
        pop es
        mov [pb+4], cs
        mov [pb+8], cs
        mov [pb+12], cs
        mov [sp_antes], sp
        mov dx, ruta
        mov bx, pb
        mov ax, 0x4B00
        int 0x21
        jc .fx
        cmp sp, [sp_antes]
        jne .fx
        mov ax, ss
        mov bx, cs
        cmp ax, bx
        jne .fx
        mov ah, 0x4D
        int 0x21
        cmp ax, 0x005A
.fx:
        push cs
        pop ds
        push cs
        pop es
        call resultado_cf

; ---- cuánto se ha usado de la pila ----
        mov si, pila
.pu:
        cmp byte [si], 0xCC
        jne .pf
        inc si
        cmp si, pila_fin
        jb .pu
.pf:
        mov ax, pila_fin
        sub ax, si
        mov bl, 10
        div bl
        add ax, '00'
        mov [m_usad_n], ax
        mov dx, m_usados
        mov ah, 0x09
        int 0x21
        mov dx, m_fin
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C00
        int 0x21

; ZF=1 y CF=0 -> bien
resultado_cf:
        jnc resultado
        or al, 1
; ZF=1 -> bien. Además, la guarda debe seguir intacta.
resultado:
        mov word [texto], m_ok
        je .g
        mov word [texto], m_fallo
.g:
        mov si, guarda
        mov cx, GUARDA
.l:
        mov ax, si
        xor al, 0x5A
        cmp al, [si]
        jne .rota
        inc si
        loop .l
        jmp .di
.rota:
        mov word [texto], m_guarda
.di:
        mov dx, m_pil
        mov ah, 0x09
        int 0x21
        mov dx, [nombre]
        mov ah, 0x09
        int 0x21
        mov dx, [texto]
        mov ah, 0x09
        int 0x21
        ret

m_pil       db "PIL $"
m_ok        db " OK", 13, 10, "$"
m_fallo     db " FALLO", 13, 10, "$"
m_guarda    db " FALLO: se ha escrito debajo de la pila", 13, 10, "$"
m_hijo      db "PIL hijo en marcha", 13, 10, "$"
m_usados    db "PIL pila usada="
m_usad_n    db "00 de 96", 13, 10, "$"
m_fin       db "PIL fin", 13, 10, "$"
m_crlf      db 13, 10
f_uno       db "PILA1.TMP", 0
f_dos       db "PILA2.TMP", 0
f_patron    db "PILA?.TMP", 0
f_dir       db "PILADIR", 0
f_sube      db "..", 0
fcb         db 0, "PILAFCB TMP"
            times 25 db 0
cola        db 2, " H", 13
pb          dw 0
            dw cola, 0
            dw 0x5C, 0
            dw 0x6C, 0
nombre      dw 0
texto       dw 0
sp_antes    dw 0

        section .bss
ruta        resb 80
dta         resb 128
datos       resb NDATOS
leido       resb NDATOS + 60
guarda      resb GUARDA
pila        resb PILA
pila_fin:
fin:
