; =============================================================================
;  Hispa Ventanas - [HISPA-486] GDI32 de la fase 3d
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Fase 3d de Hispa-486 (docs/486/HISPA-486.md): el GDI de los programas
;  sencillos de Windows 95/98. Va en la parte alta de VENTANAS.COM, detrás de
;  win32m.inc. Sala limpia: hecho a partir de la documentación pública del
;  API Win32 (qué hace cada función), sin mirar código de nadie.
;
;  Cómo funciona:
;  - Un HDC es una ficha (DC_*, en el bloque del programa). Cada función de
;    dibujo la «abre» (wg_abrir): deja en la región de Hispa Ventanas (reg,
;    reg_n) los rectángulos donde se puede pintar, ya en coordenadas de la
;    superficie, y en wg_ox, wg_oy lo que hay que sumar a las coordenadas
;    lógicas del programa (el origen de su zona, el de la vista y el de la
;    ventana). Dibuja con las primitivas de aquí (wg_punto, wg_tramo...), que
;    escriben en la superficie con la ROP2 del contexto, y cierra (wg_cerrar).
;  - Los contextos de memoria (CreateCompatibleDC) dibujan en su mapa de
;    bits: al abrir, la superficie de Hispa Ventanas pasa a ser el mapa
;    (sup_ptr, sup_ancho, sup_alto) y al cerrar vuelve a ser la pantalla.
;    Los mapas son de un octeto por punto, con los colores de la paleta de
;    Hispa Ventanas (como la pantalla); los monocromos, de 0 y 1.
;  - Los colores: COLORREF -> el índice de la paleta (el gris o el del cubo
;    más cercano, lo mismo que w32_color, por tablas). Las ROP (SetROP2 y las
;    de BitBlt) se hacen con los colores RGB de los puntos, como en una
;    pantalla de color verdadero, y el resultado vuelve a la paleta: así
;    funcionan las máscaras (SRCAND y SRCPAINT) y el XOR.
;  - Las letras: la de Hispa Ventanas (8×16) a cualquier tamaño (cada punto
;    de la letra se estira), con negrita, cursiva, subrayado y tachado.
;  - Los DIB (SetDIBitsToDevice, StretchDIBits, CreateDIBSection...) de 1, 4,
;    8, 16, 24 y 32 bits se pasan a la paleta fila a fila al dibujarlos.
;  - Las regiones son listas de hasta RG_MAX rectángulos (las elípticas y las
;    de polígono, por franjas).
; =============================================================================

; ---- tipos de objeto (GO_TIPO) ----
GT_PLUMA    equ 1
GT_PINCEL   equ 2
GT_LETRA    equ 3
GT_SERIE    equ 4                   ; (la paleta de serie)
GT_MAPA     equ 5
GT_REGION   equ 6
GT_PALETA   equ 7
GT_ICONO    equ 8
GT_CURSOR   equ 9
; campos de más de un objeto
GO_A        equ 8
GO_B        equ 12
GO_C        equ 16
GO_D        equ 20
GO_E        equ 24
GO_F        equ 28
; mapa de bits: GO_A los puntos, GO_B ancho, GO_C alto, GO_D mango de DPMI
; (0: del montón), GO_E su DIB (DB_*, de CreateDIBSection; 0: no es), GO_ANCHO
; los bits que dice tener; GO_ESTILO bit 0: monocromo
; letra: GO_A alto de la celda, GO_B ancho, GO_C peso, GO_D bit 0 cursiva,
; bit 1 subrayado, bit 2 tachado; GO_E el lfHeight pedido, GO_F el ancho pedido
; pincel: GO_ESTILO 0 liso, 1 ninguno, 2 rayado (GO_A = cuál), 3 dibujo
; (GO_A = 64 octetos del montón; GO_B = 1 si es de un mapa monocromo)
; región: GO_A = su bloque (RG_*); paleta: GO_A = su bloque (número y colores)
; icono y cursor: GO_A = sus dibujos (IC_*), GO_B y GO_C el punto caliente,
; GO_D la forma de cursor de Hispa Ventanas (CUR_*); GO_ESTILO bit 0: de serie
RG_MAX      equ 64
RG_N        equ 0
RG_R        equ 4
RG_TAM      equ RG_R + RG_MAX * 16
; el DIB de un mapa (o el de una llamada, en wg_dibd)
DB_BPP      equ 0
DB_AN       equ 4
DB_AL       equ 8
DB_ABAJO    equ 12                  ; 1: la primera fila es la de abajo
DB_PASO     equ 16
DB_BITS     equ 20                  ; los puntos (puntero de Hispa Ventanas)
DB_MANGO    equ 24                  ; (CreateDIBSection) su bloque de DPMI
DB_INV      equ 28                  ; 1: DB_CERCA vale
DB_INI      equ 32                  ; la primera línea que hay (de las de DB_AL)
DB_N        equ 36                  ;   y cuántas
DB_565      equ 40                  ; (16 bits) 1: 5-6-5; 0: 5-5-5
DB_COLORES  equ 48                  ; 256 COLORREF
DB_XLAT     equ DB_COLORES + 1024   ; su color -> el de la paleta
DB_CERCA    equ DB_XLAT + 256       ; el de la paleta -> el suyo más cercano
DB_TAM      equ DB_CERCA + 256
; los dibujos de un icono (o cursor)
IC_32       equ 0                   ; 32×32, un octeto por punto (CLAVE: nada)
IC_4B       equ 1024                ; 32×32 a 4 bits, los 16 colores (barra.inc)
IC_CUR      equ 1536                ; 16×18 para el cursor
IC_TAM      equ IC_CUR + CUR_ANCHO * CUR_ALTO
; (en el bloque del programa, PE_*) los bloques de DPMI de sus mapas grandes
WG_BLQ_MAX  equ 32
MAPA_MONTON equ 48 * 1024           ; hasta aquí, del montón; más, de DPMI
WG_FILAS    equ 1024                ; filas de una figura, como mucho
WG_PUNTOS   equ 512                ; puntos de un polígono, como mucho
WG_FILA     equ 2048                ; puntos de una fila de un DIB, como mucho
WG_TBUF     equ 1024                ; letras de un texto, como mucho

; =============================================================================
;  Las fichas: objetos y contextos
; =============================================================================
; ---- EAX=objeto de GDI EDX=programa -> ESI=su ficha; CF=1 si no vale ----
w32_obj:
        push eax
        sub eax, W32_HGDI
        jb .no
        cmp eax, GO_MAX
        jae .no
        shl eax, 5
        mov esi, [edx+PE_W32]
        lea esi, [esi+eax+WB_OBJ]
        cmp byte [esi+GO_TIPO], 0
        je .no
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret
; ---- lo mismo, de un tipo AL ----
wg_obj_de:
        call w32_obj
        jc .x
        cmp [esi+GO_TIPO], al
        je .x
        stc
.x:
        ret
; ---- EAX=HDC EDX=programa -> EDI=su ficha; CF=1 si no vale ----
w32_dc:
        push eax
        sub eax, W32_HDC
        jb .no
        cmp eax, DC_MAX
        jae .no
        shl eax, 7
        mov edi, [edx+PE_W32]
        lea edi, [edi+eax+WB_DC]
        cmp dword [edi+DC_USO], 0
        je .no
        cmp dword [edi+DC_USO], 3   ; (los de memoria no tienen ventana)
        je .si
        mov eax, [edi+DC_VEN]
        cmp dword [eax+V_USO], 1
        jne .no
.si:
        pop eax
        clc
        ret
.no:
        pop eax
        stc
        ret
; ---- un contexto nuevo para la ventana EBP del programa EDX, del tipo ECX
; (DC_USO) -> EDI=su ficha EAX=el HDC; CF=1 si no quedan ----
w32_dc_nuevo:
        mov edi, [edx+PE_W32]
        add edi, WB_DC
        xor eax, eax
.b:
        cmp dword [edi+DC_USO], 0
        je .hay
        add edi, DC_TAM
        inc eax
        cmp eax, DC_MAX
        jb .b
        xor eax, eax
        stc
        ret
.hay:
        push eax
        push ecx
        push edi
        xor eax, eax
        mov ecx, DC_TAM / 4
        rep stosd
        pop edi
        pop ecx
        pop eax
        mov [edi+DC_USO], ecx
        mov [edi+DC_VEN], ebp
        mov dword [edi+DC_CFONDO], 0xFFFFFF
        mov dword [edi+DC_MODO], 2
        mov dword [edi+DC_PLUMA], W32_HGDI + 7
        mov dword [edi+DC_PINCEL], W32_HGDI + 0
        mov dword [edi+DC_LETRA], W32_HGDI + 13
        mov dword [edi+DC_PALETA], W32_HGDI + 15
        mov dword [edi+DC_BMP], W32_HGDI + WG_MAPA0
        mov dword [edi+DC_ROP2], 13
        mov dword [edi+DC_RELLENO], 1
        mov dword [edi+DC_ESTIRA], 1
        add eax, W32_HDC
        clc
        ret
; ---- suelta el contexto EDI (su recorte y lo guardado con SaveDC) ----
wg_dc_soltar:
        push eax
        push esi
        mov eax, [edi+DC_RECORTE]
        call mem_soltar
        mov esi, [edi+DC_GUARDA]
.g:
        test esi, esi
        jz .x
        mov eax, [esi+DC_RECORTE+4]
        call mem_soltar
        mov eax, esi
        mov esi, [esi]
        call mem_soltar
        jmp .g
.x:
        mov dword [edi+DC_USO], 0
        mov dword [edi+DC_RECORTE], 0
        mov dword [edi+DC_GUARDA], 0
        pop esi
        pop eax
        ret

; =============================================================================
;  Los colores
; =============================================================================
; ---- (una vez) las tablas de wg_idx: el cubo y los grises, como w32_color ----
wg_tablas:
        cmp byte [wg_tabla_ok], 0
        jne .x
        pushad
        xor ebx, ebx
.l:
        lea eax, [ebx+25]
        mov cl, 51
        div cl
        mov [wg_q6+ebx], al
        mov eax, ebx                ; el gris de ese valor
        shl eax, 8
        mov al, bl
        shl eax, 8
        mov al, bl
        call w32_color
        mov [wg_gris+ebx], al
        inc ebx
        cmp ebx, 256
        jb .l
        mov byte [wg_tabla_ok], 1
        popad
.x:
        ret
; ---- EAX=RGB (00BBGGRR) -> EAX=el color del destino que se está pintando
; (el de la paleta; 0 o 1 si es monocromo) ----
wg_idx:
        push ebx
        push ecx
        push edx
        movzx ebx, al
        movzx ecx, ah
        shr eax, 16
        movzx edx, al
        cmp byte [wg_mono], 0
        jne .mono
        cmp ebx, ecx
        jne .cubo
        cmp ebx, edx
        jne .cubo
        movzx eax, byte [wg_gris+ebx]
        jmp .x
.cubo:
        movzx eax, byte [wg_q6+ebx]
        imul eax, 36
        movzx ebx, byte [wg_q6+ecx]
        imul ebx, 6
        add eax, ebx
        movzx ebx, byte [wg_q6+edx]
        lea eax, [eax+ebx+16]
.x:
        pop edx
        pop ecx
        pop ebx
        ret
.mono:
        imul ebx, 30                ; claro u oscuro
        imul ecx, 59
        imul edx, 11
        lea eax, [ebx+ecx]
        add eax, edx
        cmp eax, 128 * 100
        setae al
        movzx eax, al
        jmp .x
; ---- AL=un color del destino -> EAX=su RGB ----
wg_rgb:
        cmp byte [wg_mono], 0
        jne .mono
        movzx eax, al
        mov eax, [pal_rgb+eax*4]
        and eax, 0xFFFFFF
        ret
.mono:
        test al, al
        setnz al
        movzx eax, al
        neg eax
        and eax, 0xFFFFFF
        ret
; ---- EAX=COLORREF (también PALETTEINDEX y PALETTERGB) -> EAX=RGB, con la
; paleta del contexto EDI y el programa EDX ----
wg_colref:
        cmp eax, 0x01000000
        jb .x
        cmp eax, 0x02000000
        jae .rgb
        push esi
        push ecx
        movzx ecx, ax               ; PALETTEINDEX(n): el n de su paleta (con la de serie, el de Hispa Ventanas)
        mov eax, [edi+DC_PALETA]
        call w32_obj
        jc .sin
        cmp byte [esi+GO_TIPO], GT_PALETA
        jne .sin
        mov esi, [esi+GO_A]
        cmp esi, wg_pal20_blq       ; la de serie: la paleta de Hispa Ventanas
        je .hv                      ; (como la entendía w32_color: el Navegador)
        cmp ecx, [esi]
        jae .sin
        mov eax, [esi+4+ecx*4]
        and eax, 0xFFFFFF
        pop ecx
        pop esi
        ret
.hv:
        cmp ecx, 256
        jae .sin
        mov eax, [pal_rgb+ecx*4]
        and eax, 0xFFFFFF
        pop ecx
        pop esi
        ret
.sin:
        mov eax, ecx                ; (sin paleta: los 20 de serie)
        cmp eax, 20
        jb .s20
        xor eax, eax
.s20:
        mov eax, [wg_pal20+eax*4]
        pop ecx
        pop esi
        ret
.rgb:
        and eax, 0xFFFFFF
.x:
        ret
; ---- lo mismo, hasta el color del destino ----
wg_col:
        call wg_colref
        jmp wg_idx

; los 20 colores de serie de la paleta de Windows
wg_pal20:
        dd 0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xC0C0C0
        dd 0xC0DCC0, 0xF0CAA6, 0xF0FBFF, 0xA4A0A0, 0x808080, 0x0000FF, 0x00FF00, 0x00FFFF
        dd 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF

; =============================================================================
;  Abrir y cerrar un contexto para pintar
; =============================================================================
; ---- EAX=HDC EDX=programa -> EDI=su ficha, con la región, el origen, los
; colores del texto y del fondo y la ROP2 puestos; CF=1 si no vale ----
wg_abrir:
        call w32_dc
        jc .x
        pushad
        call wg_tablas
        mov [wg_dc], edi
        mov [wg_prog], edx
        mov byte [wg_mem], 0
        mov byte [wg_mono], 0
        cmp dword [edi+DC_USO], 3
        je .memoria
        mov ebp, [edi+DC_VEN]
        xor al, al
        call reg_ventana
        cmp dword [edi+DC_USO], 2   ; BeginPaint: solo lo que había que pintar
        jne .o
        mov eax, [edi+DC_X0]
        mov ebx, [edi+DC_Y0]
        mov ecx, [edi+DC_X1]
        mov edx, [edi+DC_Y1]
        call reg_cortar
.o:
        call w32_cli                ; (sin las barras de desplazamiento)
        mov [wg_dox], eax
        mov [wg_doy], ebx
        add ecx, eax
        add edx, ebx
        call reg_cortar
        call wg_recorte
        call dc_origen              ; (la presenta y quita el cursor)
        jmp .comun
.memoria:
        mov eax, [sup_ptr]          ; la superficie pasa a ser su mapa
        mov [wg_sptr], eax
        mov eax, [sup_ancho]
        mov [wg_san], eax
        mov eax, [sup_alto]
        mov [wg_sal], eax
        mov byte [wg_mem], 1
        mov eax, [edi+DC_BMP]
        mov edx, [wg_prog]
        call w32_obj
        mov eax, [esi+GO_A]
        mov [sup_ptr], eax
        mov ecx, [esi+GO_B]
        mov [sup_ancho], ecx
        mov edx, [esi+GO_C]
        mov [sup_alto], edx
        mov al, [esi+GO_ESTILO]
        and al, 1
        mov [wg_mono], al
        mov [wg_mapa], esi
        cmp dword [esi+GO_E], 0     ; un DIB: primero, sus puntos al mapa
        je .m1
        call wg_dib_a_mapa
.m1:
        mov dword [reg_n], 0
        xor eax, eax
        xor ebx, ebx
        call reg_anadir
        mov [wg_dox], eax
        mov [wg_doy], eax
        mov [dc_ox], eax
        mov [dc_oy], eax
        call wg_recorte
.comun:
        mov edi, [wg_dc]
        mov eax, [wg_dox]
        add eax, [edi+DC_DX]
        sub eax, [edi+DC_WX]
        mov [wg_ox], eax
        mov eax, [wg_doy]
        add eax, [edi+DC_DY]
        sub eax, [edi+DC_WY]
        mov [wg_oy], eax
        mov edx, [wg_prog]
        mov eax, [edi+DC_CTEXTO]
        call wg_col
        mov [wg_txt], eax
        mov eax, [edi+DC_CFONDO]
        call wg_col
        mov [wg_bk], eax
        mov eax, [edi+DC_ROP2]
        mov [wg_rop], eax
        mov eax, [edi+DC_BRX]
        add eax, [wg_dox]
        mov [wg_bx], eax
        mov eax, [edi+DC_BRY]
        add eax, [wg_doy]
        mov [wg_by], eax
        mov byte [gdi_negrita], 0
        popad
        clc
.x:
        ret
; ---- cierra lo que se abrió con wg_abrir (EDI=el contexto) ----
wg_cerrar:
        cmp byte [wg_mem], 0
        je .x
        pushad
        mov esi, [wg_mapa]
        cmp dword [esi+GO_E], 0     ; un DIB: lo pintado vuelve a sus puntos
        je .m
        call wg_mapa_a_dib
.m:
        mov eax, [wg_sptr]
        mov [sup_ptr], eax
        mov eax, [wg_san]
        mov [sup_ancho], eax
        mov eax, [wg_sal]
        mov [sup_alto], eax
        mov byte [wg_mem], 0
        mov byte [wg_mono], 0
        popad
.x:
        jmp gdi_recorte_total
; ---- (interna) la región en curso, cortada con el recorte del contexto
; [wg_dc] (en coordenadas del contexto: se le suma wg_dox, wg_doy) ----
wg_recorte:
        pushad
        mov edi, [wg_dc]
        mov esi, [edi+DC_RECORTE]
        test esi, esi
        jz .x
        mov eax, [wg_dox]
        mov ebx, [wg_doy]
        call wg_reg_por
.x:
        popad
        ret
; ---- (interna) la región en curso := región en curso ∩ la región ESI
; (bloque RG_*) desplazada EAX, EBX ----
wg_reg_por:
        pushad
        mov [wg_t0], eax
        mov [wg_t1], ebx
        mov ecx, [reg_n]
        shl ecx, 2
        push esi
        mov esi, reg
        mov edi, wg_regtmp
        rep movsd
        pop esi
        mov ebp, [reg_n]
        mov dword [reg_n], 0
        mov edi, [esi+RG_N]
        add esi, RG_R
.r:
        test edi, edi
        jz .x
        push ebp
        mov ebx, wg_regtmp
.t:
        test ebp, ebp
        jz .rs
        mov eax, [esi]
        add eax, [wg_t0]
        cmp eax, [ebx]
        jge .a
        mov eax, [ebx]
.a:
        mov ecx, [esi+8]
        add ecx, [wg_t0]
        cmp ecx, [ebx+8]
        jle .c
        mov ecx, [ebx+8]
.c:
        mov edx, [esi+12]
        add edx, [wg_t1]
        cmp edx, [ebx+12]
        jle .d
        mov edx, [ebx+12]
.d:
        push ebx
        mov ebx, [esi+4]
        add ebx, [wg_t1]
        mov [wg_t2], ebx
        pop ebx
        push ebx
        mov ebx, [ebx+4]
        cmp ebx, [wg_t2]
        jge .b
        mov ebx, [wg_t2]
.b:
        call reg_anadir
        pop ebx
        add ebx, 16
        dec ebp
        jmp .t
.rs:
        pop ebp
        add esi, 16
        dec edi
        jmp .r
.x:
        popad
        ret

; =============================================================================
;  Las primitivas (coordenadas de la superficie; recortan con la región)
; =============================================================================
; ---- AL=color EDI=el punto: lo pone con la ROP2 [wg_rop] ----
wg_px:
        cmp byte [wg_rop], 13       ; R2_COPYPEN
        jne .rop
        mov [edi], al
        ret
.rop:
        push eax
        push ebx
        push ecx
        push esi
        call wg_rgb
        mov ebx, eax                ; P
        mov esi, eax
        mov al, [edi]
        call wg_rgb                 ; D
        mov ecx, [wg_rop]           ; la tabla de la ROP2 (P, D) como la de
        dec ecx                     ; una ROP3 (P, S, D) que no mira S
        and ecx, 15
        movzx ecx, byte [wg_r2a3+ecx]
        call wg_rop3
        call wg_idx
        mov [edi], al
        pop esi
        pop ecx
        pop ebx
        pop eax
        ret
; las ROP2 (1 a 16) como tablas de ROP3
wg_r2a3:
        db 0x00, 0x05, 0x0A, 0x0F, 0x50, 0x55, 0x5A, 0x5F
        db 0xA0, 0xA5, 0xAA, 0xAF, 0xF0, 0xF5, 0xFA, 0xFF
; ---- la ROP3 de tabla CL con P=EBX S=ESI D=EAX (RGB) -> EAX ----
wg_rop3:
        cmp cl, 0xCC
        je .s
        cmp cl, 0xF0
        je .p
        push edx
        push edi
        push ebp
        mov ebp, eax                ; D
        xor edx, edx
        xor edi, edi
.l:
        bt ecx, edi
        jnc .sig
        or eax, -1
        test edi, 4
        jnz .p1
        mov eax, ebx
        not eax
        jmp .p2
.p1:
        and eax, ebx
.p2:
        push eax
        mov eax, esi
        test edi, 2
        jnz .s1
        not eax
.s1:
        and [esp], eax
        mov eax, ebp
        test edi, 1
        jnz .d1
        not eax
.d1:
        and eax, [esp]
        add esp, 4
        or edx, eax
.sig:
        inc edi
        cmp edi, 8
        jb .l
        mov eax, edx
        and eax, 0xFFFFFF
        pop ebp
        pop edi
        pop edx
        ret
.s:
        mov eax, esi
        ret
.p:
        mov eax, ebx
        ret
; ---- EAX=x EBX=y: el punto del color [wg_c] ----
wg_punto:
        push ecx
        push esi
        push edi
        mov esi, reg
        mov ecx, [reg_n]
.l:
        test ecx, ecx
        jz .x
        cmp eax, [esi]
        jl .s
        cmp eax, [esi+8]
        jge .s
        cmp ebx, [esi+4]
        jl .s
        cmp ebx, [esi+12]
        jge .s
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
        add edi, eax
        push eax
        mov al, [wg_c]
        call wg_px
        pop eax
        jmp .x
.s:
        add esi, 16
        dec ecx
        jmp .l
.x:
        pop edi
        pop esi
        pop ecx
        ret
; ---- EAX=x0 ECX=x1 (fuera) EBX=y: un tramo del color [wg_c] ----
wg_tramo_c:
        mov byte [wg_tmodo], 0
        jmp wg_tramo0
; ---- lo mismo, con el pincel preparado (wg_ppincel) ----
wg_tramo:
        mov byte [wg_tmodo], 1
        cmp dword [wg_pm], 0
        jne wg_tramo0
        push eax
        mov eax, [wg_pt]
        mov [wg_c], eax
        pop eax
        mov byte [wg_tmodo], 0
wg_tramo0:
        cmp eax, ecx
        jge .r
        pushad
        mov esi, reg
        mov ebp, [reg_n]
.l:
        test ebp, ebp
        jz .x
        cmp ebx, [esi+4]
        jl .s
        cmp ebx, [esi+12]
        jge .s
        push eax
        push ecx
        cmp eax, [esi]
        jge .a
        mov eax, [esi]
.a:
        cmp ecx, [esi+8]
        jle .b
        mov ecx, [esi+8]
.b:
        cmp eax, ecx
        jge .n
        mov edi, ebx
        imul edi, [sup_ancho]
        add edi, [sup_ptr]
        add edi, eax
        sub ecx, eax
        cmp byte [wg_tmodo], 0
        jne .pinc
        cmp byte [wg_rop], 13
        jne .cr
        mov al, [wg_c]
        rep stosb
        jmp .n
.cr:
        mov al, [wg_c]
.cr1:
        call wg_px
        inc edi
        dec ecx
        jnz .cr1
        jmp .n
.pinc:
        call wg_tramo_pincel
.n:
        pop ecx
        pop eax
.s:
        add esi, 16
        dec ebp
        jmp .l
.x:
        popad
.r:
        ret
; (de wg_tramo) ECX puntos desde EDI (que es el punto EAX, EBX) con el
; pincel rayado o de dibujo
wg_tramo_pincel:
        push edx
        push esi
        mov edx, ebx
        sub edx, [wg_by]
        and edx, 7
        mov esi, eax
        sub esi, [wg_bx]
.p:
        push ecx
        mov ecx, esi
        and ecx, 7
        cmp dword [wg_pm], 1
        jne .dib
        mov al, 0x80                ; rayado: el punto de su máscara
        shr al, cl
        test [wg_pmask+edx], al
        jz .fondo
        mov al, [wg_pt]
        jmp .pon
.fondo:
        cmp dword [wg_pf], -1
        je .sig
        mov al, [wg_pf]
        jmp .pon
.dib:
        lea eax, [edx*8+ecx]
        mov al, [wg_patbuf+eax]
.pon:
        call wg_px
.sig:
        pop ecx
        inc edi
        inc esi
        dec ecx
        jnz .p
        pop esi
        pop edx
        ret

; =============================================================================
;  Plumas y pinceles
; =============================================================================
; ---- prepara la pluma EAX (del contexto abierto): CF=1 si no pinta ----
wg_ppluma:
        pushad
        mov edx, [wg_prog]
        mov edi, [wg_dc]
        call w32_obj
        jc .no
        cmp byte [esi+GO_TIPO], GT_PLUMA
        jne .no
        movzx eax, byte [esi+GO_ESTILO]
        and eax, 15
        cmp eax, 5                  ; PS_NULL
        je .no
        cmp eax, 6                  ; PS_INSIDEFRAME: lisa
        jb .e
        xor eax, eax
.e:
        movzx ecx, word [esi+GO_ANCHO]
        cmp ecx, 1
        jbe .a1
        xor eax, eax                ; (las anchas, lisas: como en Windows 95)
        cmp ecx, 64
        jbe .a2
        mov ecx, 64
        jmp .a2
.a1:
        mov ecx, 1
.a2:
        mov [wg_pa], ecx
        mov [wg_pe], eax
        mov ecx, [wg_estilos+eax*8]
        mov [wg_pbits], ecx
        mov ecx, [wg_estilos+eax*8+4]
        mov [wg_plen], ecx
        mov dword [wg_ppos], 0
        mov eax, [esi+GO_COLOR]
        call wg_col
        mov [wg_pcol], eax
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; los estilos de la pluma: los puntos (bit 0, el primero) y cuántos son
wg_estilos:
        dd 1, 1                             ; PS_SOLID
        dd 0x0003FFFF, 24                   ; PS_DASH: 18 y 6
        dd 0x00000007, 6                    ; PS_DOT: 3 y 3
        dd 0x0071FFFF, 30 - 2               ; PS_DASHDOT: 18, 3, 3, 3 (aprox.)
        dd 0x01C71FFF, 30                   ; PS_DASHDOTDOT: 13 3 3 3 3 3 (aprox.)
; ---- prepara el pincel EAX (un objeto, o un color del sistema + 1): CF=1 si
; no pinta ----
wg_ppincel:
        pushad
        mov edx, [wg_prog]
        mov edi, [wg_dc]
        cmp eax, 0x100
        jae .obj
        dec eax                     ; COLOR_* + 1
        js .no
        cmp eax, W32_NSYSCOL
        jae .no
        mov eax, [w32_syscol+eax*4]
        jmp .liso
.obj:
        call w32_obj
        jc .no
        cmp byte [esi+GO_TIPO], GT_PINCEL
        jne .no
        movzx ecx, byte [esi+GO_ESTILO]
        cmp ecx, 1
        je .no
        mov eax, [esi+GO_COLOR]
        cmp ecx, 2
        je .rayado
        cmp ecx, 3
        je .dibujo
.liso:
        call wg_col
        mov [wg_pt], eax
        mov dword [wg_pm], 0
        popad
        clc
        ret
.rayado:
        call wg_col
        mov [wg_pt], eax
        mov eax, [esi+GO_A]
        cmp eax, 5
        jbe .r1
        xor eax, eax
.r1:
        mov eax, [wg_rayas+eax*8]
        mov [wg_pmask], eax
        mov eax, [esi+GO_A]
        cmp eax, 5
        jbe .r2
        xor eax, eax
.r2:
        mov eax, [wg_rayas+eax*8+4]
        mov [wg_pmask+4], eax
        mov eax, -1                 ; el fondo, si es opaco
        cmp dword [edi+DC_MODO], 2
        jne .r3
        mov eax, [wg_bk]
.r3:
        mov [wg_pf], eax
        mov dword [wg_pm], 1
        popad
        clc
        ret
.dibujo:
        mov ebx, [esi+GO_B]         ; (de un mapa monocromo: 0 el texto, 1 el fondo)
        mov esi, [esi+GO_A]
        xor ecx, ecx
.d1:
        movzx eax, byte [esi+ecx]
        test ebx, ebx
        jz .d3
        test eax, eax
        mov eax, [wg_txt]
        jz .d3
        mov eax, [wg_bk]
.d3:
        cmp byte [wg_mono], 0       ; a un destino monocromo: claro u oscuro
        je .d4
        test ebx, ebx
        jnz .d4
        push ecx
        mov byte [wg_mono], 0
        call wg_rgb
        mov byte [wg_mono], 1
        call wg_idx
        pop ecx
.d4:
        mov [wg_patbuf+ecx], al
        inc ecx
        cmp ecx, 64
        jb .d1
        mov dword [wg_pm], 2
        popad
        clc
        ret
.no:
        mov dword [wg_pm], -1
        popad
        stc
        ret
; los rayados (HS_*): 8 filas de 8 puntos (el bit 7, el de la izquierda)
wg_rayas:
        db 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00   ; HS_HORIZONTAL
        db 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08   ; HS_VERTICAL
        db 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01   ; HS_FDIAGONAL
        db 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80   ; HS_BDIAGONAL
        db 0x08, 0x08, 0x08, 0xFF, 0x08, 0x08, 0x08, 0x08   ; HS_CROSS
        db 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81   ; HS_DIAGCROSS
; ---- EAX=x EBX=y: un punto de la pluma preparada (con su estilo y ancho) ----
wg_ppunto:
        cmp dword [wg_pa], 1
        ja .ancha
        push eax
        push ecx
        mov ecx, [wg_ppos]
        inc dword [wg_ppos]
        cmp ecx, [wg_plen]
        jb .c
        xor ecx, ecx
        mov dword [wg_ppos], 1
.c:
        mov eax, [wg_pbits]
        bt eax, ecx
        pop ecx
        pop eax
        jnc .hueco
        push eax
        mov eax, [wg_pcol]
        mov [wg_c], eax
        pop eax
        jmp wg_punto
.hueco:
        push eax                    ; (el hueco, del color del fondo si es opaco)
        mov eax, [wg_dc]
        cmp dword [eax+DC_MODO], 2
        pop eax
        jne .x
        push eax
        mov eax, [wg_bk]
        mov [wg_c], eax
        pop eax
        jmp wg_punto
.x:
        ret
.ancha:
        pushad                      ; un cuadrado del ancho de la pluma
        mov ecx, [wg_pcol]
        mov [wg_c], ecx
        mov ecx, [wg_pa]
        mov edx, ecx
        shr edx, 1
        sub eax, edx
        sub ebx, edx
        lea edx, [eax+ecx]
        mov esi, ecx
.f:
        push ecx
        mov ecx, edx
        call wg_tramo_c
        pop ecx
        inc ebx
        dec esi
        jnz .f
        popad
        ret
; ---- EAX=x0 EBX=y0 ECX=x1 EDX=y1: una línea con la pluma preparada, sin el
; último punto (con [wg_ultimo] distinto de 0, con él) ----
wg_linea:
        pushad
        mov esi, ecx
        sub esi, eax
        mov dword [ln_sx], 1
        jns .1
        neg esi
        mov dword [ln_sx], -1
.1:
        mov edi, edx
        sub edi, ebx
        mov dword [ln_sy], 1
        jns .2
        neg edi
        mov dword [ln_sy], -1
.2:
        neg edi
        mov [ln_dx], esi
        mov [ln_dy], edi
        add esi, edi
.l:
        cmp eax, ecx
        jne .n
        cmp ebx, edx
        jne .n
        cmp byte [wg_ultimo], 0
        je .x
        call wg_ppunto
        jmp .x
.n:
        call wg_ppunto
        lea edi, [esi+esi]
        cmp edi, [ln_dy]
        jl .ny
        add esi, [ln_dy]
        add eax, [ln_sx]
.ny:
        cmp edi, [ln_dx]
        jg .l
        add esi, [ln_dx]
        add ebx, [ln_sy]
        jmp .l
.x:
        popad
        ret

; =============================================================================
;  Figuras por filas: cada fila de la figura es un tramo [L, R); se rellena
;  con el pincel y su borde (los puntos que no tienen sus cuatro vecinos
;  dentro) se pinta con la pluma. Con [wg_filtro] = 1 (quesos) o 2 (cuerdas),
;  solo lo que cae dentro del sector o del lado de la cuerda.
; =============================================================================
; ---- rellena wg_fil_l/wg_fil_r con la elipse (EAX=l EBX=t ECX=r EDX=b,
; superficie) desde la fila wg_fil_y0; [wg_fil_n] filas ----
wg_filas_elipse:
        pushad
        mov [wg_fil_y0], ebx
        mov esi, ecx
        sub esi, eax                ; W
        mov edi, edx
        sub edi, ebx                ; H
        mov [wg_fil_n], edi
        mov [wg_t0], eax            ; l
        mov [wg_t1], ecx            ; r
        xor ebp, ebp                ; la fila
.f:
        cmp ebp, edi
        jge .x
        cmp ebp, WG_FILAS
        jae .x
        lea eax, [ebp*2+1]          ; yy = 2·fila + 1 − H
        sub eax, edi
        imul eax, eax
        mov ecx, edi
        imul ecx, ecx
        sub ecx, eax                ; H² − yy²
        jg .r
        xor ecx, ecx
.r:
        mov eax, esi
        imul eax, esi               ; W² · (H² − yy²), de 64 bits
        mul ecx
        call wg_raiz64              ; -> EAX
        xor edx, edx
        test edi, edi
        jz .h0
        div edi                     ; hx = √(...) / H
.h0:
        mov ecx, esi
        sub ecx, eax
        inc ecx
        sar ecx, 1                  ; lo que entra de cada lado
        jns .m
        xor ecx, ecx
.m:
        mov eax, [wg_t0]
        add eax, ecx
        mov [wg_fil_l+ebp*4], eax
        mov eax, [wg_t1]
        sub eax, ecx
        mov [wg_fil_r+ebp*4], eax
        inc ebp
        jmp .f
.x:
        cmp dword [wg_fil_n], WG_FILAS
        jbe .y
        mov dword [wg_fil_n], WG_FILAS
.y:
        popad
        ret
; ---- EDX:EAX -> EAX = su raíz cuadrada (entera) ----
wg_raiz64:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov esi, eax                ; ESI:EDI... el número en EDX:ESI
        xor ebx, ebx                ; la raíz
        mov ecx, 31
.l:
        mov edi, ebx                ; prueba = raíz | (1 << ecx)
        bts edi, ecx
        push edx
        push esi
        mov eax, edi
        mul edi                     ; prueba² en EDX:EAX
        pop esi
        mov ebp, edx
        pop edx
        cmp ebp, edx
        ja .no
        jb .si
        cmp eax, esi
        ja .no
.si:
        mov ebx, edi
.no:
        dec ecx
        jns .l
        mov eax, ebx
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret
; ---- rellena las filas con un rectángulo de esquinas redondeadas ----
; EAX=l EBX=t ECX=r EDX=b; [wg_t4], [wg_t5] = ancho y alto de la esquina
wg_filas_redondo:
        pushad
        mov [wg_rr_l], eax
        mov [wg_rr_t], ebx
        mov [wg_rr_r], ecx
        mov [wg_rr_b], edx
        mov esi, ecx
        sub esi, eax
        mov edi, edx
        sub edi, ebx
        mov eax, [wg_t4]            ; (como mucho, todo)
        cmp eax, esi
        jle .w
        mov eax, esi
.w:
        mov [wg_t4], eax
        mov eax, [wg_t5]
        cmp eax, edi
        jle .h
        mov eax, edi
.h:
        mov [wg_t5], eax
        ; las esquinas: una elipse de wg_t4 × wg_t5, en un búfer aparte
        mov eax, [wg_rr_l]
        mov ebx, [wg_rr_t]
        mov ecx, eax
        add ecx, [wg_t4]
        mov edx, ebx
        add edx, [wg_t5]
        call wg_filas_elipse
        mov ecx, [wg_fil_n]
        mov esi, wg_fil_l
        mov edi, wg_regtmp          ; (de paso, como búfer)
        cmp ecx, 512
        jbe .c
        mov ecx, 512
.c:
        rep movsd
        mov eax, [wg_rr_b]
        sub eax, [wg_rr_t]
        mov [wg_fil_n], eax
        cmp eax, WG_FILAS
        jbe .n
        mov dword [wg_fil_n], WG_FILAS
.n:
        mov eax, [wg_rr_t]
        mov [wg_fil_y0], eax
        mov ecx, [wg_t5]
        mov ebx, ecx
        shr ebx, 1                  ; las filas de arriba con esquina
        mov edi, [wg_rr_b]
        sub edi, [wg_rr_t]
        xor ebp, ebp
.f:
        cmp ebp, [wg_fil_n]
        jge .x
        mov eax, ebp                ; ¿qué fila de la esquina? (o ninguna)
        cmp eax, ebx
        jl .esq
        mov edx, edi
        sub edx, ebp
        dec edx                     ; filas hasta abajo
        cmp edx, ebx
        jge .recta
        mov eax, ecx
        sub eax, edx
        dec eax
.esq:
        cmp eax, 512
        jb .e2
        xor eax, eax
.e2:
        mov edx, [wg_regtmp+eax*4]  ; la L de la esquina
        sub edx, [wg_rr_l]
        mov eax, [wg_rr_l]
        add eax, edx
        mov [wg_fil_l+ebp*4], eax
        mov eax, [wg_rr_r]
        sub eax, edx
        mov [wg_fil_r+ebp*4], eax
        jmp .s
.recta:
        mov eax, [wg_rr_l]
        mov [wg_fil_l+ebp*4], eax
        mov eax, [wg_rr_r]
        mov [wg_fil_r+ebp*4], eax
.s:
        inc ebp
        jmp .f
.x:
        popad
        ret
; ---- pinta la figura de las filas: rellena (si hay pincel, [wg_con_pincel])
; y el borde (si hay pluma, [wg_con_pluma]) ----
wg_figura:
        pushad
        cmp byte [wg_con_pincel], 0
        je .borde
        xor ebp, ebp
.f:
        cmp ebp, [wg_fil_n]
        jge .borde
        mov eax, [wg_fil_l+ebp*4]
        mov ecx, [wg_fil_r+ebp*4]
        mov ebx, [wg_fil_y0]
        add ebx, ebp
        cmp byte [wg_filtro], 0
        jne .ff
        call wg_tramo
        jmp .fs
.ff:
        call wg_tramo_filtro
.fs:
        inc ebp
        jmp .f
.borde:
        cmp byte [wg_con_pluma], 0
        je .x
        xor ebp, ebp
.b:
        cmp ebp, [wg_fil_n]
        jge .x
        mov eax, [wg_fil_l+ebp*4]   ; lo de dentro: [max(L+1, Lant, Lsig),
        inc eax                     ;   min(R−1, Rant, Rsig))
        mov ecx, [wg_fil_r+ebp*4]
        dec ecx
        test ebp, ebp
        jz .todo
        lea edx, [ebp+1]
        cmp edx, [wg_fil_n]
        jae .todo
        cmp eax, [wg_fil_l+ebp*4-4]
        jge .b1
        mov eax, [wg_fil_l+ebp*4-4]
.b1:
        cmp eax, [wg_fil_l+ebp*4+4]
        jge .b2
        mov eax, [wg_fil_l+ebp*4+4]
.b2:
        cmp ecx, [wg_fil_r+ebp*4-4]
        jle .b3
        mov ecx, [wg_fil_r+ebp*4-4]
.b3:
        cmp ecx, [wg_fil_r+ebp*4+4]
        jle .b4
        mov ecx, [wg_fil_r+ebp*4+4]
.b4:
        cmp eax, ecx
        jl .dos
.todo:
        mov eax, [wg_fil_r+ebp*4]   ; toda la fila es borde
        mov ecx, eax
.dos:
        mov ebx, [wg_fil_y0]
        add ebx, ebp
        mov esi, [wg_fil_l+ebp*4]   ; de L a lo de dentro, y de ahí a R
        mov edi, eax
        call .tr
        mov esi, ecx
        cmp esi, edi
        jge .d2
        mov esi, edi
.d2:
        mov edi, [wg_fil_r+ebp*4]
        call .tr
        inc ebp
        jmp .b
.x:
        popad
        ret
.tr:                                ; los puntos [ESI, EDI) de la fila EBX
        push eax
.t1:
        cmp esi, edi
        jge .t2
        mov eax, esi
        cmp byte [wg_filtro], 0
        je .t3
        call wg_en_filtro
        jnc .t4
.t3:
        call wg_ppunto
.t4:
        inc esi
        jmp .t1
.t2:
        pop eax
        ret
; ---- (quesos y cuerdas) el tramo [EAX, ECX) de la fila EBX, solo lo que cae
; dentro del filtro ----
wg_tramo_filtro:
        push eax
        push esi
.l:
        cmp eax, ecx
        jge .x
        call wg_en_filtro
        jnc .s
        mov esi, eax                ; (punto a punto)
        push ecx
        lea ecx, [eax+1]
        call wg_tramo
        pop ecx
        mov eax, esi
.s:
        inc eax
        jmp .l
.x:
        pop esi
        pop eax
        ret
; ---- ¿el punto EAX, EBX (superficie) está dentro del sector (1) o del lado
; de la cuerda (2)? CF=1 si sí. En coordenadas dobles desde el centro
; (wg_cx2, wg_cy2), con la y hacia arriba ----
wg_en_filtro:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        lea eax, [eax*2+1]
        sub eax, [wg_cx2]           ; X
        lea ebx, [ebx*2+1]
        sub ebx, [wg_cy2]
        neg ebx                     ; Y
        cmp byte [wg_filtro], 2
        je .cuerda
        mov ecx, [wg_sx]            ; c_sp = S × P
        imul ecx, ebx
        mov edx, [wg_sy]
        imul edx, eax
        sub ecx, edx
        mov esi, eax                ; c_pe = P × E
        imul esi, [wg_ey]
        mov edi, ebx
        imul edi, [wg_ex]
        sub esi, edi
        mov edx, [wg_cse]
        test edx, edx
        jg .menor
        jl .mayor
        cmp dword [wg_dse], 0       ; en línea: el mismo sentido, todo
        jg .si
.menor2:
        test ecx, ecx
        jns .si
        jmp .no
.menor:
        test ecx, ecx
        js .no
        test esi, esi
        js .no
        jmp .si
.mayor:
        test ecx, ecx
        jns .si
        test esi, esi
        jns .si
        jmp .no
.cuerda:                            ; (E − S) × (P − S) <= 0
        sub eax, [wg_sx]
        sub ebx, [wg_sy]
        mov ecx, [wg_ex]
        sub ecx, [wg_sx]
        imul ecx, ebx
        mov edx, [wg_ey]
        sub edx, [wg_sy]
        imul edx, eax
        sub ecx, edx
        jg .no
.si:
        stc
        jmp .x
.no:
        clc
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
; ---- (Arc, Pie, Chord) prepara el filtro: EAX..EDX = la caja (superficie),
; [wg_t0..wg_t3] = el punto de inicio y el de fin (superficie). Deja en
; wg_px0/wg_py0 y wg_px1/wg_py1 los puntos de la elipse ----
wg_sector:
        pushad
        mov esi, eax
        add esi, ecx
        mov [wg_cx2], esi           ; el centro, en dobles
        mov esi, ebx
        add esi, edx
        mov [wg_cy2], esi
        sub ecx, eax                ; A = W, B = H
        mov [wg_sa], ecx
        sub edx, ebx
        mov [wg_sb], edx
        mov eax, [wg_t0]
        mov ebx, [wg_t1]
        call .dir
        mov [wg_sx], eax
        mov [wg_sy], ebx
        call .punto
        mov [wg_px0], eax
        mov [wg_py0], ebx
        mov eax, [wg_t2]
        mov ebx, [wg_t3]
        call .dir
        mov [wg_ex], eax
        mov [wg_ey], ebx
        call .punto
        mov [wg_px1], eax
        mov [wg_py1], ebx
        mov eax, [wg_sx]            ; S × E y S · E
        imul eax, [wg_ey]
        mov ecx, [wg_sy]
        imul ecx, [wg_ex]
        sub eax, ecx
        mov [wg_cse], eax
        mov eax, [wg_sx]
        imul eax, [wg_ex]
        mov ecx, [wg_sy]
        imul ecx, [wg_ey]
        add eax, ecx
        mov [wg_dse], eax
        cmp byte [wg_filtro], 2     ; (la cuerda va de punto a punto de la
        jne .x                      ; elipse, no por las direcciones)
        mov eax, [wg_px0]
        mov ebx, [wg_py0]
        call .dir
        mov [wg_sx], eax
        mov [wg_sy], ebx
        mov eax, [wg_px1]
        mov ebx, [wg_py1]
        call .dir
        mov [wg_ex], eax
        mov [wg_ey], ebx
.x:
        popad
        ret
.dir:                               ; EAX, EBX (superficie) -> desde el centro,
        lea eax, [eax*2+1]          ; en dobles, y hacia arriba; sin pasar de 16000
        sub eax, [wg_cx2]
        lea ebx, [ebx*2+1]
        sub ebx, [wg_cy2]
        neg ebx
.d1:
        mov ecx, eax
        cdq
        xor ecx, edx
        sub ecx, edx
        push eax
        mov eax, ebx
        cdq
        xor eax, edx
        sub eax, edx
        or ecx, eax
        pop eax
        cmp ecx, 16000
        jb .d2
        sar eax, 1
        sar ebx, 1
        jmp .d1
.d2:
        mov ecx, eax
        or ecx, ebx
        jnz .d3
        inc eax                     ; (el centro: hacia la derecha)
.d3:
        ret
.punto:                             ; la dirección EAX, EBX -> el punto de la
        push ebp                    ; elipse en ella (superficie)
        mov esi, eax
        mov edi, ebx
        mov eax, esi                ; s = √(X²B² + Y²A²)
        imul eax, [wg_sb]
        imul eax
        mov ecx, eax
        mov ebp, edx
        mov eax, edi
        imul eax, [wg_sa]
        imul eax
        add eax, ecx
        adc edx, ebp
        call wg_raiz64
        mov ecx, eax
        test ecx, ecx
        jnz .p1
        inc ecx
.p1:
        mov eax, [wg_sa]            ; x = (cx2 + X·A·B/s) / 2
        imul eax, [wg_sb]
        imul esi
        idiv ecx
        add eax, [wg_cx2]
        sar eax, 1
        push eax
        mov eax, [wg_sa]
        imul eax, [wg_sb]
        imul edi
        idiv ecx
        neg eax
        add eax, [wg_cy2]
        sar eax, 1
        mov ebx, eax
        pop eax
        pop ebp
        ret

; (fase 3b y 3c) lo mismo que wg_abrir
w32_dc_abrir:
        jmp wg_abrir
