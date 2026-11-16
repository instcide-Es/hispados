; =============================================================================
;  Hispa-8086 - INT 21h: archivos, directorios, fecha y hora
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Las funciones de INT 21h que tocan el disco, sobre disco.inc. Cada una
;  recibe en BP los registros de quien llama (R_AX, R_BX... en nucleo.asm), con
;  DS = ES = el núcleo, y si falla salta a f_error con el código en AX: quien
;  llama recibe el acarreo y ese código, como en MS-DOS.
;
;    0Dh  mandar al disco lo pendiente      3Bh  cambiar de directorio
;    1Ah  dónde se dejan las búsquedas      3Ch  crear un archivo
;    2Ah  la fecha       2Ch  la hora       3Dh  abrirlo     3Eh  cerrarlo
;    2Fh  dónde se dejan las búsquedas      3Fh  leer        40h  escribir
;    36h  sitio libre en el disco           41h  borrarlo    42h  moverse por él
;    39h  crear un directorio               43h  sus atributos
;    3Ah  quitarlo                          47h  el directorio de trabajo
;    4Eh, 4Fh  buscar archivos              56h  cambiarle el nombre
; =============================================================================

; el error AX, a quien llama
f_error:
        or byte [bp+R_BAND], 1
        mov [bp+R_AX], ax
        ret
f_acceso:
        mov ax, E_ACCESO
        jmp f_error
f_no_archivo:
        test ax, ax                 ; (si era un fallo del disco, ese)
        jnz f_error
        mov ax, E_ARCHIVO
        jmp f_error

; la ruta de quien llama (DS:DX) -> `ruta`, resuelta. CF=1, AX=error
f_ruta:
        mov es, [bp+R_DS]
        mov si, [bp+R_DX]
        call ruta_copia
        push cs
        pop es
        jmp ruta_resuelve
; lo mismo, y además tiene que acabar en un nombre sin comodines
f_ruta_nombre:
        call f_ruta
        jc .x
        cmp byte [nom11], ' '
        je .mal
        push cx
        push di
        mov di, nom11
        mov cx, 11
        mov al, '?'
        repne scasb
        pop di
        pop cx
        je .mal
        clc
        ret
.mal:
        mov ax, E_ARCHIVO
        stc
.x:
        ret

; ---- 0Dh: lo pendiente, al disco ----
f0D:
        jmp disco_vuelca

; ---- 1Ah y 2Fh: la zona donde 4Eh y 4Fh dejan lo que encuentran (DTA) ----
f1A:
        mov ax, [bp+R_DS]
        mov [dta_seg], ax
        mov ax, [bp+R_DX]
        mov [dta_off], ax
        ret
f2F:
        mov ax, [dta_seg]
        mov [bp+R_ES], ax
        mov ax, [dta_off]
        mov [bp+R_BX], ax
        ret

; ---- 2Ah: la fecha -> CX=año DH=mes DL=día ----
f2A:
        call ahora
        mov ax, dx
        mov cl, 9
        shr ax, cl
        add ax, 1980
        mov [bp+R_CX], ax
        mov ax, dx
        mov cl, 5
        shr ax, cl
        and al, 0x0F
        and dl, 0x1F
        mov dh, al
        mov [bp+R_DX], dx
        mov byte [bp+R_AX], 0
        ret
; ---- 2Ch: la hora -> CH=hora CL=minuto DH=segundo ----
f2C:
        call ahora                  ; CX = hhhhhmmm mmmsssss (segundos de dos en dos)
        mov ax, cx
        and al, 0x1F
        shl al, 1
        mov dh, al
        xor dl, dl
        mov [bp+R_DX], dx
        mov ax, cx
        mov cl, 5
        shr ax, cl
        and al, 0x3F
        mov dl, al                  ; el minuto
        mov cl, 6
        shr ax, cl
        mov dh, al                  ; la hora
        mov [bp+R_CX], dx
        ret

; ---- 36h: el sitio libre -> AX=sectores por grupo BX=grupos libres
;      CX=octetos por sector DX=grupos del disco ----
f36:
        mov al, [bp+R_DX]           ; DL: 0 la de trabajo, 1 A:, 2 B:...
        call unidad_dl
        jc .no
        call disco_fresco
        jnc .hay
.no:
        mov word [bp+R_AX], 0xFFFF
        ret
.hay:
        xor cx, cx
        mov bx, 2
.b:
        cmp bx, [max_clus]
        ja .f
        mov ax, bx
        call fat_lee
        test ax, ax
        jnz .s
        inc cx
.s:
        inc bx
        jmp .b
.f:
        mov [bp+R_BX], cx
        mov ax, [spc]
        mov [bp+R_AX], ax
        mov word [bp+R_CX], 512
        mov ax, [max_clus]
        dec ax
        mov [bp+R_DX], ax
        ret

; (interna) AL = unidad como la dan 36h y 47h (0: la de trabajo, 1: A:...): pasa
; a ser la cargada. CF=1, AX=E_UNIDAD si no existe
unidad_dl:
        dec al
        jns .u
        mov al, [unidad_def]
.u:
        jmp unidad_pon

; ---- 3Ch: crear un archivo (si ya existe, lo vacía). CX=atributos -> AX ----
f3C:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jc .nuevo
        test byte [si+11], 0x11     ; ni directorios ni los de solo leer
        jnz f_acceso
        mov ax, [si+26]
        call fat_suelta
        jmp short .comun
.nuevo:
        test ax, ax
        jnz f_error
        mov ax, [r_dir]
        call dir_hueco
        jc f_error
        push si
        mov di, si
        mov si, nom11
        mov cx, 11
        rep movsb
        xor al, al
        mov cx, 21
        rep stosb
        pop si
.comun:
        mov al, [bp+R_CX]
        and al, 0x27
        or al, 0x20
        mov [si+11], al
        xor ax, ax
        mov [si+26], ax
        mov [si+28], ax
        mov [si+30], ax
        call ahora
        mov [si+22], cx
        mov [si+24], dx
        mov byte [buf_sucio], 1
        call h_libre
        jc f_error
        mov byte [bx+H_MODO], 2
        jmp short abre_fin

; ---- 3Dh: abrir un archivo. AL=modo (0 leer, 1 escribir, 2 las dos) -> AX ----
f3D:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jc f_no_archivo
        test byte [si+11], 0x10
        jnz f_acceso
        mov dl, [bp+R_AX]
        and dl, 3
        cmp dl, 3
        je f_acceso
        test dl, dl
        jz .ok
        test byte [si+11], 0x01     ; el de solo leer no se abre para escribir
        jnz f_acceso
.ok:
        call h_libre
        jc f_error
        mov [bx+H_MODO], dl
        mov ax, [si+26]
        mov [bx+H_PRIM], ax
        mov ax, [si+28]
        mov [bx+H_TAM], ax
        mov ax, [si+30]
        mov [bx+H_TAM+2], ax
; (común con crear) la ficha BX queda abierta sobre la entrada SI
abre_fin:
        mov byte [bx+H_USO], 1
        mov al, [unidad_act]
        mov [bx+H_UNI], al
        mov ax, [psp_act]
        mov [bx+H_PSP], ax
        mov ax, [di_sec]
        mov [bx+H_DSEC], ax
        mov ax, si
        sub ax, sector
        mov [bx+H_DOFF], ax
        inc byte [abiertos]
        call disco_vuelca
        jc .mal
        call h_numero
        mov [bp+R_AX], ax
        ret
.mal:
        push ax
        mov byte [bx+H_USO], 0
        dec byte [abiertos]
        pop ax
        jmp f_error

; abre para leer el archivo de `ruta` (para el propio núcleo) -> BX=su ficha.
; CF=1, AX=error
abre_ruta:
        call ruta_resuelve
        jc .x
        cmp byte [nom11], ' '
        je .no
        mov ax, [r_dir]
        call dir_busca
        jc .no0
        test byte [si+11], 0x10
        jnz .no
        call h_libre
        jc .x
        mov byte [bx+H_USO], 1
        mov ax, [si+26]
        mov [bx+H_PRIM], ax
        mov ax, [si+28]
        mov [bx+H_TAM], ax
        mov ax, [si+30]
        mov [bx+H_TAM+2], ax
        mov ax, [di_sec]
        mov [bx+H_DSEC], ax
        mov ax, si
        sub ax, sector
        mov [bx+H_DOFF], ax
        mov ax, [psp_act]
        mov [bx+H_PSP], ax
        mov al, [unidad_act]
        mov [bx+H_UNI], al
        inc byte [abiertos]
        clc
        ret
.no0:
        test ax, ax
        jnz .mal
.no:
        mov ax, E_ARCHIVO
.mal:
        stc
.x:
        ret

; ---- 3Eh: cerrar el archivo BX ----
f3E:
        mov ax, [bp+R_BX]
        cmp ax, PRIMERO             ; (la consola no se cierra)
        jb .x
        call h_ficha
        jc f_error
        call h_cierra
        jc f_error
.x:
        ret

; ---- 3Fh: leer CX octetos del archivo BX en DS:DX -> AX=los leídos ----
f3F:
        mov ax, [bp+R_BX]
        cmp ax, PRIMERO
        jb .consola
        call h_ficha
        jc f_error
        cmp byte [bx+H_MODO], 1
        je f_acceso
        mov byte [es_op], 0
        jmp short es_comun
.consola:
        cmp word [red_ent], 0       ; redirigida: del archivo
        je .teclado
        mov bx, [red_ent]
        mov byte [es_op], 0
        jmp short es_comun
.teclado:
        ; de la consola se lee una línea, que acaba en retorno y salto
        mov di, linea
        mov cx, LIN_MAX
        call lee_linea
        call pon_nl
        mov bx, cx
        mov word [linea+bx], 0x0A0D
        add cx, 2
        cmp cx, [bp+R_CX]
        jbe .c1
        mov cx, [bp+R_CX]
.c1:
        mov [bp+R_AX], cx
        mov si, linea
        mov di, [bp+R_DX]
        mov es, [bp+R_DS]
        rep movsb
        ret

; ---- 40h: escribir CX octetos de DS:DX en el archivo BX -> AX=los escritos.
;      Con CX=0, el archivo se queda del tamaño de por donde va ----
f40:
        mov ax, [bp+R_BX]
        cmp ax, PRIMERO
        jb f40_consola
        call h_ficha
        jc f_error
        cmp byte [bx+H_MODO], 0
        je f_acceso
        cmp word [bp+R_CX], 0
        jne .algo
        call h_corta
        jc f_error
        mov word [bp+R_AX], 0
        ret
.algo:
        mov byte [es_op], 1
; (común con leer) la ficha BX, [es_op] puesto
es_comun:
        mov ax, [bp+R_DS]
        mov [es_seg], ax
        mov ax, [bp+R_DX]
        mov [es_off], ax
        mov cx, [bp+R_CX]
        call h_es
        jc f_error
        mov [bp+R_AX], ax
        ret
f40_consola:
        mov cx, [bp+R_CX]
        mov [bp+R_AX], cx
        jcxz .x
        mov si, [bp+R_DX]
        mov es, [bp+R_DS]
.c:
        mov al, [es:si]
        call pon_car_dos
        inc si
        loop .c
.x:
        ret

; ---- 41h: borrar un archivo ----
f41:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jc f_no_archivo
        test byte [si+11], 0x11
        jnz f_acceso
        mov byte [si], 0xE5
        mov byte [buf_sucio], 1
        mov ax, [si+26]
        call fat_suelta
        call disco_vuelca
        jc f_error
        ret

; ---- 42h: moverse por el archivo BX. AL=desde (0 el principio, 1 donde va,
;      2 el final), CX:DX=cuánto -> DX:AX=dónde queda ----
f42:
        mov ax, [bp+R_BX]
        call h_ficha
        jc f_error
        xor ax, ax
        xor dx, dx
        mov cl, [bp+R_AX]
        test cl, cl
        jz .suma
        mov ax, [bx+H_POS]
        mov dx, [bx+H_POS+2]
        cmp cl, 1
        je .suma
        mov ax, [bx+H_TAM]
        mov dx, [bx+H_TAM+2]
        cmp cl, 2
        je .suma
        mov ax, E_FUNCION
        jmp f_error
.suma:
        add ax, [bp+R_DX]
        adc dx, [bp+R_CX]
        mov [bx+H_POS], ax
        mov [bx+H_POS+2], dx
        mov [bp+R_AX], ax
        mov [bp+R_DX], dx
        ret

; ---- 43h: los atributos del archivo. AL=0 leerlos (-> CX), 1 ponerlos (CX) ----
f43:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jc f_no_archivo
        cmp byte [bp+R_AX], 0
        jne .pon
        mov al, [si+11]
        xor ah, ah
        mov [bp+R_CX], ax
        ret
.pon:
        test byte [si+11], 0x10     ; (a un directorio no se le cambian)
        jnz f_acceso
        mov al, [bp+R_CX]
        and al, 0x27
        mov [si+11], al
        mov byte [buf_sucio], 1
        call disco_vuelca
        jc f_error
        ret

; ---- 56h: cambiar el nombre de DS:DX por el de ES:DI (en su mismo
;      directorio) ----
f56:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jc f_no_archivo
        mov ax, [di_sec]            ; dónde está la entrada
        mov [rn_sec], ax
        mov [rn_off], si
        mov ax, [r_dir]
        mov [rn_dir], ax
        mov al, [unidad_act]
        mov [rn_uni], al
        mov es, [bp+R_ES]           ; el nombre nuevo
        mov si, [bp+R_DI]
        call ruta_copia
        push cs
        pop es
        call ruta_resuelve
        jc f_error
        cmp byte [nom11], ' '
        je f_acceso
        mov di, nom11
        mov cx, 11
        mov al, '?'
        repne scasb
        je f_acceso
        mov al, [unidad_act]
        cmp al, [rn_uni]
        jne .otro
        mov ax, [r_dir]
        cmp ax, [rn_dir]
        je .mismo
.otro:
        mov ax, E_OTRO_DISP
        jmp f_error
.mismo:
        call dir_busca              ; no puede haber ya otro con ese nombre
        jnc f_acceso
        test ax, ax
        jnz f_error
        mov ax, [rn_sec]
        call sec_trae
        jc f_error
        mov di, [rn_off]
        mov si, nom11
        mov cx, 11
        rep movsb
        mov byte [buf_sucio], 1
        call disco_vuelca
        jc f_error
        ret

; ---- 39h: crear un directorio ----
f39:
        call f_ruta_nombre
        jc f_error
        mov ax, [r_dir]
        call dir_busca
        jnc f_acceso                ; ya hay algo con ese nombre
        test ax, ax
        jnz f_error
        call fat_nuevo
        jc f_acceso
        mov [rn_dir], ax            ; su grupo
        call clus_cero
        jc .mal
        mov ax, [r_dir]
        call dir_hueco
        jc .mal
        mov di, si
        push si
        mov si, nom11
        mov cx, 11
        rep movsb
        xor al, al
        mov cx, 21
        rep stosb
        pop si
        mov byte [si+11], 0x10
        mov ax, [rn_dir]
        mov [si+26], ax
        call ahora
        mov [si+22], cx
        mov [si+24], dx
        mov byte [buf_sucio], 1
        ; dentro: «.» (él mismo) y «..» (el que lo contiene)
        mov ax, [rn_dir]
        call clus_lba
        call sec_trae
        jc .mal
        mov di, sector
        mov ax, '. '
        stosw
        mov al, ' '
        mov cx, 9
        rep stosb
        mov byte [sector+11], 0x10
        mov ax, [rn_dir]
        mov [sector+26], ax
        mov di, sector + 32
        mov ax, '..'
        stosw
        mov al, ' '
        mov cx, 9
        rep stosb
        mov byte [sector+32+11], 0x10
        mov ax, [r_dir]
        mov [sector+32+26], ax
        call ahora
        mov [sector+22], cx
        mov [sector+24], dx
        mov [sector+32+22], cx
        mov [sector+32+24], dx
        mov byte [buf_sucio], 1
        call disco_vuelca
        jc f_error
        ret
.mal:
        push ax
        mov ax, [rn_dir]
        call fat_suelta
        pop ax
        jmp f_error

; ---- 3Ah: quitar un directorio (vacío, y que no sea el de trabajo) ----
f3A:
        call f_ruta_nombre
        jc .ruta
        mov ax, [r_dir]
        call dir_busca
        jc .ruta
        test byte [si+11], 0x10
        jz .ruta
        mov ax, [si+26]
        cmp ax, [dir_act]
        jne .otro
        mov ax, E_DIR_ACT
        jmp f_error
.otro:
        mov [rn_dir], ax
        mov ax, [di_sec]
        mov [rn_sec], ax
        mov [rn_off], si
        mov ax, [rn_dir]            ; ¿está vacío?
        call dir_abre
.b:
        call dir_sig
        jc .fin
        mov al, [si]
        test al, al
        jz .v1
        cmp al, 0xE5
        je .b
        cmp al, '.'
        je .b
        jmp f_acceso
.fin:
        test ax, ax                 ; (¿se ha acabado, o ha fallado el disco?)
        jnz f_error
.v1:
        mov ax, [rn_sec]
        call sec_trae
        jc f_error
        mov si, [rn_off]
        mov byte [si], 0xE5
        mov byte [buf_sucio], 1
        mov ax, [rn_dir]
        call fat_suelta
        call disco_vuelca
        jc f_error
        ret
.ruta:
        mov ax, E_RUTA
        jmp f_error

; ---- 3Bh: cambiar de directorio ----
; Primero se compone el texto de la ruta nueva (desde la raíz, o desde donde
; se está, quitando «.» y subiendo con «..»); luego se comprueba que existe
f3B:
        mov es, [bp+R_DS]
        mov si, [bp+R_DX]
        call ruta_copia
        push cs
        pop es
        call cd_ruta
        jc f_error
        ret
; la ruta de `ruta` pasa a ser el directorio de trabajo. CF=1, AX=error
cd_ruta:
        mov si, ruta
        call ruta_unidad
        jnc .u
        ret
.u:
        mov di, cd_txt              ; el texto nuevo, sin la barra del principio
        mov byte [di], 0
        cmp byte [si], '\'
        je .raiz
        cmp byte [si], '/'
        je .raiz
        push si                     ; relativa: se parte de donde se está
        mov si, dir_txt
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        dec di
        pop si
        jmp short .trozo
.raiz:
        inc si
.trozo:
        mov al, [si]
        cmp al, ' '
        jbe .fin
        cmp al, '\'
        je .sep
        cmp al, '/'
        je .sep
        cmp word [si], '..'         ; «..»: fuera el último trozo
        jne .punto
        mov al, [si+2]
        call .es_fin
        jne .nombre
        add si, 2
        cmp di, cd_txt
        je .mal                     ; (por encima de la raíz no hay nada)
.quita:
        dec di
        cmp di, cd_txt
        je .q1
        cmp byte [di], '\'
        jne .quita
.q1:
        mov byte [di], 0
        jmp .trozo
.punto:
        cmp byte [si], '.'          ; «.»: nada
        jne .nombre
        mov al, [si+1]
        call .es_fin
        jne .nombre
        inc si
        jmp .trozo
.nombre:
        cmp di, cd_txt              ; un nombre: detrás de una barra, si no es
        je .n1                      ; el primero
        mov al, '\'
        stosb
.n1:
        mov al, [si]
        call .es_fin
        je .n2
        call mayus
        cmp di, cd_txt + DIR_MAX - 1
        jae .mal
        stosb
        inc si
        jmp .n1
.n2:
        mov byte [di], 0
        jmp .trozo
.sep:
        inc si
        jmp .trozo
.fin:
        ; ¿existe? Se resuelve «\texto\»: el directorio queda en [r_dir]
        mov si, cd_txt
        mov di, ruta
        mov al, [unidad_act]
        add al, 'A'
        mov ah, ':'
        stosw
        mov al, '\'
        stosb
.c2:
        lodsb
        test al, al
        jz .c3
        stosb
        jmp .c2
.c3:
        cmp di, ruta + 3
        je .c4
        mov al, '\'
        stosb
.c4:
        mov byte [di], 0
        call ruta_resuelve
        jc .x
        mov ax, [r_dir]
        mov [dir_act], ax
        mov si, cd_txt
        mov di, dir_txt
.c5:
        lodsb
        stosb
        test al, al
        jnz .c5
        clc
        ret
.mal:
        mov ax, E_RUTA
        stc
.x:
        ret
; (interna) ZF=1 si AL acaba un trozo de la ruta
.es_fin:
        cmp al, ' '
        jbe .e1
        cmp al, '\'
        je .e2
        cmp al, '/'
.e2:
        ret
.e1:
        cmp al, al
        ret

; ---- 47h: el directorio de trabajo, en DS:SI (sin la barra del principio) ----
f47:
        mov al, [bp+R_DX]
        call unidad_dl
        jc f_error
        mov es, [bp+R_DS]
        mov di, [bp+R_SI]
        mov si, dir_txt
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        ret

; ---- 4Eh: buscar el primer archivo que case con DS:DX y los atributos CX;
;      4Fh: el siguiente. Lo que se encuentra queda en la zona de búsquedas:
;      21 atributos, 22 hora, 24 fecha, 26 tamaño, 30 el nombre ----
; (los 21 primeros octetos son nuestros: 0 los atributos pedidos, 1 el patrón,
; 12 el directorio, 14 por qué entrada vamos, 16 la unidad)
f4E:
        call f_ruta
        jc f_error
        cmp byte [nom11], ' '
        jne .p
        mov ax, E_NO_MAS
        jmp f_error
.p:
        mov es, [dta_seg]
        mov di, [dta_off]
        mov al, [bp+R_CX]
        stosb
        mov si, nom11
        mov cx, 11
        rep movsb
        mov ax, [r_dir]
        stosw
        xor ax, ax
        stosw
        mov al, [cs:unidad_act]
        stosb
        push cs
        pop es
f4F:
        push ds
        mov si, [dta_off]           ; el patrón y por dónde se iba
        mov ds, [dta_seg]
        lodsb
        mov [cs:bus_atr], al
        mov di, nom11
        mov cx, 11
        rep movsb
        lodsw
        mov bx, ax                  ; el directorio
        lodsw
        mov dx, ax                  ; las entradas ya vistas
        lodsb                       ; y la unidad
        pop ds
        call unidad_pon
        jc f_error
        mov ax, bx
        call dir_abre
        xor cx, cx                  ; las que se llevan vistas ahora
.b:
        call dir_sig
        jc .no
        inc cx
        mov al, [si]
        test al, al
        jz .no0
        cmp cx, dx
        jbe .b
        cmp al, 0xE5
        je .b
        mov al, [si+11]
        cmp al, 0x0F                ; un trozo de nombre largo
        je .b
        test al, 0x08               ; la etiqueta, solo si se pide
        jz .a1
        test byte [bus_atr], 0x08
        jz .b
        jmp short .a2
.a1:
        mov ah, [bus_atr]           ; ocultos, de sistema y directorios, solo
        not ah                      ; si se piden
        and ah, 0x16
        test al, ah
        jnz .b
.a2:
        call nom_casa
        jne .b
        ; este: a la zona de búsquedas
        mov es, [dta_seg]
        mov di, [dta_off]
        mov [es:di+14], cx
        mov al, [si+11]
        mov [es:di+21], al
        mov ax, [si+22]
        mov [es:di+22], ax
        mov ax, [si+24]
        mov [es:di+24], ax
        mov ax, [si+28]
        mov [es:di+26], ax
        mov ax, [si+30]
        mov [es:di+28], ax
        add di, 30
        call nombre_txt
        push cs
        pop es
        ret
.no0:
        xor ax, ax
.no:
        test ax, ax
        jnz f_error
        mov ax, E_NO_MAS
        jmp f_error
