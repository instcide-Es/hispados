; =============================================================================
;  Hispa-DOS INSTALAR.COM - instalar Hispa-DOS en un disco duro (o disquete)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  INSTALAR           sin unidad, guiado: PARTIR /INSTALAR propone el disco
;                     duro (nunca la memoria USB ni el disco de arranque) y,
;                     si no tiene una partición DOS, ofrece crearla (con
;                     todo el disco si está vacío, o en el espacio libre).
;                     Después pregunta antes de seguir.
;  INSTALAR unidad:   en esa unidad. Si no tiene formato, pregunta y se lo da
;                     (FORMATEA unidad: /V:HISPADOS). Hace la unidad
;                     arrancable (SYS) y copia desde la unidad
;                     de arranque la carpeta del sistema (\HISPADOS:
;                     programas, drivers y ALIAS.INI; \HISPADOS\JUEGOS:
;                     los juegos de Hispa Ventanas, con los cartuchos de
;                     sus emuladores en JUEGOS\NES y JUEGOS\SMS; y
;                     \HISPADOS\PROGS: sus accesorios), AUTOEXEC.BAT,
;                     CONFIG.SYS y LEEME.TXT. Si la unidad ya tiene un
;                     AUTOEXEC.BAT o un CONFIG.SYS, se respetan. En los que
;                     se copian, la letra de origen (A: o la USB) y la que
;                     tiene ahora la unidad de destino (D: arrancando desde
;                     una memoria USB) pasan a C:, la que tendrá el disco al
;                     arrancar desde él.
;                     En un disco duro deja además la partición activa y,
;                     si el disco no tiene, el MBR de Hispa-DOS (PARTIR
;                     /ARRANQUE:unidad:). El sector de arranque lleva la
;                     unidad 80h y la geometría de ESE disco (SYS).
;  [HISPA-486] La edición la da el origen: si trae \HISPADOS\EDICION.INI
;  (la imagen USB de Hispa-486) se copia como todo \HISPADOS, y además la
;  carpeta \HISPA32 si la trae. Al ACTUALIZAR un Hispa-386 con un origen de
;  Hispa-486, el VENTANAS.INI y el CENTRO.INI del disco se guardan como .ANT
;  (los nuevos traen el escritorio de Hispa-486 y el grupo «Hispa32»); un
;  Hispa-486 no se actualiza con un origen de Hispa-386 (sí con [N]). Desde
;  disquetes, al final se ofrece el disquete «Hispa-486» (docs/486/HISPA-486.md).
;  [INTERNET] El Navegador, en \HISPADOS\INTERNET: se copia si el origen lo
;  trae (la memoria USB de Hispa-486); desde disquetes, si el disco ha quedado
;  de Hispa-486, se ofrece después el disquete «Internet» (sus archivos van a
;  \HISPADOS\INTERNET). Al actualizar se conserva su NAVEGA.INI.
;  Último paso, la comprobación del sonido: AUDICION /RAPIDA (la versión
;  corta de la prueba de sonido) guarda lo que suena en el SONIDO.INI del
;  disco instalado (unidad:\HISPADOS\SONIDO.INI, con su informe SONIDO.TXT
;  al lado); así HSPSND lo usa desde el primer arranque. /NOSONIDO no la hace.
;  [CD] El origen: la unidad de arranque, salvo que se diga otra con
;  /ORIGEN:x: o que, arrancando de disquete, haya un CD de Hispa-DOS puesto
;  (una unidad con \HISPADOS\HISPACD.INI): entonces se copia todo del CD y
;  no se piden los disquetes «Instalación 2», «Oficina», «Hispa-486» ni
;  «Internet» (el CD lo trae todo).
;  [SETUP] INSTALAR /SETUP: el programa de instalación con pantallas, como el
;  SETUP de MS-DOS 6.22 (pantalla azul, INTRO=Continuar, F1=Ayuda, F3=Salir):
;  bienvenida, el disco duro (prepararlo entero si no tiene sitio, avisando
;  de que se borra), formato, la carpeta C:\HISPADOS, la copia con barra de
;  progreso, los demás disquetes si se instala desde disquetes y, al final,
;  reiniciar. Lo lanza la opción «Instalar Hispa-DOS en el disco duro» del
;  primer menú de arranque del disquete, la memoria USB y el CD. Por debajo
;  hace lo mismo que INSTALAR: PARTIR (/SITUACION, /BORRARTODO, /INSTALAR
;  /SI, /ARRANQUE), FORMATEA y SISTEMA. Sin la comprobación del sonido (se
;  hace después con la opción «Prueba» del menú de arranque).
;  Nivel de error: 0 instalado, 1-5 error, 6 cancelado por el usuario.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

%include "version.inc"          ; VER_STR (la cabecera del programa de instalación)

start:
        mov sp, stack_top
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov si, msg_title
        call puts
        ; unidad de destino (sin ella: la propone PARTIR /INSTALAR)
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 9
        je .sk
        cmp al, 13
        je .guided
        cmp al, '/'
        jne .unit
        cmp byte [si], '?'
        je usage
        dec si
.guided:
        dec si
        mov byte [guided], 1
        jmp .op
.unit:
        and al, 0xDF
        cmp al, 'A'
        jb usage
        cmp al, 'Z'
        ja usage
        cmp byte [si], ':'
        jne usage
        call set_dst
        ; opciones: /NOSONIDO
        inc si
.op:
        lodsb
        cmp al, 13
        je .ope
        cmp al, '/'
        jne .op
        mov eax, [si]
        and eax, 0xDFDFDFDF
        cmp eax, 'SETU'             ; [SETUP] /SETUP
        jne .noset
        mov al, [si+4]
        and al, 0xDF
        cmp al, 'P'
        jne .op
        mov byte [setup], 1
        mov byte [nosnd], 1
        add si, 5
        jmp .op
.noset:
        cmp eax, 'ORIG'             ; [CD] /ORIGEN:x:
        jne .noori
        mov eax, [si+4]
        and eax, 0x00DFDFDF
        cmp eax, 'EN:' & 0xFFFFFF
        jne .op
        mov al, [si+7]
        and al, 0xDF
        cmp al, 'A'
        jb usage
        cmp al, 'Z'
        ja usage
        mov [origen], al
        add si, 8
        jmp .op
.noori:
        cmp eax, 'NOSO'
        jne .op
        mov eax, [si+4]
        and eax, 0xDFDFDFDF
        cmp eax, 'NIDO'
        jne .op
        mov byte [nosnd], 1
        add si, 8
        jmp .op
.ope:
        ; unidad de origen: la de arranque (o el CD, o /ORIGEN:x:)
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [arranque], dl
        mov [src], dl
        call elige_origen
        call pon_origen
        cmp byte [setup], 0
        jne setup_main
        cmp byte [guided], 0
        je .chk
        call guided_pick
.chk:
        mov dl, [src]
        cmp dl, [dst]
        je .same
        mov dl, [arranque]
        cmp dl, [dst]
        jne .ok
.same:
        mov si, msg_same
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        ; ¿existe? ¿es un disco duro?
        call drv_params
        jnc .ex
        mov si, msg_nofmt
        call puts
        mov ax, 0x4C02
        int 0x21
.ex:
        ; ¿tiene formato? (INT 21/36 es seguro aunque la unidad no valga;
        ; sin formato no puede haber Hispa-DOS, así que ni se busca)
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        je .unformatted
        ; con formato: ¿ya hay Hispa-DOS en el destino? (actualizar en el sitio)
        call detect_hispados
        jc .hasfmt                  ; no hay: flujo normal sobre disco formateado
        ; ya hay Hispa-DOS: menú Actualizar / Nueva / Cancelar
        call ask_mode               ; pone [updmode] y [donew], o cancela
        cmp byte [updmode], 0
        jne .upd_confirm
        ; [N] instalación nueva: formatear aunque ya tuviera formato (borra todo)
        mov si, msg_asknew1
        call puts
        mov si, msg_askfmt2
        call puts
        jmp .doformat
.unformatted:
        ; sin formato: se le da (preguntando antes), como siempre
        mov si, msg_askfmt1
        call puts
        mov si, msg_askfmt2
        call puts
.doformat:
        call ask_yes
        jc cancel
        mov si, msg_crlf
        call puts
        mov dx, fmt_path
        mov bx, fmt_tail
        call run
        jc .fmterr
        test al, al
        jnz .fmterr
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .fmt
.fmterr:
        mov si, msg_fmterr
        call puts
        mov ax, 0x4C02
        int 0x21
.hasfmt:
        cmp byte [guided], 0
        je .fmt
        mov si, msg_askins1
        call puts
        mov si, msg_askins2
        call puts
        call ask_yes
        jc cancel
        jmp .fmt
.upd_confirm:
        ; [HISPA-486] ¿de qué edición son el disco y el origen?
        call ed_mirar
        cmp byte [ed_dst], 0
        je .ed_conv
        cmp byte [ed_src], 0
        jne .ed_ok
        mov al, [src]               ; desde disquetes, el de Hispa-486 se pide
        and al, 0xDF                ; al final (disq486)
        cmp al, 'B'
        jbe .ed_ok
        mov si, msg_ed_baja         ; un Hispa-486 con un origen de Hispa-386: no
        call puts
        jmp cancel
.ed_conv:
        cmp byte [ed_src], 0
        je .ed_ok
        mov byte [ed_pasar], 1      ; de Hispa-386 a Hispa-486
        mov si, msg_ed_pasa
        call puts
.ed_ok:
        ; ACTUALIZAR en el sitio: resumen y confirmación; nunca se formatea
        mov si, msg_upd1
        call puts
        mov si, msg_upd2
        call puts
        call ask_yes
        jc cancel
        mov si, msg_crlf
        call puts
        ; guardar el CONFIG.SYS y el AUTOEXEC.BAT del usuario como .ANT
        call backup_user_cfg
        cmp byte [ed_pasar], 0      ; [HISPA-486] y, al pasar de edición, su
        je .fmt                     ; VENTANAS.INI y su CENTRO.INI
        call ed_guardar
.fmt:
instala_comun:
        ; 1) sistema: SYS origen: unidad:
        cmp byte [setup], 0
        je .sys
        mov si, sc_copia
        mov bx, pie_espera
        mov dx, ay_copia
        call pantalla
.sys:
        mov dx, sys_path
        mov bx, sys_tail
        call run
        jc .syserr
        test al, al
        jz .copy
.syserr:
        mov si, msg_syserr
        cmp byte [setup], 0
        jne s_error
        call puts
        mov ax, 0x4C03
        int 0x21
.copy:
        ; 2) archivos
        cmp byte [setup], 0
        je .copy_n
        mov bx, copia_todo          ; [SETUP] con la barra de progreso
        call con_progreso
        jmp .copied
.copy_n:
        mov si, msg_copying
        call puts
        call copia_todo
        jmp .copied

.copied:
        cmp word [nfailed], 0
        je .allok
        ; alguno no se ha copiado (disco lleno...): no decir que está instalado
        mov si, msg_fail
        cmp byte [setup], 0
        je .fail_n
        mov si, sc_fallo
        jmp s_error
.fail_n:
        call puts
        movzx eax, word [nfailed]
        call putnum
        mov si, msg_fail2
        call puts
        mov ax, 0x4C04
        int 0x21
.allok:
        cmp byte [setup], 0
        jne .after_msg
        cmp byte [updmode], 0
        jne .updmsg
        mov si, msg_done
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_done2
        call puts
        jmp .after_msg
.updmsg:
        ; ACTUALIZAR: qué se cambió y qué se conservó
        mov si, msg_updone
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_updone2
        call puts
        mov si, msg_updkept
        call puts
        movzx eax, word [nkept]
        call putnum
        mov si, msg_updkept2
        call puts
        cmp byte [bak_auto], 0
        je .nba
        mov si, msg_bak_a
        call puts
.nba:
        cmp byte [bak_conf], 0
        je .after_msg
        mov si, msg_bak_c
        call puts
.after_msg:
        ; 3) disco duro: la partición, activa, y el MBR si no tiene (solo en
        ; instalación nueva; ACTUALIZAR no toca la tabla de particiones)
        cmp byte [updmode], 0
        jne .snd
        ; (PARTIR /ARRANQUE:X:; en el disco de arranque pide escribir SI)
        cmp byte [fixed], 0
        je .snd
        mov dx, part_path
        mov bx, arr_tail
        call run
        jc .arrerr
        test al, al
        jz .snd
.arrerr:
        mov si, msg_arrerr
        cmp byte [setup], 0
        jne s_error
        call puts
        mov ax, 0x4C05
        int 0x21
.snd:
        ; 4) la comprobación del sonido, con el disco recién instalado. En
        ; ACTUALIZAR no se re-sondea: se conserva el SONIDO.INI del usuario
        cmp byte [updmode], 0
        jne .end
        cmp byte [nosnd], 0
        jne .end
        call sound_check
.end:
        call instal2                ; [0.9.24] el disquete «Instalación 2», si hace falta
        call oficina                ; [0.9.22] el disquete «Oficina», si hace falta
        call disq486                ; [HISPA-486] y el disquete «Hispa-486», si se quiere
        call disqinet               ; [INTERNET] y, en Hispa-486, el disquete «Internet»
        cmp byte [setup], 0
        jne setup_fin
        cmp byte [updmode], 0
        jne .end_upd
        mov si, msg_boot
        call puts
        cmp byte [guided], 0
        je .e2
        mov si, msg_reboot
        call puts
.e2:
        mov ax, 0x4C00
        int 0x21
.end_upd:
        mov si, msg_updboot
        call puts
        mov ax, 0x4C00
        int 0x21

; [SETUP] todo lo que se copia del origen (dos veces en SETUP: una para
; contar y otra para copiar)
copia_todo:
        ; el sistema: \HISPADOS entero. En ACTUALIZAR se conservan los
        ; *.INI y *.REC que ya existan (datos y ajustes del usuario)
        mov al, [updmode]
        mov [keepdata], al
        mov byte [keep], 0
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
        mov si, s_hspj              ; y los juegos de Hispa Ventanas
        mov di, s_hjdir
        call make_dir
        call copy_set
        mov si, s_hspn              ; con los cartuchos de los emuladores,
        mov di, s_hndir             ; cada uno en su carpeta
        call make_dir
        call copy_set
        mov si, s_hsps
        mov di, s_hsdir
        call make_dir
        call copy_set
        mov si, s_hspp              ; y sus accesorios (Pintura)
        mov di, s_hpdir
        call make_dir
        call copy_set
        call copia_h32              ; [HISPA-486] y \HISPA32, si la trae
        call copia_inet             ; [INTERNET] y \HISPADOS\INTERNET, si lo trae
        mov byte [keepdata], 0
        ; AUTOEXEC.BAT y CONFIG.SYS
        cmp byte [updmode], 0
        jne .cfg_upd
        ; instalación nueva: se respetan los que ya tenga la unidad
        mov si, s_autoexec
        mov di, s_root
        mov byte [keep], 1
        mov byte [fix], 1
        call copia_auto
        mov si, s_config
        mov di, s_root
        call copy_set
        jmp .cfg_done
.cfg_upd:
        ; ACTUALIZAR: ya se guardaron como .ANT; se ponen los nuevos (a C:)
        mov byte [keep], 0
        mov byte [fix], 1
        mov si, s_autoexec
        mov di, s_root
        call copia_auto
        mov si, s_config
        mov di, s_root
        call copy_set
.cfg_done:
        mov byte [keep], 0
        mov byte [fix], 0
        mov si, s_leeme
        mov di, s_root
        call copy_set
        ret

cancel:
        mov si, msg_nothing
        call puts
        mov ax, 0x4C06
        int 0x21

; AL = letra de la unidad de destino
set_dst:
        mov [dst], al
        mov [tail_drv], al
        mov [msg_drv1], al
        mov [msg_drv2], al
        mov [msg_drv3], al
        mov [msg_drv4], al
        mov [aud_path], al
        mov [aud_d1], al
        mov [aud_d2], al
        mov [fmt_drv], al
        mov [arr_drv], al
        mov [msg_menu_drv], al
        mov [msg_upd_drv], al
        mov [msg_updone_d], al
        mov [msg_asknew_drv], al
        mov [sc_d1], al             ; [SETUP]
        mov [sc_d2], al
        mov [sc_d3], al
        mov [sc_d4], al
        mov [sc_d5], al
        ret

; sin unidad: PARTIR /INSTALAR elige el disco duro (nunca el de arranque),
; ofrece crear la partición y devuelve su letra como nivel de error
guided_pick:
        mov dx, part_path
        mov bx, inst_tail
        call run
        jc .noprog
        cmp al, 'C'
        jb .none
        cmp al, 'Z'
        ja .none
        call set_dst
        ret
.none:
        test al, al
        jz cancel
        mov si, msg_nothing
        call puts
        mov ax, 0x4C01
        int 0x21
.noprog:
        mov si, msg_nopart
        call puts
        mov ax, 0x4C01
        int 0x21

; parámetros de la unidad de destino -> [fixed]; CF si no existe
drv_params:
        mov ax, 0x440D
        mov bl, [dst]
        sub bl, 'A' - 1
        mov cx, 0x0860
        mov dx, params
        int 0x21
        jc .r
        mov byte [fixed], 0
        cmp byte [params+1], 5
        jne .r
        mov byte [fixed], 1
        clc
.r:
        ret

; DX = programa, BX = su cola de órdenes -> AL = su nivel de error; CF si no
; se ha podido ejecutar
run:
        cmp byte [setup], 0         ; [SETUP] lo que escriben, a NUL (la
        je run_tal                  ; pantalla es la del programa)
        push dx
        push bx
        mov ah, 0x45
        mov bx, 1
        int 0x21
        mov [so_guarda], ax
        mov dx, s_nul
        mov ax, 0x3D01
        int 0x21
        jc .sin
        mov bx, ax
        mov cx, 1
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
.sin:
        pop bx
        pop dx
        call run_tal
        pushf
        push ax
        mov bx, [so_guarda]
        mov cx, 1
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
        pop ax
        popf
        ret
run_tal:
        mov word [pblock], 0
        mov [pblock+2], bx
        mov [pblock+4], cs
        mov dword [pblock+6], 0
        mov dword [pblock+10], 0
        mov bx, pblock
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        jc .r
        mov ah, 0x4D
        int 0x21
        clc
.r:
        ret

; [0.9.24] Lo que ya no cabe en el disquete de arranque (los programas de
; Windows de consola: HW32 y MSVCRT.DLL, de Hispa-486) va en el disquete
; «Instalación 2», en su \HISPADOS. Igual que «Oficina»: si se instala desde
; A: o B: y el origen no lo trae, se ofrece ese disquete y se copia. [GLIFOS]
; También su \HISPADOS\PROGS: lo de la oficina que ya no cabe en «Oficina»
; (TIPOS32.DAT, los tipos de letra grandes).
instal2:
        mov al, [src]
        and al, 0xDF
        cmp al, 'B'
        ja .r
        mov [i2_ruta], al
        mov dx, i2_ruta             ; ¿el origen ya trae HW32?
        mov ax, 0x3D00
        int 0x21
        jc .pregunta
        mov bx, ax
        mov ah, 0x3E
        int 0x21
.r:
        ret
.pregunta:
        mov si, msg_i2_preg
        mov bx, sd_i2
        mov dx, i2_ruta             ; ¿es el de Instalación 2?
        mov di, msg_i2_no
        call pide_disco
        jc .r
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [keepdata], 0
        mov byte [fix], 0
        mov bx, .copia
        call copia_disco
        jnc .msg
        ret
.copia:
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
        mov si, s_hspp              ; [GLIFOS] y su \HISPADOS\PROGS (si lo trae)
        mov di, s_hpdir
        call make_dir
        jmp copy_set
.msg:
        mov si, msg_i2_hecho
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_of_hecho2
        call puts
        cmp word [nfailed], 0
        je .vuelta
        mov si, msg_of_fallo
        call puts
.vuelta:
        mov si, msg_of_vuelve       ; (el intérprete vuelve a leerse del disquete)
        call puts
        mov ah, 0x08
        int 0x21
        mov si, msg_crlf
        jmp puts

; [0.9.22] La familia de oficina (Pluma, Cuentas...) no cabe en el disquete de
; arranque: va en el disquete «Oficina». Si se instala desde A: o B: y el
; origen no la trae, se ofrece ese disquete y se copia su \HISPADOS\PROGS.
; Desde la memoria USB (o con ACTUALIZAR, que la trae) no se pregunta nada.
oficina:
        mov al, [src]
        and al, 0xDF
        cmp al, 'B'
        ja .r
        mov [of_ruta], al
        mov dx, of_ruta             ; ¿el origen ya trae Pluma?
        mov ax, 0x3D00
        int 0x21
        jc .pregunta
        mov bx, ax
        mov ah, 0x3E
        int 0x21
.r:
        ret
.pregunta:
        mov si, msg_of_preg
        mov bx, sd_of
        mov dx, of_ruta             ; ¿es el de Oficina?
        mov di, msg_of_no
        call pide_disco
        jc .r
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [keepdata], 0
        mov byte [fix], 0
        mov bx, .copia
        call copia_disco
        jnc .msg
        ret
.copia:
        mov si, s_hspp
        mov di, s_hpdir
        call make_dir
        jmp copy_set
.msg:
        mov si, msg_of_hecho
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_of_hecho2
        call puts
        cmp word [nfailed], 0
        je .vuelta
        mov si, msg_of_fallo
        call puts
.vuelta:
        mov si, msg_of_vuelve       ; (el intérprete vuelve a leerse del disquete)
        call puts
        mov ah, 0x08
        int 0x21
        mov si, msg_crlf
        jmp puts

; [HISPA-486] Instalando desde disquetes (los de Hispa-386), el último que se
; ofrece es el de «Hispa-486»: \HISPA32, lo de la edición en \HISPADOS
; (EDICION.INI, VENTANAS.INI, CENTRO.INI) y el AUTOEXEC.BAT que arranca el
; escritorio (si el de Hispa-386 se ha puesto ahora). Con N, queda Hispa-386.
disq486:
        mov al, [src]
        and al, 0xDF
        cmp al, 'B'
        ja .r
        mov [d4_ruta], al
        mov dx, d4_ruta             ; ¿el origen ya es de Hispa-486?
        call existe
        jnc .r
.pregunta:
        mov si, msg_d4_preg
        mov bx, sd_d4
        mov dx, d4_ruta             ; ¿es el de Hispa-486?
        mov di, msg_d4_no
        call pide_disco
        jc .r
        mov al, [dst]               ; un Hispa-386 que pasa a Hispa-486: sus
        mov [ed_ruta], al           ; VENTANAS.INI y CENTRO.INI, a .ANT
        mov dx, ed_ruta
        call existe
        jnc .yaes
        call ed_guardar
.yaes:
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [fix], 0
        mov al, [updmode]
        mov [keepdata], al
        mov bx, .copia
        call copia_disco
        jnc .msg
        ret
.copia:
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
        call copia_h32
        mov byte [keepdata], 0
        cmp byte [auto_puesto], 0
        je .sinauto
        mov byte [fix], 1
        mov si, s_autoexec
        mov di, s_root
        call copy_set
        mov byte [fix], 0
.sinauto:
        ret
.msg:
        mov si, msg_d4_hecho
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_of_hecho2
        call puts
        cmp word [nfailed], 0
        je .vuelta
        mov si, msg_of_fallo
        call puts
.vuelta:
        mov si, msg_of_vuelve       ; (el intérprete vuelve a leerse del disquete)
        call puts
        mov ah, 0x08
        int 0x21
        mov si, msg_crlf
        jmp puts
.r:
        ret

; [HISPA-486] \HISPA32 (los programas de 32 bits), si el origen la trae
copia_h32:
        mov al, [src]
        mov [h32_ruta], al
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, h32_ruta
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jc .r
        test byte [dta+0x15], 0x10
        jz .r
        mov si, s_h32
        mov di, s_h32dir
        call make_dir
        call copy_set
.r:
        ret

; [INTERNET] \HISPADOS\INTERNET (el Navegador), si el origen lo trae
copia_inet:
        mov al, [src]
        mov [in_ruta], al
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, in_ruta
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jc .r
        test byte [dta+0x15], 0x10
        jz .r
        mov si, s_in
        mov di, s_indir
        call make_dir
        call copy_set
.r:
        ret

; [INTERNET] Instalando desde disquetes un Hispa-486 (el disco tiene ya
; EDICION.INI), se ofrece al final el disquete «Internet» (make internet: el
; Navegador y sus DLL, en la raíz): sus programas, DLL, NAVEGA.INI y
; LEEME.TXT van a \HISPADOS\INTERNET (HW32.COM ya está en \HISPADOS). Hispa
; Ventanas tiene ya su acceso en el grupo «Internet» de CENTRO.INI.
disqinet:
        mov al, [src]
        and al, 0xDF
        cmp al, 'B'
        ja .r
        mov [in_ruta], al           ; ¿el origen ya lo trae?
        mov dx, in_ruta
        call existe_dir
        jnc .r
        mov al, [dst]               ; ¿el disco ha quedado de Hispa-486?
        mov [ed_ruta], al
        mov dx, ed_ruta
        call existe
        jc .r
        mov al, [src]
        mov [di_ruta], al
.pregunta:
        mov si, msg_di_preg
        mov bx, sd_di
        mov dx, di_ruta             ; ¿es el disquete «Internet»?
        mov di, msg_di_no
        call pide_disco
        jc .r
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [fix], 0
        mov al, [updmode]           ; al actualizar, su NAVEGA.INI se conserva
        mov [keepdata], al
        mov bx, .copia
        call copia_disco
        mov byte [keepdata], 0
        jnc .msg
        ret
.copia:
        mov di, s_indir
        call make_dir
        mov si, s_di_exe
        call copy_set
        mov di, s_indir
        mov si, s_di_dll
        call copy_set
        mov di, s_indir
        mov si, s_di_ini
        call copy_set
        mov di, s_indir
        mov si, s_di_txt
        jmp copy_set
.msg:
        mov si, msg_di_hecho
        call puts
        movzx eax, word [ncopied]
        call putnum
        mov si, msg_of_hecho2
        call puts
        cmp word [nfailed], 0
        je .vuelta
        mov si, msg_of_fallo
        call puts
.vuelta:
        mov si, msg_of_vuelve       ; (el intérprete vuelve a leerse del disquete)
        call puts
        mov ah, 0x08
        int 0x21
        mov si, msg_crlf
        jmp puts
.r:
        ret

; DX = ruta -> CF=0 si es una carpeta que existe
existe_dir:
        push dx
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        pop dx
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jc .r
        test byte [dta+0x15], 0x10
        jnz .r
        stc
.r:
        ret

; AUTOEXEC.BAT (SI, DI y las señales ya puestas): copy_set, y apuntar si se ha
; puesto el del sistema (el disquete «Hispa-486» lo cambia entonces por el suyo)
copia_auto:
        mov ax, [ncopied]
        push ax
        call copy_set
        pop ax
        cmp ax, [ncopied]
        je .r
        mov byte [auto_puesto], 1
.r:
        ret

; DX = ruta -> CF=0 si el archivo existe (se abre y se cierra)
existe:
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        clc
.r:
        ret

; [HISPA-486] ¿llevan el origen y el destino \HISPADOS\EDICION.INI?
; -> [ed_src], [ed_dst] = 1 si sí
ed_mirar:
        mov al, [src]
        mov [ed_ruta], al
        mov dx, ed_ruta
        call existe
        setnc [ed_src]
        mov al, [dst]
        mov [ed_ruta], al
        mov dx, ed_ruta
        call existe
        setnc [ed_dst]
        ret

; [HISPA-486] al pasar un Hispa-386 a Hispa-486: su VENTANAS.INI y su
; CENTRO.INI, como VENTANAS.ANT y CENTRO.ANT (así se ponen los de la edición:
; el escritorio de Hispa-486 y el grupo «Hispa32»)
ed_guardar:
        mov al, [dst]
        mov [bk_vi], al
        mov [bk_va], al
        mov [bk_ci], al
        mov [bk_cn], al
        mov si, bk_vi
        mov di, bk_va
        call backup_one
        jc .c
        mov si, msg_bak_v
        call puts
.c:
        mov si, bk_ci
        mov di, bk_cn
        call backup_one
        jc .r
        mov si, msg_bak_n
        call puts
.r:
        ret

; ¿S/N? (se escribe la respuesta y se pulsa Intro) -> CF=0 si es que sí
ask_yes:
        mov byte [lbuf], 8
        mov dx, lbuf
        mov ah, 0x0A
        int 0x21
        mov si, msg_crlf
        call puts
        cmp byte [lbuf+1], 0
        je .no
        mov al, [lbuf+2]
        and al, 0xDF
        cmp al, 'S'
        je .y
        cmp al, 'Y'
        je .y
.no:
        stc
        ret
.y:
        clc
        ret

; AUDICION /RAPIDA del disco instalado: lo aprendido, en su SONIDO.INI
; (AUDICION dice ella misma cómo ha ido; aquí, solo si no ha podido empezar)
sound_check:
        mov si, msg_snd
        call puts
        mov dx, aud_path
        mov bx, aud_tail
        call run
        jc .fail
        cmp al, 1                   ; 1: sin memoria (0 y 2 los explica ella)
        jne .r
.fail:
        mov si, msg_sndko
        call puts
.r:
        mov si, msg_crlf
        jmp puts

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; =============================================================================
;  [CD] El origen de la instalación
; =============================================================================
; /ORIGEN:x: o, arrancando de disquete, el CD de Hispa-DOS (una unidad con
; \HISPADOS\HISPACD.INI) -> [src]. Mientras se busca, los errores de unidad
; (un lector sin disco...) fallan sin preguntar (INT 24h propia).
elige_origen:
        mov al, [origen]
        test al, al
        jz .cd
        mov [src], al
        ret
.cd:
        mov al, [arranque]
        and al, 0xDF
        cmp al, 'B'
        ja .r
        call i24_pon
        mov al, 'C'
.l:
        mov [cd_ruta], al
        push ax
        mov dx, cd_ruta
        call existe
        pop ax
        jnc .si
        inc al
        cmp al, 'Z'
        jbe .l
        jmp .q
.si:
        mov [src], al
        mov byte [desde_cd], 1
.q:
        call i24_quita
.r:
        ret

; [src] -> las rutas de los programas que se usan del origen
pon_origen:
        mov dl, [src]
        mov [sys_path], dl
        mov [sys_src], dl
        mov [part_path], dl
        mov [fmt_path], dl
        ret

; [SETUP] tras cambiar las particiones el núcleo reparte otra vez las letras
; (el CD, que era C: con el disco vacío, pasa a D:): volver a buscar el CD
rebusca_cd:
        cmp byte [desde_cd], 0
        je .r
        call elige_origen.cd
        call pon_origen
.r:
        ret

; INT 24h: «fallar» sin preguntar (AL = 3)
i24_fallar:
        mov al, 3
        iret
i24_pon:
        push es
        mov ax, 0x3524
        int 0x21
        mov [i24_viejo], bx
        mov [i24_viejo+2], es
        pop es
        mov dx, i24_fallar
        mov ax, 0x2524
        int 0x21
        ret
i24_quita:
        push ds
        lds dx, [i24_viejo]
        mov ax, 0x2524
        int 0x21
        pop ds
        ret

; =============================================================================
;  [SETUP] INSTALAR /SETUP: el programa de instalación con pantallas
; =============================================================================
setup_main:
        call i24_pon                ; (un disquete que falta no pregunta)
        mov ah, 0x01                ; sin cursor
        mov cx, 0x2000
        int 0x10
        ; la edición del origen (Hispa-486 si trae EDICION.INI)
        mov al, [src]
        mov [ed_ruta], al
        mov dx, ed_ruta
        call existe
        jc .e386
        mov si, s_ed486
        mov di, cab_ed
        call strcpy_s
.e386:
        ; 1) bienvenida
        mov si, sc_bienv
        mov bx, pie_bienv
        mov dx, ay_bienv
        call pantalla
.b:
        call tecla_s
        cmp al, 13
        jne .b
        ; 2) el disco duro: ¿cómo está?
.disco:
        mov dx, part_path
        mov bx, situ_tail
        call run
        jc .nopart
        cmp al, 'C'
        jae .letra
        cmp al, 2
        je .vacio
        cmp al, 3
        je .sitio
        cmp al, 4
        je .lleno
        ; 1 (u otro): sin disco duro
        mov si, sc_sindisco
        mov bx, pie_salir
        mov dx, ay_disco
        call pantalla
.sd:
        call tecla_s
        cmp al, 13
        jne .sd
        jmp s_salir
.nopart:
        mov si, sc_nopart
        jmp s_error
.vacio:
        mov si, sc_vacio
        mov bx, pie_intro
        mov dx, ay_disco
        call pantalla
.v:
        call tecla_s
        cmp al, 13
        jne .v
        jmp .crear
.sitio:
        mov si, sc_sitio
        mov bx, pie_sitio
        mov dx, ay_disco
        call pantalla
.st:
        call tecla_s
        cmp al, 13
        je .crear
        and al, 0xDF
        cmp al, 'B'
        jne .st
        jmp .borrar
.lleno:
        mov si, sc_lleno
        mov bx, pie_lleno
        mov dx, ay_disco
        call pantalla
.ll:
        call tecla_s
        and al, 0xDF
        cmp al, 'B'
        jne .ll
.borrar:
        mov si, sc_borrar
        mov bx, pie_borrar
        mov dx, ay_disco
        call pantalla
.bo:
        call tecla_s
        cmp al, 27
        je .disco
        and al, 0xDF
        cmp al, 'S'
        jne .bo
        mov si, sc_preparando
        mov bx, pie_espera
        mov dx, ay_disco
        call pantalla
        mov dx, part_path
        mov bx, borr_tail
        call run
        jc .nopart
        push ax
        call rebusca_cd
        pop ax
        test al, al
        jz .crear2
        mov si, sc_parterr
        jmp s_error
.crear:
        mov si, sc_preparando
        mov bx, pie_espera
        mov dx, ay_disco
        call pantalla
.crear2:
        mov dx, part_path
        mov bx, inst_si_tail
        call run
        jc .nopart
        push ax
        call rebusca_cd
        pop ax
        cmp al, 'C'
        jb .parterr
        cmp al, 'Z'
        ja .parterr
        mov byte [s_formatear], 1   ; partición nueva: formato sin preguntar más
        call set_dst
        call drv_params
        jnc .carpeta
        ; el núcleo no ve aún la unidad: reiniciar y volver a empezar
        mov si, sc_reinicio
        mov bx, pie_reinicio
        mov dx, ay_disco
        call pantalla
.ri:
        call tecla_s
        cmp al, 13
        jne .ri
        jmp reinicia
.parterr:
        mov si, sc_parterr
        jmp s_error
.letra:
        call set_dst
        call drv_params
        jnc .ex
        mov si, sc_parterr
        jmp s_error
.ex:
        ; ¿tiene formato?
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .confmt
        mov si, sc_sinfmt
        mov bx, pie_intro
        mov dx, ay_formato
        call pantalla
.sf:
        call tecla_s
        cmp al, 13
        jne .sf
        mov byte [s_formatear], 1
        jmp .carpeta
.confmt:
        call detect_hispados
        jc .sinhsp
        ; ya tiene Hispa-DOS: Actualizar o Nueva
        mov si, sc_yahay
        mov bx, pie_yahay
        mov dx, ay_actualizar
        call pantalla
.yh:
        call tecla_s
        and al, 0xDF
        cmp al, 'N'
        je .nueva
        cmp al, 'A'
        jne .yh
        mov byte [updmode], 1
        ; [HISPA-486] las ediciones del disco y del origen
        call ed_mirar
        cmp byte [ed_dst], 0
        je .edc
        cmp byte [ed_src], 0
        jne .carpeta
        mov al, [src]
        and al, 0xDF
        cmp al, 'B'
        jbe .carpeta                ; (desde disquetes, el de Hispa-486 se pide)
        mov si, sc_edbaja
        jmp s_error
.edc:
        cmp byte [ed_src], 0
        je .carpeta
        mov byte [ed_pasar], 1
        jmp .carpeta
.nueva:
        mov byte [s_formatear], 1
        jmp .carpeta
.sinhsp:
        mov si, sc_conserva
        mov bx, pie_intro
        mov dx, ay_disco
        call pantalla
.cs:
        call tecla_s
        cmp al, 13
        jne .cs
        ; 3) la carpeta de destino
.carpeta:
        mov si, sc_carpeta
        mov bx, pie_intro
        mov dx, ay_carpeta
        call pantalla
        mov al, [src]               ; de dónde se copia
        mov [sc_orig_d], al
        mov dx, 0x1104
        mov si, sc_orig
        call txt_at
        cmp byte [desde_cd], 0
        je .ca
        mov si, sc_orig_cd
        call txt_at_aqui
.ca:
        call tecla_s
        cmp al, 13
        jne .ca
        ; 4) el formato
        cmp byte [s_formatear], 0
        je .sinf
        mov si, sc_formato
        mov bx, pie_espera
        mov dx, ay_formato
        call pantalla
        mov dx, fmt_path
        mov bx, fmt_tail
        call run
        jc .fmterr
        test al, al
        jnz .fmterr
        mov dl, [dst]
        sub dl, 'A' - 1
        mov ah, 0x36
        int 0x21
        cmp ax, 0xFFFF
        jne .sinf
.fmterr:
        mov si, sc_fmterr
        jmp s_error
.sinf:
        cmp byte [updmode], 0
        je instala_comun
        call backup_user_cfg        ; ACTUALIZAR: los suyos, como .ANT
        cmp byte [ed_pasar], 0
        je instala_comun
        call ed_guardar
        jmp instala_comun

; el final: instalado; quitar el medio y reiniciar
setup_fin:
        mov si, sc_fin
        cmp byte [updmode], 0
        je .n
        mov si, sc_finupd
.n:
        mov bx, pie_fin
        mov dx, ay_fin
        call pantalla
.t:
        call tecla_s
        cmp al, 13
        jne .t
reinicia:
        call i24_quita
        mov ah, 0x0D                ; que no quede nada sin escribir
        int 0x21
        mov ax, 0x0003
        int 0x10
        mov al, 0xFE                ; reinicio por el controlador del teclado
        out 0x64, al
        mov cx, 0xFFFF
.e:
        loop .e
        jmp 0xFFFF:0x0000

; SI = texto del error: pantalla de error y salir (nivel 2)
s_error:
        mov bx, pie_salir
        mov dx, ay_error
        call pantalla
.t:
        call tecla_s
        cmp al, 13
        jne .t
        mov byte [s_nivel], 2
        jmp s_salir_ya

; F3: ¿salir? (INTRO = seguir; F3 = salir)
s_pregunta_salir:
        push word [p_cuerpo]
        push word [p_pie]
        push word [p_ayuda]
        mov si, sc_salir
        mov bx, pie_salir2
        mov dx, ay_bienv
        call pantalla
.t:
        call tecla_raw
        cmp ax, 0x3D00
        je s_salir
        cmp al, 13
        jne .t
        pop dx
        pop bx
        pop si
        jmp pantalla                ; la que había

s_salir:
        mov byte [s_nivel], 6
s_salir_ya:
        call i24_quita
        mov ax, 0x0003
        int 0x10
        mov si, msg_s_salido
        call puts
        mov al, [arranque]
        and al, 0xDF
        cmp al, 'B'
        ja .x
        mov si, msg_of_vuelve2
        call puts
.x:
        mov ah, 0x4C
        mov al, [s_nivel]
        int 0x21

; tecla -> AX (AL = carácter; si es 0, AH = código de la tecla especial)
tecla_raw:
        mov ah, 0x07
        int 0x21
        xor ah, ah
        test al, al
        jnz .r
        mov ah, 0x07
        int 0x21
        mov ah, al
        xor al, al
.r:
        ret

; tecla de una pantalla: F1 enseña la ayuda, F3 pregunta si salir; las
; demás -> AX
tecla_s:
        call tecla_raw
        cmp ax, 0x3B00              ; F1
        je .ayuda
        cmp ax, 0x3D00              ; F3
        jne .r
        call s_pregunta_salir
        jmp tecla_s
.ayuda:
        push word [p_cuerpo]
        push word [p_pie]
        push word [p_ayuda]
        mov si, [p_ayuda]
        mov bx, pie_ayuda
        mov dx, ay_bienv
        call pantalla
.a:
        call tecla_raw
        cmp al, 13
        je .v
        cmp al, 27
        jne .a
.v:
        pop dx
        pop bx
        pop si
        call pantalla
        jmp tecla_s
.r:
        ret

; SI = cuerpo, BX = pie, DX = ayuda: pintar la pantalla del programa
pantalla:
        mov [p_cuerpo], si
        mov [p_pie], bx
        mov [p_ayuda], dx
        mov ax, 0x0600              ; toda azul
        mov bh, 0x1F
        xor cx, cx
        mov dx, 0x184F
        int 0x10
        mov ax, 0x0600              ; el pie, en gris
        mov bh, 0x70
        mov cx, 0x1800
        mov dx, 0x184F
        int 0x10
        xor dx, dx
        mov si, s_nl
        call txt_at
        mov dx, 0x0102
        mov si, s_cab
        call txt_at
        mov dx, 0x0202
        mov si, s_raya
        call txt_at
        mov dx, 0x0404
        mov si, [p_cuerpo]
        call txt_at
        mov si, s_nl                ; (por el puerto serie: el pie, aparte)
        call txt_at_aqui
        mov dx, 0x1802
        mov si, [p_pie]
        call txt_at
        ret

; DH = fila, DL = columna, SI = texto (10 = línea siguiente en la misma
; columna; 1 = esa línea en amarillo, un aviso)
txt_at:
        mov [t_fila], dh
        mov [t_col], dl
        call cur_pon
.l:
        lodsb
        test al, al
        jz .r
        cmp al, 10
        je .nl
        cmp al, 1
        je .am
        mov dl, al
        mov ah, 0x02
        int 0x21
        jmp .l
.nl:
        mov dl, 13                  ; (por el puerto serie, una línea nueva)
        mov ah, 0x02
        int 0x21
        mov dl, 10
        int 0x21
        inc byte [t_fila]
        mov dh, [t_fila]
        mov dl, [t_col]
        call cur_pon
        jmp .l
.am:
        mov ax, 0x0600
        mov bh, 0x1E
        mov ch, [t_fila]
        mov cl, [t_col]
        mov dh, ch
        mov dl, 77
        int 0x10
        jmp .l
.r:
        ret
cur_pon:
        mov ah, 0x02
        xor bh, bh
        int 0x10
        ret

; BX = rutina de copia: en SETUP, contar, la pantalla con la barra y copiar
; (CF = 1: no hace falta decir nada más); si no, solo copiar (CF = 0)
copia_disco:
        cmp byte [setup], 0
        jne .s
        call bx
        clc
        ret
.s:
        call con_progreso
        stc
        ret
con_progreso:
        mov [prog_rut], bx
        mov byte [solo_contar], 1
        mov word [ntotal], 0
        call bx
        mov byte [solo_contar], 0
        mov word [nhechos], 0
        mov byte [barra_n], 0xFF
        mov si, sc_copia
        mov bx, pie_espera
        mov dx, ay_copia
        call pantalla
        mov ax, 0x0600              ; la barra, en amarillo
        mov bh, 0x1E
        mov cx, 0x0C0A
        mov dx, 0x0C45
        int 0x10
        mov dx, 0x0C0A              ; la barra vacía
        mov si, s_barra0
        call txt_at
        mov word [cs_aviso], aviso_archivo
        call [prog_rut]
        mov word [cs_aviso], 0
        mov ax, [ncopied]
        add [ncop_total], ax
        ret

; antes de cada archivo: la barra y su nombre (path_s)
aviso_archivo:
        pushad
        mov ax, [nhechos]
        inc ax
        mov [nhechos], ax
        mov cx, [ntotal]
        cmp ax, cx
        jbe .ok
        mov ax, cx
.ok:
        mov bx, 60                  ; trozos llenos = hechos * 60 / total
        mul bx
        test cx, cx
        jz .lleno
        div cx
        jmp .n
.lleno:
        mov ax, 60
.n:
        cmp al, [barra_n]
        je .nombre
        mov [barra_n], al
        movzx cx, al
        push cx
        mov dx, 0x0C0A
        call cur_pon
        pop cx
        jcxz .pc
.b:
        mov dl, 0xDB
        mov ah, 0x02
        int 0x21
        loop .b
.pc:
        mov dx, 0x0C47              ; el tanto por ciento
        call cur_pon
        movzx ax, byte [barra_n]
        imul ax, ax, 100
        mov bl, 60
        div bl
        movzx ax, al
        call num3
        mov si, s_pc
        call txt_at_aqui
.nombre:
        mov dx, 0x0E0A              ; «Copiando: \HISPADOS\EDITAR.COM»
        mov si, s_copiando
        call txt_at
        mov si, path_s+2
        xor cx, cx
.c:
        lodsb
        test al, al
        jz .pad
        mov dl, al
        mov ah, 0x02
        int 0x21
        inc cx
        jmp .c
.pad:
        cmp cx, 48
        jae .fin
        mov dl, ' '
        mov ah, 0x02
        int 0x21
        inc cx
        jmp .pad
.fin:
        mov dl, 13                  ; (por el puerto serie, una línea nueva)
        mov ah, 0x02
        int 0x21
        popad
        ret
; AX (0-999) -> tres cifras, con espacios delante, donde está el cursor
num3:
        mov bx, 10
        xor dx, dx
        div bx
        add dl, '0'
        mov [n3+2], dl
        xor dx, dx
        div bx
        add dl, '0'
        mov [n3+1], dl
        xor dx, dx
        div bx
        add dl, '0'
        mov [n3], dl
        cmp byte [n3], '0'
        jne .p
        mov byte [n3], ' '
        cmp byte [n3+1], '0'
        jne .p
        mov byte [n3+1], ' '
.p:
        mov si, n3
        jmp txt_at_aqui
; SI = texto, donde está el cursor
txt_at_aqui:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 0x02
        int 0x21
        jmp txt_at_aqui
.r:
        ret

; SI <- DI (ASCIIZ)
strcpy_s:
        lodsb
        mov [di], al
        inc di
        test al, al
        jnz strcpy_s
        ret

; [SETUP] pedir un disquete. SI = pregunta (sin SETUP), BX = pantalla (en
; SETUP), DX = archivo que lo distingue, DI = «ese no es» -> CF = 1 si no se
; quiere (N, o ESC en SETUP)
pide_disco:
        mov [pd_preg], si
        mov [pd_pant], bx
        mov [pd_ruta], dx
        mov [pd_malo], di
        mov byte [pd_err], 0
.p:
        cmp byte [setup], 0
        jne .s
        mov si, [pd_preg]
        call puts
        call ask_yes
        jc .r
        jmp .mira
.s:
        mov al, [src]
        mov [sd_letra1], al
        mov [sd_letra2], al
        mov si, [pd_pant]
        mov bx, pie_disco
        mov dx, ay_disquete
        call pantalla
        cmp byte [pd_err], 0
        je .t
        mov dx, 0x1204
        mov si, sc_noesdisco
        call txt_at
.t:
        call tecla_s
        cmp al, 27
        je .no
        cmp al, 13
        jne .t
.mira:
        mov dx, [pd_ruta]
        call existe
        jnc .r
        cmp byte [setup], 0
        jne .malo
        mov si, [pd_malo]
        call puts
        jmp .p
.malo:
        mov byte [pd_err], 1
        jmp .p
.no:
        stc
.r:
        ret

; El motor de copia de arbol (make_dir, copy_set, dest_exists, is_user_data,
; copy_file, fix_drive, strcpy) y sus datos (src, dst, keep, keepdata, fix,
; ncopied, nfailed, nkept, hs, hd, cs_dir, src_pre) estan en lib/copiaarbol.inc,
; compartido con ACTUALIZAR. Aqui src_pre queda vacio (origen = la unidad "X:").
%include "copiaarbol.inc"

; ¿ya hay Hispa-DOS en la unidad de destino? CF=0 si sí (hay \HISPADOS\ o
; KERNEL.SYS en la raíz). La unidad ya tiene formato cuando se llama aquí.
detect_hispados:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov al, [dst]
        mov [det_hdir], al
        mov dx, det_hdir
        mov cx, 0x10            ; carpetas
        mov ah, 0x4E
        int 0x21
        jnc .yes
        mov al, [dst]
        mov [det_kern], al
        mov dx, det_kern
        mov cx, 0x07            ; sistema + oculto + solo lectura
        mov ah, 0x4E
        int 0x21
        jnc .yes
        stc
        ret
.yes:
        clc
        ret

; menú al detectar un Hispa-DOS ya instalado: Actualizar / Nueva / Cancelar.
; Pone [updmode]=1 (actualizar) o [donew]=1 (nueva), o salta a cancel.
ask_mode:
        mov si, msg_menu1
        call puts
        mov si, msg_menu_drv
        call puts
        mov si, msg_menu2
        call puts
        mov byte [lbuf], 8
        mov dx, lbuf
        mov ah, 0x0A
        int 0x21
        mov si, msg_crlf
        call puts
        cmp byte [lbuf+1], 0
        je cancel               ; nada escrito: cancelar
        mov al, [lbuf+2]
        and al, 0xDF
        cmp al, 'A'
        je .upd
        cmp al, 'N'
        je .new
        jmp cancel
.upd:
        mov byte [updmode], 1
        ret
.new:
        mov byte [donew], 1
        ret

; ACTUALIZAR: guarda el CONFIG.SYS y el AUTOEXEC.BAT que tenga el usuario como
; CONFIG.ANT y AUTOEXEC.ANT (para que pueda recuperar sus cambios), antes de
; poner los nuevos. Avisa de lo que guarda.
backup_user_cfg:
        mov al, [dst]
        mov [bk_ab], al
        mov [bk_aa], al
        mov [bk_cs], al
        mov [bk_ca], al
        mov si, bk_ab           ; AUTOEXEC.BAT -> AUTOEXEC.ANT
        mov di, bk_aa
        call backup_one
        jc .conf
        mov byte [bak_auto], 1
        mov si, msg_bak_a
        call puts
.conf:
        mov si, bk_cs           ; CONFIG.SYS -> CONFIG.ANT
        mov di, bk_ca
        call backup_one
        jc .r
        mov byte [bak_conf], 1
        mov si, msg_bak_c
        call puts
.r:
        ret

; renombrar SI (origen) a DI (destino .ANT); borra antes un .ANT anterior.
; CF=0 si lo renombró (había un archivo del usuario)
backup_one:
        push si
        push di
        mov dx, di              ; quitar atributos de un .ANT anterior
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov dx, di              ; y borrarlo (si no está, no pasa nada)
        mov ah, 0x41
        int 0x21
        pop di
        pop si
        push si
        push di
        mov dx, si              ; DS:DX = nombre antiguo
                                ; ES:DI = nombre nuevo (DI ya apunta a él)
        mov ah, 0x56
        int 0x21                ; renombrar (CF si no existía el antiguo)
        pop di
        pop si
        ret
putnum:
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
puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

nosnd       db 0
aud_path    db "C:\HISPADOS\AUDICION.COM", 0
aud_tail    db aud_te - aud_tail - 1, " /RAPIDA "
aud_d1      db "C:\HISPADOS /INI:"
aud_d2      db "C:\HISPADOS\SONIDO.INI"
aud_te      db 13
guided      db 0
fixed       db 0
updmode     db 0                ; 1: ACTUALIZAR en el sitio (sin formatear)
donew       db 0                ; 1: el usuario eligió instalación nueva en el menú
bak_auto    db 0                ; se guardó AUTOEXEC.BAT como AUTOEXEC.ANT
bak_conf    db 0                ; se guardó CONFIG.SYS como CONFIG.ANT
det_hdir    db "C:\HISPADOS", 0
det_kern    db "C:\KERNEL.SYS", 0
bk_ab       db "C:\AUTOEXEC.BAT", 0
bk_aa       db "C:\AUTOEXEC.ANT", 0
bk_cs       db "C:\CONFIG.SYS", 0
bk_ca       db "C:\CONFIG.ANT", 0
bk_vi       db "C:\HISPADOS\VENTANAS.INI", 0
bk_va       db "C:\HISPADOS\VENTANAS.ANT", 0
bk_ci       db "C:\HISPADOS\CENTRO.INI", 0
bk_cn       db "C:\HISPADOS\CENTRO.ANT", 0
ed_ruta     db "C:\HISPADOS\EDICION.INI", 0
d4_ruta     db "A:\HISPADOS\EDICION.INI", 0
h32_ruta    db "C:\HISPA32", 0
ed_src      db 0                ; el origen es de Hispa-486
ed_dst      db 0                ; el destino ya es de Hispa-486
ed_pasar    db 0                ; ACTUALIZAR un Hispa-386 con uno de Hispa-486
auto_puesto db 0                ; se ha puesto el AUTOEXEC.BAT del origen
part_path   db "A:\HISPADOS\PARTIR.COM", 0
inst_tail   db 10, " /INSTALAR", 13
arr_tail    db 13, " /ARRANQUE:"
arr_drv     db "C:", 13
fmt_path    db "A:\HISPADOS\FORMATEA.COM", 0
fmt_tail    db fmt_te - fmt_tail - 1, " "
fmt_drv     db "C: /V:HISPADOS /AUTOTEST"
fmt_te      db 13
sys_path    db "A:\HISPADOS\SISTEMA.COM", 0
sys_tail    db 7, " "
sys_src     db "A:\ "                ; [CD] el origen (KERNEL.SYS y COMMAND.COM)
tail_drv    db "C:", 13
s_root      db "\", 0
s_hdir      db "\HISPADOS\", 0
s_hsp       db "\HISPADOS\*.*", 0
s_hjdir     db "\HISPADOS\JUEGOS\", 0
s_hspj      db "\HISPADOS\JUEGOS\*.*", 0
s_hndir     db "\HISPADOS\JUEGOS\NES\", 0
s_hspn      db "\HISPADOS\JUEGOS\NES\*.*", 0
s_hsdir     db "\HISPADOS\JUEGOS\SMS\", 0
s_hsps      db "\HISPADOS\JUEGOS\SMS\*.*", 0
s_hpdir     db "\HISPADOS\PROGS\", 0
s_hspp      db "\HISPADOS\PROGS\*.*", 0
s_h32dir    db "\HISPA32\", 0
s_h32       db "\HISPA32\*.*", 0
in_ruta     db "C:\HISPADOS\INTERNET", 0
s_indir     db "\HISPADOS\INTERNET\", 0
s_in        db "\HISPADOS\INTERNET\*.*", 0
di_ruta     db "A:\NAVEGA.EXE", 0
s_di_exe    db "\*.EXE", 0
s_di_dll    db "\*.DLL", 0
s_di_ini    db "\*.INI", 0
s_di_txt    db "\*.TXT", 0
s_autoexec  db "\AUTOEXEC.BAT", 0
s_config    db "\CONFIG.SYS", 0
s_leeme     db "\LEEME.TXT", 0
msg_title   db "Instalador de Hispa-DOS", 13, 10, 0
msg_usage   db "Uso: INSTALAR unidad: [/NOSONIDO]   o   INSTALAR [/NOSONIDO]", 13, 10
            db "Sin unidad busca el disco duro (nunca la memoria USB de arranque), ofrece", 13, 10
            db "crear la partición si no tiene y pregunta antes de seguir.", 13, 10
            db "Con unidad instala en esa (si no tiene formato, pregunta y se lo da).", 13, 10
            db "Al final comprueba el sonido (AUDICION /RAPIDA): lo que suena queda en", 13, 10
            db "unidad:\HISPADOS\SONIDO.INI y HSPSND lo usa desde el primer arranque.", 13, 10
            db "Esc salta ese paso; /NOSONIDO no lo hace.", 13, 10
            db "Si el destino ya tiene Hispa-DOS, ofrece ACTUALIZARLO en el sitio: cambia", 13, 10
            db "los programas del sistema por los nuevos y conserva tus datos y ajustes", 13, 10
            db "(*.INI, *.REC) y tus carpetas; no formatea nada. Lo normal: arranca desde", 13, 10
            db "el USB nuevo y actualiza el disco interno.", 13, 10
            db "La edición (Hispa-386 o Hispa-486) es la del origen; desde disquetes, al", 13, 10
            db "final se ofrece el de Hispa-486.", 13, 10, 0
msg_same    db "No se puede instalar sobre la unidad de arranque.", 13, 10, 0
msg_nofmt   db "La unidad "
msg_drv1    db "C: no existe.", 13, 10
            db "Cree la partición con PARTIR y vuelva a escribir INSTALAR.", 13, 10, 0
msg_askfmt1 db 13, 10, "La unidad "
msg_drv3    db "C: no tiene formato: se le dará formato (se borra todo lo que", 13, 10
            db "tenga) y se instalará Hispa-DOS en ella.", 13, 10, 0
msg_askfmt2 db "¿Continuar (S/N)? ", 0
msg_askins1 db 13, 10, "Se va a instalar Hispa-DOS en la unidad "
msg_drv4    db "C:. Lo que ya tenga se conserva", 13, 10
            db "(solo se ponen o se cambian los archivos de Hispa-DOS).", 13, 10, 0
msg_askins2 equ msg_askfmt2
msg_fmterr  db "No se ha podido dar formato a la unidad: no se ha instalado nada.", 13, 10, 0
msg_nothing db "No se ha instalado nada.", 13, 10, 0
of_ruta     db "A:\HISPADOS\PROGS\PLUMA.HVP", 0
i2_ruta     db "A:\HISPADOS\HW32.COM", 0
msg_of_preg db 13, 10, "La oficina (Pluma, Cuentas, Facturas, Datos, el Lector de PDF,", 13, 10
            db "Publicaciones y Presentaciones) va en el disquete ", 0xAE, "Oficina", 0xAF, ".", 13, 10
            db "Si lo tiene, p", 0xA2, "ngalo en la unidad de arranque y escriba S", 13, 10
            db "(N para terminar sin ella; se puede copiar despu", 0x82, "s): ", 0
msg_of_no   db "Ese no es el disquete ", 0xAE, "Oficina", 0xAF, ".", 13, 10, 0
msg_of_hecho db "Oficina: ", 0
msg_of_hecho2 db " archivos copiados.", 13, 10, 0
msg_i2_preg db 13, 10, "Los programas de Windows de consola (HW32, de Hispa-486) van en el", 13, 10
            db "disquete ", 0xAE, "Instalaci", 0xA2, "n 2", 0xAF, ".", 13, 10
            db "Si lo tiene, p", 0xA2, "ngalo en la unidad de arranque y escriba S", 13, 10
            db "(N para seguir sin ", 0x82, "l; se puede copiar despu", 0x82, "s): ", 0
msg_i2_no   db "Ese no es el disquete ", 0xAE, "Instalaci", 0xA2, "n 2", 0xAF, ".", 13, 10, 0
msg_i2_hecho db "Instalaci", 0xA2, "n 2: ", 0
msg_d4_preg db 13, 10, "Hispa-486 (el escritorio con el men", 0xA3, " Inicio y los programas Hispa32)", 13, 10
            db "va en el disquete ", 0xAE, "Hispa-486", 0xAF, ". Para instalarlo, p", 0xA2, "ngalo en la unidad", 13, 10
            db "de arranque y escriba S (N para quedarse con Hispa-386): ", 0
msg_d4_no   db "Ese no es el disquete ", 0xAE, "Hispa-486", 0xAF, ".", 13, 10, 0
msg_d4_hecho db "Hispa-486: ", 0
msg_di_preg db 13, 10, "El Navegador (para entrar en p", 0xA0, "ginas web, como la biblioteca de ClicGest)", 13, 10
            db "va en el disquete ", 0xAE, "Internet", 0xAF, ". Si lo tiene, p", 0xA2, "ngalo en la unidad de", 13, 10
            db "arranque y escriba S (N para terminar sin ", 0x82, "l; se puede copiar despu", 0x82, "s): ", 0
msg_di_no   db "Ese no es el disquete ", 0xAE, "Internet", 0xAF, ".", 13, 10, 0
msg_di_hecho db "Internet: ", 0
msg_ed_baja db 13, 10, "Esa unidad tiene Hispa-486 y este origen es de Hispa-386: para", 13, 10
            db "actualizarla, arranque con la memoria USB de Hispa-486 (o use sus", 13, 10
            db "disquetes). Con [N] se puede poner Hispa-386 de nuevo (se borra todo).", 13, 10, 0
msg_ed_pasa db 13, 10, "Esa unidad tiene Hispa-386 y este origen es de Hispa-486: al actualizar", 13, 10
            db "pasa a Hispa-486 (el escritorio con el men", 0xA3, " Inicio). Su VENTANAS.INI y su", 13, 10
            db "CENTRO.INI se guardan como VENTANAS.ANT y CENTRO.ANT.", 13, 10, 0
msg_bak_v   db "Tu VENTANAS.INI anterior se ha guardado como VENTANAS.ANT.", 13, 10, 0
msg_bak_n   db "Tu CENTRO.INI anterior se ha guardado como CENTRO.ANT.", 13, 10, 0
msg_of_fallo db "Alguno no se ha podido copiar (", 0xA8, "disco lleno?).", 13, 10, 0
msg_of_vuelve db "Vuelva a poner el disquete de arranque y pulse una tecla.", 0
msg_nopart  db "No se encuentra \HISPADOS\PARTIR.COM en la unidad de arranque.", 13, 10, 0
msg_arrerr  db "Los archivos están copiados, pero el disco duro NO ha quedado listo para", 13, 10
            db "arrancar (partición activa y MBR). Pruebe con PARTIR: opción 2.", 13, 10, 0
msg_reboot  db "Quite la memoria USB (o el disquete) y reinicie el ordenador.", 13, 10, 0
msg_syserr  db "No se ha podido transferir el sistema (SYS).", 13, 10, 0
msg_copying db "Copiando los archivos de Hispa-DOS...", 13, 10, 0
msg_fail    db "La instalación NO está completa: no se han podido copiar ", 0
msg_fail2   db " archivos", 13, 10, "(¿disco lleno?).", 13, 10, 0
msg_done    db "Hispa-DOS instalado en la unidad "
msg_drv2    db "C: (", 0
msg_done2   db " archivos copiados).", 13, 10, 0
msg_crlf    db 13, 10, 0
msg_boot    db "Ya puede arrancar desde ella.", 13, 10, 0
msg_snd     db 13, 10, "Por último, la comprobación del sonido", 13, 10
            db "--------------------------------------", 13, 10, 0
msg_sndko   db "No se ha podido comprobar el sonido. Puede hacerlo después con la", 13, 10
            db "opción «Prueba» del menú de arranque o con la orden AUDICION.", 13, 10, 0
; --- el menú cuando el destino ya tiene Hispa-DOS ---
msg_menu1   db 13, 10, "La unidad "
msg_menu_drv db "C: ya tiene Hispa-DOS instalado.", 13, 10, 0
msg_menu2   db "¿Qué quieres hacer?", 13, 10
            db "  [A] Actualizar (conserva tus datos y ajustes)", 13, 10
            db "  [N] Instalación nueva (se FORMATEA: se borra TODO lo que haya)", 13, 10
            db "  [C] Cancelar", 13, 10
            db "Elige una opción y pulsa Intro (A/N/C): ", 0
; --- [N] desde el menú: formatear aunque ya tuviera formato ---
msg_asknew1 db 13, 10, "Instalación nueva en la unidad "
msg_asknew_drv db "C:. Se dará formato y se borrará TODO lo que tenga", 13, 10
            db "(tus datos y ajustes incluidos). ", 0
; --- [A] actualizar: resumen y confirmación ---
msg_upd1    db 13, 10, "Se va a ACTUALIZAR Hispa-DOS en la unidad "
msg_upd_drv db "C: (no se formatea nada):", 13, 10
            db "- Se cambian los programas del sistema por los nuevos.", 13, 10
            db "- Se conservan tus datos y ajustes (*.INI, *.REC) y tus carpetas.", 13, 10
            db "- CONFIG.SYS y AUTOEXEC.BAT: se ponen los nuevos y los tuyos se", 13, 10
            db "  guardan como CONFIG.ANT y AUTOEXEC.ANT.", 13, 10, 0
msg_upd2    db "¿Continuar (S/N)? ", 0
msg_bak_a   db "Tu AUTOEXEC.BAT anterior se ha guardado como AUTOEXEC.ANT.", 13, 10, 0
msg_bak_c   db "Tu CONFIG.SYS anterior se ha guardado como CONFIG.ANT.", 13, 10, 0
msg_updone  db "Hispa-DOS actualizado en la unidad "
msg_updone_d db "C: (", 0
msg_updone2 db " archivos nuevos del sistema).", 13, 10, 0
msg_updkept db "Conservados tus datos y ajustes: ", 0
msg_updkept2 db " archivos (*.INI, *.REC); tus carpetas quedan intactas.", 13, 10, 0
msg_updboot db "Actualización terminada. Quita el USB (o el disquete) y reinicia el", 13, 10
            db "ordenador.", 13, 10, 0

; ---- [CD] y [SETUP]: datos ----
setup       db 0                ; 1: INSTALAR /SETUP
origen      db 0                ; /ORIGEN:x:
arranque    db 0                ; la unidad de arranque
desde_cd    db 0                ; el origen es el CD de Hispa-DOS
cd_ruta     db "C:\HISPADOS\HISPACD.INI", 0
i24_viejo   dd 0
so_guarda   dw 0
s_nul       db "NUL", 0
s_formatear db 0
s_nivel     db 6
situ_tail db situ_tail_f - situ_tail - 1, " /SITUACION"
situ_tail_f db 13
borr_tail db borr_tail_f - borr_tail - 1, " /BORRARTODO"
borr_tail_f db 13
inst_si_tail db inst_si_tail_f - inst_si_tail - 1, " /INSTALAR /SI"
inst_si_tail_f db 13
p_cuerpo    dw 0
p_pie       dw 0
p_ayuda     dw 0
t_fila      db 0
t_col       db 0
prog_rut    dw 0
nhechos     dw 0
ncop_total  dw 0
barra_n     db 0
n3          db "000", 0
pd_preg     dw 0
pd_pant     dw 0
pd_ruta     dw 0
pd_malo     dw 0
pd_err      db 0
s_cab       db "Programa de instalación de Hispa-DOS ", VER_STR, " - "
cab_ed      db "Hispa-386", 0
s_ed486     db "Hispa-486", 0
s_raya      times 76 db 0xCD
s_nl        db 13, 10, 0
s_barra0    times 60 db 0xB0
            db "   0 %", 0
s_pc        db " %", 0
s_copiando  db "Copiando: ", 0

; --- los pies (las teclas de cada pantalla) ---
pie_bienv   db "INTRO=Continuar   F1=Ayuda   F3=Salir", 0
pie_intro   equ pie_bienv
pie_salir   db "INTRO=Salir", 0
pie_salir2  db "F3=Salir del programa de instalación   INTRO=Seguir instalando", 0
pie_sitio   db "INTRO=Usar el sitio libre   B=Borrar todo el disco   F1=Ayuda   F3=Salir", 0
pie_lleno   db "B=Borrar todo el disco   F1=Ayuda   F3=Salir", 0
pie_borrar  db "S=Sí, borrar todo el disco   ESC=Volver atrás   F3=Salir", 0
pie_espera  db "Espere, por favor...", 0
pie_reinicio db "INTRO=Reiniciar el ordenador", 0
pie_yahay   db "A=Actualizar   N=Instalación nueva   F1=Ayuda   F3=Salir", 0
pie_disco   db "INTRO=Continuar   ESC=Seguir sin este disco   F1=Ayuda   F3=Salir", 0
pie_fin     db "INTRO=Reiniciar el ordenador", 0
pie_ayuda   db "INTRO o ESC=Volver", 0

; --- las pantallas ---
sc_bienv    db "Bienvenido al programa de instalación de Hispa-DOS.", 10, 10
            db "Este programa instala Hispa-DOS en el disco duro de este ordenador:", 10
            db "el sistema, Hispa Ventanas, los programas, la oficina y los juegos.", 10
            db "Después, el ordenador arrancará solo desde el disco duro.", 10, 10
            db "  - Para instalar Hispa-DOS ahora, pulse INTRO.", 10, 10
            db "  - Para saber más antes de seguir, pulse F1.", 10, 10
            db "  - Para salir sin instalar nada, pulse F3.", 0
sc_sindisco db "No se ha encontrado ningún disco duro donde instalar Hispa-DOS.", 10, 10
            db "Compruebe que el disco duro está bien conectado y que la BIOS del", 10
            db "ordenador lo ve. Hispa-DOS se puede seguir usando sin instalar,", 10
            db "desde este disquete, memoria USB o CD.", 10, 10
            db "Pulse INTRO para salir del programa de instalación.", 0
sc_nopart   db "No se encuentra el programa PARTIR (\HISPADOS\PARTIR.COM) en la", 10
            db "unidad de origen. No se ha instalado nada.", 0
sc_vacio    db "El disco duro de este ordenador está vacío: todavía no está preparado", 10
            db "para guardar archivos (no tiene ninguna partición).", 10, 10
            db "El programa lo va a preparar entero para Hispa-DOS (hasta 2 GB) y", 10
            db "después le dará formato.", 10, 10
            db "  - Para preparar el disco duro, pulse INTRO.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_sitio    db "El disco duro ya tiene partes (particiones) de otro sistema, pero le", 10
            db "queda sitio libre.", 10, 10
            db "  - Para instalar Hispa-DOS en el sitio libre, pulse INTRO. Lo que", 10
            db "    ya tiene el disco no se toca.", 10, 10
            db "  - Para borrar todo el disco y usarlo entero para Hispa-DOS, pulse B.", 10
            db 1, "    Atención: con B se pierde todo lo que tenga el disco duro.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_lleno    db "El disco duro no tiene sitio para Hispa-DOS: está ocupado entero por", 10
            db "partes (particiones) de otro sistema.", 10, 10
            db "Para instalar Hispa-DOS hay que borrar todo el disco duro.", 10
            db 1, "Atención: se perderá todo lo que tenga.", 10, 10
            db "  - Para borrar todo el disco y seguir, pulse B.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_borrar   db 1, "Se va a BORRAR TODO EL DISCO DURO.", 10, 10
            db "Todo lo que tenga (otros sistemas, programas, documentos...) se", 10
            db "perderá y no se podrá recuperar.", 10, 10
            db "  - Si está seguro, pulse S.", 10, 10
            db "  - Para volver atrás sin tocar nada, pulse ESC.", 0
sc_preparando db "Preparando el disco duro para Hispa-DOS...", 0
sc_parterr  db "No se ha podido preparar el disco duro. No se ha instalado nada.", 10, 10
            db "Puede hacerlo a mano con la orden PARTIR y volver a empezar.", 0
sc_reinicio db "El disco duro ya está preparado, pero hay que reiniciar el ordenador", 10
            db "para seguir.", 10, 10
            db "Deje puesto el disquete, la memoria USB o el CD. Al volver a salir", 10
            db "el menú de arranque, elija otra vez", 10
            db "«Instalar Hispa-DOS en el disco duro».", 10, 10
            db "Pulse INTRO para reiniciar el ordenador.", 0
sc_sinfmt   db "La unidad "
sc_d1       db "C: del disco duro no tiene formato: todavía no puede guardar", 10
            db "archivos. El programa le dará formato.", 10, 10
            db 1, "Atención: al dar formato se borra lo que tenga la unidad.", 10, 10
            db "  - Para dar formato y seguir, pulse INTRO.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_yahay    db "La unidad "
sc_d2       db "C: ya tiene Hispa-DOS instalado.", 10, 10
            db "  - Para actualizarlo, pulse A. Se cambian los programas del sistema", 10
            db "    por los nuevos y se conservan sus datos y ajustes; no se borra", 10
            db "    nada.", 10, 10
            db "  - Para una instalación nueva, pulse N. Se da formato a la unidad:", 10
            db 1, "    se borra TODO lo que tenga, también sus datos y ajustes.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_edbaja   db "Esa unidad tiene Hispa-486 y este medio es de Hispa-386: para", 10
            db "actualizarla, use el de Hispa-486. Con una instalación nueva (N) se", 10
            db "puede poner Hispa-386 (se borra todo). No se ha instalado nada.", 0
sc_conserva db "Hispa-DOS se va a instalar en la unidad "
sc_d3       db "C: del disco duro.", 10, 10
            db "Lo que ya tiene la unidad se conserva: solo se ponen los archivos de", 10
            db "Hispa-DOS.", 10, 10
            db "  - Para seguir, pulse INTRO.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_carpeta  db "Hispa-DOS se instalará en esta carpeta del disco duro:", 10, 10
            db "        "
sc_d4       db "C:\HISPADOS", 10, 10
            db "Los programas de 32 bits de Hispa-486 van en \HISPA32 y la", 10
            db "configuración (CONFIG.SYS y AUTOEXEC.BAT), en la raíz de la unidad.", 10, 10
            db "  - Para empezar a copiar los archivos, pulse INTRO.", 10, 10
            db "  - Para salir sin tocar nada, pulse F3.", 0
sc_orig     db "Se copia desde la unidad "
sc_orig_d   db "A:", 0
sc_orig_cd  db " (el CD de Hispa-DOS)", 0
sc_formato  db "Dando formato a la unidad "
sc_d5       db "C:...", 0
sc_fmterr   db "No se ha podido dar formato a la unidad. No se ha instalado nada.", 0
sc_copia    db "Copiando los archivos de Hispa-DOS en el disco duro.", 10, 10
            db "Espere, por favor: tarda unos minutos.", 0
sc_fallo    db "Algunos archivos no se han podido copiar (¿el disco duro está", 10
            db "lleno?). La instalación no está completa.", 0
sc_salir    db "Hispa-DOS no está instalado del todo.", 10, 10
            db "Si sale ahora, el disco duro no podrá arrancar Hispa-DOS. Puede", 10
            db "volver a empezar más tarde eligiendo «Instalar Hispa-DOS en el disco", 10
            db "duro» en el menú de arranque, o con la orden INSTALAR /SETUP.", 10, 10
            db "  - Para seguir instalando, pulse INTRO.", 10, 10
            db "  - Para salir del programa de instalación, pulse F3.", 0
sc_noesdisco db 1, "Ese no es el disco pedido. Ponga el que se pide y pulse INTRO.", 0
sd_i2       db "Inserte el disco rotulado", 10, 10
            db "        Hispa-DOS - Instalación 2", 10, 10
            db "en la unidad "
sd_letra1   db "A: y pulse INTRO.", 10, 10
            db "Trae los programas de Windows de consola (HW32) y los tipos de letra", 10
            db "grandes de la oficina. Sin él, se pueden copiar después.", 0
sd_of       db "Inserte el disco rotulado", 10, 10
            db "        Hispa-DOS - Oficina", 10, 10
            db "en la unidad "
sd_letra2   db "A: y pulse INTRO.", 10, 10
            db "Trae la oficina: Pluma, Cuentas, Facturas, Datos, el Lector de PDF,", 10
            db "Publicaciones y Presentaciones.", 0
sd_d4       db "Para instalar Hispa-486 (el escritorio con el menú Inicio y los", 10
            db "programas Hispa32), inserte el disco rotulado", 10, 10
            db "        Hispa-DOS - Hispa-486", 10, 10
            db "en la unidad de disquete y pulse INTRO.", 10, 10
            db "Para quedarse con Hispa-386, pulse ESC.", 0
sd_di       db "Para instalar el Navegador (páginas web), inserte el disco rotulado", 10, 10
            db "        Hispa-DOS - Internet", 10, 10
            db "en la unidad de disquete y pulse INTRO.", 10, 10
            db "Sin él, pulse ESC (se puede copiar después).", 0
sc_fin      db "Hispa-DOS está instalado en el disco duro.", 10, 10
            db "Saque el disquete, el CD o la memoria USB y pulse INTRO para", 10
            db "reiniciar el ordenador: arrancará Hispa-DOS desde el disco duro.", 10, 10
            db "Para comprobar el sonido, elija en el menú de arranque la opción", 10
            db "«Prueba del equipo y del sonido».", 0
sc_finupd   db "Hispa-DOS está actualizado en el disco duro. Sus datos y ajustes se", 10
            db "han conservado (los CONFIG.SYS y AUTOEXEC.BAT anteriores quedan como", 10
            db "CONFIG.ANT y AUTOEXEC.ANT).", 10, 10
            db "Saque el disquete, el CD o la memoria USB y pulse INTRO para", 10
            db "reiniciar el ordenador.", 0
msg_s_salido db "Se ha salido del programa de instalación. Para volver a empezar:", 13, 10
            db "INSTALAR /SETUP", 13, 10, 0
msg_of_vuelve2 db "Si ha cambiado el disquete, vuelva a poner el de arranque.", 13, 10, 0

; --- la ayuda (F1) de cada paso ---
ay_bienv    db "Ayuda del programa de instalación", 10, 10
            db "El programa va paso a paso: mira el disco duro, lo prepara si hace", 10
            db "falta (avisando antes si se va a borrar algo), le da formato, copia", 10
            db "los archivos y deja el disco listo para arrancar.", 10, 10
            db "En cada pantalla, abajo, están las teclas que se pueden usar:", 10
            db "  INTRO  sigue con el paso siguiente", 10
            db "  F1     enseña esta ayuda", 10
            db "  F3     sale del programa (pregunta antes)", 10, 10
            db "Hispa-DOS también se puede usar sin instalar: en el menú de arranque,", 10
            db "«Arrancar Hispa-DOS sin instalar».", 0
ay_disco    db "Ayuda: el disco duro", 10, 10
            db "Para guardar archivos, un disco duro se divide en partes llamadas", 10
            db "particiones. Hispa-DOS necesita una partición de DOS (FAT).", 10, 10
            db "Si el disco está vacío, el programa hace una con todo el disco. Si", 10
            db "tiene particiones de otros sistemas y queda sitio, puede usar ese", 10
            db "sitio sin tocar lo demás. Si no queda sitio, hay que borrar todo el", 10
            db "disco: el programa lo pregunta dos veces antes de hacerlo.", 10, 10
            db "Para hacerlo a mano, salga con F3 y use la orden PARTIR.", 0
ay_formato  db "Ayuda: el formato", 10, 10
            db "Dar formato prepara la unidad para guardar archivos de Hispa-DOS.", 10
            db "Se borra todo lo que tuviera la unidad.", 0
ay_actualizar db "Ayuda: actualizar o instalar de nuevo", 10, 10
            db "Actualizar (A) cambia los programas del sistema por los de este", 10
            db "medio y conserva sus datos y ajustes (*.INI, *.REC) y sus carpetas.", 10
            db "Es lo normal para pasar a una versión nueva.", 10, 10
            db "La instalación nueva (N) da formato a la unidad y lo pone todo como", 10
            db "recién instalado: se borra todo lo que tenga.", 0
ay_carpeta  db "Ayuda: la carpeta", 10, 10
            db "Todo Hispa-DOS va en la carpeta \HISPADOS (como \DOS en MS-DOS), y", 10
            db "esa carpeta va en el camino de búsqueda (PATH): las órdenes se", 10
            db "encuentran desde cualquier carpeta.", 0
ay_copia    db "Ayuda: la copia", 10, 10
            db "Se copian los archivos de Hispa-DOS al disco duro.", 0
ay_disquete db "Ayuda: los disquetes", 10, 10
            db "Desde disquetes, Hispa-DOS viene en varios discos. El programa los", 10
            db "pide uno a uno: ponga el que dice en la unidad y pulse INTRO. Si no", 10
            db "lo tiene, ESC sigue sin él (sus archivos se pueden copiar después con", 10
            db "XCOPIAR A:\HISPADOS C:\HISPADOS /S).", 0
ay_fin      db "Ayuda: el final", 10, 10
            db "Hispa-DOS ya está en el disco duro. Si el ordenador vuelve a arrancar", 10
            db "desde el disquete, la memoria USB o el CD, sáquelo, o cambie en la", 10
            db "BIOS el orden de arranque para que el disco duro vaya primero.", 0
ay_error    db "Ayuda", 10, 10
            db "Algo no ha ido bien. Lo que pone la pantalla dice qué ha pasado y", 10
            db "cómo seguir.", 0

        section .bss
pblock      resb 14
pat         resb 80
sdir        resb 80
path_s      resb 128
path_d      resb 128
dta         resb 128
dta2        resb 128
params      resb 40
lbuf        resb 12
buf         resb BUFSZ
            resb 512
stack_top:
prog_end:
