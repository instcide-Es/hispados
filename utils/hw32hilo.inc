; =============================================================================
;  HW32 - [HISPA-486] fase 3d: hilos, exclusiones, eventos y archivos .INI
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hispa-DOS tiene un solo hilo, así que CreateThread ejecuta el hilo entero
;  antes de volver (como si el que lo crea esperara a que acabe): vale para
;  los programas que lanzan un trabajo en otro hilo y esperan a que termine
;  (el ensamblador de FASMW, por ejemplo). WaitForSingleObject, las
;  exclusiones (mutex) y los eventos están siempre libres. ExitThread dentro
;  de un hilo vuelve a CreateThread.
;  GetPrivateProfileStringA, WritePrivateProfileStringA y
;  GetPrivateProfileIntA leen y escriben los .INI como Windows: [sección] y
;  clave=valor, sin mirar mayúsculas. Sala limpia: de la documentación
;  pública del API Win32.
; =============================================================================

H_HILO      equ 0x60000             ; un hilo: esto + su número
N_HILO      equ 8
H_MUTEX     equ 0x61000
H_EVENTO    equ 0x62000
HILO_TRAMP  equ 0x4F00              ; (en lo nuestro) la vuelta del hilo

; HANDLE CreateThread(atributos, pila, función, parámetro, opciones, DWORD* id)
k_CreateThread:
        cmp byte [hilo_dentro], 0   ; (uno dentro de otro: no)
        jne .no
        mov eax, [hilo_n]
        cmp eax, N_HILO
        jb .h
        xor eax, eax
        mov [hilo_n], eax
.h:
        mov [hilo_act], eax
        inc dword [hilo_n]
        mov dword [hilo_cod + eax * 4], 259     ; STILL_ACTIVE
        mov edi, [img_base]         ; el trampolín de vuelta: jmp lejos hilo_vuelve
        add edi, [misc_off]
        add edi, HILO_TRAMP
        mov byte [es:edi], 0xEA
        mov dword [es:edi + 1], hilo_vuelve
        mov [es:edi + 5], cs
        mov ecx, [ebp + 20]
        test ecx, ecx
        jz .i
        lea eax, [eax + 1]
        mov [es:ecx], eax
.i:
        mov byte [hilo_dentro], 1
        mov [hilo_esp], esp
        mov [hilo_ebp], ebp
        push dword [ebp + 12]       ; el parámetro (en la pila del programa)
        push edi                    ; la vuelta: el trampolín
        movzx eax, word [sel_pc]
        push eax
        push dword [ebp + 8]        ; la función
        mov ax, [sel_pd]
        mov ds, ax
        retf
.no:
        mov dword [ult_error], 8
        xor eax, eax
        ret
; (lejos, desde el trampolín, o desde ExitThread) EAX = el código del hilo
hilo_vuelve:
        mov ds, [cs:sel_ds_cs]
        mov es, [cs:sel_pd_cs]
        cld
        mov esp, [hilo_esp]
        mov ebp, [hilo_ebp]
        mov byte [hilo_dentro], 0
        mov ecx, [hilo_act]
        mov [hilo_cod + ecx * 4], eax
        lea eax, [ecx + H_HILO]
        ret
; VOID ExitThread(código): dentro de un hilo, vuelve a CreateThread; si no,
; acaba el programa (es el único hilo)
k_ExitThread3d:
        cmp byte [hilo_dentro], 0
        je k_ExitThread
        mov eax, [ebp]
        jmp hilo_vuelve
; BOOL GetExitCodeThread(hilo, DWORD*)
k_GetExitCodeThread:
        mov eax, [ebp]
        mov edi, [ebp + 4]
        cmp eax, -2                 ; (GetCurrentThread: el de siempre, en marcha)
        je .yo
        sub eax, H_HILO
        cmp eax, N_HILO
        jae .mal
        mov eax, [hilo_cod + eax * 4]
        mov [es:edi], eax
        mov eax, 1
        ret
.yo:
        mov dword [es:edi], 259
        mov eax, 1
        ret
.mal:
        mov dword [ult_error], 6
        xor eax, eax
        ret
; los que no tienen nada que hacer
k_uno:                              ; TerminateThread, SetThreadPriority,
        mov eax, 1                  ; ReleaseMutex, SetEvent, ResetEvent...
        ret
k_cero:                             ; GetThreadPriority, ResumeThread...
        xor eax, eax
        ret
; HANDLE CreateMutexA(atributos, dueño, nombre) y CreateEventA(...)
k_CreateMutexA:
        mov eax, H_MUTEX
        ret
k_CreateEventA:
        mov eax, H_EVENTO
        ret
; int MulDiv(a, b, c): a·b/c redondeado (64 bits en medio); c = 0: -1
k_MulDiv:
        mov ecx, [ebp + 8]
        test ecx, ecx
        jz .mal
        mov eax, [ebp]
        imul dword [ebp + 4]
        mov ebx, ecx                ; redondeo: ± c/2 según el signo
        sar ebx, 1
        test edx, edx
        js .neg
        add eax, ebx
        adc edx, 0
        jmp .d
.neg:
        sub eax, ebx
        sbb edx, 0
.d:
        idiv ecx
        ret
.mal:
        or eax, -1
        ret

; ---- (dll_de_nombre) las DLL del CRT de Visual C++ 2015 y siguientes
; (api-ms-win-crt-*.dll, VCRUNTIME140.DLL, UCRTBASE.DLL) son una sola, la
; nuestra: UCRT.DLL (b_dll, en mayúsculas) ----
alias_ucrt:
        push esi
        push edi
        push ecx
        push es
        push ds
        pop es
        mov esi, b_dll
        mov edi, t_apims
        mov ecx, 15
        repe cmpsb
        je .si
        mov esi, b_dll
        mov edi, t_vcrt
        mov ecx, 12
        repe cmpsb
        je .si
        mov esi, b_dll
        mov edi, t_ucrtb
        mov ecx, 8
        repe cmpsb
        jne .x
.si:
        mov esi, t_ucrt
        mov edi, b_dll
        mov ecx, 9
        rep movsb
.x:
        pop es
        pop ecx
        pop edi
        pop esi
        ret
t_apims     db 'API-MS-WIN-CRT-'
t_vcrt      db 'VCRUNTIME140'
t_ucrtb     db 'UCRTBASE'
t_ucrt      db 'UCRT.DLL', 0
; ---- lo que piden los programas de Visual C++ 2015 al arrancar ----
k_GetStartupInfoW:                  ; (STARTUPINFOW*): como la A (sin textos)
        jmp k_GetStartupInfoA
k_InitializeSListHead:              ; (cabeza): a cero
        mov edi, [ebp]
        mov dword [es:edi], 0
        mov dword [es:edi + 4], 0
        ret
k_IsProcessorFeaturePresent:        ; (qué): PF_FLOATING_POINT_EMULATED (1) no, lo demás no
        xor eax, eax
        ret
k_MoveFileExA:                      ; (de, a, opciones): como MoveFileA
        jmp k_MoveFileA
k_FormatMessageA:                   ; (...): no hay textos de los errores
        xor eax, eax
        ret

; =============================================================================
;  Los .INI
; =============================================================================
INI_MAX     equ 0x10000             ; lo más que se lee de uno
; (interna) lee el .INI de nombre [EBP+ESI] (del programa) -> EBX=su texto
; (en el programa, con un 0 detrás) ECX=sus octetos; EBX=0 si no hay memoria
ini_cargar:
        push esi
        mov ecx, INI_MAX + 16
        call reserva
        mov ebx, eax
        xor ecx, ecx
        test eax, eax
        jz .x
        mov eax, [ebp + esi]
        push ebx
        push 0                      ; CreateFileA(nombre, GENERIC_READ, 1, 0,
        push 0                      ;   OPEN_EXISTING, 0, 0)
        push 3
        push 0
        push 1
        push 0x80000000
        push eax
        push ebp
        lea ebp, [esp + 4]
        call k_CreateFileA
        pop ebp
        add esp, 28
        pop ebx
        xor ecx, ecx
        cmp eax, -1
        je .z
        push eax
        push ebx
        lea ecx, [ebx + INI_MAX + 4]    ; (los leídos, detrás del texto)
        push 0                      ; ReadFile(h, búfer, INI_MAX, *leídos, 0)
        push ecx
        push INI_MAX
        push ebx
        push eax
        push ebp
        lea ebp, [esp + 4]
        call k_ReadFile
        pop ebp
        add esp, 20
        pop ebx
        mov ecx, [es:ebx + INI_MAX + 4]
        test eax, eax
        jnz .r
        xor ecx, ecx
.r:
        pop eax
        push ebx
        push ecx
        push eax                    ; CloseHandle(h)
        push ebp
        lea ebp, [esp + 4]
        call k_CloseHandle
        pop ebp
        add esp, 4
        pop ecx
        pop ebx
.z:
        mov byte [es:ebx + ecx], 0
.x:
        pop esi
        ret
; (interna) ESI=texto (del programa) -> salta espacios y tabuladores
ini_blancos:
        cmp byte [es:esi], ' '
        je .s
        cmp byte [es:esi], 9
        jne .x
.s:
        inc esi
        jmp ini_blancos
.x:
        ret
; (interna) ¿empieza en ESI (del texto) el nombre EDI (del programa, sin
; mirar mayúsculas), seguido de AH (']' o '=', o espacios antes)? ZF=1 si sí
; (ESI queda tras él)
ini_igual:
        push edi
        push eax
.c:
        mov al, [es:edi]
        test al, al
        jz .f
        mov ah, [es:esi]
        call mayus2
        cmp al, ah
        jne .x
        inc esi
        inc edi
        jmp .c
.f:
        call ini_blancos
        mov al, [es:esi]
        cmp al, [esp + 1]
.x:
        pop eax
        pop edi
        ret
; (interna) ESI -> al principio de la línea siguiente (o en el 0 final)
ini_linea_sig:
        mov al, [es:esi]
        test al, al
        jz .x
        inc esi
        cmp al, 10
        jne ini_linea_sig
.x:
        ret
; DWORD GetPrivateProfileStringA(sección, clave, por defecto, búfer, sitio,
;                                archivo) -> las letras copiadas
k_GetPrivateProfileStringA:
        mov esi, 20
        call ini_cargar
        test ebx, ebx
        jz .defecto0
        mov [ini_buf], ebx
        mov edi, [ebp + 12]         ; EDI = el búfer, EDX = lo que cabe
        mov edx, [ebp + 16]
        test edx, edx
        jz .fin0
        dec edx
        mov dword [ini_n], 0
        mov esi, ebx
        mov byte [ini_dentro], 0
.l:
        call ini_blancos
        mov al, [es:esi]
        test al, al
        jz .nohay
        cmp al, '['
        jne .clave
        inc esi                     ; una sección: ¿es la nuestra?
        mov byte [ini_dentro], 0
        mov eax, [ebp]
        test eax, eax
        jz .lista_sec
        push edi
        mov edi, eax
        mov ah, ']'
        call ini_igual
        pop edi
        jne .sig
        mov byte [ini_dentro], 1
        jmp .sig
.lista_sec:                         ; (sin sección: los nombres de todas)
        mov ah, ']'
        call ini_copiar_hasta
        jmp .sig
.clave:
        cmp byte [ini_dentro], 0
        je .sig
        cmp al, ';'                 ; (comentarios)
        je .sig
        mov eax, [ebp + 4]
        test eax, eax
        jz .lista_cla
        push edi
        mov edi, eax
        mov ah, '='
        call ini_igual
        pop edi
        jne .sig
        inc esi                     ; el valor: tras «=», sin los blancos de
        call ini_blancos            ; delante ni de detrás
        mov ebx, esi
.v:
        mov al, [es:esi]
        test al, al
        jz .vf
        cmp al, 13
        je .vf
        cmp al, 10
        je .vf
        inc esi
        jmp .v
.vf:
        cmp esi, ebx
        jbe .vc
        cmp byte [es:esi - 1], ' '
        je .vb
        cmp byte [es:esi - 1], 9
        jne .vc
.vb:
        dec esi
        jmp .vf
.vc:
        mov ecx, esi
        sub ecx, ebx
        cmp ecx, edx
        jbe .vd
        mov ecx, edx
.vd:
        mov eax, ecx
        mov esi, ebx
.ve:
        jecxz .vg
        mov bl, [es:esi]
        mov [es:edi], bl
        inc esi
        inc edi
        dec ecx
        jmp .ve
.vg:
        mov byte [es:edi], 0
        mov [ini_n], eax
        jmp .fin
.lista_cla:                         ; (sin clave: los nombres de las de la sección)
        mov ah, '='
        call ini_copiar_hasta
.sig:
        call ini_linea_sig
        jmp .l
.nohay:
        cmp dword [ebp + 4], 0      ; (las listas acaban con otro 0)
        je .lista_fin
        cmp dword [ebp], 0
        je .lista_fin
        mov eax, [ini_buf]
        call suelta
        jmp .defecto
.lista_fin:
        mov byte [es:edi], 0
        jmp .fin
.defecto0:
        mov edx, [ebp + 16]
        test edx, edx
        jz .cero
        dec edx
        mov edi, [ebp + 12]
.defecto:
        mov esi, [ebp + 8]          ; lo de por defecto
        xor ecx, ecx
        test esi, esi
        jz .d2
.d1:
        cmp ecx, edx
        jae .d2
        mov al, [es:esi + ecx]
        test al, al
        jz .d2
        mov [es:edi + ecx], al
        inc ecx
        jmp .d1
.d2:
        mov byte [es:edi + ecx], 0
        mov eax, ecx
        ret
.fin0:
        mov dword [ini_n], 0
.fin:
        mov eax, [ini_buf]
        call suelta
        mov eax, [ini_n]
        ret
.cero:
        xor eax, eax
        ret
; (de GetPrivateProfileStringA) copia a EDI el nombre que empieza en ESI
; hasta AH (o el fin de la línea), con su 0; EDX = lo que queda de sitio
; (deja uno para el 0 final de la lista)
ini_copiar_hasta:
.c:
        mov al, [es:esi]
        test al, al
        jz .f
        cmp al, ah
        je .f
        cmp al, 13
        je .f
        cmp al, 10
        je .f
        cmp edx, 1
        jbe .s
        mov [es:edi], al
        inc edi
        dec edx
        inc dword [ini_n]
.s:
        inc esi
        jmp .c
.f:
        cmp edx, 1
        jbe .x
        mov byte [es:edi], 0
        inc edi
        dec edx
        inc dword [ini_n]
.x:
        ret
; UINT GetPrivateProfileIntA(sección, clave, por defecto, archivo)
k_GetPrivateProfileIntA:
        sub esp, 16                 ; el texto, en la pila del programa
        mov eax, esp
        push dword [ebp + 12]       ; GetPrivateProfileStringA(sección, clave,
        push 16                     ;   "", búfer, 16, archivo)
        push eax
        push ini_nada_p             ; (lo pone abajo: un "" del programa)
        push dword [ebp + 4]
        push dword [ebp]
        mov eax, [img_base]         ; (el "" del programa: en lo nuestro)
        add eax, [misc_off]
        add eax, HILO_TRAMP + 16
        mov byte [es:eax], 0
        mov [esp + 8], eax
        push ebp
        lea ebp, [esp + 4]
        call k_GetPrivateProfileStringA
        pop ebp
        add esp, 24
        test eax, eax
        jz .def
        mov esi, esp
        xor eax, eax
        xor ecx, ecx
        cmp byte [ss:esi], '-'
        jne .c
        inc esi
        inc ecx
.c:
        movzx ebx, byte [ss:esi]
        sub ebx, '0'
        jb .f
        cmp ebx, 9
        ja .f
        imul eax, 10
        add eax, ebx
        inc esi
        jmp .c
.f:
        jecxz .x
        neg eax
        jmp .x
.def:
        mov eax, [ebp + 8]
.x:
        add esp, 16
        ret
ini_nada_p  equ 0
; BOOL WritePrivateProfileStringA(sección, clave, valor, archivo)
k_WritePrivateProfileStringA:
        cmp dword [ebp], 0
        je .mal
        mov esi, 12
        call ini_cargar
        test ebx, ebx
        jz .mal
        mov [ini_buf], ebx
        mov [ini_len], ecx
        mov ecx, INI_MAX + 1024     ; lo que se escribe
        call reserva
        test eax, eax
        jz .mal2
        mov [ini_sal], eax
        mov edi, eax
        mov esi, [ini_buf]
        mov byte [ini_dentro], 0
        mov byte [ini_hecho], 0
        mov byte [ini_vista], 0
.l:
        mov ebx, esi                ; el principio de la línea
        call ini_blancos
        mov al, [es:esi]
        test al, al
        jz .eof
        cmp al, '['
        jne .clave
        cmp byte [ini_dentro], 0    ; sale de la nuestra: la clave nueva, aquí
        je .s1
        call ini_poner_clave
.s1:
        mov byte [ini_dentro], 0
        inc esi
        push edi
        mov edi, [ebp]
        mov ah, ']'
        call ini_igual
        pop edi
        jne .copia
        mov byte [ini_dentro], 1
        mov byte [ini_vista], 1
        cmp dword [ebp + 4], 0      ; (sin clave: la sección entera, fuera)
        jne .copia
        mov byte [ini_hecho], 1
        jmp .salta
.clave:
        cmp byte [ini_dentro], 0
        je .copia
        cmp dword [ebp + 4], 0
        je .salta
        push edi
        mov edi, [ebp + 4]
        mov ah, '='
        call ini_igual
        pop edi
        jne .copia
        cmp byte [ini_hecho], 0     ; (repetida: fuera)
        jne .salta
        mov byte [ini_hecho], 1
        cmp dword [ebp + 8], 0      ; (sin valor: la clave, fuera)
        je .salta
        call ini_escribir_clave
        mov esi, ebx
        call ini_linea_sig
        jmp .l
.salta:
        mov esi, ebx
        call ini_linea_sig
        jmp .l
.copia:
        mov esi, ebx
.cp:
        mov al, [es:esi]
        test al, al
        jz .l
        mov [es:edi], al
        inc esi
        inc edi
        cmp al, 10
        jne .cp
        jmp .l
.eof:
        cmp byte [ini_dentro], 0
        je .e1
        call ini_poner_clave
.e1:
        cmp byte [ini_vista], 0     ; no estaba la sección: al final
        jne .esc
        cmp dword [ebp + 4], 0
        je .esc
        cmp dword [ebp + 8], 0
        je .esc
        mov eax, [ini_sal]
        cmp edi, eax
        je .e2
        cmp byte [es:edi - 1], 10
        je .e2
        mov word [es:edi], 0x0A0D
        add edi, 2
.e2:
        mov byte [es:edi], '['
        inc edi
        mov esi, [ebp]
        call ini_poner_txt
        mov byte [es:edi], ']'
        mov word [es:edi + 1], 0x0A0D
        add edi, 3
        call ini_escribir_clave
.esc:
        mov ecx, edi                ; al archivo
        sub ecx, [ini_sal]
        mov eax, [ebp + 12]
        push ecx
        push 0                      ; CreateFileA(nombre, GENERIC_WRITE, 0, 0,
        push 0                      ;   CREATE_ALWAYS, 0, 0)
        push 2
        push 0
        push 0
        push 0x40000000
        push eax
        push ebp
        lea ebp, [esp + 4]
        call k_CreateFileA
        pop ebp
        add esp, 28
        pop ecx
        cmp eax, -1
        je .mal3
        push eax
        mov edx, [ini_buf]          ; (los escritos, en el búfer de antes)
        push 0                      ; WriteFile(h, salida, n, *escritos, 0)
        push edx
        push ecx
        push dword [ini_sal]
        push eax
        push ebp
        lea ebp, [esp + 4]
        call k_WriteFile
        pop ebp
        add esp, 20
        pop eax
        push eax                    ; CloseHandle(h)
        push ebp
        lea ebp, [esp + 4]
        call k_CloseHandle
        pop ebp
        add esp, 4
        mov eax, [ini_sal]
        call suelta
        mov eax, [ini_buf]
        call suelta
        mov eax, 1
        ret
.mal3:
        mov eax, [ini_sal]
        call suelta
.mal2:
        mov eax, [ini_buf]
        call suelta
.mal:
        xor eax, eax
        ret
; (interna) si en la sección no estaba la clave, se pone (antes de la línea
; que sigue)
ini_poner_clave:
        cmp byte [ini_hecho], 0
        jne .x
        cmp dword [ebp + 4], 0
        je .x
        cmp dword [ebp + 8], 0
        je .x
        mov byte [ini_hecho], 1
        cmp edi, [ini_sal]          ; (tras un salto de línea)
        je .e
        cmp byte [es:edi - 1], 10
        je .e
        mov word [es:edi], 0x0A0D
        add edi, 2
.e:
        call ini_escribir_clave
.x:
        ret
; (interna) escribe en EDI «clave=valor» y el salto de línea
ini_escribir_clave:
        push esi
        mov esi, [ebp + 4]
        call ini_poner_txt
        mov byte [es:edi], '='
        inc edi
        mov esi, [ebp + 8]
        call ini_poner_txt
        mov word [es:edi], 0x0A0D
        add edi, 2
        pop esi
        ret
; (interna) copia el texto ESI (del programa) a EDI, sin el 0 (1000 como mucho)
ini_poner_txt:
        push ecx
        mov ecx, 1000
.c:
        mov al, [es:esi]
        test al, al
        jz .x
        mov [es:edi], al
        inc esi
        inc edi
        dec ecx
        jnz .c
.x:
        pop ecx
        ret

        align 4
hilo_n      dd 0
hilo_act    dd 0
hilo_esp    dd 0
hilo_ebp    dd 0
hilo_cod    times N_HILO dd 0
ini_buf     dd 0
ini_sal     dd 0
ini_len     dd 0
ini_n       dd 0
hilo_dentro db 0
ini_dentro  db 0
ini_hecho   db 0
ini_vista   db 0
