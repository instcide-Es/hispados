; =============================================================================
;  Hispa-Nes - la imagen (el procesador de imagen de la consola), los
;  registros de 2000h a 401Fh, los mandos y el cuadro entero
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del hardware: registros 2000h a
;  2007h, los registros internos v, t, x y w del desplazamiento, las tablas
;  de nombres y de atributos, los dibujos de dos planos, los 64 objetos
;  («sprites») con su prioridad y el aviso del objeto 0.
;
;  Se dibuja por líneas: cada línea visible se pinta de una vez con el estado
;  que hay al empezarla y después corre el procesador el tiempo de esa línea
;  (341 puntos; 3 puntos por ciclo). En el punto 260 la imagen pasa a la
;  línea siguiente (y cuenta la línea el cartucho que lo necesite). El aviso
;  del objeto 0 se da en el punto exacto en el que coincide con el fondo.
;  Sin hacer: los colores apagados o reforzados del registro 2001h (énfasis
;  y grises) y el cuadro impar un punto más corto.
;
;    nes_cuadro      emula un cuadro (262 líneas). Con [dibujar]=1 deja la
;                    imagen en «imagen»: 256x240, un octeto por punto con el
;                    color de la consola (0-63)
;    ppu_reiniciar   registros de imagen como al encender
;    ppu_tablas      (una vez) las tablas para pasar los dibujos a puntos
;    pad_estado      (lo pone quien llama) los botones de los dos mandos:
;                    bit 0 A, 1 B, 2 Select, 3 Start, 4 arriba, 5 abajo,
;                    6 izquierda, 7 derecha
; =============================================================================

PUNTOS_LINEA equ 341
PUNTO_SALTO equ 260                 ; aquí la imagen pasa a la línea siguiente

        align 4
tab_atr     dd 0, 0x04040404, 0x08080808, 0x0C0C0C0C

ppu_tablas:
        pushad
        mov edi, tab_plano          ; para cada octeto de un plano, sus ocho
        xor ebx, ebx                ; puntos (uno por octeto, 0 o 1)
.o:
        mov ecx, 8
        mov dl, bl
.b:
        xor eax, eax
        shl dl, 1
        adc eax, 0
        stosb
        dec ecx
        jnz .b
        inc ebx
        cmp ebx, 256
        jb .o
        popad
        ret

ppu_reiniciar:
        pushad
        xor eax, eax
        mov [ppu_ctrl], al
        mov [ppu_mask], al
        mov [ppu_status], al
        mov [oam_addr], al
        mov [ppu_w], al
        mov [ppu_x], al
        mov [ppu_buf], al
        mov [ppu_v], eax
        mov [ppu_t], eax
        mov [pad_strobe], al
        mov dword [s0_punto], -1
        mov dword [punto_fin], PUNTOS_LINEA
        popad
        ret

; ---- un cuadro ----
nes_cuadro:
        pushad
        xor eax, eax
        mov [linea], eax
.visible:
        cmp byte [dibujar], 0
        jne .pinta
        call s0_en_linea            ; sin pintar: solo las líneas donde puede
        jnc .nopinta                ; coincidir el objeto 0
.pinta:
        call ppu_dibuja
.nopinta:
        mov dword [punto_fin], PUNTO_SALTO
        mov edi, PUNTO_SALTO
        call corre_tramo
        call s0_vence
        test byte [ppu_mask], 0x18
        jz .sinimg
        call ppu_baja
        call [map_linea]
.sinimg:
        mov dword [punto_fin], PUNTOS_LINEA
        mov edi, PUNTOS_LINEA - PUNTO_SALTO
        call corre_tramo
        call apu_linea
        inc dword [linea]
        cmp dword [linea], 240
        jb .visible
        ; línea 240: nada. 241: empieza el retorno vertical (y la NMI)
        mov edi, PUNTOS_LINEA
        call corre_tramo
        call apu_linea
        or byte [ppu_status], 0x80
        test byte [ppu_ctrl], 0x80
        jz .v
        mov byte [nmi_pend], 1
.v:
        mov dword [linea], 241
.vbl:
        mov edi, PUNTOS_LINEA
        call corre_tramo
        call apu_linea
        inc dword [linea]
        cmp dword [linea], 261
        jb .vbl
        ; línea 261: la de antes de la imagen
        and byte [ppu_status], 0x1F
        mov dword [s0_punto], -1
        mov dword [punto_fin], PUNTO_SALTO
        mov edi, PUNTO_SALTO
        call corre_tramo
        test byte [ppu_mask], 0x18
        jz .sinimg2
        mov eax, [ppu_t]            ; el desplazamiento entero, de t a v
        mov [ppu_v], eax
        call [map_linea]
.sinimg2:
        mov dword [punto_fin], PUNTOS_LINEA
        mov edi, PUNTOS_LINEA - PUNTO_SALTO
        call corre_tramo
        call apu_linea
        inc dword [cuadros]
        popad
        ret

; el procesador corre EDI puntos, hasta el punto [punto_fin] de la línea. Si
; en medio le toca un paso al secuenciador del sonido (envolventes,
; duraciones y su interrupción), se parte ahí: el paso cae en su ciclo
corre_tramo:
        push eax
        push ebx
        push edx
.otra:
        mov eax, PTS_PASO
        sub eax, [fc_acc]           ; lo que falta para el paso
        cmp eax, edi
        jg .todo
        mov ebx, [punto_fin]
        mov edx, edi
        sub edx, eax                ; lo que quedará después
        push edx
        neg edx
        add edx, ebx
        mov [punto_fin], edx
        mov dword [fc_acc], PTS_PASO
        mov edi, eax
        call cpu_correr
        mov dword [fc_acc], 0
        call apu_paso
        mov [punto_fin], ebx
        pop edi
        jmp .otra
.todo:
        add [fc_acc], edi
        call cpu_correr
        pop edx
        pop ebx
        pop eax
        ret

; el aviso del objeto 0 que quedaba pendiente en esta línea
s0_vence:
        cmp dword [s0_punto], 0
        jl .x
        or byte [ppu_status], 0x40
        mov dword [s0_punto], -1
.x:
        ret

; CF=1 si en esta línea puede coincidir el objeto 0 (y aún no ha avisado)
s0_en_linea:
        test byte [ppu_status], 0x40
        jnz .no
        mov al, [ppu_mask]
        and al, 0x18
        cmp al, 0x18
        jne .no
        mov eax, [linea]
        movzx edx, byte [oam]
        sub eax, edx
        dec eax
        js .no
        mov edx, 8
        test byte [ppu_ctrl], 0x20
        jz .a
        mov edx, 16
.a:
        cmp eax, edx
        jae .no
        stc
        ret
.no:
        clc
        ret

; al acabar la parte visible de una línea: v baja una línea y toma de t el
; desplazamiento horizontal
ppu_baja:
        mov eax, [ppu_v]
        mov edx, eax
        and edx, 0x7000
        cmp edx, 0x7000
        je .fila
        add eax, 0x1000
        jmp .h
.fila:
        and eax, 0x0FFF
        mov edx, eax
        shr edx, 5
        and edx, 31
        cmp edx, 29
        jne .f1
        xor edx, edx
        xor eax, 0x0800
        jmp .f2
.f1:
        inc edx
        and edx, 31
.f2:
        and eax, ~0x03E0
        shl edx, 5
        or eax, edx
.h:
        and eax, ~0x041F
        mov edx, [ppu_t]
        and edx, 0x041F
        or eax, edx
        mov [ppu_v], eax
        ret

; ---- pinta la línea [linea] ----
ppu_dibuja:
        pushad
        test byte [ppu_mask], 0x18
        jnz .hay
        mov edi, [linea]            ; imagen apagada: el color de fondo
        shl edi, 8
        add edi, imagen
        movzx eax, byte [pal_vis]
        imul eax, 0x01010101
        mov ecx, 64
        rep stosd
        popad
        ret
.hay:
        test byte [ppu_mask], 0x08
        jnz .fondo
        mov edi, lin_buf + 8
        xor eax, eax
        mov ecx, 64
        rep stosd
        jmp .objetos
.fondo:
        mov eax, [ppu_v]
        mov edx, eax
        shr edx, 12
        and edx, 7
        test byte [ppu_ctrl], 0x10
        jz .b0
        add edx, 0x1000
.b0:
        mov [r_base], edx           ; dónde están los dibujos + la fila del dibujo
        mov edx, eax
        shr edx, 10
        and edx, 3
        mov [r_nt], edx
        mov ecx, eax
        and ecx, 31                 ; ECX = columna
        shr eax, 5
        and eax, 31                 ; la fila de la tabla de nombres
        mov edx, eax
        shl edx, 5
        mov [r_fila], edx
        mov edx, eax
        shr edx, 2
        lea edx, [edx*8+0x3C0]
        mov [r_afila], edx
        and eax, 2
        shl eax, 1
        mov [r_ash], eax
        movzx eax, byte [ppu_x]
        mov edi, lin_buf + 8
        sub edi, eax
        mov dword [r_n], 33
.t:
        mov esi, [r_nt]
        mov ebx, [nt_pg+esi*4]
        mov eax, [r_fila]
        add eax, ecx
        movzx eax, byte [ebx+eax]   ; el dibujo
        mov edx, ecx
        shr edx, 2
        add edx, [r_afila]
        movzx edx, byte [ebx+edx]   ; su atributo: dos bits de los ocho
        mov ebp, ecx
        mov ecx, ebp
        and ecx, 2
        add ecx, [r_ash]
        shr edx, cl
        mov ecx, ebp
        and edx, 3
        shl eax, 4
        add eax, [r_base]
        mov ebp, eax
        shr ebp, 10
        mov ebp, [chr_pg+ebp*4]
        and eax, 0x3FF
        movzx ebx, byte [ebp+eax]
        movzx eax, byte [ebp+eax+8]
        mov edx, [tab_atr+edx*4]
        mov ebp, [tab_plano+ebx*8]
        mov esi, [tab_plano+eax*8]
        lea ebp, [ebp+esi*2]
        add ebp, edx
        mov [edi], ebp
        mov ebp, [tab_plano+ebx*8+4]
        mov esi, [tab_plano+eax*8+4]
        lea ebp, [ebp+esi*2]
        add ebp, edx
        mov [edi+4], ebp
        add edi, 8
        inc ecx
        cmp ecx, 32
        jb .s
        xor ecx, ecx
        xor byte [r_nt], 1
.s:
        dec dword [r_n]
        jnz .t
        test byte [ppu_mask], 0x02
        jnz .objetos
        xor eax, eax                ; los 8 puntos de la izquierda, tapados
        mov [lin_buf+8], eax
        mov [lin_buf+12], eax

.objetos:
        ; los objetos que tocan esta línea (ocho como mucho; se cuentan
        ; aunque estén apagados y solo se vea el fondo)
        mov ebp, 8
        test byte [ppu_ctrl], 0x20
        jz .o8
        mov ebp, 16
.o8:
        mov [r_alto], ebp
        xor esi, esi
        xor ecx, ecx                ; cuántos van
.busca:
        mov eax, [linea]
        movzx edx, byte [oam+esi*4]
        sub eax, edx
        dec eax
        js .bs
        cmp eax, ebp
        jae .bs
        cmp ecx, 8
        jb .cabe
        or byte [ppu_status], 0x20  ; más de ocho en una línea
        jmp .hechos
.cabe:
        mov [r_obj+ecx*4], esi
        inc ecx
.bs:
        inc esi
        cmp esi, 64
        jb .busca
.hechos:
        mov [r_nobj], ecx
        test ecx, ecx
        jz near .pasa
        test byte [ppu_mask], 0x10
        jz near .pasa
        ; del último al primero: el de número más bajo queda encima
.pon:
        dec ecx
        mov [r_n], ecx
        mov esi, [r_obj+ecx*4]
        mov eax, [linea]
        movzx edx, byte [oam+esi*4]
        sub eax, edx
        dec eax                     ; EAX = fila dentro del objeto
        movzx ebx, byte [oam+esi*4+2]       ; BL = atributos
        test bl, 0x80
        jz .nv
        neg eax
        add eax, [r_alto]
        dec eax
.nv:
        movzx edx, byte [oam+esi*4+1]       ; el dibujo
        cmp dword [r_alto], 16
        jne .ch
        mov ebp, edx
        and ebp, 1
        shl ebp, 12
        and edx, 0xFE
        cmp eax, 8
        jb .dir
        inc edx
        sub eax, 8
        jmp .dir
.ch:
        xor ebp, ebp
        test byte [ppu_ctrl], 0x08
        jz .dir
        mov ebp, 0x1000
.dir:
        shl edx, 4
        add edx, ebp
        add edx, eax
        mov ebp, edx
        shr ebp, 10
        mov ebp, [chr_pg+ebp*4]
        and edx, 0x3FF
        movzx eax, byte [ebp+edx]
        movzx edx, byte [ebp+edx+8]
        mov ebp, [tab_plano+eax*8]
        mov ecx, [tab_plano+edx*8]
        lea ebp, [ebp+ecx*2]
        mov [r_px], ebp
        mov ebp, [tab_plano+eax*8+4]
        mov ecx, [tab_plano+edx*8+4]
        lea ebp, [ebp+ecx*2]
        mov [r_px+4], ebp
        mov dl, bl                  ; DL = lo que se apunta con cada punto:
        and dl, 3                   ; paleta, «detrás del fondo» y «objeto 0»
        shl dl, 2
        mov al, bl
        and al, 0x20
        or dl, al
        test esi, esi
        jnz .n0
        or dl, 0x40
.n0:
        movzx edi, byte [oam+esi*4+3]
        add edi, spr_buf
        xor ecx, ecx
        test bl, 0x40
        jnz .espejo
.p1:
        mov al, [r_px+ecx]
        test al, al
        jz .p2
        or al, dl
        mov [edi+ecx], al
.p2:
        inc ecx
        cmp ecx, 8
        jb .p1
        jmp .puesto
.espejo:
        mov ebp, 7
.e1:
        mov al, [r_px+ebp]
        test al, al
        jz .e2
        or al, dl
        mov [edi+ecx], al
.e2:
        inc ecx
        dec ebp
        jns .e1
.puesto:
        mov ecx, [r_n]
        test ecx, ecx
        jnz near .pon
        ; se juntan con el fondo
        xor ecx, ecx
.junta:
        mov esi, [r_obj+ecx*4]
        movzx edi, byte [oam+esi*4+3]
        mov ebp, 8
.j1:
        cmp edi, 255
        ja .j9
        mov al, [spr_buf+edi]
        test al, al
        jz .j8
        mov byte [spr_buf+edi], 0
        cmp edi, 8
        jae .j2
        test byte [ppu_mask], 0x04
        jz .j8                      ; a la izquierda, tapados
.j2:
        mov dl, [lin_buf+8+edi]
        test dl, 3
        jz .j5                      ; fondo transparente: se ve el objeto
        test al, 0x40
        jz .j4
        cmp edi, 255
        je .j4
        test byte [ppu_status], 0x40
        jnz .j4
        cmp dword [s0_punto], 0
        jge .j4
        lea ebx, [edi+2]            ; el objeto 0 coincide con el fondo aquí
        mov [s0_punto], ebx
.j4:
        test al, 0x20
        jnz .j8                     ; detrás del fondo
.j5:
        and al, 0x0F
        or al, 0x10
        mov [lin_buf+8+edi], al
.j8:
        inc edi
        dec ebp
        jnz .j1
.j9:
        inc ecx
        cmp ecx, [r_nobj]
        jb .junta
        xor eax, eax
        mov [spr_buf+256], eax
        mov [spr_buf+260], eax

.pasa:
        ; de la línea (números de la paleta) a la imagen (colores)
        mov edi, [linea]
        shl edi, 8
        add edi, imagen
        mov esi, lin_buf + 8
        mov ecx, 64
        xor eax, eax
        xor ebx, ebx
.c:
        mov al, [esi]
        mov bl, [esi+1]
        mov dl, [pal_vis+eax]
        mov dh, [pal_vis+ebx]
        mov al, [esi+2]
        mov bl, [esi+3]
        mov [edi], dx
        mov dl, [pal_vis+eax]
        mov dh, [pal_vis+ebx]
        mov [edi+2], dx
        add esi, 4
        add edi, 4
        dec ecx
        jnz .c
        popad
        ret

; ---- los registros: leer (EDX=dirección, de 2000h a 5FFFh -> AL) ----
leer_es:
        cmp edx, 0x4000
        jae .apu
        mov eax, edx
        and eax, 7
        jmp [.tabla+eax*4]
        align 4
.tabla      dd .bus, .bus, .r2002, .bus, .r2004, .bus, .bus, .r2007
.bus:
        mov al, [bus]
        ret
.r2002:
        cmp dword [s0_punto], 0
        jl .sin0
        mov eax, [punto_fin]        ; ¿ha llegado ya el punto del objeto 0?
        sub eax, edi
        add eax, 9
        cmp eax, [s0_punto]
        jl .sin0
        or byte [ppu_status], 0x40
        mov dword [s0_punto], -1
.sin0:
        mov al, [bus]
        and al, 0x1F
        or al, [ppu_status]
        and byte [ppu_status], 0x7F
        mov byte [ppu_w], 0
        ret
.r2004:
        movzx eax, byte [oam_addr]
        mov al, [oam+eax]
        ret
.r2007:
        push ebx
        mov ebx, [ppu_v]
        and ebx, 0x3FFF
        cmp ebx, 0x3F00
        jae .pal
        call ppu_mem_leer
        xchg al, [ppu_buf]
        jmp .inc
.pal:
        sub ebx, 0x1000             ; (por debajo sigue la tabla de nombres)
        call ppu_mem_leer
        mov [ppu_buf], al
        mov eax, ebx
        and eax, 0x1F
        test al, 3
        jnz .p1
        and al, 0x0F
.p1:
        mov al, [pal+eax]
.inc:
        mov ebx, 1
        test byte [ppu_ctrl], 0x04
        jz .i1
        mov ebx, 32
.i1:
        push eax
        mov eax, [ppu_v]
        add [ppu_v], ebx
        and dword [ppu_v], 0x7FFF
        call escr_es.a12
        pop eax
        pop ebx
        ret
.apu:
        cmp edx, 0x4016
        je .mando1
        cmp edx, 0x4017
        je .mando2
        cmp edx, 0x4015
        je apu_estado
        mov al, 0x40
        ret
.mando1:
        xor eax, eax
        jmp .mando
.mando2:
        mov eax, 1
.mando:
        cmp byte [pad_strobe], 0
        je .m1
        mov al, [pad_estado+eax]
        and al, 1
        or al, 0x40
        ret
.m1:
        push ebx
        mov bl, [pad_reg+eax]
        mov bh, bl
        shr bh, 1
        or bh, 0x80
        mov [pad_reg+eax], bh
        mov al, bl
        and al, 1
        or al, 0x40
        pop ebx
        ret

; EBX=dirección de la memoria de imagen (menos de 3F00h) -> AL
ppu_mem_leer:
        push ebx
        mov ebp, ebx
        shr ebp, 10
        cmp ebx, 0x2000
        jae .nt
        mov ebp, [chr_pg+ebp*4]
        jmp .l
.nt:
        and ebp, 3
        mov ebp, [nt_pg+ebp*4]
.l:
        and ebx, 0x3FF
        mov al, [ebp+ebx]
        pop ebx
        ret

; ---- los registros: escribir (AL -> EDX, de 2000h a 5FFFh) ----
escr_es:
        cmp edx, 0x4000
        jae .apu
        mov [bus], al
        mov ebp, edx
        and ebp, 7
        jmp [.tabla+ebp*4]
        align 4
.tabla      dd .w2000, .w2001, .nada, .w2003, .w2004, .w2005, .w2006, .w2007
.nada:
        ret
.w2000:
        mov ah, [ppu_ctrl]
        mov [ppu_ctrl], al
        test al, 0x80               ; se encienden las NMI en pleno retorno
        jz .c1                      ; vertical: salta una ya
        test ah, 0x80
        jnz .c1
        test byte [ppu_status], 0x80
        jz .c1
        mov byte [nmi_pend], 1
        cmp byte [cpu_ap_fase], 0   ; (el procesador la atiende tras la
        jne .c1                     ; instrucción siguiente: ver cpu_fuera)
        mov byte [cpu_ap_fase], 1
        mov [cpu_aparte], edi
        xor edi, edi
.c1:
        movzx eax, al
        and eax, 3
        shl eax, 10
        and dword [ppu_t], ~0x0C00
        or [ppu_t], eax
        ret
.w2001:
        mov [ppu_mask], al
        ret
.w2003:
        mov [oam_addr], al
        ret
.w2004:
        movzx ebp, byte [oam_addr]
        mov [oam+ebp], al
        inc byte [oam_addr]
        ret
.w2005:
        movzx eax, al
        xor byte [ppu_w], 1
        jz .s2
        mov ebp, eax                ; primera: la columna y el punto
        and ebp, 7
        mov [ppu_x], ebp
        shr eax, 3
        and dword [ppu_t], ~0x001F
        or [ppu_t], eax
        ret
.s2:
        mov ebp, eax                ; segunda: la fila y la línea del dibujo
        and ebp, 7
        shl ebp, 12
        and eax, 0xF8
        shl eax, 2
        or eax, ebp
        and dword [ppu_t], ~0x73E0
        or [ppu_t], eax
        ret
.w2006:
        movzx eax, al
        xor byte [ppu_w], 1
        jz .a2
        and eax, 0x3F
        shl eax, 8
        and dword [ppu_t], 0x00FF
        or [ppu_t], eax
        ret
.a2:
        and dword [ppu_t], 0x7F00
        or eax, [ppu_t]
        mov [ppu_t], eax
        xchg eax, [ppu_v]
; (interna) EAX=v de antes: si el bit 12 de la dirección sube con la imagen
; apagada o en el retorno vertical, cuenta como una línea para el cartucho
; (así lo cuenta el del contador de líneas: mira esa patilla)
.a12:
        test eax, 0x1000
        jnz .a12x
        test dword [ppu_v], 0x1000
        jz .a12x
        test byte [ppu_mask], 0x18
        jz .a12s
        cmp dword [linea], 240
        jb .a12x
.a12s:
        call [map_linea]
.a12x:
        ret
.w2007:
        push ebx
        mov ebx, [ppu_v]
        and ebx, 0x3FFF
        cmp ebx, 0x3F00
        jae .wpal
        mov ebp, ebx
        shr ebp, 10
        cmp ebx, 0x2000
        jae .wnt
        cmp byte [chr_es_ram], 0
        je .winc                    ; los dibujos de ROM no se escriben
        mov ebp, [chr_pg+ebp*4]
        jmp .wl
.wnt:
        and ebp, 3
        mov ebp, [nt_pg+ebp*4]
.wl:
        and ebx, 0x3FF
        mov [ebp+ebx], al
        jmp .winc
.wpal:
        and ebx, 0x1F
        and al, 0x3F
        test bl, 3
        jnz .wp1
        and ebx, 0x0F               ; los de 10h, 14h, 18h y 1Ch son los
        mov [pal+ebx], al           ; mismos que los de 00h, 04h, 08h y 0Ch
        test ebx, ebx
        jnz .winc
        mov ah, al                  ; el color 0 de todas las paletas es el
        mov ebx, 28                 ; del fondo
.wp0:
        mov [pal_vis+ebx], al
        sub ebx, 4
        jns .wp0
        jmp .winc
.wp1:
        mov [pal+ebx], al
        mov [pal_vis+ebx], al
.winc:
        mov ebx, 1
        test byte [ppu_ctrl], 0x04
        jz .wi1
        mov ebx, 32
.wi1:
        mov eax, [ppu_v]
        add [ppu_v], ebx
        and dword [ppu_v], 0x7FFF
        call .a12
        pop ebx
        ret

.apu:
        cmp edx, 0x4014
        je .dma
        cmp edx, 0x4016
        je .mandos
        cmp edx, 0x4017
        jbe apu_escribir
        ret
.mandos:
        test al, 1
        jnz .m1
        cmp byte [pad_strobe], 0
        je .m0
        mov ax, [pad_estado]        ; al bajar la señal, los botones quedan
        mov [pad_reg], ax           ; apuntados para leerlos de uno en uno
.m0:
        mov byte [pad_strobe], 0
        ret
.m1:
        mov byte [pad_strobe], 1
        ret
.dma:
        ; copia 256 octetos de la página AL a la memoria de los objetos
        push ebx
        push ecx
        push esi
        movzx esi, al
        shl esi, 8
        cmp esi, 0x2000
        jae .d1
        and esi, 0x7FF
        add esi, ram
        jmp .d2
.d1:
        cmp esi, 0x6000
        jb .d9
        mov ebx, esi
        shr ebx, 13
        add esi, [pg+ebx*4]
.d2:
        movzx ebx, byte [oam_addr]
        mov ecx, 256
.d3:
        mov al, [esi]
        inc esi
        mov [oam+ebx], al
        inc bl
        dec ecx
        jnz .d3
.d9:
        sub edi, 513 * 3
        pop esi
        pop ecx
        pop ebx
        ret
