; =============================================================================
;  Hispa Ventanas - CENTRO.INI: los grupos y los accesos directos del Centro
;  de programas, dónde está cada ventana y el volumen
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CENTRO.INI está en la carpeta de VENTANAS.COM. Es un archivo de texto
;  (página 437, líneas con CR+LF) con un formato propio, parecido a un .INI
;  (docs/VENTANAS.md, «CENTRO.INI»): una sección [Centro], y por cada grupo
;  una sección [Grupo] seguida de una sección [Acceso] por cada acceso
;  directo suyo. Si no existe, se crea con los grupos por omisión.
;
;    cfg_cargar     lee CENTRO.INI (o pone lo de omisión y lo crea)
;    cfg_guardar    lo escribe si ha cambiado algo (también dónde están las
;                   ventanas de los grupos y cuáles están abiertas)
;    gr_nuevo       -> EAX=grupo nuevo (0 si no caben más), EDX=su número
;    ac_nuevo       DL=número de grupo -> EAX=acceso nuevo (0 si no caben)
;    gr_num         EAX=grupo -> EAX=su número
;    env_valor      ESI="NOMBRE=" -> EAX=el valor en el entorno (0 si no está)
;    cad_igual_i    ESI, EDI: ZF=1 si son iguales sin mirar mayúsculas
;    lee_num        ESI=texto -> EAX=número (ESI avanza)
;    pon_num_s      EAX (con signo) en decimal en EDI, que avanza
; =============================================================================

MAX_GR      equ 12
GR_TAM      equ 64
GR_USO      equ 0
GR_ESTADO   equ 1                   ; 0 cerrado, 1 abierto, 2 minimizado, 3 maximizado
GR_BAND     equ 2                   ; [TAMAÑO] banderas del grupo:
GRB_MEDIDA  equ 1                   ;   el usuario le ha dado el tamaño (no se alarga sola)
GR_NOMBRE   equ 4                   ; 27 letras
GR_X        equ 32
GR_Y        equ 36
GR_AN       equ 40
GR_AL       equ 44
GR_VEN      equ 48                  ; su ventana (0: cerrado)
GR_SEL      equ 52                  ; el icono marcado
MAX_AC      equ 96
AC_TAM      equ 320
AC_USO      equ 0
AC_GRUPO    equ 1
AC_ICONO    equ 2
AC_BAND     equ 3
AC_NOMBRE   equ 4                   ; 31 letras
AC_PROG     equ 36                  ; 79
AC_DIR      equ 116                 ; 67
AC_PARAM    equ 184                 ; 71
AB_VOLVER   equ 1                   ; volver a Hispa Ventanas al terminar
AB_PAUSA    equ 2                   ; esperar una tecla al terminar
; opciones de juego y accesos de un grupo ligado a una carpeta (accesos.inc)
AC_VEL      equ 256                 ; velocidad: 0 normal, 1-99 %, VEL_NIV+1.. un procesador
AC_BAND2    equ 257
AB2_LOADFIX equ 1                   ; cargarlo con LOADFIX
AB2_SB      equ 2                   ; con la Sound Blaster emulada (HSPSND /SB)
AB2_AUTO    equ 4                   ; lo ha puesto Hispa Ventanas (carpeta del grupo)
AB2_MANO    equ 8                   ;   y el usuario lo ha cambiado después
AB2_OCULTO  equ 16                  ;   o lo ha borrado: no se enseña ni vuelve a salir
AB2_FALTA   equ 32                  ; (no se guarda) su programa ya no está
VEL_NIV     equ 100                 ; 101 Pentium, 102 486, 103 386, 104 286, 105 XT
VEL_NIVELES equ 5
JD_DIRTAM   equ 68                  ; la carpeta de un grupo: 67 letras
CFG_MAX     equ 49152
ICO_N       equ 12
T_CAD       equ 1
T_NUM       equ 2
T_OCT       equ 3
T_RECT      equ 4
T_BIT       equ 5
T_ICONO     equ 6
T_GDIR      equ 7                   ; (estos tres, en accesos.inc) carpeta del grupo
T_VEL       equ 8                   ; velocidad: «386», «50%»...
T_SON       equ 9                   ; sonido: «sb»

; los iconos que se pueden elegir: dibujo y nombre en CENTRO.INI
ico_tabla:
        dd ico_programa, k_i0
        dd ico_juego, k_i1
        dd ico_disco, k_i2
        dd ico_documento, k_i3
        dd ico_herramienta, k_i4
        dd ico_musica, k_i5
        dd ico_ordenes, k_i6
        dd ico_bloc, k_i7
        dd ico_calc, k_i8
        dd ico_reloj, k_i9
        dd ico_ventana, k_i10
        dd ico_carpeta, k_i11
k_i0        db "programa", 0
k_i1        db "juego", 0
k_i2        db "disco", 0
k_i3        db "documento", 0
k_i4        db "herramienta", 0
k_i5        db "musica", 0
k_i6        db "ordenes", 0
k_i7        db "bloc", 0
k_i8        db "calculadora", 0
k_i9        db "reloj", 0
k_i10       db "sistema", 0
k_i11       db "carpeta", 0

; las claves: sección, tipo, dato, 0; destino; nombre
cfg_claves:
        db 1, T_RECT, 0, 0
        dd cen_rect, k_ventana
        db 1, T_NUM, 0, 0
        dd vol_pct, k_volumen
        db 1, T_NUM, 0, 0
        dd vol_mudo, k_silencio
        db 1, T_NUM, 0, 0
        dd cen_activo, k_activo
        db 1, T_NUM, 0, 0
        dd hvp_vistos, k_modulos
        db 1, T_NUM, 0, 0
        dd jd_hecho, k_juegosdos
        db 1, T_CAD, 119, 0         ; [ARCHIVOS] el módulo que se abre al volver
        dd cen_reabrir, k_reabrir
        db 2, T_GDIR, 0, 0
        dd 0, k_carpeta
        db 2, T_CAD, 27, 0
        dd GR_NOMBRE, k_nombre
        db 2, T_RECT, 0, 0
        dd GR_X, k_ventana
        db 2, T_OCT, 0, 0
        dd GR_ESTADO, k_estado
        db 2, T_NUM, 0, 0
        dd GR_SEL, k_marcado
        db 2, T_BIT, GRB_MEDIDA, 0  ; [TAMAÑO]
        dd GR_BAND, k_medida
        db 3, T_CAD, 31, 0
        dd AC_NOMBRE, k_nombre
        db 3, T_ICONO, 0, 0
        dd AC_ICONO, k_icono
        db 3, T_CAD, 79, 0
        dd AC_PROG, k_programa
        db 3, T_CAD, 67, 0
        dd AC_DIR, k_carpeta
        db 3, T_CAD, 71, 0
        dd AC_PARAM, k_parametros
        db 3, T_BIT, AB_VOLVER, 0
        dd AC_BAND, k_volver
        db 3, T_BIT, AB_PAUSA, 0
        dd AC_BAND, k_pausa
        db 3, T_VEL, 0, 0
        dd AC_VEL, k_velocidad
        db 3, T_BIT, AB2_LOADFIX, 0
        dd AC_BAND2, k_loadfix
        db 3, T_SON, AB2_SB, 0
        dd AC_BAND2, k_sonido
        db 3, T_BIT, AB2_AUTO, 0
        dd AC_BAND2, k_auto
        db 3, T_BIT, AB2_MANO, 0
        dd AC_BAND2, k_cambiado
        db 3, T_BIT, AB2_OCULTO, 0
        dd AC_BAND2, k_oculto
        db 0
k_ventana   db "Ventana", 0
k_volumen   db "Volumen", 0
k_silencio  db "Silencio", 0
k_activo    db "Activo", 0
k_modulos   db "Modulos", 0
k_nombre    db "Nombre", 0
k_estado    db "Estado", 0
k_marcado   db "Marcado", 0
k_medida    db "Medida", 0          ; [TAMAÑO]
k_icono     db "Icono", 0
k_programa  db "Programa", 0
k_carpeta   db "Carpeta", 0
k_parametros db "Parametros", 0
k_volver    db "Volver", 0
k_pausa     db "Pausa", 0
s_centro    db "[Centro]", 0
s_grupo     db "[Grupo]", 0
s_acceso    db "[Acceso]", 0
s_nl        db 13, 10, 0
n_centro_ini db "CENTRO.INI", 0
s_ini_cab   db "; Hispa Ventanas - Centro de programas: grupos y accesos directos.", 13, 10
            db "; Lo escribe Hispa Ventanas; se puede cambiar a mano con Hispa Ventanas", 13, 10
            db "; cerrado (formato: docs/VENTANAS.md).", 13, 10, 13, 10, 0
s_comspec   db "COMSPEC=", 0

; lo de omisión: nombre, icono, archivo de la carpeta propia, banderas
def_principal:
        dd td_editar, 3, tn_editar, AB_VOLVER
        dd td_gestor, 2, tn_gestor, AB_VOLVER
        dd td_info, 4, tn_info, AB_VOLVER | AB_PAUSA
        dd 1                        ; [Configuración] (1: aquí, el intérprete de órdenes)
        dd td_config, 4, tn_config, AB_VOLVER   ; [Configuración] CONFIG.HVP
        dd 0
def_accesorios:
        dd t_bloc, 7, ti_bloc, AB_VOLVER
        dd t_calc, 8, ti_calc, AB_VOLVER
        dd t_reloj, 9, ti_reloj, AB_VOLVER
        dd tit_sistema, 10, ti_sistema, AB_VOLVER
        dd 0
tg_principal db "Principal", 0
tg_accesorios db "Accesorios", 0
tg_juegos   db "Juegos", 0
tg_sonido   db "Sonido", 0          ; [MEZCLADOR] el Reproductor y el Mezclador
td_editar   db "Editar", 0
td_gestor   db "Gestor", 0
td_info     db "Información", 0
td_ordenes  db "Hispa-DOS", 0
td_config   db "Configuración", 0   ; [Configuración]
tn_editar   db "EDITAR.COM", 0
tn_gestor   db "GESTOR.COM", 0
tn_info     db "INFO.COM", 0
ti_bloc     db ":BLOC", 0
ti_calc     db ":CALC", 0
ti_reloj    db ":RELOJ", 0
ti_sistema  db ":SISTEMA", 0

; ---- utilidades ----
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

lee_num:
        push ebx
        push ecx
        xor eax, eax
        xor ecx, ecx
.e:
        cmp byte [esi], ' '
        jne .s
        inc esi
        jmp .e
.s:
        cmp byte [esi], '-'
        jne .d
        inc ecx
        inc esi
.d:
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .f
        imul eax, 10
        add eax, ebx
        inc esi
        jmp .d
.f:
        test ecx, ecx
        jz .x
        neg eax
.x:
        pop ecx
        pop ebx
        ret

pon_num_s:
        test eax, eax
        jns pon_num
        push eax
        mov al, '-'
        stosb
        pop eax
        neg eax
        call pon_num
        neg eax
        ret

; copia ESI a EDI con el 0 final, hasta ECX letras
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

env_valor:
        push ebx
        push ecx
        push edx
        push edi
        movzx edi, word [env_seg]
        shl edi, 4
        add edi, [cero]
.v:
        cmp byte [edi], 0
        je .no
        xor ecx, ecx
.c:
        mov al, [esi+ecx]
        test al, al
        jz .hay
        cmp al, [edi+ecx]
        jne .otra
        inc ecx
        jmp .c
.otra:
        inc edi
        cmp byte [edi-1], 0
        jne .otra
        jmp .v
.hay:
        lea eax, [edi+ecx]
        jmp .x
.no:
        xor eax, eax
.x:
        pop edi
        pop edx
        pop ecx
        pop ebx
        ret

; dir_propia = la carpeta de VENTANAS.COM, acabada en «\»
dir_propia_poner:
        pushad
        movzx esi, word [env_seg]
        shl esi, 4
        add esi, [cero]
.e1:
        cmp byte [esi], 0
        je .e2
.s:
        lodsb
        test al, al
        jnz .s
        jmp .e1
.e2:
        add esi, 3
        mov edi, dir_propia
        mov edx, edi
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c2
        mov edx, edi
.c2:
        cmp edi, dir_propia + 78
        jae .f
        test al, al
        jnz .c
.f:
        mov byte [edx], 0
        popad
        ret

gr_nuevo:
        push ecx
        push edi
        mov eax, [gr_tabla]
        xor edx, edx
.b:
        cmp byte [eax+GR_USO], 0
        je .h
        add eax, GR_TAM
        inc edx
        cmp edx, MAX_GR
        jb .b
        xor eax, eax
        jmp .x
.h:
        push eax
        mov edi, eax
        mov ecx, GR_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov byte [eax+GR_USO], 1
        imul ecx, edx, JD_DIRTAM    ; sin carpeta de juegos
        mov byte [gr_dirs+ecx], 0
        imul ecx, edx, 24
        add ecx, 200
        mov [eax+GR_X], ecx
        sub ecx, 60
        mov [eax+GR_Y], ecx
        mov dword [eax+GR_AN], 452
        mov dword [eax+GR_AL], 190
.x:
        pop edi
        pop ecx
        ret

ac_nuevo:
        push ebx
        push ecx
        push edi
        mov eax, [ac_tabla]
        mov ebx, MAX_AC
.b:
        cmp byte [eax+AC_USO], 0
        je .h
        add eax, AC_TAM
        dec ebx
        jnz .b
        xor eax, eax
        jmp .x
.h:
        push eax
        mov edi, eax
        mov ecx, AC_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov byte [eax+AC_USO], 1
        mov [eax+AC_GRUPO], dl
        mov byte [eax+AC_BAND], AB_VOLVER
.x:
        pop edi
        pop ecx
        pop ebx
        ret

gr_num:
        sub eax, [gr_tabla]
        shr eax, 6
        ret

; ESI=nombre de icono -> EAX=su número (0 si no se conoce)
ico_por_nombre:
        push ecx
        push edi
        xor ecx, ecx
.b:
        mov edi, [ico_tabla+ecx*8+4]
        call cad_igual_i
        je .x
        inc ecx
        cmp ecx, ICO_N
        jb .b
        xor ecx, ecx
.x:
        mov eax, ecx
        pop edi
        pop ecx
        ret

; (interna) EAX = suma de comprobación de ECX octetos en ESI
cfg_sumar:
        push ecx
        push edx
        push esi
        xor eax, eax
        xor edx, edx
.b:
        test ecx, ecx
        jz .x
        mov dl, [esi]
        inc esi
        rol eax, 5
        add eax, edx
        dec ecx
        jmp .b
.x:
        pop esi
        pop edx
        pop ecx
        ret

; ---- leer ----
cfg_cargar:
        pushad
        mov ecx, MAX_GR * GR_TAM
        call mem_pedir
        mov [gr_tabla], eax
        mov ecx, MAX_AC * AC_TAM
        call mem_pedir
        mov [ac_tabla], eax
        mov ecx, CFG_MAX
        call mem_pedir
        mov [cfg_buf], eax
        mov dword [cen_rect], 36
        mov dword [cen_rect+4], 52
        mov dword [cen_rect+8], 452
        mov dword [cen_rect+12], 190
        mov dword [cen_activo], -1
        mov dword [vol_pct], -1     ; (no se sabe: no se toca la tarjeta)
        call dir_propia_poner
        mov edi, cfg_ruta
        mov esi, dir_propia
        call pon_cad
        mov esi, n_centro_ini
        call pon_cad
        mov byte [edi], 0
        mov esi, cfg_ruta
        xor al, al
        call dos_abrir
        jc .defecto
        mov ebx, eax
        mov edi, [cfg_buf]
        mov ecx, CFG_MAX - 2
        call dos_leer
        jnc .leido
        xor eax, eax
.leido:
        call dos_cerrar
        mov edi, [cfg_buf]
        mov byte [edi+eax], 0
        mov [cfg_len], eax
        mov ecx, eax
        mov esi, edi
        call cfg_sumar
        mov [cfg_suma], eax
        call cfg_leer
        mov eax, [gr_tabla]         ; sin ningún grupo no vale
        mov ecx, MAX_GR
.g:
        cmp byte [eax+GR_USO], 0
        jne .hvp
        add eax, GR_TAM
        dec ecx
        jnz .g
.defecto:
        call cfg_defecto
        mov dword [cfg_len], 0
.hvp:
        ; los programas de Hispa Ventanas (.HVP) de las carpetas JUEGOS y
        ; PROGS: si no son los mismos que la última vez, los que no tengan
        ; acceso directo entran en su grupo («Juegos» o «Accesorios»)
        xor ecx, ecx
        call hvp_las_dos
        cmp eax, [hvp_vistos]
        je .h2
        mov [hvp_vistos], eax
        inc ecx
        call hvp_las_dos
        mov dword [cfg_len], 0
.h2:
        ; los grupos ligados a una carpeta de juegos: los accesos de los
        ; juegos nuevos entran solos y salen los que ya no están
        call jd_arranque
        test eax, eax
        jz .h3
        mov dword [cfg_len], 0
.h3:
        cmp dword [cfg_len], 0
        jne .x
        call cfg_guardar
.x:
        popad
        ret

cfg_leer:
        pushad
        mov esi, [cfg_buf]
        mov dword [cfg_secc], 0
        mov dword [cfg_grupo], -1
.linea:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, ' '
        ja .l1
        inc esi
        jmp .linea
.l1:
        mov edi, esi
.l2:
        mov al, [edi]
        test al, al
        jz .l3
        cmp al, 13
        je .l3
        cmp al, 10
        je .l3
        inc edi
        jmp .l2
.l3:
        mov [cfg_sig], edi
        mov [cfg_term], al
        mov byte [edi], 0
.l4:
        cmp edi, esi
        jbe .l5
        cmp byte [edi-1], ' '
        ja .l5
        dec edi
        mov byte [edi], 0
        jmp .l4
.l5:
        cmp byte [esi], ';'
        je .sig
        cmp byte [esi], '['
        je .seccion
        call cfg_clave
.sig:
        mov esi, [cfg_sig]
        cmp byte [cfg_term], 0
        je .fin
        inc esi
        jmp .linea
.seccion:
        mov dword [cfg_secc], 0
        mov edi, s_centro
        call cad_igual_i
        jne .s2
        mov dword [cfg_secc], 1
        jmp .sig
.s2:
        mov edi, s_grupo
        call cad_igual_i
        jne .s3
        mov dword [cfg_grupo], -1
        call gr_nuevo
        test eax, eax
        jz .sig
        mov [cfg_grupo], edx
        mov [cfg_base], eax
        mov dword [cfg_secc], 2
        jmp .sig
.s3:
        mov edi, s_acceso
        call cad_igual_i
        jne .sig
        mov edx, [cfg_grupo]
        test edx, edx
        js .sig
        call ac_nuevo
        test eax, eax
        jz .sig
        mov [cfg_base], eax
        mov dword [cfg_secc], 3
        jmp .sig
.fin:
        mov eax, [vol_pct]          ; valores con sentido
        cmp eax, 100
        jle .v1
        mov dword [vol_pct], 100
.v1:
        cmp dword [vol_mudo], 0
        je .v2
        mov dword [vol_mudo], 1
.v2:
        popad
        ret

; (interna) una línea «clave=valor» (ESI) de la sección en curso
cfg_clave:
        pushad
        cmp dword [cfg_secc], 0
        je .x
        mov edi, esi
.b:
        mov al, [edi]
        test al, al
        jz .x
        cmp al, '='
        je .h
        inc edi
        jmp .b
.h:
        mov byte [edi], 0
        lea ebx, [edi+1]            ; el valor
.t1:
        cmp byte [ebx], ' '
        jne .t3
        inc ebx
        jmp .t1
.t3:
        cmp edi, esi
        jbe .t4
        cmp byte [edi-1], ' '
        jne .t4
        dec edi
        mov byte [edi], 0
        jmp .t3
.t4:
        mov edx, cfg_claves
.c:
        mov al, [edx]
        test al, al
        jz .x
        cmp al, [cfg_secc]
        jne .cs
        mov edi, [edx+8]
        call cad_igual_i
        je .hay
.cs:
        add edx, 12
        jmp .c
.hay:
        mov edi, [edx+4]
        cmp byte [cfg_secc], 1
        je .d
        add edi, [cfg_base]
.d:
        movzx ecx, byte [edx+2]
        mov al, [edx+1]
        mov esi, ebx
        cmp al, T_CAD
        jne .n1
        call cad_copiar
        jmp .x
.n1:
        cmp al, T_NUM
        jne .n2
        call lee_num
        mov [edi], eax
        jmp .x
.n2:
        cmp al, T_OCT
        jne .n3
        call lee_num
        cmp eax, 3
        jbe .o1
        mov eax, 1
.o1:
        mov [edi], al
        jmp .x
.n3:
        cmp al, T_RECT
        jne .n4
        mov ecx, 4
.r:
        call lee_num
        stosd
        cmp byte [esi], ','
        jne .r2
        inc esi
.r2:
        dec ecx
        jnz .r
        jmp .x
.n4:
        cmp al, T_BIT
        je .n4b
        cmp al, T_GDIR
        jb .n5
        call cfg_clave_alta
        jmp .x
.n4b:
        call lee_num
        test eax, eax
        jz .b0
        or [edi], cl
        jmp .x
.b0:
        not cl
        and [edi], cl
        jmp .x
.n5:
        call ico_por_nombre
        mov [edi], al
.x:
        popad
        ret

; ---- lo de omisión ----
cfg_defecto:
        pushad
        mov edi, [gr_tabla]         ; (por si había algo a medias)
        mov ecx, MAX_GR * GR_TAM / 4
        xor eax, eax
        rep stosd
        mov edi, [ac_tabla]
        mov ecx, MAX_AC * AC_TAM / 4
        rep stosd
        ; Principal: los programas de Hispa-DOS que haya en la carpeta propia
        call gr_nuevo
        mov dword [eax+GR_AL], 260  ; [Configuración] (dos filas de iconos)
        lea edi, [eax+GR_NOMBRE]
        mov esi, tg_principal
        mov ecx, 27
        call cad_copiar
        mov ebx, def_principal
.p:
        cmp dword [ebx], 1          ; [Configuración] 0: fin; 1: el intérprete
        jb .acc
        je .ordenes
        xor edx, edx
        call ac_nuevo
        mov ebp, eax
        lea edi, [ebp+AC_PROG]
        mov esi, dir_propia
        call pon_cad
        mov esi, [ebx+8]
        call pon_cad
        mov byte [edi], 0
        lea esi, [ebp+AC_PROG]
        xor al, al
        call dos_abrir
        jnc .p1
        mov byte [ebp+AC_USO], 0    ; no está: sin acceso
        jmp .p2
.p1:
        push ebx
        mov ebx, eax
        call dos_cerrar
        pop ebx
        call .datos
        mov eax, [dir_propia]       ; carpeta de trabajo: la raíz de su unidad
        and eax, 0x00FFFFFF
        mov [ebp+AC_DIR], eax
.p2:
        add ebx, 16
        jmp .p
.ordenes:
        mov esi, s_comspec          ; el intérprete de órdenes
        call env_valor
        test eax, eax
        jz .ofin
        mov esi, eax
        xor edx, edx
        call ac_nuevo
        mov ebp, eax
        lea edi, [ebp+AC_PROG]
        mov ecx, 79
        call cad_copiar
        lea edi, [ebp+AC_NOMBRE]
        mov esi, td_ordenes
        mov ecx, 31
        call cad_copiar
        mov byte [ebp+AC_ICONO], 6
        mov eax, [ebp+AC_PROG]
        and eax, 0x00FFFFFF
        mov [ebp+AC_DIR], eax
.ofin:
        add ebx, 4                  ; [Configuración] y sigue la lista
        jmp .p
.acc:
        call gr_nuevo
        lea edi, [eax+GR_NOMBRE]
        mov esi, tg_accesorios
        mov ecx, 27
        call cad_copiar
        mov ebx, def_accesorios
.a:
        cmp dword [ebx], 0
        je .juegos
        mov edx, 1
        call ac_nuevo
        mov ebp, eax
        lea edi, [ebp+AC_PROG]
        mov esi, [ebx+8]
        mov ecx, 79
        call cad_copiar
        call .datos
        add ebx, 16
        jmp .a
.juegos:
        call gr_nuevo
        lea edi, [eax+GR_NOMBRE]
        mov esi, tg_juegos
        mov ecx, 27
        call cad_copiar
        popad
        ret
; (interna) nombre, icono y banderas de la fila EBX al acceso EBP
.datos:
        lea edi, [ebp+AC_NOMBRE]
        mov esi, [ebx]
        mov ecx, 31
        call cad_copiar
        mov al, [ebx+4]
        mov [ebp+AC_ICONO], al
        mov al, [ebx+12]
        mov [ebp+AC_BAND], al
        ret

; ---- escribir ----
; (interna) EBP=ventana EDI=destino: su rectángulo sin maximizar -> AL=estado
ven_rect_normal:
        push esi
        lea esi, [ebp+V_X]
        test dword [ebp+V_ESTADO], S_MAXIM
        jz .c
        lea esi, [ebp+V_RX]
.c:
        movsd
        movsd
        movsd
        movsd
        pop esi
        mov al, 2
        test dword [ebp+V_ESTADO], S_MINIM
        jnz .x
        mov al, 3
        test dword [ebp+V_ESTADO], S_MAXIM
        jnz .x
        mov al, 1
.x:
        ret

; (interna) apunta dónde está cada ventana y cuál es la activa
cfg_capturar:
        pushad
        mov ebp, [cen_ven]
        test ebp, ebp
        jz .x                       ; (todavía no hay ventanas)
        mov edi, cen_rect
        call ven_rect_normal
        mov ebx, [gr_tabla]
        mov ecx, MAX_GR
        xor edx, edx                ; grupos en uso antes de este
        mov dword [cen_activo], -1
.g:
        cmp byte [ebx+GR_USO], 0
        je .s
        mov byte [ebx+GR_ESTADO], 0
        mov ebp, [ebx+GR_VEN]
        test ebp, ebp
        jz .s1
        lea edi, [ebx+GR_X]
        call ven_rect_normal
        mov [ebx+GR_ESTADO], al
        mov eax, [ebp+V_D0]
        mov [ebx+GR_SEL], eax
        cmp ebp, [activa]
        jne .s1
        mov [cen_activo], edx
.s1:
        inc edx
.s:
        add ebx, GR_TAM
        dec ecx
        jnz .g
.x:
        popad
        ret

; (internas) escriben en EDI: «clave=» (ESI), fin de línea, un rectángulo (ESI)
pon_clave:
        call pon_cad
        mov al, '='
        stosb
        ret
pon_nl:
        mov ax, 0x0A0D
        stosw
        ret
pon_rect:
        push ecx
        mov ecx, 4
.r:
        lodsd
        call pon_num_s
        mov al, ','
        stosb
        dec ecx
        jnz .r
        dec edi
        pop ecx
        jmp pon_nl
; «clave=número» y fin de línea: ESI=clave EAX=número
pon_clave_num:
        push eax
        call pon_clave
        pop eax
        call pon_num_s
        jmp pon_nl
; «clave=texto» y fin de línea: ESI=clave EDX=texto
pon_clave_cad:
        call pon_clave
        mov esi, edx
        call pon_cad
        jmp pon_nl

cfg_guardar:
        pushad
        cmp dword [cfg_buf], 0
        je .x
        call cfg_capturar
        mov edi, [cfg_buf]
        mov esi, s_ini_cab
        call pon_cad
        mov esi, s_centro
        call pon_cad
        call pon_nl
        mov esi, k_ventana
        call pon_clave
        mov esi, cen_rect
        call pon_rect
        mov eax, [vol_pct]
        test eax, eax
        js .sinvol
        mov esi, k_volumen
        call pon_clave_num
        mov esi, k_silencio
        mov eax, [vol_mudo]
        call pon_clave_num
.sinvol:
        mov esi, k_activo
        mov eax, [cen_activo]
        call pon_clave_num
        mov eax, [hvp_vistos]
        test eax, eax
        jz .sinmod
        mov esi, k_modulos
        call pon_clave_num
.sinmod:
        mov eax, [jd_hecho]
        test eax, eax
        jz .sinjd
        mov esi, k_juegosdos
        call pon_clave_num
.sinjd:
        cmp byte [cen_reabrir], 0   ; [ARCHIVOS] solo si hay algo que reabrir
        je .sinre
        mov esi, k_reabrir
        mov edx, cen_reabrir
        call pon_clave_cad
.sinre:
        mov ebx, [gr_tabla]
        xor ecx, ecx
.g:
        cmp byte [ebx+GR_USO], 0
        je .gs
        call pon_nl
        mov esi, s_grupo
        call pon_cad
        call pon_nl
        mov esi, k_nombre
        lea edx, [ebx+GR_NOMBRE]
        call pon_clave_cad
        mov esi, k_ventana
        call pon_clave
        lea esi, [ebx+GR_X]
        call pon_rect
        mov esi, k_estado
        movzx eax, byte [ebx+GR_ESTADO]
        call pon_clave_num
        mov esi, k_marcado
        mov eax, [ebx+GR_SEL]
        call pon_clave_num
        test byte [ebx+GR_BAND], GRB_MEDIDA ; [TAMAÑO] solo si se la ha dado
        jz .sinmedida
        mov esi, k_medida
        mov eax, 1
        call pon_clave_num
.sinmedida:
        imul edx, ecx, JD_DIRTAM    ; su carpeta de juegos, si la tiene
        add edx, gr_dirs
        cmp byte [edx], 0
        je .sindir
        mov esi, k_carpeta
        call pon_clave_cad
.sindir:
        mov ebp, [ac_tabla]
        mov edx, MAX_AC
.a:
        cmp byte [ebp+AC_USO], 0
        je .as
        cmp [ebp+AC_GRUPO], cl
        jne .as
        push edx
        call pon_nl
        mov esi, s_acceso
        call pon_cad
        call pon_nl
        mov esi, k_nombre
        lea edx, [ebp+AC_NOMBRE]
        call pon_clave_cad
        movzx eax, byte [ebp+AC_ICONO]
        cmp eax, ICO_N
        jb .i
        xor eax, eax
.i:
        mov edx, [ico_tabla+eax*8+4]
        mov esi, k_icono
        call pon_clave_cad
        mov esi, k_programa
        lea edx, [ebp+AC_PROG]
        call pon_clave_cad
        mov esi, k_carpeta
        lea edx, [ebp+AC_DIR]
        call pon_clave_cad
        mov esi, k_parametros
        lea edx, [ebp+AC_PARAM]
        call pon_clave_cad
        mov esi, k_volver
        movzx eax, byte [ebp+AC_BAND]
        and eax, AB_VOLVER
        call pon_clave_num
        mov esi, k_pausa
        movzx eax, byte [ebp+AC_BAND]
        shr eax, 1
        and eax, 1
        call pon_clave_num
        call cfg_acc_juego          ; velocidad, LOADFIX... si no son lo normal
        pop edx
.as:
        add ebp, AC_TAM
        dec edx
        jnz .a
.gs:
        add ebx, GR_TAM
        inc ecx
        cmp ecx, MAX_GR
        jb .g
        ; ¿ha cambiado? Solo entonces se escribe
        mov esi, [cfg_buf]
        mov ecx, edi
        sub ecx, esi
        call cfg_sumar
        cmp ecx, [cfg_len]
        jne .escribe
        cmp eax, [cfg_suma]
        je .x
.escribe:
        mov edx, eax
        push esi
        mov esi, cfg_ruta
        call dos_crear
        pop esi
        jc .x
        mov ebx, eax
        call dos_escribir
        pushf
        call dos_cerrar
        popf
        jc .x
        mov [cfg_len], ecx
        mov [cfg_suma], edx
.x:
        popad
        ret
