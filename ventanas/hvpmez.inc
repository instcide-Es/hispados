; =============================================================================
;  Hispa Ventanas - el mezclador de HSPSND para los módulos .HVP
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lo usan el Mezclador (MEZCLAD.HVP) y Configuración (CONFIG.HVP). Se
;  ensambla dentro del módulo, detrás de hvplib.inc (%include "hvpmez.inc").
;  Habla con HSPSND por la entrada snd_mezcla de la interfaz (sus funciones
;  09h-0Fh, versión 4) y guarda el estado en el único sitio que hay para el
;  sonido: la sección [Mezclador] de \HISPADOS\SONIDO.INI, la misma que lee
;  HSPSND al cargarse (formato en docs/MEZCLADOR.md). Se guarda siempre, sin
;  preguntar: HSPSND apunta que algo ha cambiado (lo cambie quien lo cambie:
;  el Mezclador, el altavoz de la barra, las teclas de volumen, SONIDO) y
;  mz_vigila, que los dos módulos llaman cada segundo, lo escribe.
;
;    mz_ruta     ESI=ruta del módulo: la de SONIDO.INI en su unidad -> mzs_ruta
;    mz_leer     todo lo de HSPSND a mzs_* -> CF=1 si no hay HSPSND con mezclador
;    mz_fuente   AL=fuente BL=volumen BH=casillas CL=balance (FFh no cambia)
;                -> HSPSND (y a mzs_*, como queda)
;    mz_ajuste   AL=ajuste BL=valor -> HSPSND (y a mzs_aj)
;    mz_guardar  escribe la sección [Mezclador] (mzs_*) en mzs_ruta: si ya
;                está, en su sitio; si no, detrás de los comentarios del
;                principio; lo demás del archivo (lo aprendido en la prueba
;                de sonido) no se toca. -> CF=1 si no se puede
;    mz_vigila   si HSPSND dice que hay cambios sin guardar: mz_leer,
;                mz_guardar y se lo dice (función 0Fh)
;  mzs_vista: 'S' (sencilla), 'C' (completa) o 0; se guarda como «Vista=» y
;  con 0 se deja la que haya en el archivo (la usa solo el Mezclador).
; =============================================================================

MZ_N        equ 7
MZF_MUDO    equ 1
MZF_SONAR   equ 2
MZF_GRABAR  equ 4
MZC_BAL     equ 0x0001
MZC_GRAVES  equ 0x0002
MZC_AGUDOS  equ 0x0004
MZC_GENT    equ 0x0008
MZC_GSAL    equ 0x0010
MZC_AGC     equ 0x0020
MZC_UNA     equ 0x0040
MZC_ALT4    equ 0x0080
MZC_REFUERZO equ 0x0100
MZ_FMAX     equ 32767               ; lo que se lee de SONIDO.INI

mzs_vol     times MZ_N db 0
mzs_flg     times MZ_N db 0
mzs_bal     times MZ_N db 0
mzs_aj      times 6 db 0
mzs_src     db 0                    ; fuentes que hay
mzs_hw      db 0                    ; ... las que lleva la tarjeta
mzs_rec     db 0                    ; ... las que se graban
mzs_son     db 0                    ; ... las que tienen «Sonar»
mzs_ajs     dd 0                    ; ajustes que hay (MZC_*)
mzs_vista   db 0
mzs_ruta    times 84 db 0
mzs_nom     dd mzk0, mzk1, mzk2, mzk3, mzk4, mzk5, mzk6
mzk0        db "General", 0
mzk1        db "Sonidos", 0
mzk2        db "Musica", 0
mzk3        db "CD", 0
mzk4        db "Linea", 0
mzk5        db "Microfono", 0
mzk6        db "Altavoz", 0
mzs_ajn     dd mza0, mza1, mza2, mza3, mza4, mza5
mza0        db "Graves=", 0
mza1        db "Agudos=", 0
mza2        db "GananciaEntrada=", 0
mza3        db "GananciaSalida=", 0
mza4        db "AjusteMicrofono=", 0
mza5        db "RefuerzoMicrofono=", 0
mzs_t_sil   db "Silencio=", 0
mzs_t_son   db "Sonar=", 0
mzs_t_gra   db "Grabar=", 0
mzs_t_vis   db "Vista=", 0
mzs_t_sen   db "sencilla", 0
mzs_t_com   db "completa", 0
mzs_t_cab   db "[Mezclador]", 13, 10
            db "; Lo guarda Hispa Ventanas cada vez que cambia algo y HSPSND lo pone al", 13, 10
            db "; cargarse. Volumen y balance de 0 a 100 (balance: 50 = en medio).", 13, 10, 0
mzs_t_nuevo db "; El sonido de este equipo: lo leen y lo escriben HSPSND, AUDICION y", 13, 10
            db "; Hispa Ventanas (formato: docs/MEZCLADOR.md y docs/AUDICION.md).", 13, 10, 13, 10, 0
mzs_t_ruta  db ":\HISPADOS\SONIDO.INI", 0

; ESI = ruta del módulo -> mzs_ruta = «X:\HISPADOS\SONIDO.INI» (X, su unidad;
; si no la dice, la de arranque)
mz_ruta:
        pushad
        mov al, [esi]
        cmp byte [esi+1], ':'
        je .u
        mov eax, 0x3305
        int 0x21
        mov al, dl
        add al, 'A' - 1
.u:
        and al, 0xDF
        mov edi, mzs_ruta
        stosb
        mov esi, mzs_t_ruta
        call hvl_cad
        mov byte [edi], 0
        popad
        ret

; -> CF=1 si no hay HSPSND con mezclador; si no, mzs_* como está
mz_leer:
        pushad
        mov eax, 0x0900
        HV snd_mezcla
        jc .x
        mov [mzs_src], al
        mov [mzs_hw], ah
        movzx ebx, bx
        mov [mzs_ajs], ebx
        mov [mzs_rec], cl
        mov [mzs_son], dl
        xor esi, esi
.f:
        mov eax, esi
        mov ah, 0x0A
        HV snd_mezcla
        jc .x
        mov [mzs_vol+esi], al
        mov [mzs_flg+esi], ah
        mov [mzs_bal+esi], bl
        inc esi
        cmp esi, MZ_N
        jb .f
        xor esi, esi
.a:
        mov eax, esi
        mov ah, 0x0C
        HV snd_mezcla
        jc .x
        mov [mzs_aj+esi], al
        inc esi
        cmp esi, 6
        jb .a
        clc
.x:
        popad
        ret

mz_fuente:
        pushad
        movzx esi, al
        mov ah, 0x0B
        HV snd_mezcla
        jc .x
        cmp esi, MZ_N
        jae .x
        mov [mzs_vol+esi], al
        mov [mzs_flg+esi], ah
        mov [mzs_bal+esi], bl
        xor esi, esi                ; (con una sola fuente de grabación,
.g:                                 ; las demás pueden haber cambiado)
        mov eax, esi
        mov ah, 0x0A
        HV snd_mezcla
        jc .x
        mov [mzs_flg+esi], ah
        inc esi
        cmp esi, MZ_N
        jb .g
.x:
        popad
        ret

mz_ajuste:
        pushad
        movzx esi, al
        mov ah, 0x0D
        HV snd_mezcla
        jc .x
        cmp esi, 6
        jae .x
        mov [mzs_aj+esi], al
.x:
        popad
        ret

mz_vigila:
        pushad
        mov eax, 0x0F00
        HV snd_mezcla
        jc .x
        test al, al
        jz .x
        call mz_leer
        jc .x
        call mz_guardar
        jc .x
        mov eax, 0x0F01
        HV snd_mezcla
.x:
        popad
        ret

; ---- escribir la sección ----
mz_guardar:
        pushad
        cmp byte [mzs_ruta], 0
        je .mal
        mov esi, mzs_ruta
        mov edi, mzb_ent
        mov ecx, MZ_FMAX + 1
        call hvl_leer               ; -> EAX = octetos (0 si no está)
        mov [mzb_n], eax
        mov edi, mzb_sal
        test eax, eax
        jnz .hay
        mov esi, mzs_t_nuevo        ; un archivo nuevo: con su explicación
        call hvl_cad
        call mz_seccion
        jmp .escribe
.hay:
        call mz_buscar              ; -> EBX = principio, EDX = final (o -1)
        cmp ebx, -1
        jne .esta
        ; no está: detrás de los comentarios y líneas vacías del principio
        call mz_tras_coment         ; -> EBX
        mov esi, mzb_ent
        mov ecx, ebx
        rep movsb
        call mz_seccion
        mov esi, t_mz_crlf
        call hvl_cad
        lea esi, [mzb_ent+ebx]
        mov ecx, [mzb_n]
        sub ecx, ebx
        rep movsb
        jmp .escribe
.esta:
        mov esi, mzb_ent
        mov ecx, ebx
        rep movsb
        call mz_seccion
        lea esi, [mzb_ent+edx]
        mov ecx, [mzb_n]
        sub ecx, edx
        jz .escribe
        push esi                    ; (una línea en blanco antes de la siguiente)
        mov esi, t_mz_crlf
        call hvl_cad
        pop esi
        rep movsb
.escribe:
        mov ecx, edi
        sub ecx, mzb_sal
        mov esi, mzs_ruta
        mov edx, mzb_sal
        call hvl_escribir
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; EDI -> la sección entera (acaba en CR LF); EDI avanza
mz_seccion:
        push eax
        push ebx
        push ecx
        push esi
        mov esi, mzs_t_cab
        call hvl_cad
        xor ebx, ebx
.v:
        mov esi, [mzs_nom+ebx*4]
        call hvl_cad
        mov al, '='
        stosb
        movzx eax, byte [mzs_vol+ebx]
        call hvl_num
        mov al, ','
        stosb
        movzx eax, byte [mzs_bal+ebx]
        call hvl_num
        mov ax, 0x0A0D
        stosw
        inc ebx
        cmp ebx, MZ_N
        jb .v
        mov esi, mzs_t_sil
        mov cl, MZF_MUDO
        call .lista
        mov esi, mzs_t_son
        mov cl, MZF_SONAR
        call .lista
        mov esi, mzs_t_gra
        mov cl, MZF_GRABAR
        call .lista
        xor ebx, ebx
.a:
        mov esi, [mzs_ajn+ebx*4]
        call hvl_cad
        movzx eax, byte [mzs_aj+ebx]
        call hvl_num
        mov ax, 0x0A0D
        stosw
        inc ebx
        cmp ebx, 6
        jb .a
        mov al, [mzs_vista]
        test al, al
        jz .x
        mov esi, mzs_t_vis
        call hvl_cad
        mov esi, mzs_t_sen
        cmp al, 'S'
        je .vs
        mov esi, mzs_t_com
.vs:
        call hvl_cad
        mov ax, 0x0A0D
        stosw
.x:
        pop esi
        pop ecx
        pop ebx
        pop eax
        ret
; ESI = «Clave=», CL = casilla: las fuentes que la tienen, separadas por comas
.lista:
        call hvl_cad
        xor ebx, ebx
        xor ch, ch                  ; CH = ya hay alguna
.l:
        test [mzs_flg+ebx], cl
        jz .ln
        test ch, ch
        jz .l1
        mov al, ','
        stosb
.l1:
        mov ch, 1
        mov esi, [mzs_nom+ebx*4]
        call hvl_cad
.ln:
        inc ebx
        cmp ebx, MZ_N
        jb .l
        mov ax, 0x0A0D
        stosw
        ret

; -> EBX = donde empieza la línea «[Mezclador]» y EDX = donde empieza la
; sección siguiente (o el final); EBX = -1 si no está. Si mzs_vista es 0,
; la toma de su «Vista=»
mz_buscar:
        push eax
        push ecx
        push esi
        push edi
        xor esi, esi                ; ESI = principio de la línea
        or ebx, -1
.l:
        cmp esi, [mzb_n]
        jae .fin
        mov edi, esi
        call .blancos
        cmp byte [mzb_ent+edi], '['
        jne .no
        cmp ebx, -1
        jne .fin_sec                ; la siguiente sección
        push esi
        lea esi, [mzb_ent+edi+1]
        mov ecx, t_mz_sec
        call .igual
        pop esi
        jne .sig
        mov ebx, esi
        jmp .sig
.no:
        cmp ebx, -1                 ; dentro: ¿«Vista=»?
        je .sig
        cmp byte [mzs_vista], 0
        jne .sig
        push esi
        lea esi, [mzb_ent+edi]
        mov ecx, t_mz_vis
        call .igual
        jne .nv
        call .blancos2
        mov al, [esi]
        and al, 0xDF
        cmp al, 'S'
        je .vp
        cmp al, 'C'
        jne .nv
.vp:
        mov [mzs_vista], al
.nv:
        pop esi
.sig:
        cmp esi, [mzb_n]            ; a la línea siguiente
        jae .fin
        cmp byte [mzb_ent+esi], 10
        je .s2
        inc esi
        jmp .sig
.s2:
        inc esi
        jmp .l
.fin_sec:
        mov edx, esi
        jmp .x
.fin:
        mov edx, [mzb_n]
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret
.blancos:
        cmp edi, [mzb_n]
        jae .b
        cmp byte [mzb_ent+edi], ' '
        je .b1
        cmp byte [mzb_ent+edi], 9
        jne .b
.b1:
        inc edi
        jmp .blancos
.b:
        ret
.blancos2:
        cmp byte [esi], ' '
        jne .c2
        inc esi
        jmp .blancos2
.c2:
        ret
; ESI = texto, ECX = palabra en mayúsculas (acaba en 0): ZF si empieza por
; ella (sin mirar las mayúsculas); ESI, detrás
.igual:
        mov al, [ecx]
        test al, al
        jz .ig
        mov ah, [esi]
        cmp ah, 'a'
        jb .i1
        cmp ah, 'z'
        ja .i1
        sub ah, 0x20
.i1:
        cmp ah, al
        jne .ix
        inc esi
        inc ecx
        jmp .igual
.ig:
        cmp al, al
.ix:
        ret

; -> EBX = donde acaban los comentarios y líneas vacías del principio
mz_tras_coment:
        xor ebx, ebx
.l:
        cmp ebx, [mzb_n]
        jae .x
        mov al, [mzb_ent+ebx]
        cmp al, ';'
        je .sig
        cmp al, 13
        je .sig
        cmp al, 10
        je .sig
        ret
.sig:
        cmp ebx, [mzb_n]
        jae .x
        cmp byte [mzb_ent+ebx], 10
        je .s2
        inc ebx
        jmp .sig
.s2:
        inc ebx
        jmp .l
.x:
        ret

t_mz_sec    db "MEZCLADOR]", 0
t_mz_vis    db "VISTA=", 0
t_mz_crlf   db 13, 10, 0

        section .bss
mzb_n       resd 1
mzb_ent     resb MZ_FMAX + 4
mzb_sal     resb MZ_FMAX + 2048
        section .text
