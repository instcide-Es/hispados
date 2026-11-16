; =============================================================================
;  Hispa Ventanas - Reloj
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Dos formas, que se eligen en «Opciones»:
;   - Esfera con agujas: un círculo de verdad (discos llenos, fila a fila)
;     con aro de dos tonos y su sombra, las doce marcas de hora (más largas
;     y gruesas las de las 12, 3, 6 y 9), las sesenta de minuto y los números
;     cuando el tamaño lo permite; agujas con forma de lanza y sombra, que se
;     mueven como en un reloj de verdad (la de las horas avanza con los
;     minutos y la de los minutos, con los segundos); segundero fino con
;     contrapeso; y, debajo, la fecha.
;   - Solo las cifras: un reloj de mesa. Un panel hundido con cifras de
;     siete segmentos biselados dibujadas aquí (crecen con la ventana), los
;     segmentos apagados a la vista, muy tenues; los dos puntos laten cada
;     segundo; los segundos, más pequeños, al lado; en 12 horas, «AM»/«PM»;
;     y debajo, el día de la semana y la fecha completa.
;  Todo se adapta al tamaño de la ventana y lo que no cabe, no se pone: la
;  fecha se acorta («lunes 16 nov 2026», «lun 16 nov») o se quita; la esfera
;  pierde los números y luego las marcas de minuto; y si no hay sitio ni para
;  unas cifras pequeñas, la hora va con la letra de la casa.
;
;  Opciones (diálogo): segundero sí o no, esfera o cifras y formato de 12
;  horas (para las cifras). Menú «Tema»: tres juegos de colores (Rescoldo,
;  Claro y Noche), todos con los 16 colores de la casa, así que valen también
;  en la VGA de 16 colores. Las opciones NO se guardan: valen mientras Hispa
;  Ventanas esté en marcha (el Reloj no tiene archivo de configuración).
;
;  Cómo se pinta: el fondo no lo borra la clase (SIN_FONDO), lo pone el propio
;  Reloj con el color del tema. Como todo se dibuja en la superficie de Hispa
;  Ventanas y a la pantalla solo va el resultado, no parpadea. Un temporizador
;  de medio segundo mira la hora (INT 21h, 2Ch) y la fecha (2Ah, que da también
;  el día de la semana) y solo se repinta cuando cambia lo que se ve: cada
;  segundo con segundero; sin él, cada diez segundos la esfera y cada minuto
;  las cifras. El latido de los dos puntos repinta solo su trocito.
;
;  Sin coprocesador: los ángulos van en 3600 partes por vuelta (0 arriba,
;  hacia la derecha), y el seno sale de una tabla de un cuarto de vuelta, de
;  3 en 3 grados, interpolando (por 16384). Las agujas y las marcas gruesas
;  son polígonos convexos que se rellenan fila a fila.
; =============================================================================

c_reloj     dd reloj_proc
            db SIN_FONDO, 0, 0, 0
reloj_plant:
        PLANTILLA c_reloj, t_reloj, E_NORMAL, 560, 330, 200, 252, 0
ro_ventana:
        PLANTILLA c_dialogo, t_reloj_op, E_DIALOGO, 0, 0, 264, 180, 0
ro_hijos:
        PLANTILLA c_boton, t_ro_seg, E_HIJA | E_TABULA | BS_CASILLA, 20, 14, 220, 20, 20
        PLANTILLA c_boton, t_ro_agujas, E_HIJA | E_TABULA | BS_OPCION, 20, 44, 220, 20, 21
        PLANTILLA c_boton, t_ro_cifras, E_HIJA | E_TABULA | BS_OPCION, 20, 68, 220, 20, 22
        PLANTILLA c_boton, rl_t_12h, E_HIJA | E_TABULA | BS_CASILLA, 20, 94, 220, 20, 23
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 44, 122, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 136, 122, 80, 26, ID_CANCELAR
        dd 0
; (el menú «Tema» lleva delante de cada nombre un hueco donde rl_poner_tema
; pone la marca «»» del que está elegido: los menús no tienen marcas propias)
RL_MARCA    equ 0xAF
menu_reloj:
        db "&Reloj", 0
        dw 301
        db "&Opciones...", 0
        dw 0
        db 0
        dw 302
        db "&Salir", 0
        dw 0xFFFF
        db "&Tema", 0
        dw 310
rl_mt0  db RL_MARCA, " &Rescoldo", 0
        dw 311
rl_mt1  db "  &Claro", 0
        dw 312
rl_mt2  db "  &Noche", 0
        dw 0xFFFF
        db 0
rl_mts      dd rl_mt0, rl_mt1, rl_mt2
rl_t_12h    db "Formato de 12 horas", 0
rl_t_am     db "AM", 0
rl_t_pm     db "PM", 0
rl_t_de     db " de ", 0
rl_dias     db "domingo", 0, "lunes", 0, "martes", 0, "miércoles", 0
            db "jueves", 0, "viernes", 0, "sábado", 0
rl_meses    db "enero", 0, "febrero", 0, "marzo", 0, "abril", 0, "mayo", 0
            db "junio", 0, "julio", 0, "agosto", 0, "septiembre", 0
            db "octubre", 0, "noviembre", 0, "diciembre", 0

; ---- las opciones (no se guardan en ningún archivo) ----
reloj_seg   db 1                    ; con aguja de segundos (y segundos en las cifras)
reloj_cifras db 0                   ; solo las cifras
reloj_12h   db 0                    ; las cifras, de 12 horas
reloj_tema  db 0                    ; 0 Rescoldo, 1 Claro, 2 Noche

; ---- los temas: un color por cada cosa que se pinta ----
RT_FONDO     equ 0                   ; el fondo de la ventana
RT_SOMBRA    equ 1                   ; la sombra de la esfera sobre el fondo
RT_ARO1      equ 2                   ; el aro: filete de fuera,
RT_ARO2      equ 3                   ; banda
RT_ARO3      equ 4                   ; y filete de dentro
RT_CARA      equ 5                   ; la cara de la esfera
RT_MARCA     equ 6                   ; marcas de hora y números
RT_MINUTO    equ 7                   ; marcas de minuto
RT_AGUJA     equ 8                   ; agujas de horas y minutos
RT_ASOMB     equ 9                   ; su sombra sobre la cara
RT_SEG       equ 10                  ; segundero
RT_EJE       equ 11                  ; el punto del eje
RT_FECHA     equ 12                  ; la fecha, debajo de la esfera
RT_PANEL     equ 13                  ; cifras: el panel
RT_PSOM      equ 14                  ; su bisel de arriba y de la izquierda
RT_PLUZ      equ 15                  ; y el de abajo y de la derecha
RT_ENC       equ 16                  ; segmento encendido
RT_APAG      equ 17                  ; segmento apagado
RT_PTXT      equ 18                  ; la fecha, dentro del panel
RT_ACENTO    equ 19                  ; «AM»/«PM»
RT_TAM       equ 20
rl_temas:
        ; Rescoldo: esfera crema con tinta y terracota; cifras ámbar sobre tinta
        db C_CREMA, C_PIEDRA, C_MARRON, C_ARENA, C_ARENA_OSC, C_CREMA, C_MARRON
        db C_ARENA_OSC, C_TINTA, C_ARENA, C_TERRACOTA, C_OCRE, C_MARRON
        db C_TINTA, C_ARENA_OSC, C_BLANCO, C_OCRE, C_MARRON, C_ARENA, C_TERRACOTA_CL
        ; Claro: esfera blanca con petróleo; cifras de tinta sobre piedra
        db C_CREMA, C_PIEDRA, C_PETROLEO, C_PETROLEO_CL, C_PETROLEO, C_BLANCO, C_PETROLEO
        db C_ARENA_OSC, C_PETROLEO, C_PIEDRA, C_TERRACOTA, C_CREMA, C_PETROLEO
        db C_PIEDRA, C_ARENA_OSC, C_BLANCO, C_TINTA, C_ARENA, C_MARRON, C_PETROLEO
        ; Noche: todo oscuro; agujas crema y segundero ocre; cifras terracota
        db C_TINTA, C_TINTA, C_ARENA_OSC, C_MARRON, C_TINTA, C_MARRON, C_ARENA
        db C_ARENA_OSC, C_CREMA, C_TINTA, C_OCRE, C_TERRACOTA, C_ARENA
        db C_TINTA, C_MARRON, C_MARRON, C_TERRACOTA_CL, C_MARRON, C_ARENA_OSC, C_OCRE
rl_tema     dd rl_temas             ; el tema en uso

; pone el color del campo %1 del tema en uso
%macro TINTE 1
        push dword %1
        call rl_tinte
%endmacro
; EAX = radio de la cara por %1/256 (las medidas van en proporción a ella)
%macro PROP 1
        imul eax, [rl_rf], %1
        shr eax, 8
%endmacro

; seno por 16384 de 0 a 90 grados, de 3 en 3 (y uno más, para interpolar)
rl_senos    dw 0, 857, 1713, 2563, 3406, 4240, 5063, 5872, 6664, 7438, 8192
            dw 8923, 9630, 10311, 10963, 11585, 12176, 12733, 13255, 13741
            dw 14189, 14598, 14968, 15296, 15582, 15826, 16026, 16182, 16294
            dw 16362, 16384, 16384

; cifras de siete segmentos: los que enciende cada una (bit 0 = a, el de
; arriba; b y c, los de la derecha; d, abajo; e y f, la izquierda; g, el del
; medio); la 10 es el hueco (todo apagado)
rl_mascaras db 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0
; cada segmento: tumbado (0: x0, x1, y) o de pie (1: x, y0, y1), con los
; números de sus coordenadas en rl_co (0 izquierda, 1 derecha, 2 arriba,
; 3 medio, 4 abajo)
rl_segs     db 0, 0, 1, 2           ; a
            db 1, 1, 2, 3           ; b
            db 1, 1, 3, 4           ; c
            db 0, 0, 1, 4           ; d
            db 1, 0, 3, 4           ; e
            db 1, 0, 2, 3           ; f
            db 0, 0, 1, 3           ; g

; ---- variables (aquí, con valor 0, para no depender de otro archivo) ----
            align 4
rl_w        dd 0                    ; la zona de la ventana
rl_h        dd 0
rl_rf       dd 0                    ; radio de la cara (dentro del aro)
rl_to       dd 0                    ; radio donde empiezan las marcas
rl_lm       dd 0                    ; largo de la marca de minuto
rl_lh       dd 0                    ; de la de hora
rl_lq       dd 0                    ; y de las de las 12, 3, 6 y 9
rl_ang      dd 0                    ; ángulo de lo que se dibuja
rl_gro      dd 0                    ; medio grosor de una barra
rl_amin     dd 0                    ; ángulo del minutero
rl_ahor     dd 0                    ; y de la aguja de las horas
rl_fh       dd 0                    ; alto que se lleva la fecha
rl_esc      dd 1                    ; tamaño del texto (1, 2, 3...)
rl_np       dd 0                    ; polígono: puntos
rl_pts      times 16 dd 0           ; x, y de cada uno (hasta 8)
rl_lo       dd 0                    ; tramo de una fila del polígono
rl_hi       dd 0
rl_dr       dd 0                    ; disco: radio
rl_dw       dd 0                    ; y medio ancho de la fila
rl_ult      dd 0                    ; lo último que se pintó (rl_firma)
; cifras
rl_pm       dd 0                    ; margen del panel
rl_pw       dd 0                    ; su ancho y su alto
rl_ph       dd 0
rl_aw       dd 0                    ; sitio útil dentro
rl_ah       dd 0
rl_cdw      dd 0                    ; ancho de una cifra grande
rl_cdh      dd 0                    ; y su alto
rl_g        dd 0                    ; hueco entre cifras
rl_cw       dd 0                    ; ancho de los dos puntos
rl_sdw      dd 0                    ; ancho de una cifra pequeña (segundos)
rl_sg       dd 0                    ; y su hueco
rl_tot      dd 0                    ; ancho de todo
rl_x0       dd 0                    ; esquina de las cifras
rl_y0       dd 0
rl_hueco    dd 0                    ; de las cifras a la fecha
rl_sh       dd 0                    ; medio grosor de un segmento
rl_co       times 5 dd 0            ; coordenadas de la cifra que se dibuja
rl_ppx      dd 0                    ; dónde están los dos puntos (para repintarlos)
rl_ppy      dd 0
rl_ppan     dd 0
rl_ppal     dd 0
rl_anio     dw 0                    ; la fecha de DOS
rl_mes      db 0
rl_dia      db 0
rl_dsem     db 0                    ; día de la semana (0 = domingo)
rl_pp       db 1                    ; los dos puntos, encendidos (primera mitad del segundo)
rl_ppv      db 1                    ; y cómo se pintaron
rl_col      db 0                    ; hay columna a la derecha (segundos, AM/PM)
rl_hayf     db 0                    ; se pone la fecha
rl_tarde    db 0                    ; es PM
rl_dig      times 6 db 0            ; las cifras: HH MM SS
rl_ftxt     times 48 db 0           ; la fecha, en letra

; gancho del diálogo de opciones
ro_gancho:
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        mov ecx, 20
        call ro_marca
        mov [reloj_seg], al
        mov ecx, 22
        call ro_marca
        mov [reloj_cifras], al
        mov ecx, 23
        call ro_marca
        mov [reloj_12h], al
        jmp .no
.inicio:
        mov ecx, 20
        movzx edx, byte [reloj_seg]
        call ro_poner
        mov ecx, 23
        movzx edx, byte [reloj_12h]
        call ro_poner
        movzx edx, byte [reloj_cifras]
        mov ecx, 22
        call ro_poner
        xor edx, 1
        mov ecx, 21
        call ro_poner
.no:
        xor eax, eax
        ret
; (internas) marca del control ECX del diálogo EBP: leerla (AL) y ponerla (EDX)
ro_marca:
        push ebp
        call ven_hija_id
        mov ebp, eax
        mov eax, BM_MARCA
        call msg_enviar
        pop ebp
        ret
ro_poner:
        push ebp
        push ecx
        call ven_hija_id
        mov ebp, eax
        mov eax, BM_PONER_MARCA
        mov ecx, edx
        call msg_enviar
        pop ecx
        pop ebp
        ret

reloj_tarea:
        mov esi, reloj_plant
        xor ebp, ebp
        call ven_crear
        mov [reloj_ven], eax
        jmp app_bucle

; (interna) el tema reloj_tema: su tabla de colores y su marca en el menú
rl_poner_tema:
        pushad
        movzx eax, byte [reloj_tema]
        imul ebx, eax, RT_TAM
        add ebx, rl_temas
        mov [rl_tema], ebx
        mov byte [rl_mt0], ' '
        mov byte [rl_mt1], ' '
        mov byte [rl_mt2], ' '
        mov ebx, [rl_mts+eax*4]
        mov byte [ebx], RL_MARCA
        popad
        ret

; (interna) el color del campo que va en la pila (macro TINTE)
rl_tinte:
        push eax
        mov eax, [esp+8]
        add eax, [rl_tema]
        mov al, [eax]
        mov [gdi_color], al
        pop eax
        ret 4

; -----------------------------------------------------------------------------
;  Trigonometría y figuras
; -----------------------------------------------------------------------------
; (interna) EAX=ángulo (3600 por vuelta, 0 arriba, hacia la derecha; sin
; signo) -> EAX=seno por 16384
rl_seno:
        push ebx
        push ecx
        push edx
        xor edx, edx
        mov ebx, 3600
        div ebx
        mov eax, edx
        xor edx, edx
        mov ebx, 900
        div ebx                     ; EAX=cuarto EDX=dentro del cuarto
        mov ecx, eax
        test cl, 1
        jz .a
        neg edx
        add edx, 900
.a:
        mov eax, edx
        xor edx, edx
        mov ebx, 30
        div ebx                     ; EAX=tramo de 3 grados EDX=dentro de él
        push ecx
        movzx ecx, word [rl_senos+eax*2]
        movzx eax, word [rl_senos+eax*2+2]
        sub eax, ecx
        imul eax, edx
        xor edx, edx
        div ebx
        add eax, ecx
        pop ecx
        test cl, 2
        jz .b
        neg eax
.b:
        pop edx
        pop ecx
        pop ebx
        ret

; (interna) EAX=ángulo ECX=a lo largo EDX=de lado (a la derecha de la
; dirección) -> EAX=x EBX=y, desde el centro (reloj_cx, reloj_cy)
rl_pt:
        push ecx
        push edx
        push esi
        push edi
        mov esi, eax
        call rl_seno
        mov edi, eax                ; seno
        lea eax, [esi+900]
        call rl_seno
        mov esi, eax                ; coseno
        mov eax, ecx
        imul eax, edi
        mov ebx, edx
        imul ebx, esi
        add eax, ebx
        add eax, 8192               ; (redondeo)
        sar eax, 14
        add eax, [reloj_cx]
        mov ebx, edx
        imul ebx, edi
        imul ecx, esi
        sub ebx, ecx
        add ebx, 8192
        sar ebx, 14
        add ebx, [reloj_cy]
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

; (interna) añade al polígono el punto ECX (a lo largo), EDX (de lado) de la
; dirección rl_ang
rl_v:
        push eax
        push ebx
        push edi
        mov eax, [rl_ang]
        call rl_pt
        mov edi, [rl_np]
        mov [rl_pts+edi*8], eax
        mov [rl_pts+edi*8+4], ebx
        inc dword [rl_np]
        pop edi
        pop ebx
        pop eax
        ret

; (interna) rellena el polígono convexo rl_pts (rl_np puntos): en cada fila,
; del corte más a la izquierda al más a la derecha
rl_poli:
        pushad
        mov esi, rl_pts
        mov edi, [esi+4]            ; fila más alta
        mov ebp, edi                ; y más baja
        mov edx, [rl_np]
.mm:
        mov eax, [esi+4]
        cmp eax, edi
        jge .m1
        mov edi, eax
.m1:
        cmp eax, ebp
        jle .m2
        mov ebp, eax
.m2:
        add esi, 8
        dec edx
        jnz .mm
.fila:
        cmp edi, ebp
        jg .x
        mov dword [rl_lo], 0x7FFFFFFF
        mov dword [rl_hi], 0x80000000
        xor esi, esi                ; cada lado: del punto ESI al siguiente (EBX)
.lado:
        lea ebx, [esi+1]
        cmp ebx, [rl_np]
        jb .l1
        xor ebx, ebx
.l1:
        mov eax, edi
        sub eax, [rl_pts+esi*8+4]   ; fila - y0
        mov ecx, edi
        sub ecx, [rl_pts+ebx*8+4]   ; fila - y1
        test eax, eax
        jnz .l2
        mov edx, [rl_pts+esi*8]     ; el punto ESI está en la fila
        call .une
.l2:
        test ecx, ecx
        jnz .l3
        mov edx, [rl_pts+ebx*8]     ; y el siguiente
        call .une
        jmp .sig
.l3:
        test eax, eax
        jz .sig
        mov edx, eax
        xor edx, ecx
        jns .sig                    ; los dos al mismo lado: no la corta
        mov edx, [rl_pts+ebx*8]
        sub edx, [rl_pts+esi*8]
        imul edx, eax               ; (x1 - x0) (fila - y0)
        sub eax, ecx                ; y1 - y0
        mov ecx, eax
        mov eax, edx
        test ecx, ecx
        jns .l4
        neg ecx
        neg eax
.l4:
        mov edx, ecx                ; redondeo al más cercano
        shr edx, 1
        test eax, eax
        jns .l5
        neg edx
.l5:
        add eax, edx
        cdq
        idiv ecx
        add eax, [rl_pts+esi*8]
        mov edx, eax
        call .une
.sig:
        inc esi
        cmp esi, [rl_np]
        jb .lado
        mov eax, [rl_lo]
        mov ecx, [rl_hi]
        sub ecx, eax
        jl .nf
        inc ecx
        mov ebx, edi
        mov edx, 1
        call dc_rect
.nf:
        inc edi
        jmp .fila
.x:
        popad
        ret
.une:
        cmp edx, [rl_lo]
        jge .u1
        mov [rl_lo], edx
.u1:
        cmp edx, [rl_hi]
        jle .u2
        mov [rl_hi], edx
.u2:
        ret

; (interna) disco lleno: EAX=x EBX=y del centro, ECX=radio. Fila a fila; el
; medio ancho (raíz de r² + r - dy²) se va corrigiendo de una fila a la otra,
; sin calcular raíces. Con el «+ r» el círculo sale redondo, sin picos.
rl_disco:
        pushad
        test ecx, ecx
        js .x
        mov [rl_dr], ecx
        mov dword [rl_dw], 0
        mov esi, eax
        mov edi, ecx
        imul edi, ecx
        add edi, ecx                ; r² + r
        sub ebx, ecx                ; la primera fila
        mov ebp, ecx
        neg ebp                     ; dy, de -r a r
.f:
        mov eax, ebp
        imul eax, eax
        neg eax
        add eax, edi
        mov ecx, [rl_dw]
.sube:
        lea edx, [ecx+1]
        imul edx, edx
        cmp edx, eax
        ja .baja
        inc ecx
        jmp .sube
.baja:
        mov edx, ecx
        imul edx, edx
        cmp edx, eax
        jbe .ya
        dec ecx
        jmp .baja
.ya:
        mov [rl_dw], ecx
        mov eax, esi
        sub eax, ecx
        lea ecx, [ecx*2+1]
        mov edx, 1
        call dc_rect
        inc ebx
        inc ebp
        cmp ebp, [rl_dr]
        jle .f
.x:
        popad
        ret

; (interna) barra en la dirección rl_ang, del radio ECX al EDX, de medio
; grosor rl_gro (0: una raya de un punto)
rl_barra:
        pushad
        mov esi, [rl_gro]
        test esi, esi
        jnz .p
        mov edi, edx
        xor edx, edx
        mov eax, [rl_ang]
        call rl_pt
        push eax
        push ebx
        mov ecx, edi
        mov eax, [rl_ang]
        call rl_pt
        mov ecx, eax
        mov edx, ebx
        pop ebx
        pop eax
        call dc_linea
        popad
        ret
.p:
        mov dword [rl_np], 0
        mov edi, edx
        mov edx, esi
        neg edx
        call rl_v
        neg edx
        call rl_v
        mov ecx, edi
        call rl_v
        neg edx
        call rl_v
        call rl_poli
        popad
        ret

; (interna) aguja con forma de lanza en la dirección rl_ang: ECX=largo
; EDX=cola (lo que asoma por detrás del eje) ESI=medio ancho en lo más ancho
; (a cinco octavos del largo); la cola es algo más estrecha y acaba en punta
rl_lanza:
        pushad
        mov dword [rl_np], 0
        mov edi, ecx
        lea ebp, [esi*2]
        xor eax, eax
        xchg eax, ebp
        push edx
        xor edx, edx
        mov ebx, 3
        div ebx
        pop edx
        mov ebp, eax                ; dos tercios
        test ebp, ebp
        jnz .a
        inc ebp
.a:
        mov ecx, edx
        neg ecx
        mov edx, ebp
        neg edx
        call rl_v                   ; la cola
        neg edx
        call rl_v
        lea ecx, [edi*4+edi]
        shr ecx, 3
        mov edx, esi
        call rl_v                   ; lo más ancho
        mov ecx, edi
        xor edx, edx
        call rl_v                   ; la punta
        lea ecx, [edi*4+edi]
        shr ecx, 3
        mov edx, esi
        neg edx
        call rl_v
        call rl_poli
        popad
        ret

; (interna) texto con la fuente de la casa a tamaño rl_esc (cada punto, un
; cuadrado de rl_esc de lado): EAX=x EBX=y ESI=texto. Con gdi_negrita.
rl_texto:
        pushad
        cmp dword [rl_esc], 1
        ja .c
        call dc_texto
        popad
        ret
.c:
        movzx edx, byte [esi]
        inc esi
        test edx, edx
        jz .x
        shl edx, 4
        lea edi, [fuente8x16+edx]
        push ebx
        mov ch, 16
.fila:
        movzx edx, byte [edi]
        inc edi
        shl edx, 1                  ; 9 columnas: bit 8 = la primera
        cmp byte [gdi_negrita], 0
        je .f1
        mov ebp, edx
        shr ebp, 1
        or edx, ebp
.f1:
        push eax
.bit:
        test edx, 0x1FF
        jz .fb
        test edx, 0x100
        jz .nb
        push ecx
        push edx
        mov ecx, [rl_esc]
        mov edx, ecx
        call dc_rect
        pop edx
        pop ecx
.nb:
        add eax, [rl_esc]
        shl edx, 1
        jmp .bit
.fb:
        pop eax
        add ebx, [rl_esc]
        dec ch
        jnz .fila
        pop ebx
        mov edx, [rl_esc]
        shl edx, 3
        add eax, edx
        jmp .c
.x:
        popad
        ret

; (interna) lo mismo, centrado en la x EAX
rl_centra:
        push eax
        push edx
        mov edx, eax
        call gdi_ancho_texto
        imul eax, [rl_esc]
        shr eax, 1
        sub edx, eax
        mov eax, edx
        call rl_texto
        pop edx
        pop eax
        ret

; -----------------------------------------------------------------------------
;  La fecha en letra
; -----------------------------------------------------------------------------
; (interna) AL=forma: 0 «lunes, 16 de noviembre de 2026», 1 «lunes 16 nov
; 2026», 2 «lun 16 nov» -> en rl_ftxt; EAX=letras
rl_fecha:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov bl, al
        mov edi, rl_ftxt
        mov esi, rl_dias
        movzx ecx, byte [rl_dsem]
        cmp ecx, 6
        jbe .d1
        xor ecx, ecx
.d1:
        call .salta
        mov ecx, 3
        cmp bl, 2
        je .d2
        mov ecx, 40
.d2:
        call .copia
        test bl, bl
        jnz .d3
        mov al, ','
        stosb
.d3:
        mov al, ' '
        stosb
        movzx eax, byte [rl_dia]
        call .numero
        call .de
        mov esi, rl_meses
        movzx ecx, byte [rl_mes]
        dec ecx
        cmp ecx, 11
        jbe .d4
        xor ecx, ecx
.d4:
        call .salta
        mov ecx, 3
        test bl, bl
        jnz .d5
        mov ecx, 40
.d5:
        call .copia
        cmp bl, 2
        je .fin
        call .de
        movzx eax, word [rl_anio]
        call .numero
.fin:
        mov byte [edi], 0
        mov eax, edi
        sub eax, rl_ftxt
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; « de » en la forma larga y un espacio en las otras
.de:
        test bl, bl
        jz .de1
        mov al, ' '
        stosb
        ret
.de1:
        mov esi, rl_t_de
        mov ecx, 40
        jmp .copia
; ESI=lista de palabras acabadas en 0: la número ECX
.salta:
        test ecx, ecx
        jz .s2
.s1:
        lodsb
        test al, al
        jnz .s1
        dec ecx
        jnz .s1
.s2:
        ret
; copia la palabra ESI (como mucho ECX letras)
.copia:
        lodsb
        test al, al
        jz .c1
        stosb
        dec ecx
        jnz .copia
.c1:
        ret
; EAX en cifras, sin ceros delante
.numero:
        xor ecx, ecx
        mov esi, 10
.n1:
        xor edx, edx
        div esi
        push edx
        inc ecx
        test eax, eax
        jnz .n1
.n2:
        pop eax
        add al, '0'
        stosb
        dec ecx
        jnz .n2
        ret

; (interna) la forma más larga de la fecha que quepa en ECX puntos al tamaño
; rl_esc, en rl_ftxt. CF=1: no cabe ninguna
rl_fecha_cabe:
        push eax
        push edx
        xor edx, edx
.p:
        mov al, dl
        call rl_fecha
        imul eax, [rl_esc]
        shl eax, 3
        cmp eax, ecx
        jle .si
        inc edx
        cmp edx, 3
        jb .p
        stc
        jmp .x
.si:
        clc
.x:
        pop edx
        pop eax
        ret

; -----------------------------------------------------------------------------
;  La ventana
; -----------------------------------------------------------------------------
reloj_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_TIMER
        je .tic
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_SIZE
        je .inv
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov dword [reloj_ven], 0
        call msg_salir
        jmp .nada
.orden:
        cmp ecx, 302
        jne .o1
        call ven_destruir
        jmp .nada
.o1:
        cmp ecx, 301
        jne .o2
        mov esi, ro_ventana
        mov edi, ro_hijos
        mov ebx, ro_gancho
        xor edx, edx
        call dlg_modal
        jmp .inv
.o2:
        mov eax, ecx                ; 310 a 312: el tema
        sub eax, 310
        cmp eax, 2
        ja .nada
        mov [reloj_tema], al
        call rl_poner_tema
        jmp .inv
.crear:
        mov dword [ebp+V_ICONO], ico_reloj
        mov dword [ebp+V_MENU], menu_reloj
        call rl_poner_tema
        mov ecx, 1
        mov edx, 500
        call tmp_poner
        call reloj_hora
        call rl_firma
        mov [rl_ult], eax
        jmp .nada
.tic:
        call reloj_hora
        call rl_firma
        cmp eax, [rl_ult]
        jne .inv
        cmp byte [reloj_cifras], 0  ; lo mismo que hay pintado: solo el
        je .nada                    ; latido de los dos puntos
        mov al, [rl_pp]
        cmp al, [rl_ppv]
        je .nada
        mov eax, [rl_ppx]
        mov ebx, [rl_ppy]
        mov ecx, [rl_ppan]
        mov edx, [rl_ppal]
        call ven_invalidar_rect
        jmp .nada
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        pushad
        call rl_firma
        mov [rl_ult], eax
        mov al, [rl_pp]
        mov [rl_ppv], al
        call ven_cli
        mov [rl_w], ecx
        mov [rl_h], edx
        TINTE RT_FONDO
        xor eax, eax
        xor ebx, ebx
        call dc_rect
        cmp byte [reloj_cifras], 0
        jne .pc
        call rl_esfera
        jmp .pf
.pc:
        call rl_cifras
.pf:
        mov byte [gdi_negrita], 0
        popad
        call dc_cerrar
        xor eax, eax
        ret

; (interna) AL en dos cifras, a EDI
reloj_dos:
        aam
        add ax, 0x3030
        xchg al, ah
        stosw
        ret

; (interna) lee la hora y la fecha de DOS
reloj_hora:
        pushad
        mov ah, 0x2A
        int 0x21
        mov [rl_dsem], al
        mov [rl_anio], cx
        mov [rl_mes], dh
        mov [rl_dia], dl
        mov ah, 0x2C
        int 0x21
        mov [reloj_h], ch
        mov [reloj_m], cl
        mov [reloj_s], dh
        cmp dl, 50                  ; los dos puntos: encendidos la primera
        setb [rl_pp]                ; mitad de cada segundo
        popad
        ret

; (interna) -> EAX=lo que se ve de la hora: día, hora, minuto y, según las
; opciones, el segundo (con segundero), la decena de segundos (la esfera sin
; él: el minutero se mueve muy poco) o nada (las cifras sin segundos)
rl_firma:
        push edx
        mov al, [reloj_s]
        cmp byte [reloj_seg], 0
        jne .s
        aam
        mov al, ah
        cmp byte [reloj_cifras], 0
        je .s
        xor al, al
.s:
        movzx edx, al
        movzx eax, byte [rl_dia]
        shl eax, 8
        mov al, [reloj_h]
        shl eax, 8
        mov al, [reloj_m]
        shl eax, 8
        or eax, edx
        pop edx
        ret

; -----------------------------------------------------------------------------
;  Esfera con agujas
; -----------------------------------------------------------------------------
rl_esfera:
        ; la fecha, debajo, si hay sitio (más grande en una ventana grande)
        mov dword [rl_esc], 1
        cmp dword [rl_h], 420
        jl .e1
        inc dword [rl_esc]
.e1:
        mov dword [rl_fh], 0
        cmp dword [rl_h], 130
        jl .sinf
        mov ecx, [rl_w]
        sub ecx, 12
        call rl_fecha_cabe
        jc .sinf
        mov eax, [rl_esc]
        shl eax, 4
        add eax, 10
        mov [rl_fh], eax
        TINTE RT_FECHA
        mov byte [gdi_negrita], 0
        mov eax, [rl_w]
        shr eax, 1
        mov ebx, [rl_h]
        sub ebx, [rl_fh]
        add ebx, 3
        mov esi, rl_ftxt
        call rl_centra
.sinf:
        ; el centro y el radio: lo que quepa
        mov eax, [rl_w]
        shr eax, 1
        mov [reloj_cx], eax
        mov ebx, [rl_h]
        sub ebx, [rl_fh]
        shr ebx, 1
        mov [reloj_cy], ebx
        cmp eax, ebx
        jbe .r1
        mov eax, ebx
.r1:
        sub eax, 8
        cmp eax, 6
        jge .r2
        mov eax, 6
.r2:
        mov [reloj_r], eax
        ; la sombra, el aro (filete, banda, filete) y la cara
        mov ecx, eax
        cmp ecx, 30
        jl .ss
        TINTE RT_SOMBRA
        mov eax, [reloj_cx]
        add eax, 2
        mov ebx, [reloj_cy]
        add ebx, 3
        call rl_disco
.ss:
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        TINTE RT_ARO1
        call rl_disco
        sub ecx, 2
        cmp dword [reloj_r], 150
        jl .a1
        dec ecx
.a1:
        TINTE RT_CARA               ; la banda, con un brillo arriba a la
        call rl_disco               ; izquierda: debajo va un disco claro y
        TINTE RT_ARO2               ; la banda, un punto corrida
        inc eax
        inc ebx
        dec ecx
        call rl_disco
        dec eax
        dec ebx
        inc ecx
        mov edx, [reloj_r]
        shr edx, 4
        cmp edx, 2
        jge .a2
        mov edx, 2
.a2:
        sub ecx, edx
        TINTE RT_ARO3
        call rl_disco
        dec ecx
        cmp dword [reloj_r], 150
        jl .a3
        dec ecx
.a3:
        TINTE RT_CARA
        call rl_disco
        cmp ecx, 1
        jge .a4
        mov ecx, 1
.a4:
        mov [rl_rf], ecx
        ; las medidas de las marcas, en proporción a la cara
        mov eax, ecx
        shr eax, 5
        neg eax
        lea eax, [eax+ecx-1]
        mov [rl_to], eax
        PROP 12
        cmp eax, 2
        jge .m1
        mov eax, 2
.m1:
        mov [rl_lm], eax
        PROP 22
        cmp eax, 3
        jge .m2
        mov eax, 3
.m2:
        mov [rl_lh], eax
        PROP 34
        cmp eax, 5
        jge .m3
        mov eax, 5
.m3:
        mov [rl_lq], eax
        ; las sesenta marcas
        xor edi, edi
.marca:
        imul eax, edi, 60
        mov [rl_ang], eax
        mov eax, edi
        xor edx, edx
        mov ebx, 5
        div ebx
        test edx, edx
        jz .hora
        cmp dword [rl_rf], 55       ; las de minuto, si hay sitio
        jl .msig
        TINTE RT_MINUTO
        mov dword [rl_gro], 0
        mov ecx, [rl_to]
        mov edx, ecx
        sub edx, [rl_lm]
        call rl_barra
        jmp .msig
.hora:
        TINTE RT_MARCA
        xor edx, edx
        mov ebx, 3
        div ebx                     ; (EDX=0: las 12, 3, 6 y 9)
        PROP 4
        mov ebx, [rl_lh]
        test edx, edx
        jnz .h1
        PROP 6
        mov ebx, [rl_lq]
        cmp dword [rl_rf], 30       ; (en una esfera mínima, al menos gruesas)
        jl .h1
        test eax, eax
        jnz .h1
        inc eax
.h1:
        mov [rl_gro], eax
        mov ecx, [rl_to]
        mov edx, ecx
        sub edx, ebx
        call rl_barra
.msig:
        inc edi
        cmp edi, 60
        jb .marca
        call rl_numeros
        call rl_agujas
        ret

; (interna) los números: los doce si la esfera es grande, 12, 3, 6 y 9 si es
; mediana, ninguno si es pequeña
rl_numeros:
        cmp dword [rl_rf], 45
        jl .x
        mov dword [rl_esc], 1
        cmp dword [rl_rf], 150
        jl .e
        inc dword [rl_esc]
        cmp dword [rl_rf], 290
        jl .e
        inc dword [rl_esc]
.e:
        TINTE RT_MARCA
        mov byte [gdi_negrita], 1
        mov edi, 1
.n:
        cmp dword [rl_rf], 95
        jge .pone
        mov eax, edi
        xor edx, edx
        mov ebx, 3
        div ebx
        test edx, edx
        jnz .sig
.pone:
        mov eax, edi                ; el número, en letra
        mov esi, reloj_txt
        cmp eax, 10
        jb .u
        mov byte [esi], '1'
        inc esi
        sub eax, 10
.u:
        add al, '0'
        mov [esi], al
        mov byte [esi+1], 0
        imul eax, edi, 300          ; su sitio: por dentro de las marcas
        mov ecx, [rl_to]
        sub ecx, [rl_lq]
        mov edx, [rl_esc]
        shl edx, 3
        sub ecx, edx
        sub ecx, 3
        xor edx, edx
        call rl_pt
        mov edx, [rl_esc]           ; (las cifras ocupan las filas 3 a 11)
        imul edx, 15
        shr edx, 1
        sub ebx, edx
        mov esi, reloj_txt
        call rl_centra
.sig:
        inc edi
        cmp edi, 12
        jbe .n
        mov byte [gdi_negrita], 0
.x:
        ret

; (interna) las agujas: la sombra de las dos grandes, ellas, el eje y el
; segundero con su contrapeso
rl_agujas:
        movzx eax, byte [reloj_h]
        xor edx, edx
        mov ebx, 12
        div ebx
        imul eax, edx, 3600         ; la hora de 0 a 11, en segundos
        movzx ecx, byte [reloj_m]
        imul ecx, 60
        movzx edx, byte [reloj_s]
        add ecx, edx
        mov [rl_amin], ecx          ; el minutero avanza con los segundos
        add eax, ecx
        xor edx, edx
        div ebx
        mov [rl_ahor], eax          ; y la de las horas, con los minutos
        mov eax, [rl_rf]            ; la sombra, corrida abajo y a la derecha
        shr eax, 7
        add eax, 2
        add [reloj_cx], eax
        add [reloj_cy], eax
        TINTE RT_ASOMB
        call .dos
        sub [reloj_cx], eax
        sub [reloj_cy], eax
        TINTE RT_AGUJA
        call .dos
        PROP 18                     ; el eje
        cmp eax, 3
        jge .e1
        mov eax, 3
.e1:
        mov edi, eax
        mov ecx, eax
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        call rl_disco
        cmp byte [reloj_seg], 0
        je .eje
        TINTE RT_SEG
        movzx eax, byte [reloj_s]
        imul eax, 60
        mov [rl_ang], eax
        xor eax, eax
        cmp dword [rl_rf], 150
        setge al
        mov [rl_gro], eax
        PROP 46
        mov ecx, eax
        neg ecx
        mov edx, [rl_to]
        call rl_barra
        PROP 36                     ; el contrapeso, detrás del eje
        mov ecx, eax
        neg ecx
        xor edx, edx
        mov eax, [rl_ang]
        call rl_pt
        push eax
        PROP 11
        cmp eax, 2
        jge .c1
        mov eax, 2
.c1:
        mov ecx, eax
        pop eax
        call rl_disco
        mov ecx, edi                ; y su tapa sobre el eje
        imul ecx, 5
        shr ecx, 3
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        call rl_disco
.eje:
        TINTE RT_EJE
        mov ecx, edi
        shr ecx, 2
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        call rl_disco
        ret
; las dos agujas grandes, con el color puesto (conserva EAX)
.dos:
        push eax
        mov eax, [rl_ahor]
        mov [rl_ang], eax
        PROP 26
        mov edx, eax
        PROP 16
        lea esi, [eax+1]
        PROP 138
        mov ecx, eax
        call rl_lanza
        mov eax, [rl_amin]
        mov [rl_ang], eax
        PROP 31
        mov edx, eax
        PROP 9
        lea esi, [eax+1]
        mov ecx, [rl_to]
        sub ecx, [rl_lm]
        sub ecx, 2
        call rl_lanza
        pop eax
        ret

; -----------------------------------------------------------------------------
;  Solo las cifras: el reloj de mesa
; -----------------------------------------------------------------------------
rl_cifras:
        ; el panel, hundido en el fondo de la ventana
        mov esi, 10
        cmp dword [rl_w], 120
        jl .m1
        cmp dword [rl_h], 90
        jge .m2
.m1:
        mov esi, 3
.m2:
        mov [rl_pm], esi
        mov eax, esi
        mov ebx, esi
        mov ecx, [rl_w]
        sub ecx, esi
        sub ecx, esi
        mov edx, [rl_h]
        sub edx, esi
        sub edx, esi
        mov [rl_pw], ecx
        mov [rl_ph], edx
        TINTE RT_PSOM
        call dc_rect
        add eax, 2
        add ebx, 2
        sub ecx, 2
        sub edx, 2
        TINTE RT_PLUZ
        call dc_rect
        sub ecx, 2
        sub edx, 2
        TINTE RT_PANEL
        call dc_rect
        ; el sitio útil: un margen dentro del panel, mayor cuanto mayor es
        mov eax, [rl_pw]
        cmp eax, [rl_ph]
        jle .u1
        mov eax, [rl_ph]
.u1:
        sar eax, 4
        add eax, 2
        cmp eax, 12
        jle .u2
        shr eax, 1
        add eax, 6
.u2:
        mov ecx, [rl_pw]
        sub ecx, eax
        sub ecx, eax
        mov [rl_aw], ecx
        mov edx, [rl_ph]
        sub edx, eax
        sub edx, eax
        mov [rl_ah], edx
        mov al, [reloj_seg]         ; a la derecha, los segundos y «AM»/«PM»
        or al, [reloj_12h]
        mov [rl_col], al
        mov byte [rl_hayf], 0       ; la fecha, si hay alto
        mov ebx, edx
        cmp edx, 80
        jl .sf
        mov byte [rl_hayf], 1
        sub ebx, 26
.sf:
        cmp ebx, 12
        jge .sf1
        mov ebx, 12
.sf1:
        ; el ancho de una cifra: el mayor con el que quepa todo
        mov edi, ecx
        sar edi, 2
        mov eax, ebx
        shr eax, 1
        cmp edi, eax
        jle .busca
        mov edi, eax
.busca:
        cmp edi, 6
        jg .b1
        mov edi, 6
        jmp .ya
.b1:
        call rl_ancho
        cmp eax, [rl_aw]
        jle .ya
        dec edi
        jmp .busca
.ya:
        cmp edi, 8
        jge .grandes
        ; no hay sitio ni para unas cifras pequeñas: la hora, con la letra de
        ; la casa
        mov edi, reloj_txt
        mov al, [reloj_h]
        cmp byte [reloj_12h], 0
        je .t1
        cmp al, 12
        jb .t0
        sub al, 12
.t0:
        test al, al
        jnz .t1
        mov al, 12
.t1:
        call reloj_dos
        mov al, ':'
        stosb
        mov al, [reloj_m]
        call reloj_dos
        cmp byte [reloj_seg], 0
        je .t2
        mov al, ':'
        stosb
        mov al, [reloj_s]
        call reloj_dos
.t2:
        mov byte [edi], 0
        TINTE RT_ENC
        mov byte [gdi_negrita], 1
        mov dword [rl_esc], 1
        mov dword [rl_ppan], 0      ; (sin dos puntos que latan)
        mov eax, [rl_w]
        shr eax, 1
        mov ebx, [rl_h]
        sub ebx, 16
        sar ebx, 1
        mov esi, reloj_txt
        call rl_centra
        ret
.grandes:
        call rl_ancho
        mov [rl_tot], eax
        mov [rl_cdw], edi
        ; el alto: el doble, y hasta dos veces y media si sobra
        lea eax, [edi*4+edi]
        shr eax, 1
        cmp eax, ebx
        jle .al1
        mov eax, ebx
.al1:
        mov [rl_cdh], eax
        mov dword [rl_hueco], 0
        cmp byte [rl_hayf], 0
        je .sitio
        ; la fecha: tan grande como vaya con las cifras y quepa entera
        mov eax, edi
        xor edx, edx
        mov ebx, 26
        div ebx
        cmp eax, 1
        jae .f1
        mov eax, 1
.f1:
        cmp eax, 3
        jbe .f2
        mov eax, 3
.f2:
        mov [rl_esc], eax
        mov eax, edi
        xor edx, edx
        mov ebx, 6
        div ebx
        add eax, 6
        mov [rl_hueco], eax
.f3:
        xor al, al                  ; ¿cabe la forma larga a este tamaño?
        call rl_fecha
        imul eax, [rl_esc]
        shl eax, 3
        cmp eax, [rl_aw]
        jg .f4
        mov eax, [rl_esc]
        shl eax, 4
        add eax, [rl_hueco]
        add eax, [rl_cdh]
        cmp eax, [rl_ah]
        jle .sitio
.f4:
        cmp dword [rl_esc], 1
        je .f5
        dec dword [rl_esc]
        jmp .f3
.f5:
        mov dword [rl_hueco], 10    ; a tamaño 1: la forma que quepa
        mov ecx, [rl_aw]
        call rl_fecha_cabe
        jnc .sitio
        mov byte [rl_hayf], 0
        mov dword [rl_hueco], 0
.sitio:
        ; todo centrado en el panel
        mov eax, [rl_cdh]
        cmp byte [rl_hayf], 0
        je .s1
        mov ebx, [rl_esc]
        shl ebx, 4
        add eax, ebx
        add eax, [rl_hueco]
.s1:
        mov ebx, [rl_ph]
        sub ebx, eax
        sar ebx, 1
        add ebx, [rl_pm]
        mov [rl_y0], ebx
        mov eax, [rl_pw]
        sub eax, [rl_tot]
        sar eax, 1
        add eax, [rl_pm]
        mov [rl_x0], eax
        ; las cifras de la hora
        movzx eax, byte [reloj_h]
        mov byte [rl_tarde], 0
        cmp byte [reloj_12h], 0
        je .h24
        cmp al, 12
        jb .am
        sub al, 12
        mov byte [rl_tarde], 1
.am:
        test al, al
        jnz .h12
        mov al, 12
.h12:
        aam
        test ah, ah                 ; en 12 horas, sin cero delante
        jnz .hh
        mov ah, 10
        jmp .hh
.h24:
        aam
.hh:
        mov [rl_dig], ah
        mov [rl_dig+1], al
        mov al, [reloj_m]
        aam
        mov [rl_dig+2], ah
        mov [rl_dig+3], al
        mov al, [reloj_s]
        aam
        mov [rl_dig+4], ah
        mov [rl_dig+5], al
        ; HH : MM
        mov eax, [rl_cdw]
        call rl_grosor
        mov eax, [rl_x0]
        mov ebx, [rl_y0]
        mov ecx, [rl_cdw]
        mov edx, [rl_cdh]
        movzx esi, byte [rl_dig]
        call rl_digito
        add eax, ecx
        add eax, [rl_g]
        movzx esi, byte [rl_dig+1]
        call rl_digito
        add eax, ecx
        add eax, [rl_g]
        ; los dos puntos: dos rombos, encendidos media vuelta de segundo
        push eax
        push ebx
        push ecx
        push edx
        mov [rl_ppy], ebx
        mov [rl_ppal], edx
        mov ecx, [rl_cw]
        shr ecx, 1
        add eax, ecx                ; su centro
        mov ecx, [rl_sh]
        mov esi, eax
        sub esi, ecx
        mov [rl_ppx], esi
        lea esi, [ecx*2+1]
        mov [rl_ppan], esi
        TINTE RT_APAG
        cmp byte [rl_pp], 0
        je .pp1
        TINTE RT_ENC
.pp1:
        mov esi, edx
        imul edx, 5
        shr edx, 4
        add ebx, edx                ; a 5/16 y a 11/16 del alto
        call dc_rombo
        mov edx, esi
        imul edx, 6
        shr edx, 4
        add ebx, edx
        call dc_rombo
        pop edx
        pop ecx
        pop ebx
        pop eax
        add eax, [rl_cw]
        add eax, [rl_g]
        movzx esi, byte [rl_dig+2]
        call rl_digito
        add eax, ecx
        add eax, [rl_g]
        movzx esi, byte [rl_dig+3]
        call rl_digito
        add eax, ecx
        ; la columna de la derecha: «AM»/«PM» arriba, los segundos abajo
        cmp byte [rl_col], 0
        je .fecha
        add eax, [rl_g]
        mov ecx, [rl_g]
        shr ecx, 1
        add eax, ecx
        cmp byte [reloj_12h], 0
        je .seg
        push eax
        push ebx
        mov ecx, [rl_sdw]           ; letra: según el ancho de la columna
        xor edx, edx
        xchg eax, ecx
        mov esi, 12
        div esi
        xchg eax, ecx
        cmp ecx, 1
        jae .ap1
        mov ecx, 1
.ap1:
        push dword [rl_esc]
        mov [rl_esc], ecx
        lea ecx, [ecx*2+ecx]        ; (las mayúsculas empiezan en la fila 3)
        sub ebx, ecx
        TINTE RT_ACENTO
        mov byte [gdi_negrita], 1
        mov esi, rl_t_am
        cmp byte [rl_tarde], 0
        je .ap2
        mov esi, rl_t_pm
.ap2:
        call rl_texto
        mov byte [gdi_negrita], 0
        pop dword [rl_esc]
        pop ebx
        pop eax
.seg:
        cmp byte [reloj_seg], 0
        je .fecha
        mov edx, [rl_cdh]
        shr edx, 1
        mov ebx, [rl_y0]
        add ebx, [rl_cdh]
        sub ebx, edx
        mov ecx, [rl_sdw]
        push eax
        mov eax, ecx
        call rl_grosor
        pop eax
        movzx esi, byte [rl_dig+4]
        call rl_digito
        add eax, ecx
        add eax, [rl_sg]
        movzx esi, byte [rl_dig+5]
        call rl_digito
.fecha:
        cmp byte [rl_hayf], 0
        je .x
        TINTE RT_PTXT
        mov byte [gdi_negrita], 0
        mov eax, [rl_w]
        shr eax, 1
        mov ebx, [rl_y0]
        add ebx, [rl_cdh]
        add ebx, [rl_hueco]
        mov esi, rl_ftxt
        call rl_centra
.x:
        ret

; (interna) EDI=ancho de una cifra grande -> EAX=ancho de todo (HH:MM y, con
; rl_col, la columna de la derecha); deja rl_g, rl_cw, rl_sdw y rl_sg
rl_ancho:
        push ebx
        push edx
        mov eax, edi
        shr eax, 2
        cmp eax, 2
        jae .a
        mov eax, 2
.a:
        mov [rl_g], eax
        mov ebx, edi
        shr ebx, 1
        mov [rl_sdw], ebx
        shr ebx, 2
        jnz .b
        inc ebx
.b:
        mov [rl_sg], ebx
        mov eax, edi
        xor edx, edx
        mov ebx, 3
        div ebx
        cmp eax, 3
        jae .c
        mov eax, 3
.c:
        mov [rl_cw], eax
        mov ebx, [rl_g]
        lea eax, [eax+ebx*4]
        lea eax, [eax+edi*4]        ; 4 cifras + 4 huecos + los dos puntos
        cmp byte [rl_col], 0
        je .x
        add eax, ebx
        shr ebx, 1
        add eax, ebx
        mov ebx, [rl_sdw]
        lea eax, [eax+ebx*2]
        add eax, [rl_sg]
.x:
        pop edx
        pop ebx
        ret

; (interna) EAX=ancho de la cifra -> rl_sh, el medio grosor de sus segmentos
rl_grosor:
        push eax
        push edx
        push ebx
        xor edx, edx
        mov ebx, 10
        div ebx
        test eax, eax
        jnz .a
        inc eax
.a:
        mov [rl_sh], eax
        pop ebx
        pop edx
        pop eax
        ret

; (interna) una cifra de siete segmentos: EAX=x EBX=y ECX=ancho EDX=alto
; ESI=cifra (10: hueco). Los encendidos, de RT_ENC; los apagados, de RT_APAG
rl_digito:
        pushad
        mov edi, [rl_sh]
        lea ebp, [eax+edi]
        mov [rl_co], ebp            ; eje de los de la izquierda
        lea ebp, [eax+ecx-1]
        sub ebp, edi
        mov [rl_co+4], ebp          ; de los de la derecha
        lea ebp, [ebx+edi]
        mov [rl_co+8], ebp          ; del de arriba
        mov ebp, edx
        shr ebp, 1
        add ebp, ebx
        mov [rl_co+12], ebp         ; del del medio
        lea ebp, [ebx+edx-1]
        sub ebp, edi
        mov [rl_co+16], ebp         ; del de abajo
        movzx ebp, byte [rl_mascaras+esi]
        mov esi, rl_segs
        mov ecx, 7
.s:
        TINTE RT_APAG
        shr ebp, 1
        jnc .a
        TINTE RT_ENC
.a:
        movzx eax, byte [esi+1]
        mov eax, [rl_co+eax*4]
        movzx ebx, byte [esi+2]
        mov ebx, [rl_co+ebx*4]
        movzx edx, byte [esi+3]
        mov edx, [rl_co+edx*4]
        cmp byte [esi], 0
        jne .v
        call rl_seg_h
        jmp .n
.v:
        call rl_seg_v
.n:
        add esi, 4
        dec ecx
        jnz .s
        popad
        ret

; (internas) un segmento: un hexágono alargado (las puntas en bisel), de medio
; grosor rl_sh, entre dos ejes; se queda corto de las esquinas para que los
; segmentos vecinos no se toquen
; tumbado: EAX=x0 EBX=x1 EDX=y del eje
rl_seg_h:
        pushad
        mov edi, [rl_sh]
        mov esi, edi
        shr esi, 2
        inc esi
        add eax, esi
        sub ebx, esi
        mov ebp, ebx
        sub ebp, eax
        inc ebp                     ; largo en el eje
        mov esi, eax
        mov ebx, edx
        sub ebx, edi
        neg edi                     ; de -grosor a +grosor
.f:
        mov eax, edi
        cdq
        xor eax, edx
        sub eax, edx                ; lo que se mete cada fila: |i|
        mov ecx, ebp
        sub ecx, eax
        sub ecx, eax
        add eax, esi
        mov edx, 1
        call dc_rect
        inc ebx
        inc edi
        cmp edi, [rl_sh]
        jle .f
        popad
        ret
; de pie: EAX=x del eje EBX=y0 EDX=y1
rl_seg_v:
        pushad
        mov esi, [rl_sh]
        shr esi, 2
        inc esi
        add ebx, esi
        sub edx, esi
        mov esi, eax
        mov ebp, edx
        mov edi, ebx
.f:
        cmp ebx, ebp
        jg .x
        mov eax, ebx
        sub eax, edi                ; lo que falta a cada punta
        mov ecx, ebp
        sub ecx, ebx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        cmp eax, [rl_sh]
        jle .b
        mov eax, [rl_sh]
.b:
        lea ecx, [eax*2+1]
        neg eax
        add eax, esi
        mov edx, 1
        call dc_rect
        inc ebx
        jmp .f
.x:
        popad
        ret
