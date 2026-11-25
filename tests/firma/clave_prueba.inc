; =============================================================================
;  tests/firma/clave_prueba.inc - la clave pública DE PRUEBA de ACTUALIZAR
;  Solo la lleva build/ACTPRUEB.COM (make ... -DCLAVE_PRUEBA), la copia de
;  ACTUALIZAR para las pruebas: su privada está a la vista en
;  tests/firma/clave_prueba.key. NUNCA va en una imagen ni en un paquete.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
%define CLAVE_HUELLA "dc91ce8533460a90"
clave_act:
        db 0xDC, 0x91, 0xCE, 0x85, 0x33, 0x46, 0x0A, 0x90, 0xBB, 0xFC, 0x26, 0x2E, 0xE7, 0x9E, 0x4C, 0x44
        db 0x60, 0x99, 0x12, 0xF6, 0xB2, 0x70, 0x23, 0x05, 0xF0, 0xB5, 0x58, 0x23, 0x28, 0x9F, 0x27, 0x44
