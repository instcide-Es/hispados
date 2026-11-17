; =============================================================================
;  TAREAS.HVP - el Administrador de tareas de Hispa Ventanas (Accesorios)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Enseña los programas que hay en marcha, con la memoria que gasta cada uno y
;  si responden, y la memoria del equipo (total, libre, el mayor bloque libre
;  y la convencional de DOS). Con un programa elegido:
;    - «Pasar a» (Intro, doble clic): lo trae delante; si estaba parado tras
;      un Ctrl+Alt+Supr, lo pone otra vez en marcha.
;    - «Terminar» (Supr): primero se le pide que cierre (puede preguntar si
;      guarda); si a los tres segundos no se ha cerrado, o no responde, se
;      ofrece terminarlo a la fuerza: el sistema suelta todo lo suyo.
;  Los del sistema (el Centro, el escritorio) salen en gris y no se terminan.
;  Se pone al día cada segundo, y solo se repinta si algo ha cambiado.
;
;  Ctrl+Alt+Supr dentro de Hispa Ventanas abre este programa (lo lanza el
;  núcleo, también con un programa colgado: ventanas/tareas_api.inc). Si se
;  vuelve a pulsar con el Administrador delante, el núcleo le manda
;  WM_HV_REINICIAR y aquí se pregunta si se reinicia el equipo.
;
;  Gasta muy poca memoria y nunca se queda esperando nada: es el programa al
;  que se acude cuando lo demás va mal. El aspecto es el de la familia
;  (programas/comun/barra.inc, docs/OFICINA.md).
; =============================================================================
%include "hvp.inc"
%include "fichas.inc"

O_SALIR     equ 101
O_ACTUALIZAR equ 102
O_PASAR     equ 103
O_TERMINAR  equ 104
O_ACERCA    equ 121

TB_AL       equ 30                  ; la barra de botones
CAB_Y       equ TB_AL + 6           ; los títulos de la lista
LY0         equ CAB_Y + 20          ; la primera fila
FIL_AL      equ 18                  ; alto de una fila
MEM_AL      equ 64                  ; la zona de la memoria, abajo
X_EST       equ 250                 ; la columna «Estado»
MAXF        equ 32                  ; programas que se enseñan, a lo más
T_REFRESCO  equ 1000                ; milisegundos entre una puesta al día y otra
ESPERA_CIERRE equ 55                ; tics (tres segundos) que se espera a un cierre

HVP_CABECERA "Administrador de tareas", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db "..0000000000000000000000000000.."
        db "..0444444444444444444444444440.."
        db "..0444444444444444444444444440.."
        db "..0444444444444444444444444440.."
        db "..0444444444444444444444444440.."
        db "..0000000000000000000000000000.."
        db "..0888888888888888888888888880.."
        db "..0888888888888888888888888880.."
        db "..0888000000000008888000008880.."
        db "..0888888888888888888888888880.."
        db "..0822222222222222222222222280.."
        db "..0822999999999992222999992280.."
        db "..0822222222222222222222222280.."
        db "..0888888888888888888888888880.."
        db "..0888000000000008888000008880.."
        db "..0888888888888888888888888880.."
        db "..0888888888888888888888888880.."
        db "..0888000000000008888000008880.."
        db "..0888888888888888888888888880.."
        db "..0888888888888888888888888880.."
        db "..0880000000000000000000000880.."
        db "..0880444444444444999999990880.."
        db "..0880444444444444999999990880.."
        db "..0880000000000000000000000880.."
        db "..0888888888888888888888888880.."
        db "..0000000000000000000000000000.."
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_tareas    dd tar_proc
            db C_CREMA, CB_FOCO, 0, 0
tar_plant:
        PLANTILLA c_tareas, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 170, 80, 470, 420, 0

; ---- la barra de menús ----
tar_menu:
        db "&Archivo", 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_ACTUALIZAR
        db "&Actualizar ahora        F5", 0
        dw 0xFFFF
        db "&Tarea", 0
        dw O_PASAR
        db "&Pasar a              Intro", 0
        dw O_TERMINAR
        db "&Terminar              Supr", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca del Administrador de tareas...", 0
        dw 0xFFFF
        db 0

; ---- los botones (programas/comun/barra.inc): x, ancho, dibujo, orden,
;      nombre y clase ----
            align 4
botones     dd 8, 76, t_b_pasar, O_PASAR, n_pasar, BT_TEXTO
            dd 88, 84, t_b_term, O_TERMINAR, n_term, BT_TEXTO
            dd 184, 96, t_b_act, O_ACTUALIZAR, n_act, BT_TEXTO
            dd 0, 0, 0, 0, 0, 0

t_titulo    db "Administrador de tareas", 0
t_b_pasar   db "Pasar a", 0
t_b_term    db "Terminar", 0
t_b_act     db "Actualizar", 0
n_pasar     db "Pasar al programa elegido (Intro)", 0
n_term      db "Terminar el programa elegido (Supr)", 0
n_act       db "Volver a mirar lo que hay en marcha (F5)", 0
t_c_prog    db "Programa", 0
t_c_est     db "Estado", 0
t_c_mem     db "Memoria", 0
t_e_abierto db "Abierto", 0
t_e_minim   db "Minimizado", 0
t_e_noresp  db "No responde", 0
t_e_sistema db "Sistema", 0
t_k         db " K", 0
t_m1        db "Libre: ", 0
t_m2        db " K de ", 0
t_m3        db " K   (el mayor bloque: ", 0
t_m4        db " K)", 0
t_m5        db "En los programas: ", 0
t_m6        db " K      Memoria de DOS libre: ", 0
t_p1        db " programas en marcha", 0
t_p1b       db " programa en marcha", 0
t_p2        db "      Esperando a que se cierre...", 0
t_q_term1   db "¿Terminar «", 0
t_q_term2   db "»?", 10, "Se perderá lo que no esté guardado.", 0
t_q_fuerza1 db "«", 0
t_q_fuerza2 db "» no se cierra.", 10, "¿Terminarlo a la fuerza? Se perderá lo que no esté guardado.", 0
t_q_reini   db "¿Reiniciar el equipo?", 10, "Se perderá lo que no esté guardado en los programas abiertos.", 0
t_e_sist    db "Es parte del sistema: no se puede terminar.", 0
t_e_pasar   db "No se puede pasar a ese programa: no tiene ventana.", 0
t_e_term    db "No se ha podido terminar ese programa.", 0
t_acerca    db "Administrador de tareas", 10
            db "Los programas en marcha y la memoria del equipo.", 10
            db "Ctrl+Alt+Supr lo abre; dos veces, reinicia.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0

; =============================================================================
;  La entrada y el procedimiento de la ventana
; =============================================================================
entrada:
        cld
        mov esi, tar_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

tar_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_TIMER
        je al_tic
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_LBUTTONDOWN
        je clic
        cmp eax, WM_LBUTTONDBLCLK
        je doble
        cmp eax, WM_MOUSEMOVE
        je mueve
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_HV_REINICIAR
        je reiniciar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], tar_menu
        mov dword [ebp+V_ICONO], hvp_icono
        call lee
        mov ebp, [ven]
        mov ecx, 1
        mov edx, T_REFRESCO
        HV tmp_poner
        jmp hecho
.cerrar:
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov ebp, [ven]
        mov ecx, 1
        HV tmp_quitar
        mov dword [ven], 0
        HV msg_salir
hecho:
        xor eax, eax
        ret

; la ventana, a repintar
inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        popad
        ret

; =============================================================================
;  Lo que hay en marcha
; =============================================================================
; lee la lista de tareas y la memoria; deja [sel] dentro de la lista
lee:
        pushad
        mov edi, fichas
        mov ecx, MAXF
        HV tar_lista
        cmp eax, MAXF
        jbe .n
        mov eax, MAXF
.n:
        mov [nf], eax
        HV mem_estado
        mov [m_total], eax
        mov [m_libre], ebx
        mov [m_mayor], ecx
        mov [m_mods], edx
        mov [m_dos], esi
        mov eax, [nf]
        test eax, eax
        jz .cero
        cmp [sel], eax
        jb .ok
        dec eax
        mov [sel], eax
        jmp short .ok
.cero:
        mov dword [sel], 0
.ok:
        popad
        ret

; -> EAX = una suma de todo lo que se enseña (para no repintar si no cambia)
suma:
        push ecx
        push esi
        mov esi, fichas
        mov ecx, [nf]
        shl ecx, 4                  ; (dwords: 16 por ficha)
        xor eax, eax
.b:
        jecxz .f
        add eax, [esi]
        rol eax, 3
        add esi, 4
        dec ecx
        jmp .b
.f:
        add eax, [nf]
        add eax, [m_libre]
        add eax, [m_mods]
        add eax, [m_dos]
        add eax, [sel]
        add eax, [pend_id]
        pop esi
        pop ecx
        ret

; EAX=índice -> ESI=su ficha
ficha:
        mov esi, eax
        shl esi, 6
        add esi, fichas
        ret

; el temporizador: se mira otra vez; y lo que se esperaba que cerrase
al_tic:
        call lee
        call pendiente
        call suma
        cmp eax, [ult_suma]
        je hecho
        mov [ult_suma], eax
        call inval
        jmp hecho

; =============================================================================
;  Las órdenes
; =============================================================================
orden:
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_ACTUALIZAR
        je .act
        cmp ecx, O_PASAR
        je .pasar
        cmp ecx, O_TERMINAR
        je .term
        cmp ecx, O_ACERCA
        jne hecho
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp hecho
.salir:
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.act:
        call lee
        call inval
        jmp hecho
.pasar:
        call pasar
        jmp hecho
.term:
        call terminar
        jmp hecho

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret
; pregunta: ESI=texto -> ZF=1 si se acepta
pregunta:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        popad
        ret

; EDI=búfer ESI=antes EBX=ficha EDX=después: «antes» + nombre + «después»
frase:
        push esi
        mov edi, txt
        call hvl_cad
        lea esi, [ebx+TF_NOMBRE]
        call hvl_cad
        mov esi, edx
        call hvl_cad
        mov byte [edi], 0
        pop esi
        ret

; ---- Pasar a: el programa elegido, delante ----
pasar:
        pushad
        cmp dword [nf], 0
        je .x
        mov eax, [sel]
        call ficha
        mov eax, [esi+TF_ID]
        HV tar_activar
        jnc .x
        mov esi, t_e_pasar
        call aviso
.x:
        call lee
        call inval
        popad
        ret

; ---- Terminar: primero por las buenas; si no cierra o no responde, a la
;      fuerza ----
terminar:
        pushad
        cmp dword [nf], 0
        je .x
        mov eax, [sel]
        call ficha
        mov ebx, esi
        test dword [ebx+TF_ESTADO], TE_SISTEMA
        jz .no_sis
        mov esi, t_e_sist
        call aviso
        jmp .x
.no_sis:
        mov esi, t_q_term1          ; «¿Terminar «X»?»
        mov edx, t_q_term2
        call frase
        mov esi, txt
        call pregunta
        jne .x
        call lee                    ; (puede haber cambiado mientras se preguntaba)
        mov eax, [ebx+TF_ID]
        test dword [ebx+TF_ESTADO], TE_NORESP | TE_PARADA
        jnz .fuerza
        xor edx, edx                ; por las buenas: que cierre él
        HV tar_cerrar
        jc .fuerza                  ; (no se le puede pedir: sin ventana o parado)
        mov eax, [ebx+TF_ID]
        mov [pend_id], eax
        mov eax, [ebx+TF_NVEN]
        mov [pend_nven], eax
        HV tics
        mov [pend_t], eax
        jmp .x
.fuerza:
        mov eax, [ebx+TF_ID]
        mov edx, 1
        HV tar_cerrar
        jnc .x
        mov esi, t_e_term
        call aviso
.x:
        call lee
        call inval
        popad
        ret

; ---- lo que se esperaba que cerrase por las buenas: si ya no está, bien;
;      si ha abierto un cuadro (pregunta si guarda), se deja al usuario; si a
;      los tres segundos sigue, o no responde, se ofrece la fuerza ----
pendiente:
        pushad
        mov eax, [pend_id]
        test eax, eax
        jz .x
        xor ecx, ecx
.b:
        cmp ecx, [nf]
        jae .ya                     ; ya no está: se ha cerrado
        push eax
        mov eax, ecx
        call ficha
        pop eax
        cmp [esi+TF_ID], eax
        je .esta
        inc ecx
        jmp .b
.ya:
        mov dword [pend_id], 0
        jmp .x
.esta:
        mov ebx, esi
        mov eax, [ebx+TF_NVEN]
        cmp eax, [pend_nven]
        ja .ya                      ; pregunta algo: que conteste el usuario
        test dword [ebx+TF_ESTADO], TE_NORESP | TE_PARADA
        jnz .ofrece
        HV tics
        sub eax, [pend_t]
        cmp eax, ESPERA_CIERRE
        jb .x
.ofrece:
        mov dword [pend_id], 0      ; (antes de preguntar: el temporizador sigue)
        mov esi, t_q_fuerza1
        mov edx, t_q_fuerza2
        call frase
        mov esi, txt
        call pregunta
        jne .x
        mov eax, [ebx+TF_ID]
        mov edx, 1
        HV tar_cerrar
        jnc .hecho
        mov esi, t_e_term
        call aviso
.hecho:
        call lee
        call inval
.x:
        popad
        ret

; ---- Ctrl+Alt+Supr con el Administrador delante: ¿reiniciar? ----
reiniciar:
        mov esi, t_q_reini
        call pregunta
        jne hecho
        mov ebp, [ven]
        HV tar_reiniciar
        jmp hecho

; =============================================================================
;  Teclado y ratón
; =============================================================================
tecla:
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_INICIO
        je .ini
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_INTRO
        je .pasar
        cmp ecx, TC_SUPR
        je .term
        cmp ecx, TC_F1 + 4          ; F5
        je .act
        jmp hecho
.arr:
        cmp dword [sel], 0
        je hecho
        dec dword [sel]
        jmp .mov
.aba:
        mov eax, [sel]
        inc eax
        cmp eax, [nf]
        jae hecho
        mov [sel], eax
        jmp .mov
.ini:
        mov dword [sel], 0
        jmp .mov
.fin:
        mov eax, [nf]
        test eax, eax
        jz hecho
        dec eax
        mov [sel], eax
.mov:
        call a_la_vista
        call inval
        jmp hecho
.pasar:
        call pasar
        jmp hecho
.term:
        call terminar
        jmp hecho
.act:
        call lee
        call inval
        jmp hecho

; las filas que caben -> EAX (al menos 1)
filas_vis:
        mov eax, [cli_al]
        sub eax, LY0 + MEM_AL + BR_PIE + 4
        jg .d
        mov eax, 1
        ret
.d:
        push edx
        push ecx
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        pop ecx
        pop edx
        test eax, eax
        jnz .x
        inc eax
.x:
        ret

; que la elegida se vea
a_la_vista:
        push eax
        push ebx
        mov eax, [sel]
        cmp eax, [arriba]
        jae .a
        mov [arriba], eax
        jmp short .x
.a:
        mov ebx, eax
        call filas_vis
        sub ebx, eax                ; EBX = sel - vis
        jl .x
        inc ebx
        cmp ebx, [arriba]
        jbe .x
        mov [arriba], ebx
.x:
        pop ebx
        pop eax
        ret

; EDX=x + y*65536 -> EAX=la fila de la lista que hay ahí; CF=1 si ninguna
fila_en:
        mov eax, edx
        sar eax, 16
        sub eax, LY0
        jl .no
        push edx
        push ecx
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        pop ecx
        pop edx
        push eax
        call filas_vis
        mov ecx, eax
        pop eax
        cmp eax, ecx
        jae .no
        add eax, [arriba]
        cmp eax, [nf]
        jae .no
        clc
        ret
.no:
        stc
        ret

clic:
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jge .lista
        movsx eax, dx               ; la barra: el botón que hay ahí
        mov esi, botones
        call br_boton_en
        jc hecho
        mov dword [br_sobre], 0
        mov ecx, [esi+12]
        jmp orden
.lista:
        call fila_en
        jc hecho
        mov [sel], eax
        call inval
        jmp hecho

doble:
        call fila_en
        jc hecho
        mov [sel], eax
        call pasar
        jmp hecho

; el ratón sobre un botón: su nombre en el pie
mueve:
        xor edi, edi
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jge .n
        test ebx, ebx
        js .n
        movsx eax, dx
        mov esi, botones
        call br_boton_en
        jc .n
        mov edi, [esi+16]
.n:
        call br_sobre_pon
        je hecho
        call inval
        jmp hecho

; lo que pregunta la barra común: ningún botón se queda «puesto»
barra_estado:
        ret

; =============================================================================
;  El dibujo
; =============================================================================
pintar:
        mov ebp, [ven]
        HV dc_pintar
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, botones
        mov ebx, 4
        call br_fila
        ; los títulos de la lista
        COLOR C_ARENA
        xor eax, eax
        mov ebx, CAB_Y
        mov ecx, [cli_an]
        mov edx, 18
        HV dc_rect
        COLOR C_TINTA
        NEGRITA 1
        mov eax, 10
        mov ebx, CAB_Y + 1
        mov esi, t_c_prog
        HV dc_texto
        mov eax, X_EST
        mov esi, t_c_est
        HV dc_texto
        mov esi, t_c_mem
        call der_x
        HV dc_texto
        NEGRITA 0
        call pinta_filas
        call pinta_memoria
        call pinta_pie
        HV dc_cerrar
        jmp hecho

; ESI=texto -> EAX=la x para que acabe a 12 puntos del borde derecho
der_x:
        push ecx
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .f
        inc ecx
        jmp .l
.f:
        shl ecx, 3
        mov eax, [cli_an]
        sub eax, 12
        sub eax, ecx
        pop ecx
        ret

pinta_filas:
        pushad
        call a_la_vista
        call filas_vis
        mov [vis], eax
        mov dword [f_n], 0          ; la fila en pantalla
.b:
        mov ecx, [f_n]
        cmp ecx, [vis]
        jae .x
        mov eax, ecx
        add eax, [arriba]
        cmp eax, [nf]
        jae .x
        mov [f_i], eax
        call ficha
        mov ebx, esi                ; EBX = la ficha
        mov eax, ecx
        imul eax, FIL_AL
        add eax, LY0
        mov [f_y], eax
        ; el color: la elegida, en petróleo; el texto, según su estado
        mov byte [f_col], C_TINTA
        mov eax, [f_i]
        cmp eax, [sel]
        jne .c1
        COLOR C_PETROLEO
        mov eax, 4
        mov ebx, [f_y]
        push ecx
        mov ecx, [cli_an]
        sub ecx, 8
        mov edx, FIL_AL
        HV dc_rect
        pop ecx
        mov eax, [f_i]
        call ficha
        mov ebx, esi
        mov byte [f_col], C_CREMA
        jmp short .c2
.c1:
        test dword [ebx+TF_ESTADO], TE_SISTEMA
        jz .c2
        mov byte [f_col], C_PIEDRA
.c2:
        ; el nombre, hasta donde quepa antes de «Estado»
        mov edi, txt
        lea esi, [ebx+TF_NOMBRE]
        push ecx
        mov ecx, (X_EST - 24) / 8
.n:
        lodsb
        test al, al
        jz .nf
        stosb
        dec ecx
        jnz .n
.nf:
        mov byte [edi], 0
        pop ecx
        movzx eax, byte [f_col]
        HV dc_color
        mov eax, 10
        mov ebx, [f_y]
        inc ebx
        mov esi, txt
        HV dc_texto
        ; el estado
        mov eax, [f_i]
        call ficha
        mov ebx, esi
        mov eax, [ebx+TF_ESTADO]
        mov esi, t_e_sistema
        test eax, TE_SISTEMA
        jnz .e
        mov esi, t_e_noresp
        test eax, TE_NORESP | TE_PARADA
        jnz .er
        mov esi, t_e_minim
        test eax, TE_MINIM
        jnz .e
        mov esi, t_e_abierto
        jmp short .e
.er:
        mov eax, [f_i]              ; (no responde: en granate, salvo elegida)
        cmp eax, [sel]
        je .e
        COLOR C_GRANATE
.e:
        mov eax, X_EST
        mov ebx, [f_y]
        inc ebx
        HV dc_texto
        movzx eax, byte [f_col]
        HV dc_color
        ; la memoria, en K
        mov eax, [f_i]
        call ficha
        mov eax, [esi+TF_MEM]
        add eax, [esi+TF_PEDIDA]
        shr eax, 10
        mov edi, txt
        call hvl_num
        mov esi, t_k
        call hvl_cad
        mov byte [edi], 0
        mov esi, txt
        call der_x
        mov ebx, [f_y]
        inc ebx
        HV dc_texto
        inc dword [f_n]
        jmp .b
.x:
        popad
        ret

pinta_memoria:
        pushad
        mov eax, [cli_al]
        sub eax, BR_PIE + MEM_AL
        mov [f_y], eax
        COLOR C_ARENA_OSC           ; la raya de arriba
        xor eax, eax
        mov ebx, [f_y]
        mov ecx, [cli_an]
        mov edx, 1
        HV dc_rect
        ; «Memoria: N K en total, N K libres (el mayor bloque, N K)»
        mov edi, txt
        mov esi, t_m1
        call hvl_cad
        mov eax, [m_libre]
        shr eax, 10
        call hvl_num
        mov esi, t_m2
        call hvl_cad
        mov eax, [m_total]
        shr eax, 10
        call hvl_num
        mov esi, t_m3
        call hvl_cad
        mov eax, [m_mayor]
        shr eax, 10
        call hvl_num
        mov esi, t_m4
        call hvl_cad
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 10
        mov ebx, [f_y]
        add ebx, 6
        mov esi, txt
        HV dc_texto
        ; la barra: lo usado, en terracota
        mov eax, 10
        mov ebx, [f_y]
        add ebx, 26
        mov ecx, [cli_an]
        sub ecx, 20
        mov edx, 12
        COLOR C_BLANCO
        HV dc_rect
        mov esi, [m_total]
        test esi, esi
        jz .marco
        push eax
        push edx
        mov eax, esi
        sub eax, [m_libre]          ; lo usado
        jae .u
        xor eax, eax
.u:
        shr eax, 10                 ; (en K: que no se desborde al multiplicar)
        shr esi, 10
        jz .u0
        mul ecx
        div esi
        jmp short .u1
.u0:
        xor eax, eax
.u1:
        mov ecx, eax
        pop edx
        pop eax
        jecxz .marco
        COLOR C_TERRACOTA
        HV dc_rect
.marco:
        mov eax, 10
        mov ebx, [f_y]
        add ebx, 26
        mov ecx, [cli_an]
        sub ecx, 20
        mov edx, 12
        COLOR C_TINTA
        HV dc_marco
        ; «En los programas: N K      Memoria de DOS libre: N K»
        mov edi, txt
        mov esi, t_m5
        call hvl_cad
        mov eax, [m_mods]
        shr eax, 10
        call hvl_num
        mov esi, t_m6
        call hvl_cad
        mov eax, [m_dos]
        shr eax, 10
        call hvl_num
        mov esi, t_k
        call hvl_cad
        mov byte [edi], 0
        mov eax, 10
        mov ebx, [f_y]
        add ebx, 44
        mov esi, txt
        HV dc_texto
        popad
        ret

pinta_pie:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie                 ; (CF=1: el nombre de un botón)
        jc .x
        mov edi, txt
        mov eax, [nf]
        call hvl_num
        mov esi, t_p1
        cmp dword [nf], 1
        jne .p
        mov esi, t_p1b
.p:
        call hvl_cad
        cmp dword [pend_id], 0
        je .q
        mov esi, t_p2
        call hvl_cad
.q:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        mov esi, txt
        HV dc_texto
.x:
        popad
        ret

%include "barra.inc"
%include "hvplib.inc"

        section .bss
            BARRA_BSS
ven         resd 1
cli_an      resd 1
cli_al      resd 1
nf          resd 1                  ; programas en la lista
sel         resd 1                  ; el elegido
arriba      resd 1                  ; el primero que se ve
vis         resd 1
f_i         resd 1
f_n         resd 1
f_y         resd 1
m_total     resd 1
m_libre     resd 1
m_mayor     resd 1
m_mods      resd 1
m_dos       resd 1
ult_suma    resd 1
pend_id     resd 1                  ; al que se le ha pedido que cierre
pend_nven   resd 1                  ; sus ventanas entonces
pend_t      resd 1                  ; y cuándo
f_col       resb 1
            alignb 4
txt         resb 256
fichas      resb MAXF * TF_TAM
HVP_FIN
