; =============================================================================
;  Hispa Ventanas - rutinas de ayuda para los módulos .HVP
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Se ensamblan dentro del módulo (%include "hvplib.inc", después del
;  código). Conservan los registros salvo los que devuelven algo.
;    hvl_clases     ESI=plantillas seguidas (acabadas en dd 0): donde la clase
;                   es un número de la interfaz (HV_c_boton, HV_c_estatico...)
;                   pone la dirección de esa clase. Una vez, antes de usarlas.
;    hvl_cad        copia ESI (acabada en 0) a EDI, sin el 0; EDI avanza
;    hvl_num        escribe EAX en decimal en EDI, que avanza (sin 0 final)
;    hvl_lee_num    ESI=texto -> EAX=número (ESI avanza; salta los espacios)
;    hvl_opcion     ESI=parámetros AL=letra (mayúscula): busca «/L» -> CF=0 y
;                   ESI detrás (y detrás del «=», si lo hay); CF=1 si no está
;    hvl_clave      ESI=texto (acabado en 0) EDI=clave ("Nombre="): la busca
;                   -> CF=0 y ESI detrás de la clave; CF=1 si no está
;    hvl_ruta       ESI=ruta del módulo EDI=búfer (80) EAX=extensión ('REC'):
;                   la misma ruta con esa extensión (el archivo de datos)
;    hvl_leer       ESI=ruta EDI=búfer ECX=tamaño -> EAX=octetos leídos (0 si
;                   no está); detrás deja un 0
;    hvl_escribir   ESI=ruta EDX=datos ECX=octetos -> CF=1 si no se puede (o no cabe entero);
;                   primero en un temporal: si falla, el archivo de antes queda
;                   como estaba (guardar.inc)
;    gua_crear, gua_cerrar   lo mismo, para quien escribe a trozos (guardar.inc)
;    hvl_reemplazar ESI=ruta que se va a escribir («Guardar como») EDX=la del
;                   documento (0: ninguna) EBP=dueña EDI=título -> CF=1 si ya
;                   hay otro archivo con ese nombre y no se quiere reemplazar
;                   («Ya existe... ¿Quiere reemplazarlo?», Sí / No)
; =============================================================================

hvl_clases:
        push eax
        push esi
.b:
        mov eax, [esi+P_CLASE]
        test eax, eax
        jz .x
        cmp eax, HV_API_N
        jae .s
        mov eax, [hv_api+eax*4]
        mov [esi+P_CLASE], eax
.s:
        add esi, P_TAM
        jmp .b
.x:
        pop esi
        pop eax
        ret

hvl_cad:
        push eax
.c:
        lodsb
        test al, al
        jz .x
        stosb
        jmp .c
.x:
        pop eax
        ret

hvl_num:
        push eax
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor ecx, ecx
.d:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .d
.e:
        pop eax
        add al, '0'
        stosb
        dec ecx
        jnz .e
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

hvl_lee_num:
        push ebx
        xor eax, eax
.e:
        cmp byte [esi], ' '
        jne .d
        inc esi
        jmp .e
.d:
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .x
        imul eax, 10
        add eax, ebx
        inc esi
        jmp .d
.x:
        pop ebx
        ret

hvl_opcion:
        push eax
        push ebx
        mov bl, al
.b:
        lodsb
        test al, al
        jz .no
        cmp al, '/'
        jne .b
        mov al, [esi]
        cmp al, 'a'
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 0x20
.m:
        cmp al, bl
        jne .b
        inc esi
        cmp byte [esi], '='
        jne .si
        inc esi
.si:
        clc
        jmp .x
.no:
        stc
.x:
        pop ebx
        pop eax
        ret

hvl_ruta:
        push eax
        push ebx
        push esi
        push edi
        xor ebx, ebx
.c:
        lodsb
        stosb
        cmp al, '.'
        jne .c1
        mov ebx, edi
.c1:
        test al, al
        jnz .c
        test ebx, ebx
        jz .x
        mov eax, [esp+12]
        mov [ebx], eax              ; (el cuarto octeto de EAX es el 0 final)
.x:
        pop edi
        pop esi
        pop ebx
        pop eax
        ret

hvl_leer:
        push ebx
        push ecx
        xor al, al
        HV dos_abrir
        jc .no
        mov ebx, eax
        dec ecx
        HV dos_leer
        jnc .ok
        xor eax, eax
.ok:
        HV dos_cerrar
        mov byte [edi+eax], 0
        jmp .x
.no:
        xor eax, eax
        mov [edi], al
.x:
        pop ecx
        pop ebx
        ret

; (sin vaciar el de antes: guardar.inc)
hvl_escribir:
        jmp gua_escribir

hvl_clave:
        push eax
        push ecx
.b:
        cmp byte [esi], 0
        je .no
        xor ecx, ecx
.c:
        mov al, [edi+ecx]
        test al, al
        jz .si
        cmp al, [esi+ecx]
        jne .s
        inc ecx
        jmp .c
.s:
        inc esi
        jmp .b
.si:
        add esi, ecx
        clc
        jmp .x
.no:
        stc
.x:
        pop ecx
        pop eax
        ret

hvl_reemplazar:
        pushad
        test edx, edx               ; ¿es el mismo del documento? Eso es guardar
        jz .mira
        xor ebx, ebx
.cmp:
        mov al, [esi+ebx]
        mov ah, [edx+ebx]
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        cmp ah, 'a'
        jb .c2
        cmp ah, 'z'
        ja .c2
        sub ah, 32
.c2:
        cmp al, ah
        jne .mira
        test al, al
        jz .si
        inc ebx
        jmp .cmp
.mira:
        push esi
        xor al, al
        HV dos_abrir
        pop esi
        jc .si                      ; no está: nada que reemplazar
        mov ebx, eax
        HV dos_cerrar
        push edi
        mov edi, hvl_txt
        push esi
        mov esi, hvl_t_existe
        call hvl_cad
        pop esi
        mov ecx, 100
.n:
        lodsb
        test al, al
        jz .n1
        stosb
        dec ecx
        jnz .n
.n1:
        mov esi, hvl_t_reempl
        call hvl_cad
        mov byte [edi], 0
        pop edi
        mov esi, hvl_txt
        mov eax, DM_SI_NO
        HV dlg_mensaje
        cmp eax, ID_SI
        jne .no
.si:
        popad
        clc
        ret
.no:
        popad
        stc
        ret
hvl_t_existe db "Ya hay un archivo con ese nombre:", 10, 0
hvl_t_reempl db 10, "¿Quiere reemplazarlo? (lo que tiene se perderá)", 0

%define GUA_MODULO
%include "guardar.inc"
        section .bss
hvl_txt     resb 200
gua_ruta    resb GUA_RUTA
gua_tmp     resb GUA_RUTA
gua_bak     resb GUA_RUTA
            alignb 4
gua_rl      resb RL_TAM
        section .text
