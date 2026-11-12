; =============================================================================
;  Hispa Ventanas - núcleo: memoria, tareas, mensajes y temporizadores
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Memoria: un «montón» en memoria extendida (función 0501h del DPMI).
;    mem_iniciar                 CF=1 si no hay memoria
;    mem_pedir    ECX=octetos -> EAX=puntero (puesto a cero); CF=1 si no hay
;    mem_soltar   EAX=puntero
;
;  Tareas (multitarea cooperativa): cada programa es una tarea con su pila y
;  su cola de mensajes. Solo se cambia de tarea dentro de msg_tomar, cuando
;  la que corre no tiene nada que hacer. La tarea 0 es el sistema (entrada,
;  escritorio y menús).
;    tar_crear    EAX=entrada -> EAX=tarea (0 si no caben más)
;    tar_ceder    deja pasar a las demás
;  Cuando la rutina de entrada vuelve (ret), la tarea termina y se destruyen
;  sus ventanas.
;
;  Mensajes:
;    msg_poner    EBP=ventana EAX=mensaje ECX, EDX: a la cola (PostMessage)
;    msg_enviar   lo mismo, llamando ya al procedimiento -> EAX (SendMessage)
;    msg_tomar    EDI=mensaje: espera el siguiente -> EAX=0 si es WM_QUIT
;    msg_mirar    EDI=mensaje: sin esperar; CF=1 si no hay ninguno
;    msg_despachar EDI=mensaje: lo entrega a su ventana (DispatchMessage)
;    msg_salir    pone WM_QUIT en la cola de la tarea que corre
;  Con la cola vacía, msg_tomar entrega WM_PAINT a las ventanas con algo que
;  repintar y WM_TIMER a los temporizadores vencidos.
;
;  Temporizadores (reloj de la BIOS, 18,2 tics por segundo):
;    tmp_poner    EBP=ventana ECX=número EDX=milisegundos
;    tmp_quitar   EBP=ventana ECX=número
;    tics         -> EAX=tics desde medianoche
; =============================================================================

MONTON_TAM  equ 640 * 1024
PILA_TAM    equ 8192
TAR_MAX     equ 8
COLA_MAX    equ 48
T_USO       equ 0                   ; 0 libre, 1 en marcha, 2 terminada
T_ESP       equ 4
T_PILA      equ 8
T_CAB       equ 12
T_N         equ 16
T_COLA      equ 20
T_TAM       equ T_COLA + COLA_MAX * M_TAM
TMP_MAX     equ 16
TM_VEN      equ 0
TM_ID       equ 4
TM_PER      equ 8
TM_PROX     equ 12

mem_iniciar:
        pushad
        mov ax, 0x0501
        mov ebx, MONTON_TAM >> 16
        mov ecx, MONTON_TAM & 0xFFFF
        int 0x31
        jc .x
        mov [mon_mango], di
        mov [mon_mango+2], si
        shl ebx, 16
        mov bx, cx
        sub ebx, [base]
        mov [mon_ini], ebx
        mov dword [ebx], MONTON_TAM
        mov dword [ebx+4], 0
        clc
.x:
        popad
        ret

mem_pedir:
        push ebx
        push ecx
        push edx
        push edi
        add ecx, 8 + 3
        and ecx, ~3
        mov ebx, [mon_ini]
        lea edx, [ebx+MONTON_TAM]
.b:
        cmp ebx, edx
        jae .no
        cmp dword [ebx+4], 0
        jne .sig
.une:
        mov eax, ebx                ; se une con los libres que le siguen
        add eax, [ebx]
        cmp eax, edx
        jae .cabe
        cmp dword [eax+4], 0
        jne .cabe
        mov eax, [eax]
        add [ebx], eax
        jmp .une
.cabe:
        mov eax, [ebx]
        cmp eax, ecx
        jb .sig
        sub eax, ecx
        cmp eax, 16
        jb .todo
        mov [ebx], ecx              ; se parte
        mov [ebx+ecx], eax
        mov dword [ebx+ecx+4], 0
.todo:
        mov dword [ebx+4], 1
        lea edi, [ebx+8]
        mov ecx, [ebx]
        sub ecx, 8
        shr ecx, 2
        xor eax, eax
        push edi
        rep stosd
        pop eax
        clc
        jmp .x
.sig:
        add ebx, [ebx]
        jmp .b
.no:
        xor eax, eax
        stc
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

mem_soltar:
        test eax, eax
        jz .x
        mov dword [eax-4], 0
.x:
        ret

; ---- tareas ----
tar_iniciar:
        pushad
        mov ecx, TAR_MAX * T_TAM
        call mem_pedir
        mov [tar_tabla], eax
        mov [tar_act], eax
        mov dword [eax+T_USO], 1
        popad
        ret

tar_crear:
        push ebx
        push ecx
        push edx
        mov edx, eax
        mov ebx, [tar_tabla]
        mov ecx, TAR_MAX
.b:
        cmp dword [ebx+T_USO], 0
        je .hay
        add ebx, T_TAM
        dec ecx
        jnz .b
.no:
        xor eax, eax
        jmp .x
.hay:
        mov ecx, PILA_TAM
        call mem_pedir
        jc .no
        mov [ebx+T_PILA], eax
        add eax, PILA_TAM
        mov dword [eax-4], tar_fin  ; adonde vuelve la entrada al acabar
        mov [eax-8], edx
        sub eax, 40                 ; y los registros (a cero) de tar_ceder
        mov [ebx+T_ESP], eax
        mov dword [ebx+T_CAB], 0
        mov dword [ebx+T_N], 0
        mov dword [ebx+T_USO], 1
        mov byte [actividad], 1
        mov eax, ebx
.x:
        pop edx
        pop ecx
        pop ebx
        ret

tar_ceder:
        pushad
        mov ebx, [tar_act]
        mov [ebx+T_ESP], esp
tar_otra:
        mov eax, [tar_tabla]
        add eax, TAR_MAX * T_TAM
.s:
        add ebx, T_TAM
        cmp ebx, eax
        jb .c
        mov ebx, [tar_tabla]
.c:
        cmp dword [ebx+T_USO], 1
        jne .s
        mov [tar_act], ebx
        mov esp, [ebx+T_ESP]
        popad
        ret

; aquí «vuelve» la entrada de una tarea
tar_fin:
        mov ebx, [tar_act]
        call ges_destruir_de_tarea
        mov dword [ebx+T_USO], 2    ; la pila la suelta el sistema
        mov byte [actividad], 1
        jmp tar_otra

; (sistema) suelta la pila de las tareas terminadas
tar_limpiar:
        pushad
        mov ebx, [tar_tabla]
        mov ecx, TAR_MAX
.b:
        cmp dword [ebx+T_USO], 2
        jne .s
        mov eax, [ebx+T_PILA]
        call mem_soltar
        mov dword [ebx+T_USO], 0
.s:
        add ebx, T_TAM
        dec ecx
        jnz .b
        popad
        ret

; ---- mensajes ----
msg_salir:
        push eax
        push ebx
        push ebp
        xor ebp, ebp
        mov eax, WM_QUIT
        mov ebx, [tar_act]
        call msg_poner_en
        pop ebp
        pop ebx
        pop eax
        ret

msg_poner:
        test ebp, ebp
        jz .x
        cmp dword [ebp+V_USO], 1
        jne .x
        push ebx
        mov ebx, [ebp+V_TAREA]
        call msg_poner_en
        pop ebx
.x:
        ret

; (interna) lo mismo, a la cola de la tarea EBX
msg_poner_en:
        push esi
        push edi
        mov esi, [ebx+T_N]
        cmp eax, WM_MOUSEMOVE       ; los movimientos seguidos se funden
        jne .n
        test esi, esi
        jz .n
        mov edi, [ebx+T_CAB]
        lea edi, [edi+esi-1]
        cmp edi, COLA_MAX
        jb .m
        sub edi, COLA_MAX
.m:
        shl edi, 4
        lea edi, [ebx+edi+T_COLA]
        cmp dword [edi+M_MSG], WM_MOUSEMOVE
        jne .n
        cmp [edi+M_VEN], ebp
        je .pon
.n:
        cmp esi, COLA_MAX
        jae .x                      ; cola llena: se pierde
        mov edi, [ebx+T_CAB]
        add edi, esi
        cmp edi, COLA_MAX
        jb .m2
        sub edi, COLA_MAX
.m2:
        shl edi, 4
        lea edi, [ebx+edi+T_COLA]
        inc dword [ebx+T_N]
.pon:
        mov [edi+M_VEN], ebp
        mov [edi+M_MSG], eax
        mov [edi+M_W], ecx
        mov [edi+M_L], edx
        mov byte [actividad], 1
.x:
        pop edi
        pop esi
        ret

; (interna) quita de las colas los mensajes de la ventana EBP (se destruye)
msg_purgar:
        pushad
        mov ebx, [ebp+V_TAREA]
        lea esi, [ebx+T_COLA]
        mov ecx, COLA_MAX
.b:
        cmp [esi+M_VEN], ebp
        jne .s
        mov dword [esi+M_VEN], 0
        mov dword [esi+M_MSG], WM_NULL
.s:
        add esi, M_TAM
        dec ecx
        jnz .b
        popad
        ret

msg_enviar:
        test ebp, ebp
        jz .no
        cmp dword [ebp+V_USO], 1
        jne .no
        pushad
        call [ebp+V_PROC]
        mov [esp+28], eax
        popad
        ret
.no:
        xor eax, eax
        ret

msg_tomar:
.l:
        call msg_mirar
        jnc .hay
        call tar_ceder
        jmp .l
.hay:
        xor eax, eax
        cmp dword [edi+M_MSG], WM_QUIT
        setne al
        ret

msg_mirar:
        pushad
        mov ebx, [tar_act]
        cmp dword [ebx+T_N], 0
        je .pintar
        mov esi, [ebx+T_CAB]
        lea eax, [esi+1]
        cmp eax, COLA_MAX
        jb .c
        xor eax, eax
.c:
        mov [ebx+T_CAB], eax
        dec dword [ebx+T_N]
        shl esi, 4
        lea esi, [ebx+esi+T_COLA]
        movsd
        movsd
        movsd
        movsd
        jmp .si
.pintar:
        mov ebp, [ven_tabla]        ; ventanas de la tarea con algo que pintar
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], ebx
        jne .vs
        test dword [ebp+V_ESTADO], S_NCSUCIO
        jz .v2
        call nc_pintar
.v2:
        mov eax, [ebp+V_UX0]
        cmp eax, [ebp+V_UX1]
        jge .vs
        mov [edi+M_VEN], ebp
        mov dword [edi+M_MSG], WM_PAINT
        xor eax, eax
        mov [edi+M_W], eax
        mov [edi+M_L], eax
        jmp .si
.vs:
        add ebp, V_TAM
        dec ecx
        jnz .v
        ; temporizadores vencidos
        call tics
        mov esi, temporizadores
        mov ecx, TMP_MAX
.t:
        mov ebp, [esi+TM_VEN]
        test ebp, ebp
        jz .ts
        cmp [ebp+V_TAREA], ebx
        jne .ts
        mov edx, [esi+TM_PROX]
        sub edx, eax                ; lo que falta
        jle .vence
        cmp edx, [esi+TM_PER]       ; (el reloj ha pasado por medianoche)
        jle .ts
.vence:
        add eax, [esi+TM_PER]
        mov [esi+TM_PROX], eax
        mov [edi+M_VEN], ebp
        mov dword [edi+M_MSG], WM_TIMER
        mov eax, [esi+TM_ID]
        mov [edi+M_W], eax
        mov dword [edi+M_L], 0
        jmp .si
.ts:
        add esi, 16
        dec ecx
        jnz .t
        popad
        stc
        ret
.si:
        mov byte [actividad], 1
        popad
        clc
        ret

msg_despachar:
        pushad
        mov ebp, [edi+M_VEN]
        test ebp, ebp
        jz .x
        mov eax, [edi+M_MSG]
        mov ecx, [edi+M_W]
        mov edx, [edi+M_L]
        call msg_enviar
        cmp dword [edi+M_MSG], WM_PAINT
        jne .x
        cmp dword [ebp+V_USO], 1
        jne .x
        xor eax, eax                ; si no la ha pintado, se da por pintada
        mov [ebp+V_UX0], eax
        mov [ebp+V_UX1], eax
.x:
        popad
        ret

; ---- temporizadores ----
tics:
        mov eax, [cero]
        mov eax, [eax+0x46C]
        ret

tmp_poner:
        pushad
        mov esi, temporizadores     ; el mismo, si ya estaba
        mov ebx, TMP_MAX
        xor edi, edi
.b:
        cmp dword [esi+TM_VEN], 0
        jne .o
        test edi, edi
        jnz .s
        mov edi, esi
        jmp .s
.o:
        cmp [esi+TM_VEN], ebp
        jne .s
        cmp [esi+TM_ID], ecx
        jne .s
        mov edi, esi
        jmp .pon
.s:
        add esi, 16
        dec ebx
        jnz .b
        test edi, edi
        jz .x
.pon:
        mov [edi+TM_VEN], ebp
        mov [edi+TM_ID], ecx
        mov eax, edx                ; milisegundos a tics (55 ms)
        xor edx, edx
        mov ebx, 55
        div ebx
        test eax, eax
        jnz .p2
        inc eax
.p2:
        mov [edi+TM_PER], eax
        mov ebx, eax
        call tics
        add eax, ebx
        mov [edi+TM_PROX], eax
.x:
        popad
        ret

tmp_quitar:
        pushad
        mov esi, temporizadores
        mov ebx, TMP_MAX
.b:
        cmp [esi+TM_VEN], ebp
        jne .s
        cmp [esi+TM_ID], ecx
        jne .s
        mov dword [esi+TM_VEN], 0
.s:
        add esi, 16
        dec ebx
        jnz .b
        popad
        ret

; (interna) quita todos los de la ventana EBP
tmp_quitar_de:
        pushad
        mov esi, temporizadores
        mov ebx, TMP_MAX
.b:
        cmp [esi+TM_VEN], ebp
        jne .s
        mov dword [esi+TM_VEN], 0
.s:
        add esi, 16
        dec ebx
        jnz .b
        popad
        ret
