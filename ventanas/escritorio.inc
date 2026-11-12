; =============================================================================
;  Hispa Ventanas - el escritorio y las ventanas de muestra
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El escritorio es una ventana más (la del fondo, de la tarea del sistema):
;  mosaico de rombos en azul petróleo, barra superior marrón con filete ocre y
;  el botón «Salir», y abajo los iconos de las ventanas minimizadas (un clic
;  las restaura).
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
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_LBUTTONUP
        je .suelta
        xor eax, eax
        ret
.pulsa:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        call esc_en_salir
        jnc .iconos
        mov byte [sal_armado], 1
        call esc_inv_salir
        jmp .fin
.iconos:
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
        mov byte [salir], 1
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
