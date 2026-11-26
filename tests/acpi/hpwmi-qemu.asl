/*
 * hpwmi-qemu.asl - (HPRADIO) La WMI de HP de mentira, para probar en QEMU lo
 * que hace ACPI.BIN con el Wi-Fi de los HP (acpi/pentium/hpradio.c; castel/
 * pruebas.sh, la parte «wifi», la carga con -acpitable file=hpwmi-qemu.aml).
 * Como \_SB.WMID del HP Mini 5103 (su DSDT: tests/acpi/hp-mini-5103/): un
 * aparato PNP0C14 con el GUID de la BIOS de HP (5FB7F034-2C63-45E9-BE91-
 * 3D44E2C707E4, objeto «AA») y su método WMAA, que solo sabe la consulta 1Bh
 * (la tabla de las radios, como la lee hp-wmi de Linux):
 *   - leer: 2 radios, el Wi-Fi (14E4:4727, la BCM4313, número 0) con el estado
 *     de PWR y el Bluetooth (número 1) encendido;
 *   - escribir { 01h, 00h, número, encender }: el Wi-Fi se enciende (01h y 02h
 *     de PWR) o se apaga; ESCR cuenta las escrituras.
 * PWR empieza en 0Ch: la Configuración de la BIOS (04h) y la tecla (08h) lo
 * dejan, pero está apagado por programa (sin el 02h), como lo deja Windows o
 * Linux al apagar el Wi-Fi antes de apagar el equipo.
 * Compilar: iasl hpwmi-qemu.asl  (el .aml va en el repositorio)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
DefinitionBlock ("", "SSDT", 2, "HISPA ", "HPWMIPRU", 0x00000001)
{
    Scope (\_SB)
    {
        Device (WMID)
        {
            Name (_HID, EisaId ("PNP0C14"))
            Name (_UID, 0x00)
            Name (_WDG, Buffer (0x14)
            {
                0x34, 0xF0, 0xB7, 0x5F, 0x63, 0x2C, 0xE9, 0x45,
                0xBE, 0x91, 0x3D, 0x44, 0xE2, 0xC7, 0x07, 0xE4,
                0x41, 0x41, 0x01, 0x02
            })
            Name (PWR, 0x0C)
            Name (ESCR, 0x00)
            Method (WMAA, 3, Serialized)
            {
                CreateDWordField (Arg2, 0x00, SNIN)
                CreateDWordField (Arg2, 0x04, COMD)
                CreateDWordField (Arg2, 0x08, CMTP)
                CreateDWordField (Arg2, 0x0C, DASI)
                Local0 = Buffer (0x88) {}
                CreateDWordField (Local0, 0x00, SNOU)
                CreateDWordField (Local0, 0x04, RTCD)
                SNOU = 0x4C494146
                RTCD = 0x04
                If ((SNIN == 0x55434553) && (CMTP == 0x1B))
                {
                    If (COMD == 0x01)
                    {
                        If (Arg1 < 0x03)
                        {
                            RTCD = 0x05
                        }
                        Else
                        {
                            Local0 [0x0F] = 0x02
                            Local0 [0x18] = 0x00
                            Local0 [0x19] = 0x01
                            Local0 [0x1A] = 0xE4
                            Local0 [0x1B] = 0x14
                            Local0 [0x1C] = 0x27
                            Local0 [0x1D] = 0x47
                            Local0 [0x1E] = 0x3C
                            Local0 [0x1F] = 0x10
                            Local0 [0x20] = 0x83
                            Local0 [0x21] = 0x14
                            Local0 [0x22] = 0x00
                            Local0 [0x23] = PWR
                            Local0 [0x28] = 0x01
                            Local0 [0x29] = 0x02
                            Local0 [0x32] = 0x01
                            Local0 [0x33] = 0x0F
                            RTCD = 0x00
                            SNOU = 0x53534150
                        }
                    }

                    If ((COMD == 0x02) && (DASI >= 0x04))
                    {
                        CreateByteField (Arg2, 0x12, RID)
                        CreateByteField (Arg2, 0x13, RON)
                        ESCR++
                        If (RID == 0x00)
                        {
                            If (RON)
                            {
                                PWR |= 0x03
                            }
                            Else
                            {
                                PWR &= 0xFC
                            }
                        }

                        RTCD = 0x00
                        SNOU = 0x53534150
                    }
                }

                Return (Local0)
            }
        }
    }
}
