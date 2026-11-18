/*
 * bateria-qemu.asl - Una batería de mentira para probar HSPACPI en QEMU
 * (tests/run_acpi_tests.sh la carga con -acpitable file=bateria-qemu.aml).
 * QEMU tiene ACPI pero no batería ni controlador embebido: esta SSDT declara
 * una batería (PNP0C0A) y un adaptador (ACPI0003) cuyos métodos leen dos
 * octetos de la memoria CMOS (puertos 70h/71h), que la prueba cambia desde
 * DOS con CMOSW:
 *    CMOS 60h   el nivel, de 0 a 100 (la batería es de 5000 mAh)
 *    CMOS 61h   bit 0: con cable (y cargando); bit 1: la batería está puesta
 * _BST usa un Mutex, duerme 20 ms (Sleep) y avisa (Notify), como los equipos
 * de verdad, para que la prueba pase por lo mismo.
 * Compilar: iasl bateria-qemu.asl  (el .aml va en el repositorio)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
DefinitionBlock ("", "SSDT", 1, "HISPA ", "BATPRUEB", 0x00000001)
{
    Scope (\_SB)
    {
        OperationRegion (HCMS, SystemIO, 0x70, 0x02)
        Field (HCMS, ByteAcc, NoLock, Preserve)
        {
            HIDX,   8,
            HDAT,   8
        }

        IndexField (HIDX, HDAT, ByteAcc, NoLock, Preserve)
        {
            Offset (0x60),
            HPCT,   8,
            HFLG,   8
        }

        Mutex (HMUT, 0x00)
        Name (HANT, 0xFF)

        Device (HBAT)
        {
            Name (_HID, EisaId ("PNP0C0A"))
            Name (_UID, One)
            Name (_PCL, Package (0x01)
            {
                \_SB
            })
            Method (_STA, 0, NotSerialized)
            {
                If ((HFLG & 0x02))
                {
                    Return (0x1F)
                }

                Return (0x0F)
            }

            Method (_BIF, 0, NotSerialized)
            {
                Return (Package (0x0D)
                {
                    One,
                    5000,
                    5000,
                    One,
                    11100,
                    500,
                    250,
                    10,
                    10,
                    "HISPA-PRUEBA",
                    "0001",
                    "LION",
                    "Hispa-DOS"
                })
            }

            Method (_BST, 0, Serialized)
            {
                Name (PBST, Package (0x04)
                {
                    Zero,
                    1000,
                    Zero,
                    11000
                })
                Acquire (HMUT, 0xFFFF)
                Sleep (20)
                Local0 = (HPCT * 50)
                Local1 = (HFLG & One)
                Release (HMUT)
                If (Local1)
                {
                    PBST [Zero] = 0x02
                }
                Else
                {
                    PBST [Zero] = One
                }

                PBST [0x02] = Local0
                If ((HANT != Local1))
                {
                    HANT = Local1
                    Notify (\_SB.HBAT, 0x80)
                }

                Return (PBST)
            }
        }

        Device (HACA)
        {
            Name (_HID, "ACPI0003")
            Name (_PCL, Package (0x01)
            {
                \_SB
            })
            Method (_PSR, 0, NotSerialized)
            {
                Return ((HFLG & One))
            }
        }
    }
}
