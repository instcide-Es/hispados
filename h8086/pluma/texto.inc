; =============================================================================
;  PLUMA - el texto: en la memoria, con un hueco donde está el cursor, y lo
;  que no cabe, en tres archivos de trozos en el disco
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El documento, de principio a fin, es:
;
;     A (archivo)  |  memoria: «antes» [hueco] «después»  |  B (pila)  |  C
;
;  - en la memoria (de BUF a [bfin]) está el trozo en el que se trabaja: lo de
;    antes del cursor al principio, lo de después pegado al final, y el hueco
;    entre los dos. Escribir una letra es ponerla en el hueco: no se mueve nada.
;  - A guarda, en orden, lo que hay antes de la memoria (trozos de TROZO octetos).
;  - B es una pila de trozos de TROZO octetos con lo que hay después: el de
;    arriba es el que va justo detrás de la memoria.
;  - C es el resto del archivo que se abrió y que no cupo al cargarlo (ya
;    convertido), del que se va leyendo hacia delante.
;  Cuando el cursor se acerca a un borde de la memoria y al otro lado hay
;  disco, se trae un trozo (y si no hay sitio, se lleva otro al lado
;  contrario). Así un documento puede ser mucho mayor que la memoria y el
;  usuario no lo nota: es lo que hacía WordStar con sus archivos .$$$.
;
;  Las posiciones del documento son de 32 bits (DX:AX); las de la memoria, de
;  16: «w» va de 0 a [pre]+[post] y no cuenta el hueco.
; =============================================================================

; ---- de una posición de la memoria (w, en AX) a su dirección (AX) ----
dir_de:
        cmp ax, [pre]
        jb .a
        sub ax, [pre]
        add ax, [pstart]
        ret
.a:
        add ax, BUF
        ret
; y al revés: de una dirección (AX) a su w (AX)
w_de:
        cmp ax, [g_ini]
        ja .d
        jb .a
        mov ax, [pre]               ; (el principio del hueco es el cursor)
        ret
.a:
        sub ax, BUF
        ret
.d:
        sub ax, [pstart]
        add ax, [pre]
        ret

; [pstart] y [g_ini]: dónde empieza lo de después y dónde el hueco
pon_hueco:
        mov ax, [bfin]
        sub ax, [post]
        mov [pstart], ax
        mov [g_fin], ax
        mov ax, BUF
        add ax, [pre]
        mov [g_ini], ax
        ret

; ---- el cursor, como posición del documento: DX:AX ----
cursor32:
        mov ax, [pre]
        xor dx, dx
        add ax, [lenA]
        adc dx, [lenA+2]
        ret
; lo que mide el documento entero: DX:AX
total32:
        mov ax, [nB]
        call nB_pos                 ; (B: nB trozos de TROZO)
        add ax, [lenA]
        adc dx, [lenA+2]
        add ax, [pre]
        adc dx, 0
        add ax, [post]
        adc dx, 0
        add ax, [clen]
        adc dx, [clen+2]
        sub ax, [cpos]
        sbb dx, [cpos+2]
        ret

; ---- mover el hueco a la posición de la memoria AX ----
mueve_hueco:
        push si
        push di
        push cx
        cmp ax, [pre]
        je .x
        jb .atras
        mov cx, ax                  ; adelante: lo de después pasa a antes
        sub cx, [pre]
        mov si, [pstart]
        mov di, [g_ini]
        add [pre], cx
        sub [post], cx
        rep movsb
        jmp short .x
.atras:
        mov cx, [pre]               ; atrás: lo de antes pasa a después
        sub cx, ax
        mov si, [g_ini]
        dec si
        mov di, [pstart]
        dec di
        sub [pre], cx
        add [post], cx
        std
        rep movsb
        cld
.x:
        call pon_hueco
        pop cx
        pop di
        pop si
        ret

; ---- los archivos de trozos ----
; abre (si no lo está) el archivo temporal del octeto [BX] (hA, hB, hC o hP):
; BX apunta a su número; CF=1 si no se puede
abre_tmp:
        cmp word [bx], 0
        jne .ya
        push dx
        push cx
        mov ax, bx                  ; la letra: A, B, C o P
        sub ax, hA
        shr ax, 1
        mov si, .letras
        add si, ax
        mov al, [si]
        mov [n_tmp+7], al
        mov dx, n_tmp
        xor cx, cx
        DOS 0x3C
        pop cx
        pop dx
        jc .x
        mov [bx], ax
.ya:
        clc
.x:
        ret
.letras     db "ABCP"
; borra los archivos temporales (al salir y al empezar otro documento)
cierra_tmps:
        mov bx, hA
.c:
        cmp word [bx], 0
        je .s
        push bx
        mov bx, [bx]
        DOS 0x3E
        pop bx
        mov ax, bx
        sub ax, hA
        shr ax, 1
        mov si, abre_tmp.letras
        add si, ax
        mov al, [si]
        mov [n_tmp+7], al
        mov dx, n_tmp
        DOS 0x41
        mov word [bx], 0
.s:
        add bx, 2
        cmp bx, hA + 8
        jb .c
        ret

; lee o escribe CX octetos de DI en el archivo BX, en la posición DX:AX.
; CF=1 si falla (y lo dice)
en_disco_lee:
        mov byte [es_escr], 0
        jmp short en_disco
en_disco_escribe:
        mov byte [es_escr], 1
en_disco:
        push cx
        push bx
        mov cx, dx
        mov dx, ax
        mov ax, 0x4200
        int 0x21
        pop bx
        pop cx
        jc .mal
        mov dx, di
        mov ah, 0x3F
        cmp byte [es_escr], 0
        je .l
        mov ah, 0x40
.l:
        int 0x21
        jc .mal
        cmp ax, cx
        jne .mal
        clc
        ret
.mal:
        mov si, m_disco
        call aviso
        stc
        ret

; ---- de la memoria al disco y vuelta ----
; el primer trozo de lo de antes, al final de A. CF=1 si no se puede
lleva_a_A:
        cmp word [pre], TROZO + MARGEN
        jb .no
        mov bx, hA
        call abre_tmp
        jc .no
        mov bx, [hA]
        mov ax, [lenA]
        mov dx, [lenA+2]
        mov di, BUF
        mov cx, TROZO
        call en_disco_escribe
        jc .no
        add word [lenA], TROZO
        adc word [lenA+2], 0
        mov si, BUF + TROZO            ; lo demás de antes, al principio
        mov di, BUF
        mov cx, [pre]
        sub cx, TROZO
        mov [pre], cx
        rep movsb
        call pon_hueco
        mov ax, TROZO                  ; lo de la memoria se ha corrido
        jmp corre_w
.no:
        stc
        ret
; el último trozo de A, delante de la memoria (hace falta un hueco de TROZO)
trae_de_A:
        mov ax, [lenA]
        or ax, [lenA+2]
        jz .no
        call hueco_para_CH
        jc .no
        mov si, [g_ini]             ; lo de antes, TROZO más allá
        dec si
        mov di, si
        add di, TROZO
        mov cx, [pre]
        std
        rep movsb
        cld
        mov ax, [lenA]
        mov dx, [lenA+2]
        sub ax, TROZO
        sbb dx, 0
        mov bx, [hA]
        mov di, BUF
        mov cx, TROZO
        call en_disco_lee
        jc .no
        sub word [lenA], TROZO
        sbb word [lenA+2], 0
        add word [pre], TROZO
        call pon_hueco
        add word [top_w], TROZO
        mov byte [filas_ok], 0
        clc
        ret
.no:
        stc
        ret
; el último trozo de lo de después, arriba de la pila B. CF=1 si no se puede
lleva_a_B:
        cmp word [post], TROZO + MARGEN
        jb .no
        mov bx, hB
        call abre_tmp
        jc .no
        mov ax, [nB]
        call nB_pos
        mov bx, [hB]
        mov di, [bfin]
        sub di, TROZO
        mov cx, TROZO
        call en_disco_escribe
        jc .no
        inc word [nB]
        mov si, [bfin]              ; lo demás de después, al final
        sub si, TROZO + 1
        mov di, [bfin]
        dec di
        mov cx, [post]
        sub cx, TROZO
        mov [post], cx
        std
        rep movsb
        cld
        call pon_hueco
        clc
        ret
.no:
        stc
        ret
; AX = número de trozo de B: DX:AX = dónde empieza en el archivo
nB_pos:
        xor dx, dx
        mov cx, TROZO_BITS
.c:
        shl ax, 1
        rcl dx, 1
        loop .c
        ret
; lo que va detrás de la memoria (de B o de C), detrás de ella. CF=1 si no hay
trae_de_B:
        mov cx, TROZO
        cmp word [nB], 0
        jne .hay
        mov ax, [clen]              ; de C: lo que quede, hasta TROZO
        mov dx, [clen+2]
        sub ax, [cpos]
        sbb dx, [cpos+2]
        jnz .hay
        cmp ax, cx
        jae .hay
        mov cx, ax
        jcxz .no
.hay:
        push cx
        call hueco_para_CH
        pop cx
        jc .no
        push cx                     ; lo de después, CX más hacia el hueco:
        mov si, [pstart]            ; el trozo va detrás de ello, al final
        mov di, si
        sub di, cx
        mov cx, [post]
        rep movsb
        pop cx
        mov di, [bfin]
        sub di, cx
        cmp word [nB], 0
        je .de_C
        mov ax, [nB]
        dec ax
        push cx
        call nB_pos
        pop cx
        mov bx, [hB]
        call en_disco_lee
        jc .mal
        dec word [nB]
        jmp short .bien
.de_C:
        mov ax, [cpos]
        mov dx, [cpos+2]
        mov bx, [hC]
        call en_disco_lee
        jc .mal
        add [cpos], cx
        adc word [cpos+2], 0
.bien:
        add [post], cx
        call pon_hueco
        clc
        ret
.mal:
        mov si, [bfin]              ; (no se ha leído: lo de después, a su
        sub si, cx                  ; sitio otra vez)
        dec si
        mov di, [bfin]
        dec di
        mov cx, [post]
        std
        rep movsb
        cld
        stc
        ret
.no:
        stc
        ret
; que el hueco tenga al menos TROZO octetos: si no, se lleva al disco un trozo
; del lado que más tenga. CF=1 si no se puede
hueco_para_CH:
        mov ax, [pstart]
        sub ax, [g_ini]
        cmp ax, TROZO
        jae .si
        mov ax, [pre]
        cmp ax, [post]
        jb .b
        call lleva_a_A
        ret
.b:
        call lleva_a_B
        ret
.si:
        clc
        ret
; lo de la memoria se ha corrido AX octetos hacia el principio (o hacia el
; final, si es negativo): las posiciones guardadas como w, con ello
corre_w:
        sub [top_w], ax
        jae .x
        mov word [top_w], 0         ; (se ha ido al disco: desde el principio)
        mov word [top_k], 0
.x:
        mov byte [filas_ok], 0
        clc
        ret

; ---- que alrededor del cursor haya al menos MARGEN octetos (o el borde del
; documento): así la pantalla siempre está en la memoria ----
asegura:
.a:
        cmp word [pre], MARGEN
        jae .b
        call trae_de_A
        jnc .a
.b:
        cmp word [post], MARGEN
        jae .x
        call trae_de_B
        jnc .b
.x:
        ret

; ---- ir a la posición DX:AX del documento ----
ir_a:
        push si
        push di
.c:
        mov [ir_pos], ax
        mov [ir_pos+2], dx
        sub ax, [lenA]
        sbb dx, [lenA+2]
        jb .antes
        jnz .despues                ; (más de 64 K detrás del principio)
        mov cx, [pre]
        add cx, [post]
        cmp ax, cx
        ja .despues
        call mueve_hueco            ; está en la memoria
        call asegura
        mov byte [filas_ok], 0
        pop di
        pop si
        ret
.antes:
        xor ax, ax                  ; más atrás: todo a lo de después, y se
        call mueve_hueco            ; trae de A
        call trae_de_A
        jmp short .otra
.despues:
        mov ax, [pre]
        add ax, [post]
        call mueve_hueco
        call trae_de_B
.otra:
        mov ax, [ir_pos]
        mov dx, [ir_pos+2]
        jnc .c
        ; (no se ha podido: se queda en el borde)
        call asegura
        pop di
        pop si
        ret

; ---- escribir y borrar ----
; mete las CX letras de SI donde está el cursor (y el cursor, detrás).
; CF=1 si no caben (no queda memoria ni disco)
inserta:
        jcxz .bien
        mov [ins_n], cx
.sitio:
        mov ax, [pstart]
        sub ax, [g_ini]
        cmp ax, [ins_n]
        jae .cabe
        call hueco_para_CH
        jnc .sitio
        call pita
        stc
        ret
.cabe:
        call cursor32               ; deshacer y las marcas del bloque
        mov cx, [ins_n]
        call u_inserta
        call cursor32
        mov cx, [ins_n]
        call marcas_ins
        mov cx, [ins_n]
        mov di, [g_ini]
        add [pre], cx
        rep movsb
        call pon_hueco
        mov byte [cambiado], 1
        mov byte [filas_ok], 0
.bien:
        clc
        ret
; una letra (AL)
inserta_al:
        mov [una], al
        mov si, una
        mov cx, 1
        jmp inserta
; borra CX letras de delante del cursor (si hace falta, se traen del disco).
; Devuelve en CX las que se han borrado
borra_delante:
        mov word [bor_n], 0
        mov [bor_q], cx
.c:
        cmp word [bor_q], 0
        je .x
        cmp word [post], 0
        jne .hay
        call trae_de_B
        jc .x
.hay:
        mov cx, [bor_q]
        cmp cx, [post]
        jbe .k
        mov cx, [post]
.k:
        mov [ins_n], cx             ; CX letras de una vez
        mov si, [pstart]
        call cursor32
        call u_borra
        call cursor32
        mov cx, [ins_n]
        call marcas_bor
        mov cx, [ins_n]
        sub [post], cx
        add [bor_n], cx
        sub [bor_q], cx
        call pon_hueco
        mov byte [cambiado], 1
        mov byte [filas_ok], 0
        jmp .c
.x:
        mov cx, [bor_n]
        ret
; borra la letra de detrás del cursor. CF=1 si no hay
borra_atras:
        cmp word [pre], 0
        jne .hay
        call trae_de_A
        jc .x
.hay:
        mov ax, [pre]
        dec ax
        call mueve_hueco
        mov cx, 1
        call borra_delante
        clc
.x:
        ret

; ---- las marcas del bloque, cuando se escribe o se borra en DX:AX ----
; ¿hay bloque? (ZF=1 si no: sin marcar, y principio y fin iguales)
hay_marcas:
        cmp byte [marcando], 0
        jne .x
        push ax
        mov ax, [b_ini]
        cmp ax, [b_fin]
        jne .y
        mov ax, [b_ini+2]
        cmp ax, [b_fin+2]
.y:
        pop ax
.x:
        ret
; se meten CX letras en DX:AX: lo de detrás se corre
marcas_ins:
        call hay_marcas
        jz marcas_bor.x0
        mov bx, b_ini
        call .una
        mov bx, b_fin
.una:
        cmp dx, [bx+2]
        ja .x
        jb .si
        cmp ax, [bx]
        ja .x
        jne .si
        cmp bx, b_ini               ; (justo en el principio: se queda)
        je .x
.si:
        add [bx], cx
        adc word [bx+2], 0
.x:
        ret
; se quitan CX letras de DX:AX
marcas_bor:
        call hay_marcas
        jnz .si
.x0:
        ret
.si:
        mov bx, b_ini
        call .una
        mov bx, b_fin
.una:
        push ax
        push dx
        cmp dx, [bx+2]              ; antes de lo borrado: se queda
        ja .x
        jb .d
        cmp ax, [bx]
        jae .x
.d:
        add ax, cx                  ; dentro: al principio de lo borrado
        adc dx, 0
        cmp dx, [bx+2]
        jb .detras
        ja .dentro
        cmp ax, [bx]
        jbe .detras
.dentro:
        pop dx
        pop ax
        mov [bx], ax
        mov [bx+2], dx
        ret
.detras:
        sub [bx], cx                ; detrás: se corre
        sbb word [bx+2], 0
.x:
        pop dx
        pop ax
        ret

; =============================================================================
;  Leer el documento de principio a fin (para guardar, exportar, imprimir y
;  buscar): it_pos es por dónde va, y se lee a trozos, de la memoria o del
;  disco, sin que el que lee tenga que saber de dónde viene
; =============================================================================
; empieza a leer desde DX:AX
lee_desde:
        mov [it_pos], ax
        mov [it_pos+2], dx
        mov word [it_n], 0
        ret
; la letra siguiente, en AL. CF=1 si se ha acabado el documento
lee_letra:
        cmp word [it_n], 0
        jne .hay
        push bx
        push cx
        push dx
        push di
        call .trozo
        pop di
        pop dx
        pop cx
        pop bx
        jc .x
.hay:
        push si
        mov si, [it_si]
        lodsb
        mov [it_si], si
        pop si
        dec word [it_n]
        add word [it_pos], 1
        adc word [it_pos+2], 0
        clc
.x:
        ret
; (interna) el trozo que empieza en it_pos: it_si y it_n
.trozo:
        push si
        mov ax, [it_pos]
        mov dx, [it_pos+2]
        sub ax, [lenA]
        sbb dx, [lenA+2]
        jae .noA
        mov ax, [lenA]              ; de A: hasta 512 octetos
        mov dx, [lenA+2]
        sub ax, [it_pos]
        sbb dx, [it_pos+2]
        mov bx, [hA]
        mov si, [it_pos]
        mov di, [it_pos+2]
        jmp .del_disco
.noA:
        test dx, dx
        jnz .fuera
        cmp ax, [pre]
        jae .noPre
        mov si, BUF                 ; de lo de antes
        add si, ax
        mov cx, [pre]
        sub cx, ax
        jmp short .mem
.noPre:
        sub ax, [pre]
        cmp ax, [post]
        jae .detras0
        mov si, [pstart]            ; de lo de después
        add si, ax
        mov cx, [post]
        sub cx, ax
.mem:
        mov [it_si], si
        mov [it_n], cx
        pop si
        clc
        ret
.detras0:
        sub ax, [post]
        jmp short .detras
.fuera:
        sub ax, [pre]
        sbb dx, 0
        sub ax, [post]
        sbb dx, 0
.detras:
        ; DX:AX = cuánto detrás de la memoria. ¿En B?
        push ax
        push dx
        mov ax, [nB]
        call nB_pos                 ; DX:AX = lo que hay en B
        mov cx, ax
        mov bx, dx
        pop dx
        pop ax
        cmp dx, bx
        ja .enC
        jb .enB
        cmp ax, cx
        jae .enC
.enB:
        ; el trozo k (desde arriba) = (DX:AX) / TROZO; dentro, (AX mod TROZO)
        mov di, ax
        and di, TROZO - 1              ; DI = dentro del trozo
        push cx
        mov cx, TROZO_BITS
.sh:
        shr dx, 1
        rcr ax, 1
        loop .sh
        pop cx
        mov bx, [nB]                ; el trozo del archivo: nB-1-k
        dec bx
        sub bx, ax
        mov ax, bx
        push di
        call nB_pos
        pop di
        add ax, di
        adc dx, 0
        mov si, ax                  ; DX:SI = en el archivo
        mov cx, TROZO
        sub cx, di
        mov ax, cx                  ; lo que queda del trozo
        xor cx, cx
        mov di, dx
        mov dx, cx
        mov bx, [hB]
        jmp short .del_disco
.enC:
        sub ax, cx
        sbb dx, bx
        add ax, [cpos]
        adc dx, [cpos+2]
        mov si, ax
        mov di, dx                  ; DI:SI = en C
        mov ax, [clen]
        mov dx, [clen+2]
        sub ax, si
        sbb dx, di
        jb .fin
        mov bx, ax
        or bx, dx
        jz .fin
        mov bx, [hC]
.del_disco:
        ; DX:AX = lo que queda en ese archivo desde DI:SI; se leen hasta 512
        test dx, dx
        jnz .t512
        cmp ax, 512
        jbe .t
.t512:
        mov ax, 512
.t:
        mov cx, ax
        mov ax, si
        mov dx, di
        mov di, ibuf
        call en_disco_lee
        jc .fin
        mov word [it_si], ibuf
        mov [it_n], cx
        pop si
        clc
        ret
.fin:
        pop si
        stc
        ret

; =============================================================================
;  Deshacer: una pila de cambios en UB (los últimos, hasta UB_TAM octetos).
;  Cada cambio: [lo borrado] [posición: 4] [cuántos: 2] [grupo: 1] [tipo: 1].
;  Un grupo es lo que se hace con una tecla (o una ristra de letras seguidas);
;  deshacer quita el último grupo entero.
; =============================================================================
U_INS       equ 1
U_BOR       equ 2

; se van a meter CX letras en DX:AX
u_inserta:
        mov [esp_cx], cx
        cmp byte [u_apag], 0
        jne .x
        cmp byte [u_roto], 0
        jne .x
        mov bx, [u_top]
        cmp bx, 8
        jb .nuevo
        cmp byte [UB+bx-1], U_INS
        jne .nuevo
        mov cl, [UB+bx-2]
        cmp cl, [u_grupo]
        jne .nuevo
        push ax
        push dx
        sub ax, [UB+bx-4]           ; ¿justo detrás del anterior?
        sbb dx, 0
        sub ax, [UB+bx-8]
        sbb dx, [UB+bx-6]
        or ax, dx
        pop dx
        pop ax
        jnz .nuevo
        mov cx, [esp_cx]
        add [UB+bx-4], cx
        ret
.nuevo:
        push ax
        mov ax, 8
        call u_sitio
        pop ax
        jc .x
        mov bx, [u_top]
        mov [UB+bx], ax
        mov [UB+bx+2], dx
        mov cx, [esp_cx]
        mov [UB+bx+4], cx
        mov cl, [u_grupo]
        mov ch, U_INS
        mov [UB+bx+6], cx
        add word [u_top], 8
.x:
        ret
; se van a borrar las CX letras de SI, que están en DX:AX (delante del cursor)
u_borra:
        mov [esp_cx], cx
        mov [u_src], si
        cmp byte [u_apag], 0
        jne .x
        cmp byte [u_roto], 0
        jne .x
        mov bx, [u_top]
        cmp bx, 8
        jb .nuevo
        cmp byte [UB+bx-1], U_BOR
        jne .nuevo
        mov cl, [UB+bx-2]
        cmp cl, [u_grupo]
        jne .nuevo
        cmp ax, [UB+bx-8]           ; ¿en el mismo sitio (Supr seguidos)?
        jne .atras
        cmp dx, [UB+bx-6]
        jne .atras
        ; se añade detrás de lo borrado: la cabecera sube CX
        mov ax, [esp_cx]
        call u_sitio
        jc .x
        mov bx, [u_top]
        lea si, [UB+bx-1]
        mov di, si
        add di, [esp_cx]
        mov cx, 8
        std
        rep movsb
        cld
        lea di, [UB+bx-8]
        jmp short .copia
.atras:
        mov cx, [esp_cx]            ; ¿justo delante (Retroceso seguidos)?
        add ax, cx
        adc dx, 0
        cmp ax, [UB+bx-8]
        jne .nuevo0
        cmp dx, [UB+bx-6]
        jne .nuevo0
        sub ax, cx
        sbb dx, 0
        mov [u_pos], ax
        mov [u_pos+2], dx
        mov ax, cx
        call u_sitio
        jc .x
        mov bx, [u_top]             ; el cambio entero sube CX, y lo nuevo
        mov cx, [UB+bx-4]           ; va delante
        add cx, 8
        mov dx, bx
        sub dx, cx                  ; DX = donde empieza el cambio
        lea si, [UB+bx-1]
        mov di, si
        add di, [esp_cx]
        std
        rep movsb
        cld
        mov ax, [u_pos]             ; su posición, la nueva
        add bx, [esp_cx]
        mov [UB+bx-8], ax
        mov ax, [u_pos+2]
        mov [UB+bx-6], ax
        mov di, dx
        add di, UB
.copia:
        mov si, [u_src]
        mov cx, [esp_cx]
        rep movsb
        mov cx, [esp_cx]
        add [u_top], cx
        mov bx, [u_top]
        add [UB+bx-4], cx
        jmp short .x
.nuevo0:
        sub ax, cx
        sbb dx, 0
.nuevo:
        mov [u_pos], ax
        mov [u_pos+2], dx
        mov ax, [esp_cx]
        add ax, 8
        jc .roto
        cmp ax, UB_TAM
        ja .roto
        call u_sitio
        jc .roto
        mov bx, [u_top]
        lea di, [UB+bx]
        mov si, [u_src]
        mov cx, [esp_cx]
        rep movsb
        mov bx, di
        sub bx, UB
        mov ax, [u_pos]
        mov dx, [u_pos+2]
        mov [UB+bx], ax
        mov [UB+bx+2], dx
        mov cx, [esp_cx]
        mov [UB+bx+4], cx
        mov cl, [u_grupo]
        mov ch, U_BOR
        mov [UB+bx+6], cx
        add bx, 8
        mov [u_top], bx
.x:
        mov si, [u_src]
        ret
.roto:
        mov word [u_top], 0         ; demasiado grande: este cambio (y lo de
        mov byte [u_roto], 1        ; antes) no se podrá deshacer
        jmp .x
; que quepan AX octetos más en la pila: se tiran los cambios más viejos.
; CF=1 si no puede ser
u_sitio:
        push si
        push di
        push cx
.otra:
        mov cx, UB_TAM
        sub cx, [u_top]
        cmp cx, ax
        jae .bien
        ; el cambio más viejo: se busca bajando desde arriba
        mov bx, [u_top]
.b:
        mov cx, [UB+bx-4]
        cmp byte [UB+bx-1], U_BOR
        je .b1
        xor cx, cx
.b1:
        add cx, 8
        mov di, bx
        sub bx, cx
        jnz .b
        ; DI = lo que ocupa el más viejo: se quita
        cmp di, [u_top]
        je .nada                    ; (es el único)
        lea si, [UB+di]
        mov cx, [u_top]
        sub cx, di
        sub [u_top], di
        mov di, UB
        rep movsb
        jmp .otra
.nada:
        mov word [u_top], 0
        stc
        jmp short .x
.bien:
        clc
.x:
        pop cx
        pop di
        pop si
        ret

; ---- deshacer el último grupo de cambios ----
deshacer:
        mov bx, [u_top]
        cmp bx, 8
        jae .hay
        jmp pita
.hay:
        mov al, [UB+bx-2]
        mov [u_cual], al
        mov byte [u_apag], 1
.otro:
        mov bx, [u_top]
        cmp bx, 8
        jb .fin
        mov al, [UB+bx-2]
        cmp al, [u_cual]
        jne .fin
        mov ax, [UB+bx-8]
        mov dx, [UB+bx-6]
        mov cx, [UB+bx-4]
        push cx
        push word [UB+bx-2]
        call ir_a
        pop ax
        pop cx
        cmp ah, U_INS
        jne .bor
        sub word [u_top], 8         ; lo metido, se quita
        call borra_delante
        jmp .otro
.bor:
        mov bx, [u_top]             ; lo borrado, se vuelve a poner
        sub bx, 8
        sub bx, cx
        mov [u_top], bx
        lea si, [UB+bx]
.pon:
        jcxz .otro
        mov ax, cx                  ; (de 512 en 512, que es lo que cabe
        cmp ax, 512                 ; de una vez)
        jbe .p1
        mov ax, 512
.p1:
        sub cx, ax
        push cx
        mov cx, ax
        call inserta
        pop cx
        jmp .pon
.fin:
        mov byte [u_apag], 0
        mov byte [filas_ok], 0
        mov byte [centrar], 1
        ret
