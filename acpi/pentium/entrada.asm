; =============================================================================
;  Hispa-Pentium - ACPI.BIN, el servicio de ACPI: la cabecera de programa del
;  anillo 3 (HPU3, docs/PENTIUM.md sección 6) y la entrada.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El mismo núcleo de C que HSPACPI.BIN de Hispa-DOS (acpi/nucleo/ y ACPICA),
;  enlazado para 0x40000000 (donde el núcleo de Hispa-Pentium pone los
;  programas) y con su propia «máquina» (acpi/pentium/maquina.c): los puertos,
;  la memoria física y la configuración PCI se piden al núcleo con las
;  llamadas 150 a 152, que solo valen para el servicio que lanza él al
;  arrancar. El núcleo pone la pila (arriba del todo de la zona del programa)
;  y deja a cero lo que va detrás del archivo (la cabecera lo dice).
; =============================================================================
        bits 32
        extern servicio_principal, _fin_imagen, _fin_bss
        global _inicio

        section .entrada progbits alloc exec
_inicio:
        jmp short salto
        align 4, db 0
        dd 0x33555048               ; «HPU3»
        dd _inicio                  ; dónde va (0x40000000)
        dd _fin_imagen              ; hasta dónde llega lo del archivo
        dd _fin_bss                 ; hasta dónde lo que empieza a cero
        dd salto                    ; por dónde se empieza
salto:
        cld
        and esp, 0xFFFFFFF0
        call servicio_principal
        mov ebx, eax                ; acabar (llamada 2) con lo que devuelve
        mov eax, 2
        int 0x80
.nunca:
        jmp .nunca
