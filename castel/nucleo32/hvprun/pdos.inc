; =============================================================================
;  HVPRUN - los archivos de Ventanas (dos_*) y las interrupciones de modo
;  real que piden los programas (rm_interrup), sobre Hispa-Pentium
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Las mismas funciones (y los mismos registros) que ventanas/dos.inc; los
;  errores, con los números de DOS (2: no está, 3: la carpeta no está, 5:
;  acceso denegado...). Las rutas son las de Ventanas («C:\OFICINA\X.DAT»):
;  Hispa-Pentium admite la letra delante.
;  De INT 21h (rm_interrup y dos_int): 0Eh, 19h, 1Ah, 2Ah, 2Ch, 36h, 39h,
;  3Ah, 3Bh, 41h, 47h, 4408h, 4Eh, 4Fh y 56h (y los archivos, 3Ch-42h). De
;  la BIOS: INT 11h dice que no hay puerto paralelo e INT 17h, que la
;  impresora no contesta (Hispa-Pentium aún no imprime). Lo demás, «función
;  no válida» (y una línea por el puerto serie, para saber qué falta).
; =============================================================================

PE_NO_HAY       equ 0xFFFFFFFF
PE_SIN_DISCO    equ 0xFFFFFFFE
PE_LLENO        equ 0xFFFFFFFD
PE_MAL          equ 0xFFFFFFFC
PE_NO_CARPETA   equ 0xFFFFFFF1

; (interna) EAX=lo que ha dado Hispa-Pentium -> CF=1 y EAX=el error de DOS
; si es un error; si no, CF=0 y EAX como estaba
p_err:
        cmp eax, PRIMER_ERROR
        jb .bien
        cmp eax, PE_NO_HAY
        jne .e1
        mov eax, 2
        stc
        ret
.e1:
        cmp eax, PE_NO_CARPETA
        jne .e2
        mov eax, 3
        stc
        ret
.e2:
        cmp eax, PE_LLENO
        jne .e3
        mov eax, 4
        stc
        ret
.e3:
        cmp eax, PE_SIN_DISCO
        jne .e4
        mov eax, 0x0F
        stc
        ret
.e4:
        cmp eax, PE_MAL
        jne .e5
        mov eax, 0x1F
        stc
        ret
.e5:
        mov eax, 5
        stc
        ret
.bien:
        clc
        ret

; ESI=nombre AL=modo (0 leer, 1 escribir, 2 las dos) -> EAX=manejador
dos_abrir:
        push ebx
        push ecx
        push edx
        mov ebx, esi
        xor ecx, ecx
        test al, 3
        jz .a
        mov ecx, 1                  ; (para escribir: uno que ya está)
.a:
        PSIS PS_ABRIR
        pop edx
        pop ecx
        pop ebx
        jmp p_err

; ESI=nombre -> EAX=manejador (nuevo; si estaba, vacío)
dos_crear:
        push ebx
        push ecx
        push edx
        mov ebx, esi
        mov ecx, 2
        PSIS PS_ABRIR
        pop edx
        pop ecx
        pop ebx
        jmp p_err

; EBX=manejador
dos_cerrar:
        push eax
        PSIS PS_CERRAR
        pop eax
        clc
        ret

; EBX=manejador EDI=destino ECX=octetos -> EAX=leídos
dos_leer:
        push ecx
        push edx
        mov edx, ecx
        mov ecx, edi
        PSIS PS_LEER
        pop edx
        pop ecx
        jmp p_err

; EBX=manejador ESI=datos ECX=octetos -> EAX=escritos (con 0, como en DOS:
; el archivo se corta donde está)
dos_escribir:
        push ecx
        push edx
        test ecx, ecx
        jnz .e
        PSIS PS_CORTAR
        pop edx
        pop ecx
        call p_err
        jc .r
        xor eax, eax
.r:
        ret
.e:
        mov edx, ecx
        mov ecx, esi
        PSIS PS_ESCRIBIR_A
        pop edx
        pop ecx
        call p_err
        jc .x
        cmp eax, ecx
        je .x
        mov eax, 0x1D               ; disco lleno: «fallo al escribir»
        stc
.x:
        ret

; EBX=manejador AL=origen EDX=posición -> EAX=posición nueva
dos_puntero:
        push ecx
        push edx
        movzx eax, al
        mov ecx, edx
        mov edx, eax
        PSIS PS_MOVER
        pop edx
        pop ecx
        jmp p_err

; ESI=nombre
dos_borrar:
        push ebx
        mov ebx, esi
        PSIS PS_BORRAR
        pop ebx
        jmp p_err
dos_cambiar_dir:
        push ebx
        mov ebx, esi
        PSIS PS_CAMBIAR_DIR
        pop ebx
        jmp p_err
dos_crear_dir:
        push ebx
        mov ebx, esi
        PSIS PS_CREAR_DIR
        pop ebx
        jmp p_err

; ESI=ruta, con unidad o sin ella: la unidad y la carpeta de trabajo pasan a
; ser esas. CF=1 si no se puede.
dos_ir_carpeta:
        pushad
        cmp byte [esi], 0
        je .mal
        cmp byte [esi+1], ':'
        jne .dir
        movzx ebx, byte [esi]
        and bl, 0xDF
        PSIS PS_UNIDAD
        cmp eax, PRIMER_ERROR
        jae .mal
        add esi, 2
        cmp byte [esi], 0
        je .bien
.dir:
        call dos_cambiar_dir
        jc .mal
.bien:
        popad
        clc
        ret
.mal:
        popad
        mov eax, 3
        stc
        ret

; EDI=búfer (80 octetos): «C:\CARPETA»
dos_dir_actual:
        pushad
        call p_unidad
        stosb
        mov al, ':'
        stosb
        mov ebx, edi
        mov ecx, 76
        PSIS PS_DIR_ACTUAL
        cmp eax, 76
        jb .b
        mov byte [edi], '\'
        mov eax, 1
.b:
        mov byte [edi+eax], 0
        test eax, eax
        jnz .x
        mov word [edi], '\'
.x:
        popad
        ret

; ESI=patrón ECX=atributos: el primero (en dos_dta); dos_siguiente, el que sigue
dos_primero:
        push edx
        mov dword [p_dta_ptr], dos_dta
        mov edx, esi
        call bus_primero
        pop edx
        ret
dos_siguiente:
        jmp bus_siguiente

; -> EAX=el último error crítico de disco (-1: ninguno)
dos_crit:
        or eax, -1
        ret

; EAX, EBX, ECX, EDX: INT 21h con esos registros (lo de dialogos.inc)
dos_int:
        push esi
        sub esp, RL_TAM
        mov esi, esp
        mov [esi+RL_EAX], eax
        mov [esi+RL_EBX], ebx
        mov [esi+RL_ECX], ecx
        mov [esi+RL_EDX], edx
        mov dword [esi+RL_ESI], 0
        mov dword [esi+RL_EDI], 0
        mov byte [esi+RL_INT], 0x21
        call rm_interrup
        mov eax, [esi+RL_EAX]
        lea esp, [esp+RL_TAM]       ; (sin tocar las banderas)
        pop esi
        ret

; =============================================================================
;  rm_interrup: ESI=bloque de registros (RL_*). A la vuelta, lo que deja la
;  interrupción y RL_BAND (bit 0, el acarreo); CF=1 si hay error.
; =============================================================================
rm_interrup:
        pushad
        mov ebp, esi
        and dword [ebp+RL_BAND], ~1
        mov al, [ebp+RL_INT]
        cmp al, 0x21
        je .dos
        cmp al, 0x11
        je .equipo
        cmp al, 0x17
        je .impresora
        jmp .no_vale
.equipo:
        mov dword [ebp+RL_EAX], 0x0000  ; ni puertos paralelos ni disqueteras
        jmp .bien
.impresora:
        mov eax, [ebp+RL_EAX]
        mov ah, 0x08                ; error de la impresora (no contesta)
        mov [ebp+RL_EAX], eax
        jmp .bien
.dos:
        mov eax, [ebp+RL_EAX]
        mov ebx, [ebp+RL_EBX]
        mov ecx, [ebp+RL_ECX]
        mov edx, [ebp+RL_EDX]
        movzx esi, ah
        cmp ah, 0x0E
        je .f0e
        cmp ah, 0x19
        je .f19
        cmp ah, 0x1A
        je .f1a
        cmp ah, 0x2A
        je .f2a
        cmp ah, 0x2C
        je .f2c
        cmp ah, 0x36
        je .f36
        cmp ah, 0x39
        je .f39
        cmp ah, 0x3A
        je .f3a
        cmp ah, 0x3B
        je .f3b
        cmp ah, 0x3C
        je .f3c
        cmp ah, 0x3D
        je .f3d
        cmp ah, 0x3E
        je .f3e
        cmp ah, 0x3F
        je .f3f
        cmp ah, 0x40
        je .f40
        cmp ah, 0x41
        je .f41
        cmp ah, 0x42
        je .f42
        cmp ah, 0x44
        je .f44
        cmp ah, 0x47
        je .f47
        cmp ah, 0x4E
        je .f4e
        cmp ah, 0x4F
        je .f4f
        cmp ah, 0x56
        je .f56
.no_vale:
        push esi                    ; lo que falta, por el puerto serie
        mov esi, t_int_falta
        call pent_escribir
        movzx eax, byte [ebp+RL_INT]
        call p_hex2
        mov esi, t_int_ah
        call pent_escribir
        movzx eax, byte [ebp+RL_EAX+1]
        call p_hex2
        mov esi, t_nl
        call pent_escribir
        pop esi
        mov eax, 1                  ; función no válida
.mal:
        mov [ebp+RL_EAX], eax
        or dword [ebp+RL_BAND], 1
        popad
        stc
        ret
.bien:
        popad
        clc
        ret
.ax:                                ; (lo que da, en AX; el resto de EAX, como estaba)
        mov [ebp+RL_EAX], ax
        jmp .bien
.f0e:                               ; la unidad DL (0 = A)
        movzx ebx, dl
        add bl, 'A'
        PSIS PS_UNIDAD
        mov al, 26
        mov byte [ebp+RL_EAX], al
        jmp .bien
.f19:
        call p_unidad
        sub al, 'A'
        mov byte [ebp+RL_EAX], al
        jmp .bien
.f1a:
        mov [p_dta_ptr], edx
        jmp .bien
.f2a:                               ; la fecha: CX=año DH=mes DL=día AL=día de la semana
        call p_fecha
        shr eax, 16
        mov ecx, eax
        shr ecx, 9
        add ecx, 1980
        mov [ebp+RL_ECX], cx
        mov edx, eax
        shr edx, 5
        and edx, 15
        mov bh, dl                  ; el mes
        mov bl, al
        and bl, 31                  ; el día
        mov [ebp+RL_EDX], bx
        ; el día de la semana (0 = domingo), por Zeller
        movzx eax, bh               ; m
        movzx edx, bl               ; d
        cmp eax, 3
        jae .z1
        add eax, 12
        dec ecx
.z1:
        inc eax                     ; 13 * (m + 1) / 5
        imul eax, 13
        push edx
        xor edx, edx
        mov ebx, 5
        div ebx
        pop edx
        add eax, edx                ; + d
        add eax, ecx                ; + año + año/4 - año/100 + año/400
        mov ebx, ecx
        shr ebx, 2
        add eax, ebx
        push eax
        mov eax, ecx
        xor edx, edx
        mov ebx, 100
        div ebx
        mov ebx, eax
        pop eax
        sub eax, ebx
        shr ebx, 2
        add eax, ebx
        xor edx, edx
        mov ebx, 7
        div ebx                     ; 0 = sábado
        add edx, 6
        cmp edx, 7
        jb .z2
        sub edx, 7
.z2:
        mov byte [ebp+RL_EAX], dl
        jmp .bien
.f2c:                               ; la hora: CH=horas CL=minutos DH=segundos DL=0
        call p_fecha
        mov ecx, eax
        shr ecx, 11
        and ecx, 31
        mov edx, eax
        shr edx, 5
        and edx, 63
        mov ch, cl
        mov cl, dl
        mov [ebp+RL_ECX], cx
        and eax, 31
        add eax, eax
        mov ah, al
        xor al, al
        mov [ebp+RL_EDX], ax
        jmp .bien
.f36:                               ; lo libre de la unidad DL (0: la de ahora)
        call p_info_unidad
        jc .f36no
        mov word [ebp+RL_EAX], 2    ; (clusters de 1 K: 2 sectores de 512)
        mov word [ebp+RL_ECX], 512
        mov eax, [p_info+8]         ; libre_k
        cmp eax, 0xFFFF
        jbe .f36a
        mov eax, 0xFFFF
.f36a:
        mov [ebp+RL_EBX], ax
        mov eax, [p_info+4]
        cmp eax, 0xFFFF
        jbe .f36b
        mov eax, 0xFFFF
.f36b:
        mov [ebp+RL_EDX], ax
        jmp .bien
.f36no:
        mov word [ebp+RL_EAX], 0xFFFF
        jmp .bien
.f39:
        mov esi, edx
        call dos_crear_dir
        jc .mal
        jmp .bien
.f3a:
        mov ebx, edx
        PSIS PS_BORRAR_DIR
        call p_err
        jc .mal
        jmp .bien
.f3b:
        mov esi, edx
        call dos_cambiar_dir
        jc .mal
        jmp .bien
.f3c:
        mov esi, edx
        call dos_crear
        jc .mal
        jmp .ax
.f3d:
        mov esi, edx
        call dos_abrir
        jc .mal
        jmp .ax
.f3e:
        call dos_cerrar
        jmp .bien
.f3f:
        mov edi, edx
        call dos_leer
        jc .mal
        jmp .ax
.f40:
        mov esi, edx
        call dos_escribir
        jc .mal
        jmp .ax
.f41:
        mov esi, edx
        call dos_borrar
        jc .mal
        jmp .bien
.f42:                               ; CX:DX = posición, AL = origen -> DX:AX
        shl ecx, 16
        mov cx, dx
        mov edx, ecx
        call dos_puntero
        jc .mal
        mov [ebp+RL_EAX], ax
        shr eax, 16
        mov [ebp+RL_EDX], ax
        jmp .bien
.f44:                               ; 4408h: ¿existe la unidad BL (1 = A)?
        cmp al, 0x08
        jne .no_vale
        movzx edx, bl
        test edx, edx
        jnz .f44a
        call p_unidad
        movzx edx, al
        sub edx, 'A' - 1
.f44a:
        add edx, 'A' - 1
        call p_hay_unidad
        jc .f44no
        mov word [ebp+RL_EAX], 1    ; (fija)
        jmp .bien
.f44no:
        mov eax, 0x0F
        jmp .mal
.f47:                               ; DS:SI = la carpeta de la unidad DL, sin la «\» de delante
        mov edi, [ebp+RL_ESI]
        mov ebx, p_aux2
        mov ecx, 100
        PSIS PS_DIR_ACTUAL
        cmp eax, 100
        jb .f47a
        xor eax, eax
.f47a:
        mov byte [p_aux2+eax], 0
        mov esi, p_aux2
        cmp byte [esi], '\'
        jne .f47b
        inc esi
.f47b:
        mov ecx, 63
        call cad_copiar
        jmp .bien
.f4e:
        call bus_primero
        jc .mal
        jmp .bien
.f4f:
        call bus_siguiente
        jc .mal
        jmp .bien
.f56:                               ; DS:DX el nombre de ahora, ES:DI el nuevo
        mov ebx, edx
        mov ecx, [ebp+RL_EDI]
        PSIS PS_RENOMBRAR
        call p_err
        jc .mal
        jmp .bien

t_int_falta db "HVPRUN: falta la interrupci", 0xA2, "n ", 0
t_int_ah    db "h, AH=", 0
t_nl        db "h", 10, 0

; (interna) -> EAX=la fecha (arriba) y la hora (abajo), como las guarda la FAT
p_fecha:
        push ebx
        mov ebx, DATO_FECHA
        PSIS PS_DATO
        pop ebx
        ret

; (interna) escribe AL en hexadecimal por la salida
p_hex2:
        pushad
        mov edi, p_aux2
        mov ecx, eax
        shr eax, 4
        and eax, 15
        mov al, [t_hex+eax]
        stosb
        mov eax, ecx
        and eax, 15
        mov al, [t_hex+eax]
        stosb
        mov byte [edi], 0
        mov esi, p_aux2
        call pent_escribir
        popad
        ret
t_hex       db "0123456789ABCDEF"

; (interna) EDX=letra de la unidad -> CF=0 si existe
p_hay_unidad:
        pushad
        and dl, 0xDF
        xor esi, esi
.b:
        mov ebx, esi
        mov ecx, p_info
        mov eax, 43
        int 0x80
        cmp eax, 1
        jne .no
        cmp [p_info], dl
        je .si
        inc esi
        cmp esi, 32
        jb .b
.no:
        popad
        stc
        ret
.si:
        popad
        clc
        ret

; (interna) DL=unidad (0 = la de ahora, 1 = A) -> p_info con lo que se
; sabe de ella; CF=1 si no está
p_info_unidad:
        pushad
        movzx edx, dl
        test edx, edx
        jnz .l
        call p_unidad
        movzx edx, al
        sub edx, 'A' - 1
.l:
        add edx, 'A' - 1
        call p_hay_unidad
        popad
        ret

; =============================================================================
;  Buscar archivos (4Eh y 4Fh): la carpeta se abre y se lee entrada a entrada
;  (llamadas 7 y 12); lo que vale, a la zona de transferencia (p_dta_ptr):
;  +15h atributo, +16h hora, +18h fecha, +1Ah tamaño, +1Eh nombre
; =============================================================================
; EDX=patrón («C:\CARPETA\*.HVP», «*.*») ECX=atributos -> CF=1 y EAX=error
bus_primero:
        pushad
        call bus_cerrar
        mov [bus_atr], cl
        ; la carpeta: hasta la última «\» o «:»
        mov esi, edx
        mov ebx, edx                ; dónde empieza el nombre
.c:
        lodsb
        test al, al
        jz .c1
        cmp al, '\'
        je .c0
        cmp al, ':'
        jne .c
.c0:
        mov ebx, esi
        jmp .c
.c1:
        mov ecx, ebx
        sub ecx, edx                ; las letras de la carpeta
        cmp ecx, 100
        ja .nocarpeta
        mov esi, edx
        mov edi, bus_dir
        rep movsb
        mov byte [edi], 0
        ; sin la «\» del final (salvo en la raíz: «\», «C:\»)
        cmp edi, bus_dir
        jne .d1
        mov word [bus_dir], '.'     ; nada: la carpeta de trabajo
        jmp .d3
.d1:
        cmp byte [edi-1], ':'       ; «C:» solo: su raíz
        jne .d2
        mov word [edi], '\'
        jmp .d3
.d2:
        cmp byte [edi-1], '\'
        jne .d3
        lea eax, [edi-1]
        cmp eax, bus_dir
        je .d3
        cmp byte [edi-2], ':'
        je .d3
        mov byte [edi-1], 0
.d3:
        ; el nombre, como en las fichas del DOS: 8 + 3, con «?»
        mov esi, ebx
        mov edi, bus_patron
        mov ecx, 8
        call bus_campo
        cmp byte [esi], '.'
        jne .e
        inc esi
.e:
        mov ecx, 3
        call bus_campo
        mov ebx, bus_dir
        xor ecx, ecx
        PSIS PS_ABRIR
        cmp eax, PRIMER_ERROR
        jae .nocarpeta
        mov [bus_m], eax
        popad
        jmp bus_siguiente
.nocarpeta:
        popad
        mov eax, 3
        stc
        ret

; (interna) ESI -> EDI: un campo de ECX letras (en mayúsculas; «*» lo llena de «?»)
bus_campo:
.l:
        mov al, [esi]
        test al, al
        jz .resto
        cmp al, '.'
        je .resto
        inc esi
        cmp al, '*'
        je .estrella
        call bus_mayus
        stosb
        loop .l
.salta:                             ; lo que sobra, hasta el punto
        mov al, [esi]
        test al, al
        jz .x
        cmp al, '.'
        je .x
        inc esi
        jmp .salta
.estrella:
        mov al, '?'
        rep stosb
        jmp .salta
.resto:
        mov al, ' '
        rep stosb
.x:
        ret

bus_mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 0x20
.x:
        ret

; -> el siguiente en la zona de transferencia; CF=1 y EAX=18 si no hay más
bus_siguiente:
        pushad
        cmp dword [bus_m], 0
        je .fin
.l:
        mov ebx, [bus_m]
        mov ecx, bus_ent
        mov eax, PS_ENTRADA
        int 0x80
        cmp eax, 1
        jne .acabar
        mov al, [bus_ent+13]        ; los atributos
        test al, 0x08               ; la etiqueta del disco, nunca
        jnz .l
        mov ah, [bus_atr]
        test al, 0x10
        jz .a1
        test ah, 0x10
        jz .l
.a1:
        test al, 0x06               ; ocultos y del sistema, si se piden
        jz .a2
        mov dl, al
        and dl, 0x06
        and dl, ah
        jz .l
.a2:
        ; el nombre, a 8 + 3, y se compara con el patrón
        mov esi, bus_ent
        mov edi, bus_nom
        mov ecx, 8
        call bus_campo
        cmp byte [esi], '.'
        jne .n1
        inc esi
.n1:
        mov ecx, 3
        call bus_campo
        cmp byte [bus_ent], '.'     ; «.» y «..» tal cual
        jne .cmp
        mov edi, bus_nom
        mov ecx, 11
        mov al, ' '
        rep stosb
        mov al, [bus_ent+1]
        mov byte [bus_nom], '.'
        cmp al, '.'
        jne .cmp
        mov byte [bus_nom+1], '.'
.cmp:
        xor ecx, ecx
.k:
        mov al, [bus_patron+ecx]
        cmp al, '?'
        je .k1
        cmp al, [bus_nom+ecx]
        jne .l
.k1:
        inc ecx
        cmp ecx, 11
        jb .k
        ; vale: a la zona de transferencia
        mov edi, [p_dta_ptr]
        mov al, [bus_ent+13]
        mov [edi+0x15], al
        mov ax, [bus_ent+22]
        mov [edi+0x16], ax
        mov ax, [bus_ent+20]
        mov [edi+0x18], ax
        mov eax, [bus_ent+16]
        mov [edi+0x1A], eax
        lea edi, [edi+0x1E]
        mov esi, bus_ent
        mov ecx, 12
        call cad_copiar
        popad
        clc
        ret
.acabar:
        call bus_cerrar
.fin:
        popad
        mov eax, 18                 ; no hay más
        stc
        ret

bus_cerrar:
        push eax
        push ebx
        mov ebx, [bus_m]
        test ebx, ebx
        jz .x
        PSIS PS_CERRAR
        mov dword [bus_m], 0
.x:
        pop ebx
        pop eax
        ret
