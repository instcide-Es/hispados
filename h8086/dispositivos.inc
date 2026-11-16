; =============================================================================
;  Hispa-8086 - los dispositivos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como en MS-DOS, cada dispositivo tiene una cabecera de 18 octetos, y todas
;  están encadenadas a partir de NUL:
;       dd  la cabecera siguiente (FFFF:FFFF en la última)
;       dw  atributos (bit 15: de carácter; si no, de bloque: un disco)
;       dw  su rutina de «estrategia» (recibe en ES:BX la petición y la apunta)
;       dw  su rutina de «interrupción» (la atiende)
;       8   su nombre (o, en uno de bloque, cuántas unidades tiene)
;  La petición es un paquete: 0 cuánto mide, 1 la unidad, 2 la orden (0
;  iniciar, 4 leer, 8 escribir...), 3 el estado que devuelve (bit 15 error,
;  bit 8 hecho), 13 el octeto del medio, 14 dónde (dd), 18 cuántos, 20 el
;  primer sector.
;
;  Los del núcleo son NUL, CON, AUX, PRN, COM1 y LPT1, de carácter. Un archivo
;  que se abre con uno de esos nombres (sea cual sea su ruta y su extensión)
;  es el dispositivo: su ficha lleva H_USO = 2 y, en H_TAM, la cabecera. Leer
;  y escribir en él (h_es) es mandarle una petición. Los que se cargan con
;  DEVICE= en CONFIG.SYS (config.inc) se enlazan detrás de NUL, así que un
;  nombre nuevo tapa al antiguo; los de bloque reciben las letras de unidad
;  siguientes (D:, E:).
;
;  La consola de INT 21h (01h a 0Ch) y los números de archivo 0 a 2 no pasan
;  por la cadena: van a la BIOS directamente, como hasta ahora.
; =============================================================================
D_SIG       equ 0
D_ATR       equ 4
D_ESTR      equ 6
D_INT       equ 8
D_NOMBRE    equ 10
P_ORDEN     equ 2
P_ESTADO    equ 3
P_MEDIO     equ 13
P_DONDE     equ 14
P_CUANTOS   equ 18
P_SECTOR    equ 20

%macro DISPOSITIVO 4                ; siguiente, atributos, interrupción, nombre
        dw %1, 0                    ; (el segmento lo pone disp_ini)
        dw %2, dev_estr, %3
        db %4
%endmacro
dev_nul:    DISPOSITIVO dev_con, 0x8004, dev_i_nul, "NUL     "
dev_con:    DISPOSITIVO dev_aux, 0x8003, dev_i_con, "CON     "
dev_aux:    DISPOSITIVO dev_prn, 0x8000, dev_i_aux, "AUX     "
dev_prn:    DISPOSITIVO dev_com1, 0x8000, dev_i_prn, "PRN     "
dev_com1:   DISPOSITIVO dev_lpt1, 0x8000, dev_i_aux, "COM1    "
dev_lpt1:   dw 0xFFFF, 0xFFFF, 0x8000, dev_estr, dev_i_prn
            db "LPT1    "

; pone el segmento en los enlaces de la cadena (al arrancar)
disp_ini:
        mov [dev_nul+2], cs
        mov [dev_con+2], cs
        mov [dev_aux+2], cs
        mov [dev_prn+2], cs
        mov [dev_com1+2], cs
        ret

; ---- los del núcleo ----
dev_estr:
        mov [cs:dv_pet], bx
        mov [cs:dv_pet+2], es
        retf
dev_i_nul:
        push ax
        mov al, 0
        jmp short dev_int
dev_i_con:
        push ax
        mov al, 1
        jmp short dev_int
dev_i_aux:
        push ax
        mov al, 2
        jmp short dev_int
dev_i_prn:
        push ax
        mov al, 3
dev_int:
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push ds
        push es
        push cs
        pop ds
        cld
        mov dl, al                  ; DL = cuál
        les bx, [dv_pet]
        mov word [es:bx+P_ESTADO], 0x0100
        mov cx, [es:bx+P_CUANTOS]
        mov al, [es:bx+P_ORDEN]
        cmp al, 4
        je .leer
        cmp al, 8
        je .escribir
        cmp al, 9
        je .escribir
        jmp .fin                    ; lo demás (iniciar, vaciar...): hecho
.leer:
        mov word [es:bx+P_CUANTOS], 0
        cmp dl, 1
        je .lee_con
        cmp dl, 2
        jne .fin0                   ; NUL y PRN no dan nada
        jcxz .fin0
        mov ah, 2                   ; AUX: un octeto del puerto serie
        xor dx, dx
        int 0x14
        test ah, 0x80
        jnz .fin0
        les bx, [dv_pet]
        push es
        push bx
        les bx, [es:bx+P_DONDE]
        mov [es:bx], al
        pop bx
        pop es
        mov word [es:bx+P_CUANTOS], 1
.fin0:
        jmp .fin
.lee_con:
        ; CON: una línea del teclado, con su retorno y su salto; si empieza
        ; por Ctrl+Z, nada (el fin de lo que se teclea)
        push es
        push bx
        push cx
        push cs
        pop es
        mov byte [con_directo], 1
        mov di, linea
        mov cx, LIN_MAX
        call lee_linea
        call pon_nl
        mov byte [con_directo], 0
        mov si, cx
        pop cx
        pop bx
        pop es
        cmp byte [linea], 0x1A
        je .fin
        mov word [linea+si], 0x0A0D
        add si, 2
        cmp cx, si
        jbe .lc
        mov cx, si
.lc:
        mov [es:bx+P_CUANTOS], cx
        les di, [es:bx+P_DONDE]
        mov si, linea
        rep movsb
        jmp short .fin
.escribir:
        test dl, dl
        jz .fin                     ; NUL se lo traga todo
        jcxz .fin
        push ds
        lds si, [es:bx+P_DONDE]
        xor di, di                  ; DI = cuántos van
.e:
        lodsb
        cmp dl, 2
        jb .e_con
        je .e_aux
        push dx                     ; PRN: a la impresora
        xor dx, dx
        xor ah, ah
        int 0x17
        pop dx
        test ah, 0x28               ; sin papel o error
        jnz .e_mal
        test ah, 0x01               ; no contesta (si dice que está
        jz .e_sig                   ; «seleccionada», hay BIOS que dejan ese
        test ah, 0x10               ; bit puesto sin que haya pasado nada)
        jz .e_mal
        jmp short .e_sig
.e_aux:
        push dx
        xor dx, dx
        mov ah, 1
        int 0x14
        pop dx
        test ah, 0x80
        jnz .e_mal
        jmp short .e_sig
.e_con:
        push bx
        push bp
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        pop bp
        pop bx
.e_sig:
        inc di
        loop .e
        pop ds
        jmp short .fin
.e_mal:
        pop ds
        mov [es:bx+P_CUANTOS], di   ; hasta aquí
        mov word [es:bx+P_ESTADO], 0x810A
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
        retf

; ---- pedirle algo a un dispositivo ----
; manda la petición `pet` (ya rellena) al dispositivo de cabecera DX:AX.
; Conserva los registros
dev_llama:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push ds
        push es
        mov byte [pet], 26
        mov word [pet+P_ESTADO], 0
        mov es, dx
        mov si, ax
        mov [dv_far+2], dx
        mov ax, [es:si+D_ESTR]
        mov [dv_far], ax
        mov ax, [es:si+D_INT]
        push ax
        push cs
        pop es
        mov bx, pet
        call far [dv_far]
        pop ax
        push cs
        pop ds
        mov [dv_far], ax
        push cs
        pop es
        mov bx, pet
        call far [dv_far]
        pop es
        pop ds
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        cld
        ret

; lee ([es_op]=0) o escribe (1) CX octetos de [es_seg]:[es_off] en el
; dispositivo de carácter de cabecera DX:AX -> AX = los que han sido. CF=1 y
; AX=error si el dispositivo dice que no ha podido con ninguno
dev_es:
        mov [pet+P_CUANTOS], cx
        push ax
        mov ax, [es_off]
        mov [pet+P_DONDE], ax
        mov ax, [es_seg]
        mov [pet+P_DONDE+2], ax
        mov al, 4
        cmp byte [es_op], 0
        je .c
        mov al, 8
.c:
        mov [pet+P_ORDEN], al
        pop ax
        call dev_llama
        mov ax, [pet+P_CUANTOS]
        test byte [pet+P_ESTADO+1], 0x80
        jz .bien
        test ax, ax
        jnz .bien
        mov ax, E_DISP
        stc
        ret
.bien:
        clc
        ret

; lee ([op13]=2) o escribe (3) el sector AX de la unidad cargada, que es de
; un controlador de bloque, en ES:BX. CF=1 y AX=E_DISCO si falla. Conserva los
; demás registros
dev_bloque:
        push dx
        push ax
        mov [pet+P_SECTOR], ax
        mov [pet+P_DONDE], bx
        mov [pet+P_DONDE+2], es
        mov word [pet+P_CUANTOS], 1
        mov al, [u_sub]
        mov [pet+1], al
        mov al, [bpb_ant+10]
        mov [pet+P_MEDIO], al
        mov al, 4
        cmp byte [op13], 2
        je .c
        mov al, 8
.c:
        mov [pet+P_ORDEN], al
        mov ax, [u_dev]
        mov dx, [u_dev+2]
        call dev_llama
        pop ax
        pop dx
        test byte [pet+P_ESTADO+1], 0x80
        jz .bien
        mov ax, E_DISCO
        stc
        ret
.bien:
        clc
        ret

; ¿es de un dispositivo el nombre con que acaba `ruta`? CF=0 y DX:AX = su
; cabecera si lo es. Conserva los demás registros (y `ruta`; cambia nom11)
dev_busca:
        push bx
        push cx
        push si
        push di
        push es
        mov si, ruta                ; lo último de la ruta
        mov bx, si
.u:
        lodsb
        test al, al
        jz .nom
        cmp al, '\'
        je .m
        cmp al, '/'
        je .m
        cmp al, ':'
        jne .u
.m:
        mov bx, si
        jmp .u
.nom:
        mov si, bx
        call nombre_fat             ; -> nom11 (la extensión no cuenta)
        mov bx, dev_nul
        mov dx, cs
.b:
        mov es, dx
        test byte [es:bx+D_ATR+1], 0x80
        jz .sig                     ; (los de bloque no tienen nombre)
        lea di, [bx+D_NOMBRE]
        mov si, nom11
        mov cx, 8
        repe cmpsb
        je .esta
.sig:
        les bx, [es:bx+D_SIG]
        mov dx, es
        cmp bx, 0xFFFF
        jne .b
        stc
        jmp short .x
.esta:
        mov ax, bx
        clc
.x:
        pop es
        pop di
        pop si
        pop cx
        pop bx
        ret

; (para 3Ch y 3Dh) si la ruta de quien llama (DS:DX) nombra un dispositivo,
; lo abre: CF=0 y ya está contestado (el número de archivo, o el error). CF=1
; si no es un dispositivo
dev_abre:
        mov es, [bp+R_DS]
        mov si, [bp+R_DX]
        call ruta_copia
        push cs
        pop es
        call dev_busca
        jc .no
        call h_libre
        jc .err
        mov byte [bx+H_USO], 2
        mov byte [bx+H_MODO], 2
        mov byte [bx+H_UNI], 0xFF
        mov [bx+H_TAM], ax
        mov [bx+H_TAM+2], dx
        mov ax, [psp_act]
        mov [bx+H_PSP], ax
        call h_numero
        mov [bp+R_AX], ax
        clc
        ret
.err:
        call f_error
        clc
.no:
        ret
