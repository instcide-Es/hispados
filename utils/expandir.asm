; =============================================================================
;  Hispa-DOS EXPANDIR.COM (EXPAND) - descomprimir archivos de los disquetes
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  EXPANDIR origen [destino]
;     origen   archivo comprimido (admite comodines: *.EX_)
;     destino  archivo o carpeta; sin destino, junto al origen con su nombre
;              original (el que guarda el propio archivo comprimido)
;
;  Formatos: SZDD (LZSS, disquetes de Windows 3.0/3.1) y KWAJ (métodos 0
;  sin comprimir, 1 XOR, 2 LZSS y 3 LZ + Huffman, como MS-DOS 6 y Windows
;  para Trabajo en Grupo). Escrito a partir de la descripción pública de los
;  formatos.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

INSZ    equ 8192
OUTSZ   equ 8192
WSIZE   equ 4096

; árboles de Huffman del método 3: número de símbolos y desplazamiento de su
; tabla de longitudes
T_MLEN  equ 0
T_MLEN2 equ 1
T_LLEN  equ 2
T_OFF   equ 3
T_LIT   equ 4

start:
        mov sp, stack_top
        cld
        mov di, bss_start
        mov cx, stack_top - bss_start
        xor al, al
        rep stosb
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; argumentos
        mov si, 0x81
        mov di, src
        call next_arg
        jc usage
        mov di, dst
        call next_arg
        mov di, extra
        call next_arg
        jnc usage
        ; ¿destino carpeta?
        cmp byte [dst], 0
        je .nodst
        mov dx, dst
        mov ax, 0x4300
        int 0x21
        jc .root
        test cl, 0x10
        jz .nodst
.isdir:
        mov byte [dst_isdir], 1
        jmp .nodst
.root:
        ; «B:\» y «B:» son carpetas aunque no se puedan consultar sus atributos
        mov si, dst
.re:
        lodsb
        test al, al
        jnz .re
        mov al, [si-2]
        cmp al, '\'
        je .isdir
        cmp al, ':'
        je .isdir
.nodst:
        ; varios archivos (comodines) a un solo nombre: se pisarían unos a otros
        cmp byte [dst], 0
        je .wok
        cmp byte [dst_isdir], 0
        jne .wok
        mov si, src
.wc:
        lodsb
        test al, al
        jz .wok
        cmp al, '*'
        je .wbad
        cmp al, '?'
        jne .wc
.wbad:
        mov si, msg_multi
        call puts
        mov ax, 0x4C01
        int 0x21
.wok:
        mov si, src
        mov di, sdir
        call dir_prefix
        mov dx, dta
        mov ah, 0x1A
        int 0x21
        mov dx, src
        mov cx, 0x27
        mov ah, 0x4E
        int 0x21
        jnc .f
        mov si, msg_nf
        call puts
        mov ax, 0x4C01
        int 0x21
.f:
        inc word [count]
        call one
        mov ah, 0x4F
        int 0x21
        jnc .f
        mov al, [errors]
        mov ah, 0x4C
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21

; descomprimir el archivo de la DTA
one:
        mov si, sdir
        mov di, fsrc
        call strcpy
        mov si, dta+0x1E
        call strcpy
        mov dx, fsrc
        mov ax, 0x3D00
        int 0x21
        jnc .op
        jmp fail_src
.op:
        mov [hin], ax
        mov word [ipos], 0
        mov word [ilen], 0
        mov byte [ieof], 0
        mov byte [haslen], 0        ; el tamaño del archivo anterior no vale
        mov byte [plain], 0
        ; cabecera
        mov di, hdr
        mov cx, 14
.h:
        call get_byte
        jc .notc
        stosb
        loop .h
        cmp dword [hdr], 'SZDD'
        je szdd
        cmp dword [hdr], 'KWAJ'
        je kwaj
.notc:
        ; no está comprimido: se copia tal cual (como EXPAND de MS-DOS)
        mov byte [plain], 1
        mov byte [kname], 0
        call name_kwaj              ; destino con el nombre del origen
        call open_out               ; (si es el propio origen, aviso)
        jc close_in
        mov bx, [hin]
        xor cx, cx
        xor dx, dx
        mov ax, 0x4200
        int 0x21
        mov word [ipos], 0
        mov word [ilen], 0
        mov byte [ieof], 0
        mov byte [xorv], 0
        jmp kwaj.copy

; ---- SZDD: 'SZDD' 88 F0 27 33, 'A', letra que falta, tamaño (4) ----
szdd:
        mov al, [hdr+9]
        mov [miss], al
        mov eax, [hdr+10]
        mov [outlen], eax
        mov byte [haslen], 1
        call name_from_src          ; destino con la letra que falta
        call open_out
        jc close_in
        mov word [wpos], WSIZE - 16
        call lzss
        jmp finish

; ---- KWAJ ----
kwaj:
        mov ax, [hdr+8]
        mov [method], ax
        ; resto de la cabecera hasta el desplazamiento de los datos
        movzx ecx, word [hdr+10]
        mov [dataoff], cx
        mov di, hdr+14
        sub cx, 14
        jb .bad
        cmp cx, 256
        ja .bad
        jcxz .hd
.hr:
        call get_byte
        jc .bad
        stosb
        loop .hr
.hd:
        ; campos opcionales según las banderas
        mov si, hdr+14
        mov ax, [hdr+12]
        mov [flags], ax
        test al, 1
        jz .f1
        mov eax, [si]
        mov [outlen], eax
        mov byte [haslen], 1
        add si, 4
.f1:
        test byte [flags], 2
        jz .f2
        add si, 2
.f2:
        test byte [flags], 4
        jz .f3
        mov ax, [si]
        add si, 2
        add si, ax
.f3:
        mov ax, si                  ; los campos no pueden salirse de la cabecera
        sub ax, hdr
        cmp ax, [dataoff]
        ja .bad
        ; nombre y extensión originales
        mov byte [kname], 0
        test byte [flags], 8
        jz .f4
        mov di, kname
        mov cx, 9
.n:
        lodsb
        test al, al
        jz .ne
        stosb
        loop .n
.ne:
        mov byte [di], 0
.f4:
        test byte [flags], 0x10
        jz .f5
        mov di, kname
.fe:
        cmp byte [di], 0
        je .fx
        inc di
        jmp .fe
.fx:
        mov byte [di], '.'
        inc di
        mov cx, 4
.x:
        lodsb
        test al, al
        jz .xe
        stosb
        loop .x
.xe:
        mov byte [di], 0
.f5:
        call name_kwaj
        call open_out
        jc close_in
        mov ax, [method]
        cmp ax, 0
        je .m0
        cmp ax, 1
        je .m1
        cmp ax, 2
        je .m2
        cmp ax, 3
        je .m3
        mov si, msg_method
        call say_name
        jmp abort_out
.m0:
        mov byte [xorv], 0
        jmp .copy
.m1:
        mov byte [xorv], 0xFF
.copy:
        call get_byte
        jc finish
        xor al, [xorv]
        call put_byte
        jc finish
        jmp .copy
.m2:
        mov word [wpos], WSIZE - 18
        call lzss
        jmp finish
.m3:
        call lzh
        jmp finish
.bad:
        mov si, msg_bad
        call say_name
        jmp close_in

; ---- terminar: vaciar, fecha del original y cerrar ----
finish:
        call flush_out
        ; salida más corta que el tamaño de la cabecera: entrada cortada
        cmp byte [haslen], 0
        je .len
        mov eax, [outcnt]
        cmp eax, [outlen]
        je .len
        or byte [derr], 1
.len:
        mov bx, [hin]
        mov ax, 0x5700
        int 0x21
        mov bx, [hout]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        jnc .cl
        or byte [derr], 2           ; no se ha podido cerrar (escribir) bien
.cl:
        cmp byte [derr], 0
        jne .bad
        mov si, fsrc
        call puts
        mov si, msg_arrow
        call puts
        mov si, fdst
        call puts
        call crlf
        jmp close_in
.bad:
        mov si, msg_bad
        test byte [derr], 2
        jz .say
        mov si, msg_full            ; error al escribir: disco lleno
.say:
        call say_name
        mov dx, fdst
        mov ah, 0x41
        int 0x21
        jmp close_in
abort_out:
        mov bx, [hout]
        mov ah, 0x3E
        int 0x21
        mov dx, fdst
        mov ah, 0x41
        int 0x21
close_in:
        mov bx, [hin]
        mov ah, 0x3E
        int 0x21
        ret
fail_src:
        mov byte [errors], 1
        mov si, msg_nf
        jmp puts

; mensaje SI seguido del nombre del origen
say_name:
        mov byte [errors], 1
        push si
        mov si, fsrc
        call puts
        mov al, ':'
        call putc
        mov al, ' '
        call putc
        pop si
        jmp puts

; ---- nombres de destino ----
; SZDD: el del origen con la letra que falta en lugar de '_'
name_from_src:
        mov si, dta+0x1E
        mov di, oname
        call strcpy
        cmp byte [di-1], '_'
        jne .r
        mov al, [miss]
        test al, al
        jz .r
        mov [di-1], al
.r:
        jmp build_dst
; KWAJ: el guardado en la cabecera; si no hay, el del origen
name_kwaj:
        cmp byte [kname], 0
        je .src
        mov si, kname
        mov di, oname
        call strcpy
        jmp build_dst
.src:
        mov si, dta+0x1E
        mov di, oname
        call strcpy
        jmp build_dst
; fdst = destino dado, o carpeta + oname
build_dst:
        mov di, fdst
        cmp byte [dst], 0
        jne .given
        mov si, sdir
        call strcpy
        jmp .name
.given:
        mov si, dst
        call strcpy
        cmp byte [dst_isdir], 0
        je .r
        cmp byte [di-1], '\'
        je .name
        cmp byte [di-1], ':'
        je .name
        mov byte [di], '\'
        inc di
.name:
        mov si, oname
        call strcpy
.r:
        ret

open_out:
        ; el destino no puede ser el propio origen (se perdería); se comparan
        ; las rutas completas (.\X, X y A:\X son el mismo archivo)
        mov si, fsrc
        mov di, tn1
        call truename
        mov si, fdst
        mov di, tn2
        call truename
        mov si, tn1
        mov di, tn2
.cmp:
        mov al, [si]
        call upcase
        mov ah, al
        mov al, [di]
        call upcase
        cmp al, ah
        jne .diff
        test al, al
        jz .same
        inc si
        inc di
        jmp .cmp
.same:
        mov si, msg_needdst
        cmp byte [plain], 0
        je .ss
        mov si, msg_notc
.ss:
        call say_name
        stc
        ret
.diff:
        mov dx, fdst
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .e
        mov [hout], ax
        mov word [opos], 0
        mov dword [outcnt], 0
        mov byte [derr], 0
        clc
        ret
.e:
        mov si, msg_create
        call say_name
        stc
        ret

; SI = ruta -> DI = ruta completa (o la misma, si el núcleo no la resuelve)
truename:
        push si
        push di
        mov ah, 0x60
        int 0x21
        pop di
        pop si
        jnc .r
        call strcpy
.r:
        ret

; ---- entrada y salida con búfer ----
; -> AL; CF al final
get_byte:
        push bx
        mov bx, [ipos]
        cmp bx, [ilen]
        jb .have
        cmp byte [ieof], 0
        jne .eof
        push cx
        push dx
        mov bx, [hin]
        mov dx, ibuf
        mov cx, INSZ
        mov ah, 0x3F
        int 0x21
        pop dx
        pop cx
        jc .rerr
        test ax, ax
        jz .seteof
        mov [ilen], ax
        xor bx, bx
.have:
        mov al, [ibuf+bx]
        inc bx
        mov [ipos], bx
        pop bx
        clc
        ret
.rerr:
        or byte [derr], 1           ; error de lectura: no es un final normal
.seteof:
        mov byte [ieof], 1
.eof:
        pop bx
        stc
        ret
; AL -> salida; CF cuando ya se ha escrito todo (si se sabe el tamaño)
put_byte:
        test byte [derr], 2         ; ya no se puede escribir: terminar
        jnz .stop
        cmp byte [haslen], 0
        je .w
        push eax
        mov eax, [outcnt]
        cmp eax, [outlen]
        pop eax
        jb .w
.stop:
        stc
        ret
.w:
        push bx
        mov bx, [opos]
        mov [obuf+bx], al
        inc bx
        mov [opos], bx
        inc dword [outcnt]
        cmp bx, OUTSZ
        jb .r
        call flush_out
.r:
        pop bx
        clc
        ret
flush_out:
        pusha
        mov cx, [opos]
        jcxz .r
        mov bx, [hout]
        mov dx, obuf
        mov ah, 0x40
        int 0x21
        jc .e
        cmp ax, cx
        je .ok
.e:
        or byte [derr], 2
.ok:
        mov word [opos], 0
.r:
        popa
        ret

; ---- LZSS (SZDD y KWAJ 2): ventana de 4096 llena de espacios ----
lzss:
        call win_clear
.ctl:
        call get_byte
        jc .r
        mov [ctl], al
        mov byte [cbits], 8
.b:
        dec byte [cbits]
        js .ctl
        shr byte [ctl], 1
        jnc .pair
        ; literal
        call get_byte
        jc .r
        call win_put
        jc .r
        jmp .b
.pair:
        call get_byte
        jc .r
        mov dl, al
        call get_byte
        jc .r
        mov dh, al
        movzx bx, dl
        mov al, dh
        and ax, 0xF0
        shl ax, 4
        or bx, ax                   ; posición en la ventana
        mov cl, dh
        and cx, 0x0F
        add cx, 3                   ; longitud
.cp:
        and bx, WSIZE - 1
        mov al, [win+bx]
        call win_put
        jc .r
        inc bx
        loop .cp
        jmp .b
.r:
        ret
win_clear:
        push di
        push cx
        push es
        push ds
        pop es
        mov di, win
        mov cx, WSIZE
        mov al, ' '
        rep stosb
        pop es
        pop cx
        pop di
        ret
; AL -> salida y ventana; CF al terminar
win_put:
        push bx
        mov bx, [wpos]
        mov [win+bx], al
        inc bx
        and bx, WSIZE - 1
        mov [wpos], bx
        pop bx
        jmp put_byte

; ---- KWAJ método 3: LZ + Huffman ----
; lectura de bits, el más significativo primero
; CL = número de bits (1-16) -> AX; CF si se acaba la entrada
get_bits:
        push bx
        push dx
        xor dx, dx
.l:
        test cl, cl
        jz .d
        cmp byte [bleft], 0
        jne .h
        call get_byte
        jc .eof
        mov [bbuf], al
        mov byte [bleft], 8
.h:
        shl byte [bbuf], 1
        rcl dx, 1
        dec byte [bleft]
        dec cl
        jmp .l
.d:
        mov ax, dx
        pop dx
        pop bx
        clc
        ret
.eof:
        mov byte [ieof2], 1
        ; al final se completa con ceros
        shl dx, cl
        mov ax, dx
        pop dx
        pop bx
        clc
        ret

lzh:
        call win_clear
        mov word [wpos], WSIZE - 17
        mov byte [bleft], 0
        mov byte [ieof2], 0
        mov byte [litrun], 0
        ; seis tipos de codificación (4 bits cada uno; se usan cinco)
        xor si, si
.ty:
        mov cl, 4
        call get_bits
        mov [types+si], al
        inc si
        cmp si, 6
        jb .ty
        ; las cinco tablas
        mov bl, T_MLEN
        call build_tree
        jc .bad
        mov bl, T_MLEN2
        call build_tree
        jc .bad
        mov bl, T_LLEN
        call build_tree
        jc .bad
        mov bl, T_OFF
        call build_tree
        jc .bad
        mov bl, T_LIT
        call build_tree
        jc .bad
.loop:
        cmp byte [ieof2], 0
        jne .r
        mov bl, T_MLEN
        cmp byte [litrun], 0
        je .m
        mov bl, T_MLEN2
.m:
        call get_sym
        jc .bad
        test ax, ax
        jz .lits
        ; coincidencia: longitud + 2, distancia = símbolo << 6 | 6 bits
        add ax, 2
        mov [mlen], ax
        mov byte [litrun], 0
        mov bl, T_OFF
        call get_sym
        jc .bad
        shl ax, 6
        mov [moff], ax
        mov cl, 6
        call get_bits
        or [moff], ax
.mc:
        mov bx, [wpos]
        add bx, WSIZE
        sub bx, [moff]
        and bx, WSIZE - 1
        mov al, [win+bx]
        call win_put
        jc .r
        dec word [mlen]
        jnz .mc
        jmp .loop
.lits:
        ; tanda de literales: longitud + 1; si es 32, sigue otra tanda
        mov bl, T_LLEN
        call get_sym
        jc .bad
        inc ax
        mov [mlen], ax
        mov byte [litrun], 1
        cmp ax, 32
        jne .lc
        mov byte [litrun], 0
.lc:
        mov bl, T_LIT
        call get_sym
        jc .bad
        call win_put
        jc .r
        dec word [mlen]
        jnz .lc
        jmp .loop
.bad:
        or byte [derr], 1
.r:
        ret

; número de símbolos y tablas del árbol BL
tree_nsyms  dw 16, 16, 32, 64, 256
tree_lens   dw lens_ml, lens_ml2, lens_ll, lens_off, lens_lit
tree_syms   dw syms_ml, syms_ml2, syms_ll, syms_off, syms_lit
tree_count  dw cnt_ml, cnt_ml2, cnt_ll, cnt_off, cnt_lit

; leer las longitudes del árbol BL según su tipo y preparar la decodificación
; canónica (los códigos más cortos primero; a igual longitud, el símbolo menor)
build_tree:
        movzx bx, bl
        mov al, [types+bx]
        mov [ttype], al
        shl bx, 1
        mov cx, [tree_nsyms+bx]
        mov [nsyms], cx
        mov di, [tree_lens+bx]
        mov [tl], di
        mov ax, [tree_syms+bx]
        mov [ts], ax
        mov ax, [tree_count+bx]
        mov [tc], ax
        ; longitudes
        mov al, [ttype]
        cmp al, 0
        je .t0
        cmp al, 1
        je .t1
        cmp al, 2
        je .t2
        cmp al, 3
        je .t3
        stc
        ret
.t0:
        ; todas iguales: 4, 5, 6 u 8 bits según el número de símbolos
        mov al, 4
        cmp cx, 16
        je .t0f
        mov al, 5
        cmp cx, 32
        je .t0f
        mov al, 6
        cmp cx, 64
        je .t0f
        mov al, 8
.t0f:
        rep stosb
        jmp .canon
.t1:
        push cx
        mov cl, 4
        call get_bits
        pop cx
        mov dl, al                  ; c
        stosb
        dec cx
        jz .canon
.t1l:
        push cx
        mov cl, 1
        call get_bits
        test al, al
        jz .t1same
        mov cl, 1
        call get_bits
        test al, al
        jz .t1inc
        mov cl, 4
        call get_bits
        mov dl, al
        jmp .t1put
.t1inc:
        inc dl
        jmp .t1put
.t1same:
.t1put:
        pop cx
        mov al, dl
        stosb
        loop .t1l
        jmp .canon
.t2:
        push cx
        mov cl, 4
        call get_bits
        pop cx
        mov dl, al
        stosb
        dec cx
        jz .canon
.t2l:
        push cx
        mov cl, 2
        call get_bits
        cmp al, 3
        jne .t2d
        mov cl, 4
        call get_bits
        mov dl, al
        jmp .t2p
.t2d:
        add dl, al
        dec dl                      ; c += sel - 1
.t2p:
        pop cx
        mov al, dl
        stosb
        loop .t2l
        jmp .canon
.t3:
.t3l:
        push cx
        mov cl, 4
        call get_bits
        pop cx
        stosb
        loop .t3l
.canon:
        ; cuántos códigos de cada longitud (1-15) y los símbolos en orden
        mov di, [tc]
        push di
        mov cx, 16
        xor ax, ax
.cz:
        mov [di], ax
        add di, 2
        loop .cz
        pop di
        mov si, [tl]
        mov cx, [nsyms]
.cc:
        lodsb
        and al, 15
        jz .cn
        movzx bx, al
        shl bx, 1
        inc word [di+bx]
.cn:
        loop .cc
        ; símbolos ordenados por longitud y, dentro, por número
        mov di, [ts]
        mov dl, 1
.sl:
        mov si, [tl]
        xor bx, bx
.ss:
        cmp bx, [nsyms]
        jae .sn
        mov al, [si+bx]
        and al, 15
        cmp al, dl
        jne .sx
        mov [di], bl
        inc di
.sx:
        inc bx
        jmp .ss
.sn:
        inc dl
        cmp dl, 16
        jb .sl
        clc
        ret

; leer un símbolo del árbol BL -> AX; CF si el código no existe
get_sym:
        movzx bx, bl
        shl bx, 1
        mov si, [tree_count+bx]
        mov di, [tree_syms+bx]
        xor dx, dx                  ; código leído
        xor bp, bp                  ; primer código de esta longitud
        xor ax, ax                  ; índice del primer símbolo de esta longitud
        mov bx, 2                   ; longitud 1 (en palabras)
.l:
        push ax
        mov cl, 1
        call get_bits
        shl dx, 1
        or dx, ax
        pop ax
        ; ¿el código cae en esta longitud?
        mov cx, dx
        sub cx, bp
        cmp cx, [si+bx]
        jb .found
        ; siguiente longitud
        add ax, [si+bx]
        add bp, [si+bx]
        shl bp, 1
        add bx, 2
        cmp bx, 32
        jb .l
        stc
        ret
.found:
        add ax, cx
        add di, ax
        movzx ax, byte [di]
        clc
        ret

; ---- utilidades ----
next_arg:
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 9
        je .sk
        cmp al, 13
        je .none
        cmp al, '/'
        je .opt
.w:
        call upcase
        stosb
        lodsb
        cmp al, ' '
        ja .w
        dec si
        mov byte [di], 0
        clc
        ret
.opt:
        jmp usage
.none:
        dec si
        mov byte [di], 0
        stc
        ret
dir_prefix:
        mov bx, di
.l:
        lodsb
        mov [di], al
        inc di
        test al, al
        jz .e
        cmp al, '\'
        je .s
        cmp al, ':'
        jne .l
.s:
        mov bx, di
        jmp .l
.e:
        mov byte [bx], 0
        ret
strcpy:
        lodsb
        mov [di], al
        test al, al
        jz .e
        inc di
        jmp strcpy
.e:
        ret

%include "util.inc"

msg_usage   db "Uso: EXPANDIR origen [destino]", 13, 10
            db "Descomprime archivos de los disquetes (formatos SZDD y KWAJ).", 13, 10
            db "  origen   puede llevar comodines (*.EX_); destino, una carpeta", 13, 10, 0
msg_nf      db "No se encontró el archivo.", 13, 10, 0
msg_arrow   db " => ", 0
msg_notc    db "no está comprimido.", 13, 10, 0
msg_method  db "método de compresión desconocido.", 13, 10, 0
msg_bad     db "los datos comprimidos están dañados.", 13, 10, 0
msg_needdst db "no guarda su nombre original: indique el destino (EXPANDIR origen destino).", 13, 10, 0
msg_full    db "no cabe en el disco de destino.", 13, 10, 0
msg_multi   db "Con varios archivos de origen el destino tiene que ser una carpeta.", 13, 10, 0
msg_create  db "no se puede crear el archivo de destino.", 13, 10, 0

        section .bss
bss_start:
dst_isdir   resb 1
errors      resb 1
haslen      resb 1
ieof        resb 1
ieof2       resb 1
derr        resb 1
miss        resb 1
plain       resb 1
xorv        resb 1
ctl         resb 1
cbits       resb 1
bbuf        resb 1
bleft       resb 1
litrun      resb 1
ttype       resb 1
types       resb 6
count       resw 1
hin         resw 1
hout        resw 1
ipos        resw 1
ilen        resw 1
opos        resw 1
wpos        resw 1
method      resw 1
dataoff     resw 1
flags       resw 1
mlen        resw 1
moff        resw 1
nsyms       resw 1
tl          resw 1
ts          resw 1
tc          resw 1
outlen      resd 1
outcnt      resd 1
src         resb 128
dst         resb 128
extra       resb 128
sdir        resb 128
fsrc        resb 160
fdst        resb 160
tn1         resb 160
tn2         resb 160
oname       resb 16
kname       resb 16
dta         resb 64
hdr         resb 280
lens_ml     resb 16
lens_ml2    resb 16
lens_ll     resb 32
lens_off    resb 64
lens_lit    resb 256
syms_ml     resb 16
syms_ml2    resb 16
syms_ll     resb 32
syms_off    resb 64
syms_lit    resb 256
cnt_ml      resw 16
cnt_ml2     resw 16
cnt_ll      resw 16
cnt_off     resw 16
cnt_lit     resw 16
win         resb WSIZE
u_numbuf    resb 16
u_lbuf      resb 84
ibuf        resb INSZ
obuf        resb OUTSZ
            resb 1024
stack_top:
prog_end:
