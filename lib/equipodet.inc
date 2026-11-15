; =============================================================================
;  Hispa-DOS - deteccion minima de hardware para recomendar version
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Rutinas sacadas (lo justo) de utils/equipo.asm para que ACTUALIZAR pueda
;  recomendar la edicion mas rica que el equipo aguanta:
;    det_cpu    -> AL = nivel de CPU  (3 = 386, 4 = 486, 5 = 586/Pentium+)
;    det_ram    -> EAX = KiB de memoria extendida (aprox.; E801h, 88h o 0)
;    det_disco  -> entra DL = unidad (1=A,2=B,3=C...), sale EAX = MiB libres
;
;  Requiere 386 (usa pushfd/popfd y registros de 32 bits). Sin acentos ni
;  enies: se incluye sin pasar por cp437.
; =============================================================================

; Se permite CPUID (instruccion de 586) aunque el programa sea cpu 386: solo se
; ejecuta en tiempo real cuando el bit ID de EFLAGS dice que existe.
        cpu 586

; nivel de CPU -> AL (3 = 386, 4 = 486, 5 = Pentium o superior)
det_cpu:
        push ebx
        push ecx
        mov ebx, 0x200000           ; bit ID de EFLAGS: hay CPUID?
        call dd_toggle
        jnz .cpuid
        mov ebx, 0x40000            ; bit AC de EFLAGS: lo tiene el 486, el 386 no
        call dd_toggle
        jnz .i486
        mov al, 3                  ; ni CPUID ni AC: un 386
        jmp .fin
.i486:
        mov al, 4
        jmp .fin
.cpuid:
        xor eax, eax
        cpuid
        test eax, eax
        jz .cpuid486               ; CPUID pero sin hoja 1: tratar como 486
        mov eax, 1
        cpuid
        shr eax, 8
        and eax, 15                ; familia
        cmp al, 5
        jae .i586
        cmp al, 4
        je .i486b
        cmp al, 3
        je .i386b
        ; familia rara (6, 15...): es Pentium o superior
.i586:
        mov al, 5
        jmp .fin
.i486b:
        mov al, 4
        jmp .fin
.i386b:
        mov al, 3
        jmp .fin
.cpuid486:
        mov al, 4
.fin:
        pop ecx
        pop ebx
        ret

; EFLAGS: intenta cambiar los bits de EBX; ZF=0 si cambiaron (existen).
; (copiado de equipo.asm: flag_toggles)
dd_toggle:
        mov dx, sp
        and sp, 0xFFFC
        pushfd
        pop eax
        mov ecx, eax
        xor eax, ebx
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        mov sp, dx
        xor eax, ecx
        test eax, ebx
        ret

; memoria extendida en KiB -> EAX (aprox.). Primero INT 15h E801h (KB de 1 a
; 16 MB en AX/CX + bloques de 64 KB por encima en BX/DX); si no, INT 15h 88h;
; si no, 0.
det_ram:
        push ebx
        push ecx
        push edx
        xor cx, cx
        xor dx, dx
        mov ax, 0xE801
        stc
        int 0x15
        jc .try88
        test ax, ax                ; algunas BIOS usan CX/DX en vez de AX/BX
        jnz .haveab
        test cx, cx
        jz .try88
        mov ax, cx
        mov bx, dx
.haveab:
        movzx eax, ax              ; KB entre 1 y 16 MB
        movzx ebx, bx              ; bloques de 64 KB por encima de 16 MB
        shl ebx, 6                 ; *64 -> KB
        add eax, ebx
        jmp .fin
.try88:
        mov ah, 0x88
        stc
        int 0x15
        jc .zero
        movzx eax, ax
        test eax, eax
        jnz .fin
.zero:
        xor eax, eax
.fin:
        pop edx
        pop ecx
        pop ebx
        ret

; MiB libres de una unidad -> EAX. Entra DL = unidad (0=actual,1=A,2=B,3=C...).
det_disco:
        push ebx
        push ecx
        push edx
        mov ah, 0x36
        int 0x21                    ; AX=sect/cluster BX=clusters libres
        cmp ax, 0xFFFF              ; CX=bytes/sector DX=clusters totales
        je .bad
        movzx eax, ax
        movzx ecx, cx
        mul ecx                     ; EAX = bytes por cluster (cabe en 32 bits)
        movzx ebx, bx
        mul ebx                     ; EDX:EAX = bytes libres
        ; (EDX:EAX) >> 20 -> MiB
        shrd eax, edx, 20
        shr edx, 20
        test edx, edx
        jz .fin                    ; < 4 TB: todo en EAX
        mov eax, 0xFFFFFFFF        ; por si acaso, tope
        jmp .fin
.bad:
        xor eax, eax
.fin:
        pop edx
        pop ecx
        pop ebx
        ret

; volver al nivel del programa (no emitir instrucciones de mas de 386)
        cpu 386
