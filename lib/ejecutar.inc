; =============================================================================
;  Hispa-DOS - buscar y ejecutar un programa desde una utilidad (.COM)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lo usan VELOCIDAD y LOADFIX («orden programa argumentos»). El programa se
;  busca como lo hace el interprete: tal cual si lleva ruta; si no, en la
;  carpeta actual y despues en cada carpeta de PATH; sin extension se prueban
;  .COM, .EXE y .BAT (los .BAT se pasan a %COMSPEC% /C).
;
;  El programa que lo incluye pone EJ_BSS en su zona de datos sin iniciar y,
;  antes de ej_lanzar, encoge su bloque de memoria (INT 21h 4Ah).
;  (Este archivo no pasa por tools/cp437.py: solo ASCII.)
; =============================================================================

%macro EJ_BSS 0
ej_nombre   resb 80             ; lo que se escribio (en mayusculas)
ej_ruta     resb 160            ; ruta completa del programa encontrado
ej_comspec  resb 80
ej_cola     resb 130
ej_fcb1     resb 40
ej_fcb2     resb 40
ej_pblk     resb 14
ej_ss       resw 1
ej_sp       resw 1
ej_args     resw 1
ej_conruta  resb 1
ej_conext   resb 1
ej_tipo     resb 1              ; 0 = .COM/.EXE, 1 = .BAT
%endmacro

; ---- ej_buscar: DS:SI -> nombre del programa (hasta espacio, tab, CR o 0)
;      Sale: ej_ruta = ruta completa, ej_tipo, SI -> lo que sigue (argumentos).
;      CF si no se encuentra.
ej_buscar:
        push ax
        push bx
        push cx
        push di
        push es
        push ds
        pop es
        mov di, ej_nombre
        mov byte [ej_conruta], 0
        mov byte [ej_conext], 0
        xor cx, cx
.c:
        mov al, [si]
        cmp al, ' '
        jbe .ce
        cmp cx, 78
        jae .nf
        cmp al, 'a'
        jb .up
        cmp al, 'z'
        ja .up
        sub al, 32
.up:
        cmp al, '\'
        je .sep
        cmp al, '/'
        je .sep
        cmp al, ':'
        jne .nsep
.sep:
        mov byte [ej_conruta], 1
        mov byte [ej_conext], 0
        jmp .put
.nsep:
        cmp al, '.'
        jne .put
        mov byte [ej_conext], 1
.put:
        stosb
        inc si
        inc cx
        jmp .c
.ce:
        mov byte [di], 0
        mov [ej_args], si
        jcxz .nf
        ; 1) tal cual (carpeta actual o la ruta dada)
        mov byte [ej_ruta], 0
        call ej_probar
        jnc .ok
        cmp byte [ej_conruta], 0
        jne .nf
        ; 2) cada carpeta de PATH
        mov bx, ej_s_path
        call ej_getenv
        jc .nf
.dir:
        ; ES:DI -> resto de PATH
        cmp byte [es:di], 0
        je .nf
        mov bx, ej_ruta
.dc:
        mov al, [es:di]
        test al, al
        jz .de
        inc di
        cmp al, ';'
        je .de
        cmp bx, ej_ruta + 70
        jae .dc
        mov [bx], al
        inc bx
        jmp .dc
.de:
        cmp bx, ej_ruta
        je .dir
        cmp byte [bx-1], '\'
        je .dz
        mov byte [bx], '\'
        inc bx
.dz:
        mov byte [bx], 0
        push es
        push di
        push ds
        pop es
        call ej_probar
        pop di
        pop es
        jc .dir
.ok:
        mov si, [ej_args]
        clc
        jmp .out
.nf:
        mov si, [ej_args]
        stc
.out:
        pop es
        pop di
        pop cx
        pop bx
        pop ax
        ret

; ej_ruta = carpeta (con su barra, o vacia): se le pega el nombre y se prueba
; (con las tres extensiones si no llevaba). CF si no existe. ES = DS.
ej_probar:
        push si
        push di
        push bx
        mov di, ej_ruta
.e:
        cmp byte [di], 0
        je .cp
        inc di
        jmp .e
.cp:
        mov si, ej_nombre
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        dec di                      ; DI -> el 0 final
        cmp byte [ej_conext], 0
        je .exts
        ; con extension: el tipo sale de ella
        mov byte [ej_tipo], 0
        cmp di, ej_ruta + 4
        jb .t1
        cmp word [di-4], '.B'
        jne .t1
        cmp word [di-2], 'AT'
        jne .t1
        mov byte [ej_tipo], 1
.t1:
        call ej_existe
        jmp .out
.exts:
        mov bx, ej_s_exts
        mov byte [ej_tipo], 0
.x:
        mov ax, [bx]
        mov [di], ax
        mov ax, [bx+2]
        mov [di+2], ax
        mov byte [di+4], 0
        call ej_existe
        jnc .out
        add bx, 4
        cmp bx, ej_s_exts + 8
        jb .x
        ja .no
        mov byte [ej_tipo], 1       ; la tercera: .BAT
        jmp .x
.no:
        stc
.out:
        pop bx
        pop di
        pop si
        ret

; ?existe el archivo ej_ruta (y no es una carpeta)? CF si no
ej_existe:
        push ax
        push cx
        push dx
        mov dx, ej_ruta
        mov ax, 0x4300
        int 0x21
        jc .r
        test cl, 0x10
        jz .r
        stc
.r:
        pop dx
        pop cx
        pop ax
        ret

; DS:BX -> "NOMBRE=" (ASCIIZ). Sale ES:DI -> el valor en el entorno; CF si
; no esta.
ej_getenv:
        push ax
        push si
        mov es, [0x2C]
        mov ax, es
        test ax, ax
        jz .no
        xor di, di
.var:
        cmp byte [es:di], 0
        je .no
        mov si, bx
.cmp:
        mov al, [si]
        test al, al
        jz .si
        cmp al, [es:di]
        jne .skip
        inc si
        inc di
        jmp .cmp
.skip:
        cmp byte [es:di], 0
        je .nx
        inc di
        jmp .skip
.nx:
        inc di
        jmp .var
.si:
        clc
        jmp .out
.no:
        stc
.out:
        pop si
        pop ax
        ret

; ---- ej_lanzar: ejecuta ej_ruta con los argumentos DS:SI (como los dejo
;      ej_buscar: desde el separador hasta CR o 0) y espera a que acabe.
;      Sale: CF=0 y AX = codigo de retorno del programa (AH = como acabo),
;            o CF=1 y AX = error de DOS al cargarlo.
ej_lanzar:
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push es
        push ds
        pop es
        mov di, ej_cola + 1
        xor cx, cx
        cmp byte [ej_tipo], 1
        jne .args
        ; .BAT: %COMSPEC% /C ruta argumentos
        push si
        mov bx, ej_s_comspec
        call ej_getenv
        jnc .hay
        pop si
        push ds
        pop es
        mov ax, 2
        stc
        jmp .out
.hay:
        mov si, ej_comspec
.cs:
        mov al, [es:di]
        mov [si], al
        inc si
        inc di
        test al, al
        jz .cse
        cmp si, ej_comspec + 79
        jb .cs
        mov byte [si], 0
.cse:
        push ds
        pop es
        mov di, ej_cola + 1
        mov al, ' '
        stosb
        mov ax, '/C'
        stosw
        mov al, ' '
        stosb
        mov cx, 4
        mov si, ej_ruta
.br:
        lodsb
        test al, al
        jz .bre
        cmp cx, 126
        jae .br
        stosb
        inc cx
        jmp .br
.bre:
        pop si
.args:
        mov bx, si                  ; para los FCB
.a:
        lodsb
        cmp al, 13
        je .ae
        test al, al
        jz .ae
        cmp cx, 126
        jae .a
        stosb
        inc cx
        jmp .a
.ae:
        mov [ej_cola], cl
        mov byte [di], 13
        mov si, bx
        mov di, ej_fcb1
        mov ax, 0x2901
        int 0x21
        mov di, ej_fcb2
        mov ax, 0x2901
        int 0x21
        mov bx, ej_pblk
        mov word [bx], 0            ; el entorno: copia del nuestro
        mov word [bx+2], ej_cola
        mov [bx+4], ds
        mov word [bx+6], ej_fcb1
        mov [bx+8], ds
        mov word [bx+10], ej_fcb2
        mov [bx+12], ds
        mov dx, ej_ruta
        cmp byte [ej_tipo], 1
        jne .go
        mov dx, ej_comspec
.go:
        mov [cs:ej_ss], ss
        mov [cs:ej_sp], sp
        mov ax, 0x4B00
        int 0x21
        cli
        mov ss, [cs:ej_ss]
        mov sp, [cs:ej_sp]
        sti
        push cs
        pop ds
        push cs
        pop es
        cld
        jc .out
        mov ah, 0x4D
        int 0x21
        clc
.out:
        pop es
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        ret

ej_s_path       db "PATH=", 0
ej_s_comspec    db "COMSPEC=", 0
ej_s_exts       db ".COM", ".EXE", ".BAT"
