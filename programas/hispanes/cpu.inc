; =============================================================================
;  Hispa-Nes - el procesador: 6502 sin modo decimal (el de la consola)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del 6502 (juego de instrucciones,
;  modos de direccionamiento, banderas y ciclos de cada instrucción) y de la
;  consola (el mapa de memoria). Todas las instrucciones oficiales y las no
;  oficiales que usan los programas: NOP de varios tamaños, LAX, SAX, DCP,
;  ISB, SLO, RLA, SRE, RRA, ANC, ALR, AXS y el segundo SBC.
;
;    cpu_reiniciar   pone el procesador como al pulsar «reset»
;    cpu_correr      EDI=tiempo que se añade (en puntos de imagen: un ciclo
;                    del procesador son 3): ejecuta hasta gastarlo. Lo que
;                    sobra (o falta) queda en cpu_resto para la siguiente
;
;  Mientras corre:  BL=A  BH=acarreo (0/1)  CL=X  CH=Y  ESI=puntero a la
;  instrucción (el contador de programa es ESI-[pcb])  EDI=tiempo que queda
;  EDX=dirección  EAX, EBP=de paso. N y Z se guardan como «el último
;  resultado»: N es el bit 7 de f_n y Z vale cuando f_z es 0.
;
;  La memoria: pg[] tiene, para cada trozo de 8 KB, un puntero tal que
;  [pg+dirección] es el octeto (RAM, ROM del cartucho, RAM de la batería).
;  De 2000h a 5FFFh son los registros (leer_es y escr_es, en ppu.inc) y
;  escribir de 8000h en adelante es hablarle al cartucho (map_escribir).
;  El código se lee directamente con ESI: vale mientras lo que sigue en la
;  consola también siga en nuestra memoria (dentro de un banco, o en bancos
;  seguidos de la ROM, que es lo que hacen los programas).
; =============================================================================

%macro DESPACHA 0
%ifdef TRAZA
        jmp cpu_traza
%else
        movzx eax, byte [esi]
        inc esi
        jmp [cpu_tabla+eax*4]
%endif
%endmacro
; fin de una instrucción de %1 ciclos
%macro FIN 1
        sub edi, %1*3
        jle near cpu_fuera
        DESPACHA
%endmacro
; N y Z según el resultado %1
%macro NZ 1
        mov [f_n], %1
        mov [f_z], %1
%endmacro
; EDX=dirección -> AL (conserva EDX; gasta EBP)
%macro LEER 0
        cmp edx, 0x2000
        jae %%f
        mov ebp, edx
        and ebp, 0x7FF
        mov al, [ram+ebp]
        jmp short %%x
%%f:
        call leer_alto
%%x:
%endmacro
; AL -> la dirección EDX (conserva EDX; gasta EBP y EAX)
%macro ESCRIBIR 0
        cmp edx, 0x2000
        jae %%f
        mov ebp, edx
        and ebp, 0x7FF
        mov [ram+ebp], al
        jmp short %%x
%%f:
        call escribir_alto
%%x:
%endmacro
%macro APILA 1
        movzx ebp, byte [reg_s]
        mov [ram+0x100+ebp], %1
        dec byte [reg_s]
%endmacro
%macro DESAPILA 0
        inc byte [reg_s]
        movzx ebp, byte [reg_s]
        mov al, [ram+0x100+ebp]
%endmacro
; el contador de programa pasa a ser EDX
%macro SALTA 0
        mov eax, edx
        shr eax, 13
        mov eax, [pg+eax*4]
        mov [pcb], eax
        lea esi, [eax+edx]
%endmacro

; ---- modos de direccionamiento: dejan la dirección en EDX ----
; A_modo: para leer (los indexados gastan un ciclo más al cambiar de página)
; AW_modo: para escribir o leer y escribir (sin ese ciclo: ya va contado)
%macro A_zp 0
        movzx edx, byte [esi]
        inc esi
%endmacro
%macro A_zpx 0
        movzx edx, byte [esi]
        inc esi
        add dl, cl
%endmacro
%macro A_zpy 0
        movzx edx, byte [esi]
        inc esi
        add dl, ch
%endmacro
%macro A_abs 0
        movzx edx, word [esi]
        add esi, 2
%endmacro
%macro A_absx 0
        movzx edx, word [esi]
        add esi, 2
        add dl, cl
        jnc %%n
        inc dh
        sub edi, 3
%%n:
%endmacro
%macro A_absy 0
        movzx edx, word [esi]
        add esi, 2
        add dl, ch
        jnc %%n
        inc dh
        sub edi, 3
%%n:
%endmacro
%macro A_indx 0
        movzx eax, byte [esi]
        inc esi
        add al, cl
        movzx edx, byte [ram+eax]
        inc al
        mov dh, [ram+eax]
%endmacro
%macro A_indy 0
        movzx eax, byte [esi]
        inc esi
        movzx edx, byte [ram+eax]
        inc al
        mov dh, [ram+eax]
        add dl, ch
        jnc %%n
        inc dh
        sub edi, 3
%%n:
%endmacro
%define AW_zp A_zp
%define AW_zpx A_zpx
%define AW_zpy A_zpy
%define AW_abs A_abs
%define AW_indx A_indx
%macro AW_absx 0
        movzx edx, word [esi]
        add esi, 2
        add dl, cl
        adc dh, 0
%endmacro
%macro AW_absy 0
        movzx edx, word [esi]
        add esi, 2
        add dl, ch
        adc dh, 0
%endmacro
%macro AW_indy 0
        movzx eax, byte [esi]
        inc esi
        movzx edx, byte [ram+eax]
        inc al
        mov dh, [ram+eax]
        add dl, ch
        adc dh, 0
%endmacro
; leer y escribir el operando (en página cero, directo)
%macro R_zp 0
        mov al, [ram+edx]
%endmacro
%define R_zpx R_zp
%define R_zpy R_zp
%define R_abs LEER
%define R_absx LEER
%define R_absy LEER
%define R_indx LEER
%define R_indy LEER
%macro W_zp 0
        mov [ram+edx], al
%endmacro
%define W_zpx W_zp
%define W_zpy W_zp
%define W_abs ESCRIBIR
%define W_absx ESCRIBIR
%define W_absy ESCRIBIR
%define W_indx ESCRIBIR
%define W_indy ESCRIBIR
; las instrucciones que leen, cambian y escriben: el procesador escribe dos
; veces (primero lo que había). Los cartuchos con registro en serie cuentan
; solo la primera: de 8000h en adelante se escribe lo que había
%macro RM_alto 0
        LEER
        mov [rmw_ant], al
%endmacro
%macro WM_alto 0
        cmp edx, 0x8000
        jb %%w
        mov al, [rmw_ant]
%%w:
        ESCRIBIR
%endmacro
%define RM_zp R_zp
%define RM_zpx R_zp
%define RM_abs RM_alto
%define RM_absx RM_alto
%define RM_absy RM_alto
%define RM_indx RM_alto
%define RM_indy RM_alto
%define WM_zp W_zp
%define WM_zpx W_zp
%define WM_abs WM_alto
%define WM_absx WM_alto
%define WM_absy WM_alto
%define WM_indx WM_alto
%define WM_indy WM_alto

; ---- las operaciones: el operando está en AL ----
%macro M_ora 0
        or bl, al
        NZ bl
%endmacro
%macro M_and 0
        and bl, al
        NZ bl
%endmacro
%macro M_eor 0
        xor bl, al
        NZ bl
%endmacro
%macro M_adc 0
        shr bh, 1
        adc bl, al
        setc bh
        seto [f_v]
        NZ bl
%endmacro
%macro M_sbc 0
        not al
        shr bh, 1
        adc bl, al
        setc bh
        seto [f_v]
        NZ bl
%endmacro
%macro M_cmp 0
        mov ah, bl
        sub ah, al
        setnc bh
        NZ ah
%endmacro
%macro M_cpx 0
        mov ah, cl
        sub ah, al
        setnc bh
        NZ ah
%endmacro
%macro M_cpy 0
        mov ah, ch
        sub ah, al
        setnc bh
        NZ ah
%endmacro
%macro M_lda 0
        mov bl, al
        NZ al
%endmacro
%macro M_ldx 0
        mov cl, al
        NZ al
%endmacro
%macro M_ldy 0
        mov ch, al
        NZ al
%endmacro
%macro M_lax 0
        mov bl, al
        mov cl, al
        NZ al
%endmacro
%macro M_bit 0
        mov [f_n], al
        test al, 0x40
        setnz [f_v]
        and al, bl
        mov [f_z], al
%endmacro
%macro M_nop 0
%endmacro
; (las que cambian el operando: lo dejan en AL para escribirlo)
%macro M_asl 0
        shl al, 1
        setc bh
        NZ al
%endmacro
%macro M_lsr 0
        shr al, 1
        setc bh
        NZ al
%endmacro
%macro M_rol 0
        shr bh, 1
        rcl al, 1
        setc bh
        NZ al
%endmacro
%macro M_ror 0
        shr bh, 1
        rcr al, 1
        setc bh
        NZ al
%endmacro
%macro M_inc 0
        inc al
        NZ al
%endmacro
%macro M_dec 0
        dec al
        NZ al
%endmacro
%macro M_slo 0
        shl al, 1
        setc bh
        or bl, al
        NZ bl
%endmacro
%macro M_rla 0
        shr bh, 1
        rcl al, 1
        setc bh
        and bl, al
        NZ bl
%endmacro
%macro M_sre 0
        shr al, 1
        setc bh
        xor bl, al
        NZ bl
%endmacro
%macro M_rra 0
        shr bh, 1
        rcr al, 1
        adc bl, al
        setc bh
        seto [f_v]
        NZ bl
%endmacro
%macro M_dcp 0
        dec al
        mov ah, bl
        sub ah, al
        setnc bh
        NZ ah
%endmacro
%macro M_isb 0
        inc al
        mov ah, al
        not ah
        shr bh, 1
        adc bl, ah
        setc bh
        seto [f_v]
        NZ bl
%endmacro

; ---- plantillas ----
%macro OP 1
%assign %%n %1
%xdefine hay_%[%%n] 1
        align 4
op_%[%%n]:
%endmacro
; código, operación, modo, ciclos: lee el operando y opera
%macro OPL 4
        OP %1
%ifidn %3, imm
        mov al, [esi]
        inc esi
%else
        A_%3
        R_%3
%endif
        M_%2
        FIN %4
%endmacro
; lee, cambia y escribe
%macro OPM 4
        OP %1
        AW_%3
        RM_%3
        M_%2
        WM_%3
        FIN %4
%endmacro
; guarda el registro %2
%macro OPS 4
        OP %1
        AW_%3
        mov al, %2
        W_%3
        FIN %4
%endmacro
; salta si... (%2 %3: la comparación; %4: el salto de x86 que NO toma la rama)
%macro OPB 4
        OP %1
        %2 %3
        %4 near rama_no
        jmp rama_si
%endmacro
; las ocho formas de ORA, AND, EOR, ADC, LDA, CMP y SBC: base y operación
%macro GRUPO 2
        OPL %1+0x09, %2, imm, 2
        OPL %1+0x05, %2, zp, 3
        OPL %1+0x15, %2, zpx, 4
        OPL %1+0x0D, %2, abs, 4
        OPL %1+0x1D, %2, absx, 4
        OPL %1+0x19, %2, absy, 4
        OPL %1+0x01, %2, indx, 6
        OPL %1+0x11, %2, indy, 5
%endmacro
; las siete formas de las no oficiales que cambian la memoria
%macro GRUPOM 2
        OPM %1+0x07, %2, zp, 5
        OPM %1+0x17, %2, zpx, 6
        OPM %1+0x0F, %2, abs, 6
        OPM %1+0x1F, %2, absx, 7
        OPM %1+0x1B, %2, absy, 7
        OPM %1+0x03, %2, indx, 8
        OPM %1+0x13, %2, indy, 8
%endmacro
; ASL, LSR, ROL, ROR: base (06h, 46h, 26h, 66h) y operación
%macro GRUPOD 2
        OPM %1, %2, zp, 5
        OPM %1+0x10, %2, zpx, 6
        OPM %1+0x08, %2, abs, 6
        OPM %1+0x18, %2, absx, 7
        OP %1+0x04
        mov al, bl
        M_%2
        mov bl, al
        FIN 2
%endmacro

; =============================================================================
        align 4
; pone el procesador como al pulsar «reset»
cpu_reiniciar:
        pushad
        mov dword [reg_a], 0        ; A, X, Y y S
        mov byte [reg_s], 0xFD
        mov byte [f_i], 4
        mov byte [f_c], 0
        mov byte [f_v], 0
        mov byte [f_d], 0
        mov byte [f_n], 0
        mov byte [f_z], 1
        mov byte [nmi_pend], 0
        mov byte [irq_linea], 0
        mov dword [cpu_resto], 0
        mov dword [cpu_aparte], 0
        mov byte [cpu_ap_fase], 0
        mov eax, [pg+7*4]
        movzx edx, word [eax+0xFFFC]
        mov [reg_pc], edx
        popad
        ret

; EDI=tiempo que se añade
cpu_correr:
        pushad
        add edi, [cpu_resto]
        mov bl, [reg_a]
        mov bh, [f_c]
        mov cl, [reg_x]
        mov ch, [reg_y]
        mov edx, [reg_pc]
        SALTA
        jmp cpu_reint

cpu_fuera:
        cmp byte [cpu_ap_fase], 0   ; un registro ha pedido que se miren las
        je .fin                     ; interrupciones (ha apartado el tiempo
        cmp byte [cpu_ap_fase], 1   ; que quedaba): se ejecuta una instrucción
        jne .ya                     ; más, como en la consola, y se miran
        mov byte [cpu_ap_fase], 2
        add [cpu_aparte], edi
        dec dword [cpu_aparte]
        mov edi, 1
        DESPACHA
.ya:
        mov byte [cpu_ap_fase], 0
        add edi, [cpu_aparte]
        mov dword [cpu_aparte], 0
        jmp cpu_reint
.fin:
        mov [cpu_resto], edi
        mov [reg_a], bl
        mov [f_c], bh
        mov [reg_x], cl
        mov [reg_y], ch
        sub esi, [pcb]
        and esi, 0xFFFF
        mov [reg_pc], esi
        popad
        ret

; tras cambiar la bandera I, y al empezar: ¿hay una interrupción pendiente?
        align 4
cpu_reint:
        test edi, edi
        jle cpu_fuera
        cmp byte [nmi_pend], 0
        jne .nmi
        cmp byte [irq_linea], 0
        je .sig
        cmp byte [f_i], 0
        jne .sig
        mov edx, 0xFFFE
        jmp .int
.nmi:
        mov byte [nmi_pend], 0
        mov edx, 0xFFFA
.int:
        mov eax, esi
        sub eax, [pcb]
        APILA ah
        APILA al
        call p_compone
        APILA al
        mov byte [f_i], 4
        mov eax, [pg+7*4]
        movzx edx, word [eax+edx]
        SALTA
        sub edi, 7*3
.sig:
        DESPACHA

%ifdef TRAZA
; (pruebas) antes de cada instrucción se llama a [traza_fn] con el estado
; guardado en reg_*
cpu_traza:
        mov [reg_a], bl
        mov [f_c], bh
        mov [reg_x], cl
        mov [reg_y], ch
        mov eax, esi
        sub eax, [pcb]
        mov [reg_pc], eax
        mov [cpu_resto], edi
        pushad
        call [traza_fn]
        popad
        movzx eax, byte [esi]
        inc esi
        jmp [cpu_tabla+eax*4]
%endif

; EDX=dirección (de 2000h en adelante) -> AL
leer_alto:
        cmp edx, 0x6000
        jb leer_es
        mov ebp, edx
        shr ebp, 13
        mov ebp, [pg+ebp*4]
        mov al, [ebp+edx]
        ret

; AL -> EDX (de 2000h en adelante)
escribir_alto:
        cmp edx, 0x6000
        jb escr_es
        cmp edx, 0x8000
        jae .mapa
        mov ebp, [pg+3*4]
        mov [ebp+edx], al
        mov byte [sram_sucia], 1
        ret
.mapa:
        mov ebp, esi                ; el cartucho puede cambiar de banco el
        sub ebp, [pcb]              ; código que se está ejecutando
        mov [reg_pc], ebp
        call [map_escribir]
        mov ebp, [reg_pc]
        mov esi, ebp
        shr esi, 13
        mov esi, [pg+esi*4]
        mov [pcb], esi
        add esi, ebp
        ret

; -> AL = el registro P (sin la bandera B)
p_compone:
        mov al, [f_n]
        and al, 0x80
        cmp byte [f_z], 0
        jne .z
        or al, 0x02
.z:
        or al, bh
        mov ah, [f_v]
        shl ah, 6
        or al, ah
        or al, [f_i]
        or al, [f_d]
        or al, 0x20
        ret
; AL = P: lo reparte en las banderas
p_reparte:
        mov bh, al
        and bh, 1
        mov [f_n], al
        mov ah, al
        not ah
        and ah, 0x02
        mov [f_z], ah
        test al, 0x40
        setnz [f_v]
        mov ah, al
        and ah, 0x04
        mov [f_i], ah
        and al, 0x08
        mov [f_d], al
        ret

; ---- las ramas ----
        align 4
rama_no:
        inc esi
        FIN 2
        align 4
rama_si:
        movsx eax, byte [esi]
        inc esi
        mov edx, esi
        sub edx, [pcb]
        lea ebp, [edx+eax]
        xor edx, ebp
        add esi, eax
        test dh, dh
        jz .misma
        sub edi, 3                  ; a otra página: un ciclo más
.misma:
        FIN 3

        OPB 0x10, test, {byte [f_n], 0x80}, jnz     ; BPL
        OPB 0x30, test, {byte [f_n], 0x80}, jz      ; BMI
        OPB 0x50, cmp, {byte [f_v], 0}, jne         ; BVC
        OPB 0x70, cmp, {byte [f_v], 0}, je          ; BVS
        OPB 0x90, test, {bh, bh}, jnz               ; BCC
        OPB 0xB0, test, {bh, bh}, jz                ; BCS
        OPB 0xD0, cmp, {byte [f_z], 0}, je          ; BNE
        OPB 0xF0, cmp, {byte [f_z], 0}, jne         ; BEQ

; ---- las oficiales ----
        GRUPO 0x00, ora
        GRUPO 0x20, and
        GRUPO 0x40, eor
        GRUPO 0x60, adc
        GRUPO 0xA0, lda
        GRUPO 0xC0, cmp
        GRUPO 0xE0, sbc
        OPL 0xEB, sbc, imm, 2       ; (no oficial: otro SBC)

        OPS 0x85, bl, zp, 3         ; STA
        OPS 0x95, bl, zpx, 4
        OPS 0x8D, bl, abs, 4
        OPS 0x9D, bl, absx, 5
        OPS 0x99, bl, absy, 5
        OPS 0x81, bl, indx, 6
        OPS 0x91, bl, indy, 6
        OPS 0x86, cl, zp, 3         ; STX
        OPS 0x96, cl, zpy, 4
        OPS 0x8E, cl, abs, 4
        OPS 0x84, ch, zp, 3         ; STY
        OPS 0x94, ch, zpx, 4
        OPS 0x8C, ch, abs, 4

        OPL 0xA2, ldx, imm, 2
        OPL 0xA6, ldx, zp, 3
        OPL 0xB6, ldx, zpy, 4
        OPL 0xAE, ldx, abs, 4
        OPL 0xBE, ldx, absy, 4
        OPL 0xA0, ldy, imm, 2
        OPL 0xA4, ldy, zp, 3
        OPL 0xB4, ldy, zpx, 4
        OPL 0xAC, ldy, abs, 4
        OPL 0xBC, ldy, absx, 4
        OPL 0xE0, cpx, imm, 2
        OPL 0xE4, cpx, zp, 3
        OPL 0xEC, cpx, abs, 4
        OPL 0xC0, cpy, imm, 2
        OPL 0xC4, cpy, zp, 3
        OPL 0xCC, cpy, abs, 4
        OPL 0x24, bit, zp, 3
        OPL 0x2C, bit, abs, 4

        GRUPOD 0x06, asl
        GRUPOD 0x46, lsr
        GRUPOD 0x26, rol
        GRUPOD 0x66, ror
        OPM 0xE6, inc, zp, 5
        OPM 0xF6, inc, zpx, 6
        OPM 0xEE, inc, abs, 6
        OPM 0xFE, inc, absx, 7
        OPM 0xC6, dec, zp, 5
        OPM 0xD6, dec, zpx, 6
        OPM 0xCE, dec, abs, 6
        OPM 0xDE, dec, absx, 7

        OP 0x4C                     ; JMP
        movzx edx, word [esi]
        SALTA
        FIN 3
        OP 0x6C                     ; JMP (indirecto): el octeto alto se lee
        movzx edx, word [esi]       ; de la misma página
        LEER
        mov [rmw_ant], al
        inc dl
        LEER
        movzx edx, al
        shl edx, 8
        mov dl, [rmw_ant]
        SALTA
        FIN 5
        OP 0x20                     ; JSR
        movzx edx, word [esi]
        lea eax, [esi+1]
        sub eax, [pcb]
        APILA ah
        APILA al
        SALTA
        FIN 6
        OP 0x60                     ; RTS
        DESAPILA
        movzx edx, al
        DESAPILA
        mov dh, al
        inc edx
        and edx, 0xFFFF
        SALTA
        FIN 6
        OP 0x40                     ; RTI
        DESAPILA
        call p_reparte
        DESAPILA
        movzx edx, al
        DESAPILA
        mov dh, al
        SALTA
        sub edi, 6*3
        jmp cpu_reint
        OP 0x00                     ; BRK
        lea eax, [esi+1]
        sub eax, [pcb]
        APILA ah
        APILA al
        call p_compone
        or al, 0x10
        APILA al
        mov byte [f_i], 4
        mov eax, [pg+7*4]
        movzx edx, word [eax+0xFFFE]
        SALTA
        FIN 7

        OP 0x48                     ; PHA
        APILA bl
        FIN 3
        OP 0x08                     ; PHP
        call p_compone
        or al, 0x10
        APILA al
        FIN 3
        OP 0x68                     ; PLA
        DESAPILA
        mov bl, al
        NZ al
        FIN 4
        OP 0x28                     ; PLP
        DESAPILA
        call p_reparte
        sub edi, 4*3
        jmp cpu_reint

        OP 0xAA                     ; TAX
        mov cl, bl
        NZ bl
        FIN 2
        OP 0xA8                     ; TAY
        mov ch, bl
        NZ bl
        FIN 2
        OP 0x8A                     ; TXA
        mov bl, cl
        NZ bl
        FIN 2
        OP 0x98                     ; TYA
        mov bl, ch
        NZ bl
        FIN 2
        OP 0xBA                     ; TSX
        mov cl, [reg_s]
        NZ cl
        FIN 2
        OP 0x9A                     ; TXS
        mov [reg_s], cl
        FIN 2
        OP 0xE8                     ; INX
        inc cl
        NZ cl
        FIN 2
        OP 0xC8                     ; INY
        inc ch
        NZ ch
        FIN 2
        OP 0xCA                     ; DEX
        dec cl
        NZ cl
        FIN 2
        OP 0x88                     ; DEY
        dec ch
        NZ ch
        FIN 2
        OP 0x18                     ; CLC
        xor bh, bh
        FIN 2
        OP 0x38                     ; SEC
        mov bh, 1
        FIN 2
        OP 0x58                     ; CLI
        mov byte [f_i], 0
        sub edi, 2*3
        jmp cpu_reint
        OP 0x78                     ; SEI
        mov byte [f_i], 4
        FIN 2
        OP 0xB8                     ; CLV
        mov byte [f_v], 0
        FIN 2
        OP 0xD8                     ; CLD
        mov byte [f_d], 0
        FIN 2
        OP 0xF8                     ; SED
        mov byte [f_d], 8
        FIN 2
        OP 0xEA                     ; NOP
        FIN 2

; ---- las no oficiales ----
        GRUPOM 0x00, slo
        GRUPOM 0x20, rla
        GRUPOM 0x40, sre
        GRUPOM 0x60, rra
        GRUPOM 0xC0, dcp
        GRUPOM 0xE0, isb
        OPL 0xA7, lax, zp, 3
        OPL 0xB7, lax, zpy, 4
        OPL 0xAF, lax, abs, 4
        OPL 0xBF, lax, absy, 4
        OPL 0xA3, lax, indx, 6
        OPL 0xB3, lax, indy, 5
%macro OPSAX 3
        OP %1
        AW_%2
        mov al, bl
        and al, cl
        W_%2
        FIN %3
%endmacro
        OPSAX 0x87, zp, 3
        OPSAX 0x97, zpy, 4
        OPSAX 0x8F, abs, 4
        OPSAX 0x83, indx, 6
%macro NOPS 3-*                     ; modo, ciclos, códigos...
%define %%modo %1
%define %%cic %2
%rotate 2
%rep %0 - 2
        OPL %1, nop, %%modo, %%cic
%rotate 1
%endrep
%endmacro
        NOPS imm, 2, 0x80, 0x82, 0x89, 0xC2, 0xE2
        NOPS zp, 3, 0x04, 0x44, 0x64
        NOPS zpx, 4, 0x14, 0x34, 0x54, 0x74, 0xD4, 0xF4
        NOPS abs, 4, 0x0C
        NOPS absx, 4, 0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC
        OP 0x0B                     ; ANC: AND y el acarreo como el bit 7
        mov al, [esi]
        inc esi
        and bl, al
        NZ bl
        mov bh, bl
        shr bh, 7
        FIN 2
op_anc2 equ op_11
        OP 0x4B                     ; ALR: AND y LSR
        mov al, [esi]
        inc esi
        and bl, al
        shr bl, 1
        setc bh
        NZ bl
        FIN 2
        OP 0xCB                     ; AXS: X = (A y X) - operando
        mov al, [esi]
        inc esi
        and cl, bl
        sub cl, al
        setnc bh
        NZ cl
        FIN 2
; lo demás (y los NOP de un octeto no oficiales): no hace nada
        align 4
op_nada:
        FIN 2

        align 4
cpu_tabla:
%assign i 0
%rep 256
%ifdef hay_%[i]
        dd op_%[i]
%elif i = 0x2B
        dd op_anc2
%else
        dd op_nada
%endif
%assign i i + 1
%endrep
