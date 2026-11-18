; =============================================================================
;  Hispa Ventanas - [NAVEGADOR] lo que le faltaba al Navegador de Hispa-486
;  (w32/navega, docs/486/NAVEGADOR.md) de USER32, GDI32, COMDLG32 y KERNEL32:
;    SetDIBitsToDevice   un mapa de bits de 8 bits por punto (imágenes)
;    GetKeyState         Mayús, Ctrl y Alt (Mayús+Tab)
;    GetOpenFileNameA, GetSaveFileNameA   el diálogo de elegir un archivo
;    WinExec             abrir otro programa (el Lector con un PDF bajado)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

DIB_FILA    equ 2048                ; lo más ancho que se copia de una fila

; int SetDIBitsToDevice(HDC, xd, yd, ancho, alto, xs, ys, primera, líneas,
; bits, BITMAPINFO*, uso). Solo de 8 bits por punto, sin comprimir; de abajo
; arriba (biHeight > 0) o de arriba abajo (< 0). Con DIB_RGB_COLORS (0) cada
; color de la tabla va al más parecido de la paleta; con DIB_PAL_COLORS (1)
; los octetos son ya colores de la paleta de Hispa Ventanas (la tabla no se
; mira). «primera» y «líneas» se pasan por alto: el mapa va entero en «bits».
; -> las filas copiadas (0 si no se puede)
g_SetDIBitsToDevice:
        mov eax, [ebp]
        call w32_dc_abrir
        jc u_cero
        mov esi, [ebp+40]
        add esi, [edx+PE_DELTA]
        cmp word [esi+14], 8        ; biBitCount
        jne .no
        cmp dword [esi+16], 0       ; biCompression: BI_RGB
        jne .no
        ; la tabla de colores, a colores de la paleta
        xor eax, eax
.ident:
        mov [dib_mapa+eax], al
        inc al
        jnz .ident
        cmp dword [ebp+44], 0
        jne .rgb_x
        push edx
        mov ecx, [esi+32]           ; biClrUsed (0: los 256)
        test ecx, ecx
        jz .c256
        cmp ecx, 256
        jbe .c1
.c256:
        mov ecx, 256
.c1:
        mov ebx, [esi]              ; biSize: la tabla va detrás
        add ebx, esi
        xor edi, edi
.col:
        movzx eax, byte [ebx+edi*4]     ; azul
        shl eax, 16
        mov ah, [ebx+edi*4+1]           ; verde
        mov al, [ebx+edi*4+2]           ; rojo
        call w32_color
        mov [dib_mapa+edi], al
        inc edi
        cmp edi, ecx
        jb .col
        pop edx
.rgb_x:
        ; las medidas: el mapa (W x |H|, filas de (W+3) & ~3 octetos)
        mov eax, [esi+4]
        mov [dib_w], eax
        add eax, 3
        and eax, ~3
        mov [dib_paso], eax
        mov eax, [esi+8]
        mov dword [dib_arriba], 0
        test eax, eax
        jns .h
        neg eax
        mov dword [dib_arriba], 1
.h:
        mov [dib_h], eax
        mov eax, [ebp+36]
        add eax, [edx+PE_DELTA]
        mov [dib_bits], eax
        ; lo que se copia: ancho = min(ancho, W - xs, DIB_FILA)
        mov ecx, [ebp+12]
        mov eax, [dib_w]
        sub eax, [ebp+20]
        jle .no
        cmp ecx, eax
        jbe .w1
        mov ecx, eax
.w1:
        cmp ecx, DIB_FILA
        jbe .w2
        mov ecx, DIB_FILA
.w2:
        test ecx, ecx
        jle .no
        mov [dib_cw], ecx
        mov dword [dib_n], 0
        xor ebx, ebx                ; la fila r del destino
.fila:
        cmp ebx, [ebp+16]
        jge .fin
        ; la fila de la memoria: de arriba abajo ys + r; de abajo arriba,
        ; ys + alto - 1 - r (ys cuenta desde abajo)
        mov eax, [ebp+24]
        cmp dword [dib_arriba], 0
        je .abajo
        add eax, ebx
        jmp .m
.abajo:
        add eax, [ebp+16]
        dec eax
        sub eax, ebx
.m:
        test eax, eax
        js .sig
        cmp eax, [dib_h]
        jae .sig
        imul eax, [dib_paso]
        mov esi, [dib_bits]
        add esi, eax
        add esi, [ebp+20]
        mov edi, dib_fila
        mov ecx, [dib_cw]
        push ebx
.cp:
        lodsb
        movzx eax, al
        mov al, [dib_mapa+eax]
        stosb
        dec ecx
        jnz .cp
        pop ebx
        push ebx
        mov eax, [ebp+4]
        add ebx, [ebp+8]
        mov ecx, [dib_cw]
        mov edx, 1
        mov esi, dib_fila
        call dc_copiar
        pop ebx
        inc dword [dib_n]
.sig:
        inc ebx
        jmp .fila
.fin:
        call dc_cerrar
        mov eax, [dib_n]
        ret
.no:
        call dc_cerrar
        xor eax, eax
        ret

; SHORT GetKeyState(tecla virtual): el bit de arriba, si está pulsada. Solo
; Mayús (10h, A0h, A1h), Ctrl (11h, A2h, A3h) y Alt (12h, A4h, A5h)
u_GetKeyState:
        mov ecx, [ebp]
        call tec_estado
        mov edx, TM_MAYUS
        cmp ecx, 0x10
        je .m
        cmp ecx, 0xA0
        je .m
        cmp ecx, 0xA1
        je .m
        mov edx, TM_CTRL
        cmp ecx, 0x11
        je .m
        cmp ecx, 0xA2
        je .m
        cmp ecx, 0xA3
        je .m
        mov edx, TM_ALT
        cmp ecx, 0x12
        je .m
        cmp ecx, 0xA4
        je .m
        cmp ecx, 0xA5
        je .m
        xor eax, eax
        ret
.m:
        test eax, edx
        mov eax, 0
        jz .x
        mov eax, 0xFFFF8000
.x:
        ret

; BOOL GetOpenFileNameA(OPENFILENAMEA*) y GetSaveFileNameA(OPENFILENAMEA*):
; el diálogo de Hispa Ventanas (dlg_archivo). Se empieza en lpstrInitialDir
; (si lo dice) con el nombre que traiga lpstrFile; se devuelve en lpstrFile
; la ruta entera (con la unidad). La carpeta de trabajo, que es de todos los
; programas, queda como estaba. Los filtros no se usan.
OFN_TAM     equ 512                 ; nombre (80), carpeta de antes (80), título y ruta
u_GetOpenFileNameA:
        mov eax, t_w32_abrir
        jmp w32_archivo
u_GetSaveFileNameA:
        mov eax, t_w32_guardar
w32_archivo:
        push eax
        mov ecx, OFN_TAM
        call mem_pedir
        pop ecx
        jc u_cero
        mov ebx, eax                ; EBX = la memoria
        mov [ebx+OFN_TAM-4], ecx    ; (el título de serie)
        lea edi, [ebx+80]           ; la carpeta de ahora, para dejarla luego
        call dos_dir_actual
        mov esi, [ebp]
        add esi, [edx+PE_DELTA]     ; ESI = OPENFILENAMEA
        push esi
        mov esi, [esi+44]           ; lpstrInitialDir
        test esi, esi
        jz .ini
        lea edi, [ebx+160]
        mov ecx, 79
        call w32_texto
        lea esi, [ebx+160]
        cmp byte [esi], 0
        je .ini
        call dos_ir_carpeta
.ini:
        mov esi, [esp]
        mov esi, [esi+28]           ; lpstrFile: el nombre de partida
        mov edi, ebx
        mov ecx, 79
        call w32_texto
        mov esi, [esp]
        mov esi, [esi+48]           ; lpstrTitle
        lea edi, [ebx+160]
        mov ecx, 60
        test esi, esi
        jz .t0
        call w32_texto
        jmp .t1
.t0:
        mov esi, [ebx+OFN_TAM-4]
        call pon_cad
        mov byte [edi], 0
.t1:
        push edx
        push ebp
        mov esi, [esp+8]
        mov eax, [esi+4]            ; la dueña
        call w32_ven
        mov ebp, eax
        lea edi, [ebx+160]
        call dlg_archivo            ; EBX = el nombre (80)
        pop ebp
        pop edx
        pop esi                     ; ESI = OPENFILENAMEA
        test eax, eax
        jz .fuera
        ; la ruta entera: el nombre tal cual si ya la lleva (C:..., \...);
        ; si no, la carpeta en que ha acabado el diálogo y el nombre
        lea edi, [ebx+160]
        cmp byte [ebx+1], ':'
        je .tal
        cmp byte [ebx], '\'
        je .tal
        call dos_dir_actual
.fin_c:
        cmp byte [edi], 0
        je .bar
        inc edi
        jmp .fin_c
.bar:
        cmp byte [edi-1], '\'
        je .tal
        mov byte [edi], '\'
        inc edi
.tal:
        push esi
        mov esi, ebx
        call pon_cad
        mov byte [edi], 0
        pop esi
        ; a lpstrFile (nMaxFile), de la página 437 a la 1252
        mov edi, [esi+28]
        test edi, edi
        jz .mal
        add edi, [edx+PE_DELTA]
        mov ecx, [esi+32]
        dec ecx
        jle .mal
        push esi
        lea esi, [ebx+160]
        xor eax, eax
        mov [ebx+OFN_TAM-8], eax    ; dónde empieza el nombre
.c:
        lodsb
        test al, al
        jz .c9
        cmp al, '\'
        jne .c1
        mov edx, esi
        sub edx, ebx
        sub edx, 160
        mov [ebx+OFN_TAM-8], edx
.c1:
        test al, al
        jns .c2
        movzx eax, al
        mov al, [w32_437+eax-0x80]
.c2:
        stosb
        dec ecx
        jnz .c
.c9:
        mov byte [edi], 0
        pop esi
        mov eax, [ebx+OFN_TAM-8]
        mov [esi+56], ax            ; nFileOffset
        mov dword [ebx+OFN_TAM-4], 1
        jmp .deja
.mal:
.fuera:
        mov dword [ebx+OFN_TAM-4], 0
.deja:
        lea esi, [ebx+80]
        call dos_ir_carpeta
        mov eax, ebx
        mov ebx, [ebx+OFN_TAM-4]
        call mem_soltar
        mov eax, ebx
        ret

; UINT WinExec(línea, cómo): «programa parámetros» con prog_abrir (como un
; acceso directo: un .HVP se abre dentro de Hispa Ventanas). -> 33 (abierto)
WE_TAM      equ 160
k_WinExec:
        mov ecx, WE_TAM
        call mem_pedir
        jc .no
        mov ebx, eax
        mov esi, [ebp]
        mov edi, ebx
        mov ecx, WE_TAM - 1
        call w32_texto
        mov esi, ebx                ; el programa hasta el primer espacio
        xor edi, edi
.b:
        mov al, [esi]
        test al, al
        jz .p
        inc esi
        cmp al, ' '
        jne .b
        mov byte [esi-1], 0
        mov edi, esi                ; los parámetros
.p:
        mov esi, ebx
        xor edx, edx
        xor ecx, ecx
        push ebx
        xor ebx, ebx
        call prog_abrir
        pop eax
        call mem_soltar
        mov eax, 33
        ret
.no:
        xor eax, eax
        ret

t_w32_abrir     db "Abrir", 0
t_w32_guardar   db "Guardar como", 0
