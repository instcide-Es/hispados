; =============================================================================
;  Hispa-8086 - el arranque del núcleo (lo que solo se usa una vez)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como el INIT de Hispa-DOS (y el SYSINIT de MS-DOS): lo que solo hace falta
;  al arrancar (reconocer el procesador, poner los vectores, leer CONFIG.SYS,
;  subir a la HMA...) va en su sección, «.init», detrás de todo lo residente
;  en el archivo. El puente salta aquí, y lo primero que se hace es copiarse a
;  lo alto de la memoria convencional y seguir desde allí, con su propio
;  segmento (CS), mientras DS, ES y SS son los del núcleo. Así las variables
;  del núcleo, que en memoria van justo detrás de lo residente (donde estaba
;  esto en el archivo), se pueden poner a cero; y al acabar, esta copia la
;  pisan los programas como cualquier memoria libre.
;
;  Desde aquí, una rutina del núcleo no se puede llamar con un CALL cercano
;  (está en otro segmento): se llama a «k_nombre», que la llama por «lejos»
;  (nucleo.asm). Los textos y tablas de aquí se leen con CS: delante. Detrás
;  del arranque, en el archivo y en la copia, va el gestor de memoria
;  extendida (alta.inc, «.xms»), que config.inc pone en su sitio si hace falta.
; =============================================================================
        section .init follows=.text vstart=0 align=16

; ---- lo primero: copiarse arriba. CS = donde está en el archivo ----
i_entrada:
        mov bp, cs                  ; BP = el segmento del núcleo
        sub bp, TXT_TAM / 16 + 1
        int 0x12                    ; la memoria convencional, en K
        mov cl, 6
        shl ax, cl
        sub ax, (INI_TODO + 15) / 16
        mov es, ax
        push cs
        pop ds
        xor si, si
        xor di, di
        mov cx, INI_TODO
        cld
        rep movsb
        push es
        mov ax, i_arriba
        push ax
        retf

; ---- las llamadas al núcleo ----
%macro K 1
k_%1:
        mov word [ss:lej_rut], %1
        jmp short k_salta
%endmacro
        K pon_car
        K pon_cad
        K pon_nl
        K disp_ini
        K disco_ini
        K duro_ini
k_salta:
        call far [cs:i_lejos]
        ret
        K unidad_pon
        K abre_ruta
        K h_es
        K h_cierra
        K mayus
        K dev_llama
i_lejos     dw lejos, 0             ; (el segmento, el del núcleo: al subir a
i_interp    dw interprete, 0        ; la HMA, FFFFh)

; escribe el texto de CS:SI (los de aquí), que acaba en 0
ipon_cad:
        push ax
        push si
.c:
        cs lodsb
        test al, al
        jz .x
        call k_pon_car
        jmp .c
.x:
        pop si
        pop ax
        ret

; ---- y ahora sí, el arranque ----
i_arriba:
        cli
        mov ds, bp
        mov es, bp
        mov ss, bp
        mov sp, pila_fin
        sti
        mov [cs:i_lejos+2], bp
        mov [cs:i_interp+2], bp
        mov di, bss_ini             ; las variables, a cero
        mov cx, fichas + N_ARCH * H_TAM_F - bss_ini
        xor al, al
        rep stosb
        mov [arr_bios], dl          ; de dónde se ha arrancado
        mov [seg_bajo], ds          ; (el del puente: el núcleo puede subir)
        mov word [b_gdtp], gdt_bss
        mov ax, 0xFFFF
        mov [buf_lba], ax
        mov [fat_lba], ax
        mov byte [n_arch], N_ARCH
        mov byte [cache_mete], 1
        mov word [fecha_xt], 0x0021 ; (sin reloj: 1 de enero de 1980)
        call k_disp_ini
        call cpu_detecta
        int 0x12                    ; la memoria convencional, en K
        mov [mem_kb], ax
        ; los vectores del sistema: INT 20h (terminar), 21h, 25h, 26h y 27h
        xor di, di
        mov es, di
        mov si, i_vectores
        mov cx, 5
.v:
        cs lodsw
        mov di, ax
        cs lodsw
        stosw
        mov ax, ds
        stosw
        loop .v
        ; [REINICIO-SILENCIO] INT 15h (el aviso de tecla, AH=4Fh): con
        ; Ctrl+Alt+Supr, el sonido callado antes de que la BIOS reinicie
        cli
        les ax, [es:0x15*4]
        mov [ant15], ax
        mov [ant15+2], es
        xor ax, ax
        mov es, ax
        mov word [es:0x15*4], b_int15
        mov [es:0x15*4+2], ds
        sti
        mov bx, b_punteros          ; el puente: de momento, el núcleo está aquí
        mov cx, N_PUENTES
.pt:
        mov [bx+2], ds
        add bx, 4
        loop .pt
        mov [bajo_seg], ds
        mov bx, p_esta              ; las rutinas del puente, para llamarlas
        mov si, i_puente            ; desde arriba
        mov cl, 4
.pp:
        cs lodsw
        mov [bx], ax
        mov [bx+2], ds
        add bx, 4
        loop .pp
        mov al, [cfg_inicial]
        mov ah, al
        and al, 1
        mov [dos_alto], al
        shr ah, 1
        and ah, 1
        mov [como_8086], ah
        push ds
        pop es
        mov [dta_seg], ds           ; las búsquedas, por ahora, a lo nuestro
        mov word [dta_off], dta_int
        ; las unidades. Arrancando de un disquete, ese es A: y la unidad de
        ; trabajo; arrancando del disco duro, A: es la primera disquetera y
        ; se trabaja en C:
        mov al, [arr_bios]
        test al, al
        js .duro
        mov [u_bios], al
        call k_disco_ini
        call unidades_ini
        jmp short .unidades
.duro:
        call unidades_ini
        cmp byte [u_hay+2], 0
        je .unidades
        mov al, 2
        mov [unidad_def], al
        call k_unidad_pon
        mov byte [ent_nucleo.u1], 'C'   ; (PATH y COMSPEC, en C:)
        mov byte [ent_nucleo.u2], 'C'
.unidades:
        mov si, m_titulo
        call ipon_cad
        call config_lee             ; CONFIG.SYS: FILES=, BUFFERS=, DEVICE=...
        ; dónde va el programa: detrás de todo lo del núcleo, que acaba en la
        ; tabla de archivos abiertos, la reserva de sectores y los
        ; controladores cargados (config_lee deja dicho dónde)
        mov ax, [cf_tope]
        mov [mem_seg], ax           ; ahí empieza la memoria que se reparte
        call mem_ini
        mov byte [primera], 1
        jmp far [cs:i_interp]       ; y a lanzar el intérprete

i_vectores  dw 0x20*4, b_int20, 0x21*4, b_int21, 0x25*4, b_int25, 0x26*4, b_int26, 0x27*4, b_int27
i_puente    dw b_f_esta, b_f_abre, b_f_cierra, b_mueve

; la memoria que se reparte: un solo bloque libre, de [mem_seg] hasta el final
mem_ini:
        push es
        mov ax, [mem_seg]
        mov es, ax
        mov byte [es:MCB_TIPO], 'Z'
        mov word [es:MCB_DUENO], 0
        mov bx, [mem_kb]
        mov cl, 6
        shl bx, cl
        sub bx, ax
        dec bx
        mov [es:MCB_TAM], bx
        pop es
        ret

; las unidades que hay: A: y B: siempre (con una sola disquetera, B: es la
; misma con otro disquete), y C: si el disco duro tiene una partición FAT
unidades_ini:
        mov byte [u_hay], 1
        mov byte [u_hay+1], 1
        int 0x11                    ; ¿dos disqueteras?
        test al, 1
        jz .una
        and al, 0xC0
        jz .una
        mov byte [u_guardadas + U_TAM + (u_bios - u_ini)], 1
        jmp k_duro_ini
.una:
        mov byte [una_disq], 1      ; una sola: B: es la misma, con otro
        jmp k_duro_ini              ; disquete (ver disco_es)

m_titulo    db 10, "Hispa-8086 0.12 - de la familia Hispa-DOS", 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.", 10, 10, 0

        section .text
