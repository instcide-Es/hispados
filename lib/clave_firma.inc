; =============================================================================
;  lib/clave_firma.inc - la clave pública con la que ACTUALIZAR comprueba la
;  firma de versiones.txt (Ed25519; docs/RED.md, «La firma»). Es la misma
;  que la de los catálogos de Hispa-Pentium (castel/nucleo32/clave.cas): una
;  sola clave para todo Hispa-DOS. La privada no está en el repositorio;
;  firma quien la tiene, con tools/firma_versiones.py --clave RUTA.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
%define CLAVE_HUELLA "3ae4b4b9447dc1c4"
clave_act:
        db 0x3A, 0xE4, 0xB4, 0xB9, 0x44, 0x7D, 0xC1, 0xC4, 0xCC, 0x04, 0xA7, 0x57, 0x63, 0x37, 0x56, 0x9A
        db 0x30, 0x59, 0xA5, 0xB9, 0xB5, 0x19, 0x59, 0x10, 0x09, 0xF9, 0xF6, 0x20, 0x6E, 0xAF, 0x5F, 0xA3
