; =============================================================================
;  Hispa Ventanas - [GL] lo que pide OPENGL32.DLL (HispaGL, docs/486/OPENGL.md)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  En Windows, SwapBuffers, ChoosePixelFormat, SetPixelFormat,
;  DescribePixelFormat y GetPixelFormat son de GDI32 (que llama a OPENGL32), y
;  ChangeDisplaySettingsA y EnumDisplaySettingsA de USER32. Aquí son un
;  puente: OPENGL32.DLL da al cargarse su función (HispaGLPuente) y estas le
;  pasan la llamada por pe_llamar (op, a, b, c):
;    1 SwapBuffers(hdc)            5 GetPixelFormat(hdc)
;    2 ChoosePixelFormat(hdc, pfd) 6 ChangeDisplaySettings(modo, flags)
;    3 SetPixelFormat(hdc, n, pfd) 7 EnumDisplaySettings(n, modo)
;    4 DescribePixelFormat(n, tamaño, pfd)
;  Sin OPENGL32 cargada, dicen que no (ChangeDisplaySettingsA, que sí: «hecho»,
;  sin cambiar nada: es lo que esperan los juegos de DirectDraw que lo llaman).
;  Y WindowFromDC, que no estaba.
; =============================================================================

; BOOL HispaGLPuente(función)
u_HispaGLPuente:
        mov eax, [ebp]
        mov [edx+PE_GL], eax
        jmp u_uno

; (interna) EBX = la operación, ECX, ESI, EDI = sus argumentos -> EAX (0 sin
; puente). Conserva EBX, ESI, EDI
gl_puente:
        mov eax, [edx+PE_GL]
        test eax, eax
        jz .no
        call pe_llamar              ; (pushad/popad: vuelve EAX)
        ret
.no:
        xor eax, eax
        ret

g_SwapBuffers:                      ; (hdc)
        push ebx
        push esi
        push edi
        mov ebx, 1
        mov ecx, [ebp]
        xor esi, esi
        xor edi, edi
        jmp gl_op
g_ChoosePixelFormat:                ; (hdc, pfd)
        push ebx
        push esi
        push edi
        mov ebx, 2
        mov ecx, [ebp]
        mov esi, [ebp+4]
        xor edi, edi
        jmp gl_op
g_SetPixelFormat:                   ; (hdc, n, pfd)
        push ebx
        push esi
        push edi
        mov ebx, 3
        mov ecx, [ebp]
        mov esi, [ebp+4]
        mov edi, [ebp+8]
        jmp gl_op
g_DescribePixelFormat:              ; (hdc, n, tamaño, pfd)
        push ebx
        push esi
        push edi
        mov ebx, 4
        mov ecx, [ebp+4]
        mov esi, [ebp+8]
        mov edi, [ebp+12]
        jmp gl_op
g_GetPixelFormat:                   ; (hdc)
        push ebx
        push esi
        push edi
        mov ebx, 5
        mov ecx, [ebp]
        xor esi, esi
        xor edi, edi
        jmp gl_op
u_ChangeDisplaySettingsA:           ; (modo, flags)
        push ebx
        push esi
        push edi
        mov ebx, 6
        mov ecx, [ebp]
        mov esi, [ebp+4]
        xor edi, edi
        jmp gl_op
u_ChangeDisplaySettingsExA:         ; (nombre, modo, ventana, flags, x)
        push ebx
        push esi
        push edi
        mov ebx, 6
        mov ecx, [ebp+4]
        mov esi, [ebp+12]
        xor edi, edi
        jmp gl_op
u_EnumDisplaySettingsA:             ; (nombre, n, modo)
        push ebx
        push esi
        push edi
        mov ebx, 7
        mov ecx, [ebp+4]
        mov esi, [ebp+8]
        xor edi, edi
gl_op:
        call gl_puente
        pop edi
        pop esi
        pop ebx
        ret

; HWND WindowFromDC(hdc): la ventana del contexto (0 en los de memoria)
u_WindowFromDC:
        push edi
        mov eax, [ebp]
        call w32_dc
        jc .no
        xor eax, eax
        cmp dword [edi+DC_USO], 3
        je .x
        mov eax, [edi+DC_VEN]
        call w32_hwnd
        jmp .x
.no:
        xor eax, eax
.x:
        pop edi
        ret
