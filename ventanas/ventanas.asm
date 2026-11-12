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
;
;  La entrada llega por interrupciones: el driver de ratón llama a rm_raton
;  (INT 33h, función 0Ch), que apunta cada suceso en una cola; las teclas las
;  encola la BIOS (IRQ 1, con el teclado que haya dejado KEYB). El bucle del
;  sistema duerme con HLT en modo real hasta la siguiente interrupción.
; =============================================================================
        cpu 386
        bits 16
        org 0x100
%define CON_PROGRAMAS
%include "defs.inc"
RAT_COLA    equ 64                  ; sucesos de ratón que caben en la cola

inicio:
        cld
        mov sp, pila_fin
        mov di, bss_ini
        mov cx, pila_fin - bss_ini
        xor al, al
        rep stosb
        mov [rmseg], cs
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
        jmp .ayuda
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
        mov dx, m_carga_dpmi
        mov ah, 0x09
        int 0x21
        mov si, n_hspdpmi
        call cargar
        call hay_dpmi
        jnc .dpmi
        mov dx, m_nodpmi
        jmp morir
.dpmi:
        ; (a partir de aquí hay un 386 seguro: lo exige el servidor DPMI)
        call hay_raton
        jnc .raton
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
        or byte [desc+6], 0x40      ; código de 32 bits
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

%include "video16.inc"

; =============================================================================
;  Modo protegido, 32 bits
; =============================================================================
        bits 32
principal:
        cld
        mov edi, pila_fin           ; el resto de las variables, a cero
        mov ecx, bss_fin - pila_fin
        xor al, al
        rep stosb
        call exc_instalar
        mov eax, [base]
        neg eax
        mov [cero], eax             ; puntero a la dirección lineal 0
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
        call ges_presentar
        cmp byte [salir], 0
        je .bucle

        call rat_quitar
        call vid_cerrar
        mov esi, m_adios
        xor al, al
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
        mov bl, [edi+2]
        and bl, 1
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
        mov byte [actividad], 1
.bot:
        cmp bl, [rat_boton]
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
        cmp bl, [rat_boton]
        je .r
        mov [rat_boton], bl
        mov byte [actividad], 1
        test bl, bl
        jz .suelta
        call tics                   ; ¿doble clic?
        mov ecx, eax
        sub ecx, [dbl_tics]
        mov [dbl_tics], eax
        mov byte [rat_doble], 0
        cmp ecx, 8
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
        mov byte [actividad], 1
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
%include "centro.inc"
%include "bloc.inc"
%include "calc.inc"
%include "reloj.inc"
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
n_hspdpmi   db "HSPDPMI.COM", 0
n_hspraton  db "HSPRATON.COM", 0
cola_vacia  db 0, 13
            align 2
exec_blq    dw 0, cola_vacia, 0, 0x5C, 0, 0x6C, 0
op_ancho    dw 800
op_bpp      db 0
son_esperar db 0

%include "datos.inc"
%include "iconos.inc"
%include "fuente.inc"

; ---- datos sin valor inicial (se ponen a cero al arrancar) ----
        align 16
        absolute $
bss_ini:
rmseg       resw 1
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
            resb 4096
pila_fin:
%include "bss2.inc"
%ifdef CON_PROGRAMAS
%include "bss3.inc"
%endif
            alignb 16
bss_fin:
