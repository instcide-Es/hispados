; =============================================================================
;  Hispa-Snes - leer y escribir la memoria del 65C816
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los 16 MB que ve el procesador están partidos en 2.048 trozos de 8 KB.
;  pag_lee y pag_esc tienen, para cada trozo, un número tal que [número +
;  dirección] es el octeto (la RAM, la ROM o la RAM del cartucho), o 0 si
;  ese trozo no es memoria sin más (los registros, lo que no hay, la RAM del
;  cartucho más pequeña que 8 KB): entonces lo hace bus_lee o bus_escribe
;  (bus.inc). Escribir en la ROM es un 0 en pag_esc (bus_escribe no hace
;  nada).
;
;  EDX = dirección (24 bits) y EAX = el dato. Conservan todo lo demás
;  (bus_escribe puede pedir al procesador que pare, con cpu_romper y EDI).
; =============================================================================

rd8:
        mov eax, edx
        shr eax, 13
        mov eax, [pag_lee+eax*4]
        test eax, eax
        jz near bus_lee
        movzx eax, byte [eax+edx]
        ret

rd16:
        mov eax, edx
        not eax
        test eax, 0x1FFF            ; ¿el último octeto de un trozo?
        jz .lento
        mov eax, edx
        shr eax, 13
        mov eax, [pag_lee+eax*4]
        test eax, eax
        jz .lento
        movzx eax, word [eax+edx]
        ret
.lento:
        push edx
        call rd8
        push eax
        inc edx
        and edx, 0xFFFFFF
.alto:
        call rd8
        shl eax, 8
        or eax, [esp]
        add esp, 4
        pop edx
        ret
; la parte alta, dando la vuelta dentro del banco 0 (EDX < 10000h)
rd16_b0:
        cmp dx, 0xFFFF
        jne rd16
        push edx
        call rd8
        push eax
        xor edx, edx
        jmp rd16.alto
; dentro del mismo banco (JMP (a,X))
rd16_banco:
        cmp dx, 0xFFFF
        jne rd16
        push edx
        call rd8
        push eax
        and edx, 0xFF0000
        jmp rd16.alto
; dentro de la misma página (la página directa en emulación)
rd16_pag:
        cmp dl, 0xFF
        jne rd16
        push edx
        call rd8
        push eax
        xor dl, dl
        jmp rd16.alto

wr8:
        push ebx
        mov ebx, edx
        shr ebx, 13
        mov ebx, [pag_esc+ebx*4]
        test ebx, ebx
        jz .bus
        mov [ebx+edx], al
        pop ebx
        ret
.bus:
        pop ebx
        jmp bus_escribe

wr16:
        push edx
        push eax
        call wr8
        mov eax, [esp]
        shr eax, 8
        inc edx
        and edx, 0xFFFFFF
.alto:
        call wr8
        pop eax
        pop edx
        ret
wr16_b0:
        cmp dx, 0xFFFF
        jne wr16
        push edx
        push eax
        call wr8
        mov eax, [esp]
        shr eax, 8
        xor edx, edx
        jmp wr16.alto
