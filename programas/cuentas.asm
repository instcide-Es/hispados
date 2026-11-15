; =============================================================================
;  CUENTAS.HVP - Cuentas, la hoja de cálculo sencilla de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Una hoja de cálculo sencilla: una rejilla de 26 columnas (A-Z) por 100 filas
;  (1-100), con encabezados de columna y de fila. La celda activa se resalta y
;  su referencia (p. ej. «B7») y su contenido EN CRUDO salen en la barra de
;  arriba. Se anda por la rejilla con las flechas, Inicio/Fin (primera/última
;  columna) y Re Pág/Av Pág (una pantalla arriba/abajo); la vista se desplaza
;  sola cuando el cursor se sale. Al teclear (o con F2) se edita la celda;
;  Intro confirma y baja, Tab confirma y va a la derecha, Esc cancela.
;
;  Tipos de celda:
;    - texto:   lo que no es número ni empieza por «=»; se ve a la izquierda.
;    - número:  se ve a la derecha, tal y como se tecleó (coma o punto decimal).
;    - fórmula: empieza por «=»; evalúa una expresión con + - * / y paréntesis,
;               referencias a celdas (A1, B7...) y la función de rango
;               =SUMA(A1:A10). Los números son de coma fija (64 bits, cuatro
;               decimales). Errores: «#ERROR» (sintaxis, división por cero,
;               desbordamiento), «#CIRC» (referencia circular) y «#REF»
;               (referencia fuera de la hoja). El recálculo es a la vista: cada
;               celda se evalúa al pintarse, resolviendo las que necesita.
;
;  Archivo: formato CSV de texto (una fila por línea, celdas separadas por «;»,
;  fin de línea CR+LF como en DOS), guardando el contenido EN CRUDO (las
;  fórmulas con su «=...»). Extensión .CSV. Archivo: Nuevo, Abrir, Guardar y
;  Guardar como. Edición: Borrar celda, Cortar, Copiar y Pegar (una celda).
;  Pregunta antes de perder cambios.
;
;  Es un módulo .HVP (ventanas/hvp.inc). Dibuja la rejilla él mismo (no usa
;  controles hijos), como hace Pintura con su lienzo.
;
;  Parámetros del acceso directo: el nombre de un .CSV, que se abre al entrar.
; =============================================================================
%include "hvp.inc"

; ---- medidas de la rejilla (en celdas) ----
NCOL        equ 26                  ; columnas A-Z
NFIL        equ 100                 ; filas 1-100
CEL_MAX     equ 32                  ; octetos por celda (texto crudo, con el 0)
NCEL        equ NCOL * NFIL
; ---- coma fija de las fórmulas ----
CU_ESC      equ 10000               ; cuatro decimales
CU_DEC      equ 4
DEPTH_MAX   equ 48                  ; tope de anidamiento al evaluar
; ---- medidas en puntos ----
BARRA       equ 22                  ; alto de la barra de referencia (arriba)
ROT_AN      equ 36                  ; ancho de la columna de rótulos de fila
ROT_AL      equ 18                  ; alto de la fila de rótulos de columna
CEL_AN      equ 72                  ; ancho de una celda
CEL_AL      equ 18                  ; alto de una celda
CEL_CARS    equ 8                   ; caracteres que caben en una celda
GY0         equ BARRA + ROT_AL      ; y de la primera fila de celdas
; ---- órdenes del menú ----
O_NUEVO     equ 101
O_ABRIR     equ 102
O_GUARDAR   equ 103
O_COMO      equ 104
O_SALIR     equ 105
O_BORRAR    equ 111
O_CORTAR    equ 112
O_COPIAR    equ 113
O_PEGAR     equ 114
O_ACERCA    equ 121

HVP_CABECERA "Cuentas", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db "..0000000000000000000000000000.."
        db "..0eeeeeeeeeeeeeeeeeeeeeeeeee0.."
        db "..0eeeeeeeeeeeeeeeeeeeeeeeeee0.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0000000000000000000000000000.."
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- clase de la ventana y plantilla ----
c_cuentas   dd cue_proc
            db C_CREMA, 0, 0, 0
cue_plant:
        PLANTILLA c_cuentas, titulo, E_NORMAL, 80, 50, 560, 400, 0

; ---- la barra de menús ----
cue_menu:
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
        dw O_BORRAR
        db "&Borrar celda      Supr", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar            Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar            Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar             Ctrl+V", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_ACERCA
        db "&Acerca de Cuentas...", 0
        dw 0xFFFF
        db 0

; ---- textos (todo en español) ----
t_cuentas   db "Cuentas", 0
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_ext_csv   db ".CSV", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_suma      db "SUMA", 0
t_err_err   db "#ERROR", 0
t_err_circ  db "#CIRC", 0
t_err_ref   db "#REF", 0
t_cambios   db "La hoja tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_guardar db "No se ha podido guardar la hoja.", 0
t_acerca_tit db "Acerca de Cuentas", 0
t_acerca    db "Cuentas", 10
            db "Una hoja de cálculo sencilla para Hispa Ventanas.", 10
            db "Fórmulas con = (+ - * / , paréntesis, referencias y SUMA).", 10
            db "Lee y guarda archivos .CSV.", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        call pon_titulo
        mov esi, cue_plant
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
        HV app_bucle
.x:
        ret

; (interna) el título: «Cuentas - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_cuentas
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
cue_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_HV_ABRIR
        je .hvabrir
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_LBUTTONDOWN
        je raton
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_GETMINMAXINFO   ; lo más pequeño
        jne .nmin
        mov eax, 300 + 200 * 65536
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
        mov dword [ebp+V_MENU], cue_menu
        mov dword [ebp+V_ICONO], hvp_icono
        jmp hecho
.tam:
        mov ebp, [ven]
        HV ven_invalidar
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
        lodsb
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

; =============================================================================
;  Teclado: WM_KEYDOWN (ECX=código, EDX=estado de mayúsculas)
; =============================================================================
tecla:
        cmp byte [editando], 0
        jne .edicion
        ; --- navegación ---
        cmp ecx, 0x103              ; Ctrl+C
        je .cc
        cmp ecx, 0x118              ; Ctrl+X
        je .cx
        cmp ecx, 0x116              ; Ctrl+V
        je .cv
        cmp ecx, 0x107              ; Ctrl+G
        je .cg
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_DER
        je .der
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_INTRO
        je .aba
        cmp ecx, TC_TAB
        je .der
        cmp ecx, TC_INICIO
        je .inicio
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_AVPAG
        je .avpag
        cmp ecx, TC_SUPR
        je .supr
        cmp ecx, TC_F1 + 1          ; F2
        je .f2
        jmp hecho
.izq:
        mov eax, -1
        xor ebx, ebx
        call mueve_rel
        jmp hecho
.der:
        mov eax, 1
        xor ebx, ebx
        call mueve_rel
        jmp hecho
.arr:
        xor eax, eax
        mov ebx, -1
        call mueve_rel
        jmp hecho
.aba:
        xor eax, eax
        mov ebx, 1
        call mueve_rel
        jmp hecho
.inicio:
        xor eax, eax
        mov [cur_col], eax
        call invalida
        jmp hecho
.fin:
        mov dword [cur_col], NCOL - 1
        call invalida
        jmp hecho
.repag:
        xor eax, eax
        mov ebx, [vis_fil]
        neg ebx
        call mueve_rel
        jmp hecho
.avpag:
        xor eax, eax
        mov ebx, [vis_fil]
        call mueve_rel
        jmp hecho
.supr:
        call borra_celda
        jmp hecho
.f2:
        call empieza_edit_f2
        jmp hecho
.cc:
        call copia_celda
        jmp hecho
.cx:
        call corta_celda
        jmp hecho
.cv:
        call pega_celda
        jmp hecho
.cg:
        call guardar
        jmp hecho
; --- edición en curso ---
.edicion:
        cmp ecx, TC_INTRO
        je .e_intro
        cmp ecx, TC_TAB
        je .e_tab
        cmp ecx, TC_ESC
        je .e_esc
        cmp ecx, TC_RETRO
        je .e_retro
        cmp ecx, TC_IZQ
        je .e_izq
        cmp ecx, TC_DER
        je .e_der
        cmp ecx, TC_ARRIBA
        je .e_arr
        cmp ecx, TC_ABAJO
        je .e_aba
        jmp hecho
.e_intro:
        call commit_edit
        xor eax, eax
        mov ebx, 1
        call mueve_rel
        jmp hecho
.e_tab:
        call commit_edit
        mov eax, 1
        xor ebx, ebx
        call mueve_rel
        jmp hecho
.e_esc:
        mov byte [editando], 0
        call invalida
        jmp hecho
.e_retro:
        call borra_char
        jmp hecho
.e_izq:
        call commit_edit
        mov eax, -1
        xor ebx, ebx
        call mueve_rel
        jmp hecho
.e_der:
        call commit_edit
        mov eax, 1
        xor ebx, ebx
        call mueve_rel
        jmp hecho
.e_arr:
        call commit_edit
        xor eax, eax
        mov ebx, -1
        call mueve_rel
        jmp hecho
.e_aba:
        call commit_edit
        xor eax, eax
        mov ebx, 1
        call mueve_rel
        jmp hecho

; WM_CHAR (ECX=carácter)
letra:
        mov al, cl
        cmp al, 32
        jb hecho
        cmp byte [editando], 0
        jne .ap
        call empieza_edit_char      ; AL = carácter
        jmp hecho
.ap:
        call append_char            ; AL
        jmp hecho

; WM_LBUTTONDOWN (EDX = x + y*65536)
raton:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp eax, ROT_AN
        jl hecho
        cmp ebx, GY0
        jl hecho
        sub eax, ROT_AN
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        add eax, [desp_col]
        cmp eax, NCOL
        jae hecho
        push eax
        mov eax, ebx
        sub eax, GY0
        xor edx, edx
        mov ecx, CEL_AL
        div ecx
        add eax, [desp_fil]
        cmp eax, NFIL
        jae .pop
        cmp byte [editando], 0
        je .set
        push eax
        call commit_edit
        pop eax
.set:
        mov [cur_fil], eax
        pop eax
        mov [cur_col], eax
        call invalida
        jmp hecho
.pop:
        add esp, 4
        jmp hecho

; =============================================================================
;  Las órdenes del menú: ECX=número
; =============================================================================
orden:
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
        cmp ecx, O_BORRAR
        je .borrar
        cmp ecx, O_CORTAR
        je .cortar
        cmp ecx, O_COPIAR
        je .copiar
        cmp ecx, O_PEGAR
        je .pegar
        cmp ecx, O_ACERCA
        jne hecho
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp hecho
.nuevo:
        call preguntar
        jc hecho
        call limpia_todo
        mov byte [nombre], 0
        xor eax, eax
        mov [cur_col], eax
        mov [cur_fil], eax
        mov [desp_col], eax
        mov [desp_fil], eax
        mov byte [editando], 0
        mov byte [modif], 0
        call pon_titulo
        call invalida
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
        call guarda_como
        jmp hecho
.borrar:
        call borra_celda
        jmp hecho
.cortar:
        call corta_celda
        jmp hecho
.copiar:
        call copia_celda
        jmp hecho
.pegar:
        call pega_celda
        jmp hecho
.salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp hecho

; =============================================================================
;  Movimiento del cursor y edición
; =============================================================================
; EAX = incremento de columna, EBX = incremento de fila (con signo)
mueve_rel:
        add eax, [cur_col]
        test eax, eax
        jns .a1
        xor eax, eax
.a1:
        cmp eax, NCOL
        jl .a2
        mov eax, NCOL - 1
.a2:
        mov [cur_col], eax
        add ebx, [cur_fil]
        test ebx, ebx
        jns .b1
        xor ebx, ebx
.b1:
        cmp ebx, NFIL
        jl .b2
        mov ebx, NFIL - 1
.b2:
        mov [cur_fil], ebx
        call invalida
        ret

invalida:
        push ebp
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        pop ebp
        ret

; confirma la edición: vuelca edt_buf a la celda activa
commit_edit:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero            ; ESI = destino
        mov edi, esi
        mov esi, edt_buf
        mov ecx, CEL_MAX - 1
.l:
        mov al, [esi]
        test al, al
        jz .x
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .l
.x:
        mov byte [edi], 0
        mov byte [editando], 0
        mov byte [modif], 1
        popad
        ret

; F2: edita el contenido actual
empieza_edit_f2:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero            ; ESI = crudo
        mov edi, edt_buf
        mov ecx, CEL_MAX - 1
.l:
        mov al, [esi]
        test al, al
        jz .x
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .l
.x:
        mov byte [edi], 0
        mov byte [editando], 1
        popad
        call invalida
        ret

; empieza a editar con un carácter nuevo (AL)
empieza_edit_char:
        mov edi, edt_buf
        mov [edi], al
        mov byte [edi+1], 0
        mov byte [editando], 1
        call invalida
        ret

; añade el carácter AL al final de edt_buf
append_char:
        push edi
        push ecx
        mov edi, edt_buf
        mov ecx, CEL_MAX - 1
.l:
        cmp byte [edi], 0
        je .f
        inc edi
        dec ecx
        jnz .l
        pop ecx                     ; lleno: no cabe
        pop edi
        ret
.f:
        mov [edi], al
        mov byte [edi+1], 0
        pop ecx
        pop edi
        call invalida
        ret

; borra el último carácter de edt_buf
borra_char:
        push esi
        mov esi, edt_buf
.l:
        cmp byte [esi], 0
        je .f
        inc esi
        jmp .l
.f:
        cmp esi, edt_buf
        je .x
        dec esi
        mov byte [esi], 0
.x:
        pop esi
        call invalida
        ret

; borra el contenido de la celda activa
borra_celda:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
        mov byte [esi], 0
        mov byte [modif], 1
        popad
        call invalida
        ret

; copia la celda activa al portapapeles de una celda
copia_celda:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
        mov edi, clip_buf
        call copia_cad
        popad
        ret

corta_celda:
        call copia_celda
        call borra_celda
        ret

pega_celda:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
        mov edi, esi
        mov esi, clip_buf
        call copia_cad
        mov byte [modif], 1
        popad
        call invalida
        ret

; copia ESI (0) a EDI, como mucho CEL_MAX-1 caracteres, acabado en 0
copia_cad:
        push eax
        push ecx
        mov ecx, CEL_MAX - 1
.l:
        mov al, [esi]
        test al, al
        jz .x
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .l
.x:
        mov byte [edi], 0
        pop ecx
        pop eax
        ret

; =============================================================================
;  Pintar
; =============================================================================
pintar:
        mov ebp, [ven]
        HV dc_pintar                ; borra el fondo con C_CREMA
        call calc_medidas
        call pinta_barra
        call pinta_rejilla
        jmp hecho

; medidas de la zona y de lo que se ve; ajusta el desplazamiento
calc_medidas:
        pushad
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto
        mov [cli_an], ecx
        mov [cli_al], edx
        ; columnas visibles = (ancho - ROT_AN) / CEL_AN
        sub ecx, ROT_AN
        jns .c1
        xor ecx, ecx
.c1:
        mov eax, ecx
        xor edx, edx
        mov ecx, CEL_AN
        div ecx
        mov [vis_col], eax
        ; filas visibles = (alto - GY0) / CEL_AL
        mov eax, [cli_al]
        sub eax, GY0
        jns .f1
        xor eax, eax
.f1:
        xor edx, edx
        mov ecx, CEL_AL
        div ecx
        mov [vis_fil], eax
        call ajusta_vista
        popad
        ret

; mueve el desplazamiento para que la celda activa se vea
ajusta_vista:
        ; columnas
        mov eax, [cur_col]
        cmp eax, [desp_col]
        jge .c1
        mov [desp_col], eax
.c1:
        mov eax, [desp_col]
        add eax, [vis_col]
        cmp [cur_col], eax
        jl .f0
        mov eax, [cur_col]
        sub eax, [vis_col]
        inc eax
        js .c2
        mov [desp_col], eax
        jmp .f0
.c2:
        mov dword [desp_col], 0
.f0:
        ; filas
        mov eax, [cur_fil]
        cmp eax, [desp_fil]
        jge .f1
        mov [desp_fil], eax
.f1:
        mov eax, [desp_fil]
        add eax, [vis_fil]
        cmp [cur_fil], eax
        jl .x
        mov eax, [cur_fil]
        sub eax, [vis_fil]
        inc eax
        js .f2
        mov [desp_fil], eax
        ret
.f2:
        mov dword [desp_fil], 0
.x:
        ret

; la barra de arriba: referencia + contenido crudo (o lo que se edita)
pinta_barra:
        pushad
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, BARRA
        HV dc_rect
        COLOR C_ARENA_OSC           ; filete abajo
        xor eax, eax
        mov ebx, BARRA - 1
        mov ecx, [cli_an]
        mov edx, 1
        HV dc_rect
        mov eax, 54                 ; separador
        xor ebx, ebx
        mov ecx, 1
        mov edx, BARRA
        HV dc_rect
        ; referencia «B7»
        call construye_ref
        COLOR C_TINTA
        mov eax, 6
        mov ebx, 3
        mov esi, ref_txt
        HV dc_texto
        ; contenido
        cmp byte [editando], 0
        je .crudo
        mov esi, edt_buf
        jmp .copia
.crudo:
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
.copia:
        mov edi, bar_txt
        mov ecx, 71
.cp:
        mov al, [esi]
        test al, al
        jz .cpx
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .cp
.cpx:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 62
        mov ebx, 3
        mov esi, bar_txt
        HV dc_texto
        popad
        ret

; la rejilla: rótulos, celdas, líneas y la celda activa
pinta_rejilla:
        pushad
        ; fondo blanco de la zona de celdas
        mov eax, ROT_AN
        mov ebx, GY0
        mov ecx, [vis_col]
        imul ecx, CEL_AN
        mov edx, [vis_fil]
        imul edx, CEL_AL
        COLOR C_BLANCO
        HV dc_rect
        ; la esquina
        COLOR C_PIEDRA
        xor eax, eax
        mov ebx, BARRA
        mov ecx, ROT_AN
        mov edx, ROT_AL
        HV dc_rect
        ; rótulos de columna
        xor esi, esi
.col:
        cmp esi, [vis_col]
        jae .colf
        mov eax, esi
        add eax, [desp_col]
        cmp eax, NCOL
        jae .colf
        mov ebx, esi
        imul ebx, CEL_AN
        add ebx, ROT_AN             ; x de la columna
        COLOR C_PIEDRA
        mov eax, ebx
        push ebx
        mov ebx, BARRA
        mov ecx, CEL_AN
        mov edx, ROT_AL
        HV dc_rect
        pop ebx
        mov eax, esi
        add eax, [desp_col]
        call letra_col              ; col_txt = letra
        mov eax, ebx
        add eax, CEL_AN / 2 - 4
        mov ebx, BARRA + 1
        COLOR C_TINTA
        mov [esi2tmp], esi
        mov esi, col_txt
        HV dc_texto
        mov esi, [esi2tmp]
        inc esi
        jmp .col
.colf:
        ; rótulos de fila
        xor esi, esi
.fil:
        cmp esi, [vis_fil]
        jae .filf
        mov eax, esi
        add eax, [desp_fil]
        cmp eax, NFIL
        jae .filf
        mov ebx, esi
        imul ebx, CEL_AL
        add ebx, GY0                ; y de la fila
        COLOR C_PIEDRA
        xor eax, eax
        push ebx
        mov ecx, ROT_AN
        mov edx, CEL_AL
        HV dc_rect
        pop ebx
        mov eax, esi
        add eax, [desp_fil]
        inc eax
        mov edi, fil_txt
        call hvl_num
        mov byte [edi], 0
        mov eax, 5
        push ebx
        add ebx, 1
        COLOR C_TINTA
        mov [fil_i], esi
        mov esi, fil_txt
        HV dc_texto
        mov esi, [fil_i]
        pop ebx
        inc esi
        jmp .fil
.filf:
        call pinta_lineas
        call pinta_contenidos
        call pinta_activa
        popad
        ret

; EAX = columna (0..25) -> col_txt = letra, 0
letra_col:
        add al, 'A'
        mov [col_txt], al
        mov byte [col_txt+1], 0
        ret

; líneas finas de la rejilla
pinta_lineas:
        pushad
        mov edi, [vis_col]           ; derecha
        imul edi, CEL_AN
        add edi, ROT_AN              ; x derecha de la rejilla
        mov ebp, [vis_fil]
        imul ebp, CEL_AL
        add ebp, GY0                 ; y inferior
        ; verticales
        xor esi, esi
.v:
        cmp esi, [vis_col]
        ja .vf
        mov eax, esi
        imul eax, CEL_AN
        add eax, ROT_AN
        mov ecx, eax
        mov ebx, GY0
        mov edx, ebp
        COLOR C_ARENA_OSC
        HV dc_linea
        inc esi
        jmp .v
.vf:
        ; horizontales
        xor esi, esi
.h:
        cmp esi, [vis_fil]
        ja .hf
        mov ebx, esi
        imul ebx, CEL_AL
        add ebx, GY0
        mov edx, ebx
        mov eax, ROT_AN
        mov ecx, edi
        COLOR C_ARENA_OSC
        HV dc_linea
        inc esi
        jmp .h
.hf:
        popad
        ret

; el contenido de cada celda visible
pinta_contenidos:
        pushad
        mov dword [p_fi], 0
.rf:
        mov eax, [p_fi]
        cmp eax, [vis_fil]
        jae .x
        add eax, [desp_fil]
        cmp eax, NFIL
        jae .x
        mov dword [p_ci], 0
.cf:
        mov eax, [p_ci]
        cmp eax, [vis_col]
        jae .nextr
        add eax, [desp_col]
        cmp eax, NCOL
        jae .nextr
        ; celda (p_ci+desp_col, p_fi+desp_fil)
        mov ecx, [p_ci]
        add ecx, [desp_col]
        mov edx, [p_fi]
        add edx, [desp_fil]
        call dibuja_celda
.nextc:
        inc dword [p_ci]
        jmp .cf
.nextr:
        inc dword [p_fi]
        jmp .rf
.x:
        popad
        ret

; dibuja el contenido de la celda (col=ECX, fil=EDX)
dibuja_celda:
        pushad
        mov [d_col], ecx
        mov [d_fil], edx
        call celda_mostrar          ; rellena disp y disp_der
        ; truncar a CEL_CARS en cel7
        mov esi, disp
        mov edi, cel7
        mov ecx, CEL_CARS
.t:
        mov al, [esi]
        test al, al
        jz .t2
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .t
.t2:
        mov byte [edi], 0
        ; largo en caracteres
        mov esi, cel7
        xor ecx, ecx
.lg:
        cmp byte [esi+ecx], 0
        je .lgf
        inc ecx
        jmp .lg
.lgf:
        ; posición en pantalla
        mov eax, [d_col]
        sub eax, [desp_col]
        imul eax, CEL_AN
        add eax, ROT_AN             ; x izquierda de la celda
        mov ebx, [d_fil]
        sub ebx, [desp_fil]
        imul ebx, CEL_AL
        add ebx, GY0 + 1            ; y del texto
        cmp byte [disp_der], 0
        je .izq
        ; alinear a la derecha
        mov edx, ecx
        imul edx, 8
        add eax, CEL_AN - 3
        sub eax, edx
        jmp .pinta
.izq:
        add eax, 3
.pinta:
        COLOR C_TINTA
        mov esi, cel7
        HV dc_texto
        popad
        ret

; la celda activa: borde grueso, o la edición en curso
pinta_activa:
        pushad
        mov eax, [cur_col]
        sub eax, [desp_col]
        js .x
        cmp eax, [vis_col]
        jge .x
        mov ebx, [cur_fil]
        sub ebx, [desp_fil]
        js .x
        cmp ebx, [vis_fil]
        jge .x
        imul eax, CEL_AN
        add eax, ROT_AN             ; x
        imul ebx, CEL_AL
        add ebx, GY0                ; y
        mov [a_x], eax
        mov [a_y], ebx
        cmp byte [editando], 0
        je .borde
        ; fondo blanco y texto de edición con cursor
        COLOR C_BLANCO
        mov eax, [a_x]
        inc eax
        mov ebx, [a_y]
        inc ebx
        mov ecx, CEL_AN - 1
        mov edx, CEL_AL - 1
        HV dc_rect
        ; cel7 = edt_buf (como mucho CEL_CARS-1) + '_'
        mov esi, edt_buf
        mov edi, cel7
        mov ecx, CEL_CARS - 1
.e:
        mov al, [esi]
        test al, al
        jz .ef
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .e
.ef:
        mov byte [edi], '_'
        mov byte [edi+1], 0
        COLOR C_TINTA
        mov eax, [a_x]
        add eax, 2
        mov ebx, [a_y]
        inc ebx
        mov esi, cel7
        HV dc_texto
.borde:
        COLOR C_TINTA
        mov eax, [a_x]
        mov ebx, [a_y]
        mov ecx, CEL_AN
        mov edx, CEL_AL
        HV dc_marco
        mov eax, [a_x]
        inc eax
        mov ebx, [a_y]
        inc ebx
        mov ecx, CEL_AN - 2
        mov edx, CEL_AL - 2
        HV dc_marco
.x:
        popad
        ret

; referencia de la celda activa -> ref_txt («B7»)
construye_ref:
        push eax
        push edi
        mov eax, [cur_col]
        add al, 'A'
        mov [ref_txt], al
        mov edi, ref_txt + 1
        mov eax, [cur_fil]
        inc eax
        call hvl_num
        mov byte [edi], 0
        pop edi
        pop eax
        ret

; =============================================================================
;  Punteros a la celda (col=ECX, fil=EDX)
; =============================================================================
; -> ESI = puntero al texto crudo. Conserva los demás registros.
cel_puntero:
        push eax
        push edx
        mov eax, edx
        imul eax, NCOL
        add eax, ecx
        imul eax, CEL_MAX
        add eax, cel_txt
        mov esi, eax
        pop edx
        pop eax
        ret

; -> EDI = puntero al octeto de estado. Conserva los demás registros.
cel_estado_ptr:
        push eax
        push edx
        mov eax, edx
        imul eax, NCOL
        add eax, ecx
        add eax, cel_estado
        mov edi, eax
        pop edx
        pop eax
        ret

; =============================================================================
;  Cómo se muestra una celda (col=ECX, fil=EDX) -> disp[], disp_der
; =============================================================================
celda_mostrar:
        pushad
        mov [cm_col], ecx
        mov [cm_fil], edx
        mov byte [disp_der], 0
        mov byte [disp], 0
        call cel_puntero            ; ESI = crudo
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, '='
        je .formula
        ; literal: ¿número o texto?
        call cad_a_num              ; ESI -> EDX:EAX, CF=1 si no es número
        jc .texto
        mov byte [disp_der], 1
.texto:
        mov ecx, [cm_col]
        mov edx, [cm_fil]
        call cel_puntero
        mov edi, disp
        call copia_disp
        jmp .fin
.formula:
        mov byte [ev_err], 0
        mov dword [ev_depth], 0
        mov ecx, [cm_col]
        mov edx, [cm_fil]
        call celda_valor            ; ECX=col EDX=fil
        cmp byte [ev_err], 0
        jne .ferr
        mov edi, disp
        call num_a_cad
        mov byte [edi], 0
        mov byte [disp_der], 1
        jmp .fin
.ferr:
        call err_texto              ; ESI = texto del error
        mov edi, disp
        call copia_disp
.fin:
        popad
        ret

; copia ESI (0) a EDI, como mucho CEL_MAX-1, con 0 al final
copia_disp:
        push eax
        push ecx
        mov ecx, CEL_MAX - 1
.l:
        mov al, [esi]
        test al, al
        jz .x
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .l
.x:
        mov byte [edi], 0
        pop ecx
        pop eax
        ret

; ESI = texto del error según ev_err
err_texto:
        cmp byte [ev_err], 2
        je .circ
        cmp byte [ev_err], 3
        je .ref
        mov esi, t_err_err
        ret
.circ:
        mov esi, t_err_circ
        ret
.ref:
        mov esi, t_err_ref
        ret

; =============================================================================
;  Valor de una celda (col=ECX, fil=EDX) -> EDX:EAX coma fija; ev_err
; =============================================================================
celda_valor:
        test ecx, ecx
        js .cero
        cmp ecx, NCOL
        jae .referr
        test edx, edx
        js .cero
        cmp edx, NFIL
        jae .referr
        call cel_estado_ptr         ; EDI = estado
        cmp byte [edi], 0
        jne .circ
        call cel_puntero            ; ESI = texto
        mov al, [esi]
        test al, al
        jz .cero
        cmp al, '='
        je .formula
        call cad_a_num              ; literal
        jnc .ret
        jmp .cero
.formula:
        inc dword [ev_depth]
        cmp dword [ev_depth], DEPTH_MAX
        ja .deep
        mov byte [edi], 1           ; marca «visitando»
        push edi
        push dword [ev_ptr]
        lea eax, [esi+1]
        mov [ev_ptr], eax
        call eval_expr
        call salta_esp
        mov esi, [ev_ptr]
        cmp byte [esi], 0
        je .ftrail
        cmp byte [ev_err], 0
        jne .ftrail
        mov byte [ev_err], 1        ; sobra texto tras la fórmula
.ftrail:
        pop dword [ev_ptr]
        pop edi
        mov byte [edi], 0
        dec dword [ev_depth]
        ret
.deep:
        dec dword [ev_depth]
        mov byte [ev_err], 1
        jmp .cero
.circ:
        mov byte [ev_err], 2
        jmp .cero
.referr:
        mov byte [ev_err], 3
.cero:
        xor eax, eax
        xor edx, edx
.ret:
        ret

; =============================================================================
;  Evaluador de expresiones. Puntero en [ev_ptr], error en [ev_err],
;  valor en EDX:EAX. Las rutinas pueden tocar todos los registros.
; =============================================================================
salta_esp:
        push esi
        mov esi, [ev_ptr]
.l:
        cmp byte [esi], ' '
        jne .x
        inc esi
        jmp .l
.x:
        mov [ev_ptr], esi
        pop esi
        ret

eval_expr:
        call eval_term
        cmp byte [ev_err], 0
        jne .x
.l:
        call salta_esp
        mov esi, [ev_ptr]
        mov al, [esi]
        cmp al, '+'
        je .mas
        cmp al, '-'
        je .menos
        ret
.mas:
        inc dword [ev_ptr]
        push edx
        push eax
        call eval_term
        cmp byte [ev_err], 0
        jne .perr
        pop ebx
        pop ecx
        add eax, ebx
        adc edx, ecx
        jmp .l
.menos:
        inc dword [ev_ptr]
        push edx
        push eax
        call eval_term
        cmp byte [ev_err], 0
        jne .perr
        pop ebx                     ; valor izquierdo (bajo)
        pop ecx                     ; valor izquierdo (alto)
        sub ebx, eax
        sbb ecx, edx
        mov eax, ebx
        mov edx, ecx
        jmp .l
.perr:
        add esp, 8
.x:
        ret

eval_term:
        call eval_factor
        cmp byte [ev_err], 0
        jne .x
.l:
        call salta_esp
        mov esi, [ev_ptr]
        mov al, [esi]
        cmp al, '*'
        je .por
        cmp al, '/'
        je .entre
        ret
.por:
        inc dword [ev_ptr]
        push edx
        push eax
        call eval_factor
        cmp byte [ev_err], 0
        jne .perr
        pop ebx
        pop ecx                     ; izquierdo en ECX:EBX, derecho en EDX:EAX
        call mul_fijo               ; EDX:EAX * ECX:EBX / ESC
        jc .over
        jmp .l
.entre:
        inc dword [ev_ptr]
        push edx
        push eax
        call eval_factor
        cmp byte [ev_err], 0
        jne .perr
        pop ebx
        pop ecx                     ; izquierdo (dividendo) en ECX:EBX
        mov esi, eax                ; ¿divisor 0?
        or esi, edx
        jz .divz
        xchg eax, ebx
        xchg edx, ecx               ; EDX:EAX=dividendo, ECX:EBX=divisor
        call div_fijo
        jc .over
        jmp .l
.divz:
        mov byte [ev_err], 1
        ret
.over:
        mov byte [ev_err], 1
        ret
.perr:
        add esp, 8
.x:
        ret

eval_factor:
        call salta_esp
        mov esi, [ev_ptr]
        mov al, [esi]
        cmp al, '-'
        je .neg
        cmp al, '+'
        je .pos
        jmp eval_primary
.pos:
        inc dword [ev_ptr]
        jmp eval_factor
.neg:
        inc dword [ev_ptr]
        call eval_factor
        cmp byte [ev_err], 0
        jne .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        ret

eval_primary:
        call salta_esp
        mov esi, [ev_ptr]
        mov al, [esi]
        cmp al, '('
        je .paren
        cmp al, '0'
        jb .nd
        cmp al, '9'
        jbe .num
.nd:
        cmp al, '.'
        je .num
        cmp al, ','
        je .num
        cmp al, 'a'
        jb .nl
        cmp al, 'z'
        jbe .letra
.nl:
        cmp al, 'A'
        jb .bad
        cmp al, 'Z'
        ja .bad
.letra:
        jmp parse_ident
.num:
        jmp num_parse
.paren:
        inc dword [ev_ptr]
        call eval_expr
        cmp byte [ev_err], 0
        jne .x
        call salta_esp
        mov esi, [ev_ptr]
        cmp byte [esi], ')'
        jne .bad
        inc dword [ev_ptr]
.x:
        ret
.bad:
        mov byte [ev_err], 1
        xor eax, eax
        xor edx, edx
        ret

; lee un número (sin signo) de [ev_ptr] -> EDX:EAX coma fija
num_parse:
        mov esi, [ev_ptr]
        xor eax, eax
        xor edx, edx
        mov [cu_acc], eax
        mov [cu_acc+4], edx
        xor ecx, ecx                ; decimales
        mov byte [cu_dot], 0
        mov byte [cu_any], 0
.l:
        mov bl, [esi]
        cmp bl, '0'
        jb .nd
        cmp bl, '9'
        ja .nd
        mov byte [cu_any], 1
        mov eax, [cu_acc]
        mov edx, [cu_acc+4]
        push ebx
        push ecx
        mov ebx, 10
        call mul64_32
        pop ecx
        pop ebx
        movzx edi, bl
        sub edi, '0'
        add eax, edi
        adc edx, 0
        mov [cu_acc], eax
        mov [cu_acc+4], edx
        cmp byte [cu_dot], 0
        je .dn
        inc ecx
.dn:
        inc esi
        jmp .l
.nd:
        cmp bl, '.'
        je .dot
        cmp bl, ','
        je .dot
        jmp .fin
.dot:
        cmp byte [cu_dot], 0
        jne .fin
        mov byte [cu_dot], 1
        inc esi
        jmp .l
.fin:
        mov [ev_ptr], esi
        cmp byte [cu_any], 0
        je .bad
        mov eax, [cu_acc]
        mov edx, [cu_acc+4]
        cmp ecx, CU_DEC
        ja .trunc
        mov ebx, CU_DEC
        sub ebx, ecx
        mov esi, 1
.mf:
        test ebx, ebx
        jz .mfd
        imul esi, 10
        dec ebx
        jmp .mf
.mfd:
        mov ebx, esi
        call mul64_32
        ret
.trunc:
        mov ebx, ecx
        sub ebx, CU_DEC
        mov esi, 1
.df:
        test ebx, ebx
        jz .dfd
        imul esi, 10
        dec ebx
        jmp .df
.dfd:
        mov ebx, esi
        call div64_32
        ret
.bad:
        mov byte [ev_err], 1
        xor eax, eax
        xor edx, edx
        ret

; un identificador: «SUMA» (función) o una referencia de una letra
parse_ident:
        mov esi, [ev_ptr]
        mov edi, cu_ident
        xor ecx, ecx
.l:
        mov al, [esi]
        cmp al, 'a'
        jb .u
        cmp al, 'z'
        ja .u
        sub al, 0x20
.u:
        cmp al, 'A'
        jb .done
        cmp al, 'Z'
        ja .done
        cmp ecx, 7
        jae .skip
        mov [edi], al
        inc edi
        inc ecx
.skip:
        inc esi
        jmp .l
.done:
        mov byte [edi], 0
        mov [ev_ptr], esi
        mov esi, cu_ident
        mov edi, t_suma
        call cad_igual
        jnc parse_suma
        cmp ecx, 1
        jne .bad
        movzx eax, byte [cu_ident]
        sub eax, 'A'                ; columna
        push eax
        call lee_fila               ; EAX = fila (0-base), CF=1 si no vale
        mov edx, eax
        pop ecx
        jc .bad
        call celda_valor
        ret
.bad:
        mov byte [ev_err], 3
        xor eax, eax
        xor edx, edx
        ret

; lee un número de fila de [ev_ptr] -> EAX = fila-1; CF=1 si no vale (1..NFIL)
lee_fila:
        mov esi, [ev_ptr]
        xor eax, eax
        xor ecx, ecx
.l:
        movzx ebx, byte [esi]
        cmp bl, '0'
        jb .f
        cmp bl, '9'
        ja .f
        imul eax, 10
        sub bl, '0'
        movzx ebx, bl
        add eax, ebx
        inc esi
        inc ecx
        cmp eax, 100000
        jb .l
        mov [ev_ptr], esi
        stc
        ret
.f:
        mov [ev_ptr], esi
        test ecx, ecx
        jz .bad
        test eax, eax
        jz .bad
        cmp eax, NFIL
        ja .bad
        dec eax
        clc
        ret
.bad:
        stc
        ret

; lee una referencia (letra + fila) de [ev_ptr] -> EAX=col EBX=fil; CF=1 si no
lee_ref:
        call salta_esp
        mov esi, [ev_ptr]
        mov al, [esi]
        cmp al, 'a'
        jb .u
        cmp al, 'z'
        ja .u
        sub al, 0x20
.u:
        cmp al, 'A'
        jb .bad
        cmp al, 'Z'
        ja .bad
        sub al, 'A'
        movzx eax, al
        inc dword [ev_ptr]
        push eax
        call lee_fila
        jc .bad2
        mov ebx, eax
        pop eax
        clc
        ret
.bad2:
        add esp, 4
.bad:
        stc
        ret

; =SUMA(ref:ref): suma del rango
parse_suma:
        call salta_esp
        mov esi, [ev_ptr]
        cmp byte [esi], '('
        jne .bad
        inc dword [ev_ptr]
        call lee_ref
        jc .bad
        mov [sum_c1], eax
        mov [sum_f1], ebx
        call salta_esp
        mov esi, [ev_ptr]
        cmp byte [esi], ':'
        jne .bad
        inc dword [ev_ptr]
        call lee_ref
        jc .bad
        mov [sum_c2], eax
        mov [sum_f2], ebx
        call salta_esp
        mov esi, [ev_ptr]
        cmp byte [esi], ')'
        jne .bad
        inc dword [ev_ptr]
        ; normalizar el rango
        mov eax, [sum_c1]
        mov ebx, [sum_c2]
        cmp eax, ebx
        jle .nc
        mov [sum_c1], ebx
        mov [sum_c2], eax
.nc:
        mov eax, [sum_f1]
        mov ebx, [sum_f2]
        cmp eax, ebx
        jle .nf
        mov [sum_f1], ebx
        mov [sum_f2], eax
.nf:
        xor eax, eax
        mov [sum_lo], eax
        mov [sum_hi], eax
        mov eax, [sum_f1]
        mov [sum_fi], eax
.rf:
        mov eax, [sum_c1]
        mov [sum_ci], eax
.rc:
        push dword [sum_lo]
        push dword [sum_hi]
        push dword [sum_ci]
        push dword [sum_fi]
        push dword [sum_c1]
        push dword [sum_c2]
        push dword [sum_f1]
        push dword [sum_f2]
        mov ecx, [sum_ci]
        mov edx, [sum_fi]
        call celda_valor
        pop dword [sum_f2]
        pop dword [sum_f1]
        pop dword [sum_c2]
        pop dword [sum_c1]
        pop dword [sum_fi]
        pop dword [sum_ci]
        pop dword [sum_hi]
        pop dword [sum_lo]
        cmp byte [ev_err], 0
        jne .err
        add [sum_lo], eax
        adc [sum_hi], edx
        mov eax, [sum_ci]
        inc eax
        mov [sum_ci], eax
        cmp eax, [sum_c2]
        jle .rc
        mov eax, [sum_fi]
        inc eax
        mov [sum_fi], eax
        cmp eax, [sum_f2]
        jle .rf
        mov eax, [sum_lo]
        mov edx, [sum_hi]
        ret
.err:
        xor eax, eax
        xor edx, edx
        ret
.bad:
        mov byte [ev_err], 1
        xor eax, eax
        xor edx, edx
        ret

; compara ESI y EDI (0): CF=0 si son iguales
cad_igual:
        push esi
        push edi
        push eax
.l:
        mov al, [esi]
        cmp al, [edi]
        jne .no
        test al, al
        jz .si
        inc esi
        inc edi
        jmp .l
.si:
        pop eax
        pop edi
        pop esi
        clc
        ret
.no:
        pop eax
        pop edi
        pop esi
        stc
        ret

; =============================================================================
;  Aritmética de coma fija (64 bits)
; =============================================================================
; EDX:EAX *= EBX (sin signo, lo que desborde se pierde)
mul64_32:
        push ecx
        push esi
        mov ecx, edx
        mul ebx
        mov esi, eax
        mov eax, ecx
        push edx
        mul ebx
        pop ecx
        add eax, ecx
        mov edx, eax
        mov eax, esi
        pop esi
        pop ecx
        ret

; EDX:EAX /= EBX (sin signo); el resto queda en cu_rem
div64_32:
        push ecx
        push esi
        push edi
        mov esi, ebx
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        div esi
        mov edi, eax
        mov eax, ecx
        div esi
        mov [cu_rem], edx
        mov edx, edi
        pop edi
        pop esi
        pop ecx
        ret

; EDX:EAX y ECX:EBX a valor absoluto; cu_neg = signo del resultado
signos64:
        mov byte [cu_neg], 0
        test edx, edx
        jns .a
        not byte [cu_neg]
        neg edx
        neg eax
        sbb edx, 0
.a:
        test ecx, ecx
        jns .b
        not byte [cu_neg]
        neg ecx
        neg ebx
        sbb ecx, 0
.b:
        ret

; aplica cu_neg a EDX:EAX; CF=1 si no cabe (más de 63 bits)
pon_signo64:
        test edx, edx
        js .ov
        cmp byte [cu_neg], 0
        je .x
        neg edx
        neg eax
        sbb edx, 0
.x:
        clc
        ret
.ov:
        stc
        ret

; EDX:EAX (sin signo) * ECX:EBX -> cu_m (128 bits)
mul128:
        pushad
        mov esi, eax
        mov edi, edx
        mul ebx
        mov [cu_m], eax
        mov [cu_m+4], edx
        mov eax, edi
        mul ecx
        mov [cu_m+8], eax
        mov [cu_m+12], edx
        mov eax, esi
        mul ecx
        add [cu_m+4], eax
        adc [cu_m+8], edx
        adc dword [cu_m+12], 0
        mov eax, edi
        mul ebx
        add [cu_m+4], eax
        adc [cu_m+8], edx
        adc dword [cu_m+12], 0
        popad
        ret

; cu_m (128 bits) / ECX:EBX -> EDX:EAX; CF=1 si no cabe en 64 bits
div128:
        push esi
        push edi
        push ebp
        xor esi, esi
        xor edi, edi
        mov ebp, 128
.l:
        shl dword [cu_m], 1
        rcl dword [cu_m+4], 1
        rcl dword [cu_m+8], 1
        rcl dword [cu_m+12], 1
        rcl esi, 1
        rcl edi, 1
        jc .resta
        cmp edi, ecx
        ja .resta
        jb .sig
        cmp esi, ebx
        jb .sig
.resta:
        sub esi, ebx
        sbb edi, ecx
        or byte [cu_m], 1
.sig:
        dec ebp
        jnz .l
        mov eax, [cu_m]
        mov edx, [cu_m+4]
        mov esi, [cu_m+8]
        or esi, [cu_m+12]
        pop ebp
        pop edi
        pop esi
        jnz .no
        clc
        ret
.no:
        stc
        ret

; EDX:EAX = (EDX:EAX * ECX:EBX) / CU_ESC (con signo); CF=1 si desborda
mul_fijo:
        call signos64
        call mul128
        mov ebx, CU_ESC
        xor ecx, ecx
        call div128
        jc .ov
        call pon_signo64
        ret
.ov:
        stc
        ret

; EDX:EAX = (EDX:EAX * CU_ESC) / ECX:EBX (con signo); CF=1 si desborda
div_fijo:
        call signos64
        push ebx
        push ecx
        mov ebx, CU_ESC
        xor ecx, ecx
        call mul128
        pop ecx
        pop ebx
        call div128
        jc .ov
        call pon_signo64
        ret
.ov:
        stc
        ret

; EDX:EAX coma fija (con signo) -> texto en EDI (EDI avanza; sin 0 final)
num_a_cad:
        test edx, edx
        jns .pos
        mov byte [edi], '-'
        inc edi
        neg edx
        neg eax
        sbb edx, 0
.pos:
        mov ebx, CU_ESC
        call div64_32               ; EDX:EAX = entero, cu_rem = decimales
        call print_u64
        mov eax, [cu_rem]
        test eax, eax
        jz .x
        mov byte [edi], ','
        inc edi
        call print_frac
.x:
        ret

; EDX:EAX (sin signo) -> decimal en EDI
print_u64:
        push ecx
        push esi
        xor ecx, ecx
.l:
        mov ebx, 10
        call div64_32
        mov esi, [cu_rem]
        push esi
        inc ecx
        mov esi, eax
        or esi, edx
        jnz .l
.e:
        pop eax
        add al, '0'
        mov [edi], al
        inc edi
        dec ecx
        jnz .e
        pop esi
        pop ecx
        ret

; EAX = decimales (1..9999) -> hasta CU_DEC dígitos en EDI, sin ceros finales
print_frac:
        push ebx
        push ecx
        mov ecx, eax
        mov eax, ecx
        xor edx, edx
        mov ebx, 1000
        div ebx
        mov [cu_fd+0], al
        mov eax, edx
        xor edx, edx
        mov ebx, 100
        div ebx
        mov [cu_fd+1], al
        mov eax, edx
        xor edx, edx
        mov ebx, 10
        div ebx
        mov [cu_fd+2], al
        mov [cu_fd+3], dl
        mov ecx, 4
.tl:
        mov al, [cu_fd+ecx-1]
        test al, al
        jnz .e
        dec ecx
        jnz .tl
.e:
        xor ebx, ebx
.e2:
        mov al, [cu_fd+ebx]
        add al, '0'
        mov [edi], al
        inc edi
        inc ebx
        cmp ebx, ecx
        jl .e2
        pop ecx
        pop ebx
        ret

; ESI (0) -> EDX:EAX coma fija; CF=1 si no es un número entero completo
cad_a_num:
        push esi
        push ebx
        push ecx
        push edi
        mov edi, esi
        mov byte [cu_sign], 0
        mov al, [edi]
        cmp al, '-'
        jne .s1
        mov byte [cu_sign], 1
        inc edi
        jmp .s2
.s1:
        cmp al, '+'
        jne .s2
        inc edi
.s2:
        xor eax, eax
        xor edx, edx
        mov [cu_acc], eax
        mov [cu_acc+4], edx
        xor ecx, ecx
        mov byte [cu_dot], 0
        mov byte [cu_any], 0
.dl:
        mov bl, [edi]
        cmp bl, '0'
        jb .nd
        cmp bl, '9'
        ja .nd
        mov byte [cu_any], 1
        mov eax, [cu_acc]
        mov edx, [cu_acc+4]
        push ebx
        push ecx
        mov ebx, 10
        call mul64_32
        pop ecx
        pop ebx
        movzx ebx, bl
        sub ebx, '0'
        add eax, ebx
        adc edx, 0
        mov [cu_acc], eax
        mov [cu_acc+4], edx
        cmp byte [cu_dot], 0
        je .dn
        inc ecx
.dn:
        inc edi
        jmp .dl
.nd:
        cmp bl, '.'
        je .dot
        cmp bl, ','
        je .dot
        jmp .end
.dot:
        cmp byte [cu_dot], 0
        jne .end
        mov byte [cu_dot], 1
        inc edi
        jmp .dl
.end:
        cmp byte [cu_any], 0
        je .bad
        cmp byte [edi], 0
        jne .bad
        mov eax, [cu_acc]
        mov edx, [cu_acc+4]
        cmp ecx, CU_DEC
        ja .trunc
        mov ebx, CU_DEC
        sub ebx, ecx
        mov esi, 1
.mf:
        test ebx, ebx
        jz .mfd
        imul esi, 10
        dec ebx
        jmp .mf
.mfd:
        mov ebx, esi
        call mul64_32
        jmp .sign
.trunc:
        mov ebx, ecx
        sub ebx, CU_DEC
        mov esi, 1
.df:
        test ebx, ebx
        jz .dfd
        imul esi, 10
        dec ebx
        jmp .df
.dfd:
        mov ebx, esi
        call div64_32
.sign:
        cmp byte [cu_sign], 0
        je .pos
        neg edx
        neg eax
        sbb edx, 0
.pos:
        clc
        jmp .out
.bad:
        stc
.out:
        pop edi
        pop ecx
        pop ebx
        pop esi
        ret

; =============================================================================
;  Archivos (.CSV)
; =============================================================================
; pone todas las celdas a vacío (y limpia los estados)
limpia_todo:
        pushad
        mov edi, cel_txt
        mov ecx, NCEL * CEL_MAX / 4
        xor eax, eax
        rep stosd
        mov edi, cel_estado
        mov ecx, NCEL / 4
        rep stosd
        popad
        ret

; busca la última columna y fila con contenido -> maxc, maxr (-1 si vacío)
bounds:
        pushad
        mov dword [maxc], -1
        mov dword [maxr], -1
        xor edx, edx
.rf:
        cmp edx, NFIL
        jae .x
        xor ecx, ecx
.cf:
        cmp ecx, NCOL
        jae .nextr
        call cel_puntero
        cmp byte [esi], 0
        je .next
        cmp ecx, [maxc]
        jle .nc
        mov [maxc], ecx
.nc:
        cmp edx, [maxr]
        jle .nr
        mov [maxr], edx
.nr:
.next:
        inc ecx
        jmp .cf
.nextr:
        inc edx
        jmp .rf
.x:
        popad
        ret

guardar:
        cmp byte [nombre], 0
        jne guarda_escribe
        ; cae a guarda_como
guarda_como:
        pushad
        mov ebp, [ven]
        mov edi, nom_tmp
        mov esi, nombre
        call hvl_cad
        mov byte [edi], 0
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .no
        ; copiar a nombre, con extensión .CSV si falta
        mov esi, nom_tmp
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
        mov esi, t_ext_csv
        call hvl_cad
.f2:
        mov byte [edi], 0
        popad
        jmp guarda_escribe
.no:
        popad
        stc
        ret

; escribe la hoja en [nombre] como CSV; CF=1 si no se guarda
guarda_escribe:
        pushad
        call bounds
        mov ecx, NCEL
        imul ecx, CEL_MAX + 2
        add ecx, NFIL * 2 + 64
        HV mem_pedir
        jc .falla
        mov [buf_ptr], eax
        mov edi, eax
        cmp dword [maxr], 0
        jl .write
        mov dword [sv_r], 0
.rl:
        mov eax, [sv_r]
        cmp eax, [maxr]
        jg .write
        mov dword [sv_c], 0
.cl:
        mov eax, [sv_c]
        cmp eax, [maxc]
        jg .eol
        mov ecx, [sv_c]
        mov edx, [sv_r]
        call cel_puntero
.cpy:
        mov al, [esi]
        test al, al
        jz .cpyend
        cmp al, 13
        je .skipc
        cmp al, 10
        je .skipc
        mov [edi], al
        inc edi
.skipc:
        inc esi
        jmp .cpy
.cpyend:
        mov eax, [sv_c]
        cmp eax, [maxc]
        jge .nosep
        mov byte [edi], ';'
        inc edi
.nosep:
        inc dword [sv_c]
        jmp .cl
.eol:
        mov byte [edi], 13
        inc edi
        mov byte [edi], 10
        inc edi
        inc dword [sv_r]
        jmp .rl
.write:
        mov ecx, edi
        sub ecx, [buf_ptr]
        mov esi, nombre
        mov edx, [buf_ptr]
        call hvl_escribir           ; ESI=ruta EDX=datos ECX=octetos -> CF
        pushf
        mov eax, [buf_ptr]
        HV mem_soltar
        popf
        jc .falla2
        mov byte [modif], 0
        call pon_titulo
        popad
        clc
        ret
.falla2:
.falla:
        mov ebp, [ven]
        mov esi, t_no_guardar
        mov edi, t_cuentas
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        stc
        ret

; carga el .CSV de ESI (0)
cargar:
        pushad
        mov [car_ruta], esi
        mov ecx, 196608
        HV mem_pedir
        jc .falla
        mov [buf_ptr], eax
        mov esi, [car_ruta]
        xor al, al
        HV dos_abrir
        jc .falla1
        mov ebx, eax
        mov ecx, 196607
        mov edi, [buf_ptr]
        HV dos_leer
        mov ecx, eax
        HV dos_cerrar
        mov edi, [buf_ptr]
        mov byte [edi+ecx], 0
        call limpia_todo
        mov esi, [buf_ptr]
        xor edx, edx                ; fila
        xor ecx, ecx                ; columna
.pl:
        mov al, [esi]
        test al, al
        jz .pfin
        inc esi
        cmp al, 13
        je .pl
        cmp al, 10
        je .nl
        cmp al, ';'
        je .sep
        cmp edx, NFIL
        jae .pl
        cmp ecx, NCOL
        jae .pl
        push esi
        push ecx
        push edx
        call cel_puntero            ; ESI=destino (conserva AL)
        mov edi, esi
        xor ebx, ebx
.fe:
        cmp byte [edi], 0
        je .fa
        inc edi
        inc ebx
        cmp ebx, CEL_MAX - 1
        jb .fe
        pop edx
        pop ecx
        pop esi
        jmp .pl
.fa:
        mov [edi], al
        mov byte [edi+1], 0
        pop edx
        pop ecx
        pop esi
        jmp .pl
.sep:
        inc ecx
        jmp .pl
.nl:
        inc edx
        xor ecx, ecx
        jmp .pl
.pfin:
        mov eax, [buf_ptr]
        HV mem_soltar
        mov esi, [car_ruta]
        mov edi, nombre
        mov ecx, 79
.nm:
        mov al, [esi]
        test al, al
        jz .nmx
        mov [edi], al
        inc esi
        inc edi
        dec ecx
        jnz .nm
.nmx:
        mov byte [edi], 0
        xor eax, eax
        mov [cur_col], eax
        mov [cur_fil], eax
        mov [desp_col], eax
        mov [desp_fil], eax
        mov byte [editando], 0
        mov byte [modif], 0
        call pon_titulo
        call invalida
        popad
        ret
.falla1:
        mov eax, [buf_ptr]
        HV mem_soltar
.falla:
        mov ebp, [ven]
        mov esi, t_no_abrir
        mov edi, t_cuentas
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; si hay cambios, pregunta si se guardan. CF=1 si no hay que seguir
preguntar:
        cmp byte [modif], 0
        je .si
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_cuentas
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_NO
        je .si
        cmp eax, ID_SI
        jne .no
        call guardar
        jc .no
.si:
        clc
        ret
.no:
        stc
        ret

%include "hvplib.inc"

; =============================================================================
;  Variables
; =============================================================================
        section .bss
        alignb 4
ven         resd 1                  ; la ventana
param       resd 1                  ; parámetros del acceso directo
cli_an      resd 1                  ; zona de la ventana
cli_al      resd 1
vis_col     resd 1                  ; columnas y filas que se ven
vis_fil     resd 1
cur_col     resd 1                  ; celda activa
cur_fil     resd 1
desp_col    resd 1                  ; desplazamiento (esquina superior izquierda)
desp_fil    resd 1
editando    resd 1                  ; 1 si se está editando la celda activa
modif       resd 1                  ; 1 si hay cambios sin guardar

; pintado (iteradores y auxiliares)
p_ci        resd 1
p_fi        resd 1
d_col       resd 1
d_fil       resd 1
cm_col      resd 1
cm_fil      resd 1
a_x         resd 1
a_y         resd 1
fil_i       resd 1
esi2tmp     resd 1

; evaluador
ev_ptr      resd 1                  ; puntero de análisis
ev_depth    resd 1                  ; profundidad de anidamiento
ev_err      resb 1                  ; 0 ok, 1 #ERROR, 2 #CIRC, 3 #REF
            alignb 4
cu_acc      resd 2                  ; acumulador de dígitos (64 bits)
cu_m        resd 4                  ; producto de 128 bits
cu_rem      resd 1                  ; resto de div64_32
cu_neg      resb 1                  ; signo del producto/cociente
cu_dot      resb 1
cu_any      resb 1
cu_sign     resb 1
cu_fd       resb 4                  ; dígitos decimales
cu_ident    resb 16                 ; identificador leído
            alignb 4
sum_lo      resd 1                  ; SUMA: acumulador y rango
sum_hi      resd 1
sum_c1      resd 1
sum_f1      resd 1
sum_c2      resd 1
sum_f2      resd 1
sum_ci      resd 1
sum_fi      resd 1

; archivos
buf_ptr     resd 1
car_ruta    resd 1
maxc        resd 1
maxr        resd 1
sv_r        resd 1
sv_c        resd 1

disp_der    resb 1                  ; 1: alinear a la derecha
            alignb 4
disp        resb 40                 ; cómo se ve la celda
cel7        resb CEL_CARS + 4       ; recortado a la celda
ref_txt     resb 16                 ; «B7»
col_txt     resb 4
fil_txt     resb 16
bar_txt     resb 80
edt_buf     resb CEL_MAX            ; lo que se edita
clip_buf    resb CEL_MAX            ; portapapeles de una celda
nombre      resb 80                 ; nombre del archivo
nom_tmp     resb 80
titulo      resb 120

            alignb 4
cel_estado  resb NCEL               ; estado de evaluación (0/1)
            alignb 4
cel_txt     resb NCEL * CEL_MAX      ; texto crudo de cada celda

HVP_FIN
