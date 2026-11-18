; =============================================================================
;  Hispa-DOS REVISAR.COM - la parte de FAT32 (MS-DOS 7.1)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como CHKDSK de MS-DOS 7.1 (Windows 95 OSR2/98) en una unidad FAT32: revisa
;  y da el informe, pero no corrige (con /F lo dice y revisa sin escribir;
;  para corregir, la herramienta de disco del sistema que lo tenga).
;
;  Una FAT32 puede tener millones de clústeres (200 GB con clústeres de 32 KB
;  son 6,5 millones): no caben ni la FAT ni un dueño por clúster en la memoria
;  convencional. Así que:
;   - la FAT se lee por trozos de R32_FATN sectores (una caché de un trozo);
;   - de cada clúster solo se guarda un bit, «ya lo usa alguien», en un mapa
;     del tamaño que quepa en la memoria. Si no cabe entero, el recorrido del
;     árbol se repite por ventanas de clústeres (cada pasada marca solo los de
;     su ventana): un vínculo cruzado es un clúster que ya estaba marcado, y
;     uno perdido, uno ocupado en la FAT que nadie ha marcado.
;   - las cadenas se siguen con tope (las de un archivo, lo que dice su
;     tamaño y una más; las de una carpeta, 2 MB): un bucle no cuelga nada.
; =============================================================================

R32_FATN    equ 8                   ; sectores de FAT por lectura (4 KB)
R32_DIRMAX  equ 2 * 1024 * 1024     ; una carpeta no pasa de 2 MB (65.536 entradas)
; una carpeta en la pila del recorrido (F_PLEN y F_MATCH, en el mismo sitio
; que las de FAT12/16: append_name y set_match valen para las dos)
G_FIRST     equ 0                   ; primer clúster
G_CLUS      equ 4                   ; el que se lee (FFFFFFFFh = terminado)
G_SEC       equ 10                  ; sector dentro del clúster
G_ENT       equ 12                  ; entrada dentro del sector
G_STEPS     equ 14                  ; clústeres recorridos (palabra; tope)
G_PARENT    equ 18                  ; primer clúster de la madre
G_SZ        equ 24

; ---- entrada: el sector de arranque es de FAT32 (sin sectores por FAT de
; 16 bits). No vuelve: termina el programa. ----
r32_main:
        mov word [ebpb], 0x42
        cmp word [boot+0x11], 0         ; sin raíz fija
        jne not_fat
        mov eax, [boot+0x24]            ; sectores por FAT
        test eax, eax
        jz not_fat
        mov [spf32], eax
        movzx ecx, byte [boot+0x10]
        mov [nfats], cl
        imul eax, ecx
        movzx ecx, word [boot+0x0E]
        add eax, ecx
        mov [data_lba], eax
        ; la FAT que vale: la primera o, sin espejo (bit 7), la activa
        mov [fat_lba], ecx
        test byte [boot+0x28], 0x80
        jz .m
        movzx eax, byte [boot+0x28]
        and al, 0x0F
        cmp al, [nfats]
        jae not_fat
        imul eax, [spf32]
        add [fat_lba], eax
.m:
        movzx edx, word [boot+0x13]
        test edx, edx
        jnz .t
        mov edx, [boot+0x20]
.t:
        sub edx, [data_lba]
        jbe not_fat
        mov eax, edx
        xor edx, edx
        movzx ecx, byte [boot+0x0D]
        div ecx
        cmp eax, 65525
        jb not_fat
        cmp eax, 0x0FFFFFF5
        ja not_fat
        mov [nclus32], eax
        add eax, 2
        mov [endc], eax                 ; primer número que ya no es clúster
        mov eax, [spf32]
        shl eax, 7                      ; 128 entradas por sector
        cmp eax, [endc]
        jb not_fat                      ; la FAT no llega
        mov eax, [boot+0x2C]
        mov [rootc], eax
        call r32_inrange
        jc not_fat
        mov eax, R32_DIRMAX
        xor edx, edx
        div dword [bpc]
        mov [dircap], eax
        mov dword [fc_sec], 0xFFFFFFFF
        ; /F: como CHKDSK de MS-DOS 7.1, en FAT32 solo se informa
        cmp byte [opt_f], 0
        je .nf
        mov si, msg_f32nof
        call puts
        mov byte [opt_f], 0
        mov byte [warned], 1
.nf:
        call r32_alloc
        call r32_label
        call show_head
        ; las pasadas: una por ventana del mapa
        mov dword [win_lo], 0
        mov byte [pass], 1
.p:
        call r32_clear
        mov eax, [win_lo]
        add eax, [winsz]
        mov [win_hi], eax
        call r32_walk
        call r32_scan
        mov eax, [win_hi]
        mov [win_lo], eax
        mov byte [pass], 2
        cmp eax, [endc]
        jb .p
        cmp byte [have_spec], 0
        je .nsp
        cmp byte [noncontig], 0
        jne .nsp
        mov si, msg_contig
        call puts
.nsp:
        call r32_lost
        call r32_summary
        mov ax, 0x4C00
        cmp byte [errors], 0
        je .ex
        mov al, 255
.ex:
        int 0x21

; EAX = clúster: CF si no es de datos
r32_inrange:
        cmp eax, 2
        jb .no
        cmp eax, [endc]
        jae .no
        clc
        ret
.no:
        stc
        ret

; ---- la FAT, por trozos de R32_FATN sectores ----
; EAX = clúster -> EAX = su entrada (28 bits)
r32_fat:
        push ebx
        push ecx
        push edx
        mov ebx, eax
        shr eax, 7                      ; sector de la FAT
        and eax, -R32_FATN
        cmp eax, [fc_sec]
        je .h
        push ebx
        mov [fc_sec], eax
        add eax, [fat_lba]
        ; (el último trozo puede ser más corto)
        mov ecx, [spf32]
        sub ecx, [fc_sec]
        cmp ecx, R32_FATN
        jbe .n
        mov ecx, R32_FATN
.n:
        mov bx, fatc
        mov dx, ds
        call disk_rd
        pop ebx
        jnc .h
        mov dword [fc_sec], 0xFFFFFFFF
        jmp rd_err
.h:
        and ebx, R32_FATN * 128 - 1
        mov eax, [fatc+ebx*4]
        and eax, 0x0FFFFFFF
        pop edx
        pop ecx
        pop ebx
        ret

; ---- el mapa de bits: lo más grande que quepa (hasta lo que haga falta) ----
r32_alloc:
        mov eax, [endc]
        add eax, 127
        shr eax, 7                      ; párrafos para todos (128 bits cada uno)
        cmp eax, 0xFFFF
        jbe .w
        mov eax, 0xFFFF
.w:
        mov bp, ax
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21                        ; (falla: BX = el mayor bloque)
        sub bx, 64                      ; algo de margen
        jbe no_mem
        cmp bx, bp
        jbe .s
        mov bx, bp
.s:
        cmp bx, 64
        jb no_mem
        mov [bmpar], bx
        mov ah, 0x48
        int 0x21
        jc no_mem
        mov [bmseg], ax
        movzx eax, word [bmpar]
        shl eax, 7
        mov [winsz], eax
        ret

; a cero el mapa
r32_clear:
        push es
        mov dx, [bmseg]
        mov bp, [bmpar]
.z:
        mov es, dx
        xor di, di
        mov cx, bp
        cmp cx, 0x0800
        jbe .z1
        mov cx, 0x0800
.z1:
        sub bp, cx
        add dx, cx
        shl cx, 3
        xor ax, ax
        rep stosw
        test bp, bp
        jnz .z
        pop es
        ret

; EAX = clúster -> ZF=0 si está en la ventana; entonces ES:DI y DL = su bit
r32_bit:
        cmp eax, [win_lo]
        jb .out
        cmp eax, [win_hi]
        jae .out
        push eax
        push cx
        sub eax, [win_lo]
        mov cl, al
        and cl, 7
        mov dl, 1
        shl dl, cl
        mov edi, eax
        shr edi, 7                      ; párrafo
        add di, [bmseg]
        mov es, di
        mov di, ax
        shr di, 3
        and di, 15
        pop cx
        pop eax
        or dl, dl                       ; ZF=0
        ret
.out:
        cmp al, al                      ; ZF=1
        ret

; ---- la etiqueta del volumen: en la raíz (una cadena) ----
r32_label:
        mov eax, [rootc]
        xor ebp, ebp
.c:
        mov [lb_clus], eax
        xor cx, cx
.s:
        call r32_csec
        call load_dir
        mov si, dirbuf
.e:
        mov al, [si]
        test al, al
        jz .r
        cmp al, 0xE5
        je .n
        mov al, [si+11]
        cmp al, 0x0F
        je .n
        test al, 0x08
        jz .n
        mov byte [have_lab], 1
        mov di, curlab
        push cx
        mov cx, 11
        push si
        rep movsb
        pop si
        pop cx
        mov ax, [si+0x16]
        mov [lab_time], ax
        mov ax, [si+0x18]
        mov [lab_date], ax
        ret
.n:
        add si, 32
        cmp si, dirbuf + 512
        jb .e
        inc cx
        cmp cl, [boot+0x0D]
        jb .s
        inc ebp
        cmp ebp, [dircap]
        jae .r
        mov eax, [lb_clus]
        call r32_fat
        call r32_inrange
        jnc .c
.r:
        ret

; [lb_clus] y CX = sector dentro del clúster -> EAX = sector del disco
r32_csec:
        mov eax, [lb_clus]
; EAX = clúster, CX = sector dentro -> EAX = sector del disco
r32_lba:
        push edx
        push ecx
        sub eax, 2
        movzx edx, byte [boot+0x0D]
        imul eax, edx
        movzx ecx, cx
        add eax, ecx
        add eax, [data_lba]
        pop ecx
        pop edx
        ret

; ---- recorrido del árbol (una pasada) ----
r32_top:
        mov bx, [depth]
        dec bx
        imul bx, bx, G_SZ
        add bx, frames32
        ret

r32_walk:
        mov word [num], 0
        mov word [last_key], 0xFFFF
        mov al, [drive]
        add al, 'A'
        mov [path], al
        mov word [path+1], ':\'
        mov byte [path+3], 0
        ; la cadena de la raíz, también marcada
        mov eax, [rootc]
        mov ecx, [dircap]
        call r32_follow
        cmp byte [pass], 1
        jne .r1
        add [c_dir], eax                ; (sus clústeres cuentan como de carpetas)
.r1:
        mov di, frames32
        mov cx, G_SZ
        xor al, al
        rep stosb
        mov eax, [rootc]
        mov [frames32+G_FIRST], eax
        mov [frames32+G_CLUS], eax
        mov word [frames32+F_PLEN], 3
        mov word [depth], 1
        mov bx, frames32
        call set_match
.l:
        call r32_next
        jc .pop
        call r32_entry
        jmp .l
.pop:
        dec word [depth]
        jnz .l
        ret

; siguiente entrada de la carpeta de arriba -> SI (copia en ent32), BX =
; carpeta; CF al terminarse
r32_next:
        call r32_top
        mov eax, [bx+G_CLUS]
        cmp eax, 0xFFFFFFFF
        je .end
        mov cx, [bx+G_SEC]
        call r32_lba
        call load_dir
        mov si, [bx+G_ENT]
        shl si, 5
        add si, dirbuf
        cmp byte [si], 0
        je .end
        push di
        push cx
        mov di, ent32
        mov cx, 16
        rep movsw
        pop cx
        pop di
        mov si, ent32
        inc word [bx+G_ENT]
        cmp word [bx+G_ENT], 16
        jb .ok
        mov word [bx+G_ENT], 0
        inc word [bx+G_SEC]
        movzx ax, byte [boot+0x0D]
        cmp [bx+G_SEC], ax
        jb .ok
        mov word [bx+G_SEC], 0
        inc word [bx+G_STEPS]
        movzx eax, word [bx+G_STEPS]
        cmp eax, [dircap]
        jae .last
        mov eax, [bx+G_CLUS]
        call r32_fat
        call r32_inrange
        jc .last
        mov [bx+G_CLUS], eax
        jmp .ok
.last:
        mov dword [bx+G_CLUS], 0xFFFFFFFF
.ok:
        clc
        ret
.end:
        mov dword [bx+G_CLUS], 0xFFFFFFFF
        stc
        ret

; SI = copia de la entrada, BX = su carpeta
r32_entry:
        mov di, [bx+F_PLEN]
        mov byte [path+di], 0
        mov al, [si]
        cmp al, 0xE5
        je .r
        mov al, [si+11]
        cmp al, 0x0F                    ; nombre largo
        je .r
        test al, 0x08                   ; etiqueta
        jnz .r
        cmp byte [si], '.'
        je .r
        inc word [num]
        cmp word [num], 0xFFFE          ; (es solo para no repetir la ruta)
        jb .nn
        mov word [num], 1
.nn:
        call append_name
        mov ax, [si+0x14]
        shl eax, 16
        mov ax, [si+0x1A]
        mov [e_first], eax
        test byte [si+11], 0x10
        jnz .dir
        ; ---- un archivo ----
        cmp byte [pass], 1
        jne .nv
        cmp byte [opt_v], 0
        je .nv
        push si
        mov si, path
        call puts
        call crlf
        pop si
        mov ax, [num]
        mov [last_key], ax
.nv:
        mov eax, [si+0x1C]
        xor edx, edx
        div dword [bpc]
        test edx, edx
        jz .e
        inc eax
.e:
        mov [e_need], eax
        lea ecx, [eax+1]
        mov eax, [e_first]
        call r32_follow
        cmp byte [pass], 1
        jne .r
        mov ecx, eax
        test byte [si+11], 0x02
        jz .u
        inc dword [n_hid]
        add [c_hid], ecx
        jmp .chk
.u:
        inc dword [n_usr]
        add [c_usr], ecx
.chk:
        cmp byte [fl_rep], 0
        jne .frag
        cmp byte [fl_xl], 0
        jne .frag
        cmp ecx, [e_need]
        je .frag
        mov dx, msg_badch               ; más larga que el archivo
        ja .rp
        mov dx, msg_size                ; más corta
.rp:
        mov ax, [num]
        call report
.frag:
        call r32_frag
.r:
        ret
        ; ---- una carpeta ----
.dir:
        cmp byte [pass], 1
        jne .nv2
        cmp byte [opt_v], 0
        je .nv2
        push si
        mov si, msg_vdir
        call puts
        mov si, path
        call puts
        call crlf
        pop si
.nv2:
        mov eax, [e_first]
        mov ecx, [dircap]
        call r32_follow
        cmp byte [pass], 1
        jne .d1
        test eax, eax
        jnz .d0
        cmp byte [fl_rep], 0
        jne .r
        mov dx, msg_badch
        mov ax, [num]
        call report
        ret
.d0:
        inc dword [n_dir]
        add [c_dir], eax
.d1:
        cmp byte [fl_xl], 0
        jne .r                          ; (es de otro: no se entra)
        test eax, eax
        jz .r
        ; su primer sector tiene que empezar por «.» (si no, serían datos de
        ; otro archivo)
        mov eax, [e_first]
        xor cx, cx
        call r32_lba
        call load_dir
        push si
        push di
        mov si, dirbuf
        mov di, s_dot
        mov cx, 11
        repe cmpsb
        pop di
        pop si
        jne .bad
        test byte [dirbuf+11], 0x10
        jz .bad
        ; a la pila
        cmp word [depth], MAXDEPTH
        jae .deep
        mov eax, [bx+G_FIRST]
        mov ecx, [e_first]
        inc word [depth]
        call r32_top
        mov [bx+G_PARENT], eax
        mov [bx+G_FIRST], ecx
        mov [bx+G_CLUS], ecx
        xor eax, eax
        mov [bx+G_SEC], ax
        mov [bx+G_ENT], ax
        mov [bx+G_STEPS], ax
        mov ax, [cur_end]
        sub ax, path
        mov [bx+F_PLEN], ax
        jmp set_match
.bad:
        cmp byte [pass], 1
        jne .r
        mov dx, msg_notdir
        mov ax, [num]
        jmp report
.deep:
        cmp byte [pass], 1
        jne .r
        mov dx, msg_deep
        mov ax, [num]
        call report
        mov byte [partial], 1
        ret

; ¿archivo del patrón en varios bloques? (como en FAT12/16)
r32_frag:
        cmp word [bx+F_MATCH], 0
        je .r
        cmp dword [e_blocks], 1
        jbe .r
        push si
        mov di, spec_pat
        mov cx, 11
.m:
        mov al, [di]
        cmp al, '?'
        je .mn
        cmp al, [si]
        jne .nm
.mn:
        inc si
        inc di
        loop .m
        mov byte [noncontig], 1
        mov si, path
        call puts
        mov si, msg_frag1
        call puts
        mov eax, [e_blocks]
        call putnum
        mov si, msg_frag2
        call puts
.nm:
        pop si
.r:
        ret

; ---- una cadena: EAX = primer clúster, ECX = tope ----
; -> EAX = clústeres buenos; marca en el mapa los de la ventana; fl_rep si la
; cadena está rota (y en la primera pasada, avisado); fl_xl si choca con otra
; (avisado en la pasada de su ventana); e_blocks, los bloques
r32_follow:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push es
        mov byte [fl_rep], 0
        mov byte [fl_xl], 0
        xor esi, esi                    ; ESI = clústeres
        mov dword [e_blocks], 0
        mov dword [e_prev], 0
        test eax, eax
        jz .done
        mov ebx, ecx                    ; EBX = tope
.l:
        call r32_inrange
        jc .bad
        push eax
        call r32_fat
        mov edx, eax                    ; EDX = el siguiente
        pop eax
        test edx, edx
        jz .bad                         ; apunta a uno libre
        cmp edx, 0x0FFFFFF7
        je .bad                         ; defectuoso
        mov [e_next], edx
        call r32_bit
        jz .nb
        test [es:di], dl
        jnz .xl
        or [es:di], dl
.nb:
        mov edx, [e_next]
        inc esi
        mov ecx, [e_prev]
        inc ecx
        cmp ecx, eax
        je .ct
        inc dword [e_blocks]
.ct:
        mov [e_prev], eax
        cmp edx, 0x0FFFFFF8
        jae .done
        cmp esi, ebx
        jae .done                       ; tope: más larga de lo que puede ser
        mov eax, edx
        jmp .l
.xl:
        ; ya era de otro (o de sí misma: un bucle)
        mov byte [fl_xl], 1
        push si
        push eax
        call warn
        mov dx, msg_xl32
        mov ax, [num]
        call report
        pop eax
        call putnum
        mov al, '.'
        call putc
        call crlf
        pop si
        jmp .done
.bad:
        mov byte [fl_rep], 1
        cmp byte [pass], 1
        jne .done
        push si
        mov dx, msg_badch
        mov ax, [num]
        call report
        pop si
.done:
        mov eax, esi
        pop es
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ---- la FAT de la ventana: libres, defectuosos y perdidos ----
r32_scan:
        mov eax, [win_lo]
        cmp eax, 2
        jae .s
        mov eax, 2
.s:
        mov ecx, [win_hi]
        cmp ecx, [endc]
        jbe .l
        mov ecx, [endc]
.l:
        cmp eax, ecx
        jae .r
        push eax
        call r32_fat
        mov edx, eax
        pop eax
        test edx, edx
        jnz .u
        inc dword [n_free]
        jmp .n
.u:
        cmp edx, 0x0FFFFFF7
        jne .o
        inc dword [n_bad]
        jmp .n
.o:
        push edx
        call r32_bit
        test [es:di], dl
        pop edx
        jnz .n
        ; perdido; empieza cadena si el siguiente no es otro perdido de la
        ; misma ventana (de otra ventana no se sabe: cuenta como cadena)
        inc dword [nlost]
        inc dword [nchains]
        push eax
        mov eax, edx
        call r32_inrange
        jc .nc
        call r32_bit
        jz .nc
        test [es:di], dl
        jnz .nc
        push ecx
        call r32_fat
        pop ecx
        test eax, eax
        jz .nc
        cmp eax, 0x0FFFFFF7
        je .nc
        dec dword [nchains]             ; la sigue otra perdida
.nc:
        pop eax
.n:
        inc eax
        jmp .l
.r:
        push ds
        pop es
        ret

; ---- informe de las perdidas (en FAT32, sin corregir) ----
r32_lost:
        mov eax, [nlost]
        test eax, eax
        jz .r
        cmp byte [partial], 0
        je .w
        mov si, msg_partial
        jmp puts
.w:
        call warn
        ; (las cadenas se cuentan por su final: unas que lleguen a la misma
        ; no restan dos veces, así que no pueden salir menos de una)
        cmp dword [nchains], 0
        jg .c
        mov dword [nchains], 1
.c:
        mov si, msg_lost1
        call puts
        mov eax, [nlost]
        call putnum
        mov si, msg_lost2
        call puts
        mov eax, [nchains]
        call putnum
        mov si, msg_lost3
        call puts
        mov eax, [nlost]
        mul dword [bpc]
        mov cl, 0
        call putnum64_w
        mov si, msg_wfreed
        call puts
.r:
        ret

; ---- informe final (cifras de 64 bits) ----
; ECX = clústeres -> sus bytes, con 15 columnas
r32_num:
        mov eax, ecx
        mul dword [bpc]
        mov cl, 15
        jmp putnum64_w

r32_summary:
        call crlf
        mov ecx, [nclus32]
        call r32_num
        mov si, msg_total
        call puts
        cmp dword [n_hid], 0
        je .nh
        mov ecx, [c_hid]
        call r32_num
        mov si, msg_en
        call puts
        mov eax, [n_hid]
        call putnum
        mov si, msg_hid
        call puts
.nh:
        cmp dword [n_dir], 0
        je .nd
        mov ecx, [c_dir]
        call r32_num
        mov si, msg_en
        call puts
        mov eax, [n_dir]
        call putnum
        mov si, msg_dirs
        call puts
.nd:
        mov ecx, [c_usr]
        call r32_num
        mov si, msg_en
        call puts
        mov eax, [n_usr]
        call putnum
        mov si, msg_usr
        call puts
        cmp dword [n_bad], 0
        je .nbd
        mov ecx, [n_bad]
        call r32_num
        mov si, msg_bad
        call puts
.nbd:
        mov ecx, [n_free]
        call r32_num
        mov si, msg_avail
        call puts
        call crlf
        mov eax, [bpc]
        mov cl, 15
        call putnum_w
        mov si, msg_bpc
        call puts
        mov eax, [nclus32]
        mov cl, 15
        call putnum_w
        mov si, msg_units
        call puts
        mov eax, [n_free]
        mov cl, 15
        call putnum_w
        mov si, msg_ufree
        call puts
        call crlf
        mov es, [bmseg]
        mov ah, 0x49
        int 0x21
        push ds
        pop es
        int 0x12
        movzx eax, ax
        shl eax, 10
        mov cl, 15
        call putnum_w
        mov si, msg_mem1
        call puts
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        movzx eax, bx
        shl eax, 4
        mov cl, 15
        call putnum_w
        mov si, msg_mem2
        call puts
        ret

; EDX:EAX con separador de miles, alineado a la derecha en CL columnas
putnum64_w:
        pushad
        mov di, n64buf + 31
        mov byte [di], 0
        mov ebx, 10
        xor ch, ch
.l:
        mov esi, eax
        mov eax, edx
        xor edx, edx
        div ebx
        xchg eax, esi
        div ebx
        add dl, '0'
        cmp ch, 3
        jne .nc
        dec di
        mov byte [di], '.'
        mov ch, 0
.nc:
        dec di
        mov [di], dl
        inc ch
        mov edx, esi
        mov ebp, eax
        or ebp, edx
        jnz .l
        mov ax, n64buf + 31
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
        popad
        ret
