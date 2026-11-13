; =============================================================================
;  VENTANAS.COM - Hispa Ventanas, etapa 3: ventanas, mensajes y programas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  VENTANAS [/1024] [/640] [/8] [/15] [/16] [/24] [/32] [/COLOR] [/VGA]
;                 [/DEMO]
;
;  El entorno gráfico propio que va encima de Hispa-DOS (docs/VENTANAS.md).
;  Todo está hecho aquí desde cero: el código, la paleta, el cursor, el
;  mosaico del fondo y la fuente (ventanas/fuente.txt).
;
;  Es un cliente DPMI de 32 bits (HSPDPMI.COM, que necesita HSPXMS.SYS):
;  - arranca en modo real, comprueba lo que necesita (y carga HSPDPMI y
;    HSPRATON si están en su misma carpeta y no estaban cargados), elige el
;    modo de vídeo y entra en modo protegido;
;  - un solo segmento de datos de 4 GB con la base del programa: un puntero
;    «cercano» llega a toda la memoria (dirección lineal menos «base»).
;
;  Capas (cada una en su archivo; las interfaces, en docs/VENTANAS.md):
;    video.inc      driver de pantalla: elegir y poner el modo, paleta y
;                   llevar un rectángulo de la superficie a la pantalla
;    gdi.inc        dibujo sobre la superficie (8 bits por punto), con recorte
;    dc.inc         regiones de recorte y contexto de dispositivo por ventana
;    raton.inc      cursor por software
;    nucleo.inc     memoria, tareas (multitarea cooperativa), colas de
;                   mensajes y temporizadores
;    gestor.inc     ventanas, orden, activa, foco, zona no cliente, reparto
;                   del ratón y del teclado
;    menus.inc, controles.inc, dialogos.inc, dos.inc
;    escritorio.inc el fondo, la barra y los iconos de lo minimizado
;    centro.inc, bloc.inc, calc.inc, reloj.inc    los programas
;    accesos.inc    (Centro) grupos ligados a una carpeta de juegos y opciones
;                   de juego de un acceso: velocidad, LOADFIX y sonido
;    apagado.inc    [APAGAR] el cuadro «Salir»: volver a Hispa-DOS, reiniciar o apagar
;    api.inc, modulos.inc   los programas aparte (.HVP) y su interfaz
;
;  La entrada llega por interrupciones: el driver de ratón llama a rm_raton
;  (INT 33h, función 0Ch), que apunta cada suceso en una cola; las teclas las
;  encola la BIOS (IRQ 1, con el teclado que haya dejado KEYB). El bucle del
;  sistema duerme con HLT en modo real hasta la siguiente interrupción.
; =============================================================================
        cpu 386
        bits 16
; VENTANAS.COM tiene formato .EXE (cabecera «MZ») aunque se llame .COM: el
; núcleo mira la cabecera, no el nombre. Así el archivo puede pasar de 64 KB:
; lo que usa la parte de modo real (todo el código de 16 bits, y las
; variables de bss.inc) sigue en los primeros 64 KB, y detrás va la «parte
; alta» (alto_ini): datos y código que solo usa la parte de 32 bits, que
; llega a todo con punteros de 32 bits. La cabecera pide CS = SS = PSP
; (FFF0h: el segmento de carga menos 10h) e IP = 100h, es decir, el programa
; arranca igual que un .COM (ORG 100h, DS = ES = CS = PSP); el núcleo carga
; un .EXE así donde cargaría un .COM (sin el relleno hasta 1000h).
MZ_CAB      equ 32
        org 0x100 - MZ_CAB
mz_cab:
        db "MZ"
        dw (fin_archivo - mz_cab) % 512             ; octetos de la última página
        dw (fin_archivo - mz_cab + 511) / 512       ; páginas de 512
        dw 0                                        ; reubicaciones
        dw MZ_CAB / 16                              ; párrafos de la cabecera
        dw (bss_fin - fin_archivo + 15) / 16        ; memoria detrás de la imagen
        dw 0xFFFF
        dw 0xFFF0                                   ; SS = PSP
        dw pila_rm_fin                              ; SP (la parte gráfica pone la suya)
        dw 0
        dw 0x100                                    ; IP
        dw 0xFFF0                                   ; CS = PSP
        dw 0x1C, 0
        times MZ_CAB - ($ - mz_cab) db 0
%define CON_PROGRAMAS
%include "defs.inc"
RAT_COLA    equ 64                  ; sucesos de ratón que caben en la cola

inicio:
        jmp near arranque

; =============================================================================
;  El lanzador: lo único de VENTANAS.COM que se queda en la memoria
;  convencional (unos 2 KB) mientras Hispa Ventanas está en marcha.
;
;  VENTANAS.COM se ejecuta dos veces. La primera copia (esta) suelta toda su
;  memoria menos este trozo, carga HSPRATON y HSPDPMI si faltan y ejecuta
;  otra vez VENTANAS.COM con «/@» delante de las opciones: esa segunda copia
;  es el entorno gráfico. Para abrir un programa de Hispa-DOS (un juego), el
;  entorno gráfico guarda su estado en CENTRO.INI, deja aquí el encargo
;  (programa, parámetros, carpeta) y termina con el código FEh; el lanzador
;  quita HSPDPMI de la memoria (o lo desactiva, si ya estaba cargado antes de
;  entrar), ejecuta el programa con toda la memoria convencional menos este
;  trozo y, cuando acaba, vuelve a cargar el entorno gráfico, que se
;  reconstruye desde CENTRO.INI.
;  Un juego con opciones (accesos.inc) llega aquí ya compuesto: el programa
;  es VELOCIDA.COM o LOADFIX.COM y el juego va en los parámetros. El lanzador
;  solo añade dos cosas: cargar antes HSPSND /SB si se pide (lz_sb) y, al
;  volver de un juego con velocidad, comprobar que el equipo no se queda
;  frenado (lz_vel_red).
; =============================================================================
lz_firma    db "HVLZ"
lz_orden    db 0                    ; 1: hay un encargo (lo pone la parte gráfica)
lz_band     db 0                    ; bit 0 volver a Hispa Ventanas, bit 1 esperar una tecla,
                                    ; bit 2 va con VELOCIDAD (al volver se mira que no
                                    ; quede frenando), bit 3 cargar antes HSPSND /SB
lz_velst    db 0                    ; cómo quedó VELOCIDAD tras el programa (LZV_*):
                                    ; lo lee y lo borra la parte gráfica al volver
lz_dpmi     db 0                    ; HSPDPMI: 1 lo ha cargado el lanzador, 2 ya estaba
lz_ayuda    db 0                    ; «/?»: no hace falta cargar nada
lz_unidad   db 0
            align 2
lz_old24    dd 0
lz_sp       dw 0
lz_nom      dw 0                    ; dónde empieza el nombre dentro de lz_ruta
lz_blq      dw 0, 0x80, 0, 0x5C, 0, 0x6C, 0     ; bloque de EXEC: la cola, en PSP:80h
lz_prog     times 80 db 0           ; el encargo: programa,
lz_param    times 128 db 0          ;   parámetros (hasta 126 letras: con VELOCIDAD
                                    ;   y LOADFIX delante llevan también el programa)
lz_dir      times 68 db 0           ;   y carpeta de trabajo
lz_ruta     times 80 db 0           ; VENTANAS.COM, con su carpeta
lz_cola     times 48 db 0           ; la cola de órdenes de la parte gráfica
lz_dir0     times 68 db 0           ; la carpeta en la que se estaba
lz_aux      times 80 db 0
lz_n_raton  db "HSPRATON.COM", 0
lz_n_dpmi   db "HSPDPMI.COM", 0
lz_n_vel    db "VELOCIDA.COM", 0
lz_n_snd    db "HSPSND.COM", 0
lz_c_nada   db 0, 13                ; colas de órdenes para lz_hermano
lz_c_normal db 7, " NORMAL", 13
lz_c_sb     db 4, " /SB", 13
            align 2
lz_velpre   dw 0                    ; el residente de VELOCIDAD que había antes
lz_s_comspec db "COMSPEC="
lz_s_command db "C:\COMMAND.COM", 0
lz_m_dpmi   db "Hispa Ventanas: cargando el servidor DPMI (HSPDPMI)...", 13, 10, "$"
lz_m_noexec db "Hispa Ventanas: no se ha podido cargar VENTANAS.COM.", 13, 10, "$"
lz_m_noprog db "Hispa Ventanas: no se ha podido ejecutar el programa.", 13, 10, "$"
lz_m_tecla  db 13, 10, "Pulse una tecla para continuar...$"
lz_m_nl     db 13, 10, "$"

; error crítico mientras está el entorno gráfico: la llamada falla, sin
; preguntar en la pantalla
lz_int24:
        mov al, 3
        iret

; ejecuta el programa DS:DX con la cola de PSP:80h -> CF=1 si no se puede
lz_exec:
        mov [lz_blq+4], cs
        mov [lz_blq+8], cs
        mov [lz_blq+12], cs
        mov [lz_sp], sp
        mov bx, lz_blq
        mov ax, 0x4B00
        int 0x21
        cli
        mov bx, cs
        mov ss, bx
        mov sp, [cs:lz_sp]
        sti
        mov ds, bx
        mov es, bx
        cld
        ret

; ejecuta el programa SI de la carpeta de VENTANAS.COM con la cola de
; órdenes BX (largo, texto y 13)
lz_hermano:
        push si
        mov si, bx
        mov di, 0x80
        mov cx, 10
        rep movsb
        mov si, lz_ruta
        mov di, lz_aux
        mov cx, [lz_nom]
        sub cx, si
        rep movsb
        pop si
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        mov dx, lz_aux
        jmp lz_exec

; ¿hay un residente de VELOCIDAD (utils/velocidad.asm)? Recorre la cadena de
; bloques de memoria buscando su firma, como hace VELOCIDAD.
; -> AX = su segmento (0: no hay) y DL distinto de 0 si está frenando (el
; freno del temporizador puesto o las cachés desactivadas)
lz_vel_res:
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
.l:
        mov es, ax
        inc ax
        cmp [es:1], ax              ; un bloque que es su propio dueño: un PSP
        jne .nx
        cmp dword [es:0x10+VELR_FIRMA], 'HSPV'
        jne .nx
        cmp dword [es:0x10+VELR_FIRMA+4], 'EL1'
        jne .nx
        mov dl, [es:0x10+VELR_ACTIVO]
        or dl, [es:0x10+VELR_CACHE]
        or dl, [es:0x10+VELR_CD]
        jmp .x
.nx:
        cmp byte [es:0], 'M'
        jne .no
        add ax, [es:3]
        jmp .l
.no:
        xor ax, ax
        xor dl, dl
.x:
        push cs
        pop es
        ret

; Tras un programa lanzado con VELOCIDAD: el equipo tiene que quedar a su
; velocidad normal. «VELOCIDAD x programa» ya lo deja todo como estaba al
; acabar el programa (también si acaba con Ctrl+C o con un error); esta es la
; segunda red: si aun así queda un residente frenando (o con las cachés
; desactivadas), se ejecuta VELOCIDAD NORMAL. En lz_velst queda lo que ha
; pasado, para que lo cuente la parte gráfica.
LZV_NORMAL  equ 1                   ; seguía frenando: se ha puesto NORMAL
LZV_FRENA   equ 2                   ; sigue frenando: no se ha podido quitar
LZV_QUEDA   equ 3                   ; sin frenar, pero VELOCIDAD se ha quedado en la
                                    ; memoria (otro residente tomó la INT 08h después)
lz_vel_red:
        call lz_vel_res
        test ax, ax
        jz .x
        test dl, dl
        jz .queda
        mov byte [lz_velst], LZV_NORMAL
        mov si, lz_n_vel
        mov bx, lz_c_normal
        call lz_hermano
        call lz_vel_res
        test ax, ax
        jz .x
        test dl, dl
        jz .queda
        mov byte [lz_velst], LZV_FRENA
        ret
.queda:
        cmp ax, [lz_velpre]         ; (si ya estaba antes, es cosa del usuario)
        je .x
        mov byte [lz_velst], LZV_QUEDA
.x:
        ret

; La Sound Blaster emulada: carga HSPSND /SB (se queda residente). HSPSND
; apunta la variable BLASTER en el entorno de quien lo ejecuta, que es este
; lanzador, y de él la hereda el juego: antes se le hace sitio (64 octetos).
lz_sb:
        mov ax, [0x2C]
        dec ax
        mov es, ax
        mov cx, [es:3]              ; párrafos del entorno de ahora
        mov bx, cx
        add bx, 4
        mov ah, 0x48
        int 0x21
        jc .carga
        push ds
        mov es, ax
        mov ds, [0x2C]
        xor si, si
        xor di, di
        shl cx, 3
        rep movsw
        pop ds
        push es
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        pop word [0x2C]
.carga:
        push cs
        pop es
        mov si, lz_n_snd
        mov bx, lz_c_sb
        jmp lz_hermano

; carga HSPRATON y HSPDPMI si faltan (primero el ratón: HSPDPMI tiene que
; ser el último residente para poder quitarlo)
lz_residentes:
        xor ax, ax
        mov es, ax
        mov ax, [es:0x33*4]
        or ax, [es:0x33*4+2]
        push cs
        pop es
        jz .cr
        xor ax, ax
        int 0x33
        cmp ax, 0xFFFF
        je .dpmi
.cr:
        mov si, lz_n_raton
        mov bx, lz_c_nada
        call lz_hermano
.dpmi:
        mov ax, 0x4300              ; sin XMS no hay nada que cargar
        int 0x2F
        cmp al, 0x80
        jne .x
        call lz_hay_dpmi
        jnz .falta
        cmp byte [lz_dpmi], 0
        jne .x
        mov byte [lz_dpmi], 2       ; ya estaba: se deja al salir
        jmp .x
.falta:
        mov ax, 0xD600              ; ¿HSPDPMI desactivado? Se activa
        mov bx, 0x4844
        xor dl, dl
        int 0x2F
        push cs
        pop es
        cmp ax, 0x4450
        jne .carga
        mov ax, 0xD600
        mov bx, 0x4844
        mov dl, 2
        int 0x2F
        push cs
        pop es
        mov byte [lz_dpmi], 2
        jmp .x
.carga:
        mov dx, lz_m_dpmi
        mov ah, 0x09
        int 0x21
        mov si, lz_n_dpmi
        mov bx, lz_c_nada
        call lz_hermano
        call lz_hay_dpmi
        jnz .x
        cmp byte [lz_dpmi], 0
        jne .x
        mov byte [lz_dpmi], 1
.x:
        ret

; ZF=1 si hay un servidor DPMI
lz_hay_dpmi:
        mov ax, 0x1687
        int 0x2F
        push cs
        pop es
        test ax, ax
        ret

; quita HSPDPMI de la memoria (si es el nuestro y se deja). Si ya estaba
; cargado al entrar no se descarga, solo se desactiva: volver a cargarlo lo
; dejaría en otro sitio y partiría la memoria del usuario
lz_quitar_dpmi:
        mov ax, 0xD600
        mov bx, 0x4844
        mov dl, 1
        cmp byte [lz_dpmi], 2
        jne .q
        mov dl, 3
.q:
        int 0x2F
        cmp ax, 0x4450
        jne .x
        cmp bl, 2
        jne .x
        mov ah, 0x49                ; ES = el segmento del residente
        int 0x21
.x:
        push cs
        pop es
        ret

; el encargo: ejecuta lz_prog con lz_param en la carpeta lz_dir
lz_programa:
        call lz_quitar_dpmi
        mov byte [lz_velst], 0
        test byte [lz_band], 8
        jz .sinsb
        call lz_sb
.sinsb:
        test byte [lz_band], 4
        jz .sinvel0
        call lz_vel_res
        mov [lz_velpre], ax
.sinvel0:
        mov ah, 0x19
        int 0x21
        mov [lz_unidad], al
        mov byte [lz_dir0], '\'
        mov si, lz_dir0 + 1
        xor dl, dl
        mov ah, 0x47
        int 0x21
        mov si, lz_dir              ; la carpeta de trabajo
        cmp byte [si], 0
        je .sindir
        cmp byte [si+1], ':'
        jne .cd
        mov dl, [si]
        and dl, 0xDF
        sub dl, 'A'
        mov ah, 0x0E
        int 0x21
        cmp byte [si+2], 0
        je .sindir
.cd:
        mov dx, si
        mov ah, 0x3B
        int 0x21
.sindir:
        mov si, lz_prog             ; ¿un archivo de órdenes (.BAT)?
.f:
        lodsb
        test al, al
        jnz .f
        mov di, 0x81
        mov dx, lz_prog
        cmp byte [si-5], '.'
        jne .cola
        mov ax, [si-4]
        and ax, 0xDFDF
        cmp ax, 'BA'
        jne .cola
        mov al, [si-2]
        and al, 0xDF
        cmp al, 'T'
        jne .cola
        call lz_comspec             ; COMSPEC /C programa parámetros
        mov ax, ' /'
        stosw
        mov ax, 'C '
        stosw
        mov si, lz_prog
.b1:
        lodsb
        test al, al
        jz .b2
        stosb
        jmp .b1
.b2:
        mov dx, lz_aux
.cola:
        mov si, lz_param
        cmp byte [si], 0
        je .c2
        mov al, ' '
        stosb
.c1:
        lodsb
        test al, al
        jz .c2
        cmp di, 0x80 + 126
        jae .c2
        stosb
        jmp .c1
.c2:
        mov byte [di], 13
        mov ax, di
        sub ax, 0x81
        mov [0x80], al
        call lz_exec
        jnc .ok
        mov dx, lz_m_noprog
        mov ah, 0x09
        int 0x21
        or byte [lz_band], 2
.ok:
        test byte [lz_band], 4
        jz .sinvel
        call lz_vel_red
.sinvel:
        test byte [lz_band], 2
        jz .sinpausa
        mov dx, lz_m_tecla
        mov ah, 0x09
        int 0x21
        mov ax, 0x0C08              ; vacía el teclado y espera una tecla
        int 0x21
        test al, al
        jnz .t2
        mov ah, 0x08
        int 0x21
.t2:
        mov dx, lz_m_nl
        mov ah, 0x09
        int 0x21
.sinpausa:
        mov ah, 0x0F                ; si el programa ha dejado otro modo, a texto
        int 0x10
        cmp al, 3
        je .m
        cmp al, 7
        je .m
        mov ax, 0x0003
        int 0x10
.m:
        mov dl, [lz_unidad]         ; la unidad y la carpeta de antes
        mov ah, 0x0E
        int 0x21
        mov dx, lz_dir0
        mov ah, 0x3B
        int 0x21
        ret

; lz_aux = el intérprete de órdenes (COMSPEC del entorno, o C:\COMMAND.COM)
lz_comspec:
        push di
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .no
        mov si, lz_s_comspec
        mov cx, 8
        mov bx, di
        repe cmpsb
        je .hay
        mov di, bx
        xor al, al
        mov cx, 0xFFFF
        repne scasb
        jmp .v
.hay:
        mov bx, lz_aux
.c:
        mov al, [es:di]
        inc di
        mov [bx], al
        inc bx
        cmp bx, lz_aux + 79
        jae .f
        test al, al
        jnz .c
.f:
        mov byte [bx], 0
        jmp .x
.no:
        push cs
        pop es
        mov si, lz_s_command
        mov di, lz_aux
        mov cx, 15
        rep movsb
.x:
        push cs
        pop es
        pop di
        ret

; el ciclo: entorno gráfico, programa encargado, entorno gráfico...
lz_ciclo:
        cmp byte [lz_ayuda], 0
        jne .sinres
        call lz_residentes
.sinres:
        mov dx, lz_int24
        mov ax, 0x2524
        int 0x21
        mov si, lz_cola
        mov di, 0x80
        mov cx, 48
        rep movsb
        mov dx, lz_ruta
        call lz_exec
        pushf
        push ds
        lds dx, [lz_old24]
        mov ax, 0x2524
        int 0x21
        pop ds
        popf
        jc .noexec
        mov ah, 0x4D
        int 0x21
        cmp al, 0xFE
        jne .fin
        cmp byte [lz_orden], 1
        jne .fin
        mov byte [lz_orden], 0
        call lz_programa
        test byte [lz_band], 1
        jnz lz_ciclo
        cmp byte [lz_dpmi], 2       ; sin vuelta: si HSPDPMI ya estaba al
        jne .f0                     ; entrar, se deja cargado otra vez
        call lz_residentes
.f0:
        xor al, al
.fin:
        push ax
        cmp byte [lz_dpmi], 1       ; la memoria, como estaba al entrar
        jne .f2
        call lz_quitar_dpmi
.f2:
        pop ax
        mov ah, 0x4C
        int 0x21
.noexec:
        mov dx, lz_m_noexec
        mov ah, 0x09
        int 0x21
        mov al, 1
        jmp .fin

            align 2
            times 384 db 0          ; la pila del lanzador
lz_pila_fin:
            align 16
lz_fin:

; ---- arranque: ¿soy el lanzador o el entorno gráfico («/@»)? ----
arranque:
        cld
        mov si, 0x81
.b:
        lodsb
        cmp al, 13
        je lanzador
        cmp al, '/'
        jne .b
        cmp byte [si], '@'
        jne .b
        jmp grafico

lanzador:
        mov sp, lz_pila_fin
        mov es, [0x2C]              ; entorno: tras el doble cero, la ruta propia
        xor di, di
        xor al, al
.e1:
        mov cx, 0xFFFF
        repne scasb
        cmp [es:di], al
        jne .e1
        add di, 3
        mov bx, lz_ruta
        mov dx, bx
.c1:
        mov al, [es:di]
        inc di
        mov [bx], al
        inc bx
        cmp al, '\'
        jne .c2
        mov dx, bx
.c2:
        cmp bx, lz_ruta + 79
        jae .c4
        test al, al
        jnz .c1
.c4:
        mov byte [bx], 0
        mov [lz_nom], dx
        push cs
        pop es
        mov di, lz_cola + 1         ; la cola de la parte gráfica: « /@» y la nuestra
        mov al, ' '
        stosb
        mov ax, '/@'
        stosw
        mov si, 0x81
        movzx cx, byte [0x80]
        cmp cx, 40
        jbe .cc
        mov cx, 40
.cc:
        jcxz .cf
        lodsb
        cmp al, '?'
        jne .c5
        mov byte [lz_ayuda], 1
.c5:
        stosb
        loop .cc
.cf:
        mov byte [di], 13
        mov ax, di
        sub ax, lz_cola + 1
        mov [lz_cola], al
        mov ax, 0x3524
        int 0x21
        mov [lz_old24], bx
        mov [lz_old24+2], es
        push cs
        pop es
        mov bx, (lz_fin - inicio + 0x100 + 15) / 16
        mov ah, 0x4A                ; soltar todo menos el lanzador
        int 0x21
        jmp lz_ciclo

; =============================================================================
;  El entorno gráfico (la segunda copia)
; =============================================================================
grafico:
        cld
        mov sp, pila_rm_fin
        mov di, bss_ini
        mov cx, pila_rm - bss_ini
        xor al, al
        rep stosb
        mov [rmseg], cs
        mov ax, [0x2C]              ; el entorno (el servidor DPMI cambia
        mov [env_seg], ax           ; luego esta palabra del PSP por un selector)
        mov bx, (bss_fin - inicio + 0x100 + 15) / 16
        mov ah, 0x4A                ; soltar la memoria que no se usa
        int 0x21

        ; ---- opciones ----
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .argfin
        cmp al, '/'
        jne .arg
        mov bx, opciones
.o1:
        mov cl, [bx]
        xor ch, ch
        test cx, cx
        jz .ayuda                   ; opción desconocida
        push si
        lea di, [bx+1]
.o2:
        lodsb
        cmp al, 'a'
        jb .o3
        cmp al, 'z'
        ja .o3
        sub al, 0x20
.o3:
        cmp al, [di]
        jne .o4
        inc di
        loop .o2
        pop ax
        mov al, [di]                ; clase de opción y valor
        mov dx, [di+1]
        cmp al, 1
        jb .ovga
        je .oancho
        cmp al, 3
        jb .obpp
        je .ocolor
        cmp al, 5
        je .ofallo
        cmp al, 6
        je .odemo
        cmp al, 7
        je .ohijo
        jmp .ayuda
.ohijo:
        mov ax, [0x16]              ; el PSP del padre: ¿es el lanzador?
        mov es, ax
        cmp dword [es:lz_firma], 'HVLZ'
        push cs
        pop es
        jne .arg
        mov [lz_seg], ax
        jmp .arg
.odemo:
        mov byte [op_demo], 1
        jmp .arg
.ovga:
        mov byte [op_vga], 1
        jmp .arg
.oancho:
        mov [op_ancho], dx
        jmp .arg
.obpp:
        mov [op_bpp], dl
        jmp .arg
.ocolor:
        mov byte [op_color], 1
        jmp .arg
.ofallo:
        mov byte [op_fallo], 1
        jmp .arg
.o4:
        pop si
        mov al, [bx]
        xor ah, ah
        add bx, ax
        add bx, 4
        jmp .o1
.ayuda:
        mov dx, m_ayuda
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C00
        int 0x21
.argfin:

        ; ---- lo que hace falta ----
        mov ax, 0x4300              ; ¿hay gestor XMS?
        int 0x2F
        cmp al, 0x80
        je .xms
        mov dx, m_noxms
        jmp morir
.xms:
        call hay_dpmi
        jnc .dpmi
        cmp word [lz_seg], 0        ; (el lanzador ya lo ha intentado)
        jne .sindpmi
        mov dx, m_carga_dpmi
        mov ah, 0x09
        int 0x21
        mov si, n_hspdpmi
        call cargar
        call hay_dpmi
        jnc .dpmi
.sindpmi:
        mov dx, m_nodpmi
        jmp morir
.dpmi:
        ; (a partir de aquí hay un 386 seguro: lo exige el servidor DPMI)
        call hay_raton
        jnc .raton
        cmp word [lz_seg], 0
        jne .raton
        mov si, n_hspraton
        call cargar
        call hay_raton
.raton:
        mov ah, 0x0F
        int 0x10
        mov [modo_ant], al
        call vid_detectar
        jnc .video
        mov dx, m_novideo
        jmp morir
.video:
        ; ---- a modo protegido ----
        mov bx, [dpmi_priv]
        xor ax, ax
        mov es, ax
        test bx, bx
        jz .sinpriv
        mov ah, 0x48
        int 0x21
        jc .nomem
        mov es, ax
.sinpriv:
        mov ax, 1                   ; cliente de 32 bits
        call far [dpmi_entrada]
        jnc .pm
.nomem:
        push cs
        pop es
        mov dx, m_nopm
        jmp morir
.pm:
        ; todavía en el segmento de código de 16 bits que da el servidor
        push ds
        pop es
        xor ax, ax
        mov cx, 2
        int 0x31
        jc .f
        mov [sel_cod], ax
        add ax, 8
        mov [sel_dat], ax
        mov bx, cs
        mov ax, 0x000B
        mov edi, desc
        int 0x31
        jc .f
        mov word [desc], 0xFFFF     ; código de 32 bits y 4 GB: los módulos
        or byte [desc+6], 0xCF      ; (.HVP) se cargan fuera del programa
        mov bx, [sel_cod]
        mov ax, 0x000C
        int 0x31
        jc .f
        mov bx, ds
        mov ax, 0x000B
        int 0x31
        jc .f
        mov word [desc], 0xFFFF     ; datos y pila: 4 GB, 32 bits, misma base
        or byte [desc+6], 0xCF
        mov bx, [sel_dat]
        mov ax, 0x000C
        int 0x31
        jc .f
        mov eax, [desc+2]
        and eax, 0x00FFFFFF
        movzx ebx, byte [desc+7]
        shl ebx, 24
        or eax, ebx
        mov [base], eax
        movzx esp, sp
        mov ax, [sel_dat]
        mov ss, ax
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        push word [cs:sel_cod]
        push word principal
        retf
.f:
        mov ax, 0x4C03
        int 0x21

morir:
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C01
        int 0x21

; ¿hay servidor DPMI con clientes de 32 bits? CF=1 si no
hay_dpmi:
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz .no
        test bl, 1
        jz .no
        mov [cs:dpmi_entrada], di
        mov [cs:dpmi_entrada+2], es
        mov [cs:dpmi_priv], si
        push cs
        pop es
        clc
        ret
.no:
        push cs
        pop es
        stc
        ret

; ¿hay driver de ratón (INT 33h)? CF=1 si no
hay_raton:
        xor ax, ax
        mov es, ax
        mov ax, [es:0x33*4]
        or ax, [es:0x33*4+2]
        push cs
        pop es
        jz .no
        xor ax, ax
        int 0x33
        cmp ax, 0xFFFF
        jne .no
        mov byte [hay_rat], 1
        clc
        ret
.no:
        stc
        ret

; Ejecuta el programa SI (nombre, acabado en 0) de la carpeta de VENTANAS.COM.
; Sirve para dejar residentes HSPDPMI y HSPRATON.
cargar:
        push si
        mov es, [0x2C]              ; entorno: tras el doble cero, la ruta propia
        xor di, di
        xor al, al
.e1:
        mov cx, 0xFFFF
        repne scasb
        cmp [es:di], al
        jne .e1
        add di, 3
        mov bx, ruta
        mov dx, bx
.c1:
        mov al, [es:di]
        inc di
        mov [bx], al
        inc bx
        cmp al, '\'
        jne .c2
        mov dx, bx
.c2:
        cmp bx, ruta + 100
        jae .c4
        test al, al
        jnz .c1
.c4:
        push cs
        pop es
        pop si
        mov di, dx
.c3:
        lodsb
        stosb
        test al, al
        jnz .c3
        mov [exec_blq+4], cs
        mov [exec_blq+8], cs
        mov [exec_blq+12], cs
        mov [exec_sp], sp
        mov dx, ruta
        mov bx, exec_blq
        mov ax, 0x4B00
        int 0x21
        cli
        mov ax, cs
        mov ss, ax
        mov sp, [cs:exec_sp]
        sti
        mov ds, ax
        mov es, ax
        cld
        ret

; ---- sondeo en modo real (se llama con la función 0301h del DPMI) ----
; Espera a la próxima interrupción si no hay nada que hacer, y recoge las
; teclas que haya en la cola de la BIOS (hasta 8).
rm_sondeo:
        push ds
        push cs
        pop ds
        cmp byte [son_esperar], 0
        je .ya
        mov ah, 0x11
        int 0x16
        jnz .ya
        cli
        mov al, [rc_cab]
        cmp al, [rc_cola]
        jne .ya
        sti
        hlt
.ya:
        sti
        mov byte [son_nteclas], 0
        mov di, son_teclas
.tecla:
        mov ah, 0x11
        int 0x16
        jz .fin
        mov ah, 0x10
        int 0x16
        mov [di], ax
        add di, 2
        inc byte [son_nteclas]
        cmp byte [son_nteclas], 8
        jb .tecla
.fin:
        pop ds
        retf

; ---- sucesos del ratón (la llama el driver: INT 33h, función 0Ch) ----
; AX=qué ha pasado BX=botones SI, DI=contadores de movimiento. Solo apunta.
rm_raton:
        push ds
        push bp
        push dx
        push cs
        pop ds
        movzx bp, byte [rc_cab]
        mov dx, bp
        inc dx
        and dx, RAT_COLA - 1
        cmp dl, [rc_cola]
        je .x                       ; cola llena
        shl bp, 3
        mov [ds:rat_cola+bp], ax
        mov [ds:rat_cola+bp+2], bx
        mov [ds:rat_cola+bp+4], si
        mov [ds:rat_cola+bp+6], di
        mov [rc_cab], dl
.x:
        pop dx
        pop bp
        pop ds
        retf

; ---- llamada al driver de sonido (HSPSND): AX y lo demás, en rmcs ----
rm_snd:
        call far [cs:snd_entrada]
        retf
; [CAMPANITA] (lo mismo con los datos en snd_tb, que está más allá de los 64 KB)
rm_snd2:
        mov ds, [cs:snd_tbseg]
        call far [cs:snd_entrada]
        retf

; ---- [Hispa-Nes] INT 15h: qué teclas están pulsadas ----
; La BIOS avisa de cada código que llega del teclado (AH=4Fh, AL=código; el
; bit 7 dice que se suelta) antes de traducirlo. Aquí solo se apunta, en
; tec_mapa (un octeto por tecla: lo leen los módulos que necesitan saber qué
; teclas siguen pulsadas, que con WM_KEYDOWN no se sabe), y se deja seguir.
rm_int15:
        cmp ah, 0x4F
        jne .c
        push bx
        mov bl, al
        and bx, 0x007F
        test al, 0x80
        setz [cs:tec_mapa+bx]
        pop bx
        stc
.c:
        jmp far [cs:tec_ant15]
tec_ant15   dd 0
tec_mapa    times 128 db 0

%include "video16.inc"

; =============================================================================
;  Modo protegido, 32 bits
; =============================================================================
        bits 32
principal:
        cld
        mov esp, pila_fin           ; la pila de modo protegido (tarea del sistema)
        mov edi, pila_fin           ; el resto de las variables, a cero
        mov ecx, bss_fin - pila_fin
        xor al, al
        rep stosb
        call exc_instalar
        mov eax, [base]
        neg eax
        mov [cero], eax             ; puntero a la dirección lineal 0
        call tec_instalar           ; [Hispa-Nes]
        ; la superficie de dibujo: un byte por punto
        mov eax, [vid_ancho]
        imul eax, [vid_alto]
        mov ecx, eax
        mov ebx, eax
        shr ebx, 16
        mov ax, 0x0501
        int 0x31
        jc .sinmem
        mov [sup_mango], di
        mov [sup_mango+2], si
        shl ebx, 16
        mov bx, cx
        sub ebx, [base]
        mov [sup_ptr], ebx
        call mem_iniciar
        jc .sinmem
        call vid_abrir
        jc .sinvideo
        mov byte [en_grafico], 1
        call gdi_iniciar
        call rat_iniciar
        call tar_iniciar
        call ges_iniciar
        call esc_iniciar
        call programas_iniciar
        cmp byte [op_fallo], 0
        je .bucle
        call sis_mensajes
        call ges_presentar
        mov eax, 0x0FF4             ; (prueba) fallo de protección en gráficos
        mov es, eax
.bucle:
        mov al, [actividad]         ; si no ha pasado nada, a dormir
        xor al, 1
        mov [son_esperar], al
        mov byte [actividad], 0
        call sondeo
        call eventos
        mov ecx, 4
.ronda:
        call sis_mensajes           ; el escritorio y los menús
        call tar_ceder              ; y un turno para cada programa
        call sis_mensajes
        call ges_pendiente
        jnc .pres
        dec ecx
        jnz .ronda
.pres:
        call tar_limpiar
%ifdef CON_PROGRAMAS
        call hvp_limpiar
        call cam_bomba              ; [CAMPANITA] la campanita del volumen
%endif
        call ges_presentar
        cmp byte [salir], 0
        je .bucle

%ifdef CON_PROGRAMAS
        cmp byte [op_demo], 0
        jne .sincfg
        call cfg_guardar            ; grupos abiertos y posiciones, a CENTRO.INI
.sincfg:
%endif
        call rat_quitar
        call vid_cerrar
        mov esi, m_adios
        xor al, al
        cmp byte [salir], 2         ; hay un encargo para el lanzador
        jne .fin
        mov esi, m_nada
        mov al, 0xFE
        jmp .fin
.sinmem:
        mov esi, m_nomem
        mov al, 1
        jmp .fin
.sinvideo:
        call vid_cerrar
        mov esi, m_novideo2
        mov al, 1
.fin:
        call tec_quitar             ; [Hispa-Nes]
        push eax
        call escribe
        cmp byte [hay_rat], 0
        je .f2
        xor eax, eax                ; el ratón, como estaba al entrar
        int 0x33
.f2:
        pop eax
        mov ah, 0x4C
        int 0x21

; lo que se pone en marcha al entrar
programas_iniciar:
        cmp byte [op_demo], 0
        jne esc_muestra
%ifdef CON_PROGRAMAS
        call cfg_cargar             ; CENTRO.INI: grupos, accesos y volumen
        call snd_iniciar
        mov eax, centro_tarea
        call tar_crear
%endif
        ret

; (sistema) despacha la cola de la tarea 0: el escritorio y los menús
sis_mensajes:
        push edi
        sub esp, M_TAM
        mov edi, esp
.l:
        call msg_mirar
        jc .x
        call msg_despachar
        jmp .l
.x:
        add esp, M_TAM
        pop edi
        ret

; (sistema) CF=1 si queda alguna ventana por pintar
ges_pendiente:
        push eax
        push ebx
        push ecx
        mov ebx, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebx+V_USO], 1
        jne .s
        test dword [ebx+V_ESTADO], S_NCSUCIO
        jnz .si
        mov eax, [ebx+V_UX0]
        cmp eax, [ebx+V_UX1]
        jl .si
.s:
        add ebx, V_TAM
        dec ecx
        jnz .b
        clc
        jmp .x
.si:
        stc
.x:
        pop ecx
        pop ebx
        pop eax
        ret

; escribe en la consola de DOS la cadena ESI (acabada en 0)
escribe:
        lodsb
        test al, al
        jz .x
        mov dl, al
        mov ah, 0x02
        int 0x21
        jmp escribe
.x:
        ret

; ---- llamada a modo real ----
; rm_llamar: llama (0301h) a la rutina de modo real de este programa que
; está en [rmcs+0x2A]. rm_int: BL=interrupción (0300h). Los registros van y
; vuelven en rmcs; aquí se ponen los segmentos y la pila (la del servidor).
rm_llamar:
        mov ax, 0x0301
        jmp rm_ir
rm_int:
        mov ax, 0x0300
rm_ir:
        push ecx
        push edi
        push eax
        mov ax, [rmseg]
        mov [rmcs+0x22], ax         ; ES
        mov [rmcs+0x24], ax         ; DS
        mov [rmcs+0x2C], ax         ; CS
        xor eax, eax
        mov [rmcs+0x20], ax         ; banderas
        mov [rmcs+0x2E], eax        ; SS:SP
        pop eax
        xor ecx, ecx
        mov bh, 0
        mov edi, rmcs
        int 0x31
        pop edi
        pop ecx
        ret

; ---- [Hispa-Nes] el vector de INT 15h de modo real: ponerlo y quitarlo ----
; (un vector es «segmento:desplazamiento» en la dirección lineal 15h*4)
tec_instalar:
        pushad
        mov eax, [cero]
        mov edx, [eax+0x15*4]
        mov [tec_ant15], edx
        movzx edx, word [rmseg]
        shl edx, 16
        mov dx, rm_int15
        mov [eax+0x15*4], edx
        mov byte [tec_puesto], 1
        popad
        ret
tec_quitar:
        cmp byte [tec_puesto], 0
        je .x
        push eax
        push edx
        mov eax, [cero]
        mov edx, [tec_ant15]
        mov [eax+0x15*4], edx
        mov byte [tec_puesto], 0
        pop edx
        pop eax
.x:
        ret

; ---- una vuelta de sondeo, en modo real ----
sondeo:
        pushad
        mov word [rmcs+0x2A], rm_sondeo
        call rm_llamar
        popad
        ret

; ---- reparto de lo que ha llegado: ratón (de la cola) y teclas ----
eventos:
        pushad
        mov eax, [cero]
        mov al, [eax+0x417]         ; mayúsculas, Ctrl y Alt (datos de la BIOS)
        mov [tec_mayus], al
        cmp byte [hay_rat], 0
        je .teclas
.r:
        movzx esi, byte [rc_cola]
        mov eax, esi
        cmp al, [rc_cab]
        je .teclas
        lea edi, [rat_cola+esi*8]
        mov ax, [edi+4]             ; lo que se ha movido desde el anterior
        mov cx, ax
        sub cx, [ult_si]
        mov [ult_si], ax
        movsx ecx, cx
        mov ax, [edi+6]
        mov dx, ax
        sub dx, [ult_di]
        mov [ult_di], ax
        movsx edx, dx
        mov eax, ecx                ; [Configuración] la velocidad del ratón
        mov ebx, aj_resto
        call rat_escala
        mov ecx, eax
        mov eax, edx
        add ebx, 4
        call rat_escala
        mov edx, eax
        mov bl, [edi+2]
        mov bh, bl
        and bx, 0x0201              ; BL=el izquierdo, BH=el derecho
        cmp byte [aj_rat_zurdo], 0  ; [Configuración] botones intercambiados
        je .diestro
        shr bh, 1
        shl bl, 1
        xchg bl, bh
.diestro:
        inc esi
        and esi, RAT_COLA - 1
        mov eax, esi
        mov [rc_cola], al
        mov eax, ecx
        or eax, edx
        jz .bot
        mov eax, ecx
        add eax, [rat_x]
        push ebx
        mov ebx, edx
        add ebx, [rat_y]
        call rat_mover              ; (acota a la pantalla)
        pop ebx
        mov byte [ev_movido], 1
        call ev_act                 ; [Configuración] (actividad y cuándo)
.bot:
        cmp bl, [rat_boton]
        jne .cambia
        cmp bh, [rat_der]
        jne .cambia
        mov al, [rc_cola]           ; varios movimientos seguidos cuentan por uno
        cmp al, [rc_cab]
        jne .r
.cambia:
        cmp byte [ev_movido], 0
        je .c2
        mov byte [ev_movido], 0
        push ebx
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_raton_mueve
        pop ebx
.c2:
        cmp bh, [rat_der]
        je .c3
        mov [rat_der], bh
        call ges_raton_der
.c3:
        cmp bl, [rat_boton]
        je .r
        mov [rat_boton], bl
        call ev_act                 ; [Configuración]
        test bl, bl
        jz .suelta
        call tics                   ; ¿doble clic?
        mov ecx, eax
        sub ecx, [dbl_tics]
        mov [dbl_tics], eax
        mov byte [rat_doble], 0
        cmp ecx, [aj_rat_doble]     ; [Configuración] (antes, 8 tics)
        ja .nd
        mov eax, [rat_x]
        sub eax, [dbl_x]
        add eax, 4
        cmp eax, 8
        ja .nd
        mov eax, [rat_y]
        sub eax, [dbl_y]
        add eax, 4
        cmp eax, 8
        ja .nd
        mov byte [rat_doble], 1
        sub dword [dbl_tics], 100
.nd:
        mov eax, [rat_x]
        mov ebx, [rat_y]
        mov [dbl_x], eax
        mov [dbl_y], ebx
        call ges_raton_pulsa
        jmp .r
.suelta:
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_raton_suelta
        jmp .r
.teclas:
        movzx ecx, byte [son_nteclas]
        mov esi, son_teclas
.t:
        test ecx, ecx
        jz .x
        lodsw
        call ges_tecla
        call ev_act                 ; [Configuración]
        dec ecx
        jmp .t
.x:
        mov byte [son_nteclas], 0
        popad
        ret

; ---- excepciones: antes de que el servidor escriba su mensaje, a texto ----
%macro EXC 1
exc_%1:
        call exc_texto
        jmp far dword [cs:exc_ant_%1]
%endmacro
        EXC 00
        EXC 06
        EXC 0C
        EXC 0D
exc_texto:
        pushad
        push ds
        push es
        mov ds, [cs:sel_dat]
        call tec_quitar             ; [Hispa-Nes]
        cmp byte [en_grafico], 0
        je .x
        mov byte [en_grafico], 0
        mov eax, 0x0003
        int 0x10
        cmp byte [hay_rat], 0
        je .x
        xor eax, eax                ; (y el driver de ratón suelta rm_raton)
        int 0x33
.x:
        pop es
        pop ds
        popad
        ret

%macro EXCI 1
        mov ax, 0x0202
        mov bl, 0x%1
        int 0x31
        mov [exc_ant_%1], edx
        mov [exc_ant_%1+4], cx
        mov ax, 0x0203
        mov bl, 0x%1
        mov cx, cs
        mov edx, exc_%1
        int 0x31
%endmacro
exc_instalar:
        EXCI 00
        EXCI 06
        EXCI 0C
        EXCI 0D
        ret

%include "video.inc"
%include "gdi.inc"
%include "dc.inc"
%include "raton.inc"
%include "nucleo.inc"
%include "gestor.inc"
%include "menus.inc"
%include "escritorio.inc"
%ifdef CON_PROGRAMAS
%include "controles.inc"
%include "dialogos.inc"
%include "dos.inc"
%include "config.inc"
%include "centro.inc"
%include "volumen.inc"
%include "bloc.inc"
%include "calc.inc"
%include "reloj.inc"
%include "api.inc"
%include "modulos.inc"
%include "ajustes.inc"              ; [Configuración]
%endif

; =============================================================================
;  Datos
; =============================================================================
; opciones: longitud, texto, clase (0 VGA, 1 ancho, 2 bits, 3 color, 4 ayuda,
; 5 prueba de fallo, 6 ventanas de muestra) y valor
opciones:
        db 3, "VGA", 0
        dw 0
        db 4, "1024", 1
        dw 1024
        db 3, "800", 1
        dw 800
        db 3, "640", 1
        dw 640
        db 1, "8", 2
        dw 8
        db 2, "15", 2
        dw 15
        db 2, "16", 2
        dw 16
        db 2, "24", 2
        dw 24
        db 2, "32", 2
        dw 32
        db 5, "COLOR", 3
        dw 0
        db 5, "FALLO", 5
        dw 0
        db 4, "DEMO", 6
        dw 0
        db 1, "@", 7                ; (interna: la pone el lanzador)
        dw 0
        db 1, "?", 4
        dw 0
        db 0

m_ayuda     db "Hispa Ventanas 0.3 - entorno gráfico de Hispa-DOS (etapa 3)", 13, 10
            db 13, 10
            db "VENTANAS [/1024 | /800 | /640] [/8 | /15 | /16 | /24 | /32 | /COLOR] [/VGA]", 13, 10
            db "         [/DEMO]", 13, 10
            db 13, 10
            db "  /1024 /800 /640  resolución VESA: 1024x768, 800x600 (la normal) o 640x480", 13, 10
            db "  /8 ... /32       bits por punto (lo normal: 8, es decir, 256 colores)", 13, 10
            db "  /COLOR           color verdadero: el modo con más colores que haya", 13, 10
            db "  /VGA             no usar VESA: VGA de 640x480 y 16 colores", 13, 10
            db "  /DEMO            solo las ventanas de muestra (Tab, flechas y Esc)", 13, 10
            db 13, 10
            db "Necesita HSPXMS.SYS (CONFIG.SYS) y HSPDPMI; con HSPRATON hay ratón.", 13, 10
            db "Dentro: doble clic en un icono del Centro de programas para abrirlo;", 13, 10
            db "Alt+F4 cierra la ventana, Ctrl+Tab pasa a otra, F10 abre el menú y el", 13, 10
            db "botón Salir de la barra de arriba vuelve a Hispa-DOS.", 13, 10, "$"
m_noxms     db "Hispa Ventanas necesita memoria extendida: añada a CONFIG.SYS la línea", 13, 10
            db "  DEVICE=A:\HISPADOS\HSPXMS.SYS", 13, 10
            db "(con la unidad de arranque) y reinicie.", 13, 10, "$"
m_carga_dpmi db "Hispa Ventanas: cargando el servidor DPMI (HSPDPMI)...", 13, 10, "$"
m_nodpmi    db "Hispa Ventanas necesita el servidor DPMI y no ha podido cargarlo.", 13, 10
            db "Ejecute antes HSPDPMI (está en \HISPADOS) o póngalo en AUTOEXEC.BAT.", 13, 10, "$"
m_novideo   db "Hispa Ventanas necesita una tarjeta VGA o VESA (VBE 2.0).", 13, 10, "$"
m_nopm      db "Hispa Ventanas: no se ha podido entrar en modo protegido.", 13, 10, "$"
m_nomem     db "Hispa Ventanas: no hay memoria extendida suficiente.", 13, 10, 0
m_novideo2  db "Hispa Ventanas: no se ha podido poner el modo gráfico.", 13, 10, 0
m_adios     db "Hispa Ventanas ha terminado.", 13, 10, 0
m_nada      db 0
n_hspdpmi   db "HSPDPMI.COM", 0
n_hspraton  db "HSPRATON.COM", 0
cola_vacia  db 0, 13
            align 2
exec_blq    dw 0, cola_vacia, 0, 0x5C, 0, 0x6C, 0
op_ancho    dw 800
op_bpp      db 0
son_esperar db 0

%include "datos.inc"

; ---- datos sin valor inicial de la parte de modo real (se ponen a cero al
; arrancar). Están en el archivo, como ceros, porque detrás va la parte alta
        align 16
bss_ini:
rmseg       resw 1
lz_seg      resw 1                  ; el PSP del lanzador (0: no hay)
env_seg     resw 1                  ; el segmento del entorno
snd_entrada resd 1                  ; punto de entrada de HSPSND (modo real)
snd_tbseg   resw 1                  ; [CAMPANITA] el segmento de snd_tb
exec_sp     resw 1
dpmi_entrada resd 1
dpmi_priv   resw 1
sel_cod     resw 1
sel_dat     resw 1
base        resd 1                  ; dirección lineal del programa
sup_mango   resd 1
modo_ant    resb 1
op_vga      resb 1
op_color    resb 1
op_fallo    resb 1
hay_rat     resb 1
en_grafico  resb 1
salir       resb 1
op_demo     resb 1
son_nteclas resb 1
rc_cab      resb 1                  ; cola de sucesos del ratón
rc_cola     resb 1
            alignb 2
son_teclas  resw 8
rat_cola    resb RAT_COLA * 8
            alignb 4
exc_ant_00  resb 8
exc_ant_06  resb 8
exc_ant_0C  resb 8
exc_ant_0D  resb 8
desc        resb 8
rmcs        resb 0x34
ruta        resb 128
%include "bss.inc"
            alignb 4
pila_rm:
            resb 1024               ; la pila del arranque, en modo real
pila_rm_fin:
; ---- hasta aquí, dentro de los primeros 64 KB (lo usa la parte de modo
; real); lo que sigue solo se usa en modo protegido, con punteros de 32 bits
; ---- la parte alta: los dibujos, la fuente y el código de 32 bits que no
; cabe abajo (nada de esto se puede nombrar desde el código de 16 bits)
        align 16
alto_ini:
%include "iconos.inc"
%include "iconos2.inc"
%include "fuente.inc"
        bits 32
%include "accesos.inc"
%include "apagado.inc"                ; [APAGAR] salir: volver, reiniciar o apagar
        align 16
fin_archivo:
        absolute $
            resb 4096
pila_fin:
%include "bss2.inc"
%ifdef CON_PROGRAMAS
%include "bss3.inc"
%endif
            alignb 16
bss_fin:
