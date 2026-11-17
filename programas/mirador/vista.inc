; =============================================================================
;  MIRADOR.HVP - cómo se ve la imagen: la zona, la ampliación, el
;  desplazamiento, la vista reducida, pintarla, y girar y voltear
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  A su tamaño (100 %) el mapa se copia tal cual a la ventana, recortado a la
;  zona. Más pequeña, se hace una vez una «vista» reducida promediando (del
;  original en color verdadero si se guarda; si no, de los colores del mapa)
;  y pasada otra vez a la paleta; se copia igual. Ampliada, cada punto se
;  repite (el vecino más cercano) en un mapa del tamaño de la pantalla, solo
;  con lo que se ve. Si no hay memoria para la vista reducida, se reduce
;  también por el vecino más cercano.
; =============================================================================

; -----------------------------------------------------------------------------
;  La disposición: la zona de la imagen, a qué tamaño se ve y cuánto se puede
;  desplazar. Se llama cuando cambia la ventana, la imagen o la ampliación
; -----------------------------------------------------------------------------
dispone:
        pushad
        cmp byte [pantc], 0
        je .ventana
        mov dword [vy], 0           ; a pantalla completa: toda la ventana
        mov eax, [cli_an]
        mov ebx, [cli_al]
        jmp .zona
.ventana:
        mov eax, TB_AL
        cmp dword [av_txt], 0
        je .v1
        add eax, AVISO_AL
.v1:
        mov [vy], eax
        mov ebx, [cli_al]
        sub ebx, eax
        sub ebx, BR_PIE + BD_AN
        mov eax, [cli_an]
        sub eax, BD_AN
.zona:
        test eax, eax
        jns .z1
        xor eax, eax
.z1:
        cmp eax, [vb_an]
        jbe .z2
        mov eax, [vb_an]
.z2:
        mov [vw], eax
        test ebx, ebx
        jns .z3
        xor ebx, ebx
.z3:
        cmp ebx, [vb_al]
        jbe .z4
        mov ebx, [vb_al]
.z4:
        mov [vh], ebx
        xor eax, eax
        mov [pa_an], eax
        mov [pa_al], eax
        mov [sx_max], eax
        mov [sy_max], eax
        mov [ox], eax
        mov [oy], eax
        cmp byte [hay_img], 0
        je .limites
        ; ---- la ampliación: ajustada, la mayor con la que cabe entera (sin
        ;      pasar del 100 %: una imagen pequeña no se agranda sola) ----
        cmp byte [zajuste], 0
        je .fija
        mov ebx, 0x10000
        mov eax, [vw]
        mov ecx, [ma_an]
        call .cabe
        mov eax, [vh]
        mov ecx, [ma_al]
        call .cabe
        mov [zoom], ebx
.fija:
        mov eax, [zoom]
        cmp eax, ZOOM_MIN
        jae .f1
        mov eax, ZOOM_MIN
.f1:
        cmp eax, 0x80000
        jbe .f2
        mov eax, 0x80000
.f2:
        mov [zoom], eax
        mov eax, [ma_an]
        call a_pantalla
        mov [pa_an], eax
        mov eax, [ma_al]
        call a_pantalla
        mov [pa_al], eax
        ; lo que se puede desplazar, y dónde cae si es menor que la zona
        mov eax, [pa_an]
        sub eax, [vw]
        jns .m1
        neg eax
        shr eax, 1
        mov [ox], eax
        xor eax, eax
.m1:
        mov [sx_max], eax
        mov eax, [pa_al]
        sub eax, [vh]
        jns .m2
        neg eax
        shr eax, 1
        mov [oy], eax
        xor eax, eax
.m2:
        mov [sy_max], eax
.limites:
        mov eax, [sx]
        cmp eax, [sx_max]
        jle .l1
        mov eax, [sx_max]
.l1:
        test eax, eax
        jns .l2
        xor eax, eax
.l2:
        mov [sx], eax
        mov eax, [sy]
        cmp eax, [sy_max]
        jle .l3
        mov eax, [sy_max]
.l3:
        test eax, eax
        jns .l4
        xor eax, eax
.l4:
        mov [sy], eax
        mov eax, [pa_an]               ; lo que se ve de ella
        cmp eax, [vw]
        jbe .l5
        mov eax, [vw]
.l5:
        mov [vis_an], eax
        mov eax, [pa_al]
        cmp eax, [vh]
        jbe .l6
        mov eax, [vh]
.l6:
        mov [vis_al], eax
        ; las barras de desplazamiento, en su sitio
        cmp byte [pantc], 0
        jne .x
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, [cli_an]
        sub eax, BD_AN
        mov ebx, [vy]
        mov ecx, BD_AN
        mov edx, [vh]
        HV ven_mover
        mov ebp, [barra_h]
        xor eax, eax
        mov ebx, [vy]
        add ebx, [vh]
        mov ecx, [cli_an]
        sub ecx, BD_AN
        mov edx, BD_AN
        HV ven_mover
        call pon_barras
.x:
        cmp byte [mini], 0          ; con las miniaturas, la barra es de la rejilla
        je .x1
        call mini_mide
.x1:
        mov byte [sucio], 1
        popad
        ret
; (interna) EAX=puntos de la zona ECX=puntos de la imagen: si con la
; ampliación EBX no cabe, EBX=la mayor con la que cabe
.cabe:
        mov edx, eax
        xor eax, eax
        shrd eax, edx, 16           ; EDX:EAX = zona x 65536
        shr edx, 16
        div ecx
        cmp eax, ebx
        jae .c1
        mov ebx, eax
.c1:
        ret

; EAX=puntos de la imagen -> EAX=los que ocupan con la ampliación de ahora (1 al menos)
a_pantalla:
        push edx
        mul dword [zoom]
        shrd eax, edx, 16
        test eax, eax
        jnz .a
        inc eax
.a:
        pop edx
        ret

; -> EAX=la ampliación de ahora, en tantos por ciento de la imagen del
; archivo (si se redujo al leerla, el mapa a su tamaño es la mitad, un cuarto...)
zoom_pct:
        push edx
        mov eax, [zoom]
        imul eax, 100
        cmp dword [f_red], 1
        jbe .z
        xor edx, edx
        div dword [f_red]
.z:
        add eax, 0x8000
        shr eax, 16
        pop edx
        ret

; las barras de desplazamiento, al día
pon_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, BDM_PONER
        mov ecx, [sy_max]
        mov edx, [vh]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sy]
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_PONER
        mov ecx, [sx_max]
        mov edx, [vw]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sx]
        HV msg_enviar
.x:
        popad
        ret

; pone el desplazamiento vertical (u horizontal) en EAX, dentro de sus límites
desplaza_v:
        pushad
        cmp eax, [sy_max]
        jle .a
        mov eax, [sy_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        cmp eax, [sy]
        je .x
        mov [sy], eax
        call pon_barras
        call ensucia
.x:
        popad
        ret
desplaza_h:
        pushad
        cmp eax, [sx_max]
        jle .a
        mov eax, [sx_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        cmp eax, [sx]
        je .x
        mov [sx], eax
        call pon_barras
        call ensucia
.x:
        popad
        ret

; ---- la ampliación ----
; EAX=ampliación nueva (16.16): lo que estaba en el centro de la zona sigue en él
pon_zoom:
        pushad
        mov byte [zajuste], 0
        mov ebx, eax
        ; el centro de lo que se ve, como fracción (0.16) de la imagen
        mov eax, [vis_an]
        shr eax, 1
        add eax, [sx]
        call .frac_x
        mov esi, eax
        mov eax, [vis_al]
        shr eax, 1
        add eax, [sy]
        call .frac_y
        mov edi, eax
        mov [zoom], ebx
        call dispone
        mov eax, esi                ; y otra vez al centro
        mul dword [pa_an]
        shrd eax, edx, 16
        mov ecx, [vis_an]
        shr ecx, 1
        sub eax, ecx
        mov [sx], eax
        mov eax, edi
        mul dword [pa_al]
        shrd eax, edx, 16
        mov ecx, [vis_al]
        shr ecx, 1
        sub eax, ecx
        mov [sy], eax
        call dispone
        call pon_marcas
        call inval
        popad
        ret
.frac_x:
        mov ecx, [pa_an]
        jmp .frac
.frac_y:
        mov ecx, [pa_al]
.frac:
        test ecx, ecx
        jnz .f1
        inc ecx
.f1:
        cmp eax, ecx
        jb .f2
        lea eax, [ecx-1]
.f2:
        mov edx, eax
        xor eax, eax
        shrd eax, edx, 16
        shr edx, 16
        div ecx
        ret

; (interna) EAX=un tanto por ciento -> EAX=la ampliación (16.16)
pct_zoom:
        push edx
        push ecx
        shl eax, 16
        xor edx, edx
        mov ecx, 100
        div ecx
        pop ecx
        pop edx
        ret
ampliar:
        xor ecx, ecx
.b:
        mov eax, [zooms+ecx*4]
        call pct_zoom
        cmp eax, [zoom]
        ja pon_zoom
        inc ecx
        cmp ecx, ZOOMS_N
        jb .b
        ret                         ; (ya está en el mayor)
reducir:
        mov ecx, ZOOMS_N
.b:
        dec ecx
        js .x
        mov eax, [zooms+ecx*4]
        call pct_zoom
        cmp eax, [zoom]
        jb pon_zoom
        jmp .b
.x:
        ret
real:
        mov eax, 0x10000
        jmp pon_zoom
ajustar:
        mov byte [zajuste], 1
        mov dword [sx], 0
        mov dword [sy], 0
        call pon_marcas
        call dispone
        jmp inval

; -----------------------------------------------------------------------------
;  Las filas de la imagen que hay en memoria, en rojo, verde y azul: del
;  original si se guarda; si no, de los colores del mapa. Para reduce.inc
; -----------------------------------------------------------------------------
; EAX=fila EDI=destino
fuente_fila:
        mul dword [ma_an]
        mov ecx, [ma_an]
        cmp byte [hay_orig], 0
        je .mapa
        lea esi, [eax+eax*2]
        add esi, [orig_p]
        lea ecx, [ecx+ecx*2]
        rep movsb
        clc
        ret
.mapa:
        mov esi, [mapa_p]
        add esi, eax
        mov ebx, [hv_api + HV_pal_rgb * 4]
.m1:
        movzx eax, byte [esi]
        inc esi
        mov eax, [ebx+eax*4]
        stosw
        shr eax, 16
        stosb
        dec ecx
        jnz .m1
        clc
        ret
; lo mismo, de la imagen girada un cuarto de vuelta a la derecha (para
; imprimirla apaisada): su fila EAX es la columna EAX, de abajo arriba
fuente_fila_g:
        mov ebp, eax                ; EBP=la columna
        mov ecx, [ma_al]            ; ECX=puntos de la fila girada
        mov eax, [ma_al]
        dec eax
        mul dword [ma_an]
        add eax, ebp                ; EAX=el punto de abajo de esa columna
        cmp byte [hay_orig], 0
        je .mapa
        lea esi, [eax+eax*2]
        add esi, [orig_p]
        mov edx, [ma_an]
        lea edx, [edx+edx*2]
.o1:
        mov ax, [esi]
        stosw
        mov al, [esi+2]
        stosb
        sub esi, edx
        dec ecx
        jnz .o1
        clc
        ret
.mapa:
        mov esi, [mapa_p]
        add esi, eax
        mov ebx, [hv_api + HV_pal_rgb * 4]
.m1:
        movzx eax, byte [esi]
        sub esi, [ma_an]
        mov eax, [ebx+eax*4]
        stosw
        shr eax, 16
        stosb
        dec ecx
        jnz .m1
        clc
        ret

; -----------------------------------------------------------------------------
;  La vista reducida: la imagen entera a [pa_an] x [pa_al], promediando, y otra vez
;  a la paleta. Si no hay memoria, no la hay (hay_vista = 0)
; -----------------------------------------------------------------------------
haz_vista:
        pushad
        call suelta_vista
        mov eax, [pa_an]
        mul dword [pa_al]
        lea ecx, [eax+64]
        HV mem_bloque
        jc .x
        mov [vista_p], eax
        mov [vista_h], edx
        ; la memoria de trabajo: los errores, el reductor y una fila
        mov ecx, [pa_an]
        add ecx, 2
        imul ecx, 12
        mov eax, [ma_an]
        lea eax, [eax+eax*2]
        add ecx, eax
        mov eax, [pa_an]
        shl eax, 4
        add ecx, eax
        mov eax, [pa_an]
        lea eax, [eax+eax*2]
        lea ecx, [ecx+eax+128]
        HV mem_bloque
        jnc .hay
        mov edx, [vista_h]
        HV mem_bloque_soltar
        jmp .x
.hay:
        mov [sal_h], edx
        mov edi, eax
        mov ecx, [pa_an]
        call ap_empieza
        mov eax, [trama]
        mov [ap_modo], eax
        lea eax, [ecx+2]
        imul eax, 12
        add edi, eax
        mov eax, [ma_an]
        mov ebx, [ma_al]
        mov edx, [pa_al]
        mov esi, fuente_fila
        call red_prepara
        mov eax, [ma_an]
        lea eax, [eax+eax*2]
        add edi, eax
        mov eax, [pa_an]
        shl eax, 4
        lea edi, [edi+eax+36]
        and edi, ~3                 ; EDI=una fila
        xor ebx, ebx
        mov ebp, [vista_p]
.f:
        cmp ebx, [pa_al]
        jae .f9
        mov eax, ebx
        call red_fila
        mov esi, edi
        push edi
        mov edi, ebp
        mov ecx, [pa_an]
        mov eax, ebx
        call ap_fila
        pop edi
        add ebp, [pa_an]
        inc ebx
        jmp .f
.f9:
        mov edx, [sal_h]
        HV mem_bloque_soltar
        mov eax, [pa_an]
        mov [vi_an], eax
        mov eax, [pa_al]
        mov [vi_al], eax
        mov eax, [zoom]
        mov [vista_zoom], eax
        mov byte [hay_vista], 1
.x:
        popad
        ret

; -----------------------------------------------------------------------------
;  Pintar la imagen en su zona (con el contexto abierto; es lo último que se
;  pinta: deja el contexto recortado a la imagen)
; -----------------------------------------------------------------------------
pinta_imagen:
        pushad
        ; lo de alrededor
        call color_fondo
        xor eax, eax                ; arriba
        mov ebx, [vy]
        mov ecx, [vw]
        mov edx, [oy]
        call .rect
        mov ebx, [vy]               ; abajo
        add ebx, [oy]
        add ebx, [vis_al]
        mov edx, [vh]
        sub edx, [oy]
        sub edx, [vis_al]
        call .rect
        mov ebx, [vy]               ; a la izquierda
        add ebx, [oy]
        mov ecx, [ox]
        mov edx, [vis_al]
        call .rect
        mov eax, [ox]               ; a la derecha
        add eax, [vis_an]
        mov ecx, [vw]
        sub ecx, eax
        call .rect
        ; ---- la imagen ----
        mov eax, [zoom]
        cmp eax, 0x10000
        ja .amplia
        mov esi, [mapa_p]
        mov ecx, [ma_an]
        mov edx, [ma_al]
        je .copia
        cmp dword [an_n], 2         ; (una animación reducida: sin suavizar, que hay que
        jae .amplia                 ; hacerla en cada cuadro)
        cmp byte [hay_vista], 0     ; reducida: la vista, hecha a esta ampliación
        je .hazla
        cmp eax, [vista_zoom]
        jne .hazla
        mov ecx, [pa_an]
        cmp ecx, [vi_an]
        jne .hazla
        mov ecx, [pa_al]
        cmp ecx, [vi_al]
        je .hecha
.hazla:
        call haz_vista
        cmp byte [hay_vista], 0
        je .amplia                  ; (sin memoria: por el vecino más cercano)
.hecha:
        mov esi, [vista_p]
        mov ecx, [vi_an]
        mov edx, [vi_al]
.copia:
        push ecx
        push edx
        mov eax, [ox]
        mov ebx, [vy]
        add ebx, [oy]
        mov ecx, [vis_an]
        mov edx, [vis_al]
        HV dc_cortar
        pop edx
        pop ecx
        mov eax, [ox]
        sub eax, [sx]
        mov ebx, [vy]
        add ebx, [oy]
        sub ebx, [sy]
        HV dc_copiar
        jmp .encima
.amplia:
        call pide_vbuf
        jc .sin_vbuf
        cmp byte [sucio], 0
        je .a1
        call escala
        mov byte [sucio], 0
.a1:
        mov eax, [ox]
        mov ebx, [vy]
        add ebx, [oy]
        mov ecx, [vis_an]
        mov edx, [vis_al]
        mov esi, [vbuf_p]
        HV dc_copiar
        jmp .encima
.sin_vbuf:
        COLOR C_PIEDRA              ; ni para eso hay memoria: su sitio, liso, y se dice
        mov eax, [ox]
        mov ebx, [vy]
        add ebx, [oy]
        mov ecx, [vis_an]
        mov edx, [vis_al]
        HV dc_rect
        COLOR C_GRANATE
        add eax, 8
        add ebx, 8
        mov esi, t_e_ampliar
        HV dc_texto
.encima:
        popad
        ret
; (interna) un rectángulo, si mide algo
.rect:
        test ecx, ecx
        jle .r9
        test edx, edx
        jle .r9
        HV dc_rect
.r9:
        ret

; el mapa en el que se amplía (del tamaño de la pantalla): se pide la primera
; vez que hace falta. CF=1 si no hay memoria
pide_vbuf:
        cmp byte [hay_vbuf], 0
        jne .x                      ; (CF=0)
        pushad
        mov ecx, [vb_an]
        imul ecx, [vb_al]
        add ecx, 64
        HV mem_bloque
        jc .no
        mov [vbuf_p], eax
        mov [vbuf_h], edx
        mov byte [hay_vbuf], 1
        mov byte [sucio], 1
        popad
        clc
.x:
        ret
.no:
        popad
        stc
        ret
suelta_vbuf:
        cmp byte [hay_vbuf], 0
        je .x
        pushad
        mov edx, [vbuf_h]
        HV mem_bloque_soltar
        popad
        mov byte [hay_vbuf], 0
.x:
        ret

; lo que se ve de la imagen, a la ampliación de ahora, en vbuf ([vis_an] x
; [vis_al]): de cada punto de la pantalla, el del mapa que le toca
escala:
        pushad
        mov eax, 0xFFFFFFFF         ; el paso: 65536 / ampliación (16.16)
        xor edx, edx
        div dword [zoom]
        mov ebp, eax                ; EBP=puntos del mapa por punto de pantalla (16.16)
        mov edi, [vbuf_p]
        xor ebx, ebx                ; EBX=la fila de la pantalla
.fila:
        cmp ebx, [vis_al]
        jae .x
        mov eax, [sy]
        add eax, ebx
        mul ebp
        shrd eax, edx, 16
        mov ecx, [ma_al]
        dec ecx
        cmp eax, ecx
        jbe .f1
        mov eax, ecx
.f1:
        mul dword [ma_an]
        mov esi, [mapa_p]
        add esi, eax                ; ESI=su fila del mapa
        mov eax, [sx]
        mul ebp
        mov edx, eax                ; EDX=la columna (16.16)
        mov ecx, [vis_an]
        push ebx
        mov ebx, [ma_an]
        dec ebx
.px:
        mov eax, edx
        shr eax, 16
        cmp eax, ebx
        jbe .p1
        mov eax, ebx
.p1:
        mov al, [esi+eax]
        stosb
        add edx, ebp
        dec ecx
        jnz .px
        pop ebx
        inc ebx
        jmp .fila
.x:
        popad
        ret

; -----------------------------------------------------------------------------
;  Girar y voltear: ECX=O_GIRA_IZQ, O_GIRA_DER, O_VOLT_H u O_VOLT_V. Se hace
;  con el mapa y, si se guarda, con el original
; -----------------------------------------------------------------------------
t_e_girar   db "No hay memoria bastante para girar esta imagen.", 0
t_e_ampliar db "No hay memoria para ampliar.", 0

transforma:
        pushad
        call an_para                ; (una animación se queda en el cuadro que se ve)
        mov [tr_orden], ecx
        cmp ecx, O_VOLT_H
        jae .voltea
        ; ---- girar: a un bloque nuevo ----
        mov eax, [ma_an]
        mul dword [ma_al]
        lea ecx, [eax+64]
        HV mem_bloque
        jnc .g1
        mov esi, t_e_girar
        call aviso
        jmp .x
.g1:
        push edx
        mov edi, eax
        mov esi, [mapa_p]
        mov ebx, 1
        call gira
        mov [mapa_p], edi
        mov edx, [mapa_h]
        HV mem_bloque_soltar
        pop dword [mapa_h]
        cmp byte [hay_orig], 0
        je .g3
        mov eax, [ma_an]
        mul dword [ma_al]
        lea ecx, [eax+eax*2+64]
        HV mem_bloque
        jnc .g2
        mov edx, [orig_h]           ; sin sitio: se deja de guardar el original
        HV mem_bloque_soltar
        mov byte [hay_orig], 0
        jmp .g3
.g2:
        push edx
        mov edi, eax
        mov esi, [orig_p]
        mov ebx, 3
        call gira
        mov [orig_p], edi
        mov edx, [orig_h]
        HV mem_bloque_soltar
        pop dword [orig_h]
.g3:
        mov eax, [ma_an]
        xchg eax, [ma_al]
        mov [ma_an], eax
        mov eax, [g_an]
        xchg eax, [g_al]
        mov [g_an], eax
        jmp .hecho
.voltea:
        mov esi, [mapa_p]
        mov ebx, 1
        call voltea
        cmp byte [hay_orig], 0
        je .hecho
        mov esi, [orig_p]
        mov ebx, 3
        call voltea
.hecho:
        call tr_anota
        call suelta_vista
        mov dword [sx], 0
        mov dword [sy], 0
        call dispone
        call inval
.x:
        popad
        ret

; (interna) apunta el giro o el volteo que se acaba de hacer. Como esté la
; imagen se guarda en dos datos: [or_rot] cuartos de vuelta a la derecha y,
; después, [or_volt] volteada en horizontal. (Girar algo ya volteado es
; girarlo al revés antes de voltearlo; voltear en vertical es voltear en
; horizontal y dar media vuelta.)
tr_anota:
        cmp dword [tr_calla], 0
        jne .x
        push eax
        push ebx
        mov eax, [tr_orden]
        mov ebx, 1
        cmp eax, O_GIRA_DER
        je .gira
        mov ebx, -1
        cmp eax, O_GIRA_IZQ
        je .gira
        xor dword [or_volt], 1
        cmp eax, O_VOLT_V
        jne .f
        mov ebx, 2
        jmp .suma
.gira:
        cmp dword [or_volt], 0
        je .suma
        neg ebx
.suma:
        add ebx, [or_rot]
        and ebx, 3
        mov [or_rot], ebx
.f:
        pop ebx
        pop eax
.x:
        ret
; deja la imagen recién leída girada y volteada como dicen or_rot y or_volt
tr_repite:
        pushad
        mov dword [tr_calla], 1
        mov ebx, [or_rot]
.g:
        test ebx, ebx
        jz .v
        mov ecx, O_GIRA_DER
        call transforma
        dec ebx
        jmp .g
.v:
        cmp dword [or_volt], 0
        je .x
        mov ecx, O_VOLT_H
        call transforma
.x:
        mov dword [tr_calla], 0
        popad
        ret

; (interna) ESI=puntos ([ma_an] x [ma_al], EBX octetos cada uno) EDI=destino:
; girados un cuarto de vuelta, a la derecha o a la izquierda según [tr_orden].
; Conserva EDI
gira:
        pushad
        mov eax, [ma_an]
        imul eax, ebx
        mov [tr_paso], eax          ; octetos de una fila de origen
        xor ebp, ebp                ; EBP=la fila de destino (hay [ma_an])
.fila:
        cmp ebp, [ma_an]
        jae .x
        cmp dword [tr_orden], O_GIRA_DER
        jne .izq
        ; a la derecha: la fila «y» nueva es la columna «y», de abajo arriba
        mov eax, [ma_al]
        dec eax
        mul dword [ma_an]
        add eax, ebp
        imul eax, ebx
        add eax, [esp+4]            ; (el ESI de la entrada)
        mov esi, eax
        mov edx, [tr_paso]
        neg edx
        jmp .col
.izq:
        ; a la izquierda: es la columna «ancho-1-y», de arriba abajo
        mov eax, [ma_an]
        dec eax
        sub eax, ebp
        imul eax, ebx
        add eax, [esp+4]
        mov esi, eax
        mov edx, [tr_paso]
.col:
        mov ecx, [ma_al]
.c1:
        mov al, [esi]
        stosb
        cmp ebx, 3
        jne .c2
        mov ax, [esi+1]
        stosw
.c2:
        add esi, edx
        dec ecx
        jnz .c1
        inc ebp
        jmp .fila
.x:
        popad
        ret

; (interna) ESI=puntos ([ma_an] x [ma_al], EBX octetos cada uno): volteados en
; el sitio, en horizontal o en vertical según [tr_orden]
voltea:
        pushad
        mov eax, [ma_an]
        imul eax, ebx
        mov [tr_paso], eax
        cmp dword [tr_orden], O_VOLT_V
        je .vert
        mov ebp, [ma_al]            ; en horizontal: cada fila, del revés
.fila:
        mov edi, esi
        add edi, [tr_paso]
        sub edi, ebx                ; EDI=el último punto de la fila
        push esi
.h1:
        cmp esi, edi
        jae .h3
        xor ecx, ecx
.h2:
        mov al, [esi+ecx]
        xchg al, [edi+ecx]
        mov [esi+ecx], al
        inc ecx
        cmp ecx, ebx
        jb .h2
        add esi, ebx
        sub edi, ebx
        jmp .h1
.h3:
        pop esi
        add esi, [tr_paso]
        dec ebp
        jnz .fila
        jmp .x
.vert:
        mov eax, [ma_al]            ; en vertical: la primera fila con la última...
        dec eax
        mul dword [tr_paso]
        lea edi, [esi+eax]
.v1:
        cmp esi, edi
        jae .x
        mov ecx, [tr_paso]
.v2:
        mov al, [esi]
        xchg al, [edi]
        mov [esi], al
        inc esi
        inc edi
        dec ecx
        jnz .v2
        sub edi, [tr_paso]
        sub edi, [tr_paso]
        jmp .v1
.x:
        popad
        ret
