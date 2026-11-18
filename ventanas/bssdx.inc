; Hispa Ventanas - [DX] variables de la fase 5 de Hispa-486 (win32dx.inc)
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
            alignb 4
dx_prog     resd 1                  ; el programa que tiene la pantalla (0: nadie)
dx_ven      resd 1                  ;   su ventana (la de arriba del todo)
dx_gw       resd 1                  ;   cómo se ve: el modo del juego (ancho, alto)
dx_gh       resd 1
dx_ox       resd 1                  ;   y dónde sale en la pantalla
dx_oy       resd 1
dx_dw       resd 1
dx_dh       resd 1
dx_cursor   resd 1                  ; la cuenta de ShowCursor
