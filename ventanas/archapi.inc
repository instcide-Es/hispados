; =============================================================================
;  Hispa Ventanas - [ARCHIVOS] lo que pide el Administrador de archivos
;  (programas/archivos.asm, ARCHIVOS.HVP)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Va en la parte alta del archivo (solo modo protegido). Para los módulos:
;    rm_interrup  ESI=bloque de registros (RL_*, defs.inc): una interrupción
;                 de modo real (RL_INT) con EAX, EBX, ECX, EDX, ESI y EDI del
;                 bloque y DS=ES=Hispa Ventanas: los punteros, dentro de
;                 dos_buf. A la vuelta, el bloque lleva los registros que
;                 deja la interrupción y RL_BAND sus banderas; CF=1 si deja
;                 el acarreo (el error de DOS, en RL_EAX)
;    dos_crit     -> EAX = el último error crítico de disco (el código de
;                 INT 24h: 0 protegido contra escritura, 2 no preparada...)
;                 desde la última vez que se preguntó; -1 si no ha habido
;    prog_abrir   EBP=ventana ESI=programa (.COM, .EXE, .BAT con su ruta, un
;                 .HVP o un interno, «:BLOC») EDI=parámetros EDX=carpeta de
;                 trabajo (0: la del programa) ECX=banderas (AB_VOLVER,
;                 AB_PAUSA) EBX=qué abrir al volver («ruta del .HVP y
;                 parámetros»; 0: nada): lo abre como un acceso directo
;  Del sistema:
;    arch_reabrir (centro_tarea) abre lo que se pidió en prog_abrir al volver
;    arch_abrir   (el menú del escritorio) abre PROGS\ARCHIVOS.HVP
; =============================================================================

rm_interrup:
        pushad
        mov eax, [esi+RL_EDI]
        mov [rmcs+0x00], eax
        mov eax, [esi+RL_ESI]
        mov [rmcs+0x04], eax
        mov eax, [esi+RL_EBX]
        mov [rmcs+0x10], eax
        mov eax, [esi+RL_EDX]
        mov [rmcs+0x14], eax
        mov eax, [esi+RL_ECX]
        mov [rmcs+0x18], eax
        mov eax, [esi+RL_EAX]
        mov [rmcs+0x1C], eax
        mov bl, [esi+RL_INT]
        call rm_int
        mov eax, [rmcs+0x00]
        mov [esi+RL_EDI], eax
        mov eax, [rmcs+0x04]
        mov [esi+RL_ESI], eax
        mov eax, [rmcs+0x10]
        mov [esi+RL_EBX], eax
        mov eax, [rmcs+0x14]
        mov [esi+RL_EDX], eax
        mov eax, [rmcs+0x18]
        mov [esi+RL_ECX], eax
        mov eax, [rmcs+0x1C]
        mov [esi+RL_EAX], eax
        movzx eax, word [rmcs+0x20]
        mov [esi+RL_BAND], eax
        mov dword [rmcs+0x00], 0    ; (que no se queden para los demás)
        mov dword [rmcs+0x04], 0
        popad
        test byte [esi+RL_BAND], 1
        jz .ok
        stc
        ret
.ok:
        clc
        ret

dos_crit:
        push edx
        or eax, -1
        movzx edx, word [lz_seg]
        test edx, edx
        jz .x
        shl edx, 4
        add edx, [cero]
        movzx eax, byte [edx+lz_crit]
        mov byte [edx+lz_crit], 0
        dec eax
.x:
        pop edx
        ret

prog_abrir:
        pushad
        mov [arch_reab_p], ebx
        mov edi, arch_acc           ; un acceso directo de paso
        push ecx
        mov ecx, AC_TAM / 4
        xor eax, eax
        rep stosd
        pop ecx
        mov byte [arch_acc+AC_USO], 1
        and cl, AB_VOLVER | AB_PAUSA
        mov [arch_acc+AC_BAND], cl
        lea edi, [arch_acc+AC_PROG]
        mov ecx, 79
        call cad_copiar
        mov esi, [esp+0]            ; (EDI de antes: los parámetros)
        test esi, esi
        jz .p
        lea edi, [arch_acc+AC_PARAM]
        mov ecx, 71
        call cad_copiar
.p:
        mov esi, [esp+20]           ; EDX: la carpeta
        test esi, esi
        jz .d
        lea edi, [arch_acc+AC_DIR]
        mov ecx, 67
        call cad_copiar
.d:
        mov eax, arch_acc
        call acc_lanzar
        cmp byte [salir], 2         ; se sale a Hispa-DOS: ¿qué abrir al volver?
        jne .x
        test byte [arch_acc+AC_BAND], AB_VOLVER
        jz .x
        mov esi, [arch_reab_p]
        test esi, esi
        jz .x
        mov edi, cen_reabrir
        mov ecx, 119
        call cad_copiar
.x:
        popad
        ret

arch_reabrir:
        pushad
        cmp byte [cen_reabrir], 0
        je .x
        mov edi, arch_acc
        mov ecx, AC_TAM / 4
        xor eax, eax
        rep stosd
        mov byte [arch_acc+AC_USO], 1
        mov esi, cen_reabrir        ; «ruta parámetros»
        lea edi, [arch_acc+AC_PROG]
        mov ecx, 79
.r:
        lodsb
        cmp al, ' '
        jbe .r1
        stosb
        loop .r
.r1:
        mov byte [edi], 0
        dec esi
.e:
        lodsb
        cmp al, ' '
        je .e
        dec esi
        lea edi, [arch_acc+AC_PARAM]
        mov ecx, 71
        call cad_copiar
        mov byte [cen_reabrir], 0
        lea esi, [arch_acc+AC_PROG]
        call es_hvp
        jne .x
        mov ebx, arch_acc
        mov ebp, [cen_ven]
        call hvp_abrir
.x:
        popad
        ret

arch_abrir:
        pushad
        mov edi, arch_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tn_archivos
        call pon_cad
        mov byte [edi], 0
        mov dword [arch_acc+AC_PARAM], 0
        mov ebx, arch_acc
        mov ebp, [cen_ven]
        call hvp_abrir
        popad
        ret

tn_archivos db "PROGS\ARCHIVOS.HVP", 0
k_reabrir   db "Reabrir", 0
