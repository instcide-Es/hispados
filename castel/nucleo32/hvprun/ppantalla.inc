; =============================================================================
;  HVPRUN - la pantalla de Ventanas dentro de una ventana de Hispa-Pentium:
;  la superficie, la paleta, presentar, la ventana principal y los sucesos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La «pantalla» de Ventanas (vid_ancho x vid_alto, la superficie de 8 bits
;  de gdi.inc) mide lo que lo de dentro de la ventana de Hispa-Pentium. La
;  primera ventana con título que abre el programa es la principal: se le
;  quitan el borde que estira y los botones de maximizar y minimizar (eso
;  lo hace la ventana de Hispa-Pentium) y se pone de modo que su zona de
;  dentro (con la barra de menús) ocupe toda la pantalla, y su marco y su
;  título queden fuera. Si el programa la mueve o la estira, vuelve a su
;  sitio (pan_principal); si la ventana de Hispa-Pentium cambia de tamaño,
;  cambian la pantalla y la principal (pan_tamano).
; =============================================================================

EVP_TECLA       equ 1
EVP_BAJA        equ 2
EVP_SUBE        equ 3
EVP_MUEVE       equ 4
EVP_CERRAR      equ 5
EVP_TAMANO      equ 6
EVP_FOCO        equ 7
P_CON_MAYUS     equ 0x100           ; en «c» de los del ratón
P_CON_CONTROL   equ 0x200
OPP_MAPA        equ 7               ; la orden de dibujar un mapa de 32 bits
PRES_MAX        equ 128 * 1024      ; puntos que caben en el búfer de presentar
PPAL_DX         equ BORDE           ; lo que queda fuera de la pantalla, a la
PPAL_DY         equ BORDE + TIT_H + 1   ; izquierda y arriba (y lo mismo, abajo y a
PPAL_AN         equ 2 * BORDE           ; la derecha)
PPAL_AL         equ 2 * BORDE + TIT_H + 1
PB_BORDE        equ 4               ; el marco de las ventanas de Hispa-Pentium
PB_TITULO       equ 28              ; (ventanas.cas: BORDE, ALTO_TÍTULO)
PB_BARRA        equ 34              ; y su barra de tareas (escritorio.cas: ALTO_BARRA)

; -> CF=1 si no hay memoria
pan_iniciar:
        pushad
        mov ecx, [pan_max_an]
        imul ecx, [pan_max_al]
        add ecx, 4095
        and ecx, ~4095
        call pent_mas_memoria
        jc .x
        mov [sup_ptr], eax
        ; la pantalla que se le dice al programa antes de que abra su
        ; ventana: lo de dentro de una ventana de Hispa-Pentium que ocupe la
        ; pantalla menos la barra de tareas, más el título y el marco de la
        ; de Ventanas (que quedan fuera)
        mov eax, [pan_max_an]
        sub eax, 2 * PB_BORDE
        mov [vid_ancho], eax
        mov eax, [pan_max_al]
        sub eax, PB_BARRA + PB_TITULO + PB_BORDE - PPAL_AL
        mov [vid_alto], eax
        mov dword [area_y0], 0
        mov [area_y1], eax
        mov byte [vid_bits], 8
        mov byte [vid_octetos], 1
        call pan_paleta
        clc
.x:
        popad
        ret

; La paleta de Ventanas (video.inc, vid_paleta): sus 16 colores, un cubo de
; 6x6x6 (16-231) y una escala de grises (232-255). En pal_rgb como en
; Ventanas (rojo, verde, azul, 0) y en pal32 como los quiere Hispa-Pentium
; (0x00RRGGBB).
pan_paleta:
        pushad
        mov esi, paleta16
        mov edi, pal_rgb
        mov ecx, 16
.p16:
        movsw
        movsb
        xor al, al
        stosb
        loop .p16
        xor ebx, ebx
.cubo:
        mov eax, ebx
        xor edx, edx
        mov ecx, 36
        div ecx
        imul eax, 51
        stosb
        mov eax, edx
        xor edx, edx
        mov ecx, 6
        div ecx
        imul eax, 51
        stosb
        imul eax, edx, 51
        stosb
        xor al, al
        stosb
        inc ebx
        cmp ebx, 216
        jb .cubo
        xor ebx, ebx
.gris:
        imul eax, ebx, 11
        stosb
        stosb
        stosb
        xor al, al
        stosb
        inc ebx
        cmp ebx, 24
        jb .gris
        mov esi, pal_rgb
        mov edi, pal32
        mov ecx, 256
.p32:
        movzx eax, byte [esi]
        shl eax, 16
        mov ah, [esi+1]
        mov al, [esi+2]
        stosd
        add esi, 4
        loop .p32
        popad
        ret

; (gestor) EAX=x0 EBX=y0 ECX=x1 EDX=y1 de la superficie, a la ventana
vid_presentar:
        pushad
        cmp dword [pan_ven], 0
        je .x
        test eax, eax
        jns .a
        xor eax, eax
.a:
        test ebx, ebx
        jns .b
        xor ebx, ebx
.b:
        cmp ecx, [vid_ancho]
        jle .c
        mov ecx, [vid_ancho]
.c:
        cmp edx, [vid_alto]
        jle .d
        mov edx, [vid_alto]
.d:
        sub ecx, eax                ; ECX=ancho
        jle .x
        sub edx, ebx                ; EDX=alto
        jle .x
        mov [pr_ox], eax
        mov [pr_an], ecx
        push edx
        mov eax, PRES_MAX           ; las filas que caben de una vez
        xor edx, edx
        div ecx
        mov [pr_filas], eax
        pop edx
.trozo:
        mov ecx, [pr_filas]
        cmp ecx, edx
        jbe .t1
        mov ecx, edx
.t1:
        push edx
        push ecx
        ; las filas EBX... (ECX de ellas), a 32 bits
        mov edi, pres_buf
        mov ebp, ecx
        push ebx
.fila:
        mov esi, ebx
        imul esi, [sup_ancho]
        add esi, [pr_ox]
        add esi, [sup_ptr]
        mov ecx, [pr_an]
.p:
        movzx eax, byte [esi]
        inc esi
        mov eax, [pal32+eax*4]
        stosd
        dec ecx
        jnz .p
        inc ebx
        dec ebp
        jnz .fila
        pop ebx
        pop ecx
        mov dword [pr_orden], OPP_MAPA
        mov eax, [pr_ox]
        mov [pr_orden+4], eax
        mov [pr_orden+8], ebx
        mov eax, [pr_an]
        mov [pr_orden+12], eax
        mov [pr_orden+16], ecx
        mov dword [pr_orden+28], pres_buf
        push ebx
        push ecx
        mov ebx, [pan_ven]
        mov ecx, pr_orden
        PSIS PS_DIBUJAR
        pop ecx
        pop ebx
        pop edx
        add ebx, ecx
        sub edx, ecx
        jnz .trozo
.x:
        popad
        ret

; ---- el cursor: lo pinta Hispa-Pentium (hay_rat = 0) ----
cur_ocultar:
cur_mostrar:
        ret

; (gestor, en ven_crear) EDI=ventana de primer nivel que se está creando: la
; primera con título del programa es la principal
bt_colocar:
        cmp dword [pan_ppal], 0
        jne .r
        test dword [edi+V_ESTILO], E_HIJA | E_EMERGENTE
        jnz .r
        test dword [edi+V_ESTILO], E_TITULO
        jz .r
        push eax
        mov eax, [tar_act]
        cmp eax, [hvp_tarea_p]
        pop eax
        jne .r
        pushad
        mov [pan_ppal], edi
        and dword [edi+V_ESTILO], ~(E_TAMANO | E_MAXIM | E_MINIM)
        mov ecx, [edi+V_AN]
        sub ecx, PPAL_AN
        mov edx, [edi+V_AL]
        sub edx, PPAL_AL
        call pan_acotar
        cmp dword [pan_ven], 0
        jne .ya
        push ecx
        push edx
        mov ebx, [edi+V_TEXTO]
        test ebx, ebx
        jnz .t
        mov ebx, t_sin_titulo
.t:
        PSIS PS_VENTANA
        pop edx
        pop ecx
        cmp eax, PRIMER_ERROR
        jae .sin_ventana
        mov [pan_ven], eax
        push ecx                    ; en el centro de la pantalla
        mov ebx, eax
        mov ecx, -1
        PSIS PS_COLOCAR
        pop ecx
.ya:
        call pan_medidas
        mov dword [edi+V_X], -PPAL_DX   ; (aún no está en uso: a mano)
        mov dword [edi+V_Y], -PPAL_DY
        mov eax, [vid_ancho]
        add eax, PPAL_AN
        mov [edi+V_AN], eax
        mov eax, [vid_alto]
        add eax, PPAL_AL
        mov [edi+V_AL], eax
        popad
.r:
        ret
.sin_ventana:
        mov esi, t_sin_ventana
        call pent_escribir
        mov ebx, 1
        jmp pent_salir

; (interna) ECX=ancho EDX=alto: que quepan en la pantalla de Hispa-Pentium
pan_acotar:
        cmp ecx, 120
        jge .a
        mov ecx, 120
.a:
        cmp ecx, [pan_max_an]
        jle .b
        mov ecx, [pan_max_an]
.b:
        cmp edx, 60
        jge .c
        mov edx, 60
.c:
        cmp edx, [pan_max_al]
        jle .d
        mov edx, [pan_max_al]
.d:
        ret

; (interna) ECX=ancho EDX=alto de lo de dentro de la ventana: la pantalla de
; Ventanas pasa a medir eso, y todo se repinta
pan_medidas:
        pushad
        mov [vid_ancho], ecx
        mov [vid_alto], edx
        mov dword [area_y0], 0
        mov [area_y1], edx
        call gdi_iniciar
        xor eax, eax                ; todo, a repintar
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        cmp dword [escritorio], 0
        je .x
        call ges_exponer
        mov [pres_x0], eax          ; y a presentar entera
        mov [pres_y0], ebx
        mov [pres_x1], ecx
        mov [pres_y1], edx
.x:
        popad
        ret

; (sistema) la principal, en su sitio: con su zona de dentro en toda la
; pantalla. Si ya no está (el programa la ha cerrado), se olvida.
pan_principal:
        pushad
        mov ebp, [pan_ppal]
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_USO], 1
        jne .perdida
        and dword [ebp+V_ESTADO], ~S_MAXIM
        mov eax, -PPAL_DX
        mov ebx, -PPAL_DY
        mov ecx, [vid_ancho]
        add ecx, PPAL_AN
        mov edx, [vid_alto]
        add edx, PPAL_AL
        cmp eax, [ebp+V_X]
        jne .mover
        cmp ebx, [ebp+V_Y]
        jne .mover
        cmp ecx, [ebp+V_AN]
        jne .mover
        cmp edx, [ebp+V_AL]
        je .x
.mover:
        call ven_mover
        jmp .x
.perdida:
        mov dword [pan_ppal], 0
.x:
        popad
        ret

; el título de una ventana (ven_texto): si es la principal, también el de
; la ventana de Hispa-Pentium
p_ven_texto:
        call ven_texto
        cmp ebp, [pan_ppal]
        jne .x
        cmp dword [pan_ven], 0
        je .x
        test esi, esi
        jz .x
        push eax
        push ebx
        push ecx
        mov ebx, [pan_ven]
        mov ecx, esi
        PSIS PS_TITULO
        pop ecx
        pop ebx
        pop eax
.x:
        ret

; la pantalla: -> EAX=ancho EBX=alto
pantalla:
        mov eax, [vid_ancho]
        mov ebx, [vid_alto]
        ret

; ---- la ventana del fondo (como la del escritorio de Ventanas): lo que no
; tapa ninguna ventana, y adonde va el ratón si no cae en otra ----
p_fondo_crear:
        pushad
        mov esi, p_fondo_plant
        xor ebp, ebp
        call ven_crear
        mov [escritorio], eax
        mov ebp, eax
        call orden_quitar           ; no está en el orden: es el fondo
        popad
        ret
c_fondo_hvp dd ven_defecto
            db C_ARENA_OSC, 0, 0, 0
p_fondo_plant:
        PLANTILLA c_fondo_hvp, 0, E_SINFOCO, 0, 0, 4096, 4096, 0

; =============================================================================
;  Los sucesos de la ventana
; =============================================================================
; (sistema) si en la vuelta de antes no ha pasado nada, espera: a un suceso
; (que queda guardado para pan_sucesos) o, si hay temporizadores, un tic
pan_esperar:
        pushad
        cmp byte [actividad], 0
        jne .x
        cmp byte [pan_hay_guardado], 0
        jne .x
        mov esi, temporizadores     ; ¿algún temporizador en marcha?
        mov ecx, TMP_MAX
.t:
        cmp dword [esi+TM_VEN], 0
        jne .tic
        add esi, 16
        loop .t
        mov ebx, [pan_ven]
        test ebx, ebx
        jz .tic
        mov ecx, pan_suceso
        mov edx, 1                  ; esperarlo
        PSIS PS_SUCESO
        cmp eax, 1
        jne .tic
        mov byte [pan_hay_guardado], 1
        jmp .x
.tic:
        mov ebx, 1
        PSIS PS_DORMIR
.x:
        popad
        ret

; (sistema) los sucesos que hay, al gestor
pan_sucesos:
        pushad
        cmp byte [pan_hay_guardado], 0
        je .l
        mov byte [pan_hay_guardado], 0
        call pan_uno
.l:
        mov ebx, [pan_ven]
        test ebx, ebx
        jz .x
        mov ecx, pan_suceso
        xor edx, edx
        PSIS PS_SUCESO
        cmp eax, 1
        jne .x
        call pan_uno
        jmp .l
.x:
        popad
        ret

; (interna) el suceso de pan_suceso
pan_uno:
        mov byte [actividad], 1
        mov eax, [pan_suceso]
        cmp eax, EVP_TECLA
        je pan_tecla
        cmp eax, EVP_BAJA
        je pan_raton
        cmp eax, EVP_SUBE
        je pan_raton
        cmp eax, EVP_MUEVE
        je pan_raton
        cmp eax, EVP_CERRAR
        je pan_cerrar
        cmp eax, EVP_TAMANO
        je pan_tamano
        cmp eax, EVP_FOCO
        je pan_foco
        ret

; ---- el portapapeles ----
; Ventanas tiene el suyo (controles.inc: pp_buf, pp_len), y los programas
; copian y pegan por él. Se pone al día con el de Hispa-Pentium (llamada 40)
; cuando la ventana pasa a ser la activa (lo que se haya copiado en otro
; programa, aquí) y cuando deja de serlo (lo copiado aquí, si ha cambiado,
; para los demás).
pan_foco:
        pushad
        cmp dword [pan_suceso+4], 0
        je .sale
        mov ebx, 2                  ; entra: ¿hay algo en el del sistema?
        PSIS PS_PORTAPAPELES
        test eax, eax
        jz .x
        cmp eax, PRIMER_ERROR
        jae .x
        mov ebx, 1
        mov ecx, pres_buf
        mov edx, PP_MAX
        PSIS PS_PORTAPAPELES
        cmp eax, PRIMER_ERROR
        jae .x
        test eax, eax
        jz .x
        mov esi, pres_buf
        mov ecx, eax
        call pp_poner
        call pp_sello
        mov [pp_visto], eax
        jmp .x
.sale:
        call pp_exportar
.x:
        popad
        ret

; si en el portapapeles de Ventanas hay algo nuevo, al del sistema (al salir
; de la ventana, y al acabar el programa)
pp_exportar:
        pushad
        mov ecx, [pp_len]
        test ecx, ecx
        jz .x
        cmp dword [pp_buf], 0
        je .x
        call pp_sello
        cmp eax, [pp_visto]
        je .x
        mov [pp_visto], eax
        mov esi, [pp_buf]           ; (sin los retornos de carro)
        mov edi, pres_buf
.c:
        lodsb
        cmp al, 13
        je .s
        stosb
.s:
        loop .c
        mov edx, edi
        sub edx, pres_buf
        xor ebx, ebx
        mov ecx, pres_buf
        PSIS PS_PORTAPAPELES
.x:
        popad
        ret

; (interna) -> EAX=una cuenta de lo que hay en el portapapeles de Ventanas
pp_sello:
        push ecx
        push esi
        mov eax, [pp_len]
        mov ecx, eax
        mov esi, [pp_buf]
        test esi, esi
        jz .x
.l:
        test ecx, ecx
        jz .x
        rol eax, 5
        xor al, [esi]
        inc esi
        dec ecx
        jmp .l
.x:
        pop esi
        pop ecx
        ret

; el botón de cerrar de la ventana: WM_CLOSE a la principal (o, si hay un
; cuadro abierto encima, al cuadro: como si se cerrara él)
pan_cerrar:
        mov ebp, [pan_ppal]
        test ebp, ebp
        jz .fuera
        test dword [ebp+V_ESTADO], S_INACTIVA
        jz .pon
        mov eax, [activa]
        test eax, eax
        jz .pon
        mov ebp, eax
.pon:
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        jmp msg_poner
.fuera:
        xor ebx, ebx
        jmp pent_salir

; la ventana ha cambiado de tamaño: a, b = lo de dentro
pan_tamano:
        mov ecx, [pan_suceso+4]
        mov edx, [pan_suceso+8]
        call pan_acotar
        cmp ecx, [vid_ancho]
        jne .si
        cmp edx, [vid_alto]
        je .x
.si:
        call pan_medidas
        call pan_principal
.x:
        ret

; el ratón: a, b = dónde (dentro de la ventana), c = los botones, las teclas
; de cambio y las veces seguidas
pan_raton:
        cmp dword [pan_ppal], 0     ; (hasta que hay ventana principal, nada)
        je .x
        mov ecx, [pan_suceso+12]
        call pan_cambio
        mov eax, [pan_suceso+4]
        mov ebx, [pan_suceso+8]
        cmp eax, [rat_x]
        jne .mueve
        cmp ebx, [rat_y]
        je .bot
.mueve:
        call rat_mover              ; (acota a la pantalla)
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_raton_mueve
.bot:
        mov ecx, [pan_suceso+12]
        mov bh, cl
        shr bh, 1
        and bh, 1                   ; BH=el derecho
        cmp bh, [rat_der]
        je .izq
        mov [rat_der], bh
        call ges_raton_der
.izq:
        mov ecx, [pan_suceso+12]
        mov bl, cl
        and bl, 1                   ; BL=el izquierdo
        cmp bl, [rat_boton]
        je .x
        mov [rat_boton], bl
        mov eax, [rat_x]
        mov ebx, [rat_y]
        cmp byte [rat_boton], 0
        je .suelta
        mov byte [rat_doble], 0     ; dos clics seguidos en el mismo sitio (lo
        shr ecx, 16                 ; cuenta Hispa-Pentium: la segunda vez)
        cmp ecx, 2
        jne .pulsa
        mov byte [rat_doble], 1
.pulsa:
        call ges_raton_pulsa
        ret
.suelta:
        call ges_raton_suelta
.x:
        ret

; (interna) ECX=c de un suceso del ratón: Mayús y Control, a tec_mayus
pan_cambio:
        xor al, al
        test ecx, P_CON_MAYUS
        jz .m
        or al, TM_MAYUS
.m:
        test ecx, P_CON_CONTROL
        jz .c
        or al, TM_CTRL
.c:
        mov [tec_mayus], al
        ret

rat_mover:
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp eax, [vid_ancho]
        jl .b
        mov eax, [vid_ancho]
        dec eax
.b:
        test ebx, ebx
        jns .c
        xor ebx, ebx
.c:
        cmp ebx, [vid_alto]
        jl .d
        mov ebx, [vid_alto]
        dec ebx
.d:
        mov [rat_x], eax
        mov [rat_y], ebx
        ret

; una tecla: a = la letra (página 437), 0x100 + su código si no es letra, o
; 1-26 con Control; b = 1 si venía con Mayús. A ges_tecla, como la daría
; INT 16h (AL=carácter, AH=código de la tecla).
pan_tecla:
        cmp dword [pan_ppal], 0
        je .x
        mov eax, [pan_suceso+4]
        xor ecx, ecx                ; tec_mayus: Mayús si lo dice el suceso
        test dword [pan_suceso+8], 1
        jz .m
        mov cl, TM_MAYUS
.m:
        mov [tec_mayus], cl
        cmp eax, 0x100
        jb .letra
        sub eax, 0x100              ; no es una letra: AL=0, AH=su código
        cmp eax, 0xFF
        ja .x
        shl eax, 8
        jmp .va
.letra:
        cmp eax, 10                 ; Intro
        jne .l1
        mov eax, 0x1C0D
        jmp .va
.l1:
        cmp eax, 13                 ; (13 solo llega con Ctrl+M)
        jne .l2
        mov eax, 0x320D
        jmp .va
.l2:
        cmp eax, 8
        jne .l3
        mov eax, 0x0E08
        jmp .va
.l3:
        cmp eax, 9
        jne .l4
        mov eax, 0x0F09
        jmp .va
.l4:
        cmp eax, 27
        jne .l5
        mov eax, 0x011B
        jmp .va
.l5:
        cmp eax, 27                 ; Ctrl+letra (1-26): con Control pulsado
        jae .va
        or byte [tec_mayus], TM_CTRL
.va:
        call ges_tecla
        mov byte [tec_mayus], 0
.x:
        ret

t_sin_titulo    db "Hispa Ventanas", 0
t_sin_ventana   db "HVPRUN: no se puede abrir la ventana.", 10, 0
