; =============================================================================
;  Hispa-DOS COMMAND.COM - intérprete de órdenes
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;  Usa solo la API pública de INT 21h, así que también funciona sobre otros DOS.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        jmp boot

; =============================================================================
;  PARTE RESIDENTE. Para dejar a los programas casi toda la memoria
;  convencional (muchos juegos piden más de 500 KB libres), el intérprete se
;  parte en dos como el de MS-DOS:
;   - esto (unos 2 KB junto al PSP): los manejadores de Ctrl+C y de error
;     crítico y la rutina que lanza los programas;
;   - el resto («el cuerpo»): se copia al arrancar al final de la memoria
;     convencional y allí trabaja. Al lanzar un programa el cuerpo entero
;     (código, datos y pila) se guarda en memoria extendida (XMS), se libera
;     su bloque y al volver se recupera.
;  Sin gestor XMS (o si falla algo) no se parte: todo se queda aquí, como
;  antes (r_body = r_psp). El cuerpo es el mismo binario en otro segmento: no
;  guarda segmentos propios de una orden a otra.
; =============================================================================
R_STACK     equ 384
r_psp       dw 0                    ; segmento del PSP (y de esta parte)
r_body      dw 0                    ; segmento del cuerpo
r_swap      db 0                    ; 1 = el cuerpo está aparte y se guarda en XMS
r_fuera     db 0                    ; el cuerpo está fuera (guardado en XMS)
r_inexec    db 0                    ; lanzando un programa
r_cf        db 0
r_res       dw 0
r_strat     dw 0
r_handle    dw 0                    ; bloque de XMS para el cuerpo
r_paras     dw 0                    ; párrafos del cuerpo
r_xms       dd 0                    ; entrada del gestor XMS
r_mv        times 16 db 0           ; estructura de XMS 0Bh (mover)
r_pb        times 14 db 0           ; bloque de parámetros de EXEC
r_fcb1      times 16 db 0
r_fcb2      times 16 db 0
r_path      times 160 db 0
r_tail      times 130 db 0
r_msg_dead  db 13, 10, "No hay memoria para recargar el int", 0x82, "rprete de ", 0xA2, "rdenes. Sistema detenido.", 13, 10, 0
            times R_STACK db 0
r_stack_top:

; Lanza r_path con r_tail/r_fcb1/r_fcb2 (los deja run_prog). Se entra con un
; salto lejano desde el cuerpo y se vuelve a exec_ret (del cuerpo) con
; AX = resultado de AH=4Bh y BL = FFh si hubo error.
r_exec:
        cli
        mov ax, cs
        mov ss, ax
        mov sp, r_stack_top
        sti
        mov ds, ax
        mov es, ax
        mov byte [r_inexec], 1
        mov byte [r_fuera], 0
        cmp byte [r_swap], 0
        je .run
        ; cuerpo -> XMS
        mov ax, prog_end + 1
        and ax, 0xFFFE
        mov [r_mv+0], ax
        mov word [r_mv+2], 0
        mov word [r_mv+4], 0
        mov word [r_mv+6], 0
        mov ax, [r_body]
        mov [r_mv+8], ax
        mov ax, [r_handle]
        mov [r_mv+10], ax
        mov dword [r_mv+12], 0
        mov si, r_mv
        mov ah, 0x0B
        call far [r_xms]
        cmp ax, 1
        jne .run
        mov es, [r_body]
        mov ah, 0x49
        int 0x21
        jc .run
        mov byte [r_fuera], 1
.run:
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov word [r_pb], 0
        mov word [r_pb+2], r_tail
        mov [r_pb+4], ax
        mov word [r_pb+6], r_fcb1
        mov [r_pb+8], ax
        mov word [r_pb+10], r_fcb2
        mov [r_pb+12], ax
        mov dx, r_path
        mov bx, r_pb
        mov ax, 0x4B00
        int 0x21
        ; (los MOV, CLI, STI y CLD no tocan CF)
        cli
        mov bx, cs
        mov ss, bx
        mov sp, r_stack_top
        sti
        mov ds, bx
        mov es, bx
        cld
        sbb bl, bl
        mov [r_cf], bl
        mov [r_res], ax
        cmp byte [r_fuera], 0
        je .back
        ; recuperar el cuerpo: bloque al final de la memoria (último ajuste)
        mov ax, 0x5800
        int 0x21
        mov [r_strat], ax
        mov ax, 0x5801
        mov bx, 2
        int 0x21
        mov bx, [r_paras]
        mov ah, 0x48
        int 0x21
        pushf
        push ax
        mov ax, 0x5801
        mov bx, [r_strat]
        int 0x21
        pop ax
        popf
        jc .dead
        mov [r_body], ax
        mov [r_mv+14], ax
        mov ax, prog_end + 1
        and ax, 0xFFFE
        mov [r_mv+0], ax
        mov word [r_mv+2], 0
        mov bx, [r_handle]
        mov [r_mv+4], bx
        mov dword [r_mv+6], 0
        mov word [r_mv+10], 0
        mov word [r_mv+12], 0
        mov si, r_mv
        mov ah, 0x0B
        call far [r_xms]
        cmp ax, 1
        jne .dead
        mov byte [r_fuera], 0
.back:
        mov byte [r_inexec], 0
        mov ax, [r_res]
        mov bl, [r_cf]
        mov dx, [r_body]
        mov ds, dx
        mov es, dx
        cli
        mov ss, dx
        mov sp, [exec_sp]           ; (la pila del cuerpo, donde la dejó)
        sti
        push dx
        push word exec_ret
        retf
.dead:
        mov dx, r_msg_dead
        call ce_puts
.halt:
        hlt
        jmp .halt

; Ctrl+C (INT 23h): si corre un programa, terminarlo; si no, cancelar el lote
; y la línea y volver al indicador (ctrlc_body, en el cuerpo)
ctrlc_handler:
        push ax
        push bx
        mov ah, 0x62
        int 0x21
        cmp bx, [cs:r_psp]
        jne .child
        cmp byte [cs:r_inexec], 0
        jne .ign
        pop bx
        pop ax
        mov ax, [cs:r_body]
        mov ds, ax
        mov es, ax
        cli
        mov ss, ax
        mov sp, stack_top
        sti
        push ax
        push word ctrlc_body
        retf
.ign:
        pop bx
        pop ax
        iret
.child:
        pop bx
        pop ax
        mov ax, 0x4C03
        int 0x21

; -----------------------------------------------------------------------------
;  Error crítico (INT 24h). AH = tipo (bit 7: dispositivo de caracteres;
;  bit 0: escritura; bits 3-5: se puede descartar, reintentar, ignorar),
;  AL = unidad, DI = código, BP:SI = cabecera del dispositivo.
;  Solo se usan las funciones 01h-0Ch de INT 21h. -> AL = acción.
; -----------------------------------------------------------------------------
crit_handler:
        sti
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
        mov [ce_ah], ah
        mov [ce_al], al
        mov es, bp                  ; ES:SI = cabecera del dispositivo
        call ce_crlf
        mov bx, di
        and bx, 0xFF
        cmp bx, 15
        jbe .k
        mov bx, 12
.k:
        shl bx, 1
        mov dx, [ce_msgs+bx]
        test byte [ce_ah], 0x80
        jnz .dev
        ; «Error al leer la unidad A: ...»
        push dx
        mov dx, ce_rd
        test byte [ce_ah], 1
        jz .rw
        mov dx, ce_wr
.rw:
        call ce_puts
        mov dl, [ce_al]
        add dl, 'A'
        call ce_putc
        mov dl, ':'
        call ce_putc
        mov dl, ' '
        call ce_putc
        pop dx
        call ce_puts
        jmp .ask
.dev:
        ; «Error en el dispositivo PRN: ...»
        push dx
        mov dx, ce_dev
        call ce_puts
        mov cx, 8
        add si, 10
.dn:
        mov dl, [es:si]
        cmp dl, ' '
        je .dne
        call ce_putc
        inc si
        loop .dn
.dne:
        mov dl, ':'
        call ce_putc
        mov dl, ' '
        call ce_putc
        pop dx
        call ce_puts
.ask:
        call ce_crlf
        mov dx, ce_q_a
        call ce_puts
        test byte [ce_ah], 0x10
        jz .q1
        mov dx, ce_q_r
        call ce_puts
.q1:
        test byte [ce_ah], 0x20
        jz .q2
        mov dx, ce_q_i
        call ce_puts
.q2:
        test byte [ce_ah], 0x08
        jz .q3
        mov dx, ce_q_d
        call ce_puts
.q3:
        mov dx, ce_q_end
        call ce_puts
.key:
        mov ah, 0x07                ; sin eco ni Ctrl+C
        int 0x21
        mov dl, al
        and al, 0xDF
        mov bl, 2
        cmp al, 'A'
        je .ok
        mov bl, 1
        cmp al, 'R'
        jne .ni
        test byte [ce_ah], 0x10
        jnz .ok
.ni:
        mov bl, 0
        cmp al, 'I'
        jne .nd
        test byte [ce_ah], 0x20
        jnz .ok
.nd:
        mov bl, 3
        cmp al, 'D'
        jne .key
        test byte [ce_ah], 0x08
        jz .key
.ok:
        mov dl, al
        call ce_putc
        call ce_crlf
        mov al, bl
        pop es
        pop ds
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        iret
ce_putc:
        push ax
        mov ah, 0x02
        int 0x21
        pop ax
        ret
ce_puts:
        push si
        push dx
        mov si, dx
.l:
        mov dl, [si]
        test dl, dl
        jz .e
        call ce_putc
        inc si
        jmp .l
.e:
        pop dx
        pop si
        ret
ce_crlf:
        push dx
        mov dl, 13
        call ce_putc
        mov dl, 10
        call ce_putc
        pop dx
        ret
ce_ah       db 0
ce_al       db 0
ce_rd       db "Error al leer la unidad ", 0
ce_wr       db "Error al escribir en la unidad ", 0
ce_dev      db "Error en el dispositivo ", 0
ce_q_a      db "¿Anular", 0
ce_q_r      db ", Reintentar", 0
ce_q_i      db ", Ignorar", 0
ce_q_d      db ", Descartar", 0
ce_q_end    db "? ", 0
ce_e0       db "el disco está protegido contra escritura", 0
ce_e1       db "unidad desconocida", 0
ce_e2       db "la unidad no está preparada", 0
ce_e3       db "orden desconocida", 0
ce_e4       db "error en los datos (CRC)", 0
ce_e5       db "petición incorrecta", 0
ce_e6       db "error de búsqueda", 0
ce_e7       db "tipo de disco desconocido", 0
ce_e8       db "no se encuentra el sector", 0
ce_e9       db "no queda papel", 0
ce_e10      db "fallo de escritura", 0
ce_e11      db "fallo de lectura", 0
ce_e12      db "error general", 0
ce_e15      db "cambio de disco no válido", 0
ce_msgs     dw ce_e0, ce_e1, ce_e2, ce_e3, ce_e4, ce_e5, ce_e6, ce_e7
            dw ce_e8, ce_e9, ce_e10, ce_e11, ce_e12, ce_e12, ce_e12, ce_e15

resident_end:
; =============================================================================
;  Fin de la parte residente
; =============================================================================

; Arranque: decidir si el cuerpo va aparte (hace falta un gestor XMS)
boot:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, (stack_top - bss_start) / 2
        xor ax, ax
        rep stosw
        mov [r_psp], cs
        mov [r_body], cs
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .classic
        mov ax, 0x4310
        int 0x2F
        mov [r_xms], bx
        mov [r_xms+2], es
        push cs
        pop es
        mov dx, prog_end + 1023
        shr dx, 10
        mov ah, 0x09
        call far [r_xms]
        cmp ax, 1
        jne .classic
        mov [r_handle], dx
        ; el bloque del PSP se queda con la parte residente...
        mov bx, resident_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        jc .undo
        ; ...y el cuerpo va al final de la memoria convencional
        mov ax, 0x5800
        int 0x21
        mov [r_strat], ax
        mov ax, 0x5801
        mov bx, 2
        int 0x21
        jc .undo                    ; (un DOS sin estrategias: no se parte)
        mov bx, prog_end + 15
        shr bx, 4
        mov [r_paras], bx
        mov ah, 0x48
        int 0x21
        pushf
        push ax
        mov ax, 0x5801
        mov bx, [r_strat]
        int 0x21
        pop ax
        popf
        jc .undo
        ; (no puede pisar la copia de aquí, que todavía se está usando)
        mov bx, cs
        add bx, [r_paras]
        cmp ax, bx
        jb .undo_free
        mov [r_body], ax
        mov byte [r_swap], 1
        mov es, ax
        xor si, si
        xor di, di
        mov cx, prog_end + 1
        shr cx, 1
        rep movsw
        cli
        mov ss, ax
        sti
        mov ds, ax
        push ax
        push word .go
        retf
.undo_free:
        mov es, ax
        mov ah, 0x49
        int 0x21
.undo:
        push cs
        pop es
        mov dx, [r_handle]
        mov ah, 0x0A
        call far [r_xms]
.classic:
        push cs
        pop es
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A                ; reducir nuestro bloque y dejar memoria
        int 0x21
.go:
        mov byte [echo_on], 1
        mov word [sv_in], 0xFFFF
        mov word [sv_out], 0xFFFF
        call env_init
        ; Ctrl+C y error crítico («¿Anular, Reintentar, Ignorar, Descartar?»):
        ; los manejadores de la parte residente (los heredan los programas)
        push ds
        mov ds, [r_psp]
        mov dx, ctrlc_handler
        mov ax, 0x2523
        int 0x21
        mov dx, crit_handler
        mov ax, 0x2524
        int 0x21
        pop ds
        ; opciones: /P (intérprete principal), /D (sin AUTOEXEC.BAT) y /C orden
        mov si, 0x81
.opt:
        lodsb
        cmp al, 13
        je .optd
        cmp al, '/'
        jne .opt
        mov al, [si]
        call upcase
        cmp al, 'P'
        jne .notp
        mov byte [primary], 1
        jmp .opt
.notp:
        cmp al, 'D'
        jne .notd
        mov byte [no_auto], 1
        jmp .opt
.notd:
        cmp al, 'C'
        jne .opt
        inc si
        mov di, inbuf+2
.cc:
        lodsb
        cmp al, 13
        je .cce
        stosb
        jmp .cc
.cce:
        mov byte [di], 0
        mov byte [exit_after], 1
        mov si, inbuf+2
        mov word [pl_in], 0
        mov byte [pl_depth], 0
        call run_line
        jmp main_loop
.optd:
        mov si, msg_hello
        call puts
        cmp byte [primary], 0
        je main_loop
        cmp byte [no_auto], 0
        jne main_loop
        ; AUTOEXEC.BAT de la unidad de arranque
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [s_autoexec], dl
        call set_dta
        mov dx, s_autoexec
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jc main_loop
        mov si, s_autoexec
        mov di, progpath
.ca:
        lodsb
        stosb
        test al, al
        jnz .ca
        mov dword [cmdword], 'AUTO'
        mov dword [cmdword+4], 'EXEC'
        mov byte [cmdword+8], 0
        mov word [argp], s_empty
        call start_batch

main_loop:
        cmp byte [bdepth], 0
        jne .batch
        cmp byte [exit_after], 0
        jne .exit
        cmp byte [echo_on], 0
        je .noprompt
        call prompt
.noprompt:
        mov dx, inbuf
        mov byte [inbuf], 127
        mov ah, 0x0A
        int 0x21
        mov dl, 10
        mov ah, 2
        int 0x21
        movzx bx, byte [inbuf+1]
        mov byte [inbuf+2+bx], 0
        mov si, inbuf+2
        mov word [pl_in], 0
        mov byte [pl_depth], 0
        call run_line
        jmp main_loop
.batch:
        call batch_line
        jc main_loop
        mov si, eline
        mov word [pl_in], 0
        mov byte [pl_depth], 0
        call run_line
        jmp main_loop
.exit:
        call xms_release
        mov al, [errlvl]
        mov ah, 0x4C
        int 0x21

; mayúsculas para el nombre de una orden, sin acentos (CP437 no tiene Ó, Á...)
; así «Versión» o «VERSIÓN» son VERSION
cw_upcase:
        push si
        mov si, cw_fold
.f:
        cmp byte [si], 0
        je .u
        cmp al, [si]
        je .h
        add si, 2
        jmp .f
.h:
        mov al, [si+1]
        pop si
        ret
.u:
        pop si
        jmp upcase
cw_fold     db 0xA0, 'A', 0x82, 'E', 0x90, 'E', 0xA1, 'I', 0xA2, 'O', 0xA3, 'U'
            db 0x81, 'U', 0x9A, 'U', 0x85, 'A', 0x8A, 'E', 0x8D, 'I', 0x95, 'O'
            db 0x97, 'U', 0xA4, 0xA5
            db 0xB5, 'A', 0xD6, 'I', 0xE0, 'O', 0xE9, 'U', 0   ; Á Í Ó Ú de CP850
; Ctrl+C en el intérprete (viene de ctrlc_handler, con DS=ES=SS y la pila ya
; puestos): cancelar el lote y la línea y volver al indicador
ctrlc_body:
        cld
        call dir_free
        call redir_restore
        mov byte [bdepth], 0
        mov byte [echo_on], 1
        mov byte [call_flag], 0
        mov byte [in_for], 0
        jmp main_loop

; Lanza progpath con tail/fcb1/fcb2 por la parte residente (que guarda el
; cuerpo en XMS mientras corre el programa). -> CF y AX como AH=4Bh
run_prog:
        pusha                       ; (como AH=4Bh: solo cambia AX)
        push es
        mov es, [r_psp]
        mov si, progpath
        mov di, r_path
        mov cx, 160
        rep movsb
        mov si, tail
        mov di, r_tail
        mov cx, 130
        rep movsb
        mov si, fcb1
        mov di, r_fcb1
        mov cx, 16
        rep movsb
        mov si, fcb2
        mov di, r_fcb2
        mov cx, 16
        rep movsb
        pop es
        mov [exec_sp], sp
        push word [r_psp]
        push word r_exec
        retf
exec_ret:
        mov sp, [exec_sp]
        cld
        mov [exec_ax], ax
        mov [exec_cf], bl
        popa
        mov ax, [exec_ax]
        cmp byte [exec_cf], 1       ; CF = no hubo error...
        cmc                         ; ...y al revés: CF = hubo error
        ret

; al salir: devolver el bloque de XMS del cuerpo
xms_release:
        cmp byte [r_swap], 0
        je .r
        mov dx, [r_handle]
        mov ah, 0x0A
        call far [r_xms]
.r:
        ret

; -----------------------------------------------------------------------------
prompt:
        mov si, s_promptvar
        mov di, pbuf
        call env_get
        jnc .have
        mov si, s_defprompt
        mov di, pbuf
.dc:
        lodsb
        stosb
        test al, al
        jnz .dc
.have:
        mov si, pbuf
.l:
        lodsb
        test al, al
        jz .r
        cmp al, '$'
        je .code
        call putc
        jmp .l
.code:
        lodsb
        test al, al
        jz .r
        call upcase
        cmp al, 'P'
        je .p
        cmp al, 'G'
        je .g
        cmp al, 'L'
        je .lt
        cmp al, 'B'
        je .b
        cmp al, 'Q'
        je .q
        cmp al, 'N'
        je .n
        cmp al, '_'
        je .nl
        cmp al, 'E'
        je .e
        cmp al, 'H'
        je .h
        cmp al, 'D'
        je .d
        cmp al, 'T'
        je .t
        cmp al, 'V'
        je .v
        cmp al, '$'
        je .dl
        jmp .l
.p:
        push si
        call put_drive
        mov si, curdir
        mov dl, 0
        mov ah, 0x47
        int 0x21
        call puts
        pop si
        jmp .l
.g:
        mov al, '>'
        jmp .out
.lt:
        mov al, '<'
        jmp .out
.b:
        mov al, '|'
        jmp .out
.q:
        mov al, '='
        jmp .out
.dl:
        mov al, '$'
        jmp .out
.e:
        mov al, 27
        jmp .out
.h:
        mov al, 8
        call putc
        mov al, ' '
        call putc
        mov al, 8
        jmp .out
.n:
        mov ah, 0x19
        int 0x21
        add al, 'A'
        jmp .out
.nl:
        call crlf
        jmp .l
.d:
        push si
        call put_date
        pop si
        jmp .l
.t:
        push si
        call put_time
        pop si
        jmp .l
.v:
        push si
        mov si, msg_vername
        call puts
        pop si
        jmp .l
.out:
        call putc
        jmp .l
.r:
        ret

; DS:SI = línea
execute_line:
        call skip_sp
        cmp byte [si], 0
        je .r
        cmp byte [si], ':'
        je .r
        cmp byte [si], '@'
        jne .w
        inc si
        call skip_sp
        cmp byte [si], 0            ; «@» sola: nada
        je .r
.w:
        ; «/ver» como «ver»; «/?» solo, la ayuda
        cmp byte [si], '/'
        jne .w2
        cmp word [si+1], '?'        ; «/?» y fin de línea
        jne .w1
        mov si, e_ayuda
        jmp .w2
.w1:
        inc si
        jmp .w
.w2:
        ; palabra de orden -> cmdword (mayúsculas)
        mov di, cmdword
        mov cx, 63
.cw:
        mov al, [si]
        test al, al
        jz .curdir
        cmp al, ' '
        je .curdir
        cmp al, 9
        je .curdir
        cmp al, '/'
        je .curdir
        cmp al, ';'                 ; «PATH=C:\DOS», «ECHO,», «PROMPT=$P$G»
        je .curdir
        cmp al, ','
        je .curdir
        cmp al, '='
        je .curdir
        cmp al, '\'
        je .cwi
        cmp al, '.'
        jne .cw1
.cwi:
        ; "ECHO." / "CD.." / "CD\" : cortar si lo anterior es una orden interna
        push si
        push di
        mov byte [di], 0
        call find_internal
        pop di
        pop si
        jnc .curdir
        mov al, [si]
.cw1:
        call cw_upcase
        mov [di], al
        inc di
        inc si
        loop .cw
.curdir:
        mov byte [di], 0
        mov [argp], si
        ; "A:" cambiar de unidad
        cmp byte [cmdword+1], ':'
        jne .ni
        cmp byte [cmdword+2], 0
        jne .ni
        mov dl, [cmdword]
        sub dl, 'A'
        mov ah, 0x0E
        int 0x21
        mov ah, 0x19
        int 0x21
        cmp al, dl
        je .r
        mov si, msg_baddrv
        jmp puts
.ni:
        call find_internal
        jc .ext
        mov si, [argp]
        call skip_sp
        cmp word [si], '/?'
        je int_help
        cmp bx, do_echo             ; ECHO mira él mismo su separador
        jne .go
        mov si, [argp]
.go:
        jmp bx
.ext:
        jmp run_external
.r:
        ret

; cmdword -> BX = rutina; CF=1 si no es interna
find_internal:
        mov bx, cmd_table
.l:
        mov si, [bx]
        test si, si
        jz .no
        mov di, cmdword
.c:
        mov al, [si]
        cmp al, [di]
        jne .n
        test al, al
        jz .yes
        inc si
        inc di
        jmp .c
.n:
        add bx, 4
        jmp .l
.yes:
        mov bx, [bx+2]
        clc
        ret
.no:
        stc
        ret

cmd_table:
        dw n_ver, do_ver
        dw n_cls, do_cls
        dw n_dir, do_dir
        dw n_type, do_type
        dw n_cd, do_cd
        dw n_chdir, do_cd
        dw n_echo, do_echo
        dw n_set, do_set
        dw n_mem, do_mem
        dw n_date, do_date
        dw n_time, do_time
        dw n_vol, do_vol
        dw n_help, do_help
        dw n_rem, do_rem
        dw n_exit, do_exit
        dw n_copy, do_copy
        dw n_del, do_del
        dw n_erase, do_del
        dw n_ren, do_ren
        dw n_rename, do_ren
        dw n_md, do_md
        dw n_mkdir, do_md
        dw n_rd, do_rd
        dw n_rmdir, do_rd
        dw n_deltree, do_deltree
        dw n_pause, do_pause
        dw n_if, do_if
        dw n_goto, do_goto
        dw n_for, do_for
        dw n_shift, do_shift
        dw n_call, do_call
        dw n_lh, do_lh
        dw n_loadhigh, do_lh
        dw n_path, do_path
        dw n_prompt, do_prompt
        dw e_ayuda, do_help
        dw e_copiar, do_copy
        dw e_borrar, do_del
        dw e_renombrar, do_ren
        dw e_mostrar, do_type
        dw e_listar, do_dir
        dw e_limpiar, do_cls
        dw e_fecha, do_date
        dw e_hora, do_time
        dw e_version, do_ver
        dw e_memoria, do_mem
        dw e_volumen, do_vol
        dw e_salir, do_exit
        dw e_pausa, do_pause
        dw e_creardir, do_md
        dw e_borrardir, do_rd
        dw e_borrararbol, do_deltree
        dw e_llamar, do_call
        dw e_ruta, do_path
        dw e_eco, do_echo
        dw 0, 0

n_ver   db "VER", 0
n_cls   db "CLS", 0
n_dir   db "DIR", 0
n_type  db "TYPE", 0
n_cd    db "CD", 0
n_chdir db "CHDIR", 0
n_echo  db "ECHO", 0
n_set   db "SET", 0
n_mem   db "MEM", 0
n_date  db "DATE", 0
n_time  db "TIME", 0
n_vol   db "VOL", 0
n_help  db "HELP", 0
n_rem   db "REM", 0
n_exit  db "EXIT", 0
n_copy  db "COPY", 0
n_del   db "DEL", 0
n_erase db "ERASE", 0
n_ren   db "REN", 0
n_rename db "RENAME", 0
n_md    db "MD", 0
n_mkdir db "MKDIR", 0
n_rd    db "RD", 0
n_rmdir db "RMDIR", 0
n_deltree db "DELTREE", 0
n_pause db "PAUSE", 0
n_if    db "IF", 0
n_goto  db "GOTO", 0
n_for   db "FOR", 0
n_shift db "SHIFT", 0
n_call  db "CALL", 0
n_lh    db "LH", 0
n_loadhigh db "LOADHIGH", 0
n_path  db "PATH", 0
n_prompt db "PROMPT", 0
e_ayuda     db "AYUDA", 0
e_copiar    db "COPIAR", 0
e_borrar    db "BORRAR", 0
e_renombrar db "RENOMBRAR", 0
e_mostrar   db "MOSTRAR", 0
e_listar    db "LISTAR", 0
e_limpiar   db "LIMPIAR", 0
e_fecha     db "FECHA", 0
e_hora      db "HORA", 0
e_version   db "VERSION", 0
e_memoria   db "MEMORIA", 0
e_volumen   db "VOLUMEN", 0
e_salir     db "SALIR", 0
e_pausa     db "PAUSA", 0
e_creardir  db "CREARDIR", 0
e_borrardir db "BORRARDIR", 0
e_borrararbol db "BORRARARBOL", 0
e_llamar    db "LLAMAR", 0
e_ruta      db "RUTA", 0
e_eco       db "ECO", 0

; -----------------------------------------------------------------------------
;  Órdenes internas (SI = argumentos)
; -----------------------------------------------------------------------------
do_rem:
        ret

do_exit:
        cmp byte [primary], 0
        jne .r
        call xms_release
        mov al, [errlvl]
        mov ah, 0x4C
        int 0x21
.r:
        ret

do_ver:
        mov si, msg_ver
        call puts
        mov ah, 0x30
        int 0x21
        push ax
        movzx eax, al
        call putnum
        mov dl, '.'
        mov ah, 2
        int 0x21
        pop ax
        movzx eax, ah
        cmp al, 10
        jae .two
        push ax
        mov dl, '0'
        mov ah, 2
        int 0x21
        pop ax
.two:
        call putnum
        jmp crlf

do_cls:
        mov ax, 0x0600
        mov bh, 0x07
        xor cx, cx
        mov dx, 0x184F
        int 0x10
        mov ah, 2
        xor bh, bh
        xor dx, dx
        int 0x10
        ret

do_echo:
        ; «ECHO.», «ECHO,», «ECHO;» y «ECHO=»: el resto tal cual (o línea vacía)
        mov al, [si]
        cmp al, '.'
        je .sep
        cmp al, ','
        je .sep
        cmp al, ';'
        je .sep
        cmp al, '='
        jne .n
.sep:
        inc si
        jmp .print
.n:
        call skip_sp
        cmp byte [si], 0
        jne .arg
        mov si, msg_echoon
        cmp byte [echo_on], 0
        jne .st
        mov si, msg_echooff
.st:
        call puts
        jmp crlf
.arg:
        mov di, s_on
        call word_is
        jnc .on
        mov di, s_off
        call word_is
        jnc .off
.print:
        call puts
        jmp crlf
.on:
        mov byte [echo_on], 1
        ret
.off:
        mov byte [echo_on], 0
        ret

do_set:
        cmp byte [si], 0
        je .list
        ; NOMBRE=valor
        mov di, sname
        mov cx, 60
.nm:
        lodsb
        test al, al
        jz .syn
        cmp al, '='
        je .eq
        call upcase
        stosb
        loop .nm
.syn:
        mov si, msg_syntax
        jmp puts
.eq:
        ; quitar espacios al final del nombre
.tr:
        cmp di, sname
        je .syn
        cmp byte [di-1], ' '
        jne .tre
        dec di
        jmp .tr
.tre:
        mov byte [di], 0
        mov di, si
        mov si, sname
        jmp env_set
.list:
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .d
.c:
        mov dl, [es:di]
        inc di
        test dl, dl
        jz .e
        mov ah, 2
        int 0x21
        jmp .c
.e:
        call crlf
        jmp .l
.d:
        push ds
        pop es
        ret

do_cd:
        cmp byte [si], 0
        jne .chg
        call put_drive
        mov si, curdir
        mov dl, 0
        mov ah, 0x47
        int 0x21
        call puts
        jmp crlf
.chg:
        call cut_arg
        cmp byte [si+1], ':'
        jne .cd
        cmp byte [si+2], 0
        jne .cd
        ; «CD X:» muestra la carpeta actual de esa unidad
        mov al, [si]
        call upcase
        push ax
        mov dl, al
        sub dl, 'A' - 1
        mov si, curdir
        mov ah, 0x47
        int 0x21
        pop ax
        jnc .shw
        mov si, msg_baddrv
        jmp puts
.shw:
        call putc
        mov al, ':'
        call putc
        mov al, '\'
        call putc
        mov si, curdir
        call puts
        jmp crlf
.cd:
        mov dx, si
        mov ah, 0x3B
        int 0x21
        jnc .r
        mov si, msg_baddir
        call puts
.r:
        ret

do_type:
        cmp byte [si], 0
        jne .go
        mov si, msg_reqparam
        jmp puts
.go:
        call cut_arg
        mov dx, si
        mov ax, 0x3D00
        int 0x21
        jc .nf
        mov bx, ax
.l:
        mov dx, iobuf
        mov cx, 512
        mov ah, 0x3F
        int 0x21
        jc .cl
        test ax, ax
        jz .cl
        mov cx, ax
        push bx
        mov bx, 1
        mov ah, 0x40
        int 0x21
        pop bx
        jmp .l
.cl:
        mov ah, 0x3E
        int 0x21
        ret
.nf:
        mov si, msg_nofile
        jmp err_puts

do_vol:
        call arg_drive
        call crlf
        call print_label
        jmp crlf

; SI = argumento -> AL = letra de unidad ("X:" o la actual)
arg_drive:
        cmp byte [si], 0
        je .cur
        cmp byte [si+1], ':'
        jne .cur
        mov al, [si]
        call upcase
        ret
.cur:
        mov ah, 0x19
        int 0x21
        add al, 'A'
        ret

; escribir "X:\" de la unidad actual
put_drive:
        push ax
        mov ah, 0x19
        int 0x21
        add al, 'A'
        call putc
        mov al, ':'
        call putc
        mov al, '\'
        call putc
        pop ax
        ret

; AL = letra de la unidad
print_label:
        mov [s_lab], al
        mov [vol1_l], al
        mov [vol0_l], al
        call set_dta
        mov dx, s_lab
        mov cx, 8
        mov ah, 0x4E
        int 0x21
        jc .none
        mov si, msg_vol1
        call puts
        mov si, dta+0x1E
        mov di, lbl
.c:
        lodsb
        cmp al, '.'
        jne .c1
        ; nombre de la etiqueta hasta 8 con espacios, luego la «extensión»
.pad:
        cmp di, lbl+8
        jae .c
        mov byte [di], ' '
        inc di
        jmp .pad
.c1:
        mov [di], al
        inc di
        test al, al
        jnz .c
        mov si, lbl
        call puts
        jmp crlf
.none:
        mov si, msg_vol0
        call puts
        jmp crlf

set_dta:
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        ret

; -----------------------------------------------------------------------------
;  LISTAR / DIR [unidad:][ruta][archivo] [/P] [/W] [/A[[:]atributos]]
;               [/O[[:]orden]] [/S] [/B] [/L] [/C]
;   /P  pausa en cada pantalla        /W  formato ancho (5 columnas)
;   /A  atributos: D H S R A, «-» = sin ese atributo (/A solo = todos)
;   /O  orden: N E G S D, «-» = al revés (/O solo = G N)
;   /S  también las subcarpetas       /B  solo los nombres
;   /L  minúsculas                    /C, /CH  aceptadas, sin efecto
;   Las opciones van pegadas o no y en cualquier sitio; /-X anula /X.
;   DIRCMD (entorno) tiene las opciones por omisión.
; -----------------------------------------------------------------------------
DIR_MAX     equ 1500                ; entradas que caben en la tabla de /O
DE_SZ       equ 34                  ; bytes por entrada de la tabla
DE_11       equ 22                  ; nombre 8+3 dentro de la entrada
DIR_PTRS    equ DIR_MAX*DE_SZ       ; después, los punteros (dw) para ordenar
DIR_LVLS    equ 32                  ; niveles de subcarpetas con /S
DIR_DTA     equ 44                  ; DTA por nivel

do_dir:
        mov [d_args], si
        ; opciones por omisión: ninguna
        mov di, d_opts
        mov cx, d_opts_end - d_opts
        xor al, al
        rep stosb
        mov byte [d_arg], 0
        ; primero las de DIRCMD, luego las de la línea
        mov si, s_dircmd
        mov di, d_env
        call env_get
        jc .noenv
        mov byte [d_inenv], 1
        mov si, d_env
        call dir_opts
        jnc .noenv
        mov byte [d_inenv], 0
        mov si, msg_denv
        jmp puts
.noenv:
        mov byte [d_inenv], 0
        mov si, [d_args]
        call dir_opts
        jc .r
        ; /O: tabla en memoria propia (si no hay, se lista sin ordenar)
        mov word [d_seg], 0
        cmp byte [d_sort], 0
        je .go
        mov bx, (DIR_MAX * (DE_SZ + 2) + 15) / 16
        mov ah, 0x48
        int 0x21
        jc .go
        mov [d_seg], ax
.go:
        call dir_main
        call dir_free
.r:
        ret

; liberar la tabla de /O (también tras Ctrl+C)
dir_free:
        cmp word [d_seg], 0
        je .r
        push es
        mov es, [d_seg]
        mov ah, 0x49
        int 0x21
        pop es
        mov word [d_seg], 0
.r:
        ret

; ¿fin de palabra? (espacio, tabulador, «/» o fin) -> ZF
d_isend:
        cmp al, ' '
        je .r
        cmp al, 9
        je .r
        cmp al, '/'
        je .r
        test al, al
.r:
        ret

; SI = opciones (y el patrón) -> d_opts, d_arg; CF y mensaje si hay error
dir_opts:
.l:
        call skip_sp
        lodsb
        test al, al
        jz .ok
        cmp al, '/'
        je .sw
        ; una palabra: el patrón
        dec si
        mov [d_tok], si
        cmp byte [d_arg], 0
        jne .many
        mov di, d_arg
.w:
        lodsb
        call d_isend
        je .we
        cmp di, d_arg + 127
        jae .w
        stosb
        jmp .w
.we:
        dec si
        mov byte [di], 0
        cmp byte [d_inenv], 0
        je .l
        mov byte [d_arg], 0         ; en DIRCMD solo cuentan las opciones
        jmp .l
.many:
        mov si, msg_dmany
        jmp .err
.sw:
        lea bx, [si-1]
        mov [d_tok], bx
        mov dl, 1                   ; 1 = activar, 0 = «/-» (anular)
        cmp byte [si], '-'
        jne .s1
        inc si
        mov dl, 0
.s1:
        lodsb
        call upcase
        cmp al, 'A'
        je .sa
        cmp al, 'O'
        je .so
        cmp al, 'C'
        je .sc
        ; las de una letra: P W S B L
        mov bx, d_ftab
.ft:
        cmp byte [bx], 0
        je .bad
        cmp al, [bx]
        je .ff
        add bx, 3
        jmp .ft
.ff:
        mov al, [si]
        call d_isend
        jne .bad
        mov di, [bx+1]
        mov [di], dl
        jmp .l
.sc:
        ; /C y /CH (compresión): sin efecto
        mov al, [si]
        call upcase
        cmp al, 'H'
        jne .end
        inc si
.end:
        mov al, [si]
        call d_isend
        jne .bad
        jmp .l
        ; /A[[:]atributos]
.sa:
        mov byte [d_aset], 0
        mov byte [d_aclr], 0
        mov [d_attr], dl
        test dl, dl
        jz .end
        cmp byte [si], ':'
        jne .sal
        inc si
.sal:
        mov al, [si]
        call d_isend
        je .l
        inc si
        xor dh, dh
        cmp al, '-'
        jne .sa2
        mov dh, 1
        lodsb
.sa2:
        call upcase
        mov bx, d_atab
.sa3:
        cmp byte [bx], 0
        je .bad
        cmp al, [bx]
        je .sa4
        add bx, 2
        jmp .sa3
.sa4:
        mov ah, [bx+1]
        test dh, dh
        jnz .sa5
        or [d_aset], ah
        not ah
        and [d_aclr], ah
        jmp .sal
.sa5:
        or [d_aclr], ah
        not ah
        and [d_aset], ah
        jmp .sal
        ; /O[[:]orden]
.so:
        mov byte [d_nkeys], 0
        mov [d_sort], dl
        test dl, dl
        jz .end
        cmp byte [si], ':'
        jne .sol
        inc si
.sol:
        mov al, [si]
        call d_isend
        je .soe
        inc si
        xor dh, dh
        cmp al, '-'
        jne .so2
        mov dh, 0x80
        lodsb
.so2:
        call upcase
        cmp al, 'C'                 ; por compresión: sin efecto
        je .sol
        mov bx, s_okeys
.so3:
        cmp byte [bx], 0
        je .bad
        cmp al, [bx]
        je .so4
        inc bx
        jmp .so3
.so4:
        movzx bx, byte [d_nkeys]
        cmp bx, 8
        jae .sol
        or al, dh
        mov [d_keys+bx], al
        inc byte [d_nkeys]
        jmp .sol
.soe:
        cmp byte [d_nkeys], 0
        jne .l
        mov word [d_keys], 'G' | ('N' << 8)
        mov byte [d_nkeys], 2
        jmp .l
.bad:
        mov si, msg_dbad
.err:
        call puts
        mov si, [d_tok]
        call dir_puttok
        stc
        ret
.ok:
        clc
        ret

; escribir la palabra de SI (la opción o el parámetro que sobra)
dir_puttok:
        lodsb
        call putc
.l:
        lodsb
        call d_isend
        je .e
        call putc
        jmp .l
.e:
        jmp crlf

dir_main:
        ; construir el patrón
        mov si, d_arg
        mov di, pattern
        cmp byte [si], 0
        jne .copy
        mov si, s_all
.copy:
        lodsb
        stosb
        test al, al
        jnz .copy
        dec di
        mov [pat_end], di
        ; ".COM" -> "*.COM" (DIR.COM, DIR .TXT): nombre vacío = todos
        mov si, pattern
        mov bx, si
.lc:
        lodsb
        test al, al
        jz .lcd
        cmp al, '\'
        je .sl
        cmp al, ':'
        jne .lc
.sl:
        mov bx, si
        jmp .lc
.lcd:
        cmp byte [bx], '.'
        jne .nodot
        cmp byte [bx+1], 0
        je .nodot
        cmp byte [bx+1], '.'
        je .nodot
        mov si, [pat_end]
.sh:
        mov al, [si]
        mov [si+1], al
        cmp si, bx
        je .ins
        dec si
        jmp .sh
.ins:
        mov byte [bx], '*'
        inc word [pat_end]
        mov di, [pat_end]
.nodot:
        mov al, [di-1]
        cmp al, '\'
        je .addall
        cmp al, ':'
        je .addall
        ; ¿es un directorio sin comodines?
        mov si, pattern
.wc:
        lodsb
        test al, al
        jz .nowc
        cmp al, '*'
        je .ready
        cmp al, '?'
        je .ready
        jmp .wc
.nowc:
        call set_dta
        mov dx, pattern
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .ready
        test byte [dta+0x15], 0x10
        jz .ready
.addall:
        mov di, [pat_end]
        cmp byte [di-1], '\'
        je .a2
        cmp byte [di-1], ':'
        je .a2
        mov al, '\'
        stosb
.a2:
        mov si, s_all
.a3:
        lodsb
        stosb
        test al, al
        jnz .a3
.ready:
        mov si, pattern
        mov di, truebuf
        mov ah, 0x60
        int 0x21
        jnc .tn
        mov si, msg_baddrv
        jmp err_puts
.tn:
        ; carpeta (d_path) y nombre (d_spec)
        mov si, truebuf
        mov di, si
.ls:
        lodsb
        test al, al
        jz .lse
        cmp al, '\'
        jne .ls
        mov di, si
        jmp .ls
.lse:
        mov si, di
        mov bx, d_spec
.cs:
        lodsb
        mov [bx], al
        inc bx
        test al, al
        jnz .cs
        cmp di, truebuf+3
        jbe .root
        dec di
.root:
        mov byte [di], 0
        mov si, truebuf
        mov di, d_path
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        xor eax, eax
        mov [nfiles], eax
        mov [nbytes], eax
        mov [d_tfiles], eax
        mov [d_tbytes], eax
        mov byte [d_lines], 0
        mov byte [d_lvl], 0
        cmp byte [d_bare], 0
        jne .nohdr
        call crlf
        mov al, [truebuf]
        call print_label
        mov byte [d_lines], 2
        cmp byte [d_sub], 0
        jne .nohdr
        call dir_header
.nohdr:
        cmp byte [d_sub], 0
        jne .tree
        call dir_list
        cmp dword [nfiles], 0
        je .empty
        cmp byte [d_bare], 0
        jne .r
        call dir_sum
        jmp dir_freeln
.tree:
        call dir_tree
        cmp dword [d_tfiles], 0
        je .empty
        cmp byte [d_bare], 0
        jne .r
        mov si, msg_dtotal
        call puts
        call d_nl
        mov eax, [d_tfiles]
        mov [nfiles], eax
        mov eax, [d_tbytes]
        mov [nbytes], eax
        call dir_sum
        jmp dir_freeln
.empty:
        mov si, msg_nofile
        jmp puts
.r:
        ret

; " Directorio de X" y una línea en blanco
dir_header:
        mov si, msg_dirof
        call puts
        mov si, d_path
        call puts
        call d_nl
        jmp d_nl

; "n archivo(s)  n bytes"
dir_sum:
        mov eax, [nfiles]
        mov cl, 9
        call putnum_w
        mov si, msg_files
        call puts
        mov eax, [nbytes]
        mov cl, 15
        call putnum_w
        mov si, msg_bytes
        call puts
        jmp d_line

; "n bytes libres" de la unidad listada
dir_freeln:
        mov ah, 0x36
        mov dl, [truebuf]
        sub dl, 'A' - 1
        int 0x21
        movzx eax, ax
        movzx ebx, bx
        movzx ecx, cx
        mul ebx
        mul ecx
        mov cl, 25
        call putnum_w
        mov si, msg_free
        call puts
        jmp d_line

; salto de línea de DIR (cuenta las líneas para /P)
d_nl:
        call crlf
d_line:
        cmp byte [d_page], 0
        je .r
        inc byte [d_lines]
        cmp byte [d_lines], 23
        jb .r
        push si
        push ax
        mov si, msg_pause
        call puts
        mov ah, 0x08
        int 0x21
        call crlf
        mov byte [d_lines], 0
        cmp byte [d_bare], 0
        jne .p
        mov si, msg_dcont
        call puts
        mov si, d_path
        call puts
        mov al, ')'
        call putc
        call crlf
        mov byte [d_lines], 1
.p:
        pop ax
        pop si
.r:
        ret

; carácter de un nombre (en minúsculas con /L)
d_putn:
        cmp byte [d_lower], 0
        je .p
        cmp al, 'A'
        jb .p
        cmp al, 'Z'
        ja .p
        add al, 32
.p:
        jmp putc

; SI = carpeta -> copiada en DI con «\» al final; DI = fin
d_join:
        lodsb
        stosb
        test al, al
        jnz d_join
        dec di
        cmp byte [di-1], '\'
        je .r
        mov byte [di], '\'
        inc di
.r:
        ret

; ¿la entrada de la DTA pasa el filtro de /A (y de /B)? CF si no
dir_ok:
        mov al, [dta+0x15]
        cmp byte [d_attr], 0
        je .b
        mov ah, al
        and ah, [d_aset]
        cmp ah, [d_aset]
        jne .no
        test al, [d_aclr]
        jnz .no
.b:
        cmp byte [d_bare], 0
        je .ok
        cmp byte [dta+0x1E], '.'    ; con /B, sin «.» ni «..»
        je .no
.ok:
        clc
        ret
.no:
        stc
        ret

; listar los archivos de d_path que encajan con d_spec
dir_list:
        xor eax, eax
        mov [nfiles], eax
        mov [nbytes], eax
        mov byte [d_col], 0
        mov si, d_path
        mov di, d_srch
        call d_join
        mov [d_sname], di
        mov si, d_spec
.cs:
        lodsb
        stosb
        test al, al
        jnz .cs
        call set_dta
        mov word [d_sattr], 0x10
        cmp byte [d_attr], 0
        je .a
        mov word [d_sattr], 0x16    ; con /A, también ocultos y de sistema
.a:
        cmp word [d_seg], 0
        je .stream
        ; leer todas en la tabla
        mov fs, [d_seg]
        mov word [d_n], 0
        mov dx, d_srch
        mov cx, [d_sattr]
        mov ah, 0x4E
        int 0x21
        jc .done
.tl:
        call dir_ok
        jc .tn
        mov bx, [d_n]
        cmp bx, DIR_MAX
        jae .stream                 ; no caben: sin ordenar
        imul di, bx, DE_SZ
        shl bx, 1
        mov [fs:bx+DIR_PTRS], di
        inc word [d_n]
        mov si, dta+0x15
        mov cx, 22
.c1:
        lodsb
        mov [fs:di], al
        inc di
        loop .c1
        call d_mk11
        mov si, nm11
        mov cx, 11
.c2:
        lodsb
        mov [fs:di], al
        inc di
        loop .c2
.tn:
        mov ah, 0x4F
        int 0x21
        jnc .tl
        call dir_sortt
        xor bx, bx
.pl:
        cmp bx, [d_n]
        jae .done
        push bx
        shl bx, 1
        mov si, [fs:bx+DIR_PTRS]
        mov di, dta+0x15
        mov cx, 22
.c3:
        mov al, [fs:si]
        inc si
        mov [di], al
        inc di
        loop .c3
        call dir_out
        pop bx
        inc bx
        jmp .pl
.stream:
        mov dx, d_srch
        mov cx, [d_sattr]
        mov ah, 0x4E
        int 0x21
        jc .done
.sl:
        call dir_ok
        jc .sn
        call dir_out
.sn:
        mov ah, 0x4F
        int 0x21
        jnc .sl
.done:
        ; /W: terminar la última fila
        cmp byte [d_col], 0
        je .r
        mov byte [d_col], 0
        call d_nl
.r:
        ret

; ordenar la tabla (inserción, estable) según d_keys
dir_sortt:
        mov bx, 1
.i:
        cmp bx, [d_n]
        jae .r
        push bx
        shl bx, 1
        mov dx, [fs:bx+DIR_PTRS]
.j:
        test bx, bx
        jz .put
        mov si, [fs:bx+DIR_PTRS-2]
        mov di, dx
        call d_cmp
        cmp al, 0
        jle .put
        mov [fs:bx+DIR_PTRS], si
        sub bx, 2
        jmp .j
.put:
        mov [fs:bx+DIR_PTRS], dx
        pop bx
        inc bx
        jmp .i
.r:
        ret

; FS:SI y FS:DI = entradas -> AL = 1 si SI va después, -1 si antes, 0 igual
d_cmp:
        push bx
        push cx
        xor bx, bx
.k:
        cmp bl, [d_nkeys]
        jae .eq
        mov ah, [d_keys+bx]
        mov al, ah
        and al, 0x7F
        cmp al, 'N'
        je .kn
        cmp al, 'E'
        je .ke
        cmp al, 'G'
        je .kg
        cmp al, 'S'
        je .ks
        ; D: fecha y hora juntas (la hora en la palabra baja)
        mov ecx, [fs:si+1]
        cmp ecx, [fs:di+1]
        jmp .res
.ks:
        mov ecx, [fs:si+5]
        cmp ecx, [fs:di+5]
        jmp .res
.kg:
        ; carpetas primero
        mov cl, [fs:si]
        and cl, 0x10
        mov ch, [fs:di]
        and ch, 0x10
        cmp ch, cl
        jmp .res
.kn:
        mov cx, 8
        push si
        push di
        add si, DE_11
        add di, DE_11
        jmp .fc
.ke:
        mov cx, 3
        push si
        push di
        add si, DE_11 + 8
        add di, DE_11 + 8
.fc:
        mov al, [fs:si]
        cmp al, [fs:di]
        jne .fce
        inc si
        inc di
        loop .fc
        cmp al, al
.fce:
        pop di
        pop si
.res:
        je .nx
        mov al, 1
        ja .sg
        mov al, -1
.sg:
        test ah, 0x80
        jz .out
        neg al
        jmp .out
.nx:
        inc bx
        jmp .k
.eq:
        xor al, al
.out:
        pop cx
        pop bx
        ret

; una entrada (en la DTA): contar y escribir en el formato pedido
dir_out:
        cmp byte [d_sub], 0
        je .c
        cmp byte [d_bare], 0
        jne .c
        cmp dword [nfiles], 0
        jne .c
        call dir_header             ; con /S, solo las carpetas con algo
.c:
        inc dword [nfiles]
        inc dword [d_tfiles]
        test byte [dta+0x15], 0x10
        jnz .nd
        mov eax, [dta+0x1A]
        add [nbytes], eax
        add [d_tbytes], eax
.nd:
        cmp byte [d_bare], 0
        jne dir_bare
        cmp byte [d_wide], 0
        jne dir_wide
        jmp dir_line

; /B: NOMBRE.EXT (con /S, con la ruta)
dir_bare:
        cmp byte [d_sub], 0
        je .n
        mov si, d_srch
.p:
        cmp si, [d_sname]
        jae .n
        lodsb
        call d_putn
        jmp .p
.n:
        mov si, dta+0x1E
.l:
        lodsb
        test al, al
        jz .e
        call d_putn
        jmp .l
.e:
        jmp d_nl

; /W: 5 columnas de 15, las carpetas entre corchetes
dir_wide:
        xor cx, cx
        test byte [dta+0x15], 0x10
        jz .n
        mov al, '['
        call putc
        inc cx
.n:
        mov si, dta+0x1E
.l:
        lodsb
        test al, al
        jz .e
        call d_putn
        inc cx
        jmp .l
.e:
        test byte [dta+0x15], 0x10
        jz .p
        mov al, ']'
        call putc
        inc cx
.p:
        inc byte [d_col]
        cmp byte [d_col], 5
        jae .nl
        mov al, ' '
.pad:
        cmp cx, 15
        jae .r
        call putc
        inc cx
        jmp .pad
.r:
        ret
.nl:
        mov byte [d_col], 0
        jmp d_nl

; /S: d_path y sus subcarpetas
dir_tree:
        call dir_list
        cmp dword [nfiles], 0
        je .sub
        cmp byte [d_bare], 0
        jne .sub
        call dir_sum
        call d_nl
.sub:
        cmp byte [d_lvl], DIR_LVLS
        jae .r
        call d_lvldta
        mov ah, 0x1A
        int 0x21
        mov si, d_path
        mov di, d_srch
        call d_join
        mov si, s_all
.cs:
        lodsb
        stosb
        test al, al
        jnz .cs
        mov dx, d_srch
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .r
.l:
        call d_lvldta
        mov bx, dx
        test byte [bx+0x15], 0x10
        jz .n
        cmp byte [bx+0x1E], '.'
        je .n
        ; d_path\NOMBRE
        mov di, d_path
.e:
        cmp byte [di], 0
        je .ee
        inc di
        jmp .e
.ee:
        push di
        cmp byte [di-1], '\'
        je .j
        mov byte [di], '\'
        inc di
.j:
        lea si, [bx+0x1E]
.jc:
        lodsb
        stosb
        test al, al
        jnz .jc
        inc byte [d_lvl]
        call dir_tree
        dec byte [d_lvl]
        pop di
        mov byte [di], 0
        call d_lvldta
        mov ah, 0x1A
        int 0x21
.n:
        mov ah, 0x4F
        int 0x21
        jnc .l
.r:
        ret

; DX = DTA del nivel d_lvl
d_lvldta:
        movzx dx, byte [d_lvl]
        imul dx, dx, DIR_DTA
        add dx, d_dtas
        ret

; nombre de la DTA -> nm11 (8+3, con espacios)
d_mk11:
        push ax
        push cx
        push si
        push di
        mov di, nm11
        mov cx, 11
        mov al, ' '
        rep stosb
        mov si, dta+0x1E
        mov di, nm11
        cmp byte [si], '.'
        je .dots
.n:
        lodsb
        test al, al
        jz .out
        cmp al, '.'
        je .x
        stosb
        jmp .n
.x:
        mov di, nm11+8
.e:
        lodsb
        test al, al
        jz .out
        stosb
        jmp .e
.dots:
        lodsb
        test al, al
        jz .out
        stosb
        jmp .dots
.out:
        pop di
        pop si
        pop cx
        pop ax
        ret

dir_line:
        call d_mk11
        mov si, nm11
        mov cx, 8
.p1:
        lodsb
        call d_putn
        loop .p1
        mov al, ' '
        call putc
        mov cx, 3
.p2:
        lodsb
        call d_putn
        loop .p2
.size:
        test byte [dta+0x15], 0x10
        jz .file
        mov si, s_dirtag
        call puts
        jmp .date
.file:
        mov eax, [dta+0x1A]
        mov cl, 14
        call putnum_w
.date:
        mov al, ' '
        call putc
        mov ax, [dta+0x18]
        mov bx, ax
        and ax, 31
        call put2                   ; día
        mov al, '-'
        call putc
        mov ax, bx
        shr ax, 5
        and ax, 15
        call put2                   ; mes
        mov al, '-'
        call putc
        mov ax, bx
        shr ax, 9
        add ax, 80
        xor dx, dx
        mov cx, 100
        div cx
        mov ax, dx
        call put2                   ; año
        ; hora de 24 horas
        mov ax, [dta+0x16]
        shr ax, 11
        movzx eax, ax
        mov cl, 4
        call putnum_w
        mov al, ':'
        call putc
        mov ax, [dta+0x16]
        shr ax, 5
        and ax, 63
        call put2
        jmp d_nl

; -----------------------------------------------------------------------------
;  MEM [/C | /F | /D | /M nombre] [/P]
;   /C (/CLASSIFY)  programas en memoria y lo que usa cada uno
;   /F (/FREE)      bloques libres
;   /M (/MODULE)    bloques de un programa
;   /D (/DEBUG)     todos los bloques y los controladores de dispositivo
;   /P (/PAGE)      pausa en cada pantalla
; -----------------------------------------------------------------------------
MM_MAXN     equ 20                  ; nombres distintos en /C
; (con la memoria superior enlazada mientras dura, para que salga en las
; listas)
do_mem:
        push si
        mov ax, 0x5802
        int 0x21
        mov [lh_link], al
        mov byte [mm_hasumb], 0
        mov ax, 0x5803
        mov bx, 1
        int 0x21
        jc .nu
        mov byte [mm_hasumb], 1
.nu:
        pop si
        call do_mem2
        mov ax, 0x5803
        movzx bx, byte [lh_link]
        int 0x21
        ret
do_mem2:
        mov byte [mm_mode], 0
        mov byte [mm_page], 0
        mov byte [mm_lines], 0
.opt:
        call skip_sp
        lodsb
        test al, al
        jz .optd
        cmp al, '/'
        jne .bad
        lodsb
        call upcase
        cmp al, 'P'
        jne .o1
        mov byte [mm_page], 1
        jmp .sk
.o1:
        cmp al, 'C'
        je .mode
        cmp al, 'F'
        je .mode
        cmp al, 'D'
        je .mode
        cmp al, 'M'
        jne .bad
        mov [mm_mode], al
        ; resto de la palabra, «:» o espacios y el nombre
.mw:
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .mn
        cmp al, 'Z'
        ja .mn
        inc si
        jmp .mw
.mn:
        cmp byte [si], ':'
        jne .mn2
        inc si
.mn2:
        call skip_sp
        mov di, mm_name
        mov cx, 8
        mov eax, '    '
        mov [di], eax
        mov [di+4], eax
.mc:
        mov al, [si]
        cmp al, ' '
        jbe .mce
        cmp al, '/'
        je .mce
        cmp al, '.'
        je .mcx
        call upcase
        mov [di], al
        inc di
        inc si
        loop .mc
        jmp .mce
.mcx:
        ; la extensión no cuenta (COMMAND.COM = COMMAND)
        inc si
        mov al, [si]
        cmp al, ' '
        jbe .mce
        cmp al, '/'
        je .mce
        jmp .mcx
.mce:
        cmp byte [mm_name], ' '
        je .bad
        jmp .opt
.mode:
        mov [mm_mode], al
.sk:
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .opt
        cmp al, 'Z'
        ja .opt
        inc si
        jmp .sk
.bad:
        mov si, msg_mmbad
        jmp puts
.optd:
        mov al, [mm_mode]
        cmp al, 'C'
        je mem_classify
        cmp al, 'F'
        je mem_free
        cmp al, 'D'
        je mem_debug
        cmp al, 'M'
        je mem_module

mem_summary:
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        push ds
        pop es
        xor ebx, ebx            ; libre total
        xor edx, edx            ; mayor libre
        xor edi, edi            ; fin
        mov dword [mu_tot], 0
        mov dword [mu_free], 0
        mov dword [mu_large], 0
.l:
        cmp ax, 0xA000
        jae .upper
        call mm_isbridge
        je .upper
        push ds
        mov ds, ax
        mov cl, [0]
        movzx esi, word [3]
        cmp word [1], 0
        pop ds
        jne .used
        mov ebp, esi
        shl ebp, 4
        add ebx, ebp
        mov [mm_prev], ebp
        cmp ebp, edx
        jbe .cont
        mov edx, ebp
        jmp .cont
.used:
        ; el cuerpo del intérprete (al final de la memoria) cuenta como
        ; libre: se quita de en medio mientras corre un programa
        cmp byte [r_swap], 0
        je .nb
        push ax
        push es
        inc ax
        mov es, [r_psp]
        cmp ax, [es:r_body]
        pop es
        pop ax
        jne .nb
        mov ebp, esi
        inc ebp
        shl ebp, 4
        add ebx, ebp
        add ebp, [mm_prev]
        cmp ebp, edx
        jbe .nb
        mov edx, ebp
.nb:
        mov dword [mm_prev], 0
.cont:
        movzx edi, ax
        add edi, esi
        inc edi
        cmp cl, 'Z'
        je .done
        cmp cl, 'M'
        jne .done
        add ax, si
        inc ax
        jmp .l
; memoria superior (UMB): los puentes (dueño 8, «SC») no cuentan
.upper:
        push ds
        mov ds, ax
        mov cl, [0]
        movzx esi, word [3]
        mov bp, [1]
        cmp dword [8], 'SC'
        pop ds
        jne .u1
        cmp bp, 8
        je .unext
.u1:
        push eax
        mov eax, esi
        inc eax
        shl eax, 4
        add [mu_tot], eax
        test bp, bp
        jnz .u2
        mov eax, esi
        shl eax, 4
        add [mu_free], eax
        cmp eax, [mu_large]
        jbe .u2
        mov [mu_large], eax
.u2:
        pop eax
.unext:
        cmp cl, 'M'
        jne .done
        add ax, si
        inc ax
        jmp .l
.done:
        cmp dword [mu_tot], 0
        je .noupper
        push edx
        push ebx
        push edi
        call .conv
        pop edi
        pop ebx
        pop edx
        call crlf
        mov eax, [mu_tot]
        mov cl, 12
        call putnum_w
        mov si, msg_mutotal
        call puts
        mov eax, [mu_free]
        mov cl, 12
        call putnum_w
        mov si, msg_mfree
        call puts
        mov eax, [mu_large]
        mov cl, 12
        call putnum_w
        mov si, msg_mularge
        jmp puts
.noupper:
.conv:
        push edx
        push ebx
        shl edi, 4
        call crlf
        mov eax, edi
        mov cl, 12
        call putnum_w
        mov si, msg_mtotal
        call puts
        pop eax
        mov cl, 12
        call putnum_w
        mov si, msg_mfree
        call puts
        pop eax
        mov cl, 12
        call putnum_w
        mov si, msg_mlarge
        call puts
        ; ¿el DOS está en la HMA? (INT 21h AX=3306h: bit 4 de DH)
        mov ax, 0x3306
        xor dx, dx
        int 0x21
        test dh, 0x10
        jz .r
        mov si, msg_mhma
        call puts
.r:
        ret

; ---- ayudas de MEM ----
; salto de línea con pausa en cada pantalla si /P
mm_nl:
        call crlf
        cmp byte [mm_page], 0
        je .r
        inc byte [mm_lines]
        cmp byte [mm_lines], 22
        jb .r
        mov byte [mm_lines], 0
        push si
        mov si, msg_mmmore
        call puts
        pop si
        push ax
        mov ah, 0x08
        int 0x21
        pop ax
        call crlf
.r:
        ret
; AX -> 4 cifras hexadecimales
mm_hex4:
        push ax
        push cx
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        pop ax
        ret
; EAX bytes -> «nnn.nnn (nnnK)» en CL columnas para los bytes
mm_bytes:
        push eax
        call putnum_w
        mov al, ' '
        call putc
        mov al, '('
        call putc
        pop eax
        push eax
        add eax, 512
        shr eax, 10
        mov cl, 0
        call putnum_w
        mov al, 'K'
        call putc
        mov al, ')'
        call putc
        pop eax
        ret
; mm_nbuf (8) y relleno hasta 10 columnas
mm_name8:
        push si
        push cx
        mov si, mm_nbuf
        mov cx, 8
.l:
        lodsb
        call putc
        loop .l
        mov al, ' '
        call putc
        call putc
        pop cx
        pop si
        ret
; mm_nbuf sin los espacios del final, y un espacio
mm_nametrim:
        push si
        push cx
        mov si, mm_nbuf
        mov cx, 8
.l:
        lodsb
        cmp al, ' '
        je .e
        call putc
        loop .l
.e:
        mov al, ' '
        call putc
        pop cx
        pop si
        ret
; aviso de que no hay memoria superior (si no la hay)
mm_noumb:
        cmp byte [mm_hasumb], 0
        jne .r
        mov si, msg_mmnoumb
        call puts
.r:
        ret
; MCB AX: ZF = 1 si es un «puente» de la memoria superior (dueño 8, «SC»):
; tapa el hueco entre la memoria convencional y un UMB, o entre dos UMB
mm_isbridge:
        push es
        mov es, ax
        cmp word [es:1], 8
        jne .r
        cmp dword [es:8], 'SC'
.r:
        pop es
        ret
; primer MCB -> AX
mm_first:
        push es
        push bx
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        pop bx
        pop es
        ret
; MCB AX -> mm_nbuf = nombre, BL = tipo (0 libre, 1 sistema, 2 programa,
; 3 entorno, 4 datos), ECX = bytes (con la cabecera), DL = 'M' o 'Z'
mm_class:
        push es
        push si
        push di
        push ax
        mov es, ax
        mov dl, [es:0]
        movzx ecx, word [es:3]
        inc ecx
        shl ecx, 4
        mov si, [es:1]                  ; propietario
        mov dword [mm_nbuf], '    '
        mov dword [mm_nbuf+4], '    '
        test si, si
        jnz .own
        mov bl, 0
        mov di, s_mmfree
        jmp .setn
.own:
        cmp si, 8
        jbe .sys
        inc ax
        cmp si, ax
        jne .other
        mov bl, 2                       ; el bloque del propio programa
        jmp .mname
.other:
        ; de otro programa: el nombre está en el MCB de su PSP
        mov bl, 4
        push es
        mov es, si
        cmp [es:0x2C], ax               ; ¿es su entorno?
        pop es
        jne .o2
        mov bl, 3
.o2:
        mov ax, si
        dec ax
        mov es, ax
        cmp word [es:1], 8
        jbe .sysn
.mname:
        ; nombre del MCB (+8, hasta 8 caracteres)
        xor di, di
.mc:
        mov al, [es:8+di]
        cmp al, ' '
        jbe .mce
        mov [mm_nbuf+di], al
        inc di
        cmp di, 8
        jb .mc
.mce:
        test di, di
        jnz .done
        mov byte [mm_nbuf], '?'
        jmp .done
.sysn:
        mov di, s_mmsys
        jmp .setn
.sys:
        mov bl, 1
        mov di, s_mmsys
.setn:
        push bx
        xor bx, bx
.sn:
        mov al, [di+bx]
        test al, al
        jz .sne
        mov [mm_nbuf+bx], al
        inc bx
        jmp .sn
.sne:
        pop bx
.done:
        pop ax
        pop di
        pop si
        pop es
        ret
; tipo BL -> SI = texto
mm_typename:
        movzx si, bl
        shl si, 1
        mov si, [mm_types+si]
        ret

; ---- MEM /C ----
mem_classify:
        mov byte [mm_up], 0
        mov word [mm_upseg], 0
        mov word [mm_n], 0
        ; el núcleo y las tablas de la BIOS, antes del primer MCB
        call mm_first
        push ax
        movzx ecx, ax
        shl ecx, 4
        mov dword [mm_nbuf], 'SIST'
        mov dword [mm_nbuf+4], 'EMA '
        call mm_add
        pop ax
.l:
        call mm_isbridge
        jne .norm
        ; un puente: en la primera pasada, ahí acaba la memoria convencional
        push es
        mov es, ax
        mov dl, [es:0]
        add ax, [es:3]
        inc ax
        pop es
        cmp byte [mm_up], 0
        jne .skip
        mov [mm_upseg], ax
        jmp .show
.skip:
        cmp dl, 'M'
        je .l
        jmp .show
.norm:
        push ax
        call mm_class
        call mm_add
        pop ax
        cmp dl, 'M'
        jne .show
        push es
        mov es, ax
        add ax, [es:3]
        pop es
        inc ax
        jmp .l
.show:
        call mm_nl
        mov si, msg_mmc1
        cmp byte [mm_up], 0
        je .t1
        mov si, msg_mmc1u
.t1:
        call puts
        call mm_nl
        call mm_nl
        mov si, msg_mmc2
        call puts
        call mm_nl
        mov si, msg_mmc3
        call puts
        call mm_nl
        xor bx, bx
        xor edx, edx
.p:
        cmp bx, [mm_n]
        jae .tot
        imul si, bx, 12
        add si, mm_tab
        mov eax, [si]
        mov [mm_nbuf], eax
        mov eax, [si+4]
        mov [mm_nbuf+4], eax
        mov al, ' '
        call putc
        call putc
        call mm_name8
        mov eax, [si+8]
        add edx, eax
        mov cl, 12
        call mm_bytes
        call mm_nl
        inc bx
        jmp .p
.tot:
        mov si, msg_mmc3
        call puts
        call mm_nl
        mov si, msg_mmtot
        call puts
        mov eax, edx
        mov cl, 12
        call mm_bytes
        call mm_nl
        ; segunda pasada: la memoria superior
        cmp byte [mm_up], 0
        jne .fin
        mov ax, [mm_upseg]
        test ax, ax
        jz .fin
        mov byte [mm_up], 1
        mov word [mm_n], 0
        jmp .l
.fin:
        call mm_nl
        call mm_noumb
        jmp mm_nl
; sumar ECX bytes al nombre de mm_nbuf
mm_add:
        push bx
        push si
        push eax
        xor bx, bx
.f:
        cmp bx, [mm_n]
        jae .new
        imul si, bx, 12
        add si, mm_tab
        mov eax, [mm_nbuf]
        cmp eax, [si]
        jne .n
        mov eax, [mm_nbuf+4]
        cmp eax, [si+4]
        jne .n
        add [si+8], ecx
        jmp .r
.n:
        inc bx
        jmp .f
.new:
        cmp bx, MM_MAXN
        jae .r
        imul si, bx, 12
        add si, mm_tab
        mov eax, [mm_nbuf]
        mov [si], eax
        mov eax, [mm_nbuf+4]
        mov [si+4], eax
        mov [si+8], ecx
        inc word [mm_n]
.r:
        pop eax
        pop si
        pop bx
        ret

; ---- MEM /F ----
mem_free:
        call mm_nl
        mov si, msg_mmf1
        call puts
        call mm_nl
        call mm_nl
        mov si, msg_mmf2
        call puts
        call mm_nl
        mov si, msg_mmf3
        call puts
        call mm_nl
        xor ebp, ebp                    ; total
        xor edi, edi                    ; mayor
        call mm_first
.l:
        push ax
        call mm_class
        test bl, bl
        jnz .n
        pop ax
        push ax
        mov si, s_mmind
        call puts
        call mm_hex4
        mov si, s_mmind
        call puts
        add ebp, ecx
        cmp ecx, edi
        jbe .b
        mov edi, ecx
.b:
        mov eax, ecx
        mov cl, 12
        call mm_bytes
        call mm_nl
.n:
        pop ax
        push es
        mov es, ax
        cmp byte [es:0], 'M'
        jne .end
        add ax, [es:3]
        pop es
        inc ax
        jmp .l
.end:
        pop es
        call mm_nl
        mov si, msg_mmftot
        call puts
        mov eax, ebp
        mov cl, 12
        call mm_bytes
        call mm_nl
        mov si, msg_mmfbig
        call puts
        mov eax, edi
        mov cl, 12
        call mm_bytes
        call mm_nl
        call mm_nl
        call mm_noumb
        jmp mm_nl

; ---- MEM /M nombre ----
mem_module:
        mov byte [mm_found], 0
        xor edx, edx
        call mm_first
.l:
        push ax
        push edx
        call mm_class
        pop edx
        mov [mm_sz], ecx
        mov si, mm_nbuf
        mov di, mm_name
        mov cx, 8
        push es
        push ds
        pop es
        repe cmpsb
        pop es
        jne .n
        cmp byte [mm_found], 0
        jne .row
        mov byte [mm_found], 1
        call mm_nl
        mov si, msg_mmm1
        call puts
        call mm_nametrim
        mov si, msg_mmm2
        call puts
        call mm_nl
        call mm_nl
        mov si, msg_mmm3
        call puts
        call mm_nl
        mov si, msg_mmm4
        call puts
        call mm_nl
.row:
        pop ax
        push ax
        mov si, s_mmind
        call puts
        call mm_hex4
        mov si, s_mmind
        call puts
        mov ecx, [mm_sz]
        add edx, ecx
        mov eax, ecx
        mov cl, 12
        call mm_bytes
        mov al, ' '
        call putc
        call putc
        call mm_typename
        call puts
        call mm_nl
.n:
        pop ax
        push es
        mov es, ax
        cmp byte [es:0], 'M'
        jne .end
        add ax, [es:3]
        pop es
        inc ax
        jmp .l
.end:
        pop es
        cmp byte [mm_found], 0
        jne .tot
        mov si, msg_mmnf1
        call puts
        mov si, mm_name
        mov cx, 8
.nf:
        lodsb
        cmp al, ' '
        je .nfe
        call putc
        loop .nf
.nfe:
        mov si, msg_mmnf2
        call puts
        jmp mm_nl
.tot:
        call mm_nl
        mov si, msg_mmtot
        call puts
        mov eax, edx
        mov cl, 12
        call mm_bytes
        jmp mm_nl

; ---- MEM /D ----
mem_debug:
        call mm_nl
        mov si, msg_mmd1
        call puts
        call mm_nl
        call mm_nl
        mov si, msg_mmd2
        call puts
        call mm_nl
        mov si, msg_mmd3
        call puts
        call mm_nl
        ; zonas fijas antes del primer MCB
        mov si, msg_mmivt
        call puts
        call mm_nl
        mov si, msg_mmbda
        call puts
        call mm_nl
        call mm_first
        push ax
        mov si, s_mmind
        call puts
        mov ax, 0x0050
        call mm_hex4
        mov si, s_mmind
        call puts
        pop ax
        push ax
        movzx eax, ax
        sub eax, 0x50
        shl eax, 4
        mov cl, 12
        call mm_bytes
        mov si, msg_mmkern
        call puts
        call mm_nl
        pop ax
.l:
        push ax
        call mm_class
        pop ax
        push ax
        mov si, s_mmind
        call puts
        call mm_hex4
        mov si, s_mmind
        call puts
        mov eax, ecx
        mov cl, 12
        call mm_bytes
        mov al, ' '
        call putc
        call putc
        call mm_name8
        call mm_typename
        call puts
        call mm_nl
        pop ax
        push es
        mov es, ax
        cmp byte [es:0], 'M'
        jne .devs
        add ax, [es:3]
        pop es
        inc ax
        jmp .l
.devs:
        pop es
        ; controladores de dispositivo (cadena desde NUL, lista de listas+22h)
        call mm_nl
        mov si, msg_mmdev
        call puts
        call mm_nl
        mov ah, 0x52
        int 0x21
        add bx, 0x22
.d:
        mov si, s_mmind
        call puts
        mov ax, es
        call mm_hex4
        mov al, ':'
        call putc
        mov ax, bx
        call mm_hex4
        mov al, ' '
        call putc
        call putc
        test word [es:bx+4], 0x8000
        jz .blk
        push bx
        mov cx, 8
.dn:
        mov al, [es:bx+10]
        call putc
        inc bx
        loop .dn
        pop bx
        mov si, msg_mmchr
        call puts
        jmp .dnx
.blk:
        movzx eax, byte [es:bx+10]
        mov cl, 0
        call putnum_w
        mov si, msg_mmblk
        call puts
.dnx:
        call mm_nl
        cmp word [es:bx], 0xFFFF
        je .dend
        les bx, [es:bx]
        jmp .d
.dend:
        push ds
        pop es
        ret

do_date:
        cmp byte [si], 0
        je .show
        ; FECHA dd-mm-aaaa (también con «/» o «.»; año de dos cifras: 80-99
        ; = 19xx, 00-79 = 20xx)
        call parse_u16
        jc .bad
        mov [dt_tmp], al
        call .sep
        jc .bad
        call parse_u16
        jc .bad
        mov [dt_tmp+1], al
        call .sep
        jc .bad
        call parse_u16
        jc .bad
        cmp ax, 100
        jae .y4
        add ax, 1900
        cmp ax, 1980
        jae .y4
        add ax, 100
.y4:
        mov cx, ax
        mov dl, [dt_tmp]
        mov dh, [dt_tmp+1]
        mov ah, 0x2B
        int 0x21
        test al, al
        jz .r
.bad:
        mov si, msg_baddate
        jmp puts
.sep:
        mov al, [si]
        cmp al, '-'
        je .sy
        cmp al, '/'
        je .sy
        cmp al, '.'
        je .sy
        stc
        ret
.sy:
        inc si
        clc
.r:
        ret
.show:
        mov ah, 0x2A
        int 0x21
        push cx
        push dx
        mov si, msg_date
        call puts
        movzx bx, al
        shl bx, 2
        lea si, [days+bx]
        call puts
        mov al, ' '
        call putc
        pop dx
        push dx
        movzx ax, dl
        call put2
        mov al, '-'
        call putc
        pop dx
        movzx ax, dh
        call put2
        mov al, '-'
        call putc
        pop cx
        call put_year
        jmp crlf

; CX = año -> cuatro cifras sin separador
put_year:
        mov ax, cx
        xor dx, dx
        mov cx, 100
        div cx
        call put2
        mov ax, dx
        jmp put2

do_time:
        cmp byte [si], 0
        je .show
        ; HORA hh:mm[:ss] (también con «.»)
        mov word [dt_tmp], 0
        call parse_u16
        jc .bad
        cmp ax, 23
        ja .bad
        mov [dt_tmp+1], al
        call .sep
        jc .bad
        call parse_u16
        jc .bad
        cmp ax, 59
        ja .bad
        mov [dt_tmp], al
        xor dx, dx
        call .sep
        jc .set
        call parse_u16
        jc .bad
        cmp ax, 59
        ja .bad
        mov dh, al
        xor dl, dl
.set:
        mov cx, [dt_tmp]
        mov ah, 0x2D
        int 0x21
        test al, al
        jz .r
.bad:
        mov si, msg_badtime
        jmp puts
.sep:
        mov al, [si]
        cmp al, ':'
        je .sy
        cmp al, '.'
        je .sy
        stc
        ret
.sy:
        inc si
        clc
.r:
        ret
.show:
        mov ah, 0x2C
        int 0x21
        push dx
        push cx
        mov si, msg_time
        call puts
        pop cx
        push cx
        movzx eax, ch
        mov cl, 2
        call putnum_w
        mov al, ':'
        call putc
        pop cx
        movzx ax, cl
        call put2
        mov al, ':'
        call putc
        pop dx
        movzx ax, dh
        call put2
        jmp crlf

; AYUDA            lista de órdenes
; AYUDA orden      lo mismo que «orden /?» (internas y programas)
do_help:
        cmp byte [si], 0
        je .list
        mov di, help_line
        mov cx, 100
.c:
        lodsb
        cmp al, ' '
        jbe .e
        cmp al, '/'
        je .e
        stosb
        loop .c
.e:
        mov dword [di], ' /?' ; (con el cero final)
        mov si, help_line
        jmp execute_line
.list:
        mov si, msg_help
        jmp puts

; «orden /?» de una orden interna (BX = su rutina)
int_help:
        mov si, help_tab
.l:
        lodsw
        test ax, ax
        jz .none
        cmp ax, bx
        lodsw
        jne .l
        mov si, ax
        jmp puts
.none:
        mov si, h_none
        jmp puts

; -----------------------------------------------------------------------------
;  Programas externos (.COM / .EXE) con búsqueda en PATH
; -----------------------------------------------------------------------------
run_external:
        mov byte [al_tried], 0
.again:
        ; ¿lleva extensión o ruta?
        mov byte [hasext], 0
        mov byte [haspath], 0
        mov si, cmdword
.sc:
        lodsb
        test al, al
        jz .scd
        cmp al, '.'
        jne .s1
        mov byte [hasext], 1
.s1:
        cmp al, '\'
        je .hp
        cmp al, ':'
        jne .sc
.hp:
        mov byte [haspath], 1
        mov byte [hasext], 0
        jmp .sc
.scd:
        ; la extensión solo cuenta en el último componente
        mov si, cmdword
        mov di, si
.lc:
        lodsb
        test al, al
        jz .lcd
        cmp al, '\'
        je .lcs
        cmp al, ':'
        jne .lc
.lcs:
        mov di, si
        jmp .lc
.lcd:
        mov byte [hasext], 0
.le:
        mov al, [di]
        test al, al
        jz .lee
        inc di
        cmp al, '.'
        jne .le
        mov byte [hasext], 1
.lee:
        ; con extensión, solo se ejecutan .COM, .EXE y .BAT (como MS-DOS):
        ; «CONFIG.SYS» o «KERNEL.SYS» no son programas
        cmp byte [hasext], 0
        je .extok
        cmp byte [di+3], 0
        jne .extmal
        mov ax, [di]
        mov dl, [di+2]
        cmp ax, 'CO'
        jne .x1
        cmp dl, 'M'
        je .extok
.x1:
        cmp ax, 'EX'
        jne .x2
        cmp dl, 'E'
        je .extok
.x2:
        cmp ax, 'BA'
        jne .extmal
        cmp dl, 'T'
        je .extok
.extmal:
        jmp .badmsg
.extok:
        ; cola de órdenes: argumentos tal cual (con el espacio inicial)
        mov si, [argp]
        mov di, tail+1
        xor cx, cx
.t:
        lodsb
        test al, al
        jz .td
        cmp cx, 126
        jae .t
        stosb
        inc cx
        jmp .t
.td:
        mov [tail], cl
        mov byte [di], 13
        ; FCB 1 y 2
        mov si, [argp]
        mov di, fcb1
        mov ax, 0x2901
        int 0x21
        mov di, fcb2
        mov ax, 0x2901
        int 0x21
        ; 1) directorio actual / ruta dada
        mov word [pathptr], 0
        mov byte [prefix], 0
        call try_dir
        jnc .done
        cmp byte [haspath], 0
        jne .bad
        ; 2) cada entrada de PATH
        call find_path_var
        jc .bad
        mov [pathptr], si
.next:
        mov si, [pathptr]
        mov es, [0x2C]
        cmp byte [es:si], 0
        je .badr
        mov di, prefix
.pc:
        mov al, [es:si]
        test al, al
        jz .pce
        inc si
        cmp al, ';'
        je .pce2
        cmp di, prefix + 77
        jae .pc
        mov [di], al
        inc di
        jmp .pc
.pce:
.pce2:
        mov [pathptr], si
        push ds
        pop es
        cmp di, prefix
        je .next
        cmp byte [di-1], '\'
        je .pz
        mov byte [di], '\'
        inc di
.pz:
        mov byte [di], 0
        call try_dir
        jnc .done
        jmp .next
.badr:
        push ds
        pop es
.bad:
        ; ¿es un alias? (EDIT=EDITAR en ALIAS.INI): un solo intento
        cmp byte [al_tried], 0
        jne .badmsg
        mov byte [al_tried], 1
        cmp byte [haspath], 0
        jne .badmsg
        cmp byte [hasext], 0
        jne .badmsg
        call alias_lookup
        jc .badmsg
        jmp .again
.badmsg:
        mov si, msg_badcmd
        call puts
.done:
        ret

; ---- alias: buscar cmdword en <unidad de arranque>:\HISPADOS\ALIAS.INI ----
; Líneas «NOMBRE=PROGRAMA»; «;» empieza un comentario. Si está, cmdword pasa
; a ser el programa (en mayúsculas). CF si no hay alias.
ALIAS_MAX equ 2048
alias_lookup:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [s_alias], dl
        mov dx, s_alias
        mov ax, 0x3D00
        int 0x21
        jc .no
        mov bx, ax
        mov dx, alias_buf
        mov cx, ALIAS_MAX - 1
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        mov ah, 0x3E
        int 0x21
        pop ax
        popf
        jc .no
        mov bx, ax
        mov byte [alias_buf+bx], 0
        mov si, alias_buf
.line:
        call al_skip
        mov al, [si]
        test al, al
        jz .no
        cmp al, ';'
        je .eol
        cmp al, 13
        je .eol
        cmp al, 10
        je .eol
        ; comparar el nombre con cmdword
        mov di, cmdword
.cmp:
        mov al, [si]
        call upcase
        cmp al, '='
        je .nend
        cmp al, ' '
        je .nend
        cmp al, 9
        je .nend
        cmp al, [di]
        jne .eol
        inc si
        inc di
        jmp .cmp
.nend:
        cmp byte [di], 0            ; el nombre entero
        jne .eol
        call al_skip
        cmp byte [si], '='
        jne .eol
        inc si
        call al_skip
        ; copiar el programa en cmdword
        mov di, cmdword
        mov cx, 63
.cp:
        mov al, [si]
        cmp al, ' '
        jbe .cpe
        cmp al, ';'
        je .cpe
        call upcase
        mov [di], al
        inc di
        inc si
        loop .cp
.cpe:
        cmp di, cmdword
        je .no
        mov byte [di], 0
        clc
        ret
.eol:
        mov al, [si]
        test al, al
        jz .no
        inc si
        cmp al, 10
        jne .eol
        jmp .line
.no:
        stc
        ret
al_skip:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp al_skip
.r:
        ret

; prefix + cmdword (+ .COM/.EXE) -> ejecutar. CF=1 si no existe
try_dir:
        cmp byte [hasext], 0
        je .try2
        xor bx, bx
        jmp try_one
.try2:
        mov bx, s_com
        call try_one
        jnc .r
        mov bx, s_exe
        call try_one
        jnc .r
        mov bx, s_bat
        call try_one
.r:
        ret

; BX = extensión (0 = ninguna)
try_one:
        mov di, progpath
        mov si, prefix
.a:
        lodsb
        test al, al
        jz .b
        stosb
        jmp .a
.b:
        mov si, cmdword
.c:
        lodsb
        test al, al
        jz .d
        stosb
        jmp .c
.d:
        test bx, bx
        jz .e
        mov si, bx
.f:
        lodsb
        stosb
        test al, al
        jnz .f
        jmp .g
.e:
        stosb
.g:
        ; ¿existe?
        call set_dta
        mov dx, progpath
        mov cx, 0x06
        mov ah, 0x4E
        int 0x21
        jc .no
        ; ¿es un .BAT?
        mov si, progpath
        call strlen
        add si, cx
        cmp cx, 4
        jb .notbat
        mov eax, [si-4]
        and eax, 0xDFDFDFFF
        cmp eax, '.BAT'
        jne .notbat
        call start_batch
        clc
        ret
.notbat:
        ; (el bloque de parámetros lo monta la parte residente: run_prog)
        cmp byte [lh_flag], 0
        je .low
        ; LH: enlazar la memoria superior y reservar solo en ella; si no hay
        ; o el programa no cabe (error 8), se carga abajo
        mov byte [lh_flag], 0
        mov ax, 0x5802
        int 0x21
        mov [lh_link], al
        mov ax, 0x5803
        mov bx, 1
        int 0x21
        jc .low
        mov ax, 0x5801
        mov bx, 0x40
        int 0x21
        call run_prog
        pushf
        push ax
        mov ax, 0x5801
        xor bx, bx
        int 0x21
        mov ax, 0x5803
        movzx bx, byte [lh_link]
        int 0x21
        pop ax
        popf
        jnc .ran
        cmp ax, 8
        jne .xerr
.low:
        call run_prog
        jnc .ran
.xerr:
        mov si, msg_execerr
        call puts
        jmp .ok
.ran:
        mov ah, 0x4D
        int 0x21
        mov [errlvl], al
.ok:
        clc
        ret
.no:
        stc
        ret

; -> SI = valor de PATH en el entorno (offset), CF si no hay
find_path_var:
        mov es, [0x2C]
        xor si, si
.l:
        cmp byte [es:si], 0
        je .no
        cmp dword [es:si], 'PATH'
        jne .s
        cmp byte [es:si+4], '='
        jne .s
        add si, 5
        push ds
        pop es
        clc
        ret
.s:
        inc si
        cmp byte [es:si-1], 0
        jne .s
        jmp .l
.no:
        push ds
        pop es
        stc
        ret

; -----------------------------------------------------------------------------
;  Utilidades
; -----------------------------------------------------------------------------
skip_sp:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        jne .r
.s:
        inc si
        jmp skip_sp
.r:
        ret

; termina el argumento en el primer espacio
cut_arg:
        push si
.l:
        mov al, [si]
        test al, al
        jz .d
        cmp al, ' '
        je .z
        inc si
        jmp .l
.z:
        mov byte [si], 0
.d:
        pop si
        ret

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret

; SI = mensaje; AX = código de error de DOS: 21 -> «no está preparada»
err_puts:
        cmp ax, 21
        jne puts
        mov si, msg_nrdy
puts:
        push ax
        push dx
.l:
        lodsb
        test al, al
        jz .d
        mov dl, al
        mov ah, 2
        int 0x21
        jmp .l
.d:
        pop dx
        pop ax
        ret

crlf:
        push si
        mov si, s_crlf
        call puts
        pop si
        ret

; AX (0..99) con dos cifras
put2:
        push ax
        push dx
        push cx
        xor dx, dx
        mov cx, 10
        div cx
        add al, '0'
        call putc
        mov al, dl
        add al, '0'
        call putc
        pop cx
        pop dx
        pop ax
        ret

; EAX sin relleno
putnum:
        mov cl, 0
; EAX con separador de miles, alineado a la derecha en CL columnas
putnum_w:
        push eax
        push ebx
        push ecx
        push edx
        push si
        push di
        mov di, numbuf+15
        mov byte [di], 0
        mov ebx, 10
        xor ch, ch
.l:
        xor edx, edx
        div ebx
        add dl, '0'
        cmp ch, 3
        jne .nc
        dec di
        mov byte [di], '.'           ; separador de miles español
        mov ch, 0
.nc:
        dec di
        mov [di], dl
        inc ch
        test eax, eax
        jnz .l
        ; relleno
        mov ax, numbuf+15
        sub ax, di
        movzx cx, cl
        sub cx, ax
        jle .p
.pad:
        mov al, ' '
        call putc
        loop .pad
.p:
        mov si, di
        call puts
        pop di
        pop si
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret


; -----------------------------------------------------------------------------
;  Tuberías ("|") y redirecciones ("<", ">", ">>")
; -----------------------------------------------------------------------------
; SI = línea; [pl_in] = fichero de entrada heredado de la tubería (0 = ninguno)
run_line:
        mov di, si
.f:
        mov al, [di]
        test al, al
        jz .simple
        cmp al, '|'
        je .pipe
        inc di
        jmp .f
.simple:
        mov word [pl_out], 0
        jmp run_simple
.pipe:
        cmp byte [pl_depth], PIPE_MAX
        jb .pok
        mov si, msg_pipes
        jmp puts
.pok:
        mov byte [di], 0
        inc di
        push di
        ; nombre del fichero temporal según la profundidad
        movzx bx, byte [pl_depth]
        inc byte [pl_depth]
        mov al, bl
        imul bx, bx, PIPE_LEN
        add bx, pipe_tab
        push bx
        call pipe_name
        mov [pl_out], bx
        call run_simple
        pop bx
        pop si
        push word [pl_in]
        mov [pl_in], bx
        push bx
        call run_line
        pop dx
        mov ah, 0x41
        int 0x21
        pop word [pl_in]
        dec byte [pl_depth]
        ret

; BX = búfer, AL = número -> «%TEMP%\~PIPEn.TMP» (sin TEMP, en la raíz de la
; unidad actual). Conserva SI y BX.
pipe_name:
        push si
        push di
        push bx
        push ax
        mov si, s_temp
        mov di, vval
        call env_get
        pop ax
        pop bx
        push bx
        push ax
        mov di, bx
        jc .cur
        mov si, vval
        cmp byte [si], 0
        je .cur
        mov cx, 64
.c:
        lodsb
        test al, al
        jz .ce
        stosb
        loop .c
.ce:
        cmp byte [di-1], '\'
        je .nm
        cmp byte [di-1], ':'
        je .nm
        mov al, '\'
        stosb
        jmp .nm
.cur:
        mov ah, 0x19
        int 0x21
        add al, 'A'
        stosb
        mov ax, ':\'
        stosw
.nm:
        mov si, s_pipe
        mov cx, 5
        rep movsb
        pop ax
        add al, '0'
        stosb
        mov si, s_tmp
        mov cx, 5
        rep movsb
        pop bx
        pop di
        pop si
        ret

; SI = orden sin tuberías
run_simple:
        mov word [r_in], 0
        mov word [r_out], 0
        mov byte [r_app], 0
        mov di, si
.scan:
        mov al, [di]
        test al, al
        jz .done
        cmp al, '<'
        je .in
        cmp al, '>'
        je .out
        inc di
        jmp .scan
.in:
        mov byte [di], ' '
        inc di
        mov bx, rin_name
        mov [r_in], bx
        mov dx, rin_name + 79
        call .name
        jmp .scan
.out:
        mov byte [di], ' '
        inc di
        cmp byte [di], '>'
        jne .o1
        mov byte [di], ' '
        inc di
        mov byte [r_app], 1
.o1:
        mov bx, rout_name
        mov [r_out], bx
        mov dx, rout_name + 79
        call .name
        jmp .scan
; copiar el nombre que sigue a DI en BX y borrarlo de la línea
.name:
        cmp byte [di], ' '
        jne .n1
        inc di
        jmp .name
.n1:
        mov al, [di]
        test al, al
        jz .ne
        cmp al, ' '
        je .ne
        cmp al, '<'
        je .ne
        cmp al, '>'
        je .ne
        mov byte [di], ' '
        inc di
        cmp bx, dx
        jae .n1
        mov [bx], al
        inc bx
        jmp .n1
.ne:
        mov byte [bx], 0
        ret
.done:
        ; quitar los espacios que han dejado las redirecciones al final
.trim:
        cmp di, si
        jbe .trimd
        cmp byte [di-1], ' '
        jne .trimd
        dec di
        mov byte [di], 0
        jmp .trim
.trimd:
        ; la tubería manda si no hay redirección explícita
        cmp word [r_in], 0
        jne .ri
        mov ax, [pl_in]
        mov [r_in], ax
.ri:
        cmp word [r_out], 0
        jne .ro
        mov ax, [pl_out]
        mov [r_out], ax
.ro:
        push si
        call redir_setup
        pop si
        jc .bad
        call execute_line
        call redir_restore
        ret
.bad:
        call redir_restore
        mov si, msg_redir
        jmp puts

redir_setup:
        mov word [sv_in], 0xFFFF
        mov word [sv_out], 0xFFFF
        mov dx, [r_in]
        test dx, dx
        jz .noin
        mov ax, 0x3D00
        int 0x21
        jc .err
        mov bx, 0
        call swap_handle
        mov [sv_in], ax
.noin:
        mov dx, [r_out]
        test dx, dx
        jz .ok
        cmp byte [r_app], 0
        je .create
        mov ax, 0x3D01
        int 0x21
        jc .create
        push ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        pop ax
        jmp .gotout
.create:
        mov dx, [r_out]
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .err
.gotout:
        mov bx, 1
        call swap_handle
        mov [sv_out], ax
.ok:
        clc
        ret
.err:
        stc
        ret

; AX = handle nuevo, BX = handle estándar -> AX = copia del antiguo
swap_handle:
        push cx
        push dx
        mov dx, ax
        push bx
        mov ah, 0x45
        int 0x21
        pop bx
        push ax
        mov cx, bx
        mov bx, dx
        mov ah, 0x46
        int 0x21
        mov bx, dx
        mov ah, 0x3E
        int 0x21
        pop ax
        pop dx
        pop cx
        ret

redir_restore:
        mov bx, [sv_out]
        cmp bx, 0xFFFF
        je .i
        mov cx, 1
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov word [sv_out], 0xFFFF
.i:
        mov bx, [sv_in]
        cmp bx, 0xFFFF
        je .r
        xor cx, cx
        mov ah, 0x46
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov word [sv_in], 0xFFFF
.r:
        ret

; -----------------------------------------------------------------------------
;  Utilidades de rutas
; -----------------------------------------------------------------------------
; SI -> palabra en DI (ASCIIZ), SI avanza; CF si no hay
get_tok:
        call skip_sp
        cmp byte [si], 0
        je .none
        push cx
        mov cx, 127                 ; los búferes de destino son de 132
.l:
        lodsb
        test al, al
        jz .e0
        cmp al, ' '
        je .e
        cmp al, 9
        je .e
        jcxz .l
        stosb
        dec cx
        jmp .l
.e0:
        dec si
.e:
        mov byte [di], 0
        pop cx
        clc
        ret
.none:
        mov byte [di], 0
        stc
        ret

; SI = ruta -> CF=1 si tiene comodines
has_wild:
        push si
.l:
        lodsb
        test al, al
        jz .no
        cmp al, '*'
        je .yes
        cmp al, '?'
        je .yes
        jmp .l
.yes:
        pop si
        stc
        ret
.no:
        pop si
        clc
        ret

; SI = ruta -> DI = su parte de carpeta ("A:\DOCS\", "A:", "")
path_prefix:
        push si
        push di
        mov bx, di
.c:
        lodsb
        stosb
        test al, al
        jz .e
        cmp al, '\'
        je .m
        cmp al, ':'
        jne .c
.m:
        mov bx, di
        jmp .c
.e:
        mov byte [bx], 0
        pop di
        pop si
        ret

; DI = destino <- SI (prefijo) + BX (nombre)
join_path:
        push si
.a:
        lodsb
        test al, al
        jz .b
        stosb
        jmp .a
.b:
        mov si, bx
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop si
        ret

; SI = ruta: añadir "\*.*" (o "*.*" tras "\" o ":")
append_all:
        push si
.e:
        lodsb
        test al, al
        jnz .e
        dec si
        mov di, si
        mov al, [di-1]
        cmp al, '\'
        je .a
        cmp al, ':'
        je .a
        mov byte [di], '\'
        inc di
.a:
        mov si, s_all
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop si
        ret

; SI = ruta -> CF=0 si es un directorio existente
is_dir:
        push si
        push di
        mov di, t_buf
        mov ah, 0x60
        int 0x21
        jc .no
        mov si, t_buf
        call has_wild
        jc .no
.e:
        lodsb
        test al, al
        jnz .e
        cmp byte [si-2], '\'
        je .yes
        mov dx, dta2
        mov ah, 0x1A
        int 0x21
        mov dx, t_buf
        mov cx, 0x16
        mov ah, 0x4E
        int 0x21
        jc .no
        test byte [dta2+0x15], 0x10
        jz .no
.yes:
        pop di
        pop si
        clc
        ret
.no:
        pop di
        pop si
        stc
        ret

; SI, DI = rutas -> CF=1 (y aviso) si son el mismo fichero
same_file:
        push si
        push di
        push di
        mov di, t_buf
        mov ah, 0x60
        int 0x21
        pop si
        mov di, t_buf2
        mov ah, 0x60
        int 0x21
        mov si, t_buf
        mov di, t_buf2
.c:
        mov al, [si]
        cmp al, [di]
        jne .diff
        inc si
        inc di
        test al, al
        jnz .c
        mov si, msg_self
        call puts
        pop di
        pop si
        stc
        ret
.diff:
        pop di
        pop si
        clc
        ret

msg_required:
        mov si, msg_reqparam
        jmp puts

; -----------------------------------------------------------------------------
;  COPY [/Y | /-Y] [/V] [/A | /B] origen [/A | /B] [+ origen ...]
;       [destino [/A | /B]]
;   /Y   sobrescribir sin preguntar      /-Y  preguntar antes
;   Por omisión se pregunta en la línea de órdenes y no en los archivos por
;   lotes; la variable COPYCMD puede llevar /Y o /-Y (la línea manda).
;   /V   se acepta (la escritura ya se comprueba)
;   /A   texto: el origen se lee hasta Ctrl+Z; al destino se le añade
;   /B   binario (lo normal, también al unir con «+»; en MS-DOS al unir lo
;        normal es /A: aquí no, para no cortar archivos binarios sin avisar)
; -----------------------------------------------------------------------------
do_copy:
        xor ax, ax
        mov [c_src], al
        mov [c_dst], al
        mov [c_cur], al
        mov [c_smode], al
        mov [c_dmode], al
        mov [c_plus], al
        mov [c_nodst], al
        mov [c_dwild], al
        mov [c_asc], al
        cmp byte [bdepth], 0
        sete [c_ask]                ; fuera de los lotes se pregunta
        ; COPYCMD: /Y o /-Y
        push si
        mov si, s_copycmd
        mov di, vval
        call env_get
        jc .noenv
        mov si, vval
.ev:
        lodsb
        test al, al
        jz .noenv
        cmp al, '/'
        jne .ev
        mov ah, 0
        cmp byte [si], '-'
        jne .ev1
        inc si
        mov ah, 1
.ev1:
        mov al, [si]
        call upcase
        cmp al, 'Y'
        jne .ev
        mov [c_ask], ah
        jmp .ev
.noenv:
        pop si
        ; la línea: opciones, orígenes (con «+») y destino
.arg:
        mov al, [si]
        cmp al, ' '
        je .asp
        cmp al, 9
        je .asp
        cmp al, ','
        jne .a1
.asp:
        inc si
        jmp .arg
.a1:
        test al, al
        jz .parsed
        cmp al, '/'
        je .sw
        cmp al, '+'
        je .pl
        ; un nombre: origen (el primero o tras «+») o destino
        cmp byte [c_src], 0
        je .tosrc
        cmp byte [c_plus], 0
        jne .tosrc
        cmp byte [c_dst], 0
        jne .many
        mov di, c_dst
        mov dx, c_dst + 130
        mov al, [c_cur]
        mov [c_dmode], al
        jmp .cpname
.tosrc:
        mov di, c_src
        call .eos
        mov dx, c_src + 130
        mov byte [c_plus], 0
.cpname:
        mov al, [si]
        test al, al
        jz .cpe
        cmp al, ' '
        je .cpe
        cmp al, 9
        je .cpe
        cmp al, ','
        je .cpe
        cmp al, '/'
        je .cpe
        cmp al, '+'
        je .cpe
        inc si
        cmp di, dx
        jae .cpname
        mov [di], al
        inc di
        jmp .cpname
.cpe:
        mov byte [di], 0
        jmp .arg
.pl:
        inc si
        mov di, c_src
        call .eos
        cmp di, c_src + 130
        jae .arg
        mov word [di], '+'
        mov byte [c_plus], 1
        jmp .arg
.sw:
        inc si
        mov al, [si]
        cmp al, '-'
        jne .s1
        inc si
        mov al, [si]
        call upcase
        cmp al, 'Y'
        jne .badsw
        inc si
        mov byte [c_ask], 1
        jmp .arg
.s1:
        call upcase
        test al, al
        jz .badsw
        inc si
        cmp al, 'V'
        je .arg
        cmp al, 'Y'
        jne .s2
        mov byte [c_ask], 0
        jmp .arg
.s2:
        mov ah, 1
        cmp al, 'A'
        je .mode
        mov ah, 2
        cmp al, 'B'
        je .mode
.badsw:
        mov si, msg_badparam
        jmp puts
.mode:
        mov [c_cur], ah
        cmp byte [c_dst], 0
        je .ms
        mov [c_dmode], ah
        jmp .arg
.ms:
        mov [c_smode], ah
        jmp .arg
.many:
        mov si, msg_cmany
        jmp puts
; DI -> el cero final de la cadena
.eos:
        cmp byte [di], 0
        je .eosr
        inc di
        jmp .eos
.eosr:
        ret
.parsed:
        cmp byte [c_src], 0
        je msg_required
        cmp byte [c_dst], 0
        jne .hd
        mov word [c_dst], '.'
        mov byte [c_nodst], 1
.hd:
        mov dword [c_count], 0
        mov byte [c_open], 0
        cmp byte [c_smode], 1
        sete [c_asc]
        ; ¿concatenación A+B[+C...]?
        mov si, c_src
.plus:
        lodsb
        test al, al
        jz .noplus
        cmp al, '+'
        jne .plus
        jmp do_concat
.noplus:
        ; ¿origen dispositivo CON?
        mov si, c_src
        mov ax, 0x3D00
        mov dx, si
        int 0x21
        jc .notdev
        mov bx, ax
        mov ax, 0x4400
        int 0x21
        push dx
        mov ah, 0x3E
        int 0x21
        pop dx
        test dl, 0x80
        jz .notdev
        jmp .device
.notdev:
        mov si, c_src
        call is_dir
        jc .nd
        mov si, c_src
        call append_all
.nd:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        mov byte [c_dstdir], 0
        mov si, c_dst
        call is_dir
        jc .nd2
        mov byte [c_dstdir], 1
.nd2:
        mov si, c_src
        call has_wild
        setc [c_wild]
        ; destino con comodines (COPY *.TXT *.BAK): máscara como en REN
        mov si, c_dst
        call has_wild
        jnc .nd3
        mov byte [c_dwild], 1
        mov si, c_dst
        mov di, c_dpre
        call path_prefix
        mov si, c_dpre
        call strlen
        mov si, c_dst
        add si, cx
        mov di, m_mask
        call to11
        jmp .nd4
.nd3:
.nd4:
        call set_dta
        mov dx, c_src
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jnc .loop
        mov si, msg_nofile
        call err_puts
        jmp .summary
.loop:
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        cmp byte [c_wild], 0
        je .nw
        mov si, c_spath
        call puts
        call crlf
.nw:
        cmp byte [c_dwild], 0
        je .nomask
        ; destino = carpeta del destino + nombre con la máscara
        mov si, dta+0x1E
        call mask_name
        mov si, c_dpre
        mov bx, m_new
        mov di, c_dpath
        call join_path
        jmp .todir
.nomask:
        cmp byte [c_dstdir], 0
        je .tofile
        ; destino carpeta: destino\nombre
        mov si, c_dst
        mov di, c_dpre
.dp:
        lodsb
        stosb
        test al, al
        jnz .dp
        dec di
        mov al, [di-1]
        cmp al, '\'
        je .dp2
        cmp al, ':'
        je .dp2
        mov byte [di], '\'
        inc di
        mov byte [di], 0
.dp2:
        mov si, c_dpre
        mov bx, dta+0x1E
        mov di, c_dpath
        call join_path
.todir:
        mov si, c_spath
        mov di, c_dpath
        call same_file
        jc .next
        mov dx, c_dpath
        call copy_ask
        jc .next
        mov dx, c_dpath
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        call copy_one
        call copy_close
        jmp .next
.tofile:
        cmp byte [c_open], 0
        jne .append
        mov si, c_spath
        mov di, c_dst
        call same_file
        jc .next
        mov dx, c_dst
        call copy_ask
        jc .summary
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        mov byte [c_open], 1
.append:
        call copy_one
.next:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .loop
        jmp .close
.cerr:
        mov si, msg_cantcreate
        call puts
.close:
        cmp byte [c_open], 0
        je .summary
        call copy_close
.summary:
        mov eax, [c_count]
        mov cl, 9
        call putnum_w
        mov si, msg_copied
        jmp puts
.device:
        mov si, c_dst
        call is_dir
        jnc .devbad
        mov dx, c_dst
        call copy_ask
        jc .summary
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cerr
        mov [c_dh], ax
        mov byte [c_open], 1
        mov dx, c_src
        mov ax, 0x3D00
        int 0x21
        jc .close
        mov bx, ax
        mov byte [c_isdev], 1
        call copy_data
        mov byte [c_isdev], 0
        mov ah, 0x3E
        int 0x21
        inc dword [c_count]
        jmp .close
.devbad:
        mov si, msg_cantcreate
        call puts
        jmp .summary

; cerrar el destino (con /A en el destino, añade Ctrl+Z)
copy_close:
        mov bx, [c_dh]
        cmp byte [c_dmode], 1
        jne .c
        mov byte [cbuf], 0x1A
        mov dx, cbuf
        mov cx, 1
        mov ah, 0x40
        int 0x21
.c:
        mov ah, 0x3E
        int 0x21
        mov byte [c_open], 0
        ret

; DX = destino: si ya existe (y no es un dispositivo) y hay que preguntar,
; «¿Sobrescribir X (Sí/No/Todos)?». CF=1 si no se debe escribir.
copy_ask:
        cmp byte [c_ask], 0
        je .ok
        push dx
        mov ax, 0x3D00
        int 0x21
        pop dx
        jc .ok
        mov bx, ax
        push dx
        mov ax, 0x4400
        int 0x21
        mov cx, dx
        mov ah, 0x3E
        int 0x21
        pop dx
        test cl, 0x80
        jnz .ok
.q:
        mov si, msg_over1
        call puts
        mov si, dx
        call puts
        mov si, msg_over2
        call puts
        push dx
        mov dx, ynbuf
        mov byte [ynbuf], 4
        mov ah, 0x0A
        int 0x21
        call crlf
        pop dx
        cmp byte [ynbuf+1], 0
        je .no
        mov al, [ynbuf+2]
        call upcase
        cmp al, 'S'
        je .ok
        cmp al, 'Y'
        je .ok
        cmp al, 'N'
        je .no
        cmp al, 'T'
        je .all
        cmp al, 'A'
        jne .q
.all:
        mov byte [c_ask], 0
.ok:
        clc
        ret
.no:
        stc
        ret

; SI = nombre -> m_new = el nombre con la máscara m_mask aplicada
mask_name:
        mov di, m_name
        call to11
        mov si, m_mask
        mov di, m_name
        mov cx, 11
.m:
        mov al, [si]
        cmp al, '?'
        je .keep
        mov [di], al
.keep:
        inc si
        inc di
        loop .m
        mov si, m_name
        mov di, m_new
        jmp from11

; COPY A+B[+C] [destino]: unir ficheros (sin destino, se añade a A)
do_concat:
        mov si, c_src
        mov di, c_spath
        mov byte [c_first], 1
.part:
        ; copiar la parte siguiente en c_spath
        mov di, c_spath
.cp:
        lodsb
        cmp al, '+'
        je .end
        test al, al
        je .end
        stosb
        jmp .cp
.end:
        mov byte [di], 0
        mov [c_next], si
        mov [c_last], al
        cmp byte [c_first], 0
        je .add
        mov byte [c_first], 0
        cmp byte [c_nodst], 0
        je .newdst
        ; sin destino: abrir la primera parte y añadir al final
        mov dx, c_spath
        mov ax, 0x3D02
        int 0x21
        jc .err
        mov [c_dh], ax
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        mov byte [c_open], 1
        inc dword [c_count]
        jmp .more
.newdst:
        mov dx, c_dst
        call copy_ask
        jc .sum
        mov dx, c_dst
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .err
        mov [c_dh], ax
        mov byte [c_open], 1
        inc dword [c_count]
.add:
        cmp byte [c_spath], 0       ; «A+,,»: parte vacía
        je .more
        call copy_one
.more:
        mov si, [c_next]
        cmp byte [c_last], 0
        jne .part
        call copy_close
.sum:
        mov eax, [c_count]
        cmp eax, 1
        jbe .s1
        mov eax, 1
.s1:
        mov cl, 9
        call putnum_w
        mov si, msg_copied
        jmp puts
.err:
        mov si, msg_nofile
        jmp puts

; copiar c_spath en el handle c_dh (conserva fecha y hora si es copia simple)
copy_one:
        mov dx, c_spath
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        call copy_data
        jc .full
        mov ax, 0x5700
        int 0x21
        push cx
        push dx
        mov ah, 0x3E
        int 0x21
        pop dx
        pop cx
        mov bx, [c_dh]
        mov ax, 0x5701
        int 0x21
        inc dword [c_count]
.r:
        ret
.full:
        mov ah, 0x3E
        int 0x21
        mov si, msg_full
        jmp puts

; BX = origen -> c_dh; CF si el disco se llena
copy_data:
        mov byte [c_eof], 0
.l:
        mov dx, cbuf
        mov cx, CBUF_LEN
        mov ah, 0x3F
        int 0x21
        jc .ok
        test ax, ax
        jz .ok
        mov cx, ax
        mov al, [c_isdev]
        or al, [c_asc]
        jz .w
        ; de un dispositivo o como texto (/A): parar en Ctrl+Z
        push di
        mov di, cbuf
        push cx
        mov al, 0x1A
        repne scasb
        pop cx
        jne .noz
        mov cx, di
        sub cx, cbuf
        dec cx
        mov byte [c_eof], 1
.noz:
        pop di
        jcxz .ok                    ; nada antes del Ctrl+Z (CX=0 truncaría)
.w:
        push bx
        push cx
        mov bx, [c_dh]
        mov dx, cbuf
        mov ah, 0x40
        int 0x21
        pop cx
        pop bx
        jc .err
        cmp ax, cx
        jb .err
        cmp byte [c_eof], 0
        je .l
.ok:
        mov byte [c_eof], 0
        clc
        ret
.err:
        stc
        ret

; -----------------------------------------------------------------------------
;  DEL / ERASE patrón
; -----------------------------------------------------------------------------
do_del:
        mov byte [c_src], 0
        mov byte [dl_p], 0
.arg:
        call skip_sp
        mov al, [si]
        test al, al
        jz .parsed
        cmp al, '/'
        jne .name
        mov al, [si+1]
        call upcase
        cmp al, 'P'
        jne .bad
        add si, 2
        mov byte [dl_p], 1
        jmp .arg
.bad:
        mov si, msg_badparam
        jmp puts
.name:
        cmp byte [c_src], 0
        je .n1
        mov si, msg_cmany
        jmp puts
.n1:
        mov di, c_src
.cn:
        mov al, [si]
        test al, al
        jz .ce
        cmp al, ' '
        je .ce
        cmp al, 9
        je .ce
        cmp al, '/'
        je .ce
        inc si
        cmp di, c_src + 127
        jae .cn
        mov [di], al
        inc di
        jmp .cn
.ce:
        mov byte [di], 0
        jmp .arg
.parsed:
        cmp byte [c_src], 0
        je msg_required
        mov si, c_src
        call is_dir
        jc .nd
        mov si, c_src
        call append_all
.nd:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        ; ¿todo el directorio?
        mov si, c_src
.e:
        lodsb
        test al, al
        jnz .e
        cmp dword [si-4], '*.*'
        je .all
        cmp word [si-2], '*'
        jne .go
        cmp si, c_src+2
        je .all
        cmp byte [si-3], '\'
        je .all
        cmp byte [si-3], ':'
        je .all
        jmp .go
.all:
        cmp byte [dl_p], 0          ; con /P ya se pregunta por cada uno
        jne .go
        mov si, msg_delall
        call puts
        call ask_yn
        jc .r
.go:
        call set_dta
        mov dx, c_src
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jnc .l
        mov si, msg_nofile
        jmp err_puts
.l:
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        cmp byte [dl_p], 0
        je .del
        mov si, c_spath
        call puts
        mov si, msg_delp
        call puts
        call ask_yn
        jc .n
.del:
        mov dx, c_spath
        mov ah, 0x41
        int 0x21
        jnc .n
        mov si, msg_accden
        call puts
.n:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .l
.r:
        ret

; leer S/N -> CF=0 si sí
ask_yn:
        mov dx, ynbuf
        mov byte [ynbuf], 4
        mov ah, 0x0A
        int 0x21
        call crlf
        mov al, [ynbuf+2]
        call upcase
        cmp al, 'Y'
        je .y
        cmp al, 'S'
        je .y
        stc
        ret
.y:
        clc
        ret

; -----------------------------------------------------------------------------
;  REN / RENAME origen nombre (admite máscaras como *.BAK)
; -----------------------------------------------------------------------------
do_ren:
        mov di, c_src
        call get_tok
        jc msg_required
        mov di, c_dst
        call get_tok
        jc msg_required
        mov si, c_dst
.chk:
        lodsb
        test al, al
        jz .ok
        cmp al, '\'
        je .bad
        cmp al, ':'
        je .bad
        jmp .chk
.bad:
        mov si, msg_badparam
        jmp puts
.ok:
        mov si, c_src
        mov di, c_pre
        call path_prefix
        mov si, c_dst
        mov di, m_mask
        call to11
        call set_dta
        mov dx, c_src
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
        jnc .l
        mov si, msg_dupnf
        jmp puts
.l:
        cmp byte [dta+0x1E], '.'    ; «.» y «..» no se renombran (REN *.* ...)
        je .n
        mov si, dta+0x1E
        mov di, m_name
        call to11
        ; aplicar la máscara
        mov si, m_mask
        mov di, m_name
        mov cx, 11
.m:
        mov al, [si]
        cmp al, '?'
        je .keep
        mov [di], al
.keep:
        inc si
        inc di
        loop .m
        mov si, m_name
        mov di, m_new
        call from11
        mov si, c_pre
        mov bx, dta+0x1E
        mov di, c_spath
        call join_path
        mov si, c_pre
        mov bx, m_new
        mov di, c_dpath
        call join_path
        mov dx, c_spath
        mov di, c_dpath
        mov ah, 0x56
        int 0x21
        jnc .n
        mov si, msg_dupnf
        call puts
.n:
        call set_dta
        mov ah, 0x4F
        int 0x21
        jnc .l
        ret

; nombre "NOMBRE.EXT" o máscara en SI -> 11 caracteres en DI ('*' -> '?')
to11:
        push di
        push cx
        mov cx, 11
        mov al, ' '
        rep stosb
        pop cx
        pop di
        push di
        mov bx, di
        cmp byte [si], '.'
        je .dots
        mov cx, 8
.n:
        lodsb
        test al, al
        jz .d
        cmp al, '.'
        je .x
        call upcase
        cmp al, '*'
        jne .nc
.nf:
        jcxz .n
        mov byte [di], '?'
        inc di
        dec cx
        jmp .nf
.nc:
        jcxz .n
        stosb
        dec cx
        jmp .n
.x:
        lea di, [bx+8]
        mov cx, 3
.e:
        lodsb
        test al, al
        jz .d
        call upcase
        cmp al, '*'
        jne .ec
.ef:
        jcxz .e
        mov byte [di], '?'
        inc di
        dec cx
        jmp .ef
.ec:
        jcxz .e
        stosb
        dec cx
        jmp .e
.dots:
        lodsb
        test al, al
        jz .d
        stosb
        jmp .dots
.d:
        pop di
        ret

; 11 caracteres en SI -> "NOMBRE.EXT" en DI
from11:
        push si
        mov cx, 8
.n:
        lodsb
        cmp al, ' '
        je .s
        stosb
.s:
        loop .n
        cmp byte [si], ' '
        je .e
        mov al, '.'
        stosb
        mov cx, 3
.x:
        lodsb
        cmp al, ' '
        je .xs
        stosb
.xs:
        loop .x
.e:
        mov byte [di], 0
        pop si
        ret

; -----------------------------------------------------------------------------
do_md:
        mov di, c_src
        call get_tok
        jc msg_required
        mov dx, c_src
        mov ah, 0x39
        int 0x21
        jnc .r
        mov si, msg_mderr
        call puts
.r:
        ret

do_rd:
        mov di, c_src
        call get_tok
        jc msg_required
        mov dx, c_src
        mov ah, 0x3A
        int 0x21
        jnc .r
        mov si, msg_rderr
        call puts
.r:
        ret

; -----------------------------------------------------------------------------
;  DELTREE / BORRARARBOL [/Y] nombre...  borra archivos y carpetas con todo
;  su contenido (también ocultos, de sistema y de solo lectura); admite
;  comodines. Sin /Y pregunta antes de cada uno.
; -----------------------------------------------------------------------------
DT_MAXDEPTH equ 32
do_deltree:
        mov byte [dt_yes], 0
        mov byte [dt_any], 0
        mov [dt_args], si
        ; primero las opciones
.opt:
        mov di, c_src
        call get_tok
        jc .optd
        cmp byte [c_src], '/'
        jne .opt
        mov al, [c_src+1]
        call upcase
        cmp al, 'Y'
        je .y
        cmp al, 'S'
        jne .opt
.y:
        mov byte [dt_yes], 1
        jmp .opt
.optd:
        mov si, [dt_args]
.arg:
        mov di, c_src
        call get_tok
        jc .end
        cmp byte [c_src], '/'
        je .arg
        mov byte [dt_any], 1
        push si
        call dt_pattern
        pop si
        jmp .arg
.end:
        cmp byte [dt_any], 0
        jne .r
        jmp msg_required
.r:
        ret

; un argumento (c_src, puede tener comodines)
dt_pattern:
        ; carpeta del patrón: hasta la última \ o :
        mov si, c_src
        mov di, dt_dir
        mov bx, di
.d:
        lodsb
        mov [di], al
        inc di
        test al, al
        jz .de
        cmp al, '\'
        je .ds
        cmp al, ':'
        jne .d
.ds:
        mov bx, di
        jmp .d
.de:
        mov byte [bx], 0
        mov dx, dt_dta0
        mov ah, 0x1A
        int 0x21
        mov dx, c_src
        mov cx, 0x37
        mov ah, 0x4E
        int 0x21
        jnc .f
        mov si, msg_dtnf
        jmp err_puts
.f:
        ; . y .. no
        cmp byte [dt_dta0+0x1E], '.'
        je .next
        ; ruta completa
        mov si, dt_dir
        mov di, dt_path
.c1:
        lodsb
        test al, al
        jz .c1e
        stosb
        jmp .c1
.c1e:
        mov si, dt_dta0+0x1E
.c2:
        lodsb
        stosb
        test al, al
        jnz .c2
        dec di
        mov [dt_end], di
        ; preguntar
        cmp byte [dt_yes], 0
        jne .go
        mov si, msg_dtask1
        call puts
        mov si, dt_path
        call puts
        mov si, msg_dtask2
        test byte [dt_dta0+0x15], 0x10
        jnz .q
        mov si, msg_dtask3
.q:
        call puts
        call ask_yn
        jc .next
.go:
        mov si, msg_dtdel
        call puts
        mov si, dt_path
        call puts
        mov si, msg_dtdots
        call puts
        mov byte [dt_depth], 0
        call dt_remove
        jnc .ok
        mov si, msg_dterr
        call puts
.ok:
        ; dt_remove cambia la DTA
        mov dx, dt_dta0
        mov ah, 0x1A
        int 0x21
.next:
        mov ah, 0x4F
        int 0x21
        jnc .f
        ret

; borrar dt_path (fichero o carpeta, con todo su contenido); dt_end = su fin
; -> CF si algo no se ha podido borrar
dt_remove:
        mov dx, dt_path
        mov ax, 0x4300
        int 0x21
        jc .r
        test cl, 0x10
        jnz .dir
        mov ax, 0x4301              ; quitar solo lectura, oculto, sistema
        xor cx, cx
        int 0x21
        mov ah, 0x41
        int 0x21
.r:
        ret
.dir:
        cmp byte [dt_depth], DT_MAXDEPTH
        jb .deep
        stc
        ret
.deep:
        inc byte [dt_depth]
        push bp
        sub sp, 44                  ; DTA de este nivel
        mov bp, sp
        mov byte [bp+43], 0         ; errores en este nivel
        mov dx, bp
        mov ah, 0x1A
        int 0x21
        mov di, [dt_end]
        push di
        cmp di, dt_path + 200
        jae .full
        mov dword [di], '\*.*'
        mov byte [di+4], 0
        mov dx, dt_path
        mov cx, 0x37
        mov ah, 0x4E
        int 0x21
        jc .empty
.l:
        cmp byte [bp+0x1E], '.'
        je .n
        ; ruta del elemento
        pop di
        push di
        mov byte [di], '\'
        inc di
        lea si, [bp+0x1E]
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        dec di
        mov [dt_end], di
        call dt_remove
        jnc .ok
        mov byte [bp+43], 1
.ok:
        mov dx, bp                  ; la recursión ha cambiado la DTA
        mov ah, 0x1A
        int 0x21
.n:
        mov ah, 0x4F
        int 0x21
        jnc .l
.empty:
        pop di
        mov byte [di], 0
        mov [dt_end], di
        mov al, [bp+43]
        add sp, 44
        pop bp
        dec byte [dt_depth]
        test al, al
        jnz .bad
        mov dx, dt_path
        mov ah, 0x3A
        int 0x21
        ret
.full:
        pop di
        add sp, 44
        pop bp
        dec byte [dt_depth]
.bad:
        stc
        ret

do_pause:
        mov si, msg_pause
        call puts
        mov ah, 0x08
        int 0x21
        jmp crlf

msg_redir   db "Error al crear el archivo", 13, 10, 0
msg_self    db "No se puede copiar un archivo sobre sí mismo", 13, 10, 0
msg_cantcreate db "No se puede crear el destino", 13, 10, 0
msg_cmany   db "Demasiados parámetros", 13, 10, 0
msg_over1   db "¿Sobrescribir ", 0
msg_over2   db " (Sí/No/Todos)? ", 0
s_copycmd   db "COPYCMD", 0
msg_copied  db " archivo(s) copiado(s)", 13, 10, 0
msg_full    db "Espacio insuficiente en el disco", 13, 10, 0
msg_delall  db "¡Se borrarán todos los archivos del directorio!", 13, 10
            db "¿Está seguro (S/N)? ", 0
msg_delp    db ",    ¿Eliminar (S/N)? ", 0
msg_accden  db "Acceso denegado", 13, 10, 0
msg_badparam db "Parámetro no válido", 13, 10, 0
msg_dupnf   db "Nombre de archivo duplicado o archivo no encontrado", 13, 10, 0
msg_mderr   db "No se puede crear el directorio", 13, 10, 0
msg_rderr   db "Ruta no válida, no es un directorio", 13, 10, "o el directorio no está vacío", 13, 10, 0
msg_pause   db "Pulse una tecla para continuar . . .", 0
PIPE_MAX    equ 10                  ; tuberías en una línea
PIPE_LEN    equ 80
s_pipe      db "~PIPE"
s_tmp       db ".TMP", 0
s_temp      db "TEMP", 0
msg_pipes   db "Demasiadas tuberías en la línea", 13, 10, 0
CBUF_LEN    equ 4096

; -----------------------------------------------------------------------------
;  Entorno propio para SET, PATH y PROMPT: 1 KB, o lo que diga /E:nnnn
;  (COMMAND /E:2048 /P, lo habitual en SHELL= de CONFIG.SYS)
; -----------------------------------------------------------------------------
ENV_SIZE    equ 1024

env_init:
        mov word [env_size], ENV_SIZE
        mov si, 0x81
.o:
        lodsb
        cmp al, 13
        je .od
        cmp al, '/'
        jne .o
        mov al, [si]
        call upcase
        cmp al, 'C'                 ; lo que sigue a /C es la orden
        je .od
        cmp al, 'E'
        jne .o
        cmp byte [si+1], ':'
        jne .o
        add si, 2
        call parse_u16
        jc .o
        cmp ax, 256
        jae .o1
        mov ax, 256
.o1:
        cmp ax, 32752
        jbe .o2
        mov ax, 32752
.o2:
        add ax, 15
        and ax, 0xFFF0
        mov [env_size], ax
        jmp .o
.od:
        mov bx, [env_size]
        shr bx, 4
        mov ah, 0x48
        int 0x21
        jc .r
        mov es, ax
        xor di, di
        mov cx, [env_size]
        shr cx, 1
        push ax
        xor ax, ax
        rep stosw
        pop ax
        xor di, di
        mov cx, [env_size]
        sub cx, 2
        push ds
        mov ds, [0x2C]
        xor si, si
.l:
        cmp byte [si], 0
        je .end
.c:
        lodsb
        stosb
        dec cx
        jz .end
        test al, al
        jnz .c
        jmp .l
.end:
        pop ds
        mov byte [es:di], 0
        mov bx, es
        push ds
        pop es
        ; liberar el entorno heredado (es nuestro) y usar el nuevo
        push bx
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        push ds
        pop es
        pop bx
        mov [0x2C], bx
        push es
        mov es, [r_psp]             ; (el PSP de verdad; aquí hay una copia)
        mov [es:0x2C], bx
        pop es
.r:
        push ds
        pop es
        ret

; SI = nombre (mayúsculas) -> ES:DI = entrada "NOMBRE=..."; CF si no está
env_find:
        mov es, [0x2C]
        xor di, di
.l:
        cmp byte [es:di], 0
        je .nf
        push si
        push di
.c:
        mov al, [si]
        test al, al
        jz .eq
        mov ah, [es:di]
        cmp al, ah
        jne .n
        inc si
        inc di
        jmp .c
.eq:
        cmp byte [es:di], '='
        jne .n
        pop di
        pop si
        clc
        ret
.n:
        pop di
        pop si
.sk:
        inc di
        cmp byte [es:di-1], 0
        jne .sk
        jmp .l
.nf:
        push ds
        pop es
        stc
        ret

; SI = nombre -> valor copiado en DS:DI (ASCIIZ); CF si no está
env_get:
        push di
        call env_find
        pop bx
        jc .r
.s:
        cmp byte [es:di], '='
        je .v
        inc di
        jmp .s
.v:
        inc di
        mov cx, 255                 ; tope: los búferes son de 256 o más
.c:
        mov al, [es:di]
        inc di
        test al, al
        jz .ce
        jcxz .c
        mov [bx], al
        inc bx
        dec cx
        jmp .c
.ce:
        mov byte [bx], 0
        push ds
        pop es
        clc
.r:
        ret

; SI = nombre (mayúsculas), DI = valor (vacío = borrar)
env_set:
        mov [es_name], si
        mov [es_val], di
        call env_find
        jc .add
        ; borrar la entrada existente
        mov si, di
.sk:
        inc si
        cmp byte [es:si-1], 0
        jne .sk
        push ds
        push es
        pop ds
.mv:
        cmp byte [si], 0
        je .mve
.ms:
        lodsb
        stosb
        test al, al
        jnz .ms
        jmp .mv
.mve:
        mov byte [es:di], 0
        pop ds
.add:
        mov si, [es_val]
        cmp byte [si], 0
        je .done
        ; final del entorno
        mov es, [0x2C]
        xor di, di
.e:
        cmp byte [es:di], 0
        je .gotend
.e2:
        inc di
        cmp byte [es:di-1], 0
        jne .e2
        jmp .e
.gotend:
        ; ¿cabe?
        mov si, [es_name]
        call strlen
        mov bx, cx
        mov si, [es_val]
        call strlen
        add bx, cx
        add bx, 3
        add bx, di
        cmp bx, [env_size]
        ja .full
        mov si, [es_name]
.cn:
        lodsb
        test al, al
        jz .cv
        stosb
        jmp .cn
.cv:
        mov al, '='
        stosb
        mov si, [es_val]
.cvl:
        lodsb
        stosb
        test al, al
        jnz .cvl
        mov byte [es:di], 0
.done:
        push ds
        pop es
        ret
.full:
        push ds
        pop es
        mov si, msg_envfull
        jmp puts

; SI -> CX = longitud
strlen:
        push si
        xor cx, cx
.l:
        lodsb
        test al, al
        jz .e
        inc cx
        jmp .l
.e:
        pop si
        ret

; ¿la palabra en SI (hasta espacio o fin) es DI (mayúsculas)? CF=0 si sí
word_is:
        push si
        push di
.l:
        mov al, [si]
        call upcase
        mov ah, [di]
        test ah, ah
        jz .end
        cmp al, ah
        jne .no
        inc si
        inc di
        jmp .l
.end:
        test al, al
        jz .yes
        cmp al, ' '
        je .yes
        cmp al, 9
        je .yes
.no:
        pop di
        pop si
        stc
        ret
.yes:
        pop di
        pop si
        clc
        ret

do_path:
        call skip_eq                ; «PATH=C:\DOS» como en MS-DOS
        cmp byte [si], 0
        jne .set
        mov si, s_pathvar
        mov di, pbuf
        call env_get
        jc .none
        mov si, s_pathvar
        call puts
        mov al, '='
        call putc
        mov si, pbuf
        call puts
        jmp crlf
.none:
        mov si, msg_nopath
        jmp puts
.set:
        cmp word [si], ';'
        jne .s2
        mov byte [si], 0
.s2:
        mov di, si
        mov si, s_pathvar
        jmp env_set

; saltar un «=» pegado a la orden (PATH=..., PROMPT=...)
skip_eq:
        cmp byte [si], '='
        jne .r
        inc si
        call skip_sp
.r:
        ret

do_prompt:
        call skip_eq
        mov di, si
        mov si, s_promptvar
        jmp env_set

; escribir la fecha y la hora actuales (para PROMPT $D $T)
put_date:
        mov ah, 0x2A
        int 0x21
        push cx
        push dx
        movzx bx, al
        shl bx, 2
        lea si, [days+bx]
        call puts
        mov al, ' '
        call putc
        pop dx
        push dx
        movzx ax, dl
        call put2
        mov al, '-'
        call putc
        pop dx
        movzx ax, dh
        call put2
        mov al, '-'
        call putc
        pop cx
        jmp put_year
put_time:
        mov ah, 0x2C
        int 0x21
        push dx
        push cx
        movzx eax, ch
        mov cl, 2
        call putnum_w
        mov al, ':'
        call putc
        pop cx
        movzx ax, cl
        call put2
        mov al, ':'
        call putc
        pop dx
        movzx ax, dh
        jmp put2

; -----------------------------------------------------------------------------
;  Ficheros por lotes (.BAT)
; -----------------------------------------------------------------------------
BC_NAME     equ 0
BC_POS      equ 128
BC_SHIFT    equ 132
BC_ARGS     equ 134
BC_LEN      equ 256
BMAX        equ 4
BLINE       equ 200

; -> BX = contexto actual
cur_ctx:
        movzx bx, byte [bdepth]
        dec bx
        shl bx, 8
        add bx, bctx
        ret

; progpath = .BAT, cmdword y argp = orden y argumentos
start_batch:
        cmp byte [call_flag], 0
        jne .push
        cmp byte [bdepth], 0
        jne .set
.push:
        cmp byte [bdepth], BMAX
        jb .ok
        mov si, msg_toodeep
        jmp puts
.ok:
        inc byte [bdepth]
.set:
        mov byte [call_flag], 0
        call cur_ctx
        mov si, progpath
        lea di, [bx+BC_NAME]
        mov ah, 0x60
        int 0x21
        mov dword [bx+BC_POS], 0
        mov word [bx+BC_SHIFT], 0
        lea di, [bx+BC_ARGS]
        lea dx, [bx+BC_LEN-2]
        mov si, cmdword
.c1:
        lodsb
        test al, al
        jz .c2
        stosb
        jmp .c1
.c2:
        mov si, [argp]
.c3:
        cmp di, dx
        jae .c4
        lodsb
        stosb
        test al, al
        jnz .c3
        ret
.c4:
        mov byte [di], 0
        ret

; BX = contexto, EAX = posición -> bline, EAX = siguiente; CF al final
batch_read:
        mov [br_pos], eax
        push bx
        lea dx, [bx+BC_NAME]
        mov ax, 0x3D00
        int 0x21
        jc .eofp
        mov bx, ax
        mov dx, [br_pos]
        mov cx, [br_pos+2]
        mov ax, 0x4200
        int 0x21
        mov dx, rbuf
        mov cx, BLINE
        mov ah, 0x3F
        int 0x21
        jnc .rd
        xor ax, ax
.rd:
        push ax
        mov ah, 0x3E
        int 0x21
        pop ax
        pop bx
        test ax, ax
        jz .eof
        mov cx, ax
        mov si, rbuf
        mov di, bline
        xor dx, dx
        cmp byte [si], 0x1A
        je .eof
.l:
        jcxz .done
        lodsb
        dec cx
        inc dx
        cmp al, 13
        je .cr
        cmp al, 10
        je .done
        cmp al, 0x1A
        je .z
        cmp di, bline + BLINE - 1
        jae .l
        stosb
        jmp .l
.cr:
        jcxz .done
        cmp byte [si], 10
        jne .done
        inc dx
        jmp .done
.z:
        dec dx
.done:
        mov byte [di], 0
        movzx eax, dx
        add eax, [br_pos]
        clc
        ret
.eofp:
        pop bx
.eof:
        stc
        ret

; siguiente línea del lote -> eline; CF si no hay nada que ejecutar
batch_line:
        call cur_ctx
        mov eax, [bx+BC_POS]
        call batch_read
        jc .eof
        call cur_ctx
        mov [bx+BC_POS], eax
        mov si, bline
        call skip_sp
        cmp byte [si], 0
        je .skip
        cmp byte [si], ':'
        je .skip
        call expand
        cmp byte [echo_on], 0
        je .ok
        mov si, eline
        call skip_sp
        cmp byte [si], '@'
        je .ok
        call crlf
        call prompt
        mov si, eline
        call puts
        call crlf
.ok:
        clc
        ret
.skip:
        stc
        ret
.eof:
        dec byte [bdepth]
        jnz .skip
        mov byte [echo_on], 1
        stc
        ret

; bline -> eline con %0-%9, %% y %VARIABLE% sustituidos
expand:
        mov si, bline
        mov di, eline
.l:
        cmp di, eline + BLINE - 1
        jae .end
        lodsb
        test al, al
        jz .end
        cmp al, '%'
        jne .st
        mov al, [si]
        cmp al, '%'
        je .pct
        cmp al, '0'
        jb .var
        cmp al, '9'
        ja .var
        inc si
        sub al, '0'
        push si
        call get_arg
        mov ax, eline + BLINE - 1   ; lo que queda en eline
        sub ax, di
        cmp cx, ax
        jbe .fits
        mov cx, ax
.fits:
        rep movsb
        pop si
        jmp .l
.pct:
        inc si
        mov al, '%'
        jmp .st
.var:
        ; buscar el % de cierre
        mov bx, si
.f:
        mov al, [bx]
        test al, al
        jz .lit
        cmp al, ' '
        je .lit
        cmp al, '%'
        je .gotv
        inc bx
        jmp .f
.lit:
        mov al, '%'
        jmp .st
.gotv:
        push di
        mov di, vname
.vn:
        cmp si, bx
        je .vne
        lodsb
        call upcase
        cmp di, vname + 63
        jae .vn
        stosb
        jmp .vn
.vne:
        mov byte [di], 0
        pop di
        inc si
        push si
        mov si, vname
        push di
        mov di, vval
        call env_get
        pop di
        jc .novar
        mov si, vval
.cp:
        lodsb
        test al, al
        jz .novar
        cmp di, eline + BLINE - 1
        jae .novar
        stosb
        jmp .cp
.novar:
        pop si
        jmp .l
.st:
        stosb
        jmp .l
.end:
        mov byte [di], 0
        ret

; AL = número de parámetro -> SI = texto, CX = longitud (0 si no hay)
get_arg:
        push bx
        push dx
        movzx dx, al
        call cur_ctx
        add dx, [bx+BC_SHIFT]
        lea si, [bx+BC_ARGS]
.tok:
        ; saltar separadores
.sp:
        mov al, [si]
        call is_sep
        jc .gs
        inc si
        jmp .sp
.gs:
        cmp byte [si], 0
        je .none
        test dx, dx
        jz .this
        ; saltar este parámetro
.skip:
        mov al, [si]
        test al, al
        jz .none
        call is_sep
        jc .sk1
        dec dx
        jmp .tok
.sk1:
        inc si
        jmp .skip
.this:
        mov bx, si
.ln:
        mov al, [bx]
        test al, al
        jz .lnd
        call is_sep
        jnc .lnd
.lnd0:
        inc bx
        jmp .ln
.lnd:
        mov cx, bx
        sub cx, si
        pop dx
        pop bx
        ret
.none:
        xor cx, cx
        pop dx
        pop bx
        ret

; AL -> CF=0 si es separador de parámetros
is_sep:
        cmp al, ' '
        je .y
        cmp al, 9
        je .y
        cmp al, ','
        je .y
        cmp al, ';'
        je .y
        cmp al, '='
        je .y
        stc
        ret
.y:
        clc
        ret

do_shift:
        cmp byte [bdepth], 0
        je .r
        call cur_ctx
        inc word [bx+BC_SHIFT]
.r:
        ret

; LH / LOADHIGH programa: lo carga en la memoria superior (UMB) si la hay y
; cabe; si no, en la convencional, como siempre
do_lh:
        mov byte [lh_flag], 1
        call execute_line
        mov byte [lh_flag], 0
        ret

do_call:
        mov al, [bdepth]
        push ax
        mov byte [call_flag], 1
        call execute_line
        mov byte [call_flag], 0
        pop ax
        ; Dentro de un FOR (FOR %%A IN (...) DO CALL OTRO %%A) el lote llamado
        ; se ejecuta ahora mismo, antes del elemento siguiente; si no, los
        ; lotes se apilarían y correrían después y al revés.
        cmp byte [in_for], 0
        je .r
        cmp byte [pl_depth], 0      ; no dentro de una tubería
        jne .r
        cmp [bdepth], al
        jbe .r
        push ax                     ; profundidad a la que hay que volver
        push word [sv_in]
        push word [sv_out]
        push word [pl_in]
        push word [pl_out]
        sub sp, FOR_STLEN           ; el estado del FOR, a la pila
        mov di, sp
        mov si, for_state
        mov cx, FOR_STLEN
        rep movsb
.loop:
        mov bx, sp
        mov al, [bx+FOR_STLEN+8]
        cmp [bdepth], al
        jbe .done
        call batch_line
        jc .loop
        mov si, eline
        mov word [pl_in], 0
        mov byte [pl_depth], 0
        call run_line
        jmp .loop
.done:
        mov si, sp
        mov di, for_state
        mov cx, FOR_STLEN
        rep movsb
        add sp, FOR_STLEN
        mov byte [pl_depth], 0
        pop word [pl_out]
        pop word [pl_in]
        pop word [sv_out]
        pop word [sv_in]
        pop ax
.r:
        ret

do_goto:
        cmp byte [bdepth], 0
        je .r
        cmp byte [si], ':'
        jne .n
        inc si
.n:
        mov di, glabel
        call get_tok
        jc .r
        mov byte [glabel+8], 0      ; como MS-DOS: cuentan los 8 primeros
        call cur_ctx
        xor eax, eax
.l:
        push bx
        call batch_read
        pop bx
        jc .nf
        mov si, bline
        call skip_sp
        cmp byte [si], ':'
        jne .l
        inc si
        mov di, glabel
.c:
        mov dl, [si]
        mov dh, [di]
        test dh, dh
        jz .end
        push ax
        mov al, dl
        call upcase
        mov dl, al
        mov al, dh
        call upcase
        mov dh, al
        pop ax
        cmp dl, dh
        jne .l
        inc si
        inc di
        jmp .c
.end:
        cmp di, glabel + 8          ; iguales los 8 primeros: vale
        je .ok
        cmp dl, 0
        je .ok
        cmp dl, ' '
        je .ok
        cmp dl, 9
        jne .l
.ok:
        mov [bx+BC_POS], eax
.r:
        ret
.nf:
        mov si, msg_nolabel
        call puts
        dec byte [bdepth]
        jnz .r
        mov byte [echo_on], 1       ; el lote acaba aquí: vuelve el indicador
        ret

; IF [NOT] ERRORLEVEL n | EXIST fichero | a==b  orden
do_if:
        mov byte [if_not], 0
        mov di, s_not
        call word_is
        jc .t
        mov byte [if_not], 1
        add si, 3
        call skip_sp
.t:
        mov di, s_errorlevel
        call word_is
        jnc .el
        mov di, s_exist
        call word_is
        jnc .ex
        ; comparación de cadenas
        mov di, if_a
        call get_tok
        jc .syn
        mov di, if_a
.fe:
        mov al, [di]
        test al, al
        jz .noeq
        cmp word [di], '=='
        je .eq
        inc di
        jmp .fe
.noeq:
        call skip_sp
        cmp word [si], '=='
        jne .syn
        add si, 2
        mov di, if_b
        call skip_sp
        call get_tok
        jc .syn
        jmp .cmp
.eq:
        mov byte [di], 0
        add di, 2
        cmp byte [di], 0
        jne .inl
        mov di, if_b
        call get_tok
        jc .syn
        jmp .cmp
.inl:
        push si
        mov si, di
        mov di, if_b
.ci:
        lodsb
        stosb
        test al, al
        jnz .ci
        pop si
.cmp:
        push si
        mov si, if_a
        mov di, if_b
.cl:
        mov al, [si]
        cmp al, [di]
        jne .cf
        inc si
        inc di
        test al, al
        jnz .cl
        pop si
        mov al, 1
        jmp .res
.cf:
        pop si
        xor al, al
        jmp .res
.el:
        add si, 10
        call skip_sp
        call parse_dec
        jc .syn
        cmp [errlvl], al
        mov al, 1
        jae .res
        xor al, al
        jmp .res
.ex:
        add si, 5
        mov di, if_a
        call get_tok
        jc .syn
        push si
        mov dx, dta2
        mov ah, 0x1A
        int 0x21
        mov dx, if_a
        mov cx, 0x06
        mov ah, 0x4E
        int 0x21
        jnc .exy
        call if_device              ; IF EXIST carpeta\NUL
.exy:
        pop si
        mov al, 1
        jnc .res
        xor al, al
.res:
        xor al, [if_not]
        jz .r
        jmp execute_line
.syn:
        mov si, msg_syntax
        jmp puts
.r:
        ret

; if_a = «[carpeta\]dispositivo» (NUL, CON...): CF=0 si la carpeta existe.
; Es la forma clásica de saber si existe una carpeta: IF EXIST C:\DOS\NUL
if_device:
        mov si, if_a
        mov di, truebuf
        call path_prefix
        ; ¿el nombre es un dispositivo?
        mov si, truebuf
        call strlen
        mov dx, if_a
        add dx, cx
        mov si, dx
        call has_wild
        jc .no
        mov ax, 0x3D00
        int 0x21
        jc .no
        mov bx, ax
        mov ax, 0x4400
        int 0x21
        push dx
        mov ah, 0x3E
        int 0x21
        pop dx
        test dl, 0x80
        jz .no
        ; ¿y la carpeta existe?
        mov si, truebuf
        call strlen
        jcxz .yes
        mov bx, cx
        cmp byte [si+bx-1], ':'
        je .yes
        cmp bx, 1
        je .yes                     ; «\»
        cmp byte [si+bx-2], ':'
        je .yes                     ; «X:\»
        mov byte [si+bx-1], 0
        jmp is_dir
.yes:
        clc
        ret
.no:
        stc
        ret

; SI -> AX = número decimal; CF si no hay cifras
parse_u16:
        xor ax, ax
        xor cx, cx
.l:
        movzx bx, byte [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        imul ax, ax, 10
        add ax, bx
        inc si
        inc cx
        jmp .l
.e:
        cmp cx, 1
        ret

; SI -> AL = número decimal (0-255); CF si no hay cifras
parse_dec:
        xor ax, ax
        xor cx, cx
.l:
        mov bl, [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        mov bh, 10
        mul bh
        add al, bl
        inc si
        inc cx
        jmp .l
.e:
        cmp cx, 1
        ret

; FOR %v IN (conjunto) DO orden
do_for:
        cmp byte [si], '%'
        jne .syn
        mov al, [si+1]
        call upcase
        mov [for_var], al
        add si, 2
        call skip_sp
        mov di, s_in
        call word_is
        jc .syn
        add si, 2
        call skip_sp
        cmp byte [si], '('
        jne .syn
        inc si
        mov di, for_set
.cs:
        lodsb
        test al, al
        jz .syn
        cmp al, ')'
        je .cse
        stosb
        jmp .cs
.cse:
        mov byte [di], 0
        call skip_sp
        mov di, s_do
        call word_is
        jc .syn
        add si, 2
        call skip_sp
        mov di, for_tmpl
.ct:
        lodsb
        stosb
        test al, al
        jnz .ct
        ; recorrer el conjunto
        mov byte [in_for], 1
        mov word [for_pos], for_set
.item:
        mov si, [for_pos]
.isp:
        mov al, [si]
        test al, al
        jz .done
        call is_sep
        jc .igot
        inc si
        jmp .isp
.igot:
        mov di, for_item
.ic:
        mov al, [si]
        test al, al
        jz .ice
        call is_sep
        jnc .ice
        stosb
        inc si
        jmp .ic
.ice:
        mov byte [di], 0
        mov [for_pos], si
        mov si, for_item
        call has_wild
        jc .wild
        mov si, for_item
        call for_run
        jmp .item
.wild:
        mov si, for_item
        mov di, for_pre
        call path_prefix
        mov dx, for_dta
        mov ah, 0x1A
        int 0x21
        mov dx, for_item
        xor cx, cx
        mov ah, 0x4E
        int 0x21
        jc .item
.wl:
        mov si, for_pre
        mov bx, for_dta+0x1E
        mov di, for_full
        call join_path
        mov si, for_full
        call for_run
        mov dx, for_dta
        mov ah, 0x1A
        int 0x21
        mov ah, 0x4F
        int 0x21
        jnc .wl
        jmp .item
.done:
        mov byte [in_for], 0
        ret
.syn:
        mov si, msg_syntax
        jmp puts

; SI = elemento -> ejecutar la plantilla con %v sustituido
for_run:
        mov bx, si
        mov si, for_tmpl
        mov di, for_cmd
.l:
        lodsb
        test al, al
        jz .e
        cmp al, '%'
        jne .st
        mov ah, [si]
        push ax
        mov al, ah
        call upcase
        cmp al, [for_var]
        pop ax
        jne .st
        inc si
        push si
        mov si, bx
.cp:
        lodsb
        test al, al
        jz .cpe
        cmp di, for_cmd + 255
        jae .cp
        stosb
        jmp .cp
.cpe:
        pop si
        jmp .l
.st:
        cmp di, for_cmd + 255
        jae .l
        stosb
        jmp .l
.e:
        stosb
        mov si, for_cmd
        jmp execute_line

msg_echoon  db "ECHO está activado", 0
msg_echooff db "ECHO está desactivado", 0
msg_syntax  db "Error de sintaxis", 13, 10, 0
msg_envfull db "No queda espacio en el entorno", 13, 10, 0
msg_nopath  db "Sin ruta", 13, 10, 0
msg_nolabel db "No se encontró la etiqueta", 13, 10, 0
msg_toodeep db "Demasiados archivos de proceso por lotes anidados", 13, 10, 0
msg_vername db "Hispa-DOS versión 0.9", 0
s_on        db "ON", 0
s_off       db "OFF", 0
s_not       db "NOT", 0
s_errorlevel db "ERRORLEVEL", 0
s_exist     db "EXIST", 0
s_in        db "IN", 0
s_do        db "DO", 0
s_pathvar   db "PATH", 0
s_promptvar db "PROMPT", 0
s_defprompt db "$P$G", 0
s_autoexec  db "A:\AUTOEXEC.BAT", 0
s_alias     db "A:\HISPADOS\ALIAS.INI", 0
s_empty     db 0
; -----------------------------------------------------------------------------
msg_hello   db 13, 10, "Intérprete de órdenes de Hispa-DOS 0.9", 13, 10
            db "Escriba AYUDA para ver la lista de órdenes.", 13, 10, 13, 10, 0
msg_ver     db 13, 10, "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)", 13, 10
            db "Versión de DOS notificada: ", 0
msg_badcmd  db "Comando o nombre de archivo incorrecto", 13, 10, 0
msg_execerr db "No se puede ejecutar el programa", 13, 10, 0
msg_baddir  db "Directorio no válido", 13, 10, 0
msg_baddrv  db "Especificación de unidad no válida", 13, 10, 0
msg_nrdy    db "La unidad no está preparada", 13, 10, 0
msg_nofile  db "No se encontró el archivo", 13, 10, 0
msg_reqparam db "Falta un parámetro obligatorio", 13, 10, 0
msg_vol1    db " El volumen de la unidad "
vol1_l      db "A es ", 0
msg_vol0    db " El volumen de la unidad "
vol0_l      db "A no tiene etiqueta", 0
msg_dirof   db " Directorio de ", 0
msg_dtask1  db "¿Eliminar ", 0
msg_dtask2  db " y todo su contenido? (S/N) ", 0
msg_dtask3  db "? (S/N) ", 0
msg_dtdel   db "Eliminando ", 0
msg_dtdots  db "...", 13, 10, 0
msg_dterr   db "No se ha podido eliminar todo.", 13, 10, 0
msg_dtnf    db "No se encontró el archivo", 13, 10, 0
msg_mmbad   db "Parámetro no válido. Uso: MEM [/C | /F | /D | /M nombre] [/P]", 13, 10, 0
msg_mmmore  db "Pulse una tecla para continuar . . .", 0
msg_mmc1    db "Programas en la memoria convencional:", 0
msg_mmc1u   db "Programas en la memoria superior (UMB):", 0
msg_mmc2    db "  Nombre                 Total", 0
msg_mmc3    db "  --------  ------------------------", 0
msg_mmtot   db "  Total     ", 0
msg_mmnoumb db "No hay memoria superior (UMB) disponible.", 0
msg_mmf1    db "Bloques libres de la memoria convencional:", 0
msg_mmf2    db "  Segmento               Total", 0
msg_mmf3    db "  --------  ------------------------", 0
msg_mmftot  db "  Total libre     ", 0
msg_mmfbig  db "  Mayor bloque    ", 0
msg_mmm1    db "El programa ", 0
msg_mmm2    db "usa esta memoria:", 0
msg_mmm3    db "  Segmento               Total     Tipo", 0
msg_mmm4    db "  --------  ------------------------  ---------", 0
msg_mmnf1   db "El programa ", 0
msg_mmnf2   db " no está en la memoria.", 0
msg_mmd1    db "Memoria convencional, bloque a bloque:", 0
msg_mmd2    db "  Segmento               Total     Nombre    Tipo", 0
msg_mmd3    db "  --------  ------------------------  --------  ---------", 0
msg_mmivt   db "    0000             1.024 (1K)    Tabla de interrupciones", 0
msg_mmbda   db "    0040               256 (0K)    Datos de la BIOS", 0
msg_mmkern  db "  SISTEMA   Núcleo de Hispa-DOS", 0
msg_mmdev   db "Controladores de dispositivo:", 0
msg_mmchr   db "  (carácter)", 0
msg_mmblk   db " unidad(es) (bloque)", 0
s_mmind     db "    ", 0
s_mmfree    db "Libre", 0
s_mmsys     db "SISTEMA", 0
mm_t0       db "Libre", 0
mm_t1       db "Sistema", 0
mm_t2       db "Programa", 0
mm_t3       db "Entorno", 0
mm_t4       db "Datos", 0
mm_types    dw mm_t0, mm_t1, mm_t2, mm_t3, mm_t4
msg_files   db " archivo(s)", 0
msg_bytes   db " bytes", 13, 10, 0
msg_free    db " bytes libres", 13, 10, 0
msg_mtotal  db " bytes de memoria convencional en total", 13, 10, 0
msg_mfree   db " bytes libres", 13, 10, 0
msg_mutotal db " bytes de memoria superior (UMB) en total", 13, 10, 0
msg_mularge db " bytes del mayor bloque superior libre", 13, 10, 0
msg_mlarge  db " bytes del mayor programa ejecutable", 13, 10, 0
msg_mhma    db "Hispa-DOS está cargado en la memoria alta (HMA)", 13, 10, 0
msg_baddate db "Fecha no válida (dd-mm-aaaa)", 13, 10, 0
msg_badtime db "Hora no válida (hh:mm[:ss])", 13, 10, 0
msg_date    db "La fecha actual es ", 0
msg_time    db "La hora actual es ", 0
msg_help    db "Lista de órdenes internas (nombre español / nombre clásico):", 13, 10
            db "  AYUDA/HELP        COPIAR/COPY       BORRAR/DEL,ERASE  RENOMBRAR/REN", 13, 10
            db "  LISTAR/DIR        MOSTRAR/TYPE      CREARDIR/MD       BORRARDIR/RD", 13, 10
            db "  CD/CHDIR          LIMPIAR/CLS       FECHA/DATE        HORA/TIME", 13, 10
            db "  VERSION/VER       MEMORIA/MEM       VOLUMEN/VOL       PAUSA/PAUSE", 13, 10
            db "  ECO/ECHO          RUTA/PATH         LLAMAR/CALL       SALIR/EXIT", 13, 10
            db "  BORRARARBOL/DELTREE (carpetas con todo su contenido)", 13, 10
            db "  SET  PROMPT  REM  IF  FOR  GOTO  SHIFT", 13, 10
            db "Redirecciones: > >> < y tuberías |. Archivos por lotes: .BAT, AUTOEXEC.BAT.", 13, 10
            db "Los programas (.COM, .EXE, .BAT) se buscan en el directorio actual y", 13, 10
            db "después en PATH. Programas en \HISPADOS (también con su nombre clásico):", 13, 10
            db "  PARTIR (FDISK), FORMATEA, SISTEMA, INSTALAR, TECLADO, SONAR, SONIDO, EDITAR,", 13, 10
            db "  GESTOR, INFO, MOVER, ATRIBUTO, ARBOL, BUSCAR, ORDENAR, PAGINAR, ELEGIR,", 13, 10
            db "  EXPANDIR, ETIQUETA, XCOPIAR, REVISAR (CHKDSK), MANDO (mandos de juego),", 13, 10
            db "  VELOCIDAD (frenar el procesador para juegos antiguos), LOADFIX.", 13, 10
            db "AYUDA orden (u «orden /?») explica cada orden.", 13, 10
            db "Flechas arriba/abajo: órdenes anteriores. F3: repetir la última. Esc: borrar.", 13, 10
            db "Tab: completar nombres de archivos y carpetas (otra vez: el siguiente).", 13, 10
            db "Flechas izquierda/derecha, Inicio, Fin y Ctrl+flechas (por palabras): mover el", 13, 10
            db "cursor por la línea. Supr y Retroceso: borrar. Insertar: insertar/sobrescribir.", 13, 10
            db "En un Mac (TECLADO /MAC): Ctrl+Retroceso = Supr, Alt+flechas = Inicio/Fin.", 13, 10, 0
help_tab:
        dw do_dir, h_dir
        dw do_type, h_type
        dw do_cd, h_cd
        dw do_echo, h_echo
        dw do_set, h_set
        dw do_mem, h_mem
        dw do_date, h_date
        dw do_time, h_time
        dw do_vol, h_vol
        dw do_help, h_help
        dw do_rem, h_rem
        dw do_exit, h_exit
        dw do_copy, h_copy
        dw do_del, h_del
        dw do_ren, h_ren
        dw do_md, h_md
        dw do_rd, h_rd
        dw do_deltree, h_deltree
        dw do_pause, h_pause
        dw do_if, h_if
        dw do_goto, h_goto
        dw do_for, h_for
        dw do_shift, h_shift
        dw do_call, h_call
        dw do_path, h_path
        dw do_prompt, h_prompt
        dw do_cls, h_cls
        dw do_ver, h_ver
        dw 0
h_dir       db "Muestra los archivos y carpetas de una carpeta.", 13, 10
            db "", 13, 10
            db "LISTAR (DIR) [unidad:][ruta][archivo] [/P] [/W] [/A[[:]atributos]]", 13, 10
            db "      [/O[[:]orden]] [/S] [/B] [/L]", 13, 10
            db "", 13, 10
            db "  /P  pausa en cada pantalla llena     /W  formato ancho", 13, 10
            db "  /A  atributos: D carpetas, H ocultos, S de sistema, R solo lectura,", 13, 10
            db "      A para archivar; «-» delante = los que no lo tienen", 13, 10
            db "  /O  orden: N nombre, E extensión, G carpetas primero, S tamaño,", 13, 10
            db "      D fecha y hora; «-» delante = al revés", 13, 10
            db "  /S  también las subcarpetas   /B  solo los nombres   /L  en minúsculas", 13, 10
            db "Las opciones de la variable DIRCMD se aplican antes; /-W anula /W, etc.", 13, 10, 0
h_type      db "Muestra el contenido de un archivo de texto.", 13, 10
            db "", 13, 10
            db "MOSTRAR (TYPE) [unidad:][ruta]archivo", 13, 10, 0
h_cd        db "Muestra o cambia la carpeta actual.", 13, 10
            db "", 13, 10
            db "CD [unidad:][ruta]", 13, 10
            db "CD ..   sube a la carpeta anterior", 13, 10
            db "CD \    va a la raíz", 13, 10
            db "Sin parámetros muestra la carpeta actual.", 13, 10, 0
h_echo      db "Muestra un mensaje o activa y desactiva el eco de las órdenes.", 13, 10
            db "", 13, 10
            db "ECO (ECHO) [ON | OFF]", 13, 10
            db "ECO (ECHO) mensaje", 13, 10
            db "ECHO.   escribe una línea vacía", 13, 10, 0
h_set       db "Muestra, pone o quita variables de entorno.", 13, 10
            db "", 13, 10
            db "SET [variable=[texto]]", 13, 10
            db "Sin parámetros muestra todas las variables.", 13, 10, 0
h_mem       db "Muestra la memoria usada y la libre.", 13, 10
            db "", 13, 10
            db "MEMORIA (MEM) [/C] [/F] [/M programa] [/D] [/P]", 13, 10
            db "", 13, 10
            db "  /C  programas en memoria    /F  bloques libres", 13, 10
            db "  /M  un programa en detalle  /D  todo en detalle  /P  pausa por pantallas", 13, 10, 0
h_date      db "Muestra o cambia la fecha.", 13, 10
            db "", 13, 10
            db "FECHA (DATE) [dd-mm-aaaa]", 13, 10, 0
h_time      db "Muestra o cambia la hora.", 13, 10
            db "", 13, 10
            db "HORA (TIME) [hh:mm[:ss]]", 13, 10, 0
h_vol       db "Muestra la etiqueta y el número de serie de un disco.", 13, 10
            db "", 13, 10
            db "VOLUMEN (VOL) [unidad:]", 13, 10, 0
h_help      db "Muestra la lista de órdenes o la ayuda de una orden.", 13, 10
            db "", 13, 10
            db "AYUDA (HELP) [orden]", 13, 10
            db "AYUDA orden es lo mismo que «orden /?».", 13, 10, 0
h_rem       db "Comentario en un archivo por lotes o en CONFIG.SYS.", 13, 10
            db "", 13, 10
            db "REM [comentario]", 13, 10, 0
h_exit      db "Sale del intérprete de órdenes (si se abrió desde un programa).", 13, 10
            db "", 13, 10
            db "SALIR (EXIT)", 13, 10, 0
h_copy      db "Copia uno o más archivos.", 13, 10
            db "", 13, 10
            db "COPIAR (COPY) origen [destino]", 13, 10
            db "COPIAR origen1+origen2[+...] destino   (une archivos)", 13, 10
            db "COPIAR CON archivo   (escribe desde el teclado; termina con Ctrl+Z)", 13, 10
            db "El origen y el destino admiten comodines (* y ?): COPY *.TXT *.BAK", 13, 10
            db "", 13, 10
            db "  /Y   sobrescribe sin preguntar       /-Y  pregunta antes", 13, 10
            db "       (por omisión pregunta, salvo en los archivos por lotes; la", 13, 10
            db "       variable COPYCMD puede llevar /Y o /-Y)", 13, 10
            db "  /A   texto (hasta Ctrl+Z)   /B  binario   /V  comprobar (se acepta)", 13, 10, 0
h_del       db "Borra uno o más archivos.", 13, 10
            db "", 13, 10
            db "BORRAR (DEL, ERASE) [unidad:][ruta]archivo [/P]", 13, 10
            db "  /P  pregunta antes de borrar cada archivo", 13, 10
            db "Con *.* pregunta antes de borrar.", 13, 10, 0
h_ren       db "Cambia el nombre de uno o más archivos.", 13, 10
            db "", 13, 10
            db "RENOMBRAR (REN, RENAME) archivo nombre_nuevo", 13, 10
            db "Admite comodines: REN *.TXT *.BAK", 13, 10, 0
h_md        db "Crea una carpeta.", 13, 10
            db "", 13, 10
            db "CREARDIR (MD, MKDIR) [unidad:]ruta", 13, 10, 0
h_rd        db "Borra una carpeta vacía.", 13, 10
            db "", 13, 10
            db "BORRARDIR (RD, RMDIR) [unidad:]ruta", 13, 10
            db "Para borrar una carpeta con todo su contenido: BORRARARBOL.", 13, 10, 0
h_deltree   db "Borra una carpeta con todos sus archivos y subcarpetas.", 13, 10
            db "", 13, 10
            db "BORRARARBOL (DELTREE) [/Y] [unidad:]ruta", 13, 10
            db "  /Y  no preguntar", 13, 10, 0
h_pause     db "Espera a que se pulse una tecla (en archivos por lotes).", 13, 10
            db "", 13, 10
            db "PAUSA (PAUSE)", 13, 10, 0
h_if        db "Ejecuta una orden según una condición (archivos por lotes).", 13, 10
            db "", 13, 10
            db "IF [NOT] ERRORLEVEL número orden", 13, 10
            db "IF [NOT] texto1==texto2 orden", 13, 10
            db "IF [NOT] EXIST archivo orden", 13, 10, 0
h_goto      db "Salta a una etiqueta de un archivo por lotes.", 13, 10
            db "", 13, 10
            db "GOTO etiqueta", 13, 10
            db "La etiqueta es una línea que empieza por dos puntos (:etiqueta).", 13, 10, 0
h_for       db "Repite una orden para cada elemento de una lista.", 13, 10
            db "", 13, 10
            db "FOR %variable IN (lista) DO orden", 13, 10
            db "(en archivos por lotes, %%variable)", 13, 10, 0
h_shift     db "Desplaza los parámetros de un archivo por lotes (%1 pasa a %0...).", 13, 10
            db "", 13, 10
            db "SHIFT", 13, 10, 0
h_call      db "Ejecuta un archivo por lotes desde otro y vuelve después.", 13, 10
            db "", 13, 10
            db "LLAMAR (CALL) archivo [parámetros]", 13, 10, 0
h_path      db "Muestra o cambia las carpetas donde se buscan los programas.", 13, 10
            db "", 13, 10
            db "RUTA (PATH) [[unidad:]ruta[;...]]", 13, 10
            db "PATH ;   borra la lista", 13, 10, 0
h_prompt    db "Cambia el indicador de órdenes.", 13, 10
            db "", 13, 10
            db "PROMPT [texto]", 13, 10
            db "", 13, 10
            db "  $P unidad y carpeta  $G >  $L <  $N unidad  $D fecha  $T hora", 13, 10
            db "  $V versión  $_ salto de línea  $$ $  $E Esc", 13, 10, 0
h_cls       db "Borra la pantalla.", 13, 10
            db "", 13, 10
            db "LIMPIAR (CLS)", 13, 10, 0
h_ver       db "Muestra la versión de Hispa-DOS.", 13, 10
            db "", 13, 10
            db "VERSION (VER)", 13, 10, 0
h_none      db "No hay ayuda para esta orden.", 13, 10, 0
days        db "Dom", 0, "Lun", 0, "Mar", 0, "Mié", 0, "Jue", 0, "Vie", 0, "Sáb", 0
s_drive     db "A:\", 0
s_all       db "*.*", 0
s_lab       db "A:\*.*", 0
s_com       db ".COM", 0
s_exe       db ".EXE", 0
s_bat       db ".BAT", 0
s_dirtag    db "    <DIR>     ", 0
s_dircmd    db "DIRCMD", 0
msg_dbad    db "Parámetro no válido - ", 0
msg_dmany   db "Demasiados parámetros - ", 0
msg_denv    db "(Error en la variable de entorno DIRCMD)", 13, 10, 0
msg_dtotal  db "Total de archivos listados:", 0
msg_dcont   db "(continuando ", 0
s_okeys     db "NEGSD", 0
d_ftab      db 'P'
            dw d_page
            db 'W'
            dw d_wide
            db 'S'
            dw d_sub
            db 'B'
            dw d_bare
            db 'L'
            dw d_lower
            db 0
d_atab      db 'D', 0x10, 'H', 0x02, 'S', 0x04, 'R', 0x01, 'A', 0x20, 0
s_crlf      db 13, 10, 0

prog_end_data:
        section .bss
bss_start:
inbuf       resb 130
cmdword     resb 64
argp        resw 1
curdir         resb 68
dta         resb 128
lbl         resb 14
help_line   resb 108
pattern     resb 132
pat_end     resw 1
truebuf     resb 132
nfiles      resd 1
nbytes      resd 1
numbuf      resb 16
nm11        resb 12
d_args      resw 1
d_opts:                             ; opciones de DIR (se ponen a cero)
d_page      resb 1
d_wide      resb 1
d_sub       resb 1
d_bare      resb 1
d_lower     resb 1
d_attr      resb 1
d_aset      resb 1
d_aclr      resb 1
d_sort      resb 1
d_nkeys     resb 1
d_keys      resb 8
d_opts_end:
d_inenv     resb 1
d_lines     resb 1
d_col       resb 1
d_lvl       resb 1
d_tok       resw 1
d_sname     resw 1
d_sattr     resw 1
d_seg       resw 1
d_n         resw 1
d_tfiles    resd 1
d_tbytes    resd 1
d_arg       resb 128
d_env       resb 256
d_spec      resb 80
d_path      resb 264
d_srch      resb 352
d_dtas      resb DIR_LVLS*DIR_DTA
iobuf       resb 512
hasext      resb 1
haspath     resb 1
pathptr     resw 1
prefix      resb 80
progpath    resb 160
tail        resb 130
fcb1        resb 37
fcb2        resb 37
pblock      resb 14
exec_sp     resw 1
exec_ax     resw 1
exec_cf     resb 1
            resb 1
mm_prev     resd 1
pl_in       resw 1
pl_out      resw 1
pl_depth    resb 1
pipe_tab    resb PIPE_MAX*PIPE_LEN
r_app       resb 1
r_in        resw 1
r_out       resw 1
sv_in       resw 1
sv_out      resw 1
rin_name    resb 80
rout_name   resb 80
t_buf       resb 132
t_buf2      resb 132
dta2        resb 128
c_src       resb 132
c_dst       resb 132
c_pre       resb 132
c_dpre      resb 132
c_spath     resb 160
c_dpath     resb 160
c_count     resd 1
c_dh        resw 1
c_open      resb 1
c_dstdir    resb 1
c_wild      resb 1
c_isdev     resb 1
c_eof       resb 1
c_nodst     resb 1
c_first     resb 1
c_last      resb 1
c_next      resw 1
c_cur       resb 1                  ; COPY: modo en curso (1 = /A, 2 = /B)
c_smode     resb 1                  ; modo de los orígenes
c_dmode     resb 1                  ; modo del destino
c_plus      resb 1                  ; se acaba de leer un «+»
c_ask       resb 1                  ; preguntar antes de sobrescribir
c_asc       resb 1                  ; leer los orígenes hasta Ctrl+Z
c_dwild     resb 1
env_size    resw 1                  ; bytes del entorno (/E:nnnn)
dt_tmp      resw 1                  ; DATE / TIME
dl_p        resb 1                  ; DEL /P                  ; el destino lleva comodines
ynbuf       resb 8
mm_mode     resb 1
mm_page     resb 1
mm_lines    resb 1
mm_found    resb 1
mm_n        resw 1
mm_sz       resd 1
mm_name     resb 8
mm_nbuf     resb 8
mm_tab      resb MM_MAXN*12
dt_yes      resb 1
dt_any      resb 1
dt_depth    resb 1
            resb 1
dt_args     resw 1
dt_end      resw 1
dt_dta0     resb 44
dt_dir      resb 132
dt_path     resb 264
m_mask      resb 12
m_name      resb 12
m_new       resb 14
cbuf        resb CBUF_LEN
echo_on     resb 1
primary     resb 1
al_tried    resb 1
lh_flag     resb 1
mm_hasumb   resb 1
mm_up       resb 1
mm_upseg    resw 1
lh_link     resb 1
mu_tot      resd 1
mu_free     resd 1
mu_large    resd 1
alias_buf   resb ALIAS_MAX
no_auto     resb 1
exit_after  resb 1
errlvl      resb 1
bdepth      resb 1
call_flag   resb 1
if_not      resb 1
es_name     resw 1
es_val      resw 1
br_pos      resd 1
sname       resb 64
pbuf        resb 258
vname       resb 64
vval        resb 258
glabel      resb 132
if_a        resb 132
if_b        resb 132
for_state:                          ; lo que CALL guarda dentro de un FOR
for_var     resb 1
in_for      resb 1
for_pos     resw 1
for_set     resb BLINE+2
for_tmpl    resb BLINE+2
for_pre     resb BLINE+2
for_dta     resb 44
FOR_STLEN   equ $ - for_state
for_cmd     resb 258
for_item    resb BLINE+2
for_full    resb BLINE+16
rbuf        resb BLINE+2
bline       resb BLINE+2
eline       resb BLINE+2
bctx        resb BC_LEN*BMAX
            resb 6144
stack_top:
prog_end:
