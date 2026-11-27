; =============================================================================
;  Hispa-8086 - el núcleo (NUCLEO86.SYS)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El sistema de la familia Hispa para los PC más veteranos: 8086, 8088, V20,
;  80186 y 286, en modo real. Todo en ensamblador y solo con instrucciones del
;  8086 (el ensamblador lo vigila con «cpu 8086»), para que quepa y corra en
;  un PC o un XT de 256 K. docs/8086/HISPA-8086.md cuenta el plan entero.
;
;  Lo que hay (etapas 0 a 5):
;    - arranca desde un disquete o desde el disco duro (lo carga
;      arranque.asm), y lee CONFIG.SYS (FILES=, BUFFERS=, SHELL=);
;    - reconoce el procesador y la memoria, y si la máquina puede con un
;      Hispa mayor, lo dice;
;    - INT 21h: la consola (01h, 02h, 06h-0Bh), los vectores (25h, 35h), la
;      versión (30h), terminar (00h, 4Ch, 4Dh, INT 20h) y los ARCHIVOS: crear,
;      abrir, leer, escribir, cerrar, borrar, renombrar, buscar, directorios,
;      sitio libre, fecha y hora (archivos.inc, sobre disco.inc);
;    - la memoria por bloques (48h-4Ah) y los programas: .COM y .EXE, que
;      pueden lanzar otros (4Bh) (proceso.inc);
;    - INT 25h e INT 26h: leer y escribir sectores «en crudo», como en DOS;
;    - lanza el intérprete de órdenes, que es un programa aparte
;      (command.asm).
;    - los controladores de dispositivo de DOS (DEVICE=: config.inc y
;      dispositivos.inc), IOCTL (44h), la lista de listas (52h), INT 29h y
;      los residentes (31h, INT 27h): docs/DRIVERS-MICROSOFT.md.
;
;  Mapa de memoria: el núcleo se carga en 0070:0000 y ahí se queda, con sus
;  variables, su pila y la copia de la FAT detrás; el programa que se ejecuta
;  va justo después, hasta el final de la memoria convencional.
; =============================================================================
        cpu 8086
        bits 16
        org 0x10                    ; (ver «el puente», aquí abajo)

DOS_MAYOR   equ 3                   ; lo que contesta INT 21h/30h: el nivel
DOS_MENOR   equ 30                  ; de MS-DOS 3.30, que es el objetivo
LIN_MAX     equ 126                 ; letras de una línea de órdenes
HIST_TAM    equ 384                 ; octetos para las líneas tecleadas antes (flechas arriba y abajo)
RUTA_MAX    equ 80
DIR_MAX     equ 64                  ; letras del directorio de trabajo
PILA_TAM    equ 768
VERSION10   equ 15                  ; la versión del núcleo (0.15: el 15)
FILES_MAX   equ 64                  ; lo más que admite FILES= de CONFIG.SYS

; el procesador
CPU_8086    equ 0
CPU_V20     equ 1
CPU_186     equ 2
CPU_286     equ 3
CPU_386     equ 4
CPU_486     equ 5

; =============================================================================
;  El puente
; =============================================================================
; El núcleo se ensambla desde la dirección 10h y corre con su segmento un
; párrafo por debajo de donde está cargado: así la misma imagen vale tal cual
; en la memoria alta (FFFF:0010 es el primer octeto de más allá del mega),
; adonde se muda en un 286 con DOS=ALTO (alta.inc). Lo primero, pues, es
; pasar de 0070:0000, que es donde nos deja el sector de arranque, a
; 006F:0010, que es el mismo sitio.
entrada:
        mov ax, cs
        dec ax
        push ax
        mov ax, inicio
        push ax
        retf
; Todas las interrupciones del sistema entran por aquí, por «el puente», que
; es lo único que se queda en la memoria baja cuando el núcleo sube. Cada
; entrada se asegura de que la puerta A20 está abierta (si hace falta: en un
; 8086 es una comparación y sigue) y salta al núcleo, esté donde esté.
N_PUENTES   equ 7
%macro PUENTE 2
%1:
        call b_a20
        jmp far [cs:%2]
%endmacro
        PUENTE b_int20, b_punteros
        PUENTE b_int21, b_punteros + 4
        PUENTE b_int25, b_punteros + 8
        PUENTE b_int26, b_punteros + 12
        PUENTE b_int27, b_punteros + 16
        PUENTE b_int2f, b_punteros + 20
; INT 29h, la salida rápida de la consola (la usan los programas y los
; controladores de consola, que la cambian por la suya): con la BIOS
b_int29:
        push ax
        push bx
        push bp
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        pop bp
        pop bx
        pop ax
        iret
; el punto de entrada del gestor de memoria extendida (empieza así para que
; otro programa pueda engancharse, como manda la norma)
b_xms:
        jmp short .a
        nop
        nop
        nop
.a:
        call b_a20
        jmp far [cs:b_punteros + 24]
; la BIOS copia memoria extendida (INT 15h/87h) y al volver puede dejar A20
; cerrada: se la llama desde aquí abajo y se vuelve a abrir antes de volver
b_mueve:
        push ax                     ; (y se abre antes: hay BIOS que llevan su
        call b_esta                 ; propia cuenta de A20 y, si se cerró por
        jnz .a                      ; otro lado, no se enteran)
        call b_abre
.a:
        pop ax
        int 0x15
        pushf
        push ax
        cmp byte [cs:b_quiere], 0
        jne .q
        call b_cierra               ; nadie la quiere: como estaba
        jmp short .f
.q:
        call b_esta
        jnz .f
        call b_abre
.f:
        pop ax
        popf
        retf
; si A20 tiene que estar abierta y no lo está, la abre. Conserva todo
b_a20:
        cmp byte [cs:b_quiere], 0
        je .x
        pushf
        push ax
        call b_esta
        jnz .ok
        call b_abre
.ok:
        pop ax
        popf
.x:
        ret
; ¿está abierta A20? Se mira de verdad: con ella cerrada, FFFF:0010 es el
; mismo octeto que 0000:0000. ZF=0 si está abierta. No conserva AX
b_esta:
        push bx
        push ds
        push es
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        pushf
        cli
        mov bx, [0]
        mov ax, bx
        xor ax, [es:0x10]
        jnz .f                      ; distintos: abierta
        not word [0]                ; iguales: ¿por casualidad? Se cambia uno
        mov ax, [0]
        xor ax, [es:0x10]
        mov [0], bx
.f:
        popf
        pop es
        pop ds
        pop bx
        test ax, ax
        ret
; abre A20: por el controlador del teclado (8042), como en el AT; y si no
; basta, por el puerto 92h (PS/2 y posteriores). No conserva AX
b_abre:
        push cx
        call b_8042
        mov al, 0xD1                ; «escribir en el puerto de salida»
        out 0x64, al
        call b_8042
        mov al, 0xDF                ; con A20 abierta
        out 0x60, al
        call b_8042
        mov cx, 0x100               ; (tarda un poco)
.v:
        call b_esta
        jnz .p92
        loop .v
.p92:
        in al, 0x92                 ; el puerto 92h, si lo hay, también: que
        cmp al, 0xFF                ; los dos digan lo mismo
        je .x
        test al, 0x02
        jnz .x
        or al, 0x02
        and al, 0xFE                ; (el bit 0 reinicia la máquina)
        out 0x92, al
.x:
        pop cx
        ret
; cierra A20, por donde se abriera
b_cierra:
        push cx
        call b_8042
        mov al, 0xD1
        out 0x64, al
        call b_8042
        mov al, 0xDD
        out 0x60, al
        call b_8042
        in al, 0x92
        cmp al, 0xFF                ; (no hay tal puerto)
        je .x
        test al, 0x02
        jz .x
        and al, 0xFC
        out 0x92, al
.x:
        pop cx
        ret
; espera a que el 8042 admita otro octeto
b_8042:
        push cx
        xor cx, cx
.e:
        in al, 0x64
        test al, 0x02
        loopnz .e
        pop cx
        ret
; lo mismo, para llamarlo desde el núcleo cuando está arriba (llamadas lejanas)
b_f_esta:
        call b_esta
        retf
b_f_abre:
        push ax
        call b_abre
        pop ax
        retf
b_f_cierra:
        push ax
        call b_cierra
        pop ax
        retf
            align 2
b_punteros  dw int20, 0, int21, 0, int25, 0, int26, 0, int27, 0, int2f, 0, xms_entra, 0
b_gdt       times 48 db 0           ; la tabla que pide INT 15h/87h
b_quiere    db 0                    ; A20 tiene que estar abierta
; para el banco de pruebas: «HCFG:» y un octeto con lo que vale sin decir nada
; en CONFIG.SYS (bit 0: DOS=ALTO; bit 1: PROCESADOR=8086)
            db "HCFG:"
cfg_inicial db 0
seg_bajo    dw 0                    ; el segmento del puente (el de la memoria baja)
; Con el núcleo en la HMA, la BIOS no siempre llega por encima del mega
; (SeaBIOS, por ejemplo, no lee el disco duro en FFFF:xxxx): cada sector que
; lee o escribe el núcleo pasa por aquí abajo (disco.inc, bios_bajo)
            align 2
b_buf       times 512 db 0
bajo_fin:

inicio:
        cli
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, pila_fin
        sti
        cld
        mov di, bss_ini             ; las variables, a cero
        mov cx, fichas + N_ARCH * H_TAM_F - bss_ini
        xor al, al
        rep stosb
        mov [arr_bios], dl          ; de dónde se ha arrancado
        mov [seg_bajo], cs          ; (el del puente: el núcleo puede subir)
        mov word [buf_lba], 0xFFFF
        mov word [fat_lba], 0xFFFF
        mov byte [n_arch], N_ARCH
        mov byte [cache_mete], 1
        mov word [fecha_xt], 0x0021 ; (sin reloj: 1 de enero de 1980)
        call disp_ini
        call cpu_detecta
        int 0x12                    ; la memoria convencional, en K
        mov [mem_kb], ax
        ; los vectores del sistema: INT 20h (terminar) e INT 21h
        xor ax, ax
        mov es, ax
        mov word [es:0x20*4], b_int20
        mov [es:0x20*4+2], cs
        mov word [es:0x21*4], b_int21
        mov [es:0x21*4+2], cs
        mov word [es:0x25*4], b_int25
        mov [es:0x25*4+2], cs
        mov word [es:0x26*4], b_int26
        mov [es:0x26*4+2], cs
        mov word [es:0x27*4], b_int27
        mov [es:0x27*4+2], cs
        mov word [es:0x29*4], b_int29
        mov [es:0x29*4+2], cs
        mov bx, b_punteros          ; el puente: de momento, el núcleo está aquí
        mov cx, N_PUENTES
.pt:
        mov [bx+2], cs
        add bx, 4
        loop .pt
        mov [bajo_seg], cs
        mov word [p_esta], b_f_esta
        mov [p_esta+2], cs
        mov word [p_abre], b_f_abre
        mov [p_abre+2], cs
        mov word [p_cierra], b_f_cierra
        mov [p_cierra+2], cs
        mov word [p_mueve], b_mueve
        mov [p_mueve+2], cs
        mov al, [cfg_inicial]
        mov ah, al
        and al, 1
        mov [dos_alto], al
        shr ah, 1
        and ah, 1
        mov [como_8086], ah
        push cs
        pop es
        mov [dta_seg], cs           ; las búsquedas, por ahora, a lo nuestro
        mov word [dta_off], dta_int
        ; las unidades. Arrancando de un disquete, ese es A: y la unidad de
        ; trabajo; arrancando del disco duro, A: es la primera disquetera y
        ; se trabaja en C:
        mov al, [arr_bios]
        test al, al
        js .duro
        mov [u_bios], al
        call disco_ini
        call unidades_ini
        jmp short .unidades
.duro:
        call unidades_ini
        cmp byte [u_hay+2], 0
        je .unidades
        mov al, 2
        mov [unidad_def], al
        call unidad_pon
        mov byte [ent_nucleo.u1], 'C'   ; (PATH y COMSPEC, en C:)
        mov byte [ent_nucleo.u2], 'C'
.unidades:
        mov si, m_titulo
        call pon_cad
        call config_lee             ; CONFIG.SYS: FILES=, BUFFERS=, DEVICE=...
        ; dónde va el programa: detrás de todo lo del núcleo, que acaba en la
        ; tabla de archivos abiertos, la reserva de sectores y los
        ; controladores cargados (config_lee deja dicho dónde)
        mov ax, [cf_tope]
        mov [mem_seg], ax           ; ahí empieza la memoria que se reparte
        mov [sv_mcb], ax
        call mem_ini
        mov byte [primera], 1

; lanza el intérprete de órdenes (\COMMAND.COM); al arrancar, con «/P». Si
; termina, se lanza otra vez; si no está, no hay nada que hacer
interprete:
        cli
        mov ax, cs
        mov ss, ax
        mov sp, pila_fin
        mov ds, ax
        mov es, ax
        sti
        cld
        mov word [psp_act], 0
        mov [dta_seg], cs
        mov word [dta_off], dta_int
        mov si, t_command
        mov word [lz_cola], cola_p
        cmp byte [sh_ruta], 0       ; ¿otro intérprete (SHELL= de CONFIG.SYS)?
        je .cual
        mov si, sh_ruta
        mov word [lz_cola], sh_cola
.cual:
        mov di, ruta
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        cmp byte [primera], 0
        jne .l
        mov word [lz_cola], cola_nada
.l:
        mov byte [primera], 0
        mov [lz_cola+2], cs
        mov word [lz_ent], 0
        call lanza                  ; (si vuelve, no ha podido)
        cmp byte [sh_ruta], 0       ; si era el de SHELL=, se dice y se prueba
        je .sin                     ; con COMMAND.COM
        mov byte [sh_ruta], 0
        mov si, m_sin_shell
        call pon_cad
        mov byte [primera], 1
        jmp interprete
.sin:
        mov si, m_sin_command
        call pon_cad
.para:
        hlt
        jmp .para
t_command   db "\COMMAND.COM", 0
cola_p      db 3, " /P", 13
cola_nada   db 0, 13

; =============================================================================
;  INT 20h e INT 21h
; =============================================================================
int20:
        xor al, al
        jmp termina

; la pila, dentro de INT 21h, con todos los registros de quien llama
R_ES        equ 0
R_DS        equ 2
R_BP        equ 4
R_DI        equ 6
R_SI        equ 8
R_DX        equ 10
R_CX        equ 12
R_BX        equ 14
R_AX        equ 16
R_BAND      equ 22                  ; sus banderas

int21:
        sti
        cmp ah, 0x4C
        je termina
        test ah, ah
        jz int20
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push ds
        push es
        mov bp, sp
        push cs
        pop ds
        push cs
        pop es
        cld
        and byte [bp+R_BAND], 0xFE  ; sin error, mientras no se diga
        mov bx, f21                 ; la tabla: número de función y rutina
.b:
        cmp byte [bx], 0
        je .mal
        cmp ah, [bx]
        je .esta
        add bx, 3
        jmp .b
.esta:
        call [bx+1]
        jmp short .fin
.mal:
        mov ax, E_FUNCION           ; no existe (todavía)
        call f_error
.fin:
        pop es
        pop ds
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        iret

%macro F21 2
        db %1
        dw %2
%endmacro
f21:
        F21 0x01, f01
        F21 0x02, f02
        F21 0x06, f06
        F21 0x07, f07
        F21 0x08, f07
        F21 0x09, f09
        F21 0x0A, f0A
        F21 0x0B, f0B
        F21 0x0D, f0D
        F21 0x0E, f0E
        F21 0x19, f19
        F21 0x1A, f1A
        F21 0x25, f25
        F21 0x2A, f2A
        F21 0x2B, f2B
        F21 0x2C, f2C
        F21 0x2D, f2D
        F21 0x2E, f2E
        F21 0x2F, f2F
        F21 0x30, f30
        F21 0x31, f31
        F21 0x35, f35
        F21 0x36, f36
        F21 0x39, f39
        F21 0x3A, f3A
        F21 0x3B, f3B
        F21 0x3C, f3C
        F21 0x3D, f3D
        F21 0x3E, f3E
        F21 0x3F, f3F
        F21 0x40, f40
        F21 0x41, f41
        F21 0x42, f42
        F21 0x43, f43
        F21 0x44, f44
        F21 0x46, f46
        F21 0x47, f47
        F21 0x48, f48
        F21 0x49, f49
        F21 0x4A, f4A
        F21 0x4B, f4B
        F21 0x4D, f4D
        F21 0x4E, f4E
        F21 0x4F, f4F
        F21 0x50, f50
        F21 0x51, f51
        F21 0x52, f52
        F21 0x54, f54
        F21 0x56, f56
        F21 0x57, f57
        F21 0x62, f51
        F21 0xE8, fE8
        db 0

; 01h: una tecla, con eco -> AL
f01:
        call tecla
        mov [bp+R_AX], al
        jmp pon_car_dos
; 02h: escribe DL
f02:
        mov al, [bp+R_DX]
        jmp pon_car_dos
; 06h: DL = el carácter que se escribe; con DL = FFh, la tecla que haya
; (AL, y ZF=0) o ninguna (ZF=1)
f06:
        mov al, [bp+R_DX]
        cmp al, 0xFF
        jne pon_car_dos
        call hay_tecla
        jnz .hay
        or byte [bp+R_BAND], 0x40
        mov byte [bp+R_AX], 0
        ret
.hay:
        and byte [bp+R_BAND], 0xBF
; 07h y 08h: una tecla, sin eco -> AL
f07:
        call tecla
        mov [bp+R_AX], al
        ret
; 09h: escribe el texto de DS:DX, que acaba en «$»
f09:
        mov si, [bp+R_DX]
        mov es, [bp+R_DS]
.c:
        mov al, [es:si]
        cmp al, '$'
        je .x
        call pon_car_dos
        inc si
        jmp .c
.x:
        ret
; 0Ah: lee una línea en el búfer de DS:DX (el primer octeto, cuánto cabe; en
; el segundo se deja lo leído, sin contar el retorno del final)
f0A:
        mov es, [bp+R_DS]
        mov bx, [bp+R_DX]
        mov cl, [es:bx]
        xor ch, ch
        jcxz .x
        dec cx
        lea di, [bx+2]
        call lee_linea              ; ES:DI, CX letras -> CX = las leídas
        mov [es:bx+1], cl
        add di, cx
        mov byte [es:di], 0x0D
        mov al, 0x0D
        call pon_car_dos
.x:
        ret
; 0Bh: ¿hay una tecla? -> AL = FFh o 0
f0B:
        mov al, 0xFF
        cmp word [red_ent], 0
        jne .x
        call hay_tecla
        mov al, 0
        jz .x
        dec al
.x:
        mov [bp+R_AX], al
        ret
; 0Eh: la unidad de trabajo pasa a ser DL (0 = A:), si existe -> AL = cuántas
; letras de unidad hay
f0E:
        mov bl, [bp+R_DX]
        cmp bl, N_UNI
        jae .x
        xor bh, bh
        cmp byte [u_hay+bx], 0
        je .x
        mov [unidad_def], bl
.x:
        mov byte [bp+R_AX], N_UNI
        ret
; 19h: la unidad de trabajo -> AL (0 = A:)
f19:
        mov al, [unidad_def]
        mov [bp+R_AX], al
        ret
; 25h: pone el vector AL en DS:DX
f25:
        mov bl, [bp+R_AX]
        xor bh, bh
        shl bx, 1
        shl bx, 1
        xor ax, ax
        mov es, ax
        cli
        mov ax, [bp+R_DX]
        mov [es:bx], ax
        mov ax, [bp+R_DS]
        mov [es:bx+2], ax
        sti
        ret
; 30h: la versión -> AL.AH (y BX = CX = 0)
f30:
        mov word [bp+R_AX], DOS_MAYOR + DOS_MENOR * 256
        mov word [bp+R_BX], 0
        mov word [bp+R_CX], 0
        ret
; 35h: el vector AL -> ES:BX
f35:
        mov bl, [bp+R_AX]
        xor bh, bh
        shl bx, 1
        shl bx, 1
        xor ax, ax
        mov es, ax
        mov ax, [es:bx]
        mov [bp+R_BX], ax
        mov ax, [es:bx+2]
        mov [bp+R_ES], ax
        ret
; E8h (propia de Hispa-8086), según AL:
;   0  AX = 4869h («Hi»), BL = el procesador (CPU_*), BH = la versión del
;      núcleo por diez
;   1  los discos han cambiado por debajo (un programa los ha formateado o ha
;      tocado las particiones con la BIOS): el núcleo olvida lo que sabía de
;      ellos y vuelve a mirar el disco duro
;   2  cuántos archivos hay abiertos en la unidad DL (0 = A:) -> AX
;   3  con una sola disquetera: el disquete que hay puesto es el de la unidad
;      DL (lo dicen FORMATEA y SISTEMA, que lo han pedido ellas)
;   8  la línea que se lea a continuación del teclado empieza con el texto de
;      DS:DX, acabado en 0 (EDIT, en BASIC) -> AX = 4564h; CF=1 si la entrada
;      o la salida están redirigidas
;   4  cuántos sectores se han leído (AX) y escrito (DX) de verdad desde el
;      arranque: para medir lo que ahorra la reserva
;   5  las medidas de la unidad DL, en ES:DI (nueve palabras: sectores por
;      grupo, reservados, cuántas FAT, sectores de cada una, primer sector de
;      la raíz, cuántos tiene, primer sector de los datos, último grupo, y 1
;      si es FAT16). CF=1 si la unidad no está preparada
fE8:
        mov al, [bp+R_AX]
        cmp al, 1
        jne .n1
        jmp discos_nuevos
.n1:
        cmp al, 2
        je .abiertos
        cmp al, 3
        jne .n3
        mov al, [bp+R_DX]
        cmp al, 1
        ja .x
        mov [disq_puesto], al
.x:
        ret
.n3:
        cmp al, 4
        jne .n4
        mov ax, [n_lect]
        mov [bp+R_AX], ax
        mov ax, [n_escr]
        mov [bp+R_DX], ax
        ret
.n4:
        cmp al, 5
        jne .n5
        mov al, [bp+R_DX]
        call unidad_pon
        jc .e5
        call disco_fresco
        jc .e5
        mov es, [bp+R_ES]
        mov di, [bp+R_DI]
        mov ax, [spc]
        stosw
        mov ax, [res_sec]
        stosw
        mov ax, [nfat]
        stosw
        mov ax, [spf]
        stosw
        mov ax, [raiz_ini]
        stosw
        mov ax, [raiz_sec]
        stosw
        mov ax, [datos_ini]
        stosw
        mov ax, [max_clus]
        stosw
        mov al, [u_fat16]
        xor ah, ah
        stosw
        ret
.e5:
        jmp f_error
.n5:
        cmp al, 6                   ; 6: la memoria de más allá del mega ->
        jne .n6                     ; AX: bit 0 hay gestor, bit 1 el núcleo
        mov al, [xms_hay]           ; está en la HMA, bit 2 la tiene un
        mov ah, [alto]              ; programa; BX = K de memoria extendida
        shl ah, 1
        or al, ah
        mov ah, [hma_dueno]
        shl ah, 1
        shl ah, 1
        or al, ah
        xor ah, ah
        mov [bp+R_AX], ax
        mov ax, [ext_k]
        mov [bp+R_BX], ax
        ret
.n6:
        cmp al, 7                   ; 7: el último error de disco de la BIOS
        jne .n7                     ; -> AL = su código, AH = lo que se pedía
        mov ax, [err_bios]          ; (2 leer, 3 escribir)
        mov [bp+R_AX], ax
        ret
.n7:
        cmp al, 8                   ; 8: la línea que se lea a continuación
        jne .n8                     ; del teclado empieza con el texto de
        mov ax, [red_ent]           ; DS:DX (acabado en 0), para corregirlo
        or ax, [red_sal]            ; -> AX = 4564h («dE»). Con la entrada o
        jnz f_error                 ; la salida redirigidas, CF=1
        mov ax, [bp+R_DX]
        mov [ed_pre], ax
        mov ax, [bp+R_DS]
        mov [ed_pre+2], ax
        mov word [bp+R_AX], 0x4564
        ret
.n8:
        mov word [bp+R_AX], 0x4869
        mov bl, [cpu_tipo]
        mov bh, VERSION10
        mov [bp+R_BX], bx
        ret
.abiertos:
        mov dl, [bp+R_DX]
        xor ax, ax
        mov bx, fichas
        mov cl, [n_arch]
        xor ch, ch
.a:
        cmp byte [bx+H_USO], 0
        je .a1
        cmp [bx+H_UNI], dl
        jne .a1
        inc ax
.a1:
        add bx, H_TAM_F
        loop .a
        mov [bp+R_AX], ax
        ret
; 46h: redirección. El archivo BX pasa a ser la entrada (CX=0) o la salida
; (CX=1) de la consola, para todos los programas; con BX menor que 5 (la
; consola), la redirección se quita
f46:
        mov si, [bp+R_CX]
        cmp si, 1
        ja .mal
        shl si, 1
        mov ax, [bp+R_BX]
        cmp ax, PRIMERO
        jb .quita
        call h_ficha
        jc f_error
        mov [red_ent+si], bx
        ret
.quita:
        mov word [red_ent+si], 0
        ret
.mal:
        mov ax, E_NUMERO
        jmp f_error
; 31h: terminar y quedarse en la memoria (los programas residentes): el
; programa se queda con DX párrafos y sus archivos siguen abiertos. INT 27h
; es lo mismo, a la antigua: DX = el primer octeto que ya no hace falta
f31:
        mov ax, [psp_act]
        test ax, ax
        jz .x
        mov bx, [bp+R_DX]
        cmp bx, 0x11                ; (por lo menos, su prefijo)
        jae .t
        mov bx, 0x11
.t:
        call mem_cambia
        mov byte [queda], 1
        mov al, [bp+R_AX]
        jmp termina
.x:
        ret
int27:
        add dx, 15
        mov cl, 4
        shr dx, cl
        mov ax, 0x3100
        int 0x21
; 2Eh: VERIFICAR (AL=1: lo que se escribe en el disco se repasa); 54h: cómo
; está -> AL
f2E:
        mov al, [bp+R_AX]
        and al, 1
        mov [verificar], al
        ret
f54:
        mov al, [verificar]
        mov [bp+R_AX], al
        ret
; 44h, IOCTL, según AL (como en MS-DOS 3.3):
;   0     qué hay detrás del número de archivo BX -> DX (bit 7: un
;         dispositivo; entonces, bits 0 a 3 los de sus atributos: consola,
;         NUL, reloj; bit 14, que entiende IOCTL)
;   1     ponerlo (se toma nota); 0Bh, los reintentos (también)
;   2, 3  leer o escribir CX octetos de control de DS:DX en el dispositivo
;         del archivo BX (órdenes 3 y 12) -> AX = cuántos
;   4, 5  lo mismo, en el controlador de la unidad BL (0: la de trabajo)
;   6, 7  ¿está listo para leer o escribir el archivo BX? -> AL = FFh o 0
;   8     ¿se puede sacar el disco de la unidad BL? -> AX = 0 sí, 1 es fijo
;   9     ¿es de red la unidad BL? -> DX (bit 12 a 0: no); 0Ah, el archivo
;   0Dh   la IOCTL genérica de la unidad BL (CH categoría, CL función,
;         DS:DX los datos; orden 19), si su controlador la entiende
;   0Eh, 0Fh  qué letra tiene la unidad BL -> AL = 0 (solo una)
; Lo de los controladores se les pide a ellos, si dicen que lo entienden
; (t44); a los discos de la BIOS, solo el 8
f44:
        mov cl, [bp+R_AX]
        cmp cl, 1
        je .x
        cmp cl, 0x0B
        je .x
        cmp cl, 0x0A
        je .cero
        cmp cl, 0x0E
        jb .n
        cmp cl, 0x0F
        ja .no
        mov byte [bp+R_AX], 0
.x:
        ret
.cero:
        mov word [bp+R_DX], 0
        ret
.no:
        mov ax, E_FUNCION
.err:
        jmp f_error
.n:
        cmp cl, 0x0C
        je .no
        cmp cl, 4
        jb .h
        cmp cl, 6
        jb .u
        cmp cl, 8
        jb .h
.u:
        mov al, [bp+R_BX]           ; ---- por unidad ----
        dec al
        jns .u1
        mov al, [unidad_def]
.u1:
        call unidad_pon
        jc .err
        mov al, [u_sub]
        mov [pet+1], al
        mov ax, [u_dev]
        mov dx, [u_dev+2]
        cmp cl, 9
        jne .u8
        xor bx, bx                  ; 9: sus atributos, o 0
        test dx, dx
        jz .u9
        mov es, dx
        mov si, ax
        mov bx, [es:si+D_ATR]
        and bh, 0xEF
.u9:
        mov [bp+R_DX], bx
        ret
.u8:
        test dx, dx
        jnz .pide
        cmp cl, 8                   ; de la BIOS: solo el 8
        jne .no
        mov ax, 1
        cmp byte [u_bios], 0x80
        sbb ax, 0
        mov [bp+R_AX], ax
        ret
.h:
        mov ax, [bp+R_BX]           ; ---- por número de archivo ----
        cmp ax, PRIMERO
        jae .hf
        mov dx, 0x80D3              ; la consola
        test cl, cl
        jz .dx
        mov ax, [sv_con]
        mov dx, [sv_con+2]
        jmp short .pide
.hf:
        call h_ficha
        jc .err
        cmp byte [bx+H_USO], 2
        je .hd
        mov dl, [bx+H_UNI]          ; un archivo: 0, su unidad
        xor dh, dh
        test cl, cl
        jz .dx
        cmp cl, 6
        jb .no
        mov ax, [bx+H_POS]          ; 6: ¿queda algo por leer? 7: sí
        mov dx, [bx+H_POS+2]
        sub ax, [bx+H_TAM]
        sbb dx, [bx+H_TAM+2]
        sbb al, al
        cmp cl, 7
        jne .al
        mov al, 0xFF
.al:
        mov [bp+R_AX], al
        ret
.hd:
        mov ax, [bx+H_TAM]          ; un dispositivo
        mov dx, [bx+H_TAM+2]
        test cl, cl
        jnz .pide
        mov es, dx
        mov si, ax
        mov dx, [es:si+D_ATR]
        and dx, 0x680F
        or dx, 0x80C0
.dx:
        mov [bp+R_DX], dx
        ret
.pide:
        ; CL = la subfunción, DX:AX = el controlador: t44 dice qué orden se
        ; le manda y qué atributo tiene que tener para entenderla
        mov bl, cl
        cmp bl, 0x0D
        jne .p1
        mov bl, 9
.p1:
        sub bl, 2
        xor bh, bh
        mov di, bx
        shl bx, 1
        add bx, di
        add bx, t44
        mov es, dx
        mov si, ax
        mov di, [bx+1]
        test [es:si+D_ATR], di
        jnz .vale
        test di, di
        jnz .no2
.vale:
        push cs
        pop es
        mov ch, [bx]
        mov [pet+P_ORDEN], ch
        mov bx, [bp+R_DX]
        mov [pet+P_DONDE], bx
        mov bx, [bp+R_DS]
        mov [pet+P_DONDE+2], bx
        mov bx, [bp+R_CX]
        mov [pet+P_CUANTOS], bx
        cmp cl, 0x0D
        jne .ll
        xchg bl, bh                 ; la genérica: 13 categoría, 14 función,
        mov [pet+13], bx            ; 15 SI, 17 DI, 19 los datos
        mov bx, [bp+R_SI]
        mov [pet+15], bx
        mov bx, [bp+R_DI]
        mov [pet+17], bx
        mov bx, [bp+R_DX]
        mov [pet+19], bx
        mov bx, [bp+R_DS]
        mov [pet+21], bx
.ll:
        call dev_llama
        mov bx, [pet+P_ESTADO]
        test bh, 0x80
        jz .bien
        mov al, bl                  ; (el error del controlador, como en DOS:
        xor ah, ah                  ; más 13h)
        add al, 0x13
        jmp f_error
.no2:
        jmp .no
.bien:
        mov ax, [pet+P_CUANTOS]     ; 2 a 5: cuántos
        cmp cl, 6
        jb .a
        cmp cl, 8
        ja .x2
        mov al, bh                  ; 6 a 8: el bit de «ocupado»
        and al, 2
        shr al, 1
        cmp cl, 8                   ; 8: ocupado es fijo (1)
        je .ocho
        dec al                      ; 6 y 7: ocupado es «no» (0)
.ocho:
        xor ah, ah
.a:
        mov [bp+R_AX], ax
.x2:
        ret
; la orden y el atributo que hace falta, de las subfunciones 2 a 8 y 0Dh
t44     db 3
        dw 0x4000
        db 12
        dw 0x4000
        db 3
        dw 0x4000
        db 12
        dw 0x4000
        db 6
        dw 0
        db 10
        dw 0
        db 15
        dw 0x0800
        db 19
        dw 0x0040
; 52h: la «lista de listas» -> ES:BX (dispositivos.inc, sysvars)
f52:
        mov word [bp+R_BX], sysvars
        mov [bp+R_ES], cs
        ret
; 4Dh: el código con que terminó el último programa -> AL (AH = 0)
f4D:
        mov al, [cod_salida]
        xor ah, ah
        mov [bp+R_AX], ax
        ret

; =============================================================================
;  La consola (con la BIOS: funciona con cualquier tarjeta de vídeo)
; =============================================================================
; escribe AL tal cual (lo que mandan los programas). Si la salida está
; redirigida a un archivo ([red_sal]), va a él. Conserva los registros
pon_car_dos:
        cmp byte [con_directo], 0   ; (el dispositivo CON: siempre la pantalla)
        jne .pantalla
        cmp word [red_sal], 0
        je .pantalla
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push es
        push cs
        pop es
        mov [red_car], al
        mov [es_seg], cs
        mov word [es_off], red_car
        mov byte [es_op], 1
        mov bx, [red_sal]
        mov cx, 1
        call h_es
        pop es
        jmp sal_di
.pantalla:
        cmp word [cs:con_dev+2], 0  ; ¿un CON cargado con DEVICE=? A él
        je .bios
        push ax
        mov [cs:con_car], al
        mov al, 8
        call con_pide
        pop ax
        ret
.bios:
        int 0x29                    ; (la salida rápida: b_int29, la BIOS)
        ret
; escribe AL; el salto de línea (10) baja y vuelve al margen: así van los
; textos del núcleo. Conserva los registros
pon_car:
        cmp al, 10
        jne pon_car_dos
        push ax
        mov al, 13
        call pon_car_dos
        mov al, 10
        call pon_car_dos
        pop ax
        ret
pon_nl:
        push ax
        mov al, 10
        call pon_car
        pop ax
        ret
; escribe el texto de DS:SI, que acaba en 0
pon_cad:
        push ax
        push si
.c:
        lodsb
        test al, al
        jz .x
        call pon_car
        jmp .c
.x:
        pop si
        pop ax
        ret
; escribe AX en decimal
pon_num:
        push dx
        xor dx, dx
        call pon_num32
        pop dx
        ret
; escribe DX:AX en decimal (pon_num32_ancho: arrimado a la derecha en CX)
pon_num32:
        push cx
        xor cx, cx
        call pon_num32_ancho
        pop cx
        ret
pon_num32_ancho:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov di, cx                  ; el ancho
        mov bx, 10
        xor cx, cx                  ; cuántas cifras
.d:
        push ax                     ; DX:AX entre 10, en dos pasos de 16 bits
        mov ax, dx
        xor dx, dx
        div bx
        mov si, ax                  ; la mitad alta del cociente
        pop ax
        div bx                      ; AX = la baja, DX = la cifra
        push dx
        inc cx
        mov dx, si
        mov si, ax
        or si, dx
        jnz .d
        mov si, cx
.esp:
        cmp si, di                  ; los espacios de delante
        jae .c
        mov al, ' '
        call pon_car
        inc si
        jmp .esp
.c:
        pop ax
        add al, '0'
        call pon_car
        loop .c
; las salidas que sacan DI, SI, DX, CX, BX y AX (o los últimos) y vuelven:
; todas aquí («jmp sal_..» en vez de los pop y el ret)
sal_di:
        pop di
sal_si:
        pop si
sal_dx:
        pop dx
sal_cx:
        pop cx
sal_bx:
        pop bx
sal_ax:
        pop ax
        ret
; espera una tecla -> AL. Las que no son letra (las flechas, Inicio, las de
; función...) llegan en dos veces, como en DOS: primero un 0 y, a la lectura
; siguiente, el código de la tecla. Si la entrada está redirigida ([red_ent]),
; es el octeto siguiente del archivo; cuando se acaba, vuelve el teclado
tecla:
        cmp byte [con_directo], 0   ; (el dispositivo CON: siempre el teclado)
        jne .teclado
        cmp word [red_ent], 0
        je .teclado
        push bx
        push cx
        push dx
        push si
        push di
        push es
        push cs
        pop es
        mov [es_seg], cs
        mov word [es_off], red_car
        mov byte [es_op], 0
        mov bx, [red_ent]
        mov cx, 1
        call h_es
        pop es
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        jc .fin
        test ax, ax
        jz .fin
        mov al, [red_car]
        cmp al, 10                  ; (de «retorno y salto», solo el retorno)
        je tecla
        ret
.fin:
        mov word [red_ent], 0
.teclado:
        cmp word [con_dev+2], 0     ; ¿un CON cargado con DEVICE=? De él
        je .bios
        mov al, 4
        jmp con_pide
.bios:
        xor al, al                  ; ¿queda por dar el código de una tecla
        xchg al, [tecla_cod]        ; de las de dos veces?
        test al, al
        jnz .x
        xor ah, ah
        int 0x16
        call tecla_es_ext
        jne .x
        mov [tecla_cod], ah         ; el código, para la lectura siguiente
        xor al, al
.x:
        ret
; ¿es AX (lo que da INT 16h) una tecla de las que no son letra? ZF=1 si lo es
; (su código está en AH). Lo son las que traen AL=0 y, en los teclados de 101
; teclas, las del bloque gris, que según la BIOS y la función pueden traer
; AL=E0h (con AH=0, E0h es una letra: la que se teclea con Alt y el teclado
; numérico)
tecla_es_ext:
        test al, al
        jz .x
        cmp al, 0xE0
        jne .x
        test ah, ah
        jz .no
        cmp al, al
        ret
.no:
        test al, al                 ; (AL = E0h: ZF=0)
.x:
        ret

; lee una línea en ES:DI, de CX letras como mucho, hasta Intro. Deja un 0 al
; final y CX = las letras leídas. Conserva lo demás.
; Del teclado y a la pantalla, la línea se edita (ed_linea, más abajo): las
; flechas, Inicio, Fin, Supr, Insert, Esc y las líneas de antes. Con la
; entrada o la salida redirigidas no hay cursor que mover: solo Retroceso
lee_linea:
        mov byte [tecla_cod], 0
        cmp byte [con_directo], 0
        jne ed_linea
        push ax
        mov ax, [red_ent]
        or ax, [red_sal]
        pop ax
        jz ed_linea
        mov word [ed_pre+2], 0      ; (y sin texto puesto de antemano)
        push ax
        push bx
        push dx
        mov dx, cx                  ; lo que cabe
        xor bx, bx                  ; lo que hay
.t:
        call tecla
        test al, al                 ; (una flecha: no cuenta, ni su código)
        jnz .t0
        mov byte [tecla_cod], 0
        jmp .t
.t0:
        cmp al, 13
        je .fin
        cmp al, 8
        je .borra
        cmp al, 0x1A                ; (Ctrl+Z: el «fin de archivo» del teclado)
        je .vale
        cmp al, ' '
        jb .t
.vale:
        cmp bx, dx
        jae .t
        mov [es:di+bx], al
        inc bx
        call pon_car_dos
        jmp .t
.borra:
        test bx, bx
        jz .t
        dec bx
        mov al, 8
        call pon_car_dos
        mov al, ' '
        call pon_car_dos
        mov al, 8
        call pon_car_dos
        jmp .t
.fin:
        mov byte [es:di+bx], 0
        mov cx, bx
        pop dx
        pop bx
        pop ax
        ret

; =============================================================================
;  La línea que se teclea, con sus teclas de cursor
;
;       izquierda, derecha      mueven el cursor por la línea
;       Inicio, Fin             al principio y al final
;       Supr, Retroceso         borran la letra del cursor y la de delante
;       Insert                  intercalar (lo normal) o escribir encima
;       arriba, abajo           las líneas tecleadas antes (HIST_TAM octetos)
;       F3                      lo que falta de la última línea, como en DOS
;       Esc                     borra la línea
;
;  Sirve a todo el que lee una línea con el sistema: el intérprete, BASIC (su
;  programa, INPUT), COPY CON... Valen las teclas grises del teclado de 101
;  teclas y las del teclado numérico (sin BloqNum), que son las únicas del de
;  83. La pantalla se maneja con la BIOS (INT 10h): se apunta dónde empieza
;  la línea y, de ahí, sale el sitio de cada letra, aunque la línea ocupe más
;  de un renglón o haga subir la pantalla.
; =============================================================================
ed_linea:
        push ax
        push bx
        push dx
        push si
        cld
        mov [ed_max], cx
        xor ax, ax
        mov [ed_n], ax
        mov [ed_pos], ax
        mov [ed_sobra], ax
        mov [ed_sob], al
        mov ax, [hist_fin]
        mov [ed_h], ax
        mov ah, 3                   ; dónde empieza la línea
        xor bh, bh
        call ed_v10
        mov [ed_org], dx
        ; ¿hay un texto puesto de antemano (INT 21h/E808h)? Es la línea
        xor bx, bx
        xor cx, cx
        xchg cx, [ed_pre+2]
        jcxz .t
        mov si, [ed_pre]
        mov dx, [ed_max]
        push ds
        mov ds, cx
.pre:
        lodsb
        test al, al
        jz .pre_f
        cmp bx, dx
        jae .pre_f
        mov [es:di+bx], al
        inc bx
        jmp .pre
.pre_f:
        pop ds
        jmp .puesta
; ---- (los saltos largos de las teclas) ----
.fin_j:
        jmp .fin
.arriba_j:
        jmp .arriba
.abajo_j:
        jmp .abajo
.f3_j:
        jmp .f3
.t:
        mov ax, [ed_pos]
        call ed_cursor
        xor ah, ah
        int 0x16
        mov bx, [ed_pos]
        call tecla_es_ext
        je .ext
        cmp al, 13
        je .fin_j
        cmp al, 8
        je .borra
        cmp al, 27
        je .esc
        cmp al, 0x1A                ; (Ctrl+Z: el «fin de archivo» del teclado)
        je .vale
        cmp al, ' '
        jb .t
.vale:
        mov cx, [ed_n]
        cmp byte [ed_sob], 0        ; ¿encima de la que hay?
        je .mete
        cmp bx, cx
        jb .pon
.mete:
        cmp cx, [ed_max]
        jae .t
        push bx                     ; hueco: las de detrás, un sitio más allá
        sub cx, bx
        mov bx, [ed_n]
        jcxz .m2
.m1:
        mov dl, [es:di+bx-1]
        mov [es:di+bx], dl
        dec bx
        loop .m1
.m2:
        pop bx
        inc word [ed_n]
.pon:
        mov [es:di+bx], al
        mov ax, bx
        call ed_pinta
        inc word [ed_pos]
        jmp .t
.borra:
        test bx, bx
        jz .t
        dec bx
        mov [ed_pos], bx
.supr:
        mov cx, [ed_n]              ; fuera la letra BX: las de detrás, un
        sub cx, bx                  ; sitio más acá
        jbe .t
        dec cx
        push bx
        jcxz .s2
.s1:
        mov al, [es:di+bx+1]
        mov [es:di+bx], al
        inc bx
        loop .s1
.s2:
        pop bx
        dec word [ed_n]
        mov word [ed_sobra], 1
        mov ax, bx
        call ed_pinta
        jmp .t
.esc:
        mov ax, [hist_fin]
        mov [ed_h], ax
        xor bx, bx
.puesta:
        ; la línea pasa a tener BX letras (las que hay ya en ES:DI): se pinta
        ; entera, se borra lo que sobre de la de antes y el cursor va al final
        mov cx, [ed_n]
        mov [ed_n], bx
        mov [ed_pos], bx
        sub cx, bx
        jbe .p1
        mov [ed_sobra], cx
.p1:
        xor ax, ax
        call ed_pinta
.t_j:
        jmp .t
.ext:
        mov al, ah
        cmp al, 0x4B                ; izquierda
        jne .e1
        test bx, bx
        jz .t_j
        dec word [ed_pos]
        jmp .t
.e1:
        cmp al, 0x4D                ; derecha
        jne .e2
        cmp bx, [ed_n]
        jae .t_j
        inc word [ed_pos]
        jmp .t
.e2:
        cmp al, 0x47                ; Inicio
        jne .e3
        mov word [ed_pos], 0
        jmp .t
.e3:
        cmp al, 0x4F                ; Fin
        jne .e4
        mov ax, [ed_n]
        mov [ed_pos], ax
        jmp .t
.e4:
        cmp al, 0x53                ; Supr
        je .supr
        cmp al, 0x48                ; arriba
        je .arriba_j
        cmp al, 0x50                ; abajo
        je .abajo_j
        cmp al, 0x3D                ; F3
        je .f3_j
        cmp al, 0x52                ; Insert
        jne .t_j
        xor byte [ed_sob], 1
        jmp .t
.puesta_j:
        jmp .puesta
.arriba:
        mov si, [ed_h]              ; la línea de antes de la que se ve
        test si, si
        jz .t_j
        call ed_h_antes
        jmp short .trae
.abajo:
        mov si, [ed_h]              ; la de después (tras la última, nada)
        cmp si, [hist_fin]
        jae .t_j
.b1:
        inc si
        cmp byte [hist+si-1], 0
        jne .b1
.trae:
        mov [ed_h], si
        xor bx, bx
.copia:
        ; del historial (desde SI, hasta su 0) a la línea, de la letra BX en
        ; adelante
        cmp si, [hist_fin]
        jae .puesta_j
        mov al, [hist+si]
        test al, al
        jz .puesta_j
        cmp bx, [ed_max]
        jae .puesta_j
        mov [es:di+bx], al
        inc si
        inc bx
        jmp .copia
.f3:
        mov si, [hist_fin]          ; de la última línea, lo de más allá del
        test si, si                 ; cursor
        jz .f3_no
        call ed_h_antes
        mov cx, bx
        jcxz .copia
.f1:
        cmp byte [hist+si], 0
        je .f3_no
        inc si
        loop .f1
        jmp .copia
.f3_no:
        jmp .t
.fin:
        mov ax, [ed_n]              ; el cursor, al final (lo que se escriba
        call ed_cursor              ; después va detrás de la línea)
        call ed_guarda
        mov bx, [ed_n]
        mov byte [es:di+bx], 0
        mov cx, bx
        pop si
        pop dx
        pop bx
        pop ax
        ret

; INT 10h, conservando BP, SI y DI (hay BIOS que los estropean al subir la
; pantalla)
ed_v10:
        push bp
        push si
        push di
        int 0x10
        pop di
        pop si
        pop bp
        ret
; el ancho de la pantalla -> CX (lo dice la BIOS en 40:4Ah)
ed_ancho:
        push ds
        mov cx, 0x40
        mov ds, cx
        mov cx, [0x4A]
        pop ds
        test cx, cx
        jnz .x
        mov cx, 80
.x:
        ret
; pone el cursor en la letra AX de la línea. Conserva los registros
ed_cursor:
        push ax
        push bx
        push cx
        push dx
        call ed_ancho
        mov bx, [ed_org]            ; BH = la fila, BL = la columna
        mov dl, bl
        xor dh, dh
        add ax, dx
        xor dx, dx
        div cx                      ; AX = renglones más abajo, DX = columna
        add al, bh
        mov dh, al
        xor bh, bh
        mov ah, 2
        call ed_v10
        jmp sal_dx
; pinta la línea desde la letra AX, y [ed_sobra] espacios detrás (para borrar
; lo que quede de antes). Si con eso sube la pantalla, corrige dónde empieza
; la línea. Deja el cursor detrás de lo pintado. Conserva los registros
ed_pinta:
        push ax
        push bx
        push cx
        push dx
        push si
        call ed_cursor
        mov si, ax
.c:
        cmp si, [ed_n]
        jae .resto
        mov bx, si
        mov al, [es:di+bx]
        call .pon
        inc si
        jmp .c
.resto:
        mov cx, [ed_sobra]
        jcxz .ya
.r:
        mov al, ' '
        call .pon
        loop .r
.ya:
        ; el cursor ha quedado detrás de la última letra pintada: de su fila
        ; y de las letras que hay hasta ahí sale la fila donde empieza todo
        mov ah, 3
        xor bh, bh
        call ed_v10
        push dx
        call ed_ancho
        mov ax, [ed_n]
        add ax, [ed_sobra]
        mov dl, [ed_org]
        xor dh, dh
        add ax, dx
        xor dx, dx
        div cx
        pop dx
        sub dh, al
        mov [ed_org+1], dh
        mov word [ed_sobra], 0
        jmp sal_si
.pon:
        push cx
        mov ah, 0x0E
        mov bx, 7
        call ed_v10
        pop cx
        ret
; (historial) SI = dónde acaba una línea (detrás de su 0, SI > 0) -> SI =
; dónde empieza
ed_h_antes:
        dec si
.a:
        test si, si
        jz .x
        cmp byte [hist+si-1], 0
        je .x
        dec si
        jmp .a
.x:
        ret
; apunta en el historial la línea recién tecleada (si no es la misma que la
; última, ni está vacía), tirando las más viejas si no cabe. Conserva ES:DI
ed_guarda:
        push di
        push es
        mov cx, [ed_n]
        jcxz .x
        mov si, [hist_fin]
        test si, si
        jz .sitio
        call ed_h_antes
        add si, hist                ; ¿la misma que la última?
        push di
        repe cmpsb
        pop di
        jne .sitio
        cmp byte [si], 0
        je .x
.sitio:
        mov ax, [hist_fin]
        add ax, [ed_n]
        inc ax
        cmp ax, HIST_TAM
        jbe .cabe
        mov si, hist                ; fuera la más vieja
.v:
        lodsb
        test al, al
        jnz .v
        mov cx, hist
        add cx, [hist_fin]
        sub cx, si                  ; lo que queda detrás
        mov ax, si
        sub ax, hist
        sub [hist_fin], ax
        push di
        push es
        push ds
        pop es
        mov di, hist
        rep movsb
        pop es
        pop di
        jmp .sitio
.cabe:
        mov bx, hist
        add bx, [hist_fin]
        mov [hist_fin], ax
        mov cx, [ed_n]
.c:
        mov al, [es:di]
        mov [bx], al
        inc di
        inc bx
        loop .c
        mov byte [bx], 0
.x:
        pop es
        pop di
        ret

; =============================================================================
;  El equipo: procesador y memoria
; =============================================================================
; reconoce el procesador -> [cpu_tipo]. Cada prueba solo usa lo que ya se sabe
; que el procesador tiene
cpu_detecta:
        pushf
        ; PUSH SP: hasta el 80186 se guarda SP ya restado; del 286 en adelante,
        ; el de antes
        push sp
        pop ax
        cmp ax, sp
        jne .antiguo
        ; 286 o más. En el 286, en modo real, los bits 12 a 14 de las
        ; banderas no se pueden poner a uno
        mov bl, CPU_286
        pushf
        pop ax
        or ax, 0x7000
        push ax
        popf
        pushf
        pop ax
        test ax, 0x7000
        jz .fin
        ; 386 o más (ya se pueden usar sus instrucciones). El 486 deja
        ; cambiar el bit AC (18) de las banderas de 32 bits
        cpu 386
        mov bl, CPU_386
        mov dx, sp
        and sp, 0xFFFC              ; (con AC puesto, la pila alineada)
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x00040000
        push eax
        popfd
        pushfd
        pop eax
        xor eax, ecx
        push ecx
        popfd
        mov sp, dx
        test eax, 0x00040000
        jz .fin
        mov bl, CPU_486
        jmp .fin
        cpu 8086
.antiguo:
        ; el 80186 no desplaza más de 31 veces (33 se queda en 1); el 8086, sí
        mov bl, CPU_186
        mov ax, 0xFFFF
        mov cl, 33
        shl ax, cl
        jnz .fin
        ; el V20/V30 de NEC hace AAD siempre en base 10; el 8086 usa el octeto
        ; que le sigue (aquí, 16)
        mov bl, CPU_V20
        mov ax, 0x0101
        db 0xD5, 0x10               ; AAD 16
        cmp al, 0x0B
        je .fin
        mov bl, CPU_8086
.fin:
        popf
        mov [cpu_tipo], bl
        ret

%include "dispositivos.inc"
%include "disco.inc"
%include "archivos.inc"
%include "proceso.inc"
%include "config.inc"
%include "alta.inc"

; =============================================================================
;  Los textos del núcleo (página 437)
; =============================================================================
m_titulo    db 10, "Hispa-8086 0.15 - de la familia Hispa-DOS", 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.", 10, 10, 0
m_sin_command db "Falta COMMAND.COM: no se puede seguir.", 10, 0
m_sin_shell db "No se puede lanzar el intérprete de SHELL=: se usa COMMAND.COM.", 10, 0

; =============================================================================
;  Las variables (no van en el archivo: el núcleo las pone a cero al arrancar)
; =============================================================================
        absolute $
            alignb 2
bss_ini:
cpu_tipo    resb 1
arr_bios    resb 1                  ; la unidad de arranque, para la BIOS
n_arch      resb 1                  ; cuántos archivos abiertos caben (FILES=)
primera     resb 1                  ; aún no se ha lanzado el intérprete
cod_salida  resb 1
op13        resb 1                  ; disco_es: 2 leer, 3 escribir
buf_sucio   resb 1
fat_sucia   resb 1
abiertos    resb 1                  ; cuántos archivos hay abiertos
es_op       resb 1                  ; h_es: 0 leer, 1 escribir
bus_atr     resb 1
red_car     resb 1
cf_files    resb 1                  ; CONFIG.SYS: lo que dice FILES=
cf_fin      resb 1
cf_bufs     resb 1                  ; lo que dice BUFFERS=
cf_pasada   resb 1                  ; 1: todo menos DEVICE=; 2: DEVICE=
cf_uni      resb 1                  ; la primera unidad libre para un controlador
n_cache     resb 1                  ; sectores en la reserva
cache_sig   resb 1                  ; el sitio que toca
cache_mete  resb 1                  ; lo que se lea entra en la reserva
una_disq    resb 1                  ; solo hay una disquetera (B: es la misma)
disq_puesto resb 1                  ; de qué unidad es el disquete puesto
verificar   resb 1                  ; VERIFICAR ON
queda       resb 1                  ; el programa que termina se queda residente
con_directo resb 1                  ; CON: sin redirección
tecla_cod   resb 1                  ; el código de la tecla de dos veces, a medio dar
ed_sob      resb 1                  ; la línea que se teclea: se escribe encima
dos_alto    resb 1                  ; DOS=ALTO: se quiere el núcleo en la HMA
como_8086   resb 1                  ; PROCESADOR=8086: como si no hubiera 286
xms_hay     resb 1                  ; hay gestor de memoria extendida
alto        resb 1                  ; el núcleo está en la HMA
hma_dueno   resb 1                  ; un programa tiene la HMA
a20_g       resb 1                  ; A20 pedida «para todos»
a20_l       resb 1                  ; y cuántas veces «para mí»
cf_bufs_hay resb 1
            alignb 2
mem_kb      resw 1                  ; memoria convencional, en K
ed_max      resw 1                  ; la línea que se teclea: lo que cabe,
ed_n        resw 1                  ; las letras que tiene,
ed_pos      resw 1                  ; en cuál está el cursor,
ed_org      resw 1                  ; dónde empieza en la pantalla (columna y fila),
ed_sobra    resw 1                  ; cuántas letras de antes hay que borrar detrás
ed_h        resw 1                  ; y la línea del historial que se ve
ed_pre      resw 2                  ; el texto puesto de antemano (E808h; segmento 0: no hay)
hist_fin    resw 1                  ; el historial: los octetos que tiene
cf_ficha    resw 1
cf_tope     resw 1                  ; el segmento donde acaba el núcleo
cf_arg      resw 1
cf_tope2    resw 1
cf_lon      resw 1                  ; lo que mide el controlador (DEVICE=)
bajo_seg    resw 1                  ; donde está el puente
p_esta      resw 2                  ; sus rutinas, para llamarlas desde arriba
p_abre      resw 2
p_cierra    resw 2
p_mueve     resw 2
ant2f       resw 2                  ; la INT 2Fh que había
err_bios    resw 1                  ; el último error de disco de la BIOS
ext_k       resw 1                  ; memoria extendida, en K
x_ini       resw 1                  ; de dónde a dónde se reparte (K)
x_fin       resw 1
x_si        resw 1
x_ds        resw 1
x_org       resw 2                  ; mover: de dónde, adónde y cuánto
x_dst       resw 2
x_len       resw 2
x_tabla     resb N_XMS * X_ENT
cache_p     resw 1                  ; la reserva de sectores
n_lect      resw 1                  ; sectores leídos y escritos de verdad
n_escr      resw 1
fecha_xt    resw 1                  ; la fecha, en las máquinas sin reloj
dv_pet      resw 2                  ; la petición que apunta la «estrategia»
dv_far      resw 2
pet         resb 26                 ; la petición a un dispositivo
pet_con     resb 26                 ; y la de la consola (con_pide)
con_dev     resw 2                  ; el CON cargado con DEVICE= (0: ninguno)
con_car     resb 1                  ; su octeto
red_ent     resw 1                  ; la ficha del archivo que hace de entrada (0: el teclado)
red_sal     resw 1                  ; (detrás de red_ent) y de salida (0: la pantalla)
mem_seg     resw 1                  ; la primera cabecera de la memoria que se reparte
psp_act     resw 1                  ; el programa que corre (0: el intérprete)
lz_ent      resw 1                  ; lanza: el entorno pedido
lz_cola     resw 2                  ; lo que va detrás del nombre
lz_ficha    resw 1
lz_ent_seg  resw 1
lz_ent_org  resw 1
lz_ent_off  resw 1
lz_ent_tam  resw 1
lz_psp      resw 1
lz_tam      resw 1
lz_min      resw 1
lz_max      resw 1
lz_img      resw 1
lz_base     resw 1
lz_reub     resw 2
lz_cab      resb 28                 ; la cabecera del .EXE
lz_exe      resb 1
            alignb 2
buf_lba     resw 1                  ; el sector que hay en `sector`
di_clus     resw 1                  ; dir_sig: por dónde va
di_sec      resw 1
di_resto    resw 1
di_off      resw 1
di_quedan   resw 1                  ; los grupos que puede tener aún (dir_sig)
dh_dir      resw 1
r_dir       resw 1                  ; ruta_resuelve: el directorio
rn_sec      resw 1
rn_off      resw 1
rn_dir      resw 1
es_seg      resw 1                  ; h_es: de dónde o adónde
es_off      resw 1
es_queda    resw 1
es_hecho    resw 1
es_n        resw 1
es_dsec     resw 1
dta_seg     resw 1                  ; la zona de las búsquedas (DTA)
dta_off     resw 1
nom11       resb 12
cd_txt      resb DIR_MAX + 2
ruta        resb RUTA_MAX + 2
dta_int     resb 44                 ; la zona de búsquedas del intérprete
linea       resb LIN_MAX + 4
hist        resb HIST_TAM           ; las líneas tecleadas, una tras otra, cada una con su 0
sh_ruta     resb RUTA_MAX + 2       ; SHELL= de CONFIG.SYS: el intérprete...
sh_cola     resb 64                 ; ...y lo que se le pasa
            alignb 2
sector      resb 512
fat         resb 512                ; un sector de la FAT
; ---- la unidad cargada: todo lo que cambia de una unidad a otra ----
u_ini:
u_bios      resb 1                  ; su número para la BIOS (0, 1, 80h)
u_fat16     resb 1                  ; 1: FAT16 (0: FAT12)
disco_ok    resb 1                  ; sus medidas están leídas
            alignb 2
u_base      resw 2                  ; el sector donde empieza (la partición)
spp         resw 1                  ; sectores por pista
caras       resw 1
spc         resw 1                  ; sectores por grupo
bpc         resw 1                  ; octetos por grupo
res_sec     resw 1                  ; sectores reservados (delante de la FAT)
nfat        resw 1                  ; cuántas FAT
spf         resw 1                  ; sectores de cada una
raiz_ini    resw 1                  ; primer sector del directorio raíz
raiz_sec    resw 1                  ; y cuántos tiene
datos_ini   resw 1                  ; primer sector de los datos
max_clus    resw 1                  ; el último grupo del disco
fat_pista   resw 1                  ; por dónde buscar un grupo libre
dir_act     resw 1                  ; el directorio de trabajo (0: la raíz)
t_disco     resw 1                  ; cuándo se usó por última vez
u_dev       resw 2                  ; su controlador (DEVICE=), si es de uno
u_bpb       resw 2                  ; y las medidas que dio
u_sub       resb 1                  ; qué unidad es dentro del controlador
            alignb 2
bpb_ant     resb 52                 ; las medidas del último disco visto
dir_txt     resb DIR_MAX            ; el directorio de trabajo, en texto
u_fin:
U_TAM       equ u_fin - u_ini
u_guardadas resb N_UNI * U_TAM      ; las variables de las unidades no cargadas
u_hay       resb N_UNI + 1          ; qué unidades existen
unidad_act  resb 1                  ; la cargada
unidad_def  resb 1                  ; la de trabajo
rn_uni      resb 1
            alignb 2
fat_lba     resw 1                  ; el sector que hay en `fat`
            resb PILA_TAM
pila_fin:
bss_fin:
; la tabla de archivos abiertos va la última: con FILES= puede crecer hacia
; la memoria que se reparte (hasta bss_fin se pone a cero al arrancar; lo que
; crezca lo pone a cero config_lee)
fichas      resb N_ARCH * H_TAM_F
