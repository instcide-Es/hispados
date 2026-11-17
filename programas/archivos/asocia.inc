; =============================================================================
;  ARCHIVOS.HVP - qué programa abre cada tipo de archivo (\HISPADOS\ASOCIA.INI)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo común (leer, buscar, guardar, los de serie) está en
;  programas/comun/asocia_ini.inc. ASOCIA.INI, en la carpeta de Hispa
;  Ventanas, tiene una línea por
;  extensión:  EXT=programa. El programa va con su ruta desde esa carpeta
;  (PROGS\PLUMA.HVP), con la ruta entera (C:\JUEGOS\X.HVP) o es «:BLOC»
;  (el Bloc de notas, que va dentro de Hispa Ventanas). Si no existe, se
;  escribe con los de serie la primera vez que hace falta. Lo cambian
;  «Abrir con…» (con «usar siempre») y Configuración › Tipos de archivo.
; =============================================================================

; ---- ASOCIA.INI está en la carpeta de Hispa Ventanas (d_hisp)
asoc_pon_ruta:
        pushad
        mov esi, d_hisp
        mov ebx, n_asocia
        mov edi, asoc_ruta
        call ruta_unir
        popad
        ret
%macro ASOC_OLVIDA 0
        call crit_olvidar
%endmacro

; ---- abre ab_ruta con el programa de asoc_val. CF=1 si no lo hace ----
asoc_abrir:
        cmp byte [asoc_val], ':'
        jne .modulo
        mov esi, asoc_val           ; uno de dentro («:BLOC»)
        mov edi, ab_ruta
        call abrir_con_int
        clc
        ret
.modulo:
        mov esi, asoc_val
        cmp byte [esi+1], ':'       ; con la unidad: tal cual
        je .tal
        cmp byte [esi], '\'
        je .tal
        mov esi, d_hisp             ; si no, desde la carpeta de Hispa Ventanas
        mov ebx, asoc_val
        mov edi, ab_prog
        call ruta_unir
        mov esi, ab_prog
.tal:
        mov edi, ab_ruta
        call abrir_con
        clc
        ret

; ---- al abrir un archivo (o_abrir): si su extensión tiene programa, se
; abre con él y CF=0; si no, CF=1 (y se hace lo de siempre) ----
asoc_intenta:
        mov esi, ab_ruta
        call asoc_extension
        jc .no
        call asoc_carga
        call asoc_busca
        jc .no
        jmp asoc_abrir
.no:
        stc
        ret

; =============================================================================
;  Abrir con…: la lista de programas; luego, «¿siempre?»
; =============================================================================
o_abrir_con:
        call objetivo
        cmp eax, 1
        je .x
        call n_elegidos
        test eax, eax
        jz .x
        mov eax, [foco_i]
        cmp eax, [n_ents]
        jae .x
        mov ebx, eax
        shl ebx, 5
        test byte [ents+ebx+E_ATR], 0x10    ; (una carpeta, no)
        jnz .x
        mov edi, ab_ruta
        call ent_ruta
        jmp abrir_con_lista
.x:
        ret
; (también para lo que no se sabe abrir: ab_ruta ya puesta)
abrir_con_lista:
        ; ¿con qué?
        mov ebp, [ven]
        mov edi, t_con_titulo
        mov esi, t_con_preg
        mov ebx, asoc_progs
        mov edx, ASOC_PROG_TAM
        mov ecx, ASOC_N_PROGS
        HV dlg_lista
        cmp eax, ASOC_N_PROGS
        jae .x
        imul eax, eax, ASOC_PROG_TAM
        lea esi, [asoc_progs+eax+ASOC_NOM_TAM]
        mov edi, asoc_val
        call hvl_cad
        mov byte [edi], 0
        ; ¿siempre? (solo si tiene extensión)
        mov esi, ab_ruta
        call asoc_extension
        jc .abre
        mov edi, msg_txt
        mov esi, t_con_siempre1
        call hvl_cad
        mov esi, asoc_ext
        call hvl_cad
        mov esi, t_con_siempre2
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, msg_txt
        mov edi, t_con_titulo
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_SI
        je .guarda
        cmp eax, ID_NO
        jne .x
        jmp .abre
.guarda:
        call asoc_guarda
.abre:
        jmp asoc_abrir
.x:
        ret

; ---- los textos ----
t_con_titulo db "Abrir con", 0
t_con_preg  db "¿Con qué programa se abre?", 0
t_con_siempre1 db "¿Usar siempre este programa para los .", 0
t_con_siempre2 db "?", 10, "(Se puede cambiar en Configuración, en «Tipos de archivo».)", 0

; (programas/comun/asocia_ini.inc lo incluye archivos.asm detrás de este)
