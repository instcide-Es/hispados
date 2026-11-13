; =============================================================================
;  Simulador de los puertos de los mandos, para las pruebas de HSPJOY
;  (HSPJOYT.COM = drivers/hspjoy.asm con -DPRUEBA; tests/run_joy_tests.sh).
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  QEMU no tiene puerto de juegos (201h) y no deja mover las entradas del
;  puerto de impresora, así que en HSPJOYT las entradas y salidas de esos
;  puertos (PIN/POUT) llegan aquí en vez de al puerto: sim_in y sim_out, con
;  DX = puerto y AL = dato, como IN y OUT. El simulador hace de:
;    - puerto de juegos: al escribir en el 201h los cuatro ejes se ponen a 1
;      y cada uno baja tras un número de lecturas del puerto (sim_gp_t). No
;      se usa el tiempo de verdad: en un emulador lo que tarda cada vuelta
;      del bucle de medida cambia de una vez a otra y la prueba no sería
;      repetible; así la medida de HSPJOY tiene que dar justo ese número;
;    - mando de NES o Super Nintendo: un registro de desplazamiento que se
;      carga con la patilla 3 (D1) y avanza con el flanco de subida de la 2
;      (D0); la línea de datos, a las patillas 10 y 11;
;    - mando de PlayStation: selección en la patilla 3, reloj en la 4,
;      órdenes en la 2; contesta sim_psx si la primera orden es 01h y guarda
;      lo que le mandan en sim_psx_cmd;
;    - joystick Atari y mando de Mega Drive: las patillas de datos leídas con
;      el puerto en entrada, según la selección (patilla 14).
;  El programa de prueba (JOYTEST S desplazamiento bytes...) cambia estos
;  datos a través de INT 2Fh AX=D97Fh (ES:DI = sim).
; =============================================================================
SIM_LPT     equ 0x378

sim:
sim_gp_t    dw 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF   ; +00 lecturas de X1 Y1 X2 Y2 (FFFFh = no hay)
sim_gp_btn  db 0xF0                 ; +08 bits 4-7 del puerto 201h (0 = pulsado)
sim_kind    db 0                    ; +09 qué hay en el cable: 4 Atari, 5 Mega Drive,
                                    ;     6 NES, 7 SNES, 8 PlayStation
sim_pad     dw 0, 0                 ; +0A NES/SNES: botones pulsados de los mandos 1 y 2,
                                    ;     en el orden en que salen (bit 0 = el primero)
sim_db9     db 0                    ; +0E Atari / Mega Drive con la selección arriba:
                                    ;     bit 0 arriba, 1 abajo, 2 izq., 3 der., 4 y 5 botones
sim_md      db 0                    ; +0F Mega Drive con la selección abajo: bit 4 A, 5 Start
sim_psx     db 0xFF, 0x41, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80     ; +10 mando 1
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF     ; +19 mando 2
sim_psx_cmd times 9 db 0            ; +22 las órdenes recibidas
sim_sw      db 0                    ; +2B poner a 1: a los 5 s la palanca del puerto de
                                    ;     juegos da vueltas durante 4 s (X1 e Y1 entre
                                    ;     40 y 560) y vuelve al centro (300). Para
                                    ;     probar MANDO /CALIBRAR sin manos.
sim_sw_t    dw 0
sim_sw_d    dw 13
; estado interno
sim_t0      dw 0
sim_data    db 0
sim_ctrl    db 0
sim_sh      dw 0xFFFF, 0xFFFF
sim_pi      db 0                    ; PlayStation: byte y bit en curso
sim_pb      db 0
sim_d       db 3                    ; nivel de las líneas de datos (bit 0 mando 1, bit 1 mando 2)
sim_fin:

sim_out:
        push ax
        push bx
        push cx
        push ds
        push cs
        pop ds
        cmp dx, 0x201
        jne .lpt
        mov word [sim_t0], 0
        cmp byte [sim_sw], 0
        je .r
        ; ---- barrido ----
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        pop es
        cmp byte [sim_sw], 1
        jne .sw2
        add ax, 91
        mov [sim_sw_t], ax
        mov byte [sim_sw], 2
        jmp .r
.sw2:
        sub ax, [sim_sw_t]
        js .r                       ; aún no
        cmp ax, 73
        jb .sw3
        mov byte [sim_sw], 0        ; se acabó: al centro
        mov word [sim_gp_t], 300
        mov word [sim_gp_t+2], 300
        jmp .r
.sw3:
        mov ax, [sim_gp_t]
        add ax, [sim_sw_d]
        cmp ax, 560
        jl .sw4
        mov ax, 560
        neg word [sim_sw_d]
.sw4:
        cmp ax, 40
        jg .sw5
        mov ax, 40
        neg word [sim_sw_d]
.sw5:
        mov [sim_gp_t], ax
        neg ax
        add ax, 600
        mov [sim_gp_t+2], ax
        jmp .r
.lpt:
        cmp dx, SIM_LPT + 2
        jne .dat
        mov [sim_ctrl], al
        jmp .r
.dat:
        cmp dx, SIM_LPT
        jne .r
        mov ah, [sim_data]          ; AH = antes, AL = ahora
        mov [sim_data], al
        cmp byte [sim_kind], 8
        je .psx
        cmp byte [sim_kind], 6
        jb .r
        ; ---- NES / SNES ----
        test al, 2                  ; carga arriba: el registro toma los botones
        jz .clk
        mov bx, [sim_pad]
        not bx
        mov [sim_sh], bx
        mov bx, [sim_pad+2]
        not bx
        mov [sim_sh+2], bx
        jmp .lvl
.clk:
        test al, 1                  ; flanco de subida del reloj: avanza
        jz .lvl
        test ah, 1
        jnz .lvl
        stc
        rcr word [sim_sh], 1
        stc
        rcr word [sim_sh+2], 1
.lvl:
        mov al, [sim_sh]
        and al, 1
        mov ah, [sim_sh+2]
        and ah, 1
        shl ah, 1
        or al, ah
        mov [sim_d], al
        jmp .r
        ; ---- PlayStation ----
.psx:
        test al, 2                  ; selección arriba: en reposo
        jz .sel
        mov byte [sim_d], 3
        mov word [sim_pi], 0
        jmp .r
.sel:
        test ah, 2                  ; acaba de bajar: empieza la respuesta
        jz .pc
        mov word [sim_pi], 0
        push di
        push es
        push ds
        pop es
        mov di, sim_psx_cmd
        mov cx, 9
        push ax
        xor al, al
        cld
        rep stosb
        pop ax
        pop es
        pop di
.pc:
        mov bl, al
        xor bl, ah
        test bl, 4                  ; ¿cambia el reloj?
        jz .r
        test al, 4
        jnz .sube
        ; baja: se toma el bit de la orden y el mando pone el suyo
        movzx bx, byte [sim_pi]
        cmp bx, 9
        jae .r
        mov cl, [sim_pb]
        mov ch, al
        and ch, 1
        shl ch, cl
        or [sim_psx_cmd+bx], ch
        mov al, [sim_psx+bx]
        mov ah, [sim_psx+9+bx]
        ; el mando solo contesta si la primera orden fue 01h
        test bx, bx
        jz .pn
        cmp byte [sim_psx_cmd], 0x01
        je .pn
        mov ax, 0xFFFF
.pn:
        shr al, cl
        and al, 1
        shr ah, cl
        and ah, 1
        shl ah, 1
        or al, ah
        mov [sim_d], al
        jmp .r
.sube:
        inc byte [sim_pb]
        cmp byte [sim_pb], 8
        jb .r
        mov byte [sim_pb], 0
        inc byte [sim_pi]
.r:
        pop ds
        pop cx
        pop bx
        pop ax
        ret

sim_in:
        push bx
        push cx
        push ds
        push cs
        pop ds
        mov [sim_ah], ah            ; (IN no cambia AH)
        cmp dx, 0x201
        jne .lpt
        mov bx, [sim_t0]            ; BX = lecturas desde la escritura
        cmp bx, 0xFFFE
        jae .g0
        inc word [sim_t0]
.g0:
        mov al, [sim_gp_btn]
        and al, 0xF0
        cmp bx, [sim_gp_t]
        jae .g1
        or al, 1
.g1:
        cmp bx, [sim_gp_t+2]
        jae .g2
        or al, 2
.g2:
        cmp bx, [sim_gp_t+4]
        jae .g3
        or al, 4
.g3:
        cmp bx, [sim_gp_t+6]
        jae .r
        or al, 8
        jmp .r
.lpt:
        cmp dx, SIM_LPT + 1         ; estado: patilla 10 = bit 6; 11 = bit 7 invertido
        jne .ctl
        mov al, [sim_d]
        mov ah, al
        and al, 1
        shl al, 6
        and ah, 2
        xor ah, 2
        shl ah, 6
        or al, ah
        or al, 0x18
        jmp .r
.ctl:
        cmp dx, SIM_LPT + 2
        jne .dat
        mov al, [sim_ctrl]
        jmp .r
.dat:
        mov al, 0xFF
        cmp dx, SIM_LPT
        jne .r
        mov al, [sim_data]
        test byte [sim_ctrl], 0x20  ; ¿en entrada?
        jz .r
        mov al, 0xFF
        cmp byte [sim_kind], 4
        jb .r
        cmp byte [sim_kind], 5
        ja .r
        mov al, [sim_db9]
        not al
        cmp byte [sim_kind], 5
        jne .r
        test byte [sim_ctrl], 2     ; AUTOFD a 1 = patilla 14 abajo = selección abajo
        jz .r
        and al, 0x03                ; arriba y abajo siguen; izquierda y derecha, a 0
        mov ah, [sim_md]
        not ah
        and ah, 0x30
        or al, ah
        or al, 0xC0
.r:
        mov ah, [sim_ah]
        pop ds
        pop cx
        pop bx
        ret
sim_ah      db 0
