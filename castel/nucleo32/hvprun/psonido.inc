; =============================================================================
;  HVPRUN - (SONIDO) el sonido de los programas de Hispa Ventanas (snd_info,
;  snd_pcm, snd_pcm2, snd_orden y snd_campana, las de HSPSND en Ventanas:
;  ventanas/modulos.inc) sobre el sonido de Hispa-Pentium (las llamadas 190 a
;  193, nucleo32/sonido.cas): un canal con la frecuencia y la forma de lo
;  que se toca (si cambian, se abre otro y el de antes acaba de sonar solo).
;  Así suenan Hispa-Nes, Hispa-System, Hispa-Snes y el Reproductor de sonido.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

PS_ABRIR_CANAL  equ 190
PS_DAR          equ 191
PS_CANAL        equ 192
PS_SONIDO       equ 193
SN_CERRAR       equ 0
SN_VACIAR       equ 3
SN_EN_COLA      equ 4
SN_ESPERAR      equ 5
SN_AL_ACABAR    equ 7
SG_ESTADO       equ 0
SG_VOLUMEN      equ 1
SG_SILENCIO     equ 2
SG_SISTEMA      equ 3
SN_COLA_MS      equ 170                 ; lo que se deja ir por delante (como HSPSND: unos 4 trozos)
SN_TROZO_MS     equ 43                  ; un trozo de HSPSND: 2.048 tramas a 48 kHz

; -> EAX = la tarjeta (4 HD Audio, 3 AC'97, 2 Sound Blaster 16; -1: no hay)
;    y snd_est, lo que dice el núcleo de ella
snd_mira:
        push ebx
        push ecx
        push edx
        xor ecx, ecx
        mov ebx, SG_ESTADO
        mov ecx, snd_est
        PSIS PS_SONIDO
        cmp eax, 1
        je .hay
        cmp byte [snd_probado], 0
        jne .no
        ; (sin servicio de sonido: abrir un canal lo pone en marcha; una vez)
        mov byte [snd_probado], 1
        mov ebx, 48000
        mov ecx, 2 | 16 << 8
        PSIS PS_ABRIR_CANAL
        cmp eax, PRIMER_ERROR
        jae .no
        mov ebx, eax
        mov ecx, SN_CERRAR
        PSIS PS_CANAL
        mov ebx, SG_ESTADO
        mov ecx, snd_est
        PSIS PS_SONIDO
        cmp eax, 1
        je .hay
.no:
        or eax, -1
        jmp .x
.hay:
        mov eax, [snd_est + 4]          ; (Estado_sonido.tarjeta)
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; -> EAX=la tarjeta (-1: no hay), ESI=su nombre, EDX=la versión de la interfaz (3)
snd_info:
        call snd_mira
        cmp eax, -1
        je .x
        mov esi, snd_est + 64           ; (Estado_sonido.nombre)
        mov edx, 3
.x:
        ret

; ESI=muestras ECX=octetos EDX=Hz AL=formato (bit 0: 16 bits, bit 1: estéreo):
; a la cola del canal (esperando si está llena, como HSPSND). CF=1 si no hay sonido
snd_pcm:
snd_pcm2:
        pushad
        and eax, 3
        mov ebx, edx
        shl eax, 24
        or ebx, eax                     ; la forma: Hz y formato juntos
        cmp dword [snd_canal], 0
        je .abre
        cmp ebx, [snd_forma]
        je .da
        ; (otra forma: el canal de antes acaba de sonar solo, y uno nuevo)
        push ebx
        mov ebx, [snd_canal]
        mov ecx, SN_AL_ACABAR
        PSIS PS_CANAL
        pop ebx
        mov dword [snd_canal], 0
.abre:
        mov [snd_forma], ebx
        call snd_mira
        cmp eax, -1
        je .no
        mov ebx, [snd_forma]
        mov ecx, ebx
        shr ecx, 24
        mov eax, ecx
        and ecx, 2
        shr ecx, 1
        inc ecx                         ; los canales: 1 o 2
        and eax, 1
        shl eax, 3
        add eax, 8                      ; los bits: 8 o 16
        shl eax, 8
        or ecx, eax
        and ebx, 0xFFFFFF
        PSIS PS_ABRIR_CANAL
        cmp eax, PRIMER_ERROR
        jae .no
        mov [snd_canal], eax
.da:
        mov ebx, [snd_canal]
        mov ecx, [esp + 4]              ; (ESI, de pushad)
        mov edx, [esp + 24]             ; (ECX)
        PSIS PS_DAR
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; los octetos por segundo de lo que se toca (para la 08h)
snd_por_segundo:
        mov eax, [snd_forma]
        and eax, 0xFFFFFF
        test byte [snd_forma + 3], 1
        jz .b8
        shl eax, 1
.b8:
        test byte [snd_forma + 3], 2
        jz .m
        shl eax, 1
.m:
        ret

; AH=función de HSPSND AL=dato -> EAX como la deja HSPSND. CF=1 si no hay sonido
snd_orden:
        push ebx
        push ecx
        push edx
        push eax
        call snd_mira
        cmp eax, -1
        pop eax
        je .no
        cmp ah, 0x02
        je .espera
        cmp ah, 0x03
        je .para
        cmp ah, 0x05
        je .vol
        cmp ah, 0x06
        je .leevol
        cmp ah, 0x07
        je .mudo
        cmp ah, 0x08
        je .sitio
        cmp ah, 0x00
        je .info
        xor eax, eax
        jmp .si
.info:
        mov eax, [snd_est + 4]
        jmp .si
.espera:
        mov ebx, [snd_canal]
        test ebx, ebx
        jz .cero
        mov ecx, SN_ESPERAR
        PSIS PS_CANAL
        jmp .cero
.para:
        mov ebx, [snd_canal]
        test ebx, ebx
        jz .cero
        mov ecx, SN_VACIAR
        PSIS PS_CANAL
.cero:
        xor eax, eax
        jmp .si
.vol:
        movzx edx, al
        mov ebx, SG_VOLUMEN
        PSIS PS_SONIDO
        xor edx, edx
        mov ebx, SG_SILENCIO
        PSIS PS_SONIDO
        jmp .leevol
.mudo:
        movzx edx, al
        mov ebx, SG_SILENCIO
        PSIS PS_SONIDO
.leevol:
        mov ebx, SG_VOLUMEN
        mov edx, 0xFFFFFFFF
        PSIS PS_SONIDO
        push eax
        mov ebx, SG_SILENCIO
        mov edx, 0xFFFFFFFF
        PSIS PS_SONIDO
        mov ah, al
        pop ecx
        mov al, cl
        movzx eax, ax
        jmp .si
.sitio:
        ; los trozos (de 43 ms) que caben antes de ir 170 ms por delante
        mov ebx, [snd_canal]
        mov eax, 4
        test ebx, ebx
        jz .si
        mov ecx, SN_EN_COLA
        PSIS PS_CANAL
        cmp eax, PRIMER_ERROR
        jae .cero
        mov ecx, 1000
        mul ecx
        mov ecx, eax
        call snd_por_segundo
        xchg eax, ecx
        test ecx, ecx
        jz .cero
        div ecx                         ; EAX = los ms en cola
        mov ecx, SN_COLA_MS
        sub ecx, eax
        jbe .cero
        mov eax, ecx
        xor edx, edx
        mov ecx, SN_TROZO_MS
        div ecx
        cmp eax, 4
        jbe .si
        mov eax, 4
.si:
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.no:
        pop edx
        pop ecx
        pop ebx
        stc
        ret

; la campanita del volumen: el «clic» del sistema
snd_campana:
        pushad
        call snd_mira
        cmp eax, -1
        je .x
        mov ebx, SG_SISTEMA
        mov edx, 4                      ; (SS_VOLUMEN)
        PSIS PS_SONIDO
.x:
        popad
        ret

snd_canal       dd 0
snd_forma       dd 0
snd_probado     db 0
                align 4, db 0
snd_est         times 240 db 0          ; un Estado_sonido (sonido.cas)
