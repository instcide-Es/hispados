; =============================================================================
;  FORMATEA.COM (FORMAT) - da formato a un disquete o al disco duro
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       FORMATEA unidad: [/S] [/V:etiqueta] [/F:medida] [/R] [/A]
;
;  A: y B: son los disquetes; C:, la partición de DOS del disco duro (la que
;  crea PARTIR). Opciones:
;       /S           copia además el sistema: el disco queda de arranque
;       /V:etiqueta  le pone ese nombre al disco (hasta 11 letras)
;       /F:medida    360, 720, 1200 o 1440 (K), o las del PC original: 160,
;                    180 o 320. Sin decirla, un disquete que
;                    ya tiene formato conserva su medida; uno nuevo recibe la
;                    mayor de la unidad
;       /R           formato rápido: no se graban ni se repasan las pistas
;                    (solo vale para un disquete que ya tuvo formato)
;       /A           automático: no pregunta (para los lotes)
;
;  UN DISQUETE recibe formato de verdad: se graba cada pista (INT 13h/05h) y
;  se repasa leyéndola entera (02h); la pista que falla se apunta en la FAT como
;  defectuosa, y el disquete sirve con lo demás (salvo que falle el principio,
;  donde van el arranque, la FAT y el directorio). Si la BIOS no sabe grabar
;  pistas, se dice y se hace el formato rápido. Después se escriben el sector
;  de arranque, las dos FAT y el directorio raíz, y se comprueba leyéndolos.
;
;  EL DISCO DURO solo recibe formato lógico (el de bajo nivel lo trae de
;  fábrica o de su controladora): FAT16 con grupos de 2 K si la partición
;  tiene 16 megas o más, y FAT12 si tiene menos, como MS-DOS 3.3.
;
;  Como todo esto se hace con la BIOS, por debajo del núcleo, al acabar se le
;  avisa para que vuelva a mirar los discos (INT 21h/E8h). Devuelve 0 si ha
;  ido bien, 4 si no, y 5 si se contesta que no. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100
%define CON_SISTEMA
%define XFER_TAM 18 * 512           ; una pista entera, para repasarla

        jmp inicio
%include "util.inc"
%include "sectores.inc"         ; (MBR_COD y ARR_COD: lo que miden sin los ceros del final)

; las medidas de disquete
T_K         equ 0                   ; dw: cuántos K
T_PISTAS    equ 2                   ; db
T_SPP       equ 3                   ; db: sectores por pista
T_SPC       equ 4                   ; db: sectores por grupo
T_RAIZ      equ 5                   ; dw: entradas del directorio raíz
T_SPF       equ 7                   ; db: sectores de cada FAT
T_MEDIO     equ 8                   ; db: el octeto que dice qué disco es
T_TOT       equ 9                   ; dw: sectores
T_HUECO     equ 11                  ; db: el hueco entre sectores, al grabar
T_CARAS     equ 12                  ; db
T_TAM       equ 13
%macro MEDIDA 10                    ; K, pistas, spp, spc, raíz, spf, medio, sectores, hueco, caras
        dw %1
        db %2, %3, %4
        dw %5
        db %6, %7
        dw %8
        db %9, %10
%endmacro
medidas:
        MEDIDA 360, 40, 9, 2, 112, 2, 0xFD, 720, 0x50, 2
        MEDIDA 720, 80, 9, 2, 112, 3, 0xF9, 1440, 0x50, 2
        MEDIDA 1200, 80, 15, 1, 224, 7, 0xF9, 2400, 0x54, 2
        MEDIDA 1440, 80, 18, 1, 224, 9, 0xF0, 2880, 0x6C, 2
        MEDIDA 160, 40, 8, 1, 64, 1, 0xFE, 320, 0x50, 1     ; las del PC original:
        MEDIDA 180, 40, 9, 1, 64, 2, 0xFC, 360, 0x50, 1     ; una cara, u ocho
        MEDIDA 320, 40, 8, 2, 112, 1, 0xFF, 640, 0x50, 2    ; sectores por pista
        dw 0

inicio:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        mov word [arr+510], 0xAA55
        call pide_nucleo
        call xfer_ini
        mov byte [unidad], 0xFF
        mov byte [prueba_mala], 0xFF
        ; ---- lo que se pide ----
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        je .opcion
        cmp byte [si+1], ':'
        je .uni
        jmp uso
.uni:
        call mayus
        sub al, 'A'
        mov [unidad], al
        add si, 2
        jmp .arg
.opcion:
        inc si
        lodsb
        call mayus
        cmp al, 'S'
        jne .o1
        mov byte [con_sistema], 1
        jmp .arg
.o1:
        cmp al, 'A'
        jne .o2
        mov byte [sin_preguntar], 1
        jmp .arg
.o2:
        cmp al, 'R'
        je .o3
        cmp al, 'Q'
        jne .o4
.o3:
        mov byte [rapido], 1
        jmp .arg
.o4:
        cmp al, 'V'
        jne .o6
        cmp byte [si], ':'
        jne .v0
        inc si
.v0:
        mov di, etiqueta
        mov cx, 11
.v1:
        mov al, [si]
        cmp al, ' '
        jbe .v2
        cmp al, '/'
        je .v2
        inc si
        jcxz .v1                    ; (lo que pasa de once letras se pierde)
        call mayus
        stosb
        dec cx
        mov byte [hay_etiqueta], 1
        jmp .v1
.v2:
        jmp .arg
.o6:
        cmp al, 'F'
        jne .o7
        call lee_num
        mov [pide_k], ax
        jmp .arg
.o7:
        cmp al, 'M'                 ; /M:n (solo para las pruebas): la pista n
        je .o8                      ; de la cara 0 se da por mala
        jmp uso
.o8:
        call lee_num
        mov [prueba_mala], al
        jmp .arg
.fin_args:
        mov al, [unidad]
        cmp al, 0xFF
        jne .hay
        jmp uso
.hay:
        add al, 'A'                 ; la letra, en los textos que la dicen
        mov [m_aviso_a.u], al
        mov [m_abiertos.u], al
        cmp byte [unidad], 2
        ja .no_unidad
        je .abiertos
        cmp byte [unidad], 0
        je .abiertos
        jmp short .abiertos         ; (B: existe siempre: con una sola
.no_unidad:                         ; disquetera es la misma, con otro disquete)
        mov si, m_e_unidad
        jmp error
.abiertos:
        mov ax, 0xE802              ; con archivos abiertos en ella, no
        mov dl, [unidad]
        int 0x21
        test ax, ax
        jz .sis
        mov si, m_abiertos
        jmp error
.sis:
        cmp byte [con_sistema], 0   ; el sistema se lee antes de empezar: con
        je .cual                    ; una sola disquetera, luego ya no estará
        call sis_lee
        jnc .cual
        mov si, m_sin_sistema
        jmp error
.cual:
        cmp byte [unidad], 2
        jne disquete
        jmp duro

; el número de SI (detrás de «:», si lo hay) -> AX; SI queda detrás
lee_num:
        cmp byte [si], ':'
        jne .n0
        inc si
.n0:
        xor ax, ax
.n1:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .x
        mov cx, 10
        mul cx
        xor bh, bh
        add ax, bx
        inc si
        jmp .n1
.x:
        ret

; =============================================================================
;  Un disquete
; =============================================================================
disquete:
        mov al, [unidad]
        mov [d_bios], al
        int 0x11                    ; con una sola disquetera, A: y B: son la
        test al, 1                  ; misma: la 0 de la BIOS
        jz .una
        and al, 0xC0
        jnz .dos
.una:
        mov byte [d_bios], 0
.dos:
        mov si, m_aviso_a
        call confirma
        je .va
        jmp no_quiere
.va:
        push es                     ; qué unidad es (un XT no lo sabe decir)
        mov ah, 8
        mov dl, [d_bios]
        int 0x13
        pop es
        jc .tipo
        mov [tipo_u], bl
.tipo:
        mov word [d_spp], 9         ; (para leer el primer sector vale cualquiera)
        mov word [d_caras], 2
        mov ax, [pide_k]
        test ax, ax
        jnz .busca_k
        ; sin /F: ¿tiene ya formato? Entonces, su misma medida
        xor ax, ax
        mov bx, sec
        call disco_lee
        jc .por_unidad
        cmp word [sec+11], 512
        jne .por_unidad
        cmp word [sec+510], 0xAA55
        jne .por_unidad
        mov ax, [sec+19]
        mov bx, medidas
.m1:
        cmp word [bx], 0
        je .por_unidad
        cmp ax, [bx+T_TOT]
        je .hallada
        add bx, T_TAM
        jmp .m1
.por_unidad:
        cmp byte [rapido], 0        ; el rápido necesita saber qué había
        je .pu1
        mov si, m_e_rapido
        jmp error
.pu1:
        mov ax, 360                 ; la mayor que admite la unidad
        mov bl, [tipo_u]
        cmp bl, 2
        jb .busca_k
        mov ax, 1200
        je .busca_k
        mov ax, 720
        cmp bl, 3
        je .busca_k
        mov ax, 1440
.busca_k:
        mov bx, medidas
.k1:
        cmp word [bx], 0
        jne .k2
        mov si, m_e_medida
        jmp error
.k2:
        cmp ax, [bx+T_K]
        je .hallada
        add bx, T_TAM
        jmp .k1
.hallada:
        mov [med], bx
        mov ax, [bx+T_TOT]
        mov [f_tot], ax
        mov al, [bx+T_SPC]
        mov [f_spc], al
        mov ax, [bx+T_RAIZ]
        mov [f_raiz], ax
        mov al, [bx+T_SPF]
        xor ah, ah
        mov [f_spf], ax
        mov al, [bx+T_MEDIO]
        mov [f_medio], al
        mov al, [bx+T_SPP]
        mov [f_spp], ax
        mov [d_spp], ax
        mov al, [bx+T_CARAS]
        mov [f_caras], ax
        mov [d_caras], ax
        mov al, [bx+T_PISTAS]
        mov [f_pistas], al
        call calc_datos
        mov si, m_formato
        call pon_cad
        mov ax, [bx+T_K]
        call pon_num
        mov si, m_formato2
        call pon_cad
        ; la FAT, en la memoria: vacía (aquí se apuntan las pistas malas)
        mov di, fatbuf
        mov cx, 9 * 512 / 2
        xor ax, ax
        rep stosw
        mov al, [f_medio]
        mov [fatbuf], al
        mov word [fatbuf+1], 0xFFFF
        cmp byte [rapido], 0
        jne .logico
        call bajo_nivel
        call quita_1e
        jnc .logico
        jmp error                   ; (con el mensaje en SI)
.logico:
        call escribe_todo
        jnc .hecho
        jmp error
.hecho:
        jmp final

; ---- graba y repasa todas las pistas. CF=1 y el mensaje en SI si el
;      disquete no sirve; si la BIOS no sabe, lo dice y deja [rapido] = 1 ----
bajo_nivel:
        ; qué disco se va a grabar: primero como los AT (18h), que devuelven
        ; la tabla de parámetros que hay que usar; si no, como los primeros
        ; AT (17h) y con la tabla de la BIOS
        mov bx, [med]
        push es
        mov ah, 0x18
        mov ch, [bx+T_PISTAS]
        dec ch
        mov cl, [bx+T_SPP]
        mov dl, [d_bios]
        int 0x13
        jc .sin18
        mov si, di                  ; ES:DI = la tabla: una copia
        mov ax, es
        pop es
        push ds
        mov ds, ax
        mov di, dpt
        mov cx, 11
        rep movsb
        pop ds
        jmp short .tabla
.sin18:
        pop es
        mov bx, [med]
        mov al, 0
        cmp word [bx+T_K], 1440
        je .sin17
        mov al, 4
        cmp word [bx+T_K], 720
        je .con17
        mov al, 3
        cmp word [bx+T_K], 1200
        je .con17
        mov al, 1                   ; 360 K: en su unidad, o en una de 1,2 M
        cmp byte [tipo_u], 2
        jne .con17
        mov al, 2
.con17:
        mov ah, 0x17
        mov dl, [d_bios]
        int 0x13
.sin17:
        push ds                     ; la tabla que tiene la BIOS (INT 1Eh)
        xor ax, ax
        mov ds, ax
        lds si, [0x1E * 4]
        mov di, dpt
        mov cx, 11
        rep movsb
        pop ds
.tabla:
        mov bx, [med]
        mov al, [bx+T_SPP]          ; el último sector de la pista
        mov [dpt+4], al
        mov al, [bx+T_HUECO]        ; el hueco entre sectores, al grabar
        mov [dpt+7], al
        mov byte [dpt+8], 0xF6      ; con qué se rellenan
        push es                     ; nuestra tabla, mientras dure
        xor ax, ax
        mov es, ax
        mov ax, [es:0x1E * 4]
        mov [ant_1e], ax
        mov ax, [es:0x1E * 4 + 2]
        mov [ant_1e+2], ax
        cli
        mov word [es:0x1E * 4], dpt
        mov [es:0x1E * 4 + 2], cs
        sti
        pop es
        mov byte [hay_1e], 1
        xor ax, ax
        mov dl, [d_bios]
        int 0x13
        mov byte [f_cil], 0
.pista:
        mov byte [f_cara], 0
.cara:
        mov si, m_pista
        call pon_cad
        mov al, [f_cil]
        xor ah, ah
        call pon_num
        mov si, m_cara
        call pon_cad
        mov al, [f_cara]
        call pon_num
        mov al, ' '
        call pon_car
        mov al, 13
        call pon_car_dos
        mov ah, 5                   ; grabarla...
        call es_pista
        jnc .repasa
        mov al, [d_err]
        cmp al, 3
        jne .e1
        mov si, m_e_protegido
        stc
        ret
.e1:
        mov ah, [f_cil]
        or ah, [f_cara]
        jnz .mala
        cmp al, 1                   ; a la primera: ¿es que la BIOS no sabe?
        je .no_sabe
        cmp al, 0x0C
        je .no_sabe
.inutil:
        mov si, m_e_pista0
        stc
        ret
.no_sabe:
        mov si, m_no_sabe
        call pon_cad
        mov byte [rapido], 1
        clc
        ret
.repasa:
        mov ah, 2                   ; ...y repasarla: se lee entera
        call es_pista
        jc .mala
        mov al, [f_cil]             ; (la pista que las pruebas dan por mala:
        cmp al, [prueba_mala]       ; los emuladores no saben fallar)
        jne .sig
        cmp byte [f_cara], 0
        jne .sig
.mala:
        call marca_mala
        jc .inutil
.sig:
        inc byte [f_cara]
        mov al, [f_cara]
        cmp al, [f_caras]
        jb .cara
        inc byte [f_cil]
        mov al, [f_cil]
        cmp al, [f_pistas]
        jae .fin
        jmp .pista
.fin:
        mov si, m_borra_linea
        call pon_cad
        clc
        ret

; graba (AH=5) o lee (AH=2) la pista [f_cil], cara [f_cara], entera. Tres
; intentos. CF=1 y el error en [d_err]. Se repasa leyendo (02h) y no con la
; función de «verificar» (04h) porque hay BIOS que no la hacen de verdad
es_pista:
        mov [d_op], ah
        mov di, [p_xfer]            ; lo que se graba en cada sector: su
        mov cl, [f_spp]             ; cilindro, su cara, su número y su tamaño
        xor ch, ch                  ; (2: 512 octetos)
        mov ah, 1
.t:
        mov al, [f_cil]
        stosb
        mov al, [f_cara]
        stosb
        mov al, ah
        stosb
        mov al, 2
        stosb
        inc ah
        loop .t
        mov di, 3
.otra:
        mov ah, [d_op]
        mov al, [f_spp]
        mov bx, [p_xfer]
        mov ch, [f_cil]
        mov cl, 1
        mov dh, [f_cara]
        mov dl, [d_bios]
        int 0x13
        jnc .x
        mov [d_err], ah
        cmp ah, 3
        je .mal
        cmp ah, 1
        je .mal
        xor ax, ax
        int 0x13
        dec di
        jnz .otra
.mal:
        stc
.x:
        ret

; la pista [f_cil], cara [f_cara], no sirve: sus grupos se apuntan en la FAT
; como defectuosos. CF=1 si en ella va algo del sistema (arranque, FAT o
; directorio): entonces no sirve el disquete
marca_mala:
        mov al, [f_cil]
        mul byte [f_caras]
        add al, [f_cara]
        mul word [f_spp]            ; AX = su primer sector
        mov cx, [f_spp]
        cmp ax, [datos_ini]
        jb .mal
.s:
        push ax
        sub ax, [datos_ini]
        xor dx, dx
        mov bl, [f_spc]
        xor bh, bh
        div bx
        add ax, 2
        mov bx, ax                  ; el grupo: 12 bits en el octeto n*3/2
        shr bx, 1
        add bx, ax
        mov dx, [fatbuf+bx]
        test al, 1
        jnz .impar
        and dx, 0xF000
        or dx, 0x0FF7
        jmp short .pon
.impar:
        and dx, 0x000F
        or dx, 0xFF70
.pon:
        cmp dx, [fatbuf+bx]
        je .ya
        mov [fatbuf+bx], dx
        inc word [n_malos]
.ya:
        pop ax
        inc ax
        loop .s
        clc
        ret
.mal:
        stc
        ret

; devuelve a la BIOS su tabla de parámetros de disquete. Conserva las banderas
quita_1e:
        pushf
        cmp byte [hay_1e], 0
        je .x
        push ax
        push es
        xor ax, ax
        mov es, ax
        cli
        mov ax, [ant_1e]
        mov [es:0x1E * 4], ax
        mov ax, [ant_1e+2]
        mov [es:0x1E * 4 + 2], ax
        sti
        pop es
        mov byte [hay_1e], 0
        xor ax, ax
        mov dl, [d_bios]
        int 0x13
        pop ax
.x:
        popf
        ret

; =============================================================================
;  El disco duro (C:)
; =============================================================================
duro:
        call duro_lee
        jnc .hay
        mov si, m_e_sin_duro
        jmp error
.hay:
        call duro_particion
        jnc .part
        mov si, m_e_sin_part
        jmp error
.part:
        mov [fila], si
        cmp word [si+14], 0         ; lo más, 65.535 sectores: 32 megas
        je .cabe
.no_vale:
        mov si, m_e_grande
        jmp error
.cabe:
        mov ax, [si+12]
        cmp ax, 256                 ; (y lo menos, algo en lo que quepa una FAT)
        jb .no_vale
        mov [f_tot], ax
        cmp byte [sin_preguntar], 0
        jne .va
        mov si, m_aviso_c
        call pon_cad
        mov ax, [f_tot]             ; en megas: sectores entre 2.048
        add ax, 1024
        mov cl, 11
        shr ax, cl
        call pon_num
        mov si, m_aviso_c2
        call confirma
        je .va
        jmp no_quiere
.va:
        ; FAT16 con grupos de 2 K desde 16 megas; por debajo, FAT12 con los
        ; grupos justos para no pasar de 4.080
        mov byte [f_spc], 4
        mov byte [f_fat16], 1
        cmp word [f_tot], 32680
        jae .medidas
        mov byte [f_fat16], 0
        mov byte [f_spc], 1
.g:
        mov ax, [f_tot]
        xor dx, dx
        mov bl, [f_spc]
        xor bh, bh
        div bx
        cmp ax, 4080
        jbe .medidas
        shl byte [f_spc], 1
        jmp .g
.medidas:
        mov word [f_raiz], 512
        mov byte [f_medio], 0xF8
        mov ax, [d_spp]
        mov [f_spp], ax
        mov ax, [d_caras]
        mov [f_caras], ax
        mov word [f_spf], 1         ; lo que mide la FAT: se prueba hasta que
.spf:
        call calc_datos             ; quepan todos sus grupos
        mov ax, [n_grupos]
        add ax, 2
        cmp byte [f_fat16], 0
        je .doce
        add ax, 255                 ; dos octetos por grupo: 256 por sector
        mov al, ah
        xor ah, ah
        jmp short .cmp
.doce:
        mov bx, ax                  ; octeto y medio por grupo
        shl ax, 1
        add ax, bx
        inc ax
        shr ax, 1
        add ax, 511
        mov cl, 9
        shr ax, cl
.cmp:
        cmp ax, [f_spf]
        jbe .listo
        inc word [f_spf]
        jmp .spf
.listo:
        ; la tabla de particiones: el tipo que corresponde (01 FAT12, 04
        ; FAT16) y, con /S, la partición activa y un sector que la arranque
        mov si, [fila]
        mov al, 1
        cmp byte [f_fat16], 0
        je .tipo
        mov al, 4
        cmp byte [si+4], 6          ; («FAT16 grande» también vale)
        jne .tipo
        mov al, 6
.tipo:
        mov [si+4], al
        cmp byte [con_sistema], 0
        je .mbr
        call duro_codigo
        call duro_activa
.mbr:
        call duro_escribe
        jnc .base
        mov si, m_e_escribir
        jmp error
.base:
        mov si, [fila]
        mov ax, [si+8]
        mov [d_base], ax
        mov [f_ocultos], ax
        mov ax, [si+10]
        mov [d_base+2], ax
        mov [f_ocultos+2], ax
        call escribe_todo
        jnc final
        jmp error

; =============================================================================
;  Lo común
; =============================================================================
; de [f_raiz], [f_spf], [f_tot] y [f_spc] -> [datos_ini], el primer sector de
; los datos, y [n_grupos], cuántos grupos caben. Conserva BX
calc_datos:
        push bx
        mov ax, [f_raiz]
        mov cl, 4
        shr ax, cl
        inc ax                      ; (el sector de arranque)
        add ax, [f_spf]
        add ax, [f_spf]
        mov [datos_ini], ax
        mov bx, [f_tot]
        sub bx, ax
        mov ax, bx
        xor dx, dx
        mov bl, [f_spc]
        xor bh, bh
        div bx
        mov [n_grupos], ax
        pop bx
        ret

; escribe el sector de arranque, las dos FAT y el directorio raíz, y los
; comprueba leyendo. CF=1 y el mensaje en SI si falla
escribe_todo:
        ; ---- el sector de arranque: el nuestro, con las medidas del disco ----
        mov word [arr+11], 512
        mov al, [f_spc]
        mov [arr+13], al
        mov word [arr+14], 1
        mov byte [arr+16], 2
        mov ax, [f_raiz]
        mov [arr+17], ax
        mov ax, [f_tot]
        mov [arr+19], ax
        mov al, [f_medio]
        mov [arr+21], al
        mov ax, [f_spf]
        mov [arr+22], ax
        mov ax, [f_spp]
        mov [arr+24], ax
        mov ax, [f_caras]
        mov [arr+26], ax
        mov ax, [f_ocultos]
        mov [arr+28], ax
        mov ax, [f_ocultos+2]
        mov [arr+30], ax
        xor ax, ax
        mov [arr+32], ax
        mov [arr+34], ax
        mov al, [d_bios]
        mov [arr+36], al
        mov byte [arr+37], 0
        mov byte [arr+38], 0x29
        xor ah, ah                  ; el número de serie: del reloj
        int 0x1A
        mov [arr+39], dx
        xor cx, 0x8086
        mov [arr+41], cx
        mov si, t_sin_nombre
        cmp byte [hay_etiqueta], 0
        je .et
        mov si, etiqueta
        mov cx, 11                  ; (lo que no se escribió, espacios)
.e0:
        cmp byte [si], 0
        jne .e1
        mov byte [si], ' '
.e1:
        inc si
        loop .e0
        mov si, etiqueta
.et:
        mov di, arr + 43
        mov cx, 11
        rep movsb
        mov byte [arr+58], '2'      ; «FAT12   » o «FAT16   »
        mov al, [f_fat16]
        mov [arr+509], al
        test al, al
        jz .a1
        mov byte [arr+58], '6'
.a1:
        xor ax, ax
        mov bx, arr
        call disco_escribe
        jnc .fat
        jmp .mal
        ; ---- las dos FAT ----
.fat:
        mov word [e_sec], 1
        mov cx, 2
.copia:
        push cx
        xor dx, dx                  ; DX = qué sector de la FAT
.f1:
        call fat_sector
        mov ax, [e_sec]
        mov bx, sec
        call disco_escribe
        jc .mal1
        inc word [e_sec]
        inc dx
        cmp dx, [f_spf]
        jb .f1
        pop cx
        loop .copia
        ; ---- el directorio raíz, vacío; con la etiqueta, si se ha dado ----
        mov di, sec
        mov cx, 256
        xor ax, ax
        rep stosw
        cmp byte [hay_etiqueta], 0
        je .raiz
        mov si, etiqueta
        mov di, sec
        mov cx, 11
        rep movsb
        mov byte [sec+11], 0x08
        DOS 0x2A                    ; su fecha: CX=año DH=mes DL=día
        sub cx, 1980
        mov ax, cx
        mov cl, 9
        shl ax, cl
        mov bl, dh
        xor bh, bh
        mov cl, 5
        shl bx, cl
        or ax, bx
        xor dh, dh
        or ax, dx
        mov [sec+24], ax
.raiz:
        mov cx, [f_raiz]
        shr cx, 1
        shr cx, 1
        shr cx, 1
        shr cx, 1
.r1:
        mov ax, [e_sec]
        mov bx, sec
        call disco_escribe
        jc .mal
        inc word [e_sec]
        push cx
        mov di, sec                 ; (los demás, vacíos del todo)
        mov cx, 32
        xor al, al
        rep stosb
        pop cx
        loop .r1
        ; ---- la comprobación: lo escrito se lee y tiene que ser lo mismo ----
        xor ax, ax
        mov bx, sec
        call disco_lee
        jc .mal_leer
        mov si, sec
        mov di, arr
        mov cx, 256
        repe cmpsw
        jne .mal_leer
        mov ax, 1
        mov bx, sec
        call disco_lee
        jc .mal_leer
        mov al, [f_medio]
        cmp [sec], al
        jne .mal_leer
        cmp word [sec+1], 0xFFFF
        jne .mal_leer
        clc
        ret
.mal1:
        pop cx
.mal:
        mov si, m_e_escribir
        cmp byte [d_err], 3
        jne .m1
        mov si, m_e_protegido
.m1:
        stc
        ret
.mal_leer:
        mov si, m_e_comprobar
        stc
        ret

; el sector DX de la FAT -> `sec`. La de un disquete está hecha en la memoria
; (con sus pistas malas); la del disco duro está vacía. Conserva DX
fat_sector:
        mov di, sec
        cmp byte [unidad], 2
        je .duro
        mov si, dx
        mov cl, 9
        shl si, cl
        add si, fatbuf
        mov cx, 256
        rep movsw
        ret
.duro:
        mov cx, 256
        xor ax, ax
        rep stosw
        test dx, dx
        jnz .x
        mov al, [f_medio]           ; los dos primeros grupos no existen: ahí
        mov [sec], al               ; va qué disco es
        mov word [sec+1], 0xFFFF
        cmp byte [f_fat16], 0
        je .x
        mov byte [sec+3], 0xFF
.x:
        ret

; ---- el final: se avisa al núcleo, se copia el sistema y se dice lo que hay ----
final:
        call avisa_nucleo
        cmp byte [unidad], 2        ; (con una sola disquetera: el disquete
        jae .f0                     ; puesto es el de esta unidad)
        mov ax, 0xE803
        mov dl, [unidad]
        int 0x21
.f0:
        mov si, m_hecho
        call pon_cad
        cmp byte [con_sistema], 0
        je .cuenta
        mov al, [unidad]
        add al, 'A'
        call sis_escribe
        jnc .s1
        mov si, m_e_copiar
        jmp error
.s1:
        mov si, m_sistema
        call pon_cad
.cuenta:
        mov dl, [unidad]            ; lo que dice el propio sistema del disco
        inc dl                      ; nuevo: grupos x sectores x 512
        DOS 0x36
        cmp ax, 0xFFFF
        jne .c1
        mov si, m_e_no_ve
        jmp error
.c1:
        mov [n_spc], ax
        mov [n_libres], bx
        mul dx
        call por512
        call pon_num32
        mov si, m_total
        call pon_cad
        mov ax, [n_malos]
        test ax, ax
        jz .c2
        mul word [n_spc]
        call por512
        call pon_num32
        mov si, m_malos
        call pon_cad
.c2:
        mov ax, [n_libres]
        mul word [n_spc]
        call por512
        call pon_num32
        mov si, m_libres
        call pon_cad
        mov ax, 0x4C00
        int 0x21
; DX:AX por 512
por512:
        mov cl, 9
        shl dx, cl
        mov bx, ax
        mov cl, 7
        shr bx, cl
        or dx, bx
        mov cl, 9
        shl ax, cl
        ret

uso:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C04
        int 0x21
no_quiere:
        mov si, m_nada
        call pon_cad
        mov ax, 0x4C05
        int 0x21
; escribe el mensaje de SI y termina con el código 4
error:
        call quita_1e
        call pon_cad
        call avisa_nucleo           ; (lo que se haya llegado a tocar)
        mov ax, 0x4C04
        int 0x21

m_uso       db "FORMATEA - da formato a un disquete o al disco duro", 10
            db "  FORMATEA unidad: [/S] [/V:etiqueta] [/F:medida] [/R] [/A]", 10
            db "  /S           copia además el sistema: el disco queda de arranque", 10
            db "  /V:etiqueta  el nombre del disco, hasta 11 letras", 10
            db "  /F:medida    360, 720, 1200 o 1440 (y 160, 180 o 320), un disquete", 10
            db "  /R           rápido: sin grabar ni repasar las pistas", 10
            db "  /A           automático: no pregunta (para los lotes)", 10
            db "A: y B: son los disquetes; C:, la partición del disco duro (la crea PARTIR).", 10, 0
m_aviso_a   db "Atención: se va a BORRAR todo lo que haya en el disquete de la unidad "
.u          db "A:.", 10
            db "Ponga el disquete y pulse S para seguir (otra tecla lo deja): ", 0
m_aviso_c   db "Atención: se va a BORRAR todo lo que hay en el disco duro C: (", 0
m_aviso_c2  db " megas).", 10, "Pulse S para seguir (otra tecla lo deja): ", 0
m_nada      db "No se ha formateado nada.", 10, 0
m_formato   db "Formato de ", 0
m_formato2  db " K.", 10, 0
m_pista     db "Pista ", 0
m_cara      db ", cara ", 0
m_borra_linea db "                    ", 13, 0
m_no_sabe   db "La BIOS no sabe grabar pistas: se hace el formato rápido.", 10, 0
m_hecho     db "Formato terminado.", 10, 0
m_sistema   db "Sistema transferido.", 10, 0
m_total     db " octetos en total", 10, 0
m_malos     db " octetos en sectores defectuosos", 10, 0
m_libres    db " octetos libres", 10, 0
m_e_unidad  db "No existe esa unidad.", 10, 0
m_abiertos  db "Hay archivos abiertos en "
.u          db "A:: no se puede formatear.", 10, 0
m_sin_sistema db "No se encuentran NUCLEO86.SYS y COMMAND.COM junto a esta orden (/S).", 10, 0
m_e_medida  db "Esa medida no vale: 160, 180, 320, 360, 720, 1200 o 1440.", 10, 0
m_e_rapido  db "El formato rápido solo vale para un disquete que ya tiene formato.", 10, 0
m_e_protegido db "El disco está protegido contra escritura.", 10, 0
m_e_pista0  db 10, "No se puede dar formato al principio del disquete: no sirve, o no", 10
            db "es de esa medida (pruebe con /F:).", 10, 0
m_e_escribir db "Error al escribir en el disco.", 10, 0
m_e_comprobar db "Lo escrito no se lee bien: el disco no ha quedado formateado.", 10, 0
m_e_sin_duro db "No hay disco duro.", 10, 0
m_e_sin_part db "El disco duro no tiene partición de DOS: créela con PARTIR.", 10, 0
m_e_grande  db "La partición no vale: tiene que ser de 32 megas como mucho.", 10, 0
m_e_copiar  db "No se ha podido copiar el sistema al disco.", 10, 0
m_e_no_ve   db "El formato ha acabado, pero el sistema no reconoce el disco.", 10, 0
t_sin_nombre db "NO NAME    "       ; (así se dice «sin etiqueta»)

; el sector de particiones y el de arranque, tal como salen de ensamblar
mbr_codigo:
        incbin "../mbr.bin", 0, MBR_COD
arr:
        incbin "../arranque.bin", 0, ARR_COD

        absolute $
bss_ini:
            resb 512 - ARR_COD      ; lo que falta de arr: ceros y la firma (al empezar)
            alignb 2
unidad      resb 1                  ; 0 = A:
con_sistema resb 1                  ; /S
rapido      resb 1                  ; /R
hay_etiqueta resb 1
hay_1e      resb 1                  ; está puesta nuestra tabla de parámetros
prueba_mala resb 1                  ; /M:
tipo_u      resb 1                  ; la disquetera: 1 360 K, 2 1,2 M, 3 720 K, 4 1,44 M
f_cil       resb 1                  ; la pista que se graba
f_cara      resb 1
f_pistas    resb 1
f_spc       resb 1                  ; ---- las medidas del formato ----
f_medio     resb 1
f_fat16     resb 1
            alignb 2
f_tot       resw 1
f_raiz      resw 1
f_spf       resw 1
f_spp       resw 1
f_caras     resw 1
f_ocultos   resw 2
datos_ini   resw 1
n_grupos    resw 1
n_malos     resw 1                  ; grupos defectuosos
n_spc       resw 1
n_libres    resw 1
pide_k      resw 1                  ; /F:
med         resw 1                  ; la fila de `medidas`
fila        resw 1                  ; la fila de la tabla de particiones
e_sec       resw 1
ant_1e      resw 2
etiqueta    resb 12
dpt         resb 12                 ; la tabla de parámetros de disquete
            UTIL_BSS
sec         resb 512
fatbuf      resb 9 * 512            ; la FAT de un disquete
bss_fin:
sis_mem:                            ; aquí se leen los archivos del sistema
