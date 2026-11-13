; =============================================================================
;  HSPDPMI.COM - servidor DPMI 0.9 de Hispa-DOS (residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPDPMI        instalar (en AUTOEXEC.BAT; necesita HSPXMS.SYS u otro
;                       gestor XMS y un 386 o superior)
;        HSPDPMI /D     descargar: lo quita de la memoria si no hay clientes
;                       (para los programas que traen su propio extensor);
;                       si después se cargó otro residente que usa INT 2Fh,
;                       no se puede quitar y queda desactivado hasta que se
;                       ejecute HSPDPMI otra vez
;
;  Primera etapa de Hispa Ventanas (docs/VENTANAS.md). Escrito desde cero a
;  partir de la especificación pública de DPMI 0.9/1.0.
;
;  Cómo funciona:
;  - Sin paginación ni V86: el cliente corre en modo protegido (anillo 3, con
;    IOPL 3) y, para las interrupciones y las llamadas a DOS/BIOS, se vuelve a
;    modo real de verdad (conmutación «en crudo» por CR0) y se regresa.
;  - El servidor corre en anillo 0, siempre con las interrupciones cerradas.
;    Toda entrada desde el anillo 3 es por una puerta de interrupción de la
;    IDT, que cambia a la pila de anillo 0 de la TSS.
;  - La LDT (8192 descriptores), la IDT, las tablas de memoria y las pilas
;    (anillo 0 y pila bloqueada del cliente) están en un bloque de memoria
;    extendida pedido a XMS mientras hay clientes («bloque del servidor»).
;  - La memoria de los clientes (0501h) sale de bloques XMS bloqueados que se
;    reparten por dentro (así no se gastan los handles de XMS).
;  - Al entrar un cliente se cambia la dirección de terminación de su PSP
;    (PSP:0Ah): termine como termine (4Ch, Ctrl+C, error crítico, excepción),
;    DOS pasa por aquí y se libera todo lo suyo.
;  - Clientes anidados: el estado del anterior se guarda en la zona privada
;    que el cliente nuevo entrega al entrar (INT 2Fh AX=1687h, SI párrafos).
; =============================================================================
        cpu 586
        bits 16
        org 0x100

; ---- selectores de la GDT ----
SEL_HCODE   equ 0x08                ; código del servidor (16 bits, anillo 0)
SEL_HDATA   equ 0x10                ; datos del servidor
SEL_FLAT    equ 0x18                ; toda la memoria (anillo 0)
SEL_HB      equ 0x20                ; bloque del servidor (y pila de anillo 0)
SEL_LDTA    equ 0x28                ; la LDT vista como datos
SEL_TSS     equ 0x30
SEL_LDT     equ 0x38
SEL_BIOS    equ 0x40                ; datos de la BIOS (segmento 40h), anillo 3
SEL_R3      equ 0x48+3              ; trampolines de anillo 3
SEL_LPM     equ 0x50+3              ; pila bloqueada del cliente
SEL_RMSTK   equ 0x58+3              ; pila de modo real (en los callbacks)
SEL_VCPI    equ 0x60                ; tres descriptores del servidor VCPI
GDT_SIZE    equ 0x78

; ---- bloque del servidor (memoria extendida) ----
HB_IDT      equ 0x0000              ; 256 puertas
HB_REG      equ 0x0800              ; regiones de memoria (NREG x 16)
HB_ARN      equ 0x1800              ; bloques XMS (NARN x 16)
HB_OWN      equ 0x1A00              ; dueño de cada descriptor de la LDT
HB_R0TOP    equ 0x5000              ; cima de la pila de anillo 0
HB_LPM      equ 0x5000              ; pila bloqueada (anillo 3)
LPM_SIZE    equ 0x2000
HB_LDT      equ 0x10000
HB_SIZE     equ 0x20000
HB_KB       equ 128
LDT_N       equ 8192
LDT_FIRST   equ 16                  ; los 15 primeros, para la función 000Dh
NREG        equ 256
NARN        equ 24
ARENA_MIN   equ 0x40000             ; bloque XMS mínimo: 256 KB
HMAGIC      equ 0x4850              ; parte alta de los handles de memoria

NCB         equ 16                  ; callbacks de modo real
NHOOK       equ 19                  ; vectores que se reflejan de real a protegido
RM_STACK    equ 0x0C00              ; pila de modo real del servidor
RM_CHUNK    equ 0x0180              ; lo que se reserva por nivel
MAXLEVEL    equ 100

; ---- marco de registros en la pila de anillo 0 (BP -> marco) ----
CF_GS       equ 0
CF_FS       equ 2
CF_ES       equ 4
CF_DS       equ 6
CF_EDI      equ 8
CF_ESI      equ 12
CF_EBP      equ 16
CF_EBX      equ 24
CF_EDX      equ 28
CF_ECX      equ 32
CF_EAX      equ 36
CF_VEC      equ 40
CF_EIP      equ 44
CF_CS       equ 48
CF_EFL      equ 52
CF_ESP      equ 56
CF_SS       equ 60
CF_SIZE     equ 64

; ---- trampolines de anillo 3 (desplazamientos dentro de SEL_R3) ----
R3_EXC      equ 0x200               ; 32 manejadores de excepción por defecto
R3_LPMRET   equ 0x240               ; vuelta de un manejador de IRQ
R3_EXCRET   equ 0x242               ; vuelta de un manejador de excepción
R3_CBRET    equ 0x244               ; vuelta de un callback
R3_HOOKRET  equ 0x246               ; vuelta de una interrupción venida de real
R3_RAWSW    equ 0x248               ; conmutación en crudo a modo real
R3_SAVE16   equ 0x24A               ; guardar/restaurar estado (no hace nada)
R3_SAVE32   equ 0x24B
R3_END      equ 0x24D

start:
        jmp install

; =============================================================================
;  Datos residentes
; =============================================================================
            align 4
old2f       dd 0
xms         dd 0                    ; punto de entrada del gestor XMS
hostseg     dw 0
level       db 0                    ; clientes activos (anidados)
cputype     db 3
hb_handle   dw 0
oculto      db 0                    ; 1: desactivado (HSPDPMI /D sin poder descargar)
            db 0
hb_fresh    db 0
cs_big      db 0
hb_base     dd 0
pool_total  dd 0
gdtr        dw GDT_SIZE-1
            dd 0
pm_idtr     dw 0x7FF
            dd 0
rm_idtr     dw 0x3FF
            dd 0
            align 4
tss         times 104 db 0
gdt         dq 0
            dw 0xFFFF, 0
            db 0, 0x9A, 0, 0        ; 08 código del servidor
            dw 0xFFFF, 0
            db 0, 0x92, 0, 0        ; 10 datos del servidor
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0     ; 18 plano
            dw 0xFFFF, 0
            db 0, 0x92, 0, 0        ; 20 bloque del servidor
            dw 0xFFFF, 0
            db 0, 0x92, 0, 0        ; 28 LDT como datos
            dw 103, 0
            db 0, 0x89, 0, 0        ; 30 TSS
            dw 0xFFFF, 0
            db 0, 0x82, 0, 0        ; 38 LDT
            dw 0xFFFF, 0x400
            db 0, 0xF2, 0, 0        ; 40 datos de la BIOS
            dw R3_END-1, 0
            db 0, 0xFA, 0, 0        ; 48 trampolines de anillo 3
            dw LPM_SIZE-1, 0
            db 0, 0xF2, 0, 0        ; 50 pila bloqueada
            dw 0xFFFF, 0
            db 0, 0xF2, 0, 0        ; 58 pila de modo real
            dq 0, 0, 0              ; 60-70 los del servidor VCPI (HSPEMM)

; ---- cliente VCPI: cuando el procesador ya está en modo V86 (HSPEMM u otro
;      EMM386), el modo protegido se pide al servidor VCPI (INT 67h DE0Ch) ----
vcpi        db 0                    ; 1 = conmutar por VCPI
            align 4
vcpi_entry  dd 0                    ; entrada del servidor en modo protegido
            dw SEL_VCPI
            align 4
vsw_cr3     dd 0                    ; lo que pide DE0Ch: CR3,
vsw_gdtr    dd 0                    ; dirección lineal del GDTR,
vsw_idtr    dd 0                    ; del IDTR,
            dw SEL_LDT              ; LDTR,
            dw SEL_TSS              ; TR
            dd go_pm.vpm            ; y CS:EIP
            dw SEL_HCODE
            align 4
vsw_lin     dd 0                    ; dirección lineal de vsw_cr3
vsw_esi     dd 0
; el espacio lineal por VCPI: con PSE (Pentium), páginas de 4 MB; sin PSE
; (386 y 486), tablas de 4 KB en un bloque XMS bloqueado: las de la RAM y un
; fondo de VT_EXTRA para la memoria física de fuera (0800h: marcos lineales)
vpse        db 0                    ; 1 = páginas de 4 MB (CR4.PSE)
            align 4
vt_dir      dd 0                    ; dirección del directorio (primer mega)
vt_next     dd 0                    ; siguiente tabla libre del fondo
vt_end      dd 0                    ; fin del fondo
vt_handle   dw 0                    ; bloque XMS de las tablas (0: ninguno)
            times 80 db 0           ; pila para volver al modo V86
vcpi_stk:

; ---- estado de cada cliente (se guarda entero al entrar otro) ----
state_begin:
pmvec       times 256*6 db 0        ; vectores de modo protegido (dd, dw)
excvec      times 32*6 db 0         ; manejadores de excepción
is32        db 0
            db 0
cl_psp      dw 0
cl_env      dw 0
cl_save     dw 0                    ; segmento de la zona privada
cl_term     dd 0                    ; dirección de terminación original
st_esp0     dd 0
rm_free     dw 0                    ; pila de modo real libre por debajo de aquí
lpm_free    dd 0                    ; pila bloqueada libre por debajo de aquí
hookon      times NHOOK db 0        ; vectores de modo real enganchados
            align 2, db 0
state_end:
STATE_LEN   equ state_end - state_begin
PRIV_PARAS  equ (STATE_LEN + 15) / 16

rm_old      times NHOOK dd 0
hookvec     db 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
            db 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77
            db 0x1C, 0x23, 0x24
            align 2
cb_tab      times NCB*16 db 0       ; +0 dueño, +2 proc (dd, dw), +8 estructura (dd, dw)
hrm         times 0x32 db 0         ; registros de modo real para las llamadas propias
xr          times 0x32 db 0         ; registros para saltar a modo real
xmove       times 16 db 0
exc_err     dd 0
ma_size     dd 0
rc_target   dd 0
rc_cx       dw 0
rc_mode     db 0
            db 0
t_off       dd 0
t_esp       dd 0
t_efl       dd 0
t_sel       dw 0
t_ss        dw 0
t_sp        dw 0
t_term      dd 0
t_rmfree    dw 0
t_es        dw 0
raw_ebx     dd 0
raw_ax      dw 0
raw_fl      dw 0
e_blk       dd 0
e_ss        dw 0
e_sp        dw 0
e_cssel     dw 0
e_sssel     dw 0
e_dssel     dw 0
e_essel     dw 0
dm_seg      dw 0
dm_n        dw 0

            align 2
rm_stack    times RM_STACK db 0
rm_stack_top:

; =============================================================================
;  Trampolines: anillo 3, IDT, callbacks y vectores de modo real
; =============================================================================
r3stubs:
        times 256+32+5 db 0xCD, 0x31    ; int 31h
        retf                            ; R3_SAVE16
        o32 retf                        ; R3_SAVE32

idt_stubs:
%rep 256
        call int_entry
        nop
%endrep

cb_stubs:
%rep NCB
        call cb_entry
        nop
%endrep

rmhook_stubs:
%rep NHOOK
        call rmhook_entry
        nop
%endrep

; =============================================================================
;  Modo real: INT 2Fh y entrada de los clientes
; =============================================================================
; al descargar: soltar el bloque XMS de las tablas de páginas (VCPI sin PSE)
vt_free:
        cmp word [cs:vt_handle], 0
        je .r
        push ax
        push bx
        push dx
        mov dx, [cs:vt_handle]
        mov ah, 0x0D
        call far [cs:xms]
        mov dx, [cs:vt_handle]
        mov ah, 0x0A
        call far [cs:xms]
        mov word [cs:vt_handle], 0
        pop dx
        pop bx
        pop ax
.r:
        ret

int2f:
        cmp ax, 0x1687
        je .det
        cmp ax, 0xD600
        je .priv
.pasa:
        jmp far [cs:old2f]
; ---- interfaz propia: AX=D600h BX=4844h ("HD"), DL=función ----
;   DL=0  ¿está?     -> AX=4450h ("DP"), BL=1 si está desactivado, BH=clientes
;   DL=1  descargar  -> AX=4450h, BL=2: hecho, y ES=segmento que hay que
;                       liberar (INT 21h, 49h); BL=1: no se ha podido quitar
;                       de la cadena de INT 2Fh (otro residente detrás) y
;                       queda desactivado: no responde a 1687h; BL=FFh: hay
;                       clientes y no se toca
;   DL=2  activar de nuevo -> como DL=0
;   DL=3  desactivar (sin moverlo de la memoria) -> como DL=0; BL=FFh si
;         hay clientes
.priv:
        cmp bx, 0x4844
        jne .pasa
        cmp dl, 3
        ja .pasa
        je .ocultar
        cmp dl, 2
        je .activar
        cmp dl, 1
        je .quitar
.info:
        mov ax, 0x4450
        mov bl, [cs:oculto]
        mov bh, [cs:level]
        push cs
        pop es
        iret
.activar:
        mov byte [cs:oculto], 0
        jmp .info
.ocultar:
        cmp byte [cs:level], 0
        je .oc1
        mov ax, 0x4450
        mov bh, [cs:level]
        mov bl, 0xFF
        jmp .q9
.oc1:
        mov byte [cs:oculto], 1
        jmp .info
.quitar:
        mov ax, 0x4450
        mov bh, [cs:level]
        mov bl, 0xFF
        test bh, bh
        jnz .q9
        push ds
        push si
        push cx
        xor si, si
        mov ds, si
        mov bl, 1
        mov cx, cs
        cmp word [0x2F*4], int2f
        jne .q1
        cmp [0x2F*4+2], cx
        jne .q1
        cli
        mov cx, [cs:old2f]
        mov [0x2F*4], cx
        mov cx, [cs:old2f+2]
        mov [0x2F*4+2], cx
        mov bl, 2
        call vt_free
.q1:
        mov [cs:oculto], bl         ; (1: desactivado; si se descarga, da igual)
        pop cx
        pop si
        pop ds
.q9:
        push cs
        pop es
        iret
.det:
        cmp byte [cs:oculto], 0
        jne .pasa
        xor ax, ax
        mov bx, 1                   ; admite clientes de 32 bits
        mov cl, [cs:cputype]
        mov dx, 0x005A              ; DPMI 0.90
        mov si, PRIV_PARAS
        push cs
        pop es
        mov di, dpmi_entry
        iret

; guardar/restaurar estado en modo real (0305h): nada que guardar
rm_save_stub:
        retf

; ---- entrada a modo protegido (llamada lejana): AX bit 0 = cliente de 32
;      bits, ES = zona privada ----
dpmi_entry:
        pushf
        pushad
        push ds
        push es
        mov bp, sp                  ; +0 ES +2 DS +4 EDI ... +32 EAX +36 flags +38 IP +40 CS
        push cs
        pop ds
        cld
        cmp byte [level], 0
        jne .have
        call hb_alloc
        jc .fail
.have:
        cmp byte [level], MAXLEVEL
        jae .fail
        mov ah, 0x51
        int 0x21                    ; BX = PSP del cliente
        cli
        mov eax, [tss+4]
        mov [st_esp0], eax
        mov es, [bp]
        xor di, di
        mov si, state_begin
        mov cx, STATE_LEN
        rep movsb                   ; estado del anterior -> zona privada
        mov [cl_save], es
        mov [cl_psp], bx
        mov al, [bp+32]
        and al, 1
        mov [is32], al
        call set_lpm_bits
        mov es, bx
        mov eax, [es:0x0A]
        mov [cl_term], eax
        mov word [es:0x0A], term_stub
        mov [es:0x0C], cs
        mov ax, [es:0x2C]
        mov [cl_env], ax
        inc byte [level]
        mov dx, ss
        mov si, bp
        mov bx, .pm
        jmp go_pm
.fail:
        pop es
        pop ds
        popad
        popf
        stc
        retf

.pm:    ; ---- anillo 0 ----
        cmp byte [hb_fresh], 0
        je .ready
        mov byte [hb_fresh], 0
        ; bloque recién pedido: a cero (sin usar la pila, que está dentro)
        mov ax, SEL_FLAT
        mov es, ax
        mov edi, [hb_base]
        mov ecx, HB_SIZE/4
        xor eax, eax
        a32 rep stosd
        mov ax, ds
        mov es, ax
        mov di, HB_IDT
        mov ax, idt_stubs
        mov cx, 256
.idt:
        mov [ss:di], ax
        mov word [ss:di+2], SEL_HCODE
        mov word [ss:di+4], 0xEE00  ; puerta de interrupción de 32 bits, DPL 3
        mov word [ss:di+6], 0
        add ax, 4
        add di, 8
        loop .idt
.ready:
        mov [e_ss], dx
        mov [e_sp], si
        movzx ebx, dx
        shl ebx, 4
        movzx esi, si
        add ebx, esi
        mov [e_blk], ebx            ; registros guardados en la pila de modo real
        ; CS
        mov cx, 1
        call ldt_alloc
        mov [e_cssel], ax
        mov ebx, [e_blk]
        movzx eax, word [fs:ebx+40]
        shl eax, 4
        mov ecx, 0xFFFF
        mov dx, 0x00FA
        call desc_set
        ; SS
        mov cx, 1
        call ldt_alloc
        mov [e_sssel], ax
        movzx eax, word [e_ss]
        shl eax, 4
        mov ecx, 0xFFFF
        mov dx, 0x00F2
        call desc_set
        ; DS (el mismo selector si coincide con SS)
        mov ebx, [e_blk]
        mov ax, [fs:ebx+2]
        cmp ax, [e_ss]
        mov ax, [e_sssel]
        je .dsok
        mov cx, 1
        call ldt_alloc
        push ax
        mov ebx, [e_blk]
        movzx eax, word [fs:ebx+2]
        shl eax, 4
        mov ecx, 0xFFFF
        mov dx, 0x00F2
        call desc_set
        pop ax
.dsok:
        mov [e_dssel], ax
        ; ES = PSP
        mov cx, 1
        call ldt_alloc
        mov [e_essel], ax
        movzx eax, word [cl_psp]
        shl eax, 4
        mov ecx, 0xFF
        mov dx, 0x00F2
        call desc_set
        ; entorno: PSP:2Ch pasa a ser un selector
        cmp word [cl_env], 0
        je .noenv
        mov cx, 1
        call ldt_alloc
        push ax
        movzx eax, word [cl_env]
        shl eax, 4
        mov ecx, 0xFFFF
        mov dx, 0x00F2
        call desc_set
        pop ax
        movzx ebx, word [cl_psp]
        shl ebx, 4
        mov [fs:ebx+0x2C], ax
.noenv:
        ; marco de vuelta al cliente
        mov ebx, [e_blk]
        movzx eax, word [e_sssel]
        push eax
        movzx eax, word [e_sp]
        add ax, 42
        push eax
        movzx eax, word [fs:ebx+36]
        and ax, 0x0FD4
        or ax, 0x3002
        push eax
        movzx eax, word [e_cssel]
        push eax
        movzx eax, word [fs:ebx+38]
        push eax
        push dword 0
        push dword [fs:ebx+32]
        push dword [fs:ebx+28]
        push dword [fs:ebx+24]
        push dword [fs:ebx+20]
        push dword 0
        push dword [fs:ebx+12]
        push dword [fs:ebx+8]
        push dword [fs:ebx+4]
        push word [e_dssel]
        push word [e_essel]
        push word 0
        push word 0
        mov bp, sp
        jmp frame_exit

; ---- pedir y soltar el bloque del servidor (modo real, DS = servidor) ----
hb_alloc:
        mov dx, HB_KB
        mov ah, 0x09
        call far [xms]
        cmp ax, 1
        jne .f
        mov [hb_handle], dx
        mov ah, 0x0C
        call far [xms]
        cmp ax, 1
        jne .f2
        mov [hb_base], bx
        mov [hb_base+2], dx
        mov ah, 0x05                ; A20 abierta mientras haya clientes
        call far [xms]
        cmp ax, 1
        jne .f3
        mov eax, [hb_base]
        mov di, gdt+SEL_HB
        call gdt_base
        mov [pm_idtr+2], eax
        add eax, HB_LPM
        mov di, gdt+(SEL_LPM & ~7)
        call gdt_base
        mov eax, [hb_base]
        add eax, HB_LDT
        mov di, gdt+SEL_LDTA
        call gdt_base
        mov di, gdt+SEL_LDT
        call gdt_base
        mov dword [tss+4], HB_R0TOP
        mov dword [lpm_free], LPM_SIZE
        mov word [rm_free], rm_stack_top
        mov dword [pool_total], 0
        mov byte [hb_fresh], 1
        clc
        ret
.f3:
        mov dx, [hb_handle]
        mov ah, 0x0D
        call far [xms]
.f2:
        mov dx, [hb_handle]
        mov ah, 0x0A
        call far [xms]
.f:
        stc
        ret

hb_free:
        mov ah, 0x06
        call far [xms]
        mov dx, [hb_handle]
        mov ah, 0x0D
        call far [xms]
        mov dx, [hb_handle]
        mov ah, 0x0A
        call far [xms]
        ret

; DI -> descriptor de la GDT, EAX = base (se conserva EAX)
gdt_base:
        push eax
        mov [di+2], ax
        shr eax, 16
        mov [di+4], al
        mov [di+7], ah
        pop eax
        ret

; la pila bloqueada es de 32 bits para los clientes de 32 bits
set_lpm_bits:
        mov byte [gdt+(SEL_LPM & ~7)+6], 0
        cmp byte [is32], 0
        je .x
        mov byte [gdt+(SEL_LPM & ~7)+6], 0x40
.x:
        ret

; ---- fin de un cliente: DOS salta aquí (dirección de terminación del PSP)
;      con los registros y la pila del padre. Se conserva todo. ----
term_stub:
        pushf
        cli
        cmp byte [cs:level], 0
        je .stale
        mov [cs:t_ss], ss
        mov [cs:t_sp], sp
        ; a la pila de modo real del servidor, por donde la dejó el cliente
        ; anterior (lo que usara el que termina ya no vale)
        mov [cs:t_es], es
        mov es, [cs:cl_save]
        mov ss, [cs:hostseg]
        mov sp, [es:rm_free-state_begin]
        mov es, [cs:t_es]
        pushad
        push ds
        push es
        push fs
        push gs
        push cs
        pop ds
        cld
        ; vectores de modo real: dejarlos como los tenía el cliente anterior
        xor ax, ax
        mov fs, ax
        mov es, [cl_save]
        xor bx, bx
.hk:
        mov al, [hookon+bx]
        cmp al, [es:(hookon-state_begin)+bx]
        je .hn
        test al, al
        jz .hi
        call hook_remove
        jmp .hn
.hi:
        call hook_install
.hn:
        inc bx
        cmp bx, NHOOK
        jb .hk
        ; el estado del cliente anterior vuelve ya (sus vectores de modo
        ; protegido, antes de que llegue otra interrupción); la pila de modo
        ; real sigue siendo esta hasta el final
        mov eax, [cl_term]
        mov [t_term], eax
        push ds
        mov ds, [cl_save]
        xor si, si
        mov di, state_begin
        mov cx, STATE_LEN
        push cs
        pop es
        rep movsb
        pop ds
        mov eax, [st_esp0]
        mov [tss+4], eax
        call set_lpm_bits
        mov ax, [rm_free]
        mov [t_rmfree], ax
        mov [rm_free], sp
        ; descriptores, memoria y callbacks del que ha terminado
        mov dx, ss
        mov si, sp
        mov bx, .pm
        jmp go_pm
.pm:
        push dx
        push si
        xor bp, bp
        call cleanup
        pop si
        pop dx
        mov bx, .rm
        jmp go_rm
.rm:
        mov ax, [t_rmfree]
        mov [rm_free], ax
        dec byte [level]
        xor ax, ax
        mov es, ax
        mov eax, [t_term]
        mov [es:0x22*4], eax        ; INT 22h, como lo habría dejado DOS
        cmp byte [level], 0
        jne .out
        call hb_free
.out:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        mov ss, [cs:t_ss]
        mov sp, [cs:t_sp]
.stale:
        popf
        jmp far [cs:t_term]

; =============================================================================
;  Conmutación de modo. Siempre con las interrupciones cerradas.
; =============================================================================
; -> modo real. BX = continuación, DX:SI = pila. Allí DS = ES = servidor.
go_rm:
        cmp byte [cs:vcpi], 0
        jne go_rm_vcpi
        mov ax, SEL_HDATA
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov eax, cr0
        and al, 0xFE
        mov cr0, eax
        db 0xEA                     ; jmp far servidor:.rm
        dw go_rm_real
go_rm_seg dw 0
go_rm_real:
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, dx
        mov sp, si
        o32 lidt [rm_idtr]
        xor ax, ax
        mov fs, ax
        mov gs, ax
        jmp bx
; por VCPI: en una pila del primer mega, el marco del modo V86 (EIP, CS,
; EFLAGS, ESP, SS, ES, DS, FS, GS) y llamada lejana al servidor
go_rm_vcpi:
        mov ax, SEL_HDATA
        mov ds, ax
        mov ss, ax
        mov esp, vcpi_stk
        movzx eax, word [go_rm_seg]
        push dword 0                ; GS
        push dword 0                ; FS
        push eax                    ; DS
        push eax                    ; ES
        movzx edx, dx
        push edx                    ; SS
        movzx esi, si
        push esi                    ; ESP
        push dword 0                ; EFLAGS: los pone el servidor
        push eax                    ; CS
        movzx ebx, bx
        push ebx                    ; EIP
        mov ax, SEL_FLAT
        mov ds, ax
        mov ax, 0xDE0C
        call dword far [cs:vcpi_entry]

; ¿está abierta la A20? ZF = 1 si parece cerrada (0000:0000 = FFFF:0010)
a20_test:
        push ds
        push es
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov eax, [0]
        cmp eax, [es:0x10]
        pop es
        pop ds
        ret

; abrir la A20 directamente: puerto 92h y, si no basta, el controlador del
; teclado. (El gestor XMS la cree abierta: su cuenta no cambia.)
a20_force:
        in al, 0x92
        test al, 2
        jnz .kbc
        or al, 2
        and al, 0xFE
        out 0x92, al
        call a20_test
        jne .x
.kbc:
        call .wait
        mov al, 0xD1
        out 0x64, al
        call .wait
        mov al, 0xDF
        out 0x60, al
        call .wait
        mov al, 0xFF
        out 0x64, al
        call .wait
.x:
        ret
.wait:
        push cx
        xor cx, cx
.w:
        in al, 0x64
        test al, 2
        loopnz .w
        pop cx
        ret

; -> modo protegido (anillo 0). BX = continuación. Conserva CX, DX, SI, DI, BP.
;    Allí DS = ES = datos, FS = plano, GS = LDT, SS:SP = pila de anillo 0.
;    Si alguien ha cerrado la A20 a espaldas del XMS, se vuelve a abrir
;    (el bloque del servidor y la memoria de los clientes están encima de 1 MB).
go_pm:
        call a20_test
        jne .a20
        call a20_force
.a20:
        cmp byte [cs:vcpi], 0
        je .raw
        mov [cs:vsw_esi], esi       ; (el servidor no conserva EAX ni ESI)
        mov esi, [cs:vsw_lin]
        mov ax, 0xDE0C
        int 0x67
; llegada por VCPI: GDT, IDT, LDT y TSS ya están cargadas; la paginación es
; la identidad (páginas de 4 MB, o de 4 KB sin PSE), salvo el primer mega,
; que es el del servidor
.vpm:
        mov ax, SEL_HDATA
        mov ds, ax
        mov es, ax
        cmp byte [vpse], 0          ; (un 386 no tiene CR4)
        je .vpm2
        mov eax, cr4
        or al, 0x10                 ; PSE
        mov cr4, eax
.vpm2:
        mov esi, [vsw_esi]
        mov ax, SEL_HB
        mov ss, ax
        mov esp, [tss+4]
        mov ax, SEL_FLAT
        mov fs, ax
        mov ax, SEL_LDTA
        mov gs, ax
        jmp .flags
.raw:
        o32 lgdt [cs:gdtr]
        o32 lidt [cs:pm_idtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp SEL_HCODE:.pm
.pm:
        mov ax, SEL_HDATA
        mov ds, ax
        mov es, ax
        mov ax, SEL_HB
        mov ss, ax
        mov esp, [tss+4]
        mov ax, SEL_FLAT
        mov fs, ax
        mov ax, SEL_LDTA
        mov gs, ax
        mov ax, SEL_LDT
        lldt ax
        and byte [gdt+SEL_TSS+5], 0xFD      ; TSS libre otra vez
        mov ax, SEL_TSS
        ltr ax
.flags:
        pushfd
        and dword [esp], 0xFFFFBFFF         ; sin NT
        popfd
        cld
        jmp bx

; =============================================================================
;  Llamada a modo real desde el anillo 0
;  ES:EDI -> estructura de registros de modo real (la de DPMI, 32h bytes)
;  AL = 0 interrupción BL, 1 llamada lejana, 2 llamada lejana con marco IRET
;  CX = palabras que copiar de la pila del cliente; BP -> marco (o 0)
;  Devuelve CF = 1 si no queda pila de modo real. Conserva ES, EDI, BP.
; =============================================================================
rm_call:
        mov [rc_mode], al
        mov [rc_cx], cx
        test al, al
        jnz .far
        mov al, bl
        call rm_vector
        jmp .tg
.far:
        mov eax, [es:edi+0x2A]
.tg:
        mov [rc_target], eax
        push es
        push edi
        push bp
        push word [rm_free]
        push dword [lpm_free]
        test bp, bp
        jz .nolpm
        cmp word [bp+CF_SS], SEL_LPM
        jne .nolpm
        movzx eax, word [bp+CF_ESP]
        mov [lpm_free], eax         ; el cliente está en la pila bloqueada
.nolpm:
        mov dx, [es:edi+0x30]
        mov si, [es:edi+0x2E]
        mov ax, dx
        or ax, si
        jnz .own
        mov dx, [hostseg]
        mov si, [rm_free]
        cmp si, rm_stack+RM_CHUNK
        jb .nostack
        sub word [rm_free], RM_CHUNK
.own:
        movzx ebx, dx
        shl ebx, 4
        movzx esi, si
        mov cx, [rc_cx]
        jcxz .nop
        test bp, bp
        jz .nop
        push es
        push edi
        call client_stack
        sub si, cx
        sub si, cx
        push si
.cp:
        mov ax, [es:edi]
        mov [fs:ebx+esi], ax
        add edi, 2
        add si, 2
        loop .cp
        pop si
        pop edi
        pop es
.nop:
        mov ax, [es:edi+0x20]
        and ax, 0x0FD5
        cmp byte [rc_mode], 1
        je .nf
        sub si, 2
        mov [fs:ebx+esi], ax        ; banderas para el IRET del destino
        and ah, 0xFD                ; entra sin IF
.nf:
        and ah, 0xFE                ; y sin TF
        sub si, 4
        mov word [fs:ebx+esi], rm_ret
        mov cx, [hostseg]
        mov [fs:ebx+esi+2], cx
        sub si, 6
        mov ecx, [rc_target]
        mov [fs:ebx+esi], ecx
        mov [fs:ebx+esi+4], ax
        sub si, 32
        push si
        mov cx, 8
.rg:
        mov eax, [es:edi]
        mov [fs:ebx+esi], eax
        add edi, 4
        add si, 4
        loop .rg
        pop si
        sub edi, 32
        sub si, 8
        mov ax, [es:edi+0x28]
        mov [fs:ebx+esi], ax
        mov ax, [es:edi+0x26]
        mov [fs:ebx+esi+2], ax
        mov ax, [es:edi+0x22]
        mov [fs:ebx+esi+4], ax
        mov ax, [es:edi+0x24]
        mov [fs:ebx+esi+6], ax
        push dword [tss+4]
        movzx eax, sp
        mov [tss+4], eax
        mov bx, rm_launch
        jmp go_rm
.nostack:
        pop dword [lpm_free]
        pop word [rm_free]
        pop bp
        pop edi
        pop es
        stc
        ret

; (modo real) cargar los registros y saltar al destino
rm_launch:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        iret

; (modo real) el destino ha vuelto
rm_ret:
        pushf
        cli
        pushad
        push ds
        push es
        push fs
        push gs
        mov dx, ss
        mov si, sp
        mov bx, rm_back
        jmp go_pm

rm_back:
        pop dword [tss+4]
        pop dword [lpm_free]
        pop word [rm_free]
        pop bp
        pop edi
        pop es
        movzx ebx, dx
        shl ebx, 4
        movzx esi, si
        add ebx, esi
        xor si, si
.c:
        mov eax, [fs:ebx+esi+8]
        mov [es:edi+esi], eax
        add si, 4
        cmp si, 32
        jb .c
        mov ax, [fs:ebx+40]
        mov [es:edi+0x20], ax
        mov ax, [fs:ebx]
        mov [es:edi+0x28], ax
        mov ax, [fs:ebx+2]
        mov [es:edi+0x26], ax
        mov ax, [fs:ebx+4]
        mov [es:edi+0x22], ax
        mov ax, [fs:ebx+6]
        mov [es:edi+0x24], ax
        clc
        ret

; llamada a modo real con la estructura propia (hrm). AL = modo, BL = int.
; Conserva SI, DI, BP.
host_rm_call:
        mov dword [hrm+0x2E], 0
        push si
        push es
        push edi
        push ds
        pop es
        mov edi, hrm
        xor cx, cx
        call rm_call
        pop edi
        pop es
        pop si
        ret

; INT 21h con hrm. -> AX, CF como los deja DOS (CF también si no hay pila)
dos_int:
        mov word [hrm+0x20], 0x0202
        xor al, al
        mov bl, 0x21
        call host_rm_call
        jc .x
        mov ax, [hrm+0x1C]
        test byte [hrm+0x20], 1
        jz .x
        stc
.x:
        ret

; llamada al gestor XMS: AH función, EDX, EBX, ESI -> EAX, EBX, EDX
xms_call:
        mov [hrm+0x1C], eax
        mov [hrm+0x14], edx
        mov [hrm+0x10], ebx
        mov [hrm+0x04], esi
        mov eax, [xms]
        mov [hrm+0x2A], eax
        mov word [hrm+0x20], 0x0202
        mov ax, [hostseg]
        mov [hrm+0x24], ax
        mov [hrm+0x22], ax
        push cx
        mov al, 1
        call host_rm_call
        pop cx
        mov eax, [hrm+0x1C]
        mov ebx, [hrm+0x10]
        mov edx, [hrm+0x14]
        ret

; AL = vector -> EAX = dirección de modo real a la que hay que llamar
; (si lo tenemos enganchado, la anterior)
rm_vector:
        call hook_idx
        jc .ivt
        cmp byte [hookon+bx], 0
        je .ivt
        shl bx, 2
        mov eax, [rm_old+bx]
        ret
.ivt:
        movzx bx, al
        shl bx, 2
        mov eax, [fs:bx]
        ret

; AL = vector -> BX = índice en la tabla de vectores reflejados (CF si no)
hook_idx:
        mov bl, al
        sub bl, 8
        cmp bl, 8
        jb .ok
        mov bl, al
        sub bl, 0x70
        cmp bl, 8
        jb .ok8
        mov bl, 16
        cmp al, 0x1C
        je .ok
        inc bl
        cmp al, 0x23
        je .ok
        inc bl
        cmp al, 0x24
        je .ok
        stc
        ret
.ok8:
        add bl, 8
.ok:
        xor bh, bh
        clc
        ret

; enganchar / soltar el vector de modo real BX (índice). FS = base 0.
; Conservan BX.
hook_install:
        push bx
        push di
        movzx di, byte [hookvec+bx]
        shl di, 2
        shl bx, 2
        mov eax, [fs:di]
        mov [rm_old+bx], eax
        add bx, rmhook_stubs
        mov [fs:di], bx
        mov ax, [hostseg]
        mov [fs:di+2], ax
        pop di
        pop bx
        mov byte [hookon+bx], 1
        ret
hook_remove:
        push bx
        push di
        mov byte [hookon+bx], 0
        movzx di, byte [hookvec+bx]
        shl di, 2
        shl bx, 2
        mov ax, [hostseg]
        cmp [fs:di+2], ax
        jne .x                      ; otro lo enganchó después: se deja pasar
        lea ax, [bx+rmhook_stubs]
        cmp [fs:di], ax
        jne .x
        mov eax, [rm_old+bx]
        mov [fs:di], eax
.x:
        pop di
        pop bx
        ret

; =============================================================================
;  Modo real -> manejador del cliente (vectores enganchados)
; =============================================================================
rmhook_entry:
        push bx
        mov bx, sp
        mov bx, [ss:bx+2]
        sub bx, rmhook_stubs+3
        shr bx, 2
        cmp byte [cs:hookon+bx], 0
        je .pass
        cmp byte [cs:level], 0
        je .pass
        pop bx
        pushad
        push ds
        push es
        push fs
        push gs
        cli
        mov dx, ss
        mov si, sp
        mov bx, hook_pm
        jmp go_pm
.pass:
        shl bx, 2
        push ax
        mov ax, [cs:rm_old+bx+2]
        mov bx, [cs:rm_old+bx]
        push bp
        mov bp, sp
        xchg bx, [bp+4]
        mov [bp+6], ax
        pop bp
        pop ax
        retf

hook_pm:
        movzx ebx, dx
        shl ebx, 4
        movzx eax, si
        add ebx, eax                ; EBX -> GS FS ES DS, 8 registros, vuelta, IP CS FL
        mov ax, [fs:ebx+40]
        sub ax, rmhook_stubs+3
        shr ax, 2
        mov di, ax
        movzx di, byte [hookvec+di]
        push dx
        push si
        push di
        push word [rm_free]
        cmp dx, [hostseg]
        jne .s
        cmp si, [rm_free]
        jae .s
        mov [rm_free], si
.s:
        push dword [tss+4]
        movzx eax, sp
        mov [tss+4], eax
        imul di, di, 6
        add di, pmvec
        mov ecx, [lpm_free]
        cmp cx, 0x100
        jb lpm_overflow
        mov dx, R3_HOOKRET
        call lpm_iret_frame
        push dword SEL_LPM
        push ecx
        push dword 0x3002
        movzx eax, word [di+4]
        push eax
        push dword [di]
        push dword 0
        push dword [fs:ebx+36]
        push dword [fs:ebx+32]
        push dword [fs:ebx+28]
        push dword [fs:ebx+24]
        push dword 0
        push dword [fs:ebx+16]
        push dword [fs:ebx+12]
        push dword [fs:ebx+8]
        push dword 0                ; DS, ES
        push dword 0                ; FS, GS
        mov bp, sp
        jmp frame_exit

; ECX = cima de la pila bloqueada, DX = trampolín de vuelta: pone un marco
; IRET (banderas 3002h) y devuelve ECX = nuevo ESP. Usa SI.
lpm_iret_frame:
        cmp byte [is32], 0
        je .w
        sub cx, 12
        mov si, cx
        movzx edx, dx
        mov [ss:si+HB_LPM], edx
        mov dword [ss:si+HB_LPM+4], SEL_R3
        mov dword [ss:si+HB_LPM+8], 0x3002
        ret
.w:
        sub cx, 6
        mov si, cx
        mov [ss:si+HB_LPM], dx
        mov word [ss:si+HB_LPM+2], SEL_R3
        mov word [ss:si+HB_LPM+4], 0x3002
        ret

hook_ret:       ; BP -> marco; encima: ESP0 anterior, rm_free, vector, SI, DX
        mov ax, [bp+CF_SIZE+6]
        cmp al, 0x23
        jb .nc
        ; INT 23h/24h: los registros del manejador vuelven a modo real
        movzx ebx, word [bp+CF_SIZE+10]
        shl ebx, 4
        movzx eax, word [bp+CF_SIZE+8]
        add ebx, eax
        xor esi, esi
.c:
        mov eax, [bp+si+CF_EDI]
        mov [fs:ebx+esi+8], eax
        add si, 4
        cmp si, 32
        jb .c
.nc:
        lea sp, [bp+CF_SIZE]
        pop dword [tss+4]
        pop word [rm_free]
        pop ax
        pop si
        pop dx
        mov bx, .rm
        jmp go_rm
.rm:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        add sp, 2
        iret

; =============================================================================
;  Callbacks de modo real (0303h)
; =============================================================================
cb_entry:
        pushf
        cli
        pushad
        push ds
        push es
        push fs
        push gs
        mov dx, ss
        mov si, sp
        mov bx, cb_pm
        jmp go_pm

cb_pm:
        movzx ebx, dx
        shl ebx, 4
        movzx eax, si
        add ebx, eax                ; EBX -> GS FS ES DS, 8 registros, banderas, vuelta
        mov bp, [fs:ebx+42]
        sub bp, cb_stubs+3
        shl bp, 2
        add bp, cb_tab              ; BP -> callback
        mov es, [ds:bp+12]
        mov edi, [ds:bp+8]
        xor ecx, ecx
.c:
        mov eax, [fs:ebx+ecx+8]
        mov [es:edi+ecx], eax
        add cx, 4
        cmp cx, 32
        jb .c
        mov ax, [fs:ebx+40]
        mov [es:edi+0x20], ax
        mov ax, [fs:ebx+4]
        mov [es:edi+0x22], ax
        mov ax, [fs:ebx+6]
        mov [es:edi+0x24], ax
        mov ax, [fs:ebx+2]
        mov [es:edi+0x26], ax
        mov ax, [fs:ebx]
        mov [es:edi+0x28], ax
        mov ax, [fs:ebx+42]
        sub ax, 3
        mov [es:edi+0x2A], ax
        mov ax, [hostseg]
        mov [es:edi+0x2C], ax
        mov ax, si
        add ax, 44
        mov [es:edi+0x2E], ax
        mov [es:edi+0x30], dx
        push word [rm_free]
        cmp dx, [hostseg]
        jne .s
        cmp si, [rm_free]
        jae .s
        mov [rm_free], si
.s:
        push dword [gdt+(SEL_RMSTK & ~7)]
        push dword [gdt+(SEL_RMSTK & ~7)+4]
        movzx eax, dx
        shl eax, 4
        push di
        mov di, gdt+(SEL_RMSTK & ~7)
        call gdt_base
        pop di
        push dword [tss+4]
        movzx eax, sp
        mov [tss+4], eax
        movzx ebx, si
        add bx, 44                  ; ESI del procedimiento = SP de modo real
        mov ecx, [lpm_free]
        cmp cx, 0x100
        jb lpm_overflow
        mov dx, R3_CBRET
        call lpm_iret_frame
        push dword SEL_LPM
        push ecx
        push dword 0x3002
        movzx eax, word [ds:bp+6]
        push eax
        push dword [ds:bp+2]
        push dword 0
        push dword 0
        push dword 0
        push dword 0
        push dword 0
        push dword 0
        push dword 0
        push ebx
        push edi
        push word SEL_RMSTK
        push es
        push dword 0
        mov bp, sp
        push ds
        pop es
        jmp frame_exit

cb_ret:         ; BP -> marco; ES:(E)DI del cliente = registros de modo real
        call cli_esdi
        xor si, si
.c:
        mov ax, [es:edi]
        mov [xr+si], ax
        add edi, 2
        add si, 2
        cmp si, 0x32
        jb .c
        push ds
        pop es
        lea sp, [bp+CF_SIZE]
        pop dword [tss+4]
        pop dword [gdt+(SEL_RMSTK & ~7)+4]
        pop dword [gdt+(SEL_RMSTK & ~7)]
        pop word [rm_free]
        ; sigue en rm_jump

; saltar a modo real con todos los registros de xr (CS:IP, SS:SP, banderas)
rm_jump:
        movzx ebx, word [xr+0x30]
        shl ebx, 4
        movzx esi, word [xr+0x2E]
        sub si, 6
        mov [xr+0x2E], si
        mov eax, [xr+0x2A]
        mov [fs:ebx+esi], eax       ; IP, CS
        mov ax, [xr+0x20]
        and ax, 0x0FD5
        mov [fs:ebx+esi+4], ax
        mov dx, [hostseg]
        mov si, [rm_free]
        mov bx, .rm
        jmp go_rm
.rm:
        mov edi, [xr]
        mov esi, [xr+0x04]
        mov ebp, [xr+0x08]
        mov ebx, [xr+0x10]
        mov edx, [xr+0x14]
        mov ecx, [xr+0x18]
        mov es, [xr+0x22]
        mov fs, [xr+0x26]
        mov gs, [xr+0x28]
        mov ss, [xr+0x30]
        mov sp, [xr+0x2E]
        mov eax, [xr+0x1C]
        mov ds, [cs:xr+0x24]
        iret

; ---- conmutación en crudo (0306h) ----
; modo protegido -> real: AX = DS, CX = ES, DX = SS, BX = SP, SI = CS, DI = IP
raw_to_rm:
        mov eax, [bp+CF_EBP]
        mov [xr+0x08], eax
        mov ax, [bp+CF_EAX]
        mov [xr+0x24], ax
        mov ax, [bp+CF_ECX]
        mov [xr+0x22], ax
        mov ax, [bp+CF_EDX]
        mov [xr+0x30], ax
        mov ax, [bp+CF_EBX]
        mov [xr+0x2E], ax
        mov ax, [bp+CF_ESI]
        mov [xr+0x2C], ax
        mov ax, [bp+CF_EDI]
        mov [xr+0x2A], ax
        mov ax, [bp+CF_EFL]
        and ax, 0x0200
        mov [xr+0x20], ax
        xor ax, ax
        mov [xr+0x26], ax
        mov [xr+0x28], ax
        lea sp, [bp+CF_SIZE]
        jmp rm_jump

; modo real -> protegido (salto lejano): AX = DS, CX = ES, DX = SS,
; (E)BX = (E)SP, SI = CS, (E)DI = (E)IP
raw_rm2pm:
        pushf
        pop word [cs:raw_fl]
        cli
        mov [cs:raw_ax], ax
        mov [cs:raw_ebx], ebx
        mov bx, .pm
        jmp go_pm
.pm:
        movzx eax, dx
        push eax
        mov eax, [raw_ebx]
        cmp byte [is32], 0
        jne .a
        movzx eax, ax
        movzx edi, di
.a:
        push eax
        movzx eax, word [raw_fl]
        and ax, 0x0200
        or ax, 0x3002
        push eax
        movzx eax, si
        push eax
        push edi
        mov es, cx
        xor ax, ax
        mov fs, ax
        mov gs, ax
        mov ds, [raw_ax]
        iretd

; =============================================================================
;  Entrada desde el anillo 3 (todas las puertas de la IDT)
; =============================================================================
int_entry:
        sub sp, 2
        pushad
        push ds
        push es
        push fs
        push gs
        mov bp, sp
        mov ax, SEL_HDATA
        mov ds, ax
        mov es, ax
        mov ax, SEL_FLAT
        mov fs, ax
        mov ax, SEL_LDTA
        mov gs, ax
        cld
        movzx eax, word [bp+CF_VEC+2]
        sub ax, idt_stubs+3
        shr ax, 2
        mov [bp+CF_VEC], eax
        mov bx, [tss+4]
        sub bx, bp
        cmp bx, CF_SIZE
        je .noerr
        cmp bx, CF_SIZE+4
        jne ring0_fault
        ; ¿excepción con código de error?
        cmp al, 8
        je .err
        cmp al, 10
        jb host_fault
        cmp al, 14
        ja host_fault
.err:
        test byte [bp+CF_CS+4], 3
        jz host_fault
        mov edx, [bp+CF_EIP]
        mov [exc_err], edx
        mov cx, 11                  ; quitar el código de error del marco
        lea si, [bp+40]
.mv:
        mov edx, [ss:si]
        mov [ss:si+4], edx
        sub si, 4
        loop .mv
        add bp, 4
        mov sp, bp
        jmp exception
.noerr:
        test byte [bp+CF_CS], 3
        jz host_fault
        mov dword [exc_err], 0
        cmp al, 8
        jae interrupt
        cmp al, 2
        je interrupt
        jmp exception

; salir al anillo 3 con el marco de BP
frame_exit:
        mov sp, bp
fx_segs:
        pop gs
        pop fs
        pop es
        pop ds
fx_end:
        popad
        add sp, 4
        iretd

; fallo en el anillo 0. Si ha sido al cargar un registro de segmento del
; cliente en frame_exit (un descriptor que ya no vale), ese registro se deja
; a cero y se sigue; cualquier otro es un error del servidor o un puntero
; no válido en una llamada.
ring0_fault:
        cmp al, 11
        je .seg
        cmp al, 13
        jne .msg
.seg:
        cmp word [bp+52], SEL_HCODE
        jne .msg
        mov dx, [bp+48]             ; IP del fallo
        sub dx, fx_segs
        jb .msg
        cmp dx, fx_end-fx_segs
        jae .msg
        cmp dx, 5                   ; 0, 2, 4, 5 -> GS, FS, ES, DS
        jb .k
        inc dx
.k:
        lea bx, [bp+60]             ; SP en el momento del fallo
        mov word [ss:bx], 0
        sub bx, dx
        mov bp, bx
        jmp frame_exit
.msg:
        ; mensaje con la excepción y el IP del servidor donde ha ocurrido
        push ax
        mov di, msgbuf
        mov si, m_fault
        call cpy
        mov si, m_fault2
        call cpy
        pop ax
        push ax
        call hex_al
        mov si, m_fault3
        call cpy
        pop ax
        mov bx, [bp+44]             ; IP (sin código de error)
        cmp al, 8
        je .e
        cmp al, 10
        jb .ne
        cmp al, 14
        ja .ne
.e:
        mov bx, [bp+48]
.ne:
        mov ax, bx
        call hex_ax
        mov al, ')'
        stosb
        mov si, m_fin
        call cpy
        jmp kill_client

; ---- interrupción (software o IRQ) AL ----
interrupt:
        cmp al, 0x31
        jne .n
        cmp word [bp+CF_CS], SEL_R3
        je internal
.n:
        movzx bx, al
        imul si, bx, 6
        add si, pmvec
        cmp word [si+4], SEL_R3
        jne .client
        call default_int
        jmp frame_exit
.client:
        call hook_idx
        jc to_client_soft
        cmp bx, 16
        jae to_client_soft
        ; IRQ con manejador del cliente: en la pila bloqueada
        call get_lpm
        cmp byte [is32], 0
        je .h16
        sub cx, 12
        mov bx, cx
        mov dword [ss:bx+HB_LPM], R3_LPMRET
        mov dword [ss:bx+HB_LPM+4], SEL_R3
        mov eax, [bp+CF_EFL]
        mov [ss:bx+HB_LPM+8], eax
        jmp enter_locked
.h16:
        sub cx, 6
        mov bx, cx
        mov word [ss:bx+HB_LPM], R3_LPMRET
        mov word [ss:bx+HB_LPM+2], SEL_R3
        mov ax, [bp+CF_EFL]
        mov [ss:bx+HB_LPM+4], ax
        ; sigue

; entrar en SI -> (dd, dw) con la pila bloqueada en ECX, sin interrupciones;
; el marco actual queda guardado debajo
enter_locked:
        mov eax, [si]
        mov [t_off], eax
        mov ax, [si+4]
        mov [t_sel], ax
        mov [t_esp], ecx
        mov eax, [bp+CF_EFL]
        mov [t_efl], eax
        call nest_frame
        mov eax, [t_off]
        mov [bp+CF_EIP], eax
        movzx eax, word [t_sel]
        mov [bp+CF_CS], eax
        mov eax, [t_efl]
        and eax, 0xFFFFBCFF         ; sin NT, IF ni TF
        mov [bp+CF_EFL], eax
        mov eax, [t_esp]
        mov [bp+CF_ESP], eax
        mov dword [bp+CF_SS], SEL_LPM
        jmp frame_exit

; ECX = ESP para un manejador en la pila bloqueada
get_lpm:
        mov ecx, [lpm_free]
        cmp word [bp+CF_SS], SEL_LPM
        jne .x
        movzx ecx, word [bp+CF_ESP]
.x:
        cmp cx, 0x100
        jb lpm_overflow
        ret

; guardar el marco actual (su parte IRET se queda arriba, con el ESP0
; anterior debajo) y abrir otro igual más abajo. BP -> marco nuevo.
nest_frame:
        pop dx
        mov eax, [tss+4]
        mov [bp+CF_VEC], eax
        lea ax, [bp+CF_VEC]
        movzx eax, ax
        mov [tss+4], eax
        mov cx, 10
        mov si, bp
        lea di, [bp-24]
.l:
        mov eax, [ss:si]
        mov [ss:di], eax
        add si, 4
        add di, 4
        loop .l
        sub bp, 24
        mov sp, bp
        mov dword [bp+CF_VEC], 0
        jmp dx

; deshacer nest_frame: registros los de ahora, parte IRET la guardada
unnest_frame:
        pop dx
        mov eax, [bp+CF_SIZE]
        mov [tss+4], eax
        mov cx, 10
        lea si, [bp+36]
        lea di, [bp+36+24]
.l:
        mov eax, [ss:si]
        mov [ss:di], eax
        sub si, 4
        sub di, 4
        loop .l
        add bp, 24
        mov sp, bp
        jmp dx

; ES:EDI -> cima de la pila del cliente (cs_big = 1 si es de 32 bits)
client_stack:
        push eax
        mov ax, [bp+CF_SS]
        mov es, ax
        movzx eax, ax
        lar eax, eax
        mov edi, [bp+CF_ESP]
        mov byte [cs_big], 1
        test eax, 0x400000
        jnz .x
        mov byte [cs_big], 0
        movzx edi, di
.x:
        pop eax
        ret

set_client_esp:
        cmp byte [cs_big], 0
        je .w
        mov [bp+CF_ESP], edi
        ret
.w:
        mov [bp+CF_ESP], di
        ret

; interrupción de software con manejador del cliente: como una INT en su pila
to_client_soft:
        call client_stack
        cmp byte [is32], 0
        je .w
        sub edi, 12
        mov eax, [bp+CF_EIP]
        mov [es:edi], eax
        mov eax, [bp+CF_CS]
        mov [es:edi+4], eax
        mov eax, [bp+CF_EFL]
        mov [es:edi+8], eax
        jmp .set
.w:
        sub edi, 6
        mov ax, [bp+CF_EIP]
        mov [es:edi], ax
        mov ax, [bp+CF_CS]
        mov [es:edi+2], ax
        mov ax, [bp+CF_EFL]
        mov [es:edi+4], ax
.set:
        call set_client_esp
        mov eax, [si]
        mov [bp+CF_EIP], eax
        movzx eax, word [si+4]
        mov [bp+CF_CS], eax
        and dword [bp+CF_EFL], 0xFFFFBEFF   ; sin NT ni TF
        jmp frame_exit

; ---- llamadas internas: INT 31h desde los trampolines de anillo 3 ----
internal:
        mov ax, [bp+CF_EIP]
        cmp ax, R3_EXC
        jbe chain_default
        cmp ax, R3_LPMRET
        jbe exc_chain
        cmp ax, R3_LPMRET+2
        je lpm_ret
        cmp ax, R3_EXCRET+2
        je exc_ret
        cmp ax, R3_CBRET+2
        je cb_ret
        cmp ax, R3_HOOKRET+2
        je hook_ret
        cmp ax, R3_RAWSW+2
        je raw_to_rm
        jmp host_fault

; el cliente ha encadenado con el manejador por defecto del vector
chain_default:
        sub ax, 2
        shr ax, 1
        push ax
        call client_stack
        cmp byte [is32], 0
        je .w
        mov eax, [es:edi]
        mov [bp+CF_EIP], eax
        movzx eax, word [es:edi+4]
        mov [bp+CF_CS], eax
        mov eax, [es:edi+8]
        add edi, 12
        jmp .f
.w:
        movzx eax, word [es:edi]
        mov [bp+CF_EIP], eax
        movzx eax, word [es:edi+2]
        mov [bp+CF_CS], eax
        movzx eax, word [es:edi+4]
        add edi, 6
.f:
        and eax, 0x0FD5
        or eax, 0x3002
        mov [bp+CF_EFL], eax
        call set_client_esp
        push ds
        pop es
        pop ax
        call default_int
        jmp frame_exit

lpm_ret:
        call unnest_frame
        jmp frame_exit

; ---- acción por defecto de la interrupción AL ----
default_int:
        cmp al, 0x31
        je dpmi
        cmp al, 0x21
        jne .n21
        mov ah, [bp+CF_EAX+1]
        cmp ah, 0x4C
        je .term
        test ah, ah
        jnz reflect
.term:
        call restore_env
        jmp reflect
.n21:
        cmp al, 0x20
        je .term
        cmp al, 0x2F
        jne .n2f
        cmp word [bp+CF_EAX], 0x1686
        jne reflect
        mov word [bp+CF_EAX], 0     ; estamos en modo protegido
        ret
.n2f:
        cmp al, 8
        jae reflect
        cmp al, 5
        jae kill_exc
        test al, al
        jz kill_exc
        ; sigue: 1, 2, 3 y 4 van a modo real

; reflejar la interrupción AL a modo real con los registros del cliente
reflect:
        mov bl, al
        xor si, si
.c:
        mov eax, [bp+si+CF_EDI]
        mov [hrm+si], eax
        add si, 4
        cmp si, 32
        jb .c
        mov ax, [bp+CF_EFL]
        mov [hrm+0x20], ax
        xor eax, eax
        mov [hrm+0x22], eax
        mov [hrm+0x26], eax
        xor al, al
        call host_rm_call
        jc rm_overflow
        xor si, si
.b:
        mov eax, [hrm+si]
        mov [bp+si+CF_EDI], eax
        add si, 4
        cmp si, 32
        jb .b
        mov ax, [hrm+0x20]
        and ax, 0x08D5
        and word [bp+CF_EFL], 0xF72A
        or [bp+CF_EFL], ax
        ret

; el entorno del PSP vuelve a ser un segmento antes de terminar
restore_env:
        push ax
        movzx ebx, word [cl_psp]
        shl ebx, 4
        mov ax, [cl_env]
        mov [fs:ebx+0x2C], ax
        pop ax
        ret

; =============================================================================
;  Excepciones
; =============================================================================
exception:      ; BP -> marco, [exc_err]
        movzx bx, byte [bp+CF_VEC]
        imul si, bx, 6
        add si, excvec
        cmp word [si+4], SEL_R3
        je .def
        call get_lpm
        cmp byte [is32], 0
        je .w
        sub cx, 32
        mov bx, cx
        mov dword [ss:bx+HB_LPM], R3_EXCRET
        mov dword [ss:bx+HB_LPM+4], SEL_R3
        mov eax, [exc_err]
        mov [ss:bx+HB_LPM+8], eax
        mov eax, [bp+CF_EIP]
        mov [ss:bx+HB_LPM+12], eax
        mov eax, [bp+CF_CS]
        mov [ss:bx+HB_LPM+16], eax
        mov eax, [bp+CF_EFL]
        mov [ss:bx+HB_LPM+20], eax
        mov eax, [bp+CF_ESP]
        mov [ss:bx+HB_LPM+24], eax
        mov eax, [bp+CF_SS]
        mov [ss:bx+HB_LPM+28], eax
        jmp enter_locked
.w:
        sub cx, 16
        mov bx, cx
        mov word [ss:bx+HB_LPM], R3_EXCRET
        mov word [ss:bx+HB_LPM+2], SEL_R3
        mov ax, [exc_err]
        mov [ss:bx+HB_LPM+4], ax
        mov ax, [bp+CF_EIP]
        mov [ss:bx+HB_LPM+6], ax
        mov ax, [bp+CF_CS]
        mov [ss:bx+HB_LPM+8], ax
        mov ax, [bp+CF_EFL]
        mov [ss:bx+HB_LPM+10], ax
        mov ax, [bp+CF_ESP]
        mov [ss:bx+HB_LPM+12], ax
        mov ax, [bp+CF_SS]
        mov [ss:bx+HB_LPM+14], ax
        jmp enter_locked
.def:
        mov al, bl
        cmp al, 8
        jae kill_exc
        ; 0 a 7 sin manejador de excepción: como interrupción
        imul si, bx, 6
        add si, pmvec
        cmp word [si+4], SEL_R3
        jne to_client_soft
        call default_int
        jmp frame_exit

; vuelta del manejador de excepción del cliente (RETF): el marco que deja
; en su pila dice dónde seguir
exc_ret:
        call client_stack
        cmp byte [is32], 0
        je .w
        mov eax, [es:edi+4]
        mov [t_off], eax
        mov ax, [es:edi+8]
        mov [t_sel], ax
        mov eax, [es:edi+12]
        mov [t_efl], eax
        mov eax, [es:edi+16]
        mov [t_esp], eax
        mov ax, [es:edi+20]
        jmp .f
.w:
        movzx eax, word [es:edi+2]
        mov [t_off], eax
        mov ax, [es:edi+4]
        mov [t_sel], ax
        movzx eax, word [es:edi+6]
        mov [t_efl], eax
        movzx eax, word [es:edi+8]
        mov [t_esp], eax
        mov ax, [es:edi+10]
.f:
        mov [t_ss], ax
        push ds
        pop es
        call unnest_frame
        mov eax, [t_off]
        mov [bp+CF_EIP], eax
        movzx eax, word [t_sel]
        mov [bp+CF_CS], eax
        mov eax, [t_efl]
        and eax, 0x0FD5
        or eax, 0x3002
        mov [bp+CF_EFL], eax
        mov eax, [t_esp]
        mov [bp+CF_ESP], eax
        movzx eax, word [t_ss]
        mov [bp+CF_SS], eax
        jmp frame_exit

; el cliente ha encadenado con el manejador de excepción por defecto
exc_chain:
        sub ax, R3_EXC+2
        shr ax, 1
        push ax
        call client_stack
        cmp byte [is32], 0
        je .w
        mov eax, [es:edi+8]
        mov [exc_err], eax
        mov eax, [es:edi+12]
        mov [bp+CF_EIP], eax
        movzx eax, word [es:edi+16]
        mov [bp+CF_CS], eax
        mov eax, [es:edi+24]
        mov [bp+CF_ESP], eax
        movzx eax, word [es:edi+28]
        mov [bp+CF_SS], eax
        jmp .f
.w:
        movzx eax, word [es:edi+4]
        mov [exc_err], eax
        movzx eax, word [es:edi+6]
        mov [bp+CF_EIP], eax
        movzx eax, word [es:edi+8]
        mov [bp+CF_CS], eax
        movzx eax, word [es:edi+12]
        mov [bp+CF_ESP], eax
        movzx eax, word [es:edi+14]
        mov [bp+CF_SS], eax
.f:
        push ds
        pop es
        pop ax
        ; sigue

; ---- excepción AL sin manejador: mensaje y fin del cliente ----
kill_exc:
        push ax
        mov di, msgbuf
        mov si, m_exc
        call cpy
        pop ax
        push ax
        call hex_al
        mov si, m_exc2
        call cpy
        pop bx
        xor bh, bh
        cmp bx, 14
        jbe .nm
        mov bx, 15
.nm:
        shl bx, 1
        mov si, [exc_names+bx]
        call cpy
        mov si, m_exc3
        call cpy
        mov ax, [bp+CF_CS]
        call hex_ax
        mov al, ':'
        stosb
        mov eax, [bp+CF_EIP]
        call hex_eax
        mov si, m_exc4
        call cpy
        mov ax, [exc_err]
        call hex_ax
        mov si, m_regs
        mov bx, kill_regs
.r:
        call cpy                    ; nombre del registro
        push si
        mov si, [bx]
        add bx, 2
        mov eax, [bp+si]
        call hex_eax
        pop si
        cmp bx, kill_regs+14
        jb .r
        call cpy
        mov eax, [bp+CF_ESP]
        cmp byte [is32], 0
        jne .e32
        movzx eax, ax
.e32:
        call hex_eax
        mov bx, kill_segs
.s:
        call cpy
        push si
        mov si, [bx]
        add bx, 2
        mov ax, [bp+si]
        call hex_ax
        pop si
        cmp bx, kill_segs+10
        jb .s
        mov si, m_fin
        call cpy
        jmp kill_client

lpm_overflow:
        mov si, m_lpm
        jmp kill_msg
rm_overflow:
        mov si, m_rmstk
        jmp kill_msg
host_fault:
        mov si, m_fault
kill_msg:
        mov di, msgbuf
        call cpy
        mov si, m_fin
        call cpy
        ; sigue

; terminar el cliente: mensaje (msgbuf) y INT 21h 4Ch en modo real. DOS
; vuelve por term_stub, que lo libera todo.
kill_client:
        mov sp, [tss+4]
        call restore_env
        mov dx, [hostseg]
        mov si, [rm_free]
        mov bx, .rm
        jmp go_rm
.rm:
        sti
        mov dx, msgbuf
        mov ah, 0x09
        int 0x21
        mov ax, 0x4CFF
        int 0x21

; copiar la cadena SI (acabada en 0) a ES:DI
cpy:
        lodsb
        test al, al
        jz .x
        stosb
        jmp cpy
.x:
        ret
hex_eax:
        push eax
        shr eax, 16
        call hex_ax
        pop eax
hex_ax:
        push ax
        mov al, ah
        call hex_al
        pop ax
hex_al:
        push ax
        shr al, 4
        call .n
        pop ax
.n:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .s
        add al, 7
.s:
        stosb
        ret

; =============================================================================
;  Descriptores (LDT)
; =============================================================================
; CX descriptores seguidos -> AX = primer selector, SI = su posición en la LDT
ldt_alloc:
        mov bx, LDT_FIRST
.s:
        mov dx, cx
        mov di, bx
.t:
        cmp di, LDT_N
        jae .fail
        cmp byte [ss:di+HB_OWN], 0
        jne .next
        inc di
        dec dx
        jnz .t
        mov al, [level]
        mov di, bx
        mov dx, cx
.m:
        mov [ss:di+HB_OWN], al
        mov si, di
        shl si, 3
        mov dword [gs:si], 0
        mov dword [gs:si+4], 0x0000F200     ; datos, presente, anillo 3
        inc di
        dec dx
        jnz .m
        mov si, bx
        shl si, 3
        mov ax, si
        or al, 7
        clc
        ret
.next:
        lea bx, [di+1]
        jmp .s
.fail:
        mov ax, 0x8011
        stc
        ret

; BX = selector -> SI = posición en la LDT, DI = índice (CF, AX = 8022h si no vale)
sel_check:
        test bl, 4
        jz .bad
        mov si, bx
        and si, 0xFFF8
        jz .bad
        mov di, si
        shr di, 3
        cmp byte [ss:di+HB_OWN], 0
        je .bad
        clc
        ret
.bad:
        mov ax, 0x8022
        stc
        ret

; SI = posición, EAX = base, ECX = límite, DL = byte de acceso, DH = banderas
desc_set:
        mov [gs:si+2], ax
        shr eax, 16
        mov [gs:si+4], al
        mov [gs:si+7], ah
        mov [gs:si+5], dl
        cmp ecx, 0xFFFFF
        jbe .b
        shr ecx, 12
        or dh, 0x80
.b:
        mov [gs:si], cx
        shr ecx, 16
        and cl, 0x0F
        or cl, dh
        mov [gs:si+6], cl
        ret

desc_base:
        mov ah, [gs:si+7]
        mov al, [gs:si+4]
        shl eax, 16
        mov ax, [gs:si+2]
        ret

desc_limit:
        movzx eax, byte [gs:si+6]
        and al, 0x0F
        shl eax, 16
        mov ax, [gs:si]
        test byte [gs:si+6], 0x80
        jz .x
        shl eax, 12
        or eax, 0xFFF
.x:
        ret

; SI = posición. El contenido se deja: si el cliente tiene todavía el selector
; en un registro de segmento, no falla al volver (se rehace al reservarlo).
desc_free:
        push di
        mov di, si
        shr di, 3
        mov byte [ss:di+HB_OWN], 0
        pop di
        ret

; ES:EDI del cliente (EDI de 16 bits si el cliente lo es)
cli_esdi:
        mov es, [bp+CF_ES]
        mov edi, [bp+CF_EDI]
        cmp byte [is32], 0
        jne .x
        movzx edi, di
.x:
        ret

; liberar lo del cliente actual (y de los que tuviera por encima)
cleanup:
        mov dl, [level]
        mov di, cb_tab
        mov cx, NCB
.c:
        cmp [di], dl
        jb .cn
        mov byte [di], 0
.cn:
        add di, 16
        loop .c
        mov si, HB_REG
        mov cx, NREG
.r:
        cmp byte [ss:si+8], 2
        jne .rn
        mov al, [level]
        cmp [ss:si+10], al
        jb .rn
        push si
        push cx
        call mem_free
        pop cx
        pop si
.rn:
        add si, 16
        loop .r
        mov di, 1
.l:
        mov al, [ss:di+HB_OWN]
        and al, 0x7F
        jz .ln
        cmp al, [level]
        jb .ln
        mov si, di
        shl si, 3
        mov dword [gs:si], 0
        mov dword [gs:si+4], 0
        mov byte [ss:di+HB_OWN], 0
.ln:
        inc di
        cmp di, LDT_N
        jb .l
        ret

; =============================================================================
;  Memoria extendida: regiones dentro de bloques XMS bloqueados
;  región: +0 base, +4 tamaño, +8 estado (0 nada, 1 libre, 2 en uso),
;          +9 bloque XMS, +10 dueño
;  bloque XMS: +0 handle, +2 en uso, +4 base, +8 tamaño
; =============================================================================
reg_new:        ; -> SI = región sin usar (CF si no quedan)
        mov si, HB_REG
        mov cx, NREG
.l:
        cmp byte [ss:si+8], 0
        je .ok
        add si, 16
        loop .l
        stc
        ret
.ok:
        clc
        ret

; EAX = bytes -> SI = región, EAX = dirección (CF, AX = error)
mem_alloc:
        add eax, 0xFFF
        jc .bad
        and eax, 0xFFFFF000
        jz .bad
        mov [ma_size], eax
.again:
        mov si, HB_REG
        mov cx, NREG
.l:
        cmp byte [ss:si+8], 1
        jne .n
        mov eax, [ss:si+4]
        cmp eax, [ma_size]
        jae .fit
.n:
        add si, 16
        loop .l
        call arena_new
        jnc .again
        mov ax, 0x8013
        stc
        ret
.bad:
        mov ax, 0x8021
        stc
        ret
.fit:
        sub eax, [ma_size]
        jz .take
        mov di, si
        push si
        call reg_new
        jc .nosplit
        mov [ss:si+4], eax
        mov eax, [ss:di]
        add eax, [ma_size]
        mov [ss:si], eax
        mov byte [ss:si+8], 1
        mov al, [ss:di+9]
        mov [ss:si+9], al
        mov eax, [ma_size]
        mov [ss:di+4], eax
.nosplit:
        pop si
.take:
        mov byte [ss:si+8], 2
        mov al, [level]
        mov [ss:si+10], al
        mov eax, [ss:si]
        clc
        ret

; pedir otro bloque a XMS (al menos [ma_size]) y apuntarlo como región libre
arena_new:
        mov di, HB_ARN
        mov cx, NARN
.l:
        cmp byte [ss:di+2], 0
        je .got
        add di, 16
        loop .l
        stc
        ret
.got:
        mov eax, [pool_total]
        shr eax, 1
        cmp eax, ARENA_MIN
        jae .a
        mov eax, ARENA_MIN
.a:
        cmp eax, [ma_size]
        jae .b
        mov eax, [ma_size]
.b:
        push dword [ma_size]
        push di
        push eax
        call arena_xms
        pop eax
        jnc .ok
        mov bx, sp
        mov eax, [ss:bx+2]          ; lo justo
        push eax
        call arena_xms
        pop eax
        jc .f2
.ok:
        pop di
        pop dword [ma_size]
        push eax
        call reg_new
        pop eax
        jc .noreg
        mov [ss:di], dx
        mov byte [ss:di+2], 1
        mov [ss:di+4], ebx
        mov [ss:di+8], eax
        add [pool_total], eax
        mov [ss:si], ebx
        mov [ss:si+4], eax
        mov byte [ss:si+8], 1
        mov ax, di
        sub ax, HB_ARN
        shr ax, 4
        mov [ss:si+9], al
        clc
        ret
.f2:
        pop di
        pop dword [ma_size]
        stc
        ret
.noreg:
        push dx
        mov ah, 0x0D
        call xms_call
        pop dx
        mov ah, 0x0A
        call xms_call
        stc
        ret

; EAX = bytes -> DX = handle, EBX = dirección (bloqueado). CF si no hay.
arena_xms:
        mov edx, eax
        shr edx, 10
        push edx
        mov ah, 0x89
        call xms_call
        pop ecx
        cmp ax, 1
        je .lk
        cmp bl, 0x80
        jne .f
        cmp ecx, 0xFFFF
        ja .f
        mov edx, ecx
        mov ah, 0x09
        call xms_call
        cmp ax, 1
        jne .f
.lk:
        push dx
        mov ah, 0x0C
        call xms_call
        pop cx
        cmp ax, 1
        jne .fl
        shl edx, 16
        mov dx, bx
        mov ebx, edx
        mov dx, cx
        clc
        ret
.fl:
        mov dx, cx
        mov ah, 0x0A
        call xms_call
.f:
        stc
        ret

; liberar la región SI (se une a las libres vecinas; si el bloque XMS queda
; vacío se devuelve)
mem_free:
        mov byte [ss:si+8], 1
        mov dl, [ss:si+9]
        mov eax, [ss:si]
        add eax, [ss:si+4]
        mov di, HB_REG
        mov cx, NREG
.n:
        cmp byte [ss:di+8], 1
        jne .nn
        cmp [ss:di+9], dl
        jne .nn
        cmp [ss:di], eax
        jne .nn
        mov eax, [ss:di+4]
        add [ss:si+4], eax
        mov byte [ss:di+8], 0
        jmp .p
.nn:
        add di, 16
        loop .n
.p:
        mov di, HB_REG
        mov cx, NREG
.q:
        cmp byte [ss:di+8], 1
        jne .qn
        cmp di, si
        je .qn
        cmp [ss:di+9], dl
        jne .qn
        mov eax, [ss:di]
        add eax, [ss:di+4]
        cmp eax, [ss:si]
        jne .qn
        mov eax, [ss:si+4]
        add [ss:di+4], eax
        mov byte [ss:si+8], 0
        mov si, di
        jmp .a
.qn:
        add di, 16
        loop .q
.a:
        movzx bx, dl
        shl bx, 4
        add bx, HB_ARN
        mov eax, [ss:si+4]
        cmp eax, [ss:bx+8]
        jne .done
        mov byte [ss:si+8], 0
        mov byte [ss:bx+2], 0
        sub [pool_total], eax
        mov dx, [ss:bx]
        push dx
        mov ah, 0x0D
        call xms_call
        pop dx
        mov ah, 0x0A
        call xms_call
.done:
        ret

; cambiar el tamaño de la región SI a EAX bytes -> SI, EAX = dirección
mem_resize:
        add eax, 0xFFF
        jc .bad
        and eax, 0xFFFFF000
        jz .bad
        cmp eax, [ss:si+4]
        je .same
        ja .grow
        ; encoger: lo que sobra pasa a ser una región libre
        mov edx, eax
        mov di, si
        push si
        call reg_new
        jc .keep
        mov eax, [ss:di]
        add eax, edx
        mov [ss:si], eax
        mov eax, [ss:di+4]
        sub eax, edx
        mov [ss:si+4], eax
        mov al, [ss:di+9]
        mov [ss:si+9], al
        mov [ss:di+4], edx
        call mem_free
.keep:
        pop si
.same:
        mov eax, [ss:si]
        clc
        ret
.bad:
        mov ax, 0x8021
        stc
        ret
.grow:
        mov [ma_size], eax
        ; ¿hay sitio libre justo detrás?
        mov edx, eax
        sub edx, [ss:si+4]          ; lo que falta
        mov eax, [ss:si]
        add eax, [ss:si+4]
        mov bl, [ss:si+9]
        mov di, HB_REG
        mov cx, NREG
.g:
        cmp byte [ss:di+8], 1
        jne .gn
        cmp [ss:di+9], bl
        jne .gn
        cmp [ss:di], eax
        jne .gn
        cmp [ss:di+4], edx
        jb .move
        je .all
        add [ss:di], edx
        sub [ss:di+4], edx
        jmp .ext
.all:
        mov byte [ss:di+8], 0
.ext:
        add [ss:si+4], edx
        jmp .same
.gn:
        add di, 16
        loop .g
.move:
        ; si la región es todo su bloque XMS, que lo agrande el gestor (no
        ; hace falta tener sitio para las dos a la vez si crece en el sitio)
        movzx bx, byte [ss:si+9]
        shl bx, 4
        add bx, HB_ARN
        mov eax, [ss:si+4]
        cmp eax, [ss:bx+8]
        jne .copy
        push dword [ma_size]
        push si
        push bx
        mov dx, [ss:bx]
        mov ah, 0x0D
        call xms_call               ; desbloquear
        mov di, sp
        mov bx, [ss:di]
        mov dx, [ss:bx]
        mov ebx, [ss:di+4]
        shr ebx, 10
        mov ah, 0x8F
        call xms_call               ; cambiar de tamaño
        push ax
        mov di, sp
        mov bx, [ss:di+2]
        mov dx, [ss:bx]
        mov ah, 0x0C
        call xms_call               ; y bloquear otra vez: DX:BX = dirección
        shl edx, 16
        mov dx, bx
        pop cx
        pop bx
        pop si
        pop dword [ma_size]
        mov [ss:bx+4], edx
        mov [ss:si], edx
        cmp cx, 1
        jne .copy
        mov eax, [ma_size]
        mov ecx, eax
        sub ecx, [ss:si+4]
        add [pool_total], ecx
        mov [ss:bx+8], eax
        mov [ss:si+4], eax
        jmp .same
.copy:
        ; otra región, copiar (con el movimiento de XMS) y soltar la vieja
        push si
        push dword [ss:si+4]
        mov eax, [ma_size]
        call mem_alloc
        pop ecx
        pop di
        jc .r
        mov [xmove], ecx
        movzx bx, byte [ss:di+9]
        shl bx, 4
        mov ax, [ss:bx+HB_ARN]
        mov [xmove+4], ax
        mov eax, [ss:di]
        sub eax, [ss:bx+HB_ARN+4]
        mov [xmove+6], eax
        movzx bx, byte [ss:si+9]
        shl bx, 4
        mov ax, [ss:bx+HB_ARN]
        mov [xmove+10], ax
        mov eax, [ss:si]
        sub eax, [ss:bx+HB_ARN+4]
        mov [xmove+12], eax
        push si
        push di
        mov esi, xmove
        mov ah, 0x0B
        call xms_call
        pop di
        pop si
        cmp ax, 1
        jne .mf
        push si
        mov si, di
        call mem_free
        pop si
        jmp .same
.mf:
        call mem_free
        mov ax, 0x8013
        stc
.r:
        ret

; =============================================================================
;  Servicios de INT 31h
; =============================================================================
dpmi:
        mov ax, [bp+CF_EAX]
        mov si, svc_tab
.l:
        cmp word [si], 0xFFFF
        je .unsup
        cmp ax, [si]
        je .go
        add si, 4
        jmp .l
.go:
        call [si+2]
        jc .err
        and byte [bp+CF_EFL], 0xFE
        ret
.unsup:
        mov ax, 0x8001
.err:
        mov [bp+CF_EAX], ax
        or byte [bp+CF_EFL], 1
        ret

svc_tab:
        dw 0x0000, s0000, 0x0001, s0001, 0x0002, s0002, 0x0003, s0003
        dw 0x0006, s0006, 0x0007, s0007, 0x0008, s0008, 0x0009, s0009
        dw 0x000A, s000A, 0x000B, s000B, 0x000C, s000C, 0x000D, s000D
        dw 0x0100, s0100, 0x0101, s0101, 0x0102, s0102
        dw 0x0200, s0200, 0x0201, s0201, 0x0202, s0202, 0x0203, s0203
        dw 0x0204, s0204, 0x0205, s0205
        dw 0x0300, s0300, 0x0301, s0301, 0x0302, s0302, 0x0303, s0303
        dw 0x0304, s0304, 0x0305, s0305, 0x0306, s0306
        dw 0x0400, s0400
        dw 0x0500, s0500, 0x0501, s0501, 0x0502, s0502, 0x0503, s0503
        dw 0x0600, s_ok, 0x0601, s_ok, 0x0602, s_ok, 0x0603, s_ok
        dw 0x0604, s0604, 0x0702, s_ok, 0x0703, s_ok
        dw 0x0800, s0800, 0x0801, s_ok
        dw 0x0900, s0900, 0x0901, s0901, 0x0902, s0902
        dw 0xFFFF

s_ok:
        clc
        ret

; ---- 0000h: reservar CX descriptores ----
s0000:
        mov cx, [bp+CF_ECX]
        test cx, cx
        jz s_bad
        call ldt_alloc
        jc .r
        mov [bp+CF_EAX], ax
.r:
        ret
s_bad:
        mov ax, 0x8021
        stc
        ret

; ---- 0001h: liberar el descriptor BX ----
s0001:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        call desc_free
        clc
.r:
        ret

; ---- 0002h: segmento de modo real BX -> descriptor ----
s0002:
        movzx edx, word [bp+CF_EBX]
        shl edx, 4
        mov di, 1
.l:
        test byte [ss:di+HB_OWN], 0x80
        jz .n
        mov si, di
        shl si, 3
        call desc_base
        cmp eax, edx
        je .found
.n:
        inc di
        cmp di, LDT_N
        jb .l
        mov cx, 1
        push edx
        call ldt_alloc
        pop edx
        jc .r
        push ax
        mov di, si
        shr di, 3
        or byte [ss:di+HB_OWN], 0x80
        mov eax, edx
        mov ecx, 0xFFFF
        mov dx, 0x00F2
        call desc_set
        pop ax
        mov [bp+CF_EAX], ax
        clc
.r:
        ret
.found:
        mov ax, si
        or al, 7
        mov [bp+CF_EAX], ax
        clc
        ret

; ---- 0003h: incremento entre selectores ----
s0003:
        mov word [bp+CF_EAX], 8
        clc
        ret

; ---- 0006h/0007h: base del segmento BX (CX:DX) ----
s0006:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        call desc_base
        mov [bp+CF_EDX], ax
        shr eax, 16
        mov [bp+CF_ECX], ax
        clc
.r:
        ret
s0007:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        mov ax, [bp+CF_EDX]
        mov [gs:si+2], ax
        mov ax, [bp+CF_ECX]
        mov [gs:si+4], al
        mov [gs:si+7], ah
        clc
.r:
        ret

; ---- 0008h: límite del segmento BX (CX:DX) ----
s0008:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        mov ax, [bp+CF_ECX]
        shl eax, 16
        mov ax, [bp+CF_EDX]
        and byte [gs:si+6], 0x70
        cmp eax, 0xFFFFF
        jbe .b
        mov edx, eax
        and dx, 0xFFF
        cmp dx, 0xFFF
        jne s_bad
        shr eax, 12
        or byte [gs:si+6], 0x80
.b:
        mov [gs:si], ax
        shr eax, 16
        or [gs:si+6], al
        clc
.r:
        ret

; ---- 0009h: derechos de acceso del segmento BX (CL, CH) ----
s0009:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        mov cx, [bp+CF_ECX]
        mov al, cl
        and al, 0x70
        cmp al, 0x70                ; segmento de código o datos de anillo 3
        jne s_bad
        mov [gs:si+5], cl
        cmp byte [is32], 0
        je .ok
        and ch, 0xD0
        and byte [gs:si+6], 0x0F
        or [gs:si+6], ch
.ok:
        clc
.r:
        ret

; ---- 000Ah: alias de datos del segmento BX ----
s000A:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        push si
        mov cx, 1
        call ldt_alloc
        pop di
        jc .r
        mov edx, [gs:di]
        mov [gs:si], edx
        mov edx, [gs:di+4]
        mov [gs:si+4], edx
        mov byte [gs:si+5], 0xF2
        mov [bp+CF_EAX], ax
        clc
.r:
        ret

; ---- 000Bh/000Ch: leer / escribir el descriptor BX en ES:(E)DI ----
s000B:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        call cli_esdi
        mov eax, [gs:si]
        mov [es:edi], eax
        mov eax, [gs:si+4]
        mov [es:edi+4], eax
        push ds
        pop es
        clc
.r:
        ret
s000C:
        mov bx, [bp+CF_EBX]
        call sel_check
        jc .r
        call cli_esdi
        mov eax, [es:edi]
        mov edx, [es:edi+4]
        push ds
        pop es
        mov cl, dh
        and cl, 0x70
        cmp cl, 0x70
        jne s_bad
        and edx, 0xFFDFFFFF
        mov [gs:si], eax
        mov [gs:si+4], edx
        clc
.r:
        ret

; ---- 000Dh: reservar el descriptor concreto BX (los 15 primeros) ----
s000D:
        mov bx, [bp+CF_EBX]
        test bl, 4
        jz .bad
        mov si, bx
        and si, 0xFFF8
        jz .bad
        mov di, si
        shr di, 3
        cmp di, LDT_FIRST
        jae .bad
        cmp byte [ss:di+HB_OWN], 0
        jne .used
        mov al, [level]
        mov [ss:di+HB_OWN], al
        mov dword [gs:si], 0
        mov dword [gs:si+4], 0x0000F200
        clc
        ret
.used:
        mov ax, 0x8011
        stc
        ret
.bad:
        mov ax, 0x8022
        stc
        ret

; ---- 0100h-0102h: memoria de DOS ----
; EAX = bytes -> CX = descriptores que hacen falta (uno por cada 64 KB)
dm_count:
        mov ecx, eax
        add ecx, 0xFFFF
        shr ecx, 16
        jnz .x
        inc cx
.x:
        ret
; SI = primer descriptor, EBX = base, EDX = bytes: el primero abarca todo
; el bloque; los siguientes, 64 KB cada uno
dm_set:
        mov ecx, edx
        test ecx, ecx
        jz .first
        dec ecx
.first:
        push edx
        push ebx
        mov eax, ebx
        mov dx, 0x00F2
        call desc_set
        pop ebx
        pop edx
.l:
        cmp edx, 0x10000
        jbe .x
        sub edx, 0x10000
        add ebx, 0x10000
        add si, 8
        mov ecx, edx
        cmp ecx, 0x10000
        jbe .c
        mov ecx, 0x10000
.c:
        dec ecx
        push edx
        push ebx
        mov eax, ebx
        mov dx, 0x00F2
        call desc_set
        pop ebx
        pop edx
        jmp .l
.x:
        ret

s0100:
        mov ax, [bp+CF_EBX]
        mov [hrm+0x10], ax
        mov byte [hrm+0x1D], 0x48
        call dos_int
        jnc .ok
        mov bx, [hrm+0x10]
        mov [bp+CF_EBX], bx
        stc
        ret
.ok:
        mov [dm_seg], ax
        movzx eax, word [bp+CF_EBX]
        shl eax, 4
        call dm_count
        push eax
        call ldt_alloc
        pop edx
        jc .nodesc
        mov [bp+CF_EDX], ax
        movzx ebx, word [dm_seg]
        shl ebx, 4
        call dm_set
        mov ax, [dm_seg]
        mov [bp+CF_EAX], ax
        clc
        ret
.nodesc:
        mov ax, [dm_seg]
        mov [hrm+0x22], ax
        mov byte [hrm+0x1D], 0x49
        call dos_int
        mov ax, 0x8011
        stc
        ret

s0101:
        mov bx, [bp+CF_EDX]
        call sel_check
        jc .r
        call desc_base
        shr eax, 4
        mov [hrm+0x22], ax
        mov byte [hrm+0x1D], 0x49
        call dos_int
        jc .r
        call desc_limit
        shr eax, 16
        inc ax
        mov cx, ax
.f:
        mov di, si
        shr di, 3
        cmp di, LDT_N
        jae .d
        cmp byte [ss:di+HB_OWN], 0
        je .d
        call desc_free
        add si, 8
        loop .f
.d:
        clc
.r:
        ret

s0102:
        mov bx, [bp+CF_EDX]
        call sel_check
        jc .r
        call desc_limit
        shr eax, 16
        inc ax
        mov [dm_n], ax              ; descriptores que tiene
        movzx eax, word [bp+CF_EBX]
        shl eax, 4
        call dm_count               ; CX = los que harán falta
        mov ax, [dm_n]
        cmp cx, ax
        jbe .dos
        ; los nuevos tienen que estar libres justo detrás
        push di
        add di, ax
        mov dx, cx
        sub dx, ax
.chk:
        cmp di, LDT_N
        jae .nofit
        cmp byte [ss:di+HB_OWN], 0
        jne .nofit
        inc di
        dec dx
        jnz .chk
        pop di
.dos:
        push si
        push di
        push cx
        call desc_base
        shr eax, 4
        mov [hrm+0x22], ax
        mov [dm_seg], ax
        mov ax, [bp+CF_EBX]
        mov [hrm+0x10], ax
        mov byte [hrm+0x1D], 0x4A
        push word [dm_n]
        push word [dm_seg]
        call dos_int
        pop word [dm_seg]
        pop word [dm_n]
        pop cx
        pop di
        pop si
        jnc .ok
        mov bx, [hrm+0x10]
        mov [bp+CF_EBX], bx
        stc
.r:
        ret
.nofit:
        pop di
        mov ax, 0x8011
        stc
        ret
.ok:
        ; marcar los nuevos y soltar los que sobran
        mov ax, [dm_n]
        cmp cx, ax
        je .set
        ja .more
        push si
        mov dx, ax
        sub dx, cx
        mov ax, cx
        shl ax, 3
        add si, ax
.fr:
        call desc_free
        add si, 8
        dec dx
        jnz .fr
        pop si
        jmp .set
.more:
        mov dx, cx
        sub dx, ax
        add di, ax
        mov al, [level]
.mk:
        mov [ss:di+HB_OWN], al
        inc di
        dec dx
        jnz .mk
.set:
        movzx edx, word [bp+CF_EBX]
        shl edx, 4
        movzx ebx, word [dm_seg]
        shl ebx, 4
        call dm_set
        clc
        ret

; ---- 0200h/0201h: vector de modo real BL (CX:DX) ----
s0200:
        mov al, [bp+CF_EBX]
        call rm_vector
        mov [bp+CF_EDX], ax
        shr eax, 16
        mov [bp+CF_ECX], ax
        clc
        ret
s0201:
        mov ax, [bp+CF_ECX]
        shl eax, 16
        mov ax, [bp+CF_EDX]
        mov edx, eax
        mov al, [bp+CF_EBX]
        call hook_idx
        jc .ivt
        cmp byte [hookon+bx], 0
        je .ivt
        shl bx, 2
        mov [rm_old+bx], edx
        clc
        ret
.ivt:
        movzx bx, al
        shl bx, 2
        mov [fs:bx], edx
        clc
        ret

; ---- 0202h-0205h: manejadores de excepción y vectores de modo protegido ----
s0202:
        mov bl, [bp+CF_EBX]
        cmp bl, 32
        jae s_bad
        movzx si, bl
        imul si, si, 6
        add si, excvec
        jmp getvec
s0204:
        movzx si, byte [bp+CF_EBX]
        imul si, si, 6
        add si, pmvec
getvec:
        mov eax, [si]
        mov [bp+CF_EDX], eax
        mov ax, [si+4]
        mov [bp+CF_ECX], ax
        clc
        ret
s0203:
        mov bl, [bp+CF_EBX]
        cmp bl, 32
        jae s_bad
        movzx si, bl
        imul si, si, 6
        add si, excvec
        ; sigue
setvec:
        mov eax, [bp+CF_EDX]
        cmp byte [is32], 0
        jne .a
        movzx eax, ax
.a:
        mov [si], eax
        mov ax, [bp+CF_ECX]
        mov [si+4], ax
        clc
        ret
s0205:
        movzx si, byte [bp+CF_EBX]
        imul si, si, 6
        add si, pmvec
        call setvec
        ; IRQ, 1Ch, 23h y 24h: si hay manejador, también llegan desde modo real
        mov al, [bp+CF_EBX]
        call hook_idx
        jc .ok
        cmp word [si+4], SEL_R3
        je .def
        cmp byte [hookon+bx], 0
        jne .ok
        call hook_install
        jmp .ok
.def:
        cmp byte [hookon+bx], 0
        je .ok
        call hook_remove
.ok:
        clc
        ret

; ---- 0300h-0302h: interrupción o llamada lejana de modo real ----
s0300:
        xor al, al
        jmp sim
s0301:
        mov al, 1
        jmp sim
s0302:
        mov al, 2
sim:
        call cli_esdi
        mov bl, [bp+CF_EBX]
        mov cx, [bp+CF_ECX]
        call rm_call
        push ds
        pop es
        jnc .r
        mov ax, 0x8012
.r:
        ret

; ---- 0303h/0304h: callbacks de modo real ----
s0303:
        mov di, cb_tab
        xor bx, bx
.l:
        cmp byte [di], 0
        je .got
        add di, 16
        inc bx
        cmp bx, NCB
        jb .l
        mov ax, 0x8015
        stc
        ret
.got:
        mov al, [level]
        mov [di], al
        mov eax, [bp+CF_ESI]
        mov edx, [bp+CF_EDI]
        cmp byte [is32], 0
        jne .a
        movzx eax, ax
        movzx edx, dx
.a:
        mov [di+2], eax
        mov [di+8], edx
        mov ax, [bp+CF_DS]
        mov [di+6], ax
        mov ax, [bp+CF_ES]
        mov [di+12], ax
        shl bx, 2
        add bx, cb_stubs
        mov [bp+CF_EDX], bx
        mov ax, [hostseg]
        mov [bp+CF_ECX], ax
        clc
        ret
s0304:
        mov ax, [bp+CF_ECX]
        cmp ax, [hostseg]
        jne .bad
        mov bx, [bp+CF_EDX]
        sub bx, cb_stubs
        jb .bad
        test bl, 3
        jnz .bad
        cmp bx, NCB*4
        jae .bad
        shl bx, 2
        cmp byte [cb_tab+bx], 0
        je .bad
        mov byte [cb_tab+bx], 0
        clc
        ret
.bad:
        mov ax, 0x8024
        stc
        ret

; ---- 0305h/0306h: guardar estado y conmutación en crudo ----
s0305:
        mov word [bp+CF_EAX], 0     ; no hay nada que guardar
        mov ax, [hostseg]
        mov [bp+CF_EBX], ax
        mov word [bp+CF_ECX], rm_save_stub
        mov word [bp+CF_ESI], SEL_R3
        mov eax, R3_SAVE16
        cmp byte [is32], 0
        je .a
        mov eax, R3_SAVE32
.a:
        mov [bp+CF_EDI], eax
        clc
        ret
s0306:
        mov ax, [hostseg]
        mov [bp+CF_EBX], ax
        mov word [bp+CF_ECX], raw_rm2pm
        mov word [bp+CF_ESI], SEL_R3
        mov dword [bp+CF_EDI], R3_RAWSW
        clc
        ret

; ---- 0400h: versión ----
s0400:
        mov word [bp+CF_EAX], 0x005A
        mov word [bp+CF_EBX], 0x0003        ; 32 bits, interrupciones en modo real
        mov al, [cputype]
        mov [bp+CF_ECX], al
        mov word [bp+CF_EDX], 0x0870        ; PIC: 08h y 70h
        clc
        ret

; ---- 0500h: memoria libre (ES:(E)DI -> 30h bytes) ----
s0500:
        mov ah, 0x88
        call xms_call
        cmp bl, 0x80
        jne .have
        mov ah, 0x08
        call xms_call
        movzx eax, ax
        movzx edx, dx
.have:
        cmp eax, 0x3FFFFF
        jbe .a
        mov eax, 0x3FFFFF
.a:
        cmp edx, 0x3FFFFF
        jbe .b
        mov edx, 0x3FFFFF
.b:
        shl eax, 10                 ; mayor bloque libre de XMS
        shl edx, 10                 ; total libre de XMS
        ; más lo que haya libre en nuestros bloques
        mov si, HB_REG
        mov cx, NREG
.l:
        cmp byte [ss:si+8], 1
        jne .n
        mov ebx, [ss:si+4]
        add edx, ebx
        jnc .nc
        mov edx, 0xFFFFF000
.nc:
        cmp ebx, eax
        jbe .n
        mov eax, ebx
.n:
        add si, 16
        loop .l
        call cli_esdi
        mov [es:edi], eax
        shr eax, 12
        mov [es:edi+0x04], eax
        mov [es:edi+0x08], eax
        shr edx, 12
        mov [es:edi+0x10], edx
        mov [es:edi+0x14], edx
        mov [es:edi+0x1C], edx
        mov eax, [pool_total]
        shr eax, 12
        add eax, edx
        mov [es:edi+0x0C], eax
        mov [es:edi+0x18], eax
        mov eax, 0xFFFFFFFF
        mov [es:edi+0x20], eax
        mov [es:edi+0x24], eax
        mov [es:edi+0x28], eax
        mov [es:edi+0x2C], eax
        push ds
        pop es
        clc
        ret

; ---- 0501h-0503h: bloques de memoria ----
s0501:
        mov ax, [bp+CF_EBX]
        shl eax, 16
        mov ax, [bp+CF_ECX]
        call mem_alloc
        jc ret_block.r
ret_block:
        mov [bp+CF_ECX], ax
        shr eax, 16
        mov [bp+CF_EBX], ax
        sub si, HB_REG
        shr si, 4
        mov [bp+CF_EDI], si
        mov word [bp+CF_ESI], HMAGIC
        clc
.r:
        ret
get_handle:
        cmp word [bp+CF_ESI], HMAGIC
        jne .bad
        mov si, [bp+CF_EDI]
        cmp si, NREG
        jae .bad
        shl si, 4
        add si, HB_REG
        cmp byte [ss:si+8], 2
        jne .bad
        clc
        ret
.bad:
        mov ax, 0x8023
        stc
        ret
s0502:
        call get_handle
        jc .r
        call mem_free
        clc
.r:
        ret
s0503:
        call get_handle
        jc .r
        mov ax, [bp+CF_EBX]
        shl eax, 16
        mov ax, [bp+CF_ECX]
        call mem_resize
        jnc ret_block
.r:
        ret

; ---- 0604h: tamaño de página ----
s0604:
        mov word [bp+CF_EBX], 0
        mov word [bp+CF_ECX], 0x1000
        clc
        ret

; ---- 0800h: dirección física -> lineal (siempre la misma: sin paginación,
;      o con la identidad de VCPI). Sin PSE, la memoria de fuera de la RAM
;      (marcos lineales de vídeo...) no tiene tablas: se sacan del fondo y se
;      quedan puestas (0801h no hace nada) ----
s0800:
        mov ax, [bp+CF_ESI]
        or ax, [bp+CF_EDI]
        jz s_bad
        cmp byte [vcpi], 0
        je .ok
        cmp byte [vpse], 0
        jne .ok
        mov ax, [bp+CF_EBX]
        shl eax, 16
        mov ax, [bp+CF_ECX]         ; EAX = dirección física
        mov dx, [bp+CF_ESI]
        shl edx, 16
        mov dx, [bp+CF_EDI]         ; EDX = tamaño
        dec edx
        add edx, eax                ; último octeto
        jc s_bad
        shr eax, 22                 ; entradas del directorio
        shr edx, 22
        xor ch, ch                  ; 1: hay que vaciar la TLB
        mov ebx, [vt_dir]
.l:
        cmp dword [fs:ebx+eax*4], 0
        jne .n
        mov edi, [vt_next]
        cmp edi, [vt_end]
        jae .full
        add dword [vt_next], 0x1000
        mov esi, eax
        shl esi, 22
        or si, 7                    ; identidad: presente, escritura, usuario
        push edi
        mov cl, 0                   ; 256 x 4 = 1024 entradas
.f:
%rep 4
        mov [fs:edi], esi
        add edi, 4
        add esi, 0x1000
%endrep
        dec cl
        jnz .f
        pop edi
        or di, 7
        mov [fs:ebx+eax*4], edi
        mov ch, 1
.n:
        inc eax
        cmp eax, edx
        jbe .l
        call .tlb
.ok:
        clc
        ret
.full:
        call .tlb
        mov ax, 0x8012              ; no queda espacio lineal
        stc
        ret
.tlb:
        test ch, ch
        jz .t
        mov esi, cr3
        mov cr3, esi
.t:
        ret

; ---- 0900h-0902h: interrupciones virtuales (aquí, las de verdad) ----
s0900:
        call s0902
        and byte [bp+CF_EFL+1], 0xFD
        clc
        ret
s0901:
        call s0902
        or byte [bp+CF_EFL+1], 0x02
        clc
        ret
s0902:
        mov al, [bp+CF_EFL+1]
        shr al, 1
        and al, 1
        mov [bp+CF_EAX], al
        clc
        ret

; ---- textos residentes ----
m_exc       db 13, 10, "HSPDPMI: excepción ", 0
m_exc2      db "h (", 0
m_exc3      db ") en ", 0
m_exc4      db ", código de error ", 0
m_regs      db 13, 10, "EAX=", 0, " EBX=", 0, " ECX=", 0, " EDX=", 0
            db 13, 10, "ESI=", 0, " EDI=", 0, " EBP=", 0, " ESP=", 0
            db 13, 10, "DS=", 0, " ES=", 0, " FS=", 0, " GS=", 0, " SS=", 0
m_fin       db 13, 10, "HSPDPMI: el programa se ha terminado.", 13, 10, "$", 0
m_fault     db 13, 10, "HSPDPMI: error interno o puntero no válido en una llamada al servidor", 0
m_fault2    db " (excepción ", 0
m_fault3    db "h, IP=", 0
m_lpm       db 13, 10, "HSPDPMI: pila de interrupciones agotada.", 0
m_rmstk     db 13, 10, "HSPDPMI: pila de modo real agotada.", 0
kill_regs   dw CF_EAX, CF_EBX, CF_ECX, CF_EDX, CF_ESI, CF_EDI, CF_EBP, CF_ESP
kill_segs   dw CF_DS, CF_ES, CF_FS, CF_GS, CF_SS
exc_names   dw n_e0, n_e1, n_e2, n_e3, n_e4, n_e5, n_e6, n_e7
            dw n_e8, n_e9, n_e10, n_e11, n_e12, n_e13, n_e14, n_ex
n_e0        db "división por cero", 0
n_e1        db "depuración", 0
n_e2        db "NMI", 0
n_e3        db "punto de ruptura", 0
n_e4        db "desbordamiento", 0
n_e5        db "fuera de límites", 0
n_e6        db "instrucción no válida", 0
n_e7        db "sin coprocesador", 0
n_e8        db "doble fallo", 0
n_e9        db "coprocesador", 0
n_e10       db "TSS no válida", 0
n_e11       db "segmento no presente", 0
n_e12       db "fallo de pila", 0
n_e13       db "fallo de protección general", 0
n_e14       db "fallo de página", 0
n_ex        db "desconocida", 0
msgbuf      times 420 db 0

resident_end:

; =============================================================================
;  Instalación
; =============================================================================
install:
        cld
        ; ¿/D? (descargar; sin el título, para quien lo llama desde un programa)
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .noarg
        cmp al, '/'
        jne .arg
        lodsb
        cmp al, 13
        je .noarg
        and al, 0xDF
        cmp al, 'D'
        je descargar
        jmp .arg
.noarg:
        mov dx, msg_title
        call print
        ; ¿hay una copia desactivada? Se activa de nuevo
        mov ax, 0xD600
        mov bx, 0x4844
        xor dl, dl
        int 0x2F
        push cs
        pop es
        cmp ax, 0x4450
        jne .nueva
        cmp bl, 1
        jne .nueva
        mov ax, 0xD600
        mov bx, 0x4844
        mov dl, 2
        int 0x2F
        push cs
        pop es
        mov dx, msg_react
        call print
        mov ax, 0x4C00
        int 0x21
.nueva:
        ; ¿386 o superior? (solo instrucciones del 8086 hasta saberlo)
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
        je .no386
        or ax, 0xF000
        push ax
        popf
        pushf
        pop ax
        test ax, 0xF000
        jnz .is386
.no386:
        mov dx, msg_no386
        jmp .fail
        cpu 586
.is386:
        ; ¿ya en modo protegido? (un EMM386 ajeno: haría falta VCPI)
        smsw ax
        test al, 1
        jz .real
        ; modo V86: solo con un servidor VCPI (HSPEMM)
        mov byte [vcpi], 1
        xor ax, ax
        mov es, ax
        mov eax, [es:0x67*4]
        push cs
        pop es
        test eax, eax
        jz .nov
        mov ax, 0xDE00
        int 0x67
        test ah, ah
        jz .real
.nov:
        mov dx, msg_v86
        jmp .fail
.real:
        ; 386, 486 o superior
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x40000            ; AC
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x40000
        jz .cpuok
        mov byte [cputype], 4
        mov eax, ecx
        xor eax, 0x200000           ; ID
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x200000
        jz .cpuok
        mov eax, 1
        cpuid
        shr ax, 8
        and al, 0x0F
        cmp al, 6
        jbe .fam
        mov al, 6
.fam:
        cmp al, 4
        jb .cpuok
        mov [cputype], al
.cpuok:
        ; ¿ya hay un servidor DPMI?
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz .nodpmi
        mov dx, msg_already
        jmp .fail
.nodpmi:
        push cs
        pop es
        ; gestor XMS
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        je .xms
        mov dx, msg_noxms
        jmp .fail
.xms:
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        ; tablas
        mov [hostseg], cs
        mov [go_rm_seg], cs
        mov word [rm_free], rm_stack_top
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov di, gdt+SEL_HCODE
        call gdt_base
        mov di, gdt+SEL_HDATA
        call gdt_base
        push eax
        add eax, gdt
        mov [gdtr+2], eax
        pop eax
        push eax
        add eax, tss
        mov di, gdt+SEL_TSS
        call gdt_base
        pop eax
        add eax, r3stubs
        mov di, gdt+(SEL_R3 & ~7)
        call gdt_base
        mov word [tss+8], SEL_HB    ; SS0
        mov word [tss+102], 104     ; sin mapa de E/S
        cmp byte [vcpi], 0
        je .novcpi
        call vcpi_setup
        jnc .novcpi
        jmp .fail
.novcpi:
        ; vectores por defecto: los trampolines de anillo 3
        mov di, pmvec
        xor eax, eax
.pv:
        mov [di], eax
        mov word [di+4], SEL_R3
        add di, 6
        add ax, 2
        cmp ax, 512
        jb .pv
        mov di, excvec
        mov ax, R3_EXC
.ev:
        mov [di], eax
        mov word [di+4], SEL_R3
        add di, 6
        add ax, 2
        cmp ax, R3_EXC+64
        jb .ev
        ; INT 2Fh
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        ; soltar el entorno y quedarse residente
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, msg_ok
        call print
        mov dx, resident_end + 15
        shr dx, 4
        cmp byte [vcpi], 0
        je .tsr
        mov dx, msg_vcpi
        call print
        mov dx, [vcpi_top]          ; con las tablas de páginas
.tsr:
        mov ax, 0x3100
        int 0x21
.fail:
        call print
        mov ax, 0x4C01
        int 0x21

; Preparar la conmutación por VCPI. El directorio y la primera tabla de
; páginas quedan residentes, tras el programa (alineados a 4 KB). La primera
; tabla la rellena el servidor (el primer mega y lo suyo); el resto del
; espacio lineal es la identidad: con PSE (Pentium), páginas de 4 MB; sin
; PSE (386, 486), tablas de 4 KB en memoria extendida (vt_build).
; CF y DX = mensaje si no se puede.
vcpi_setup:
        mov byte [vpse], 0
        cmp byte [cputype], 5
        jb .nopse
        mov eax, 1
        cpuid
        test dl, 0x08               ; PSE
        jz .nopse
        mov byte [vpse], 1
.nopse:
        mov ax, cs
        add ax, ((image_end - $$) + 0x200 + 15) >> 4
        add ax, 0xFF
        and ax, 0xFF00
        mov [vdir], ax
        add ax, 0x200
        mov bx, cs
        sub ax, bx
        mov [vcpi_top], ax          ; párrafos que quedan residentes
        ; a cero
        mov es, [vdir]
        xor di, di
        xor ax, ax
        mov cx, 0x1000
        rep stosw
        ; la tabla 0 y los descriptores, del servidor
        mov ax, [vdir]
        add ax, 0x100
        mov es, ax
        xor di, di
        mov si, gdt + SEL_VCPI
        mov ax, 0xDE01
        int 0x67
        mov dx, msg_v86
        test ah, ah
        jnz .err
        mov [vcpi_entry], ebx
        ; lo del servidor por encima de la HMA tiene que ser la identidad
        ; (lo es con HSPEMM cargado justo tras HSPXMS): si no, habría memoria
        ; física que aquí no se vería
        mov dx, msg_vorder
        mov bx, 0x110 * 4
.chk:
        cmp bx, di
        jae .fill
        mov eax, [es:bx]
        shr eax, 12
        shl eax, 2
        cmp ax, bx
        jne .err
        add bx, 4
        jmp .chk
.fill:
        ; el resto de la tabla 0: identidad
        movzx eax, di
        shl eax, 10                 ; entrada -> dirección
        or al, 7
.f0:
        cmp di, 0x1000
        jae .dir
        stosd
        add eax, 0x1000
        jmp .f0
.dir:
        push cs
        pop es
        cmp byte [vpse], 0
        jne .pse
        call vt_build               ; (usa la página del directorio de paso)
        jc .err
.pse:
        ; directorio: la tabla 0 y, con PSE, páginas de 4 MB
        mov cx, [vdir]
        add cx, 0x100
        shr cx, 8
        mov ax, 0xDE06
        int 0x67
        or dl, 7
        mov es, [vdir]
        mov [es:0], edx
        cmp byte [vpse], 0
        je .cr3
        mov di, 4
        mov eax, 0x00400087
.f1:
        stosd
        add eax, 0x00400000
        cmp di, 0x1000
        jb .f1
.cr3:
        mov cx, [vdir]
        shr cx, 8
        mov ax, 0xDE06
        int 0x67
        mov [vsw_cr3], edx
        push cs
        pop es
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, gdtr
        mov [vsw_gdtr], eax
        mov eax, ebx
        add eax, pm_idtr
        mov [vsw_idtr], eax
        add ebx, vsw_cr3
        mov [vsw_lin], ebx
        clc
        ret
.err:
        push cs
        pop es
        mov byte [vcpi], 0
        stc
        ret

; Sin PSE: tablas de páginas de 4 KB para la identidad de toda la RAM, en un
; bloque XMS bloqueado (más VT_EXTRA de reserva para 0800h). Se rellenan
; desde aquí con XMS 0Bh, usando de paso la página del directorio, y luego
; se deja el directorio apuntando a ellas (la entrada 0 la pone vcpi_setup).
; La memoria por encima de la HMA es la identidad también en HSPEMM (.chk),
; así que la dirección del bloque bloqueado es la física.
; CF y DX = mensaje si no se puede.
VT_EXTRA    equ 8                   ; 32 MB de memoria física de fuera
vt_build:
        call mem_top                ; EAX = fin de la RAM
        add eax, 0x3FFFFF
        jnc .nc
        mov eax, 0xFFFFFFFF
.nc:
        shr eax, 22                 ; entradas de 4 MB con RAM
        cmp ax, 1
        jae .n1
        mov ax, 1
.n1:
        mov [vt_n], ax
        ; KB del bloque: (n - 1 + VT_EXTRA) tablas, y 3 KB para alinear
        add ax, VT_EXTRA - 1
        shl ax, 2
        add ax, 3
        mov [vt_kb], ax
        mov dx, ax
        mov ah, 0x09
        call far [xms]
        test ax, ax
        jz .nomem
        mov [vt_handle], dx
        mov ah, 0x0C
        call far [xms]
        test ax, ax
        jz .nomem_free
        shl edx, 16
        mov dx, bx                  ; EDX = dirección física del bloque
        mov ebx, edx
        add ebx, 0xFFF
        and bx, 0xF000              ; EBX = primera tabla
        mov eax, ebx
        sub eax, edx
        mov [vt_off], eax           ; su desplazamiento dentro del bloque
        ; el bloque tiene que caer en la RAM que cubren las tablas
        movzx ecx, word [vt_kb]
        shl ecx, 10
        add ecx, edx
        dec ecx
        shr ecx, 22
        mov dx, msg_vtop
        cmp cx, [vt_n]
        jae .bad
        ; tabla a tabla: identidad de los 4 MB de la entrada i (1..n-1)
        mov word [vt_i], 1
.t:
        mov ax, [vt_i]
        cmp ax, [vt_n]
        jae .tdir
        movzx eax, ax
        shl eax, 22
        or al, 7
        mov es, [vdir]
        xor di, di
.te:
        stosd
        add eax, 0x1000
        cmp di, 0x1000
        jb .te
        push cs
        pop es
        mov dword [vt_mv], 0x1000   ; longitud
        mov word [vt_mv+4], 0       ; origen: memoria convencional
        mov word [vt_mv+6], 0
        mov ax, [vdir]
        mov [vt_mv+8], ax
        mov ax, [vt_handle]
        mov [vt_mv+10], ax          ; destino: el bloque
        movzx eax, word [vt_i]
        dec ax
        shl eax, 12
        add eax, [vt_off]
        mov [vt_mv+12], eax
        mov si, vt_mv
        mov ah, 0x0B
        call far [xms]
        mov dx, msg_vmove
        test ax, ax
        jz .bad
        inc word [vt_i]
        jmp .t
.tdir:
        ; directorio: a cero (la página ha servido de paso) y las entradas
        mov es, [vdir]
        xor di, di
        xor ax, ax
        mov cx, 0x800
        rep stosw
        mov di, 4
        mov eax, ebx
        or al, 7
        mov cx, [vt_n]
.d:
        dec cx
        jz .dfin
        stosd
        add eax, 0x1000
        jmp .d
.dfin:
        push cs
        pop es
        and al, 0xF8
        mov [vt_next], eax          ; el fondo, tras las de la RAM
        add eax, VT_EXTRA * 0x1000
        mov [vt_end], eax
        movzx eax, word [vdir]
        shl eax, 4
        mov [vt_dir], eax
        clc
        ret
.bad:
        push dx
        mov dx, [vt_handle]
        mov ah, 0x0D
        call far [xms]
        pop dx
.nomem_free:
        push dx
        mov dx, [vt_handle]
        mov ah, 0x0A
        call far [xms]
        pop dx
        mov word [vt_handle], 0
        cmp dx, msg_vtop
        je .e
        cmp dx, msg_vmove
        je .e
.nomem:
        ; no hay memoria extendida: decir cuánta hace falta
        mov dx, msg_vnomem
        call print
        mov ax, [vt_kb]
        call print_dec
        mov dx, msg_vnomem2
.e:
        push cs
        pop es
        stc
        ret

; -> EAX = dirección del fin de la RAM (lo mayor de INT 15h E801h y de lo
;    que diga el XMS: función 88h o, si no la tiene, 08h)
mem_top:
        mov dword [vt_top], 0
        mov ax, 0xE801
        xor bx, bx
        xor cx, cx
        xor dx, dx
        int 0x15
        jc .xms
        test ax, ax
        jnz .e8
        mov ax, cx
        mov bx, dx
.e8:
        movzx eax, ax
        shl eax, 10
        add eax, 0x100000           ; de 1 MB a 16 MB
        mov [vt_top], eax
        test bx, bx
        jz .xms
        movzx eax, bx
        shl eax, 16
        add eax, 0x1000000          ; encima de 16 MB
        cmp eax, [vt_top]
        jbe .xms
        mov [vt_top], eax
.xms:
        mov ah, 0x00
        call far [xms]
        cmp ah, 3
        jb .x08
        mov ah, 0x88
        xor bl, bl
        call far [xms]
        test bl, bl
        jz .x88
        cmp bl, 0xA0                ; (sin libre: ECX vale igual)
        jne .x08
.x88:
        inc ecx
        jmp .max
.x08:
        mov ah, 0x08
        call far [xms]
        movzx ecx, dx
        shl ecx, 10
        add ecx, 0x110000
.max:
        mov eax, [vt_top]
        cmp ecx, eax
        jbe .r
        mov eax, ecx
.r:
        ret

; AX -> en decimal por la salida estándar
print_dec:
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.p:
        pop dx
        add dl, '0'
        mov ah, 0x02
        int 0x21
        loop .p
        pop dx
        pop cx
        pop bx
        ret

            align 4
vt_off      dd 0
vt_top      dd 0
vt_mv       times 16 db 0           ; movimiento de XMS (0Bh)
vt_n        dw 0
vt_kb       dw 0
vt_i        dw 0
vdir        dw 0
vcpi_top    dw 0

; HSPDPMI /D: quita el servidor de la memoria (o, si no puede, lo desactiva)
descargar:
        mov ax, 0xD600
        mov bx, 0x4844
        mov dl, 1
        int 0x2F
        mov dx, msg_noinst
        cmp ax, 0x4450
        jne .mal
        mov dx, msg_ocupado
        cmp bl, 0xFF
        je .mal
        mov dx, msg_desact
        cmp bl, 2
        jne .bien
        mov ah, 0x49                ; ES = el segmento del residente
        int 0x21
        mov dx, msg_desc
.bien:
        push cs
        pop es
        call print
        mov ax, 0x4C00
        int 0x21
.mal:
        push cs
        pop es
        call print
        mov ax, 0x4C01
        int 0x21

print:
        mov ah, 0x09
        int 0x21
        ret

msg_title   db "HSPDPMI 0.1 - servidor DPMI 0.9 de Hispa-DOS", 13, 10, "$"
msg_react   db "HSPDPMI: estaba desactivado en memoria; activado de nuevo.", 13, 10, "$"
msg_noinst  db "HSPDPMI no está en memoria.", 13, 10, "$"
msg_ocupado db "HSPDPMI: no se puede descargar mientras haya programas usándolo.", 13, 10, "$"
msg_desc    db "HSPDPMI: descargado; la memoria queda libre.", 13, 10, "$"
msg_desact  db "HSPDPMI: desactivado (sigue en memoria: hay otro residente cargado después).", 13, 10, "$"
msg_ok      db "Instalado: clientes de 16 y de 32 bits, memoria de XMS.", 13, 10, "$"
msg_no386   db "HSPDPMI: hace falta un procesador 386 o superior.", 13, 10, "$"
msg_v86     db "HSPDPMI: el procesador ya está en modo protegido y no hay un servidor VCPI (HSPEMM); no se instala.", 13, 10, "$"
msg_vcpi    db "Con HSPEMM cargado: el modo protegido se pide por VCPI.", 13, 10, "$"
msg_vnomem  db "HSPDPMI: con HSPEMM (modo V86) y sin páginas de 4 MB, las tablas de páginas van en memoria extendida y no queda bastante: hacen falta $"
msg_vnomem2 db " KB de XMS; no se instala.", 13, 10, "$"
msg_vtop    db "HSPDPMI: no se ha podido medir la memoria (INT 15h E801h y XMS) para las tablas de páginas; no se instala.", 13, 10, "$"
msg_vmove   db "HSPDPMI: el gestor XMS no ha podido copiar las tablas de páginas; no se instala.", 13, 10, "$"
msg_vorder  db "HSPDPMI: cargue HSPEMM.SYS justo después de HSPXMS.SYS en CONFIG.SYS; no se instala.", 13, 10, "$"
msg_already db "HSPDPMI: ya hay un servidor DPMI instalado.", 13, 10, "$"
msg_noxms   db "HSPDPMI: hace falta un gestor XMS (HSPXMS.SYS en CONFIG.SYS).", 13, 10, "$"
image_end:
