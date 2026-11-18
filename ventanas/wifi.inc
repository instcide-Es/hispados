; =============================================================================
;  Hispa Ventanas - [WIFI] el icono de red de la barra de tareas de Hispa-486
;  y su ventanita (docs/WIFI.md «En Hispa Ventanas»; docs/VENTANAS.md)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Va en la parte alta de VENTANAS.COM; en barra.inc solo hay ganchos de una
;  o dos líneas, marcados con [WIFI]. En Hispa-386 (sin barra de tareas) no se
;  usa nada de esto: allí sigue el icono de siempre (red_pintar_icono).
;
;  El icono, a la izquierda del altavoz, dice cómo está la red por la forma
;  (no solo por el color):
;    sin red             dos nodos huecos y un aspa
;    cable sin enlace    dos nodos llenos y un aspa
;    cable               dos nodos llenos unidos
;    Wi-Fi conectado     cuatro rayitas de alto creciente: llenas, las de la
;                        señal (de 1 a 4, como WIFI REDES); huecas, las demás
;    Wi-Fi conectando    las cuatro huecas
;    Wi-Fi sin conectar  las cuatro huecas y un aspa
;  Un clic abre (o cierra) la ventanita, encima de la barra:
;    - con HSPWIFI: cómo está la conexión, la lista de redes (señal en
;      rayitas, seguridad), la contraseña (con asteriscos), «Buscar»,
;      «Conectar», «Desconectar», «Olvidar»;
;    - con HSPRED: el enlace, la dirección IP y la puerta de enlace;
;    - y siempre «Configuración de red...» (Configuración › Red).
;  Habla con HSPWIFI por su interrupción (INT 2Fh AX=DC01h: CX = función del
;  núcleo, DS:SI = la petición; las funciones, en wifi/nucleo/llamadas.h), lo
;  mismo que la orden WIFI: no hace falta nada nuevo en el controlador. La
;  petición y los datos van por dos_buf (memoria convencional: el núcleo
;  recibe direcciones lineales) y se copian aquí enseguida. Lo que tarda
;  (buscar, preparar la clave: 8.192 HMAC-SHA1, conectar) no para el
;  escritorio: lo lleva wf_vigilar a trozos, una vez por vuelta de la barra.
;  Al conectar, la red y su clave ya preparada se guardan en RED.INI (WIFI= y
;  WIFICLAVE=), como hace WIFI CONECTAR.
;
;    wf_vigilar      (bt_vigilar) mira la red cada segundo (más a menudo con
;                    la ventanita abierta o algo en marcha) y lleva lo que tarda
;    wf_pintar_icono (bt_pintar) el icono
;    wf_en_icono     (bt_proc) EAX=x EBX=y -> CF=1 si el punto está en él
;    wf_alternar     (bt_proc) abre o cierra la ventanita
;    wf_firma        (bt_vigilar) lo que se ve: entra en la firma de la bandeja
; =============================================================================

WF_REDES    equ 16                  ; redes que se piden al núcleo
WF_FILAS    equ 6                   ; las que se ven a la vez
WF_FILA     equ 18
WF_INI      equ 4096                ; RED.INI es pequeño
; struct ll_red (wifi/nucleo/llamadas.h)
WR_SENAL    equ 7
WR_SEG      equ 8
WR_SSID_N   equ 9
WR_SSID     equ 10
WR_LEN      equ 42
; struct w_info (wifi/nucleo/nucleo.h), con lo que añade el residente
WI_ESTADO   equ 0                   ; 5: conectado
WI_FALLO    equ 1
WI_SENAL    equ 3
WI_SSID     equ 10
WI_SSID_N   equ 43
WI_BARRE    equ 46
WI_LEN      equ 85
; las funciones del núcleo
WLL_BARRE   equ 5
WLL_REDES   equ 6
WLL_CONECTA equ 7
WLL_DESCON  equ 8
WLL_ESTADO  equ 9
WLL_PMK_E   equ 11
WLL_PMK_S   equ 12
; el paso por dos_buf: la petición (tres dobles palabras) y los datos
WF_PET      equ dos_buf
WF_DAT      equ dos_buf + 16
; lo que está haciendo la ventanita
WF_F_BUSCA  equ 1
WF_F_CLAVE  equ 2
WF_F_CONECTA equ 3
; los controles
WF_ID_BUSCAR equ 30
WF_ID_CONECTAR equ 31
WF_ID_DESCON equ 32
WF_ID_OLVIDAR equ 33
WF_ID_CONFIG equ 34
WF_LIS_X    equ 8
WF_LIS_Y    equ 44
WF_LIS_AN   equ 344

c_wifi      dd wf_proc
            db C_CREMA, 0, 0, 0
c_wflis     dd wfl_proc
            db C_BLANCO, CB_FOCO, 0, 0
c_wfcla     dd wfc_proc
            db C_BLANCO, CB_FOCO, 0, 0
wf_h_wifi:
        PLANTILLA c_wflis, 0, E_HIJA | E_TABULA, WF_LIS_X, WF_LIS_Y, WF_LIS_AN, WF_FILAS * WF_FILA + 4, 35
        PLANTILLA c_wfcla, 0, E_HIJA | E_TABULA, 104, 163, 248, 22, 36
        PLANTILLA c_boton, wf_t_buscar, E_HIJA | E_TABULA, 8, 194, 72, 24, WF_ID_BUSCAR
        PLANTILLA c_boton, wf_t_conectar, E_HIJA | E_TABULA | BS_DEFECTO, 86, 194, 88, 24, WF_ID_CONECTAR
        PLANTILLA c_boton, wf_t_descon, E_HIJA | E_TABULA, 180, 194, 104, 24, WF_ID_DESCON
        PLANTILLA c_boton, wf_t_olvidar, E_HIJA | E_TABULA, 290, 194, 62, 24, WF_ID_OLVIDAR
        PLANTILLA c_boton, wf_t_config, E_HIJA | E_TABULA, 8, 226, 200, 24, WF_ID_CONFIG
        dd 0
wf_h_cable:
        PLANTILLA c_boton, wf_t_config, E_HIJA | E_TABULA, 10, 92, 200, 24, WF_ID_CONFIG
        dd 0
wf_h_nada:
        PLANTILLA c_boton, wf_t_config, E_HIJA | E_TABULA, 10, 74, 200, 24, WF_ID_CONFIG
        dd 0
; ancho, alto y controles de la ventanita en cada caso (sin red, cable, Wi-Fi)
wf_medidas  dd 320, 133, wf_h_nada
            dd 320, 151, wf_h_cable
            dd 364, 283, wf_h_wifi

wf_t_tit    db "Red", 0
wf_t_buscar db "Buscar", 0
wf_t_conectar db "Conectar", 0
wf_t_descon db "Desconectar", 0
wf_t_olvidar db "Olvidar", 0
wf_t_config db "Configuración de red...", 0
wf_t_clave  db "Contraseña:", 0
wf_t_sinred db "Sin red", 0
wf_t_sinred2 db "No hay controlador de red cargado:", 0
wf_t_sinred3 db "HSPRED (cable) o HSPWIFI (Wi-Fi).", 0
wf_t_cable  db "Red por cable", 0
wf_t_otro   db "Hay un controlador de red (packet driver),", 0
wf_t_otro2  db "pero no da el enlace ni la dirección.", 0
wf_t_enl    db "Enlace: ", 0
wf_t_enl1   db "conectado", 0
wf_t_enl0   db "no hay (¿está el cable?)", 0
wf_t_enlx   db "no se sabe", 0
wf_t_ip     db "Dirección IP: ", 0
wf_t_noip   db "todavía sin dirección", 0
wf_t_gw     db "Puerta de enlace: ", 0
wf_t_con    db "Conectado a ", 0
wf_t_conando db "Conectando con ", 0
wf_t_sincon db "Wi-Fi: sin conectar", 0
wf_t_busca  db "Buscando redes...", 0
wf_t_prepara db "Preparando la clave...", 0
wf_t_senal  db "Señal ", 0
wf_t_dbm    db " dBm, ", 0
wf_t_ipw    db "IP ", 0
wf_t_noipw  db "sin IP: falta CONFRED", 0
wf_t_elija  db "Elija una red y pulse «Conectar».", 0
wf_t_ninguna db "(ninguna red: pulse «Buscar»)", 0
wf_t_abierta db "abierta", 0
wf_t_wpa2   db "WPA2", 0
wf_t_noadm  db "no admitida", 0
wf_m_elige  db "Elija una red de la lista.", 0
wf_m_noseg  db "Esa red no se puede usar: no es WPA2.", 0
wf_m_pide   db "Escriba la contraseña de la red.", 0
wf_m_corta  db "La contraseña va de 8 a 63 letras.", 0
wf_m_descon db "Desconectado.", 0
wf_m_olvido db "Red olvidada (quitada de RED.INI).", 0
wf_m_noini  db "No se ha podido guardar RED.INI.", 0
; por qué no conecta (struct w_info, fallo: los de WIFI, más cortos)
wf_t_fallo  dd wf_f10, wf_f1, wf_f2, wf_f3, wf_f2, wf_f3, wf_f6, wf_f7, wf_f8, wf_f9, wf_f10
wf_f1       db "No conecta: la red no aparece.", 0
wf_f2       db "No conecta: el router no contesta.", 0
wf_f3       db "No conecta: el router la rechaza.", 0
wf_f6       db "No conecta: la contraseña no es esa.", 0
wf_f7       db "No conecta: seguridad no admitida.", 0
wf_f8       db "El router ha cortado la conexión.", 0
wf_f9       db "Se ha perdido la señal.", 0
wf_f10      db "No conecta: no contesta a tiempo.", 0
wf_k_wifi   db "WIFI", 0
wf_k_clave  db "WIFICLAVE", 0
wf_k_ruta   db "A:\HISPADOS\RED.INI", 0

; =============================================================================
;  Hablar con los controladores
; =============================================================================
; (interna) EAX = desplazamiento en el programa -> EAX = dirección lineal
wf_lin:
        push edx
        movzx edx, word [rmseg]
        shl edx, 4
        add eax, edx
        pop edx
        ret

; (interna) INT 2Fh en modo real con AX=EAX -> rmcs
wf_2f:
        push ebx
        mov [rmcs+0x1C], eax
        xor eax, eax
        mov [rmcs+0x00], eax
        mov [rmcs+0x10], eax
        mov bl, 0x2F
        call rm_int
        pop ebx
        ret

; (interna) ES:reg de rmcs -> ESI = puntero plano. EAX = desplazamiento
wf_es:
        movzx esi, word [rmcs+0x22]
        shl esi, 4
        add esi, eax
        add esi, [cero]
        ret

; (interna) llama al núcleo del Wi-Fi: ECX=función, la petición en WF_PET ->
; EAX = lo que devuelve (DX:AX)
wf_nuc:
        push ebx
        mov dword [rmcs+0x1C], 0xDC01
        mov [rmcs+0x18], ecx
        mov dword [rmcs+0x04], WF_PET
        mov bl, 0x2F
        call rm_int
        mov dword [rmcs+0x04], 0
        movzx eax, word [rmcs+0x14]
        shl eax, 16
        mov ax, [rmcs+0x1C]
        pop ebx
        ret

; (interna) la petición: EAX, EDX, EBX, con EAX y EDX desplazamientos que
; pasan a direcciones lineales (0: se deja 0)
wf_pet:
        test eax, eax
        jz .a
        call wf_lin
.a:
        mov [WF_PET], eax
        mov eax, edx
        test eax, eax
        jz .d
        call wf_lin
.d:
        mov [WF_PET+4], eax
        mov [WF_PET+8], ebx
        ret

; (interna) el estado del Wi-Fi, a wf_info
wf_estado:
        pushad
        mov eax, WF_DAT
        xor edx, edx
        xor ebx, ebx
        call wf_pet
        mov dword [WF_PET+4], WI_LEN
        mov ecx, WLL_ESTADO
        call wf_nuc
        mov esi, WF_DAT
        mov edi, wf_info
        mov ecx, WI_LEN
        rep movsb
        popad
        ret

; (interna) la lista de redes, a wf_redes, de más a menos señal; elige la
; red conectada (o la primera)
wf_lista:
        pushad
        mov eax, WF_DAT
        xor edx, edx
        mov ebx, 0
        call wf_pet
        mov dword [WF_PET+4], WF_REDES
        mov ecx, WLL_REDES
        call wf_nuc
        cmp eax, WF_REDES
        jbe .n
        xor eax, eax                ; (-1: no está listo)
.n:
        mov [wf_nredes], eax
        imul ecx, eax, WR_LEN
        mov esi, WF_DAT
        mov edi, wf_redes
        rep movsb
        ; por la señal (una burbuja: son pocas)
.otra:
        xor edx, edx                ; ¿se ha cambiado alguna?
        mov ecx, [wf_nredes]
        dec ecx
        jle .ord
        mov esi, wf_redes
.par:
        mov al, [esi+WR_SENAL]
        cmp al, [esi+WR_LEN+WR_SENAL]
        jge .bien
        push ecx
        mov ecx, WR_LEN
.cambia:
        mov al, [esi]
        xchg al, [esi+WR_LEN]
        mov [esi], al
        inc esi
        loop .cambia
        pop ecx
        sub esi, WR_LEN
        inc edx
.bien:
        add esi, WR_LEN
        loop .par
        test edx, edx
        jnz .otra
.ord:
        mov dword [wf_prim], 0
        mov dword [wf_sel], -1
        cmp dword [wf_nredes], 0
        je .x
        mov dword [wf_sel], 0
        xor ecx, ecx
.b:
        call wf_es_la_conectada
        jc .es
        inc ecx
        cmp ecx, [wf_nredes]
        jb .b
        jmp .x
.es:
        mov [wf_sel], ecx
        cmp ecx, WF_FILAS
        jb .x
        lea eax, [ecx - WF_FILAS + 1]
        mov [wf_prim], eax
.x:
        call wf_inv_lista
        popad
        ret

; (interna) ECX = red de la lista -> ESI = su entrada; CF=1 si es aquella con
; la que se está conectado
wf_es_la_conectada:
        push eax
        push ecx
        push edi
        imul esi, ecx, WR_LEN
        add esi, wf_redes
        cmp byte [wf_info+WI_ESTADO], 5
        jne .no
        movzx ecx, byte [esi+WR_SSID_N]
        cmp cl, [wf_info+WI_SSID_N]
        jne .no
        push esi
        add esi, WR_SSID
        mov edi, wf_info+WI_SSID
        repe cmpsb
        pop esi
        jne .no
        pop edi
        pop ecx
        pop eax
        stc
        ret
.no:
        pop edi
        pop ecx
        pop eax
        clc
        ret

; (interna) ¿qué red hay? -> wf_modo, wf_hspred, wf_enlace, wf_cfg, wf_info
wf_leer:
        pushad
        mov byte [wf_modo], 0
        mov byte [wf_hspred], 0
        mov byte [wf_enlace], 0xFF
        mov byte [wf_cfg], 0
        mov eax, 0xDC00             ; ¿HSPWIFI?
        call wf_2f
        cmp byte [rmcs+0x1C], 0xFF
        jne .da
        cmp word [rmcs+0x10], 0x5746    ; «WF»
        jne .da
        mov byte [wf_modo], 2
        call wf_estado
.da:
        ; HSPRED (y HSPWIFI, que hace lo mismo): la configuración IP compartida
        mov eax, 0xDA00
        call wf_2f
        cmp byte [rmcs+0x1C], 0xFF
        jne .pkt
        cmp word [rmcs+0x10], 0x5248    ; «HR»
        jne .pkt
        movzx eax, word [rmcs+0x00]     ; ES:DI
        call wf_es
        mov edi, wf_cfg
        mov ecx, 30
        rep movsb
        cmp byte [wf_modo], 2
        je .x
        mov byte [wf_modo], 1
        mov byte [wf_hspred], 1
        mov eax, 0xDA01             ; el enlace
        call wf_2f
        mov al, [rmcs+0x1C]
        mov [wf_enlace], al
        jmp .x
.pkt:
        cmp byte [wf_modo], 2
        je .x
        call red_leer               ; otro packet driver: cable, sin más datos
        mov al, [red_hay]
        mov [wf_modo], al
.x:
        popad
        ret

; (interna) EAX = señal en dBm -> EAX = rayitas (1 a 4, como WIFI REDES)
wf_nivel:
        push edx
        mov edx, 1
        cmp eax, -75
        jl .x
        inc edx
        cmp eax, -65
        jl .x
        inc edx
        cmp eax, -55
        jl .x
        inc edx
.x:
        mov eax, edx
        pop edx
        ret

; (interna) -> EAX = la firma de lo que se ve (icono y ventanita)
wf_firmar:
        push ecx
        push esi
        movzx eax, byte [wf_modo]
        mov ah, [wf_enlace]
        shl eax, 8
        mov al, [wf_fase]
        xor eax, [wf_msg]
        mov esi, wf_cfg
        mov ecx, 14
.c:
        rol eax, 5
        xor al, [esi]
        inc esi
        loop .c
        cmp byte [wf_modo], 2
        jne .x
        mov esi, wf_info
        mov ecx, 4                  ; estado, fallo, canal y señal
.w:
        rol eax, 5
        xor al, [esi]
        inc esi
        loop .w
        rol eax, 3
        xor al, [wf_info+WI_BARRE]
.x:
        pop esi
        pop ecx
        ret

; =============================================================================
;  Cada vuelta de la barra
; =============================================================================
wf_vigilar:
        pushad
        cmp byte [op_demo], 0
        jne .x
        call tics
        mov ebx, eax
        sub eax, [wf_tic]
        jl .ya                      ; (pasada la medianoche)
        mov ecx, 18                 ; cada segundo...
        cmp dword [wf_ven], 0
        jne .rapido
        cmp byte [wf_fase], 0
        je .c
.rapido:
        mov ecx, 3                  ; ...o cada 3 tics, con la ventanita o algo en marcha
.c:
        cmp eax, ecx
        jb .x
.ya:
        mov [wf_tic], ebx
        call wf_leer
        call wf_fases
        call wf_firmar
        cmp eax, [wf_firma]
        je .x
        mov [wf_firma], eax         ; (la bandeja se repinta sola: bt_vigilar)
        mov ebp, [wf_ven]
        test ebp, ebp
        jz .x
        call ven_cli                ; lo de arriba de la ventanita (o toda, sin Wi-Fi)
        mov edx, WF_LIS_Y - 2
        cmp byte [wf_modo], 2
        je .inv
        mov edx, 90
.inv:
        xor eax, eax
        xor ebx, ebx
        call ven_invalidar_rect
.x:
        popad
        ret

; (interna) lo que tarda: buscar, preparar la clave, conectar
wf_fases:
        mov al, [wf_fase]
        test al, al
        jz .r
        cmp byte [wf_modo], 2
        jne .fin                    ; (se ha descargado HSPWIFI)
        call tics
        sub eax, [wf_t0]
        mov edx, eax                ; EDX = tics desde que empezó
        mov al, [wf_fase]
        cmp al, WF_F_BUSCA
        jne .f2
        cmp edx, 2
        jb .r
        cmp byte [wf_info+WI_BARRE], 0
        je .buscado
        cmp edx, 110                ; 6 s como mucho
        jb .r
.buscado:
        mov byte [wf_fase], 0
        jmp wf_lista
.f2:
        cmp al, WF_F_CLAVE
        jne .f3
        mov ebp, 16                 ; 16 x 128 HMAC por vuelta
.pmk:
        mov eax, WF_DAT
        xor edx, edx
        xor ebx, ebx
        call wf_pet
        mov ecx, WLL_PMK_S
        call wf_nuc
        cmp eax, 1
        je .pmkf
        dec ebp
        jnz .pmk
.r:
        ret
.pmkf:
        mov esi, WF_DAT
        mov edi, wf_pmk
        mov ecx, 32
        rep movsb
        jmp wf_conectar_ya
.f3:
        cmp byte [wf_info+WI_ESTADO], 5
        je .bien
        movzx eax, byte [wf_info+WI_FALLO]
        cmp al, 6                   ; contraseña, seguridad, rechazos: ya
        je .mal
        cmp al, 7
        je .mal
        cmp al, 3
        je .mal
        cmp al, 5
        je .mal
        cmp al, 1                   ; «no aparece», tras dos búsquedas
        jne .t
        cmp edx, 200
        jae .mal
.t:
        cmp edx, 455                ; 25 s
        jb .r
        xor eax, eax
.mal:
        cmp eax, 10
        jbe .m
        xor eax, eax
.m:
        mov eax, [wf_t_fallo + eax*4]
        mov [wf_msg], eax
        mov ecx, WLL_DESCON         ; que no siga probando con una clave mala
        call wf_nuc
.fin:
        mov byte [wf_fase], 0
        ret
.bien:
        mov byte [wf_fase], 0       ; (la 2.ª línea: la señal y la IP)
        call wf_ini_guardar
        call wf_ip_olvidar
        jmp wf_lista                ; (la conectada, en negrita)

; (interna) conectar con wf_ssid (y wf_pmk si wf_con_clave)
wf_conectar_ya:
        pushad
        movzx ecx, byte [wf_ssid_n]
        mov esi, wf_ssid
        mov edi, WF_DAT
        rep movsb
        xor edx, edx
        mov dword [WF_PET+8], 0
        cmp byte [wf_con_clave], 0
        je .sin
        mov esi, wf_pmk
        mov edi, WF_DAT + 64
        mov ecx, 8
        rep movsd
        mov edx, WF_DAT + 64
.sin:
        mov eax, WF_DAT
        movzx ebx, byte [wf_ssid_n]
        call wf_pet                 ; [0] nombre, [1] PMK: hay que ponerlos al revés
        mov eax, [WF_PET+4]
        mov [WF_PET+8], eax
        mov [WF_PET+4], ebx
        mov ecx, WLL_CONECTA
        call wf_nuc
        mov edi, WF_DAT + 64        ; (la clave, fuera de dos_buf)
        xor eax, eax
        mov ecx, 8
        rep stosd
        mov byte [wf_fase], WF_F_CONECTA
        call tics
        mov [wf_t0], eax
        mov dword [wf_msg], 0
        popad
        ret

; (interna) la configuración IP que hubiera es de otra red: que se pida otra
; vez (salvo con HSPWIFI /JUNTO, que es la de HSPRED). Como WIFI CONECTAR.
wf_ip_olvidar:
        pushad
        mov eax, 0xDC02
        call wf_2f
        movzx eax, word [rmcs+0x10]     ; ES:BX = datos del residente
        call wf_es
        test byte [esi+64], 8
        jnz .x
        mov eax, 0xDA00
        call wf_2f
        cmp byte [rmcs+0x1C], 0xFF
        jne .x
        movzx eax, word [rmcs+0x00]
        call wf_es
        mov word [esi], 0
.x:
        popad
        ret

; (interna) «Buscar»
wf_buscar:
        cmp byte [wf_modo], 2
        jne .r
        cmp byte [wf_fase], WF_F_CLAVE  ; (no a media conexión)
        jae .r
        pushad
        mov ecx, WLL_BARRE
        call wf_nuc
        mov byte [wf_fase], WF_F_BUSCA
        mov byte [wf_info+WI_BARRE], 1
        call tics
        mov [wf_t0], eax
        mov dword [wf_msg], 0
        popad
.r:
        ret

; (interna) «Conectar»: la red elegida, con la contraseña escrita (o la
; guardada en RED.INI si es esa red y no se escribe ninguna)
wf_conectar:
        pushad
        cmp byte [wf_fase], WF_F_CLAVE
        jae .x
        mov ecx, [wf_sel]
        cmp ecx, [wf_nredes]
        jb .hay
        mov dword [wf_msg], wf_m_elige
        jmp .x
.hay:
        imul esi, ecx, WR_LEN
        add esi, wf_redes
        mov al, [esi+WR_SEG]
        push esi
        movzx ecx, byte [esi+WR_SSID_N]
        mov [wf_ssid_n], cl
        add esi, WR_SSID
        mov edi, wf_ssid
        rep movsb
        pop esi
        mov byte [wf_con_clave], 0
        test al, al
        jz .ya                      ; abierta
        cmp al, 3
        je .wpa2
        mov dword [wf_msg], wf_m_noseg
        jmp .x
.wpa2:
        mov byte [wf_con_clave], 1
        mov ecx, [wf_clave_n]
        test ecx, ecx
        jnz .escrita
        call wf_ini_pmk             ; ¿la guardada?
        jnc .ya
        mov dword [wf_msg], wf_m_pide
        mov ebp, [wf_cla]
        test ebp, ebp
        jz .x
        call ven_enfocar
        jmp .x
.escrita:
        cmp ecx, 64                 ; 64 cifras: ya es la clave preparada
        jne .frase
        mov esi, wf_clave
        call wf_hex_pmk
        jnc .borra_ya
.frase:
        cmp ecx, 8
        jb .corta
        cmp ecx, 63
        jbe .prepara
.corta:
        mov dword [wf_msg], wf_m_corta
        jmp .x
.prepara:
        mov esi, wf_clave
        mov edi, WF_DAT
        rep movsb
        mov byte [edi], 0
        movzx ecx, byte [wf_ssid_n]
        mov esi, wf_ssid
        mov edi, WF_DAT + 80
        rep movsb
        mov eax, WF_DAT
        mov edx, WF_DAT + 80
        movzx ebx, byte [wf_ssid_n]
        call wf_pet
        mov ecx, WLL_PMK_E
        call wf_nuc
        mov edi, WF_DAT             ; la contraseña, fuera de dos_buf...
        xor eax, eax
        mov ecx, 20
        rep stosd
        call wf_clave_borrar        ; ...y de aquí
        mov byte [wf_fase], WF_F_CLAVE
        mov dword [wf_msg], 0
        jmp .x
.borra_ya:
        call wf_clave_borrar
.ya:
        call wf_conectar_ya
.x:
        popad
        ret

; (interna) borra la contraseña escrita
wf_clave_borrar:
        pushad
        mov edi, wf_clave
        xor eax, eax
        mov ecx, 68 / 4
        rep stosd
        mov [wf_clave_n], eax
        mov ebp, [wf_cla]
        test ebp, ebp
        jz .x
        call ven_invalidar
.x:
        popad
        ret

; (interna) la lista, a repintar
wf_inv_lista:
        pushad
        mov ebp, [wf_lis]
        test ebp, ebp
        jz .x
        call ven_invalidar
.x:
        popad
        ret

; =============================================================================
;  RED.INI (\HISPADOS de la unidad de arranque, como WIFI): WIFI= y WIFICLAVE=
; =============================================================================
; (interna) lee RED.INI a wf_ini -> [wf_ini_n]; CF si no se ha podido
wf_ini_leer:
        pushad
        mov esi, wf_k_ruta
        mov edi, wf_ruta
        mov ecx, 20
        rep movsb
        mov eax, 0x3305             ; la unidad de arranque
        call dos_int
        mov al, [rmcs+0x14]
        add al, 'A' - 1
        mov [wf_ruta], al
        mov dword [wf_ini_n], 0
        mov esi, wf_ruta
        mov al, 0
        call dos_abrir
        jc .x
        mov ebx, eax
        mov edi, wf_ini
        mov ecx, WF_INI
        call dos_leer
        jc .c
        mov [wf_ini_n], eax
.c:
        call dos_cerrar
        clc
.x:
        popad
        ret

; (interna) ESI = principio de una línea de wf_ini -> EAX = 1 si es WIFI=,
; 2 si WIFICLAVE=, 0 otra; EBX = donde empieza el valor
wf_ini_clave:
        push esi
        push edi
        mov edi, wf_k_clave
        call .cmp
        mov eax, 2
        jnc .r
        mov esi, [esp+4]
        mov edi, wf_k_wifi
        call .cmp
        mov eax, 1
        jnc .r
        xor eax, eax
.r:
        mov ebx, esi
        pop edi
        pop esi
        ret
.cmp:
        mov al, [esi]
        cmp al, 'a'
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 32
.m:
        cmp byte [edi], 0
        je .fin
        cmp al, [edi]
        jne .no
        inc esi
        inc edi
        jmp .cmp
.fin:
        cmp al, '='
        jne .no
        inc esi
        clc
        ret
.no:
        stc
        ret

; (interna) ESI = 64 cifras hexadecimales -> wf_pmk; CF si no lo son
wf_hex_pmk:
        pushad
        mov edi, wf_pmk
        mov ecx, 32
.l:
        call .cifra
        jc .x
        shl al, 4
        mov ah, al
        call .cifra
        jc .x
        or al, ah
        stosb
        loop .l
        clc
.x:
        popad
        ret
.cifra:
        lodsb
        sub al, '0'
        cmp al, 9
        jbe .ok
        or al, 0x20                 ; (de «A» a «a»: 11h -> 31h)
        sub al, 'a' - '0'
        cmp al, 5
        ja .mal
        add al, 10
.ok:
        clc
        ret
.mal:
        stc
        ret

; (interna) ¿RED.INI guarda wf_ssid con su clave? -> wf_pmk; CF si no
wf_ini_pmk:
        pushad
        call wf_ini_leer
        mov esi, wf_ini
        mov ebp, esi
        add ebp, [wf_ini_n]
        xor edx, edx                ; bit 0: es la red; bit 1: hay clave
.linea:
        cmp esi, ebp
        jae .fin
        call wf_ini_clave
        cmp eax, 1
        jne .n2
        ; WIFI=nombre, hasta el fin de la línea
        push esi
        mov esi, ebx
        mov edi, wf_ssid
        movzx ecx, byte [wf_ssid_n]
        repe cmpsb
        jne .nom
        mov al, [esi]
        cmp al, 13
        je .si
        cmp al, 10
        je .si
        cmp esi, ebp
        jb .nom
.si:
        or dl, 1
.nom:
        pop esi
        jmp .salta
.n2:
        cmp eax, 2
        jne .salta
        push esi
        mov esi, ebx
        lea eax, [ebx+64]
        cmp eax, ebp
        ja .nk
        call wf_hex_pmk
        jc .nk
        or dl, 2
.nk:
        pop esi
.salta:
        cmp esi, ebp
        jae .fin
        lodsb
        cmp al, 10
        jne .salta
        jmp .linea
.fin:
        cmp dl, 3
        je .bien
        stc
        jmp .x
.bien:
        clc
.x:
        popad
        ret

; (interna) escribe RED.INI sin las líneas del Wi-Fi y, si hay red
; (wf_ssid_n), con las de wf_ssid y su clave al final. Si no puede, lo dice.
wf_ini_guardar:
        pushad
        call wf_ini_leer
        cmp dword [wf_ini_n], WF_INI    ; demasiado grande para rehacerlo
        jae .mal
        mov esi, wf_ini
        mov ebp, esi
        add ebp, [wf_ini_n]
        mov edi, wf_ini2
.linea:
        cmp esi, ebp
        jae .fin
        call wf_ini_clave
        mov edx, eax
        mov ebx, esi                ; el final de la línea
.e:
        cmp ebx, ebp
        jae .ef
        inc ebx
        cmp byte [ebx-1], 10
        jne .e
.ef:
        mov ecx, ebx
        sub ecx, esi
        test edx, edx
        jnz .sig                    ; las del Wi-Fi no se copian
        rep movsb
.sig:
        mov esi, ebx
        jmp .linea
.fin:
        cmp edi, wf_ini2            ; que acabe en fin de línea antes de añadir
        je .nuevas
        cmp byte [edi-1], 10
        je .nuevas
        mov ax, 0x0A0D
        stosw
.nuevas:
        movzx ecx, byte [wf_ssid_n]
        jecxz .escribe
        mov eax, 'WIFI'
        stosd
        mov al, '='
        stosb
        mov esi, wf_ssid
        rep movsb
        mov ax, 0x0A0D
        stosw
        cmp byte [wf_con_clave], 0
        je .escribe
        mov esi, wf_k_clave
        call pon_cad
        mov al, '='
        stosb
        mov esi, wf_pmk
        mov ecx, 32
.h:
        lodsb
        mov ah, al
        shr al, 4
        call .cifra
        mov al, ah
        call .cifra
        loop .h
        mov ax, 0x0A0D
        stosw
.escribe:
        mov esi, wf_ruta
        call dos_crear
        jc .mal
        mov ebx, eax
        mov esi, wf_ini2
        mov ecx, edi
        sub ecx, esi
        call dos_escribir
        pushf
        call dos_cerrar
        popf
        jnc .x
.mal:
        mov dword [wf_msg], wf_m_noini
.x:
        popad
        ret
.cifra:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 'a' - '9' - 1
.d:
        stosb
        ret

; =============================================================================
;  El icono de la bandeja
; =============================================================================
; (interna) EAX=x EBX=y de abajo ECX=rayitas llenas: cuatro rayitas de alto
; creciente; las llenas, en [wf_c_lleno]; las demás, huecas en [wf_c_hueco]
wf_rayitas:
        pushad
        mov [wf_nv], ecx
        mov esi, eax
        mov edi, ebx
        xor ebp, ebp
.r:
        lea edx, [ebp*2+ebp+4]      ; alto: 4, 7, 10, 13
        mov ebx, edi
        sub ebx, edx
        lea eax, [esi+ebp*4]
        mov ecx, 3
        cmp ebp, [wf_nv]
        jae .hueca
        mov cl, [wf_c_lleno]
        mov [gdi_color], cl
        mov ecx, 3
        call dc_rect
        jmp .s
.hueca:
        mov cl, [wf_c_hueco]
        mov [gdi_color], cl
        mov ecx, 3
        call dc_marco
.s:
        inc ebp
        cmp ebp, 4
        jb .r
        popad
        ret

; (interna) EAX=x EBX=y: un aspa de 5x5, en terracota clara (con dos de grueso)
wf_aspa:
        pushad
        COLOR C_TERRACOTA_CL
        lea ecx, [eax+4]
        lea edx, [ebx+4]
        call dc_linea
        inc eax
        inc ecx
        call dc_linea
        dec eax
        dec ecx
        xchg ebx, edx
        call dc_linea
        inc eax
        inc ecx
        call dc_linea
        popad
        ret

wf_pintar_icono:
        pushad
        cmp byte [op_demo], 0
        jne .x
        mov esi, [vid_ancho]
        sub esi, [vol_x]
        sub esi, 24                 ; a la izquierda del altavoz
        cmp byte [wf_modo], 2
        je .wifi
        ; los dos nodos unidos (como red_pintar_icono): llenos con red,
        ; huecos sin ella
        COLOR C_CREMA
        mov edi, dc_rect
        cmp byte [wf_modo], 0
        jne .c
        COLOR C_ARENA_OSC
        mov edi, dc_marco
.c:
        mov eax, esi
        mov ebx, 3
        mov ecx, 5
        mov edx, 4
        call edi
        lea eax, [esi+8]
        mov ebx, 8
        call edi
        lea eax, [esi+2]
        mov ebx, 6
        lea ecx, [esi+10]
        mov edx, 9
        call dc_linea
        cmp byte [wf_modo], 0
        je .aspa                    ; sin red
        cmp byte [wf_enlace], 0
        jne .x                      ; cable con enlace (o no se sabe)
.aspa:
        lea eax, [esi+9]            ; arriba a la derecha
        mov ebx, 1
        call wf_aspa
        jmp .x
.wifi:
        mov byte [wf_c_lleno], C_CREMA
        mov byte [wf_c_hueco], C_ARENA
        xor ecx, ecx
        cmp byte [wf_info+WI_ESTADO], 5
        jne .r
        movsx eax, byte [wf_info+WI_SENAL]
        call wf_nivel
        mov ecx, eax
.r:
        lea eax, [esi-1]
        mov ebx, 17
        call wf_rayitas
        cmp byte [wf_info+WI_ESTADO], 0
        jne .x                      ; conectado o conectando
        cmp byte [wf_fase], WF_F_CLAVE
        jae .x
        lea eax, [esi-1]            ; sin conectar: el aspa, arriba a la izquierda
        mov ebx, 2
        call wf_aspa
.x:
        popad
        ret

wf_en_icono:
        push ecx
        cmp byte [op_demo], 0
        jne .no
        mov ecx, [vid_ancho]
        sub ecx, [vol_x]
        sub ecx, 24 + 3
        cmp eax, ecx
        jl .no
        add ecx, 21
        cmp eax, ecx
        jge .no
        cmp ebx, 2
        jl .no
        cmp ebx, BARRA_H - 3
        jge .no
        pop ecx
        stc
        ret
.no:
        pop ecx
        clc
        ret

; =============================================================================
;  La ventanita
; =============================================================================
wf_alternar:
        pushad
        mov ebp, [wf_ven]
        test ebp, ebp
        jz .abrir
        call ven_destruir
        jmp .x
.abrir:
        call wf_leer
        movzx eax, byte [wf_modo]
        imul eax, 12
        lea edi, [wf_medidas + eax]
        sub esp, P_TAM
        mov esi, esp
        mov dword [esi+P_CLASE], c_wifi
        mov dword [esi+P_TEXTO], wf_t_tit
        mov dword [esi+P_ESTILO], E_TITULO | E_CERRAR
        mov eax, [vid_ancho]        ; acabando sobre el icono...
        sub eax, [vol_x]
        add eax, BT_DESP - 4
        sub eax, [edi]
        cmp eax, 4
        jge .px
        mov eax, 4
.px:
        mov [esi+P_X], eax
        mov eax, [area_y1]          ; ...y encima de la barra
        sub eax, [edi+4]
        sub eax, SOMBRA + 3
        mov [esi+P_Y], eax
        mov eax, [edi]
        mov [esi+P_AN], eax
        mov eax, [edi+4]
        mov [esi+P_AL], eax
        mov dword [esi+P_ID], 0
        xor ebp, ebp
        call ven_crear
        add esp, P_TAM
        mov [wf_ven], eax
.x:
        popad
        ret

wf_proc:
        cmp eax, WM_PAINT
        je wf_pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_DESTROY
        je .destruir
        jmp ven_defecto
.crear:
        mov [wf_ven], ebp
        movzx eax, byte [wf_modo]
        imul eax, 12
        mov esi, [wf_medidas + eax + 8]
        call ven_lista
        mov dword [wf_msg], 0
        cmp byte [wf_modo], 2
        jne .nada
        mov ecx, 35
        call ven_hija_id
        mov [wf_lis], eax
        mov ecx, 36
        call ven_hija_id
        mov [wf_cla], eax
        push ebp
        mov ebp, [wf_lis]
        call ven_enfocar
        pop ebp
        call wf_lista               ; las que ya se conocen...
        cmp dword [wf_nredes], 0
        jne .nada
        call wf_buscar              ; ...y si no hay ninguna, se busca
        jmp .nada
.tecla:
        cmp ecx, TC_ESC
        jne .nada
        call ven_destruir
        jmp .nada
.orden:
        cmp cx, WF_ID_CONFIG
        jne .o1
        call ven_destruir           ; Configuración › Red (el 7.º apartado)
        mov edx, 7
        call ctx_config
        jmp .nada
.o1:
        cmp cx, WF_ID_BUSCAR
        jne .o2
        call wf_buscar
        jmp .rep
.o2:
        cmp cx, WF_ID_CONECTAR
        jne .o3
        call wf_conectar
        jmp .rep
.o3:
        cmp cx, WF_ID_DESCON
        je .o3b
        cmp cx, WF_ID_OLVIDAR
        jne .nada
.o3b:
        push ecx
        cmp byte [wf_modo], 2
        jne .o3c
        mov ecx, WLL_DESCON
        call wf_nuc
.o3c:
        mov byte [wf_fase], 0
        call wf_clave_borrar
        mov dword [wf_msg], wf_m_descon
        pop ecx
        cmp cx, WF_ID_OLVIDAR
        jne .rep
        mov byte [wf_ssid_n], 0
        mov dword [wf_msg], wf_m_olvido
        call wf_ini_guardar
.rep:
        mov dword [wf_tic], 0       ; que se mire ya
        call ven_invalidar
        call wf_inv_lista
        jmp .nada
.destruir:
        mov dword [wf_ven], 0
        mov dword [wf_lis], 0
        mov dword [wf_cla], 0
        call wf_clave_borrar
        cmp byte [wf_fase], WF_F_CLAVE  ; (a medio preparar la clave: se deja;
                                    ; buscando o conectando, sigue sola)
        jne .nada
        mov byte [wf_fase], 0
.nada:
        xor eax, eax
        ret

; (interna) EDI = texto: «nombre» de la red ESI con ECX letras (lo que no es
; ASCII, «?»)
wf_pon_ssid:
        mov al, 0xAE                ; «
        stosb
        jecxz .f
.l:
        lodsb
        cmp al, 32
        jb .q
        cmp al, 127
        jb .ok
.q:
        mov al, '?'
.ok:
        stosb
        loop .l
.f:
        mov al, 0xAF                ; »
        stosb
        ret

; (interna) ESI = 4 octetos -> «a.b.c.d» en EDI
wf_pon_ip:
        push ecx
        mov ecx, 4
.b:
        movzx eax, byte [esi]
        inc esi
        call pon_num
        dec ecx
        jz .x
        mov al, '.'
        stosb
        jmp .b
.x:
        pop ecx
        ret

; (interna) wf_txt (acabado aquí en EDI), en EAX=x EBX=y
wf_texto:
        mov byte [edi], 0
        mov esi, wf_txt
        jmp dc_texto

wf_pintar:
        call dc_pintar
        COLOR C_TINTA
        mov byte [gdi_negrita], 1
        mov edi, wf_txt
        movzx eax, byte [wf_modo]
        cmp al, 1
        je .cable
        ja .wifi
        ; ---- sin red ----
        mov esi, wf_t_sinred
        mov eax, 10
        mov ebx, 8
        call dc_texto
        mov byte [gdi_negrita], 0
        mov esi, wf_t_sinred2
        mov ebx, 30
        call dc_texto
        mov esi, wf_t_sinred3
        mov ebx, 48
        call dc_texto
        jmp .f
.cable:
        mov esi, wf_t_cable
        mov eax, 10
        mov ebx, 8
        call dc_texto
        mov byte [gdi_negrita], 0
        cmp byte [wf_hspred], 0
        jne .hspred
        mov esi, wf_t_otro
        mov ebx, 30
        call dc_texto
        mov esi, wf_t_otro2
        mov ebx, 48
        call dc_texto
        jmp .f
.hspred:
        mov esi, wf_t_enl
        call pon_cad
        mov esi, wf_t_enl1
        mov al, [wf_enlace]
        cmp al, 1
        je .e
        mov esi, wf_t_enl0
        test al, al
        jz .e
        mov esi, wf_t_enlx
.e:
        call pon_cad
        mov eax, 10
        mov ebx, 30
        call wf_texto
        mov edi, wf_txt
        mov esi, wf_t_ip
        call pon_cad
        mov esi, wf_t_noip
        test byte [wf_cfg], 1
        jz .noip
        mov esi, wf_cfg + 2
        call wf_pon_ip
        jmp .ip
.noip:
        call pon_cad
.ip:
        mov eax, 10
        mov ebx, 48
        call wf_texto
        test byte [wf_cfg], 1
        jz .f
        mov edi, wf_txt
        mov esi, wf_t_gw
        call pon_cad
        mov esi, wf_cfg + 10
        call wf_pon_ip
        mov eax, 10
        mov ebx, 66
        call wf_texto
        jmp .f
        ; ---- Wi-Fi ----
.wifi:
        mov al, [wf_fase]
        mov esi, wf_t_busca
        cmp al, WF_F_BUSCA
        je .l1
        mov esi, wf_t_prepara
        cmp al, WF_F_CLAVE
        je .l1
        cmp al, WF_F_CONECTA
        je .conando
        mov al, [wf_info+WI_ESTADO]
        cmp al, 5
        je .conectado
        test al, al
        jnz .conando2
        mov esi, wf_t_sincon
.l1:
        call pon_cad
        jmp .l1f
.conando:
        mov esi, wf_t_conando       ; la que se ha pedido
        call pon_cad
        mov esi, wf_ssid
        movzx ecx, byte [wf_ssid_n]
        call wf_pon_ssid
        mov eax, '...'
        stosd
        dec edi
        jmp .l1f
.conando2:
        mov esi, wf_t_conando       ; la que lleva el controlador (reconectando)
        call pon_cad
        mov esi, wf_info+WI_SSID
        movzx ecx, byte [wf_info+WI_SSID_N]
        call wf_pon_ssid
        mov eax, '...'
        stosd
        dec edi
        jmp .l1f
.conectado:
        mov esi, wf_t_con
        call pon_cad
        mov esi, wf_info+WI_SSID
        movzx ecx, byte [wf_info+WI_SSID_N]
        call wf_pon_ssid
.l1f:
        mov eax, 10
        mov ebx, 6
        call wf_texto
        mov byte [gdi_negrita], 0
        ; la segunda línea: el aviso, o la señal y la IP, o por qué no conecta
        mov edi, wf_txt
        mov esi, [wf_msg]
        test esi, esi
        jnz .l2c
        cmp byte [wf_fase], 0
        jne .l2v
        mov al, [wf_info+WI_ESTADO]
        cmp al, 5
        je .senal
        test al, al
        jnz .l2v
        mov esi, wf_t_elija
        cmp byte [wf_info+WI_SSID_N], 0
        je .l2c
        movzx eax, byte [wf_info+WI_FALLO]  ; quiere una red y no está
        cmp eax, 10
        jbe .fa
        xor eax, eax
.fa:
        test eax, eax
        jz .l2c
        mov esi, [wf_t_fallo + eax*4]
.l2c:
        call pon_cad
        jmp .l2
.senal:
        mov esi, wf_t_senal
        call pon_cad
        movsx eax, byte [wf_info+WI_SENAL]
        test eax, eax
        jns .pos
        mov byte [edi], '-'
        inc edi
        neg eax
.pos:
        call pon_num
        mov esi, wf_t_dbm
        call pon_cad
        mov esi, wf_t_noipw
        test byte [wf_cfg], 1
        jz .l2c
        mov esi, wf_t_ipw
        call pon_cad
        mov esi, wf_cfg + 2
        call wf_pon_ip
.l2:
        mov eax, 10
        mov ebx, 24
        call wf_texto
.l2v:
        ; la señal también en rayitas, a la derecha de la primera línea
        cmp byte [wf_info+WI_ESTADO], 5
        jne .clave
        cmp byte [wf_fase], 0
        jne .clave
        mov byte [wf_c_lleno], C_TINTA
        mov byte [wf_c_hueco], C_ARENA_OSC
        call ven_cli
        lea esi, [ecx-26]
        movsx eax, byte [wf_info+WI_SENAL]
        call wf_nivel
        mov ecx, eax
        mov eax, esi
        mov ebx, 20
        call wf_rayitas
.clave:
        COLOR C_TINTA
        mov eax, 10
        mov ebx, 167
        mov esi, wf_t_clave
        call dc_texto
.f:
        mov byte [gdi_negrita], 0
        call dc_cerrar
        xor eax, eax
        ret

; ---- la lista de redes (una hija: se maneja con el ratón y con las flechas) ----
wfl_proc:
        cmp eax, WM_PAINT
        je wfl_pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        jmp ven_defecto
.doble:
        call .fila
        jc .nada
        jmp .conecta
.pulsa:
        call .fila
        jmp .nada
.fila:
        call raton_xy
        jc .no
        sub ebx, 2
        js .no
        mov eax, ebx
        xor edx, edx
        mov ecx, WF_FILA
        div ecx
        add eax, [wf_prim]
        cmp eax, [wf_nredes]
        jae .no
        call .elige
        clc
        ret
.no:
        stc
        ret
.elige:
        cmp eax, [wf_sel]
        je .el
        mov [wf_sel], eax
        call wf_clave_borrar        ; (la contraseña era para la otra)
        mov dword [wf_msg], 0
.el:
        push ebp
        mov ebp, [wf_ven]
        call ven_invalidar
        pop ebp
        call ven_invalidar
        ret
.tecla:
        mov eax, [wf_sel]
        cmp ecx, TC_ARRIBA
        jne .t1
        dec eax
        jns .mueve
        jmp .nada
.t1:
        cmp ecx, TC_ABAJO
        jne .t2
        inc eax
        cmp eax, [wf_nredes]
        jb .mueve
        jmp .nada
.t2:
        cmp ecx, TC_INTRO
        jne ven_defecto             ; (Tab, Esc: lo de siempre)
.conecta:
        push ebp
        mov ebp, [wf_ven]
        mov eax, WM_COMMAND
        mov ecx, WF_ID_CONECTAR
        xor edx, edx
        call msg_poner
        pop ebp
        jmp .nada
.mueve:
        cmp eax, [wf_prim]          ; que se vea
        jge .m1
        mov [wf_prim], eax
.m1:
        mov edx, [wf_prim]
        add edx, WF_FILAS
        cmp eax, edx
        jl .m2
        lea edx, [eax - WF_FILAS + 1]
        mov [wf_prim], edx
.m2:
        call .elige
        jmp .nada
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret

wfl_pintar:
        call dc_pintar
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        call dc_marco
        cmp ebp, [foco]             ; con el foco, el filete doble
        jne .sinfoco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        call dc_marco
.sinfoco:
        cmp dword [wf_nredes], 0
        jne .hay
        COLOR C_ARENA_OSC
        mov esi, wf_t_busca
        cmp byte [wf_fase], WF_F_BUSCA
        je .vacia
        mov esi, wf_t_ninguna
.vacia:
        mov eax, 8
        mov ebx, 6
        call dc_texto
        jmp .x
.hay:
        mov ecx, [wf_prim]
        mov ebx, 2
.f:
        cmp ecx, [wf_nredes]
        jae .x
        mov eax, ebx
        sub eax, 2
        cmp eax, WF_FILAS * WF_FILA
        jae .x
        call wfl_fila
        inc ecx
        add ebx, WF_FILA
        jmp .f
.x:
        mov byte [gdi_negrita], 0
        call dc_cerrar
        xor eax, eax
        ret

; (interna) la red ECX en la fila que empieza en y = EBX
wfl_fila:
        pushad
        mov edi, ebx
        call wf_es_la_conectada     ; ESI = la red; la conectada, en negrita
        setc byte [gdi_negrita]
        mov byte [wf_c_lleno], C_TINTA
        mov byte [wf_c_hueco], C_ARENA_OSC
        COLOR C_TINTA
        cmp ecx, [wf_sel]
        jne .t
        COLOR C_ANIL                ; la elegida: en añil, con letra blanca
        mov eax, 2
        mov ebx, edi
        mov ecx, [ebp+V_AN]
        sub ecx, 4
        mov edx, WF_FILA
        call dc_rect
        mov byte [wf_c_lleno], C_BLANCO
        mov byte [wf_c_hueco], C_ARENA
        COLOR C_BLANCO
.t:
        push esi
        push edi
        movzx ecx, byte [esi+WR_SSID_N]
        cmp ecx, 24
        jbe .n
        mov ecx, 24
.n:
        add esi, WR_SSID
        mov edi, wf_txt
        call wf_pon_ssid
        mov byte [edi-1], 0         ; (sin las comillas)
        pop edi
        mov eax, 8
        lea ebx, [edi+1]
        mov esi, wf_txt + 1
        call dc_texto
        mov byte [gdi_negrita], 0
        pop esi
        movsx eax, byte [esi+WR_SENAL]  ; la señal
        call wf_nivel
        mov ecx, eax
        mov eax, 212
        lea ebx, [edi+15]
        call wf_rayitas
        mov al, [esi+WR_SEG]        ; la seguridad
        push esi
        mov esi, wf_t_abierta
        test al, al
        jz .s
        mov esi, wf_t_wpa2
        cmp al, 3
        je .s
        mov esi, wf_t_noadm
.s:
        mov eax, 236
        lea ebx, [edi+1]
        call dc_texto
        pop esi
        popad
        ret

; ---- el cuadro de la contraseña: enseña asteriscos (la contraseña, en
; wf_clave; no va a ningún sitio más que al núcleo, y se borra al usarla) ----
wfc_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .inv
        cmp eax, WM_KILLFOCUS
        je .inv
        jmp ven_defecto
.car:
        mov eax, [wf_clave_n]
        cmp eax, 64
        jae .nada
        mov [wf_clave + eax], cl
        inc dword [wf_clave_n]
        mov dword [wf_msg], 0
        jmp .inv
.tecla:
        cmp ecx, TC_RETRO
        jne .t1
        mov eax, [wf_clave_n]
        test eax, eax
        jz .nada
        dec eax
        mov [wf_clave_n], eax
        mov byte [wf_clave + eax], 0
        jmp .inv
.t1:
        cmp ecx, TC_INTRO
        jne ven_defecto
        push ebp
        mov ebp, [wf_ven]
        mov eax, WM_COMMAND
        mov ecx, WF_ID_CONECTAR
        xor edx, edx
        call msg_poner
        pop ebp
        jmp .nada
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [ebp+V_AN]
        mov edx, [ebp+V_AL]
        call dc_marco
        mov ecx, [ebp+V_AN]         ; los asteriscos que caben
        sub ecx, 12
        shr ecx, 3
        cmp ecx, [wf_clave_n]
        jbe .c
        mov ecx, [wf_clave_n]
.c:
        mov edi, wf_txt
        push ecx
        mov al, '*'
        rep stosb
        mov byte [edi], 0
        pop ecx
        COLOR C_TINTA
        mov eax, 5
        mov ebx, 3
        mov esi, wf_txt
        call dc_texto
        cmp ebp, [foco]
        jne .x
        lea eax, [ecx*8+4]          ; el cursor
        mov ebx, 3
        mov ecx, 2
        mov edx, 16
        COLOR C_TERRACOTA
        call dc_rect
.x:
        call dc_cerrar
        xor eax, eax
        ret
