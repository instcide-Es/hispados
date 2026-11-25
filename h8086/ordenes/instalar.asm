; =============================================================================
;  INSTALAR.COM - pone Hispa-8086 en el disco duro, paso a paso
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       INSTALAR        (desde el disquete del sistema, en A:)
;
;  Va diciendo lo que hace, en llano, y pregunta antes de borrar nada:
;    1. si el disco duro no tiene unidad C:, la crea (PARTIR CREAR) y le da
;       formato con el sistema (FORMATEA C: /S); si ya la tiene, pregunta si
;       se le da formato o si se instala encima (entonces, SISTEMA C:);
;    2. copia a C:\ las órdenes del disquete;
;    3. pregunta si se instalan también la Oficina (PLUMA y CUENTAS, en
;       C:\OFICINA) y los Juegos (DERRIBO y MARCIANADA, en C:\JUEGOS), y pide
;       su disquete en A: cuando toca (con una sola disquetera, se cambia);
;    4. pregunta si al arrancar sale el menú de las aplicaciones (MENU), y
;       escribe C:\AUTOEXEC.BAT: el teclado español, la ruta y, si se quiere,
;       el menú.
;  No hace falta reiniciar entre PARTIR y FORMATEA: el núcleo ve la partición
;  nueva en cuanto PARTIR se lo dice (INT 21h/E8h). Devuelve 0 si ha acabado,
;  1 si se ha dejado y 2 si algo ha fallado. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

BUF_TAM     equ 8192

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov sp, pila_fin
        mov bx, (pila_fin - $$ + 0x100 + 15) / 16
        DOS 0x4A                    ; lo demás, para los programas que se lanzan
        mov dx, dta
        DOS 0x1A
        mov si, m_hola
        call pon_cad
        mov ah, 0x08                ; ¿hay disco duro?
        mov dl, 0x80
        int 0x13
        jc .sin_duro
        test dl, dl
        jnz .duro
.sin_duro:
        mov si, m_sin_duro
        jmp fin_mal
.duro:
        call hay_c
        jnc .ya_c
        mov si, m_nuevo             ; sin C:: partición y formato
        call confirma
        jne dejar
        mov dx, t_partir
        mov si, c_partir
        call lanza
        jnc .formatea
        mov si, m_e_partir
        jmp fin_mal
.ya_c:
        mov si, m_ya_c              ; con C:: ¿formato, o encima?
        call pon_cad
        DOS 0x08
        call mayus
        push ax
        call pon_nl
        pop ax
        cmp al, 'S'
        je .formatea
        cmp al, 'N'
        jne dejar
        mov dx, t_sistema
        mov si, c_sistema
        call lanza
        jnc .copia
        mov si, m_e_sistema
        jmp fin_mal
.formatea:
        mov dx, t_formatea
        mov si, c_formatea
        call lanza
        jc .mal_formato
        call hay_c
        jnc .copia
.mal_formato:
        mov si, m_e_formato
        jmp fin_mal
.copia:
        mov si, m_copia
        call pon_cad
        mov word [d_fin], t_c
        call copia_todo
        jc fin_mal
        ; la Oficina y los Juegos
        mov si, m_oficina
        mov di, t_pluma
        mov bx, t_d_oficina
        call opcional
        jc fin_mal
        mov si, m_juegos
        mov di, t_derribo
        mov bx, t_d_juegos
        call opcional
        jc fin_mal
        ; el AUTOEXEC.BAT de C:
        mov si, m_menu
        call confirma
        mov si, t_auto
        mov cx, t_auto_tam
        jne .auto
        add cx, t_menu_tam          ; (con el menú: una línea más)
.auto:
        mov dx, t_c_auto
        call escribe_arch
        jc fin_mal
        mov si, m_hecho
        call pon_cad
        mov ax, 0x4C00
        int 0x21
dejar:
        mov si, m_nada
        call pon_cad
        mov ax, 0x4C01
        int 0x21
fin_mal:
        call pon_cad
        mov si, m_mal
        call pon_cad
        mov ax, 0x4C02
        int 0x21

; ¿existe ya C:? (INT 21h/36h) CF=1 si no
hay_c:
        mov ah, 0x36
        mov dl, 3
        int 0x21
        cmp ax, 0xFFFF
        cmc
        ret

; lanza el programa DX con la cola SI (texto acabado en 0) y espera a que
; acabe. CF=1 si no se ha podido o ha acabado con un código que no es 0
lanza:
        mov di, cola + 1
        xor cx, cx
.c:
        lodsb
        test al, al
        jz .f
        stosb
        inc cx
        jmp .c
.f:
        mov byte [di], 13
        mov [cola], cl
        mov word [bloque], 0
        mov word [bloque+2], cola
        mov [bloque+4], cs
        push cs
        pop es
        mov bx, bloque
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        jc .x
        DOS 0x4D
        cmp al, 1                   ; (CF=1 si AL no es 0)
        cmc
.x:
        ret

; ---- lo opcional: pregunta SI y, si es que sí, pide el disquete que tiene el
; archivo DI y lo copia entero a la carpeta BX de C:. CF=1 si falla ----
opcional:
        mov [op_arch], di
        mov [op_dir], bx
        call confirma
        jne .x0
.pide:
        mov si, m_pon
        call pon_cad
        mov si, [op_dir]            ; «Oficina» o «Juegos»: el nombre de la
        add si, 3                   ; carpeta
        call pon_cad
        mov si, m_pon2
        call pon_cad
        DOS 0x08
        push ax
        call pon_nl
        pop ax
        cmp al, 27
        je .x0
        mov dx, [op_arch]           ; ¿es ese disquete?
        xor cx, cx
        DOS 0x4E
        jnc .es
        mov si, m_no_es
        call pon_cad
        jmp .pide
.es:
        mov dx, [op_dir]
        DOS 0x39                    ; la carpeta (si ya está, no pasa nada)
        mov si, [op_dir]
        mov di, d_dir
        call copia_txt
        mov ax, '\'
        stosw
        mov word [d_fin], d_dir
        call copia_todo
        ret
.x0:
        clc
        ret

; ---- copia A:\*.* (menos el sistema y el AUTOEXEC.BAT) a la carpeta de
; [d_fin] (un texto acabado en «\»). CF=1 (SI = el mensaje) si falla ----
copia_todo:
        mov dx, t_todos
        xor cx, cx
        DOS 0x4E
        jc .x0
.uno:
        mov si, dta + 30
        mov di, no_copiar           ; ¿de los que no se copian?
.n:
        cmp byte [di], 0
        je .va
        push si
        push di
        call igual
        pop di
        pop si
        je .sig
.n1:
        inc di
        cmp byte [di-1], 0
        jne .n1
        jmp .n
.va:
        mov si, t_a                 ; el origen, A:\NOMBRE
        mov di, o_nom
        call copia_txt
        mov si, dta + 30
        call copia_txt
        mov si, [d_fin]             ; el destino
        mov di, d_nom
        call copia_txt
        mov si, dta + 30
        call copia_txt
        mov si, m_sangria
        call pon_cad
        mov si, d_nom
        call pon_cad
        call pon_nl
        call copia_arch
        jc .x
.sig:
        DOS 0x4F
        jnc .uno
.x0:
        clc
.x:
        ret
; ¿el texto de SI es el de DI? ZF=1 si lo es
igual:
        lodsb
        cmp al, [di]
        jne .x
        inc di
        test al, al
        jnz igual
.x:
        ret
; copia el texto de SI a DI (con su 0); DI queda en el 0
copia_txt:
        lodsb
        stosb
        test al, al
        jnz copia_txt
        dec di
        ret

; copia el archivo o_nom a d_nom, con su fecha. CF=1 (SI = el mensaje) si no
copia_arch:
        mov dx, o_nom
        mov ax, 0x3D00
        int 0x21
        jc .no_lee
        mov [h1], ax
        mov dx, d_nom
        xor cx, cx
        DOS 0x3C
        jnc .creado
        mov bx, [h1]
        DOS 0x3E
        jmp short .no_esc
.creado:
        mov [h2], ax
.trozo:
        mov bx, [h1]
        mov cx, BUF_TAM
        mov dx, buf
        DOS 0x3F
        jc .mal
        test ax, ax
        jz .hecho
        mov cx, ax
        mov bx, [h2]
        DOS 0x40
        jc .mal
        cmp ax, cx
        je .trozo
.mal:
        call .cierra
.no_esc:
        mov si, m_e_copia
        stc
        ret
.no_lee:
        mov si, m_e_leer
        stc
        ret
.hecho:
        mov bx, [h1]
        mov ax, 0x5700
        int 0x21
        mov bx, [h2]
        mov ax, 0x5701
        int 0x21
.cierra:
        mov bx, [h1]
        DOS 0x3E
        mov bx, [h2]
        DOS 0x3E
        clc
        ret

; escribe CX octetos de SI en el archivo DX (nuevo). CF=1 (SI = el mensaje)
escribe_arch:
        push si
        push cx
        xor cx, cx
        DOS 0x3C
        pop cx
        pop dx
        jc .mal
        mov bx, ax
        DOS 0x40
        pushf
        DOS 0x3E
        popf
        jnc .x
.mal:
        mov si, m_e_copia
        stc
.x:
        ret

m_hola      db 10, "INSTALAR: Hispa-8086 en el disco duro.", 10
            db "Se va paso a paso, y se pregunta antes de borrar nada.", 10, 10, 0
m_sin_duro  db "Este equipo no tiene disco duro: no hay dónde instalar.", 10, 0
m_nuevo     db "El disco duro no tiene la unidad C:. Se le va a crear una partición y se", 10
            db "le va a dar formato: se borrará TODO lo que tenga el disco duro.", 10
            db "¿Seguir? (S: sí; otra tecla: dejarlo) ", 0
m_ya_c      db "El disco duro ya tiene la unidad C:. ¿Darle formato antes de instalar?", 10
            db "  S: sí, formatearla (se borrará TODO lo que tenga C:)", 10
            db "  N: no, instalar encima (lo que tiene se queda)", 10
            db "  otra tecla: dejarlo ", 0
m_copia     db 10, "Copiando el sistema y sus órdenes a C:...", 10, 0
m_sangria   db "  ", 0
m_oficina   db 10, "¿Instalar también la Oficina (Pluma y Cuentas)? (S/N) ", 0
m_juegos    db 10, "¿Instalar también los Juegos (Derribo y Marcianada)? (S/N) ", 0
m_pon       db "Ponga en A: el disquete de ", 0
m_pon2      db " y pulse una tecla (Esc: no instalarlo)... ", 0
m_no_es     db "No es ese disquete.", 10, 0
m_menu      db 10, "¿Quiere que al arrancar salga el menú de las aplicaciones? (S/N) ", 0
m_hecho     db 10, "Hispa-8086 está instalado en el disco duro. Saque el disquete y", 10
            db "reinicie el equipo: arrancará de C:, con el teclado español.", 10, 0
m_nada      db "No se ha tocado nada.", 10, 0
m_mal       db "La instalación no ha acabado.", 10, 0
m_e_partir  db "No se ha podido crear la partición (¿tiene otras? PARTIR las enseña).", 10, 0
m_e_formato db "No se ha podido dar formato a C:.", 10, 0
m_e_sistema db "No se ha podido poner el sistema en C:.", 10, 0
m_e_copia   db "No se ha podido escribir en C: (¿está lleno?).", 10, 0
m_e_leer    db "No se ha podido leer el disquete.", 10, 0
t_partir    db "A:\PARTIR.COM", 0
c_partir    db " CREAR /A", 0
t_formatea  db "A:\FORMATEA.COM", 0
c_formatea  db " C: /S /A /V:HISPA-8086", 0
t_sistema   db "A:\SISTEMA.COM", 0
c_sistema   db " C: /A", 0
t_todos     db "A:\*.*", 0
t_a         db "A:\", 0
t_c         db "C:\", 0
t_pluma     db "A:\PLUMA.COM", 0
t_derribo   db "A:\DERRIBO.COM", 0
t_d_oficina db "C:\OFICINA", 0
t_d_juegos  db "C:\JUEGOS", 0
t_c_auto    db "C:\AUTOEXEC.BAT", 0
no_copiar   db "NUCLEO86.SYS", 0, "COMMAND.COM", 0, "AUTOEXEC.BAT", 0, "INSTALAR.COM", 0, 0
t_auto      db "@ECO OFF", 13, 10
            db "REM El arranque de Hispa-8086 en el disco duro (lo ha hecho INSTALAR).", 13, 10
            db "RUTA C:\;C:\OFICINA;C:\JUEGOS", 13, 10
            db "TECLADO", 13, 10
t_auto_tam  equ $ - t_auto
            db "MENU", 13, 10
t_menu_tam  equ $ - t_auto - t_auto_tam
sin_preguntar db 0

        absolute $
            alignb 2
op_arch     resw 1
op_dir      resw 1
d_fin       resw 1
h1          resw 1
h2          resw 1
bloque      resw 7
cola        resb 130
o_nom       resb 80
d_dir       resb 80
d_nom       resb 80
dta         resb 44
buf         resb BUF_TAM
            resb 512
pila_fin:
