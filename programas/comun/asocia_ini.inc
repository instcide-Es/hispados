; =============================================================================
;  Lo común de \HISPADOS\ASOCIA.INI (qué programa abre cada tipo de archivo)
;  para los módulos que lo usan: Archivos (abrir, «Abrir con…») y
;  Configuración («Tipos de archivo»).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Quien lo incluye pone: asoc_pon_ruta (deja en asoc_ruta la ruta entera de
;  ASOCIA.INI), la macro ASOC_OLVIDA (olvidar un error crítico de disco, o
;  nada) y, en sus variables, ASOC_BSS. ASOCIA.INI: una línea por extensión,
;  EXT=programa (ruta desde \HISPADOS, ruta entera o «:BLOC»); EXT=- o sin
;  línea: ninguno. Si no existe, se escribe con los de serie.
; =============================================================================
ASOC_TAM    equ 2048
ASOC_RUTA   equ 80                  ; (lo que cabe una ruta, como RUTA_TAM)
%macro ASOC_BSS 0
asoc_ruta   resb ASOC_RUTA
asoc_ext    resb 8
asoc_val    resb ASOC_RUTA
asoc_buf    resb ASOC_TAM
asoc_nuevo  resb ASOC_TAM + ASOC_RUTA
%endmacro

; ---- la ruta de ASOCIA.INI y el texto (de disco o de serie), en asoc_buf
asoc_carga:
        pushad
        call asoc_pon_ruta          ; (la pone cada módulo: asoc_ruta)
        mov esi, asoc_ruta
        mov edi, asoc_buf
        mov ecx, ASOC_TAM - 1
        call hvl_leer
        ASOC_OLVIDA
        test eax, eax
        jnz .x
        ; no está: los de serie (y se deja escrito, si se puede)
        mov esi, t_asoc_serie
        mov edi, asoc_buf
        call hvl_cad
        mov byte [edi], 0
        mov ecx, edi
        sub ecx, asoc_buf
        mov esi, asoc_ruta
        mov edx, asoc_buf
        call hvl_escribir
        ASOC_OLVIDA                 ; (un disquete protegido: se usan igual)
.x:
        popad
        ret

; ---- la extensión de la ruta ESI (lo de detrás del último punto, si está
; detrás de la última barra) a asoc_ext, en mayúsculas; CF=1 si no tiene
asoc_extension:
        pushad
        xor ebx, ebx                ; el último punto
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '\'
        jne .p
        xor ebx, ebx
        jmp .c
.p:
        cmp al, '.'
        jne .c
        mov ebx, esi
        jmp .c
.f:
        test ebx, ebx
        jz .no
        mov esi, ebx
        mov edi, asoc_ext
        mov ecx, 4
.e:
        lodsb
        test al, al
        jz .e_f
        call asoc_mayus
        stosb
        loop .e
.e_f:
        mov byte [edi], 0
        cmp byte [asoc_ext], 0
        je .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret
asoc_mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 32
.x:
        ret

; ---- el programa de la extensión asoc_ext: a asoc_val. CF=1 si no tiene
; (o es «-»). La clave tiene que ir al principio de una línea ----
asoc_busca:
        pushad
        mov esi, asoc_buf
.linea:
        ; ¿«EXT=» aquí?
        xor ecx, ecx
.k:
        mov al, [esi+ecx]
        call asoc_mayus
        mov ah, [asoc_ext+ecx]
        test ah, ah
        jz .fin_ext
        cmp al, ah
        jne .sig
        inc ecx
        jmp .k
.fin_ext:
        cmp al, '='
        jne .sig
        lea esi, [esi+ecx+1]
        ; el valor, sin espacios al principio ni al final
.esp:
        cmp byte [esi], ' '
        jne .v
        inc esi
        jmp .esp
.v:
        mov edi, asoc_val
        mov ecx, ASOC_RUTA - 1
.v1:
        lodsb
        cmp al, ' '
        jb .v_f
        stosb
        loop .v1
.v_f:
        cmp edi, asoc_val
        je .no
        cmp byte [edi-1], ' '
        jne .v_ok
        dec edi
        jmp .v_f
.v_ok:
        mov byte [edi], 0
        cmp word [asoc_val], '-'    ; «-» y nada detrás: sin programa
        je .no
        popad
        clc
        ret
.sig:
        ; a la línea siguiente
        lodsb
        test al, al
        jz .no
        cmp al, 10
        jne .sig
        jmp .linea
.no:
        popad
        stc
        ret

; ---- pone «asoc_ext=asoc_val» en ASOCIA.INI (cambia la línea o la añade) ---
asoc_guarda:
        pushad
        call asoc_carga
        mov esi, asoc_buf
        mov edi, asoc_nuevo
        xor edx, edx                ; DL: ya puesta
.linea:
        cmp byte [esi], 0
        je .fin
        ; ¿es la de esta extensión?
        xor ecx, ecx
.k:
        mov al, [esi+ecx]
        call asoc_mayus
        mov ah, [asoc_ext+ecx]
        test ah, ah
        jz .k_f
        cmp al, ah
        jne .copia
        inc ecx
        jmp .k
.k_f:
        cmp al, '='
        jne .copia
        test dl, dl
        jnz .salta                  ; (repetida: fuera)
        call .pon
        mov dl, 1
.salta:
        lodsb
        test al, al
        jz .fin0
        cmp al, 10
        jne .salta
        jmp .linea
.copia:
        lodsb
        test al, al
        jz .fin0
        stosb
        cmp al, 10
        jne .copia
        jmp .linea
.fin0:
        cmp byte [edi-1], 10        ; la última línea, con su salto
        je .fin
        mov ax, 0x0A0D
        stosw
.fin:
        test dl, dl
        jnz .esc
        call .pon
.esc:
        mov ecx, edi
        sub ecx, asoc_nuevo
        mov esi, asoc_ruta
        mov edx, asoc_nuevo
        call hvl_escribir
        ASOC_OLVIDA
        popad
        ret
.pon:
        cmp byte [asoc_val], 0      ; sin programa: la línea no se pone (quitar)
        je .pon_x
        push esi
        mov esi, asoc_ext
        call hvl_cad
        mov al, '='
        stosb
        mov esi, asoc_val
        call hvl_cad
        mov ax, 0x0A0D
        stosw
        pop esi
.pon_x:
        ret

n_asocia    db "ASOCIA.INI", 0

; los programas que se ofrecen: el nombre (ASOC_NOM_TAM) y la ruta
ASOC_NOM_TAM  equ 24
ASOC_PROG_TAM equ 48
%macro APROG 2
%%a:    db %1, 0
        times ASOC_NOM_TAM - ($ - %%a) db 0
%%b:    db %2, 0
        times ASOC_PROG_TAM - ASOC_NOM_TAM - ($ - %%b) db 0
%endmacro
asoc_progs:
        APROG "Bloc de notas", ":BLOC"
        APROG "Pluma", "PROGS\PLUMA.HVP"
        APROG "Cuentas", "PROGS\CUENTAS.HVP"
        APROG "Datos", "PROGS\DATOS.HVP"
        APROG "Lector de PDF", "PROGS\LECTOR.HVP"
        APROG "Publicaciones", "PROGS\PUBLICA.HVP"
        APROG "Presentaciones", "PROGS\PRESENTA.HVP"
        APROG "Mirador", "PROGS\MIRADOR.HVP"
        APROG "Pintura", "PROGS\PINTURA.HVP"
        APROG "Sonidos", "PROGS\SONIDOS.HVP"
ASOC_N_PROGS equ ($ - asoc_progs) / ASOC_PROG_TAM

; los de serie (lo que se escribe si no hay ASOCIA.INI)
t_asoc_serie:
        db "; Hispa Ventanas - qué programa abre cada tipo de archivo.", 13, 10
        db "; EXT=programa: con su ruta desde \HISPADOS (PROGS\PLUMA.HVP), con", 13, 10
        db "; la ruta entera, o :BLOC (el Bloc de notas). EXT=- : ninguno.", 13, 10
        db "[Asociaciones]", 13, 10
        db "TXT=:BLOC", 13, 10
        db "PLU=PROGS\PLUMA.HVP", 13, 10
        db "HOJ=PROGS\CUENTAS.HVP", 13, 10
        db "CSV=PROGS\CUENTAS.HVP", 13, 10
        db "BDD=PROGS\DATOS.HVP", 13, 10
        db "PDF=PROGS\LECTOR.HVP", 13, 10
        db "PUB=PROGS\PUBLICA.HVP", 13, 10
        db "PRE=PROGS\PRESENTA.HVP", 13, 10
        db "BMP=PROGS\MIRADOR.HVP", 13, 10
        db "PCX=PROGS\MIRADOR.HVP", 13, 10
        db "GIF=PROGS\MIRADOR.HVP", 13, 10
        db "PNG=PROGS\MIRADOR.HVP", 13, 10
        db "TGA=PROGS\MIRADOR.HVP", 13, 10
        db "ICO=PROGS\MIRADOR.HVP", 13, 10
        db "JPG=PROGS\MIRADOR.HVP", 13, 10
        db "JPEG=PROGS\MIRADOR.HVP", 13, 10
        db "PBM=PROGS\MIRADOR.HVP", 13, 10
        db "PGM=PROGS\MIRADOR.HVP", 13, 10
        db "PPM=PROGS\MIRADOR.HVP", 13, 10
        db "WAV=PROGS\SONIDOS.HVP", 13, 10
        db 0
