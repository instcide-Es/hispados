; =============================================================================
;  HispaBIOS (x64) - la BIOS de modo real y los cambios de modo
; =============================================================================
;  Se copia a la dirección física BASE (memoria baja pedida al firmware) y la
;  parte UEFI (efi64.asm) la llama una vez, con la tabla de traspaso rellena
;  (T_* abajo). Guarda el estado del firmware, baja al modo real y arranca
;  la BIOS. Cuando la BIOS necesita algo del firmware (dibujar la pantalla,
;  leer el disco o el teclado), sube al modo largo con el estado guardado,
;  llama al despachador de efi64.asm y vuelve a bajar.
;
;  La página 0 (vectores y datos de la BIOS) la usa también el firmware, así
;  que en cada cambio de modo se intercambia: el firmware tiene la suya y
;  DOS la suya. Se hace en 32 bits sin paginación (la página 0 puede no estar
;  en las tablas de páginas del firmware).
;
;  Referencias: Intel SDM vol. 3, 9.8 y 9.9; la lista de interrupciones de la
;  BIOS de Ralf Brown (comportamiento de cada función).
;
;  Ensamblado con org 0: en modo real el segmento es BASE >> 4; en 32 y 64
;  bits las direcciones absolutas llevan BASE sumado (macro L).
; =============================================================================
; BASE: por omisión 90000h; efi64.asm y efi32.asm llevan también copias para
; 70000h y 60000h (-DBASE=...), por si el firmware usa esa zona
%ifndef BASE
%define BASE 90000h
%endif
SEG_BIOS        equ BASE >> 4

SEL_C64         equ 08h
SEL_C32         equ 10h
SEL_D32         equ 18h
SEL_C16         equ 20h             ; código de 16 bits con base BASE
SEL_D16         equ 28h
SEL_DGR         equ 30h             ; datos de 16 bits con límite de 4 GB (modo «irreal»)
EFER            equ 0C0000080h

; zonas dentro de la BIOS (desplazamientos)
PILA_REAL       equ 7FF0h           ; pila de la BIOS en modo real
TX              equ 8000h           ; pantalla de texto 80x25 (4000 octetos)
EBDA            equ 8FA0h           ; «EBDA»: las entradas de ACPI (+10h) y de SMBIOS (+40h)
REG             equ 9000h           ; registro de la BIOS (texto: pantalla, VBE), REG_TAM octetos
REG_TAM         equ 0FF0h
PAG0_FW         equ 0A000h          ; la página 0 del firmware, guardada
PAG0_DOS        equ 0B000h          ; la página 0 de DOS, guardada
PILA32          equ 0EFF0h          ; pila de los tramos de 32 bits

; peticiones al firmware
PET_DIBUJAR     equ 1
PET_ESPERAR     equ 2
PET_MODO        equ 8               ; argumento: 0 = texto; 1... = modo VBE (bit 8: sin borrar)

%define L(x) (BASE + (x))

        org 0

; ---- tabla de traspaso (la rellena efi64.asm antes de llamar) ----
cabecera:
        db 0E9h                         ; +0 jmp near de 32 bits (se ejecuta
%ifdef ARQ32                            ;    en 32 o 64 bits; NASM aquí está en 16)
        dd entrada_32 - 5
%else
        dd entrada_64 - 5
%endif
        times 8 - ($ - cabecera) db 0
T_DESPACHADOR   dq 0                    ; +8  dirección del despachador (64 o 32 bits)
T_PETICION      dd 0                    ; +16 código de la petición
T_ARG           dd 0                    ; +20 argumento
T_RESULTADO     dd 0                    ; +24 resultado
T_MEMKB         dw 576                  ; +28 memoria convencional para DOS (KB)
T_NDISCOS       db 0                    ; +30 discos (el 0 es el de arranque, 80h)
                db 0
T_EXTKB         dd 0                    ; +32 KB seguidos desde 1 MB
T_TSC_TICK      dd 0                    ; +36 cuentas del TSC por tic (1/18,2 s)
T_DISCOS        times 8 * 16 db 0       ; +40 por disco: sectores (8), tamaño (4), -
P_DISCO         db 0                    ; +168 petición de disco: número (0 = 80h)
                db 0
P_N             dw 0                    ; +170 sectores (como mucho TROZO)
P_LBA           dq 0                    ; +172 primer sector
T_CURSOR        dw 0                    ; +180 posición del cursor (para dibujarlo)
T_VEC0          db 0                    ; +182 vector de la IRQ 0 del firmware (0: no usa el 8259)
                db 0
; ---- (0.3) la pantalla gráfica (VBE) y las cuentas para depurar la velocidad ----
T_VBE_LFB       dd 0                    ; +184 el marco lineal que se da a los programas
                                        ;      (memoria normal por debajo de 4 GB; 0: no hay VBE)
T_VBE_AN        dw 0                    ; +188 ancho y alto de la pantalla real: los modos
T_VBE_AL        dw 0                    ; +190 mayores no se ofrecen
T_GRAF          db 0                    ; +192 0: modo texto; 1..: modo VBE puesto (vbe_modos)
T_SIN_IRQ       db 0                    ; +193 1: la IRQ 0 no está llegando; los tics los da k_tics
T_REG_N         dw 0                    ; +194 octetos del registro de la BIOS (zona REG)
T_DIB_N         dd 0                    ; +196 veces que se ha dibujado la pantalla
T_DIB_ULT       dd 0                    ; +200 microsegundos del último dibujo
T_DIB_MAX       dd 0                    ; +204 ... del más lento
T_N_IRQ         dd 0                    ; +208 INT 08h recibidas
T_N_SIN         dd 0                    ; +212 tics dados sin IRQ (al sondear el teclado)
T_N_TECLA       dd 0                    ; +216 veces que se ha preguntado al firmware por el teclado
T_N_FW          dd 0                    ; +220 viajes al firmware
T_DIB_TOT       dd 0                    ; +224 microsegundos dibujando, en total
T_TABLAS        db 0                    ; +228 bit 0: RSDP de ACPI en EBDA + 10h; 1: SMBIOS en
                                        ;      EBDA + 40h; 2: copiadas también en F0000h
T_INT_N         db 0                    ; +229 páginas del firmware prestadas a DOS
                dw 0
T_INT_GUARDA    dd 0                    ; +232 dónde está la otra versión de cada una
T_INT_DIR       dd 0, 0, 0, 0           ; +236 sus direcciones

%ifdef ARQ32
; =============================================================================
;  Entrada desde el firmware IA32 (modo protegido de 32 bits, plano)
; =============================================================================
;  Con ARQ32 (BIOS32.BIN, para efi32.asm) el firmware vive en 32 bits: no hay
;  modo largo ni EFER. Se baja quitando la paginación (si el firmware la usa,
;  la memoria está mapeada uno a uno, como pide UEFI) y se sube devolviéndole
;  CR4, CR3 y CR0, su GDT, su IDT y sus segmentos.
        bits 32
entrada_32:
        push ebx
        push ebp
        push esi
        push edi
        pushfd
        cli
        mov [L(sv_rsp)], esp
        sgdt [L(sv_gdt)]
        sidt [L(sv_idt)]
        mov eax, cr0
        mov [L(sv_cr0)], eax
        mov eax, cr3
        mov [L(sv_cr3)], eax
        mov eax, cr4
        mov [L(sv_cr4)], eax
        mov [L(sv_ds)], ds
        mov [L(sv_es)], es
        mov [L(sv_ss)], ss
        mov [L(sv_fs)], fs
        mov [L(sv_gs)], gs
        mov [L(sv_cs)], cs
        mov byte [L(primera)], 1

; ---- bajar: modo protegido del firmware -> real ----
baja_fw:
        lgdt [L(gdtr_real)]
        jmp SEL_C32:L(baja_32)

baja_32:
        mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov esp, L(PILA32)              ; pila propia: la de DOS o la del firmware
                                        ; no valen aquí (otra base, o > 4 GB)
        mov eax, cr0
        and eax, 7FFFFFFFh              ; sin paginación
        mov cr0, eax
%else
; =============================================================================
;  Entrada desde el firmware (modo largo)
; =============================================================================
        bits 64
entrada_64:
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        pushfq
        cli
        mov [rel sv_rsp], rsp
        sgdt [rel sv_gdt]
        sidt [rel sv_idt]
        mov rax, cr0
        mov [rel sv_cr0], rax
        mov rax, cr3
        mov [rel sv_cr3], rax
        mov rax, cr4
        mov [rel sv_cr4], rax
        mov ecx, EFER
        rdmsr
        mov [rel sv_efer], eax
        mov [rel sv_efer + 4], edx
        mov [rel sv_ds], ds
        mov [rel sv_es], es
        mov [rel sv_ss], ss
        mov [rel sv_fs], fs
        mov [rel sv_gs], gs
        mov [rel sv_cs], cs
        mov byte [rel primera], 1
        jmp baja_64

; ---- bajar: modo largo -> real (y la página 0 de DOS en su sitio) ----
baja_64:
        lgdt [rel gdtr]
        push SEL_C32
        mov rax, L(baja_32)
        push rax
        retfq

        bits 32
baja_32:
        mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov esp, L(PILA32)              ; pila propia: la de DOS o la del firmware
                                        ; no valen aquí (otra base, o > 4 GB)
        mov eax, cr0
        and eax, 7FFFFFFFh              ; sin paginación: fuera del modo largo
        mov cr0, eax
        mov ecx, EFER
        rdmsr
        and eax, ~100h
        wrmsr
%endif
        cld                             ; la página 0: la del firmware se guarda
        xor esi, esi                    ; y se pone la de DOS
        mov edi, L(PAG0_FW)
        mov ecx, 1024
        rep movsd
        mov esi, L(PAG0_DOS)
        xor edi, edi
        mov ecx, 1024
        rep movsd
        call paginas_cambiar
        call reloj_dos
        jmp SEL_C16:baja_16

        bits 16
baja_16:
        mov ax, SEL_D16
        mov ss, ax
        ; Los segmentos de datos vuelven al modo real con límite de 4 GB, no de
        ; 64 KB: así el modo «irreal» que tuviera puesto el programa (FS o GS
        ; para llegar por encima de 1 MB: HSPRED con las tarjetas Intel, los
        ; juegos) sobrevive al viaje al firmware. Antes, cada vuelta (el tic
        ; del reloj que dibuja la pantalla, el disco, el teclado) lo dejaba en
        ; 64 KB y la lectura siguiente con FS era un fallo de protección: el
        ; equipo se reiniciaba (al cargar HSPRED, si el tic caía en mal sitio).
        ; Un límite de más no molesta a quien no lo usa.
        mov ax, SEL_DGR
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov eax, cr0
        and eax, ~1
        mov cr0, eax
        jmp SEG_BIOS:en_real

en_real:
        mov ax, SEG_BIOS
        mov ds, ax
        lidt [idtr_real]
        cmp byte [primera], 0
        je .vuelta
        mov byte [primera], 0
        mov ss, ax
        mov sp, PILA_REAL
        jmp bios_inicio
.vuelta:                                ; vuelta de una llamada al firmware
        lss sp, [rm_pila]
        ret                             ; a llamar_fw

; =============================================================================
;  llamar_fw: desde modo real, hace la petición AX (argumento EBX) al
;  firmware y vuelve con el resultado en EAX. Conserva el resto.
; =============================================================================
llamar_fw:
        pushf
        push ds
        push es
        push fs
        push gs
        pushad
        cli
        xor dx, dx                      ; el cursor, para dibujarlo
        mov ds, dx
        mov dx, [450h]
        mov cx, SEG_BIOS
        mov ds, cx
        mov [T_CURSOR], dx
        mov byte [en_fw], 1
        inc dword [T_N_FW]
        movzx eax, ax
        mov [T_PETICION], eax
        mov [T_ARG], ebx
        call .sube                      ; vuelve aquí al bajar otra vez
        mov byte [cs:en_fw], 0
        popad
        mov eax, [cs:T_RESULTADO]
        pop gs
        pop fs
        pop es
        pop ds
        popf
        ret
.sube:
        mov [rm_pila], sp
        mov [rm_pila + 2], ss
        o32 lgdt [gdtr_real]            ; (con la base de la GDT ya física)
        mov eax, cr0
        or eax, 1
        mov cr0, eax
        jmp dword SEL_C32:L(sube_32)

        bits 32
sube_32:
        mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov esp, L(PILA32)              ; pila propia: la de DOS o la del firmware
                                        ; no valen aquí (otra base, o > 4 GB)
        call reloj_fw
        call paginas_cambiar
        cld                             ; la página 0: la de DOS se guarda y
        xor esi, esi                    ; vuelve la del firmware
        mov edi, L(PAG0_DOS)
        mov ecx, 1024
        rep movsd
        mov esi, L(PAG0_FW)
        xor edi, edi
        mov ecx, 1024
        rep movsd
        mov eax, [L(sv_cr4)]
        mov cr4, eax
        mov eax, [L(sv_cr3)]
        mov cr3, eax
%ifdef ARQ32
        mov eax, [L(sv_cr0)]
        mov cr0, eax                    ; la paginación del firmware, si la usa
        jmp .cr0
.cr0:   lgdt [L(sv_gdt)]
        lidt [L(sv_idt)]
        mov ax, [L(sv_ds)]
        mov ds, ax
        mov ax, [L(sv_es)]
        mov es, ax
        mov ax, [L(sv_ss)]
        mov ss, ax
        mov ax, [L(sv_fs)]
        mov fs, ax
        mov ax, [L(sv_gs)]
        mov gs, ax
        mov esp, [L(sv_rsp)]
        and esp, -16
        sub esp, 64
        movzx eax, word [L(sv_cs)]      ; al selector de código del firmware
        push eax
        push dword L(.fw)
        retf
.fw:
        mov ecx, [L(T_PETICION)]
        mov edx, [L(T_ARG)]
        call [L(T_DESPACHADOR)]         ; (cierra las interrupciones al volver)
        cli
        mov [L(T_RESULTADO)], eax
        jmp baja_fw
%else
        mov ecx, EFER
        mov eax, [L(sv_efer)]
        mov edx, [L(sv_efer) + 4]
        wrmsr
        mov eax, [L(sv_cr0)]
        mov cr0, eax                    ; paginación: modo largo (compatible)
        jmp SEL_C64:L(sube_64)

        bits 64
sube_64:
        lgdt [rel sv_gdt]
        lidt [rel sv_idt]
        mov ax, [rel sv_ds]
        mov ds, ax
        mov ax, [rel sv_es]
        mov es, ax
        mov ax, [rel sv_ss]
        mov ss, ax
        mov ax, [rel sv_fs]
        mov fs, ax
        mov ax, [rel sv_gs]
        mov gs, ax
        mov rsp, [rel sv_rsp]
        and rsp, -16
        sub rsp, 64
        movzx eax, word [rel sv_cs]     ; al selector de código del firmware
        push rax
        lea rax, [rel .fw]
        push rax
        retfq
.fw:
        mov ecx, [rel T_PETICION]
        mov edx, [rel T_ARG]
        call [rel T_DESPACHADOR]        ; (cierra las interrupciones al volver)
        cli
        mov [rel T_RESULTADO], eax
        jmp baja_64
%endif


; =============================================================================
;  Las interrupciones al cambiar de modo (32 bits, sin paginación)
; =============================================================================
;  En modo DOS manda el temporizador clásico: PIT a 18,2 Hz por el 8259
;  (vectores 08h y 70h), que llega al procesador por LINT0 del APIC local
;  como ExtINT (el cableado virtual de siempre). El temporizador del APIC
;  local del firmware se enmascara y el TPR se pone a F0h, para que no llegue
;  ninguna otra interrupción del firmware al modo real (ExtINT no depende del
;  TPR). Al volver al firmware se calla el 8259 y todo queda como estaba.
;  (Intel SDM vol. 3, 11.5 y 11.8.3.1; 8259A: hoja de datos de Intel.)
        bits 32
; paginas_cambiar: las páginas de memoria baja que el firmware usa y se prestan
; a DOS (T_INT_*: efi64.asm, paginas_prestar) cambian de contenido: al bajar
; queda lo de DOS y se guarda lo del firmware; al subir, al revés
paginas_cambiar:
        pushad
        movzx ecx, byte [L(T_INT_N)]
        jecxz .f
        mov ebx, L(T_INT_DIR)
        mov edi, [L(T_INT_GUARDA)]
.p:     mov esi, [ebx]
        mov edx, 1024
.x:     mov eax, [esi]
        xchg eax, [edi]
        mov [esi], eax
        add esi, 4
        add edi, 4
        dec edx
        jnz .x
        add ebx, 4
        loop .p
.f:     popad
        ret

reloj_dos:
        push eax
        push ebx
        push ecx
        push edx
        cmp byte [L(T_VEC0)], 0         ; ¿el firmware usa el 8259?
        je .apic
        in al, 21h                      ; sí: se guardan sus máscaras y se
        mov [L(mask_fw)], al            ; ponen los vectores de DOS
        in al, 0A1h
        mov [L(mask_fw + 1)], al
        mov bl, 08h
        call pic_vectores
.apic:  mov al, [L(mask_dos)]           ; las máscaras de DOS
        out 21h, al
        mov al, [L(mask_dos + 1)]
        out 0A1h, al
        mov ecx, 1Bh                    ; IA32_APIC_BASE
        rdmsr
        test eax, 800h                  ; ¿APIC activado?
        jz .fin
        test eax, 400h                  ; ¿x2APIC?
        jnz .x2
        and eax, 0FFFFF000h
        mov ebx, eax
        mov eax, [ebx + 80h]            ; TPR
        mov [L(sv_tpr)], eax
        mov dword [ebx + 80h], 0F0h
        mov eax, [ebx + 320h]           ; temporizador: enmascarado
        mov [L(sv_lvtt)], eax
        or eax, 10000h
        mov [ebx + 320h], eax
        mov eax, [ebx + 350h]           ; LINT0: ExtINT
        mov [L(sv_lint0)], eax
        mov dword [ebx + 350h], 700h
        jmp .fin
.x2:    mov ecx, 808h
        rdmsr
        mov [L(sv_tpr)], eax
        mov eax, 0F0h
        xor edx, edx
        wrmsr
        mov ecx, 832h
        rdmsr
        mov [L(sv_lvtt)], eax
        or eax, 10000h
        xor edx, edx
        wrmsr
        mov ecx, 835h
        rdmsr
        mov [L(sv_lint0)], eax
        mov eax, 700h
        xor edx, edx
        wrmsr
.fin:   pop edx
        pop ecx
        pop ebx
        pop eax
        ret

reloj_fw:
        push eax
        push ebx
        push ecx
        push edx
        in al, 21h                      ; las máscaras de DOS se guardan y
        mov [L(mask_dos)], al           ; el 8259 se calla
        in al, 0A1h
        mov [L(mask_dos + 1)], al
        mov al, 0FFh
        out 21h, al
        out 0A1h, al
        mov ecx, 1Bh
        rdmsr
        test eax, 800h
        jz .pic
        test eax, 400h
        jnz .x2
        and eax, 0FFFFF000h
        mov ebx, eax
        call lint0_fw
        mov [ebx + 350h], eax
        mov eax, [L(sv_lvtt)]
        mov [ebx + 320h], eax
        mov eax, [L(sv_tpr)]
        mov [ebx + 80h], eax
        jmp .pic
.x2:    xor edx, edx
        mov ecx, 835h
        call lint0_fw
        wrmsr
        mov ecx, 832h
        mov eax, [L(sv_lvtt)]
        wrmsr
        mov ecx, 808h
        mov eax, [L(sv_tpr)]
        wrmsr
.pic:   cmp byte [L(T_VEC0)], 0         ; si el firmware usa el 8259: sus
        je .fin                         ; vectores y sus máscaras
        mov bl, [L(T_VEC0)]
        call pic_vectores
        mov al, [L(mask_fw)]
        out 21h, al
        mov al, [L(mask_fw + 1)]
        out 0A1h, al
.fin:   pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; lint0_fw -> EAX = LINT0 para el firmware: el suyo; si no usa el 8259, además
; enmascarado, para que una IRQ del 8259 que estuviera a punto de llegar (o
; una falsa, la 7) no le entre por un vector suyo de excepción y lo cuelgue
lint0_fw:
        mov eax, [L(sv_lint0)]
        cmp byte [L(T_VEC0)], 0
        jne .r
        or eax, 10000h
.r:     ret

; pic_vectores: BL = vector de la IRQ 0 (el esclavo, a BL + 8); todo enmascarado
pic_vectores:
        mov al, 11h                     ; ICW1: flanco, cascada, con ICW4
        out 20h, al
        out 0A0h, al
        mov al, bl                      ; ICW2
        out 21h, al
        add al, 8
        cmp bl, 08h                     ; (DOS: el esclavo en 70h)
        jne .e
        mov al, 70h
.e:     out 0A1h, al
        mov al, 4                       ; ICW3
        out 21h, al
        mov al, 2
        out 0A1h, al
        mov al, 1                       ; ICW4: 8086
        out 21h, al
        out 0A1h, al
        mov al, 0FFh
        out 21h, al
        out 0A1h, al
        ret
        bits 16

; =============================================================================
;  La BIOS de modo real
; =============================================================================
        bits 16
bios_inicio:
        ; ---- vectores: todos a un IRET; los nuestros, a su rutina ----
        xor ax, ax
        mov es, ax
        xor di, di
        mov cx, 256
.v:     mov ax, iret_nada
        stosw
        mov ax, SEG_BIOS
        stosw
        loop .v
        mov word [es:08h * 4], int08
        mov word [es:10h * 4], int10
        mov word [es:11h * 4], int11
        mov word [es:12h * 4], int12
        mov word [es:13h * 4], int13
        mov word [es:14h * 4], int14
        mov word [es:15h * 4], int15
        mov word [es:16h * 4], int16
        mov word [es:17h * 4], int17
        mov word [es:18h * 4], int18
        mov word [es:19h * 4], int19
        mov word [es:1Ah * 4], int1a
        mov al, 'V'                     ; [depuración] el vector, por COM1
        call dbg
        mov al, [T_VEC0]
        call dbg_hex
        mov di, 70h * 4                 ; IRQ 8-15
        mov cx, 8
.e:     mov word [es:di], irq_esclavo
        add di, 4
        loop .e
        ; ---- el reloj de DOS: 8259 en 08h/70h y PIT a 18,2 Hz ----
        mov al, 11h
        out 20h, al
        out 0A0h, al
        mov al, 08h
        out 21h, al
        mov al, 70h
        out 0A1h, al
        mov al, 4
        out 21h, al
        mov al, 2
        out 0A1h, al
        mov al, 1
        out 21h, al
        out 0A1h, al
        mov al, [mask_dos]
        out 21h, al
        mov al, [mask_dos + 1]
        out 0A1h, al
        mov al, 36h                     ; canal 0, modo 3, cuenta 65536
        out 43h, al
        xor al, al
        out 40h, al
        out 40h, al
        ; ---- el reloj: el TSC de ahora es el tic 0 ----
        rdtsc
        mov [tsc0], eax
        mov [tsc0 + 4], edx
        ; ---- el mapa de memoria (E820): DOS, la BIOS y la extendida ----
        movzx eax, word [T_MEMKB]
        shl eax, 10
        mov dword [e820 + 8], eax       ; 0 - tope de DOS: libre
        mov dword [e820 + 16], 1
        mov [e820 + 20], eax            ; tope de DOS - 1 MB: reservado
        mov ecx, 100000h
        sub ecx, eax
        mov [e820 + 28], ecx
        mov dword [e820 + 36], 2
        mov dword [e820 + 40], 100000h  ; 1 MB + extendida: libre
        mov eax, [T_EXTKB]
        shl eax, 10
        mov [e820 + 48], eax
        mov dword [e820 + 56], 1
        xor ax, ax
        mov es, ax
        ; ---- datos de la BIOS (40:xx) ----
        mov ax, 40h
        mov es, ax
        mov ax, [T_MEMKB]
        mov [es:13h], ax                ; memoria convencional
        mov byte [es:49h], 3            ; modo de vídeo
        mov word [es:4Ah], 80           ; columnas
        mov word [es:4Ch], 4000         ; tamaño de la página
        mov word [es:4Eh], 0
        mov word [es:50h], 0            ; cursor de la página 0
        mov byte [es:62h], 0
        mov word [es:63h], 3D4h
        mov byte [es:84h], 24           ; filas - 1
        mov word [es:85h], 16           ; alto del carácter
        ; ---- las tablas del equipo (ACPI y SMBIOS) a la vista: 40:0Eh apunta a
        ;      la «EBDA» y, si F0000h es memoria libre que se puede escribir
        ;      (lo que haría un CSM), también allí ----
        test byte [T_TABLAS], 3
        jz .sin_tablas
        mov word [EBDA], 1              ; (la EBDA dice su tamaño: 1 KB)
        mov word [es:0Eh], SEG_BIOS + (EBDA >> 4)
        push es
        mov ax, 0F000h
        mov es, ax
        xor di, di
        mov al, [es:di]
        cmp al, 0
        je .f_igual
        cmp al, 0FFh
        jne .f_no
.f_igual:
        mov cx, 60h                     ; ¿libre? (todo a 00 o a FF)
        cld
        repe scasb
        jne .f_no
        mov byte [es:0], 5Ah            ; ¿se puede escribir?
        mov byte [es:1], 0A5h           ; (otro valor en el bus, por si no hay nada)
        cmp byte [es:0], 5Ah
        jne .f_no
        xor di, di
        mov si, EBDA + 40h              ; SMBIOS en F000:0000, RSDP en F000:0020
        mov cx, 20h
        rep movsb
        mov si, EBDA + 10h
        mov cx, 30h
        rep movsb
        or byte [T_TABLAS], 4
        mov si, t_r_f_si
        jmp .f_ya
.f_no:  mov si, t_r_f_no
.f_ya:  call reg_txt
        pop es
.sin_tablas:
        ; ---- los modos VBE que caben en la pantalla ----
        push es
        push ds
        pop es
        mov si, vbe_modos
        mov di, vbe_lista
.vm:    mov cx, [si]
        test cx, cx
        jz .vf
        push si
        call vbe_buscar
        pop si
        jc .vs
        mov ax, cx
        stosw
.vs:    add si, 6
        jmp .vm
.vf:    pop es
        ; ---- pantalla limpia ----
        mov ax, 0003h
        int 10h
        mov word [es:1Ah], 1Eh          ; búfer del teclado vacío
        mov word [es:1Ch], 1Eh
        mov dword [es:6Ch], 0           ; tics
        ; ---- letrero y a arrancar el disco ----
        mov si, t_prueba
        call escribir_bios
        mov ax, PET_DIBUJAR
        call llamar_fw
        sti
        int 19h
.para:  cli
        hlt
        jmp .para

; escribir_bios: DS:SI = texto terminado en 0, por INT 10h 0Eh
escribir_bios:
        push ax
        push bx
.c:     lodsb
        test al, al
        jz .f
        mov ah, 0Eh
        mov bx, 0007h
        int 10h
        jmp .c
.f:     pop bx
        pop ax
        ret

iret_nada:
        iret

; =============================================================================
;  INT 10h - vídeo (modo texto 80x25 sobre la pantalla TX)
; =============================================================================
int10:
        sti
        call k_tics                     ; (por si la IRQ 0 no llega)
        cmp ah, 00h
        je v_modo
        cmp ah, 01h
        je v_nada
        cmp ah, 02h
        je v_poner_cursor
        cmp ah, 03h
        je v_leer_cursor
        cmp ah, 05h
        je v_nada
        cmp ah, 06h
        je v_subir
        cmp ah, 07h
        je v_bajar
        cmp ah, 08h
        je v_leer_car
        cmp ah, 09h
        je v_car_atr
        cmp ah, 0Ah
        je v_car
        cmp ah, 0Eh
        je v_teletipo
        cmp ah, 0Fh
        je v_leer_modo
        cmp ah, 12h
        je v_info
        cmp ah, 13h
        je v_cadena
        cmp ah, 1Ah
        je v_combinacion
        cmp ah, 4Fh
        je v_vbe
        cmp ah, 0FEh
        je v_pantalla
v_nada:
        iret

v_modo:                                 ; 00h: cualquier modo -> texto 80x25
        push ax
        push cx
        push di
        push es
        push ax                         ; un modo que no hay (gráfico de VGA...):
        and al, 7Fh                     ; al registro, y queda el de texto
        cmp al, 3
        jbe .hay
        cmp al, 7
        je .hay
        push si
        mov si, t_r_modo
        call reg_txt
        xor ah, ah
        call reg_hex8
        mov si, t_r_modo2
        call reg_txt
        pop si
.hay:   pop ax
        call v_a_texto
        mov ax, SEG_BIOS + (TX >> 4)
        mov es, ax
        xor di, di
        mov ax, 0720h
        mov cx, 80 * 25
        cld
        rep stosw
        mov ax, 40h
        mov es, ax
        mov byte [es:49h], 3
        mov word [es:50h], 0
        call pantalla_cambiada
        pop es
        pop di
        pop cx
        pop ax
        iret

v_poner_cursor:                         ; 02h: DH = fila, DL = columna
        push es
        push ax
        mov ax, 40h
        mov es, ax
        mov [es:50h], dx
        call pantalla_cambiada
        pop ax
        pop es
        iret

v_leer_cursor:                          ; 03h -> DX = posición, CX = forma
        push es
        push ax
        mov ax, 40h
        mov es, ax
        mov dx, [es:50h]
        mov cx, 0607h
        pop ax
        pop es
        iret

v_leer_modo:                            ; 0Fh -> AL = modo, AH = columnas, BH = página
        mov ax, 5003h
        mov bh, 0
        iret

v_info:                                 ; 12h BL=10h: información EGA/VGA
        cmp bl, 10h
        jne v_nada
        mov bx, 0003h                   ; color, 256 KB
        mov cx, 0009h
        iret

v_combinacion:                          ; 1Ah: VGA con color
        mov al, 1Ah
        mov bx, 0008h
        iret

v_pantalla:                             ; FEh (como DESQview): ES:DI = pantalla real
        mov di, SEG_BIOS + (TX >> 4)
        mov es, di
        xor di, di
        iret

; dirección de la celda (DH, DL) -> DI (en ES = pantalla)
celda:
        push ax
        mov al, dh
        mov ah, 80
        mul ah
        add al, dl
        adc ah, 0
        shl ax, 1
        mov di, ax
        pop ax
        ret

v_leer_car:                             ; 08h -> AL = carácter, AH = atributo
        push es
        push di
        push dx
        mov dx, 40h
        mov es, dx
        mov dx, [es:50h]
        mov di, SEG_BIOS + (TX >> 4)
        mov es, di
        call celda
        mov ax, [es:di]
        pop dx
        pop di
        pop es
        iret

v_car_atr:                              ; 09h: AL carácter, BL atributo, CX veces
        push ax
        mov ah, bl
        jmp v_escribir_n
v_car:                                  ; 0Ah: AL carácter, CX veces (sin atributo)
        push ax
        mov ah, 0FFh                    ; (marca: conservar el atributo)
v_escribir_n:
        push cx
        push dx
        push di
        push es
        push bx
        mov bx, 40h
        mov es, bx
        mov dx, [es:50h]
        mov bx, SEG_BIOS + (TX >> 4)
        mov es, bx
        call celda
        jcxz .f
.l:     cmp di, 4000
        jae .f
        cmp ah, 0FFh
        je .solo
        mov [es:di], ax
        jmp .s
.solo:  mov [es:di], al
.s:     add di, 2
        loop .l
.f:     call pantalla_cambiada
        pop bx
        pop es
        pop di
        pop dx
        pop cx
        pop ax
        iret

v_teletipo:                             ; 0Eh: AL carácter (con CR, LF, BS, BEL)
        push ax
        push bx
        push dx
        push di
        push es
        mov bx, 40h
        mov es, bx
        mov dx, [es:50h]
        cmp al, 13
        je .cr
        cmp al, 10
        je .lf
        cmp al, 8
        je .bs
        cmp al, 7
        je .fin
        mov bx, SEG_BIOS + (TX >> 4)
        mov es, bx
        call celda
        mov [es:di], al                 ; (conserva el atributo de la celda)
        inc dl
        cmp dl, 80
        jb .pon
        xor dl, dl
.lf:    inc dh
        cmp dh, 25
        jb .pon
        mov dh, 24
        call subir_una                  ; desplaza la pantalla una línea
        jmp .pon
.cr:    xor dl, dl
        jmp .pon
.bs:    test dl, dl
        jz .pon
        dec dl
.pon:   mov bx, 40h
        mov es, bx
        mov [es:50h], dx
        call pantalla_cambiada
.fin:   pop es
        pop di
        pop dx
        pop bx
        pop ax
        iret

; sube toda la pantalla una línea y deja la última en gris (07h)
subir_una:
        push ax
        push cx
        push si
        push di
        push ds
        push es
        mov ax, SEG_BIOS + (TX >> 4)
        mov ds, ax
        mov es, ax
        mov si, 160
        xor di, di
        mov cx, 80 * 24
        cld
        rep movsw
        mov ax, 0720h
        mov cx, 80
        rep stosw
        pop es
        pop ds
        pop di
        pop si
        pop cx
        pop ax
        ret

v_subir:                                ; 06h: AL líneas (0 = borrar), BH atributo,
v_bajar:                                ; 07h  CH,CL - DH,DL la ventana
        pusha
        push ds
        push es
        mov [cs:v_ini], cx
        mov [cs:v_fin], dx
        mov [cs:v_atr], bh
        mov si, SEG_BIOS + (TX >> 4)
        mov ds, si
        mov es, si
        cld
        cmp byte [cs:v_fin], 79
        jbe .c1
        mov byte [cs:v_fin], 79
.c1:    cmp byte [cs:v_fin + 1], 24
        jbe .c2
        mov byte [cs:v_fin + 1], 24
.c2:    mov bl, [cs:v_fin + 1]
        sub bl, [cs:v_ini + 1]
        inc bl                          ; BL = alto
        mov bh, [cs:v_fin]
        sub bh, [cs:v_ini]
        inc bh                          ; BH = ancho
        test al, al
        jz .todo
        cmp al, bl
        jbe .n
.todo:  mov al, bl
.n:     mov dl, al                      ; DL = líneas
        cmp ah, 07h
        je .abajo
        mov dh, [cs:v_ini + 1]          ; hacia arriba
.ca:    mov al, [cs:v_fin + 1]
        sub al, dl
        jb .rell_a
        cmp dh, al
        ja .rell_a
        mov al, dh
        call off_fila
        mov si, di
        mov al, dl
        mov ah, 160
        mul ah
        add si, ax
        movzx cx, bh
        rep movsw
        inc dh
        jmp .ca
.rell_a:
        mov dh, [cs:v_fin + 1]
        sub dh, dl
        inc dh
.ra:    cmp dh, [cs:v_fin + 1]
        ja .fin
        call rellenar_fila
        inc dh
        jmp .ra
.abajo:
        mov dh, [cs:v_fin + 1]
.cb:    mov al, [cs:v_ini + 1]
        add al, dl
        cmp dh, al
        jb .rell_b
        mov al, dh
        call off_fila
        mov si, di
        mov al, dl
        mov ah, 160
        mul ah
        sub si, ax
        movzx cx, bh
        rep movsw
        dec dh
        jmp .cb
.rell_b:
        mov dh, [cs:v_ini + 1]
.rb:    mov al, [cs:v_ini + 1]
        add al, dl
        cmp dh, al
        jae .fin
        call rellenar_fila
        inc dh
        jmp .rb
.fin:   call pantalla_cambiada
        pop es
        pop ds
        popa
        iret

; off_fila: AL = fila -> DI = (fila * 80 + columna inicial) * 2
off_fila:
        push ax
        mov ah, 80
        mul ah
        add al, [cs:v_ini]
        adc ah, 0
        shl ax, 1
        mov di, ax
        pop ax
        ret
; rellenar_fila: la fila DH de la ventana, con espacios y el atributo v_atr
rellenar_fila:
        push ax
        push cx
        mov al, dh
        call off_fila
        movzx cx, bh
        mov ah, [cs:v_atr]
        mov al, ' '
        rep stosw
        pop cx
        pop ax
        ret

v_cadena:                               ; 13h: AL modo, BL atributo, CX largo,
        pusha                           ;      DH,DL posición, ES:BP cadena
        push ds
        mov [cs:v_cmodo], al
        mov si, 40h
        mov ds, si
        push word [50h]                 ; la posición de antes
        mov [50h], dx
        mov si, bp
        jcxz .fin
.c:     mov al, [es:si]
        inc si
        test byte [cs:v_cmodo], 2        ; modos 2 y 3: carácter y atributo
        jz .e
        inc si
.e:     mov ah, 0Eh
        int 10h
        loop .c
.fin:   pop ax
        test byte [cs:v_cmodo], 1        ; modos 1 y 3: el cursor se queda al final
        jnz .deja
        mov [50h], ax
.deja:  call pantalla_cambiada
        pop ds
        popa
        iret

; v_a_texto: si había un modo gráfico (VBE), se deja y vuelve la pantalla de texto
v_a_texto:
        cmp byte [cs:T_GRAF], 0
        je .f
        push eax
        push ebx
        mov byte [cs:T_GRAF], 0
        xor ebx, ebx
        mov ax, PET_MODO
        call llamar_fw
        pop ebx
        pop eax
.f:     ret

; =============================================================================
;  VBE 2.0 (INT 10h AH = 4Fh) sobre la pantalla del firmware
; =============================================================================
;  Los equipos que solo tienen UEFI no llevan VBE ni VGA, y su pantalla puede
;  estar por encima de los 4 GB, donde un programa de 32 bits no llega. Se
;  ofrecen modos de color directo de 32 bits con marco lineal (640x480,
;  800x600 y 1024x768, los que quepan en la pantalla): el marco es memoria
;  normal por debajo de 4 GB (T_VBE_LFB, la pide efi64.asm) y en cada tic la
;  parte UEFI lleva a la pantalla de verdad las líneas que han cambiado.
;  No hay modos de 8 bits (su paleta se pone por los puertos de la VGA, que
;  aquí no existen) ni ventanas de 64 KB en A0000h: solo el marco lineal.
;  Cada llamada se apunta en el registro de la BIOS (zona REG), con lo que se
;  contesta. Referencia: especificación VBE 2.0/3.0 de VESA.
v_vbe:
        push ds
        push si
        push ax
        mov si, SEG_BIOS
        mov ds, si
        cmp byte [vbe_reg_n], 80        ; (como mucho 80 llamadas en el registro)
        jae .sinreg
        mov si, t_r_vbe
        call reg_txt
        call reg_hex16
        mov si, t_r_bx
        call reg_txt
        mov ax, bx
        call reg_hex16
        mov si, t_r_cx
        call reg_txt
        mov ax, cx
        call reg_hex16
.sinreg:
        pop ax
        call vbe_hacer
        cmp byte [vbe_reg_n], 80
        jae .f
        inc byte [vbe_reg_n]
        mov si, t_r_res
        call reg_txt
        call reg_hex16
        mov si, t_r_nl
        call reg_txt
.f:     pop si
        pop ds
        iret

; vbe_hacer: la función AL (DS = BIOS) -> AX = 004Fh bien, 014Fh mal, 0100h no la hay
vbe_hacer:
        cmp dword [T_VBE_LFB], 0
        je .no
        cmp al, 00h
        je vbe_info
        cmp al, 01h
        je vbe_modo_info
        cmp al, 02h
        je vbe_poner
        cmp al, 03h
        je vbe_actual
.no:    mov ax, 0100h
        ret

vbe_info:                               ; 4F00h: ES:DI = búfer -> la tarjeta
        push cx
        push di
        push di
        xor al, al
        mov cx, 22h
        cld
        rep stosb
        pop di
        mov dword [es:di], 'VESA'
        mov word [es:di + 4], 0200h
        mov word [es:di + 6], t_vbe_oem
        mov word [es:di + 8], SEG_BIOS
        mov dword [es:di + 0Ah], 1      ; (el DAC no cambia de anchura)
        mov word [es:di + 0Eh], vbe_lista
        mov word [es:di + 10h], SEG_BIOS
        mov word [es:di + 12h], VBE_MEM >> 16   ; memoria, en bloques de 64 KB
        mov word [es:di + 14h], 0003h
        mov word [es:di + 16h], t_vbe_oem
        mov word [es:di + 18h], SEG_BIOS
        mov word [es:di + 1Ah], t_vbe_oem
        mov word [es:di + 1Ch], SEG_BIOS
        mov word [es:di + 1Eh], t_vbe_oem
        mov word [es:di + 20h], SEG_BIOS
        pop di
        pop cx
        mov ax, 004Fh
        ret

; vbe_buscar: CX = número de modo -> SI = su entrada en vbe_modos, AL = índice
; (1...); CF si no lo hay o no cabe en la pantalla
vbe_buscar:
        push cx
        push dx
        and cx, 01FFh
        mov si, vbe_modos
        mov dl, 1
.m:     cmp word [si], 0
        je .no
        cmp [si], cx
        je .si
        add si, 6
        inc dl
        jmp .m
.si:    mov cx, [si + 2]
        cmp cx, [T_VBE_AN]
        ja .no
        mov cx, [si + 4]
        cmp cx, [T_VBE_AL]
        ja .no
        mov al, dl
        pop dx
        pop cx
        clc
        ret
.no:    pop dx
        pop cx
        stc
        ret

vbe_modo_info:                          ; 4F01h: CX = modo, ES:DI = búfer de 256
        push si
        call vbe_buscar
        jc .mal
        push cx
        push di
        xor al, al
        mov cx, 256
        cld
        rep stosb
        pop di
        pop cx
        mov word [es:di], 00DBh         ; hay, color, gráfico, sin ventanas, marco lineal
        mov ax, [si + 2]
        mov [es:di + 12h], ax
        shl ax, 2
        mov [es:di + 10h], ax           ; octetos por línea
        mov [es:di + 32h], ax
        mov ax, [si + 4]
        mov [es:di + 14h], ax
        mov word [es:di + 16h], 1008h   ; celda de 8x16
        mov word [es:di + 18h], 2001h   ; 1 plano, 32 bits por punto
        mov word [es:di + 1Ah], 0601h   ; 1 banco, color directo
        mov byte [es:di + 1Eh], 1
        mov dword [es:di + 1Fh], 08081008h      ; rojo 8 en 16, verde 8 en 8,
        mov dword [es:di + 23h], 18080008h      ; azul 8 en 0, reserva 8 en 24
        mov dword [es:di + 36h], 08081008h      ; (las mismas, «lineales» de VBE 3.0)
        mov dword [es:di + 3Ah], 18080008h
        mov eax, [T_VBE_LFB]
        mov [es:di + 28h], eax          ; el marco lineal
        pop si
        mov ax, 004Fh
        ret
.mal:   pop si
        mov ax, 014Fh
        ret

vbe_poner:                              ; 4F02h: BX = modo (bit 14: marco lineal; 15: sin borrar)
        push si
        push cx
        mov cx, bx
        and cx, 3FFFh
        cmp cx, 100h
        jb .texto
        call vbe_buscar
        jc .mal
        test bh, 40h                    ; solo con marco lineal
        jz .mal
        mov [T_GRAF], al
        push ebx
        push eax
        movzx eax, al
        and ebx, 8000h
        shr ebx, 7                      ; bit 8: sin borrar
        or ebx, eax
        mov ax, PET_MODO
        call llamar_fw
        pop eax
        pop ebx
        jmp .bien
.texto: mov ax, 0003h                   ; un modo de la VGA: el de texto
        int 10h
.bien:  pop cx
        pop si
        mov ax, 004Fh
        ret
.mal:   pop cx
        pop si
        mov ax, 014Fh
        ret

vbe_actual:                             ; 4F03h -> BX = modo puesto
        mov bx, 0003h
        cmp byte [T_GRAF], 0
        je .f
        push si
        movzx si, byte [T_GRAF]
        dec si
        imul si, si, 6
        mov bx, [vbe_modos + si]
        or bh, 40h
        pop si
.f:     mov ax, 004Fh
        ret

; ---- el registro de la BIOS (texto en la zona REG; lo lee HBLOG.COM por
;      INT 15h AX = 4844h) ----
; reg_txt: CS:SI = texto terminado en 0
reg_txt:
        push ax
        push bx
        push si
        mov bx, [cs:T_REG_N]
.c:     mov al, [cs:si]
        test al, al
        jz .f
        cmp bx, REG_TAM
        jae .f
        mov [cs:REG + bx], al
        inc bx
        inc si
        jmp .c
.f:     mov [cs:T_REG_N], bx
        pop si
        pop bx
        pop ax
        ret
; reg_hex16: AX en hexadecimal (4 cifras); reg_hex8: AL (2 cifras)
reg_hex16:
        xchg al, ah
        call reg_hex8
        xchg al, ah
reg_hex8:
        push ax
        push bx
        push cx
        mov cx, 2
        mov bx, [cs:T_REG_N]
        rol al, 4
.d:     cmp bx, REG_TAM
        jae .f
        push ax
        and al, 0Fh
        add al, '0'
        cmp al, '9'
        jbe .n
        add al, 'A' - '9' - 1
.n:     mov [cs:REG + bx], al
        inc bx
        pop ax
        rol al, 4
        loop .d
.f:     mov [cs:T_REG_N], bx
        pop cx
        pop bx
        pop ax
        ret

; la pantalla ha cambiado: se dibujará en la próxima llamada al firmware
pantalla_cambiada:
        push ds
        push ax
        mov ax, SEG_BIOS
        mov ds, ax
        mov byte [sucia], 1
        pop ax
        pop ds
        ret

; =============================================================================
;  Datos
; =============================================================================
t_prueba        db 'HispaBIOS 0.3 - BIOS de Hispa-DOS sobre UEFI. Arrancando el disco 80h...', 13, 10, 13, 10, 0
t_vbe_oem       db 'HispaBIOS (VBE sobre la pantalla del firmware UEFI)', 0
t_r_vbe         db 'INT 10h AX=', 0
t_r_bx          db ' BX=', 0
t_r_cx          db ' CX=', 0
t_r_res         db ' -> AX=', 0
t_r_nl          db 13, 10, 0
t_r_f_si        db 'Tablas (ACPI, SMBIOS): en la EBDA de la BIOS y copiadas tambien en F0000h', 13, 10, 0
t_r_f_no        db 'Tablas (ACPI, SMBIOS): solo en la EBDA de la BIOS (F0000h no es memoria libre que se pueda escribir)', 13, 10, 0
t_r_modo        db 'INT 10h AH=00 AL=', 0
t_r_modo2       db ': ese modo de la VGA no lo hay (queda el de texto)', 13, 10, 0
vbe_reg_n       db 0
        align 2
; los modos VBE: número, ancho, alto (color directo de 32 bits); acaba en 0
VBE_MEM         equ 300000h             ; 3 MB: 1024x768 de 32 bits
vbe_modos       dw 112h, 640, 480
                dw 115h, 800, 600
                dw 118h, 1024, 768
                dw 0
vbe_lista       dw 0FFFFh, 0FFFFh, 0FFFFh, 0FFFFh   ; (la rellena bios_inicio)
primera         db 0
mask_dos        db 0FAh, 0FFh          ; DOS: IRQ 0 y la cascada
mask_fw         db 0FFh, 0FFh
        align 4
sv_tpr          dd 0
sv_lvtt         dd 0
sv_lint0        dd 0
v_ini           dw 0
v_fin           dw 0
v_atr           db 0
v_cmodo         db 0
sucia           db 0
rm_pila         dw 0, 0

        align 16
gdt:
        dq 0
        dq 00AF9A000000FFFFh            ; 08h código de 64 bits
        dq 00CF9A000000FFFFh            ; 10h código de 32 bits
        dq 00CF92000000FFFFh            ; 18h datos de 32 bits
        dw 0FFFFh, BASE & 0FFFFh        ; 20h código de 16 bits, base BASE
        db (BASE >> 16) & 0FFh, 9Ah, 00h, BASE >> 24
        dw 0FFFFh, BASE & 0FFFFh        ; 28h datos de 16 bits, base BASE
        db (BASE >> 16) & 0FFh, 92h, 00h, BASE >> 24
        dq 008F92000000FFFFh            ; 30h datos de 16 bits, límite de 4 GB
gdt_fin:
gdtr:   dw gdt_fin - gdt - 1            ; (para lgdt en modo largo: base de 64 bits)
        dq L(gdt)
gdtr_real:
        dw gdt_fin - gdt - 1            ; (para lgdt desde modo real: base de 32)
        dd L(gdt)
idtr_real:
        dw 3FFh
        dd 0

        align 8
sv_rsp  dq 0
sv_cr0  dq 0
sv_cr3  dq 0
sv_cr4  dq 0
sv_efer dq 0
sv_gdt  dw 0
        dq 0
sv_idt  dw 0
        dq 0
sv_ds   dw 0
sv_es   dw 0
sv_ss   dw 0
sv_fs   dw 0
sv_gs   dw 0
sv_cs   dw 0

%include "bios_int.inc"

bios_fin:
        times (PILA_REAL - 400h) - (bios_fin - cabecera) db 0   ; (falla si no cabe)
