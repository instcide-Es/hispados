; =============================================================================
;  Hispa-8086 - el núcleo (NUCLEO86.SYS)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El sistema de la familia Hispa para los PC más veteranos: 8086, 8088, V20,
;  80186 y 286, en modo real. Todo en ensamblador y solo con instrucciones del
;  8086 (el ensamblador lo vigila con «cpu 8086»), para que quepa y corra en
;  un PC o un XT de 256 K. docs/8086/HISPA-8086.md cuenta el plan entero.
;
;  ESTO SON LOS CIMIENTOS (etapa 0). Lo que ya hace:
;    - arranca desde un disquete FAT12 (lo carga h8086/arranque.asm);
;    - reconoce el procesador (8086/8088, V20/V30, 80186, 286, 386, 486 o
;      más) y la memoria, y si la máquina puede con un Hispa mayor, lo dice;
;    - da los servicios de consola de INT 21h (01h, 02h, 06h-0Bh), los
;      vectores (25h, 35h), la versión (30h) y terminar (00h, 4Ch, INT 20h);
;    - lee el disquete (FAT12, directorio raíz) y ejecuta programas .COM;
;    - trae un intérprete de órdenes mínimo, dentro del núcleo por ahora:
;      AYUDA, VER, EQUIPO, DIR, TIPO, BORRAR (la pantalla) y ACTUALIZAR.
;  Lo que falta es casi todo lo demás (archivos por INT 21h, escribir en
;  disco, memoria, .EXE, CONFIG.SYS...): ver «Etapas» en el documento.
;
;  Mapa de memoria: el núcleo se carga en 0070:0000 y ahí se queda, con sus
;  variables, su pila y la copia de la FAT detrás; el programa que se ejecuta
;  va justo después, hasta el final de la memoria convencional.
; =============================================================================
        cpu 8086
        bits 16
        org 0

VER_MAYOR   equ 0                   ; Hispa-8086 0.1
VER_MENOR   equ 1
DOS_MAYOR   equ 3                   ; lo que contesta INT 21h/30h: el nivel
DOS_MENOR   equ 30                  ; de MS-DOS 3.30, que es el objetivo
LIN_MAX     equ 78                  ; letras de una línea de órdenes
PILA_TAM    equ 512
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
        mov [unidad], dl
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
        call disco_ini
        ; la bienvenida
        mov si, m_titulo
        call pon_cad
        call pon_equipo
        call pon_consejo
        mov si, m_ayuda0
        call pon_cad

; =============================================================================
;  El intérprete de órdenes (provisional: irá aparte, como COMMAND.COM)
; =============================================================================
interprete:
        cli
        mov ax, cs
        mov ss, ax
        mov sp, pila_fin
        mov ds, ax
        mov es, ax
        sti
        cld
        mov si, m_indicador
        call pon_cad
        mov di, linea
        mov cx, LIN_MAX
        call lee_linea
        call pon_nl
        ; la orden: la primera palabra, en mayúsculas
        mov si, linea
        call salta_esp
        cmp byte [si], 0
        je interprete
        mov di, orden
        mov cx, 8
.p:
        mov al, [si]
        cmp al, ' '
        jbe .pf
        cmp al, '.'
        je .pf
        call mayus
        stosb
        inc si
        loop .p
.p2:
        mov al, [si]                ; (una palabra de más de ocho letras)
        cmp al, ' '
        jbe .pf
        cmp al, '.'
        je .pf
        inc si
        jmp .p2
.pf:
        xor al, al
        stosb
        mov [resto], si             ; lo que viene detrás
        ; ¿es una orden de las de dentro?
        mov bx, ordenes
.b:
        mov si, [bx]
        test si, si
        jz .programa
        mov di, orden
.c:
        mov al, [si]
        cmp al, [di]
        jne .otra
        inc si
        inc di
        test al, al
        jnz .c
        mov si, [resto]
        call salta_esp
        call [bx+2]
        jmp interprete
.otra:
        add bx, 4
        jmp .b
.programa:
        call ejecuta
        jmp interprete

            align 2
ordenes     dw t_ayuda, o_ayuda
            dw t_ver, o_ver
            dw t_equipo, o_equipo
            dw t_dir, o_dir
            dw t_tipo, o_tipo
            dw t_borrar, o_borrar
            dw t_actualizar, o_actualizar
            dw 0
t_ayuda     db "AYUDA", 0
t_ver       db "VER", 0
t_equipo    db "EQUIPO", 0
t_dir       db "DIR", 0
t_tipo      db "TIPO", 0
t_borrar    db "BORRAR", 0
t_actualizar db "ACTUALIZ", 0       ; (ACTUALIZAR: las órdenes se miran a ocho letras)

o_ayuda:
        mov si, m_ayuda
        jmp pon_cad
o_ver:
        mov si, m_ver
        jmp pon_cad
o_equipo:
        call pon_equipo
        jmp pon_consejo
o_borrar:
        mov ax, 0x0003              ; el modo de texto, otra vez: pantalla limpia
        cmp byte [es_mono], 0
        je .m
        mov al, 7
.m:
        int 0x10
        ret
o_actualizar:
        mov si, m_actualizar
        call pon_cad
        jmp pon_consejo

; ---- DIR: el directorio raíz ----
o_dir:
        xor bp, bp                  ; cuántos archivos
        mov word [d_total], 0
        mov word [d_total+2], 0
        mov ax, [raiz_ini]
        mov cx, [raiz_sec]
.s:
        push cx
        push ax
        mov bx, sector
        call lee_sector
        jc .err
        mov si, sector
.e:
        mov al, [si]
        test al, al
        jz .fin2
        cmp al, 0xE5                ; borrada
        je .sig
        mov al, [si+11]
        test al, 0x08               ; la etiqueta (y los nombres largos)
        jnz .sig
        call dir_linea
        inc bp
.sig:
        add si, 32
        cmp si, sector + 512
        jb .e
        pop ax
        pop cx
        inc ax
        loop .s
        jmp .fin
.fin2:
        pop ax
        pop cx
.fin:
        mov ax, bp
        call pon_num
        mov si, m_archivos
        call pon_cad
        mov ax, [d_total]
        mov dx, [d_total+2]
        call pon_num32
        mov si, m_octetos
        jmp pon_cad
.err:
        pop ax
        pop cx
        mov si, m_e_disco
        jmp pon_cad
; (interna) una línea del DIR: la entrada de SI
dir_linea:
        push si
        mov cx, 8
.n:
        lodsb
        call pon_car
        loop .n
        mov al, ' '
        call pon_car
        mov cx, 3
.x:
        lodsb
        call pon_car
        loop .x
        pop si
        test byte [si+11], 0x10
        jz .arch
        push si
        mov si, m_dir
        call pon_cad
        pop si
        ret
.arch:
        mov ax, [si+28]
        mov dx, [si+30]
        add [d_total], ax
        adc [d_total+2], dx
        push si
        mov cx, 10                  ; el tamaño, arrimado a la derecha en 10
        call pon_num32_ancho
        call pon_nl
        pop si
        ret

; ---- TIPO: enseña un archivo de texto ----
o_tipo:
        cmp byte [si], 0
        jne .n
        mov si, m_e_falta_nombre
        jmp pon_cad
.n:
        call nombre_fat
        call carga
        jc .no
        push ds
        mov cx, [f_tam]
        mov ds, [prog_seg]
        mov si, 0x100
        jcxz .fin
.c:
        lodsb
        cmp al, 0x1A                ; el fin de archivo de los textos
        je .fin
        call pon_car
        loop .c
.fin:
        pop ds
        jmp pon_nl
.no:
        ret

; ---- un programa: ORDEN.COM, con lo que venga detrás como parámetros ----
ejecuta:
        mov si, orden               ; el nombre, con la extensión COM
        mov di, nom11
        mov cx, 8
.n:
        lodsb
        test al, al
        jz .pad
        stosb
        loop .n
        jmp .ext
.pad:
        mov al, ' '
        rep stosb
.ext:
        mov word [nom11+8], 'CO'
        mov byte [nom11+10], 'M'
        mov byte [carga_callada], 1
        call carga
        mov byte [carga_callada], 0
        jnc .esta
        mov si, m_e_orden
        jmp pon_cad
.esta:
        ; el prefijo del programa (PSP), delante de él
        mov es, [prog_seg]
        xor di, di
        mov cx, 0x80
        xor ax, ax
        rep stosw
        mov word [es:0], 0x20CD     ; INT 20h: «ret» al principio también termina
        mov ax, [mem_kb]            ; el final de la memoria, en segmento
        mov cl, 6
        shl ax, cl
        mov [es:2], ax
        mov si, [resto]             ; los parámetros
        mov di, 0x81
        xor cx, cx
.t:
        lodsb
        test al, al
        jz .tf
        stosb
        inc cx
        cmp cx, 126
        jb .t
.tf:
        mov byte [es:di], 0x0D
        mov [es:0x80], cl
        ; y a él: todos los segmentos en el suyo, la pila arriba del todo
        mov ax, [prog_seg]
        mov bx, [es:2]
        sub bx, ax                  ; los párrafos que tiene
        mov dx, 0xFFFE
        cmp bx, 0x1000              ; (con menos de 64 K, hasta donde llegue)
        jae .pila
        mov dx, bx
        mov cl, 4
        shl dx, cl
        sub dx, 2
.pila:
        cli
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, dx
        sti
        xor bx, bx
        push bx                     ; un cero: «ret» salta a PSP:0000
        push ax
        mov bx, 0x100
        push bx
        retf

; =============================================================================
;  INT 20h e INT 21h
; =============================================================================
int20:
        xor al, al
termina:
        push cs
        pop ds
        mov [cod_salida], al
        jmp interprete

; la pila, dentro de INT 21h, con todos los registros guardados
R_ES        equ 0
R_DS        equ 2
R_BP        equ 4
R_DI        equ 6
R_SI        equ 8
R_DX        equ 10
R_CX        equ 12
R_BX        equ 14
R_AX        equ 16
R_BAND      equ 22                  ; las banderas de quien llama

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
        cld
        and byte [bp+R_BAND], 0xFE  ; sin error, mientras no se diga
        mov bl, ah
        xor bh, bh
        cmp bl, F21_N
        jae .mal
        shl bx, 1
        call [f21+bx]
        jmp .fin
.mal:
        call f_mal
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

            align 2
f21         dw f_mal, f01, f02, f_mal, f_mal, f_mal, f06, f07      ; 00-07
            dw f07, f09, f0A, f0B, f_mal, f_nada, f0E, f_mal       ; 08-0F
            dw f_mal, f_mal, f_mal, f_mal, f_mal, f_mal, f_mal, f_mal   ; 10-17
            dw f_mal, f19, f_nada, f_mal, f_mal, f_mal, f_mal, f_mal    ; 18-1F
            dw f_mal, f_mal, f_mal, f_mal, f_mal, f25, f_mal, f_mal     ; 20-27
            dw f_mal, f_mal, f_mal, f_mal, f_mal, f_mal, f_nada, f_mal  ; 28-2F
            dw f30, f_mal, f_mal, f_mal, f_mal, f35                     ; 30-35
F21_N       equ ($ - f21) / 2

; la función no existe (todavía): acarreo y el error 1
f_mal:
        or byte [bp+R_BAND], 1
        mov word [bp+R_AX], 1
f_nada:
        ret
; 01h: una tecla, con eco -> AL
f01:
        call tecla
        mov [bp+R_AX], al
        jmp pon_car
; 02h: escribe DL
f02:
        mov al, [bp+R_DX]
        jmp pon_car
; 06h: DL = el carácter que se escribe; con DL = FFh, la tecla que haya
; (AL, y ZF=0) o ninguna (ZF=1)
f06:
        mov al, [bp+R_DX]
        cmp al, 0xFF
        jne pon_car
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
        call pon_car
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
        push bx
        call lee_linea              ; ES:DI, CX letras -> CX = las leídas
        pop bx
        mov [es:bx+1], cl
        add di, cx
        mov byte [es:di], 0x0D
        mov al, 0x0D
        call pon_car
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

; =============================================================================
;  La consola (con la BIOS: funciona con cualquier tarjeta de vídeo)
; =============================================================================
; escribe AL. Conserva los registros
pon_car:
        push ax
        push bx
        push bp
        cmp al, 10                  ; el salto de línea baja y vuelve al margen
        jne .c
        mov ax, 0x0E0D
        mov bx, 7
        int 0x10
        mov al, 10
.c:
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        pop bp
        pop bx
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
; borra. Deja un 0 al final y CX = las letras leídas. Conserva DI
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
        call pon_car
        jmp .t
.borra:
        test bx, bx
        jz .t
        dec bx
        mov al, 8
        call pon_car
        mov al, ' '
        call pon_car
        mov al, 8
        call pon_car
        jmp .t
.fin:
        mov byte [es:di+bx], 0
        mov cx, bx
        pop dx
        pop bx
        pop ax
        ret
; SI salta los espacios
salta_esp:
        cmp byte [si], ' '
        jne .x
        inc si
        jmp salta_esp
.x:
        ret
; AL, a mayúscula
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret

; =============================================================================
;  El equipo: procesador y memoria
; =============================================================================
; reconoce el procesador -> [cpu_tipo]. Cada prueba solo usa lo que ya se sabe que
; el procesador tiene
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

; =============================================================================
;  El disco: FAT12, de lectura (el directorio raíz)
; =============================================================================
; lee las medidas del disquete (su BPB) y la FAT
disco_ini:
        mov word [spp], 18          ; (para leer el primer sector, cualquier
        mov word [caras], 2         ; medida vale: es pista 0, cara 0, sector 1)
        xor ax, ax
        mov bx, sector
        call lee_sector
        jc .mal
        mov ax, [sector+0x18]
        mov [spp], ax
        mov ax, [sector+0x1A]
        mov [caras], ax
        mov al, [sector+0x0D]
        xor ah, ah
        mov [spc], ax
        mov al, [sector+0x10]       ; la raíz: detrás de lo reservado y las FAT
        mul word [sector+0x16]
        add ax, [sector+0x0E]
        mov [raiz_ini], ax
        mov bx, [sector+0x11]
        mov cl, 4
        shr bx, cl
        mov [raiz_sec], bx
        add ax, bx
        mov [datos_ini], ax
        mov cx, [sector+0x16]       ; la FAT, a la memoria
        cmp cx, FAT_MAX / 512
        ja .mal
        mov ax, [sector+0x0E]
        mov bx, fat
.f:
        call lee_sector
        jc .mal
        inc ax
        add bx, 512
        loop .f
        mov byte [disco_ok], 1
        ret
.mal:
        mov si, m_e_disco
        jmp pon_cad

; lee el sector AX en ES:BX; CF=1 si no se puede (tres intentos). Conserva
; los registros
lee_sector:
        push ax
        push cx
        push dx
        push si
        mov si, 3
.otra:
        push ax
        xor dx, dx
        div word [spp]
        mov cl, dl
        inc cl
        xor dx, dx
        div word [caras]
        mov ch, al
        mov dh, dl
        mov dl, [unidad]
        mov ax, 0x0201
        int 0x13
        pop ax
        jnc .x
        push ax
        xor ax, ax
        int 0x13
        pop ax
        dec si
        jnz .otra
        stc
.x:
        pop si
        pop dx
        pop cx
        pop ax
        ret

; el nombre de DS:SI («carta.txt») -> nom11, como en el directorio
; («CARTA   TXT»)
nombre_fat:
        push ax
        push cx
        push di
        mov di, nom11
        mov cx, 11
        mov al, ' '
        rep stosb
        mov di, nom11
        mov cx, 8
.n:
        mov al, [si]
        cmp al, ' '
        jbe .x
        inc si
        cmp al, '.'
        je .ext
        jcxz .n                     ; (lo que pasa de ocho letras se pierde)
        call mayus
        stosb
        dec cx
        jmp .n
.ext:
        mov di, nom11 + 8
        mov cx, 3
.e:
        mov al, [si]
        cmp al, ' '
        jbe .x
        inc si
        jcxz .e
        call mayus
        stosb
        dec cx
        jmp .e
.x:
        pop di
        pop cx
        pop ax
        ret

; carga el archivo nom11 en prog_seg:0100 -> [f_tam]; CF=1 (y lo dice, salvo
; con carga_callada) si no está, no cabe o falla el disco
carga:
        cmp byte [disco_ok], 0
        je .e_disco
        ; buscarlo en la raíz
        mov ax, [raiz_ini]
        mov cx, [raiz_sec]
.s:
        mov bx, sector
        call lee_sector
        jc .e_disco
        mov di, sector
.e:
        cmp byte [di], 0
        je .e_no
        test byte [di+11], 0x18     ; ni etiquetas ni carpetas
        jnz .sig
        push cx
        push di
        mov si, nom11
        mov cx, 11
        repe cmpsb
        pop di
        pop cx
        je .esta
.sig:
        add di, 32
        cmp di, sector + 512
        jb .e
        inc ax
        loop .s
.e_no:
        mov si, m_e_no_esta
        jmp .err
.esta:
        cmp word [di+30], 0         ; ¿cabe? (un .COM: menos de 64 K; y la
        jne .e_grande               ; memoria que haya)
        mov ax, [di+28]
        cmp ax, 0xFE00
        ja .e_grande
        mov [f_tam], ax
        mov bx, [mem_kb]
        mov cl, 6
        shl bx, cl
        sub bx, [prog_seg]          ; los párrafos libres
        mov dx, ax
        add dx, 0x100 + 15 + 512    ; (y el último grupo, que se lee entero)
        mov cl, 4
        shr dx, cl
        cmp dx, bx
        jae .e_grande
        ; leerlo, grupo a grupo
        mov ax, [di+26]
        push es
        mov es, [prog_seg]
        mov bx, 0x100
        cmp word [f_tam], 0
        je .leido
.g:
        cmp ax, 2
        jb .cadena
        cmp ax, 0x0FF0
        jae .cadena
        push ax
        sub ax, 2
        mul word [spc]
        add ax, [datos_ini]
        mov cx, [spc]
        pop si                      ; SI = el grupo
.sec:
        call lee_sector
        jc .e_lec
        inc ax
        add bx, 512
        loop .sec
        mov ax, si                  ; el grupo siguiente: 12 bits en n*3/2
        shr si, 1
        add si, ax
        mov dx, [fat+si]
        test al, 1
        jz .par
        mov cl, 4
        shr dx, cl
.par:
        and dx, 0x0FFF
        mov ax, dx
        mov cx, bx                  ; ¿está ya todo?
        sub cx, 0x100
        cmp cx, [f_tam]
        jb .g
.leido:
        pop es
        clc
        ret
.cadena:
.e_lec:
        pop es
.e_disco:
        mov si, m_e_disco
        jmp .err
.e_grande:
        mov si, m_e_grande
.err:
        cmp byte [carga_callada], 0
        jne .c
        call pon_cad
.c:
        stc
        ret

; =============================================================================
;  Los textos (página 437)
; =============================================================================
m_titulo    db 10, "Hispa-8086 0.1 - de la familia Rescoldo", 10
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
m_indicador db 10, "A>", 0
m_ver       db "Hispa-8086 versión 0.1 (cimientos)", 10, 0
m_ayuda     db "Las órdenes de Hispa-8086 0.1:", 10
            db "  AYUDA       esta lista", 10
            db "  VER         la versión del sistema", 10
            db "  EQUIPO      el procesador y la memoria", 10
            db "  DIR         los archivos del disco", 10
            db "  TIPO nombre enseña un archivo de texto", 10
            db "  BORRAR      limpia la pantalla", 10
            db "  ACTUALIZAR  cómo pasar a otro sistema Hispa", 10
            db "Cualquier otra palabra: el programa .COM de ese nombre.", 10, 0
m_actualizar db "Todos los sistemas Hispa se actualizan entre sí:", 10
            db "  Hispa-8086  PC, XT y AT: 8086, 8088, V20, 80186 y 286", 10
            db "  Hispa-DOS   386, 486 y superiores, con Hispa Ventanas", 10
            db "Las versiones y los discos, en https://hispados.es", 10, 0
m_archivos  db " archivos, ", 0
m_octetos   db " octetos", 10, 0
m_dir       db "     <DIR>", 10, 0
m_e_disco   db "Error al leer el disco.", 10, 0
m_e_no_esta db "No existe ese archivo.", 10, 0
m_e_grande  db "El archivo no cabe en la memoria.", 10, 0
m_e_orden   db "Orden desconocida. Escriba AYUDA.", 10, 0
m_e_falta_nombre db "Falta el nombre del archivo.", 10, 0

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
carga_callada resb 1
cod_salida  resb 1
mem_kb      resw 1                  ; memoria convencional, en K
prog_seg    resw 1                  ; donde se carga el programa
spp         resw 1                  ; sectores por pista
caras       resw 1
spc         resw 1                  ; sectores por grupo
raiz_ini    resw 1                  ; primer sector del directorio raíz
raiz_sec    resw 1                  ; y cuántos tiene
datos_ini   resw 1                  ; primer sector de los datos
f_tam       resw 1                  ; tamaño del archivo cargado
d_total     resw 2
resto       resw 1
orden       resb 10
nom11       resb 12
linea       resb LIN_MAX + 2
sector      resb 512
fat         resb FAT_MAX
            resb PILA_TAM
pila_fin:
bss_fin:
