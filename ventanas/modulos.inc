; =============================================================================
;  Hispa Ventanas - programas aparte: los módulos .HVP
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un .HVP («programa de Hispa Ventanas») es un binario plano de 32 bits con
;  una cabecera (ventanas/hvp.inc, docs/VENTANAS.md), hecho con NASM y
;  tools/mkhvp.py. Se carga en un bloque de memoria extendida propio (función
;  0501h del DPMI), se reubica (la tabla de reubicaciones va al final del
;  archivo), recibe la tabla de la interfaz (api.inc) y corre como una tarea
;  más: sus ventanas son ventanas normales.
;
;    hvp_abrir    EBX=acceso directo cuyo programa es un .HVP: lo carga y lo
;                 pone en marcha (si ya está abierto, trae su ventana delante)
;    hvp_limpiar  (sistema) suelta la memoria de los módulos que han acabado
;    hvp_icono    EBX=acceso ESI=icono -> ESI=el icono propio si es un .HVP
;    hvp_buscar   cen_arch=carpeta acabada en «\», ECX=0 contar / 1 crear los
;                 accesos que falten en el grupo EDX (-1: cada uno en el
;                 que dice su cabecera, «Juegos» o «Accesorios»)
;                 -> EAX=cuántos .HVP hay / cuántos accesos ha creado
;    mem_bloque, mem_bloque_soltar   memoria extendida para un módulo
;    es_hvp       ESI=ruta: ZF=1 si acaba en .HVP
;  Al entrar en el módulo: EBX=su base (la cabecera), ESI=su ruta completa,
;  EDI=los parámetros del acceso directo, ECX=entradas de la interfaz que hay.
; =============================================================================

H_FIRMA     equ 0                   ; "HVP1"
H_API       equ 6                   ; entradas de la interfaz que usa
H_ARCH      equ 8                   ; tamaño del archivo
H_MEM       equ 12                  ; memoria que necesita
H_ENTRADA   equ 16
H_IMG       equ 20                  ; tamaño de la imagen; detrás, las reubicaciones
H_NREUB     equ 24
H_GRUPO     equ 28                  ; 0 juego, 1 accesorio
H_NOMBRE    equ 32
H_ICONO     equ 64
H_TABLA     equ 576                 ; aquí se copia la tabla de la interfaz
HVP_MAX     equ 6
HS_USO      equ 0                   ; 0 libre, 1 en marcha, 2 terminado
HS_MANGO    equ 4
HS_BASE     equ 8
HS_TAREA    equ 12
HS_RUTA     equ 16
HS_PARAM    equ 96
HS_TAM      equ 168
HVI_N       equ 16                  ; iconos de módulos que se recuerdan

; la tabla de la interfaz, en el orden de api.inc
%macro HVF 1
        dd %1
%endmacro
api_tabla:
        HV_LISTA
API_N       equ ($ - api_tabla) / 4
%unmacro HVF 1

t_ext_hvp   db ".HVP", 0
t_hvp_pat   db "*.HVP", 0
t_hvp_dir   db "JUEGOS\", 0
t_hvp_dir2  db "PROGS\", 0
t_hvp_mal   db "No se ha podido cargar el programa.", 0
t_hvp_viejo db "Este programa necesita un Hispa Ventanas más nuevo.", 0
t_hvp_muchos db "Hay demasiados programas abiertos.", 0

; ---- lo que la interfaz tiene de propio ----
dc_color:
        mov [gdi_color], eax
        ret
dc_negrita:
        mov [gdi_negrita], eax
        ret
; EAX=n -> EAX=un número de 0 a n-1
azar:
        push edx
        push eax
        imul eax, [azar_x], 1103515245
        add eax, 12345
        mov [azar_x], eax
        shr eax, 8
        xor edx, edx
        div dword [esp]
        mov eax, edx
        add esp, 4
        pop edx
        ret
azar_semilla:
        mov [azar_x], eax
        ret
tec_estado:
        movzx eax, byte [tec_mayus]
        ret
; -> EAX=ancho EBX=alto de la pantalla
pantalla:
        mov eax, [vid_ancho]
        mov ebx, [vid_alto]
        ret

; [Hispa-Nes] da el turno a los demás programas y al sistema (ratón, teclas,
; pantalla) y vuelve en cuanto le toca otra vez, sin esperar a un mensaje:
; para los programas que tienen algo que hacer todo el rato (un emulador)
ceder:
        mov byte [actividad], 1
        jmp tar_ceder
; [CAMPANITA] desde aquí hasta mem_bloque: snd_pcm pasa por snd_pcm2 (el
; búfer grande), snd_orden, snd_info, snd_campana y cam_bomba
; [Hispa-Nes] ESI=muestras ECX=octetos (2048 como mucho) EDX=Hz AL=formato
; (bit 0: 16 bits, bit 1: estéreo): las pone en la cola de HSPSND, que las
; hace sonar a continuación de las anteriores. CF=1 si no hay sonido
snd_pcm:
        push ecx
        cmp ecx, DOS_TRANS
        jbe .c
        mov ecx, DOS_TRANS
.c:
        call snd_pcm2
        pop ecx
        ret
; [Reproductor de sonido] lo mismo con trozos de hasta SND_TB octetos (lo que
; HD Audio y AC'97 tocan de una vez: 2.048 tramas de 16 bits en estéreo). El
; búfer de paso (snd_tb) está en este programa, pero más allá de sus primeros
; 64 KB: HSPSND lo recibe con su propio segmento (rm_snd2, snd_tbseg)
snd_pcm2:
        push eax
        call tics
        mov [snd_ult], eax          ; (mientras un programa suena, no hay
        pop eax                     ; campanita; si sonaba, se deja)
        mov dword [cam_pos], 0
snd_trozo:
        cmp byte [snd_hay], 0
        je .no
        pushad
        cmp ecx, SND_TB
        jbe .c
        mov ecx, SND_TB
.c:
        mov [rmcs+0x18], ecx
        mov [rmcs+0x14], edx
        mov edi, snd_tb
        mov edx, edi
        and edx, 15
        mov [rmcs+4], edx           ; SI
        mov edx, edi
        shr edx, 4
        add dx, [rmseg]
        mov [snd_tbseg], dx         ; DS
        rep movsb
        mov ah, 0x01
        movzx eax, ax
        mov [rmcs+0x1C], eax
        mov word [rmcs+0x2A], rm_snd2
        call rm_llamar
        popad
        clc
        ret
.no:
        stc
        ret
; [Reproductor de sonido] AH=función de HSPSND (02h esperar, 03h parar, 06h
; leer el volumen, 08h cuántos trozos caben...) AL=dato -> EAX como lo deja. CF=1 si no hay sonido.
; (El volumen y el silencio se cambian con aj_poner, que además los apunta)
snd_orden:
        cmp byte [snd_hay], 0
        je snd_trozo.no
        movzx eax, ax
        call snd_llamar
        clc
        ret
; [Reproductor de sonido] -> EAX=la tarjeta que lleva HSPSND (0 ninguna, 1
; Sound Blaster de 8 bits, 2 Sound Blaster 16, 3 AC'97, 4 HD Audio; -1 si
; HSPSND no está cargado: ESI y EDX no se tocan), ESI=su nombre (acabado en
; 0) y EDX=la versión de la interfaz de HSPSND (3: tiene la función 08h)
snd_info:
        or eax, -1
        cmp dword [snd_entrada], 0
        je .x
        xor eax, eax
        call snd_llamar
        movzx edx, word [rmcs+0x10]
        movzx esi, word [rmcs+0x22] ; ES:DI = el nombre
        shl esi, 4
        movzx eax, word [rmcs]
        add esi, eax
        add esi, [cero]
        movzx eax, byte [rmcs+0x1C]
.x:
        ret

; ---- [CAMPANITA] la campanita del volumen ----
; Es \HISPADOS\CAMPANA.WAV (tools/mksample.py: 8 bits, mono, cabecera de 44
; octetos), que se lee la primera vez. Suena por trozos de lo que la tarjeta
; toca de una vez (así HSPSND no mete silencios): 2.048 muestras a 48.000 Hz
; con HD Audio y AC'97 (se repiten muestras para llegar) y 1.024 o 4.096 a su
; frecuencia con las Sound Blaster. cam_bomba, desde el bucle del sistema,
; pregunta a HSPSND cuántos caben sin esperar (su función 08h, de la versión
; 3) y manda esos: la ventana nunca espera al sonido.
CAM_MAX     equ 4096
t_campana   db "CAMPANA.WAV", 0
; toca la campanita con el volumen que hay (nada si está en silencio, si no
; hay sonido o si un programa está sonando). Si ya sonaba, empieza otra vez
snd_campana:
        pushad
        cmp byte [snd_hay], 0
        je near .x
        cmp word [snd_ver], 3
        jb near .x
        cmp dword [vol_mudo], 0
        jne near .x
        call tics
        mov [cam_tic], eax
        sub eax, [snd_ult]
        cmp eax, 6
        jb near .x
        cmp dword [cam_n], 0
        jne .ya
        inc dword [cam_n]           ; (si no está, no se busca más)
        mov edi, cam_dat
        mov esi, dir_propia
        call pon_cad
        mov esi, t_campana
        call pon_cad
        mov byte [edi], 0
        mov esi, cam_dat
        xor al, al
        call dos_abrir
        jc .x
        mov ebx, eax
        mov edi, cam_dat
        mov ecx, CAM_MAX
        call dos_leer
        jc .mal
        cmp dword [cam_dat], 'RIFF'
        jne .mal
        mov edx, [cam_dat+24]       ; (Hz: con cualquier cosa no se divide)
        sub edx, 4000
        cmp edx, 44000
        ja .mal
        mov [cam_n], eax
.mal:
        call dos_cerrar
.ya:
        cmp dword [cam_n], 44
        jbe .x
        cmp dword [cam_pos], 0
        je .libre
        mov eax, 0x0300             ; la anterior, fuera
        call snd_llamar
.libre:
        mov ebx, 48000              ; EBX=Hz ECX=muestras de un trozo
        mov ecx, 2048
        cmp byte [snd_tipo], 3
        jae .t
        mov ebx, [cam_dat+24]
        mov ecx, 1024
        cmp byte [snd_tipo], 2
        je .t
        shl ecx, 2
.t:
        mov [cam_hz], ebx
        mov [cam_blq], ecx
        mov dword [cam_cola], 8     ; el silencio de detrás: 8 trozos (un
        cmp ecx, 4096               ; tercio de segundo largo), o 2 de los
        jb .c8                      ; de 4.096 muestras
        mov dword [cam_cola], 2
.c8:
        mov eax, [cam_dat+24]       ; el paso: Hz del archivo / Hz de la tarjeta
        shl eax, 16
        xor edx, edx
        div ebx
        mov [cam_paso], eax
        mov dword [cam_pos], 44 << 16
        mov byte [actividad], 1
.x:
        popad
        ret

; (bucle del sistema) manda los trozos de la campanita que quepan. Detrás
; van unos de silencio (cam_cola) y, cuando ha sonado todo, se le dice a
; HSPSND que pare: HD Audio da vueltas a sus cuatro trozos y, si no,
; repetiría el final de la campanita sin parar
cam_bomba:
        cmp dword [cam_pos], 0
        je .r
        pushad
        call tics                   ; (si HSPSND no acaba, se deja)
        sub eax, [cam_tic]
        cmp eax, 40
        ja .para
.l:
        mov eax, 0x0800             ; ¿cabe otro trozo sin esperar?
        call snd_llamar
        test al, al
        jz .x
        mov ebx, [cam_pos]
        mov edx, ebx
        shr edx, 16
        cmp edx, [cam_n]
        jb .da
        cmp dword [cam_cola], 0
        je .fin
        dec dword [cam_cola]
.da:
        mov ecx, [cam_blq]
        mov edi, snd_tb
.m:
        mov edx, ebx
        shr edx, 16
        mov al, 0x80                ; detrás del final, silencio
        cmp edx, [cam_n]
        jae .s
        mov al, [cam_dat+edx]
        add ebx, [cam_paso]
.s:
        stosb
        dec ecx
        jnz .m
        mov [cam_pos], ebx
        mov esi, snd_tb
        mov ecx, [cam_blq]
        mov edx, [cam_hz]
        xor eax, eax                ; 8 bits, un canal
        call snd_trozo
        jmp .l
.fin:
        cmp al, 4                   ; ¿ha sonado todo? (la cola, vacía)
        jb .x
.para:
        mov eax, 0x0300
        call snd_llamar
        mov dword [cam_pos], 0
.x:
        popad
.r:
        ret

; ECX=octetos -> EAX=puntero EDX=mango; CF=1 si no hay memoria
mem_bloque:
        push ebx
        push ecx
        push esi
        push edi
        mov ebx, ecx
        shr ebx, 16
        mov ax, 0x0501
        int 0x31
        jc .x
        shl ebx, 16
        mov bx, cx
        sub ebx, [base]
        mov eax, ebx
        mov edx, esi
        shl edx, 16
        mov dx, di
        clc
.x:
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret
; EDX=mango de mem_bloque
mem_bloque_soltar:
        pushad
        mov edi, edx
        mov esi, edx
        shr esi, 16
        mov ax, 0x0502
        int 0x31
        popad
        ret

; (interna) ECX=0 contar / 1 crear los accesos de los módulos de las dos
; carpetas que hay al lado de VENTANAS.COM (JUEGOS y PROGS) -> EAX
hvp_las_dos:
        push ebx
        push edx
        push esi
        mov esi, t_hvp_dir
        call .una
        mov ebx, eax
        mov esi, t_hvp_dir2
        call .una
        add eax, ebx
        pop esi
        pop edx
        pop ebx
        ret
.una:
        push edi
        push esi
        mov edi, cen_arch
        mov esi, dir_propia
        call pon_cad
        pop esi
        call pon_cad
        mov byte [edi], 0
        pop edi
        or edx, -1
        jmp hvp_buscar

es_hvp:
        push eax
        push esi
        push edi
        call cad_largo
        sub eax, 4
        jb .x
        add esi, eax
        mov edi, t_ext_hvp
        call cad_igual_i
.x:
        pop edi
        pop esi
        pop eax
        ret

hvp_abrir:
        pushad
        lea esi, [ebx+AC_PROG]
        mov edx, hvp_tabla
        xor ebp, ebp
        mov ecx, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 0
        jne .b1
        mov ebp, edx
        jmp .b2
.b1:
        lea edi, [edx+HS_RUTA]
        call cad_igual_i
        je .ya
.b2:
        add edx, HS_TAM
        dec ecx
        jnz .b
        test ebp, ebp
        jz .muchos
        lea edi, [ebp+HS_RUTA]
        mov ecx, 79
        call cad_copiar
        lea esi, [ebx+AC_PARAM]
        lea edi, [ebp+HS_PARAM]
        call cad_copiar
        lea esi, [ebp+HS_RUTA]
        xor al, al
        call dos_abrir
        jc .mal
        mov ebx, eax
        mov edi, cen_txt            ; la cabecera
        mov ecx, 64
        call dos_leer
        jc .malc
        cmp dword [edi], 'HVP1'
        jne .malc
        mov esi, t_hvp_viejo
        cmp word [edi+H_API], API_N
        ja .malc2
        push ebx
        mov ecx, [edi+H_MEM]
        mov ebx, ecx
        shr ebx, 16
        mov ax, 0x0501
        int 0x31
        jc .malm
        mov [ebp+HS_MANGO], di
        mov [ebp+HS_MANGO+2], si
        shl ebx, 16
        mov bx, cx
        sub ebx, [base]
        mov [ebp+HS_BASE], ebx
        mov dword [ebp+HS_TAREA], 0
        mov dword [ebp+HS_USO], 2   ; (si algo falla, hvp_limpiar la suelta)
        mov edi, ebx
        pop ebx
        xor edx, edx                ; el archivo entero, a su memoria
        xor al, al
        call dos_puntero
        mov ecx, [cen_txt+H_MEM]
        call dos_leer
        jc .malc
        cmp eax, [edi+H_ARCH]
        jb .malc
        call dos_cerrar
        mov edx, [edi+H_IMG]
        lea esi, [edi+edx]          ; reubicar: a cada puntero se le suma la base
        mov ecx, [edi+H_NREUB]
.r:
        test ecx, ecx
        jz .rf
        lodsd
        cmp eax, edx
        jae .r1
        add [edi+eax], edi
.r1:
        dec ecx
        jmp .r
.rf:
        push edi
        mov ecx, [edi+H_MEM]        ; lo que no viene en el archivo, a cero
        sub ecx, edx
        add edi, edx
        xor al, al
        rep stosb
        pop edi
        movzx ecx, word [edi+H_API]
        add edi, H_TABLA
        mov esi, api_tabla
        rep movsd
        mov dword [ebp+HS_USO], 1
        mov eax, hvp_tarea
        call tar_crear
        mov [ebp+HS_TAREA], eax
        test eax, eax
        jnz .x
        mov dword [ebp+HS_USO], 2
.muchos:
        mov esi, t_hvp_muchos
        jmp .aviso
.ya:
        mov eax, [edx+HS_TAREA]     ; ya está abierto: su ventana, delante
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], eax
        jne .vs
        cmp dword [ebp+V_PADRE], 0
        jne .vs
        mov eax, 1                  ; [Configuración] (por si está oculta)
        call ven_mostrar
        mov eax, 4
        call ven_mostrar
        call ven_activar
        jmp .x
.vs:
        add ebp, V_TAM
        dec ecx
        jnz .v
        jmp .x
.malm:
        pop ebx
.malc:
        mov esi, t_hvp_mal
.malc2:
        call dos_cerrar
        jmp .aviso
.mal:
        mov esi, t_hvp_mal
.aviso:
        mov ebp, [esp+8]            ; (la ventana desde la que se abre)
        call cen_aviso
.x:
        popad
        ret

; la entrada de la tarea de un módulo
hvp_tarea:
        mov edx, hvp_tabla - HS_TAM
        mov eax, [tar_act]
.b:
        add edx, HS_TAM
        cmp dword [edx+HS_USO], 1
        jne .b
        cmp [edx+HS_TAREA], eax
        jne .b
        push edx
        mov ebx, [edx+HS_BASE]
        lea esi, [edx+HS_RUTA]
        lea edi, [edx+HS_PARAM]
        mov ecx, API_N
        mov eax, [ebx+H_ENTRADA]
        add eax, ebx
        call eax
        pop edx
        mov dword [edx+HS_USO], 2
        ret

hvp_limpiar:
        pushad
        mov edx, hvp_tabla
        mov ebp, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 2
        jne .s
        mov di, [edx+HS_MANGO]
        mov si, [edx+HS_MANGO+2]
        mov ax, 0x0502
        int 0x31
        mov dword [edx+HS_USO], 0
.s:
        add edx, HS_TAM
        dec ebp
        jnz .b
        popad
        ret

hvp_icono:
        pushad
        lea esi, [ebx+AC_PROG]
        call es_hvp
        jne .x
        xor ecx, ecx
.b:
        cmp [hvi_de+ecx*4], ebx
        je .hay
        inc ecx
        cmp ecx, HVI_N
        jb .b
        mov ecx, [hvi_sig]          ; no está: se lee del archivo
        inc dword [hvi_sig]
        and ecx, HVI_N - 1
        mov dword [hvi_de+ecx*4], 0
        xor al, al
        call dos_abrir
        jc .x
        push ebx
        mov ebx, eax
        mov edi, ecx
        shl edi, 9
        add edi, hvi_buf
        mov edx, H_ICONO
        xor al, al
        call dos_puntero
        push ecx
        mov ecx, 512
        call dos_leer
        pop ecx
        call dos_cerrar
        pop ebx
        cmp eax, 512
        jne .x
        mov [hvi_de+ecx*4], ebx
.hay:
        shl ecx, 9
        add ecx, hvi_buf
        mov [esp+4], ecx
.x:
        popad
        ret

hvp_buscar:
        pushad
        xor ebp, ebp
        mov edi, cen_txt
        mov esi, cen_arch
        call pon_cad
        mov esi, t_hvp_pat
        call pon_cad
        mov [edi], ch               ; (ECX es 0 o 1)
        mov esi, cen_txt
        push ecx
        xor ecx, ecx
        call dos_primero
        pop ecx
.f:
        jc .fin
        test ecx, ecx
        jz .cuenta
        mov edi, cen_txt            ; la ruta completa
        mov esi, cen_arch
        call pon_cad
        mov esi, dos_dta + 0x1E
        call pon_cad
        mov [edi], ch
        mov esi, cen_txt
        mov eax, [ac_tabla]         ; ¿ya tiene acceso directo?
        mov ebx, MAX_AC
.a:
        cmp byte [eax+AC_USO], 0
        je .as
        lea edi, [eax+AC_PROG]
        call cad_igual_i
        je .sig
.as:
        add eax, AC_TAM
        dec ebx
        jnz .a
        xor al, al                  ; la cabecera: si no es un módulo, no cuenta
        call dos_abrir
        jc .sig
        push ecx
        mov ebx, eax
        mov edi, cen_lista
        mov [edi], ebp              ; (que no valga una firma de antes)
        mov ecx, 64
        call dos_leer
        call dos_cerrar
        pop ecx
        cmp dword [edi], 'HVP1'
        jne .sig
        test edx, edx
        jns .g2
        mov ebx, tg_juegos          ; su grupo: «Juegos» o, si la cabecera lo
        cmp byte [edi+H_GRUPO], 1   ; dice, «Accesorios» (si no está, se crea)
        jne .g0
        mov ebx, tg_accesorios
.g0:
        mov eax, [gr_tabla]
        xor edx, edx
.g:
        cmp byte [eax+GR_USO], 0
        je .gs
        lea esi, [eax+GR_NOMBRE]
        mov edi, ebx
        call cad_igual_i
        je .g2
.gs:
        add eax, GR_TAM
        inc edx
        cmp edx, MAX_GR
        jb .g
        call gr_nuevo
        test eax, eax
        jz .fin
        lea edi, [eax+GR_NOMBRE]
        mov esi, ebx
        call pon_cad
.g2:
        call ac_nuevo
        test eax, eax
        jz .fin
        mov byte [eax+AC_ICONO], 1
        push ecx
        mov esi, cen_txt
        lea edi, [eax+AC_PROG]
        mov ecx, 79
        call cad_copiar
        mov esi, cen_lista + H_NOMBRE   ; el nombre: el de la cabecera
        lea edi, [eax+AC_NOMBRE]
        mov ecx, 31
        call cad_copiar
        pop ecx
.cuenta:
        inc ebp
.sig:
        mov edx, [esp+20]           ; (el grupo que se pidió: cada módulo, al suyo)
        call dos_siguiente
        jmp .f
.fin:
        mov [esp+28], ebp
        popad
        ret
