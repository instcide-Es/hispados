/* achispa.h - ACPICA en Hispa-486: el entorno (lo incluye acenv.h con -D__HISPA__).
 * Núcleo de 32 bits sin sistema debajo, un solo hilo, sin biblioteca de C.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ACHISPA_H
#define ACHISPA_H

#define ACPI_MACHINE_WIDTH          32
#define ACPI_32BIT_PHYSICAL_ADDRESS
#define COMPILER_DEPENDENT_INT64    long long
#define COMPILER_DEPENDENT_UINT64   unsigned long long
#define ACPI_USE_NATIVE_DIVIDE			/* __udivdi3 y __umoddi3, en base.c */
#define ACPI_USE_DO_WHILE_0
#define ACPI_USE_LOCAL_CACHE
#define ACPI_SINGLE_THREADED
#define ACPI_MUTEX_TYPE             ACPI_BINARY_SEMAPHORE
#undef ACPI_USE_SYSTEM_CLIBRARY			/* las de ACPICA (utclib.c) */
#undef ACPI_USE_STANDARD_HEADERS
#undef ACPI_DEBUG_OUTPUT
#undef ACPI_DEBUGGER
#undef ACPI_DISASSEMBLER
#undef ACPI_DBG_TRACK_ALLOCATIONS

#endif
