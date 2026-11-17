; =============================================================================
;  Hispa-8086 - el disco: sectores, FAT12, directorios, rutas y archivos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Capas, de abajo arriba:
;    sectores     disco_es lee o escribe un sector con la BIOS (INT 13h);
;                 sec_trae lo deja en el búfer único `sector` (que se apunta
;                 «sucio» cuando se cambia y se escribe al cambiar de sector);
;    FAT          la FAT12 entera está en memoria (`fat`): fat_lee, fat_pon,
;                 fat_nuevo, fat_suelta; fat_vuelca la escribe en todas sus
;                 copias;
;    directorios  dir_abre y dir_sig recorren un directorio (la raíz, de
;                 tamaño fijo, o un subdirectorio, que es una cadena de
;                 grupos); dir_busca, dir_hueco;
;    rutas        ruta_resuelve: «\CARTAS\JUAN.TXT» -> el directorio que lo
;                 contiene y el nombre, como en el disco (nom11);
;    archivos     las fichas de los archivos abiertos (H_*) y h_es, que lee o
;                 escribe por donde va cada uno.
;  Todas las rutinas suponen DS = ES = el núcleo y devuelven CF=1 con el
;  código de error de DOS en AX cuando fallan.
;
;  UNIDADES. Hay hasta tres: A: y B:, los disquetes, y C:, la primera partición
;  FAT del primer disco duro (FAT12 o FAT16 de hasta 32 megas, como en MS-DOS
;  3.3: así los números de sector caben en 16 bits). Todas las rutinas de
;  este archivo trabajan con «la unidad cargada»: sus medidas, su directorio
;  de trabajo y los dos búferes están en un bloque de variables (de u_ini a
;  u_fin), y unidad_pon cambia de unidad guardando ese bloque y trayendo el de
;  la otra. Cada ruta («C:\DATOS\X») y cada archivo abierto dicen de qué
;  unidad son.
;
;  LA FAT se lee por sectores, a su propio búfer (`fat`), no entera: una FAT16
;  de 32 megas son 32 K, que en estas máquinas no sobran.
; =============================================================================
N_ARCH      equ 8                   ; archivos abiertos a la vez (sin FILES=)
N_UNI       equ 5                   ; unidades: A:, B:, C: y dos de controladores (D:, E:)
C_TAM       equ 516                 ; una entrada de la reserva de sectores:
C_UNI       equ 0                   ; db su unidad (FFh: libre), dw su sector
C_LBA       equ 2                   ; (en 2) y los 512 octetos (en 4)
BUF_DEF     equ 4                   ; sectores en la reserva, sin BUFFERS=
BUF_MAX     equ 30
BUF_ALTO    equ 20                  ; sin BUFFERS=, cuando el núcleo va en la HMA
PRIMERO     equ 5                   ; el primer número de archivo (0-4: la consola)
; la ficha de un archivo abierto
H_USO       equ 0                   ; db: 0 libre
H_MODO      equ 1                   ; db: 0 leer, 1 escribir, 2 las dos cosas
H_PRIM      equ 2                   ; dw: su primer grupo (0: vacío)
H_TAM       equ 4                   ; dd: tamaño
H_POS       equ 8                   ; dd: por dónde va
H_DSEC      equ 12                  ; dw: el sector de su entrada de directorio
H_DOFF      equ 14                  ; dw: y dónde está dentro de él
H_CCLU      equ 16                  ; dw: el último grupo visitado...
H_CIDX      equ 18                  ; dw: ...y qué número hace en la cadena
H_SUCIO     equ 20                  ; db: hay que poner al día su entrada
H_UNI       equ 21                  ; db: su unidad (0 = A:)
H_PSP       equ 22                  ; dw: el programa que lo abrió
H_FECHA     equ 24                  ; dw: la fecha que se le ha puesto con 57h
H_HORA      equ 26                  ; dw: (0: ninguna; al cerrar, la de entonces)
H_TAM_F     equ 28
; errores de DOS
E_FUNCION   equ 1
E_ARCHIVO   equ 2                   ; no existe el archivo
E_RUTA      equ 3                   ; no existe la ruta
E_MUCHOS    equ 4                   ; demasiados archivos abiertos
E_ACCESO    equ 5                   ; acceso denegado
E_NUMERO    equ 6                   ; número de archivo no válido
E_UNIDAD    equ 15                  ; esa unidad no existe
E_DIR_ACT   equ 16                  ; no se puede quitar el directorio de trabajo
E_OTRO_DISP equ 17                  ; no es el mismo dispositivo
E_NO_MAS    equ 18                  ; no hay más archivos
E_PROTEGIDO equ 19                  ; disco protegido contra escritura
E_DISCO     equ 21                  ; unidad no preparada
E_DISP      equ 29                  ; el dispositivo no ha podido escribir
E_LLENO     equ 39                  ; disco lleno (interno)

; =============================================================================
;  Sectores
; =============================================================================
; lee (lee_sector) o escribe (escribe_sector) el sector AX de la unidad
; cargada en ES:BX (que es del núcleo). CF=1 y AX=error si no se puede.
; Conserva los demás registros.
;
; LA RESERVA DE SECTORES. Entre estas dos rutinas y el disco hay una reserva
; pequeña (lo que diga BUFFERS= en CONFIG.SYS) con los últimos sectores
; leídos de la FAT y de los directorios, que son los que se piden una y otra
; vez: lo que está en ella no se vuelve a leer del disquete. Los sectores de
; datos de los archivos no entran (h_es quita [cache_mete] mientras los
; pide): pasan una vez y echarían a los que sirven. Escribir va SIEMPRE al
; disco en el momento (y pone al día la copia, si la hay): la reserva no
; retrasa nada, así que sacar el disquete no pierde más que antes. Lo único
; que se retrasa, como hasta ahora, es el sector en uso (`sector` y `fat`),
; que se escribe al cambiar de sector y al cerrar el archivo.
lee_sector:
        push si
        call cache_busca
        jc .disco
        push cx                     ; está: de la reserva
        push di
        add si, 4
        mov di, bx
        mov cx, 256
        rep movsw
        pop di
        pop cx
        pop si
        clc
        ret
.disco:
        pop si
        mov byte [op13], 2
        call disco_es
        jc .x
        cmp byte [cache_mete], 0
        je .x                       ; (CF=0)
        call cache_pon
        clc
.x:
        ret
escribe_sector:
        mov byte [op13], 3
        call disco_es
        jc .x
        push si
        call cache_busca
        jc .n
        push cx                     ; su copia, al día
        push di
        lea di, [si+4]
        mov si, bx
        mov cx, 256
        rep movsw
        pop di
        pop cx
.n:
        pop si
        clc
.x:
        ret
; busca en la reserva el sector AX de la unidad cargada -> SI = su entrada.
; CF=1 si no está. Conserva los demás registros
cache_busca:
        push cx
        push dx
        mov cl, [n_cache]
        xor ch, ch
        jcxz .no
        mov si, [cache_p]
        mov dl, [unidad_act]
.b:
        cmp [si+C_UNI], dl
        jne .s
        cmp [si+C_LBA], ax
        je .si
.s:
        add si, C_TAM
        loop .b
.no:
        stc
.si:
        pop dx
        pop cx
        ret
; mete en la reserva el sector AX, que está en ES:BX, en el sitio del más
; antiguo (se dan por turno). Conserva los registros
cache_pon:
        cmp byte [n_cache], 0
        je .x
        push ax
        push cx
        push dx
        push si
        push di
        push ax
        mov al, [cache_sig]
        xor ah, ah
        mov cx, C_TAM
        mul cx
        mov di, ax
        add di, [cache_p]
        pop ax
        mov cl, [cache_sig]
        inc cl
        cmp cl, [n_cache]
        jb .k
        xor cl, cl
.k:
        mov [cache_sig], cl
        mov cl, [unidad_act]
        mov [di+C_UNI], cl
        mov [di+C_LBA], ax
        add di, 4
        mov si, bx
        mov cx, 256
        rep movsw
        pop di
        pop si
        pop dx
        pop cx
        pop ax
.x:
        ret
; olvida lo que hay en la reserva de la unidad AL (FFh: de todas). Conserva
; los registros
cache_olvida:
        push cx
        push si
        mov cl, [n_cache]
        xor ch, ch
        jcxz .x
        mov si, [cache_p]
.b:
        cmp al, 0xFF
        je .f
        cmp [si+C_UNI], al
        jne .s
.f:
        mov byte [si+C_UNI], 0xFF
.s:
        add si, C_TAM
        loop .b
.x:
        pop si
        pop cx
        ret

; el disco de verdad: lee ([op13]=2) o escribe (3) el sector AX de la unidad
; cargada en ES:BX, con la BIOS (INT 13h) o, si la unidad es de un controlador
; (DEVICE=), pidiéndoselo a él. Tres intentos. Conserva los demás registros
disco_es:
        cmp word [u_dev+2], 0
        je .bios
        jmp dev_bloque
.bios:
        ; con una sola disquetera, A: y B: son la misma: si el disquete que
        ; hay puesto es el de la otra, se pide que lo cambien
        cmp byte [una_disq], 0
        je .va
        cmp byte [u_bios], 0x80
        jae .va
        push ax
        mov al, [unidad_act]
        cmp al, [disq_puesto]
        je .p0
        call pide_disquete
.p0:
        pop ax
.va:
        push bx
        push cx
        push dx
        push si
        push ax
        mov si, 3
.otra:
        pop ax
        push ax
        push bx
        xor dx, dx                  ; el sector, contado desde el principio del
        add ax, [u_base]            ; disco (32 bits): más donde empieza la
        adc dx, [u_base+2]          ; partición
        mov bx, ax
        mov ax, dx                  ; entre los sectores de una pista, en dos pasos
        xor dx, dx
        div word [spp]
        mov ax, bx
        div word [spp]              ; AX = pista x caras + cara, DX = sector - 1
        mov cx, dx
        inc cl
        xor dx, dx
        div word [caras]            ; AX = cilindro, DX = cara
        mov dh, dl
        mov ch, al
        mov al, ah                  ; (los dos bits altos del cilindro, con el sector)
        ror al, 1
        ror al, 1
        and al, 0xC0
        or cl, al
        pop bx
        mov dl, [u_bios]
        cmp byte [alto], 0
        jne .alto
        mov ah, [op13]
        mov al, 1
        int 0x13
        jmp short .hecho13
.alto:
        call bios_bajo              ; (el núcleo en la HMA: por la memoria baja)
.hecho13:
        jc .falla
        cmp byte [op13], 3          ; VERIFICAR ON: lo escrito se repasa
        jne .bien
        cmp byte [verificar], 0
        je .bien
        mov ax, 0x0401
        int 0x13
        jnc .bien
.falla:
        mov [err_bios], ah          ; (lo que ha dicho la BIOS, para quien
        mov al, [op13]              ; quiera saberlo: INT 21h/E8h con AL=7)
        mov [err_bios+1], al
        cmp ah, 3                   ; protegido contra escritura: no insistir
        je .prot
        cmp ah, 6                   ; han cambiado el disquete (lo dicen los
        jne .rep                    ; AT): lo guardado de él ya no vale
        mov al, [unidad_act]
        call cache_olvida
.rep:
        xor ax, ax
        int 0x13
        dec si
        jnz .otra
        pop ax
        mov ax, E_DISCO
        jmp short .mal
.prot:
        pop ax
        mov ax, E_PROTEGIDO
.mal:
        stc
        jmp short .x
.bien:
        push es
        xor ax, ax                  ; la hora del último uso del disco
        mov es, ax
        mov ax, [es:0x46C]
        mov [t_disco], ax
        pop es
        mov si, n_lect              ; (la cuenta de lo leído y lo escrito,
        cmp byte [op13], 2          ; para medir: INT 21h/E8h con AL=4)
        je .cta
        mov si, n_escr
.cta:
        inc word [si]
        pop ax
        clc
.x:
        pop si
        pop dx
        pop cx
        pop bx
        ret

; con el núcleo en la HMA, un sector por la BIOS pasando por b_buf, en la
; memoria baja (en el puente): hay BIOS que no leen ni escriben por encima del
; mega. Como INT 13h: [op13] = 2 leer o 3 escribir, CX y DX como para la BIOS,
; ES:BX = el sector de verdad. Devuelve CF y AH como la BIOS. Conserva BX, CX,
; SI, DI y ES
bios_bajo:
        push si
        push di
        push cx
        push es
        push bx
        cmp byte [op13], 3
        jne .va
        push ds                     ; escribir: el sector, antes, abajo
        push cx
        mov si, bx
        mov ax, es
        mov es, [seg_bajo]
        mov ds, ax                  ; DS:SI = el sector, ES:DI = b_buf
        mov di, b_buf
        mov cx, 256
        rep movsw
        pop cx
        pop ds
.va:
        mov es, [seg_bajo]
        mov bx, b_buf
        mov ah, [op13]
        mov al, 1
        int 0x13
        pop bx
        pop es                      ; ES:BX = adonde iba
        jc .x
        cmp byte [op13], 2
        jne .bien
        push ds                     ; leer: de abajo, a su sitio
        mov di, bx
        mov si, b_buf
        mov ds, [seg_bajo]
        mov cx, 256
        rep movsw
        pop ds
.bien:
        clc
.x:
        pop cx
        pop di
        pop si
        ret

; «Ponga el disquete de la unidad B:»: se pide por la BIOS (no por la consola,
; que puede estar redirigida) y se espera una tecla. AL = la unidad cuyo
; disquete hace falta. Conserva los registros
pide_disquete:
        push ax
        push bx
        push si
        push bp
        mov [disq_puesto], al
        add al, 'A'
        mov [m_ponga.u], al
        mov si, m_ponga
        call .cad
        xor ah, ah
        int 0x16
        mov si, m_ponga.nl
        call .cad
        pop bp
        pop si
        pop bx
        pop ax
        ret
.cad:
        lodsb
        test al, al
        jz .x
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp .cad
.x:
        ret
m_ponga     db 13, 10, "Ponga el disquete de la unidad "
.u          db "A: y pulse una tecla...", 0
.nl         db 13, 10, 0
; los disquetes sin medidas en su primer sector (los de DOS 1) se conocen por
; el primer octeto de la FAT: FCh 180 K, FDh 360 K, FEh 160 K, FFh 320 K.
; Cada fila: las medidas como van en el disco (13 octetos), sectores por
; pista y caras
medios:
        dw 512
        db 1
        dw 1
        db 2
        dw 64, 360
        db 0xFC
        dw 2
        db 9, 1
        dw 512
        db 2
        dw 1
        db 2
        dw 112, 720
        db 0xFD
        dw 2
        db 9, 2
        dw 512
        db 1
        dw 1
        db 2
        dw 64, 320
        db 0xFE
        dw 1
        db 8, 1
        dw 512
        db 2
        dw 1
        db 2
        dw 112, 640
        db 0xFF
        dw 1
        db 8, 2

; deja el sector AX en `sector` (si no es el que ya está). CF=1, AX=error
sec_trae:
        cmp ax, [buf_lba]
        je .x
        call sec_vuelca
        jc .x1
        push bx
        mov bx, sector
        call lee_sector
        pop bx
        jc .mal
        mov [buf_lba], ax
.x:
        clc
.x1:
        ret
.mal:
        mov word [buf_lba], 0xFFFF
        stc
        ret
; el búfer pasa a ser el del sector AX SIN leerlo (se va a escribir entero)
sec_pone:
        cmp ax, [buf_lba]
        je .x
        call sec_vuelca
        jc .x1
        mov [buf_lba], ax
.x:
        clc
.x1:
        ret
; si el búfer está cambiado, lo escribe. CF=1, AX=error
sec_vuelca:
        cmp byte [buf_sucio], 0
        je .x
        push ax
        push bx
        mov ax, [buf_lba]
        mov bx, sector
        call escribe_sector
        pop bx
        jc .mal
        mov byte [buf_sucio], 0
        pop ax
.x:
        clc
        ret
.mal:
        mov byte [buf_sucio], 0     ; (no se va a poder: que no se repita)
        mov word [buf_lba], 0xFFFF
        add sp, 2
        stc
        ret

; =============================================================================
;  El disquete: sus medidas y su FAT
; =============================================================================
; lee las medidas (la BPB) de la unidad cargada. CF=1 si no hay disco que valga
disco_ini:
        mov byte [disco_ok], 0
        mov byte [buf_sucio], 0
        mov byte [fat_sucia], 0
        mov word [buf_lba], 0xFFFF
        mov word [fat_lba], 0xFFFF
        cmp word [u_dev+2], 0
        je .fis
        call .limpia                ; de un controlador: las medidas que dio
        push ds
        lds si, [u_bpb]
        mov di, sector + 11
        mov cx, 13
        rep movsb
        pop ds
        jmp .cmp
.fis:
        cmp byte [u_bios], 0x80     ; (un disco duro ya tiene sus medidas: las
        jae .lee                    ; de la BIOS)
        mov word [spp], 18          ; (para leer el primer sector de un disquete
        mov word [caras], 2         ; vale cualquier medida: pista 0, cara 0)
.lee:
        xor ax, ax                  ; (del disco, no de la reserva: se trata
        mov bx, sector              ; de saber qué disco hay)
        mov byte [op13], 2
        call disco_es
        jc .mal0
        cmp word [sector+0x0B], 512 ; ¿trae sus medidas? Sectores de 512
        jne .sin_bpb                ; octetos, una o dos FAT y grupos de algo
        mov al, [sector+0x10]
        dec al
        cmp al, 1
        ja .sin_bpb
        cmp byte [sector+0x0D], 0
        jne .cmp
.sin_bpb:
        cmp byte [u_bios], 0x80     ; un disquete sin medidas: por el primer
        jae .mal0                   ; octeto de su FAT
        mov ax, 1
        mov bx, sector
        mov byte [op13], 2
        call disco_es
        jc .mal0
        cmp word [sector+1], 0xFFFF
        jne .mal0
        mov al, [sector]
        sub al, 0xFC
        jb .mal0
        mov ah, 15
        mul ah
        add ax, medios
        push ax
        call .limpia
        pop si
        mov di, sector + 11
        mov cx, 13
        rep movsb
        lodsb
        mov [sector+0x18], al
        lodsb
        mov [sector+0x1A], al
        jmp short .cmp
.mal0:
        jmp .mal
; (interna) `sector`, a cero
.limpia:
        mov di, sector
        mov cx, 256
        xor ax, ax
        rep stosw
        ret
.cmp:
        mov si, sector + 11         ; ¿es otro disco? Entonces, a su raíz, y
        mov di, bpb_ant             ; lo guardado del anterior no vale
        mov cx, 51
        repe cmpsb
        je .igual
        mov word [dir_act], 0
        mov byte [dir_txt], 0
        mov si, sector + 11
        mov di, bpb_ant
        mov cx, 51
        rep movsb
        jmp short .olvida
.igual:
        cmp byte [u_bios], 0x80     ; las mismas medidas: si el disquete no
        jae .mismo                  ; lleva número de serie, puede ser otro
        cmp byte [sector+0x26], 0x29
        je .mismo
.olvida:
        mov al, [unidad_act]
        call cache_olvida
.mismo:
        cmp byte [u_bios], 0x80
        jae .m1
        mov ax, [sector+0x18]
        mov [spp], ax
        mov ax, [sector+0x1A]
        mov [caras], ax
.m1:
        mov al, [sector+0x0D]
        xor ah, ah
        test ax, ax
        jz .mal
        mov [spc], ax
        mov cl, 9
        shl ax, cl
        mov [bpc], ax               ; octetos por grupo
        mov ax, [sector+0x0E]
        mov [res_sec], ax
        mov al, [sector+0x10]
        xor ah, ah
        mov [nfat], ax
        mov ax, [sector+0x16]
        mov [spf], ax
        mul word [nfat]
        add ax, [res_sec]
        mov [raiz_ini], ax
        mov bx, [sector+0x11]
        add bx, 15
        mov cl, 4
        shr bx, cl
        mov [raiz_sec], bx
        add ax, bx
        mov [datos_ini], ax
        mov bx, [sector+0x13]       ; los sectores del disco: 16 bits (o, si es
        test bx, bx                 ; 0, los 32 de más allá, que tienen que
        jnz .tot                    ; caber en 16)
        cmp word [sector+0x22], 0
        jne .mal
        mov bx, [sector+0x20]
.tot:
        sub bx, ax                  ; el último grupo: los sectores de datos
        jbe .mal                    ; entre los que tiene cada grupo, más 1
        mov ax, bx
        xor dx, dx
        div word [spc]
        inc ax
        mov [max_clus], ax
        mov byte [u_fat16], 0       ; menos de 4.085 grupos: FAT12
        cmp ax, 0x0FF6
        jb .tipo
        mov byte [u_fat16], 1
        cmp ax, 0xFFF0
        jae .mal
.tipo:
        mov word [fat_pista], 2
        mov byte [disco_ok], 1
        clc
        ret
.mal:
        mov ax, E_DISCO
        stc
        ret

; AL = unidad (0 = A:) -> esa pasa a ser la cargada: se manda al disco lo
; pendiente, se guardan las variables de la que había y se traen las suyas.
; CF=1 y AX=E_UNIDAD si no existe. Conserva los demás registros
unidad_pon:
        cmp al, [unidad_act]
        je .ya
        cmp al, N_UNI
        jae .mal
        push bx
        mov bl, al
        xor bh, bh
        cmp byte [u_hay+bx], 0
        pop bx
        je .mal
        push ax
        push bx
        push cx
        push si
        push di
        push es
        push cs
        pop es
        push ax
        call disco_vuelca
        mov al, [unidad_act]
        call .sitio
        mov si, u_ini
        mov cx, U_TAM
        rep movsb
        pop ax
        mov [unidad_act], al
        call .sitio
        mov si, di
        mov di, u_ini
        mov cx, U_TAM
        rep movsb
        mov word [buf_lba], 0xFFFF
        mov word [fat_lba], 0xFFFF
        mov byte [buf_sucio], 0
        mov byte [fat_sucia], 0
        pop es
        pop di
        pop si
        pop cx
        pop bx
        pop ax
.ya:
        clc
        ret
.mal:
        mov ax, E_UNIDAD
        stc
        ret
; (interna) AL = unidad -> DI = donde se guardan sus variables
.sitio:
        mov ah, U_TAM
        mul ah
        add ax, u_guardadas
        mov di, ax
        ret

; mira qué unidades hay además de la de arranque: un segundo disquete (B:) y
; la primera partición FAT del primer disco duro (C:)
unidades_ini:
        mov byte [u_hay], 1
        mov byte [u_hay+1], 1
        int 0x11                    ; ¿dos disqueteras?
        test al, 1
        jz .una
        and al, 0xC0
        jz .una
        mov byte [u_guardadas + U_TAM + (u_bios - u_ini)], 1
        jmp short .duro
.una:
        mov byte [una_disq], 1      ; una sola: B: es la misma, con otro
.duro:                              ; disquete (ver disco_es)
; (y también sola: cuando PARTIR o FORMATEA han cambiado el disco duro)
; busca la primera partición FAT del primer disco duro: si la hay y tiene
; formato, existe C:. Entra y sale con A: como unidad cargada
duro_ini:
        mov byte [u_hay+2], 0
        push es
        mov ah, 8                   ; ¿hay disco duro? Sus medidas
        mov dl, 0x80
        int 0x13
        pop es
        jc .x
        test dl, dl
        jz .x
        push cx
        mov di, u_guardadas + 2 * U_TAM     ; todo lo de C:, de nuevo
        mov bx, di
        mov cx, U_TAM
        xor al, al
        rep stosb
        pop cx
        mov byte [bx + (u_bios - u_ini)], 0x80
        and cx, 63
        mov [bx + (spp - u_ini)], cx
        mov cl, dh
        xor ch, ch
        inc cx
        mov [bx + (caras - u_ini)], cx
        mov byte [u_hay+2], 1
        mov al, 2                   ; su tabla de particiones: el primer sector
        call unidad_pon
        xor ax, ax
        mov bx, sector
        call lee_sector
        jc .no
        cmp word [sector+510], 0xAA55
        jne .no
        mov si, sector + 0x1BE
        mov cx, 4
.p:
        mov al, [si+4]              ; FAT12, FAT16, o FAT16 «grande» (que solo
        cmp al, 1                   ; vale si no pasa de 32 megas)
        je .esta
        cmp al, 4
        je .esta
        cmp al, 6
        je .esta
        add si, 16
        loop .p
        jmp short .no
.esta:
        mov ax, [si+8]
        mov [u_base], ax
        mov ax, [si+10]
        mov [u_base+2], ax
        call disco_ini
        jnc .vuelve
.no:
        mov byte [u_hay+2], 0
.vuelve:
        push word [u_hay+2]         ; (de u_hay depende poder volver a A:)
        mov byte [u_hay+2], 1
        xor al, al
        call unidad_pon
        pop ax
        mov [u_hay+2], al
.x:
        ret

; los discos han cambiado por debajo (INT 21h/E8h con AL=1): se manda al disco
; lo pendiente, se olvida lo que se sabía de cada unidad (sus medidas y su
; directorio de trabajo) y se vuelve a mirar el disco duro
discos_nuevos:
        call disco_vuelca
        mov al, 0xFF
        call cache_olvida
        mov al, [unidad_act]
        push ax
        xor bx, bx
.u:
        cmp byte [u_hay+bx], 0
        je .s
        mov al, bl
        call unidad_pon
        mov byte [disco_ok], 0
        mov word [dir_act], 0
        mov byte [dir_txt], 0
.s:
        inc bx
        cmp bx, N_UNI
        jb .u
        xor al, al
        call unidad_pon
        call duro_ini
        pop ax
        call unidad_pon             ; (si ya no existe, se queda en A:)
        mov bl, [unidad_def]
        xor bh, bh
        cmp byte [u_hay+bx], 0
        jne .x
        mov byte [unidad_def], 0
.x:
        ret

; ---- INT 25h e INT 26h: leer y escribir sectores «en crudo», como en DOS ----
; AL = unidad (0 = A:), CX = cuántos, DX = el primero (contado desde el
; principio de la unidad), DS:BX = dónde. Vuelve con CF=0, o con CF=1 y el
; error en AX; como en DOS, deja las banderas de quien llama en la pila (hay
; que quitarlas) y no conserva más registros que los de segmento y la pila
int25:
        mov ah, 2
        jmp short crudo
int26:
        mov ah, 3
crudo:
        sti
        cld
        push ds
        push es
        mov bp, ds
        push cs
        pop ds
        push cs
        pop es
        mov [es_seg], bp            ; dónde (de quien llama)
        mov [es_off], bx
        mov [es_queda], cx          ; cuántos faltan
        mov [es_dsec], dx           ; por cuál se va
        mov [es_n], dx              ; el primero
        mov [es_op], ah
        call unidad_pon
        jc .mal
        call disco_vuelca
        call disco_fresco           ; sus medidas (por si es otro disquete)
        jc .mal
        ; cada sector pasa por el búfer del núcleo: el de quien llama puede
        ; cruzar una frontera de 64 K, y las disqueteras (DMA) no saben
        mov word [buf_lba], 0xFFFF
.b:
        cmp word [es_queda], 0
        je .hecho
        cmp byte [es_op], 3
        jne .lee
        push ds
        mov si, [es_off]
        mov di, sector
        mov cx, 256
        mov ds, [es_seg]
        rep movsw
        pop ds
        mov ax, [es_dsec]
        mov bx, sector
        call escribe_sector
        jc .mal
        jmp short .sig
.lee:
        mov ax, [es_dsec]
        mov bx, sector
        call lee_sector
        jc .mal
        push es
        mov si, sector
        mov di, [es_off]
        mov es, [es_seg]
        mov cx, 256
        rep movsw
        pop es
.sig:
        add word [es_off], 512
        inc word [es_dsec]
        dec word [es_queda]
        jmp .b
.hecho:
        cmp byte [es_op], 3         ; si se ha escrito, la copia de la FAT no
        jne .bien                   ; vale; y si ha sido el primer sector,
        mov word [fat_lba], 0xFFFF  ; tampoco las medidas del disco
        cmp word [es_n], 0
        jne .bien
        mov byte [disco_ok], 0
.bien:
        xor ax, ax
        jmp short .fin
.mal:
        mov bx, 0x0300              ; protegido contra escritura
        cmp ax, E_PROTEGIDO
        je .m1
        mov bx, 0x0201              ; esa unidad no existe
        cmp ax, E_UNIDAD
        je .m1
        mov bx, 0x8002              ; no está preparada
.m1:
        mov ax, bx
        stc
.fin:
        pop es
        pop ds
        retf

; antes de buscar nada en el disco: si no hay archivos abiertos y lleva más
; de dos segundos sin usarse, puede que hayan cambiado el disquete (un XT no
; avisa): se leen otra vez sus medidas y su FAT. CF=1 si no hay disco
disco_fresco:
        cmp byte [u_bios], 0x80     ; (un disco duro no se cambia)
        jae .vale
        cmp byte [abiertos], 0
        jne .vale
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        pop es
        sub ax, [t_disco]
        cmp ax, 37
        jb .vale
        jmp disco_ini
.vale:
        cmp byte [disco_ok], 0      ; si no se sabe nada del disco (no lo
        je disco_ini                ; había, o lo acaban de formatear), se mira
        clc
        ret

; ---- la FAT ----
; trae el sector AX de la FAT a su búfer. CF=1 si falla el disco. Conserva
; los registros
fat_trae:
        cmp ax, [fat_lba]
        je .x
        call fat_vuelca
        jc .x1
        push bx
        mov bx, fat
        call lee_sector
        pop bx
        jc .mal
        mov [fat_lba], ax
.x:
        clc
.x1:
        ret
.mal:
        mov word [fat_lba], 0xFFFF
        stc
        ret
; escribe el sector de la FAT que hay en el búfer (si ha cambiado) en todas
; las copias de la FAT. CF=1, AX=error
fat_vuelca:
        cmp byte [fat_sucia], 0
        je .x
        push bx
        push cx
        mov byte [fat_sucia], 0
        mov ax, [fat_lba]
        mov bx, fat
        mov cx, [nfat]
.c:
        call escribe_sector
        jc .mal
        add ax, [spf]
        loop .c
        clc
.mal:
        pop cx
        pop bx
        ret
.x:
        clc
        ret
; (interna) BX = un octeto de la FAT, contado desde su principio -> SI = su
; sitio en el búfer. CF=1 si falla el disco. Conserva lo demás
fat_octeto:
        push ax
        push cx
        mov ax, bx
        mov cl, 9
        shr ax, cl
        add ax, [res_sec]
        call fat_trae
        mov si, bx
        and si, 511
        lea si, [fat+si]
        pop cx
        pop ax
        ret
; AX=grupo -> AX=el que le sigue (menos de 2 o más que [max_clus]: es el
; último). Conserva lo demás
fat_lee:
        push bx
        push cx
        push si
        cmp byte [u_fat16], 0
        je .doce
        mov bx, ax                  ; FAT16: una palabra por grupo
        shl bx, 1
        jc .alta
        call fat_octeto
        jc .mal
        jmp short .p16
.alta:
        push ax                     ; (más allá de los primeros 64 K de la FAT)
        mov ax, bx
        mov cl, 9
        shr ax, cl
        add ax, 128
        add ax, [res_sec]
        call fat_trae
        pop ax
        jc .mal
        mov si, bx
        and si, 511
        lea si, [fat+si]
.p16:
        mov ax, [si]
        jmp short .x
.doce:
        mov bx, ax                  ; FAT12: doce bits, en el octeto n*3/2
        shr bx, 1
        add bx, ax
        mov ch, al                  ; (¿par o impar?)
        call fat_octeto
        jc .mal
        mov al, [si]
        inc bx
        call fat_octeto             ; (el segundo puede caer en otro sector)
        jc .mal
        mov ah, [si]
        test ch, 1
        jz .par
        mov cl, 4
        shr ax, cl
.par:
        and ax, 0x0FFF
        cmp ax, 0x0FF8              ; el fin de cadena, igual en las dos FAT
        jb .x
.mal:
        mov ax, 0xFFFF
.x:
        pop si
        pop cx
        pop bx
        ret
; pone DX en la casilla del grupo AX. Conserva los registros
fat_pon:
        push ax
        push bx
        push cx
        push dx
        push si
        cmp byte [u_fat16], 0
        je .doce
        mov bx, ax
        shl bx, 1
        jc .alta
        call fat_octeto
        jc .x
        jmp short .p16
.alta:
        mov ax, bx
        mov cl, 9
        shr ax, cl
        add ax, 128
        add ax, [res_sec]
        call fat_trae
        jc .x
        mov si, bx
        and si, 511
        lea si, [fat+si]
.p16:
        mov [si], dx
        mov byte [fat_sucia], 1
        jmp short .x
.doce:
        mov bx, ax
        shr bx, 1
        add bx, ax
        and dx, 0x0FFF
        test al, 1
        jnz .impar
        call fat_octeto             ; par: el octeto entero y la mitad baja del
        jc .x                       ; siguiente
        mov [si], dl
        mov byte [fat_sucia], 1
        inc bx
        call fat_octeto
        jc .x
        and byte [si], 0xF0
        or [si], dh
        mov byte [fat_sucia], 1
        jmp short .x
.impar:
        mov cl, 4                   ; impar: la mitad alta de uno y el siguiente
        shl dx, cl                  ; entero
        call fat_octeto
        jc .x
        and byte [si], 0x0F
        or [si], dl
        mov byte [fat_sucia], 1
        inc bx
        call fat_octeto
        jc .x
        mov [si], dh
        mov byte [fat_sucia], 1
.x:
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; busca un grupo libre (desde el último que se dio, para no repasar la FAT
; entera cada vez) y lo marca como último de una cadena -> AX. CF=1 si el
; disco está lleno
fat_nuevo:
        push bx
        push cx
        push dx
        mov cx, [max_clus]
        dec cx                      ; cuántos se miran: todos
        mov bx, [fat_pista]
.b:
        cmp bx, [max_clus]
        jbe .mira
        mov bx, 2
.mira:
        mov ax, bx
        call fat_lee
        test ax, ax
        jz .libre
        inc bx
        loop .b
        mov ax, E_LLENO
        stc
        jmp short .x
.libre:
        mov ax, bx
        mov dx, 0xFFFF
        call fat_pon
        inc bx
        mov [fat_pista], bx
        clc
.x:
        pop dx
        pop cx
        pop bx
        ret
; suelta la cadena que empieza en el grupo AX
fat_suelta:
        push ax
        push bx
        push dx
.b:
        cmp ax, 2
        jb .x
        cmp ax, [max_clus]
        ja .x
        cmp ax, [fat_pista]         ; (y por ahí se vuelve a buscar)
        jae .s
        mov [fat_pista], ax
.s:
        mov bx, ax
        call fat_lee
        xchg ax, bx                 ; AX=este, BX=el siguiente
        xor dx, dx
        call fat_pon
        mov ax, bx
        jmp .b
.x:
        pop dx
        pop bx
        pop ax
        ret
; AX=grupo -> AX=su primer sector
clus_lba:
        push dx
        sub ax, 2
        mul word [spc]
        add ax, [datos_ini]
        pop dx
        ret
; pone a cero todos los sectores del grupo AX (un directorio nuevo). CF=1
clus_cero:
        push bx
        push cx
        push di
        call clus_lba
        mov bx, ax
        call sec_vuelca
        jc .x
        mov di, sector
        mov cx, 256
        xor ax, ax
        rep stosw
        mov cx, [spc]
.s:
        mov [buf_lba], bx
        mov byte [buf_sucio], 1
        call sec_vuelca
        jc .x
        inc bx
        loop .s
        mov word [buf_lba], 0xFFFF
        clc
.x:
        pop di
        pop cx
        pop bx
        ret
; todo lo pendiente, al disco. CF=1, AX=error
disco_vuelca:
        call sec_vuelca
        jc .x
        call fat_vuelca
.x:
        ret

; =============================================================================
;  Directorios
; =============================================================================
; empieza a recorrer el directorio del grupo AX (0: la raíz)
dir_abre:
        test ax, ax
        jnz .sub
        mov word [di_clus], 0
        mov ax, [raiz_ini]
        mov [di_sec], ax
        mov ax, [raiz_sec]
        jmp short .c
.sub:
        mov [di_clus], ax
        call clus_lba
        mov [di_sec], ax
        mov ax, [spc]
.c:
        mov [di_resto], ax
        mov word [di_off], -32
        ret
; la entrada siguiente -> SI (dentro de `sector`; el sector es [di_sec]).
; CF=1 si se ha acabado el directorio (AX=0) o falla el disco (AX=error)
dir_sig:
        add word [di_off], 32
        cmp word [di_off], 512
        jb .ent
        mov word [di_off], 0
        inc word [di_sec]
        dec word [di_resto]
        jnz .ent
        cmp word [di_clus], 0       ; se acabó el tramo: ¿hay otro grupo?
        je .fin
        mov ax, [di_clus]
        call fat_lee
        cmp ax, 2
        jb .fin
        cmp ax, [max_clus]
        ja .fin
        mov [di_clus], ax
        call clus_lba
        mov [di_sec], ax
        mov ax, [spc]
        mov [di_resto], ax
.ent:
        mov ax, [di_sec]
        call sec_trae
        jc .x
        mov si, sector
        add si, [di_off]
        clc
        ret
.fin:
        mov word [di_resto], 1      ; (por si se vuelve a llamar)
        mov word [di_off], 512 - 32
        xor ax, ax
        stc
.x:
        ret

; ¿casa la entrada SI con el patrón nom11 («?» vale por cualquier letra)?
; ZF=1 si casa. Conserva los registros
nom_casa:
        push ax
        push bx
        xor bx, bx
.c:
        mov al, [nom11+bx]
        cmp al, '?'
        je .s
        cmp al, [si+bx]
        jne .x
.s:
        inc bx
        cmp bx, 11
        jb .c
        cmp al, al
.x:
        pop bx
        pop ax
        ret

; busca nom11 en el directorio del grupo AX -> SI=su entrada. No ve las
; borradas, los nombres largos ni la etiqueta. CF=1 si no está (AX=0) o falla
; el disco
dir_busca:
        call dir_abre
.b:
        call dir_sig
        jc .x
        mov al, [si]
        test al, al
        jz .no
        cmp al, 0xE5
        je .b
        test byte [si+11], 0x08
        jnz .b
        call nom_casa
        jne .b
        clc
        ret
.no:
        xor ax, ax
        stc
.x:
        ret

; busca un hueco en el directorio del grupo AX -> SI (y el sector apuntado
; como cambiado). Un subdirectorio lleno crece un grupo; la raíz, no.
; CF=1, AX=error
dir_hueco:
        mov [dh_dir], ax
        call dir_abre
.b:
        call dir_sig
        jc .fin
        mov al, [si]
        test al, al
        jz .hay
        cmp al, 0xE5
        jne .b
.hay:
        mov byte [buf_sucio], 1
        clc
        ret
.fin:
        test ax, ax
        jnz .mal                    ; (fallo del disco)
        cmp word [dh_dir], 0
        je .lleno
        call fat_nuevo              ; otro grupo para el subdirectorio
        jc .lleno
        push ax
        mov dx, ax
        mov ax, [di_clus]
        call fat_pon
        pop ax
        push ax
        call clus_cero
        pop ax
        jc .mal
        call clus_lba
        mov [di_sec], ax
        call sec_trae
        jc .mal
        mov si, sector
        jmp .hay
.lleno:
        mov ax, E_ACCESO
.mal:
        stc
        ret

; =============================================================================
;  Rutas y nombres
; =============================================================================
; AL, a mayúscula (también la eñe y las vocales con tilde de la página 437)
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .otras
        sub al, 32
.x:
        ret
.otras:
        cmp al, 0xA4                ; ñ -> Ñ
        jne .x
        inc al
        ret

; un trozo de ruta de DS:SI -> nom11, como en el disco; SI queda en lo que lo
; acaba (\, / o el final). «*» vale por «?» hasta el final de su parte.
; Conserva los demás registros
nombre_fat:
        push ax
        push cx
        push di
        mov di, nom11
        mov cx, 11
        mov al, ' '
        rep stosb
        mov di, nom11
        cmp byte [si], '.'          ; «.» y «..»: tal cual
        jne .nom
.pt:
        cmp byte [si], '.'
        jne .x
        movsb
        jmp .pt
.nom:
        mov cx, 8
.n:
        mov al, [si]
        call .fin
        je .x
        inc si
        cmp al, '.'
        je .ext
        cmp al, '*'
        je .ast
        jcxz .n                     ; (lo que pasa de ocho letras se pierde)
        call mayus
        stosb
        dec cx
        jmp .n
.ast:
        mov al, '?'
        rep stosb
        jmp .n
.ext:
        mov di, nom11 + 8
        mov cx, 3
.e:
        mov al, [si]
        call .fin
        je .x
        inc si
        cmp al, '*'
        je .east
        jcxz .e
        call mayus
        stosb
        dec cx
        jmp .e
.east:
        mov al, '?'
        rep stosb
        jmp .e
.x:
        pop di
        pop cx
        pop ax
        ret
; (interna) ZF=1 si AL acaba el trozo
.fin:
        cmp al, ' '
        jbe .f1
        cmp al, '\'
        je .f2
        cmp al, '/'
.f2:
        ret
.f1:
        cmp al, al
        ret

; la ruta de `ruta` (acabada en 0) -> [r_dir] = el grupo del directorio que
; contiene lo último, y nom11 = lo último (vacío si la ruta acaba en «\»).
; CF=1 y AX=E_RUTA si algún directorio del camino no existe
ruta_resuelve:
        mov si, ruta
        call ruta_unidad
        jc .x
        push si                     ; (si relee el disco, no conserva nada)
        call disco_fresco
        pop si
        jc .x
        mov ax, [dir_act]
        cmp byte [si], '\'
        je .raiz
        cmp byte [si], '/'
        jne .d
.raiz:
        xor ax, ax
        inc si
.d:
        mov [r_dir], ax
.trozo:
        call nombre_fat
        mov al, [si]
        cmp al, '\'
        je .entra
        cmp al, '/'
        je .entra
        clc                         ; era lo último
        ret
.entra:
        inc si
        cmp byte [nom11], ' '       ; («\\»: nada)
        je .trozo
        cmp word [r_dir], 0         ; en la raíz no hay «.» ni «..»
        jne .busca
        cmp word [nom11], '. '
        je .trozo
        cmp word [nom11], '..'
        je .no
.busca:
        push si
        mov ax, [r_dir]
        call dir_busca
        jc .no1
        test byte [si+11], 0x10
        jz .no1
        mov ax, [si+26]
        mov [r_dir], ax
        pop si
        jmp .trozo
.no1:
        pop si
.no:
        mov ax, E_RUTA
        stc
.x:
        ret

; la unidad de la ruta de SI («C:», o la de trabajo si no la dice) pasa a ser
; la cargada, y SI queda detrás de ella. CF=1, AX=E_UNIDAD si no existe
ruta_unidad:
        mov al, [unidad_def]
        cmp byte [si+1], ':'
        jne .u
        mov al, [si]
        call mayus
        sub al, 'A'
        add si, 2
.u:
        jmp unidad_pon

; copia la ruta de ES:SI (de quien llama) a `ruta`. Conserva los registros
ruta_copia:
        push ax
        push cx
        push si
        push di
        mov di, ruta
        mov cx, RUTA_MAX - 1
.c:
        mov al, [es:si]
        test al, al
        jz .f
        mov [di], al
        inc si
        inc di
        loop .c
.f:
        mov byte [di], 0
        pop di
        pop si
        pop cx
        pop ax
        ret

; la entrada SI -> su nombre «NOMBRE.EXT», acabado en 0, en ES:DI (que avanza
; hasta el 0). Conserva los demás registros
nombre_txt:
        push ax
        push bx
        push cx
        xor bx, bx
.n:
        mov al, [si+bx]
        cmp al, ' '
        je .ne
        stosb
        inc bx
        cmp bx, 8
        jb .n
.ne:
        cmp byte [si+8], ' '
        je .f
        mov al, '.'
        stosb
        mov bx, 8
.e:
        mov al, [si+bx]
        cmp al, ' '
        je .f
        stosb
        inc bx
        cmp bx, 11
        jb .e
.f:
        mov byte [es:di], 0
        pop cx
        pop bx
        pop ax
        ret

; la fecha y la hora, como se guardan en el disco -> DX=fecha CX=hora. Con el
; reloj de un AT, las suyas; sin él (PC y XT), la hora que lleva la BIOS desde
; que se encendió y el 1 de enero de 1980
ahora:
        push ax
        push bx
        mov ah, 4                   ; la fecha del reloj: CX=año DH=mes DL=día (BCD)
        stc
        int 0x1A
        jc .sin
        cmp dl, 0
        je .sin
        mov al, cl                  ; el año
        call .bcd
        mov bl, al
        mov al, ch
        call .bcd
        mov ah, 100
        mul ah
        xor bh, bh
        add ax, bx
        sub ax, 1980
        jb .sin
        mov bx, ax                  ; BX=años desde 1980
        mov al, dh
        call .bcd
        mov bh, al                  ; BH=mes
        mov al, dl
        call .bcd                   ; AL=día
        xor ah, ah
        mov cl, 9
        xor dx, dx
        mov dl, bl
        shl dx, cl
        mov bl, bh
        xor bh, bh
        mov cl, 5
        shl bx, cl
        or dx, bx
        or dx, ax
        push dx
        mov ah, 2                   ; la hora: CH=hora CL=minuto DH=segundo (BCD)
        int 0x1A
        mov al, ch
        call .bcd
        mov bh, al
        mov al, cl
        call .bcd
        mov bl, al
        mov al, dh
        call .bcd
        shr al, 1                   ; (de dos en dos segundos)
        xor ah, ah
        push ax
        mov al, bl
        mov cl, 5
        shl ax, cl
        mov dx, ax
        mov al, bh
        xor ah, ah
        mov cl, 11
        shl ax, cl
        or dx, ax
        pop ax
        or dx, ax
        mov cx, dx
        pop dx
        jmp short .x
.sin:
        xor ah, ah                  ; los tics de la BIOS (18,2 por segundo)
        int 0x1A                    ; CX:DX; una hora son 65.543 tics
        mov ax, dx
        mov dx, cx
        mov bx, 1092                ; un minuto: 1.092 tics
        cmp dx, bx
        jae .cero
        div bx                      ; AX=minutos desde medianoche
        xor dx, dx
        mov bx, 60
        div bx                      ; AX=hora DX=minuto
        mov cl, 11
        shl ax, cl
        mov cl, 5
        shl dx, cl
        or ax, dx
        mov cx, ax
        jmp short .f80
.cero:
        xor cx, cx
.f80:
        mov dx, [fecha_xt]          ; la que se haya dado con FECHA
.x:
        pop bx
        pop ax
        ret
.bcd:
        push cx
        mov ah, al
        and al, 0x0F
        mov cl, 4
        shr ah, cl
        mov cl, al
        mov al, 10
        mul ah
        add al, cl
        pop cx
        ret

; =============================================================================
;  Archivos abiertos
; =============================================================================
; busca una ficha libre -> BX. CF=1, AX=E_MUCHOS si no hay
h_libre:
        push cx
        mov bx, fichas
        mov cl, [n_arch]
        xor ch, ch
.b:
        cmp byte [bx+H_USO], 0
        je .hay
        add bx, H_TAM_F
        loop .b
        mov ax, E_MUCHOS
        stc
        jmp short .x
.hay:
        push di
        push ax
        mov di, bx
        mov cx, H_TAM_F
        xor al, al
        rep stosb
        pop ax
        pop di
        clc
.x:
        pop cx
        ret
; AX=número de archivo -> BX=su ficha. CF=1, AX=E_NUMERO si no es de archivo
h_ficha:
        sub ax, PRIMERO
        jb .mal
        cmp al, [n_arch]
        jae .mal
        test ah, ah
        jnz .mal
        mov bx, H_TAM_F
        push dx
        mul bx
        pop dx
        mov bx, ax
        add bx, fichas
        cmp byte [bx+H_USO], 0
        je .mal
        clc
        ret
.mal:
        mov ax, E_NUMERO
        stc
        ret
; BX=ficha -> AX=su número de archivo
h_numero:
        push bx
        push dx
        mov ax, bx
        sub ax, fichas
        mov bl, H_TAM_F
        div bl
        xor ah, ah
        add ax, PRIMERO
        pop dx
        pop bx
        ret

; el grupo número AX (0: el primero) del archivo de la ficha BX -> AX. Con
; DL=1, si la cadena no llega tan lejos, se alarga. CF=1 si no llega (o el
; disco está lleno). Conserva BX
h_grupo:
        push cx
        push si
        mov si, ax                  ; el que se busca
        cmp word [bx+H_PRIM], 0
        jne .hay
        test dl, dl
        jz .no
        call fat_nuevo
        jc .no
        mov [bx+H_PRIM], ax
        mov byte [bx+H_SUCIO], 1
        mov word [bx+H_CCLU], 0
.hay:
        xor cx, cx                  ; por dónde se empieza: por el apuntado,
        mov ax, [bx+H_PRIM]         ; si no está más allá, o por el primero
        cmp word [bx+H_CCLU], 0
        je .anda
        cmp si, [bx+H_CIDX]
        jb .anda
        mov cx, [bx+H_CIDX]
        mov ax, [bx+H_CCLU]
.anda:
        cmp cx, si
        jae .ya
        push ax
        call fat_lee
        cmp ax, 2
        jb .fin
        cmp ax, [max_clus]
        ja .fin
        add sp, 2
        inc cx
        jmp .anda
.fin:
        pop ax                      ; AX=el último que hay
        test dl, dl
        jz .no
        push dx
        push ax
        call fat_nuevo
        mov dx, ax
        pop ax
        jc .lleno
        call fat_pon
        mov ax, dx
        pop dx
        inc cx
        jmp .anda
.lleno:
        pop dx
.no:
        stc
        jmp short .x
.ya:
        mov [bx+H_CCLU], ax
        mov [bx+H_CIDX], cx
        clc
.x:
        pop si
        pop cx
        ret

; lee ([es_op]=0) o escribe (1) CX octetos del archivo de la ficha BX, por
; donde va, de o en [es_seg]:[es_off] -> AX=los que han sido (menos si se
; acaba el archivo o el disco). CF=1, AX=error si falla el disco
h_es:
        cmp byte [bx+H_USO], 2      ; un dispositivo: se le pide a él
        jne .arch
        mov ax, [bx+H_TAM]
        mov dx, [bx+H_TAM+2]
        jmp dev_es
.arch:
        mov al, [bx+H_UNI]
        call unidad_pon
        mov [es_queda], cx
        mov word [es_hecho], 0
.otro:
        mov cx, [es_queda]
        test cx, cx
        jnz .sigue
        jmp .fin
.sigue:
        cmp byte [es_op], 0
        jne .n
        mov ax, [bx+H_TAM]          ; al leer: no más de lo que queda
        mov dx, [bx+H_TAM+2]
        sub ax, [bx+H_POS]
        sbb dx, [bx+H_POS+2]
        jb .fin
        jnz .n
        test ax, ax
        jz .fin
        cmp ax, cx
        jae .n
        mov cx, ax
.n:
        mov ax, [bx+H_POS]          ; dentro del sector: desde dónde y cuánto
        and ax, 511
        mov [es_dsec], ax
        mov dx, 512
        sub dx, ax
        cmp cx, dx
        jbe .n1
        mov cx, dx
.n1:
        mov [es_n], cx
        mov ax, [bx+H_POS]          ; qué grupo y qué sector de él
        mov dx, [bx+H_POS+2]
        cmp dx, [bpc]
        jae .fin                    ; (más allá de lo que cabe en un disquete)
        div word [bpc]
        push dx
        mov dl, [es_op]
        call h_grupo
        pop dx
        jc .fin
        call clus_lba
        mov cl, 9
        shr dx, cl
        add ax, dx
        mov byte [cache_mete], 0    ; (los datos no entran en la reserva)
        cmp byte [es_op], 0         ; un sector que se va a escribir entero no
        je .trae                    ; hace falta leerlo antes
        cmp word [es_n], 512
        jne .trae
        call sec_pone
        jmp short .t1
.trae:
        call sec_trae
.t1:
        mov byte [cache_mete], 1
        jc .mal
        push es
        push ds
        push si
        push di
        mov cx, [es_n]
        cmp byte [es_op], 0
        jne .esc
        mov si, sector              ; leer: del búfer a quien llama
        add si, [es_dsec]
        mov di, [es_off]
        mov es, [es_seg]
        rep movsb
        jmp short .c
.esc:
        mov byte [buf_sucio], 1     ; escribir: de quien llama al búfer
        mov di, sector
        add di, [es_dsec]
        mov si, [es_off]
        mov ds, [es_seg]
        rep movsb
.c:
        pop di
        pop si
        pop ds
        pop es
        mov ax, [es_n]
        add [es_off], ax
        add [es_hecho], ax
        sub [es_queda], ax
        add [bx+H_POS], ax
        adc word [bx+H_POS+2], 0
        cmp byte [es_op], 0
        je .otro
        mov ax, [bx+H_POS]          ; al escribir: ¿ha crecido?
        mov dx, [bx+H_POS+2]
        cmp dx, [bx+H_TAM+2]
        jb .otro
        ja .crece
        cmp ax, [bx+H_TAM]
        jbe .s
.crece:
        mov [bx+H_TAM], ax
        mov [bx+H_TAM+2], dx
.s:
        mov byte [bx+H_SUCIO], 1
        jmp .otro
.fin:
        mov ax, [es_hecho]
        clc
        ret
.mal:
        stc
        ret

; deja el archivo de la ficha BX del tamaño de por donde va (escribir cero
; octetos): suelta lo que sobra de la cadena, o la alarga. CF=1, AX=error
h_corta:
        cmp byte [bx+H_USO], 2      ; (a un dispositivo no se le corta nada)
        jne .arch
        clc
        ret
.arch:
        mov al, [bx+H_UNI]
        call unidad_pon
        mov byte [bx+H_SUCIO], 1
        mov word [bx+H_CCLU], 0
        mov ax, [bx+H_POS]
        mov dx, [bx+H_POS+2]
        mov [bx+H_TAM], ax
        mov [bx+H_TAM+2], dx
        mov cx, ax
        or cx, dx
        jnz .algo
        mov ax, [bx+H_PRIM]         ; vacío: toda la cadena, fuera
        call fat_suelta
        mov word [bx+H_PRIM], 0
        clc
        ret
.algo:
        sub ax, 1                   ; el grupo del último octeto
        sbb dx, 0
        cmp dx, [bpc]
        jae .mal
        div word [bpc]
        mov dl, 1
        call h_grupo
        jc .mal
        push ax
        call fat_lee                ; lo de detrás, fuera
        call fat_suelta
        pop ax
        mov dx, 0xFFFF
        call fat_pon
        clc
        ret
.mal:
        mov ax, E_ACCESO
        stc
        ret

; cierra el archivo de la ficha BX: pone al día su entrada del directorio y
; lo manda todo al disco. CF=1, AX=error
h_cierra:
        cmp bx, [red_sal]           ; (si era la salida o la entrada de la
        jne .r1                     ; consola, deja de serlo)
        mov word [red_sal], 0
.r1:
        cmp bx, [red_ent]
        jne .r2
        mov word [red_ent], 0
.r2:
        cmp byte [bx+H_USO], 2      ; un dispositivo: no hay nada que guardar
        jne .arch
        mov byte [bx+H_USO], 0
        clc
        ret
.arch:
        mov al, [bx+H_UNI]
        call unidad_pon
        cmp word [bx+H_FECHA], 0    ; (si se le ha puesto fecha, hay que
        jne .guarda                 ; guardarla aunque no se haya escrito)
        cmp byte [bx+H_SUCIO], 0
        je .libre
.guarda:
        mov ax, [bx+H_DSEC]
        call sec_trae
        jc .mal
        mov si, sector
        add si, [bx+H_DOFF]
        mov ax, [bx+H_PRIM]
        mov [si+26], ax
        mov ax, [bx+H_TAM]
        mov [si+28], ax
        mov ax, [bx+H_TAM+2]
        mov [si+30], ax
        or byte [si+11], 0x20       ; cambiado desde la última copia
        call ahora
        cmp word [bx+H_FECHA], 0
        je .hoy
        mov dx, [bx+H_FECHA]
        mov cx, [bx+H_HORA]
.hoy:
        mov [si+22], cx
        mov [si+24], dx
        mov byte [buf_sucio], 1
.libre:
        mov byte [bx+H_USO], 0
        dec byte [abiertos]
        jmp disco_vuelca
.mal:
        mov byte [bx+H_USO], 0
        dec byte [abiertos]
        stc
        ret

; cierra los archivos que abrió el programa DX (cuando termina). Conserva DX
h_cierra_de:
        mov bx, fichas
        mov cl, [n_arch]
        xor ch, ch
.b:
        cmp byte [bx+H_USO], 0
        je .s
        cmp [bx+H_PSP], dx
        jne .s
        push dx
        push bx
        push cx
        call h_cierra
        pop cx
        pop bx
        pop dx
.s:
        add bx, H_TAM_F
        loop .b
        ret
