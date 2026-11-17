; =============================================================================
;  Hispa-8086 - la memoria de más allá de 1 mega (solo en un 286 o superior)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hispa-8086 es el mismo sistema en un 8088 y en un 286; pero si al arrancar
;  encuentra un 286 (o más) con memoria extendida, la aprovecha:
;
;  LA PUERTA A20. Un 286 tiene 24 líneas de dirección, pero el PC AT arranca
;  con la 21.ª (A20) cerrada, para que la memoria «dé la vuelta» al megabyte
;  como en un 8088. Se abre por el controlador del teclado (8042) y, donde
;  eso no basta, por el puerto 92h; y se comprueba de verdad, escribiendo en
;  0000:0000 y mirando FFFF:0010 (ver el puente, en nucleo.asm).
;
;  LA MEMORIA ALTA (HMA). Con A20 abierta, el segmento FFFFh llega 64 K menos
;  16 octetos más allá del mega. Con DOS=ALTO en CONFIG.SYS, el núcleo entero
;  (código, variables, pila, tabla de archivos y reserva de sectores) se
;  copia ahí y sigue desde ahí: en la memoria convencional solo queda el
;  «puente» (nucleo.asm), unos 350 octetos. Por eso el núcleo se ensambla
;  desde la dirección 10h: corre igual en 006F:0010 que en FFFF:0010.
;
;  LA MEMORIA EXTENDIDA (XMS 2.0). Lo que hay por encima de la HMA se reparte
;  a los programas como lo hace HIMEM.SYS: INT 2Fh/4300h dice que hay gestor,
;  4310h da su punto de entrada, y por él se piden y se sueltan bloques (en
;  K), se copian datos entre ellos y la memoria convencional, se abre y se
;  cierra A20 y se pide la HMA. Va DENTRO del núcleo y se activa solo: lo
;  útil es que la memoria esté ahí sin tener que saber poner un DEVICE=.
;
;  Para copiar se usa la BIOS (INT 15h/87h), que es quien sabe entrar en modo
;  protegido y volver en cada máquina (en un 286 no hay vuelta limpia: la
;  BIOS reinicia el procesador por el 8042 y lo recoge). Así aquí no hay ni
;  una instrucción de 286: todo este archivo es de 8086, pero no se llega a
;  él si la detección no encuentra un 286 (o si CONFIG.SYS dice
;  PROCESADOR=8086, que es como se prueba que sin esto todo sigue igual).
;  La BIOS de un AT cierra A20 al volver de copiar: por eso se la llama desde
;  el puente, que está abajo, y es él quien la vuelve a abrir.
; =============================================================================
N_XMS       equ 16                  ; bloques de memoria extendida a la vez
X_USO       equ 0                   ; db: 0 libre
X_BLOQ      equ 1                   ; db: cuántas veces está «bloqueado»
X_BASE      equ 2                   ; dw: dónde empieza, en K desde 0
X_TAM       equ 4                   ; dw: cuántos K
X_ENT       equ 8
XE_NOFUN    equ 0x80                ; esa función no está
XE_A20      equ 0x82                ; no se puede con A20
XE_HMA_USO  equ 0x91                ; la HMA ya tiene dueño
XE_HMA_NO   equ 0x93                ; la HMA no estaba pedida
XE_A20_ON   equ 0x94                ; A20 sigue abierta
XE_LLENO    equ 0xA0                ; no queda memoria extendida
XE_ASAS     equ 0xA1                ; no quedan bloques
XE_ASA      equ 0xA2                ; ese bloque no existe
XE_LARGO    equ 0xA7                ; el largo no vale
XE_SIN_BLOQ equ 0xAA                ; no estaba bloqueado
XE_BLOQ     equ 0xAB                ; está bloqueado

; ¿hay un 286 con memoria extendida? Entonces hay gestor: [xms_hay]
xms_ini:
        cmp byte [cpu_tipo], CPU_286
        jb .x
        cmp byte [como_8086], 0
        jne .x
        mov ah, 0x88                ; cuántos K hay por encima del mega
        int 0x15
        jc .x
        cmp ax, 64 + 16             ; (la HMA y algo más)
        jb .x
        cmp ax, 0xFFFF - 1024       ; (las cuentas van en K, en 16 bits)
        jbe .k
        mov ax, 0xFFFF - 1024
.k:
        mov [ext_k], ax
        add ax, 1024
        mov [x_fin], ax
        mov word [x_ini], 1024 + 64
        mov byte [xms_hay], 1
        push es                     ; INT 2Fh: por ahí se pregunta si lo hay
        xor ax, ax
        mov es, ax
        mov ax, [es:0x2F*4]
        mov [ant2f], ax
        mov ax, [es:0x2F*4+2]
        mov [ant2f+2], ax
        cli
        mov word [es:0x2F*4], b_int2f
        mov [es:0x2F*4+2], cs
        sti
        pop es
.x:
        ret

; sube el núcleo a la HMA, si se ha pedido (DOS=ALTO) y se puede. DI = donde
; acaba el núcleo (con sus tablas). Conserva DI. Al volver puede que ya se
; esté arriba (CS = DS = ES = SS = FFFFh)
alto_sube:
        cmp byte [dos_alto], 0
        je .x
        cmp byte [xms_hay], 0
        je .x
        cmp di, 0xFFE0              ; tiene que caber en el segmento FFFFh
        ja .x
        mov byte [b_quiere], 1      ; A20, abierta desde ahora
        call far [p_abre]
        call far [p_esta]
        jnz .va
        mov byte [b_quiere], 0      ; no se abre: nos quedamos abajo
        mov si, m_sin_a20
        jmp pon_cad
.va:
        push di
        mov cx, di
        mov si, 0x10
        sub cx, si
        mov di, si
        mov ax, 0xFFFF
        mov es, ax
        rep movsb
        pop di
        mov bx, b_punteros          ; el puente, a partir de ahora, salta arriba
        mov cx, N_PUENTES
.p:
        mov [bx+2], ax
        add bx, 4
        loop .p
        cli
        mov ss, ax
        mov ds, ax
        sti
        push ax
        mov ax, .arriba
        push ax
        retf
.arriba:
        mov byte [alto], 1
        call disp_ini               ; (la cadena de dispositivos, con su sitio nuevo)
.x:
        push cs
        pop es
        ret

; ---- INT 2Fh: ¿hay gestor de memoria extendida? ----
int2f:
        cmp ah, 0x43
        jne .otro
        cmp al, 0
        jne .n
        mov al, 0x80                ; sí
        iret
.n:
        cmp al, 0x10
        jne .otro
        mov bx, b_xms               ; su punto de entrada (en el puente)
        mov es, [cs:bajo_seg]
        iret
.otro:
        cmp word [cs:ant2f+2], 0    ; lo demás, a quien estuviera antes (si
        je .nadie                   ; había alguien)
        jmp far [cs:ant2f]
.nadie:
        iret

; ---- el gestor: se llama con AH = la función. Devuelve AX = 1 si va bien, o
;      AX = 0 y el error en BL (salvo las que dicen otra cosa). Conserva los
;      registros que no devuelven nada ----
xms_entra:
        push ds
        push es
        push bp
        push si
        push di
        push cx
        mov bp, ds                  ; (el DS de quien llama: para «mover»)
        push cs
        pop ds
        push cs
        pop es
        cld
        mov [x_si], si
        mov [x_ds], bp
        mov bp, bx                  ; (BX de quien llama, a salvo)
        mov bl, XE_NOFUN
        cmp ah, 0x0F
        ja .mal
        mov bl, ah
        xor bh, bh
        shl bx, 1
        mov si, [x_funciones+bx]
        mov bx, bp
        call si
        jc .mal
        mov ax, 1
        jmp short .fin
.mal:
        xor ax, ax
.fin:
        pop cx
        pop di
        pop si
        pop bp
        pop es
        pop ds
        retf
            align 2
x_funciones dw x00, x01, x02, x03, x04, x05, x06, x07
            dw x08, x09, x0A, x0B, x0C, x0D, x0E, x0F

; 00: la versión -> AX = 0200h, BX = la nuestra, DX = 1 (hay HMA)
x00:
        add sp, 2                   ; (no devuelve «1»: vuelve ella misma)
        mov ax, 0x0200
        mov bx, VERSION10
        mov dx, 1
        jmp xms_entra.fin
; 01: pedir la HMA; 02: soltarla
x01:
        mov bl, XE_HMA_USO
        mov al, [alto]
        or al, [hma_dueno]
        jnz x_mal
        mov byte [hma_dueno], 1
        clc
        ret
x02:
        mov bl, XE_HMA_NO
        cmp byte [hma_dueno], 0
        je x_mal
        mov byte [hma_dueno], 0
        jmp short x_a20
x_mal:
        stc
        ret
; 03 y 04: abrir y cerrar A20 «para todos»; 05 y 06: «para mí» (se cuentan)
x03:
        mov byte [a20_g], 1
        jmp short x_a20
x04:
        mov byte [a20_g], 0
        jmp short x_a20
x05:
        inc byte [a20_l]
        jmp short x_a20
x06:
        cmp byte [a20_l], 0
        je x_a20
        dec byte [a20_l]
; (común) A20 queda abierta si alguien la quiere: el sistema (si está arriba),
; quien tenga la HMA, o quien la haya pedido; si no, se cierra
x_a20:
        push es
        mov es, [bajo_seg]
        mov al, [alto]
        or al, [a20_g]
        or al, [a20_l]
        mov [es:b_quiere], al
        pop es
        jz .cierra
        call far [p_abre]
        call far [p_esta]
        mov bl, XE_A20
        jz x_mal
        clc
        ret
.cierra:
        call far [p_cierra]
        clc
        ret
; 07: ¿está abierta A20? -> AX = 1 o 0
x07:
        add sp, 2
        call far [p_esta]
        mov ax, 0
        jz .x
        inc ax
.x:
        xor bl, bl
        jmp xms_entra.fin
; 08: cuánta queda -> AX = el mayor trozo, DX = toda, en K
x08:
        add sp, 2
        mov dx, [x_fin]             ; toda: lo que hay menos lo repartido
        sub dx, [x_ini]
        mov bx, x_tabla
        mov cx, N_XMS
.s:
        cmp byte [bx+X_USO], 0
        je .s1
        sub dx, [bx+X_TAM]
.s1:
        add bx, X_ENT
        loop .s
        push dx
        ; el mayor trozo: se prueba desde el principio y desde el final de
        ; cada bloque
        xor di, di                  ; DI = el mayor
        mov ax, [x_ini]
        call .desde
        mov bx, x_tabla
        mov cx, N_XMS
.t:
        cmp byte [bx+X_USO], 0
        je .t1
        mov ax, [bx+X_BASE]
        add ax, [bx+X_TAM]
        push bx
        push cx
        call .desde
        pop cx
        pop bx
.t1:
        add bx, X_ENT
        loop .t
        mov ax, di
        pop dx
        xor bl, bl
        test ax, ax
        jnz .x
        mov bl, XE_LLENO
.x:
        jmp xms_entra.fin
; (interna) el hueco que empieza en AX, si empieza en sitio libre: hasta el
; bloque siguiente o el final; si es mayor que DI, a DI
.desde:
        mov si, [x_fin]             ; SI = hasta dónde llega
        mov bx, x_tabla
        mov cx, N_XMS
.d:
        cmp byte [bx+X_USO], 0
        je .d2
        cmp word [bx+X_TAM], 0
        je .d2
        mov dx, [bx+X_BASE]
        cmp dx, ax
        ja .d1
        add dx, [bx+X_TAM]          ; empieza antes: ¿nos pilla dentro?
        cmp dx, ax
        ja .dx
        jmp short .d2
.d1:
        cmp dx, si                  ; empieza después: acorta el hueco
        jae .d2
        mov si, dx
.d2:
        add bx, X_ENT
        loop .d
        sub si, ax
        jb .dx
        cmp si, di
        jbe .dx
        mov di, si
.dx:
        ret

; (interna) un sitio para DX K (sin contar el bloque BP, si no es 0) -> AX =
; dónde. CF=1 si no lo hay
x_hueco:
        mov ax, [x_ini]
.otra:
        mov bx, x_tabla
        mov cx, N_XMS
.b:
        cmp byte [bx+X_USO], 0
        je .s
        cmp bx, bp
        je .s
        cmp word [bx+X_TAM], 0
        je .s
        mov si, ax                  ; ¿se pisan?
        add si, dx
        jc .no
        cmp [bx+X_BASE], si
        jae .s
        mov si, [bx+X_BASE]
        add si, [bx+X_TAM]
        cmp ax, si
        jae .s
        mov ax, si                  ; sí: detrás de él, y a mirar otra vez
        jmp .otra
.s:
        add bx, X_ENT
        loop .b
        mov si, ax
        add si, dx
        jc .no
        cmp si, [x_fin]
        ja .no
        clc
        ret
.no:
        stc
        ret
; (interna) DX = un bloque que nos dan -> BX = su entrada. CF=1 y BL=error si
; no es de los nuestros
x_asa:
        mov bx, dx
        sub bx, x_tabla
        jb .mal
        cmp bx, N_XMS * X_ENT
        jae .mal
        test bl, X_ENT - 1
        jnz .mal
        mov bx, dx
        cmp byte [bx+X_USO], 0
        je .mal
        clc
        ret
.mal:
        mov bl, XE_ASA
        stc
        ret

; 09: pedir DX K -> DX = el bloque
x09:
        mov di, x_tabla             ; una entrada libre
        mov cx, N_XMS
.b:
        cmp byte [di+X_USO], 0
        je .hay
        add di, X_ENT
        loop .b
        mov bl, XE_ASAS
        stc
        ret
.hay:
        push bp
        xor bp, bp
        call x_hueco
        pop bp
        mov bl, XE_LLENO
        jc .x
        mov word [di+X_USO], 1      ; (en uso y sin bloquear)
        mov [di+X_BASE], ax
        mov [di+X_TAM], dx
        mov dx, di
        mov bx, bp
        clc
.x:
        ret
; 0A: soltar el bloque DX
x0A:
        call x_asa
        jc .x
        cmp byte [bx+X_BLOQ], 0
        mov byte [bx+X_USO], 0
        mov bx, bp
        je .x                       ; (CF=0)
        mov bx, dx
        mov byte [bx+X_USO], 1
        mov bl, XE_BLOQ
        stc
.x:
        ret
; 0C: bloquear el bloque DX -> DX:BX = su dirección; 0D: soltarlo
x0C:
        call x_asa
        jc .x
        inc byte [bx+X_BLOQ]
        mov ax, [bx+X_BASE]         ; K x 1.024
        mov dx, ax
        mov cl, 6
        shr dx, cl
        mov cl, 10
        shl ax, cl
        mov bx, ax
        clc
.x:
        ret
x0D:
        call x_asa
        jc .x
        cmp byte [bx+X_BLOQ], 0
        je .mal
        dec byte [bx+X_BLOQ]
        mov bx, bp
        clc
.x:
        ret
.mal:
        mov bl, XE_SIN_BLOQ
        stc
        ret
; 0E: qué hay del bloque DX -> BH = bloqueos, BL = bloques libres, DX = K
x0E:
        call x_asa
        jc .x
        mov ah, [bx+X_BLOQ]
        mov dx, [bx+X_TAM]
        xor al, al
        mov bx, x_tabla
        mov cx, N_XMS
.b:
        cmp byte [bx+X_USO], 0
        jne .s
        inc al
.s:
        add bx, X_ENT
        loop .b
        mov bx, ax
        clc
.x:
        ret
; 0F: el bloque DX pasa a tener BX K. Si no puede crecer donde está, se muda
; a otro sitio (y se copia lo que tiene)
x0F:
        call x_asa
        jc .x
        mov di, bx
        mov bl, XE_BLOQ
        cmp byte [di+X_BLOQ], 0
        jne .mal
        cmp bp, [di+X_TAM]
        jbe .pon                    ; encoger: sin más
        push dx
        push bp
        mov dx, bp                  ; ¿está libre lo que le sigue?
        mov ax, [di+X_BASE]
        mov si, ax
        add si, dx
        jc .otro
        cmp si, [x_fin]
        ja .otro
        mov bp, di
        call .libre_desde
        jc .otro
        pop bp
        pop dx
        jmp short .pon
.otro:
        xor bp, bp                  ; no: un sitio nuevo (sin pisar el viejo)
        call x_hueco
        pop bp
        pop dx
        mov bl, XE_LLENO
        jc .mal
        push ax
        push dx
        mov bx, ax
        call .lineal
        mov [x_dst], ax
        mov [x_dst+2], dx
        mov bx, [di+X_BASE]
        call .lineal
        mov [x_org], ax
        mov [x_org+2], dx
        mov bx, [di+X_TAM]
        call .lineal
        mov [x_len], ax
        mov [x_len+2], dx
        call x_copia
        pop dx
        pop ax
        mov bl, XE_A20
        jc .mal
        mov [di+X_BASE], ax
.pon:
        mov [di+X_TAM], bp
        mov bx, bp
        clc
.x:
        ret
.mal:
        stc
        ret
; (interna) BX K -> DX:AX octetos
.lineal:
        mov ax, bx
        mov dx, bx
        mov cl, 6
        shr dx, cl
        mov cl, 10
        shl ax, cl
        ret
; (interna) ¿están libres los DX K desde AX (sin contar el bloque BP)? CF=1
; si no
.libre_desde:
        mov bx, x_tabla
        mov cx, N_XMS
.l:
        cmp byte [bx+X_USO], 0
        je .ls
        cmp bx, bp
        je .ls
        cmp word [bx+X_TAM], 0
        je .ls
        mov si, ax
        add si, dx
        cmp [bx+X_BASE], si
        jae .ls
        mov si, [bx+X_BASE]
        add si, [bx+X_TAM]
        cmp ax, si
        jae .ls
        stc
        ret
.ls:
        add bx, X_ENT
        loop .l
        clc
        ret

; 0B: mover. DS:SI (de quien llama) -> dd cuántos octetos (par), dw el bloque
; de origen y dd dónde dentro de él, dw el de destino y dd dónde. Un bloque 0
; es la memoria convencional, y su «dónde», una dirección segmento:desplazamiento
x0B:
        mov es, [x_ds]
        mov si, [x_si]
        mov ax, [es:si]
        mov dx, [es:si+2]
        mov bl, XE_LARGO
        test al, 1
        jnz .mal
        mov [x_len], ax
        mov [x_len+2], dx
        add si, 4
        call .extremo
        jc .mal
        mov [x_org], ax
        mov [x_org+2], dx
        add si, 6
        call .extremo
        jc .mal
        mov [x_dst], ax
        mov [x_dst+2], dx
        push cs
        pop es
        call x_copia
        mov bl, XE_A20
        jc .mal
        mov bx, bp
        ret
.mal:
        push cs
        pop es
        stc
        ret
; (interna) ES:SI -> dw bloque, dd dónde: su dirección de verdad en DX:AX.
; CF=1 y BL=error si el bloque no existe o no llega tan lejos
.extremo:
        mov dx, [es:si]
        test dx, dx
        jnz .bloque
        mov ax, [es:si+4]           ; convencional: segmento x 16 + desplazamiento
        mov dx, ax
        mov cl, 12
        shr dx, cl
        mov cl, 4
        shl ax, cl
        add ax, [es:si+2]
        adc dx, 0
        clc
        ret
.bloque:
        push es
        push cs
        pop es
        call x_asa
        pop es
        jc .x
        mov di, bx
        ; ¿cabe? dónde + cuántos <= su tamaño
        mov ax, [es:si+2]
        mov dx, [es:si+4]
        add ax, [x_len]
        adc dx, [x_len+2]
        mov bl, XE_LARGO
        jc .e
        push ax
        push dx
        mov bx, [di+X_TAM]
        call x0F.lineal
        mov bx, dx                  ; BX:AX = su tamaño
        pop dx
        pop cx                      ; DX:CX = hasta dónde se llega
        cmp dx, bx
        ja .e0
        jb .cabe
        cmp cx, ax
        ja .e0
.cabe:
        mov bx, [di+X_BASE]
        call x0F.lineal
        add ax, [es:si+2]
        adc dx, [es:si+4]
        clc
.x:
        ret
.e0:
        mov bl, XE_LARGO
.e:
        stc
        ret

; copia [x_len] octetos de la dirección [x_org] a la [x_dst] (de 32 bits), en
; trozos de 32 K, con la BIOS (INT 15h/87h), a la que se llama desde el
; puente. CF=1 si la BIOS no puede
x_copia:
        push di
        push bp
.trozo:
        mov cx, [x_len]             ; cuánto esta vez: hasta 8000h octetos
        cmp word [x_len+2], 0
        jne .max
        test cx, cx
        jz .hecho
        cmp cx, 0x8000
        jbe .va
.max:
        mov cx, 0x8000
.va:
        mov bp, cx
        ; la tabla que quiere la BIOS: seis descriptores; el tercero dice de
        ; dónde y el cuarto, adónde
        mov es, [bajo_seg]
        mov di, b_gdt
        push cx
        mov cx, 24
        xor ax, ax
        rep stosw
        pop cx
        mov di, b_gdt + 0x10
        mov si, x_org
        call .descriptor
        mov di, b_gdt + 0x18
        mov si, x_dst
        call .descriptor
        shr cx, 1                   ; en palabras
        mov si, b_gdt
        mov ah, 0x87
        call far [p_mueve]
        push cs
        pop es
        jc .x
        add [x_org], bp
        adc word [x_org+2], 0
        add [x_dst], bp
        adc word [x_dst+2], 0
        sub [x_len], bp
        sbb word [x_len+2], 0
        jmp .trozo
.hecho:
        clc
.x:
        push cs
        pop es
        pop bp
        pop di
        ret
; (interna) en ES:DI, el descriptor de 64 K que empieza en la dirección de [SI]
.descriptor:
        mov word [es:di], 0xFFFF
        mov ax, [si]
        mov [es:di+2], ax
        mov ax, [si+2]
        mov [es:di+4], al
        mov byte [es:di+5], 0x93
        mov [es:di+7], ah
        ret

m_sin_a20   db "No se puede abrir la puerta A20: el sistema se queda en la memoria baja.", 10, 0
