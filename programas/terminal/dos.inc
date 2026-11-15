; =============================================================================
;  TERMINAL.HVP - DOS: las llamadas que la interfaz de siempre no exporta a los
;  módulos (buscar archivos, crear y quitar carpetas, renombrar) hechas con
;  rm_interrup, con los nombres en dos_buf. Es el mismo patrón que usa el
;  Administrador de archivos (programas/archivos/dos.inc): las operaciones que
;  no están en la tabla de la interfaz se piden como una INT 21h de modo real.
;  Todas conservan los registros salvo EAX (y lo que devuelven); CF=1 y EAX=
;  el error de DOS si falla.
; =============================================================================
DB_R1       equ 0                   ; dentro de dos_buf: la primera ruta,
DB_R2       equ 128                 ;   la segunda
DB_DTA      equ 256                 ;   y la zona de transferencia (DTA)
DTA_TAM     equ 44
DTA_ATR     equ 0x15
DTA_HORA    equ 0x16
DTA_FECHA   equ 0x18
DTA_TAMANO  equ 0x1A
DTA_NOMBRE  equ 0x1E

; (interna) copia ESI (ruta) a dos_buf+EDI, hasta 127 letras
d_poner:
        push eax
        push ecx
        push esi
        push edi
        add edi, [dbuf]
        mov ecx, 127
.c:
        lodsb
        stosb
        test al, al
        jz .x
        loop .c
        mov byte [edi], 0
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; (interna) INT 21h con el bloque rl (EAX ya puesto en rl+RL_EAX...)
d_int21:
        push esi
        mov byte [rl+RL_INT], 0x21
        mov esi, rl
        HV rm_interrup
        mov eax, [rl+RL_EAX]
        pop esi
        ret

; ESI=nombre de antes EDI=el nuevo (los dos con su ruta): renombra (56h)
d_renombrar:
        push esi
        push edi
        push edi
        xor edi, edi
        call d_poner
        pop esi
        mov edi, DB_R2
        call d_poner
        mov dword [rl+RL_EAX], 0x5600
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        add eax, DB_R2
        mov [rl+RL_EDI], eax
        call d_int21
        pop edi
        pop esi
        ret

; ESI=ruta: crear (39h) o quitar (3Ah) una carpeta, borrar un archivo (41h)
d_crear_dir:
        mov eax, 0x3900
        jmp d_ruta1
d_quitar_dir:
        mov eax, 0x3A00
        jmp d_ruta1
d_borrar:
        mov eax, 0x4100
d_ruta1:
        push edi
        xor edi, edi
        call d_poner
        mov [rl+RL_EAX], eax
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        call d_int21
        pop edi
        ret

; ESI=patrón (con su ruta) ECX=atributos EDI=DTA propia (44 octetos): el
; primero; d_siguiente EDI=la misma DTA: el siguiente. La DTA de DOS se pone
; cada vez en dos_buf+DB_DTA y se copia a la propia.
d_primero:
        push ecx
        push esi
        push edi
        call d_dta_poner
        push edi
        xor edi, edi
        call d_poner
        pop edi
        mov dword [rl+RL_EAX], 0x4E00
        mov [rl+RL_ECX], ecx
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        call d_int21
        jmp d_dta_traer
d_siguiente:
        push ecx
        push esi
        push edi
        call d_dta_poner
        push esi                    ; la propia, a la de DOS
        push edi
        mov esi, edi
        mov edi, [dbuf]
        add edi, DB_DTA
        mov ecx, DTA_TAM / 4
        rep movsd
        pop edi
        pop esi
        mov dword [rl+RL_EAX], 0x4F00
        call d_int21
d_dta_traer:
        pushf                       ; la de DOS, a la propia
        push eax
        mov esi, [dbuf]
        add esi, DB_DTA
        mov ecx, DTA_TAM / 4
        rep movsd
        pop eax
        popf
        pop edi
        pop esi
        pop ecx
        ret
d_dta_poner:
        mov dword [rl+RL_EAX], 0x1A00
        mov eax, [dbuf]
        add eax, DB_DTA
        mov [rl+RL_EDX], eax
        jmp d_int21
