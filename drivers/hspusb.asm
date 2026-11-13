; =============================================================================
;  HSPUSB.SYS - USB nativo de Hispa-DOS: controladoras UHCI, OHCI, EHCI y
;  xHCI; memorias USB, teclado, ratón, mandos de juego y concentradores
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\HISPADOS\HSPUSB.SYS [/F] [/U] [/INFO] [/V] [/?]
;     (sin opciones) seguro: si la BIOS está usando el USB no se toca nada. Se
;         entiende que lo usa si el controlador es suyo (USB Legacy Support),
;         si lo tiene en marcha, o si el sistema ha arrancado desde un disco
;         USB de la BIOS. Salvo [USB-AUTO] en los equipos sin ratón PS/2
;         (sin 8042, como los MacBook; o con 8042 sin nada en el puerto
;         auxiliar y sin arranque por USB): ahí solo HSPUSB puede dar un
;         ratón y lo toma como con /F (sin 8042, como con /U si se ha
;         arrancado de una memoria USB). Ver auto_take.
;     /F  tomar el controlador aunque lo esté usando la BIOS (la BIOS dejará
;         de atender sus dispositivos USB: el teclado y el ratón pasan a
;         HSPUSB). No vale si el sistema ha arrancado desde un disco USB.
;     /U  como /F, también cuando el sistema ha arrancado desde una memoria
;         USB de la BIOS: HSPUSB la reconoce (por su sector 0) y desde ese
;         momento atiende él la unidad 80h de INT 13h, con la geometría que
;         daba la BIOS. Riesgo: si la memoria de arranque o el teclado no
;         aparecen después (otro controlador, un dispositivo que no se
;         admite), el equipo se queda sin ellos hasta reiniciar (F5 al
;         arrancar salta CONFIG.SYS).
;     /INFO  diagnóstico en pantalla: controladores USB del equipo, estado
;         del xHCI y árbol de dispositivos (puerto, velocidad, VID:PID,
;         clase/subclase/protocolo de cada interfaz y el uso que se le da);
;         espera 15 s para poder hacer una foto.
;     /V  mensajes detallados
;
;  Busca las controladoras USB del equipo, de las cuatro clases que hay (UHCI
;  y OHCI de USB 1.x, EHCI de USB 2.0 y xHCI de USB 3; varias a la vez, hasta
;  HC_N, pero un solo xHCI), se las quita a la BIOS, las inicializa y enumera
;  los dispositivos. Lo que sigue funciona igual con cualquiera de ellas: hay
;  una capa común de «controladora» (lib/usb_hc.inc: control, bulk, tuberías
;  de interrupción y puertos raíz) con cuatro implementaciones (el xHCI en
;  este fichero; lib/usb_uhci.inc, usb_ohci.inc y usb_ehci.inc; el inicio de
;  todas, en lib/usb_hcini.inc). Ver docs/USB.md.
;  Se enumeran los dispositivos de los puertos y los que cuelgan de concentradores
;  (hubs; hasta HUB_DEPTH encadenados, HUB_N a la vez, POOL_N dispositivos):
;   - la primera memoria USB (clase 08h, SCSI, Bulk-Only Transport), que se
;     ofrece a DOS como una unidad nueva (FAT12/FAT16, con o sin particiones);
;   - un teclado USB (HID, protocolo de arranque): las teclas se entregan como
;     si vinieran del teclado PS/2 (orden D2h del controlador 8042), así que
;     funcionan la BIOS, KEYB y todos los programas;
;   - un ratón USB (o un panel táctil que ofrezca una interfaz de ratón de
;     arranque, 03/01/02; en un dispositivo con varias interfaces se elige
;     esa): se entrega a HSPRATON (INT 33h AX=48D0h) o, si no está, como
;     paquetes PS/2 (orden D3h del 8042);
;   - hasta JOY_N mandos de juego: los HID genéricos (gamepads y joysticks;
;     se lee su descriptor de informe, lib/hidjoyp.inc, para saber dónde van
;     los ejes, la cruceta y los botones) y los de tipo Xbox 360 con cable
;     (interfaz FFh/5Dh/01h, formato fijo). El estado de cada mando (HJS_*,
;     lib/hidjoyx.inc) se entrega a HSPJOY por INT 2Fh AX=D903h (DL = mando,
;     ES:DI = estado) con cada informe y unas cuatro veces por segundo; lo
;     demás (INT 15h 84h, mando a teclado) lo hace HSPJOY (docs/MANDOS.md).
;     Tras atender el informe de un mando, si HSPJOY está, se espera al
;     siguiente (lo que dure su intervalo, 10 ms como mucho, medido con
;     MFINDEX) para entregar un estado reciente y no el de hace un tic.
;  Los informes del teclado y del ratón se recogen con la interrupción del
;  reloj (INT 08h). En modo real usa el modo "irreal" para los registros del
;  xHCI. En modo V86 con un gestor de memoria que dé VCPI (HSPEMM, EMM386) no
;  puede (el límite de los segmentos es de 64 KB y la dirección lineal no es
;  la física): entonces llega a los registros con INT 15h AH=87h (un acceso de
;  32 bits por llamada; lo atiende el monitor) y pregunta la dirección física
;  de sus anillos y contextos por VCPI (INT 67h AX=DE06h), página a página,
;  de modo que también vale cargado en la memoria superior (DEVICEHIGH).
;  En V86 sin VCPI (Windows en modo 386 mejorado) no funciona.
;
;  Memoria: se queda residente la sección .text (transferencias, puertos,
;  informes, INT 13h, el reloj y lo que hace falta para reconocer una
;  memoria USB conectada después) y, detrás, la zona de trabajo. La
;  inicialización (sección .init: buscar y tomar las controladoras, ponerlas
;  en marcha, el teclado, el ratón y los mandos, /INFO y los mensajes) va en
;  el fichero detrás de .text, se ejecuta en CS:INIT_ORG (init_go) y se
;  descarta. Ojo: lo de .text no puede usar nada de .init después del
;  arranque (los mensajes solo con /V o /INFO, que se apagan en rescan).
;
;  Concentradores (USB 2.0 cap. 11, xHCI 4.5/6.2.2): descriptor de
;  concentrador, Configure Endpoint con Hub = 1, número de puertos y TT Think
;  Time (un solo traductor: MTT = 0), PORT_POWER, GET_STATUS, PORT_RESET y,
;  para cada dispositivo, un Slot Context con la cadena de ruta, el puerto
;  del controlador y, si es de velocidad baja o plena detrás de un
;  concentrador de alta velocidad, el slot y el puerto del traductor (TT).
;  Los de USB 3 se tratan según la norma (descriptor 2Ah, SET_HUB_DEPTH),
;  sin haberse podido probar: QEMU solo ofrece un concentrador de USB 1.1.
;
;  [USB-INTEL] En los Intel de las series 7, 8 y 9 los puertos que pueden ir
;  por el xHCI se le pasan (XUSB2PR, USB3_PSSEN; intel_routes); lo que solo
;  llega a un EHCI (el teclado y el panel de un MacBook) va por él, detrás
;  de su concentrador interno, con transacciones partidas.
;  [USB-RATON] Un ratón que no confirma el protocolo de arranque
;  (GET_PROTOCOL), o que dice ser de ratón sin la subclase de arranque, se
;  lee según su descriptor de informe (lib/hidraton.inc): X e Y relativas y
;  los botones 1-3, con su identificador de informe.
;  [USB-BT] Los receptores de Bluetooth en modo «HID proxy» (los de Apple y
;  Broadcom) se dejan para el final: solo se usan si no hay otro teclado o
;  ratón (proxy_retry).
;  Lo que NO hace: usar el descriptor de informe HID para un panel táctil sin
;  ninguna interfaz de ratón (protocolo 2): un HID genérico con X e Y que no
;  sea un mando no se usa como ratón. Los modos multitáctiles propios de cada
;  fabricante no están publicados y no se usan. Los mandos tienen que estar
;  conectados al cargar el driver (no se buscan después), no se encienden los
;  LED ni la vibración, y no se admiten los que necesitan un saludo propio
;  antes de dar informes (Xbox One/Series por cable, Switch Pro por USB, el
;  receptor inalámbrico de Xbox 360). Los aparatos de USB 1.x detrás de un
;  concentrador de USB 2.0 en un EHCI (transacciones partidas) están hechos
;  según la norma (revisados con EHCI 1.0, 4.12) pero sin probar (QEMU no
;  tiene concentrador de USB 2.0).
; =============================================================================
        cpu 386                     ; (sin BSWAP)
        bits 16
        org 0
; Dos secciones: .text, lo residente, e .init, la inicialización, que va en
; el fichero detrás de .text pero se ejecuta en CS:INIT_ORG (init_go) y no se
; queda en la memoria.
        section .text
        section .init follows=.text vstart=INIT_ORG
init_start:
        section .text

; Zona de memoria tras lo residente (res_end), en una página física (mem_off),
; donde estaba la inicialización en el fichero (se ejecuta en CS:INIT_ORG,
; por encima: ver init_go). Lo fijo:
; el búfer de datos del disco (4 KB, [data_rel]: al principio de la zona
; durante el inicio), los búferes comunes y, si hay xHCI, sus anillos y
; contextos. Lo demás (colas de las controladoras clásicas, contextos de
; salida de cada aparato del xHCI, listas de tramas) se reserva detrás según
; hace falta (zalloc, fl_alloc: lib/usb_hc.inc).
DESC        equ 0x1000
PIPE_BUF    equ 0x1100                ; un búfer de informe (40h) por tubería
KBD_BUF     equ PIPE_BUF
MOU_BUF     equ PIPE_BUF + 0x40
JOY_BUF     equ PIPE_BUF + 0x80
CBW_BUF     equ 0x1200
CSW_BUF     equ 0x1240
COM_END     equ 0x1280                ; fin de lo común
IN_CTX      equ 0x1300                ; xHCI (900h: no cruza de página)
CMD_RING    equ 0x1C00
EVT_RING    equ 0x1D00
BIN_RING    equ 0x1E00
BOUT_RING   equ 0x1F00
PINT_RING   equ 0x2000                ; un anillo (100h) por tubería
KINT_RING   equ PINT_RING
MINT_RING   equ PINT_RING + 0x100
JINT_RING   equ PINT_RING + 0x200
EP0_RINGS   equ 0x2400                ; un anillo de EP0 por dispositivo
ERST        equ 0x2C00
DCBAA       equ 0x2C40                ; MAX_SLOTS + 1 entradas de 8 bytes
X_END       equ 0x2D00                ; fin de lo del xHCI
JOY_N       equ 2                     ; mandos de juego a la vez
PIPE_N      equ 2 + JOY_N             ; tuberías de interrupción: teclado, ratón y mandos
POOL_N      equ 8                     ; dispositivos a la vez (concentradores incluidos)
INIT_ORG    equ 0xB000                ; dónde se ejecuta la inicialización (.init)
MAX_SLOTS   equ POOL_N
HUB_N       equ 6                     ; concentradores a la vez
HUB_DEPTH   equ 3                     ; niveles de concentradores encadenados
REP_DELAY   equ 9                   ; repetición: 0,5 s y unas 9 por segundo
REP_RATE    equ 2
RING_TRBS   equ 16
CHUNK_SECS  equ 8                   ; 4 KB por transferencia

header:
        dw 0xFFFF, 0xFFFF
        dw 0x0802                   ; bloque, sectores de 32 bits
        dw strategy
        dw interrupt
        db 1, "HSPUSB "

req         dd 0
xbase       dd 0
op_base     dd 0
rt_base     dd 0
db_base     dd 0
ctx_sz      dw 32
mem_off     dw 0
slot        db 0
port        db 0
speed       db 0
dci_in      db 0
dci_out     db 0
d_if        db 0                    ; su interfaz (Bulk-Only Mass Storage Reset)
            db 0
mps_in      dw 0
mps_out     dw 0
part_start  dd 0
tag         dd 0
; estado de los anillos: índice y bit de ciclo
cmd_idx     db 0
cmd_cyc     db 1
bin_idx     db 0
bin_cyc     db 1
bout_idx    db 0
bout_cyc    db 1
evt_idx     db 0
evt_cyc     db 1
evt         times 16 db 0
; dispositivo que se está enumerando (lo usa control)
cur_slot    db 0
            db 0
cur_ep0     dw EP0_RINGS
cur_st      dw pool_st
; teclado y ratón USB
busy        db 1
; tuberías de interrupción: 0 teclado, 1 ratón, 2... mandos de juego
ip_on       times PIPE_N db 0
ip_slot     times PIPE_N db 0       ; xHCI: slot
ip_dci      times PIPE_N db 0       ; endpoint: 2 * número + 1 (entrada)
ip_pool     times PIPE_N db 0       ; su aparato en la reserva
ip_tog      times PIPE_N db 0       ; UHCI: testigo de alternancia
            align 2
ip_len      dw 8, 4
            times JOY_N dw 0        ; bytes que se piden
ip_mps      times PIPE_N dw 0
ip_ring     times PIPE_N db 0, 1    ; xHCI: índice y ciclo de su anillo
ip_got      dw 0                    ; bytes del informe que acaba de llegar
k_on        equ ip_on
m_on        equ ip_on + 1
j_on        equ ip_on + 2
k_slot      equ ip_slot
m_slot      equ ip_slot + 1
j_slot      equ ip_slot + 2
k_dci       equ ip_dci
m_dci       equ ip_dci + 1
j_dci       equ ip_dci + 2
k_len       equ ip_len
m_len       equ ip_len + 2
j_len       equ ip_len + 4
jint_idx    equ ip_ring + 4
j_ev        db 0                    ; bit n: se acaba de atender un informe del mando n
j_tick      db 0
j_ack       db 0                    ; HSPJOY recogió el último estado entregado
j_wait      dw 16                   ; espera del informe reciente (microtramas)
ctl_buf     dw DESC                 ; adónde van los datos de una petición de control
oldrep      times 8 db 0
newrep      times 8 db 0
rep_key     db 0
rep_cnt     db 0
old08       dd 0
sv_gdtr     dw 0
            dd 0
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
bpb         times 25 db 0
bpbarr      dw bpb
media_byte  db 0xF8

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        pushad
        push ds
        push es
        push fs                     ; el modo irreal lo cambia: es del programa
        mov byte [cs:busy], 1
        ; la GDT del modo irreal, antes de usarlo por primera vez
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [cs:gdtr+2], eax
        call io_mode
        call unreal
        call a20_open
        lds bx, [cs:req]
        mov al, [bx+2]
        cmp al, 0
        je init_go
        cmp byte [cs:iomode], 2     ; V86 sin VCPI: no se llega al xHCI
        jne .can
        mov word [bx+3], 0x8102     ; unidad no preparada
        jmp .out
.can:
        cmp al, 1
        je .media
        cmp al, 2
        je .bpb
        cmp al, 4
        je .read
        cmp al, 8
        je .write
        cmp al, 9
        je .write
.done:
        mov word [bx+3], 0x0100
.out:
        call a20_restore
        mov byte [cs:busy], 0
        pop fs
        pop es
        pop ds
        popad
        retf
.media:
        call usb_media
        lds bx, [cs:req]
        mov [bx+0x0E], al           ; 1 la misma, FFh cambiada o quitada
        jmp .done
.bpb:
        cmp byte [cs:gone], 0
        je .bpb1
        call try_back
        lds bx, [cs:req]
        jc .nrdy
.bpb1:
        mov word [bx+0x12], bpb
        mov [bx+0x14], cs
        jmp .done
.nrdy:
        mov word [bx+3], 0x8102     ; unidad no preparada
        jmp .out
.read:
        mov byte [cs:rw_dir], 1
        jmp .rw
.write:
        mov byte [cs:rw_dir], 0
.rw:
        cmp byte [cs:gone], 0
        je .rw1
        call try_back
        lds bx, [cs:req]
        jnc .rw1
        mov word [bx+0x12], 0
        jmp .nrdy
.rw1:
        movzx eax, word [bx+0x14]
        cmp ax, 0xFFFF
        jne .s
        mov eax, [bx+0x1A]
.s:
%ifdef USBTRACE
        push eax
        push dx
        mov dx, 0x3F8
        mov al, 'W'
        cmp byte [cs:rw_dir], 0
        je .tw
        mov al, 'R'
.tw:
        out dx, al
        pop dx
        pop eax
        push eax
        push dx
        mov dx, 0x3F8
        call trace_num
        mov al, ' '
        out dx, al
        pop dx
        pop eax
%endif
        add eax, [cs:part_start]
        mov [cs:rw_lba], eax
        mov cx, [bx+0x12]
        mov [cs:rw_left], cx
        mov ax, [bx+0x0E]
        mov [cs:rw_off], ax
        mov ax, [bx+0x10]
        mov [cs:rw_seg], ax
        call rw_run
        lds bx, [cs:req]
        jc .err
        jmp .done
.err:
        mov ax, [bx+0x12]
        sub ax, [cs:rw_left]
        mov [bx+0x12], ax
        mov word [bx+3], 0x810C     ; error general
        jmp .out

; leer (rw_dir = 1) o escribir rw_left sectores desde rw_lba en rw_seg:rw_off
; -> CF si falla (rw_left = los que quedaban)
rw_run:
        ; normalizado ya antes del primer trozo (4 KB no caben si el
        ; desplazamiento pasa de F000h: la copia daría la vuelta al segmento)
        mov ax, [cs:rw_off]
        shr ax, 4
        add [cs:rw_seg], ax
        and word [cs:rw_off], 15
.chunk:
        mov cx, [cs:rw_left]
        jcxz .ok
        cmp cx, CHUNK_SECS
        jbe .c
        mov cx, CHUNK_SECS
.c:
        mov [cs:rw_n], cx
        cmp byte [cs:rw_dir], 0
        jne .rd
        ; escritura: DOS -> búfer -> USB
        call copy_from_dos
        mov al, 0x2A
        call scsi_rw
        jc .r
        jmp .adv
.rd:
        mov al, 0x28
        call scsi_rw
        jc .r
        call copy_to_dos
.adv:
        movzx eax, word [cs:rw_n]
        add [cs:rw_lba], eax
        sub [cs:rw_left], ax
        shl ax, 9
        add [cs:rw_off], ax
        ; normalizar el puntero de DOS
        mov ax, [cs:rw_off]
        shr ax, 4
        add [cs:rw_seg], ax
        and word [cs:rw_off], 15
        jmp .chunk
.ok:
        clc
.r:
        ret

; =============================================================================
;  INT 13h del disco de arranque (opción /U). Cuando el sistema ha arrancado
;  desde una memoria USB que servía la BIOS y HSPUSB le quita el controlador,
;  la BIOS ya no puede leerla: desde ese momento la unidad 80h la atiende
;  HSPUSB por el xHCI, con la misma geometría que daba la BIOS.
; =============================================================================
int13:
        cmp dl, 0x80
        je .ours
        jmp far [cs:old13]
.ours:
        push ds
        push es
        push fs
        pushad
        mov bp, sp                  ; [bp+28] EAX, +24 ECX, +20 EDX, +16 EBX,
                                    ; +4 ESI, +0 EDI, +34 ES, +36 DS, +42 flags
        sti
        cld
        mov al, [cs:busy]
        push ax
        mov byte [cs:busy], 1
        call io_mode
        cmp byte [cs:iomode], 2
        jne .rm
        mov ah, 0x80                ; modo V86 sin VCPI: no se puede
        jmp .ret
.rm:
        call unreal
        call a20_open
        mov ah, [bp+29]
        call i13_do
        call a20_restore
.ret:
        mov [cs:i13_last], ah
        mov [bp+29], ah
        and byte [bp+42], 0xFE
        test ah, ah
        jz .nc
        cmp byte [cs:i13_keep], 0   ; (AH=15h y 41h devuelven un valor, no un error)
        jne .nc
        or byte [bp+42], 1
.nc:
        mov byte [cs:i13_keep], 0
        pop ax
        mov [cs:busy], al
        popad
        pop fs
        pop es
        pop ds
        iret

; AH = función -> AH = estado (los demás resultados, en la pila de int13)
i13_do:
        cmp ah, 0x01
        je .last
        cmp ah, 0x02
        je .chs
        cmp ah, 0x03
        je .chs
        cmp ah, 0x08
        je .geom
        cmp ah, 0x15
        je .type
        cmp ah, 0x41
        je .ext
        cmp ah, 0x42
        je .lba
        cmp ah, 0x43
        je .lba
        cmp ah, 0x48
        je .parm
        ; reinicio, verificación, posicionado...: nada que hacer
        cmp ah, 0x00
        je .ok
        cmp ah, 0x04
        je .ok
        cmp ah, 0x0C
        je .ok
        cmp ah, 0x0D
        je .ok
        cmp ah, 0x10
        je .ok
        cmp ah, 0x11
        je .ok
        cmp ah, 0x44
        je .ok
        cmp ah, 0x47
        je .ok
        mov ah, 0x01                ; función no admitida
        ret
.ok:
        xor ah, ah
        ret
.last:
        mov ah, [cs:i13_last]
        ret
.geom:
        mov ax, [cs:i13_cx]
        mov [bp+24], ax
        mov ax, [cs:i13_dx]
        mov [bp+20], ax
        mov byte [bp+16], 0
        mov word [bp+0], 0          ; ES:DI = 0 (sin tabla de parámetros)
        mov word [bp+34], 0
        mov byte [bp+28], 0
        xor ah, ah
        ret
.type:
        mov eax, [cs:i13_total]
        mov [bp+20], ax             ; CX:DX = sectores
        shr eax, 16
        mov [bp+24], ax
        mov byte [cs:i13_keep], 1
        mov ah, 3
        ret
.ext:
        cmp word [bp+16], 0x55AA
        jne .bad
        mov word [bp+16], 0xAA55
        mov word [bp+24], 1         ; acceso por paquetes
        mov byte [bp+28], 0
        mov byte [cs:i13_keep], 1
        mov ah, 0x21                ; versión 1.1
        ret
.bad:
        mov ah, 0x01
        ret
.parm:
        mov ds, [bp+36]
        mov si, [bp+4]
        cmp word [si], 26
        jb .bad
        mov word [si], 26
        mov word [si+2], 0
        xor eax, eax
        mov [si+4], eax
        mov [si+8], eax
        mov [si+12], eax
        mov [si+20], eax
        mov eax, [cs:i13_total]
        mov [si+16], eax
        mov word [si+24], 512
        xor ah, ah
        ret
.chs:
        ; (cilindro * cabezas + cabeza) * sectores + sector - 1
        mov cx, [bp+24]
        movzx eax, cl
        and al, 0xC0
        shl ax, 2
        mov al, ch                  ; cilindro
        movzx ebx, byte [cs:i13_dx+1]
        inc ebx                     ; cabezas
        mul ebx
        movzx ebx, byte [bp+21]     ; cabeza
        add eax, ebx
        movzx ebx, byte [cs:i13_cx]
        and bl, 0x3F                ; sectores por pista
        jz .bad
        mul ebx
        and ecx, 0x3F
        jz .bad
        dec ecx
        add eax, ecx
        mov [cs:rw_lba], eax
        movzx ax, byte [bp+28]
        mov [cs:rw_left], ax
        mov ax, [bp+16]
        mov [cs:rw_off], ax
        mov ax, [bp+34]
        mov [cs:rw_seg], ax
        mov al, 1
        cmp byte [bp+29], 0x02
        je .go
        xor al, al
        jmp .go
.lba:
        mov ds, [bp+36]
        mov si, [bp+4]
        cmp dword [si+12], 0
        jne .range
        mov eax, [si+8]
        mov [cs:rw_lba], eax
        mov ax, [si+2]
        mov [cs:rw_left], ax
        mov ax, [si+4]
        mov [cs:rw_off], ax
        mov ax, [si+6]
        mov [cs:rw_seg], ax
        mov al, 1
        cmp byte [bp+29], 0x42
        je .go
        xor al, al
.go:
        mov [cs:rw_dir], al
        movzx eax, word [cs:rw_left]
        add eax, [cs:rw_lba]
        jc .range
        cmp eax, [cs:i13_total]
        ja .range
        push bp
        call rw_run
        pop bp
        jc .fail
        xor ah, ah
        ret
.range:
        mov ah, 0x04                ; sector no encontrado
        ret
.fail:
        mov ah, 0x80                ; sin respuesta
        ret
old13       dd 0
i13_cx      dw 0
i13_dx      dw 0
i13_total   dd 0
i13_last    db 0
i13_keep    db 0

; ---- cambio de la memoria USB ----
; -> AL = 1 la misma, FFh cambiada o quitada (DS cambia)
usb_media:
        push cs
        pop ds
        sti
        cmp byte [dport], 0
        je .same
        cmp byte [gone], 0
        jne .look
        mov al, [bk_pool]
        call dev_hc
        mov cl, [dport]
        call rp_get
        test al, 1                  ; conectada
        jz .lost
        test al, 0x80               ; se ha desconectado y conectado
        jnz .lost
        ; detrás de un concentrador: el estado de su puerto
        cmp byte [d_hub], 0xFF
        je .same
        mov al, [d_hub]
        mov [hp_hub], al
        mov al, [d_hport]
        mov [hp_port], al
        call hub_status
        jc .lost
        test al, 1
        jz .lost
        test dl, 1                  ; C_PORT_CONNECTION
        jz .same
.lost:
        call drop_disk
.look:
        call rescan
        mov al, 0xFF
        ret
.same:
        mov al, 1
        ret

; sin memoria: ¿han conectado una? (DS cambia; CF si sigue sin haber)
try_back:
        push cs
        pop ds
        sti
        call rescan
        cmp byte [gone], 1
        cmc
        ret

; ESI = PORTSC del puerto de la memoria, EAX = su valor
dport_sc:
        movzx esi, byte [dport]
port_sc:
        dec esi
        shl esi, 4
        add esi, [op_base]
        add esi, 0x400
        call mrd
        ret

; borrar los bits de cambio del puerto (ESI = PORTSC)
clr_csc:
        call mrd
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | 0x00FE0000
        call mwr
        ret

; xHCI: las operaciones de puerto raíz de la capa común (lib/usb_hc.inc)
x_rp_get:
        push esi
        push edx
        movzx esi, cl
        call port_sc
        mov edx, eax
        and al, 3
        test edx, 1 << 17
        jz .a
        or al, 0x80
.a:
        pop edx
        pop esi
        ret
x_rp_ack:
        push esi
        push eax
        movzx esi, cl
        call port_sc
        call clr_csc
        pop eax
        pop esi
        ret
x_time:
        push esi
        mov esi, [cs:rt_base]
        call mrd
        and ax, 0x3FFF
        pop esi
        ret

; la controladora actual, si es una UHCI que se ha quedado sin aparatos en
; uso: se aparca (deja libre su lista de tramas)
park_idle:
        cmp byte [cs:cur_type], HCT_UHCI
        jne .r
        mov al, [cs:cur_hc]
        call hc_idle
        jnz .r
        call u_park
.r:
        ret

; liberar el slot AL (Disable Slot) y su entrada de la tabla de contextos
slot_free:
        push bx
        push di
        push ax
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        pop bp
        push bp
        and ebp, 0xFF
        shl ebp, 24
        or ebp, 10 << 10            ; Disable Slot
        call xhci_cmd
        pop ax
        movzx di, al
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov dword [cs:di], 0
        pop di
        pop bx
        ret

; la memoria ya no está: liberar su slot
drop_disk:
        mov al, [dpool]
        call dev_free
        movzx bx, byte [dpool]
        mov byte [pool_used+bx], 0
        mov byte [gone], 1
        mov byte [disk_found], 0
        call park_idle
        ret

; buscar una memoria en los puertos que acaban de conectarse (CSC) y en los
; de los concentradores que avisan de un cambio
rescan:
        mov al, [opt_verbose]
        mov ah, [opt_info]
        push ax
        mov byte [opt_verbose], 0   ; sin mensajes: estamos dentro de DOS
        mov byte [opt_info], 0
        mov byte [only_disk], 1
        xor bx, bx
.cl:
        cmp bl, [hc_n]
        jae .hubs
        cmp byte [gone], 0
        je .r
        test byte [hc_flags+bx], HCF_OURS
        jz .cn
        mov al, bl
        call hc_sel
        mov al, [hc_flags+bx]
        and al, HCF_PARK
        mov [rs_park], al
        jz .ports
        ; UHCI aparcada: solo se arranca (con la lista de tramas de
        ; reserva) si le han conectado algo
        push bx
        call u_any
        pop bx
        jz .cn
        call u_start
        jc .cn
.ports:
        xor cl, cl
.pl:
        inc cl
        cmp cl, [hc_np+bx]
        ja .cd
        cmp byte [gone], 0
        je .cd
        call rp_get
        test al, 0x80
        jz .pl
        push bx
        push cx
        call rp_ack
        ; lo que colgaba de este puerto ya no está: sus concentradores
        call hubs_drop
        call rp_get
        test al, 1
        jz .nx
        call port_enable
        jc .nx
        call root_dev
        call enum_one
        call disk_back
.nx:
        pop cx
        pop bx
        mov al, bl
        call hc_sel
        jmp .pl
.cd:
        cmp byte [rs_park], 0
        je .cn
        call park_idle              ; no era una memoria: se vuelve a aparcar
.cn:
        inc bx
        jmp .cl
.hubs:
        xor bx, bx
.hl:
        cmp byte [gone], 0
        je .r
        cmp byte [h_used+bx], 0
        je .hn
        push bx
        mov bh, 1                   ; solo los puertos con cambio
        call hub_scan
        call disk_back
        pop bx
.hn:
        inc bx
        cmp bx, HUB_N
        jb .hl
.r:
        mov byte [only_disk], 0
        pop ax
        mov [opt_verbose], al
        mov [opt_info], ah
        ret
rs_park     db 0

; tras enumerar en rescan: si ha aparecido una memoria, prepararla
disk_back:
        cmp byte [disk_found], 0
        je .r
        cmp byte [gone], 0
        je .r
        call disk_setup
        jc .bad
        mov byte [gone], 0
        ret
.bad:
        call drop_disk
.r:
        ret

; dar de baja los concentradores que colgaban del puerto raíz CL de la
; controladora actual
hubs_drop:
        push bx
        push cx
        xor bx, bx
.l:
        cmp byte [h_used+bx], 0
        je .n
        cmp cl, [h_root+bx]
        jne .n
        mov al, [cur_hc]
        cmp al, [h_hc+bx]
        jne .n
        mov byte [h_used+bx], 0
        push bx
        push cx
        mov al, [h_pool+bx]
        movzx bx, al
        mov byte [pool_used+bx], 0
        call dev_free
        pop cx
        pop bx
.n:
        inc bx
        cmp bx, HUB_N
        jb .l
        pop cx
        pop bx
        ret

; el dispositivo que se va a enumerar cuelga de un puerto del controlador
root_dev:
        mov dword [route], 0
        mov byte [depth], 0
        mov word [tt_slot], 0
        mov byte [par_hub], 0xFF
        mov byte [par_port], 0
        ret

; =============================================================================
;  Concentradores (USB 2.0 cap. 11; USB 3 cap. 10)
; =============================================================================
; petición de control al dispositivo actual: EAX/EDX = los 8 bytes
ctl_req:
        mov [ctl_setup], eax
        mov [ctl_setup+4], edx
        call control
        ret

; dirigir las peticiones de control al concentrador [hp_hub]
hub_sel:
        push bx
        movzx bx, byte [hp_hub]
        mov al, [h_slot+bx]
        mov [cur_slot], al
        mov al, [h_pool+bx]
        call pool_sel
        pop bx
        ret

; reserva: AL = entrada -> cur_pool, cur_ep0, cur_st, cur_out
pool_sel:
        push bx
        mov [cur_pool], al
        call dev_hc                 ; y su controladora pasa a ser la actual
        movzx bx, al
        shl bx, 1
        add bx, pool_st
        mov [cur_st], bx
        movzx bx, al
        shl bx, 8
        add bx, EP0_RINGS
        mov [cur_ep0], bx
        movzx bx, al
        shl bx, 1
        mov bx, [pool_mem+bx]
        mov [cur_out], bx
        pop bx
        ret

; GET_STATUS del puerto [hp_port] del concentrador [hp_hub]
; -> AX = wPortStatus, DX = wPortChange; CF si falla
hub_status:
        push bx
        call hub_sel
        mov eax, 0x000000A3
        movzx edx, byte [hp_port]
        or edx, 0x00040000
        call ctl_req
        jc .r
        mov bx, DESC
        call zone
        mov ax, [cs:bx]
        mov dx, [cs:bx+2]
        clc
.r:
        pop bx
        ret

; SET_FEATURE (AH = 3) o CLEAR_FEATURE (AH = 1) de la característica AL en
; el puerto [hp_port] del concentrador [hp_hub]
hub_feat:
        push bx
        push ax
        call hub_sel
        pop ax
        movzx edx, al
        shl edx, 16
        mov dh, ah
        mov dl, 0x23
        mov eax, edx
        movzx edx, byte [hp_port]
        call ctl_req
        pop bx
        ret

; recorrer los puertos del concentrador BL. BH = 0: todo lo conectado;
; BH = 1: solo los puertos que avisan de un cambio de conexión
hub_scan:
        xor cl, cl
.pl:
        inc cl
        movzx di, bl
        cmp byte [h_used+di], 0
        je .end
        cmp cl, [h_np+di]
        ja .end
        push bx
        push cx
        call hub_port
        pop cx
        pop bx
        jmp .pl
.end:
        ret

; puerto CL del concentrador BL (BH como en hub_scan): si hay un dispositivo
; se reinicia el puerto y se enumera
hub_port:
        mov [hp_hub], bl
        mov [hp_port], cl
        mov [hp_mode], bh
        call hub_status
        jc .r
        cmp byte [hp_mode], 0
        je .a
        test dl, 1
        jz .r
.a:
        test dl, 1
        jz .b
        push ax
        mov ax, 0x0110              ; borrar C_PORT_CONNECTION
        call hub_feat
        pop ax
.b:
        test al, 1
        jz .r
        cmp byte [hp_mode], 0
        je .c
        call reset_rest             ; recién conectado: que se asiente
        call reset_rest
.c:
        ; USB 3: el puerto se habilita solo
        movzx di, byte [hp_hub]
        cmp byte [h_speed+di], 4
        jb .rst
        test al, 2
        jnz .en
.rst:
        mov ax, 0x0304              ; PORT_RESET
        call hub_feat
        jc .r
        mov cx, 10
.rw:
        push cx
        call reset_rest
        call hub_status
        pop cx
        jc .r
        test dl, 0x10               ; C_PORT_RESET
        jnz .rd
        loop .rw
.rd:
        mov ax, 0x0114              ; borrar C_PORT_RESET
        call hub_feat
        call reset_rest
        call hub_status
        jc .r
        test al, 2                  ; habilitado
        jz .r
.en:
        ; velocidad: la del puerto (USB 2: bit 9 baja, bit 10 alta)
        movzx di, byte [hp_hub]
        mov bl, 4
        cmp byte [h_speed+di], 4
        jae .sp
        mov bl, 2
        test ah, 2
        jnz .sp
        mov bl, 3
        test ah, 4
        jnz .sp
        mov bl, 1
.sp:
        mov [speed], bl
        mov al, [h_root+di]
        mov [port], al
        ; cadena de ruta: 4 bits por nivel con el puerto de cada concentrador
        mov cl, [h_depth+di]
        mov al, cl
        inc al
        mov [depth], al
        shl cl, 2
        movzx eax, byte [hp_port]
        shl eax, cl
        mov bx, di
        shl bx, 2
        or eax, [h_route+bx]
        mov [route], eax
        ; traductor de transacciones: un dispositivo de velocidad baja o
        ; plena detrás de un concentrador de alta velocidad usa el de ese
        ; concentrador (y lo hereda lo que cuelgue de él más abajo)
        xor ax, ax
        cmp byte [speed], 3
        jae .tt
        mov al, [h_slot+di]
        mov ah, [hp_port]
        cmp byte [h_speed+di], 3
        je .tt
        mov al, [h_ttslot+di]
        mov ah, [h_ttport+di]
.tt:
        mov [tt_slot], al
        mov [tt_port], ah
        mov ax, di
        mov [par_hub], al
        mov al, [hp_port]
        mov [par_port], al
        call enum_one
.r:
        ret

hp_hub      db 0
hp_port     db 0
hp_mode     db 0
new_hub     db 0
hubf        db 0
hub_np      db 0
hub_ttt     db 0
hub_pwr     db 0
h_used      times HUB_N db 0
h_slot      times HUB_N db 0
h_pool      times HUB_N db 0
h_np        times HUB_N db 0
h_speed     times HUB_N db 0
h_root      times HUB_N db 0
h_depth     times HUB_N db 0
h_ttslot    times HUB_N db 0
h_ttport    times HUB_N db 0
h_route     times HUB_N dd 0
h_hc        times HUB_N db 0        ; su controladora
; el dispositivo que se enumera: dónde cuelga
route       dd 0
depth       db 0
par_hub     db 0xFF
par_port    db 0
cur_pool    db 0
tt_slot     db 0
tt_port     db 0
d_hub       db 0xFF
d_hport     db 0
; reserva de dispositivos: anillo de EP0 y contexto de salida de cada uno
pool_mem    times POOL_N dw 0xFFFF  ; xHCI: su contexto de salida (32 * ctx_sz; FFFFh = sin él)
dv_slot     times POOL_N db 0       ; xHCI: su slot
pool_used   times POOL_N db 0
pool_st     times POOL_N db 0, 1

; xHCI: dejar el puerto CL habilitado (reiniciarlo si es USB 2) -> speed
x_rp_reset:
        movzx esi, cl
        call port_sc
        test al, 1
        jz .no
        test al, 2
        jnz .en
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | (1 << 4) ; PP | PR
        call mwr
        mov cx, 10
        call tmo_start
.pr:
        call mrd
        test eax, 1 << 21           ; PRC
        jnz .prd
        call tmo_check
        jnc .pr
.prd:
        call reset_rest
        call clr_csc
        call mrd
        test al, 2
        jz .no
.en:
        shr eax, 10
        and al, 0x0F
        mov [speed], al
        clc
        ret
.no:
        stc
        ret

; pausa tras reiniciar un puerto: el dispositivo tiene 10 ms para reponerse
; antes de la primera orden (aquí entre 55 y 110 ms)
reset_rest:
        push cx
        mov cx, 2
        call tmo_start
.l:
        call tmo_check
        jnc .l
        pop cx
        ret

dport       db 0
dpool       db 0
gone        db 0
only_disk   db 0

%ifdef USBTRACE
; EAX -> decimal por COM1 (DX = 3F8h)
trace_num:
        push ebx
        push ecx
        push edx
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
        pop ax
        add al, '0'
        mov dx, 0x3F8
        out dx, al
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret
%endif
rw_dir      db 0
opt_verbose_r db 0
rw_n        dw 0
rw_left     dw 0
rw_off      dw 0
rw_seg      dw 0
rw_lba      dd 0

; copiar rw_n sectores entre el búfer DATA y la memoria de DOS
copy_to_dos:
        push ds
        push es
        mov cx, [cs:rw_n]
        shl cx, 8
        mov si, [cs:mem_off]
        add si, [cs:data_rel]
        push cs
        pop ds
        les di, [cs:rw_off]
        cld
        rep movsw
        pop es
        pop ds
        ret
copy_from_dos:
        push ds
        push es
        mov cx, [cs:rw_n]
        shl cx, 8
        mov di, [cs:mem_off]
        add di, [cs:data_rel]
        push cs
        pop es
        lds si, [cs:rw_off]
        cld
        rep movsw
        pop es
        pop ds
        ret

; =============================================================================
;  Utilidades: modo irreal, MMIO, direcciones físicas
; =============================================================================
unreal:
        push eax
        push bx
        smsw ax
        test al, 1
        jnz .r
        cli
        lgdt [cs:gdtr]
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
        sti
.r:
        pop bx
        pop eax
        ret

; EAX con los octetos al revés (BSWAP es del 486)
bswap_eax:
        xchg al, ah
        rol eax, 16
        xchg al, ah
        ret


; ---- línea A20 ----
; Los registros del controlador están por encima de 1 MB, casi siempre en una
; dirección con el bit 20 a uno: con A20 cerrada (la deja así cualquier
; programa que la abra y la cierre por el XMS, y hay BIOS que arrancan con
; ella cerrada) se leería y se escribiría en otra dirección. Se abre mientras
; el driver trabaja y se deja como estaba. Conservan todos los registros y
; los indicadores.
a20_open:
        pushf
        push ax
        cli
        inc byte [cs:a20_depth]
        cmp byte [cs:a20_depth], 1
        jne .r
        call a20_test
        jnz .r
        mov byte [cs:a20_closed], 1
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
        dec byte [cs:a20_depth]
        jnz .r
        cmp byte [cs:a20_closed], 0
        je .r
        mov byte [cs:a20_closed], 0
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
        xor al, al                  ; ZF = 1
        jmp .x
.open:
        or al, 1                    ; ZF = 0
.x:
        pop ax
        pop es
        pop ds
        ret
a20_depth   db 0
a20_closed  db 0

; ---- modo V86 (HSPEMM, EMM386) ----
; iomode: cómo se llega a los registros del xHCI y a la memoria física.
;   0  modo real: modo irreal (FS con límite de 4 GB); lineal = física
;   1  modo V86 con VCPI: INT 15h AH=87h y INT 67h AX=DE06h
;   2  modo V86 sin VCPI (Windows 386 mejorado): no se puede
; Se mira en cada entrada al driver (io_mode): Windows apaga y enciende el
; modo V86 de HSPEMM.
iomode      db 0
            align 4
io_val      dd 0
; tabla de descriptores de INT 15h AH=87h: origen en +10h y destino en +18h
i15_tbl     times 16 db 0
            dw 0xFFFF, 0, 0x9300, 0
            dw 0xFFFF, 0, 0x9300, 0
            times 16 db 0

io_mode:
        push eax
        push ebx
        push ds
        mov byte [cs:iomode], 0
        smsw ax
        test al, 1
        jz .r
        mov byte [cs:iomode], 2
        xor ax, ax
        mov ds, ax
        cmp dword [0x67*4], 0
        je .r
        mov ax, 0xDE00              ; ¿VCPI?
        int 0x67
        test ah, ah
        jnz .r
        mov byte [cs:iomode], 1
.r:
        pop ds
        pop ebx
        pop eax
        ret

; copiar CX palabras de la dirección física ESI a la EDI (modo V86). Las
; direcciones del primer mega son las lineales (las que ve el programa). Con
; CX = 2 es un solo acceso de 32 bits: vale para los registros.
pcopy:
        pushf
        cli
        push eax
        push si
        push es
        mov [cs:i15_tbl+0x12], esi
        mov eax, esi
        shr eax, 24
        mov byte [cs:i15_tbl+0x15], 0x93
        mov byte [cs:i15_tbl+0x16], 0
        mov [cs:i15_tbl+0x17], al
        mov [cs:i15_tbl+0x1A], edi
        mov eax, edi
        shr eax, 24
        mov byte [cs:i15_tbl+0x1D], 0x93
        mov byte [cs:i15_tbl+0x1E], 0
        mov [cs:i15_tbl+0x1F], al
        push cs
        pop es
        mov si, i15_tbl
        mov ah, 0x87
        int 0x15
        pop es
        pop si
        pop eax
        popf
        ret

; EAX = dirección lineal de CS:io_val
io_lin:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, io_val
        ret

; ESI = dirección absoluta
mrd:
        cmp byte [cs:iomode], 0
        jne .v
        mov eax, [fs:esi]
        ret
.v:
        push edi
        push cx
        call io_lin
        mov edi, eax
        mov cx, 2
        call pcopy
        mov eax, [cs:io_val]
        pop cx
        pop edi
        ret
mwr:
        cmp byte [cs:iomode], 0
        jne .v
        mov [fs:esi], eax
        ret
.v:
        push eax
        push esi
        push edi
        push cx
        mov edi, esi
        mov [cs:io_val], eax
        call io_lin
        mov esi, eax
        mov cx, 2
        call pcopy
        pop cx
        pop edi
        pop esi
        pop eax
        ret

; CS:BX -> EAX física. En modo V86 la lineal no es la física (el driver
; puede estar en la memoria superior): se pregunta página a página por VCPI.
; Nada de lo que se entrega al controlador cruza una página de 4 KB.
phys:
        push ebx
        movzx ebx, bx
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, ebx
        pop ebx
; EAX = dirección lineal -> EAX física
lphys:
        cmp byte [cs:iomode], 1
        jne .r
        push ebx
        push ecx
        push edx
        mov ebx, eax
        mov ecx, eax
        shr ecx, 12
        mov edx, eax
        mov ax, 0xDE06
        int 0x67
        mov eax, edx
        and eax, 0xFFFFF000
        and ebx, 0xFFF
        or eax, ebx
        pop edx
        pop ecx
        pop ebx
.r:
        ret

; BX = desplazamiento dentro de la zona -> BX = desplazamiento en CS
zone:
        add bx, [cs:mem_off]
        ret

; plazo: CX = ticks (18,2 por segundo)
tmo_start:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        mov [cs:t0], ax
        mov [cs:tlen], cx
        pop ax
        pop ds
        ret
tmo_check:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        sub ax, [cs:t0]
        cmp ax, [cs:tlen]
        pop ax
        pop ds
        cmc
        ret                         ; CF=1 si se agotó
t0          dw 0
tlen        dw 0

; =============================================================================
;  Anillos de TRB
; =============================================================================
; poner un TRB: BX = anillo (desplazamiento en la zona), SI = &índice,
; EAX/EDX/ECX = dwords 0-2, EBP = dword 3 (sin el bit de ciclo)
put_trb:
        push di
        push bx
        call zone
        movzx di, byte [cs:si]
        shl di, 4
        add di, bx
        mov [cs:di], eax
        mov [cs:di+4], edx
        mov [cs:di+8], ecx
        mov eax, ebp
        or al, [cs:si+1]            ; bit de ciclo
        mov [cs:di+12], eax
        inc byte [cs:si]
        cmp byte [cs:si], RING_TRBS - 1
        jb .r
        ; TRB de enlace al principio del anillo (cambia el ciclo)
        add di, 16
        call phys                   ; BX = inicio del anillo
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        mov dword [cs:di+8], 0
        mov eax, (6 << 10) | 2      ; Link, Toggle Cycle
        or al, [cs:si+1]
        mov [cs:di+12], eax
        mov byte [cs:si], 0
        xor byte [cs:si+1], 1
.r:
        pop bx
        pop di
        ret

; tomar el siguiente evento del anillo -> evt, AL = tipo; CF si no hay
get_event:
        push bx
        push di
        push esi
        mov bx, EVT_RING
        call zone
        movzx di, byte [cs:evt_idx]
        shl di, 4
        add di, bx
        mov eax, [cs:di+12]
        and al, 1
        cmp al, [cs:evt_cyc]
        jne .none
        mov eax, [cs:di]
        mov [cs:evt], eax
        mov eax, [cs:di+4]
        mov [cs:evt+4], eax
        mov eax, [cs:di+8]
        mov [cs:evt+8], eax
        mov eax, [cs:di+12]
        mov [cs:evt+12], eax
        inc byte [cs:evt_idx]
        cmp byte [cs:evt_idx], RING_TRBS
        jb .nw
        mov byte [cs:evt_idx], 0
        xor byte [cs:evt_cyc], 1
.nw:
        ; ERDP = siguiente posición (y borrar EHB)
        movzx bx, byte [cs:evt_idx]
        shl bx, 4
        add bx, EVT_RING
        call zone
        call phys
        or al, 8
        mov esi, [cs:rt_base]
        add esi, 0x20 + 0x18
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov eax, [cs:evt+12]
        shr eax, 10
        and al, 0x3F
        pop esi
        pop di
        pop bx
        clc
        ret
.none:
        pop esi
        pop di
        pop bx
        stc
        ret

; esperar un evento del tipo AL -> evt; CF si vence el plazo. Los informes
; del teclado y del ratón que lleguen mientras tanto se atienden aquí.
wait_event:
        push ax
        push cx
        mov [cs:want], al
        mov cx, [cs:ev_tmo]
        call tmo_start
.l:
        call get_event
        jc .none
        cmp al, 32
        jne .chk
        call hid_event
        jnc .l
        mov al, 32
.chk:
        cmp al, [cs:want]
        je .ok
        jmp .l
.none:
        call tmo_check
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
want        db 0
ev_tmo      dw 40                   ; ~2 s (órdenes y control)

; timbre: AL = destino, AH = slot (0 = controlador)
doorbell:
        push esi
        push eax
        movzx esi, ah
        shl esi, 2
        add esi, [cs:db_base]
        movzx eax, al
        call mwr
        pop eax
        pop esi
        ret

; orden al controlador: EAX/EDX/ECX/EBP como put_trb -> CF si falla,
; AL = código de finalización, AH = slot
xhci_cmd:
        push bx
        push si
        mov bx, CMD_RING
        mov si, cmd_idx
        call put_trb
        xor ax, ax
        call doorbell
        mov al, 33                  ; Command Completion
        call wait_event
        jnc .got
        mov al, 0xFF
        jmp .r
.got:
        mov al, [cs:evt+11]         ; código
        mov ah, [cs:evt+15]         ; slot
        cmp al, 1
        je .r
        stc
.r:
        pop si
        pop bx
        ret

; esperar una transferencia -> CF si falla (éxito o paquete corto valen)
wait_xfer:
        mov byte [cs:xf_code], 0xFF
        mov al, 32
        call wait_event
        jc .r
        mov al, [cs:evt+11]
        mov [cs:xf_code], al
        cmp al, 1
        je .ok
        cmp al, 13
        je .ok
        stc
        ret
.ok:
        clc
.r:
        ret

; xHCI: transferencia de control: setup en ctl_setup (8 bytes), datos en
; [ctl_buf] (entrada). La entrada común es «control» (lib/usb_hc.inc).
x_control:
        push bx
        push si
        mov eax, [cs:ctl_setup]
        mov edx, [cs:ctl_setup+4]
        mov ecx, 8
        mov ebp, (2 << 10) | (1 << 6)       ; Setup, datos inmediatos
        cmp word [cs:ctl_setup+6], 0
        je .nodata
        or ebp, 3 << 16                     ; TRT = IN
.nodata:
        mov bx, [cs:cur_ep0]
        mov si, [cs:cur_st]
        call put_trb
        cmp word [cs:ctl_setup+6], 0
        je .status
        mov bx, [cs:ctl_buf]
        call zone
        call phys
        xor edx, edx
        movzx ecx, word [cs:ctl_setup+6]
        mov ebp, (3 << 10) | (1 << 16)      ; Data, IN
        mov bx, [cs:cur_ep0]
        call put_trb
.status:
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        mov ebp, (4 << 10) | (1 << 5)       ; Status, IOC
        cmp word [cs:ctl_setup+6], 0
        jne .st
        or ebp, 1 << 16                     ; sin datos: estado IN
.st:
        mov bx, [cs:cur_ep0]
        mov si, [cs:cur_st]
        call put_trb
        mov al, 1
        mov ah, [cs:cur_slot]
        call doorbell
        call wait_xfer
        jnc .r
        cmp byte [cs:xf_code], 0xFF
        je .to
        call ep0_recover
.to:
        stc
.r:
        pop si
        pop bx
        ret
            align 8                 ; (que no cruce de página: va a las controladoras)
ctl_setup   dd 0, 0
xf_code     db 0

; Una petición de control rechazada (STALL) o con error deja parado el EP0
; en el controlador: hay que reanudarlo (Reset Endpoint) y saltarse lo que
; quedara de la transferencia (Set TR Dequeue Pointer), o las peticiones
; siguientes a ese dispositivo no saldrían nunca.
ep0_recover:
        pushad
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cs:cur_slot]
        shl ebp, 24
        or ebp, (14 << 10) | (1 << 16)      ; Reset Endpoint, EP0
        call xhci_cmd
        mov si, [cs:cur_st]
        movzx bx, byte [cs:si]
        shl bx, 4
        add bx, [cs:cur_ep0]
        call zone
        call phys
        or al, [cs:si+1]                    ; estado del ciclo
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cs:cur_slot]
        shl ebp, 24
        or ebp, (16 << 10) | (1 << 16)      ; Set TR Dequeue Pointer, EP0
        call xhci_cmd
        popad
        ret

; xHCI: transferencia bulk: AL = 1 entrada / 0 salida, EBX = dirección
; física, CX = bytes. La entrada común es «bulk» (lib/usb_hc.inc).
x_bulk:
        push si
        push di
        mov edi, ebx
        mov [cs:bk_dir], al
        mov eax, edi
        xor edx, edx
        movzx ecx, cx
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)    ; Normal, IOC, ISP
        cmp byte [cs:bk_dir], 0
        je .out
        mov bx, BIN_RING
        mov si, bin_idx
        call put_trb
        mov al, [cs:dci_in]
        jmp .ring
.out:
        mov bx, BOUT_RING
        mov si, bout_idx
        call put_trb
        mov al, [cs:dci_out]
.ring:
        mov ah, [cs:slot]
        call doorbell
        ; un disco USB de verdad puede tardar segundos (arranque del motor,
        ; borrado de bloques de una memoria llena): 5 s, no 2
        mov word [cs:ev_tmo], 91
        call wait_xfer
        mov word [cs:ev_tmo], 40
        pop di
        pop si
        ret
bk_dir      db 0

%include "usb_hc.inc"
%include "usb_uhci.inc"
%include "usb_ohci.inc"
%include "usb_ehci.inc"

; =============================================================================
;  Almacenamiento masivo (Bulk-Only Transport + SCSI)
; =============================================================================
; orden SCSI en scsi_cb (scsi_cblen bytes), datos: EBX física, ECX bytes,
; AL = 1 entrada -> CF si falla. En las controladoras clásicas, si falla
; (un paquete perdido, un STALL, los testigos de alternancia desacompasados)
; se hace la recuperación de Bulk-Only (reinicio de la memoria y
; CLEAR_FEATURE de los dos endpoints, testigos a DATA0) y se repite una vez.
; (En el xHCI no: el endpoint parado pediría Reset Endpoint.)
scsi:
        mov [cs:sc_a], eax
        mov [cs:sc_b], ebx
        mov [cs:sc_c], ecx
        call scsi_once
        jnc .ok
        cmp byte [cs:sc_tr], 0
        je .ko
        cmp byte [cs:cur_type], HCT_XHCI
        je .ko
        call bot_reset
        mov eax, [cs:sc_a]
        mov ebx, [cs:sc_b]
        mov ecx, [cs:sc_c]
        call scsi_once
        jnc .ok
.ko:
        mov eax, [cs:sc_a]
        mov ebx, [cs:sc_b]
        mov ecx, [cs:sc_c]
        stc
        ret
.ok:
        mov eax, [cs:sc_a]
        mov ebx, [cs:sc_b]
        mov ecx, [cs:sc_c]
        ret
sc_a        dd 0
sc_b        dd 0
sc_c        dd 0

; recuperación de Bulk-Only (BOT 1.0, 5.3.4) de la memoria USB [bk_pool]
bot_reset:
        pushad
        push ds
        push cs
        pop ds
        mov al, [cur_pool]
        push ax
        mov al, [bk_pool]
        call pool_sel
        ; Bulk-Only Mass Storage Reset: 21h FFh, wIndex = la interfaz
        mov dword [ctl_setup], 0x0000FF21
        movzx eax, byte [d_if]
        mov [ctl_setup+4], eax
        call control
        ; CLEAR_FEATURE(ENDPOINT_HALT) de bulk IN y de bulk OUT
        mov dword [ctl_setup], 0x00000102
        movzx eax, byte [dci_in]
        shr al, 1
        or al, 0x80
        mov [ctl_setup+4], eax
        call control
        mov dword [ctl_setup], 0x00000102
        movzx eax, byte [dci_out]
        shr al, 1
        mov [ctl_setup+4], eax
        call control
        mov word [tog_in], 0        ; (y tog_out)
        pop ax
        call pool_sel
        pop ds
        popad
        ret

scsi_once:
        push bx
        push di
        mov byte [cs:sc_tr], 1
        mov [cs:sc_dir], al
        mov [cs:sc_len], ecx
        mov [cs:sc_buf], ebx
        mov bx, CBW_BUF
        call zone
        mov di, bx
        mov dword [cs:di], 0x43425355       ; "USBC"
        inc dword [cs:tag]
        mov eax, [cs:tag]
        mov [cs:di+4], eax
        mov eax, [cs:sc_len]
        mov [cs:di+8], eax
        mov al, 0
        cmp byte [cs:sc_dir], 0
        je .f
        mov al, 0x80
.f:
        mov [cs:di+12], al
        mov byte [cs:di+13], 0              ; LUN 0
        mov al, [cs:scsi_cblen]
        mov [cs:di+14], al
        push si
        mov si, scsi_cb
        mov cx, 16
.cb:
        mov al, [cs:si]
        mov [cs:di+15], al
        inc si
        inc di
        loop .cb
        pop si
        mov bx, CBW_BUF
        call zone
        call phys
        mov ebx, eax
        mov cx, 31
        xor al, al
        call bulk
        jc .r
        mov ecx, [cs:sc_len]
        jecxz .csw
        mov ebx, [cs:sc_buf]
        mov al, [cs:sc_dir]
        call bulk
        jc .r
.csw:
        mov bx, CSW_BUF
        call zone
        mov di, bx
        call phys
        mov ebx, eax
        mov cx, 13
        mov al, 1
        call bulk
        jc .r
        cmp dword [cs:di], 0x53425355       ; "USBS"
        jne .bad                            ; (sc_tr = 1)
        mov byte [cs:sc_tr], 0
        cmp byte [cs:di+12], 1
        je .bad                             ; la orden ha fallado: no es del transporte
        cmp byte [cs:di+12], 0
        je .r                               ; (CF = 0)
        mov byte [cs:sc_tr], 1              ; error de fase
.bad:
        stc
.r:
        pop di
        pop bx
        ret
sc_tr       db 0                            ; ha fallado el transporte: hay que recuperar
sc_dir      db 0
sc_len      dd 0
sc_buf      dd 0
scsi_cblen  db 0
scsi_cb     times 16 db 0

; READ(10) (AL=28h) o WRITE(10) (AL=2Ah) de rw_n sectores en rw_lba con DATA
scsi_rw:
        push bx
        mov [cs:scsi_cb], al
        mov byte [cs:scsi_cblen], 10
        mov byte [cs:scsi_cb+1], 0
        mov eax, [cs:rw_lba]
        call bswap_eax
        mov [cs:scsi_cb+2], eax
        mov byte [cs:scsi_cb+6], 0
        mov ax, [cs:rw_n]
        xchg al, ah
        mov [cs:scsi_cb+7], ax
        mov byte [cs:scsi_cb+9], 0
        mov bx, [cs:data_rel]
        call zone
        call phys
        mov ebx, eax
        movzx ecx, word [cs:rw_n]
        shl ecx, 9
        mov al, 1
        cmp byte [cs:scsi_cb], 0x28
        je .d
        xor al, al
.d:
        call scsi
        pop bx
        ret

; =============================================================================
;  Teclado y ratón USB (HID, protocolo de arranque)
; =============================================================================
; evento de transferencia en evt: si es del teclado o del ratón se atiende y
; se vuelve a pedir el siguiente informe; CF si no es suyo
hid_event:
        push ax
        push bx
        mov al, [cs:evt+14]
        and al, 0x1F                ; endpoint
        mov ah, [cs:evt+15]         ; slot
        xor bx, bx
.l:
        cmp byte [cs:ip_on+bx], 0
        je .n
        cmp ah, [cs:ip_slot+bx]
        jne .n
        cmp al, [cs:ip_dci+bx]
        jne .n
        push bx                     ; (que la tubería sea del xHCI)
        movzx bx, byte [cs:ip_pool+bx]
        movzx bx, byte [cs:dv_hc+bx]
        cmp byte [cs:hc_type+bx], HCT_XHCI
        pop bx
        jne .n
        call ev_ok
        jc .q
        push eax
        push esi
        mov si, bx
        shl si, 1
        mov eax, [cs:evt+8]
        and eax, 0x00FFFFFF         ; lo que ha faltado por llegar
        movzx esi, word [cs:ip_len+si]
        sub esi, eax
        jae .g
        xor si, si
.g:
        mov [cs:ip_got], si
        pop esi
        pop eax
        call pipe_done
.q:
        call pipe_queue
        pop bx
        pop ax
        clc
        ret
.n:
        inc bx
        cmp bx, PIPE_N
        jb .l
        pop bx
        pop ax
        stc
        ret

; ---- mandos de juego ----
; pedir el siguiente informe del mando BX
joy_queue:
        push bx
        add bx, 2
        call pipe_queue
        pop bx
        ret

; xHCI: pedir el siguiente informe de la tubería BX
x_pipe_queue:
        pushad
        mov di, bx
        shl bx, 6
        add bx, PIPE_BUF
        call zone
        call phys
        xor edx, edx
        mov si, di
        shl si, 1
        movzx ecx, word [cs:ip_len+si]
        add si, ip_ring
        mov bx, di
        shl bx, 8
        add bx, PINT_RING
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)    ; Normal, IOC, ISP
        call put_trb
        mov al, [cs:ip_dci+di]
        mov ah, [cs:ip_slot+di]
        call doorbell
        popad
        ret

; ha llegado un informe del mando BX ([ip_got] bytes): pasarlo al estado con
; su mapa y entregarlo a HSPJOY
joy_report:
        pushad
        push ds
        push cs
        pop ds
        mov cx, [ip_got]
        jcxz .r
        mov si, bx
        imul si, HJM_SZ
        add si, j_map
        mov di, bx
        imul di, HJS_SZ
        add di, j_est
        push bx
        shl bx, 6
        add bx, JOY_BUF
        call zone
        call hj_extract
        pop bx
        jc .r
        bts word [j_ev], bx
        call joy_push
.r:
        pop ds
        popad
        ret

; entregar a HSPJOY el estado del mando BX: INT 2Fh AX=D903h, DL = mando,
; ES:DI = estado (HJS_SZ bytes). Si HSPJOY no está, nadie contesta.
joy_push:
        pushad
        push ds
        push es
        xor ax, ax
        mov es, ax
        cmp dword [es:0x2F*4], 0
        je .r
        push cs
        pop es
        mov di, bx
        imul di, HJS_SZ
        add di, j_est
        mov dl, bl
        mov ax, 0xD903
        int 0x2F
        cmp al, 0xFF                ; ¿lo ha recogido HSPJOY?
        sete [cs:j_ack]
.r:
        pop es
        pop ds
        popad
        ret

; Tras atender el informe de algún mando (j_ev) ya está pedido el siguiente:
; se le espera lo que dura su intervalo (j_wait microtramas del reloj de su
; controladora, hc_time) para entregar un estado de ahora mismo. El que acaba de
; atenderse puede llevar esperando desde el tic anterior (55 ms).
joy_fresh:
        pushad
        mov bl, [cs:j_ev]
        mov byte [cs:j_ev], 0
        ; el reloj: el de la controladora del primero de esos mandos
        xor si, si
        test bl, 1
        jnz .h
        inc si
.h:
        movzx si, byte [cs:ip_pool+si+2]
        mov al, [cs:dv_hc+si]
        mov [cs:jf_hc], al
        call hc_sel
        call hc_time
        mov di, ax
        mov ecx, 200000             ; (por si el reloj no avanzara)
.l:
        call usb_poll
        mov al, [cs:j_ev]
        and al, bl
        cmp al, bl
        je .r
        dec ecx
        jz .r
        mov al, [cs:jf_hc]
        call hc_sel
        call hc_time
        sub ax, di
        and ax, 0x3FFF
        cmp ax, [cs:j_wait]
        jb .l
.r:
        mov byte [cs:j_ev], 0
        popad
        ret
jf_hc       db 0

%include "hidjoyx.inc"
j_est       times JOY_N * HJS_SZ db 0   ; estado de cada mando (lo que va a HSPJOY)
j_map       times JOY_N * HJM_SZ db 0xFF ; dónde va cada cosa en su informe
; ¿transferencia correcta? (éxito o paquete corto)
ev_ok:
        cmp byte [cs:evt+11], 1
        je .y
        cmp byte [cs:evt+11], 13
        je .y
        stc
        ret
.y:
        clc
        ret

; pedir el siguiente informe
kbd_queue:
        push bx
        xor bx, bx
        call pipe_queue
        pop bx
        ret
mou_queue:
        push bx
        mov bx, 1
        call pipe_queue
        pop bx
        ret

; ---- teclado: comparar el informe con el anterior ----
kbd_report:
        pushad
        mov bx, KBD_BUF
        call zone
        xor di, di
.cp:
        mov al, [cs:bx+di]
        mov [cs:newrep+di], al
        inc di
        cmp di, 8
        jb .cp
        ; "demasiadas teclas" (01h): se ignora el informe
        mov di, 2
.ro:
        cmp byte [cs:newrep+di], 1
        je .r
        inc di
        cmp di, 8
        jb .ro
        ; modificadores
        mov dl, [cs:newrep]
        mov dh, [cs:oldrep]
        xor bx, bx
.mo:
        mov al, dl
        xor al, dh
        bt ax, bx
        jnc .mn
        mov al, [cs:modtab+bx]
        mov ah, 0x80                ; soltada
        bt dx, bx
        jnc .mb
        xor ah, ah                  ; pulsada
.mb:
        call send_key
.mn:
        inc bx
        cmp bx, 8
        jb .mo
        ; teclas soltadas
        mov di, 2
.rl:
        mov al, [cs:oldrep+di]
        cmp al, 4
        jb .rn
        mov si, newrep
        call in_rep
        jnc .rn
        cmp al, [cs:rep_key]
        jne .rk
        mov byte [cs:rep_key], 0
.rk:
        call usage_code
        mov ah, 0x80
        call send_key
.rn:
        inc di
        cmp di, 8
        jb .rl
        ; teclas pulsadas
        mov di, 2
.pl:
        mov al, [cs:newrep+di]
        cmp al, 4
        jb .pn
        mov si, oldrep
        call in_rep
        jnc .pn
        mov [cs:rep_key], al
        mov byte [cs:rep_cnt], REP_DELAY
        call usage_code
        xor ah, ah
        call send_key
.pn:
        inc di
        cmp di, 8
        jb .pl
        ; guardar
        xor di, di
.sv:
        mov al, [cs:newrep+di]
        mov [cs:oldrep+di], al
        inc di
        cmp di, 8
        jb .sv
.r:
        popad
        ret

; ¿está la tecla AL en las posiciones 2-7 del informe CS:SI? CF si NO está
in_rep:
        push bx
        mov bx, 2
.l:
        cmp al, [cs:si+bx]
        je .y
        inc bx
        cmp bx, 8
        jb .l
        pop bx
        stc
        ret
.y:
        pop bx
        clc
        ret

; uso HID AL -> AL = código de tecla (bit 7 = prefijo E0h; 0 = ninguno)
usage_code:
        cmp al, 0x65
        ja .no
        sub al, 4
        jb .no
        push bx
        movzx bx, al
        mov al, [cs:keytab+bx]
        pop bx
        ret
.no:
        xor al, al
        ret

; enviar la tecla AL (bit 7 = E0h) pulsada (AH = 0) o soltada (AH = 80h)
send_key:
        test al, al
        jz .r
        cmp byte [cs:kbc_ok], 0
        je soft_key
        push ax
        test al, 0x80
        jz .n
        mov al, 0xE0
        call kbc_put
.n:
        pop ax
        push ax
        and al, 0x7F
        or al, ah
        call kbc_put
        pop ax
.r:
        ret

; Sin controlador de teclado (8042) -los Mac, por ejemplo- no hay por dónde
; hacer llegar el código a la BIOS (INT 09h lee el puerto 60h). La tecla se
; resuelve aquí: primero se le ofrece a KEYB (INT 2Fh AX=AD81h, distribución
; española); si no la cambia, se traduce con la distribución estadounidense
; y se deja en el búfer de teclado de la BIOS (0040:001A), manteniendo los
; indicadores de Mayús, Ctrl, Alt y los bloqueos en 0040:0017.
; AL = código (bit 7 = prefijo E0h), AH = 0 pulsada / 80h soltada
soft_key:
        pushad
        push ds
        push es
        mov bl, al
        and bl, 0x7F
        mov bh, al
        and bh, 0x80
        mov dl, ah
        test bh, bh
        jz .k1
        push bx
        push dx
        mov bl, 0xE0
        mov ax, 0xAD81
        int 0x2F
        pop dx
        pop bx
.k1:
        push bx
        push dx
        or bl, dl
        mov ax, 0xAD81
        int 0x2F
        pop dx
        pop bx
        cmp ax, 0x4B01
        je .done
        mov ax, 0x40
        mov ds, ax
        ; Mayús, Ctrl y Alt
        xor al, al
        cmp bl, 0x36
        jne .m1
        mov al, 0x01
.m1:
        cmp bl, 0x2A
        jne .m2
        mov al, 0x02
.m2:
        cmp bl, 0x1D
        jne .m3
        mov al, 0x04
.m3:
        cmp bl, 0x38
        jne .m4
        mov al, 0x08
.m4:
        test al, al
        jz .nomod
        test dl, dl
        jnz .rel
        or [0x17], al
        jmp .done
.rel:
        not al
        and [0x17], al
        jmp .done
.nomod:
        ; bloqueos: cambian al pulsar (la repetición no cuenta)
        mov al, 0x40
        cmp bl, 0x3A
        je .tog
        mov al, 0x20
        cmp bl, 0x45
        je .tog
        mov al, 0x10
        cmp bl, 0x46
        je .tog
        test dl, dl
        jnz .done                   ; soltar las demás: nada
        jmp .key
.tog:
        test dl, dl
        jnz .trel
        test [0x18], al
        jnz .done
        or [0x18], al
        xor [0x17], al
        jmp .done
.trel:
        not al
        and [0x18], al
        jmp .done
.key:
        mov dh, [0x17]
        ; Ctrl+Alt+Supr: reinicio en caliente
        mov al, dh
        and al, 0x0C
        cmp al, 0x0C
        jne .nocad
        cmp bl, 0x53
        jne .nocad
        mov word [0x72], 0x1234
        jmp 0xFFFF:0x0000
.nocad:
        test bh, bh
        jz .plain
        ; teclas extendidas: Intro y / del teclado numérico, cursor
        mov cx, 0x1C0D
        cmp bl, 0x1C
        je .put
        mov cx, 0x352F
        cmp bl, 0x35
        je .put
        cmp bl, 0x47
        jb .done
        cmp bl, 0x53
        ja .done
        jmp .nav
.plain:
        cmp bl, 0x47
        jb .p1
        cmp bl, 0x53
        ja .p1
        ; teclado numérico: cifras con BloqNum (Mayús lo invierte)
        cmp bl, 0x4A
        je .kpc
        cmp bl, 0x4E
        je .kpc
        mov al, dh
        and al, 3
        jz .ns
        mov al, 0x20
.ns:
        xor al, dh
        test al, 0x20
        jz .nav
.kpc:
        movzx si, bl
        mov cl, [cs:kp_tab+si-0x47]
        mov ch, bl
        jmp .put
.nav:
        mov ch, bl
        xor cl, cl
        test dh, 4
        jz .put
        movzx si, bl
        mov ch, [cs:nav_ctrl+si-0x47]
        jmp .put
.p1:
        ; F1-F10 (con Mayús, Ctrl o Alt cambian de código)
        cmp bl, 0x3B
        jb .p2
        cmp bl, 0x44
        ja .p2
        mov ch, bl
        xor cl, cl
        test dh, 8
        jz .f1
        add ch, 0x68 - 0x3B
        jmp .put
.f1:
        test dh, 4
        jz .f2
        add ch, 0x5E - 0x3B
        jmp .put
.f2:
        test dh, 3
        jz .put
        add ch, 0x54 - 0x3B
        jmp .put
.p2:
        cmp bl, 0x57                ; F11 y F12
        jb .p3
        cmp bl, 0x58
        ja .p3
        mov ch, bl
        add ch, 0x85 - 0x57
        xor cl, cl
        test dh, 3
        jz .put
        add ch, 2
        jmp .put
.p3:
        cmp bl, 0x56                ; la tecla de más junto a Mayús
        jne .p4
        mov cx, 0x565C
        test dh, 3
        jz .put
        mov cl, '|'
        jmp .put
.p4:
        cmp bl, 0x39
        ja .done
        mov ch, bl
        movzx si, bl
        mov cl, [cs:asc_lo+si]
        mov al, [cs:asc_hi+si]
        test dh, 8
        jz .na
        ; Alt: sin carácter (las cifras, con otro código)
        xor cl, cl
        cmp bl, 0x02
        jb .put
        cmp bl, 0x0D
        ja .put
        add ch, 0x76
        jmp .put
.na:
        test dh, 4
        jz .nc
        ; Ctrl + letra: 01h-1Ah; unas pocas más
        cmp cl, 'a'
        jb .c1
        cmp cl, 'z'
        ja .c1
        and cl, 0x1F
        jmp .put
.c1:
        mov si, ctrl_keys
.c2:
        mov al, [cs:si]
        test al, al
        jz .done
        cmp al, bl
        je .c3
        add si, 2
        jmp .c2
.c3:
        mov cl, [cs:si+1]
        jmp .put
.nc:
        cmp cl, 'a'
        jb .sym
        cmp cl, 'z'
        ja .sym
        ; letras: mayúscula con Mayús o con Bloq Mayús (no con las dos)
        mov ah, dh
        and ah, 3
        jz .l1
        mov ah, 0x40
.l1:
        xor ah, dh
        test ah, 0x40
        jz .put
        mov cl, al
        jmp .put
.sym:
        test dh, 3
        jz .put
        mov cl, al
.put:
        mov ax, cx
        test ax, ax
        jz .done
        ; al búfer de teclado de la BIOS
        pushf
        cli
        mov si, [0x1C]
        mov di, si
        add di, 2
        mov ax, [0x82]
        test ax, ax
        jnz .b1
        mov ax, 0x3E
.b1:
        cmp di, ax
        jb .b2
        mov di, [0x80]
        test di, di
        jnz .b2
        mov di, 0x1E
.b2:
        cmp di, [0x1A]
        je .bfull                   ; lleno: la tecla se pierde
        mov [si], cx
        mov [0x1C], di
.bfull:
        popf
.done:
        pop es
        pop ds
        popad
        ret
; caracteres de los códigos 00h-39h, sin y con Mayús (teclado estadounidense)
asc_lo  db 0, 27, "1234567890-=", 8, 9, "qwertyuiop[]", 13, 0, "asdfghjkl;'`", 0
        db "\zxcvbnm,./", 0, "*", 0, " "
asc_hi  db 0, 27, "!@#$%^&*()_+", 8, 0, "QWERTYUIOP{}", 13, 0, 'ASDFGHJKL:"~', 0
        db "|ZXCVBNM<>?", 0, "*", 0, " "
kp_tab  db "789-456+1230."
nav_ctrl db 0x77, 0x8D, 0x84, 0x8E, 0x73, 0x8F, 0x74, 0x90, 0x75, 0x91, 0x76, 0x92, 0x93
ctrl_keys db 0x1A, 0x1B, 0x1B, 0x1D, 0x2B, 0x1C, 0x1C, 0x0A, 0x0E, 0x7F, 0x39, 0x20, 0x01, 0x1B, 0

; poner AL en el búfer de salida del 8042 como si viniera del teclado
; (orden D2h) o del ratón PS/2 (orden D3h)
kbc_put:
        push bx
        mov bl, 0xD2
        call kbc_out
        pop bx
        ret
kbc_aux:
        push bx
        mov bl, 0xD3
        call kbc_out
        pop bx
        ret
kbc_out:
        cmp byte [cs:kbc_ok], 0
        jne .go
        ret
.go:
        push cx
        push ax
        sti
        call kbc_drain              ; que se haya leído el anterior
        call kbc_ibf
        mov al, bl
        out 0x64, al
        call kbc_ibf
        pop ax
        push ax
        out 0x60, al
        call kbc_drain              ; IRQ 1 / 12: la BIOS lo lee
        pop ax
        pop cx
        ret
kbc_ok      db 1                    ; hay controlador de teclado (8042)
kbc_ibf:
        push ax
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 2
        jz .r
        loop .l
.r:
        pop ax
        ret
kbc_drain:
        push ax
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 1
        jz .r
        loop .l
.r:
        pop ax
        ret

; ---- ratón ----
mouse_report:
        pushad
        push es
        mov bx, MOU_BUF
        call zone
        mov si, bx
        cmp byte [cs:hr_on], 0
        je .boot
        ; [USB-RATON] informes «de informe»: según su descriptor
        call hr_extract
        jc .r                       ; otro informe del mismo aparato
        jmp .have
.boot:
        movzx bx, byte [cs:si]      ; botones
        and bl, 7
        movsx cx, byte [cs:si+1]    ; x
        movsx dx, byte [cs:si+2]    ; y (positivo hacia abajo)
.have:
        ; ¿está HSPRATON?
        xor ax, ax
        mov es, ax
        cmp dword [es:0x33*4], 0
        je .ps2
        push bx
        push cx
        push dx
        mov ax, 0x48D2
        int 0x33
        pop dx
        pop cx
        pop bx
        cmp ax, 0x5253
        jne .ps2
        mov ax, 0x48D0
        int 0x33
        jmp .r
.ps2:
        ; paquete PS/2: botones | 08h | signos, x, y (positivo hacia arriba)
        call clamp127
        xchg cx, dx
        call clamp127
        xchg cx, dx
        neg dx
        mov al, bl
        or al, 8
        test cx, cx
        jns .sx
        or al, 0x10
.sx:
        test dx, dx
        jns .sy
        or al, 0x20
.sy:
        call kbc_aux
        mov al, cl
        call kbc_aux
        mov al, dl
        call kbc_aux
.r:
        pop es
        popad
        ret

; CX -> CX entre -127 y 127 (lo que cabe en un paquete PS/2)
clamp127:
        cmp cx, 127
        jle .a
        mov cx, 127
.a:
        cmp cx, -127
        jge .b
        mov cx, -127
.b:
        ret
%include "hidratx.inc"

; ---- INT 08h: recoger informes y repetir la tecla mantenida ----
int08:
        pushf
        call far [cs:old08]
        cmp byte [cs:busy], 0
        jne .ret
        mov byte [cs:busy], 1
        ; pila propia: lo que sigue (los informes, INT 33h con la rutina del
        ; programa, las interrupciones que entren mientras tanto) gasta
        ; bastante más de lo que se le puede pedir a la pila del programa
        ; interrumpido. busy impide que se entre dos veces.
        mov [cs:sv_ss], ss
        mov [cs:sv_sp], sp
        push cs
        pop ss
        mov sp, istack_top
        sti
        pushad
        push ds
        push es
        push fs
        call io_mode
        cmp byte [cs:iomode], 1
        je .v86                     ; modo V86 con VCPI: sin modo irreal
        ja .out                     ; modo V86 sin VCPI: no se puede
        sgdt [cs:sv_gdtr]
        call unreal
.v86:
        call a20_open
        call usb_poll
        ; mandos: el informe reciente y, cada 4 tics, su estado a HSPJOY
        ; (para que sepa que están aunque no se muevan)
        cmp byte [cs:j_ev], 0
        je .jk
        cmp byte [cs:j_ack], 0
        jne .jf
        mov byte [cs:j_ev], 0       ; nadie los recoge: no hay prisa
        jmp .jk
.jf:
        call joy_fresh
.jk:
        inc byte [cs:j_tick]
        test byte [cs:j_tick], 3
        jnz .jd
        xor bx, bx
.jl:
        cmp byte [cs:j_on+bx], 0
        je .jn
        call joy_push
.jn:
        inc bx
        cmp bx, JOY_N
        jb .jl
.jd:
        cmp byte [cs:rep_key], 0
        je .g
        dec byte [cs:rep_cnt]
        jnz .g
        mov byte [cs:rep_cnt], REP_RATE
        mov al, [cs:rep_key]
        call usage_code
        xor ah, ah
        call send_key
.g:
        call a20_restore
        cmp byte [cs:iomode], 0
        jne .out
        cli
        lgdt [cs:sv_gdtr]
.out:
        pop fs
        pop es
        pop ds
        popad
        cli
        mov ss, [cs:sv_ss]
        mov sp, [cs:sv_sp]
        mov byte [cs:busy], 0
.ret:
        iret
sv_ss       dw 0
sv_sp       dw 0
            align 2
istack      times 1024 db 0
istack_top:

; códigos de tecla (juego 1 del PC) de los usos HID 04h-65h; bit 7 = E0h
keytab:
        db 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23  ; a-h
        db 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19  ; i-p
        db 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D  ; q-x
        db 0x15, 0x2C                                      ; y z
        db 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B  ; 1-0
        db 0x1C, 0x01, 0x0E, 0x0F, 0x39                    ; Intro Esc Retroceso Tab Espacio
        db 0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x2B, 0x27, 0x28  ; - = [ ] \ # ; '
        db 0x29, 0x33, 0x34, 0x35, 0x3A                    ; ` , . / BloqMayús
        db 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58 ; F1-F12
        db 0xB7, 0x46, 0x00                                ; ImprPant BloqDespl Pausa
        db 0xD2, 0xC7, 0xC9, 0xD3, 0xCF, 0xD1              ; Insert Inicio RePág Supr Fin AvPág
        db 0xCD, 0xCB, 0xD0, 0xC8                          ; flechas: der izq abajo arriba
        db 0x45, 0xB5, 0x37, 0x4A, 0x4E, 0x9C              ; BloqNum / * - + Intro (numérico)
        db 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x53 ; 1-9 0 . (numérico)
        db 0x56, 0xDD                                      ; < > (teclado español), Aplicación
modtab  db 0x1D, 0x2A, 0x38, 0xDB, 0x9D, 0x36, 0xB8, 0xDC  ; Ctrl Mayús Alt Win (izq, der)

section .init

; =============================================================================
;  Inicialización (se descarta)
; =============================================================================
init:
        ; opciones /F y /V en la línea de CONFIG.SYS
        push ds
        push bx
        lds si, [bx+0x12]
.opt:
        lodsb
        cmp al, 13
        je .optd
        cmp al, 10
        je .optd
        test al, al
        jz .optd
        cmp al, '/'
        jne .opt
        lodsb
        cmp al, '?'
        jne .nh
        mov byte [cs:opt_help], 1
.nh:
        and al, 0xDF
        cmp al, 'F'
        jne .nf
        mov byte [cs:opt_force], 1
.nf:
        cmp al, 'U'
        jne .nu
        mov byte [cs:opt_force], 1
        mov byte [cs:opt_boot], 1
.nu:
        cmp al, 'I'
        jne .ni
        mov byte [cs:opt_info], 1
.ni:
        cmp al, 'V'
        jne .opt
        mov byte [cs:opt_verbose], 1
        mov byte [cs:opt_verbose_r], 1
        jmp .opt
.optd:
        pop bx
        pop ds
.mok:
        push cs
        pop ds
        mov si, msg_title
        call puts
        mov word [fail_msg], msg_usage
        cmp byte [opt_help], 0
        jne .fail
        mov word [fail_msg], msg_v86
        cmp byte [iomode], 2
        je .fail
        ; qué controladoras hay y si las usa la BIOS
        call pci_survey
        mov word [fail_msg], msg_nohc
        cmp byte [hc_n], 0
        je .fail
        call bios_usbdisk
        call info_all
        ; ¿La BIOS usa el USB? Alguna controladora es suya (USB Legacy
        ; Support) o la tiene en marcha: sin /F no se toca ninguna. Y si el
        ; sistema ha arrancado de una memoria USB de la BIOS, solo con /U.
        cmp byte [opt_boot], 0
        jne .take
        cmp byte [boot_usb], 0
        jne .busy
        cmp byte [opt_force], 0
        jne .take
        cmp byte [any_busy], 0
        je .take
.busy:
        ; [USB-AUTO] la BIOS usa el USB: sin /F ni /U se le deja, salvo que
        ; el equipo no tenga ratón PS/2 (entonces solo HSPUSB puede darlo)
        call auto_take
        jc .fail
.take:
        call mem_setup
        mov word [fail_msg], msg_nomem
        jc .fail
        cmp byte [boot_usb], 0
        je .tk
        call boot_save
.tk:
        mov word [fail_msg], msg_notake
        call hc_setup_all
        jc .fail
        ; Intel 7/8/9: los puertos del EHCI, al xHCI (con todo ya tomado)
        call intel_routes
        call usb_enum
        jc .fail
        ; los cambios de conexión de ahora ya están vistos
        xor bx, bx
.cc:
        cmp bl, [hc_n]
        jae .ccd
        test byte [hc_flags+bx], HCF_OURS
        jz .ccn
        mov al, bl
        call hc_sel
        xor cl, cl
.cc1:
        inc cl
        cmp cl, [hc_np+bx]
        ja .ccn
        call rp_ack
        jmp .cc1
.ccn:
        inc bx
        jmp .cc
.ccd:
        mov byte [units], 0
        cmp byte [boot_saved], 0
        je .nb
        cmp byte [boot_match], 0
        jne .bootd
        mov si, msg_bootlost
        call puts
        jmp .hid
.bootd:
        ; la memoria de arranque: desde ahora su INT 13h es nuestro (no se
        ; ofrece además como unidad nueva: sería el mismo disco dos veces)
        mov ax, 0x3513
        int 0x21
        mov [old13], bx
        mov [old13+2], es
        mov dx, int13
        mov ax, 0x2513
        int 0x21
        mov byte [dport], 0
        mov si, msg_bootok
        call puts
        jmp .hid
.nb:
        cmp byte [disk_found], 0
        je .hid
        call disk_setup
        jnc .disk
        mov si, [fail_msg]
        call puts
        jmp .hid
.disk:
        mov byte [units], 1
        lds bx, [cs:req]
        mov al, [bx+0x16]
        add al, 'A'
        mov [cs:msg_drv], al
        push cs
        pop ds
        mov si, msg_disk
        call puts
        mov eax, [cap_mb]
        call putdec
        mov si, msg_mbas
        call puts
.hid:
        ; [USB-AUTO] sin 8042, el teclado solo puede ser USB: si no ha
        ; aparecido, que se sepa qué hacer
        cmp byte [auto_on], 0
        je .hk
        cmp byte [ps2_st], 0
        jne .hk
        cmp byte [k_on], 0
        jne .hk
        mov si, msg_autonok
        call puts
.hk:
        mov al, [k_on]
        or al, [m_on]
        or al, [j_on]
        or al, [j_on+1]
        jnz .hok
        mov al, [units]
        or al, [boot_match]
        jz .fail
        jmp .resp
.hok:
        cmp byte [k_on], 0
        je .h1
        mov si, msg_kbd
        call puts
        ; las teclas se entregan por el controlador de teclado (8042)
        in al, 0x64
        cmp al, 0xFF
        jne .k8042
        mov byte [kbc_ok], 0
        mov si, msg_no8042
        call puts
.k8042:
        call kbd_queue
.h1:
        cmp byte [m_on], 0
        je .h2
        mov si, msg_mouse
        call puts
        call mou_queue
.h2:
        call hid_where
        ; mandos de juego
        xor bx, bx
.hj:
        cmp byte [j_on+bx], 0
        je .hjn
        mov si, msg_joy
        call puts
        mov di, bx
        imul di, HJS_SZ
        add di, j_est
        mov ax, [di+HJS_VID]
        call puthex4
        mov dl, ':'
        call putc
        mov ax, [di+HJS_PID]
        call puthex4
        mov si, msg_joy2
        call puts
        movzx eax, byte [di+HJS_NAX]
        call putdec
        mov si, msg_joy3
        call puts
        movzx eax, byte [di+HJS_NBTN]
        call putdec
        mov si, msg_joy4
        cmp byte [di+HJS_TIPO], 2
        jne .hjt
        mov si, msg_joy5
.hjt:
        call puts
        call joy_queue
.hjn:
        inc bx
        cmp bx, JOY_N
        jb .hj
        ; reloj: recoger los informes
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov dx, int08
        mov ax, 0x2508
        int 0x21
.resp:
        ; respuesta al núcleo: unidades, BPB y memoria residente
        ; memoria residente: hasta donde se haya reservado (init_fin hace
        ; el reparto final)
        call init_fin
        mov eax, [cs:ztop]
        mov dx, ax
        and dx, 15
        shr eax, 4
        mov cx, cs
        add cx, ax
        lds bx, [cs:req]
        mov al, [cs:units]
        mov [bx+0x0D], al
        mov [bx+0x0E], dx
        mov [bx+0x10], cx
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        jmp .done
.fail:
        push cs
        pop ds
        mov si, [fail_msg]
        call puts
        ; el driver se descarga: las controladoras no pueden seguir en marcha
        ; con sus colas y contextos en una memoria que ya no es nuestra
        call hc_stop_all
        lds bx, [cs:req]
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
.done:
        call info_pause
        lds bx, [cs:req]
        mov word [bx+3], 0x0100
        call a20_restore
        mov byte [cs:busy], 0
        pop fs
        pop es
        pop ds
        popad
        retf

; [USB-AUTO] La BIOS usa el USB y no se ha dicho /F ni /U. ¿Se toma igual?
; Solo si el equipo no tiene ratón PS/2, porque entonces nadie más que
; HSPUSB puede dar un ratón (ni la BIOS, que en estos equipos solo emula el
; teclado):
;  - sin controlador de teclado 8042 (el puerto 64h lee FFh: los Mac con
;    Intel arrancados en modo BIOS, los PC «sin legado»). Ahí la BIOS
;    tampoco da el teclado a los programas que leen el 8042, y HSPUSB lo
;    lleva igual que ella (al búfer de la BIOS). Se toma todo, también la
;    memoria USB de arranque si se ha arrancado de una (como /U): no se
;    puede quitar a la BIOS solo una parte del USB.
;  - con 8042 pero sin nada en su puerto auxiliar (ni la BIOS dice que hay
;    ratón en INT 11h, ni responde al reinicio de INT 15h C201h): se toma
;    como con /F, pero no si se ha arrancado de una memoria USB (ahí el
;    teclado ya va bien por la BIOS y perder el disco sería peor: /U).
; Donde hay ratón PS/2 (o la BIOS emula uno con el ratón USB) no se toca
; nada, como siempre. -> CF si no se toma (fail_msg)
auto_take:
        call ps2_probe
        mov [ps2_st], al
        cmp al, 2
        je .no
        test al, al
        jz .yes
        cmp byte [boot_usb], 0
        jne .no
.yes:
        mov byte [opt_force], 1
        mov byte [auto_on], 1
        mov al, [boot_usb]
        or [opt_boot], al
        mov si, msg_auto0
        cmp byte [ps2_st], 0
        je .m
        mov si, msg_auto1
.m:
        call puts
        cmp byte [boot_usb], 0
        je .ok
        mov si, msg_autou
        call puts
.ok:
        clc
        ret
.no:
        mov word [fail_msg], msg_bootusb
        cmp byte [boot_usb], 0
        jne .f
        mov word [fail_msg], msg_bios
.f:
        stc
        ret

; ¿Hay ratón PS/2? -> AL = 0 sin 8042, 1 con 8042 y sin ratón, 2 con ratón
ps2_probe:
%ifdef SIN_PS2
        mov al, SIN_PS2             ; (pruebas: como si fuera así)
        ret
%endif
        in al, 0x64
        cmp al, 0xFF
        jne .k
        xor al, al
        ret
.k:
        int 0x11
        test al, 4                  ; la BIOS ha visto un ratón
        jnz .si
        mov ax, 0xC201              ; reiniciarlo: sin ratón, error
        int 0x15
        jc .nada
        mov ax, 0xC200              ; y dejarlo parado (HSPRATON lo prepara)
        xor bh, bh
        int 0x15
.si:
        call unreal
        mov al, 2
        ret
.nada:
        call unreal
        mov al, 1
        ret
ps2_st      db 2
auto_on     db 0
msg_auto0   db "La BIOS usa el USB, pero este equipo no tiene controlador de teclado", 13, 10
            db "(8042) ni ratón PS/2: HSPUSB lleva el USB (teclado, ratón y panel).", 13, 10, 0
msg_auto1   db "La BIOS usa el USB y no hay ratón PS/2: HSPUSB lleva el USB (ratón USB).", 13, 10, 0
msg_autou   db "También atiende la memoria USB de arranque (como con /U).", 13, 10, 0

; [USB-BT] los receptores de Bluetooth que se han dejado para después: si
; falta el teclado o el ratón, se vuelve a reiniciar su puerto y se
; enumeran (ahora ya valen)
proxy_retry:
        mov byte [px_ok], 1
        xor si, si
.l:
        movzx ax, byte [px_n]
        cmp si, ax
        jae .r
        mov al, [k_on]
        and al, [m_on]
        jnz .r
        mov bx, si
        shl bx, 1
        mov cl, [px_tab+bx+1]
        mov bl, [px_tab+bx]
        xor bh, bh
        push si
        call hub_port
        pop si
        inc si
        jmp .l
.r:
        ret

; qué controladora lleva el teclado y el ratón (para saber en un equipo
; de verdad por dónde va cada uno)
hid_where:
        mov al, [k_on]
        or al, [m_on]
        jz .r
        mov si, msg_where
        call puts
        xor bx, bx
        cmp byte [k_on], 0
        je .m
        mov si, msg_wk
        call puts
        call .hc
        cmp byte [m_on], 0
        je .e
        mov dl, ','
        call putc
        mov dl, ' '
        call putc
.m:
        cmp byte [m_on], 0
        je .e
        mov bx, 1
        mov si, msg_wm
        call puts
        call .hc
.e:
        mov dl, '.'
        call putc
        call crlf
.r:
        ret
; la controladora de la tubería BX: clase y fabricante:modelo
.hc:
        mov al, [ip_pool+bx]
        call dev_hc
        movzx si, byte [cur_type]
        shl si, 1
        mov si, [hc_tags+si]
        call puts
        call cur_bdf
        xor di, di
        call pci_rw
        mov ax, cx
        call puthex4
        mov dl, ':'
        call putc
        call cur_bdf
        mov di, 2
        call pci_rw
        mov ax, cx
        call puthex4
        ret
msg_where   db "USB: ", 0
msg_wk      db "teclado por ", 0
msg_wm      db "ratón por ", 0
msg_autonok db "AVISO: no ha aparecido el teclado USB. Si no responde, reinicie y pulse F5", 13, 10
            db "al ver el menú de arranque (arranque sin CONFIG.SYS).", 13, 10, 0

; /INFO: dejar el informe en pantalla unos segundos (para la foto)
info_pause:
        cmp byte [cs:opt_info], 0
        jne .go
        ret
.go:
        push cs
        pop ds
        mov si, msg_pause
        call puts
        sti
        mov cx, 273                 ; 15 s
        call tmo_start
.l:
        mov ah, 1
        int 0x16
        jnz .key
        call tmo_check
        jnc .l
        jmp .r
.key:
        xor ah, ah
        int 0x16
.r:
        call unreal
        ret

; ¿Ha arrancado el sistema desde un disco USB que maneja la BIOS? Si HSPUSB
; le quita el controlador a la BIOS, ese disco deja de existir y el sistema
; se queda sin su unidad de arranque. Se mira la unidad 80h con las
; extensiones de INT 13h (EDD 3.0: tipo de interfaz "USB"); si la BIOS no lo
; dice, se supone que sí cuando la BIOS tiene en marcha algún controlador USB.
bios_usbdisk:
        mov byte [boot_usb], 0
        mov ax, 0x3305
        xor dl, dl
        int 0x21
        cmp dl, 3
        jb .r                       ; arranque desde disquete
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, 0x80
        int 0x13
        jc .unk
        cmp bx, 0xAA55
        jne .unk
        push es
        push cs
        pop es
        mov di, edd_buf
        mov cx, 0x4A / 2 ; (hueco de sobra)
        xor ax, ax
        cld
        rep stosw
        pop es
        mov word [edd_buf], 0x42
        mov ah, 0x48
        mov dl, 0x80
        mov si, edd_buf
        int 0x13
        jc .unk
        cmp word [edd_buf], 0x42
        jb .unk
        cmp word [edd_buf+0x1E], 0xBEDD
        jne .unk
        mov eax, [edd_buf+0x28]
        cmp eax, 'USB '
        je .usb
        and eax, 0x00FFFFFF
        cmp eax, 'USB'
        je .usb
        cmp al, 'A'                 ; "ATA", "SATA", "SCSI"...: no es USB
        jae .r
.unk:
        cmp byte [any_busy], 0
        je .r
.usb:
        mov byte [boot_usb], 1
.r:
        call unreal
        ret

; /U: guardar el sector 0 y la geometría del disco de arranque de la BIOS
; (para reconocerlo después entre las memorias USB y seguir sirviéndolo)
boot_save:
        mov cx, 512
        mov dx, 16
        call zalloc
        jc .r
        mov [boot_sec], bx
        push es
        push cs
        pop es
        mov ax, 0x0201
        mov cx, 1
        mov dx, 0x0080
        int 0x13
        pop es
        jc .r
        push es
        mov ah, 8
        mov dl, 0x80
        int 0x13
        pop es
        jc .r
        mov [i13_cx], cx
        mov [i13_dx], dx
        mov byte [boot_saved], 1
.r:
        call unreal
        ret

; /INFO: el controlador y lo que hace la BIOS con él
info_ctrl:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        mov si, msg_ic1
        call puts
        mov bx, [bdf]
        mov ax, 0xB109
        xor di, di
        int 0x1A
        mov ax, cx
        call puthex4
        mov dl, ':'
        call putc
        mov bx, [bdf]
        mov ax, 0xB109
        mov di, 2
        int 0x1A
        mov ax, cx
        call puthex4
        call unreal
        mov si, msg_ic2
        call puts
        mov esi, [xbase]
        call mrd
        shr eax, 16
        call puthex4
        mov si, msg_ic3
        call puts
        movzx eax, word [ctx_sz]
        call putdec
        mov si, msg_ic4
        call puts
        mov al, [hc_owned]
        call .yn
        mov si, msg_ic5
        call puts
        mov al, [hc_running]
        call .yn
        mov si, msg_ic6
        call puts
        mov al, [boot_usb]
        call .yn
        call crlf
        ret
.yn:
        mov si, msg_si
        test al, al
        jnz .y
        mov si, msg_no
.y:
        call puts
        ret
msg_ic1     db "xHCI ", 0
msg_ic2     db " v", 0
msg_ic3     db " ctx ", 0
msg_ic4     db ". BIOS: suyo ", 0
msg_ic5     db ", en marcha ", 0
msg_ic6     db ", arranque USB ", 0
msg_si      db "sí", 0
msg_no      db "no", 0
hc_running  db 0
hc_owned    db 0
boot_usb    db 0
opt_boot    db 0
            align 2
edd_buf     times 0x4A db 0

; ---- encontrar e inicializar el controlador xHCI ----
; xHCI en [bdf]: dirección, registros de capacidad y si lo usa la BIOS
; (hc_running, hc_owned). CF si no vale.
xhci_probe:
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        mov [bar_lo], cl
        and ecx, 0xFFFFFFF0
        mov [xbase], ecx
        jz .bad
        ; la mitad alta solo existe si el BAR es de 64 bits (tipo 10b); en
        ; uno de 32 bits lo que hay en 14h es otro BAR distinto
        mov al, [bar_lo]
        and al, 6
        cmp al, 4
        jne .lo
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0x14
        int 0x1A
        test ecx, ecx
        jz .lo
        mov word [fail_msg], msg_high
        stc
        ret
.lo:
        mov bx, [bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0006
        mov bx, [bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        call unreal
        mov si, msg_found
        call puts
        mov eax, [xbase]
        call puthex8
        call crlf
        ; registros de capacidad
        mov esi, [xbase]
        call mrd
        movzx ebx, al               ; CAPLENGTH
        mov eax, [xbase]
        add eax, ebx
        mov [op_base], eax
        mov esi, [xbase]
        add esi, 0x04
        call mrd
        shr eax, 24
        mov [maxports], al
        mov esi, [xbase]
        add esi, 0x10
        call mrd
        mov [hcc1], eax
        test al, 4
        jz .c32
        mov word [ctx_sz], 64
.c32:
        mov esi, [xbase]
        add esi, 0x14
        call mrd
        and eax, 0xFFFFFFFC
        add eax, [xbase]
        mov [db_base], eax
        mov esi, [xbase]
        add esi, 0x18
        call mrd
        and eax, 0xFFFFFFE0
        add eax, [xbase]
        mov [rt_base], eax
        ; ¿la BIOS usa el controlador? Es suyo (USB Legacy Support) o lo
        ; tiene en marcha: sin /F no se toca. Y si el sistema ha arrancado de
        ; una memoria USB de la BIOS, solo con /U.
        mov esi, [op_base]
        call mrd
        and al, 1
        mov [hc_running], al
        call legacy_find
        jc .nown
        call mrd
        test eax, 0x10000
        jz .nown
        mov byte [hc_owned], 1
.nown:
        clc
        ret
.bad:
        stc
        ret

; tomar el xHCI: traspaso desde la BIOS, reinicio, anillos y en marcha
xhci_take:
        mov al, [x_hc]
        call hc_sel
        call legacy_handoff
        ; parar y reiniciar
        mov esi, [op_base]
        call mrd
        and al, 0xFE
        call mwr
        mov cx, 10
        call tmo_start
.h:
        mov esi, [op_base]
        add esi, 4
        call mrd
        test al, 1                  ; HCHalted
        jnz .halted
        call tmo_check
        jnc .h
.halted:
        ; dejar que la BIOS gaste los eventos que le queden (hay BIOS, como
        ; SeaBIOS, que sondean su teclado USB con el reloj y escribirían en
        ; nuestros registros después del reinicio)
        sti
        mov cx, 3
        call tmo_start
.bw:
        call tmo_check
        jnc .bw
        mov esi, [op_base]
        mov eax, 2                  ; HCRST
        call mwr
        mov cx, 20
        call tmo_start
.rs:
        call mrd
        test al, 2
        jz .rsd
        call tmo_check
        jnc .rs
        mov word [fail_msg], msg_reset
        stc
        ret
.rsd:
        mov byte [hc_ours], 1
        mov esi, [op_base]
        add esi, 4
.cnr:
        call mrd
        test eax, 0x800             ; Controller Not Ready
        jz .ready
        call tmo_check
        jnc .cnr
.ready:
        ; slots, DCBAA, anillo de órdenes y anillo de eventos
        mov esi, [op_base]
        add esi, 0x38
        mov eax, MAX_SLOTS
        call mwr
        mov bx, DCBAA
        call zone
        call phys
        mov esi, [op_base]
        add esi, 0x30
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov word [fail_msg], msg_scratch
        call scratch_setup
        jc .r
        mov bx, CMD_RING
        call zone
        call phys
        or al, 1
        mov esi, [op_base]
        add esi, 0x18
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov bx, EVT_RING
        call zone
        call phys
        mov bx, ERST
        call zone
        mov [cs:bx], eax
        mov dword [cs:bx+4], 0
        mov dword [cs:bx+8], RING_TRBS
        mov esi, [rt_base]
        add esi, 0x20 + 0x08        ; ERSTSZ
        mov eax, 1
        call mwr
        mov bx, EVT_RING
        call zone
        call phys
        mov esi, [rt_base]
        add esi, 0x20 + 0x18        ; ERDP
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov bx, ERST
        call zone
        call phys
        mov esi, [rt_base]
        add esi, 0x20 + 0x10        ; ERSTBA
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        ; en marcha (sin interrupciones: se consulta el anillo de eventos)
        mov esi, [op_base]
        mov eax, 1
        call mwr
        mov cx, 10
        call tmo_start
        add esi, 4
.run:
        call mrd
        test al, 1
        jz .running
        call tmo_check
        jnc .run
.running:
        clc
.r:
        ret
.no:
        stc
        ret

; parar el controlador (si lo hemos reiniciado nosotros)
xhci_stop:
        cmp byte [hc_ours], 0
        je .r
        mov esi, [op_base]
        call mrd
        and al, 0xFE
        call mwr
        mov cx, 10
        call tmo_start
.h:
        mov esi, [op_base]
        add esi, 4
        call mrd
        test al, 1
        jnz .r
        call tmo_check
        jnc .h
.r:
        ret
bar_lo      db 0

; Páginas de trabajo del controlador («scratchpad»). Los xHCI de verdad (los
; de Intel entre ellos) piden en HCSPARAMS2 un número de páginas de 4 KB para
; su uso; hay que dárselas en DCBAA[0] o no arrancan, o escriben donde apunte
; la dirección 0. El de QEMU no pide ninguna. Se toman de la memoria
; extendida (XMS), que el controlador alcanza por dirección física.
scratch_setup:
        mov esi, [xbase]
        add esi, 0x08
        call mrd
        mov ebx, eax
        shr ebx, 27                 ; bits 31-27: parte baja
        mov ecx, eax
        shr ecx, 21
        and ecx, 0x1F               ; bits 25-21: parte alta
        shl ecx, 5
        or ebx, ecx
%ifdef FORCE_SCRATCH
        mov ebx, FORCE_SCRATCH      ; (para probar este camino en QEMU)
%endif
        test ebx, ebx
        jz .ok
        mov [n_scratch], bx
        mov word [fail_msg], msg_scratch
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .err
        push es
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        pop es
        ; 2 páginas para la tabla + n páginas + 4 KB para alinear
        mov dx, [n_scratch]
        add dx, 3
        shl dx, 2
        mov ah, 0x09
        call far [xms]
        test ax, ax
        jz .err
        mov ah, 0x0C                ; bloquear: DX:BX = dirección lineal
        call far [xms]
        test ax, ax
        jz .err
        shl edx, 16
        mov dx, bx
        add edx, 0xFFF
        and edx, 0xFFFFF000
        mov [scr_base], edx
        call unreal                 ; (A20 ya está abierta: a20_open)
        cmp byte [iomode], 0
        jne .v86
        mov edi, [scr_base]
        movzx ecx, word [n_scratch]
        add ecx, 2
        shl ecx, 10
        xor eax, eax
.z:
        mov [fs:edi], eax
        add edi, 4
        dec ecx
        jnz .z
        mov edi, [scr_base]
        mov eax, edi
        add eax, 0x2000
        movzx ecx, word [n_scratch]
.t:
        mov [fs:edi], eax
        mov dword [fs:edi+4], 0
        add edi, 8
        add eax, 0x1000
        dec ecx
        jnz .t
        jmp .tab
.v86:
        ; modo V86: lo mismo con INT 15h AH=87h. Las páginas se ponen a
        ; cero copiando la zona DATA (4 KB, todavía a cero) y la tabla se
        ; escribe palabra a palabra. La memoria de XMS es física = lineal.
        mov bx, [cs:data_rel]
        call zone
        xor esi, esi
        mov si, cs
        shl esi, 4
        movzx ebx, bx
        add esi, ebx
        mov edi, [scr_base]
        mov cx, [n_scratch]
        add cx, 2
.vz:
        push cx
        mov cx, 0x800
        call pcopy
        pop cx
        add edi, 0x1000
        loop .vz
        mov esi, [scr_base]
        mov edx, esi
        add edx, 0x2000
        mov cx, [n_scratch]
.vt:
        mov eax, edx
        call mwr
        add esi, 8
        add edx, 0x1000
        loop .vt
.tab:
        mov bx, DCBAA
        call zone
        mov eax, [scr_base]
        mov [cs:bx], eax
        mov dword [cs:bx+4], 0
.ok:
        clc
        ret
.err:
        stc
        ret
n_scratch   dw 0
xms         dd 0
scr_base    dd 0

; [USB-INTEL] Intel series 7, 8 y 9 (Panther Point, Lynx Point, Wildcat
; Point; también Wellsburg y Bay Trail): de fábrica los puertos USB salen por
; los EHCI. Los que también llegan al xHCI (máscaras XUSB2PRM, D4h, y
; USB3PRM, DCh, del espacio PCI del xHCI; hojas de datos de Intel) se pasan
; al xHCI con XUSB2PR (D0h), y USB3_PSSEN (D8h) enciende sus terminaciones
; de USB 3, como hacen los sistemas actuales (primero USB3_PSSEN). Así lo de
; esos puertos va por el xHCI, sin transacciones partidas. Lo demás (en esos
; PCH el xHCI solo llega a 4 puertos de USB 2; en un MacBook, por ejemplo,
; el teclado y el panel, la cámara y el Bluetooth van por el concentrador
; interno de uno de los EHCI) se queda en el EHCI.
; Se hace con todas las controladoras ya tomadas (la BIOS ya no usa los
; EHCI: no se le quita nada que siga usando) y antes de enumerar. Después se
; espera a que lo que haya en esos puertos aparezca en el xHCI. Si HSPUSB
; no se queda, se deja como estaba (hc_stop_all).
intel_routes:
        cmp byte [x_have], 0        ; el xHCI es nuestro (y está en marcha)
        je .r
        movzx bx, byte [x_hc]
        test byte [hc_flags+bx], HCF_OURS
        jz .r
        cmp byte [n_hc+HCT_EHCI], 0 ; y hay algún EHCI de donde traerlos
        je .r
        mov bx, [bdf]
        xor di, di
        call ir_rd
        cmp cx, 0x8086
        jne .r
        shr ecx, 16
        mov si, intel_xids
.id:
        lodsw
        test ax, ax
        jz .r
        cmp ax, cx
        jne .id
        mov di, 0xD4                ; XUSB2PRM: los que se pueden pasar
        call ir_rd
        and ecx, 0x7FFF
        mov [ir_u2m], ecx
        mov di, 0xDC                ; USB3PRM
        call ir_rd
        and ecx, 0x3F
        mov [ir_u3m], ecx
        mov eax, [ir_u2m]
        or eax, ecx
        jz .r
        mov di, 0xD0                ; cómo están (para dejarlos así si no
        call ir_rd                  ; nos quedamos)
        mov [ir_u2o], ecx
        mov di, 0xD8
        call ir_rd
        mov [ir_u3o], ecx
        mov ecx, [ir_u3m]
        mov di, 0xD8                ; USB3_PSSEN
        call ir_wr
        mov ecx, [ir_u2m]
        mov di, 0xD0                ; XUSB2PR
        call ir_wr
        mov byte [ir_done], 1
        mov di, 0xD0
        call ir_rd
        mov [ir_u2n], ecx
        mov di, 0xD8
        call ir_rd
        mov [ir_u3n], ecx
        cmp byte [opt_info], 0
        je .w
        mov si, msg_ir1
        call puts
        mov eax, [ir_u2n]
        call puthex8
        mov si, msg_ir2
        call puts
        mov eax, [ir_u2m]
        call puthex8
        mov si, msg_ir3
        call puts
        mov eax, [ir_u3n]
        call puthex8
        mov si, msg_ir2
        call puts
        mov eax, [ir_u3m]
        call puthex8
        mov si, msg_ir4
        call puts
.w:
        ; ¿ha cambiado algo? Lo que estuviera conectado en esos puertos se
        ; desconecta del EHCI y aparece en el xHCI: hasta unos 0,7 s para
        ; que aparezca algo y, cuando aparece, unos 0,15 s para lo demás
        mov eax, [ir_u2n]
        not dword [ir_u2o]
        and eax, [ir_u2o]
        mov edx, [ir_u3n]
        not dword [ir_u3o]
        and edx, [ir_u3o]
        not dword [ir_u2o]
        not dword [ir_u3o]
        or eax, edx
        jz .r
        mov al, [x_hc]
        call hc_sel
        mov cx, 13
        call tmo_start
.wl:
        xor cl, cl
.wp:
        inc cl
        cmp cl, [maxports]
        ja .wn
        call rp_get
        test al, 1
        jz .wp
        mov cx, 3
        call tick_wait
        jmp .r
.wn:
        call tmo_check
        jnc .wl
.r:
        ret
; dejar los puertos como estaban (HSPUSB no se queda)
intel_unroute:
        cmp byte [ir_done], 0
        je .r
        mov bx, [bdf]
        mov ecx, [ir_u2o]
        mov di, 0xD0
        call ir_wr
        mov ecx, [ir_u3o]
        mov di, 0xD8
        call ir_wr
        mov byte [ir_done], 0
.r:
        ret
; registro DI del espacio PCI del xHCI (BX) -> ECX; escribirlo
ir_rd:
%ifdef INTEL_FAKE
        ; (pruebas en QEMU, que no tiene estos registros: como un xHCI
        ; 8086:1E31 con 4 puertos de USB 2 y 4 de USB 3 que se pueden pasar)
        push di
        call ir_fi
        mov ecx, [ir_fake+di]
        pop di
        ret
%endif
        call pci_rd
        ret
ir_wr:
%ifdef INTEL_FAKE
        push di
        call ir_fi
        mov [ir_fake+di], ecx
        pop di
        ret
%endif
        call pci_wd
        ret
%ifdef INTEL_FAKE
; registro DI (00h, D0h, D4h, D8h, DCh) -> DI = su sitio en ir_fake
ir_fi:
        test di, di
        jz .r
        sub di, 0xD0 - 4
.r:
        ret
ir_fake     dd 0x1E318086, 0, 0x0F, 0, 0x0F
%endif
intel_xids  dw 0x1E31, 0x8C31, 0x9C31, 0x8CB1, 0x9CB1, 0x8D31, 0x0F35, 0
ir_done     db 0
ir_u2m      dd 0
ir_u3m      dd 0
ir_u2o      dd 0
ir_u3o      dd 0
ir_u2n      dd 0
ir_u3n      dd 0
msg_ir1     db "Intel: puertos pasados del EHCI al xHCI: USB 2 XUSB2PR=", 0
msg_ir2     db " (de ", 0
msg_ir3     db "),", 13, 10, "  USB 3 USB3_PSSEN=", 0
msg_ir4     db ")", 13, 10, 0

; registro USBLEGSUP del controlador -> ESI; CF si no lo tiene
legacy_find:
        mov eax, [hcc1]
        shr eax, 16
        jz .no
        shl eax, 2
        add eax, [xbase]
        mov esi, eax
.cap:
        call mrd
        cmp al, 1
        je .leg
        shr eax, 8
        and eax, 0xFF
        jz .no
        shl eax, 2
        add esi, eax
        jmp .cap
.leg:
        clc
        ret
.no:
        stc
        ret

; ceder la propiedad del controlador desde la BIOS (si la tiene)
legacy_handoff:
        call legacy_find
        jc .ok
        call mrd
        or eax, 0x1000000           ; propiedad del sistema (siempre: hay
        call mwr                    ; BIOS que usan sus SMI sin decirlo)
        test eax, 0x10000           ; propiedad de la BIOS: que la suelte
        jz .done
        mov cx, 20
        call tmo_start
.w:
        call mrd
        test eax, 0x10000
        jz .done
        call tmo_check
        jnc .w
.done:
        ; desactivar las SMI de la BIOS
        add esi, 4
        mov eax, 0xE0000000
        call mwr
.ok:
        ret

; enumerar el dispositivo que acaba de habilitarse: [port] (puerto del
; controlador), [speed], [route], [depth], [tt_slot], [tt_port]. Si es un
; concentrador se enumera también lo que cuelga de él.
section .text
enum_one:
        mov byte [d_have], 0
        mov byte [d_role], 6
        ; una entrada libre de la reserva
        call pool_find
        jnc .pg
        mov word [fail_msg], msg_full
        call info_line
        ret
.pg:
        mov al, [cur_hc]
        mov [dv_hc+bx], al
        mov al, bl
        call pool_sel
        cmp byte [cur_type], HCT_XHCI
        jne .q0
        mov si, [cur_st]
        mov byte [si], 0
        mov byte [si+1], 1
        mov bx, [cur_ep0]
        mov cx, 0x100
        call zero_zone
        mov bx, [cur_out]
        mov cx, [ctx_sz]
        shl cx, 5
        call zero_zone
.q0:
        cmp byte [opt_verbose], 0
        je .q1
        mov si, msg_port
        call puts
        movzx eax, byte [port]
        call putdec
        mov si, msg_speed
        call puts
        movzx eax, byte [speed]
        call putdec
        call crlf
.q1:
        cmp byte [cur_type], HCT_XHCI
        je .xh
        ; controladora clásica: darle dirección (SET_ADDRESS)
        call c_dev_open
        mov word [fail_msg], msg_addr
        jnc .adok
        call info_line
        jmp .err
.xh:
        ; Enable Slot
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        mov ebp, 9 << 10
        call xhci_cmd
        mov word [fail_msg], msg_slot
        jnc .slok
        call show_cc
        call info_line
        jmp .err
.slok:
        mov [cur_slot], ah
        movzx bx, byte [cur_pool]
        mov [dv_slot+bx], ah
        ; DCBAA[slot] = contexto de salida
        mov bx, [cur_out]
        call zone
        call phys
        movzx di, byte [cur_slot]
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        ; contexto de entrada: slot y EP0
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 3                  ; A0 | A1
        add bx, [ctx_sz]
        mov eax, 1                              ; una entrada de contexto
        call slot_fill
        add bx, [ctx_sz]
        ; tamaño máximo de paquete de EP0 según la velocidad
        mov eax, 8
        cmp byte [speed], 3
        jb .m
        mov eax, 64
        cmp byte [speed], 4
        jb .m
        mov eax, 512
.m:
        mov [mps0], ax
        shl eax, 16
        or eax, (3 << 1) | (4 << 3)             ; CErr=3, tipo Control
        mov [cs:bx+4], eax
        push bx
        mov bx, [cur_ep0]
        call zone
        call phys
        pop bx
        or al, 1
        mov [cs:bx+8], eax
        mov dword [cs:bx+12], 0
        mov dword [cs:bx+16], 8
        ; Address Device
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 11 << 10
        call xhci_cmd
        mov word [fail_msg], msg_addr
        jnc .adok
        call show_cc
        jmp .dis
.adok:
        ; descriptor de dispositivo: primero 8 bytes (tamaño de paquete de EP0)
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00080000
        call control
        mov word [fail_msg], msg_desc
        jc .dis
        cmp byte [speed], 4
        jae .fulld
        mov bx, DESC
        call zone
        movzx ax, byte [cs:bx+7]
        cmp ax, 8
        jb .fulld
        cmp ax, [mps0]
        je .fulld
        mov [mps0], ax
        cmp byte [cur_type], HCT_XHCI
        je .ev
        movzx bx, byte [cur_pool]
        mov [dv_mps0+bx], al
        jmp .fulld
.ev:
        ; Evaluate Context con el tamaño de paquete real
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 2                  ; A1
        add bx, [ctx_sz]
        add bx, [ctx_sz]
        movzx eax, word [mps0]
        shl eax, 16
        or eax, (3 << 1) | (4 << 3)
        mov [cs:bx+4], eax
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 13 << 10
        call xhci_cmd
        jc .dis
.fulld:
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00120000
        call control
        jc .dis
        mov bx, DESC
        call zone
        mov ax, [cs:bx+8]
        mov [d_vid], ax
        mov ax, [cs:bx+10]
        mov [d_pid], ax
        mov byte [n_if], 0
        mov byte [d_have], 1
        ; descriptor de configuración: primero 9 bytes, después entero
        mov dword [ctl_setup], 0x02000680
        mov dword [ctl_setup+4], 0x00090000
        call control
        jc .dis
        mov bx, DESC
        call zone
        mov ax, [cs:bx+2]
        cmp ax, 255
        jbe .tl
        mov ax, 255
.tl:
        mov [ctl_setup+6], ax
        call control
        jc .dis
        call parse_config
        mov byte [d_role], 0
        ; ¿qué papel tiene?
        cmp byte [p_hub], 0
        je .nohub
        call role_hub
        jc .bad
        mov byte [d_role], 5
        movzx bx, byte [cur_pool]
        mov byte [pool_used+bx], 1
        call info_line
        mov bl, [new_hub]
        xor bh, bh
        call hub_scan
        ret
.nohub:
        cmp byte [p_msc], 0
        je .hid
        cmp byte [disk_found], 0
        jne .hid
        call role_disk
        jc .bad
        mov byte [d_role], 1
        jmp .used
.hid:
        cmp byte [only_disk], 0
        jne .dis
        ; [USB-BT] un receptor de Bluetooth que se hace pasar por teclado o
        ; ratón (HID proxy: el de los Mac va por el mismo concentrador
        ; interno que el teclado y el panel): se deja para el final, por si
        ; aparece el de verdad (proxy_retry)
        call is_proxy
        jnc .np0
        cmp byte [px_ok], 0
        jne .np0
        cmp byte [par_hub], 0xFF
        je .np0
        movzx bx, byte [px_n]
        cmp bx, PX_N
        jae .np0
        inc byte [px_n]
        shl bx, 1
        mov al, [par_hub]
        mov [px_tab+bx], al
        mov al, [par_port]
        mov [px_tab+bx+1], al
        mov byte [d_role], 8
        jmp .dis
.np0:
        mov byte [want_k], 0
        mov byte [want_m], 0
        cmp byte [p_kbd], 0
        je .hm
        cmp byte [k_on], 0
        jne .hm
        mov byte [want_k], 1
.hm:
        cmp byte [p_mou], 0
        je .hc
        cmp byte [m_on], 0
        jne .hc
        mov byte [want_m], 1
.hc:
        mov al, [want_k]
        or al, [want_m]
        jnz .hkm
        ; ni teclado ni ratón: ¿un mando de juego?
        cmp byte [p_joy], 0
        je .dis
        call role_joy
        jnc .joy
        cmp byte [j_notpad], 0
        jne .dis
        jmp .bad
.joy:
        mov byte [d_role], 7
        jmp .used
.hkm:
        call role_hid
        jc .bad
        mov al, [want_k]
        add al, al
        add al, [want_m]            ; 2 teclado, 1 ratón, 3 los dos
        mov ah, 3
        cmp al, 1
        je .hr
        mov ah, 2
        cmp al, 2
        je .hr
        mov ah, 4
.hr:
        mov [d_role], ah
.used:
        movzx bx, byte [cur_pool]
        mov byte [pool_used+bx], 1
        call info_line
        ret
.bad:
        mov byte [d_role], 6
.dis:
        ; no se usa: liberar el slot
        call info_line
        mov al, [cur_pool]
        call dev_free
.err:
        ret

; ¿es [d_vid]:[d_pid] un receptor de Bluetooth en modo HID proxy? CF si sí
is_proxy:
        push si
        mov si, px_ids
.l:
        mov ax, [si]
        test ax, ax
        jz .no
        add si, 4
        cmp ax, [d_vid]
        jne .l
        mov ax, [si-2]
        cmp ax, [d_pid]
        jne .l
        pop si
        stc
        ret
.no:
        pop si
        clc
        ret
PX_N        equ 2
px_ids      dw 0x05AC, 0x820A, 0x05AC, 0x820B   ; Apple: teclado y ratón por Bluetooth
            dw 0x0A5C, 0x4502, 0x0A5C, 0x4503   ; Broadcom (BCM2045/2046)
%ifdef PROXY_TEST
            dw 0x0627, 0x0001                   ; (pruebas: los de QEMU)
%endif
            dw 0
px_ok       db 0
px_n        db 0
px_tab      times PX_N * 2 db 0                 ; concentrador y puerto

; una entrada libre de la reserva para un aparato de la controladora actual
; -> BX; CF si no hay. Los del xHCI necesitan memoria para su contexto de
; salida (400h u 800h): primero una entrada que ya la tenga y, si no, se reserva
; (solo durante el inicio). Los de las clásicas no gastan memoria: se toman
; por el final.
pool_find:
        cmp byte [cur_type], HCT_XHCI
        je .x
        mov bx, POOL_N
.c:
        test bx, bx
        jz .full
        dec bx
        cmp byte [pool_used+bx], 0
        jne .c
        clc
        ret
.x:
        xor bx, bx
.x1:
        cmp byte [pool_used+bx], 0
        jne .x2
        mov si, bx
        shl si, 1
        cmp word [pool_mem+si], 0xFFFF
        jne .ok
.x2:
        inc bx
        cmp bx, POOL_N
        jb .x1
        xor bx, bx
.x3:
        cmp byte [pool_used+bx], 0
        je .x4
        inc bx
        cmp bx, POOL_N
        jb .x3
.full:
        stc
        ret
.x4:
        push bx
        mov cx, [ctx_sz]            ; 32 contextos de 32 o de 64 bytes
        shl cx, 5
        mov dx, cx
        call zalloc
        mov ax, bx
        pop bx
        jc .full
        sub ax, [mem_off]
        mov si, bx
        shl si, 1
        mov [pool_mem+si], ax
.ok:
        clc
        ret

; contexto del slot (CS:BX) del dispositivo que se enumera: EAX = número de
; entradas de contexto. Cadena de ruta, velocidad, puerto del controlador y
; traductor de transacciones; con [hubf], los datos de concentrador.
slot_fill:
        push edx
        shl eax, 27
        movzx edx, byte [speed]
        shl edx, 20
        or eax, edx
        or eax, [route]
        cmp byte [hubf], 0
        je .a
        or eax, 1 << 26                         ; Hub
.a:
        mov [cs:bx], eax
        movzx eax, byte [port]
        shl eax, 16
        movzx edx, byte [hub_np]
        shl edx, 24                             ; número de puertos
        or eax, edx
        mov [cs:bx+4], eax
        movzx eax, byte [tt_slot]
        movzx edx, byte [tt_port]
        shl edx, 8
        or eax, edx
        movzx edx, byte [hub_ttt]
        shl edx, 16                             ; TT Think Time
        or eax, edx
        mov [cs:bx+8], eax
        pop edx
        ret

; concentrador: configurarlo, decírselo al controlador (Hub = 1, puertos,
; TT Think Time) y dar corriente a sus puertos -> [new_hub]; CF si falla.
; Se deja en el modo de un solo traductor (interfaz alternativa 0), así que
; MTT = 0, que es lo que pide la especificación en ese caso.
role_hub:
        mov word [fail_msg], msg_hubmax
        cmp byte [depth], HUB_DEPTH
        jae .err
        xor bx, bx
.f:
        cmp byte [h_used+bx], 0
        je .got
        inc bx
        cmp bx, HUB_N
        jb .f
        jmp .err
.got:
        mov [new_hub], bl
        call set_config
        mov word [fail_msg], msg_cfg
        jc .err
%ifdef STALL_TEST
        ; (prueba) una petición que el concentrador rechaza: lo que sigue
        ; solo funciona si el EP0 se reanuda
        mov eax, 0x000055A0
        mov edx, 0x00080000
        call ctl_req
        jc .st1
        mov word [fail_msg], msg_hubdesc
        jmp .err
.st1:
%endif
        ; descriptor de concentrador (29h; 2Ah en USB 3)
        mov eax, 0x290006A0
        cmp byte [speed], 4
        jb .d2
        mov eax, 0x2A0006A0
.d2:
        mov edx, 0x000C0000
        call ctl_req
        mov word [fail_msg], msg_hubdesc
        jc .err
        mov bx, DESC
        call zone
        mov al, [cs:bx+2]                       ; bNbrPorts
        test al, al
        jz .err
        cmp al, 15
        jbe .np
        mov al, 15
.np:
        mov [hub_np], al
        mov al, [cs:bx+5]                       ; bPwrOn2PwrGood (2 ms)
        mov [hub_pwr], al
        mov al, [cs:bx+3]                       ; wHubCharacteristics
        shr al, 5
        and al, 3
        cmp byte [speed], 3
        je .tt
        xor al, al
.tt:
        mov [hub_ttt], al
        cmp byte [cur_type], HCT_XHCI
        jne .cok                    ; (lo del contexto es cosa del xHCI)
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 1                  ; A0: solo el slot
        add bx, [ctx_sz]
        mov byte [hubf], 1
        mov eax, 1
        call slot_fill
        mov byte [hubf], 0
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jnc .cok
        call show_cc
        mov word [hub_np], 0                    ; (y hub_ttt)
        jmp .err
.cok:
        movzx bx, byte [new_hub]
        mov byte [h_used+bx], 1
        mov al, [cur_hc]
        mov [h_hc+bx], al
        mov al, [cur_slot]
        mov [h_slot+bx], al
        mov al, [cur_pool]
        mov [h_pool+bx], al
        mov al, [hub_np]
        mov [h_np+bx], al
        mov [d_np], al
        mov al, [speed]
        mov [h_speed+bx], al
        mov al, [port]
        mov [h_root+bx], al
        mov al, [depth]
        mov [h_depth+bx], al
        mov al, [tt_slot]
        mov [h_ttslot+bx], al
        mov al, [tt_port]
        mov [h_ttport+bx], al
        shl bx, 2
        mov eax, [route]
        mov [h_route+bx], eax
        mov word [hub_np], 0                    ; (y hub_ttt)
        mov al, [new_hub]
        mov [hp_hub], al
        ; USB 3: SET_HUB_DEPTH
        cmp byte [speed], 4
        jb .pw
        movzx eax, byte [depth]
        shl eax, 16
        or eax, 0x0C20
        xor edx, edx
        call ctl_req
.pw:
        ; corriente a los puertos (PORT_POWER) y esperar a que sea estable
        mov cl, 1
.pl:
        cmp cl, [d_np]
        ja .pd
        mov [hp_port], cl
        push cx
        mov ax, 0x0308
        call hub_feat
        pop cx
        inc cl
        jmp .pl
.pd:
        movzx ax, byte [hub_pwr]
        shl ax, 1
        mov cl, 55
        div cl
        movzx cx, al
        add cx, 3
        call tmo_start
.w:
        call tmo_check
        jnc .w
        clc
        ret
.err:
        stc
        ret

; ---- /INFO: una línea por dispositivo ----
info_line:
        cmp byte [opt_info], 0
        jne info_line_go
        ret
section .init
info_line_go:
        pushad
        mov cl, [depth]
        inc cl
.in:
        mov dl, ' '
        call putc
        mov dl, ' '
        call putc
        dec cl
        jnz .in
        movzx eax, byte [port]
        call putdec
        mov ebx, [route]
        mov cl, [depth]
.rt:
        test cl, cl
        jz .rtd
        mov dl, '.'
        call putc
        mov eax, ebx
        and eax, 15
        call putdec
        shr ebx, 4
        dec cl
        jmp .rt
.rtd:
        mov dl, ' '
        call putc
        movzx bx, byte [speed]
        cmp bx, 4
        jbe .spk
        xor bx, bx
.spk:
        shl bx, 1
        mov dl, [spd_tab+bx]
        call putc
        mov dl, [spd_tab+bx+1]
        call putc
        mov dl, ' '
        call putc
        cmp byte [d_have], 0
        je .fail
        mov ax, [d_vid]
        call puthex4
        mov dl, ':'
        call putc
        mov ax, [d_pid]
        call puthex4
        ; interfaces: clase/subclase/protocolo
        xor bx, bx
.if:
        cmp bl, [n_if]
        jae .ifd
        mov dl, ' '
        call putc
        mov al, [if_tab+bx]
        call puthex2
        mov dl, '/'
        call putc
        mov al, [if_tab+bx+1]
        call puthex2
        mov dl, '/'
        call putc
        mov al, [if_tab+bx+2]
        call puthex2
        add bx, 3
        jmp .if
.ifd:
        mov si, msg_arrow
        call puts
        movzx bx, byte [d_role]
        shl bx, 1
        mov si, [role_tab+bx]
        call puts
        cmp byte [d_role], 5
        jne .nh
        movzx eax, byte [d_np]
        call putdec
        mov si, msg_nports
        call puts
.nh:
        cmp byte [d_role], 6
        je .fmsg
        call crlf
        jmp .r
.fail:
        mov si, msg_r6
        call puts
.fmsg:
        mov dl, ' '
        call putc
        mov si, [fail_msg]
        call puts
.r:
        popad
        ret

section .text
putc:
        push ax
        mov ah, 2
        int 0x21
        pop ax
        ret
puthex4:
        push ax
        mov al, ah
        call puthex2
        pop ax
puthex2:
        push ax
        push dx
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 15
        call .n
        pop dx
        pop ax
        ret
.n:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        call putc
        ret

d_have      db 0
d_role      db 0
d_np        db 0
n_if        db 0
d_vid       dw 0
d_pid       dw 0
IF_MAX      equ 8
if_tab      times IF_MAX * 3 db 0
section .init
spd_tab     db "??", "FS", "LS", "HS", "SS"
role_tab    dw msg_r0, msg_r1, msg_r2, msg_r3, msg_r4, msg_r5, msg_r6, msg_r7, msg_r8
msg_arrow   db " -> ", 0
msg_r0      db "sin uso", 0
msg_r1      db "disco", 0
msg_r2      db "teclado", 0
msg_r3      db "ratón", 0
msg_r4      db "teclado y ratón", 0
msg_r5      db "concentrador de ", 0
msg_r6      db "ERROR", 0
msg_r7      db "mando de juego", 0
msg_r8      db "Bluetooth (HID proxy): para después", 0
msg_nports  db " puertos", 0
msg_iports  db "puertos con algo conectado:", 0
msg_iports2 db " (de ", 0
msg_iports3 db "). Velocidad: LS baja, FS plena, HS alta, SS súper.", 13, 10, 0

section .text
; /V: código de finalización de la orden que ha fallado (AL; FFh = sin respuesta)
show_cc:
        jnc .r
        pushf
        push ax
        cmp byte [opt_verbose], 0
        je .p
        push ax
        mov si, msg_cc
        call puts
        pop ax
        movzx eax, al
        call putdec
        call crlf
.p:
        pop ax
        popf
.r:
        ret

; SET_CONFIGURATION con el valor del descriptor de configuración en DESC
set_config:
        mov bx, DESC
        call zone
        movzx eax, byte [cs:bx+5]
        shl eax, 16
        or eax, 0x0900
        mov [ctl_setup], eax
        mov dword [ctl_setup+4], 0
        call control
        ret

; memoria USB: endpoints bulk
role_disk:
        call set_config
        mov word [fail_msg], msg_cfg
        jc .err
        mov al, [cur_pool]
        mov [bk_pool], al
        mov word [tog_in], 0        ; (y tog_out)
        cmp byte [cur_type], HCT_XHCI
        jne .nr
        ; anillos bulk desde el principio (puede haberlos usado otra memoria)
        mov word [bin_idx], 0x0100  ; índice 0, ciclo 1
        mov word [bout_idx], 0x0100
        mov bx, BIN_RING
        mov cx, 0x100
        call zero_zone
        mov bx, BOUT_RING
        mov cx, 0x100
        call zero_zone
.nr:
        mov al, [cur_slot]
        mov [slot], al
        mov al, [t_in]
        mov [dci_in], al
        mov al, [t_out]
        mov [dci_out], al
        mov al, [t_if]
        mov [d_if], al
        mov ax, [t_mps_in]
        mov [mps_in], ax
        mov ax, [t_mps_out]
        mov [mps_out], ax
        cmp byte [cur_type], HCT_XHCI
        jne .cfgd
        ; Configure Endpoint: bulk IN y bulk OUT
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        mov cl, [dci_in]
        mov edx, 1
        shl edx, cl
        or eax, edx
        mov cl, [dci_out]
        mov edx, 1
        shl edx, cl
        or eax, edx
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, byte [dci_in]
        cmp al, [dci_out]
        jae .mx
        mov al, [dci_out]
.mx:
        call slot_fill
        mov byte [ep_ival], 0
        mov dword [ep_d4], 1024
        mov al, [dci_in]
        mov dx, BIN_RING
        mov cx, [mps_in]
        mov ah, 6                               ; bulk IN
        call set_ep
        mov al, [dci_out]
        mov dx, BOUT_RING
        mov cx, [mps_out]
        mov ah, 2                               ; bulk OUT
        call set_ep
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jc .err
.cfgd:
        ; con /U solo vale la memoria de la que arrancó el sistema
        cmp byte [boot_saved], 0
        je .any
        cmp byte [boot_match], 0
        jne .any
        call boot_cmp
        mov word [fail_msg], msg_notboot
        jc .err
        mov byte [boot_match], 1
.any:
        mov byte [disk_found], 1
        mov al, [port]
        mov [dport], al
        mov al, [cur_pool]
        mov [dpool], al
        mov al, [par_hub]
        mov [d_hub], al
        mov al, [par_port]
        mov [d_hport], al
        mov cl, [port]
        call rp_ack                 ; la conexión de ahora ya está vista
        clc
.err:
        ret

; ¿es esta memoria el disco de arranque de la BIOS? Se compara su sector 0
; con el que se leyó por INT 13h antes de tomar el controlador. CF si no.
boot_cmp:
        call disk_ready
        jc .r
        push es
        push cs
        pop es
        mov si, [data_rel]
        add si, [mem_off]
        mov di, [boot_sec]
        mov cx, 256
        cld
        repe cmpsw
        pop es
        je .ok
        stc
        ret
.ok:
        clc
.r:
        ret

; Configure Endpoint del slot actual con el contexto de entrada
cfg_ep_cmd:
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 12 << 10
        call xhci_cmd
        ret

; teclado y/o ratón (want_k, want_m): protocolo de arranque y endpoints
section .init
role_hid:
        call set_config
        jc .err
        cmp byte [want_k], 0
        je .m1
        movzx eax, byte [pk_if]
        call set_boot
.m1:
        cmp byte [want_m], 0
        je .m2
        movzx eax, byte [pm_if]
        call set_boot
        call m_layout
.m2:
        cmp byte [cur_type], HCT_XHCI
        jne .cfgd
        ; endpoints de interrupción
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        xor dl, dl                              ; DCI mayor
        cmp byte [want_k], 0
        je .a1
        mov cl, [pk_dci]
        bts eax, ecx
        mov dl, cl
.a1:
        cmp byte [want_m], 0
        je .a2
        mov cl, [pm_dci]
        bts eax, ecx
        cmp cl, dl
        jbe .a2
        mov dl, cl
.a2:
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, dl
        call slot_fill
        cmp byte [want_k], 0
        je .e1
        mov al, [pk_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [pk_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov al, [pk_dci]
        mov dx, KINT_RING
        mov cx, [pk_mps]
        mov ah, 7                               ; interrupción IN
        call set_ep
.e1:
        cmp byte [want_m], 0
        je .e2
        mov al, [pm_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [pm_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov al, [pm_dci]
        mov dx, MINT_RING
        mov cx, [pm_mps]
        mov ah, 7
        call set_ep
.e2:
        call cfg_ep_cmd
        jc .err
.cfgd:
        mov al, [cur_slot]
        cmp byte [want_k], 0
        je .o1
        mov [k_slot], al
        mov ah, [pk_dci]
        mov [k_dci], ah
        mov cx, [pk_mps]
        mov [ip_mps], cx
        call cap_len
        mov [k_len], cx
        mov ah, [cur_pool]
        mov [ip_pool], ah
        xor bx, bx
        call pipe_ready
        mov byte [k_on], 1
.o1:
        cmp byte [want_m], 0
        je .o2
        mov [m_slot], al
        mov ah, [pm_dci]
        mov [m_dci], ah
        mov cx, [pm_mps]
        mov [ip_mps+2], cx
        call cap_len
        mov [m_len], cx
        mov ah, [cur_pool]
        mov [ip_pool+1], ah
        mov bx, 1
        call pipe_ready
        mov byte [m_on], 1
.o2:
        clc
.err:
        ret
; [USB-RATON] el ratón (interfaz pm_if): ¿da los informes de arranque que
; se le acaban de pedir? GET_PROTOCOL = 0, sí. Si no lo dice, o no es de
; arranque, se leen sus informes según su descriptor de informe (hr_parse),
; si en él están X e Y relativas; si no, como los de arranque.
m_layout:
        mov byte [hr_on], 0
%ifndef HR_FORZAR
        cmp byte [pm_boot], 0
        je .desc
        mov dword [ctl_setup], 0x000003A1
        movzx eax, byte [pm_if]
        or eax, 0x00010000
        mov [ctl_setup+4], eax
        mov bx, DESC
        call zone
        mov byte [cs:bx], 0xFF
        call control
        jc .desc
        mov bx, DESC
        call zone
        cmp byte [cs:bx], 0
        je .r                       ; de arranque: bytes 0, 1 y 2
.desc:
%endif
        mov cx, [pm_rlen]
        jcxz .r
        cmp cx, 1024
        jbe .l1
        mov cx, 1024
.l1:
        push cx
        mov bx, [data_rel]
        push cx
        mov cx, 1024
        call zero_zone
        pop cx
        mov dword [ctl_setup], 0x22000681
        movzx ax, byte [pm_if]
        mov [ctl_setup+4], ax
        mov [ctl_setup+6], cx
        mov ax, [data_rel]
        mov [ctl_buf], ax
        call control
        mov word [ctl_buf], DESC
        pop cx
        jc .r
        mov bx, [data_rel]
        call zone
        mov si, bx
        call hr_parse
        jc .r
        mov byte [hr_on], 1
        cmp byte [opt_info], 0
        je .r
        mov si, msg_hron
        call puts
        movzx eax, byte [hr_rid]
        call putdec
        call crlf
.r:
        ret
msg_hron    db "Ratón: informes según su descriptor (no de arranque); identificador ", 0

cap_len:
        cmp cx, 64
        jbe .r
        mov cx, 64
.r:
        ret

; SET_PROTOCOL (arranque) y SET_IDLE (0) a la interfaz EAX; los errores no
; importan (hay dispositivos que no admiten SET_IDLE)
set_boot:
        push eax
        shl eax, 0
        mov dword [ctl_setup], 0x00000B21
        mov [ctl_setup+4], eax
        call control
        pop eax
        mov dword [ctl_setup], 0x00000A21
        mov [ctl_setup+4], eax
        call control
        ret

; bInterval AL -> exponente del intervalo del xHCI (AL)
calc_ival:
        cmp byte [speed], 3
        jae .hs
        mov ah, 3                   ; FS/LS: milisegundos -> 2^n * 125 us
.l:
        cmp al, 1
        jbe .d
        shr al, 1
        inc ah
        jmp .l
.d:
        cmp ah, 10
        jbe .k
        mov ah, 10
.k:
        mov al, ah
        ret
.hs:
        test al, al
        jz .r
        dec al
        cmp al, 15
        jbe .r
        mov al, 15
.r:
        ret

; poner a cero CX bytes de la zona desde BX
section .text
zero_zone:
        push di
        push es
        call zone
        mov di, bx
        push cs
        pop es
        xor al, al
        cld
        rep stosb
        pop es
        pop di
        ret

want_k      db 0
want_m      db 0
disk_found  db 0
            db 0
mps0        dw 8
cur_out     dw 0
ep_ival     db 0
            db 0
ep_d4       dd 1024

clear_in:
        push di
        push cx
        push es
        mov di, IN_CTX
        add di, [mem_off]
        push cs
        pop es
        mov cx, 0x900 / 2
        xor ax, ax
        rep stosw
        pop es
        pop cx
        pop di
        ret

; contexto del endpoint DCI AL: AH = tipo, DX = anillo, CX = paquete máximo
set_ep:
        push bx
        movzx bx, al
        inc bx
        imul bx, [ctx_sz]
        add bx, IN_CTX
        call zone
        movzx eax, ah
        shl eax, 3
        or eax, 3 << 1                          ; CErr = 3
        movzx ecx, cx
        shl ecx, 16
        or eax, ecx
        mov [cs:bx+4], eax
        push bx
        mov bx, dx
        call zone
        call phys
        pop bx
        or al, 1
        mov [cs:bx+8], eax
        mov dword [cs:bx+12], 0
        mov eax, [cs:ep_d4]
        mov [cs:bx+16], eax
        movzx eax, byte [cs:ep_ival]
        shl eax, 16
        mov [cs:bx], eax
        pop bx
        ret

; recorrer el descriptor de configuración: memoria (08/06/50, bulk), teclado
; (03/01/01) y ratón (03/01/02) con su endpoint de interrupción de entrada
parse_config:
        mov bx, DESC
        call zone
        mov si, bx
        movzx cx, byte [ctl_setup+6]
        add cx, bx
        xor ax, ax
        mov [p_msc], al
        mov [p_kbd], al
        mov [p_mou], al
        mov [t_in], al
        mov [t_out], al
        mov [ifk], al
        mov [p_hub], al
        mov [p_joy], al
.l:
        cmp si, cx
        jae .end
        mov al, [cs:si]
        test al, al
        jz .end
        mov ah, [cs:si+1]
        cmp ah, 4                   ; interfaz
        jne .ep
        mov byte [ifk], 0
        mov al, [cs:si+2]
        mov [ifn], al
        ; /INFO: clase/subclase/protocolo de cada interfaz (sin alternativas)
        cmp byte [cs:si+3], 0
        jne .nif
        movzx di, byte [n_if]
        cmp di, IF_MAX * 3
        jae .nif
        mov al, [cs:si+5]
        mov [if_tab+di], al
        mov ax, [cs:si+6]
        mov [if_tab+di+1], ax
        add byte [n_if], 3
.nif:
        cmp byte [cs:si+5], 9
        jne .nh
        mov byte [p_hub], 1
        jmp .n
.nh:
        cmp byte [cs:si+5], 8
        jne .hid
        cmp byte [cs:si+6], 6
        jne .n
        cmp byte [cs:si+7], 0x50
        jne .n
        mov byte [ifk], 1
        mov al, [ifn]
        mov [t_if], al
        jmp .n
.hid:
        cmp byte [cs:si+5], 3
        jne .xb
        mov byte [if_rlen], 0
        mov byte [if_rlen+1], 0
        mov byte [if_boot], 1
        cmp byte [cs:si+6], 1       ; subclase de arranque
        jne .hr
        mov al, [cs:si+7]
        cmp al, 1
        jb .hj
        cmp al, 2
        ja .hj
        inc al
        mov [ifk], al               ; 2 teclado, 3 ratón
        jmp .n
.hr:
        ; [USB-RATON] protocolo de ratón sin la subclase de arranque (hay
        ; paneles así): ratón, con los informes según su descriptor
        cmp byte [cs:si+6], 0
        jne .hj
        cmp byte [cs:si+7], 2
        jne .hj
        mov byte [if_boot], 0
        mov byte [ifk], 3
        jmp .n
.hj:
        ; un HID que no es teclado ni ratón de arranque: puede ser un mando
        ; (lo dirá su descriptor de informe)
        cmp byte [p_joy], 0
        jne .n
        cmp byte [cs:si+3], 0       ; sin interfaces alternativas
        jne .n
        mov byte [ifk], 4
        mov word [pj_rlen], 0
        jmp .n
.xb:
        ; mando tipo Xbox 360 con cable: FFh/5Dh/01h
        cmp byte [cs:si+5], 0xFF
        jne .n
        cmp word [cs:si+6], 0x015D
        jne .n
        cmp byte [p_joy], 0
        jne .n
        mov byte [ifk], 5
        jmp .n
.ep:
        cmp ah, 0x21                ; descriptor HID: longitud del de informe
        jne .ep5
        cmp al, 9
        jb .n
        cmp byte [cs:si+6], 0x22
        jne .n
        mov ax, [cs:si+7]
        cmp byte [ifk], 3
        jne .ep4
        mov [if_rlen], ax           ; (el de un ratón: por si hace falta)
        jmp .n
.ep4:
        cmp byte [ifk], 4
        jne .n
        mov [pj_rlen], ax
        jmp .n
.ep5:
        cmp ah, 5                   ; endpoint
        jne .n
        mov dl, [cs:si+2]           ; dirección
        mov dh, dl
        and dh, 0x0F
        shl dh, 1                   ; DCI = 2 * número (+1 si es de entrada)
        test dl, 0x80
        jz .o
        inc dh
.o:
        mov di, [cs:si+4]
        and di, 0x7FF
        mov al, [cs:si+3]
        and al, 3
        cmp byte [ifk], 1
        jne .ek
        cmp al, 2                   ; bulk
        jne .n
        test dl, 0x80
        jz .bo
        mov [t_in], dh
        mov [t_mps_in], di
        jmp .n
.bo:
        mov [t_out], dh
        mov [t_mps_out], di
        jmp .n
.ek:
        cmp al, 3                   ; interrupción
        jne .n
        test dl, 0x80
        jz .n
        mov al, [cs:si+6]           ; bInterval
        cmp byte [ifk], 2
        jne .em
        cmp byte [p_kbd], 0
        jne .n
        mov [pk_dci], dh
        mov [pk_mps], di
        mov [pk_ival], al
        mov ah, [ifn]
        mov [pk_if], ah
        mov byte [p_kbd], 1
        jmp .n
.em:
        cmp byte [ifk], 3
        jne .ej
        cmp byte [p_mou], 0
        jne .n
        mov [pm_dci], dh
        mov [pm_mps], di
        mov [pm_ival], al
        mov ah, [ifn]
        mov [pm_if], ah
        mov ax, [if_rlen]
        mov [pm_rlen], ax
        mov al, [if_boot]
        mov [pm_boot], al
        mov byte [p_mou], 1
        jmp .n
.ej:
        cmp byte [ifk], 4           ; candidato a mando (4 HID, 5 tipo Xbox 360)
        jb .n
        cmp byte [p_joy], 0
        jne .n
        mov [pj_dci], dh
        mov [pj_mps], di
        mov [pj_ival], al
        mov ah, [ifn]
        mov [pj_if], ah
        mov ah, [ifk]
        sub ah, 3
        mov [p_joy], ah             ; 1 HID, 2 tipo Xbox 360
.n:
        movzx ax, byte [cs:si]
        add si, ax
        jmp .l
.end:
        cmp byte [t_in], 0
        je .r
        cmp byte [t_out], 0
        je .r
        mov byte [p_msc], 1
.r:
        ret
ifk         db 0
p_hub       db 0
ifn         db 0
p_msc       db 0
p_kbd       db 0
p_mou       db 0
t_in        db 0
t_out       db 0
t_if        db 0                    ; interfaz de la memoria USB
            db 0
t_mps_in    dw 0
t_mps_out   dw 0
pk_dci      db 0
pk_ival     db 0
pk_if       db 0
            db 0
pk_mps      dw 0
pm_dci      db 0
pm_ival     db 0
pm_if       db 0
            db 0
pm_mps      dw 0
pm_rlen     dw 0                    ; ratón: longitud de su descriptor de informe
pm_boot     db 0                    ; ratón: interfaz de arranque (subclase 1)
if_boot     db 0
if_rlen     dw 0
p_joy       db 0                    ; 1 = HID que puede ser un mando, 2 = tipo Xbox 360
pj_dci      db 0
pj_ival     db 0
pj_if       db 0
pj_mps      dw 0
pj_rlen     dw 0                    ; longitud de su descriptor de informe
j_new       dw 0
j_caps      dw 0
j_notpad    db 0

section .init
; mando de juego: leer su descriptor de informe y sacar el mapa (o poner el
; fijo de los de tipo Xbox 360), y su endpoint de interrupción. CF si no se
; usa: con [j_notpad] = 1 porque no es un mando (otro HID), si no por un fallo.
role_joy:
        mov byte [j_notpad], 1
        xor bx, bx
.f:
        cmp byte [j_on+bx], 0
        je .got
        inc bx
        cmp bx, JOY_N
        jb .f
        stc
        ret
.got:
        mov [j_new], bx
        cmp byte [p_joy], 2
        jne .hid
        call set_config
        jc .err
        mov di, [j_new]
        imul di, HJM_SZ
        add di, j_map
        call hj_xbox360
        mov al, [di+HJM_NAX]
        mov ah, [di+HJM_NBTN]
        jmp .ep
.hid:
        cmp word [pj_rlen], 0
        je .np
        call set_config
        jc .err
        ; el descriptor de informe (hasta 1 KB), a la zona de datos
        mov bx, [data_rel]
        mov cx, 1024
        call zero_zone
        mov cx, [pj_rlen]
        cmp cx, 1024
        jbe .l1
        mov cx, 1024
.l1:
        mov [pj_rlen], cx
        mov dword [ctl_setup], 0x22000681
        movzx ax, byte [pj_if]
        mov [ctl_setup+4], ax
        mov [ctl_setup+6], cx
        mov ax, [data_rel]
        mov [ctl_buf], ax
        call control
        mov word [ctl_buf], DESC
        mov word [fail_msg], msg_desc
        jc .err
        mov bx, [cs:data_rel]
        call zone
        mov si, bx
        mov cx, [pj_rlen]
        mov di, [j_new]
        imul di, HJM_SZ
        add di, j_map
        call hj_parse
        jc .np
.ep:
        mov [j_caps], ax
        cmp byte [cur_type], HCT_XHCI
        jne .cfgd
        ; su anillo, vacío
        mov bx, [j_new]
        shl bx, 8
        add bx, JINT_RING
        mov cx, 0x100
        call zero_zone
        mov bx, [j_new]
        shl bx, 1
        mov word [jint_idx+bx], 0x0100
        ; el endpoint de interrupción
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        movzx ecx, byte [pj_dci]
        bts eax, ecx
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, byte [pj_dci]
        call slot_fill
        mov al, [pj_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [pj_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov al, [pj_dci]
        mov dx, [j_new]
        shl dx, 8
        add dx, JINT_RING
        mov cx, [pj_mps]
        mov ah, 7                               ; interrupción IN
        call set_ep
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jc .err
.cfgd:
        mov bx, [j_new]
        mov al, [cur_slot]
        mov [j_slot+bx], al
        mov al, [pj_dci]
        mov [j_dci+bx], al
        mov cx, [pj_mps]
        call cap_len
        mov di, bx
        shl di, 1
        mov [j_len+di], cx
        ; el estado: en reposo hasta que llegue el primer informe
        mov di, bx
        imul di, HJS_SZ
        add di, j_est
        mov al, [p_joy]
        mov [di+HJS_TIPO], al
        mov byte [di+HJS_FLAGS], 1
        mov ax, [j_caps]
        mov [di+HJS_NAX], ax
        mov ax, [d_vid]
        mov [di+HJS_VID], ax
        mov ax, [d_pid]
        mov [di+HJS_PID], ax
        mov dword [di+HJS_EJES], 0x80808080
        mov word [di+HJS_EJES+4], 0
        mov word [di+HJS_CRUZ], 0
        mov word [di+HJS_BTN], 0
        ; espera del informe reciente: su intervalo y 1 ms, en microtramas
        movzx ax, byte [pj_ival]
        cmp byte [speed], 3
        jae .hs
        shl ax, 3
        jmp .w
.hs:
        mov cl, al
        dec cl
        and cl, 15
        mov ax, 1
        shl ax, cl
.w:
        add ax, 8
        cmp ax, 80
        jbe .w1
        mov ax, 80
.w1:
        cmp ax, [j_wait]
        jbe .w2
        mov [j_wait], ax
.w2:
        mov al, [cur_pool]
        mov [ip_pool+bx+2], al
        mov di, bx
        shl di, 1
        mov ax, [pj_mps]
        mov [ip_mps+di+4], ax
        push bx
        add bx, 2
        call pipe_ready
        pop bx
        mov byte [j_on+bx], 1
        clc
        ret
.np:
        stc
        ret
.err:
        mov byte [j_notpad], 0
        stc
        ret

%include "hidjoyp.inc"
%include "hidraton.inc"

section .text
; ---- disco: capacidad, partición y BPB ----
disk_setup:
        call disk_ready
        jc .err
        mov bx, [cs:data_rel]
        call zone
        jmp disk_sec0
.err:
        stc
        ret
; la memoria preparada, su capacidad y su sector 0 en DATA; CF si falla
disk_ready:
        mov word [fail_msg], msg_scsi
        ; TEST UNIT READY. Si falla se pide el motivo (REQUEST SENSE: borra
        ; el aviso de «recién conectado») y se espera un poco: un lector de
        ; tarjetas o un disco duro USB tardan en estar preparados.
        mov cx, 12
.tur:
        push cx
        mov dword [scsi_cb], 0
        mov dword [scsi_cb+4], 0
        mov byte [scsi_cblen], 6
        xor ebx, ebx
        xor ecx, ecx
        mov al, 0
        call scsi
        pop cx
        jnc .ready
        push cx
        mov dword [scsi_cb], 0x03
        mov dword [scsi_cb+4], 18
        mov byte [scsi_cblen], 6
        mov bx, [cs:data_rel]
        call zone
        call phys
        mov ebx, eax
        mov ecx, 18
        mov al, 1
        call scsi
        mov cx, 4
        call tmo_start
.tw:
        call tmo_check
        jnc .tw
        pop cx
        loop .tur
.ready:
        ; READ CAPACITY(10)
        mov dword [scsi_cb], 0x25
        mov dword [scsi_cb+4], 0
        mov word [scsi_cb+8], 0
        mov byte [scsi_cblen], 10
        mov bx, [cs:data_rel]
        call zone
        call phys
        mov ebx, eax
        mov ecx, 8
        mov al, 1
        call scsi
        jc .err
        mov bx, [cs:data_rel]
        call zone
        mov eax, [cs:bx]
        call bswap_eax
        inc eax
        mov [i13_total], eax
        shr eax, 11
        mov [cap_mb], eax
        mov eax, [cs:bx+4]
        call bswap_eax
        cmp eax, 512
        mov word [fail_msg], msg_bsize
        jne .err
        ; sector 0: ¿tabla de particiones o FAT directamente?
        mov dword [part_start], 0
        mov dword [rw_lba], 0
        mov word [rw_n], 1
        mov al, 0x28
        call scsi_rw
        mov word [fail_msg], msg_scsi
        jc .err
        clc
        ret
.err:
        stc
        ret
disk_sec0:
        cmp word [cs:bx+510], 0xAA55
        mov word [fail_msg], msg_nofat
        jne .err
        cmp word [cs:bx+0x0B], 512
        jne .mbr
        cmp byte [cs:bx+0x26], 0x29             ; BPB extendido: sin particiones
        je .bpb
        cmp byte [cs:bx+0x15], 0xF0
        jae .bpb
.mbr:
        mov si, bx
        add si, 0x1BE
        mov cx, 4
.pe:
        mov al, [cs:si+4]
        cmp al, 0x01
        je .gotp
        cmp al, 0x04
        je .gotp
        cmp al, 0x06
        je .gotp
        cmp al, 0x0E
        je .gotp
        add si, 16
        loop .pe
        jmp .err
.gotp:
        mov eax, [cs:si+8]
        mov [part_start], eax
        mov [rw_lba], eax
        mov word [rw_n], 1
        mov al, 0x28
        call scsi_rw
        jc .err
        mov bx, [cs:data_rel]
        call zone
        cmp word [cs:bx+0x0B], 512
        jne .err
.bpb:
        ; ¿de verdad FAT12/FAT16? Un FAT32 o un NTFS sin particiones también
        ; llevan 512 en +0Bh y F8h en +15h, pero no tienen sectores por FAT
        ; de 16 bits ni entradas de directorio raíz: al núcleo no se le da
        ; un BPB así
        mov word [fail_msg], msg_nofat
        cmp byte [cs:bx+0x0D], 0            ; sectores por clúster
        je .err
        cmp word [cs:bx+0x0E], 0            ; sectores reservados
        je .err
        cmp byte [cs:bx+0x10], 0            ; número de FAT
        je .err
        cmp word [cs:bx+0x11], 0            ; entradas del directorio raíz
        je .err
        cmp word [cs:bx+0x16], 0            ; sectores por FAT
        je .err
        mov si, bx
        add si, 0x0B
        mov di, bpb
        mov cx, 25
.cp:
        mov al, [cs:si]
        mov [cs:di], al
        inc si
        inc di
        loop .cp
        clc
        ret
.err:
        stc
        ret

; ---- texto ----
puts:
        push ax
        push dx
.l:
        mov dl, [cs:si]
        test dl, dl
        jz .d
        mov ah, 2
        int 0x21
        inc si
        jmp .l
.d:
        pop dx
        pop ax
        ret
crlf:
        push si
        mov si, msg_crlf
        call puts
        pop si
        ret
putdec:
        push ebx
        push ecx
        push edx
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
        pop edx
        pop ecx
        pop ebx
        ret
section .init
puthex8:
        push cx
        mov cx, 8
.h:
        rol eax, 4
        push eax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov dl, al
        mov ah, 2
        int 0x21
        pop eax
        loop .h
        pop cx
        ret

section .text
bdf         dw 0
maxports    db 0
opt_force   db 0
opt_verbose db 0
hcc1        dd 0
cap_mb      dd 0
fail_msg    dw 0
data_rel    dw 0                    ; el búfer de datos del disco (4 KB), en la zona
boot_sec    dw 0                    ; /U: sector 0 del disco de arranque de la BIOS
opt_help    db 0
; Desde aquí, solo mensajes y código de inicialización: nada se usa después
; del arranque (init_fin puede poner encima el búfer de datos del disco).
section .init
msg_title   db "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)", 13, 10, 0
msg_found   db "Controlador xHCI en ", 0
msg_port    db "Dispositivo en el puerto ", 0
msg_speed   db ", velocidad ", 0
msg_disk    db "Disco USB: ", 0
msg_cc      db "xHCI: código de finalización ", 0
msg_kbd     db "Teclado USB preparado.", 13, 10, 0
msg_mouse   db "Ratón USB preparado.", 13, 10, 0
msg_joy     db "Mando USB preparado: ", 0
msg_joy2    db ", ", 0
msg_joy3    db " ejes y ", 0
msg_joy4    db " botones.", 13, 10, 0
msg_joy5    db " botones (tipo Xbox 360).", 13, 10, 0
units       db 0
msg_mbas    db " MB en la unidad "
msg_drv     db "?:", 13, 10, 0
msg_no8042  db "Sin controlador de teclado (8042): las teclas van al búfer de la BIOS.", 13, 10, 0
msg_scratch db "El controlador xHCI necesita memoria extendida: cargue HSPXMS.SYS", 13, 10
            db "antes que HSPUSB.SYS en CONFIG.SYS.", 13, 10, 0
msg_nomem   db "No hay memoria suficiente para HSPUSB.", 13, 10, 0
msg_v86     db "No funciona en modo V86 sin VCPI (Windows 386 mejorado).", 13, 10, 0
msg_high    db "Los registros del xHCI están por encima de 4 GB: no se admite.", 13, 10, 0
msg_bios    db "La BIOS está usando el controlador USB (teclado USB o unidad de", 13, 10
            db "arranque): no se toca. Use /F para tomarlo igualmente.", 13, 10, 0
msg_bootusb db "El sistema ha arrancado desde una memoria USB que maneja la BIOS:", 13, 10
            db "HSPUSB no toma el controlador (la BIOS perdería el disco de arranque).", 13, 10
            db "Con /U lo toma y atiende él ese disco; si algo falla habrá que reiniciar.", 13, 10, 0
msg_bootok  db "Disco de arranque USB atendido por HSPUSB (INT 13h).", 13, 10, 0
msg_bootlost db "AVISO: la memoria de arranque no aparece entre los dispositivos USB.", 13, 10
            db "Si era USB, el sistema ya no puede leerla: reinicie y pulse F5 al ver", 13, 10
            db "el menú de arranque (o quite /U).", 13, 10, 0
msg_notboot db "no es la memoria de arranque.", 13, 10, 0
msg_full    db "USB: demasiados dispositivos.", 13, 10, 0
msg_hubmax  db "USB: demasiados concentradores encadenados.", 13, 10, 0
msg_hubdesc db "USB: el concentrador no da su descriptor.", 13, 10, 0
msg_pause   db "Pausa de 15 s para la foto (una tecla la acorta)...", 13, 10, 0
msg_reset   db "El controlador xHCI no se reinicia.", 13, 10, 0
msg_nodev   db "No hay ninguna memoria, teclado, ratón ni mando USB que se pueda usar.", 13, 10, 0
msg_slot    db "USB: ha fallado Enable Slot.", 13, 10, 0
msg_addr    db "USB: ha fallado Address Device.", 13, 10, 0
msg_desc    db "USB: no se pueden leer los descriptores.", 13, 10, 0
msg_cfg     db "USB: ha fallado Set Configuration.", 13, 10, 0
msg_epcfg   db "USB: ha fallado Configure Endpoint.", 13, 10, 0
msg_scsi    db "El disco USB no responde.", 13, 10, 0
msg_bsize   db "El disco USB no tiene sectores de 512 bytes.", 13, 10, 0
msg_nofat   db "El disco USB no tiene un sistema de archivos FAT12/FAT16.", 13, 10, 0
%include "usb_hcini.inc"
section .text
boot_saved  db 0
boot_match  db 0
opt_info    db 0
hc_ours     db 0
msg_crlf    db 13, 10, 0
; ---- INIT: la inicialización está en el fichero detrás de lo residente
; (sección .init), pero se ejecuta en CS:INIT_ORG, por encima de donde irá la
; zona de trabajo. Se lleva allí y, al acabar, solo se queda lo residente y
; la zona: la inicialización se descarta. Hace falta memoria hasta
; CS:init_end mientras tanto; si no la hay (DEVICEHIGH en un bloque de
; memoria superior pequeño), sin tocar nada se dice «no me quedo, me falta
; memoria» (estado «ocupado»; el núcleo deja el final de la memoria
; disponible en el campo de la dirección de corte, como MS-DOS 5+) y el
; núcleo lo carga abajo. DS:BX = la petición.
init_go:
        mov cx, [bx+0x0E]
        or cx, [bx+0x10]
        jz .ok                      ; (un DOS que no lo dice)
        movzx eax, word [bx+0x10]
        shl eax, 4
        movzx ecx, word [bx+0x0E]
        add eax, ecx                ; final de la memoria disponible
        xor ecx, ecx
        mov cx, cs
        shl ecx, 4
        add ecx, init_end
        cmp ecx, eax
        jbe .ok
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
        mov word [bx+3], 0x0300
        mov ax, cs
        cmp ax, 0xA000
        jae interrupt.out           ; arriba: sin mensajes, se probará abajo
        push cs
        pop ds
        ; (los mensajes, de la copia de .init que hay en el fichero)
        mov si, msg_title
        add si, section..init.start - INIT_ORG
        call puts
        mov si, msg_nomem
        add si, section..init.start - INIT_ORG
        call puts
        jmp interrupt.out
.ok:
        push ds
        push es
        push cs
        pop ds
        push cs
        pop es
        mov si, init_end - 1
        add si, section..init.start - INIT_ORG
        mov di, init_end - 1
        mov cx, init_end - INIT_ORG
        std                         ; (de atrás adelante, por si se pisan)
        rep movsb
        cld
        pop es
        pop ds
        jmp init
res_end:
section .init
init_end:
; La inicialización tiene que acabar por debajo de F000h (si no, bajar
; INIT_ORG). Las llamadas y saltos entre .init y .text son relativos de 16
; bits y dan la vuelta a los 64 KB (IP se queda en 16 bits, como manda la
; norma); con 4 KB de margen, origen y destino «sin la vuelta» nunca caen en
; la misma página de 4 KB, que es cuando QEMU (10.x, con CF_PCREL) se salta la
; vuelta y sigue en CS:1xxxxh (se vio con HSPEMM: HSPUSB cargado abajo en
; 0681h). En un procesador de verdad daría igual.
        times -((init_end - init_start) > (0xF000 - INIT_ORG)) db 0
