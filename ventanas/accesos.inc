; =============================================================================
;  Hispa Ventanas - juegos de DOS en el Centro de programas: grupos ligados a
;  una carpeta de juegos y opciones de juego de un acceso directo (velocidad,
;  LOADFIX y Sound Blaster emulada)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Este archivo va en la «parte alta» de VENTANAS.COM (ventanas.asm): solo
;  código y datos de 32 bits.
;
;  Un grupo puede estar ligado a una carpeta (C:\JUEGOS): al arrancar y al
;  abrir el grupo se le añade un acceso directo por cada juego nuevo de la
;  carpeta (cada subcarpeta, con las reglas de «Buscar programas...», y cada
;  programa suelto) y se quitan los de los juegos que ya no están. Solo se
;  quitan los que puso Hispa Ventanas (AB2_AUTO) y nadie ha tocado: uno
;  cambiado a mano en «Propiedades» (AB2_MANO) se queda, con un aspa, y uno
;  creado a mano no se mira. El que se borra a mano se queda oculto
;  (AB2_OCULTO) mientras su juego siga en la carpeta, para que no vuelva.
;
;    jd_arranque     (cfg_cargar) crea el grupo «Juegos de DOS» la primera vez
;                    que hay carpeta \JUEGOS en el disco duro y pone al día
;                    todos los grupos ligados -> EAX=cambios
;    jd_sinc         EBX=grupo: lo pone al día con su carpeta -> EAX=cambios
;    jd_sinc_abrir   (grupo_abrir) lo mismo, y guarda CENTRO.INI
;    jd_carpeta_dlg  EBP=vista: «Carpeta de juegos...»
;    jd_borrar_acc   EBX=acceso: lo borra (o lo oculta)
;    jd_marca_falta  (vista_pintar) el aspa del acceso cuyo programa falta
;    jd_elegir       (bp_carpeta) EDX=clase -> ECX=el candidato mayor
;    jd_ad_inicio, jd_ad_validar, jd_ad_guardar   (ad_gancho) las opciones de
;                    juego del diálogo del acceso EDI
;    jd_componer     (acc_lanzar) EBX=acceso: pone VELOCIDAD y LOADFIX delante
;                    de la orden que se deja al lanzador -> CF=1 y ESI=aviso
;    jd_tras_volver  al volver de un juego: lo que diga el lanzador de VELOCIDAD
;    cfg_clave_alta, cfg_acc_juego   (config.inc) leer y escribir las claves
;    dlg_carpeta     EBP=dueña -> EAX=1 y la carpeta elegida en cen_txt
; =============================================================================

; ---- textos ----
tj_grupo    db "Juegos de DOS", 0
k_velocidad db "Velocidad", 0
k_loadfix   db "Loadfix", 0
k_sonido    db "Sonido", 0
k_auto      db "Auto", 0
k_cambiado  db "Cambiado", 0
k_oculto    db "Oculto", 0
k_juegosdos db "JuegosDOS", 0
tk_sb       db "sb", 0
; los niveles de velocidad: como se escriben en CENTRO.INI, en el diálogo y
; en la orden VELOCIDAD (AC_VEL = VEL_NIV + lugar en la tabla)
vel_nombres dd tv_normal, tv_pentium, tv_486, tv_386, tv_286, tv_xt
tv_normal   db "Normal", 0
tv_pentium  db "Pentium", 0
tv_486      db "486", 0
tv_386      db "386", 0
tv_286      db "286", 0
tv_xt       db "XT", 0
tn_velocida db "VELOCIDA.COM", 0
tn_loadfix  db "LOADFIX.COM", 0
tn_hspsnd   db "HSPSND.COM", 0
tj_ad_juego db "Opciones para juegos de DOS", 0
tj_ad_vel   db "Velocidad:", 0
tj_ad_otra  db "Otra:", 0
tj_ad_pct   db "% de la velocidad normal", 0
tj_ad_loadfix db "LOADFIX: cargarlo por encima de los primeros 64 KB", 0
tj_ad_sb    db "Sonido: Sound Blaster emulada (HSPSND /SB)", 0
tj_ad_sb_no db "Sonido: Sound Blaster emulada (hace falta HSPEMM)", 0
tj_pct_mal  db "La velocidad tiene que ser un tanto por ciento", 10
            db "entre 1 y 100.", 0
tj_no_vel   db "Falta VELOCIDA.COM en la carpeta de Hispa Ventanas:", 10
            db "hace falta para abrir este acceso directo con la", 10
            db "velocidad que tiene puesta. Cópielo ahí (está en el", 10
            db "disco de Hispa-DOS, en \HISPADOS) o ponga la velocidad", 10
            db "«Normal» en Archivo > Propiedades.", 0
tj_no_lf    db "Falta LOADFIX.COM en la carpeta de Hispa Ventanas:", 10
            db "hace falta para abrir este acceso directo. Cópielo ahí", 10
            db "(está en el disco de Hispa-DOS, en \HISPADOS) o quite", 10
            db "la casilla LOADFIX en Archivo > Propiedades.", 0
tj_larga    db "La orden no cabe: con la velocidad y LOADFIX pasa de", 10
            db "126 letras. Acorte los parámetros o deje vacía la", 10
            db "carpeta de trabajo en Archivo > Propiedades.", 0
tj_sb_sin   db "HSPEMM no está cargado: el juego se abre sin la", 10
            db "Sound Blaster emulada.", 0
tj_sb_falta db "Falta HSPSND.COM en la carpeta de Hispa Ventanas:", 10
            db "el juego se abre sin la Sound Blaster emulada.", 0
tj_vel_tit  db "Velocidad", 0
tj_vel_frena db "No se ha podido volver a la velocidad normal:", 10
            db "VELOCIDAD sigue frenando el equipo.", 10
            db "Salga a Hispa-DOS y ejecute VELOCIDAD NORMAL,", 10
            db "o reinicie el equipo.", 0
tj_vel_queda db "El equipo ha vuelto a su velocidad normal, pero", 10
            db "VELOCIDAD no ha podido soltar el temporizador", 10
            db "(INT 08h): el juego ha dejado otro programa", 10
            db "residente detrás. VELOCIDAD sigue en la memoria,", 10
            db "sin frenar, hasta que se reinicie el equipo.", 0
tj_cd_tit   db "Carpeta de juegos", 0
tj_cd_txt   db "Hispa Ventanas pone en este grupo un acceso directo", 10
            db "por cada juego que haya en la carpeta, y lo quita si", 10
            db "se borra el juego. Carpeta de los juegos de DOS:", 0
tj_cd_nota  db "Si se deja vacía, el grupo deja de estar ligado a una", 10
            db "carpeta (los accesos directos que tiene se quedan).", 0
tj_cd_noesta db "No se encuentra esa carpeta.", 0
tj_cd_hecho db "Juegos nuevos de la carpeta: ", 0
tj_dc_tit   db "Elegir la carpeta", 0
tj_dc_nombre db "Entre en la carpeta (doble clic) y pulse Aceptar:", 0

; el diálogo «Carpeta de juegos»
jc_ventana:
        PLANTILLA c_dialogo, tj_cd_tit, E_DIALOGO, 0, 0, 460, 226, 0
jc_hijos:
        PLANTILLA c_estatico, tj_cd_txt, E_HIJA, 16, 12, 428, 56, 100
        PLANTILLA c_edit, 0, E_HIJA | E_TABULA, 16, 74, 304, 24, 101
        PLANTILLA c_boton, t_ad_examinar, E_HIJA | E_TABULA, 328, 73, 96, 26, 120
        PLANTILLA c_estatico, tj_cd_nota, E_HIJA, 16, 106, 428, 38, 102
        PLANTILLA c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 142, 156, 80, 26, ID_ACEPTAR
        PLANTILLA c_boton, t_cancelar, E_HIJA | E_TABULA, 234, 156, 80, 26, ID_CANCELAR
        dd 0

; ---- utilidades ----
; EAX=grupo -> EAX=su carpeta de juegos (una cadena; vacía: no tiene)
jd_gdir:
        call gr_num
        imul eax, JD_DIRTAM
        add eax, gr_dirs
        ret

; ESI=carpeta («C:\JUEGOS», «C:\») -> CF=1 si no está o no se puede leer
jd_existe_dir:
        pushad
        mov edi, cen_txt
        call pon_cad
        cmp byte [edi-1], '\'
        jne .a
        dec edi
.a:
        mov esi, t_bp_todo
        call pon_cad
        mov byte [edi], 0
        mov esi, cen_txt
        mov ecx, 0x10
        call dos_primero
        popad
        ret

; la carpeta de juegos que se propone -> cen_arch; CF=1 si no existe ninguna:
; \JUEGOS de la unidad de Hispa Ventanas si es un disco duro, o C:\JUEGOS
jd_defecto:
        pushad
        mov esi, t_bp_defecto
        mov edi, cen_arch
        mov ecx, 63
        call cad_copiar
        cmp byte [dir_propia+1], ':'
        jne .c
        mov al, [dir_propia]
        and al, 0xDF
        cmp al, 'C'
        jbe .c
        mov [cen_arch], al
        mov esi, cen_arch
        call jd_existe_dir
        jnc .x
        mov byte [cen_arch], 'C'
.c:
        mov esi, cen_arch
        call jd_existe_dir
.x:
        popad
        ret

; ZF=1 si hay HSPEMM (un gestor de memoria expandida en la INT 67h)
jd_hay_emm:
        push eax
        push ebx
        mov eax, [cero]
        movzx ebx, word [eax+0x67*4+2]
        shl ebx, 4
        add ebx, eax
        mov eax, [ebx+10]           ; «EMMXXXX0» (o «EMMQXXX0», sin EMS)
        and eax, 0x00FFFFFF
        cmp eax, 'EMM'
        jne .x
        cmp dword [ebx+14], 'XXX0'
.x:
        pop ebx
        pop eax
        ret

; EAX=velocidad (AC_VEL, no 0) -> su texto en EDI, que avanza: «386», «50%»
vel_texto:
        push esi
        cmp eax, VEL_NIV
        ja .n
        call pon_num
        mov al, '%'
        stosb
        jmp .x
.n:
        sub eax, VEL_NIV
        cmp eax, VEL_NIVELES
        jbe .n1
        xor eax, eax
.n1:
        mov esi, [vel_nombres+eax*4]
        call pon_cad
.x:
        pop esi
        ret

; ESI=texto («386», «50%», «normal») -> AL=velocidad (AC_VEL)
vel_leer:
        push ecx
        push edi
        xor ecx, ecx
.b:
        mov edi, [vel_nombres+ecx*4]
        call cad_igual_i
        je .hay
        inc ecx
        cmp ecx, VEL_NIVELES
        jbe .b
        call lee_num
        cmp eax, 99
        jbe .x
        xor eax, eax
        jmp .x
.hay:
        mov eax, ecx
        test eax, eax
        jz .x
        add eax, VEL_NIV
.x:
        pop edi
        pop ecx
        ret

; ---- CENTRO.INI ----
; (cfg_clave) las claves de los tipos T_GDIR, T_VEL y T_SON: AL=tipo
; EDI=destino CL=dato ESI=valor
cfg_clave_alta:
        cmp al, T_GDIR
        jne .v
        mov edi, [cfg_grupo]
        imul edi, JD_DIRTAM
        add edi, gr_dirs
        mov ecx, JD_DIRTAM - 1
        jmp cad_copiar
.v:
        cmp al, T_VEL
        jne .s
        call vel_leer
        mov [edi], al
        ret
.s:
        push edi
        mov edi, tk_sb
        call cad_igual_i
        pop edi
        jne .s0
        or [edi], cl
        ret
.s0:
        not cl
        and [edi], cl
        ret

; (cfg_guardar) las opciones de juego del acceso EBP, en EDI: solo las que no
; son lo normal (un CENTRO.INI sin juegos queda como siempre)
cfg_acc_juego:
        push eax
        push edx
        push esi
        movzx eax, byte [ebp+AC_VEL]
        test eax, eax
        jz .l
        mov esi, k_velocidad
        call pon_clave
        movzx eax, byte [ebp+AC_VEL]
        call vel_texto
        call pon_nl
.l:
        mov esi, k_loadfix
        mov dl, AB2_LOADFIX
        call .bit
        test byte [ebp+AC_BAND2], AB2_SB
        jz .a
        mov esi, k_sonido
        mov edx, tk_sb
        call pon_clave_cad
.a:
        mov esi, k_auto
        mov dl, AB2_AUTO
        call .bit
        mov esi, k_cambiado
        mov dl, AB2_MANO
        call .bit
        mov esi, k_oculto
        mov dl, AB2_OCULTO
        call .bit
        pop esi
        pop edx
        pop eax
        ret
.bit:
        test [ebp+AC_BAND2], dl
        jz .b0
        mov eax, 1
        call pon_clave_num
.b0:
        ret

; ---- los grupos ligados a una carpeta ----
jd_arranque:
        pushad
        xor ebp, ebp
        mov byte [jd_arr], 1
        cmp dword [jd_hecho], 0
        jne .sinc
        mov ebx, [gr_tabla]         ; ¿ya hay algún grupo ligado?
        xor ecx, ecx
.g:
        cmp byte [ebx+GR_USO], 0
        je .gs
        imul eax, ecx, JD_DIRTAM
        cmp byte [gr_dirs+eax], 0
        jne .marca
.gs:
        add ebx, GR_TAM
        inc ecx
        cmp ecx, MAX_GR
        jb .g
        call jd_defecto             ; ¿hay carpeta de juegos? El grupo, la primera vez
        jc .sinc
        call gr_nuevo
        test eax, eax
        jz .sinc
        lea edi, [eax+GR_NOMBRE]
        mov esi, tj_grupo
        mov ecx, 27
        call cad_copiar
        imul edi, edx, JD_DIRTAM
        add edi, gr_dirs
        mov esi, cen_arch
        mov ecx, JD_DIRTAM - 1
        call cad_copiar
        inc ebp
.marca:
        mov dword [jd_hecho], 1
.sinc:
        mov ebx, [gr_tabla]
        mov ecx, MAX_GR
.s:
        cmp byte [ebx+GR_USO], 0
        je .ss
        call jd_sinc
        add ebp, eax
.ss:
        add ebx, GR_TAM
        dec ecx
        jnz .s
        mov [esp+28], ebp
        popad
        ret

; (grupo_abrir) EBX=grupo que se abre
jd_sinc_abrir:
        cmp byte [jd_arr], 0
        jne .x
        push eax
        call jd_sinc
        test eax, eax
        jz .x0
        call cfg_guardar
.x0:
        pop eax
.x:
        ret

; (interna) ZF=1 si el nombre de archivo ESI acaba en .EXE, .COM o .BAT
jd_es_prog:
        push eax
        push esi
.b:
        lodsb
        test al, al
        jz .no
        cmp al, '.'
        jne .b
        mov eax, [esi]
        and eax, 0x00FFFFFF
        cmp eax, 'EXE'
        je .x
        cmp eax, 'COM'
        je .x
        cmp eax, 'BAT'
        jmp .x
.no:
        or esi, esi                 ; (ESI no es 0: ZF=0)
.x:
        pop esi
        pop eax
        ret

; (interna) ZF=1 si algún acceso (también los ocultos) abre el programa cen_txt
jd_ya_prog:
        push eax
        push ecx
        push esi
        push edi
        mov eax, [ac_tabla]
        mov ecx, MAX_AC
.r:
        cmp byte [eax+AC_USO], 0
        je .rs
        lea esi, [eax+AC_PROG]
        mov edi, cen_txt
        call cad_igual_i
        je .x
.rs:
        add eax, AC_TAM
        dec ecx
        jnz .r
        or eax, eax                 ; (no es 0: ZF=0)
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; (interna) ZF=1 si algún acceso abre un programa de la subcarpeta [bp_sub]
; de bp_dir (ese juego ya está: no hay que mirar dentro)
jd_cubierta:
        pushad
        call bp_ruta
        mov al, '\'
        stosb
        mov byte [edi], 0
        mov ebx, [ac_tabla]
        mov ecx, MAX_AC
.r:
        cmp byte [ebx+AC_USO], 0
        je .rs
        lea esi, [ebx+AC_PROG]
        mov edi, cen_txt
.c:
        mov ah, [edi]
        test ah, ah
        jz .x                       ; empieza igual: ZF=1
        mov al, [esi]
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
        je .c
.rs:
        add ebx, AC_TAM
        dec ecx
        jnz .r
        or ebx, ebx                 ; (no es 0: ZF=0)
.x:
        popad
        ret

jd_sinc:
        pushad
        mov dword [esp+28], 0
        mov eax, ebx
        call jd_gdir
        cmp byte [eax], 0
        je .x
        mov ebp, eax
        mov esi, bp_dir             ; (bp_dir es también lo último que se
        mov edi, jd_dir0            ; escribió en «Buscar programas...»: se deja
        mov ecx, 16                 ; como estaba)
        rep movsd
        mov esi, ebp
        mov edi, bp_dir
        mov ecx, 63
        call cad_copiar
        mov esi, bp_dir
        call cad_largo
        test eax, eax
        jz .fin
        cmp byte [bp_dir+eax-1], '\'
        jne .a
        mov byte [bp_dir+eax-1], 0
.a:
        ; lo que hay en la carpeta: subcarpetas y programas sueltos
        mov edi, cen_txt
        mov esi, bp_dir
        call pon_cad
        mov esi, t_bp_todo
        call pon_cad
        mov byte [edi], 0
        xor ebx, ebx
        mov dword [jd_nsuel], 0
        mov esi, cen_txt
        mov ecx, 0x10
        call dos_primero
        jc .fin                     ; no está o no se puede leer: no se toca nada
.d:
        jc .df
        mov esi, dos_dta + 0x1E
        test byte [dos_dta+0x15], 0x10
        jz .arch
        cmp byte [esi], '.'
        je .ds
        cmp ebx, BP_MAXDIR
        jae .ds
        imul edi, ebx, 13
        add edi, bp_dirs
        mov ecx, 12
        call cad_copiar
        inc ebx
        jmp .ds
.arch:
        test byte [dos_dta+0x15], 0x08
        jnz .ds
        call jd_es_prog
        jne .ds
        call bp_excluido
        je .ds
        mov eax, [jd_nsuel]
        cmp eax, JD_MAXSUEL
        jae .ds
        imul edi, eax, 13
        add edi, jd_sueltos
        mov ecx, 12
        call cad_copiar
        inc dword [jd_nsuel]
.ds:
        call dos_siguiente
        jmp .d
.df:
        mov [bp_ndir], ebx
        mov eax, [esp+16]           ; el grupo
        call gr_num
        mov [bp_grupo], eax
        ; 1) los que puso Hispa Ventanas y ya no están: fuera (o, si se
        ;    cambiaron a mano, con el aspa)
        mov dword [jd_quita], 0
        mov ebx, [ac_tabla]
        mov ecx, MAX_AC
.q:
        cmp byte [ebx+AC_USO], 0
        je .qs
        mov al, [bp_grupo]
        cmp [ebx+AC_GRUPO], al
        jne .qs
        test byte [ebx+AC_BAND2], AB2_AUTO
        jz .qs
        lea esi, [ebx+AC_PROG]
        xor al, al
        push ecx
        call dos_abrir
        pop ecx
        jc .falta
        push ebx
        push ecx
        mov ebx, eax
        call dos_cerrar
        pop ecx
        pop ebx
        and byte [ebx+AC_BAND2], ~AB2_FALTA
        jmp .qs
.falta:
        mov al, [ebx+AC_BAND2]
        and al, AB2_MANO | AB2_OCULTO
        cmp al, AB2_MANO
        jne .borra
        or byte [ebx+AC_BAND2], AB2_FALTA
        jmp .qs
.borra:
        mov byte [ebx+AC_USO], 0
        inc dword [jd_quita]
.qs:
        add ebx, AC_TAM
        dec ecx
        jnz .q
        ; 2) los juegos nuevos: las subcarpetas que no tienen acceso...
        mov byte [bp_auto], 1
        mov dword [bp_nuevos], 0
        xor ebx, ebx
.c:
        cmp ebx, [bp_ndir]
        jae .s
        imul esi, ebx, 13
        add esi, bp_dirs
        mov [bp_sub], esi
        call jd_cubierta
        je .cs
        call bp_carpeta
.cs:
        inc ebx
        jmp .c
.s:
        ; ...y los programas sueltos
        xor ebx, ebx
.s1:
        cmp ebx, [jd_nsuel]
        jae .s9
        mov edi, cen_txt
        mov esi, bp_dir
        call pon_cad
        mov al, '\'
        stosb
        imul esi, ebx, 13
        add esi, jd_sueltos
        call pon_cad
        mov byte [edi], 0
        call jd_ya_prog
        je .s2
        mov edx, [bp_grupo]
        call ac_nuevo
        test eax, eax
        jz .s9
        mov ebp, eax
        lea edi, [ebp+AC_PROG]
        mov esi, cen_txt
        mov ecx, 79
        call cad_copiar
        lea edi, [ebp+AC_DIR]
        call dir_de_ruta
        lea edi, [ebp+AC_NOMBRE]
        call nombre_de_ruta
        mov byte [ebp+AC_ICONO], 1
        mov byte [ebp+AC_BAND2], AB2_AUTO
        inc dword [bp_nuevos]
.s2:
        inc ebx
        jmp .s1
.s9:
        mov byte [bp_auto], 0
        mov eax, [bp_nuevos]
        add eax, [jd_quita]
        mov [esp+28], eax
.fin:
        mov esi, jd_dir0
        mov edi, bp_dir
        mov ecx, 16
        rep movsd
.x:
        popad
        ret

; (bp_carpeta, sin preguntar) EDX=clase (0 .EXE, 1 .COM, 2 .BAT) de la que
; hay varios candidatos -> ECX=el de mayor tamaño
jd_elegir:
        push eax
        push ebx
        push esi
        push edi
        xor ebx, ebx
        or ecx, -1
        xor edi, edi
.b:
        cmp ebx, [bp_n]
        jae .x
        cmp [cen_mapa+ebx], dl
        jne .s
        imul esi, ebx, 13
        add esi, cen_lista
        call bp_excluido
        je .s
        mov eax, [jd_tam+ebx*4]
        cmp ecx, -1
        je .t
        cmp eax, edi
        jbe .s
.t:
        mov edi, eax
        mov ecx, ebx
.s:
        inc ebx
        jmp .b
.x:
        pop edi
        pop esi
        pop ebx
        pop eax
        ret

; borra el acceso EBX. Si lo puso la carpeta de su grupo y el juego sigue
; ahí, se queda oculto: borrado del todo volvería a salir solo
jd_borrar_acc:
        mov al, [ebx+AC_BAND2]
        and al, AB2_AUTO | AB2_FALTA
        cmp al, AB2_AUTO
        jne .b
        movzx eax, byte [ebx+AC_GRUPO]
        imul eax, JD_DIRTAM
        cmp byte [gr_dirs+eax], 0
        je .b
        or byte [ebx+AC_BAND2], AB2_OCULTO
        ret
.b:
        mov byte [ebx+AC_USO], 0
        ret

; (vista_pintar) EAX, EBX = dónde está el icono del acceso [vis_ficha_p] en
; la vista EBP: si su programa ya no está, un aspa granate encima
jd_marca_falta:
        pushad
        cmp dword [ebp+V_ID], 0
        je .x
        mov esi, [vis_ficha_p]
        test byte [esi+AC_BAND2], AB2_FALTA
        jz .x
        COLOR C_GRANATE
        add eax, 3
        add ebx, 3
        mov edi, 3                  ; tres rayas juntas por brazo
.l:
        pushad
        lea ecx, [eax+24]
        lea edx, [ebx+24]
        call dc_linea
        popad
        pushad
        mov ecx, eax
        add eax, 24
        lea edx, [ebx+24]
        call dc_linea
        popad
        inc eax
        dec edi
        jnz .l
.x:
        popad
        ret

; ---- «Carpeta de juegos...» ----
jd_carpeta_dlg:
        pushad
        call vista_grupo
        test eax, eax
        js .x
        mov ebx, eax
        imul esi, ebx, JD_DIRTAM    ; la que tiene, o la que se propone
        add esi, gr_dirs
        cmp byte [esi], 0
        jne .ini
        call jd_defecto
        mov esi, cen_arch
.ini:
        mov edi, jd_buf
        mov ecx, JD_DIRTAM - 1
        call cad_copiar
        push ebx
        mov esi, jc_ventana
        mov edi, jc_hijos
        mov ebx, jc_gancho
        mov edx, jd_buf
        call dlg_modal
        pop ebx
        cmp eax, ID_ACEPTAR
        jne .x
        imul edi, ebx, JD_DIRTAM
        add edi, gr_dirs
        mov esi, jd_buf
        mov ecx, JD_DIRTAM - 1
        call cad_copiar
        mov dword [jd_hecho], 1
        mov eax, [ac_tabla]         ; los que se borraron a mano vuelven a salir
        mov ecx, MAX_AC
.o:
        cmp [eax+AC_GRUPO], bl
        jne .os
        and byte [eax+AC_BAND2], ~AB2_OCULTO
.os:
        add eax, AC_TAM
        dec ecx
        jnz .o
        shl ebx, 6
        add ebx, [gr_tabla]
        mov dword [bp_nuevos], 0
        call jd_sinc
        call vistas_repintar
        call cfg_guardar
        mov eax, ebx
        call jd_gdir
        cmp byte [eax], 0
        je .x
        mov edi, cen_txt
        mov esi, tj_cd_hecho
        call pon_cad
        mov eax, [bp_nuevos]
        call pon_num
        mov byte [edi], 0
        mov esi, cen_txt
        mov edi, tj_cd_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        mov byte [jd_arr], 1        ; (ya está al día)
        mov eax, ebx
        call grupo_abrir
        mov byte [jd_arr], 0
.x:
        popad
        ret

; V_D3 = el búfer de la carpeta
jc_gancho:
        cmp eax, DM_INICIO
        je .inicio
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 120
        je .examinar
        cmp ecx, ID_ACEPTAR
        jne .no
        mov ecx, 101
        mov edx, jd_buf
        mov ebx, JD_DIRTAM
        call ad_leer_txt
        mov esi, jd_buf
        call cad_largo
        test eax, eax
        jz .no                      ; vacía: el grupo queda sin carpeta
        cmp eax, 3
        jbe .v
        cmp byte [jd_buf+eax-1], '\'
        jne .v
        mov byte [jd_buf+eax-1], 0
.v:
        mov esi, jd_buf
        call jd_existe_dir
        jnc .no
        mov esi, tj_cd_noesta
        call cen_aviso
        jmp .si
.examinar:
        call dlg_carpeta
        cmp eax, 1
        jne .si
        mov ecx, 101
        mov edx, cen_txt
        call ad_poner_txt
        jmp .si
.inicio:
        mov ecx, 101
        mov edx, [ebp+V_D3]
        call ad_poner_txt
.no:
        xor eax, eax
        ret
.si:
        mov eax, 1
        ret

; elegir una carpeta con el diálogo de archivos (solo enseña carpetas; vale
; la que se está viendo al aceptar)
dlg_carpeta:
        push ebx
        push esi
        push edi
        mov byte [da_carpeta], 1
        mov byte [cen_arch], 0
        mov edi, tj_dc_tit
        mov ebx, cen_arch
        call dlg_archivo
        mov byte [da_carpeta], 0
        cmp eax, 1
        jne .x
        cmp byte [cen_arch], 0      ; (un nombre escrito a mano: entrar en él)
        je .a
        cmp byte [cen_arch], '['
        je .a
        mov esi, cen_arch
        call dos_cambiar_dir
.a:
        mov edi, cen_txt
        call dos_dir_actual
        mov eax, 1
.x:
        pop edi
        pop esi
        pop ebx
        ret

; ---- las opciones de juego en el diálogo del acceso (EBP=diálogo EDI=acceso) ----
jd_ad_inicio:
        pushad
        mov esi, edi                ; cómo estaba, para saber si se cambia
        mov edi, jd_copia
        mov ecx, AC_TAM / 4
        rep movsd
        mov edi, [esp]
        movzx eax, byte [edi+AC_VEL]
        mov ecx, 130
        test eax, eax
        jz .r
        mov ecx, 136
        cmp eax, VEL_NIV
        jbe .r
        lea ecx, [eax-VEL_NIV+130]
        cmp ecx, 135
        jbe .r
        mov ecx, 130
.r:
        mov edx, 1
        call ro_poner
        movzx eax, byte [edi+AC_VEL]
        dec eax
        cmp eax, 98
        jbe .p
        mov eax, 49
.p:
        inc eax
        push edi
        mov edi, jd_buf
        call pon_num
        mov byte [edi], 0
        pop edi
        mov ecx, 116
        mov edx, jd_buf
        call ad_poner_txt
        movzx edx, byte [edi+AC_BAND2]
        and edx, AB2_LOADFIX
        mov ecx, 117
        call ro_poner
        movzx edx, byte [edi+AC_BAND2]
        shr edx, 1
        and edx, 1
        mov ecx, 118
        call ro_poner
        call jd_hay_emm             ; sin HSPEMM no hay Sound Blaster emulada
        je .x
        mov ecx, 118
        call ven_hija_id
        or dword [eax+V_ESTADO], S_INACTIVA
        mov dword [eax+V_TEXTO], tj_ad_sb_no
.x:
        popad
        ret

; Aceptar: CF=1 (y un aviso) si el tanto por ciento no vale
jd_ad_validar:
        pushad
        mov ecx, 136
        call ro_marca
        test eax, eax
        jz .ok
        mov ecx, 116
        mov edx, jd_buf
        mov ebx, 8
        call ad_leer_txt
        mov esi, jd_buf
        call lee_num
        cmp eax, 1
        jl .mal
        cmp eax, 100
        jg .mal
.ok:
        popad
        clc
        ret
.mal:
        mov esi, tj_pct_mal
        call cen_aviso
        popad
        stc
        ret

jd_ad_guardar:
        pushad
        xor ebx, ebx
        mov ecx, 131
.n:
        call ro_marca
        test eax, eax
        jnz .niv
        inc ecx
        cmp ecx, 135
        jbe .n
        mov ecx, 136
        call ro_marca
        test eax, eax
        jz .pon
        push ebx
        mov ecx, 116
        mov edx, jd_buf
        mov ebx, 8
        call ad_leer_txt
        pop ebx
        mov esi, jd_buf
        call lee_num
        cmp eax, 99                 ; (el 100 % es la normal)
        ja .pon
        mov ebx, eax
        jmp .pon
.niv:
        lea ebx, [ecx-130+VEL_NIV]
.pon:
        mov [edi+AC_VEL], bl
        mov bl, [edi+AC_BAND2]
        and bl, ~(AB2_LOADFIX | AB2_SB)
        mov ecx, 117
        call ro_marca
        test eax, eax
        jz .l
        or bl, AB2_LOADFIX
.l:
        mov ecx, 118
        call ro_marca
        test eax, eax
        jz .s
        or bl, AB2_SB
.s:
        mov [edi+AC_BAND2], bl
        test bl, AB2_AUTO           ; uno de la carpeta del grupo que se cambia
        jz .x                       ; a mano: desde ahora no se quita solo
        mov esi, edi
        mov edi, jd_copia
        mov ecx, AC_TAM
        repe cmpsb
        je .x
        mov edi, [esp]
        or byte [edi+AC_BAND2], AB2_MANO
.x:
        popad
        ret

; ---- abrir un acceso con opciones de juego ----
; (acc_lanzar) EBX=acceso EBP=ventana EDX=el lanzador (puntero); lz_prog,
; lz_param y lz_dir ya tienen el programa, los parámetros y la carpeta.
; Con velocidad o LOADFIX el programa que se encarga pasa a ser VELOCIDA.COM
; (o LOADFIX.COM) y el juego va en los parámetros:
;     VELOCIDA.COM  386 C:\HISPADOS\LOADFIX.COM JUEGO.EXE parámetros
; -> CF=1 y ESI=aviso si no se puede
jd_componer:
        pushad
        mov al, [ebx+AC_BAND2]
        and al, AB2_LOADFIX
        or al, [ebx+AC_VEL]
        jz .sonido
        ; el juego: si está en la carpeta de trabajo basta su nombre
        lea esi, [ebx+AC_PROG]
        mov edi, cen_arch
        call dir_de_ruta
        lea esi, [edx+lz_dir]
        call cad_igual_i
        lea esi, [ebx+AC_PROG]
        jne .entera
        mov edi, esi
.n:
        lodsb
        test al, al
        jz .n2
        cmp al, '\'
        jne .n
        mov edi, esi
        jmp .n
.n2:
        mov esi, edi
.entera:
        push esi
        mov edi, cen_txt            ; los parámetros del envoltorio
        mov ebp, tn_loadfix         ; EBP = el envoltorio que se ejecuta
        movzx eax, byte [ebx+AC_VEL]
        test eax, eax
        jz .prog
        mov ebp, tn_velocida
        call vel_texto
        mov al, ' '
        stosb
        test byte [ebx+AC_BAND2], AB2_LOADFIX
        jz .prog
        mov esi, dir_propia
        call pon_cad
        mov esi, tn_loadfix
        call pon_cad
        mov al, ' '
        stosb
.prog:
        pop esi
        call pon_cad
        lea esi, [ebx+AC_PARAM]
        cmp byte [esi], 0
        je .fp
        mov al, ' '
        stosb
        call pon_cad
.fp:
        mov byte [edi], 0
        mov esi, tj_larga
        sub edi, cen_txt
        cmp edi, 126
        ja .err
        ; ¿están VELOCIDA.COM y LOADFIX.COM?
        test byte [ebx+AC_BAND2], AB2_LOADFIX
        jz .v
        mov esi, tn_loadfix
        call .esta
        mov esi, tj_no_lf
        jc .err
.v:
        cmp byte [ebx+AC_VEL], 0
        je .pon
        mov esi, tn_velocida
        call .esta
        mov esi, tj_no_vel
        jc .err
        or byte [edx+lz_band], 4
.pon:
        mov esi, ebp
        call .ruta
        mov esi, cen_arch
        lea edi, [edx+lz_prog]
        mov ecx, 79
        call cad_copiar
        mov esi, cen_txt
        lea edi, [edx+lz_param]
        mov ecx, 126
        call cad_copiar
.sonido:
        test byte [ebx+AC_BAND2], AB2_SB
        jz .ok
        cmp byte [snd_hay], 0       ; HSPSND ya está cargado: como esté
        jne .ok
        mov ebp, [esp+8]
        mov esi, tj_sb_sin
        call jd_hay_emm
        jne .sinsb
        mov esi, tn_hspsnd
        call .esta
        mov esi, tj_sb_falta
        jc .sinsb
        or byte [edx+lz_band], 8    ; el lanzador carga HSPSND /SB antes
        jmp .ok
.sinsb:
        call cen_aviso
.ok:
        popad
        clc
        ret
.err:
        mov [esp+4], esi
        popad
        stc
        ret
; (internas) cen_arch = el archivo ESI de la carpeta de Hispa Ventanas; .esta:
; CF=1 si no está
.ruta:
        push edi
        mov edi, cen_arch
        push esi
        mov esi, dir_propia
        call pon_cad
        pop esi
        call pon_cad
        mov byte [edi], 0
        pop edi
        ret
.esta:
        push eax
        push ebx
        call .ruta
        mov esi, cen_arch
        xor al, al
        call dos_abrir
        jc .e9
        mov ebx, eax
        call dos_cerrar
        clc
.e9:
        pop ebx
        pop eax
        ret

; al volver de un juego abierto con VELOCIDAD, el lanzador ha comprobado que
; el equipo queda a su velocidad normal (lz_vel_red, ventanas.asm) y deja
; dicho si algo no ha ido como debía. EBP=la ventana activa
jd_tras_volver:
        pushad
        movzx edx, word [lz_seg]
        test edx, edx
        jz .x
        shl edx, 4
        add edx, [cero]
        movzx eax, byte [edx+lz_velst]
        mov byte [edx+lz_velst], 0
        mov esi, tj_vel_frena
        cmp eax, LZV_FRENA
        je .av
        mov esi, tj_vel_queda
        cmp eax, LZV_QUEDA
        jne .x
.av:
        mov edi, tj_vel_tit
        mov eax, DM_ACEPTAR
        call dlg_mensaje
.x:
        popad
        ret
