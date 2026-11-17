; =============================================================================
;  HW32: KERNEL32 de consola (fases 1 y 2 de Hispa-486)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Las funciones se llaman desde «despacho» con EBP = los argumentos (stdcall,
;  en el orden de C), ES = el programa (sus punteros), DS = lo nuestro y GS =
;  el búfer de DOS. Devuelven en EAX (y EDX). Pueden usar todos los registros.
;  Hecho a partir de la documentación pública del API Win32; se comporta
;  como Windows 98 (4.10).
;  (Este archivo no pasa por tools/cp437.py: los textos, sin acentos.)
; =============================================================================

H_BASE      equ 0x10                ; handle de Win32 = el de DOS + H_BASE
H_BUSCA     equ 0x4E00              ; las búsquedas: H_BUSCA + n
H_KERNEL32  equ 0xBFF70000          ; (donde estaba KERNEL32 en Windows 9x)
LCID_ES     equ 0x0C0A              ; español (internacional)

; =============================================================================
;  Ayudas
; =============================================================================
; el handle de Win32 EAX a DOS en rm_bx; CF=1 (y error 6) si no vale
handle_dos:
        sub eax, H_BASE
        cmp eax, 255
        ja .mal
        mov [rm_bx], ax
        clc
        ret
.mal:
        mov dword [ult_error], 6
        stc
        ret
; acarreo de DOS: el error de DOS (es el mismo número en Win32) a ult_error
error_dos:
        movzx eax, word [rm_ax]
        mov [ult_error], eax
        ret
; ---- el texto ASCIZ del programa ES:ESI al búfer GS:EDI (hasta 259) ----
nom_a_tb:
        push eax
        push ecx
        mov ecx, 259
.c:
        mov al, [es:esi]
        mov [gs:edi], al
        test al, al
        jz .x
        inc esi
        inc edi
        loop .c
        mov byte [gs:edi], 0
.x:
        pop ecx
        pop eax
        ret
; ---- el texto ASCIZ del búfer GS:ESI al programa ES:EDI, con ECX de sitio
; (con el 0): EAX = su largo sin el 0; si no cabe, no copia y EAX = largo+1 ----
tb_a_prog:
        push esi
        xor eax, eax
.l:
        cmp byte [gs:esi + eax], 0
        je .l1
        inc eax
        jmp .l
.l1:
        cmp eax, ecx
        jb .copia
        inc eax                     ; no cabe: lo que hace falta
        pop esi
        ret
.copia:
        push eax
        push edi
.c:
        mov dl, [gs:esi]
        mov [es:edi], dl
        inc esi
        inc edi
        test dl, dl
        jnz .c
        pop edi
        pop eax
        pop esi
        ret
; ---- compara sin mayúsculas el texto del programa ES:ESI con el nuestro
; DS:EDI; ZF=1 si son iguales ----
iguala_ci:
        push esi
        push edi
        push eax
.c:
        mov al, [es:esi]
        mov ah, [edi]
        call mayus2
        cmp al, ah
        jne .x
        test al, al
        jz .x
        inc esi
        inc edi
        jmp .c
.x:
        pop eax
        pop edi
        pop esi
        ret
; AL y AH a mayúsculas (solo a-z)
mayus2:
        cmp al, 'a'
        jb .a
        cmp al, 'z'
        ja .a
        sub al, 32
.a:
        cmp ah, 'a'
        jb .b
        cmp ah, 'z'
        ja .b
        sub ah, 32
.b:
        ret
; ---- ECX octetos a cero en el programa ES:EDI ----
a_cero:
        push eax
        push ecx
        push edi
        xor eax, eax
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret
; ---- una INT 10h con los rm_* ----
bios10:
        push ebx
        mov bl, 0x10
        call int_rm
        pop ebx
        ret
bios16:
        push ebx
        mov bl, 0x16
        call int_rm
        pop ebx
        ret

; =============================================================================
;  Consola: handles, escribir y leer
; =============================================================================
k_GetStdHandle:                     ; (nStdHandle)
        mov eax, [ebp]
        cmp eax, -10
        je .ent
        cmp eax, -11
        je .sal
        cmp eax, -12
        je .err
        mov dword [ult_error], 6
        mov eax, -1
        ret
.ent:
        mov eax, H_BASE + 0
        ret
.sal:
        mov eax, H_BASE + 1
        ret
.err:
        mov eax, H_BASE + 2
        ret
k_SetStdHandle:                     ; (n, h)
        mov eax, 1
        ret
; ¿el handle de DOS de rm_bx es la consola? CF=0 si lo es
es_consola:
        push word [rm_bx]
        mov word [rm_ax], 0x4400
        call dos
        pop word [rm_bx]
        jc .no
        test byte [rm_dx], 0x80     ; dispositivo
        jz .no
        test byte [rm_dx], 3        ; entrada o salida de la consola
        jz .no
        clc
        ret
.no:
        stc
        ret
k_GetFileType:                      ; (h)
        mov eax, [ebp]
        cmp eax, H_BUSCA
        jae .desc
        call handle_dos
        jc .desc
        mov word [rm_ax], 0x4400
        call dos
        jc .desc
        mov eax, 1                  ; FILE_TYPE_DISK
        test byte [rm_dx], 0x80
        jz .x
        mov eax, 2                  ; FILE_TYPE_CHAR
.x:
        ret
.desc:
        xor eax, eax                ; FILE_TYPE_UNKNOWN
        ret

k_WriteFile:                        ; (h, buf, n, *escritos, ovl)
k_WriteConsoleA:                    ; (h, buf, n, *escritos, reservado)
        mov eax, [ebp]
        call handle_dos
        jc .mal_h
        mov esi, [ebp + 4]          ; el texto (en el programa: ES)
        mov ecx, [ebp + 8]
        xor ebx, ebx                ; lo escrito
        ; ¿con color? (a la consola, por la BIOS)
        cmp byte [con_attr], 7
        je .t
        call es_consola
        jc .t
        jmp escribe_color           ; (acaba como .x)
.t:
        test ecx, ecx
        jz .x
        mov edx, ecx
        cmp edx, TB_TROZO
        jbe .t1
        mov edx, TB_TROZO
.t1:
        push ecx
        push esi
        xor edi, edi
.cp:
        mov al, [es:esi]
        mov [gs:edi], al
        inc esi
        inc edi
        cmp edi, edx
        jb .cp
        pop esi
        pop ecx
        mov [rm_cx], dx
        mov word [rm_dx], 0
        mov word [rm_ax], 0x4000
        call dos
        jc .mal
        movzx eax, word [rm_ax]
        add ebx, eax
        add esi, eax
        sub ecx, eax
        cmp eax, edx
        jb .x                       ; (disco lleno)
        jmp .t
.x:
        mov eax, [ebp + 12]
        test eax, eax
        jz .y
        mov [es:eax], ebx
.y:
        mov eax, 1
        ret
.mal:
        call error_dos
.mal_h:
        xor eax, eax
        ret
; los ECX caracteres de ES:ESI a la pantalla con el color con_attr (EBX:
; contados); acaba como WriteFile
escribe_color:
        mov word [rm_ax], 0x0F00    ; la página
        call bios10
        mov al, [rm_bx + 1]
        mov [pag_act], al
.c:
        test ecx, ecx
        jz k_WriteConsoleA.x
        mov al, [es:esi]
        cmp al, 32
        jb .ctl
        ; el carácter con su color (09h) y luego avanzar (0Eh)
        mov ah, 0x09
        mov [rm_ax], ax
        mov ah, [pag_act]
        mov al, [con_attr]
        mov [rm_bx], ax
        mov word [rm_cx], 1
        call bios10
        mov al, [es:esi]
.ctl:
        mov ah, 0x0E
        mov [rm_ax], ax
        mov ah, [pag_act]
        mov al, 7
        mov [rm_bx], ax
        call bios10
        inc esi
        inc ebx
        dec ecx
        jmp .c

k_ReadFile:                         ; (h, buf, n, *leídos, ovl)
k_ReadConsoleA:                     ; (h, buf, n, *leídos, reservado)
        mov eax, [ebp]
        call handle_dos
        jc .mal_h
        mov edi, [ebp + 4]
        mov ecx, [ebp + 8]
        xor ebx, ebx                ; lo leído
.t:
        test ecx, ecx
        jz .x
        mov edx, ecx
        cmp edx, TB_TROZO
        jbe .t1
        mov edx, TB_TROZO
.t1:
        mov [rm_cx], dx
        mov word [rm_dx], 0
        mov word [rm_ax], 0x3F00
        call dos
        jc .mal
        movzx eax, word [rm_ax]
        push ecx
        xor esi, esi
        mov ecx, eax
        jecxz .cp_x
.cp:
        mov dl, [gs:esi]
        mov [es:edi], dl
        inc esi
        inc edi
        loop .cp
.cp_x:
        pop ecx
        add ebx, eax
        sub ecx, eax
        cmp ax, [rm_cx]
        jb .x                       ; (fin del archivo o una línea de la consola)
        jmp .t
.x:
        mov eax, [ebp + 12]
        test eax, eax
        jz .y
        mov [es:eax], ebx
.y:
        mov eax, 1
        ret
.mal:
        call error_dos
.mal_h:
        xor eax, eax
        ret

k_SetConsoleTextAttribute:          ; (h, color)
        mov al, [ebp + 4]
        mov [con_attr], al
        mov eax, 1
        ret
k_SetConsoleCursorPosition:         ; (h, COORD: X abajo, Y arriba)
        mov word [rm_ax], 0x0F00
        call bios10
        mov al, [rm_bx + 1]
        mov [rm_bx + 1], al
        mov al, [ebp + 4]           ; X
        mov ah, [ebp + 6]           ; Y
        mov [rm_dx], ax
        mov word [rm_ax], 0x0200
        call bios10
        mov eax, 1
        ret
; las columnas y filas de la pantalla (de la BIOS): ECX columnas, EDX filas
tam_pantalla:
        push es
        mov es, [sel_bios]
        movzx ecx, word [es:0x4A]
        movzx edx, byte [es:0x84]
        pop es
        test edx, edx
        jnz .f
        mov edx, 24
.f:
        inc edx
        test ecx, ecx
        jnz .x
        mov ecx, 80
.x:
        ret
; el cursor: AL = X, AH = Y
cursor_act:
        push es
        push ebx
        mov es, [sel_bios]
        movzx ebx, byte [es:0x62]
        mov ax, [es:0x50 + ebx * 2]
        pop ebx
        pop es
        ret
k_GetConsoleScreenBufferInfo:       ; (h, *info)
        mov edi, [ebp + 4]
        call tam_pantalla
        mov [es:edi], cx            ; dwSize
        mov [es:edi + 2], dx
        call cursor_act
        movzx ebx, al
        mov [es:edi + 4], bx        ; dwCursorPosition
        movzx ebx, ah
        mov [es:edi + 6], bx
        movzx ax, byte [con_attr]
        mov [es:edi + 8], ax        ; wAttributes
        mov word [es:edi + 10], 0   ; srWindow
        mov word [es:edi + 12], 0
        lea eax, [ecx - 1]
        mov [es:edi + 14], ax
        lea eax, [edx - 1]
        mov [es:edi + 16], ax
        mov [es:edi + 18], cx       ; dwMaximumWindowSize
        mov [es:edi + 20], dx
        mov eax, 1
        ret
k_GetConsoleMode:                   ; (h, *modo)
        mov edi, [ebp + 4]
        mov dword [es:edi], 7       ; (entrada: procesada, línea, eco; salida: 3)
        cmp dword [ebp], H_BASE
        je .x
        mov dword [es:edi], 3
.x:
        mov eax, 1
        ret
k_SetConsoleMode:
k_SetConsoleTitleA:
k_SetConsoleCP:
k_SetConsoleOutputCP:
k_FlushFileBuffers_con:
        mov eax, 1
        ret
k_GetConsoleTitleA:                 ; (buf, n)
        xor eax, eax
        ret
k_GetConsoleCP:
k_GetConsoleOutputCP:
        mov eax, 437
        ret
k_GetConsoleCursorInfo:             ; (h, *info): tamaño y si se ve
        mov edi, [ebp + 4]
        mov dword [es:edi], 25
        mov dword [es:edi + 4], 1
        mov eax, 1
        ret
k_SetConsoleCursorInfo:             ; (h, *info)
        mov edi, [ebp + 4]
        mov word [rm_cx], 0x0607
        cmp dword [es:edi + 4], 0
        jne .s
        mov word [rm_cx], 0x2000
.s:
        mov word [rm_ax], 0x0100
        call bios10
        mov eax, 1
        ret
; la dirección en la pantalla de la COORD [EBP+12] (X, Y) -> EDI, y ES = la
; pantalla (el ES del programa, en la pila); ECX = cuántas celdas caben
celda_coord:
        call tam_pantalla
        movzx eax, word [ebp + 14]  ; Y
        imul eax, ecx
        movzx ebx, word [ebp + 12]  ; X
        add eax, ebx
        imul edx, ecx               ; las celdas que hay
        sub edx, eax
        mov ecx, edx
        lea edi, [eax * 2]
        push es
        mov es, [sel_bios]
        cmp byte [es:0x49], 7
        pop es
        mov ax, [sel_vid]
        jne .v
        mov ax, [sel_mda]
.v:
        mov [sel_pant], ax
        ret
k_FillConsoleOutputCharacterA:      ; (h, car, n, COORD, *escritos)
        call celda_coord
        mov ebx, [ebp + 8]
        cmp ebx, ecx
        jbe .n
        mov ebx, ecx
.n:
        mov ecx, ebx
        mov al, [ebp + 4]
        push es
        mov es, [sel_pant]
        jecxz .x
.c:
        mov [es:edi], al
        add edi, 2
        loop .c
.x:
        pop es
        jmp escritos_ebx
k_FillConsoleOutputAttribute:       ; (h, color, n, COORD, *escritos)
        call celda_coord
        mov ebx, [ebp + 8]
        cmp ebx, ecx
        jbe .n
        mov ebx, ecx
.n:
        mov ecx, ebx
        mov al, [ebp + 4]
        push es
        mov es, [sel_pant]
        jecxz .x
.c:
        mov [es:edi + 1], al
        add edi, 2
        loop .c
.x:
        pop es
        jmp escritos_ebx
k_WriteConsoleOutputCharacterA:     ; (h, texto, n, COORD, *escritos)
        call celda_coord
        mov ebx, [ebp + 8]
        cmp ebx, ecx
        jbe .n
        mov ebx, ecx
.n:
        mov ecx, ebx
        mov esi, [ebp + 4]
        jecxz .x
.c:
        mov al, [es:esi]
        push es
        mov es, [sel_pant]
        mov [es:edi], al
        pop es
        inc esi
        add edi, 2
        loop .c
.x:
        jmp escritos_ebx
; *[EBP+16] = EBX; devuelve 1
escritos_ebx:
        mov eax, [ebp + 16]
        test eax, eax
        jz .x
        mov [es:eax], ebx
.x:
        mov eax, 1
        ret

; ---- el teclado como eventos (INPUT_RECORD de 20 octetos). Tras cada
; pulsación se da también la de soltar ----
; ¿hay tecla? ZF=0 si hay (sin sacarla); AX = la tecla
mira_tecla:
        cmp byte [suelta_pend], 0
        jne .hay
        mov word [rm_ax], 0x1100
        call bios16
        test byte [rm_flags], 0x40  ; ZF de la BIOS
        ret
.hay:
        or al, 1                    ; (ZF=0)
        ret
; llena el registro ES:EDI con la tecla AX (AH código, AL carácter);
; BL = 1 pulsada, 0 soltada
llena_tecla:
        push ecx
        push edx
        push eax
        mov ecx, 20
        call a_cero
        mov word [es:edi], 1        ; KEY_EVENT
        movzx ecx, bl
        mov [es:edi + 4], ecx       ; bKeyDown
        mov word [es:edi + 8], 1    ; wRepeatCount
        movzx edx, ah               ; el código de la tecla
        mov [es:edi + 12], dx       ; wVirtualScanCode
        call vk_de_codigo           ; DL -> CL
        movzx ecx, cl
        mov [es:edi + 10], cx       ; wVirtualKeyCode
        pop eax
        push eax
        cmp al, 0xE0
        jne .c
        xor al, al
.c:
        mov [es:edi + 14], al       ; uChar
        ; las mayúsculas y demás (INT 16h, 02h)
        mov word [rm_ax], 0x0200
        call bios16
        mov al, [rm_ax]
        xor ecx, ecx
        test al, 3
        jz .s1
        or ecx, 0x10                ; SHIFT_PRESSED
.s1:
        test al, 4
        jz .s2
        or ecx, 0x08                ; LEFT_CTRL_PRESSED
.s2:
        test al, 8
        jz .s3
        or ecx, 0x02                ; LEFT_ALT_PRESSED
.s3:
        test al, 0x10
        jz .s4
        or ecx, 0x40                ; SCROLLLOCK_ON
.s4:
        test al, 0x20
        jz .s5
        or ecx, 0x20                ; NUMLOCK_ON
.s5:
        test al, 0x40
        jz .s6
        or ecx, 0x80                ; CAPSLOCK_ON
.s6:
        mov [es:edi + 16], ecx
        pop eax
        pop edx
        pop ecx
        ret
; el código de tecla DL (de la BIOS) -> la tecla virtual CL
vk_de_codigo:
        push esi
        xor ecx, ecx
        cmp dl, 0x58
        ja .ext
        movzx esi, dl
        mov cl, [vk_tabla + esi]
        pop esi
        ret
.ext:
        mov esi, vk_ext
.e:
        mov ch, [esi]
        test ch, ch
        jz .x
        cmp ch, dl
        je .si
        add esi, 2
        jmp .e
.si:
        mov cl, [esi + 1]
.x:
        xor ch, ch
        pop esi
        ret
; saca un evento al registro ES:EDI (espera si no hay)
saca_evento:
        cmp byte [suelta_pend], 0
        je .nueva
        mov byte [suelta_pend], 0
        mov ax, [suelta_tecla]
        xor bl, bl
        jmp llena_tecla
.nueva:
        mov word [rm_ax], 0x1000
        call bios16
        mov ax, [rm_ax]
        mov [suelta_tecla], ax
        mov byte [suelta_pend], 1
        mov bl, 1
        jmp llena_tecla
k_ReadConsoleInputA:                ; (h, *registros, n, *leídos)
        mov edi, [ebp + 4]
        call saca_evento
        mov ebx, 1
        mov eax, [ebp + 12]
        test eax, eax
        jz .x
        mov [es:eax], ebx
.x:
        mov eax, 1
        ret
k_PeekConsoleInputA:                ; (h, *registros, n, *leídos)
        xor ebx, ebx
        call mira_tecla
        jz .x
        mov edi, [ebp + 4]
        cmp byte [suelta_pend], 0
        je .p
        mov ax, [suelta_tecla]
        mov bl, 0
        call llena_tecla
        mov ebx, 1
        jmp .x
.p:
        mov ax, [rm_ax]
        mov bl, 1
        call llena_tecla
        mov ebx, 1
.x:
        mov eax, [ebp + 12]
        test eax, eax
        jz .y
        mov [es:eax], ebx
.y:
        mov eax, 1
        ret
k_GetNumberOfConsoleInputEvents:    ; (h, *n)
        xor ebx, ebx
        call mira_tecla
        jz .x
        inc ebx
.x:
        mov eax, [ebp + 4]
        mov [es:eax], ebx
        mov eax, 1
        ret
k_FlushConsoleInputBuffer:          ; (h)
        mov byte [suelta_pend], 0
.c:
        mov word [rm_ax], 0x1100
        call bios16
        test byte [rm_flags], 0x40
        jnz .x
        mov word [rm_ax], 0x1000
        call bios16
        jmp .c
.x:
        mov eax, 1
        ret

; =============================================================================
;  Archivos y directorios
; =============================================================================
; Los nombres especiales de la consola
s_conin     db 'CONIN$', 0
s_conout    db 'CONOUT$', 0
k_CreateFileA:                      ; (nombre, acceso, compartir, seg, cómo, opciones, plantilla)
        mov esi, [ebp]
        mov edi, s_conin
        call iguala_ci
        jne .no_in
        mov eax, H_BASE + 0
        ret
.no_in:
        mov edi, s_conout
        call iguala_ci
        jne .no_out
        mov eax, H_BASE + 1
        ret
.no_out:
        mov edi, TB_NOM1
        call nom_a_tb
        ; el modo de abrir: 0 leer, 1 escribir, 2 los dos
        mov eax, [ebp + 4]
        xor ebx, ebx
        test eax, 0x40000000        ; GENERIC_WRITE
        jz .m1
        mov bl, 1
        test eax, 0x80000000        ; y GENERIC_READ
        jz .m1
        mov bl, 2
.m1:
        mov eax, [ebp + 16]         ; cómo
        cmp eax, 1
        je .nuevo
        cmp eax, 2
        je .crea
        cmp eax, 3
        je .abre
        cmp eax, 4
        je .abre_o_crea
        cmp eax, 5
        je .trunca
        mov dword [ult_error], 87   ; ERROR_INVALID_PARAMETER
        jmp .mal_x
.nuevo:                             ; CREATE_NEW: 5Bh
        mov word [rm_ax], 0x5B00
        jmp .crea_con
.crea:                              ; CREATE_ALWAYS: 3Ch
        mov word [rm_ax], 0x3C00
.crea_con:
        mov eax, [ebp + 20]         ; los atributos (los de DOS)
        and eax, 0x27
        mov [rm_cx], ax
        mov word [rm_dx], TB_NOM1
        call dos
        jc .mal
        jmp .bien
.abre:                              ; OPEN_EXISTING
        call abre_bl
        jc .mal
        jmp .bien
.abre_o_crea:                       ; OPEN_ALWAYS
        call abre_bl
        jnc .bien
        cmp word [rm_ax], 2
        jne .mal
        jmp .crea
.trunca:                            ; TRUNCATE_EXISTING: si está, se rehace
        call abre_bl
        jc .mal
        mov ax, [rm_ax]
        mov [rm_bx], ax
        mov word [rm_ax], 0x3E00
        call dos
        jmp .crea
.bien:
        movzx eax, word [rm_ax]
        add eax, H_BASE
        mov dword [ult_error], 0
        ret
.mal:
        call error_dos
.mal_x:
        mov eax, -1                 ; INVALID_HANDLE_VALUE
        ret
abre_bl:
        mov bh, 0x3D                ; AH = 3Dh, AL = el modo (BL)
        mov [rm_ax], bx
        mov word [rm_dx], TB_NOM1
        jmp dos

k_CloseHandle:                      ; (h)
        mov eax, [ebp]
        cmp eax, H_BUSCA
        jae .si
        cmp eax, H_BASE + 5
        jb .si                      ; (la consola no se cierra)
        cmp eax, H_BASE + 255
        ja .si
        call handle_dos
        mov word [rm_ax], 0x3E00
        call dos
        jc .mal
.si:
        mov eax, 1
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
k_DuplicateHandle:                  ; (proc, h, proc, *nuevo, acceso, heredar, opciones)
        mov eax, [ebp + 4]
        call handle_dos
        jc .mal
        mov word [rm_ax], 0x4500
        call dos
        jc .mal_d
        movzx eax, word [rm_ax]
        add eax, H_BASE
        mov edi, [ebp + 12]
        mov [es:edi], eax
        mov eax, 1
        ret
.mal_d:
        call error_dos
.mal:
        xor eax, eax
        ret
k_SetFilePointer:                   ; (h, distancia, *alta, cómo)
        mov eax, [ebp]
        call handle_dos
        jc .mal_h
        mov eax, [ebp + 4]
        mov [rm_dx], ax
        shr eax, 16
        mov [rm_cx], ax
        mov al, [ebp + 12]
        mov ah, 0x42
        mov [rm_ax], ax
        call dos
        jc .mal
        mov eax, [ebp + 8]
        test eax, eax
        jz .s
        mov dword [es:eax], 0
.s:
        movzx eax, word [rm_dx]
        shl eax, 16
        mov ax, [rm_ax]
        ret
.mal:
        call error_dos
.mal_h:
        mov eax, -1
        ret
k_GetFileSize:                      ; (h, *alta)
        mov eax, [ebp]
        call handle_dos
        jc .mal_h
        mov ax, [rm_bx]
        mov [h_tam], ax
        xor ecx, ecx                ; dónde está
        mov al, 1
        call mueve_a
        jc .mal
        mov esi, eax
        xor ecx, ecx                ; el final
        mov al, 2
        call mueve_a
        jc .mal
        mov ebx, eax
        mov ecx, esi                ; y volver
        mov al, 0
        call mueve_a
        mov eax, [ebp + 4]
        test eax, eax
        jz .s
        mov dword [es:eax], 0
.s:
        mov eax, ebx
        ret
.mal:
        call error_dos
.mal_h:
        mov eax, -1
        ret
; 42h del handle [h_tam]: AL = cómo, ECX = la distancia -> EAX la posición
mueve_a:
        mov ah, 0x42
        mov [rm_ax], ax
        mov ax, [h_tam]
        mov [rm_bx], ax
        mov [rm_dx], cx
        shr ecx, 16
        mov [rm_cx], cx
        call dos
        jc .x
        movzx eax, word [rm_dx]
        shl eax, 16
        mov ax, [rm_ax]
        clc
.x:
        ret
k_SetEndOfFile:                     ; (h): escribir 0 octetos corta ahí
        mov eax, [ebp]
        call handle_dos
        jc .mal
        mov word [rm_cx], 0
        mov word [rm_dx], 0
        mov word [rm_ax], 0x4000
        call dos
        jc .mal_d
        mov eax, 1
        ret
.mal_d:
        call error_dos
.mal:
        xor eax, eax
        ret
k_FlushFileBuffers:                 ; (h)
        mov eax, [ebp]
        call handle_dos
        jc .x
        mov word [rm_ax], 0x6800
        call dos
.x:
        mov eax, 1
        ret
; ---- una orden de DOS con el nombre [EBP] en TB_NOM1 (DS:DX); devuelve
; 1, o 0 con el error ----
dos_con_nombre:                     ; AX = la orden
        mov [rm_ax], ax
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        mov word [rm_dx], TB_NOM1
        call dos
        jc .mal
        mov eax, 1
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
k_DeleteFileA:                      ; (nombre)
        mov ax, 0x4100
        jmp dos_con_nombre
k_CreateDirectoryA:                 ; (nombre, seg)
        mov ax, 0x3900
        jmp dos_con_nombre
k_RemoveDirectoryA:                 ; (nombre)
        mov ax, 0x3A00
        jmp dos_con_nombre
k_SetFileAttributesA:               ; (nombre, atributos)
        mov eax, [ebp + 4]
        and eax, 0x27
        mov [rm_cx], ax
        mov ax, 0x4301
        jmp dos_con_nombre
k_GetFileAttributesA:               ; (nombre)
        mov ax, 0x4300
        call dos_con_nombre
        test eax, eax
        jz .mal
        movzx eax, word [rm_cx]
        test eax, eax
        jnz .x
        mov eax, 0x80               ; FILE_ATTRIBUTE_NORMAL
.x:
        ret
.mal:
        mov eax, -1
        ret
k_MoveFileA:                        ; (de, a)
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        mov esi, [ebp + 4]
        mov edi, TB_NOM2
        call nom_a_tb
        mov word [rm_dx], TB_NOM1
        mov dword [rm_edi], TB_NOM2
        mov word [rm_ax], 0x5600
        call dos
        jc .mal
        mov eax, 1
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
k_CopyFileA:                        ; (de, a, falla_si_está)
        ; abrir el de origen
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        mov word [rm_ax], 0x3D00
        mov word [rm_dx], TB_NOM1
        call dos
        jc .mal
        mov ax, [rm_ax]
        mov [h_cop1], ax
        ; crear el otro
        mov esi, [ebp + 4]
        mov edi, TB_NOM1
        call nom_a_tb
        mov word [rm_ax], 0x3C00
        cmp dword [ebp + 8], 0
        je .c
        mov word [rm_ax], 0x5B00
.c:
        mov word [rm_cx], 0
        mov word [rm_dx], TB_NOM1
        call dos
        jc .mal_1
        mov ax, [rm_ax]
        mov [h_cop2], ax
.t:
        mov ax, [h_cop1]
        mov [rm_bx], ax
        mov word [rm_cx], TB_TROZO
        mov word [rm_dx], 0
        mov word [rm_ax], 0x3F00
        call dos
        jc .mal_2
        mov cx, [rm_ax]
        test cx, cx
        jz .fin
        mov [rm_cx], cx
        mov ax, [h_cop2]
        mov [rm_bx], ax
        mov word [rm_dx], 0
        mov word [rm_ax], 0x4000
        call dos
        jc .mal_2
        jmp .t
.fin:
        mov ax, [h_cop2]
        call cierra_ax
        mov ax, [h_cop1]
        call cierra_ax
        mov eax, 1
        ret
.mal_2:
        call error_dos
        mov ax, [h_cop2]
        call cierra_ax
        jmp .mal_c
.mal_1:
        call error_dos
.mal_c:
        mov ax, [h_cop1]
        call cierra_ax
        xor eax, eax
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
cierra_ax:
        push dword [ult_error]
        mov [rm_bx], ax
        mov word [rm_ax], 0x3E00
        call dos
        pop dword [ult_error]
        ret

k_GetCurrentDirectoryA:             ; (n, buf)
        mov word [rm_ax], 0x1900
        call dos
        mov al, [rm_ax]
        add al, 'A'
        mov [gs:TB_SAL], al
        mov word [gs:TB_SAL + 1], '\' * 256 + ':'
        mov word [rm_dx], 0
        mov dword [rm_esi], TB_SAL + 3
        mov word [rm_ax], 0x4700
        call dos
        mov esi, TB_SAL
        mov edi, [ebp + 4]
        mov ecx, [ebp]
        jmp tb_a_prog
k_SetCurrentDirectoryA:             ; (nombre)
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        cmp byte [gs:TB_NOM1 + 1], ':'
        jne .dir
        mov dl, [gs:TB_NOM1]
        or dl, 0x20
        sub dl, 'a'
        mov [rm_dx], dl
        mov word [rm_ax], 0x0E00
        call dos
        cmp byte [gs:TB_NOM1 + 2], 0
        je .bien
.dir:
        mov word [rm_dx], TB_NOM1
        mov word [rm_ax], 0x3B00
        call dos
        jc .mal
.bien:
        mov eax, 1
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
k_GetFullPathNameA:                 ; (nombre, n, buf, *parte)
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        mov dword [rm_esi], TB_NOM1
        mov dword [rm_edi], TB_SAL
        mov word [rm_ax], 0x6000
        call dos
        jc .mal
        mov esi, TB_SAL
        mov edi, [ebp + 8]
        mov ecx, [ebp + 4]
        call tb_a_prog
        cmp eax, [ebp + 4]
        jae .x                      ; (no cabía)
        ; *parte: tras la última barra
        mov ecx, [ebp + 12]
        test ecx, ecx
        jz .x
        mov edx, edi
        xor ebx, ebx
.b:
        mov bl, [es:edx]
        test bl, bl
        jz .b_f
        inc edx
        cmp bl, '\'
        jne .b
        mov [es:ecx], edx
        jmp .b
.b_f:
.x:
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
; la unidad de un nombre «X:...» (o la de ahora): DL = 1 (A:)...
unidad_de:
        mov esi, [ebp]
        test esi, esi
        jz .act
        cmp byte [es:esi + 1], ':'
        jne .act
        mov dl, [es:esi]
        or dl, 0x20
        sub dl, 'a' - 1
        ret
.act:
        mov word [rm_ax], 0x1900
        call dos
        mov dl, [rm_ax]
        inc dl
        ret
k_GetDriveTypeA:                    ; (raíz)
        call unidad_de
        mov eax, 2                  ; DRIVE_REMOVABLE (A: y B:)
        cmp dl, 2
        jbe .x
        mov [rm_bx], dl
        mov word [rm_ax], 0x4408
        call dos
        mov eax, 1                  ; DRIVE_NO_ROOT_DIR
        jc .x
        mov eax, 2
        cmp word [rm_ax], 0
        je .x
        mov eax, 3                  ; DRIVE_FIXED
.x:
        ret
k_GetLogicalDrives:
        xor ebx, ebx
        mov ecx, 1
.u:
        cmp ecx, 2
        jbe .si                     ; (A: y B: siempre)
        mov [rm_bx], cl
        mov word [rm_ax], 0x4409
        call dos
        jc .sig
.si:
        bts ebx, ecx
.sig:
        inc ecx
        cmp ecx, 26
        jbe .u
        mov eax, ebx
        shr eax, 1                  ; (bit 0 = A:)
        ret
k_GetDiskFreeSpaceA:                ; (raíz, *sect_clus, *octetos_sect, *libres, *total)
        call unidad_de
        mov [rm_dx], dl
        mov word [rm_ax], 0x3600
        call dos
        cmp word [rm_ax], 0xFFFF
        je .mal
        movzx eax, word [rm_ax]
        mov edi, [ebp + 4]
        mov [es:edi], eax
        movzx eax, word [rm_cx]
        mov edi, [ebp + 8]
        mov [es:edi], eax
        movzx eax, word [rm_bx]
        mov edi, [ebp + 12]
        mov [es:edi], eax
        movzx eax, word [rm_dx]
        mov edi, [ebp + 16]
        mov [es:edi], eax
        mov eax, 1
        ret
.mal:
        mov dword [ult_error], 15   ; ERROR_INVALID_DRIVE
        xor eax, eax
        ret
k_GetTempPathA:                     ; (n, buf): TEMP, TMP o la raíz de C:
        mov edi, s_temp
        call busca_env
        jnc .hay
        mov edi, s_tmp
        call busca_env
        jnc .hay
        mov dword [gs:TB_SAL], '\' * 65536 + 'C:'
        jmp .dar
.hay:
        ; el valor (ES:ESI), al búfer, con la barra del final
        xor edi, edi
.c:
        mov al, [es:esi]
        mov [gs:TB_SAL + edi], al
        inc esi
        inc edi
        test al, al
        jz .f
        cmp edi, 250
        jb .c
        mov byte [gs:TB_SAL + edi], 0
        inc edi
.f:
        dec edi
        cmp byte [gs:TB_SAL + edi - 1], '\'
        je .dar
        mov word [gs:TB_SAL + edi], '\'
.dar:
        mov esi, TB_SAL
        mov edi, [ebp + 4]
        mov ecx, [ebp]
        jmp tb_a_prog
s_temp  db 'TEMP', 0
s_tmp   db 'TMP', 0
k_GetTempFileNameA:                 ; (dir, prefijo, número, *nombre)
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        ; dir + \ + 3 del prefijo + 4 cifras + .TMP
        mov edi, TB_NOM1
.f:
        cmp byte [gs:edi], 0
        je .f1
        inc edi
        jmp .f
.f1:
        cmp edi, TB_NOM1
        je .sb
        cmp byte [gs:edi - 1], '\'
        je .sb
        mov byte [gs:edi], '\'
        inc edi
.sb:
        mov esi, [ebp + 4]
        mov ecx, 3
.p:
        mov al, [es:esi]
        test al, al
        jz .p_f
        mov [gs:edi], al
        inc esi
        inc edi
        loop .p
.p_f:
        mov [pos_cifras], edi
        mov eax, [ebp + 8]
        and eax, 0xFFFF
        mov [num_temp], eax
        test eax, eax
        jnz .uno
        ; sin número: se busca uno que no exista (y se crea)
        call k_GetTickCount
        and eax, 0xFFFF
        jnz .n0
        inc eax
.n0:
        mov [num_temp], eax
.prueba:
        call pon_cifras_temp
        mov word [rm_cx], 0
        mov word [rm_dx], TB_NOM1
        mov word [rm_ax], 0x5B00
        call dos
        jnc .creado
        cmp word [rm_ax], 80        ; ya existe: el siguiente
        jne .mal
        inc word [num_temp]
        jnz .prueba
        inc word [num_temp]
        jmp .prueba
.creado:
        mov ax, [rm_ax]
        call cierra_ax
        jmp .dar
.uno:
        call pon_cifras_temp
.dar:
        mov esi, TB_NOM1
        mov edi, [ebp + 12]
        mov ecx, 260
        call tb_a_prog
        mov eax, [num_temp]
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
pon_cifras_temp:
        mov edi, [pos_cifras]
        mov eax, [num_temp]
        mov ecx, 4
.c:
        rol ax, 4
        mov bl, al
        and bl, 15
        add bl, '0'
        cmp bl, '9'
        jbe .c1
        add bl, 7
.c1:
        mov [gs:edi], bl
        inc edi
        loop .c
        mov dword [gs:edi], '.TMP'
        mov byte [gs:edi + 4], 0
        ret
; ---- SearchPathA (ruta, nombre, extensión, n, buf, *parte): busca el archivo
; en la lista de carpetas «ruta» (con ;) o, si es NULL, en la del programa, en
; la de ahora y en las del PATH ----
k_SearchPathA:
        ; el nombre (y la extensión si no la lleva) a b_sp_nom
        mov esi, [ebp + 4]
        mov edi, b_sp_nom
        xor ebx, ebx                ; BL: ¿punto en la última parte? BH: ¿con carpeta?
        mov ecx, 200
.n:
        mov al, [es:esi]
        test al, al
        jz .n_f
        mov [edi], al
        cmp al, '.'
        jne .n1
        mov bl, 1
.n1:
        cmp al, '\'
        je .n_sep
        cmp al, ':'
        jne .n2
.n_sep:
        mov bh, 1
        xor bl, bl
.n2:
        inc esi
        inc edi
        loop .n
.n_f:
        mov byte [edi], 0
        test bl, bl
        jnz .con_ext
        mov esi, [ebp + 8]
        test esi, esi
        jz .con_ext
.e:
        mov al, [es:esi]
        mov [edi], al
        inc esi
        inc edi
        test al, al
        jnz .e
.con_ext:
        ; con carpeta en el nombre: solo ese
        test bh, bh
        jz .lista
        mov byte [b_sp_dir], 0
        call sp_prueba
        jnc .hallado
        jmp .no
.lista:
        mov esi, [ebp]
        test esi, esi
        jz .por_defecto
        call sp_lista               ; ES:ESI la lista
        jnc .hallado
        jmp .no
.por_defecto:
        mov esi, exe_dir
        mov edi, b_sp_dir
.d:
        lodsb
        mov [edi], al
        inc edi
        test al, al
        jnz .d
        call sp_prueba
        jnc .hallado
        mov byte [b_sp_dir], 0
        call sp_prueba
        jnc .hallado
        mov edi, s_path_env
        call busca_env
        jc .no
        call sp_lista
        jnc .hallado
.no:
        mov dword [ult_error], 2
        xor eax, eax
        ret
.hallado:
        mov esi, TB_SAL
        mov edi, [ebp + 16]
        mov ecx, [ebp + 12]
        call tb_a_prog
        cmp eax, [ebp + 12]
        jae .x
        mov ecx, [ebp + 20]
        test ecx, ecx
        jz .x
        mov edx, edi
.p:
        mov bl, [es:edx]
        test bl, bl
        jz .x
        inc edx
        cmp bl, '\'
        jne .p
        mov [es:ecx], edx
        jmp .p
.x:
        ret
s_path_env  db 'PATH', 0
; prueba cada carpeta de la lista ES:ESI (con ;): CF=0 si está (TB_SAL)
sp_lista:
.t:
        cmp byte [es:esi], 0
        je .no
        mov edi, b_sp_dir
.c:
        mov al, [es:esi]
        test al, al
        jz .c_f
        inc esi
        cmp al, ';'
        je .c_f
        mov [edi], al
        inc edi
        cmp edi, b_sp_dir + 120
        jb .c
.c_f:
        mov byte [edi], 0
        cmp edi, b_sp_dir
        je .t
        push esi
        call sp_prueba
        pop esi
        jnc .x
        jmp .t
.no:
        stc
.x:
        ret
; b_sp_dir + \ + b_sp_nom: ¿existe (y no es carpeta)? CF=0 y su nombre
; completo en TB_SAL
sp_prueba:
        mov esi, b_sp_dir
        mov edi, TB_NOM1
.d:
        lodsb
        test al, al
        jz .d_f
        mov [gs:edi], al
        inc edi
        jmp .d
.d_f:
        cmp edi, TB_NOM1
        je .nom
        mov al, [gs:edi - 1]
        cmp al, '\'
        je .nom
        cmp al, ':'
        je .nom
        mov byte [gs:edi], '\'
        inc edi
.nom:
        mov esi, b_sp_nom
.n:
        lodsb
        mov [gs:edi], al
        inc edi
        test al, al
        jnz .n
        mov word [rm_dx], TB_NOM1
        mov word [rm_ax], 0x4300
        call dos
        jc .no
        test byte [rm_cx], 0x10
        jnz .no
        mov dword [rm_esi], TB_NOM1
        mov dword [rm_edi], TB_SAL
        mov word [rm_ax], 0x6000
        call dos
        ret
.no:
        stc
        ret

k_GetWindowsDirectoryA:             ; (buf, n)
k_GetSystemDirectoryA:
        mov esi, s_dir_win
        xor edi, edi
.c:
        lodsb
        mov [gs:TB_SAL + edi], al
        inc edi
        test al, al
        jnz .c
        mov esi, TB_SAL
        mov edi, [ebp]
        mov ecx, [ebp + 4]
        jmp tb_a_prog
s_dir_win   db 'C:\HISPADOS', 0

; ---- buscar archivos (4Eh/4Fh con un DTA para cada búsqueda) ----
k_FindFirstFileA:                   ; (patrón, *datos)
        ; un hueco
        xor ebx, ebx
.h:
        cmp byte [busca_usada + ebx], 0
        je .h_si
        inc ebx
        cmp ebx, N_BUSCA
        jb .h
        mov dword [ult_error], 4    ; ERROR_TOO_MANY_OPEN_FILES
        mov eax, -1
        ret
.h_si:
        mov [busca_act], ebx
        mov esi, [ebp]
        mov edi, TB_NOM1
        call nom_a_tb
        ; «*» solo, al final, es «*.*» (como en Windows)
        mov edi, TB_NOM1
        xor edx, edx                ; ¿hay punto tras la última barra?
.f:
        mov al, [gs:edi]
        test al, al
        jz .f_x
        cmp al, '\'
        jne .f1
        xor edx, edx
.f1:
        cmp al, '.'
        jne .f2
        inc edx
.f2:
        inc edi
        jmp .f
.f_x:
        test edx, edx
        jnz .dta
        cmp edi, TB_NOM1
        je .dta
        cmp byte [gs:edi - 1], '*'
        jne .dta
        mov dword [gs:edi], '.*'
.dta:
        call pon_dta
        mov word [rm_cx], 0x37
        mov word [rm_dx], TB_NOM1
        mov word [rm_ax], 0x4E00
        call dos
        jc .mal
        mov ebx, [busca_act]
        mov byte [busca_usada + ebx], 1
        mov edi, [ebp + 4]
        call llena_busqueda
        mov eax, [busca_act]
        add eax, H_BUSCA
        ret
.mal:
        call error_dos
        cmp dword [ult_error], 18   ; (no hay ninguno: «no se encuentra»)
        jne .m1
        mov dword [ult_error], 2
.m1:
        mov eax, -1
        ret
k_FindNextFileA:                    ; (h, *datos)
        mov eax, [ebp]
        sub eax, H_BUSCA
        cmp eax, N_BUSCA
        jae .mal_h
        cmp byte [busca_usada + eax], 0
        je .mal_h
        mov [busca_act], eax
        call pon_dta
        mov word [rm_ax], 0x4F00
        call dos
        jc .mal
        mov edi, [ebp + 4]
        call llena_busqueda
        mov eax, 1
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
.mal_h:
        mov dword [ult_error], 6
        xor eax, eax
        ret
k_FindClose:                        ; (h)
        mov eax, [ebp]
        sub eax, H_BUSCA
        cmp eax, N_BUSCA
        jae .mal
        mov byte [busca_usada + eax], 0
        mov eax, 1
        ret
.mal:
        mov dword [ult_error], 6
        xor eax, eax
        ret
; el DTA de DOS: el de la búsqueda [busca_act]
pon_dta:
        mov eax, [busca_act]
        shl eax, 6
        add eax, TB_DTA
        mov [dta_act], eax
        mov [rm_dx], ax
        mov word [rm_ax], 0x1A00
        jmp dos
; WIN32_FIND_DATAA (320) en ES:EDI desde el DTA [dta_act]
llena_busqueda:
        mov ecx, 320
        call a_cero
        mov esi, [dta_act]
        movzx eax, byte [gs:esi + 0x15]
        and eax, 0x37
        jnz .a
        mov eax, 0x80
.a:
        mov [es:edi], eax           ; dwFileAttributes
        mov ax, [gs:esi + 0x18]     ; la fecha
        mov dx, [gs:esi + 0x16]     ; la hora
        push edi
        call ft_de_dos              ; -> EDX:EAX
        pop edi
        mov [es:edi + 4], eax       ; creación, último acceso y escritura
        mov [es:edi + 8], edx
        mov [es:edi + 12], eax
        mov [es:edi + 16], edx
        mov [es:edi + 20], eax
        mov [es:edi + 24], edx
        mov eax, [gs:esi + 0x1A]
        mov [es:edi + 32], eax      ; nFileSizeLow
        ; el nombre
        lea edx, [edi + 44]
        add esi, 0x1E
        mov ecx, 13
.n:
        mov al, [gs:esi]
        mov [es:edx], al
        test al, al
        jz .x
        inc esi
        inc edx
        loop .n
.x:
        ret

; =============================================================================
;  La hora y la fecha. FILETIME = décimas de microsegundo desde el 1-1-1601
;  (64 bits); se pasa por «partes» (p_anio, p_mes...). Sin husos horarios: la
;  hora del sistema es la de DOS.
; =============================================================================
; ¿año EAX bisiesto? ZF=1 si lo es
bisiesto:
        push ebx
        push eax
        push ecx
        push edx
        mov ecx, eax
        xor edx, edx
        mov ebx, 400
        div ebx
        test edx, edx
        jz .si
        mov eax, ecx
        xor edx, edx
        mov ebx, 100
        div ebx
        test edx, edx
        jz .no
        test ecx, 3
        jmp .x                      ; (ZF según los 4)
.si:
        cmp eax, eax
        jmp .x
.no:
        or ecx, 1                   ; (ZF=0)
        test ecx, ecx
.x:
        pop edx
        pop ecx
        pop eax
        pop ebx
        ret
; los días de las partes desde el 1-1-1601 -> EAX
dias_de_partes:
        push ebx
        push ecx
        push edx
        xor ecx, ecx                ; los días
        mov eax, 1601
.a:
        cmp eax, [p_anio]
        jae .a_f
        add ecx, 365
        call bisiesto
        jnz .a1
        inc ecx
.a1:
        inc eax
        jmp .a
.a_f:
        mov edx, [p_mes]
        dec edx
        cmp edx, 11
        jbe .m
        xor edx, edx
.m:
        movzx ebx, word [dias_antes + edx * 2]
        add ecx, ebx
        cmp edx, 2                  ; marzo o después en año bisiesto: +1
        jb .m1
        mov eax, [p_anio]
        call bisiesto
        jnz .m1
        inc ecx
.m1:
        mov eax, [p_dia]
        dec eax
        add eax, ecx
        pop edx
        pop ecx
        pop ebx
        ret
dias_antes  dw 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
dias_mes    db 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
; las partes -> FILETIME EDX:EAX
ft_de_partes:
        push ebx
        push ecx
        push esi
        call dias_de_partes
        mov ecx, 86400
        mul ecx                     ; EDX:EAX segundos de los días
        mov ebx, [p_hora]
        imul ebx, 3600
        mov ecx, [p_min]
        imul ecx, 60
        add ebx, ecx
        add ebx, [p_seg]
        add eax, ebx
        adc edx, 0
        ; x 10.000.000
        mov ecx, edx
        mov esi, 10000000
        mul esi
        imul ecx, esi
        add edx, ecx
        mov ebx, [p_ms]
        imul ebx, 10000
        add eax, ebx
        adc edx, 0
        pop esi
        pop ecx
        pop ebx
        ret
; FILETIME EDX:EAX -> las partes
partes_de_ft:
        push ebx
        push ecx
        push esi
        mov esi, 10000000
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        div esi
        mov ebx, eax                ; los segundos (alta)
        mov eax, ecx
        div esi                     ; EAX segundos (baja), EDX el resto
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        mov esi, 10000
        div esi
        mov [p_ms], eax
        mov eax, ecx
        mov edx, ebx
        mov esi, 86400
        div esi                     ; EAX los días, EDX los segundos del día
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        mov esi, 3600
        div esi
        mov [p_hora], eax
        mov eax, edx
        xor edx, edx
        mov esi, 60
        div esi
        mov [p_min], eax
        mov [p_seg], edx
        ; el día de la semana: el 1-1-1601 fue lunes
        lea eax, [ecx + 1]
        xor edx, edx
        mov esi, 7
        div esi
        mov [p_sem], edx
        ; el año
        mov eax, 1601
.a:
        mov edx, 365
        call bisiesto
        jnz .a1
        inc edx
.a1:
        cmp ecx, edx
        jb .a_f
        sub ecx, edx
        inc eax
        jmp .a
.a_f:
        mov [p_anio], eax
        ; el mes
        xor esi, esi
.m:
        movzx edx, byte [dias_mes + esi]
        cmp esi, 1
        jne .m1
        call bisiesto
        jnz .m1
        inc edx
.m1:
        cmp ecx, edx
        jb .m_f
        sub ecx, edx
        inc esi
        jmp .m
.m_f:
        inc esi
        mov [p_mes], esi
        inc ecx
        mov [p_dia], ecx
        pop esi
        pop ecx
        pop ebx
        ret
; SYSTEMTIME ES:ESI -> partes
partes_de_st:
        movzx eax, word [es:esi]
        mov [p_anio], eax
        movzx eax, word [es:esi + 2]
        mov [p_mes], eax
        movzx eax, word [es:esi + 6]
        mov [p_dia], eax
        movzx eax, word [es:esi + 8]
        mov [p_hora], eax
        movzx eax, word [es:esi + 10]
        mov [p_min], eax
        movzx eax, word [es:esi + 12]
        mov [p_seg], eax
        movzx eax, word [es:esi + 14]
        mov [p_ms], eax
        ret
; partes -> SYSTEMTIME ES:EDI
st_de_partes:
        mov eax, [p_anio]
        mov [es:edi], ax
        mov eax, [p_mes]
        mov [es:edi + 2], ax
        mov eax, [p_sem]
        mov [es:edi + 4], ax
        mov eax, [p_dia]
        mov [es:edi + 6], ax
        mov eax, [p_hora]
        mov [es:edi + 8], ax
        mov eax, [p_min]
        mov [es:edi + 10], ax
        mov eax, [p_seg]
        mov [es:edi + 12], ax
        mov eax, [p_ms]
        mov [es:edi + 14], ax
        ret
; la hora de DOS -> partes
partes_de_ahora:
        mov word [rm_ax], 0x2A00
        call dos
        movzx eax, word [rm_cx]
        mov [p_anio], eax
        movzx eax, byte [rm_dx + 1]
        mov [p_mes], eax
        movzx eax, byte [rm_dx]
        mov [p_dia], eax
        movzx eax, byte [rm_ax]
        mov [p_sem], eax
        mov word [rm_ax], 0x2C00
        call dos
        movzx eax, byte [rm_cx + 1]
        mov [p_hora], eax
        movzx eax, byte [rm_cx]
        mov [p_min], eax
        movzx eax, byte [rm_dx + 1]
        mov [p_seg], eax
        movzx eax, byte [rm_dx]
        imul eax, 10
        mov [p_ms], eax
        ret
; fecha (AX) y hora (DX) de DOS -> FILETIME EDX:EAX
ft_de_dos:
        movzx ecx, ax
        shr ecx, 9
        add ecx, 1980
        mov [p_anio], ecx
        movzx ecx, ax
        shr ecx, 5
        and ecx, 15
        mov [p_mes], ecx
        movzx ecx, ax
        and ecx, 31
        mov [p_dia], ecx
        movzx ecx, dx
        shr ecx, 11
        mov [p_hora], ecx
        movzx ecx, dx
        shr ecx, 5
        and ecx, 63
        mov [p_min], ecx
        movzx ecx, dx
        and ecx, 31
        shl ecx, 1
        mov [p_seg], ecx
        mov dword [p_ms], 0
        jmp ft_de_partes
k_GetLocalTime:                     ; (*SYSTEMTIME)
k_GetSystemTime:
        call partes_de_ahora
        mov edi, [ebp]
        jmp st_de_partes
k_GetSystemTimeAsFileTime:          ; (*FILETIME)
        call partes_de_ahora
        call ft_de_partes
        mov edi, [ebp]
        mov [es:edi], eax
        mov [es:edi + 4], edx
        ret
k_SystemTimeToFileTime:             ; (*st, *ft)
        mov esi, [ebp]
        call partes_de_st
        call ft_de_partes
        mov edi, [ebp + 4]
        mov [es:edi], eax
        mov [es:edi + 4], edx
        mov eax, 1
        ret
k_FileTimeToSystemTime:             ; (*ft, *st)
        mov esi, [ebp]
        mov eax, [es:esi]
        mov edx, [es:esi + 4]
        call partes_de_ft
        mov edi, [ebp + 4]
        call st_de_partes
        mov eax, 1
        ret
k_FileTimeToLocalFileTime:          ; (*ft, *ft_local): (sin husos) lo mismo
k_LocalFileTimeToFileTime:
        mov esi, [ebp]
        mov edi, [ebp + 4]
        mov eax, [es:esi]
        mov [es:edi], eax
        mov eax, [es:esi + 4]
        mov [es:edi + 4], eax
        mov eax, 1
        ret
k_CompareFileTime:                  ; (*a, *b) -> -1, 0, 1
        mov esi, [ebp]
        mov edi, [ebp + 4]
        mov eax, [es:esi + 4]
        cmp eax, [es:edi + 4]
        jne .d
        mov eax, [es:esi]
        cmp eax, [es:edi]
        jne .d
        xor eax, eax
        ret
.d:
        mov eax, 1
        ja .x
        mov eax, -1
.x:
        ret
k_FileTimeToDosDateTime:            ; (*ft, *fecha, *hora)
        mov esi, [ebp]
        mov eax, [es:esi]
        mov edx, [es:esi + 4]
        call partes_de_ft
        mov eax, [p_anio]
        sub eax, 1980
        shl eax, 4
        or eax, [p_mes]
        shl eax, 5
        or eax, [p_dia]
        mov edi, [ebp + 4]
        mov [es:edi], ax
        mov eax, [p_hora]
        shl eax, 6
        or eax, [p_min]
        shl eax, 5
        mov ecx, [p_seg]
        shr ecx, 1
        or eax, ecx
        mov edi, [ebp + 8]
        mov [es:edi], ax
        mov eax, 1
        ret
k_DosDateTimeToFileTime:            ; (fecha, hora, *ft)
        mov ax, [ebp]
        mov dx, [ebp + 4]
        call ft_de_dos
        mov edi, [ebp + 8]
        mov [es:edi], eax
        mov [es:edi + 4], edx
        mov eax, 1
        ret
k_GetTimeZoneInformation:           ; (*tzi): sin huso
        mov edi, [ebp]
        mov ecx, 172
        call a_cero
        xor eax, eax                ; TIME_ZONE_ID_UNKNOWN
        ret
k_GetTickCount:                     ; los ms del día (de la hora de DOS)
        mov word [rm_ax], 0x2C00
        call dos
        movzx eax, byte [rm_cx + 1]
        imul eax, 60
        movzx ebx, byte [rm_cx]
        add eax, ebx
        imul eax, 60
        movzx ebx, byte [rm_dx + 1]
        add eax, ebx
        imul eax, 1000
        movzx ebx, byte [rm_dx]
        imul ebx, 10
        add eax, ebx
        ret
k_QueryPerformanceFrequency:        ; (*li): 1000 por segundo
        mov edi, [ebp]
        mov dword [es:edi], 1000
        mov dword [es:edi + 4], 0
        mov eax, 1
        ret
k_QueryPerformanceCounter:          ; (*li)
        call k_GetTickCount
        mov edi, [ebp]
        mov [es:edi], eax
        mov dword [es:edi + 4], 0
        mov eax, 1
        ret
k_Sleep:                            ; (ms): con los tics de la BIOS (55 ms)
        mov eax, [ebp]
        xor edx, edx
        mov ecx, 55
        div ecx
        mov ecx, eax
        jecxz .x
        push es
        mov es, [sel_bios]
.t:
        mov ebx, [es:0x6C]
.e:
        cmp ebx, [es:0x6C]
        je .e
        loop .t
        pop es
.x:
        ret

; =============================================================================
;  La memoria. El montón reparte bloques de 32 octetos a 1 MB (potencias de
;  dos, con 16 de cabecera: clase, tamaño pedido, handle DPMI, enlace) y guarda
;  los que se sueltan en una lista por clase. Lo de más de 1 MB va en su
;  propio bloque de DPMI.
; =============================================================================
CAB_M       equ 16
CLASE_MIN   equ 5
CLASE_MAX   equ 20
CLASE_GRANDE equ 0xFF
; reserva ECX octetos (a cero) -> EAX la dirección (en el programa) o 0
reserva:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov [m_pedido], ecx
        lea eax, [ecx + CAB_M]
        cmp eax, ecx
        jb .no                      ; (desborda)
        cmp eax, 1 << CLASE_MAX
        ja .grande
        ; la clase: la potencia de dos que cabe
        mov edx, CLASE_MIN
.cl:
        mov ebx, 1
        mov cl, dl
        shl ebx, cl
        cmp eax, ebx
        jbe .cl_f
        inc edx
        jmp .cl
.cl_f:
        ; ¿hay uno suelto de esa clase?
        mov eax, [m_libres + edx * 4]
        test eax, eax
        jz .nuevo
        mov ecx, [es:eax + 12]
        mov [m_libres + edx * 4], ecx
        jmp .listo
.nuevo:
        ; del trozo de ahora; si no cabe, uno nuevo de 1 MB
        cmp [monton_libre], ebx
        jae .del_trozo
        push ebx
        mov eax, 1 << CLASE_MAX
        call pide_bloque
        mov eax, ebx
        pop ebx
        jc .no
        sub eax, [prog_base]
        mov [monton_ptr], eax
        mov dword [monton_libre], 1 << CLASE_MAX
.del_trozo:
        mov eax, [monton_ptr]
        add [monton_ptr], ebx
        sub [monton_libre], ebx
.listo:
        mov [es:eax], edx           ; la clase
        mov ecx, [m_pedido]
        mov [es:eax + 4], ecx
        mov dword [es:eax + 8], 0
        mov dword [es:eax + 12], 0
        ; a cero lo de dentro (todo lo que cabe)
        mov ecx, ebx
        sub ecx, CAB_M
        lea edi, [eax + CAB_M]
        push eax
        shr ecx, 2
        xor eax, eax
        rep stosd
        pop eax
        add eax, CAB_M
        jmp .x
.grande:
        add eax, 0xFFF
        and eax, ~0xFFF
        mov esi, eax                ; (su tamaño)
        call pide_bloque
        jc .no
        mov eax, ebx
        sub eax, [prog_base]
        mov dword [es:eax], CLASE_GRANDE
        mov ecx, [m_pedido]
        mov [es:eax + 4], ecx
        mov ecx, [ult_handle]
        mov [es:eax + 8], ecx
        mov [es:eax + 12], esi
        lea edi, [eax + CAB_M]
        mov ecx, esi
        sub ecx, CAB_M
        shr ecx, 2
        push eax
        xor eax, eax
        rep stosd
        pop eax
        add eax, CAB_M
        jmp .x
.no:
        mov dword [ult_error], 8    ; ERROR_NOT_ENOUGH_MEMORY
        xor eax, eax
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; suelta el bloque EAX (del montón)
suelta:
        test eax, eax
        jz .x
        push ebx
        push ecx
        push esi
        push edi
        sub eax, CAB_M
        mov ecx, [es:eax]
        cmp ecx, CLASE_GRANDE
        je .grande
        cmp ecx, CLASE_MAX
        ja .y                       ; (no es nuestro: se deja)
        cmp ecx, CLASE_MIN
        jb .y
        mov ebx, [m_libres + ecx * 4]
        mov [es:eax + 12], ebx
        mov [m_libres + ecx * 4], eax
        jmp .y
.grande:
        mov dword [es:eax], 0
        mov esi, [es:eax + 8]
        call suelta_dpmi
.y:
        pop edi
        pop esi
        pop ecx
        pop ebx
.x:
        ret
; lo que cabe en el bloque EAX (sin la cabecera) -> ECX
cabe_en:
        mov ecx, [es:eax - CAB_M]
        cmp ecx, CLASE_GRANDE
        je .g
        push eax
        mov eax, 1
        shl eax, cl
        mov ecx, eax
        pop eax
        sub ecx, CAB_M
        ret
.g:
        mov ecx, [es:eax - CAB_M + 12]
        sub ecx, CAB_M
        ret
; cambia el tamaño del bloque EAX a ECX -> EAX el nuevo (o 0); BL=1: sin
; moverlo (o falla)
cambia_tam:
        test eax, eax
        jnz .hay
        jmp reserva
.hay:
        push ecx
        push eax
        call cabe_en                ; ECX = lo que cabe
        pop eax
        pop edx                     ; EDX = el tamaño nuevo
        cmp edx, ecx
        ja .mover
        ; cabe: si crece, a cero lo nuevo (de lo pedido antes a lo de ahora)
        mov ecx, [es:eax - CAB_M + 4]
        mov [es:eax - CAB_M + 4], edx
        cmp edx, ecx
        jbe .x
        push eax
        lea edi, [eax + ecx]
        sub edx, ecx
        mov ecx, edx
        xor eax, eax
        rep stosb
        pop eax
.x:
        ret
.mover:
        test bl, bl
        jz .m
        mov dword [ult_error], 8
        xor eax, eax
        ret
.m:
        mov esi, eax
        mov ecx, edx
        call reserva
        test eax, eax
        jz .x
        mov ecx, [es:esi - CAB_M + 4]   ; lo que había
        push eax
        push esi
        mov edi, eax
        push ds
        mov dx, es
        mov ds, dx
        rep movsb
        pop ds
        pop eax                     ; el viejo
        call suelta
        pop eax
        ret

k_GetProcessHeap:
k_HeapCreate:                       ; (opciones, inicial, máximo)
        mov eax, 0x00480000         ; (un solo montón para todo)
        ret
k_HeapDestroy:
k_HeapValidate:
k_HeapLock:
k_HeapUnlock:
        mov eax, 1
        ret
k_HeapCompact:
        mov eax, 1 << CLASE_MAX
        ret
k_HeapAlloc:                        ; (montón, opciones, tamaño)
        mov ecx, [ebp + 8]
        jmp reserva
k_HeapFree:                         ; (montón, opciones, dir)
        mov eax, [ebp + 8]
        call suelta
        mov eax, 1
        ret
k_HeapSize:                         ; (montón, opciones, dir)
        mov eax, [ebp + 8]
        mov eax, [es:eax - CAB_M + 4]
        ret
k_HeapReAlloc:                      ; (montón, opciones, dir, tamaño)
        mov eax, [ebp + 8]
        mov ecx, [ebp + 12]
        xor ebx, ebx
        test byte [ebp + 4], 0x10   ; HEAP_REALLOC_IN_PLACE_ONLY
        jz .c
        inc ebx
.c:
        jmp cambia_tam
k_LocalAlloc:                       ; (opciones, tamaño)
k_GlobalAlloc:
        mov ecx, [ebp + 4]
        jmp reserva
k_LocalFree:                        ; (dir) -> 0
k_GlobalFree:
        mov eax, [ebp]
        call suelta
        xor eax, eax
        ret
k_LocalLock:                        ; (dir) -> la misma
k_GlobalLock:
k_GlobalHandle:
        mov eax, [ebp]
        ret
k_LocalUnlock:
k_GlobalUnlock:
        mov eax, 1
        ret
k_LocalSize:                        ; (dir)
k_GlobalSize:
        mov eax, [ebp]
        test eax, eax
        jz .x
        mov eax, [es:eax - CAB_M + 4]
.x:
        ret
k_LocalReAlloc:                     ; (dir, tamaño, opciones)
k_GlobalReAlloc:
        mov eax, [ebp]
        mov ecx, [ebp + 4]
        xor ebx, ebx
        jmp cambia_tam
; ---- VirtualAlloc: cada reserva, su bloque de DPMI (en una tabla) ----
N_VIRT      equ 64
k_VirtualAlloc:                     ; (dirección, tamaño, tipo, protección)
        mov eax, [ebp]
        test eax, eax
        jz .nuevo
        ; ¿dentro de uno nuestro? (comprometer lo reservado: ya lo está)
        xor ecx, ecx
.b:
        mov edx, [virt_dir + ecx * 4]
        test edx, edx
        jz .b_s
        cmp eax, edx
        jb .b_s
        add edx, [virt_tam + ecx * 4]
        cmp eax, edx
        jae .b_s
        and eax, ~0xFFF
        ret
.b_s:
        inc ecx
        cmp ecx, N_VIRT
        jb .b
        mov dword [ult_error], 487  ; ERROR_INVALID_ADDRESS
        xor eax, eax
        ret
.nuevo:
        ; un hueco en la tabla
        xor ecx, ecx
.h:
        cmp dword [virt_dir + ecx * 4], 0
        je .h_si
        inc ecx
        cmp ecx, N_VIRT
        jb .h
        jmp .no
.h_si:
        mov eax, [ebp + 4]
        add eax, 0xFFF
        and eax, ~0xFFF
        jz .no
        mov esi, eax
        push ecx
        call pide_bloque
        pop ecx
        jc .no
        sub ebx, [prog_base]
        mov [virt_dir + ecx * 4], ebx
        mov [virt_tam + ecx * 4], esi
        mov eax, [ult_handle]
        mov [virt_h + ecx * 4], eax
        mov edi, ebx
        mov ecx, esi
        shr ecx, 2
        xor eax, eax
        rep stosd
        mov eax, ebx
        ret
.no:
        mov dword [ult_error], 8
        xor eax, eax
        ret
k_VirtualFree:                      ; (dirección, tamaño, tipo)
        test dword [ebp + 8], 0x8000    ; MEM_RELEASE (si no, «descomprometer»: nada)
        jz .si
        mov eax, [ebp]
        xor ecx, ecx
.b:
        cmp [virt_dir + ecx * 4], eax
        je .es
        inc ecx
        cmp ecx, N_VIRT
        jb .b
        mov dword [ult_error], 487
        xor eax, eax
        ret
.es:
        mov dword [virt_dir + ecx * 4], 0
        mov esi, [virt_h + ecx * 4]
        call suelta_dpmi
.si:
        mov eax, 1
        ret
k_VirtualQuery:                     ; (dirección, *info, tamaño): lo justo
        mov edi, [ebp + 4]
        mov ecx, 28
        call a_cero
        mov eax, [ebp]
        and eax, ~0xFFF
        mov [es:edi], eax           ; BaseAddress
        mov [es:edi + 4], eax       ; AllocationBase
        mov dword [es:edi + 8], 4   ; PAGE_READWRITE
        mov dword [es:edi + 12], 0x1000
        mov dword [es:edi + 16], 0x1000 ; MEM_COMMIT
        mov dword [es:edi + 20], 4
        mov dword [es:edi + 24], 0x20000 ; MEM_PRIVATE
        mov eax, 28
        ret
k_VirtualProtect:                   ; (dir, tamaño, nueva, *vieja)
        mov edi, [ebp + 12]
        test edi, edi
        jz .x
        mov dword [es:edi], 4
.x:
        mov eax, 1
        ret
k_GlobalMemoryStatus:               ; (*MEMORYSTATUS): con DPMI 0500h
        push es
        push ds
        pop es
        mov edi, b_mem
        mov ax, 0x0500
        int 0x31
        pop es
        mov edi, [ebp]
        mov ecx, 32
        call a_cero
        mov dword [es:edi], 32
        mov eax, [b_mem]            ; lo libre (el bloque mayor)
        mov ebx, [b_mem + 0x18]     ; las páginas físicas
        cmp ebx, -1
        je .t
        shl ebx, 12
        cmp ebx, eax
        jae .t1
.t:
        mov ebx, eax
.t1:
        mov [es:edi + 8], ebx       ; dwTotalPhys
        mov [es:edi + 12], eax      ; dwAvailPhys
        mov [es:edi + 16], ebx      ; dwTotalPageFile
        mov [es:edi + 20], eax
        mov dword [es:edi + 24], 0x7FFE0000 ; dwTotalVirtual
        mov [es:edi + 28], eax
        ; la carga: 100 - libre*100/total
        push edx
        mov ecx, ebx
        shr ecx, 10
        jz .c
        shr eax, 10
        imul eax, 100
        xor edx, edx
        div ecx
        mov ecx, 100
        sub ecx, eax
        mov [es:edi + 4], ecx
.c:
        pop edx
        ret
k_GetSystemInfo:                    ; (*SYSTEM_INFO)
        mov edi, [ebp]
        mov ecx, 36
        call a_cero
        mov dword [es:edi + 4], 0x1000      ; dwPageSize
        mov dword [es:edi + 8], 0x10000     ; lo mínimo
        mov dword [es:edi + 12], 0x7FFEFFFF ; lo máximo
        mov dword [es:edi + 16], 1          ; los procesadores
        mov dword [es:edi + 20], 1
        mov dword [es:edi + 24], 486        ; dwProcessorType
        mov dword [es:edi + 28], 0x10000    ; dwAllocationGranularity
        mov word [es:edi + 32], 4           ; wProcessorLevel
        ret
k_IsBadReadPtr:                     ; (dir, n): todo es bueno
k_IsBadWritePtr:
        xor eax, eax
        ret
k_IsBadCodePtr:
k_IsBadStringPtrA:
        xor eax, eax
        ret

; =============================================================================
;  Proceso, hilos, módulos y versión
; =============================================================================
k_ExitProcess:                      ; (código)
k_ExitThread:
        mov al, [ebp]
        jmp acaba
k_TerminateProcess:                 ; (proceso, código)
        mov al, [ebp + 4]
        jmp acaba
k_FatalAppExitA:                    ; (0, texto)
        cmp byte [md_modulo], 0
        je .dos
        mov eax, [ebp + 4]          ; (módulo) lo dice Hispa Ventanas
        mov ecx, 2
        jmp aviso_y_fin
.dos:
        mov esi, [ebp + 4]
        call pon_cad_prog
        mov esi, m_nl2
        call pon_cad
        mov al, 0xFF
        jmp acaba
k_RaiseException:                   ; (código, opciones, n, args)
        cmp byte [md_modulo], 0
        je .dos
        mov eax, [ebp]              ; (módulo) el texto, entero, a Hispa Ventanas
        mov edi, m_exc_cod
        mov ecx, 8
.c:
        rol eax, 4
        mov bl, al
        and bl, 15
        add bl, '0'
        cmp bl, '9'
        jbe .c1
        add bl, 7
.c1:
        mov [edi], bl
        inc edi
        loop .c
        mov byte [m_exc_cod - 1], ' '
        mov eax, m_excepcion
        mov ecx, 1
        jmp aviso_y_fin
.dos:
        mov esi, m_excepcion
        call pon_cad
        mov esi, m_esp
        call pon_cad
        mov eax, [ebp]
        call pon_hex8
        mov esi, m_nl2
        call pon_cad
        mov al, 0xFF
        jmp acaba
; escribe el texto del programa ES:ESI
pon_cad_prog:
        xor edi, edi
.c:
        mov al, [es:esi]
        test al, al
        jz .f
        mov [gs:edi], al
        inc esi
        inc edi
        cmp edi, TB_TROZO - 1
        jb .c
.f:
        mov ecx, edi
        jmp escribe_tb
; EAX en hexadecimal (8 cifras) por la salida
pon_hex8:
        mov edi, b_hex8
        mov ecx, 8
.c:
        rol eax, 4
        mov bl, al
        and bl, 15
        add bl, '0'
        cmp bl, '9'
        jbe .c1
        add bl, 7
.c1:
        mov [edi], bl
        inc edi
        loop .c
        mov byte [edi], 0
        mov esi, b_hex8
        jmp pon_cad
k_GetCurrentProcess:
        mov eax, -1
        ret
k_GetCurrentThread:
        mov eax, -2
        ret
k_GetCurrentProcessId:
        movzx eax, word [segrm]
        ret
k_GetCurrentThreadId:
        movzx eax, word [segrm]
        inc eax
        ret
k_GetCommandLineA:
        mov eax, [cmd_dir]
        ret
k_GetEnvironmentStrings:            ; el entorno de DOS (copiado)
        mov eax, [env_dir]
        ret
k_FreeEnvironmentStringsA:
        mov eax, 1
        ret
; busca la variable de nombre DS:EDI en el entorno: CF=0 y ES:ESI = su valor
busca_env:
        mov esi, [env_dir]
.v:
        cmp byte [es:esi], 0
        je .no
        push esi
        push edi
.c:
        mov al, [es:esi]
        mov ah, [edi]
        test ah, ah
        jz .fin_nom
        call mayus2
        cmp al, ah
        jne .otra
        inc esi
        inc edi
        jmp .c
.fin_nom:
        cmp al, '='
        jne .otra
        pop edi
        add esp, 4
        inc esi
        clc
        ret
.otra:
        pop edi
        pop esi
.s:
        cmp byte [es:esi], 0
        je .s1
        inc esi
        jmp .s
.s1:
        inc esi
        jmp .v
.no:
        stc
        ret
k_GetEnvironmentVariableA:          ; (nombre, buf, n)
        ; el nombre del programa a lo nuestro (b_nombre)
        mov esi, [ebp]
        mov edi, b_nombre
        mov ecx, 63
.n:
        mov al, [es:esi]
        mov [edi], al
        test al, al
        jz .n_f
        inc esi
        inc edi
        loop .n
        mov byte [edi], 0
.n_f:
        mov edi, b_nombre
        call busca_env
        jc .no
        ; el valor ES:ESI: su largo
        xor eax, eax
.l:
        cmp byte [es:esi + eax], 0
        je .l_f
        inc eax
        jmp .l
.l_f:
        cmp eax, [ebp + 8]
        jb .copia
        inc eax                     ; no cabe: lo que hace falta
        ret
.copia:
        mov edi, [ebp + 4]
        push eax
        lea ecx, [eax + 1]
        push ds
        mov dx, es
        mov ds, dx
        rep movsb
        pop ds
        pop eax
        ret
.no:
        mov dword [ult_error], 203  ; ERROR_ENVVAR_NOT_FOUND
        xor eax, eax
        ret
k_SetEnvironmentVariableA:          ; (nombre, valor): no se guarda
        mov eax, 1
        ret
k_GetStartupInfoA:                  ; (*STARTUPINFO): a cero, con su tamaño
        mov edi, [ebp]
        mov ecx, 68
        call a_cero
        mov dword [es:edi], 68
        ret
k_GetModuleHandleW:                 ; (nombre ancho o NULL): a 8 bits y como A
        mov esi, [ebp]
        test esi, esi
        jz k_GetModuleHandleA.prog
        mov edi, [cmd_dir]
        add edi, 0xF00              ; (un sitio del programa para el nombre)
        xor ecx, ecx
.c:
        mov ax, [es:esi + ecx * 2]
        mov [es:edi + ecx], al
        inc ecx
        test ax, ax
        jz .f
        cmp ecx, 60
        jb .c
        mov byte [es:edi + ecx], 0
.f:
        mov [ebp], edi
k_GetModuleHandleA:                 ; (nombre o NULL)
        mov esi, [ebp]
        test esi, esi
        jnz .nom
.prog:
        mov eax, [img_base]
        ret
.nom:
        call dll_cargada            ; ¿una de las nuestras? -> EAX
        jnc .x
        jmp es_kernel32
.x:
        ret
; ¿la DLL de nombre ES:ESI ya está cargada? CF=0 y EAX = su dirección
dll_cargada:
        push esi
        mov edi, b_dll
        mov ecx, 26
        xor edx, edx                ; ¿lleva extensión?
.c:
        mov al, [es:esi]
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        cmp al, '.'
        jne .c2
        inc edx
.c2:
        mov [edi], al
        test al, al
        jz .c_f
        inc esi
        inc edi
        loop .c
        mov byte [edi], 0
.c_f:
        test edx, edx
        jnz .b
        mov dword [edi], '.DLL'
        mov byte [edi + 4], 0
.b:
        pop esi
        call ficha_de_b_dll         ; -> EDX
        jc .x
        mov eax, [edx + M_DIR]
.x:
        ret
k_IsDBCSLeadByteEx:                 ; (página, c): sin páginas de dos octetos
k_IsDBCSLeadByte:
        xor eax, eax
        ret
k_LoadLibraryA:                     ; (nombre)
k_LoadLibraryExA:                   ; (nombre, 0, opciones)
        mov esi, [ebp]
        call dll_cargada            ; (las que ya trae el programa)
        jnc .x
        jmp es_kernel32
.x:
        ret
es_kernel32:
        ; ¿KERNEL32 (con .DLL o sin él)?
        mov edi, s_kernel32
        xor ecx, ecx
.c:
        mov al, [es:esi + ecx]
        mov ah, [edi + ecx]
        call mayus2
        test ah, ah
        jz .fin
        cmp al, ah
        jne .no
        inc ecx
        jmp .c
.fin:
        test al, al
        jz .si
        cmp al, '.'
        je .si
.no:
        cmp byte [md_modulo], 0     ; (módulo) ¿una del entorno (USER32, GDI32)?
        je .no2
        mov al, 1
        call anfitrion
        test eax, eax
        jnz .x
.no2:
        mov dword [ult_error], 126  ; ERROR_MOD_NOT_FOUND
        xor eax, eax
.x:
        ret
.si:
        mov eax, H_KERNEL32
        ret
s_kernel32  db 'KERNEL32', 0
k_FreeLibrary:
        mov eax, 1
        ret
k_GetModuleFileNameA:               ; (módulo, buf, n): el nombre completo
        mov esi, nombre
        xor edi, edi
.c:
        lodsb
        mov [gs:TB_NOM1 + edi], al
        inc edi
        test al, al
        jnz .c
        mov dword [rm_esi], TB_NOM1
        mov dword [rm_edi], TB_SAL
        mov word [rm_ax], 0x6000
        call dos
        mov esi, TB_SAL
        jnc .d
        mov esi, TB_NOM1
.d:
        mov edi, [ebp + 4]
        mov ecx, [ebp + 8]
        call tb_a_prog
        cmp eax, [ebp + 8]
        jb .x
        mov eax, [ebp + 8]          ; (no cabe: se corta, como en Windows)
.x:
        ret
k_GetProcAddress:                   ; (módulo, nombre)
        cmp byte [md_modulo], 0     ; (módulo) lo del entorno, y lo que el
        je .nuestro                 ; entorno hace en vez de nosotros (Sleep)
        mov edi, [ebp + 4]
        cmp edi, 0x10000
        jb .nuestro
        mov ebx, [ebp]
        mov al, 2
        call anfitrion
        test eax, eax
        jz .nuestro
        ret
.nuestro:
        mov eax, [ebp]
        cmp eax, H_KERNEL32
        je .k32
        ; ¿una de las DLL cargadas (o el programa)?
        xor ecx, ecx
.m:
        cmp ecx, [n_mods]
        jae .no
        mov edx, ecx
        imul edx, MOD_TAM
        add edx, mods
        cmp [edx + M_DIR], eax
        je .m_si
        inc ecx
        jmp .m
.m_si:
        mov eax, [ebp + 4]
        cmp eax, 0x10000
        jb .m_num
        call busca_export
        jmp .m_x
.m_num:
        call export_numero
.m_x:
        test eax, eax
        jz .no
        ret
.k32:
        mov eax, [ebp + 4]
        cmp eax, 0x10000
        jb .no                      ; (por número: no)
        call busca_funcion          ; ES:EAX -> ECX manejador, DX args
        cmp ecx, f_falta
        je .no
        ; ¿ya tiene trampolín?
        xor ebx, ebx
.b:
        cmp ebx, [n_imp]
        jae .nuevo
        mov edi, ebx
        imul edi, IMP_FICHA
        cmp [imp_tab + edi], ecx
        je .esta
        inc ebx
        jmp .b
.esta:
        mov eax, ebx
        imul eax, TRAMP
        add eax, [tramp_off]
        add eax, [img_base]
        ret
.nuevo:
        mov eax, [ebp + 4]
        call nuevo_tramp
        jc .no
        ret
.no:
        mov dword [ult_error], 127  ; ERROR_PROC_NOT_FOUND
        xor eax, eax
        ret
k_GetVersion:                       ; Windows 98: 4.10, build 1998
        mov eax, 0xC0000A04
        ret
k_GetVersionExA:                    ; (*OSVERSIONINFOA)
        mov edi, [ebp]
        mov ecx, [es:edi]
        cmp ecx, 148
        jb .mal
        push ecx
        add edi, 4
        sub ecx, 4
        call a_cero
        pop ecx
        sub edi, 4
        mov dword [es:edi + 4], 4           ; dwMajorVersion
        mov dword [es:edi + 8], 10          ; dwMinorVersion
        mov dword [es:edi + 12], 0x040A07CE ; dwBuildNumber
        mov dword [es:edi + 16], 1          ; VER_PLATFORM_WIN32_WINDOWS
        mov eax, 1
        ret
.mal:
        mov dword [ult_error], 122  ; ERROR_INSUFFICIENT_BUFFER
        xor eax, eax
        ret
k_GetLastError:
        mov eax, [ult_error]
        ret
k_SetLastError:                     ; (error)
        mov eax, [ebp]
        mov [ult_error], eax
        ret
k_SetErrorMode:                     ; (modo) -> el de antes
        mov eax, [modo_error]
        mov ecx, [ebp]
        mov [modo_error], ecx
        ret
k_SetUnhandledExceptionFilter:      ; (función) -> la de antes
        mov eax, [filtro_exc]
        mov ecx, [ebp]
        mov [filtro_exc], ecx
        ret
k_UnhandledExceptionFilter:         ; (info) -> EXCEPTION_EXECUTE_HANDLER
        mov eax, 1
        ret
k_SetHandleCount:                   ; (n)
        mov eax, [ebp]
        ret
k_OutputDebugStringA:
k_DebugBreak:
        ret
k_Beep:                             ; (frecuencia, ms)
k_GetComputerNameA_x:
        mov eax, 1
        ret
k_GetComputerNameA:                 ; (buf, *n)
        mov esi, s_equipo
        xor edi, edi
.c:
        lodsb
        mov [gs:TB_SAL + edi], al
        inc edi
        test al, al
        jnz .c
        mov edx, [ebp + 4]
        mov ecx, [es:edx]
        mov esi, TB_SAL
        mov edi, [ebp]
        call tb_a_prog
        mov edx, [ebp + 4]
        cmp eax, [es:edx]
        jae .no
        mov [es:edx], eax
        mov eax, 1
        ret
.no:
        mov [es:edx], eax
        mov dword [ult_error], 111  ; ERROR_BUFFER_OVERFLOW
        xor eax, eax
        ret
s_equipo    db 'HISPA-486', 0
; ---- otros programas: CreateProcessA los ejecuta por COMMAND.COM (/C), así
; que valen .COM, .EXE de DOS, .BAT y los de Windows (COMMAND.COM se los pasa a
; HW32). Es síncrono: al volver, el hijo ya ha terminado y su código queda
; guardado (WaitForSingleObject vuelve enseguida) ----
N_PROC      equ 16
H_PROC      equ 0x50000
k_CreateProcessA:                   ; (app, línea, sa, sa, heredar, opciones, entorno, carpeta, *si, *pi)
        cmp byte [md_modulo], 0     ; (módulo) dentro de Hispa Ventanas, no
        je .dos
        mov dword [ult_error], 120  ; ERROR_CALL_NOT_IMPLEMENTED
        xor eax, eax
        ret
.dos:
        ; el intérprete: COMSPEC
        mov edi, s_comspec
        call busca_env
        jc .sin_comspec
        xor edi, edi
.cs:
        mov al, [es:esi]
        mov [gs:TB_NOM1 + edi], al
        inc esi
        inc edi
        test al, al
        jz .cola
        cmp edi, 120
        jb .cs
        mov byte [gs:TB_NOM1 + edi], 0
        jmp .cola
.sin_comspec:
        mov esi, s_command
        xor edi, edi
.cs2:
        lodsb
        mov [gs:TB_NOM1 + edi], al
        inc edi
        test al, al
        jnz .cs2
.cola:
        ; la cola: « /C » + la línea (o el programa, si no hay línea)
        mov dword [gs:TB_NOM2 + 1], ' /C '
        mov edi, 5
        mov esi, [ebp + 4]
        test esi, esi
        jnz .l
        mov esi, [ebp]
        test esi, esi
        jz .mal_p
.l:
        mov al, [es:esi]
        test al, al
        jz .l_f
        cmp edi, 127
        jae .larga
        mov [gs:TB_NOM2 + edi], al
        inc esi
        inc edi
        jmp .l
.l_f:
        mov byte [gs:TB_NOM2 + edi], 13
        lea eax, [edi - 1]
        mov [gs:TB_NOM2], al
        ; el bloque de parámetros (entorno heredado, la cola, FCB vacíos)
        mov word [gs:TB_SAL], 0
        mov word [gs:TB_SAL + 2], TB_NOM2
        mov ax, [tb_seg]
        mov [gs:TB_SAL + 4], ax
        mov word [gs:TB_SAL + 6], TB_SAL + 0x40
        mov [gs:TB_SAL + 8], ax
        mov word [gs:TB_SAL + 10], TB_SAL + 0x40
        mov [gs:TB_SAL + 12], ax
        mov edi, TB_SAL + 0x40
        mov ecx, 16
.f:
        mov byte [gs:edi], 0
        inc edi
        loop .f
        mov byte [gs:TB_SAL + 0x41], ' '
        mov word [rm_dx], TB_NOM1
        mov word [rm_bx], TB_SAL
        mov word [rm_ax], 0x4B00
        call dos
        jc .mal
        mov word [rm_ax], 0x4D00
        call dos
        ; su ficha (dando la vuelta a las 16)
        mov ebx, [proc_sig]
        inc dword [proc_sig]
        and ebx, N_PROC - 1
        movzx eax, byte [rm_ax]
        mov [proc_codigo + ebx * 4], eax
        mov edi, [ebp + 36]         ; PROCESS_INFORMATION
        test edi, edi
        jz .bien
        lea eax, [ebx + H_PROC]
        mov [es:edi], eax
        add eax, 0x100
        mov [es:edi + 4], eax
        lea eax, [ebx + 1000]
        mov [es:edi + 8], eax
        add eax, 1000
        mov [es:edi + 12], eax
.bien:
        mov dword [ult_error], 0
        mov eax, 1
        ret
.larga:
        mov dword [ult_error], 206  ; ERROR_FILENAME_EXCED_RANGE
        xor eax, eax
        ret
.mal_p:
        mov dword [ult_error], 87
        xor eax, eax
        ret
.mal:
        call error_dos
        xor eax, eax
        ret
s_comspec   db 'COMSPEC', 0
s_command   db 'C:\COMMAND.COM', 0
k_GetExitCodeProcess:               ; (h, *código)
        mov eax, [ebp]
        mov edi, [ebp + 4]
        cmp eax, -1
        je .yo
        sub eax, H_PROC
        cmp eax, N_PROC
        jae .mal
        mov eax, [proc_codigo + eax * 4]
        mov [es:edi], eax
        mov eax, 1
        ret
.yo:
        mov dword [es:edi], 259     ; STILL_ACTIVE
        mov eax, 1
        ret
.mal:
        mov dword [ult_error], 6
        xor eax, eax
        ret
k_WaitForSingleObject:              ; (h, ms): ya está
        xor eax, eax
        ret
k_GetSystemDefaultLangID:
k_GetUserDefaultLangID:
k_GetSystemDefaultLCID:
k_GetUserDefaultLCID:
k_GetThreadLocale:
        mov eax, LCID_ES
        ret

; ---- TLS y secciones críticas (un solo hilo) ----
N_TLS       equ 64
k_TlsAlloc:
        xor eax, eax
.b:
        cmp byte [tls_usado + eax], 0
        je .si
        inc eax
        cmp eax, N_TLS
        jb .b
        mov eax, -1
        ret
.si:
        mov byte [tls_usado + eax], 1
        mov dword [tls_val + eax * 4], 0
        ret
k_TlsFree:                          ; (i)
        mov eax, [ebp]
        cmp eax, N_TLS
        jae .no
        mov byte [tls_usado + eax], 0
        mov eax, 1
        ret
.no:
        xor eax, eax
        ret
k_TlsGetValue:                      ; (i)
        mov ecx, [ebp]
        xor eax, eax
        cmp ecx, N_TLS
        jae .x
        mov dword [ult_error], 0
        mov eax, [tls_val + ecx * 4]
.x:
        ret
k_TlsSetValue:                      ; (i, valor)
        mov ecx, [ebp]
        xor eax, eax
        cmp ecx, N_TLS
        jae .x
        mov eax, [ebp + 4]
        mov [tls_val + ecx * 4], eax
        mov eax, 1
.x:
        ret
k_InitializeCriticalSection:        ; (*cs): a cero
        mov edi, [ebp]
        mov ecx, 24
        jmp a_cero
k_EnterCriticalSection:
k_LeaveCriticalSection:
k_DeleteCriticalSection:
        ret
k_InitializeCriticalSectionAndSpinCount:
k_TryEnterCriticalSection:
        mov eax, 1
        ret
k_InterlockedIncrement:             ; (*n) -> el nuevo
        mov edi, [ebp]
        mov eax, [es:edi]
        inc eax
        mov [es:edi], eax
        ret
k_InterlockedDecrement:
        mov edi, [ebp]
        mov eax, [es:edi]
        dec eax
        mov [es:edi], eax
        ret
k_InterlockedExchange:              ; (*n, v) -> el viejo
        mov edi, [ebp]
        mov eax, [ebp + 4]
        xchg eax, [es:edi]
        ret
k_InterlockedExchangeAdd:           ; (*n, v) -> el viejo
        mov edi, [ebp]
        mov ecx, [ebp + 4]
        mov eax, [es:edi]
        add [es:edi], ecx
        ret
k_InterlockedCompareExchange:       ; (*n, nuevo, comparar) -> el viejo
        mov edi, [ebp]
        mov eax, [es:edi]
        cmp eax, [ebp + 8]
        jne .x
        mov ecx, [ebp + 4]
        mov [es:edi], ecx
.x:
        ret

; =============================================================================
;  Textos y páginas de códigos (las de 8 bits pasan tal cual a 16)
; =============================================================================
k_GetACP:
        mov eax, 1252
        ret
k_GetOEMCP:
        mov eax, 437
        ret
k_IsValidCodePage:
        mov eax, 1
        ret
k_GetCPInfo:                        ; (página, *CPINFO)
        mov edi, [ebp + 4]
        mov ecx, 20
        call a_cero
        mov dword [es:edi], 1       ; MaxCharSize
        mov byte [es:edi + 4], '?'  ; DefaultChar
        mov eax, 1
        ret
k_MultiByteToWideChar:              ; (página, opciones, texto, n, buf, nbuf)
        mov esi, [ebp + 8]
        mov ecx, [ebp + 12]
        cmp ecx, -1
        jne .n
        xor ecx, ecx
.l:
        cmp byte [es:esi + ecx], 0
        je .l1
        inc ecx
        jmp .l
.l1:
        inc ecx
.n:
        mov eax, ecx
        mov edx, [ebp + 20]
        test edx, edx
        jz .x                       ; (solo cuánto)
        cmp ecx, edx
        ja .corto
        mov edi, [ebp + 16]
        jecxz .x
.c:
        movzx edx, byte [es:esi]
        mov [es:edi], dx
        inc esi
        add edi, 2
        loop .c
.x:
        ret
.corto:
        mov dword [ult_error], 122
        xor eax, eax
        ret
k_WideCharToMultiByte:              ; (página, opciones, texto, n, buf, nbuf, defecto, *usado)
        mov esi, [ebp + 8]
        mov ecx, [ebp + 12]
        cmp ecx, -1
        jne .n
        xor ecx, ecx
.l:
        cmp word [es:esi + ecx * 2], 0
        je .l1
        inc ecx
        jmp .l
.l1:
        inc ecx
.n:
        mov eax, ecx
        mov edx, [ebp + 20]
        test edx, edx
        jz .x
        cmp ecx, edx
        ja .corto
        mov edi, [ebp + 16]
        jecxz .x
.c:
        mov dx, [es:esi]
        cmp dx, 255
        jbe .c1
        mov dl, '?'
.c1:
        mov [es:edi], dl
        add esi, 2
        inc edi
        loop .c
.x:
        ret
.corto:
        mov dword [ult_error], 122
        xor eax, eax
        ret
k_lstrlenA:                         ; (texto)
        mov esi, [ebp]
        xor eax, eax
        test esi, esi
        jz .x
.c:
        cmp byte [es:esi + eax], 0
        je .x
        inc eax
        jmp .c
.x:
        ret
k_lstrcpyA:                         ; (destino, origen) -> destino
        mov edi, [ebp]
        mov esi, [ebp + 4]
.c:
        mov al, [es:esi]
        mov [es:edi], al
        inc esi
        inc edi
        test al, al
        jnz .c
        mov eax, [ebp]
        ret
k_lstrcpynA:                        ; (destino, origen, n)
        mov edi, [ebp]
        mov esi, [ebp + 4]
        mov ecx, [ebp + 8]
        jecxz .x
        dec ecx
        jecxz .f
.c:
        mov al, [es:esi]
        test al, al
        jz .f
        mov [es:edi], al
        inc esi
        inc edi
        loop .c
.f:
        mov byte [es:edi], 0
.x:
        mov eax, [ebp]
        ret
k_lstrcatA:                         ; (destino, origen)
        mov edi, [ebp]
.f:
        cmp byte [es:edi], 0
        je .c
        inc edi
        jmp .f
.c:
        mov esi, [ebp + 4]
.k:
        mov al, [es:esi]
        mov [es:edi], al
        inc esi
        inc edi
        test al, al
        jnz .k
        mov eax, [ebp]
        ret
k_lstrcmpA:                         ; (a, b)
        xor bl, bl
        jmp lstrcmp_com
k_lstrcmpiA:
        mov bl, 1
lstrcmp_com:
        mov esi, [ebp]
        mov edi, [ebp + 4]
.c:
        mov al, [es:esi]
        mov ah, [es:edi]
        test bl, bl
        jz .s
        call mayus2
.s:
        cmp al, ah
        jne .d
        test al, al
        jz .ig
        inc esi
        inc edi
        jmp .c
.ig:
        xor eax, eax
        ret
.d:
        mov eax, 1
        ja .x
        mov eax, -1
.x:
        ret

; =============================================================================
;  La tabla: manejador, octetos de argumentos (stdcall), nombre
; =============================================================================
%macro FUNC 3
        dd %1
        dw %2
        db %3, 0
%endmacro
funciones:
        ; consola
        FUNC k_GetStdHandle, 4, 'GetStdHandle'
        FUNC k_SetStdHandle, 8, 'SetStdHandle'
        FUNC k_GetFileType, 4, 'GetFileType'
        FUNC k_WriteFile, 20, 'WriteFile'
        FUNC k_WriteConsoleA, 20, 'WriteConsoleA'
        FUNC k_ReadFile, 20, 'ReadFile'
        FUNC k_ReadConsoleA, 20, 'ReadConsoleA'
        FUNC k_SetConsoleTextAttribute, 8, 'SetConsoleTextAttribute'
        FUNC k_SetConsoleCursorPosition, 8, 'SetConsoleCursorPosition'
        FUNC k_GetConsoleScreenBufferInfo, 8, 'GetConsoleScreenBufferInfo'
        FUNC k_GetConsoleMode, 8, 'GetConsoleMode'
        FUNC k_SetConsoleMode, 8, 'SetConsoleMode'
        FUNC k_SetConsoleTitleA, 4, 'SetConsoleTitleA'
        FUNC k_GetConsoleTitleA, 8, 'GetConsoleTitleA'
        FUNC k_GetConsoleCP, 0, 'GetConsoleCP'
        FUNC k_GetConsoleOutputCP, 0, 'GetConsoleOutputCP'
        FUNC k_SetConsoleCP, 4, 'SetConsoleCP'
        FUNC k_SetConsoleOutputCP, 4, 'SetConsoleOutputCP'
        FUNC k_GetConsoleCursorInfo, 8, 'GetConsoleCursorInfo'
        FUNC k_SetConsoleCursorInfo, 8, 'SetConsoleCursorInfo'
        FUNC k_FillConsoleOutputCharacterA, 20, 'FillConsoleOutputCharacterA'
        FUNC k_FillConsoleOutputAttribute, 20, 'FillConsoleOutputAttribute'
        FUNC k_WriteConsoleOutputCharacterA, 20, 'WriteConsoleOutputCharacterA'
        FUNC k_ReadConsoleInputA, 16, 'ReadConsoleInputA'
        FUNC k_PeekConsoleInputA, 16, 'PeekConsoleInputA'
        FUNC k_GetNumberOfConsoleInputEvents, 8, 'GetNumberOfConsoleInputEvents'
        FUNC k_FlushConsoleInputBuffer, 4, 'FlushConsoleInputBuffer'
        ; archivos
        FUNC k_CreateFileA, 28, 'CreateFileA'
        FUNC k_CloseHandle, 4, 'CloseHandle'
        FUNC k_DuplicateHandle, 28, 'DuplicateHandle'
        FUNC k_SetFilePointer, 16, 'SetFilePointer'
        FUNC k_GetFileSize, 8, 'GetFileSize'
        FUNC k_SetEndOfFile, 4, 'SetEndOfFile'
        FUNC k_FlushFileBuffers, 4, 'FlushFileBuffers'
        FUNC k_DeleteFileA, 4, 'DeleteFileA'
        FUNC k_CreateDirectoryA, 8, 'CreateDirectoryA'
        FUNC k_RemoveDirectoryA, 4, 'RemoveDirectoryA'
        FUNC k_SetFileAttributesA, 8, 'SetFileAttributesA'
        FUNC k_GetFileAttributesA, 4, 'GetFileAttributesA'
        FUNC k_MoveFileA, 8, 'MoveFileA'
        FUNC k_CopyFileA, 12, 'CopyFileA'
        FUNC k_GetCurrentDirectoryA, 8, 'GetCurrentDirectoryA'
        FUNC k_SetCurrentDirectoryA, 4, 'SetCurrentDirectoryA'
        FUNC k_GetFullPathNameA, 16, 'GetFullPathNameA'
        FUNC k_GetDriveTypeA, 4, 'GetDriveTypeA'
        FUNC k_GetLogicalDrives, 0, 'GetLogicalDrives'
        FUNC k_GetDiskFreeSpaceA, 20, 'GetDiskFreeSpaceA'
        FUNC k_GetTempPathA, 8, 'GetTempPathA'
        FUNC k_GetTempFileNameA, 16, 'GetTempFileNameA'
        FUNC k_SearchPathA, 24, 'SearchPathA'
        FUNC k_GetWindowsDirectoryA, 8, 'GetWindowsDirectoryA'
        FUNC k_GetSystemDirectoryA, 8, 'GetSystemDirectoryA'
        FUNC k_FindFirstFileA, 8, 'FindFirstFileA'
        FUNC k_FindNextFileA, 8, 'FindNextFileA'
        FUNC k_FindClose, 4, 'FindClose'
        ; hora
        FUNC k_GetLocalTime, 4, 'GetLocalTime'
        FUNC k_GetSystemTime, 4, 'GetSystemTime'
        FUNC k_GetSystemTimeAsFileTime, 4, 'GetSystemTimeAsFileTime'
        FUNC k_SystemTimeToFileTime, 8, 'SystemTimeToFileTime'
        FUNC k_FileTimeToSystemTime, 8, 'FileTimeToSystemTime'
        FUNC k_FileTimeToLocalFileTime, 8, 'FileTimeToLocalFileTime'
        FUNC k_LocalFileTimeToFileTime, 8, 'LocalFileTimeToFileTime'
        FUNC k_CompareFileTime, 8, 'CompareFileTime'
        FUNC k_FileTimeToDosDateTime, 12, 'FileTimeToDosDateTime'
        FUNC k_DosDateTimeToFileTime, 12, 'DosDateTimeToFileTime'
        FUNC k_GetTimeZoneInformation, 4, 'GetTimeZoneInformation'
        FUNC k_GetTickCount, 0, 'GetTickCount'
        FUNC k_QueryPerformanceFrequency, 4, 'QueryPerformanceFrequency'
        FUNC k_QueryPerformanceCounter, 4, 'QueryPerformanceCounter'
        FUNC k_Sleep, 4, 'Sleep'
        ; memoria
        FUNC k_GetProcessHeap, 0, 'GetProcessHeap'
        FUNC k_HeapCreate, 12, 'HeapCreate'
        FUNC k_HeapDestroy, 4, 'HeapDestroy'
        FUNC k_HeapAlloc, 12, 'HeapAlloc'
        FUNC k_HeapFree, 12, 'HeapFree'
        FUNC k_HeapSize, 12, 'HeapSize'
        FUNC k_HeapReAlloc, 16, 'HeapReAlloc'
        FUNC k_HeapValidate, 12, 'HeapValidate'
        FUNC k_HeapCompact, 8, 'HeapCompact'
        FUNC k_HeapLock, 4, 'HeapLock'
        FUNC k_HeapUnlock, 4, 'HeapUnlock'
        FUNC k_LocalAlloc, 8, 'LocalAlloc'
        FUNC k_GlobalAlloc, 8, 'GlobalAlloc'
        FUNC k_LocalFree, 4, 'LocalFree'
        FUNC k_GlobalFree, 4, 'GlobalFree'
        FUNC k_LocalLock, 4, 'LocalLock'
        FUNC k_GlobalLock, 4, 'GlobalLock'
        FUNC k_GlobalHandle, 4, 'GlobalHandle'
        FUNC k_LocalUnlock, 4, 'LocalUnlock'
        FUNC k_GlobalUnlock, 4, 'GlobalUnlock'
        FUNC k_LocalSize, 4, 'LocalSize'
        FUNC k_GlobalSize, 4, 'GlobalSize'
        FUNC k_LocalReAlloc, 12, 'LocalReAlloc'
        FUNC k_GlobalReAlloc, 12, 'GlobalReAlloc'
        FUNC k_VirtualAlloc, 16, 'VirtualAlloc'
        FUNC k_VirtualFree, 12, 'VirtualFree'
        FUNC k_VirtualQuery, 12, 'VirtualQuery'
        FUNC k_VirtualProtect, 16, 'VirtualProtect'
        FUNC k_GlobalMemoryStatus, 4, 'GlobalMemoryStatus'
        FUNC k_GetSystemInfo, 4, 'GetSystemInfo'
        FUNC k_IsBadReadPtr, 8, 'IsBadReadPtr'
        FUNC k_IsBadWritePtr, 8, 'IsBadWritePtr'
        FUNC k_IsBadCodePtr, 4, 'IsBadCodePtr'
        FUNC k_IsBadStringPtrA, 8, 'IsBadStringPtrA'
        ; proceso
        FUNC k_ExitProcess, 4, 'ExitProcess'
        FUNC k_ExitThread, 4, 'ExitThread'
        FUNC k_TerminateProcess, 8, 'TerminateProcess'
        FUNC k_FatalAppExitA, 8, 'FatalAppExitA'
        FUNC k_RaiseException, 16, 'RaiseException'
        FUNC k_GetCurrentProcess, 0, 'GetCurrentProcess'
        FUNC k_GetCurrentThread, 0, 'GetCurrentThread'
        FUNC k_GetCurrentProcessId, 0, 'GetCurrentProcessId'
        FUNC k_GetCurrentThreadId, 0, 'GetCurrentThreadId'
        FUNC k_GetCommandLineA, 0, 'GetCommandLineA'
        FUNC k_GetEnvironmentStrings, 0, 'GetEnvironmentStrings'
        FUNC k_GetEnvironmentStrings, 0, 'GetEnvironmentStringsA'
        FUNC k_FreeEnvironmentStringsA, 4, 'FreeEnvironmentStringsA'
        FUNC k_GetEnvironmentVariableA, 12, 'GetEnvironmentVariableA'
        FUNC k_SetEnvironmentVariableA, 8, 'SetEnvironmentVariableA'
        FUNC k_GetStartupInfoA, 4, 'GetStartupInfoA'
        FUNC k_GetModuleHandleA, 4, 'GetModuleHandleA'
        FUNC k_GetModuleHandleW, 4, 'GetModuleHandleW'
        FUNC k_IsDBCSLeadByteEx, 8, 'IsDBCSLeadByteEx'
        FUNC k_IsDBCSLeadByte, 4, 'IsDBCSLeadByte'
        FUNC k_LoadLibraryA, 4, 'LoadLibraryA'
        FUNC k_LoadLibraryExA, 12, 'LoadLibraryExA'
        FUNC k_FreeLibrary, 4, 'FreeLibrary'
        FUNC k_GetModuleFileNameA, 12, 'GetModuleFileNameA'
        FUNC k_GetProcAddress, 8, 'GetProcAddress'
        FUNC k_GetVersion, 0, 'GetVersion'
        FUNC k_GetVersionExA, 4, 'GetVersionExA'
        FUNC k_GetLastError, 0, 'GetLastError'
        FUNC k_SetLastError, 4, 'SetLastError'
        FUNC k_SetErrorMode, 4, 'SetErrorMode'
        FUNC k_SetUnhandledExceptionFilter, 4, 'SetUnhandledExceptionFilter'
        FUNC k_UnhandledExceptionFilter, 4, 'UnhandledExceptionFilter'
        FUNC k_SetHandleCount, 4, 'SetHandleCount'
        FUNC k_OutputDebugStringA, 4, 'OutputDebugStringA'
        FUNC k_DebugBreak, 0, 'DebugBreak'
        FUNC k_Beep, 8, 'Beep'
        FUNC k_GetComputerNameA, 8, 'GetComputerNameA'
        FUNC k_WaitForSingleObject, 8, 'WaitForSingleObject'
        FUNC k_CreateProcessA, 40, 'CreateProcessA'
        FUNC k_GetExitCodeProcess, 8, 'GetExitCodeProcess'
        FUNC k_GetSystemDefaultLangID, 0, 'GetSystemDefaultLangID'
        FUNC k_GetUserDefaultLangID, 0, 'GetUserDefaultLangID'
        FUNC k_GetSystemDefaultLCID, 0, 'GetSystemDefaultLCID'
        FUNC k_GetUserDefaultLCID, 0, 'GetUserDefaultLCID'
        FUNC k_GetThreadLocale, 0, 'GetThreadLocale'
        FUNC k_TlsAlloc, 0, 'TlsAlloc'
        FUNC k_TlsFree, 4, 'TlsFree'
        FUNC k_TlsGetValue, 4, 'TlsGetValue'
        FUNC k_TlsSetValue, 8, 'TlsSetValue'
        FUNC k_InitializeCriticalSection, 4, 'InitializeCriticalSection'
        FUNC k_InitializeCriticalSectionAndSpinCount, 8, 'InitializeCriticalSectionAndSpinCount'
        FUNC k_EnterCriticalSection, 4, 'EnterCriticalSection'
        FUNC k_TryEnterCriticalSection, 4, 'TryEnterCriticalSection'
        FUNC k_LeaveCriticalSection, 4, 'LeaveCriticalSection'
        FUNC k_DeleteCriticalSection, 4, 'DeleteCriticalSection'
        FUNC k_InterlockedIncrement, 4, 'InterlockedIncrement'
        FUNC k_InterlockedDecrement, 4, 'InterlockedDecrement'
        FUNC k_InterlockedExchange, 8, 'InterlockedExchange'
        FUNC k_InterlockedExchangeAdd, 8, 'InterlockedExchangeAdd'
        FUNC k_InterlockedCompareExchange, 12, 'InterlockedCompareExchange'
        ; textos
        FUNC k_GetACP, 0, 'GetACP'
        FUNC k_GetOEMCP, 0, 'GetOEMCP'
        FUNC k_IsValidCodePage, 4, 'IsValidCodePage'
        FUNC k_GetCPInfo, 8, 'GetCPInfo'
        FUNC k_MultiByteToWideChar, 24, 'MultiByteToWideChar'
        FUNC k_WideCharToMultiByte, 32, 'WideCharToMultiByte'
        FUNC k_lstrlenA, 4, 'lstrlenA'
        FUNC k_lstrlenA, 4, 'lstrlen'
        FUNC k_lstrcpyA, 8, 'lstrcpyA'
        FUNC k_lstrcpyA, 8, 'lstrcpy'
        FUNC k_lstrcpynA, 12, 'lstrcpynA'
        FUNC k_lstrcatA, 8, 'lstrcatA'
        FUNC k_lstrcatA, 8, 'lstrcat'
        FUNC k_lstrcmpA, 8, 'lstrcmpA'
        FUNC k_lstrcmpA, 8, 'lstrcmp'
        FUNC k_lstrcmpiA, 8, 'lstrcmpiA'
        FUNC k_lstrcmpiA, 8, 'lstrcmpi'
        dd 0

; ---- las teclas virtuales de los códigos de la BIOS (0 a 58h) ----
vk_tabla:
        db 0x00, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xBD, 0xBB, 0x08, 0x09
        db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0xDB, 0xDD, 0x0D, 0x11, 'A', 'S'
        db 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xBA, 0xDE, 0xC0, 0x10, 0xDC, 'Z', 'X', 'C', 'V'
        db 'B', 'N', 'M', 0xBC, 0xBE, 0xBF, 0x10, 0x6A, 0x12, 0x20, 0x14, 0x70, 0x71, 0x72, 0x73, 0x74
        db 0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x24, 0x26, 0x21, 0x6D, 0x25, 0x0C, 0x27, 0x6B, 0x23
        db 0x28, 0x22, 0x2D, 0x2E, 0x00, 0x00, 0xE2, 0x7A, 0x7B
; y los de más allá (con Ctrl, Alt, Mayús o de las teclas grises): código, tecla
vk_ext:
        db 0x85, 0x7A, 0x86, 0x7B, 0x87, 0x7A, 0x88, 0x7B, 0x89, 0x7A, 0x8A, 0x7B, 0x8B, 0x7A, 0x8C, 0x7B
        db 0x73, 0x25, 0x74, 0x27, 0x77, 0x24, 0x75, 0x23, 0x84, 0x21, 0x76, 0x22
        db 0x8D, 0x26, 0x91, 0x28, 0x92, 0x2D, 0x93, 0x2E
        db 0x97, 0x24, 0x98, 0x26, 0x99, 0x21, 0x9B, 0x25, 0x9D, 0x27, 0x9F, 0x23, 0xA0, 0x28, 0xA1, 0x22, 0xA2, 0x2D, 0xA3, 0x2E
        db 0x54, 0x70, 0x55, 0x71, 0x56, 0x72, 0x57, 0x73, 0x58, 0x74, 0x59, 0x75, 0x5A, 0x76, 0x5B, 0x77, 0x5C, 0x78, 0x5D, 0x79
        db 0x5E, 0x70, 0x5F, 0x71, 0x60, 0x72, 0x61, 0x73, 0x62, 0x74, 0x63, 0x75, 0x64, 0x76, 0x65, 0x77, 0x66, 0x78, 0x67, 0x79
        db 0x68, 0x70, 0x69, 0x71, 0x6A, 0x72, 0x6B, 0x73, 0x6C, 0x74, 0x6D, 0x75, 0x6E, 0x76, 0x6F, 0x77, 0x70, 0x78, 0x71, 0x79
        db 0

m_excepcion db 'Hispa-486: excepcion', 0
m_exc_cod   db '00000000', 0         ; (justo detras: de modulo se dice todo junto)
m_nl2       db 13, 10, 0
m_esp       db ' ', 0

; ---- los datos de KERNEL32 ----
        align 4
con_attr    db 7
pag_act     db 0
suelta_pend db 0
            db 0
suelta_tecla dw 0
sel_pant    dw 0
h_tam       dw 0
h_cop1      dw 0
h_cop2      dw 0
            dw 0
busca_act   dd 0
dta_act     dd 0
busca_usada times N_BUSCA db 0
pos_cifras  dd 0
num_temp    dd 0
p_anio      dd 0
p_mes       dd 0
p_dia       dd 0
p_sem       dd 0
p_hora      dd 0
p_min       dd 0
p_seg       dd 0
p_ms        dd 0
m_pedido    dd 0
m_libres    times CLASE_MAX + 1 dd 0
virt_dir    times N_VIRT dd 0
virt_tam    times N_VIRT dd 0
virt_h      times N_VIRT dd 0
tls_usado   times N_TLS db 0
tls_val     times N_TLS dd 0
modo_error  dd 0
filtro_exc  dd 0
b_mem       times 48 db 0
b_hex8      times 12 db 0
b_nombre    times 64 db 0
b_sp_nom    times 210 db 0
proc_sig    dd 0
proc_codigo times N_PROC dd 0
b_sp_dir    times 130 db 0
