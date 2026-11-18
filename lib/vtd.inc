; =============================================================================
;  vtd.inc - [VTD] la protección de DMA de Intel (VT-d) que deja puesta un
;  firmware UEFI, y cómo se le abre el paso a nuestra tarjeta
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lo usan HSPSND y AUDICION. Escrito desde cero con la especificación
;  pública de Intel («Virtualization Technology for Directed I/O») y las
;  hojas de los procesadores Core (registros GFXVTBAR y VTDPVC0BAR).
;
;  El problema (MacBook Air 2017 arrancado con HispaBIOS): el firmware de
;  Apple enciende la traducción de DMA de VT-d para que ningún dispositivo
;  pueda leer ni escribir la memoria por su cuenta, y solo se la abre a los
;  que él maneja (el USB, el disco). Un sistema UEFI la quita al salir de los
;  servicios de arranque; HispaBIOS no sale (el firmware sigue leyendo el
;  disco), así que sigue puesta. Para la tarjeta de sonido quiere decir que
;  la controladora HD Audio no puede leer de la memoria ni la lista de
;  búferes ni el sonido: el flujo se para solo con «error de descriptor»
;  (SDSTS = 10h), la posición no avanza y las órdenes por CORB/RIRB (que
;  también van por DMA) no tienen respuesta; las inmediatas (sin DMA) sí.
;
;  Lo que se hace, de menos a más:
;   P. las regiones de memoria protegidas (PMEN), si están puestas: fuera
;      (solo quita una protección: no cambia nada de lo que ya funciona);
;   A. «paso directo» solo para nuestro dispositivo: su entrada de contexto
;      (tabla raíz -> tabla de contexto del bus -> entrada del dispositivo)
;      se pone de tipo «pass-through». Lo del firmware no se toca;
;   B. si con eso no basta (o no se puede): se apaga la traducción de esa
;      unidad (GCMD.TE a 0), que es lo que hace un sistema al arrancar.
;
;  Dónde están las unidades: sin tablas ACPI (HispaBIOS no las da), por los
;  registros del puente principal de Intel: MCHBAR (00:00.0, 48h) y en él
;  GFXVTBAR (5400h, la unidad de la gráfica: no se toca) y VTDPVC0BAR
;  (5410h, la de todo lo demás: la del sonido).
;
;  Quien lo incluye tiene FS plano (modo irreal), la A20 abierta y DS = CS.
;  En modo V86 no se llama.
; =============================================================================

vtd_base    dd 0                    ; la unidad de todo lo demás (0: no hay)
vtd_gfx     dd 0                    ; la de la gráfica (solo para el informe)
vtd_mch     dd 0                    ; MCHBAR tal cual (para el informe)
vtd_sim     dd 0                    ; pruebas: la unidad está aquí (QEMU)
vtd_ent     dd 0                    ; dónde está la entrada de contexto
vtd_viejo   times 4 dd 0            ; lo que había en ella
vtd_tes     db 0                    ; bit 0: traducción encendida; bit 1: PMR
vtd_res     db 0                    ; lo que hizo vtd_abrir (ver abajo)
vtd_mirar   db 0                    ; 1: vtd_abrir solo mira (no escribe)

; Busca las unidades -> vtd_base, vtd_gfx, vtd_mch y vtd_tes; CF si no hay
; unidad para nuestros dispositivos. Solo lee. Conserva los registros.
vtd_buscar:
        pushad
        xor eax, eax
        mov [vtd_base], eax
        mov [vtd_gfx], eax
        mov [vtd_mch], eax
        mov [vtd_tes], al
        mov eax, [vtd_sim]
        test eax, eax
        jnz .cand
        mov dx, 0xCF8
        in eax, dx
        push eax                    ; (la dirección PCI que hubiera)
        mov eax, 0x80000000         ; 00:00.0: ¿puente principal de Intel?
        call .cfg
        cmp ax, 0x8086
        jne .nop
        mov eax, 0x80000008
        call .cfg
        shr eax, 16
        cmp ax, 0x0600
        jne .nop
        mov eax, 0x8000004C         ; MCHBAR, parte alta: a cero
        call .cfg
        test eax, eax
        jnz .nop
        mov eax, 0x80000048
        call .cfg
        mov [vtd_mch], eax
.nop:
        pop eax
        mov dx, 0xCF8
        out dx, eax
        mov eax, [vtd_mch]
        test al, 1                  ; MCHBAR encendido
        jz .no
        test ax, 0x7FFE             ; y de 32 KB (los de antes de 2011, de
        jnz .no                     ; 16 KB, no tienen estos registros)
        push eax
        mov eax, 1                  ; procesador con virtualización (VMX):
        db 0x0F, 0xA2               ; (CPUID) sin ella no hay VT-d
        pop eax
        test cl, 0x20
        jz .no
        and eax, 0xFFFF8000
        cmp eax, 0x00100000
        jb .no
        mov esi, eax
        mov eax, [fs:esi+0x5400]    ; GFXVTBAR
        mov edx, [fs:esi+0x5404]
        call .val
        mov [vtd_gfx], eax
        mov eax, [fs:esi+0x5410]    ; VTDPVC0BAR
        mov edx, [fs:esi+0x5414]
        call .val
        jmp .set
.cand:
        call .unit
.set:
        mov [vtd_base], eax
        test eax, eax
        jz .no
        mov esi, eax
        test dword [fs:esi+0x1C], 0x80000000   ; GSTS.TES
        jz .npm
        or byte [vtd_tes], 1
.npm:
        test dword [fs:esi+0x08], 0x60   ; CAP: PLMR o PHMR (hay PMEN)
        jz .ok
        test dword [fs:esi+0x64], 0x01   ; PMEN.PRS
        jz .ok
        or byte [vtd_tes], 2
.ok:
        popad
        clc
        ret
.no:
        popad
        stc
        ret
; EAX = registro de configuración (mecanismo 1) -> EAX
.cfg:
        mov dx, 0xCF8
        out dx, eax
        mov dx, 0xCFC
        in eax, dx
        ret
; EDX:EAX = un registro xxVTBAR -> EAX = la base de la unidad (0 si no vale)
.val:
        test al, 1
        jz .v0
        test edx, edx
        jnz .v0
        and eax, 0xFFFFF000
; EAX = base -> EAX igual si aquello parece una unidad de VT-d; si no, 0
.unit:
        cmp eax, 0x00100000
        jb .v0
        push ebx
        mov ebx, [fs:eax]           ; versión: de la 1.0 a la 7.x
        cmp ebx, 0x10
        jb .u0
        cmp ebx, 0x7F
        ja .u0
        mov ebx, [fs:eax+0x08]      ; CAP
        cmp ebx, 0xFFFFFFFF
        je .u0
        test ebx, 0x00001F00        ; algún ancho de direcciones (SAGAW)
        jz .u0
        shr ebx, 24                 ; dónde van los registros de faltas
        jnz .u1
        test dword [fs:eax+0x0C], 3
        jz .u0
.u1:
        mov ebx, [fs:eax+0x10]      ; ECAP: dónde va el de la IOTLB
        cmp ebx, 0xFFFFFFFF
        je .u0
        test ebx, 0x0003FF00
        jz .u0
        pop ebx
        ret
.u0:
        pop ebx
.v0:
        xor eax, eax
        ret

; Las regiones de memoria protegidas (PMEN), fuera. Conserva los registros.
vtd_sin_pmr:
        pushad
        mov esi, [vtd_base]
        test esi, esi
        jz .r
        test byte [vtd_tes], 2
        jz .r
        mov dword [fs:esi+0x64], 0  ; EPM a 0
        lea ecx, [esi+0x64]
        mov edx, 1                  ; hasta que PRS baje
        call vtd_espera
        jc .r
        and byte [vtd_tes], ~2
.r:
        popad
        ret

; BX = nuestro dispositivo (bus en BH, dispositivo y función en BL): su
; entrada de contexto, de «paso directo» -> AL = vtd_res:
;    0  no hay traducción encendida: nada que hacer
;    1  hecho (el firmware no tenía entrada para él)
;    2  hecho (la tenía, cerrada: cambiada y las cachés vaciadas)
;    3  ya estaba de paso directo
;  FFh no lo está, y no se ha tocado (vtd_mirar)
;    4  cambiada, pero las cachés no se han podido vaciar (invalidación en
;       cola encendida): puede no valer hasta que se apague la traducción
;  81h la tabla raíz está por encima de 4 GB     82h tabla raíz ampliada
;  83h el bus no tiene tabla de contexto         84h esa tabla, sobre 4 GB
;  85h la unidad no admite el paso directo       87h sin ancho de direcciones
; Lo que había en la entrada queda en vtd_viejo, y su sitio en vtd_ent.
vtd_abrir:
        pushad
        xor eax, eax
        mov [vtd_ent], eax
        mov [vtd_viejo], eax
        mov [vtd_viejo+4], eax
        mov [vtd_viejo+8], eax
        mov [vtd_viejo+12], eax
        mov byte [vtd_res], 0
        mov esi, [vtd_base]
        test esi, esi
        jz .r
        test dword [fs:esi+0x1C], 0x80000000   ; GSTS.TES
        jz .r
        mov byte [vtd_res], 0x85
        test dword [fs:esi+0x10], 0x40   ; ECAP.PT
        jz .r
        mov byte [vtd_res], 0x81
        cmp dword [fs:esi+0x24], 0      ; RTADDR, parte alta
        jne .r
        mov eax, [fs:esi+0x20]
        mov byte [vtd_res], 0x82
        test ax, 0x0C00                 ; tabla ampliada o «escalable»
        jnz .r
        and eax, 0xFFFFF000
        movzx edx, bh
        shl edx, 4
        lea edi, [eax+edx]              ; la entrada raíz del bus
        mov byte [vtd_res], 0x83
        mov eax, [fs:edi]
        test al, 1
        jz .r
        mov byte [vtd_res], 0x84
        cmp dword [fs:edi+4], 0
        jne .r
        and eax, 0xFFFFF000
        movzx edx, bl
        shl edx, 4
        lea edi, [eax+edx]              ; la entrada de contexto
        mov [vtd_ent], edi
        mov eax, [fs:edi]
        mov [vtd_viejo], eax
        mov eax, [fs:edi+4]
        mov [vtd_viejo+4], eax
        mov eax, [fs:edi+8]
        mov [vtd_viejo+8], eax
        mov eax, [fs:edi+12]
        mov [vtd_viejo+12], eax
        mov al, [vtd_viejo]
        and al, 0x0D                    ; presente y tipo
        mov byte [vtd_res], 3
        cmp al, 0x09                    ; ya de paso directo
        je .r
        mov byte [vtd_res], 0xFF
        cmp byte [vtd_mirar], 0         ; solo mirar: no se toca
        jne .r
        ; ancho de direcciones: el mayor que admita (39, 48 o 57 bits)
        mov ecx, [fs:esi+0x08]
        mov eax, ecx
        shr eax, 8
        and eax, 0x0E
        mov byte [vtd_res], 0x87
        jz .r
        bsr eax, eax                    ; EAX = AW (1 a 3)
        ; dominio: el último (el firmware los reparte desde el principio)
        and cl, 7
        add cl, cl
        add cl, 4
        mov edx, 1
        shl edx, cl
        dec edx
        and edx, 0xFFFF
        shl edx, 8
        or eax, edx
        ; primero ausente, después lo demás y al final presente
        and dword [fs:edi], 0xFFFFFFFE
        mov [fs:edi+8], eax
        mov dword [fs:edi+12], 0
        mov dword [fs:edi+4], 0
        mov dword [fs:edi], 0x00000009  ; presente, paso directo
        mov byte [vtd_res], 1
        test byte [vtd_viejo], 1
        jz .inv
        mov byte [vtd_res], 2
.inv:
        test dword [fs:esi+0x1C], 0x04000000   ; GSTS.QIES: invalidación en cola
        jz .go
        cmp byte [vtd_res], 2
        jne .r
        mov byte [vtd_res], 4
        jmp .r
.go:
        call vtd_vaciar
.r:
        popad
        mov al, [vtd_res]
        ret

; ESI = la unidad: vaciar las cachés de contexto y de traducciones (todo)
vtd_vaciar:
        pushad
        test dword [fs:esi+0x08], 0x10   ; CAP.RWBF: antes, el búfer de escritura
        jz .c
        mov eax, [fs:esi+0x1C]
        and eax, 0x96FFFFFF
        or eax, 0x08000000
        mov [fs:esi+0x18], eax
        lea ecx, [esi+0x1C]
        mov edx, 0x08000000
        call vtd_espera
.c:
        mov dword [fs:esi+0x28], 0      ; CCMD: invalidación global
        mov dword [fs:esi+0x2C], 0xA0000000
        lea ecx, [esi+0x2C]
        mov edx, 0x80000000
        call vtd_espera
        mov eax, [fs:esi+0x10]          ; el registro de la IOTLB
        shr eax, 8
        and eax, 0x3FF
        shl eax, 4
        lea edi, [esi+eax]
        mov eax, [fs:esi+0x0C]          ; CAP: vaciado de lecturas y escrituras
        shr eax, 22
        and eax, 3
        shl eax, 16
        or eax, 0x90000000              ; IVT, global
        mov dword [fs:edi+8], 0
        mov [fs:edi+12], eax
        lea ecx, [edi+12]
        mov edx, 0x80000000
        call vtd_espera
        popad
        ret

; Apagar la traducción de la unidad (y las regiones protegidas) -> CF si
; no se apaga. Conserva los registros.
vtd_apagar:
        pushad
        mov esi, [vtd_base]
        test esi, esi
        jz .ok
        call vtd_sin_pmr
        test dword [fs:esi+0x1C], 0x80000000
        jz .ok
        mov eax, [fs:esi+0x1C]
        and eax, 0x16FFFFFF             ; sin TE (y sin las órdenes de una vez)
        mov [fs:esi+0x18], eax
        lea ecx, [esi+0x1C]
        mov edx, 0x80000000
        call vtd_espera
        jc .no
        and byte [vtd_tes], ~1
.ok:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; espera a que [ECX] y EDX sea 0 -> CF si no llega (unas décimas de segundo)
vtd_espera:
        push eax
        push ebx
        mov ebx, 0x40000
.l:
        test [fs:ecx], edx
        jz .ok
        in al, 0x80
        dec ebx
        jnz .l
        pop ebx
        pop eax
        stc
        ret
.ok:
        pop ebx
        pop eax
        clc
        ret
