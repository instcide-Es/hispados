; =============================================================================
;  Hispa-DOS APAGAR.COM y REINICIAR.COM (en el disco REINICIA.COM: el
;  intérprete corta el nombre a 8 letras) - apagar o reiniciar el equipo
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El mismo fuente da los dos programas: con -DREINICIAR, REINICIAR.
;
;    APAGAR    [/INFO] [/NOACPI] [/NOAPM] [/SOLO:ACPI|APM|MENSAJE]
;    REINICIAR [/INFO] [/NOACPI] [/SOLO:ACPI|TECLADO|CF9|FALLO]
;
;  Antes de nada, los dos escriben en el disco lo que quede pendiente:
;  INT 21h función 0Dh (vaciar los búferes de DOS) y, si hay una caché de
;  disco residente con la interfaz de SMARTDRV (INT 2Fh AX=4A10h), su orden de
;  escribirlo todo; después esperan unos instantes (2 s) para que los discos
;  acaben.
;
;  APAGAR (hecho con las especificaciones públicas de ACPI y APM 1.2):
;   a) ACPI. Se busca la RSDP («RSD PTR », en la EBDA o entre E0000h y
;      FFFFFh), la RSDT (o la XSDT) y en ella la FADT (firma «FACP»): de ahí
;      salen PM1a_CNT_BLK y PM1b_CNT_BLK (o sus X_ si son de E/S), SMI_CMD,
;      ACPI_ENABLE y la DSDT. En la DSDT (y si no, en las SSDT) se busca el
;      objeto \_S5_: «Name(_S5_, Package(){a, b, ...})» en AML es 08h
;      (NameOp), el nombre «_S5_» (con «\» delante, a veces), 12h
;      (PackageOp), la longitud del paquete (1 a 4 octetos), el número de
;      elementos y los elementos: 00h (Zero), 01h (One), FFh (Ones), 0Ah
;      octeto, 0Bh palabra, 0Ch doble palabra o 0Eh cuádruple. Los dos
;      primeros son SLP_TYPa y SLP_TYPb.
;      Si ACPI no está activo (SCI_EN, bit 0 de PM1a_CNT, a 0) y la FADT
;      dice cómo activarlo, se escribe ACPI_ENABLE en el puerto SMI_CMD y se
;      espera (hasta 3 s) a que se encienda SCI_EN. Después se escribe en
;      PM1a_CNT (y en PM1b_CNT, si lo hay) SLP_TYPx en los bits 10-12, primero
;      sin SLP_EN (bit 13) y luego con él: el equipo pasa al estado S5
;      (apagado). Si a los 5 s sigue encendido, se prueba lo siguiente (y si
;      ACPI lo había activado APAGAR, antes se desactiva con ACPI_DISABLE, para
;      dejarle la gestión a la APM de la BIOS).
;   b) APM: INT 15h AX=5300h (¿está?), 5301h (conectar en modo real),
;      530Eh (somos de la versión 1.2; si no la acepta, 1.1), 5308h (gestión
;      de energía activada) y 5307h BX=0001h CX=0003h (todo apagado).
;      Antes de esto se pone ya la pantalla final: hay BIOS cuya función de
;      apagar no vuelve aunque no apague.
;   c) La pantalla final: «Ya puede apagar el equipo.» en letras grandes
;      (dibujadas aquí, con medios bloques del modo de texto), en naranja
;      sobre negro y centrado, y el procesador parado (CLI y HLT en bucle).
;
;  REINICIAR, por este orden (lo que no reinicia en medio segundo, se pasa
;  al siguiente):
;   1. el RESET_REG de la FADT (ACPI 2.0 y siguientes, si la FADT lo tiene):
;      en E/S, en memoria o en la configuración PCI, con RESET_VALUE;
;   2. el controlador del teclado (8042): la orden FEh en el puerto 64h;
;   3. el puerto CF9h (Reset Control de los chipsets PCI): 02h y luego 06h;
;   4. un triple fallo: una tabla de interrupciones vacía y una interrupción.
;   La palabra 0040:0072h queda en 1234h (reinicio en caliente, sin la
;   prueba de memoria), como hace Ctrl+Alt+Supr.
;
;  /INFO no apaga ni reinicia: dice qué ha encontrado (tablas, \_S5,
;  puertos, APM) y qué haría, para probar en equipos de verdad. /NOxxx quita
;  un método y /SOLO:xxx prueba solo uno (con REINICIAR, si ese no reinicia,
;  lo dice y vuelve a DOS; con APAGAR, siempre acaba en la pantalla final).
; =============================================================================
        cpu 586
        bits 16
        org 0x100

BLQ         equ 4096                ; trozo de tabla que se mira de cada vez
TABLAS_MAX  equ 64                  ; entradas de la RSDT/XSDT que se miran
SSDT_MAX    equ 16
FADT_MAX    equ 276

%macro PR 1+
        call pr_inline
        db %1, 0
%endmacro
%define NL 13, 10

start:
        cld
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .argfin
        cmp al, '/'
        jne .arg
        lodsb
        cmp al, '?'
        je ayuda
        dec si
        mov di, si
        mov bx, opciones
.op:
        mov si, di
        mov cx, [bx]
        jcxz .mala
        push bx
        add bx, 4
.oc:
        mov al, [si]
        call upcase
        cmp al, [bx]
        jne .on
        inc si
        inc bx
        loop .oc
        pop bx
        mov al, [si]                ; tras la opción: fin, espacio o «/»
        cmp al, ' '
        jbe .ok
        cmp al, '/'
        jne .no
.ok:
        mov al, [bx+3]              ; el valor
        cmp byte [bx+2], 1          ; y qué se hace con él
        je .o1
        ja .o2
        mov byte [opt_info], 1
        jmp .arg
.o1:
        not al                      ; /NOxxx: ese método, fuera
        and [metodos], al
        jmp .arg
.o2:
        mov [metodos], al           ; /SOLO:xxx: solo ese
        jmp .arg
.on:
        pop bx
.no:
        add bx, [bx]
        add bx, 4
        jmp .op
.mala:
        PR "Opción no válida: /"
        mov si, di
.m1:
        lodsb
        cmp al, ' '
        jbe .m2
        call putc
        jmp .m1
.m2:
        call crlf
        mov si, h_uso
        call puts
        mov ax, 0x4C01
        int 0x21
.argfin:
        smsw ax
        and al, 1
        mov [v86], al
        call acpi_buscar
%ifndef REINICIAR
        call apm_buscar
%endif
        cmp byte [opt_info], 0
        je .hacer
        call informe
        mov ax, 0x4C00
        int 0x21
.hacer:
        PR "Escribiendo en el disco lo que queda pendiente...", NL
        call vaciar
%ifdef REINICIAR
        jmp reiniciar
%else
        jmp apagar
%endif

ayuda:
        mov si, h_ayuda
        call puts
        mov ax, 0x4C00
        int 0x21

; los métodos (bits de «metodos»)
M_ACPI      equ 1
M_APM       equ 2                   ; (APAGAR)
M_TECLADO   equ 2                   ; (REINICIAR)
M_CF9       equ 4
M_FALLO     equ 8
; las opciones: longitud, qué se hace (0 /INFO, 1 quitar un método, 2 solo
; ese método), el método y el texto
%macro OPCION 3
        dw %%f - %%t
        db %1, %2
%%t:    db %3
%%f:
%endmacro
opciones:
        OPCION 0, 0, "INFO"
        OPCION 1, M_ACPI, "NOACPI"
        OPCION 2, M_ACPI, "SOLO:ACPI"
%ifndef REINICIAR
        OPCION 1, M_APM, "NOAPM"
        OPCION 2, M_APM, "SOLO:APM"
        OPCION 2, 0, "SOLO:MENSAJE"
%else
        OPCION 2, M_TECLADO, "SOLO:TECLADO"
        OPCION 2, M_CF9, "SOLO:CF9"
        OPCION 2, M_FALLO, "SOLO:FALLO"
%endif
        dw 0

; =============================================================================
;  Vaciar los búferes y la caché de disco, y esperar unos instantes
; =============================================================================
vaciar:
        mov ah, 0x0D                ; los búferes de DOS
        int 0x21
        mov ax, 0x4A10              ; ¿una caché con la interfaz de SMARTDRV?
        xor bx, bx
        int 0x2F
        cmp ax, 0xBABE
        jne .sin
        mov ax, 0x4A10              ; que lo escriba todo
        mov bx, 1
        int 0x2F
        mov byte [hubo_cache], 1
.sin:
        mov ah, 0x0D                ; (otra vez, por si la caché tenía algo)
        int 0x21
        mov cx, 36                  ; unos 2 s para que los discos acaben
        jmp espera

; CX = tics del reloj de la BIOS (18,2 por segundo) que hay que esperar
espera:
        push es
        push eax
        push ebx
        sti
        xor ax, ax
        mov es, ax
        mov ebx, [es:0x46C]
.l:
        hlt                         ; (hasta la siguiente interrupción: el
        mov eax, [es:0x46C]         ; procesador descansa mientras espera)
        sub eax, ebx
        jb .x                       ; (medianoche)
        cmp ax, cx
        jb .l
.x:
        pop ebx
        pop eax
        pop es
        ret

; =============================================================================
;  ACPI: las tablas
; =============================================================================
acpi_buscar:
        ; la RSDP: en el primer KB de la EBDA o entre E0000h y FFFFFh
        xor ax, ax
        mov es, ax
        mov ax, [es:0x40E]
        cmp ax, 0x8000
        jb .rom
        cmp ax, 0xA000
        jae .rom
        mov es, ax
        mov cx, 64
        call .scan
        jnc .found
.rom:
        mov ax, 0xE000
        mov es, ax
        mov cx, 0x1000
        call .scan
        jnc .found
        mov ax, 0xF000
        mov es, ax
        mov cx, 0x1000
        call .scan
        jnc .found
        push cs
        pop es
        ret
.found:
        push ds
        push es
        pop ds
        mov si, di
        push cs
        pop es
        mov di, rsdp
        mov cx, 36
        rep movsb
        mov ax, ds
        pop ds
        movzx eax, ax
        shl eax, 4
        sub si, 36
        movzx esi, si
        add eax, esi
        mov [a_rsdp], eax
        mov byte [hay_acpi], 1
        ; la tabla raíz: XSDT (entradas de 8 octetos) si la hay; si no, RSDT
        mov byte [a_ent], 4
        mov eax, [rsdp+16]
        cmp byte [rsdp+15], 2
        jb .root
        cmp dword [rsdp+28], 0
        jne .root
        cmp dword [rsdp+24], 0
        je .root
        mov eax, [rsdp+24]
        mov byte [a_ent], 8
.root:
        mov [a_root], eax
        test eax, eax
        jz .x
        mov esi, eax
        mov di, rootbuf
        mov cx, 36 + TABLAS_MAX * 8
        call physcopy
        jc .x
        cmp dword [rootbuf], "RSDT"
        je .r2
        cmp dword [rootbuf], "XSDT"
        jne .x
.r2:
        mov eax, [rootbuf+4]
        sub eax, 36
        jb .x
        movzx ecx, byte [a_ent]
        xor edx, edx
        div ecx
        cmp eax, TABLAS_MAX
        jbe .n1
        mov eax, TABLAS_MAX
.n1:
        mov [a_n], ax
        xor bx, bx
.tl:
        cmp bx, [a_n]
        jae .te
        push bx
        movzx ax, byte [a_ent]
        mul bx
        mov si, ax
        mov eax, [rootbuf+36+si]
        cmp byte [a_ent], 8
        jne .t4
        cmp dword [rootbuf+40+si], 0
        jne .tn                     ; (por encima de 4 GB: no se mira)
.t4:
        test eax, eax
        jz .tn
        mov [a_tmp], eax
        mov esi, eax
        mov di, hdrbuf
        mov cx, 36
        call physcopy
        jc .tn
        cmp dword [hdrbuf], "FACP"
        jne .t5
        cmp dword [a_fadt], 0       ; (la primera)
        jne .tn
        mov eax, [a_tmp]
        mov [a_fadt], eax
        mov esi, eax
        mov di, fadt
        mov cx, FADT_MAX
        call physcopy
        jmp .tn
.t5:
        cmp dword [hdrbuf], "SSDT"
        jne .tn
        movzx si, byte [a_nssdt]
        cmp si, SSDT_MAX
        jae .tn
        shl si, 2
        mov eax, [a_tmp]
        mov [a_ssdt+si], eax
        inc byte [a_nssdt]
.tn:
        pop bx
        inc bx
        jmp .tl
.te:
        cmp dword [a_fadt], 0
        je .x
        call fadt_leer
        call s5_buscar
.x:
        ret
.scan:
        xor di, di
.s:
        cmp dword [es:di], 0x20445352       ; "RSD "
        jne .sn
        cmp dword [es:di+4], 0x20525450     ; "PTR "
        jne .sn
        push cx
        push di                     ; la suma de los 20 primeros octetos
        mov cx, 20
        xor al, al
.su:
        add al, [es:di]
        inc di
        loop .su
        pop di
        pop cx
        test al, al
        jnz .sn
        clc
        ret
.sn:
        add di, 16
        loop .s
        stc
        ret

; los campos de la FADT que hacen falta (ACPI 1.0 a 6.x)
fadt_leer:
        mov eax, [fadt+4]           ; la longitud (solo se ha leído FADT_MAX)
        cmp eax, FADT_MAX
        jbe .l
        mov eax, FADT_MAX
.l:
        mov [f_len], ax
        mov eax, [fadt+40]          ; la DSDT: X_DSDT si la hay (y cabe en 32 bits)
        cmp word [f_len], 148
        jb .d
        cmp dword [fadt+144], 0
        jne .d
        cmp dword [fadt+140], 0
        je .d
        mov eax, [fadt+140]
.d:
        mov [a_dsdt], eax
        mov eax, [fadt+48]
        mov [f_smi], ax
        mov ax, [fadt+52]
        mov [f_en], ax              ; ACPI_ENABLE y ACPI_DISABLE
        ; PM1a_CNT, PM1b_CNT y PM1a_EVT: los de 32 bits o, si no, los X_
        ; (si son de E/S)
        mov bx, 64
        mov si, 172
        call .puerto
        mov [f_pm1a], ax
        mov bx, 68
        mov si, 184
        call .puerto
        mov [f_pm1b], ax
        mov bx, 56
        mov si, 148
        call .puerto
        mov [f_evta], ax
        mov bx, 60
        mov si, 160
        call .puerto
        mov [f_evtb], ax
        mov al, [fadt+88]
        mov [f_evtlen], al
        ; RESET_REG (desde la revisión 2): GAS en 116, RESET_VALUE en 128
        cmp word [f_len], 129
        jb .x
        cmp byte [fadt+8], 2
        jb .x
        mov eax, [fadt+120]
        or eax, [fadt+124]
        jz .x
        mov byte [f_rst], 1
        mov eax, [fadt+112]         ; banderas: bit 10 RESET_REG_SUP
        mov [f_flags], eax
.x:
        ret
; BX = el campo de 32 bits, SI = su GAS -> AX = el puerto (0: no hay)
.puerto:
        mov eax, [fadt+bx]
        test eax, eax
        jnz .p
        cmp [f_len], si
        jbe .p0
        cmp byte [fadt+si], 1       ; espacio de E/S
        jne .p0
        mov eax, [fadt+si+4]
        test eax, eax
        jnz .p
.p0:
        xor eax, eax
.p:
        cmp eax, 0xFFFF
        jbe .p1
        xor eax, eax
.p1:
        ret

; el paquete \_S5_ en la DSDT o en las SSDT
s5_buscar:
        mov eax, [a_dsdt]
        mov byte [s5_donde], 1
        call s5_tabla
        jnc .x
        xor bx, bx
.l:
        cmp bl, [a_nssdt]
        jae .no
        push bx
        mov [s5_donde], bl
        add byte [s5_donde], 2
        shl bx, 2
        mov eax, [a_ssdt+bx]
        call s5_tabla
        pop bx
        jnc .x
        inc bx
        jmp .l
.no:
        mov byte [s5_donde], 0
.x:
        ret

; EAX = dirección de una tabla AML: CF=0 y s5_a, s5_b, s5_pos si tiene \_S5_
s5_tabla:
        test eax, eax
        jz .no
        mov [t_dir], eax
        mov esi, eax
        mov di, hdrbuf
        mov cx, 36
        call physcopy
        jc .no
        mov ecx, [hdrbuf+4]
        cmp ecx, 36
        jbe .no
        cmp ecx, 0x400000           ; (más de 4 MB: no es una tabla)
        ja .no
        mov [t_len], ecx
        mov dword [t_pos], 36
.trozo:
        mov eax, [t_pos]
        cmp eax, [t_len]
        jae .no
        mov esi, [t_dir]            ; (con 8 octetos de antes y 64 de después,
        add esi, eax                ; por si cae entre dos trozos)
        sub esi, 8
        mov di, blq
        mov cx, BLQ + 72
        call physcopy
        jc .no
        mov ecx, [t_len]            ; lo que queda de la tabla, hasta BLQ
        sub ecx, [t_pos]
        cmp ecx, BLQ
        jbe .c
        mov ecx, BLQ
.c:
        mov si, blq + 8
.b:
        cmp dword [si], "_S5_"
        jne .sig
        call s5_paquete
        jnc .si
.sig:
        inc si
        dec cx
        jnz .b
        add dword [t_pos], BLQ
        jmp .trozo
.si:
        movzx eax, si
        sub eax, blq + 8
        add eax, [t_pos]
        mov [s5_pos], eax
        clc
        ret
.no:
        stc
        ret

; SI = «_S5_» dentro de blq: ¿es «Name(_S5_, Package(){...})»? -> CF=0 y
; s5_a, s5_b
s5_paquete:
        push si
        push cx
        mov al, [si-1]
        cmp al, 0x08                ; NameOp
        je .n
        cmp al, '\'                 ; (\_S5_: la raíz delante)
        jne .no
        cmp byte [si-2], 0x08
        jne .no
.n:
        add si, 4
        lodsb
        cmp al, 0x12                ; PackageOp
        jne .no
        lodsb                       ; PkgLength: los bits 6-7 dicen cuántos
        shr al, 6                   ; octetos más lleva
        movzx ax, al
        add si, ax
        lodsb                       ; NumElements
        mov [s5_n], al
        test al, al
        jz .no
        call aml_entero
        jc .no
        mov [s5_a], al
        mov byte [s5_b], 0
        cmp byte [s5_n], 2
        jb .si
        call aml_entero
        jc .no
        mov [s5_b], al
.si:
        pop cx
        pop si
        clc
        ret
.no:
        pop cx
        pop si
        stc
        ret

; SI = un entero en AML -> AL (sus 8 bits bajos), SI detrás; CF si no lo es
aml_entero:
        lodsb
        cmp al, 0x00                ; ZeroOp
        je .ok
        cmp al, 0x01                ; OneOp
        je .ok
        cmp al, 0xFF                ; OnesOp
        je .ok
        cmp al, 0x0A                ; BytePrefix
        je .b
        cmp al, 0x0B                ; WordPrefix
        je .w
        cmp al, 0x0C                ; DWordPrefix
        je .d
        cmp al, 0x0E                ; QWordPrefix
        je .q
        stc
        ret
.b:
        lodsb
        jmp .ok
.w:
        lodsb
        inc si
        jmp .ok
.d:
        lodsb
        add si, 3
        jmp .ok
.q:
        lodsb
        add si, 7
.ok:
        clc
        ret

%ifndef REINICIAR
; =============================================================================
;  APAGAR
; =============================================================================
apagar:
        PR "Apagando el equipo...", NL
        test byte [metodos], M_ACPI
        jz .apm
        call acpi_apagar
.apm:
        call pantalla_final
        test byte [metodos], M_APM
        jz parar
        call apm_apagar
parar:
        smsw ax                     ; en V86 (un gestor de memoria), CLI y HLT
        test al, 1                  ; los atendería él: se espera sin más
        jnz .v86
.l:
        cli
        hlt
        jmp .l
.v86:
        sti
        hlt
        jmp .v86

; ACPI: estado S5 (vuelve si no se puede o si el equipo sigue encendido)
acpi_apagar:
        cmp word [f_pm1a], 0
        je .x
        cmp byte [s5_donde], 0
        je .x
        mov dx, [f_pm1a]            ; ¿ACPI activo?
        in ax, dx
        test al, 1
        jnz .activo
        mov dx, [f_smi]
        test dx, dx
        jz .activo                  ; (sin SMI_CMD: siempre activo)
        mov al, [f_en]
        test al, al
        jz .activo
        out dx, al                  ; ACPI_ENABLE -> SMI_CMD
        mov byte [acpi_puesto], 1
        xor ax, ax
        mov es, ax
        mov ebx, [es:0x46C]
        sti
.esp:
        mov dx, [f_pm1a]
        in ax, dx
        test al, 1
        jnz .activo
        mov eax, [es:0x46C]
        sub eax, ebx
        jb .activo
        cmp eax, 55                 ; 3 s
        jb .esp
.activo:
        push cs
        pop es
        mov dx, [f_evta]            ; WAK_STS (bit 15 del estado) a cero
        test dx, dx
        jz .w
        mov ax, 0x8000
        out dx, ax
.w:
        cli
        mov dx, [f_pm1a]
        mov bl, [s5_a]
        call .tipo
        mov dx, [f_pm1b]
        mov bl, [s5_b]
        call .tipo
        mov dx, [f_pm1a]
        mov bl, [s5_a]
        call .dormir
        mov dx, [f_pm1b]
        mov bl, [s5_b]
        call .dormir
        sti
        mov cx, 91                  ; si a los 5 s seguimos aquí, no ha ido
        call espera
        cmp byte [acpi_puesto], 0   ; ACPI, como estaba (para la APM de la
        je .x                       ; BIOS): ACPI_DISABLE -> SMI_CMD
        mov dx, [f_smi]
        mov al, [f_en+1]
        test al, al
        jz .x
        out dx, al
        mov cx, 9
        call espera
.x:
        ret
; DX = PM1x_CNT (0: no hay), BL = SLP_TYPx: primero el tipo, luego SLP_EN
.tipo:
        test dx, dx
        jz .t9
        in ax, dx
        and ax, 0xC3FF              ; SLP_TYP y SLP_EN fuera
        and bx, 7
        shl bx, 10
        or ax, bx
        out dx, ax
.t9:
        ret
.dormir:
        test dx, dx
        jz .t9
        in ax, dx
        and ax, 0xC3FF
        and bx, 7
        shl bx, 10
        or ax, bx
        or ax, 0x2000               ; SLP_EN
        out dx, ax
        ret

; APM: ¿está? -> apm_hay, apm_ver, apm_band
apm_buscar:
        mov ax, 0x5300
        xor bx, bx
        int 0x15
        jc .x
        cmp bx, 0x504D              ; «PM»
        jne .x
        mov byte [apm_hay], 1
        mov [apm_ver], ax
        mov [apm_band], cx
.x:
        ret

apm_apagar:
        cmp byte [apm_hay], 0
        je .x
        mov ax, 0x5301              ; conectar en modo real (si ya lo estaba,
        xor bx, bx                  ; da error 02h y se sigue)
        int 0x15
        mov ax, 0x530E              ; somos de la versión 1.2
        xor bx, bx
        mov cx, 0x0102
        int 0x15
        jnc .v
        mov ax, 0x530E              ; o de la 1.1
        xor bx, bx
        mov cx, 0x0101
        int 0x15
.v:
        mov ax, 0x5308              ; gestión de energía activada (todo)
        mov bx, 0x0001
        mov cx, 0x0001
        int 0x15
        mov ax, 0x5307              ; todo apagado
        mov bx, 0x0001
        mov cx, 0x0003
        int 0x15
        mov cx, 55
        call espera
.x:
        ret

; ---- la pantalla final ----
pantalla_final:
        mov si, m_final             ; (también por DOS: se ve en la consola serie)
        call puts
        call crlf
        mov ah, 0x0F
        int 0x10
        mov word [pseg], 0xB800
        cmp al, 7
        jne .m
        mov word [pseg], 0xB000
        jmp .m2
.m:
        mov ax, 0x0003
        int 0x10
        mov ax, 0x1010              ; el color 6 (DAC 14h), naranja
        mov bx, 0x0014
        mov dh, 63
        mov ch, 34
        mov cl, 0
        int 0x10
.m2:
        mov ah, 0x01                ; sin cursor
        mov cx, 0x2000
        int 0x10
        mov es, [pseg]
        xor di, di                  ; todo a negro
        mov cx, 80 * 25
        mov ax, 0x0720
        rep stosw
        mov si, l_final
        mov byte [fila], 3
.lin:
        cmp byte [si], 0            ; (cada línea: su texto, acabado en 0)
        je .fin
        call linea_grande
        add byte [fila], 7
.sl:
        lodsb
        test al, al
        jnz .sl
        jmp .lin
.fin:
        push cs
        pop es
        ret

; SI = texto (con letras de fuente_g), en la fila [fila], centrado
linea_grande:
        push si
        xor dx, dx                  ; el ancho
.an:
        lodsb
        test al, al
        jz .an2
        call glifo
        jc .an
        movzx ax, byte [bx]
        inc ax
        add dx, ax
        jmp .an
.an2:
        dec dx
        mov ax, 80
        sub ax, dx
        sar ax, 1
        jns .x0
        xor ax, ax
.x0:
        mov [col], ax
        pop si
        push si
.l:
        lodsb
        test al, al
        jz .x
        call glifo
        jc .l
        push si
        call glifo_dibujar
        pop si
        movzx ax, byte [bx]
        inc ax
        add [col], ax
        jmp .l
.x:
        pop si
        ret

; AL = letra -> BX = su glifo (ancho, 10 filas); CF si no está
glifo:
        mov bx, fuente_g
.b:
        cmp byte [bx], 0
        je .no
        cmp [bx], al
        je .si
        add bx, 12
        jmp .b
.si:
        inc bx
        clc
        ret
.no:
        stc
        ret

; BX = glifo: en ES (la pantalla), desde [col] y [fila]; cada celda, dos
; puntos del glifo (arriba y abajo): ▀ ▄ █ o nada
glifo_dibujar:
        movzx ax, byte [fila]
        imul di, ax, 160
        mov ax, [col]
        shl ax, 1
        add di, ax
        lea si, [bx+1]
        mov cx, 5
.f:
        push cx
        push di
        mov dl, [si]                ; la fila de arriba
        mov dh, [si+1]              ; y la de abajo
        movzx cx, byte [bx]
.c:
        xor al, al
        shl dl, 1
        rcl al, 1
        shl dh, 1
        rcl al, 1
        push bx
        movzx bx, al
        mov al, [celdas+bx]
        pop bx
        mov ah, 0x06
        stosw
        loop .c
        pop di
        pop cx
        add di, 160
        add si, 2
        loop .f
        ret
celdas      db ' ', 0xDC, 0xDF, 0xDB    ; (arriba, abajo): nada, ▄, ▀, █

m_final     db "Ya puede apagar el equipo.", 0
l_final     db "Ya puede", 0, "apagar", 0, "el equipo.", 0, 0

; Las letras grandes, dibujadas aquí: la letra, el ancho y 10 filas (el
; punto de la izquierda en el bit 7)
fuente_g:
        db 'Y', 8
        db 11000011b, 11000011b, 01100110b, 00111100b, 00011000b
        db 00011000b, 00011000b, 0, 0, 0
        db 'a', 6
        db 0, 0, 01111000b, 00001100b, 01111100b
        db 11001100b, 01111100b, 0, 0, 0
        db 'p', 6
        db 0, 0, 11111000b, 11001100b, 11001100b
        db 11001100b, 11111000b, 11000000b, 11000000b, 0
        db 'u', 6
        db 0, 0, 11001100b, 11001100b, 11001100b
        db 11001100b, 01111100b, 0, 0, 0
        db 'e', 6
        db 0, 0, 01111000b, 11001100b, 11111100b
        db 11000000b, 01111000b, 0, 0, 0
        db 'd', 6
        db 00001100b, 00001100b, 01111100b, 11001100b, 11001100b
        db 11001100b, 01111100b, 0, 0, 0
        db 'g', 6
        db 0, 0, 01111100b, 11001100b, 11001100b
        db 11001100b, 01111100b, 00001100b, 01111000b, 0
        db 'r', 6
        db 0, 0, 11011000b, 11101100b, 11000000b
        db 11000000b, 11000000b, 0, 0, 0
        db 'l', 4
        db 11100000b, 01100000b, 01100000b, 01100000b, 01100000b
        db 01100000b, 11110000b, 0, 0, 0
        db 'q', 6
        db 0, 0, 01111100b, 11001100b, 11001100b
        db 11001100b, 01111100b, 00001100b, 00001100b, 0
        db 'i', 4
        db 01100000b, 0, 11100000b, 01100000b, 01100000b
        db 01100000b, 11110000b, 0, 0, 0
        db 'o', 6
        db 0, 0, 01111000b, 11001100b, 11001100b
        db 11001100b, 01111000b, 0, 0, 0
        db '.', 2
        db 0, 0, 0, 0, 0
        db 11000000b, 11000000b, 0, 0, 0
        db ' ', 2
        db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        db 0

%else
; =============================================================================
;  REINICIAR
; =============================================================================
reiniciar:
        PR "Reiniciando el equipo...", NL
        xor ax, ax                  ; reinicio en caliente (sin prueba de memoria)
        mov es, ax
        mov word [es:0x472], 0x1234
        push cs
        pop es
        ; 1. el RESET_REG de ACPI
        test byte [metodos], M_ACPI
        jz .kbd
        cmp byte [f_rst], 0
        je .kbd
        PR "  registro de reinicio de ACPI", NL
        call reset_reg
        mov cx, 9
        call espera
.kbd:
        ; 2. el controlador del teclado: FEh (pulso en la línea de reinicio)
        test byte [metodos], M_TECLADO
        jz .cf9
        PR "  controlador del teclado", NL
        mov cx, 0xFFFF
.k1:
        in al, 0x64
        test al, 2                  ; que no esté ocupado (con un tope)
        jz .k2
        loop .k1
.k2:
        mov al, 0xFE
        out 0x64, al
        mov cx, 9
        call espera
.cf9:
        ; 3. el puerto CF9h: primero 02h y luego 06h (reinicio del procesador)
        test byte [metodos], M_CF9
        jz .fallo
        PR "  puerto CF9h", NL
        mov dx, 0xCF9
        mov al, 0x02
        out dx, al
        out 0x80, al                ; (un instante)
        mov al, 0x06
        out dx, al
        mov cx, 9
        call espera
.fallo:
        ; 4. triple fallo: sin tabla de interrupciones, una interrupción
        test byte [metodos], M_FALLO
        jz .nada
        PR "  triple fallo", NL
        cli
        lidt [idt_nula]
        int 3
        jmp $
.nada:
        PR "No se ha podido reiniciar el equipo. Pulse el botón de reinicio.", NL
        mov ax, 0x4C01
        int 0x21

; el RESET_REG (GAS en fadt+116) <- RESET_VALUE (fadt+128)
reset_reg:
        mov al, [fadt+116]          ; el espacio
        mov bl, [fadt+128]
        cmp al, 1
        je .io
        cmp al, 0
        je .mem
        cmp al, 2
        je .pci
        ret
.io:
        mov dx, [fadt+120]
        mov al, bl
        out dx, al
        ret
.mem:
        cmp byte [v86], 0           ; (en V86 no se puede escribir así)
        jne .x
        mov esi, [fadt+120]
        call a20_open
        call unreal
        mov [fs:esi], bl
        call a20_restore
.x:
        ret
.pci:
        ; dirección: dispositivo en los bits 32-47, función en 16-31 y
        ; registro en 0-15 (bus 0)
        movzx eax, word [fadt+124]  ; dispositivo
        and eax, 31
        shl eax, 11
        movzx ecx, word [fadt+122]  ; función
        and ecx, 7
        shl ecx, 8
        or eax, ecx
        movzx ecx, word [fadt+120]  ; registro
        mov si, cx
        and cl, 0xFC
        or eax, ecx
        or eax, 0x80000000
        mov dx, 0xCF8
        out dx, eax
        mov dx, si
        and dx, 3
        add dx, 0xCFC
        mov al, bl
        out dx, al
        ret
idt_nula    dw 0
            dd 0
%endif

; =============================================================================
;  /INFO
; =============================================================================
informe:
%ifdef REINICIAR
        PR "REINICIAR: lo que hay en este equipo", NL
%else
        PR "APAGAR: lo que hay en este equipo", NL
%endif
        PR "Procesador en modo "
        cmp byte [v86], 0
        jne .v
        PR "real", NL
        jmp .v2
.v:
        PR "V86 (con un gestor de memoria)", NL
.v2:
        cmp byte [hay_acpi], 0
        jne .acpi
        PR "ACPI: no hay (no se encuentra la RSDP)", NL
        jmp .apm
.acpi:
        PR "ACPI: RSDP en "
        mov eax, [a_rsdp]
        call hex8
        PR "h, revisión "
        movzx eax, byte [rsdp+15]
        call putnum
        PR ", OEM "
        mov si, rsdp + 9
        mov cx, 6
        call puts_n
        call crlf
        PR "  tabla raíz ("
        mov si, rsdt_n
        cmp byte [a_ent], 4
        je .r
        mov si, xsdt_n
.r:
        call puts
        PR ") en "
        mov eax, [a_root]
        call hex8
        PR "h, "
        movzx eax, word [a_n]
        call putnum
        PR " tablas", NL
        cmp dword [a_fadt], 0
        jne .f
        PR "  FADT: no hay", NL
        jmp .apm
.f:
        PR "  FADT en "
        mov eax, [a_fadt]
        call hex8
        PR "h, revisión "
        movzx eax, byte [fadt+8]
        call putnum
        PR ", "
        mov eax, [fadt+4]
        call putnum
        PR " octetos", NL
        PR "  DSDT en "
        mov eax, [a_dsdt]
        call hex8
        PR "h; SSDT: "
        movzx eax, byte [a_nssdt]
        call putnum
        call crlf
        PR "  SMI_CMD "
        mov ax, [f_smi]
        call puthex4
        PR "h, ACPI_ENABLE "
        mov al, [f_en]
        call hex2
        PR "h, ACPI_DISABLE "
        mov al, [f_en+1]
        call hex2
        PR "h", NL
        PR "  PM1a_EVT "
        mov ax, [f_evta]
        call .pto
        PR ", PM1b_EVT "
        mov ax, [f_evtb]
        call .pto
        PR ", PM1a_CNT "
        mov ax, [f_pm1a]
        call .pto
        PR ", PM1b_CNT "
        mov ax, [f_pm1b]
        call .pto
        call crlf
        mov dx, [f_pm1a]
        test dx, dx
        jz .s5
        PR "  PM1a_CNT vale ahora "
        in ax, dx
        push ax
        call puthex4
        pop ax
        test al, 1
        jz .noact
        PR "h: ACPI activo (SCI_EN = 1)", NL
        jmp .s5
.noact:
        PR "h: ACPI no activo (SCI_EN = 0)", NL
.s5:
        cmp byte [s5_donde], 0
        jne .s5si
        PR "  \_S5: no se encuentra en la DSDT ni en las SSDT", NL
        jmp .rst
.s5si:
        PR "  \_S5: en la "
        cmp byte [s5_donde], 1
        jne .s5ss
        PR "DSDT"
        jmp .s5p
.s5ss:
        PR "SSDT número "
        movzx eax, byte [s5_donde]
        dec eax
        call putnum
.s5p:
        PR " (desplazamiento "
        mov eax, [s5_pos]
        call hex8
        PR "h), "
        movzx eax, byte [s5_n]
        call putnum
        PR " elementos: SLP_TYPa = "
        mov al, [s5_a]
        call hex2
        PR "h, SLP_TYPb = "
        mov al, [s5_b]
        call hex2
        PR "h", NL
.rst:
        cmp byte [f_rst], 0
        jne .rsi
        PR "  RESET_REG: no hay", NL
        jmp .apm
.rsi:
        PR "  RESET_REG: "
        mov al, [fadt+116]
        mov si, esp_mem
        cmp al, 0
        je .re
        mov si, esp_io
        cmp al, 1
        je .re
        mov si, esp_pci
        cmp al, 2
        je .re
        mov si, esp_otro
.re:
        call puts
        PR " "
        mov eax, [fadt+124]
        test eax, eax
        jz .re2
        call hex8
        PR ":"
.re2:
        mov eax, [fadt+120]
        call hex8
        PR "h <- "
        mov al, [fadt+128]
        call hex2
        PR "h"
        test word [f_flags], 0x400
        jnz .re3
        PR " (la FADT no dice que se pueda usar: RESET_REG_SUP = 0)"
.re3:
        call crlf
.apm:
%ifndef REINICIAR
        cmp byte [apm_hay], 0
        jne .apmsi
        PR "APM: no hay", NL
        jmp .met
.apmsi:
        PR "APM: versión "
        movzx eax, byte [apm_ver+1]
        call putnum
        mov al, '.'
        call putc
        movzx eax, byte [apm_ver]
        call putnum
        PR ", banderas "
        mov ax, [apm_band]
        call puthex4
        PR "h", NL
.met:
        PR "Para apagar: "
        test byte [metodos], M_ACPI
        jz .m1
        cmp word [f_pm1a], 0
        je .m1
        cmp byte [s5_donde], 0
        je .m1
        PR "ACPI (S5); si no apaga, "
.m1:
        test byte [metodos], M_APM
        jz .m2
        cmp byte [apm_hay], 0
        je .m2
        PR "APM; si no apaga, "
.m2:
        PR "el mensaje «Ya puede apagar el equipo.»", NL
%else
        PR "Para reiniciar, por este orden:", NL
        test byte [metodos], M_ACPI
        jz .m1
        cmp byte [f_rst], 0
        je .m1
        PR "  el registro de reinicio de ACPI (RESET_REG)", NL
.m1:
        test byte [metodos], M_TECLADO
        jz .m2
        PR "  el controlador del teclado (64h <- FEh)", NL
.m2:
        test byte [metodos], M_CF9
        jz .m3
        PR "  el puerto CF9h (02h y 06h)", NL
.m3:
        test byte [metodos], M_FALLO
        jz .m4
        PR "  un triple fallo del procesador", NL
.m4:
%endif
        ret
.pto:
        test ax, ax
        jz .pno
        call puthex4
        mov al, 'h'
        jmp putc
.pno:
        PR "no hay"
        ret

rsdt_n      db "RSDT", 0
xsdt_n      db "XSDT", 0
esp_mem     db "memoria", 0
esp_io      db "E/S", 0
esp_pci     db "configuración PCI", 0
esp_otro    db "espacio desconocido", 0

; =============================================================================
;  Memoria física por encima de 1 MB (como EQUIPO)
; =============================================================================
; ESI = dirección física, DI = destino (nuestro segmento), CX = octetos (se
; redondea a 4 por arriba). CF si no se puede.
;  - por debajo de 1 MB: directo;
;  - en modo V86: INT 15h AH=87h (lo atiende el gestor de memoria);
;  - en modo real: modo «irreal» (FS con límite de 4 GB), con A20 abierta.
physcopy:
        pushad
        push ds
        push es
        push cs
        pop es
        movzx ecx, cx
        add cx, 3
        and cx, 0xFFFC
        mov eax, esi
        add eax, ecx
        jc .high
        cmp eax, 0x100000
        ja .high
        mov eax, esi
        shr eax, 4
        mov ds, ax
        and si, 0x0F
        rep movsb
        clc
        jmp .out
.high:
        smsw ax
        test al, 1
        jnz .bios
        call a20_open
        call unreal
        shr cx, 2
.l:
        mov eax, [fs:esi]
        stosd
        add esi, 4
        loop .l
        call a20_restore
        clc
        jmp .out
.bios:
        push di
        push cx
        mov di, i15gdt
        mov cx, 24
        xor ax, ax
        rep stosw
        pop cx
        pop di
        mov word [i15gdt+0x10], 0xFFFF
        mov [i15gdt+0x12], esi
        mov eax, esi
        shr eax, 24
        mov [i15gdt+0x17], al
        mov byte [i15gdt+0x15], 0x93
        mov word [i15gdt+0x18], 0xFFFF
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx edi, di
        add eax, edi
        mov [i15gdt+0x1A], eax
        mov byte [i15gdt+0x1D], 0x93
        mov byte [i15gdt+0x1F], 0
        shr cx, 1
        mov si, i15gdt
        mov ah, 0x87
        int 0x15
.out:
        pop es
        pop ds
        popad
        cld
        ret

unreal:
        push eax
        push bx
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        pushf
        cli
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        popf
        pop bx
        pop eax
        ret
        align 4
gdt_tbl     dq 0
            dq 0x00CF92000000FFFF
gdtr        dw 15
            dd 0

; la línea A20: se abre mientras se lee y se deja como estaba
a20_open:
        pushf
        push ax
        cli
        mov byte [a20_closed], 0
        call a20_test
        jnz .r
        mov byte [a20_closed], 1
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
        call a20_test
        jnz .r
        mov ax, 0x2401
        int 0x15
.r:
        pop ax
        popf
        ret
a20_restore:
        pushf
        push ax
        cli
        cmp byte [a20_closed], 0
        je .r
        mov byte [a20_closed], 0
        in al, 0x92
        and al, 0xFC
        out 0x92, al
        call a20_test
        jz .r
        mov ax, 0x2400
        int 0x15
.r:
        pop ax
        popf
        ret
; ZF = 1 si A20 está cerrada (0000:0500 y FFFF:0510 son el mismo octeto)
a20_test:
        push ds
        push es
        push ax
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov al, [0x500]
        push ax
        mov byte [0x500], 0x5A
        cmp byte [es:0x510], 0x5A
        jne .d
        mov byte [0x500], 0xA5
        cmp byte [es:0x510], 0xA5
.d:
        pop ax
        mov [0x500], al
        pop ax
        pop es
        pop ds
        ret

; =============================================================================
;  Pequeñas
; =============================================================================
pr_inline:
        pop si
        call puts
.f:
        lodsb
        test al, al
        jnz .f
        jmp si

; SI, CX letras (sin cero final)
puts_n:
        lodsb
        call putc
        loop puts_n
        ret

hex8:
        push eax
        shr eax, 16
        call puthex4
        pop eax
        jmp puthex4

hex2:
        push ax
        push cx
        mov cx, 2
        shl ax, 8
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        pop ax
        ret

%include "util.inc"

%ifdef REINICIAR
h_ayuda     db "Reinicia el equipo.", 13, 10, 13, 10
h_uso       db "REINICIAR [/INFO] [/NOACPI | /SOLO:método]", 13, 10, 0
h_resto     equ $
            db 13, 10
            db "Antes escribe en el disco lo que quede pendiente (los búferes de DOS y una", 13, 10
            db "caché de disco, si la hay) y espera unos instantes. Después prueba, por", 13, 10
            db "este orden: el registro de reinicio de ACPI (RESET_REG de la FADT), el", 13, 10
            db "controlador del teclado (orden FEh), el puerto CF9h y, si nada de eso", 13, 10
            db "funciona, un triple fallo del procesador.", 13, 10, 13, 10
            db "  /INFO     dice qué ha encontrado y qué haría, sin reiniciar", 13, 10
            db "  /NOACPI   no usar el registro de reinicio de ACPI", 13, 10
            db "  /SOLO:ACPI, /SOLO:TECLADO, /SOLO:CF9 o /SOLO:FALLO   probar solo ese", 13, 10
            db "            método (para buscar el que sirve en un equipo)", 13, 10, 0
%else
h_ayuda     db "Apaga el equipo.", 13, 10, 13, 10
h_uso       db "APAGAR [/INFO] [/NOACPI] [/NOAPM] [/SOLO:método]", 13, 10, 0
h_resto     equ $
            db 13, 10
            db "Antes escribe en el disco lo que quede pendiente (los búferes de DOS y una", 13, 10
            db "caché de disco, si la hay) y espera unos instantes. Después apaga por ACPI", 13, 10
            db "(el estado S5) y, si el equipo no tiene ACPI o no se apaga, por APM. Si", 13, 10
            db "tampoco, deja en la pantalla «Ya puede apagar el equipo.» y para el", 13, 10
            db "procesador: entonces se apaga con el botón.", 13, 10, 13, 10
            db "  /INFO     dice qué ha encontrado (ACPI, \_S5, puertos, APM) y qué", 13, 10
            db "            haría, sin apagar", 13, 10
            db "  /NOACPI   no usar ACPI", 13, 10
            db "  /NOAPM    no usar APM", 13, 10
            db "  /SOLO:ACPI, /SOLO:APM o /SOLO:MENSAJE   probar solo ese método", 13, 10, 0
%endif

; ---- variables (a cero al cargar: van en el archivo) ----
opt_info    db 0
metodos     db 0xFF
v86         db 0
hubo_cache  db 0
hay_acpi    db 0
a_ent       db 0
a_nssdt     db 0
s5_donde    db 0                    ; 0 no, 1 DSDT, 2.. SSDT número n-1
s5_n        db 0
s5_a        db 0
s5_b        db 0
f_rst       db 0
f_evtlen    db 0
apm_hay     db 0
acpi_puesto db 0                    ; ACPI lo ha activado APAGAR
fila        db 0
a20_closed  db 0
            align 4
a_rsdp      dd 0
a_root      dd 0
a_fadt      dd 0
a_dsdt      dd 0
a_tmp       dd 0
a_n         dw 0
f_len       dw 0
f_smi       dw 0
f_en        dw 0
f_pm1a      dw 0
f_pm1b      dw 0
f_evta      dw 0
f_evtb      dw 0
apm_ver     dw 0
apm_band    dw 0
col         dw 0
pseg        dw 0
f_flags     dd 0
s5_pos      dd 0
t_dir       dd 0
t_len       dd 0
t_pos       dd 0
a_ssdt      times SSDT_MAX dd 0

        absolute $
            alignb 4
rsdp        resb 40
hdrbuf      resb 40
fadt        resb FADT_MAX + 4
i15gdt      resb 48
rootbuf     resb 36 + TABLAS_MAX * 8 + 4
blq         resb BLQ + 72 + 4
u_numbuf    resb 16
u_lbuf      resb 84
