; =============================================================================
;  HVPRUN - las llamadas al sistema de Hispa-Pentium (int 0x80), el reloj,
;  la memoria y los argumentos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La llamada: EAX = qué, y sus datos en EBX, ECX y EDX; lo que devuelve, en
;  EAX (los errores, de 0xFFFFFFF0 para arriba). Los demás registros vuelven
;  como estaban (docs/PENTIUM.md, sección 6).
; =============================================================================

PS_ESCRIBIR     equ 1
PS_SALIR        equ 2
PS_DORMIR       equ 3
PS_TICS         equ 4
PS_ABRIR        equ 7
PS_LEER         equ 8
PS_CERRAR       equ 9
PS_ENTRADA      equ 12
PS_LARGO        equ 13
PS_ARGS         equ 14
PS_DATO         equ 15
PS_MEMORIA      equ 17
PS_ESCRIBIR_A   equ 18
PS_MOVER        equ 19
PS_CORTAR       equ 20
PS_BORRAR       equ 21
PS_RENOMBRAR    equ 22
PS_CREAR_DIR    equ 23
PS_BORRAR_DIR   equ 24
PS_CAMBIAR_DIR  equ 25
PS_DIR_ACTUAL   equ 26
PS_VENTANA      equ 32
PS_CERRAR_VEN   equ 33
PS_DIBUJAR      equ 34
PS_SUCESO       equ 35
PS_TITULO       equ 36
PS_PORTAPAPELES equ 40
PS_UNIDAD       equ 42
PS_COLOCAR      equ 80              ; colocar una ventana (nueva: usuario.cas)
PRIMER_ERROR    equ 0xFFFFFFF0
DATO_HZ         equ 9
DATO_FECHA      equ 10
DATO_ANCHO      equ 11
DATO_ALTO       equ 12
DATO_ESCRITORIO equ 13

; la llamada %1, con EBX, ECX y EDX como estén
%macro PSIS 1
        mov eax, %1
        int 0x80
%endmacro

; -> CF=1 si no hay escritorio (o no hay pantalla gráfica)
pent_iniciar:
        pushad
        mov ebx, DATO_HZ
        PSIS PS_DATO
        test eax, eax
        jnz .h
        mov eax, 100
.h:
        mov [p_hz], eax
        mov ebx, DATO_ESCRITORIO
        PSIS PS_DATO
        cmp eax, 1
        jne .no
        mov ebx, DATO_ANCHO
        PSIS PS_DATO
        mov [pan_max_an], eax
        mov ebx, DATO_ALTO
        PSIS PS_DATO
        mov [pan_max_al], eax
        cmp dword [pan_max_an], 320
        jb .no
        cmp dword [pan_max_al], 200
        jb .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; acaba el programa con el código EBX
pent_salir:
        PSIS PS_SALIR
        jmp pent_salir

; escribe el texto ESI (acabado en 0) por la salida (el puerto serie, con
; el escritorio en marcha)
pent_escribir:
        pushad
        mov ebx, esi
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .e
        inc ecx
        jmp .l
.e:
        PSIS PS_ESCRIBIR
        popad
        ret

; ECX=octetos (múltiplo de 4096) -> EAX=dónde empiezan (a cero); CF=1 si no hay
pent_mas_memoria:
        push ebx
        mov ebx, ecx
        PSIS PS_MEMORIA
        pop ebx
        cmp eax, PRIMER_ERROR
        cmc
        ret

; los tics de Hispa-Pentium, como los de la BIOS (18,2 por segundo) -> EAX
p_tics:
        push ebx
        push ecx
        push edx
        PSIS PS_TICS
        mov ecx, 182
        mul ecx
        mov ecx, [p_hz]
        imul ecx, 10
        div ecx
        pop edx
        pop ecx
        pop ebx
        ret

; (sistema) los tics de la BIOS de mentira, al día (nucleo.inc los lee ahí:
; los temporizadores)
bda_tics:
        push eax
        call p_tics
        mov [bda_falsa+0x46C], eax
        pop eax
        ret

; los argumentos: «PROGRAMA.HVP [ARCHIVO]» -> hvp_ruta (con su unidad y su
; carpeta, como en Ventanas: «C:\OFICINA\FACTURAS.HVP») y hvp_param (lo
; demás, sin los espacios de delante). CF=1 si no hay programa
p_args:
        pushad
        mov ebx, p_linea
        mov ecx, 255
        PSIS PS_ARGS
        cmp eax, 255
        jbe .l
        xor eax, eax
.l:
        mov byte [p_linea+eax], 0
        mov esi, p_linea
.e:
        cmp byte [esi], ' '
        jne .p
        inc esi
        jmp .e
.p:
        cmp byte [esi], 0
        je .no
        mov edi, p_aux              ; el programa, hasta el espacio
        mov ecx, 100
.c:
        lodsb
        cmp al, ' '
        jbe .cf
        stosb
        loop .c
.cf:
        mov byte [edi], 0
        dec esi
.e2:
        lodsb
        cmp al, ' '
        je .e2
        dec esi
        mov edi, hvp_param          ; lo demás, tal cual
        mov ecx, 120
        call cad_copiar
        mov esi, p_aux
        mov edi, hvp_ruta
        call p_ruta_entera
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; ESI=ruta -> EDI=la misma, entera: «X:\CARPETA\NOMBRE» (con la unidad y la
; carpeta de trabajo si no las lleva)
p_ruta_entera:
        pushad
        cmp byte [esi], 0
        je .x
        cmp byte [esi+1], ':'
        jne .sin_unidad
        movsw                       ; «X:»
        jmp .carpeta
.sin_unidad:
        call p_unidad
        stosb
        mov al, ':'
        stosb
.carpeta:
        cmp byte [esi], '\'
        je .resto
        cmp byte [esi], '/'
        je .resto
        push esi                    ; la carpeta de trabajo, delante
        push edi
        mov ebx, p_aux2
        mov ecx, 100
        PSIS PS_DIR_ACTUAL
        pop edi
        cmp eax, 100
        jb .d
        xor eax, eax
.d:
        mov byte [p_aux2+eax], 0
        mov esi, p_aux2
        cmp byte [esi], '\'
        je .d2
        mov al, '\'
        stosb
.d2:
        call pon_cad
        cmp byte [edi-1], '\'
        je .d3
        mov al, '\'
        stosb
.d3:
        pop esi
.resto:
        lodsb
        cmp al, '/'
        jne .r1
        mov al, '\'
.r1:
        stosb
        test al, al
        jnz .resto
.x:
        mov byte [edi], 0
        popad
        ret

; -> AL=la letra de la unidad de trabajo («C»)
p_unidad:
        push ebx
        push eax
        xor ebx, ebx
        PSIS PS_UNIDAD
        cmp eax, PRIMER_ERROR
        jb .b
        mov eax, 'C'
.b:
        and al, 0xDF
        mov bl, al
        pop eax
        mov al, bl
        pop ebx
        ret

; =============================================================================
;  La memoria
; =============================================================================
; el montón de Ventanas (nucleo.inc: mem_pedir, mem_soltar), detrás del programa
p_mem_iniciar:
        pushad
        mov ecx, MONTON_TAM
        call pent_mas_memoria
        jc .x
        mov [mon_ini], eax
        mov dword [eax], MONTON_TAM
        mov dword [eax+4], 0
        clc
.x:
        popad
        ret

; mem_bloque: ECX=octetos -> EAX=puntero EDX=mango (el mismo puntero); CF=1 si
; no hay. Los bloques sueltos se vuelven a usar; el último se devuelve al
; núcleo. Ventanas no los pone a cero (el DPMI tampoco).
MB_MAX      equ 64
mem_bloque:
        push ebx
        push ecx
        push esi
        add ecx, 4095
        and ecx, ~4095
        jz .no
        mov esi, mb_tabla           ; ¿uno suelto que valga?
        mov ebx, MB_MAX
.b:
        cmp dword [esi], 0
        je .s
        cmp byte [esi+8], 0
        jne .s
        cmp [esi+4], ecx
        jb .s
        mov byte [esi+8], 1
        mov eax, [esi]
        jmp .si
.s:
        add esi, 12
        dec ebx
        jnz .b
        mov esi, mb_tabla           ; uno nuevo: una ficha libre
        mov ebx, MB_MAX
.f:
        cmp dword [esi], 0
        je .nueva
        add esi, 12
        dec ebx
        jnz .f
        jmp .no
.nueva:
        call pent_mas_memoria
        jc .no
        mov [esi], eax
        mov [esi+4], ecx
        mov byte [esi+8], 1
.si:
        mov edx, eax
        clc
        jmp .x
.no:
        xor eax, eax
        stc
.x:
        pop esi
        pop ecx
        pop ebx
        ret

; EDX=mango de mem_bloque
mem_bloque_soltar:
        pushad
        mov esi, mb_tabla
        mov ecx, MB_MAX
.b:
        cmp [esi], edx
        je .hay
        add esi, 12
        loop .b
        jmp .x
.hay:
        mov byte [esi+8], 0
        mov ebx, [esi+4]            ; ¿es lo último? se devuelve
        add ebx, edx
        push ebx
        xor ebx, ebx
        PSIS PS_MEMORIA             ; (con 0: dónde acaba ahora)
        pop ebx
        cmp eax, ebx
        jne .x
        mov ebx, [esi+4]
        neg ebx
        PSIS PS_MEMORIA
        cmp eax, PRIMER_ERROR
        jae .x
        mov dword [esi], 0
.x:
        popad
        ret

; ---- lo pequeño de la interfaz (modulos.inc) ----
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
ceder:
        mov byte [actividad], 1
        jmp tar_ceder
