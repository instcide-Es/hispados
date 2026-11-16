; =============================================================================
;  ORDENAR.COM (SORT) - ordena las líneas de un texto
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       orden | ORDENAR      lo que escribe la orden, ordenado
;       ORDENAR < archivo    las líneas del archivo, ordenadas
;       ORDENAR archivo      lo mismo
;  Opciones:  /R    al revés (de la Z a la A)
;             /+n   se compara desde la columna n
;
;  Es un filtro: lee toda la entrada a la memoria (lo que quepa en su
;  segmento: unos 60 K, o menos si la máquina tiene poca), ordena las líneas
;  sin distinguir mayúsculas de minúsculas y las escribe en la salida. Con el
;  teclado como entrada, se acaba con una línea que empiece por Ctrl+Z.
;
;  Se ordenan punteros a las líneas, no las líneas, con el método de Shell:
;  no necesita más memoria y en un 8088 no tarda una eternidad.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

MARGEN      equ 512                 ; lo que se le deja a la pila

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        ; ---- lo que se pide ----
        mov si, 0x81
.arg:
        lodsb
        cmp al, ' '
        je .arg
        jb .leer
        cmp al, '/'
        je .opcion
        dec si                      ; un archivo
        mov dx, si
.n:
        lodsb
        cmp al, ' '
        ja .n
        dec si
        mov bl, [si]
        mov byte [si], 0
        push bx
        mov al, 0
        DOS 0x3D
        pop bx
        mov [si], bl
        jnc .abierto
        mov dx, m_no_esta
        jmp error
.abierto:
        mov [ent], ax
        jmp .arg
.opcion:
        lodsb
        cmp al, '+'
        je .columna
        and al, 0xDF
        cmp al, 'R'
        jne .uso
        mov byte [inverso], 1
        jmp .arg
.uso:
        mov dx, m_uso
        jmp error
.columna:
        xor ax, ax
.c1:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .c2
        mov cx, 10
        mul cx
        xor bh, bh
        add ax, bx
        inc si
        jmp .c1
.c2:
        test ax, ax
        jz .uso
        dec ax
        mov [columna], ax
        jmp .arg
        ; ---- todo el texto, a la memoria ----
.leer:
        mov di, texto
.l:
        mov cx, sp
        sub cx, MARGEN
        sub cx, di
        jbe mucho
        mov dx, di
        mov bx, [ent]
        DOS 0x3F
        jc .leido
        test ax, ax
        jz .leido
        add di, ax
        jmp .l
.leido:
        mov cx, di                  ; hasta el fin de archivo de los textos,
        sub cx, texto               ; si lo hay
        jz .fin_txt
        push di
        mov di, texto
        mov al, 0x1A
        repne scasb
        pop ax
        jne .sin1a
        dec di
        jmp short .fin_txt
.sin1a:
        mov di, ax
.fin_txt:
        mov [fin], di
        mov byte [di], 0
        ; ---- la tabla de punteros, detrás del texto: uno por línea ----
        inc di
        inc di
        and di, 0xFFFE
        mov [tabla], di
        mov bx, di
        mov si, texto
.linea:
        cmp si, [fin]
        jae ordena
        mov ax, sp
        sub ax, MARGEN
        cmp bx, ax
        jae mucho
        mov [bx], si
        add bx, 2
        inc word [n]
.car:
        cmp si, [fin]
        jae ordena
        lodsb
        cmp al, 10
        je .corta
        cmp al, 13
        jne .car
        mov byte [si-1], 0          ; retorno (y el salto que le siga)
        cmp si, [fin]
        jae ordena
        cmp byte [si], 10
        jne .linea
        inc si
        jmp .linea
.corta:
        mov byte [si-1], 0
        jmp .linea

mucho:
        mov dx, m_mucho
error:
        DOS 0x09
        mov ax, 0x4C01
        int 0x21

; ---- ordenar (Shell): con saltos cada vez menores, cada línea se lleva
;      hacia atrás hasta su sitio entre las que distan ese salto ----
ordena:
        mov ax, [n]
        shr ax, 1
        mov [salto], ax
.s:
        cmp word [salto], 0
        je escribe
        mov ax, [salto]
        mov [i], ax
.i:
        mov ax, [i]
        cmp ax, [n]
        jae .menor
        mov bx, ax
        shl bx, 1
        add bx, [tabla]
        mov ax, [bx]
        mov [esta], ax              ; la línea que se coloca
        mov ax, [i]
        mov [j], ax
.j:
        mov ax, [j]
        cmp ax, [salto]
        jb .pon
        sub ax, [salto]
        mov bx, ax
        shl bx, 1
        add bx, [tabla]
        mov si, [bx]                ; la de un salto más atrás: ¿va detrás?
        mov di, [esta]
        call compara
        jbe .pon
        mov ax, [bx]                ; sí: avanza un salto y se sigue mirando
        mov di, [j]
        shl di, 1
        add di, [tabla]
        mov [di], ax
        mov ax, [salto]
        sub [j], ax
        jmp .j
.pon:
        mov bx, [j]
        shl bx, 1
        add bx, [tabla]
        mov ax, [esta]
        mov [bx], ax
        inc word [i]
        jmp .i
.menor:
        shr word [salto], 1
        jmp .s

; compara las líneas de SI y DI desde la columna pedida, sin distinguir
; mayúsculas: las banderas, como «cmp SI, DI» (al revés con /R). Conserva BX
compara:
        cmp byte [inverso], 0
        je .col
        xchg si, di
.col:
        mov cx, [columna]
        jcxz .c
.s1:
        cmp byte [si], 0            ; (una línea más corta que la columna
        je .s2                      ; cuenta como vacía)
        inc si
        loop .s1
.s2:
        mov cx, [columna]
.s3:
        cmp byte [di], 0
        je .c
        inc di
        loop .s3
.c:
        mov al, [si]
        call mayus
        mov ah, al
        mov al, [di]
        call mayus
        cmp ah, al
        jne .x
        inc si
        inc di
        test al, al
        jnz .c                      ; (iguales hasta el final: ZF=1)
.x:
        ret
; AL, a mayúscula (también la eñe de la página 437)
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .otras
        sub al, 32
.x:
        ret
.otras:
        cmp al, 0xA4
        jne .x
        inc al
        ret

; ---- las líneas, por su orden, a la salida ----
escribe:
        mov si, [tabla]
        mov cx, [n]
        jcxz .fin
.l:
        push cx
        lodsw
        mov dx, ax
        mov di, ax                  ; cuánto mide
        xor al, al
        mov cx, 0xFFFF
        repne scasb
        not cx
        dec cx
        mov bx, 1
        DOS 0x40
        mov dx, m_nl
        mov cx, 2
        mov bx, 1
        DOS 0x40
        pop cx
        loop .l
.fin:
        mov ax, 0x4C00
        int 0x21

m_nl        db 13, 10
m_uso       db "ORDENAR - ordena las líneas de un texto", 13, 10
            db "  orden | ORDENAR [/R] [/+n]     ORDENAR [/R] [/+n] archivo", 13, 10
            db "  /R: al revés. /+n: se compara desde la columna n.", 13, 10, "$"
m_no_esta   db "No existe ese archivo.", 13, 10, "$"
m_mucho     db "ORDENAR: el texto no cabe en la memoria.", 13, 10, "$"
ent         dw 0                    ; de dónde se lee (0: la entrada)
columna     dw 0
n           dw 0                    ; cuántas líneas
inverso     db 0

        absolute $
            alignb 2
fin         resw 1                  ; donde acaba el texto
tabla       resw 1                  ; los punteros a las líneas
salto       resw 1
i           resw 1
j           resw 1
esta        resw 1
texto:
