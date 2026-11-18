; =============================================================================
;  Hispa Ventanas - [HISPA-486] programas de Windows de 32 bits con ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Fase 3 de Hispa-486 (docs/486/HISPA-486.md): un .EXE de Win32 (formato PE)
;  cuyo subsistema es el de ventanas (2) se abre DENTRO de Hispa Ventanas, como
;  una tarea más, en vez de salir a Hispa-DOS. Los de consola siguen saliendo
;  (allí los ejecuta HW32). Aquí están el cargador, el despacho, la llamada de
;  vuelta y KERNEL32 (fase 3a); USER32 y GDI32 (fase 3b), en win32u.inc.
;
;  Cómo funciona:
;  - El programa se carga en un bloque de memoria extendida (DPMI 0501h):
;    la imagen, sus trampolines, lo suyo (la TIB y la línea de órdenes) y su
;    pila. Tiene SUS selectores (código, datos y pila, y FS para la TIB), con
;    base «bloque − ImageBase» y 4 GB: ve sus direcciones de siempre aunque
;    no traiga reubicados (los .EXE de la época no los traen).
;  - Un puntero del programa se convierte en puntero de Hispa Ventanas
;    sumándole PE_DELTA = (bloque − ImageBase) − [base]. Con 4 GB de límite,
;    la vuelta por 2^32 da la misma dirección lineal.
;  - Cada función importada apunta a un trampolín dentro de su espacio:
;    «push trampolín; jmp lejos pe_despacho», y detrás su ficha (el programa,
;    la función, su nombre y el de su DLL). pe_despacho guarda los registros
;    en la pila del programa, pone los segmentos de Hispa Ventanas, pasa a la
;    pila de Hispa Ventanas de la tarea, llama a la función con EBP = los
;    argumentos (ya como puntero de Hispa Ventanas) y EDX = la ficha del
;    programa, y vuelve con RETF quitando los argumentos (stdcall).
;  - Dos pilas por tarea: la del programa (con su selector) y la de Hispa
;    Ventanas. Solo se cambia de tarea dentro de las funciones (en la pila de
;    Hispa Ventanas y con sus segmentos), así que el cambio de tarea no sabe
;    nada de los selectores del programa.
;  - Lo que no hay: un trampolín que, si se llama, dice «falta la función
;    NOMBRE» y termina el programa.
;  - La llamada de vuelta (pe_llamar): de Hispa Ventanas a una función del
;    programa (su WndProc). Pasa a la pila del programa, debajo de donde
;    estaba en la llamada que se está atendiendo, deja allí los cuatro
;    argumentos y la vuelta a un trampolín suyo, y salta. El trampolín salta
;    lejos a pe_volver, que vuelve a los segmentos y a la pila de Hispa
;    Ventanas. Mientras tanto, PE_ESPV apunta a la pila de Hispa Ventanas por
;    debajo de la llamada: lo que el WndProc llame (pe_despacho) sigue ahí, y
;    se puede anidar las veces que haga falta.
;  Sala limpia: hecho a partir de la documentación pública del formato PE y
;  del API Win32.
;
;    es_pe_gui    ESI=ruta: ZF=1 si es un .EXE de Win32 de ventanas
;    pe_abrir     EBX=acceso directo EBP=ventana desde la que se abre
;    pe_marca_consola  (acc_lanzar) los de consola, por el intérprete de órdenes
;    pe_limpiar   (sistema) suelta los selectores y la memoria de los que han
;                 acabado
;    pe_llamar    EDX=programa EAX=su función (EBX, ECX, ESI, EDI: los cuatro
;                 argumentos) -> EAX
; =============================================================================

PE_MAX      equ 4                   ; programas de Windows a la vez
PE_USO      equ 0                   ; 0 libre, 1 en marcha, 2 terminado
PE_MANGO    equ 4                   ; el bloque de HW32 (mango de DPMI)
PE_TAREA    equ 8
PE_SEL      equ 12                  ; su selector de código; +8 datos y pila, +16 FS
PE_DELTA    equ 16                  ; puntero de Hispa Ventanas = el suyo + esto
PE_IMG      equ 20                  ; la imagen (puntero de Hispa Ventanas)
PE_IMGTAM   equ 24                  ; lo que ocupa (detrás, los trampolines)
PE_BASE     equ 28                  ; ImageBase (su HINSTANCE)
PE_ENTRADA  equ 32                  ; el punto de entrada (suyo)
PE_CIMA     equ 36                  ; la cima de su pila (suya)
PE_CMD      equ 40                  ; su línea de órdenes (suya)
PE_NTRAMP   equ 44                  ; trampolines hechos
PE_ESPV0    equ 48                  ; ESP de Hispa Ventanas al empezar la tarea
PE_ESPV     equ 52                  ; ESP de Hispa Ventanas para la próxima llamada
PE_ESPP     equ 56                  ; su ESP en la llamada que se está atendiendo
PE_ERROR    equ 60                  ; GetLastError
PE_SALIDA   equ 64                  ; el trampolín adonde vuelve su entrada (suyo)
PE_RUTA     equ 68                  ; 79 letras
PE_PARAM    equ 148                 ; 71
; (fase 3b: USER32 y GDI32, win32u.inc)
PE_W32      equ 220                 ; su bloque: clases, objetos de GDI, contextos...
PE_QUIT     equ 224                 ; ha llamado a PostQuitMessage
PE_QCOD     equ 228                 ;   con este código
PE_VUELTA   equ 232                 ; el trampolín adonde vuelven sus funciones (suyo)
PE_TEC_CAR  equ 236                 ; el carácter de la tecla que se acaba de tomar,
PE_TEC_VEN  equ 240                 ;   su HWND
PE_TEC_L    equ 244                 ;   y su lParam (para TranslateMessage)
PE_BOT      equ 248                 ; botones del ratón apretados (MK_LBUTTON, MK_RBUTTON)
PE_NPEND    equ 252                 ; mensajes inventados que esperan
PE_PEND     equ 256                 ;   (HWND, mensaje, wParam, lParam cada uno)
PE_PEND_MAX equ 4
; (fase 3c: HW32 es su módulo, con KERNEL32 y el cargador de DLL)
PE_MOD      equ PE_PEND + PE_PEND_MAX * 16  ; HW32.COM en memoria (su 100h)
PE_MD       equ PE_MOD + 4          ; su mod_d (MD_*)
PE_MODSEL   equ PE_MOD + 8          ; su selector de código (+8: datos)
PE_TRAMPS   equ PE_MOD + 12         ; los trampolines de lo de aquí (del montón)
PE_BLQS     equ PE_MOD + 16         ; (fase 3d) los bloques de DPMI de sus mapas (32)
PE_TEC_MOD  equ PE_BLQS + 32 * 4    ; (fase 3d) Mayús., Ctrl y Alt del último mensaje
PE_RVEN     equ PE_TEC_MOD + 4      ;   su ventana con el ratón encima
PE_CURSOR   equ PE_RVEN + 4         ;   SetCursor
PE_CAPT     equ PE_CURSOR + 4       ;   SetCapture
PE_PP       equ PE_CAPT + 4         ;   lo de GetClipboardData
PE_CAR_VEN  equ PE_PP + 4           ;   el caret: su ventana,
PE_CAR_X    equ PE_CAR_VEN + 4      ;     dónde,
PE_CAR_Y    equ PE_CAR_X + 4
PE_CAR_W    equ PE_CAR_Y + 4        ;     cuánto mide,
PE_CAR_H    equ PE_CAR_W + 4
PE_CAR_N    equ PE_CAR_H + 4        ;     las veces que se ha escondido
PE_CAR_DIB  equ PE_CAR_N + 4        ;     y si está en la pantalla
PE_SICO     equ PE_CAR_DIB + 4      ;   los iconos de serie (6)
PE_TEC_ULT  equ PE_SICO + 6 * 4     ;   el carácter de la última tecla (ToAscii)
PE_DXTIC    equ PE_TEC_ULT + 4      ; [DX] la función de HispaDXTic (suya)
PE_TAM      equ PE_DXTIC + 4

PT_TAM      equ 28                  ; un trampolín: push (5), jmp lejos (7) y
PT_PE       equ 12                  ;   la ficha del programa,
PT_FUN      equ 16                  ;   la función (su ficha en w32_tabla),
PT_NOMBRE   equ 20                  ;   su nombre (del programa; 0: por número)
PT_DLL      equ 24                  ;   y el de la DLL (del programa)
PE_TRAMP_TAM equ 0x2000             ; sitio para los trampolines
PE_TRAMP_MAX equ PE_TRAMP_TAM / PT_TAM
PE_MISC_TAM equ 0x1000              ; lo suyo: la TIB (+0) y la línea de órdenes (+100h)
PE_PILA_MIN equ 0x10000             ; su pila: lo que pida, entre 64 y 256 KB
PE_PILA_MAX equ 0x40000
PE_IMG_MAX  equ 0x4000000           ; (una imagen de más de 64 MB: no)

; la cabecera, en pe_cab desde la firma «PE»
PC_SECC     equ 6                   ; cuántas secciones
PC_OPC_TAM  equ 20                  ; tamaño de la cabecera opcional
PC_CARACT   equ 22
PC_OPC      equ 24                  ; la cabecera opcional
PC_ENTRADA  equ PC_OPC + 16
PC_BASE     equ PC_OPC + 28
PC_IMGTAM   equ PC_OPC + 56
PC_CABTAM   equ PC_OPC + 60
PC_SUBSIS   equ PC_OPC + 68
PC_PILA     equ PC_OPC + 72
PC_NDIR     equ PC_OPC + 92
PC_IMPORT   equ PC_OPC + 96 + 8     ; el directorio de importaciones (RVA)
PC_LEER     equ PC_OPC + 96 + 16

t_ext_exe   db ".EXE", 0
t_pe_mal    db "No se ha podido cargar el programa de Windows.", 0
t_pe_sinhw  db "Falta HW32.COM: hace falta para los programas de Windows.", 0
t_pe_hw32   db "HW32.COM", 0
t_pe_mem    db "No hay memoria para el programa de Windows.", 0
t_pe_muchas db "El programa de Windows usa demasiadas funciones.", 0
t_pe_falta  db "Hispa-486: falta la función ", 0
t_pe_num    db "(por número)", 0
t_pe_titulo db "Hispa-486", 0
t_pe_error  db "Error", 0
t_k32       db "KERNEL32", 0
t_u32       db "USER32", 0
t_cd32      db "COMDLG32", 0        ; (fase 3d)
t_adv32     db "ADVAPI32", 0
t_sh32      db "SHELL32", 0
t_g32       db "GDI32", 0

; ---- ¿es un programa de Windows de ventanas? ESI=ruta -> ZF=1 si lo es
; (acaba en .EXE, es un PE de 386 y su subsistema es el 2). Conserva todo ----
es_pe_gui:
        push eax
        call pe_subsistema
        cmp eax, 2
        pop eax
        ret

; ---- ESI=ruta -> EAX=el subsistema del programa de Windows (2 ventanas,
; 3 consola); 0 si no es un .EXE con cabecera PE de 386 ----
pe_subsistema:
        pushad
        call cad_largo
        sub eax, 4
        jb .no
        add esi, eax
        mov edi, t_ext_exe
        call cad_igual_i
        jne .no
        mov esi, [esp+4]
        xor al, al
        call dos_abrir
        jc .no
        mov ebx, eax
        call pe_leer_cab
        pushf
        call dos_cerrar
        popf
        jc .no
        movzx eax, word [pe_cab+PC_SUBSIS]
        jmp .x
.no:
        xor eax, eax
.x:
        mov [esp+28], eax
        popad
        ret

; ---- (acc_lanzar) EBX=acceso EDX=el lanzador: si el programa que va a
; ejecutar Hispa-DOS es de Windows (de consola), que lo lance el intérprete
; de órdenes (COMSPEC /C), que es quien sabe pasárselo a HW32 ----
pe_marca_consola:
        push eax
        push esi
        lea esi, [ebx+AC_PROG]
        call pe_subsistema
        test eax, eax
        jz .x
        or byte [edx+lz_band], 0x10
.x:
        pop esi
        pop eax
        ret

; ---- lee la cabecera PE del archivo EBX a pe_cab (desde la firma «PE») y
; apunta en pe_lfanew dónde está; CF=1 si no es un PE de 386 ----
pe_leer_cab:
        pushad
        mov edi, pe_cab
        mov ecx, 64
        call dos_leer
        jc .no
        cmp eax, 64
        jne .no
        cmp word [edi], 'MZ'
        jne .no
        mov edx, [edi+0x3C]
        cmp edx, 64
        jb .no
        cmp edx, 0x100000
        ja .no
        mov [pe_lfanew], edx
        xor al, al
        call dos_puntero
        jc .no
        mov ecx, PC_LEER
        call dos_leer
        jc .no
        cmp eax, PC_LEER
        jne .no
        cmp dword [edi], 'PE'
        jne .no
        cmp word [edi+4], 0x014C    ; 386
        jne .no
        cmp word [edi+PC_OPC], 0x010B ; cabecera opcional de 32 bits
        jne .no
        cmp word [edi+PC_OPC_TAM], PC_LEER - PC_OPC
        jb .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; ---- abre un programa de Windows de ventanas ----
pe_abrir:
        pushad
        mov edx, pe_tabla
        mov ecx, PE_MAX
.b:
        cmp dword [edx+PE_USO], 0
        je .hay
        add edx, PE_TAM
        dec ecx
        jnz .b
        mov esi, t_hvp_muchos
        jmp .aviso
.hay:
        mov edi, edx
        mov ecx, PE_TAM / 4
        xor eax, eax
        rep stosd
        lea esi, [ebx+AC_PROG]
        lea edi, [edx+PE_RUTA]
        mov ecx, 79
        call cad_copiar
        lea esi, [ebx+AC_PARAM]
        lea edi, [edx+PE_PARAM]
        mov ecx, 71
        call cad_copiar
        call dx_carpeta             ; [DX] la carpeta de trabajo: la suya
        mov dword [edx+PE_USO], 2   ; (si algo falla, pe_limpiar lo suelta)
        mov ecx, PE_TRAMP_TAM       ; el sitio de sus trampolines
        call mem_pedir
        mov esi, t_pe_mem
        jc .aviso
        mov [edx+PE_TRAMPS], eax
        call pe_modulo              ; HW32 y, con él, el programa y sus DLL
        jc .aviso                   ; (ESI = el aviso)
        mov esi, t_pe_muchas
        mov eax, w32_e_salida       ; adonde vuelve la entrada si hace «ret»
        xor ecx, ecx
        xor edi, edi
        call pe_tramp
        jc .aviso
        mov [edx+PE_SALIDA], eax
        mov eax, w32_e_salida       ; y adonde vuelven sus funciones (pe_llamar):
        xor ecx, ecx                ; como los demás, pero salta a pe_volver
        xor edi, edi
        call pe_tramp
        jc .aviso
        mov [edx+PE_VUELTA], eax
        add eax, [edx+PE_DELTA]
        mov dword [eax+6], pe_volver
        mov esi, t_pe_mem           ; su bloque de USER32 y GDI32
        call w32_iniciar
        jc .aviso
        ; y la tarea
        mov dword [edx+PE_USO], 1
        mov eax, pe_tarea
        call tar_crear
        mov [edx+PE_TAREA], eax
        test eax, eax
        jz .sintarea
        mov ebx, [edx+PE_W32]       ; (su bloque queda apuntado a su tarea: si
        mov [ebx-4], eax            ; la terminan a la fuerza, se suelta con ella)
        jmp .x
.sintarea:
        mov eax, [edx+PE_W32]
        call mem_soltar
        mov dword [edx+PE_USO], 2
        mov esi, t_hvp_muchos
.aviso:
        mov ebp, [esp+8]            ; (la ventana desde la que se abre)
        call cen_aviso
.x:
        popad
        ret

; (interna) el selector EBX con la base EDI (lineal), 4 GB y los derechos CX
pe_selector:
        pushad
        mov edx, edi
        mov ecx, edi
        shr ecx, 16
        mov ax, 0x0007
        int 0x31
        mov ecx, 0xFFFF
        mov edx, 0xFFFF
        mov ax, 0x0008
        int 0x31
        mov ecx, [esp+24]
        mov ax, 0x0009
        int 0x31
        popad
        ret

; ---- (interna) carga HW32.COM como módulo del programa EDX y, con él, el
; programa y sus DLL (fase 3c). HW32.COM se busca al lado de VENTANAS.COM, al
; lado del programa y en la carpeta de ahora. CF=1 si no se puede: ESI = el
; aviso. Lo que deja a medias lo suelta pe_limpiar ----
HM_FIRMA    equ 3                   ; «HW32MOD», 1 (en el archivo)
HM_CARGAR   equ 0x0B                ; mod_cargar (lejos)
HM_FIN      equ 0x0F                ; mod_fin (lejos)
HM_DATOS    equ 0x13                ; mod_d
HM_NOMBRE   equ 0x17                ; el programa
HM_PARAM    equ 0x1B                ; sus parámetros
HM_DIR      equ 0x1F                ; la carpeta de HW32
MD_MODULO   equ 0
MD_CS       equ 4
MD_DS       equ 8
MD_ENV      equ 12
MD_PSP      equ 16
MD_FICHA    equ 20
MD_RESOLVER equ 24
MD_SALIR    equ 32
MD_FALTA    equ 40
MD_SELC     equ 48
MD_BASE     equ 60
MD_IMG      equ 64
MD_ENTRADA  equ 68
MD_ESP      equ 72
MD_ERROR    equ 76
MD_ERROR2   equ 80
pe_modulo:
        pushad
        mov edi, pe_ruta            ; 1.º al lado de VENTANAS.COM
        mov esi, dir_propia
        call pon_cad
        call .abre
        jnc .abierto
        mov edi, pe_ruta            ; 2.º al lado del programa
        lea esi, [edx+PE_RUTA]
        mov ebx, edi
.c:
        lodsb
        test al, al
        jz .cf
        stosb
        cmp al, '\'
        je .cs
        cmp al, ':'
        jne .c
.cs:
        mov ebx, edi
        jmp .c
.cf:
        mov edi, ebx
        call .abre
        jnc .abierto
        mov edi, pe_ruta            ; 3.º en la carpeta de ahora
        call .abre
        jnc .abierto
        mov dword [esp+4], t_pe_sinhw
        jmp .mal
.abierto:
        mov ebx, eax                ; EBX = el archivo
        mov [pe_dirfin], edi        ; (dónde acaba la carpeta en pe_ruta)
        push edx
        mov al, 2
        xor edx, edx
        call dos_puntero
        mov ecx, eax                ; ECX = lo que ocupa
        xor al, al
        xor edx, edx
        call dos_puntero
        pop edx
        cmp ecx, 0x200
        jb .malc
        cmp ecx, 0xFFF0
        ja .malc
        push edx
        push ecx
        add ecx, 0x1000
        call mem_bloque             ; EAX = dónde, EDX = su mango
        mov esi, edx
        pop ecx
        pop edx
        jc .malm
        mov [edx+PE_MANGO], esi
        mov [edx+PE_MOD], eax
        mov edi, eax
        call dos_leer
        jc .malc
        cmp eax, ecx
        jne .malc
        call dos_cerrar
        mov ebp, [edx+PE_MOD]       ; EBP = HW32 (su 100h)
        cmp dword [ebp+HM_FIRMA], 'HW32'
        jne .malv
        cmp dword [ebp+HM_FIRMA+4], 'MOD' | (1 << 24)
        jne .malv
        ; sus dos selectores: código y datos, con la base en «copia − 100h»
        push edx
        xor eax, eax
        mov ecx, 2
        int 0x31
        pop edx
        jc .malm2
        movzx eax, ax
        mov [edx+PE_MODSEL], eax
        mov ebx, eax
        mov edi, ebp
        add edi, [base]
        sub edi, 0x100
        mov ecx, 0xC0FA
        call pe_selector
        add ebx, 8
        mov ecx, 0xC0F2
        call pe_selector
        ; lo que se le da
        mov eax, [ebp+HM_DATOS]
        lea ebx, [ebp+eax-0x100]    ; EBX = mod_d
        mov [edx+PE_MD], ebx
        mov dword [ebx+MD_MODULO], 1
        mov eax, [edx+PE_MODSEL]
        mov [ebx+MD_CS], eax
        add eax, 8
        mov [ebx+MD_DS], eax
        movzx eax, word [env_seg]
        mov [ebx+MD_ENV], eax
        movzx eax, word [rmseg]
        mov [ebx+MD_PSP], eax
        mov [ebx+MD_FICHA], edx
        movzx eax, word [sel_cod]
        mov dword [ebx+MD_RESOLVER], pe_resolver
        mov [ebx+MD_RESOLVER+4], eax
        mov dword [ebx+MD_SALIR], pe_salir_mod
        mov [ebx+MD_SALIR+4], eax
        mov dword [ebx+MD_FALTA], pe_falta_mod
        mov [ebx+MD_FALTA+4], eax
        mov eax, [ebp+HM_NOMBRE]    ; el programa
        lea edi, [ebp+eax-0x100]
        lea esi, [edx+PE_RUTA]
        mov ecx, 120
        call cad_copiar
        mov eax, [ebp+HM_PARAM]     ; sus parámetros, con el espacio delante
        lea edi, [ebp+eax-0x100]
        cmp byte [edx+PE_PARAM], 0
        je .sinp
        mov byte [edi], ' '
        inc edi
        lea esi, [edx+PE_PARAM]
        mov ecx, 120
        call cad_copiar
.sinp:
        mov eax, [ebp+HM_DIR]       ; y la carpeta de HW32 (donde busca las DLL)
        lea edi, [ebp+eax-0x100]
        mov esi, pe_ruta
        mov ecx, [pe_dirfin]
        sub ecx, esi
        cmp ecx, 120
        jbe .d1
        xor ecx, ecx
.d1:
        rep movsb
        mov byte [edi], 0
        ; a cargar
        push dword [edx+PE_MODSEL]
        push dword [ebp+HM_CARGAR]
        call dword far [esp]
        add esp, 8
        test eax, eax
        jnz .malp
        mov eax, [ebx+MD_SELC]
        mov [edx+PE_SEL], eax
        mov eax, [ebx+MD_BASE]
        sub eax, [base]
        mov [edx+PE_DELTA], eax
        mov ecx, [ebx+MD_IMG]
        mov [edx+PE_BASE], ecx
        add eax, ecx
        mov [edx+PE_IMG], eax
        mov ecx, [eax+0x3C]         ; (lo que ocupa: de su cabecera)
        mov ecx, [eax+ecx+PC_IMGTAM]
        mov [edx+PE_IMGTAM], ecx
        mov eax, [ebx+MD_ENTRADA]
        mov [edx+PE_ENTRADA], eax
        mov eax, [ebx+MD_ESP]
        mov [edx+PE_CIMA], eax
        popad
        clc
        ret
; no carga: lo que dice HW32 (y su segunda parte: la DLL que falta)
.malp:
        mov edi, pe_ruta
        mov esi, [ebx+MD_ERROR]
        test esi, esi
        jz .malv
        lea esi, [ebp+esi-0x100]
        mov ecx, 100
        call w32_pon
        mov esi, [ebx+MD_ERROR2]
        test esi, esi
        jz .mp2
        lea esi, [ebp+esi-0x100]
        mov ecx, 40
        call w32_pon
.mp2:
        mov byte [edi], 0
        mov dword [esp+4], pe_ruta
        jmp .mal
.malm2:
.malm:
        mov dword [esp+4], t_pe_mem
        call dos_cerrar
        jmp .mal
.malc:
        call dos_cerrar
.malv:
        mov dword [esp+4], t_pe_mal
.mal:
        popad
        stc
        ret
; (de pe_modulo) añade «HW32.COM» a la ruta que acaba en EDI y lo abre ->
; EAX=el archivo; CF=1 si no está. EDI no cambia
.abre:
        push edi
        mov esi, t_pe_hw32
        call pon_cad
        mov byte [edi], 0
        mov esi, pe_ruta
        xor al, al
        call dos_abrir
        pop edi
        ret

; (interna) EAX=RVA EDX=ficha -> EAX=puntero de Hispa Ventanas; CF=1 si cae
; fuera de la imagen
pe_rva:
        cmp eax, [edx+PE_IMGTAM]
        jae .no
        add eax, [edx+PE_IMG]
        clc
        ret
.no:
        stc
        ret

; (interna) un trampolín nuevo para el programa EDX: EAX=la función (su ficha
; de w32_tabla), ECX=su nombre y EDI=el de su DLL (del programa, o 0)
; -> EAX=el trampolín (dirección del programa); CF=1 si no caben más
pe_tramp:
        push ebx
        mov ebx, [edx+PE_NTRAMP]
        cmp ebx, PE_TRAMP_MAX
        jae .no
        inc dword [edx+PE_NTRAMP]
        imul ebx, PT_TAM
        add ebx, [edx+PE_TRAMPS]
        mov byte [ebx], 0x68        ; push trampolín
        mov [ebx+1], ebx
        mov byte [ebx+5], 0xEA      ; jmp lejos pe_despacho
        mov dword [ebx+6], pe_despacho
        mov [ebx+PT_FUN], eax
        mov ax, [sel_cod]
        mov [ebx+10], ax
        mov [ebx+PT_PE], edx
        mov [ebx+PT_NOMBRE], ecx
        mov [ebx+PT_DLL], edi
        mov eax, ebx
        sub eax, [edx+PE_DELTA]
        pop ebx
        clc
        ret
.no:
        pop ebx
        stc
        ret

; ---- (lejos, desde HW32) lo que el cargador y KERNEL32 le piden al entorno.
; EDX=la ficha del programa; todo lo demás, direcciones del programa.
;   AL=0: la función de nombre EDI (0: por número) de la DLL de nombre ESI
;         -> EAX=su trampolín. De USER32 y GDI32, siempre (la que no hay, con
;         su «falta»); de KERNEL32, solo las que se hacen aquí (0: las demás);
;         de otra DLL, 0 (la carga HW32)
;   AL=1: la DLL de nombre ESI -> EAX=su mango (0: no es de aquí)
;   AL=2: la función de nombre EDI del mango EBX -> EAX (0: no la hay)
; Conserva lo demás. Llega con la pila de HW32 (al cargar) o con la del
; programa: la pila solo se usa para guardar, y nada de [EBP] ni [ESP] ----
H_KERNEL32  equ 0xBFF70000
H_USER32    equ 0xBFF50000
H_GDI32     equ 0xBFF20000
H_OTRAS     equ 0xBFC00000          ; (fase 3d) + DLL << 16: COMDLG32, ADVAPI32, SHELL32
pe_resolver:
        push ds
        push es
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov cx, [cs:sel_dat]
        mov ds, cx
        mov es, cx
        cld
        mov ebp, [edx+PE_MD]        ; (al cargar aún no está apuntado)
        mov ebp, [ds:ebp+MD_BASE]   ; (con DS: la pila no es la nuestra)
        sub ebp, [base]
        mov [edx+PE_DELTA], ebp
        cmp al, 2
        je .mango
        add esi, ebp
        call pe_que_dll             ; -> BL
        cmp al, 1
        je .dll
        cmp bl, 0xFF
        je .cero
        mov ecx, edi                ; ECX = el nombre (del programa)
        mov eax, w32_e_falta
        test edi, edi
        jz .f1
        lea eax, [edi+ebp]
        call w32_busca
.f1:
        cmp eax, w32_e_falta
        jne .pon
        test bl, bl                 ; de KERNEL32: la tiene HW32
        jz .cero
.pon:
        mov edi, esi
        sub edi, ebp                ; EDI = el nombre de la DLL (del programa)
        call pe_tramp
        jnc .x
.cero:
        xor eax, eax
.x:
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        pop es
        pop ds
        retf
.dll:
        mov eax, H_USER32
        cmp bl, 1
        je .x
        mov eax, H_GDI32
        cmp bl, 2
        je .x
        movzx eax, bl               ; (fase 3d) COMDLG32, ADVAPI32 y SHELL32
        cmp eax, 3
        jb .cero
        cmp eax, 5
        ja .cero
        shl eax, 16
        add eax, H_OTRAS
        jmp .x
.mango:
        mov ecx, ebx
        xor bl, bl
        cmp ecx, H_KERNEL32
        je .m1
        inc bl
        cmp ecx, H_USER32
        je .m1
        inc bl
        cmp ecx, H_GDI32
        je .m1
        mov eax, ecx                ; (fase 3d) las otras
        sub eax, H_OTRAS + 0x30000
        jb .cero
        shr eax, 16
        cmp eax, 2
        ja .cero
        lea ebx, [eax+3]
.m1:
        lea eax, [edi+ebp]
        call w32_busca
        cmp eax, w32_e_falta
        je .cero
        mov esi, [edx+PE_TRAMPS]    ; ¿ya tiene trampolín?
        mov ecx, [edx+PE_NTRAMP]
.m2:
        jecxz .m3
        cmp [esi+PT_FUN], eax
        je .m4
        add esi, PT_TAM
        dec ecx
        jmp .m2
.m3:
        mov ecx, edi
        xor edi, edi
        call pe_tramp
        jnc .x
        jmp .cero
.m4:
        mov eax, esi
        sub eax, ebp
        jmp .x
; (interna) ESI=nombre de una DLL -> BL=0 KERNEL32, 1 USER32, 2 GDI32, 3 COMDLG32,
; FFh otra
pe_que_dll:
        push edi
        mov bl, 0
        mov edi, t_k32
        call pe_es_dll
        je .x
        mov bl, 1
        mov edi, t_u32
        call pe_es_dll
        je .x
        mov bl, 2
        mov edi, t_g32
        call pe_es_dll
        je .x
        mov bl, 3                   ; (fase 3d)
        mov edi, t_cd32
        call pe_es_dll
        je .x
        mov bl, 4
        mov edi, t_adv32
        call pe_es_dll
        je .x
        mov bl, 5
        mov edi, t_sh32
        call pe_es_dll
        je .x
        mov bl, 0xFF
.x:
        pop edi
        ret

; ---- (lejos, desde HW32: ExitProcess y demás) el programa EDX acaba ----
pe_salir_mod:
        mov cx, [cs:sel_dat]
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        mov ss, cx
        mov esp, [edx+PE_ESPV0]
        cld
        jmp pe_salir
; ---- (lejos, desde HW32) un aviso y el programa EDX acaba. ECX=0: falta la
; función de nombre EAX (del programa; 0: por número); 1: el texto EAX (de
; HW32); 2: el texto EAX (del programa) ----
pe_falta_mod:
        mov bx, [cs:sel_dat]
        mov ds, bx
        mov es, bx
        mov fs, bx
        mov gs, bx
        mov ss, bx
        mov esp, [edx+PE_ESPV0]
        cld
        mov dword [edx+PE_ESPP], 0  ; (a sus funciones ya no se las llama)
        sub esp, 192
        mov edi, esp
        mov esi, eax
        cmp ecx, 1
        je .hw
        ja .prog
        mov esi, t_pe_falta
        call pon_cad
        mov esi, t_pe_num
        test eax, eax
        jz .hw2
        mov esi, eax
.prog:
        add esi, [edx+PE_DELTA]
        jmp .hw2
.hw:
        add esi, [edx+PE_MOD]
        sub esi, 0x100
.hw2:
        mov ecx, 150
        call w32_pon
        mov byte [edi], 0
        mov esi, esp
        mov edi, t_pe_titulo
        xor ebp, ebp
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        add esp, 192
        jmp pe_salir

; (interna) ¿el nombre de DLL ESI es EDI (en mayúsculas), con «.DLL» o sin
; él? ZF=1 si lo es
pe_es_dll:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        mov ah, [edi]
        test ah, ah
        jz .f
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        cmp al, ah
        jne .x
        inc esi
        inc edi
        jmp .c
.f:
        test al, al
        jz .x
        cmp al, '.'
.x:
        pop edi
        pop esi
        pop eax
        ret

; (interna) EAX=nombre de la función BL=DLL (0 KERNEL32, 1 USER32, 2 GDI32, 3 COMDLG32)
; -> EAX=su ficha en w32_tabla (w32_e_falta si no está)
w32_busca:
        push ecx
        push esi
        push edi
        mov esi, w32_tabla
.b:
        cmp dword [esi], 0
        je .no
        lea edi, [esi+7]
        cmp [esi+6], bl
        jne .sig
        push eax
.c:
        mov cl, [eax]
        cmp cl, [edi]
        jne .dif
        inc eax
        inc edi
        test cl, cl
        jnz .c
        pop eax
        mov eax, esi
        jmp .x
.dif:
        pop eax
        lea edi, [esi+7]
.sig:
        cmp byte [edi], 0           ; a la siguiente: tras el 0 del nombre
        je .s1
        inc edi
        jmp .sig
.s1:
        lea esi, [edi+1]
        jmp .b
.no:
        mov eax, w32_e_falta
.x:
        pop edi
        pop esi
        pop ecx
        ret

; ---- la entrada de la tarea de un programa de Windows: pasa a su pila y a
; sus segmentos y salta a su punto de entrada. No vuelve: el programa acaba
; en pe_salir (ExitProcess, o el «ret» de su entrada) ----
pe_tarea:
        mov edx, pe_tabla - PE_TAM
        mov eax, [tar_act]
.b:
        add edx, PE_TAM
        cmp dword [edx+PE_USO], 1
        jne .b
        cmp [edx+PE_TAREA], eax
        jne .b
        mov [edx+PE_ESPV0], esp
        mov [edx+PE_ESPV], esp
        mov edi, [edx+PE_CIMA]
        sub edi, 20                 ; (un hueco en la cima) y la vuelta
        mov eax, [edx+PE_SALIDA]
        mov ebx, edi
        add ebx, [edx+PE_DELTA]
        mov [ebx], eax
        mov eax, [edx+PE_SEL]
        lea ebx, [eax+8]
        lea ecx, [eax+16]
        mov esi, [edx+PE_ENTRADA]
        mov fs, cx
        mov es, bx
        mov gs, bx
        mov ss, bx
        mov esp, edi
        push eax
        push esi
        mov ds, bx
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        xor ebp, ebp
        retf

; ---- el despacho. Llega con los segmentos y la pila del programa:
; [ESP] = el trampolín, [ESP+4] = la vuelta al programa, [ESP+8...] = los
; argumentos. Las funciones reciben EBP = los argumentos, EDX = la ficha del
; programa y EBX = el trampolín, ya con los segmentos y la pila de Hispa
; Ventanas; devuelven EAX (y EDX) y pueden cambiar los demás registros ----
pe_despacho:
        push ds
        push es
        push fs
        push gs
        push ebp
        push ebx
        push esi
        push edi
        mov ax, [cs:sel_dat]
        mov ds, ax
        mov es, ax
        mov fs, ax                  ; (que no quede ningún selector suyo
        mov gs, ax                  ; puesto mientras corre lo demás)
        cld
        mov ebx, [esp+32]           ; el trampolín
        mov edx, [ebx+PT_PE]
        mov esi, esp                ; su pila
        mov edi, [edx+PE_ESPV]
        mov ss, ax
        mov esp, edi                ; la de Hispa Ventanas
        push dword [edx+PE_ESPP]    ; (se puede anidar: lo de la llamada de
        push edx                    ; antes se guarda en esta pila)
        push esi
        mov [edx+PE_ESPP], esi
        lea ebp, [esi+40]
        add ebp, [edx+PE_DELTA]
        mov eax, [ebx+PT_FUN]
        movzx ecx, word [eax+4]     ; los octetos de argumentos
        push ecx
        mov [wg_prog], edx          ; (fase 3d: el programa, para GDI32)
        call [eax]
        pop ecx
        pop esi
        pop ebx
        pop dword [ebx+PE_ESPP]
        mov edi, [ebx+PE_SEL]
        add edi, 8
        mov ss, di
        mov esp, esi                ; otra vez su pila
        sub edi, 8
        mov esi, [esp+36]           ; la vuelta, encima de los argumentos (RETF)
        mov [esp+32+ecx], esi
        mov [esp+36+ecx], edi
        pop edi
        pop esi
        pop ebx
        pop ebp
        pop gs
        pop fs
        pop es
        pop ds
        add esp, ecx
        retf

; ---- el programa EDX termina: de vuelta al principio de su tarea (en la
; pila de Hispa Ventanas), que acaba como cualquier otra (tar_fin destruye
; sus ventanas); los selectores y la memoria los suelta pe_limpiar ----
pe_salir:
        mov esp, [edx+PE_ESPV0]
        mov dword [edx+PE_USO], 2   ; (a sus ventanas ya no se las avisa)
        mov ebx, [tar_act]
        call ges_destruir_de_tarea
        mov eax, [edx+PE_W32]       ; su bloque de USER32 y GDI32
        mov dword [edx+PE_W32], 0
        call mem_soltar
        ret

; ---- la llamada de vuelta: llama a la función EAX del programa EDX (un
; WndProc: stdcall, cuatro argumentos, EBX, ECX, ESI y EDI) -> EAX. Solo desde
; la tarea del programa y mientras atiende una llamada suya; si no (o si el
; programa ya ha acabado), no llama y devuelve 0. Conserva los registros ----
pe_llamar:
        pushad
        mov dword [esp+28], 0
        test eax, eax
        jz .no
        cmp dword [edx+PE_USO], 1
        jne .no
        call w32_en_tarea
        jne .no
        mov ebp, [edx+PE_ESPP]      ; su pila, donde la dejó al llamar
        test ebp, ebp
        jz .no
        push dword [edx+PE_ESPV]
        push ebp
        push edx
        mov [edx+PE_ESPV], esp      ; lo que llame desde dentro, de aquí para abajo
        sub ebp, 24
        and ebp, ~3
        add ebp, [edx+PE_DELTA]     ; los argumentos y la vuelta, en su pila
        mov [ebp+4], ebx
        mov [ebp+8], ecx
        mov [ebp+12], esi
        mov [ebp+16], edi
        mov ecx, [edx+PE_VUELTA]
        mov [ebp], ecx
        sub ebp, [edx+PE_DELTA]
        mov ecx, [edx+PE_SEL]
        lea esi, [ecx+8]
        lea edi, [ecx+16]
        mov fs, di
        mov es, si
        mov gs, si
        mov ss, si
        mov esp, ebp
        push ecx
        push eax
        mov ds, si
        xor ebp, ebp
        retf
.no:
        popad
        ret
; aquí vuelve (por el trampolín PE_VUELTA, que ha empujado su dirección) la
; función del programa, con sus segmentos y su pila; EAX = lo que devuelve
pe_volver:
        mov cx, [cs:sel_dat]
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        cld
        pop ebx                     ; el trampolín
        mov edx, [ebx+PT_PE]
        mov ss, cx
        mov esp, [edx+PE_ESPV]
        pop edx
        pop dword [edx+PE_ESPP]
        pop dword [edx+PE_ESPV]
        mov [esp+28], eax
        popad
        ret

; ---- (sistema) suelta los selectores y la memoria de los programas de
; Windows que han acabado, y de los que ha terminado el Administrador de
; tareas (su tarea ya no está) ----
pe_limpiar:
        pushad
        mov edx, pe_tabla
        mov ebp, PE_MAX
.b:
        cmp dword [edx+PE_USO], 1
        jne .b2
        mov eax, [edx+PE_TAREA]
        test eax, eax
        jz .s
        cmp dword [eax+T_USO], 1
        je .s
        cmp dword [eax+T_USO], T_PARADA
        je .s
        mov dword [edx+PE_USO], 2
.b2:
        cmp dword [edx+PE_USO], 2
        jne .s
        call dx_fin_prog            ; [DX] si tenía la pantalla, se devuelve
        mov esi, [edx+PE_MOD]
        test esi, esi
        jz .m
        mov ebx, [edx+PE_MODSEL]
        test ebx, ebx
        jz .m0
        push ebx                    ; HW32 suelta lo del programa (su memoria,
        push dword [esi+HM_FIN]     ; sus DLL, su montón, sus archivos y sus
        call dword far [esp]        ; selectores)
        add esp, 8
        mov ax, 0x0001              ; y aquí, HW32: sus selectores y su bloque
        int 0x31
        add ebx, 8
        mov ax, 0x0001
        int 0x31
.m0:
        push edx
        mov edx, [edx+PE_MANGO]
        call mem_bloque_soltar
        pop edx
.m:
        mov eax, [edx+PE_TRAMPS]
        call mem_soltar
        call wg_limpiar             ; (fase 3d) los bloques de sus mapas
        mov dword [edx+PE_USO], 0
.s:
        add edx, PE_TAM
        dec ebp
        jnz .b
        popad
        ret

; =============================================================================
;  Las funciones. Cada ficha: la rutina (4), los octetos de argumentos (2),
;  la DLL (1: 0 KERNEL32, 1 USER32, 2 GDI32, 3 COMDLG32) y el nombre.
; =============================================================================
%macro W32F 4
        dd %1
        dw %2
        db %3
        db %4, 0
%endmacro
w32_e_falta:                        ; (las dos de dentro: sin nombre)
        dd pe_f_falta
        dw 0
        db 0xFE, 0
w32_e_salida:
        dd k_ExitProcess
        dw 0
        db 0xFE, 0
w32_tabla:
        W32F k_Sleep, 4, 0, 'Sleep'
        W32F k_FindResourceA, 12, 0, 'FindResourceA'
        W32F k_LoadResource, 8, 0, 'LoadResource'
        W32F k_LockResource, 4, 0, 'LockResource'
        W32F k_SizeofResource, 8, 0, 'SizeofResource'
        W32F u_MessageBoxA, 16, 1, 'MessageBoxA'
        W32F u_HispaDXPantalla, 16, 1, 'HispaDXPantalla'    ; [DX] fase 5 (win32dx.inc)
        W32F u_HispaDXSonido, 16, 1, 'HispaDXSonido'        ; [DX]
        W32F u_HispaDXEntrada, 12, 1, 'HispaDXEntrada'      ; [DX]
        W32F u_HispaDXTic, 4, 1, 'HispaDXTic'               ; [DX]
        W32F u_ShowCursor, 4, 1, 'ShowCursor'               ; [DX]
        W32F u_uno, 4, 1, 'ClipCursor'                      ; [DX]
        W32F u_CreateMenu, 8, 1, 'GetSystemMenu'            ; [DX] (un menú vacío: el del sistema es el de Hispa Ventanas)
        W32F u_cero, 16, 1, 'GetMenuItemInfoA'              ; [DX] lo justo para FCE Ultra: se dice que
        W32F u_uno, 16, 1, 'SetMenuItemInfoA'               ;   no se sabe (0) o que se ha hecho, sin
        W32F u_uno, 16, 1, 'InsertMenuItemA'                ;   tocar el menú (los menús por posición
        W32F u_uno, 12, 1, 'RemoveMenu'                     ;   y MENUITEMINFO, más adelante)
        W32F u_cero, 12, 1, 'GetKeyNameTextA'               ; [DX]
        ; fase 3b: USER32 (win32u.inc)
        W32F u_RegisterClassA, 4, 1, 'RegisterClassA'
        W32F u_RegisterClassExA, 4, 1, 'RegisterClassExA'
        W32F u_UnregisterClassA, 8, 1, 'UnregisterClassA'
        W32F u_CreateWindowExA, 48, 1, 'CreateWindowExA'
        W32F u_ShowWindow, 8, 1, 'ShowWindow'
        W32F u_UpdateWindow, 4, 1, 'UpdateWindow'
        W32F u_DestroyWindow, 4, 1, 'DestroyWindow'
        W32F u_DefWindowProcA, 16, 1, 'DefWindowProcA'
        W32F u_GetMessageA, 16, 1, 'GetMessageA'
        W32F u_PeekMessageA, 20, 1, 'PeekMessageA'
        W32F u_TranslateMessage, 4, 1, 'TranslateMessage'
        W32F u_DispatchMessageA, 4, 1, 'DispatchMessageA'
        W32F u_PostQuitMessage, 4, 1, 'PostQuitMessage'
        W32F u_PostMessageA, 16, 1, 'PostMessageA'
        W32F u_SendMessageA, 16, 1, 'SendMessageA'
        W32F u_GetClientRect, 8, 1, 'GetClientRect'
        W32F u_GetWindowRect, 8, 1, 'GetWindowRect'
        W32F u_InvalidateRect, 12, 1, 'InvalidateRect'
        W32F u_ValidateRect, 8, 1, 'ValidateRect'
        W32F u_MoveWindow, 24, 1, 'MoveWindow'
        W32F u_SetWindowTextA, 8, 1, 'SetWindowTextA'
        W32F u_GetWindowTextA, 12, 1, 'GetWindowTextA'
        W32F u_GetWindowTextLengthA, 4, 1, 'GetWindowTextLengthA'
        W32F u_GetWindowLongA, 8, 1, 'GetWindowLongA'
        W32F u_SetWindowLongA, 12, 1, 'SetWindowLongA'
        W32F u_SetTimer, 16, 1, 'SetTimer'
        W32F u_KillTimer, 8, 1, 'KillTimer'
        W32F u_LoadCursorA, 8, 1, 'LoadCursorA'
        W32F u_LoadIconA, 8, 1, 'LoadIconA'
        W32F u_SetCursor, 4, 1, 'SetCursor'
        W32F u_uno, 4, 1, 'MessageBeep'
        W32F u_GetSystemMetrics, 4, 1, 'GetSystemMetrics'
        W32F u_GetSysColor, 4, 1, 'GetSysColor'
        W32F u_GetSysColorBrush, 4, 1, 'GetSysColorBrush'
        W32F u_GetDC, 4, 1, 'GetDC'
        W32F u_ReleaseDC, 8, 1, 'ReleaseDC'
        W32F u_BeginPaint, 8, 1, 'BeginPaint'
        W32F u_EndPaint, 8, 1, 'EndPaint'
        W32F u_FillRect, 12, 1, 'FillRect'
        W32F u_DrawTextA, 20, 1, 'DrawTextA'
        W32F u_IsWindow, 4, 1, 'IsWindow'
        W32F u_IsWindowVisible, 4, 1, 'IsWindowVisible'
        W32F u_IsIconic, 4, 1, 'IsIconic'
        W32F u_IsZoomed, 4, 1, 'IsZoomed'
        W32F u_GetParent, 4, 1, 'GetParent'
        W32F u_SetFocus, 4, 1, 'SetFocus'
        W32F u_GetFocus, 0, 1, 'GetFocus'
        W32F u_wsprintfA, 0, 1, 'wsprintfA'
        ; fase 3c: los controles (win32c.inc)
        W32F u_GetDlgItem, 8, 1, 'GetDlgItem'
        W32F u_GetDlgCtrlID, 4, 1, 'GetDlgCtrlID'
        W32F u_SendDlgItemMessageA, 20, 1, 'SendDlgItemMessageA'
        W32F u_SetDlgItemTextA, 12, 1, 'SetDlgItemTextA'
        W32F u_GetDlgItemTextA, 16, 1, 'GetDlgItemTextA'
        W32F u_SetDlgItemInt, 16, 1, 'SetDlgItemInt'
        W32F u_GetDlgItemInt, 16, 1, 'GetDlgItemInt'
        W32F u_CheckDlgButton, 12, 1, 'CheckDlgButton'
        W32F u_IsDlgButtonChecked, 8, 1, 'IsDlgButtonChecked'
        W32F u_CheckRadioButton, 16, 1, 'CheckRadioButton'
        W32F u_EnableWindow, 8, 1, 'EnableWindow'
        W32F u_IsWindowEnabled, 4, 1, 'IsWindowEnabled'
        W32F u_SetWindowPos, 28, 1, 'SetWindowPos'
        W32F u_cero, 8, 1, 'IsDialogMessageA'
        ; fase 3c: los menús, los recursos y los diálogos (win32m.inc)
        W32F u_CreateMenu, 0, 1, 'CreateMenu'
        W32F u_CreateMenu, 0, 1, 'CreatePopupMenu'
        W32F u_AppendMenuA, 16, 1, 'AppendMenuA'
        W32F u_DestroyMenu, 4, 1, 'DestroyMenu'
        W32F u_SetMenu, 8, 1, 'SetMenu'
        W32F u_GetMenu, 4, 1, 'GetMenu'
        W32F u_GetSubMenu, 8, 1, 'GetSubMenu'
        W32F u_DrawMenuBar, 4, 1, 'DrawMenuBar'
        W32F u_CheckMenuItem, 12, 1, 'CheckMenuItem'
        W32F u_EnableMenuItem, 12, 1, 'EnableMenuItem'
        W32F u_GetMenuState, 12, 1, 'GetMenuState'
        W32F u_GetMenuItemCount, 4, 1, 'GetMenuItemCount'
        W32F u_LoadStringA, 16, 1, 'LoadStringA'
        W32F u_LoadMenuA, 8, 1, 'LoadMenuA'
        W32F u_DialogBoxParamA, 20, 1, 'DialogBoxParamA'
        W32F u_CreateDialogParamA, 20, 1, 'CreateDialogParamA'
        W32F u_EndDialog, 8, 1, 'EndDialog'
        ; fase 3b: GDI32
        W32F g_TextOutA, 20, 2, 'TextOutA'
        W32F g_SetTextColor, 8, 2, 'SetTextColor'
        W32F g_SetBkColor, 8, 2, 'SetBkColor'
        W32F g_SetBkMode, 8, 2, 'SetBkMode'
        W32F g_SetTextAlign, 8, 2, 'SetTextAlign'
        W32F g_GetTextColor, 4, 2, 'GetTextColor'
        W32F g_GetBkColor, 4, 2, 'GetBkColor'
        W32F g_GetStockObject, 4, 2, 'GetStockObject'
        W32F g_CreateSolidBrush, 4, 2, 'CreateSolidBrush'
        W32F g_CreatePen, 12, 2, 'CreatePen'
        W32F g_SelectObject, 8, 2, 'SelectObject'
        W32F g_DeleteObject, 4, 2, 'DeleteObject'
        W32F g_Rectangle, 20, 2, 'Rectangle'
        W32F g_MoveToEx, 16, 2, 'MoveToEx'
        W32F g_LineTo, 12, 2, 'LineTo'
        W32F g_SetPixel, 16, 2, 'SetPixel'
        W32F g_GetTextExtentPoint32A, 16, 2, 'GetTextExtentPoint32A'
        W32F g_GetTextExtentPoint32A, 16, 2, 'GetTextExtentPointA'
        W32F g_GetTextMetricsA, 8, 2, 'GetTextMetricsA'
        ; [NAVEGADOR] (win32n.inc)
        W32F k_WinExec, 8, 0, 'WinExec'
        ; fase 3d: GDI32 (win32g.inc, win32t.inc, win32b.inc, win32r.inc, win32ga.inc)
        W32F g_TextOutW, 20, 2, 'TextOutW'
        W32F g_ExtTextOutA, 32, 2, 'ExtTextOutA'
        W32F g_ExtTextOutW, 32, 2, 'ExtTextOutW'
        W32F g_TabbedTextOutA, 32, 2, 'TabbedTextOutA'
        W32F g_GetTabbedTextExtentA, 20, 2, 'GetTabbedTextExtentA'
        W32F g_GetTextExtentPoint32W, 16, 2, 'GetTextExtentPoint32W'
        W32F g_GetTextExtentExPointA, 28, 2, 'GetTextExtentExPointA'
        W32F g_GetCharWidth32A, 16, 2, 'GetCharWidth32A'
        W32F g_GetCharWidth32A, 16, 2, 'GetCharWidthA'
        W32F g_GetCharABCWidthsA, 16, 2, 'GetCharABCWidthsA'
        W32F g_GetTextFaceA, 12, 2, 'GetTextFaceA'
        W32F g_SetTextCharacterExtra, 8, 2, 'SetTextCharacterExtra'
        W32F g_GetTextCharacterExtra, 4, 2, 'GetTextCharacterExtra'
        W32F g_GetTextAlign, 4, 2, 'GetTextAlign'
        W32F g_CreateFontA, 56, 2, 'CreateFontA'
        W32F g_CreateFontIndirectA, 4, 2, 'CreateFontIndirectA'
        W32F g_CreateCompatibleDC, 4, 2, 'CreateCompatibleDC'
        W32F g_DeleteDC, 4, 2, 'DeleteDC'
        W32F g_CreateCompatibleBitmap, 12, 2, 'CreateCompatibleBitmap'
        W32F g_CreateCompatibleBitmap, 12, 2, 'CreateDiscardableBitmap'
        W32F g_CreateBitmap, 20, 2, 'CreateBitmap'
        W32F g_CreateBitmapIndirect, 4, 2, 'CreateBitmapIndirect'
        W32F g_GetBitmapBits, 12, 2, 'GetBitmapBits'
        W32F g_SetBitmapBits, 12, 2, 'SetBitmapBits'
        W32F g_CreateDIBSection, 24, 2, 'CreateDIBSection'
        W32F g_SetDIBitsToDevice, 48, 2, 'SetDIBitsToDevice'
        W32F g_StretchDIBits, 52, 2, 'StretchDIBits'
        W32F g_SetDIBits, 28, 2, 'SetDIBits'
        W32F g_GetDIBits, 28, 2, 'GetDIBits'
        W32F g_CreateDIBitmap, 24, 2, 'CreateDIBitmap'
        W32F g_BitBlt, 36, 2, 'BitBlt'
        W32F g_StretchBlt, 44, 2, 'StretchBlt'
        W32F g_PatBlt, 24, 2, 'PatBlt'
        W32F g_GetPixel, 12, 2, 'GetPixel'
        W32F g_CreateRectRgn, 16, 2, 'CreateRectRgn'
        W32F g_CreateRectRgnIndirect, 4, 2, 'CreateRectRgnIndirect'
        W32F g_CreateEllipticRgn, 16, 2, 'CreateEllipticRgn'
        W32F g_CreateEllipticRgnIndirect, 4, 2, 'CreateEllipticRgnIndirect'
        W32F g_CreateRoundRectRgn, 24, 2, 'CreateRoundRectRgn'
        W32F g_CreatePolygonRgn, 12, 2, 'CreatePolygonRgn'
        W32F g_CombineRgn, 16, 2, 'CombineRgn'
        W32F g_SetRectRgn, 20, 2, 'SetRectRgn'
        W32F g_OffsetRgn, 12, 2, 'OffsetRgn'
        W32F g_GetRgnBox, 8, 2, 'GetRgnBox'
        W32F g_PtInRegion, 12, 2, 'PtInRegion'
        W32F g_RectInRegion, 8, 2, 'RectInRegion'
        W32F g_EqualRgn, 8, 2, 'EqualRgn'
        W32F g_FillRgn, 12, 2, 'FillRgn'
        W32F g_PaintRgn, 8, 2, 'PaintRgn'
        W32F g_InvertRgn, 8, 2, 'InvertRgn'
        W32F g_FrameRgn, 20, 2, 'FrameRgn'
        W32F g_SelectClipRgn, 8, 2, 'SelectClipRgn'
        W32F g_ExtSelectClipRgn, 12, 2, 'ExtSelectClipRgn'
        W32F g_IntersectClipRect, 20, 2, 'IntersectClipRect'
        W32F g_ExcludeClipRect, 20, 2, 'ExcludeClipRect'
        W32F g_OffsetClipRgn, 12, 2, 'OffsetClipRgn'
        W32F g_GetClipBox, 8, 2, 'GetClipBox'
        W32F g_GetClipRgn, 8, 2, 'GetClipRgn'
        W32F g_PtVisible, 12, 2, 'PtVisible'
        W32F g_RectVisible, 8, 2, 'RectVisible'
        W32F g_CreatePenIndirect, 4, 2, 'CreatePenIndirect'
        W32F g_ExtCreatePen, 20, 2, 'ExtCreatePen'
        W32F g_CreateHatchBrush, 8, 2, 'CreateHatchBrush'
        W32F g_CreatePatternBrush, 4, 2, 'CreatePatternBrush'
        W32F g_CreateBrushIndirect, 4, 2, 'CreateBrushIndirect'
        W32F g_CreateDIBPatternBrushPt, 8, 2, 'CreateDIBPatternBrushPt'
        W32F g_CreateDIBPatternBrushPt, 8, 2, 'CreateDIBPatternBrush'
        W32F g_UnrealizeObject, 4, 2, 'UnrealizeObject'
        W32F g_GetObjectType, 4, 2, 'GetObjectType'
        W32F g_GetCurrentObject, 8, 2, 'GetCurrentObject'
        W32F g_GetObjectA, 12, 2, 'GetObjectA'
        W32F g_Ellipse, 20, 2, 'Ellipse'
        W32F g_RoundRect, 28, 2, 'RoundRect'
        W32F g_Arc, 36, 2, 'Arc'
        W32F g_Pie, 36, 2, 'Pie'
        W32F g_Chord, 36, 2, 'Chord'
        W32F g_GetCurrentPositionEx, 8, 2, 'GetCurrentPositionEx'
        W32F g_Polyline, 12, 2, 'Polyline'
        W32F g_PolylineTo, 12, 2, 'PolylineTo'
        W32F g_PolyPolyline, 16, 2, 'PolyPolyline'
        W32F g_Polygon, 12, 2, 'Polygon'
        W32F g_PolyPolygon, 16, 2, 'PolyPolygon'
        W32F g_PolyBezier, 12, 2, 'PolyBezier'
        W32F g_PolyBezierTo, 12, 2, 'PolyBezierTo'
        W32F g_SetPixelV, 16, 2, 'SetPixelV'
        W32F g_SetROP2, 8, 2, 'SetROP2'
        W32F g_GetROP2, 4, 2, 'GetROP2'
        W32F g_GetBkMode, 4, 2, 'GetBkMode'
        W32F g_SetPolyFillMode, 8, 2, 'SetPolyFillMode'
        W32F g_GetPolyFillMode, 4, 2, 'GetPolyFillMode'
        W32F g_SetStretchBltMode, 8, 2, 'SetStretchBltMode'
        W32F g_GetStretchBltMode, 4, 2, 'GetStretchBltMode'
        W32F g_SetArcDirection, 8, 2, 'SetArcDirection'
        W32F g_GetArcDirection, 4, 2, 'GetArcDirection'
        W32F g_SetMapMode, 8, 2, 'SetMapMode'
        W32F g_GetMapMode, 4, 2, 'GetMapMode'
        W32F g_SetViewportOrgEx, 16, 2, 'SetViewportOrgEx'
        W32F g_SetWindowOrgEx, 16, 2, 'SetWindowOrgEx'
        W32F g_OffsetViewportOrgEx, 16, 2, 'OffsetViewportOrgEx'
        W32F g_OffsetWindowOrgEx, 16, 2, 'OffsetWindowOrgEx'
        W32F g_GetViewportOrgEx, 8, 2, 'GetViewportOrgEx'
        W32F g_GetWindowOrgEx, 8, 2, 'GetWindowOrgEx'
        W32F g_SetViewportExtEx, 16, 2, 'SetViewportExtEx'
        W32F g_SetViewportExtEx, 16, 2, 'SetWindowExtEx'
        W32F g_GetViewportExtEx, 8, 2, 'GetViewportExtEx'
        W32F g_GetViewportExtEx, 8, 2, 'GetWindowExtEx'
        W32F g_DPtoLP, 12, 2, 'DPtoLP'
        W32F g_LPtoDP, 12, 2, 'LPtoDP'
        W32F g_SetBrushOrgEx, 16, 2, 'SetBrushOrgEx'
        W32F g_GetBrushOrgEx, 8, 2, 'GetBrushOrgEx'
        W32F g_SaveDC, 4, 2, 'SaveDC'
        W32F g_RestoreDC, 8, 2, 'RestoreDC'
        W32F g_GetDeviceCaps, 8, 2, 'GetDeviceCaps'
        W32F g_GdiFlush, 0, 2, 'GdiFlush'
        W32F g_GdiFlush, 4, 2, 'GdiSetBatchLimit'
        W32F g_CreatePalette, 4, 2, 'CreatePalette'
        W32F g_SelectPalette, 12, 2, 'SelectPalette'
        W32F g_RealizePalette, 4, 2, 'RealizePalette'
        W32F g_GetPaletteEntries, 16, 2, 'GetPaletteEntries'
        W32F g_SetPaletteEntries, 16, 2, 'SetPaletteEntries'
        W32F g_GetNearestPaletteIndex, 8, 2, 'GetNearestPaletteIndex'
        W32F g_GetSystemPaletteEntries, 16, 2, 'GetSystemPaletteEntries'
        W32F g_GetNearestColor, 8, 2, 'GetNearestColor'
        W32F g_GetSystemPaletteUse, 4, 2, 'GetSystemPaletteUse'
        W32F g_GdiFlush, 8, 2, 'SetSystemPaletteUse'
        W32F g_GdiFlush, 16, 2, 'AnimatePalette'
        W32F g_GdiFlush, 8, 2, 'ResizePalette'
        W32F g_GdiFlush, 12, 2, 'SetTextJustification'
        ; fase 3d: USER32 que pinta
        W32F u_DrawTextW, 20, 1, 'DrawTextW'
        W32F u_DrawTextExA, 24, 1, 'DrawTextExA'
        W32F u_FrameRect, 12, 1, 'FrameRect'
        W32F u_InvertRect, 8, 1, 'InvertRect'
        W32F u_DrawFocusRect, 8, 1, 'DrawFocusRect'
        W32F u_DrawEdge, 16, 1, 'DrawEdge'
        W32F u_InvalidateRgn, 12, 1, 'InvalidateRgn'
        W32F u_ValidateRgn, 8, 1, 'ValidateRgn'
        W32F u_GetUpdateRgn, 12, 1, 'GetUpdateRgn'
        W32F u_GetUpdateRect, 12, 1, 'GetUpdateRect'
        W32F u_LoadBitmapA, 8, 1, 'LoadBitmapA'
        ; fase 3d: USER32 (win32x.inc)
        W32F u_SetScrollInfo, 16, 1, 'SetScrollInfo'
        W32F u_GetScrollInfo, 12, 1, 'GetScrollInfo'
        W32F u_SetScrollPos, 16, 1, 'SetScrollPos'
        W32F u_GetScrollPos, 8, 1, 'GetScrollPos'
        W32F u_SetScrollRange, 20, 1, 'SetScrollRange'
        W32F u_GetScrollRange, 16, 1, 'GetScrollRange'
        W32F u_nada1, 12, 1, 'ShowScrollBar'
        W32F u_nada1, 12, 1, 'EnableScrollBar'
        W32F u_CreateCaret, 16, 1, 'CreateCaret'
        W32F u_DestroyCaret, 0, 1, 'DestroyCaret'
        W32F u_ShowCaret, 4, 1, 'ShowCaret'
        W32F u_HideCaret, 4, 1, 'HideCaret'
        W32F u_SetCaretPos, 8, 1, 'SetCaretPos'
        W32F u_GetCaretPos, 4, 1, 'GetCaretPos'
        W32F u_GetCaretBlinkTime, 0, 1, 'GetCaretBlinkTime'
        W32F u_nada1, 4, 1, 'SetCaretBlinkTime'
        W32F u_GetKeyState, 4, 1, 'GetKeyState'
        W32F u_GetAsyncKeyState, 4, 1, 'GetAsyncKeyState'
        W32F u_GetKeyboardState, 4, 1, 'GetKeyboardState'
        W32F u_ToAscii, 20, 1, 'ToAscii'
        W32F u_ToAscii, 24, 1, 'ToAsciiEx'
        W32F u_MapVirtualKeyA, 8, 1, 'MapVirtualKeyA'
        W32F u_VkKeyScanA, 4, 1, 'VkKeyScanA'
        W32F u_LoadAcceleratorsA, 8, 1, 'LoadAcceleratorsA'
        W32F u_CreateAcceleratorTableA, 8, 1, 'CreateAcceleratorTableA'
        W32F u_DestroyAcceleratorTable, 4, 1, 'DestroyAcceleratorTable'
        W32F u_CopyAcceleratorTableA, 12, 1, 'CopyAcceleratorTableA'
        W32F u_TranslateAcceleratorA, 12, 1, 'TranslateAcceleratorA'
        W32F u_TranslateAcceleratorA, 12, 1, 'TranslateAccelerator'
        W32F u_LoadImageA, 24, 1, 'LoadImageA'
        W32F u_DestroyIcon, 4, 1, 'DestroyIcon'
        W32F u_DestroyIcon, 4, 1, 'DestroyCursor'
        W32F u_CopyIcon, 4, 1, 'CopyIcon'
        W32F u_DrawIcon, 16, 1, 'DrawIcon'
        W32F u_DrawIconEx, 36, 1, 'DrawIconEx'
        W32F u_GetCursor, 0, 1, 'GetCursor'
        W32F u_GetClassLongA, 8, 1, 'GetClassLongA'
        W32F u_SetClassLongA, 12, 1, 'SetClassLongA'
        W32F u_OpenClipboard, 4, 1, 'OpenClipboard'
        W32F u_OpenClipboard, 0, 1, 'CloseClipboard'
        W32F u_EmptyClipboard, 0, 1, 'EmptyClipboard'
        W32F u_SetClipboardData, 8, 1, 'SetClipboardData'
        W32F u_GetClipboardData, 4, 1, 'GetClipboardData'
        W32F u_IsClipboardFormatAvailable, 4, 1, 'IsClipboardFormatAvailable'
        W32F u_CountClipboardFormats, 0, 1, 'CountClipboardFormats'
        W32F u_EnumClipboardFormats, 4, 1, 'EnumClipboardFormats'
        W32F u_cero, 0, 1, 'GetClipboardOwner'
        W32F u_TrackPopupMenu, 28, 1, 'TrackPopupMenu'
        W32F u_SetRect, 20, 1, 'SetRect'
        W32F u_SetRectEmpty, 4, 1, 'SetRectEmpty'
        W32F u_CopyRect, 8, 1, 'CopyRect'
        W32F u_InflateRect, 12, 1, 'InflateRect'
        W32F u_OffsetRect, 12, 1, 'OffsetRect'
        W32F u_IsRectEmpty, 4, 1, 'IsRectEmpty'
        W32F u_EqualRect, 8, 1, 'EqualRect'
        W32F u_PtInRect, 12, 1, 'PtInRect'
        W32F u_IntersectRect, 12, 1, 'IntersectRect'
        W32F u_UnionRect, 12, 1, 'UnionRect'
        W32F u_SubtractRect, 12, 1, 'SubtractRect'
        W32F u_ClientToScreen, 8, 1, 'ClientToScreen'
        W32F u_ScreenToClient, 8, 1, 'ScreenToClient'
        W32F u_MapWindowPoints, 16, 1, 'MapWindowPoints'
        W32F u_GetCursorPos, 4, 1, 'GetCursorPos'
        W32F u_nada1, 8, 1, 'SetCursorPos'
        W32F u_WindowFromPoint, 8, 1, 'WindowFromPoint'
        W32F u_ChildWindowFromPoint, 12, 1, 'ChildWindowFromPoint'
        W32F u_AdjustWindowRect, 12, 1, 'AdjustWindowRect'
        W32F u_AdjustWindowRectEx, 16, 1, 'AdjustWindowRectEx'
        W32F u_SetCapture, 4, 1, 'SetCapture'
        W32F u_ReleaseCapture, 0, 1, 'ReleaseCapture'
        W32F u_GetCapture, 0, 1, 'GetCapture'
        W32F u_GetDesktopWindow, 0, 1, 'GetDesktopWindow'
        W32F u_GetActiveWindow, 0, 1, 'GetActiveWindow'
        W32F u_GetActiveWindow, 0, 1, 'GetForegroundWindow'
        W32F u_SetActiveWindow, 4, 1, 'SetActiveWindow'
        W32F u_SetActiveWindow, 4, 1, 'SetForegroundWindow'
        W32F u_SetActiveWindow, 4, 1, 'BringWindowToTop'
        W32F u_GetWindowPlacement, 8, 1, 'GetWindowPlacement'
        W32F u_SetWindowPlacement, 8, 1, 'SetWindowPlacement'
        W32F u_FindWindowA, 8, 1, 'FindWindowA'
        W32F u_WinHelpA, 16, 1, 'WinHelpA'
        W32F u_GetMessagePos, 0, 1, 'GetMessagePos'
        W32F u_GetMessageTime, 0, 1, 'GetMessageTime'
        W32F u_CheckMenuRadioItem, 20, 1, 'CheckMenuRadioItem'
        W32F u_MapDialogRect, 8, 1, 'MapDialogRect'
        W32F u_MessageBoxIndirectA, 4, 1, 'MessageBoxIndirectA'
        W32F u_SystemParametersInfoA, 16, 1, 'SystemParametersInfoA'
        W32F u_GetClassNameA, 12, 1, 'GetClassNameA'
        W32F u_ScrollWindow, 20, 1, 'ScrollWindow'
        W32F u_ScrollWindowEx, 32, 1, 'ScrollWindowEx'
        W32F u_ScrollDC, 28, 1, 'ScrollDC'
        W32F u_RedrawWindow, 16, 1, 'RedrawWindow'
        W32F u_GetWindow, 8, 1, 'GetWindow'
        W32F u_IsChild, 8, 1, 'IsChild'
        W32F u_EnumChildWindows, 12, 1, 'EnumChildWindows'
        W32F u_DefDlgProcA, 16, 1, 'DefDlgProcA'
        W32F u_nada1, 4, 1, 'LockWindowUpdate'
        W32F u_cero, 4, 1, 'GetQueueStatus'
        W32F u_cero, 0, 1, 'GetInputState'
        W32F u_CharUpperA, 4, 1, 'CharUpperA'
        W32F u_CharLowerA, 4, 1, 'CharLowerA'
        W32F u_CharUpperBuffA, 8, 1, 'CharUpperBuffA'
        W32F u_CharLowerBuffA, 8, 1, 'CharLowerBuffA'
        W32F u_CharNextA, 4, 1, 'CharNextA'
        W32F u_CharPrevA, 8, 1, 'CharPrevA'
        W32F u_IsCharAlphaA, 4, 1, 'IsCharAlphaA'
        W32F u_IsCharAlphaNumericA, 4, 1, 'IsCharAlphaNumericA'
        W32F u_IsCharUpperA, 4, 1, 'IsCharUpperA'
        W32F u_IsCharLowerA, 4, 1, 'IsCharLowerA'
        W32F u_wvsprintfA, 12, 1, 'wvsprintfA'
        W32F u_cero, 16, 1, 'GetMenuItemRect'
        W32F u_cero, 8, 2, 'StartDocA'
        W32F u_cero, 4, 2, 'StartPage'
        W32F u_cero, 4, 2, 'EndPage'
        W32F u_cero, 4, 2, 'EndDoc'
        W32F u_cero, 4, 2, 'AbortDoc'
        ; fase 3d: COMDLG32, ADVAPI32 y SHELL32 (win32x.inc)
        W32F c_GetOpenFileNameA, 4, 3, 'GetOpenFileNameA'
        W32F c_GetSaveFileNameA, 4, 3, 'GetSaveFileNameA'
        W32F c_GetFileTitleA, 12, 3, 'GetFileTitleA'
        W32F c_ChooseColorA, 4, 3, 'ChooseColorA'
        W32F c_ChooseFontA, 4, 3, 'ChooseFontA'
        W32F c_CommDlgExtendedError, 0, 3, 'CommDlgExtendedError'
        W32F c_CommDlgExtendedError, 4, 3, 'PrintDlgA'
        W32F c_CommDlgExtendedError, 4, 3, 'PageSetupDlgA'
        W32F a_no_esta, 20, 4, 'RegOpenKeyExA'
        W32F a_no_esta, 12, 4, 'RegOpenKeyA'
        W32F a_no_puede, 36, 4, 'RegCreateKeyExA'
        W32F a_no_puede, 12, 4, 'RegCreateKeyA'
        W32F a_no_esta, 24, 4, 'RegQueryValueExA'
        W32F a_no_esta, 16, 4, 'RegQueryValueA'
        W32F a_no_puede, 24, 4, 'RegSetValueExA'
        W32F a_no_puede, 20, 4, 'RegSetValueA'
        W32F a_no_hay_mas, 32, 4, 'RegEnumKeyExA'
        W32F a_no_hay_mas, 16, 4, 'RegEnumKeyA'
        W32F a_no_hay_mas, 32, 4, 'RegEnumValueA'
        W32F a_no_esta, 8, 4, 'RegDeleteKeyA'
        W32F a_no_esta, 8, 4, 'RegDeleteValueA'
        W32F u_cero, 4, 4, 'RegCloseKey'
        W32F u_cero, 4, 4, 'RegFlushKey'
        W32F a_GetUserNameA, 8, 4, 'GetUserNameA'
        W32F u_cero, 8, 5, 'DragAcceptFiles'
        W32F u_cero, 16, 5, 'DragQueryFileA'
        W32F u_cero, 16, 5, 'DragQueryFile'
        W32F u_cero, 4, 5, 'DragFinish'
        W32F s_ShellExecuteA, 24, 5, 'ShellExecuteA'
        W32F s_ShellAboutA, 16, 5, 'ShellAboutA'
        W32F u_cero, 16, 5, 'SHGetSpecialFolderPathA'
        W32F u_cero, 12, 5, 'ExtractIconA'
        W32F u_cero, 8, 5, 'Shell_NotifyIconA'
        W32F u_cero, 4, 5, 'SHBrowseForFolderA'
        W32F u_cero, 8, 5, 'SHGetPathFromIDListA'
        dd 0
%unmacro W32F 4

; ---- una función que no hay: se dice cuál y el programa termina ----
pe_f_falta:
        sub esp, 192
        mov edi, esp
        mov esi, t_pe_falta
        call pon_cad
        mov esi, [ebx+PT_NOMBRE]
        test esi, esi
        jnz .n
        mov esi, t_pe_num
        call pon_cad
        jmp .d
.n:
        add esi, [edx+PE_DELTA]
        mov ecx, 80
        call w32_pon
.d:
        mov esi, [ebx+PT_DLL]
        test esi, esi
        jz .f
        mov ax, ' ('
        stosw
        add esi, [edx+PE_DELTA]
        mov ecx, 30
        call w32_pon
        mov al, ')'
        stosb
.f:
        mov byte [edi], 0
        mov esi, esp
        mov edi, t_pe_titulo
        xor ebp, ebp
        mov eax, DM_ACEPTAR
        call dlg_mensaje
        add esp, 192
        jmp pe_salir
; (interna) añade a EDI el texto ESI, ECX letras como mucho; EDI avanza
w32_pon:
        lodsb
        test al, al
        jz .x
        stosb
        dec ecx
        jnz w32_pon
.x:
        ret

; ---- KERNEL32: está en HW32 (utils/hw32k.inc), que es el módulo del
; programa. Aquí, solo lo que es cosa del entorno ----
; (adonde vuelve la entrada del programa si hace «ret»)
k_ExitProcess:
        jmp pe_salir
; void Sleep(ms): deja el turno a los demás hasta que pase ese tiempo
k_Sleep:
        call tics
        mov ebx, eax
.b:
        call ceder
        call dx_tic                 ; [DX] WINMM: el sonido y sus temporizadores
        call tics
        sub eax, ebx
        imul eax, eax, 55
        cmp eax, [ebp]
        jb .b
        ret
; ---- USER32 ----
MB_TEXTO    equ 1024                ; letras del texto de un MessageBox
MB_TITULO   equ 128                 ; y de su título
; lo que pide MessageBox (tipo, de 0 a 5) -> el cuadro de Hispa Ventanas
mb_tipos    db DM_ACEPTAR, DM_ACEPTAR_CANCELAR, DM_ACEPTAR, DM_SI_NO_CANCELAR
            db DM_SI_NO, DM_ACEPTAR_CANCELAR
; int MessageBoxA(ventana, texto, título, tipo): un cuadro de Hispa Ventanas
; (dlg_mensaje) cuya dueña es esa ventana, si es del programa. MB_OK, MB_OKCANCEL, MB_YESNOCANCEL y MB_YESNO, con las
; respuestas de Windows (1 aceptar, 2 cancelar, 6 sí, 7 no);
; MB_RETRYCANCEL sale como aceptar y cancelar (4 reintentar, 2 cancelar) y lo
; demás, con «Aceptar». Los textos vienen en la página 1252 de Windows.
u_MessageBoxA:
        mov ecx, MB_TEXTO + MB_TITULO
        call mem_pedir
        jc .no
        mov ebx, eax
        mov edi, eax
        mov esi, [ebp+4]
        mov ecx, MB_TEXTO - 1
        call w32_texto
        lea edi, [ebx+MB_TEXTO]
        mov esi, [ebp+8]
        mov ecx, MB_TITULO - 1
        test esi, esi
        jnz .t
        mov esi, t_pe_error         ; sin título: «Error», como Windows
        call pon_cad
        mov byte [edi], 0
        jmp .t2
.t:
        call w32_texto
.t2:
        mov ecx, [ebp+12]
        and ecx, 15
        cmp ecx, 5
        jbe .otra
        xor ecx, ecx
.otra:
        mov esi, ebx
        lea edi, [ebx+MB_TEXTO]
        push ebp
        mov eax, [ebp]              ; la dueña (si sigue ahí)
        call w32_ven
        mov ebp, eax
        movzx eax, byte [mb_tipos+ecx]
        call dlg_mensaje
        pop ebp
        cmp ecx, 4                  ; sí o no: no vale cerrarlo sin contestar
        jne .r
        cmp eax, ID_CANCELAR
        je .otra
.r:
        cmp ecx, 5                  ; reintentar o cancelar
        jne .f
        cmp eax, ID_ACEPTAR
        jne .f
        mov eax, 4
.f:
        xchg eax, ebx
        call mem_soltar
        mov eax, ebx
        ret
.no:
        xor eax, eax
        ret

; (interna) copia a EDI el texto ESI del programa EDX (0: vacío), ECX letras
; como mucho, de la página 1252 a la 437 y sin los retornos de carro
w32_texto:
        test esi, esi
        jz .f
        add esi, [edx+PE_DELTA]
.c:
        lodsb
        test al, al
        jz .f
        cmp al, 13
        je .c
        test al, al
        jns .p
        movzx eax, al
        mov al, [w32_1252+eax-0x80]
.p:
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        ret
; de la página 1252 (80h a FFh) a la 437 (tools/mk1252.py); [GLIFOS] la 437 de Ventanas,
; con Á, Ó, €... en códigos que no se pintaban (tools/hispa437.py)
w32_1252:
        db 0xF4, 0x3F, 0x2C, 0x9F, 0x22, 0xF5, 0x2B, 0x2B, 0x5E, 0x25, 0x53, 0x3C, 0xE2, 0x3F, 0x5A, 0x3F
        db 0x3F, 0x27, 0x27, 0x22, 0x22, 0xFA, 0x2D, 0x2D, 0x7E, 0x3F, 0x73, 0x3E, 0xE5, 0x3F, 0x7A, 0x59
        db 0xFF, 0xAD, 0x9B, 0x9C, 0x3F, 0x9D, 0x7C, 0x3F, 0x22, 0xE7, 0xA6, 0xAE, 0xAA, 0x3F, 0xE8, 0x2D
        db 0xF8, 0xF1, 0xFD, 0x33, 0x27, 0xE6, 0x3F, 0xFA, 0x2C, 0x31, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8
        db 0xB5, 0xB6, 0xB7, 0xB8, 0x8E, 0x8F, 0x92, 0x80, 0xBD, 0x90, 0xBE, 0xC6, 0xC7, 0xCF, 0xD0, 0xD1
        db 0x44, 0xA5, 0xD2, 0xD3, 0xD4, 0xD5, 0x99, 0x78, 0x4F, 0xD6, 0xD7, 0xD8, 0x9A, 0x59, 0x3F, 0xE1
        db 0x85, 0xA0, 0x83, 0xDD, 0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B
        db 0x64, 0xA4, 0x95, 0xA2, 0x93, 0xDE, 0x94, 0xF6, 0x6F, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x3F, 0x98
