; =============================================================================
;  HSPEMM.SYS - gestor de memoria expandida y superior de Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS (después de HSPXMS.SYS):
;       DEVICE=C:\HISPADOS\HSPEMM.SYS [KB] [NOEMS] [NOUMB] [FRAME=xxxx]
;                                     [I=xxxx-yyyy] [X=xxxx-yyyy]
;    KB           memoria (EMS y VCPI) que toma del gestor XMS; si no se dice,
;                 la mitad de la que haya libre (como mucho 32 MB)
;    NOEMS        sin marco de página: no hay EMS, pero sí VCPI y UMB
;    NOUMB        no ofrece memoria superior (UMB)
;    FRAME=xxxx   segmento del marco de página (D000, E000...)
;    I=xxxx-yyyy  incluir esa zona en la búsqueda de memoria superior libre
;                 (por omisión, C000-DFFF)
;    X=xxxx-yyyy  excluirla (una tarjeta con memoria propia, por ejemplo)
;
;  Qué hace (como EMM386 en MS-DOS), en un 386 o superior:
;   - Pone el procesador en modo protegido con paginación y deja DOS y los
;     programas en modo virtual 8086 (V86). El «monitor» (anillo 0) atiende
;     las excepciones: refleja las interrupciones a la tabla de vectores del
;     modo real, emula las instrucciones privilegiadas (HLT, MOV CRx/DRx...) y
;     los puertos que virtualiza (línea A20, registros de DMA).
;   - EMS 4.0 (INT 67h, funciones 40h-5Dh): marco de 64 KB en la zona superior
;     y páginas de 16 KB de la memoria extendida.
;   - VCPI 1.0 (INT 67h AX=DExxh) para los extensores de DOS (DOS/4GW...).
;   - UMB: la memoria superior libre se llena de RAM y se ofrece por las
;     funciones 10h-12h de XMS (se intercepta el punto de entrada de HSPXMS).
;   - INT 15h AH=87h (los movimientos de XMS) se hace en el monitor.
;   - Windows en modo 386 mejorado: al arrancar (INT 2Fh AX=1605h) HSPEMM le
;     entrega la rutina que apaga y enciende el modo V86 (como EMM386); si hay
;     memoria superior o EMS en uso no puede apagarse y lo avisa.
;
;  Escrito desde cero a partir de las especificaciones públicas: LIM EMS 4.0,
;  VCPI 1.0, XMS 3.0 y el manual del programador del Intel 386.
;
;  Mapa de la memoria lineal bajo el monitor:
;     000000-0FFFFF  el primer mega, tal cual (identidad), salvo las páginas
;                    de UMB y del marco EMS, que apuntan a RAM extendida
;     100000-10FFFF  la HMA (o un espejo del primer mega si A20 está cerrada)
;     110000-123FFF  el monitor: directorio, tablas de páginas, código y datos
;                    (solo anillo 0; va también en las tablas de los clientes
;                    VCPI, que comparten la primera tabla de páginas)
;     400000-...     ventanas temporales para copiar memoria física
; =============================================================================
        cpu 386
        bits 16

MONBASE     equ 0x110000            ; dirección lineal del monitor
SYS_PAGES   equ 3                   ; directorio, PT0 y PT1
MON_PAGES   equ 17                  ; código y datos del monitor
MONIMG      equ MONBASE + SYS_PAGES*4096
MONEND_PG   equ 0x110 + SYS_PAGES + MON_PAGES
PD_LIN      equ MONBASE
PT0_LIN     equ MONBASE + 0x1000
PT1_LIN     equ MONBASE + 0x2000
WIN_SRC     equ 0x400000            ; ventanas (PT1): origen y destino
WIN_DST     equ 0x420000
WIN_EMS1    equ 0x440000            ; una página EMS (función 57h)
WIN_EMS2    equ 0x450000

SEL_CODE    equ 0x08
SEL_DATA    equ 0x10
SEL_TSS     equ 0x18
SEL_C16     equ 0x20
SEL_D16     equ 0x28

MAXPOOL     equ 4096                ; páginas de 16 KB (64 MB)
NHAND       equ 64
NUMB        equ 8                   ; bloques de memoria superior

SVC_EMS     equ 0
SVC_I15     equ 1
SVC_REAL    equ 2
SVC_BUSY    equ 3

; llamada al monitor desde el modo V86: ARPL no existe en modo real/V86 y da
; «código no válido» (#UD), que el monitor reconoce si viene de este segmento
; dirección, en el fichero, de una variable de la imagen del monitor
%define MONV(x) (section..mon.start + (x - mon_start))

%macro MONCALL 1
        db 0x63, 0xC0 + %1
%endmacro

        section .text start=0

header:
        dw 0xFFFF, 0xFFFF
        dw 0x8000
        dw strategy
        dw interrupt
devname db "EMMXXXX0"               ; los programas buscan este nombre

req         dd 0
old15       dd 0
old2f       dd 0
xms_chain   dd 0
active      db 0                    ; 1 = en modo V86 con el monitor
winmsg      db 0
sw_ss       dw 0                    ; pila con la que se sigue tras cambiar
sw_sp       dw 0                    ; de modo
mon_cr3     dd 0
tmp_gdtr    dw 0x17
            dd 0
real_idtr   dw 0x3FF
            dd 0
tmp_gdt     dq 0
            dq 0x00CF9A000000FFFF
            dq 0x00CF92000000FFFF
real_jmp    dw real_cont
real_seg    dw 0
frame_seg   dw 0                    ; 0 = sin EMS
fat_code    db 0
fat_cs      dw 0
fat_ip      dw 0
; bloques de memoria superior: segmento, párrafos, estado (0 libre, 1 en uso,
; FFh entrada vacía)
umb_tab     times NUMB*5 db 0xFF
umb_given   dw 0                    ; bloques entregados

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        push ds
        push bx
        lds bx, [cs:req]
        cmp byte [bx+2], 0
        jne .other
        pop bx
        pop ds
        jmp init
.other:
        cmp byte [bx+2], 4          ; leer / escribir: nada
        je .zero
        cmp byte [bx+2], 8
        je .zero
        cmp byte [bx+2], 9
        jne .done
.zero:
        mov word [bx+0x12], 0
.done:
        mov word [bx+3], 0x0100
        pop bx
        pop ds
        retf

; ---- INT 67h: EMS y VCPI (lo atiende el monitor) ----
int67:
        cmp byte [cs:active], 0
        je .off
        MONCALL SVC_EMS
        iret
.off:
        mov ah, 0x84
        iret

; ---- INT 15h AH=87h: mover memoria extendida ----
int15:
        cmp ah, 0x87
        jne .chain
        cmp byte [cs:active], 0
        je .chain
        MONCALL SVC_I15
        push bp
        mov bp, sp
        jc .c
        and byte [bp+6], 0xFE
        jmp .o
.c:
        or byte [bp+6], 1
.o:
        pop bp
        iret
.chain:
        jmp far [cs:old15]

; ---- INT 2Fh AX=1605h: Windows arranca ----
int2f:
        cmp ax, 0x1605
        je .win
        cmp ax, 0x1606
        jne .chain
        ; Windows termina: el dispositivo recupera su nombre
        cmp word [cs:frame_seg], 0
        je .chain
        mov byte [cs:devname+3], 'X'
.chain:
        jmp far [cs:old2f]
.win:
        pushf
        call far [cs:old2f]
        test dx, 1                  ; modo estándar: usa VCPI, nada que hacer
        jnz .r
        test cx, cx
        jnz .r
        cmp byte [cs:active], 0
        je .r
        push ax
        MONCALL SVC_BUSY            ; AX = páginas EMS/VCPI en uso
        add ax, [cs:umb_given]
        test ax, ax
        pop ax
        jnz .busy
        push cs
        pop ds
        mov si, win_cb              ; DS:SI = apagar / encender el modo V86
        ; Mientras Windows está en marcha el dispositivo se llama EMMQXXX0
        ; (sin EMS, como con NOEMS): si ve un EMMXXXX0 exige heredar su
        ; estado («importación global de EMM») y, sin ella, no arranca. El
        ; EMS de sus sesiones de MS-DOS lo da el propio Windows.
        mov byte [devname+3], 'Q'
        iret
.busy:
        push ax
        push si
        push ds
        push cs
        pop ds
        mov si, msg_win
        call rputs
        pop ds
        pop si
        pop ax
        mov cx, 1                   ; Windows no debe seguir
.r:
        iret

; Windows llama aquí con AX = 0 (apagar el modo V86) o 1 (encenderlo), con
; las interrupciones cerradas
win_cb:
        cmp ax, 1
        je .on
        cmp byte [cs:active], 0
        je .ok
        call go_real
        jmp .ok
.on:
        cmp byte [cs:active], 0
        jne .ok
        call go_v86
.ok:
        clc
        retf

; modo V86 -> modo real
go_real:
        pushad
        push ds
        push es
        mov [cs:sw_ss], ss
        mov [cs:sw_sp], sp
        MONCALL SVC_REAL            ; el monitor salta a real16
; (segmento de código de 16 bits con base en este segmento)
real16:
        mov ax, SEL_D16
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov fs, ax
        mov gs, ax
        mov eax, cr0
        and eax, 0x7FFFFFFF         ; primero la paginación
        mov cr0, eax
        xor eax, eax
        mov cr3, eax
        mov eax, cr0
        and al, 0xFE
        mov cr0, eax
        jmp far [cs:real_jmp]
real_cont:
        mov ss, [cs:sw_ss]
        mov sp, [cs:sw_sp]
        lidt [cs:real_idtr]
        mov byte [cs:active], 0
        pop es
        pop ds
        popad
        ret

; modo real -> modo V86 (las interrupciones quedan cerradas)
go_v86:
        pushad
        push ds
        push es
        cli
        mov [cs:sw_ss], ss
        mov [cs:sw_sp], sp
        lgdt [cs:tmp_gdtr]
        mov eax, [cs:mon_cr3]
        mov cr3, eax
        mov eax, cr0
        or eax, 0x80000001
        mov cr0, eax
        jmp dword SEL_CODE:mon_start
v86_resume:
        mov byte [cs:active], 1
        pop es
        pop ds
        popad
        ret

; ---- XMS: memoria superior (funciones 10h, 11h y 12h) ----
xms_hook:
        cmp ah, 0x10
        je umb_alloc
        cmp ah, 0x11
        je umb_free
        cmp ah, 0x12
        je umb_realloc
        jmp far [cs:xms_chain]

; DX = párrafos -> AX = 1, BX = segmento, DX = tamaño real; o AX = 0,
; BL = B0h (hay uno menor: DX) o B1h (no queda ninguno)
umb_alloc:
        push si
        push di
        push cx
        xor di, di                  ; el mejor (el menor que sirva)
        xor cx, cx                  ; el mayor libre
        mov si, umb_tab
.l:
        cmp byte [cs:si+4], 0
        jne .n
        mov ax, [cs:si+2]
        cmp ax, cx
        jbe .m
        mov cx, ax
.m:
        cmp ax, dx
        jb .n
        test di, di
        jz .best
        cmp ax, [cs:di+2]
        jae .n
.best:
        mov di, si
.n:
        add si, 5
        cmp si, umb_tab + NUMB*5
        jb .l
        test di, di
        jz .fail
        ; partir si sobra y queda una entrada libre
        mov ax, [cs:di+2]
        sub ax, dx
        jz .take
        mov si, umb_tab
.s:
        cmp byte [cs:si+4], 0xFF
        je .split
        add si, 5
        cmp si, umb_tab + NUMB*5
        jb .s
        jmp .take
.split:
        mov [cs:si+2], ax
        mov ax, [cs:di]
        add ax, dx
        mov [cs:si], ax
        mov byte [cs:si+4], 0
        mov [cs:di+2], dx
.take:
        mov byte [cs:di+4], 1
        inc word [cs:umb_given]
        mov bx, [cs:di]
        mov dx, [cs:di+2]
        mov ax, 1
        pop cx
        pop di
        pop si
        retf
.fail:
        mov dx, cx
        mov bl, 0xB0
        test cx, cx
        jnz .f
        mov bl, 0xB1
.f:
        xor ax, ax
        pop cx
        pop di
        pop si
        retf

; DX = segmento -> SI = entrada en uso; CF si no la hay
umb_find:
        mov si, umb_tab
.l:
        cmp byte [cs:si+4], 1
        jne .n
        cmp [cs:si], dx
        je .ok
.n:
        add si, 5
        cmp si, umb_tab + NUMB*5
        jb .l
        stc
        ret
.ok:
        clc
        ret

; unir los bloques libres contiguos
umb_join:
        push si
        push di
        push ax
.again:
        mov si, umb_tab
.l:
        cmp byte [cs:si+4], 0
        jne .n
        mov ax, [cs:si]
        add ax, [cs:si+2]
        mov di, umb_tab
.f:
        cmp byte [cs:di+4], 0
        jne .fn
        cmp [cs:di], ax
        jne .fn
        mov ax, [cs:di+2]
        add [cs:si+2], ax
        mov byte [cs:di+4], 0xFF
        jmp .again
.fn:
        add di, 5
        cmp di, umb_tab + NUMB*5
        jb .f
.n:
        add si, 5
        cmp si, umb_tab + NUMB*5
        jb .l
        pop ax
        pop di
        pop si
        ret

umb_free:
        push si
        call umb_find
        jc .bad
        mov byte [cs:si+4], 0
        dec word [cs:umb_given]
        call umb_join
        pop si
        mov ax, 1
        xor bl, bl
        retf
.bad:
        pop si
        xor ax, ax
        mov bl, 0xB2                ; segmento de UMB no válido
        retf

; DX = segmento, BX = párrafos nuevos (solo se puede encoger)
umb_realloc:
        push si
        push di
        call umb_find
        jc .bad
        cmp bx, [cs:si+2]
        je .ok
        ja .big
        mov di, umb_tab
.s:
        cmp byte [cs:di+4], 0xFF
        je .split
        add di, 5
        cmp di, umb_tab + NUMB*5
        jb .s
        jmp .ok                     ; sin entradas: se queda como está
.split:
        mov ax, [cs:si+2]
        sub ax, bx
        mov [cs:di+2], ax
        mov ax, [cs:si]
        add ax, bx
        mov [cs:di], ax
        mov byte [cs:di+4], 0
        mov [cs:si+2], bx
        call umb_join
.ok:
        pop di
        pop si
        mov ax, 1
        xor bl, bl
        retf
.big:
        mov dx, [cs:si+2]
        pop di
        pop si
        xor ax, ax
        mov bl, 0xB0
        retf
.bad:
        pop di
        pop si
        xor ax, ax
        mov bl, 0xB2
        retf

; ---- error sin remedio de un programa en modo V86 ----
; El monitor desvía aquí al programa que hace algo que no se puede emular
; (pasar a modo protegido por su cuenta, por ejemplo): aviso y fin.
fatal_stub:
        cli
        mov ax, cs
        mov ds, ax
        mov ss, ax
        mov sp, fat_stack
        sti
        cld
        mov si, msg_fat
        call rputs
        mov si, msg_fat_pm
        cmp byte [fat_code], 1
        je .m
        mov si, msg_fat_op
        cmp byte [fat_code], 2
        je .m
        mov si, msg_fat_exc
.m:
        call rputs
        mov si, msg_fat_at
        call rputs
        mov ax, [fat_cs]
        call rhex4
        mov al, ':'
        int 0x29
        mov ax, [fat_ip]
        call rhex4
        mov si, msg_fat_end
        call rputs
        mov ax, 0x4CFF
        int 0x21
.h:
        hlt
        jmp .h

rputs:
        lodsb
        test al, al
        jz .r
        int 0x29
        jmp rputs
.r:
        ret
rhex4:
        push cx
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        int 0x29
        pop ax
        loop .l
        pop cx
        ret

msg_fat     db 13, 10, "HSPEMM: ", 0
msg_fat_pm  db "el programa quiere pasar a modo protegido por su cuenta (sin VCPI ni DPMI); no puede funcionar con HSPEMM cargado", 0
msg_fat_op  db "instrucción privilegiada que no se puede emular", 0
msg_fat_exc db "excepción del procesador", 0
msg_fat_at  db ", en ", 0
msg_fat_end db ". Programa terminado.", 13, 10, 0
msg_win     db 13, 10, "HSPEMM: hay memoria superior (UMB), EMS o VCPI en uso y Windows en modo 386", 13, 10
            db "mejorado no puede apagar el modo V86. Quite HSPEMM de CONFIG.SYS (o use", 13, 10
            db "NOUMB y no cargue nada que use EMS) para arrancar Windows.", 13, 10, 0
            align 2
            times 160 db 0
fat_stack:

resident_end:

; -----------------------------------------------------------------------------
;  Inicialización (modo real; se descarta)
; -----------------------------------------------------------------------------
xms         dd 0
opt_kb      dd 0                    ; 0 = automático
opt_noems   db 0
opt_noumb   db 0
opt_frame   dw 0
a20_was     db 0
xhandle     dw 0
lin_base    dd 0                    ; dirección lineal del bloque XMS
phys_base   dd 0                    ; alineada a página
sys_pages   dw 0
umb_pages   dw 0
pool_kb     dd 0
free_kb     dd 0
def_phys    dd 0
umb_phys    dd 0
pool_phys   dd 0
mv          times 16 db 0
; un byte por página de 4 KB de A0000 a FFFFF: 0 = ocupada, 1 = libre,
; 2 = marco EMS, 3 = UMB
pgmap       times 0x60 db 0
pgforce     times 0x60 db 0         ; 1 = incluida (I=), 2 = excluida (X=)

init:
        pushad
        push ds
        push es
        push cs
        pop ds
        cld
        mov [real_seg], cs
        ; ¿ya en modo protegido?
        smsw ax
        test al, 1
        jz .rm
        mov dx, msg_pm
        jmp .fail
.rm:
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
        ; el bloque de memoria que nos da el núcleo tiene que llegar para la
        ; zona de trabajo (4 KB tras el fichero)
        les bx, [req]
        mov ax, [es:bx+0x10]
        mov dx, cs
        sub ax, dx
        mov dx, scratch + 4096 + 15
        shr dx, 4
        cmp ax, dx
        jae .room
        mov dx, msg_nomem
        jmp .fail
.room:
        call parse_args
        call scan_upper
        call pick_frame
        call pick_umbs
        ; tamaño del bloque: sistema + marco por omisión + UMB + páginas
        mov ax, SYS_PAGES + MON_PAGES
        cmp word [frame_seg], 0
        je .nf
        add ax, 16
.nf:
        add ax, [umb_pages]
        mov [sys_pages], ax
        mov ah, 0x08
        call far [xms]
        movzx eax, ax
        mov [free_kb], eax
        movzx ebx, word [sys_pages]
        shl ebx, 2
        add ebx, 4                  ; holgura para alinear a página
        cmp eax, ebx
        jae .fits
        mov dx, msg_noext
        jmp .fail
.fits:
        sub eax, ebx                ; lo que queda para EMS/VCPI
        mov ecx, [opt_kb]
        test ecx, ecx
        jnz .given
        mov ecx, eax
        shr ecx, 1
        cmp ecx, 32768
        jbe .given
        mov ecx, 32768
.given:
        cmp ecx, eax
        jbe .c1
        mov ecx, eax
.c1:
        cmp ecx, MAXPOOL*16
        jbe .c2
        mov ecx, MAXPOOL*16
.c2:
        and ecx, ~15
        mov [pool_kb], ecx
        lea edx, [ecx+ebx]
        mov ah, 0x09
        call far [xms]
        test ax, ax
        jnz .got
        mov dx, msg_noext
        jmp .fail
.got:
        mov [xhandle], dx
        mov ah, 0x0C
        call far [xms]
        test ax, ax
        jnz .locked
        mov dx, [xhandle]
        mov ah, 0x0A
        call far [xms]
        mov dx, msg_noext
        jmp .fail
.locked:
        mov [lin_base], bx
        mov [lin_base+2], dx
        mov eax, [lin_base]
        add eax, 0xFFF
        and eax, ~0xFFF
        mov [phys_base], eax
        mov [mon_cr3], eax
        ; direcciones físicas de cada parte
        add eax, (SYS_PAGES + MON_PAGES) * 4096
        cmp word [frame_seg], 0
        je .nd
        mov [def_phys], eax
        add eax, 16 * 4096
.nd:
        mov [umb_phys], eax
        movzx ebx, word [umb_pages]
        shl ebx, 12
        add eax, ebx
        mov [pool_phys], eax
        ; la línea A20: abierta de verdad desde ahora (el monitor está por
        ; encima del mega); los programas ven la suya, virtual
        call a20_test
        mov [a20_was], al
        mov ah, 0x05                ; activación local
        call far [xms]
        call a20_test
        test al, al
        jnz .a20ok
        mov dx, msg_a20
        jmp .fail_free
.a20ok:
        ; --- directorio de páginas ---
        call scratch_zero
        mov eax, [phys_base]
        add eax, 0x1000
        or eax, 7
        mov [scratch], eax
        add eax, 0x1000
        and al, 0xF8
        or al, 3
        mov [scratch+4], eax
        xor eax, eax
        call put_page
        jc .mverr
        ; --- PT1 (ventanas), vacía ---
        call scratch_zero
        mov eax, 2
        call put_page
        jc .mverr
        ; --- PT0 ---
        call build_pt0
        mov eax, 1
        call put_page
        jc .mverr
        ; --- el monitor ---
        mov eax, [phys_base]
        mov [MONV(p_phys)], eax
        mov eax, [pool_phys]
        mov [MONV(p_poolphys)], eax
        mov eax, [pool_kb]
        shr eax, 4
        mov [MONV(p_poolpg)], eax
        mov eax, [def_phys]
        mov [MONV(p_defphys)], eax
        movzx eax, word [frame_seg]
        shr eax, 8
        mov [MONV(p_framepg)], eax
        xor eax, eax
        mov ax, cs
        mov [MONV(p_drvseg)], eax
        mov al, [a20_was]
        cmp byte [opt_a20], 0
        je .a20v
        mov al, 1
.a20v:
        mov [MONV(p_a20)], al
        mov dword [mv], mon_size
        mov word [mv+4], 0
        mov word [mv+6], section..mon.start
        mov [mv+8], cs
        mov ax, [xhandle]
        mov [mv+10], ax
        mov eax, [phys_base]
        sub eax, [lin_base]
        add eax, SYS_PAGES * 4096
        mov [mv+12], eax
        mov si, mv
        mov ah, 0x0B
        call far [xms]
        test ax, ax
        jz .mverr
        ; --- al modo V86 ---
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, tmp_gdt
        mov [tmp_gdtr+2], eax
        pushf
        call go_v86
        popf
        ; --- ya en modo V86: vectores ---
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov dx, int67
        mov ax, 0x2567
        int 0x21
        ; la memoria nueva, a cero
        cmp word [frame_seg], 0
        je .nz
        mov es, [frame_seg]
        xor di, di
        xor ax, ax
        mov cx, 0x8000
        rep stosw
.nz:
        call umb_setup
        cmp byte [opt_noems], 0
        je .name
        mov byte [devname+3], 'Q'   ; «EMMQXXX0»: VCPI sin EMS
.name:
        call report
        les bx, [req]
        mov word [es:bx+0x0E], resident_end
        mov [es:bx+0x10], cs
        jmp .out
.mverr:
        mov dx, msg_mverr
.fail_free:
        push dx
        mov dx, [xhandle]
        mov ah, 0x0D
        call far [xms]
        mov dx, [xhandle]
        mov ah, 0x0A
        call far [xms]
        mov ah, 0x06
        call far [xms]
        pop dx
.fail:
        mov ah, 9
        int 0x21
        les bx, [req]
        mov word [es:bx+0x0E], 0
        mov [es:bx+0x10], cs
.out:
        mov word [es:bx+3], 0x0100
        pop es
        pop ds
        popad
        retf

scratch_zero:
        push es
        push cs
        pop es
        mov di, scratch
        xor ax, ax
        mov cx, 2048
        rep stosw
        pop es
        ret

; copiar la zona de trabajo a la página EAX del bloque; CF si falla
put_page:
        shl eax, 12
        add eax, [phys_base]
        sub eax, [lin_base]
        mov [mv+12], eax
        mov dword [mv], 4096
        mov word [mv+4], 0
        mov word [mv+6], scratch
        mov [mv+8], cs
        mov ax, [xhandle]
        mov [mv+10], ax
        mov si, mv
        mov ah, 0x0B
        call far [xms]
        cmp ax, 1
        ret                         ; CF si AX = 0

; la primera tabla de páginas (0 a 4 MB)
build_pt0:
        call scratch_zero
        mov di, scratch
        xor eax, eax
        mov al, 7
        mov cx, 0x100
.id:
        mov [di], eax
        add eax, 0x1000
        add di, 4
        loop .id
        ; UMB y marco
        mov si, pgmap
        mov di, scratch + 0xA0*4
        mov ebx, [umb_phys]
        mov edx, [def_phys]
.up:
        cmp byte [si], 3
        jne .nu
        mov eax, ebx
        or al, 7
        mov [di], eax
        add ebx, 0x1000
.nu:
        cmp byte [si], 2
        jne .nf
        mov eax, edx
        or al, 7
        mov [di], eax
        add edx, 0x1000
.nf:
        inc si
        add di, 4
        cmp si, pgmap + 0x60
        jb .up
        ; HMA: de verdad, o el espejo del principio si A20 estaba cerrada
        mov di, scratch + 0x100*4
        mov eax, 0x100007
        cmp byte [a20_was], 0
        jne .h
        cmp byte [opt_a20], 0
        jne .h
        mov eax, 7
.h:
        mov cx, 16
.hl:
        mov [di], eax
        add eax, 0x1000
        add di, 4
        loop .hl
        ; el monitor (solo anillo 0)
        mov eax, [phys_base]
        or al, 3
        mov cx, SYS_PAGES + MON_PAGES
.ml:
        mov [di], eax
        add eax, 0x1000
        add di, 4
        loop .ml
        ret

; ---- opciones de la línea de CONFIG.SYS ----
opt_a20     db 1                    ; A20 virtual abierta al empezar
parse_args:
        les bx, [req]
        les si, [es:bx+0x12]
        ; saltar el nombre del fichero
.sk:
        mov al, [es:si]
        cmp al, ' '
        je .args
        cmp al, 9
        je .args
        cmp al, 13
        jbe .end
        inc si
        jmp .sk
.args:
        call .blank
        mov al, [es:si]
        cmp al, 13
        jbe .end
        call .upper
        cmp al, '0'
        jb .word
        cmp al, '9'
        ja .word
        ; número: KB
        xor ebx, ebx
.num:
        mov al, [es:si]
        cmp al, '0'
        jb .numend
        cmp al, '9'
        ja .numend
        imul ebx, ebx, 10
        and eax, 0x0F
        add ebx, eax
        inc si
        jmp .num
.numend:
        cmp ebx, 64
        jae .nok
        mov ebx, 64
.nok:
        mov [opt_kb], ebx
        jmp .args
.word:
        mov di, k_noems
        call .match
        jne .w2
        mov byte [opt_noems], 1
        jmp .args
.w2:
        mov di, k_noumb
        call .match
        jne .w3
        mov byte [opt_noumb], 1
        jmp .args
.w3:
        mov di, k_frame
        call .match
        jne .w4
        call .hex
        mov [opt_frame], bx
        jmp .args
.w4:
        mov di, k_inc
        call .match
        jne .w5
        mov dl, 1
        call .range
        jmp .args
.w5:
        mov di, k_exc
        call .match
        jne .w6
        mov dl, 2
        call .range
        jmp .args
.w6:
        ; palabra desconocida (RAM, ...): se salta
.ws:
        mov al, [es:si]
        cmp al, ' '
        jbe .args
        inc si
        jmp .ws
.end:
        ret
.blank:
        mov al, [es:si]
        cmp al, ' '
        je .b1
        cmp al, 9
        jne .b2
.b1:
        inc si
        jmp .blank
.b2:
        ret
.upper:
        cmp al, 'a'
        jb .u1
        cmp al, 'z'
        ja .u1
        sub al, 0x20
.u1:
        ret
; ¿empieza por la palabra de DS:DI (acabada en 0)? ZF y SI avanzado si sí
.match:
        push si
.m1:
        mov ah, [di]
        test ah, ah
        jz .myes
        mov al, [es:si]
        call .upper
        cmp al, ah
        jne .mno
        inc si
        inc di
        jmp .m1
.myes:
        add sp, 2
        xor al, al                  ; ZF
        ret
.mno:
        pop si
        or al, 1
        ret
; número hexadecimal -> BX
.hex:
        xor bx, bx
.h1:
        mov al, [es:si]
        call .upper
        sub al, '0'
        jb .h9
        cmp al, 9
        jbe .h2
        sub al, 7
        cmp al, 10
        jb .h9
        cmp al, 15
        ja .h9
.h2:
        shl bx, 4
        or bl, al
        inc si
        jmp .h1
.h9:
        ret
; xxxx-yyyy: marca las páginas con DL
.range:
        call .hex
        mov cx, bx
        cmp byte [es:si], '-'
        jne .r9
        inc si
        call .hex
        shr cx, 8                   ; página inicial
        shr bx, 8                   ; página final
.r1:
        cmp cx, bx
        ja .r9
        cmp cx, 0xA0
        jb .r2
        cmp cx, 0xFF
        ja .r2
        mov di, cx
        mov [pgforce+di-0xA0], dl
.r2:
        inc cx
        jmp .r1
.r9:
        ret

k_noems     db "NOEMS", 0
k_noumb     db "NOUMB", 0
k_frame     db "FRAME=", 0
k_inc       db "I=", 0
k_exc       db "X=", 0

; ---- memoria superior libre ----
; Una página de 4 KB se da por libre si no la ocupa una ROM de ampliación
; (firma 55 AA cada 2 KB) y todo su contenido es FFh (o 00h). Por omisión
; solo se mira C000-DFFF.
scan_upper:
        ; candidatas
        mov bx, 0xA0
.c:
        xor al, al
        cmp bx, 0xC0
        jb .c1
        cmp bx, 0xE0
        jae .c1
        mov al, 1
.c1:
        cmp byte [pgforce+bx-0xA0], 1
        jne .c2
        mov al, 1
.c2:
        cmp byte [pgforce+bx-0xA0], 2
        jne .c3
        xor al, al
.c3:
        mov [pgmap+bx-0xA0], al
        inc bx
        cmp bx, 0x100
        jb .c
        ; ROM de ampliación
        mov bx, 0xC000
.rom:
        mov es, bx
        cmp word [es:0], 0xAA55
        jne .rn
        movzx ax, byte [es:2]
        test ax, ax
        jnz .r1
        inc ax
.r1:
        shl ax, 5                   ; bloques de 512 bytes -> párrafos
        mov cx, bx
        add cx, ax
        dec cx                      ; último párrafo
        mov si, bx
        shr si, 8
        shr cx, 8
.rm:
        mov byte [pgmap+si-0xA0], 0
        inc si
        cmp si, cx
        jbe .rm
        add ax, 0x7F
        and ax, 0xFF80
        add bx, ax
        jc .romend
        cmp bx, 0xF000
        jb .rom
        jmp .romend
.rn:
        add bx, 0x80
        cmp bx, 0xF000
        jb .rom
.romend:
        ; contenido
        mov bx, 0xA0
.t:
        cmp byte [pgmap+bx-0xA0], 1
        jne .tn
        mov ax, bx
        shl ax, 8
        mov es, ax
        xor di, di
        mov ax, [es:0]
        cmp ax, 0xFFFF
        je .t1
        test ax, ax
        jnz .used
.t1:
        mov cx, 2048
        repe scasw
        je .tn
.used:
        mov byte [pgmap+bx-0xA0], 0
.tn:
        inc bx
        cmp bx, 0x100
        jb .t
        ret

; marco de página EMS: 64 KB libres en un segmento múltiplo de 400h; el más
; alto (o el que diga FRAME=)
pick_frame:
        mov word [frame_seg], 0
        cmp byte [opt_noems], 0
        jne .r
        mov bx, [opt_frame]
        test bx, bx
        jz .auto
        and bx, 0xFC00
        cmp bx, 0xA000
        jb .auto
        cmp bx, 0xF000
        ja .auto
        jmp .take                   ; lo manda el usuario
.auto:
        mov bx, 0xF000
.l:
        mov si, bx
        shr si, 8
        mov cx, 16
.f:
        cmp si, 0x100
        jae .next
        cmp byte [pgmap+si-0xA0], 1
        jne .next
        inc si
        loop .f
        jmp .take
.next:
        sub bx, 0x400
        cmp bx, 0xC000
        jae .l
        mov byte [opt_noems], 2     ; no hay sitio para el marco
        ret
.take:
        mov [frame_seg], bx
        mov si, bx
        shr si, 8
        mov cx, 16
.m:
        mov byte [pgmap+si-0xA0], 2
        inc si
        loop .m
.r:
        ret

pick_umbs:
        mov word [umb_pages], 0
        mov bx, 0xA0
.l:
        cmp byte [pgmap+bx-0xA0], 1
        jne .n
        cmp byte [opt_noumb], 0
        jne .n
        mov byte [pgmap+bx-0xA0], 3
        inc word [umb_pages]
.n:
        inc bx
        cmp bx, 0x100
        jb .l
        ret

; (ya en modo V86) limpiar los UMB, hacer la tabla e interceptar XMS
umb_setup:
        cmp word [umb_pages], 0
        je .r
        mov di, umb_tab
        mov bx, 0xA0
.l:
        cmp byte [pgmap+bx-0xA0], 3
        jne .n
        ; principio de una zona
        mov ax, bx
        shl ax, 8
        mov [di], ax
        xor cx, cx
.run:
        push di
        push cx
        mov ax, bx
        shl ax, 8
        mov es, ax
        xor di, di
        xor ax, ax
        mov cx, 2048
        rep stosw
        pop cx
        pop di
        add cx, 0x100
        inc bx
        cmp bx, 0x100
        jae .rend
        cmp byte [pgmap+bx-0xA0], 3
        je .run
.rend:
        mov [di+2], cx
        mov byte [di+4], 0
        add di, 5
        cmp di, umb_tab + (NUMB-2)*5
        jae .hook                   ; (quedan entradas para partir bloques)
        jmp .l2
.n:
        inc bx
.l2:
        cmp bx, 0x100
        jb .l
.hook:
        ; el punto de entrada de XMS empieza por un salto corto y tres NOP
        ; (o por saltos lejanos de quien ya lo interceptó): se pone ahí un
        ; salto lejano a xms_hook y se sigue en lo que había detrás
        les bx, [xms]
.ch:
        cmp byte [es:bx], 0xEA
        jne .ins
        les bx, [es:bx+1]
        jmp .ch
.ins:
        cmp byte [es:bx], 0xEB
        je .patch
        cmp byte [es:bx], 0x90
        jne .r
.patch:
        lea ax, [bx+5]
        mov [xms_chain], ax
        mov [xms_chain+2], es
        cli
        mov byte [es:bx], 0xEA
        mov word [es:bx+1], xms_hook
        mov [es:bx+3], cs
        sti
.r:
        ret

; ---- mensaje de instalación ----
report:
        mov dx, msg_ok
        mov ah, 9
        int 0x21
        cmp word [frame_seg], 0
        je .noems
        mov dx, msg_ems
        mov ah, 9
        int 0x21
        mov eax, [pool_kb]
        call print_dec
        mov dx, msg_frame
        mov ah, 9
        int 0x21
        mov ax, [frame_seg]
        call print_hex4
        jmp .umb
.noems:
        mov dx, msg_vcpi
        mov ah, 9
        int 0x21
        mov eax, [pool_kb]
        call print_dec
        mov dx, msg_kb
        mov ah, 9
        int 0x21
        cmp byte [opt_noems], 2
        jne .umb
        mov dx, msg_noframe
        mov ah, 9
        int 0x21
.umb:
        cmp word [umb_pages], 0
        je .nl
        mov dx, msg_umb
        mov ah, 9
        int 0x21
        movzx eax, word [umb_pages]
        shl eax, 2
        call print_dec
        mov dx, msg_kb2
        mov ah, 9
        int 0x21
        ; las zonas
        mov si, umb_tab
.z:
        cmp byte [si+4], 0xFF
        je .zn
        mov dl, ' '
        mov ah, 2
        int 0x21
        mov ax, [si]
        call print_hex4
        mov dl, '-'
        mov ah, 2
        int 0x21
        mov ax, [si]
        add ax, [si+2]
        dec ax
        call print_hex4
.zn:
        add si, 5
        cmp si, umb_tab + NUMB*5
        jb .z
        mov dl, ')'
        mov ah, 2
        int 0x21
.nl:
        mov dx, msg_crlf
        mov ah, 9
        int 0x21
        ret

print_dec:
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret
print_hex4:
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .l
        ret

; -> AL = 1 si A20 está abierta
a20_test:
        push ds
        push es
        push si
        push di
        pushf
        cli
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov si, 0x0500
        mov di, 0x0510
        mov ah, [si]
        mov byte [si], 0x55
        cmp byte [es:di], 0x55
        jne .on
        mov byte [si], 0xAA
        cmp byte [es:di], 0xAA
        jne .on
        xor al, al
        jmp .r
.on:
        mov al, 1
.r:
        mov [si], ah
        popf
        pop di
        pop si
        pop es
        pop ds
        ret

msg_pm      db "HSPEMM: el procesador ya está en modo protegido; no se instala.", 13, 10, "$"
msg_noxms   db "HSPEMM: falta el gestor XMS (cargue antes HSPXMS.SYS); no se instala.", 13, 10, "$"
msg_nomem   db "HSPEMM: no hay memoria convencional para arrancar; no se instala.", 13, 10, "$"
msg_noext   db "HSPEMM: no hay memoria extendida libre; no se instala.", 13, 10, "$"
msg_a20     db "HSPEMM: no se puede abrir la línea A20; no se instala.", 13, 10, "$"
msg_mverr   db "HSPEMM: error al copiar a la memoria extendida; no se instala.", 13, 10, "$"
msg_ok      db "HSPEMM 0.1: $"
msg_ems     db "EMS 4.0 y VCPI, $"
msg_frame   db " KB; marco de página en $"
msg_vcpi    db "VCPI, $"
msg_kb      db " KB$"
msg_noframe db " (sin EMS: no hay 64 KB libres para el marco; pruebe FRAME=)$"
msg_umb     db "; memoria superior: $"
msg_kb2     db " KB ($"
msg_crlf    db 13, 10, "$"

; =============================================================================
;  El monitor (32 bits, anillo 0). Se copia a la memoria extendida y se ve en
;  la dirección lineal MONIMG.
; =============================================================================
        section .mon vstart=MONIMG follows=.text align=16
        bits 32

; ---- marco de pila del monitor (tras PUSHAD) ----
P_EDI       equ 0
P_ESI       equ 4
P_EBP       equ 8
P_EBX       equ 16
P_EDX       equ 20
P_ECX       equ 24
P_EAX       equ 28
F_VEC       equ 32
F_ERR       equ 36
F_EIP       equ 40
F_CS        equ 44
F_EFL       equ 48
F_ESP       equ 52
F_SS        equ 56
F_ES        equ 60
F_DS        equ 64
F_FS        equ 68
F_GS        equ 72

; entrada de la tabla de handles EMS
H_START     equ 0                   ; primera entrada suya en pagemap
H_COUNT     equ 2                   ; páginas
H_FLAGS     equ 4                   ; 1 = abierto, 2 = contexto guardado
H_NAME      equ 6
H_SAVE      equ 14                  ; contexto guardado (4 x handle, página)
H_LEN       equ 32

TSS_LEN     equ 104 + 8192 + 1

; dirección lineal de un puntero seg:desp del programa V86
; (registro de destino -no EAX-, campo del segmento, campo del desplazamiento)
%macro V86PTR 3
        movzx %1, word [ebp+%2]
        shl %1, 4
        push eax
        movzx eax, word [ebp+%3]
        add %1, eax
        pop eax
%endmacro

mon_start:
        jmp mon_entry
        align 4
p_phys      dd 0                    ; dirección física del directorio
p_poolphys  dd 0                    ; páginas EMS/VCPI
p_poolpg    dd 0                    ; cuántas (de 16 KB)
p_defphys   dd 0                    ; RAM del marco sin mapear
p_framepg   dd 0                    ; página lineal del marco (0 = sin EMS)
p_drvseg    dd 0                    ; segmento de la parte residente
p_a20       dd 0                    ; línea A20 virtual
p_reenter   dd 0

mon_gdtr    dw gdt_end - gdt - 1
            dd gdt
mon_idtr    dw 256*8 - 1
            dd idt
null_idtr   dw 0
            dd 0
            align 8
gdt         dq 0
            dq 0x00CF9A000000FFFF   ; código de 32 bits, todo el espacio
            dq 0x00CF92000000FFFF   ; datos
            dq 0x0000890000000000 + TSS_LEN - 1  ; TSS (la base, al arrancar)
            dq 0x00009A000000FFFF   ; código de 16 bits (volver a modo real)
            dq 0x000092000000FFFF   ; datos de 16 bits
gdt_end:

; -----------------------------------------------------------------------------
;  Entrada desde el modo real (go_v86): paginación recién activada
; -----------------------------------------------------------------------------
mon_entry:
        mov ax, SEL_DATA
        mov ds, ax
        mov es, ax
        mov ss, ax
        cld
        cmp byte [p_reenter], 0
        jne .re
        mov edi, monbss_start
        mov ecx, (monbss_end - monbss_start) / 4
        xor eax, eax
        rep stosd
        mov esp, STKTOP
        call mon_setup
        mov byte [p_reenter], 1
.re:
        mov esp, STKTOP
        lgdt [mon_gdtr]
        jmp SEL_CODE:.cs
.cs:
        mov ax, SEL_DATA
        mov ds, ax
        mov es, ax
        mov ss, ax
        xor eax, eax
        mov fs, ax
        mov gs, ax
        lldt ax
        lidt [mon_idtr]
        and byte [gdt+SEL_TSS+5], ~2
        mov ax, SEL_TSS
        ltr ax
        ; seguir en modo V86 en v86_resume, con la pila que dejó go_v86
        mov ebx, [p_drvseg]
        mov edx, ebx
        shl edx, 4
        push ebx                    ; GS, FS, DS, ES
        push ebx
        push ebx
        push ebx
        movzx eax, word [edx+sw_ss]
        push eax
        movzx eax, word [edx+sw_sp]
        push eax
        push dword 0x00023002       ; VM, IOPL 3, interrupciones cerradas
        push ebx
        push dword v86_resume
        iretd

mon_setup:
        ; descriptores con base
        mov eax, tss
        mov [gdt+SEL_TSS+2], ax
        shr eax, 16
        mov [gdt+SEL_TSS+4], al
        mov [gdt+SEL_TSS+7], ah
        mov eax, [p_drvseg]
        shl eax, 4
        mov [gdt+SEL_C16+2], ax
        shr eax, 16
        mov [gdt+SEL_C16+4], al
        ; IDT: 256 puertas de interrupción de anillo 0 abiertas al anillo 3
        mov edi, idt
        mov eax, stubs
        mov ecx, 256
.idt:
        mov [edi], ax
        mov word [edi+2], SEL_CODE
        mov word [edi+4], 0xEE00
        mov ebx, eax
        shr ebx, 16
        mov [edi+6], bx
        add eax, 10
        add edi, 8
        loop .idt
        ; TSS: pila de anillo 0 y mapa de puertos
        mov dword [tss+4], STKTOP
        mov word [tss+8], SEL_DATA
        mov word [tss+0x66], 104
        mov byte [tss+104+8192], 0xFF
        mov esi, trap_ports
.tp:
        movzx eax, word [esi]
        cmp ax, 0xFFFF
        je .tpe
        bts [tss+104], eax
        add esi, 2
        jmp .tp
.tpe:
        ; EMS: todas las páginas libres, el handle 0 abierto y nada mapeado
        xor eax, eax
.pm:
        mov [pagemap+eax*2], ax
        inc eax
        cmp eax, MAXPOOL
        jb .pm
        mov eax, [p_poolpg]
        mov [nfree_end], eax
        mov byte [handles+H_FLAGS], 1
        mov eax, -1
        mov [curmap], eax
        mov [curmap+4], eax
        mov [curmap+8], eax
        mov [curmap+12], eax
        mov [v_hint], eax
        mov word [pic_master], 0x08
        mov word [pic_slave], 0x70
        ret

; puertos que se virtualizan: A20 (8042 y puerto 92h), DMA (8237) y la
; palabra de control del temporizador (8254)
trap_ports  dw 0x60, 0x64, 0x92, 0x43
            dw 0, 1, 2, 3, 4, 5, 6, 7, 0x0C, 0x0D
            dw 0x81, 0x82, 0x83, 0x87, 0x89, 0x8A, 0x8B
            dw 0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE, 0xD8, 0xDA
            dw 0x0A, 0x0B, 0x0F, 0xD4, 0xD6, 0xDE   ; [SB] modo y máscara del 8237
            dw 0xFFFF

; -----------------------------------------------------------------------------
;  Entrada de todas las interrupciones y excepciones
; -----------------------------------------------------------------------------
stubs:
%assign n 0
%rep 256
        push strict dword n
        jmp strict near int_common
%assign n n+1
%endrep

; Desde el modo V86 la pila de anillo 0 empieza siempre en STKTOP: por la
; profundidad se sabe si el procesador apiló un código de error (excepción)
; o no (interrupción de programa, IRQ o excepción sin código).
int_common:
        cmp esp, STKTOP - 40
        je .noerr
        cmp esp, STKTOP - 44
        je .frame
        jmp ring0_int
.noerr:
        push dword [esp]
        mov dword [esp+4], -1       ; sin código de error
.frame:
        pushad
        mov ebp, esp
        mov ax, SEL_DATA
        mov ds, ax
        mov es, ax
        cld
        mov eax, [ebp+F_VEC]
        cmp dword [ebp+F_ERR], -1
        jne exception
        cmp al, 0x67
        je i67_fast
        cmp al, 6
        je ud_trap
reflect:
        ; como el procesador en modo real: apilar FLAGS, CS, IP y saltar al
        ; vector, con IF y TF a cero
        movzx edx, word [ebp+F_SS]
        shl edx, 4
        mov ecx, [ebp+F_ESP]
        sub cx, 6
        mov [ebp+F_ESP], cx
        movzx ecx, cx
        add edx, ecx
        mov bx, [ebp+F_EIP]
        mov [edx], bx
        mov bx, [ebp+F_CS]
        mov [edx+2], bx
        mov bx, [ebp+F_EFL]
        mov [edx+4], bx
        and dword [ebp+F_EFL], ~0x0300
        mov ebx, [eax*4]
        movzx ecx, bx
        mov [ebp+F_EIP], ecx
        shr ebx, 16
        mov [ebp+F_CS], ebx
mon_exit:
        popad
        add esp, 8
        iretd

; INT 67h: si el vector sigue siendo el nuestro, se atiende sin reflejar
i67_fast:
        mov ebx, [p_drvseg]
        shl ebx, 16
        mov bx, int67
        cmp [0x67*4], ebx
        jne reflect
        mov ebx, [p_drvseg]
        shl ebx, 4
        cmp byte [ebx+active], 0
        je reflect
        call ems_dispatch
        jmp mon_exit

; código no válido: ¿es una llamada al monitor (MONCALL) de la parte residente?
ud_trap:
        mov ebx, [ebp+F_CS]
        cmp bx, [p_drvseg]
        jne reflect
        shl ebx, 4
        movzx ecx, word [ebp+F_EIP]
        cmp byte [ebx+ecx], 0x63
        jne reflect
        movzx eax, byte [ebx+ecx+1]
        add word [ebp+F_EIP], 2
        cmp al, 0xC0 + SVC_EMS
        je .s_ems
        cmp al, 0xC0 + SVC_I15
        je .s_i15
        cmp al, 0xC0 + SVC_REAL
        je svc_real
        cmp al, 0xC0 + SVC_BUSY
        je .s_busy
        jmp mon_exit
.s_ems:
        call ems_dispatch
        jmp mon_exit
.s_i15:
        call svc_i15
        jmp mon_exit
.s_busy:
        ; AX = páginas de 16 KB en uso (EMS de los handles y VCPI)
        mov eax, [ems_used]
        add eax, [p_poolpg]
        sub eax, [nfree_end]
        mov [ebp+P_EAX], ax
        jmp mon_exit

; volver a modo real (Windows): sigue en real16, en la parte residente
svc_real:
        jmp SEL_C16:real16

; interrupción con el procesador en anillo 0: solo puede ser la que despierta
; al HLT emulado; se refleja al programa V86, que sigue tras su HLT
ring0_int:
        cmp byte [in_hlt], 0
        je fatal0
        mov byte [in_hlt], 0
        mov eax, [esp]
        mov esp, [hlt_esp]
        mov ebp, esp
        cld
        jmp reflect

; -----------------------------------------------------------------------------
;  Excepciones con código de error desde el modo V86
; -----------------------------------------------------------------------------
exception:
        cmp al, 0x0D
        je gp_fault
        cmp al, 0x08
        je fatal0
        mov dl, 3
        jmp fatal_v86

; El programa V86 no puede seguir: se le desvía al aviso de la parte
; residente (DL = motivo: 1 modo protegido, 2 instrucción, 3 excepción)
fatal_v86:
        mov ebx, [p_drvseg]
        mov ecx, ebx
        shl ecx, 4
        mov [ecx+fat_code], dl
        mov eax, [ebp+F_CS]
        mov [ecx+fat_cs], ax
        mov eax, [ebp+F_EIP]
        mov [ecx+fat_ip], ax
        mov [ebp+F_CS], ebx
        mov dword [ebp+F_EIP], fatal_stub
        and dword [ebp+F_EFL], ~0x0300
        jmp mon_exit

; error dentro del monitor: aviso en la pantalla y en el puerto serie, y alto
fatal0:
        mov ax, SEL_DATA
        mov ds, ax
        mov es, ax
        mov ebx, esp
        mov esi, msg_fatal0
        mov edi, 0xB8000
.l:
        lodsb
        test al, al
        jz .regs
        call .putc
        jmp .l
.regs:
        mov ecx, 6                  ; vector y lo que haya en la pila
.r:
        mov edx, [ebx]
        add ebx, 4
        push ecx
        mov ecx, 8
.h:
        rol edx, 4
        mov al, dl
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        call .putc
        loop .h
        mov al, ' '
        call .putc
        pop ecx
        loop .r
.halt:
        hlt
        jmp .halt
.putc:
        mov ah, 0x4F
        stosw
        push edx
        push eax
        mov dx, 0x3FD
.w:
        in al, dx
        test al, 0x20
        jz .w
        pop eax
        mov dx, 0x3F8
        out dx, al
        pop edx
        ret
msg_fatal0  db "HSPEMM: error interno del monitor ", 0

; reinicio del equipo (la BIOS lo intenta con instrucciones privilegiadas)
reset_machine:
        mov ecx, 0x10000
.w:
        in al, 0x64
        test al, 2
        loopnz .w
        mov al, 0xFE
        out 0x64, al
        mov ecx, 0x100000
.d:
        loop .d
        lidt [null_idtr]
        int3

; -----------------------------------------------------------------------------
;  #GP: instrucción privilegiada o puerto vigilado
; -----------------------------------------------------------------------------
gp_fault:
        movzx esi, word [ebp+F_CS]
        shl esi, 4
        mov [gp_base], esi
        movzx eax, word [ebp+F_EIP]
        add esi, eax
        xor ebx, ebx                ; BL: 1 = operando de 32 bits, 4 = REP
.pfx:
        lodsb
        cmp al, 0x66
        jne .p1
        or bl, 1
        jmp .pfx
.p1:
        cmp al, 0x67
        je .pfx
        cmp al, 0xF3
        je .rep
        cmp al, 0xF2
        jne .p2
.rep:
        or bl, 4
        jmp .pfx
.p2:
        cmp al, 0xF0
        je .pfx
        cmp al, 0x26
        je .pfx
        cmp al, 0x2E
        je .pfx
        cmp al, 0x36
        je .pfx
        cmp al, 0x3E
        je .pfx
        cmp al, 0x64
        je .pfx
        cmp al, 0x65
        je .pfx
        cmp al, 0xF4
        je .hlt
        cmp al, 0x0F
        je .two
        mov ah, al
        and ah, 0xF6
        cmp ah, 0xE4                ; E4, E5, EC, ED: IN
        je .in
        cmp ah, 0xE6                ; E6, E7, EE, EF: OUT
        je .out
.bad:
        mov dl, 2
        jmp fatal_v86
.done:
        sub esi, [gp_base]
        mov [ebp+F_EIP], si
        jmp mon_exit

; HLT: esperar una interrupción con el procesador parado de verdad
.hlt:
        sub esi, [gp_base]
        mov [ebp+F_EIP], si
        test dword [ebp+F_EFL], 0x200
        jz mon_exit
        mov [hlt_esp], esp
        mov byte [in_hlt], 1
        sti
        hlt
        cli
        mov byte [in_hlt], 0
        jmp mon_exit

.in:
        test al, 8
        jnz .indx
        movzx edx, byte [esi]
        inc esi
        jmp .in2
.indx:
        movzx edx, word [ebp+P_EDX]
.in2:
        xor ah, ah                  ; [SB] ¿puerto de la Sound Blaster emulada?
        call sb_upcall              ; [SB] (si lo es, no vuelve)
        test al, 1
        jnz .inw
        call port_in
        mov [ebp+P_EAX], al
        jmp .done
.inw:
        call port_in
        mov cl, al
        inc edx
        call port_in
        mov ch, al
        test bl, 1
        jnz .ind
        mov [ebp+P_EAX], cx
        jmp .done
.ind:
        push cx
        inc edx
        call port_in
        mov cl, al
        inc edx
        call port_in
        mov ch, al
        shl ecx, 16
        pop cx
        mov [ebp+P_EAX], ecx
        jmp .done

.out:
        test al, 8
        jnz .outdx
        movzx edx, byte [esi]
        inc esi
        jmp .out2
.outdx:
        movzx edx, word [ebp+P_EDX]
.out2:
        mov ah, 1                   ; [SB]
        call sb_upcall              ; [SB]
        mov ecx, [ebp+P_EAX]
        test al, 1
        jnz .outw
        mov al, cl
        call port_out
        jmp .done
.outw:
        mov al, cl
        call port_out
        inc edx
        mov al, ch
        call port_out
        test bl, 1
        jz .done
        shr ecx, 16
        inc edx
        mov al, cl
        call port_out
        inc edx
        mov al, ch
        call port_out
        jmp .done

; instrucciones de dos bytes (0F xx)
.two:
        lodsb
        cmp al, 0x06                ; CLTS
        jne .t1
        clts
        jmp .done
.t1:
        cmp al, 0x01
        jne .t2
        ; LGDT / LIDT: el programa quiere pasar a modo protegido (o la BIOS
        ; está reiniciando el equipo); LMSW se pasa por alto
        mov al, [esi]
        shr al, 3
        and al, 7
        cmp al, 6
        je .lmsw
        cmp word [ebp+F_CS], 0xF000
        jae reset_machine
        mov dl, 1
        jmp fatal_v86
.lmsw:
        call .skip_modrm
        jmp .done
.t2:
        cmp al, 0x20
        jb .bad
        cmp al, 0x23
        ja .bad
        mov cl, al
        lodsb                       ; ModRM: 11 reg rm
        movzx edx, al
        shr edx, 3
        and edx, 7                  ; registro de control o de depuración
        and eax, 7
        xor eax, 7                  ; PUSHAD: EAX es el más alto
        lea edi, [ebp+eax*4]        ; el registro general, en el marco
        cmp cl, 0x20
        je .rdcr
        cmp cl, 0x22
        je .wrcr
        cmp cl, 0x21
        je .rddr
        ; MOV DRn, r32
        mov eax, [edi]
        cmp dl, 0
        jne .w1
        mov dr0, eax
.w1:
        cmp dl, 1
        jne .w2
        mov dr1, eax
.w2:
        cmp dl, 2
        jne .w3
        mov dr2, eax
.w3:
        cmp dl, 3
        jne .w6
        mov dr3, eax
.w6:
        cmp dl, 6
        jne .w7
        mov dr6, eax
.w7:
        cmp dl, 7
        jne .done
        mov dr7, eax
        jmp .done
.rddr:
        xor eax, eax
        cmp dl, 0
        jne .r1
        mov eax, dr0
.r1:
        cmp dl, 1
        jne .r2
        mov eax, dr1
.r2:
        cmp dl, 2
        jne .r3
        mov eax, dr2
.r3:
        cmp dl, 3
        jne .r6
        mov eax, dr3
.r6:
        cmp dl, 6
        jne .r7
        mov eax, dr6
.r7:
        cmp dl, 7
        jne .rs
        mov eax, dr7
.rs:
        mov [edi], eax
        jmp .done
.rdcr:
        xor eax, eax
        cmp dl, 0
        jne .c2
        mov eax, cr0
.c2:
        cmp dl, 2
        jne .c3
        mov eax, cr2
.c3:
        cmp dl, 3
        jne .rs
        mov eax, cr3
        jmp .rs
.wrcr:
        ; CR0: solo los bits del coprocesador y de la caché; CR3: nada
        ; (vaciar la TLB); el resto se pasa por alto
        cmp dl, 0
        jne .wc3
        mov eax, [edi]
        test al, 1
        jnz .wc0
        ; quita PE: es la BIOS reiniciando el equipo
        cmp word [ebp+F_CS], 0xF000
        jae reset_machine
.wc0:
        and eax, 0x6000002E
        mov ecx, cr0
        and ecx, ~0x6000002E
        or eax, ecx
        mov cr0, eax
        jmp .done
.wc3:
        cmp dl, 3
        jne .done
        mov eax, cr3
        mov cr3, eax
        jmp .done

; saltar un ModRM de 16 bits con su desplazamiento (ESI apunta al ModRM)
.skip_modrm:
        lodsb
        mov ah, al
        shr ah, 6
        cmp ah, 3
        je .sm9
        cmp ah, 1
        jne .sm1
        inc esi
        ret
.sm1:
        cmp ah, 2
        jne .sm2
        add esi, 2
        ret
.sm2:
        and al, 7
        cmp al, 6
        jne .sm9
        add esi, 2
.sm9:
        ret

; -----------------------------------------------------------------------------
;  Puertos virtualizados.  EDX = puerto; AL = dato. Conservan EBX, ECX, EDX,
;  ESI y EBP.
; -----------------------------------------------------------------------------
port_in:
        cmp edx, 0x60
        je .p60
        cmp edx, 0x92
        je .p92
        call dma_port
        jnc dma_in
        in al, dx
        ret
.p92:
        in al, 0x92
.a20bit:
        and al, ~2
        cmp byte [p_a20], 0
        je .r
        or al, 2
.r:
        ret
.p60:
        in al, 0x60
        cmp byte [kbc_state], 2     ; respuesta a D0h: el puerto de salida
        jne .r
        mov byte [kbc_state], 0
        jmp .a20bit

port_out:
        cmp edx, 0x60
        je .p60
        cmp edx, 0x64
        je .p64
        cmp edx, 0x92
        je .p92
        cmp edx, 0x43
        je .p43
        call sb_dma_ctl             ; [SB] modo y máscara de los canales cedidos
        call dma_port
        jnc dma_out
        out dx, al
        ret
; 8254: los modos 6 y 7 son el 2 y el 3 (así lo dice su hoja de datos), pero
; hay emuladores (QEMU, por ejemplo) que no lo saben y dejan el temporizador
; parado: se le da el modo con su número canónico
.p43:
        push eax
        mov ah, al
        and ah, 0xC0
        cmp ah, 0xC0                ; orden de relectura
        je .p43w
        test al, 0x30               ; orden de captura del contador
        jz .p43w
        test al, 0x04               ; modos 2, 3, 6 y 7
        jz .p43w
        and al, ~0x08
.p43w:
        out 0x43, al
        pop eax
        ret
.p92:
        push eax
        shr al, 1
        and al, 1
        call a20_set
        pop eax
        or al, 2                    ; la de verdad, siempre abierta
        out 0x92, al
        ret
.p60:
        cmp byte [kbc_state], 1     ; dato de D1h: puerto de salida del 8042
        jne .raw60
        mov byte [kbc_state], 0
        push eax
        shr al, 1
        and al, 1
        call a20_set
        pop eax
        or al, 2
.raw60:
        out 0x60, al
        ret
.p64:
        mov byte [kbc_state], 0
        cmp al, 0xD1
        jne .k1
        mov byte [kbc_state], 1
.k1:
        cmp al, 0xD0
        jne .k2
        mov byte [kbc_state], 2
.k2:
        cmp al, 0xDF                ; órdenes directas de A20
        je .kon
        cmp al, 0xDD
        je .koff
        out 0x64, al
        ret
.kon:
        mov al, 1
        jmp a20_set
.koff:
        xor al, al
; AL = 1 abrir / 0 cerrar la línea A20 virtual: con la línea cerrada, el mega
; 17 es un espejo del primero (la HMA no se ve)
a20_set:
        cmp al, [p_a20]
        je .r
        mov [p_a20], al
        push eax
        push ecx
        push edi
        mov edi, PT0_LIN + 0x100*4
        mov eax, 0x100007
        cmp byte [p_a20], 0
        jne .f
        mov eax, 7
.f:
        mov ecx, 16
.l:
        mov [edi], eax
        add eax, 0x1000
        add edi, 4
        loop .l
        mov eax, cr3
        mov cr3, eax
        pop edi
        pop ecx
        pop eax
.r:
        ret

; ---- DMA (8237) ----
; Los programas escriben direcciones del primer mega; si caen en memoria
; superior o en el marco EMS (páginas que apuntan a otra RAM), al controlador
; se le da la dirección física verdadera.
; EDX = puerto -> CF = 0 si es de DMA, con EDI = canal (0-7) y dma_class:
; 0 dirección, 1 cuenta, 2 página, 3 biestable/reinicio
dma_port:
        push eax
        mov eax, edx
        cmp eax, 8
        jb .low
        cmp eax, 0x0C
        je .ff8
        cmp eax, 0x0D
        je .ff8
        cmp eax, 0xD8
        je .ff16
        cmp eax, 0xDA
        je .ff16
        cmp eax, 0xC0
        jb .page
        cmp eax, 0xCE
        ja .no
        test al, 1
        jnz .no
        sub eax, 0xC0
        shr eax, 1
        mov edi, eax
        shr edi, 1
        add edi, 4
        and al, 1
        jmp .yes
.low:
        mov edi, eax
        shr edi, 1
        and al, 1
        jmp .yes
.ff8:
        xor edi, edi
        mov al, 3
        jmp .yes
.ff16:
        mov edi, 4
        mov al, 3
        jmp .yes
.page:
        xor edi, edi
.pl:
        cmp al, [dma_pgport+edi]
        je .pg
        inc edi
        cmp edi, 8
        jb .pl
.no:
        pop eax
        stc
        ret
.pg:
        mov al, 2
.yes:
        mov [dma_class], al
        pop eax
        clc
        ret
dma_pgport  db 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A

; biestable del canal EDI -> EAX = dirección de la variable
%macro DMAFF 0
        mov eax, dma_ff
        cmp edi, 4
        jb %%a
        inc eax
%%a:
%endmacro

dma_out:
        push ebx
        push ecx
        push esi
        mov cl, al
        cmp byte [dma_class], 3
        je .ffw
        cmp byte [dma_class], 2
        je .pagew
        DMAFF
        mov esi, eax
        mov bl, [esi]               ; 0 = byte bajo, 1 = alto
        xor byte [esi], 1
        mov al, cl
        out dx, al
        cmp byte [dma_class], 1
        je .cnt                     ; [SB] (antes: je .r)
        movzx eax, bl
        lea eax, [eax+edi*2]
        mov [dma_addr+eax], cl
        test bl, bl
        jz .r
        call dma_fix
        call sb_note_addr           ; [SB]
        jmp .r
.cnt:                               ; [SB] la cuenta también se anota
        movzx eax, bl               ; [SB]
        lea eax, [eax+edi*2]        ; [SB]
        mov [dma_cnt+eax], cl       ; [SB]
        test bl, bl                 ; [SB]
        jz .r                       ; [SB]
        call sb_note_cnt            ; [SB]
        jmp .r                      ; [SB]
.pagew:
        mov [dma_page+edi], cl
        out dx, al
        call dma_fix
        call sb_note_addr           ; [SB]
        jmp .r
.ffw:
        DMAFF
        mov byte [eax], 0
        mov al, cl
        out dx, al
.r:
        mov al, cl
        pop esi
        pop ecx
        pop ebx
        ret

dma_in:
        push ebx
        push ecx
        push esi
        cmp byte [dma_class], 2
        jae .raw
        DMAFF
        mov esi, eax
        mov bl, [esi]
        xor byte [esi], 1
        ; [SB] canal cedido a la Sound Blaster emulada: la dirección y la
        ; cuenta «actuales» las lleva HSPSND en su estructura
        mov ecx, [sb_dmaptr+edi*4]  ; [SB]
        test ecx, ecx               ; [SB]
        jz .nsb                     ; [SB]
        in al, dx                   ; [SB] (el biestable de verdad, a la par)
        movzx ebx, bl               ; [SB]
        cmp byte [dma_class], 1     ; [SB]
        jne .sba                    ; [SB]
        add ecx, 2                  ; [SB]
.sba:                               ; [SB]
        mov al, [ecx+ebx+SBD_CURADDR] ; [SB]
        jmp .r                      ; [SB]
.nsb:                               ; [SB]
        cmp byte [dma_class], 1
        je .raw
        cmp dword [dma_delta+edi*4], 0
        je .raw
        ; dirección de un canal traducido: se devuelve la que el programa
        ; espera (la física menos el desplazamiento)
        test bl, bl
        jnz .hi
        in al, dx                   ; bajo y alto de una vez
        mov cl, al
        in al, dx
        mov ch, al
        mov eax, [dma_delta+edi*4]
        cmp edi, 4
        jb .d8
        shr eax, 1
.d8:
        sub cx, ax
        mov [dma_latch+edi*2], cx
        in al, dx                   ; deja el biestable de verdad en «alto»
        mov al, cl
        jmp .r
.hi:
        in al, dx
        mov al, [dma_latch+edi*2+1]
        jmp .r
.raw:
        in al, dx
.r:
        pop esi
        pop ecx
        pop ebx
        ret

; recalcular el canal EDI tras escribir su dirección o su página
dma_fix:
        pushad
        movzx eax, word [dma_addr+edi*2]
        movzx ecx, byte [dma_page+edi]
        cmp edi, 4
        jb .a8
        shl eax, 1
        and cl, 0xFE
.a8:
        shl ecx, 16
        add eax, ecx                ; dirección lineal que quiere el programa
        mov dword [dma_delta+edi*4], 0
        cmp eax, 0xA0000
        jb .r
        cmp eax, 0x100000
        jae .r
        mov ecx, eax
        shr ecx, 12
        mov ecx, [PT0_LIN+ecx*4]
        and ecx, ~0xFFF
        mov ebx, eax
        and ebx, 0xFFF
        or ecx, ebx                 ; dirección física
        cmp ecx, eax
        je .r
        cmp ecx, 0x1000000          ; el DMA del ISA no pasa de 16 MB
        jae .r
        mov ebx, ecx                ; EBX = física
        sub ecx, eax
        mov [dma_delta+edi*4], ecx
        ; escribirla en el controlador
        cmp edi, 4
        jae .w16
        xor al, al
        out 0x0C, al                ; biestable a «bajo»
        lea edx, [edi*2]
        mov al, bl
        out dx, al
        mov al, bh
        out dx, al
        movzx edx, byte [dma_pgport+edi]
        mov eax, ebx
        shr eax, 16
        out dx, al
        cmp byte [dma_ff], 0
        je .r
        lea edx, [edi*2]
        in al, dx
        jmp .r
.w16:
        xor al, al
        out 0xD8, al
        lea edx, [edi*4+0xC0-16]
        mov eax, ebx
        shr eax, 1
        out dx, al
        mov al, ah
        out dx, al
        movzx edx, byte [dma_pgport+edi]
        mov eax, ebx
        shr eax, 16
        and al, 0xFE
        out dx, al
        cmp byte [dma_ff+1], 0
        je .r
        lea edx, [edi*4+0xC0-16]
        in al, dx
.r:
        popad
        ret

; =============================================================================
;  [SB] Apoyo a la emulación de Sound Blaster de HSPSND (HSPSND /SB).
;  El monitor solo atrapa: la emulación (DSP, mezclador, FM, salida por HD
;  Audio o AC'97) está en HSPSND, en modo V86. Servicios (INT 67h, AH=DDh):
;    AL=00h  ¿está? -> AH=0, BX=5342h («SB»), CX=versión de la interfaz
;    AL=01h  vigilar puertos: DS:SI = pares (primero, último) acabados en
;            FFFFh (8 como mucho), ES:BX = rutina de HSPSND (0:0 = ninguna).
;            Cada IN/OUT de un programa a esos puertos se convierte en una
;            llamada a la rutina, como una interrupción: en la pila del
;            programa quedan [SP]=datos (bit 0: OUT; bit 1: 16 bits; bit 2:
;            32 bits), [SP+2]=puerto y después IP, CS e indicadores (la
;            rutina quita las dos palabras y acaba con IRET). El valor va en
;            AL/AX/EAX (el del programa en OUT; en IN lo pone la rutina). La
;            E/S hecha desde el segmento de la rutina no se desvía.
;    AL=02h  mapear memoria física: BX = primera página lineal (por debajo
;            de 100h), CX = páginas, EDX = dirección física; con el bit 0 a
;            uno es RAM (con caché: para deshacerlo, con la dirección que
;            daba antes VCPI DE06h) y si no, registros de un dispositivo
;            (sin caché). Para los registros del HD Audio.
;    AL=03h  ceder un canal de DMA: BL = canal, ES:DI = estructura (0:0 =
;            devolverlo). El monitor anota en ella lo que el programa
;            escribe en el 8237 y contesta con ella sus lecturas:
;              +0  dd dirección lineal programada (página y dirección)
;              +4  dw cuenta programada      +6 db modo (puerto 0Bh/D6h)
;              +8  dw dirección actual (tal cual se lee del registro)
;              +10 dw cuenta actual          +12 db 1 = canal tapado
;    AL=04h  [SB-PM] puertos que atrapar a los clientes DPMI (HSPDPMI):
;            ES:DI = 13 pares (primero, último) + FFFFh; CX = cuántos;
;            EDX = lo dado con AL=05h; [SB-VGA] BX = sus IRQ
;    AL=05h  [SB-PM] EDX = dirección lineal del octeto de estado de la FM
;            (HSPSND, después de AL=01h; 0 = ninguno): HSPDPMI contesta
;            con él las lecturas del estado de la AdLib sin ir a modo V86.
;            [SB-VGA] Con BX=5352h, CX = las IRQ que usa HSPSND (la de la
;            tarjeta y la 8 si tira del RTC): HSPDPMI las lleva directas a
;            modo real, sin pasar por los manejadores de los clientes
; =============================================================================
SBD_BASE    equ 0
SBD_COUNT   equ 4
SBD_MODE    equ 6
SBD_CURADDR equ 8
SBD_CURCNT  equ 10
SBD_MASK    equ 12

sb_svc:
        movzx eax, byte [ebp+P_EAX]
        test al, al
        jz .chk
        cmp al, 1
        je .trap
        cmp al, 2
        je .map
        cmp al, 3
        je .dma
        cmp al, 4                   ; [SB-PM]
        je sb_pmlist                ; [SB-PM]
        cmp al, 5                   ; [SB-PM]
        je .fmst                    ; [SB-PM]
        mov ah, 0x8F
        ret
.chk:
        mov word [ebp+P_EBX], 0x5342
        mov word [ebp+P_ECX], 0x0100
        xor ah, ah
        ret
.fmst:                              ; [SB-PM]
        mov eax, [ebp+P_EDX]        ; [SB-PM]
        mov [sb_fmstat], eax        ; [SB-PM]
        mov word [sb_irqres], 0     ; [SB-VGA] y, con BX=5352h, en CX las
        cmp word [ebp+P_EBX], 0x5352 ; [SB-VGA] IRQ que son de HSPSND
        jne .fm2                    ; [SB-VGA]
        mov ax, [ebp+P_ECX]         ; [SB-VGA]
        mov [sb_irqres], ax         ; [SB-VGA]
.fm2:                               ; [SB-VGA]
        xor ah, ah                  ; [SB-PM]
        ret                         ; [SB-PM]
.trap:
        mov dword [sb_handler], 0
        mov dword [sb_fmstat], 0    ; [SB-PM]
        mov word [sb_irqres], 0     ; [SB-VGA]
        V86PTR esi, F_DS, P_ESI
        mov edi, sb_ranges
        mov ecx, 8
.tl:
        movzx eax, word [esi]
        cmp ax, 0xFFFF
        je .te
        movzx edx, word [esi+2]
        mov [edi], ax
        mov [edi+2], dx
.tb:
        bts [tss+104], eax
        inc eax
        cmp eax, edx
        jbe .tb
        add esi, 4
        add edi, 4
        loop .tl
.te:
        mov word [edi], 0xFFFF
        movzx eax, word [ebp+F_ES]
        shl eax, 16
        mov ax, [ebp+P_EBX]
        mov [sb_handler], eax
        xor ah, ah
        ret
.map:
        movzx ebx, word [ebp+P_EBX]
        movzx ecx, word [ebp+P_ECX]
        lea eax, [ebx+ecx]
        cmp eax, 0x100
        ja .bad
        jecxz .bad
        mov edx, [ebp+P_EDX]
.ml:
        mov eax, edx
        and eax, ~0xFFF
        or eax, 7                   ; presente, escritura, usuario
        test dl, 1
        jnz .mw
        or eax, 0x18                ; registros: sin caché
.mw:
        add edx, 0x1000
        mov [PT0_LIN+ebx*4], eax
        inc ebx
        loop .ml
        mov byte [need_flush], 1
        xor ah, ah
        ret
.bad:
        mov ah, 0x8F
        ret
.dma:
        movzx ebx, byte [ebp+P_EBX]
        and ebx, 7
        V86PTR edi, F_ES, P_EDI
        test edi, edi
        mov [sb_dmaptr+ebx*4], edi
        jz .dok
        mov word [edi+SBD_MODE], 0
        mov byte [edi+SBD_MASK], 0
        mov edi, ebx
        call sb_note_addr
        call sb_note_cnt
.dok:
        xor ah, ah
        ret

; [SB-PM] AL=04h: puertos que hay que atrapar también a los programas en modo
; protegido (los clientes de HSPDPMI, en anillo 3): ES:DI = sitio para 13
; pares (primero, último), que se devuelven acabados en FFFFh; CX = cuántos
; (0: no hay emulación). Son los que vigila HSPSND y, si tiene cedido algún
; canal de DMA, los del 8237 que virtualiza el monitor (para que la E/S se
; haga en modo V86 y pase por aquí como la de los programas de modo real).
sb_pmlist:
        V86PTR edi, F_ES, P_EDI
        xor ecx, ecx
        cmp dword [sb_handler], 0
        je .end
        mov esi, sb_ranges
.l:
        mov eax, [esi]
        cmp ax, 0xFFFF
        je .dma
        mov [edi+ecx*4], eax
        inc ecx
        add esi, 4
        jmp .l
.dma:
        mov eax, [sb_dmaptr]
        or eax, [sb_dmaptr+4]
        or eax, [sb_dmaptr+8]
        or eax, [sb_dmaptr+12]
        jz .d16
        mov dword [edi+ecx*4], 0x000F0000       ; 00h-0Fh
        mov dword [edi+ecx*4+4], 0x00830081     ; 81h-83h
        mov dword [edi+ecx*4+8], 0x00870087     ; 87h
        add ecx, 3
.d16:
        mov eax, [sb_dmaptr+16]
        or eax, [sb_dmaptr+20]
        or eax, [sb_dmaptr+24]
        or eax, [sb_dmaptr+28]
        jz .end
        mov dword [edi+ecx*4], 0x00DF00C0       ; C0h-DFh
        mov dword [edi+ecx*4+4], 0x008B0089     ; 89h-8Bh
        add ecx, 2
.end:
        mov word [edi+ecx*4], 0xFFFF
        mov [ebp+P_ECX], cx
        mov eax, [sb_fmstat]
        mov [ebp+P_EDX], eax
        mov ax, [sb_irqres]         ; [SB-VGA] IRQ de HSPSND (ver AL=05h)
        mov [ebp+P_EBX], ax         ; [SB-VGA]
        xor ah, ah
        ret

; el programa ha escrito la dirección (o la página) del canal EDI
sb_note_addr:
        push eax
        push ecx
        push esi
        mov esi, [sb_dmaptr+edi*4]
        test esi, esi
        jz .r
        movzx eax, word [dma_addr+edi*2]
        mov [esi+SBD_CURADDR], ax
        movzx ecx, byte [dma_page+edi]
        cmp edi, 4
        jb .a8
        shl eax, 1
        and cl, 0xFE
.a8:
        shl ecx, 16
        add eax, ecx
        mov [esi+SBD_BASE], eax
.r:
        pop esi
        pop ecx
        pop eax
        ret
; ... o la cuenta
sb_note_cnt:
        push eax
        push esi
        mov esi, [sb_dmaptr+edi*4]
        test esi, esi
        jz .r
        mov ax, [dma_cnt+edi*2]
        mov [esi+SBD_COUNT], ax
        mov [esi+SBD_CURCNT], ax
.r:
        pop esi
        pop eax
        ret

; OUT a los registros de modo (0Bh, D6h) y de máscara (0Ah, 0Fh, D4h, DEh):
; se anota en los canales cedidos y sigue su camino. Conserva todo.
sb_dma_ctl:
        push ebx
        push ecx
        push esi
        xor ebx, ebx
        cmp edx, 0x0B
        je .mode
        cmp edx, 0x0A
        je .mask1
        cmp edx, 0x0F
        je .maskall
        mov ebx, 4
        cmp edx, 0xD6
        je .mode
        cmp edx, 0xD4
        je .mask1
        cmp edx, 0xDE
        je .maskall
        jmp .r
.mode:
        movzx ecx, al
        and ecx, 3
        add ecx, ebx
        mov esi, [sb_dmaptr+ecx*4]
        test esi, esi
        jz .r
        mov [esi+SBD_MODE], al
        jmp .r
.mask1:
        movzx ecx, al
        and ecx, 3
        add ecx, ebx
        mov esi, [sb_dmaptr+ecx*4]
        test esi, esi
        jz .r
        mov cl, al
        shr cl, 2
        and cl, 1
        mov [esi+SBD_MASK], cl
        jmp .r
.maskall:
        mov ch, al
        mov cl, 4
.ma:
        mov esi, [sb_dmaptr+ebx*4]
        test esi, esi
        jz .mn
        push eax
        mov al, ch
        and al, 1
        mov [esi+SBD_MASK], al
        pop eax
.mn:
        shr ch, 1
        inc ebx
        dec cl
        jnz .ma
.r:
        pop esi
        pop ecx
        pop ebx
        ret

; E/S a un puerto atrapado: ¿es de los que vigila HSPSND? AL = código de la
; instrucción (bit 0: palabra), AH = 1 si es OUT, BL bit 0 = operando de 32
; bits, EDX = puerto, ESI = tras la instrucción. Si lo es, el programa sigue
; en la rutina de HSPSND (y esta llamada no vuelve); si no, conserva todo
; menos AH.
sb_upcall:
        cmp dword [sb_handler], 0
        je .no
        push ecx
        mov ecx, [sb_handler]
        shr ecx, 16
        cmp cx, [ebp+F_CS]
        pop ecx
        je .no
        push esi
        mov esi, sb_ranges
.l:
        cmp word [esi], 0xFFFF
        je .nor
        cmp dx, [esi]
        jb .n
        cmp dx, [esi+2]
        jbe .hit
.n:
        add esi, 4
        jmp .l
.nor:
        pop esi
.no:
        ret
.hit:
        pop esi
        add esp, 4                  ; no se vuelve a gp_fault
        sub esi, [gp_base]
        movzx ecx, ah
        test al, 1
        jz .b
        or cl, 2
        test bl, 1
        jz .b
        xor cl, 6
.b:
        movzx edi, word [ebp+F_SS]
        shl edi, 4
        mov eax, [ebp+F_ESP]
        sub ax, 10
        mov [ebp+F_ESP], ax
        movzx eax, ax
        add edi, eax
        mov [edi], cx
        mov [edi+2], dx
        mov [edi+4], si
        mov ax, [ebp+F_CS]
        mov [edi+6], ax
        mov ax, [ebp+F_EFL]
        mov [edi+8], ax
        and dword [ebp+F_EFL], ~0x0300
        mov eax, [sb_handler]
        movzx ecx, ax
        mov [ebp+F_EIP], ecx
        shr eax, 16
        mov [ebp+F_CS], eax
        jmp mon_exit
; ======================== [SB] fin del apoyo a HSPSND /SB =====================

; -----------------------------------------------------------------------------
;  INT 15h AH=87h: copiar CX palabras entre direcciones físicas
;  (ES:SI = tabla de descriptores; origen en +10h y destino en +18h)
; -----------------------------------------------------------------------------
svc_i15:
        V86PTR ebx, F_ES, P_ESI
        movzx ecx, word [ebp+P_ECX]
        shl ecx, 1
        mov esi, [ebx+0x12]
        and esi, 0x00FFFFFF
        movzx eax, byte [ebx+0x17]
        shl eax, 24
        or esi, eax
        mov edi, [ebx+0x1A]
        and edi, 0x00FFFFFF
        movzx eax, byte [ebx+0x1F]
        shl eax, 24
        or edi, eax
        call phys_copy
        mov byte [ebp+P_EAX+1], 0
        and dword [ebp+F_EFL], ~1
        or dword [ebp+F_EFL], 0x40
        ret

; copiar ECX bytes de la dirección física ESI a la EDI. Las direcciones del
; primer mega son las que ve el programa (con sus UMB y su marco EMS).
phys_copy:
.loop:
        test ecx, ecx
        jz .done
        mov edx, ecx
        cmp edx, 0x10000
        jbe .c
        mov edx, 0x10000
.c:
        push ecx
        mov eax, esi
        mov ebx, PT1_LIN + ((WIN_SRC >> 12) & 0x3FF) * 4
        call map_win
        mov eax, edi
        mov ebx, PT1_LIN + ((WIN_DST >> 12) & 0x3FF) * 4
        call map_win
        mov eax, cr3
        mov cr3, eax
        push esi
        push edi
        and esi, 0xFFF
        add esi, WIN_SRC
        and edi, 0xFFF
        add edi, WIN_DST
        mov ecx, edx
        shr ecx, 2
        rep movsd
        mov ecx, edx
        and ecx, 3
        rep movsb
        pop edi
        pop esi
        add esi, edx
        add edi, edx
        pop ecx
        sub ecx, edx
        jmp .loop
.done:
        ret

; EAX = dirección, EBX -> primera entrada de la ventana (17 páginas)
map_win:
        push ecx
        push edx
        and eax, ~0xFFF
        mov ecx, 17
.l:
        mov edx, eax
        cmp eax, 0x100000
        jae .id
        shr edx, 12
        mov edx, [PT0_LIN+edx*4]
        and edx, ~0xFFF
.id:
        or edx, 3
        mov [ebx], edx
        add ebx, 4
        add eax, 0x1000
        loop .l
        pop edx
        pop ecx
        ret

; -----------------------------------------------------------------------------
;  EMS 4.0 (INT 67h). Los registros del programa están en el marco (EBP).
; -----------------------------------------------------------------------------
ems_dispatch:
        mov byte [need_flush], 0
        movzx eax, byte [ebp+P_EAX+1]
        cmp al, 0xDE
        jne .ems
        call vcpi_v86
        jmp .done
.ems:
        cmp al, 0xDD                ; [SB] servicios para HSPSND /SB
        jne .ems1                   ; [SB]
        call sb_svc                 ; [SB]
        jmp .done                   ; [SB]
.ems1:                              ; [SB]
        cmp dword [p_framepg], 0
        je .undef
        sub al, 0x40
        cmp al, 0x1D
        ja .undef
        call [ems_table+eax*4]
.done:
        mov [ebp+P_EAX+1], ah
        cmp byte [need_flush], 0
        je .r
        mov eax, cr3
        mov cr3, eax
.r:
        ret
.undef:
        mov ah, 0x84
        jmp .done

ems_table   dd f40, f41, f42, f43, f44, f45, f46, f47
            dd f48, e84, e84, f4b, f4c, f4d, f4e, f4f
            dd f50, f51, f52, f53, f54, e84, e84, f57
            dd f58, f59, f5a, f5b, f40, f5d

e84:
        mov ah, 0x84
        ret
e8f:
        mov ah, 0x8F
        ret

; DX del programa -> ESI = handle abierto; CF y AH = 83h si no vale
get_handle:
        movzx esi, word [ebp+P_EDX]
get_handle_esi:
        cmp esi, NHAND
        jae .bad
        shl esi, 5
        add esi, handles
        test byte [esi+H_FLAGS], 1
        jz .bad
        clc
        ret
.bad:
        mov ah, 0x83
        stc
        ret

f40:
        xor ah, ah
        ret
f41:
        mov eax, [p_framepg]
        shl eax, 8
        mov [ebp+P_EBX], ax
        xor ah, ah
        ret
f42:
        mov eax, [nfree_end]
        sub eax, [ems_used]
        mov [ebp+P_EBX], ax
        mov eax, [p_poolpg]
        mov [ebp+P_EDX], ax
        xor ah, ah
        ret
f43:
        movzx ebx, word [ebp+P_EBX]
        test ebx, ebx
        jnz alloc_h
        mov ah, 0x89
        ret
f5a:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        movzx ebx, word [ebp+P_EBX]
alloc_h:
        cmp ebx, [p_poolpg]
        ja .e87
        mov eax, [nfree_end]
        sub eax, [ems_used]
        cmp ebx, eax
        ja .e88
        mov esi, handles + H_LEN
        mov ecx, 1
.l:
        test byte [esi+H_FLAGS], 1
        jz .got
        add esi, H_LEN
        inc ecx
        cmp ecx, NHAND
        jb .l
        mov ah, 0x85
        ret
.got:
        push ecx
        mov edi, esi
        xor eax, eax
        mov ecx, H_LEN / 4
        rep stosd
        pop ecx
        mov byte [esi+H_FLAGS], 1
        mov eax, [ems_used]
        mov [esi+H_START], ax
        mov [esi+H_COUNT], bx
        add [ems_used], ebx
        mov [ebp+P_EDX], cx
        xor ah, ah
        ret
.e87:
        mov ah, 0x87
        ret
.e88:
        mov ah, 0x88
        ret

f44:
        call get_handle
        jc .r
        movzx ecx, byte [ebp+P_EAX]
        movzx edx, word [ebp+P_EDX]
        mov bx, [ebp+P_EBX]
        call map_core
.r:
        ret

; ECX = página física (0-3), EDX = handle, BX = página lógica (FFFFh: quitar)
; -> AH = estado. Cambia EAX, ESI y EDI.
map_core:
        cmp ecx, 4
        jae .e8b
        cmp bx, 0xFFFF
        je .unmap
        mov esi, edx
        call get_handle_esi
        jc .r
        cmp bx, [esi+H_COUNT]
        jae .e8a
        movzx eax, word [esi+H_START]
        movzx edi, bx
        add eax, edi
        movzx eax, word [pagemap+eax*2]
        shl eax, 14
        add eax, [p_poolphys]
        mov [curmap+ecx*4], dx
        mov [curmap+ecx*4+2], bx
.set:
        mov edi, [p_framepg]
        lea edi, [edi+ecx*4]
        lea edi, [PT0_LIN+edi*4]
        or eax, 7
        mov [edi], eax
        add eax, 0x1000
        mov [edi+4], eax
        add eax, 0x1000
        mov [edi+8], eax
        add eax, 0x1000
        mov [edi+12], eax
        mov byte [need_flush], 1
        xor ah, ah
.r:
        ret
.unmap:
        mov dword [curmap+ecx*4], -1
        mov eax, ecx
        shl eax, 14
        add eax, [p_defphys]
        jmp .set
.e8a:
        mov ah, 0x8A
        ret
.e8b:
        mov ah, 0x8B
        ret

; poner el mapa de 16 bytes de ESI (4 x handle, página); lo que ya no exista
; queda sin mapear
set_map16:
        xor ecx, ecx
.l:
        movzx edx, word [esi+ecx*4]
        mov bx, [esi+ecx*4+2]
        cmp dx, 0xFFFF
        jne .m
        mov bx, 0xFFFF
.m:
        push esi
        call map_core
        test ah, ah
        jz .n
        mov bx, 0xFFFF
        call map_core
.n:
        pop esi
        inc ecx
        cmp ecx, 4
        jb .l
        xor ah, ah
        ret

f45:
        call get_handle
        jc .r
        mov ah, 0x86
        test byte [esi+H_FLAGS], 2
        jnz .r
        xor ebx, ebx
        call ems_resize
        cmp esi, handles
        je .ok                      ; el handle 0 no se cierra
        mov byte [esi+H_FLAGS], 0
.ok:
        xor ah, ah
.r:
        ret
f46:
        mov byte [ebp+P_EAX], 0x40
        xor ah, ah
        ret
f47:
        call get_handle
        jc .r
        mov ah, 0x8D
        test byte [esi+H_FLAGS], 2
        jnz .r
        or byte [esi+H_FLAGS], 2
        lea edi, [esi+H_SAVE]
        mov esi, curmap
        mov ecx, 4
        rep movsd
        xor ah, ah
.r:
        ret
f48:
        call get_handle
        jc .r
        mov ah, 0x8E
        test byte [esi+H_FLAGS], 2
        jz .r
        and byte [esi+H_FLAGS], ~2
        add esi, H_SAVE
        call set_map16
.r:
        ret
f4b:
        xor ebx, ebx
        mov esi, handles
        mov ecx, NHAND
.l:
        test byte [esi+H_FLAGS], 1
        jz .n
        inc ebx
.n:
        add esi, H_LEN
        loop .l
        mov [ebp+P_EBX], bx
        xor ah, ah
        ret
f4c:
        call get_handle
        jc .r
        mov ax, [esi+H_COUNT]
        mov [ebp+P_EBX], ax
        xor ah, ah
.r:
        ret
f4d:
        V86PTR edi, F_ES, P_EDI
        xor ebx, ebx
        xor ecx, ecx
        mov esi, handles
.l:
        test byte [esi+H_FLAGS], 1
        jz .n
        mov [edi], cx
        mov ax, [esi+H_COUNT]
        mov [edi+2], ax
        add edi, 4
        inc ebx
.n:
        add esi, H_LEN
        inc ecx
        cmp ecx, NHAND
        jb .l
        mov [ebp+P_EBX], bx
        xor ah, ah
        ret

; 4Eh: mapa de páginas completo (16 bytes)
f4e:
        mov al, [ebp+P_EAX]
        cmp al, 3
        ja e8f
        je .size
        cmp al, 0
        je .get
        ; poner (1) o leer y poner (2): primero se copia el mapa nuevo
        V86PTR esi, F_DS, P_ESI
        mov edi, tmp16
        mov ecx, 4
        rep movsd
        cmp al, 1
        je .set
        call .get
.set:
        mov esi, tmp16
        jmp set_map16
.get:
        V86PTR edi, F_ES, P_EDI
        mov esi, curmap
        mov ecx, 4
        rep movsd
        xor ah, ah
        ret
.size:
        mov byte [ebp+P_EAX], 16
        xor ah, ah
        ret

; EAX = segmento -> ECX = página física del marco; CF si no lo es
seg2phys:
        mov ecx, [p_framepg]
        shl ecx, 8
        sub eax, ecx
        jb .bad
        test eax, 0x3FF
        jnz .bad
        shr eax, 10
        cmp eax, 4
        jae .bad
        mov ecx, eax
        clc
        ret
.bad:
        stc
        ret

; 4Fh: mapa parcial. Lista: dw n, n x dw segmento. Mapa guardado: dw n,
; n x (dw segmento, dw handle, dw página).
f4f:
        mov al, [ebp+P_EAX]
        cmp al, 2
        ja e8f
        je .size
        V86PTR esi, F_DS, P_ESI
        cmp al, 1
        je .set
        V86PTR edi, F_ES, P_EDI
        movzx edx, word [esi]
        cmp edx, 4
        ja .e8b
        mov [edi], dx
        add esi, 2
        add edi, 2
.gl:
        test edx, edx
        jz .ok
        movzx eax, word [esi]
        mov [edi], ax
        call seg2phys
        jc .e8b
        mov eax, [curmap+ecx*4]
        mov [edi+2], eax
        add esi, 2
        add edi, 6
        dec edx
        jmp .gl
.set:
        movzx edx, word [esi]
        cmp edx, 4
        ja .ea3
        add esi, 2
.sl:
        test edx, edx
        jz .ok
        movzx eax, word [esi]
        call seg2phys
        jc .ea3
        push edx
        push esi
        movzx edx, word [esi+2]
        mov bx, [esi+4]
        cmp dx, 0xFFFF
        jne .sm
        mov bx, 0xFFFF
.sm:
        call map_core
        test ah, ah
        jz .sn
        mov bx, 0xFFFF
        call map_core
.sn:
        pop esi
        pop edx
        add esi, 6
        dec edx
        jmp .sl
.size:
        movzx eax, word [ebp+P_EBX]
        cmp eax, 4
        ja .e8b
        imul eax, eax, 6
        add eax, 2
        mov [ebp+P_EAX], al
.ok:
        xor ah, ah
        ret
.e8b:
        mov ah, 0x8B
        ret
.ea3:
        mov ah, 0xA3
        ret

; 50h: mapear varias páginas. DS:SI -> CX x (dw página lógica, dw página
; física o segmento)
f50:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        call get_handle
        jc .r
        V86PTR esi, F_DS, P_ESI
        movzx edx, word [ebp+P_ECX]
.l:
        xor ah, ah
        test edx, edx
        jz .r
        movzx ecx, word [esi+2]
        cmp byte [ebp+P_EAX], 0
        je .ph
        mov eax, ecx
        call seg2phys
        jc .e8b
.ph:
        mov bx, [esi]
        push edx
        push esi
        movzx edx, word [ebp+P_EDX]
        call map_core
        pop esi
        pop edx
        test ah, ah
        jnz .r
        add esi, 4
        dec edx
        jmp .l
.e8b:
        mov ah, 0x8B
.r:
        ret

f51:
        call get_handle
        jc .r
        movzx ebx, word [ebp+P_EBX]
        call ems_resize
        mov cx, [esi+H_COUNT]
        mov [ebp+P_EBX], cx
.r:
        ret
f52:
        mov al, [ebp+P_EAX]
        cmp al, 2
        ja e8f
        je .cap
        call get_handle
        jc .r
        cmp al, 0
        je .cap
        mov ah, 0x91                ; solo hay handles volátiles
        cmp byte [ebp+P_EBX], 0
        jne .r
        xor ah, ah
        ret
.cap:
        mov byte [ebp+P_EAX], 0
        xor ah, ah
.r:
        ret

; 53h: nombre del handle
f53:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        call get_handle
        jc .r
        cmp byte [ebp+P_EAX], 1
        je .set
        V86PTR edi, F_ES, P_EDI
        add esi, H_NAME
        movsd
        movsd
        xor ah, ah
.r:
        ret
.set:
        mov ebx, esi
        V86PTR esi, F_DS, P_ESI
        call find_name
        jc .put                     ; no hay otro con ese nombre (o es nulo)
        cmp edx, ebx
        je .put
        mov ah, 0xA1
        ret
.put:
        lea edi, [ebx+H_NAME]
        movsd
        movsd
        xor ah, ah
        ret

; ESI -> nombre de 8 bytes -> EDX = handle abierto que lo tiene; CF si
; ninguno o si el nombre es nulo. Conserva ESI y EBX.
find_name:
        mov eax, [esi]
        or eax, [esi+4]
        jz .no
        mov edx, handles
        mov ecx, NHAND
.l:
        test byte [edx+H_FLAGS], 1
        jz .n
        mov eax, [esi]
        cmp eax, [edx+H_NAME]
        jne .n
        mov eax, [esi+4]
        cmp eax, [edx+H_NAME+4]
        je .yes
.n:
        add edx, H_LEN
        loop .l
.no:
        stc
        ret
.yes:
        clc
        ret

; 54h: directorio de handles
f54:
        mov al, [ebp+P_EAX]
        cmp al, 2
        ja e8f
        je .total
        cmp al, 1
        je .search
        V86PTR edi, F_ES, P_EDI
        xor ebx, ebx
        xor ecx, ecx
        mov esi, handles
.l:
        test byte [esi+H_FLAGS], 1
        jz .n
        mov [edi], cx
        mov eax, [esi+H_NAME]
        mov [edi+2], eax
        mov eax, [esi+H_NAME+4]
        mov [edi+6], eax
        add edi, 10
        inc ebx
.n:
        add esi, H_LEN
        inc ecx
        cmp ecx, NHAND
        jb .l
        mov [ebp+P_EAX], bl
        xor ah, ah
        ret
.search:
        V86PTR esi, F_DS, P_ESI
        mov ah, 0xA1
        mov eax, [esi]
        or eax, [esi+4]
        jz .ea1
        call find_name
        jc .ea0
        sub edx, handles
        shr edx, 5
        mov [ebp+P_EDX], dx
        xor ah, ah
        ret
.ea0:
        mov ah, 0xA0
        ret
.ea1:
        mov ah, 0xA1
        ret
.total:
        mov word [ebp+P_EBX], NHAND
        xor ah, ah
        ret

; 58h: páginas físicas mapeables
f58:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        je .n
        V86PTR edi, F_ES, P_EDI
        mov eax, [p_framepg]
        shl eax, 8
        xor ecx, ecx
.l:
        mov [edi], ax
        mov [edi+2], cx
        add eax, 0x400
        add edi, 4
        inc ecx
        cmp ecx, 4
        jb .l
.n:
        mov word [ebp+P_ECX], 4
        xor ah, ah
        ret
; 59h: datos del hardware
f59:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        je .raw
        V86PTR edi, F_ES, P_EDI
        mov dword [edi], 0x00000400 ; página de 400h párrafos; sin juegos
        mov dword [edi+4], 0x00000010  ; de registros; contexto de 16 bytes
        mov word [edi+8], 0
        xor ah, ah
        ret
.raw:
        jmp f42
; 5Bh: juegos de registros alternativos (solo el 0, el de siempre)
f5b:
        mov al, [ebp+P_EAX]
        cmp al, 8
        ja e8f
        cmp al, 0
        je .get
        cmp al, 1
        je .set
        cmp al, 2
        je .size
        cmp al, 3
        je .zero
        cmp al, 5
        je .zero
        ; 4, 6, 7, 8: solo con BL = 0
        mov ah, 0x9C
        cmp byte [ebp+P_EBX], 0
        jne .r
        xor ah, ah
.r:
        ret
.zero:
        mov byte [ebp+P_EBX], 0
        xor ah, ah
        ret
.size:
        mov word [ebp+P_EDX], 16
        xor ah, ah
        ret
.get:
        mov byte [ebp+P_EBX], 0
        mov eax, [alt_ptr]
        mov [ebp+P_EDI], ax
        shr eax, 16
        mov [ebp+F_ES], eax
        ; el contexto actual se deja en la zona que dio el programa
        mov edi, [alt_ptr]
        test edi, edi
        jz .g0
        mov eax, edi
        shr eax, 16
        shl eax, 4
        movzx edi, di
        add edi, eax
        mov esi, curmap
        mov ecx, 4
        rep movsd
.g0:
        xor ah, ah
        ret
.set:
        mov ah, 0x9C
        cmp byte [ebp+P_EBX], 0
        jne .r
        mov eax, [ebp+F_ES]
        shl eax, 16
        mov ax, [ebp+P_EDI]
        mov [alt_ptr], eax
        xor ah, ah
        test eax, eax
        jz .r
        V86PTR esi, F_ES, P_EDI
        jmp set_map16
; 5Dh: funciones del sistema operativo (siempre permitidas)
f5d:
        cmp byte [ebp+P_EAX], 2
        ja e8f
        je f40
        cmp byte [os_key], 0
        jne f40
        mov byte [os_key], 1
        mov word [ebp+P_EBX], 0x4853
        mov word [ebp+P_ECX], 0x5045
        xor ah, ah
        ret

; ---- páginas de un handle ----
; pagemap es una permutación de las páginas: primero las de los handles (cada
; uno un tramo seguido), después las libres y al final las que usa VCPI.
; ESI = handle, EBX = páginas nuevas -> AH = estado
ems_resize:
        movzx eax, word [esi+H_COUNT]
        cmp ebx, eax
        je .ok
        ja .grow
        call rotate_end
        mov ecx, eax
        sub ecx, ebx
        sub [ems_used], ecx
        mov [esi+H_COUNT], bx
        ; lo que tuviera mapeado de las páginas que pierde, fuera
        mov edx, esi
        sub edx, handles
        shr edx, 5
        xor ecx, ecx
.um:
        cmp [curmap+ecx*4], dx
        jne .un
        mov ax, [curmap+ecx*4+2]
        cmp ax, [esi+H_COUNT]
        jb .un
        push esi
        push ebx
        mov bx, 0xFFFF
        call map_core
        pop ebx
        pop esi
.un:
        inc ecx
        cmp ecx, 4
        jb .um
.ok:
        xor ah, ah
        ret
.grow:
        cmp ebx, [p_poolpg]
        ja .e87
        mov ecx, ebx
        sub ecx, eax
        mov edx, [nfree_end]
        sub edx, [ems_used]
        cmp ecx, edx
        ja .e88
        call rotate_end
        add [ems_used], ecx
        mov [esi+H_COUNT], bx
        xor ah, ah
        ret
.e87:
        mov ah, 0x87
        ret
.e88:
        mov ah, 0x88
        ret

; llevar el tramo del handle ESI al final de la zona usada (tres inversiones)
; Conserva todos los registros.
rotate_end:
        pushad
        movzx ebx, word [esi+H_START]
        movzx ecx, word [esi+H_COUNT]
        test ecx, ecx
        jz .last
        lea eax, [ebx+ecx]
        cmp eax, [ems_used]
        je .done
        mov edx, eax                ; EBX = s, EDX = s + k
        mov eax, ebx
        call reverse
        mov eax, edx
        mov edx, [ems_used]
        call reverse
        mov eax, ebx
        call reverse
        ; los tramos que estaban detrás bajan k
        mov edi, handles
        mov edx, NHAND
.adj:
        cmp [edi+H_START], bx
        jbe .an
        sub [edi+H_START], cx
.an:
        add edi, H_LEN
        dec edx
        jnz .adj
        mov eax, [ems_used]
        sub eax, ecx
        mov [esi+H_START], ax
        jmp .done
.last:
        mov eax, [ems_used]
        mov [esi+H_START], ax
.done:
        popad
        ret
; invertir pagemap[EAX..EDX)
reverse:
        push eax
        push edx
        push ecx
        dec edx
.l:
        cmp eax, edx
        jae .r
        mov cx, [pagemap+eax*2]
        xchg cx, [pagemap+edx*2]
        mov [pagemap+eax*2], cx
        inc eax
        dec edx
        jmp .l
.r:
        pop ecx
        pop edx
        pop eax
        ret

; ---- 57h: mover o intercambiar una zona de memoria ----
; DS:SI -> dd longitud; origen y destino: db tipo (0 convencional, 1 EMS),
; dw handle, dw desplazamiento, dw segmento o página lógica
MV_TYPE     equ 0
MV_HANDLE   equ 2
MV_OFF      equ 4
MV_PAGE     equ 8
f57:
        cmp byte [ebp+P_EAX], 1
        ja e8f
        V86PTR esi, F_DS, P_ESI
        mov eax, [esi]
        mov ah, 0x96
        cmp dword [esi], 0x100000
        ja .r
        mov eax, [esi]
        mov [mv_len], eax
        add esi, 4
        mov edi, mv_s
        call .load
        jc .e98
        mov edi, mv_d
        call .load
        jc .e98
.loop:
        xor ah, ah
        cmp dword [mv_len], 0
        je .r
        mov ebx, mv_s
        mov edx, WIN_EMS1
        call .resolve
        jc .r
        push eax
        push ecx
        mov ebx, mv_d
        mov edx, WIN_EMS2
        call .resolve
        pop edx
        pop esi
        jc .r
        mov edi, eax
        cmp ecx, edx
        jbe .c1
        mov ecx, edx
.c1:
        cmp ecx, [mv_len]
        jbe .c2
        mov ecx, [mv_len]
.c2:
        mov eax, cr3
        mov cr3, eax
        sub [mv_len], ecx
        add [mv_s+MV_OFF], ecx
        add [mv_d+MV_OFF], ecx
        cmp byte [ebp+P_EAX], 0
        jne .x
        rep movsb
        jmp .loop
.x:
        mov al, [esi]
        xchg al, [edi]
        mov [esi], al
        inc esi
        inc edi
        loop .x
        jmp .loop
.e98:
        mov ah, 0x98
.r:
        ret
; ESI -> descripción en la estructura del programa, EDI -> la nuestra
.load:
        movzx eax, byte [esi]
        cmp al, 1
        ja .lbad
        mov [edi+MV_TYPE], al
        movzx eax, word [esi+1]
        mov [edi+MV_HANDLE], ax
        movzx eax, word [esi+3]
        movzx ecx, word [esi+5]
        cmp byte [edi+MV_TYPE], 0
        jne .lems
        shl ecx, 4                  ; convencional: dirección lineal
        add eax, ecx
        xor ecx, ecx
.lems:
        mov [edi+MV_OFF], eax
        mov [edi+MV_PAGE], ecx
        add esi, 7
        clc
        ret
.lbad:
        stc
        ret
; EBX -> descripción, EDX = ventana -> EAX = dirección lineal, ECX = bytes
; seguidos que hay ahí; CF y AH si no vale
.resolve:
        cmp byte [ebx+MV_TYPE], 0
        jne .rems
        mov eax, [ebx+MV_OFF]
        mov ecx, 0x100000
        clc
        ret
.rems:
        cmp dword [ebx+MV_OFF], 0x4000
        jb .rn
        sub dword [ebx+MV_OFF], 0x4000
        inc dword [ebx+MV_PAGE]
        jmp .rems
.rn:
        movzx esi, word [ebx+MV_HANDLE]
        call get_handle_esi
        jc .rr
        mov eax, [ebx+MV_PAGE]
        movzx ecx, word [esi+H_COUNT]
        cmp eax, ecx
        jae .r8a
        movzx ecx, word [esi+H_START]
        add eax, ecx
        movzx eax, word [pagemap+eax*2]
        shl eax, 14
        add eax, [p_poolphys]
        or eax, 3
        mov ecx, edx
        shr ecx, 12
        and ecx, 0x3FF
        lea ecx, [PT1_LIN+ecx*4]
        mov [ecx], eax
        add eax, 0x1000
        mov [ecx+4], eax
        add eax, 0x1000
        mov [ecx+8], eax
        add eax, 0x1000
        mov [ecx+12], eax
        mov eax, edx
        add eax, [ebx+MV_OFF]
        mov ecx, 0x4000
        sub ecx, [ebx+MV_OFF]
        clc
.rr:
        ret
.r8a:
        mov ah, 0x8A
        stc
        ret

; -----------------------------------------------------------------------------
;  VCPI 1.0 (INT 67h AX=DExxh) desde el modo V86
; -----------------------------------------------------------------------------
vcpi_v86:
        movzx eax, byte [ebp+P_EAX]
        cmp al, 0x0C
        ja e8f
        jmp [vcpi_table+eax*4]
vcpi_table  dd v00, v01, v02, v03, v04, v05, v06, v07, v08, v09, v0a, v0b, v0c

v00:
        mov word [ebp+P_EBX], 0x0100
        xor ah, ah
        ret
; DE01h: la interfaz de modo protegido. ES:DI = tabla de páginas del cliente
; (se le copia la nuestra hasta el final del monitor); DS:SI = 3 descriptores
; para su GDT (código y datos nuestros); EBX = punto de entrada.
v01:
        V86PTR edi, F_ES, P_EDI
        mov esi, PT0_LIN
        mov ecx, MONEND_PG
        rep movsd
        add word [ebp+P_EDI], MONEND_PG * 4
        V86PTR edi, F_DS, P_ESI
        mov esi, gdt + SEL_CODE
        mov ecx, 4
        rep movsd
        xor eax, eax
        stosd
        stosd
        mov dword [ebp+P_EBX], vcpi_pm_entry
        xor ah, ah
        ret
v02:
        mov eax, [p_poolpg]
        shl eax, 14
        add eax, [p_poolphys]
        sub eax, 0x1000
        mov [ebp+P_EDX], eax
        xor ah, ah
        ret
v03:
        call v_count
        mov [ebp+P_EDX], edx
        xor ah, ah
        ret
v04:
        call v_alloc
        test ah, ah
        jnz .r
        mov [ebp+P_EDX], edx
.r:
        ret
v05:
        mov edx, [ebp+P_EDX]
        jmp v_free
v06:
        movzx ecx, word [ebp+P_ECX]
        mov ah, 0x8B
        cmp ecx, 0x100
        jae .r
        mov eax, [PT0_LIN+ecx*4]
        and eax, ~0xFFF
        mov [ebp+P_EDX], eax
        xor ah, ah
.r:
        ret
v07:
        mov eax, cr0
        mov [ebp+P_EBX], eax
        xor ah, ah
        ret
v08:
        V86PTR edi, F_ES, P_EDI
        mov eax, dr0
        mov [edi], eax
        mov eax, dr1
        mov [edi+4], eax
        mov eax, dr2
        mov [edi+8], eax
        mov eax, dr3
        mov [edi+12], eax
        mov eax, dr6
        mov [edi+24], eax
        mov eax, dr7
        mov [edi+28], eax
        xor ah, ah
        ret
v09:
        V86PTR edi, F_ES, P_EDI
        mov eax, [edi]
        mov dr0, eax
        mov eax, [edi+4]
        mov dr1, eax
        mov eax, [edi+8]
        mov dr2, eax
        mov eax, [edi+12]
        mov dr3, eax
        mov eax, [edi+24]
        mov dr6, eax
        mov eax, [edi+28]
        mov dr7, eax
        xor ah, ah
        ret
v0a:
        mov ax, [pic_master]
        mov [ebp+P_EBX], ax
        mov ax, [pic_slave]
        mov [ebp+P_ECX], ax
        xor ah, ah
        ret
v0b:
        mov ax, [ebp+P_EBX]
        mov [pic_master], ax
        mov ax, [ebp+P_ECX]
        mov [pic_slave], ax
        xor ah, ah
        ret

; DE0Ch: pasar del modo V86 al modo protegido del cliente. ESI = dirección
; lineal de: dd CR3, dd puntero al GDTR, dd puntero al IDTR, dw LDTR, dw TR,
; dd EIP, dw CS
v0c:
        mov esi, [ebp+P_ESI]
        mov eax, [esi+4]
        mov ecx, [eax]
        mov [sw_gdtr], ecx
        mov cx, [eax+4]
        mov [sw_gdtr+4], cx
        mov eax, [esi+8]
        mov ecx, [eax]
        mov [sw_idtr], ecx
        mov cx, [eax+4]
        mov [sw_idtr+4], cx
        mov eax, [esi+0x10]
        mov [sw_target], eax
        mov ax, [esi+0x14]
        mov [sw_target+4], ax
        mov cx, [esi+0x0C]
        movzx edx, word [esi+0x0E]
        mov eax, [esi]
        mov cr3, eax
        lgdt [sw_gdtr]
        lidt [sw_idtr]
        lldt cx
        mov eax, [sw_gdtr+2]
        mov ebx, edx
        and ebx, ~7
        and byte [eax+ebx+5], ~2    ; la TSS del cliente, no ocupada
        ltr dx
        mov edi, [ebp+P_EDI]
        mov ebx, [ebp+P_EBX]
        mov ecx, [ebp+P_ECX]
        mov edx, [ebp+P_EDX]
        mov ebp, [ebp+P_EBP]
        jmp far [sw_target]

; ---- entrada desde el modo protegido del cliente (llamada lejana) ----
; CS es el primero de los tres descriptores que le dimos; el siguiente, los
; datos. AX = DE0Ch: volver al modo V86; DE03h-DE05h: páginas.
vcpi_pm_entry:
        cmp ax, 0xDE0C
        je pm_to_v86
        push ds
        push ebx
        mov bx, cs
        add bx, 8
        mov ds, bx
        pop ebx
        cmp ax, 0xDE03
        jne .n4
        call v_count
        xor ah, ah
        jmp .r
.n4:
        cmp ax, 0xDE04
        jne .n5
        call v_alloc
        jmp .r
.n5:
        cmp ax, 0xDE05
        jne .bad
        call v_free
        jmp .r
.bad:
        mov ah, 0x8F
.r:
        pop ds
        retf

; En la pila del cliente: 8 bytes de la dirección de retorno y después EIP,
; CS, EFLAGS, ESP, SS, ES, DS, FS y GS del modo V86.
pm_to_v86:
        mov ax, cs
        add ax, 8
        mov ds, ax
        mov es, ax
        mov ax, ss
        lar eax, ax
        test eax, 0x00400000        ; ¿pila de 32 bits?
        jnz .big
        movzx esp, sp
.big:
        mov [sv_esi], esi
        mov [sv_edi], edi
        mov [sv_ecx], ecx
        lea esi, [esp+8]
        mov edi, STKTOP - 36
        mov ecx, 9
        cld
        ss rep movsd
        mov esi, [sv_esi]
        mov edi, [sv_edi]
        mov ecx, [sv_ecx]
        mov eax, [p_phys]
        mov cr3, eax
        lgdt [mon_gdtr]
        lidt [mon_idtr]
        mov ax, SEL_DATA
        mov ss, ax
        mov esp, STKTOP - 36
        mov ds, ax
        mov es, ax
        xor eax, eax
        lldt ax
        mov fs, ax
        mov gs, ax
        and byte [gdt+SEL_TSS+5], ~2
        mov ax, SEL_TSS
        ltr ax
        mov dword [esp+8], 0x00023002
        iretd

; ---- páginas de 4 KB para VCPI ----
; Salen de las páginas EMS libres (cada una, cuatro); vstate[página] = 80h +
; un bit por cada cuarto ocupado. Estas rutinas solo usan DS (sirven también
; llamadas desde el modo protegido del cliente) y conservan los registros
; salvo EDX y AH.

; -> EDX = páginas de 4 KB libres
v_count:
        mov edx, [nfree_end]
        sub edx, [ems_used]
        shl edx, 2
        add edx, [v_partial]
        ret

; -> AH = 0 y EDX = dirección física, o AH = 88h
v_alloc:
        push ebx
        push ecx
        push esi
        mov esi, [v_hint]
        cmp esi, -1
        je .new
        mov cl, [vstate+esi]
        and cl, 0x0F
        cmp cl, 0x0F
        jne .use
.new:
        mov esi, [nfree_end]
        cmp esi, [ems_used]
        jbe .scan
        dec esi
        mov [nfree_end], esi
        movzx esi, word [pagemap+esi*2]
        mov byte [vstate+esi], 0x80
        add dword [v_partial], 4
        jmp .use
.scan:
        cmp dword [v_partial], 0
        je .full
        mov ebx, [nfree_end]
.sl:
        cmp ebx, [p_poolpg]
        jae .full
        movzx esi, word [pagemap+ebx*2]
        mov cl, [vstate+esi]
        and cl, 0x0F
        cmp cl, 0x0F
        jne .use
        inc ebx
        jmp .sl
.use:
        mov [v_hint], esi
        xor ebx, ebx
        mov cl, [vstate+esi]
.bit:
        bt ecx, ebx
        jnc .got
        inc ebx
        jmp .bit
.got:
        bts dword [vstate+esi], ebx
        dec dword [v_partial]
        mov edx, esi
        shl edx, 2
        add edx, ebx
        shl edx, 12
        add edx, [p_poolphys]
        xor ah, ah
        jmp .r
.full:
        mov ah, 0x88
.r:
        pop esi
        pop ecx
        pop ebx
        ret

; EDX = dirección física -> AH = 0, o 8Ah si no es una página nuestra
v_free:
        push ebx
        push ecx
        push esi
        push edx
        sub edx, [p_poolphys]
        jb .bad
        shr edx, 12
        mov ebx, edx
        and ebx, 3
        shr edx, 2
        cmp edx, [p_poolpg]
        jae .bad
        test byte [vstate+edx], 0x80
        jz .bad
        btr dword [vstate+edx], ebx
        jnc .bad
        inc dword [v_partial]
        test byte [vstate+edx], 0x0F
        jnz .ok
        ; la página de 16 KB queda libre del todo: vuelve a las de EMS
        mov ebx, [nfree_end]
.f:
        cmp ebx, [p_poolpg]
        jae .ok                     ; (no debería pasar)
        cmp [pagemap+ebx*2], dx
        je .found
        inc ebx
        jmp .f
.found:
        mov esi, [nfree_end]
        mov cx, [pagemap+esi*2]
        mov [pagemap+ebx*2], cx
        mov [pagemap+esi*2], dx
        inc dword [nfree_end]
        mov byte [vstate+edx], 0
        sub dword [v_partial], 4
        cmp [v_hint], edx
        jne .ok
        mov dword [v_hint], -1
.ok:
        xor ah, ah
        jmp .r
.bad:
        mov ah, 0x8A
.r:
        pop edx
        pop esi
        pop ecx
        pop ebx
        ret

        align 16
mon_end:
mon_size    equ mon_end - mon_start

; ---- datos sin iniciar del monitor (se ponen a cero al arrancar) ----
; (en direcciones fijas, tras el sitio reservado al código)
MONBSS      equ MONIMG + 0x7000
        times (mon_size > 0x7000) * -1 db 0     ; el código no cabe: error
        absolute MONBSS
monbss_start:
idt         resb 256*8
tss         resb TSS_LEN + 3
pagemap     resw MAXPOOL
vstate      resb MAXPOOL + 4
handles     resb NHAND*H_LEN
curmap      resd 4
ems_used    resd 1                  ; entradas de pagemap de los handles
nfree_end   resd 1                  ; fin de las libres (después, las de VCPI)
v_hint      resd 1
v_partial   resd 1                  ; cuartos libres de páginas de VCPI
hlt_esp     resd 1
gp_base     resd 1
alt_ptr     resd 1
mv_len      resd 1
mv_s        resd 3
mv_d        resd 3
sv_esi      resd 1
sv_edi      resd 1
sv_ecx      resd 1
dma_delta   resd 8
dma_addr    resw 8
dma_latch   resw 8
dma_page    resb 8
dma_ff      resb 2
pic_master  resw 1
pic_slave   resw 1
sw_gdtr     resb 8
sw_idtr     resb 8
sw_target   resb 8
tmp16       resb 16
need_flush  resb 1
in_hlt      resb 1
kbc_state   resb 1
dma_class   resb 1
os_key      resb 1
            alignb 4                ; [SB]
sb_handler  resd 1                  ; [SB] seg:desp de la rutina de HSPSND
sb_dmaptr   resd 8                  ; [SB] estructura de cada canal cedido
sb_ranges   resw 2*8 + 1            ; [SB] puertos (primero, último), FFFFh
            alignb 4                ; [SB-PM]
sb_fmstat   resd 1                  ; [SB-PM] estado de la FM (lineal) o 0
sb_irqres   resw 1                  ; [SB-VGA] IRQ de HSPSND (bit n = IRQ n)
dma_cnt     resw 8                  ; [SB] cuenta programada de cada canal
            alignb 4
            resb 4096               ; pila de anillo 0
STKTOP:
monbss_end:
; todo tiene que caber en las páginas del monitor
            resb MONIMG + MON_PAGES*4096 - $

        section .scr nobits follows=.mon align=16


; zona de trabajo de la inicialización (4 KB): tras el fichero
scratch:    resb 4096
