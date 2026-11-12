; =============================================================================
;  Hispa Ventanas - el volumen: altavoz de la barra superior y su ventanita
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  En la barra superior, a la izquierda de «Salir», hay un altavoz (dibujo
;  propio). Un clic abre o cierra una ventanita con una barra deslizante
;  (0-100 %; también con las flechas) y la casilla «Silencio». El volumen se
;  le da al driver de sonido HSPSND por su interfaz (INT 2Fh AX=D700h ->
;  punto de entrada en ES:BX; función 04h, AL=0..31), llamándolo en modo real
;  (rm_snd, en ventanas.asm). Con HSPSND de API 2 o más se lee el
;  volumen de verdad (06h) y se pone en % (05h) y silencio (07h); con uno
;  anterior, el último se guarda en CENTRO.INI (Volumen, Silencio).
;  Sin HSPSND, o si no ha encontrado tarjeta, el altavoz sale gris y la
;  ventanita lo explica.
;
;    snd_iniciar      busca HSPSND y pone el volumen guardado
;    vol_pintar_icono (escritorio) dibuja el altavoz
;    vol_en_icono     EAX=x EBX=y -> CF=1 si el punto está en el altavoz
;    vol_alternar     abre o cierra la ventanita
; =============================================================================

VOL_X       equ SALIR_AN + 12 + 36  ; del borde derecho al altavoz

c_volumen   dd vol_proc
            db C_CREMA, 0, 0, 0
vol_hijo:
        PLANTILLA c_boton, t_vol_mudo, E_HIJA | E_TABULA | BS_CASILLA, 16, 56, 150, 20, 20
        dd 0

snd_iniciar:
        pushad
        mov dword [rmcs+0x1C], 0xD700
        mov dword [rmcs+0x10], 0
        mov bl, 0x2F
        call rm_int
        cmp byte [rmcs+0x1C], 0xFF
        jne .x
        mov ax, [rmcs+0x10]
        mov [snd_entrada], ax
        mov ax, [rmcs+0x22]
        mov [snd_entrada+2], ax
        xor eax, eax                ; función 00h: ¿qué tarjeta?
        call snd_llamar
        test al, al
        jz .x
        mov byte [snd_hay], 1
        mov ax, [rmcs+0x10]         ; BX = versión de la API de HSPSND
        mov [snd_ver], ax
        cmp ax, 2
        jb .vieja
        ; API 2: se puede leer el volumen de verdad (función 06h)
        mov eax, 0x0600
        call snd_llamar
        movzx edx, al
        mov [vol_pct], edx
        movzx edx, ah
        and edx, 1
        mov [vol_mudo], edx
        jmp .x
.vieja:
        cmp dword [vol_pct], 0      ; (sin volumen guardado no se toca)
        jl .x
        call vol_aplicar
.x:
        popad
        ret

; llama a HSPSND: AH=función AL=dato -> EAX como lo deja
snd_llamar:
        mov [rmcs+0x1C], eax
        mov word [rmcs+0x2A], rm_snd
        call rm_llamar
        mov eax, [rmcs+0x1C]
        ret

; (interna) -> EAX=el volumen de 0 a 100 (100 si no se sabe)
vol_valor:
        mov eax, [vol_pct]
        test eax, eax
        jns .x
        mov eax, 100
.x:
        ret

; (interna) da a HSPSND el volumen que hay puesto
vol_aplicar:
        pushad
        cmp byte [snd_hay], 0
        je .x
        cmp word [snd_ver], 2
        jb .v1
        ; API 2: silencio con 07h, volumen en % con 05h (que quita el silencio)
        mov eax, 0x0701
        cmp dword [vol_mudo], 0
        jne .p2
        call vol_valor
        mov ah, 0x05
.p2:
        call snd_llamar
        jmp .x
.v1:
        xor eax, eax
        cmp dword [vol_mudo], 0
        jne .p
        call vol_valor
        imul eax, 31
        add eax, 50
        xor edx, edx
        mov ecx, 100
        div ecx
.p:
        mov ah, 0x04
        call snd_llamar
.x:
        popad
        ret

vol_en_icono:
        push ecx
        cmp byte [op_demo], 0
        jne .no
        mov ecx, [vid_ancho]
        sub ecx, VOL_X + 2
        cmp eax, ecx
        jl .no
        add ecx, 26
        cmp eax, ecx
        jge .no
        cmp ebx, 2
        jl .no
        cmp ebx, BARRA_H - 3
        jge .no
        pop ecx
        stc
        ret
.no:
        pop ecx
        clc
        ret

; (interna) el altavoz de la barra, a repintar
vol_inv_icono:
        pushad
        mov ebp, [escritorio]
        test ebp, ebp
        jz .x
        mov eax, [vid_ancho]
        sub eax, VOL_X + 2
        xor ebx, ebx
        mov ecx, 28
        mov edx, BARRA_H - 2
        call ven_invalidar_rect
.x:
        popad
        ret

; el altavoz: imán, cono y dos ondas (o un aspa si está en silencio)
vol_pintar_icono:
        pushad
        cmp byte [op_demo], 0
        jne .x
        mov esi, [vid_ancho]
        sub esi, VOL_X
        mov edi, 5
        COLOR C_CREMA
        cmp byte [snd_hay], 0
        jne .c
        COLOR C_ARENA_OSC
.c:
        lea eax, [esi+1]
        lea ebx, [edi+5]
        mov ecx, 4
        mov edx, 6
        call dc_rect
        mov dword [vol_i], 0
.k:
        mov ecx, [vol_i]
        lea eax, [esi+ecx+5]
        lea ebx, [edi+5]
        sub ebx, ecx
        lea edx, [ecx*2+6]
        mov ecx, 1
        call dc_rect
        inc dword [vol_i]
        cmp dword [vol_i], 6
        jb .k
        cmp byte [snd_hay], 0
        je .x
        cmp dword [vol_mudo], 0
        jne .mudo
        lea eax, [esi+13]
        lea ebx, [edi+6]
        mov ecx, 1
        mov edx, 4
        call dc_rect
        lea eax, [esi+16]
        lea ebx, [edi+3]
        mov edx, 10
        call dc_rect
        lea eax, [esi+15]
        lea ebx, [edi+2]
        call dc_punto
        lea ebx, [edi+13]
        call dc_punto
        jmp .x
.mudo:
        COLOR C_TERRACOTA_CL
        lea eax, [esi+13]
        lea ebx, [edi+5]
        lea ecx, [esi+19]
        lea edx, [edi+11]
        call dc_linea
        lea ebx, [edi+11]
        lea edx, [edi+5]
        call dc_linea
        lea eax, [esi+14]
        lea ecx, [esi+20]
        call dc_linea
        lea ebx, [edi+5]
        lea edx, [edi+11]
        call dc_linea
.x:
        popad
        ret

vol_alternar:
        pushad
        mov ebp, [vol_ven]
        test ebp, ebp
        jz .abrir
        call ven_destruir
        jmp .x
.abrir:
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_volumen
        mov dword [esi+P_TEXTO], t_volumen
        mov dword [esi+P_ESTILO], E_TITULO | E_CERRAR
        mov eax, [vid_ancho]
        sub eax, 250
        mov [esi+P_X], eax
        mov dword [esi+P_Y], BARRA_H + 6
        mov dword [esi+P_AN], 236
        mov dword [esi+P_AL], 116
        mov dword [esi+P_ID], 0
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [vol_ven], eax
.x:
        popad
        ret

vol_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_DESTROY
        je .destruir
        jmp ven_defecto
.crear:
        cmp byte [snd_hay], 0
        je .nada
        mov esi, vol_hijo
        call ven_lista
        mov ecx, 20
        mov edx, [vol_mudo]
        call ro_poner
        jmp .nada
.pulsa:
        cmp byte [snd_hay], 0
        je .nada
        call raton_xy
        jc .nada
        cmp ebx, 14
        jl .nada
        cmp ebx, 48
        jge .nada
        mov byte [vol_arr], 1
        jmp .pon
.mueve:
        cmp byte [vol_arr], 0
        je .nada
        movsx eax, dx
.pon:
        sub eax, 18                 ; del punto de la barra al tanto por ciento
        jns .p1
        xor eax, eax
.p1:
        shr eax, 1
        cmp eax, 100
        jbe .p2
        mov eax, 100
.p2:
        cmp eax, [vol_pct]
        je .nada
        mov [vol_pct], eax
.cambio:
        call vol_aplicar
        call ven_invalidar
        call vol_inv_icono
        jmp .nada
.suelta:
        mov byte [vol_arr], 0
        jmp .nada
.tecla:
        cmp ecx, TC_ESC
        jne .t1
        call ven_destruir
        jmp .nada
.t1:
        cmp byte [snd_hay], 0
        je .nada
        call vol_valor
        cmp ecx, TC_DER
        jne .t2
        add eax, 5
        cmp eax, 100
        jle .tp
        mov eax, 100
        jmp .tp
.t2:
        cmp ecx, TC_IZQ
        jne .nada
        sub eax, 5
        jns .tp
        xor eax, eax
.tp:
        mov [vol_pct], eax
        jmp .cambio
.orden:
        cmp cx, 20
        jne .nada
        mov ecx, 20
        call ro_marca
        movzx eax, al
        mov [vol_mudo], eax
        jmp .cambio
.destruir:
        mov dword [vol_ven], 0
        mov byte [vol_arr], 0
        call cfg_guardar            ; el volumen, a CENTRO.INI
.nada:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        COLOR C_TINTA
        cmp byte [snd_hay], 0
        jne .p0
        mov eax, 14
        mov ebx, 14
        mov esi, t_vol_no
        call dc_texto
        mov ebx, 34
        mov esi, t_vol_no2
        call dc_texto
        mov ebx, 54
        mov esi, t_vol_no3
        call dc_texto
        jmp .pf
.p0:
        mov edi, vol_txt
        mov esi, t_vol_pct
        call pon_cad
        call vol_valor
        push eax
        call pon_num
        mov eax, ' %'
        stosd
        pop edi                     ; EDI = el tanto por ciento
        mov eax, 16
        mov ebx, 4
        mov esi, vol_txt
        call dc_texto
        COLOR C_ARENA_OSC           ; el carril, con lo puesto en añil
        mov eax, 18
        mov ebx, 30
        mov ecx, 200
        mov edx, 6
        call dc_rect
        COLOR C_ANIL
        lea ecx, [edi*2]
        test ecx, ecx
        jz .p3
        call dc_rect
.p3:
        COLOR C_MARRON
        mov ecx, 200
        call dc_marco
        COLOR C_TERRACOTA           ; el pulgar
        lea eax, [edi*2+14]
        mov ebx, 24
        mov ecx, 9
        mov edx, 18
        call dc_rect
        COLOR C_TINTA
        call dc_marco
.pf:
        call dc_cerrar
        xor eax, eax
        ret
