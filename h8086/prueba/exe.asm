; =============================================================================
;  EXE.EXE - prueba de los programas .EXE, la memoria y lanzar otro programa
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un .EXE hecho a mano (cabecera «MZ» y tabla de reubicación escritas aquí),
;  con el código, los datos y la pila en segmentos distintos. Comprueba que:
;    - las dos direcciones de su tabla se reubican (carga DS con el segmento
;      de los datos y hace una llamada lejana);
;    - puede encoger su bloque (4Ah), pedir otro (48h), soltarlo (49h), y
;      pedir más de lo que hay falla diciendo cuánto queda;
;    - encuentra su propia ruta detrás de su entorno;
;    - lanza HOLA.COM con parámetros (4Bh), espera, y recoge su código de
;      salida (4Dh), que es 7.
; =============================================================================
        cpu 8086
CAB         equ 48                  ; la cabecera: tres párrafos
%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro
        ; ---- la cabecera ----
        db "MZ"
        dw (fin - $$) % 512         ; octetos de la última página
        dw (fin - $$ + 511) / 512   ; páginas
        dw 2                        ; direcciones que reubicar
        dw CAB / 16                 ; párrafos de la cabecera
        dw 0x40                     ; memoria de más que necesita (la pila)
        dw 0xFFFF                   ; y la que quiere: toda
        dw PILA_SEG                 ; SS
        dw 0x200                    ; SP
        dw 0
        dw inicio - codigo          ; IP
        dw 0                        ; CS
        dw 0x1C                     ; dónde está la tabla
        dw 0
        dw r1 - codigo, 0           ; la tabla: desplazamiento y segmento
        dw r2 - codigo, 0
        times CAB - ($ - $$) db 0

; ---- el código (segmento 0 del programa) ----
codigo:
%define D(x) (x - datos)
inicio:
        mov bx, ds                  ; al entrar, DS = ES = el prefijo
        mov ax, DATOS_SEG
r1      equ $ - 2
        mov ds, ax
        mov [D(psp)], bx
        mov dx, D(m_datos)
        DOS 9
        call LEJOS_SEG:lejos - codigo2
r2      equ $ - 2
        ; ---- la memoria ----
        mov es, [D(psp)]            ; encoger: lo cargado más la pila
        mov bx, PILA_SEG + 0x30 + 0x10
        DOS 0x4A
        jc mal
        mov bx, 0x100
        DOS 0x48
        jc mal
        mov es, ax
        mov word [es:0], 0x1234     ; (se puede escribir en él)
        DOS 0x49
        jc mal
        mov bx, 0xFFFF              ; todo no cabe: error 8 y cuánto queda
        DOS 0x48
        jnc mal
        cmp ax, 8
        jne mal
        cmp bx, 0x1000              ; (en una máquina de 640 K, más de 64 K)
        jb mal
        mov dx, D(m_mem)
        DOS 9
        ; ---- mi ruta, detrás del entorno ----
        mov es, [D(psp)]
        mov es, [es:0x2C]
        xor di, di
.e:
        cmp word [es:di], 0
        je .ef
        inc di
        jmp .e
.ef:
        add di, 4
        mov dx, D(m_soy)
        DOS 9
.p:
        mov dl, [es:di]
        test dl, dl
        jz .pf
        DOS 2
        inc di
        jmp .p
.pf:
        ; ---- lanzar HOLA.COM y esperar ----
        mov dx, D(m_lanza)
        DOS 9
        mov [D(bloque)+4], ds
        push ds
        pop es
        mov bx, D(bloque)
        mov dx, D(n_hola)
        mov ax, 0x4B00
        int 0x21
        jc mal
        mov ax, DATOS_SEG           ; (al volver solo se conserva la pila)
r3      equ 0                       ; (esta no se reubica: se toma de SS)
        mov ax, ss
        sub ax, PILA_SEG - DATOS_SEG
        mov ds, ax
        DOS 0x4D
        add al, '0'
        mov [D(m_hijo_n)], al
        mov dx, D(m_hijo)
        DOS 9
        mov ax, 0x4C00
        int 0x21
mal:
        mov ax, ss
        sub ax, PILA_SEG - DATOS_SEG
        mov ds, ax
        mov dx, D(m_mal)
        DOS 9
        mov ax, 0x4C01
        int 0x21
        align 16
; ---- otro segmento de código, para la llamada lejana ----
codigo2:
LEJOS_SEG   equ (codigo2 - codigo) / 16
lejos:
        mov dx, D(m_lejos)
        DOS 9
        retf
        align 16
; ---- los datos ----
datos:
DATOS_SEG   equ (datos - codigo) / 16
psp         dw 0
bloque      dw 0                    ; el entorno: una copia del mío
            dw D(cola), 0           ; lo que va detrás del nombre
n_hola      db "HOLA.COM", 0
cola        db 5, " hijo", 13
m_datos     db "EXE: datos bien", 13, 10, "$"
m_lejos     db "EXE: llamada lejana bien", 13, 10, "$"
m_mem       db "EXE: memoria bien", 13, 10, "$"
m_soy       db "EXE: soy $"
m_lanza     db 13, 10, "EXE: lanzo HOLA.COM", 13, 10, "$"
m_hijo      db "EXE: el hijo acaba con "
m_hijo_n    db "?", 13, 10, "$"
m_mal       db "EXE: MAL", 13, 10, "$"
        align 16
fin:
PILA_SEG    equ (fin - codigo) / 16
