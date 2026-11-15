; =============================================================================
;  Hispa-DOS - la versión del sistema (UN SOLO SITIO para subirla)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Para subir la versión del sistema, cambie SOLO estas dos líneas:
;    VER_STR  la versión como texto ("mayor.menor.parche"), para enseñarla.
;    VER_NUM  la misma, como número comparable: mayor*10000 + menor*100 +
;             parche. Así ACTUALIZAR puede comparar la versión instalada con
;             la que anuncia el servidor y decir si hay una más nueva.
;
;  Lo incluyen shell/command.asm (VERSION/VER y el saludo) y
;  utils/actualizar.asm (la comparación). No lleva acentos ni eñes: así no
;  hace falta pasarlo por cp437.
;
;  Ojo: no es lo mismo que «Hispa Ventanas 0.3» (otro programa, otra versión).
; =============================================================================

%define VER_STR "0.9.14"
VER_NUM     equ 0*10000 + 9*100 + 14        ; 0.9.14 -> 914
