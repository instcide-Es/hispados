; =============================================================================
;  HSPACPI.COM - la batería de los portátiles por ACPI (residente)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Uso:  HSPACPI [/COMPLETO] [/CADA:s] [/V] [/REG:archivo] [/F]
;        HSPACPI /AUTO     para el AUTOEXEC.BAT (ver abajo)
;        HSPACPI /E        (ya cargado) cómo está la batería, con detalle
;        HSPACPI /U        descargarlo
;
;  Hispa Ventanas (y cualquier programa de DOS) pregunta la batería a la BIOS
;  por APM: INT 15h AX=530Ah. Los portátiles de 2008 en adelante (el HP Mini
;  5103, el Toshiba NB250) ya no tienen APM: la batería solo se sabe por ACPI,
;  ejecutando los métodos que trae el propio equipo en sus tablas (en AML).
;  HSPACPI hace eso y contesta él a INT 15h AX=5300h (¿hay APM?) y AX=530Ah
;  (estado de la batería) como lo haría una BIOS con APM 1.2: así el
;  indicador de la barra funciona sin tocarlo, en Hispa-486 y en Hispa-386.
;  Las demás funciones de APM (53xxh) siguen yendo a la BIOS (que dirá que
;  no las tiene): APAGAR ya apaga por ACPI por su cuenta.
;
;  Este archivo es solo la parte de modo real: lo residente son unos 2 KB.
;  El trabajo lo hace HSPACPI.BIN, un programa de 32 bits (ACPICA de Intel y
;  acpi/nucleo/, ver docs/ACPI.md) que se carga en un bloque de memoria
;  extendida (XMS), como el núcleo de HSPWIFI: para cada llamada, el
;  residente pasa a modo protegido con su propia tabla de segmentos (base =
;  el bloque), llama al núcleo y vuelve a modo real. Por eso NO funciona en
;  modo V86 (con HSPEMM): lo dice y no se carga. Con HSPDPMI sí.
;
;  La batería se lee sola cada 10 segundos (/CADA:s) desde el tic del reloj
;  (INT 08h), a trocitos: el núcleo corre unos milisegundos y vuelve; cuando
;  un método del equipo pide esperar, se sigue en un tic posterior. INT 15h
;  contesta al momento con lo último leído.
;
;  /AUTO: se queda solo si hace falta (la BIOS no da la batería por APM, hay
;  ACPI y el equipo declara una batería); si no, sale sin decir nada (nivel
;  de error 1). Es lo que lleva el AUTOEXEC.BAT de Hispa-486.
;  /COMPLETO: arrancar ACPI como lo haría un sistema con ACPI (los _INI de
;  los dispositivos, decir que somos Windows XP y atender los avisos del
;  controlador embebido). Sin él, el modo mínimo: solo se avisa al
;  controlador embebido de que se le va a leer y se ejecutan los métodos de
;  la batería (ver docs/ACPI.md, «Los dos modos»).
;  /OSI:n (0 ninguno, 1 XP, 2 Windows 7, 3 todos), /EVENTOS y /PSR afinan
;  eso mismo; /F lo carga aunque la BIOS tenga APM con batería.
;  /V enseña el registro; /REG:archivo lo guarda (se añade al final): es el
;  BATERIA.LOG de las pruebas.
;
;  Archivo: HSPACPI.BIN, al lado de HSPACPI.COM (o en \HISPADOS de la unidad
;  de arranque).
;
;  INT 2Fh AH=DBh:
;    AL=00h -> AL = FFh, BX = 4143h («AC»), CX = versión
;    AL=01h  llamar al núcleo: CX = función (acpi/nucleo/llamadas.h: 2
;            registro, 3 estado, 5 leer ya), DS:SI = petición (dobles
;            palabras; las direcciones, lineales) -> DX:AX = resultado
;    AL=02h -> ES:BX = struct ll_apm (lo último leído, ver llamadas.h)
;    AL=03h  descargar -> AL = 0 hecho, 1 otro residente encima
;    AL=04h  [APAGAR-USB] parar: avisa al controlador embebido (_REG 3,0) y
;            el núcleo no se vuelve a llamar (INT 15h 530Ah sigue dando lo
;            último leído); lo pide APAGAR/REINICIAR antes de vaciar los
;            búferes y de tocar ACPI -> AL = 0
;    AL=05h -> ES:BX = struct ll_salud ([SALUD] el desgaste de cada batería:
;            capacidad de fábrica, la de la última carga completa, unidades
;            y ciclos; ver llamadas.h). Lo enseña la barra de Hispa-486.
; =============================================================================
        cpu 586
        bits 16
        org 0x100

        section .text
        section .init follows=.text
        section .text

PILA        equ 384
VERSION     equ 2                   ; 2: AL=05h, la salud ([SALUD])
LL_VER      equ 2                   ; la de HSPACPI.BIN (+24), que tiene que casar
BLOQUE_KB   equ 3072                ; memoria extendida para el núcleo (más 64 KB
                                    ; para alinear su base)

LL_INICIA   equ 0
LL_PASO     equ 1
LL_REGISTRO equ 2
LL_ESTADO   equ 3
LL_PARA     equ 4
LL_AHORA    equ 5
OP_DETALLE  equ 1
OP_SININI   equ 2
OP_SINEV    equ 4
OP_SINREF   equ 8
OP_PSR      equ 16
OP_OS98     equ 32
OP_MINIMO   equ OP_SININI | OP_SINEV | OP_OS98

SEL_C32     equ 0x08
SEL_D32     equ 0x10
SEL_C16     equ 0x18
SEL_D16     equ 0x20
SEL_D4G     equ 0x28                ; datos de 16 bits con límite de 4 GB (ver nuc)

start:
        jmp init

; =============================================================================
;  Datos residentes
; =============================================================================
; struct ll_apm: lo escribe el núcleo después de cada lectura
apm:
a_fase      db 0                    ; 0 empezando, 1 listo, 2 sin batería, 3 error
a_ca        db 0xFF                 ; BH
a_estado    db 0xFF                 ; BL
a_band      db 0x80                 ; CH
a_pct       db 0xFF                 ; CL
a_nbat      db 0
a_min       dw 0xFFFF               ; DX
a_lect      dd 0
a_error     dd 0
; [SALUD] struct ll_salud: también la escribe el núcleo después de cada lectura
salud:
s_nbat      db 0
s_pct       db 0xFF
s_lect      dw 0
            times 4*12 db 0         ; struct ll_salud_bat bat[4]

old15       dd 0
old2f       dd 0
old08       dd 0
xms         dd 0
xms_h       dw 0
en_nucleo   db 0
en_paso     db 0
muerto      db 0                    ; el núcleo ha fallado: ya no se le llama
apm_bios    db 0                    ; la BIOS tiene APM de verdad (5300h es suyo)
a20_puesta  db 0
tic_ini     dd 0                    ; cuándo se llamó al núcleo por última vez
tic_esp     dd 1                    ; y cuántos tics pidió esperar
sv_sp       dw 0
sv_ss       dw 0
rm_sp       dw 0
rm_ss       dw 0
pet         times 4 dd 0

        align 4
entrada32   dd 0
            dw SEL_C32
vuelta_rm   dw nuc.rm
            dw 0
bloque      dd 0
pila32      dd 0
gdtr        dw 6 * 8 - 1
            dd 0
idtr        dw 32 * 8 - 1
            dd 0
gdt_ant     dw 0
            dd 0
idt_ant     dw 0
            dd 0
        align 8
gdt         dq 0
            dq 0x00CF9A000000FFFF   ; código de 32 bits (la base, al instalar)
            dq 0x00CF92000000FFFF   ; datos de 32 bits
            dq 0x00009A000000FFFF   ; código de 16 bits
            dq 0x000092000000FFFF   ; datos de 16 bits
            dq 0x008F92000000FFFF   ; datos de 16 bits, base 0 y límite de 4 GB

; =============================================================================
;  Llamar al núcleo: EAX = función, EDX = dirección lineal de la petición
;  -> EAX = resultado (-97 si ya se estaba dentro). DS = CS. Conserva lo demás.
;  (Como en HSPWIFI.)
; =============================================================================
nuc:
        pushf
        cli
        cmp byte [en_nucleo], 0
        je .libre
        mov eax, -97
        popf
        ret
.libre:
        mov byte [en_nucleo], 1
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push ds
        push es
        push fs
        push gs
        ; la línea A20, abierta (por el puerto 92h, sin tocar el bit del reinicio)
        push ax
        in al, 0x92
        mov [a20_puesta], al
        test al, 2
        jnz .a20
        or al, 2
        and al, 0xFE
        out 0x92, al
.a20:
        pop ax
        mov [rm_ss], ss
        mov [rm_sp], sp
        sgdt [gdt_ant]
        sidt [idt_ant]
        lgdt [gdtr]
        lidt [idtr]
        mov ebx, [bloque]
        mov ecx, cr0
        or cl, 1
        mov cr0, ecx
        jmp SEL_C16:.pm
.pm:
        mov cx, SEL_D32
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        mov ss, cx
        mov esp, [cs:pila32]
        o32 call far [cs:entrada32]
        ; DS, ES, FS y GS, con el límite de 4 GB: esta llamada puede venir
        ; del reloj y haber interrumpido a un controlador en modo «irreal»
        ; (HSPUSB, HSPSND); con 64 KB se quedaría colgado al volver (como en
        ; drivers/hspwifi.asm, nuc: MacBook Air 2012). La pila, con 64 KB.
        mov cx, SEL_D16
        mov ss, cx
        mov cx, SEL_D4G
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        mov ecx, cr0
        and cl, 0xFE
        mov cr0, ecx
        jmp far [cs:vuelta_rm]
.rm:
        mov cx, cs
        mov ds, cx
        lss sp, [rm_sp]
        movzx esp, sp
        lidt [idt_ant]
        lgdt [gdt_ant]
        test byte [a20_puesta], 2
        jnz .a20b
        push ax
        mov al, [a20_puesta]
        and al, 0xFE
        out 0x92, al
        pop ax
.a20b:
        pop gs
        pop fs
        pop es
        pop ds
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        mov byte [en_nucleo], 0
        popf
        ret

; =============================================================================
;  El tic del reloj: cuando toca, se deja correr al núcleo un rato
; =============================================================================
int08:
        pushf
        call far [cs:old08]
        cli
        cmp byte [cs:muerto], 0
        jne .no
        cmp byte [cs:en_paso], 0
        jne .no
        cmp byte [cs:en_nucleo], 0
        jne .no
        push ds
        push eax
        xor ax, ax
        mov ds, ax
        mov eax, [0x46C]
        push cs
        pop ds
        sub eax, [tic_ini]          ; (a medianoche el contador vuelve a cero:
        cmp eax, [tic_esp]          ; la resta sale enorme y se llama antes)
        jb .aun
        mov byte [en_paso], 1
        mov [sv_ss], ss
        mov [sv_sp], sp
        push cs
        pop ss
        mov sp, pila_top
        push edx
        cld
        call paso
        pop edx
        lss sp, [sv_sp]
        mov byte [en_paso], 0
.aun:
        pop eax
        pop ds
.no:
        iret

; un paso del núcleo y apuntar cuándo toca el siguiente. DS = CS.
paso:
        push es
        xor ax, ax
        mov es, ax
        mov eax, [es:0x46C]
        mov [tic_ini], eax
        pop es
        mov eax, LL_PASO
        xor edx, edx
        call nuc
        test eax, eax
        jg .ok
        jz .uno
        cmp eax, -97                ; ocupado: dentro de un segundo
        je .luego
        mov byte [muerto], 1        ; el núcleo ha fallado: no se le llama más
.luego:
        mov eax, 18
        jmp .p
.uno:
        mov eax, 1
        jmp .p
.ok:
        cmp eax, 0x10000
        jb .p
        mov eax, 0xFFFF
.p:
        mov [tic_esp], eax
        ret

; =============================================================================
;  INT 15h: APM. 5300h (¿hay APM?) y 530Ah (estado de la batería)
; =============================================================================
int15:
        cmp ah, 0x53
        jne .otro
        cmp al, 0x0A
        je .bat
        cmp al, 0x00
        jne .otro
        test bx, bx
        jnz .otro
        cmp byte [cs:apm_bios], 0   ; la BIOS tiene APM: que conteste ella
        jne .otro
        mov ax, 0x0102              ; APM 1.2
        mov bx, 0x504D              ; «PM»
        xor cx, cx                  ; sin interfaces de modo protegido
        jmp .bien
.bat:
        cmp bx, 0x0001              ; todas las baterías
        je .b1
        cmp bx, 0x8001              ; o la primera
        jne .otro
.b1:
        mov bx, 0xFFFF              ; no se sabe
        mov cx, 0x80FF              ; (CH bit 7: no hay batería)
        mov dx, 0xFFFF
        xor si, si
        cmp byte [cs:a_fase], 1
        jne .bien
        mov bh, [cs:a_ca]
        mov bl, [cs:a_estado]
        mov ch, [cs:a_band]
        mov cl, [cs:a_pct]
        mov dx, [cs:a_min]
        push ax
        movzx ax, byte [cs:a_nbat]
        mov si, ax
        pop ax
.bien:
        push bp
        mov bp, sp
        and byte [bp+6], 0xFE       ; sin acarreo: hecho
        pop bp
        iret
.otro:
        jmp far [cs:old15]

; =============================================================================
;  INT 2Fh AH=DBh
; =============================================================================
int2f:
        cmp ah, 0xDB
        je .mio
        jmp far [cs:old2f]
.mio:
        cmp al, 0
        jne .n1
        mov al, 0xFF
        mov bx, 0x4143
        mov cx, VERSION
        iret
.n1:
        cmp al, 1
        jne .n2
        cmp cx, LL_REGISTRO
        je .llama
        cmp cx, LL_ESTADO
        je .llama
        cmp cx, LL_AHORA
        je .llama
        mov ax, 0xFFFF
        mov dx, ax
        iret
.llama:
        push ebx
        push ds
        xor ebx, ebx
        mov bx, ds
        shl ebx, 4
        movzx edx, si
        add edx, ebx
        movzx eax, cx
        push cs
        pop ds
        call nuc
        pop ds
        pop ebx
        mov edx, eax
        shr edx, 16
        iret
.n2:
        cmp al, 2
        jne .n3
        push cs
        pop es
        mov bx, apm
        iret
.n3:
        cmp al, 3
        jne .n4
        push ds
        push cs
        pop ds
        call unload
        pop ds
        iret
.n4:
        cmp al, 4                   ; [APAGAR-USB] parar
        jne .n5
        cmp byte [cs:muerto], 0
        jne .n4x
        mov byte [cs:muerto], 1     ; (primero: el reloj ya no lo llama)
        push eax
        push edx
        push ds
        push cs
        pop ds
        mov eax, LL_PARA
        xor edx, edx
        call nuc
        pop ds
        pop edx
        pop eax
.n4x:
        xor al, al
        iret
.n5:
        cmp al, 5                   ; [SALUD]
        jne .n6
        push cs
        pop es
        mov bx, salud
.n6:
        iret

; descargar -> AL = 0 hecho, 1 otro residente encima. DS = CS.
unload:
        push es
        push bx
        push dx
        xor ax, ax
        mov es, ax
        mov dx, cs
        cmp word [es:0x2F*4], int2f
        jne .notours
        cmp [es:0x2F*4+2], dx
        jne .notours
        cmp word [es:0x08*4], int08
        jne .notours
        cmp [es:0x08*4+2], dx
        jne .notours
        cmp word [es:0x15*4], int15
        jne .notours
        cmp [es:0x15*4+2], dx
        jne .notours
        mov eax, LL_PARA
        xor edx, edx
        call nuc
        cli
        mov eax, [old2f]
        mov [es:0x2F*4], eax
        mov eax, [old08]
        mov [es:0x08*4], eax
        mov eax, [old15]
        mov [es:0x15*4], eax
        sti
        mov dx, [xms_h]
        mov ah, 0x0D                ; desbloquear
        call far [xms]
        mov dx, [xms_h]
        mov ah, 0x0A                ; liberar
        call far [xms]
        push cs
        pop es
        mov ah, 0x49
        int 0x21
        xor al, al
        jmp .r
.notours:
        mov al, 1
.r:
        pop dx
        pop bx
        pop es
        ret

pila        times PILA db 0
pila_top:
resident_end:

; =============================================================================
;  Instalación (se descarta)
; =============================================================================
        section .init
init:
        cld
        ; hace falta un 386 (y esto se comprueba sin usar nada de 386)
        cpu 8086
        pushf
        pop ax
        and ax, 0x0FFF
        push ax
        popf
        pushf
        pop ax
        and ax, 0xF000
        cmp ax, 0xF000
        je .viejo
        or ax, 0x7000
        push ax
        popf
        pushf
        pop ax
        test ax, 0x7000
        jnz .es386
.viejo:
        mov si, msg_386
        call puts
        mov ax, 0x4C01
        int 0x21
.es386:
        cpu 586
        call parse
        pushf
        cmp byte [o_auto], 0        ; con /AUTO, el título solo si se carga
        jne .tt
        mov si, msg_title
        call puts
.tt:
        popf
        jnc .pok
.uso:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.pok:
        cmp byte [o_help], 0
        je .nh
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
.nh:
        ; ¿ya está?
        mov ax, 0xDB00
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .new
        cmp bx, 0x4143
        jne .new
        cmp byte [o_unload], 0
        jne .unl
        cmp byte [o_auto], 0        ; /AUTO: ya está, nada que decir
        jne fin_bien_callado
        jmp ya_cargado
.unl:
        mov ax, 0xDB03
        int 0x2F
        mov si, msg_unloaded
        test al, al
        jz .ue
        mov si, msg_noun1
        jmp fin_mal
.ue:
        call puts
        mov ax, 0x4C00
        int 0x21
.new:
        mov si, msg_notres
        cmp byte [o_unload], 0
        jne fin_mal
        cmp byte [o_estado], 0
        jne fin_mal
        ; ¿la BIOS ya da la batería por APM? Entonces no hace falta (salvo /F)
        mov ax, 0x5300
        xor bx, bx
        int 0x15
        jc .sin_apm
        cmp bx, 0x504D
        jne .sin_apm
        mov byte [apm_bios], 1
        mov ax, 0x530A
        mov bx, 1
        int 0x15
        jc .sin_apm
        test ch, 0x80
        jnz .sin_apm
        cmp cl, 0xFF
        je .sin_apm
        cmp byte [o_f], 0
        jne .sin_apm
        cmp byte [o_auto], 0
        jne fin_mal_callado
        mov si, msg_apm
        jmp fin_mal
.sin_apm:
        ; modo V86: no se puede pasar a modo protegido por nuestra cuenta
        smsw ax
        test al, 1
        jz .real
        cmp byte [o_auto], 0
        jne fin_mal_callado
        mov si, msg_v86
        jmp fin_mal
.real:
        ; gestor XMS
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        je .hay_xms
        cmp byte [o_auto], 0
        jne fin_mal_callado
        mov si, msg_noxms
        jmp fin_mal
.hay_xms:
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        call mide_reloj
        ; el núcleo: al lado de este programa; si no, en \HISPADOS de la
        ; unidad de arranque
        call ruta_propia
        mov dx, ruta_bin
        mov ax, 0x3D00
        int 0x21
        jnc .abierto
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [ruta_bin2], dl
        mov dx, ruta_bin2
        mov ax, 0x3D00
        int 0x21
        jnc .abierto
        cmp byte [o_auto], 0
        jne fin_mal_callado
        mov si, msg_nobin
        jmp fin_mal
.abierto:
        mov [fh], ax
        mov bx, ax
        mov dx, tbuf
        mov cx, 32
        mov ah, 0x3F
        int 0x21
        jc .mal_bin
        cmp ax, 32
        jne .mal_bin
        cmp dword [tbuf+4], 0x31434148  ; «HAC1»
        jne .mal_bin
        cmp dword [tbuf+24], LL_VER     ; [SALUD] y de esta versión
        je .cab
.mal_bin:
        mov si, msg_badbin
        jmp fin_mal
.cab:
        mov ah, 0x09                ; pedir bloque
        mov dx, BLOQUE_KB + 64
        call far [xms]
        cmp ax, 1
        mov si, msg_nomem
        jne fin_mal
        mov [xms_h], dx
        mov ah, 0x0C                ; bloquear -> DX:BX = dirección
        call far [xms]
        cmp ax, 1
        mov si, msg_nomem
        jne fin_mal_xms
        mov [bloque], bx
        mov [bloque+2], dx
        ; la base, alineada a 64 KB dentro del bloque (como HSPWIFI)
        mov eax, [bloque]
        neg eax
        and eax, 0xFFFF
        mov [relleno], eax
        add [bloque], eax
        mov ah, 0x03                ; A20 abierta (global)
        call far [xms]
        ; copiar el núcleo al bloque
        mov bx, [fh]
        xor cx, cx
        xor dx, dx
        mov ax, 0x4200
        int 0x21
        mov dword [x_des], 0
        mov dword [x_tam], 0
        call copia_archivo
        mov si, msg_badbin
        jc fin_mal_xms
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        mov eax, [x_tam]
        cmp eax, [tbuf_cab+8]
        mov si, msg_badbin
        jne fin_mal_xms
        ; la tabla de segmentos
        mov eax, [bloque]
        mov [gdt+8+2], ax
        mov [gdt+16+2], ax
        shr eax, 16
        mov [gdt+8+4], al
        mov [gdt+16+4], al
        mov [gdt+8+7], ah
        mov [gdt+16+7], ah
        xor eax, eax
        mov ax, cs
        mov [vuelta_rm+2], ax
        shl eax, 4
        mov [gdt+24+2], ax
        mov [gdt+32+2], ax
        mov ebx, eax
        shr eax, 16
        mov [gdt+24+4], al
        mov [gdt+32+4], al
        lea eax, [ebx + gdt]
        mov [gdtr+2], eax
        mov eax, [tbuf_cab+16]
        mov [pila32], eax
        mov eax, [tbuf_cab+20]
        add eax, [bloque]
        mov [idtr+2], eax
        ; preparar el núcleo
        mov eax, [bloque]
        mov [p_ini], eax
        mov dword [p_ini+4], BLOQUE_KB * 1024
        movzx eax, byte [opciones]
        mov [p_ini+8], eax
        movzx eax, byte [o_osi]
        mov [p_ini+12], eax
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + apm]
        mov [p_ini+20], eax
        movzx eax, word [o_cada]
        mov [p_ini+24], eax
        lea eax, [edx + salud]      ; [SALUD]
        mov [p_ini+28], eax
        add edx, p_ini
        mov eax, LL_INICIA
        call nuc
        test eax, eax
        jnz .falla
        ; dejarlo correr hasta la primera lectura (como mucho, un minuto):
        ; el registro se va guardando, por si un paso se cuelga
        mov word [quedan], 1100
.arr:
        call paso
        call vuelca_registro
        cmp byte [a_fase], 0
        jne .listo
        cmp byte [muerto], 0
        je .vivo
        mov eax, -95
        jmp .falla
.vivo:
        ; esperar los tics que pide
        mov ecx, [tic_esp]
        cmp ecx, 40
        jbe .esp
        mov ecx, 40
.esp:
        push ecx
        call un_tic
        pop ecx
        dec word [quedan]
        jz .tarda
        loop .esp
        jmp .arr
.tarda:
        mov eax, -96
        jmp .falla
.listo:
        cmp byte [a_fase], 1
        je .va
        cmp byte [a_fase], 2
        jne .err
        cmp byte [o_auto], 0
        jne .suelta_callado
        mov si, msg_nobat
        call puts
        jmp fin_mal_xms_s
.suelta_callado:
        call suelta_xms
        jmp fin_mal_callado
.err:
        mov eax, [a_error]
        neg eax
.falla:
        ; EAX = el código (negativo); el porqué está en el registro
        cmp byte [o_auto], 0
        jne .suelta_callado
        push eax
        mov si, msg_fallo
        call puts
        pop eax
        neg eax
        call putdec
        mov si, msg_fallo2
        call puts
        jmp fin_mal_xms_s
.va:
        cmp byte [o_auto], 0
        je .t2
        mov si, msg_title
        call puts
.t2:
        push cs
        pop es
        mov bx, apm
        call di_estado
        ; los vectores
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        push cs
        pop es
        mov ax, 0x252F
        mov dx, int2f
        int 0x21
        mov ax, 0x2515
        mov dx, int15
        int 0x21
        mov ax, 0x2508
        mov dx, int08
        int 0x21
        ; soltar el entorno y quedarse
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, resident_end + 15
        shr dx, 4
        mov ax, 0x3100
        int 0x21

fin_mal_xms:
        call puts
fin_mal_xms_s:
        call suelta_xms
        mov ax, 0x4C01
        int 0x21
fin_mal:
        call puts
        mov ax, 0x4C01
        int 0x21
fin_bien_callado:
        mov ax, 0x4C00
        int 0x21
fin_mal_callado:
        mov ax, 0x4C01
        int 0x21

suelta_xms:
        mov dx, [xms_h]
        mov ah, 0x0D
        call far [xms]
        mov dx, [xms_h]
        mov ah, 0x0A
        call far [xms]
        ret

; esperar a que cambie el contador de tics de la BIOS
un_tic:
        push es
        push ax
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
.l:
        sti
        hlt
        cmp ax, [es:0x46C]
        je .l
        pop ax
        pop es
        ret

; ---- ya está cargado: /E (o nada) enseña el estado; /REG y /V, el registro ----
ya_cargado:
        ; una lectura ya, y esperar a que se haga (como mucho 3 s)
        mov ax, 0xDB02
        int 0x2F
        mov [res_apm], bx
        mov [res_apm+2], es
        mov eax, [es:bx+8]
        mov [lect0], eax
        mov ax, 0xDB01
        mov cx, LL_AHORA
        mov si, pet_t
        int 0x2F
        mov cx, 55
.esp:
        call un_tic
        les bx, [res_apm]
        mov eax, [es:bx+8]
        cmp eax, [lect0]
        jne .ya
        loop .esp
.ya:
        les bx, [res_apm]
        call di_estado
        call di_detalle
        cmp byte [o_v], 0
        jne .reg
        cmp byte [reg_ruta], 0
        je .fin
.reg:
        mov byte [por_2f], 1
        call vuelca_registro
.fin:
        mov ax, 0x4C00
        int 0x21

; ---- «Batería: 72 %, sin cable, quedan 126 min» (ES:BX = struct ll_apm) ----
di_estado:
        mov si, msg_bat
        call puts
        cmp byte [es:bx], 1
        je .hay
        mov si, msg_sinlect
        jmp puts
.hay:
        cmp byte [es:bx+5], 0
        jne .puesta
        mov si, msg_quitada
        call puts
        jmp .cable
.puesta:
        movzx eax, byte [es:bx+4]
        cmp al, 0xFF
        jne .pct
        mov si, msg_nose
        call puts
        jmp .cable
.pct:
        call putdec
        mov si, msg_pct
        call puts
.cable:
        mov si, msg_concable
        cmp byte [es:bx+1], 1
        je .c
        mov si, msg_sincable
        cmp byte [es:bx+1], 0
        jne .nc
.c:
        call puts
.nc:
        cmp byte [es:bx+2], 3
        jne .min
        mov si, msg_cargando
        call puts
.min:
        mov ax, [es:bx+6]
        cmp ax, 0xFFFF
        je .fin
        and eax, 0x7FFF
        mov si, msg_quedan
        call puts
        call putdec
        mov si, msg_min
        call puts
.fin:
        mov si, msg_nl
        jmp puts

; ---- [SALUD] «  Salud: 68 % (3400 de 5000 mAh de fábrica), 312 ciclos»:
; de cada batería con la salud leída (INT 2Fh AX=DB05h) ----
di_salud:
        push es
        mov ax, 0xDB05
        int 0x2F
        mov cx, 4
.b:
        cmp byte [es:bx+4], 1       ; valida
        jne .sig
        push cx
        mov si, msg_s_sal
        call puts
        mov eax, [es:bx+12]         ; llena × 100 / diseño
        mov ecx, 100
        mul ecx
        div dword [es:bx+8]
        call putdec32
        mov si, msg_s_pct
        call puts
        mov eax, [es:bx+12]
        call putdec32
        mov si, msg_d_de
        call puts
        mov eax, [es:bx+8]
        call putdec32
        mov si, msg_d_mah
        cmp byte [es:bx+5], 0
        jne .u
        mov si, msg_d_mwh
.u:
        call puts
        mov si, msg_s_fab
        call puts
        movzx eax, word [es:bx+6]
        cmp ax, 0xFFFF
        je .nc
        mov si, msg_s_coma
        call puts
        call putdec32
        mov si, msg_s_cic
        call puts
.nc:
        mov si, msg_nl
        call puts
        pop cx
.sig:
        add bx, 12
        loop .b
        pop es
        ret

; ---- el detalle (struct ll_estado, por INT 2Fh AX=DB01h CX=3) ----
E_EC        equ 20
E_ECRUTA    equ 68
E_MEM       equ 56
E_ERR       equ 64
E_BAT       equ 132
B_TAM       equ 128
di_detalle:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, tbuf
        mov [pet_t], eax
        mov dword [pet_t+4], 644
        mov ax, 0xDB01
        mov cx, LL_ESTADO
        mov si, pet_t
        int 0x2F
        cmp ax, 644
        jne .r
        mov di, tbuf + E_BAT
        mov cx, 4
.b:
        cmp dword [di], 0
        je .sig
        push cx
        mov si, msg_d_bat
        call puts
        lea si, [di+44]
        call puts
        mov si, msg_d_no
        test byte [di+4], 0x10      ; _STA bit 4: puesta
        jz .bfin
        mov si, msg_d_2p
        call puts
        lea si, [di+76]
        call puts_esp
        lea si, [di+96]
        call puts_esp
        lea si, [di+108]
        call puts_esp
        mov eax, [di+36]
        call putdec32
        mov si, msg_d_de
        call puts
        mov eax, [di+20]
        call putdec32
        mov si, msg_d_mah
        cmp dword [di+12], 0
        jne .u
        mov si, msg_d_mwh
.u:
        call puts
        mov si, msg_d_dis
        call puts
        mov eax, [di+16]
        call putdec32
        mov si, msg_d_v
        call puts
        mov eax, [di+40]
        call putdec32
        mov si, msg_d_mv
.bfin:
        call puts
        pop cx
.sig:
        add di, B_TAM
        loop .b
        call di_salud               ; [SALUD]
        cmp dword [tbuf+E_EC], 0
        je .mem
        mov si, msg_d_ec
        call puts
        mov si, tbuf + E_ECRUTA
        call puts
        mov si, msg_d_ec2
        call puts
        mov eax, [tbuf+E_EC+12]
        call putdec
        mov si, msg_d_ec3
        call puts
        mov eax, [tbuf+E_EC+16]
        call putdec
        mov si, msg_d_ec4
        call puts
        mov eax, [tbuf+E_EC+20]
        call putdec
        mov si, msg_d_ec5
        call puts
        mov eax, [tbuf+E_EC+24]
        call putdec
        mov si, msg_nl
        call puts
.mem:
        mov si, msg_d_mem
        call puts
        mov eax, [tbuf+E_MEM+4]
        shr eax, 10
        call putdec
        mov si, msg_d_mem2
        call puts
        mov eax, [tbuf+E_ERR]
        call putdec
        mov si, msg_d_mem3
        call puts
        les bx, [res_apm]
        mov eax, [es:bx+8]
        call putdec
        mov si, msg_nl
        call puts
.r:
        ret

; un valor de ACPI: FFFFFFFFh es «no se sabe»
putdec32:
        cmp eax, 0xFFFFFFFF
        jne putdec
        mov al, '?'
        jmp putc

; una palabra y un espacio (nada si está vacía)
puts_esp:
        cmp byte [si], 0
        je .r
        call puts
        mov al, ' '
        call putc
.r:
        ret

; ---- el contador de tiempo del procesador: vueltas por milisegundo ----
mide_reloj:
        ; ¿hay CPUID? (se puede cambiar el bit 21 de EFLAGS)
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x200000
        jz .no
        mov eax, 1
        cpuid
        test dl, 0x10               ; TSC
        jz .no
        call un_tic
        rdtsc
        mov esi, eax
        mov edi, edx
        call un_tic
        call un_tic                 ; dos tics: 109,85 ms
        rdtsc
        sub eax, esi
        sbb edx, edi
        jnz .no                     ; (imposible: más de 39 GHz)
        mov ecx, 20                 ; vueltas * 20 / 2197 = vueltas / 109,85
        mul ecx
        mov ecx, 2197
        div ecx
        mov [p_ini+16], eax
        ret
.no:
        mov dword [p_ini+16], 0
        ret

; ---- la ruta de este programa (del entorno), con HSPACPI.BIN al final ----
ruta_propia:
        push es
        mov es, [0x2C]
        xor di, di
        xor al, al
        mov cx, 0x7FFF
.e:
        repne scasb
        jne .r
        scasb
        jne .e
        add di, 2                   ; el contador de cadenas
        mov si, di
        mov di, ruta_bin
        xor bx, bx                  ; tras la última barra
.c:
        mov al, [es:si]
        inc si
        mov [di], al
        inc di
        cmp al, '\'
        je .barra
        cmp al, ':'
        jne .nb
.barra:
        mov bx, di
.nb:
        test al, al
        jz .fin
        cmp di, ruta_bin + 100
        jb .c
.fin:
        test bx, bx
        jnz .pon
        mov bx, ruta_bin
.pon:
        mov di, bx
        mov si, n_bin
        push ds
        pop es
.n:
        lodsb
        stosb
        test al, al
        jnz .n
.r:
        pop es
        ret

; el archivo [fh], desde donde esté hasta el final, a bloque + x_des
copia_archivo:
.l:
        mov bx, [fh]
        mov dx, tbuf
        mov cx, TBUF
        mov ah, 0x3F
        int 0x21
        jc .mal
        test ax, ax
        jz .fin
        cmp dword [x_des], 0
        jne .nc
        push ax
        mov si, tbuf
        mov di, tbuf_cab
        mov cx, 16
        rep movsw
        pop ax
.nc:
        movzx eax, ax
        add [x_tam], eax
        inc ax
        and ax, 0xFFFE              ; XMS mueve un número par de octetos
        mov [x_mov], eax
        mov word [x_mov+4], 0       ; origen: memoria convencional
        mov word [x_mov+6], tbuf
        mov [x_mov+8], cs
        mov dx, [xms_h]
        mov [x_mov+10], dx
        mov edx, [x_des]
        add edx, [relleno]
        mov [x_mov+12], edx
        mov edx, [x_des]
        add edx, eax
        cmp edx, BLOQUE_KB * 1024 - 0x100000    ; que quede 1 MB para trabajar
        ja .mal
        mov [x_des], edx
        mov si, x_mov
        mov ah, 0x0B
        call far [xms]
        cmp ax, 1
        jne .mal
        jmp .l
.fin:
        clc
        ret
.mal:
        stc
        ret

; ---- el registro del núcleo: a la pantalla (/V) y al archivo (/REG) ----
; (durante la instalación, llamando al núcleo; ya cargado, por INT 2Fh)
vuelca_registro:
        mov word [reg_fh], 0
.l:
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + tbuf]
        mov [pet_t], eax
        mov dword [pet_t+4], TBUF
        cmp byte [por_2f], 0
        je .dir
        mov ax, 0xDB01
        mov cx, LL_REGISTRO
        mov si, pet_t
        int 0x2F
        movzx eax, ax
        cmp ax, TBUF
        jbe .hay
        xor eax, eax
        jmp .hay
.dir:
        add edx, pet_t
        mov eax, LL_REGISTRO
        call nuc
.hay:
        test eax, eax
        jle .fin
        mov cx, ax
        cmp byte [o_v], 0
        je .nov
        push cx
        mov dx, tbuf
        mov bx, 1
        mov ah, 0x40
        int 0x21
        pop cx
.nov:
        cmp byte [reg_ruta], 0
        je .l
        cmp word [reg_fh], 0
        jne .esc
        ; abrir para añadir; si no existe, crearlo
        push cx
        mov dx, reg_ruta
        mov ax, 0x3D02
        int 0x21
        jnc .ab
        mov dx, reg_ruta
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jnc .ab
        pop cx
        mov byte [reg_ruta], 0      ; no se puede: se deja de intentar
        jmp .l
.ab:
        mov [reg_fh], ax
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov ax, 0x4202
        int 0x21
        pop cx
.esc:
        mov bx, [reg_fh]
        mov dx, tbuf
        mov ah, 0x40
        int 0x21
        jmp .l
.fin:
        mov bx, [reg_fh]
        test bx, bx
        jz .r
        mov ah, 0x3E
        int 0x21
.r:
        ret

; ---- opciones ----
parse:
        mov si, 0x81
.l:
        lodsb
        cmp al, ' '
        je .l
        cmp al, 9
        je .l
        cmp al, 13
        je .ok
        test al, al
        jz .ok
        cmp al, '/'
        je .opt
        cmp al, '-'
        je .opt
.bad:
        stc
        ret
.ok:
        clc
        ret
.opt:
        ; la palabra de la opción, en mayúsculas, a «pal»
        mov di, pal
.p:
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .pf
        cmp al, 'Z'
        ja .pf
        cmp di, pal + 12
        jae .bad
        mov [di], al
        inc di
        inc si
        jmp .p
.pf:
        cmp al, '?'
        jne .np
        inc si
        mov byte [o_help], 1
        jmp .l
.np:
        mov byte [di], 0
        mov bx, tabla
.t:
        cmp byte [bx], 0
        je .bad
        push si
        mov si, bx
        mov di, pal
.cmp:
        lodsb
        cmp al, [di]
        jne .otra
        inc di
        test al, al
        jnz .cmp
        ; es esta: SI apunta a la acción
        mov bx, si
        pop si
        jmp [bx]
.otra:
        ; a la siguiente: saltar el resto del nombre y la acción
        dec si
.sk:
        lodsb
        test al, al
        jnz .sk
        lea bx, [si+2]
        pop si
        jmp .t

.o_u:
        mov byte [o_unload], 1
        jmp .l
.o_e:
        mov byte [o_estado], 1
        jmp .l
.o_v:
        mov byte [o_v], 1
        or byte [opciones], OP_DETALLE
        jmp .l
.o_a:
        mov byte [o_auto], 1
        jmp .l
.o_f:
        mov byte [o_f], 1
        jmp .l
.o_comp:
        and byte [opciones], 0xFF ^ OP_MINIMO
        cmp byte [osi_dicho], 0
        jne .l
        mov byte [o_osi], 1
        jmp .l
.o_ev:
        and byte [opciones], 0xFF ^ OP_SINEV
        jmp .l
.o_psr:
        or byte [opciones], OP_PSR
        jmp .l
.o_sinref:
        or byte [opciones], OP_SINREF
        jmp .l
.o_osi:
        call .num
        jc .bad
        cmp ax, 3
        ja .bad
        mov [o_osi], al
        mov byte [osi_dicho], 1
        jmp .l
.o_cada:
        call .num
        jc .bad
        cmp ax, 2
        jb .bad
        cmp ax, 3600
        ja .bad
        mov [o_cada], ax
        jmp .l
.o_reg:
        cmp byte [si], ':'
        jne .bad
        inc si
        mov di, reg_ruta
.rg:
        mov al, [si]
        cmp al, ' '
        jbe .rgf
        cmp di, reg_ruta + 63
        jae .bad
        mov [di], al
        inc di
        inc si
        jmp .rg
.rgf:
        mov byte [di], 0
        cmp di, reg_ruta
        je .bad
        jmp .l
; «:número» -> AX (acarreo si no lo hay)
.num:
        cmp byte [si], ':'
        jne .nmal
        inc si
        xor ax, ax
        xor cx, cx
.nd:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .nf
        mov dx, 10
        mul dx
        xor bh, bh
        add ax, bx
        inc si
        inc cx
        jmp .nd
.nf:
        test cx, cx
        jz .nmal
        clc
        ret
.nmal:
        stc
        ret

tabla:
        db "U", 0
        dw parse.o_u
        db "D", 0
        dw parse.o_u
        db "E", 0
        dw parse.o_e
        db "ESTADO", 0
        dw parse.o_e
        db "V", 0
        dw parse.o_v
        db "AUTO", 0
        dw parse.o_a
        db "F", 0
        dw parse.o_f
        db "COMPLETO", 0
        dw parse.o_comp
        db "EVENTOS", 0
        dw parse.o_ev
        db "PSR", 0
        dw parse.o_psr
        db "SINREFRESCO", 0
        dw parse.o_sinref
        db "OSI", 0
        dw parse.o_osi
        db "CADA", 0
        dw parse.o_cada
        db "REG", 0
        dw parse.o_reg
        db 0

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret

puts:
        push ax
        push si
.l:
        lodsb
        test al, al
        jz .d
        call putc
        jmp .l
.d:
        pop si
        pop ax
        ret

; EAX sin signo, en decimal
putdec:
        push eax
        push ebx
        push cx
        push edx
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop edx
        pop cx
        pop ebx
        pop eax
        ret

o_help      db 0
o_unload    db 0
o_estado    db 0
o_auto      db 0
o_v         db 0
o_f         db 0
o_osi       db 0                    ; LL_OSI_NADA (con /COMPLETO, 1: XP)
osi_dicho   db 0
o_cada      dw 10
opciones    db OP_MINIMO
por_2f      db 0
quedan      dw 0
fh          dw 0
reg_fh      dw 0
lect0       dd 0
res_apm     dd 0
x_des       dd 0
x_tam       dd 0
relleno     dd 0
x_mov       times 16 db 0
p_ini       times 8 dd 0            ; struct ll_inicio
pet_t       times 4 dd 0
tbuf_cab    times 32 db 0
pal         times 14 db 0
reg_ruta    times 64 db 0
n_bin       db "HSPACPI.BIN", 0
ruta_bin    times 116 db 0
ruta_bin2   db "A:\HISPADOS\HSPACPI.BIN", 0

msg_title   db "HSPACPI 0.1: la batería del portátil por ACPI", 13, 10, 0
msg_usage   db "Uso: HSPACPI [/COMPLETO] [/CADA:s] [/V] [/REG:archivo] [/F]", 13, 10
            db "     HSPACPI /AUTO", 13, 10
            db "     HSPACPI /E", 13, 10
            db "     HSPACPI /U", 13, 10
            db "  /AUTO       para el AUTOEXEC.BAT: solo se carga si el equipo tiene una", 13, 10
            db "              batería que la BIOS no enseña por APM; si no, calla", 13, 10
            db "  /COMPLETO   arranca ACPI como un sistema con ACPI (los _INI del equipo,", 13, 10
            db "              decir que somos Windows XP, avisos del controlador embebido)", 13, 10
            db "  /CADA:s     segundos entre lecturas (10)", 13, 10
            db "  /V          enseña el registro", 13, 10
            db "  /REG:arch   guarda el registro en ese archivo (se añade al final)", 13, 10
            db "  /F          se carga aunque la BIOS ya dé la batería por APM", 13, 10
            db "  /E          (ya cargado) el estado de la batería, con detalle", 13, 10
            db "  /U          lo descarga", 13, 10
            db "Para afinar: /OSI:n (0 ninguno, 1 XP, 2 Windows 7, 3 todos), /EVENTOS, /PSR,", 13, 10
            db "/SINREFRESCO (ver docs/ACPI.md).", 13, 10, 0
msg_386     db "HSPACPI necesita un 386 o superior.", 13, 10, 0
msg_unloaded db "HSPACPI descargado.", 13, 10, 0
msg_noun1   db "No se puede descargar: otro programa residente se cargó después.", 13, 10, 0
msg_notres  db "HSPACPI no está cargado.", 13, 10, 0
msg_apm     db "La BIOS de este equipo ya da la batería por APM: HSPACPI no hace falta", 13, 10
            db "(con /F se carga de todos modos).", 13, 10, 0
msg_v86     db "HSPACPI no funciona con HSPEMM ni otro gestor de memoria expandida (modo V86).", 13, 10
            db "Quite HSPEMM.SYS de CONFIG.SYS para ver la batería.", 13, 10, 0
msg_noxms   db "HSPACPI necesita memoria extendida: cargue HSPXMS.SYS en CONFIG.SYS.", 13, 10, 0
msg_nobin   db "Falta el archivo HSPACPI.BIN (va al lado de HSPACPI.COM).", 13, 10, 0
msg_badbin  db "El archivo HSPACPI.BIN no vale (dañado o de otra versión).", 13, 10, 0
msg_nomem   db "No hay 3,1 MB de memoria extendida libres para HSPACPI.", 13, 10, 0
msg_nobat   db "Este equipo no declara ninguna batería por ACPI: HSPACPI no se queda.", 13, 10, 0
msg_fallo   db "No se ha podido leer la batería por ACPI (código ", 0
msg_fallo2  db ").", 13, 10
            db "Para ver por qué: HSPACPI /V  (o /REG:C:\BATERIA.LOG para guardarlo)", 13, 10, 0
msg_nl      db 13, 10, 0
msg_bat     db "Batería: ", 0
msg_sinlect db "todavía no hay lectura", 13, 10, 0
msg_quitada db "no está puesta", 0
msg_nose    db "nivel desconocido", 0
msg_pct     db " %", 0
msg_concable db ", con cable", 0
msg_sincable db ", sin cable", 0
msg_cargando db ", cargando", 0
msg_quedan  db ", quedan ", 0
msg_min     db " min", 0
msg_d_bat   db "  ", 0
msg_d_no    db ": no está puesta", 13, 10, 0
msg_d_2p    db ": ", 0
msg_d_de    db " de ", 0
msg_d_mah   db " mAh", 0
msg_d_mwh   db " mWh", 0
msg_d_dis   db " (de fábrica, ", 0
msg_d_v     db "), ", 0
msg_d_mv    db " mV", 13, 10, 0
msg_s_sal   db "  Salud: ", 0
msg_s_pct   db " % (", 0
msg_s_fab   db " de fábrica)", 0
msg_s_coma  db ", ", 0
msg_s_cic   db " ciclos de carga", 0
msg_d_ec    db "  Controlador embebido ", 0
msg_d_ec2   db ": ", 0
msg_d_ec3   db " lecturas, ", 0
msg_d_ec4   db " escrituras, ", 0
msg_d_ec5   db " fallos, avisos: ", 0
msg_d_mem   db "  Memoria de ACPI: ", 0
msg_d_mem2  db " KB; avisos o errores de ACPICA: ", 0
msg_d_mem3  db "; lecturas: ", 0

TBUF        equ 4096
tbuf:
