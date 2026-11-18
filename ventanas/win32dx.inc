; =============================================================================
;  Hispa Ventanas - [DX] lo que piden las DLL de DirectX de Hispa-486 (fase 5)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  DDRAW.DLL, DSOUND.DLL, DINPUT.DLL y WINMM.DLL (w32/, en C) se hacen sobre
;  USER32 y GDI32 y, para lo que solo sabe el entorno, sobre cuatro funciones
;  propias de USER32 (en w32_tabla, DLL 1):
;
;    HispaDXPantalla(orden, a, b, c)   la pantalla entera para un programa
;      0 tomar (a = HWND de su ventana, b = HXPANT*) -> 1 si la tiene
;        (marco lineal de VESA: 8, 15, 16, 24 o 32 bits; en la VGA de 16
;        colores, no). Mientras es suya, Hispa Ventanas sigue pintando en su
;        superficie pero no la lleva a la pantalla (vid_presentar). Si deja
;        de ser la activa la ventana del programa (otra ventana, Ctrl+Alt+
;        Supr, un MessageBox), la pantalla se devuelve sola: la paleta de
;        Hispa Ventanas y todo lo pintado.
;      1 soltar
;      2 paleta (a = primero, b = cuántos, c = PALETTEENTRY*): al DAC (solo
;        en los modos de 8 bits y si la pantalla es suya)
;      3 esperar el comienzo del retrazo vertical (puerto 3DAh)
;      4 ¿es suya? -> 1/0
;      5 ¿retrazo ahora? -> 1/0
;      7 cómo es, sin tomarla (a = HXPANT*; el puntero a la pantalla, 0)
;      6 cómo se ve (a = int[6]: ancho y alto del modo del juego, x, y,
;        ancho y alto de donde sale en la pantalla): para pasar el ratón a
;        las coordenadas del juego y para GetSystemMetrics
;    HispaDXSonido(orden, a, b, c)      HSPSND
;      0 la tarjeta (-1 sin HSPSND; 0 ninguna, 1 SB, 2 SB16, 3 AC'97, 4 HDA)
;      1 tocar (a = muestras, b = octetos, hasta 8.192, c = Hz | formato << 24)
;      2 sitio: trozos que caben sin esperar (0..4; -1 sin sonido)
;      3 parar
;    HispaDXEntrada(orden, a, b)
;      0 -> las teclas (tec_mapa: 128 octetos, 1 = pulsada; puntero suyo)
;      1 el ratón (a = int[3]: x, y en la pantalla, botones)
;      2 un mando de HSPJOY (a = número 0-5, FFh = el del jugador 1, FEh = el
;        del 2,
;        b = 32 octetos: su entrada de la tabla) -> 1 si está
;      3 -> la dirección lineal 0 (puntero suyo: los tics de la BIOS)
;      4 las teclas que se han pulsado desde la última vez (a = 128 octetos;
;        tec_dx, que apunta el gancho de INT 15h): así no se pierde una
;        pulsación corta entre dos lecturas
;    HispaDXTic(función)   una función del programa (stdcall, cuatro
;      argumentos) a la que se llama cada vez que pide un mensaje
;      (GetMessageA, PeekMessageA) y mientras espera uno: ahí WINMM mezcla el
;      sonido y lanza sus temporizadores, sin hilos.
; =============================================================================

HXP_ANCHO   equ 0                   ; lo que se le da al programa al tomar
HXP_ALTO    equ 4
HXP_PASO    equ 8
HXP_BITS    equ 12
HXP_OCT     equ 16
HXP_LFB     equ 20                  ; la pantalla (puntero suyo)
HXP_MASC    equ 24                  ; tamaño y posición de rojo, verde, azul

u_HispaDXPantalla:
        mov eax, [ebp]
        cmp eax, 0
        je .tomar
        cmp eax, 1
        je .soltar
        cmp eax, 2
        je .paleta
        cmp eax, 3
        je .retrazo
        cmp eax, 4
        je .suya
        cmp eax, 5
        je .ahora
        cmp eax, 6
        je .como
        cmp eax, 7
        je .info
        jmp u_cero
.info:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        call .rellena
        mov dword [edi+HXP_LFB], 0
        jmp u_uno
.tomar:
        cmp byte [vid_tipo], 1
        jne u_cero
        mov eax, [dx_prog]
        test eax, eax
        jz .t1
        cmp eax, edx
        jne u_cero
.t1:
        mov eax, [ebp+4]
        call w32_ven
        jz u_cero
        push ebp
        push edx
        mov ebp, eax
        call ven_activar
        call ven_tope
        pop edx
        pop ebp
        mov [dx_ven], eax
        mov [dx_prog], edx
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        call .rellena
        jmp u_uno
.rellena:
        mov eax, [vid_ancho]
        mov [edi+HXP_ANCHO], eax
        mov eax, [vid_alto]
        mov [edi+HXP_ALTO], eax
        mov eax, [vid_paso]
        mov [edi+HXP_PASO], eax
        movzx eax, byte [vid_bits]
        mov [edi+HXP_BITS], eax
        movzx eax, byte [vid_octetos]
        mov [edi+HXP_OCT], eax
        mov eax, [vid_ptr]
        sub eax, [edx+PE_DELTA]
        mov [edi+HXP_LFB], eax
        mov eax, [vid_masc]
        mov [edi+HXP_MASC], eax
        mov ax, [vid_masc+4]
        mov [edi+HXP_MASC+4], ax
        mov word [edi+HXP_MASC+6], 0
        ret
.soltar:
        cmp [dx_prog], edx
        jne u_cero
        call dx_soltar
        jmp u_uno
.paleta:
        cmp [dx_prog], edx
        jne u_cero
        cmp byte [vid_octetos], 1
        jne u_cero
        mov eax, [ebp+4]
        mov ecx, [ebp+8]
        mov esi, [ebp+12]
        add esi, [edx+PE_DELTA]
        cmp eax, 256
        jae u_cero
        lea ebx, [eax+ecx]
        cmp ebx, 256
        jbe .p1
        mov ecx, 256
        sub ecx, eax
.p1:
        jecxz .p3
        mov dx, 0x3C8
        out dx, al
        inc dx
.p2:
        lodsb
        shr al, 2
        out dx, al
        lodsb
        shr al, 2
        out dx, al
        lodsb
        shr al, 2
        out dx, al
        inc esi
        loop .p2
.p3:
        jmp u_uno
.retrazo:
        mov dx, 0x3DA               ; si ya está en el retrazo, a que acabe
        mov ecx, 0x100000           ; (con un tope: no todas las tarjetas lo dan)
.r1:
        in al, dx
        test al, 8
        jz .r2
        loop .r1
        jmp u_uno
.r2:
        mov ecx, 0x100000
.r3:
        in al, dx
        test al, 8
        jnz u_uno
        loop .r3
        jmp u_uno
.suya:
        cmp [dx_prog], edx
        jne u_cero
        jmp u_uno
.ahora:
        mov dx, 0x3DA
        in al, dx
        test al, 8
        jz u_cero
        jmp u_uno
.como:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov edi, dx_gw
        mov ecx, 6
        rep movsd
        jmp u_uno

; (interna) devuelve la pantalla a Hispa Ventanas: su paleta y lo pintado
dx_soltar:
        pushad
        cmp dword [dx_prog], 0
        je .x
        mov dword [dx_prog], 0
        mov dword [dx_ven], 0
        mov dword [dx_gw], 0
        mov dword [dx_dw], 0
        call vid_paleta
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        call vid_presentar
.x:
        popad
        ret

; (desde vid_presentar, con la pantalla tomada) ZF=1: no se lleva nada a la
; pantalla. Si la ventana del programa ya no es la activa, se le quita (y se
; pinta todo: ZF=0 con la pantalla ya devuelta)
dx_tapa:
        push eax
        mov eax, [dx_ven]
        cmp eax, [activa]
        je .x
        call dx_soltar
        or eax, 1                   ; ZF=0
.x:
        pop eax
        ret

; (pe_limpiar) el programa EDX ha acabado: si tenía la pantalla, se devuelve
dx_fin_prog:
        cmp [dx_prog], edx
        jne .x
        call dx_soltar
.x:
        ret

; (w32_traducir) EDX = lParam de un mensaje del ratón para el programa ESI:
; si tiene la pantalla y ha dicho cómo se ve, a las coordenadas de su modo
; (se cuenta con que su ventana empieza en 0,0, como en Windows)
dx_raton:
        cmp [dx_prog], esi
        jne .x
        cmp dword [dx_dw], 0
        je .x
        cmp dword [dx_dh], 0
        je .x
        push eax
        push ebx
        push ecx
        push edi
        mov edi, edx
        movsx eax, di
        sub eax, [dx_ox]
        imul eax, [dx_gw]
        cdq
        idiv dword [dx_dw]
        mov ecx, [dx_gw]
        call .acota
        mov ebx, eax
        mov eax, edi
        sar eax, 16
        sub eax, [dx_oy]
        imul eax, [dx_gh]
        cdq
        idiv dword [dx_dh]
        mov ecx, [dx_gh]
        call .acota
        shl eax, 16
        mov ax, bx
        mov edx, eax
        pop edi
        pop ecx
        pop ebx
        pop eax
.x:
        ret
.acota:
        test eax, eax
        jns .a1
        xor eax, eax
.a1:
        cmp eax, ecx
        jl .a2
        lea eax, [ecx-1]
.a2:
        ret

; (u_GetSystemMetrics) ECX = cuál, EDX = el programa: con la pantalla
; tomada, SM_CXSCREEN y SM_CYSCREEN son las del modo del juego. CF=1 y EAX
; si es eso
dx_metricas:
        cmp [dx_prog], edx
        jne .no
        cmp dword [dx_gw], 0
        je .no
        mov eax, [dx_gw]
        cmp ecx, 0
        je .si
        mov eax, [dx_gh]
        cmp ecx, 1
        je .si
.no:
        clc
        ret
.si:
        stc
        ret

u_HispaDXSonido:
        mov eax, [ebp]
        cmp eax, 0
        je .info
        cmp eax, 1
        je .tocar
        cmp eax, 2
        je .sitio
        cmp eax, 3
        je .parar
        jmp u_cero
.info:
        push edx
        call snd_info
        pop edx
        ret
.tocar:
        mov esi, [ebp+4]
        add esi, [edx+PE_DELTA]
        mov ecx, [ebp+8]
        mov edx, [ebp+12]
        mov eax, edx
        shr eax, 24
        and edx, 0xFFFFFF
        call snd_pcm2
        jc u_cero
        jmp u_uno
.sitio:
        or eax, -1
        cmp byte [snd_hay], 0
        je .x
        cmp word [snd_ver], 3
        jb .s4
        mov eax, 0x0800
        call snd_orden
        movzx eax, al
        ret
.s4:
        mov eax, 1                  ; (HSPSND viejo: no lo sabe; de uno en uno)
        ret
.parar:
        mov eax, 0x0300
        call snd_orden
        jmp u_uno
.x:
        ret

u_HispaDXEntrada:
        mov eax, [ebp]
        cmp eax, 0
        je .teclas
        cmp eax, 1
        je .raton
        cmp eax, 2
        je .mando
        cmp eax, 3
        je .cero
        cmp eax, 4
        je .pulsadas
        jmp u_cero
.pulsadas:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        xor ecx, ecx
.pu1:
        xor al, al
        xchg al, [tec_dx+ecx]
        mov [edi+ecx], al
        inc ecx
        cmp ecx, 128
        jb .pu1
        jmp u_uno
.teclas:
        mov eax, tec_mapa
        sub eax, [edx+PE_DELTA]
        ret
.cero:
        mov eax, [cero]
        sub eax, [edx+PE_DELTA]
        ret
.raton:
        mov edi, [ebp+4]
        add edi, [edx+PE_DELTA]
        mov eax, [rat_x]
        mov [edi], eax
        mov eax, [rat_y]
        mov [edi+4], eax
        movzx eax, byte [rat_boton]
        mov [edi+8], eax
        jmp u_uno
.mando:
        mov edi, [ebp+8]
        add edi, [edx+PE_DELTA]
        push edi
        mov dword [rmcs+0x1C], 0xD900   ; ¿está HSPJOY?
        mov bl, 0x2F
        call rm_int
        pop edi
        cmp byte [rmcs+0x1C], 0xFF
        jne u_cero
        cmp word [rmcs+0x10], 0x484A
        jne u_cero
        mov eax, [ebp+4]
        cmp eax, 0xFE               ; FFh: el del jugador 1; FEh: el del 2
        jb .m1
        movzx esi, word [rmcs+0x22] ; ES:DI = sus datos
        shl esi, 4
        movzx ecx, word [rmcs]
        add esi, ecx
        add esi, [cero]
        neg eax                     ; FFh -> 0Dh, FEh -> 0Eh
        add eax, 0xFF + 0x0D
        movzx eax, byte [esi+eax]
        cmp eax, 0xFE
        jae u_cero
.m1:
        cmp eax, 5
        ja u_cero
        mov [rmcs+0x14], eax        ; DL = el mando
        mov dword [rmcs+0x1C], 0xD902
        push edi
        mov bl, 0x2F
        call rm_int
        pop edi
        test byte [rmcs+0x20], 1    ; CF: no hay ese número
        jnz u_cero
        cmp byte [rmcs+0x1C], 0xFF
        jne u_cero
        movzx esi, word [rmcs+0x22]
        shl esi, 4
        movzx eax, word [rmcs]
        add esi, eax
        add esi, [cero]
        test byte [esi+1], 1        ; presente
        jz u_cero
        mov ecx, 8
        rep movsd
        jmp u_uno

u_HispaDXTic:
        mov eax, [ebp]
        mov [edx+PE_DXTIC], eax
        jmp u_uno

; (w32_tomar) la función de HispaDXTic del programa EDX, si la ha dado.
; Conserva todos los registros
dx_tic:
        cmp dword [edx+PE_DXTIC], 0
        je .x
        push eax
        mov eax, [edx+PE_DXTIC]
        call pe_llamar
        pop eax
.x:
        ret

; (pe_abrir) EBX = el acceso, EDX = el programa: si el acceso dice su carpeta
; («Carpeta=»), la de trabajo pasa a ser esa (los juegos de la época leen sus
; datos así: «DDRAW.GIF»). Sin ella, se queda como estaba (como hasta ahora:
; los diálogos de archivos de los programas empiezan en ella). Si no se puede,
; también. Conserva los registros
dx_carpeta:
        cmp byte [ebx+AC_DIR], 0
        je .x
        pushad
        lea esi, [ebx+AC_DIR]
        call dos_ir_carpeta
        popad
.x:
        ret

; [DX] ShowCursor(mostrar) -> la cuenta (el cursor de Hispa Ventanas sigue
; igual; en pantalla completa no se ve) y ClipCursor (nada)
u_ShowCursor:
        mov eax, [dx_cursor]
        cmp dword [ebp], 0
        je .menos
        inc eax
        jmp .x
.menos:
        dec eax
.x:
        mov [dx_cursor], eax
        ret
