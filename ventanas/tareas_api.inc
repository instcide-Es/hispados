; =============================================================================
;  Hispa Ventanas - [TAREAS] lo que pide el Administrador de tareas
;  (programas/tareas.asm, TAREAS.HVP)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Todo lo del núcleo para el Administrador de tareas está en este archivo;
;  en los demás de ventanas/ solo hay «ganchos» de una o dos líneas marcados
;  con [TAREAS]. ventanas.asm lo incluye tres veces, y cada vez entra una
;  parte: con TAREAS_RM, la de modo real (16 bits, en los primeros 64 KB);
;  con TAREAS_BSS, las variables sin valor inicial; sin nada, el código de 32
;  bits (en la parte alta).
;
;  Para los módulos (interfaz, entradas 91 a 95):
;    tar_lista    EDI=búfer ECX=cuántas fichas caben -> EAX=cuántas tareas hay
;                 (se escriben las que quepan). La ficha, de TF_TAM (64) octetos:
;                   TF_ID      dd  identificador de la tarea
;                   TF_VEN     dd  su ventana principal (0: no tiene)
;                   TF_MEM     dd  memoria del módulo: código, datos y bss (0 en
;                                  los programas que van dentro de VENTANAS.COM;
;                                  en el sistema: VENTANAS.COM, su montón y la
;                                  superficie de dibujo)
;                   TF_PEDIDA  dd  memoria que ha pedido: mem_pedir y
;                                  mem_bloque, más su pila
;                   TF_ESTADO  dd  bit 0 (TE_NORESP) no responde; bit 1
;                                  (TE_SISTEMA) es del sistema y no se cierra;
;                                  bit 2 (TE_MINIM) minimizada; bit 3 (TE_PARADA)
;                                  está parada tras un Ctrl+Alt+Supr
;                   TF_NOMBRE  40 octetos: el título de su ventana principal o
;                                  el nombre del .HVP, acabado en 0
;                   TF_NVEN    dd  cuántas ventanas de primer nivel con título
;                                  tiene (si sube al pedirle que cierre, es que
;                                  ha abierto un cuadro para preguntar)
;    tar_activar  EAX=identificador: la trae delante (la restaura si está
;                 minimizada y la pone otra vez en marcha si estaba parada).
;                 CF=1 si no existe o no tiene ventana
;    tar_cerrar   EAX=identificador EDX=0 pedírselo (WM_CLOSE a su ventana
;                 principal: el programa puede preguntar si guarda) / 1 a la
;                 fuerza. CF=1 si no se puede (no existe, es del sistema, es la
;                 que llama o, al pedirlo, no tiene ventana o está parada)
;    mem_estado   -> EAX=memoria total que maneja Hispa Ventanas (la del
;                 servidor DPMI) EBX=libre ECX=el mayor bloque libre EDX=la que
;                 usan los módulos ESI=memoria convencional libre de DOS (el
;                 mayor bloque) EDI=lo libre en el montón del sistema
;    tar_reiniciar EBP=ventana dueña: reinicia el equipo como el cuadro «Salir»
;                 (guarda CENTRO.INI y deja el encargo al lanzador); si no se
;                 puede, lo dice en un aviso y vuelve
;  Al Administrador le llega WM_HV_REINICIAR cuando se pulsa Ctrl+Alt+Supr con
;  él abierto y delante: entonces pregunta si se reinicia el equipo.
;  (Las constantes TF_*, TE_* y WM_HV_REINICIAR están repetidas en
;  programas/tareas/fichas.inc: tools/mkhvp.py no lleva este archivo a los
;  módulos. Tienen que coincidir.)
;
;  Del sistema (los ganchos):
;    tar_rm_tecla   (modo real, INT 15h/4Fh de ventanas.asm) AL=código de tecla
;    tar_rm_poner, tar_rm_quitar   (tec_instalar, tec_quitar)
;    tar_en_rm      (rm_ir) distinto de 0 dentro de una llamada a modo real
;    tar_sistema    (eventos) una vez por vuelta del bucle del sistema
;    tar_latido     (msg_mirar) la tarea que corre mira su cola
;    tar_abrir      (el menú del escritorio, contexto.inc) abre el Administrador
;    msg_enviar.tar_vuelta      adonde vuelve un procedimiento de ventana
;
;  QUÉ SE LLEVA DE CADA TAREA. La tabla de la interfaz se retoca al arrancar
;  (tar_rm_poner): los módulos reciben dos_abrir, dos_crear, dos_cerrar,
;  mem_pedir, mem_bloque, mem_bloque_soltar, snd_pcm y snd_pcm2 a través de
;  unas envolturas que hacen lo mismo y además apuntan de quién es cada
;  archivo abierto (tar_arch), cada bloque de mem_bloque (tar_blq) y cada
;  trozo del montón (en su cabecera, donde mem_pedir pone un 1 de «en uso»,
;  queda la tarea: para el montón cualquier valor distinto de 0 es «en uso»).
;  Con eso, al acabar una tarea (a la fuerza, tar_matar, o por las buenas,
;  tar_sistema) se cierra y se suelta lo que haya dejado.
;
;  «NO RESPONDE». tar_latido apunta el tic en que cada tarea miró su cola por
;  última vez. Una tarea no responde si tiene mensajes esperando y lleva más
;  de TAR_SIN_RESP tics sin mirarla, o si está parada (T_USO = 3).
;
;  CTRL+ALT+SUPR Y EL PROGRAMA COLGADO. La multitarea es cooperativa: si un
;  módulo se mete en un bucle sin fin, el bucle del sistema no vuelve a
;  correr, nadie lee el teclado con INT 16h y ninguna tarea recibe nada. Lo
;  único que sigue vivo son las interrupciones, que HSPDPMI pasa a modo real:
;   1. Cada código del teclado llega a rm_int15 (INT 15h, AH=4Fh), y de ahí a
;      tar_rm_tecla: si es Supr con Ctrl y Alt, pone tar_cas = 1 y le cambia
;      el código a la BIOS por el de «soltar Supr», para que no reinicie.
;   2. Si el sistema está vivo, tar_sistema lo ve en la siguiente vuelta y
;      abre el Administrador (en una tarea suya, tar_adm_tarea: el bucle del
;      sistema no puede esperar a un disco ni a un aviso).
;   3. Si no, tar_rm_1c (INT 1Ch, el tic del reloj) cuenta los tics que pasan
;      con tar_cas puesto y Hispa Ventanas en modo protegido (fuera de una
;      llamada a DOS o a la BIOS: tar_en_rm). A los TAR_ESPERA tics (unos
;      tres cuartos de segundo) llama, por un «callback» de modo real del
;      DPMI (función 0303h), a tar_arrebatar, que corre en modo protegido
;      dentro de la interrupción.
;   4. Desde ahí no se puede saber dónde estaba el programa (su contexto lo
;      guarda el servidor en su pila de anillo 0) ni cambiarle la dirección de
;      vuelta. Lo que sí se puede es tocar su memoria: tar_arrebatar guarda
;      una copia del código y los datos de cada módulo en marcha y los llena
;      de INT 3 (CCh). Al volver de la interrupción, la siguiente instrucción
;      que ejecute un módulo es un INT 3, esté donde esté.
;   5. INT 3 es una interrupción de programa: el servidor DPMI la entrega en
;      la pila de la tarea que corría, con su dirección encima, a tar_int3
;      (vector puesto con la función 0205h). Ahí se devuelve a los módulos lo
;      suyo (están otra vez intactos), se mira en qué módulo ha caído y:
;      - si corría en su propia tarea, se le guarda el contexto tal cual
;        (como hace tar_ceder, con la vuelta a un IRETD) y la tarea queda
;        PARADA (T_USO = 3): tar_otra ya no le da el turno;
;      - si era un procedimiento de ventana suyo llamado desde otra tarea
;        (msg_enviar desde el sistema, por ejemplo), se busca en la pila la
;        vuelta a msg_enviar (msg_enviar.tar_vuelta) y se sale por ahí con EAX = 0,
;        como si el procedimiento hubiera vuelto; su tarea queda parada.
;      El sistema sigue: como tar_cas no se ha atendido, abre el Administrador.
;   6. Las ventanas de una tarea parada pasan a un procedimiento del sistema
;      (tar_proc_fantasma, «no responde») y las pinta tar_fantasmas; ya no se
;      entra en el código del módulo. «Pasar a» la pone otra vez en marcha
;      donde estaba (tar_descongelar); «Terminar», tar_matar.
;  tar_matar NO vuelve nunca a la tarea: deja sus ventanas sin procedimiento
;  propio, las destruye, cierra sus archivos, suelta sus bloques y su montón,
;  calla su sonido y marca la tarea y el módulo como terminados: la pila y la
;  memoria del módulo las sueltan tar_limpiar y hvp_limpiar, como siempre.
;
;  Límites: no se puede arrebatar el control a un programa de dentro de
;  VENTANAS.COM (Bloc de notas, Calculadora, Reloj, Centro) ni al propio
;  núcleo, ni a un módulo que cierre las interrupciones; para eso quedan
;  Ctrl+Alt+Fin y Ctrl+Alt+Supr tres veces, de HSPDPMI. Con teclado USB
;  (HSPUSB) Ctrl+Alt+Supr no pasa por INT 15h y reinicia como antes.
; =============================================================================

%ifdef TAREAS_RM
%undef TAREAS_RM
; -----------------------------------------------------------------------------
;  La parte de modo real (16 bits; la ve también la de 32)
; -----------------------------------------------------------------------------
TAR_ESPERA  equ 14                  ; tics sin atender Ctrl+Alt+Supr: arrebatar

tar_cas     db 0                    ; 1: se ha pulsado Ctrl+Alt+Supr y nadie lo ha atendido
tar_cas_n   db 0                    ; tics que lleva así con el programa en modo protegido
tar_en_rm   db 0                    ; distinto de 0: dentro de una llamada a modo real (rm_ir)
tar_rm_ok   db 0                    ; el vector de INT 1Ch está puesto
tar_cb      dd 0                    ; el «callback» de modo real que lleva a tar_arrebatar
tar_ant1c   dd 0                    ; el vector de INT 1Ch de antes

; (rm_int15) AL=el código que llega del teclado. Si es Supr con Ctrl y Alt
; pulsadas, se apunta y a la BIOS le llega «soltar Supr» (D3h) en su lugar:
; no reinicia, y no se queda esperando el final de una tecla extendida (E0h),
; que es lo que pasaría si el código se le quitara sin más
tar_rm_tecla:
        cmp al, 0x53
        jne .x
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov al, [0x417]             ; mayúsculas, Ctrl y Alt, según la BIOS
        and al, 0x0C
        cmp al, 0x0C
        pop ax
        pop ds
        jne .x
        mov byte [cs:tar_cas], 1
        mov al, 0xD3
.x:
        ret

; INT 1Ch: el tic del reloj (18,2 por segundo), venga de donde venga
tar_rm_1c:
        cmp byte [cs:tar_cas], 0
        je .c
        cmp byte [cs:tar_en_rm], 0  ; en DOS o en la BIOS (o durmiendo en el
        jne .c                      ; sondeo): no es un programa colgado
        inc byte [cs:tar_cas_n]
        cmp byte [cs:tar_cas_n], TAR_ESPERA
        jb .c
        mov byte [cs:tar_cas_n], 0
        cmp dword [cs:tar_cb], 0
        je .c
        call far [cs:tar_cb]        ; a modo protegido: tar_cb_pm
.c:
        jmp far [cs:tar_ant1c]

%elifdef TAREAS_BSS
%undef TAREAS_BSS
; -----------------------------------------------------------------------------
;  Variables sin valor inicial (se ponen a cero al entrar en modo protegido)
; -----------------------------------------------------------------------------
            alignb 4
tar_ult     resd TAR_MAX            ; el tic en que cada tarea miró su cola por última vez
tar_arch    resd TAR_ARCH           ; de qué tarea es cada archivo abierto por un módulo
tar_blq     resb TAR_BLQ * TB_TAM   ; los bloques de mem_bloque de los módulos
tar_copia   resd HVP_MAX            ; la copia de cada módulo mientras está lleno de INT 3
tar_proc_orig resd MAX_VEN          ; el procedimiento de cada ventana de una tarea parada
tar_d1_orig resd MAX_VEN            ;   y el gancho de su diálogo
tar_snd     resd 1                  ; la última tarea que ha hecho sonar algo
tar_caido   resd 1                  ; (tar_int3) el módulo que ha caído en el INT 3
tar_mem_total resd 1                ; la memoria que había libre al arrancar: el total
tar_n_par   resd 1                  ; cuántas tareas hay paradas
tar_meminfo resb 0x30               ; lo que dice el DPMI de la memoria (0500h)
tar_cb_reg  resb 0x34               ; los registros de modo real del «callback»
tar_acc     resb AC_TAM             ; el acceso con que se abre TAREAS.HVP
tar_msg     resb M_TAM              ; (tar_fantasmas) el WM_PAINT que se reparte
tar_viva    resb TAR_MAX            ; 1: esa tarea ha mirado su cola alguna vez
tar_fase    resb 1                  ; 1: los módulos están llenos de INT 3; 2: se están reponiendo
tar_adm_curso resb 1                ; 1: hay una tarea abriendo el Administrador
            alignb 4

%else
; -----------------------------------------------------------------------------
;  El código de 32 bits
; -----------------------------------------------------------------------------
TAR_SIN_RESP equ 82                 ; tics sin mirar la cola con mensajes en ella: no responde (4,5 s)
TAR_ARCH    equ 32                  ; manejadores de archivo que se siguen (0 a 31)
TAR_BLQ     equ 48                  ; bloques de mem_bloque que se siguen
TB_MANGO    equ 0                   ; un bloque: su mango (0: libre),
TB_TAREA    equ 4                   ;   de quién es
TB_OCT      equ 8                   ;   y cuánto mide
TB_TAM      equ 12
T_PARADA    equ 3                   ; T_USO de una tarea parada (ni corre ni está libre)

; la ficha de tar_lista y su estado (repetido en programas/tareas/fichas.inc)
TF_ID       equ 0
TF_VEN      equ 4
TF_MEM      equ 8
TF_PEDIDA   equ 12
TF_ESTADO   equ 16
TF_NOMBRE   equ 20
TF_NVEN     equ 60
TF_TAM      equ 64
TE_NORESP   equ 1
TE_SISTEMA  equ 2
TE_MINIM    equ 4
TE_PARADA   equ 8
WM_HV_REINICIAR equ WM_USER + 220   ; al Administrador: Ctrl+Alt+Supr con él delante

tar_t_admin db "PROGS\TAREAS.HVP", 0
tar_t_sis   db "Hispa Ventanas (el sistema)", 0
tar_t_sinven db "(sin ventana)", 0
tar_t_noresp db "(no responde)", 0
tar_t_falta db "No se encuentra el Administrador de tareas", 10
            db "(TAREAS.HVP) en la carpeta PROGS de Hispa Ventanas.", 0
tar_t_falta2 db "No se encuentra el Administrador de tareas", 10
            db "(TAREAS.HVP) en la carpeta PROGS de Hispa Ventanas.", 10
            db "El programa que no respondía se ha cerrado.", 0

; lo que la interfaz da a los módulos a través de una envoltura: dd la
; función, dd la envoltura
tar_envolturas:
        dd dos_abrir, tar_w_abrir
        dd dos_crear, tar_w_crear
        dd dos_cerrar, tar_w_cerrar
        dd mem_pedir, tar_w_pedir
        dd mem_bloque, tar_w_bloque
        dd mem_bloque_soltar, tar_w_bloque_soltar
        dd snd_pcm, tar_w_pcm
        dd snd_pcm2, tar_w_pcm2
        dd 0

; ---- poner y quitar (tec_instalar y tec_quitar) ----
; El «callback» de modo real, el vector de INT 3 de modo protegido, el de
; INT 1Ch de modo real y las envolturas de la tabla de la interfaz
tar_rm_poner:
        pushad
        cmp byte [tar_rm_ok], 0
        jne .x
        mov esi, tar_envolturas     ; las envolturas, a la tabla
.e:
        mov eax, [esi]
        test eax, eax
        jz .ef
        mov edi, api_tabla
        mov ecx, API_N
.e1:
        cmp [edi], eax
        jne .e2
        mov edx, [esi+4]
        mov [edi], edx
.e2:
        add edi, 4
        dec ecx
        jnz .e1
        add esi, 8
        jmp .e
.ef:
        mov ax, 0x0500              ; la memoria libre ahora, antes de pedir nada,
        mov edi, tar_meminfo        ; es toda la que va a manejar Hispa Ventanas
        int 0x31
        mov eax, [tar_meminfo+0x14]
        shl eax, 12
        mov [tar_mem_total], eax
        mov ax, 0x0205              ; INT 3 de modo protegido: tar_int3
        mov bl, 3
        mov cx, cs
        mov edx, tar_int3
        int 0x31
        jc .x                       ; (sin eso no hay nada que arrebatar)
        push ds                     ; el «callback»: DS:ESI=la rutina, ES:EDI=sus registros
        push cs
        pop ds
        mov esi, tar_cb_pm
        mov edi, tar_cb_reg
        mov ax, 0x0303
        int 0x31
        pop ds
        jc .x
        mov [tar_cb], dx
        mov [tar_cb+2], cx
        mov eax, [cero]             ; el vector de INT 1Ch de modo real
        mov edx, [eax+0x1C*4]
        mov [tar_ant1c], edx
        movzx edx, word [rmseg]
        shl edx, 16
        mov dx, tar_rm_1c
        mov [eax+0x1C*4], edx
        mov byte [tar_rm_ok], 1
.x:
        popad
        ret

; (también desde una excepción, con otra pila: solo DS es de fiar)
tar_rm_quitar:
        cmp byte [tar_rm_ok], 0
        je .x
        push eax
        push edx
        mov eax, [cero]
        mov edx, [tar_ant1c]
        mov [eax+0x1C*4], edx
        mov byte [tar_rm_ok], 0
        mov byte [tar_cas], 0
        pop edx
        pop eax
.x:
        ret

; ---- las envolturas de la interfaz ----
tar_w_abrir:
        call dos_abrir
        jmp tar_w_arch
tar_w_crear:
        call dos_crear
tar_w_arch:
        jc .x
        cmp eax, TAR_ARCH
        jae .ok
        push ebx
        mov ebx, [tar_act]
        mov [tar_arch+eax*4], ebx
        pop ebx
.ok:
        clc
.x:
        ret
tar_w_cerrar:
        cmp ebx, TAR_ARCH
        jae .c
        mov dword [tar_arch+ebx*4], 0
.c:
        jmp dos_cerrar
tar_w_pedir:
        call mem_pedir
        jc .x
        push ebx
        mov ebx, [tar_act]          ; (en la cabecera del trozo: distinto de 0, «en uso»)
        mov [eax-4], ebx
        pop ebx
.x:
        ret
tar_w_bloque:
        call mem_bloque
        jc .x
        push esi
        push edi
        mov esi, tar_blq
        mov edi, TAR_BLQ
.b:
        cmp dword [esi+TB_MANGO], 0
        je .hay
        add esi, TB_TAM
        dec edi
        jnz .b
        jmp .f                      ; (no caben más: ese no se sigue)
.hay:
        mov [esi+TB_MANGO], edx
        mov [esi+TB_OCT], ecx
        mov edi, [tar_act]
        mov [esi+TB_TAREA], edi
.f:
        pop edi
        pop esi
        clc
.x:
        ret
tar_w_bloque_soltar:
        push esi
        push edi
        mov esi, tar_blq
        mov edi, TAR_BLQ
.b:
        cmp [esi+TB_MANGO], edx
        jne .s
        mov dword [esi+TB_MANGO], 0
.s:
        add esi, TB_TAM
        dec edi
        jnz .b
        pop edi
        pop esi
        jmp mem_bloque_soltar
tar_w_pcm:
        push dword [tar_act]
        pop dword [tar_snd]
        jmp snd_pcm
tar_w_pcm2:
        push dword [tar_act]
        pop dword [tar_snd]
        jmp snd_pcm2

; ---- ayudas ----
; EAX=tarea -> EAX=su número (0 a TAR_MAX-1)
tar_num:
        push ecx
        push edx
        sub eax, [tar_tabla]
        xor edx, edx
        mov ecx, T_TAM
        div ecx
        pop edx
        pop ecx
        ret

; EAX=identificador -> CF=1 si no es una tarea en marcha o parada
tar_valida:
        push eax
        push ecx
        push edx
        mov ecx, eax
        sub eax, [tar_tabla]
        jb .no
        cmp eax, TAR_MAX * T_TAM
        jae .no
        xor edx, edx
        push ecx
        mov ecx, T_TAM
        div ecx
        pop ecx
        test edx, edx
        jnz .no
        cmp dword [ecx+T_USO], 1
        je .si
        cmp dword [ecx+T_USO], T_PARADA
        jne .no
.si:
        clc
        jmp .x
.no:
        stc
.x:
        pop edx
        pop ecx
        pop eax
        ret

; EBX=tarea -> EBP=su ventana principal (0: no tiene) ECX=cuántas ventanas de
; primer nivel con título tiene. La principal es la primera que no es un
; cuadro de diálogo; si todas lo son, la primera
tar_ven_principal:
        push eax
        push edx
        push esi
        xor ebp, ebp
        xor ecx, ecx
        xor edx, edx                ; (el primer cuadro de diálogo)
        mov esi, [ven_tabla]
        mov eax, MAX_VEN
.b:
        cmp dword [esi+V_USO], 1
        jne .s
        cmp [esi+V_TAREA], ebx
        jne .s
        cmp dword [esi+V_PADRE], 0
        jne .s
        test dword [esi+V_ESTILO], E_TITULO
        jz .s
        inc ecx
        cmp dword [esi+V_CLASE], c_dialogo
        jne .normal
        test edx, edx
        jnz .s
        mov edx, esi
        jmp .s
.normal:
        test ebp, ebp
        jnz .s
        mov ebp, esi
.s:
        add esi, V_TAM
        dec eax
        jnz .b
        test ebp, ebp
        jnz .x
        mov ebp, edx
.x:
        pop esi
        pop edx
        pop eax
        ret

; EBX=tarea -> EDX=su módulo (la ficha de hvp_tabla); 0 si no es un módulo
tar_modulo:
        push ecx
        mov edx, hvp_tabla
        mov ecx, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 1
        jne .s
        cmp [edx+HS_TAREA], ebx
        je .x
.s:
        add edx, HS_TAM
        dec ecx
        jnz .b
        xor edx, edx
.x:
        pop ecx
        ret

; EAX=dirección -> EDX=el módulo en marcha que la tiene dentro; 0 si ninguno
tar_modulo_de:
        push ebx
        push ecx
        mov edx, hvp_tabla
        mov ecx, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 1
        jne .s
        mov ebx, [edx+HS_BASE]
        cmp eax, ebx
        jb .s
        add ebx, [ebx+H_MEM]
        cmp eax, ebx
        jb .x
.s:
        add edx, HS_TAM
        dec ecx
        jnz .b
        xor edx, edx
.x:
        pop ecx
        pop ebx
        ret

; EBX=tarea -> EAX=octetos del montón que ha pedido (mem_pedir de la interfaz)
tar_monton_de:
        push ecx
        push edx
        xor eax, eax
        mov ecx, [mon_ini]
        lea edx, [ecx+MONTON_TAM]
.b:
        cmp ecx, edx
        jae .x
        cmp [ecx+4], ebx
        jne .s
        add eax, [ecx]
.s:
        cmp dword [ecx], 8          ; (un montón estropeado: no se da vueltas)
        jb .x
        add ecx, [ecx]
        jmp .b
.x:
        pop edx
        pop ecx
        ret

; EBX=tarea: suelta lo que quede suyo: archivos abiertos, bloques y montón
tar_soltar_de:
        pushad
        xor esi, esi                ; los archivos
.a:
        cmp [tar_arch+esi*4], ebx
        jne .as
        mov dword [tar_arch+esi*4], 0
        push ebx
        mov ebx, esi
        call dos_cerrar
        pop ebx
.as:
        inc esi
        cmp esi, TAR_ARCH
        jb .a
        mov esi, tar_blq            ; los bloques de mem_bloque
        mov ecx, TAR_BLQ
.b:
        cmp dword [esi+TB_MANGO], 0
        je .bs
        cmp [esi+TB_TAREA], ebx
        jne .bs
        mov edx, [esi+TB_MANGO]
        mov dword [esi+TB_MANGO], 0
        call mem_bloque_soltar
.bs:
        add esi, TB_TAM
        dec ecx
        jnz .b
        mov ecx, [mon_ini]          ; el montón
        lea edx, [ecx+MONTON_TAM]
.m:
        cmp ecx, edx
        jae .mf
        cmp [ecx+4], ebx
        jne .ms
        mov dword [ecx+4], 0
.ms:
        cmp dword [ecx], 8
        jb .mf
        add ecx, [ecx]
        jmp .m
.mf:
        cmp [tar_snd], ebx          ; y si sonaba, se calla
        jne .x
        mov dword [tar_snd], 0
        mov eax, 0x0300
        call snd_orden
.x:
        popad
        ret

; ---- el latido (msg_mirar) ----
; La tarea que corre mira su cola: se apunta cuándo. Si es el sistema y los
; módulos estaban llenos de INT 3, es que no había nadie colgado: se reponen
tar_latido:
        push eax
        push ebx
        mov eax, [tar_act]
        cmp byte [tar_fase], 0
        je .n
        cmp eax, [tar_tabla]
        jne .n
        call tar_reponer
.n:
        call tar_num
        mov ebx, eax
        mov byte [tar_viva+ebx], 1
        call tics
        mov [tar_ult+ebx*4], eax
        pop ebx
        pop eax
        ret

; ---- una vez por vuelta del bucle del sistema (eventos) ----
tar_sistema:
        pushad
        ; lo que hayan dejado las tareas que han acabado por las buenas
        mov ebx, [tar_tabla]
        xor esi, esi
.t:
        cmp byte [tar_viva+esi], 0
        je .ts
        cmp dword [ebx+T_USO], 0
        jne .ts
        mov byte [tar_viva+esi], 0
        call tar_soltar_de
.ts:
        add ebx, T_TAM
        inc esi
        cmp esi, TAR_MAX
        jb .t
        cmp dword [tar_n_par], 0
        je .cas
        call tar_fantasmas
.cas:
        cmp byte [tar_cas], 0
        je .x
        mov byte [tar_cas], 0
        mov byte [tar_cas_n], 0
        cmp byte [tar_fase], 0
        je .c1
        call tar_reponer
.c1:
        cmp byte [tar_adm_curso], 0
        jne .x
        mov byte [tar_adm_curso], 1
        mov eax, tar_adm_tarea      ; (abrir un archivo o dar un aviso es cosa
        call tar_crear              ; de una tarea, no del bucle del sistema)
        test eax, eax
        jnz .x
        mov byte [tar_adm_curso], 0
        call tar_matar_paradas      ; ni una tarea más: al menos, fuera lo colgado
.x:
        popad
        ret

; la tarea que abre el Administrador (o le avisa de que se ha vuelto a pulsar)
tar_adm_tarea:
        call dir_propia_poner
        mov edi, tar_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tar_t_admin
        call pon_cad
        mov byte [edi], 0
        mov dword [tar_acc+AC_PARAM], 0
        lea esi, [tar_acc+AC_PROG]  ; ¿está abierto?
        mov edx, hvp_tabla
        mov ecx, HVP_MAX
.b:
        cmp dword [edx+HS_USO], 1
        jne .s
        lea edi, [edx+HS_RUTA]
        call cad_igual_i
        je .abierto
.s:
        add edx, HS_TAM
        dec ecx
        jnz .b
        xor al, al                  ; no: ¿está en el disco?
        call dos_abrir
        jc .falta
        mov ebx, eax
        call dos_cerrar
        jmp .abre
.abierto:
        mov ebx, [edx+HS_TAREA]
        test ebx, ebx
        jz .abre
        cmp dword [ebx+T_USO], T_PARADA
        jne .viva
        mov eax, ebx                ; es él el que estaba parado: que siga
        call tar_descongelar
        jmp .abre
.viva:
        mov eax, [activa]           ; ¿y delante? Entonces se ofrece reiniciar
        test eax, eax
        jz .abre
        cmp [eax+V_TAREA], ebx
        jne .abre
        call tar_ven_principal
        mov eax, WM_HV_REINICIAR
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        jmp .x
.abre:
        mov ebx, tar_acc
        mov ebp, [cen_ven]          ; (si no se puede cargar, el aviso sale aquí)
        call hvp_abrir
        jmp .x
.falta:
        mov esi, tar_t_falta
        cmp dword [tar_n_par], 0
        je .av
        call tar_matar_paradas      ; sin Administrador no hay quien lo cierre
        mov esi, tar_t_falta2
.av:
        mov ebp, [cen_ven]
        call cen_aviso
.x:
        mov byte [tar_adm_curso], 0
        ret

; (el menú del escritorio) abre el Administrador de tareas o, si ya está
; abierto, lo trae delante. No es Ctrl+Alt+Supr: aquí no se para a nadie ni
; se ofrece reiniciar
tar_abrir:
        pushad
        mov edi, tar_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tar_t_admin
        call pon_cad
        mov byte [edi], 0
        mov dword [tar_acc+AC_PARAM], 0
        mov ebx, tar_acc
        mov ebp, [cen_ven]          ; (si no se puede cargar, el aviso sale aquí)
        call hvp_abrir
        popad
        ret

; (interna) termina a la fuerza todas las tareas paradas
tar_matar_paradas:
        pushad
        mov eax, [tar_tabla]
        mov ecx, TAR_MAX
.b:
        cmp dword [eax+T_USO], T_PARADA
        jne .s
        call tar_matar
.s:
        add eax, T_TAM
        dec ecx
        jnz .b
        popad
        ret

; ---- arrebatar el control (dentro de la interrupción del reloj) ----
; El «callback» de modo real: DS:ESI=la pila de modo real, ES:EDI=los
; registros de modo real (tar_cb_reg, con el selector de datos), SS:ESP=la
; pila bloqueada del servidor, interrupciones cerradas. Se vuelve con IRETD y
; en los registros, la dirección de modo real donde se sigue: la de vuelta de
; la llamada lejana, que está en la pila de modo real
tar_cb_pm:
        cld
        mov ax, [esi]
        mov [es:edi+0x2A], ax       ; IP
        mov ax, [esi+2]
        mov [es:edi+0x2C], ax       ; CS
        add word [es:edi+0x2E], 4   ; SP
        push ds
        push es
        pop ds
        pushad
        call tar_arrebatar
        popad
        pop ds
        iretd

; Llena de INT 3 todos los módulos en marcha (guardando una copia de cada
; uno en el montón). Si ya lo estaban y nadie ha caído, es que lo colgado no
; es un módulo: se reponen y no se insiste. (Aquí SS no es el segmento de
; datos: nada de direcciones con EBP o ESP)
tar_arrebatar:
        cmp byte [tar_fase], 2
        je .x
        cmp byte [tar_fase], 1
        je .rinde
        mov edx, hvp_tabla
        mov ebx, tar_copia
.b:
        cmp dword [edx+HS_USO], 1
        jne .s
        mov eax, [edx+HS_TAREA]
        test eax, eax
        jz .s
        cmp dword [eax+T_USO], 1
        jne .s
        mov esi, [edx+HS_BASE]
        call tar_zona               ; ESI=dónde empieza ECX=octetos
        jc .s
        call mem_pedir
        jc .s                       ; (no cabe la copia: ese módulo no se toca)
        mov [ebx], eax
        mov edi, eax
        push esi
        push ecx
        rep movsb
        pop ecx
        pop edi
        mov al, 0xCC
        rep stosb
        mov byte [tar_fase], 1
.s:
        add edx, HS_TAM
        add ebx, 4
        cmp ebx, tar_copia + HVP_MAX * 4
        jb .b
        cmp byte [tar_fase], 1
        je .x
        mov byte [tar_cas], 0       ; no hay ningún módulo: nada que hacer desde aquí
.x:
        ret
.rinde:
        call tar_reponer
        mov byte [tar_cas], 0
        ret

; (interna) ESI=base de un módulo -> ESI=dónde empieza lo que se llena de
; INT 3 (detrás de la cabecera, el icono y la tabla de la interfaz) ECX=cuánto
; (hasta el final de la imagen: código y datos); CF=1 si no hay nada
tar_zona:
        push eax
        movzx eax, word [esi+H_API]
        lea eax, [eax*4+H_TABLA]
        mov ecx, [esi+H_IMG]
        add esi, eax
        sub ecx, eax
        pop eax
        jbe .no
        clc
        ret
.no:
        stc
        ret

; Devuelve a cada módulo su código y sus datos y suelta las copias
tar_reponer:
        pushad
        mov byte [tar_fase], 2
        mov edx, hvp_tabla
        mov ebx, tar_copia
.b:
        mov eax, [ebx]
        test eax, eax
        jz .s
        mov dword [ebx], 0
        cmp dword [edx+HS_USO], 1
        jne .suelta
        mov esi, [edx+HS_BASE]
        call tar_zona
        jc .suelta
        mov edi, esi
        mov esi, eax
        rep movsb
.suelta:
        call mem_soltar
.s:
        add edx, HS_TAM
        add ebx, 4
        cmp ebx, tar_copia + HVP_MAX * 4
        jb .b
        mov byte [tar_fase], 0
        popad
        ret

; INT 3 en modo protegido (el servidor DPMI la entrega como una interrupción
; de programa: en la pila de quien corría, con EIP, CS y las banderas)
tar_int3:
        push eax
        mov ax, [cs:sel_dat]
        mov ds, ax
        mov es, ax
        pop eax
        cld
        cmp byte [tar_fase], 1
        jne .ajena                  ; un INT 3 que no es nuestro: como si nada
        call tar_reponer            ; los módulos, otra vez enteros
        push eax
        push edx
        mov eax, [esp+8]            ; la dirección siguiente al INT 3
        dec eax
        call tar_modulo_de
        mov [tar_caido], edx
        test edx, edx
        pop edx
        pop eax
        jnz .mio
.ajena:
        iretd
.mio:
        dec dword [esp]             ; otra vez a la instrucción que no llegó a correr
        push dword tar_reanuda      ; el contexto, como lo deja tar_ceder
        pushad
        mov edx, [tar_caido]
        mov ebx, [tar_act]
        mov eax, [edx+HS_TAREA]
        cmp eax, ebx
        jne .de_otra
        mov [ebx+T_ESP], esp        ; corría en su tarea: se queda parada ahí
        call tar_congelar
        jmp tar_otra                ; (EBX=la tarea que deja el turno)
.de_otra:
        call tar_congelar           ; su tarea, parada donde estuviera
        mov esi, esp                ; y de su procedimiento de ventana se sale:
        mov edi, pila_fin           ; la vuelta a msg_enviar está en la pila
        cmp ebx, [tar_tabla]
        je .busca
        mov edi, [ebx+T_PILA]
        add edi, PILA_TAM
.busca:
        sub edi, 4
.b1:
        cmp esi, edi
        ja .no_esta
        cmp dword [esi], msg_enviar.tar_vuelta
        je .sale
        inc esi
        jmp .b1
.sale:
        mov esp, esi
        xor eax, eax                ; «el procedimiento ha devuelto 0»
        ret
.no_esta:
        cmp ebx, [tar_tabla]
        je .sistema
        mov [ebx+T_ESP], esp        ; no se sabe salir: esa tarea, parada también
        mov eax, ebx
        call tar_congelar
        jmp tar_otra
.sistema:
        mov esp, pila_fin           ; el bucle del sistema, desde el principio
        mov byte [arrastre], 0
        mov dword [arr_ven], 0
        mov byte [nc_apretado], 0
        mov dword [captura], 0
        call gdi_recorte_total
        jmp principal.bucle

; aquí «vuelve» una tarea parada cuando se la pone otra vez en marcha
tar_reanuda:
        iretd

; EAX=tarea: la deja parada. Su cola se vacía, sus ventanas pasan al
; procedimiento del sistema (se guarda el suyo para tar_descongelar) y se
; quita de en medio lo que tuviera cogido: la captura del ratón, el menú
; abierto, la forma del cursor y el contexto de dibujo
tar_congelar:
        pushad
        mov ebx, eax
        cmp dword [ebx+T_USO], T_PARADA
        je .x
        mov dword [ebx+T_USO], T_PARADA
        inc dword [tar_n_par]
        mov dword [ebx+T_N], 0
        mov dword [ebx+T_CAB], 0
        call gdi_recorte_total      ; (si estaba pintando)
        mov byte [gdi_negrita], 0
        mov eax, [menu_duena]
        test eax, eax
        jz .m
        cmp [eax+V_TAREA], ebx
        jne .m
        call menu_cerrar
.m:
        mov eax, [captura]
        test eax, eax
        jz .c
        cmp [eax+V_TAREA], ebx
        jne .c
        mov dword [captura], 0
        mov byte [cap_der], 0
.c:
        mov eax, [cur_duena]
        test eax, eax
        jz .f
        cmp [eax+V_TAREA], ebx
        jne .f
        mov dword [cur_duena], 0
        xor eax, eax
        call cur_poner
.f:
        mov ebp, [ven_tabla]
        xor esi, esi
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], ebx
        jne .vs
        xor eax, eax
        mov [tar_proc_orig+esi*4], eax
        mov [tar_d1_orig+esi*4], eax
        mov eax, [ebp+V_PROC]
        cmp eax, bss_fin            ; ¿su procedimiento es del módulo?
        jb .v1
        mov [tar_proc_orig+esi*4], eax
        mov dword [ebp+V_PROC], tar_proc_fantasma
.v1:
        cmp dword [ebp+V_CLASE], c_dialogo
        jne .v2
        mov eax, [ebp+V_D1]         ; ¿y el gancho de su diálogo?
        cmp eax, bss_fin
        jb .v2
        mov [tar_d1_orig+esi*4], eax
        mov dword [ebp+V_D1], 0
.v2:
        cmp dword [ebp+V_PADRE], 0
        jne .v3
        call ven_inv_toda
        jmp .vs
.v3:
        call ven_invalidar
.vs:
        add ebp, V_TAM
        inc esi
        cmp esi, MAX_VEN
        jb .v
        mov byte [actividad], 1
.x:
        popad
        ret

; EAX=tarea parada: otra vez en marcha, donde estaba
tar_descongelar:
        pushad
        mov ebx, eax
        cmp dword [ebx+T_USO], T_PARADA
        jne .x
        mov ebp, [ven_tabla]
        xor esi, esi
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], ebx
        jne .vs
        mov eax, [tar_proc_orig+esi*4]
        test eax, eax
        jz .v1
        cmp dword [ebp+V_PROC], tar_proc_fantasma
        jne .v1
        mov [ebp+V_PROC], eax
.v1:
        mov eax, [tar_d1_orig+esi*4]
        test eax, eax
        jz .v2
        mov [ebp+V_D1], eax
.v2:
        xor eax, eax
        mov [tar_proc_orig+esi*4], eax
        mov [tar_d1_orig+esi*4], eax
        cmp dword [ebp+V_PADRE], 0
        jne .v3
        call ven_inv_toda
        jmp .vs
.v3:
        call ven_invalidar
.vs:
        add ebp, V_TAM
        inc esi
        cmp esi, MAX_VEN
        jb .v
        mov eax, ebx                ; (que no parezca colgada nada más volver)
        call tar_num
        mov esi, eax
        call tics
        mov [tar_ult+esi*4], eax
        mov dword [ebx+T_USO], 1
        dec dword [tar_n_par]
        mov byte [actividad], 1
.x:
        popad
        ret

; el procedimiento de las ventanas de una tarea parada (o que se va a
; terminar): no entra en el módulo. Pinta la zona en crema con «(no
; responde)»; el botón de cerrar lleva al Administrador de tareas
tar_proc_fantasma:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CLOSE
        jne .nada
        mov byte [tar_cas], 1
.nada:
        xor eax, eax
        ret
.pintar:
        pushad
        call dc_pintar
        call ven_cli
        COLOR C_CREMA
        xor eax, eax
        xor ebx, ebx
        call dc_rect
        cmp dword [ebp+V_PADRE], 0
        jne .f
        mov eax, ecx
        sub eax, 13 * 8
        jl .f
        sar eax, 1
        mov ebx, edx
        sub ebx, 16
        jl .f
        sar ebx, 1
        COLOR C_ARENA_OSC
        mov esi, tar_t_noresp
        call dc_texto
.f:
        call dc_cerrar
        popad
        xor eax, eax
        ret

; (tar_sistema) pinta las ventanas de las tareas paradas: nadie más lo hace,
; porque cada tarea pinta las suyas y esas ya no corren
tar_fantasmas:
        pushad
        mov ebp, [ven_tabla]
        mov ecx, MAX_VEN
.v:
        cmp dword [ebp+V_USO], 1
        jne .s
        mov eax, [ebp+V_TAREA]
        cmp dword [eax+T_USO], T_PARADA
        jne .s
        test dword [ebp+V_ESTADO], S_NCSUCIO
        jz .v1
        call nc_pintar
.v1:
        mov eax, [ebp+V_UX0]
        cmp eax, [ebp+V_UX1]
        jge .s
        mov edi, tar_msg
        mov [edi+M_VEN], ebp
        mov dword [edi+M_MSG], WM_PAINT
        xor eax, eax
        mov [edi+M_W], eax
        mov [edi+M_L], eax
        call msg_despachar
.s:
        add ebp, V_TAM
        dec ecx
        jnz .v
        popad
        ret

; ---- terminar una tarea a la fuerza ----
; EAX=tarea (ni el sistema ni la que corre). No se vuelve a ella: su contexto
; se abandona. Conserva los registros
tar_matar:
        pushad
        mov ebx, eax
        ; 1. sus ventanas dejan de entrar en el módulo, y si un diálogo suyo
        ;    tenía fuera de juego a una ventana de otro, se le devuelve
        mov ebp, [ven_tabla]
        xor esi, esi
.v:
        cmp dword [ebp+V_USO], 1
        jne .vs
        cmp [ebp+V_TAREA], ebx
        jne .vs
        xor eax, eax
        mov [tar_proc_orig+esi*4], eax
        mov [tar_d1_orig+esi*4], eax
        cmp dword [ebp+V_PROC], bss_fin
        jb .v1
        mov dword [ebp+V_PROC], tar_proc_fantasma
.v1:
        cmp dword [ebp+V_CLASE], c_dialogo
        jne .vs
        cmp dword [ebp+V_D1], bss_fin
        jb .v2
        mov dword [ebp+V_D1], 0
.v2:
        mov eax, [ebp+V_D2]         ; la dueña del diálogo
        test eax, eax
        jz .vs
        cmp eax, [ven_tabla]        ; (por si no es un diálogo de dlg_modal)
        jb .vs
        cmp dword [eax+V_USO], 1
        jne .vs
        and dword [eax+V_ESTADO], ~S_INACTIVA
.vs:
        add ebp, V_TAM
        inc esi
        cmp esi, MAX_VEN
        jb .v
        ; 2. lo que el núcleo tuviera apuntando a sus ventanas
        mov eax, [cru_susc]         ; los códigos crudos del teclado
        test eax, eax
        jz .k1
        cmp [eax+V_TAREA], ebx
        jne .k1
        xor ebp, ebp
        call tec_suscribir
.k1:
        mov eax, [menu_duena]       ; un menú abierto
        test eax, eax
        jz .k2
        cmp [eax+V_TAREA], ebx
        jne .k2
        call menu_cerrar
.k2:
        mov eax, [captura]          ; la captura del ratón
        test eax, eax
        jz .k3
        cmp [eax+V_TAREA], ebx
        jne .k3
        mov dword [captura], 0
        mov byte [cap_der], 0
.k3:
        mov eax, [cur_duena]        ; la forma del cursor
        test eax, eax
        jz .k4
        cmp [eax+V_TAREA], ebx
        jne .k4
        mov dword [cur_duena], 0
        xor eax, eax
        call cur_poner
.k4:
        mov eax, [activa]           ; la ventana activa y el foco
        test eax, eax
        jz .k5
        cmp [eax+V_TAREA], ebx
        jne .k5
        call ges_desactivar
.k5:
        ; 3. sus ventanas, fuera (con sus hijas, sus diálogos y sus
        ;    temporizadores). Se destruyen «como si fuera ella»: los programas
        ;    de dentro de VENTANAS.COM piden WM_QUIT al destruirse su ventana,
        ;    y tiene que ir a su cola, no a la de quien la termina
        push dword [tar_act]
        mov [tar_act], ebx
        call ges_destruir_de_tarea
        mov ebp, [ven_tabla]        ; (y las que no colgaran de una suya)
        mov ecx, MAX_VEN
.d:
        cmp dword [ebp+V_USO], 1
        jne .ds
        cmp [ebp+V_TAREA], ebx
        jne .ds
        call ven_destruir
.ds:
        add ebp, V_TAM
        dec ecx
        jnz .d
        pop dword [tar_act]
        ; 4. sus archivos, sus bloques, su montón y su sonido
        call tar_soltar_de
        ; 5. el módulo y la tarea, terminados: su memoria y su pila las
        ;    sueltan hvp_limpiar y tar_limpiar en el bucle del sistema
        call tar_modulo
        test edx, edx
        jz .t
        mov dword [edx+HS_USO], 2
.t:
        cmp dword [ebx+T_USO], T_PARADA
        jne .t1
        dec dword [tar_n_par]
.t1:
        mov dword [ebx+T_USO], 2
        mov dword [ebx+T_N], 0
        mov eax, ebx
        call tar_num
        mov byte [tar_viva+eax], 0
        ; 6. y delante queda otra ventana
        cmp dword [activa], 0
        jne .f
        call ges_activar_otra
.f:
        mov byte [actividad], 1
        popad
        ret

; ---- la interfaz ----
tar_lista:
        pushad
        xor esi, esi
        mov ebx, [tar_tabla]
        mov edx, TAR_MAX
.t:
        cmp dword [ebx+T_USO], 1
        je .si
        cmp dword [ebx+T_USO], T_PARADA
        jne .s
.si:
        cmp esi, [esp+24]           ; (el ECX que se dio: cuántas caben)
        jae .cuenta
        call tar_ficha
        add edi, TF_TAM
.cuenta:
        inc esi
.s:
        add ebx, T_TAM
        dec edx
        jnz .t
        mov [esp+28], esi
        popad
        ret

; (interna) EBX=tarea EDI=dónde va su ficha
tar_ficha:
        pushad
        push edi
        mov ecx, TF_TAM / 4
        xor eax, eax
        rep stosd
        pop edi
        mov [edi+TF_ID], ebx
        call tar_ven_principal      ; EBP=ventana ECX=cuántas
        mov [edi+TF_VEN], ebp
        mov [edi+TF_NVEN], ecx
        call tar_modulo             ; EDX=módulo (0: no lo es)
        ; el estado
        xor esi, esi
        cmp dword [ebx+T_USO], T_PARADA
        jne .e1
        or esi, TE_NORESP | TE_PARADA
        jmp .e2
.e1:
        cmp dword [ebx+T_N], 0      ; ¿mensajes esperando y mucho sin mirarlos?
        je .e2
        mov eax, ebx
        call tar_num
        mov ecx, eax
        call tics
        sub eax, [tar_ult+ecx*4]
        cmp eax, TAR_SIN_RESP
        jl .e2
        or esi, TE_NORESP
.e2:
        cmp ebx, [tar_tabla]
        je .sis
        mov eax, [cen_ven]          ; el Centro de programas
        test eax, eax
        jz .e3
        cmp [eax+V_TAREA], ebx
        je .sis
.e3:
        test ebp, ebp
        jz .e5
        cmp dword [ebp+V_TEXTO], apa_tit    ; el cuadro «Salir»
        jne .e4
.sis:
        or esi, TE_SISTEMA
.e4:
        test ebp, ebp
        jz .e5
        test dword [ebp+V_ESTADO], S_MINIM
        jz .e5
        or esi, TE_MINIM
.e5:
        mov [edi+TF_ESTADO], esi
        ; la memoria
        xor eax, eax
        test edx, edx
        jz .m1
        mov eax, [edx+HS_BASE]
        mov eax, [eax+H_MEM]
.m1:
        cmp ebx, [tar_tabla]
        jne .m2
        mov eax, [vid_ancho]        ; el sistema: VENTANAS.COM, el montón y
        imul eax, [vid_alto]        ; la superficie de dibujo
        add eax, bss_fin + MONTON_TAM
.m2:
        mov [edi+TF_MEM], eax
        call tar_monton_de          ; EAX=lo suyo del montón
        mov esi, tar_blq
        mov ecx, TAR_BLQ
.b:
        cmp dword [esi+TB_MANGO], 0
        je .bs
        cmp [esi+TB_TAREA], ebx
        jne .bs
        add eax, [esi+TB_OCT]
.bs:
        add esi, TB_TAM
        dec ecx
        jnz .b
        cmp ebx, [tar_tabla]
        je .m3
        add eax, PILA_TAM
.m3:
        mov [edi+TF_PEDIDA], eax
        ; el nombre
        mov esi, tar_t_sis
        cmp ebx, [tar_tabla]
        je .n
        test ebp, ebp
        jz .n1
        mov esi, [ebp+V_TEXTO]
        test esi, esi
        jz .n1
        cmp byte [esi], 0
        jne .n
.n1:
        mov esi, tar_t_sinven
        test edx, edx
        jz .n
        mov esi, [edx+HS_BASE]
        add esi, H_NOMBRE
.n:
        add edi, TF_NOMBRE
        mov ecx, 39
        call cad_copiar
        popad
        ret

tar_activar:
        call tar_valida
        jc .r
        pushad
        mov ebx, eax
        cmp ebx, [tar_tabla]
        je .no
        cmp dword [ebx+T_USO], T_PARADA
        jne .a
        call tar_descongelar
.a:
        mov esi, [n_orden]          ; su ventana de más arriba (un diálogo
.b:                                 ; abierto está encima de su dueña)
        dec esi
        js .oculta
        mov ebp, [orden+esi*4]
        cmp [ebp+V_TAREA], ebx
        jne .b
        test dword [ebp+V_ESTILO], E_TITULO
        jz .b
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .b
        test dword [ebp+V_ESTADO], S_MINIM
        jz .act
        mov eax, 4
        call ven_mostrar
.act:
        call ven_activar
        jmp .si
.oculta:
        call tar_ven_principal      ; (la tiene oculta: se enseña)
        test ebp, ebp
        jz .no
        mov eax, 1
        call ven_mostrar
        call ven_activar
.si:
        popad
        clc
        ret
.no:
        popad
        stc
.r:
        ret

tar_cerrar:
        call tar_valida
        jc .r
        pushad
        mov ebx, eax
        cmp ebx, [tar_tabla]        ; el sistema no se cierra
        je .no
        cmp ebx, [tar_act]          ; ni la que lo pide (que acabe ella)
        je .no
        mov eax, [cen_ven]          ; ni el Centro de programas
        test eax, eax
        jz .c1
        cmp [eax+V_TAREA], ebx
        je .no
.c1:
        call tar_ven_principal
        test ebp, ebp
        jz .c2
        cmp dword [ebp+V_TEXTO], apa_tit    ; ni el cuadro «Salir»
        je .no
.c2:
        cmp dword [esp+20], 0       ; (el EDX que se dio)
        jne .fuerza
        test ebp, ebp
        jz .no
        cmp dword [ebx+T_USO], T_PARADA
        je .no
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        call msg_poner
        jmp .si
.fuerza:
        mov eax, ebx
        call tar_matar
.si:
        popad
        clc
        ret
.no:
        popad
        stc
.r:
        ret

mem_estado:
        push ebp
        push edi                    ; (el total, de la primera vez)
        mov ax, 0x0500
        mov edi, tar_meminfo
        int 0x31
        pop edi
        xor edx, edx                ; EDX=lo de los módulos
        mov esi, hvp_tabla
        mov ecx, HVP_MAX
.h:
        cmp dword [esi+HS_USO], 1
        jne .hs
        mov eax, [esi+HS_BASE]
        add edx, [eax+H_MEM]
.hs:
        add esi, HS_TAM
        dec ecx
        jnz .h
        mov esi, tar_blq
        mov ecx, TAR_BLQ
.b:
        cmp dword [esi+TB_MANGO], 0
        je .bs
        add edx, [esi+TB_OCT]
.bs:
        add esi, TB_TAM
        dec ecx
        jnz .b
        xor edi, edi                ; EDI=lo libre del montón
        mov ecx, [mon_ini]
        lea ebp, [ecx+MONTON_TAM]
.m:
        cmp ecx, ebp
        jae .mf
        mov eax, [ecx+4]
        test eax, eax
        jnz .m1
        add edi, [ecx]
        jmp .ms
.m1:
        cmp eax, 1                  ; (1: del núcleo; otra cosa: de una tarea)
        je .ms
        add edx, [ecx]
.ms:
        cmp dword [ecx], 8
        jb .mf
        add ecx, [ecx]
        jmp .m
.mf:
        push edx                    ; la convencional: pedir a DOS un bloque
        mov eax, 0x4800             ; imposible; el error dice el mayor que hay
        mov ebx, 0xFFFF
        xor ecx, ecx
        xor edx, edx
        call dos_int
        movzx esi, word [rmcs+0x10]
        shl esi, 4
        pop edx
        mov ebx, [tar_meminfo+0x14] ; lo libre, en páginas
        shl ebx, 12
        mov ecx, [tar_meminfo]      ; el mayor bloque libre
        mov eax, [tar_mem_total]
        cmp eax, ebx                ; (el total es lo que había libre al arrancar)
        jae .x
        mov eax, ebx
.x:
        pop ebp
        ret

tar_reiniciar:
        push esi
        mov esi, apa_n_reini
        call apa_encargo
        pop esi
        ret
%endif
