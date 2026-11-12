; =============================================================================
;  Hispa Ventanas - Bloc de notas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un cuadro de texto de varias líneas que ocupa toda la ventana, con menús
;  para abrir y guardar archivos de texto (dos.inc) y para cortar, copiar y
;  pegar. Los archivos se guardan con líneas acabadas en CR+LF, como espera
;  DOS; dentro, el texto lleva solo LF. Hasta 60.000 caracteres.
; =============================================================================

BLOC_NUEVO  equ 101
BLOC_ABRIR  equ 102
BLOC_GUARDAR equ 103
BLOC_COMO   equ 104
BLOC_SALIR  equ 105
BLOC_CORTAR equ 111
BLOC_COPIAR equ 112
BLOC_PEGAR  equ 113
BLOC_TODO   equ 114
BLOC_BUSCAR equ 115
BLOC_ACERCA equ 121

c_bloc      dd bloc_proc
            db C_CREMA, 0, 0, 0
bloc_plant:
        PLANTILLA c_bloc, bloc_titulo, E_NORMAL, 150, 90, 520, 340, 0
bloc_edit_plant:
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA | ES_MULTI, 0, 0, 100, 100, 100

menu_bloc:
        db "&Archivo", 0
        dw BLOC_NUEVO
        db "&Nuevo", 0
        dw BLOC_ABRIR
        db "&Abrir...", 0
        dw BLOC_GUARDAR
        db "&Guardar", 0
        dw BLOC_COMO
        db "Guardar &como...", 0
        dw 0
        db 0
        dw BLOC_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw BLOC_CORTAR
        db "Cor&tar", 0
        dw BLOC_COPIAR
        db "&Copiar", 0
        dw BLOC_PEGAR
        db "&Pegar", 0
        dw 0
        db 0
        dw BLOC_TODO
        db "&Seleccionar todo", 0
        dw BLOC_BUSCAR
        db "&Buscar...", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw BLOC_ACERCA
        db "&Acerca del Bloc de notas...", 0
        dw 0xFFFF
        db 0

bloc_tarea:
        mov byte [bloc_nombre], 0
        call bloc_poner_titulo
        mov esi, bloc_plant
        xor ebp, ebp
        call ven_crear
        mov [bloc_ven], eax
        jmp app_bucle

; (interna) el título: «Bloc de notas - NOMBRE»
bloc_poner_titulo:
        pushad
        mov edi, bloc_titulo
        mov esi, t_bloc
        call pon_cad
        mov esi, t_guion
        call pon_cad
        mov esi, bloc_nombre
        cmp byte [esi], 0
        jne .n
        mov esi, t_sin_nombre
.n:
        call pon_cad
        mov byte [edi], 0
        mov ebp, [bloc_ven]
        test ebp, ebp
        jz .x
        mov esi, bloc_titulo
        call ven_texto
.x:
        popad
        ret

bloc_proc:
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov dword [bloc_ven], 0
        call msg_salir
        jmp .nada
.crear:
        mov dword [ebp+V_MENU], menu_bloc
        mov dword [ebp+V_ICONO], ico_bloc
        mov esi, bloc_edit_plant
        call ven_crear
        mov [bloc_edit], eax
        call ven_cli
        shl edx, 16
        mov dx, cx
.tam:
        movzx ecx, dx               ; el cuadro de texto ocupa toda la zona
        shr edx, 16
        xor eax, eax
        xor ebx, ebx
        mov ebp, [bloc_edit]
        call ven_mover
        jmp .nada
.foco:
        mov ebp, [bloc_edit]
        call ven_enfocar
        jmp .nada
.cerrar:
        call bloc_preguntar
        jc .nada
        call ven_destruir
        jmp .nada
.orden:
        mov eax, WM_CUT
        cmp ecx, BLOC_CORTAR
        je .aledit
        mov eax, WM_COPY
        cmp ecx, BLOC_COPIAR
        je .aledit
        mov eax, WM_PASTE
        cmp ecx, BLOC_PEGAR
        je .aledit
        mov eax, EM_SEL_TODO
        cmp ecx, BLOC_TODO
        je .aledit
        cmp ecx, BLOC_NUEVO
        je .nuevo
        cmp ecx, BLOC_ABRIR
        je .abrir
        cmp ecx, BLOC_GUARDAR
        je .guardar
        cmp ecx, BLOC_COMO
        je .como
        cmp ecx, BLOC_SALIR
        je .cerrar
        cmp ecx, BLOC_BUSCAR
        je .buscar
        cmp ecx, BLOC_ACERCA
        jne .nada
        mov esi, t_bloc_acerca
        mov edi, t_bloc
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        jmp .nada
.aledit:
        mov ebp, [bloc_edit]
        call msg_enviar
        jmp .nada
.nuevo:
        call bloc_preguntar
        jc .nada
        mov byte [bloc_nombre], 0
        mov ebp, [bloc_edit]
        xor ecx, ecx
        mov esi, bloc_nombre
        call ed_poner
        call bloc_poner_titulo
        jmp .nada
.abrir:
        call bloc_preguntar
        jc .nada
        sub esp, 80
        mov byte [esp], 0
        mov ebx, esp
        mov edi, t_abrir
        call dlg_archivo
        test eax, eax
        jz .a9
        mov esi, esp
        call bloc_cargar
.a9:
        add esp, 80
        jmp .nada
.guardar:
        call bloc_guardar
        jmp .nada
.buscar:
        mov esi, t_buscar_preg
        mov edi, t_buscar
        mov ebx, bloc_busca
        mov ecx, 40
        call dlg_pedir
        test eax, eax
        jz .nada
        call bloc_buscar
        jmp .nada
.como:
        call bloc_guardar_como
.nada:
        xor eax, eax
        ret

; (interna) busca bloc_busca desde el cursor y lo deja seleccionado
bloc_buscar:
        pushad
        mov esi, bloc_busca
        call cad_largo
        test eax, eax
        jz .x
        mov edx, eax                ; largo de lo que se busca
        mov ebp, [bloc_edit]
        mov ebx, [ebp+V_D0]
        mov eax, [ebp+V_D3]         ; desde el final de lo seleccionado
        cmp eax, [ebp+V_D4]
        jae .b0
        mov eax, [ebp+V_D4]
.b0:
        push eax
.b:
        lea ecx, [eax+edx]
        cmp ecx, [ebp+V_D1]
        ja .otra
        lea esi, [ebx+eax]
        mov edi, bloc_busca
        mov ecx, edx
        repe cmpsb
        je .si
        inc eax
        jmp .b
.otra:
        pop eax                     ; y, si no, otra vuelta desde el principio
        test eax, eax
        jz .no
        xor eax, eax
        push eax
        jmp .b
.si:
        add esp, 4
        add edx, eax
        call ed_seleccionar
        jmp .x
.no:
        mov ebp, [bloc_ven]
        mov esi, t_no_encontrado
        mov edi, t_buscar
        mov eax, DM_ACEPTAR
        call dlg_mensaje
.x:
        popad
        ret

; (interna) EBP=ventana del Bloc. Si el texto ha cambiado, pregunta si se
; guarda. CF=1 si no hay que seguir (se ha cancelado o no se ha guardado)
bloc_preguntar:
        pushad
        push ebp
        mov ebp, [bloc_edit]
        mov eax, EM_MODIFICADO
        call msg_enviar
        pop ebp
        test eax, eax
        jz .si
        mov esi, t_bloc_cambios
        mov edi, t_bloc
        mov eax, DM_SI_NO_CANCELAR
        call dlg_mensaje
        cmp eax, ID_NO
        je .si
        cmp eax, ID_SI
        jne .no
        call bloc_guardar
        jc .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; (interna) guarda con el nombre que tiene (o lo pide). CF=1 si no se guarda
bloc_guardar:
        cmp byte [bloc_nombre], 0
        jne bloc_escribir
bloc_guardar_como:
        pushad
        mov ebp, [bloc_ven]
        sub esp, 80
        mov edi, esp
        mov esi, bloc_nombre
        call pon_cad
        mov byte [edi], 0
        mov ebx, esp
        mov edi, t_guardar_como
        call dlg_archivo
        test eax, eax
        jz .no
        mov esi, esp                ; sin extensión: .TXT
        mov edi, bloc_nombre
        xor edx, edx
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '.'
        jne .c1
        inc edx
.c1:
        cmp al, '\'
        jne .c2
        xor edx, edx
.c2:
        stosb
        jmp .c
.f:
        test edx, edx
        jnz .f2
        mov esi, t_ext_txt
        call pon_cad
.f2:
        mov byte [edi], 0
        add esp, 80
        popad
        jmp bloc_escribir
.no:
        add esp, 80
        popad
        stc
        ret

; (interna) escribe el texto en bloc_nombre, con CR+LF. CF=1 si falla
bloc_escribir:
        pushad
        mov ebp, [bloc_edit]
        call ed_datos               ; ESI=texto ECX=octetos
        push ecx
        lea ecx, [ecx*2+16]
        call mem_pedir
        pop ecx
        jc .falla0
        mov edi, eax
        mov edx, eax                ; el búfer
.c:
        test ecx, ecx
        jz .cf
        lodsb
        dec ecx
        cmp al, 10
        jne .c1
        mov byte [edi], 13
        inc edi
.c1:
        stosb
        jmp .c
.cf:
        sub edi, edx
        mov esi, bloc_nombre
        call dos_crear
        jc .falla
        mov ebx, eax
        mov esi, edx
        mov ecx, edi
        call dos_escribir
        pushf
        call dos_cerrar
        popf
        jc .falla
        mov eax, edx
        call mem_soltar
        mov eax, EM_PONER_MODIF
        xor ecx, ecx
        call msg_enviar
        call bloc_poner_titulo
        popad
        clc
        ret
.falla:
        mov eax, edx
        call mem_soltar
.falla0:
        mov ebp, [bloc_ven]
        mov esi, t_no_guardar
        mov edi, t_bloc
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        popad
        stc
        ret

; (interna) carga el archivo ESI en el cuadro de texto
bloc_cargar:
        pushad
        xor al, al
        call dos_abrir
        jc .falla
        mov ebx, eax
        mov ecx, ED_MAX_MULTI + 8192
        call mem_pedir
        jc .falla1
        mov edi, eax
        call dos_leer
        mov ecx, eax
        call dos_cerrar
        push esi
        mov esi, edi
        mov ebp, [bloc_edit]
        call ed_poner
        pop esi
        mov eax, edi
        call mem_soltar
        mov edi, bloc_nombre
        call pon_cad
        mov byte [edi], 0
        call bloc_poner_titulo
        popad
        ret
.falla1:
        call dos_cerrar
.falla:
        mov ebp, [bloc_ven]
        mov esi, t_no_abrir
        mov edi, t_bloc
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        popad
        ret
