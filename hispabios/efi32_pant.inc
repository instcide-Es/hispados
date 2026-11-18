; =============================================================================
;  HispaBIOS (IA32) - la pantalla (parte de efi32.asm)
; =============================================================================
;  La pantalla del firmware (GOP) es un marco de 32 bits por punto. Como en
;  efi64.asm: el texto de DOS (80x25) se dibuja ampliado y solo las celdas que
;  cambian; los modos gráficos (VBE) se dan a los programas en un marco de
;  memoria normal (vbe_lfb) y en cada tic se llevan a la pantalla las líneas
;  que han cambiado (graf_presentar); se mide lo que cuesta pintar la pantalla
;  entera y, si es lenta, se pasa a un modo menor o se amplía menos.
;
;  ESCRITURA COMBINADA (WC). El firmware suele dejar el marco sin caché (UC) y
;  cada punto va al bus por separado. En 64 bits se pinta por un alias del
;  marco en las tablas de páginas del firmware; un firmware de 32 bits casi
;  nunca usa paginación, así que aquí hay unas TABLAS PROPIAS (PAE, páginas de
;  2 MB, toda la memoria de 0 a 4 GB tal cual, y las páginas del marco con el
;  tipo WC por la entrada 5 del PAT) que solo están puestas mientras se dibuja
;  (wc_entrar, wc_salir), con las interrupciones cerradas. No se tocan los
;  MTRR ni lo que tenga el firmware (se le devuelven CR0, CR3 y CR4).
;  Referencias: Intel SDM vol. 3, 4.4 (paginación PAE), 11.11 y 11.12.
;
;  LA PANTALLA GIRADA. «giro» dice cuántos cuartos de vuelta a la derecha hay
;  que dar a la imagen para que se vea derecha (0: ninguno; 1: 90 grados a la
;  derecha, lo normal en una tableta con el panel vertical; 2: del revés; 3:
;  90 a la izquierda). Con giro, todo se dibuja en la PANTALLA LÓGICA (lb:
;  memoria normal, de p_an x p_al puntos, ya apaisada) y se apunta el
;  rectángulo que ha cambiado; girar_sucio lo lleva a la pantalla de verdad,
;  girado, por teselas de 256x16 puntos que se componen en memoria normal y se
;  escriben por filas (así la pantalla recibe tiras seguidas, que es como rinde
;  la escritura combinada). Sin giro, la pantalla lógica es la de verdad.

; ---- gop_buscar: localiza la salida gráfica -> fb, ancho, alto, línea; CF si no ----
gop_buscar:
        push ebx
        push ecx
        push edx
%ifdef PRUEBA_SIN_GOP
        jmp .no
%endif
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEPROT], &guid_gop, 0, &gop
        test eax, eax
        jnz .no
        mov ebx, [V(gop)]
        mov ebx, [ebx + GOP_MODE]
        mov eax, [ebx + GM_FB]
        mov [V(fb)], eax
        mov eax, [ebx + GM_FB + 4]
        mov [V(fb_alta)], eax
        mov ebx, [ebx + GM_INFO]
        mov eax, [ebx + 4]
        mov [V(fb_ancho)], eax
        mov eax, [ebx + 8]
        mov [V(fb_alto)], eax
        mov eax, [ebx + 12]
        mov [V(fb_formato)], eax        ; 0 = RGBX, 1 = BGRX
        mov eax, [ebx + 32]
        mov [V(fb_linea)], eax
        cmp dword [V(fb_alta)], 0       ; (en 32 bits, por debajo de 4 GB)
        jne .no
        cmp dword [V(fb)], 0
        je .no
        cmp dword [V(fb_formato)], 1
        ja .no
        mov eax, [V(fb_ancho)]          ; 640x400 o más, de frente o de lado
        mov ecx, [V(fb_alto)]
        cmp eax, 640
        jb .de_lado
        cmp ecx, 400
        jae .si
.de_lado:
        cmp ecx, 640
        jb .no
        cmp eax, 400
        jb .no
.si:    pop edx
        pop ecx
        pop ebx
        clc
        ret
.no:    pop edx
        pop ecx
        pop ebx
        stc
        ret

; gop_decir: «Pantalla (GOP): ancho x alto, formato, puntos por línea, en...»
gop_decir:
        DI t_gop
        push eax
        mov eax, [V(fb_ancho)]
        call decimal
        DI t_por
        mov eax, [V(fb_alto)]
        call decimal
        DI t_gop_f
        mov eax, [V(fb_formato)]
        call decimal
        pop eax
        jmp gop_decir2.resto
gop_decir2:
        push eax
        mov eax, [V(fb_ancho)]
        call decimal
        DI t_por
        mov eax, [V(fb_alto)]
        call decimal
        pop eax
.resto: push eax
        push edx
        DI t_gop_l
        mov eax, [V(fb_linea)]
        call decimal
        DI t_gop_d
        mov eax, [V(fb)]
        mov edx, [V(fb_alta)]
        call hex64
        DI t_nl
        pop edx
        pop eax
        ret

; ---- giro_decidir: cuánto hay que girar la imagen ----
; Por omisión, un cuarto de vuelta a la derecha si la pantalla es más alta que
; ancha (el panel vertical de una tableta puesto de lado) y nada si es
; apaisada. Manda lo guardado en \HBGIRO.CFG de la memoria USB (un número del
; 0 al 3), y la tecla G al arrancar da un cuarto de vuelta más y lo guarda.
giro_decidir:
        pushad
        xor eax, eax
        mov ecx, [V(fb_ancho)]
        cmp ecx, [V(fb_alto)]
        jae .apaisada
        mov al, 1
        mov byte [V(giro_vert)], 1
.apaisada:
%ifdef PRUEBA_GIRO
        mov al, PRUEBA_GIRO
%endif
        mov [V(giro)], al
        lea esi, [V(nombre_giro)]       ; lo guardado
        lea edi, [V(buf_giro)]
        mov ecx, 1
        call fich_leer
        jc .sin_fich
        cmp eax, 1
        jb .sin_fich
        mov al, [V(buf_giro)]
        sub al, '0'
        cmp al, 3
        ja .sin_fich
        mov [V(giro)], al
        mov byte [V(giro_fich)], 1
.sin_fich:
        call giro_validar
        cmp byte [V(giro_tecla)], 0     ; la tecla G: un cuarto de vuelta más
        je .puesto
        inc byte [V(giro)]
        and byte [V(giro)], 3
        call giro_validar
        call giro_guardar
.puesto:
        call giro_poner
        ; lo que queda, al registro; a la pantalla, solo si hay giro o es vertical
        mov al, [V(giro)]
        or al, [V(giro_vert)]
        setz byte [V(callado)]
        cmp byte [V(giro_vert)], 0
        je .dice
        DI t_gi_vert
.dice:  DI t_gi
        movzx eax, byte [V(giro)]
        lea esi, [V(t_gi_0)]
        imul eax, eax, t_gi_1 - t_gi_0
        add esi, eax
        call escribir
        DI t_gi_log
        mov eax, [V(p_an)]
        call decimal
        DI t_por
        mov eax, [V(p_al)]
        call decimal
        cmp byte [V(giro_fich)], 0
        je .sin_g
        DI t_gi_fich
.sin_g: cmp byte [V(giro_tecla)], 0
        je .sin_t
        DI t_gi_tecla
.sin_t: DI t_gi_fin
        mov byte [V(callado)], 0
        popad
        ret

; giro_validar: si con ese giro no caben los 640x400 del texto, el de al lado
giro_validar:
        push eax
        push ecx
        mov eax, [V(fb_ancho)]
        mov ecx, [V(fb_alto)]
        test byte [V(giro)], 1
        jz .d
        xchg eax, ecx
.d:     cmp eax, 640
        jb .otro
        cmp ecx, 400
        jae .f
.otro:  xor byte [V(giro)], 1
.f:     pop ecx
        pop eax
        ret

; giro_poner: las medidas de la pantalla lógica (p_an x p_al) según el giro
giro_poner:
        push eax
        push ecx
        mov eax, [V(fb_ancho)]
        mov ecx, [V(fb_alto)]
        test byte [V(giro)], 1
        jz .d
        xchg eax, ecx
.d:     mov [V(p_an)], eax
        mov [V(p_al)], ecx
        pop ecx
        pop eax
        ret

; giro_guardar: el giro, a \HBGIRO.CFG (para el próximo arranque)
giro_guardar:
        pushad
        mov al, [V(giro)]
        add al, '0'
        mov [V(buf_giro)], al
        lea esi, [V(nombre_giro)]
        lea edi, [V(buf_giro)]
        mov ecx, 1
        call fich_guardar
        popad
        ret

; ---- giro_cambiar: Ctrl+Alt+G, con DOS en marcha: un cuarto de vuelta más ----
; (solo en modo texto; los modos de VBE que caben se deciden al arrancar)
giro_cambiar:
        pushad
        cmp byte [V(graf)], 0
        jne .f
        inc byte [V(giro)]
        and byte [V(giro)], 3
        call giro_validar
        call giro_poner
        call logica_poner
        call escala
        call vbe_medidas
        mov byte [V(borrada)], 0        ; (el texto se pinta entero otra vez,
        call pantalla_dibujar           ; ahora mismo)
        sti
        call giro_guardar
        cli
.f:     popad
        ret

; ---- logica_poner: dónde se dibuja (p_dir, p_lin). Sin giro, en la pantalla;
;      con giro, en la pantalla lógica (lb), que se pide aquí si hace falta ----
logica_poner:
        pushad
.otra:  cmp byte [V(giro)], 0
        jne .girada
        mov eax, [V(fb)]
        mov [V(p_dir)], eax
        mov eax, [V(fb_linea)]
        mov [V(p_lin)], eax
        jmp .f
.girada:
        mov eax, [V(fb_ancho)]
        mul dword [V(fb_alto)]
        add eax, 1023
        shr eax, 10                     ; puntos de 4 octetos -> páginas
        cmp eax, [V(lb_pags)]
        jbe .hay
        mov [V(lb_quiere)], eax
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_ANY, MEM_LOADERDATA, [V(lb_quiere)], &dir_pag
        test eax, eax
        jnz .sin
        mov eax, [V(dir_pag)]
        mov [V(lb)], eax
        mov eax, [V(lb_quiere)]
        mov [V(lb_pags)], eax
.hay:   mov eax, [V(lb)]
        mov [V(p_dir)], eax
        mov eax, [V(p_an)]
        mov [V(p_lin)], eax
        jmp .f
.sin:   DI t_gi_sin_mem                 ; sin memoria para ella: sin giro
        mov byte [V(giro)], 0
        call giro_poner
        jmp .otra
.f:     call sucio_nada
        popad
        ret

; ---- vbe_preparar: la memoria del marco lineal de VBE ----
vbe_preparar:
        pushad
        mov byte [V(callado)], 1
        mov dword [V(dir_pag)], 0FFFFFFFFh
        mov dword [V(dir_pag) + 4], 0
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_MAX, MEM_LOADERDATA, VBE_PAGS, &dir_pag
        test eax, eax
        jnz .no
        mov eax, [V(dir_pag)]
        mov [V(vbe_lfb)], eax
        mov ebx, [V(bios_base)]
        mov [ebx + 184], eax            ; T_VBE_LFB
        add eax, VBE_MEM
        mov [V(vbe_ant)], eax
        call vbe_medidas
        DI t_v_lfb
        mov eax, [V(vbe_lfb)]
        xor edx, edx
        call hex64
        DI t_v_lfb2
        jmp .f
.no:    DI t_v_no
.f:     mov byte [V(callado)], 0
        popad
        ret

; vbe_medidas: lo que mide la pantalla (la lógica), a la BIOS: los modos de
; VBE mayores no se ofrecen (T_VBE_AN, T_VBE_AL)
vbe_medidas:
        push eax
        push ebx
        push ecx
        mov ebx, [V(bios_base)]
        mov ecx, 0FFFFh
        mov eax, [V(p_an)]
        cmp eax, ecx
        jbe .a
        mov eax, ecx
.a:     mov [ebx + 188], ax
        mov eax, [V(p_al)]
        cmp eax, ecx
        jbe .b
        mov eax, ecx
.b:     mov [ebx + 190], ax
        pop ecx
        pop ebx
        pop eax
        ret

; ---- gop_modos: los modos de pantalla que da el firmware, al registro ----
gop_modos:
        pushad
        DI t_g_modos
        mov ebx, [V(gop)]
        mov eax, [ebx + GOP_MODE]
        mov eax, [eax]                  ; MaxMode
        mov [V(gm_max)], eax
        xor esi, esi
.m:     cmp esi, [V(gm_max)]
        jae .f
        cmp esi, 40
        jae .mas
        FW [ebx + GOP_QUERY], ebx, esi, &gm_tam, &gm_info
        test eax, eax
        jnz .sig
        test esi, esi
        jz .p
        DI t_g_coma
.p:     mov edx, [V(gm_info)]
        mov eax, [edx + 4]
        call decimal
        DI t_por
        mov eax, [edx + 8]
        call decimal
        mov eax, [ebx + GOP_MODE]
        cmp esi, [eax + 4]
        jne .sig
        DI t_g_ast
.sig:   inc esi
        jmp .m
.mas:   DI t_g_mas
.f:     DI t_nl
        popad
        ret

; ---- gop_menor: con la pantalla lenta, pone el modo del firmware de 800x600 o
;      más (32 bits) que se pinte entero en T_LENTO (el mayor que lo cumpla o,
;      si ninguno, el más pequeño). CF si se queda el que había. ----
gop_menor:
        pushad
        mov ebx, [V(gop)]
        mov eax, [ebx + GOP_MODE]
        mov eax, [eax]                  ; MaxMode
        mov [V(gm_max)], eax
        mov dword [V(gn_el)], -1        ; el elegido y sus puntos
        mov dword [V(gn_pts)], 0
        mov byte [V(gn_cumple)], 0      ; 1: el elegido se pinta en T_LENTO
        mov eax, [V(fb_ancho)]
        mul dword [V(fb_alto)]
        mov [V(gn_tot)], eax            ; los puntos del modo puesto
        xor esi, esi
.m:     cmp esi, [V(gm_max)]
        jae .elegido
        FW [ebx + GOP_QUERY], ebx, esi, &gm_tam, &gm_info
        test eax, eax
        jnz .sig
        mov edx, [V(gm_info)]
        cmp dword [edx + 12], 1         ; color de 32 bits
        ja .sig
        mov eax, [edx + 4]
        mov ecx, [edx + 8]
        cmp eax, ecx                    ; (el lado largo y el corto)
        jae .ord
        xchg eax, ecx
.ord:   cmp eax, 800
        jb .sig
        cmp eax, LIN_MAX
        ja .sig
        cmp ecx, 600
        jb .sig
        mul ecx                         ; EAX = puntos del modo
        cmp eax, [V(gn_tot)]
        jae .sig
        mov edi, eax
        mul dword [V(t_pant)]
        cmp edx, [V(gn_tot)]
        jae .nc
        div dword [V(gn_tot)]           ; EAX = lo que costaría pintarlo
        cmp eax, T_LENTO
        setbe dl                        ; DL = 1: cumple
        jmp .dec
.nc:    xor edx, edx
.dec:   cmp dword [V(gn_el)], -1
        je .toma
        test dl, dl
        jz .no_cumple
        cmp byte [V(gn_cumple)], 0      ; cumple: mejor que uno que no, o el mayor
        je .toma
        cmp edi, [V(gn_pts)]
        ja .toma
        jmp .sig
.no_cumple:
        cmp byte [V(gn_cumple)], 0      ; no cumple: solo si es más pequeño que
        jne .sig                        ; otro que tampoco
        cmp edi, [V(gn_pts)]
        jae .sig
.toma:  mov [V(gn_el)], esi
        mov [V(gn_pts)], edi
        mov [V(gn_cumple)], dl
.sig:   inc esi
        jmp .m
.elegido:
        cmp dword [V(gn_el)], -1
        je .no
        FW [ebx + GOP_SETMODE], ebx, [V(gn_el)]
        test eax, eax
        jnz .no
        popad
        clc
        ret
.no:    popad
        stc
        ret

; ---- wc_preparar: las tablas propias con la pantalla en escritura combinada,
;      la medida de la velocidad y la ampliación del texto; todo al registro ----
wc_preparar:
        pushad
        mov dword [V(wc_cr3)], 0
        mov byte [V(wc_usar)], 0
        mov eax, 1
        cpuid
        mov [V(wp_cpu)], edx
        ; el tipo de caché que tiene la pantalla según los MTRR
        DI t_w_mtrr
        bt dword [V(wp_cpu)], 12        ; ¿MTRR?
        jnc .ninguno
        mov ecx, 2FFh                   ; IA32_MTRR_DEF_TYPE
        rdmsr
        and eax, 0FFFh
        mov ecx, 3
        call hex
        DI t_w_mtrr2
        mov ecx, 0FEh                   ; IA32_MTRRCAP: cuántos variables
        rdmsr
        movzx eax, al
        mov [V(wp_n)], eax
        xor esi, esi
.v:     cmp esi, [V(wp_n)]
        jae .ninguno
        lea ecx, [esi * 2 + 201h]       ; máscara
        rdmsr
        bt eax, 11                      ; ¿en uso?
        jnc .vs
        and eax, -4096
        mov ebx, eax                    ; EBX = la máscara (parte baja)
        mov edi, edx
        lea ecx, [esi * 2 + 200h]       ; base y tipo
        rdmsr
        mov [V(wp_tipo)], eax
        and edx, edi                    ; (un tramo por encima de 4 GB)
        jnz .vs
        and eax, -4096
        and eax, ebx
        mov edx, [V(fb)]
        and edx, ebx
        cmp eax, edx
        jne .vs
        DI t_w_var
        mov eax, esi
        call decimal
        DI t_w_tipo
        mov eax, [V(wp_tipo)]
        and eax, 7
        lea esi, [V(t_w_tipos) + eax * 4]
        call escribir
        jmp .pat
.vs:    inc esi
        jmp .v
.ninguno:
        DI t_w_nomtrr
.pat:   bt dword [V(wp_cpu)], 16        ; ¿PAT?
        jc .con_pat
        DI t_nl
        DI t_w_no_pat
        jmp .medir
.con_pat:
        DI t_w_pat
        mov ecx, 277h                   ; IA32_PAT
        rdmsr
        call hex64
        DI t_nl
        ; cómo tiene el firmware la paginación
        DI t_w_fw
        mov eax, cr0
        mov ecx, 8
        call hex
        DI t_w_fw3
        mov eax, cr3
        call hex
        DI t_w_fw4
        mov eax, cr4
        call hex
        mov eax, cr0
        test eax, eax
        js .fw_pag
        DI t_w_fw_sin
        jmp .fw_ya
.fw_pag:
        DI t_w_fw_con
.fw_ya:
%ifdef PRUEBA_SIN_PAG
        DI t_w_sinpag
%endif
%ifdef PRUEBA_SIN_WC
        jmp .medir
%endif
        cmp byte [V(seguro)], 0
        jne .medir
        bt dword [V(wp_cpu)], 6         ; ¿PAE?
        jc .pae
        DI t_w_no_pae
        jmp .medir
.pae:   ; cinco páginas: la tabla de 4 punteros y 4 directorios (4 GB en páginas de 2 MB)
        cmp dword [V(wc_tablas)], 0
        jne .tengo
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_ANY, MEM_LOADERDATA, 5, &dir_pag
        test eax, eax
        jnz .no_mem
        mov eax, [V(dir_pag)]
        mov [V(wc_tablas)], eax
.tengo: mov ebx, [V(wc_tablas)]
        cld
        mov edi, ebx
        mov ecx, 1024
        xor eax, eax
        rep stosd
        lea eax, [ebx + 1000h + 1]      ; los 4 punteros: presente
        mov [ebx], eax
        add eax, 1000h
        mov [ebx + 8], eax
        add eax, 1000h
        mov [ebx + 16], eax
        add eax, 1000h
        mov [ebx + 24], eax
        lea edi, [ebx + 1000h]          ; las 2048 páginas de 2 MB: presente, se
        mov eax, 83h                    ; puede escribir, grande
        mov ecx, 2048
.pd:    mov [edi], eax
        mov dword [edi + 4], 0
        add edi, 8
        add eax, 200000h
        dec ecx
        jnz .pd
        ; las del marco de la pantalla: bit PAT (12) y PWT = la entrada 5 del PAT
        mov eax, [V(fb_linea)]
        mul dword [V(fb_alto)]
        test edx, edx
        jnz .no_mem
        cmp eax, 30000000h
        jae .no_mem
        shl eax, 2
        add eax, [V(fb)]
        jc .no_mem
        dec eax
        shr eax, 21
        mov edx, eax                    ; EDX = la última
        mov eax, [V(fb)]
        shr eax, 21
        mov [V(wp_e0)], eax
        mov [V(wp_e1)], edx
.wc:    or dword [ebx + 1000h + eax * 8], 1008h
        inc eax
        cmp eax, edx
        jbe .wc
        mov ecx, 277h                   ; la entrada 5 del PAT, de WT a WC (01h);
        rdmsr                           ; el firmware no usa el bit PAT (si es que
        and edx, 0FFFF00FFh             ; usa paginación), así que a él no le
        or edx, 00000100h               ; cambia nada
        wrmsr
        mov [V(wc_cr3)], ebx
        DI t_w_tablas
        push eax
        mov eax, ebx
        mov ecx, 8
        call hex
        DI t_w_tablas2
        mov eax, [V(wp_e0)]
        call decimal
        DI t_a
        mov eax, [V(wp_e1)]
        call decimal
        DI t_w_tablas3
        pop eax
        call hex64
        DI t_nl
        jmp .medir
.no_mem:
        DI t_w_no_mem
.medir:
        ; lo que cuesta pintar la pantalla entera (en negro) por cada camino
        pushfd
        cli
        mov edi, [V(fb)]
        call medir_borrado
        mov [V(wp_t1)], eax
        mov dword [V(wp_t2)], 0
        cmp dword [V(wc_cr3)], 0
        je .medido
        call wc_entrar
        mov edi, [V(fb)]
        call medir_borrado
        mov [V(wp_t2)], eax
        call wc_salir
.medido:
        popfd
        mov eax, [V(wp_t1)]
        mov [V(t_pant)], eax
        cmp dword [V(wc_cr3)], 0
        je .log
        mov edx, eax                    ; con las tablas propias, salvo que salga
        shr edx, 2                      ; claramente más lento (un cuarto más): en
        add edx, eax                    ; un equipo de verdad es mucho más rápido
        mov eax, [V(wp_t2)]             ; o, si la pantalla ya tenía WC, igual
        cmp eax, edx
        ja .log
        mov byte [V(wc_usar)], 1
        mov [V(t_pant)], eax
.log:
%ifdef PRUEBA_LENTA
        mov eax, [V(fb_linea)]          ; (pruebas) como una pantalla lenta:
        imul eax, [V(fb_alto)]          ; PRUEBA_LENTA microsegundos por cada
        mov edx, PRUEBA_LENTA           ; millón de puntos
        mul edx
        mov ecx, 1000000
        div ecx
        mov [V(t_pant)], eax
        mov [V(wp_t1)], eax
        mov [V(wp_t2)], eax
%endif
        DI t_w_medida
        mov eax, [V(wp_t1)]
        call decimal
        DI t_w_medida2
        mov eax, [V(wp_t2)]
        call decimal
        DI t_w_medida3
        lea esi, [V(t_w_directo)]
        cmp byte [V(wc_usar)], 0
        je .cual
        lea esi, [V(t_w_portablas)]
.cual:  call escribir
        DI t_nl
        ; dónde se dibuja y la ampliación del texto
        call logica_poner
        call escala
        DI t_w_esc
        cmp dword [V(esc_k)], 0
        jne .ek
        DI t_w_esc_todo
        jmp .et
.ek:    DI t_w_esc_k
        mov eax, [V(esc_k)]
        call decimal
        DI t_w_esc_k2
.et:    mov eax, [V(dib_an)]
        call decimal
        DI t_por
        mov eax, [V(dib_al)]
        call decimal
        DI t_w_cierra
        popad
        ret

; wc_entrar: pone las tablas propias (con las interrupciones cerradas). Cambia EAX.
; wc_salir: devuelve al firmware su CR0, CR3 y CR4.
; (El código y la pila están en el mismo sitio con paginación que sin ella:
; UEFI pide al firmware que todo esté «uno a uno», y las tablas propias también.)
wc_entrar:
%ifdef PRUEBA_SIN_PAG
        ; (pruebas) como en un firmware que no usa paginación (lo corriente en
        ; los de 32 bits de verdad; OVMF sí la usa): se le quita aquí —y PAE—
        ; y se le devuelve al final de wc_salir
        mov eax, cr0
        mov [V(wc_svp0)], eax
        and eax, 7FFFFFFFh
        mov cr0, eax
        jmp .p
.p:     mov eax, cr4
        mov [V(wc_svp4)], eax
        and eax, ~20h
        mov cr4, eax
%endif
        mov eax, cr0
        mov [V(wc_sv0)], eax
        mov eax, cr3
        mov [V(wc_sv3)], eax
        mov eax, cr4
        mov [V(wc_sv4)], eax
        mov eax, [V(wc_sv0)]
        and eax, 7FFFFFFFh              ; sin paginación mientras se cambian
        mov cr0, eax
        jmp .a
.a:     mov eax, [V(wc_sv4)]
        or eax, 20h                     ; PAE
        mov cr4, eax
        mov eax, [V(wc_cr3)]
        mov cr3, eax
        mov eax, [V(wc_sv0)]
        or eax, 80000000h
        mov cr0, eax
        jmp .b
.b:     ret
wc_salir:
        push eax
        mov eax, cr0
        and eax, 7FFFFFFFh
        mov cr0, eax
        jmp .a
.a:     mov eax, [V(wc_sv4)]
        mov cr4, eax
        mov eax, [V(wc_sv3)]
        mov cr3, eax
        mov eax, [V(wc_sv0)]
        mov cr0, eax
        jmp .b
.b:
%ifdef PRUEBA_SIN_PAG
        mov eax, [V(wc_svp4)]
        mov cr4, eax
        mov eax, [V(wc_svp0)]
        mov cr0, eax
        jmp .p
.p:
%endif
        pop eax
        ret

; medir_borrado: pone en negro la pantalla entera escribiendo desde EDI
; -> EAX = microsegundos que ha costado (0 si no hay medida del TSC)
medir_borrado:
        push ebx
        push ecx
        push edx
        push esi
        rdtsc
        mov ebx, eax
        mov esi, edx
        mov ecx, [V(fb_linea)]
        imul ecx, [V(fb_alto)]
        xor eax, eax
        cld
        rep stosd
        bt dword [V(wp_cpu)], 25        ; (SSE: que salga lo que quede por escribir)
        jnc .sin_sse
        sfence
.sin_sse:
        rdtsc
        sub eax, ebx
        sbb edx, esi
        jnz .cero
        mov ecx, [V(tsc_tic)]
        test ecx, ecx
        jz .cero
        mov edx, 54925                  ; un tic = 54925 us
        mul edx
        cmp edx, ecx
        jae .cero
        div ecx
        jmp .f
.cero:  xor eax, eax
.f:     pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; registro_a_bios: lo apuntado desde reg_marca (pantalla, memoria, VBE) se copia
; al registro de la BIOS de modo real, que HBLOG.COM enseña desde DOS
registro_a_bios:
        pushad
        mov esi, [V(reg_dir)]
        test esi, esi
        jz .f
        mov eax, [V(reg_marca)]
        mov ecx, [V(reg_n)]
        sub ecx, eax
        jbe .f
        cmp ecx, REG_BIOS_TAM - 1280    ; (queda sitio para lo de VBE)
        jbe .c
        mov ecx, REG_BIOS_TAM - 1280
.c:     add esi, eax
        mov edi, [V(bios_base)]
        mov [edi + 194], cx             ; T_REG_N
        add edi, REG_BIOS
        cld
        rep movsb
.f:     popad
        ret

; escala_tope: EAX = ancho, EDX = alto de lo que hay que enseñar -> ECX = por
; cuánto (entero) se amplía: lo que quepa en la pantalla y en linea_buf y, si
; la pantalla es lenta, lo que se pinte entero en T_LENTO. Cambia EAX y EDX.
escala_tope:
        push ebx
        push esi
        push edi
        mov esi, eax
        mov edi, edx
        mov eax, [V(p_an)]
        xor edx, edx
        div esi
        mov ecx, eax
        mov eax, [V(p_al)]
        xor edx, edx
        div edi
        cmp eax, ecx
        jae .a
        mov ecx, eax
.a:     mov eax, LIN_MAX
        xor edx, edx
        div esi
        cmp eax, ecx
        jae .b
        mov ecx, eax
.b:     cmp ecx, 1
        jae .vel
        mov ecx, 1
.vel:   cmp dword [V(t_pant)], T_LENTO
        jbe .f
.prueba:
        cmp ecx, 1
        jbe .f
        mov eax, esi                    ; lo que costaría: t_pant por la parte
        imul eax, ecx                   ; de la pantalla que se pinta
        mov ebx, edi
        imul ebx, ecx
        mul ebx
        mul dword [V(t_pant)]
        mov ebx, [V(fb_linea)]
        imul ebx, [V(fb_alto)]
        cmp edx, ebx
        jae .menos
        div ebx
        cmp eax, T_LENTO
        jbe .f
.menos: dec ecx
        jmp .prueba
.f:     pop edi
        pop esi
        pop ebx
        ret

; ---- modo_poner: EDX = 0 texto; 1... modo VBE (g_modos), bit 8: sin borrar ----
modo_poner:
        pushad
        movzx eax, dl
        test eax, eax
        jz .texto
        cmp eax, G_NMODOS
        ja .texto
        cmp dword [V(vbe_lfb)], 0
        je .texto
        mov ecx, [V(g_modos) + eax * 8 - 8]
        mov ebx, [V(g_modos) + eax * 8 - 4]
        cmp ecx, [V(p_an)]
        ja .texto
        cmp ebx, [V(p_al)]
        ja .texto
        mov [V(graf)], al
        mov [V(g_an)], ecx
        mov [V(g_al)], ebx
        push edx
        mov eax, ecx
        mov edx, ebx
        call escala_tope
        pop edx
        mov [V(g_k)], ecx
        mov eax, [V(g_an)]              ; centrado
        imul eax, ecx
        mov ebx, [V(p_an)]
        sub ebx, eax
        shr ebx, 1
        mov [V(g_x0)], ebx
        mov eax, [V(g_al)]
        imul eax, ecx
        mov ebx, [V(p_al)]
        sub ebx, eax
        shr ebx, 1
        mov [V(g_y0)], ebx
        mov byte [V(g_todo)], 1
        test edx, 100h
        jnz .f
        mov edi, [V(vbe_lfb)]           ; el marco, en negro
        mov ecx, [V(g_an)]
        imul ecx, [V(g_al)]
        xor eax, eax
        cld
        rep stosd
        jmp .f
.texto: mov byte [V(graf)], 0
        mov byte [V(borrada)], 0        ; (el texto se pinta entero otra vez)
.f:     popad
        ret

; ---- escala: tablas de dónde cae cada punto de 640x400 en la pantalla lógica ----
; Se amplía lo más posible sin deformar (8:5): en 1280x800, a toda ella. Si la
; pantalla es lenta (t_pant), por un entero (esc_k): menos puntos que pintar.
; tab_x[i] = x0 + i*An/640 (i = 0..640) y tab_y[j] = y0 + j*Al/400 (j = 0..400)
escala:
        pushad
        mov dword [V(esc_k)], 0
        mov eax, [V(p_an)]
        mov ecx, [V(p_al)]
        lea edx, [eax * 4 + eax]        ; An*5 <= Al*8: manda el ancho
        lea ebx, [ecx * 8]
        cmp edx, ebx
        ja .alto
        mov [V(dib_an)], eax
        shr edx, 3
        mov [V(dib_al)], edx
        jmp .lenta
.alto:  mov [V(dib_al)], ecx
        mov eax, ebx                    ; Al*8/5
        xor edx, edx
        mov ebx, 5
        div ebx
        mov [V(dib_an)], eax
.lenta: cmp dword [V(t_pant)], T_LENTO
        jbe .origen
        mov eax, 640
        mov edx, 400
        call escala_tope
        mov [V(esc_k)], ecx
        imul eax, ecx, 640
        mov [V(dib_an)], eax
        imul eax, ecx, 400
        mov [V(dib_al)], eax
.origen:
        mov eax, [V(p_an)]
        sub eax, [V(dib_an)]
        shr eax, 1
        mov esi, eax                    ; ESI = x0
        xor ecx, ecx
.tx:    mov eax, ecx
        mul dword [V(dib_an)]
        mov ebx, 640
        div ebx
        add eax, esi
        mov [V(tab_x) + ecx * 4], eax
        inc ecx
        cmp ecx, 640
        jbe .tx
        mov eax, [V(p_al)]
        sub eax, [V(dib_al)]
        shr eax, 1
        mov esi, eax                    ; ESI = y0
        xor ecx, ecx
.ty:    mov eax, ecx
        mul dword [V(dib_al)]
        mov ebx, 400
        div ebx
        add eax, esi
        mov [V(tab_y) + ecx * 4], eax
        inc ecx
        cmp ecx, 400
        jbe .ty
        popad
        ret

; ---- pantalla_dibujar: lleva a la pantalla lo que haya cambiado (el texto o
;      el modo gráfico) y apunta lo que ha tardado (T_DIB_*) ----
; (con las interrupciones cerradas; mientras, con las tablas propias de WC)
pantalla_dibujar:
        pushad
        rdtsc
        mov [V(pd_t)], eax
        mov [V(pd_t) + 4], edx
        cmp byte [V(wc_usar)], 0
        je .sin
        call wc_entrar
.sin:   cld
        cmp byte [V(graf)], 0
        je .texto
        call graf_presentar
        jmp .hecho
.texto: call texto_dibujar
.hecho: cmp byte [V(giro)], 0
        je .derecha
        call girar_sucio
.derecha:
        cmp byte [V(wc_usar)], 0
        je .cuenta
        call wc_salir
.cuenta:
        rdtsc
        sub eax, [V(pd_t)]
        sbb edx, [V(pd_t) + 4]
        jnz .fin
        mov ecx, [V(tsc_tic)]
        test ecx, ecx
        jz .fin
        mov edx, 54925
        mul edx
        cmp edx, ecx
        jae .fin
        div ecx                         ; EAX = microsegundos
        mov ebx, [V(bios_base)]
        inc dword [ebx + 196]           ; T_DIB_N
        mov [ebx + 200], eax            ; T_DIB_ULT
        add [ebx + 224], eax            ; T_DIB_TOT
        cmp eax, [ebx + 204]            ; T_DIB_MAX
        jbe .fin
        mov [ebx + 204], eax
.fin:   popad
        ret

; texto_dibujar: pinta la pantalla de texto de la BIOS (80x25), ampliada
texto_dibujar:
        pushad
        ; la primera vez se borra todo en negro; después solo se pintan las
        ; celdas que han cambiado
        cmp byte [V(borrada)], 0
        jne .ya_borrada
        mov byte [V(borrada)], 1
        mov edi, [V(p_dir)]
        mov ecx, [V(p_lin)]
        imul ecx, [V(p_al)]
        xor eax, eax
        rep stosd
        call sucio_todo
        lea edi, [V(sombra)]            ; nada pintado todavía
        mov ecx, 2000
        mov ax, 0FFFFh
        rep stosw
.ya_borrada:
        xor edi, edi                    ; EDI = número de celda (0..1999)
.celda:
        mov ebx, [V(bios_base)]
        movzx ebx, word [ebx + TX + edi * 2] ; BL = carácter, BH = atributo
        cmp bx, [V(sombra) + edi * 2]
        je .siguiente                   ; igual que la última vez
        mov [V(sombra) + edi * 2], bx
        movzx eax, bh
        and eax, 0Fh
        mov ecx, [V(paleta) + eax * 4]  ; color de la letra
        movzx eax, bh
        shr eax, 4
        and eax, 7
        mov edx, [V(paleta) + eax * 4]  ; color del fondo
        cmp dword [V(fb_formato)], 0
        jne .bgr
        bswap ecx                       ; RGBX: R y B cambiados
        shr ecx, 8
        bswap edx
        shr edx, 8
.bgr:   movzx eax, bl
        shl eax, 4
        lea esi, [V(fuente) + eax]      ; ESI = las 16 filas del carácter
        mov eax, edi
        push edx
        xor edx, edx
        mov ebx, 80
        div ebx                         ; EAX = fila, EDX = columna
        mov ebx, edx
        pop edx
        call pintar_celda
.siguiente:
        inc edi
        cmp edi, 2000
        jb .celda
        popad
        ret

; pintar_celda: EAX = fila, EBX = columna, ESI = letra (16 octetos), ECX =
; tinta, EDX = fondo. Cada fila de puntos de la letra se amplía una vez en
; linea_buf (memoria normal) y se copia tantas veces como líneas ocupe.
pintar_celda:
        pushad
        mov [V(pc_tin)], ecx
        mov [V(pc_fon)], edx
        shl eax, 4
        mov [V(pc_y)], eax              ; la fila de puntos (0..399)
        shl ebx, 3
        mov [V(pc_x)], ebx              ; la primera columna de puntos
        cmp byte [V(giro)], 0
        je .filas
        push eax                        ; lo que ocupa la celda, al rectángulo sucio
        push ebx
        mov ecx, [V(tab_x) + ebx * 4 + 32]
        mov edx, [V(tab_y) + eax * 4 + 64]
        mov ebx, [V(tab_y) + eax * 4]
        mov eax, [esp]
        mov eax, [V(tab_x) + eax * 4]
        call sucio_mas
        pop ebx
        pop eax
.filas: mov dword [V(pc_gy)], 16
.fila:  mov al, [esi]
        inc esi
        mov [V(pc_bits)], al
        mov eax, [V(pc_y)]
        mov edx, [V(tab_y) + eax * 4]   ; EDX = primera línea de la pantalla
        cmp edx, [V(tab_y) + eax * 4 + 4]
        jae .sig
        lea edi, [V(linea_buf)]
        mov ebx, [V(pc_x)]
        mov dword [V(pc_gx)], 8
.punto: mov eax, [V(pc_fon)]
        shl byte [V(pc_bits)], 1
        jnc .color
        mov eax, [V(pc_tin)]
.color: mov ecx, [V(tab_x) + ebx * 4 + 4]
        sub ecx, [V(tab_x) + ebx * 4]
        rep stosd
        inc ebx
        dec dword [V(pc_gx)]
        jnz .punto
        mov ebx, [V(pc_x)]
        mov ecx, [V(tab_x) + ebx * 4 + 32]
        sub ecx, [V(tab_x) + ebx * 4]   ; el ancho de la celda en la pantalla
        jz .sig
        mov [V(pc_an)], ecx
        push esi
.y:     mov edi, [V(p_lin)]
        imul edi, edx
        add edi, [V(tab_x) + ebx * 4]
        shl edi, 2
        add edi, [V(p_dir)]
        lea esi, [V(linea_buf)]
        mov ecx, [V(pc_an)]
        rep movsd
        inc edx
        mov eax, [V(pc_y)]
        cmp edx, [V(tab_y) + eax * 4 + 4]
        jb .y
        pop esi
.sig:   inc dword [V(pc_y)]
        dec dword [V(pc_gy)]
        jnz .fila
        popad
        ret

; graf_presentar: lleva a la pantalla las líneas del marco lineal de VBE que
; han cambiado desde la última vez (se comparan con la copia vbe_ant), cada
; punto ampliado G_K veces y la imagen centrada
graf_presentar:
        pushad
        cmp byte [V(g_todo)], 0
        je .lineas
        mov edi, [V(p_dir)]             ; modo recién puesto: todo en negro
        mov ecx, [V(p_lin)]
        imul ecx, [V(p_al)]
        xor eax, eax
        rep stosd
        call sucio_todo
.lineas:
        mov dword [V(gp_y)], 0          ; la línea
.linea: mov ebx, [V(gp_y)]
        imul ebx, [V(g_an)]
        shl ebx, 2                      ; EBX = dónde empieza la línea en el marco
        mov dword [V(gp_xa)], 0         ; lo que ha cambiado de ella: de xa a xb
        mov eax, [V(g_an)]
        mov [V(gp_xb)], eax
        cmp byte [V(g_todo)], 0
        jne .pinta
        mov esi, [V(vbe_lfb)]
        add esi, ebx
        mov edi, [V(vbe_ant)]
        add edi, ebx
        mov ecx, [V(g_an)]
        repe cmpsd
        je .sig
        cmp byte [V(giro)], 0
        je .pinta
        mov eax, [V(g_an)]              ; (girada: solo el trozo que ha cambiado
        sub eax, ecx                    ; va después a la pantalla)
        dec eax
        mov [V(gp_xa)], eax
        mov ecx, [V(g_an)]
        mov esi, [V(vbe_lfb)]
        add esi, ebx
        lea esi, [esi + ecx * 4 - 4]
        mov edi, [V(vbe_ant)]
        add edi, ebx
        lea edi, [edi + ecx * 4 - 4]
        std
        repe cmpsd
        cld
        inc ecx
        mov [V(gp_xb)], ecx
.pinta: mov esi, [V(vbe_lfb)]
        add esi, ebx
        mov edi, [V(vbe_ant)]
        add edi, ebx
        mov ecx, [V(g_an)]
        rep movsd
        mov esi, [V(vbe_ant)]           ; la línea, ampliada, en linea_buf
        add esi, ebx
        lea edi, [V(linea_buf)]
        mov edx, [V(g_an)]
.px:    lodsd
        cmp dword [V(fb_formato)], 0
        jne .bgr
        bswap eax                       ; (pantalla con el rojo abajo)
        shr eax, 8
.bgr:   mov ecx, [V(g_k)]
        rep stosd
        dec edx
        jnz .px
        mov edx, [V(gp_y)]              ; y a la pantalla, G_K líneas iguales
        imul edx, [V(g_k)]
        add edx, [V(g_y0)]
        mov ebx, [V(g_k)]
.fila:  mov edi, [V(p_lin)]
        imul edi, edx
        add edi, [V(g_x0)]
        shl edi, 2
        add edi, [V(p_dir)]
        lea esi, [V(linea_buf)]
        mov ecx, [V(g_an)]
        imul ecx, [V(g_k)]
        rep movsd
        inc edx
        dec ebx
        jnz .fila
        cmp byte [V(giro)], 0
        je .sig
        mov eax, [V(gp_xa)]             ; al rectángulo sucio
        imul eax, [V(g_k)]
        add eax, [V(g_x0)]
        mov ecx, [V(gp_xb)]
        imul ecx, [V(g_k)]
        add ecx, [V(g_x0)]
        mov ebx, edx
        sub ebx, [V(g_k)]
        call sucio_mas
.sig:   inc dword [V(gp_y)]
        mov eax, [V(gp_y)]
        cmp eax, [V(g_al)]
        jb .linea
        mov byte [V(g_todo)], 0
        popad
        ret

; ---- el rectángulo sucio de la pantalla lógica (lo que hay que girar) ----
; sucio_mas: se le añade el rectángulo de (EAX, EBX) a (ECX, EDX), sin incluir
; estos; sucio_todo: la pantalla entera; sucio_nada: vacío
sucio_mas:
        cmp eax, [V(s_x0)]
        jae .a
        mov [V(s_x0)], eax
.a:     cmp ebx, [V(s_y0)]
        jae .b
        mov [V(s_y0)], ebx
.b:     cmp ecx, [V(s_x1)]
        jbe .c
        mov [V(s_x1)], ecx
.c:     cmp edx, [V(s_y1)]
        jbe .d
        mov [V(s_y1)], edx
.d:     ret
sucio_todo:
        push eax
        xor eax, eax
        mov [V(s_x0)], eax
        mov [V(s_y0)], eax
        mov eax, [V(p_an)]
        mov [V(s_x1)], eax
        mov eax, [V(p_al)]
        mov [V(s_y1)], eax
        pop eax
        ret
sucio_nada:
        mov dword [V(s_x0)], 7FFFFFFFh
        mov dword [V(s_y0)], 7FFFFFFFh
        mov dword [V(s_x1)], 0
        mov dword [V(s_y1)], 0
        ret

; ---- girar_sucio: el rectángulo sucio de la pantalla lógica, girado, a la
;      pantalla de verdad ----
; Al punto lógico (lx, ly) le toca en la pantalla (An x Al la de verdad):
;   giro 1 (a la derecha):   x = An - 1 - ly,  y = lx
;   giro 2 (del revés):      x = An - 1 - lx,  y = Al - 1 - ly
;   giro 3 (a la izquierda): x = ly,           y = Al - 1 - lx
; Se recorre el rectángulo de la pantalla por teselas de TES_AN x TES_AL
; puntos: cada una se compone en memoria normal (tesela) leyendo la pantalla
; lógica con dos pasos fijos (gr_paso_f: al bajar una fila de la pantalla;
; gr_paso_c: al avanzar una columna) y se escribe por filas enteras.
girar_sucio:
        pushad
        mov eax, [V(s_x0)]
        cmp eax, [V(s_x1)]
        jae .fin
        mov eax, [V(s_y0)]
        cmp eax, [V(s_y1)]
        jae .fin
        mov ebx, [V(p_lin)]
        shl ebx, 2                      ; EBX = octetos de una línea lógica
        mov al, [V(giro)]
        cmp al, 1
        je .g1
        cmp al, 2
        je .g2
        ; giro 3: x de s_y0 a s_y1; y de Al - s_x1 a Al - s_x0
        mov eax, [V(s_y0)]
        mov [V(gr_x0)], eax
        mov eax, [V(s_y1)]
        mov [V(gr_x1)], eax
        mov eax, [V(fb_alto)]
        sub eax, [V(s_x1)]
        mov [V(gr_y0)], eax
        mov eax, [V(fb_alto)]
        sub eax, [V(s_x0)]
        mov [V(gr_y1)], eax
        mov dword [V(gr_paso_f)], -4
        mov [V(gr_paso_c)], ebx
        jmp .ya
.g1:    ; giro 1: x de An - s_y1 a An - s_y0; y de s_x0 a s_x1
        mov eax, [V(fb_ancho)]
        sub eax, [V(s_y1)]
        mov [V(gr_x0)], eax
        mov eax, [V(fb_ancho)]
        sub eax, [V(s_y0)]
        mov [V(gr_x1)], eax
        mov eax, [V(s_x0)]
        mov [V(gr_y0)], eax
        mov eax, [V(s_x1)]
        mov [V(gr_y1)], eax
        mov dword [V(gr_paso_f)], 4
        neg ebx
        mov [V(gr_paso_c)], ebx
        jmp .ya
.g2:    ; giro 2: x de An - s_x1 a An - s_x0; y de Al - s_y1 a Al - s_y0
        mov eax, [V(fb_ancho)]
        sub eax, [V(s_x1)]
        mov [V(gr_x0)], eax
        mov eax, [V(fb_ancho)]
        sub eax, [V(s_x0)]
        mov [V(gr_x1)], eax
        mov eax, [V(fb_alto)]
        sub eax, [V(s_y1)]
        mov [V(gr_y0)], eax
        mov eax, [V(fb_alto)]
        sub eax, [V(s_y0)]
        mov [V(gr_y1)], eax
        neg ebx
        mov [V(gr_paso_f)], ebx
        mov dword [V(gr_paso_c)], -4
.ya:    mov eax, [V(gr_y0)]
.banda: mov [V(gr_y)], eax              ; las filas de la pantalla de esta banda
        mov ecx, [V(gr_y1)]
        sub ecx, eax
        cmp ecx, TES_AL
        jbe .nf
        mov ecx, TES_AL
.nf:    mov [V(gr_nf)], ecx
        mov eax, [V(gr_x0)]
.tesela:
        mov [V(gr_x)], eax              ; y las columnas de esta tesela
        mov ecx, [V(gr_x1)]
        sub ecx, eax
        cmp ecx, TES_AN
        jbe .nc
        mov ecx, TES_AN
.nc:    mov [V(gr_nc)], ecx
        call .origen                    ; ESI = el punto lógico de (gr_x, gr_y)
        lea edi, [V(tesela)]
        mov ebx, [V(gr_paso_f)]
        mov edx, [V(gr_nc)]
.col:   push esi
        push edi
        mov ecx, [V(gr_nf)]
.fil:   mov eax, [esi]
        mov [edi], eax
        add esi, ebx
        add edi, TES_AN * 4
        dec ecx
        jnz .fil
        pop edi
        pop esi
        add edi, 4
        add esi, [V(gr_paso_c)]
        dec edx
        jnz .col
        mov edx, [V(gr_y)]              ; la tesela, a la pantalla, fila a fila
        lea esi, [V(tesela)]
        mov ebx, [V(gr_nf)]
.sal:   mov edi, [V(fb_linea)]
        imul edi, edx
        add edi, [V(gr_x)]
        shl edi, 2
        add edi, [V(fb)]
        push esi
        mov ecx, [V(gr_nc)]
        rep movsd
        pop esi
        add esi, TES_AN * 4
        inc edx
        dec ebx
        jnz .sal
        mov eax, [V(gr_x)]
        add eax, [V(gr_nc)]
        cmp eax, [V(gr_x1)]
        jb .tesela
        mov eax, [V(gr_y)]
        add eax, [V(gr_nf)]
        cmp eax, [V(gr_y1)]
        jb .banda
.fin:   call sucio_nada
        popad
        ret
; .origen: -> ESI = dónde está en la pantalla lógica el punto que va a (gr_x, gr_y)
.origen:
        mov al, [V(giro)]
        cmp al, 1
        je .o1
        cmp al, 2
        je .o2
        mov eax, [V(fb_alto)]           ; giro 3: lx = Al - 1 - y, ly = x
        dec eax
        sub eax, [V(gr_y)]
        mov ecx, [V(gr_x)]
        jmp .o
.o1:    mov eax, [V(gr_y)]              ; giro 1: lx = y, ly = An - 1 - x
        mov ecx, [V(fb_ancho)]
        dec ecx
        sub ecx, [V(gr_x)]
        jmp .o
.o2:    mov eax, [V(fb_ancho)]          ; giro 2: lx = An - 1 - x, ly = Al - 1 - y
        dec eax
        sub eax, [V(gr_x)]
        mov ecx, [V(fb_alto)]
        dec ecx
        sub ecx, [V(gr_y)]
.o:     imul ecx, [V(p_lin)]            ; ESI = lb + (ly * línea + lx) * 4
        add ecx, eax
        mov esi, [V(p_dir)]
        lea esi, [esi + ecx * 4]
        ret

; ---- escribir: ESI = texto ASCII terminado en 0 -> al registro y, si no está
;      «callado», a la pantalla por el servicio de texto del firmware.
;      Conserva todos los registros. ----
escribir:
        pushad
        cld
        mov edx, esi
        mov edi, [V(reg_dir)]           ; al registro
        test edi, edi
        jz .pantalla
        mov ecx, [V(reg_n)]
.r:     mov al, [esi]
        test al, al
        jz .rf
        cmp ecx, REG_MAX
        jae .rf
        mov [edi + ecx], al
        inc ecx
        inc esi
        jmp .r
.rf:    mov [V(reg_n)], ecx
.pantalla:
        cmp byte [V(callado)], 0
        jne .fin
        cmp dword [V(conout)], 0        ; (hay equipos sin consola de texto)
        je .fin
        mov esi, edx
.trozo: lea edi, [V(buf16)]             ; por trozos de 255 letras
        mov ebx, 255
.c:     lodsb
        movzx eax, al
        stosw
        test al, al
        jz .ya
        dec ebx
        jnz .c
        xor eax, eax
        stosw
        mov ecx, [V(conout)]
        FW [ecx + TXT_OUTSTR], ecx, &buf16      ; (ESI y EBX se conservan)
        jmp .trozo
.ya:    mov ecx, [V(conout)]
        FW [ecx + TXT_OUTSTR], ecx, &buf16
.fin:   popad
        ret

; ---- hex: EAX en hexadecimal con ECX cifras (hasta 8); hex64: EDX:EAX con 16 ----
hex:
        pushad
        lea edi, [V(buf_txt) + ecx]
        mov byte [edi], 0
.d:     dec edi
        mov edx, eax
        and edx, 0Fh
        cmp dl, 10
        jb .n
        add dl, 'A' - 10 - '0'
.n:     add dl, '0'
        mov [edi], dl
        shr eax, 4
        dec ecx
        jnz .d
        DI buf_txt
        popad
        ret
hex64:
        push ecx
        push eax
        mov eax, edx
        mov ecx, 8
        call hex
        pop eax
        call hex
        pop ecx
        ret

; ---- decimal: EAX en decimal ----
decimal:
        pushad
        lea esi, [V(buf_txt) + 30]
        mov byte [esi], 0
        mov ecx, 10
.d:     xor edx, edx
        div ecx
        add dl, '0'
        dec esi
        mov [esi], dl
        test eax, eax
        jnz .d
        call escribir
        popad
        ret
