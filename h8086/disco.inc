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
;  Por ahora: una sola unidad (A:, la de arranque) y FAT12. El disco duro y la
;  FAT16 son de la etapa 4 (docs/8086/HISPA-8086.md).
; =============================================================================
N_ARCH      equ 8                   ; archivos abiertos a la vez
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
H_TAM_F     equ 22
; errores de DOS
E_FUNCION   equ 1
E_ARCHIVO   equ 2                   ; no existe el archivo
E_RUTA      equ 3                   ; no existe la ruta
E_MUCHOS    equ 4                   ; demasiados archivos abiertos
E_ACCESO    equ 5                   ; acceso denegado
E_NUMERO    equ 6                   ; número de archivo no válido
E_DIR_ACT   equ 16                  ; no se puede quitar el directorio de trabajo
E_OTRO_DISP equ 17                  ; no es el mismo dispositivo
E_NO_MAS    equ 18                  ; no hay más archivos
E_PROTEGIDO equ 19                  ; disco protegido contra escritura
E_DISCO     equ 21                  ; unidad no preparada
E_LLENO     equ 39                  ; disco lleno (interno)

; =============================================================================
;  Sectores
; =============================================================================
; lee (lee_sector) o escribe (escribe_sector) el sector AX en ES:BX. Tres
; intentos. CF=1 y AX=error si no se puede. Conserva los demás registros
lee_sector:
        mov byte [op13], 2
        jmp short disco_es
escribe_sector:
        mov byte [op13], 3
disco_es:
        push bx
        push cx
        push dx
        push si
        push ax
        mov si, 3
.otra:
        pop ax
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
        mov ah, [op13]
        mov al, 1
        int 0x13
        jnc .bien
        cmp ah, 3                   ; protegido contra escritura: no insistir
        je .prot
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
        pop ax
        clc
.x:
        pop si
        pop dx
        pop cx
        pop bx
        ret

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
; lee las medidas (la BPB) y la FAT. CF=1 si no hay disco que valga
disco_ini:
        mov byte [disco_ok], 0
        mov byte [buf_sucio], 0
        mov byte [fat_sucia], 0
        mov word [buf_lba], 0xFFFF
        mov word [spp], 18          ; (para leer el primer sector vale
        mov word [caras], 2         ; cualquier medida: pista 0, cara 0)
        xor ax, ax
        call sec_trae
        jc .mal
        mov si, sector + 11         ; ¿es otro disco? Entonces, a su raíz
        mov di, bpb_ant
        mov cx, 51
        repe cmpsb
        je .mismo
        mov word [dir_act], 0
        mov byte [dir_txt], 0
        mov si, sector + 11
        mov di, bpb_ant
        mov cx, 51
        rep movsb
.mismo:
        mov ax, [sector+0x18]
        mov [spp], ax
        mov ax, [sector+0x1A]
        mov [caras], ax
        mov al, [sector+0x0D]
        xor ah, ah
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
        cmp ax, FAT_MAX / 512
        ja .mal
        mul word [nfat]
        add ax, [res_sec]
        mov [raiz_ini], ax
        mov bx, [sector+0x11]
        mov cl, 4
        shr bx, cl
        mov [raiz_sec], bx
        add ax, bx
        mov [datos_ini], ax
        mov bx, [sector+0x13]       ; el último grupo: los sectores de datos
        sub bx, ax                  ; entre los que tiene cada grupo, más 1
        mov ax, bx
        xor dx, dx
        div word [spc]
        inc ax
        mov [max_clus], ax
        cmp ax, 0x0FF0              ; (solo FAT12)
        jae .mal
        mov cx, [spf]               ; la FAT, a la memoria
        mov ax, [res_sec]
        mov bx, fat
.f:
        call lee_sector
        jc .mal
        inc ax
        add bx, 512
        loop .f
        mov byte [disco_ok], 1
        clc
        ret
.mal:
        mov ax, E_DISCO
        stc
        ret

; antes de buscar nada en el disco: si no hay archivos abiertos y lleva más
; de dos segundos sin usarse, puede que hayan cambiado el disquete (un XT no
; avisa): se leen otra vez sus medidas y su FAT. CF=1 si no hay disco
disco_fresco:
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
        cmp byte [disco_ok], 1      ; CF=1 si no hay disco
        mov ax, E_DISCO
        ret

; ---- la FAT ----
; AX=grupo -> AX=el que le sigue (0FF8h o más: es el último). Conserva lo demás
fat_lee:
        push bx
        push cx
        mov bx, ax
        shr bx, 1
        add bx, ax
        mov bx, [fat+bx]
        test al, 1
        jz .par
        mov cl, 4
        shr bx, cl
.par:
        and bx, 0x0FFF
        mov ax, bx
        pop cx
        pop bx
        ret
; pone DX en la casilla del grupo AX. Conserva los registros
fat_pon:
        push ax
        push bx
        push cx
        push dx
        mov bx, ax
        shr bx, 1
        add bx, ax
        and dx, 0x0FFF
        test al, 1
        jz .par
        mov cl, 4
        shl dx, cl
        and word [fat+bx], 0x000F
        jmp short .p
.par:
        and word [fat+bx], 0xF000
.p:
        or [fat+bx], dx
        mov byte [fat_sucia], 1
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; busca un grupo libre y lo marca como último de una cadena -> AX. CF=1 si
; el disco está lleno
fat_nuevo:
        push bx
        push dx
        mov bx, 2
.b:
        cmp bx, [max_clus]
        ja .lleno
        mov ax, bx
        call fat_lee
        test ax, ax
        jz .libre
        inc bx
        jmp .b
.libre:
        mov ax, bx
        mov dx, 0x0FFF
        call fat_pon
        clc
        jmp short .x
.lleno:
        mov ax, E_LLENO
        stc
.x:
        pop dx
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
; escribe la FAT (si ha cambiado) en todas sus copias. CF=1, AX=error
fat_vuelca:
        cmp byte [fat_sucia], 0
        je .x
        push bx
        push cx
        push dx
        mov ax, [res_sec]
        mov dx, [nfat]
.copia:
        mov bx, fat
        mov cx, [spf]
.s:
        call escribe_sector
        jc .mal
        inc ax
        add bx, 512
        loop .s
        dec dx
        jnz .copia
        mov byte [fat_sucia], 0
        clc
.mal:
        pop dx
        pop cx
        pop bx
        ret
.x:
        clc
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
        call disco_fresco
        jc .x
        mov si, ruta
        cmp byte [si+1], ':'        ; la unidad: solo hay una
        jne .u
        add si, 2
.u:
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
        mov dx, 0x0021              ; 1 de enero de 1980
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
        mov cx, N_ARCH
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
        cmp ax, N_ARCH
        jae .mal
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
        cmp byte [es_op], 0         ; un sector que se va a escribir entero no
        je .trae                    ; hace falta leerlo antes
        cmp word [es_n], 512
        jne .trae
        call sec_pone
        jmp short .t1
.trae:
        call sec_trae
.t1:
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
        cmp ax, 0x0FF8
        jae .ult
        call fat_suelta
.ult:
        pop ax
        mov dx, 0x0FFF
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
        cmp byte [bx+H_SUCIO], 0
        je .libre
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

; cierra todos los archivos (cuando un programa termina)
h_cierra_todos:
        mov bx, fichas
        mov cx, N_ARCH
.b:
        cmp byte [bx+H_USO], 0
        je .s
        push bx
        push cx
        call h_cierra
        pop cx
        pop bx
.s:
        add bx, H_TAM_F
        loop .b
        mov byte [abiertos], 0
        ret
