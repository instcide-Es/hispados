; =============================================================================
;  ARCH.COM - prueba de los archivos de Hispa-8086 por INT 21h
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hace lo que haría un programa de la época y comprueba cada paso; si algo
;  no sale como en MS-DOS, dice en qué paso y termina con el código 1. Lo que
;  deja en el disco lo mira luego la prueba desde fuera (prueba.sh):
;    PRUEBA.DAT   3.000 octetos escritos y luego cortado a 500
;    GRANDE.DAT   20.000 octetos (cuatro tandas de 5.000: varios grupos)
;    SUB\TRES.TXT «hola», creado como DOS.TXT y renombrado
;  El octeto número i de cada tanda vale (i*7+3) mod 256.
; =============================================================================
        cpu 8086
        org 0x100
%macro PASO 0
        inc byte [paso]
%endmacro
%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro
        mov di, buf                 ; el dibujo de los datos
        mov cx, 5000
        mov al, 3
.d:
        stosb
        add al, 7
        loop .d

        PASO                        ; 1: crear y escribir 3.000
        mov dx, n_prueba
        xor cx, cx
        DOS 0x3C
        jc fallo
        mov bx, ax
        mov dx, buf
        mov cx, 3000
        DOS 0x40
        jc fallo
        cmp ax, 3000
        jne fallo
        DOS 0x3E
        jc fallo

        PASO                        ; 2: abrir, ir al final: 3.000
        mov dx, n_prueba
        mov al, 2
        DOS 0x3D
        jc fallo
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov al, 2
        DOS 0x42
        jc fallo
        cmp ax, 3000
        jne fallo
        test dx, dx
        jnz fallo

        PASO                        ; 3: ir al 1.000, leer 10 y compararlos
        xor cx, cx
        mov dx, 1000
        mov al, 0
        DOS 0x42
        jc fallo
        mov dx, leido
        mov cx, 10
        DOS 0x3F
        jc fallo
        cmp ax, 10
        jne fallo
        mov si, buf + 1000
        mov di, leido
        mov cx, 10
        repe cmpsb
        jne fallo

        PASO                        ; 4: leer más allá del final: lo que quede
        xor cx, cx
        mov dx, 2995
        mov al, 0
        DOS 0x42
        mov dx, leido
        mov cx, 10
        DOS 0x3F
        jc fallo
        cmp ax, 5
        jne fallo
        mov dx, leido               ; y ya en el final: nada, sin error
        mov cx, 10
        DOS 0x3F
        jc fallo
        test ax, ax
        jnz fallo

        PASO                        ; 5: cortarlo a 500 (escribir cero octetos)
        xor cx, cx
        mov dx, 500
        mov al, 0
        DOS 0x42
        xor cx, cx
        DOS 0x40
        jc fallo
        DOS 0x3E
        jc fallo

        PASO                        ; 6: un archivo de varios grupos
        mov dx, n_grande
        xor cx, cx
        DOS 0x3C
        jc fallo
        mov bx, ax
        mov si, 4
.g:
        mov dx, buf
        mov cx, 5000
        DOS 0x40
        jc fallo
        cmp ax, 5000
        jne fallo
        dec si
        jnz .g
        DOS 0x3E
        jc fallo

        PASO                        ; 7: un directorio y un archivo dentro
        mov dx, n_sub
        DOS 0x39
        jc fallo
        mov dx, n_dos
        xor cx, cx
        DOS 0x3C
        jc fallo
        mov bx, ax
        mov dx, t_hola
        mov cx, 4
        DOS 0x40
        jc fallo
        DOS 0x3E
        jc fallo

        PASO                        ; 8: cambiarle el nombre
        mov dx, n_dos
        mov di, n_tres
        DOS 0x56
        jc fallo
        mov dx, n_dos               ; el viejo ya no está: error 2
        mov al, 0
        DOS 0x3D
        jnc fallo
        cmp ax, 2
        jne fallo

        PASO                        ; 9: buscar en el directorio: «.», «..» y TRES.TXT
        mov dx, zona
        DOS 0x1A
        mov dx, n_todos
        mov cx, 0x10
        DOS 0x4E
        jc fallo
        xor si, si
.b:
        inc si
        DOS 0x4F
        jnc .b
        cmp ax, 18
        jne fallo
        cmp si, 3
        jne fallo
        mov si, zona + 30           ; el último encontrado, y su tamaño
        mov di, n_tres + 4
        mov cx, 9
        repe cmpsb
        jne fallo
        cmp word [zona+26], 4
        jne fallo

        PASO                        ; 10: entrar, ver dónde se está, abrir sin ruta, salir
        mov dx, n_sub
        DOS 0x3B
        jc fallo
        mov si, leido
        xor dl, dl
        DOS 0x47
        jc fallo
        cmp word [leido], 'SU'
        jne fallo
        cmp word [leido+2], 'B'
        jne fallo
        mov dx, n_tres + 4
        mov al, 0
        DOS 0x3D
        jc fallo
        mov bx, ax
        mov dx, leido
        mov cx, 20
        DOS 0x3F
        jc fallo
        cmp ax, 4
        jne fallo
        cmp word [leido], 'ho'
        jne fallo
        DOS 0x3E
        mov dx, n_arriba
        DOS 0x3B
        jc fallo

        PASO                        ; 11: lo que no se puede
        mov dx, n_sub               ; quitar un directorio con algo dentro: 5
        DOS 0x3A
        jnc fallo
        cmp ax, 5
        jne fallo
        mov dx, n_noruta            ; abrir en una ruta que no existe: 3
        mov al, 0
        DOS 0x3D
        jnc fallo
        cmp ax, 3
        jne fallo
        mov bx, 77                  ; un número de archivo que no es: 6
        DOS 0x3E
        jnc fallo
        cmp ax, 6
        jne fallo

        PASO                        ; 12: borrar, y quitar un directorio vacío
        mov dx, n_vacio
        DOS 0x39
        jc fallo
        mov dx, n_vacio
        DOS 0x3A
        jc fallo
        mov dx, n_temp
        xor cx, cx
        DOS 0x3C
        jc fallo
        mov bx, ax
        DOS 0x3E
        mov dx, n_temp
        DOS 0x41
        jc fallo
        mov dx, n_temp
        DOS 0x41
        jnc fallo

        mov dx, m_bien
        DOS 9
        mov ax, 0x4C00
        int 0x21
fallo:
        push ax
        mov dx, m_fallo
        DOS 9
        mov al, [paso]
        xor ah, ah
        mov bl, 10
        div bl
        push ax
        add al, '0'
        mov dl, al
        DOS 2
        pop ax
        mov dl, ah
        add dl, '0'
        DOS 2
        mov dx, m_error
        DOS 9
        pop ax
        add al, '0'
        mov dl, al
        DOS 2
        mov ax, 0x4C01
        int 0x21

paso        db 0
n_prueba    db "PRUEBA.DAT", 0
n_grande    db "grande.dat", 0
n_sub       db "SUB", 0
n_dos       db "SUB\DOS.TXT", 0
n_tres      db "SUB\TRES.TXT", 0
n_todos     db "SUB\*.*", 0
n_arriba    db "..", 0
n_noruta    db "NOHAY\X.TXT", 0
n_vacio     db "VACIO", 0
n_temp      db "TEMP.$$$", 0
t_hola      db "hola"
m_bien      db "ARCHIVOS: BIEN", 13, 10, "$"
m_fallo     db "ARCHIVOS: FALLO en el paso $"
m_error     db ", error $"
leido       times 24 db 0
zona        times 48 db 0
buf:
