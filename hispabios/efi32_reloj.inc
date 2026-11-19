; =============================================================================
;  HispaBIOS (IA32) - el reloj de DOS en este equipo y el despachador
;  (parte de efi32.asm)
; =============================================================================
;  reloj_mirar: DOS vive del 8254 (PIT) por la IRQ 0. En los equipos de Intel
;  recientes el firmware lo deja PARADO («8254 clock gating») y entonces no
;  cuenta ni interrumpe; en los Atom «Bay Trail» el 8254 suele andar, pero el
;  camino de siempre (8259 por LINT0) no siempre. Como en efi64.asm:
;   1. se mira si el contador del canal 0 se mueve;
;   2. si no, y el puente es un PCH de Intel, se le quita el bloqueo: registro
;      ITSSPRC (desplazamiento 3300h del puerto ITSS, C4h, de la banda lateral
;      del PCH), bit 2 = 8254CGE, por SBREG_BAR del dispositivo P2SB (00:1F.1),
;      que el firmware deja oculto (coreboot: soc/intel/common/block/itss y
;      p2sb; Linux: drivers/platform/x86/p2sb.c). Un Bay Trail no tiene P2SB:
;      se ve que no aparece y se sigue;
;   3. se apunta lo que hay (tablas HPET y APIC de ACPI, el HPET, el APIC local
;      y el IOAPIC) y se pasa a la BIOS la dirección del HPET y del IOAPIC: es
;      ella la que prueba, con las interrupciones abiertas, qué fuente da la
;      IRQ 0 (bios64.asm, reloj_elegir).
;  En el modo seguro no se toca nada de eso y la BIOS solo prueba el 8254 y
;  el reloj CMOS por LINT0. Los registros que no estén en las tablas de
;  páginas del firmware (si usa paginación) no se tocan (mapeada).
P2SB            equ (31 << 11) | (1 << 8)   ; 00:1F.1 en CF8h
LPC             equ (31 << 11)              ; 00:1F.0
ITSSPRC         equ (0C4h << 16) + 3300h    ; puerto ITSS, registro ITSSPRC
reloj_mirar:
        pushad
        mov edi, [V(bios_base)]         ; EDI = la BIOS
        mov al, [V(callado)]
        mov [V(rj_callado)], al
        mov byte [V(callado)], 1        ; (solo al registro)
%ifdef PRUEBA_RELOJ
        mov byte [edi + 255], PRUEBA_RELOJ      ; T_PRUEBA
%endif
%ifdef PRUEBA_FW_SIN_RELOJ
        or byte [edi + 312], 80h        ; T_TEC_OPC: el firmware, sin interrupciones
%endif
        cmp byte [V(seguro)], 0
        je .mirar
        or byte [edi + 255], 2Eh        ; modo seguro: solo el 8254 y el reloj CMOS
                                        ; por LINT0 (puertos de siempre)
        DI t_k_seguro
        jmp .fin
.mirar:
        ; ---- 1. ¿cuenta el 8254? ----
        call pit_cuenta
        DI t_k_pit
        call pit_decir
        jc .parado
        or byte [edi + 254], 3          ; T_PIT: contaba y cuenta
        jmp .tablas
.parado:
        ; ---- 2. el bloqueo del 8254 en el PCH de Intel ----
        call itss_abrir
        jc .tablas
%ifdef PRUEBA_P2SB
        DI t_sim                        ; (pruebas) cómo ha quedado el P2SB de mentira
        movzx eax, byte [V(sim_oculto)]
        call decimal
        DI t_sim2
        movzx eax, byte [V(sim_cambios)]
        call decimal
        DI t_sim3
        mov eax, [V(sim_prc)]
        mov ecx, 8
        call hex
        DI t_nl
%endif
        call pit_cuenta
        DI t_k_pit2
        call pit_decir
        jc .tablas
        or byte [edi + 254], 6          ; T_PIT: cuenta ahora, tras quitar el bloqueo
.tablas:
        ; ---- 3. lo que hay: HPET, APIC local, IOAPIC ----
        mov eax, 'HPET'
        call acpi_tabla
        test eax, eax
        jnz .hpet
        DI t_k_sin_hpet
        jmp .apic
.hpet:  mov ebx, [eax + 44]             ; la dirección (en +40, el tipo de espacio)
        mov esi, [eax + 48]
        mov ecx, [eax + 36]             ; el identificador del bloque
        mov [V(rm_a)], ecx
        DI t_k_hpet
        mov eax, ebx
        mov edx, esi
        call hex64
        DI t_k_hpet_id
        mov eax, [V(rm_a)]
        mov ecx, 8
        call hex
        test esi, esi
        jnz .hpet_alto
        test ebx, ebx
        jz .hpet_nl
        mov [edi + 256], ebx            ; T_HPET
        mov eax, ebx
        call mapeada
        jc .hpet_nomap
        DI t_k_hpet_cap
        mov eax, [ebx]
        mov edx, [ebx + 4]
        call hex64
        DI t_k_hpet_cfg
        mov eax, [ebx + 10h]
        mov ecx, 8
        call hex
        DI t_k_hpet_t0
        mov eax, [ebx + 100h]
        mov edx, [ebx + 104h]
        call hex64
        jmp .hpet_nl
.hpet_alto:
        DI t_k_alto
        jmp .hpet_nl
.hpet_nomap:
        DI t_k_nomap
.hpet_nl:
        DI t_nl
.apic:  mov ecx, 1Bh                    ; IA32_APIC_BASE
        rdmsr
        mov [V(rm_a)], eax
        mov [V(rm_b)], edx
        DI t_k_lapic
        call hex64
        mov eax, [V(rm_a)]
        bt eax, 11
        jnc .lapic_nl
        bt eax, 10
        jc .x2
        cmp dword [V(rm_b)], 0          ; (por encima de 4 GB: no se llega)
        jne .lapic_nomap
        and eax, 0FFFFF000h
        mov ebx, eax
        call mapeada
        jc .lapic_nomap
        mov eax, [ebx + 0F0h]           ; SVR, LINT0, LINT1, temporizador
        mov [V(rm_a)], eax
        mov eax, [ebx + 350h]
        mov [V(rm_b)], eax
        mov eax, [ebx + 360h]
        mov [V(rm_c)], eax
        mov eax, [ebx + 320h]
        mov [V(rm_d)], eax
        jmp .lapic_decir
.x2:    DI t_k_x2
        mov ecx, 80Fh
        rdmsr
        mov [V(rm_a)], eax
        mov ecx, 835h
        rdmsr
        mov [V(rm_b)], eax
        mov ecx, 836h
        rdmsr
        mov [V(rm_c)], eax
        mov ecx, 832h
        rdmsr
        mov [V(rm_d)], eax
.lapic_decir:
        mov ecx, 8
        DI t_k_svr
        mov eax, [V(rm_a)]
        call hex
        DI t_k_lint0
        mov eax, [V(rm_b)]
        call hex
        DI t_k_lint1
        mov eax, [V(rm_c)]
        call hex
        DI t_k_lvtt
        mov eax, [V(rm_d)]
        call hex
        jmp .lapic_nl
.lapic_nomap:
        DI t_k_nomap
.lapic_nl:
        DI t_nl
        ; la tabla APIC (MADT): el IOAPIC y a qué entrada va la IRQ 0
        mov eax, 'APIC'
        call acpi_tabla
        test eax, eax
        jnz .madt
        DI t_k_sin_madt
        jmp .fin
.madt:  mov ebx, eax
        DI t_k_madt
        mov eax, [ebx + 40]
        mov ecx, 8
        call hex
        mov eax, [ebx + 4]              ; el fin de la tabla
        add eax, ebx
        mov [V(rm_a)], eax
        lea esi, [ebx + 44]             ; ESI = cada entrada
        mov dword [V(rm_b)], 0          ; el IOAPIC de la entrada 0
.e:     lea eax, [esi + 2]
        cmp eax, [V(rm_a)]
        ja .madt_f
        movzx eax, byte [esi + 1]
        cmp eax, 2
        jb .madt_f
        cmp byte [esi], 1               ; IOAPIC
        jne .e2
        DI t_k_ioapic
        mov eax, [esi + 4]
        mov ecx, 8
        call hex
        DI t_k_gsi
        mov eax, [esi + 8]
        call decimal
        cmp dword [esi + 8], 0
        jne .es
        mov eax, [esi + 4]
        mov [V(rm_b)], eax
        jmp .es
.e2:    cmp byte [esi], 2               ; una IRQ que va a otra entrada
        jne .es
        cmp byte [esi + 3], 0
        jne .es
        DI t_k_irq0
        mov eax, [esi + 4]
        call decimal
.es:    movzx eax, byte [esi + 1]
        add esi, eax
        jmp .e
.madt_f:
        mov ebx, [V(rm_b)]
        test ebx, ebx
        jz .madt_nl
        mov [edi + 260], ebx            ; T_IOAPIC
        mov eax, ebx
        call mapeada
        jc .ioa_nomap
        pushfd                          ; sus entradas 0 (el 8259) y 2 (la IRQ 0)
        cli
        mov dword [ebx], 10h
        mov esi, [ebx + 10h]
        mov dword [ebx], 14h
        mov edx, [ebx + 10h]
        popfd
        DI t_k_ioa0
        mov eax, esi
        mov ecx, 8
        call hex
        DI t_k_ioa2
        mov eax, edx
        call hex
        jmp .madt_nl
.ioa_nomap:
        DI t_k_nomap
.madt_nl:
        DI t_nl
.fin:   mov al, [V(rj_callado)]
        mov [V(callado)], al
        popad
        ret

; pit_cuenta: programa el canal 0 del 8254 como lo quiere DOS (modo 3, 65536
; cuentas: 18,2 Hz) y lee el contador dos veces con 2 ms entre medias ->
; pit_l1 y pit_l2; CF si no se ha movido
pit_cuenta:
        pushad
        pushfd
        cli
        mov al, 36h
        out 43h, al
        xor al, al
        out 40h, al
        out 40h, al
        popfd
        call .espera
        call .leer
        mov [V(pit_l1)], eax
        call .espera
        call .leer
        mov [V(pit_l2)], eax
        popad
        jmp pit_avanza
.leer:  pushfd
        cli
        xor eax, eax
        out 43h, al                     ; (la cuenta del canal 0, retenida)
        in al, 40h
        mov ah, al
        in al, 40h
        xchg al, ah
        popfd
        ret
.espera:                                ; 2 ms por el TSC (o unas vueltas, sin él)
        mov ecx, [V(tsc_tic)]
        shr ecx, 5
        jnz .t
        mov ecx, 2000000
.v:     pause
        dec ecx
        jnz .v
        ret
.t:     rdtsc
        mov ebx, eax
        mov esi, edx
.te:    pause
        rdtsc
        sub eax, ebx
        sbb edx, esi
        jnz .ef
        cmp eax, ecx
        jb .te
.ef:    ret
; pit_decir: apunta «cuenta» o «NO cuenta» con las dos lecturas -> CF si no cuenta
pit_decir:
        push eax
        push ecx
        mov eax, [V(pit_l1)]
        cmp eax, [V(pit_l2)]
        jne .mueve
        DI t_k_no
        jmp .l
.mueve: call pit_avanza
        jnc .si
        DI t_k_lento
        jmp .l
.si:    DI t_k_si
.l:     mov eax, [V(pit_l1)]
        mov ecx, 4
        call hex
        DI t_k_y
        mov eax, [V(pit_l2)]
        call hex
        DI t_k_fin
        pop ecx
        pop eax
        ; (sigue)
; pit_avanza: ¿han pasado entre las dos lecturas (pit_l1 y pit_l2, 2 ms) al
; menos 256 cuentas? A 1,19 MHz son unas 2.400 (el doble en el modo 3). El
; Monovate 14.1 (Tiger Lake) con el «clock gating» del PCH da un 8254 que solo
; baja 12 cuentas por lectura: se mueve, pero no cuenta el tiempo, y sin esto
; se tomaba por bueno y no se probaba a quitar el bloqueo. -> CF si no avanza
pit_avanza:
        push eax
        mov eax, [V(pit_l1)]
        sub eax, [V(pit_l2)]            ; (baja: la primera es la mayor)
        and eax, 0FFFFh
        cmp eax, 256                    ; CF = 1 si son menos de 256
        pop eax
        ret

; pci_leer: EAX = dispositivo y registro (bus 0) -> EAX = la doble palabra
pci_leer:
%ifdef PRUEBA_P2SB
        ; (pruebas) un P2SB de mentira, oculto, con el bit 8254CGE puesto en un
        ; ITSSPRC de mentira (sim_prc): QEMU no tiene PCH con banda lateral
        push eax
        and eax, 0FF00h
        cmp eax, P2SB
        pop eax
        jne .real
        cmp byte [V(sim_oculto)], 0
        jne .sim_ff
        cmp al, 00h
        je .sim_id
        cmp al, 08h
        je .sim_clase
        cmp al, 10h
        je .sim_bar
        xor eax, eax
        ret
.sim_ff:
        mov eax, -1
        ret
.sim_id:
        mov eax, 0A0A08086h
        ret
.sim_clase:
        mov eax, 05800020h
        ret
.sim_bar:
        lea eax, [V(sim_prc)]
        sub eax, ITSSPRC
        and eax, 0FFFFFFF0h
        ret
.real:
%endif
        push edx
        pushfd
        cli
        or eax, 80000000h
        and al, 0FCh
        mov dx, 0CF8h
        out dx, eax
        mov dx, 0CFCh
        in eax, dx
        popfd
        pop edx
        ret
; pci_poner8: EAX = dispositivo y registro, CL = el octeto
pci_poner8:
%ifdef PRUEBA_P2SB
        cmp eax, P2SB + 0E1h
        jne .real
        mov [V(sim_oculto)], cl
        inc byte [V(sim_cambios)]
        ret
.real:
%endif
        push eax
        push ebx
        push edx
        pushfd
        cli
        mov ebx, eax
        or eax, 80000000h
        and al, 0FCh
        mov dx, 0CF8h
        out dx, eax
        mov edx, ebx
        and edx, 3
        add dx, 0CFCh
        mov al, cl
        out dx, al
        popfd
        pop edx
        pop ebx
        pop eax
        ret

; itss_abrir: quita el «clock gating» del 8254 en un PCH de Intel -> CF = 0 si
; el bit estaba puesto y se ha quitado. Apunta cada cosa que ve.
itss_abrir:
        pushad
        mov byte [V(it_oc)], 0          ; 1: hay que volver a ocultar el P2SB
        mov eax, LPC
        call pci_leer
        mov esi, eax
        DI t_i_lpc
        mov ecx, 8
        call hex
        cmp si, 8086h
        je .intel
        DI t_i_no_intel
        jmp .no
.intel: mov eax, P2SB
        call pci_leer
        mov esi, eax                    ; ESI = identificador del P2SB
        mov eax, P2SB + 0E0h
        call pci_leer
        mov edi, eax
        DI t_i_p2sb
        mov eax, esi
        mov ecx, 8
        call hex
        DI t_i_e0
        mov eax, edi
        call hex
        cmp esi, -1
        jne .visible
        mov eax, P2SB + 0E1h            ; oculto: se muestra (bit 8 de E0h a 0)
        xor ecx, ecx
        call pci_poner8
        mov byte [V(it_oc)], 1
        mov eax, P2SB
        call pci_leer
        mov esi, eax
        DI t_i_mostrado
        mov ecx, 8
        call hex
        cmp esi, -1
        jne .visible
        DI t_i_no_hay
        jmp .ocultar_no
.visible:
        cmp si, 8086h
        jne .raro
        mov eax, P2SB + 08h             ; clase 0580h: memoria (otra)
        call pci_leer
        shr eax, 16
        cmp eax, 0580h
        jne .raro
        mov eax, P2SB + 10h             ; SBREG_BAR (puede ser de 64 bits)
        call pci_leer
        mov ebx, eax
        mov eax, P2SB + 14h
        call pci_leer
        mov edi, eax
        call .ocultar
        test bl, 1                      ; (de E/S: no es)
        jnz .sin_bar
        mov eax, ebx
        and eax, 6
        cmp eax, 4                      ; ¿BAR de 64 bits?
        je .b64
        xor edi, edi
.b64:   and ebx, 0FFFFFFF0h
        DI t_i_bar
        mov eax, ebx
        mov edx, edi
        call hex64
        test edi, edi                   ; (por encima de 4 GB: no se llega)
        jnz .sin_bar
        test ebx, ebx
        jz .sin_bar
        lea eax, [ebx + ITSSPRC]
        call mapeada
        jc .nomap
        mov esi, [ebx + ITSSPRC]
        DI t_i_prc
        mov eax, esi
        mov ecx, 8
        call hex
        cmp esi, -1
        je .sin_bar
        test esi, 4                     ; 8254CGE
        jnz .quitar
        DI t_i_ya
        jmp .no
.quitar:
        and esi, ~4
        mov [ebx + ITSSPRC], esi
        mov esi, [ebx + ITSSPRC]
        DI t_i_quitado
        mov eax, esi
        mov ecx, 8
        call hex
        DI t_nl
        test esi, 4
        jnz .no_nl
        popad
        clc
        ret
.nomap: DI t_k_nomap
        jmp .no
.sin_bar:
        DI t_i_sin_bar
        jmp .no
.raro:  DI t_i_raro
.ocultar_no:
        call .ocultar
.no:    DI t_nl
.no_nl: popad
        stc
        ret
.ocultar:
        cmp byte [V(it_oc)], 0
        je .of
        push eax
        push ecx
        mov eax, P2SB + 0E1h
        mov ecx, 1
        call pci_poner8
        pop ecx
        pop eax
        mov byte [V(it_oc)], 0
.of:    ret

; mapeada: EAX = dirección -> CF = 1 si no está en las tablas de páginas del
; firmware. Sin paginación (lo corriente en un firmware de 32 bits) se llega a
; todo; con ella, se miran sus tablas (de dos niveles, o de tres con PAE).
mapeada:
        push ebx
        push ecx
        push edx
        mov edx, cr0
        test edx, edx
        jns .si                         ; sin paginación
        mov ebx, cr3
        mov edx, cr4
        test dl, 20h                    ; PAE
        jnz .pae
        and ebx, 0FFFFF000h
        mov ecx, eax
        shr ecx, 22
        mov ebx, [ebx + ecx * 4]
        test bl, 1
        jz .no
        test bl, 80h                    ; página de 4 MB
        jz .n2
        test dl, 10h                    ; (con PSE)
        jnz .si
.n2:    and ebx, 0FFFFF000h
        mov ecx, eax
        shr ecx, 12
        and ecx, 3FFh
        mov ebx, [ebx + ecx * 4]
        test bl, 1
        jz .no
        jmp .si
.pae:   and ebx, 0FFFFFFE0h
        mov ecx, eax
        shr ecx, 30
        cmp dword [ebx + ecx * 8 + 4], 0    ; (tablas por encima de 4 GB: no se miran)
        jne .si
        mov ebx, [ebx + ecx * 8]
        test bl, 1
        jz .no
        and ebx, 0FFFFF000h
        mov ecx, eax
        shr ecx, 21
        and ecx, 1FFh
        cmp dword [ebx + ecx * 8 + 4], 0
        mov ebx, [ebx + ecx * 8]
        jne .pae_alto
        test bl, 1
        jz .no
        test bl, 80h                    ; página de 2 MB
        jnz .si
        and ebx, 0FFFFF000h
        mov ecx, eax
        shr ecx, 12
        and ecx, 1FFh
        mov ebx, [ebx + ecx * 8]
        test bl, 1
        jz .no
        jmp .si
.pae_alto:                              ; (la parte alta lleva NX u otra cosa)
        test bl, 1
        jz .no
.si:    clc
        jmp .f
.no:    stc
.f:     pop edx
        pop ecx
        pop ebx
        ret

; acpi_tabla: EAX = firma -> EAX = la tabla de ACPI con esa firma (0: no hay)
acpi_tabla:
        push ebx
        push ecx
        push edx
        push esi
        mov edx, eax
        mov ebx, [V(rsdp)]
        test ebx, ebx
        jz .no
        cmp byte [ebx + 15], 2          ; ACPI 2.0: la XSDT (punteros de 64 bits)
        jb .rsdt
        cmp dword [ebx + 28], 0         ; (por encima de 4 GB: la RSDT)
        jne .rsdt
        mov esi, [ebx + 24]
        test esi, esi
        jz .rsdt
        mov ecx, [esi + 4]
        sub ecx, 36
        jbe .rsdt
        shr ecx, 3
        jz .rsdt
        add esi, 36
.x:     cmp dword [esi + 4], 0
        jne .xs
        mov eax, [esi]
        test eax, eax
        jz .xs
        cmp [eax], edx
        je .f
.xs:    add esi, 8
        dec ecx
        jnz .x
        jmp .no
.rsdt:  mov esi, [ebx + 16]
        test esi, esi
        jz .no
        mov ecx, [esi + 4]
        sub ecx, 36
        jbe .no
        shr ecx, 2
        jz .no
        add esi, 36
.r:     mov eax, [esi]
        test eax, eax
        jz .rs
        cmp [eax], edx
        je .f
.rs:    add esi, 4
        dec ecx
        jnz .r
.no:    xor eax, eax
.f:     pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ---- despachador: las peticiones de la BIOS (ECX = petición, EDX = argumento) ----
; (llega con las interrupciones cerradas; las abre mientras trabaja el firmware)
despachador:
        push ebx
        push esi
        push edi
        push ebp
        BASE_EBP
        cld
        call fwt_dar                    ; al firmware, los tics de su reloj que le tocan
        mov eax, [V(bios_base)]         ; con el ratón activado, en cada viaje
        cmp byte [eax + 285], 0         ; se le pregunta (T_RAT_ON)
        je .sin_raton
        call raton_sondear
.sin_raton:
        cmp ecx, 1                      ; dibujar la pantalla
        je .dibujar
        cmp ecx, 2                      ; esperar EDX segundos
        je .esperar
        cmp ecx, 3                      ; leer sectores (bloque P_*)
        je .leer
        cmp ecx, 4                      ; escribir sectores
        je .escribir
        cmp ecx, 5                      ; una tecla, si hay
        je .tecla
        cmp ecx, 6                      ; esperar EDX microsegundos
        je .microseg
        cmp ecx, 7                      ; reiniciar (EDX = 0) o apagar (2)
        je .reiniciar
        cmp ecx, 8                      ; modo de pantalla: texto (0) o VBE (1...)
        je .modo
        cmp ecx, 9                      ; lo apuntado por la BIOS, a HISPABIOS.LOG
        je .registro
        mov eax, -1                     ; (0: un viaje sin más)
        jmp .fin
.registro:
        ; EDX = desde dónde (desplazamiento en el registro de la BIOS); bit 16:
        ; guardarlo ya en la memoria USB (algo va mal)
        mov ebx, edx
        mov edi, [V(bios_base)]
        movzx ecx, word [edi + 194]     ; T_REG_N
        movzx esi, bx
        cmp ecx, REG_BIOS_TAM
        ja .reg_f
        sub ecx, esi
        jbe .reg_f
        lea esi, [edi + esi + REG_BIOS]
        mov dl, [V(callado)]
        mov byte [V(callado)], 1        ; (solo al registro: la pantalla es de DOS)
.reg_c: mov al, [esi]
        mov [V(buf_car)], al
        DI buf_car
        inc esi
        dec ecx
        jnz .reg_c
        mov [V(callado)], dl
        sti
        bt ebx, 16
        jnc .reg_p
        call registro_guardar
        jmp .reg_g
.reg_p: call registro_paso
.reg_g: cli
.reg_f: xor eax, eax
        jmp .fin
.modo:
        call modo_poner
        xor eax, eax
        jmp .fin
.dibujar:
        call pantalla_dibujar
        xor eax, eax
        jmp .fin
.esperar:
        imul edx, edx, 1000000
.microseg:
        mov eax, [V(bootserv)]
        sti
        FW [eax + BS_STALL], edx
        cli
        xor eax, eax
        jmp .fin
.leer:
.escribir:
        mov ebx, ecx                    ; 3 = leer, 4 = escribir
        mov edx, [V(bios_base)]         ; EDX = la tabla de traspaso
        movzx eax, byte [edx + 168]     ; P_DISCO
        cmp al, [V(ndiscos)]
        jae .mal
        lea esi, [V(discos)]
        mov edi, [esi + eax * 8]        ; Block I/O
        mov esi, [esi + eax * 8 + 4]    ; MediaId
        movzx ecx, word [edx + 170]     ; P_N
        shl ecx, 9                      ; octetos
        lea eax, [edx + PASO]           ; el búfer de paso
        mov [V(dp_buf)], eax
        sti
        cmp ebx, 3
        jne .w
        ; ReadBlocks(This, MediaId, LBA (64 bits), BufferSize, Buffer)
        FW [edi + BIO_READ], edi, esi, [edx + 172], [edx + 176], ecx, [V(dp_buf)]
        jmp .rw
.w:     FW [edi + BIO_WRITE], edi, esi, [edx + 172], [edx + 176], ecx, [V(dp_buf)]
.rw:    cli
        test eax, eax
        jnz .mal
        xor eax, eax
        jmp .fin
.mal:   mov eax, 1
        jmp .fin
.tecla:
        mov eax, [V(bootserv)]          ; un momento para que el firmware
        sti                             ; atienda el teclado (USB)
        FW [eax + BS_STALL], 1000
        cli
        call tecla_esperada             ; (CheckEvent de WaitForKey)
        cmp byte [V(inex_est)], 1       ; el protocolo con el estado de Ctrl/Alt
        je .ex                          ; (SimpleTextInputEx): 1 = localizado y hay
        ja .plano                       ; 2 = no hay -> sin modificadores
        mov byte [V(inex_est)], 2       ; primera vez: localizarlo
        mov eax, [V(sistema)]
        mov ecx, [eax + ST_CONINH]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], ecx, &guid_inex, &coninex
        test eax, eax
        jnz .plano
        cmp dword [V(coninex)], 0
        je .plano
        mov byte [V(inex_est)], 1
.ex:
        sti
        mov ecx, [V(coninex)]
        FW [ecx + 4], ecx, &keydata     ; ReadKeyStrokeEx (tecla + Ctrl/Alt/...)
        cli
        test eax, eax
        jnz .sin_tecla
        mov eax, [V(keydata) + 4]       ; estado de las teclas modificadoras
        test eax, 80000000h             ; ¿válido?
        jz .devolver
        test eax, 0Ch                   ; ¿algún Ctrl?
        jz .devolver
        test eax, 30h                   ; ¿algún Alt?
        jz .devolver
        cmp word [V(keydata)], 08h      ; ¿Supr? (código de UEFI) -> reiniciar
        jne .no_supr
        xor edx, edx                    ; EfiResetCold
        jmp .reiniciar
.no_supr:
        mov ax, [V(keydata) + 2]        ; Ctrl+Alt+G: la pantalla, un cuarto de
        cmp ax, 7                       ; vuelta más (y queda guardado)
        je .giro
        or al, 20h
        cmp ax, 'g'
        jne .devolver
.giro:  call giro_cambiar
        jmp .sin_tecla
.devolver:
        mov eax, [V(bios_base)]
        inc dword [eax + 320]           ; T_TEC_N
        movzx eax, word [V(keydata)]    ; ScanCode (bajo)
        movzx edx, word [V(keydata) + 2]   ; UnicodeChar (alto)
        shl edx, 16
        or eax, edx
        jmp .fin
.plano:
        sti
        mov eax, [V(sistema)]
        mov ecx, [eax + ST_CONIN]
        FW [ecx + TXT_READKEY], ecx, &tecla_efi     ; (sin modificadores)
        cli
        test eax, eax
        jnz .sin_tecla
        mov eax, [V(bios_base)]
        inc dword [eax + 320]           ; T_TEC_N
        mov eax, [V(tecla_efi)]         ; ScanCode (bajo) y UnicodeChar (alto)
        jmp .fin
.sin_tecla:
        mov eax, -1
        jmp .fin
.reiniciar:
        mov eax, [V(sistema)]
        mov eax, [eax + ST_RUNTIME]
        FW [eax + RT_RESET], edx, 0, 0, 0   ; EfiResetCold = 0, EfiResetShutdown = 2
        mov eax, -1
.fin:   pop ebp
        pop edi
        pop esi
        pop ebx
        ret
