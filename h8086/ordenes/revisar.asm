; =============================================================================
;  REVISAR.COM (CHKDSK) - repasa un disco
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       REVISAR [unidad:] [/F]
;
;  Recorre todos los directorios del disco siguiendo la cadena de grupos de
;  cada archivo en la FAT, y dice lo que encuentra:
;    - un archivo cuyo tamaño no concuerda con los grupos que tiene;
;    - una cadena rota (acaba en un grupo que no existe o que está libre);
;    - dos archivos cruzados (comparten un grupo);
;    - grupos perdidos: ocupados en la FAT, pero de ningún archivo.
;  Sin /F solo mira. Con /F corrige: ajusta el tamaño, corta la cadena rota, y
;  guarda cada cadena perdida en un archivo FILE0000.CHK, FILE0001.CHK... en
;  la raíz, para ver si tiene algo que valga (si no, se borran y el sitio
;  queda libre). Los archivos cruzados no los arregla: hay que copiarlos a
;  otro sitio y borrar los dos. Al acabar dice lo que hay en el disco y la
;  memoria total y libre.
;
;  Cómo: la FAT entera se lee a la memoria que hay detrás del programa (hasta
;  128 K), y por cada grupo hay un bit en dos mapas: `usado` (es de algún
;  archivo) y `apuntado` (otro grupo perdido lo señala: no es principio de
;  cadena). Los sectores se leen y se escriben con INT 25h y 26h, y las
;  medidas del disco las da el núcleo (INT 21h/E8h con AL=5), así que vale
;  para cualquier unidad. Devuelve 0 si el disco está bien y 1 si no.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define SOLO_TEXTO

M_SPC       equ 0                   ; las medidas que da el núcleo
M_RES       equ 2
M_NFAT      equ 4
M_SPF       equ 6
M_RAIZ      equ 8
M_RAIZ_SEC  equ 10
M_DATOS     equ 12
M_MAX       equ 14
M_FAT16     equ 16
MALO        equ 0xFFF7
ULTIMO      equ 0xFFFF

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        call pide_nucleo
        DOS 0x19
        mov [unidad], al
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        jne .uni
        mov al, [si+1]
        call mayus
        cmp al, 'F'
        jne .uso
        mov byte [corrige], 1
        add si, 2
        jmp .arg
.uni:
        cmp byte [si+1], ':'
        jne .uso
        call mayus
        sub al, 'A'
        mov [unidad], al
        add si, 2
        jmp .arg
.uso:
        mov si, m_uso
        jmp error
.fin_args:
        mov ax, 0xE805
        mov dl, [unidad]
        mov di, med
        int 0x21
        jnc .mem
        mov si, m_e_unidad
        jmp error
.mem:
        mov ax, [med+M_SPC]         ; octetos por grupo
        mov cl, 9
        shl ax, cl
        mov [bpc], ax
        ; la FAT, detrás de nuestros 64 K: ¿hay memoria?
        mov ax, cs
        add ax, 0x1000
        mov [fat_seg], ax
        mov bx, [med+M_SPF]
        mov cl, 5
        shl bx, cl
        add ax, bx
        jc .sin_mem
        cmp ax, [0x02]              ; (hasta dónde llega nuestra memoria)
        jbe .lee_fat
.sin_mem:
        mov si, m_e_memoria
        jmp error
.lee_fat:
        mov byte [op], 0x25
        call fat_es
        jnc .recorre
        mov si, m_e_leer
        jmp error
.recorre:
        xor ax, ax                  ; desde la raíz
        call revisa_dir
        call perdidos
        cmp byte [fat_sucia], 0
        je .informe
        mov byte [op], 0x26         ; la FAT corregida, a todas sus copias
        mov cx, [med+M_NFAT]
.copia:
        push cx
        call fat_es
        pop cx
        jc .mal_esc
        mov ax, [med+M_SPF]
        add [fat_base], ax
        loop .copia
        jmp short .informe
.mal_esc:
        mov si, m_e_escribir
        jmp error
.informe:
        call informe
        mov al, [errores]
        mov ah, 0x4C
        int 0x21
error:
        call pon_cad
        mov ax, 0x4C01
        int 0x21

; ---- la FAT entera, del disco a la memoria ([op] = 25h) o al revés (26h),
;      de 32 en 32 sectores. CF=1 si falla ----
fat_es:
        mov ax, [med+M_RES]
        add ax, [fat_base]
        mov [fe_sec], ax
        mov ax, [med+M_SPF]
        mov [fe_resto], ax
        mov ax, [fat_seg]
        mov [fe_seg], ax
.b:
        mov cx, [fe_resto]
        jcxz .x                     ; (CF=0)
        cmp cx, 32
        jbe .c
        mov cx, 32
.c:
        sub [fe_resto], cx
        mov dx, [fe_sec]
        add [fe_sec], cx
        mov al, [unidad]
        xor bx, bx
        push ds
        mov ds, [fe_seg]
        cmp byte [cs:op], 0x25
        jne .esc
        int 0x25
        jmp short .hecho
.esc:
        int 0x26
.hecho:
        pop dx                      ; (las banderas que deja en la pila)
        pop ds
        cld
        jc .x
        add word [fe_seg], 0x400
        jmp .b
.x:
        ret

; el grupo que sigue a AX en la FAT -> AX: 0 libre, MALO defectuoso, ULTIMO
; el fin de la cadena. Conserva lo demás
fat_lee:
        push bx
        push cx
        push es
        mov es, [fat_seg]
        cmp byte [med+M_FAT16], 0
        je .doce
        mov bx, ax
        shl bx, 1
        jnc .f
        mov ax, es                  ; (más allá de los primeros 64 K)
        add ax, 0x1000
        mov es, ax
.f:
        mov ax, [es:bx]
        cmp ax, 0xFFF8
        jb .x
        mov ax, ULTIMO
        jmp short .x
.doce:
        mov bx, ax
        shr bx, 1
        add bx, ax
        mov cl, al
        mov ax, [es:bx]
        test cl, 1
        jz .par
        mov cl, 4
        shr ax, cl
.par:
        and ax, 0x0FFF
        cmp ax, 0x0FF7
        jb .x
        mov ax, MALO
        je .x
        mov ax, ULTIMO
.x:
        pop es
        pop cx
        pop bx
        ret
; pone DX (0 o ULTIMO) en la casilla del grupo AX. Conserva los registros
fat_pon:
        push ax
        push bx
        push cx
        push dx
        push es
        mov byte [fat_sucia], 1
        mov es, [fat_seg]
        cmp byte [med+M_FAT16], 0
        je .doce
        mov bx, ax
        shl bx, 1
        jnc .f
        mov ax, es
        add ax, 0x1000
        mov es, ax
.f:
        mov [es:bx], dx
        jmp short .x
.doce:
        mov bx, ax
        shr bx, 1
        add bx, ax
        and dx, 0x0FFF
        test al, 1
        jz .par
        mov cl, 4
        shl dx, cl
        and word [es:bx], 0x000F
        jmp short .p
.par:
        and word [es:bx], 0xF000
.p:
        or [es:bx], dx
.x:
        pop es
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; el bit del grupo AX en el mapa BX: ¿está puesto? (ZF=0 si lo está); mapa_pon
; lo pone. Conservan los registros
mapa_mira:
        push ax
        push bx
        push cx
        call mapa_sitio
        test [bx], al
        pop cx
        pop bx
        pop ax
        ret
mapa_pon:
        push ax
        push bx
        push cx
        call mapa_sitio
        or [bx], al
        pop cx
        pop bx
        pop ax
        ret
mapa_sitio:
        mov cl, al
        and cl, 7
        shr ax, 1
        shr ax, 1
        shr ax, 1
        add bx, ax
        mov al, 1
        shl al, cl
        ret

; lee (lee_sec) o escribe (esc_sec) el sector DX en `sec`. CF=1 si falla.
; Conserva los registros (INT 25h y 26h no conservan casi ninguno)
lee_sec:
        mov byte [op], 0x25
        jmp short sec_es
esc_sec:
        mov byte [op], 0x26
sec_es:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        mov al, [unidad]
        mov cx, 1
        mov bx, sec
        cmp byte [op], 0x25
        jne .esc
        int 0x25
        jmp short .hecho
.esc:
        int 0x26
.hecho:
        pop bp                      ; (las banderas)
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        cld
        ret

; ---- un directorio: AX = su primer grupo (0: la raíz). Mira cada entrada,
;      sigue su cadena y baja a los directorios que tiene ----
revisa_dir:
        mov [d_clus], ax
        test ax, ax
        jnz .sub
        mov ax, [med+M_RAIZ]
        mov [d_sec], ax
        mov ax, [med+M_RAIZ_SEC]
        jmp short .ini
.sub:
        call clus_sec
        mov [d_sec], ax
        mov ax, [med+M_SPC]
.ini:
        mov [d_resto], ax
.sector:
        mov dx, [d_sec]
        call lee_sec
        jnc .leido
        mov si, m_e_leer
        call pon_cad
        mov byte [errores], 1
        ret
.leido:
        mov byte [sec_sucio], 0
        mov word [d_ent], sec
.ent:
        mov si, [d_ent]
        mov al, [si]
        test al, al
        jnz .e0
        jmp .fin_dir                ; detrás del primer 0 no hay nada
.e0:
        cmp al, 0xE5
        je .sig0
        cmp al, '.'
        je .sig0
        mov al, [si+11]
        cmp al, 0x0F
        je .sig0
        test al, 0x08
        jz .e1
.sig0:
        jmp .sig
.e1:
        mov ax, [si+26]
        call cadena                 ; CX = cuántos grupos tiene
        mov si, [d_ent]
        test byte [si+11], 0x10
        jz .archivo
        inc word [n_dirs]
        add [g_dirs], cx
        mov ax, [si+26]             ; adentro (si su cadena empieza bien)
        cmp ax, 2
        jb .sig
        cmp ax, [med+M_MAX]
        ja .sig
        cmp word [nivel], 24
        jae .sig
        call .guarda
        push word [d_clus]          ; este nivel, a la pila; y a la vuelta,
        push word [d_sec]           ; su sector otra vez
        push word [d_resto]
        push word [d_ent]
        inc word [nivel]
        call revisa_dir
        dec word [nivel]
        pop word [d_ent]
        pop word [d_resto]
        pop word [d_sec]
        pop word [d_clus]
        mov dx, [d_sec]
        call lee_sec
        mov byte [sec_sucio], 0
        jmp short .sig
.archivo:
        test byte [si+11], 0x06
        jz .a1
        inc word [n_ocultos]
        add [g_ocultos], cx
        jmp short .a2
.a1:
        inc word [n_arch]
        add [g_arch], cx
.a2:
        ; ¿concuerda el tamaño? Hacen falta (tamaño + bpc - 1) / bpc grupos
        mov ax, [si+28]
        mov dx, [si+30]
        add ax, [bpc]
        adc dx, 0
        sub ax, 1
        sbb dx, 0
        cmp dx, [bpc]               ; (más de lo que cabe en ningún disco)
        jae .mal_tam
        div word [bpc]
        cmp ax, cx
        je .sig
.mal_tam:
        call pon_nombre
        mov si, m_tam
        call pon_cad
        mov byte [errores], 1
        cmp byte [corrige], 0
        je .sig
        mov si, [d_ent]             ; el tamaño, el de sus grupos
        mov ax, cx
        mul word [bpc]
        cmp dx, [si+30]             ; (si le sobran grupos, se deja el
        ja .sig                     ; tamaño: los de más no hacen daño)
        jb .pon_tam
        cmp ax, [si+28]
        jae .sig
.pon_tam:
        mov [si+28], ax
        mov [si+30], dx
        test cx, cx
        jnz .t1
        mov word [si+26], 0
.t1:
        mov byte [sec_sucio], 1
.sig:
        add word [d_ent], 32
        cmp word [d_ent], sec + 512
        jae .otro_sec
        jmp .ent
.otro_sec:
        call .guarda
        inc word [d_sec]
        dec word [d_resto]
        jz .otro_grupo
        jmp .sector
.otro_grupo:
        mov ax, [d_clus]
        test ax, ax
        jz .x                       ; (la raíz se acaba aquí)
        call fat_lee
        cmp ax, 2
        jb .x
        cmp ax, [med+M_MAX]
        ja .x
        jmp revisa_dir.sub0
.fin_dir:
        call .guarda
.x:
        ret
; (interna) si el sector se ha corregido, al disco
.guarda:
        cmp byte [sec_sucio], 0
        je .g
        mov dx, [d_sec]
        call esc_sec
        mov byte [sec_sucio], 0
.g:
        ret
.sub0:
        mov [d_clus], ax
        jmp .sub

; AX = grupo -> AX = su primer sector
clus_sec:
        push dx
        sub ax, 2
        mul word [med+M_SPC]
        add ax, [med+M_DATOS]
        pop dx
        ret

; sigue la cadena que empieza en el grupo AX, marcando sus grupos como usados
; -> CX = cuántos tiene. Dice lo que encuentre mal (la entrada es [d_ent]) y,
; con /F, corta la cadena rota
cadena:
        xor cx, cx
        xor dx, dx                  ; DX = el anterior
        test ax, ax
        jz .x                       ; (un archivo vacío no tiene grupos)
.b:
        cmp ax, 2
        jb .rota
        cmp ax, [med+M_MAX]
        ja .rota
        mov bx, usado
        call mapa_mira
        jnz .cruce
        call mapa_pon
        inc cx
        mov dx, ax
        call fat_lee
        cmp ax, ULTIMO
        jne .b
.x:
        ret
.rota:
        call pon_nombre
        mov si, m_rota
        call pon_cad
        mov byte [errores], 1
        cmp byte [corrige], 0
        je .x
        test dx, dx
        jz .x                       ; (rota desde el principio: no hay cadena)
        mov ax, dx
        mov dx, ULTIMO
        jmp fat_pon
.cruce:
        push ax
        call pon_nombre
        mov si, m_cruce
        call pon_cad
        pop ax
        call pon_num
        call pon_nl
        mov byte [errores], 1
        mov byte [cruces], 1
        ret

; escribe «  NOMBRE.EXT» (la entrada de [d_ent]). Conserva los registros
pon_nombre:
        push ax
        push cx
        push si
        mov al, ' '
        call pon_car
        call pon_car
        mov si, [d_ent]
        mov cx, 11
.c:
        cmp cx, 3
        jne .c1
        cmp byte [si], ' '
        je .c1
        mov al, '.'
        call pon_car
.c1:
        lodsb
        cmp al, ' '
        je .c2
        call pon_car
.c2:
        loop .c
        pop si
        pop cx
        pop ax
        ret

; ---- los grupos que nadie usa: libres, defectuosos o perdidos ----
perdidos:
        mov ax, 2
.b:
        cmp ax, [med+M_MAX]
        ja .cadenas
        push ax
        mov bx, usado
        call mapa_mira
        jnz .sig
        mov dx, ax
        call fat_lee
        test ax, ax
        jnz .p1
        inc word [g_libres]
        jmp short .sig
.p1:
        cmp ax, MALO
        jne .p2
        inc word [g_malos]
        jmp short .sig
.p2:
        inc word [g_perdidos]       ; perdido: y el que señala no es principio
        cmp ax, 2                   ; de cadena
        jb .sig
        cmp ax, [med+M_MAX]
        ja .sig
        mov bx, apuntado
        call mapa_pon
.sig:
        pop ax
        inc ax
        jnz .b
.cadenas:
        cmp word [g_perdidos], 0
        jne .hay
        ret
.hay:
        mov byte [errores], 1
        ; cada principio de cadena perdida: se cuenta y, con /F, se guarda
        mov ax, 2
.c:
        cmp ax, [med+M_MAX]
        ja .resto
        push ax
        mov bx, usado
        call mapa_mira
        jnz .c1
        mov bx, apuntado
        call mapa_mira
        jnz .c1
        mov dx, ax
        call fat_lee
        test ax, ax
        jz .c1
        cmp ax, MALO
        je .c1
        inc word [n_cadenas]
        cmp byte [corrige], 0
        je .c1
        mov ax, dx
        call guarda_cadena
.c1:
        pop ax
        inc ax
        jnz .c
.resto:
        cmp byte [corrige], 0
        jne .suelta
        mov ax, [g_perdidos]
        call pon_num
        mov si, m_perdidos
        call pon_cad
        mov ax, [n_cadenas]
        call pon_num
        mov si, m_perdidos2
        jmp pon_cad
.suelta:
        ; lo que quede perdido (cadenas en círculo, o sin sitio en la raíz):
        ; libre
        mov ax, 2
.s:
        cmp ax, [med+M_MAX]
        ja .dice
        push ax
        mov bx, usado
        call mapa_mira
        jnz .s1
        mov dx, ax
        call fat_lee
        test ax, ax
        jz .s1
        cmp ax, MALO
        je .s1
        mov ax, dx
        xor dx, dx
        call fat_pon
        inc word [g_libres]
.s1:
        pop ax
        inc ax
        jnz .s
.dice:
        mov ax, [n_guardadas]
        call pon_num
        mov si, m_guardadas
        jmp pon_cad

; la cadena perdida que empieza en AX, a un archivo FILEnnnn.CHK de la raíz
guarda_cadena:
        mov [gc_ini], ax
        ; un hueco en la raíz
        mov ax, [med+M_RAIZ]
        mov [d_sec], ax
        mov cx, [med+M_RAIZ_SEC]
.sector:
        mov dx, [d_sec]
        call lee_sec
        jc .no
        mov si, sec
.ent:
        mov al, [si]
        test al, al
        jz .hueco
        cmp al, 0xE5
        je .hueco
        add si, 32
        cmp si, sec + 512
        jb .ent
        inc word [d_sec]
        loop .sector
.no:
        ret                         ; (no hay sitio: se soltará)
.hueco:
        mov [d_ent], si
        mov ax, [gc_ini]            ; sus grupos pasan a estar usados; si la
        call .sigue                 ; cadena acaba mal, se cierra bien
        mov di, [d_ent]
        push di
        mov si, t_file
        mov cx, 11
        rep movsb
        xor al, al
        mov cx, 21
        rep stosb
        pop di
        mov ax, [n_guardadas]       ; FILEnnnn
        mov cx, 10
        mov bx, di
        mov si, 7
.n:
        xor dx, dx
        div cx
        add dl, '0'
        mov [bx+si], dl
        dec si
        cmp si, 4
        jae .n
        mov byte [di+11], 0x20
        mov ax, [gc_ini]
        mov [di+26], ax
        mov ax, [gc_n]
        mul word [bpc]
        mov [di+28], ax
        mov [di+30], dx
        mov dx, [d_sec]
        call esc_sec
        mov ax, [gc_n]
        add [g_arch], ax
        inc word [n_arch]
        inc word [n_guardadas]
        ret
; (interna) recorre la cadena perdida desde AX -> [gc_n] = sus grupos
.sigue:
        mov word [gc_n], 0
        xor dx, dx
.b:
        cmp ax, 2
        jb .corta
        cmp ax, [med+M_MAX]
        ja .corta
        mov bx, usado
        call mapa_mira
        jnz .corta
        call mapa_pon
        inc word [gc_n]
        mov dx, ax
        call fat_lee
        cmp ax, ULTIMO
        jne .b
        ret
.corta:
        mov ax, dx
        mov dx, ULTIMO
        jmp fat_pon

; ---- lo que hay en el disco, y la memoria ----
informe:
        mov ax, [med+M_MAX]
        dec ax
        mov si, m_total
        call .linea
        mov ax, [g_ocultos]
        test ax, ax
        jz .i1
        mov bx, [n_ocultos]
        mov si, m_ocultos
        call .linea_n
.i1:
        mov ax, [g_dirs]
        test ax, ax
        jz .i2
        mov bx, [n_dirs]
        mov si, m_dirs
        call .linea_n
.i2:
        mov ax, [g_arch]
        mov bx, [n_arch]
        mov si, m_arch
        call .linea_n
        mov ax, [g_malos]
        test ax, ax
        jz .i3
        mov si, m_malos
        call .linea
.i3:
        mov ax, [g_libres]
        mov si, m_libres
        call .linea
        call pon_nl
        int 0x12                    ; la memoria: K, en octetos
        mov cx, 1024
        mul cx
        call pon_num32
        mov si, m_mem
        call pon_cad
        mov ax, [0x02]              ; y lo que hay desde aquí hasta el final
        mov bx, cs
        sub ax, bx
        mov cx, 16
        mul cx
        call pon_num32
        mov si, m_mem_libre
        call pon_cad
        cmp byte [errores], 0
        je .x
        cmp byte [corrige], 0
        jne .x
        mov si, m_con_f
        call pon_cad
        cmp byte [cruces], 0
        je .x
        mov si, m_cruzados
        call pon_cad
.x:
        ret
; (internas) AX grupos, en octetos, y el texto de SI; con BX, «en BX cosas»
.linea:
        mul word [bpc]
        call pon_num32
        jmp pon_cad
.linea_n:
        mul word [bpc]
        call pon_num32
        push si
        mov si, m_en
        call pon_cad
        mov ax, bx
        call pon_num
        pop si
        jmp pon_cad

m_uso       db "REVISAR - repasa un disco", 10
            db "  REVISAR [unidad:] [/F]     /F: corrige lo que encuentre mal", 10, 0
m_e_unidad  db "La unidad no está preparada.", 10, 0
m_e_memoria db "No hay memoria para revisar ese disco.", 10, 0
m_e_leer    db "No se puede leer el disco.", 10, 0
m_e_escribir db "No se puede escribir en el disco.", 10, 0
m_tam       db ": el tamaño no concuerda con sus grupos.", 10, 0
m_rota      db ": su cadena de grupos está rota.", 10, 0
m_cruce     db ": cruzado con otro archivo en el grupo ", 0
m_perdidos  db " grupos perdidos en ", 0
m_perdidos2 db " cadenas.", 10, 0
m_guardadas db " cadenas perdidas guardadas en archivos FILEnnnn.CHK.", 10, 0
m_total     db " octetos en total en el disco", 10, 0
m_en        db " octetos en ", 0
m_ocultos   db " archivos ocultos", 10, 0
m_dirs      db " directorios", 10, 0
m_arch      db " archivos", 10, 0
m_malos     db " octetos en sectores defectuosos", 10, 0
m_libres    db " octetos libres", 10, 0
m_mem       db " octetos de memoria", 10, 0
m_mem_libre db " octetos libres", 10, 0
m_con_f     db 10, "Hay errores. REVISAR /F los corrige.", 10, 0
m_cruzados  db "Los archivos cruzados no: cópielos a otro sitio y borre los dos.", 10, 0
t_file      db "FILE0000CHK"

        absolute $
            alignb 2
bss_ini:
unidad      resb 1
corrige     resb 1                  ; /F
errores     resb 1
cruces      resb 1
fat_sucia   resb 1
sec_sucio   resb 1
op          resb 1                  ; 25h leer, 26h escribir
sin_preguntar resb 1
            alignb 2
med         resw 9                  ; las medidas de la unidad
bpc         resw 1                  ; octetos por grupo
fat_seg     resw 1                  ; la FAT, en la memoria
fat_base    resw 1                  ; (al escribirla: qué copia)
fe_sec      resw 1
fe_resto    resw 1
fe_seg      resw 1
nivel       resw 1
d_clus      resw 1                  ; el directorio que se recorre
d_sec       resw 1
d_resto     resw 1
d_ent       resw 1
n_arch      resw 1                  ; cuántos hay de cada cosa...
n_dirs      resw 1
n_ocultos   resw 1
n_cadenas   resw 1
n_guardadas resw 1
g_arch      resw 1                  ; ...y cuántos grupos ocupan
g_dirs      resw 1
g_ocultos   resw 1
g_malos     resw 1
g_libres    resw 1
g_perdidos  resw 1
gc_ini      resw 1
gc_n        resw 1
sec         resb 512
usado       resb 8192               ; un bit por grupo: es de algún archivo
apuntado    resb 8192               ; un bit por grupo: otro perdido lo señala
bss_fin:
