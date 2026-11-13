; =============================================================================
;  Hispa-DOS VELOCIDAD.COM - frenar el procesador para programas antiguos
;  (en el disco, VELOCIDA.COM: el intérprete corta el nombre a 8 letras)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  VELOCIDAD [nivel [/C] [/S] [/1] [programa [argumentos]]]
;     nivel: n% | PENTIUM | 486 | 386 | 286 | XT | NORMAL
;     Sin nada: dice cómo está (/D añade los números del residente).
;     Con programa: frena solo mientras corre y lo deja todo como estaba.
;
;  Cómo frena
;  ----------
;  a) Temporizador. Un residente pequeño (656 octetos) toma la INT 08h y
;     pone el canal 0 del temporizador N veces más deprisa (N = 1..32;
;     divisor 65536/N, modo 2). En cada interrupción «quema» tiempo leyendo
;     la cuenta del temporizador hasta que ha contado la parte del trozo que
;     toca desde que llegó la interrupción, y lo que queda del trozo es para
;     el programa: reparto proporcional, que no depende de lo rápido que sea
;     el equipo. Quema con las interrupciones abiertas,
;     para no retrasar el teclado ni el sonido. A la INT 08h anterior (la
;     BIOS, la hora de DOS, la INT 1Ch) se la llama una de cada N veces:
;     sigue a 18,2 Hz.
;     N es 32 (582 Hz) salvo que al programa le fuera a tocar tan poco en
;     cada trozo (menos de unos 27 µs) que no se pueda medir bien: entonces
;     menos trozos y más largos.
;     El residente se protege de los programas que usan el temporizador. En
;     cada interrupción mira si la cuenta sigue siendo la suya y si la
;     INT 08h sigue apuntándole. Si otro programa ha reprogramado el
;     temporizador o ha tomado la INT 08h después (muchos juegos hacen las
;     dos cosas), deja de tocar el temporizador (y si el otro no lo había
;     reprogramado, se lo devuelve a 18,2 Hz) y pasa al modo «bucle»: una
;     vez por tic de 18,2 Hz quema un número fijo de vueltas, calculado al
;     poner el freno, que no depende de cómo esté programado el temporizador.
;     Así el programa recibe la INT 08h al ritmo que espera y la hora no se
;     descuadra. Con /1 se empieza ya en modo «bucle».
;  b) Cachés (/C, y de serie en XT): en un 486 o superior y en modo real,
;     CR0.CD = 1, NW = 0 y WBINVD. Si el procesador está en modo protegido
;     (V86: un EMM386, Windows) no se puede: se avisa y se frena solo con el
;     temporizador. No depende de la INT 08h: sirve con cualquier programa.
;
;  Calibración
;  -----------
;  La velocidad se mide contando vueltas de un bucle «DEC CX / JNZ» durante
;  4 tics del reloj de la BIOS. Los niveles con nombre son las vueltas por
;  segundo de ese bucle en cada procesador, calculadas con los ciclos por
;  instrucción de sus manuales (tabla_obj). Puesto el freno, se vuelve a
;  medir y se corrige (hasta 4 veces) para acercarse al objetivo; lo que se
;  ha conseguido se dice («medido: ...»).
;
;  Límites conocidos
;  -----------------
;  - Mide un bucle de instrucciones sencillas: es lo que cuentan los bucles
;    de espera de los programas antiguos (los que dan división por cero),
;    pero no dice lo que tarda el vídeo o el disco.
;  - Un programa que tome la INT 08h y no llame a la anterior se queda sin
;    freno de temporizador (queda /C).
;  - Un programa que lea la cuenta del canal 0 dando por hecho el divisor
;    65536 verá otra cosa mientras el freno esté en modo normal (use /1).
;  - Cárguese el último: un residente cargado después hace pasar al modo
;    «bucle» y VELOCIDAD NORMAL no puede soltar la INT 08h (queda inactivo).
;
;  El residente se reconoce por la firma que hay tras el salto inicial (se
;  recorre la cadena de bloques de memoria): no usa ninguna interrupción más.
; =============================================================================
        cpu 486
        bits 16
        org 0x100

MT      equ 4                   ; tics de cada medida
NMAX    equ 32                  ; trozos por tic como mucho (582 Hz)
UNI     equ 256                 ; vueltas del bucle por unidad de medida

M_NORMAL equ 0
M_PCT    equ 1
M_XT     equ 2                  ; 2.. = índice en tabla_obj + 2

start:
        jmp near init
firma       db "HSPVEL1", 0
; ---- estado: «VELOCIDAD x programa» lo guarda y lo repone entero ----
r_estado:
r_activo    db 0                ; el freno del temporizador está puesto
r_cache     db 0                ; se quieren las cachés desactivadas
r_modo      db 0                ; M_NORMAL = sin freno
r_pct       db 0
r_n         db 1                ; trozos por tic
r_bucle     db 0                ; /1: sin reprogramar el temporizador
r_ajeno     db 0                ; el residente ha pasado él solo a «bucle»
            db 0
r_div       dw 0xFFFF           ; divisor del temporizador (FFFFh si N = 1)
r_div4      dw 0x4000           ; su cuarta parte
r_umbral    dw 0                ; cuentas de cada trozo que son del programa
r_quema     dw 0                ; y las que se queman (divisor - umbral)
r_vueltas   dd 1                ; «bucle»: unidades que se queman en cada tic
r_base      dd 0                ; velocidad sin freno (unidades en MT tics)
r_obj       dd 0                ; velocidad pedida
R_ESTADO    equ $ - r_estado
; ---- lo que no se guarda ----
r_cnt       db 1                ; trozos que faltan para el tic de 18,2 Hz
r_dentro    db 0                ; quemando (no se anida)
r_pit       db 0                ; el temporizador está programado por nosotros
r_tsr       db 0                ; 1 = residente; 0 = un «VELOCIDAD x programa»
r_cd        db 0                ; CR0.CD puesto por nosotros
r_desb      db 0                ; interrupciones llegadas con la anterior a medias
r_segu      db 0                ; veces seguidas que se ha vuelto a empezar (2)
            db 0
r_fase      dw 0                ; cuenta con la que llegó la última interrupción
r_prev      dw 0                ; última cuenta leída mientras se quema
r_acc       dw 0                ; cuentas quemadas en este trozo
r_resto     dd 0                ; «bucle»: unidades que quedan por quemar
r_old08     dd 0

int08h:
        cmp byte [cs:r_activo], 0
        je .pasa
        push eax
        cmp byte [cs:r_pit], 0
        je .tic
        ; ¿sigue el temporizador como lo dejamos y somos los primeros?
        push ds
        xor ax, ax
        mov ds, ax
        out 0x43, al                ; retener la cuenta del canal 0
        in al, 0x40
        mov ah, al
        in al, 0x40
        xchg al, ah
        cmp ax, [cs:r_div]
        ja .ajeno                   ; cuenta mayor que nuestro divisor: es de otro
        cmp word [0x20], int08h
        jne .otro
        push ax
        mov ax, cs
        cmp [0x22], ax
        pop ax
        jne .otro
        cmp byte [cs:r_dentro], 0
        jne .nuestro
        mov [cs:r_fase], ax         ; la cuenta con la que llega la interrupción
        mov [cs:r_prev], ax         ; (desde ella se mide lo que se quema)
        mov word [cs:r_acc], 0
        jmp short .nuestro
.otro:
        ; otro ha tomado la INT 08h después. Si la interrupción llega con la
        ; cuenta de siempre, no ha reprogramado el temporizador: se le deja
        ; el ritmo normal de 18,2 Hz
        sub ax, [cs:r_fase]
        jns .o1
        neg ax
.o1:
        cmp ax, [cs:r_div4]
        ja .ajeno
        mov al, 0x36
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
.ajeno:
        ; desde ahora, una vez por tic y sin mirar el temporizador
        mov byte [cs:r_n], 1
        mov byte [cs:r_cnt], 1
        mov byte [cs:r_ajeno], 1
        mov byte [cs:r_pit], 0
.nuestro:
        pop ds
.tic:
        dec byte [cs:r_cnt]
        jnz .eoi
        mov al, [cs:r_n]
        mov [cs:r_cnt], al
        pop eax
        pushf
        call far [cs:r_old08]       ; el tic de 18,2 Hz de siempre
        jmp short .frena
.eoi:
        mov al, 0x20
        out 0x20, al
        pop eax
.frena:
        cmp byte [cs:r_dentro], 0
        jne .anidada
        mov byte [cs:r_dentro], 1
        cmp byte [cs:r_pit], 0
        je .bucle
        ; quemar, con las interrupciones abiertas, hasta que el temporizador
        ; haya contado r_quema desde que llegó la interrupción (lo que queda
        ; del trozo es para el programa)
        push ax
        push bx
.q:
        cli
        xor al, al
        out 0x43, al
        in al, 0x40
        mov ah, al
        in al, 0x40
        xchg al, ah
        sti
        mov bx, [cs:r_prev]
        mov [cs:r_prev], ax
        sub bx, ax                  ; cuentas desde la lectura anterior
        jnc .q1
        add bx, [cs:r_div]          ; (ha dado la vuelta)
.q1:
        add [cs:r_acc], bx
        jc .qf
        mov bx, [cs:r_acc]
        cmp bx, [cs:r_quema]
        jb .q
.qf:
        cli
        pop bx
        pop ax
        jmp short .sal
.bucle:
        ; quemar un número fijo de vueltas (calibrado al poner el freno)
        push eax
        push cx
        mov eax, [cs:r_vueltas]
        mov [cs:r_resto], eax
        sti
.o:
        mov cx, UNI
.i:
        dec cx
        jnz .i
        dec dword [cs:r_resto]
        jnz .o
        cli
        pop cx
        pop eax
.sal:
        mov byte [cs:r_segu], 0
        mov byte [cs:r_dentro], 0
        iret
.anidada:
        ; ha llegado otra interrupción con la anterior aún quemando: se
        ; empieza a quemar de nuevo para este trozo, pero no más de dos veces
        ; seguidas (el programa nunca se queda sin nada)
        inc byte [cs:r_desb]
        cmp byte [cs:r_segu], 2
        jae .fin
        inc byte [cs:r_segu]
        mov word [cs:r_acc], 0
        push eax
        mov eax, [cs:r_vueltas]
        mov [cs:r_resto], eax
        pop eax
.fin:
        iret
.pasa:
        jmp far [cs:r_old08]
res_end:

; =============================================================================
;  Parte no residente
; =============================================================================
init:
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
        call detectar
        mov si, 0x81
        call skip_sp
        cmp byte [si], 13
        je estado
        cmp word [si], '/?'
        je ayuda
        mov ax, [si]
        and ah, 0xDF
        cmp ax, '/D'
        jne .niv
        mov byte [opt_d], 1
        jmp estado
.niv:
        call nivel
        jc mal_nivel
.opts:
        call skip_sp
        cmp byte [si], '/'
        jne .prog
        mov al, [si+1]
        call upcase
        cmp al, '?'
        je ayuda
        mov bx, opt_c
        cmp al, 'C'
        je .op
        mov bx, opt_s
        cmp al, 'S'
        je .op
        mov bx, opt_1
        cmp al, '1'
        jne mal_opcion
.op:
        mov byte [bx], 1
        add si, 2
        jmp .opts
.prog:
        cmp byte [si], 13
        je .sinprog
        call ej_buscar
        jc no_encontrado
        mov [args], si
        mov byte [conprog], 1
.sinprog:
        call buscar_res
        cmp byte [modo], M_NORMAL
        jne poner
        cmp byte [conprog], 0
        je normal

; ---- poner un nivel (o, con NORMAL y programa, quitarlo mientras corre) ----
poner:
        mov ax, [res_seg]
        test ax, ax
        jz .sinres
        ; ya hay un residente: guardar cómo está y soltar el freno para medir
        mov byte [habia], 1
        push ds
        mov ds, ax
        mov si, r_estado
        mov di, previo
        mov cx, R_ESTADO
        rep movsb
        pop ds
        mov es, ax
        call desactivar
        push ds
        pop es
.sinres:
        call medir
        mov [base], eax
        cmp byte [modo], M_NORMAL
        je lanzar
        ; el objetivo, en las mismas unidades
        cmp byte [modo], M_PCT
        jne .tabla
        movzx ecx, byte [pct]
        mul ecx
        mov ecx, 100
        div ecx
        jmp .obj
.tabla:
        movzx bx, byte [modo]
        sub bx, M_XT
        shl bx, 2
        mov eax, [tabla_obj + bx]
        call de_miles
.obj:
        test eax, eax
        jnz .o1
        inc eax
.o1:
        mov [obj], eax
        cmp eax, [base]
        jb .frenar
        ; este equipo ya es así de lento
        mov si, msg_lento
        call puts
        call pon_nombre
        mov si, msg_lento2
        call puts
        jmp lanzar
.frenar:
        cmp word [res_seg], 0
        jne .hay
        call enganchar
.hay:
        mov es, [res_seg]
        mov al, [modo]
        mov [es:r_modo], al
        mov al, [pct]
        mov [es:r_pct], al
        mov eax, [base]
        mov [es:r_base], eax
        mov [cur], eax
        mov eax, [obj]
        mov [es:r_obj], eax
        ; ---- cachés: /C, o XT sin /S ----
        cmp byte [opt_s], 0
        jne .sincache
        cmp byte [opt_c], 0
        jne .cache
        cmp byte [modo], M_XT
        jne .sincache
.cache:
        cmp byte [es486], 0
        jne .c486
        cmp byte [opt_c], 0
        je .sincache
        mov si, msg_no486
        call puts
        jmp .sincache
.c486:
        cmp byte [v86], 0
        je .creal
        mov si, msg_v86
        call puts
        jmp .sincache
.creal:
        mov byte [es:r_cache], 1
        call cache_aplicar
        call medir
        mov [cur], eax
.sincache:
        ; ---- temporizador, si aún va más deprisa de lo pedido ----
        mov eax, [cur]
        cmp eax, [obj]
        ja .temp
        mov [medido], eax
        jmp .resumen
.temp:
        call calcular
        call ajustar
.resumen:
        call resumen
        ; (sigue en lanzar)

; ---- ejecutar el programa, si lo hay, y dejarlo todo como estaba ----
lanzar:
        cmp byte [conprog], 0
        je .sinprog
        call c_quitar
        mov si, [args]
        call ej_lanzar
        pushf
        push ax
        cmp byte [nuevo], 0
        je .nc
        call c_poner
.nc:
        pop ax
        popf
        jnc .ok
        mov byte [codigo], 255
        mov si, msg_nomem
        cmp ax, 8
        je .em
        mov si, msg_exec
.em:
        call puts
        mov si, ej_ruta
        call puts
        call crlf
        jmp .reponer
.ok:
        mov [codigo], al
.reponer:
        cmp byte [nuevo], 0
        jne .propio
        cmp byte [habia], 0
        je .fin
        ; el residente que había: si sigue ahí, como estaba
        mov ax, [res_seg]
        push ax
        call buscar_res
        pop ax
        cmp ax, [res_seg]
        jne .fin
        mov es, ax
        cli
        mov si, previo
        mov di, r_estado
        mov cx, R_ESTADO
        rep movsb
        sti
        call aplicar
        call cache_aplicar
        jmp .fin
.propio:
        ; el freno era nuestro: quitarlo y soltar la INT 08h
        push cs
        pop es
        call desactivar
        call es_ultimo
        jne .quedo
        push ds
        lds dx, [r_old08]
        mov ax, 0x2508
        int 0x21
        pop ds
        jmp .fin
.quedo:
        mov si, msg_quedo
        call puts
        jmp residente
.fin:
        call c_quitar
        mov al, [codigo]
        mov ah, 0x4C
        int 0x21
.sinprog:
        cmp byte [nuevo], 0
        je .fin
        mov si, msg_residente
        call puts
        ; (sigue)

; quedarse residente (inactivo o frenando) con el código de retorno [codigo]
residente:
        call c_quitar
        mov byte [r_tsr], 1
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, (res_end - $$ + 0x100 + 15) / 16
        mov al, [codigo]
        mov ah, 0x31
        int 0x21

; ---- VELOCIDAD NORMAL ----
normal:
        mov ax, [res_seg]
        test ax, ax
        jnz .hay
        mov si, msg_ya
        jmp .di
.hay:
        mov es, ax
        call desactivar
        mov si, msg_normal
        cmp byte [es:r_tsr], 0
        je .di                      ; es de un «VELOCIDAD x programa» en curso
        call es_ultimo
        mov si, msg_noquito
        jne .di
        push ds
        lds dx, [es:r_old08]
        mov ax, 0x2508
        int 0x21
        pop ds
        mov ah, 0x49
        int 0x21
        mov si, msg_quitado
.di:
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- VELOCIDAD (sin nada): cómo está ----
estado:
        call buscar_res
        call medir
        mov [medido], eax
        mov ax, [res_seg]
        test ax, ax
        jz .normal
        mov es, ax
        cmp byte [es:r_modo], M_NORMAL
        jne .frenado
.normal:
        mov si, msg_sinfreno
        call puts
        jmp .indice
.frenado:
        mov al, [es:r_modo]
        mov [modo], al
        mov al, [es:r_pct]
        mov [pct], al
        mov si, msg_vel
        call puts
        call pon_nombre
        call crlf
        mov si, msg_ahora
        call puts
        mov eax, [medido]
        mov ecx, [es:r_base]
        call pon_pct
        mov si, msg_ahora2
        call puts
        cmp byte [es:r_cd], 0
        je .nocd
        mov si, msg_cache
        call puts
.nocd:
        cmp byte [es:r_activo], 0
        je .indice
        mov si, msg_ajeno
        cmp byte [es:r_ajeno], 0
        jne .ut
        mov si, msg_untic
        cmp byte [es:r_bucle], 0
        je .indice
.ut:
        call puts
.indice:
        cmp byte [opt_d], 0
        je .sind
        cmp word [res_seg], 0
        je .sind
        ; /D: los números del residente
        mov si, msg_d1
        call puts
        movzx eax, byte [es:r_n]
        call putnum
        mov si, msg_d2
        call puts
        movzx eax, word [es:r_umbral]
        call putnum
        mov si, msg_d3
        call puts
        mov eax, [es:r_vueltas]
        call putnum
        mov si, msg_d4
        call puts
        movzx eax, byte [es:r_desb]
        call putnum
        mov si, msg_d5
        call puts
        movzx eax, byte [es:r_pit]
        call putnum
        call crlf
.sind:
        mov si, msg_indice
        call puts
        mov eax, [medido]
        call a_miles
        call putnum
        mov si, msg_indice2
        call puts
        mov ax, 0x4C00
        int 0x21

ayuda:
        mov si, msg_ayuda
        call puts
        mov ax, 0x4C00
        int 0x21
mal_nivel:
        mov si, msg_malnivel
        jmp error
mal_opcion:
        mov si, msg_malopcion
        jmp error
no_encontrado:
        mov si, msg_nf
        call puts
        mov si, ej_nombre
        call puts
        mov si, msg_crlf
error:
        call puts
        mov ax, 0x4CFF
        int 0x21

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

; ---- el nivel: DS:SI -> palabra; sale [modo], [pct], SI detrás; CF si no ----
nivel:
        mov bx, tabla_niv
.t:
        cmp byte [bx], 0
        je .num
        push si
        mov di, bx
.c:
        mov al, [si]
        call upcase
        mov ah, [di]
        test ah, ah
        jz .finpal
        cmp al, ah
        jne .no
        inc si
        inc di
        jmp .c
.finpal:
        cmp al, ' '
        jbe .si
        cmp al, '/'
        jne .no
.si:
        add sp, 2
        mov al, [di+1]
        mov [modo], al
        clc
        ret
.no:
        pop si
.s:
        cmp byte [bx], 0
        je .s2
        inc bx
        jmp .s
.s2:
        add bx, 2
        jmp .t
.num:
        call parse_num
        jc .mal
        cmp byte [si], '%'
        jne .np
        inc si
.np:
        cmp byte [si], ' '
        jbe .fin
        cmp byte [si], '/'
        jne .mal
.fin:
        test eax, eax
        jz .mal
        cmp eax, 100
        ja .mal
        mov byte [modo], M_NORMAL
        je .ok                      ; 100 % = NORMAL
        mov [pct], al
        mov byte [modo], M_PCT
.ok:
        clc
        ret
.mal:
        stc
        ret

; ---- ¿486 o superior? ¿modo real? ----
detectar:
        pushfd
        pop eax
        mov ecx, eax
        cli                         ; (una interrupción borraría AC)
        xor eax, 0x40000            ; el bit AC solo existe desde el 486
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x40000
        jz .n486
        mov byte [es486], 1
.n486:
        smsw ax
        test al, 1                  ; PE: modo protegido (aquí, V86)
        jz .real
        mov byte [v86], 1
        ret
.real:
        mov al, [es486]
        mov [puede_cr0], al
        ret

; ---- buscar el residente: [res_seg] (0 si no hay) ----
buscar_res:
        push es
        mov word [res_seg], 0
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
.l:
        mov es, ax
        mov bx, ax
        inc bx
        cmp [es:1], bx              ; un bloque que es su propio dueño: un PSP
        jne .nx
        mov dx, cs
        cmp bx, dx
        je .nx
        cmp word [es:3], 0x11
        jb .nx
        push es
        mov es, bx
        mov si, firma
        mov di, firma
        mov cx, 8
        repe cmpsb
        pop es
        jne .nx
        mov [res_seg], bx
        jmp .out
.nx:
        cmp byte [es:0], 'M'
        jne .out
        mov ax, es
        add ax, [es:3]
        inc ax
        jmp .l
.out:
        pop es
        ret

; ---- tomar la INT 08h en esta copia del programa ----
enganchar:
        push es
        call c_poner
        mov ax, 0x3508
        int 0x21
        mov [r_old08], bx
        mov [r_old08+2], es
        mov dx, int08h
        mov ax, 0x2508
        int 0x21
        mov [res_seg], cs
        mov byte [nuevo], 1
        pop es
        ret

; ZF=1 si la INT 08h apunta al residente ES (nadie la ha tomado después)
es_ultimo:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        cmp word [0x20], int08h
        jne .r
        mov ax, es
        cmp [0x22], ax
.r:
        pop ax
        pop ds
        ret

; Ctrl+C: mientras la INT 08h es nuestra y aún no somos residentes, no se
; puede acabar el programa sin soltarla; se ignora (y se repone la de antes
; para el programa que se ejecuta)
c_poner:
        cmp byte [c_puesta], 0
        jne .r
        push es
        mov ax, 0x3523
        int 0x21
        mov [old23], bx
        mov [old23+2], es
        pop es
        mov dx, int23h
        mov ax, 0x2523
        int 0x21
        mov byte [c_puesta], 1
.r:
        ret
c_quitar:
        cmp byte [c_puesta], 0
        je .r
        push ds
        lds dx, [old23]
        mov ax, 0x2523
        int 0x21
        pop ds
        mov byte [c_puesta], 0
.r:
        ret
int23h:
        iret

; ---- medir: EAX = unidades (de UNI vueltas) en MT tics; nunca 0 ----
medir:
        push ebx
        push cx
        push dx
        push es
        sti
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
.w:
        cmp ax, [es:0x6C]
        je .w
        mov dx, [es:0x6C]
        xor ebx, ebx
.o:
        mov cx, UNI
.i:
        dec cx
        jnz .i
        inc ebx
        mov ax, [es:0x6C]
        sub ax, dx
        cmp ax, MT
        jb .o
        mov eax, ebx
        test eax, eax
        jnz .r
        inc eax
.r:
        pop es
        pop dx
        pop cx
        pop ebx
        ret

; unidades en MT tics <-> miles de vueltas por segundo
;   miles = unidades * UNI * 18,2065 / MT / 1000 = unidades * 1,165216
a_miles:
        push ecx
        push edx
        mov ecx, 1165216
        mul ecx
        mov ecx, 1000000
        div ecx
        pop edx
        pop ecx
        ret
de_miles:
        push ecx
        push edx
        mov ecx, 1000000
        mul ecx
        mov ecx, 1165216
        div ecx
        pop edx
        pop ecx
        ret

; ---- programar el temporizador según el estado del residente ES ----
aplicar:
        push ax
        cli
        mov al, [es:r_n]
        mov [es:r_cnt], al
        cmp byte [es:r_activo], 0
        je .normal
        cmp byte [es:r_bucle], 0
        jne .normal
        cmp byte [es:r_ajeno], 0
        jne .normal
        mov al, 0x34                ; canal 0, modo 2: la cuenta baja de una
        out 0x43, al                ; en una del divisor a 1 en cada trozo
        xor ax, ax
        cmp byte [es:r_n], 1
        je .d
        mov ax, [es:r_div]
.d:
        out 0x40, al
        mov al, ah
        out 0x40, al
        mov byte [es:r_pit], 1
        jmp .r
.normal:
        cmp byte [es:r_pit], 0
        je .r
        mov al, 0x36                ; como lo deja la BIOS: modo 3, 65536
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
        mov byte [es:r_pit], 0
.r:
        sti
        pop ax
        ret

; ---- poner o quitar CR0.CD según [ES:r_cache] ----
cache_aplicar:
        cmp byte [puede_cr0], 0
        je .r
        push eax
        mov al, [es:r_cache]
        cmp al, [es:r_cd]
        je .p
        mov [es:r_cd], al
        cli
        mov eax, cr0
        and eax, 0x9FFFFFFF         ; CD = 0, NW = 0: cachés normales
        cmp byte [es:r_cache], 0
        je .w
        or eax, 0x40000000          ; CD = 1: no se llenan...
.w:
        mov cr0, eax
        wbinvd                      ; ...y se vacían
        sti
.p:
        pop eax
.r:
        ret

; ---- soltar el freno del residente ES ----
desactivar:
        mov byte [es:r_activo], 0
        mov byte [es:r_cache], 0
        mov byte [es:r_modo], M_NORMAL
        mov byte [es:r_ajeno], 0
        call aplicar
        jmp cache_aplicar

; ---- poner el freno del temporizador y la primera estimación ----
;      [cur] = velocidad ahora (sin el freno del temporizador), [obj]
calcular:
        mov bl, 1
        cmp byte [opt_1], 0
        jne .n
        call medir_lect
        mov bl, NMAX
.n1:
        cmp bl, 1
        jbe .n
        ; al programa le tienen que tocar en cada trozo al menos [tmin]
        ; cuentas del temporizador (32, unos 27 µs, o más si el equipo tarda
        ; en leerlo); si no, trozos más largos
        call umbral0
        cmp eax, [tmin]
        jae .n
        shr bl, 1
        jmp .n1
.n:
        mov al, [opt_1]
        mov [es:r_bucle], al
        ; «bucle»: K = (cur - obj) / MT unidades por tic
        mov eax, [cur]
        sub eax, [obj]
        shr eax, 2                  ; / MT
        call poner_k
        ; (sigue)

; ---- poner el freno con BL trozos por tic ----
poner_n:
        cli
        mov byte [es:r_activo], 0
        mov [es:r_n], bl
        mov byte [es:r_ajeno], 0
        mov ax, 0xFFFF
        mov dx, 0x4000
        cmp bl, 1
        je .d
        xor ax, ax
        mov dx, 1                   ; 65536 / N
        movzx cx, bl
        div cx
        mov dx, ax
        shr dx, 2
.d:
        mov [es:r_div], ax
        mov [es:r_div4], dx
        call umbral0
        call poner_t
        mov byte [es:r_activo], 1
        jmp aplicar

; ---- cuánto tarda este equipo en leer la cuenta del temporizador ----
; Mientras quema, el residente la lee sin parar; lo que tarda cada lectura
; (1 µs en un PC, 50 en algunas máquinas virtuales) es lo más fino que puede
; hilar. [tmin] = 4 lecturas + 8 cuentas (32 por lo menos): menos que eso no
; se le deja al programa en cada trozo.
medir_lect:
        push ebx
        push cx
        push dx
        cli
        mov al, 0x34                ; modo 2, 65536: la cuenta baja de una en una
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
        mov byte [es:r_pit], 1      ; (aplicar lo dejará como toque)
        xor ebx, ebx
        call .leer
        mov dx, ax
        mov cx, 16
.l:
        call .leer
        sub dx, ax
        movzx edx, dx
        add ebx, edx
        mov dx, ax
        loop .l
        sti
        shr ebx, 2                  ; / 16 lecturas * 4
        add ebx, 8
        cmp ebx, 32
        jae .a
        mov ebx, 32
.a:
        cmp ebx, 16384
        jbe .b
        mov ebx, 16384
.b:
        mov [tmin], ebx
        pop dx
        pop cx
        pop ebx
        ret
.leer:
        xor al, al
        out 0x43, al
        in al, 0x40
        mov ah, al
        in al, 0x40
        xchg al, ah
        ret

; EAX = (65536 / BL) * obj / cur: la cuenta en la que hay que dejar de quemar
umbral0:
        push ecx
        push edx
        mov eax, 65536
        xor edx, edx
        movzx ecx, bl
        div ecx
        mul dword [obj]
        mov ecx, [cur]
        cmp edx, ecx
        jb .d
        mov eax, 0xFFFF
        jmp .r
.d:
        div ecx
.r:
        pop edx
        pop ecx
        ret

; ---- EAX = umbral (con signo): se acota (de [tmin] a 15/16 del divisor) y
;      se guarda ----
poner_t:
        push ecx
        push edx
        call divisor
        mov edx, ecx
        shr edx, 4
        sub ecx, edx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        cmp eax, [tmin]
        jge .b
        mov eax, [tmin]
.b:
        call divisor
        sub ecx, eax
        cli
        mov [es:r_umbral], ax
        mov [es:r_quema], cx
        sti
        pop edx
        pop ecx
        ret

; ECX = divisor del temporizador con los trozos por tic de ahora
divisor:
        movzx ecx, word [es:r_div]
        cmp byte [es:r_n], 1
        jne .r
        mov ecx, 65536
.r:
        ret

; ---- EAX = unidades que quemar en cada tic («bucle»): se acota (a [techo],
;      o a 63/64 de lo que se calcula que dura el tic) y se guarda ----
poner_k:
        push ecx
        mov ecx, [techo]
        test ecx, ecx
        jnz .t
        mov ecx, [cur]
        shr ecx, 2                  ; / MT
        push eax
        mov eax, ecx
        shr eax, 6
        sub ecx, eax
        pop eax
.t:
        cmp eax, ecx
        jbe .a
        mov eax, ecx
.a:
        test eax, eax
        jnz .b
        inc eax
.b:
        mov [es:r_vueltas], eax
        pop ecx
        ret

; ---- medir con el freno puesto y corregir hasta acercarse al objetivo ----
ajustar:
        mov byte [rondas], 4
        cmp byte [es:r_bucle], 0
        je .l
        mov byte [rondas], 6
.l:
        mov al, [es:r_desb]
        mov [desb0], al
        call medir
        mov [medido], eax
        cmp byte [es:r_ajeno], 0
        jne .fin                    ; otro programa manda en el temporizador
        mov al, [es:r_desb]
        sub al, [desb0]
        cmp byte [es:r_bucle], 0
        jne .desbk
        ; si a uno de cada ocho trozos le ha llegado la interrupción
        ; siguiente con el anterior aún quemando (el equipo tarda en leer el
        ; temporizador), trozos más largos
        mov bl, [es:r_n]
        cmp bl, 1
        jbe .cerca
        shl al, 1                   ; (MT = 4: N * MT / 8 = N / 2)
        cmp al, bl
        jb .cerca
        cmp byte [rondas], 0
        je .fin
        shr bl, 1
        call poner_n
        jmp .l
.desbk:
        ; «bucle»: si la mitad de los tics han llegado con el anterior aún
        ; quemando, se quema de más: un poco menos, y ese es el techo
        cmp al, MT / 2
        jb .cerca
        cmp byte [rondas], 0
        je .fin
        dec byte [rondas]
        mov eax, [es:r_vueltas]
        mov ecx, eax
        shr ecx, 5
        sub eax, ecx
        mov [techo], eax
        call poner_k
        jmp .l
.cerca:
        ; ¿a menos de un 4 % del objetivo?
        mov eax, [medido]
        sub eax, [obj]
        jae .pos
        neg eax
.pos:
        mov ecx, 25
        mul ecx
        test edx, edx
        jnz .corr
        cmp eax, [obj]
        jbe .fin
.corr:
        cmp byte [rondas], 0
        je .fin
        dec byte [rondas]
        cmp byte [es:r_bucle], 0
        jne .k
        ; Al programa le toca en cada trozo el umbral T más un tiempo fijo L
        ; (lo que tarda en llegar la interrupción, menos lo que se pierde en
        ; entrar y salir): medido/cur = (T + L) / D. Con eso, L y el T nuevo:
        ;     L = medido * D / cur - T        T' = obj * D / cur - L
        ; Si T' queda en nada, menos trozos por tic (D mayor): L pesa menos.
        call divisor                ; ECX = D
        mov eax, [medido]
        call .porcur
        movzx edx, word [es:r_umbral]
        sub eax, edx
        mov ebp, eax                ; L (con signo)
        mov bl, [es:r_n]
.d:
        mov eax, [obj]
        call .porcur
        sub eax, ebp                ; T'
        cmp eax, [tmin]
        jge .t
        cmp bl, 1
        jbe .t
        shr bl, 1
        shl ecx, 1
        jmp .d
.t:
        mov dx, [es:r_umbral]
        cmp bl, [es:r_n]
        je .pt
        push eax
        call poner_n
        pop eax
        call poner_t
        jmp .l
.pt:
        call poner_t
        cmp dx, [es:r_umbral]
        jne .l
        ret                         ; no da más de sí
.k:
        ; el programa recibe 1 - K*c del tiempo: K' = K * (cur-obj)/(cur-medido)
        mov ebx, [es:r_vueltas]
        mov ecx, [cur]
        sub ecx, [medido]
        jbe .doble                  ; no se nota el freno: el doble
        mov eax, [cur]
        sub eax, [obj]
        mul ebx
        cmp edx, ecx
        jae .doble
        div ecx
        mov edx, ebx                ; sin pasarse: entre la mitad y el doble
        shr edx, 1
        cmp eax, edx
        jae .m1
        mov eax, edx
.m1:
        mov edx, ebx
        shl edx, 1
        jc .pk
        cmp eax, edx
        jbe .pk
.doble:
        mov eax, ebx
        shl eax, 1
        jnc .pk
        mov eax, 0xFFFFFFFF
.pk:
        call poner_k
        cmp ebx, [es:r_vueltas]
        jne .l
.fin:
        ret
; EAX = EAX * ECX / [cur] (acotado a ECX)
.porcur:
        push edx
        mul ecx
        cmp edx, [cur]
        jb .pc1
        mov eax, ecx
        jmp .pc2
.pc1:
        div dword [cur]
.pc2:
        pop edx
        ret

; ---- escribir el nivel [modo]/[pct] ----
pon_nombre:
        push eax
        push si
        cmp byte [modo], M_PCT
        jne .t
        movzx eax, byte [pct]
        call putnum
        mov si, msg_pct
        call puts
        jmp .r
.t:
        mov si, msg_como
        call puts
        movzx si, byte [modo]
        sub si, M_XT
        shl si, 1
        mov si, [tabla_nom + si]
        call puts
.r:
        pop si
        pop eax
        ret

; escribir EAX * 100 / ECX (redondeado)
pon_pct:
        push eax
        push ecx
        push edx
        test ecx, ecx
        jnz .a
        inc ecx
.a:
        mov edx, 100
        mul edx
        push ecx
        shr ecx, 1
        add eax, ecx
        adc edx, 0
        pop ecx
        cmp edx, ecx
        jb .d
        mov eax, 9999
        jmp .p
.d:
        div ecx
.p:
        call putnum
        pop edx
        pop ecx
        pop eax
        ret

; ---- «Velocidad: ... (medido: ...)» ----
resumen:
        mov si, msg_vel
        call puts
        call pon_nombre
        mov si, msg_medido
        call puts
        mov eax, [medido]
        cmp byte [modo], M_PCT
        jne .cpu
        mov ecx, [base]
        call pon_pct
        mov si, msg_medido2
        jmp .f
.cpu:
        mov ecx, [obj]
        call pon_pct
        mov si, msg_medido3
.f:
        call puts
        mov es, [res_seg]
        cmp byte [es:r_cd], 0
        je .nocd
        mov si, msg_cache
        call puts
.nocd:
        ; ¿se ha quedado lejos (más de un 25 % por encima)?
        mov eax, [obj]
        mov ecx, eax
        shr ecx, 2
        add eax, ecx
        cmp [medido], eax
        jbe .r
        mov si, msg_nollega
        call puts
        cmp byte [es:r_cd], 0
        jne .r
        cmp byte [puede_cr0], 0
        je .r
        mov si, msg_nollega2
        call puts
.r:
        ret

%include "util.inc"
%include "ejecutar.inc"

; niveles: palabra, 0, modo
tabla_niv   db "NORMAL", 0, M_NORMAL
            db "XT", 0, M_XT
            db "8088", 0, M_XT
            db "286", 0, M_XT + 1
            db "386", 0, M_XT + 2
            db "486", 0, M_XT + 3
            db "PENTIUM", 0, M_XT + 4
            db 0
; miles de vueltas por segundo de «DEC CX / JNZ» en cada procesador, con los
; ciclos de reloj de sus manuales:
;   8088 a 4,77 MHz:  DEC 3 + JNZ 16, más el tiempo de leer el código por su
;                     bus de 8 bits: unos 20 ciclos por vuelta
;   286 a 12 MHz:     DEC 2 + JNZ 7+m: unos 10,5 ciclos
;   386DX a 33 MHz:   DEC 2 + JNZ 7+m: unos 10,5 ciclos
;   486DX2 a 66 MHz:  DEC 1 + JNZ 3: 4 ciclos
;   Pentium a 100 MHz: las dos instrucciones se emparejan: 1 ciclo
tabla_obj   dd 240, 1140, 3200, 16600, 100000
tabla_nom   dw nom_xt, nom_286, nom_386, nom_486, nom_pent
nom_xt      db "un XT (8088 a 4,77 MHz)", 0
nom_286     db "un 286 a 12 MHz", 0
nom_386     db "un 386 a 33 MHz", 0
nom_486     db "un 486 a 66 MHz", 0
nom_pent    db "un Pentium a 100 MHz", 0

msg_ayuda   db "Frena el procesador para los juegos y programas antiguos que en un equipo", 13, 10
            db "rápido fallan (división por cero, pantalla negra) o van demasiado deprisa.", 13, 10, 13, 10
            db "VELOCIDAD [nivel [/C] [/S] [/1] [programa [argumentos]]]", 13, 10, 13, 10
            db "  nivel     n%      un porcentaje de la velocidad normal (del 1 al 99)", 13, 10
            db "            PENTIUM, 486, 386, 286 o XT   parecida a la de ese procesador", 13, 10
            db "            NORMAL  quita el freno (y VELOCIDAD de la memoria, si puede)", 13, 10
            db "  programa  frena solo mientras se ejecuta ese programa; al volver, todo", 13, 10
            db "            queda como estaba", 13, 10
            db "  /C        desactiva además las cachés del procesador (486 o superior)", 13, 10
            db "  /S        solo con el temporizador, sin tocar las cachés (XT las quita)", 13, 10
            db "  /1        frena una vez por tic del reloj, sin reprogramar el temporizador", 13, 10, 13, 10
            db "Sin nada, dice la velocidad actual. Ejemplos:", 13, 10
            db "  VELOCIDAD 386        VELOCIDAD 25% JUEGO.EXE        VELOCIDAD NORMAL", 13, 10
            db "Cárguelo después de los demás programas residentes. Si un juego pierde el", 13, 10
            db "freno al arrancar (reprograma el temporizador), pruebe con /C.", 13, 10, 0
msg_malnivel db "VELOCIDAD: nivel no válido. Use un porcentaje (25%), PENTIUM, 486, 386,", 13, 10
            db "286, XT o NORMAL. VELOCIDAD /? lo explica.", 13, 10, 0
msg_malopcion db "VELOCIDAD: opción no válida. Las opciones son /C, /S y /1.", 13, 10, 0
msg_nf      db "VELOCIDAD: no se encuentra el programa ", 0
msg_crlf    db 13, 10, 0
msg_nomem   db "VELOCIDAD: no hay memoria suficiente para ejecutar ", 0
msg_exec    db "VELOCIDAD: no se ha podido ejecutar ", 0
msg_vel     db "Velocidad: ", 0
msg_pct     db " % de la normal", 0
msg_como    db "como ", 0
msg_medido  db " (medido: ", 0
msg_medido2 db " %).", 13, 10, 0
msg_medido3 db " % del objetivo).", 13, 10, 0
msg_cache   db "Las cachés del procesador están desactivadas.", 13, 10, 0
msg_nollega db "Aviso: este equipo no se deja frenar tanto con el temporizador.", 13, 10, 0
msg_nollega2 db "Pruebe a añadir /C (desactiva las cachés del procesador).", 13, 10, 0
msg_lento   db "Este equipo no es más rápido que ", 0
msg_lento2  db ": no se frena.", 13, 10, 0
msg_no486   db "Aviso: las cachés solo se pueden desactivar en un 486 o superior.", 13, 10, 0
msg_v86     db "Aviso: el procesador está en modo protegido (V86) y las cachés no se pueden", 13, 10
            db "desactivar desde aquí; se frena solo con el temporizador.", 13, 10, 0
msg_residente db "VELOCIDAD queda en la memoria. VELOCIDAD NORMAL lo quita.", 13, 10, 0
msg_quedo   db "VELOCIDAD no ha podido soltar el temporizador (otro programa lo tomó", 13, 10
            db "después): queda en la memoria, sin frenar.", 13, 10, 0
msg_ya      db "La velocidad ya es la normal.", 13, 10, 0
msg_normal  db "Velocidad normal.", 13, 10, 0
msg_quitado db "Velocidad normal. VELOCIDAD se ha quitado de la memoria.", 13, 10, 0
msg_noquito db "Velocidad normal. VELOCIDAD sigue en la memoria, sin frenar, porque otro", 13, 10
            db "programa residente se cargó después.", 13, 10, 0
msg_sinfreno db "Velocidad normal (sin freno).", 13, 10, 0
msg_ahora   db "Medido ahora: ", 0
msg_ahora2  db " % de la velocidad normal.", 13, 10, 0
msg_untic   db "Frena una vez por tic del reloj (18,2 Hz).", 13, 10, 0
msg_ajeno   db "Otro programa usa el temporizador: frena una vez por tic (18,2 Hz).", 13, 10, 0
msg_d1      db "Trozos por tic: ", 0
msg_d2      db "  cuentas del programa: ", 0
msg_d3      db "  vueltas: ", 0
msg_d4      db "  tics montados: ", 0
msg_d5      db "  temporizador propio: ", 0
msg_indice  db "Velocidad medida (índice): ", 0
msg_indice2 db 13, 10, "(XT = 240; 286 = 1.140; 386 = 3.200; 486 = 16.600; Pentium = 100.000)", 13, 10, 0

prog_end_data:
        absolute prog_end_data
bss_start:
res_seg     resw 1
args        resw 1
old23       resd 1
base        resd 1
cur         resd 1
obj         resd 1
medido      resd 1
modo        resb 1
pct         resb 1
opt_c       resb 1
opt_s       resb 1
opt_1       resb 1
opt_d       resb 1
conprog     resb 1
habia       resb 1
nuevo       resb 1
codigo      resb 1
es486       resb 1
v86         resb 1
puede_cr0   resb 1
c_puesta    resb 1
rondas      resb 1
desb0       resb 1
techo       resd 1
tmin        resd 1
previo      resb R_ESTADO
u_lbuf      resb 84
u_numbuf    resb 16
            EJ_BSS
            alignb 2
            resb 512
stack_top:
prog_end:
