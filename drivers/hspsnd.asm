; =============================================================================
;  HSPSND.COM - driver de sonido de Hispa-DOS (residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPSND [/SB] [/SB8] [/NOSB] [/VOL:n] [/INFO] [/GPIO:hh] [/NOGPIO]
;               [/EAPD:0] [/NOAPRENDIDO]
;               [/NOTECLAS]   (en AUTOEXEC.BAT o con INSTALL= en CONFIG.SYS)
;        /SB        Sound Blaster 16 emulada sobre el HD Audio o el AC'97,
;                   para los juegos (hace falta HSPEMM; si hay una Sound
;                   Blaster de verdad, no hace nada). Ver docs/HSPSB.md
;        /SB8       [SONIDO-JUEGOS] Sound Blaster 16: tocar por la vía de 8
;                   bits (DMA baja), como una SB Pro, y no por la de 16
;        /NOSB      no buscar una Sound Blaster
;        /VOL:n     volumen del 0 al 100 % (80 si no se dice); con el driver
;                   ya instalado cambia el volumen del residente
;        /INFO      (o /DIAG) muestra la tarjeta, el códec, las salidas y el
;                   camino elegido; también con el driver ya instalado
;        /GPIO:hh   HD Audio: valor (hex) de las patillas GPIO del códec, que
;                   en muchos portátiles encienden el amplificador de los
;                   altavoces (si no se dice, todas a 1); /NOGPIO no las toca
;        /EAPD:0    HD Audio: apagar EAPD en vez de encenderlo
;        /NOAPRENDIDO  no usar \HISPADOS\SONIDO.INI (lo aprendido en la
;                   prueba de sonido: ver más abajo)
;        /NOTECLAS  sin teclas de volumen (Ctrl+Alt+flecha arriba/abajo,
;                   Ctrl+Alt+Fin = silencio, y las teclas multimedia)
;
;  Tarjetas: Sound Blaster 2.0/Pro/16 (ISA; variable BLASTER o sondeo en
;  220h-280h; si no está BLASTER, la pone), Intel HD Audio (HDA, PCs
;  actuales) e Intel AC'97 (ICH y
;  compatibles). HD Audio necesita el modo real "irreal" (FS con límite de
;  4 GB) para llegar a sus registros. En modo V86 solo funciona con HSPEMM,
;  que le mapea los registros en una página de su propia memoria (hda_wseg);
;  bajo otros EMM386 no.
;
;  Lo aprendido (SONIDO.INI): AUDICION, cuando se contesta que SÍ suena,
;  guarda en \HISPADOS\SONIDO.INI de la unidad de arranque una sección por
;  tarjeta (cabecera [HDA pci subsistema códec subsistema-del-códec] o
;  [AC97 pci subsistema códec]) con el camino exacto que sonó. Al cargarse,
;  antes de buscar, HSPSND lee ese fichero (de su unidad o de la de arranque)
;  y, si está la tarjeta de este equipo, pone ese camino: pin, ruta con la
;  entrada de cada nodo, control del pin, EAPD, GPIO, «snoop», 44,1 kHz y
;  volumen por programa (codec_learn, que comprueba antes que la ruta existe
;  en el códec). Si no está, no cuadra o falla, busca como siempre y lo dice.
;  Las opciones /GPIO, /NOGPIO y /EAPD mandan sobre lo aprendido. El análisis
;  del fichero va en la instalación (se descarta); residente quedan solo
;  unos 50 octetos para /INFO (lrn_*). Ver docs/AUDICION.md.
;
;  Con LH: si los búferes de DMA (van detrás del código residente) no caben
;  en el bloque de memoria superior, el programa se carga otra vez a sí mismo
;  en la memoria convencional (fit_check; la opción interna /~ es de esa
;  segunda copia).
;
;  API: INT 2Fh AX=D700h -> AL=FFh si está instalado, ES:BX = punto de entrada
;  (llamada lejana, AH = función):
;    00h información  -> AL = tarjeta (0 ninguna, 1 Sound Blaster,
;                        2 Sound Blaster 16, 3 AC'97, 4 HD Audio),
;                        BX = versión,
;                        ES:DI = nombre
;    01h reproducir   DS:SI = datos PCM, CX = bytes, DX = Hz,
;                     AL = formato (bit 0: 16 bits, bit 1: estéreo).
;                     Vuelve cuando los datos están en cola; el sonido sigue.
;    02h esperar a que termine
;    03h parar
;    04h volumen      AL = 0 (silencio) .. 31 (máximo)
;    05h volumen      AL = 0 .. 100 (%); quita el silencio
;    06h leer volumen -> AL = volumen (0..100 %), AH = 1 si está en silencio
;    07h silencio     AL = 0 quitar, 1 poner, 2 cambiar -> como 06h
;    08h sitio        -> AL = trozos que caben ahora en la cola sin que 01h
;                     espere (de 0 a 4). Un trozo es lo que la tarjeta toca de
;                     una vez: 2.048 tramas a la frecuencia de la tarjeta con
;                     HD Audio y AC'97, 1.024 con Sound Blaster 16 y 4.096
;                     muestras con las de 8 bits. Para quien no puede esperar
;                     (una ventana): preguntar y mandar solo lo que cabe.
;                     (Versión 3 de la interfaz.)
;  El mezclador por fuentes (versión 4; lib/hspmez.inc, docs/MEZCLADOR.md).
;  Fuentes: 0 General (el volumen de 05h-07h), 1 Sonidos (la onda: lo que
;  se toca con 01h y la voz de la Sound Blaster emulada), 2 Música (FM),
;  3 CD, 4 Línea, 5 Micrófono, 6 Altavoz del PC. Volumen 0..100 %, balance
;  0..100 (50 en medio) y casillas: bit 0 silencio, bit 1 «Sonar» (CD,
;  Línea y Micrófono pasan a los altavoces), bit 2 «Grabar».
;    09h controles    -> AL = fuentes que hay (bit n = fuente n), AH = las
;                     que lleva la tarjeta (las demás, por programa),
;                     BX = ajustes que hay (bit 0 balance, 1 graves,
;                     2 agudos, 3 ganancia de entrada, 4 ganancia de
;                     salida, 5 ajuste automático del micrófono, 6 se graba
;                     de una sola fuente a la vez, 7 el altavoz en cuatro
;                     pasos, 8 refuerzo del micrófono), CX = fuentes que se
;                     pueden grabar, DX = fuentes con «Sonar»
;    0Ah leer fuente  AL = fuente -> AL = volumen, AH = casillas, BL = balance
;    0Bh poner fuente AL = fuente, BL = volumen, BH = casillas, CL = balance
;                     (FFh: no se cambia) -> como 0Ah. Si la tarjeta graba
;                     de una sola fuente, marcar «Grabar» quita las demás
;    0Ch leer ajuste  AL = ajuste (0 graves, 1 agudos: 0..100, 50 en medio;
;                     2 ganancia de entrada, 3 de salida: 0..3 = x1 x2 x4
;                     x8; 4 ajuste automático del micrófono, 5 refuerzo del
;                     micrófono: 0/1) -> AL = valor
;    0Dh poner ajuste AL = ajuste, BL = valor
;    0Eh AL = 0: todo a los valores de fábrica; 1: volver a escribirlo todo
;                     en la tarjeta (por si otro programa lo ha cambiado)
;    0Fh cambios      AL = 0 -> AL = 1 si hay cambios sin guardar en
;                     SONIDO.INI; AL = 1 ya están guardados; AL = 2 los hay
;  Al cargarse, HSPSND pone lo de la sección [Mezclador] de SONIDO.INI (lo
;  guardan Hispa Ventanas y su Mezclador); /VOL:n manda sobre «General».
; =============================================================================
        cpu 386
        bits 16
        org 0x100

VERSION     equ 0x0004

; el mezclador por fuentes (lib/hspmez.inc)
MZ_N        equ 7                   ; fuentes
MZ_GEN      equ 0
MZ_SON      equ 1
MZ_MUS      equ 2
MZ_CD       equ 3
MZ_LIN      equ 4
MZ_MIC      equ 5
MZ_ALT      equ 6
MZ_NAJ      equ 6                   ; ajustes
MZF_MUDO    equ 1                   ; casillas de cada fuente
MZF_SONAR   equ 2
MZF_GRABAR  equ 4
MZC_BAL     equ 0x0001              ; ajustes que tiene la tarjeta (función 09h)
MZC_GRAVES  equ 0x0002
MZC_AGUDOS  equ 0x0004
MZC_GENT    equ 0x0008
MZC_GSAL    equ 0x0010
MZC_AGC     equ 0x0020
MZC_UNA     equ 0x0040              ; se graba de una sola fuente a la vez
MZC_ALT4    equ 0x0080              ; el altavoz del PC, en cuatro pasos
MZC_REFUERZO equ 0x0100
MZA_CD      equ 0x01                ; AC'97: registros que tiene
MZA_LIN     equ 0x02
MZA_MIC     equ 0x04
MZA_BEEP    equ 0x08
MZA_TONO    equ 0x10

NBUF        equ 4                   ; búferes de DMA
BUFSZ       equ 8192                ; bytes por búfer (16 bits estéreo)
FRAMES      equ BUFSZ / 4

start:
        jmp init

; ---- datos residentes ----
old2f       dd 0
dev_type    db 0
started     db 0
wr_idx      db 0
vra         db 0
nam         dw 0
nabm        dw 0
cur_rate    dw 0
p_fmt       db 0
            db 0
p_left      dw 0
p_src       dd 0
name_ac97   db "Intel AC'97", 0
name_hda    db "Intel HD Audio", 0
hda_base    dd 0
hda_reg     dd 0                    ; lo que se suma al registro: hda_base en
                                    ; modo real; en modo V86, lo que pasa de
                                    ; la página (la ventana empieza en FS:0)
hda_wseg    dw 0                    ; modo V86: segmento de la ventana
v86_xlat    db 0                    ; modo V86 con VCPI: lineal != física
e_on        db 0                    ; Sound Blaster emulada en marcha (/SB)
e_sbres     db 0                    ; ... y su código está residente
hda_sd      dw 0                    ; registros del primer flujo de salida
hda_cad     db 0
hda_dac     db 0                    ; primer DAC (informativo)
hda_afg     db 0
hda_dacs    times 4 db 0            ; DAC en uso (0 = fin)
hda_npin    db 0
hda_pins    times 4 * 5 db 0        ; caminos: pin, intermedios..., DAC, ceros
hda_pidx    times 4 * 4 db 0        ; ... la entrada elegida en cada nodo
hda_pctl    times 4 db 0            ; ... y el control del pin
; lo aprendido en la prueba de sonido (SONIDO.INI), para /INFO
lrn_why     db 0                    ; 0 nada, 1 en uso, 2 de otra tarjeta, 3 no
                                    ; sonó, 4 no cuadra, 5 /NOAPRENDIDO
lrn_date    times 17 db 0
lrn_gpio    db 0                    ; GPIO aprendidas (y sus valores)
lrn_g       times 3 db 0
lrn_eapd    db 0                    ; 1 encendido, 2 apagado (AC'97: invertido)
lrn_snoop   db 0
lrn_44      db 0
lrn_soft    db 0
lrn_smb     db 0                    ; 1 mismo equipo (SMBIOS), 2 otro modelo
hda_nvol    db 0
hda_vols    times 8 * 6 db 0        ; nodo, 80h salida | índice de entrada,
                                    ; paso de 0 dB, pasos, tamaño (1/4 dB), -
pci_bdf     dw 0
hda_rates   dw 0x0040               ; frecuencias que admiten los DAC (bits
                                    ; del parámetro 0Ah: 8, 11,025, 16,
                                    ; 22,05, 32, 44,1, 48 kHz...)
hw_rate     dw 48000                ; frecuencia a la que toca la tarjeta
rs_acc      dd 0                    ; fase del cambio de frecuencia
fb_bpf      dw 0
; volumen y mezclador por fuentes (lib/hspmez.inc). Cada fuente tiene su
; volumen (0..100 %), sus casillas (MZF_*) y su balance (0..100, 50 en
; medio). La fuente 0, «General», es el volumen de siempre: vol_pct y
; vol_mute son su volumen y su silencio. Orden: General, Sonidos, Música,
; CD, Línea, Micrófono y Altavoz del PC.
mz_vol:
vol_pct     db 80                   ; 0..100 %
            db 90, 85, 85, 85, 85, 50
mz_flg:
vol_mute    db 0
            db 0, 0, MZF_SONAR, 0, MZF_GRABAR, 0
mz_bal      times MZ_N db 50
mz_ajv      db 50, 50, 0, 0, 1, 0   ; graves, agudos, ganancia de entrada y
                                    ; de salida (0..3: x1 x2 x4 x8), ajuste
                                    ; automático y refuerzo del micrófono
MZ_EST      equ $ - mz_vol          ; (lo que vuelve a los valores de fábrica)
mz_fab      db 80, 90, 85, 85, 85, 85, 50
            db 0, 0, 0, MZF_SONAR, 0, MZF_GRABAR, 0
            times MZ_N db 50
            db 50, 50, 0, 0, 1, 0
mz_src      db 0                    ; las fuentes que hay en esta tarjeta (bits)
mz_hwsrc    db 0                    ; ... las que lleva la tarjeta (las demás, por programa)
mz_rec      db 0                    ; ... las que se pueden grabar
mz_son      db 0                    ; ... las que tienen la casilla «Sonar»
mz_ajs      dw 0                    ; los ajustes que hay (MZC_*)
mz_chg      db 0                    ; hay cambios sin guardar (función 0Fh)
mz_ac       db 0                    ; AC'97: registros que tiene (MZA_*)
mz_ct1335   db 0                    ; Sound Blaster 2.0 con el mezclador CT1335
vol_dirty   db 0                    ; hay que llevarlo a la tarjeta
busy        db 0                    ; dentro de la API
att_q       db 0                    ; General, izquierda: atenuación en
att_r       db 0                    ; cuartos de dB (0..187); y derecha
vol_zero    db 0                    ; silencio efectivo (General)
hw_vol      db 1                    ; la tarjeta tiene control de volumen
soft_mul    dw 256                  ; (de antes: lo que pone soft_l)
soft_l      dw 256                  ; factor por programa de los Sonidos (256
soft_r      dw 256                  ; = sin cambio), izquierda y derecha
mus_l       dw 256                  ; ... y de la Música (la FM emulada)
mus_r       dw 256
hda_nin     db 0                    ; HD Audio: entradas del mezclador del
hda_in      times 6 * 6 db 0        ; códec (nodo, entrada, fuente, paso de
                                    ; 0 dB, pasos, tamaño en 1/4 dB)
kb_e0       db 0
keys_on     db 0
old15       dd 0
hda_fmt     dw 0
hda_written dd 0                    ; bytes encolados
hda_played  dd 0
hda_last    dd 0
hda_cycles  dd 0
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
name_none   db "ninguna", 0
bdl_off     dw 0
buf_off     dw 0

int2f:
        cmp ax, 0xD700
        jne .win
        mov al, 0xFF
        push cs
        pop es
        mov bx, entry
        iret
.win:
        ; Windows en modo 386 mejorado (DX bit 0 = 0) arranca o termina: la
        ; Sound Blaster emulada se para mientras tanto (ver hspsb.inc)
        cmp ax, 0x1605
        jne .w6
        test dl, 1
        jnz .chain
        cmp byte [cs:e_on], 0
        je .chain
        call e_winoff
        jmp .chain
.w6:
        cmp ax, 0x1606
        jne .chain
        test dl, 1
        jnz .chain
        cmp byte [cs:e_on], 0       ; (e_wason solo existe si hubo /SB)
        jne .chain
        cmp byte [cs:e_sbres], 0
        je .chain
        cmp byte [cs:e_wason], 0
        je .chain
        call e_winon
.chain:
        jmp far [cs:old2f]

entry:
        inc byte [cs:busy]
        push cs
        call .disp
        pushf
        dec byte [cs:busy]
        jnz .x
        cmp byte [cs:vol_dirty], 0
        je .x
        call vol_apply
.x:
        popf
        retf
.disp:
        cmp ah, 8                   ; (el sitio de HD Audio se mira en la tarjeta)
        je .hw
        cmp ah, 1
        jb .go
        cmp ah, 3
        ja .go
        ; con la Sound Blaster emulada, la API es una voz más de su anillo
        cmp byte [cs:e_on], 0
        jne e_api
.hw:
        cmp byte [cs:dev_type], 4
        jne .go
        ; HD Audio: las funciones usan FS (modo irreal) y hay que devolverlo
        ; como estaba; en modo V86 (EMM386 o Windows cargados después) no
        ; se puede llegar a los registros, salvo por la ventana de HSPEMM
        call hda_reach
        jc .v86
        push fs
        call a20_open
        push cs
        call .go
        call a20_restore
        pop fs
        retf
.v86:
        stc
        retf
.go:
        cmp ah, 0
        je f_info
        cmp ah, 1
        je f_play
        cmp ah, 2
        je f_wait
        cmp ah, 3
        je f_stop
        cmp ah, 4
        je f_volume
        cmp ah, 5
        je f_setpct
        cmp ah, 6
        je f_getvol
        cmp ah, 7
        je f_mute
        cmp ah, 8
        je f_room
        cmp ah, 0x09
        je f_mzcaps
        cmp ah, 0x0A
        je f_mzget
        cmp ah, 0x0B
        je f_mzset
        cmp ah, 0x0C
        je f_mzaget
        cmp ah, 0x0D
        je f_mzaset
        cmp ah, 0x0E
        je f_mzfab
        cmp ah, 0x0F
        je f_mzchg
        stc
        retf

; 08h: -> AL = trozos que caben en la cola sin esperar
f_room:
        mov al, 1                   ; Sound Blaster emulada: la API es una voz
        cmp byte [cs:e_on], 0       ; de su anillo, que siempre espera al trozo
        jne .r
        mov al, NBUF                ; parado: cabe todo
        cmp byte [cs:started], 0
        je .r
        cmp byte [cs:dev_type], 3
        je .ac
        ja .hda
        ; Sound Blaster: los bloques libres seguidos desde el que toca llenar
        ; hasta el que suena (como sb_wait_slot)
        push bx
        pushf
        cli
        xor al, al
        movzx bx, byte [cs:sb_wr]
.s:
        cmp byte [cs:sb_full+bx], 0
        jne .sx
        cmp bl, [cs:sb_play]
        je .sx
        inc al
        inc bl
        and bl, 3
        jmp .s
.sx:
        popf
        pop bx
        jmp .r
.ac:
        push dx                     ; AC'97: los que hay entre el que suena
        mov dx, [cs:nabm]           ; (CIV) y el último puesto (como ac_wait_slot)
        add dx, 0x16
        in ax, dx
        test al, 1                  ; DMA parado: todo libre
        mov al, NBUF
        jnz .acx
        mov dx, [cs:nabm]
        add dx, 0x14
        in al, dx
        mov ah, [cs:wr_idx]
        sub ah, al
        and ah, 31
        mov al, NBUF
        sub al, ah
        jnc .acx
        xor al, al
.acx:
        pop dx
        jmp .r
.hda:
        push ebx                    ; HD Audio: lo encolado menos lo tocado
        call unreal                 ; (como hda_play)
        call hda_update
        mov ebx, [cs:hda_written]
        sub ebx, [cs:hda_played]
        mov al, NBUF
        jbe .hx                     ; (se ha acabado: 01h empieza de nuevo)
        neg ebx
        add ebx, NBUF * BUFSZ
        mov al, 0
        js .hx
        shr ebx, 13
        mov al, bl
.hx:
        pop ebx
.r:
        clc
        retf

f_info:
        mov al, [cs:dev_type]
        mov bx, VERSION
        push cs
        pop es
        mov di, name_none
        cmp al, 1
        jne .s16
        mov di, name_sb
.s16:
        cmp al, 2
        jne .s3
        mov di, name_sb16
.s3:
        cmp al, 3
        jne .h
        mov di, name_ac97
.h:
        cmp al, 4
        jne .r
        mov di, name_hda
.r:
        clc
        retf

f_play:
        pushad
        push ds
        push es
        cmp byte [cs:dev_type], 4
        je hda_play
        cmp byte [cs:dev_type], 2
        je sb_play_f
        cmp byte [cs:dev_type], 1
        je sb_play_f
        cmp byte [cs:dev_type], 3
        jne .done
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:cur_rate]
        je .rate_ok
        ; cambiar de frecuencia: esperar a que acabe lo que suena
        call ac_wait
        call ac_stop
        mov [cs:cur_rate], dx
        call ac_set_rate
.rate_ok:
        cmp byte [cs:started], 0
        jne .loop
        mov byte [cs:wr_idx], 0     ; al arrancar, CIV empieza en 0
.loop:
        cmp word [cs:p_left], 0
        je .done
        call ac_wait_slot
        call fill_buffer
        test bx, bx
        jz .done
        call ac_queue
        jmp .loop
.done:
        pop es
        pop ds
        popad
        clc
        retf

f_wait:
        cmp byte [cs:dev_type], 2
        ja .nsb
        cmp byte [cs:dev_type], 0
        je .r
        call sb_wait
        jmp .r
.nsb:
        cmp byte [cs:dev_type], 4
        jne .a
        call unreal
        call hda_wait
        jmp .r
.a:
        cmp byte [cs:dev_type], 3
        jne .r
        call ac_wait
.r:
        clc
        retf

f_stop:
        cmp byte [cs:dev_type], 2
        ja .nsb
        cmp byte [cs:dev_type], 0
        je .r
        call sb_halt
        jmp .r
.nsb:
        cmp byte [cs:dev_type], 4
        jne .a
        call unreal
        call hda_stop
        jmp .r
.a:
        cmp byte [cs:dev_type], 3
        jne .r
        call ac_stop
.r:
        clc
        retf

; 04h: AL = 0..31
f_volume:
        push ax
        and al, 31
        mov ah, 100
        mul ah
        add ax, 15
        mov ah, 31
        div ah
        mov [cs:vol_pct], al
        pop ax
        jmp f_setpct.set
; 05h: AL = 0..100
f_setpct:
        push ax
        cmp al, 100
        jbe .ok
        mov al, 100
.ok:
        mov [cs:vol_pct], al
        pop ax
.set:
        mov byte [cs:vol_mute], 0
        mov byte [cs:vol_dirty], 1  ; se aplica al salir de la API
        mov byte [cs:mz_chg], 1
        clc
        retf
; 07h: AL = 0 quitar el silencio, 1 ponerlo, 2 cambiarlo
f_mute:
        cmp al, 2
        jb .put
        mov al, [cs:vol_mute]
        xor al, 1
.put:
        mov [cs:vol_mute], al
        mov byte [cs:vol_dirty], 1
        mov byte [cs:mz_chg], 1
; 06h: -> AL = volumen, AH = silencio
f_getvol:
        mov al, [cs:vol_pct]
        mov ah, [cs:vol_mute]
        clc
        retf

%include "hspmez.inc"

; ---- teclas de volumen: INT 15h AH=4Fh (la BIOS la llama desde INT 09h con
; el código de la tecla en AL; se vuelve con CF=0 para que no la procese) ----
int15:
        cmp ah, 0x4F
        je .key
.chain:
        jmp far [cs:old15]
.key:
        push ax
        push bx
        push ds
        mov bx, 0x40
        mov ds, bx
        cmp al, 0xE0
        jne .ne0
        mov byte [cs:kb_e0], 1
        jmp .pass
.ne0:
        mov ah, [cs:kb_e0]
        mov byte [cs:kb_e0], 0
        test ah, ah
        jz .arrows
        ; teclas multimedia: E0 30 subir, E0 2E bajar, E0 20 silencio
        mov bl, al
        and bl, 0x7F
        cmp bl, 0x30
        je .mm
        cmp bl, 0x2E
        je .mm
        cmp bl, 0x20
        jne .arrows
.mm:
        test al, 0x80               ; al soltarla: solo se consume
        jnz .eat
        cmp bl, 0x30
        je .up
        cmp bl, 0x2E
        je .down
        jmp .mute
.arrows:
        mov bl, [0x17]
        and bl, 0x0C                ; Ctrl y Alt
        cmp bl, 0x0C
        jne .pass
        cmp al, 0x48
        je .up
        cmp al, 0x50
        je .down
        cmp al, 0x4F
        je .mute
.pass:
        pop ds
        pop bx
        pop ax
        stc
        jmp .chain
.up:
        mov al, 10
        jmp .step
.down:
        mov al, -10
.step:
        mov byte [cs:vol_mute], 0
        add al, [cs:vol_pct]
        jns .s1
        xor al, al
.s1:
        cmp al, 100
        jbe .s2
        mov al, 100
.s2:
        mov [cs:vol_pct], al
        jmp .apply
.mute:
        xor byte [cs:vol_mute], 1
.apply:
        mov byte [cs:vol_dirty], 1
        mov byte [cs:mz_chg], 1
        cmp byte [cs:busy], 0       ; en mitad de una llamada: al acabar
        jne .eat
        call vol_apply
.eat:
        and byte [0x96], 0xFD       ; la BIOS ya vio el prefijo E0
        pop ds
        pop bx
        pop ax
        push bp
        mov bp, sp
        and byte [bp+6], 0xFE       ; CF = 0: tecla consumida
        pop bp
        iret

; ---- conversión: p_src -> búfer de DMA wr_idx&3 (16 bits estéreo) ----
; -> BX = tramas escritas
; Si la tarjeta no toca a la frecuencia de los datos (cur_rate != hw_rate:
; códecs HD Audio que solo admiten 44,1 y 48 kHz, AC'97 de 48 kHz fijos) se
; repiten o se saltan muestras con un acumulador de fase.
fill_buffer:
        push bp
        ; bytes por trama de entrada
        mov cx, 1
        test byte [cs:p_fmt], 1
        jz .b
        shl cx, 1
.b:
        test byte [cs:p_fmt], 2
        jz .c
        shl cx, 1
.c:
        ; destino
        movzx di, byte [cs:wr_idx]
        and di, NBUF - 1
        shl di, 13                  ; * BUFSZ
        add di, [cs:buf_off]
        push cs
        pop es
        lds si, [cs:p_src]
        mov dl, [cs:p_fmt]
        xor bp, bp                  ; tramas escritas
        cld
.l:
        cmp [cs:p_left], cx
        jb .eof
        cmp bp, FRAMES
        jae .end
        push si
        test dl, 1
        jnz .s16
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        mov bx, ax
        test dl, 2
        jz .mono
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        jmp .st
.s16:
        lodsw
        mov bx, ax
        test dl, 2
        jz .mono
        lodsw
        jmp .st
.mono:
        mov ax, bx
.st:
        pop si
        xchg ax, bx
        call vol_scale
        stosw                       ; izquierdo
        mov ax, bx
        call vol_scale_r
        stosw                       ; derecho
        inc bp
        ; avanzar en los datos: cur_rate tramas por cada hw_rate de salida
        movzx eax, word [cs:cur_rate]
        add eax, [cs:rs_acc]
        movzx ebx, word [cs:hw_rate]
.adv:
        cmp eax, ebx
        jb .na
        sub eax, ebx
        add si, cx
        sub [cs:p_left], cx
        cmp [cs:p_left], cx
        jae .adv
.na:
        mov [cs:rs_acc], eax
        jmp .l
.eof:
        mov word [cs:p_left], 0
.end:
        ; normalizar el puntero de origen
        mov ax, ds
        mov bx, si
        shr bx, 4
        add ax, bx
        and si, 15
        mov [cs:p_src], si
        mov [cs:p_src+2], ax
        mov bx, bp
        pop bp
        ret

; ---- AC'97 ----
; EAX <- dirección física de CS:BX
phys:
        push ebx
        movzx ebx, bx
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, ebx
        pop ebx
        cmp byte [cs:v86_xlat], 0
        je .r
        ; modo V86: la página puede estar en otra RAM (HSPSND cargado en la
        ; memoria superior): VCPI DE06h da su dirección física
        push ecx
        push edx
        push eax
        mov ecx, eax
        shr ecx, 12
        mov ax, 0xDE06
        int 0x67
        test ah, ah
        pop eax
        jnz .x
        and eax, 0xFFF
        and edx, 0xFFFFF000
        or eax, edx
.x:
        pop edx
        pop ecx
.r:
        ret

; CF = 1 si no se puede llegar a los registros del HD Audio (modo V86 sin la
; ventana de HSPEMM). Conserva todo.
hda_reach:
        push ax
        smsw ax
        test al, 1
        pop ax
        jz .r
        cmp word [cs:hda_wseg], 1
.r:
        ret

; poner en cola el búfer wr_idx con BX tramas
ac_queue:
        push ax
        push bx
        push dx
        push di

        movzx di, byte [cs:wr_idx]
        shl di, 3
        add di, [cs:bdl_off]
        push bx
        movzx bx, byte [cs:wr_idx]
        and bx, NBUF - 1
        shl bx, 13
        add bx, [cs:buf_off]
        call phys
        pop bx
        mov [cs:di], eax
        shl bx, 1                   ; muestras de 16 bits (2 por trama)
        mov [cs:di+4], bx
        mov word [cs:di+6], 0
        cmp byte [cs:started], 0
        jne .lvi
        ; reiniciar el canal de salida y arrancar
        mov dx, [cs:nabm]
        add dx, 0x1B
        mov al, 2
        out dx, al
        mov cx, 0xFFFF
.rw:
        in al, dx
        test al, 2
        loopnz .rw
        mov bx, [cs:bdl_off]
        call phys
        mov dx, [cs:nabm]
        add dx, 0x10
        out dx, eax
        mov dx, [cs:nabm]
        add dx, 0x15
        mov al, [cs:wr_idx]
        out dx, al
        mov dx, [cs:nabm]
        add dx, 0x1B
        mov al, 1
        out dx, al
        mov byte [cs:started], 1
        jmp .next
.lvi:
        mov dx, [cs:nabm]
        add dx, 0x15
        mov al, [cs:wr_idx]
        out dx, al
.next:
        inc byte [cs:wr_idx]
        and byte [cs:wr_idx], 31
        pop di
        pop dx
        pop bx
        pop ax
        ret

; esperar a que haya un búfer libre
ac_wait_slot:
        push ax
        push dx
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        mov dx, [cs:nabm]
        add dx, 0x16
        in ax, dx
        test al, 1                  ; DMA parado: todo libre
        jnz .r
        mov dx, [cs:nabm]
        add dx, 0x14
        in al, dx
        mov ah, [cs:wr_idx]
        sub ah, al
        and ah, 31
        cmp ah, NBUF
        jb .r
        call tmo_check
        jnc .l
.r:
        pop dx
        pop ax
        ret

; esperar a que termine la reproducción
ac_wait:
        push ax
        push dx
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        mov dx, [cs:nabm]
        add dx, 0x16
        in ax, dx
        test al, 1
        jnz .r
        call tmo_check
        jnc .l
.r:
        pop dx
        pop ax
        ret

ac_stop:
        push ax
        push dx
        mov dx, [cs:nabm]
        add dx, 0x1B
        xor al, al
        out dx, al
        mov al, 2
        out dx, al
        mov byte [cs:started], 0
        mov byte [cs:wr_idx], 0
        pop dx
        pop ax
        ret

ac_set_rate:
        push ax
        push dx
        mov word [cs:hw_rate], 48000
        mov dword [cs:rs_acc], 0
        cmp byte [cs:vra], 0
        je .r                       ; 48 kHz fijos: se cambia por programa
        mov ax, [cs:cur_rate]
        mov dx, [cs:nam]
        add dx, 0x2C
        out dx, ax
        in ax, dx                   ; la que el códec ha puesto de verdad
        test ax, ax
        jz .r
        mov [cs:hw_rate], ax
.r:
        pop dx
        pop ax
        ret

; plazo máximo de unos 20 segundos (ticks de la BIOS)
tmo_start:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        mov [cs:tmo_t0], ax
        pop ax
        pop ds
        ret
tmo_check:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        sub ax, [cs:tmo_t0]
        cmp ax, 364
        pop ax
        pop ds
        cmc
        ret                         ; CF=1 si se agotó el plazo
tmo_t0      dw 0

; =============================================================================
;  Intel HD Audio
; =============================================================================
; FS = segmento plano de 4 GB (modo real "irreal")
unreal:
        push eax
        push bx
        smsw ax
        test al, 1                  ; en modo V86: la ventana de HSPEMM
        jnz .v86
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
.r:
        pop bx
        pop eax
        ret
.v86:
        cmp word [cs:hda_wseg], 0
        je .r
        mov fs, [cs:hda_wseg]
        jmp .r


; ---- línea A20 ----
; Los registros del controlador están por encima de 1 MB, casi siempre en una
; dirección con el bit 20 a uno: con A20 cerrada (la deja así cualquier
; programa que la abra y la cierre por el XMS, y hay BIOS que arrancan con
; ella cerrada) se leería y se escribiría en otra dirección. Se abre mientras
; el driver trabaja y se deja como estaba. Conservan todos los registros y
; los indicadores.
a20_open:
        pushf
        push ax
        cli
        inc byte [cs:a20_depth]
        cmp byte [cs:a20_depth], 1
        jne .r
        call a20_test
        jnz .r
        mov byte [cs:a20_closed], 1
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
        dec byte [cs:a20_depth]
        jnz .r
        cmp byte [cs:a20_closed], 0
        je .r
        mov byte [cs:a20_closed], 0
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
        xor al, al                  ; ZF = 1
        jmp .x
.open:
        or al, 1                    ; ZF = 0
.x:
        pop ax
        pop es
        pop ds
        ret
a20_depth   db 0
a20_closed  db 0

; registros: DX = desplazamiento
hrd32:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov eax, [fs:esi]
        pop esi
        ret
hwr32:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov [fs:esi], eax
        pop esi
        ret
hrd16:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov ax, [fs:esi]
        pop esi
        ret
hwr16:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov [fs:esi], ax
        pop esi
        ret
hrd8:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov al, [fs:esi]
        pop esi
        ret
hwr8:
        push esi
        movzx esi, dx
        add esi, [cs:hda_reg]
        mov [fs:esi], al
        pop esi
        ret

; EAX = orden para el códec (sin la dirección) -> EAX = respuesta; CF si falla
hda_cmd:
        push ecx
        push edx
        push ebx
        movzx ebx, byte [cs:hda_cad]
        shl ebx, 28
        or eax, ebx
        mov ebx, eax
        mov ecx, 0x100000
.w1:
        mov dx, 0x68
        call hrd16
        test al, 1
        jz .free
        loop .w1, ecx
        jmp .fail
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
        mov ecx, 0x100000
.w2:
        mov dx, 0x68
        call hrd16
        test al, 2
        jnz .got
        loop .w2, ecx
        jmp .fail
.got:
        mov dx, 0x64
        call hrd32
        push eax
        mov dx, 0x68
        mov ax, 2
        call hwr16
        pop eax
        pop ebx
        pop edx
        pop ecx
        clc
        ret
.fail:
        pop ebx
        pop edx
        pop ecx
        stc
        ret

; BL = nodo, AX = verbo de 12 bits, CL = dato -> EAX = respuesta
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
; BL = nodo, AL = verbo de 4 bits, CX = dato de 16 bits
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

; formato del flujo para los datos de frecuencia DX (16 bits estéreo): esa
; misma si los DAC la admiten (hda_rates); si no, 44,1 kHz para 11,025 y
; 22,05 kHz y 48 kHz para las demás, y fill_buffer cambia la frecuencia
hda_rate_fmt:
        push si
        push bx
        mov bx, [cs:hda_rates]
        mov si, rate_tab
.l:
        mov ax, [cs:si]
        test ax, ax
        jz .alt
        cmp ax, dx
        jne .n
        test bx, [cs:si+4]
        jnz .got
.n:
        add si, 6
        jmp .l
.alt:
        mov si, rt_441
        cmp dx, 11025
        je .a1
        cmp dx, 22050
        jne .a2
.a1:
        test bl, 0x20
        jnz .got
.a2:
        mov si, rt_48
        test bl, 0x40
        jnz .got
        test bl, 0x20
        jz .got
        mov si, rt_441
.got:
        mov ax, [cs:si]
        mov [cs:hw_rate], ax
        mov dword [cs:rs_acc], 0
        ; ticks de una vuelta: NBUF*BUFSZ/4 tramas / Hz * 18,2
        push dx
        mov dx, 2
        mov ax, 0x466D              ; 149101
        div word [cs:si]
        mov [cs:hda_lapt], ax
        pop dx
        mov ax, [cs:si+2]
        or ax, 0x0011
        mov [cs:hda_fmt], ax
        pop bx
        pop si
        ret
; frecuencia, formato del flujo, bit del parámetro 0Ah
rate_tab    dw 8000, 0x0500, 0x01, 11025, 0x4300, 0x02, 16000, 0x0200, 0x04
            dw 22050, 0x4100, 0x08, 32000, 0x0A00, 0x10
rt_441      dw 44100, 0x4000, 0x20
rt_48       dw 48000, 0x0000, 0x40
            dw 0

; posición reproducida (bytes, sin vueltas perdidas si se consulta a menudo)
hda_update:
        push eax
        push dx
        ; si ha pasado una vuelta entera del anillo sin consultar, la cuenta
        ; de vueltas ya no vale: se da todo por reproducido
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        mov dx, ax
        sub dx, [cs:hda_tick]
        mov [cs:hda_tick], ax
        cmp dx, [cs:hda_lapt]
        jb .pos
        mov eax, [cs:hda_written]
        mov [cs:hda_played], eax
        jmp .r
.pos:
        mov dx, [cs:hda_sd]
        add dx, 4
        call hrd32
        cmp eax, [cs:hda_last]
        jae .s
        add dword [cs:hda_cycles], NBUF * BUFSZ
.s:
        mov [cs:hda_last], eax
        add eax, [cs:hda_cycles]
        mov [cs:hda_played], eax
.r:
        pop dx
        pop eax
        ret
hda_tick    dw 0
hda_lapt    dw 3                    ; ticks de una vuelta del anillo

hda_play:
        call unreal
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:cur_rate]
        je .loop
        call hda_wait
        call hda_stop
        mov [cs:cur_rate], dx
        call hda_rate_fmt
        call hda_setfmt
.loop:
        cmp word [cs:p_left], 0
        je .done
        ; esperar a que quede un búfer libre en el anillo
        cmp byte [cs:started], 0
        je .fill
        call tmo_start
.ws:
        call hda_update
        mov eax, [cs:hda_written]
        sub eax, [cs:hda_played]
        jbe .under
        cmp eax, (NBUF - 1) * BUFSZ
        jbe .fill
        call tmo_check
        jnc .ws
        jmp .fill
.under:
        ; el flujo ya ha pasado por todo lo encolado (el programa ha tardado
        ; en traer más: un disquete, un CD): se para y se empieza de nuevo.
        ; Antes la resta salía «negativa» y se esperaba el plazo entero (20 s)
        call hda_stop
.fill:
        mov eax, [cs:hda_written]
        shr eax, 13
        mov [cs:wr_idx], al
        call fill_buffer
        test bx, bx
        jz .done
        ; rellenar con silencio hasta el final del búfer
        mov cx, FRAMES
        sub cx, bx
        shl cx, 1
        xor ax, ax
        rep stosw
        add dword [cs:hda_written], BUFSZ
        cmp byte [cs:started], 0
        jne .loop
        call hda_start
        jmp .loop
.done:
        pop es
        pop ds
        popad
        clc
        retf

; arrancar el flujo de salida sobre el anillo de NBUF búferes
hda_start:
        pushad
        mov dx, [cs:hda_sd]
        xor al, al
        call hwr8
        mov al, 1                   ; reinicio del flujo
        call hwr8
        mov cx, 0xFFFF
.r1:
        call hrd8
        test al, 1
        loopz .r1
        xor al, al
        call hwr8
        mov cx, 0xFFFF
.r2:
        call hrd8
        test al, 1
        loopnz .r2
        ; lista de descriptores: 32 entradas de 1 KB sobre el anillo (así
        ; ninguna cruza una página: en modo V86 las páginas pueden no ir
        ; seguidas en la memoria física). Con la Sound Blaster emulada, cada
        ; una avisa al acabar (IOC).
        mov di, [cs:bdl_off]
        xor si, si
.bd:
        mov bx, si
        shl bx, 10
        add bx, [cs:buf_off]
        call phys
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        mov dword [cs:di+8], 1024
        movzx eax, byte [cs:e_on]
        mov [cs:di+12], eax
        add di, 16
        inc si
        cmp si, 32
        jb .bd
        mov bx, [cs:bdl_off]
        call phys
        mov dx, [cs:hda_sd]
        add dx, 0x18
        call hwr32
        add dx, 4
        xor eax, eax
        call hwr32
        mov dx, [cs:hda_sd]
        add dx, 0x08
        mov eax, NBUF * BUFSZ
        call hwr32
        mov dx, [cs:hda_sd]
        add dx, 0x0C
        mov ax, 31
        call hwr16
        mov dx, [cs:hda_sd]
        add dx, 0x12
        mov ax, [cs:hda_fmt]
        call hwr16
        mov dx, [cs:hda_sd]
        add dx, 2
        mov al, 0x10                ; flujo número 1
        call hwr8
        mov dx, [cs:hda_sd]
        mov al, [cs:e_on]
        shl al, 2                   ; IOCE (Sound Blaster emulada)
        or al, 2                    ; RUN
        call hwr8
        mov byte [cs:started], 1
        xor eax, eax
        mov [cs:hda_played], eax
        mov [cs:hda_last], eax
        mov [cs:hda_cycles], eax
        push ds
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        mov [cs:hda_tick], ax
        popad
        ret

; formato del flujo en todos los DAC en uso
hda_setfmt:
        push ax
        push bx
        push cx
        push si
        mov si, hda_dacs
.l:
        mov bl, [cs:si]
        test bl, bl
        jz .r
        mov al, 2
        mov cx, [cs:hda_fmt]
        call hda_verb4
        inc si
        cmp si, hda_dacs + 4
        jb .l
.r:
        pop si
        pop cx
        pop bx
        pop ax
        ret

hda_wait:
        cmp byte [cs:started], 0
        je .r
        push eax
        call tmo_start
.l:
        call hda_update
        mov eax, [cs:hda_played]
        cmp eax, [cs:hda_written]
        jae .end
        call tmo_check
        jnc .l
.end:
        call hda_stop
        pop eax
.r:
        ret

hda_stop:
        push ax
        push dx
        mov dx, [cs:hda_sd]
        xor al, al
        call hwr8
        mov byte [cs:started], 0
        mov dword [cs:hda_written], 0
        ; vaciar el anillo para que no suene lo viejo al volver a empezar
        push cx
        push di
        push es
        push cs
        pop es
        mov di, [cs:buf_off]
        mov cx, NBUF * BUFSZ / 2
        xor ax, ax
        rep stosw
        pop es
        pop di
        pop cx
        pop dx
        pop ax
        ret

; =============================================================================
;  Sound Blaster (2.0, Pro y 16) - parte residente
; =============================================================================
; 4 bloques de SB_BLK octetos seguidos que no cruzan una frontera de 64 KB (la
; DMA del ISA no puede). DMA de autoinicio sobre los 4 bloques; la tarjeta
; avisa (IRQ) al acabar cada bloque: se llena de silencio y queda libre.
; Sin datos durante SB_IDLE bloques se para (reset del DSP), así la tarjeta
; queda libre para los juegos.
SB_BLK      equ 4096
SB_IDLE     equ 8

sb_base     dw 0
sb_irq      db 0
sb_dma      db 1
sb_hdma     db 0                    ; 0 = sin DMA de 16 bits
sb_minor    db 0                    ; (seguidos: se comparan como palabra)
sb_major    db 0
sb_vec      db 0
sb_play     db 0                    ; bloque que suena
sb_wr       db 0                    ; siguiente bloque que se llena
sb_queued   db 0                    ; bloques con datos sin acabar
sb_idle     db 0
sb_full     db 0, 0, 0, 0
sb_hs       db 0                    ; modo de alta velocidad (8 bits)
sb_old      dd 0
sb_req      dw 0
sb_nirq     dw 0                    ; [SONIDO-JUEGOS] interrupciones atendidas (/INFO)
name_sb     db "Sound Blaster", 0
name_sb16   db "Sound Blaster 16", 0

; ---- DSP ----
; AL -> DSP (CF si no responde)
sb_out:
        push cx
        push dx
        push ax
        mov dx, [cs:sb_base]
        add dx, 0x0C
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jz .ok
        loop .w
        pop ax
        stc
        jmp .r
.ok:
        pop ax
        out dx, al
        clc
.r:
        pop dx
        pop cx
        ret
; DSP -> AL (CF si no responde)
sb_in:
        push cx
        push dx
        mov dx, [cs:sb_base]
        add dx, 0x0E
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .ok
        loop .w
        stc
        jmp .r
.ok:
        sub dx, 4
        in al, dx
        clc
.r:
        pop dx
        pop cx
        ret
; reiniciar el DSP (CF si no hay DSP en sb_base)
sb_reset:
        push ax
        push cx
        push dx
        mov dx, [cs:sb_base]
        add dx, 6
        mov al, 1
        out dx, al
        mov cx, 16                  ; > 3 microsegundos
.d:
        in al, dx
        loop .d
        xor al, al
        out dx, al
        mov cx, 8
.t:
        call sb_in
        jc .n
        cmp al, 0xAA
        je .ok
.n:
        loop .t
        stc
        jmp .r
.ok:
        clc
.r:
        pop dx
        pop cx
        pop ax
        ret
; escribir un registro del mezclador: AH = registro, AL = valor
sb_mix:
        push dx
        mov dx, [cs:sb_base]
        add dx, 4
        xchg al, ah
        out dx, al
        inc dx
        xchg al, ah
        out dx, al
        pop dx
        ret

; dirección del bloque AL -> DI (en CS)
sb_blk_di:
        movzx di, al
        and di, 3
        shl di, 12
        add di, [cs:buf_off]
        ret

; llenar de silencio el bloque AL
sb_silence:
        push ax
        push cx
        push di
        push es
        call sb_blk_di
        push cs
        pop es
        mov ax, 0x8080              ; 8 bits sin signo
        cmp byte [cs:dev_type], 2
        jne .f
        xor ax, ax                  ; 16 bits con signo
.f:
        mov cx, SB_BLK / 2
        cld
        rep stosw
        pop es
        pop di
        pop cx
        pop ax
        ret

; parar: reset del DSP, canal DMA tapado y bloques en silencio
sb_halt:
        push ax
        push dx
        pushf
        cli
        call sb_reset
        call sb_dma_mask
        mov byte [cs:started], 0
        mov byte [cs:sb_queued], 0
        mov dword [cs:sb_full], 0
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
        xor al, al
.s:
        call sb_silence
        inc al
        cmp al, 4
        jb .s
        popf
        pop dx
        pop ax
        ret

; tapar el canal de DMA que se usa
sb_dma_mask:
        push ax
        cmp byte [cs:dev_type], 2
        je .h
        mov al, [cs:sb_dma]
        or al, 4
        out 0x0A, al
        pop ax
        ret
.h:
        mov al, [cs:sb_hdma]
        and al, 3
        or al, 4
        out 0xD4, al
        pop ax
        ret

; poner en marcha la DMA y el DSP sobre los 4 bloques (cur_rate, dev_type)
sb_start:
        pushad
        call sb_reset
        jc .r
        ; la IRQ puede estar tapada: lo hacen los juegos al salir, y la rutina
        ; por omisión de la BIOS con una interrupción que llegue estando parados
        call sb_unmask
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ebx, word [cs:buf_off]
        add eax, ebx                ; dirección física de los bloques
        cmp byte [cs:dev_type], 2
        je .d16
        ; --- DMA de 8 bits (canales 0-3) ---
        mov bl, [cs:sb_dma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0x0A, al                ; tapar
        out 0x0C, al                ; biestable a cero
        mov al, 0x58                ; lectura, autoinicio, uno a uno
        or al, bl
        out 0x0B, al
        movzx dx, bl
        shl dx, 1                   ; puerto de dirección
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ecx, word [cs:buf_off]
        add eax, ecx
        out dx, al
        mov al, ah
        out dx, al
        shr eax, 16
        push ax
        inc dx                      ; puerto de cuenta
        mov ax, 4 * SB_BLK - 1
        out dx, al
        mov al, ah
        out dx, al
        pop ax
        movzx bx, bl
        mov dl, [cs:pg8+bx]
        xor dh, dh
        out dx, al                  ; página
        mov al, [cs:sb_dma]
        and al, 3
        out 0x0A, al                ; destapar
        ; --- DSP de 8 bits ---
        mov al, 0xD1                ; altavoz
        call sb_out
        ; constante de tiempo: 256 - 1000000 / Hz
        mov eax, 1000000
        xor edx, edx
        movzx ecx, word [cs:cur_rate]
        div ecx
        mov ah, 0
        neg al                      ; 256 - x (en 8 bits)
        push ax
        mov al, 0x40
        call sb_out
        pop ax
        call sb_out
        cmp byte [cs:sb_major], 2
        jae .auto
        call sb_single              ; DSP 1.x: no tiene autoinicio
        jmp .on
.auto:
        mov al, 0x48                ; tamaño del bloque
        call sb_out
        mov al, (SB_BLK - 1) & 0xFF
        call sb_out
        mov al, (SB_BLK - 1) >> 8
        call sb_out
        mov al, 0x1C                ; autoinicio, normal
        cmp byte [cs:sb_hs], 0
        je .g8
        mov al, 0x90                ; autoinicio, alta velocidad
.g8:
        call sb_out
        jmp .on
.d16:
        ; --- DMA de 16 bits (canales 5-7): en palabras ---
        mov bl, [cs:sb_hdma]
        and bl, 3
        mov al, bl
        or al, 4
        out 0xD4, al
        out 0xD8, al
        mov al, 0x58
        or al, bl
        out 0xD6, al
        movzx dx, bl
        shl dx, 2
        add dx, 0xC0                ; dirección
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx ecx, word [cs:buf_off]
        add eax, ecx
        mov ecx, eax
        shr eax, 1
        out dx, al
        mov al, ah
        out dx, al
        add dx, 2                   ; cuenta
        mov ax, 4 * SB_BLK / 2 - 1
        out dx, al
        mov al, ah
        out dx, al
        shr ecx, 16
        and cl, 0xFE
        movzx bx, bl
        mov dl, [cs:pg16+bx]
        xor dh, dh
        mov al, cl
        out dx, al
        mov al, [cs:sb_hdma]
        and al, 3
        out 0xD4, al
        ; --- DSP 4.x: frecuencia de salida y 16 bits estéreo ---
        mov al, 0x41
        call sb_out
        mov ax, [cs:cur_rate]
        xchg al, ah
        call sb_out
        xchg al, ah
        call sb_out
        mov al, 0xB6                ; 16 bits, salida, autoinicio, FIFO
        call sb_out
        mov al, 0x30                ; estéreo con signo
        call sb_out
        mov ax, SB_BLK / 2 - 1      ; muestras por bloque
        call sb_out
        mov al, ah
        call sb_out
.on:
        mov byte [cs:started], 1
        mov byte [cs:sb_idle], 0
.r:
        popad
        ret
pg8         db 0x87, 0x83, 0x81, 0x82
pg16        db 0x8F, 0x8B, 0x89, 0x8A

; DSP 1.x (SB 1.0/1.5): un bloque de ciclo único (orden 14h); la interrupción
; pide el siguiente. La DMA sí va en autoinicio sobre los 4 bloques.
sb_single:
        push ax
        mov al, 0x14
        call sb_out
        mov al, (SB_BLK - 1) & 0xFF
        call sb_out
        mov al, (SB_BLK - 1) >> 8
        call sb_out
        pop ax
        ret

; destapar la IRQ de la tarjeta en el PIC
sb_unmask:
        push ax
        push cx
        pushf
        cli
        mov cl, [cs:sb_irq]
        cmp cl, 8
        jae .slave
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0x21
        and al, ah
        out 0x21, al
        jmp .r
.slave:
        sub cl, 8
        mov ah, 1
        shl ah, cl
        not ah
        in al, 0xA1
        and al, ah
        out 0xA1, al
        in al, 0x21
        and al, 0xFB                ; cascada
        out 0x21, al
.r:
        popf
        pop cx
        pop ax
        ret

; quedar residente (DX = párrafos). Los bloques de DMA pueden caer encima del
; código de instalación (van justo detrás de resident_end): se llenan de
; silencio aquí, cuando ese código ya ha terminado, y no antes.
sb_tsr:
        call sb_halt
        mov ax, 0x3100
        int 0x21

; ---- interrupción de la tarjeta: ha acabado el bloque sb_play ----
sb_isr:
        cmp byte [cs:started], 0
        jne .ours
        jmp far [cs:sb_old]
.ours:
        push ax
        push dx
        ; IRQ 7 / 15 espuria (el PIC la inventa si la petición desaparece):
        ; no está «en servicio» y no lleva EOI (quitaría el de otra)
        cmp byte [cs:sb_irq], 7
        jne .n7
        mov al, 0x0B
        out 0x20, al
        in al, 0x20
        mov ah, al
        mov al, 0x0A                ; dejar el PIC leyendo IRR, como estaba
        out 0x20, al
        test ah, 0x80
        jnz .real
        jmp .spur
.n7:
        cmp byte [cs:sb_irq], 15
        jne .real
        mov al, 0x0B
        out 0xA0, al
        in al, 0xA0
        mov ah, al
        mov al, 0x0A
        out 0xA0, al
        test ah, 0x80
        jnz .real
        mov al, 0x20
        out 0x20, al                ; solo al maestro
.spur:
        pop dx
        pop ax
        iret
.real:
        mov dx, [cs:sb_base]
        add dx, 0x0E                ; acuse de 8 bits
        cmp byte [cs:dev_type], 2
        jne .ack
        inc dx                      ; acuse de 16 bits
.ack:
        in al, dx
        inc word [cs:sb_nirq]       ; [SONIDO-JUEGOS] para el diagnóstico de /INFO
        push bx
        movzx bx, byte [cs:sb_play]
        cmp byte [cs:sb_full+bx], 0
        je .e
        mov byte [cs:sb_full+bx], 0
        dec byte [cs:sb_queued]
        mov byte [cs:sb_idle], 0
        jmp .z
.e:
        inc byte [cs:sb_idle]
.z:
        mov al, bl
        call sb_silence
        inc bl
        and bl, 3
        mov [cs:sb_play], bl
        ; sin nada en cola: no escribir en el que suena
        cmp byte [cs:sb_queued], 0
        jne .k
        cmp [cs:sb_wr], bl
        jne .k
        inc bl
        and bl, 3
        mov [cs:sb_wr], bl
.k:
        pop bx
        cmp byte [cs:sb_idle], SB_IDLE
        jb .more
        call sb_halt                ; un rato sin datos: a descansar
        jmp .eoi
.more:
        cmp byte [cs:dev_type], 1
        jne .eoi
        cmp byte [cs:sb_major], 2
        jae .eoi
        call sb_single              ; DSP 1.x: pedir el bloque siguiente
.eoi:
        mov al, 0x20
        cmp byte [cs:sb_irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop dx
        pop ax
        iret

; ---- reproducir (desde f_play, con pushad/ds/es ya guardados) ----
sb_play_f:
        mov [cs:p_fmt], al
        mov [cs:p_left], cx
        mov [cs:p_src], si
        mov [cs:p_src+2], ds
        cmp dx, [cs:sb_req]
        je .rok
        mov [cs:sb_req], dx
        call sb_wait
        call sb_halt
        ; 8 bits: hasta 22050 Hz modo normal, por encima alta velocidad
        cmp dx, 4000
        jae .lo
        mov dx, 4000
.lo:
        cmp dx, 44100
        jbe .hi
        mov dx, 44100
.hi:
        mov [cs:cur_rate], dx
        mov byte [cs:sb_hs], 0
        cmp byte [cs:dev_type], 2
        je .rok
        cmp dx, 22050
        jbe .rok
        cmp word [cs:sb_minor], 0x0201  ; (menor, mayor)
        jb .slow                    ; DSP 1.x y 2.00: sin alta velocidad
        mov byte [cs:sb_hs], 1
        jmp .rok
.slow:
        mov word [cs:cur_rate], 22050
.rok:
.loop:
        cmp word [cs:p_left], 0
        je .done
        call sb_wait_slot
        jc .done
        cli
        call sb_fill
        test bx, bx
        jz .dn
        movzx bx, byte [cs:sb_wr]
        mov byte [cs:sb_full+bx], 1
        inc byte [cs:sb_queued]
        inc bl
        and bl, 3
        mov [cs:sb_wr], bl
        cmp byte [cs:started], 0
        jne .go
        call sb_start
.go:
        sti
        jmp .loop
.dn:
        sti
.done:
        pop es
        pop ds
        popad
        clc
        retf

; esperar un bloque libre (CF si vence el plazo)
sb_wait_slot:
        push bx
        cmp byte [cs:started], 0
        jne .s
        ; parada: se empieza por el bloque 0
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
        clc
        jmp .r
.s:
        call tmo_start
.l:
        sti
        movzx bx, byte [cs:sb_wr]
        cmp byte [cs:sb_full+bx], 0
        jne .w
        cmp bl, [cs:sb_play]
        jne .ok
.w:
        cmp byte [cs:started], 0
        je .s0
        call tmo_check
        jnc .l
        jmp .r
.s0:
        mov byte [cs:sb_play], 0
        mov byte [cs:sb_wr], 0
.ok:
        clc
.r:
        pop bx
        ret

; esperar a que acabe lo que hay en cola
sb_wait:
        cmp byte [cs:started], 0
        je .r
        call tmo_start
.l:
        sti
        cmp byte [cs:sb_queued], 0
        je .r
        call tmo_check
        jnc .l
.r:
        ret

; p_src -> bloque sb_wr: 16 bits estéreo (SB16) u 8 bits mono -> BX tramas
sb_fill:
        mov cl, 1
        test byte [cs:p_fmt], 1
        jz .b
        shl cl, 1
.b:
        test byte [cs:p_fmt], 2
        jz .c
        shl cl, 1
.c:
        movzx cx, cl
        mov ax, [cs:p_left]
        xor dx, dx
        div cx                      ; tramas disponibles
        mov bx, SB_BLK / 4
        cmp byte [cs:dev_type], 2
        je .m
        mov bx, SB_BLK
.m:
        cmp ax, bx
        jbe .n
        mov ax, bx
.n:
        mov bx, ax
        test ax, ax
        jnz .go
        mov word [cs:p_left], 0
        ret
.go:
        push bx
        mul cx
        sub [cs:p_left], ax
        mov al, [cs:sb_wr]
        call sb_blk_di
        push cs
        pop es
        lds si, [cs:p_src]
        mov cx, bx
        mov dl, [cs:p_fmt]
        mov dh, [cs:dev_type]
        cld
.l:
        ; una trama -> AX izquierdo, BX derecho (16 bits con signo)
        test dl, 1
        jnz .s16
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        mov bx, ax
        test dl, 2
        jz .mono
        lodsb
        sub al, 0x80
        mov ah, al
        xor al, al
        xchg ax, bx
        jmp .put
.s16:
        lodsw
        mov bx, ax
        test dl, 2
        jz .mono
        lodsw
        xchg ax, bx
        jmp .put
.mono:
        mov ax, bx
.put:
        call vol_scale
        xchg ax, bx
        call vol_scale_r
        xchg ax, bx
        cmp dh, 2
        jne .p8
        stosw
        mov ax, bx
        stosw
        loop .l
        jmp .end
.p8:
        sar ax, 1                   ; media de los dos canales
        sar bx, 1
        add ax, bx
        mov al, ah
        add al, 0x80
        stosb
        loop .l
.end:
        mov ax, ds
        mov bx, si
        shr bx, 4
        add ax, bx
        and si, 15
        mov [cs:p_src], si
        mov [cs:p_src+2], ax
        pop bx
        ret

resident_end:

; =============================================================================
;  Sound Blaster emulada (/SB): solo queda residente si se activa; si no, los
;  búferes van encima
; =============================================================================
%include "hspsb.inc"
%include "hspopl.inc"
sbemu_end:

; =============================================================================
;  Instalación (lo que sigue se descarta)
; =============================================================================
init:
        ; Cargado con LH en un bloque de memoria superior pequeño, los
        ; bloques de DMA (que van detrás del código residente) pueden no
        ; caber: antes de escribir nada, si los de la Sound Blaster no caben,
        ; el programa se vuelve a cargar a sí mismo en la memoria
        ; convencional. (Sin esto pisaba lo que hubiera detrás de su bloque:
        ; la cadena de bloques de memoria, y el sistema se detenía.)
        call sb_layout
        call fit_check
        ; (la copia que se carga abajo lleva la opción interna /~: el
        ; intérprete, a quien hay que ponerle BLASTER, es su abuelo; con
        ; /~~, además, el título ya está puesto y no lo repite)
        mov si, 0x81
.tl:
        lodsb
        cmp al, 13
        je .title
        cmp al, '~'
        jne .tl
        mov byte [relaunched], 1
        cmp byte [si], '~'
        je .notitle
.title:
        mov si, msg_title
        call puts
.notitle:
        mov byte [title_done], 1
        mov si, msg_empty
        call puts
        call parse_opts
        cmp byte [opt_help], 0
        je .nh
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
.nh:
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        je installed
        cmp byte [have_vol], 0
        je .notinst
        mov al, [opt_vol]
        mov [vol_pct], al
.notinst:
        ; ¿modo V86? ¿con HSPEMM y sus servicios para HSPSND?
        call v86_probe
        cmp byte [opt_sb], 0
        je .lay
        cmp byte [v86_svc], 0
        jne .lay
        mov byte [opt_sb], 0
        mov si, msg_sb_noemm
        call puts
.lay:
        call layout
        ; Sound Blaster (ISA) primero, salvo con /NOSB
        cmp byte [opt_nosb], 0
        jne .pci
        call sb_detect
        jc .pci
        call sb_install
        jmp .install
.pci:
        ; (los búferes de HD Audio y AC'97 son mayores: ¿caben?)
        call layout
        call fit_check
        ; ¿hay BIOS PCI?
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .nopci
        cmp edx, 0x20494350
        jne .nopci
        ; lo aprendido en la prueba de sonido (SONIDO.INI) va primero: si la
        ; tarjeta de este equipo está, se pone exactamente ese camino
        call ini_load
        call hda_learn_pass
        jnc .hok
        call lrn_say
        ; buscar primero HD Audio y después AC'97
        jmp .noac97
.try_ac97:
        ; AC'97: clase 04h, subclase 01h
        mov ax, 0xB103
        mov ecx, 0x040100
        xor si, si
        int 0x1A
        jc .nopci
        mov [pci_bdf], bx
        ; VIA y SiS tienen otros registros: no son «compatibles con ICH»
        mov ax, 0xB109
        xor di, di
        int 0x1A
        cmp cx, 0x1106
        je .nopci
        cmp cx, 0x1039
        je .nopci
        call ac97_init
        jc .fail
        call ac97_learn
        mov byte [dev_type], 3
        ; sin ventana para los registros del HD Audio
        mov byte [lay_win], 0
        call layout
        mov si, msg_ac97
        call puts
        mov ax, [nam]
        call hex4
        mov al, '/'
        call putc
        mov ax, [nabm]
        call hex4
        mov si, msg_vra
        cmp byte [vra], 0
        jne .v
        mov si, msg_novra
.v:
        call puts
        jmp .install
.noac97:
        ; puede haber varios (el de la placa y el HDMI de la gráfica): se
        ; toma el primero que tenga un códec con salida
        mov ax, 0xB103
        mov ecx, 0x040300
        mov si, [hda_idx]
        int 0x1A
        jc .nohda
        mov [pci_bdf], bx
        call hda_init
        jnc .hok
        inc word [hda_idx]
        cmp word [hda_idx], 4
        jb .noac97
        jmp .hfail
.nohda:
        cmp word [hda_idx], 0
        je .try_ac97
        jmp .hfail
.hok:
        mov byte [dev_type], 4
        call lrn_say
        mov si, msg_hda
        call puts
        mov eax, [hda_base]
        shr eax, 16
        call hex4
        mov eax, [hda_base]
        call hex4
        mov si, msg_codec
        call puts
        mov al, [hda_cad]
        add al, '0'
        call putc
        mov si, msg_pin
        call puts
        mov si, hda_pins
        movzx cx, byte [hda_npin]
        jcxz .np
.pl:
        mov al, ' '
        call putc
        mov al, [si]
        call hex2
        add si, 5
        loop .pl
.np:
        mov si, msg_crlf2
        call puts
        jmp .install
.hfail:
        mov si, msg_hdafail
        call puts
        ; datos para el diagnóstico: controlador, registros y códecs vistos
        mov si, msg_i_pci + 2
        call puts
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        xor di, di
        int 0x1A
        mov ax, cx
        call hex4
        mov al, ':'
        call putc
        shr ecx, 16
        mov ax, cx
        call hex4
        mov si, msg_i_reg
        call puts
        mov eax, [hda_base]
        call hex8
        mov si, msg_i_sts
        call puts
        mov ax, [statests]
        call hex4
        mov si, msg_crlf2
        call puts
        jmp .nocard2
.nocard:
        mov ax, 0xB103
        mov ecx, 0x040100
        xor si, si
        int 0x1A
        jnc .try_ac97
.nopci:
        mov si, msg_nocard
        call puts
.nocard2:
        mov ax, 0x4C02
        int 0x21
.fail:
        mov si, msg_fail
        call puts
        mov ax, 0x4C03
        int 0x21
.install:
        ; Sound Blaster emulada: solo si no hay una de verdad
        cmp byte [dev_type], 3
        jae .sbe
        cmp byte [opt_sb], 0
        je .sbe
        mov byte [opt_sb], 0
        mov si, msg_sb_real
        call puts
.sbe:
        ; (con /NOSB no se ha mirado si BLASTER ya está en el entorno)
        cmp byte [opt_sb], 0
        je .sbe2
        cmp byte [opt_nosb], 0
        je .sbe2
        call sb_parse_env
.sbe2:
        ; volumen inicial (las SB sin mezclador, por programa)
        cmp byte [dev_type], 2
        ja .hw
        cmp byte [sb_major], 3
        jae .hw
        mov byte [hw_vol], 0
.hw:
        call mz_iniciar             ; el mezclador: lo que hay y SONIDO.INI
        call vol_apply
        mov al, [opt_nokeys]
        xor al, 1
        mov [keys_on], al
        cmp byte [opt_info], 0
        je .ni
        call show_info
        jmp .keys
.ni:
        call show_vol
.keys:
        ; teclas de volumen
        cmp byte [opt_nokeys], 0
        jne .nk
        mov ax, 0x3515
        int 0x21
        mov [old15], bx
        mov [old15+2], es
        mov dx, int15
        mov ax, 0x2515
        int 0x21
.nk:
        ; enganchar INT 2Fh y quedar residente
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        ; liberar el entorno
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, [tsr_end]
        add dx, 15
        shr dx, 4
        cmp byte [dev_type], 1
        je sb_tsr
        cmp byte [dev_type], 2
        je sb_tsr
        cmp byte [opt_sb], 0
        je .tsr
        mov [e_tsrpar], dx
        mov byte [e_sbres], 1
        call e_prepare
        jmp e_go                    ; (residente: el anillo cae encima de esto)
.tsr:
        mov ax, 0x3100
        int 0x21

; ---- modo V86: ¿está HSPEMM con sus servicios (INT 67h AH=DDh)? ¿VCPI? ----
v86_probe:
        smsw ax
        test al, 1
        jz .r
        push es
        xor ax, ax
        mov es, ax
        mov eax, [es:0x67*4]
        pop es
        test eax, eax
        jz .r
        mov ax, 0xDD00
        xor bx, bx
        int 0x67
        test ah, ah
        jnz .vcpi
        cmp bx, 0x5342
        jne .vcpi
        mov byte [v86_svc], 1
        mov byte [v86_xlat], 1
        ret
.vcpi:
        mov ax, 0xDE00
        int 0x67
        test ah, ah
        jnz .r
        mov byte [v86_xlat], 1
.r:
        ret

; ---- dónde van la lista de descriptores y el anillo (tras lo residente) ----
; HD Audio pide 128 bytes de alineación en la dirección física.
; Con HSPEMM (modo V86): la lista y el anillo alineados a 1 KB (ninguna
; entrada cruza una página) y, detrás (más allá del final de este programa:
; no puede caer encima del código que se está ejecutando), una página entera
; para la ventana de los registros del HD Audio (lay_win).
; Con la Sound Blaster emulada, todo en páginas enteras: el estado (3 KB) y
; la lista (1 KB) en una, el anillo en las ocho siguientes y la ventana.
; Así ninguna de esas páginas tiene código: escribir a cada muestra en una
; página con código (o que lo tuvo: los emuladores guardan su traducción
; hasta que se pisa) es lentísimo.
layout:
        ; si ya no hace falta la ventana, se deshace
        cmp byte [lay_win], 0
        jne .k
        call win_unmap
.k:
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        cmp byte [v86_svc], 0
        je .real
        cmp byte [opt_sb], 0
        jne .sb
        add eax, resident_end + 1023
        and eax, 0xFFFFFC00
        sub eax, ebx
        mov [bdl_off], ax
        add ax, 1024
        mov [buf_off], ax
        add ax, NBUF * BUFSZ
        mov [tsr_end], ax
        cmp byte [lay_win], 0
        je .r
        cmp ax, file_end + INIMAX   ; (detrás de SONIDO.INI: ini_buf)
        jae .w
        mov ax, file_end + INIMAX
.w:
        movzx eax, ax
        add eax, ebx
        add eax, 4095
        and eax, 0xFFFFF000
        jmp .win
.sb:
        add eax, sbemu_end + 4095
        and eax, 0xFFFFF000
        mov edx, eax
        shr edx, 4
        mov [e_seg], dx
        sub eax, ebx
        add ax, ST_MAX
        mov [bdl_off], ax
        add ax, 4096 - ST_MAX
        mov [buf_off], ax
        add ax, NBUF * BUFSZ
        mov [tsr_end], ax
        cmp byte [lay_win], 0
        je .r
        movzx eax, ax
        add eax, ebx
.win:
        mov edx, eax
        shr edx, 4
        mov [win_seg], dx
        add eax, 4096
        sub eax, ebx
        mov [tsr_end], ax
.r:
        ret
.real:
        add eax, resident_end + 127
        and eax, 0xFFFFFF80
        sub eax, ebx
        mov [bdl_off], ax
        add ax, 32 * 16
        mov [buf_off], ax
        add ax, NBUF * BUFSZ
        mov [tsr_end], ax
        ret
; deshacer la ventana de los registros del HD Audio (si la hay)
win_unmap:
        cmp word [hda_wseg], 0
        je .r
        pushad
        mov bx, [hda_wseg]
        shr bx, 8
        mov cx, 1
        mov edx, [win_phys]         ; (con el bit 0: RAM)
        mov ax, 0xDD02
        int 0x67
        popad
        mov word [hda_wseg], 0
.r:
        ret
lay_win     db 1
v86_svc     db 0
win_seg     dw 0
win_phys    dd 0
opt_sb      db 0

; ---- preparar la Sound Blaster emulada: IRQ de la tarjeta de verdad,
; vectores, BLASTER y el aviso. El anillo y los puertos, en e_go ----
e_prepare:
        ; IRQ del HD Audio / AC'97 (registro 3Ch de PCI)
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x3C
        int 0x1A
        jc .noirq
        cmp cl, 3
        jb .noirq
        cmp cl, 15
        ja .noirq
        cmp cl, 8
        je .noirq
        cmp cl, 13
        je .noirq
        mov [e_hwirq], cl
        ; si es la 5, la tarjeta emulada va en la 7
        cmp cl, 5
        jne .noirq
        mov byte [e_irq], 7
.noirq:
        mov ax, 0x3508
        int 0x21
        mov [e_old08], bx
        mov [e_old08+2], es
        mov dx, e_int08
        mov ax, 0x2508
        int 0x21
        mov al, [e_hwirq]
        test al, al
        jnz .hwv                    ; [SB-EOI] sin IRQ: el RTC (INT 70h)
        mov byte [e_rtc], 1         ; [SB-EOI]
        mov ax, 0x3570              ; [SB-EOI]
        int 0x21                    ; [SB-EOI]
        mov [e_oldhw], bx           ; [SB-EOI]
        mov [e_oldhw+2], es         ; [SB-EOI]
        mov dx, e_rtcisr            ; [SB-EOI]
        mov ax, 0x2570              ; [SB-EOI]
        int 0x21                    ; [SB-EOI]
        jmp .nohw                   ; [SB-EOI]
.hwv:                               ; [SB-EOI]
        add al, 8
        cmp al, 16
        jb .v
        add al, 0x70 - 16
.v:
        push ax
        mov ah, 0x35
        int 0x21
        mov [e_oldhw], bx
        mov [e_oldhw+2], es
        pop ax
        mov dx, e_hwisr
        mov ah, 0x25
        int 0x21
.nohw:
        push ds
        pop es
        ; «Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)»
        mov al, [dev_type]
        push ax
        mov byte [dev_type], 2
        mov word [sb_base], 0x220
        mov al, [e_irq]
        mov [sb_irq], al
        mov byte [sb_dma], 1
        mov byte [sb_hdma], 5
        mov word [sb_minor], 0x0405
        mov si, msg_sb_emu
        call puts
        call sb_print.en
        ; BLASTER, si no está ya
        cmp byte [bl_have], 0
        jne .hb
        call sb_set_blaster
.hb:
        pop ax
        mov [dev_type], al
        ret

; ---- ya hay un HSPSND residente (INT 2Fh ha dejado ES:BX = su entrada) ----
installed:
        mov [drv], bx
        mov [drv+2], es
        mov ah, 0
        call far [drv]
        cmp bx, VERSION
        je .vok
        mov si, msg_oldver
        call puts
        mov ax, 0x4C01
        int 0x21
.vok:
        mov al, [have_vol]
        or al, [opt_info]
        or al, [have_gpio]
        or al, [have_eapd]
        jnz .do
        mov si, msg_already
        call puts
        mov ax, 0x4C01
        int 0x21
.do:
        cmp byte [have_vol], 0
        je .nv
        mov al, [opt_vol]
        mov ah, 5
        call far [drv]
.nv:
        ; los datos del residente (mismo programa: mismas direcciones)
        push ds
        push cs
        pop es
        mov ds, [drv+2]
        mov si, old2f
        mov di, si
        mov cx, resident_end - old2f
        cld
        rep movsb
        pop ds
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        ; GPIO y EAPD sobre la marcha
        cmp byte [dev_type], 4
        jne .ng
        mov al, [have_gpio]
        or al, [have_eapd]
        jz .ng
        call hda_reach
        jc .ng
        call a20_open
        call unreal
        cmp byte [have_gpio], 0
        je .ne
        call gpio_apply
.ne:
        cmp byte [have_eapd], 0
        je .nx
        call eapd_apply
.nx:
        call a20_restore
.ng:
        cmp byte [opt_info], 0
        je .sv
        call show_info
        jmp .x
.sv:
        call show_vol
.x:
        mov ax, 0x4C00
        int 0x21

; ---- opciones de la línea de órdenes ----
parse_opts:
        mov si, 0x81
.l:
        lodsb
        cmp al, 13
        je .end
        cmp al, '/'
        jne .l
        mov bx, opt_tab
.t:
        mov di, [bx]
        test di, di
        jz .bad
        push si
.c:
        mov ah, [di]
        test ah, ah
        jz .match
        lodsb
        cmp al, 'a'
        jb .u
        cmp al, 'z'
        ja .u
        sub al, 0x20
.u:
        cmp al, ah
        jne .nomatch
        inc di
        jmp .c
.nomatch:
        pop si
        add bx, 4
        jmp .t
.match:
        add sp, 2
        call [bx+2]
        jmp .l
.bad:
        mov si, msg_badopt
        call puts
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.end:
        ret
o_noapr:
        mov byte [opt_noapr], 1
        ret
o_nosb:
        mov byte [opt_nosb], 1
o_tilde:
        ret
o_sb:
        mov byte [opt_sb], 1
        ret
o_sb8:                              ; [SONIDO-JUEGOS]
        mov byte [opt_sb8], 1
        ret
o_nogpio:
        mov byte [opt_nogpio], 1
        ret
o_nokeys:
        mov byte [opt_nokeys], 1
        ret
o_info:
        mov byte [opt_info], 1
        ret
o_help:
        mov byte [opt_help], 1
        ret
o_vol:
        xor ax, ax
        xor cx, cx
.d:
        mov cl, [si]
        sub cl, '0'
        jb .e
        cmp cl, 9
        ja .e
        imul ax, ax, 10
        add ax, cx
        cmp ax, 100
        jbe .k
        mov ax, 100
.k:
        inc si
        jmp .d
.e:
        mov [opt_vol], al
        mov byte [have_vol], 1
        ret
o_gpio:
        xor ax, ax
.d:
        mov cl, [si]
        cmp cl, 'a'
        jb .u
        sub cl, 0x20
.u:
        sub cl, '0'
        jb .e
        cmp cl, 9
        jbe .k
        sub cl, 7
        cmp cl, 10
        jb .e
        cmp cl, 15
        ja .e
.k:
        shl al, 4
        or al, cl
        inc si
        jmp .d
.e:
        mov [opt_gpio], al
        mov byte [have_gpio], 1
        ret
o_eapd:
        mov al, [si]
        sub al, '0'
        and al, 1
        mov [opt_eapd], al
        mov byte [have_eapd], 1
        ret

opt_tab     dw k_noapr, o_noapr, k_nosb, o_nosb, k_nogpio, o_nogpio, k_nokeys, o_nokeys, k_sb8, o_sb8, k_sb, o_sb
            dw k_vol, o_vol, k_info, o_info, k_diag, o_info
            dw k_gpio, o_gpio, k_eapd, o_eapd, k_help, o_help, k_tilde, o_tilde, 0
k_tilde     db "~", 0
k_nosb      db "NOSB", 0
k_noapr     db "NOAPRENDIDO", 0
opt_noapr   db 0
k_sb        db "SB", 0
k_sb8       db "SB8", 0           ; (antes que SB en opt_tab: se compara por prefijo)
opt_sb8     db 0
sb_hw_h     db 0
k_nogpio    db "NOGPIO", 0
k_nokeys    db "NOTECLAS", 0
k_vol       db "VOL:", 0
k_info      db "INFO", 0
k_diag      db "DIAG", 0
k_gpio      db "GPIO:", 0
k_eapd      db "EAPD:", 0
k_help      db "?", 0
opt_nosb    db 0
opt_nogpio  db 0
opt_nokeys  db 0
opt_info    db 0
opt_help    db 0
have_vol    db 0
opt_vol     db 0
have_gpio   db 0
opt_gpio    db 0
have_eapd   db 0
opt_eapd    db 1
drv         dd 0

; ---- «Volumen: 80 %» ----
show_vol:
        mov si, msg_vol
        call puts
        movzx ax, byte [vol_pct]
        call pdec
        mov si, msg_pct
        call puts
        cmp byte [vol_mute], 0
        je .n
        mov si, msg_muted
        call puts
.n:
        mov si, msg_crlf2
        jmp puts

; ---- /INFO ----
show_info:
        mov si, msg_i_card
        call puts
        movzx bx, byte [dev_type]
        shl bx, 1
        mov si, [i_names+bx]
        cmp byte [dev_type], 2
        ja .pci
        cmp byte [dev_type], 0
        je .none
        call sb_print
        call sb_info_extra          ; [SONIDO-JUEGOS]
        cmp byte [sb_major], 3
        jb .end
        mov si, msg_i_sbmix
        call puts
        mov al, 0x30
        cmp byte [dev_type], 2
        je .m1
        mov al, 0x22
.m1:
        call sb_mixrd
        call hex2
        mov si, msg_i_sbvoz
        call puts
        mov al, 0x32
        cmp byte [dev_type], 2
        je .m2
        mov al, 0x04
.m2:
        call sb_mixrd
        call hex2
        mov si, msg_crlf2
        call puts
        jmp .end
.none:
        call puts
        mov si, msg_crlf2
        call puts
        jmp .end
.pci:
        call puts
        mov si, msg_i_pci
        call puts
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        xor di, di
        int 0x1A
        mov ax, cx
        call hex4
        mov al, ':'
        call putc
        shr ecx, 16
        mov ax, cx
        call hex4
        mov si, msg_i_en
        call puts
        mov al, [pci_bdf+1]
        call hex2
        mov al, ':'
        call putc
        mov al, [pci_bdf]
        shr al, 3
        call hex2
        mov al, '.'
        call putc
        mov al, [pci_bdf]
        and al, 7
        add al, '0'
        call putc
        cmp byte [dev_type], 4
        je .hda
        ; AC'97
        mov si, msg_i_io
        call puts
        mov ax, [nam]
        call hex4
        mov al, '/'
        call putc
        mov ax, [nabm]
        call hex4
        mov si, msg_i_accodec
        call puts
        mov dx, [nam]
        add dx, 0x7C
        in ax, dx
        call hex4
        mov al, ':'
        call putc
        mov dx, [nam]
        add dx, 0x7E
        in ax, dx
        call hex4
        mov si, msg_i_acmas
        call puts
        mov dx, [nam]
        add dx, 0x02
        in ax, dx
        call hex4
        mov si, msg_i_achp
        call puts
        mov dx, [nam]
        add dx, 0x04
        in ax, dx
        call hex4
        mov si, msg_i_acpcm
        call puts
        mov dx, [nam]
        add dx, 0x18
        in ax, dx
        call hex4
        mov si, msg_crlf2
        call puts
        jmp .end
.hda:
        mov si, msg_i_reg
        call puts
        mov eax, [hda_base]
        call hex8
        mov si, msg_crlf2
        call puts
        call hda_info
.end:
        call lrn_info
        call show_vol
        call mz_info
        cmp byte [e_on], 0
        je .nsb
        mov si, msg_i_sbemu
        call puts
.nsb:
        mov si, msg_i_keys
        cmp byte [keys_on], 0
        jne .k
        mov si, msg_i_nokeys
.k:
        jmp puts

; códec HD Audio: lo que hay y lo que se ha elegido (lee el códec en vivo)
hda_info:
        call hda_reach
        jnc .rm
        mov si, msg_i_v86
        jmp puts
.rm:
        call a20_open
        call unreal
        mov dx, 0x0E
        call hrd16
        mov [statests], ax
        mov al, [hda_cad]
        mov [sv_cad], al
        xor cl, cl
.c:
        mov ax, [statests]
        shr ax, cl
        test al, 1
        jz .cn
        mov [hda_cad], cl
        push cx
        xor bl, bl
        xor cl, cl
        call hda_param
        pop cx
        jc .cn
        push eax
        mov si, msg_i_codec
        call puts
        mov al, cl
        add al, '0'
        call putc
        mov al, ':'
        call putc
        mov al, ' '
        call putc
        pop eax
        push eax
        shr eax, 16
        call hex4
        mov al, ':'
        call putc
        pop eax
        call hex4
        cmp cl, [sv_cad]
        jne .cl
        mov si, msg_i_used
        call puts
        mov ax, [hda_rates]
        call hex4
.cl:
        mov si, msg_crlf2
        call puts
.cn:
        inc cl
        cmp cl, 15
        jb .c
        mov al, [sv_cad]
        mov [hda_cad], al
        ; grupo de función de audio
        call afg_range
        mov si, msg_i_afg
        call puts
        mov al, [hda_afg]
        call hex2
        mov si, msg_i_nodes
        call puts
        mov al, [w_first]
        call hex2
        mov al, '-'
        call putc
        mov al, [w_first]
        add al, [w_count]
        dec al
        call hex2
        mov si, msg_i_subsys
        call puts
        mov bl, [hda_afg]
        mov ax, 0xF20
        xor cl, cl
        call hda_verb
        call hex8
        mov si, msg_i_gpio
        call puts
        mov bl, [hda_afg]
        mov cl, 0x11
        call hda_param
        movzx ax, al
        push ax
        call pdec
        pop ax
        test ax, ax
        jz .nog
        mov si, msg_i_gdat
        call puts
        mov ax, 0xF15
        call .gp
        mov si, msg_i_gmask
        call puts
        mov ax, 0xF16
        call .gp
        mov si, msg_i_gdir
        call puts
        mov ax, 0xF17
        call .gp
.nog:
        mov si, msg_crlf2
        call puts
        ; pines que pueden sacar señal
        mov byte [nc_cnt], 0
        mov bl, [w_first]
        movzx cx, byte [w_count]
.w:
        push cx
        call w_type
        cmp al, 4
        jne .wn
        mov cl, 0x0C
        call hda_param
        mov [pincaps], eax
        test al, 0x10
        jz .wn
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        mov [pincfg], eax
        ; ¿en uso?
        mov si, hda_pins
        movzx cx, byte [hda_npin]
.u:
        jcxz .unused
        cmp [si], bl
        je .used
        add si, 5
        dec cx
        jmp .u
.unused:
        xor si, si
        mov eax, [pincfg]
        shr eax, 30
        cmp al, 1
        jne .used
        movzx di, byte [nc_cnt]
        cmp di, 16
        jae .wn
        mov [nc_buf+di], bl
        inc byte [nc_cnt]
        jmp .wn
.used:
        push si
        mov si, msg_i_pin
        call puts
        mov al, bl
        call hex2
        mov al, ':'
        call putc
        mov al, ' '
        call putc
        mov eax, [pincfg]
        shr eax, 20
        and ax, 0x0F
        shl ax, 1
        mov si, ax
        mov si, [i_devs+si]
        call puts
        mov al, ','
        call putc
        mov al, ' '
        call putc
        mov eax, [pincfg]
        shr eax, 30
        shl ax, 1
        mov si, ax
        mov si, [i_conns+si]
        call puts
        mov al, ' '
        call putc
        mov al, '('
        call putc
        mov eax, [pincfg]
        call hex8
        mov al, ')'
        call putc
        test dword [pincaps], 0x10000
        jz .noe
        mov si, msg_i_eapd
        call puts
        mov ax, 0xF0C
        xor cl, cl
        call hda_verb
        shr al, 1
        and al, 1
        add al, '0'
        call putc
.noe:
        pop si
        test si, si
        jnz .path
        mov si, msg_i_unused
        call puts
        jmp .pe
.path:
        push si
        mov si, msg_i_arrow
        call puts
        pop si
        mov cx, 5
.pp:
        mov al, [si]
        test al, al
        jz .pe
        call hex2
        inc si
        dec cx
        jz .pe
        cmp byte [si], 0
        je .pe
        mov al, '>'
        call putc
        jmp .pp
.pe:
        mov si, msg_crlf2
        call puts
.wn:
        pop cx
        inc bl
        dec cx
        jnz .w
        ; pines sin conectar
        cmp byte [nc_cnt], 0
        je .vols
        mov si, msg_i_nc
        call puts
        mov si, nc_buf
        movzx cx, byte [nc_cnt]
.ncl:
        mov al, ' '
        call putc
        lodsb
        call hex2
        loop .ncl
        mov si, msg_crlf2
        call puts
.vols:
        mov si, hda_vols
        movzx cx, byte [hda_nvol]
        test cx, cx
        jnz .vl
        mov si, msg_i_soft
        call puts
        jmp .x
.vl:
        push cx
        push si
        mov si, msg_i_amp
        call puts
        pop si
        mov al, [si]
        call hex2
        mov dx, msg_i_out
        test byte [si+1], 0x80
        jnz .vo
        mov dx, msg_i_in
.vo:
        push si
        mov si, dx
        call puts
        pop si
        mov al, [si+2]
        call hex2
        push si
        mov si, msg_i_of
        call puts
        pop si
        mov al, [si+3]
        call hex2
        push si
        mov si, msg_i_stp
        call puts
        pop si
        movzx ax, byte [si+4]
        mov bl, 4
        div bl
        push ax
        xor ah, ah
        call pdec
        mov al, ','
        call putc
        pop ax
        mov al, ah
        mov ah, 25
        mul ah
        cmp ax, 10
        jae .f2
        push ax
        mov al, '0'
        call putc
        pop ax
.f2:
        call pdec
        push si
        mov si, msg_i_now
        call puts
        pop si
        ; valor actual (canal izquierdo)
        mov bl, [si]
        mov cx, 0xA000
        mov al, [si+1]
        test al, 0x80
        jnz .rd
        mov ch, al
        and ch, 0x0F
        or ch, 0x20
        xor cl, cl
.rd:
        mov al, 0x0B
        call hda_verb4
        call hex2
        push si
        mov si, msg_crlf2
        call puts
        pop si
        pop cx
        add si, 6
        dec cx
        jnz .vl
.x:
        jmp a20_restore
.gp:
        mov bl, [hda_afg]
        xor cl, cl
        call hda_verb
        jmp hex2

; nodos del grupo de audio -> w_first, w_count
afg_range:
        mov bl, [hda_afg]
        mov cl, 0x04
        call hda_param
        mov [w_count], al
        shr eax, 16
        mov [w_first], al
        ret

; =============================================================================
;  Sound Blaster - detección e instalación (se descarta)
; =============================================================================
; BLASTER=A220 I5 D1 H5 T6 del entorno -> bl_*
sb_parse_env:
        push es
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .none
        mov si, s_blaster
        push di
.c:
        lodsb
        test al, al
        jz .found
        mov ah, [es:di]
        cmp ah, 'a'
        jb .u
        sub ah, 32
.u:
        cmp al, ah
        jne .nx
        inc di
        jmp .c
.nx:
        pop di
.sk:
        cmp byte [es:di], 0
        je .n2
        inc di
        jmp .sk
.n2:
        inc di
        jmp .v
.found:
        pop ax
        mov byte [bl_have], 1
.t:
        mov al, [es:di]
        test al, al
        jz .none
        inc di
        and al, 0xDF
        cmp al, 'A'
        je .a
        cmp al, 'I'
        je .i
        cmp al, 'D'
        je .d
        cmp al, 'H'
        je .h
        jmp .t
.a:
        call .hex
        mov [bl_a], dx
        jmp .t
.i:
        call .dec
        mov [bl_i], dl
        jmp .t
.d:
        call .dec
        mov [bl_d], dl
        jmp .t
.h:
        call .dec
        mov [bl_h], dl
        jmp .t
.none:
        pop es
        ret
.hex:
        xor dx, dx
.hl:
        mov al, [es:di]
        sub al, '0'
        jb .hr
        cmp al, 9
        jbe .hd
        and al, 0xDF
        sub al, 'A' - '0' - 10
        cmp al, 10
        jb .hr
        cmp al, 15
        ja .hr
.hd:
        shl dx, 4
        or dl, al
        inc di
        jmp .hl
.hr:
        ret
.dec:
        xor dx, dx
.dl:
        mov al, [es:di]
        sub al, '0'
        jb .dr
        cmp al, 9
        ja .dr
        imul dx, dx, 10
        add dl, al
        inc di
        jmp .dl
.dr:
        ret

; leer un registro del mezclador AL -> AL
sb_mixrd:
        push dx
        mov dx, [sb_base]
        add dx, 4
        out dx, al
        inc dx
        in al, dx
        pop dx
        ret

; buscar la tarjeta -> CF si no hay; dev_type, sb_* rellenos
sb_detect:
        mov byte [bl_have], 0
        mov word [bl_a], 0
        mov byte [bl_i], 0xFF
        mov byte [bl_d], 0xFF
        mov byte [bl_h], 0xFF
        call sb_parse_env
        mov ax, [bl_a]
        test ax, ax
        jz .probe
        mov [sb_base], ax
        call sb_reset
        jnc .found
        stc
        ret
.probe:
        mov si, sb_ports
.p:
        lodsw
        test ax, ax
        jz .no
        mov [sb_base], ax
        call sb_reset
        jc .p
.found:
        mov al, 0xE1                ; versión del DSP
        call sb_out
        call sb_in
        jc .no
        mov [sb_major], al
        call sb_in
        jc .no
        mov [sb_minor], al
        ; valores por omisión, la variable BLASTER manda
        mov byte [sb_irq], 5
        mov byte [sb_dma], 1
        mov byte [sb_hdma], 0
        cmp byte [sb_major], 4
        jb .user
        ; SB16: la configuración está en el mezclador (80h IRQ, 81h DMA)
        mov al, 0x80
        call sb_mixrd
        mov ah, 2
        test al, 1
        jnz .ir
        mov ah, 5
        test al, 2
        jnz .ir
        mov ah, 7
        test al, 4
        jnz .ir
        mov ah, 10
        test al, 8
        jnz .ir
        mov ah, 5
.ir:
        mov [sb_irq], ah
        mov al, 0x81
        call sb_mixrd
        mov bl, al
        mov ah, 0
        test bl, 1
        jnz .d8
        mov ah, 1
        test bl, 2
        jnz .d8
        mov ah, 3
        test bl, 8
        jnz .d8
        mov ah, 1
.d8:
        mov [sb_dma], ah
        mov ah, 5
        test bl, 0x20
        jnz .d16
        mov ah, 6
        test bl, 0x40
        jnz .d16
        mov ah, 7
        test bl, 0x80
        jnz .d16
        mov ah, 0
.d16:
        mov [sb_hdma], ah
.user:
        mov al, [bl_i]
        cmp al, 0xFF
        je .u1
        mov [sb_irq], al
.u1:
        mov al, [bl_d]
        cmp al, 0xFF
        je .u2
        mov [sb_dma], al
.u2:
        mov al, [bl_h]
        cmp al, 0xFF
        je .u3
        mov [sb_hdma], al
.u3:
        ; [SONIDO-JUEGOS] /SB8: la SB16 se usa por la vía de 8 bits; la DMA
        ; alta se guarda para decirla en BLASTER (los juegos sí la usan)
        cmp byte [opt_sb8], 0
        je .u3b
        mov al, [sb_hdma]
        mov [sb_hw_h], al
        mov byte [sb_hdma], 0
.u3b:
        ; SB 1.x/2.0/Pro sin BLASTER: la IRQ está en puentes de la tarjeta
        ; (de fábrica la 7 en muchas, no la 5): se pregunta al DSP
        cmp byte [sb_major], 4
        jae .u4
        cmp byte [bl_i], 0xFF
        jne .u4
        call sb_probe_irq
.u4:
        cmp byte [sb_irq], 2
        jne .i9
        mov byte [sb_irq], 9        ; la IRQ 2 llega por la 9
.i9:
        ; comprobar lo que no se puede usar
        movzx bx, byte [sb_irq]
        cmp bx, 15
        ja .no
        cmp byte [sb_dma], 3
        ja .no
        cmp byte [sb_dma], 2        ; la 2 es de la disquetera
        je .no
        mov byte [dev_type], 1
        cmp byte [sb_major], 4
        jb .ok
        mov al, [sb_hdma]
        cmp al, 5
        jb .ok
        cmp al, 7
        ja .ok
        mov byte [dev_type], 2
.ok:
        clc
        ret
.no:
        mov byte [dev_type], 0
        stc
        ret

; ¿qué IRQ usa la tarjeta? Se pide al DSP una interrupción (orden F2h) con
; cada candidata destapada y una rutina de prueba en su vector.
; -> sb_irq (no cambia si ninguna contesta)
sb_probe_irq:
        mov si, pr_list
.n:
        lodsb
        test al, al
        jz .r
        mov [pr_irq], al
        add al, 8
        cmp byte [pr_irq], 8
        jb .v
        add al, 0x70 - 16
.v:
        mov [pr_vec], al
        push si
        push es
        mov ah, 0x35
        int 0x21
        mov [pr_old], bx
        mov [pr_old+2], es
        pop es
        mov al, [pr_vec]
        mov dx, sb_pisr
        mov ah, 0x25
        int 0x21
        ; destapar (guardando las máscaras)
        cli
        in al, 0x21
        mov [pr_m1], al
        in al, 0xA1
        mov [pr_m2], al
        mov al, [sb_irq]
        push ax
        mov al, [pr_irq]
        mov [sb_irq], al
        call sb_unmask
        pop ax
        mov [sb_irq], al
        sti
        call delay                  ; lo que hubiera pendiente no cuenta
        mov byte [pr_hit], 0
        mov al, 0xF2
        call sb_out
        mov cx, 8
.w:
        call delay
        cmp byte [pr_hit], 0
        jne .wd
        loop .w
.wd:
        cli
        mov al, [pr_m2]
        out 0xA1, al
        mov al, [pr_m1]
        out 0x21, al
        sti
        push ds
        lds dx, [pr_old]
        mov al, [cs:pr_vec]
        mov ah, 0x25
        int 0x21
        pop ds
        pop si
        cmp byte [pr_hit], 0
        je .n
        mov al, [pr_irq]
        mov [sb_irq], al
.r:
        ret
sb_pisr:
        push ax
        push dx
        mov byte [cs:pr_hit], 1
        mov dx, [cs:sb_base]
        add dx, 0x0E
        in al, dx
        mov al, 0x20
        cmp byte [cs:pr_irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop dx
        pop ax
        iret
pr_list     db 5, 7, 9, 10, 0
pr_irq      db 0
pr_vec      db 0
pr_hit      db 0
pr_m1       db 0
pr_m2       db 0
pr_old      dd 0

; instalar: bloques alineados, IRQ y BLASTER en el entorno
; ¿Cabe lo que va a quedar residente (hasta tsr_end) en el bloque de memoria
; del programa (de CS:0 al párrafo que dice el PSP en 0002h)? Si no cabe y
; el programa está en la memoria superior (LH), se carga otra vez a sí mismo
; en la convencional y termina con su resultado; si ya está abajo, no hay
; memoria: lo dice y termina.
fit_check:
        mov ax, [0x0002]
        mov bx, cs
        sub ax, bx                  ; párrafos del bloque
        mov bx, [tsr_end]
        add bx, 15
        jc .no
        shr bx, 4
        cmp bx, ax
        ja .no
        ret
.no:
        mov ax, cs
        cmp ax, 0xA000
        jb .nomem
        ; estrategia de reserva: abajo, y sin enlazar la memoria superior
        mov ax, 0x5801
        xor bx, bx
        int 0x21
        mov ax, 0x5803
        xor bx, bx
        int 0x21
        ; el nombre del programa: detrás del entorno (00 00, una palabra)
        mov es, [0x2C]
        xor di, di
.e:
        cmp word [es:di], 0
        je .f
        inc di
        cmp di, 0x7FF0
        jb .e
        jmp .nomem
.f:
        add di, 4
        ; la misma línea de órdenes, con /~ (o /~~ si el título ya está puesto)
        push es
        push di
        push cs
        pop es
        mov si, 0x80
        mov di, rl_tail
        movzx cx, byte [si]
        cmp cx, 120
        jbe .tk
        mov cx, 120
.tk:
        inc si
        mov bx, di
        inc di
        rep movsb
        mov ax, ' /'
        stosw
        mov al, '~'
        stosb
        cmp byte [title_done], 0
        je .tn
        stosb
.tn:
        mov ax, di
        sub ax, bx
        dec ax
        mov [bx], al
        mov byte [di], 13
        pop di
        pop es
        mov word [rl_pb], 0         ; el mismo entorno
        mov word [rl_pb+2], rl_tail
        mov [rl_pb+4], cs
        mov word [rl_pb+6], 0x5C
        mov [rl_pb+8], cs
        mov word [rl_pb+10], 0x6C
        mov [rl_pb+12], cs
        mov [rl_sp], sp
        mov dx, di
        push es
        pop ds
        push cs
        pop es
        mov bx, rl_pb
        mov ax, 0x4B00
        int 0x21
        cli
        mov bx, cs
        mov ss, bx
        mov sp, [cs:rl_sp]
        sti
        mov ds, bx
        mov es, bx
        cld
        jc .nomem
        mov ah, 0x4D
        int 0x21
        mov ah, 0x4C
        int 0x21
.nomem:
        cmp byte [title_done], 0
        jne .nm2
        mov si, msg_title
        call puts
.nm2:
        mov si, msg_nofit
        call puts
        mov ax, 0x4C01
        int 0x21
rl_sp       dw 0
rl_pb       times 14 db 0
rl_tail     times 128 db 0
title_done  db 0
relaunched  db 0
msg_empty   db 0
msg_nofit   db "No hay memoria suficiente para cargar HSPSND.", 13, 10, 0

sb_install:
        call sb_layout
        jmp sb_install2
; bloques de DMA de la Sound Blaster -> buf_off, tsr_end
sb_layout:
        ; bloques (16 KB) justo detrás del código residente, salvo que así
        ; crucen una frontera de 64 KB (la DMA del ISA no puede): entonces
        ; empiezan en la frontera. (Antes se alineaban a 16 KB: se perdían
        ; hasta 16 KB de memoria convencional, que a los juegos les hace falta.)
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, resident_end + 15
        and eax, 0xFFFFFFF0
        mov ecx, eax
        and ecx, 0xFFFF
        add ecx, 4 * SB_BLK
        cmp ecx, 0x10000
        jbe .cabe
        add eax, 0xFFFF
        and eax, 0xFFFF0000
.cabe:
        sub eax, ebx
        mov [buf_off], ax
        add ax, 4 * SB_BLK
        mov [tsr_end], ax
        ret
sb_install2:
        ; (sin tocar aún los bloques: pueden estar encima de este código;
        ; sb_tsr los deja en silencio al final)
        call sb_reset
        call sb_dma_mask
        ; vector de la IRQ
        mov al, [sb_irq]
        add al, 8
        cmp byte [sb_irq], 8
        jb .v
        add al, 0x70 - 16
.v:
        mov [sb_vec], al
        mov ah, 0x35
        int 0x21
        mov [sb_old], bx
        mov [sb_old+2], es
        mov al, [sb_vec]
        mov dx, sb_isr
        mov ah, 0x25
        int 0x21
        call sb_unmask
        ; la voz (PCM) al máximo: el reinicio del mezclador la deja a -14 dB;
        ; el volumen se lleva con el maestro
        cmp byte [sb_major], 3
        jb .nomix
        mov ax, 0x04FF              ; SB Pro: voz
        cmp byte [dev_type], 2
        jne .vz
        mov ax, 0x32F8              ; SB16: voz izquierda y derecha
        call sb_mix
        mov ax, 0x33F8
.vz:
        call sb_mix
.nomix:
        call sb_print
        cmp byte [bl_have], 0
        jne .r
        call sb_set_blaster
.r:
        ret
; «Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)»
sb_print:
        mov si, name_sb
        cmp byte [dev_type], 2
        jne .n
        mov si, name_sb16
.n:
        call puts
.en:
        mov si, msg_sb_en
        call puts
        mov ax, [sb_base]
        call hex3
        mov si, msg_sb_irq
        call puts
        movzx ax, byte [sb_irq]
        call dec2
        mov si, msg_sb_dma
        call puts
        movzx ax, byte [sb_dma]
        call dec2
        cmp byte [dev_type], 2
        jne .nh
        mov si, msg_sb_y
        call puts
        movzx ax, byte [sb_hdma]
        call dec2
.nh:
        mov si, msg_sb_dsp
        call puts
        movzx ax, byte [sb_major]
        call dec2
        mov al, '.'
        call putc
        mov al, [sb_minor]
        aam
        add ax, 0x3030
        xchg al, ah
        call putc
        mov al, ah
        call putc
        mov si, msg_sb_end
        jmp puts

; BLASTER=A220 I5 D1 [H5] T6 en el entorno del intérprete que nos ejecuta
sb_set_blaster:
        ; montar el texto
        mov di, bl_text
        mov si, s_blaster
.c:
        lodsb
        test al, al
        jz .c1
        mov [di], al
        inc di
        jmp .c
.c1:
        mov byte [di], 'A'
        inc di
        mov ax, [sb_base]
        call hex3_di
        mov dword [di], ' I'
        add di, 2
        movzx ax, byte [sb_irq]
        call dec_di
        mov dword [di], ' D'
        add di, 2
        movzx ax, byte [sb_dma]
        call dec_di
        ; [SONIDO-JUEGOS] con /SB8 la DMA alta sigue en la variable
        movzx ax, byte [sb_hdma]
        cmp byte [dev_type], 2
        je .hh
        movzx ax, byte [sb_hw_h]
        cmp al, 5
        jb .t
        cmp al, 7
        ja .t
.hh:
        mov dword [di], ' H'
        add di, 2
        call dec_di
.t:
        mov al, '1'
        cmp byte [sb_major], 2
        jb .tt
        mov al, '3'
        cmp byte [sb_major], 3
        jb .tt
        mov al, '4'
        cmp byte [sb_major], 4
        jb .tt
        mov al, '6'
.tt:
        mov byte [di], ' '
        mov byte [di+1], 'T'
        mov [di+2], al
        mov byte [di+3], 0
        add di, 4
        sub di, bl_text
        mov [bl_len], di            ; con el cero
        ; entorno del padre (o del abuelo: fit_check)
        mov ax, [0x16]
        test ax, ax
        jz .fail
        mov es, ax
        cmp byte [relaunched], 0
        je .par
        mov ax, [es:0x16]
        test ax, ax
        jz .fail
        mov es, ax
.par:
        mov ax, [es:0x2C]
        test ax, ax
        jz .fail
        mov es, ax
        dec ax
        push ds
        mov ds, ax
        mov cx, [3]                 ; tamaño del bloque en párrafos
        pop ds
        shl cx, 4
        mov [bl_size], cx
        ; fin de las variables: ES:BX = el cero que cierra la lista
        xor bx, bx
        cmp byte [es:0], 0
        je .end
.e:
        cmp word [es:bx], 0
        je .e1
        inc bx
        cmp bx, cx
        jb .e
        jmp .fail
.e1:
        inc bx
.end:
        ; lo que va detrás (DOS 3+: cuenta y nombre del programa)
        lea si, [bx+1]
        xor dx, dx
        cmp word [es:si], 1
        jne .tl
        mov dx, 2
        lea di, [si+2]
.tn:
        inc dx
        cmp byte [es:di], 0
        je .tl
        inc di
        cmp di, cx
        jb .tn
        jmp .fail
.tl:
        ; ¿cabe? bx (variables) + texto + 1 (cero final) + dx (cola)
        mov ax, bx
        add ax, [bl_len]
        inc ax
        add ax, dx
        cmp ax, cx
        ja .full
        ; mover la cola hacia delante
        push ds
        push es
        pop ds
        std
        lea si, [bx+1]
        add si, dx
        dec si                      ; último octeto de la cola
        mov di, si
        add di, [cs:bl_len]
        mov cx, dx
        inc cx                      ; y el cero final
        rep movsb
        cld
        pop ds
        ; copiar el texto en ES:BX
        mov di, bx
        mov si, bl_text
        mov cx, [bl_len]
        rep movsb
        mov byte [es:di], 0         ; fin de la lista
        mov si, msg_sb_set
        call puts
        mov si, bl_text
        call puts
        mov si, msg_crlf2
        call puts
        ret
.full:
        mov si, msg_sb_full
        call puts
        mov si, bl_text
        call puts
        mov si, msg_crlf2
        call puts
        ret
.fail:
        ; [SONIDO-JUEGOS] antes no se decía nada: los juegos se quedaban sin
        ; la variable y sin sonido, y no había pista de por qué
        mov si, msg_sb_noenv
        call puts
        mov si, bl_text
        call puts
        mov si, msg_crlf2
        call puts
        ret

; [SONIDO-JUEGOS] /INFO: interrupciones atendidas, máscara del PIC y la
; variable BLASTER que ven los programas
sb_info_extra:
        mov si, msg_i_sbirq
        call puts
        mov ax, [sb_nirq]
        call pdec
        mov si, msg_i_sbirq2
        call puts
        movzx ax, byte [sb_irq]
        call dec2
        mov cl, [sb_irq]
        mov dx, 0x21
        cmp cl, 8
        jb .m
        mov dx, 0xA1
        sub cl, 8
.m:
        in al, dx
        shr al, cl
        mov si, msg_i_dest
        test al, 1
        jz .p
        mov si, msg_i_tap
.p:
        call puts
        ; BLASTER en el entorno de este programa (copia del de quien lo lanza)
        push es
        mov es, [0x2C]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .no
        mov si, s_blaster
        mov bx, di
.c:
        lodsb
        test al, al
        jz .si
        cmp al, [es:di]
        jne .nx
        inc di
        jmp .c
.nx:
        cmp byte [es:di], 0
        je .n2
        inc di
        jmp .nx
.n2:
        inc di
        jmp .v
.si:
        mov si, msg_i_env
        call puts
.e:
        mov al, [es:bx]
        test al, al
        jz .ef
        call putc
        inc bx
        jmp .e
.ef:
        pop es
        mov si, msg_crlf2
        jmp puts
.no:
        pop es
        mov si, msg_i_noenv
        jmp puts

; AX -> 3 cifras hexadecimales (puerto)
hex3:
        push ax
        push cx
        mov cx, 3
        shl ax, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        pop ax
        mov al, 'h'
        jmp putc
hex3_di:
        push cx
        mov cx, 3
        shl ax, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov [di], al
        inc di
        pop ax
        loop .l
        pop cx
        ret
; AX (0-99) en decimal
dec2:
        cmp ax, 10
        jb .u
        push ax
        mov cl, 10
        div cl
        add al, '0'
        call putc
        mov al, ah
        add al, '0'
        call putc
        pop ax
        ret
.u:
        add al, '0'
        jmp putc
dec_di:
        cmp ax, 10
        jb .u
        mov cl, 10
        div cl
        add al, '0'
        mov [di], al
        inc di
        mov al, ah
.u:
        add al, '0'
        mov [di], al
        inc di
        ret

sb_ports    dw 0x220, 0x240, 0x260, 0x280, 0
s_blaster   db "BLASTER=", 0
msg_sb_en   db " en ", 0
msg_sb_irq  db ", IRQ ", 0
msg_sb_dma  db ", DMA ", 0
msg_sb_y    db " y ", 0
msg_sb_dsp  db " (DSP ", 0
msg_sb_end  db ")", 13, 10, 0
msg_sb_set  db "Variable puesta: ", 0
msg_sb_full db "No cabe en el entorno; póngala en AUTOEXEC.BAT: SET ", 0
msg_sb_noenv db "No se ha podido poner la variable; escriba: SET ", 0
msg_i_sbirq db "Interrupciones de la tarjeta atendidas: ", 0
msg_i_sbirq2 db "; IRQ ", 0
msg_i_dest  db " destapada", 13, 10, 0
msg_i_tap   db " tapada (se destapa al tocar)", 13, 10, 0
msg_i_env   db "Variable para los juegos: ", 0
msg_i_noenv db "Variable BLASTER: no está puesta (los juegos no verán la tarjeta)", 13, 10, 0
bl_have     db 0
bl_i        db 0
bl_d        db 0
bl_h        db 0
bl_a        dw 0
bl_len      dw 0
bl_size     dw 0
bl_text     times 40 db 0

; poner en marcha el controlador HD Audio y buscar las salidas del códec
hda_init:
        smsw ax
        test al, 1
        jz .rm
        cmp byte [v86_svc], 0       ; modo V86: solo con la ventana de HSPEMM
        jne .rm
        stc
        ret
.rm:
        call a20_open
        ; GDT del modo irreal
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        ; identificadores (para lo aprendido en la prueba de sonido)
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        xor di, di
        int 0x1A
        mov [lr_pid], ecx
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x2C
        int 0x1A
        mov [lr_psub], ecx
        ; BAR0 (memoria)
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        and ecx, 0xFFFFFFF0
        mov [hda_base], ecx
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0006               ; memoria y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        ; Intel: clase de tráfico 0 (TCSEL), o el flujo puede no salir limpio
        mov bx, [pci_bdf]
        mov ax, 0xB109
        xor di, di
        int 0x1A
        cmp cx, 0x8086
        jne .ntc
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x44
        int 0x1A
        jc .ntc
        and cl, 0xF8
        mov bx, [pci_bdf]
        mov ax, 0xB10B
        mov di, 0x44
        int 0x1A
.ntc:
        mov eax, [hda_base]
        mov [hda_reg], eax
        test eax, eax
        jz .fail
        cmp byte [v86_svc], 0
        je .nw
        ; modo V86 con HSPEMM: los registros, en la página win_seg (antes
        ; se apunta la RAM que había en ella, para poder deshacerlo)
        movzx ecx, word [win_seg]
        shr cx, 8
        mov ax, 0xDE06
        int 0x67
        test ah, ah
        jnz .fail
        or dl, 1
        mov [win_phys], edx
        mov bx, [win_seg]
        shr bx, 8
        mov cx, 1
        mov edx, [hda_base]
        and edx, 0xFFFFF000
        mov ax, 0xDD02
        int 0x67
        test ah, ah
        jnz .fail
        mov ax, [win_seg]
        mov [hda_wseg], ax
        and dword [hda_reg], 0xFFF
.nw:
        call unreal
        ; reinicio del controlador (GCTL.CRST)
        mov dx, 0x08
        xor eax, eax
        call hwr32
        mov cx, 0xFFFF
.c0:
        call hrd32
        test al, 1
        loopnz .c0
        call delay
        mov eax, 1
        call hwr32
        mov cx, 0xFFFF
.c1:
        call hrd32
        test al, 1
        loopz .c1
        jz .fail
        ; dar tiempo a los códecs y ver cuáles responden
        call delay
        call delay
        mov dx, 0x0E
        call hrd16
        mov [statests], ax
        test ax, ax
        jz .fail
        ; primer flujo de salida
        mov dx, 0x00
        call hrd16
        shr ax, 8
        and ax, 0x0F
        shl ax, 5
        add ax, 0x80
        mov [hda_sd], ax
        mov dx, 48000
        mov [cur_rate], dx
        call hda_rate_fmt
        ; el primer códec con un grupo de audio y una salida analógica (puede
        ; haber también un módem o el HDMI)
        xor cl, cl
.cad:
        mov ax, [statests]
        shr ax, cl
        test al, 1
        jz .cn
        mov [hda_cad], cl
        push cx
        cmp byte [lrn_mode], 0
        jne .lm
        call codec_setup
        jmp .cr
.lm:
        call codec_learn            ; (solo lo aprendido en la prueba)
.cr:
        pop cx
        jnc .ok
.cn:
        inc cl
        cmp cl, 15
        jb .cad
.fail:
        call a20_restore
        call win_unmap              ; (modo V86: la página vuelve a ser RAM)
        stc
        ret
.ok:
        call a20_restore
        clc
        ret

; preparar el códec hda_cad -> CF si no tiene salida analógica
codec_setup:
        call find_afg
        jnc .afg
.no:
        stc
        ret
.afg:
        mov ax, 0x705
        xor cl, cl
        call hda_verb               ; encendido D0
        call delay                  ; el códec tarda en despertar
        call delay
        call afg_range
        ; GPIO: en muchos portátiles encienden el amplificador de altavoces
        call gpio_apply
        ; salidas: altavoces internos, auriculares y línea, todas a la vez
        xor al, al
        mov [hda_npin], al
        mov [hda_nvol], al
        mov [hda_dac], al
        mov [no_hw], al
        mov dword [hda_dacs], 0
        mov word [new_rates], 0x7FF
        mov byte [prio], 0
.pr:
        mov bl, [w_first]
        movzx cx, byte [w_count]
        jcxz .prn
.w:
        push cx
        cmp byte [hda_npin], 4
        jae .wn
        call pin_class
        cmp al, [prio]
        jne .wn
        mov [p_node], bl
        mov byte [lvl], 0
        push bx
        call find_dac
        pop bx
        jc .wn
        call path_apply
        mov bl, [p_node]
.wn:
        pop cx
        inc bl
        loop .w
.prn:
        inc byte [prio]
        cmp byte [prio], 3
        jb .pr
        ja .picked
        cmp byte [hda_npin], 0      ; sin ninguna «conectada»: las demás
        je .pr
.picked:
        cmp byte [hda_npin], 0
        jne .vol
        ; sin pin utilizable: al menos el primer DAC analógico
        mov bl, [w_first]
        movzx cx, byte [w_count]
        test cx, cx
        jz .no
.d:
        call w_type
        test ax, ax
        jz .dac
        inc bl
        loop .d
        jmp .no
.dac:
        call dac_add
        mov byte [path_vol], 0
        test byte [cur_caps], 0x04
        jz .dv
        mov dl, 0x80
        call amp_setup
.dv:
        cmp byte [path_vol], 0
        jne .vol
        mov byte [no_hw], 1
.vol:
        call setup_tail
        clc
        ret

; lo común al final: volumen (por los amplificadores o por programa), EAPD
; y las frecuencias de los DAC en uso
setup_tail:
        ; si algún camino no tiene amplificador regulable, el volumen se
        ; hace por programa y los amplificadores quedan en 0 dB
        mov byte [hw_vol], 1
        cmp byte [no_hw], 0
        je .eapd
        mov byte [hw_vol], 0
        mov si, hda_vols
        movzx cx, byte [hda_nvol]
        jcxz .nv
.fx:
        push cx
        mov bl, [si]
        mov dl, [si+1]
        mov al, [si+2]
        mov [ac_off], al
        call amp_fixed
        pop cx
        add si, 6
        loop .fx
.nv:
        mov byte [hda_nvol], 0
.eapd:
        call eapd_apply
        ; frecuencias comunes a los DAC en uso (si no dicen nada: 44,1 y 48)
        mov ax, [new_rates]
        test ax, ax
        jnz .rs
        mov ax, 0x0060
.rs:
        mov [hda_rates], ax
        mov word [cur_rate], 0      ; el formato se pone al empezar a tocar
        jmp hda_inputs              ; CD, línea y micrófono (hspmezi.inc)

; grupo de función de audio (tipo 1) entre los del códec -> BL y hda_afg; CF
; si no hay
find_afg:
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
        mov [hda_afg], bl
        clc
        ret
; GPIO del grupo de audio: salidas y con el valor pedido (todas a 1)
gpio_apply:
        cmp byte [opt_nogpio], 0
        jne .r
        mov bl, [hda_afg]
        mov cl, 0x11
        call hda_param
        jc .r
        test al, al
        jz .r
        mov cl, al
        cmp cl, 8
        jbe .c
        mov cl, 8
.c:
        mov dx, 1
        shl dx, cl
        dec dx                      ; DL = máscara de las que hay
        mov dh, dl
        cmp byte [have_gpio], 0
        je .d
        and dh, [opt_gpio]
.d:
        mov ax, 0x716               ; máscara de activación
        mov cl, dl
        call hda_verb
        mov ax, 0x717               ; dirección: salidas
        mov cl, dl
        call hda_verb
        mov ax, 0x715               ; datos
        mov cl, dh
        call hda_verb
.r:
        ret

; EAPD (amplificador externo) en todos los pines de salida que lo tengan
eapd_apply:
        call afg_range
        mov bl, [w_first]
        movzx cx, byte [w_count]
        jcxz .r
.w:
        push cx
        call w_type
        cmp al, 4
        jne .n
        mov cl, 0x0C
        call hda_param
        test eax, 0x10000
        jz .n
        test al, 0x10
        jz .n
        mov ax, 0xF0C
        xor cl, cl
        call hda_verb
        jc .n
        and al, 0x05                ; BTL e intercambio, como estén
        cmp byte [opt_eapd], 0
        je .wr
        or al, 0x02
.wr:
        mov cl, al
        mov ax, 0x70C
        call hda_verb
.n:
        pop cx
        inc bl
        loop .w
.r:
        ret

; BL = nodo -> AL = tipo (FFh si falla), AH = 1 si es digital; cur_caps
w_type:
        push cx
        mov cl, 0x09
        call hda_param
        pop cx
        jc .e
        mov [cur_caps], eax
        shr eax, 20
        and ax, 0x0F
        test byte [cur_caps+1], 0x02
        jz .x
        mov ah, 1
.x:
        ret
.e:
        mov ax, 0xFFFF
        ret

; BL = nodo -> AL = clase de salida: 0 altavoz, 1 auriculares, 2 línea,
; 3 de reserva (sin conectar o sin configurar), FFh no vale
pin_class:
        call w_type
        cmp ax, 4                   ; pin analógico
        jne .no
        mov cl, 0x0C
        call hda_param
        jc .no
        test al, 0x10               ; puede sacar señal
        jz .no
        mov ax, 0xF1C
        xor cl, cl
        call hda_verb
        jc .no
        mov ecx, eax
        shr ecx, 20
        and cl, 0x0F                ; dispositivo
        cmp cl, 2
        jbe .dev
        test eax, eax               ; sin configurar: de reserva
        jnz .no
        mov al, 3
        ret
.dev:
        shr eax, 30
        cmp al, 1                   ; «sin conexión física»
        mov al, 3
        je .r
        mov al, 0
        cmp cl, 1
        je .r
        mov al, 1
        cmp cl, 2
        je .r
        mov al, 2
.r:
        ret
.no:
        mov al, 0xFF
        ret

; lista de conexiones del nodo BL en DI (hasta CONNMAX) -> CX = cuántas
CONNMAX     equ 24
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
        and dl, 0x7F                ; entradas
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
        mov [ds:di+bp], al
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

; camino desde el nodo BL (nivel lvl; el pin es el 0) hasta un DAC analógico,
; directo o por mezcladores y selectores -> CF=0: p_node, p_idx, plen
find_dac:
        push si
        push di
        push dx
        movzx di, byte [lvl]
        imul di, di, CONNMAX
        add di, conn_bufs
        call get_conn
        mov dx, cx
        ; primero, un DAC conectado directamente
        xor si, si
.d:
        cmp si, dx
        jae .m0
        mov bx, di
        mov bl, [bx+si]
        call w_type
        test ax, ax
        jnz .dn
        call .take
        mov al, [lvl]
        add al, 2
        mov [plen], al
        clc
        jmp .out
.dn:
        inc si
        jmp .d
.m0:
        cmp byte [lvl], 2
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
        call .take
        inc byte [lvl]
        call find_dac
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
.take:
        push di
        movzx di, byte [lvl]
        mov [p_node+di+1], bl
        mov ax, si
        mov [p_idx+di], al
        pop di
        ret

; BL = DAC: apuntarlo y prepararlo (encendido, flujo 1 canal 0, formato)
dac_add:
        push si
        mov si, hda_dacs
.l:
        cmp [si], bl
        je .r
        cmp byte [si], 0
        je .new
        inc si
        cmp si, hda_dacs + 4
        jb .l
        jmp .r
.new:
        mov [si], bl
        cmp byte [hda_dac], 0
        jne .c
        mov [hda_dac], bl
.c:
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        mov ax, 0x706
        mov cl, 0x10
        call hda_verb
        mov al, 2
        mov cx, [hda_fmt]
        call hda_verb4
        ; frecuencias que admite (las del grupo de audio si no trae las suyas)
        mov cl, 0x0A
        call hda_param
        jc .r
        test ax, 0x7FF
        jnz .rt
        push bx
        mov bl, [hda_afg]
        call hda_param
        pop bx
        jc .r
.rt:
        and [new_rates], ax
.r:
        pop si
        ret

; poner en marcha el camino p_node[0..plen-1]: encendido D0, selección de
; conexión, amplificadores (uno hace de volumen; los demás, 0 dB y sin
; silencio) y el pin como salida
path_apply:
        movzx di, byte [hda_npin]
        imul di, di, 5
        add di, hda_pins
        mov dword [di], 0
        mov byte [di+4], 0
        movzx cx, byte [plen]
        mov si, p_node
        push cs
        pop es
        cld
        rep movsb
        ; (para /INFO: las entradas elegidas)
        movzx di, byte [hda_npin]
        shl di, 2
        add di, hda_pidx
        mov eax, [p_idx]
        mov [di], eax
        movzx si, byte [plen]
        mov bl, [p_node+si-1]
        call dac_add
        mov byte [path_vol], 0
.k:
        dec si
        js .end
        mov bl, [p_node+si]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        call w_type
        mov [cur_type], al
        movzx ax, byte [plen]
        dec ax
        cmp si, ax
        jae .out                    ; el DAC no tiene entradas
        mov cl, [p_idx+si]
        cmp byte [cur_type], 2      ; el mezclador no elige: mezcla
        je .inamp
        mov ax, 0x701
        call hda_verb
.inamp:
        cmp byte [cur_type], 4
        je .out
        test byte [cur_caps], 0x02  ; amplificador de entrada
        jz .out
        mov dl, [p_idx+si]
        cmp dl, 15
        ja .out
        call amp_setup
.out:
        test byte [cur_caps], 0x04  ; amplificador de salida
        jz .pin
        mov dl, 0x80
        call amp_setup
.pin:
        cmp byte [cur_type], 4
        jne .k
        mov cl, 0x0C
        call hda_param
        mov cl, 0x40                ; salida activa
        test al, 0x08
        jz .w
        mov cl, 0xC0                ; y con amplificador de auriculares
.w:
        cmp byte [lrn_ctlv], 0xFF   ; lo aprendido en la prueba manda
        je .w2
        mov cl, [lrn_ctlv]
.w2:
        movzx di, byte [hda_npin]
        mov [hda_pctl+di], cl
        mov ax, 0x707
        call hda_verb
        jmp .k
.end:
        inc byte [hda_npin]
        cmp byte [path_vol], 0
        jne .r
        mov byte [no_hw], 1
.r:
        ret

; capacidades del amplificador (CL = 12h salida, 0Dh entrada) del nodo BL;
; si el nodo no trae las suyas valen las del grupo de audio
amp_caps:
        push bx
        test byte [cur_caps], 0x08
        jnz .own
        mov bl, [hda_afg]
.own:
        call hda_param
        pop bx
        ret

; amplificador del nodo BL (DL = 80h salida, o el índice de la entrada):
; el primero regulable del camino lleva el volumen; los demás, a 0 dB
amp_setup:
        push si
        mov cl, 0x12
        test dl, 0x80
        jnz .c
        mov cl, 0x0D
.c:
        call amp_caps
        mov cl, al
        and cl, 0x7F
        mov [ac_off], cl            ; paso que da 0 dB
        shr eax, 8
        mov cl, al
        and cl, 0x7F
        mov [ac_steps], cl
        shr eax, 8
        and al, 0x7F
        inc al
        mov [ac_stepq], al          ; tamaño del paso en cuartos de dB
        mov si, hda_vols
        movzx cx, byte [hda_nvol]
.f:
        jcxz .nf
        cmp [si], bl
        jne .fn
        cmp [si+1], dl
        je .isvol
.fn:
        add si, 6
        dec cx
        jmp .f
.nf:
        cmp byte [path_vol], 0
        jne .fixed
        cmp byte [ac_steps], 0
        je .fixed
        cmp byte [ac_off], 0
        je .fixed
        cmp byte [hda_nvol], 8
        jae .fixed
        mov [si], bl
        mov [si+1], dl
        mov al, [ac_off]
        mov [si+2], al
        mov al, [ac_steps]
        mov [si+3], al
        mov al, [ac_stepq]
        mov [si+4], al
        inc byte [hda_nvol]
.isvol:
        mov byte [path_vol], 1
        jmp .r
.fixed:
        call amp_fixed
.r:
        pop si
        ret
; amplificador a 0 dB (ac_off) y sin silencio
amp_fixed:
        movzx cx, byte [ac_off]
        test dl, 0x80
        jz .in
        or cx, 0xB000
        jmp .w
.in:
        mov ch, dl
        or ch, 0x70
.w:
        mov al, 3
        jmp hda_verb4

w_first     db 0
w_count     db 0
statests    dw 0
sv_cad      db 0
prio        db 0
lvl         db 0
plen        db 0
p_node      times 6 db 0
p_idx       times 6 db 0
cur_caps    dd 0
cur_type    db 0
path_vol    db 0
no_hw       db 0
new_rates   dw 0
ac_off      db 0
ac_steps    db 0
ac_stepq    db 0
gc_prev     db 0
gc_k        db 0
pincaps     dd 0
pincfg      dd 0
nc_cnt      db 0
nc_buf      times 16 db 0
conn_bufs   times 4 * CONNMAX db 0

; poner en marcha el controlador AC'97 y el códec
ac97_init:
        ; BAR0 = mezclador (NAM), BAR1 = bus master (NABM); los dos de E/S
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        and cx, 0xFFFE
        mov [nam], cx
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x14
        int 0x1A
        and cx, 0xFFFE
        mov [nabm], cx
        ; sin puertos asignados (ICH6/7 con solo los BAR de memoria): nada
        ; de escribir en los puertos 0..3Fh, que son del controlador de DMA
        jz .nobar
        cmp word [nam], 0
        jne .bars
.nobar:
        stc
        ret
.bars:
        ; activar E/S y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0005
        mov bx, [pci_bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        ; reinicio en frío
        mov dx, [nabm]
        add dx, 0x2C
        xor eax, eax
        out dx, eax
        call delay
        mov eax, 2
        out dx, eax
        call delay
        ; esperar al códec: en frío puede tardar medio segundo largo
        mov cx, 64                  ; ~1 s
        mov dx, [nabm]
        add dx, 0x30
.w:
        in eax, dx
        test ax, 0x0100
        jnz .ready
        call delay
        loop .w
        stc
        ret
.ready:
        ; reiniciar el códec y abrir volúmenes
        mov dx, [nam]
        xor ax, ax
        out dx, ax
        call delay
        mov dx, [nam]
        add dx, 0x02                ; maestro: 0 dB
        xor ax, ax
        out dx, ax
        mov dx, [nam]
        add dx, 0x04                ; auriculares
        out dx, ax
        mov dx, [nam]
        add dx, 0x18                ; PCM
        mov ax, 0x0808
        out dx, ax
        ; frecuencia variable
        mov dx, [nam]
        add dx, 0x28
        in ax, dx
        test al, 1
        jz .novra
        mov dx, [nam]
        add dx, 0x2A
        in ax, dx
        or al, 1
        out dx, ax
        mov byte [vra], 1
.novra:
        mov word [cur_rate], 48000
        clc
        ret

delay:
        push cx
        mov cx, 0x4000
.l:
        in al, 0x80
        loop .l
        pop cx
        ret

hex4:
        push ax
        push cx
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .h
        pop cx
        pop ax
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret

; AL en hexadecimal (2 cifras); EAX (8 cifras)
hex2:
        push ax
        push cx
        mov ah, al
        mov cx, 2
        jmp hex4.h
hex8:
        push eax
        shr eax, 16
        call hex4
        pop eax
        jmp hex4
; AX en decimal
pdec:
        push ax
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; =============================================================================
;  Lo aprendido en la prueba de sonido: \HISPADOS\SONIDO.INI (lo escribe
;  AUDICION cuando se contesta que SÍ se oye). Todo esto se descarta.
; =============================================================================
INIMAX      equ 8192
ini_buf     equ file_end            ; (detrás del programa: ver layout)

; ---- leer SONIDO.INI: de la unidad de HSPSND o de la de arranque ----
ini_load:
        cmp byte [ini_tried], 0     ; (una vez: con la Sound Blaster se lee
        jne .r                      ; al final, por el mezclador)
        mov byte [ini_tried], 1
        cmp byte [opt_noapr], 0
        je .go
        mov byte [lrn_why], 5       ; (/NOAPRENDIDO: solo el mezclador)
.go:
        ; sitio: detrás del programa, hasta 1 KB antes del final de su
        ; bloque de memoria (la pila); los búferes de DMA aún no se usan
        mov ax, [0x0002]
        mov bx, cs
        sub ax, bx
        cmp ax, 0x0FFF
        jb .bl
        mov ax, 0x0FFF
.bl:
        shl ax, 4
        sub ax, file_end + 1024
        jbe .r
        cmp ax, INIMAX
        jbe .rm
        mov ax, INIMAX
.rm:
        mov [ini_room], ax
        ; la unidad del programa: su nombre va detrás del entorno
        push es
        mov es, [0x2C]
        xor di, di
.e:
        cmp word [es:di], 0
        je .f
        inc di
        cmp di, 0x7FF0
        jb .e
        pop es
        jmp .boot
.f:
        mov al, [es:di+4]
        cmp byte [es:di+5], ':'
        pop es
        jne .boot
        call .try
        jnc .ok
.boot:
        mov ax, 0x3305
        int 0x21
        mov al, dl
        add al, 'A' - 1
        call .try
        jnc .ok
.r:
        ret
.ok:
        mov byte [ini_lei], 1
        cmp byte [opt_noapr], 0
        jne .cs
        mov byte [ini_ok], 1
.cs:
        ; cuántas secciones hay
        mov si, ini_buf
.l:
        call ini_skipsp
        cmp byte [si], '['
        jne .n
        inc byte [ini_nsec]
.n:
        call ini_nextline
        jnc .l
        ret
; AL = unidad -> CF si no se puede leer
.try:
        and al, 0xDF
        mov [ini_path], al
        mov ax, 0x3D00
        mov dx, ini_path
        int 0x21
        jc .tr
        mov bx, ax
        mov cx, [ini_room]
        dec cx
        mov dx, ini_buf
        mov ah, 0x3F
        int 0x21
        jnc .rd
        xor ax, ax
.rd:
        mov si, ax
        mov byte [ini_buf+si], 0
        mov ah, 0x3E
        int 0x21
        clc
.tr:
        ret

; SI -> saltar espacios y tabuladores
ini_skipsp:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp ini_skipsp
.r:
        ret
; SI -> principio de la línea siguiente; CF si se acaba el fichero
ini_nextline:
        lodsb
        test al, al
        jz .end
        cmp al, 10
        jne ini_nextline
        cmp byte [si], 0
        je .end
        clc
        ret
.end:
        stc
        ret
; AL -> mayúscula
ini_up:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 0x20
.r:
        ret

; ---- buscar la sección cuya cabecera es key_buf -> CF si no está ----
; -> [sec_ptr] = la línea siguiente a la cabecera
ini_find:
        mov si, ini_buf
.l:
        push si
        call ini_skipsp
        cmp byte [si], '['
        jne .no
        mov di, key_buf
.c:
        mov al, [si]
        call ini_up
        cmp al, [di]
        jne .no
        inc si
        inc di
        cmp al, ']'
        jne .c
        pop ax
        call ini_nextline
        jc .nf
        mov [sec_ptr], si
        clc
        ret
.no:
        pop si
        call ini_nextline
        jnc .l
.nf:
        stc
        ret

; ---- SI = nombre con su «=», en mayúsculas -> SI = el valor, CF si no está ----
ini_get:
        mov di, si
        mov si, [sec_ptr]
.l:
        push si
        push di
        call ini_skipsp
        cmp byte [si], '['
        je .end
.c:
        mov al, [si]
        call ini_up
        cmp al, [di]
        jne .no
        inc si
        inc di
        cmp al, '='
        jne .c
        add sp, 4
        call ini_skipsp
        clc
        ret
.no:
        pop di
        pop si
        call ini_nextline
        jnc .l
        stc
        ret
.end:
        pop di
        pop si
        stc
        ret

; SI -> número en hexadecimal (con o sin «h») -> AL; CF si no hay
ini_hex:
        call ini_skipsp
        xor ah, ah
        xor cx, cx
.l:
        mov al, [si]
        call ini_up
        sub al, '0'
        jb .e
        cmp al, 9
        jbe .d
        sub al, 7
        cmp al, 10
        jb .e
        cmp al, 15
        ja .e
.d:
        shl ah, 4
        or ah, al
        inc si
        inc cx
        jmp .l
.e:
        mov al, [si]
        call ini_up
        cmp al, 'H'
        jne .x
        inc si
.x:
        mov al, ah
        cmp cx, 1
        jb .r                       ; (CF = 1: ni una cifra)
        cmp cx, 3
        cmc                         ; (más de dos cifras: CF = 1)
.r:
        ret
; SI -> número en decimal -> AX; CF si no hay
ini_dec:
        call ini_skipsp
        xor ax, ax
        xor cx, cx
.l:
        movzx bx, byte [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        imul ax, ax, 10
        add ax, bx
        inc si
        inc cx
        jmp .l
.e:
        cmp cx, 1
        ret

; SI -> ruta «14h(0) 0Ch(1) 02h» -> p_node, p_idx, plen; CF si está mal
ini_route:
        xor di, di
.l:
        cmp di, 5
        jae .bad
        call ini_hex
        jc .bad
        mov [p_node+di], al
        inc di
        mov ax, di
        mov [plen], al
        call ini_skipsp
        cmp byte [si], '('
        jne .end
        inc si
        call ini_dec
        jc .bad
        cmp byte [si], ')'
        jne .bad
        inc si
        cmp ax, 15
        ja .bad
        mov [p_idx+di-1], al
        jmp .l
.end:
        cmp byte [plen], 2
        jb .bad
        clc
        ret
.bad:
        stc
        ret

; ---- la cabecera de la tarjeta: «[HDA ffff:dddd ffff:ssss ffff:cccc ffff:ssss]»
; (lr_pid, lr_psub: PCI 00h y 2Ch; lr_cid, lr_csub: el códec) -> key_buf
; (DI = detrás de «[HDA » o «[AC97 »)
mk_key:
        mov eax, [lr_pid]
        call .lohi
        mov eax, [lr_psub]
        call .lohi
        mov eax, [lr_cid]
        call .hilo
        cmp byte [key_buf+1], 'A'   ; (AC'97: sin el subsistema del códec)
        je .end
        mov eax, [lr_csub]
        call .hilo
.end:
        mov byte [di-1], ']'
        mov byte [di], 0
        ret
.lohi:
        rol eax, 16
.hilo:
        push eax
        shr eax, 16
        call .h4
        mov byte [di], ':'
        inc di
        pop eax
        call .h4
        mov byte [di], ' '
        inc di
        ret
.h4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov [di], al
        inc di
        pop ax
        loop .h
        ret

; ---- las secciones de HD Audio: primero, solo lo aprendido ----
; Por cada controladora HD Audio, hda_init con lrn_mode: cada códec busca su
; sección (codec_learn). CF si ninguna sirve (y lrn_why dice por qué).
hda_learn_pass:
        cmp byte [ini_ok], 0
        je .no
        call smb_read
        mov byte [lrn_mode], 1
        mov word [lr_idx], 0
.l:
        mov ax, 0xB103
        mov ecx, 0x040300
        mov si, [lr_idx]
        int 0x1A
        jc .end
        mov [pci_bdf], bx
        call hda_init
        jnc .ok
        inc word [lr_idx]
        cmp word [lr_idx], 4
        jb .l
.end:
        mov byte [lrn_mode], 0
        cmp word [lr_idx], 0        ; sin HD Audio: lo dirá el AC'97
        je .no
        call lrn_reason
.no:
        stc
        ret
.ok:
        mov byte [lrn_mode], 0
        mov byte [lrn_why], 1
        clc
        ret

; por qué no se ha usado lo aprendido -> lrn_why
lrn_reason:
        mov al, 4
        cmp byte [lr_bad], 0
        jne .w
        mov al, 3
        cmp byte [lr_nos], 0
        jne .w
        mov al, 2
        cmp byte [ini_nsec], 0
        jne .w
        xor al, al
.w:
        mov [lrn_why], al
        ret

; ---- el códec hda_cad, con su sección de SONIDO.INI -> CF si no ----
codec_learn:
        call find_afg
        jc .r0
        ; la cabecera: el identificador y el subsistema del códec
        push bx
        xor bl, bl
        xor cl, cl
        call hda_param
        pop bx
        jc .r0
        mov [lr_cid], eax
        mov ax, 0xF20
        xor cl, cl
        call hda_verb
        jc .r0
        mov [lr_csub], eax
        mov dword [key_buf], '[HDA'
        mov byte [key_buf+4], ' '
        mov di, key_buf + 5
        call mk_key
        call ini_find
        jc .r0
        mov si, i_estado
        call ini_get
        jc .nos
        mov di, i_suena
        call ini_word
        jne .nos
        ; las rutas: primero se comprueba todo, sin tocar nada
        call afg_range
        mov si, i_ruta
        call ini_get
        jc .bad
        call ini_route
        jc .bad
        call route_check
        jc .bad
        call .save1
        mov byte [lr_hp], 0
        mov si, i_rutah
        call ini_get
        jc .nh
        call ini_route
        jc .bad
        call route_check
        jc .bad
        mov al, [p_node]
        cmp al, [lr_r1+1]           ; (el mismo pin: ya está)
        je .nh
        mov byte [lr_hp], 1
        mov si, plen
        mov di, lr_r2
        mov cx, 13
        push cs
        pop es
        rep movsb
.nh:
        ; adelante: encendido D0, como codec_setup
        mov bl, [hda_afg]
        mov ax, 0x705
        xor cl, cl
        call hda_verb
        call delay
        call delay
        xor al, al
        mov [hda_npin], al
        mov [hda_nvol], al
        mov [hda_dac], al
        mov [no_hw], al
        mov dword [hda_dacs], 0
        mov word [new_rates], 0x7FF
        ; GPIO: las de la prueba (salvo que se den /GPIO o /NOGPIO)
        mov al, [have_gpio]
        or al, [opt_nogpio]
        jz .lg
        call gpio_apply
        jmp .ge
.lg:
        mov si, i_gpio
        call ini_get
        jc .ge
        call ini_hex
        jc .ge
        mov [lrn_g], al
        call ini_hex
        jc .ge
        mov [lrn_g+1], al
        call ini_hex
        jc .ge
        mov [lrn_g+2], al
        mov byte [lrn_gpio], 1
        mov bl, [hda_afg]
        mov ax, 0x716
        mov cl, [lrn_g]
        call hda_verb
        mov ax, 0x717
        mov cl, [lrn_g+1]
        call hda_verb
        mov ax, 0x715
        mov cl, [lrn_g+2]
        call hda_verb
.ge:
        ; los caminos, con el control del pin de la prueba
        mov si, lr_r1
        call .load
        mov si, i_control
        call .ctl
        call path_apply
        cmp byte [lr_hp], 0
        je .p2
        mov si, lr_r2
        call .load
        mov si, i_controlh
        call .ctl
        call path_apply
.p2:
        mov byte [lrn_ctlv], 0xFF
        call pins_off
        ; EAPD (salvo /EAPD:n)
        cmp byte [have_eapd], 0
        jne .eo
        mov si, i_eapd
        call ini_get
        jc .eo
        mov al, [si]
        call ini_up
        cmp al, 'E'
        jne .e0
        mov byte [opt_eapd], 1
        mov byte [lrn_eapd], 1
        jmp .eo
.e0:
        cmp al, 'A'
        jne .eo
        mov byte [opt_eapd], 0
        mov byte [lrn_eapd], 2
.eo:
        ; volumen por programa, si en la prueba no bajaba
        mov si, i_volumen
        call ini_get
        jc .vo
        mov al, [si]
        call ini_up
        cmp al, 'P'
        jne .vo
        mov byte [no_hw], 1
        mov byte [lrn_soft], 1
.vo:
        call setup_tail
        ; 44,1 kHz si fue así como sonó
        mov si, i_frec
        call ini_get
        jc .fo
        call ini_dec
        jc .fo
        cmp ax, 44100
        jne .fo
        mov word [hda_rates], 0x0020
        mov byte [lrn_44], 1
.fo:
        ; AMD/ATI: «snoop» (registro PCI 42h)
        mov si, i_snoop
        call ini_get
        jc .so
        mov al, [si]
        call ini_up
        cmp al, 'A'
        jne .so
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x42
        int 0x1A
        and cl, 0xF8
        or cl, 2
        mov bx, [pci_bdf]
        mov ax, 0xB10B
        mov di, 0x42
        int 0x1A
        mov byte [lrn_snoop], 1
.so:
        call lrn_common
        clc
        ret
.nos:
        mov byte [lr_nos], 1
        stc
        ret
.bad:
        mov byte [lr_bad], 1
.r0:
        stc
        ret
; la ruta comprobada, a lr_r1 (plen, p_node y p_idx, seguidos: 13 octetos)
.save1:
        mov si, plen
        mov di, lr_r1
        mov cx, 13
        push cs
        pop es
        rep movsb
        ret
.load:
        mov di, plen
        mov cx, 13
        push cs
        pop es
        rep movsb
        ret
; SI = «Control=» -> lrn_ctlv (FFh si no está: el de siempre)
.ctl:
        mov byte [lrn_ctlv], 0xFF
        call ini_get
        jc .cr
        call ini_hex
        jc .cr
        mov [lrn_ctlv], al
.cr:
        ret

; SI = valor, DI = palabra en mayúsculas -> ZF si el valor es esa palabra
ini_word:
.l:
        mov al, [di]
        test al, al
        jz .end
        mov al, [si]
        call ini_up
        cmp al, [di]
        jne .r
        inc si
        inc di
        jmp .l
.end:
        mov al, [si]                ; detrás: fin de línea, espacio o «;»
        cmp al, ' '
        jbe .y
        cmp al, ';'
.r:
        ret
.y:
        cmp al, al
        ret

; la fecha y el equipo de la sección
lrn_common:
        mov si, i_fecha
        call ini_get
        jc .nf
        mov di, lrn_date
        mov cx, 16
.f:
        lodsb
        cmp al, ' '
        jb .fe
        mov [di], al
        inc di
        loop .f
.fe:
        mov byte [di], 0
.nf:
        ; ¿el mismo modelo de equipo? (SMBIOS: solo como ayuda)
        cmp byte [smb_ok], 0
        je .r
        mov si, i_equipo
        call ini_get
        jc .r
        mov di, smb_buf
        mov byte [lrn_smb], 2
.c:
        mov al, [di]
        test al, al
        jz .ce
        cmp al, [si]
        jne .r
        inc si
        inc di
        jmp .c
.ce:
        mov al, [si]
        inc si
        cmp al, ' '
        je .ce
        cmp al, 9
        je .ce
        cmp al, 13
        je .same
        cmp al, 10
        je .same
        test al, al
        jnz .r
.same:
        mov byte [lrn_smb], 1
.r:
        ret

; ---- p_node/p_idx/plen: ¿es un camino de verdad en este códec? CF si no ----
; pin con salida, nodos del grupo de audio, mezcladores o selectores en
; medio, un DAC analógico al final y cada nodo unido al siguiente por la
; entrada que dice la ruta
route_check:
        xor si, si
.l:
        mov bl, [p_node+si]
        mov al, bl
        sub al, [w_first]
        jb .bad
        cmp al, [w_count]
        jae .bad
        call w_type
        movzx cx, byte [plen]
        dec cx
        test si, si
        jnz .mid
        cmp ax, 4                   ; el primero, un pin analógico
        jne .bad
        push ax
        mov cl, 0x0C
        call hda_param
        mov dl, al
        pop ax
        jc .bad
        test dl, 0x10               ; que pueda sacar señal
        jz .bad
        jmp .conn
.mid:
        cmp si, cx
        je .last
        cmp al, 2
        je .conn
        cmp al, 3
        jne .bad
.conn:
        ; la entrada elegida tiene que ser el nodo siguiente
        push si
        mov di, conn_bufs
        call get_conn
        pop si
        movzx di, byte [p_idx+si]
        cmp di, cx
        jae .bad
        mov al, [conn_bufs+di]
        cmp al, [p_node+si+1]
        jne .bad
        inc si
        jmp .l
.last:
        test ax, ax                 ; un DAC analógico
        jnz .bad
        clc
        ret
.bad:
        stc
        ret

; los pines con salida que no están en ningún camino: apagados (como en la
; prueba)
pins_off:
        mov bl, [w_first]
        movzx cx, byte [w_count]
        jcxz .r
.w:
        push cx
        call w_type
        cmp ax, 4
        jne .n
        mov cl, 0x0C
        call hda_param
        jc .n
        test al, 0x10
        jz .n
        mov si, hda_pins
        movzx cx, byte [hda_npin]
.u:
        jcxz .off
        cmp [si], bl
        je .n
        add si, 5
        dec cx
        jmp .u
.off:
        mov ax, 0xF07
        xor cl, cl
        call hda_verb
        jc .n
        and al, 0x3F
        mov cl, al
        mov ax, 0x707
        call hda_verb
.n:
        pop cx
        inc bl
        loop .w
.r:
        ret

; ---- AC'97 (después de ac97_init): ¿hay una sección suya? ----
ac97_learn:
        cmp byte [ini_ok], 0
        je .r
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        xor di, di
        int 0x1A
        mov [lr_pid], ecx
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        mov di, 0x2C
        int 0x1A
        mov [lr_psub], ecx
        mov dx, [nam]
        add dx, 0x7C
        in ax, dx
        shl eax, 16
        add dx, 2
        in ax, dx
        mov [lr_cid], eax
        mov dword [key_buf], '[AC9'
        mov word [key_buf+4], '7 '
        mov di, key_buf + 6
        call mk_key
        call ini_find
        jc .other
        mov si, i_estado
        call ini_get
        jc .nos
        mov di, i_suena
        call ini_word
        jne .nos
        call smb_read
        mov byte [lrn_eapd], 1
        mov si, i_eapd
        call ini_get
        jc .ok
        mov al, [si]
        call ini_up
        cmp al, 'I'
        jne .ok
        ; EAPD al revés y las salidas mono, central y traseras abiertas
        mov byte [lrn_eapd], 2
        mov dx, [nam]
        add dx, 0x26
        in ax, dx
        xor ax, 0x8000
        out dx, ax
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
.ok:
        call lrn_common
        mov byte [lrn_why], 1
        jmp lrn_say
.nos:
        mov byte [lr_nos], 1
.other:
        call lrn_reason
.r:
        jmp lrn_say

; ---- lo que se ha hecho con lo aprendido, en una línea ----
lrn_say:
        cmp byte [lrn_said], 0
        jne .r
        movzx bx, byte [lrn_why]
        test bx, bx
        jz .r
        mov byte [lrn_said], 1
        cmp bx, 1
        jne .w
        mov si, msg_l_used
        call puts
        mov si, lrn_date
        call puts
        mov si, msg_l_used2
        call puts
        cmp byte [lrn_smb], 2
        jne .r
        mov si, msg_l_other
        jmp puts
.w:
        shl bx, 1
        mov si, [l_whys+bx]
        jmp puts
.r:
        ret

; ---- /INFO: lo aprendido ----
lrn_info:
        cmp byte [lrn_why], 1
        je .used
        jmp lrn_say
.used:
        mov si, msg_l_used
        call puts
        mov si, lrn_date
        call puts
        mov si, msg_l_colon
        call puts
        cmp byte [dev_type], 4
        je .hda
        mov si, msg_l_acn
        cmp byte [lrn_eapd], 2
        jne .ac
        mov si, msg_l_aci
.ac:
        call puts
        jmp .smb
.hda:
        ; cada camino: ruta (con la entrada elegida) y control del pin
        xor bx, bx
.p:
        cmp bl, [hda_npin]
        jae .opt
        mov si, msg_l_ruta
        call puts
        mov di, bx
        imul di, di, 5
        add di, hda_pins
        mov si, bx
        shl si, 2
        add si, hda_pidx
        mov cx, 5
.n:
        mov al, [di]
        call hex2
        mov al, 'h'
        call putc
        dec cx
        jz .ne
        cmp byte [di+1], 0
        je .ne
        mov al, '('
        call putc
        mov al, [si]
        call hex1
        mov al, ')'
        call putc
        mov al, ' '
        call putc
        inc si
        inc di
        jmp .n
.ne:
        mov si, msg_l_ctl
        call puts
        mov al, [hda_pctl+bx]
        call hex2
        mov si, msg_l_h
        call puts
        inc bx
        jmp .p
.opt:
        mov si, msg_l_sp
        call puts
        mov si, msg_l_eapd0
        cmp byte [lrn_eapd], 0
        je .e
        mov si, msg_l_eapd1
        cmp byte [lrn_eapd], 1
        je .e
        mov si, msg_l_eapd2
.e:
        call puts
        mov si, msg_l_gnone
        cmp byte [lrn_gpio], 0
        je .g0
        mov si, msg_l_gm
        call puts
        mov al, [lrn_g]
        call hex2
        mov si, msg_l_gd
        call puts
        mov al, [lrn_g+1]
        call hex2
        mov si, msg_l_gv
        call puts
        mov al, [lrn_g+2]
        call hex2
        mov si, msg_l_h0
.g0:
        call puts
        cmp byte [lrn_snoop], 0
        je .s0
        mov si, msg_l_snoop
        call puts
.s0:
        cmp byte [lrn_44], 0
        je .f0
        mov si, msg_l_44
        call puts
.f0:
        cmp byte [lrn_soft], 0
        je .v0
        mov si, msg_l_soft
        call puts
.v0:
        mov si, msg_crlf2
        call puts
.smb:
        cmp byte [lrn_smb], 2
        jne .x
        mov si, msg_l_other
        call puts
.x:
        ret
hex1:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        jmp putc

; ---- SMBIOS: «fabricante | modelo | versión» del equipo (tipo 1) -> smb_buf
; (igual que AUDICION, para comparar con «Equipo=»). Solo en modo real.
smb_read:
        cmp byte [smb_done], 0
        jne .r
        mov byte [smb_done], 1
        smsw ax
        test al, 1
        jnz .r
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        call a20_open
        call unreal
        push es
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
.f3:
        cmp dword [es:di], '_SM3'
        jne .n3
        cmp byte [es:di+4], '_'
        jne .n3
        cmp dword [es:di+0x14], 0
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
        jmp .walk
.n3:
        add di, 16
        jnz .f3
        pop es
        jmp .x
.got:
        mov eax, [es:di+0x18]
        mov [sm_addr], eax
        mov ax, [es:di+0x16]
        mov [sm_len], ax
        mov ax, [es:di+0x1C]
        mov [sm_cnt], ax
.walk:
        pop es
        mov ebx, [sm_addr]
        movzx edx, word [sm_len]
        add edx, ebx
        mov cx, [sm_cnt]
.st:
        test cx, cx
        jz .x
        lea eax, [ebx+4]
        cmp eax, edx
        jae .x
        mov al, [fs:ebx]
        cmp al, 127
        je .x
        cmp al, 1
        je .t1
        ; la siguiente: la parte con formato y las cadenas (acaban en 00 00)
        movzx eax, byte [fs:ebx+1]
        cmp al, 4
        jb .x
        add ebx, eax
.z:
        cmp ebx, edx
        jae .x
        mov ax, [fs:ebx]
        inc ebx
        test ax, ax
        jnz .z
        inc ebx
        dec cx
        jmp .st
.t1:
        mov di, smb_buf
        mov al, [fs:ebx+4]
        call .str
        call .bar
        mov al, [fs:ebx+5]
        call .str
        call .bar
        mov al, [fs:ebx+6]
        call .str
        mov byte [di], 0
        mov byte [smb_ok], 1
.x:
        call a20_restore
.r:
        ret
.bar:
        mov byte [di], ' '
        mov word [di+1], '| '
        add di, 3
        ret
; AL = número de cadena de la estructura EBX -> a DI (como AUDICION: «-» si
; no hay, hasta 64 caracteres, sin los de fuera de 20h-7Eh)
.str:
        push ebx
        push cx
        mov dl, al
        test dl, dl
        jz .none
        movzx eax, byte [fs:ebx+1]
        add ebx, eax
.sk:
        dec dl
        jz .p
        mov cx, 200
.k:
        mov al, [fs:ebx]
        inc ebx
        test al, al
        loopnz .k
        cmp byte [fs:ebx], 0
        je .none
        jmp .sk
.p:
        mov cx, 64
.pl:
        mov al, [fs:ebx]
        inc ebx
        cmp al, ' '
        jb .sr
        cmp al, 126
        ja .q
        mov [di], al
        inc di
.q:
        loop .pl
        jmp .sr
.none:
        mov byte [di], '-'
        inc di
.sr:
        pop cx
        pop ebx
        ret

i_estado    db "ESTADO=", 0
i_suena     db "SUENA", 0
i_ruta      db "RUTA=", 0
i_rutah     db "RUTAAURICULARES=", 0
i_control   db "CONTROL=", 0
i_controlh  db "CONTROLAURICULARES=", 0
i_gpio      db "GPIO=", 0
i_eapd      db "EAPD=", 0
i_snoop     db "SNOOP=", 0
i_frec      db "FRECUENCIA=", 0
i_volumen   db "VOLUMEN=", 0
i_fecha     db "FECHA=", 0
i_equipo    db "EQUIPO=", 0
ini_path    db "?:\HISPADOS\SONIDO.INI", 0
ini_room    dw 0
ini_ok      db 0                    ; leído y se puede usar lo aprendido
ini_lei     db 0                    ; leído (para el mezclador)
ini_tried   db 0
ini_nsec    db 0
sec_ptr     dw 0
key_buf     times 64 db 0
lrn_mode    db 0
lrn_ctlv    db 0xFF
lrn_said    db 0
lr_idx      dw 0
lr_pid      dd 0
lr_psub     dd 0
lr_cid      dd 0
lr_csub     dd 0
lr_bad      db 0
lr_nos      db 0
lr_hp       db 0
lr_r1       times 13 db 0
lr_r2       times 13 db 0
smb_done    db 0
smb_ok      db 0
sm_addr     dd 0
sm_len      dw 0
sm_cnt      dw 0
smb_buf     times 3 * 64 + 8 db 0
l_whys      dw 0, 0, msg_l_otra, msg_l_nos, msg_l_bad, msg_l_noapr
msg_l_used  db "Configuración aprendida en la prueba (", 0
msg_l_used2 db ").", 13, 10, 0
msg_l_colon db "):", 13, 10, 0
msg_l_other db "  (La prueba se hizo en otro modelo de equipo con la misma tarjeta.)", 13, 10, 0
msg_l_otra  db "Lo aprendido en la prueba de sonido (SONIDO.INI) es de otra tarjeta:", 13, 10
            db "se busca la salida automáticamente.", 13, 10, 0
msg_l_nos   db "En la prueba de sonido no se encontró un camino para esta tarjeta:", 13, 10
            db "se busca la salida automáticamente.", 13, 10, 0
msg_l_bad   db "Lo aprendido en la prueba de sonido (SONIDO.INI) no cuadra con esta", 13, 10
            db "tarjeta: se busca la salida automáticamente.", 13, 10, 0
msg_l_noapr db "/NOAPRENDIDO: no se usa lo aprendido en la prueba de sonido.", 13, 10, 0
msg_l_ruta  db "  ruta ", 0
msg_l_ctl   db ", control del pin ", 0
msg_l_h     db "h", 13, 10, 0
msg_l_h0    db "h", 0
msg_l_sp    db "  ", 0
msg_l_eapd0 db "EAPD como siempre", 0
msg_l_eapd1 db "EAPD encendido", 0
msg_l_eapd2 db "EAPD apagado", 0
msg_l_gnone db ", GPIO sin tocar", 0
msg_l_gm    db ", GPIO máscara ", 0
msg_l_gd    db "h dirección ", 0
msg_l_gv    db "h datos ", 0
msg_l_snoop db ", «snoop» activado", 0
msg_l_44    db ", siempre a 44100 Hz", 0
msg_l_soft  db ", volumen por programa", 0
msg_l_acn   db "  EAPD normal", 13, 10, 0
msg_l_aci   db "  EAPD invertido (y las salidas mono, central y traseras abiertas)", 13, 10, 0

hda_idx     dw 0
tsr_end     dw 0
msg_title   db "HSPSND 0.1 - controlador de sonido de Hispa-DOS", 13, 10, 0
msg_already db "Ya está instalado.", 13, 10, 0
msg_ac97    db "Audio Intel AC'97 en E/S ", 0
msg_vra     db ", frecuencia variable", 13, 10, 0
msg_novra   db ", 48 kHz fijos", 13, 10, 0
msg_hda     db "Intel HD Audio en ", 0
msg_codec   db ", códec ", 0
msg_pin     db ", salidas:", 0
msg_oldver  db "Hay cargado un HSPSND de otra versión.", 13, 10, 0
msg_badopt  db "Opción no válida.", 13, 10, 0
msg_sb_noemm db "/SB: hace falta HSPEMM.SYS (en CONFIG.SYS) para emular la Sound Blaster.", 13, 10, 0
msg_sb_real db "/SB: hay una Sound Blaster de verdad; no se emula.", 13, 10, 0
msg_sb_emu  db "Sound Blaster 16 emulada", 0
msg_usage   db "Uso: HSPSND [/SB] [/VOL:n] [/INFO] [/NOSB] [/NOTECLAS] [/GPIO:hh] [/NOGPIO]", 13, 10
            db "            [/EAPD:0] [/NOAPRENDIDO]", 13, 10
            db "  /SB        Sound Blaster 16 emulada para los juegos, sobre el HD Audio o el", 13, 10
            db "             AC'97 (hace falta HSPEMM.SYS)", 13, 10
            db "  /VOL:n     volumen del 0 al 100 % (también con el driver ya cargado)", 13, 10
            db "  /INFO      muestra la tarjeta, el códec, las salidas y el camino elegido", 13, 10
            db "  /NOSB      no buscar una Sound Blaster", 13, 10
            db "  /SB8       Sound Blaster 16: tocar por la vía de 8 bits (como una SB Pro)", 13, 10
            db "  /NOTECLAS  sin teclas de volumen (Ctrl+Alt+flechas arriba y abajo,", 13, 10
            db "             Ctrl+Alt+Fin para el silencio y las teclas multimedia)", 13, 10
            db "  /GPIO:hh   HD Audio: valor (hex) de las patillas GPIO del códec, que en", 13, 10
            db "             muchos portátiles encienden los altavoces (si no, todas a 1)", 13, 10
            db "  /NOGPIO    HD Audio: no tocar las GPIO", 13, 10
            db "  /EAPD:0    HD Audio: apagar EAPD en vez de encenderlo", 13, 10
            db "  /NOAPRENDIDO  no usar lo aprendido en la prueba de sonido", 13, 10
            db "Si la prueba de sonido (AUDICION) hizo sonar este equipo, HSPSND pone ese", 13, 10
            db "mismo camino (\HISPADOS\SONIDO.INI) en vez de buscarlo.", 13, 10
            db "El volumen se cambia después con SONIDO.", 13, 10, 0
msg_vol     db "Volumen: ", 0
msg_pct     db " %", 0
msg_muted   db " (en silencio)", 0
msg_i_card  db "Tarjeta: ", 0
msg_i_sbmix db "Mezclador: maestro ", 0
msg_i_sbvoz db ", voz ", 0
msg_i_pci   db ", PCI ", 0
msg_i_en    db " en ", 0
msg_i_io    db ", E/S ", 0
msg_i_reg   db ", registros ", 0
msg_i_sts   db ", códecs que responden (STATESTS) ", 0
msg_i_accodec db 13, 10, "Códec AC'97 ", 0
msg_i_acmas db "; maestro ", 0
msg_i_achp  db ", auriculares ", 0
msg_i_acpcm db ", PCM ", 0
msg_i_sbemu db "Sound Blaster 16 emulada para los juegos (/SB): en marcha.", 13, 10, 0
msg_i_keys  db "Teclas de volumen: Ctrl+Alt+flecha arriba/abajo, Ctrl+Alt+Fin (silencio).", 13, 10, 0
msg_i_nokeys db "Teclas de volumen: no.", 13, 10, 0
msg_i_v86   db "En modo protegido (EMM386 que no es HSPEMM) no se puede consultar el códec.", 13, 10, 0
msg_i_codec db "Códec ", 0
msg_i_used  db " (en uso), frecuencias ", 0
msg_i_afg   db "AFG ", 0
msg_i_nodes db ", nodos ", 0
msg_i_subsys db ", subsistema ", 0
msg_i_gpio  db ", GPIO ", 0
msg_i_gdat  db ": datos ", 0
msg_i_gmask db " másc. ", 0
msg_i_gdir  db " dir. ", 0
msg_i_pin   db "Pin ", 0
msg_i_eapd  db " EAPD=", 0
msg_i_unused db " (no usado)", 0
msg_i_arrow db " -> ", 0
msg_i_nc    db "Pines sin conectar:", 0
msg_i_soft  db "Volumen por programa (el camino no tiene amplificador regulable).", 13, 10, 0
msg_i_amp   db "Amplificador de volumen: nodo ", 0
msg_i_out   db " (salida), 0 dB=", 0
msg_i_in    db " (entrada), 0 dB=", 0
msg_i_of    db " de ", 0
msg_i_stp   db " pasos de ", 0
msg_i_now   db " dB, ahora ", 0
i_names     dw name_none, name_sb, name_sb16, name_ac97, name_hda
i_devs      dw d_0, d_1, d_2, d_3, d_4, d_5, d_6, d_7, d_8, d_9, d_a, d_b, d_c, d_d, d_e, d_f
i_conns     dw c_0, c_1, c_2, c_3
d_0         db "línea", 0
d_1         db "altavoz", 0
d_2         db "auriculares", 0
d_3         db "CD", 0
d_4         db "S/PDIF", 0
d_5         db "salida digital", 0
d_6         db "módem (línea)", 0
d_7         db "módem (teléfono)", 0
d_8         db "entrada de línea", 0
d_9         db "auxiliar", 0
d_a         db "micrófono", 0
d_b         db "telefonía", 0
d_c         db "entrada S/PDIF", 0
d_d         db "entrada digital", 0
d_e         db "reservado", 0
d_f         db "otro", 0
c_0         db "conector", 0
c_1         db "sin conectar", 0
c_2         db "interno", 0
c_3         db "conector e interno", 0
msg_crlf2   db 13, 10, 0
msg_hdafail db "Hay un controlador HD Audio, pero no se ha podido poner en marcha.", 13, 10, 0
msg_nocard  db "No se ha encontrado ninguna tarjeta de sonido compatible.", 13, 10, 0
msg_fail    db "El códec AC'97 no responde.", 13, 10, 0
msg_mz      db "Mezclador:", 0
msg_mz_sil  db " (silencio)", 0
msg_mz_son  db " suena", 0
msg_mz_gra  db " graba", 0
msg_mz_sep  db ",", 0
msg_mz_nl   db 13, 10, "          ", 0
mz_nombres  dw mzn0, mzn1, mzn2, mzn3, mzn4, mzn5, mzn6
mzn0        db " General ", 0
mzn1        db " Sonidos ", 0
mzn2        db " Música ", 0
mzn3        db " CD ", 0
mzn4        db " Línea ", 0
mzn5        db " Micrófono ", 0
mzn6        db " Altavoz ", 0

; ---- /INFO: el mezclador («Mezclador: General 80 %, Sonidos 90 %, ...») ----
mz_info:
        cmp byte [mz_src], 0
        je .r
        mov si, msg_mz
        call puts
        xor bx, bx
        xor dx, dx                  ; DL = cuántas en esta línea
.f:
        mov al, [mz_src]
        mov cl, bl
        shr al, cl
        test al, 1
        jz .n
        test dh, dh
        jz .p
        mov si, msg_mz_sep
        call puts
        cmp dl, 4
        jb .p
        mov si, msg_mz_nl
        call puts
        xor dl, dl
.p:
        mov dh, 1
        inc dl
        push bx
        shl bx, 1
        mov si, [mz_nombres+bx]
        pop bx
        call puts
        movzx ax, byte [mz_vol+bx]
        call pdec
        mov si, msg_pct
        call puts
        mov al, [mz_flg+bx]
        test al, MZF_MUDO
        jz .s
        mov si, msg_mz_sil
        call puts
.s:
        mov cl, bl
        mov ah, [mz_son]
        shr ah, cl
        test ah, 1
        jz .g
        test byte [mz_flg+bx], MZF_SONAR
        jz .g
        mov si, msg_mz_son
        call puts
.g:
        mov ah, [mz_rec]
        shr ah, cl
        test ah, 1
        jz .n
        test byte [mz_flg+bx], MZF_GRABAR
        jz .n
        mov si, msg_mz_gra
        call puts
.n:
        inc bx
        cmp bx, MZ_N
        jb .f
        mov si, msg_crlf2
        call puts
.r:
        ret

%include "hspmezi.inc"
file_end:
; (el anillo de la Sound Blaster emulada tiene que tapar todo el código de
; instalación: la ventana de los registros va detrás)
%if file_end - sbemu_end > NBUF * BUFSZ
%error "la instalación no cabe bajo el anillo"
%endif
