; =============================================================================
;  EDITAR.COM (EDIT) - un editor de texto de pantalla completa
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       EDITAR archivo
;
;  Para escribir un AUTOEXEC.BAT, un CONFIG.SYS o una carta. En modo de texto
;  de 80 columnas, con cualquier tarjeta (MDA, Hercules, CGA, EGA, VGA):
;  veinticuatro líneas de texto y, abajo, una línea con las teclas.
;       flechas, Inicio, Fin, RePág, AvPág      moverse
;       Ins      insertar o sobrescribir        Supr, Retroceso   borrar
;       F2  guardar      F3  buscar (otra vez F3 e Intro: el siguiente)
;       F4  copiar la línea     F5  cortarla     F6  pegarla encima
;       F10 o Esc   salir (pregunta si hay cambios sin guardar)
;
;  Cómo: el texto entero está en la memoria, detrás del programa (lo que quepa
;  en su segmento: unos 55 K), con las líneas separadas por un salto (10); el
;  retorno (13) se quita al leer y se pone al guardar. La línea en la que
;  está el cursor se trabaja en un búfer aparte (hasta 250 letras) y vuelve al
;  texto al salir de ella: así teclear no mueve todo lo que hay detrás. La
;  pantalla se escribe directamente en la memoria de vídeo.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

FILAS       equ 24                  ; líneas de texto en la pantalla
LIN_MAX     equ 250                 ; letras de una línea
SALTO       equ 10
C_TEXTO     equ 0x07
C_MENU      equ 0x70

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        mov si, 0x81                ; el nombre del archivo
.e:
        lodsb
        cmp al, ' '
        je .e
        ja .n0
        mov dx, m_uso
        DOS 0x09
        mov ax, 0x4C01
        int 0x21
.n0:
        mov di, nombre
.n:
        stosb
        lodsb
        cmp al, ' '
        ja .n
        mov byte [di], 0
        mov ax, sp                  ; hasta dónde puede crecer el texto
        sub ax, 1024
        mov [limite], ax
        mov ah, 0x0F                ; la memoria de vídeo: monocromo o color
        int 0x10
        mov [modo], al
        mov bx, 0xB800
        cmp al, 7
        jne .v
        mov bh, 0xB0
.v:
        mov [vseg], bx
        call carga
        mov ax, txt
        mov [top], ax
        mov [lin_ini], ax
        mov word [n_lin], 1
        mov byte [insertar], 1
        call trae_lin

; ---- una tecla tras otra ----
bucle:
        mov ax, [cur_x]             ; la columna, a la vista
        cmp ax, [izq]
        jae .b1
        mov [izq], ax
.b1:
        mov bx, [izq]
        add bx, 79
        cmp ax, bx
        jbe .b2
        sub ax, 79
        mov [izq], ax
.b2:
        call pinta
        mov dh, [cur_y]
        mov dl, [cur_x]
        sub dl, [izq]
        call tecla_en
        test al, al
        jz .ext
        cmp al, 0xE0
        je .ext
        mov bx, teclas_a
        jmp short .busca
.ext:
        mov al, ah
        mov bx, teclas_e
.busca:
        cmp byte [bx], 0
        je .letra
        cmp al, [bx]
        je .esta
        add bx, 3
        jmp .busca
.esta:
        call [bx+1]
        jmp bucle
.letra:
        cmp bx, teclas_e + (teclas_e_fin - teclas_e)
        jae bucle                   ; (una tecla especial que no usamos)
        cmp al, ' '
        jb bucle
        call inserta
        jmp bucle

%macro TECLA 2
        db %1
        dw %2
%endmacro
teclas_a:
        TECLA 13, intro
        TECLA 8, retroceso
        TECLA 27, salir
        TECLA 9, tabulador
        db 0
teclas_e:
        TECLA 0x48, arriba
        TECLA 0x50, abajo
        TECLA 0x4B, izquierda
        TECLA 0x4D, derecha
        TECLA 0x47, al_inicio
        TECLA 0x4F, al_fin
        TECLA 0x49, repag
        TECLA 0x51, avpag
        TECLA 0x52, cambia_ins
        TECLA 0x53, suprime
        TECLA 0x3C, guardar
        TECLA 0x3D, buscar
        TECLA 0x3E, copiar
        TECLA 0x3F, cortar
        TECLA 0x40, pegar
        TECLA 0x44, salir
teclas_e_fin:
        db 0

; ---- el archivo ----
; lo lee a `txt`: sin los retornos, hasta el fin de archivo de los textos, y
; partiendo las líneas que pasen de LIN_MAX. Si no existe, se empieza de cero
carga:
        mov word [fin], txt
        mov dx, nombre
        mov ax, 0x3D00
        int 0x21
        jc .x
        mov [h], ax
        xor bp, bp                  ; BP = letras de la línea que se lleva
.lee:
        mov bx, [h]
        mov cx, 512
        mov dx, sec
        DOS 0x3F
        jc .cierra
        test ax, ax
        jz .cierra
        mov cx, ax
        mov si, sec
        mov di, [fin]
.c:
        lodsb
        cmp al, 0x1A
        je .eof
        cmp al, 13
        je .sig
        cmp al, SALTO
        je .nl
        cmp bp, LIN_MAX
        jb .pon
        mov byte [di], SALTO        ; demasiado larga: se parte
        inc di
        xor bp, bp
.pon:
        inc bp
        jmp short .p1
.nl:
        xor bp, bp
.p1:
        cmp di, [limite]
        jae .lleno
        stosb
.sig:
        loop .c
        mov [fin], di
        jmp .lee
.lleno:
        mov byte [cortado], 1       ; no cabe entero: se dirá
.eof:
        mov [fin], di
.cierra:
        mov bx, [h]
        DOS 0x3E
.x:
        ret

; ---- F2: guardar. CF=1 si no se ha podido ----
guardar:
        call deja_lin
        mov dx, nombre
        xor cx, cx
        DOS 0x3C
        jc .mal
        mov [h], ax
        mov si, txt
.trozo:
        mov di, sec                 ; hasta 256 letras de una vez (con sus
        mov cx, 256                 ; retornos, hasta 512)
.c:
        cmp si, [fin]
        jae .manda
        lodsb
        cmp al, SALTO
        jne .c1
        mov byte [di], 13
        inc di
.c1:
        stosb
        loop .c
.manda:
        mov cx, di
        sub cx, sec
        jcxz .hecho
        mov bx, [h]
        mov dx, sec
        DOS 0x40
        jc .mal_c
        cmp ax, cx
        jne .mal_c
        jmp .trozo
.hecho:
        mov bx, [h]
        DOS 0x3E
        jc .mal
        mov byte [cambiado], 0
        clc
        ret
.mal_c:
        mov bx, [h]
        DOS 0x3E
.mal:
        mov si, m_e_guardar
        call aviso
        stc
        ret

; ---- la línea del cursor: del texto al búfer y vuelta ----
trae_lin:
        mov si, [lin_ini]
        mov di, lin
        xor cx, cx
.c:
        cmp si, [fin]
        jae .f
        lodsb
        cmp al, SALTO
        je .f
        stosb
        inc cx
        jmp .c
.f:
        mov [lin_n], cx
        mov [lin_tam], cx
        mov byte [lin_sucia], 0
        cmp [cur_x], cx             ; (el cursor, dentro de ella)
        jbe .x
        mov [cur_x], cx
.x:
        ret
; si la línea ha cambiado, vuelve al texto (que se abre o se cierra lo justo)
deja_lin:
        cmp byte [lin_sucia], 0
        je .x
        mov byte [lin_sucia], 0
        call cambia
        mov cx, [lin_n]
        sub cx, [lin_tam]
        je .copia
        jb .menos
        mov di, [lin_ini]
        add di, [lin_tam]
        call abre
        jnc .copia
        mov ax, [lin_tam]           ; no cabe: la línea se queda como estaba
        mov [lin_n], ax
        ret
.menos:
        neg cx
        mov di, [lin_ini]
        add di, [lin_n]
        call cierra
.copia:
        mov si, lin
        mov di, [lin_ini]
        mov cx, [lin_n]
        mov [lin_tam], cx
        rep movsb
.x:
        ret
; abre CX octetos en DI (lo de detrás se corre). CF=1 (y pita) si no cabe
abre:
        push si
        push di
        push cx
        mov ax, [fin]
        add ax, cx
        jc .no
        cmp ax, [limite]
        jae .no
        mov cx, [fin]
        sub cx, di                  ; cuánto hay que correr
        mov si, [fin]
        dec si
        mov [fin], ax
        mov di, ax
        dec di
        std
        rep movsb
        cld
        pop cx
        pop di
        pop si
        clc
        ret
.no:
        call pita
        pop cx
        pop di
        pop si
        stc
        ret
; quita CX octetos de DI (lo de detrás se arrima)
cierra:
        push si
        push di
        push cx
        mov si, di
        add si, cx
        sub [fin], cx
        mov cx, [fin]
        sub cx, di
        rep movsb
        pop cx
        pop di
        pop si
        ret
pita:
        mov ax, 0x0E07
        int 0x10
        ret

; ---- moverse ----
izquierda:
        cmp word [cur_x], 0
        je .x
        dec word [cur_x]
.x:
        ret
derecha:
        mov ax, [cur_x]
        cmp ax, [lin_n]
        jae .x
        inc word [cur_x]
.x:
        ret
al_inicio:
        mov word [cur_x], 0
        ret
cambia:
        mov byte [cambiado], 1
        ret
; hay cambios; el cursor, al principio de la línea (cortar y pegar)
cambia_al_inicio:
        call cambia
        call al_inicio
        jmp trae_lin
al_fin:
        mov ax, [lin_n]
        mov [cur_x], ax
        ret
; a la línea de arriba. CF=1 si no la hay
arriba:
        mov bx, [lin_ini]
        cmp bx, txt
        je .no
        call deja_lin
        mov bx, [lin_ini]
        dec bx                      ; (el salto de la de arriba)
.b:
        cmp bx, txt
        je .esta
        cmp byte [bx-1], SALTO
        je .esta
        dec bx
        jmp .b
.esta:
        mov [lin_ini], bx
        dec word [n_lin]
        cmp byte [cur_y], 0
        je .sube
        dec byte [cur_y]
        jmp short .t
.sube:
        mov [top], bx
.t:
        call trae_lin
        clc
        ret
.no:
        stc
        ret
; a la línea de abajo. CF=1 si no la hay
abajo:
        call deja_lin
        mov bx, [lin_ini]
        add bx, [lin_tam]
        cmp bx, [fin]
        jae .no
        inc bx
        mov [lin_ini], bx
        inc word [n_lin]
        cmp byte [cur_y], FILAS - 1
        jae .baja
        inc byte [cur_y]
        jmp short .t
.baja:
        mov bx, [top]               ; la pantalla, una línea abajo
.b:
        inc bx
        cmp byte [bx-1], SALTO
        jne .b
        mov [top], bx
.t:
        call trae_lin
        clc
        ret
.no:
        stc
        ret
repag:
        mov dx, arriba
        jmp short pagina
avpag:
        mov dx, abajo
; una pantalla arriba o abajo: DX = la de una línea (no tocan DX)
pagina:
        mov cx, FILAS - 1
.c:
        push cx
        call dx
        pop cx
        jc .x
        loop .c
.x:
        ret

; ---- escribir y borrar ----
cambia_ins:
        xor byte [insertar], 1
        ret
tabulador:
        cmp word [lin_n], LIN_MAX   ; espacios hasta la columna siguiente que
        jae .x                      ; sea múltiplo de ocho
        mov al, ' '
        call inserta
        test word [cur_x], 7
        jnz tabulador
.x:
        ret
; la letra AL, donde está el cursor
inserta:
        mov bx, [cur_x]
        cmp byte [insertar], 0
        jne .ins
        cmp bx, [lin_n]
        jae .ins
        mov [lin+bx], al            ; sobrescribir
        jmp short .hecho
.ins:
        cmp word [lin_n], LIN_MAX
        jb .cabe
        jmp pita
.cabe:
        mov cx, [lin_n]
        sub cx, bx
        mov si, lin
        add si, [lin_n]
        mov di, si
        dec si
        std
        rep movsb
        cld
        mov [lin+bx], al
        inc word [lin_n]
.hecho:
        inc word [cur_x]
        jmp suprime.sucia
; Supr: la letra del cursor; al final de la línea, se le junta la siguiente
suprime:
        mov bx, [cur_x]
        cmp bx, [lin_n]
        jae .junta
        mov cx, [lin_n]
        sub cx, bx
        dec cx
        lea di, [lin+bx]
        lea si, [di+1]
        rep movsb
        dec word [lin_n]
.sucia:                             ; (también de inserta)
        mov byte [lin_sucia], 1
        ret
.junta:
        call deja_lin
        mov di, [lin_ini]
        add di, [lin_tam]
        cmp di, [fin]
        jae .x                      ; (es la última)
        lea si, [di+1]              ; ¿caben las dos en una?
        mov cx, [lin_tam]
.c:
        cmp si, [fin]
        jae .cabe
        cmp byte [si], SALTO
        je .cabe
        inc si
        inc cx
        cmp cx, LIN_MAX
        jbe .c
        jmp pita
.cabe:
        mov cx, 1
        call cierra
        call cambia
        call trae_lin
.x:
        ret
; Retroceso: la letra de antes; al principio, la línea se junta a la de arriba
retroceso:
        cmp word [cur_x], 0
        je .junta
        dec word [cur_x]
        jmp suprime
.junta:
        call arriba
        jc .x
        call al_fin
        call suprime
.x:
        ret
; Intro: la línea se parte por el cursor
intro:
        call deja_lin
        mov di, [lin_ini]
        add di, [cur_x]
        mov cx, 1
        call abre
        jc .x
        mov byte [di], SALTO
        call cambia
        call trae_lin
        call abajo
        call al_inicio
.x:
        ret

; ---- F4 copiar la línea, F5 cortarla, F6 pegarla encima de esta ----
copiar:
        mov si, lin
        mov di, clip
        mov cx, [lin_n]
        mov [clip_n], cx
        rep movsb
        mov byte [hay_clip], 1
        ret
cortar:
        call copiar
        call deja_lin
        mov di, [lin_ini]
        mov cx, [lin_tam]
        mov bx, di
        add bx, cx
        cmp bx, [fin]               ; con su salto, si lo tiene
        jae .c
        inc cx
.c:
        jcxz .x
        call cierra
        jmp cambia_al_inicio
.x:
        ret
pegar:
        cmp byte [hay_clip], 0
        je .x
        call deja_lin
        mov di, [lin_ini]
        mov cx, [clip_n]
        inc cx
        call abre
        jc .x
        mov si, clip
        dec cx
        rep movsb
        mov byte [di], SALTO
        jmp cambia_al_inicio
.x:
        ret

; ---- F3: buscar, desde detrás del cursor, sin distinguir mayúsculas ----
buscar:
        call deja_lin
        mov si, m_buscar            ; se pregunta qué (Intro: lo de antes)
        call menu
        mov dx, 0x1800 + 8
        xor bx, bx                  ; BX = letras tecleadas
.t:
        push bx
        push dx
        call tecla_en
        pop dx
        pop bx
        cmp al, 13
        je .va
        cmp al, 27
        je .x0
        cmp al, 8
        jne .t1
        test bx, bx
        jz .t
        dec bx
        dec dl
        mov al, ' '
        call .pon
        jmp .t
.t1:
        cmp al, ' '
        jb .t
        cmp bx, 30
        jae .t
        mov [busca+bx], al
        call .pon
        inc bx
        inc dl
        jmp .t
.x0:
        ret
; (interna) la letra AL en la línea de abajo, columna DL
.pon:
        push es
        push di
        mov es, [vseg]
        xor dh, dh
        mov di, dx
        shl di, 1
        add di, FILAS * 160
        mov ah, C_MENU
        stosw
        mov dh, 0x18
        pop di
        pop es
        ret
.va:
        test bx, bx
        jz .mismo
        mov [busca_n], bx
.mismo:
        mov cx, [busca_n]
        jcxz .x0
        mov si, [lin_ini]
        add si, [cur_x]
        inc si
.b:
        mov ax, si                  ; ¿queda sitio para que esté?
        add ax, cx
        cmp ax, [fin]
        ja .no
        push si
        mov di, busca
        mov cx, [busca_n]
.c:
        lodsb
        call mayus
        mov ah, al
        mov al, [di]
        call mayus
        inc di
        cmp al, ah
        jne .c1
        loop .c
.c1:
        pop si
        je .esta
        mov cx, [busca_n]
        inc si
        jmp .b
.no:
        jmp pita
.esta:
        ; SI = dónde: su línea pasa a ser la primera de la pantalla
        mov bx, si
.l:
        cmp bx, txt
        je .l1
        cmp byte [bx-1], SALTO
        je .l1
        dec bx
        jmp .l
.l1:
        mov [lin_ini], bx
        mov [top], bx
        mov byte [cur_y], 0
        sub si, bx
        mov [cur_x], si
        mov word [n_lin], 1         ; y su número: los saltos que hay delante
        mov si, txt
.n:
        cmp si, bx
        jae .n1
        lodsb
        cmp al, SALTO
        jne .n
        inc word [n_lin]
        jmp .n
.n1:
        jmp trae_lin
; el cursor en la fila DH, columna DL, y espera una tecla -> AX (no conserva BX)
tecla_en:
        xor bh, bh
        mov ah, 2
        int 0x10
tecla:
        xor ah, ah
        int 0x16
        ret
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret

; ---- F10 o Esc: salir ----
salir:
        call deja_lin
        cmp byte [cambiado], 0
        je .fuera
        mov si, m_cambios
        call menu
        call tecla
        and al, 0xDF
        cmp al, 'N'
        je .fuera
        cmp al, 'S'
        jne .x
        call guardar
        jc .x
.fuera:
        mov al, [modo]              ; la pantalla, limpia
        xor ah, ah
        int 0x10
        mov ax, 0x4C00
        int 0x21
.x:
        ret

; ---- la pantalla ----
pinta:
        push es
        mov es, [vseg]
        xor di, di
        mov si, [top]
        xor bl, bl                  ; BL = la fila
.fila:
        cmp bl, [cur_y]
        jne .otra
        push si                     ; la del cursor: del búfer
        mov si, lin
        mov cx, [lin_n]
        call .linea
        pop si
        mov si, [lin_ini]
        add si, [lin_tam]
        inc si
        jmp short .sig
.otra:
        xor cx, cx                  ; las demás: del texto, hasta su salto
        cmp si, [fin]
        jae .o2
        push si
.o1:
        lodsb
        cmp al, SALTO
        je .o3
        inc cx
        cmp si, [fin]
        jb .o1
        inc si                      ; (la última, sin salto)
.o3:
        pop ax
        push si
        mov si, ax
        call .linea
        pop si
        jmp short .sig
.o2:
        call .linea
.sig:
        inc bl
        cmp bl, FILAS
        jb .fila
        ; la línea de abajo: las teclas, y por dónde se va
        mov si, m_menu
        mov ah, C_MENU
        call menu_txt
        mov ax, [n_lin]
        call .num
        mov al, ':'                 ; (.num deja AH = C_MENU)
        stosw
        mov ax, [cur_x]
        inc ax
        call .num
        mov al, ' '
        stosw
        mov si, m_ins
        cmp byte [insertar], 0
        jne .m2
        mov si, m_sob
.m2:
        mov cx, 3
.m3:
        lodsb
        stosw
        loop .m3
        mov al, ' '
        cmp byte [cambiado], 0
        je .m4
        mov al, '*'
.m4:
        stosw
        jmp menu.r                  ; (lo demás de la línea, y POP ES)
; (interna) las CX letras de SI, en una fila entera de la pantalla (desde la
; columna [izq])
.linea:
        mov dx, 80
        mov ax, [izq]
        cmp cx, ax
        jbe .rel
        add si, ax
        sub cx, ax
        cmp cx, dx
        jbe .l1
        mov cx, dx
.l1:
        sub dx, cx
        mov ah, C_TEXTO
.l2:
        lodsb
        stosw
        loop .l2
.rel:
        mov cx, dx
        mov ax, C_TEXTO * 256 + ' '
        rep stosw
        ret
; (interna) AX en decimal, en la línea de abajo
.num:
        push bx
        mov bx, 10
        xor cx, cx
.n1:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .n1
.n2:
        pop ax
        add al, '0'
        mov ah, C_MENU
        stosw
        loop .n2
        pop bx
        ret
; escribe el texto de SI en la línea de abajo (y borra lo demás de ella)
menu:
        push es
        mov es, [vseg]
        mov di, FILAS * 160
        mov ah, C_MENU
        call menu_txt
.r:
        mov al, ' '
.r1:
        cmp di, 25 * 160
        jae .x
        stosw
        jmp .r1
.x:
        pop es
        ret
; el texto de SI en ES:DI, con el color de AH
menu_txt:
        lodsb
        test al, al
        jz .x
        stosw
        jmp menu_txt
.x:
        ret
; un aviso en la línea de abajo, hasta que se pulse una tecla
aviso:
        call menu
        call pita
        jmp tecla

m_uso       db "EDITAR - editor de texto", 13, 10, "  EDITAR archivo", 13, 10, "$"
m_menu      db " F2 Guardar  F3 Buscar  F4 Copiar  F5 Cortar  F6 Pegar  F10 Salir   ", 0
m_ins       db "INS"
m_sob       db "SOB"
m_buscar    db " Buscar:", 0
m_cambios   db " Hay cambios sin guardar. ¿Guardarlos? (S: sí, N: no, otra: seguir)", 0
m_e_guardar db " No se ha podido guardar (¿disco lleno o protegido?). Pulse una tecla.", 0
cur_x       dw 0                    ; la columna, dentro de la línea
izq         dw 0                    ; la primera columna que se ve
busca_n     dw 0
cur_y       db 0                    ; la fila, en la pantalla
cambiado    db 0
cortado     db 0
hay_clip    db 0

        absolute $
            alignb 2
top         resw 1                  ; la primera línea de la pantalla
lin_ini     resw 1                  ; la línea del cursor, en el texto
lin_tam     resw 1                  ; lo que mide allí
lin_n       resw 1                  ; lo que mide en el búfer
n_lin       resw 1                  ; su número
fin         resw 1                  ; donde acaba el texto
limite      resw 1                  ; y hasta dónde puede llegar
vseg        resw 1
h           resw 1
clip_n      resw 1
modo        resb 1
insertar    resb 1
lin_sucia   resb 1
nombre      resb 82
busca       resb 32
lin         resb LIN_MAX + 6
clip        resb LIN_MAX + 6
sec         resb 512
txt:
