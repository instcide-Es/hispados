; =============================================================================
;  Hispa Ventanas - Reloj
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Esfera con agujas que se adapta al tamaño de la ventana y, debajo, la hora
;  en cifras. En «Opciones» (un diálogo con una casilla y dos opciones) se
;  quita la aguja de los segundos o se deja el reloj solo con las cifras. Un temporizador de medio segundo mira la hora de DOS (función
;  2Ch) y solo se repinta cuando cambia el segundo. Los senos salen de una
;  tabla de un cuarto de vuelta (de 6 en 6 grados), sin coprocesador.
; =============================================================================

c_reloj     dd reloj_proc
            db C_CREMA, 0, 0, 0
reloj_plant:
        PLANTILLA c_reloj, t_reloj, E_NORMAL, 560, 330, 200, 252, 0
ro_ventana:
        PLANTILLA c_dialogo, t_reloj_op, E_DIALOGO, 0, 0, 264, 180, 0
ro_hijos:
        PLANTILLA c_boton, t_ro_seg, E_HIJA | E_TABULA | BS_CASILLA, 20, 14, 220, 20, 20
        PLANTILLA c_boton, t_ro_agujas, E_HIJA | E_TABULA | BS_OPCION, 20, 44, 220, 20, 21
        PLANTILLA c_boton, t_ro_cifras, E_HIJA | E_TABULA | BS_OPCION, 20, 68, 220, 20, 22
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 44, 106, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 136, 106, 80, 26, ID_CANCELAR
        dd 0
menu_reloj:
        db "&Reloj", 0
        dw 301
        db "&Opciones...", 0
        dw 0
        db 0
        dw 302
        db "&Salir", 0
        dw 0xFFFF
        db 0
reloj_seg   db 1                    ; con aguja de segundos
reloj_cifras db 0                   ; solo las cifras

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
        jmp .no
.inicio:
        mov ecx, 20
        movzx edx, byte [reloj_seg]
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

; seno por mil de 0 a 90 grados, de 6 en 6
reloj_senos dw 0, 105, 208, 309, 407, 500, 588, 669, 743, 809, 866, 914, 951, 978, 995, 1000

reloj_tarea:
        mov esi, reloj_plant
        xor ebp, ebp
        call ven_crear
        mov [reloj_ven], eax
        jmp app_bucle

; (interna) EAX=sesentavo de vuelta (0 arriba, hacia la derecha) -> EAX=seno por mil
reloj_seno:
        push ebx
        push edx
        xor edx, edx
        mov ebx, 60
        div ebx
        mov eax, edx
        xor edx, edx
        mov ebx, 15
        div ebx                     ; EAX=cuarto EDX=dentro del cuarto
        test al, 1
        jz .a
        neg edx
        add edx, 15
.a:
        movzx edx, word [reloj_senos+edx*2]
        test al, 2
        jz .b
        neg edx
.b:
        mov eax, edx
        pop edx
        pop ebx
        ret

; (interna) EAX=sesentavo ECX=largo -> EAX=x EBX=y del extremo (centro en
; reloj_cx, reloj_cy)
reloj_punto:
        push edx
        push esi
        mov esi, eax
        call reloj_seno
        imul eax, ecx
        mov ebx, 1000
        cdq
        idiv ebx
        add eax, [reloj_cx]
        push eax
        lea eax, [esi+15]
        call reloj_seno             ; coseno
        imul eax, ecx
        cdq
        idiv ebx
        mov ebx, [reloj_cy]
        sub ebx, eax
        pop eax
        pop esi
        pop edx
        ret

; (interna) aguja: EAX=sesentavo ECX=largo EDX=grosor (1 a 3)
reloj_aguja:
        pushad
        mov edi, edx
        call reloj_punto
        mov ecx, eax
        mov edx, ebx
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        call dc_linea
        cmp edi, 2
        jb .x
        inc eax
        inc ecx
        call dc_linea
        dec eax
        dec ecx
        inc ebx
        inc edx
        call dc_linea
        cmp edi, 3
        jb .x
        inc eax
        inc ecx
        call dc_linea
.x:
        popad
        ret

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
        jne .nada
        mov esi, ro_ventana
        mov edi, ro_hijos
        mov ebx, ro_gancho
        xor edx, edx
        call dlg_modal
        jmp .inv
.crear:
        mov dword [ebp+V_ICONO], ico_reloj
        mov dword [ebp+V_MENU], menu_reloj
        mov ecx, 1
        mov edx, 500
        call tmp_poner
        call reloj_hora
        jmp .nada
.tic:
        mov al, [reloj_s]
        call reloj_hora
        cmp al, [reloj_s]
        je .nada
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        call ven_cli
        mov eax, ecx
        shr eax, 1
        mov [reloj_cx], eax
        sub edx, 26                 ; abajo, la hora en cifras
        mov ebx, edx
        shr ebx, 1
        mov [reloj_cy], ebx
        cmp eax, ebx                ; el radio: lo que quepa
        jbe .r1
        mov eax, ebx
.r1:
        sub eax, 8
        jg .r2
        mov eax, 1
.r2:
        mov [reloj_r], eax
        cmp byte [reloj_cifras], 0
        je .esfera
        mov eax, [reloj_cy]         ; solo las cifras, en el centro
        sub eax, 11
        sar eax, 1
        mov [reloj_cy], eax
        jmp .cifras
.esfera:
        ; el borde: sesenta tramos
        COLOR C_MARRON
        xor edi, edi
.borde:
        mov eax, edi
        mov ecx, [reloj_r]
        call reloj_punto
        push eax
        push ebx
        lea eax, [edi+1]
        call reloj_punto
        mov ecx, eax
        mov edx, ebx
        pop ebx
        pop eax
        call dc_linea
        inc edi
        cmp edi, 60
        jb .borde
        ; las doce horas: rombos
        xor edi, edi
.horas:
        COLOR C_TERRACOTA
        mov eax, edi
        mov ecx, [reloj_r]
        sub ecx, 9
        jg .h1
        mov ecx, 1
.h1:
        call reloj_punto
        mov ecx, 2
        xor edx, edx                ; las 12, 3, 6 y 9, más grandes (cada 15)
        push eax
        mov eax, edi
        mov esi, 15
        div esi
        pop eax
        test edx, edx
        jnz .h2
        mov ecx, 4
        COLOR C_MARRON
.h2:
        call dc_rombo
        add edi, 5
        cmp edi, 60
        jb .horas
        ; las agujas
        COLOR C_TINTA
        movzx eax, byte [reloj_h]
        xor edx, edx
        mov ebx, 12
        div ebx
        lea eax, [edx*4+edx]        ; hora x 5 + minutos / 12
        push eax
        movzx eax, byte [reloj_m]
        xor edx, edx
        div ebx
        pop edx
        add eax, edx
        mov ecx, [reloj_r]
        shr ecx, 1
        mov edx, 3
        call reloj_aguja
        COLOR C_ANIL
        movzx eax, byte [reloj_m]
        mov ecx, [reloj_r]
        imul ecx, 3
        shr ecx, 2
        mov edx, 2
        call reloj_aguja
        cmp byte [reloj_seg], 0
        je .sinseg
        COLOR C_GRANATE
        movzx eax, byte [reloj_s]
        mov ecx, [reloj_r]
        imul ecx, 7
        shr ecx, 3
        mov edx, 1
        call reloj_aguja
.sinseg:
        COLOR C_TINTA
        mov eax, [reloj_cx]
        mov ebx, [reloj_cy]
        mov ecx, 3
        call dc_rombo
        ; la hora en cifras
.cifras:
        mov edi, reloj_txt
        mov al, [reloj_h]
        call reloj_dos
        mov al, ':'
        stosb
        mov al, [reloj_m]
        call reloj_dos
        mov al, ':'
        stosb
        mov al, [reloj_s]
        call reloj_dos
        mov byte [edi], 0
        COLOR C_TINTA
        mov byte [gdi_negrita], 1
        mov eax, [reloj_cx]
        sub eax, 32
        mov ebx, [reloj_cy]
        shl ebx, 1
        add ebx, 6
        mov esi, reloj_txt
        call dc_texto
        mov byte [gdi_negrita], 0
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

; (interna) lee la hora de DOS
reloj_hora:
        pushad
        mov ah, 0x2C
        int 0x21
        mov [reloj_h], ch
        mov [reloj_m], cl
        mov [reloj_s], dh
        popad
        ret
