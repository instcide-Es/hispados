; =============================================================================
;  PLUMA.HVP - Pluma, el procesador de textos de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un procesador de textos sencillo, tipo «WordPad» básico (todavía sin
;  formato enriquecido). Una ventana con barra de menús (Archivo y Edición) y,
;  llenando casi toda la zona, un cuadro de texto de varias líneas (la clase
;  c_edit con ES_MULTI de Hispa Ventanas), que ya hace toda la edición:
;  escribir, borrar con Retroceso y Supr, mover el cursor con las flechas,
;  Inicio, Fin, Re Pág y Av Pág, saltar de línea con Intro, seleccionar con el
;  ratón o con Mayús, y cortar, copiar y pegar. Las líneas largas se ven
;  corriendo el cursor (desplazamiento), y si el texto no cabe a lo alto sale
;  la barra de desplazamiento del propio cuadro. Abajo, una barra de estado
;  con el nombre del archivo y la posición del cursor (línea y columna).
;
;  Archivo: Nuevo, Abrir, Guardar y Guardar como, en texto llano .TXT (las
;  líneas se guardan acabadas en CR+LF, como espera DOS; dentro del cuadro el
;  texto lleva solo LF). Edición: cortar, copiar, pegar y seleccionar todo.
;  Pregunta antes de perder cambios. Hasta 60.000 caracteres (el tope del
;  cuadro de texto: el ED_MAX_MULTI de los cuadros de varias líneas).
;
;  Es un módulo .HVP (ventanas/hvp.inc). Reaprovecha el cuadro de texto de
;  varias líneas (el mismo del Bloc de notas): Pluma crea el cuadro, le manda
;  los textos con WM_SETTEXT y le reenvía las órdenes de edición con
;  WM_CUT/WM_COPY/WM_PASTE/EM_SEL_TODO; para la barra de estado lee del cuadro
;  el búfer (V_D0), su largo (V_D1) y el cursor (V_D3).
;
;  Parámetros del acceso directo: el nombre de un .TXT, que se abre al entrar.
; =============================================================================
%include "hvp.inc"

ESTADO      equ 22                  ; alto de la barra de estado
EDIT_ID     equ 100                 ; id del cuadro de texto (hija)
; tope del cuadro de texto de varias líneas (el ED_MAX_MULTI de controles.inc,
; que no se incluye en un .HVP); de aquí sale el tamaño de los búferes de archivo
PLU_MAX     equ 60000
; órdenes del menú
O_NUEVO     equ 101
O_ABRIR     equ 102
O_GUARDAR   equ 103
O_COMO      equ 104
O_SALIR     equ 105
O_CORTAR    equ 111
O_COPIAR    equ 112
O_PEGAR     equ 113
O_TODO      equ 114
O_ACERCA    equ 121

HVP_CABECERA "Pluma", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db ".....................0222220...."
        db "....................0222220....."
        db "...................0222220......"
        db "..................0222220......."
        db ".................fffffff........"
        db "................0ceccc0........."
        db "...............0ceccc0.........."
        db "..............0ceccc0..........."
        db ".............0ceccc0............"
        db "............0ceccc0............."
        db "...........0ceccc0.............."
        db "..........0ceccc0..............."
        db ".........0ceccc0................"
        db "........0ceccc0................."
        db ".......0ceccc0.................."
        db "......0ceccc0..................."
        db ".....0ceccc0...................."
        db "....0ceccc0....................."
        db "...0ceccc0......................"
        db "..0ceccc0......................."
        db ".0ceccc0........................"
        db "0ff0ff0........................."
        db "0f0f0..........................."
        db "f0f0............................"
        db "000............................."
        db "00.............................."
        db "00.............................."
        db "...ce..........................."
        db "...c............................"
HVP_CODIGO

; ---- clase de la ventana y plantillas ----
c_pluma     dd plu_proc
            db C_CREMA, 0, 0, 0
plu_plant:
        PLANTILLA c_pluma, titulo, E_NORMAL, 120, 70, 540, 380, 0
; el cuadro de texto de varias líneas (ocupa la zona menos la barra de estado).
; La clase va como número de la interfaz (HV_c_edit); hvl_clases la resuelve
edit_plant:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA | ES_MULTI, 0, 0, 100, 100, EDIT_ID
        dd 0                        ; (fin para hvl_clases)

; ---- la barra de menús ----
plu_menu:
        db "&Archivo", 0
        dw O_NUEVO
        db "&Nuevo", 0
        dw O_ABRIR
        db "&Abrir...", 0
        dw O_GUARDAR
        db "&Guardar          Ctrl+G", 0
        dw O_COMO
        db "Guardar &como...", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_CORTAR
        db "Cor&tar            Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar            Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar             Ctrl+V", 0
        dw 0
        db 0
        dw O_TODO
        db "&Seleccionar todo  Ctrl+A", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_ACERCA
        db "&Acerca de Pluma...", 0
        dw 0xFFFF
        db 0

; ---- textos (todo en español) ----
t_pluma     db "Pluma", 0
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_sep       db "   ", 0
t_linea     db "Línea ", 0
t_columna   db ", Columna ", 0
t_ext_txt   db ".TXT", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_cambios   db "El texto tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_guardar db "No se ha podido guardar el texto.", 0
t_acerca_tit db "Acerca de Pluma", 0
t_acerca    db "Pluma", 10
            db "Un procesador de textos sencillo para Hispa Ventanas.", 10
            db "Lee y guarda archivos de texto .TXT.", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        mov esi, edit_plant         ; resolver la clase del cuadro de texto
        call hvl_clases
        call pon_titulo
        mov esi, plu_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        ; ¿un archivo en los parámetros?
        mov esi, [param]
.e:
        cmp byte [esi], ' '
        jne .e1
        inc esi
        jmp .e
.e1:
        cmp byte [esi], 0
        je .b
        cmp byte [esi], '/'
        je .b
        mov edi, nom_tmp
        mov ecx, 79
.e2:
        lodsb
        cmp al, ' '
        jbe .e3
        stosb
        dec ecx
        jnz .e2
.e3:
        mov byte [edi], 0
        mov esi, nom_tmp
        call cargar
.b:
        mov ebp, [ven]              ; el reloj de la barra de estado
        mov ecx, 1
        mov edx, 150
        HV tmp_poner
        HV app_bucle
.x:
        ret

; (interna) el título: «Pluma - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_pluma
        call hvl_cad
        mov esi, t_guion
        call hvl_cad
        mov esi, nombre
        cmp byte [esi], 0
        jne .n
        mov esi, t_sin_nombre
.n:
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, titulo
        HV ven_texto
.x:
        popad
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
plu_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_HV_ABRIR        ; un .TXT que le mandan estando abierto
        je .hvabrir
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_GETMINMAXINFO   ; lo más pequeño: el menú y unas líneas
        jne .nmin
        mov eax, 240 + 140 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], plu_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, edit_plant
        HV ven_crear                ; EBP=la ventana (madre del cuadro)
        mov [edit], eax
        ; y colocar el cuadro de texto
.tam:
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        sub edx, ESTADO             ; el cuadro, menos la barra de estado
        jns .t1
        xor edx, edx
.t1:
        xor eax, eax
        xor ebx, ebx
        mov ebp, [edit]
        test ebp, ebp
        jz .t2
        HV ven_mover
.t2:
        call estado_inval
        jmp hecho
.foco:
        mov ebp, [edit]
        test ebp, ebp
        jz hecho
        HV ven_enfocar
        jmp hecho
.reloj:
        call estado_reloj           ; ¿se movió el cursor? repintar la posición
        jmp hecho
.cerrar:
        call preguntar
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
        jmp hecho
.hvabrir:
        push edx
        call preguntar
        pop esi
        jc hecho
.ha:
        lodsb                       ; el primer nombre de los parámetros
        cmp al, ' '
        je .ha
        jb hecho
        cmp al, '/'
        je hecho
        dec esi
        mov edi, nom_tmp
        mov ecx, 79
.hc:
        lodsb
        cmp al, ' '
        jbe .hf
        stosb
        dec ecx
        jnz .hc
.hf:
        mov byte [edi], 0
        mov esi, nom_tmp
        call cargar
        jmp hecho

hecho:
        xor eax, eax
        ret

; ---- las órdenes del menú ----
orden:
        cmp ecx, O_CORTAR
        je .cortar
        cmp ecx, O_COPIAR
        je .copiar
        cmp ecx, O_PEGAR
        je .pegar
        cmp ecx, O_TODO
        je .todo
        cmp ecx, O_NUEVO
        je .nuevo
        cmp ecx, O_ABRIR
        je .abrir
        cmp ecx, O_GUARDAR
        je .guardar
        cmp ecx, O_COMO
        je .como
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_ACERCA
        jne hecho
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp hecho
; cortar, copiar, pegar y seleccionar todo: al cuadro de texto
.cortar:
        mov eax, WM_CUT
        jmp .aledit
.copiar:
        mov eax, WM_COPY
        jmp .aledit
.pegar:
        mov eax, WM_PASTE
        jmp .aledit
.todo:
        mov eax, EM_SEL_TODO
.aledit:
        mov ebp, [edit]
        test ebp, ebp
        jz hecho
        xor ecx, ecx
        xor edx, edx
        HV msg_enviar
        jmp hecho
.nuevo:
        call preguntar
        jc hecho
        mov byte [nombre], 0
        mov ebp, [edit]
        mov eax, WM_SETTEXT
        mov edx, nombre             ; (apunta a un 0: texto vacío)
        HV msg_enviar
        mov ebp, [edit]
        mov eax, EM_PONER_MODIF
        xor ecx, ecx
        HV msg_enviar
        call pon_titulo
        call estado_inval
        jmp hecho
.abrir:
        call preguntar
        jc hecho
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz hecho
        mov esi, nom_tmp
        call cargar
        jmp hecho
.guardar:
        call guardar
        jmp hecho
.como:
        call guardar_como
        jmp hecho
.salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp hecho

; =============================================================================
;  Pintar: solo la barra de estado (el cuadro de texto se pinta solo)
; =============================================================================
pintar:
        mov ebp, [ven]
        HV dc_pintar                ; borra el fondo de lo sucio con C_CREMA
        call estado_txt             ; el texto de la barra, al día
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        COLOR C_PIEDRA              ; el fondo de la barra de estado
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, ESTADO
        mov ecx, [cli_an]
        mov edx, ESTADO
        HV dc_rect
        COLOR C_ARENA_OSC           ; un filete arriba
        mov edx, 1
        HV dc_rect
        COLOR C_TINTA
        mov eax, 8
        add ebx, 4
        mov esi, est_txt
        HV dc_texto
        jmp hecho

; (interna) invalida la franja de la barra de estado
estado_inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, ESTADO
        mov ecx, [cli_an]
        mov edx, ESTADO
        HV ven_invalidar_rect
.x:
        popad
        ret

; (interna, en el reloj) si el cursor se ha movido, repinta la barra de estado
estado_reloj:
        pushad
        mov ebp, [edit]
        test ebp, ebp
        jz .x
        mov eax, [ebp+V_D3]
        cmp eax, [ult_cur]
        je .x
        mov [ult_cur], eax
        call estado_inval
.x:
        popad
        ret

; (interna) arma est_txt: «NOMBRE   Línea L, Columna C»
estado_txt:
        pushad
        mov edi, est_txt
        mov esi, nombre
        cmp byte [esi], 0
        jne .n
        mov esi, t_sin_nombre
.n:
        call hvl_cad
        mov ebp, [edit]
        test ebp, ebp
        jz .fin
        mov esi, t_sep
        call hvl_cad
        ; línea y columna del cursor (V_D3) recorriendo el búfer (V_D0)
        mov esi, [ebp+V_D0]
        mov ecx, [ebp+V_D3]         ; posición del cursor
        xor eax, eax                ; líneas antes (0-base)
        xor ebx, ebx                ; principio de la línea del cursor
        xor edx, edx
.l:
        cmp edx, ecx
        jae .lf
        cmp byte [esi+edx], 10
        jne .l1
        inc eax
        lea ebx, [edx+1]
.l1:
        inc edx
        jmp .l
.lf:
        push ecx
        push ebx
        mov esi, t_linea
        call hvl_cad
        inc eax                     ; línea, 1-base
        call hvl_num
        mov esi, t_columna
        call hvl_cad
        pop ebx
        pop ecx
        mov eax, ecx
        sub eax, ebx
        inc eax                     ; columna, 1-base
        call hvl_num
.fin:
        mov byte [edi], 0
        popad
        ret

; =============================================================================
;  Archivos
; =============================================================================
; (interna) carga el archivo ESI (0) en el cuadro de texto
cargar:
        pushad
        xor al, al
        HV dos_abrir                ; ESI=nombre -> EAX=mango, CF=1 si falla
        jc .falla
        mov ebx, eax
        mov ecx, PLU_MAX + 8192
        HV mem_pedir
        jc .falla1
        mov edi, eax                ; búfer
        mov ecx, PLU_MAX + 8191 ; (deja sitio para el 0 del final)
        HV dos_leer                 ; EBX=mango ECX=cuánto EDI=búfer -> EAX=leído
        mov ecx, eax
        HV dos_cerrar
        mov byte [edi+ecx], 0       ; acabar en 0
        push edi                    ; el búfer, para soltarlo
        mov ebp, [edit]
        mov eax, WM_SETTEXT         ; (WM_SETTEXT quita los CR)
        mov edx, edi
        HV msg_enviar
        mov ebp, [edit]
        mov eax, EM_PONER_MODIF
        xor ecx, ecx
        HV msg_enviar
        pop eax
        HV mem_soltar
        mov edi, nombre             ; recordar el nombre (siempre en nom_tmp)
        mov esi, nom_tmp
        call hvl_cad
        mov byte [edi], 0
        call pon_titulo
        call estado_inval
        popad
        ret
.falla1:
        HV dos_cerrar
.falla:
        mov ebp, [ven]
        mov esi, t_no_abrir
        mov edi, t_pluma
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; (interna) guarda con el nombre que tiene (o lo pide). CF=1 si no se guarda
guardar:
        cmp byte [nombre], 0
        jne guardar_escribir
guardar_como:
        pushad
        mov ebp, [ven]
        mov edi, nom_tmp            ; partir del nombre actual
        mov esi, nombre
        call hvl_cad
        mov byte [edi], 0
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .no
        mov esi, nom_tmp           ; sin extensión: .TXT
        mov edi, nombre
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
        call hvl_cad
.f2:
        mov byte [edi], 0
        popad
        jmp guardar_escribir
.no:
        popad
        stc
        ret

; (interna) escribe el texto del cuadro en [nombre], con CR+LF. CF=1 si falla
guardar_escribir:
        pushad
        mov ebp, [edit]
        mov esi, [ebp+V_D0]         ; el texto (con LF)
        mov ecx, [ebp+V_D1]         ; octetos
        push ecx
        lea ecx, [ecx*2+16]         ; peor caso: cada LF pasa a CR+LF
        HV mem_pedir
        pop ecx
        jc .falla0
        mov edi, eax
        mov edx, eax                ; el búfer de salida
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
        sub edi, edx                ; octetos a escribir
        mov esi, nombre
        mov ecx, edi
        call hvl_escribir           ; ESI=ruta EDX=datos ECX=octetos -> CF
        pushf
        mov eax, edx
        HV mem_soltar
        popf
        jc .falla
        mov ebp, [edit]             ; marcar como «sin cambios»
        mov eax, EM_PONER_MODIF
        xor ecx, ecx
        HV msg_enviar
        call pon_titulo
        call estado_inval
        popad
        clc
        ret
.falla:
.falla0:
        mov ebp, [ven]
        mov esi, t_no_guardar
        mov edi, t_pluma
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        stc
        ret

; (interna) si el texto ha cambiado, pregunta si se guarda.
; CF=1 si no hay que seguir (se canceló o no se pudo guardar)
preguntar:
        pushad
        mov ebp, [edit]
        test ebp, ebp
        jz .si
        mov eax, EM_MODIFICADO
        xor ecx, ecx
        xor edx, edx
        HV msg_enviar
        test eax, eax
        jz .si
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_pluma
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_NO
        je .si
        cmp eax, ID_SI
        jne .no
        call guardar
        jc .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

%include "hvplib.inc"

        section .bss
        alignb 4
ven         resd 1                  ; la ventana de Pluma
edit        resd 1                  ; el cuadro de texto (hija)
param       resd 1                  ; los parámetros del acceso directo
cli_an      resd 1                  ; la zona de la ventana
cli_al      resd 1
ult_cur     resd 1                  ; última posición del cursor (para el reloj)
nombre      resb 80                 ; el nombre del archivo abierto
nom_tmp     resb 80                 ; el que se teclea en los diálogos
titulo      resb 120
est_txt     resb 128

HVP_FIN
