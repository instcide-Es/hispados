; =============================================================================
;  Hispa Ventanas - [PANTALLA] la resolución y [ICONOS] el tamaño de los
;  iconos: lo que piden Configuración › Resolución (CONFIG.HVP) y el resto
;  del entorno (docs/VENTANAS.md, «La pantalla» y «El tamaño de los iconos»)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Va en la «parte alta» de VENTANAS.COM. El modo se elige en modo real al
;  arrancar (video16.inc); aquí está lo de después:
;    aj_pan_leer   (aj_leer) AJ_PANTALLA, AJ_ICONOS, AJ_PRUEBA
;    aj_pan_poner  (aj_poner) AJ_REINICIAR, AJ_ICONOS
;    ico_talla_poner  EDX=0 pequeños, 1 normales, 2 grandes: lo aplica ya
;    dc_icono_t    EAX=x EBX=y ESI=icono: como dc_icono, del tamaño puesto
;  Pedido de Carlos (MacBook Air de 1440x900): «Necesitamos un gestor de
;  gráficos y otro de tamaño de iconos».
;
;  Cambiar de resolución no se hace en marcha: «Probar» vuelve a cargar
;  Hispa Ventanas (como al volver de un juego: CENTRO.INI guarda las ventanas
;  y los grupos) con « /MODO:AxBxC /PRUEBA» detrás de las opciones de
;  siempre, solo esa vez (lz_extra del lanzador). Con /PRUEBA, Configuración
;  pregunta «¿Se ve bien?» y, si nadie contesta en 15 segundos o se dice que
;  no, vuelve a cargar Hispa Ventanas sin nada de más: con el modo de antes,
;  que sigue en VENTANAS.INI. Así una pantalla que no ve el modo nuevo vuelve
;  sola a verse.
; =============================================================================

; los tamaños: el dibujo, de un icono al siguiente, de una fila a la
; siguiente, dónde empieza el dibujo y dónde el nombre (desde la casilla)
ico_tallas  dd 16, 96, 60, 8, 30            ; pequeños
            dd 32, 104, 76, 16, 58          ; normales (los de siempre)
            dd 48, 120, 100, 10, 66         ; grandes
ico_talla   dd 1
ico_px      dd 32
cen_paso    dd 104
cen_fila    dd 76
cen_icoy    dd 16
cen_etqy    dd 58

aj_pan_leer:
        cmp eax, AJ_PANTALLA
        jne .i
        mov eax, vl_n
        ret
.i:
        cmp eax, AJ_ICONOS
        jne .p
        mov eax, [ico_talla]
        ret
.p:
        movzx eax, byte [op_prueba]
        ret

aj_pan_poner:
        cmp eax, AJ_ICONOS
        jne .r
        call ico_talla_poner
        ret
.r:
        push esi
        mov esi, edx
        call pan_reiniciar
        pop esi
        ret

; EDX=0, 1 o 2: los iconos pequeños, normales o grandes, ya
ico_talla_poner:
        pushad
        cmp edx, 2
        jbe .a
        mov edx, 1
.a:
        mov [ico_talla], edx
        imul esi, edx, 20
        add esi, ico_tallas
        mov edi, ico_px
        mov ecx, 5
        rep movsd
        call bt_mi_tallas           ; el menú Inicio de Hispa-486
        call cen_alargar            ; el Centro, si tiene su medida de fábrica, a lo que haga falta
        call vistas_repintar        ; los grupos y el escritorio de Hispa-486
        popad
        ret

; ESI=las opciones de más (« /MODO:1440x900x8 /PRUEBA», 31 letras como
; mucho; 0: ninguna): vuelve a cargar Hispa Ventanas -> EAX=0 se hace, 1 no
; se puede (sin el lanzador), 2 el usuario no ha querido (cambios sin
; guardar: apa_consultar)
pan_reiniciar:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov [pan_duena], ebp
        mov eax, 1
        cmp word [lz_seg], 0
        je .x
        mov edi, pan_t_tit          ; ¿cambios sin guardar? [0.9.31] en el Bloc
        call apa_consultar          ; de notas y en cada programa
        mov eax, 2
        jc .x
.poner:
        movzx edx, word [lz_seg]    ; a la memoria del lanzador
        shl edx, 4
        add edx, [cero]
        lea edi, [edx+lz_extra]
        mov byte [edi], 0
        test esi, esi
        jz .o
        mov ecx, 31
        call cad_copiar
.o:
        mov byte [edx+lz_orden], 2
        mov byte [salir], 2
        xor eax, eax
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
pan_t_tit   db "Resolución de la pantalla", 0
pan_duena   dd 0

; EAX=x EBX=y ESI=icono (32x32 a 4 bits por punto): como dc_icono, del
; tamaño puesto (ico_px). Los grandes, ampliados (cada punto a 1 o 2, sin
; difuminar: el dibujo sigue nítido); los pequeños, de cada 2x2 el primer
; punto que no es transparente (si lo son tres de los cuatro, transparente)
dc_icono_t:
        cmp dword [ico_px], 32
        je dc_icono
        pushad
        call ico_decod
        mov ecx, [ico_px]
        mov edi, ico_tmp2
        cmp ecx, 16
        je .peq
        xor ebx, ebx                ; EBX=fila
.fy:
        mov eax, ebx
        shl eax, 5
        xor edx, edx
        div ecx
        shl eax, 5
        lea esi, [ico_tmp+eax]
        xor ebp, ebp                ; EBP=columna
.fx:
        mov eax, ebp
        shl eax, 5
        xor edx, edx
        div ecx
        mov al, [esi+eax]
        stosb
        inc ebp
        cmp ebp, ecx
        jb .fx
        inc ebx
        cmp ebx, ecx
        jb .fy
        jmp .pinta
.peq:
        mov esi, ico_tmp
        mov ebx, 16
.py:
        mov ecx, 16
.px:
        xor edx, edx                ; DL=transparentes, AL=el elegido
        mov al, CLAVE
        mov ah, [esi]
        call .uno
        mov ah, [esi+1]
        call .uno
        mov ah, [esi+32]
        call .uno
        mov ah, [esi+33]
        call .uno
        cmp dl, 3
        jb .pp
        mov al, CLAVE
.pp:
        stosb
        add esi, 2
        dec ecx
        jnz .px
        add esi, 32
        dec ebx
        jnz .py
.pinta:
        popad
        push ecx
        push edx
        push esi
        mov ecx, [ico_px]
        mov edx, ecx
        mov esi, ico_tmp2
        call dc_copiar_clave
        pop esi
        pop edx
        pop ecx
        ret
.uno:
        cmp ah, CLAVE
        jne .u1
        inc dl
        ret
.u1:
        cmp al, CLAVE
        jne .u2
        mov al, ah
.u2:
        ret
