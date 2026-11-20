; =============================================================================
;  Hispa-Snes - la imagen: los registros de la PPU y el dibujo línea a línea
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública de la PPU de la consola: la VRAM
;  de 64 KB (con su forma de avanzar y de recolocar las direcciones), la
;  CGRAM (256 colores de 15 bits), la OAM (128 objetos), los modos 0 a 7, los
;  fondos de 8x8 y 16x16, el desplazamiento por columnas de los modos 2, 4 y
;  6, el modo 7 (girar y escalar, con EXTBG), los objetos (8x8 a 64x64, 32
;  por línea), las dos ventanas con sus lógicas, la pantalla principal y la
;  secundaria, la suma y la resta de colores (y a la mitad), el color fijo,
;  el color directo y el brillo.
;
;    ppu_escribe    EDX = registro (00h-3Fh de $21xx), AL = dato
;    ppu_lee        EDX = registro -> EAX
;    ppu_linea      pinta la línea [linea] (1-224) en la fila [linea]-1 de
;                   «imagen» (los colores de Hispa Ventanas, con lut15) o,
;                   con [ppu_15], en imagen15 (los 15 bits, para las pruebas)
;
;  Cómo pinta: los dibujos de la VRAM se pasan, cuando hace falta, a una
;  «caché» de un octeto por punto (una para los de 2, 4 y 8 bits por punto;
;  escribir en la VRAM marca los que cambian). Cada capa se pinta en la línea
;  con su «altura» (z) según el modo y la prioridad: un punto solo se queda
;  si está por encima de lo que hay (pant_m: la altura y el color de cada
;  punto de la pantalla principal; pant_s, la secundaria, solo si hace falta
;  para la suma de colores). Luego cada
;  punto se pasa a su color (con la suma, si toca, y el brillo).
; =============================================================================
CAPA_FONDO  equ 5                   ; (en cm/cs, la capa: 0-3 fondos, 4 objetos
CAPA_DIR    equ 6                   ;  que suman, 5 el fondo de todo, 6 color
CAPA_OBJ0   equ 7                   ;  directo, 7 objetos de las paletas 0-3)

ppu_reiniciar:
        pushad
        mov edi, ppu_regs
        mov ecx, PPU_REGS_TAM / 4
        xor eax, eax
        rep stosd
        mov edi, vram
        mov ecx, 0x10000 / 4
        rep stosd
        mov edi, cgram
        mov ecx, 512 / 4
        rep stosd
        mov edi, oam
        mov ecx, 544 / 4
        rep stosd
        mov edi, sucio2             ; toda la caché, por hacer
        mov ecx, (4096 + 2048 + 1024) / 4
        mov eax, 0x01010101
        rep stosd
        mov byte [inidisp], 0x80
        mov dword [vram_paso], 1
        mov byte [oam_sucia], 1
        call ppu_brillo
        popad
        ret

; al empezar el borrado vertical: la dirección de la OAM vuelve a la puesta
ppu_vblank:
        test byte [inidisp], 0x80
        jnz .x
        mov eax, [oam_recarga]
        mov [oam_dir], eax
.x:
        ret

; (WRIO o $2137) los contadores de la línea y el punto, guardados
ppu_contadores:
        push eax
        call bus_hpos
        shr eax, 2
        mov [ophct], ax
        mov eax, [linea]
        mov [opvct], ax
        or byte [stat78], 0x40
        pop eax
        ret

; =============================================================================
;  Escribir en los registros
; =============================================================================
ppu_escribe:
        push ebx
        push ecx
        push edx
        mov ebx, edx
        and ebx, 0x3F
        movzx eax, al
        jmp [.tab+ebx*4]
.tab:
        dd .inidisp, .obsel, .oamaddl, .oamaddh, .oamdata, .bgmode, .mosaic, .bgsc
        dd .bgsc, .bgsc, .bgsc, .bgnba, .bgnba, .hofs, .vofs, .hofs
        dd .vofs, .hofs, .vofs, .hofs, .vofs, .vmain, .vmaddl, .vmaddh
        dd .vmdatal, .vmdatah, .m7sel, .m7, .m7, .m7, .m7, .m7
        dd .m7, .cgadd, .cgdata, .wsel, .wsel, .wsel, .wh, .wh
        dd .wh, .wh, .wlog, .wlog, .tmts, .tmts, .tmts, .tmts
        dd .cgwsel, .cgadsub, .coldata, .setini, .x, .x, .x, .x
        dd .x, .x, .x, .x, .x, .x, .x, .x
.x:
        pop edx
        pop ecx
        pop ebx
        ret
.inidisp:
        mov ah, [inidisp]
        mov [inidisp], al
        xor ah, al
        test ah, 0x0F
        jz .x
        call ppu_brillo
        jmp .x
.obsel:
        mov [obsel], al
        mov byte [oam_sucia], 1
        mov ecx, eax                ; la base de los dibujos de los objetos
        and ecx, 7
        shl ecx, 13
        mov [obj_base], ecx
        mov ecx, eax                ; y la distancia hasta la segunda tabla
        shr ecx, 3
        and ecx, 3
        inc ecx
        shl ecx, 12
        mov [obj_hueco], ecx
        shr eax, 5                  ; los tamaños
        movzx ecx, byte [obj_tams+eax*4]
        mov [obj_tam], cl
        movzx ecx, byte [obj_tams+eax*4+1]
        mov [obj_tam+1], cl
        movzx ecx, byte [obj_tams+eax*4+2]
        mov [obj_tam+2], cl
        movzx ecx, byte [obj_tams+eax*4+3]
        mov [obj_tam+3], cl
        jmp .x
.oamaddl:
        mov [oam_recarga], al
        jmp .oamadd
.oamaddh:
        mov [oam_rot], al
        and eax, 1
        mov [oam_recarga+1], al
.oamadd:
        mov eax, [oam_recarga]
        and eax, 0x1FF
        shl eax, 1
        mov [oam_dir], eax
        mov byte [oam_pri], 0
        jmp .x
.oamdata:
        mov ecx, [oam_dir]
        and ecx, 0x3FF
        cmp ecx, 0x200
        jae .oam_alta
        test ecx, 1
        jnz .oam_impar
        mov [oam_latch], al
        jmp .oam_sig
.oam_impar:
        mov ah, al
        mov al, [oam_latch]
        mov [oam+ecx-1], ax
        jmp .oam_sig
.oam_alta:
        and ecx, 0x21F
        mov [oam+ecx], al
.oam_sig:
        mov byte [oam_sucia], 1
        inc dword [oam_dir]
        and dword [oam_dir], 0x3FF
        jmp .x
.bgmode:
        mov [bgmode], al
        jmp .x
.mosaic:
        mov [mosaic], al
        jmp .x
.bgsc:
        mov [bgsc+ebx-7], al
        jmp .x
.bgnba:
        mov [bgnba+ebx-0x0B], al
        jmp .x
.hofs:                              ; BGnHOFS (y M7HOFS en 0Dh)
        lea ecx, [ebx-0x0D]
        shr ecx, 1                  ; ECX = el fondo
        cmp ebx, 0x0D
        jne .h1
        mov edx, eax                ; M7HOFS: 13 bits con su propio «latch»
        shl edx, 8
        or dl, [m7_latch]
        mov [m7hofs], dx
        mov [m7_latch], al
.h1:
        mov edx, eax
        shl edx, 8
        movzx ebx, byte [ofs_latch]
        and ebx, ~7
        or edx, ebx
        movzx ebx, word [bg_hofs+ecx*2]
        shr ebx, 8
        and ebx, 7
        or edx, ebx
        and edx, 0x3FF
        mov [bg_hofs+ecx*2], dx
        mov [ofs_latch], al
        jmp .x
.vofs:
        lea ecx, [ebx-0x0E]
        shr ecx, 1
        cmp ebx, 0x0E
        jne .v1
        mov edx, eax
        shl edx, 8
        or dl, [m7_latch]
        mov [m7vofs], dx
        mov [m7_latch], al
.v1:
        mov edx, eax
        shl edx, 8
        or dl, [ofs_latch]
        and edx, 0x3FF
        mov [bg_vofs+ecx*2], dx
        mov [ofs_latch], al
        jmp .x
.vmain:
        mov [vmain], al
        mov ecx, eax
        and ecx, 3
        mov ecx, [vram_pasos+ecx*4]
        mov [vram_paso], ecx
        jmp .x
.vmaddl:
        mov [vram_dir], al
        jmp .vmadd
.vmaddh:
        mov [vram_dir+1], al
.vmadd:
        call vram_traduce
        movzx eax, word [vram+ecx*2]
        mov [vram_latch], ax
        jmp .x
.vmdatal:
        call vram_puede
        jc .vl1
        call vram_traduce
        mov [vram+ecx*2], al
        call vram_sucio
.vl1:
        test byte [vmain], 0x80
        jnz .x
        mov ecx, [vram_paso]
        add [vram_dir], cx
        jmp .x
.vmdatah:
        call vram_puede
        jc .vh1
        call vram_traduce
        mov [vram+ecx*2+1], al
        call vram_sucio
.vh1:
        test byte [vmain], 0x80
        jz .x
        mov ecx, [vram_paso]
        add [vram_dir], cx
        jmp .x
.m7sel:
        mov [m7sel], al
        jmp .x
.m7:                                ; M7A-M7D, M7X, M7Y: dos escrituras
        mov edx, eax
        shl edx, 8
        or dl, [m7_latch]
        mov [m7_latch], al
        mov [m7a+(ebx-0x1B)*2], dx
        cmp ebx, 0x1C               ; (M7A y M7B: la multiplicación)
        ja .x
        movsx ecx, word [m7a]
        movsx edx, byte [m7a+3]     ; (la parte alta de M7B: lo último escrito)
        imul ecx, edx
        mov [mpy], ecx
        jmp .x
.cgadd:
        mov [cg_dir], al
        mov byte [cg_pri], 0
        jmp .x
.cgdata:
        xor byte [cg_pri], 1
        jz .cg2
        mov [cg_latch], al
        jmp .x
.cg2:
        movzx ecx, byte [cg_dir]
        mov ah, al
        and ah, 0x7F
        mov al, [cg_latch]
        mov [cgram+ecx*2], ax
        call cg_brillo_uno
        inc byte [cg_dir]
        jmp .x
.wsel:
        mov [wsel+ebx-0x23], al
        jmp .x
.wh:
        mov [wh+ebx-0x26], al
        jmp .x
.wlog:
        mov [wlog+ebx-0x2A], al
        jmp .x
.tmts:
        mov [tm+ebx-0x2C], al
        jmp .x
.cgwsel:
        mov [cgwsel], al
        jmp .x
.cgadsub:
        mov [cgadsub], al
        jmp .x
.coldata:
        mov ecx, eax
        and ecx, 0x1F
        test al, 0x20
        jz .cd1
        mov [fijo_r], cl
.cd1:
        test al, 0x40
        jz .cd2
        mov [fijo_g], cl
.cd2:
        test al, 0x80
        jz .cd3
        mov [fijo_b], cl
.cd3:
        movzx eax, byte [fijo_b]    ; el color fijo, en 15 bits
        shl eax, 5
        or al, [fijo_g]
        shl eax, 5
        or al, [fijo_r]
        mov [fijo15], eax
        jmp .x
.setini:
        mov [setini], al
        mov dword [vis_fin], 225
        test al, 4
        jz .x
        mov dword [vis_fin], 240
        jmp .x

; ¿se puede escribir en la VRAM? (solo en el borrado vertical o con la
; pantalla apagada) -> CF=1 si no
vram_puede:
        test byte [inidisp], 0x80
        jnz .si
        cmp byte [en_vblank], 0
        jne .si
        stc
        ret
.si:
        clc
        ret

; -> ECX = la dirección de la VRAM (palabra) con la recolocación de VMAIN
vram_traduce:
        push eax
        movzx ecx, word [vram_dir]
        mov al, [vmain]
        shr al, 2
        and al, 3
        jz .x
        cmp al, 1
        je .t8
        cmp al, 2
        je .t9
        mov eax, ecx                ; aaaaaaBBBccccccc -> aaaaaacccccccBBB
        and ecx, 0xFC00
        mov edx, eax
        shr edx, 7
        and edx, 7
        or ecx, edx
        and eax, 0x7F
        shl eax, 3
        or ecx, eax
        jmp .x
.t8:
        mov eax, ecx                ; aaaaaaaaBBBccccc -> aaaaaaaacccccBBB
        and ecx, 0xFF00
        mov edx, eax
        shr edx, 5
        and edx, 7
        or ecx, edx
        and eax, 0x1F
        shl eax, 3
        or ecx, eax
        jmp .x
.t9:
        mov eax, ecx                ; aaaaaaaBBBcccccc -> aaaaaaaccccccBBB
        and ecx, 0xFE00
        mov edx, eax
        shr edx, 6
        and edx, 7
        or ecx, edx
        and eax, 0x3F
        shl eax, 3
        or ecx, eax
.x:
        and ecx, 0x7FFF
        pop eax
        ret
; ECX = palabra de la VRAM que ha cambiado: sus dibujos, por rehacer
vram_sucio:
        push ecx
        shr ecx, 3
        mov byte [sucio2+ecx], 1
        shr ecx, 1
        mov byte [sucio4+ecx], 1
        shr ecx, 1
        mov byte [sucio8+ecx], 1
        pop ecx
        ret

; el brillo cambia: los 256 colores, otra vez
ppu_brillo:
        pushad
        movzx eax, byte [inidisp]
        and eax, 0x0F
        shl eax, 5
        add eax, brillo_tab
        mov [brillo_p], eax
        xor ecx, ecx
.c:
        call cg_brillo_uno
        inc ecx
        cmp ecx, 256
        jb .c
        popad
        ret
; ECX = color de la CGRAM: su versión con el brillo (cgram_b) y en Hispa
; Ventanas (cg_host)
cg_brillo_uno:
        push eax
        push edx
        movzx eax, word [cgram+ecx*2]
        call brillo15
        mov [cgram_b+ecx*2], ax
        movzx eax, byte [lut15+eax]
        mov [cg_host+ecx], al
        pop edx
        pop eax
        ret
; EAX = color de 15 bits -> EAX, con el brillo puesto (conserva lo demás)
brillo15:
        push ebx
        push edx
        mov ebx, [brillo_p]
        mov edx, eax
        and edx, 0x1F
        movzx edx, byte [ebx+edx]
        push edx
        mov edx, eax
        shr edx, 5
        and edx, 0x1F
        movzx edx, byte [ebx+edx]
        shl edx, 5
        or [esp], edx
        shr eax, 10
        and eax, 0x1F
        movzx eax, byte [ebx+eax]
        shl eax, 10
        or eax, [esp]
        add esp, 4
        pop edx
        pop ebx
        ret

; los tamaños de los objetos según OBSEL (ancho y alto del pequeño, del grande)
obj_tams    db 8, 8, 16, 16,   8, 8, 32, 32,   8, 8, 64, 64,   16, 16, 32, 32
            db 16, 16, 64, 64,  32, 32, 64, 64,  16, 32, 32, 64,  16, 32, 32, 32
        align 4
vram_pasos  dd 1, 32, 128, 128

; =============================================================================
;  Leer los registros
; =============================================================================
ppu_lee:
        push ebx
        push ecx
        push edx
        mov ebx, edx
        and ebx, 0x3F
        cmp ebx, 0x34
        jb .nada
        jmp [.tab+(ebx-0x34)*4]
.tab:
        dd .mpyl, .mpym, .mpyh, .slhv, .oamr, .vrl, .vrh, .cgr
        dd .hc, .vc, .st77, .st78
.nada:
        movzx eax, byte [ppu_abierto]
.x:
        mov [ppu_abierto], al
        pop edx
        pop ecx
        pop ebx
        ret
.mpyl:
        movzx eax, byte [mpy]
        jmp .x
.mpym:
        movzx eax, byte [mpy+1]
        jmp .x
.mpyh:
        movzx eax, byte [mpy+2]
        jmp .x
.slhv:
        test byte [wrio], 0x80
        jz .nada
        call ppu_contadores
        jmp .nada
.oamr:
        mov ecx, [oam_dir]
        and ecx, 0x3FF
        cmp ecx, 0x200
        jb .o1
        and ecx, 0x21F
.o1:
        movzx eax, byte [oam+ecx]
        inc dword [oam_dir]
        and dword [oam_dir], 0x3FF
        jmp .x
.vrl:
        movzx eax, byte [vram_latch]
        test byte [vmain], 0x80
        jnz .x
        jmp .vr_sig
.vrh:
        movzx eax, byte [vram_latch+1]
        test byte [vmain], 0x80
        jz .x
.vr_sig:
        push eax
        call vram_traduce
        movzx eax, word [vram+ecx*2]
        mov [vram_latch], ax
        mov ecx, [vram_paso]
        add [vram_dir], cx
        pop eax
        jmp .x
.cgr:
        movzx ecx, byte [cg_dir]
        xor byte [cg_pri_l], 1
        jz .cg2
        movzx eax, byte [cgram+ecx*2]
        jmp .x
.cg2:
        movzx eax, byte [cgram+ecx*2+1]
        inc byte [cg_dir]
        jmp .x
.hc:
        xor byte [hc_pri], 1
        jz .hc2
        movzx eax, byte [ophct]
        jmp .x
.hc2:
        movzx eax, byte [ophct+1]
        and al, 1
        jmp .x
.vc:
        xor byte [vc_pri], 1
        jz .vc2
        movzx eax, byte [opvct]
        jmp .x
.vc2:
        movzx eax, byte [opvct+1]
        and al, 1
        jmp .x
.st77:
        movzx eax, byte [stat77]
        or al, 1
        jmp .x
.st78:
        movzx eax, byte [stat78]
        and al, 0x40
        or al, [campo]
        or al, 3
        cmp dword [cart_pal], 0
        je .s1
        or al, 0x10
.s1:
        and byte [stat78], ~0x40
        mov byte [hc_pri], 0
        mov byte [vc_pri], 0
        jmp .x

; =============================================================================
;  La caché de los dibujos: un octeto por punto (64 por dibujo)
; =============================================================================
; EAX = número del dibujo de 2 bits -> ESI = sus 64 puntos (rehace si hace falta)
%macro PLANO 2                     ; desplazamiento del octeto, número del plano
        movzx eax, byte [esi+%1]
        mov edx, [plano_lo+eax*8]
%if %2
        shl edx, %2
%endif
        or ebx, edx
        mov edx, [plano_lo+eax*8+4]
%if %2
        shl edx, %2
%endif
        or ecx, edx
%endmacro
dib2:
        mov esi, eax
        shl esi, 6
        add esi, cache2
        cmp byte [sucio2+eax], 0
        jne .rehace
        ret
.rehace:
        pushad
        mov byte [sucio2+eax], 0
        mov edi, esi
        shl eax, 4
        lea esi, [vram+eax]         ; 8 palabras: los planos 0 y 1
        mov ebp, 8
.f:
        xor ebx, ebx
        xor ecx, ecx
        PLANO 0, 0
        PLANO 1, 1
        mov [edi], ebx
        mov [edi+4], ecx
        add esi, 2
        add edi, 8
        dec ebp
        jnz .f
        popad
        ret
dib4:
        mov esi, eax
        shl esi, 6
        add esi, cache4
        cmp byte [sucio4+eax], 0
        jne .rehace
        ret
.rehace:
        pushad
        mov byte [sucio4+eax], 0
        mov edi, esi
        shl eax, 5
        lea esi, [vram+eax]         ; 16 palabras: planos 0-1 y luego 2-3
        mov ebp, 8
.f:
        xor ebx, ebx
        xor ecx, ecx
        PLANO 0, 0
        PLANO 1, 1
        PLANO 16, 2
        PLANO 17, 3
        mov [edi], ebx
        mov [edi+4], ecx
        add esi, 2
        add edi, 8
        dec ebp
        jnz .f
        popad
        ret
dib8:
        mov esi, eax
        shl esi, 6
        add esi, cache8
        cmp byte [sucio8+eax], 0
        jne .rehace
        ret
.rehace:
        pushad
        mov byte [sucio8+eax], 0
        mov edi, esi
        shl eax, 6
        lea esi, [vram+eax]         ; 32 palabras: planos 0-1, 2-3, 4-5, 6-7
        mov ebp, 8
.f:
        xor ebx, ebx
        xor ecx, ecx
        PLANO 0, 0
        PLANO 1, 1
        PLANO 16, 2
        PLANO 17, 3
        PLANO 32, 4
        PLANO 33, 5
        PLANO 48, 6
        PLANO 49, 7
        mov [edi], ebx
        mov [edi+4], ecx
        add esi, 2
        add edi, 8
        dec ebp
        jnz .f
        popad
        ret

; la tabla que pasa un octeto de un plano a 8 puntos (1 en cada uno que tiene
; el bit; el de la izquierda es el bit 7)
ppu_tablas:
        pushad
        xor eax, eax
.c:
        xor ecx, ecx
.b:
        mov edx, eax
        mov ebx, 7
        sub ebx, ecx
        push ecx
        mov ecx, ebx
        shr edx, cl
        pop ecx
        and edx, 1
        mov [plano_lo+eax*8+ecx], dl
        inc ecx
        cmp ecx, 8
        jb .b
        inc eax
        cmp eax, 256
        jb .c
        ; el brillo: 16 niveles x 32 valores
        xor ebx, ebx
.br:
        xor ecx, ecx
.bv:
        mov eax, ecx
        lea edx, [ebx+1]
        imul eax, edx
        shr eax, 4
        mov edx, ebx
        shl edx, 5
        mov [brillo_tab+edx+ecx], al
        inc ecx
        cmp ecx, 32
        jb .bv
        inc ebx
        cmp ebx, 16
        jb .br
        popad
        ret

; =============================================================================
;  Una línea
; =============================================================================
ppu_linea:
        pushad
        mov eax, [linea]
        mov [lin_y], eax
        test byte [inidisp], 0x80
        jnz near .negra
        ; lo que hay al fondo de todo
        mov ecx, 256
        mov edi, pant_m
        xor eax, eax
        rep stosb
        mov ecx, 256
        mov eax, CAPA_FONDO << 8
        rep stosw
        movzx eax, byte [bgmode]
        and eax, 7
        mov [modo], eax
        ; ¿hace falta la pantalla secundaria?
        mov byte [con_sub], 0
        test byte [cgwsel], 2
        jz .s1
        test byte [cgadsub], 0x3F
        jz .s1
        mov byte [con_sub], 1
        mov ecx, 256
        mov edi, pant_s
        xor eax, eax
        rep stosb
        mov ecx, 256
        mov eax, CAPA_FONDO << 8
        rep stosw
.s1:
        call ventanas_linea
        ; las capas de este modo
        mov ebx, [modo]
        movzx eax, byte [tm]
        or al, [ts]
        mov [capas], al
        mov ecx, 0                  ; los fondos, del 1 al 4
.capa:
        movzx edx, byte [modo_bpp+ebx*4+ecx]
        test edx, edx
        jz .sig
        bt dword [capas], ecx
        jnc .sig
        mov [cap_n], ecx
        mov [cap_bpp], edx
        push ebx
        push ecx
        cmp ebx, 7
        je .m7
        call fondo_linea
        jmp .hecha
.m7:
        call modo7_linea
.hecha:
        pop ecx
        pop ebx
.sig:
        inc ecx
        cmp ecx, 4
        jb .capa
        test byte [capas], 0x10
        jz .sinobj
        call objetos_linea
.sinobj:
        call ppu_colores
        popad
        ret
.negra:
        mov eax, [lin_y]
        dec eax
        cmp byte [ppu_15], 0
        jne .n15
        shl eax, 8
        lea edi, [imagen+eax]
        mov ecx, 256
        mov al, [lut15]
        rep stosb
        popad
        ret
.n15:
        shl eax, 9
        lea edi, [imagen15+eax]
        mov ecx, 128
        xor eax, eax
        rep stosd
        popad
        ret

; los bits por punto de cada fondo en cada modo (0: no hay); el 8 del modo 7
; es el suyo
modo_bpp    db 2, 2, 2, 2,   4, 4, 2, 0,   4, 4, 0, 0,   8, 4, 0, 0
            db 8, 2, 0, 0,   4, 2, 0, 0,   4, 0, 0, 0,   8, 8, 0, 0
; la altura (z) de cada capa con prioridad 0 y 1, por modo: fondos 1-4 y
; objetos (prioridad 0-3); más alto, más delante
z_fondo     db  8, 11,  7, 10,  2, 5,  1, 4     ; modo 0
            db  6,  9,  5,  8,  1, 3,  0, 0     ; modo 1
            db  3,  7,  1,  5,  0, 0,  0, 0     ; modos 2 a 6
            db  2,  2,  1,  5,  0, 0,  0, 0     ; modo 7 (y EXTBG)
z_obj       db  3,  6,  9, 12                   ; modo 0
            db  2,  4,  7, 10                   ; modo 1
            db  2,  4,  6,  8                   ; modos 2 a 6
            db  1,  3,  4,  5                   ; modo 7
z_obj_ext   db  2,  4,  6,  7                   ; modo 7 con EXTBG
z_grupo     db  0, 1, 2, 2, 2, 2, 2, 3

; ---- las ventanas de la línea: una máscara por capa (1 = tapada) ----
; [vmask_m+capa] / [vmask_s+capa]: puntero a la máscara de la pantalla
; principal / secundaria (0 si la capa no se tapa); hay_colv: la del color
; (vmasc+5*256: 1 = dentro). Las máscaras se rehacen solo si han cambiado
; los registros de las ventanas desde la línea anterior (w_clave)
ventanas_linea:
        pushad
        xor eax, eax
        mov edi, vmask_m
        mov ecx, 12
        rep stosd
        ; ¿alguna capa usa ventana? (si no, nada)
        mov eax, [wsel]
        and eax, 0x00AAAAAA
        jnz .hay
        mov byte [hay_colv], 0
        mov byte [w_usa], 0
        popad
        ret
.hay:
        mov eax, [wh]               ; la clave: los bordes, las selecciones y
        mov edx, [wsel]             ; las lógicas
        and edx, 0x00FFFFFF
        movzx ecx, word [wlog]
        cmp eax, [w_clave]
        jne .rehace
        cmp edx, [w_clave+4]
        jne .rehace
        cmp ecx, [w_clave+8]
        je .pon
.rehace:
        mov [w_clave], eax
        mov [w_clave+4], edx
        mov [w_clave+8], ecx
        call ventanas_rehace
.pon:
        mov byte [hay_colv], 0
        xor ebx, ebx
.c:
        bt dword [w_usa], ebx
        jnc .s
        mov eax, ebx
        shl eax, 8
        add eax, vmasc
        cmp ebx, 5
        je .col
        movzx edx, byte [tmw]
        bt edx, ebx
        jnc .s1
        mov [vmask_m+ebx*4], eax
.s1:
        movzx edx, byte [tsw]
        bt edx, ebx
        jnc .s
        mov [vmask_s+ebx*4], eax
        jmp .s
.col:
        mov byte [hay_colv], 1
.s:
        inc ebx
        cmp ebx, 6
        jb .c
        popad
        ret

; (interna) las dos ventanas (w1, w2: 1 = dentro) y la máscara de cada capa
; con su lógica, de cuatro en cuatro puntos; w_usa: las capas que usan alguna
ventanas_rehace:
        pushad
        xor eax, eax
        mov edi, w1
        mov ecx, 512 / 4
        rep stosd
        movzx ecx, byte [wh]
        movzx edx, byte [wh+1]
        mov edi, w1
        call .tramo
        movzx ecx, byte [wh+2]
        movzx edx, byte [wh+3]
        mov edi, w2
        call .tramo
        mov byte [w_usa], 0
        xor ebx, ebx
.capa:
        mov eax, ebx
        shr eax, 1
        movzx edx, byte [wsel+eax]  ; los 4 bits de esta capa
        test ebx, 1
        jz .a
        shr edx, 4
.a:
        and edx, 0x0F
        test edx, 0x0A
        jz near .sig
        bts dword [w_usa], ebx
        ; la lógica (2 bits)
        cmp ebx, 4
        jae .lo
        movzx eax, byte [wlog]
        mov ecx, ebx
        shl ecx, 1
        shr eax, cl
        jmp .l1
.lo:
        movzx eax, byte [wlog+1]
        cmp ebx, 5
        jne .l1
        shr eax, 2
.l1:
        and eax, 3
        mov [w_logica], al
        ; las inversiones (01010101h en cada octeto, o 0)
        xor eax, eax
        test edx, 1
        jz .i1
        mov eax, 0x01010101
.i1:
        mov [w_inv1], eax
        xor eax, eax
        test edx, 4
        jz .i2
        mov eax, 0x01010101
.i2:
        mov [w_inv2], eax
        mov edi, ebx
        shl edi, 8
        add edi, vmasc
        xor ecx, ecx
.p:
        mov eax, [w1+ecx*4]
        xor eax, [w_inv1]
        mov esi, [w2+ecx*4]
        xor esi, [w_inv2]
        test edx, 8                 ; ¿la 2?
        jz .solo1
        test edx, 2                 ; ¿la 1?
        jz .solo2
        mov ebp, eax
        mov al, [w_logica]
        cmp al, 1
        jb .or
        je .and
        mov eax, ebp
        xor eax, esi
        cmp byte [w_logica], 2
        je .pon
        xor eax, 0x01010101         ; XNOR
        jmp .pon
.or:
        mov eax, ebp
        or eax, esi
        jmp .pon
.and:
        mov eax, ebp
        and eax, esi
        jmp .pon
.solo2:
        mov eax, esi
.solo1:
.pon:
        mov [edi+ecx*4], eax
        inc ecx
        cmp ecx, 64
        jb .p
.sig:
        inc ebx
        cmp ebx, 6
        jb .capa
        popad
        ret
; ECX = izquierda, EDX = derecha: unos de una a otra en EDI (si izq <= der)
.tramo:
        cmp ecx, edx
        ja .t9
        sub edx, ecx
        inc edx
        add edi, ecx
        mov ecx, edx
        mov al, 1
        rep stosb
.t9:
        ret

; =============================================================================
;  Un fondo de los modos 0 a 6: [cap_n] (0-3), [cap_bpp], en la línea
; =============================================================================
fondo_linea:
        pushad
        mov ebx, [cap_n]
        ; la base del mapa y su tamaño
        movzx eax, byte [bgsc+ebx]
        mov edx, eax
        and edx, 3
        mov [fl_sc], edx
        and eax, 0xFC
        shl eax, 8                  ; (palabras)
        mov [fl_mapa], eax
        ; la base de los dibujos
        mov eax, ebx
        shr eax, 1
        movzx eax, byte [bgnba+eax]
        test ebx, 1
        jz .n1
        shr eax, 4
.n1:
        and eax, 0x0F
        shl eax, 12
        mov [fl_dib], eax
        ; dibujos de 16x16
        movzx eax, byte [bgmode]
        mov ecx, ebx
        add ecx, 4
        shr eax, cl
        and eax, 1
        mov [fl_16], eax
        ; la paleta base (modo 0: 32 colores por fondo)
        xor eax, eax
        cmp dword [modo], 0
        jne .n2
        mov eax, ebx
        shl eax, 5
.n2:
        mov [fl_palb], eax
        ; las alturas de las dos prioridades
        mov eax, [modo]
        movzx eax, byte [z_grupo+eax]
        shl eax, 3
        lea eax, [eax+ebx*2]
        movzx edx, byte [z_fondo+eax]
        mov [fl_z0], edx
        movzx edx, byte [z_fondo+eax+1]
        mov [fl_z1], edx
        cmp dword [modo], 1         ; modo 1 con el fondo 3 delante de todo
        jne .n3
        cmp ebx, 2
        jne .n3
        test byte [bgmode], 8
        jz .n3
        mov dword [fl_z1], 13
.n3:
        ; color directo (8 bits en los modos 3 y 4)
        mov byte [fl_dir], 0
        cmp dword [cap_bpp], 8
        jne .n4
        test byte [cgwsel], 1
        jz .n4
        mov byte [fl_dir], 1
.n4:
        ; ¿desplazamiento por columnas? (modos 2, 4 y 6, fondos 1 y 2)
        mov byte [fl_opt], 0
        mov eax, [modo]
        cmp eax, 2
        je .o1
        cmp eax, 4
        je .o1
        cmp eax, 6
        jne .o2
.o1:
        cmp ebx, 2
        jae .o2
        mov byte [fl_opt], 1
.o2:
        ; y se pinta en la principal y en la secundaria
        movzx eax, byte [tm]
        bt eax, ebx
        jnc .p1
        mov dword [fl_cz], pant_m
        mov eax, [vmask_m+ebx*4]
        mov [fl_mask], eax
        call fondo_mosaico
.p1:
        cmp byte [con_sub], 0
        je .x
        movzx eax, byte [ts]
        bt eax, ebx
        jnc .x
        mov dword [fl_cz], pant_s
        mov eax, [vmask_s+ebx*4]
        mov [fl_mask], eax
        call fondo_mosaico
.x:
        popad
        ret

; (interna) fondo_pinta, con el mosaico si el fondo lo tiene puesto: la
; línea del principio del bloque (en vertical) se pinta aparte, en pant_t, y
; de ahí, el primer punto de cada bloque, repetido (en horizontal)
fondo_mosaico:
        movzx eax, byte [mosaic]
        mov edx, [cap_n]
        bt eax, edx
        jnc fondo_pinta
        shr eax, 4
        jz fondo_pinta              ; (de 1 punto: como sin él)
        pushad
        inc eax
        mov ebp, eax                ; EBP = el tamaño del bloque
        push dword [lin_y]
        push dword [fl_cz]
        push dword [fl_mask]
        mov eax, [lin_y]            ; la línea: la primera de su bloque
        dec eax
        xor edx, edx
        div ebp
        sub [lin_y], edx
        mov edi, pant_t             ; aparte, sin máscara
        mov ecx, 256 / 4
        xor eax, eax
        rep stosd
        mov dword [fl_cz], pant_t
        mov dword [fl_mask], 0
        call fondo_pinta
        pop dword [fl_mask]
        pop dword [fl_cz]
        pop dword [lin_y]
        ; los bloques, a la pantalla
        mov edi, [fl_cz]
        mov esi, [fl_mask]
        xor ecx, ecx                ; ECX = x
        xor ebx, ebx                ; EBX = el primero del bloque
        xor edx, edx                ; EDX = lo que lleva del bloque
.p:
        mov al, [pant_t+ebx]
        test al, al
        jz .s
        cmp al, [edi+ecx]
        jbe .s
        test esi, esi
        jz .m
        cmp byte [esi+ecx], 0
        jne .s
.m:
        mov [edi+ecx], al
        mov ax, [pant_t+256+ebx*2]
        mov [edi+256+ecx*2], ax
.s:
        inc ecx
        inc edx
        cmp edx, ebp
        jb .n
        xor edx, edx
        mov ebx, ecx
.n:
        cmp ecx, 256
        jb .p
        popad
        ret

; un punto de fondo_rapido: %1 = su sitio en la pantalla (desde EDI), %2 = de
; dónde sale en la fila del dibujo (ESI); DL = la altura, DH = la capa, BL =
; la paleta, ECX = la pantalla
%macro PUNTO_FR 2
        lea eax, [edi+%1]
        cmp eax, 256
        jae %%n
        movzx eax, byte [esi+%2]
        test al, al
        jz %%n
        cmp dl, [ecx+edi+%1]
        jbe %%n
        add al, bl
        mov ah, dh
        mov [ecx+edi+%1], dl
        mov [ecx+256+edi*2+%1*2], ax
%%n:
%endmacro
%macro PUNTO_FRM 2                  ; (lo mismo, con la máscara en EBP)
        lea eax, [edi+%1]
        cmp eax, 256
        jae %%n
        movzx eax, byte [esi+%2]
        test al, al
        jz %%n
        cmp dl, [ecx+edi+%1]
        jbe %%n
        cmp byte [ebp+edi+%1], 0
        jne %%n
        add al, bl
        mov ah, dh
        mov [ecx+edi+%1], dl
        mov [ecx+256+edi*2+%1*2], ax
%%n:
%endmacro
; (interna) lo de siempre (sin desplazamiento por columnas): de 8 en 8
; puntos, todo aquí dentro, sin llamar a nada por cada dibujo
fondo_rapido:
        pushad
        mov ebx, [cap_n]
        ; la fila del fondo y la del dibujo
        movzx eax, word [bg_vofs+ebx*2]
        add eax, [lin_y]
        and eax, 0x3FF
        mov edx, eax
        and edx, 7
        mov [fr_fila], edx
        mov [fr_filav], edx
        xor dword [fr_filav], 7     ; (la de los volteados)
        shr eax, 3                  ; la fila de dibujos
        mov edx, eax                ; (con los de 16x16: la mitad de abajo)
        and edx, 1
        mov [fr_mitad], edx
        cmp dword [fl_16], 0
        je .f0
        shr eax, 1
.f0:
        mov edx, eax
        and edx, 31
        shl edx, 5
        add edx, [fl_mapa]
        test eax, 32
        jz .f1
        test byte [fl_sc], 2
        jz .f1
        add edx, 0x400
        test byte [fl_sc], 1
        jz .f1
        add edx, 0x400
.f1:
        mov [fr_izq], edx           ; la fila en el mapa de la izquierda
        test byte [fl_sc], 1
        jz .f2
        add edx, 0x400
.f2:
        mov [fr_der], edx           ;   y en el de la derecha
        movzx eax, word [bg_hofs+ebx*2]
        mov edx, eax
        and edx, 7
        neg edx
        mov [fl_x], edx
        shr eax, 3
        mov [fr_tx], eax            ; (en columnas de 8)
        mov dl, [fl_z0]             ; (las alturas, en octetos)
        mov [fr_z0], dl
        mov dl, [fl_z1]
        mov [fr_z1], dl
        mov dh, CAPA_DIR
        cmp byte [fl_dir], 0
        jne .cd
        mov dh, bl
.cd:
        mov [fr_capa], dh
.col:
        ; la entrada del mapa
        mov eax, [fr_tx]
        cmp dword [fl_16], 0
        je .m0
        shr eax, 1
.m0:
        and eax, 63
        mov edx, [fr_izq]
        test eax, 32
        jz .m1
        mov edx, [fr_der]
.m1:
        and eax, 31
        add eax, edx
        and eax, 0x7FFF
        movzx ebp, word [vram+eax*2]   ; EBP = la entrada
        ; el dibujo (en la caché de su número de bits)
        mov eax, ebp
        and eax, 0x3FF
        cmp dword [fl_16], 0        ; 16x16: +1 la mitad de la derecha, +16
        je .t8                      ; la de abajo (al revés si está volteado)
        mov edx, [fr_tx]
        and edx, 1
        test ebp, 0x4000
        jz .t16a
        xor edx, 1
.t16a:
        add eax, edx
        mov edx, [fr_mitad]
        test ebp, 0x8000
        jz .t16b
        xor edx, 1
.t16b:
        shl edx, 4
        add eax, edx
.t8:
        mov ecx, [cap_bpp]
        cmp ecx, 4
        je .b4
        ja .b8
        shl eax, 3
        add eax, [fl_dib]
        and eax, 0x7FFF
        shr eax, 3
        mov esi, eax
        shl esi, 6
        add esi, cache2
        cmp byte [sucio2+eax], 0
        je .b2a
        call dib2
.b2a:
        mov eax, ebp
        shr eax, 8
        and eax, 0x1C               ; la paleta * 4
        jmp .pal
.b4:
        shl eax, 4
        add eax, [fl_dib]
        and eax, 0x7FFF
        shr eax, 4
        mov esi, eax
        shl esi, 6
        add esi, cache4
        cmp byte [sucio4+eax], 0
        je .b4a
        call dib4
.b4a:
        mov eax, ebp
        shr eax, 6
        and eax, 0x70               ; la paleta * 16
        jmp .pal
.b8:
        shl eax, 5
        add eax, [fl_dib]
        and eax, 0x7FFF
        shr eax, 5
        mov esi, eax
        shl esi, 6
        add esi, cache8
        cmp byte [sucio8+eax], 0
        je .b8a
        call dib8
.b8a:
        xor eax, eax
.pal:
        add eax, [fl_palb]
        cmp byte [fl_dir], 0
        je .pd
        xor eax, eax
.pd:
        mov bl, al                  ; BL = lo que se suma a cada color
        ; la fila (volteada de arriba a abajo si toca)
        mov eax, [fr_fila]
        test ebp, 0x8000
        jz .v
        mov eax, [fr_filav]
.v:
        lea esi, [esi+eax*8]
        mov eax, [esi]
        or eax, [esi+4]
        jz near .sig                ; (toda la fila transparente)
        mov dl, [fr_z0]
        test ebp, 0x2000
        jz .z
        mov dl, [fr_z1]
.z:
        mov dh, [fr_capa]
        mov edi, [fl_x]
        mov ecx, [fl_cz]
        test ebp, 0x4000
        mov ebp, [fl_mask]          ; (EBP: ahora, la máscara)
        jnz near .volteado
        test ebp, ebp
        jnz near .masc
        PUNTO_FR 0, 0
        PUNTO_FR 1, 1
        PUNTO_FR 2, 2
        PUNTO_FR 3, 3
        PUNTO_FR 4, 4
        PUNTO_FR 5, 5
        PUNTO_FR 6, 6
        PUNTO_FR 7, 7
        jmp .sig
.masc:
        PUNTO_FRM 0, 0
        PUNTO_FRM 1, 1
        PUNTO_FRM 2, 2
        PUNTO_FRM 3, 3
        PUNTO_FRM 4, 4
        PUNTO_FRM 5, 5
        PUNTO_FRM 6, 6
        PUNTO_FRM 7, 7
        jmp .sig
.vmasc:
        PUNTO_FRM 0, 7
        PUNTO_FRM 1, 6
        PUNTO_FRM 2, 5
        PUNTO_FRM 3, 4
        PUNTO_FRM 4, 3
        PUNTO_FRM 5, 2
        PUNTO_FRM 6, 1
        PUNTO_FRM 7, 0
        jmp .sig
.volteado:
        test ebp, ebp
        jnz near .vmasc
        PUNTO_FR 0, 7
        PUNTO_FR 1, 6
        PUNTO_FR 2, 5
        PUNTO_FR 3, 4
        PUNTO_FR 4, 3
        PUNTO_FR 5, 2
        PUNTO_FR 6, 1
        PUNTO_FR 7, 0
.sig:
        add dword [fl_x], 8
        inc dword [fr_tx]
        cmp dword [fl_x], 256
        jl .col
        popad
        ret

; (interna) el fondo [cap_n] en la pantalla [fl_cz], con la máscara
; [fl_mask] (0: sin ella)
fondo_pinta:
        cmp byte [fl_opt], 0        ; lo de siempre: el camino rápido
        je fondo_rapido
        pushad
        mov ebx, [cap_n]
        movzx eax, word [bg_hofs+ebx*2]
        mov [fl_hofs], eax
        movzx eax, word [bg_vofs+ebx*2]
        mov [fl_vofs], eax
        mov eax, [fl_hofs]
        and eax, 7
        neg eax
        mov [fl_x], eax             ; el primer punto (de -7 a 0)
        mov dword [fl_col], 0
        cmp dword [fl_16], 0
        je .col
        mov eax, [fl_hofs]          ; (con dibujos de 16: de -15 a 0)
        and eax, 15
        neg eax
        mov [fl_x], eax
.col:
        ; el desplazamiento de esta columna
        mov eax, [fl_hofs]
        mov [fc_h], eax
        mov eax, [fl_vofs]
        mov [fc_v], eax
        cmp byte [fl_opt], 0
        je .sinopt
        cmp dword [fl_col], 0
        je .sinopt
        call opt_columna
.sinopt:
        ; la fila del fondo: (VOFS + línea), y la columna: HOFS + x
        mov eax, [fc_v]
        add eax, [lin_y]
        and eax, 0x3FF
        mov [fc_y], eax
        mov eax, [fl_x]
        add eax, [fc_h]
        and eax, 0x3FF
        mov [fc_xb], eax
        ; la entrada del mapa
        mov esi, [fc_xb]
        mov edi, [fc_y]
        cmp dword [fl_16], 0
        je .t8
        shr esi, 4
        shr edi, 4
        jmp .t9
.t8:
        shr esi, 3
        shr edi, 3
.t9:
        call mapa_entrada           ; ESI = columna, EDI = fila -> EAX
        mov [fc_ent], eax
        ; el dibujo y su fila
        mov ecx, eax
        and ecx, 0x3FF              ; el número
        mov edx, [fc_y]
        test eax, 0x8000            ; volteado de arriba a abajo
        jz .v0
        not edx
.v0:
        cmp dword [fl_16], 0
        je .v1
        test edx, 8                 ; (16x16: la mitad de abajo, +16)
        jz .v1
        add ecx, 16
.v1:
        and edx, 7
        mov [fc_fila], edx
        cmp dword [fl_16], 0
        je .uno
        ; dos dibujos de 8 (16 de ancho): el de la izquierda y el de la derecha
        mov ebp, ecx
        mov eax, [fc_ent]
        test eax, 0x4000
        jz .no_h
        inc ebp                     ; volteado: primero el de la derecha
.no_h:
        mov eax, ebp
        call fondo_ocho
        add dword [fl_x], 8
        mov ebp, ecx
        mov eax, [fc_ent]
        test eax, 0x4000
        jnz .h2
        inc ebp
.h2:
        mov eax, ebp
        call fondo_ocho
        add dword [fl_x], 8
        jmp .otra
.uno:
        mov eax, ecx
        call fondo_ocho
        add dword [fl_x], 8
.otra:
        inc dword [fl_col]
        cmp dword [fl_x], 256
        jl .col
        popad
        ret

; (interna) EAX = número del dibujo (en la base del fondo): sus 8 puntos de
; la fila [fc_fila], en el punto [fl_x], con [fc_ent] (paleta, prioridad,
; volteado)
fondo_ocho:
        pushad
        mov ecx, [cap_bpp]
        mov ebx, [fl_dib]
        cmp ecx, 4
        je .b4
        ja .b8
        shl eax, 3                  ; 2 bits: 8 palabras por dibujo
        add eax, ebx
        and eax, 0x7FFF
        shr eax, 3
        call dib2
        mov eax, [fc_ent]
        shr eax, 10
        and eax, 7
        shl eax, 2
        jmp .pal
.b4:
        shl eax, 4
        add eax, ebx
        and eax, 0x7FFF
        shr eax, 4
        call dib4
        mov eax, [fc_ent]
        shr eax, 10
        and eax, 7
        shl eax, 4
        jmp .pal
.b8:
        shl eax, 5
        add eax, ebx
        and eax, 0x7FFF
        shr eax, 5
        call dib8
        xor eax, eax
.pal:
        add eax, [fl_palb]
        mov [fo_pal], al
        mov eax, [fc_fila]
        lea esi, [esi+eax*8]        ; ESI = los 8 puntos de la fila
        mov eax, [esi]              ; ¿todos transparentes?
        or eax, [esi+4]
        jz near .x
        ; la altura
        mov edx, [fl_z0]
        test dword [fc_ent], 0x2000
        jz .z
        mov edx, [fl_z1]
.z:
        mov [fo_z], dl
        mov dh, CAPA_DIR
        cmp byte [fl_dir], 0
        jne .zd
        mov dh, [cap_n]
.zd:
        ; los 8 puntos (de izquierda a derecha, o al revés si está volteado)
        mov ebx, 1
        test dword [fc_ent], 0x4000
        jz .h
        add esi, 7
        mov ebx, -1
.h:
        mov edi, [fl_x]
        mov ecx, 8
        mov ebp, [fl_cz]            ; (z en +0, el color en +256)
        cmp dword [fl_mask], 0
        jne .pm
.p:
        movzx eax, byte [esi]
        add esi, ebx
        test al, al
        jz .s
        cmp edi, 256
        jae .s
        cmp dl, [ebp+edi]
        jbe .s
        cmp byte [fl_dir], 0
        jne .d
        add al, [fo_pal]
.d:
        mov ah, dh
        mov [ebp+edi], dl
        mov [ebp+256+edi*2], ax
.s:
        inc edi
        dec ecx
        jnz .p
        popad
        ret
.pm:                                ; (lo mismo, con la máscara de la ventana)
        movzx eax, byte [esi]
        add esi, ebx
        test al, al
        jz .ms
        cmp edi, 256
        jae .ms
        cmp dl, [ebp+edi]
        jbe .ms
        push esi
        mov esi, [fl_mask]
        cmp byte [esi+edi], 0
        pop esi
        jne .ms
        cmp byte [fl_dir], 0
        jne .md
        add al, [fo_pal]
.md:
        mov ah, dh
        mov [ebp+edi], dl
        mov [ebp+256+edi*2], ax
.ms:
        inc edi
        dec ecx
        jnz .pm
.x:
        popad
        ret

; (interna) ESI = columna, EDI = fila (en dibujos) -> EAX = la entrada del
; mapa del fondo, con los mapas de 32x32 que tenga
mapa_entrada:
        push edx
        mov eax, edi
        and eax, 31
        shl eax, 5
        mov edx, esi
        and edx, 31
        add eax, edx
        test esi, 32
        jz .a
        test byte [fl_sc], 1
        jz .a
        add eax, 0x400
.a:
        test edi, 32
        jz .b
        test byte [fl_sc], 2
        jz .b
        add eax, 0x400
        test byte [fl_sc], 1
        jz .b
        add eax, 0x400
.b:
        add eax, [fl_mapa]
        and eax, 0x7FFF
        movzx eax, word [vram+eax*2]
        pop edx
        ret

; (interna) el desplazamiento de la columna [fl_col] con OPT: lo toma del
; mapa del fondo 3 -> [fc_h], [fc_v]
opt_columna:
        pushad
        ; el mapa del fondo 3
        movzx eax, byte [bgsc+2]
        mov edx, eax
        and edx, 3
        and eax, 0xFC
        shl eax, 8
        push dword [fl_sc]
        push dword [fl_mapa]
        mov [fl_sc], edx
        mov [fl_mapa], eax
        movzx esi, word [bg_hofs+4]
        shr esi, 3
        add esi, [fl_col]
        dec esi
        movzx edi, word [bg_vofs+4]
        shr edi, 3
        mov ebx, [cap_n]
        mov ecx, 0x2000             ; el bit de este fondo
        shl ecx, cl
        cmp ebx, 1
        jne .c
        mov ecx, 0x4000
.c:
        cmp dword [modo], 4
        je .m4
        call mapa_entrada           ; la fila 0: el horizontal
        test eax, ecx
        jz .v
        mov edx, [fc_h]
        and edx, 7
        and eax, 0x3F8
        or eax, edx
        mov [fc_h], eax
.v:
        inc edi                     ; la fila 1: el vertical
        call mapa_entrada
        test eax, ecx
        jz .x
        and eax, 0x3FF
        mov [fc_v], eax
        jmp .x
.m4:
        call mapa_entrada           ; modo 4: una fila, el bit 15 dice cuál
        test eax, ecx
        jz .x
        test eax, 0x8000
        jnz .m4v
        mov edx, [fc_h]
        and edx, 7
        and eax, 0x3F8
        or eax, edx
        mov [fc_h], eax
        jmp .x
.m4v:
        and eax, 0x3FF
        mov [fc_v], eax
.x:
        pop dword [fl_mapa]
        pop dword [fl_sc]
        popad
        ret

; =============================================================================
;  El modo 7: el fondo 1 (y el 2 con EXTBG), girado y escalado
; =============================================================================
modo7_linea:
        pushad
        mov ebx, [cap_n]
        cmp ebx, 1
        ja .x
        jb .b1
        test byte [setini], 0x40    ; el fondo 2 solo con EXTBG
        jz .x
.b1:
        ; el centro y el desplazamiento (13 bits con signo)
        movsx eax, word [m7hofs]
        shl eax, 19
        sar eax, 19
        movsx edx, word [m7a+8]     ; M7X
        shl edx, 19
        sar edx, 19
        mov [m7_cx], edx
        sub eax, edx
        mov [m7_hx], eax            ; HOFS - CX
        movsx eax, word [m7vofs]
        shl eax, 19
        sar eax, 19
        movsx edx, word [m7a+10]    ; M7Y
        shl edx, 19
        sar edx, 19
        mov [m7_cy], edx
        sub eax, edx
        mov [m7_vy], eax            ; VOFS - CY
        ; la fila (volteada si M7SEL bit 1)
        mov eax, [lin_y]
        test byte [m7sel], 2
        jz .y
        xor eax, 255
.y:
        add eax, [m7_vy]
        ; X0 = A*(HOFS-CX) + B*(y+VOFS-CY) + CX*256 ; lo mismo con C, D, CY
        movsx ecx, word [m7a+2]     ; B
        imul ecx, eax
        movsx edx, word [m7a+6]     ; D
        imul edx, eax
        movsx eax, word [m7a]       ; A
        imul eax, [m7_hx]
        add ecx, eax
        mov eax, [m7_cx]
        shl eax, 8
        add ecx, eax
        mov [m7_x0], ecx
        movsx eax, word [m7a+4]     ; C
        imul eax, [m7_hx]
        add edx, eax
        mov eax, [m7_cy]
        shl eax, 8
        add edx, eax
        mov [m7_y0], edx
        movsx eax, word [m7a]
        mov [m7_dx], eax
        movsx eax, word [m7a+4]
        mov [m7_dy], eax
        test byte [m7sel], 1        ; volteado de izquierda a derecha
        jz .nh
        mov eax, [m7_dx]            ; (empieza en el 255 y va al revés)
        imul eax, 255
        add [m7_x0], eax
        mov eax, [m7_dy]
        imul eax, 255
        add [m7_y0], eax
        neg dword [m7_dx]
        neg dword [m7_dy]
.nh:
        ; los 256 puntos, a m7_lin (el color, 0 = transparente)
        mov esi, [m7_x0]
        mov edi, [m7_y0]
        xor ecx, ecx
.p:
        mov eax, esi
        sar eax, 8
        mov edx, edi
        sar edx, 8
        test eax, ~0x3FF            ; ¿fuera del mapa de 1024x1024?
        jnz .fuera
        test edx, ~0x3FF
        jnz .fuera
.dentro:
        push ecx
        mov ecx, edx                ; la entrada: la parte baja de la palabra
        shr ecx, 3                  ; (fila * 128 + columna)
        shl ecx, 7
        mov ebp, eax
        shr ebp, 3
        add ecx, ebp
        movzx ebp, byte [vram+ecx*2]
        shl ebp, 6                  ; el dibujo: 64 palabras, en la parte alta
        and edx, 7
        shl edx, 3
        add ebp, edx
        and eax, 7
        add ebp, eax
        movzx eax, byte [vram+ebp*2+1]
        pop ecx
        jmp .pon
.fuera:
        test byte [m7sel], 0x80
        jz .envuelve
        test byte [m7sel], 0x40
        jz .transp
        and eax, 7                  ; fuera: el dibujo 0
        and edx, 7
        shl edx, 3
        add eax, edx
        movzx eax, byte [vram+eax*2+1]
        jmp .pon
.envuelve:
        and eax, 0x3FF
        and edx, 0x3FF
        jmp .dentro
.transp:
        xor eax, eax
.pon:
        mov [m7_lin+ecx], al
        add esi, [m7_dx]
        add edi, [m7_dy]
        inc ecx
        cmp ecx, 256
        jb .p
        ; a las pantallas
        mov ebx, [cap_n]
        movzx eax, byte [tm]
        bt eax, ebx
        jnc .s
        mov dword [fl_cz], pant_m
        mov eax, [vmask_m+ebx*4]
        mov [fl_mask], eax
        call m7_pinta
.s:
        cmp byte [con_sub], 0
        je .x
        movzx eax, byte [ts]
        bt eax, ebx
        jnc .x
        mov dword [fl_cz], pant_s
        mov eax, [vmask_s+ebx*4]
        mov [fl_mask], eax
        call m7_pinta
.x:
        popad
        ret
; (interna) m7_lin a la pantalla [fl_cz] (EBX = el fondo)
m7_pinta:
        pushad
        mov dh, bl                  ; la capa (o color directo)
        test bl, bl
        jnz .c
        test byte [cgwsel], 1
        jz .c
        mov dh, CAPA_DIR
.c:
        mov byte [m7_z1], 2         ; la altura del fondo 1: 2 (3 con EXTBG)
        test byte [setini], 0x40
        jz .c1
        mov byte [m7_z1], 3
.c1:
        xor ecx, ecx
        mov edi, [fl_cz]
.p:
        movzx eax, byte [m7_lin+ecx]
        mov dl, [m7_z1]
        test bl, bl
        jz .z
        test al, 0x80               ; fondo 2: el bit 7 es la prioridad
        mov dl, 1
        jz .z1
        mov dl, 5
.z1:
        and al, 0x7F
.z:
        test al, al
        jz .s
        cmp dl, [edi+ecx]
        jbe .s
        mov esi, [fl_mask]
        test esi, esi
        jz .m
        cmp byte [esi+ecx], 0
        jne .s
.m:
        mov [edi+ecx], dl
        mov ah, dh
        mov [edi+256+ecx*2], ax
.s:
        inc ecx
        cmp ecx, 256
        jb .p
        popad
        ret

; =============================================================================
;  Los objetos de la línea
; =============================================================================
objetos_linea:
        pushad
        ; la fila de la pantalla (los objetos van una línea más abajo)
        mov eax, [lin_y]
        dec eax
        mov [ob_y], eax
        ; los de esta línea (32 como mucho), desde el primero (la rotación)
        xor ebx, ebx                ; EBX = cuántos
        xor ecx, ecx
        test byte [oam_rot], 0x80
        jz .r
        mov ecx, [oam_recarga]
        shr ecx, 1
        and ecx, 0x7F
.r:
        mov [ob_prim], ecx
        cmp byte [oam_sucia], 0     ; la tabla de los 128, al día
        je .t
        call obj_tabla
.t:
        mov ebp, 128
.busca:
        mov edx, ecx                ; (de la tabla: x, y, ancho, alto)
        shl edx, 4
        mov eax, [ob_y]
        sub eax, [ob_tab+edx+4]
        and eax, 0xFF
        cmp eax, [ob_tab+edx+12]
        jae .no
        ; ¿y en x?
        mov eax, [ob_tab+edx]
        cmp eax, 256
        jge .no
        add eax, [ob_tab+edx+8]
        jle .no
        cmp ebx, 32
        jb .cabe
        or byte [stat77], 0x40      ; (más de 32 en la línea)
        jmp .lleno
.cabe:
        mov [ob_lista+ebx*4], ecx
        inc ebx
.no:
        inc ecx
        and ecx, 0x7F
        dec ebp
        jnz .busca
.lleno:
        test ebx, ebx
        jz near .x
        mov [ob_n], ebx
        ; la línea de objetos, de detrás (el último) a delante (el primero)
        mov edi, ob_c
        mov ecx, 256 / 4
        xor eax, eax
        rep stosd
        mov ebx, [ob_n]
.pinta:
        dec ebx
        js .mezcla
        mov ecx, [ob_lista+ebx*4]
        call obj_datos
        call obj_pinta
        jmp .pinta
.mezcla:
        ; a las pantallas
        mov eax, [modo]
        movzx eax, byte [z_grupo+eax]
        lea esi, [z_obj+eax*4]
        cmp eax, 3
        jne .z
        test byte [setini], 0x40
        jz .z
        mov esi, z_obj_ext
.z:
        mov [ob_zt], esi
        test byte [tm], 0x10
        jz .s
        mov dword [fl_cz], pant_m
        mov eax, [vmask_m+16]
        mov [fl_mask], eax
        call obj_mezcla
.s:
        cmp byte [con_sub], 0
        je .x
        test byte [ts], 0x10
        jz .x
        mov dword [fl_cz], pant_s
        mov eax, [vmask_s+16]
        mov [fl_mask], eax
        call obj_mezcla
.x:
        popad
        ret

; (interna) la tabla de los objetos (ob_tab: x, y, ancho y alto de cada
; uno), otra vez: después de escribir en la OAM o en OBSEL
obj_tabla:
        pushad
        mov byte [oam_sucia], 0
        xor ecx, ecx
.o:
        call obj_datos
        mov edx, ecx
        shl edx, 4
        mov eax, [ob_ox]
        mov [ob_tab+edx], eax
        mov eax, [ob_oy]
        mov [ob_tab+edx+4], eax
        mov eax, [ob_an]
        mov [ob_tab+edx+8], eax
        mov eax, [ob_al]
        mov [ob_tab+edx+12], eax
        inc ecx
        cmp ecx, 128
        jb .o
        popad
        ret

; (interna) ECX = número del objeto -> ob_ox, ob_oy, ob_an, ob_al, ob_t
; (dibujo y atributos)
obj_datos:
        push eax
        push edx
        mov eax, ecx
        shr eax, 2
        movzx edx, byte [oam+0x200+eax]
        mov eax, ecx
        and eax, 3
        shl eax, 1
        push ecx
        mov ecx, eax
        shr edx, cl
        pop ecx
        movzx eax, byte [oam+ecx*4]   ; x (9 bits, con signo)
        test edx, 1
        jz .x
        or eax, 0x100
        sub eax, 512
.x:
        mov [ob_ox], eax
        movzx eax, byte [oam+ecx*4+1]
        mov [ob_oy], eax
        movzx eax, word [oam+ecx*4+2]
        mov [ob_t], eax
        shr edx, 1                  ; el tamaño: pequeño o grande
        and edx, 1
        movzx eax, byte [obj_tam+edx*2]
        mov [ob_an], eax
        movzx eax, byte [obj_tam+edx*2+1]
        mov [ob_al], eax
        pop edx
        pop eax
        ret

; (interna) el objeto de ob_* en la línea de objetos (ob_c: color, ob_p:
; prioridad y si suma)
obj_pinta:
        pushad
        mov eax, [ob_y]
        sub eax, [ob_oy]
        and eax, 0xFF               ; la fila dentro del objeto
        test dword [ob_t], 0x8000   ; volteado de arriba a abajo
        jz .v
        mov edx, [ob_al]
        dec edx
        sub edx, eax
        mov eax, edx
.v:
        mov [ob_fila], eax
        ; la paleta y la prioridad
        mov edx, [ob_t]
        mov eax, edx
        shr eax, 9
        and eax, 7
        shl eax, 4
        add eax, 128
        mov [ob_pal], al
        mov eax, edx
        shr eax, 12
        and eax, 3
        test edx, 0x0800            ; las paletas 4-7 suman colores
        jz .p
        or al, 4
.p:
        mov [ob_pri], al
        ; las columnas de 8 puntos
        xor ebx, ebx                ; EBX = columna (en dibujos)
.col:
        mov eax, ebx
        shl eax, 3
        cmp eax, [ob_an]
        jae near .x
        add eax, [ob_ox]
        cmp eax, 256
        jge near .x
        cmp eax, -8
        jle near .sig
        mov [ob_px], eax
        ; el dibujo: la columna (volteada si toca) y la fila
        mov eax, ebx
        test dword [ob_t], 0x4000
        jz .h
        mov eax, [ob_an]
        shr eax, 3
        dec eax
        sub eax, ebx
.h:
        mov ecx, [ob_t]
        mov edx, ecx
        and edx, 0x0F
        add edx, eax
        and edx, 0x0F
        mov eax, ecx
        shr eax, 4
        and eax, 0x0F
        mov esi, [ob_fila]
        shr esi, 3
        add eax, esi
        and eax, 0x0F
        shl eax, 4
        or eax, edx                 ; el dibujo (8 bits)
        shl eax, 4                  ; (16 palabras cada uno)
        add eax, [obj_base]
        test ecx, 0x100             ; la segunda tabla
        jz .t
        add eax, [obj_hueco]
.t:
        and eax, 0x7FFF
        shr eax, 4
        call dib4
        mov eax, [ob_fila]
        and eax, 7
        lea esi, [esi+eax*8]
        mov eax, [esi]
        or eax, [esi+4]
        jz .sig
        mov edx, 1
        test dword [ob_t], 0x4000
        jz .h2
        add esi, 7
        mov edx, -1
.h2:
        mov edi, [ob_px]
        mov ecx, 8
.pp:
        movzx eax, byte [esi]
        add esi, edx
        test al, al
        jz .ps
        cmp edi, 256
        jae .ps
        add al, [ob_pal]
        mov [ob_c+edi], al
        mov al, [ob_pri]
        mov [ob_p+edi], al
.ps:
        inc edi
        dec ecx
        jnz .pp
.sig:
        inc ebx
        jmp .col
.x:
        popad
        ret

; (interna) la línea de objetos a [fl_cz]/[fl_cc], con la máscara [fl_mask]
obj_mezcla:
        pushad
        mov esi, [ob_zt]
        mov edi, [fl_cz]
        lea ebp, [edi+256]
        xor ecx, ecx
.p:
        movzx eax, byte [ob_c+ecx]
        test al, al
        jz .s
        movzx edx, byte [ob_p+ecx]
        mov ebx, edx
        and ebx, 3
        mov bl, [esi+ebx]
        cmp bl, [edi+ecx]
        jbe .s
        mov ebx, [fl_mask]
        test ebx, ebx
        jz .m
        cmp byte [ebx+ecx], 0
        jne .s
.m:
        mov ebx, edx
        and ebx, 3
        mov bl, [esi+ebx]
        mov [edi+ecx], bl
        mov ah, 4                   ; (la capa: 4 si suma, 7 si no)
        test dl, 4
        jnz .c
        mov ah, CAPA_OBJ0
.c:
        mov [ebp+ecx*2], ax
.s:
        inc ecx
        cmp ecx, 256
        jb .p
        popad
        ret

; =============================================================================
;  Los colores de la línea: cm (y cs) -> la fila de la imagen
; =============================================================================
;  Los colores de la línea: pant_m (y pant_s) -> la fila de la imagen
; =============================================================================
; Lo de toda la línea se decide antes: qué capas suman ([pc_suma], un bit
; por capa), y si la ventana del color recorta a negro o impide la suma
; (pc_clip y pc_prev: dos octetos, fuera y dentro de la ventana). Los puntos
; que no suman ni se recortan salen de cg_host (ya con el brillo); los demás
; se suman con los tres componentes a la vez (en una palabra de 15 bits).
ppu_colores:
        pushad
        mov eax, [lin_y]
        dec eax
        mov edi, eax
        shl edi, 8
        add edi, imagen
        mov [pc_dest], edi
        shl eax, 9
        add eax, imagen15
        mov [pc_dest15], eax
        ; las capas que suman (el color directo, con el fondo 1)
        movzx eax, byte [cgadsub]
        and al, 0x3F
        test al, 1
        jz .s1
        or al, 0x40
.s1:
        mov [pc_suma], eax
        ; el recorte y la suma impedida: (fuera, dentro) para cada modo
        movzx eax, byte [cgwsel]
        shr eax, 6
        mov ax, [vmodo_tab+eax*2]
        mov [pc_clip], ax
        movzx eax, byte [cgwsel]
        shr eax, 4
        and eax, 3
        mov ax, [vmodo_tab+eax*2]
        mov [pc_prev], ax
        cmp byte [hay_colv], 0      ; (sin ventana del color: todo está fuera)
        jne .s2
        mov al, [pc_clip]
        mov [pc_clip+1], al
        mov al, [pc_prev]
        mov [pc_prev+1], al
        cmp byte [pc_prev], 0       ; impedida en toda la línea: nada suma
        je .s2
        mov dword [pc_suma], 0
.s2:
        movzx eax, byte [inidisp]
        and eax, 0x0F
        cmp eax, 15
        sete [pc_b15]
        ; ¿lo rápido? (ni suma ni recorte en toda la línea)
        cmp byte [ppu_15], 0
        jne .general
        cmp dword [pc_suma], 0
        jne .general
        cmp word [pc_clip], 0
        jne .general
        mov esi, pant_m+256
        xor ecx, ecx
.r:
        movzx eax, word [esi+ecx*2]
        cmp ah, CAPA_DIR
        je .rd
        movzx eax, al
        mov al, [cg_host+eax]
        mov [edi+ecx], al
        inc ecx
        cmp ecx, 256
        jb .r
        popad
        ret
.rd:
        call color_directo
        call brillo15
        mov al, [lut15+eax]
        mov [edi+ecx], al
        inc ecx
        cmp ecx, 256
        jb .r
        popad
        ret
.general:
        ; sin la ventana del color (lo más corriente) y sin recortar: qué capas
        ; suman, en una tabla, y lo demás de cg_host
        cmp byte [ppu_15], 0
        jne .todo
        cmp byte [hay_colv], 0
        jne .todo
        cmp byte [pc_clip], 0
        jne .todo
        xor ebx, ebx
.tc:
        bt dword [pc_suma], ebx
        setc byte [pc_capas+ebx]
        inc ebx
        cmp ebx, 8
        jb .tc
        xor ecx, ecx
        xor edx, edx                ; (fuera de la ventana: para .suma)
.q:
        movzx eax, word [pant_m+256+ecx*2]
        movzx ebx, ah
        cmp byte [pc_capas+ebx], 0
        jne .qs
        cmp ebx, CAPA_DIR
        je .qd
        movzx eax, al
        mov al, [cg_host+eax]
        mov [edi+ecx], al
        inc ecx
        cmp ecx, 256
        jb .q
        popad
        ret
.qd:
        call color_de
        jmp .qp
.qs:
        call pc_suma_punto
.qp:
        cmp byte [pc_b15], 0
        jne .qb
        call brillo15
.qb:
        mov al, [lut15+eax]
        mov [edi+ecx], al
        inc ecx
        cmp ecx, 256
        jb .q
        popad
        ret
.todo:
        xor ecx, ecx
.p:
        movzx eax, word [pant_m+256+ecx*2]
        movzx ebx, ah               ; EBX = la capa
        xor edx, edx                ; EDX = dentro de la ventana del color
        cmp byte [hay_colv], 0
        je .w
        mov dl, [vmasc+5*256+ecx]
.w:
        ; ¿suma esta capa? ¿se recorta?
        mov ebp, [pc_suma]
        cmp byte [pc_prev+edx], 0
        je .w1
        xor ebp, ebp
.w1:
        bt ebp, ebx
        jc .suma
        cmp byte [pc_clip+edx], 0
        jne .negro
        cmp ebx, CAPA_DIR
        je .dir
        cmp byte [ppu_15], 0        ; (ni suma ni recorte: el de siempre)
        jne .dir
        movzx eax, al
        mov al, [cg_host+eax]
        mov esi, [pc_dest]
        mov [esi+ecx], al
        jmp .sig
.negro:
        xor eax, eax
        jmp .pon
.dir:
        call color_de
        jmp .pon
.suma:
        call pc_suma_punto
        jmp .pon
; (interna) el punto ECX de la principal (EAX = su palabra, EBX = su capa,
; EDX = dentro de la ventana del color) con lo que se le suma -> EAX
pc_suma_punto:
        call color_de
        mov [pc_c1], eax
        mov byte [pc_mitad], 1
        test byte [cgadsub], 0x40
        jnz .m0
        mov byte [pc_mitad], 0
.m0:
        cmp byte [pc_clip+edx], 0
        je .m1
        mov dword [pc_c1], 0        ; (a negro: y sin la mitad)
        mov byte [pc_mitad], 0
.m1:
        mov eax, [fijo15]
        test byte [cgwsel], 2
        jz .fijo
        movzx eax, word [pant_s+256+ecx*2]
        movzx edx, ah
        cmp edx, CAPA_FONDO
        jne .sub
        mov eax, [fijo15]           ; (la secundaria vacía: el fijo, sin mitad)
        mov byte [pc_mitad], 0
        jmp .fijo
.sub:
        push ebx
        mov ebx, edx
        call color_de
        pop ebx
.fijo:
        mov [pc_c2], eax
        jmp suma_colores
ppu_colores.pon:
        cmp byte [pc_b15], 0
        jne ppu_colores.b15
        call brillo15
ppu_colores.b15:
        cmp byte [ppu_15], 0
        jne ppu_colores.p15
        mov esi, [pc_dest]
        mov al, [lut15+eax]
        mov [esi+ecx], al
        jmp ppu_colores.sig
ppu_colores.p15:
        mov esi, [pc_dest15]
        mov [esi+ecx*2], ax
ppu_colores.sig:
        inc ecx
        cmp ecx, 256
        jb ppu_colores.p
        popad
        ret
; (fuera, dentro) de la ventana para cada modo: nunca, fuera, dentro, siempre
vmodo_tab   db 0, 0,  1, 0,  0, 1,  1, 1

; EAX = palabra de cm/cs (color | capa << 8), EBX = la capa -> EAX = 15 bits
color_de:
        cmp ebx, CAPA_DIR
        je color_directo
        movzx eax, al
        movzx eax, word [cgram+eax*2]
        ret
; AL = color de 8 bits (BBGGGRRR) -> EAX = 15 bits
color_directo:
        push edx
        movzx edx, al
        mov eax, edx
        and eax, 7
        shl eax, 2                  ; R
        push eax
        mov eax, edx
        shr eax, 3
        and eax, 7
        shl eax, 7                  ; G (<<5, y 2 más)
        or [esp], eax
        mov eax, edx
        shr eax, 6
        and eax, 3
        shl eax, 13                 ; B (<<10, y 3 más)
        or eax, [esp]
        add esp, 4
        pop edx
        ret

; [pc_c1] y [pc_c2]: suma o resta (CGADSUB bit 7), a la mitad si [pc_mitad]
; -> EAX. Los tres componentes a la vez: en la suma, los acarreos que salen
; de cada uno (bits 5, 10 y 15) dicen cuáles se pasan de 31; en la resta,
; cada componente va con un bit de guarda delante (separados en 32 bits)
suma_colores:
        push ebx
        push edx
        mov eax, [pc_c1]
        mov ebx, [pc_c2]
        test byte [cgadsub], 0x80
        jnz .resta
        cmp byte [pc_mitad], 0
        jne .media
        lea edx, [eax+ebx]          ; la suma, con el tope de 31
        xor eax, ebx
        xor eax, edx
        and eax, 0x8420             ; los acarreos de cada componente
        sub edx, eax
        mov ebx, eax
        shr ebx, 5
        sub eax, ebx                ; (los componentes pasados: 11111)
        or eax, edx
        and eax, 0x7FFF
        pop edx
        pop ebx
        ret
.media:
        mov edx, eax                ; (a + b) / 2 sin pasarse
        xor edx, ebx
        and edx, 0x7BDE
        shr edx, 1
        and eax, ebx
        add eax, edx
        pop edx
        pop ebx
        ret
.resta:
        call .separa
        mov edx, eax
        mov eax, ebx
        call .separa
        mov ebx, eax                ; EDX = a, EBX = b, separados
        or edx, (1 << 5) | (1 << 16) | (1 << 27)
        sub edx, ebx
        mov eax, edx
        and eax, (1 << 5) | (1 << 16) | (1 << 27)
        mov ebx, eax
        shr ebx, 5
        sub eax, ebx                ; (los que no se han pasado de 0: 11111)
        and eax, edx
        cmp byte [pc_mitad], 0
        je .junta
        shr eax, 1
        and eax, 0x0F | (0x0F << 11) | (0x0F << 22)
.junta:
        mov edx, eax                ; y otra vez en 15 bits
        and edx, 0x1F
        mov ebx, eax
        shr ebx, 6
        and ebx, 0x3E0
        or edx, ebx
        shr eax, 12
        and eax, 0x7C00
        or eax, edx
        pop edx
        pop ebx
        ret
; EAX = 15 bits -> EAX con los componentes en los bits 0, 11 y 22
.separa:
        push edx
        mov edx, eax
        and edx, 0x1F
        push edx
        mov edx, eax
        and edx, 0x3E0
        shl edx, 6
        or [esp], edx
        and eax, 0x7C00
        shl eax, 12
        or eax, [esp]
        add esp, 4
        pop edx
        ret
