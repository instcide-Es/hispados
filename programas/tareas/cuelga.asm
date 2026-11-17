; =============================================================================
;  CUELGA.HVP - un programa que se cuelga, para probar el Administrador de
;  tareas (tests/run_tareas_tests.sh). NO va en la imagen del sistema.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Teclas:
;    1  se mete en un bucle sin fin dentro del mensaje de la tecla
;    2  pone un temporizador y se mete en un bucle sin fin dentro de WM_TIMER
;    3  pide 256 K con mem_bloque, abre un archivo (CUELGA.TMP), pone un
;       temporizador, captura el ratón y se mete en un bucle sin fin
;    4  desde ahora no se deja cerrar (no hace caso de WM_CLOSE)
;    N  abre otra copia de sí mismo
;    B  abre el Bloc de notas
; =============================================================================
%include "hvp.inc"

HVP_CABECERA "Cuelga (prueba)", entrada, 1
%rep 32
        db "................................"
%endrep
HVP_CODIGO

c_cuelga    dd cu_proc
            db C_CREMA, CB_FOCO, 0, 0
cu_plant:
        PLANTILLA c_cuelga, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 30, 330, 330, 200, 0

t_titulo    db "Cuelga", 0
t_l1        db "1 bucle en un mensaje", 0
t_l2        db "2 bucle en el temporizador", 0
t_l3        db "3 memoria, archivo, ratón y bucle", 0
t_l4        db "4 no se deja cerrar", 0
t_l5        db "N otra copia   B Bloc de notas", 0
t_niega     db "(no se deja cerrar)", 0
t_yo        db "A:\HISPADOS\PROGS\CUELGA.HVP", 0
t_bloc      db ":BLOC", 0
t_tmp       db "A:\CUELGA.TMP", 0
t_nada      db 0

entrada:
        cld
        mov esi, cu_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

cu_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_CHAR
        je tecla
        cmp eax, WM_TIMER
        je al_tic
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        jmp hecho
.cerrar:
        cmp byte [niega], 0
        jne hecho                   ; (se niega: no hace nada)
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
hecho:
        xor eax, eax
        ret

tecla:
        or ecx, 0x20                ; (minúsculas)
        cmp ecx, '1'
        je .bucle
        cmp ecx, '2'
        je .tempo
        cmp ecx, '3'
        je .todo
        cmp ecx, '4'
        je .niega
        cmp ecx, 'n'
        je .otra
        cmp ecx, 'b'
        je .bloc
        jmp hecho
.bucle:
        jmp .bucle                  ; colgado dentro del mensaje
.tempo:
        mov byte [en_tic], 1
        mov ebp, [ven]
        mov ecx, 1
        mov edx, 100
        HV tmp_poner
        jmp hecho
.todo:
        mov ecx, 256 * 1024         ; memoria...
        HV mem_bloque
        mov ecx, 0                  ; ...un archivo abierto...
        mov esi, t_tmp
        HV dos_crear
        mov ebp, [ven]              ; ...un temporizador inofensivo...
        mov ecx, 2
        mov edx, 1000
        HV tmp_poner
        mov ebp, [ven]              ; ...y el ratón, suyo
        HV ven_capturar
.b3:
        jmp .b3
.niega:
        mov byte [niega], 1
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.otra:
        mov esi, t_yo
        jmp short .abre
.bloc:
        mov esi, t_bloc
.abre:
        mov ebp, [ven]
        mov edi, t_nada
        xor edx, edx
        xor ecx, ecx
        xor ebx, ebx
        HV prog_abrir
        jmp hecho

al_tic:
        cmp byte [en_tic], 0
        je hecho
        cmp ecx, 1
        jne hecho
.b2:
        jmp .b2                     ; colgado dentro del temporizador

pintar:
        mov ebp, [ven]
        HV dc_pintar
        COLOR C_TINTA
        mov eax, 10
        mov ebx, 8
        mov esi, t_l1
        HV dc_texto
        mov ebx, 26
        mov esi, t_l2
        HV dc_texto
        mov ebx, 44
        mov esi, t_l3
        HV dc_texto
        mov ebx, 62
        mov esi, t_l4
        HV dc_texto
        mov ebx, 80
        mov esi, t_l5
        HV dc_texto
        cmp byte [niega], 0
        je .x
        COLOR C_GRANATE
        mov ebx, 110
        mov esi, t_niega
        HV dc_texto
.x:
        HV dc_cerrar
        jmp hecho

%include "hvplib.inc"

        section .bss
ven         resd 1
niega       resb 1
en_tic      resb 1
HVP_FIN
