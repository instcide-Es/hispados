; =============================================================================
;  Hispa Ventanas - [APAGAR] salir: volver a Hispa-DOS, reiniciar o apagar el equipo
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Este archivo va en la «parte alta» de VENTANAS.COM (ventanas.asm): solo
;  código y datos de 32 bits.
;
;  El botón «Salir» de la barra de arriba, Alt+F4 sin ninguna ventana activa
;  (en el escritorio), cerrar el Centro de programas y su «Archivo > Salir a
;  Hispa-DOS» abren el mismo cuadro: «¿Qué quiere hacer?», con un botón
;  grande y su dibujo para cada cosa (Volver a Hispa-DOS, Reiniciar el
;  equipo, Apagar el equipo) y Cancelar. Intro es «Volver a Hispa-DOS»; las
;  flechas pasan de un botón a otro; Esc cancela.
;  Reiniciar y apagar lo hacen REINICIA.COM y APAGAR.COM, de la carpeta de
;  Hispa Ventanas, como un juego: Hispa Ventanas guarda CENTRO.INI como al
;  salir, deja el encargo al lanzador (ventanas.asm) sin «volver», vuelve a
;  modo texto y termina; el lanzador quita HSPDPMI de la memoria y ejecuta
;  el programa, que escribe los búferes del disco y apaga o reinicia.
;
;    apa_pedir    (sistema: el escritorio, Alt+F4) abre el cuadro en una
;                 tarea propia (si no está ya abierto)
;    apa_dialogo  (una tarea) EBP=dueña (0: ninguna): el cuadro, y lo que se
;                 elija; pone [salir] = 1 (volver) o 2 (encargo)
; =============================================================================

APA_VOLVER  equ ID_ACEPTAR          ; (Intro)
APA_REINI   equ 3
APA_APAGAR  equ 4
APA_AN      equ 372                 ; la zona interior del cuadro
APA_AL      equ 230
APA_FILA    equ 44                  ; la primera fila (dibujo y botón)
APA_PASO    equ 44
APA_LINEA   equ APA_FILA + 3 * APA_PASO - 2   ; el filete de encima de Cancelar

apa_tit     db "Salir de Hispa Ventanas", 0
apa_preg    db "¿Qué quiere hacer?", 0
apa_t_volver db "Volver a Hispa-DOS", 0
apa_t_reini db "Reiniciar el equipo", 0
apa_t_apagar db "Apagar el equipo", 0
apa_n_apagar db "APAGAR.COM", 0
apa_n_reini db "REINICIA.COM", 0
apa_no_esta db "No se encuentra el programa que lo hace", 10
            db "en la carpeta de Hispa Ventanas:", 10, 0
apa_bloc    db "El Bloc de notas tiene cambios sin guardar", 10
            db "y se perderán. ¿Continuar?", 0
apa_abierto dd 0
apa_ruta    times 100 db 0
apa_aviso   times 160 db 0

apa_ventana:
        PLANTILLA c_dialogo, apa_tit, E_DIALOGO, 0, 0, APA_AN + 2*BORDE, APA_AL + 2*BORDE + TIT_H + 1, 0
apa_hijos:
        PLANTILLA c_boton, apa_t_volver, E_HIJA | E_TABULA | BS_DEFECTO, 68, APA_FILA + 3, 280, 28, APA_VOLVER
        PLANTILLA c_boton, apa_t_reini, E_HIJA | E_TABULA, 68, APA_FILA + APA_PASO + 3, 280, 28, APA_REINI
        PLANTILLA c_boton, apa_t_apagar, E_HIJA | E_TABULA, 68, APA_FILA + 2*APA_PASO + 3, 280, 28, APA_APAGAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, APA_AN - 24 - 100, APA_LINEA + 14, 100, 26, ID_CANCELAR
        PLANTILLA c_estatico, apa_preg, E_HIJA | SS_NEGRITA, 24, 14, 300, 18, 0
        dd 0
apa_dibujos dd ico_ordenes, ico_reiniciar, ico_apagar

apa_pedir:
        pushad
        cmp dword [apa_abierto], 0
        jne .x
        mov eax, apa_tarea
        call tar_crear
.x:
        popad
        ret

apa_tarea:
        xor ebp, ebp
        call apa_dialogo
        ret

apa_dialogo:
        pushad
        cmp dword [apa_abierto], 0
        jne .x
        mov dword [apa_abierto], 1
        mov esi, apa_ventana
        mov edi, apa_hijos
        mov ebx, apa_gancho
        xor edx, edx
        call dlg_modal
        mov dword [apa_abierto], 0
        cmp eax, APA_VOLVER
        jne .r
        mov byte [salir], 1
        jmp .x
.r:
        mov esi, apa_n_reini
        cmp eax, APA_REINI
        je .enc
        mov esi, apa_n_apagar
        cmp eax, APA_APAGAR
        jne .x
.enc:
        call apa_encargo
.x:
        popad
        ret

; ESI = el programa (en la carpeta de Hispa Ventanas), EBP = dueña: se le
; deja al lanzador y se sale (o se avisa de por qué no)
apa_encargo:
        pushad
        cmp word [lz_seg], 0
        jne .l
        mov esi, t_sin_lanzador
        jmp .aviso
.l:
        call dir_propia_poner
        mov edi, apa_ruta
        push esi
        mov esi, dir_propia
        call pon_cad
        pop esi
        call pon_cad
        mov byte [edi], 0
        mov esi, apa_ruta           ; ¿está?
        xor al, al
        call dos_abrir
        jnc .esta
        mov edi, apa_aviso
        mov esi, apa_no_esta
        call pon_cad
        mov esi, apa_ruta
        call pon_cad
        mov byte [edi], 0
        mov esi, apa_aviso
        jmp .aviso
.esta:
        mov ebx, eax
        call dos_cerrar
        cmp dword [bloc_ven], 0     ; ¿texto sin guardar en el Bloc de notas?
        je .poner
        push ebp
        mov ebp, [bloc_edit]
        mov eax, EM_MODIFICADO
        call msg_enviar
        pop ebp
        test eax, eax
        jz .poner
        mov esi, apa_bloc
        mov edi, apa_tit
        mov eax, DM_ACEPTAR_CANCELAR
        call dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .x
.poner:
        movzx edx, word [lz_seg]    ; el encargo, a la memoria del lanzador
        shl edx, 4
        add edx, [cero]
        mov esi, apa_ruta
        lea edi, [edx+lz_prog]
        mov ecx, 79
        call cad_copiar
        mov byte [edx+lz_param], 0
        mov byte [edx+lz_dir], 0
        mov byte [edx+lz_band], 0   ; sin volver
        mov byte [edx+lz_orden], 1
        mov byte [salir], 2
        jmp .x
.aviso:
        mov edi, apa_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
.x:
        popad
        ret

; el gancho del cuadro: los dibujos y el filete, y las flechas
apa_gancho:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_KEYDOWN
        jne .no
        xor eax, eax
        cmp ecx, TC_ABAJO
        je .foco
        cmp ecx, TC_DER
        je .foco
        inc eax
        cmp ecx, TC_ARRIBA
        je .foco
        cmp ecx, TC_IZQ
        jne .no
.foco:
        mov ebp, [foco]
        test ebp, ebp
        jz .si
        call ven_foco_sig
.si:
        mov eax, 1
        ret
.no:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        xor ecx, ecx
.d:
        mov esi, [apa_dibujos+ecx*4]
        mov eax, 24
        imul ebx, ecx, APA_PASO
        add ebx, APA_FILA
        call dc_icono
        inc ecx
        cmp ecx, 3
        jb .d
        COLOR C_ARENA_OSC           ; el filete de encima de Cancelar
        mov eax, 16
        mov ebx, APA_LINEA
        mov ecx, APA_AN - 32
        mov edx, 1
        call dc_rect
        COLOR C_BLANCO
        inc ebx
        call dc_rect
        call dc_cerrar
        jmp .si

; los dibujos (32x32, en texto: tools/icopack.py los empaqueta al compilar)
ico_reiniciar:
        db "................................"
        db "................................"
        db ".................2.............."
        db ".................22............."
        db ".................222............"
        db ".................2222..........."
        db ".............222222222.........."
        db "..........22222222222222........"
        db ".........22222222222222........."
        db "........22222222222222.........."
        db ".......22222.....2222..........."
        db "......22222......222............"
        db ".....22222.......2.............."
        db ".....2222......................."
        db ".....222..................2....."
        db "....2222................2222...."
        db "....2222................2222...."
        db "....2222................2222...."
        db "....2222................2222...."
        db "....2222................2222...."
        db "....2222................2222...."
        db ".....222................222....."
        db ".....2222..............2222....."
        db ".....22222............22222....."
        db "......22222..........22222......"
        db ".......22222........22222......."
        db "........2222222222222222........"
        db ".........22222222222222........."
        db "..........222222222222.........."
        db ".............222222............."
        db "................................"
        db "................................"

ico_apagar:
        db "................................"
        db "................................"
        db "................................"
        db "..............dddd.............."
        db "..............dddd.............."
        db "..............dddd.............."
        db "..............dddd.............."
        db "..............dddd.............."
        db ".......dd.....dddd....dd........"
        db "......dddd....dddd...dddd......."
        db "......dddd....dddd...dddd......."
        db ".....dddd.....dddd....dddd......"
        db ".....dddd.....dddd....dddd......"
        db "....dddd......dddd.....dddd....."
        db "....dddd......dddd.....dddd....."
        db "....dddd......dddd.....dddd....."
        db "....dddd......dddd.....dddd....."
        db "....dddd......dddd.....dddd....."
        db "....dddd...............dddd....."
        db "....dddd...............dddd....."
        db ".....dddd.............dddd......"
        db ".....dddd.............dddd......"
        db "......dddd...........dddd......."
        db "......dddddd.......dddddd......."
        db ".......ddddddddddddddddd........"
        db "........ddddddddddddddd........."
        db "..........ddddddddddd..........."
        db "............ddddddd............."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
