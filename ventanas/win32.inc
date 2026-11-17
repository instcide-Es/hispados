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
PE_MANGO    equ 4                   ; el bloque (mango de DPMI)
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
PE_TAM      equ PE_PEND + PE_PEND_MAX * 16

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
t_pe_mem    db "No hay memoria para el programa de Windows.", 0
t_pe_muchas db "El programa de Windows usa demasiadas funciones.", 0
t_pe_falta  db "Hispa-486: falta la función ", 0
t_pe_num    db "(por número)", 0
t_pe_titulo db "Hispa-486", 0
t_pe_error  db "Error", 0
t_k32       db "KERNEL32", 0
t_u32       db "USER32", 0
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
        lea esi, [edx+PE_RUTA]
        xor al, al
        call dos_abrir
        jc .mal
        mov ebx, eax                ; EBX = el archivo, EDX = la ficha
        call pe_leer_cab
        jc .malc
        test word [pe_cab+PC_CARACT], 0x2000    ; una DLL no se ejecuta
        jnz .malc
        ; el bloque: imagen + trampolines + lo suyo + pila
        mov eax, [pe_cab+PC_IMGTAM]
        cmp eax, PE_IMG_MAX
        ja .malc
        add eax, 0xFFF
        and eax, ~0xFFF
        jz .malc
        mov [edx+PE_IMGTAM], eax
        mov ecx, [pe_cab+PC_PILA]
        cmp ecx, PE_PILA_MAX
        jbe .p1
        mov ecx, PE_PILA_MAX
.p1:
        cmp ecx, PE_PILA_MIN
        jae .p2
        mov ecx, PE_PILA_MIN
.p2:
        add ecx, 0xFFF
        and ecx, ~0xFFF
        lea ebp, [eax+ecx+PE_TRAMP_TAM+PE_MISC_TAM]  ; EBP = todo el bloque
        push ebx
        push edx
        mov ecx, ebp
        mov ebx, ebp
        shr ebx, 16
        mov ax, 0x0501
        int 0x31
        pop edx                     ; (no cambia las banderas)
        jc .malm
        mov [edx+PE_MANGO], di
        mov [edx+PE_MANGO+2], si
        mov dword [edx+PE_USO], 2   ; (si algo falla, pe_limpiar lo suelta)
        shl ebx, 16
        mov bx, cx                  ; EBX = dirección lineal del bloque
        mov edi, ebx
        sub edi, [base]
        mov [edx+PE_IMG], edi
        mov eax, [pe_cab+PC_BASE]
        mov [edx+PE_BASE], eax
        mov esi, edi
        sub esi, eax
        mov [edx+PE_DELTA], esi
        add eax, [pe_cab+PC_ENTRADA]
        mov [edx+PE_ENTRADA], eax
        mov ecx, ebp                ; todo, a cero
        shr ecx, 2
        xor eax, eax
        rep stosd
        ; lo suyo y la cima de la pila, en direcciones suyas
        mov eax, [edx+PE_BASE]
        add eax, [edx+PE_IMGTAM]
        add eax, PE_TRAMP_TAM
        mov [pe_misc], eax
        add eax, 0x100
        mov [edx+PE_CMD], eax
        mov eax, [edx+PE_BASE]
        add eax, ebp
        mov [edx+PE_CIMA], eax
        ; los selectores: código, datos y pila, y FS (la TIB)
        mov esi, ebx                ; ESI = dirección lineal del bloque
        xor eax, eax
        mov ecx, 3
        int 0x31
        jc .malm
        movzx eax, ax
        mov [edx+PE_SEL], eax
        mov edi, esi
        sub edi, [edx+PE_BASE]      ; EDI = la base: bloque − ImageBase
        mov ebx, eax
        mov ecx, 0xC0FA             ; G, D, presente, código legible
        call pe_selector
        add ebx, 8
        mov ecx, 0xC0F2             ; G, B, presente, datos escribibles
        call pe_selector
        add ebx, 8
        mov edi, [pe_misc]
        sub edi, [edx+PE_BASE]
        add edi, esi                ; la TIB, en lineal
        call pe_selector
        pop ebx
        ; las cabeceras, a la imagen
        mov ecx, [pe_cab+PC_CABTAM]
        cmp ecx, [edx+PE_IMGTAM]
        ja .malc
        mov eax, [pe_lfanew]        ; (y la tabla de secciones tiene que caber)
        movzx esi, word [pe_cab+PC_OPC_TAM]
        lea eax, [eax+esi+PC_OPC]
        mov ebp, eax                ; EBP = la tabla de secciones (en la imagen)
        movzx esi, word [pe_cab+PC_SECC]
        imul esi, 40
        add eax, esi
        cmp eax, ecx
        ja .malc
        push edx
        xor edx, edx
        xor al, al
        call dos_puntero
        pop edx
        jc .malc
        mov edi, [edx+PE_IMG]
        call dos_leer
        jc .malc
        cmp eax, ecx
        jne .malc
        ; las secciones
        add ebp, [edx+PE_IMG]
        movzx esi, word [pe_cab+PC_SECC]
.s:
        test esi, esi
        jz .sf
        mov ecx, [ebp+16]           ; lo que hay en el archivo
        mov eax, [ebp+8]            ; lo que ocupa en memoria
        test eax, eax
        jz .s1
        cmp ecx, eax
        jbe .s1
        mov ecx, eax
.s1:
        mov edi, [ebp+12]           ; dónde va (RVA)
        mov eax, [edx+PE_IMGTAM]
        cmp edi, eax
        ja .malc
        sub eax, edi
        cmp ecx, eax
        ja .malc
        test ecx, ecx
        jz .ss
        cmp dword [ebp+20], 0
        je .ss
        add edi, [edx+PE_IMG]
        push edx
        mov edx, [ebp+20]
        xor al, al
        call dos_puntero
        pop edx
        jc .malc
        call dos_leer               ; (si el archivo es más corto, lo demás a cero)
        jc .malc
.ss:
        add ebp, 40
        dec esi
        jmp .s
.sf:
        call dos_cerrar
        ; las importaciones
        call pe_importar            ; (si falla, ESI = el aviso)
        jc .aviso
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
        ; la TIB: sin manejadores de excepción, la pila y ella misma
        mov edi, [pe_misc]
        mov eax, edi
        add edi, [edx+PE_DELTA]
        mov dword [edi], -1
        mov ecx, [edx+PE_CIMA]
        mov [edi+4], ecx
        lea ecx, [eax+PE_MISC_TAM]
        mov [edi+8], ecx
        mov [edi+0x18], eax
        ; la línea de órdenes: el programa y sus parámetros
        add edi, 0x100
        lea esi, [edx+PE_RUTA]
        call pon_cad
        cmp byte [edx+PE_PARAM], 0
        je .l1
        mov al, ' '
        stosb
        lea esi, [edx+PE_PARAM]
        call pon_cad
.l1:
        mov byte [edi], 0
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
        jmp .aviso
.malm:
        pop ebx
        mov esi, t_pe_mem
        call dos_cerrar
        jmp .aviso
.malc:
        call dos_cerrar
.mal:
        mov esi, t_pe_mal
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
        add ebx, [edx+PE_IMG]
        add ebx, [edx+PE_IMGTAM]
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

; (interna) las importaciones del programa EDX: cada una, su trampolín.
; CF=1 si la tabla está mal (ESI = el aviso) o no caben los trampolines
pe_importar:
        pushad
        cmp dword [pe_cab+PC_NDIR], 2
        jb .bien
        mov eax, [pe_cab+PC_IMPORT]
        test eax, eax
        jz .bien
        call pe_rva
        jc .mal
        mov esi, eax                ; ESI = el descriptor de una DLL
.d:
        mov eax, [edx+PE_IMGTAM]    ; (entero dentro de la imagen)
        add eax, [edx+PE_IMG]
        sub eax, esi
        cmp eax, 20
        jb .mal
        mov eax, [esi+12]           ; su nombre
        test eax, eax
        jz .bien
        mov ebx, eax
        add ebx, [edx+PE_BASE]
        mov [pe_dll], ebx           ; (dirección del programa)
        call pe_rva
        jc .mal
        push esi
        mov esi, eax
        mov bl, 0
        mov edi, t_k32
        call pe_es_dll
        je .dll
        mov bl, 1
        mov edi, t_u32
        call pe_es_dll
        je .dll
        mov bl, 2
        mov edi, t_g32
        call pe_es_dll
        je .dll
        mov bl, 0xFF                ; una DLL que no hay: todo lo suyo «falta»
.dll:
        pop esi
        mov eax, [esi]              ; la tabla de nombres (o la de direcciones)
        test eax, eax
        jnz .t
        mov eax, [esi+16]
.t:
        call pe_rva
        jc .mal
        mov ebp, eax
        mov eax, [esi+16]           ; la tabla de direcciones
        call pe_rva
        jc .mal
        mov edi, eax
.f:
        mov eax, [ebp]
        test eax, eax
        jz .ds
        xor ecx, ecx                ; por número: no se resuelve
        test eax, 0x80000000
        jnz .falta
        add eax, 2                  ; (la pista) -> el nombre
        mov ecx, eax
        add ecx, [edx+PE_BASE]
        call pe_rva
        jc .mal
        call w32_busca              ; EAX=nombre BL=DLL -> EAX=la función
        jmp .pon
.falta:
        mov eax, w32_e_falta
.pon:
        push edi
        mov edi, [pe_dll]
        call pe_tramp
        pop edi
        jc .muchas
        mov [edi], eax
        add ebp, 4
        add edi, 4
        mov eax, [edx+PE_IMGTAM]    ; (las dos tablas, dentro de la imagen)
        add eax, [edx+PE_IMG]
        sub eax, 4
        cmp ebp, eax
        ja .mal
        cmp edi, eax
        ja .mal
        jmp .f
.ds:
        add esi, 20
        jmp .d
.bien:
        popad
        clc
        ret
.muchas:
        mov dword [esp+4], t_pe_muchas
        jmp .malx
.mal:
        mov dword [esp+4], t_pe_mal
.malx:
        popad
        stc
        ret

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

; (interna) EAX=nombre de la función BL=DLL (0 KERNEL32, 1 USER32)
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
        mov ebx, [edx+PE_SEL]
        test ebx, ebx
        jz .m
        mov ecx, 3
.sel:
        mov ax, 0x0001
        int 0x31
        add ebx, 8
        dec ecx
        jnz .sel
.m:
        mov di, [edx+PE_MANGO]
        mov si, [edx+PE_MANGO+2]
        mov ax, 0x0502
        int 0x31
        mov dword [edx+PE_USO], 0
.s:
        add edx, PE_TAM
        dec ebp
        jnz .b
        popad
        ret

; =============================================================================
;  Las funciones. Cada ficha: la rutina (4), los octetos de argumentos (2),
;  la DLL (1: 0 KERNEL32, 1 USER32, 2 GDI32) y el nombre.
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
        W32F k_ExitProcess, 4, 0, 'ExitProcess'
        W32F k_GetModuleHandle, 4, 0, 'GetModuleHandleA'
        W32F k_GetModuleHandle, 4, 0, 'GetModuleHandleW'
        W32F k_GetCommandLineA, 0, 0, 'GetCommandLineA'
        W32F k_GetStartupInfoA, 4, 0, 'GetStartupInfoA'
        W32F k_GetVersion, 0, 0, 'GetVersion'
        W32F k_GetLastError, 0, 0, 'GetLastError'
        W32F k_SetLastError, 4, 0, 'SetLastError'
        W32F k_GetTickCount, 0, 0, 'GetTickCount'
        W32F k_Sleep, 4, 0, 'Sleep'
        W32F k_GetModuleFileNameA, 12, 0, 'GetModuleFileNameA'
        W32F k_lstrlenA, 4, 0, 'lstrlenA'
        W32F k_lstrlenA, 4, 0, 'lstrlen'
        W32F k_lstrcpyA, 8, 0, 'lstrcpyA'
        W32F k_lstrcpyA, 8, 0, 'lstrcpy'
        W32F k_lstrcatA, 8, 0, 'lstrcatA'
        W32F k_lstrcatA, 8, 0, 'lstrcat'
        W32F u_MessageBoxA, 16, 1, 'MessageBoxA'
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
        W32F u_cero, 4, 1, 'SetCursor'
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

; ---- KERNEL32 ----
; void ExitProcess(código)
k_ExitProcess:
        jmp pe_salir
; HMODULE GetModuleHandleA/W(nombre): solo el del programa (NULL)
k_GetModuleHandle:
        xor eax, eax
        cmp [ebp], eax
        jne .x
        mov eax, [edx+PE_BASE]
.x:
        ret
k_GetCommandLineA:
        mov eax, [edx+PE_CMD]
        ret
; void GetStartupInfoA(STARTUPINFOA*): 68 octetos, todo a cero menos el tamaño
k_GetStartupInfoA:
        mov edi, [ebp]
        add edi, [edx+PE_DELTA]
        mov ebx, edi
        mov ecx, 17
        xor eax, eax
        rep stosd
        mov dword [ebx], 68
        ret
k_GetVersion:                       ; Windows 98: 4.10 (como HW32)
        mov eax, 0xC0000A04
        ret
k_GetLastError:
        mov eax, [edx+PE_ERROR]
        ret
k_SetLastError:
        mov eax, [ebp]
        mov [edx+PE_ERROR], eax
        ret
; DWORD GetTickCount(): milisegundos (del reloj de la BIOS: 55 ms por tic)
k_GetTickCount:
        call tics
        imul eax, eax, 55
        ret
; void Sleep(ms): deja el turno a los demás hasta que pase ese tiempo
k_Sleep:
        call tics
        mov ebx, eax
.b:
        call ceder
        call tics
        sub eax, ebx
        imul eax, eax, 55
        cmp eax, [ebp]
        jb .b
        ret
; DWORD GetModuleFileNameA(módulo, búfer, tamaño): la ruta del programa
k_GetModuleFileNameA:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        xor eax, eax
        test ecx, ecx
        jz .x
        lea esi, [edx+PE_RUTA]
.c:
        dec ecx
        jz .f
        mov bl, [esi]
        test bl, bl
        jz .f
        mov [edi+eax], bl
        inc esi
        inc eax
        jmp .c
.f:
        mov byte [edi+eax], 0
.x:
        ret
; int lstrlenA(texto)
k_lstrlenA:
        xor eax, eax
        mov esi, [ebp]
        test esi, esi
        jz .x
        add esi, [edx+PE_DELTA]
.c:
        cmp byte [esi+eax], 0
        je .x
        inc eax
        jmp .c
.x:
        ret
; LPSTR lstrcpyA(destino, origen) y lstrcatA(destino, origen)
k_lstrcatA:
        mov edi, [ebp]
        test edi, edi
        jz k_lstrcpyA.no
        add edi, [edx+PE_DELTA]
.f:
        cmp byte [edi], 0
        je k_lstrcpyA.o
        inc edi
        jmp .f
k_lstrcpyA:
        mov edi, [ebp]
        test edi, edi
        jz .no
        add edi, [edx+PE_DELTA]
.o:
        mov esi, [ebp+4]
        test esi, esi
        jz .no
        add esi, [edx+PE_DELTA]
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        mov eax, [ebp]
        ret
.no:
        xor eax, eax
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
; de la página 1252 (80h a FFh) a la 437 (tools/mk1252.py)
w32_1252:
        db 0x45, 0x3F, 0x3F, 0x9F, 0x3F, 0x2E, 0x3F, 0x3F, 0x3F, 0x3F, 0x53, 0x3F, 0x3F, 0x3F, 0x5A, 0x3F
        db 0x3F, 0x27, 0x27, 0x22, 0x22, 0x3F, 0x2D, 0x2D, 0x3F, 0x3F, 0x73, 0x3F, 0x3F, 0x3F, 0x7A, 0x3F
        db 0xFF, 0xAD, 0x9B, 0x9C, 0x3F, 0x9D, 0x3F, 0x3F, 0x3F, 0x63, 0xA6, 0xAE, 0xAA, 0x3F, 0x52, 0x3F
        db 0xF8, 0xF1, 0xFD, 0x3F, 0x3F, 0xE6, 0x3F, 0xFA, 0x3F, 0x3F, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8
        db 0x41, 0x41, 0x41, 0x41, 0x8E, 0x8F, 0x92, 0x80, 0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49
        db 0x3F, 0xA5, 0x4F, 0x4F, 0x4F, 0x4F, 0x99, 0x78, 0x4F, 0x55, 0x55, 0x55, 0x9A, 0x59, 0x3F, 0xE1
        db 0x85, 0xA0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B
        db 0x3F, 0xA4, 0x95, 0xA2, 0x93, 0x6F, 0x94, 0xF6, 0x6F, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x3F, 0x98
