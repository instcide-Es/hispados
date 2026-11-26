; =============================================================================
;  HVPRUN - lo de ventanas/guardar.inc que en Hispa Ventanas pasa por el DOS
;  de modo real (rmcs, dos_buf): aquí, con las llamadas de Hispa-Pentium.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Desde la 0.9.31 los programas de la oficina guardan por la interfaz
;  (api.inc 98 a 102: gua_crear, gua_cerrar, gua_escribir, gua_ext y
;  gua_atrib): lo nuevo en un .$$$ y, entero, el de antes a .BAK y el nuevo
;  a su nombre. El resto de guardar.inc vale tal cual (dos_crear, dos_cerrar,
;  dos_escribir, dos_borrar y dos_abrir los pone pdos.inc).
; =============================================================================
%define GUA_PROPIOS

; (interna) ESI=nombre de antes EDI=el nuevo -> CF=1 si no se puede
gua_renombra:
        pushad
        mov ebx, esi
        mov ecx, edi
        PSIS PS_RENOMBRAR
        call p_err
        popad                       ; (popad no toca las banderas)
        ret

; ESI=ruta -> CF=0 y CL=atributos si está; CF=1 si no. Hispa-Pentium no
; tiene «solo lectura» en sus archivos: si se puede abrir, CL=0.
gua_atrib:
        push eax
        push ebx
        xor al, al
        call dos_abrir
        jc .no
        mov ebx, eax
        call dos_cerrar
        pop ebx
        pop eax
        xor cl, cl
        clc
        ret
.no:
        pop ebx
        pop eax
        stc
        ret
