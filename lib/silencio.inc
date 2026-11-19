; =============================================================================
;  lib/silencio.inc - callar el sonido antes de reiniciar o apagar el equipo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  [REINICIO-SILENCIO] En los portátiles (HP Mini 5103 con IDT, Toshiba NB250
;  con Realtek ALC269) Ctrl+Alt+Supr reinicia «en caliente»: la BIOS hace un
;  reinicio del procesador (el 8042 o un salto a FFFF:0) y lo demás sigue como
;  estaba. La tarjeta HD Audio sigue encendida: el amplificador de los
;  altavoces (EAPD), los pines con salida, los amplificadores a 0 dB y, si
;  HSPSND tocaba, el flujo de DMA leyendo un búfer que la BIOS va machacando
;  al arrancar. El altavoz del PC, que en estos equipos suena por el códec
;  (la entrada PCBEEP o el generador de pitido), puede quedarse con la
;  puerta abierta. Mientras la BIOS reinicia la controladora y el códec, todo
;  eso suena por los altavoces: la «lluvia de interferencias» que se va
;  apagando.
;
;  silencio (llamada cercana; conserva todos los registros y las banderas):
;   1. el altavoz del PC: puerta cerrada (puerto 61h, bits 0 y 1 a 0) y el
;      canal 2 del 8254 parado en el modo 0 (la salida, a 0);
;   2. HSPSND, si está (INT 2Fh AX=D700h): su función 10h, «apagar»: los
;      flujos parados (también los de la Sound Blaster emulada), los
;      amplificadores en silencio, EAPD y los pines apagados y el códec en
;      D3. Es la única forma de llegar a la tarjeta en modo V86 (HSPEMM);
;   3. con un 386 o mayor, en modo real y con PCI: cada controladora HD Audio
;      (clase 0403h) que no esté en reinicio: los flujos de DMA, CORB/RIRB y
;      el búfer de posiciones parados, sin avisos, y en cada códec que
;      conteste por la interfaz inmediata: los amplificadores en silencio,
;      el generador de pitido apagado, los pines sin salida y sin EAPD, y el
;      grupo de audio en D3 (así queda la tarjeta como al encender). Y cada
;      AC'97 de Intel (clase 0401h): las DMA paradas y los volúmenes en
;      silencio. Sin PCI, en un 8086 o un 286, solo lo 1 y lo 2.
;  No usa datos propios (la tabla de descriptores del modo «irreal» va en la
;  pila), así que vale en código que corre en la HMA o en un controlador.
;  Abre A20 si hace falta (para los registros por encima de 1 MB con el bit
;  20 a 1) y no la vuelve a cerrar: después se reinicia o se apaga.
;
;  Quien lo incluye con «cpu 8086» (Hispa-8086) define SILENCIO_8086: lo de
;  386 solo corre si sil_es386 lo ha visto. SILENCIO_SIN_AC97 deja fuera lo
;  del AC'97 (Hispa-8086: el núcleo tiene que quedarse en menos de 16 K).
; =============================================================================
%ifdef SILENCIO_8086
        cpu 386
%endif

silencio:
        pushf
        cli
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push ds
        push es
        ; 1. el altavoz del PC
        in al, 0x61
        and al, 0xFC                ; puerta del canal 2 y datos del altavoz
        out 0x61, al
        mov al, 0xB0                ; canal 2, las dos mitades, modo 0
        out 0x43, al
        xor al, al
        out 0x42, al
        out 0x42, al
        ; 2. HSPSND (solo si INT 2Fh lleva a alguna parte)
        xor ax, ax
        mov ds, ax
        mov ax, [0x2F*4]
        or ax, [0x2F*4+2]
        jz .nosnd
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne .nosnd
        push es
        push bx
        mov bp, sp
        mov ah, 0x10                ; apagar
        call far [bp]
        add sp, 4
        cli
.nosnd:
        ; 3. las tarjetas, directamente
        call sil_es386
        jc .fin
        smsw ax
        test al, 1                  ; modo V86: no se llega (lo hace HSPSND)
        jnz .fin
        pushad
        push fs
        mov dx, 0xCF8               ; ¿PCI (mecanismo 1)?
        mov eax, 0x80000000
        out dx, eax
        in eax, dx
        cmp eax, 0x80000000
        jne .nopci
        xor bp, bp                  ; BP bit 0: A20 cerrada
        call sil_a20
        adc bp, 0
        ; modo irreal: FS con base 0 y límite de 4 GB. La tabla, en la pila
        push dword 0x00CF9200       ; datos, 4 GB
        push dword 0x0000FFFF
        push dword 0                ; la nula
        push dword 0
        mov ax, ss
        movzx eax, ax
        shl eax, 4
        movzx ebx, sp
        add eax, ebx
        push eax                    ; GDTR: base...
        push word 15                ; ... y límite
        mov bx, sp
        lgdt [ss:bx]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx                  ; (el límite de 4 GB se queda)
        add sp, 6 + 16
        call sil_pci_todo
.nopci:
        pop fs
        popad
.fin:
        pop es
        pop ds
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        popf
        ret

; CF = 1 si el procesador no es un 386 o mayor (solo instrucciones del 8086)
sil_es386:
        push ax
        pushf
        pushf
        pop ax
        and ax, 0x0FFF              ; en el 8086 y el 80186, 12-15 siempre a 1
        push ax
        popf
        pushf
        pop ax
        and ax, 0xF000
        cmp ax, 0xF000
        je .no
        mov ax, 0x7000              ; en el 286 (modo real), 12-14 siempre a 0
        push ax
        popf
        pushf
        pop ax
        test ax, 0x7000
        jz .no
        popf
        pop ax
        clc
        ret
.no:
        popf
        pop ax
        stc
        ret

; ¿A20 abierta? Si no, se abre (BIOS INT 15h AX=2401h y el puerto 92h).
; CF = 1 si sigue cerrada. Conserva todo
sil_a20:
        push ax
        call .mira
        jnc .r
        mov ax, 0x2401
        int 0x15
        call .mira
        jnc .r
        in al, 0x92
        or al, 2
        and al, 0xFE                ; (el bit 0 reinicia)
        out 0x92, al
        call .mira
.r:
        pop ax
        ret
; CF = 1 si 0000:0500 y FFFF:0510 son el mismo octeto
.mira:
        push ax
        push ds
        push es
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov al, [0x500]
        mov ah, [es:0x510]
        not byte [es:0x510]
        cmp al, [0x500]
        mov [es:0x510], ah
        mov [0x500], al
        pop es
        pop ds
        pop ax
        je .ab
        stc
        ret
.ab:
        clc
        ret

; EBX = dirección PCI (bus, dispositivo, función), AL = registro -> EAX
sil_pci:
        push dx
        movzx eax, al
        and al, 0xFC
        or eax, ebx
        mov dx, 0xCF8
        out dx, eax
        mov dl, 0xFC
        in eax, dx
        pop dx
        ret

; los buses 0 a 15: cada HD Audio y cada AC'97 de Intel
sil_pci_todo:
        mov ebx, 0x80000000
.l:
        xor al, al
        call sil_pci
        cmp ax, 0xFFFF
        jne .hay
        test bh, 7                  ; sin la función 0, nada en ese sitio
        jnz .fn
        jmp .dev
.hay:
        mov si, ax                  ; SI = fabricante
        mov al, 8
        call sil_pci
        shr eax, 16                 ; clase y subclase
        cmp ax, 0x0403
        jne .nhda
        call sil_hda
        jmp .mf
.nhda:
%ifndef SILENCIO_SIN_AC97
        cmp ax, 0x0401
        jne .mf
        cmp si, 0x8086
        jne .mf
        call sil_ac97
%endif
.mf:
        test bh, 7
        jnz .fn
        mov al, 0x0C                ; ¿varias funciones?
        call sil_pci
        test eax, 0x00800000
        jz .dev
.fn:
        add ebx, 0x100
        test bh, 7
        jnz .l
        jmp .sig
.dev:
        and bh, 0xF8
        add ebx, 0x800
.sig:
        cmp ebx, 0x80100000
        jb .l
        ret

; unos 2 ms
sil_espera:
        push ax
        push cx
        mov cx, 2000
.l:
        in al, 0x80
        loop .l
        pop cx
        pop ax
        ret

; ---- HD Audio en EBX (PCI); FS = 4 GB, BP bit 0 = A20 cerrada ----
sil_hda:
        pushad
        mov al, 4
        call sil_pci
        test al, 2                  ; memoria sin activar: no suena
        jz .r
        mov al, 0x10
        call sil_pci
        test al, 1
        jnz .r
        mov esi, eax
        and esi, 0xFFFFFFF0
        jz .r
        and al, 6
        cmp al, 4                   ; de 64 bits: arriba tiene que ser 0
        jne .b32
        mov al, 0x14
        call sil_pci
        test eax, eax
        jnz .r
.b32:
        test bp, 1
        jz .a20
        test esi, 0x00100000
        jnz .r
.a20:
        test byte [fs:esi+0x08], 1  ; GCTL.CRST a 0: en reinicio, callada
        jz .r
        ; los flujos: entrada, salida y bidireccionales (GCAP)
        movzx cx, byte [fs:esi+1]
        mov al, cl
        shr al, 4                   ; OSS
        and cl, 0x0F                ; ISS
        add cl, al
        mov al, [fs:esi]
        shr al, 3                   ; BSS
        add cl, al
        lea edi, [esi+0x80]
        jcxz .nos
.st:
        and byte [fs:edi], 0xE0     ; fuera RUN, SRST y los avisos
        add edi, 0x20
        loop .st
.nos:
        and byte [fs:esi+0x4C], 0xFC ; CORB parada
        and byte [fs:esi+0x5C], 0xF8 ; RIRB parada
        and byte [fs:esi+0x70], 0xFE ; sin búfer de posiciones
        mov dword [fs:esi+0x20], 0  ; INTCTL: ningún aviso
        call sil_espera
        ; los códecs que se han presentado (STATESTS); si nadie, del 0 al 3
        mov dx, [fs:esi+0x0E]
        and dx, 0x7FFF
        jnz .c
        mov dl, 0x0F
.c:
        xor cl, cl
.cod:
        shr dx, 1
        jnc .nc
        call sil_codec
.nc:
        inc cl
        test dx, dx
        jnz .cod
.r:
        popad
        ret

; EAX = orden sin la dirección del códec, CL = códec, FS:ESI = registros
; -> EAX = respuesta; CF si no contesta. Interfaz inmediata (60h, 64h, 68h)
sil_cmd:
        push ecx
        push edx
        movzx ecx, cl
        shl ecx, 28
        or eax, ecx
        mov edx, eax
        mov cx, 2000
.l1:
        test byte [fs:esi+0x68], 1  ; ICB: ¿libre?
        jz .lib
        in al, 0x80
        loop .l1
.lib:
        mov byte [fs:esi+0x68], 2   ; IRV a 0
        mov [fs:esi+0x60], edx
        mov byte [fs:esi+0x68], 1   ; que salga
        mov cx, 2000
.l2:
        test byte [fs:esi+0x68], 2  ; IRV: la respuesta
        jnz .ok
        in al, 0x80
        loop .l2
        mov byte [fs:esi+0x68], 2
        stc
        jmp .x
.ok:
        mov eax, [fs:esi+0x64]
        mov byte [fs:esi+0x68], 2
        clc
.x:
        pop edx
        pop ecx
        ret
; lo mismo con el nodo en BL
sil_nodo:
        push ebx
        movzx ebx, bl
        shl ebx, 20
        or eax, ebx
        pop ebx
        jmp sil_cmd

; el códec CL: cada grupo de función de audio
sil_codec:
        pushad
        xor bl, bl                  ; la raíz: sus grupos (parámetro 04h)
        mov eax, 0x000F0004
        call sil_nodo
        jc .r
        mov dl, al                  ; DL = cuántos
        and dl, 0x7F
        jz .r
        shr eax, 16
        mov bl, al                  ; BL = el primero
.g:
        mov eax, 0x000F0005         ; tipo del grupo
        call sil_nodo
        jc .ng
        cmp al, 1                   ; de audio
        jne .ng
        call sil_afg
.ng:
        inc bl
        dec dl
        jnz .g
.r:
        popad
        ret

; el grupo de audio BL del códec CL
sil_afg:
        pushad
        mov bh, bl                  ; BH = el grupo
        mov eax, 0x000F0004
        call sil_nodo
        jc .r
        mov dl, al
        and dl, 0x7F                ; DL = cuántos nodos
        jz .r
        shr eax, 16
        mov dh, al                  ; DH = el primero
        ; 1. todos los amplificadores en silencio
        mov bl, dh
        mov ch, dl
.m:
        mov eax, 0x000F0009         ; capacidades del nodo
        call sil_nodo
        jc .mn
        mov edi, eax
        test di, 4                  ; amplificador de salida
        jz .mi
        mov eax, 0x0003B080         ; salida, izquierda y derecha, silencio
        call sil_nodo
.mi:
        test di, 2                  ; amplificadores de entrada
        jz .mn
        mov eax, 0x000F000E         ; cuántas conexiones
        call sil_nodo
        jc .mn
        and al, 0x7F
        jnz .m1
        inc al
.m1:
        cmp al, 16
        jbe .m2
        mov al, 16
.m2:
        movzx si, al
        xor ah, ah                  ; AH = la entrada
.mj:
        push eax
        movzx eax, ah
        shl eax, 8
        or eax, 0x00037080          ; entrada, izquierda y derecha, silencio
        call sil_nodo
        pop eax
        inc ah
        dec si
        jnz .mj
.mn:
        inc bl
        dec ch
        jnz .m
        call sil_espera
        ; 2. pines sin salida ni EAPD; el generador de pitido, apagado
        mov bl, dh
        mov ch, dl
.p:
        mov eax, 0x000F0009
        call sil_nodo
        jc .pn
        shr eax, 20
        and al, 0x0F
        cmp al, 4                   ; pin
        jne .pb
        mov eax, 0x00070700         ; control del pin: nada
        call sil_nodo
        mov eax, 0x00070C00         ; EAPD/BTL: apagado
        call sil_nodo
        jmp .pn
.pb:
        cmp al, 7                   ; generador de pitido
        jne .pn
        mov eax, 0x00070A00
        call sil_nodo
.pn:
        inc bl
        dec ch
        jnz .p
        call sil_espera
        ; 3. el grupo, en D3
        mov bl, bh
        mov eax, 0x00070503
        call sil_nodo
        call sil_espera
.r:
        popad
        ret

%ifndef SILENCIO_SIN_AC97
; ---- AC'97 de Intel en EBX (PCI): DMA paradas y todo en silencio ----
sil_ac97:
        pushad
        mov al, 4
        call sil_pci
        test al, 1                  ; E/S sin activar
        jz .r
        mov al, 0x14                ; NABMBAR
        call sil_pci
        test al, 1
        jz .r
        and ax, 0xFFC0
        jz .r
        mov dx, ax
        xor al, al
        add dx, 0x0B                ; entrada PCM
        out dx, al
        add dx, 0x10                ; salida PCM
        out dx, al
        add dx, 0x10                ; micrófono
        out dx, al
        mov al, 0x10                ; NAMBAR
        call sil_pci
        test al, 1
        jz .r
        and ax, 0xFFFE
        jz .r
        mov dx, ax
        mov ax, 0x8000
        add dx, 0x02                ; general
        out dx, ax
        add dx, 0x02                ; auriculares
        out dx, ax
        add dx, 0x06                ; altavoz del PC (0Ah)
        out dx, ax
        add dx, 0x0E                ; PCM (18h)
        mov ax, 0x8808
        out dx, ax
.r:
        popad
        ret
%endif

%ifdef SILENCIO_8086
        cpu 8086
%endif
