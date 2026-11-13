; =============================================================================
;  AUDICION.COM - prueba de sonido de Hispa-DOS con preguntas de sí o no
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  AUDICION [carpeta]
;        AUDICION /RAPIDA [carpeta] [/INI:ruta]   (la versión corta, para INSTALAR)
;        AUDICION /?
;
;  Maneja el hardware directamente (no usa HSPSND). Primero recoge, sin
;  preguntar, todo lo que se puede saber del sonido del equipo (SMBIOS,
;  procesador, controladoras PCI de sonido, volcado completo de los códecs
;  HD Audio, registros del AC'97, Sound Blaster ISA). Después toca sonidos
;  por cada camino posible y pregunta «¿Lo oye? (S/N/R)». Todo queda en un
;  informe de texto: \SONIDO01.TXT, SONIDO02.TXT... en la raíz de la unidad
;  de arranque (nunca se sobrescribe), o SONIDO.TXT en la carpeta que se dé.
;  Ver docs/AUDICION.md.
;
;  Escrito desde cero (sala limpia) con la documentación pública: High
;  Definition Audio Specification 1.0a (Intel), Audio Codec '97 2.3, Intel
;  ICH (AC'97 bus master), PCI Local Bus 2.2, SMBIOS (DMTF DSP0134), Sound
;  Blaster Series Hardware Programming Guide (Creative) y CEA-861 (trama de
;  información de audio del HDMI).
; =============================================================================
        cpu 586
        bits 16
        org 0x100

O_SCR       equ 1                   ; a la pantalla
O_REP       equ 2                   ; al informe
O_SUM       equ 4                   ; al resumen (se escribe al final)
O_CAP       equ 8                   ; a smb_eq (fabricante y modelo SMBIOS)
O_INI       equ 16                  ; a SONIDO.INI (en el segmento de DMA)
SMBEQ       equ 200                 ; «fabricante | modelo | versión» (SMBIOS)
REC         equ 20                  ; una tarjeta para SONIDO.INI (lrn_ids)
NNF         equ 4                   ; códecs que no han sonado, como mucho
HDRSZ       equ 48
INI_OUT     equ 0x8000              ; SONIDO.INI nuevo, en el segmento de DMA
RBUFSZ      equ 3072
SUMSZ       equ 3072
CONNMAX     equ 32
NCTL        equ 8
NCAND       equ 24
; segmento de DMA (el siguiente a este programa): desplazamientos
D_CORB      equ 0x0000
D_RIRB      equ 0x0400
D_BDL       equ 0x0C00
D_ABUF      equ 0x1000

%macro PR 1+
        call prs
        db %1, 0
%endmacro
%macro MODE 1
        mov byte [omode], %1
%endmacro
%define NL 13, 10

start:
        cld
        mov di, bss_start
        mov cx, bss_end - bss_start
        xor al, al
        rep stosb
        mov [sp0], sp
        call parse_args
        MODE O_SCR
        cmp byte [quick], 0
        jne .ttl
        PR "AUDICION 1.0 - prueba de sonido de Hispa-DOS", NL, NL
.ttl:
        ; memoria para la DMA: los 64 KB que siguen al programa
        mov ax, cs
        add ax, 0x1007
        and ax, 0xFFF8              ; (CORB, RIRB y la lista de descriptores
        mov [dseg], ax              ; van alineados a 128 octetos)
        add ax, 0x1000
        cmp ax, [0x02]
        jbe .mem
        PR "No hay memoria suficiente (hacen falta 128 KB libres).", NL
        mov ax, 0x4C01
        int 0x21
.mem:
        smsw ax
        and al, 1
        mov [v86], al
        call chk_hspsnd
        call rep_create
        ; GDT del modo irreal
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        call a20_open
        mov byte [a20_on], 1
        MODE O_SCR
        PR "Recogiendo los datos del equipo (no toque nada)...", NL
        mov al, [quick]             ; versión corta: el volcado, solo al informe
        mov [hush], al
        call rep_header
        call collect_sys
        call pci_scan
        call collect_cards
        call rflush
        mov byte [hush], 0
        ; ---- las pruebas ----
        cmp byte [quick], 0
        jne quick_run
        MODE O_SCR
        PR NL, "COMO FUNCIONA", NL
        PR "  Se oirán sonidos. Después de cada uno, pulse:", NL
        PR "    S  si lo ha oído", NL
        PR "    N  si no lo ha oído", NL
        PR "    R  para repetirlo", NL
        PR "    Esc  para terminar y guardar lo que haya", NL
        PR "  Suba el volumen del equipo si tiene rueda o teclas de volumen.", NL
        PR "  Pulse una tecla para empezar.", NL
        call kflush
        mov ah, 7
        int 0x21
        cmp al, 27
        je abort
        MODE O_REP
        PR NL, "=== PRUEBAS (respuestas de la persona) ===", NL
        call test_pcspk
        call hda_tests
        call ac97_test
        call sb_test
        jmp finish

; ---- AUDICION /RAPIDA: la versión corta (la de INSTALAR) ----
; Una explicación de tres líneas, el pitido de control, el barrido hasta el
; primer camino que suene y, con él, solo el volumen y los dos canales. Sin
; el volcado en pantalla (va entero al informe). Esc salta la prueba.
quick_run:
        MODE O_SCR
        ; ¿hay algo que probar? (HD Audio fuera del modo V86, AC'97 o SB)
        cmp word [nabm], 0
        jne .go
        cmp word [sb_base], 0
        jne .go
        cmp byte [n_hda], 0
        je .none
        cmp byte [v86], 0
        je .go
.none:
        PR "No se ha encontrado ninguna tarjeta de sonido que se pueda probar.", NL
        MODE O_REP
        PR NL, "(Versión corta: no hay ninguna tarjeta de sonido que se pueda probar.)", NL
        jmp finish
.go:
        PR NL, "Vamos a comprobar que su equipo suena. Pulse S si oye el sonido,", NL
        PR "N si no lo oye (R lo repite). Suba el volumen si tiene rueda o teclas.", NL
        PR "Si no tiene altavoces ni auriculares, pulse Esc para saltar este paso.", NL
        PR "Pulse una tecla para empezar.", NL
        call kflush
        mov ah, 7
        int 0x21
        test al, al
        jnz .k
        mov ah, 7                   ; tecla extendida: se descarta el código
        int 0x21
.k:
        cmp al, 27
        je abort
        MODE O_REP
        PR NL, "=== PRUEBAS (versión corta, AUDICION /RAPIDA) ===", NL
        call test_pcspk
        call hda_tests
        cmp byte [found_an], 0
        jne finish
        call ac97_test
        cmp byte [ac_res], 1
        je finish
        cmp byte [ac_res], 2
        je finish
        call sb_test
        jmp finish

abort:
        mov sp, [cs:sp0]
        push cs
        pop ds
        mov byte [aborted], 1
        mov al, [quick]
        mov [hush], al
        MODE O_SCR | O_REP
        PR NL, "(Esc: prueba terminada antes de acabar)", NL
finish:
        mov sp, [sp0]
        cld
        call all_quiet
        mov al, [quick]             ; versión corta: el resumen y lo de
        mov [hush], al              ; SONIDO.INI, solo al informe
        call summary
        ; versión corta con Esc antes de que sonara nada: no se apunta nada
        cmp byte [quick], 0
        je .sv
        cmp byte [aborted], 0
        je .sv
        cmp byte [w_ok], 0
        jne .sv
        MODE O_REP
        PR NL, "(Versión corta saltada con Esc: no se toca SONIDO.INI.)", NL
        jmp .nsv
.sv:
        call ini_save
.nsv:
        call rflush
        mov byte [hush], 0
        cmp byte [a20_on], 0
        je .na
        call a20_restore
.na:
        MODE O_SCR
        cmp byte [quick], 0
        jne quick_end
        cmp byte [rep_ok], 0
        je .nofile
        PR NL, "Informe guardado en "
        mov si, fname
        call pstr
        PR ". Súbalo al FTP en HispaDOS/informes.", NL
        jmp .x
.nofile:
        PR NL, "ATENCION: el informe NO se ha podido guardar en el disco.", NL
.x:
        cmp byte [hsp_was], 0
        je .x2
        PR "HSPSND estaba cargado: reinicie el equipo para volver a usarlo.", NL
        mov ax, 0x4C00
        int 0x21
.x2:
        ; código de salida 2: ha sonado y HSPSND puede usarlo ya (lo aprendido
        ; está en SONIDO.INI, o es una Sound Blaster, que HSPSND encuentra sola)
        mov ax, 0x4C00
        cmp byte [ini_good], 0
        jne .x3
        cmp byte [sb_res], 0
        je .x4
.x3:
        mov al, 2
.x4:
        int 0x21

; final de la versión corta: una frase y el código de salida (2: ha sonado y
; queda guardado para HSPSND; 0: no, o se ha saltado)
quick_end:
        PR NL
        cmp byte [ini_good], 0
        jne .ok
        cmp byte [sb_res], 0
        jne .ok
        cmp byte [aborted], 0
        je .no
        PR "Se ha saltado la comprobación del sonido. Puede hacerla después con la", NL
        jmp .later
.no:
        PR "No se ha podido comprobar el sonido. Puede hacerlo después con la", NL
.later:
        PR "opción «Prueba» del menú de arranque o con la orden AUDICION.", NL
        mov byte [qexit], 0
        jmp .h
.ok:
        PR "El sonido de su equipo ha quedado configurado.", NL
        mov byte [qexit], 2
.h:
        cmp byte [hsp_was], 0
        je .x
        PR "(HSPSND se ha detenido para la prueba: volverá a sonar al reiniciar.)", NL
.x:
        cmp byte [rep_ok], 0
        jne .x2
        PR "ATENCION: el informe de la prueba NO se ha podido guardar.", NL
.x2:
        mov al, [qexit]
        mov ah, 0x4C
        int 0x21

; callar todo lo que pueda estar sonando
all_quiet:
        in al, 0x61
        and al, 0xFC
        out 0x61, al
        cmp byte [hda_live], 0
        je .a
        call hda_stream_stop
.a:
        cmp word [nabm], 0
        je .s
        mov dx, [nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
.s:
        cmp word [sb_base], 0
        je .r
        call sb_reset
.r:
        ret

; ---- argumentos: /? o la carpeta del informe ----
parse_args:
        mov si, 0x81
.sp:
        lodsb
        cmp al, ' '
        je .sp
        cmp al, 9
        je .sp
        cmp al, 13
        je .r
        cmp al, '/'
        jne .dir
        ; /SIM (para las pruebas en un emulador): hace como si el códec
        ; tuviera GPIO, EAPD, «snoop» y una salida de auriculares, para
        ; recorrer todas las variantes
        mov eax, [si]
        and eax, 0x00DFDFDF
        cmp eax, 'SIM'
        jne .q
        add si, 3
        mov byte [sim], 1
        jmp .sp
.q:
        ; /RAPIDA: la versión corta (la que lanza INSTALAR)
        mov eax, [si]
        and eax, 0xDFDFDFDF
        cmp eax, 'RAPI'
        jne .q1
        mov ax, [si+4]
        and ax, 0xDFDF
        cmp ax, 'DA'
        jne .q1
        add si, 6
        mov byte [quick], 1
        jmp .sp
.q1:
        ; /INI:ruta: dónde guardar lo aprendido (por omisión, \HISPADOS\SONIDO.INI
        ; de la unidad de arranque)
        mov eax, [si]
        and eax, 0xFFDFDFDF
        cmp eax, 'INI:'
        jne .q2
        add si, 4
        mov di, ini_name
.qi:
        mov al, [si]
        cmp al, ' '
        jbe .qe
        inc si
        cmp di, ini_name + INIMAX - 1
        jae .qi
        mov [di], al
        inc di
        jmp .qi
.qe:
        mov byte [di], 0
        cmp di, ini_name
        je .q2
        mov byte [has_ini], 1
        jmp .sp
.q2:
        cmp byte [si], '?'
        jne .dir
        MODE O_SCR
        mov si, msg_usage
        call pstr
        mov ax, 0x4C00
        int 0x21
.dir:
        mov di, outdir
.c:
        cmp al, ' '
        jbe .e
        cmp di, outdir + 60
        jae .c2
        mov [di], al
        inc di
.c2:
        lodsb
        jmp .c
.e:
        cmp di, outdir
        je .e2
        cmp byte [di-1], '\'
        jne .z
        dec di
.z:
        mov byte [di], 0
        mov byte [has_dir], 1
.e2:
        cmp al, 13                  ; detrás de la carpeta puede haber más
        jne .sp                     ; opciones (/RAPIDA, /INI:)
.r:
        ret

; ---- ¿está HSPSND? se le pide parar (función 03h) ----
chk_hspsnd:
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne .r
        mov [hsp_ent], bx
        mov [hsp_ent+2], es
        mov ah, 0
        call far [hsp_ent]
        mov [hsp_type], al
        mov ah, 3
        call far [hsp_ent]
        push cs
        pop es
        push cs
        pop ds
        cld
        mov byte [hsp_was], 1
        MODE O_SCR
        cmp byte [quick], 0
        je .long
        PR "(HSPSND está cargado: se detiene durante la prueba; volverá al reiniciar.)", NL
        jmp .r
.long:
        PR "HSPSND está cargado. Esta prueba maneja la tarjeta directamente y", NL
        PR "HSPSND dejará de sonar hasta reiniciar. Es mejor arrancar con la", NL
        PR "opción «Prueba de sonido» del menú de arranque.", NL
        mov word [replay], 0
.k:
        PR "¿Continuar de todos modos? (S/N) "
        mov ah, 7
        int 0x21
        and al, 0xDF
        cmp al, 'S'
        je .y
        cmp al, 'N'
        je .n
        cmp al, 27 & 0xDF
        je .n
        PR NL
        jmp .k
.y:
        PR "S", NL, NL
.r:
        push cs
        pop es
        ret
.n:
        PR "N", NL
        mov ax, 0x4C01
        int 0x21

; =============================================================================
;  Salida: pantalla, informe y resumen
; =============================================================================
prs:
        push bp
        mov bp, sp
        pushf
        push ax
        push si
        mov si, [bp+2]
.l:
        lodsb
        test al, al
        jz .d
        call pchar
        jmp .l
.d:
        mov [bp+2], si
        pop si
        pop ax
        popf
        pop bp
        ret

; DS:SI = texto ASCIIZ
pstr:
        push ax
        push si
.l:
        lodsb
        test al, al
        jz .d
        call pchar
        jmp .l
.d:
        pop si
        pop ax
        ret

pchar:
        push ax
        push bx
        push dx
        test byte [omode], O_REP
        jz .s
        mov bx, [rlen]
        mov [rbuf+bx], al
        inc bx
        mov [rlen], bx
        cmp bx, RBUFSZ
        jb .s
        call rflush
.s:
        test byte [omode], O_CAP
        jz .s1
        mov bx, [caplen]
        cmp bx, [capmax]
        jae .s1
        add bx, [capp]
        mov [bx], al
        mov byte [bx+1], 0
        inc word [caplen]
.s1:
        test byte [omode], O_INI
        jz .s2
        push es
        mov es, [dseg]
        mov bx, [ilen]
        cmp bx, 0xFFF0
        jae .s3
        mov [es:bx], al
        inc word [ilen]
.s3:
        pop es
.s2:
        test byte [omode], O_SUM
        jz .t
        mov bx, [slen]
        cmp bx, SUMSZ
        jae .t
        mov [sumbuf+bx], al
        inc word [slen]
.t:
        test byte [omode], O_SCR
        jz .r
        cmp byte [hush], 0          ; versión corta: sin el volcado
        jne .r
        mov dl, al
        mov ah, 2
        int 0x21
.r:
        pop dx
        pop bx
        pop ax
        ret

nl:
        PR NL
        ret

phex8:
        push eax
        shr eax, 16
        call phex4
        pop eax
phex4:
        push ax
        mov al, ah
        call phex2
        pop ax
phex2:
        push ax
        shr al, 4
        call phex1
        pop ax
phex1:
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call pchar
        pop ax
        ret

; EAX en decimal
pdec:
        pushad
        xor cx, cx
        mov ebx, 10
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        call pchar
        loop .p
        popad
        ret
; AL en decimal con dos cifras
pdec2:
        push ax
        aam
        xchg al, ah
        add al, '0'
        call pchar
        mov al, ah
        add al, '0'
        call pchar
        pop ax
        ret

; AL = índice, SI = tabla de textos ASCIIZ seguidos -> imprime el AL-ésimo
ptab:
        push ax
        push cx
        push si
        movzx cx, al
        jcxz .p
.s:
        lodsb
        test al, al
        jnz .s
        loop .s
.p:
        call pstr
        pop si
        pop cx
        pop ax
        ret

; ---- el fichero del informe ----
; Nombre: [carpeta]\SONIDO.TXT (y SONIDO02.TXT... si ya existe) o, sin
; carpeta, X:\SONIDO01.TXT... en la unidad de arranque. Nunca se sobrescribe.
rep_create:
        mov di, fname
        cmp byte [has_dir], 0
        je .boot
        mov si, outdir
.cp:
        lodsb
        test al, al
        jz .pre
        stosb
        jmp .cp
.boot:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        cmp dl, 'A'
        jae .dl
.cur:
        mov ah, 0x19
        int 0x21
        mov dl, al
        add dl, 'A'
.dl:
        mov al, dl
        stosb
        mov al, ':'
        stosb
.pre:
        mov si, s_sonido
.cp2:
        lodsb
        stosb
        test al, al
        jnz .cp2
        dec di                      ; DI -> donde va el número
        mov bl, 1
        cmp byte [has_dir], 0
        je .try
        ; con carpeta: primero SONIDO.TXT
        mov dword [di], '.TXT'
        mov byte [di+4], 0
        call .exists
        jc .make
        mov bl, 2
.try:
        mov al, bl
        aam
        add ax, 0x3030
        mov [di], ah
        mov [di+1], al
        mov dword [di+2], '.TXT'
        mov byte [di+6], 0
        call .exists
        jc .make
        inc bl
        cmp bl, 99
        jbe .try
        jmp .fail
.make:
        mov ah, 0x3C
        xor cx, cx
        mov dx, fname
        int 0x21
        jc .fail
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov byte [rep_ok], 1
        ret
.fail:
        ; no se puede en la unidad de arranque (¿protegida?): en la actual
        cmp byte [has_dir], 0
        jne .f2
        cmp byte [rc_retry], 0
        jne .f2
        mov byte [rc_retry], 1
        mov di, fname
        jmp .cur
.f2:
        mov byte [rep_ok], 0
        MODE O_SCR
        PR "ATENCION: no se puede crear el informe "
        mov si, fname
        call pstr
        PR NL, "(¿disco protegido o carpeta que no existe?). Se sigue sin guardar.", NL
        ret
; CF = 0 si fname existe
.exists:
        push bx
        mov ax, 0x3D00
        mov dx, fname
        int 0x21
        jc .ne
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        clc
.ne:
        pop bx
        ret

; añadir al fichero lo acumulado
rflush:
        pusha
        pushf
        cmp byte [rep_ok], 0
        je .z
        cmp word [rlen], 0
        je .z
        mov ax, 0x3D02
        mov dx, fname
        int 0x21
        jc .bad
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        mov ah, 0x40
        mov cx, [rlen]
        mov dx, rbuf
        int 0x21
        jc .bc
        cmp ax, [rlen]
        je .cl
.bc:
        mov byte [rep_ok], 0
.cl:
        mov ah, 0x3E
        int 0x21
        jmp .z
.bad:
        mov byte [rep_ok], 0
.z:
        mov word [rlen], 0
        popf
        popa
        ret

; ---- teclado ----
kflush:
        push ax
        push dx
.l:
        mov ah, 6
        mov dl, 0xFF
        int 0x21
        jnz .l
        pop dx
        pop ax
        ret

; SI = pregunta; [replay] = rutina que repite el sonido (0: sin «R»)
; -> AL = 'S' o 'N' (ZF = 1 si es S). Esc termina la prueba.
ask:
        push bx
        push cx
        push dx
        inc byte [qnum]
        mov byte [reps], 0
        call kflush
        mov al, [omode]
        mov ah, [hush]
        push ax
        mov byte [hush], 0          ; las preguntas siempre se ven
        MODE O_SCR | O_REP
        PR "P"
        mov al, [qnum]
        call pdec2
        PR ". "
        call pstr
        call .opts
.k:
        mov ah, 7
        int 0x21
        test al, al
        jnz .c
        mov ah, 7
        int 0x21
        jmp .k
.c:
        cmp al, 27
        je abort
        and al, 0xDF
        cmp al, 'Y'
        jne .c2
        mov al, 'S'
.c2:
        cmp al, 'S'
        je .ans
        cmp al, 'N'
        je .ans
        cmp al, 'R'
        jne .k
        cmp word [replay], 0
        je .k
        MODE O_SCR
        PR "R", NL
        inc byte [reps]
        push si
        call [replay]
        pop si
        call kflush
        MODE O_SCR
        PR "    "
        call pstr
        call .opts
        jmp .k
.ans:
        MODE O_SCR | O_REP
        call pchar
        cmp byte [reps], 0
        je .a2
        MODE O_REP
        PR "  (repetido "
        push ax
        movzx eax, byte [reps]
        call pdec
        pop ax
        PR ")"
        MODE O_SCR | O_REP
.a2:
        PR NL
        test byte [qnum], 1
        jnz .nf
        call rflush
.nf:
        pop bx
        mov [omode], bl
        mov [hush], bh
        pop dx
        pop cx
        pop bx
        cmp al, 'S'
        ret
.opts:
        cmp word [replay], 0
        je .sn
        PR " (S/N/R) "
        ret
.sn:
        PR " (S/N) "
        ret

; ---- tiempo (ticks de la BIOS, 18,2 por segundo) ----
; -> AX = contador de ticks
ticks:
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        ret
; esperar CX ticks (con un tope por si el reloj no anduviera)
wait_ticks:
        push cx
        inc cx                      ; (el primero puede estar a medias)
.t:
        call wait_tick1
        loop .t
        pop cx
        ret
; esperar al siguiente tick
wait_tick1:
        push ax
        push bx
        push edx
        sti
        call ticks
        mov bx, ax
        mov edx, 0x2000000
.w:
        in al, 0x80
        call ticks
        cmp ax, bx
        jne .n
        dec edx
        jnz .w
.n:
        pop edx
        pop bx
        pop ax
        ret
; esperar AX milisegundos
wait_ms:
        push cx
        push dx
        mov cx, 91
        mul cx
        mov cx, 5000
        div cx
        mov cx, ax
        pop dx
        call wait_ticks
        pop cx
        ret
; plazo de las órdenes: tmo_start; tmo_check -> CF = 1 si se agotó
tmo_start:
        push ax
        call ticks
        mov [tmo_t0], ax
        mov dword [tmo_cnt], 3000000
        pop ax
        sti
        ret
tmo_check:
        push ax
        dec dword [tmo_cnt]
        jz .out
        call ticks
        sub ax, [tmo_t0]
        cmp ax, 5
        pop ax
        cmc
        ret
.out:
        pop ax
        stc
        ret
; pausa corta (unos 20-50 ms de E/S)
delay:
        push cx
        push ax
        mov cx, 0x4000
.l:
        in al, 0x80
        loop .l
        pop ax
        pop cx
        ret

; =============================================================================
;  Cabecera del informe y datos del sistema
; =============================================================================
rep_header:
        MODE O_REP
        PR "INFORME DE SONIDO DE HISPA-DOS (AUDICION 1.0)", NL
        PR "Fecha: "
        mov ah, 0x2A
        int 0x21
        mov al, dl
        call pdec2
        PR "/"
        mov al, dh
        call pdec2
        PR "/"
        movzx eax, cx
        call pdec
        PR "  Hora: "
        mov ah, 0x2C
        int 0x21
        mov al, ch
        call pdec2
        PR ":"
        mov al, cl
        call pdec2
        PR NL, "Fichero: "
        mov si, fname
        call pstr
        PR NL
        cmp byte [v86], 0
        je .h
        PR "Procesador en modo V86 (hay un gestor de memoria tipo EMM386 cargado):", NL
        PR "  NO se puede probar el HD Audio ni el AC'97. Arranque sin HSPEMM.", NL
.h:
        cmp byte [hsp_was], 0
        je .r
        PR "HSPSND estaba cargado (tarjeta tipo "
        movzx eax, byte [hsp_type]
        call pdec
        PR "); se le ha pedido parar.", NL
.r:
        ret

collect_sys:
        MODE O_REP
        PR NL, "=== EQUIPO ===", NL
        call smbios
        ; procesador
        MODE O_REP
        PR "Procesador: "
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        jnz .cpuid
        PR "386/486 (sin CPUID)", NL
        jmp .mem
.cpuid:
        xor eax, eax
        cpuid
        mov [cpu_txt], ebx
        mov [cpu_txt+4], edx
        mov [cpu_txt+8], ecx
        mov byte [cpu_txt+12], 0
        mov si, cpu_txt
        call pstr
        mov eax, 1
        cpuid
        PR ", firma "
        call phex8
        PR "h"
        mov eax, 0x80000000
        cpuid
        cmp eax, 0x80000004
        jb .cn
        mov di, cpu_txt
        mov eax, 0x80000002
.b:
        push eax
        cpuid
        mov [di], eax
        mov [di+4], ebx
        mov [di+8], ecx
        mov [di+12], edx
        add di, 16
        pop eax
        inc eax
        cmp eax, 0x80000004
        jbe .b
        mov byte [di], 0
        PR ", "
        mov si, cpu_txt
.sk:
        cmp byte [si], ' '
        jne .pb
        inc si
        jmp .sk
.pb:
        call pstr
.cn:
        PR NL
.mem:
        PR "Memoria: "
        int 0x12
        movzx eax, ax
        call pdec
        PR " KB convencional, "
        mov ax, 0xE801
        int 0x15
        jc .old
        test ax, ax
        jnz .e
        mov ax, cx
        mov bx, dx
.e:
        movzx eax, ax
        movzx ebx, bx
        shl ebx, 6
        add eax, ebx
        jmp .pm
.old:
        mov ah, 0x88
        int 0x15
        movzx eax, ax
.pm:
        call pdec
        PR " KB extendida", NL
        push cs
        pop es
        ret

; ---- SMBIOS: fabricante, modelo y BIOS ----
; EBX = dirección física -> AL (0 si no se puede llegar)
rdphys:
        cmp byte [v86], 0
        jne .v
        mov al, [fs:ebx]
        ret
.v:
        cmp ebx, 0x10FFEF
        jae .z
        push es
        push ebx
        push ebx
        shr ebx, 4
        mov es, bx
        pop ebx
        and bx, 15
        mov al, [es:bx]
        pop ebx
        pop es
        ret
.z:
        xor al, al
        ret

smbios:
        mov ax, 0xF000
        mov es, ax
        xor di, di
.f:
        cmp dword [es:di], '_SM_'
        jne .n
        cmp dword [es:di+0x10], '_DMI'
        je .got
.n:
        add di, 16
        jnz .f
        ; SMBIOS 3 (entrada de 64 bits)
.f3:
        cmp dword [es:di], '_SM3'
        jne .n3
        cmp byte [es:di+4], '_'
        jne .n3
        cmp dword [es:di+0x14], 0   ; por encima de 4 GB no se llega
        jne .n3
        mov eax, [es:di+0x10]
        mov [sm_addr], eax
        mov eax, [es:di+0x0C]
        cmp eax, 0xFFFF
        jbe .l3
        mov eax, 0xFFFF
.l3:
        mov [sm_len], ax
        mov word [sm_cnt], 0xFFFF
        mov ax, [es:di+7]
        jmp .ver
.n3:
        add di, 16
        jnz .f3
        push cs
        pop es
        PR "SMBIOS: no hay (BIOS antigua)", NL
        ret
.got:
        mov eax, [es:di+0x18]
        mov [sm_addr], eax
        mov ax, [es:di+0x16]
        mov [sm_len], ax
        mov ax, [es:di+0x1C]
        mov [sm_cnt], ax
        mov ax, [es:di+6]
.ver:
        push cs
        pop es
        PR "SMBIOS "
        push ax
        movzx eax, al
        call pdec
        PR "."
        pop ax
        movzx eax, ah
        call pdec
        PR NL
        cmp byte [v86], 0
        jne .walk
        call unreal
.walk:
        mov ebx, [sm_addr]
        movzx edx, word [sm_len]
        add edx, ebx                ; fin de la tabla
        mov cx, [sm_cnt]
.st:
        test cx, cx
        jz .end
        lea eax, [ebx+4]
        cmp eax, edx
        jae .end
        call rdphys
        mov [sm_type], al
        cmp al, 127
        je .end
        mov si, sm_tab
.tt:
        cmp byte [si], 0xFF
        je .next
        cmp al, [si]
        jne .tn
        ; línea: nombre y hasta tres cadenas
        push si
        mov si, [si+4]
        call pstr
        pop si
        ; las del equipo (tipo 1) se guardan también para SONIDO.INI
        cmp byte [sm_type], 1
        jne .nc
        cmp word [caplen], 0
        jne .nc
        mov word [capp], smb_eq
        mov word [capmax], SMBEQ - 2
        or byte [omode], O_CAP
.nc:
        push cx
        mov cx, 3
        lea di, [si+1]
.fl:
        mov al, [di]
        test al, al
        jz .fn
        push ebx
        movzx eax, al
        add ebx, eax
        call rdphys
        pop ebx
        call smstr
        PR " | "
.fn:
        inc di
        loop .fl
        pop cx
        test byte [omode], O_CAP
        jz .nc2
        and byte [omode], ~O_CAP
        push bx
        mov bx, [caplen]            ; (sin el « | » del final)
        sub bx, 3
        jb .nc3
        mov [caplen], bx
        mov byte [smb_eq+bx], 0
.nc3:
        mov word [capmax], 0
        pop bx
.nc2:
        PR NL
        jmp .next
.tn:
        add si, 6
        jmp .tt
.next:
        ; saltar la parte con formato y las cadenas (acaban en dos ceros)
        push ebx
        inc ebx
        call rdphys
        pop ebx
        movzx eax, al
        cmp al, 4
        jb .end
        add ebx, eax
.z:
        cmp ebx, edx
        jae .end
        call rdphys
        mov ah, al
        inc ebx
        call rdphys
        test ax, ax
        jnz .z
        inc ebx
        dec cx
        jmp .st
.end:
        ret
; EBX = estructura, AL = número de cadena (1...) -> la imprime
smstr:
        push ebx
        push cx
        push dx
        mov dl, al
        test dl, dl
        jz .none
        push ebx
        inc ebx
        call rdphys
        pop ebx
        movzx eax, al
        add ebx, eax                ; primera cadena
.s:
        dec dl
        jz .p
        mov cx, 200
.k:
        call rdphys
        inc ebx
        test al, al
        loopnz .k
        call rdphys
        test al, al                 ; fin de las cadenas
        jz .none
        jmp .s
.p:
        mov cx, 64
.pl:
        call rdphys
        inc ebx
        cmp al, ' '
        jb .r
        cmp al, 126
        ja .q
        call pchar
.q:
        loop .pl
        jmp .r
.none:
        PR "-"
.r:
        pop dx
        pop cx
        pop ebx
        ret
; tipo, desplazamientos de hasta 3 cadenas, texto
sm_tab      db 0, 4, 5, 8
            dw sm_t0
            db 1, 4, 5, 6
            dw sm_t1
            db 2, 4, 5, 6
            dw sm_t2
            db 0xFF
sm_t0       db "BIOS (fabricante | versión | fecha): ", 0
sm_t1       db "Equipo (fabricante | modelo | versión): ", 0
sm_t2       db "Placa (fabricante | modelo | versión): ", 0

; =============================================================================
;  PCI (mecanismo de configuración 1: puertos CF8h/CFCh)
; =============================================================================
; BX = bus:disp.fun, DI = registro -> EAX
pci_rd32:
        push dx
        movzx eax, bx
        shl eax, 8
        or eax, 0x80000000
        push di
        and di, 0xFC
        or ax, di
        pop di
        mov dx, 0xCF8
        out dx, eax
        mov dx, 0xCFC
        in eax, dx
        pop dx
        ret
pci_rd16:
        push cx
        call pci_rd32
        mov cx, di
        and cl, 2
        shl cl, 3
        shr eax, cl
        pop cx
        ret
pci_rd8:
        push cx
        call pci_rd32
        mov cx, di
        and cl, 3
        shl cl, 3
        shr eax, cl
        pop cx
        ret
; BX, DI = registro, AX = valor
pci_wr16:
        push dx
        push eax
        movzx eax, bx
        shl eax, 8
        or eax, 0x80000000
        push di
        and di, 0xFC
        or ax, di
        pop di
        mov dx, 0xCF8
        out dx, eax
        pop eax
        mov dx, di
        and dx, 2
        add dx, 0xCFC
        out dx, ax
        pop dx
        ret
pci_wr8:
        push dx
        push eax
        movzx eax, bx
        shl eax, 8
        or eax, 0x80000000
        push di
        and di, 0xFC
        or ax, di
        pop di
        mov dx, 0xCF8
        out dx, eax
        pop eax
        mov dx, di
        and dx, 3
        add dx, 0xCFC
        out dx, al
        pop dx
        ret
; BX = bus:disp.fun -> «bb:dd.f»
pbdf:
        push ax
        mov al, bh
        call phex2
        PR ":"
        mov al, bl
        shr al, 3
        call phex2
        PR "."
        mov al, bl
        and al, 7
        call phex1
        pop ax
        ret

; buscar las controladoras de sonido: clase 04h, subclases 01h (audio),
; 03h (HD Audio) y 80h (otra)
pci_scan:
        MODE O_REP
        PR NL, "=== CONTROLADORAS DE SONIDO (PCI) ===", NL
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        push cs
        pop es
        jc .nopci
        cmp edx, 0x20494350
        jne .nopci
        test al, 1
        jz .nopci
        mov [pci_last], cl
        xor bx, bx
.dev:
        xor di, di
        call pci_rd32
        cmp ax, 0xFFFF
        jne .have
        test bl, 7
        jz .nextdev
        jmp .nextfn
.have:
        mov di, 0x08
        call pci_rd32
        shr eax, 16
        cmp ah, 0x04
        jne .nf
        cmp al, 0x01
        je .add
        cmp al, 0x03
        je .add
        cmp al, 0x80
        jne .nf
.add:
        movzx si, byte [nctl]
        cmp si, NCTL
        jae .nf
        shl si, 2
        mov [ctl_tab+si], bx
        mov [ctl_tab+si+2], al
        inc byte [nctl]
.nf:
        ; ¿más funciones?
        test bl, 7
        jnz .nextfn
        mov di, 0x0E
        call pci_rd8
        test al, 0x80
        jz .nextdev
.nextfn:
        inc bx
        test bl, 7
        jnz .dev
        jmp .cont
.nextdev:
        or bl, 7
        inc bx
.cont:
        test bx, bx
        jz .done
        cmp bh, [pci_last]
        jbe .dev
.done:
        cmp byte [nctl], 0
        jne .list
        PR "Ninguna (clase 04h, subclases 01h, 03h y 80h).", NL
        ret
.nopci:
        PR "Este equipo no tiene BIOS PCI.", NL
        ret
.list:
        xor si, si
.l:
        push si
        shl si, 2
        mov bx, [ctl_tab+si]
        mov al, [ctl_tab+si+2]
        call pci_dump
        pop si
        inc si
        mov ax, si
        cmp al, [nctl]
        jb .l
        ret

; BX = bdf, AL = subclase
pci_dump:
        mov [pd_sub], al
        PR NL, "PCI "
        call pbdf
        PR "  "
        xor di, di
        call pci_rd32
        mov [pd_ven], ax
        call phex4
        PR ":"
        shr eax, 16
        call phex4
        PR "  subsistema "
        mov di, 0x2C
        call pci_rd32
        call phex4
        PR ":"
        shr eax, 16
        call phex4
        PR "  clase 04"
        mov al, [pd_sub]
        call phex2
        mov si, t_hda
        cmp al, 3
        je .cn
        mov si, t_ac97
        cmp al, 1
        je .cn
        mov si, t_other
.cn:
        call pstr
        PR NL, "  Orden/estado "
        mov di, 0x04
        call pci_rd32
        call phex4
        PR "/"
        shr eax, 16
        call phex4
        PR "  revisión "
        mov di, 0x08
        call pci_rd8
        call phex2
        PR "  IRQ "
        mov di, 0x3C
        call pci_rd8
        movzx eax, al
        call pdec
        PR " (patilla "
        mov di, 0x3D
        call pci_rd8
        call phex2
        PR ")", NL, "  BAR:"
        mov di, 0x10
.bar:
        PR " "
        call pci_rd32
        call phex8
        add di, 4
        cmp di, 0x28
        jb .bar
        PR NL
        ; registros propios del fabricante
        mov ax, [pd_ven]
        cmp byte [pd_sub], 3
        jne .dump
        cmp ax, 0x8086
        jne .amd
        PR "  Intel: TCSEL (44h) = "
        mov di, 0x44
        call pci_rd8
        call phex2
        PR "h", NL
        jmp .dump
.amd:
        cmp ax, 0x1002
        je .ati
        cmp ax, 0x1022
        jne .nv
.ati:
        PR "  AMD/ATI: 42h = "
        mov di, 0x42
        call pci_rd8
        call phex2
        PR "h (bits 0-2 = 010: caché «snoop» activa), 44h = "
        mov di, 0x44
        call pci_rd8
        call phex2
        PR "h", NL
        jmp .dump
.nv:
        cmp ax, 0x10DE
        jne .dump
        PR "  Nvidia: 4Eh = "
        mov di, 0x4E
        call pci_rd8
        call phex2
        PR "h (bits 0-3 = 1111: «snoop» activa)", NL
.dump:
        PR "  Configuración PCI (256 octetos):", NL
        xor di, di
.row:
        PR "   "
        mov ax, di
        call phex2
        PR ":"
.col:
        PR " "
        call pci_rd8
        call phex2
        inc di
        test di, 15
        jnz .col
        PR NL
        cmp di, 0x100
        jb .row
        ret
t_hda       db " (HD Audio)", 0
t_ac97      db " (audio / AC'97)", 0
t_other     db " (otro multimedia)", 0

; =============================================================================
;  A20 y modo irreal (FS con límite de 4 GB)
; =============================================================================
unreal:
        push eax
        push bx
        cli
        lgdt [cs:gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        sti
        pop bx
        pop eax
        ret

; los registros están por encima de 1 MB: con A20 cerrada se leería otra
; dirección. Se abre mientras dura la prueba y se deja como estaba.
a20_open:
        pushf
        push ax
        cli
        call a20_test
        jnz .r
        mov byte [a20_closed], 1
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
        call a20_test
        jnz .r
        mov ax, 0x2401
        int 0x15
.r:
        pop ax
        popf
        ret
a20_restore:
        pushf
        push ax
        cli
        cmp byte [a20_closed], 0
        je .r
        mov byte [a20_closed], 0
        in al, 0x92
        and al, 0xFC
        out 0x92, al
        call a20_test
        jz .r
        mov ax, 0x2400
        int 0x15
.r:
        pop ax
        popf
        ret
; ZF = 1 si A20 está cerrada (0000:0500 y FFFF:0510 son el mismo octeto)
a20_test:
        push ds
        push es
        push ax
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov al, [0x500]
        mov ah, al
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        mov [0x500], ah
        jne .open
        cmp ah, [es:0x510]
        jne .open
        xor al, al
        jmp .x
.open:
        or al, 1
.x:
        pop ax
        pop es
        pop ds
        ret

; EAX <- dirección física de dseg:BX
dphys:
        push ebx
        movzx ebx, bx
        movzx eax, word [dseg]
        shl eax, 4
        add eax, ebx
        pop ebx
        ret

; =============================================================================
;  Intel HD Audio: registros y órdenes al códec
; =============================================================================
; registros: DX = desplazamiento
hrd32:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov eax, [fs:esi]
        pop esi
        ret
hwr32:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov [fs:esi], eax
        pop esi
        ret
hrd16:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov ax, [fs:esi]
        pop esi
        ret
hwr16:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov [fs:esi], ax
        pop esi
        ret
hrd8:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov al, [fs:esi]
        pop esi
        ret
hwr8:
        push esi
        movzx esi, dx
        add esi, [hda_base]
        mov [fs:esi], al
        pop esi
        ret

; EAX = orden (sin la dirección del códec) -> EAX = respuesta; CF si no
; contesta (y EAX = 0). Nunca espera más de un cuarto de segundo; al cuarto
; fallo seguido el códec se da por perdido y ya no se le espera.
hda_cmd:
        push ecx
        push edx
        push ebx
        cmp byte [codec_dead], 0
        jne .dead
        movzx ebx, byte [cad]
        shl ebx, 28
        or eax, ebx
        mov [last_cmd], eax
        call unreal
        cmp byte [use_corb], 0
        je .pio
        call corb_cmd
        jmp .res
.pio:
        call pio_cmd
.res:
        jc .fail
        mov byte [fails], 0
        pop ebx
        pop edx
        pop ecx
        clc
        ret
.fail:
        inc word [nfail]
        inc byte [fails]
        mov al, [omode]
        push ax
        MODE O_REP
        PR "  [sin respuesta a la orden "
        mov eax, [last_cmd]
        call phex8
        PR "h]", NL
        cmp byte [fails], 4
        jb .f2
        mov byte [codec_dead], 1
        PR "  [el códec ha dejado de contestar: se abandona]", NL
.f2:
        pop ax
        mov [omode], al
.dead:
        pop ebx
        pop edx
        pop ecx
        xor eax, eax
        stc
        ret

; orden por CORB/RIRB (EAX completa)
corb_cmd:
        push es
        mov es, [dseg]
        mov bx, [corb_wp]
        inc bx
        and bx, [corb_mask]
        mov [corb_wp], bx
        shl bx, 2
        mov [es:D_CORB+bx], eax
        mov dx, 0x48
        mov ax, [corb_wp]
        call hwr16
        call tmo_start
.w:
        mov dx, 0x58
        call hrd16
        and ax, [rirb_mask]
        cmp ax, [rirb_rp]
        jne .got
        call tmo_check
        jnc .w
        pop es
        stc
        ret
.got:
        mov bx, [rirb_rp]
        inc bx
        and bx, [rirb_mask]
        mov [rirb_rp], bx
        shl bx, 3
        mov eax, [es:D_RIRB+bx]
        test byte [es:D_RIRB+bx+4], 0x10    ; no pedida: se salta
        jnz .w
        push eax
        mov dx, 0x5D
        mov al, 5
        call hwr8
        pop eax
        pop es
        clc
        ret

; orden inmediata (registros 60h/64h/68h), lo que usa HSPSND
pio_cmd:
        mov ebx, eax
        call tmo_start
.w1:
        mov dx, 0x68
        call hrd16
        test al, 1
        jz .free
        call tmo_check
        jnc .w1
        ; atascado: se limpia y se intenta igualmente
.free:
        mov dx, 0x68
        mov ax, 2
        call hwr16
        mov eax, ebx
        mov dx, 0x60
        call hwr32
        mov dx, 0x68
        mov ax, 1
        call hwr16
        call tmo_start
.w2:
        mov dx, 0x68
        call hrd16
        test al, 2
        jnz .got
        call tmo_check
        jnc .w2
        stc
        ret
.got:
        mov dx, 0x64
        call hrd32
        push eax
        mov dx, 0x68
        mov ax, 2
        call hwr16
        pop eax
        clc
        ret

; BL = nodo, AX = verbo de 12 bits, CL = dato -> EAX
hda_verb:
        push ebx
        push ecx
        movzx ebx, bl
        shl ebx, 20
        movzx eax, ax
        shl eax, 8
        or eax, ebx
        movzx ecx, cl
        or eax, ecx
        call hda_cmd
        pop ecx
        pop ebx
        ret
; BL = nodo, AL = verbo de 4 bits, CX = dato de 16 bits -> EAX
hda_verb4:
        push ebx
        push ecx
        movzx ebx, bl
        shl ebx, 20
        movzx eax, al
        shl eax, 16
        or eax, ebx
        movzx ecx, cx
        or eax, ecx
        call hda_cmd
        pop ecx
        pop ebx
        ret
; BL = nodo, CL = parámetro -> EAX
hda_param:
        mov ax, 0xF00
        jmp hda_verb

; poner en marcha CORB y RIRB (anillos en el segmento de DMA)
corb_init:
        push es
        mov es, [dseg]
        xor di, di
        mov cx, 0xC00 / 2
        xor ax, ax
        rep stosw
        pop es
        mov dx, 0x4C
        xor al, al
        call hwr8
        mov dx, 0x5C
        call hwr8
        call tmo_start
.s:
        mov dx, 0x4C
        call hrd8
        test al, 2
        jz .st
        call tmo_check
        jnc .s
.st:
        ; tamaños: el mayor que admita (256, 16 o 2 entradas)
        mov dx, 0x4E
        call .size
        mov [corb_mask], bx
        mov dx, 0x5E
        call .size
        mov [rirb_mask], bx
        xor bx, bx
        call dphys
        mov dx, 0x40
        call hwr32
        mov dx, 0x44
        xor eax, eax
        call hwr32
        mov bx, D_RIRB
        call dphys
        mov dx, 0x50
        call hwr32
        mov dx, 0x54
        xor eax, eax
        call hwr32
        ; punteros a cero
        mov dx, 0x48
        xor ax, ax
        call hwr16
        mov dx, 0x4A
        mov ax, 0x8000
        call hwr16
        call tmo_start
.r1:
        mov dx, 0x4A
        call hrd16
        test ah, 0x80
        jnz .r1d
        call tmo_check
        jnc .r1
.r1d:
        mov dx, 0x4A
        xor ax, ax
        call hwr16
        call tmo_start
.r2:
        mov dx, 0x4A
        call hrd16
        test ah, 0x80
        jz .r2d
        call tmo_check
        jnc .r2
.r2d:
        mov dx, 0x58
        mov ax, 0x8000
        call hwr16
        mov dx, 0x5A
        mov ax, 1
        call hwr16
        mov word [corb_wp], 0
        mov word [rirb_rp], 0
        mov dx, 0x5D
        mov al, 5
        call hwr8
        mov dx, 0x4C
        mov al, 2
        call hwr8
        ; RIRB en marcha y con su aviso (RINTCTL): la interrupción general
        ; (INTCTL) está apagada, así que solo se enciende el bit de estado,
        ; que se borra tras cada respuesta
        mov dx, 0x5C
        mov al, 3
        call hwr8
        ret
; DX = registro de tamaño -> BX = máscara del índice
.size:
        call hrd8
        mov bx, 0xFF
        mov ah, 2
        test al, 0x40
        jnz .sz
        mov bx, 0x0F
        mov ah, 1
        test al, 0x20
        jnz .sz
        mov bx, 1
        mov ah, 0
.sz:
        mov al, ah
        call hwr8
        ret

; ---- abrir la controladora [cur_ctl] ----
; [do_reset] = 1: reinicio siempre; 0: solo si la BIOS la dejó parada.
; [verbose] = 1: escribe sus registros en el informe. CF si no se puede usar.
hda_open:
        mov byte [hda_live], 0
        movzx si, byte [cur_ctl]
        shl si, 2
        mov bx, [ctl_tab+si]
        mov [cur_bdf], bx
        xor di, di
        call pci_rd32
        mov [cur_ven], ax
        mov di, 0x10
        call pci_rd32
        mov ecx, eax
        and eax, 0xFFFFFFF0
        mov [hda_base], eax
        test cl, 1
        jnz .nobar
        test eax, eax
        jz .nobar
        and cl, 6
        cmp cl, 4                   ; BAR de 64 bits: la parte alta, a cero
        jne .b32
        mov di, 0x14
        call pci_rd32
        test eax, eax
        jz .b32
        PR "  Registros por encima de 4 GB: no se puede llegar.", NL
        stc
        ret
.nobar:
        PR "  La BIOS no ha dado dirección a los registros (BAR0): no se puede usar.", NL
        stc
        ret
.b32:
        ; memoria y bus master
        mov di, 0x04
        call pci_rd16
        mov [cur_cmd0], ax
        or ax, 0x0006
        call pci_wr16
        ; Intel: clase de tráfico 0 (TCSEL)
        cmp word [cur_ven], 0x8086
        jne .ntc
        mov di, 0x44
        call pci_rd8
        and al, 0xF8
        call pci_wr8
.ntc:
        call unreal
        cmp byte [verbose], 0
        je .rst
        PR "  Registros en "
        mov eax, [hda_base]
        call phex8
        PR "h (orden PCI antes: "
        mov ax, [cur_cmd0]
        call phex4
        PR "h)", NL, "  Como lo dejó la BIOS: GCTL="
        mov dx, 0x08
        call hrd32
        call phex8
        PR " STATESTS="
        mov dx, 0x0E
        call hrd16
        call phex4
        PR " INTCTL="
        mov dx, 0x20
        call hrd32
        call phex8
        PR " CORBCTL="
        mov dx, 0x4C
        call hrd8
        call phex2
        PR " RIRBCTL="
        mov dx, 0x5C
        call hrd8
        call phex2
        PR NL
.rst:
        mov byte [was_reset], 1
        cmp byte [do_reset], 0
        jne .doit
        mov dx, 0x08
        call hrd32
        test al, 1
        jz .doit
        mov dx, 0x0E
        call hrd16
        and ax, 0x7FFF
        jz .doit
        mov byte [was_reset], 0
        jmp .up
.doit:
        ; reiniciar el enlace (GCTL.CRST)
        mov dx, 0x08
        xor eax, eax
        call hwr32
        call tmo_start
.c0:
        mov dx, 0x08
        call hrd32
        test al, 1
        jz .c0d
        call tmo_check
        jnc .c0
.c0d:
        mov cx, 1
        call wait_ticks
        mov dx, 0x08
        mov eax, 1
        call hwr32
        call tmo_start
.c1:
        mov dx, 0x08
        call hrd32
        test al, 1
        jnz .c1d
        call tmo_check
        jnc .c1
        PR "  La controladora no sale del reinicio (GCTL.CRST): no se puede usar.", NL
        stc
        ret
.c1d:
        ; los códecs tardan en presentarse
        mov cx, 2
        call wait_ticks
        mov cx, 9
.cw:
        mov dx, 0x0E
        call hrd16
        and ax, 0x7FFF
        jnz .up
        push cx
        mov cx, 1
        call wait_ticks
        pop cx
        loop .cw
.up:
        mov dx, 0x0E
        call hrd16
        and ax, 0x7FFF
        mov [statests], ax
        ; sin interrupciones
        mov dx, 0x20
        xor eax, eax
        call hwr32
        mov dx, 0x0C
        xor ax, ax
        call hwr16
        ; primer flujo de salida
        mov dx, 0x00
        call hrd16
        mov [gcap], ax
        shr ax, 8
        and ax, 0x0F
        shl ax, 5
        add ax, 0x80
        mov [hda_sd], ax
        ; por si la BIOS dejó el flujo andando
        mov dx, ax
        xor al, al
        call hwr8
        cmp byte [verbose], 0
        je .probe
        PR "  GCAP="
        mov ax, [gcap]
        call phex4
        PR "h (flujos: "
        mov al, ah
        shr al, 4
        call phex1
        PR " de salida, "
        mov al, [gcap+1]
        and al, 15
        call phex1
        PR " de entrada, "
        mov al, [gcap]
        shr al, 3
        call phex2
        PR " bidireccionales; 64 bits: "
        mov al, [gcap]
        and al, 1
        call phex1
        PR ")  versión "
        mov dx, 0x03
        call hrd8
        call phex1
        PR "."
        mov dx, 0x02
        call hrd8
        call phex1
        PR NL, "  "
        cmp byte [was_reset], 0
        je .nr
        PR "Enlace reiniciado. "
        jmp .ss
.nr:
        PR "Sin reiniciar (se lee lo que dejó la BIOS). "
.ss:
        PR "STATESTS="
        mov ax, [statests]
        call phex4
        PR "h  CORBSIZE="
        mov dx, 0x4E
        call hrd8
        call phex2
        PR "h RIRBSIZE="
        mov dx, 0x5E
        call hrd8
        call phex2
        PR "h", NL
.probe:
        cmp word [statests], 0
        jne .hc
        PR "  Ningún códec contesta (STATESTS = 0).", NL
        stc
        ret
.hc:
        ; primer códec presente
        mov ax, [statests]
        bsf ax, ax
        mov [cad], al
        ; a) órdenes inmediatas (como HSPSND), con CORB/RIRB parados
        mov dx, 0x4C
        xor al, al
        call hwr8
        mov dx, 0x5C
        call hwr8
        mov byte [use_corb], 0
        call .try
        mov [pio_ok], al
        ; b) CORB/RIRB
        call corb_init
        mov byte [use_corb], 1
        call .try
        mov [corb_ok], al
        mov [use_corb], al
        mov byte [codec_dead], 0
        mov byte [fails], 0
        cmp byte [verbose], 0
        je .pv
        PR "  Las órdenes al códec: por CORB/RIRB "
        mov al, [corb_ok]
        call psino
        PR "; inmediatas (lo que usa HSPSND) "
        mov al, [pio_ok]
        call psino
        PR NL
.pv:
        mov al, [corb_ok]
        or al, [pio_ok]
        jnz .ok
        PR "  El códec no contesta por ninguna de las dos vías.", NL
        stc
        ret
.ok:
        cmp byte [corb_ok], 0
        jne .ok2
        ; CORB no va: se para y se sigue con las inmediatas
        mov dx, 0x4C
        xor al, al
        call hwr8
        mov dx, 0x5C
        call hwr8
.ok2:
        mov byte [hda_live], 1
        clc
        ret
; -> AL = 1 si el códec contesta a «identificación» con algo válido
.try:
        mov byte [codec_dead], 0
        mov byte [fails], 0
        mov eax, 0x000F0000
        call hda_cmd
        jc .t0
        test eax, eax
        jz .t0
        cmp eax, 0xFFFFFFFF
        je .t0
        mov al, 1
        ret
.t0:
        xor al, al
        ret

; AL = 0 «NO», otro «sí»
psino:
        test al, al
        jz .n
        PR "sí"
        ret
.n:
        PR "NO"
        ret

; ---- el grupo de audio del códec [cad] -> CF si no tiene ----
codec_afg:
        mov byte [codec_dead], 0
        mov byte [fails], 0
        xor bl, bl
        mov cl, 0x04
        call hda_param
        jc .no
        mov ch, al
        shr eax, 16
        mov bl, al
.fg:
        test ch, ch
        jz .no
        mov cl, 0x05
        call hda_param
        jc .no
        cmp al, 1
        je .afg
        inc bl
        dec ch
        jmp .fg
.no:
        stc
        ret
.afg:
        mov [afg], bl
        mov cl, 0x04
        call hda_param
        mov [w_count], al
        shr eax, 16
        mov [w_first], al
        ; GPIO que hay
        mov cl, 0x11
        call hda_param
        cmp al, 8
        jbe .g
        mov al, 8
.g:
        mov [g_n], al
        mov cl, al
        mov ax, 1
        shl ax, cl
        dec ax
        mov [g_all], al
        clc
        ret

; BL = nodo -> AL = tipo (FFh si falla), AH = 1 si es digital; [wcaps]
w_type:
        push cx
        mov cl, 0x09
        call hda_param
        pop cx
        jc .e
        mov [wcaps], eax
        shr eax, 20
        and ax, 0x0F
        test byte [wcaps+1], 0x02
        jz .x
        mov ah, 1
.x:
        ret
.e:
        mov dword [wcaps], 0
        mov ax, 0x00FF
        ret

; lista de conexiones del nodo BL en DS:DI (hasta CONNMAX) -> CX = cuántas
get_conn:
        push bx
        push dx
        push si
        push bp
        xor bp, bp
        mov cl, 0x0E
        call hda_param
        jc .done
        mov dl, al
        and dl, 0x7F
        mov dh, al
        and dh, 0x80                ; forma larga (16 bits por entrada)
        xor si, si
        mov byte [gc_prev], 0
.blk:
        mov cx, si
        cmp cl, dl
        jae .done
        mov ax, 0xF02
        call hda_verb
        jc .done
        mov byte [gc_k], 4
        test dh, dh
        jz .ent
        mov byte [gc_k], 2
.ent:
        push eax
        test dh, dh
        jnz .long
        mov ah, al
        and ax, 0x807F
        jmp .have
.long:
        and ah, 0x80
.have:
        test ah, ah                 ; rango: desde la anterior hasta esta
        jz .one
        test bp, bp
        jz .one
        mov ah, [gc_prev]
.rg:
        inc ah
        cmp ah, al
        jae .one
        xchg al, ah
        call .add
        xchg al, ah
        jmp .rg
.one:
        call .add
        mov [gc_prev], al
        pop eax
        shr eax, 8
        test dh, dh
        jz .sh
        shr eax, 8
.sh:
        inc si
        mov cx, si
        cmp cl, dl
        jae .done
        dec byte [gc_k]
        jnz .ent
        jmp .blk
.add:
        cmp bp, CONNMAX
        jae .ar
        mov [di+bp], al
        inc bp
.ar:
        ret
.done:
        mov cx, bp
        pop bp
        pop si
        pop dx
        pop bx
        ret

; capacidades del amplificador (CL = 12h salida, 0Dh entrada) del nodo BL;
; si el nodo no trae las suyas valen las del grupo de audio -> EAX
amp_caps:
        push bx
        test byte [wcaps], 0x08
        jnz .own
        mov bl, [afg]
.own:
        call hda_param
        pop bx
        ret

; =============================================================================
;  Tono de 440 Hz (seno) y su reproducción por el flujo de salida
; =============================================================================
; frecuencia, formato HD Audio, bit del parámetro 0Ah, 2*sen(pi*440/f)*2^30
rate_tab    dw 8000, 0x0500, 0x01
            dd 369214931
            dw 11025, 0x4300, 0x02
            dd 268543955
            dw 16000, 0x0200, 0x04
            dd 185298560
            dw 22050, 0x4100, 0x08
            dd 134536252
            dw 32000, 0x0A00, 0x10
            dd 92735788
            dw 44100, 0x4000, 0x20
            dd 67301185
            dw 48000, 0x0000, 0x40
            dd 61834542
RI_8K       equ 0
RI_11K      equ 1
RI_22K      equ 3
RI_44K      equ 5
RI_48K      equ 6

; SI <- entrada de rate_tab de [t_ri]
rate_ptr:
        push ax
        mov al, 10
        mul byte [t_ri]
        mov si, ax
        add si, rate_tab
        pop ax
        ret

; llena dseg:[t_off] con 0,2 s del tono (88 periodos justos: se puede tocar
; en bucle). [t_ri] frecuencia, [t_bits] 16 u 8, [t_ch] bit 0 izquierda y
; bit 1 derecha, [t_amp] amplitud. Siempre dos canales. -> [t_bytes]
gen_tone:
        pushad
        push es
        call rate_ptr
        mov ax, [si]
        xor dx, dx
        mov cx, 5
        div cx
        mov [t_frames], ax
        mov cx, ax
        mov eax, [si+6]
        mov [g_eps], eax
        movzx eax, word [t_amp]
        shl eax, 16
        mov [g_x], eax
        mov dword [g_y], 0
        mov es, [dseg]
        mov di, [t_off]
.l:
        mov eax, [g_x]
        imul dword [g_eps]
        shrd eax, edx, 30
        add [g_y], eax
        mov eax, [g_y]
        imul dword [g_eps]
        shrd eax, edx, 30
        sub [g_x], eax
        mov eax, [g_y]
        sar eax, 16
        mov bx, ax                  ; izquierda
        mov dx, ax                  ; derecha
        test byte [t_ch], 1
        jnz .l1
        xor bx, bx
.l1:
        test byte [t_ch], 2
        jnz .l2
        xor dx, dx
.l2:
        cmp byte [t_bits], 8
        je .b8
        mov ax, bx
        stosw
        mov ax, dx
        stosw
        jmp .n
.b8:
        mov al, bh
        add al, 0x80
        stosb
        mov al, dh
        add al, 0x80
        stosb
.n:
        loop .l
        mov ax, di
        sub ax, [t_off]
        mov [t_bytes], ax
        pop es
        popad
        ret

; parar el flujo de salida
hda_stream_stop:
        push ax
        push dx
        call unreal
        mov dx, [hda_sd]
        xor al, al
        call hwr8
        mov dx, [hda_sd]
        add dx, 3
        mov al, 0x1C
        call hwr8
        pop dx
        pop ax
        ret

; tocar el tono durante [t_ms] milisegundos por el flujo 1 -> [lpib_ok] = 1
; si la posición del flujo avanzó (el controlador está leyendo la memoria)
hda_tone:
        pushad
        mov word [t_off], D_ABUF
        call gen_tone
        call unreal
        ; parar y reiniciar el flujo
        mov dx, [hda_sd]
        xor al, al
        call hwr8
        mov al, 1
        call hwr8
        call tmo_start
.r1:
        mov dx, [hda_sd]
        call hrd8
        test al, 1
        jnz .r1d
        call tmo_check
        jnc .r1
.r1d:
        mov dx, [hda_sd]
        xor al, al
        call hwr8
        call tmo_start
.r2:
        mov dx, [hda_sd]
        call hrd8
        test al, 1
        jz .r2d
        call tmo_check
        jnc .r2
.r2d:
        ; lista de descriptores: dos entradas (el mínimo que pide la norma)
        push es
        mov es, [dseg]
        mov ax, [t_frames]
        shr ax, 1
        shl ax, 2
        cmp byte [t_bits], 8
        jne .h
        shr ax, 1
.h:
        mov cx, ax                  ; octetos de la primera mitad
        mov bx, D_ABUF
        call dphys
        mov [es:D_BDL], eax
        mov dword [es:D_BDL+4], 0
        movzx eax, cx
        mov [es:D_BDL+8], eax
        mov dword [es:D_BDL+12], 0
        mov bx, D_ABUF
        add bx, cx
        call dphys
        mov [es:D_BDL+16], eax
        mov dword [es:D_BDL+20], 0
        movzx eax, word [t_bytes]
        movzx ecx, cx
        sub eax, ecx
        mov [es:D_BDL+24], eax
        mov dword [es:D_BDL+28], 0
        pop es
        mov bx, D_BDL
        call dphys
        mov dx, [hda_sd]
        add dx, 0x18
        call hwr32
        add dx, 4
        xor eax, eax
        call hwr32
        mov dx, [hda_sd]
        add dx, 0x08
        movzx eax, word [t_bytes]
        call hwr32
        mov dx, [hda_sd]
        add dx, 0x0C
        mov ax, 1
        call hwr16
        call tone_fmt
        mov dx, [hda_sd]
        add dx, 0x12
        mov ax, [t_fmt]
        call hwr16
        mov dx, [hda_sd]
        add dx, 2
        mov al, 0x10                ; flujo número 1
        call hwr8
        ; formato y flujo en los DAC en uso
        mov si, dacs
.df:
        mov bl, [si]
        test bl, bl
        jz .dfe
        mov al, 2
        mov cx, [t_fmt]
        call hda_verb4
        mov ax, 0x706
        mov cl, 0x10
        call hda_verb
        inc si
        cmp si, dacs + 8
        jb .df
.dfe:
        call unreal
        mov dx, [hda_sd]
        mov al, 2                   ; RUN
        call hwr8
        mov byte [lpib_ok], 0
        mov dx, [hda_sd]
        add dx, 4
        call hrd32
        mov ebp, eax
        ; esperar mirando la posición
        mov ax, [t_ms]
        mov cx, 91
        mul cx
        mov cx, 5000
        div cx
        mov cx, ax
        inc cx
.wt:
        call wait_tick1
        mov dx, [hda_sd]
        add dx, 4
        call hrd32
        cmp eax, ebp
        je .same
        mov byte [lpib_ok], 1
.same:
        loop .wt
        call hda_stream_stop
        mov ax, 60
        call wait_ms
        popad
        ret

; [t_fmt] <- formato del flujo para [t_ri] y [t_bits], dos canales
tone_fmt:
        push si
        push ax
        call rate_ptr
        mov ax, [si+2]
        or al, 0x01
        cmp byte [t_bits], 8
        je .w
        or al, 0x10
.w:
        mov [t_fmt], ax
        pop ax
        pop si
        ret

; =============================================================================
;  Recogida: volcado de los códecs HD Audio, AC'97 y Sound Blaster
; =============================================================================
collect_cards:
        xor si, si
.l:
        mov ax, si
        cmp al, [nctl]
        jae .cards
        push si
        mov [cur_ctl], al
        shl si, 2
        mov bx, [ctl_tab+si]
        mov [cur_bdf], bx
        mov al, [ctl_tab+si+2]
        cmp al, 3
        je .hda
        cmp al, 1
        jne .n
        cmp byte [v86], 0
        jne .n
        cmp word [nabm], 0          ; solo el primero
        jne .n
        call ac97_collect
        jmp .n
.hda:
        inc byte [n_hda]
        cmp byte [v86], 0
        jne .n
        MODE O_REP
        PR NL, "=== HD AUDIO "
        mov bx, [cur_bdf]
        call pbdf
        PR " ===", NL
        mov byte [verbose], 1
        mov byte [do_reset], 0
        call hda_open
        mov byte [verbose], 0
        jc .n
        xor cl, cl
.cad:
        mov ax, [statests]
        shr ax, cl
        test al, 1
        jz .cn
        mov [cad], cl
        push cx
        call dump_codec
        pop cx
.cn:
        inc cl
        cmp cl, 15
        jb .cad
        call rflush
.n:
        pop si
        inc si
        jmp .l
.cards:
        call sb_collect
        MODE O_REP
        PR NL, "=== ALTAVOZ DEL PC ===", NL
        PR "Puerto 61h = "
        in al, 0x61
        call phex2
        PR "h (se prueba siempre, con el temporizador 2)", NL
        MODE O_SCR
        cmp byte [n_hda], 0
        jne .s2
        cmp word [nabm], 0
        jne .s2
        cmp word [sb_base], 0
        jne .s2
        PR "  No se ha encontrado ninguna tarjeta de sonido.", NL
.s2:
        ret

; nombres de fabricantes de códecs (por la mitad alta del identificador)
ven_tab     dw 0x10EC, v_realtek, 0x14F1, v_conexant, 0x111D, v_idt
            dw 0x8384, v_sigmatel, 0x1013, v_cirrus, 0x8086, v_intel
            dw 0x1002, v_ati, 0x10DE, v_nvidia, 0x1106, v_via
            dw 0x11D4, v_adi, 0x1AF4, v_qemu, 0x1022, v_amd, 0x434D, v_cmedia
            dw 0x15AD, v_vmware, 0x1D17, v_zhaoxin, 0
v_realtek   db "Realtek", 0
v_conexant  db "Conexant", 0
v_idt       db "IDT", 0
v_sigmatel  db "SigmaTel", 0
v_cirrus    db "Cirrus Logic", 0
v_intel     db "Intel", 0
v_ati       db "ATI/AMD", 0
v_nvidia    db "Nvidia", 0
v_via       db "VIA", 0
v_adi       db "Analog Devices", 0
v_qemu      db "QEMU", 0
v_amd       db "AMD", 0
v_cmedia    db "C-Media", 0
v_vmware    db "VMware", 0
v_zhaoxin   db "Zhaoxin", 0
; AX = fabricante -> imprime « (nombre)» si se conoce
pvendor:
        push si
        mov si, ven_tab
.l:
        cmp word [si], 0
        je .r
        cmp ax, [si]
        je .f
        add si, 4
        jmp .l
.f:
        PR " ("
        mov si, [si+2]
        call pstr
        PR ")"
.r:
        pop si
        ret

; ---- volcado del códec [cad] ----
dump_codec:
        mov byte [codec_dead], 0
        mov byte [fails], 0
        xor bl, bl
        xor cl, cl
        call hda_param
        mov [dc_id], eax
        MODE O_SCR | O_SUM
        PR "  HD Audio "
        mov bx, [cur_bdf]
        call pbdf
        PR ", códec "
        mov al, [cad]
        call phex1
        PR ": "
        mov eax, [dc_id]
        call phex8
        shr eax, 16
        call pvendor
        PR NL
        MODE O_REP
        PR NL, "--- Códec "
        mov al, [cad]
        call phex1
        PR " de "
        mov bx, [cur_bdf]
        call pbdf
        PR " ---", NL, "Identificador (fabricante:dispositivo) "
        mov eax, [dc_id]
        shr eax, 16
        call phex4
        PR ":"
        mov ax, [dc_id]
        call phex4
        mov ax, [dc_id+2]
        call pvendor
        PR "  revisión "
        xor bl, bl
        mov cl, 0x02
        call hda_param
        call phex8
        PR "h", NL
        ; grupos de función
        xor bl, bl
        mov cl, 0x04
        call hda_param
        mov ch, al
        shr eax, 16
        mov bl, al
.fg:
        test ch, ch
        jz .fge
        PR "Grupo de función "
        mov al, bl
        call phex2
        PR "h: tipo "
        mov cl, 0x05
        call hda_param
        call phex2
        PR "h"
        cmp al, 1
        jne .f2
        PR " (audio)"
.f2:
        cmp al, 2
        jne .f3
        PR " (módem)"
.f3:
        PR NL
        inc bl
        dec ch
        jmp .fg
.fge:
        call codec_afg
        jnc .afg
        PR "Sin grupo de función de audio.", NL
        ret
.afg:
        mov bl, [afg]
        PR "Grupo de audio "
        mov al, bl
        call phex2
        PR "h: subsistema "
        mov ax, 0xF20
        xor cl, cl
        call hda_verb
        call phex8
        PR "h  capacidades "
        mov cl, 0x08
        call hda_param
        call phex8
        PR "h", NL, "  PCM "
        mov cl, 0x0A
        call hda_param
        call ppcm
        PR NL, "  Formatos de flujo "
        mov cl, 0x0B
        call hda_param
        call phex8
        PR "h  estados de energía "
        mov cl, 0x0F
        call hda_param
        call phex8
        PR "h  energía ahora "
        mov ax, 0xF05
        xor cl, cl
        call hda_verb
        call phex8
        PR "h", NL, "  Amplificador de entrada por defecto: "
        mov cl, 0x0D
        call hda_param
        call pampcaps
        PR NL, "  Amplificador de salida por defecto: "
        mov cl, 0x12
        call hda_param
        call pampcaps
        PR NL, "  GPIO "
        mov cl, 0x11
        call hda_param
        mov [dc_gp], eax
        call phex8
        PR "h: "
        movzx eax, byte [dc_gp]
        call pdec
        PR " GPIO, "
        movzx eax, byte [dc_gp+1]
        call pdec
        PR " GPO, "
        movzx eax, byte [dc_gp+2]
        call pdec
        PR " GPI", NL
        ; estado de las GPIO (y se guarda: es «como lo dejó la BIOS» si el
        ; enlace no se ha reiniciado)
        PR "  GPIO ahora: máscara "
        mov ax, 0xF16
        xor cl, cl
        call hda_verb
        mov [dc_gm], al
        call phex2
        PR "h dirección "
        mov ax, 0xF17
        call hda_verb
        mov [dc_gd], al
        call phex2
        PR "h datos "
        mov ax, 0xF15
        call hda_verb
        mov [dc_gv], al
        call phex2
        PR "h  (despertar "
        mov ax, 0xF18
        call hda_verb
        call phex2
        PR "h, no pedidas "
        mov ax, 0xF19
        call hda_verb
        call phex2
        PR "h, fijadas "
        mov ax, 0xF1A
        call hda_verb
        call phex2
        PR "h)", NL
        movzx si, byte [n_gsave]
        cmp si, 12
        jae .ngs
        imul si, si, 5
        add si, gsave
        mov al, [cur_ctl]
        mov [si], al
        mov al, [cad]
        mov [si+1], al
        mov al, [dc_gm]
        mov [si+2], al
        mov al, [dc_gd]
        mov [si+3], al
        mov al, [dc_gv]
        mov [si+4], al
        inc byte [n_gsave]
.ngs:
        ; despertar el grupo (D0) y recorrer los nodos
        mov bl, [afg]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        mov cx, 1
        call wait_ticks
        PR "Nodos: del "
        mov al, [w_first]
        call phex2
        PR "h, "
        movzx eax, byte [w_count]
        call pdec
        PR " en total", NL
        mov bl, [w_first]
        movzx cx, byte [w_count]
        jcxz .r
.w:
        push cx
        push bx
        call dump_widget
        pop bx
        pop cx
        cmp byte [codec_dead], 0
        jne .r
        inc bl
        loop .w
.r:
        ret

; EAX = capacidades de amplificador
pampcaps:
        push eax
        push ebx
        mov ebx, eax
        call phex8
        PR "h (0 dB en el paso "
        mov eax, ebx
        and eax, 0x7F
        call pdec
        PR ", "
        mov eax, ebx
        shr eax, 8
        and eax, 0x7F
        call pdec
        PR " pasos de "
        mov eax, ebx
        shr eax, 16
        and eax, 0x7F
        inc eax
        call pdec
        PR "/4 dB, silencio "
        mov eax, ebx
        shr eax, 31
        call psino
        PR ")"
        pop ebx
        pop eax
        ret

; EAX = parámetro 0Ah (PCM): frecuencias y tamaños
ppcm:
        push eax
        push ebx
        push cx
        push si
        mov ebx, eax
        call phex8
        PR "h:"
        mov si, t_rates
        xor cl, cl
.r:
        bt ebx, ecx
        jnc .rn
        PR " "
        mov al, cl
        call ptab
.rn:
        inc cl
        cmp cl, 12
        jb .r
        PR " Hz; bits:"
        mov si, t_bitsz
        mov cl, 16
.b:
        bt ebx, ecx
        jnc .bn
        PR " "
        mov al, cl
        sub al, 16
        call ptab
.bn:
        inc cl
        cmp cl, 21
        jb .b
        pop si
        pop cx
        pop ebx
        pop eax
        ret
t_rates     db "8000", 0, "11025", 0, "16000", 0, "22050", 0, "32000", 0, "44100", 0
            db "48000", 0, "88200", 0, "96000", 0, "176400", 0, "192000", 0, "384000", 0
t_bitsz     db "8", 0, "16", 0, "20", 0, "24", 0, "32", 0

t_wtypes    db "Salida de audio (DAC)", 0, "Entrada de audio (ADC)", 0, "Mezclador", 0
            db "Selector", 0, "Pin", 0, "Energía", 0, "Mando de volumen", 0
            db "Generador de pitido", 0, "tipo 8", 0, "tipo 9", 0, "tipo A", 0, "tipo B", 0
            db "tipo C", 0, "tipo D", 0, "tipo E", 0, "Del fabricante", 0
t_wflags    db "estéreo", 0, "amp-entrada", 0, "amp-salida", 0, "amp-propio", 0
            db "formato-propio", 0, "stripe", 0, "proceso", 0, "no-pedidas", 0
            db "lista-conexiones", 0, "digital", 0, "energía", 0, "izq-der-cambiados", 0
            db "protección", 0
t_pcaps     db "mide-impedancia", 0, "hay-que-disparar", 0, "DETECTA-PRESENCIA", 0
            db "AURICULARES", 0, "SALIDA", 0, "entrada", 0, "balanceado", 0, "HDMI", 0
t_conn      db "conector externo", 0, "SIN CONEXION", 0, "fijo (interno)", 0
            db "conector y fijo", 0
t_dev       db "salida de línea", 0, "ALTAVOZ", 0, "AURICULARES", 0, "CD", 0
            db "salida SPDIF", 0, "salida digital (HDMI...)", 0, "línea de módem", 0
            db "auricular de módem", 0, "entrada de línea", 0, "AUX", 0, "micrófono", 0
            db "telefonía", 0, "entrada SPDIF", 0, "entrada digital", 0, "reservado", 0
            db "otro", 0
t_loc1      db "exterior", 0, "interior", 0, "aparte", 0, "otro sitio", 0
t_loc2      db "", 0, " detrás", 0, " delante", 0, " izquierda", 0, " derecha", 0
            db " arriba", 0, " abajo", 0, " especial 7", 0, " especial 8", 0
            db " especial 9", 0, " A", 0, " B", 0, " C", 0, " D", 0, " E", 0, " F", 0
t_ctype     db "desconocido", 0, "jack 3,5 mm", 0, "jack 6,35 mm", 0, "ATAPI interno", 0
            db "RCA", 0, "óptico", 0, "otro digital", 0, "otro analógico", 0, "DIN", 0
            db "XLR", 0, "RJ-11", 0, "combinado", 0, "C", 0, "D", 0, "E", 0, "otro", 0
t_color     db "desconocido", 0, "negro", 0, "gris", 0, "azul", 0, "verde", 0, "rojo", 0
            db "naranja", 0, "amarillo", 0, "morado", 0, "rosa", 0, "A", 0, "B", 0, "C", 0
            db "D", 0, "blanco", 0, "otro", 0

; ---- volcado del nodo BL ----
dump_widget:
        mov [dw_nid], bl
        call w_type
        mov [dw_type], al
        PR "Nodo "
        mov al, bl
        call phex2
        PR "h ["
        mov al, [dw_type]
        and al, 15
        mov si, t_wtypes
        call ptab
        PR "] caps "
        mov eax, [wcaps]
        call phex8
        PR "h:"
        mov si, t_wflags
        xor cl, cl
.fl:
        movzx ecx, cl
        bt dword [wcaps], ecx
        jnc .fn
        PR " "
        mov al, cl
        call ptab
.fn:
        inc cl
        cmp cl, 13
        jb .fl
        PR NL
        ; conexiones (se necesitan ya para los amplificadores de entrada)
        mov word [dw_nc], 0
        test byte [wcaps+1], 0x01
        jz .nconn
        mov di, dw_conn
        call get_conn
        mov [dw_nc], cx
.nconn:
        ; convertidores
        cmp byte [dw_type], 1
        ja .amps
        PR "  Convertidor: flujo/canal "
        mov ax, 0xF06
        xor cl, cl
        call hda_verb
        call phex2
        PR "h  formato "
        mov al, 0x0A
        xor cx, cx
        call hda_verb4
        call phex4
        PR "h"
        test byte [wcaps+1], 0x02
        jz .cnd
        PR "  digital (F0Dh) "
        mov ax, 0xF0D
        xor cl, cl
        call hda_verb
        call phex4
        PR "h"
.cnd:
        PR NL
        test byte [wcaps], 0x10
        jz .amps
        PR "  PCM "
        mov cl, 0x0A
        call hda_param
        call ppcm
        PR "  formatos "
        mov cl, 0x0B
        call hda_param
        call phex8
        PR "h", NL
.amps:
        test byte [wcaps], 0x02
        jz .oamp
        PR "  Amp. entrada: "
        mov cl, 0x0D
        call amp_caps
        call pampcaps
        PR NL, "    ahora (izq/der por entrada):"
        mov cx, [dw_nc]
        test cx, cx
        jnz .ia
        inc cx
.ia:
        cmp cx, 16
        jbe .ib
        mov cx, 16
.ib:
        xor dx, dx
.il:
        PR " "
        push cx
        mov cx, dx
        or ch, 0x20
        mov al, 0x0B
        call hda_verb4
        call phex2
        PR "/"
        mov cx, dx
        mov al, 0x0B
        call hda_verb4
        call phex2
        pop cx
        inc dx
        loop .il
        PR NL
.oamp:
        test byte [wcaps], 0x04
        jz .pin
        PR "  Amp. salida: "
        mov cl, 0x12
        call amp_caps
        call pampcaps
        PR "  ahora izq/der "
        mov al, 0x0B
        mov cx, 0xA000
        call hda_verb4
        call phex2
        PR "/"
        mov al, 0x0B
        mov cx, 0x8000
        call hda_verb4
        call phex2
        PR NL
.pin:
        cmp byte [dw_type], 4
        jne .pwr
        mov cl, 0x0C
        call hda_param
        mov [dw_pc], eax
        PR "  Capacidades del pin "
        call phex8
        PR "h:"
        mov si, t_pcaps
        xor cl, cl
.pc:
        movzx ecx, cl
        bt dword [dw_pc], ecx
        jnc .pn
        PR " "
        mov al, cl
        call ptab
.pn:
        inc cl
        cmp cl, 8
        jb .pc
        test byte [dw_pc+2], 0x01
        jz .pe
        PR " EAPD"
.pe:
        test byte [dw_pc+3], 0x01
        jz .pd
        PR " DisplayPort"
.pd:
        PR NL
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        mov [dw_cfg], eax
        PR "  Configuración por defecto "
        call phex8
        PR "h: "
        call pdefcfg
        PR NL, "  Control del pin "
        mov ax, 0xF07
        xor cl, cl
        call hda_verb
        call phex2
        PR "h"
        test al, 0x40
        jz .c1
        PR " (salida)"
.c1:
        test al, 0x80
        jz .c2
        PR " (amp. auriculares)"
.c2:
        test al, 0x20
        jz .c3
        PR " (entrada)"
.c3:
        test byte [dw_pc+2], 0x01
        jz .c4
        PR "  EAPD/BTL "
        mov ax, 0xF0C
        xor cl, cl
        call hda_verb
        call phex2
        PR "h"
        test al, 2
        jz .c4
        PR " (EAPD encendido)"
.c4:
        test byte [dw_pc], 0x04
        jz .c5
        PR "  presencia "
        call pin_sense
        call phex8
        PR "h"
        test eax, 0x80000000
        jz .c4n
        PR " (ENCHUFADO)"
        jmp .c5
.c4n:
        PR " (nada enchufado)"
.c5:
        PR NL
.pwr:
        test byte [wcaps+1], 0x04
        jz .knob
        PR "  Energía "
        mov ax, 0xF05
        xor cl, cl
        call hda_verb
        call phex8
        PR "h", NL
.knob:
        ; coeficientes de proceso (los de Realtek van en su nodo «del
        ; fabricante»): se leen sin cambiarlos
        test byte [wcaps], 0x40
        jz .knob2
        PR "  Proceso: capacidades "
        mov cl, 0x10
        call hda_param
        call phex8
        PR "h  estado "
        mov edx, eax
        mov ax, 0xF03
        xor cl, cl
        call hda_verb
        call phex2
        PR "h", NL
        shr dx, 8
        and dx, 0xFF                ; número de coeficientes
        jz .knob2
        cmp dx, 128
        jbe .cf
        mov dx, 128
.cf:
        mov al, 0x0D                ; índice actual (se repone al acabar)
        xor cx, cx
        call hda_verb4
        mov [dw_ci], ax
        PR "  Coeficientes:"
        xor si, si
.cfl:
        test si, 7
        jnz .cf1
        PR NL, "   "
        mov ax, si
        call phex2
        PR ":"
.cf1:
        mov al, 0x05
        mov cx, si
        call hda_verb4
        mov al, 0x0C
        xor cx, cx
        call hda_verb4
        PR " "
        call phex4
        inc si
        cmp byte [codec_dead], 0
        jne .cfe
        cmp si, dx
        jb .cfl
.cfe:
        PR NL
        mov al, 0x05
        mov cx, [dw_ci]
        call hda_verb4
.knob2:
        cmp byte [dw_type], 6
        jne .conn
        PR "  Mando de volumen: capacidades "
        mov cl, 0x13
        call hda_param
        call phex8
        PR "h  valor "
        mov ax, 0xF0F
        xor cl, cl
        call hda_verb
        call phex2
        PR "h", NL
.conn:
        mov cx, [dw_nc]
        jcxz .r
        PR "  Conexiones ("
        movzx eax, cx
        call pdec
        PR "):"
        mov dl, 0xFF
        cmp byte [dw_type], 2       ; el mezclador no elige
        je .cl
        mov ax, 0xF01
        push cx
        xor cl, cl
        call hda_verb
        pop cx
        mov dl, al
.cl:
        mov si, dw_conn
        xor dh, dh
.ce:
        PR " "
        lodsb
        call phex2
        PR "h"
        cmp dh, dl
        jne .cn
        PR "*"
.cn:
        inc dh
        loop .ce
        PR NL
.r:
        ret

; presencia del pin BL ([dw_pc] = sus capacidades) -> EAX (bit 31 = enchufado)
pin_sense:
        test byte [dw_pc], 0x02     ; hay que disparar la medida
        jz .rd
        mov ax, 0x709
        xor cl, cl
        call hda_verb
        mov cx, 1
        call wait_ticks
.rd:
        mov ax, 0xF09
        xor cl, cl
        jmp hda_verb

; [dw_cfg] descifrada
pdefcfg:
        push si
        mov eax, [dw_cfg]
        shr eax, 30
        mov si, t_conn
        call ptab
        PR " | "
        mov eax, [dw_cfg]
        shr eax, 20
        and al, 15
        mov si, t_dev
        call ptab
        PR " | "
        mov eax, [dw_cfg]
        shr eax, 28
        and al, 3
        mov si, t_loc1
        call ptab
        mov eax, [dw_cfg]
        shr eax, 24
        and al, 15
        mov si, t_loc2
        call ptab
        PR " | conector "
        mov eax, [dw_cfg]
        shr eax, 16
        and al, 15
        mov si, t_ctype
        call ptab
        PR " | color "
        mov eax, [dw_cfg]
        shr eax, 12
        and al, 15
        mov si, t_color
        call ptab
        PR " | asociación "
        mov eax, [dw_cfg]
        shr al, 4
        call phex1
        PR " secuencia "
        mov eax, [dw_cfg]
        call phex1
        test byte [dw_cfg+1], 0x01
        jz .r
        PR " | sin detección de presencia"
.r:
        pop si
        ret

; =============================================================================
;  Pruebas con preguntas
; =============================================================================
AMP_STD     equ 12000

; ---- altavoz del PC: prueba de control ----
test_pcspk:
        MODE O_SCR
        PR NL, "Primero, el altavoz interno del PC (un pitido).", NL
        mov word [replay], pc_beep
        call pc_beep
        mov si, q_beep
        mov di, l_beep
        call askrec
        ret
pc_beep:
        push ax
        mov al, 0xB6                ; temporizador 2, onda cuadrada
        out 0x43, al
        mov ax, 1356                ; 1193182 / 880 Hz
        out 0x42, al
        mov al, ah
        out 0x42, al
        in al, 0x61
        or al, 3
        out 0x61, al
        mov ax, 500
        call wait_ms
        in al, 0x61
        and al, 0xFC
        out 0x61, al
        pop ax
        ret

; SI = pregunta, DI = etiqueta para el resumen -> como ask
askrec:
        call ask
        pushf
        push ax
        mov ah, [omode]
        push ax
        MODE O_SUM
        push si
        mov si, di
        call pstr
        pop si
        pop ax
        push ax
        cmp al, 'S'
        jne .n
        PR ": sí", NL
        jmp .r
.n:
        PR ": NO", NL
.r:
        pop ax
        mov [omode], ah
        pop ax
        popf
        ret

; ---- HD Audio ----
hda_tests:
        cmp byte [n_hda], 0
        je .r
        cmp byte [v86], 0
        je .go
        MODE O_SCR | O_REP | O_SUM
        PR "HD Audio: no se puede probar en modo V86 (arranque sin HSPEMM).", NL
.r:
        ret
.go:
        MODE O_SCR
        PR NL, "Ahora, la tarjeta de sonido del equipo (HD Audio).", NL
        mov byte [pass], 0
        call .sweep
        cmp byte [found], 0
        je .dig
        mov byte [found_an], 1
        call post_tests
.dig:
        cmp byte [quick], 0         ; la versión corta no prueba las digitales
        jne .r
        ; salidas digitales (HDMI, DisplayPort, SPDIF) con algo enchufado
        mov byte [found], 0
        mov byte [pass], 1
        call .sweep
        ret
.sweep:
        xor si, si
        mov byte [hda_idx], 0xFF
.l:
        mov ax, si
        cmp al, [nctl]
        jae .sr
        push si
        mov [cur_ctl], al
        shl si, 2
        cmp byte [ctl_tab+si+2], 3
        jne .n
        inc byte [hda_idx]
        call sweep_ctl
.n:
        pop si
        cmp byte [found], 0
        jne .sr
        inc si
        jmp .l
.sr:
        ret

sweep_ctl:
        MODE O_REP
        PR NL, "-- Barrido de HD Audio "
        movzx si, byte [cur_ctl]
        shl si, 2
        mov bx, [ctl_tab+si]
        call pbdf
        cmp byte [pass], 0
        jne .p1
        PR " (salidas analógicas) --", NL
        jmp .o
.p1:
        PR " (salidas digitales con algo enchufado) --", NL
.o:
        mov byte [do_reset], 1
        mov byte [verbose], 0
        call hda_open
        jc .r
        ; AMD/ATI: bit de «snoop» del registro 42h
        mov byte [sn_can], 0
        mov ax, [cur_ven]
        cmp ax, 0x1002
        je .amd
        cmp ax, 0x1022
        jne .cads
.amd:
        mov bx, [cur_bdf]
        mov di, 0x42
        call pci_rd8
        mov [sn_orig], al
        and al, 7
        cmp al, 2
        je .cads
        mov byte [sn_can], 1
.cads:
        cmp byte [sim], 0
        je .ns
        mov bx, [cur_bdf]
        mov di, 0x42
        call pci_rd8
        mov [sn_orig], al
        mov byte [sn_can], 1
.ns:
        xor cl, cl
.cad:
        mov ax, [statests]
        shr ax, cl
        test al, 1
        jz .cn
        mov [cad], cl
        push cx
        call sweep_codec
        pop cx
        cmp byte [found], 0
        jne .r
.cn:
        inc cl
        cmp cl, 15
        jb .cad
.r:
        ret

; lista de pines que pueden sacar señal en el códec, con su clase:
; 0 altavoz interno, 1 altavoz, 2 auriculares, 3 línea, 4 otros, 5 digital
build_cands:
        mov byte [ncand], 0
        mov byte [any_eapd], 0
        mov bl, [w_first]
        movzx cx, byte [w_count]
        test cx, cx
        jz .r
.w:
        push cx
        call w_type
        mov [bc_dig], ah
        test byte [wcaps+1], 0x04   ; con control de energía: a D0
        jz .np
        push ax
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        pop ax
.np:
        cmp al, 4
        jne .n
        mov cl, 0x0C
        call hda_param
        mov [dw_pc], eax
        test al, 0x10
        jz .n
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        mov [dw_cfg], eax
        mov dl, [bc_dig]
        test byte [dw_pc], 0x04
        jz .f1
        test byte [dw_cfg+1], 0x01
        jnz .f1
        or dl, 2
        call pin_sense
        test eax, 0x80000000
        jz .f1
        or dl, 4
.f1:
        test byte [dw_pc+2], 0x01
        jz .f2
        or dl, 8
        mov byte [any_eapd], 1
.f2:
        test byte [dw_pc], 0x08
        jz .f3
        or dl, 0x10
.f3:
        test byte [dw_pc], 0x80
        jnz .f4
        test byte [dw_pc+3], 0x01
        jz .f5
.f4:
        or dl, 0x20
.f5:
        mov eax, [dw_cfg]
        shr eax, 30
        mov dh, 4
        cmp al, 1
        jne .c0
        or dl, 0x40                 ; «sin conexión»
        jmp .cls
.c0:
        cmp dword [dw_cfg], 0
        je .cls
        mov ah, al                  ; conexión
        mov ecx, [dw_cfg]
        shr ecx, 20
        and cl, 15                  ; dispositivo
        cmp cl, 1
        jne .c1
        mov dh, 1
        cmp ah, 2
        jb .cls
        mov dh, 0
        jmp .cls
.c1:
        cmp cl, 2
        jne .c2
        mov dh, 2
        jmp .cls
.c2:
        test cl, cl
        jnz .cls
        mov dh, 3
.cls:
        test dl, 1
        jz .st
        mov dh, 5
.st:
        movzx si, byte [ncand]
        cmp si, NCAND
        jae .n
        shl si, 2
        mov [cands+si], bl
        mov [cands+si+1], dh
        mov [cands+si+2], dl
        inc byte [ncand]
.n:
        pop cx
        inc bl
        dec cx
        jnz .w
.r:
        ret

sweep_codec:
        call codec_afg
        jc .r0
        mov bl, [afg]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        mov cx, 2
        call wait_ticks
        call build_cands
        cmp byte [sim], 0
        je .nsim
        cmp byte [ncand], 0
        je .nsim
        mov byte [g_n], 3
        mov byte [g_all], 7
        mov byte [any_eapd], 1
        or byte [cands+2], 0x18
        movzx si, byte [ncand]
        shl si, 2
        mov al, [cands]
        mov [cands+si], al
        mov byte [cands+si+1], 2
        mov byte [cands+si+2], 0x12
        mov [cands+si+4], al        ; y otra «sin declarar»
        mov byte [cands+si+5], 4
        mov byte [cands+si+6], 0x40
        add byte [ncand], 2
.nsim:
        ; ¿se guardó cómo dejó la BIOS las GPIO de este códec?
        mov byte [gs_have], 0
        mov si, gsave
        movzx cx, byte [n_gsave]
        jcxz .gs
.gl:
        mov al, [cur_ctl]
        cmp al, [si]
        jne .gn
        mov al, [cad]
        cmp al, [si+1]
        jne .gn
        mov ax, [si+2]
        mov [gs_m], ax              ; máscara y dirección
        mov al, [si+4]
        mov [gs_v], al
        mov byte [gs_have], 1
        jmp .gs
.gn:
        add si, 5
        loop .gl
.gs:
        MODE O_REP
        PR "Códec "
        mov al, [cad]
        call phex1
        PR ": "
        movzx eax, byte [ncand]
        call pdec
        PR " pines con salida; GPIO: "
        movzx eax, byte [g_n]
        call pdec
        PR NL
        mov byte [first], 1
        mov byte [tried], 0
        mov byte [cls], 0
        cmp byte [pass], 0
        je .cl
        mov byte [cls], 5
.cl:
        xor di, di
.pl:
        mov ax, di
        cmp al, [ncand]
        jae .cnx
        mov si, di
        shl si, 2
        add si, cands
        mov al, [si+1]
        cmp al, [cls]
        jne .pn
        mov al, [si+2]
        ; con detección y nada enchufado: no se pregunta (salvo altavoces)
        cmp byte [cls], 2
        jb .do
        mov ah, al
        and ah, 6
        cmp ah, 2
        je .skip
        mov ah, al
        and ah, 0x41                ; digital y sin conexión
        cmp ah, 0x41
        je .skip
.do:
        mov ax, di
        mov [c_ci], al
        push di
        call sweep_pin
        pop di
        cmp byte [found], 0
        jne .r0
        jmp .pn
.skip:
        MODE O_REP
        PR "Pin "
        mov al, [si]
        call phex2
        PR "h: nada enchufado o sin conexión; no se prueba ahora.", NL
.pn:
        inc di
        jmp .pl
.cnx:
        inc byte [cls]
        cmp byte [pass], 0
        jne .r0
        cmp byte [cls], 4
        jb .cl
        ; último recurso: todo abierto a la vez (los pines de la clase «otros»
        ; solo se prueban uno a uno si así suena: para saber cuál es)
        call combo_base
        mov byte [c_brute], 1
        mov byte [c_ci], 0xFF
        mov byte [c_pin], 0
        mov byte [c_flags], 0
        mov byte [c_cls], 4
        mov byte [c_hp], 0
        MODE O_SCR
        PR "  (todas las salidas a la vez)", NL
        call try
        je .loc
        cmp byte [tried], 0
        je .r0
        inc byte [an_prev]
        call lrn_nf
        MODE O_REP | O_SUM
        PR "NO suena: HD Audio "
        mov bx, [cur_bdf]
        call pbdf
        PR " códec "
        mov al, [cad]
        call phex1
        PR " ("
        movzx eax, byte [tried]
        call pdec
        PR " intentos)", NL
.r0:
        ret
.loc:
        MODE O_SCR
        PR "  Suena. Ahora, salida por salida, para saber cuál es.", NL
        MODE O_REP
        PR "Suena con todo abierto: se prueban una a una las salidas sin declarar.", NL
        mov byte [first], 0
        mov byte [found], 0
        mov byte [after_br], 1
        xor di, di
.ll:
        mov ax, di
        cmp al, [ncand]
        jae .ld
        mov si, di
        shl si, 2
        add si, cands
        cmp byte [si+1], 4
        jne .ln
        mov [c_ci], al
        push di
        call sweep_pin
        pop di
        cmp byte [found], 0
        jne .r0
.ln:
        inc di
        jmp .ll
.ld:
        ; ninguna por separado: se vuelve a «todo abierto»
        mov byte [found], 1
        mov byte [after_br], 0
        mov si, w_blk
        mov di, c_blk
        mov cx, C_SIZE
        push ds
        pop es
        rep movsb
        call combo_apply
        MODE O_REP | O_SUM
        PR "  (ninguna salida sin declarar suena por separado: se sigue con todo abierto)", NL
        ret

; valores de partida de un intento: como lo hace HSPSND sin opciones
combo_base:
        mov byte [c_eapd], 1
        mov byte [c_gmode], 1
        mov al, [g_all]
        mov [c_gdata], al
        mov byte [c_ri], RI_48K
        xor al, al
        mov [c_alt], al
        mov [c_snoop], al
        mov [c_brute], al
        ret

; SI -> candidato. Prueba el pin: la primera vez en el códec, con todas las
; variantes; después, solo la de partida.
sweep_pin:
        mov al, [si]
        mov [c_pin], al
        mov al, [si+1]
        mov [c_cls], al
        mov al, [si+2]
        mov [c_flags], al
        call combo_base
        mov al, [c_flags]
        shr al, 4
        and al, 1
        mov [c_hp], al
        MODE O_SCR
        PR "  Salida: "
        mov al, [c_cls]
        mov si, t_cls
        call ptab
        PR NL
        cmp byte [c_cls], 5
        jne .t0
        PR "  (sonaría por la pantalla o el equipo conectado a esa salida)", NL
.t0:
        call try
        je .found
        cmp byte [no_path], 0
        jne .nf
        cmp byte [first], 0
        je .nf
        mov byte [first], 0
        cmp byte [c_cls], 5
        je .nf
        ; AMD/ATI: «snoop»
        cmp byte [sn_can], 0
        je .gpio
        mov byte [c_snoop], 1
        call try
        je .found
        mov byte [c_snoop], 0
.gpio:
        cmp byte [g_n], 0
        je .eapd
        cmp byte [gs_have], 0
        je .g0
        mov byte [c_gmode], 0       ; como las dejó la BIOS
        call try
        je .found
        mov byte [c_gmode], 1
.g0:
        mov byte [c_gdata], 0
        call try
        je .found
        cmp byte [g_n], 1
        jbe .ge
        xor cl, cl
.gb:
        mov al, 1
        shl al, cl
        mov [c_gdata], al
        push cx
        call try
        pop cx
        je .found
        inc cl
        cmp cl, [g_n]
        jb .gb
.ge:
        mov al, [g_all]
        mov [c_gdata], al
.eapd:
        cmp byte [any_eapd], 0
        je .r44
        mov byte [c_eapd], 0
        call try
        je .found
        cmp byte [g_n], 0
        je .e1
        cmp byte [gs_have], 0
        je .e1
        mov byte [c_gmode], 0
        call try
        je .found
        mov byte [c_gmode], 1
.e1:
        mov byte [c_eapd], 1
.r44:
        cmp byte [c_ri], RI_48K
        jne .alt
        test byte [dac_pcm], 0x20
        jz .alt
        mov byte [c_ri], RI_44K
        call try
        je .found
        mov byte [c_ri], RI_48K
.alt:
        ; otra ruta (otro DAC u otro mezclador), si la hay
        mov byte [c_alt], 1
        call try
        je .found
        mov byte [c_alt], 0
        cmp byte [c_hp], 0
        je .nf
        mov byte [c_hp], 0          ; sin el amplificador de auriculares
        call try
        je .found
.nf:
        or al, 1
        ret
.found:
        ret
t_cls       db "altavoz interno", 0, "altavoz", 0, "auriculares", 0
            db "salida de línea", 0, "otra salida", 0
            db "salida digital (HDMI, DisplayPort o SPDIF)", 0

; un intento: aplica la combinación, toca 1 s y pregunta -> ZF = 1 si suena
try:
        call combo_apply
        MODE O_REP
        PR "Intento: "
        call combo_print
        PR NL
        cmp byte [no_path], 0
        je .play
        PR "  (por esa ruta no se llega a ningún DAC: no se prueba)", NL
        or al, 1
        ret
.play:
        inc byte [tried]
        ; los amplificadores que se han abierto (nodo, salida o entrada, paso)
        cmp byte [pamps_n], 0
        je .na
        PR "  Amplificadores a 0 dB y sin silencio:"
        mov si, pamps
        movzx cx, byte [pamps_n]
.al:
        PR " "
        mov al, [si]
        call phex2
        test byte [si+1], 0x80
        jnz .ao
        PR "h/entrada "
        mov al, [si+1]
        call phex1
        jmp .av
.ao:
        PR "h/salida"
.av:
        PR "=paso "
        movzx eax, byte [si+2]
        call pdec
        add si, 6
        loop .al
        PR NL
.na:
        mov al, [c_ri]
        mov [t_ri], al
        mov byte [t_bits], 16
        mov byte [t_ch], 3
        mov word [t_amp], AMP_STD
        call tone_std
        call plpib
        mov word [replay], tone_std
        mov si, q_hear
        call ask
        jne .n
        mov byte [found], 1
        cmp byte [pass], 0
        je .an
        MODE O_REP | O_SUM
        PR "SUENA (salida digital; HSPSND todavía no las maneja): "
        call combo_print
        PR NL
        xor ax, ax
        ret
.an:
        mov si, c_blk
        mov di, w_blk
        mov cx, C_SIZE
        push ds
        pop es
        rep movsb
        mov al, [pio_ok]
        mov [w_pio], al
        mov al, [an_prev]           ; códecs con salida analógica que no
        mov [w_idx], al             ; sonaron antes que este
        mov al, [g_n]
        mov [w_gn], al
        mov al, [g_all]
        mov [w_gall], al
        mov al, [dac_pcm]
        mov [w_pcm], al
        call lrn_take
        MODE O_REP | O_SUM
        PR "SUENA: "
        call combo_print
        PR NL
        xor ax, ax
.n:
        ret

tone_std:
        mov word [t_ms], 1000
        jmp hda_tone
tone_long:
        mov word [t_ms], 5000
        jmp hda_tone
gap:
        push ax
        mov ax, 300
        call wait_ms
        pop ax
        ret

plpib:
        MODE O_REP
        cmp byte [lpib_ok], 0
        je .bad
        PR "  (la posición del flujo avanza)", NL
        ret
.bad:
        mov byte [lpib_bad], 1
        PR "  (LA POSICION DEL FLUJO NO AVANZA: la controladora no está tocando)", NL
        ret

; la combinación actual, en una línea
combo_print:
        PR "HD Audio "
        mov bx, [cur_bdf]
        call pbdf
        PR " códec "
        mov al, [cad]
        call phex1
        cmp byte [c_brute], 0
        je .pin
        PR ", TODO ABIERTO (todas las salidas, todos los DAC, EAPD encendido)"
        jmp .gp
.pin:
        PR ", pin "
        mov al, [c_pin]
        call phex2
        PR "h ("
        mov al, [c_cls]
        mov si, t_cls
        call ptab
        PR ")"
        cmp byte [no_path], 0
        jne .gp
        PR ", ruta"
        xor si, si
.pp:
        PR " "
        mov al, [path+si]
        call phex2
        PR "h"
        inc si
        mov ax, si
        cmp al, [plen]
        jb .pp
        PR " (el último es el DAC), control del pin "
        mov al, [pinctl]
        call phex2
        PR "h, EAPD "
        test byte [c_flags], 8
        jnz .e1
        cmp byte [any_eapd], 0
        jne .e1
        PR "no hay"
        jmp .gp
.e1:
        cmp byte [c_eapd], 0
        je .e0
        PR "encendido"
        jmp .gp
.e0:
        PR "APAGADO"
.gp:
        PR ", GPIO "
        cmp byte [g_n], 0
        jne .g1
        PR "no hay"
        jmp .fm
.g1:
        cmp byte [c_gmode], 0
        jne .g2
        PR "como la BIOS: "
.g2:
        PR "máscara "
        mov al, [g_em]
        call phex2
        PR "h dirección "
        mov al, [g_ed]
        call phex2
        PR "h datos "
        mov al, [g_ev]
        call phex2
        PR "h"
.fm:
        PR ", "
        mov al, [c_ri]
        mov [t_ri], al
        call rate_ptr
        movzx eax, word [si]
        call pdec
        PR " Hz 16 bits estéreo"
        cmp byte [c_snoop], 0
        je .a
        PR ", «snoop» activado (PCI 42h)"
.a:
        cmp byte [c_alt], 0
        je .r
        PR ", RUTA ALTERNATIVA"
.r:
        ret

; ---- aplicar la combinación al códec ----
combo_apply:
        mov byte [no_path], 0
        ; GPIO
        cmp byte [g_n], 0
        je .ng
        mov al, [g_all]
        mov ah, al
        mov dl, [c_gdata]
        cmp byte [c_gmode], 0
        jne .gw
        mov ax, [gs_m]
        mov dl, [gs_v]
.gw:
        mov [g_em], al
        mov [g_ed], ah
        mov [g_ev], dl
        mov bl, [afg]
        mov cl, [g_em]
        mov ax, 0x716
        call hda_verb
        mov cl, [g_ed]
        mov ax, 0x717
        call hda_verb
        mov cl, [g_ev]
        mov ax, 0x715
        call hda_verb
.ng:
        ; AMD/ATI: «snoop»
        cmp byte [sn_can], 0
        je .ns
        mov al, [sn_orig]
        cmp byte [c_snoop], 0
        je .sw
        and al, 0xF8
        or al, 2
.sw:
        mov bx, [cur_bdf]
        mov di, 0x42
        call pci_wr8
.ns:
        xor eax, eax
        mov [dacs], eax
        mov [dacs+4], eax
        mov [dac_pcm], eax
        mov byte [pamps_n], 0
        cmp byte [c_brute], 0
        jne brute
        ; las demás salidas, apagadas
        xor si, si
.off:
        mov ax, si
        cmp al, [ncand]
        jae .route
        mov di, si
        shl di, 2
        mov bl, [cands+di]
        cmp bl, [c_pin]
        je .on
        mov ax, 0xF07
        xor cl, cl
        call hda_verb
        and al, 0x3F
        mov cl, al
        mov ax, 0x707
        call hda_verb
.on:
        inc si
        jmp .off
.route:
        mov al, [c_flags]
        and al, 1
        mov [want_dig], al
        mov al, [c_alt]
        mov [alt_k], al
        mov al, [c_hp]
        shl al, 7
        or al, 0x40
        mov [pinctl], al
        mov bl, [c_pin]
        call route_pin
        jnc .ok
        mov byte [no_path], 1
        ret
.ok:
        call eapd_all
        ; el DAC no admite 48 kHz pero sí 44,1
        cmp byte [c_ri], RI_48K
        jne .dg
        mov al, [dac_pcm]
        and al, 0x60
        cmp al, 0x20
        jne .dg
        mov byte [c_ri], RI_44K
.dg:
        test byte [c_flags], 1
        jz .r
        call dig_setup
.r:
        ret

; EAPD según [c_eapd] en todos los pines que lo tienen
eapd_all:
        xor si, si
.l:
        mov ax, si
        cmp al, [ncand]
        jae .r
        mov di, si
        shl di, 2
        test byte [cands+di+2], 8
        jz .n
        mov bl, [cands+di]
        mov ax, 0xF0C
        xor cl, cl
        call hda_verb
        and al, 0x05
        cmp byte [c_eapd], 0
        je .w
        or al, 0x02
.w:
        mov cl, al
        mov ax, 0x70C
        call hda_verb
.n:
        inc si
        jmp .l
.r:
        ret

; salida digital: activar el convertidor; HDMI: dos canales y la trama de
; información de audio (CEA-861) por el pin
dig_setup:
        mov bl, [p_dac]
        mov ax, 0xF0D
        xor cl, cl
        call hda_verb
        or al, 1
        mov cl, al
        mov ax, 0x70D
        call hda_verb
        test byte [c_flags], 0x20
        jz .r
        mov ax, 0x72D
        mov cl, 1
        call hda_verb
        mov bl, [c_pin]
        mov ax, 0x730
        xor cl, cl
        call hda_verb
        mov si, hdmi_if
        mov dx, 14
.b:
        lodsb
        mov cl, al
        mov ax, 0x731
        call hda_verb
        dec dx
        jnz .b
        mov ax, 0x732
        mov cl, 0xC0
        call hda_verb
.r:
        ret
hdmi_if     db 0x84, 0x01, 0x0A, 0x70, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0

; ---- camino del pin BL hasta un DAC y puesta en marcha ----
; [want_dig], [alt_k] (0: la primera ruta; 1: la segunda...), [pinctl]
; -> CF si no hay ruta; [path], [plen], [p_dac]
route_pin:
        mov [path], bl
        mov byte [lvl], 0
        mov al, [alt_k]
        mov [skipn], al
        call find_path
        jc .r
        movzx si, byte [plen]
        mov bl, [path+si-1]
        mov [p_dac], bl
.k:
        dec si
        js .done
        mov bl, [path+si]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        call w_type
        mov [cur_type], al
        movzx ax, byte [plen]
        dec ax
        cmp si, ax
        jae .out                    ; el DAC no tiene entradas
        cmp byte [cur_type], 2      ; el mezclador no elige: mezcla
        je .inamp
        mov cl, [pidx+si]
        mov ax, 0x701
        call hda_verb
.inamp:
        cmp byte [cur_type], 4
        je .out
        test byte [wcaps], 0x02
        jz .out
        mov dl, [pidx+si]
        cmp dl, 15
        ja .out
        call amp_open
.out:
        test byte [wcaps], 0x04
        jz .pin
        mov dl, 0x80
        call amp_open
.pin:
        cmp byte [cur_type], 4
        jne .dac
        mov cl, [pinctl]
        mov ax, 0x707
        call hda_verb
        jmp .k
.dac:
        cmp byte [cur_type], 0
        jne .k
        call dac_add
        jmp .k
.done:
        clc
.r:
        ret

; busca desde path[lvl] la ruta número [skipn] hasta un DAC ([want_dig])
find_path:
        push si
        push di
        push dx
        movzx di, byte [lvl]
        mov bl, [path+di]
        imul di, di, CONNMAX
        add di, conn_bufs
        call get_conn
        mov dx, cx
        xor si, si
.d:
        cmp si, dx
        jae .m0
        mov bx, di
        mov bl, [bx+si]
        call w_type
        test al, al
        jnz .dn
        cmp ah, [want_dig]
        jne .dn
        cmp byte [skipn], 0
        je .take
        dec byte [skipn]
.dn:
        inc si
        jmp .d
.take:
        call .set
        mov al, [lvl]
        add al, 2
        mov [plen], al
        clc
        jmp .out
.m0:
        cmp byte [lvl], 3
        jae .nf
        xor si, si
.m:
        cmp si, dx
        jae .nf
        mov bx, di
        mov bl, [bx+si]
        call w_type
        cmp al, 2
        je .rec
        cmp al, 3
        jne .mn
.rec:
        call .set
        inc byte [lvl]
        call find_path
        dec byte [lvl]              ; (no toca CF)
        jnc .out
.mn:
        inc si
        jmp .m
.nf:
        stc
.out:
        pop dx
        pop di
        pop si
        ret
.set:
        push di
        movzx di, byte [lvl]
        mov [path+di+1], bl
        mov ax, si
        mov [pidx+di], al
        pop di
        ret

; BL = DAC ([wcaps] al día): apuntarlo, flujo 1 y frecuencias que admite
dac_add:
        push si
        mov si, dacs
.l:
        cmp [si], bl
        je .have
        cmp byte [si], 0
        je .new
        inc si
        cmp si, dacs + 7
        jb .l
        jmp .have
.new:
        mov [si], bl
.have:
        mov ax, 0x706
        mov cl, 0x10
        call hda_verb
        xor eax, eax
        test byte [wcaps], 0x10
        jz .afg
        mov cl, 0x0A
        call hda_param
        test eax, eax
        jnz .st
.afg:
        push bx
        mov bl, [afg]
        mov cl, 0x0A
        call hda_param
        pop bx
.st:
        test eax, eax
        jnz .st2
        mov eax, 0x00020060         ; no lo dice: 44,1 y 48 kHz, 16 bits
.st2:
        mov [dac_pcm], eax
        pop si
        ret

; amplificador del nodo BL (DL = 80h salida, o el índice de la entrada): a
; 0 dB y sin silencio; se apunta en pamps (para la prueba de volumen)
amp_open:
        push si
        push eax
        mov cl, 0x12
        test dl, 0x80
        jnz .c
        mov cl, 0x0D
.c:
        call amp_caps
        movzx si, byte [pamps_n]
        cmp si, 8
        jae .w
        imul si, si, 6
        add si, pamps
        mov [si], bl
        mov [si+1], dl
        mov cl, al
        and cl, 0x7F
        mov [si+2], cl              ; paso de 0 dB
        mov cl, ah
        and cl, 0x7F
        mov [si+3], cl              ; pasos
        push eax
        shr eax, 16
        mov cl, al
        and cl, 0x7F
        inc cl
        mov [si+4], cl              ; tamaño del paso (cuartos de dB)
        shr ax, 15
        mov [si+5], al              ; tiene silencio
        pop eax
        inc byte [pamps_n]
.w:
        and al, 0x7F
        call amp_wr
        pop eax
        pop si
        ret
; BL = nodo, DL = 80h salida / índice de entrada, AL = ganancia (bit 7: mudo)
amp_wr:
        push cx
        push eax
        movzx cx, al
        test dl, 0x80
        jz .in
        or cx, 0xB000               ; salida, izquierdo y derecho
        jmp .go
.in:
        mov ch, dl
        and ch, 0x0F
        or ch, 0x70                 ; entrada n, izquierdo y derecho
.go:
        mov al, 3
        call hda_verb4
        pop eax
        pop cx
        ret

; ---- último recurso: todo abierto ----
brute:
        mov bl, [w_first]
        movzx cx, byte [w_count]
        test cx, cx
        jz .r
.w:
        push cx
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        call w_type
        mov [cur_type], al
        test al, al
        jnz .mix
        test ah, ah
        jnz .n
        call dac_add
        jmp .oa
.mix:
        cmp al, 2
        je .m
        cmp al, 3
        jne .pin
.m:
        test byte [wcaps], 0x02
        jz .oa
        ; entradas que vienen de un DAC, un mezclador o un selector (las de
        ; micrófono se dejan: acoplarían)
        mov di, conn_bufs
        call get_conn
        xor si, si
.mi:
        cmp si, cx
        jae .mo
        cmp si, 16
        jae .mo
        push bx
        mov bl, [conn_bufs+si]
        call w_type
        pop bx
        test al, al
        jz .mu
        cmp al, 2
        je .mu
        cmp al, 3
        jne .mn
.mu:
        call w_type
        mov dx, si
        push cx
        call amp_open
        pop cx
.mn:
        inc si
        jmp .mi
.mo:
        call w_type
        jmp .oa
.pin:
        cmp al, 4
        jne .n
        test ah, ah
        jnz .n
        mov cl, 0x0C
        call hda_param
        test al, 0x10
        jz .n
        mov edx, eax
        mov ax, 0xF07
        xor cl, cl
        call hda_verb
        or al, 0x40
        test dl, 0x08
        jz .p1
        or al, 0x80
.p1:
        mov cl, al
        mov ax, 0x707
        call hda_verb
        test edx, 0x10000
        jz .oa
        mov ax, 0xF0C
        xor cl, cl
        call hda_verb
        and al, 5
        or al, 2
        mov cl, al
        mov ax, 0x70C
        call hda_verb
.oa:
        test byte [wcaps], 0x04
        jz .n
        mov dl, 0x80
        call amp_open
.n:
        pop cx
        inc bl
        dec cx
        jnz .w
.r:
        mov byte [pamps_n], 0       ; (el volumen, por programa)
        cmp byte [dacs], 0
        jne .r2
        mov byte [no_path], 1       ; ni un DAC analógico (códec de HDMI)
.r2:
        ret

; =============================================================================
;  Pruebas con el camino que suena
; =============================================================================
post_tests:
        MODE O_SCR
        PR NL, "Bien. Ahora unas pruebas más con ese sonido.", NL
        MODE O_REP
        PR NL, "-- Pruebas con el camino que suena --", NL
        ; amplificador que hará de volumen: el primero regulable desde el DAC
        mov word [va], 0
        mov si, pamps
        movzx cx, byte [pamps_n]
        jcxz .nova
.fa:
        cmp byte [si+3], 0
        jne .gotva
        add si, 6
        loop .fa
.nova:
        mov byte [w_vsoft], 1
        PR "Volumen: el camino no tiene amplificador regulable; se prueba por programa", NL
        PR "  (bajando la amplitud de las muestras), como haría HSPSND.", NL
        jmp .v1
.gotva:
        mov [va], si
        mov ax, [si]
        mov [w_vamp], ax            ; (nodo y salida/entrada, para SONIDO.INI)
        PR "Volumen: amplificador del nodo "
        mov al, [si]
        call phex2
        PR "h ("
        test byte [si+1], 0x80
        jz .vin
        PR "salida"
        jmp .vp
.vin:
        PR "entrada "
        mov al, [si+1]
        call phex1
.vp:
        PR "), 0 dB en el paso "
        movzx eax, byte [si+2]
        call pdec
        PR " de "
        movzx eax, byte [si+3]
        call pdec
        PR ", silencio "
        mov al, [si+5]
        call psino
        PR NL
.v1:
        MODE O_SCR
        PR NL, "Volumen: sonará dos veces seguidas.", NL
        mov word [replay], rv_low
        call rv_low
        mov si, q_low
        mov di, l_low
        call askrec
        je .vok
        mov byte [w_vsoft], 1       ; no baja: en SONIDO.INI, «por programa»
.vok:
        cmp byte [quick], 0         ; versión corta: el volumen que baja y los
        jne .ch                     ; dos canales, nada más
        mov word [replay], rv_max
        call rv_max
        mov si, q_max
        mov di, l_max
        call askrec
        ; silencio (solo si el amplificador lo tiene)
        mov si, [va]
        test si, si
        jz .ch
        cmp byte [si+5], 0
        je .ch
        MODE O_SCR
        PR NL, "Silencio: sonará una vez y después habrá un segundo de silencio.", NL
        mov word [replay], rv_mute
        call rv_mute
        mov si, q_mute
        mov di, l_mute
        call askrec
.ch:
        MODE O_SCR
        PR NL, "Canales: izquierda y derecha.", NL
        mov word [replay], tone_std
        mov byte [t_ch], 1
        call tone_std
        mov si, q_left
        mov di, l_left
        call askrec
        mov byte [t_ch], 2
        call tone_std
        mov si, q_right
        mov di, l_right
        call askrec
        mov byte [t_ch], 3
        cmp byte [quick], 0
        je .rates
        ret
.rates:
        ; frecuencias de muestreo
        MODE O_SCR
        PR NL, "Frecuencias: cada vez sonarán dos tonos seguidos.", NL
        mov bx, rt_list
.rt:
        mov al, [bx]
        cmp al, 0xFF
        je .b8
        cmp al, [w_blk + c_ri - c_blk]
        je .rn
        mov [test_ri], al
        mov [t_ri], al
        call rate_ptr
        mov ax, [si+4]              ; bit del parámetro 0Ah
        test [dac_pcm], ax
        jnz .rs
        MODE O_REP | O_SUM
        PR "  "
        movzx eax, word [si]
        call pdec
        PR " Hz: el DAC no la admite (HSPSND convierte la frecuencia)", NL
        jmp .rn
.rs:
        MODE O_REP
        PR "Frecuencia "
        movzx eax, word [si]
        call pdec
        PR " Hz frente a la del camino:", NL
        push bx
        mov word [replay], rv_rate
        call rv_rate
        mov si, q_same
        mov di, [bx+1]
        call askrec
        pop bx
.rn:
        add bx, 3
        jmp .rt
.b8:
        mov al, [w_blk + c_ri - c_blk]
        mov [t_ri], al
        test byte [dac_pcm+2], 0x01
        jnz .b8s
        MODE O_REP | O_SUM
        PR "  8 bits: el DAC no los admite (es lo normal; HSPSND convierte a 16)", NL
        jmp .long
.b8s:
        MODE O_REP
        PR "8 bits frente a 16 bits:", NL
        mov word [replay], rv_8
        call rv_8
        mov si, q_same
        mov di, l_8
        call askrec
.long:
        MODE O_SCR
        PR NL, "Ahora un sonido largo, de 5 segundos.", NL
        mov word [replay], tone_long
        call tone_long
        mov si, q_long
        mov di, l_long
        call askrec
        call hp_test
        ret
; frecuencia y etiqueta del resumen
rt_list     db RI_8K
            dw l_r8
            db RI_11K
            dw l_r11
            db RI_22K
            dw l_r22
            db RI_44K
            dw l_r44
            db RI_48K
            dw l_r48
            db 0xFF

; volumen: AL = paso (bit 7: mudo) al amplificador [va]
va_set:
        push si
        push bx
        push dx
        mov si, [va]
        mov bl, [si]
        mov dl, [si+1]
        call amp_wr
        pop dx
        pop bx
        pop si
        ret
vol_low:
        mov si, [va]
        test si, si
        jz .soft
        ; unos 20 dB menos
        mov ax, 80
        div byte [si+4]
        mov ah, [si+2]
        sub ah, al
        jnc .w
        xor ah, ah
.w:
        cmp ah, [si+2]
        je .soft
        mov al, ah
        jmp va_set
.soft:
        mov word [t_amp], AMP_STD / 10
        ret
vol_max:
        mov word [t_amp], AMP_STD
        mov si, [va]
        test si, si
        jz .soft
        mov al, [si+3]
        jmp va_set
.soft:
        mov word [t_amp], 30000
        ret
vol_norm:
        mov word [t_amp], AMP_STD
        mov si, [va]
        test si, si
        jz .r
        mov al, [si+2]
        jmp va_set
.r:
        ret
rv_low:
        call tone_std
        call gap
        call vol_low
        call tone_std
        jmp vol_norm
rv_max:
        call vol_low
        call tone_std
        call gap
        call vol_max
        call tone_std
        jmp vol_norm
rv_mute:
        call tone_std
        mov si, [va]
        mov al, [si+2]
        or al, 0x80
        call va_set
        call tone_std
        jmp vol_norm
rv_rate:
        mov al, [w_blk + c_ri - c_blk]
        mov [t_ri], al
        call tone_std
        call gap
        mov al, [test_ri]
        mov [t_ri], al
        call tone_std
        mov al, [w_blk + c_ri - c_blk]
        mov [t_ri], al
        ret
rv_8:
        call tone_std
        call gap
        mov byte [t_bits], 8
        call tone_std
        mov byte [t_bits], 16
        ret

; ---- auriculares ----
; BL = pin -> AL = 1 si su detección dice «enchufado»
cand_sense:
        mov cl, 0x0C
        call hda_param
        mov [dw_pc], eax
        call pin_sense
        shr eax, 31
        ret
; presencia de todos los candidatos -> tabla DI
hp_snap:
        xor si, si
.l:
        mov ax, si
        cmp al, [ncand]
        jae .r
        mov bx, si
        shl bx, 2
        xor al, al
        test byte [cands+bx+2], 2
        jz .s
        mov bl, [cands+bx]
        call cand_sense
.s:
        mov [di], al
        inc di
        inc si
        jmp .l
.r:
        ret

hp_test:
        ; ¿hay salidas de auriculares o de línea, aparte de la que suena?
        xor si, si
        xor dx, dx
.c:
        mov ax, si
        cmp al, [ncand]
        jae .cd
        mov bx, si
        shl bx, 2
        mov ax, si
        cmp al, [w_blk + c_ci - c_blk]
        je .cn
        mov al, [cands+bx+1]
        cmp al, 2
        jb .cn
        cmp al, 3
        ja .cn
        inc dx
.cn:
        inc si
        jmp .c
.cd:
        test dx, dx
        jnz .have
        MODE O_REP | O_SUM
        PR "Auriculares: el códec no declara otra salida de auriculares o de línea", NL
        ret
.have:
        mov di, pres0
        call hp_snap
        MODE O_SCR
        PR NL, "Auriculares.", NL
        mov word [replay], 0
        mov si, q_plug
        call ask
        je .yes
        MODE O_REP | O_SUM
        PR "Auriculares: no se han probado (no se enchufaron)", NL
        ret
.yes:
        mov cx, 9
        call wait_ticks
        mov di, pres1
        call hp_snap
        ; lo que dice la detección
        MODE O_REP
        mov byte [hp_chg], 0
        xor si, si
.d:
        mov ax, si
        cmp al, [ncand]
        jae .dd
        mov bx, si
        shl bx, 2
        test byte [cands+bx+2], 2
        jz .dn
        PR "  Pin "
        mov al, [cands+bx]
        call phex2
        PR "h: presencia antes "
        mov al, [pres0+si]
        call phex1
        PR ", después "
        mov al, [pres1+si]
        call phex1
        PR NL
        cmp al, [pres0+si]
        je .dn
        mov byte [hp_chg], 1
.dn:
        inc si
        jmp .d
.dd:
        MODE O_REP | O_SUM
        cmp byte [hp_chg], 0
        je .nc
        PR "Detección de auriculares: funciona (cambia al enchufar)", NL
        jmp .try
.nc:
        PR "Detección de auriculares: NO cambia al enchufar", NL
.try:
        ; primero los que han cambiado; luego auriculares; luego línea
        mov byte [hp_pass], 0
        mov byte [hp_n], 0
.ps:
        xor si, si
.pl:
        mov ax, si
        cmp al, [ncand]
        jae .pe
        mov bx, si
        shl bx, 2
        mov ax, si
        cmp al, [w_blk + c_ci - c_blk]
        je .pn
        mov al, [pres1+si]
        sub al, [pres0+si]          ; 1: se acaba de enchufar
        mov ah, [cands+bx+1]
        cmp byte [hp_pass], 0
        jne .p1
        cmp al, 1
        jne .pn
        cmp ah, 4
        ja .pn
        jmp .go
.p1:
        cmp al, 1
        je .pn
        mov al, [hp_pass]
        inc al                      ; pasada 1: clase 2; pasada 2: clase 3
        cmp ah, al
        jne .pn
.go:
        cmp byte [hp_n], 3
        jae .fail
        inc byte [hp_n]
        push si
        call hp_try
        pop si
        je .r
.pn:
        inc si
        jmp .pl
.pe:
        inc byte [hp_pass]
        cmp byte [hp_pass], 3
        jb .ps
.fail:
        MODE O_REP | O_SUM
        PR "Auriculares: NO suenan por ninguna salida probada", NL
.r:
        ret

; BX*1 = índice*4 del candidato: se le da camino y se pregunta -> ZF si suena
hp_try:
        mov al, [cands+bx]
        mov [hp_pin], al
        mov byte [want_dig], 0
        mov byte [alt_k], 0
        mov al, 0x40
        test byte [cands+bx+2], 0x10
        jz .pc
        mov al, 0xC0
.pc:
        mov [pinctl], al
        mov bl, [hp_pin]
        call route_pin
        jnc .ok
        or al, 1
        ret
.ok:
        MODE O_REP
        PR "Auriculares: pin "
        mov al, [hp_pin]
        call phex2
        PR "h, ruta"
        xor si, si
.pp:
        PR " "
        mov al, [path+si]
        call phex2
        PR "h"
        inc si
        mov ax, si
        cmp al, [plen]
        jb .pp
        PR ", control del pin "
        mov al, [pinctl]
        call phex2
        PR "h", NL
        mov word [replay], tone_std
        call tone_std
        mov si, q_hp
        call ask
        je .snd
        ; no suena: se apaga ese pin
        mov bl, [hp_pin]
        mov ax, 0x707
        xor cl, cl
        call hda_verb
        or al, 1
        ret
.snd:
        ; para SONIDO.INI: la ruta y el control del pin de los auriculares
        push si
        mov si, path
        mov di, w_hpath
        mov cx, 16
        push ds
        pop es
        rep movsb
        pop si
        mov al, [plen]
        mov [w_hplen], al
        mov al, [pinctl]
        mov [w_hctl], al
        mov byte [w_hp], 1
        MODE O_REP | O_SUM
        PR "Auriculares: suenan por el pin "
        mov al, [hp_pin]
        call phex2
        PR "h", NL
        ; ¿y el altavoz? se apaga su pin
        mov bl, [w_blk + c_pin - c_blk]
        test bl, bl
        jz .z
        mov ax, 0x707
        xor cl, cl
        call hda_verb
        MODE O_REP
        PR "Altavoz: control del pin "
        mov al, bl
        call phex2
        PR "h puesto a 00h", NL
        call tone_std
        mov si, q_spkoff
        mov di, l_spkoff
        call askrec
        mov bl, [w_blk + c_pin - c_blk]
        mov cl, [w_blk + c_hp - c_blk]
        shl cl, 7
        or cl, 0x40
        mov ax, 0x707
        call hda_verb
.z:
        xor ax, ax
        ret

; =============================================================================
;  AC'97 (controladoras compatibles con Intel ICH)
; =============================================================================
ac97_collect:
        mov bx, [cur_bdf]
        mov [ac_bdf], bx
        MODE O_REP
        PR NL, "=== AC'97 "
        mov bx, [cur_bdf]
        call pbdf
        PR " ===", NL
        xor di, di
        call pci_rd32
        cmp ax, 0x1106
        je .other
        cmp ax, 0x1039
        jne .ich
.other:
        PR "VIA o SiS: sus registros no son los de Intel ICH; no se prueba.", NL
        ret
.ich:
        mov di, 0x10
        call pci_rd32
        test al, 1
        jz .nobar
        and ax, 0xFFFC
        jz .nobar
        mov [nam], ax
        mov di, 0x14
        call pci_rd32
        test al, 1
        jz .nobar
        and ax, 0xFFFC
        jz .nobar
        mov [nabm], ax
        jmp .bars
.nobar:
        mov word [nabm], 0
        PR "Sin puertos de E/S asignados (BAR0/BAR1): no se puede probar.", NL
        ret
.bars:
        mov di, 0x04
        call pci_rd16
        or ax, 0x0005
        call pci_wr16
        ; reinicio en frío y espera al códec (hasta 1 s)
        mov dx, [nabm]
        add dx, 0x2C
        xor eax, eax
        out dx, eax
        call delay
        mov eax, 2
        out dx, eax
        mov cx, 20
.w:
        mov dx, [nabm]
        add dx, 0x30
        in eax, dx
        test ax, 0x0100
        jnz .ready
        call wait_tick1
        loop .w
        PR "El códec no se presenta (estado global "
        call phex8
        PR "h): no se puede probar.", NL
        mov word [nabm], 0
        ret
.ready:
        PR "Mezclador en "
        mov ax, [nam]
        call phex4
        PR "h, bus master en "
        mov ax, [nabm]
        call phex4
        PR "h, estado global "
        mov dx, [nabm]
        add dx, 0x30
        in eax, dx
        call phex8
        PR "h", NL, "Códec: "
        mov dx, [nam]
        add dx, 0x7C
        in ax, dx
        mov bx, ax
        call phex4
        PR ":"
        mov dx, [nam]
        add dx, 0x7E
        in ax, dx
        call phex4
        mov cx, ax
        PR " ("
        mov al, bh
        call .chr
        mov al, bl
        call .chr
        mov al, ch
        call .chr
        PR ")", NL, "Registros del mezclador (00h-7Eh):", NL
        MODE O_SCR
        PR "  AC'97 "
        push bx
        mov bx, [cur_bdf]
        call pbdf
        pop bx
        PR ", códec "
        mov ax, bx
        call phex4
        PR ":"
        mov ax, cx
        call phex4
        PR NL
        MODE O_REP
        xor bx, bx
.row:
        PR "  "
        mov al, bl
        call phex2
        PR ":"
.col:
        PR " "
        mov dx, [nam]
        add dx, bx
        in ax, dx
        call phex4
        add bx, 2
        test bl, 15
        jnz .col
        PR NL
        cmp bx, 0x80
        jb .row
        ; abrir volúmenes: maestro, auriculares y PCM
        mov dx, [nam]
        add dx, 0x02
        xor ax, ax
        out dx, ax
        mov dx, [nam]
        add dx, 0x04
        out dx, ax
        mov dx, [nam]
        add dx, 0x18
        mov ax, 0x0808
        out dx, ax
        ; frecuencia variable: 48000 Hz
        mov dx, [nam]
        add dx, 0x28
        in ax, dx
        test al, 1
        jz .r
        mov dx, [nam]
        add dx, 0x2A
        in ax, dx
        or al, 1
        out dx, ax
        mov dx, [nam]
        add dx, 0x2C
        mov ax, 48000
        out dx, ax
.r:
        ret
.chr:
        cmp al, ' '
        jb .cq
        cmp al, 126
        jbe .cp
.cq:
        mov al, '?'
.cp:
        jmp pchar

; tono por la salida PCM (caja 10h del bus master) -> [lpib_ok]
ac_tone:
        pushad
        mov word [t_off], D_ABUF
        mov byte [t_ri], RI_48K
        mov byte [t_bits], 16
        call gen_tone
        mov dx, [nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
        mov al, 2
        out dx, al
        mov cx, 0x2000
.rw:
        in al, dx
        test al, 2
        jz .rd
        loop .rw
.rd:
        ; 32 entradas iguales (6,4 s): no hace falta atender nada
        push es
        mov es, [dseg]
        mov di, D_BDL
        mov bx, D_ABUF
        call dphys
        mov ebx, eax
        movzx edx, word [t_bytes]
        shr edx, 1                  ; en muestras
        mov cx, 32
.e:
        mov eax, ebx
        stosd
        mov eax, edx
        stosd
        loop .e
        pop es
        mov bx, D_BDL
        call dphys
        mov dx, [nabm]
        add dx, 0x10
        out dx, eax
        mov dx, [nabm]
        add dx, 0x15
        mov al, 31
        out dx, al
        mov dx, [nabm]
        add dx, 0x16
        mov ax, 0x1C
        out dx, ax
        mov dx, [nabm]
        add dx, 0x1B
        mov al, 1
        out dx, al
        mov byte [lpib_ok], 0
        mov dx, [nabm]
        add dx, 0x18
        in ax, dx
        mov bp, ax
        mov ax, [t_ms]
        mov cx, 91
        mul cx
        mov cx, 5000
        div cx
        mov cx, ax
        inc cx
.wt:
        call wait_tick1
        mov dx, [nabm]
        add dx, 0x18
        in ax, dx
        cmp ax, bp
        je .same
        mov byte [lpib_ok], 1
.same:
        loop .wt
        mov dx, [nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
        mov al, 2
        out dx, al
        popad
        ret
ac_tone_std:
        mov word [t_ms], 1000
        jmp ac_tone

ac97_test:
        cmp word [nabm], 0
        je .r
        MODE O_SCR
        PR NL, "Ahora, la tarjeta de sonido AC'97.", NL
        MODE O_REP
        PR NL, "-- AC'97: tono de 48000 Hz, 16 bits, estéreo; maestro y auriculares a 0 dB --", NL
        mov byte [t_ch], 3
        mov word [t_amp], AMP_STD
        mov word [replay], ac_tone_std
        call ac_tone_std
        call plpib
        mov si, q_hear
        mov di, l_ac97
        call askrec
        mov byte [ac_res], 1
        je .r
        mov byte [ac_res], 3
        ; variante: EAPD al revés y las demás salidas abiertas
        mov dx, [nam]
        add dx, 0x26
        in ax, dx
        xor ax, 0x8000
        out dx, ax
        mov bx, ax
        xor ax, ax
        mov dx, [nam]
        add dx, 0x06
        out dx, ax
        mov dx, [nam]
        add dx, 0x36
        out dx, ax
        mov dx, [nam]
        add dx, 0x38
        out dx, ax
        MODE O_REP
        PR "Variante: EAPD invertido (registro 26h = "
        mov ax, bx
        call phex4
        PR "h) y salidas mono, central y traseras abiertas", NL
        call ac_tone_std
        mov si, q_hear
        mov di, l_ac97b
        call askrec
        jne .r
        mov byte [ac_res], 2
.r:
        ret

; =============================================================================
;  Sound Blaster ISA
; =============================================================================
; AL -> DSP (con plazo)
sb_wr:
        push cx
        push dx
        push ax
        mov dx, [sb_base]
        add dx, 0x0C
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .go
        loop .w
.go:
        pop ax
        out dx, al
        pop dx
        pop cx
        ret
; DSP -> AL (FFh si no llega nada)
sb_rd:
        push cx
        push dx
        mov dx, [sb_base]
        add dx, 0x0E
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .go
        loop .w
        mov al, 0xFF
        jmp .r
.go:
        mov dx, [sb_base]
        add dx, 0x0A
        in al, dx
.r:
        pop dx
        pop cx
        ret
; reinicio del DSP en [sb_base] -> CF si no contesta AAh
sb_reset:
        push ax
        push cx
        push dx
        mov dx, [sb_base]
        add dx, 6
        mov al, 1
        out dx, al
        mov cx, 16
.d:
        in al, 0x80
        loop .d
        xor al, al
        out dx, al
        mov dx, [sb_base]
        add dx, 0x0E
        mov cx, 0x4000
.w:
        in al, dx
        test al, 0x80
        jnz .got
        loop .w
        jmp .no
.got:
        mov dx, [sb_base]
        add dx, 0x0A
        in al, dx
        cmp al, 0xAA
        jne .no
        pop dx
        pop cx
        pop ax
        clc
        ret
.no:
        pop dx
        pop cx
        pop ax
        stc
        ret
; AL = registro del mezclador -> AL
sb_mixrd:
        push dx
        mov dx, [sb_base]
        add dx, 4
        out dx, al
        inc dx
        in al, dx
        pop dx
        ret
; AL = registro, AH = valor
sb_mixwr:
        push dx
        mov dx, [sb_base]
        add dx, 4
        out dx, al
        inc dx
        mov al, ah
        out dx, al
        pop dx
        ret

; variable BLASTER: A (puerto), I (IRQ), D (DMA)
sb_env:
        push es
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .r
        cmp dword [es:di], 'BLAS'
        jne .nv
        cmp dword [es:di+4], 'TER='
        je .f
.nv:
        inc di
        cmp byte [es:di-1], 0
        jne .nv
        jmp .v
.f:
        add di, 8
        mov byte [sb_haveenv], 1
.t:
        mov al, [es:di]
        test al, al
        jz .r
        inc di
        and al, 0xDF
        cmp al, 'A'
        je .a
        cmp al, 'I'
        je .i
        cmp al, 'D'
        jne .t
        call .num
        mov [sb_edma], bl
        jmp .t
.i:
        call .num
        mov [sb_eirq], bl
        jmp .t
.a:
        call .num
        mov [sb_ebase], bx
        jmp .t
.r:
        pop es
        ret
; número hexadecimal (vale para una cifra decimal) -> BX
.num:
        xor bx, bx
.n1:
        mov al, [es:di]
        sub al, '0'
        jb .ne
        cmp al, 9
        jbe .nd
        and al, 0xDF
        sub al, 7
        cmp al, 10
        jb .ne
        cmp al, 15
        ja .ne
.nd:
        shl bx, 4
        or bl, al
        inc di
        jmp .n1
.ne:
        ret

sb_collect:
        MODE O_REP
        PR NL, "=== SOUND BLASTER (ISA) ===", NL
        cmp byte [hsp_was], 0
        je .go
        cmp byte [hsp_type], 3
        jb .go
        PR "No se busca: HSPSND estaba cargado (podría ser su Sound Blaster emulada).", NL
        ret
.go:
        mov byte [sb_edma], 0xFF
        mov byte [sb_eirq], 0xFF
        call sb_env
        mov ax, [sb_ebase]
        test ax, ax
        jz .probe
        mov [sb_base], ax
        call sb_reset
        jnc .got
.probe:
        mov word [sb_base], 0x220
.p:
        call sb_reset
        jnc .got
        add word [sb_base], 0x20
        cmp word [sb_base], 0x280
        jbe .p
        mov word [sb_base], 0
        PR "No hay (sin respuesta del DSP en 220h, 240h, 260h ni 280h).", NL
        ret
.got:
        mov al, 0xE1
        call sb_wr
        call sb_rd
        mov [sb_major], al
        call sb_rd
        mov [sb_minor], al
        ; DMA de 8 bits: BLASTER, el mezclador de la SB16 o el 1
        mov al, [sb_edma]
        cmp al, 3
        jbe .dma
        mov al, 1
        cmp byte [sb_major], 4
        jb .dma
        mov al, 0x81
        call sb_mixrd
        mov ah, al
        mov al, 0
        test ah, 1
        jnz .dma
        mov al, 1
        test ah, 2
        jnz .dma
        mov al, 3
        test ah, 8
        jnz .dma
        mov al, 1
.dma:
        mov [sb_dma], al
        mov al, 3
        call .line
        MODE O_REP
        cmp byte [sb_haveenv], 0
        je .mx
        PR "Variable BLASTER: puerto "
        mov ax, [sb_ebase]
        call phex4
        PR "h, IRQ "
        mov al, [sb_eirq]
        call phex2
        PR "h, DMA "
        mov al, [sb_edma]
        call phex2
        PR "h (FFh = no lo dice)", NL
.mx:
        cmp byte [sb_major], 3
        jb .r
        PR "Mezclador:"
        mov si, sb_mixregs
.ml:
        lodsb
        cmp al, 0xFF
        je .me
        cmp al, 0x30
        jb .m1
        cmp byte [sb_major], 4
        jb .ml
.m1:
        PR " "
        call phex2
        PR "="
        call sb_mixrd
        call phex2
        jmp .ml
.me:
        PR NL
.r:
        ret
; la línea de identificación, a la pantalla y al informe
.line:
        MODE O_SCR | O_REP
        PR "  Sound Blaster en "
        mov ax, [sb_base]
        call phex4
        PR "h, DSP "
        movzx eax, byte [sb_major]
        call pdec
        PR "."
        mov al, [sb_minor]
        call pdec2
        PR ", DMA "
        mov al, [sb_dma]
        call phex1
        PR NL
        ret
sb_mixregs  db 0x04, 0x0A, 0x0C, 0x0E, 0x22, 0x26, 0x28, 0x2E
            db 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A
            db 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45
            db 0x46, 0x47, 0x80, 0x81, 0x82, 0xFF
sb_pages    db 0x87, 0x83, 0x81, 0x82

; 1 s de tono: 8 bits, DMA de ciclo único (orden 14h), sin usar la IRQ
SB_LEN      equ 22050
sb_tone:
        pushad
        ; el búfer no puede cruzar una frontera física de 64 KB
        mov bx, D_ABUF
        call dphys
        mov ebx, eax
        add ebx, SB_LEN - 1
        xor ebx, eax
        mov cx, D_ABUF
        test ebx, 0xFFFF0000
        jz .off
        neg ax                      ; lo que falta hasta la frontera
        add cx, ax
.off:
        mov [t_off], cx
        mov byte [t_ri], RI_11K
        mov byte [t_bits], 8
        mov byte [t_ch], 3
        mov word [t_amp], AMP_STD
        call gen_tone
        mov byte [t_bits], 16
        ; 0,2 s -> 1 s
        push ds
        push es
        mov es, [dseg]
        mov si, [t_off]
        mov di, si
        add di, SB_LEN / 5
        mov cx, SB_LEN / 5 * 4
        mov ds, [dseg]
        rep movsb
        pop es
        pop ds
        call sb_reset
        ; DMA
        movzx bx, byte [sb_dma]
        mov al, bl
        or al, 4
        out 0x0A, al
        xor al, al
        out 0x0C, al
        mov al, bl
        or al, 0x48
        out 0x0B, al
        push bx
        mov bx, [t_off]
        call dphys
        pop bx
        mov dx, bx
        shl dx, 1
        out dx, al
        mov al, ah
        out dx, al
        shr eax, 16
        push dx
        movzx dx, byte [sb_pages+bx]
        out dx, al
        pop dx
        inc dx
        mov al, (SB_LEN - 1) & 0xFF
        out dx, al
        mov al, (SB_LEN - 1) >> 8
        out dx, al
        mov al, bl
        out 0x0A, al
        ; DSP: altavoz, 22050 Hz (pares de muestras iguales) y a tocar
        mov al, 0xD1
        call sb_wr
        mov al, 0x40
        call sb_wr
        mov al, 211
        call sb_wr
        mov al, 0x14
        call sb_wr
        mov al, (SB_LEN - 1) & 0xFF
        call sb_wr
        mov al, (SB_LEN - 1) >> 8
        call sb_wr
        mov ax, 1150
        call wait_ms
        mov dx, [sb_base]
        add dx, 0x0E
        in al, dx
        mov al, [sb_dma]
        or al, 4
        out 0x0A, al
        call sb_reset
        popad
        ret

sb_test:
        cmp word [sb_base], 0
        je .r
        MODE O_SCR
        PR NL, "Ahora, la Sound Blaster.", NL
        MODE O_REP
        PR NL, "-- Sound Blaster: tono de 8 bits por DMA (orden 14h) --", NL
        mov word [replay], sb_tone
        call sb_tone
        mov si, q_hear
        mov di, l_sb
        call askrec
        mov byte [sb_res], 1
        je .r
        mov byte [sb_res], 0
        cmp byte [sb_major], 3
        jb .r
        ; variante: mezclador al máximo
        cmp byte [sb_major], 4
        jb .pro
        mov ax, 0xF830
        call sb_mixwr
        mov ax, 0xF831
        call sb_mixwr
        mov ax, 0xF832
        call sb_mixwr
        mov ax, 0xF833
        call sb_mixwr
        jmp .v
.pro:
        mov ax, 0xFF22
        call sb_mixwr
        mov ax, 0xFF04
        call sb_mixwr
.v:
        MODE O_REP
        PR "Variante: mezclador (maestro y voz) al máximo", NL
        call sb_tone
        mov si, q_hear
        mov di, l_sbb
        call askrec
        jne .r
        mov byte [sb_res], 1
.r:
        ret

; =============================================================================
;  SONIDO.INI: lo aprendido (ver docs/AUDICION.md, «Lo que aprende»)
; =============================================================================
; ---- identificadores de la tarjeta HD Audio en uso -> lr_rec ----
; (cur_bdf, cad, afg)
lrn_ids:
        pushad
        mov bx, [cur_bdf]
        mov [lr_rec], bx
        mov al, [cad]
        mov [lr_rec+2], al
        xor di, di
        call pci_rd32
        mov [lr_rec+4], eax
        mov di, 0x2C
        call pci_rd32
        mov [lr_rec+8], eax
        xor bl, bl
        xor cl, cl
        call hda_param
        jnc .c
        xor eax, eax
.c:
        mov [lr_rec+12], eax
        mov bl, [afg]
        mov ax, 0xF20
        xor cl, cl
        call hda_verb
        jnc .s
        xor eax, eax
.s:
        mov [lr_rec+16], eax
        popad
        ret

; ---- el camino que acaba de sonar (desde try) ----
lrn_take:
        pushad
        push es
        push ds
        pop es
        call lrn_ids
        mov si, lr_rec
        mov di, w_rec
        mov cx, REC
        rep movsb
        mov si, path                ; (path y pidx, seguidos)
        mov di, w_path
        mov cx, 16
        rep movsb
        mov al, [plen]
        mov [w_plen], al
        mov al, [pinctl]
        mov [w_ctl], al
        mov al, [g_em]
        mov [w_g], al
        mov al, [g_ed]
        mov [w_g+1], al
        mov al, [g_ev]
        mov [w_g+2], al
        mov al, [any_eapd]
        mov [w_eap], al
        mov byte [w_ok], 1
        mov byte [w_hp], 0
        mov byte [w_vsoft], 0
        mov word [w_vamp], 0
        pop es
        popad
        ret

; ---- un códec con el que no ha sonado nada (desde sweep_codec) ----
lrn_nf:
        pushad
        push es
        push ds
        pop es
        cmp byte [nf_n], NNF
        jae .r
        cmp byte [codec_dead], 0
        jne .r
        call lrn_ids
        cmp dword [lr_rec+12], 0
        je .r
        movzx di, byte [nf_n]
        imul di, di, REC
        add di, nf_tab
        mov si, lr_rec
        mov cx, REC
        rep movsb
        inc byte [nf_n]
.r:
        pop es
        popad
        ret

; ---- la cabecera de la sección: SI = registro, AL = 0 HD Audio, 1 AC'97 ----
p_hdr:
        push eax
        PR "["
        test al, al
        jnz .ac
        PR "HDA "
        jmp .ids
.ac:
        PR "AC97 "
.ids:
        push ax
        mov eax, [si+4]
        call p_lohi
        PR " "
        mov eax, [si+8]
        call p_lohi
        PR " "
        mov eax, [si+12]
        call p_hilo
        pop ax
        test al, al
        jnz .e
        PR " "
        mov eax, [si+16]
        call p_hilo
.e:
        PR "]"
        pop eax
        ret
; EAX -> «bajo:alto» (PCI: fabricante:dispositivo) o «alto:bajo» (códec)
p_lohi:
        push eax
        call phex4
        PR ":"
        shr eax, 16
        call phex4
        pop eax
        ret
p_hilo:
        push eax
        shr eax, 16
        call phex4
        PR ":"
        pop eax
        call phex4
        ret

; ---- la cabecera, en hdr_tab (para comparar con las del fichero viejo) ----
; SI = registro, AL = tipo
add_hdr:
        movzx bx, byte [nhdr]
        cmp bx, 6
        jae .r
        imul bx, bx, HDRSZ
        add bx, hdr_tab
        mov [capp], bx
        mov word [caplen], 0
        mov word [capmax], HDRSZ - 2
        mov byte [bx], 0
        mov ah, [omode]
        push ax
        MODE O_CAP
        call p_hdr
        pop ax
        mov [omode], ah
        mov word [capmax], 0
        inc byte [nhdr]
.r:
        ret

; ---- líneas comunes: Estado, Fecha, Equipo, Tipo, PCI... ----
; SI = registro, AL = tipo, DI = texto del estado
p_common:
        push ax
        PR "Estado="
        push si
        mov si, di
        call pstr
        pop si
        PR NL, "Fecha="
        push si
        mov si, date_txt
        call pstr
        PR NL, "Equipo="
        mov si, smb_eq
        cmp byte [si], 0
        jne .eq
        mov si, s_dash
.eq:
        call pstr
        pop si
        PR NL, "Tipo="
        pop ax
        push ax
        test al, al
        jnz .ac
        PR "HDA"
        jmp .t
.ac:
        PR "AC97"
.t:
        PR NL, "Controladora="
        mov eax, [si+4]
        call p_lohi
        PR NL, "Subsistema="
        mov eax, [si+8]
        call p_lohi
        PR NL
        PR "Posicion="
        mov bx, [si]
        call pbdf
        PR NL
        pop ax
        push ax
        test al, al
        jnz .ac2
        PR "Codec="
        mov al, [si+2]
        call phex1
        PR NL, "IdCodec="
        mov eax, [si+12]
        call p_hilo
        PR NL, "SubCodec="
        mov eax, [si+16]
        call p_hilo
        PR NL
        pop ax
        ret
.ac2:
        PR "IdCodec="
        mov eax, [si+12]
        call p_hilo
        PR NL
        pop ax
        ret

; una ruta: SI = nodos, DI = entradas elegidas, CL = cuántos nodos
p_route:
        push si
        push di
        push cx
        movzx cx, cl
        jcxz .r
.l:
        mov al, [si]
        call phex2
        PR "h"
        cmp cx, 1
        je .r
        PR "("
        movzx eax, byte [di]
        call pdec
        PR ") "
        inc si
        inc di
        loop .l
.r:
        pop cx
        pop di
        pop si
        ret

; ---- la sección del camino de HD Audio que ha sonado ----
w_sec_hda:
        mov si, w_rec
        xor al, al
        MODE O_INI
        PR NL
        call p_hdr
        PR NL, "; Prueba de sonido del "
        push si
        mov si, date_txt
        call pstr
        pop si
        mov di, st_suena
        cmp byte [w_blk + c_brute - c_blk], 0
        je .ok
        PR ": solo ha sonado con TODAS las salidas abiertas a la vez.", NL
        PR "; HSPSND no sabe hacerlo así: no usa esta sección y busca él solo.", NL
        mov di, st_todo
        xor al, al
        jmp p_common
.ok:
        PR ": SUENA.", NL
        cmp byte [after_br], 0
        je .nb
        PR "; AVISO: ese pin se encontró después de abrirlo todo.", NL
.nb:
        xor al, al
        call p_common
        PR "Altavoz="
        mov al, [w_path]
        call phex2
        PR "h ("
        mov al, [w_blk + c_cls - c_blk]
        mov si, t_cls
        call ptab
        PR ")", NL, "Ruta="
        mov si, w_path
        mov di, w_pidx
        mov cl, [w_plen]
        call p_route
        PR NL, "Control="
        mov al, [w_ctl]
        call phex2
        PR "h", NL
        cmp byte [w_hp], 0
        je .nhp
        PR "Auriculares="
        mov al, [w_hpath]
        call phex2
        PR "h", NL, "RutaAuriculares="
        mov si, w_hpath
        mov di, w_hpidx
        mov cl, [w_hplen]
        call p_route
        PR NL, "ControlAuriculares="
        mov al, [w_hctl]
        call phex2
        PR "h", NL
.nhp:
        PR "EAPD="
        cmp byte [w_eap], 0
        jne .e1
        PR "no hay"
        jmp .e2
.e1:
        cmp byte [w_blk + c_eapd - c_blk], 0
        je .e0
        PR "encendido"
        jmp .e2
.e0:
        PR "apagado"
.e2:
        PR NL, "GPIO="
        cmp byte [w_gn], 0
        jne .g1
        PR "no hay"
        jmp .g2
.g1:
        mov al, [w_g]
        call phex2
        PR "h "
        mov al, [w_g+1]
        call phex2
        PR "h "
        mov al, [w_g+2]
        call phex2
        PR "h"
.g2:
        PR NL, "Snoop="
        cmp byte [w_blk + c_snoop - c_blk], 0
        je .s0
        PR "activado"
        jmp .s1
.s0:
        PR "no"
.s1:
        PR NL, "Frecuencia="
        mov al, [w_blk + c_ri - c_blk]
        mov [t_ri], al
        call rate_ptr
        movzx eax, word [si]
        call pdec
        PR NL, "Formato=16 bits estéreo", NL
        PR "Volumen="
        cmp byte [w_vsoft], 0
        jne .vs
        cmp word [w_vamp], 0
        je .vs
        PR "amplificador "
        mov al, [w_vamp]
        call phex2
        PR "h "
        test byte [w_vamp+1], 0x80
        jz .vi
        PR "salida"
        jmp .vx
.vi:
        PR "entrada "
        mov al, [w_vamp+1]
        call phex1
        jmp .vx
.vs:
        PR "por programa"
.vx:
        PR NL
        ret

; ---- un códec de HD Audio con el que no ha sonado nada: SI = registro ----
w_sec_nf:
        xor al, al
        MODE O_INI
        PR NL
        call p_hdr
        PR NL, "; Prueba de sonido del "
        push si
        mov si, date_txt
        call pstr
        pop si
        PR ": con este códec NO ha sonado nada.", NL
        PR "; HSPSND no usa esta sección: busca la salida él solo.", NL
        mov di, st_nosuena
        xor al, al
        jmp p_common

; ---- AC'97 ----
w_sec_ac97:
        ; los identificadores (el códec sigue en marcha)
        mov bx, [ac_bdf]
        mov [lr_rec], bx
        xor di, di
        call pci_rd32
        mov [lr_rec+4], eax
        mov di, 0x2C
        call pci_rd32
        mov [lr_rec+8], eax
        mov dx, [nam]
        add dx, 0x7C
        in ax, dx
        shl eax, 16
        add dx, 2
        in ax, dx
        mov [lr_rec+12], eax
        cmp byte [ac_hdr], 0        ; (primera llamada: solo la cabecera)
        jne .w
        mov byte [ac_hdr], 1
        mov si, lr_rec
        mov al, 1
        jmp add_hdr
.w:
        mov si, lr_rec
        mov al, 1
        MODE O_INI
        PR NL
        call p_hdr
        PR NL, "; Prueba de sonido del "
        push si
        mov si, date_txt
        call pstr
        pop si
        mov di, st_suena
        cmp byte [ac_res], 3
        jne .s
        PR ": NO ha sonado.", NL
        PR "; HSPSND no usa esta sección.", NL
        mov di, st_nosuena
        mov al, 1
        jmp p_common
.s:
        PR ": SUENA.", NL
        mov al, 1
        call p_common
        PR "EAPD="
        cmp byte [ac_res], 2
        je .i
        PR "normal"
        jmp .f
.i:
        PR "invertido"
.f:
        PR NL, "Frecuencia=48000", NL, "Formato=16 bits estéreo", NL
        ret

; ---- escribir \HISPADOS\SONIDO.INI en la unidad de arranque ----
; Lo de otros equipos se deja tal cual; las secciones de este se quitan y
; van al final las nuevas.
ini_save:
        mov byte [ini_ok], 0
        mov al, [w_ok]
        or al, [nf_n]
        or al, [ac_res]
        jnz .go
        ret
.go:
        push ds
        pop es
        ; fecha y hora
        mov di, date_txt
        mov ah, 0x2A
        int 0x21
        mov al, dl
        call b_dec2
        mov al, '/'
        stosb
        mov al, dh
        call b_dec2
        mov al, '/'
        stosb
        mov ax, cx
        mov bl, 100
        div bl
        push ax
        call b_dec2
        pop ax
        mov al, ah
        call b_dec2
        mov al, ' '
        stosb
        mov ah, 0x2C
        int 0x21
        mov al, ch
        call b_dec2
        mov al, ':'
        stosb
        mov al, cl
        call b_dec2
        mov byte [di], 0
        ; las cabeceras de las secciones nuevas
        mov byte [nhdr], 0
        cmp byte [w_ok], 0
        je .h1
        mov si, w_rec
        xor al, al
        call add_hdr
.h1:
        xor cx, cx
.h2:
        cmp cl, [nf_n]
        jae .h3
        mov si, cx
        imul si, si, REC
        add si, nf_tab
        xor al, al
        push cx
        call add_hdr
        pop cx
        inc cx
        jmp .h2
.h3:
        mov byte [ac_hdr], 0
        cmp byte [ac_res], 0
        je .h4
        call w_sec_ac97
.h4:
        ; la línea «Equipo=» de este equipo
        push ds
        pop es
        mov di, eqline
        mov si, s_equipo
        call b_cpy
        mov si, smb_eq
        cmp byte [si], 0
        jne .hq
        mov si, s_dash
.hq:
        call b_cpy
        mov byte [di], 0
        ; el nombre: X:\HISPADOS\SONIDO.INI (o el de /INI:)
        cmp byte [has_ini], 0
        jne .dr2
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        cmp dl, 'A'
        jae .dr
        mov ah, 0x19
        int 0x21
        mov dl, al
        add dl, 'A'
.dr:
        mov [ini_name], dl
.dr2:
        ; el que hay (hasta 32 KB)
        mov word [olen], 0
        mov ax, 0x3D00
        mov dx, ini_name
        int 0x21
        jc .new
        mov bx, ax
        push ds
        mov ds, [dseg]
        xor dx, dx
        mov cx, INI_OUT - 0x100
        mov ah, 0x3F
        int 0x21
        pop ds
        jc .rc
        mov [olen], ax
.rc:
        mov ah, 0x3E
        int 0x21
.new:
        ; primero lo de antes de la primera sección (los comentarios), luego
        ; las secciones nuevas y después las de los demás equipos: así lo
        ; más reciente va al principio (HSPSND lee los primeros 8 KB)
        mov word [ilen], INI_OUT
        mov word [mpos], 0
        cmp word [olen], 0
        jne .old
        MODE O_INI
        mov si, ini_head
        call pstr
        jmp .secs
.old:
        mov byte [mphase], 1
        call ini_merge
.secs:
        cmp byte [w_ok], 0
        je .s1
        call w_sec_hda
.s1:
        xor cx, cx
.s2:
        cmp cl, [nf_n]
        jae .s3
        mov si, cx
        imul si, si, REC
        add si, nf_tab
        push cx
        call w_sec_nf
        pop cx
        inc cx
        jmp .s2
.s3:
        cmp byte [ac_res], 0
        je .s4
        call w_sec_ac97
.s4:
        mov byte [mphase], 2
        call ini_merge
        ; escribirlo
        mov ah, 0x3C
        xor cx, cx
        mov dx, ini_name
        int 0x21
        jc .bad
        mov bx, ax
        mov cx, [ilen]
        sub cx, INI_OUT
        push ds
        mov ds, [dseg]
        mov dx, INI_OUT
        mov ah, 0x40
        int 0x21
        pop ds
        jc .bc
        mov dx, [ilen]
        sub dx, INI_OUT
        cmp ax, dx
        jne .bc
        mov ah, 0x3E
        int 0x21
        jc .bad
        mov byte [ini_ok], 1
        jmp .msg
.bc:
        mov ah, 0x3E
        int 0x21
.bad:
.msg:
        MODE O_SCR | O_REP
        cmp byte [ini_ok], 0
        je .m0
        cmp byte [w_ok], 0
        je .ma
        cmp byte [w_blk + c_brute - c_blk], 0
        je .my
.ma:
        cmp byte [ac_res], 1
        je .my
        cmp byte [ac_res], 2
        je .my
        PR NL, "Queda apuntado en "
        mov si, ini_name
        call pstr
        PR " que no ha sonado nada:", NL, "HSPSND buscará la salida él solo.", NL
        ret
.my:
        mov byte [ini_good], 1
        PR NL, "Lo que ha sonado se ha guardado en "
        mov si, ini_name
        call pstr
        PR ":", NL, "HSPSND usará ese mismo camino al cargarse (HSPSND /NOAPRENDIDO no lo usa).", NL
        ret
.m0:
        PR NL, "ATENCION: no se ha podido guardar lo aprendido en "
        mov si, ini_name
        call pstr
        PR ".", NL
        ret

; ---- copiar el SONIDO.INI viejo sin las secciones de este equipo ----
; [mphase] = 1: hasta la primera sección; 2: desde ahí, sin las de este
ini_merge:
        mov byte [keep], 1
        mov si, [mpos]
.l:
        mov [mpos], si
        cmp si, [olen]
        jae .end
        mov [l_beg], si
        call oline                  ; -> lbuf, SI = la línea siguiente
        mov [l_end], si
        mov bx, lbuf
.sp:
        cmp byte [bx], ' '
        je .sk
        cmp byte [bx], 9
        jne .h
.sk:
        inc bx
        jmp .sp
.h:
        cmp byte [bx], '['
        jne .cp
        cmp byte [mphase], 1
        je .end
        call sec_drop
        mov byte [keep], 1
        jnc .cp
        mov byte [keep], 0
.cp:
        mov si, [l_end]
        cmp byte [keep], 0
        je .l
        ; la línea, tal cual
        push si
        push ds
        push es
        mov cx, [l_end]
        mov si, [l_beg]
        sub cx, si
        mov di, [ilen]
        add [ilen], cx
        mov es, [dseg]
        mov ds, [dseg]
        rep movsb
        pop es
        pop ds
        pop si
        jmp .l
.end:
        ; que acabe en un fin de línea
        mov bx, [ilen]
        cmp word [olen], 0
        je .r
        cmp bx, INI_OUT
        je .r
        push es
        mov es, [dseg]
        cmp byte [es:bx-1], 10
        je .r2
        mov word [es:bx], 0x0A0D
        add word [ilen], 2
.r2:
        pop es
.r:
        ret

; dseg:SI -> la línea en lbuf (sin el fin de línea, con un 0) y SI = la siguiente
oline:
        push es
        push ax
        push di
        mov es, [dseg]
        mov di, lbuf
.l:
        cmp si, [olen]
        jae .e
        mov al, [es:si]
        inc si
        cmp al, 10
        je .e
        cmp al, 13
        je .l
        cmp di, lbuf + 158
        jae .l
        mov [di], al
        inc di
        jmp .l
.e:
        mov byte [di], 0
        pop di
        pop ax
        pop es
        ret

; BX -> cabecera vieja (en lbuf). CF = 1 si es de este equipo (se quita):
; igual que una de las nuevas, o con el mismo «Equipo=» y la misma
; controladora (los tres primeros campos de la cabecera)
sec_drop:
        ; (lbuf se va a reutilizar)
        mov si, bx
        mov di, hbuf
.c:
        lodsb
        mov [di], al
        inc di
        test al, al
        jnz .c
        xor cx, cx
.f:
        cmp cl, [nhdr]
        jae .eq
        mov di, cx
        imul di, di, HDRSZ
        add di, hdr_tab
        mov si, hbuf
        mov dl, 0
        call hdrcmp
        je .drop
        inc cx
        jmp .f
.eq:
        ; ¿«Equipo=» igual? (en las líneas de la sección)
        mov si, [l_end]
.ql:
        cmp si, [olen]
        jae .keep
        call oline
        mov bx, lbuf
.qs:
        cmp byte [bx], ' '
        jne .qh
        inc bx
        jmp .qs
.qh:
        cmp byte [bx], '['
        je .keep
        mov di, eqline
        call linecmp
        jne .ql
        ; la misma controladora que una nueva
        xor cx, cx
.p:
        cmp cl, [nhdr]
        jae .keep
        mov di, cx
        imul di, di, HDRSZ
        add di, hdr_tab
        mov si, hbuf
        mov dl, 1
        call hdrcmp
        je .drop
        inc cx
        jmp .p
.keep:
        clc
        ret
.drop:
        stc
        ret

; SI = cabecera vieja, DI = nueva; DL = 0: entera (hasta «]»), 1: hasta el
; tercer espacio -> ZF = 1 si son iguales (sin mirar mayúsculas)
hdrcmp:
        push cx
        xor cx, cx
.sp:
        cmp byte [si], ' '
        je .sk
        cmp byte [si], 9
        jne .l
.sk:
        inc si
        jmp .sp
.l:
        mov al, [si]
        call b_up
        mov ah, al
        mov al, [di]
        call b_up
        cmp al, ah
        jne .r
        test al, al
        jz .ne
        cmp al, ']'
        je .r
        inc si
        inc di
        cmp al, ' '
        jne .l
        inc cx
        test dl, dl
        jz .l
        cmp cx, 3
        jb .l
        cmp al, al
.r:
        pop cx
        ret
.ne:
        or al, 1
        pop cx
        ret

; BX = línea (lbuf), DI = la que se busca: ZF = 1 si son iguales (sin mirar
; mayúsculas, ni los espacios del final de la de BX)
linecmp:
        push si
        mov si, bx
.l:
        mov al, [si]
        call b_up
        mov ah, al
        mov al, [di]
        call b_up
        test al, al
        jz .end
        cmp al, ah
        jne .r
        inc si
        inc di
        jmp .l
.end:
        mov al, [si]
        inc si
        test al, al
        jz .r
        cmp al, ' '
        je .end
        cmp al, 9
        je .end
.r:
        pop si
        ret

b_up:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 0x20
.r:
        ret
; AL (0..99) -> dos cifras en ES:DI
b_dec2:
        push ax
        aam
        add ax, 0x3030
        xchg al, ah
        stosw
        pop ax
        ret
; copiar SI (ASCIIZ) a ES:DI, sin el 0
b_cpy:
        lodsb
        test al, al
        jz .r
        stosb
        jmp b_cpy
.r:
        ret

st_suena    db "SUENA", 0
st_nosuena  db "NO SUENA", 0
st_todo     db "SOLO CON TODO ABIERTO", 0
s_dash      db "-", 0
s_equipo    db "Equipo=", 0
INIMAX      equ 80
ini_name    db "?:\HISPADOS\SONIDO.INI", 0
            times INIMAX - ($ - ini_name) db 0
ini_head    db "; SONIDO.INI - lo que ha aprendido la prueba de sonido de Hispa-DOS", 13, 10
            db ";", 13, 10
            db "; Lo escribe AUDICION (la prueba de sonido: opción «Prueba» del menú de", 13, 10
            db "; arranque) cuando se contesta que sí se oye un sonido. HSPSND lo lee al", 13, 10
            db "; cargarse: si aquí está la tarjeta de este equipo, pone exactamente el", 13, 10
            db "; camino que sonó en vez de buscarlo (HSPSND /NOAPRENDIDO no lo usa).", 13, 10
            db ";", 13, 10
            db "; Una sección por equipo; la cabecera es la tarjeta, por los números de", 13, 10
            db "; la controladora PCI y del códec, que no cambian:", 13, 10
            db ";   [HDA controladora subsistema códec subsistema-del-códec]  HD Audio", 13, 10
            db ";   [AC97 controladora subsistema códec]                      AC'97", 13, 10
            db "; La prueba más reciente va primero. Una prueba nueva en el mismo equipo", 13, 10
            db "; reemplaza su sección; las de los demás equipos no se tocan. Si borra", 13, 10
            db "; una sección (o este fichero), HSPSND vuelve a buscar la salida solo.", 13, 10
            db ";", 13, 10
            db "; Estado      SUENA; con NO SUENA o SOLO CON TODO ABIERTO, HSPSND no la usa", 13, 10
            db "; Equipo      fabricante | modelo | versión (SMBIOS), como ayuda", 13, 10
            db "; Posicion    bus:disp.fun de la controladora, y Codec la dirección del", 13, 10
            db ";             códec: solo como pista (pueden cambiar)", 13, 10
            db "; Altavoz     el pin que sonó (Auriculares: el de los auriculares)", 13, 10
            db "; Ruta        del pin al DAC (el último); (n) es la entrada elegida en la", 13, 10
            db ";             lista de conexiones de ese nodo (y RutaAuriculares)", 13, 10
            db "; Control     control del pin: 40h salida; C0h salida con el amplificador", 13, 10
            db ";             de auriculares (y ControlAuriculares)", 13, 10
            db "; EAPD        encendido, apagado o «no hay» (AC'97: normal o invertido)", 13, 10
            db "; GPIO        máscara, dirección y datos de las GPIO del códec, o «no hay»", 13, 10
            db "; Snoop       «activado» si hizo falta el «snoop» de AMD/ATI (PCI 42h)", 13, 10
            db "; Frecuencia  la de la prueba (con 44100, HSPSND toca siempre a 44100)", 13, 10
            db "; Volumen     «amplificador nodo salida» (o «entrada n»), o «por programa»", 13, 10
            db ";             si en la prueba no bajaba o no hay amplificador", 13, 10
            db "; Las líneas que empiezan por «;» son comentarios.", 13, 10, 0


; =============================================================================
;  Resumen
; =============================================================================
summary:
        MODE O_SCR | O_REP
        PR NL, "=== RESUMEN ===", NL
        mov si, sumbuf
        mov cx, [slen]
        jcxz .ns
.sl:
        lodsb
        call pchar
        loop .sl
.ns:
        cmp byte [aborted], 0
        je .na
        PR "(La prueba se terminó con Esc: lo que no sale arriba no se ha probado.)", NL
.na:
        cmp byte [n_hda], 0
        je .tail
        cmp byte [v86], 0
        jne .tail
        cmp byte [found_an], 0
        jne .sug
        cmp byte [aborted], 0
        jne .tail
        PR "HD Audio: NINGUNA salida analógica ha sonado con las combinaciones probadas.", NL
        PR "  Envíe el informe: con el volcado de los códecs se buscará el camino.", NL
        jmp .tail
.sug:
        PR "Línea sugerida: HSPSND"
        cmp byte [w_gn], 0
        je .se
        cmp byte [w_blk + c_gmode - c_blk], 0
        jne .sg
        PR " /NOGPIO"
        jmp .se
.sg:
        mov al, [w_blk + c_gdata - c_blk]
        cmp al, [w_gall]
        je .se
        PR " /GPIO:"
        call phex2
.se:
        cmp byte [w_blk + c_eapd - c_blk], 0
        jne .s2
        PR " /EAPD:0"
.s2:
        PR NL
        mov byte [need], 0
        cmp byte [w_blk + c_snoop - c_blk], 0
        je .n1
        PR "  FALTA en HSPSND: activar «snoop» (registro PCI 42h) en AMD/ATI.", NL
        mov byte [need], 1
.n1:
        cmp byte [w_blk + c_alt - c_blk], 0
        je .n2
        PR "  FALTA en HSPSND: elegir la ruta alternativa (otro DAC o mezclador).", NL
        mov byte [need], 1
.n2:
        cmp byte [w_blk + c_brute - c_blk], 0
        je .n3
        PR "  FALTA en HSPSND: solo ha sonado con TODO ABIERTO; hay que estudiar el", NL
        PR "  volcado para saber qué salida es.", NL
        mov byte [need], 1
        jmp .n4
.n3:
        test byte [w_blk + c_flags - c_blk], 0x10
        jz .n4
        cmp byte [w_blk + c_hp - c_blk], 0
        jne .n4
        PR "  FALTA en HSPSND: no encender el amplificador de auriculares de ese pin.", NL
        mov byte [need], 1
.n4:
        cmp byte [w_blk + c_ri - c_blk], RI_44K
        jne .n5
        test byte [w_pcm], 0x40
        jz .n5
        PR "  FALTA en HSPSND: solo suena a 44100 Hz (a 48000 Hz no).", NL
        mov byte [need], 1
.n5:
        cmp byte [w_pio], 0
        jne .n6
        PR "  FALTA en HSPSND: hablar con el códec por CORB/RIRB (HSPSND usa las", NL
        PR "  órdenes inmediatas, que esta controladora no atiende).", NL
        mov byte [need], 1
.n6:
        cmp byte [w_idx], 0
        je .n7
        PR "  AVISO: antes hay otro códec con salida analógica que no ha sonado;", NL
        PR "  HSPSND toma el primero que tenga una salida analógica.", NL
.n7:
        cmp byte [w_blk + c_cls - c_blk], 4
        jne .n8
        cmp byte [w_blk + c_brute - c_blk], 0
        jne .n8
        PR "  AVISO: el pin que suena no está declarado como altavoz, auriculares ni", NL
        PR "  línea; HSPSND solo lo usaría si no hubiera otros.", NL
.n8:
        cmp byte [after_br], 0
        je .n9
        PR "  AVISO: ese pin se encontró después de abrirlo todo; puede depender de", NL
        PR "  algo más que su ruta (mirar los intentos anteriores).", NL
.n9:
        cmp byte [need], 0
        jne .nd
        PR "  Con esa línea HSPSND debería sonar en este equipo. Si no suena, la", NL
        PR "  diferencia está en cómo arranca HSPSND la tarjeta: envíe este informe.", NL
        jmp .tail
.nd:
        PR "  ATENCION: el camino que suena necesita lo marcado con FALTA, que", NL
        PR "  HSPSND todavía no sabe hacer.", NL
.tail:
        cmp byte [lpib_bad], 0
        je .t2
        PR "AVISO: en algún intento la posición del flujo no avanzó (ver el informe).", NL
.t2:
        cmp word [nfail], 0
        je .t3
        PR "AVISO: órdenes al códec sin respuesta: "
        movzx eax, word [nfail]
        call pdec
        PR NL
.t3:
        ret

; =============================================================================
;  Textos y datos
; =============================================================================
msg_usage   db "AUDICION - prueba de sonido con preguntas de sí o no", 13, 10
            db 13, 10
            db "  AUDICION [carpeta]", 13, 10
            db "  AUDICION /RAPIDA [carpeta] [/INI:ruta]", 13, 10
            db 13, 10
            db "Recoge los datos de sonido del equipo (controladoras, códecs, pines),", 13, 10
            db "toca sonidos por cada salida y pregunta si se oyen: S = sí, N = no,", 13, 10
            db "R = repetir, Esc = terminar. No usa HSPSND: maneja la tarjeta él mismo", 13, 10
            db "(lo mejor es arrancar con la opción «Prueba de sonido»).", 13, 10
            db "Guarda el informe en SONIDO01.TXT, SONIDO02.TXT... en la raíz de la", 13, 10
            db "unidad de arranque, sin sobrescribir ninguno. Con una carpeta, lo guarda", 13, 10
            db "en ella como SONIDO.TXT: AUDICION C:\INFORMES\EQUIPO01", 13, 10
            db "Lo que suena lo guarda en \HISPADOS\SONIDO.INI de la unidad de arranque:", 13, 10
            db "HSPSND lo usa al cargarse (código de salida 2: ha sonado y está guardado).", 13, 10
            db "/RAPIDA: la versión corta (la de INSTALAR): pitido, la primera salida que", 13, 10
            db "suene, el volumen y los dos canales; Esc la salta sin apuntar nada.", 13, 10
            db "/INI:ruta: guarda lo aprendido en ese SONIDO.INI (C:\HISPADOS\SONIDO.INI).", 13, 10
            db "El informe se sube al FTP, a HispaDOS/informes. Ver docs/AUDICION.md.", 13, 10, 0
s_sonido    db "\SONIDO", 0
q_beep      db "¿Ha oído el pitido?", 0
l_beep      db "Altavoz del PC (pitido)", 0
q_hear      db "¿Lo oye?", 0
q_low       db "¿El segundo ha sonado más bajo que el primero?", 0
l_low       db "  Bajar el volumen", 0
q_max       db "¿El segundo ha sonado más fuerte que el primero?", 0
l_max       db "  Subir el volumen", 0
q_mute      db "¿Se ha callado del todo?", 0
l_mute      db "  Silencio", 0
q_left      db "¿Suena solo por la izquierda?", 0
l_left      db "  Solo izquierda", 0
q_right     db "¿Suena solo por la derecha?", 0
l_right     db "  Solo derecha", 0
q_same      db "¿El segundo suena igual de agudo que el primero?", 0
l_r8        db "  8000 Hz suena igual de agudo", 0
l_r11       db "  11025 Hz suena igual de agudo", 0
l_r22       db "  22050 Hz suena igual de agudo", 0
l_r44       db "  44100 Hz suena igual de agudo", 0
l_r48       db "  48000 Hz suena igual de agudo", 0
l_8         db "  8 bits suena igual que 16 bits", 0
q_long      db "¿Se ha oído entero, sin cortes ni ruidos?", 0
l_long      db "  Sonido largo (5 s) sin cortes", 0
q_plug      db "Enchufe unos auriculares si tiene. ¿Los ha enchufado?", 0
q_hp        db "¿Suena por los auriculares?", 0
q_spkoff    db "¿Se ha callado el altavoz? (debe sonar solo por los auriculares)", 0
l_spkoff    db "  El altavoz calla al apagar su pin", 0
l_ac97      db "AC'97: suena", 0
l_ac97b     db "AC'97 con EAPD invertido: suena", 0
l_sb        db "Sound Blaster: suena", 0
l_sbb       db "Sound Blaster con el mezclador al máximo: suena", 0

gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0

        section .bss
bss_start:
sp0         resw 1
omode       resb 1
sim         resb 1
dseg        resw 1
v86         resb 1
a20_on      resb 1
a20_closed  resb 1
rep_ok      resb 1
rc_retry    resb 1
hsp_was     resb 1
hsp_type    resb 1
hsp_ent     resd 1
aborted     resb 1
has_dir     resb 1
outdir      resb 64
fname       resb 80
rlen        resw 1
slen        resw 1
qnum        resb 1
reps        resb 1
replay      resw 1
tmo_t0      resw 1
tmo_cnt     resd 1
cpu_txt     resb 52
sm_addr     resd 1
sm_len      resw 1
sm_cnt      resw 1
sm_type     resb 1
pci_last    resb 1
nctl        resb 1
ctl_tab     resb NCTL * 4
pd_sub      resb 1
pd_ven      resw 1
nam         resw 1
nabm        resw 1
sb_base     resw 1
sb_ebase    resw 1
sb_eirq     resb 1
sb_edma     resb 1
sb_haveenv  resb 1
sb_major    resb 1
sb_minor    resb 1
sb_dma      resb 1
hda_live    resb 1
hda_base    resd 1
codec_dead  resb 1
cad         resb 1
last_cmd    resd 1
use_corb    resb 1
fails       resb 1
nfail       resw 1
corb_wp     resw 1
corb_mask   resw 1
rirb_rp     resw 1
rirb_mask   resw 1
cur_ctl     resb 1
cur_bdf     resw 1
cur_ven     resw 1
cur_cmd0    resw 1
verbose     resb 1
do_reset    resb 1
was_reset   resb 1
statests    resw 1
gcap        resw 1
hda_sd      resw 1
pio_ok      resb 1
corb_ok     resb 1
afg         resb 1
w_count     resb 1
w_first     resb 1
g_n         resb 1
g_all       resb 1
wcaps       resd 1
gc_prev     resb 1
gc_k        resb 1
t_ri        resb 1
t_bits      resb 1
t_ch        resb 1
t_amp       resw 1
t_off       resw 1
t_frames    resw 1
t_bytes     resw 1
t_ms        resw 1
t_fmt       resw 1
g_eps       resd 1
g_x         resd 1
g_y         resd 1
dacs        resb 8
            resb 1
lpib_ok     resb 1
lpib_bad    resb 1
n_hda       resb 1
dc_id       resd 1
dc_gp       resd 1
dc_gm       resb 1
dc_gd       resb 1
dc_gv       resb 1
n_gsave     resb 1
gsave       resb 12 * 5
dw_nid      resb 1
dw_type     resb 1
dw_nc       resw 1
dw_conn     resb CONNMAX
dw_pc       resd 1
dw_cfg      resd 1
dw_ci       resw 1
pass        resb 1
found       resb 1
found_an    resb 1
hda_idx     resb 1
sn_can      resb 1
sn_orig     resb 1
ncand       resb 1
any_eapd    resb 1
bc_dig      resb 1
cands       resb NCAND * 4
gs_have     resb 1
gs_m        resb 1                  ; (seguidos: máscara y dirección)
gs_d        resb 1
gs_v        resb 1
first       resb 1
tried       resb 1
cls         resb 1
c_blk:
c_pin       resb 1
c_cls       resb 1
c_flags     resb 1
c_eapd      resb 1
c_gmode     resb 1
c_gdata     resb 1
c_ri        resb 1
c_alt       resb 1
c_snoop     resb 1
c_brute     resb 1
c_hp        resb 1
c_ci        resb 1
C_SIZE      equ $ - c_blk
w_blk       resb C_SIZE
w_pio       resb 1
w_idx       resb 1
w_gn        resb 1
w_gall      resb 1
w_pcm       resb 1
need        resb 1
an_prev     resb 1
after_br    resb 1
no_path     resb 1
path        resb 8
pidx        resb 8
plen        resb 1
p_dac       resb 1
pinctl      resb 1
g_em        resb 1
g_ed        resb 1
g_ev        resb 1
dac_pcm     resd 1
pamps_n     resb 1
pamps       resb 8 * 6
want_dig    resb 1
alt_k       resb 1
lvl         resb 1
skipn       resb 1
cur_type    resb 1
conn_bufs   resb 5 * CONNMAX
va          resw 1
test_ri     resb 1
pres0       resb NCAND
pres1       resb NCAND
hp_chg      resb 1
hp_pass     resb 1
hp_n        resb 1
hp_pin      resb 1
rbuf        resb RBUFSZ + 16
sumbuf      resb SUMSZ
; lo aprendido (SONIDO.INI)
capp        resw 1
caplen      resw 1
capmax      resw 1
smb_eq      resb SMBEQ
lr_rec      resb REC
w_rec       resb REC
w_path      resb 8                  ; (w_path y w_pidx, seguidos)
w_pidx      resb 8
w_plen      resb 1
w_ctl       resb 1
w_g         resb 3
w_eap       resb 1
w_ok        resb 1
quick       resb 1                  ; /RAPIDA: la versión corta
hush        resb 1                  ; 1: nada a la pantalla (salvo las preguntas)
has_ini     resb 1                  ; /INI:ruta dada
qexit       resb 1                  ; código de salida de la versión corta
w_vsoft     resb 1
w_vamp      resw 1
w_hp        resb 1
w_hpath     resb 8                  ; (y w_hpidx, seguidos)
w_hpidx     resb 8
w_hplen     resb 1
w_hctl      resb 1
nf_n        resb 1
nf_tab      resb NNF * REC
ac_bdf      resw 1
ac_res      resb 1                  ; 1 suena, 2 con EAPD invertido, 3 no
ac_hdr      resb 1
sb_res      resb 1
ini_ok      resb 1
ini_good    resb 1
nhdr        resb 1
hdr_tab     resb 6 * HDRSZ
date_txt    resb 20
eqline      resb SMBEQ + 10
ilen        resw 1
olen        resw 1
l_beg       resw 1
l_end       resw 1
keep        resb 1
mphase      resb 1
mpos        resw 1
lbuf        resb 160
hbuf        resb 160
bss_end:
