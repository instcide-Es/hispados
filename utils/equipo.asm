; =============================================================================
;  Hispa-DOS EQUIPO.COM - informe completo del hardware del equipo, en texto
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  EQUIPO [/CPU] [/BIOS] [/VESA] [/PCI] [/DISCOS] [/RES] [/ISA] [/NOxxx]
;  EQUIPO /INFORME [/NOxxx]     (lo que hace DIAGNOSTICO: DIAGNOST.BAT)
;
;  Todo sale por la salida estándar (INT 21h, función 40h, manejador 1), sin
;  dibujar en pantalla, así que se puede guardar con «EQUIPO > ARCHIVO.TXT».
;  Sin opciones salen todos los apartados; con alguno, solo los que se piden;
;  /NOxxx quita uno (por ejemplo /NOISA, si el sondeo de los puertos ISA
;  diera problemas en algún equipo).
;
;    CPU     procesador: CPUID (o 386/486 por pruebas), banderas, velocidad
;    BIOS    fecha, fabricante, SMBIOS, ACPI, APM y mapa de memoria
;    VESA    VBE: versión, memoria, fabricante y modos
;    PCI     cada función (puertos CF8h/CFCh), con nombre de la clase, BAR,
;            IRQ y, en los controladores USB, de quién son (BIOS o sistema)
;    DISCOS  disqueteras y discos de INT 13h (con EDD)
;    RES     residentes: XMS, EMS/VCPI, DPMI, HSPSND, HSPJOY, HSPUSB, ratón...
;    ISA     Sound Blaster, AdLib, puerto de juegos, serie, paralelo, 8042
;
;  /INFORME crea en la unidad de arranque una carpeta nueva cada vez,
;  \INFORMES\EQUIPO01, EQUIPO02..., con:
;    INFORME.TXT   VER, la fecha, todos los apartados, MEM /C, SET,
;                  HSPSND /INFO, HSPRATON /INFO, MANDO, CONFIG.SYS y
;                  AUTOEXEC.BAT
;    PCI\BB_DD_F.BIN  el espacio de configuración de cada función (256
;                  octetos por CF8h/CFCh) y, si hay MMCONFIG (tabla MCFG de
;                  ACPI), el extendido de 4 KB en BB_DD_F.EXT
;    ACPI\*.BIN    la RSDP, la RSDT/XSDT y todas las tablas (DSDT, FACP, FACS,
;                  SSDT1, SSDT2, MCFG, APIC, HPET...), con longitud y suma
;                  comprobadas
;    USB\BB_DD_F.BIN  los registros de capacidad, de operación y de los
;                  puertos de cada controlador (y .ECP: capacidades del xHCI)
;    SMBIOS.BIN, SMBIOSEP.BIN, CMOS.BIN, IVT.BIN, BDA.BIN, EBDA.BIN, VBE.BIN,
;    VBEMODOS.BIN (número de modo + 256 octetos por modo), EDID.BIN,
;    EDDnn.BIN (INT 13h 48h), ATA1F0_0.BIN... (identificación ATA),
;    ROMBIOS.TXT y ROMVIDEO.TXT (los textos de las ROM)
;  El informe se cierra después de cada paso (y antes de cada sondeo
;  delicado): si el equipo se colgara, lo escrito hasta ahí queda en el disco
;  y la última línea dice dónde fue. Si \HISPADOS\AUDICION existe, al final
;  se ofrece la prueba de sonido: «AUDICION carpeta» (guarda SONIDO.TXT).
;
;  No se cuelga: todas las esperas tienen un tope y lo que no responde se
;  salta con su aviso. Solo se LEEN los registros de los dispositivos. Las
;  únicas órdenes que se envían son las de consulta de siempre, que no dejan
;  nada cambiado: reinicio del DSP de la Sound Blaster (si no la usa HSPSND),
;  temporizadores del AdLib, disparo del puerto de juegos, identificación del
;  ratón PS/2 (se le deja la resolución y el envío de datos como estaban),
;  IDENTIFY a los discos IDE de 1F0h/170h y las órdenes inmediatas a los
;  códecs HD Audio (solo si HSPSND no lleva ese controlador; si estaba en
;  reinicio, se le devuelve al reinicio).
; =============================================================================
        cpu 586
        bits 16
        org 0x100

OBUF_SZ     equ 200
SMBUF_SZ    equ 1024
S_CPU       equ 1
S_BIOS      equ 2
S_VESA      equ 4
S_PCI       equ 8
S_DISCOS    equ 16
S_RES       equ 32
S_ISA       equ 64
S_ALL       equ 127
%define SANGRIA "         "

%macro PR 1+
        call pr_inline
        db %1, 0
%endmacro
%macro PR_SCR 1+
        call scr_inline
        db %1, 0
%endmacro
%define NL 13, 10

start:
        ; ¿un 386 por lo menos? (hasta saberlo, solo instrucciones del 8086)
        push sp
        pop ax
        cmp ax, sp
        jne short .viejo
        pushf
        pop bx
        mov ax, bx
        or ah, 0x70
        push ax
        popf
        pushf
        pop ax
        push bx
        popf
        test ah, 0x70
        jz short .viejo
        jmp inicio
.viejo:
        mov dx, m_viejo
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21
m_viejo     db "EQUIPO necesita un procesador 386 o superior.", 13, 10, "$"

inicio:
        cld
        mov di, bss_start
        mov cx, bss_end - bss_start
        xor al, al
        rep stosb
        mov sp, stack_top
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov word [out_h], 1
        mov word [dmp_h], 0xFFFF
        call parse_args
        jc .mal
        cmp byte [opt_help], 0
        jne ayuda
        cmp byte [opt_inf], 0
        jne informe
        PR "EQUIPO - informe del hardware del equipo (Hispa-DOS)", NL
        call run_sections
        call flush
        mov ax, 0x4C00
        int 0x21
.mal:
        PR "Parámetro no válido.", NL
        mov si, m_uso
        call puts
        call flush
        mov ax, 0x4C01
        int 0x21
ayuda:
        mov si, m_uso
        cmp byte [opt_inf], 0
        je .u
        mov si, m_usod
.u:
        call puts
        call flush
        mov ax, 0x4C00
        int 0x21

; -----------------------------------------------------------------------------
;  La línea de órdenes
; -----------------------------------------------------------------------------
parse_args:
        mov si, 0x81
.next:
        lodsb
        cmp al, ' '
        je .next
        cmp al, 9
        je .next
        cmp al, 13
        je .done
        test al, al
        jz .done
        cmp al, '/'
        jne .bad
        mov di, tok
        xor cx, cx
.t:
        mov al, [si]
        cmp al, ' '
        jbe .te
        cmp al, '/'
        je .te
        cmp al, 'a'
        jb .up
        cmp al, 'z'
        ja .up
        sub al, 32
.up:
        cmp cx, 14
        jae .bad
        stosb
        inc si
        inc cx
        jmp .t
.te:
        mov byte [di], 0
        jcxz .bad
        cmp word [tok], '?'
        jne .n1
        mov byte [opt_help], 1
        jmp .next
.n1:
        mov bx, tok
        call find_sec
        jc .n2
        or [sel_mask], ax
        jmp .next
.n2:
        push si
        mov si, tok
        mov di, k_informe
        call streq
        pop si
        jne .n3
        mov byte [opt_inf], 1
        jmp .next
.n3:
        push si
        mov si, tok
        mov di, k_nosonido
        call streq
        pop si
        jne .n4
        mov byte [opt_nosnd], 1
        jmp .next
.n4:
        cmp word [tok], 'NO'
        jne .bad
        mov bx, tok + 2
        call find_sec
        jc .bad
        or [no_mask], ax
        jmp .next
.done:
        mov ax, [sel_mask]
        test ax, ax
        jnz .m
        mov ax, S_ALL
.m:
        mov bx, [no_mask]
        not bx
        and ax, bx
        mov [mask], ax
        clc
        ret
.bad:
        stc
        ret

; BX -> nombre (ASCIIZ, mayúsculas) -> AX = bit del apartado; CF si no es
find_sec:
        push si
        push di
        mov si, sec_names
.l:
        lodsw
        test ax, ax
        jz .no
        mov di, bx
        push ax
        call streq                  ; (SI queda detrás del nombre)
        pop ax
        jne .l
        clc
        jmp .x
.no:
        stc
.x:
        pop di
        pop si
        ret

; SI, DI -> dos ASCIIZ -> ZF si son iguales; SI queda detrás de la primera
streq:
        push ax
        push bx
        mov bl, 1                   ; iguales de momento
.l:
        lodsb
        cmp bl, 0
        je .s
        cmp al, [di]
        je .s
        mov bl, 0
.s:
        inc di
        test al, al
        jnz .l
        cmp bl, 1
        pop bx
        pop ax
        ret

sec_names:
        dw S_CPU
        db "CPU", 0
        dw S_BIOS
        db "BIOS", 0
        dw S_BIOS
        db "MEMORIA", 0
        dw S_VESA
        db "VESA", 0
        dw S_VESA
        db "VIDEO", 0
        dw S_PCI
        db "PCI", 0
        dw S_DISCOS
        db "DISCOS", 0
        dw S_RES
        db "RES", 0
        dw S_RES
        db "RESIDENTES", 0
        dw S_ISA
        db "ISA", 0
        dw 0
k_informe   db "INFORME", 0
k_nosonido  db "NOSONIDO", 0

; los apartados, en el orden en que salen (los sondeos de puertos, al final)
sections:
        dw S_CPU, t_cpu, sec_cpu
        dw S_BIOS, t_bios, sec_bios
        dw S_VESA, t_vesa, sec_vesa
        dw S_PCI, t_pci, sec_pci
        dw S_DISCOS, t_discos, sec_discos
        dw S_RES, t_res, sec_res
        dw S_ISA, t_isa, sec_isa
        dw 0
t_cpu       db "PROCESADOR", 0
t_bios      db "BIOS Y MEMORIA", 0
t_vesa      db "VIDEO (VESA)", 0
t_pci       db "PCI", 0
t_discos    db "DISQUETERAS Y DISCOS (INT 13h)", 0
t_res       db "RESIDENTES Y SISTEMA", 0
t_isa       db "ISA Y PUERTOS", 0

run_sections:
        mov bx, sections
.l:
        mov ax, [bx]
        test ax, ax
        jz .r
        test [mask], ax
        jz .n
        push bx
        mov si, [bx+2]
        call titulo
        call word [bx+4]
        call fixseg
        call flush
        call rep_close
        pop bx
.n:
        add bx, 6
        jmp .l
.r:
        ret

; SI -> nombre del paso: se dice en pantalla (informe) y se pone el título
titulo:
        cmp byte [opt_inf], 0
        je .t
        push si
        PR_SCR "  "
        pop si
        push si
        call scr_puts
        PR_SCR "...", NL
        pop si
        call rep_open
.t:
        PR NL, "==== "
        call puts
        PR " ====", NL
        jmp ckpt

; -----------------------------------------------------------------------------
;  Salida
; -----------------------------------------------------------------------------
putc:
        push bx
        mov bx, [opos]
        mov [obuf+bx], al
        inc bx
        mov [opos], bx
        cmp bx, OBUF_SZ
        jae .f
        cmp al, 10
        jne .r
.f:
        call flush
.r:
        pop bx
        ret

flush:
        pushad
        push ds
        push es
        push cs
        pop ds
        mov cx, [opos]
        jcxz .r
        mov word [opos], 0
        mov bx, [out_h]
        cmp bx, 0xFFFF
        je .r
        mov dx, obuf
        mov ah, 0x40
        int 0x21
        jc .full
        cmp ax, cx
        je .r
.full:
        cmp bx, 4
        jbe .r
        mov byte [disk_full], 1
.r:
        pop es
        pop ds
        popad
        ret

; SI -> ASCIIZ; SI queda detrás del cero
puts:
        push ax
.l:
        lodsb
        test al, al
        jz .d
        call putc
        jmp .l
.d:
        pop ax
        ret

; el texto que sigue a la llamada (macro PR)
pr_inline:
        push bp
        mov bp, sp
        xchg si, [bp+2]
        call puts
        xchg si, [bp+2]
        pop bp
        ret

crlf:
        push ax
        mov al, 13
        call putc
        mov al, 10
        call putc
        pop ax
        ret

; AL imprimible o un punto
putc_safe:
        push ax
        cmp al, 32
        jb .p
        cmp al, 127
        jne .o
.p:
        mov al, '.'
.o:
        call putc
        pop ax
        ret

; ES:SI -> texto que no es nuestro (ASCIIZ, CX caracteres como mucho)
puts_far:
        push ax
        push cx
        push si
        jcxz .d
.l:
        mov al, [es:si]
        inc si
        test al, al
        jz .d
        call putc_safe
        loop .l
.d:
        pop si
        pop cx
        pop ax
        ret

; ES:SI -> CX caracteres fijos (sin cero); los espacios del final no salen
puts_fix:
        push ax
        push bx
        push cx
        push si
        ; recortar por la derecha
.tr:
        jcxz .d
        mov bx, cx
        mov al, [es:si+bx-1]
        cmp al, ' '
        je .t1
        test al, al
        jnz .l
.t1:
        dec cx
        jmp .tr
.l:
        mov al, [es:si]
        inc si
        call putc_safe
        loop .l
.d:
        pop si
        pop cx
        pop bx
        pop ax
        ret

hex8:
        push eax
        shr eax, 16
        call hex4
        pop eax
hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        shr al, 4
        call hex1
        pop ax
hex1:
        push ax
        and al, 15
        cmp al, 10
        sbb al, 0x69
        das
        call putc
        pop ax
        ret

; EAX en decimal
putdec:
        pushad
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
        popad
        ret

; AL en decimal con dos cifras
put2d:
        push ax
        aam
        xchg al, ah
        add al, '0'
        call putc
        mov al, ah
        add al, '0'
        call putc
        pop ax
        ret

fixseg:
        push cs
        pop ds
        push cs
        pop es
        cld
        ret

; ---- mensajes a la pantalla (manejador 2), mientras se hace el informe ----
scr_inline:
        push bp
        mov bp, sp
        xchg si, [bp+2]
        call scr_puts
        xchg si, [bp+2]
        pop bp
        ret
; SI -> ASCIIZ; SI queda detrás del cero
scr_puts:
        pushad
        mov dx, si
        xor cx, cx
.l:
        cmp byte [si], 0
        je .w
        inc si
        inc cx
        jmp .l
.w:
        jcxz .r
        mov bx, 2
        mov ah, 0x40
        int 0x21
.r:
        popad
.s:
        lodsb
        test al, al
        jnz .s
        ret

; ---- el archivo del informe ----
; abrirlo para añadir (solo con /INFORME)
rep_open:
        cmp byte [opt_inf], 0
        je .r
        pushad
        call flush
        mov word [out_h], 0xFFFF
        mov dx, rep_path
        mov ax, 0x3D02
        int 0x21
        jc .x
        mov [out_h], ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
.x:
        popad
.r:
        ret
rep_close:
        cmp byte [opt_inf], 0
        je .r
        pushad
        call flush
        mov bx, [out_h]
        mov word [out_h], 0xFFFF
        cmp bx, 0xFFFF
        je .x
        mov ah, 0x3E
        int 0x21
.x:
        popad
.r:
        ret
; punto de control: lo escrito hasta aquí queda en el disco (antes de un
; sondeo delicado). Conserva los registros.
ckpt:
        cmp byte [opt_inf], 0
        je .r
        push ds
        push es
        call fixseg
        call rep_close
        call rep_open
        pop es
        pop ds
.r:
        ret

; =============================================================================
;  PROCESADOR
; =============================================================================
sec_cpu:
        PR "Modo: "
        smsw ax
        test al, 1
        jz .real
        PR "virtual 8086 (hay un gestor de memoria tipo EMM386)", NL
        jmp .m2
.real:
        PR "real", NL
.m2:
        ; coprocesador
        fninit
        mov word [tmpw], 0x5A5A
        fnstsw [tmpw]
        cmp byte [tmpw], 0
        jne .nofpu
        mov byte [has_fpu], 1
.nofpu:
        mov ebx, 0x200000           ; bit ID de EFLAGS: hay CPUID
        call flag_toggles
        jnz .cpuid
        mov ebx, 0x40000            ; bit AC: es un 486
        call flag_toggles
        jnz .i486
        PR "Procesador: 80386 (sin CPUID)", NL
        jmp .fpu
.i486:
        PR "Procesador: 80486 o compatible (sin CPUID)", NL
.fpu:
        PR "Coprocesador matemático: "
        cmp byte [has_fpu], 0
        je .fno
        PR "sí", NL
        ret
.fno:
        PR "no", NL
        ret

.cpuid:
        xor eax, eax
        cpuid
        mov [cpu_max], eax
        mov [strbuf], ebx
        mov [strbuf+4], edx
        mov [strbuf+8], ecx
        PR "Fabricante: "
        mov si, strbuf
        mov cx, 12
        call puts_far
        call crlf
        cmp dword [cpu_max], 1
        jb .ext
        mov eax, 1
        cpuid
        mov [c_regs], edx
        mov [c_regs+4], ecx
        mov [c1_eax], eax
        mov ebx, eax
        shr ebx, 8
        and ebx, 15                 ; familia
        mov ecx, eax
        shr ecx, 4
        and ecx, 15                 ; modelo
        cmp bl, 6
        je .xm
        cmp bl, 15
        jne .nxm
.xm:
        mov edx, eax
        shr edx, 12
        and edx, 0xF0
        or ecx, edx
.nxm:
        cmp bl, 15
        jne .nxf
        mov edx, eax
        shr edx, 20
        and edx, 0xFF
        add ebx, edx
.nxf:
        PR "Familia "
        mov eax, ebx
        call putdec
        PR ", modelo "
        mov eax, ecx
        call putdec
        PR ", stepping "
        mov eax, [c1_eax]
        and eax, 15
        call putdec
        PR " (firma CPUID "
        mov eax, [c1_eax]
        call hex8
        PR ")", NL
.ext:
        mov eax, 0x80000000
        cpuid
        cmp eax, 0x80000001
        jb .noext
        cmp eax, 0x800000FF
        ja .noext
        mov [cpu_emax], eax
        mov eax, 0x80000001
        cpuid
        mov [c_regs+8], edx
        mov [c_regs+12], ecx
        cmp dword [cpu_emax], 0x80000004
        jb .noext
        mov di, strbuf
        mov eax, 0x80000002
.b:
        push eax
        cpuid
        mov [di], eax
        mov [di+4], ebx
        mov [di+8], ecx
        mov [di+12], edx
        add di, 16
        pop eax
        inc eax
        cmp eax, 0x80000004
        jbe .b
        mov si, strbuf
        mov cx, 48
.sp:
        cmp byte [si], ' '
        jne .bn
        inc si
        dec cx
        jnz .sp
.bn:
        cmp byte [si], 0
        je .noext
        PR "Nombre: "
        call puts_far
        call crlf
.noext:
        cmp dword [cpu_max], 1
        jb .nofl
        PR "Banderas: "
        mov si, cpu_flags
.fl:
        lodsb
        cmp al, 0xFF
        je .fle
        movzx bx, al
        shl bx, 2
        mov edx, [c_regs+bx]
        lodsb
        mov cl, al
        shr edx, cl
        test dl, 1
        jz .skip
        call puts
        mov al, ' '
        call putc
        jmp .fl
.skip:
        lodsb
        test al, al
        jnz .skip
        jmp .fl
.fle:
        call crlf
        PR "CPUID(1): EDX="
        mov eax, [c_regs]
        call hex8
        PR " ECX="
        mov eax, [c_regs+4]
        call hex8
        PR "   CPUID(80000001h): EDX="
        mov eax, [c_regs+8]
        call hex8
        PR " ECX="
        mov eax, [c_regs+12]
        call hex8
        call crlf
.nofl:
        test byte [c_regs], 0x10    ; TSC
        jz .r
        call ckpt
        call mide_mhz
.r:
        ret

; ¿se puede cambiar el bit EBX de EFLAGS? -> ZF = 0 si sí
flag_toggles:
        mov dx, sp
        and sp, 0xFFFC
        pushfd
        pop eax
        mov ecx, eax
        xor eax, ebx
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        mov sp, dx
        xor eax, ecx
        test eax, ebx
        ret

; velocidad con el contador TSC durante 4 tics del reloj (con tope)
mide_mhz:
        push es
        xor ax, ax
        mov es, ax
        mov ecx, 0x40000000
        mov al, [es:0x46C]
.w1:
        cmp al, [es:0x46C]
        jne .e1
        dec ecx
        jnz .w1
        jmp .fail
.e1:
        rdtsc
        mov [tsc0], eax
        mov [tsc0+4], edx
        mov bl, [es:0x46C]
        add bl, 4
        mov ecx, 0x40000000
.w2:
        cmp bl, [es:0x46C]
        je .e2
        dec ecx
        jnz .w2
        jmp .fail
.e2:
        rdtsc
        sub eax, [tsc0]
        sbb edx, [tsc0+4]
        mov ebx, 219700             ; microsegundos de 4 tics
        cmp edx, ebx
        jae .fail
        div ebx
        PR "Velocidad medida (TSC): unos "
        call putdec
        PR " MHz", NL
        pop es
        ret
.fail:
        PR "Velocidad: no se ha podido medir (el reloj no avanza)", NL
        pop es
        ret

; registro (0 EDX de CPUID 1, 1 ECX, 2 EDX de 80000001h, 3 ECX), bit, nombre
cpu_flags:
        db 0, 0, "FPU", 0
        db 0, 4, "TSC", 0
        db 0, 5, "MSR", 0
        db 0, 6, "PAE", 0
        db 0, 8, "CX8", 0
        db 0, 9, "APIC", 0
        db 0, 11, "SEP", 0
        db 0, 15, "CMOV", 0
        db 0, 23, "MMX", 0
        db 0, 24, "FXSR", 0
        db 0, 25, "SSE", 0
        db 0, 26, "SSE2", 0
        db 0, 28, "HTT", 0
        db 1, 0, "SSE3", 0
        db 1, 9, "SSSE3", 0
        db 1, 19, "SSE4.1", 0
        db 1, 20, "SSE4.2", 0
        db 1, 28, "AVX", 0
        db 1, 31, "HIPERVISOR", 0
        db 2, 11, "SYSCALL", 0
        db 2, 20, "NX", 0
        db 2, 29, "64BITS", 0
        db 2, 30, "3DNOW+", 0
        db 2, 31, "3DNOW", 0
        db 0xFF

; =============================================================================
;  BIOS Y MEMORIA
; =============================================================================
sec_bios:
        mov ax, 0xF000
        mov es, ax
        PR "Fecha de la BIOS (F000:FFF5): "
        mov si, 0xFFF5
        mov cx, 8
        call puts_fix
        PR "   octeto de modelo (F000:FFFE): "
        mov al, [es:0xFFFE]
        call hex2
        call crlf
        PR "Nombres que aparecen en la ROM (F000h): "
        mov byte [any], 0
        mov bx, rom_names
.vn:
        cmp byte [bx], 0
        je .vne
        call rom_find
        jc .vnn
        mov si, bx
        call puts
        PR "  "
        mov byte [any], 1
.vnn:
        inc bx
        cmp byte [bx-1], 0
        jne .vnn
        jmp .vn
.vne:
        cmp byte [any], 0
        jne .vn2
        PR "ninguno conocido"
.vn2:
        call crlf
        ; tabla de configuración (INT 15h C0h)
        mov ah, 0xC0
        stc
        int 0x15
        jc .noc0
        test ah, ah
        jnz .noc0
        PR "INT 15h C0h: modelo "
        mov al, [es:bx+2]
        call hex2
        PR ", submodelo "
        mov al, [es:bx+3]
        call hex2
        PR ", revisión "
        mov al, [es:bx+4]
        call hex2
        PR ", características "
        mov al, [es:bx+5]
        call hex2
        call crlf
.noc0:
        call fixseg
        call smbios
        call fixseg
        call acpi
        call fixseg
        ; APM
        mov ax, 0x5300
        xor bx, bx
        stc
        int 0x15
        jc .noapm
        cmp bx, 0x504D
        jne .noapm
        push cx
        push ax
        PR "APM: versión "
        pop ax
        push ax
        movzx eax, ah
        call putdec
        mov al, '.'
        call putc
        pop ax
        movzx eax, al
        call putdec
        PR ", banderas "
        pop ax
        call hex4
        call crlf
        jmp .mem
.noapm:
        PR "APM: no hay", NL
.mem:
        call fixseg
        call bios_dumps
        call fixseg
        call ckpt
        ; ---- memoria ----
        PR NL, "Memoria convencional (INT 12h): "
        int 0x12
        movzx eax, ax
        call putdec
        PR " KB", NL
        xor ax, ax
        mov es, ax
        mov ax, [es:0x40E]
        PR "EBDA (40:0E): "
        test ax, ax
        jz .noebda
        push ax
        PR "segmento "
        call hex4
        mov al, 'h'
        call putc
        pop ax
        cmp ax, 0x8000
        jb .ebe
        cmp ax, 0xA000
        jae .ebe
        mov es, ax
        PR ", "
        movzx eax, byte [es:0]
        call putdec
        PR " KB"
.ebe:
        call crlf
        jmp .e820
.noebda:
        PR "no hay", NL
.e820:
        call fixseg
        PR "Mapa de memoria (INT 15h E820h):", NL
        xor ebx, ebx
        mov word [cnt], 0
        mov dword [tot], 0
        mov dword [tot+4], 0
.el:
        mov dword [e820buf+20], 1
        mov eax, 0xE820
        mov edx, 0x534D4150
        mov ecx, 24
        mov di, e820buf
        int 0x15
        push cs
        pop ds
        push cs
        pop es
        jc .eend
        cmp eax, 0x534D4150
        jne .eend
        mov [e820nx], ebx
        inc word [cnt]
        PR "  base "
        mov eax, [e820buf+4]
        call hex8
        mov eax, [e820buf]
        call hex8
        PR "  largo "
        mov eax, [e820buf+12]
        call hex8
        mov eax, [e820buf+8]
        call hex8
        PR "  tipo "
        mov eax, [e820buf+16]
        call putdec
        mov si, e_t1
        cmp eax, 1
        jne .et2
        mov edx, [e820buf+8]
        add [tot], edx
        mov edx, [e820buf+12]
        adc [tot+4], edx
        jmp .etp
.et2:
        mov si, e_t2
        cmp eax, 2
        je .etp
        mov si, e_t3
        cmp eax, 3
        je .etp
        mov si, e_t4
        cmp eax, 4
        je .etp
        mov si, e_t5
        cmp eax, 5
        je .etp
        mov si, e_tx
.etp:
        call puts
        call crlf
        mov ebx, [e820nx]
        test ebx, ebx
        jz .eend
        cmp word [cnt], 64
        jb .el
.eend:
        cld
        cmp word [cnt], 0
        jne .etot
        PR "  no disponible en esta BIOS", NL
        jmp .e801
.etot:
        PR "  Memoria libre total: "
        mov eax, [tot]
        mov edx, [tot+4]
        shrd eax, edx, 20
        call putdec
        PR " MB", NL
.e801:
        xor cx, cx
        xor dx, dx
        mov ax, 0xE801
        stc
        int 0x15
        jc .no801
        push dx
        push cx
        push bx
        push ax
        PR "INT 15h E801h: AX="
        pop ax
        call hex4
        PR " BX="
        pop ax
        call hex4
        PR " CX="
        pop ax
        call hex4
        PR " DX="
        pop ax
        call hex4
        PR " (KB de 1 a 16 MB y bloques de 64 KB por encima)", NL
        jmp .i88
.no801:
        PR "INT 15h E801h: no disponible", NL
.i88:
        call fixseg
        mov ah, 0x88
        stc
        int 0x15
        jc .no88
        push ax
        PR "INT 15h 88h: "
        pop ax
        movzx eax, ax
        call putdec
        PR " KB de memoria extendida (0 si la gestiona un XMS)", NL
        jmp .cmos
.no88:
        PR "INT 15h 88h: no disponible", NL
.cmos:
        call fixseg
        PR "CMOS: memoria extendida "
        mov al, 0x18
        call cmos_rd
        mov ah, al
        mov al, 0x17
        call cmos_rd
        movzx eax, ax
        call putdec
        PR " KB (17h-18h), "
        mov al, 0x31
        call cmos_rd
        mov ah, al
        mov al, 0x30
        call cmos_rd
        movzx eax, ax
        call putdec
        PR " KB (30h-31h)", NL
        ret

cmos_rd:
        pushf
        cli
        out 0x70, al
        jmp short $+2
        in al, 0x71
        popf
        ret

e_t1        db " libre", 0
e_t2        db " reservada", 0
e_t3        db " ACPI (recuperable)", 0
e_t4        db " ACPI NVS", 0
e_t5        db " defectuosa", 0
e_tx        db " (otro)", 0

rom_names   db "American Megatrends", 0, "AMIBIOS", 0, "Award", 0, "Phoenix", 0
            db "Insyde", 0, "SeaBIOS", 0, "Bochs", 0, "Compaq", 0, "COMPAQ", 0
            db "Dell", 0, "TOSHIBA", 0, "Toshiba", 0, "Hewlett-Packard", 0
            db "Apple", 0, "Acer", 0, "ASUS", 0, "Lenovo", 0, "LENOVO", 0
            db "Sony", 0, "Fujitsu", 0, "FUJITSU", 0, "Packard Bell", 0
            db "SAMSUNG", 0, "Quadtel", 0, "MR BIOS", 0, "Chips & Tech", 0
            db "OPTi", 0, "Intel Corp", 0, "VirtualBox", 0, "VMware", 0, 0

; BX -> texto (ASCIIZ): ¿está en el segmento F000h? CF = 1 si no
rom_find:
        push es
        push ax
        push cx
        push dx
        push si
        push di
        mov si, bx
        xor dx, dx
.len:
        lodsb
        test al, al
        jz .l2
        inc dx
        jmp .len
.l2:
        mov ax, 0xF000
        mov es, ax
        xor di, di
        mov cx, 0xFF00
.scan:
        mov al, [bx]
        repne scasb
        jne .nf
        push cx
        push di
        mov si, bx
        inc si
        mov cx, dx
        dec cx
        repe cmpsb
        pop di
        pop cx
        je .found
        jcxz .nf
        jmp .scan
.found:
        clc
        jmp .x
.nf:
        stc
.x:
        pop di
        pop si
        pop dx
        pop cx
        pop ax
        pop es
        ret

; ---- SMBIOS (tablas DMI): fabricante y modelo del sistema, placa, memoria ----
smbios:
        mov ax, 0xF000
        mov es, ax
        xor di, di
.s:
        cmp dword [es:di], 0x5F4D535F       ; "_SM_"
        jne .n
        movzx cx, byte [es:di+5]
        cmp cx, 0x1E
        jb .n
        cmp cx, 0x20
        ja .n
        push di
        xor al, al
.ck:
        add al, [es:di]
        inc di
        loop .ck
        pop di
        test al, al
        jz .found
.n:
        add di, 16
        jnz .s
        ; la entrada de 64 bits (SMBIOS 3)
        xor di, di
.s3:
        cmp dword [es:di], 0x334D535F       ; "_SM3"
        jne .n3
        cmp byte [es:di+4], '_'
        je .found3
.n3:
        add di, 16
        jnz .s3
        PR "SMBIOS: no hay tablas", NL
        ret
.found3:
        mov [sm_ep], di
        mov al, [es:di+6]
        mov [sm_eplen], al
        mov ax, [es:di+7]
        mov [sm_ver], ax
        mov eax, [es:di+0x0C]
        mov [sm_left], eax
        mov eax, [es:di+0x10]
        mov [sm_addr], eax
        mov word [sm_cnt], 400
        cmp dword [es:di+0x14], 0
        je .go
        PR "SMBIOS 3: tablas por encima de 4 GB (no se leen)", NL
        ret
.found:
        mov [sm_ep], di
        mov al, [es:di+5]
        mov [sm_eplen], al
        mov ax, [es:di+6]
        mov [sm_ver], ax
        movzx eax, word [es:di+0x16]
        mov [sm_left], eax
        mov eax, [es:di+0x18]
        mov [sm_addr], eax
        mov ax, [es:di+0x1C]
        cmp ax, 400
        jbe .c1
        mov ax, 400
.c1:
        mov [sm_cnt], ax
.go:
        push cs
        pop es
        PR "SMBIOS "
        movzx eax, byte [sm_ver]
        call putdec
        mov al, '.'
        call putc
        movzx eax, byte [sm_ver+1]
        call putdec
        PR " (tablas en "
        mov eax, [sm_addr]
        call hex8
        PR "h, "
        mov eax, [sm_left]
        call putdec
        PR " octetos):", NL
        ; el punto de entrada y la tabla entera, en binario
        cmp byte [opt_inf], 0
        je .next
        movzx eax, word [sm_ep]
        add eax, 0xF0000
        mov [d_addr], eax
        movzx eax, byte [sm_eplen]
        mov [d_len], eax
        mov si, n_smep
        call dump_phys
        mov eax, [sm_addr]
        mov [d_addr], eax
        mov eax, [sm_left]
        cmp eax, 0x10000
        jbe .dl
        mov eax, 0x10000
.dl:
        mov [d_len], eax
        mov si, n_sm
        call dump_phys
        PR "  (guardadas en SMBIOS.BIN; el punto de entrada, en SMBIOSEP.BIN)", NL
.next:
        cmp word [sm_cnt], 0
        je .done
        cmp dword [sm_left], 4
        jb .done
        mov esi, [sm_addr]
        mov di, smbuf
        mov cx, SMBUF_SZ
        call physcopy
        jnc .ok
        PR "  (no se ha podido leer la tabla)", NL
        ret
.ok:
        mov word [smbuf+SMBUF_SZ], 0
        mov bx, smbuf
        mov al, [bx]
        mov [sm_type], al
        movzx cx, byte [bx+1]
        cmp cx, 4
        jb .done
        cmp cx, SMBUF_SZ - 8
        ja .done
        call sm_show
        ; fin de la estructura: la parte fija y las cadenas hasta el doble cero
        mov bx, smbuf
        movzx si, byte [bx+1]
        add si, bx
.e:
        cmp si, smbuf + SMBUF_SZ - 2
        jae .done
        cmp word [si], 0
        je .ee
        inc si
        jmp .e
.ee:
        add si, 2
        sub si, smbuf
        movzx eax, si
        add [sm_addr], eax
        sub [sm_left], eax
        jc .done
        dec word [sm_cnt]
        cmp byte [sm_type], 127
        jne .next
.done:
        ret

; una estructura SMBIOS en smbuf (BX)
sm_show:
        mov al, [bx]
        cmp al, 0
        jne .t1
        mov si, sm_l_bv
        mov dl, 4
        call sm_field
        mov si, sm_l_bver
        mov dl, 5
        call sm_field
        mov si, sm_l_bdate
        mov dl, 8
        jmp sm_field
.t1:
        cmp al, 1
        jne .t2
        mov si, sm_l_sm
        mov dl, 4
        call sm_field
        mov si, sm_l_sp
        mov dl, 5
        call sm_field
        mov si, sm_l_sv
        mov dl, 6
        jmp sm_field
.t2:
        cmp al, 2
        jne .t3
        mov si, sm_l_pm
        mov dl, 4
        call sm_field
        mov si, sm_l_pp
        mov dl, 5
        call sm_field
        mov si, sm_l_pv
        mov dl, 6
        jmp sm_field
.t3:
        cmp al, 3
        jne .t4
        mov si, sm_l_cm
        mov dl, 4
        call sm_field
        cmp byte [bx+1], 5
        jbe .r
        PR "  Chasis, tipo: "
        movzx eax, byte [bx+5]
        and al, 0x7F
        call putdec
        cmp al, 8
        jb .c0
        cmp al, 10
        jbe .cp
        cmp al, 14
        je .cp
        cmp al, 31
        je .cp
        cmp al, 32
        je .cp
.c0:
        cmp al, 3
        je .cs
        cmp al, 4
        je .cs
        cmp al, 6
        je .cs
        cmp al, 7
        jne .ce
.cs:
        PR " (sobremesa o torre)"
        jmp .ce
.cp:
        PR " (portátil)"
.ce:
        jmp crlf
.t4:
        cmp al, 4
        jne .t17
        mov si, sm_l_cs
        mov dl, 4
        call sm_field
        mov si, sm_l_cf
        mov dl, 7
        call sm_field
        mov si, sm_l_cv
        mov dl, 0x10
        call sm_field
        cmp byte [bx+1], 0x17
        jbe .r
        PR "  Procesador, velocidad: "
        movzx eax, word [bx+0x16]
        call putdec
        PR " MHz (máxima "
        movzx eax, word [bx+0x14]
        call putdec
        PR ")", NL
.r:
        ret
.t17:
        cmp al, 17
        jne sm_more
        cmp byte [bx+1], 0x15
        jbe .r
        mov ax, [bx+0x0C]
        test ax, ax
        jz .r                       ; zócalo vacío
        PR "  Memoria, módulo "
        push ax
        mov al, [bx+0x10]
        call sm_str
        pop ax
        PR ": "
        cmp ax, 0xFFFF
        je .munk
        cmp ax, 0x7FFF
        jne .m1
        cmp byte [bx+1], 0x1F
        jbe .munk
        mov eax, [bx+0x1C]
        and eax, 0x7FFFFFFF
        call putdec
        PR " MB"
        jmp .mspd
.m1:
        test ah, 0x80
        jz .mmb
        and ah, 0x7F
        movzx eax, ax
        call putdec
        PR " KB"
        jmp .mspd
.mmb:
        movzx eax, ax
        call putdec
        PR " MB"
        jmp .mspd
.munk:
        PR "tamaño desconocido"
.mspd:
        cmp byte [bx+1], 0x16
        jbe .mend
        movzx eax, word [bx+0x15]
        test ax, ax
        jz .mend
        PR ", "
        call putdec
        PR " MHz"
.mend:
        cmp byte [bx+1], 0x1A
        jbe .mfin
        PR ", fabricante "
        mov al, [bx+0x17]
        call sm_str
        PR ", referencia "
        mov al, [bx+0x1A]
        call sm_str
.mfin:
        jmp crlf

; los tipos 16 (memoria), 9 (ranuras) y 41 (dispositivos integrados)
sm_more:
        cmp al, 16
        jne .t9
        cmp byte [bx+1], 0x0E
        jbe .r
        PR "  Memoria, zócalos: "
        movzx eax, word [bx+0x0D]
        call putdec
        PR ", máximo "
        mov eax, [bx+7]
        cmp eax, 0x80000000
        je .big
        shr eax, 10
        call putdec
        PR " MB", NL
        ret
.big:
        PR "más de 2 TB", NL
.r:
        ret
.t9:
        cmp al, 9
        jne .t41
        cmp byte [bx+1], 0x0B
        jbe .r
        PR "  Ranura "
        mov al, [bx+4]
        call sm_str
        PR ": tipo "
        mov al, [bx+5]
        call hex2
        mov al, 'h'
        call putc
        mov al, [bx+5]
        mov si, sl_pci
        cmp al, 0x06
        je .sn
        mov si, sl_pcc
        cmp al, 0x07
        je .sn
        mov si, sl_pcix
        cmp al, 0x12
        je .sn
        mov si, sl_agp
        cmp al, 0x0F
        jb .s0
        cmp al, 0x13
        jbe .sn
.s0:
        mov si, sl_pcie
        cmp al, 0xA5
        jb .s1
        cmp al, 0xB6
        jbe .sn
.s1:
        mov si, sl_nada
.sn:
        call puts
        PR ", "
        mov al, [bx+7]
        cmp al, 3
        jne .u4
        PR "libre", NL
        ret
.u4:
        cmp al, 4
        jne .ux
        PR "ocupada", NL
        ret
.ux:
        PR "uso "
        call hex2
        PR "h", NL
        ret
.t41:
        cmp al, 41
        jne .r
        cmp byte [bx+1], 0x0A
        jbe .r
        PR "  Integrado "
        mov al, [bx+4]
        call sm_str
        PR ": "
        mov al, [bx+5]
        and al, 0x7F
        mov si, ob_vid
        cmp al, 3
        je .on
        mov si, ob_eth
        cmp al, 5
        je .on
        mov si, ob_snd
        cmp al, 7
        je .on
        mov si, ob_sata
        cmp al, 9
        je .on
        PR "tipo "
        call hex2
        mov si, sl_nada
.on:
        call puts
        test byte [bx+5], 0x80
        jnz .act
        PR " (desactivado)"
.act:
        PR ", PCI "
        mov al, [bx+9]
        call hex2
        mov al, ':'
        call putc
        mov al, [bx+10]
        shr al, 3
        call hex2
        mov al, '.'
        call putc
        mov al, [bx+10]
        and al, 7
        call hex1
        jmp crlf
sl_pci      db " (PCI)", 0
sl_pcc      db " (PC Card)", 0
sl_pcix     db " (PCI-X)", 0
sl_agp      db " (AGP)", 0
sl_pcie     db " (PCI Express)", 0
sl_nada     db 0
ob_vid      db "vídeo", 0
ob_eth      db "red Ethernet", 0
ob_snd      db "sonido", 0
ob_sata     db "SATA", 0

; SI = rótulo, DL = desplazamiento del número de cadena; BX = estructura
sm_field:
        cmp [bx+1], dl
        jbe .r
        push bx
        movzx di, dl
        mov al, [bx+di]
        pop bx
        test al, al
        jz .r
        call puts
        call sm_str
        call crlf
.r:
        ret

; AL = número de cadena (1...) de la estructura BX: se escribe
sm_str:
        push ax
        push cx
        push si
        test al, al
        jz .no
        movzx si, byte [bx+1]
        add si, bx
        mov cl, al
.f:
        dec cl
        jz .p
.sk:
        cmp si, smbuf + SMBUF_SZ
        jae .no
        cmp byte [si], 0
        je .sn
        inc si
        jmp .sk
.sn:
        inc si
        cmp byte [si], 0
        je .no                      ; no hay tantas cadenas
        jmp .f
.p:
        mov cx, 80
        call puts_far               ; (ES = nuestro segmento)
        jmp .x
.no:
        PR "(sin dato)"
.x:
        pop si
        pop cx
        pop ax
        ret

sm_l_bv     db "  BIOS, fabricante: ", 0
sm_l_bver   db "  BIOS, versión: ", 0
sm_l_bdate  db "  BIOS, fecha: ", 0
sm_l_sm     db "  Sistema, fabricante: ", 0
sm_l_sp     db "  Sistema, modelo: ", 0
sm_l_sv     db "  Sistema, versión: ", 0
sm_l_pm     db "  Placa, fabricante: ", 0
sm_l_pp     db "  Placa, modelo: ", 0
sm_l_pv     db "  Placa, versión: ", 0
sm_l_cm     db "  Chasis, fabricante: ", 0
sm_l_cs     db "  Procesador, zócalo: ", 0
sm_l_cf     db "  Procesador, fabricante: ", 0
sm_l_cv     db "  Procesador, nombre: ", 0

; ---- ACPI: la firma "RSD PTR " (en la EBDA o entre E0000h y FFFFFh) ----
acpi:
        xor ax, ax
        mov es, ax
        mov ax, [es:0x40E]
        cmp ax, 0x8000
        jb .rom
        cmp ax, 0xA000
        jae .rom
        mov es, ax
        xor di, di
        mov cx, 64
        call .scan
        jnc .found
.rom:
        mov ax, 0xE000
        mov es, ax
        xor di, di
        mov cx, 0x1000
        call .scan
        jnc .found
        mov ax, 0xF000
        mov es, ax
        xor di, di
        mov cx, 0x1000
        call .scan
        jnc .found
        PR "ACPI: no hay", NL
        ret
.found:
        ; la RSDP (36 octetos) a nuestro segmento
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
        mov [d_addr], eax
        PR "ACPI: RSDP en "
        call hex8
        PR "h, revisión "
        movzx eax, byte [rsdp+15]
        call putdec
        PR ", OEM "
        mov si, rsdp + 9
        mov cx, 6
        call puts_fix
        call crlf
        mov dword [d_len], 20
        cmp byte [rsdp+15], 2
        jb .d1
        mov dword [d_len], 36
.d1:
        mov si, n_rsdp
        call dump_phys_inf
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
        jz .noroot
        mov esi, eax
        mov di, rootbuf
        mov cx, 1024
        call physcopy
        jc .noroot
        mov eax, [rootbuf+4]
        cmp eax, 36
        jb .noroot
        sub eax, 36
        cmp eax, 1024 - 36
        jbe .n1
        mov eax, 1024 - 36
.n1:
        movzx ecx, byte [a_ent]
        xor edx, edx
        div ecx
        mov [a_n], ax
        PR "  Tablas (firma, revisión, tamaño, OEM, dirección, suma de comprobación):", NL
        mov word [a_nseen], 0
        mov dword [a_dsdt], 0
        mov dword [a_facs], 0
        mov eax, [a_root]
        call acpi_tabla
        mov word [a_i], 0
.tl:
        mov ax, [a_i]
        cmp ax, [a_n]
        jae .te
        movzx bx, byte [a_ent]
        mul bx
        mov bx, ax
        mov eax, [rootbuf+36+bx]
        cmp byte [a_ent], 8
        jne .t4
        cmp dword [rootbuf+40+bx], 0
        je .t4
        PR "  (tabla por encima de 4 GB: no se lee)", NL
        jmp .tn
.t4:
        test eax, eax
        jz .tn
        call acpi_tabla
.tn:
        inc word [a_i]
        jmp .tl
.te:
        mov eax, [a_dsdt]
        test eax, eax
        jz .nd
        call acpi_tabla
.nd:
        mov eax, [a_facs]
        test eax, eax
        jz .nf
        mov byte [a_isfacs], 1
        call acpi_tabla
        mov byte [a_isfacs], 0
.nf:
        ret
.noroot:
        PR "  (no se puede leer la RSDT/XSDT)", NL
        ret
.scan:
        cmp dword [es:di], 0x20445352       ; "RSD "
        jne .sn
        cmp dword [es:di+4], 0x20525450     ; "PTR "
        jne .sn
        clc
        ret
.sn:
        add di, 16
        loop .scan
        stc
        ret

; EAX = dirección de una tabla ACPI: una línea en el informe y, con /INFORME,
; la tabla entera en ACPI\FIRMA.BIN (se comprueban la longitud y la suma)
acpi_tabla:
        mov [d_addr], eax
        call flush
        mov esi, eax
        mov di, hdrbuf
        mov cx, 64
        call physcopy
        jnc .ok
        PR "  (no se puede leer la tabla en "
        mov eax, [d_addr]
        call hex8
        PR "h)", NL
        ret
.ok:
        mov si, hdrbuf
        mov cx, 4
.v:
        lodsb
        cmp al, 0x20
        jb .bad
        cmp al, 0x7E
        ja .bad
        loop .v
        mov eax, [hdrbuf+4]
        cmp eax, 36
        jb .bad
        cmp eax, 0x200000
        ja .bad
        mov [d_len], eax
        PR "  "
        mov si, hdrbuf
        mov cx, 4
        call puts_far
        cmp byte [a_isfacs], 0
        jne .facs
        PR "  rev. "
        movzx eax, byte [hdrbuf+8]
        call putdec
        PR "  "
        mov eax, [d_len]
        call putdec
        PR " octetos  OEM "
        mov si, hdrbuf + 10
        mov cx, 6
        call puts_fix
        mov al, ' '
        call putc
        mov si, hdrbuf + 16
        mov cx, 8
        call puts_fix
        jmp .c
.facs:
        PR "  "
        mov eax, [d_len]
        call putdec
        PR " octetos"
.c:
        PR "  en "
        mov eax, [d_addr]
        call hex8
        mov al, 'h'
        call putc
        cmp dword [hdrbuf], "FACP"
        jne .s1
        mov eax, [hdrbuf+36]
        mov [a_facs], eax
        mov eax, [hdrbuf+40]
        mov [a_dsdt], eax
.s1:
        cmp dword [hdrbuf], "MCFG"
        jne .s2
        cmp dword [d_len], 60
        jb .s2
        cmp dword [hdrbuf+48], 0
        jne .s2
        mov eax, [hdrbuf+44]
        mov [mcfg_base], eax
        mov ax, [hdrbuf+54]
        mov [mcfg_b0], ax
.s2:
        call acpi_name
        call dump_phys              ; se lee entera (y se guarda con /INFORME)
        cmp byte [d_err], 0
        je .s3
        PR "  (lectura incompleta)", NL
        ret
.s3:
        cmp byte [a_isfacs], 0
        jne .s5
        cmp byte [d_sum], 0
        je .s4
        PR "  suma MAL"
        jmp .s5
.s4:
        PR "  suma bien"
.s5:
        cmp byte [opt_inf], 0
        je .s6
        PR "  -> "
        mov si, fname
        call puts
.s6:
        jmp crlf
.bad:
        PR "  (no hay una tabla válida en "
        mov eax, [d_addr]
        call hex8
        PR "h)", NL
        ret

; el nombre del archivo de la tabla de hdrbuf -> fname (SI): ACPI\DSDT.BIN,
; ACPI\SSDT1.BIN, SSDT2... (las repetidas, con número)
acpi_name:
        push eax
        push bx
        push cx
        push di
        mov eax, [hdrbuf]
        xor cx, cx
        xor bx, bx
.l:
        cmp bx, [a_nseen]
        jae .le
        push bx
        shl bx, 2
        cmp eax, [a_seen+bx]
        pop bx
        jne .ln
        inc cx
.ln:
        inc bx
        jmp .l
.le:
        cmp bx, 60
        jae .full
        shl bx, 2
        mov [a_seen+bx], eax
        inc word [a_nseen]
.full:
        mov di, fname
        mov dword [di], "ACPI"
        mov byte [di+4], '\'
        add di, 5
        mov si, hdrbuf
        push cx
        mov cx, 4
.c:
        lodsb
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        cmp al, '0'
        jb .c2
        cmp al, '9'
        jbe .c3
        cmp al, 'A'
        jb .c2
        cmp al, 'Z'
        jbe .c3
.c2:
        mov al, '_'
.c3:
        stosb
        loop .c
        pop cx
        cmp dword [hdrbuf], "SSDT"
        je .num
        jcxz .ext
.num:
        inc cx
        mov ax, cx
        cmp al, 10
        jb .n1
        aam
        add ah, '0'
        mov [di], ah
        inc di
.n1:
        add al, '0'
        stosb
.ext:
        mov dword [di], ".BIN"
        mov byte [di+4], 0
        mov si, fname
        pop di
        pop cx
        pop bx
        pop eax
        ret

; ---- CMOS, IVT, BDA, EBDA y los textos de la ROM, a archivos (/INFORME) ----
bios_dumps:
        cmp byte [opt_inf], 0
        je .r
        mov di, dbuf
        xor bx, bx
.c:
        xor al, al
        cmp bl, 0x0C                ; (leer el 0Ch borra los avisos del reloj)
        je .c1
        mov al, bl
        call cmos_rd
.c1:
        stosb
        inc bx
        cmp bx, 128
        jb .c
        mov si, n_cmos
        mov dx, dbuf
        mov cx, 128
        call dump_buf
        mov dword [d_addr], 0
        mov dword [d_len], 1024
        mov si, n_ivt
        call dump_phys
        mov dword [d_addr], 0x400
        mov dword [d_len], 256
        mov si, n_bda
        call dump_phys
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x40E]
        pop es
        cmp ax, 0x8000
        jb .ne
        cmp ax, 0xA000
        jae .ne
        push es
        mov es, ax
        movzx ecx, byte [es:0]
        pop es
        movzx eax, ax
        shl eax, 4
        mov [d_addr], eax
        test ecx, ecx
        jz .ne
        cmp ecx, 16
        jbe .e1
        mov ecx, 16
.e1:
        shl ecx, 10
        mov [d_len], ecx
        mov si, n_ebda
        call dump_phys
.ne:
        mov ax, 0xF000
        xor cx, cx
        mov si, n_rombios
        call rom_strings
        PR "Guardados: CMOS.BIN (128 octetos; el registro 0Ch no se lee), IVT.BIN, BDA.BIN,", NL
        PR "  EBDA.BIN y ROMBIOS.TXT (los textos de la ROM de F000h).", NL
.r:
        ret

; AX = segmento, CX = octetos (0 = 64 KB), SI = nombre: los textos de 8 o más
; caracteres seguidos, uno por línea, a un archivo (/INFORME)
rom_strings:
        cmp byte [opt_inf], 0
        je .r
        pushad
        push es
        mov [rs_seg], ax
        mov [rs_len], cx
        call rep_close
        call mkpath
        mov dx, fpath
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .x
        mov [out_h], ax
        mov es, [rs_seg]
        xor si, si
        xor bx, bx
        mov cx, [rs_len]
.l:
        mov al, [es:si]
        cmp al, 32
        jb .np
        cmp al, 126
        ja .np
        inc bx
        jmp .n
.np:
        call .emit
.n:
        inc si
        dec cx
        jnz .l
        call .emit
        call flush
        mov bx, [out_h]
        mov ah, 0x3E
        int 0x21
.x:
        mov word [out_h], 0xFFFF
        pop es
        popad
        call rep_open
.r:
        ret
.emit:
        cmp bx, 8
        jb .e0
        push si
        push cx
        ; ¿parece texto? (4 de cada 5 son letras, cifras o espacios, y hay al
        ; menos 5 caracteres distintos: el código máquina «imprimible» no pasa)
        push di
        push es
        push cs
        pop es
        mov di, rs_map
        xor ax, ax
        mov cx, 8
        rep stosw
        pop es
        pop di
        mov word [rs_good], 0
        mov word [rs_dist], 0
        mov cx, bx
        sub si, bx
        push si
.x1:
        mov al, [es:si]
        inc si
        cmp al, ' '
        je .gd
        cmp al, '0'
        jb .ng
        cmp al, '9'
        jbe .gd
        cmp al, 'A'
        jb .ng
        cmp al, 'Z'
        jbe .gd
        cmp al, 'a'
        jb .ng
        cmp al, 'z'
        ja .ng
.gd:
        inc word [rs_good]
.ng:
        push bx
        push cx
        movzx bx, al
        sub bx, 32
        mov cl, bl
        and cl, 7
        shr bx, 3
        mov ah, 1
        shl ah, cl
        test [rs_map+bx], ah
        jnz .sn
        or [rs_map+bx], ah
        inc word [rs_dist]
.sn:
        pop cx
        pop bx
        loop .x1
        pop si
        cmp word [rs_dist], 5
        jb .rej
        movzx eax, word [rs_good]
        lea eax, [eax*4+eax]
        movzx ecx, bx
        shl ecx, 2
        cmp eax, ecx
        jb .rej
        mov cx, bx
.e1:
        mov al, [es:si]
        inc si
        call putc
        loop .e1
        call crlf
.rej:
        pop cx
        pop si
.e0:
        xor bx, bx
        ret

; =============================================================================
;  Archivos de la carpeta del informe (solo con /INFORME)
; =============================================================================
; SI -> nombre relativo -> fpath = carpeta del informe + nombre
mkpath:
        push ax
        push si
        push di
        push es
        push cs
        pop es
        push si
        mov si, dir_path
        mov di, fpath
.a:
        lodsb
        test al, al
        jz .b
        stosb
        jmp .a
.b:
        pop si
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop es
        pop di
        pop si
        pop ax
        ret

; SI -> nombre relativo: se crea el archivo (dmp_h). CF si no (o sin /INFORME)
dump_create:
        mov word [dmp_h], 0xFFFF
        cmp byte [opt_inf], 0
        je .no
        pushad
        call mkpath
        mov dx, fpath
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .f
        mov [dmp_h], ax
        popad
        clc
        ret
.f:
        popad
.no:
        stc
        ret
; DX -> datos, CX = octetos
dump_write:
        pushad
        mov bx, [dmp_h]
        cmp bx, 0xFFFF
        je .x
        mov ah, 0x40
        int 0x21
        jc .full
        cmp ax, cx
        je .x
.full:
        mov byte [disk_full], 1
.x:
        popad
        ret
dump_close:
        pushad
        mov bx, [dmp_h]
        cmp bx, 0xFFFF
        je .x
        mov ah, 0x3E
        int 0x21
        mov word [dmp_h], 0xFFFF
.x:
        popad
        ret
; SI -> nombre, DX -> datos, CX = octetos: el archivo entero
dump_buf:
        call dump_create
        jc .r
        call dump_write
        call dump_close
.r:
        ret
; SI -> nombre; d_addr = dirección física, d_len = octetos: se lee entero (de
; 1 KB en 1 KB), se suma (d_sum) y, con /INFORME, se guarda. d_err si no se
; pudo leer.
dump_phys_inf:
        cmp byte [opt_inf], 0
        jne dump_phys
        ret
dump_phys:
        pushad
        call dump_create
        mov byte [d_sum], 0
        mov byte [d_err], 0
.l:
        mov ecx, [d_len]
        test ecx, ecx
        jz .e
        cmp ecx, 1024
        jbe .c
        mov ecx, 1024
.c:
        mov esi, [d_addr]
        mov di, dbuf
        call physcopy
        jc .err
        push cx
        mov si, dbuf
        mov al, [d_sum]
.s:
        add al, [si]
        inc si
        loop .s
        mov [d_sum], al
        pop cx
        mov dx, dbuf
        call dump_write
        movzx ecx, cx
        add [d_addr], ecx
        sub [d_len], ecx
        jmp .l
.err:
        mov byte [d_err], 1
.e:
        call dump_close
        popad
        ret

; ---- esperas con tope: CX = tics de 55 ms (y un tope de vueltas por si el
; reloj no avanzara) ----
t_ini:
        push ax
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        add ax, cx
        inc ax
        mov [cs:t_fin], ax
        mov dword [cs:t_cnt], 0x00200000
        pop es
        pop ax
        ret
; CF = 1 si se acabó el tiempo
t_fuera:
        push ax
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        sub ax, [cs:t_fin]
        pop es
        jns .out
        dec dword [cs:t_cnt]
        jz .out
        pop ax
        clc
        ret
.out:
        pop ax
        stc
        ret

n_rsdp      db "ACPI\RSDP.BIN", 0
n_smep      db "SMBIOSEP.BIN", 0
n_sm        db "SMBIOS.BIN", 0
n_cmos      db "CMOS.BIN", 0
n_ivt       db "IVT.BIN", 0
n_bda       db "BDA.BIN", 0
n_ebda      db "EBDA.BIN", 0
n_rombios   db "ROMBIOS.TXT", 0
n_romvid    db "ROMVIDEO.TXT", 0
n_vbe       db "VBE.BIN", 0
n_vbem      db "VBEMODOS.BIN", 0
n_edid      db "EDID.BIN", 0

; =============================================================================
;  Leer memoria física (tablas SMBIOS y registros de los dispositivos PCI)
; =============================================================================
; ESI = dirección física, DI = destino (nuestro segmento), CX = octetos (se
; redondea a 4 por arriba). CF si no se puede.
;  - por debajo de 1 MB: directo;
;  - en modo V86: INT 15h AH=87h (lo atiende el gestor de memoria);
;  - en modo real: modo «irreal» (FS con límite de 4 GB), con A20 abierta, y
;    lecturas de 32 bits, como piden los registros de los dispositivos.
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
        ; (CF de la BIOS)
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
        mov ah, al
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        mov [0x500], ah
        jne .open
        cmp ah, [es:0x510]
        jne .open
        xor al, al
        jmp .x
.open:
        or al, 1
.x:
        pop ax
        pop es
        pop ds
        ret

; =============================================================================
;  VÍDEO (VESA)
; =============================================================================
sec_vesa:
        mov ah, 0x0F
        int 0x10
        push ax
        PR "Modo de vídeo actual: "
        pop ax
        push ax
        and al, 0x7F
        call hex2
        PR "h, "
        pop ax
        movzx eax, ah
        call putdec
        PR " columnas", NL
        mov ax, 0x1A00
        int 0x10
        cmp al, 0x1A
        jne .novga
        push bx
        PR "Adaptador (INT 10h 1A00h): código "
        pop bx
        mov al, bl
        call hex2
        cmp bl, 7
        jb .a1
        cmp bl, 8
        ja .a1
        PR "h (VGA)"
        jmp .a2
.a1:
        PR "h"
.a2:
        call crlf
        jmp .vbe
.novga:
        PR "Adaptador: anterior a la VGA (INT 10h 1A00h no responde)", NL
.vbe:
        call fixseg
        call ckpt
        mov di, vbebuf
        mov dword [di], 0x32454256  ; "VBE2"
        mov ax, 0x4F00
        int 0x10
        call fixseg
        cmp ax, 0x004F
        jne .none
        cmp dword [vbebuf], 0x41534556      ; "VESA"
        je .ok
.none:
        PR "VESA: no hay (la BIOS de vídeo no ofrece VBE)", NL
        jmp .rom
.ok:
        PR "VESA VBE "
        movzx eax, byte [vbebuf+5]
        call putdec
        mov al, '.'
        call putc
        movzx eax, byte [vbebuf+4]
        call putdec
        PR ", memoria de vídeo "
        movzx eax, word [vbebuf+0x12]
        shl eax, 6
        call putdec
        PR " KB", NL
        mov si, n_vbe
        mov dx, vbebuf
        mov cx, 512
        call dump_buf
        PR "  Nombre (OEM): "
        mov bx, vbebuf + 6
        call vbe_farstr
        cmp word [vbebuf+4], 0x0200
        jb .modes
        PR "  Fabricante: "
        mov bx, vbebuf + 0x16
        call vbe_farstr
        PR "  Producto: "
        mov bx, vbebuf + 0x1A
        call vbe_farstr
        PR "  Revisión: "
        mov bx, vbebuf + 0x1E
        call vbe_farstr
.modes:
        ; copiar la lista de modos (puede estar en el propio búfer)
        les si, [vbebuf+0x0E]
        mov di, modelist
        xor cx, cx
        mov ax, es
        test ax, ax
        jz .mc
.ml:
        mov ax, [es:si]
        cmp ax, 0xFFFF
        je .mc
        mov [di], ax
        add si, 2
        add di, 2
        inc cx
        cmp cx, 160
        jb .ml
.mc:
        mov [cnt], cx
        call fixseg
        PR "  Modos ("
        movzx eax, word [cnt]
        call putdec
        PR "; p = paleta, d = color directo, n = planos; L = con memoria lineal;"
        PR " T = texto):"
        mov si, n_vbem
        call dump_create
        mov dword [lfb], 0
        mov si, modelist
        mov byte [col], 0
.mi:
        cmp word [cnt], 0
        je .me
        dec word [cnt]
        lodsw
        push si
        mov cx, ax
        mov [tmpw], ax
        mov di, modebuf
        push di
        push cx
        mov cx, 128
        xor ax, ax
        rep stosw
        pop cx
        pop di
        mov ax, 0x4F01
        int 0x10
        call fixseg
        cmp ax, 0x004F
        jne .mn
        mov dx, tmpw                ; el número de modo y sus 256 octetos
        mov cx, 2
        call dump_write
        mov dx, modebuf
        mov cx, 256
        call dump_write
        test byte [modebuf], 1
        jz .mn
        test byte [col], 3
        jnz .m1
        call crlf
        PR "   "
.m1:
        inc byte [col]
        mov al, ' '
        call putc
        mov ax, [tmpw]
        call hex4
        mov al, ':'
        call putc
        movzx eax, word [modebuf+0x12]
        call putdec
        mov al, 'x'
        call putc
        movzx eax, word [modebuf+0x14]
        call putdec
        test byte [modebuf], 0x10
        jz .mt
        mov al, 'x'
        call putc
        movzx eax, byte [modebuf+0x19]
        call putdec
        mov ah, [modebuf+0x1B]
        mov al, 'p'
        cmp ah, 4
        je .mm
        mov al, 'd'
        cmp ah, 6
        je .mm
        mov al, 'n'
        cmp ah, 3
        je .mm
        mov al, 'o'
.mm:
        call putc
        test byte [modebuf], 0x80
        jz .mn
        mov al, 'L'
        call putc
        mov eax, [modebuf+0x28]
        mov [lfb], eax
        jmp .mn
.mt:
        mov al, 'T'
        call putc
.mn:
        pop si
        jmp .mi
.me:
        call dump_close
        call crlf
        cmp dword [lfb], 0
        je .ed
        PR "  Memoria lineal (LFB) en "
        mov eax, [lfb]
        call hex8
        PR "h", NL
.ed:
        call edid
.rom:
        call fixseg
        cmp byte [opt_inf], 0
        je .r
        ; los textos de la ROM de vídeo (C000h)
        mov ax, 0xC000
        mov es, ax
        cmp word [es:0], 0xAA55
        jne .r
        movzx cx, byte [es:2]
        push cs
        pop es
        cmp cx, 128
        jb .rs
        xor cx, cx
.rs:
        shl cx, 9
        mov si, n_romvid
        call rom_strings
        PR "Guardados: VBE.BIN, VBEMODOS.BIN (número de modo y 256 octetos por modo),", NL
        PR "  EDID.BIN (si hay EDID) y ROMVIDEO.TXT (los textos de la ROM de vídeo).", NL
.r:
        push cs
        pop es
        ret

; ---- EDID de la pantalla (VBE/DDC) ----
edid:
        PR "EDID de la pantalla (VBE/DDC, INT 10h 4F15h): "
        call ckpt
        mov di, edidbuf
        mov cx, 64
        xor ax, ax
        rep stosw
        mov ax, 0x4F15
        mov bl, 1
        xor cx, cx
        xor dx, dx
        mov di, edidbuf
        int 0x10
        call fixseg
        cmp ax, 0x004F
        jne .no
        cmp dword [edidbuf], 0xFFFFFF00
        jne .no
        cmp dword [edidbuf+4], 0x00FFFFFF
        jne .no
        mov si, n_edid
        mov dx, edidbuf
        mov cx, 128
        call dump_buf
        PR "fabricante "
        mov ax, [edidbuf+8]
        xchg al, ah
        mov bx, ax
        shr ax, 10
        and al, 31
        add al, '@'
        call putc_safe
        mov ax, bx
        shr ax, 5
        and al, 31
        add al, '@'
        call putc_safe
        mov al, bl
        and al, 31
        add al, '@'
        call putc_safe
        PR ", producto "
        mov ax, [edidbuf+10]
        call hex4
        PR "h, año "
        movzx eax, byte [edidbuf+17]
        add eax, 1990
        call putdec
        PR ", EDID "
        movzx eax, byte [edidbuf+18]
        call putdec
        mov al, '.'
        call putc
        movzx eax, byte [edidbuf+19]
        call putdec
        PR ", "
        movzx eax, byte [edidbuf+21]
        call putdec
        mov al, 'x'
        call putc
        movzx eax, byte [edidbuf+22]
        call putdec
        PR " cm", NL
        cmp word [edidbuf+54], 0
        je .nn
        PR "  Resolución nativa: "
        movzx eax, byte [edidbuf+58]
        shr al, 4
        shl ax, 8
        mov al, [edidbuf+56]
        call putdec
        mov al, 'x'
        call putc
        movzx eax, byte [edidbuf+61]
        shr al, 4
        shl ax, 8
        mov al, [edidbuf+59]
        call putdec
        call crlf
.nn:
        mov bx, edidbuf + 54
        mov cx, 4
.d:
        cmp dword [bx], 0xFC000000
        jne .dn
        PR "  Nombre: "
        lea si, [bx+5]
        push cx
        mov cx, 13
.c:
        lodsb
        cmp al, 0x0A
        je .ce
        call putc_safe
        loop .c
.ce:
        pop cx
        call crlf
.dn:
        add bx, 18
        loop .d
        ret
.no:
        PR "no disponible (la BIOS no lo da o la pantalla no contesta)", NL
        ret
; BX -> puntero lejano a un texto de la BIOS de vídeo
vbe_farstr:
        push es
        les si, [bx]
        mov ax, es
        test ax, ax
        jz .fs0
        mov cx, 72
        call puts_far
.fs0:
        pop es
        jmp crlf

; =============================================================================
;  PCI
; =============================================================================
sec_pci:
        ; ¿responde el mecanismo 1 (puertos CF8h/CFCh)?
        pushf
        cli
        mov dx, 0xCF8
        in eax, dx
        mov ebx, eax
        mov eax, 0x80000000
        out dx, eax
        in eax, dx
        mov ecx, eax
        mov eax, ebx
        out dx, eax
        popf
        mov [tot], ecx
        ; lo que dice la BIOS
        mov byte [pci_bios], 0
        mov byte [pci_maxbus], 0
        mov ax, 0xB101
        xor edi, edi
        xor edx, edx
        stc
        int 0x1A
        push cs
        pop ds
        push cs
        pop es
        cld
        jc .nob
        test ah, ah
        jnz .nob
        cmp edx, 0x20494350         ; "PCI "
        jne .nob
        mov byte [pci_bios], 1
        mov [pci_maxbus], cl
        push ax
        push bx
        PR "BIOS PCI "
        pop bx
        push bx
        movzx eax, bh
        call putdec
        mov al, '.'
        call putc
        pop bx
        mov al, bl
        call hex2
        PR ", último bus "
        movzx eax, byte [pci_maxbus]
        call putdec
        PR ", mecanismos "
        pop ax
        call hex2
        PR "h", NL
        jmp .b2
.nob:
        PR "La BIOS no ofrece servicios PCI (INT 1Ah B101h)", NL
.b2:
        cmp dword [tot], 0x80000000
        je .hay
        PR "No hay bus PCI: los puertos CF8h/CFCh no responden (equipo ISA o VLB).", NL
        ret
.hay:
        cmp dword [mcfg_base], 0
        je .hay2
        PR "MMCONFIG (tabla MCFG de ACPI): base "
        mov eax, [mcfg_base]
        call hex8
        PR "h, buses "
        movzx eax, byte [mcfg_b0]
        call putdec
        mov al, '-'
        call putc
        movzx eax, byte [mcfg_b0+1]
        call putdec
        call crlf
.hay2:
        cmp byte [opt_inf], 0
        je .hay3
        PR "(el espacio de configuración de cada función se guarda en PCI\BB_DD_F.BIN,", NL
        PR " 256 octetos; con MMCONFIG, también el extendido de 4 KB en PCI\BB_DD_F.EXT)", NL
.hay3:
        PR "Bus PCI presente (mecanismo 1, puertos CF8h/CFCh). Funciones:", NL
        mov word [p_bdf], 0
        mov word [p_count], 0
.dev:
        call ckpt_pci
        xor al, al
        call pci_rd
        cmp ax, 0xFFFF
        je .nextdev
        test ax, ax
        jz .nextdev
        call pci_show
        mov al, 0x0C
        call pci_rd
        test eax, 0x00800000        ; varias funciones
        jz .nextdev
.fn:
        inc word [p_bdf]
        test byte [p_bdf], 7
        jz .chk
        xor al, al
        call pci_rd
        cmp ax, 0xFFFF
        je .fn
        test ax, ax
        jz .fn
        call pci_show
        jmp .fn
.nextdev:
        and byte [p_bdf], 0xF8
        add word [p_bdf], 8
.chk:
        mov ax, [p_bdf]
        test ax, ax
        jz .end
        cmp ah, [pci_maxbus]
        jbe .dev
.end:
        PR "Total: "
        movzx eax, word [p_count]
        call putdec
        PR " funciones PCI en "
        movzx eax, byte [pci_maxbus]
        inc eax
        call putdec
        PR " buses mirados.", NL
        ret

; cada 16 buses se vacía el búfer de salida
ckpt_pci:
        cmp byte [p_bdf], 0
        jne .r
        test byte [p_bdf+1], 0x0F
        jnz .r
        call flush
.r:
        ret

; AL = registro de configuración (se alinea a 4) de la función p_bdf -> EAX
pci_rd:
        push dx
        push ebx
        movzx ebx, word [p_bdf]
        shl ebx, 8
        mov bl, al
        and bl, 0xFC
        or ebx, 0x80000000
        pushf
        cli
        mov dx, 0xCF8
        mov eax, ebx
        out dx, eax
        mov dl, 0xFC
        in eax, dx
        popf
        pop ebx
        pop dx
        ret

pci_show:
        inc word [p_count]
        xor al, al
        call pci_rd
        mov [p_id], eax
        mov al, 8
        call pci_rd
        mov [p_class], eax
        mov al, 0x0C
        call pci_rd
        shr eax, 16
        and al, 0x7F
        mov [p_hdr], al
        mov al, 4
        call pci_rd
        mov [p_cmd], eax
        ; ---- bus:dispositivo.función  fabricante:dispositivo  clase ----
        mov al, [p_bdf+1]
        call hex2
        mov al, ':'
        call putc
        mov al, [p_bdf]
        shr al, 3
        call hex2
        mov al, '.'
        call putc
        mov al, [p_bdf]
        and al, 7
        add al, '0'
        call putc
        PR "  "
        mov ax, [p_id]
        call hex4
        mov al, ':'
        call putc
        mov ax, [p_id+2]
        call hex4
        mov ax, [p_id]
        call vendor_name
        jc .nv
        PR " ("
        call puts
        PR ")"
.nv:
        PR "  clase "
        mov al, [p_class+3]
        call hex2
        mov ax, [p_class+1]
        call hex4
        PR ": "
        mov eax, [p_class]
        shr eax, 8
        call class_name
        call puts
        PR ", rev. "
        mov al, [p_class]
        call hex2
        call crlf
        ; ---- subsistema, orden y estado ----
        PR SANGRIA
        mov al, 0x2C
        cmp byte [p_hdr], 0
        je .sub
        mov al, 0x40
        cmp byte [p_hdr], 2
        jne .nosub
.sub:
        call pci_rd
        mov ebx, eax
        PR "subsistema "
        mov ax, bx
        call hex4
        mov al, ':'
        call putc
        mov eax, ebx
        shr eax, 16
        call hex4
        PR "  "
.nosub:
        PR "orden "
        mov ax, [p_cmd]
        call hex4
        PR " estado "
        mov ax, [p_cmd+2]
        call hex4
        cmp byte [p_hdr], 0
        je .nobus
        cmp byte [p_hdr], 2
        ja .nobus
        mov al, 0x18
        call pci_rd
        mov ebx, eax
        PR "  buses: primario "
        movzx eax, bl
        call putdec
        PR ", secundario "
        movzx eax, bh
        call putdec
        PR ", subordinado "
        shr ebx, 16
        movzx eax, bl
        call putdec
        cmp bl, 0xFF
        je .nobus
        cmp bl, [pci_maxbus]
        jbe .nobus
        mov [pci_maxbus], bl        ; hay buses más allá de los que dice la BIOS
.nobus:
        call crlf
        ; ---- BAR ----
        mov byte [bar_n], 6
        cmp byte [p_hdr], 0
        je .b0
        mov byte [bar_n], 2
        cmp byte [p_hdr], 1
        je .b0
        mov byte [bar_n], 1
        cmp byte [p_hdr], 2
        je .b0
        mov byte [bar_n], 0
.b0:
        mov byte [bar_i], 0
        mov byte [any], 0
.bar:
        mov al, [bar_i]
        cmp al, [bar_n]
        jae .bare
        shl al, 2
        add al, 0x10
        call pci_rd
        test eax, eax
        jz .barn
        cmp eax, 0xFFFFFFFF
        je .barn
        mov ebx, eax
        cmp byte [any], 0
        jne .b1
        PR SANGRIA
        mov byte [any], 1
.b1:
        PR "BAR"
        mov al, [bar_i]
        add al, '0'
        call putc
        test bl, 1
        jz .mem
        PR " E/S "
        mov eax, ebx
        and al, 0xFC
        test eax, 0xFFFF0000
        jz .io16
        call hex8
        jmp .bsep
.io16:
        call hex4
        jmp .bsep
.mem:
        PR " Mem "
        mov al, bl
        and al, 6
        cmp al, 4
        jne .m32
        inc byte [bar_i]
        mov al, [bar_i]
        shl al, 2
        add al, 0x10
        call pci_rd
        test eax, eax
        jz .m32
        call hex8
.m32:
        mov eax, ebx
        and al, 0xF0
        call hex8
        test bl, 8
        jz .bsep
        PR " (pref.)"
.bsep:
        PR "  "
.barn:
        inc byte [bar_i]
        jmp .bar
.bare:
        cmp byte [any], 0
        je .irq
        call crlf
.irq:
        ; ---- IRQ ----
        mov al, 0x3C
        call pci_rd
        mov ebx, eax
        test bh, bh
        jz .caps                    ; no usa interrupción
        PR SANGRIA, "IRQ "
        cmp bl, 0xFF
        jne .i1
        PR "sin asignar (FFh)"
        jmp .i2
.i1:
        movzx eax, bl
        call putdec
.i2:
        PR ", patilla "
        mov al, bh
        add al, 'A' - 1
        cmp bh, 4
        jbe .i3
        mov al, '?'
.i3:
        call putc
        call crlf
.caps:
        ; ---- capacidades ----
        test byte [p_cmd+2], 0x10
        jz .ncap
        mov al, 0x34
        cmp byte [p_hdr], 2
        jne .c1
        mov al, 0x14
.c1:
        call pci_rd
        and al, 0xFC
        mov [m_ecp], al
        cmp al, 0x40
        jb .ncap
        PR SANGRIA, "Capacidades:"
        mov byte [m_n], 24
.cl:
        mov al, [m_ecp]
        cmp al, 0x40
        jb .ce
        dec byte [m_n]
        js .ce
        call pci_rd
        mov ebx, eax
        mov al, ' '
        call putc
        mov si, cap_names
.cn:
        lodsb
        cmp al, 0xFF
        je .chex
        cmp al, bl
        je .cnf
.cs:
        lodsb
        test al, al
        jnz .cs
        jmp .cn
.cnf:
        call puts
        jmp .cnx
.chex:
        mov al, bl
        call hex2
        mov al, 'h'
        call putc
.cnx:
        mov al, bh
        and al, 0xFC
        mov [m_ecp], al
        jmp .cl
.ce:
        call crlf
.ncap:
        call pci_dump
        ; ---- lo propio de cada clase ----
        mov eax, [p_class]
        shr eax, 8
        cmp eax, 0x0C0300
        je usb_uhci
        cmp eax, 0x0C0310
        je usb_ohci
        cmp eax, 0x0C0320
        je usb_ehci
        cmp eax, 0x0C0330
        je usb_xhci
        cmp eax, 0x040300
        je snd_hda
        cmp eax, 0x040100
        je snd_ac97
        shr eax, 8
        cmp ax, 0x0101
        je sto_ide
        ret

cap_names   db 0x01, "energía", 0, 0x02, "AGP", 0, 0x05, "MSI", 0, 0x07, "PCI-X", 0
            db 0x09, "fabricante", 0, 0x0A, "depuración", 0, 0x0D, "puente", 0
            db 0x10, "PCI Express", 0, 0x11, "MSI-X", 0, 0x12, "SATA", 0
            db 0x13, "AF", 0, 0xFF

; el espacio de configuración de la función, a un archivo (/INFORME)
pci_dump:
        cmp byte [opt_inf], 0
        je .r
        mov di, dbuf
        xor bx, bx
.l:
        mov al, bl
        call pci_rd
        stosd
        add bx, 4
        cmp bx, 256
        jb .l
        mov di, fname
        mov dword [di], "PCI\"
        add di, 4
        call bdf_name
        mov dword [di], ".BIN"
        mov byte [di+4], 0
        mov si, fname
        mov dx, dbuf
        mov cx, 256
        call dump_buf
        ; el extendido (4 KB) por MMCONFIG, si coincide con lo leído por CF8h
        mov eax, [mcfg_base]
        test eax, eax
        jz .r
        mov bl, [p_bdf+1]
        cmp bl, [mcfg_b0]
        jb .r
        cmp bl, [mcfg_b0+1]
        ja .r
        sub bl, [mcfg_b0]
        movzx ebx, bl
        shl ebx, 20
        add eax, ebx
        movzx ebx, byte [p_bdf]
        shl ebx, 12
        add eax, ebx
        mov [d_addr], eax
        mov esi, eax
        mov di, mtmp
        mov cx, 4
        call physcopy
        jc .bad
        mov eax, [mtmp]
        cmp eax, [p_id]
        jne .bad
        mov dword [d_len], 4096
        mov di, fname + 4
        call bdf_name
        mov dword [di], ".EXT"
        mov byte [di+4], 0
        mov si, fname
        call dump_phys
        ret
.bad:
        PR SANGRIA, "(MMCONFIG no coincide con CF8h/CFCh en esta función: no se guarda el extendido)", NL
.r:
        ret
; DI -> "BB_DD_F" de la función p_bdf (DI queda detrás)
bdf_name:
        mov al, [p_bdf+1]
        call .h2
        mov al, '_'
        stosb
        mov al, [p_bdf]
        shr al, 3
        call .h2
        mov al, '_'
        stosb
        mov al, [p_bdf]
        and al, 7
        add al, '0'
        stosb
        ret
.h2:
        push ax
        shr al, 4
        call .h1
        pop ax
.h1:
        and al, 15
        cmp al, 10
        sbb al, 0x69
        das
        stosb
        ret
; EAX = extensión (".BIN") -> fname = USB\BB_DD_F.ext, SI = fname
usb_name:
        push di
        mov di, fname
        mov dword [di], "USB\"
        add di, 4
        push eax
        call bdf_name
        pop eax
        mov [di], eax
        mov byte [di+4], 0
        mov si, fname
        pop di
        ret

; los puertos raíz de un controlador USB con registros en memoria:
; u_off = desplazamiento del primero, u_str = separación, u_np = cuántos,
; u_kind = 0 xHCI, 1 EHCI, 2 OHCI
usb_ports:
        mov byte [u_i], 0
.l:
        mov al, [u_i]
        cmp al, [u_np]
        jae .r
        movzx eax, al
        movzx ecx, byte [u_str]
        mul ecx
        add eax, [u_off]
        add eax, [m_base]
        mov esi, eax
        mov cx, 4
        call physcopy_m
        jc .r
        PR SANGRIA, "puerto "
        movzx eax, byte [u_i]
        inc eax
        call putdec
        PR ": "
        mov eax, [mbuf]
        call hex8
        test al, 1
        jnz .con
        PR " vacío", NL
        jmp .n
.con:
        PR " conectado"
        cmp byte [u_kind], 1
        je .eh
        ja .oh
        ; xHCI: velocidad (bits 13-10) y habilitado (bit 1)
        mov ebx, eax
        shr ebx, 10
        and bx, 15
        mov si, sp_x
        cmp bl, 4
        ja .xs
        shl bx, 1
        mov si, [sp_tab+bx]
.xs:
        call puts
        test al, 2
        jmp .en
.eh:
        test ah, 0x20
        jz .eh2
        PR ", cedido al controlador USB 1.1"
.eh2:
        test al, 4
        jmp .en
.oh:
        test ah, 2
        jz .oh2
        PR ", velocidad baja"
.oh2:
        test al, 2
.en:
        jz .ne
        PR ", habilitado"
.ne:
        call crlf
.n:
        inc byte [u_i]
        jmp .l
.r:
        ret
sp_tab      dw sp_x, sp_1, sp_2, sp_3, sp_4
sp_x        db 0
sp_1        db ", velocidad plena", 0
sp_2        db ", velocidad baja", 0
sp_3        db ", alta velocidad", 0
sp_4        db ", SuperSpeed", 0
; los registros, a USB\BB_DD_F.BIN: desde la base, EAX octetos (/INFORME)
usb_dump:
        cmp byte [opt_inf], 0
        je .r
        cmp eax, 0x1000
        jbe .c
        mov eax, 0x1000
.c:
        mov [d_len], eax
        mov eax, [m_base]
        mov [d_addr], eax
        mov eax, ".BIN"
        call usb_name
        call dump_phys
.r:
        ret

; ---- IDE: cómo están los canales ----
sto_ide:
        PR SANGRIA, "IDE: canal primario "
        mov al, [p_class+1]
        test al, 1
        call .modo
        PR ", secundario "
        test al, 4
        call .modo
        test al, 0x80
        jz .x
        PR ", con bus master (DMA)"
.x:
        jmp crlf
.modo:
        jnz .nat
        PR "compatible"
        ret
.nat:
        PR "nativo PCI"
        ret

; ---- registros en memoria del dispositivo ----
; -> EAX = base del BAR0 (memoria); CF si no se pueden leer (ya avisado)
mmio_bar0:
        test byte [p_cmd], 2
        jnz .en
        PR SANGRIA, "(memoria desactivada en el registro de orden: no se leen sus registros)", NL
        stc
        ret
.en:
        mov al, 0x10
        call pci_rd
        test al, 1
        jnz .bad
        mov ebx, eax
        and al, 6
        cmp al, 4
        jne .lo
        mov al, 0x14
        call pci_rd
        test eax, eax
        jz .lo
        PR SANGRIA, "(registros por encima de 4 GB: no se leen)", NL
        stc
        ret
.lo:
        mov eax, ebx
        and al, 0xF0
        test eax, eax
        jz .bad
        mov [m_base], eax
        clc
        ret
.bad:
        PR SANGRIA, "(BAR0 sin asignar: no se leen sus registros)", NL
        stc
        ret

; ESI = dirección física, CX = octetos -> mbuf; CF si no (ya avisado, también
; si el dispositivo no responde: todo unos)
mrd_blk:
        push di
        mov di, mbuf
        call physcopy
        pop di
        jc .err
        cmp dword [mbuf], 0xFFFFFFFF
        jne .ok
        PR SANGRIA, "(el dispositivo no responde: sus registros leen FFFFFFFFh)", NL
        stc
        ret
.ok:
        clc
        ret
.err:
        PR SANGRIA, "(no se ha podido leer la memoria del dispositivo)", NL
        stc
        ret

; EAX = USBLEGSUP -> de quién es el controlador
own_text:
        test eax, 0x01000000
        jnz .os
        test eax, 0x00010000
        jnz .bios
        PR "sin dueño declarado (la BIOS no lo usa)"
        ret
.bios:
        PR "es de la BIOS"
        ret
.os:
        test eax, 0x00010000
        jnz .both
        PR "es del sistema operativo (la BIOS lo ha soltado)"
        ret
.both:
        PR "en traspaso (BIOS y sistema a la vez)"
        ret

; ---- USB xHCI ----
usb_xhci:
        call mmio_bar0
        jc .r
        mov esi, eax
        mov cx, 0x20
        call mrd_blk
        jc .r
        PR SANGRIA, "xHCI versión "
        mov ax, [mbuf+2]
        call ver_bcd
        PR ", "
        movzx eax, byte [mbuf+7]
        call putdec
        PR " puertos, "
        movzx eax, byte [mbuf+4]
        call putdec
        PR " ranuras", NL
        mov al, [mbuf+7]
        cmp al, 32
        jbe .np
        mov al, 32
.np:
        mov [u_np], al
        movzx eax, byte [mbuf]
        mov [m_cap], eax
        mov eax, [mbuf+0x10]
        shr eax, 16
        shl eax, 2
        mov [m_ecp], eax
        mov [u_ecp0], eax
        mov esi, [m_base]
        add esi, [m_cap]
        mov cx, 8
        call physcopy_m
        jc .r
        PR SANGRIA, "USBCMD="
        mov eax, [mbuf]
        call hex8
        PR " USBSTS="
        mov eax, [mbuf+4]
        call hex8
        test byte [mbuf], 1
        call marcha
        mov byte [m_n], 32
        mov byte [any], 0
.ec:
        mov eax, [m_ecp]
        test eax, eax
        jz .ece
        dec byte [m_n]
        js .ece
        mov esi, [m_base]
        add esi, eax
        mov cx, 16
        call physcopy_m
        jc .r
        mov al, [mbuf]
        cmp al, 1
        je .leg
        cmp al, 2
        je .prot
.nx:
        movzx eax, byte [mbuf+1]
        test eax, eax
        jz .ece
        shl eax, 2
        add [m_ecp], eax
        jmp .ec
.leg:
        mov byte [any], 1
        PR SANGRIA, "USBLEGSUP="
        mov eax, [mbuf]
        call hex8
        PR " USBLEGCTLSTS="
        mov eax, [mbuf+4]
        call hex8
        PR ": "
        mov eax, [mbuf]
        call own_text
        test byte [mbuf+4], 1
        jz .l1
        PR "; SMI de USB activa"
.l1:
        call crlf
        jmp .nx
.prot:
        PR SANGRIA, "USB "
        movzx eax, byte [mbuf+3]
        call putdec
        mov al, '.'
        call putc
        mov al, [mbuf+2]
        call hex2
        PR ": puertos "
        movzx eax, byte [mbuf+8]
        call putdec
        mov al, '-'
        call putc
        movzx eax, byte [mbuf+8]
        movzx edx, byte [mbuf+9]
        add eax, edx
        dec eax
        call putdec
        call crlf
        jmp .nx
.ece:
        cmp byte [any], 0
        jne .int
        PR SANGRIA, "Sin USBLEGSUP: la BIOS no declara de quién es", NL
.int:
        mov eax, [m_cap]
        add eax, 0x400
        mov [u_off], eax
        mov byte [u_str], 16
        mov byte [u_kind], 0
        call usb_ports
        movzx eax, byte [u_np]
        shl eax, 4
        add eax, [u_off]
        call usb_dump
        cmp byte [opt_inf], 0
        je .int2
        mov eax, [u_ecp0]
        test eax, eax
        jz .int2
        add eax, [m_base]
        mov [d_addr], eax
        mov dword [d_len], 0x200
        mov eax, ".ECP"
        call usb_name
        call dump_phys
.int2:
        cmp word [p_id], 0x8086
        jne .r
        PR SANGRIA, "Intel: XUSB2PR="
        mov al, 0xD0
        call pci_rd
        call hex8
        PR " USB3_PSSEN="
        mov al, 0xD8
        call pci_rd
        call hex8
        PR " (puertos pasados del EHCI al xHCI)", NL
.r:
        ret

; como mrd_blk, sin la comprobación de «todo unos»
physcopy_m:
        push di
        mov di, mbuf
        call physcopy
        pop di
        jnc .r
        PR SANGRIA, "(no se ha podido leer la memoria del dispositivo)", NL
        stc
.r:
        ret

; ZF de «test ..., 1» -> « (en marcha)» o « (parado)» y fin de línea
marcha:
        jz .p
        PR " (en marcha)", NL
        ret
.p:
        PR " (parado)", NL
        ret

; AX = versión en BCD (0100h -> 1.00)
ver_bcd:
        push ax
        movzx eax, ah
        call putdec_x
        mov al, '.'
        call putc
        pop ax
        jmp hex2
; AL en BCD sin cero delante
putdec_x:
        cmp al, 0x10
        jae hex2
        jmp hex1

; ---- USB EHCI ----
usb_ehci:
        call mmio_bar0
        jc .r
        mov esi, eax
        mov cx, 12
        call mrd_blk
        jc .r
        PR SANGRIA, "EHCI versión "
        mov ax, [mbuf+2]
        call ver_bcd
        PR ", "
        movzx eax, byte [mbuf+4]
        and al, 15
        call putdec
        PR " puertos", NL
        mov al, [mbuf+4]
        and al, 15
        mov [u_np], al
        movzx eax, byte [mbuf]
        mov [m_cap], eax
        movzx eax, byte [mbuf+9]
        mov [m_ecp], eax
        mov esi, [m_base]
        add esi, [m_cap]
        mov cx, 8
        call physcopy_m
        jc .r
        PR SANGRIA, "USBCMD="
        mov eax, [mbuf]
        call hex8
        PR " USBSTS="
        mov eax, [mbuf+4]
        call hex8
        mov al, [mbuf]
        mov [tmpw], al
        mov esi, [m_base]
        add esi, [m_cap]
        add esi, 0x40
        mov cx, 4
        call physcopy_m
        jc .r
        PR " CONFIGFLAG="
        mov eax, [mbuf]
        call hex8
        test byte [tmpw], 1
        call marcha
        mov byte [m_n], 8
        mov byte [any], 0
.ec:
        mov eax, [m_ecp]
        cmp al, 0x40
        jb .ece
        dec byte [m_n]
        js .ece
        call pci_rd
        mov ebx, eax
        cmp bl, 1
        jne .nx
        mov byte [any], 1
        PR SANGRIA, "USBLEGSUP="
        call hex8
        PR " USBLEGCTLSTS="
        mov al, [m_ecp]
        add al, 4
        call pci_rd
        mov ecx, eax
        call hex8
        PR ": "
        mov eax, ebx
        call own_text
        test cl, 1
        jz .l1
        PR "; SMI de USB activa"
.l1:
        call crlf
.nx:
        movzx eax, bh
        mov [m_ecp], eax
        jmp .ec
.ece:
        cmp byte [any], 0
        jne .p
        PR SANGRIA, "Sin USBLEGSUP: la BIOS no declara de quién es", NL
.p:
        mov eax, [m_cap]
        add eax, 0x44
        mov [u_off], eax
        mov byte [u_str], 4
        mov byte [u_kind], 1
        call usb_ports
        movzx eax, byte [u_np]
        shl eax, 2
        add eax, [u_off]
        call usb_dump
.r:
        ret

; ---- USB OHCI ----
usb_ohci:
        call mmio_bar0
        jc .r
        mov esi, eax
        mov cx, 0x18
        call mrd_blk
        jc .r
        PR SANGRIA, "OHCI revisión "
        mov al, [mbuf]
        shr al, 4
        call hex1
        mov al, '.'
        call putc
        mov al, [mbuf]
        call hex1
        PR ": HcControl="
        mov eax, [mbuf+4]
        call hex8
        PR " HcCommandStatus="
        mov eax, [mbuf+8]
        call hex8
        call crlf
        PR SANGRIA
        test byte [mbuf+5], 1
        jz .noir
        PR "InterruptRouting=1: lo lleva la BIOS (por SMI)"
        jmp .st
.noir:
        PR "InterruptRouting=0: la BIOS no lo lleva por SMI"
.st:
        PR "; estado: "
        mov al, [mbuf+4]
        shr al, 6
        and al, 3
        mov si, oh_s0
        jz .sp
        mov si, oh_s1
        cmp al, 1
        je .sp
        mov si, oh_s2
        cmp al, 2
        je .sp
        mov si, oh_s3
.sp:
        call puts
        test byte [mbuf+8], 8
        jz .nocr
        PR "; OwnershipChangeRequest pendiente"
.nocr:
        call crlf
        mov al, [mbuf+1]
        mov [tmpw], al
        mov esi, [m_base]
        add esi, 0x48
        mov cx, 4
        call physcopy_m
        jc .r
        PR SANGRIA, "Puertos: "
        movzx eax, byte [mbuf]
        call putdec
        cmp al, 15
        jbe .np
        mov al, 15
.np:
        mov [u_np], al
        test byte [tmpw], 1         ; con registros de emulación (teclado/ratón)
        jz .nl
        mov esi, [m_base]
        add esi, 0x100
        mov cx, 4
        call physcopy_m
        jc .r
        PR "; HceControl="
        mov eax, [mbuf]
        call hex8
        test al, 1
        jz .nl
        PR " (la BIOS emula teclado/ratón PS/2)"
.nl:
        call crlf
        mov dword [u_off], 0x54
        mov byte [u_str], 4
        mov byte [u_kind], 2
        call usb_ports
        movzx eax, byte [u_np]
        shl eax, 2
        add eax, 0x54
        call usb_dump
.r:
        ret
oh_s0       db "en reinicio (nadie lo usa)", 0
oh_s1       db "reanudando", 0
oh_s2       db "en marcha", 0
oh_s3       db "suspendido", 0

; ---- USB UHCI ----
usb_uhci:
        mov al, 0xC0
        call pci_rd
        mov bx, ax
        PR SANGRIA, "UHCI LEGSUP="
        call hex4
        PR ": "
        test bx, 0x001F
        jz .noemu
        PR "la BIOS emula teclado/ratón PS/2 (SMI activas)"
        jmp .e2
.noemu:
        PR "sin emulación de la BIOS"
.e2:
        test bx, 0x2000
        jz .noirq
        PR "; IRQ PCI activa", NL
        jmp .io
.noirq:
        PR "; IRQ PCI desactivada", NL
.io:
        test byte [p_cmd], 1
        jnz .ioen
        PR SANGRIA, "(E/S desactivada en el registro de orden: no se leen sus puertos)", NL
        ret
.ioen:
        mov al, 0x20
        call pci_rd
        test al, 1
        jz .r
        and ax, 0xFFE0
        jz .r
        mov dx, ax
        mov [u_io], ax
        in ax, dx
        mov bx, ax
        PR SANGRIA, "USBCMD="
        call hex4
        PR " USBSTS="
        add dx, 2
        in ax, dx
        call hex4
        PR " PORTSC1="
        add dx, 0x0E
        in ax, dx
        call hex4
        PR " PORTSC2="
        add dx, 2
        in ax, dx
        call hex4
        test bl, 1
        call marcha
        PR SANGRIA, "puerto 1"
        mov dx, [u_io]
        add dx, 0x10
        in ax, dx
        call .pt
        PR "; puerto 2"
        add dx, 2
        in ax, dx
        call .pt
        call crlf
        ; los registros, a un archivo
        cmp byte [opt_inf], 0
        je .r
        mov dx, [u_io]
        mov di, dbuf
        mov cx, 4
.w:
        in ax, dx
        stosw
        add dx, 2
        loop .w
        in eax, dx
        stosd
        add dx, 4
        in al, dx
        xor ah, ah
        stosw
        xor ax, ax
        stosw
        add dx, 4
        in ax, dx
        stosw
        add dx, 2
        in ax, dx
        stosw
        mov eax, ".BIN"
        call usb_name
        mov dx, dbuf
        mov cx, 0x14
        call dump_buf
.r:
        ret
.pt:
        test al, 1
        jnz .pc
        PR " vacío"
        ret
.pc:
        PR " conectado"
        test ah, 1
        jz .pf
        PR " (velocidad baja)"
.pf:
        ret

; ---- HD Audio ----
; (cada registro se lee con su tamaño: hay controladores que lo exigen)
snd_hda:
        call mmio_bar0
        jc .r
        mov esi, [m_base]
        mov cl, 2
        call mreg                   ; GCAP
        jc .err
        mov [mbuf], ax
        cmp ax, 0xFFFF
        jne .ok
        PR SANGRIA, "(el dispositivo no responde: sus registros leen FFFFh)", NL
        ret
.ok:
        inc esi
        inc esi
        mov cl, 1
        call mreg                   ; VMIN
        jc .err
        mov [mbuf+2], al
        inc esi
        call mreg                   ; VMAJ
        jc .err
        mov [mbuf+3], al
        mov esi, [m_base]
        add esi, 8
        mov cl, 4
        call mreg                   ; GCTL
        jc .err
        mov [mbuf+8], eax
        mov esi, [m_base]
        add esi, 0x0E
        mov cl, 2
        call mreg                   ; STATESTS
        jc .err
        mov [mbuf+0x0E], ax
        PR SANGRIA, "HD Audio "
        movzx eax, byte [mbuf+3]
        call putdec
        mov al, '.'
        call putc
        movzx eax, byte [mbuf+2]
        call putdec
        PR ": GCAP="
        mov ax, [mbuf]
        call hex4
        PR " GCTL="
        mov eax, [mbuf+8]
        call hex8
        PR " STATESTS="
        mov ax, [mbuf+0x0E]
        call hex4
        test byte [mbuf+8], 1
        jnz .on
        PR " (controlador en reinicio)", NL
        jmp hda_codecs
.on:
        PR " (en marcha; códecs en:"
        mov bx, [mbuf+0x0E]
        xor cx, cx
.c:
        shr bx, 1
        jnc .cn
        mov al, ' '
        call putc
        movzx eax, cl
        call putdec
.cn:
        inc cx
        cmp cx, 15
        jb .c
        PR ")", NL
        jmp hda_codecs
.r:
        ret
.err:
        PR SANGRIA, "(no se ha podido leer la memoria del dispositivo)", NL
        ret

; ---- los códecs de un HD Audio: ID, revisión y subsistema ----
; Se les pregunta con las órdenes inmediatas del controlador (ICW/IRV/ICS),
; solo si nadie lo está usando: si lo lleva HSPSND no se toca (HSPSND /INFO da
; sus códecs). Si el controlador estaba en reinicio se le saca un momento y se
; le deja otra vez en reinicio, como estaba.
hda_codecs:
        smsw ax
        test al, 1
        jz .rm
        PR SANGRIA, "(en modo V86 no se pregunta a los códecs)", NL
        ret
.rm:
        call snd_check
        jc .free
        xor ah, ah
        call far [far_ent]
        call fixseg
        cmp al, 4
        jne .free
        PR SANGRIA, "Códecs: los lleva HSPSND; sus ID están en HSPSND /INFO", NL
        ret
.free:
        call fixseg
        call ckpt
        mov byte [h_wasrst], 0
        test byte [mbuf+8], 1
        jnz .run
        mov byte [h_wasrst], 1
        mov esi, [m_base]
        add esi, 8
        mov eax, 1
        mov cl, 4
        call mwreg
        mov cx, 3
        call t_ini
.w1:
        mov esi, [m_base]
        add esi, 8
        mov cl, 4
        call mreg
        test al, 1
        jnz .up
        call t_fuera
        jnc .w1
        PR SANGRIA, "(el controlador no sale del reinicio: no se pregunta a los códecs)", NL
        jmp .back
.up:
        mov cx, 2                   ; los códecs se anuncian en menos de 1 ms
        call t_ini
.w2:
        call t_fuera
        jnc .w2
        mov esi, [m_base]
        add esi, 0x0E
        mov cl, 2
        call mreg
        mov [mbuf+0x0E], ax
.run:
        mov bx, [mbuf+0x0E]
        test bx, 0x7FFF
        jnz .hay
        PR SANGRIA, "(ningún códec se ha anunciado en STATESTS)", NL
        jmp .back
.hay:
        mov byte [h_cad], 0
.cl:
        shr bx, 1
        jnc .cn
        push bx
        call hda_codec
        pop bx
.cn:
        inc byte [h_cad]
        cmp byte [h_cad], 15
        jb .cl
.back:
        cmp byte [h_wasrst], 0
        je .r
        mov esi, [m_base]
        add esi, 8
        xor eax, eax
        mov cl, 4
        call mwreg
.r:
        ret

hda_codec:
        PR SANGRIA, "códec "
        movzx eax, byte [h_cad]
        call putdec
        PR ": "
        xor bl, bl
        mov edx, 0xF0000            ; parámetro 00h: fabricante y dispositivo
        call hda_cmd
        jc .fail
        PR "ID "
        call hex8
        shr eax, 16
        call vendor_name
        jc .nv
        PR " ("
        call puts
        PR ")"
.nv:
        xor bl, bl
        mov edx, 0xF0002            ; revisión
        call hda_cmd
        jc .e1
        PR ", revisión "
        call hex8
.e1:
        xor bl, bl
        mov edx, 0xF0004            ; grupos de funciones: primero y cuántos
        call hda_cmd
        jc .end
        mov ecx, eax
        shr ecx, 16
        mov ch, al
        cmp ch, 8
        jbe .g
        mov ch, 8
.g:
        test ch, ch
        jz .end
        mov bl, cl
        mov edx, 0xF0005            ; tipo de grupo
        push cx
        call hda_cmd
        pop cx
        jc .end
        push eax
        PR "; grupo "
        movzx eax, cl
        call putdec
        pop eax
        and al, 0x7F
        cmp al, 1
        jne .g2
        PR " de audio"
        jmp .g4
.g2:
        cmp al, 2
        jne .g3
        PR " de módem"
        jmp .g4
.g3:
        PR " (otro)"
.g4:
        mov bl, cl
        mov edx, 0xF2000            ; subsistema
        push cx
        call hda_cmd
        pop cx
        jc .gn
        PR ", subsistema "
        call hex8
.gn:
        inc cl
        dec ch
        jmp .g
.end:
        jmp crlf
.fail:
        PR "no contesta a las órdenes inmediatas (ICW/IRV)", NL
        ret

; BL = nodo, EDX = verbo y dato (20 bits), códec h_cad -> EAX; CF si no contesta
hda_cmd:
        push ecx
        push esi
        push edx
        mov cx, 2
        call t_ini
.w0:
        mov esi, [m_base]
        add esi, 0x68
        mov cl, 2
        call mreg
        test al, 1
        jz .go
        call t_fuera
        jnc .w0
        jmp .f
.go:
        mov eax, 2                  ; borrar «respuesta válida»
        mov cl, 2
        call mwreg
        movzx eax, byte [h_cad]
        shl eax, 28
        movzx ecx, bl
        shl ecx, 20
        or eax, ecx
        or eax, edx
        mov esi, [m_base]
        add esi, 0x60
        mov cl, 4
        call mwreg
        add esi, 8
        mov eax, 1
        mov cl, 2
        call mwreg
        mov cx, 2
        call t_ini
.w1:
        mov cl, 2
        call mreg
        and al, 3
        cmp al, 2
        je .ok
        call t_fuera
        jnc .w1
        jmp .f
.ok:
        mov esi, [m_base]
        add esi, 0x64
        mov cl, 4
        call mreg
        clc
        jmp .x
.f:
        stc
.x:
        pop edx
        pop esi
        pop ecx
        ret

; ESI = dirección de un registro, CL = tamaño (2 o 4), EAX = valor: se escribe
; (solo en modo real)
mwreg:
        call a20_open
        call unreal
        cmp cl, 2
        je .w
        mov [fs:esi], eax
        jmp .e
.w:
        mov [fs:esi], ax
.e:
        call a20_restore
        ret

; ESI = dirección física de un registro, CL = su tamaño (1, 2 o 4) -> EAX;
; CF si no se puede leer
mreg:
        smsw ax
        test al, 1
        jnz .bios
        call a20_open
        call unreal
        xor eax, eax
        cmp cl, 2
        je .w
        ja .d
        mov al, [fs:esi]
        jmp .e
.w:
        mov ax, [fs:esi]
        jmp .e
.d:
        mov eax, [fs:esi]
.e:
        call a20_restore
        clc
        ret
.bios:
        push esi
        push di
        push cx
        push cx
        mov cx, si
        and cl, 3
        shl cl, 3
        mov ch, cl                  ; desplazamiento dentro de los 4 octetos
        and esi, 0xFFFFFFFC
        mov di, mtmp
        push cx
        mov cx, 4
        call physcopy
        pop cx
        mov eax, [mtmp]
        mov cl, ch
        pop dx                      ; DL = tamaño
        jc .x
        shr eax, cl
        cmp dl, 2
        ja .x0
        je .x2
        and eax, 0xFF
        jmp .x0
.x2:
        and eax, 0xFFFF
.x0:
        clc
.x:
        pop cx
        pop di
        pop esi
        ret

; ---- AC'97 ----
snd_ac97:
        test byte [p_cmd], 1
        jz .r
        mov al, 0x14
        call pci_rd
        test al, 1
        jz .r
        and ax, 0xFFFC
        jz .r
        mov dx, ax
        add dx, 0x30
        in eax, dx
        PR SANGRIA, "AC'97: estado global (NABM+30h)="
        call hex8
        test ah, 1
        jz .nc
        PR " (códec principal listo)"
.nc:
        call crlf
.r:
        ret

; EAX = clase (clase:subclase:interfaz en los 24 bits bajos) -> SI = nombre
class_name:
        push edx
        mov si, class_tab
.l:
        cmp byte [si], 0xFE
        je .hit
        mov edx, eax
        shr edx, 16
        cmp dl, [si]
        jne .no
        mov dl, [si+1]
        cmp dl, 0xFF
        je .s
        cmp dl, ah
        jne .no
.s:
        mov dl, [si+2]
        cmp dl, 0xFF
        je .hit
        cmp dl, al
        je .hit
.no:
        add si, 3
.sk:
        inc si
        cmp byte [si-1], 0
        jne .sk
        jmp .l
.hit:
        add si, 3
        pop edx
        ret

; clase, subclase, interfaz (FFh = cualquiera), nombre: de lo concreto a lo general
class_tab:
        db 0x00, 0x01, 0xFF, "Vídeo VGA (anterior a las clases)", 0
        db 0x00, 0xFF, 0xFF, "Dispositivo anterior a las clases", 0
        db 0x01, 0x00, 0xFF, "SCSI", 0
        db 0x01, 0x01, 0xFF, "IDE", 0
        db 0x01, 0x02, 0xFF, "Controlador de disquetes", 0
        db 0x01, 0x04, 0xFF, "RAID", 0
        db 0x01, 0x05, 0xFF, "ATA", 0
        db 0x01, 0x06, 0x01, "SATA AHCI", 0
        db 0x01, 0x06, 0xFF, "SATA", 0
        db 0x01, 0x07, 0xFF, "SAS", 0
        db 0x01, 0x08, 0x02, "NVMe", 0
        db 0x01, 0x08, 0xFF, "Memoria no volátil", 0
        db 0x01, 0xFF, 0xFF, "Almacenamiento (otro)", 0
        db 0x02, 0x00, 0xFF, "Red Ethernet", 0
        db 0x02, 0x80, 0xFF, "Red (otra: wifi...)", 0
        db 0x02, 0xFF, 0xFF, "Red", 0
        db 0x03, 0x00, 0x00, "Vídeo VGA", 0
        db 0x03, 0x00, 0xFF, "Vídeo VGA (8514)", 0
        db 0x03, 0x01, 0xFF, "Vídeo XGA", 0
        db 0x03, 0x02, 0xFF, "Vídeo 3D", 0
        db 0x03, 0xFF, 0xFF, "Vídeo (otro)", 0
        db 0x04, 0x00, 0xFF, "Vídeo multimedia", 0
        db 0x04, 0x01, 0xFF, "Audio AC'97 (audio multimedia)", 0
        db 0x04, 0x02, 0xFF, "Telefonía", 0
        db 0x04, 0x03, 0xFF, "HD Audio", 0
        db 0x04, 0xFF, 0xFF, "Multimedia (otro)", 0
        db 0x05, 0xFF, 0xFF, "Memoria", 0
        db 0x06, 0x00, 0xFF, "Puente principal (host)", 0
        db 0x06, 0x01, 0xFF, "Puente ISA", 0
        db 0x06, 0x02, 0xFF, "Puente EISA", 0
        db 0x06, 0x03, 0xFF, "Puente MCA", 0
        db 0x06, 0x04, 0xFF, "Puente PCI-PCI", 0
        db 0x06, 0x05, 0xFF, "Puente PCMCIA", 0
        db 0x06, 0x07, 0xFF, "Puente CardBus", 0
        db 0x06, 0xFF, 0xFF, "Puente (otro)", 0
        db 0x07, 0x00, 0xFF, "Puerto serie", 0
        db 0x07, 0x01, 0xFF, "Puerto paralelo", 0
        db 0x07, 0x03, 0xFF, "Módem", 0
        db 0x07, 0xFF, 0xFF, "Comunicaciones (otro)", 0
        db 0x08, 0x00, 0xFF, "Controlador de interrupciones", 0
        db 0x08, 0x01, 0xFF, "Controlador de DMA", 0
        db 0x08, 0x02, 0xFF, "Temporizador", 0
        db 0x08, 0x03, 0xFF, "Reloj (RTC)", 0
        db 0x08, 0x05, 0xFF, "Lector de tarjetas SD", 0
        db 0x08, 0xFF, 0xFF, "Sistema (otro)", 0
        db 0x09, 0xFF, 0xFF, "Dispositivo de entrada", 0
        db 0x0A, 0xFF, 0xFF, "Base de acoplamiento", 0
        db 0x0B, 0xFF, 0xFF, "Procesador", 0
        db 0x0C, 0x00, 0xFF, "FireWire (IEEE 1394)", 0
        db 0x0C, 0x03, 0x00, "USB UHCI", 0
        db 0x0C, 0x03, 0x10, "USB OHCI", 0
        db 0x0C, 0x03, 0x20, "USB EHCI", 0
        db 0x0C, 0x03, 0x30, "USB xHCI", 0
        db 0x0C, 0x03, 0xFE, "USB (dispositivo, no controlador)", 0
        db 0x0C, 0x03, 0xFF, "USB (otro)", 0
        db 0x0C, 0x05, 0xFF, "SMBus", 0
        db 0x0C, 0xFF, 0xFF, "Bus serie (otro)", 0
        db 0x0D, 0x00, 0xFF, "Infrarrojos (IrDA)", 0
        db 0x0D, 0x11, 0xFF, "Bluetooth", 0
        db 0x0D, 0xFF, 0xFF, "Inalámbrico", 0
        db 0x0E, 0xFF, 0xFF, "E/S inteligente (I2O)", 0
        db 0x0F, 0xFF, 0xFF, "Satélite", 0
        db 0x10, 0xFF, 0xFF, "Cifrado", 0
        db 0x11, 0xFF, 0xFF, "Adquisición de señales", 0
        db 0xFE, 0, 0, "Clase desconocida", 0

; AX = fabricante -> SI = nombre; CF si no se conoce
vendor_name:
        mov si, vendors
.l:
        cmp word [si], 0
        je .no
        cmp ax, [si]
        je .f
        add si, 2
.s:
        inc si
        cmp byte [si-1], 0
        jne .s
        jmp .l
.f:
        add si, 2
        clc
        ret
.no:
        stc
        ret

%macro VEN 2
        dw %1
        db %2, 0
%endmacro
vendors:
        VEN 0x8086, "Intel"
        VEN 0x1022, "AMD"
        VEN 0x1002, "ATI"
        VEN 0x10DE, "NVIDIA"
        VEN 0x1106, "VIA"
        VEN 0x1039, "SiS"
        VEN 0x10B9, "ALi"
        VEN 0x10EC, "Realtek"
        VEN 0x14E4, "Broadcom"
        VEN 0x168C, "Atheros"
        VEN 0x1274, "Ensoniq"
        VEN 0x1102, "Creative"
        VEN 0x125D, "ESS"
        VEN 0x1013, "Cirrus Logic"
        VEN 0x5333, "S3"
        VEN 0x102B, "Matrox"
        VEN 0x104C, "Texas Instruments"
        VEN 0x1180, "Ricoh"
        VEN 0x1217, "O2 Micro"
        VEN 0x10C8, "NeoMagic"
        VEN 0x1023, "Trident"
        VEN 0x100B, "National Semiconductor"
        VEN 0x1073, "Yamaha"
        VEN 0x11C1, "Agere/Lucent"
        VEN 0x14F1, "Conexant"
        VEN 0x106B, "Apple"
        VEN 0x1234, "QEMU/Bochs"
        VEN 0x1AF4, "Red Hat (virtio)"
        VEN 0x1B36, "Red Hat (QEMU)"
        VEN 0x15AD, "VMware"
        VEN 0x80EE, "VirtualBox"
        VEN 0x1033, "NEC"
        VEN 0x1179, "Toshiba"
        VEN 0x10CF, "Fujitsu"
        VEN 0x197B, "JMicron"
        VEN 0x1B21, "ASMedia"
        VEN 0x1912, "Renesas"
        VEN 0x8384, "SigmaTel"
        VEN 0x1095, "Silicon Image"
        VEN 0x10B7, "3Com"
        VEN 0x1011, "DEC"
        VEN 0x121A, "3dfx"
        VEN 0x1014, "IBM"
        VEN 0x0E11, "Compaq"
        VEN 0x1028, "Dell"
        VEN 0x103C, "HP"
        VEN 0x17AA, "Lenovo"
        VEN 0x1043, "ASUS"
        VEN 0x1025, "Acer"
        VEN 0x144D, "Samsung"
        VEN 0x104D, "Sony"
        VEN 0x11AB, "Marvell"
        VEN 0x1814, "Ralink"
        VEN 0x1524, "ENE"
        VEN 0x10F7, "Panasonic"
        VEN 0x1045, "OPTi"
        VEN 0x1000, "LSI/Symbios"
        VEN 0x9004, "Adaptec"
        VEN 0x1050, "Winbond"
        VEN 0x1078, "Cyrix"
        VEN 0x1283, "ITE"
        VEN 0x13F6, "C-Media"
        VEN 0x1412, "VIA/ICEnsemble"
        VEN 0x126F, "Silicon Motion"
        VEN 0x1AE0, "Google"
        VEN 0x11D4, "Analog Devices"
        VEN 0x111D, "IDT"
        VEN 0x1057, "Motorola"
        VEN 0x1AEC, "Wolfson"
        dw 0

; =============================================================================
;  DISQUETERAS Y DISCOS
; =============================================================================
sec_discos:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        push dx
        PR "Unidad de arranque de DOS: "
        pop ax
        mov al, dl
        call putc
        PR ":   unidad actual: "
        mov ah, 0x19
        int 0x21
        add al, 'A'
        call putc
        PR ":", NL
        ; ---- disqueteras ----
        int 0x11
        push ax
        PR "Palabra de equipo (INT 11h): "
        pop ax
        call hex4
        PR "h", NL
        mov al, 0x10
        call cmos_rd
        mov bl, al
        PR "Disqueteras según la CMOS (10h): A: "
        mov al, bl
        shr al, 4
        call floppy_type
        PR ", B: "
        mov al, bl
        call floppy_type
        call crlf
        call ckpt
        xor di, di
        mov es, di
        mov ah, 8
        xor dl, dl
        xor bl, bl
        stc
        int 0x13
        push cs
        pop es
        push cs
        pop ds
        cld
        jc .nofl
        push bx
        push dx
        PR "Disqueteras según la BIOS (INT 13h 08h): "
        pop dx
        movzx eax, dl
        call putdec
        PR "; la primera: "
        pop bx
        mov al, bl
        call floppy_type
        call crlf
        jmp .hd
.nofl:
        PR "Disqueteras según la BIOS (INT 13h 08h): no informa", NL
.hd:
        ; ---- discos duros ----
        xor ax, ax
        mov es, ax
        mov al, [es:0x475]
        mov [cnt], al
        push cs
        pop es
        PR "Discos según la BIOS (40:75): "
        movzx eax, byte [cnt]
        call putdec
        call crlf
        mov al, [cnt]
        cmp al, 8
        jbe .n8
        mov al, 8
.n8:
        mov [cnt+1], al
        mov byte [dnum], 0x80
.d:
        cmp byte [cnt+1], 0
        je .dend
        dec byte [cnt+1]
        call ckpt
        PR "Disco "
        mov al, [dnum]
        call hex2
        PR "h: "
        xor di, di
        mov es, di
        mov ah, 8
        mov dl, [dnum]
        stc
        int 0x13
        push cs
        pop es
        push cs
        pop ds
        cld
        jc .nochs
        test ah, ah
        jnz .nochs
        push dx
        push cx
        PR "CHS "
        pop cx
        push cx
        mov al, ch
        mov ah, cl
        shr ah, 6
        movzx eax, ax
        inc eax
        call putdec
        mov al, '/'
        call putc
        pop cx
        pop dx
        movzx eax, dh
        inc eax
        call putdec
        mov al, '/'
        call putc
        movzx eax, cl
        and al, 0x3F
        call putdec
        jmp .edd
.nochs:
        PR "no responde a INT 13h 08h"
.edd:
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [dnum]
        stc
        int 0x13
        push cs
        pop es
        push cs
        pop ds
        cld
        jc .noedd
        cmp bx, 0xAA55
        jne .noedd
        push cx
        push ax
        PR "; EDD "
        pop ax
        mov al, ah
        shr al, 4
        call hex1
        mov al, '.'
        call putc
        mov al, ah
        call hex1
        PR " (funciones "
        pop ax
        call hex4
        PR "h)"
        ; parámetros ampliados
        mov di, eddbuf
        mov cx, 0x50
        xor al, al
        rep stosb
        mov word [eddbuf], 0x42
        mov ah, 0x48
        mov dl, [dnum]
        mov si, eddbuf
        stc
        int 0x13
        push cs
        pop es
        push cs
        pop ds
        cld
        jc .x
        ; (los parámetros, también a EDDnn.BIN)
        mov di, fname
        mov dword [di], "EDD0"
        mov al, [dnum]
        shr al, 4
        add al, '0'
        mov [di+3], al
        mov al, [dnum]
        and al, 15
        add al, '0'
        mov [di+4], al
        mov dword [di+5], ".BIN"
        mov byte [di+9], 0
        mov si, fname
        mov dx, eddbuf
        mov cx, 0x4A
        call dump_buf
        PR "; "
        mov eax, [eddbuf+0x14]
        test eax, eax
        jz .lo
        call hex8
        mov eax, [eddbuf+0x10]
        call hex8
        PR "h"
        jmp .sec
.lo:
        mov eax, [eddbuf+0x10]
        call putdec
.sec:
        PR " sectores de "
        movzx eax, word [eddbuf+0x18]
        call putdec
        cmp word [eddbuf+0x18], 512
        jne .bus
        PR " ("
        mov eax, [eddbuf+0x10]
        mov edx, [eddbuf+0x14]
        shrd eax, edx, 11
        call putdec
        PR " MB)"
.bus:
        cmp word [eddbuf], 0x42
        jb .x
        cmp word [eddbuf+0x1E], 0xBEDD
        jne .x
        PR "; bus "
        mov si, eddbuf + 0x24
        mov cx, 4
        call puts_fix
        PR ", interfaz "
        mov si, eddbuf + 0x28
        mov cx, 8
        call puts_fix
        jmp .x
.noedd:
        PR "; sin EDD"
.x:
        call crlf
        inc byte [dnum]
        jmp .d
.dend:
        call fixseg
        PR "Identificación ATA (puertos IDE clásicos 1F0h y 170h; solo se envía la", NL
        PR "orden de identificarse):", NL
        mov word [at_base], 0x1F0
        call ata_chan
        mov word [at_base], 0x170
        call ata_chan
        ret

ata_chan:
        call ckpt
        mov dx, [at_base]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        jne .c1
        PR "  "
        mov ax, [at_base]
        call hex4
        PR "h: no hay canal (lee FFh)", NL
        ret
.c1:
        mov byte [at_drv], 0
        call ata_id
        mov byte [at_drv], 1
        call ata_id
        ret

; espera a que el disco no esté ocupado (DX = base+7 al salir, AL = estado);
; CF si se pasa el tiempo (1 s)
ata_wait:
        mov cx, 20
        call t_ini
        mov dx, [at_base]
        add dx, 7
.l:
        in al, dx
        test al, 0x80
        jz .ok
        call t_fuera
        jnc .l
        ret
.ok:
        clc
        ret

ata_id:
        PR "  "
        mov ax, [at_base]
        call hex4
        cmp byte [at_drv], 0
        jne .es
        PR "h maestro: "
        jmp .go
.es:
        PR "h esclavo: "
.go:
        mov dx, [at_base]
        add dx, 6
        mov al, [at_drv]
        shl al, 4
        or al, 0xA0
        out dx, al
        call pausa
        inc dx
        in al, dx
        test al, al
        jz .none
        cmp al, 0xFF
        je .none
        call ata_wait
        jc .tmo
        mov dx, [at_base]
        add dx, 0x206
        mov al, 2                   ; sin interrupción mientras se pregunta
        out dx, al
        mov byte [at_atapi], 0
        mov al, 0xEC                ; IDENTIFY DEVICE
.cmd:
        mov dx, [at_base]
        add dx, 7
        out dx, al
        call pausa
        in al, dx
        test al, al
        jz .none2
        call ata_wait
        jc .tmo2
        test al, 1
        jnz .err
        ; esperar a los datos
        mov cx, 10
        call t_ini
.dq:
        in al, dx
        test al, 8
        jnz .rd
        test al, 1
        jnz .err
        call t_fuera
        jnc .dq
.err:
        ; ¿es un ATAPI (CD/DVD)? Lo dice su firma
        cmp byte [at_atapi], 0
        jne .none2
        mov dx, [at_base]
        add dx, 4
        in al, dx
        mov ah, al
        inc dx
        in al, dx
        cmp ax, 0x14EB
        jne .none2
        mov byte [at_atapi], 1
        mov al, 0xA1                ; IDENTIFY PACKET DEVICE
        jmp .cmd
.rd:
        mov dx, [at_base]
        mov di, idbuf
        mov cx, 256
        rep insw
        add dx, 7
        in al, dx
        call .ctl
        ; modelo, firmware y tamaño
        mov si, idbuf + 54
        mov cx, 20
        call ata_str
        PR ", firmware "
        mov si, idbuf + 46
        mov cx, 4
        call ata_str
        cmp byte [at_atapi], 0
        je .hd
        PR ", ATAPI (CD/DVD)"
        jmp .dump
.hd:
        PR ", "
        test byte [idbuf+167], 4    ; LBA de 48 bits
        jz .l28
        mov eax, [idbuf+200]
        mov edx, [idbuf+204]
        jmp .sz
.l28:
        mov eax, [idbuf+120]
        xor edx, edx
.sz:
        shrd eax, edx, 11
        call putdec
        PR " MB"
.dump:
        call crlf
        mov di, fname
        mov dword [di], "ATA1"
        mov word [di+4], "F0"
        cmp word [at_base], 0x1F0
        je .f1
        mov word [di+4], "70"
.f1:
        mov byte [di+6], '_'
        mov al, [at_drv]
        add al, '0'
        mov [di+7], al
        mov dword [di+8], ".BIN"
        mov byte [di+12], 0
        mov si, fname
        mov dx, idbuf
        mov cx, 512
        jmp dump_buf
.none2:
        call .ctl
.none:
        PR "no hay", NL
        ret
.tmo2:
        call .ctl
.tmo:
        PR "no contesta (ocupado)", NL
        ret
.ctl:
        push ax
        push dx
        mov dx, [at_base]
        add dx, 0x206
        xor al, al                  ; interrupciones como estaban
        out dx, al
        pop dx
        pop ax
        ret

; SI -> texto de la identificación ATA (CX palabras, octetos cambiados)
ata_str:
        push di
        mov di, strbuf
        push cx
.l:
        lodsw
        xchg al, ah
        stosw
        loop .l
        pop cx
        shl cx, 1
        mov si, strbuf
        call puts_fix
        pop di
        ret

; AL (4 bits bajos) = tipo de disquetera de la CMOS
floppy_type:
        push ax
        push bx
        and ax, 15
        cmp al, 6
        jbe .ok
        mov al, 7
.ok:
        mov bx, ax
        shl bx, 1
        mov si, [fl_tab+bx]
        call puts
        pop bx
        pop ax
        ret
fl_tab      dw fl_0, fl_1, fl_2, fl_3, fl_4, fl_5, fl_5, fl_x
fl_0        db "no hay", 0
fl_1        db '360 KB (5,25")', 0
fl_2        db '1,2 MB (5,25")', 0
fl_3        db '720 KB (3,5")', 0
fl_4        db '1,44 MB (3,5")', 0
fl_5        db '2,88 MB (3,5")', 0
fl_x        db "tipo desconocido", 0

; =============================================================================
;  RESIDENTES Y SISTEMA
; =============================================================================
sec_res:
        mov ah, 0x30
        int 0x21
        push ax
        PR "DOS: versión "
        pop ax
        push ax
        movzx eax, al
        call putdec
        mov al, '.'
        call putc
        pop ax
        mov al, ah
        call put2d
        mov ax, 0x3306
        xor bx, bx
        xor dx, dx
        int 0x21
        test dh, 0x10
        jz .nohma
        PR ", núcleo en la memoria alta (HMA)"
.nohma:
        call crlf
        call fixseg
        ; ---- cadena de controladores de dispositivo ----
        PR "Controladores de dispositivo: "
        mov byte [usb_res], 0
        mov ah, 0x52
        int 0x21
        add bx, 0x22                ; el dispositivo NUL
        mov cx, 64
.dv:
        test word [es:bx+4], 0x8000
        jz .blk
        lea si, [bx+10]
        push cx
        mov cx, 8
        call puts_fix
        pop cx
        jmp .dn
.blk:
        mov al, '['
        call putc
        movzx eax, byte [es:bx+10]
        call putdec
        PR " unid."
        ; (HSPUSB deja su nombre detrás del número de unidades)
        cmp dword [es:bx+11], 0x55505348    ; "HSPU"
        jne .b1
        cmp word [es:bx+15], 0x4253         ; "SB"
        jne .b1
        mov byte [usb_res], 1
        PR " HSPUSB"
.b1:
        mov al, ']'
        call putc
.dn:
        mov al, ' '
        call putc
        les bx, [es:bx]
        cmp bx, 0xFFFF
        je .dve
        loop .dv
.dve:
        call crlf
        call fixseg
        ; ---- XMS ----
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .noxms
        mov ax, 0x4310
        int 0x2F
        mov [far_ent], bx
        mov [far_ent+2], es
        call fixseg
        xor ah, ah
        call far [far_ent]
        push dx
        push bx
        push ax
        PR "XMS: versión "
        pop ax
        call ver_bcd
        PR ", revisión del gestor "
        pop ax
        call hex4
        PR "h, HMA "
        pop ax
        test ax, ax
        jz .xh0
        PR "sí"
        jmp .xh1
.xh0:
        PR "no"
.xh1:
        mov ah, 0x08
        xor bl, bl
        call far [far_ent]
        push dx
        push ax
        PR "; libre "
        pop ax
        pop dx
        movzx eax, dx
        call putdec
        PR " KB; línea A20 "
        mov ah, 0x07
        call far [far_ent]
        test ax, ax
        jz .xa0
        PR "abierta", NL
        jmp .ems
.xa0:
        PR "cerrada", NL
        jmp .ems
.noxms:
        PR "XMS: no hay gestor (HSPXMS no está cargado)", NL
.ems:
        call fixseg
        ; ---- EMS / VCPI (HSPEMM) ----
        xor ax, ax
        mov es, ax
        mov ax, [es:0x67*4+2]
        test ax, ax
        jz .noems
        mov es, ax
        cmp word [es:0x0A], 0x4D45          ; "EM"
        jne .noems
        cmp byte [es:0x0C], 'M'
        jne .noems
        cmp dword [es:0x0D], 0x58585858     ; "XXXX"
        jne .vcpi
        mov ah, 0x46
        int 0x67
        test ah, ah
        jnz .vcpi
        push ax
        PR "EMS: versión "
        pop ax
        push ax
        shr al, 4
        call hex1
        mov al, '.'
        call putc
        pop ax
        call hex1
        mov ah, 0x42
        int 0x67
        push dx
        PR ", páginas libres "
        movzx eax, bx
        call putdec
        PR " de "
        pop dx
        movzx eax, dx
        call putdec
        mov ah, 0x41
        int 0x67
        test ah, ah
        jnz .e1
        PR ", marco en "
        mov ax, bx
        call hex4
        mov al, 'h'
        call putc
.e1:
        call crlf
        jmp .v2
.vcpi:
        PR "EMS: gestor cargado sin EMS", NL
.v2:
        mov ax, 0xDE00
        int 0x67
        test ah, ah
        jnz .dpmi
        push bx
        PR "VCPI: versión "
        pop bx
        movzx eax, bh
        call putdec
        mov al, '.'
        call putc
        movzx eax, bl
        call putdec
        call crlf
        jmp .dpmi
.noems:
        PR "EMS/VCPI: no hay gestor (HSPEMM no está cargado)", NL
.dpmi:
        call fixseg
        mov ax, 0x1687
        int 0x2F
        push cs
        pop es
        test ax, ax
        jnz .nodpmi
        push bx
        push dx
        PR "DPMI: versión "
        pop dx
        push dx
        movzx eax, dh
        call putdec
        mov al, '.'
        call putc
        pop dx
        movzx eax, dl
        call putdec
        pop bx
        test bl, 1
        jz .d16
        PR " (32 bits)"
.d16:
        call crlf
        jmp .snd
.nodpmi:
        PR "DPMI: no hay (HSPDPMI no está cargado)", NL
.snd:
        call fixseg
        ; ---- HSPSND ----
        call snd_check
        jc .nosnd
        xor ah, ah
        call far [far_ent]
        push es
        push di
        push bx
        push ax
        PR "HSPSND: cargado, tarjeta "
        pop ax
        movzx eax, al
        call putdec
        PR " ("
        pop bx
        pop si
        pop es
        mov cx, 40
        call puts_far
        PR "), versión "
        mov ax, bx
        call hex4
        push cs
        pop es
        mov ah, 6
        call far [far_ent]
        push ax
        PR "h, volumen "
        pop ax
        push ax
        movzx eax, al
        call putdec
        PR " %"
        pop ax
        test ah, ah
        jz .sv
        PR " (en silencio)"
.sv:
        call crlf
        jmp .joy
.nosnd:
        PR "HSPSND: no está cargado", NL
.joy:
        call fixseg
        ; ---- HSPJOY ----
        mov ax, 0xD900
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .nojoy
        cmp bx, 0x484A
        jne .nojoy
        PR "HSPJOY: cargado, estructura versión "
        movzx eax, byte [es:di+2]
        call putdec
        PR ", puerto de juegos "
        mov ax, [es:di+0x16]
        call hex4
        PR "h (ejes "
        mov al, [es:di+0x5A]
        call hex2
        PR "h), jugador 1 = mando "
        mov al, [es:di+0x0D]
        call hex2
        PR "h, jugador 2 = "
        mov al, [es:di+0x0E]
        call hex2
        PR "h", NL
        movzx cx, byte [es:di+3]
        movzx dx, byte [es:di+4]
        mov bx, [es:di+6]
        cmp cx, 16
        ja .pjoy
        xor ax, ax
.jl:
        test cx, cx
        jz .pjoy
        test byte [es:bx+1], 1
        jz .jn
        push ax
        PR "  mando "
        pop ax
        push ax
        movzx eax, al
        call putdec
        PR ": tipo "
        movzx eax, byte [es:bx]
        call putdec
        PR ", "
        mov ax, [es:bx+4]
        call hex4
        mov al, ':'
        call putc
        mov ax, [es:bx+6]
        call hex4
        PR ", "
        movzx eax, byte [es:bx+2]
        call putdec
        PR " ejes, "
        movzx eax, byte [es:bx+3]
        call putdec
        PR " botones", NL
        pop ax
.jn:
        inc ax
        add bx, dx
        dec cx
        jmp .jl
.nojoy:
        PR "HSPJOY: no está cargado (no hay mandos, o no se ha ejecutado)", NL
.pjoy:
        call fixseg
        ; ---- HSPUSB ----
        cmp byte [usb_res], 0
        je .nousb
        PR "HSPUSB: residente (lleva él un controlador xHCI). Su /INFO solo sale al", NL
        PR "  arrancar (DEVICE=...HSPUSB.SYS /INFO en CONFIG.SYS): no tiene interfaz", NL
        PR "  de consulta; el estado de cada controlador USB está en el apartado PCI.", NL
        jmp .rat
.nousb:
        PR "HSPUSB: no está residente (no se ha cargado, no hay xHCI, o la BIOS usa", NL
        PR "  el USB y no se le ha quitado con /F o /U). Ver el apartado PCI.", NL
.rat:
        ; ---- ratón ----
        call raton_check
        cmp al, 1
        jb .norat
        je .hrat
        PR "Ratón: hay otro controlador de ratón en INT 33h (no es HSPRATON)", NL
        jmp .cd
.hrat:
        PR "HSPRATON: cargado", NL
        jmp .cd
.norat:
        PR "Ratón: no hay controlador en INT 33h", NL
.cd:
        call fixseg
        ; ---- CD (MSCDEX o HSPCD) y teclado ----
        mov ax, 0x1500
        xor bx, bx
        int 0x2F
        test bx, bx
        jz .nocd
        push cx
        push bx
        PR "CD/DVD (INT 2Fh 1500h): "
        pop ax
        movzx eax, ax
        call putdec
        PR " unidad(es), la primera "
        pop ax
        add al, 'A'
        call putc
        PR ":", NL
        jmp .kb
.nocd:
        PR "CD/DVD (INT 2Fh 1500h): ninguna unidad", NL
.kb:
        call fixseg
        mov ax, 0xAD80
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .nokb
        PR "TECLADO (KEYB): cargado", NL
        ret
.nokb:
        PR "TECLADO (KEYB): no está cargado", NL
        ret

; ¿está HSPSND? -> far_ent = su entrada; CF si no
snd_check:
        push es
        push bx
        push di
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne .no
        mov [far_ent], bx
        mov [far_ent+2], es
        clc
        jmp .x
.no:
        stc
.x:
        pop di
        pop bx
        pop es
        ret

; -> AL = 0 no hay INT 33h, 1 HSPRATON, 2 otro
raton_check:
        push es
        push bx
        xor ax, ax
        mov es, ax
        les bx, [es:0x33*4]
        mov ax, es
        or ax, bx
        jz .x                       ; AL = 0
        xor al, al
        cmp byte [es:bx], 0xCF
        je .x
        mov ax, 0x48D2
        int 0x33
        cmp ax, 0x5253
        mov al, 1
        je .x
        mov al, 2
.x:
        pop bx
        pop es
        ret

; =============================================================================
;  ISA Y PUERTOS
; =============================================================================
sec_isa:
        cmp byte [opt_inf], 0
        je .i0
        PR "(si el informe acaba dentro de este apartado, el equipo se colgó sondeando un", NL
        PR " puerto: repita con DIAGNOSTICO /NOISA)", NL
.i0:
        ; ---- PIC ----
        PR "Interrupciones enmascaradas (PIC): 21h="
        in al, 0x21
        call hex2
        PR "h A1h="
        in al, 0xA1
        call hex2
        PR "h (bit a 1 = IRQ apagada)", NL
        ; ---- serie y paralelo (lo que apuntó la BIOS) ----
        xor ax, ax
        mov es, ax
        xor bx, bx
        mov byte [any], 0
.com:
        mov dx, [es:0x400+bx]
        test dx, dx
        jz .comn
        mov byte [any], 1
        PR "COM"
        mov al, bl
        shr al, 1
        add al, '1'
        call putc
        PR " en "
        mov ax, dx
        call hex4
        PR "h"
        inc dx
        in al, dx                   ; IER: los 4 bits altos son siempre 0
        test al, 0xF0
        jz .comok
        PR " (no responde)"
.comok:
        PR "  "
.comn:
        add bx, 2
        cmp bx, 8
        jb .com
        cmp byte [any], 0
        jne .com2
        PR "Puertos serie: la BIOS no ha apuntado ninguno"
.com2:
        call crlf
        xor bx, bx
        mov byte [any], 0
.lpt:
        mov dx, [es:0x408+bx]
        test dx, dx
        jz .lptn
        mov byte [any], 1
        PR "LPT"
        mov al, bl
        shr al, 1
        add al, '1'
        call putc
        PR " en "
        mov ax, dx
        call hex4
        PR "h (estado "
        inc dx
        in al, dx
        call hex2
        PR "h)  "
.lptn:
        add bx, 2
        cmp bx, 6
        jb .lpt
        cmp byte [any], 0
        jne .lpt2
        PR "Puertos paralelo: la BIOS no ha apuntado ninguno"
.lpt2:
        call crlf
        push cs
        pop es
        ; ---- 8042 ----
        call ckpt
        PR "Controlador de teclado 8042 (puerto 64h): "
        in al, 0x64
        cmp al, 0xFF
        jne .kbc
        PR "no hay (lee FFh)", NL
        jmp .gp
.kbc:
        push ax
        PR "presente, estado "
        pop ax
        call hex2
        PR "h", NL
        call ckpt
        call ps2_info
.gp:
        call ckpt
        call gameport
        call ckpt
        ; ---- BLASTER ----
        PR "Variable BLASTER: "
        call blaster
        ; ---- Sound Blaster ----
        call snd_check
        jc .probe
        xor ah, ah
        call far [far_ent]
        call fixseg
        cmp al, 1
        jb .probe
        cmp al, 2
        ja .probe
        PR "Sound Blaster: la está usando HSPSND (no se sondea, para no reiniciarle el", NL
        PR "  DSP): ver HSPSND /INFO.", NL
        jmp .opl
.probe:
        mov byte [any], 0
        mov word [tmpw], 0x220
.sb:
        call ckpt
        mov dx, [tmpw]
        call sb_probe
        add word [tmpw], 0x20
        cmp word [tmpw], 0x280
        jbe .sb
        cmp byte [any], 0
        jne .opl
        PR "Sound Blaster: no responde ninguna en 220h, 240h, 260h ni 280h", NL
.opl:
        call ckpt
        call adlib
        ret

; ---- 8042 y ratón o panel táctil PS/2: solo órdenes de consulta ----
; Con las IRQ 1 y 12 apagadas mientras dura, para que ni la BIOS ni HSPRATON
; se lleven las respuestas. Lo único que se cambia (la resolución, con la
; pregunta de Synaptics, y el envío de datos) se deja como estaba.
ps2_info:
        in al, 0x21
        mov [k_m1], al
        or al, 2
        out 0x21, al
        in al, 0xA1
        mov [k_m2], al
        or al, 0x10
        out 0xA1, al
        call kbc_flush
        PR "  8042, octeto de órdenes (orden 20h): "
        mov al, 0x20
        call kbc_cmd
        jc .t0
        call kbc_read
        jc .t0
        call hex2
        PR "h"
        test al, 0x20
        jz .a1
        PR " (puerto del ratón desactivado)"
.a1:
        call crlf
        PR "  Ratón o panel táctil PS/2: "
        mov al, 0xE9                ; estado
        call aux_cmd
        jc .noaux
        mov di, k_st
        call .rd3
        jc .noaux
        test byte [k_st], 0x20
        jz .a2
        mov al, 0xF5                ; (que no mande datos mientras se pregunta)
        call aux_cmd
.a2:
        mov al, 0xF2                ; identificador
        call aux_cmd
        jc .noid
        call kbc_read
        jc .noid
        PR "ID "
        call hex2
        PR "h (00 normal, 03 con rueda, 04 con 5 botones); "
.noid:
        PR "estado "
        mov si, k_st
        call .pr3
        call crlf
        PR "  Identificación de Synaptics (E8 00 cuatro veces y E9): "
        mov cx, 4
.sy:
        push cx
        mov al, 0xE8
        call aux_cmd
        jc .syf
        xor al, al
        call aux_cmd
        jc .syf
        pop cx
        loop .sy
        mov al, 0xE9
        call aux_cmd
        jc .syf2
        mov di, k_id
        call .rd3
        jc .syf2
        mov si, k_id
        call .pr3
        cmp byte [k_id+1], 0x47
        jne .rest
        PR " -> panel táctil Synaptics, versión "
        movzx eax, byte [k_id+2]
        and al, 15
        call putdec
        mov al, '.'
        call putc
        movzx eax, byte [k_id]
        call putdec
        jmp .rest
.syf:
        pop cx
.syf2:
        PR "sin respuesta"
.rest:
        call crlf
        mov al, 0xE8                ; la resolución, como estaba
        call aux_cmd
        jc .r1
        mov al, [k_st+1]
        and al, 3
        call aux_cmd
.r1:
        test byte [k_st], 0x20
        jz .fin
        mov al, 0xF4
        call aux_cmd
        jmp .fin
.noaux:
        PR "no contesta (no lo hay, o es USB y lo lleva la BIOS)", NL
        jmp .fin
.t0:
        PR "el 8042 no contesta", NL
.fin:
        call kbc_flush
        mov al, [k_m2]
        out 0xA1, al
        mov al, [k_m1]
        out 0x21, al
        ret
.rd3:
        mov cx, 3
.r3:
        call kbc_read
        jc .r3x
        mov [di], al
        inc di
        loop .r3
        clc
.r3x:
        ret
.pr3:
        mov cx, 3
.p3:
        lodsb
        call hex2
        mov al, ' '
        call putc
        loop .p3
        ret

; AL -> el ratón (orden D4h del 8042); CF si no contesta «recibido» (FAh)
aux_cmd:
        push ax
        mov ah, al
        mov al, 0xD4
        call kbc_cmd
        jc .f
        call kbc_wait
        jc .f
        mov al, ah
        out 0x60, al
        call kbc_read
        jc .f
        cmp al, 0xFA
        jne .f
        pop ax
        clc
        ret
.f:
        pop ax
        stc
        ret
; AL -> orden al 8042
kbc_cmd:
        call kbc_wait
        jc .r
        out 0x64, al
        clc
.r:
        ret
; espera a que el 8042 admita un octeto; CF si no
kbc_wait:
        push ax
        push cx
        mov cx, 2
        call t_ini
.l:
        in al, 0x64
        test al, 2
        jz .ok
        call t_fuera
        jnc .l
        pop cx
        pop ax
        stc
        ret
.ok:
        pop cx
        pop ax
        clc
        ret
; un octeto del 8042 -> AL; CF si no llega (0,15 s)
kbc_read:
        push cx
        mov cx, 3
        call t_ini
.l:
        in al, 0x64
        test al, 1
        jnz .g
        call t_fuera
        jnc .l
        pop cx
        stc
        ret
.g:
        call pausa
        in al, 0x60
        pop cx
        clc
        ret
kbc_flush:
        push ax
        push cx
        mov cx, 32
.l:
        in al, 0x64
        test al, 1
        jz .d
        in al, 0x60
        call pausa
        loop .l
.d:
        pop cx
        pop ax
        ret

; ---- puerto de juegos (201h) ----
gameport:
        PR "Puerto de juegos (201h): "
        mov dx, 0x201
        in al, dx
        mov bl, al
        out dx, al                  ; disparo de los ejes
        in al, dx
        mov bh, al
        cmp bx, 0xFFFF
        jne .resp
        ; ¿sigue en FFh? (sin puerto, o con el puerto y ningún mando)
        mov cx, 0x2000
.w:
        in al, dx
        cmp al, 0xFF
        jne .resp
        loop .w
        PR "lee FFh antes y después del disparo: no hay puerto de juegos,", NL
        PR "  o lo hay sin ningún mando conectado", NL
        ret
.resp:
        push bx
        PR "responde; antes del disparo "
        pop bx
        mov al, bl
        call hex2
        PR "h, después "
        mov al, bh
        call hex2
        PR "h", NL
        ; cuánto tarda cada eje (vueltas, con tope)
        mov di, gp_cnt
        mov ax, 0xFFFF
        mov cx, 4
        rep stosw
        pushf
        cli
        out dx, al
        xor cx, cx
.m:
        in al, dx
        mov ah, al
        xor bx, bx
.b:
        shr ah, 1
        jc .bn
        cmp word [gp_cnt+bx], 0xFFFF
        jne .bn
        mov [gp_cnt+bx], cx
.bn:
        add bx, 2
        cmp bx, 8
        jb .b
        test al, 0x0F
        jz .md
        inc cx
        cmp cx, 0x6000
        jb .m
.md:
        popf
        PR "  ejes (vueltas hasta descargarse): "
        xor bx, bx
.p:
        mov si, gp_names
        mov ax, bx
        shl ax, 1
        add si, ax
        mov cx, 3
        push es
        push cs
        pop es
        call puts_fix
        pop es
        mov ax, [gp_cnt+bx]
        cmp ax, 0xFFFF
        jne .pv
        PR "no hay"
        jmp .pn
.pv:
        movzx eax, ax
        call putdec
.pn:
        PR "  "
        add bx, 2
        cmp bx, 8
        jb .p
        jmp crlf
gp_names    db "X1=", 0, "Y1=", 0, "X2=", 0, "Y2=", 0

; ---- la variable BLASTER del entorno ----
blaster:
        push es
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .no
        cmp dword [es:di], 0x53414C42       ; "BLAS"
        jne .n
        cmp dword [es:di+4], 0x3D524554     ; "TER="
        jne .n
        lea si, [di+8]
        mov cx, 60
        call puts_far
        pop es
        jmp crlf
.n:
        inc di
        cmp byte [es:di-1], 0
        jne .n
        cmp di, 0x7F00
        jb .l
.no:
        pop es
        PR "no está definida", NL
        ret

; una pausa corta (unos microsegundos por lectura de puerto)
pausa:
        push ax
        push cx
        mov cx, 24
.l:
        in al, 0x61
        loop .l
        pop cx
        pop ax
        ret

; ---- Sound Blaster en DX (base): reinicio del DSP y versión ----
sb_probe:
        mov bx, dx
        add dx, 6
        mov al, 1
        out dx, al
        call pausa
        xor al, al
        out dx, al
        lea dx, [bx+0x0E]
        mov cx, 3000
.w:
        in al, dx
        test al, 0x80
        jnz .got
        loop .w
        ret
.got:
        lea dx, [bx+0x0A]
        in al, dx
        cmp al, 0xAA
        je .ok
        ret
.ok:
        mov byte [any], 1
        PR "Sound Blaster en "
        mov ax, bx
        call hex4
        PR "h: DSP "
        mov al, 0xE1
        call sb_wr
        jc .nover
        call sb_rd
        jc .nover
        mov [sb_ver+1], al
        call sb_rd
        jc .nover
        mov [sb_ver], al
        movzx eax, byte [sb_ver+1]
        call putdec
        mov al, '.'
        call putc
        mov al, [sb_ver]
        call put2d
        mov si, sb_n1
        cmp byte [sb_ver+1], 2
        jb .nm
        mov si, sb_n2
        je .nm
        mov si, sb_n3
        cmp byte [sb_ver+1], 3
        je .nm
        mov si, sb_n4
.nm:
        call puts
        cmp byte [sb_ver+1], 4
        jb .end
        ; el mezclador de la SB16 dice la IRQ y los DMA
        lea dx, [bx+4]
        mov al, 0x80
        out dx, al
        call pausa
        inc dx
        in al, dx
        PR "; IRQ"
        mov si, sb_irqs
        call bits_list
        dec dx
        mov al, 0x81
        out dx, al
        call pausa
        inc dx
        in al, dx
        PR ", DMA"
        mov si, sb_dmas
        call bits_list
        jmp .end
.nover:
        PR "(no da la versión)"
.end:
        jmp crlf
sb_irqs     db 2, 5, 7, 10, 0xFF, 0xFF, 0xFF, 0xFF
sb_dmas     db 0, 1, 0xFF, 3, 0xFF, 5, 6, 7
; AL = bits, SI -> el número que vale cada bit (FFh ninguno): se escriben
bits_list:
        push ax
        push bx
        push cx
        mov bl, al
        mov cx, 8
.l:
        lodsb
        shr bl, 1
        jnc .n
        cmp al, 0xFF
        je .n
        push ax
        mov al, ' '
        call putc
        pop ax
        movzx eax, al
        call putdec
.n:
        loop .l
        pop cx
        pop bx
        pop ax
        ret
sb_n1       db " (Sound Blaster 1.x)", 0
sb_n2       db " (Sound Blaster 2.0)", 0
sb_n3       db " (Sound Blaster Pro)", 0
sb_n4       db " (Sound Blaster 16 o posterior)", 0

; AL -> DSP (BX = base); CF si no lo acepta
sb_wr:
        push cx
        push dx
        mov ah, al
        lea dx, [bx+0x0C]
        mov cx, 3000
.w:
        in al, dx
        test al, 0x80
        jz .go
        loop .w
        stc
        jmp .x
.go:
        mov al, ah
        out dx, al
        clc
.x:
        pop dx
        pop cx
        ret
; DSP -> AL; CF si no llega nada
sb_rd:
        push cx
        push dx
        lea dx, [bx+0x0E]
        mov cx, 3000
.w:
        in al, dx
        test al, 0x80
        jnz .go
        loop .w
        stc
        jmp .x
.go:
        lea dx, [bx+0x0A]
        in al, dx
        clc
.x:
        pop dx
        pop cx
        ret

; ---- AdLib / OPL en 388h: la prueba clásica de los temporizadores ----
adlib:
        PR "AdLib/OPL (388h): "
        mov ax, 0x6004              ; parar los temporizadores
        call opl_wr
        mov ax, 0x8004              ; borrar las banderas
        call opl_wr
        mov dx, 0x388
        in al, dx
        mov bl, al
        mov ax, 0xFF02              ; temporizador 1 = FFh
        call opl_wr
        mov ax, 0x2104              ; y en marcha
        call opl_wr
        mov cx, 0x4000              ; (80 microsegundos; con tope)
.w:
        in al, dx
        mov bh, al
        and al, 0xE0
        cmp al, 0xC0
        je .d
        loop .w
.d:
        mov ax, 0x6004
        call opl_wr
        mov ax, 0x8004
        call opl_wr
        test bl, 0xE0
        jnz .no
        mov al, bh
        and al, 0xE0
        cmp al, 0xC0
        jne .no
        test bh, 6
        jnz .opl2
        PR "responde, OPL3 (estado "
        jmp .st
.opl2:
        PR "responde, OPL2 (estado "
.st:
        mov al, bh
        call hex2
        PR "h)", NL
        ret
.no:
        PR "no responde (estados "
        mov al, bl
        call hex2
        PR "h y "
        mov al, bh
        call hex2
        PR "h)", NL
        ret
; AL = registro, AH = valor
opl_wr:
        push dx
        mov dx, 0x388
        out dx, al
        call pausa
        inc dx
        mov al, ah
        out dx, al
        call pausa
        call pausa
        pop dx
        ret

; =============================================================================
;  /INFORME: el informe completo en un archivo (lo que lanza DIAGNOSTICO)
; =============================================================================
informe:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        cmp dl, 'A'
        jb .nodrv
        cmp dl, 'Z'
        jbe .drv
.nodrv:
        mov ah, 0x19
        int 0x21
        mov dl, al
        add dl, 'A'
.drv:
        mov [c_cfg+5], dl
        mov [c_aut+5], dl
        mov [comspec_def], dl
        mov [aud_path], dl
        ; la carpeta: X:\INFORMES\EQUIPOnn, una nueva cada vez
        mov di, dir_path
        mov al, dl
        stosb
        mov si, k_informes
        call .cpy
        mov dx, dir_path
        mov ah, 0x39
        int 0x21                    ; (si ya existe, da igual)
        mov ax, 0x4300
        mov dx, dir_path
        int 0x21
        jc .nodir
        mov si, k_equipo
        call .cpy
        mov byte [cnt], 1
.name:
        mov al, [cnt]
        aam
        add ax, 0x3030
        xchg al, ah
        mov [di-2], ax
        mov ax, 0x4300
        mov dx, dir_path
        int 0x21
        jnc .next                   ; ya existe: la siguiente
        mov dx, dir_path
        mov ah, 0x39
        int 0x21
        jnc .hecho
        jmp .nodir
.next:
        inc byte [cnt]
        cmp byte [cnt], 100
        jb .name
        PR "Hay demasiadas carpetas de informes en la unidad de arranque: borre alguna", NL
        PR "(\INFORMES\EQUIPOnn).", NL
        jmp .mal
.cpy:
        lodsb
        stosb
        test al, al
        jnz .cpy
        dec di
        ret
.nodir:
        PR "No se puede crear la carpeta del informe ("
        mov si, dir_path
        call puts
        PR "):", NL
        PR "¿la unidad está protegida contra escritura o llena?", NL
        PR "EQUIPO > archivo guarda el informe del hardware donde se le diga.", NL
.mal:
        call flush
        mov ax, 0x4C01
        int 0x21
.hecho:
        mov word [di], '\'
        inc di
        mov [dir_end], di
        mov si, n_informe
        call mkpath
        mov si, fpath
        mov di, rep_path
        call .cpy
        mov dx, rep_path
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .nodir
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov si, n_dpci
        call mksub
        mov si, n_dacpi
        call mksub
        mov si, n_dusb
        call mksub
        call find_comspec
        PR "Preparando el informe del equipo en "
        mov si, rep_path
        call puts
        PR " (tarda un poco)...", NL
        call flush
        ; ---- cabecera ----
        call rep_open
        PR "Informe de diagnóstico de Hispa-DOS", NL
        PR "Archivo: "
        mov si, rep_path
        call puts
        PR NL, "En esta carpeta van también los volcados en binario: PCI\*.BIN (y *.EXT),", NL
        PR "ACPI\*.BIN, USB\*.BIN, SMBIOS.BIN, SMBIOSEP.BIN, CMOS.BIN, IVT.BIN, BDA.BIN,", NL
        PR "EBDA.BIN, VBE.BIN, VBEMODOS.BIN, EDID.BIN, EDDnn.BIN, ATAxxx_n.BIN, y los", NL
        PR "textos de las ROM (ROMBIOS.TXT y ROMVIDEO.TXT)."
        PR NL, "Fecha: "
        mov ah, 0x2A
        int 0x21
        mov al, dl
        call put2d
        mov al, '-'
        call putc
        mov al, dh
        call put2d
        mov al, '-'
        call putc
        movzx eax, cx
        call putdec
        PR "   Hora: "
        mov ah, 0x2C
        int 0x21
        mov al, ch
        call put2d
        mov al, ':'
        call putc
        mov al, cl
        call put2d
        mov al, ':'
        call putc
        mov al, dh
        call put2d
        call crlf
        PR "Apartados de EQUIPO pedidos (máscara): "
        mov ax, [mask]
        call hex2
        PR "h", NL
        call rep_close
        ; ---- los pasos ----
        mov si, c_ver
        call paso
        call run_sections
        mov si, c_mem
        call paso
        mov si, c_set
        call paso
        call snd_check
        jc .nosnd
        mov si, c_snd
        call paso
        jmp .rat
.nosnd:
        mov si, c_snd
        mov di, m_nosnd
        call paso_nota
.rat:
        call raton_check
        cmp al, 1
        jne .norat
        mov si, c_rat
        call paso
        jmp .mando
.norat:
        mov si, c_rat
        mov di, m_norat
        call paso_nota
.mando:
        mov si, c_mando
        call paso
        mov si, c_cfg
        call paso
        mov si, c_aut
        call paso
        call audicion
        call rep_open
        PR NL, "==== FIN DEL INFORME ====", NL
        call rep_close
        mov word [out_h], 1
        cmp byte [disk_full], 0
        je .okd
        PR NL, "ATENCIÓN: la unidad se ha llenado y el informe ha quedado incompleto.", NL
.okd:
        PR NL, "Informe guardado en "
        mov bx, [dir_end]
        mov byte [bx-1], 0          ; (sin la barra del final)
        mov si, dir_path
        call puts
        PR ". Suba esa carpeta al FTP en HispaDOS/informes", NL
        ; sin HSPSND (la opción «Prueba» del menú no lo carga, a propósito):
        ; que nadie piense que el equipo no tiene sonido
        push es
        mov ax, 0xD700
        int 0x2F
        pop es
        cmp al, 0xFF
        je .consnd
        PR NL, "Para tener sonido (también en Hispa Ventanas), reinicie y elija", NL
        PR "«Normal» o «Juegos» en el menú de arranque.", NL
.consnd:
        call flush
        mov ax, 0x4C00
        int 0x21

; ---- la prueba de sonido (AUDICION), si está en \HISPADOS de la unidad de
; arranque: se ofrece al final y su informe va a la misma carpeta. Interfaz:
;     AUDICION carpeta_del_informe
; (por ejemplo AUDICION C:\INFORMES\EQUIPO01); AUDICION guarda en ella
; SONIDO.TXT. /NOSONIDO no lo ofrece.
audicion:
        cmp byte [opt_nosnd], 0
        jne .r
        mov dword [aud_ext], "COM"
        call .hay
        jnc .si
        mov dword [aud_ext], "EXE"
        call .hay
        jnc .si
        mov dword [aud_ext], "BAT"
        call .hay
        jnc .si
.r:
        ret
.hay:
        mov ax, 0x4300
        mov dx, aud_path
        int 0x21
        ret
.si:
        mov word [out_h], 1
        PR NL, "¿Quiere hacer ahora la prueba de sonido? (S/N) "
        call flush
.k:
        mov ah, 0x08
        int 0x21
        test al, al
        jz .k
        and al, 0xDF
        cmp al, 'S'
        je .go
        cmp al, 'N'
        jne .k
        PR "N", NL
        call flush
        mov word [out_h], 0xFFFF
        mov si, c_aud
        mov di, m_audno
        jmp paso_nota
.go:
        PR "S", NL
        call flush
        mov word [out_h], 0xFFFF
        mov si, c_aud
        mov di, m_audsi
        call paso_nota
        ; la orden: X:\HISPADOS\AUDICION carpeta
        mov byte [aud_ext-1], 0
        mov si, aud_path
        mov di, strcmd
        call .cp
        mov al, ' '
        stosb
        mov si, dir_path
        call .cp
        ; AUDICION quiere la carpeta (guarda en ella SONIDO.TXT), sin la
        ; barra del final
        cmp byte [di-1], '\'
        jne .nobarra
        dec di
        mov byte [di], 0
.nobarra:
        mov byte [aud_ext-1], '.'
        mov si, strcmd
        mov byte [redir], 0
        jmp ejecuta
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        dec di
        ret

; SI -> orden: su título al informe y su salida detrás (%COMSPEC% /C orden >> informe)
paso:
        push si
        call titulo
        call rep_close
        pop si
        mov byte [redir], 1
; SI -> orden: se ejecuta con %COMSPEC% /C (redir = 1: su salida, al informe)
ejecuta:
        ; la cola: " /C orden >>informe"
        mov di, ctail + 1
        mov dword [di], " /C "
        add di, 4
.c:
        lodsb
        test al, al
        jz .ce
        stosb
        jmp .c
.ce:
        cmp byte [redir], 0
        je .pe
        mov ax, " >"
        stosw
        mov al, '>'
        stosb
        mov si, rep_path
.p:
        lodsb
        test al, al
        jz .pe
        stosb
        jmp .p
.pe:
        mov byte [di], 13
        mov ax, di
        sub ax, ctail + 1
        mov [ctail], al
        ; el bloque de parámetros
        mov word [pblk], 0
        mov word [pblk+2], ctail
        mov [pblk+4], cs
        mov word [pblk+6], fcb0
        mov [pblk+8], cs
        mov word [pblk+10], fcb0
        mov [pblk+12], cs
        mov [sv_sp], sp
        mov dx, [comspec_p]
        mov bx, pblk
        mov ax, 0x4B00
        int 0x21
        cli
        mov bx, cs
        mov ss, bx
        mov sp, [cs:sv_sp]
        sti
        mov ds, bx
        mov es, bx
        cld
        jnc .r
        cmp byte [redir], 0
        je .r
        call rep_open
        PR "(no se ha podido ejecutar el intérprete de órdenes para este paso)", NL
        call rep_close
.r:
        ret

; SI -> orden, DI -> nota: el título y la nota en vez de ejecutarla
paso_nota:
        push di
        call titulo
        pop si
        call puts
        jmp rep_close

; %COMSPEC%, o COMMAND.COM de la raíz de la unidad de arranque
find_comspec:
        mov word [comspec_p], comspec_def
        push es
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .x
        cmp dword [es:di], 0x534D4F43       ; "COMS"
        jne .n
        cmp dword [es:di+4], 0x3D434550     ; "PEC="
        jne .n
        add di, 8
        mov si, comspec
        mov cx, 78
.c:
        mov al, [es:di]
        mov [si], al
        inc si
        inc di
        test al, al
        jz .ok
        loop .c
        jmp .x
.ok:
        mov word [comspec_p], comspec
        jmp .x
.n:
        inc di
        cmp byte [es:di-1], 0
        jne .n
        cmp di, 0x7F00
        jb .l
.x:
        pop es
        ret

; (los títulos de los pasos son las propias órdenes)
c_ver       db "VER", 0
c_mem       db "MEM /C", 0
c_set       db "SET", 0
c_snd       db "HSPSND /INFO", 0
c_rat       db "HSPRATON /INFO", 0
c_mando     db "MANDO", 0
c_cfg       db "TYPE ?:\CONFIG.SYS", 0
c_aut       db "TYPE ?:\AUTOEXEC.BAT", 0
m_nosnd     db "HSPSND no está cargado: no se ejecuta HSPSND /INFO (se quedaría residente).", 13, 10, 0
m_norat     db "HSPRATON no está cargado: no se ejecuta HSPRATON /INFO.", 13, 10, 0
comspec_def db "?:\COMMAND.COM", 0
k_informes  db ":\INFORMES", 0
k_equipo    db "\EQUIPO00", 0
n_informe   db "INFORME.TXT", 0
n_dpci      db "PCI", 0
n_dacpi     db "ACPI", 0
n_dusb      db "USB", 0
c_aud       db "AUDICION (prueba de sonido)", 0
m_audno     db "AUDICION está, pero no se ha querido hacer la prueba de sonido.", 13, 10, 0
m_audsi     db "Prueba de sonido lanzada: su informe es SONIDO.TXT, en esta carpeta.", 13, 10, 0
aud_path    db "?:\HISPADOS\AUDICION."
aud_ext     db "COM", 0

; SI -> nombre relativo de una carpeta: se crea (con /INFORME)
mksub:
        cmp byte [opt_inf], 0
        je .r
        pushad
        call mkpath
        mov dx, fpath
        mov ah, 0x39
        int 0x21
        popad
.r:
        ret

m_uso:
        db "EQUIPO: informe completo del hardware del equipo, en texto.", 13, 10
        db 13, 10
        db "EQUIPO [/CPU] [/BIOS] [/VESA] [/PCI] [/DISCOS] [/RES] [/ISA] [/NOapartado]", 13, 10
        db 13, 10
        db "  /CPU     procesador: fabricante, familia, modelo, nombre, banderas", 13, 10
        db "  /BIOS    BIOS (fecha, fabricante, SMBIOS, ACPI, APM) y mapa de memoria", 13, 10
        db "  /VESA    vídeo: versión de VBE, memoria, fabricante y modos", 13, 10
        db "  /PCI     cada dispositivo PCI: clase con su nombre (USB UHCI/OHCI/EHCI/xHCI,", 13, 10
        db "           HD Audio, AC'97, IDE, SATA...), direcciones, IRQ y de quién es", 13, 10
        db "           cada controlador USB (de la BIOS o libre)", 13, 10
        db "  /DISCOS  disqueteras y discos de la BIOS (INT 13h)", 13, 10
        db "  /RES     residentes: XMS, EMS, DPMI, HSPSND, HSPJOY, HSPUSB, ratón, CD", 13, 10
        db "  /ISA     Sound Blaster, AdLib, puerto de juegos, serie, paralelo, teclado", 13, 10
        db 13, 10
        db "Sin opciones salen todos. /NOapartado quita uno: EQUIPO /NOISA.", 13, 10
        db "La salida es texto corrido y se puede guardar:  EQUIPO > EQUIPO.TXT", 13, 10
        db "DIAGNOSTICO guarda el informe completo (esto y más) en INFORME.TXT.", 13, 10, 0
m_usod:
        db "DIAGNOSTICO: guarda un informe completo del equipo para enviarlo.", 13, 10
        db 13, 10
        db "DIAGNOSTICO [/NOapartado] [/NOSONIDO]", 13, 10
        db 13, 10
        db "Crea en la unidad de arranque una carpeta nueva cada vez, \INFORMES\EQUIPO01,", 13, 10
        db "EQUIPO02..., con INFORME.TXT (la versión, la fecha, todo lo que dice EQUIPO,", 13, 10
        db "MEM /C, SET, HSPSND /INFO, HSPRATON /INFO, MANDO, CONFIG.SYS y AUTOEXEC.BAT)", 13, 10
        db "y los volcados en binario del equipo: PCI, ACPI, SMBIOS, USB, vídeo (VBE y", 13, 10
        db "EDID), CMOS, discos y los textos de las ROM. Solo lee: no cambia nada.", 13, 10
        db "Al acabar dice dónde lo ha dejado: suba esa carpeta al FTP, a", 13, 10
        db "HispaDOS/informes. Si está AUDICION, ofrece hacer la prueba de sonido.", 13, 10
        db 13, 10
        db "Si el equipo se quedara parado, lo escrito hasta ese momento queda en el", 13, 10
        db "archivo. Repita entonces sin ese apartado: DIAGNOSTICO /NOISA (sin sondear", 13, 10
        db "los puertos ISA ni el ratón PS/2), /NOPCI, /NOVESA, /NODISCOS, /NOBIOS,", 13, 10
        db "/NOCPU o /NORES. /NOSONIDO no ofrece la prueba de sonido.", 13, 10, 0

; =============================================================================
;  Datos sin iniciar
; =============================================================================
        align 16
bss_start:
        absolute bss_start
opos        resw 1
out_h       resw 1
mask        resw 1
sel_mask    resw 1
no_mask     resw 1
opt_nosnd   resb 1
disk_full   resb 1
redir       resb 1
a_ent       resb 1
a_isfacs    resb 1
d_sum       resb 1
d_err       resb 1
sm_eplen    resb 1
u_np        resb 1
u_i         resb 1
u_str       resb 1
u_kind      resb 1
h_wasrst    resb 1
h_cad       resb 1
at_drv      resb 1
at_atapi    resb 1
k_m1        resb 1
k_m2        resb 1
k_st        resb 3
k_id        resb 3
mcfg_b0     resb 2
            alignb 2
dmp_h       resw 1
dir_end     resw 1
sm_ep       resw 1
a_n         resw 1
a_i         resw 1
a_nseen     resw 1
rs_good     resw 1
rs_dist     resw 1
rs_map      resb 16
rs_seg      resw 1
rs_len      resw 1
t_fin       resw 1
u_io        resw 1
at_base     resw 1
opt_help    resb 1
opt_inf     resb 1
any         resb 1
col         resb 1
has_fpu     resb 1
usb_res     resb 1
pci_bios    resb 1
pci_maxbus  resb 1
bar_i       resb 1
bar_n       resb 1
m_n         resb 1
p_hdr       resb 1
dnum        resb 1
a20_closed  resb 1
sm_type     resb 1
            resb 1
tmpw        resw 1
cnt         resw 1
sv_sp       resw 1
comspec_p   resw 1
sm_ver      resw 1
sm_cnt      resw 1
sb_ver      resw 1
p_bdf       resw 1
p_count     resw 1
            alignb 4
p_id        resd 1
p_class     resd 1
p_cmd       resd 1
m_base      resd 1
m_cap       resd 1
m_ecp       resd 1
sm_addr     resd 1
sm_left     resd 1
cpu_max     resd 1
cpu_emax    resd 1
c1_eax      resd 1
c_regs      resd 4
tsc0        resd 2
tot         resd 2
lfb         resd 1
e820nx      resd 1
far_ent     resd 1
d_addr      resd 1
d_len       resd 1
a_root      resd 1
a_dsdt      resd 1
a_facs      resd 1
mcfg_base   resd 1
t_cnt       resd 1
u_off       resd 1
u_ecp0      resd 1
a_seen      resd 64
rsdp        resb 40
hdrbuf      resb 68
fname       resb 24
dir_path    resb 40
fpath       resb 72
rep_path    resb 72
strcmd      resb 120
edidbuf     resb 132
idbuf       resb 512
dbuf        resb 1028
rootbuf     resb 1028
mtmp        resd 2
gp_cnt      resw 4
e820buf     resb 28
tok         resb 16
strbuf      resb 52
mbuf        resb 68
i15gdt      resb 48
eddbuf      resb 0x60
pblk        resb 14
fcb0        resb 40
comspec     resb 80
ctail       resb 130
obuf        resb OBUF_SZ + 4
vbebuf      resb 520
modebuf     resb 264
modelist    resw 162
smbuf       resb SMBUF_SZ + 8
bss_end:
            resb 4096
stack_top:
prog_end:
