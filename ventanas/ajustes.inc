; =============================================================================
;  Hispa Ventanas - ajustes del entorno (lo que cambia el programa
;  Configuración, CONFIG.HVP)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Aquí solo está lo mínimo (VENTANAS.COM tiene que caber en 64 KB): los
;  valores y cómo se aplican. Quien lee y escribe VENTANAS.INI, y quien hace
;  el salvapantallas, es CONFIG.HVP: al arrancar, el Centro de programas lo
;  carga con el parámetro «/A» (aj_iniciar) y se queda en marcha con su
;  ventana oculta; abrir su acceso directo la enseña (hvp_abrir).
;
;    aj_leer      EAX=número del ajuste (AJ_*, en defs.inc) -> EAX=su valor
;    aj_poner     EAX=número EDX=valor: lo cambia y lo aplica ya
;    esc_fondo    ESI=mosaico de 16x16 (256 octetos, un color por punto): es
;                 el fondo nuevo del escritorio, que se repinta
;    aj_iniciar   (Centro) pone en marcha CONFIG.HVP, si está
;    ev_act       (eventos) ha habido ratón o teclado: se apunta cuándo
;    rat_escala   (eventos) EAX=movimiento del ratón EBX=dónde va el resto
;                 -> EAX=puntos, con la velocidad puesta
; =============================================================================

aj_dirs     dd aj_rat_vel, aj_rat_doble, aj_rat_zurdo, vol_pct, vol_mudo, aj_ult_act
            dd vid_bits, vid_tipo, hay_rat, snd_hay
            dd t_pantalla, t_video, t_dpmi, t_acerca
aj_rat_vel  dd 2
aj_rat_doble dd 8
tn_config   db "CONFIG.HVP", 0

aj_leer:
        cmp eax, AJ_CONTORNO        ; [TAMAÑO]
        je .cont
        cmp eax, AJ_N
        jae .no
        push ebx
        mov ebx, [aj_dirs+eax*4]
        cmp eax, AJ_N_OCT
        jae .dir
        cmp eax, AJ_N_RW
        mov eax, [ebx]
        jb .x
        movzx eax, al
.x:
        pop ebx
        ret
.dir:
        xchg eax, ebx
        pop ebx
        ret
.no:
        xor eax, eax
        ret
.cont:
        movzx eax, byte [aj_contorno]
        ret

aj_poner:
        cmp eax, AJ_CONTORNO        ; [TAMAÑO]
        jne .n
        mov [aj_contorno], dl
        ret
.n:
        cmp eax, AJ_N_RW
        jae .x
        push eax
        mov eax, [aj_dirs+eax*4]
        mov [eax], edx
        pop eax
        cmp eax, AJ_VOLUMEN
        jb .x
        cmp eax, AJ_SILENCIO
        ja .x
        call vol_aplicar
        call vol_inv_icono
.x:
        ret

esc_fondo:
        pushad
        mov edi, mosaico
        mov ecx, 64
        rep movsd
        mov ebp, [escritorio]
        call ven_invalidar
        popad
        ret

aj_iniciar:
        pushad
        mov edi, aj_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tn_config
        call pon_cad
        mov word [aj_acc+AC_PARAM], '/A'
        mov esi, aj_acc + AC_PROG
        xor al, al
        call dos_abrir
        jc .x
        mov ebx, eax
        call dos_cerrar
        mov ebx, aj_acc
        mov ebp, [cen_ven]          ; (si no se puede cargar, el aviso sale aquí)
        call hvp_abrir
.x:
        popad
        ret

ev_act:
        mov byte [actividad], 1
        push eax
        call tics
        mov [aj_ult_act], eax
        pop eax
        ret

rat_escala:
        push edx
        imul eax, [aj_rat_vel]
        add eax, [ebx]
        mov edx, eax                ; la mitad, hacia cero
        shr edx, 31
        add edx, eax
        sar edx, 1
        sub eax, edx
        sub eax, edx
        mov [ebx], eax              ; lo que sobra, para la próxima
        mov eax, edx
        pop edx
        ret
