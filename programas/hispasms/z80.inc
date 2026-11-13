; =============================================================================
;  Hispa-System - el procesador: Z80 (el de la consola, a 3,58 MHz)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del Z80 (el manual del fabricante
;  del procesador: instrucciones, banderas y ciclos) y los documentos públicos
;  sobre lo que hace sin estar documentado: las banderas 3 y 5 (copias de
;  bits del resultado, o del registro oculto «MEMPTR» en BIT n,(HL)), IX e
;  IY partidos en dos mitades (IXH, IXL, IYH, IYL), SLL, las instrucciones
;  DDCB/FDCB que además dejan el resultado en un registro, las ED que
;  repiten a otras (NEG, RETN, IM) y IN F,(C) / OUT (C),0.
;
;    z80_tablas      (una vez) las tablas de banderas
;    z80_reiniciar   el procesador como al encender
;    z80_correr      EDI=ciclos que se añaden: ejecuta hasta gastarlos. Lo
;                    que sobra (o falta) queda en z_resto para la siguiente
;    irq_linea       (lo pone la imagen) la patilla de interrupción: 1 mientras
;                    el VDP la pide (IM 1: RST 38h; IM 2: el vector de I)
;    nmi_pend        1: una NMI (el botón de pausa) antes de la siguiente
;    z_in, z_out     punteros a las rutinas de los puertos: EDX=puerto (16
;                    bits) -> AL, o AL al puerto. Conservan todo menos EAX y
;                    EBP. Si un puerto cambia algo que pueda pedir una
;                    interrupción, pone z_mira a 1
;
;  Mientras corre:  ESI=contador de programa (16 bits)  EDI=ciclos que quedan
;  EAX, EBX, ECX, EDX, EBP=de paso. Los registros del Z80 están en memoria
;  (datos.inc): z_f, z_a, z_c, z_b... en parejas (z_af = F y A, etc.).
;
;  La memoria se ve por trozos de 1 KB: pg_lee[n] y pg_esc[n] son punteros
;  tales que [pg_lee+dirección] es el octeto. Si pg_esc es 0, escribir ahí
;  lo hace map_escribe (mapas.inc: los registros del cartucho y la ROM, que
;  no se escribe). Las banderas salen casi todas de las del propio x86
;  (LAHF: S, Z, H y C están en el mismo sitio que en el Z80).
; =============================================================================

F_S          equ 0x80
F_Z          equ 0x40
F_Y          equ 0x20
F_H          equ 0x10
F_X          equ 0x08
F_P          equ 0x04
F_N          equ 0x02
F_C          equ 0x01

; los registros por su número en las instrucciones (0 B, 1 C, 2 D, 3 E, 4 H,
; 5 L, 7 A) y, con el prefijo DD o FD, H y L son las mitades de IX o IY
%define XY_0 z_l
%define XY_1 z_ixl
%define XY_2 z_iyl
%define XH_0 z_h
%define XH_1 z_ixh
%define XH_2 z_iyh
%define XL_0 z_l
%define XL_1 z_ixl
%define XL_2 z_iyl

%macro DEFR8 3                      ; nombre, número, prefijo
 %if %2 == 0
  %xdefine %1 z_b
 %elif %2 == 1
  %xdefine %1 z_c
 %elif %2 == 2
  %xdefine %1 z_d
 %elif %2 == 3
  %xdefine %1 z_e
 %elif %2 == 4
  %xdefine %1 XH_%3
 %elif %2 == 5
  %xdefine %1 XL_%3
 %elif %2 == 7
  %xdefine %1 z_a
 %else
  %error "registro 6"
 %endif
%endmacro

; ---- la memoria ----
; un octeto del programa -> EAX; el contador avanza (ESI puede pasar de FFFFh
; un momento: pg_lee tiene una entrada 64, la 0 otra vez)
%macro FETCH8 0
        mov ebp, esi
        shr ebp, 10
        mov ebp, [pg_lee+ebp*4]
        movzx eax, byte [ebp+esi]
        inc esi
%endmacro
; dos octetos del programa -> EDX (gasta EAX)
%macro FETCH16 0
        FETCH8
        mov edx, eax
        FETCH8
        shl eax, 8
        or edx, eax
%endmacro
%macro DESPACHA 0
%ifdef TRAZA
        jmp z_traza
%else
        FETCH8
        inc byte [z_r]
        jmp [tab_op+eax*4]
%endif
%endmacro
; fin de una instrucción de %1 ciclos
%macro FIN 1
        sub edi, %1
        jle near z_fuera
        DESPACHA
%endmacro
; EDX=dirección (16 bits) -> AL (conserva EDX; gasta EBP)
%macro RD 0
        mov ebp, edx
        shr ebp, 10
        mov ebp, [pg_lee+ebp*4]
        mov al, [ebp+edx]
%endmacro
; AL -> la dirección EDX (conserva todo menos EBP)
%macro WR 0
        mov ebp, edx
        shr ebp, 10
        mov ebp, [pg_esc+ebp*4]
        test ebp, ebp
        jz %%e
        mov [ebp+edx], al
        jmp short %%x
%%e:
        call map_escribe
%%x:
%endmacro
; EDX=dirección -> CX la palabra (gasta EAX, EBP; EDX queda en la siguiente)
%macro RD16 0
        RD
        movzx ecx, al
        inc edx
        and edx, 0xFFFF
        RD
        mov ch, al
%endmacro
; CX -> la dirección EDX (gasta EAX, EBP; EDX queda en la siguiente)
%macro WR16 0
        mov al, cl
        WR
        inc edx
        and edx, 0xFFFF
        mov al, ch
        WR
%endmacro
; apila CX (gasta EAX, EDX, EBP)
%macro PUSHW 0
        movzx edx, word [z_sp]
        dec edx
        and edx, 0xFFFF
        mov al, ch
        WR
        dec edx
        and edx, 0xFFFF
        mov al, cl
        WR
        mov [z_sp], dx
%endmacro
; desapila -> ECX (gasta EAX, EDX, EBP)
%macro POPW 0
        movzx edx, word [z_sp]
        RD16
        inc edx
        mov [z_sp], dx
%endmacro
; (HL), o (IX+d) / (IY+d) con el prefijo: la dirección -> EDX
%macro DIR_M 1
 %if %1 == 0
        movzx edx, word [z_l]
 %else
        FETCH8
        movsx eax, al
        movzx edx, word [XY_%1]
        add edx, eax
        and edx, 0xFFFF
        mov [z_wz], dx
 %endif
%endmacro
; el contador de programa pasa a ser EDX
%macro SALTA 0
        mov esi, edx
        mov [z_wz], dx
%endmacro

; la etiqueta de la instrucción %2 con el prefijo %1 (0 ninguno, 1 DD, 2 FD)
%macro OP 2
 %assign _n %2
zo%1_%[_n]:
 %xdefine HAY_%1_%[_n] 1
%endmacro

; =============================================================================
;  Las tablas de banderas
; =============================================================================
z80_tablas:
        pushad
        xor ebx, ebx
.t:
        mov al, bl
        and al, F_S | F_Y | F_X
        test bl, bl
        jnz .nz
        or al, F_Z
.nz:
        mov [tab_sz53+ebx], al
        test bl, bl                 ; paridad par -> P
        jnp .np
        or al, F_P
.np:
        mov [tab_sz53p+ebx], al
        inc ebx
        cmp ebx, 256
        jb .t
        popad
        ret

z80_reiniciar:
        pushad
        mov edi, z_regs
        mov ecx, z_regs_fin - z_regs
        xor eax, eax
        rep stosb
        mov word [z_af], 0xFFFF
        mov word [z_sp], 0xDFF0
        mov dword [z_resto], 0
        mov byte [z_apfase], 0
        popad
        ret

; EDI=ciclos que se añaden
z80_correr:
        pushad
        add edi, [z_resto]
        mov esi, [z_pc]
        jmp z_reint

z_fuera:
        cmp byte [z_apfase], 0      ; tras EI: ya ha ido la instrucción de
        je .fin                     ; después; ahora se miran las interrupciones
        mov byte [z_apfase], 0
        add edi, [z_aparte]
        dec edi
        jmp z_reint
.fin:
        mov [z_resto], edi
        and esi, 0xFFFF
        mov [z_pc], esi
        popad
        ret

; ¿hay una interrupción? (al empezar, tras un puerto que la puede pedir...)
        align 4
z_reint:
        test edi, edi
        jle z_fuera
        cmp byte [nmi_pend], 0
        jne .nmi
        cmp byte [irq_linea], 0
        je .sig
        cmp byte [z_iff1], 0
        je .sig
        xor eax, eax
        mov [z_iff1], al
        mov [z_iff2], al
        call .sale_halt
        inc byte [z_r]
        mov ecx, esi
        PUSHW
        cmp byte [z_im], 2
        je .im2
        mov edx, 0x38               ; IM 0 (el bus da FFh: RST 38h) e IM 1
        SALTA
        sub edi, 13
        jmp z_reint
.im2:
        mov dh, [z_i]               ; el vector: I y FFh (lo que hay en el bus)
        mov dl, 0xFF
        movzx edx, dx
        RD16
        mov edx, ecx
        SALTA
        sub edi, 19
        jmp z_reint
.nmi:
        mov byte [nmi_pend], 0
        mov byte [z_iff1], 0
        call .sale_halt
        inc byte [z_r]
        mov ecx, esi
        PUSHW
        mov edx, 0x66
        SALTA
        sub edi, 11
        jmp z_reint
.sig:
        DESPACHA
.sale_halt:                         ; parado en HALT: sigue en la siguiente
        cmp byte [z_halt], 0
        je .sh
        mov byte [z_halt], 0
        inc esi
.sh:
        and esi, 0xFFFF
        ret

; un puerto ha cambiado algo de las interrupciones: se miran ya
z_mira_ya:
        mov byte [z_mira], 0
        jmp z_reint

; un prefijo DD o FD delante de una instrucción que no lo usa: cuatro ciclos
; y la instrucción tal cual (EAX = su código)
z_prefnada:
        sub edi, 4
        jmp [tab_op+eax*4]

; =============================================================================
;  Operaciones de 8 bits: CL = el otro operando, EBX = los ciclos
; =============================================================================
        align 4
alu_add:
        mov al, [z_a]
        add al, cl
.f:
        lahf
        seto dl
        mov [z_a], al
        and ah, F_S | F_Z | F_H | F_C   ; (LAHF deja el bit 1 siempre a 1)
.g:
        and al, F_Y | F_X
        or ah, al
        shl dl, 2
        or ah, dl
        mov [z_f], ah
        sub edi, ebx
        jle near z_fuera
        DESPACHA
alu_adc:
        mov al, [z_a]
        mov ah, [z_f]
        shr ah, 1
        adc al, cl
        jmp alu_add.f
alu_sub:
        mov al, [z_a]
        sub al, cl
.f:
        lahf
        seto dl
        mov [z_a], al
        and ah, F_S | F_Z | F_H | F_C
        or ah, F_N
        jmp alu_add.g
alu_sbc:
        mov al, [z_a]
        mov ah, [z_f]
        shr ah, 1
        sbb al, cl
        jmp alu_sub.f
alu_cp:
        mov al, [z_a]
        sub al, cl
        lahf
        seto dl
        and ah, F_S | F_Z | F_H | F_C
        or ah, F_N
        mov al, cl                  ; 3 y 5: del operando
        jmp alu_add.g
alu_and:
        mov al, [z_a]
        and al, cl
        mov [z_a], al
        movzx eax, al
        mov al, [tab_sz53p+eax]
        or al, F_H
        mov [z_f], al
        sub edi, ebx
        jle near z_fuera
        DESPACHA
alu_xor:
        mov al, [z_a]
        xor al, cl
        jmp alu_or.f
alu_or:
        mov al, [z_a]
        or al, cl
.f:
        mov [z_a], al
        movzx eax, al
        mov al, [tab_sz53p+eax]
        mov [z_f], al
        sub edi, ebx
        jle near z_fuera
        DESPACHA

%macro ALU_JMP 1                    ; la operación %1 (0-7), con CL y EBX
 %if %1 == 0
        jmp alu_add
 %elif %1 == 1
        jmp alu_adc
 %elif %1 == 2
        jmp alu_sub
 %elif %1 == 3
        jmp alu_sbc
 %elif %1 == 4
        jmp alu_and
 %elif %1 == 5
        jmp alu_xor
 %elif %1 == 6
        jmp alu_or
 %else
        jmp alu_cp
 %endif
%endmacro

; INC y DEC de 8 bits: AL -> AL, y las banderas (C se queda). Gasta ECX
z_inc8:
        inc al
        lahf
        seto cl
        and ah, F_S | F_Z | F_H     ; (LAHF deja el bit 1 siempre a 1)
        jmp z_dec8.f
z_dec8:
        dec al
        lahf
        seto cl
        and ah, F_S | F_Z | F_H
        or ah, F_N
.f:
        shl cl, 2
        or ah, cl
        mov cl, al
        and cl, F_Y | F_X
        or ah, cl
        mov cl, [z_f]
        and cl, F_C
        or ah, cl
        mov [z_f], ah
        ret

; ADD HL,rr: CX + DX -> CX, con las banderas H, N, C, 3 y 5
z_add16:
        lea eax, [ecx+1]
        mov [z_wz], ax
        add cl, dl
        adc ch, dh
        lahf
        and ah, F_H | F_C
        mov al, ch
        and al, F_Y | F_X
        or ah, al
        mov al, [z_f]
        and al, F_S | F_Z | F_P
        or ah, al
        mov [z_f], ah
        ret
; ADC HL,rr y SBC HL,rr: CX y DX -> HL, con todas las banderas
z_adc16:
        lea eax, [ecx+1]
        mov [z_wz], ax
        mov ah, [z_f]
        shr ah, 1
        adc cl, dl
        adc ch, dh
        lahf
        seto dl
        xor al, al
        jmp z_sbc16.f
z_sbc16:
        lea eax, [ecx+1]
        mov [z_wz], ax
        mov ah, [z_f]
        shr ah, 1
        sbb cl, dl
        sbb ch, dh
        lahf
        seto dl
        mov al, F_N
.f:
        and ah, F_S | F_H | F_C
        or ah, al
        shl dl, 2
        or ah, dl
        mov al, ch
        and al, F_Y | F_X
        or ah, al
        test cx, cx
        jnz .nz
        or ah, F_Z
.nz:
        mov [z_f], ah
        mov [z_l], cx
        ret

; giros y desplazamientos (CB): AL -> AL y las banderas; BL = cuál (0-7)
z_rot:
        movzx ebx, bl
        jmp [.t+ebx*4]
        align 4
.t      dd .rlc, .rrc, .rl, .rr, .sla, .sra, .sll, .srl
.rlc:
        rol al, 1
        jmp .f
.rrc:
        ror al, 1
        jmp .f
.rl:
        mov ah, [z_f]
        shr ah, 1
        rcl al, 1
        jmp .f
.rr:
        mov ah, [z_f]
        shr ah, 1
        rcr al, 1
        jmp .f
.sla:
        shl al, 1
        jmp .f
.sra:
        sar al, 1
        jmp .f
.sll:
        stc
        rcl al, 1
        jmp .f
.srl:
        shr al, 1
.f:
        setc ah
        movzx ebx, al
        or ah, [tab_sz53p+ebx]
        mov [z_f], ah
        ret

; BIT n: AL = el valor, BL = la máscara del bit; DL = lo que va a 3 y 5
z_bit:
        and al, bl
        mov ah, [z_f]
        and ah, F_C
        or ah, F_H
        and dl, F_Y | F_X
        or ah, dl
        test al, al
        jnz .uno
        or ah, F_Z | F_P
        jmp .x
.uno:
        and al, F_S                  ; solo el bit 7 da S
        or ah, al
.x:
        mov [z_f], ah
        ret

; =============================================================================
;  Las instrucciones sin prefijo (y las que cambian con DD y FD)
; =============================================================================
; ---- LD r,r' (40h-7Fh) ----
%macro LD_RR 2                      ; prefijo, código
 %assign _d ((%2) >> 3) & 7
 %assign _s (%2) & 7
 %if %2 == 0x76
 %elif _d == 6
  %undef RS
  DEFR8 RS, _s, 0
  OP %1, %2
        DIR_M %1
        mov al, [RS]
        WR
  %if %1 == 0
        FIN 7
  %else
        FIN 19
  %endif
 %elif _s == 6
  %undef RD_
  DEFR8 RD_, _d, 0
  OP %1, %2
        DIR_M %1
        RD
        mov [RD_], al
  %if %1 == 0
        FIN 7
  %else
        FIN 19
  %endif
 %elif %1 == 0 || _d == 4 || _d == 5 || _s == 4 || _s == 5
  %undef RS
  DEFR8 RS, _s, %1
  %undef RD_
  DEFR8 RD_, _d, %1
  OP %1, %2
        mov al, [RS]
        mov [RD_], al
  %if %1 == 0
        FIN 4
  %else
        FIN 8
  %endif
 %endif
%endmacro

; ---- ALU A,r (80h-BFh) ----
%macro ALU_R 2
 %assign _o ((%2) >> 3) & 7
 %assign _s (%2) & 7
 %if _s == 6
  OP %1, %2
        DIR_M %1
        RD
        mov cl, al
  %if %1 == 0
        mov ebx, 7
  %else
        mov ebx, 19
  %endif
        ALU_JMP _o
 %elif %1 == 0 || _s == 4 || _s == 5
  %undef RS
  DEFR8 RS, _s, %1
  OP %1, %2
        mov cl, [RS]
  %if %1 == 0
        mov ebx, 4
  %else
        mov ebx, 8
  %endif
        ALU_JMP _o
 %endif
%endmacro

; ---- INC r, DEC r, LD r,n (y los de (HL)) ----
%macro INCDEC_R 2
 %assign _d ((%2) >> 3) & 7
 %assign _o (%2) & 7                ; 4 INC, 5 DEC, 6 LD r,n
 %if _d == 6
  OP %1, %2
  %if _o == 6
   %if %1 == 0
        movzx edx, word [z_l]
   %else
        DIR_M %1
   %endif
        FETCH8
        WR
   %if %1 == 0
        FIN 10
   %else
        FIN 19
   %endif
  %else
        DIR_M %1
        RD
   %if _o == 4
        call z_inc8
   %else
        call z_dec8
   %endif
        WR
   %if %1 == 0
        FIN 11
   %else
        FIN 23
   %endif
  %endif
 %elif %1 == 0 || _d == 4 || _d == 5
  %undef RD_
  DEFR8 RD_, _d, %1
  OP %1, %2
  %if _o == 6
        FETCH8
        mov [RD_], al
   %if %1 == 0
        FIN 7
   %else
        FIN 11
   %endif
  %else
        mov al, [RD_]
   %if _o == 4
        call z_inc8
   %else
        call z_dec8
   %endif
        mov [RD_], al
   %if %1 == 0
        FIN 4
   %else
        FIN 8
   %endif
  %endif
 %endif
%endmacro

; ---- las que usan HL entero (con DD y FD, IX o IY) ----
%macro OPS_HL 1
 %if %1 == 0
  %assign _c 0
 %else
  %assign _c 4
 %endif
OP %1, 0x21                         ; LD HL,nn
        FETCH16
        mov [XY_%1], dx
        FIN 10 + _c
OP %1, 0x22                         ; LD (nn),HL
        FETCH16
        mov cx, [XY_%1]
        WR16
        mov [z_wz], dx
        FIN 16 + _c
OP %1, 0x2A                         ; LD HL,(nn)
        FETCH16
        RD16
        mov [XY_%1], cx
        mov [z_wz], dx
        FIN 16 + _c
OP %1, 0x23                         ; INC HL
        inc word [XY_%1]
        FIN 6 + _c
OP %1, 0x2B                         ; DEC HL
        dec word [XY_%1]
        FIN 6 + _c
OP %1, 0x09                         ; ADD HL,BC
        mov cx, [XY_%1]
        mov dx, [z_bc]
        call z_add16
        mov [XY_%1], cx
        FIN 11 + _c
OP %1, 0x19                         ; ADD HL,DE
        mov cx, [XY_%1]
        mov dx, [z_de]
        call z_add16
        mov [XY_%1], cx
        FIN 11 + _c
OP %1, 0x29                         ; ADD HL,HL
        mov cx, [XY_%1]
        mov dx, cx
        call z_add16
        mov [XY_%1], cx
        FIN 11 + _c
OP %1, 0x39                         ; ADD HL,SP
        mov cx, [XY_%1]
        mov dx, [z_sp]
        call z_add16
        mov [XY_%1], cx
        FIN 11 + _c
OP %1, 0xE1                         ; POP HL
        POPW
        mov [XY_%1], cx
        FIN 10 + _c
OP %1, 0xE5                         ; PUSH HL
        mov cx, [XY_%1]
        PUSHW
        FIN 11 + _c
OP %1, 0xE3                         ; EX (SP),HL
        movzx edx, word [z_sp]
        RD16
        mov bx, [XY_%1]
        mov [XY_%1], cx
        mov [z_wz], cx
        mov al, bh
        WR
        dec edx
        and edx, 0xFFFF
        mov al, bl
        WR
        FIN 19 + _c
OP %1, 0xE9                         ; JP (HL)
        movzx esi, word [XY_%1]
        FIN 4 + _c
OP %1, 0xF9                         ; LD SP,HL
        mov ax, [XY_%1]
        mov [z_sp], ax
        FIN 6 + _c
%endmacro

; ---- todas las que dependen del prefijo ----
%macro OPS_PREF 1
 %assign _op 0x40
 %rep 64
        LD_RR %1, _op
  %assign _op _op + 1
 %endrep
 %assign _op 0x80
 %rep 64
        ALU_R %1, _op
  %assign _op _op + 1
 %endrep
 %assign _op 0
 %rep 8
        INCDEC_R %1, _op + 4
        INCDEC_R %1, _op + 5
        INCDEC_R %1, _op + 6
  %assign _op _op + 8
 %endrep
        OPS_HL %1
%endmacro

        OPS_PREF 0
        OPS_PREF 1
        OPS_PREF 2

; ---- el resto, sin prefijo ----
OP 0, 0x00                          ; NOP
        FIN 4
OP 0, 0x01                          ; LD BC,nn
        FETCH16
        mov [z_bc], dx
        FIN 10
OP 0, 0x11
        FETCH16
        mov [z_de], dx
        FIN 10
OP 0, 0x31
        FETCH16
        mov [z_sp], dx
        FIN 10
OP 0, 0x02                          ; LD (BC),A
        movzx edx, word [z_bc]
        jmp z_st_a
OP 0, 0x12                          ; LD (DE),A
        movzx edx, word [z_de]
z_st_a:
        mov al, [z_a]
        WR
        inc edx
        mov ah, [z_a]
        mov al, dl
        mov [z_wz], ax
        FIN 7
OP 0, 0x0A                          ; LD A,(BC)
        movzx edx, word [z_bc]
        jmp z_ld_a
OP 0, 0x1A                          ; LD A,(DE)
        movzx edx, word [z_de]
z_ld_a:
        RD
        mov [z_a], al
        inc edx
        mov [z_wz], dx
        FIN 7
OP 0, 0x32                          ; LD (nn),A
        FETCH16
        mov al, [z_a]
        WR
        inc edx
        mov ah, [z_a]
        mov al, dl
        mov [z_wz], ax
        FIN 13
OP 0, 0x3A                          ; LD A,(nn)
        FETCH16
        RD
        mov [z_a], al
        inc edx
        mov [z_wz], dx
        FIN 13
OP 0, 0x03                          ; INC BC
        inc word [z_bc]
        FIN 6
OP 0, 0x13
        inc word [z_de]
        FIN 6
OP 0, 0x33
        inc word [z_sp]
        FIN 6
OP 0, 0x0B                          ; DEC BC
        dec word [z_bc]
        FIN 6
OP 0, 0x1B
        dec word [z_de]
        FIN 6
OP 0, 0x3B
        dec word [z_sp]
        FIN 6
OP 0, 0x07                          ; RLCA
        mov al, [z_a]
        rol al, 1
        jmp z_rota
OP 0, 0x0F                          ; RRCA
        mov al, [z_a]
        ror al, 1
        jmp z_rota
OP 0, 0x17                          ; RLA
        mov ah, [z_f]
        shr ah, 1
        mov al, [z_a]
        rcl al, 1
        jmp z_rota
OP 0, 0x1F                          ; RRA
        mov ah, [z_f]
        shr ah, 1
        mov al, [z_a]
        rcr al, 1
z_rota:
        setc ah
        mov [z_a], al
        and al, F_Y | F_X
        or ah, al
        mov al, [z_f]
        and al, F_S | F_Z | F_P
        or ah, al
        mov [z_f], ah
        FIN 4
OP 0, 0x08                          ; EX AF,AF'
        mov ax, [z_af]
        xchg ax, [z_af2]
        mov [z_af], ax
        FIN 4
OP 0, 0xD9                          ; EXX
        mov eax, [z_bc]             ; (BC y DE seguidos)
        xchg eax, [z_bc2]
        mov [z_bc], eax
        mov ax, [z_hl]
        xchg ax, [z_hl2]
        mov [z_hl], ax
        FIN 4
OP 0, 0xEB                          ; EX DE,HL
        mov ax, [z_de]
        xchg ax, [z_hl]
        mov [z_de], ax
        FIN 4
OP 0, 0x10                          ; DJNZ
        FETCH8
        dec byte [z_b]
        jz .no
        movsx eax, al
        add esi, eax
        and esi, 0xFFFF
        mov [z_wz], si
        FIN 13
.no:
        FIN 8
OP 0, 0x18                          ; JR
        FETCH8
z_jr:
        movsx eax, al
        add esi, eax
        and esi, 0xFFFF
        mov [z_wz], si
        FIN 12
%macro JR_CC 3                      ; código, bandera, salta si está (1) o no (0)
OP 0, %1
        FETCH8
        test byte [z_f], %2
 %if %3
        jnz z_jr
 %else
        jz z_jr
 %endif
        FIN 7
%endmacro
        JR_CC 0x20, F_Z, 0
        JR_CC 0x28, F_Z, 1
        JR_CC 0x30, F_C, 0
        JR_CC 0x38, F_C, 1
OP 0, 0x27                          ; DAA
        mov al, [z_a]
        mov ah, [z_f]
        xor cl, cl                  ; lo que se suma o se resta
        xor ch, ch                  ; el acarreo nuevo
        mov dl, al
        and dl, 0x0F
        test ah, F_H
        jnz .s6
        cmp dl, 9
        jbe .n6
.s6:
        or cl, 0x06
.n6:
        test ah, F_C
        jnz .s60
        cmp al, 0x99
        jbe .n60
.s60:
        or cl, 0x60
        mov ch, F_C
.n60:
        test ah, F_N
        jnz .resta
        cmp dl, 9                   ; H al sumar: (A & 0Fh) > 9
        seta dh
        add al, cl
        jmp .f
.resta:
        test ah, F_H                 ; H al restar: H y (A & 0Fh) < 6
        setnz dh
        cmp dl, 6
        setb dl
        and dh, dl
        sub al, cl
.f:
        mov [z_a], al
        movzx eax, al
        mov al, [tab_sz53p+eax]
        or al, ch
        mov ah, [z_f]
        and ah, F_N
        or al, ah
        shl dh, 4
        or al, dh
        mov [z_f], al
        FIN 4
OP 0, 0x2F                          ; CPL
        mov al, [z_a]
        not al
        mov [z_a], al
        and al, F_Y | F_X
        mov ah, [z_f]
        and ah, F_S | F_Z | F_P | F_C
        or ah, al
        or ah, F_H | F_N
        mov [z_f], ah
        FIN 4
OP 0, 0x37                          ; SCF
        mov al, [z_a]
        and al, F_Y | F_X
        mov ah, [z_f]
        and ah, F_S | F_Z | F_P
        or ah, al
        or ah, F_C
        mov [z_f], ah
        FIN 4
OP 0, 0x3F                          ; CCF
        mov al, [z_a]
        and al, F_Y | F_X
        mov ah, [z_f]
        mov dl, ah
        and dl, F_C
        shl dl, 4                   ; H = el acarreo de antes
        and ah, F_S | F_Z | F_P | F_C
        xor ah, F_C
        or ah, al
        or ah, dl
        mov [z_f], ah
        FIN 4
OP 0, 0x76                          ; HALT: se queda en ella hasta una interrupción
        mov byte [z_halt], 1
        dec esi
        mov eax, edi                ; gasta el tramo entero, de 4 en 4
        add eax, 3
        shr eax, 2
        add [z_r], al
        shl eax, 2
        sub edi, eax
        jmp z_fuera
; ---- saltos, llamadas y vueltas ----
%macro CC_SALTA 2                   ; el número de condición (0-7), etiqueta si se cumple
 %if %1 == 0
        test byte [z_f], F_Z
        jz %2
 %elif %1 == 1
        test byte [z_f], F_Z
        jnz %2
 %elif %1 == 2
        test byte [z_f], F_C
        jz %2
 %elif %1 == 3
        test byte [z_f], F_C
        jnz %2
 %elif %1 == 4
        test byte [z_f], F_P
        jz %2
 %elif %1 == 5
        test byte [z_f], F_P
        jnz %2
 %elif %1 == 6
        test byte [z_f], F_S
        jz %2
 %else
        test byte [z_f], F_S
        jnz %2
 %endif
%endmacro
OP 0, 0xC3                          ; JP nn
        FETCH16
z_jp:
        SALTA
        FIN 10
OP 0, 0xCD                          ; CALL nn
        FETCH16
z_call:
        mov ebx, edx
        mov ecx, esi
        and ecx, 0xFFFF
        PUSHW
        mov edx, ebx
        SALTA
        FIN 17
OP 0, 0xC9                          ; RET
z_ret:
        POPW
        mov edx, ecx
        SALTA
        FIN 10
%assign _c 0
%rep 8
OP 0, 0xC2 + _c * 8                 ; JP cc,nn
        FETCH16
        CC_SALTA _c, z_jp
        mov [z_wz], dx
        FIN 10
OP 0, 0xC4 + _c * 8                 ; CALL cc,nn
        FETCH16
        CC_SALTA _c, z_call
        mov [z_wz], dx
        FIN 10
OP 0, 0xC0 + _c * 8                 ; RET cc
        CC_SALTA _c, .si
        FIN 5
.si:
        sub edi, 1
        jmp z_ret
OP 0, 0xC7 + _c * 8                 ; RST
        mov ecx, esi
        and ecx, 0xFFFF
        PUSHW
        mov edx, _c * 8
        SALTA
        FIN 11
 %assign _c _c + 1
%endrep
OP 0, 0xC1                          ; POP BC
        POPW
        mov [z_bc], cx
        FIN 10
OP 0, 0xD1
        POPW
        mov [z_de], cx
        FIN 10
OP 0, 0xF1
        POPW
        mov [z_af], cx
        FIN 10
OP 0, 0xC5                          ; PUSH BC
        mov cx, [z_bc]
        PUSHW
        FIN 11
OP 0, 0xD5
        mov cx, [z_de]
        PUSHW
        FIN 11
OP 0, 0xF5
        mov cx, [z_af]
        PUSHW
        FIN 11
%assign _o 0
%rep 8
OP 0, 0xC6 + _o * 8                 ; ALU A,n
        FETCH8
        mov cl, al
        mov ebx, 7
        ALU_JMP _o
 %assign _o _o + 1
%endrep
OP 0, 0xD3                          ; OUT (n),A
        FETCH8
        mov dh, [z_a]
        mov dl, al
        inc al
        mov ah, dh
        mov [z_wz], ax
        mov al, dh
        movzx edx, dx
        call [z_out]
        sub edi, 11
        cmp byte [z_mira], 0
        jne z_mira_ya
        test edi, edi
        jle near z_fuera
        DESPACHA
OP 0, 0xDB                          ; IN A,(n)
        FETCH8
        mov dh, [z_a]
        mov dl, al
        movzx edx, dx
        lea eax, [edx+1]
        mov [z_wz], ax
        call [z_in]
        mov [z_a], al
        FIN 11
OP 0, 0xF3                          ; DI
        mov byte [z_iff1], 0
        mov byte [z_iff2], 0
        FIN 4
OP 0, 0xFB                          ; EI: las interrupciones, tras la siguiente
        mov byte [z_iff1], 1
        mov byte [z_iff2], 1
        sub edi, 4
        cmp byte [irq_linea], 0
        je .x
        cmp byte [z_apfase], 0
        jne .x
        mov [z_aparte], edi
        mov edi, 1
        mov byte [z_apfase], 1
        DESPACHA
.x:
        test edi, edi
        jle near z_fuera
        DESPACHA

; ---- los prefijos ----
OP 0, 0xDD
        FETCH8
        inc byte [z_r]
        jmp [tab_dd+eax*4]
OP 0, 0xFD
        FETCH8
        inc byte [z_r]
        jmp [tab_fd+eax*4]
OP 0, 0xCB
        FETCH8
        inc byte [z_r]
        jmp [tab_cb+eax*4]
OP 0, 0xED
        FETCH8
        inc byte [z_r]
        jmp [tab_ed+eax*4]
OP 1, 0xCB                          ; DDCB d op y FDCB d op
        DIR_M 1
        jmp z_xcb
OP 2, 0xCB
        DIR_M 2
; EDX = IX+d o IY+d; luego el código de la operación
z_xcb:
        FETCH8
        mov ecx, eax                ; CL = el código
        RD
        mov bl, cl
        shr bl, 6
        jnz .nrot
        mov bl, cl                  ; giro o desplazamiento
        shr bl, 3
        and bl, 7
        push ecx
        call z_rot
        pop ecx
        jmp .guarda
.nrot:
        push ecx
        shr ecx, 3                  ; CL = el número del bit
        and ecx, 7
        mov ah, 1
        shl ah, cl
        pop ecx
        cmp bl, 1
        jne .rs
        mov bl, ah                  ; BIT n,(IX+d): 3 y 5, de la dirección
        push edx
        mov dl, dh
        call z_bit
        pop edx
        FIN 20
.rs:
        cmp bl, 2                   ; RES o SET
        jne .set
        not ah
        and al, ah
        jmp .guarda
.set:
        or al, ah
.guarda:
        WR
        and ecx, 7                  ; y en un registro, si no es el 6
        cmp ecx, 6
        je .x
        mov ecx, [z_reg8+ecx*4]
        mov [ecx], al
.x:
        FIN 23

; =============================================================================
;  CB: giros, desplazamientos y bits
; =============================================================================
%macro CB_OP 1
 %assign _k (%1) >> 6
 %assign _o ((%1) >> 3) & 7
 %assign _r (%1) & 7
 %assign _n %1
zcb_%[_n]:
 %if _r == 6
        movzx edx, word [z_l]
        RD
 %else
  %undef RS
  DEFR8 RS, _r, 0
        mov al, [RS]
 %endif
 %if _k == 0
        mov bl, _o
        call z_rot
 %elif _k == 1
        mov bl, 1 << _o
  %if _r == 6
        mov dl, [z_wz+1]
  %else
        mov dl, al
  %endif
        call z_bit
  %if _r == 6
        FIN 12
  %else
        FIN 8
  %endif
 %elif _k == 2
        and al, ~(1 << _o) & 0xFF
 %else
        or al, 1 << _o
 %endif
 %if _k != 1
  %if _r == 6
        WR
        FIN 15
  %else
        mov [RS], al
        FIN 8
  %endif
 %endif
%endmacro
%assign _i 0
%rep 256
        CB_OP _i
 %assign _i _i + 1
%endrep

; =============================================================================
;  ED
; =============================================================================
; la etiqueta de la instrucción ED %1
%macro EDOP 1
 %assign _n %1
zed_%[_n]:
 %xdefine HAYED_%[_n] 1
%endmacro
zed_nop:                            ; las que no hacen nada: 8 ciclos
        FIN 8
%macro ED_INR 2                     ; IN r,(C) ; %2 = registro (o 0: solo banderas)
        movzx edx, word [z_bc]
        lea eax, [edx+1]
        mov [z_wz], ax
        call [z_in]
 %ifnidn %2, 0
        mov [%2], al
 %endif
        movzx eax, al
        mov al, [tab_sz53p+eax]
        mov ah, [z_f]
        and ah, F_C
        or al, ah
        mov [z_f], al
        FIN 12
%endmacro
%macro ED_OUTR 1
        movzx edx, word [z_bc]
        lea eax, [edx+1]
        mov [z_wz], ax
 %ifidn %1, 0
        xor al, al
 %else
        mov al, [%1]
 %endif
        call [z_out]
        sub edi, 12
        cmp byte [z_mira], 0
        jne z_mira_ya
        test edi, edi
        jle near z_fuera
        DESPACHA
%endmacro
EDOP 0x40
        ED_INR 0, z_b
EDOP 0x48
        ED_INR 0, z_c
EDOP 0x50
        ED_INR 0, z_d
EDOP 0x58
        ED_INR 0, z_e
EDOP 0x60
        ED_INR 0, z_h
EDOP 0x68
        ED_INR 0, z_l
EDOP 0x70
        ED_INR 0, 0
EDOP 0x78
        ED_INR 0, z_a
EDOP 0x41
        ED_OUTR z_b
EDOP 0x49
        ED_OUTR z_c
EDOP 0x51
        ED_OUTR z_d
EDOP 0x59
        ED_OUTR z_e
EDOP 0x61
        ED_OUTR z_h
EDOP 0x69
        ED_OUTR z_l
EDOP 0x71
        ED_OUTR 0
EDOP 0x79
        ED_OUTR z_a
%macro ED_HL16 3                    ; código, z_sbc16 o z_adc16, el registro
EDOP %1
        mov cx, [z_hl]
        mov dx, [%3]
        call %2
        FIN 15
%endmacro
        ED_HL16 0x42, z_sbc16, z_bc
        ED_HL16 0x52, z_sbc16, z_de
        ED_HL16 0x62, z_sbc16, z_hl
        ED_HL16 0x72, z_sbc16, z_sp
        ED_HL16 0x4A, z_adc16, z_bc
        ED_HL16 0x5A, z_adc16, z_de
        ED_HL16 0x6A, z_adc16, z_hl
        ED_HL16 0x7A, z_adc16, z_sp
%macro ED_ST16 2
EDOP %1
        FETCH16
        mov cx, [%2]
        WR16
        mov [z_wz], dx
        FIN 20
%endmacro
%macro ED_LD16 2
EDOP %1
        FETCH16
        RD16
        mov [%2], cx
        mov [z_wz], dx
        FIN 20
%endmacro
        ED_ST16 0x43, z_bc
        ED_ST16 0x53, z_de
        ED_ST16 0x63, z_hl
        ED_ST16 0x73, z_sp
        ED_LD16 0x4B, z_bc
        ED_LD16 0x5B, z_de
        ED_LD16 0x6B, z_hl
        ED_LD16 0x7B, z_sp
zed_neg:
        xor al, al
        sub al, [z_a]
        lahf
        seto dl
        mov [z_a], al
        or ah, F_N
        and ah, F_S | F_Z | F_H | F_N | F_C
        and al, F_Y | F_X
        or ah, al
        shl dl, 2
        or ah, dl
        mov [z_f], ah
        FIN 8
zed_retn:                           ; RETN y RETI: IFF1 = IFF2
        mov al, [z_iff2]
        mov [z_iff1], al
        POPW
        mov edx, ecx
        SALTA
        FIN 14
zed_im0:
        mov byte [z_im], 0
        FIN 8
zed_im1:
        mov byte [z_im], 1
        FIN 8
zed_im2:
        mov byte [z_im], 2
        FIN 8
EDOP 0x47                             ; LD I,A
        mov al, [z_a]
        mov [z_i], al
        FIN 9
EDOP 0x4F                             ; LD R,A
        mov al, [z_a]
        mov [z_r], al
        and al, 0x80
        mov [z_r7], al
        FIN 9
EDOP 0x57                             ; LD A,I
        mov al, [z_i]
        jmp z_ld_air
EDOP 0x5F                             ; LD A,R
        mov al, [z_r]
        and al, 0x7F
        or al, [z_r7]
z_ld_air:
        mov [z_a], al
        movzx eax, al
        mov al, [tab_sz53+eax]
        mov ah, [z_f]
        and ah, F_C
        or al, ah
        cmp byte [z_iff2], 0
        je .x
        or al, F_P
.x:
        mov [z_f], al
        FIN 9
EDOP 0x67                             ; RRD
        movzx edx, word [z_hl]
        RD
        mov ah, [z_a]
        mov cl, al                  ; (HL) = A bajo : (HL) alto; A bajo = (HL) bajo
        shr al, 4
        mov ch, ah
        shl ch, 4
        or al, ch
        WR
        and cl, 0x0F
        and ah, 0xF0
        or ah, cl
        jmp z_rxd
EDOP 0x6F                             ; RLD
        movzx edx, word [z_hl]
        RD
        mov ah, [z_a]
        mov cl, al                  ; (HL) = (HL) bajo : A bajo; A bajo = (HL) alto
        shl al, 4
        mov ch, ah
        and ch, 0x0F
        or al, ch
        WR
        shr cl, 4
        and ah, 0xF0
        or ah, cl
z_rxd:
        mov [z_a], ah
        inc edx
        mov [z_wz], dx
        movzx eax, ah
        mov al, [tab_sz53p+eax]
        mov ah, [z_f]
        and ah, F_C
        or al, ah
        mov [z_f], al
        FIN 18
; ---- las de bloque ----
; LDI/LDD: (DE) = (HL); HL y DE +-1; BC - 1
%macro ED_LD 2                      ; código, 1 o -1
EDOP %1
        movzx edx, word [z_hl]
        RD
        add word [z_hl], %2
        movzx edx, word [z_de]
        WR
        add word [z_de], %2
        add al, [z_a]               ; 3 y 5: de (HL)+A, sus bits 3 y 1
        mov ah, al
        and ah, F_X
        shl al, 4
        and al, F_Y
        or ah, al
        mov al, [z_f]
        and al, F_S | F_Z | F_C
        or ah, al
        dec word [z_bc]
        jz %%c
        or ah, F_P
%%c:
        mov [z_f], ah
%endmacro
        ED_LD 0xA0, 1
        FIN 16
        ED_LD 0xA8, -1
        FIN 16
        ED_LD 0xB0, 1
        jmp z_repite
        ED_LD 0xB8, -1
; LDIR, LDDR, CPIR...: si sigue (P=1 o, en CP, además Z=0), vuelve atrás
z_repite:
        test byte [z_f], F_P
        jz .no
        sub esi, 2
        and esi, 0xFFFF
        lea eax, [esi+1]
        mov [z_wz], ax
        FIN 21
.no:
        FIN 16
; CPI/CPD: A - (HL); HL +-1; BC - 1
%macro ED_CP 2
EDOP %1
        movzx edx, word [z_hl]
        RD
        add word [z_hl], %2
        add word [z_wz], %2
        mov cl, al
        mov al, [z_a]
        sub al, cl
        lahf
        and ah, F_S | F_Z | F_H
        or ah, F_N
        mov cl, ah
        shr cl, 4                   ; 3 y 5: de A-(HL)-H, sus bits 3 y 1
        and cl, 1
        sub al, cl
        mov cl, al
        and cl, F_X
        or ah, cl
        shl al, 4
        and al, F_Y
        or ah, al
        mov al, [z_f]
        and al, F_C
        or ah, al
        dec word [z_bc]
        jz %%c
        or ah, F_P
%%c:
        mov [z_f], ah
%endmacro
        ED_CP 0xA1, 1
        FIN 16
        ED_CP 0xA9, -1
        FIN 16
        ED_CP 0xB1, 1
        jmp z_repite_cp
        ED_CP 0xB9, -1
z_repite_cp:
        test byte [z_f], F_Z
        jnz z_repite.no
        jmp z_repite
; ---- las de entrada y salida, enteras ----
; deja las banderas: AL = el octeto, CL = k (octeto + C+-1 o + L, 8 bits) y
; CH = el acarreo de esa suma; B ya bajado
z_es_flags:
        movzx edx, byte [z_b]
        mov ah, [tab_sz53+edx]
        mov bl, al
        shr bl, 6
        and bl, F_N
        or ah, bl
        test ch, ch
        jz .nc
        or ah, F_H | F_C
.nc:
        and cl, 7
        xor cl, [z_b]
        movzx edx, cl
        mov bl, [tab_sz53p+edx]
        and bl, F_P
        or ah, bl
        mov [z_f], ah
        ret
%macro ED_INX 2                     ; código, +-1
EDOP %1
        movzx edx, word [z_bc]
        lea eax, [edx+%2]
        mov [z_wz], ax
        call [z_in]
        movzx edx, word [z_hl]
        WR
        add word [z_hl], %2
        dec byte [z_b]
        mov cl, [z_c]
        add cl, %2
        add cl, al
        setc ch
        call z_es_flags
%endmacro
%macro ED_OUTX 2
EDOP %1
        movzx edx, word [z_hl]
        RD
        add word [z_hl], %2
        dec byte [z_b]
        movzx edx, word [z_bc]
        push eax
        lea eax, [edx+%2]
        mov [z_wz], ax
        pop eax
        call [z_out]
        mov cl, [z_l]
        add cl, al
        setc ch
        call z_es_flags
%endmacro
        ED_INX 0xA2, 1
        FIN 16
        ED_INX 0xAA, -1
        FIN 16
        ED_OUTX 0xA3, 1
        jmp z_es_fin
        ED_OUTX 0xAB, -1
z_es_fin:
        sub edi, 16
        cmp byte [z_mira], 0
        jne z_mira_ya
        test edi, edi
        jle near z_fuera
        DESPACHA
        ED_INX 0xB2, 1
        jmp z_es_rep
        ED_INX 0xBA, -1
z_es_rep:                           ; INIR, INDR, OTIR, OTDR: hasta que B sea 0
        cmp byte [z_b], 0
        je .no
        sub esi, 2
        and esi, 0xFFFF
        sub edi, 21
        cmp byte [z_mira], 0
        jne z_mira_ya
        test edi, edi
        jle near z_fuera
        DESPACHA
.no:
        sub edi, 16
        cmp byte [z_mira], 0
        jne z_mira_ya
        test edi, edi
        jle near z_fuera
        DESPACHA
        ED_OUTX 0xB3, 1
        jmp z_es_rep
        ED_OUTX 0xBB, -1
        jmp z_es_rep

; =============================================================================
;  Las tablas de saltos
; =============================================================================
        align 4
tab_op:
%assign _i 0
%rep 256
 %ifdef HAY_0_%[_i]
        dd zo0_%[_i]
 %else
  %error "falta una instrucción"
 %endif
 %assign _i _i + 1
%endrep
tab_dd:
%assign _i 0
%rep 256
 %ifdef HAY_1_%[_i]
        dd zo1_%[_i]
 %else
        dd z_prefnada
 %endif
 %assign _i _i + 1
%endrep
tab_fd:
%assign _i 0
%rep 256
 %ifdef HAY_2_%[_i]
        dd zo2_%[_i]
 %else
        dd z_prefnada
 %endif
 %assign _i _i + 1
%endrep
tab_cb:
%assign _i 0
%rep 256
        dd zcb_%[_i]
 %assign _i _i + 1
%endrep
tab_ed:
%assign _i 0
%rep 256
 %assign _lo _i & 7
 %assign _hi (_i >> 3) & 7
 %ifdef HAYED_%[_i]
        dd zed_%[_i]
 %elif _i < 0x40 || _i >= 0x80
        dd zed_nop
 %elif _lo == 4
        dd zed_neg
 %elif _lo == 5
        dd zed_retn
 %elif _lo == 6
  %if _hi == 2 || _hi == 6
        dd zed_im1
  %elif _hi == 3 || _hi == 7
        dd zed_im2
  %else
        dd zed_im0
  %endif
 %else
        dd zed_nop
 %endif
 %assign _i _i + 1
%endrep

; los registros de 8 bits por su número (para DDCB, que copia el resultado)
z_reg8      dd z_b, z_c, z_d, z_e, z_h, z_l, 0, z_a
