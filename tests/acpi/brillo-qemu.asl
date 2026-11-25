/*
 * brillo-qemu.asl - Un brillo de pantalla de mentira para probar ACPICA en
 * Hispa-64 (castel/nucleo64/pruebas.sh lo carga con -acpitable
 * file=brillo-qemu.aml): QEMU no tiene pantalla con brillo por ACPI. Un
 * dispositivo con _BCL (los niveles: 100 con cable, 40 con batería, y de 10
 * en 10), _BCM (lo guarda) y _BQC (lo devuelve), como el de la pantalla de
 * un portátil (\_SB.PCI0.GFX0.DD1F...).
 * Sin iasl en el equipo de compilación: brillo-qemu.aml lo hace a mano
 * haz_brillo.py (los mismos octetos que daría «iasl brillo-qemu.asl», salvo
 * el compilador de la cabecera, «HISP»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
DefinitionBlock ("", "SSDT", 1, "HISPA ", "BRILLO", 0x00000001)
{
    Scope (\_SB)
    {
        Device (HLCD)
        {
            Name (_ADR, 0x0400)
            Name (BRIL, 0x32)
            Method (_BCL, 0, NotSerialized)
            {
                Return (Package (0x0C)
                {
                    100, 40, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
                })
            }
            Method (_BCM, 1, NotSerialized)
            {
                BRIL = Arg0
            }
            Method (_BQC, 0, NotSerialized)
            {
                Return (BRIL)
            }
        }
    }
}
