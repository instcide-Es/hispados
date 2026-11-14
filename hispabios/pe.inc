; =============================================================================
;  HispaBIOS - cabecera PE/COFF escrita a mano para una aplicación UEFI
; =============================================================================
;  Uso (antes del código):
;       %define PE_ARQ 64        ; 64 = x64 (PE32+), 32 = IA32 (PE32)
;       %include "pe.inc"
;       ... código y datos (todo independiente de la posición) ...
;       PE_FIN
;
;  La imagen es plana: las direcciones relativas (RVA) coinciden con las
;  posiciones en el fichero (alineación de sección = de fichero = 200h), así
;  que el cargador de UEFI no tiene que mover nada. No hay reubicaciones: en
;  x64 todo se direcciona respecto a RIP y en IA32 respecto a una base que el
;  propio código calcula al entrar.
;
;  Referencias: especificación PE/COFF de Microsoft (formato público) y
;  especificación UEFI 2.x (subsistema 10 = aplicación EFI).
; =============================================================================

%ifndef PE_ARQ
  %define PE_ARQ 64
%endif

        bits PE_ARQ
        org 0

pe_inicio:
; ---- cabecera MS-DOS (solo «MZ» y el puntero a la cabecera PE) ----
        db 'MZ'
        times 3Ch - ($ - pe_inicio) db 0
        dd pe_cab - pe_inicio

pe_cab:
        db 'PE', 0, 0
; ---- cabecera COFF ----
%if PE_ARQ = 64
        dw 8664h                        ; máquina: x64
%else
        dw 014Ch                        ; máquina: i386
%endif
        dw 1                            ; una sección
        dd 0                            ; fecha
        dd 0, 0                         ; sin tabla de símbolos
        dw pe_opc_fin - pe_opc          ; tamaño de la cabecera opcional
%if PE_ARQ = 64
        dw 0022h                        ; ejecutable, direcciones grandes
%else
        dw 0102h                        ; ejecutable, máquina de 32 bits
%endif

; ---- cabecera opcional ----
pe_opc:
%if PE_ARQ = 64
        dw 020Bh                        ; PE32+
%else
        dw 010Bh                        ; PE32
%endif
        db 0, 1                         ; versión del enlazador
        dd pe_codigo_fin - pe_codigo    ; tamaño del código
        dd 0                            ; datos con valor inicial
        dd 0                            ; datos sin valor
        dd pe_entrada - pe_inicio       ; punto de entrada (RVA)
        dd pe_codigo - pe_inicio        ; base del código
%if PE_ARQ = 64
        dq 0                            ; base de la imagen
%else
        dd 0                            ; base de los datos
        dd 0                            ; base de la imagen
%endif
        dd 200h                         ; alineación de las secciones
        dd 200h                         ; alineación en el fichero
        dw 0, 0                         ; versión del sistema
        dw 0, 1                         ; versión de la imagen
        dw 0, 0                         ; versión del subsistema
        dd 0                            ; (reservado)
        dd pe_fin - pe_inicio           ; tamaño de la imagen
        dd pe_codigo - pe_inicio        ; tamaño de las cabeceras
        dd 0                            ; suma de comprobación
        dw 10                           ; subsistema: aplicación EFI
        dw 0                            ; características de la DLL
%if PE_ARQ = 64
        dq 10000h, 1000h, 10000h, 1000h ; pila y montón (no se usan)
%else
        dd 10000h, 1000h, 10000h, 1000h
%endif
        dd 0                            ; (reservado)
        dd 16                           ; número de directorios
        times 16 dd 0, 0                ; directorios: todos vacíos
pe_opc_fin:

; ---- tabla de secciones ----
        db '.text', 0, 0, 0
        dd pe_fin - pe_codigo           ; tamaño en memoria
        dd pe_codigo - pe_inicio        ; RVA
        dd pe_fin - pe_codigo           ; tamaño en el fichero
        dd pe_codigo - pe_inicio        ; posición en el fichero
        dd 0, 0                         ; sin reubicaciones ni líneas
        dw 0, 0
        dd 0E0000020h                   ; código, se lee, se escribe, se ejecuta

        times 200h - ($ - pe_inicio) db 0

pe_codigo:

%macro PE_FIN 0
pe_codigo_fin:
        align 200h, db 0
pe_fin:
%endmacro
