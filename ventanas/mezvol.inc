; =============================================================================
;  Hispa Ventanas - [MEZCLADOR] el mezclador de HSPSND desde Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Va en la parte alta de VENTANAS.COM (solo la usa el código de 32 bits).
;    snd_mezcla   (interfaz 82) EAX EBX ECX EDX -> HSPSND (AH = función:
;                 09h-0Fh, el mezclador por fuentes de la versión 4) -> los
;                 cuatro como los deja; CF=1 si no hay sonido o si HSPSND
;                 dice que no. Si lo que se ha tocado puede ser «General»,
;                 se vuelve a leer el volumen (06h) y se repinta el altavoz
;    mez_abrir    (interfaz 83) abre el Mezclador, \HISPADOS\PROGS\MEZCLAD.HVP
;                 (si ya está abierto, lo trae delante); EAX=1: en la vista
;                 Completa. Lo usan el botón «Mezclador...» de la ventanita
;                 del volumen y el doble clic en el altavoz de la barra
; =============================================================================

snd_mezcla:
        cmp byte [snd_hay], 0
        je .no
        cmp word [snd_ver], 4
        jb .no
        push eax
        mov [rmcs+0x1C], eax
        mov [rmcs+0x10], ebx
        mov [rmcs+0x18], ecx
        mov [rmcs+0x14], edx
        mov word [rmcs+0x2A], rm_snd
        call rm_llamar
        pop eax
        cmp ah, 0x0B                ; poner una fuente (puede ser General) o
        je .sinc                    ; los valores de fábrica
        cmp ah, 0x0E
        jne .lee
.sinc:
        push dword [rmcs+0x1C]
        push dword [rmcs+0x10]
        push dword [rmcs+0x18]
        push dword [rmcs+0x14]
        push dword [rmcs+0x20]
        mov eax, 0x0600             ; el volumen de General, para la barra
        call snd_llamar
        movzx edx, al
        mov [vol_pct], edx
        movzx edx, ah
        and edx, 1
        mov [vol_mudo], edx
        call vol_inv_icono
        pop dword [rmcs+0x20]
        pop dword [rmcs+0x14]
        pop dword [rmcs+0x18]
        pop dword [rmcs+0x10]
        pop dword [rmcs+0x1C]
.lee:
        mov eax, [rmcs+0x1C]
        mov ebx, [rmcs+0x10]
        mov ecx, [rmcs+0x18]
        mov edx, [rmcs+0x14]
        test byte [rmcs+0x20], 1
        jnz .no
        clc
        ret
.no:
        stc
        ret

mez_abrir:
        pushad
        mov edx, eax
        mov edi, mez_acc + AC_PROG
        mov esi, dir_propia
        call pon_cad
        mov esi, tn_mezclad
        call pon_cad
        mov byte [edi], 0
        mov dword [mez_acc+AC_PARAM], 0
        test edx, edx
        jz .p
        mov word [mez_acc+AC_PARAM], '/C'
.p:
        mov ebx, mez_acc
        mov ebp, [cen_ven]          ; (si no se puede cargar, el aviso sale aquí)
        call hvp_abrir
        popad
        ret

tn_mezclad  db "PROGS\MEZCLAD.HVP", 0
