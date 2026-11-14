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
BASE            equ 90000h
SEG_BIOS        equ BASE >> 4

SEL_C64         equ 08h
SEL_C32         equ 10h
SEL_D32         equ 18h
SEL_C16         equ 20h             ; código de 16 bits con base BASE
SEL_D16         equ 28h
EFER            equ 0C0000080h

; zonas dentro de la BIOS (desplazamientos)
PILA_REAL       equ 7FF0h           ; pila de la BIOS en modo real
TX              equ 8000h           ; pantalla de texto 80x25 (4000 octetos)
PAG0_FW         equ 0A000h          ; la página 0 del firmware, guardada
PAG0_DOS        equ 0B000h          ; la página 0 de DOS, guardada

; peticiones al firmware
PET_DIBUJAR     equ 1
PET_ESPERAR     equ 2

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
        call reloj_dos
        jmp SEL_C16:baja_16

        bits 16
baja_16:
        mov ax, SEL_D16
        mov ds, ax
        mov es, ax
        mov ss, ax
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
        call reloj_fw
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
        mov eax, [L(sv_lint0)]
        mov [ebx + 350h], eax
        mov eax, [L(sv_lvtt)]
        mov [ebx + 320h], eax
        mov eax, [L(sv_tpr)]
        mov [ebx + 80h], eax
        jmp .pic
.x2:    xor edx, edx
        mov ecx, 835h
        mov eax, [L(sv_lint0)]
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
        cmp ah, 0FEh
        je v_pantalla
v_nada:
        iret

v_modo:                                 ; 00h: cualquier modo -> texto 80x25
        push ax
        push cx
        push di
        push es
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
t_prueba        db 'HispaBIOS 0.1 - BIOS de Hispa-DOS sobre UEFI. Arrancando el disco 80h...', 13, 10, 13, 10, 0
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
        dq 00009A090000FFFFh            ; 20h código de 16 bits, base 90000h
        dq 000092090000FFFFh            ; 28h datos de 16 bits, base 90000h
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
