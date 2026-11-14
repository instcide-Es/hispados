; =============================================================================
;  ARCHIVOS.HVP - DOS: las llamadas que no tiene la interfaz de siempre
;  (atributos, cambiar de nombre, carpetas, buscar, espacio libre, fechas) por
;  rm_interrup, con los nombres en dos_buf; y las rutas y los errores.
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

; ESI=ruta -> ECX=atributos
d_atr_leer:
        push edi
        xor edi, edi
        call d_poner
        mov dword [rl+RL_EAX], 0x4300
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        call d_int21
        movzx ecx, word [rl+RL_ECX]
        pop edi
        ret

; ESI=ruta ECX=atributos
d_atr_poner:
        push edi
        xor edi, edi
        call d_poner
        mov dword [rl+RL_EAX], 0x4301
        mov [rl+RL_ECX], ecx
        mov eax, [dbuf]
        mov [rl+RL_EDX], eax
        call d_int21
        pop edi
        ret

; ESI=nombre de antes EDI=el nuevo (los dos con su ruta)
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
; cada vez en dos_buf+DB_DTA y se copia a la propia (así se puede buscar en
; varias carpetas a la vez y entre medias usar DOS para otras cosas)
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

; AL=unidad (0 = A:) -> EAX=KB libres, EDX=KB en total; CF=1 si no se puede
; leer; ECX=1 si es un CD (sus datos son de ISO 9660)
d_libre:
        push ebx
        movzx edx, al
        inc edx
        mov dword [rl+RL_EAX], 0x3600
        mov [rl+RL_EDX], edx
        call d_int21
        cmp ax, 0xFFFF
        je .no
        movzx eax, ax               ; sectores por grupo
        movzx ecx, word [rl+RL_ECX] ; octetos por sector
        imul eax, ecx
        shr eax, 9                  ; en medios KB (de 512)
        movzx ebx, word [rl+RL_EBX] ; grupos libres
        movzx edx, word [rl+RL_EDX] ; grupos en total
        imul edx, eax
        shr edx, 1
        xor ecx, ecx
        test ebx, ebx               ; (un CD: 4 sectores por grupo y nada libre)
        jnz .l
        cmp word [rl+RL_EAX], 4
        jne .l
        inc ecx
.l:
        imul eax, ebx
        shr eax, 1
        pop ebx
        clc
        ret
.no:
        pop ebx
        mov eax, 15
        stc
        ret

; EBX=manejador -> ECX=hora EDX=fecha (5700h); d_fecha_poner los pone (5701h)
d_fecha_leer:
        mov dword [rl+RL_EAX], 0x5700
        mov [rl+RL_EBX], ebx
        call d_int21
        movzx ecx, word [rl+RL_ECX]
        movzx edx, word [rl+RL_EDX]
        ret
d_fecha_poner:
        mov dword [rl+RL_EAX], 0x5701
        mov [rl+RL_EBX], ebx
        mov [rl+RL_ECX], ecx
        mov [rl+RL_EDX], edx
        jmp d_int21

; AL=unidad (0 = A:) -> CF=1 si no existe; EAX=1 si es fija (4408h)
d_unidad:
        push edx
        movzx edx, al
        inc edx
        mov dword [rl+RL_EAX], 0x4408
        mov [rl+RL_EBX], edx
        call d_int21
        movzx eax, ax
        pop edx
        ret

; -> EAX = cuántas disqueteras dice la BIOS (INT 11h)
d_disqueteras:
        mov dword [rl+RL_EAX], 0
        mov byte [rl+RL_INT], 0x11
        push esi
        mov esi, rl
        HV rm_interrup
        pop esi
        mov eax, [rl+RL_EAX]
        test al, 1
        jz .no
        shr eax, 6
        and eax, 3
        inc eax
        ret
.no:
        xor eax, eax
        ret

; =============================================================================
;  Rutas: «C:\CARPETA\OTRA» (la raíz, «C:\»)
; =============================================================================
; EDI=destino ESI=carpeta EBX=nombre: «carpeta\nombre»
ruta_unir:
        push eax
        push esi
        push edi
        call hvl_cad
        cmp byte [edi-1], '\'
        je .n
        mov al, '\'
        stosb
.n:
        mov esi, ebx
        call hvl_cad
        mov byte [edi], 0
        pop edi
        pop esi
        pop eax
        ret

; ESI=ruta -> EAX=largo
largo:
        xor eax, eax
.c:
        cmp byte [esi+eax], 0
        je .x
        inc eax
        jmp .c
.x:
        ret

; ESI -> EDI, con el 0 (hasta ECX letras)
copiar_n:
        push eax
        push ecx
        push esi
        push edi
.c:
        lodsb
        test al, al
        jz .f
        stosb
        loop .c
.f:
        mov byte [edi], 0
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; ESI, EDI: ZF=1 si son iguales (sin mirar mayúsculas)
igual:
        push eax
        push esi
        push edi
.c:
        mov al, [esi]
        mov ah, [edi]
        inc esi
        inc edi
        call mayus2
        cmp al, ah
        jne .x
        test al, al
        jnz .c
.x:
        pop edi
        pop esi
        pop eax
        ret
; (interna) AL y AH, en mayúsculas (también ñ, ç y las vocales con acento
; que tienen mayúscula en la página 437)
mayus2:
        call mayus
        xchg al, ah
        call mayus
        xchg al, ah
        ret
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .e
        sub al, 0x20
.x:
        ret
.e:
        cmp al, 0xA4                ; ñ
        jne .e1
        inc al
        ret
.e1:
        cmp al, 0x87                ; ç
        jne .e2
        mov al, 0x80
        ret
.e2:
        cmp al, 0x82                ; é
        jne .e3
        mov al, 0x90
        ret
.e3:
        cmp al, 0x81                ; ü
        jne .e4
        mov al, 0x9A
.e4:
        ret

; ESI=ruta: la deja en mayúsculas, con «\» y sin la barra del final (salvo
; en la raíz); quita «.» y sube con «..». CF=1 si no empieza por «X:»
ruta_limpia:
        pushad
        mov edi, esi
.m:
        mov al, [edi]
        test al, al
        jz .m1
        cmp al, '/'
        jne .m0
        mov al, '\'
.m0:
        call mayus
        mov [edi], al
        inc edi
        jmp .m
.m1:
        mov al, [esi]
        cmp al, 'A'
        jb .mal
        cmp al, 'Z'
        ja .mal
        cmp byte [esi+1], ':'
        jne .mal
        cmp byte [esi+2], '\'
        je .r
        ; «C:» o «C:CARPETA»: se le pone la barra
        call largo
        lea edi, [esi+eax+1]
        mov ecx, eax
        sub ecx, 1
.h:
        mov al, [edi-1]
        mov [edi], al
        dec edi
        dec ecx
        jnz .h
        mov byte [esi+2], '\'
.r:
        ; los trozos: «.» se quita, «..» sube
        lea edi, [esi+3]            ; donde se escribe
        mov ebx, edi                ; donde se lee
.t:
        cmp byte [ebx], 0
        je .fin
        cmp byte [ebx], '\'
        jne .t1
        inc ebx                     ; (barras repetidas)
        jmp .t
.t1:
        cmp word [ebx], '..'
        jne .t2
        mov al, [ebx+2]
        test al, al
        jz .sube
        cmp al, '\'
        jne .t2
.sube:
        add ebx, 2
        lea eax, [esi+3]
        cmp edi, eax
        jbe .t
        dec edi                     ; quita «\TROZO»
.s1:
        cmp edi, eax
        jbe .t
        dec edi
        cmp byte [edi], '\'
        jne .s1
        jmp .t
.t2:
        cmp byte [ebx], '.'
        jne .t3
        mov al, [ebx+1]
        test al, al
        jz .pto
        cmp al, '\'
        jne .t3
.pto:
        inc ebx
        jmp .t
.t3:
        lea eax, [esi+3]
        cmp edi, eax
        je .t4
        mov byte [edi], '\'
        inc edi
.t4:
        mov al, [ebx]
        test al, al
        jz .fin
        cmp al, '\'
        je .t
        mov [edi], al
        inc edi
        inc ebx
        jmp .t4
.fin:
        mov byte [edi], 0
        popad
        clc
        ret
.mal:
        popad
        stc
        ret

; ESI=ruta: quita el último trozo («C:\A\B» -> «C:\A», «C:\A» -> «C:\»).
; CF=1 si ya es la raíz
ruta_subir:
        push eax
        push edi
        call largo
        cmp eax, 3
        jbe .raiz
        lea edi, [esi+eax]
.b:
        dec edi
        cmp byte [edi], '\'
        jne .b
        lea eax, [esi+2]
        cmp edi, eax
        jne .c
        inc edi                     ; la raíz se queda con su barra
.c:
        mov byte [edi], 0
        pop edi
        pop eax
        clc
        ret
.raiz:
        pop edi
        pop eax
        stc
        ret

; ESI=ruta -> EAX=el último trozo (su nombre), dentro de la misma ruta
ruta_nombre:
        push esi
        call largo
        lea eax, [esi+eax]
.b:
        cmp eax, esi
        jbe .x
        cmp byte [eax-1], '\'
        je .x
        dec eax
        jmp .b
.x:
        pop esi
        ret

; ESI=una carpeta EDI=otra: ZF=1 si EDI es ESI o está dentro de ella
ruta_dentro:
        push eax
        push ecx
        push esi
        push edi
.c:
        mov al, [esi]
        test al, al
        jz .fin
        cmp al, [edi]
        jne .x
        inc esi
        inc edi
        jmp .c
.fin:
        mov al, [edi]               ; detrás: nada o una barra
        test al, al
        jz .x
        cmp byte [esi-1], '\'       ; (la raíz: todo está dentro)
        je .si
        cmp al, '\'
        jmp .x
.si:
        cmp al, al
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; ESI=nombre que escribe el usuario -> lo deja en mayúsculas; CF=1 si no
; vale como nombre de DOS (8 letras, punto y 3; sin espacios ni signos que
; no valen)
nombre_valido:
        pushad
        xor ecx, ecx                ; letras de la parte
        xor edx, edx                ; 1: ya hay punto
        mov edi, esi
.c:
        mov al, [edi]
        test al, al
        jz .fin
        call mayus
        mov [edi], al
        inc edi
        cmp al, '.'
        je .punto
        cmp al, ' '
        jbe .mal
        push edi
        mov edi, no_valen
.n:
        cmp byte [edi], 0
        je .n1
        cmp al, [edi]
        je .malp
        inc edi
        jmp .n
.n1:
        pop edi
        inc ecx
        cmp ecx, 8
        ja .mal
        test edx, edx
        jz .c
        cmp ecx, 3
        ja .mal
        jmp .c
.punto:
        test edx, edx
        jnz .mal
        test ecx, ecx
        jz .mal
        inc edx
        xor ecx, ecx
        jmp .c
.fin:
        cmp edi, esi
        je .mal
        cmp byte [edi-1], '.'
        je .mal
        popad
        clc
        ret
.malp:
        pop edi
.mal:
        popad
        stc
        ret
no_valen    db '"*+,/:;<=>?[\]|', 0

; ESI=nombre («ABC.TXT») EDI=patrón con * y ?: ZF=1 si encaja
coincide:
        pushad
        cmp dword [edi], '*.*'      ; (con el 0: «*.*» es todo)
        je .si
.c:
        mov al, [edi]
        cmp al, '*'
        je .ast
        mov ah, [esi]
        test al, al
        jz .fin
        test ah, ah
        jz .no
        cmp al, '?'
        je .sig
        call mayus2
        cmp al, ah
        jne .no
.sig:
        inc esi
        inc edi
        jmp .c
.ast:
        inc edi
        cmp byte [edi], 0
        je .si
.a1:
        push esi
        push edi
        call .rec
        pop edi
        pop esi
        je .si
        cmp byte [esi], 0
        je .no
        inc esi
        jmp .a1
.fin:
        test ah, ah
        jnz .no
.si:
        popad
        cmp al, al
        ret
.no:
        popad
        or al, 1                    ; ZF=0
        ret
.rec:                               ; (el resto, desde ESI y EDI)
        call coincide
        ret

; =============================================================================
;  Errores
; =============================================================================
; EAX=error de DOS -> ESI=el texto que se dice (mira antes si ha sido un error
; crítico de disco: disquete sin poner, protegido...)
err_texto:
        push eax
        push ebx
        mov ebx, eax
        HV dos_crit
        cmp eax, -1
        je .dos
        mov esi, t_e_protegido
        test eax, eax
        jz .x
        mov esi, t_e_preparada
        cmp eax, 2
        je .x
        mov esi, t_e_disco
        jmp .x
.dos:
        mov esi, t_e_nohay
        cmp ebx, 2
        je .x
        mov esi, t_e_noruta
        cmp ebx, 3
        je .x
        mov esi, t_e_acceso
        cmp ebx, 5
        je .x
        mov esi, t_e_lleno
        cmp ebx, 0x1D
        je .x
        cmp ebx, 0x27
        je .x
        mov esi, t_e_existe
        cmp ebx, 0x50
        je .x
        mov esi, t_e_actual
        cmp ebx, 0x10
        je .x
        mov esi, t_e_preparada
        cmp ebx, 0x15
        je .x
        cmp ebx, 0x0F
        je .x
        mov esi, t_e_protegido
        cmp ebx, 0x13
        je .x
        mov esi, t_e_compartido
        cmp ebx, 0x20
        je .x
        mov esi, t_e_disco
.x:
        pop ebx
        pop eax
        ret

; (interna) olvida un error crítico de antes
crit_olvidar:
        push eax
        HV dos_crit
        pop eax
        ret
