; =============================================================================
;  Hispa Ventanas - el escritorio y las ventanas de muestra
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El escritorio es una ventana más (la del fondo, de la tarea del sistema):
;  mosaico de rombos en azul petróleo, barra superior marrón con filete ocre y
;  el botón «Salir» y el altavoz del volumen (volumen.inc), y abajo los iconos
;  de las ventanas minimizadas (un clic las restaura).
;    esc_iniciar     prepara el mosaico y los textos y crea el escritorio
;    esc_inv_iconos  hay que repintar la fila de iconos
;    esc_muestra     crea las tres ventanas de muestra de la etapa 2
;                    (VENTANAS /DEMO): Tab, flechas y Esc como entonces
; =============================================================================

SALIR_AN    equ 64
SALIR_AL    equ 18
SALIR_Y     equ 4
ICO_PASO    equ 96                  ; de un icono minimizado al siguiente
ICO_ALTO    equ 62

; [BATERÍA] [RELOJ] indicador de batería (APM) y hora, en la barra entre el
; altavoz y «Salir». El orden, de izquierda a derecha, queda:
;    [altavoz] [pila][85%] [23:58] [Salir]
; La hora (HH:MM) se enseña siempre; la batería, solo si hay (portátil con
; APM). Todas las distancias se miden desde el borde derecho de la pantalla.
BAT_AN      equ 22                  ; ancho del cuerpo de la pila
BAT_AL      equ 11                  ; alto del cuerpo
BAT_BORNE   equ 2                   ; ancho del borne + (a la derecha)
BAT_Y       equ 7                   ; arriba del cuerpo dentro de la barra
BAT_NUM     equ 32                  ; hueco reservado para el número ("100%")
REL_AN      equ 40                  ; ancho de "HH:MM" (5 x 8)
REL_X       equ SALIR_AN + 12 + 8 + REL_AN                      ; = 124 (hora)
BAT_NUM_X   equ REL_X + 8 + BAT_NUM                             ; = 164 (número)
BAT_X       equ BAT_NUM_X + 4 + BAT_BORNE + BAT_AN              ; = 192 (cuerpo)

c_escritorio dd esc_proc
            db SIN_FONDO, 0, 0, 0
c_demo      dd demo_proc
            db C_CREMA, 0, 0, 0

esc_iniciar:
        pushad
        ; el mosaico: del diseño en texto a colores
        mov esi, mosaico_dib
        mov edi, mosaico
        mov ecx, 256
.m:
        lodsb
        mov ah, C_PETROLEO
        cmp al, '+'
        jne .m2
        mov ah, C_PETROLEO_CL
.m2:
        mov [edi], ah
        inc edi
        loop .m

        ; ---- textos de la ventana «Sistema» ----
        mov edi, t_pantalla
        mov esi, s_pantalla
        call pon_cad
        mov eax, [vid_ancho]
        call pon_num
        mov esi, s_por
        call pon_cad
        mov eax, [vid_alto]
        call pon_num
        mov esi, s_c16
        mov al, [vid_bits]
        cmp al, 4
        je .col
        mov esi, s_c256
        cmp al, 8
        je .col
        mov esi, s_c32k
        cmp al, 15
        je .col
        mov esi, s_c64k
        cmp al, 16
        je .col
        mov esi, s_c16m
.col:
        call pon_cad
        mov byte [edi], 0

        mov edi, t_video
        cmp byte [vid_tipo], 1
        je .vesa
        mov esi, s_vga
        call pon_cad
        jmp .vfin
.vesa:
        mov esi, s_vesa
        call pon_cad
        movzx eax, byte [vbe_ver+1]
        call pon_num
        mov al, '.'
        stosb
        movzx eax, byte [vbe_ver]
        call pon_num
        mov esi, s_lineal
        call pon_cad
.vfin:
        mov byte [edi], 0

        mov edi, t_dpmi
        mov esi, s_dpmi
        call pon_cad
        push edi
        mov ax, 0x0400
        int 0x31
        pop edi
        push eax
        movzx eax, ah
        call pon_num
        mov al, '.'
        stosb
        pop eax
        movzx eax, al
        call pon_num
        mov esi, s_dpmi2
        call pon_cad
        push edi
        mov ax, 0x0500
        mov edi, mem_info
        int 0x31
        pop edi
        mov eax, [mem_info]
        shr eax, 10
        call pon_num
        mov esi, s_kb
        call pon_cad
        mov byte [edi], 0

        ; ---- la ventana del escritorio ----
        mov edi, plant_tmp
        mov dword [edi+P_CLASE], c_escritorio
        xor eax, eax
        mov [edi+P_TEXTO], eax
        mov [edi+P_ESTILO], eax
        mov [edi+P_X], eax
        mov [edi+P_Y], eax
        mov [edi+P_ID], eax
        mov eax, [vid_ancho]
        mov [edi+P_AN], eax
        mov eax, [vid_alto]
        mov [edi+P_AL], eax
        mov esi, edi
        xor ebp, ebp
        call ven_crear
        mov [escritorio], eax
        mov ebp, eax
        call orden_quitar           ; no está en el orden: es el fondo
        popad
        ret

; las tres ventanas de muestra (la última queda delante)
esc_muestra:
        pushad
        mov eax, 60
        mov ebx, [vid_alto]
        sub ebx, 160
        mov ecx, 400
        mov edx, 150
        mov esi, tit_sistema
        mov edi, pinta_sistema
        call esc_crear_demo
        mov eax, [vid_ancho]
        sub eax, 380
        mov ebx, 110
        mov ecx, 300
        mov edx, 200
        mov esi, tit_paleta
        mov edi, pinta_paleta
        call esc_crear_demo
        mov eax, 30
        mov ebx, 50
        mov ecx, 440
        mov edx, 160
        mov esi, tit_bienvenida
        mov edi, pinta_bienvenida
        call esc_crear_demo
        popad
        ret

; (interna) EAX=x EBX=y ECX=ancho EDX=alto ESI=título EDI=pintor
esc_crear_demo:
        pushad
        mov dword [plant_tmp+P_CLASE], c_demo
        mov [plant_tmp+P_TEXTO], esi
        mov dword [plant_tmp+P_ESTILO], E_TITULO
        mov [plant_tmp+P_X], eax
        mov [plant_tmp+P_Y], ebx
        mov [plant_tmp+P_AN], ecx
        mov [plant_tmp+P_AL], edx
        mov esi, plant_tmp
        xor ebp, ebp
        call ven_crear
        test eax, eax
        jz .x
        mov [eax+V_D0], edi
.x:
        popad
        ret

; copia la cadena ESI (acabada en 0) a EDI, que avanza
pon_cad:
        lodsb
        test al, al
        jz .x
        stosb
        jmp pon_cad
.x:
        ret

; escribe EAX en decimal en EDI, que avanza
pon_num:
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor ecx, ecx
.d:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .d
.e:
        pop eax
        add al, '0'
        stosb
        loop .e
        pop edx
        pop ecx
        pop ebx
        ret

esc_inv_iconos:
        pushad
        mov ebp, [escritorio]
        test ebp, ebp
        jz .x
        xor eax, eax
        mov ebx, [vid_alto]
        sub ebx, ICO_ALTO
        mov ecx, [vid_ancho]
        mov edx, ICO_ALTO
        call ven_invalidar_rect
.x:
        popad
        ret

; (interna) el botón «Salir», a repintar
esc_inv_salir:
        pushad
        mov eax, [vid_ancho]
        sub eax, SALIR_AN + 12
        mov ebx, SALIR_Y
        mov ecx, SALIR_AN
        mov edx, SALIR_AL
        call ven_invalidar_rect
        popad
        ret

; (interna) EAX=x EBX=y -> CF=1 si el punto está en el botón «Salir»
esc_en_salir:
        push eax
        sub eax, [vid_ancho]
        add eax, SALIR_AN + 12
        js .no
        cmp eax, SALIR_AN
        jge .no
        cmp ebx, SALIR_Y
        jl .no
        cmp ebx, SALIR_Y + SALIR_AL
        jge .no
        pop eax
        stc
        ret
.no:
        pop eax
        clc
        ret

; (interna) ECX=número -> ESI=la ventana minimizada que hace ese número
; (CF=1 si no hay tantas)
esc_minimizada:
        push eax
        push ecx
        push edx
        mov esi, [ven_tabla]
        mov edx, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp dword [esi+V_PADRE], 0
        jne .s
        mov eax, [esi+V_ESTADO]
        and eax, S_VISIBLE | S_MINIM
        cmp eax, S_VISIBLE | S_MINIM
        jne .s
        dec ecx
        js .si
.s:
        add esi, V_TAM
        dec edx
        jnz .b
        stc
        jmp .x
.si:
        clc
.x:
        pop edx
        pop ecx
        pop eax
        ret

esc_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CREATE          ; [BATERÍA] primer vistazo y temporizador
        je .crear
        cmp eax, WM_TIMER           ; [BATERÍA] releer la batería cada pocos s
        je .timer
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_RBUTTONUP       ; [CONTEXTO] el menú del escritorio
        je ctx_def_rup
        cmp eax, WM_CONTEXTMENU
        je ctx_escritorio
        cmp eax, WM_COMMAND
        je ctx_esc_orden
        xor eax, eax
        ret
.crear:
        cmp byte [op_demo], 0       ; [BATERÍA] [RELOJ] en la demo, la barra como estaba
        jne .fin
        call bat_leer               ; una ojeada a la batería (la hay o no)
        call red_leer               ; [RED] ¿hay controlador de red cargado?
        call rel_leer               ; y a la hora de DOS
        call barra_firma            ; la firma de lo visible: la hora refresca
        mov [barra_firma_ant], eax
        mov ecx, 1                  ; temporizador nº 1 del escritorio
        mov edx, 5000               ; cada 5 s (EBP ya es el escritorio)
        call tmp_poner
        jmp .fin
.timer:
        call rel_leer               ; la hora, siempre (barato: INT 21h 2Ch)
        cmp byte [bat_hay], 0       ; la batería, solo si hay (no se gasta APM
        je .tf                      ; en un sobremesa cada 5 s)
        call bat_leer
.tf:
        call barra_firma
        cmp eax, [barra_firma_ant]  ; ¿ha cambiado algo (minuto o batería)?
        je .fin
        mov [barra_firma_ant], eax
        call barra_inv_dcha
        jmp .fin
.pulsa:
        mov edi, eax                ; [MEZCLADOR] (el mensaje: ¿doble clic?)
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        call esc_en_salir
        jnc .iconos
        mov byte [sal_armado], 1
        call esc_inv_salir
        jmp .fin
.iconos:
%ifdef CON_PROGRAMAS
        call vol_en_icono           ; el altavoz: abre o cierra el volumen
        jnc .ic2
        cmp edi, WM_LBUTTONDBLCLK   ; [MEZCLADOR] con doble clic, el Mezclador
        je .mez
        call vol_alternar
        jmp .fin
.mez:
        cmp dword [vol_ven], 0      ; (el primer clic abrió la ventanita)
        je .mz
        call vol_alternar
.mz:
        mov eax, 1
        call mez_abrir
        jmp .fin
.ic2:
%endif
        mov ecx, [vid_alto]
        sub ecx, ICO_ALTO
        cmp ebx, ecx
        jl .fin
        sub eax, 16
        js .fin
        xor edx, edx
        mov ecx, ICO_PASO
        div ecx
        mov ecx, eax
        call esc_minimizada
        jc .fin
        mov ebp, esi
        mov eax, 4
        call ven_mostrar
        jmp .fin
.suelta:
        cmp byte [sal_armado], 0
        je .fin
        mov byte [sal_armado], 0
        call esc_inv_salir
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        call esc_en_salir
        jnc .fin
        call apa_pedir              ; [APAGAR] ¿volver, reiniciar o apagar?
.fin:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        xor eax, eax                ; el fondo
        mov ebx, BARRA_H
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        mov esi, mosaico
        call dc_mosaico
        COLOR C_MARRON              ; la barra superior
        xor ebx, ebx
        mov edx, BARRA_H - 2
        call dc_rect
        COLOR C_OCRE
        mov ebx, BARRA_H - 2
        mov edx, 2
        call dc_rect
        COLOR C_CREMA
        mov byte [gdi_negrita], 1
        mov eax, 12
        mov ebx, 5
        mov esi, tit_barra
        call dc_texto
        mov byte [gdi_negrita], 0
        ; el botón «Salir» (se oscurece mientras está pulsado)
        COLOR C_TERRACOTA
        cmp byte [sal_armado], 0
        je .b1
        COLOR C_OCRE
.b1:
        mov eax, [vid_ancho]
        sub eax, SALIR_AN + 12
        mov ebx, SALIR_Y
        mov ecx, SALIR_AN
        mov edx, SALIR_AL
        call dc_rect
        COLOR C_CREMA
        call dc_marco
        cmp byte [sal_armado], 0
        je .b2
        COLOR C_TINTA
.b2:
        add eax, (SALIR_AN - 5*8) / 2
        inc ebx
        mov esi, tit_salir
        call dc_texto
%ifdef CON_PROGRAMAS
        call vol_pintar_icono
%endif
        call bat_pintar_icono       ; [BATERÍA] la pila, entre altavoz y «Salir»
        call red_pintar_icono       ; [RED] el icono de red, a la izquierda del altavoz
        call rel_pintar             ; [RELOJ] la hora, a la derecha de la pila
        ; los iconos de las ventanas minimizadas
        xor ecx, ecx
.ico:
        call esc_minimizada
        jc .pf
        mov edi, esi
        mov eax, ecx
        imul eax, ICO_PASO
        add eax, 16 + (ICO_PASO - 32) / 2
        mov ebx, [vid_alto]
        sub ebx, ICO_ALTO - 4
        mov esi, [edi+V_ICONO]
        test esi, esi
        jnz .i1
        mov esi, ico_ventana
.i1:
        call dc_icono
        ; el nombre, debajo (hasta 11 letras)
        mov esi, [edi+V_TEXTO]
        test esi, esi
        jz .i4
        xor edx, edx
.i2:
        cmp byte [esi+edx], 0
        je .i3
        inc edx
        cmp edx, 11
        jb .i2
.i3:
        push ecx
        mov eax, ecx
        imul eax, ICO_PASO
        add eax, 16 + ICO_PASO / 2
        lea ecx, [edx*4]
        sub eax, ecx
        add ebx, 36
        push edx
        COLOR C_PETROLEO
        sub eax, 3
        lea ecx, [edx*8+6]
        mov edx, 17
        call dc_rect
        add eax, 3
        pop ecx
        COLOR C_CREMA
        call dc_texto_n
        pop ecx
.i4:
        inc ecx
        jmp .ico
.pf:
        call dc_cerrar
        xor eax, eax
        ret

; ---- las ventanas de muestra ----
demo_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CLOSE
        je .salir
        cmp eax, WM_KEYDOWN
        jne ven_defecto
        cmp ecx, TC_ESC
        je .salir
        cmp ecx, TC_TAB
        jne .flechas
        call ges_siguiente
        jmp .fin
.flechas:
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        cmp ecx, TC_ARRIBA
        jne .t1
        sub ebx, 8
.t1:
        cmp ecx, TC_ABAJO
        jne .t2
        add ebx, 8
.t2:
        cmp ecx, TC_IZQ
        jne .t3
        sub eax, 8
.t3:
        cmp ecx, TC_DER
        jne .t4
        add eax, 8
.t4:
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        call ges_acotar
        cmp eax, [ebp+V_X]
        jne .mueve
        cmp ebx, [ebp+V_Y]
        je .fin
.mueve:
        call ven_mover
        jmp .fin
.salir:
        mov byte [salir], 1
.fin:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        call ven_cli
        xor eax, eax
        xor ebx, ebx
        call [ebp+V_D0]
        call dc_cerrar
        xor eax, eax
        ret

; pintores: EAX=x EBX=y ECX=ancho EDX=alto de la zona de la ventana
; escribe varias líneas: ESI -> (dx, dy, color, negrita, texto, 0)..., 0xFF
esc_lineas:
        pushad
        mov ebp, eax
        mov edi, ebx
.l:
        cmp byte [esi], 0xFF
        je .x
        movzx eax, byte [esi]
        add eax, ebp
        movzx ebx, byte [esi+1]
        add ebx, edi
        mov dl, [esi+2]
        mov [gdi_color], dl
        mov dl, [esi+3]
        mov [gdi_negrita], dl
        add esi, 4
        call dc_texto
.s:
        lodsb
        test al, al
        jnz .s
        jmp .l
.x:
        mov byte [gdi_negrita], 0
        popad
        ret

pinta_bienvenida:
        mov esi, lin_bienvenida
        jmp esc_lineas

pinta_sistema:
        push esi
        mov esi, lin_sistema
        call esc_lineas
        pop esi
        push eax
        push ebx
        push esi
        COLOR C_TINTA
        add eax, 14
        add ebx, 10
        mov esi, t_pantalla
        call dc_texto
        add ebx, 20
        mov esi, t_video
        call dc_texto
        add ebx, 20
        mov esi, s_raton_si
        cmp byte [hay_rat], 0
        jne .r
        mov esi, s_raton_no
.r:
        call dc_texto
        add ebx, 20
        mov esi, t_dpmi
        call dc_texto
        pop esi
        pop ebx
        pop eax
        ret

pinta_paleta:
        pushad
        mov ebp, eax                ; origen
        mov edi, ebx
        ; los 16 colores de Hispa Ventanas
        xor esi, esi
.c:
        mov eax, esi
        and eax, 7
        imul eax, 34
        lea eax, [eax+ebp+12]
        mov ebx, esi
        shr ebx, 3
        imul ebx, 28
        lea ebx, [ebx+edi+10]
        mov ecx, 30
        mov edx, 24
        mov [gdi_color], esi
        call dc_rect
        COLOR C_TINTA
        call dc_marco
        inc esi
        cmp esi, 16
        jb .c
        ; un abanico de líneas
        xor esi, esi
.l:
        lea eax, [esi+2]
        mov [gdi_color], al
        lea eax, [ebp+12]
        lea ebx, [edi+162]
        mov ecx, esi
        imul ecx, 13
        lea ecx, [ecx+ebp+12]
        lea edx, [edi+74]
        call dc_linea
        inc esi
        cmp esi, 11
        jb .l
        ; marcos uno dentro de otro
        xor esi, esi
.m:
        COLOR C_TERRACOTA
        test esi, 1
        jz .m2
        COLOR C_ANIL
.m2:
        mov eax, esi
        shl eax, 3
        mov ecx, 124
        sub ecx, eax
        sub ecx, eax
        lea eax, [eax+ebp+160]
        mov ebx, esi
        imul ebx, 6
        mov edx, 88
        sub edx, ebx
        sub edx, ebx
        lea ebx, [ebx+edi+74]
        call dc_marco
        inc esi
        cmp esi, 5
        jb .m
        COLOR C_OCRE
        lea eax, [ebp+200]
        lea ebx, [edi+104]
        mov ecx, 44
        mov edx, 28
        call dc_rect
        popad
        ret

; =============================================================================
;  [BATERÍA] indicador de nivel de batería (APM) en la barra superior
; =============================================================================
; (interna) lee el estado de la batería por APM (INT 15h) en modo real, con
; rm_int. Deja [bat_hay]=1 si hay una batería válida (un portátil con APM),
; con [bat_pct] (0..100) y [bat_ac] (1 si está enchufada o cargando). Si no
; hay APM, es un equipo de sobremesa, el porcentaje es desconocido o no se
; puede llamar a la BIOS en modo real, deja [bat_hay]=0 y no se dibuja nada.
; Con la opción de depuración /BATnn[C] usa un valor fijo, sin tocar la BIOS.
bat_leer:
        pushad
        cmp byte [op_bathay], 0     ; /BATnn[C]: valor forzado para las pruebas
        je .apm
        movzx eax, byte [op_bat]
        cmp eax, 100
        jbe .f1
        mov eax, 100
.f1:
        mov [bat_pct], eax
        movzx eax, byte [op_batac]
        mov [bat_ac], eax
        mov byte [bat_hay], 1
        jmp .x
.apm:
        mov byte [bat_hay], 0
        ; 1) ¿hay APM? INT 15h AX=5300h BX=0000h -> CF=0 y BX='PM'
        mov dword [rmcs+0x1C], 0x5300       ; EAX
        mov dword [rmcs+0x10], 0            ; EBX (dispositivo: la BIOS de APM)
        mov dword [rmcs+0x18], 0            ; ECX
        mov bl, 0x15
        call rm_int
        test byte [rmcs+0x20], 1            ; acarreo: no hay APM
        jnz .x
        cmp word [rmcs+0x10], 0x504D        ; BX = 'PM'
        jne .x
        ; 2) estado de todas las baterías: INT 15h AX=530Ah BX=0001h
        mov dword [rmcs+0x1C], 0x530A
        mov dword [rmcs+0x10], 1
        mov bl, 0x15
        call rm_int
        test byte [rmcs+0x20], 1            ; acarreo: sin estado
        jnz .x
        mov eax, [rmcs+0x10]                ; BX: BH=línea de CA, BL=estado
        mov edx, [rmcs+0x18]                ; CX: CH=bandera, CL=porcentaje
        test dh, 0x80                       ; CH bit 7: no hay batería (sobremesa)
        jnz .x
        movzx ecx, dl                       ; CL = porcentaje que queda
        cmp ecx, 0xFF                       ; desconocido
        je .x
        cmp ecx, 100
        jbe .pct
        mov ecx, 100
.pct:
        mov [bat_pct], ecx
        xor edx, edx                        ; ¿enchufada o cargando?
        cmp ah, 1                           ; BH=01h: hay línea de CA
        sete dl
        cmp al, 3                           ; BL=03h: cargando
        jne .ac
        mov dl, 1
.ac:
        mov [bat_ac], edx
        mov byte [bat_hay], 1
.x:
        ; el altavoz: con pila, a la izquierda del cuerpo (BAT_X+28); sin pila,
        ; pegado a la hora (REL_X+28) para no dejar hueco en blanco.
        mov eax, REL_X + 28
        cmp byte [bat_hay], 0
        je .setvx
        mov eax, BAT_X + 28
.setvx:
        mov [vol_x], eax
        popad
        ret

; dibuja el indicador: la pila (cuerpo con marco crema, borne + y relleno
; proporcional según el nivel —verde >50 %, ocre 20-50 %, rojo <20 %—) y el
; porcentaje al lado. Si está enchufada o cargando, un rayo blanco encima.
; Nada en la demo ni si no hay batería.
bat_pintar_icono:
        pushad
        cmp byte [op_demo], 0
        jne .x
        cmp byte [bat_hay], 0
        je .x
        mov esi, [vid_ancho]
        sub esi, BAT_X                      ; borde izquierdo del cuerpo
        ; el cuerpo: fondo oscuro (lo que le falta de carga)
        COLOR C_TINTA
        mov eax, esi
        mov ebx, BAT_Y
        mov ecx, BAT_AN
        mov edx, BAT_AL
        call dc_rect
        ; el relleno, proporcional (de izquierda a derecha)
        mov eax, [bat_pct]
        imul eax, BAT_AN - 2
        xor edx, edx
        mov ecx, 100
        div ecx                             ; EAX = ancho del relleno
        test eax, eax
        jz .marco
        mov ecx, eax                        ; ancho
        mov eax, C_OLIVA                    ; verde: más del 50 %
        cmp dword [bat_pct], 50
        ja .col
        mov eax, C_OCRE                     ; ocre: del 20 al 50 %
        cmp dword [bat_pct], 20
        jae .col
        mov eax, C_TERRACOTA               ; rojo: menos del 20 %
.col:
        mov [gdi_color], eax
        lea eax, [esi+1]
        mov ebx, BAT_Y + 1
        mov edx, BAT_AL - 2
        call dc_rect
.marco:
        COLOR C_CREMA                       ; el cuerpo y el borne, con marco crema
        mov eax, esi
        mov ebx, BAT_Y
        mov ecx, BAT_AN
        mov edx, BAT_AL
        call dc_marco
        lea eax, [esi + BAT_AN]             ; el borne +, a la derecha
        mov ebx, BAT_Y + 3
        mov ecx, BAT_BORNE
        mov edx, BAT_AL - 6
        call dc_rect
        cmp dword [bat_ac], 0               ; enchufada o cargando: un rayo
        je .num
        COLOR C_BLANCO
        lea eax, [esi+13]
        mov ebx, BAT_Y + 1
        lea ecx, [esi+9]
        mov edx, BAT_Y + 6
        call dc_linea
        lea eax, [esi+9]
        mov ebx, BAT_Y + 6
        lea ecx, [esi+12]
        mov edx, BAT_Y + 6
        call dc_linea
        lea eax, [esi+12]
        mov ebx, BAT_Y + 6
        lea ecx, [esi+8]
        mov edx, BAT_Y + 10
        call dc_linea
.num:
        mov edi, bat_txt                    ; el porcentaje ("85%")
        mov eax, [bat_pct]
        call pon_num
        mov al, '%'
        stosb
        mov byte [edi], 0
        COLOR C_CREMA
        mov eax, [vid_ancho]
        sub eax, BAT_NUM_X
        mov ebx, 5
        mov esi, bat_txt
        call dc_texto
.x:
        popad
        ret

; (interna) ¿hay un controlador de red cargado? Mira si algún vector de los de
; «packet driver» (INT 60h..80h) lleva la firma "PKT DRVR" 3 octetos después,
; como hace red_abrir de lib/red.inc. Solo lee la tabla de interrupciones (no
; llama al driver). Deja [red_hay]=1 si lo encuentra, 0 si no.
red_leer:
        pushad
        mov byte [red_hay], 0
        mov esi, [cero]                     ; la dirección lineal 0: la IVT
        mov bl, 0x60
.v:
        movzx eax, bl
        mov edx, [esi + eax*4]              ; el vector (segmento:desplazamiento)
        test edx, edx
        jz .sig
        movzx edi, dx                       ; desplazamiento
        shr edx, 16                         ; segmento
        shl edx, 4
        add edi, edx                        ; dirección lineal del manejador
        add edi, esi                        ; -> dirección en el selector plano
        cmp dword [edi+3], 'PKT '           ; "PKT DRVR", 3 octetos tras el vector
        jne .sig
        cmp dword [edi+7], 'DRVR'
        jne .sig
        mov byte [red_hay], 1
        jmp .fin
.sig:
        inc bl
        cmp bl, 0x80
        jbe .v
.fin:
        popad
        ret

; (interna) el icono de red, a la izquierda del altavoz: dos nodos unidos por una
; línea. En crema si hay controlador de red (HSPRED u otro packet driver), en
; arena oscura si no hay. Nada en la demo.
red_pintar_icono:
        pushad
        cmp byte [op_demo], 0
        jne .x
        mov esi, [vid_ancho]
        sub esi, [vol_x]
        sub esi, 24                         ; a la izquierda del altavoz
        mov eax, C_ARENA_OSC                ; apagado si no hay red
        cmp byte [red_hay], 0
        je .c
        mov eax, C_CREMA                    ; encendido si la hay
.c:
        mov [gdi_color], eax
        mov eax, esi                        ; nodo 1 (arriba-izquierda)
        mov ebx, 3
        mov ecx, 5
        mov edx, 4
        call dc_rect
        lea eax, [esi+8]                    ; nodo 2 (abajo-derecha)
        mov ebx, 8
        mov ecx, 5
        mov edx, 4
        call dc_rect
        lea eax, [esi+2]                    ; la línea que los une
        mov ebx, 6
        lea ecx, [esi+10]
        mov edx, 9
        call dc_linea
.x:
        popad
        ret

; (interna) [RELOJ] lee la hora de DOS (INT 21h función 2Ch, como el Reloj) en
; [bar_h] y [bar_m]
rel_leer:
        pushad
        mov ah, 0x2C
        int 0x21
        mov [bar_h], ch
        mov [bar_m], cl
        popad
        ret

; [RELOJ] dibuja la hora "HH:MM" (24 h) a la derecha de la batería, en crema.
; Se enseña siempre (también en un sobremesa sin batería). Nada en la demo.
rel_pintar:
        pushad
        cmp byte [op_demo], 0
        jne .x
        mov edi, bar_txt
        mov al, [bar_h]
        call rel_dos
        mov al, ':'
        stosb
        mov al, [bar_m]
        call rel_dos
        mov byte [edi], 0
        COLOR C_CREMA
        mov eax, [vid_ancho]
        sub eax, REL_X
        mov ebx, 5
        mov esi, bar_txt
        call dc_texto
.x:
        popad
        ret

; (interna) AL (0..59) en dos cifras decimales, a EDI (que avanza)
rel_dos:
        aam
        add ax, 0x3030
        xchg al, ah
        stosw
        ret

; (interna) [BATERÍA] [RELOJ] firma de lo que se ve a la derecha de la barra
; (batería y hora): si cambia, hay que repintar -> EAX
barra_firma:
        movzx eax, byte [bar_h]
        shl eax, 8
        mov al, [bar_m]
        shl eax, 8
        mov al, [bat_pct]           ; 0..100: cabe en un octeto
        shl eax, 2
        mov dl, [bat_hay]
        and dl, 1
        mov dh, [bat_ac]
        and dh, 1
        shl dh, 1
        or dl, dh
        or al, dl
        ret

; (interna) la zona de la derecha (pila, número y hora), a repintar
barra_inv_dcha:
        pushad
        mov ebp, [escritorio]
        test ebp, ebp
        jz .x
        mov eax, [vid_ancho]
        sub eax, BAT_X
        xor ebx, ebx
        mov ecx, BAT_X - (SALIR_AN + 12)    ; del cuerpo hasta «Salir»
        mov edx, BARRA_H - 2
        call ven_invalidar_rect
.x:
        popad
        ret
