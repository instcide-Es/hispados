; =============================================================================
;  Hispa-8086 - el núcleo (NUCLEO86.SYS)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El sistema de la familia Hispa para los PC más veteranos: 8086, 8088, V20,
;  80186 y 286, en modo real. Todo en ensamblador y solo con instrucciones del
;  8086 (el ensamblador lo vigila con «cpu 8086»), para que quepa y corra en
;  un PC o un XT de 256 K. docs/8086/HISPA-8086.md cuenta el plan entero.
;
;  Lo que hay (etapas 0 y 1):
;    - arranca desde un disquete FAT12 (lo carga arranque.asm);
;    - reconoce el procesador y la memoria, y si la máquina puede con un
;      Hispa mayor, lo dice;
;    - INT 21h: la consola (01h, 02h, 06h-0Bh), los vectores (25h, 35h), la
;      versión (30h), terminar (00h, 4Ch, 4Dh, INT 20h) y los ARCHIVOS: crear,
;      abrir, leer, escribir, cerrar, borrar, renombrar, buscar, directorios,
;      sitio libre, fecha y hora (archivos.inc, sobre disco.inc);
;    - ejecuta programas .COM;
;    - un intérprete de órdenes (ordenes.inc), dentro del núcleo por ahora.
;  Lo que falta: la memoria por bloques, los .EXE, CONFIG.SYS, el disco
;  duro... (ver «Etapas» en el documento).
;
;  Mapa de memoria: el núcleo se carga en 0070:0000 y ahí se queda, con sus
;  variables, su pila y la copia de la FAT detrás; el programa que se ejecuta
;  va justo después, hasta el final de la memoria convencional.
; =============================================================================
        cpu 8086
        bits 16
        org 0

DOS_MAYOR   equ 3                   ; lo que contesta INT 21h/30h: el nivel
DOS_MENOR   equ 30                  ; de MS-DOS 3.30, que es el objetivo
LIN_MAX     equ 126                 ; letras de una línea de órdenes
RUTA_MAX    equ 80
DIR_MAX     equ 64                  ; letras del directorio de trabajo
PILA_TAM    equ 768
FAT_MAX     equ 9 * 512             ; la FAT12 más grande: la de 1,44 M

; el procesador
CPU_8086    equ 0
CPU_V20     equ 1
CPU_186     equ 2
CPU_286     equ 3
CPU_386     equ 4
CPU_486     equ 5

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
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        mov [unidad], dl
        call cpu_detecta
        int 0x12                    ; la memoria convencional, en K
        mov [mem_kb], ax
        int 0x11                    ; ¿pantalla monocroma (MDA, Hercules)?
        and al, 0x30
        cmp al, 0x30
        jne .color
        mov byte [es_mono], 1
.color:
        ; los vectores del sistema: INT 20h (terminar) e INT 21h
        xor ax, ax
        mov es, ax
        mov word [es:0x20*4], int20
        mov [es:0x20*4+2], cs
        mov word [es:0x21*4], int21
        mov [es:0x21*4+2], cs
        push cs
        pop es
        ; dónde va el programa: detrás de todo lo del núcleo
        mov ax, bss_fin + 15
        mov cl, 4
        shr ax, cl
        mov bx, cs
        add ax, bx
        mov [prog_seg], ax
        mov [dta_seg], cs           ; las búsquedas, por ahora, a lo nuestro
        mov word [dta_off], dta_int
        call disco_ini
        ; la bienvenida
        mov si, m_titulo
        call pon_cad
        call pon_equipo
        call pon_consejo
        mov si, m_ayuda0
        call pon_cad
        jmp interprete

; =============================================================================
;  INT 20h e INT 21h
; =============================================================================
int20:
        xor al, al
; el programa termina con el código AL: se cierran sus archivos y se vuelve
; al intérprete
termina:
        cli
        mov bx, cs
        mov ds, bx
        mov es, bx
        mov ss, bx
        mov sp, pila_fin
        sti
        cld
        mov [cod_salida], al
        call h_cierra_todos
        mov [dta_seg], cs
        mov word [dta_off], dta_int
        jmp interprete

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
        F21 0x2C, f2C
        F21 0x2F, f2F
        F21 0x30, f30
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
        F21 0x47, f47
        F21 0x4D, f4D
        F21 0x4E, f4E
        F21 0x4F, f4F
        F21 0x56, f56
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
        mov ah, 1
        int 0x16
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
        mov ah, 1
        int 0x16
        mov al, 0
        jz .x
        dec al
.x:
        mov [bp+R_AX], al
        ret
; 0Eh: elegir unidad -> AL = cuántas hay
f0E:
        mov byte [bp+R_AX], 1
        ret
; 19h: la unidad de trabajo -> AL (0 = A:)
f19:
        mov byte [bp+R_AX], 0
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
; 4Dh: el código con que terminó el último programa -> AL (AH = 0)
f4D:
        mov al, [cod_salida]
        xor ah, ah
        mov [bp+R_AX], ax
        ret

; =============================================================================
;  La consola (con la BIOS: funciona con cualquier tarjeta de vídeo)
; =============================================================================
; escribe AL tal cual (lo que mandan los programas). Conserva los registros
pon_car_dos:
        push ax
        push bx
        push bp
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        pop bp
        pop bx
        pop ax
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
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; espera una tecla -> AL (las que no son letra, como las flechas: 0)
tecla:
        xor ah, ah
        int 0x16
        ret
; lee una línea en ES:DI, de CX letras como mucho, hasta Intro; Retroceso
; borra. Deja un 0 al final y CX = las letras leídas. Conserva lo demás
lee_linea:
        push ax
        push bx
        push dx
        mov dx, cx                  ; lo que cabe
        xor bx, bx                  ; lo que hay
.t:
        call tecla
        cmp al, 13
        je .fin
        cmp al, 8
        je .borra
        cmp al, ' '
        jb .t
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

; escribe el procesador y la memoria
pon_equipo:
        mov si, m_cpu
        call pon_cad
        mov bl, [cpu_tipo]
        xor bh, bh
        shl bx, 1
        mov si, [cpu_nombres+bx]
        call pon_cad
        mov si, m_mem
        call pon_cad
        mov ax, [mem_kb]
        call pon_num
        mov si, m_mem2
        call pon_cad
        mov ax, [mem_kb]            ; lo que queda libre para los programas
        mov cl, 6
        shl ax, cl
        sub ax, [prog_seg]
        mov cl, 6
        shr ax, cl
        call pon_num
        mov si, m_mem3
        jmp pon_cad

; si la máquina puede con un Hispa mayor, lo dice (toda la familia se
; recomienda entre sí: ver «Una familia que se actualiza» en el documento)
pon_consejo:
        mov si, m_consejo_386
        cmp byte [cpu_tipo], CPU_386
        jae pon_cad
        mov si, m_consejo_286
        cmp byte [cpu_tipo], CPU_286
        je pon_cad
        ret

            align 2
cpu_nombres dw c_8086, c_v20, c_186, c_286, c_386, c_486
c_8086      db "8086 u 8088", 0
c_v20       db "NEC V20 o V30", 0
c_186       db "80186 u 80188", 0
c_286       db "80286", 0
c_386       db "80386", 0
c_486       db "80486 o superior", 0

%include "disco.inc"
%include "archivos.inc"
%include "ordenes.inc"

; =============================================================================
;  Los textos del núcleo (página 437)
; =============================================================================
m_titulo    db 10, "Hispa-8086 0.2 - de la familia Rescoldo", 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.", 10, 10, 0
m_cpu       db "Procesador: ", 0
m_mem       db 10, "Memoria:    ", 0
m_mem2      db " K (", 0
m_mem3      db " K libres para los programas)", 10, 0
m_consejo_386 db 10, "Este equipo tiene un procesador de 32 bits: puede con HISPA-DOS, el", 10
            db "hermano mayor, que trae el entorno Hispa Ventanas. Hispa-8086 funciona", 10
            db "aquí, pero está hecho para los PC, XT y AT. Escriba ACTUALIZAR.", 10, 0
m_consejo_286 db 10, "Este equipo tiene un 286: Hispa-8086 lo aprovecha (modo 286).", 10, 0
m_ayuda0    db 10, "Escriba AYUDA para ver las órdenes.", 10, 0

; =============================================================================
;  Las variables (no van en el archivo: el núcleo las pone a cero al arrancar)
; =============================================================================
        absolute $
            alignb 2
bss_ini:
unidad      resb 1                  ; la unidad de arranque (la de la BIOS)
cpu_tipo    resb 1
es_mono     resb 1
disco_ok    resb 1
cod_salida  resb 1
op13        resb 1                  ; disco_es: 2 leer, 3 escribir
buf_sucio   resb 1
fat_sucia   resb 1
abiertos    resb 1                  ; cuántos archivos hay abiertos
es_op       resb 1                  ; h_es: 0 leer, 1 escribir
bus_atr     resb 1
            alignb 2
mem_kb      resw 1                  ; memoria convencional, en K
prog_seg    resw 1                  ; donde se carga el programa
t_disco     resw 1                  ; cuándo se usó el disco por última vez
buf_lba     resw 1                  ; el sector que hay en `sector`
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
dir_act     resw 1                  ; el directorio de trabajo (0: la raíz)
di_clus     resw 1                  ; dir_sig: por dónde va
di_sec      resw 1
di_resto    resw 1
di_off      resw 1
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
bpb_ant     resb 52                 ; las medidas del último disco visto
dir_txt     resb DIR_MAX            ; el directorio de trabajo, en texto
cd_txt      resb DIR_MAX + 2
ruta        resb RUTA_MAX + 2
dta_int     resb 44                 ; la zona de búsquedas del intérprete
fichas      resb N_ARCH * H_TAM_F
linea       resb LIN_MAX + 4
VARS_ORDENES
            alignb 2
sector      resb 512
fat         resb FAT_MAX
            resb PILA_TAM
pila_fin:
bss_fin:
