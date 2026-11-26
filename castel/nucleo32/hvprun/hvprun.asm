; =============================================================================
;  HVPRUN.BIN - los programas de Hispa Ventanas (.HVP) dentro de Hispa-Pentium
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un programa de usuario de Hispa-Pentium (anillo 3, cabecera HPU3: docs/
;  PENTIUM.md) que hace de Hispa Ventanas para UN programa .HVP: la oficina
;  de Hispa-386 (Facturas, Pluma, Cuentas, Mirador...) corre aquí sin tocarle
;  una línea. Uso: HVPRUN \OFICINA\FACTURAS.HVP [archivo]
;
;  Cómo está hecho: el gestor de ventanas, los mensajes, los controles, los
;  menús, los diálogos y el dibujo son los de Hispa Ventanas (ventanas/*.inc,
;  reensamblados tal cual: el código es de 32 bits plano y no toca el
;  hardware). Lo que hay debajo es de aquí:
;    - la pantalla de Ventanas es una superficie de 8 bits del tamaño de lo de
;      dentro de UNA ventana de Hispa-Pentium; lo que cambia se pasa a 32 bits
;      con la paleta y se le da al núcleo (llamada 34, un mapa de puntos);
;    - la ventana principal del programa ocupa esa pantalla entera, con su
;      marco y su barra de título fuera (el título lo pone Hispa-Pentium; la
;      barra de menús sí se ve); los cuadros de diálogo y los menús salen
;      dentro;
;    - el ratón y las teclas llegan como sucesos de la ventana (llamada 35);
;    - los archivos (dos_*), la fecha y la hora, la memoria (mem_bloque) y el
;      reloj van por las llamadas al sistema (pent.inc, pdos.inc).
;  Hay dos tareas (las cooperativas de Ventanas, nucleo.inc): la 0 es este
;  bucle (sucesos, repartir, presentar) y la otra es el programa .HVP.
; =============================================================================
        cpu 386
        bits 32
        org 0x40000000
%include "defs.inc"
%include "api.inc"                  ; HV_LISTA: la interfaz de los .HVP
RAT_COLA    equ 64                  ; (las colas de bss.inc: aquí no se usan)
CRU_COLA    equ 16
CUR_ANCHO   equ 16
CUR_ALTO    equ 18
; (constantes de lo que no se trae: solo dan tamaño a variables de bss*.inc)
AC_TAM      equ 256
AC_USO      equ 0
AC_BAND     equ 1
AC_BAND2    equ 2
AC_GRUPO    equ 3
AC_NOMBRE   equ 4
AC_PROG     equ 36
AC_PARAM    equ 116
AC_DIR      equ 188
HS_TAM      equ 168
HS_USO      equ 0
HS_TAREA    equ 12
HS_RUTA     equ 16
HVP_MAX     equ 1
HVI_N       equ 1
MAX_AC      equ 1
MAX_GR      equ 1
GR_BAND     equ 0
CAM_MAX     equ 1
BP_MAXCAND  equ 1
BP_MAXDIR   equ 1
ICO_ALTO    equ 32
ICO_PASO    equ 32
JD_MAXSUEL  equ 1
PE_MAX      equ 1
VL_MAX      equ 1
VL_TAM      equ 16
JD_DIRTAM   equ 4
PE_TAM      equ 4
BT_MAX      equ 1
INI_MAX     equ 1
INI_SMAX    equ 1
BE_MAX      equ 1
MI_FR       equ 1
MI_FR_MAXAL equ 1
BT_BUF      equ 4
AY_TAM      equ 4
AB2_AUTO    equ 0x40
AB2_MANO    equ 0x80

; ---- la cabecera de un programa de Hispa-Pentium ----
hpu_ini:
        jmp short hpu_salto         ; (la cabecera va en el octeto 4)
        align 4, db 0
        dd 0x33555048               ; «HPU3»
        dd hpu_ini                  ; dónde va
        dd hpu_datos_fin            ; hasta dónde llega lo del archivo
        dd hpu_vacio_fin            ; hasta dónde lo que empieza a cero
        dd arranque                 ; por dónde se empieza
hpu_salto:
        jmp arranque

; ---- lo de Hispa Ventanas, tal cual ----
%include "gdi.inc"
%include "dc.inc"
%include "nucleo.inc"
%include "gestor.inc"
%include "menus.inc"
%include "controles.inc"
%include "dialogos.inc"
%include "contexto.inc"
%include "tamano.inc"
%include "datos.inc"
%include "fuente.inc"

; ---- lo de aquí ----
%include "pent.inc"                 ; las llamadas al sistema de Hispa-Pentium
%include "ppantalla.inc"            ; la ventana, la paleta, presentar y los sucesos
%include "pdos.inc"                 ; los archivos y las interrupciones de DOS
%include "pfaltan.inc"              ; lo que el gestor espera del escritorio
%include "psonido.inc"              ; (SONIDO) el sonido, sobre el de Hispa-Pentium
%include "poficina.inc"             ; (OFICINA2) la carpeta de los documentos, la rueda y Alt+letra
%include "pguardar.inc"             ; [0.9.31] guardar sin vaciar el de antes (ventanas/guardar.inc)
%include "guardar.inc"

; ---- la tabla de la interfaz, en el orden de api.inc ----
; (las de nombre HVA_x, si las hay, sustituyen a las de Ventanas)
%macro HVF 1
%ifdef HVA_%1
        dd HVA_%1
%else
        dd %1
%endif
%endmacro
%define HVA_ven_texto p_ven_texto
%define HVA_tics p_tics
        align 4
api_tabla:
        HV_LISTA
API_N       equ ($ - api_tabla) / 4
%unmacro HVF 1

; =============================================================================
;  El arranque y el bucle del sistema (la tarea 0)
; =============================================================================
arranque:
        cld
        mov edi, hpu_vacio          ; las variables, a cero
        mov ecx, hpu_vacio_fin - hpu_vacio
        xor eax, eax
        rep stosb
        mov dword [cero], bda_falsa ; (nucleo.inc lee los tics en 0x46C)
        mov eax, 0x0A0B0C0D
        mov [azar_x], eax
        call pent_iniciar           ; el reloj y la pantalla de Hispa-Pentium
        jc .sin_escritorio
        call p_args                 ; el .HVP y su archivo, de los argumentos
        jc .uso
        call p_modulo_del_sistema   ; (OFICINA2) el .HVP, en la unidad del sistema si no está en esta
        call p_carpeta_docs         ; (OFICINA2) dónde se trabaja: poficina.inc
        call p_mem_iniciar          ; el montón de Ventanas
        jc .sin_memoria
        call pan_iniciar            ; la superficie y la paleta
        jc .sin_memoria
        call gdi_iniciar
        call tar_iniciar
        call ges_iniciar
        call p_fondo_crear          ; la ventana del fondo (como el escritorio)
        call hvp_cargar             ; el programa, a su memoria y en marcha
        jc .fin_mal
.bucle:
        call pan_esperar            ; si no ha pasado nada, a esperar un suceso
        mov byte [actividad], 0
        call bda_tics               ; el reloj de la BIOS de mentira, al día
        call pan_sucesos            ; el ratón, las teclas, el tamaño, cerrar
        mov ecx, 4
.ronda:
        call sis_mensajes           ; los menús y el fondo
        call tar_ceder              ; un turno para el programa
        call sis_mensajes
        call ges_pendiente
        jnc .pres
        dec ecx
        jnz .ronda
.pres:
        call tar_limpiar
        call pan_principal          ; la ventana principal, en su sitio
        call ges_presentar          ; lo que ha cambiado, a la ventana
        mov eax, [hvp_tarea_p]
        cmp dword [eax+T_USO], 1    ; ¿ha acabado el programa?
        je .bucle
        call pp_exportar            ; (lo que haya copiado, para los demás)
        xor ebx, ebx
        jmp pent_salir
.sin_escritorio:
        mov esi, t_sin_escritorio
        jmp .mal
.uso:
        mov esi, t_uso
        jmp .mal
.sin_memoria:
        mov esi, t_sin_memoria
.mal:
        call pent_escribir
.fin_mal:
        mov ebx, 1
        jmp pent_salir

; (sistema) despacha la cola de la tarea 0
sis_mensajes:
        push edi
        sub esp, M_TAM
        mov edi, esp
.l:
        call msg_mirar
        jc .x
        call msg_despachar
        jmp .l
.x:
        add esp, M_TAM
        pop edi
        ret

; (sistema) CF=1 si queda alguna ventana por pintar
ges_pendiente:
        push eax
        push ebx
        push ecx
        mov ebx, [ven_tabla]
        mov ecx, MAX_VEN
.b:
        cmp dword [ebx+V_USO], 1
        jne .s
        test dword [ebx+V_ESTADO], S_NCSUCIO
        jnz .si
        mov eax, [ebx+V_UX0]
        cmp eax, [ebx+V_UX1]
        jl .si
.s:
        add ebx, V_TAM
        dec ecx
        jnz .b
        clc
        jmp .x
.si:
        stc
.x:
        pop ecx
        pop ebx
        pop eax
        ret

; el bucle de mensajes de un programa (centro.inc: app_bucle)
app_bucle:
        sub esp, M_TAM
        mov edi, esp
.l:
        call msg_tomar
        test eax, eax
        jz .x
        call msg_despachar
        jmp .l
.x:
        add esp, M_TAM
        ret

; =============================================================================
;  El programa .HVP: cargarlo, reubicarlo y ponerlo en marcha
; =============================================================================
H_FIRMA     equ 0
H_API       equ 6
H_ARCH      equ 8
H_MEM       equ 12
H_ENTRADA   equ 16
H_IMG       equ 20
H_NREUB     equ 24
H_TABLA     equ 576

hvp_cargar:
        pushad
        mov esi, hvp_ruta
        xor al, al
        call dos_abrir
        jc .no_esta
        mov ebx, eax
        mov edi, hvp_cab
        mov ecx, 64
        call dos_leer
        jc .malc
        cmp eax, 64
        jb .malc
        cmp dword [hvp_cab+H_FIRMA], 'HVP1'
        jne .malc
        mov esi, t_hvp_viejo
        cmp word [hvp_cab+H_API], API_N
        ja .malc2
        mov ecx, [hvp_cab+H_MEM]
        cmp ecx, [hvp_cab+H_ARCH]
        jb .malc
        cmp ecx, 64 * 1024 * 1024
        ja .malc
        add ecx, 4095
        and ecx, ~4095
        call pent_mas_memoria
        jc .malm
        mov [hvp_base], eax
        mov edi, eax
        xor edx, edx                ; el archivo entero, a su memoria
        xor al, al
        call dos_puntero
        mov ecx, [hvp_cab+H_ARCH]
        call dos_leer
        jc .malc
        cmp eax, [edi+H_ARCH]
        jb .malc
        call dos_cerrar
        mov edx, [edi+H_IMG]
        lea esi, [edi+edx]          ; reubicar: a cada puntero se le suma la base
        mov ecx, [edi+H_NREUB]
.r:
        test ecx, ecx
        jz .rf
        lodsd
        cmp eax, edx
        jae .r1
        add [edi+eax], edi
.r1:
        dec ecx
        jmp .r
.rf:
        push edi
        mov ecx, [edi+H_MEM]        ; lo que no viene en el archivo, a cero
        sub ecx, edx
        add edi, edx
        xor al, al
        rep stosb
        pop edi
        movzx ecx, word [edi+H_API]
        add edi, H_TABLA
        mov esi, api_tabla
        rep movsd
        mov eax, hvp_tarea
        call tar_crear
        mov [hvp_tarea_p], eax
        test eax, eax
        jz .malm2
        popad
        clc
        ret
.no_esta:
        mov esi, t_hvp_no_esta
        jmp .aviso
.malm:
        mov esi, t_sin_memoria
        jmp .malc2
.malc:
        mov esi, t_hvp_mal
.malc2:
        call dos_cerrar
        jmp .aviso
.malm2:
        mov esi, t_sin_memoria
.aviso:
        call pent_escribir
        popad
        stc
        ret

; la entrada de la tarea del programa: EBX=su base ESI=su ruta EDI=sus
; parámetros ECX=entradas de la interfaz
hvp_tarea:
        mov ebx, [hvp_base]
        mov esi, hvp_ruta
        mov edi, hvp_param
        mov ecx, API_N
        mov eax, [ebx+H_ENTRADA]
        add eax, ebx
        call eax
        ret

t_uso           db "HVPRUN: uso: HVPRUN PROGRAMA.HVP [ARCHIVO]", 10, 0
t_sin_escritorio db "HVPRUN: los programas de Hispa Ventanas necesitan el escritorio.", 10, 0
t_sin_memoria   db "HVPRUN: no hay memoria.", 10, 0
t_hvp_no_esta   db "HVPRUN: no se encuentra el programa.", 10, 0
t_hvp_mal       db "HVPRUN: no se ha podido cargar el programa.", 10, 0
t_hvp_viejo     db "HVPRUN: este programa necesita un Hispa Ventanas más nuevo.", 10, 0

hpu_datos_fin:

; =============================================================================
;  Las variables (a cero)
; =============================================================================
        section .bss
        alignb 4096
hpu_vacio:
%include "bss.inc"
%include "bss2.inc"
%include "bss3.inc"
%include "bsstam.inc"
%include "bssctx.inc"
%include "bssbarra.inc"
        alignb 4
base        resd 1                  ; (Ventanas: la base del programa en DPMI; aquí 0)
rmcs        resb 0x34               ; (tamano.inc lo nombra en lo que no se usa)
hay_rat     resb 1                  ; 0: el cursor lo pinta Hispa-Pentium
op_contorno resb 1
        alignb 4
hvp_base    resd 1
hvp_tarea_p resd 1
hvp_cab     resb 64
hvp_ruta    resb 128                ; «C:\OFICINA\FACTURAS.HVP»
hvp_param   resb 128                ; lo que va detrás: el archivo que abrir
bda_falsa   resb 0x500              ; la zona de datos de la BIOS de mentira (los tics)
tec_mapa    resb 128                ; (datos de la interfaz: ninguna tecla mantenida)
%include "pbss.inc"
        alignb 4
hpu_vacio_fin:
