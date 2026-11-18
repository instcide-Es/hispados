; =============================================================================
;  Hispa-486 - núcleo ACPI: cambio de pila entre la llamada y la tarea
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  void tarea_cambia(u32 *guarda_esp, u32 nuevo_esp)
;  Guarda los registros que una función de C debe conservar y la pila de quien
;  llama en *guarda_esp, pasa a la pila nuevo_esp y sigue donde aquella se
;  quedó. Así ACPICA (que no sabe pararse a medias de un método) corre en su
;  propia pila y, cuando un método pide dormir (Sleep), el núcleo vuelve al
;  residente y el equipo sigue; la tarea continúa en el siguiente LL_PASO.
; =============================================================================
        bits 32
        global tarea_cambia
        section .text
tarea_cambia:
        push ebp
        push ebx
        push esi
        push edi
        mov eax, [esp + 20]
        mov edx, [esp + 24]
        mov [eax], esp
        mov esp, edx
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
