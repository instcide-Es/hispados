; FSTEST.COM - pruebas del sistema de archivos del núcleo (INT 21h): handles,
; FCB, búsquedas, rutas, etiquetas, disco lleno y directorio raíz lleno.
; Cada prueba escribe «Tnn OK» o «Tnn FALLO ax=XXXX».
;   FSTEST      pruebas generales en la unidad y carpeta actuales
;   FSTEST L    abre 10 FCB sin cerrarlos y termina (fuga de la SFT)
;   FSTEST F    disco lleno y raíz llena (en un disco pequeño y vacío)
;   FSTEST C    en una carpeta de un CD con ZFINAL.TXT: no se puede escribir
;   FSTEST P    en un disco protegido contra escritura con X.TXT y D1: todo
;               lo que escribe da error y el disco se sigue viendo igual
;   FSTEST E    abre X.TXT y espera una tecla (se quita el disco): leer da
;               error, no fin de fichero; otra tecla (se vuelve a poner): lee
;   FSTEST X    con X.TXT abierto y ZZ.DAT a medio escribir espera una tecla
;               (se cambia el disco por otro): ni se lee ni se escribe en el
;               disco nuevo
;   FSTEST S    600 operaciones al azar (siempre las mismas) sobre 8 ficheros
;               S0.DAT..S7.DAT: escribir, recortar, borrar, renombrar y
;               comprobar. El octeto en la posición o del fichero f es
;               (o_bajo xor o_alto) + 31*f: se puede comprobar desde fuera.
        cpu 386
        bits 16
        org 0x100

%macro T 1
        mov word [tnum], %1
%endmacro
; DOS con DX = cadena
%macro DOSDX 2
        mov dx, %2
        mov ah, %1
        int 0x21
%endmacro

start:
        ; error crítico: siempre «descartar» (las pruebas miran el código)
        mov dx, crit_fail
        mov ax, 0x2524
        int 0x21
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        and al, 0xDF
        cmp al, 'L'
        je near mode_leak
        cmp al, 'F'
        je near mode_full
        cmp al, 'C'
        je near mode_cd
        cmp al, 'S'
        je near mode_stress
        cmp al, 'P'
        je near mode_prot
        cmp al, 'E'
        je near mode_eject
        cmp al, 'X'
        je near mode_xchg
        jmp mode_main

; ---------------------------------------------------------------------------
say:
        mov ah, 9
        int 0x21
        ret
; «Tnn »
say_t:
        push ax
        mov ax, [tnum]
        mov bl, 10
        div bl
        add ax, '00'
        mov [m_t+1], ax
        mov dx, m_t
        call say
        pop ax
        ret
ok:
        call say_t
        mov dx, m_ok
        jmp say
; AX = valor que se enseña
bad:
        call say_t
        mov di, m_bad_ax
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        stosb
        pop ax
        loop .h
        mov dx, m_bad
        jmp say

; DX = nombre -> fichero de 100 octetos con el patrón; CF si falla
mk_file:
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .r
        mov bx, ax
        mov dx, pattern
        mov cx, 100
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
        clc
.r:
        ret
; BX = handle -> cerrar
close:
        mov ah, 0x3E
        int 0x21
        ret
; etiqueta del sector de arranque -> DS:DI (11 octetos)
get_label:
        push di
        mov ax, 0x440D
        mov cx, 0x0866
        xor bx, bx
        mov dx, idbuf
        int 0x21
        pop di
        mov si, idbuf+6
        mov cx, 11
        rep movsb
        ret

; ---------------------------------------------------------------------------
mode_main:
        ; patrón de datos
        mov di, pattern
        xor al, al
        mov cx, 256
.pf:
        stosb
        inc al
        loop .pf
        mov dx, dta
        mov ah, 0x1A
        int 0x21

; T01: 6Ch: la acción va en DX (y DX se conserva); CX dice qué se hizo
        T 1
        DOSDX 0x41, f1
        mov dx, 0x0012              ; no existe: crear
        call .open6c
        jc .t1b
        cmp cx, 2
        jne .t1b
        cmp dx, 0x0012
        mov ax, dx
        jne .t1b
        call close
        mov dx, 0x0010              ; existe y se pide fallar: 50h
        call .open6c
        jnc .t1b
        cmp ax, 0x50
        jne .t1b
        mov dx, 0x0001              ; existe: abrir
        call .open6c
        jc .t1b
        cmp cx, 1
        jne .t1b
        call close
        mov dx, 0x0012              ; existe: sustituir
        call .open6c
        jc .t1b
        cmp cx, 3
        jne .t1b
        call close
        DOSDX 0x41, f1
        mov dx, 0x0001              ; no existe y no se pide crear: 2
        call .open6c
        jnc .t1b
        cmp ax, 2
        jne .t1b
        call ok
        jmp .t2
.t1b:
        call bad
        jmp .t2
; DX = acción -> 6C00h sobre F1.DAT; BX = handle
.open6c:
        mov ax, 0x6C00
        mov bx, 0x0002
        xor cx, cx
        mov si, f1
        int 0x21
        mov bx, ax
        ret
.t2:
; T02: borrar con FCB (13h) un fichero normal no toca la etiqueta del
;      sector de arranque
        T 2
        mov dx, f1
        call mk_file
        mov di, lab1
        call get_label
        mov dx, fcb1
        mov ah, 0x13
        int 0x21
        xor ah, ah
        test al, al
        jnz .t2b
        mov di, lab2
        call get_label
        mov si, lab1
        mov di, lab2
        mov cx, 11
        repe cmpsb
        mov ax, 0xEEEE
        jne .t2b
        call ok
        jmp .t3
.t2b:
        call bad
.t3:
; T03: abrir 45 veces el mismo FCB sin cerrarlo no agota la tabla
        T 3
        mov dx, f1
        call mk_file
        mov cx, 45
.t3l:
        push cx
        mov dx, fcb1
        mov ah, 0x0F
        int 0x21
        pop cx
        test al, al
        jnz .t3b
        loop .t3l
        mov dx, fcb1
        mov ah, 0x10
        int 0x21
        mov ax, 0x3D00
        mov dx, f1
        int 0x21
        jc .t3c
        mov bx, ax
        call close
        call ok
        jmp .t4
.t3b:
        mov ax, cx
.t3c:
        call bad
.t4:
; T04: 4Eh sin coincidencias -> 12h; con carpeta inexistente -> 3
        T 4
        mov cx, 0
        DOSDX 0x4E, n_nomatch
        jnc .t4b
        cmp ax, 0x12
        jne .t4b
        mov cx, 0
        DOSDX 0x4E, n_nodir
        jnc .t4b
        cmp ax, 3
        jne .t4b
        call ok
        jmp .t5
.t4b:
        call bad
.t5:
; T05: etiqueta con FCB extendido desde una subcarpeta: va a la raíz
        T 5
        DOSDX 0x39, n_sub
        DOSDX 0x3B, n_sub
        jc .t5b
        mov dx, xfcb_all            ; borrar la etiqueta que haya
        mov ah, 0x13
        int 0x21
        mov dx, xfcb_lab            ; crear ETIQTEST
        mov ah, 0x16
        int 0x21
        xor ah, ah
        test al, al
        jnz .t5b
        mov dx, xfcb_lab
        mov ah, 0x10
        int 0x21
        ; buscarla con FCB desde la subcarpeta
        mov dx, xfcb_all
        mov ah, 0x11
        int 0x21
        mov ah, 1
        test al, al
        jnz .t5b
        cmp dword [dta+8], 'ETIQ'
        mov ax, 0x0002
        jne .t5b
        DOSDX 0x3B, n_root
        DOSDX 0x3A, n_sub           ; si la etiqueta quedó dentro, no se deja
        jc .t5b
        mov cx, 8
        DOSDX 0x4E, n_rootall
        jc .t5b
        cmp dword [dta+0x1E], 'ETIQ'
        mov ax, 0x0003
        jne .t5b
        call ok
        jmp .t6
.t5b:
        push ax
        DOSDX 0x3B, n_root
        pop ax
        call bad
.t6:
; T06: nombres en blanco: MD y REN deben fallar
        T 6
        DOSDX 0x39, n_blank
        mov ax, 1
        jnc .t6b
        push ds
        pop es
        mov di, n_blank
        DOSDX 0x56, f1
        mov ax, 2
        jnc .t6b
        ; «A.B.C» no deja un punto dentro del nombre: queda A.B
        xor cx, cx
        DOSDX 0x3C, n_abc
        jc .t6b
        mov bx, ax
        call close
        mov cx, 0
        DOSDX 0x4E, n_aq
        mov ax, 4
        jc .t6b
        cmp dword [dta+0x1E], 'A.B'
        mov ax, 5
        jne .t6b
        DOSDX 0x41, n_ab
        jc .t6b
        call ok
        jmp .t7
.t6b:
        call bad
.t7:
; T07: comodines en 41h, 3Dh, 56h y 43h: error y el fichero sigue
        T 7
        mov dx, n_w1
        call mk_file
        DOSDX 0x41, n_wq
        mov ax, 1
        jnc .t7b
        mov ax, 0x3D00
        mov dx, n_wq
        int 0x21
        jc .t7c
        mov bx, ax
        call close
        mov ax, 2
        jmp .t7b
.t7c:
        push ds
        pop es
        mov di, n_w2
        DOSDX 0x56, n_wq
        mov ax, 3
        jnc .t7b
        mov ax, 0x4300
        mov dx, n_wq
        int 0x21
        mov ax, 4
        jnc .t7b
        mov ax, 0x4300
        mov dx, n_w1
        int 0x21
        mov ax, 5
        jc .t7b
        DOSDX 0x41, n_w1
        jc .t7b
        call ok
        jmp .t8
.t7b:
        call bad
.t8:
; T08: 29h no toca GS y analiza el nombre
        T 8
        mov ax, 0x1234
        mov gs, ax
        push ds
        pop es
        mov si, n_parse
        mov di, pfcb
        mov ax, 0x2901
        int 0x21
        mov ax, gs
        cmp ax, 0x1234
        jne .t8b
        cmp byte [pfcb], 1
        jne .t8b
        cmp dword [pfcb+1], 'FOO '
        jne .t8b
        cmp word [pfcb+9], 'TX'
        jne .t8b
        call ok
        jmp .t9
.t8b:
        call bad
.t9:
; T09: abrir o borrar una carpeta como fichero -> acceso denegado (5)
        T 9
        mov ax, 0x3D00
        mov dx, n_docs
        int 0x21
        jnc .t9b
        cmp ax, 5
        jne .t9b
        DOSDX 0x41, n_docs
        jnc .t9b
        cmp ax, 5
        jne .t9b
        call ok
        jmp .t10
.t9b:
        call bad
.t10:
; T10: lseek desde el final, leer hasta y más allá del final, 4203h
        T 10
        mov ax, 0x3D00
        mov dx, f1
        int 0x21
        jc .t10b
        mov [h1], ax
        mov bx, ax
        mov ax, 0x4202
        mov cx, 0xFFFF
        mov dx, -10
        int 0x21
        jc .t10b
        test dx, dx
        jnz .t10b
        cmp ax, 90
        jne .t10b
        mov cx, 20
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .t10b
        cmp ax, 10
        jne .t10b
        cmp byte [buf], 90
        jne .t10b
        mov cx, 20
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .t10b
        test ax, ax
        jnz .t10b
        mov ax, 0x4200
        xor cx, cx
        mov dx, 1000
        int 0x21
        mov cx, 20
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .t10b
        test ax, ax
        jnz .t10b
        mov ax, 0x4203
        xor cx, cx
        xor dx, dx
        int 0x21
        jnc .t10b
        cmp ax, 1
        jne .t10b
        mov bx, [h1]
        call close
        call ok
        jmp .t11
.t10b:
        call bad
        mov bx, [h1]
        call close
.t11:
; T11: escribir 0 octetos recorta y alarga
        T 11
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t11b
        mov [h1], ax
        mov bx, ax
        mov ax, 0x4200
        xor cx, cx
        mov dx, 50
        int 0x21
        xor cx, cx
        mov ah, 0x40
        int 0x21
        jc .t11b
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        cmp ax, 50
        jne .t11b
        mov ax, 0x4200
        xor cx, cx
        mov dx, 5000
        int 0x21
        xor cx, cx
        mov ah, 0x40
        int 0x21
        call close
        mov cx, 0
        DOSDX 0x4E, f1
        jc .t11b
        mov ax, [dta+0x1A]
        cmp ax, 5000
        jne .t11b
        ; los primeros 50 siguen ahí
        mov ax, 0x3D00
        mov dx, f1
        int 0x21
        jc .t11b
        mov bx, ax
        mov cx, 50
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        push ax
        call close
        pop ax
        cmp ax, 50
        jne .t11b
        cmp byte [buf+49], 49
        jne .t11b
        call ok
        jmp .t12
.t11b:
        call bad
.t12:
; T12: 4Eh con nombre de dispositivo (IF EXIST carpeta\NUL)
        T 12
        mov cx, 0
        DOSDX 0x4E, n_nul
        mov ax, 1
        jc .t12b
        mov cx, 0
        DOSDX 0x4E, n_docsnul
        mov ax, 2
        jc .t12b
        DOSDX 0x4F, n_docsnul
        mov ax, 3
        jnc .t12b
        mov cx, 0
        DOSDX 0x4E, n_nadanul
        mov ax, 4
        jnc .t12b
        call ok
        jmp .t13
.t12b:
        call bad
.t13:
; T13: una subcarpeta crece: 40 ficheros, se cuentan 42 entradas, se borran
        T 13
        DOSDX 0x39, n_gr
        jc .t13b
        mov word [cnt], 0
.t13c:
        call gr_name
        xor cx, cx
        DOSDX 0x3C, n_grf
        jc .t13b
        mov bx, ax
        call close
        inc word [cnt]
        cmp word [cnt], 40
        jb .t13c
        mov word [cnt], 0
        mov cx, 0x16
        DOSDX 0x4E, n_grall
        jc .t13b
.t13n:
        inc word [cnt]
        mov ah, 0x4F
        int 0x21
        jnc .t13n
        cmp ax, 0x12
        jne .t13b
        mov ax, [cnt]
        cmp ax, 42
        jne .t13b
        mov word [cnt], 0
.t13d:
        call gr_name
        DOSDX 0x41, n_grf
        jc .t13b
        inc word [cnt]
        cmp word [cnt], 40
        jb .t13d
        DOSDX 0x3A, n_gr
        jc .t13b
        call ok
        jmp .t14
.t13b:
        call bad
.t14:
; T14: mover con 56h a otra carpeta y volver; el contenido sigue
        T 14
        mov dx, f1
        call mk_file
        DOSDX 0x39, n_m1
        push ds
        pop es
        mov di, n_m1f
        DOSDX 0x56, f1
        jc .t14b
        mov ax, 0x3D00
        mov dx, n_m1f
        int 0x21
        jc .t14b
        mov bx, ax
        mov cx, 200
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        push ax
        call close
        pop ax
        cmp ax, 100
        jne .t14b
        mov si, buf
        mov di, pattern
        mov cx, 100
        repe cmpsb
        jne .t14b
        mov di, f1
        DOSDX 0x56, n_m1f
        jc .t14b
        DOSDX 0x3A, n_m1
        jc .t14b
        call ok
        jmp .t15
.t14b:
        call bad
.t15:
; T15: 5Bh sobre uno que existe -> 50h; 5Ah crea un temporal
        T 15
        xor cx, cx
        DOSDX 0x5B, f1
        jnc .t15b
        cmp ax, 0x50
        jne .t15b
        xor cx, cx
        DOSDX 0x5A, tmppath
        jc .t15b
        mov bx, ax
        call close
        DOSDX 0x41, tmppath
        jc .t15b
        call ok
        jmp .t16
.t15b:
        call bad
.t16:
; T16: fichero de solo lectura: 3D02h, 3Ch y 41h -> 5
        T 16
        mov ax, 0x4301
        mov cx, 1
        mov dx, f1
        int 0x21
        jc .t16b
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jnc .t16b
        cmp ax, 5
        jne .t16b
        xor cx, cx
        DOSDX 0x3C, f1
        jnc .t16b
        cmp ax, 5
        jne .t16b
        DOSDX 0x41, f1
        jnc .t16b
        cmp ax, 5
        jne .t16b
        mov ax, 0x4300
        mov dx, f1
        int 0x21
        jc .t16b
        cmp cx, 1
        mov ax, cx
        jne .t16b
        mov ax, 0x4301
        xor cx, cx
        mov dx, f1
        int 0x21
        jc .t16b
        call ok
        jmp .t17
.t16b:
        call bad
.t17:
; T17: 5701h queda en el directorio al cerrar
        T 17
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t17b
        mov bx, ax
        mov ax, 0x5701
        mov cx, 0x1234
        mov dx, 0x2345
        int 0x21
        call close
        mov cx, 0
        DOSDX 0x4E, f1
        jc .t17b
        cmp word [dta+0x16], 0x1234
        jne .t17b
        cmp word [dta+0x18], 0x2345
        jne .t17b
        call ok
        jmp .t18
.t17b:
        call bad
.t18:
; T18: FCB: crear, escribir 3 registros de 64, cerrar, abrir, tamaño,
;      lectura aleatoria y en bloque, renombrar y borrar
        T 18
        mov dx, fcb2
        mov ah, 0x16
        int 0x21
        mov ah, 1
        test al, al
        jnz .t18b
        mov word [fcb2+0x0E], 64
        mov byte [fcb2+0x20], 0
        mov dx, pattern
        mov ah, 0x1A
        int 0x21
        mov cx, 3
.t18w:
        push cx
        mov dx, fcb2
        mov ah, 0x15
        int 0x21
        pop cx
        mov ah, 2
        test al, al
        jnz .t18b
        loop .t18w
        mov dx, fcb2
        mov ah, 0x10
        int 0x21
        mov dx, buf
        mov ah, 0x1A
        int 0x21
        mov dx, fcb2
        mov ah, 0x0F
        int 0x21
        mov ah, 3
        test al, al
        jnz .t18b
        mov word [fcb2+0x0E], 64
        cmp word [fcb2+0x10], 192
        mov ax, 0x0400
        jne .t18b
        mov dx, fcb2
        mov ah, 0x23
        int 0x21
        mov ax, [fcb2+0x21]
        cmp ax, 3
        jne .t18b
        ; aleatoria: registro 2 (el patrón se escribió tres veces: empieza en 0)
        mov dword [fcb2+0x21], 2
        mov byte [buf], 0xEE
        mov dx, fcb2
        mov ah, 0x21
        int 0x21
        mov ah, 5
        test al, al
        jnz .t18b
        cmp word [buf], 0x0100
        mov ax, 0x0600
        jne .t18b
        ; bloque: pedir 5 desde el 0 -> 3 leídos y AL = 1 (fin de fichero)
        mov dword [fcb2+0x21], 0
        mov cx, 5
        mov dx, fcb2
        mov ah, 0x27
        int 0x21
        mov ah, 7
        cmp cx, 3
        jne .t18b
        cmp al, 1
        jne .t18b
        cmp word [fcb2+0x21], 3
        mov ax, 0x0800
        jne .t18b
        mov dx, fcb2
        mov ah, 0x10
        int 0x21
        mov dx, fcb2r
        mov ah, 0x17
        int 0x21
        mov ah, 9
        test al, al
        jnz .t18b
        mov ax, 0x4300
        mov dx, n_fcbu
        int 0x21
        mov ax, 0x0A00
        jc .t18b
        mov dx, fcb3
        mov ah, 0x13
        int 0x21
        mov ah, 0x0B
        test al, al
        jnz .t18b
        mov ax, 0x4300
        mov dx, n_fcbu
        int 0x21
        mov ax, 0x0C00
        jnc .t18b
        call ok
        jmp .t19
.t18b:
        call bad
.t19:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
; T19: 45h y 46h
        T 19
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t19b
        mov [h1], ax
        mov bx, ax
        mov ah, 0x45
        int 0x21
        jc .t19b
        mov [h2], ax
        mov bx, [h1]
        call close
        mov bx, [h2]
        mov cx, 10
        mov dx, pattern
        mov ah, 0x40
        int 0x21
        jc .t19b
        cmp ax, 10
        jne .t19b
        mov bx, [h2]
        mov cx, 12
        mov ah, 0x46
        int 0x21
        jc .t19b
        mov bx, [h2]
        call close
        mov bx, 12
        mov ax, 0x4201
        xor cx, cx
        xor dx, dx
        int 0x21
        jc .t19b
        cmp ax, 10
        jne .t19b
        mov bx, 12
        call close
        mov bx, 12
        call close
        jnc .t19b                   ; ya cerrado: error 6
        cmp ax, 6
        jne .t19b
        call ok
        jmp .t20
.t19b:
        call bad
.t20:
; T20: rutas con «.» y «..»; por encima de la raíz -> 3
        T 20
        mov ax, 0x3D00
        mov dx, n_p1
        int 0x21
        jc .t20b
        mov bx, ax
        call close
        mov ax, 0x3D00
        mov dx, n_p2
        int 0x21
        jc .t20b
        mov bx, ax
        call close
        mov ax, 0x3D00
        mov dx, n_p3
        int 0x21
        jc .t20c
        mov bx, ax
        call close
        mov ax, 0xBBBB
        jmp .t20b
.t20c:
        cmp ax, 3
        jne .t20b
        ; 60h: nombre canónico
        push ds
        pop es
        mov si, n_p4
        mov di, buf
        mov ah, 0x60
        int 0x21
        jc .t20b
        mov si, buf+1
        mov di, n_p4r
        mov cx, 13
        repe cmpsb
        mov ax, 0xCCCC
        jne .t20b
        call ok
        jmp .t21
.t20b:
        call bad
.t21:
; T21: las minúsculas no ASCII del nombre se guardan en mayúsculas (CP437)
        T 21
        xor cx, cx
        DOSDX 0x3C, n_enye
        jc .t21b
        mov bx, ax
        call close
        mov cx, 0
        DOSDX 0x4E, n_enyeq
        jc .t21b
        mov al, [dta+0x1E+2]
        xor ah, ah
        cmp al, 0xA5
        jne .t21b
        DOSDX 0x41, n_enye
        jc .t21b
        call ok
        jmp .t22
.t21b:
        call bad
.t22:
; T22: IOCTL: 4400h de un fichero, 4408h
        T 22
        mov ax, 0x3D00
        mov dx, f1
        int 0x21
        jc .t22b
        mov bx, ax
        mov ax, 0x4400
        int 0x21
        pushf
        push dx
        call close
        pop ax
        popf
        jc .t22b
        test al, 0x80
        jnz .t22b
        mov ah, 0x19
        int 0x21
        inc al
        mov bl, al
        mov ax, 0x4408
        int 0x21
        jc .t22b
        cmp ax, 1
        ja .t22b
        mov ax, 0x4408
        mov bl, 26
        int 0x21
        jnc .t22b
        cmp ax, 0x0F
        jne .t22b
        call ok
        jmp .t23
.t22b:
        call bad
.t23:
; T23: 3Ch sobre uno que existe lo vacía; fichero de 0 octetos se lee bien
        T 23
        xor cx, cx
        DOSDX 0x3C, f1
        jc .t23b
        mov bx, ax
        mov cx, 10
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        call close
        pop ax
        popf
        jc .t23b
        test ax, ax
        jnz .t23b
        mov cx, 0
        DOSDX 0x4E, f1
        jc .t23b
        mov ax, [dta+0x1A]
        or ax, [dta+0x1C]
        jnz .t23b
        DOSDX 0x41, f1
        jc .t23b
        call ok
        jmp .t24
.t23b:
        call bad
.t24:
; T24: no se puede borrar la carpeta actual (10h) ni una con contenido (5)
        T 24
        DOSDX 0x39, n_gr
        jc .t24b
        mov dx, n_grf
        mov word [cnt], 7
        call gr_name
        mov dx, n_grf
        call mk_file
        DOSDX 0x3A, n_gr
        jnc .t24b
        cmp ax, 5
        jne .t24b
        DOSDX 0x3B, n_gr
        jc .t24b
        DOSDX 0x3A, n_grabs
        jnc .t24b
        cmp ax, 0x10
        jne .t24b
        DOSDX 0x3B, n_root
        DOSDX 0x41, n_grf
        jc .t24b
        DOSDX 0x3A, n_gr
        jc .t24b
        call ok
        jmp .t25
.t24b:
        push ax
        DOSDX 0x3B, n_root
        pop ax
        call bad
.t25:
; T25: bloqueos (5Ch): el de un fichero no estorba al de otro; el del mismo
;      fichero sí, también desde otra apertura
        T 25
        mov dx, f1
        call mk_file
        mov dx, n_w1
        call mk_file
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t25b
        mov [h1], ax
        mov ax, 0x3D02
        mov dx, n_w1
        int 0x21
        jc .t25b
        mov [h2], ax
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t25b
        mov [h3], ax
        mov bx, [h1]
        xor dx, dx
        call .lock
        mov ax, 1
        jc .t25b
        mov bx, [h2]                ; otro fichero, misma zona: se concede
        xor dx, dx
        call .lock
        jc .t25b
        mov bx, [h1]                ; mismo fichero, zona que se pisa: 21h
        mov dx, 5
        call .lock
        jnc .t25b
        cmp ax, 0x21
        jne .t25b
        mov bx, [h3]                ; otra apertura del mismo fichero: 21h
        xor dx, dx
        call .lock
        jnc .t25b
        cmp ax, 0x21
        jne .t25b
        mov bx, [h3]                ; zona aparte: se concede
        mov dx, 50
        call .lock
        jc .t25b
        mov bx, [h1]
        call close
        mov bx, [h3]                ; cerrada la primera, su zona queda libre
        xor dx, dx
        call .lock
        jc .t25b
        mov bx, [h2]
        call close
        mov bx, [h3]
        call close
        DOSDX 0x41, f1
        jc .t25b
        DOSDX 0x41, n_w1
        jc .t25b
        call ok
        jmp .fin
.t25b:
        call bad
        jmp .fin
; BX = handle, DX = inicio -> bloquear 10 octetos
.lock:
        mov ax, 0x5C00
        xor cx, cx
        xor si, si
        mov di, 10
        int 0x21
        ret
.fin:
; T26: un fichero abierto no se borra ni se renombra (20h); cerrado, sí
        T 26
        mov dx, f1
        call mk_file
        mov ax, 0x3D02
        mov dx, f1
        int 0x21
        jc .t26b
        mov [h1], ax
        DOSDX 0x41, f1
        jnc .t26b
        cmp ax, 0x20
        jne .t26b
        push ds
        pop es
        mov di, n_w2
        DOSDX 0x56, f1
        jnc .t26b
        cmp ax, 0x20
        jne .t26b
        mov dx, fcb1
        mov ah, 0x13
        int 0x21
        mov ah, 1
        cmp al, 0xFF
        jne .t26b
        mov bx, [h1]
        mov cx, 100
        mov dx, pattern
        mov ah, 0x40
        int 0x21
        mov bx, [h1]
        call close
        mov cx, 0
        DOSDX 0x4E, f1
        jc .t26b
        cmp word [dta+0x1A], 100
        jne .t26b
        DOSDX 0x41, f1
        jc .t26b
        call ok
        jmp .fin2
.t26b:
        call bad
.fin2:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; nombre GR\Fnn.DAT según [cnt]
gr_name:
        mov ax, [cnt]
        mov bl, 10
        div bl
        add ax, '00'
        mov [n_grf+4], ax
        ret

; ---------------------------------------------------------------------------
; FSTEST L: 10 FCB distintos abiertos y sin cerrar; termina
mode_leak:
        mov cx, 10
        mov di, buf
.l:
        push cx
        mov si, fcbk
        mov cx, 37
        push di
        rep movsb
        pop dx
        push di
        mov ah, 0x0F
        int 0x21
        pop di
        pop cx
        test al, al
        jnz .b
        loop .l
        mov dx, m_leak
        call say
        mov ax, 0x4C00
        int 0x21
.b:
        mov dx, m_leakb
        call say
        mov ax, 0x4C01
        int 0x21

; ---------------------------------------------------------------------------
; FSTEST F: disco lleno y raíz llena
mode_full:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov di, buf
        mov al, 0x5A
        mov cx, 4096
        rep stosb
; T31: escribir hasta llenar: la última escritura es corta y sin error
        T 31
        xor cx, cx
        DOSDX 0x3C, n_big
        jc .t31b
        mov [h1], ax
.t31w:
        mov bx, [h1]
        mov cx, 4096
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .t31b
        cmp ax, 4096
        je .t31w
        mov bx, [h1]
        mov cx, 4096
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .t31b
        test ax, ax
        jnz .t31b
        mov bx, [h1]
        call close
        mov ah, 0x36
        xor dl, dl
        int 0x21
        mov ax, bx
        test bx, bx
        jnz .t31b
        call ok
        jmp .t32
.t31b:
        call bad
.t32:
; T32: con el disco lleno: crear vale (entrada), escribir da 0 y alargar
;      con una escritura de 0 octetos no cambia el tamaño
        T 32
        xor cx, cx
        DOSDX 0x3C, n_z
        jc .t32b
        mov [h1], ax
        mov bx, ax
        mov cx, 10
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .t32b
        test ax, ax
        jnz .t32b
        mov bx, [h1]
        mov ax, 0x4200
        mov cx, 1
        mov dx, 0x86A0
        int 0x21
        xor cx, cx
        mov ah, 0x40
        int 0x21
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        or ax, dx
        jnz .t32b
        ; y con escritura de datos más allá del final: tampoco
        mov ax, 0x4200
        mov cx, 1
        mov dx, 0x86A0
        int 0x21
        mov cx, 10
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .t32b
        test ax, ax
        jnz .t32b
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        or ax, dx
        jnz .t32b
        mov bx, [h1]
        call close
        DOSDX 0x39, n_gr
        mov ax, 0xDDDD
        jnc .t32b
        call ok
        jmp .t33
.t32b:
        call bad
        mov bx, [h1]
        call close
.t33:
; T33: al borrar el grande vuelve todo el espacio
        T 33
        DOSDX 0x41, n_big
        jc .t33b
        DOSDX 0x41, n_z
        jc .t33b
        mov ah, 0x36
        xor dl, dl
        int 0x21
        cmp bx, dx
        mov ax, bx
        jne .t33b
        call ok
        jmp .t34
.t33b:
        call bad
.t34:
; T34: raíz llena: crear ficheros hasta el error 5; MD también da 5; en una
;      subcarpeta ya creada sí se puede; luego se borra todo
        T 34
        DOSDX 0x39, n_gr
        jc .t34b
        mov word [cnt], 0
.t34c:
        call r_name
        xor cx, cx
        DOSDX 0x3C, n_rf
        jc .t34full
        mov bx, ax
        call close
        inc word [cnt]
        cmp word [cnt], 600
        jb .t34c
        mov ax, 0xAAAA
        jmp .t34b
.t34full:
        cmp ax, 5
        jne .t34b
        mov ax, [cnt]
        cmp ax, 60
        jb .t34b
        DOSDX 0x39, n_m1
        jnc .t34b
        cmp ax, 5
        jne .t34b
        mov word [cnt], 3
        call gr_name
        mov dx, n_grf
        call mk_file
        mov ax, 0xBBBB
        jc .t34b
        DOSDX 0x41, n_grf
        jc .t34b
        DOSDX 0x3A, n_gr
        jc .t34b
        ; borrar todos
        mov cx, 0
        DOSDX 0x4E, n_rall
        jc .t34b
.t34d:
        DOSDX 0x41, dta+0x1E
        jc .t34b
        mov ah, 0x4F
        int 0x21
        jnc .t34d
        mov ah, 0x36
        xor dl, dl
        int 0x21
        cmp bx, dx
        mov ax, bx
        jne .t34b
        call ok
        jmp .t35
.t34b:
        call bad
.t35:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; ---------------------------------------------------------------------------
; FSTEST C: en un CD nada se borra ni se cambia, y se dice
mode_cd:
        T 41
        mov dx, fcbz
        mov ah, 0x13
        int 0x21
        mov ah, 1
        cmp al, 0xFF
        jne .b
        mov dx, fcbzr
        mov ah, 0x17
        int 0x21
        mov ah, 2
        cmp al, 0xFF
        jne .b
        mov ax, 0x4301
        mov cx, 0x20
        mov dx, n_zf
        int 0x21
        jnc .b
        cmp ax, 5
        jne .b
        DOSDX 0x41, n_zf
        jnc .b
        cmp ax, 5
        jne .b
        xor cx, cx
        DOSDX 0x3C, n_zf
        jnc .b
        cmp ax, 5
        jne .b
        mov ax, 0x3D00
        mov dx, n_zf
        int 0x21
        jc .b
        mov bx, ax
        mov cx, 6
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        push ax
        call close
        pop ax
        cmp ax, 6
        jne .b
        cmp dword [buf], 'ulti'
        jne .b
        call ok
        jmp .f
.b:
        call bad
.f:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; ---------------------------------------------------------------------------
; FSTEST S
mode_stress:
        T 50
        mov word [iter], 0
        ; los ocho, vacíos
        mov byte [cur_f], 0
.mk:
        call s_name
        xor cx, cx
        DOSDX 0x3C, n_s
        jc near .bad
        mov bx, ax
        call close
        inc byte [cur_f]
        cmp byte [cur_f], 8
        jb .mk
.loop:
        call rnd
        mov bx, ax
        and al, 7
        mov [cur_f], al
        call s_name
        mov ax, bx
        shr ax, 3
        and ax, 7
        cmp al, 5
        jb near .write
        je near .trunc
        cmp al, 7
        je .ver
        test bh, 0x60
        jz near .recreate
        jmp .rename
.ver:
        call verify
        jc near .bad
.next:
        inc word [iter]
        cmp word [iter], 600
        jb .loop
        ; al final, todos
        mov byte [cur_f], 0
.vall:
        call s_name
        call verify
        jc near .bad
        inc byte [cur_f]
        cmp byte [cur_f], 8
        jb .vall
        call ok
        jmp .fin
.bad:
        mov ax, [iter]
        call bad
.fin:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; escribir LEN octetos en OFF (OFF <= tamaño; como mucho hasta 20000)
.write:
        call f_size                 ; BX -> tamaño del fichero actual
        call rnd
        xor dx, dx
        mov cx, [bx]
        inc cx
        div cx
        mov [s_off], dx
        call rnd
        xor dx, dx
        mov cx, 4000
        div cx
        inc dx
        mov ax, [s_off]
        add ax, dx
        cmp ax, 20000
        jbe .wl
        mov dx, 20000
        sub dx, [s_off]
        jz near .next
.wl:
        mov [s_len], dx
        mov ax, 0x3D02
        mov dx, n_s
        int 0x21
        jc .bad
        mov [h1], ax
        mov bx, ax
        mov ax, 0x4200
        xor cx, cx
        mov dx, [s_off]
        int 0x21
        mov bx, [s_off]
        mov cx, [s_len]
        call fill
        mov bx, [h1]
        mov cx, [s_len]
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .bad
        cmp ax, [s_len]
        jne .bad
        mov bx, [h1]
        call close
        call f_size
        mov ax, [s_off]
        add ax, [s_len]
        cmp ax, [bx]
        jbe near .next
        mov [bx], ax
        jmp .next
; recortar a un tamaño menor o igual
.trunc:
        call f_size
        call rnd
        xor dx, dx
        mov cx, [bx]
        shr cx, 2
        inc cx
        div cx                      ; se quita hasta la cuarta parte
        neg dx
        add dx, [bx]
        mov [bx], dx
        mov [s_off], dx
        mov ax, 0x3D02
        mov dx, n_s
        int 0x21
        jc near .bad
        mov bx, ax
        mov ax, 0x4200
        xor cx, cx
        mov dx, [s_off]
        int 0x21
        xor cx, cx
        mov ah, 0x40
        int 0x21
        call close
        jmp .next
; borrar y volver a crear vacío
.recreate:
        DOSDX 0x41, n_s
        jc near .bad
        xor cx, cx
        DOSDX 0x3C, n_s
        jc near .bad
        mov bx, ax
        call close
        call f_size
        mov word [bx], 0
        jmp .next
; renombrar a .TMP y de vuelta
.rename:
        mov al, [n_s+1]
        mov [n_st+1], al
        push ds
        pop es
        mov di, n_st
        DOSDX 0x56, n_s
        jc near .bad
        mov di, n_s
        DOSDX 0x56, n_st
        jc near .bad
        jmp .next

; nombre Sn.DAT del fichero actual
s_name:
        mov al, [cur_f]
        add al, '0'
        mov [n_s+1], al
        ret
; -> BX = dirección del tamaño del fichero actual
f_size:
        movzx bx, byte [cur_f]
        shl bx, 1
        add bx, sizes
        ret
; -> AX al azar (siempre la misma serie)
rnd:
        push dx
        mov ax, [seed]
        mov dx, 25173
        mul dx
        add ax, 13849
        mov [seed], ax
        rol ax, 5
        pop dx
        ret
; BX = posición inicial, CX = cuántos -> buf con el contenido que toca
fill:
        push cx
        push di
        mov di, buf
        mov al, [cur_f]
        mov ah, 31
        mul ah
        mov dl, al
.l:
        mov al, bl
        xor al, bh
        add al, dl
        stosb
        inc bx
        loop .l
        pop di
        pop cx
        ret
; el fichero actual mide lo apuntado y contiene lo que toca -> CF si no
verify:
        mov ax, 0x3D00
        mov dx, n_s
        int 0x21
        jc .r
        mov [h1], ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        test dx, dx
        jnz .b
        call f_size
        cmp ax, [bx]
        jne .b
        mov bx, [h1]
        mov ax, 0x4200
        xor cx, cx
        xor dx, dx
        int 0x21
        mov word [s_off], 0
.rd:
        call rnd
        xor dx, dx
        mov cx, 700
        div cx
        inc dx
        mov cx, dx
        mov bx, [h1]
        mov dx, buf2
        mov ah, 0x3F
        int 0x21
        jc .b
        test ax, ax
        jz .eof
        mov [s_len], ax
        mov bx, [s_off]
        mov cx, ax
        call fill
        mov si, buf
        mov di, buf2
        mov cx, [s_len]
        repe cmpsb
        jne .b
        mov ax, [s_len]
        add [s_off], ax
        jmp .rd
.eof:
        call f_size
        mov ax, [s_off]
        cmp ax, [bx]
        jne .b
        mov bx, [h1]
        call close
        clc
        ret
.b:
        mov bx, [h1]
        call close
        stc
.r:
        ret

; ---------------------------------------------------------------------------
; FSTEST P: disco protegido contra escritura
mode_prot:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov ah, 0x36
        xor dl, dl
        int 0x21
        mov [cnt], bx
; T70: MD
        T 70
        DOSDX 0x39, n_gr
        jnc .b70
        cmp ax, 0x13
        jne .b70
        mov ax, 0x4300
        mov dx, n_gr
        int 0x21
        mov ax, 0xEEEE
        jnc .b70
        call ok
        jmp .t71
.b70:
        call bad
.t71:
; T71: borrar, renombrar, atributos, RD
        T 71
        DOSDX 0x41, n_x
        jnc .b71
        cmp ax, 0x13
        jne .b71
        push ds
        pop es
        mov di, n_w2
        DOSDX 0x56, n_x
        mov ax, 2
        jnc .b71
        mov ax, 0x4301
        mov cx, 1
        mov dx, n_x
        int 0x21
        mov ax, 3
        jnc .b71
        DOSDX 0x3A, n_d1
        mov ax, 4
        jnc .b71
        call ok
        jmp .t72
.b71:
        call bad
.t72:
; T72: escribir en un fichero que existe; crear uno nuevo
        T 72
        mov ax, 0x3D02
        mov dx, n_x
        int 0x21
        jc .b72
        mov [h1], ax
        mov bx, ax
        mov cx, 10
        mov dx, n_x
        mov ah, 0x40
        int 0x21
        jnc .b72
        cmp ax, 0x13
        jne .b72
        mov bx, [h1]
        call close
        xor cx, cx
        DOSDX 0x3C, n_w1
        jc .t72c
        mov bx, ax
        call close
        mov ax, 0xEEEE
        jmp .b72
.t72c:
        call ok
        jmp .t73
.b72:
        call bad
.t73:
; T73: el disco se ve como estaba: X.TXT con su tamaño y atributos, D1, y
;      los mismos clústeres libres
        T 73
        mov cx, 0
        DOSDX 0x4E, n_x
        jc .b73
        cmp word [dta+0x1A], 100
        jne .b73
        test byte [dta+0x15], 1
        jnz .b73
        mov ax, 0x4300
        mov dx, n_d1
        int 0x21
        jc .b73
        mov ax, 0x4300
        mov dx, n_w2
        int 0x21
        mov ax, 0xEEEE
        jnc .b73
        mov ah, 0x36
        xor dl, dl
        int 0x21
        cmp bx, [cnt]
        mov ax, bx
        jne .b73
        call ok
        jmp .f73
.b73:
        call bad
.f73:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; ---------------------------------------------------------------------------
; FSTEST E: leer con el disco quitado
mode_eject:
        T 60
        mov ax, 0x3D00
        mov dx, n_x
        int 0x21
        jc .b60
        mov [h1], ax
        mov dx, m_quite
        call say
        mov ah, 0x08
        int 0x21
        mov bx, [h1]
        mov cx, 10
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jnc .b60                    ; (sin error sería un falso fin de fichero)
        cmp ax, 0x15
        je .o60
        cmp ax, 0x1E
        jne .b60
.o60:
        call ok
        jmp .t61
.b60:
        call bad
.t61:
        T 61
        mov dx, m_ponga
        call say
        mov ah, 0x08
        int 0x21
        mov bx, [h1]
        mov cx, 10
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .b61
        cmp ax, 10
        jne .b61
        cmp word [buf], 'XX'
        jne .b61
        mov bx, [h1]
        call close
        call ok
        jmp .f61
.b61:
        call bad
.f61:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; ---------------------------------------------------------------------------
; FSTEST X: cambio de disco con ficheros abiertos
mode_xchg:
        T 80
        mov ax, 0x3D00
        mov dx, n_x
        int 0x21
        jc .b
        mov [h2], ax
        xor cx, cx
        DOSDX 0x3C, n_zz
        jc .b
        mov [h1], ax
        mov bx, ax
        mov cx, 600
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .b
        mov dx, m_cambie
        call say
        mov ah, 0x08
        int 0x21
        mov bx, [h1]
        mov cx, 600
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jnc .b                      ; habría escrito en el otro disco
        cmp ax, 0x22
        jne .b
        mov bx, [h2]
        mov cx, 10
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jnc .b                      ; habría leído del otro disco
        mov bx, [h1]
        call close
        mov bx, [h2]
        call close
        call ok
        jmp .f
.b:
        call bad
.f:
        mov dx, m_fin
        call say
        mov ax, 0x4C00
        int 0x21

; nombre Rnnn.TMP según [cnt]
r_name:
        mov ax, [cnt]
        mov bl, 100
        div bl
        add al, '0'
        mov [n_rf+1], al
        mov al, ah
        xor ah, ah
        mov bl, 10
        div bl
        add ax, '00'
        mov [n_rf+2], ax
        ret

; ---------------------------------------------------------------------------
m_t         db "T00 $"
m_ok        db "OK", 13, 10, "$"
m_bad       db "FALLO ax="
m_bad_ax    db "0000", 13, 10, "$"
m_fin       db "FSTEST FIN", 13, 10, "$"
m_leak      db "FSTEST L: 10 FCB abiertos", 13, 10, "$"
m_leakb     db "FSTEST L: FALLO al abrir", 13, 10, "$"
f1          db "F1.DAT", 0
fcb1        db 0, "F1      DAT"
            times 25 db 0
fcb2        db 0, "FCBT    DAT"
            times 25 db 0
; renombrar: nombre viejo en +1, nuevo en +11h
fcb2r       db 0, "FCBT    DAT"
            times 5 db 0
            db "FCBU    DAT"
            times 9 db 0
fcb3        db 0, "FCBU    DAT"
            times 25 db 0
fcbz        db 0, "ZFINAL  TXT"
            times 25 db 0
fcbzr       db 0, "ZFINAL  TXT"
            times 5 db 0
            db "ZOTRO   TXT"
            times 9 db 0
n_zf        db "ZFINAL.TXT", 0
fcbk        db 0, "KERNEL  SYS"
            times 25 db 0
xfcb_all    db 0xFF, 0, 0, 0, 0, 0, 0x08, 0, "???????????"
            times 25 db 0
xfcb_lab    db 0xFF, 0, 0, 0, 0, 0, 0x08, 0, "ETIQTEST   "
            times 25 db 0
n_nomatch   db "NOEXISTE.*", 0
n_nodir     db "NODIR\*.*", 0
n_sub       db "SUBX", 0
n_root      db "\", 0
n_rootall   db "\*.*", 0
n_blank     db ".X", 0
n_abc       db "A.B.C", 0
n_aq        db "A.*", 0
n_ab        db "A.B", 0
n_w1        db "W1.TMP", 0
n_w2        db "W2.TMP", 0
n_wq        db "W?.TMP", 0
n_parse     db "  A:foo.txt ", 0
n_docs      db "\DOCS", 0
n_nul       db "NUL", 0
n_docsnul   db "\DOCS\NUL", 0
n_nadanul   db "\NADA\NUL", 0
n_gr        db "GR", 0
n_grabs     db "\GR", 0
n_grf       db "GR\F00.DAT", 0
n_grall     db "GR\*.*", 0
n_m1        db "M1", 0
n_m1f       db "M1\F9.DAT", 0
tmppath     db "\"
            times 16 db 0
n_p1        db "\DOCS\..\README.TXT", 0
n_p2        db "\DOCS\.\INFO.TXT", 0
n_p3        db "\..\README.TXT", 0
n_p4        db "docs/../readme.txt", 0
n_p4r       db ":\README.TXT", 0, 0
n_enye      db "ni", 0xA4, "o.txt", 0
n_enyeq     db "NI?O.TXT", 0
n_fcbu      db "FCBU.DAT", 0
n_big       db "BIG.DAT", 0
n_z         db "Z.DAT", 0
n_rf        db "R000.TMP", 0
n_x         db "X.TXT", 0
n_zz        db "ZZ.DAT", 0
m_cambie    db "FSTEST X: cambie el disco y pulse una tecla", 13, 10, "$"
n_d1        db "D1", 0
m_quite     db "FSTEST E: quite el disco y pulse una tecla", 13, 10, "$"
m_ponga     db "FSTEST E: ponga el disco y pulse una tecla", 13, 10, "$"
n_s         db "S0.DAT", 0
n_st        db "S0.TMP", 0
seed        dw 12345
iter        dw 0
s_off       dw 0
s_len       dw 0
cur_f       db 0
sizes       times 8 dw 0
n_rall      db "R*.TMP", 0
tnum        dw 0
cnt         dw 0
h1          dw 0
h2          dw 0
h3          dw 0
pfcb        times 40 db 0
lab1        times 12 db 0
lab2        times 12 db 0
idbuf       times 32 db 0
dta         times 128 db 0
pattern     times 256 db 0
buf         times 4096 db 0
buf2        times 1024 db 0

crit_fail:
        mov al, 3
        iret
