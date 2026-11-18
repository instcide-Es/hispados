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
;  Último paso, la comprobación del sonido: AUDICION /RAPIDA (la versión
;  corta de la prueba de sonido) guarda lo que suena en el SONIDO.INI del
;  disco instalado (unidad:\HISPADOS\SONIDO.INI, con su informe SONIDO.TXT
;  al lado); así HSPSND lo usa desde el primer arranque. /NOSONIDO no la hace.
;  Nivel de error: 0 instalado, 1-5 error, 6 cancelado por el usuario.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

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
        ; unidad de origen: la de arranque
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [src], dl
        mov [sys_path], dl
        mov [part_path], dl
        mov [fmt_path], dl
        cmp byte [guided], 0
        je .chk
        call guided_pick
.chk:
        mov dl, [src]
        cmp dl, [dst]
        jne .ok
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
        ; 1) sistema: SYS unidad:
        mov dx, sys_path
        mov bx, sys_tail
        call run
        jc .syserr
        test al, al
        jz .copy
.syserr:
        mov si, msg_syserr
        call puts
        mov ax, 0x4C03
        int 0x21
.copy:
        ; 2) archivos
        mov si, msg_copying
        call puts
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
        cmp word [nfailed], 0
        je .allok
        ; alguno no se ha copiado (disco lleno...): no decir que está instalado
        mov si, msg_fail
        call puts
        movzx eax, word [nfailed]
        call putnum
        mov si, msg_fail2
        call puts
        mov ax, 0x4C04
        int 0x21
.allok:
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
; A: o B: y el origen no lo trae, se ofrece ese disquete y se copia.
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
        call puts
        call ask_yes
        jc .r
        mov dx, i2_ruta             ; ¿es el de Instalación 2?
        mov ax, 0x3D00
        int 0x21
        jnc .es
        mov si, msg_i2_no
        call puts
        jmp .pregunta
.es:
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [keepdata], 0
        mov byte [fix], 0
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
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
        call puts
        call ask_yes
        jc .r
        mov dx, of_ruta             ; ¿es el de Oficina?
        mov ax, 0x3D00
        int 0x21
        jnc .es
        mov si, msg_of_no
        call puts
        jmp .pregunta
.es:
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov byte [keep], 0
        mov byte [keepdata], 0
        mov byte [fix], 0
        mov si, s_hspp
        mov di, s_hpdir
        call make_dir
        call copy_set
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
        call puts
        call ask_yes
        jc .r
        mov dx, d4_ruta             ; ¿es el de Hispa-486?
        call existe
        jnc .es
        mov si, msg_d4_no
        call puts
        jmp .pregunta
.es:
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
sys_tail    db 3, " "
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
