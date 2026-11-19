; =============================================================================
;  Hispa-DOS - la versión del sistema (UN SOLO SITIO para subirla)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
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

%define VER_STR "0.9.27"
VER_NUM     equ 0*10000 + 9*100 + 27        ; 0.9.27 -> 927

;  VER_EDI  nivel de "riqueza" de la EDICION instalada (0 = la unica edicion de
;  hoy: Hispa-386, con Hispa Ventanas, 386+). ACTUALIZAR lo usa para recomendar
;  una edicion mas rica cuando el equipo da para mas (p.ej. pasar de una edicion
;  basica de solo texto, nivel 0, a una con entorno grafico, nivel mayor). Al
;  crear ediciones nuevas, suba este numero en la edicion que las traiga.
VER_EDI     equ 0
