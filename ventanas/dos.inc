; =============================================================================
;  Hispa Ventanas - archivos: las llamadas a DOS desde modo protegido
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  INT 21h quiere los punteros en memoria de modo real. En vez de hacer que el
;  servidor DPMI los traduzca, Hispa Ventanas usa la función 0300h con un
;  búfer de paso (dos_buf) que está dentro del propio programa, en memoria
;  convencional: como el segmento de datos tiene la base del programa, el
;  mismo búfer se ve desde los dos modos (aquí como puntero, en modo real
;  como segmento del programa y desplazamiento). No hace falta pedir memoria
;  de DOS ni tocar HSPDPMI, y los programas DPMI de terceros siguen igual.
;
;  Todas devuelven CF=1 si DOS da error (EAX=código de error de DOS).
;    dos_abrir      ESI=nombre AL=modo (0 leer, 1 escribir, 2 las dos) -> EAX=manejador
;    dos_crear      ESI=nombre -> EAX=manejador (3Ch)
;    dos_cerrar     EBX=manejador (3Eh)
;    dos_leer       EBX=manejador EDI=destino ECX=octetos -> EAX=leídos (3Fh)
;    dos_escribir   EBX=manejador ESI=datos ECX=octetos -> EAX=escritos (40h)
;    dos_puntero    EBX=manejador AL=origen EDX=posición -> EAX=posición (42h)
;    dos_borrar     ESI=nombre (41h)
;    dos_cambiar_dir, dos_crear_dir   ESI=ruta (3Bh, 39h)
;    dos_dir_actual EDI=búfer (80 octetos): «A:\CARPETA» (19h y 47h)
;    dos_ir_carpeta ESI=ruta, con unidad o sin ella: la unidad y la carpeta
;                   de trabajo pasan a ser esas (0Eh y 3Bh)
;    dos_primero    ESI=patrón ECX=atributos (4Eh); dos_siguiente (4Fh)
;                   el resultado queda en dos_dta: +0x15 atributo, +0x1A
;                   tamaño, +0x1E nombre
;    dos_int        EAX, EBX, ECX, EDX: INT 21h con esos registros (y
;                   DS=ES=el programa: los punteros, dentro de dos_buf)
; =============================================================================

DOS_TRANS   equ 2048
dos_dta     equ 0x80                ; la zona de transferencia de disco (PSP)

dos_int:
        mov [rmcs+0x1C], eax
        mov [rmcs+0x10], ebx
        mov [rmcs+0x18], ecx
        mov [rmcs+0x14], edx
        push ebx
        mov bl, 0x21
        call rm_int
        pop ebx
        movzx eax, word [rmcs+0x1C]
        test byte [rmcs+0x20], 1
        jz .ok
        stc
        ret
.ok:
        clc
        ret

; (interna) AX=función y modo, ECX, ESI=ruta: DS:DX apunta a una copia
dos_ruta:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax
        mov edi, dos_buf
        mov edx, 127
.c:
        lodsb
        stosb
        test al, al
        jz .l
        dec edx
        jnz .c
        mov byte [edi], 0
.l:
        pop eax
        mov edx, dos_buf
        xor ebx, ebx
        call dos_int
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

dos_abrir:
        mov ah, 0x3D
        jmp dos_ruta
dos_crear:
        push ecx
        xor ecx, ecx
        mov ah, 0x3C
        call dos_ruta
        pop ecx
        ret
dos_borrar:
        mov ah, 0x41
        jmp dos_ruta
dos_cambiar_dir:
        mov ah, 0x3B
        jmp dos_ruta
dos_crear_dir:
        mov ah, 0x39
        jmp dos_ruta

; ESI=ruta, con su unidad o sin ella («A:\JUEGOS\SMS», «\JUEGOS», «SMS»): la
; carpeta de trabajo pasa a ser esa, y también la unidad (0Eh y 3Bh). CF=1 si
; no se puede (y entonces no cambia nada: ni la unidad)
dos_ir_carpeta:
        pushad
        mov eax, 0x1900             ; la unidad de ahora, por si hay que volver
        call dos_int
        mov [esp+28], eax
        cmp byte [esi], 0
        je .mal
        cmp byte [esi+1], ':'
        jne .dir
        movzx edx, byte [esi]
        or dl, 0x20                 ; (a minúscula)
        sub dl, 'a'
        cmp dl, 25
        ja .mal
        mov eax, 0x0E00
        call dos_int
        mov eax, 0x1900             ; ¿ha cambiado de verdad?
        call dos_int
        movzx edx, byte [esi]
        or dl, 0x20
        sub dl, 'a'
        cmp al, dl
        jne .vuelve
        add esi, 2
        cmp byte [esi], 0           ; «A:» solo: la unidad, y basta
        je .bien
.dir:
        mov eax, 0x3B00
        call dos_ruta
        jnc .bien
.vuelve:
        movzx edx, byte [esp+28]    ; la unidad de antes
        mov eax, 0x0E00
        call dos_int
.mal:
        popad
        stc
        ret
.bien:
        popad
        clc
        ret

dos_cerrar:
        push eax
        mov eax, 0x3E00
        call dos_int
        pop eax
        ret

dos_puntero:
        push ecx
        push edx
        mov ecx, edx
        shr ecx, 16
        mov ah, 0x42
        call dos_int
        movzx edx, word [rmcs+0x14]
        pushf
        shl edx, 16
        or eax, edx
        popf
        pop edx
        pop ecx
        ret

dos_leer:
        push ecx
        push edx
        push esi
        push edi
        push ebp
        xor ebp, ebp
        mov esi, ecx
.l:
        test esi, esi
        jz .ok
        mov ecx, esi
        cmp ecx, DOS_TRANS
        jbe .c
        mov ecx, DOS_TRANS
.c:
        mov eax, 0x3F00
        mov edx, dos_buf
        call dos_int
        jc .err
        mov ecx, eax
        test ecx, ecx
        jz .ok
        add ebp, ecx
        sub esi, ecx
        push esi
        mov esi, dos_buf
        rep movsb
        pop esi
        jmp .l
.ok:
        mov eax, ebp
        clc
        jmp .x
.err:
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

dos_escribir:
        push ecx
        push edx
        push esi
        push edi
        push ebp
        xor ebp, ebp
        mov edx, ecx                ; lo que falta
.l:
        test edx, edx
        jz .ok
        mov ecx, edx
        cmp ecx, DOS_TRANS
        jbe .c
        mov ecx, DOS_TRANS
.c:
        push ecx
        mov edi, dos_buf
        rep movsb
        pop ecx
        push edx
        mov eax, 0x4000
        mov edx, dos_buf
        call dos_int
        pop edx
        jc .err
        add ebp, eax
        sub edx, eax
        cmp eax, ecx
        je .l
        mov eax, 0x1D               ; disco lleno: «fallo al escribir»
        jmp .err
.ok:
        mov eax, ebp
        clc
        jmp .x
.err:
        stc
.x:
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

dos_dir_actual:
        pushad
        mov eax, 0x1900
        call dos_int
        add al, 'A'
        stosb
        mov ax, ':\'
        stosw
        mov dword [rmcs+4], dos_buf ; DS:SI
        mov eax, 0x4700
        xor edx, edx
        call dos_int
        mov esi, dos_buf
        jc .f
        mov ecx, 64
.c:
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        popad
        ret

dos_primero:
        push ebx
        push edx
        push eax
        mov eax, 0x1A00             ; la DTA, en el PSP (por si la han movido)
        mov edx, dos_dta
        call dos_int
        pop eax
        pop edx
        pop ebx
        mov ah, 0x4E
        jmp dos_ruta

dos_siguiente:
        mov eax, 0x4F00
        jmp dos_int
