; =============================================================================
;  HVPRUN - lo que el gestor, los menús y los diálogos de Ventanas esperan
;  del resto de Hispa Ventanas (el escritorio, el Centro, la barra de tareas,
;  el sonido...), que aquí no está: lo que no hace falta no hace nada
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

; ---- cadenas (escritorio.inc y config.inc) ----
; copia la cadena ESI (acabada en 0) a EDI, que avanza
pon_cad:
        lodsb
        test al, al
        jz .x
        stosb
        jmp pon_cad
.x:
        ret

; ESI, EDI: ZF=1 si son iguales (sin mirar mayúsculas)
cad_igual_i:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        mov ah, [edi]
        inc esi
        inc edi
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 0x20
.c1:
        cmp ah, 'a'
        jb .c2
        cmp ah, 'z'
        ja .c2
        sub ah, 0x20
.c2:
        cmp al, ah
        jne .x
        test al, al
        jnz .c
.x:
        pop edi
        pop esi
        pop eax
        ret

; copia ESI a EDI, ECX letras como mucho, y el 0
cad_copiar:
        push eax
        push ecx
        push esi
        push edi
.c:
        test ecx, ecx
        jz .f
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jmp .c
.f:
        mov byte [edi], 0
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; ---- la barra de tareas (barra.inc): no hay ----
; el área de trabajo (maximizar): la pantalla entera; la principal, con su
; marco fuera (como está siempre)
bt_area:
        cmp ebp, [pan_ppal]
        je .ppal
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        ret
.ppal:
        mov eax, -PPAL_DX
        mov ebx, -PPAL_DY
        mov ecx, [vid_ancho]
        add ecx, PPAL_AN
        mov edx, [vid_alto]
        add edx, PPAL_AL
        ret
bt_orden:
bt_clic_fuera:
        ret
bt_tecla:                           ; (CF=0: la tecla no es del menú Inicio)
        clc
        ret

; ---- el escritorio, el Centro y lo demás ----
esc_inv_iconos:
ev_act:
tar_latido:
cfg_guardar:
vistas_repintar:
vista_propiedades:
w32_menu_abierto:
        ret
esc_minimizada:                     ; (no hay ventanas minimizadas)
vista_en:
vista_sel:
        stc
        ret
vista_cols:
vista_ficha:
        xor eax, eax
        ret
apa_pedir:                          ; (Alt+F4 sin ventana: nada)
hvp_abrir:
arch_abrir:
tar_abrir:
mez_abrir:
        ret
cen_paso    dd 104
cen_fila    dd 76
cen_etqy    dd 58
tn_config   db "CONFIG.HVP", 0
tj_dc_nombre db "Entre en la carpeta (doble clic) y pulse Aceptar.", 0

; ---- lo de la interfaz que aquí no hay (CF=1: no se puede) ----
aj_leer:
        xor eax, eax
        ret
aj_poner:
esc_fondo:
tec_suscribir:
        ret
snd_pcm:
snd_pcm2:
snd_orden:
snd_mezcla:
prog_abrir:
tar_activar:
tar_cerrar:
esc_imagen:
        stc
        ret
snd_info:
        or eax, -1
        ret
snd_campana:
tar_reiniciar:
        ret
tar_lista:
        xor eax, eax
        ret
; -> EAX=total EBX=libre ECX=mayor bloque EDX=módulos ESI=DOS EDI=montón
mem_estado:
        mov eax, MONTON_TAM
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        ret
; elegir de una lista: aquí, ninguno
dlg_lista:
        or eax, -1
        ret
