; =============================================================================
;  Hispa-Snes - el bus: los registros, el DMA y el HDMA, los mandos, y el
;  reloj de la consola (snes_cuadro: las líneas de un cuadro)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El reloj de la consola va a 21.477.272 Hz (PAL: 21.281.370): una línea son
;  1.364 de sus ciclos, 262 líneas por cuadro (312 en PAL). El 65C816 gasta 6
;  u 8 por ciclo suyo (según de dónde lee el código: la ROM «rápida» o la
;  lenta); el SPC700, 21 (1.024.000 por segundo). snes_cuadro corre las
;  líneas: en cada una, el procesador hasta el borrado horizontal (1.104),
;  entonces se pinta la línea y se hace el HDMA, y luego el resto. El SPC700
;  va detrás y se pone al día cuando el 65C816 toca sus puertos y al final
;  de cada línea.
;
;    snes_arrancar   enciende la consola (con el cartucho ya cargado)
;    snes_reiniciar  el botón de reinicio
;    snes_cuadro     un cuadro entero (la imagen, si [dibujar], en «imagen»)
; =============================================================================
LINEA_M     equ 1364                ; ciclos del reloj por línea
HBLANK_M    equ 1104                ; donde empieza el borrado horizontal
SPC_M       equ 21                  ; ciclos del reloj por ciclo del SPC700

; ---- encender y reiniciar ----
snes_arrancar:
        pushad
        mov edi, wram               ; la RAM, a 55h (como suele estar)
        mov ecx, 0x20000 / 4
        mov eax, 0x55555555
        rep stosd
        call snes_reiniciar
        popad
        ret

snes_reiniciar:
        pushad
        xor eax, eax
        mov edi, bus_regs
        mov ecx, BUS_REGS_TAM / 4
        rep stosd
        mov byte [wrio], 0xFF       ; (el puerto de E/S empieza todo a 1)
        mov edi, dma_reg
        mov ecx, 8 * 16 / 4
        mov eax, 0xFFFFFFFF         ; (los del DMA empiezan a FFh)
        rep stosd
        xor eax, eax
        mov [linea], eax
        mov [hpos], eax
        mov [t_linea], eax
        mov [spc_m], eax
        mov [spc_resto], eax
        mov dword [lineas], 262
        mov dword [vis_fin], 225
        cmp dword [cart_pal], 0
        je .n
        mov dword [lineas], 312
.n:
        mov dword [rel_mul], 8
        call ppu_reiniciar
        call spc_reiniciar
        call dsp_reiniciar
        call cpu_reiniciar
        popad
        ret

; =============================================================================
;  Un cuadro
; =============================================================================
snes_cuadro:
        pushad
        mov dword [linea], 0
.linea:
        mov eax, [linea]
        test eax, eax
        jnz .l1
        call cuadro_empieza
.l1:
        cmp eax, [vis_fin]
        jne .l2
        call vblank_empieza
.l2:
        ; la IRQ de esta línea (si toca) y el procesador hasta el borrado
        call irq_plan
        mov eax, [irq_h]
        cmp eax, HBLANK_M
        jae .sin_irq_antes
        call correr_hasta
        call irq_dispara
.sin_irq_antes:
        mov eax, HBLANK_M
        call correr_hasta
        mov byte [en_hblank], 1
        ; la línea: se pinta (las de dentro de la pantalla) y el HDMA
        mov eax, [linea]
        test eax, eax
        jz .nopinta
        cmp eax, [vis_fin]
        jae .nopinta
        cmp eax, 225
        jae .nopinta
        cmp byte [dibujar], 0
        je .nopinta
        call ppu_linea
.nopinta:
        mov eax, [linea]
        cmp eax, [vis_fin]
        jae .sinhdma
        call hdma_linea
.sinhdma:
        mov eax, [irq_h]
        cmp eax, HBLANK_M
        jb .sin_irq_despues
        cmp eax, LINEA_M
        jae .sin_irq_despues
        call correr_hasta
        call irq_dispara
.sin_irq_despues:
        mov eax, LINEA_M
        call correr_hasta
        mov byte [en_hblank], 0
        ; el sonido, al día, y la línea siguiente
        mov eax, [t_linea]
        add eax, LINEA_M
        call spc_sincro
        add dword [t_linea], LINEA_M
        sub dword [hpos], LINEA_M
        inc dword [linea]
        mov eax, [linea]
        cmp eax, [lineas]
        jb .linea
        call dsp_cuadro             ; (las muestras del cuadro, a la salida)
        inc dword [cuadros]
        popad
        ret

; el principio del cuadro (línea 0)
cuadro_empieza:
        pushad
        and byte [rdnmi], 0x7F      ; se acaba el borrado vertical
        mov byte [en_vblank], 0
        call hdma_empieza
        xor byte [campo], 0x80      ; (el campo del entrelazado)
        popad
        ret

; el borrado vertical (línea 225, o 240 con «overscan»)
vblank_empieza:
        pushad
        mov byte [en_vblank], 1
        or byte [rdnmi], 0x80
        test byte [nmitimen], 0x80
        jz .a
        mov byte [nmi_pend], 1
.a:
        call ppu_vblank             ; (la dirección de la OAM vuelve a la suya)
        test byte [nmitimen], 1     ; la lectura de los mandos sola
        jz .x
        mov eax, [pad]
        mov [joy_res], eax
        mov eax, [pad+4]
        mov [joy_res+4], eax
        mov eax, [linea]
        add eax, 3
        mov [joy_ocup], eax
.x:
        popad
        ret

; la IRQ de los temporizadores en esta línea: -> [irq_h] el punto (o
; LINEA_M si no hay)
irq_plan:
        push eax
        mov dword [irq_h], LINEA_M + 1
        mov al, [nmitimen]
        and al, 0x30
        jz .x
        cmp al, 0x10
        je .h                       ; solo H: en todas las líneas
        mov eax, [linea]            ; V (con H o sin ella): solo en VTIME
        cmp eax, [vtime]
        jne .x
        mov al, [nmitimen]
        and al, 0x30
        cmp al, 0x20
        jne .h
        mov dword [irq_h], 10       ; solo V: al empezar la línea
        jmp .x
.h:
        mov eax, [htime]
        shl eax, 2
        add eax, 14
        mov [irq_h], eax
.x:
        pop eax
        ret
irq_dispara:
        or byte [timeup], 0x80
        mov byte [irq_linea], 1
        mov dword [irq_h], LINEA_M + 1
        ret

; EAX = el punto de la línea: el 65C816 corre hasta él
correr_hasta:
        pushad
        mov [hasta], eax
.otra:
        mov eax, [hasta]
        sub eax, [hpos]
        jle .x
        cmp byte [replan], 0        ; (han cambiado los temporizadores: el
        je .r                       ;  punto de la IRQ, otra vez)
        mov byte [replan], 0
        call irq_replan
.r:
        mov ecx, [rel_mul]          ; los ciclos del procesador que caben
        add eax, ecx
        dec eax
        xor edx, edx
        div ecx
        mov edi, eax
        mov [seg_n], eax
        mov eax, [hpos]
        mov [seg_h0], eax
        mov dword [dma_m], 0
        call cpu_correr
        mov eax, [seg_n]            ; lo gastado, al reloj
        sub eax, edi
        imul eax, [rel_mul]
        add eax, [dma_m]
        add [hpos], eax
        mov dword [dma_m], 0
        call rel_velocidad
        jmp .otra
.x:
        popad
        ret

; (si el programa ha cambiado NMITIMEN, HTIME o VTIME a mitad de línea)
irq_replan:
        push eax
        mov eax, [irq_h]
        cmp eax, LINEA_M
        ja .x
        call irq_plan
        mov eax, [irq_h]
        cmp eax, [hpos]
        jae .x
        call irq_dispara            ; (ya ha pasado: ahora)
.x:
        pop eax
        ret

; cuántos ciclos del reloj por ciclo del procesador: 6 si lee el código de la
; ROM rápida (bancos 80-FF con MEMSEL), 8 si no
rel_velocidad:
        push eax
        mov dword [rel_mul], 8
        test byte [memsel], 1
        jz .x
        mov eax, [r_pbr]
        test eax, 0x800000
        jz .x
        cmp eax, 0xC00000
        jae .r
        test word [r_pc], 0x8000
        jz .x
.r:
        mov dword [rel_mul], 6
.x:
        pop eax
        ret

; (dentro del procesador) -> EAX = el punto de la línea en este momento
bus_hpos:
        mov eax, [seg_n]
        sub eax, edi
        imul eax, [rel_mul]
        add eax, [seg_h0]
        add eax, [dma_m]
        ret

; EAX = el reloj (el de t_linea): el SPC700 corre hasta él
spc_sincro:
        pushad
        mov edx, eax
        sub eax, [spc_m]
        jle .x
        xor edx, edx
        mov ecx, SPC_M
        div ecx                     ; EAX = ciclos del SPC700
        test eax, eax
        jz .x
        imul edx, eax, SPC_M
        add [spc_m], edx
        add eax, [spc_resto]
        jle .r
        mov edi, eax
        call spc_correr
        mov eax, edi
.r:
        mov [spc_resto], eax
        mov eax, [spc_t]
        call dsp_hasta
.x:
        popad
        ret
; (dentro del procesador) el SPC700, al día con el 65C816
spc_ahora:
        push eax
        call bus_hpos
        add eax, [t_linea]
        call spc_sincro
        pop eax
        ret

; =============================================================================
;  Los registros (las direcciones que pag_lee no da directamente)
; =============================================================================
; EDX = dirección -> EAX (conserva lo demás)
bus_lee:
        push ebx
        mov ebx, edx
        shr ebx, 13
        movzx ebx, byte [pag_tipo+ebx]
        cmp ebx, PT_REG
        je .reg
        cmp ebx, PT_SRAM
        je .sram
.abierto:
        mov eax, edx                ; (sin nada: el «bus abierto», lo último
        shr eax, 8                  ;  que pasó por él, que suele ser la parte
        and eax, 0xFF               ;  alta de la dirección)
        pop ebx
        ret
.sram:
        mov eax, [sram_tam]
        dec eax
        and eax, edx
        movzx eax, byte [sram+eax]
        pop ebx
        ret
.reg:
        movzx ebx, dx
        cmp ebx, 0x2100
        jb .abierto
        cmp ebx, 0x2200
        jb .b
        cmp ebx, 0x4016
        je .j1
        cmp ebx, 0x4017
        je .j2
        cmp ebx, 0x4200
        jb .abierto
        cmp ebx, 0x4220
        jb .cpu
        cmp ebx, 0x4300
        jb .abierto
        cmp ebx, 0x4380
        jae .abierto
        and ebx, 0x7F               ; los del DMA
        movzx eax, byte [dma_reg+ebx]
        pop ebx
        ret
.b:
        push edx
        movzx edx, dl
        call bus_b_lee
        pop edx
        pop ebx
        ret
.j1:
        xor ebx, ebx
        jmp .serie
.j2:
        mov ebx, 1
.serie:                             ; los mandos uno a uno (como el de 8 bits)
        movzx eax, word [joy_serie+ebx*2]
        cmp byte [joy_tira], 0
        jne .s1
        shl word [joy_serie+ebx*2], 1
        or byte [joy_serie+ebx*2], 1
.s1:
        shr eax, 15
        and eax, 1
        pop ebx
        ret
.cpu:
        sub ebx, 0x4200
        cmp ebx, 0x10
        je .rdnmi
        cmp ebx, 0x11
        je .timeup
        cmp ebx, 0x12
        je .hvbjoy
        cmp ebx, 0x13
        je .rdio
        cmp ebx, 0x14
        jb .abierto
        movzx eax, byte [rddiv+ebx-0x14] ; 4214h-421Fh: la división, la
        pop ebx                          ; multiplicación y los mandos
        ret
.rdnmi:
        movzx eax, byte [rdnmi]
        or al, 2                    ; (la versión de la CPU)
        and byte [rdnmi], 0x7F
        pop ebx
        ret
.timeup:
        movzx eax, byte [timeup]
        mov byte [timeup], 0
        mov byte [irq_linea], 0
        pop ebx
        ret
.hvbjoy:
        xor eax, eax
        cmp byte [en_vblank], 0
        je .h1
        or al, 0x80
.h1:
        push eax
        call bus_hpos
        cmp eax, HBLANK_M - 8
        jae .h2
        cmp eax, 4
        jae .h3
.h2:
        or byte [esp], 0x40
.h3:
        mov eax, [linea]
        cmp eax, [joy_ocup]
        jae .h4
        cmp byte [en_vblank], 0
        je .h4
        or byte [esp], 1
.h4:
        pop eax
        pop ebx
        ret
.rdio:
        movzx eax, byte [wrio]
        pop ebx
        ret

; EDX = dirección, AL = dato (conserva lo demás; puede pedir al procesador
; que pare: cpu_romper y EDI)
bus_escribe:
        push ebx
        mov ebx, edx
        shr ebx, 13
        movzx ebx, byte [pag_tipo+ebx]
        cmp ebx, PT_REG
        je .reg
        cmp ebx, PT_SRAM
        je .sram
.x:
        pop ebx
        ret
.sram:
        mov ebx, [sram_tam]
        dec ebx
        and ebx, edx
        mov [sram+ebx], al
        pop ebx
        ret
.reg:
        movzx ebx, dx
        cmp ebx, 0x2100
        jb .x
        cmp ebx, 0x2200
        jb .b
        cmp ebx, 0x4016
        je .j
        cmp ebx, 0x4200
        jb .x
        cmp ebx, 0x4220
        jb .cpu
        cmp ebx, 0x4300
        jb .x
        cmp ebx, 0x4380
        jae .x
        and ebx, 0x7F
        mov [dma_reg+ebx], al
        pop ebx
        ret
.b:
        push edx
        movzx edx, dl
        call bus_b_escribe
        pop edx
        pop ebx
        ret
.j:
        and al, 1                   ; el «latch» de los mandos uno a uno
        mov [joy_tira], al
        jz .x
        push eax
        mov eax, [pad]
        mov [joy_serie], eax
        pop eax
        pop ebx
        ret
.cpu:
        sub ebx, 0x4200
        jmp [.tab+ebx*4]
.tab    dd .nmitimen, .wrio, .mpya, .mpyb, .diva, .divah, .divb, .htl
        dd .hth, .vtl, .vth, .mdma, .hdma, .memsel, .x, .x
        dd .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x, .x
.nmitimen:
        mov ah, [nmitimen]
        mov [nmitimen], al
        test al, 0x30               ; sin la IRQ: la línea se suelta
        jnz .n1
        mov byte [irq_linea], 0
        mov byte [timeup], 0
.n1:
        mov byte [replan], 1
        test al, 0x80               ; NMI encendida en pleno borrado vertical
        jz .x                       ; (con la bandera puesta): ahora
        test ah, 0x80
        jnz .x
        test byte [rdnmi], 0x80
        jz .x
        mov byte [nmi_pend], 1
        add [cpu_romper], edi
        mov edi, 1
        pop ebx
        ret
.wrio:
        test byte [wrio], 0x80      ; el bit 7 de 1 a 0: quedan los contadores
        jz .w1
        test al, 0x80
        jnz .w1
        call ppu_contadores
.w1:
        mov [wrio], al
        pop ebx
        ret
.mpya:
        mov [mpya], al
        pop ebx
        ret
.mpyb:
        push eax
        mov ah, [mpya]
        mul ah
        mov [rdmpy], ax
        pop eax
        pop ebx
        ret
.diva:
        mov [diva], al
        pop ebx
        ret
.divah:
        mov [diva+1], al
        pop ebx
        ret
.divb:
        push eax
        push ecx
        push edx
        movzx ecx, al
        movzx eax, word [diva]
        test ecx, ecx
        jz .d0
        xor edx, edx
        div ecx
        mov [rddiv], ax
        mov [rdmpy], dx
        jmp .d1
.d0:
        mov word [rddiv], 0xFFFF    ; entre 0: FFFFh y el resto, el dividendo
        mov [rdmpy], ax
.d1:
        pop edx
        pop ecx
        pop eax
        pop ebx
        ret
.htl:
        mov [htime], al
        mov byte [replan], 1
        pop ebx
        ret
.hth:
        and al, 1
        mov [htime+1], al
        mov byte [replan], 1
        pop ebx
        ret
.vtl:
        mov [vtime], al
        mov byte [replan], 1
        pop ebx
        ret
.vth:
        and al, 1
        mov [vtime+1], al
        mov byte [replan], 1
        pop ebx
        ret
.mdma:
        call dma_hacer
        pop ebx
        ret
.hdma:
        mov [hdmaen], al
        pop ebx
        ret
.memsel:
        mov [memsel], al
        pop ebx
        ret

; ---- el bus B ($21xx): EDX = el registro (00h-FFh) ----
bus_b_lee:
        cmp edx, 0x40
        jb ppu_lee
        cmp edx, 0x80
        jb .apu
        je .wram
        mov eax, edx                ; (lo demás: nada)
        ret
.apu:
        call spc_ahora
        mov eax, edx
        and eax, 3
        movzx eax, byte [s_sal+eax]
        ret
.wram:
        push edx
        mov edx, [wm_dir]
        and edx, 0x1FFFF
        movzx eax, byte [wram+edx]
        inc dword [wm_dir]
        pop edx
        ret
bus_b_escribe:
        cmp edx, 0x40
        jb ppu_escribe
        cmp edx, 0x80
        jb .apu
        je .wram
        cmp edx, 0x84
        jae .x
        push ecx                    ; 2181h-2183h: la dirección de la RAM
        lea ecx, [edx-0x81]
        shl ecx, 3
        push eax
        movzx eax, al
        shl eax, cl
        push edx
        mov edx, 0xFF
        shl edx, cl
        not edx
        and [wm_dir], edx
        pop edx
        or [wm_dir], eax
        and dword [wm_dir], 0x1FFFF
        pop eax
        pop ecx
.x:
        ret
.apu:
        call spc_ahora
        push edx
        and edx, 3
        mov [s_ent+edx], al
        pop edx
        ret
.wram:
        push edx
        mov edx, [wm_dir]
        and edx, 0x1FFFF
        mov [wram+edx], al
        inc dword [wm_dir]
        pop edx
        ret

; =============================================================================
;  El DMA: AL = los canales (MDMAEN). Se hace de una vez; el tiempo que tarda
;  (8 ciclos del reloj por octeto) para al procesador
; =============================================================================
; los registros de cada canal en dma_reg + 16*canal
D_P         equ 0                   ; el modo
D_BB        equ 1                   ; el registro del bus B
D_A1        equ 2                   ; la dirección (16 bits)
D_A1B       equ 4                   ;   y su banco
D_DAS       equ 5                   ; cuántos (o la dirección indirecta del HDMA)
D_DASB      equ 7                   ; el banco indirecto
D_A2        equ 8                   ; (HDMA) por dónde va en la tabla
D_NTRL      equ 10                  ; (HDMA) las líneas que quedan

dma_hacer:
        pushad
        mov [dma_canales], al
        xor ebp, ebp                ; EBP = el canal
.canal:
        mov al, [dma_canales]
        bt eax, ebp
        jnc near .sig
        mov esi, ebp
        shl esi, 4
        add esi, dma_reg            ; ESI = sus registros
        add dword [dma_m], 8
        movzx ecx, word [esi+D_DAS] ; ECX = cuántos octetos
        test ecx, ecx
        jnz .c
        mov ecx, 0x10000
.c:
        movzx ebx, byte [esi+D_P]
        and ebx, 7
        mov ebx, [dma_patron+ebx*4] ; EBX = los desplazamientos del registro B
        xor edi, edi                ; (EDI = por cuál va del patrón)
        ; el paso de la dirección A
        mov eax, 1
        test byte [esi+D_P], 0x08
        jz .p1
        xor eax, eax
        jmp .p2
.p1:
        test byte [esi+D_P], 0x10
        jz .p2
        neg eax
.p2:
        mov [dma_paso], eax
.octeto:
        movzx edx, word [esi+D_A1]
        movzx eax, byte [esi+D_A1B]
        shl eax, 16
        or edx, eax                 ; EDX = la dirección A
        push ecx
        mov ecx, edi
        and ecx, 3
        shl ecx, 3
        mov eax, ebx
        shr eax, cl
        and eax, 0xFF
        add al, [esi+D_BB]          ; EAX = el registro B
        pop ecx
        test byte [esi+D_P], 0x80
        jnz .b_a
        push eax                    ; A -> B
        call dma_lee_a
        mov [dma_dato], al
        pop edx
        mov al, [dma_dato]
        call bus_b_escribe
        jmp .sigue
.b_a:                               ; B -> A
        push edx
        mov edx, eax
        call bus_b_lee
        pop edx
        call dma_escribe_a
.sigue:
        mov eax, [dma_paso]
        add [esi+D_A1], ax
        dec word [esi+D_DAS]
        add dword [dma_m], 8
        inc edi
        dec ecx
        jnz .octeto
        mov word [esi+D_DAS], 0
.sig:
        inc ebp
        cmp ebp, 8
        jb .canal
        popad
        ret

; (DMA y HDMA) el lado A: EDX = dirección. No se leen ni se escriben los
; registros por aquí (como en la consola): esos dan FFh/nada
dma_lee_a:
        mov eax, edx
        shr eax, 13
        cmp byte [pag_tipo+eax], PT_REG
        je .nada
        jmp rd8
.nada:
        mov eax, 0xFF
        ret
dma_escribe_a:
        push eax
        mov eax, edx
        shr eax, 13
        cmp byte [pag_tipo+eax], PT_REG
        pop eax
        je .x
        jmp wr8
.x:
        ret

; los patrones de registro B de cada modo (un octeto por paso, de 4 en 4)
        align 4
dma_patron  dd 0x00000000, 0x01000100, 0x00000000, 0x01010000
            dd 0x03020100, 0x01000100, 0x00000000, 0x01010000
; cuántos octetos por paso de HDMA en cada modo
hdma_n      db 1, 2, 2, 4, 4, 4, 2, 4

; =============================================================================
;  El HDMA: unos octetos en cada línea, de una tabla
; =============================================================================
hdma_empieza:
        pushad
        mov byte [hdma_vivos], 0
        xor ebp, ebp
.c:
        mov al, [hdmaen]
        bt eax, ebp
        jnc .s
        mov esi, ebp
        shl esi, 4
        add esi, dma_reg
        mov ax, [esi+D_A1]          ; la tabla, desde el principio
        mov [esi+D_A2], ax
        call hdma_carga
.s:
        inc ebp
        cmp ebp, 8
        jb .c
        popad
        ret

; (interna) ESI = el canal, EBP = su número: lee la cuenta de líneas (y la
; dirección indirecta) de la tabla
hdma_carga:
        call .octeto
        mov [esi+D_NTRL], al
        test al, al
        jz .fin
        test byte [esi+D_P], 0x40
        jz .si
        call .octeto
        mov [esi+D_DAS], al
        call .octeto
        mov [esi+D_DAS+1], al
.si:
        bts dword [hdma_vivos], ebp
        bts dword [hdma_hace], ebp
        add dword [dma_m], 8
        ret
.fin:
        btr dword [hdma_vivos], ebp
        ret
.octeto:
        movzx edx, word [esi+D_A2]
        movzx eax, byte [esi+D_A1B]
        shl eax, 16
        or edx, eax
        call dma_lee_a
        inc word [esi+D_A2]
        ret

; las transferencias de una línea
hdma_linea:
        pushad
        mov al, [hdma_vivos]
        and al, [hdmaen]
        jz .x
        add dword [dma_m], 18
        xor ebp, ebp
.c:
        mov al, [hdma_vivos]
        and al, [hdmaen]
        bt eax, ebp
        jnc near .s
        mov esi, ebp
        shl esi, 4
        add esi, dma_reg
        bt dword [hdma_hace], ebp
        jnc .cuenta
        ; los octetos de esta línea
        movzx ebx, byte [esi+D_P]
        and ebx, 7
        movzx ecx, byte [hdma_n+ebx]
        mov ebx, [dma_patron+ebx*4]
        xor edi, edi
.o:
        test byte [esi+D_P], 0x40
        jnz .ind
        movzx edx, word [esi+D_A2]
        movzx eax, byte [esi+D_A1B]
        shl eax, 16
        or edx, eax
        inc word [esi+D_A2]
        jmp .dir
.ind:
        movzx edx, word [esi+D_DAS]
        movzx eax, byte [esi+D_DASB]
        shl eax, 16
        or edx, eax
        inc word [esi+D_DAS]
.dir:
        push ecx
        mov ecx, edi
        shl ecx, 3
        mov eax, ebx
        shr eax, cl
        and eax, 0xFF
        add al, [esi+D_BB]
        pop ecx
        test byte [esi+D_P], 0x80
        jnz .ba
        push eax
        call dma_lee_a
        mov [dma_dato], al
        pop edx
        mov al, [dma_dato]
        call bus_b_escribe
        jmp .os
.ba:
        push edx
        mov edx, eax
        call bus_b_lee
        pop edx
        call dma_escribe_a
.os:
        add dword [dma_m], 8
        inc edi
        dec ecx
        jnz .o
.cuenta:
        ; la cuenta de líneas
        mov al, [esi+D_NTRL]
        dec al
        mov [esi+D_NTRL], al
        btr dword [hdma_hace], ebp
        test al, 0x80
        jz .r
        bts dword [hdma_hace], ebp
.r:
        test al, 0x7F
        jnz .s
        call hdma_carga
.s:
        inc ebp
        cmp ebp, 8
        jb .c
.x:
        popad
        ret
