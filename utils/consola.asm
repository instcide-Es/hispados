; =============================================================================
;  Hispa-DOS CONSOLA.COM - consola remota por red (abierta, visible, con código)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  CONSOLA [/PUERTO:n] [/CADUCA:s]
;  Escucha en un puerto TCP (2323 si no se dice) y espera a que alguien se
;  conecte. NO es una puerta trasera: la lanza el usuario a mano, avisa en la
;  pantalla del equipo de cada conexión y enseña aquí, desde el principio, un
;  CÓDIGO de 6 cifras que hay que decirle a quien se conecta; sin ese código
;  no se entra. El código es de un solo uso, se cambia tras 3 intentos
;  fallidos y caduca a los 10 minutos (/CADUCA:s), y siempre se ve en la
;  pantalla el vigente y lo que le queda. Dentro, ejecuta órdenes (DIR, VER, TYPE...) con
;  %COMSPEC% /C y le manda su salida. SALIR cierra la sesión; Esc en el teclado
;  del equipo cierra la sesión (o CONSOLA si no hay sesión). Ver docs/CONSOLA.md.
;
;  Necesita HSPRED (u otro packet driver) y la red configurada (ver CONFRED).
;  Los programas de pantalla completa (EDITAR, menús) no van en esta versión de
;  órdenes: es trabajo futuro (redirigir la pantalla y el teclado enteros).
; =============================================================================
        cpu 386
        bits 16
        org 0x100

PUERTO_DEF  equ 2323
CADUCA_DEF  equ 600                 ; el código caduca a los 10 minutos (s)
TICS_DIA    equ 0x1800B0            ; tics del reloj de la BIOS en un día
FALLOS_MAX  equ 3                   ; intentos fallidos con un mismo código
PAUSA_TICS  equ 546                 ; 30 s sin aceptar conexiones tras ellos,
PAUSA_MAX   equ 546 * 32            ; el doble cada vez, hasta 16 minutos
ESPERA_MAL  equ 36                  ; 2 s antes de contestar a un código malo
LINEA_MAX   equ 256
RX_SZ       equ 1024
OBUF_SZ     equ 1024

start:
        cld
        mov sp, pila_tope
        ; a cero toda la zona de datos (la de red.inc incluida)
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        ; encoger el bloque de memoria: el hijo (COMMAND.COM) necesita sitio
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        ; la rutina que recibe los datos de la conexión
        mov word [t_rutina], con_rx
        mov word [puerto], PUERTO_DEF
        mov dword [cod_dur], CADUCA_DEF * 182 / 10
        mov dword [bloq_dur], PAUSA_TICS
        call azar_ini
        ; la unidad de arranque: para COMMAND.COM y el archivo temporal
        mov ax, 0x3305
        int 0x21                    ; DL = unidad de arranque (1 = A)
        add dl, 'A' - 1
        mov [s_outfile], dl
        mov [s_redir + 2], dl
        mov [comspec_def], dl
        ; ---- argumentos: /PUERTO:n (o /P:n) y /? ----
        mov si, 0x81
.o:
        lodsb
        cmp al, 13
        je .fin_args
        cmp al, ' '
        je .o
        cmp al, 9
        je .o
        cmp al, '/'
        jne .o
        mov al, [si]
        cmp al, '?'
        je ayuda
        call upcase
        cmp al, 'C'
        je .fc
        cmp al, 'P'
        jne .o
.fp:
        lodsb
        cmp al, 13
        je uso
        cmp al, ':'
        jne .fp
        call parse_num
        jc uso
        test eax, eax
        jz uso
        cmp eax, 65535
        ja uso
        mov [puerto], ax
        jmp .o
.fc:                                ; /CADUCA:s (de 5 s a 1 día)
        lodsb
        cmp al, 13
        je uso
        cmp al, ':'
        jne .fc
        call parse_num
        jc uso
        cmp eax, 5
        jb uso
        cmp eax, 86400
        ja uso
        imul eax, eax, 182
        add eax, 9
        xor edx, edx
        mov ecx, 10
        div ecx
        mov [cod_dur], eax
        jmp .o
.fin_args:
        call find_comspec
        ; ---- la red ----
        call red_abrir
        jc nored
        xor al, al
        call red_config
        jc nored
        cmp dword [my_ip], 0
        je nored
        ; resolver ya la MAC de la puerta de enlace (en el contexto principal,
        ; donde sí se espera la respuesta): así se puede contestar al SYN de un
        ; cliente de fuera de la red sin preguntar por ARP durante el sondeo
        mov eax, [my_gw]
        test eax, eax
        jz .singw
        call arp_resolver
.singw:
        ; ---- aviso en la pantalla del equipo ----
        mov si, m_banner1
        call puts
        mov eax, [my_ip]
        call ip_poner
        mov si, m_banner2
        call puts
        movzx eax, word [puerto]
        call putnum
        mov si, m_banner3
        call puts
        ; el primer código, ya en la pantalla (antes de que llegue nadie)
        mov si, m_cod_ini
        call nuevo_codigo

; ---- esperar una conexión ----
esperar:
        mov bx, [puerto]
        call tcp_escuchar
        mov byte [est_ant], 3           ; (diag) estado de partida: escuchando
        mov dword [cuenta_ult], -1      ; volver a pintar la línea del código
.espl:
        call red_sondear
        call azar_mezcla
        call cuenta_atender
        call tcp_estado
        ; diagnóstico: si el estado del saludo TCP cambia, escribirlo en pantalla
        ; y en CONSOLA.LOG (así se ve si el SYN llega y si se contesta el SYN+ACK)
        cmp al, [est_ant]
        je .mira
        mov [est_ant], al
        push ax
        call diag_estado
        pop ax
.mira:
        cmp al, 2
        je conectada
        call tecla_esc
        jc salir
        jmp .espl

; (diag) AL = estado TCP nuevo. Lo anuncia en la pantalla del equipo:
;   4 = ha llegado un SYN y se ha contestado SYN+ACK (con la IP de quien llama)
;   2 = conexión establecida
;   3 = vuelto a escuchar
diag_estado:
        pushad
        call linea_cierra
        cmp al, 4
        jne .e2
        mov si, m_diag_syn
        call puts
        mov eax, [t_ip]
        call ip_poner                   ; la IP de quien llama
        mov si, m_fin_linea
        call puts
        jmp .x
.e2:
        cmp al, 2
        jne .e3
        mov si, m_diag_est
        call puts
        jmp .x
.e3:
        cmp al, 3
        jne .x
        mov si, m_diag_esc
        call puts
.x:
        popad
        ret
nored:
        mov si, m_necesita
        call puts
        mov al, 1
        jmp red_salir

; ---- una conexión ha llegado: código de acceso ----
conectada:
        ; quién llama y cuándo, al azar
        mov eax, [t_ip]
        call azar_mezcla_eax
        movzx eax, word [t_rpuerto]
        call azar_mezcla_eax
        call linea_cierra
        ; ¿en la pausa tras 3 intentos fallidos?
        cmp byte [bloqueado], 0
        je .nobloq
        mov si, bloq_ini
        call transcurrido
        cmp eax, [bloq_dur]
        jae .fin_bloq
        mov si, m_rechazada
        call puts
        mov eax, [t_ip]
        call ip_poner
        mov si, m_rechazada2
        call puts
        mov si, m_espere
        call manda_sz
        jmp sesion_fin
.fin_bloq:
        mov byte [bloqueado], 0
.nobloq:
        ; en la pantalla del equipo (el código ya está en ella)
        mov si, m_entrante
        call puts
        mov eax, [t_ip]
        call ip_poner
        mov si, m_entrante2
        call puts
        mov dword [cuenta_ult], -1
        ; al cliente: pedir el código
        mov byte [intentos], FALLOS_MAX
        mov si, m_pide
        call manda_sz
        jc sesion_fin
.pide:
        call con_recv_linea
        cmp al, 1
        je sesion_fin               ; conexión cerrada
        cmp al, 2
        je sesion_fin               ; Esc en el teclado del equipo
        call cuenta_atender         ; (si acaba de caducar, ya hay otro)
        call cmp_codigo
        cmp al, 1
        je concedido
        ; código incorrecto: se apunta en la pantalla y cuenta para ESTE código
        inc byte [fallos]
        call linea_cierra
        mov si, m_fallo
        call puts
        mov eax, [t_ip]
        call ip_poner
        mov si, m_fallo2
        call puts
        movzx eax, byte [fallos]
        call putnum
        mov si, m_fallo3
        call puts
        mov dword [cuenta_ult], -1
        call espera_mal             ; sin prisas para quien prueba
        cmp byte [fallos], FALLOS_MAX
        jae .quemado
        dec byte [intentos]
        jz .corta
        mov si, m_malcod
        call manda_sz
        jc sesion_fin
        jmp .pide
.quemado:
        ; 3 fallos con este código (en esta conexión o sumando las de antes):
        ; otro código en la pantalla y una pausa sin aceptar conexiones
        ; (la pausa: 30 s la primera vez y el doble cada vez que se vuelve a
        ; quemar un código sin que nadie entre, hasta 16 minutos)
        cmp byte [bloq_veces], 0
        je .pausa
        cmp dword [bloq_dur], PAUSA_MAX
        jae .pausa
        shl dword [bloq_dur], 1
.pausa:
        inc byte [bloq_veces]
        jnz .pausa1
        dec byte [bloq_veces]
.pausa1:
        call linea_cierra
        mov si, m_cod_fallos
        call puts
        mov eax, [bloq_dur]
        imul eax, eax, 10
        xor edx, edx
        mov ecx, 182
        div ecx
        call putnum
        mov si, m_cod_fallos2
        call nuevo_codigo
        call tic
        mov [bloq_ini], eax
        mov byte [bloqueado], 1
        mov si, m_demasiados
        call manda_sz
        jmp sesion_fin
.corta:
        mov si, m_malcod
        call manda_sz
        jmp sesion_fin

; ---- acceso concedido: consola de órdenes ----
concedido:
        mov byte [cod_valido], 0    ; un solo uso: este ya no vale
        mov byte [fallos], 0
        mov byte [bloq_veces], 0    ; la pausa vuelve a ser de 30 s
        mov dword [bloq_dur], PAUSA_TICS
        call linea_cierra
        mov si, m_conectado
        call puts
        mov eax, [t_ip]
        call ip_poner
        mov si, m_conectado2
        call puts
        mov si, m_concedido
        call manda_sz
        jc sesion_fin
.bucle:
        call manda_prompt
        jc sesion_fin
        call con_recv_linea
        cmp al, 1
        je sesion_fin
        cmp al, 2
        je sesion_fin
        call procesa_linea          ; AL: 0 sigue, 1 SALIR, 2 conexión caída
        cmp al, 0
        je .bucle
        jmp sesion_fin

; ---- fin de la sesión: cerrar y volver a escuchar ----
sesion_fin:
        call tcp_cerrar
        call linea_cierra
        mov si, m_cerrada
        call puts
        cmp byte [cod_valido], 0
        jne esperar
        mov si, m_cod_usado         ; el de esta sesión ya se usó: otro
        call nuevo_codigo
        jmp esperar

; ---- Esc sin sesión: salir del programa (suelta el puerto) ----
salir:
        call linea_cierra
        mov si, m_fin
        call puts
        xor al, al
        jmp red_salir

uso:
        mov si, m_uso
        call puts
        mov ax, 0x4C01
        int 0x21
ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  Recibir de la conexión
; =============================================================================
; Rutina que llama tcp_in cuando llegan datos en orden: DS:SI = datos,
; CX = octetos. Se guardan en un anillo (rx_buf) que vacía el bucle principal.
con_rx:
        pushad
.l:
        test cx, cx
        jz .done
        mov al, [si]
        inc si
        dec cx
        mov bx, [rx_w]
        lea di, [bx + 1]
        cmp di, RX_SZ
        jb .nw
        xor di, di
.nw:
        cmp di, [rx_r]
        je .l                       ; lleno: se descarta (líneas cortas: raro)
        mov [rx_buf + bx], al
        mov [rx_w], di
        jmp .l
.done:
        popad
        ret

; -> AL = un octeto del anillo; CF = 1 si está vacío
rx_getc:
        mov bx, [rx_r]
        cmp bx, [rx_w]
        je .vacio
        mov al, [rx_buf + bx]
        inc bx
        cmp bx, RX_SZ
        jb .nw
        xor bx, bx
.nw:
        mov [rx_r], bx
        clc
        ret
.vacio:
        stc
        ret

; ¿se ha pulsado Esc en el teclado del equipo (o llega por la consola serie)?
; Lectura no bloqueante por INT 21h 06h (teclado de la BIOS + consola serie).
; CF = 1 si Esc. Las demás teclas se tiran.
tecla_esc:
        push ax
        push dx
.l:
        mov ah, 0x06
        mov dl, 0xFF
        int 0x21
        jz .no
        cmp al, 27
        je .si
        jmp .l
.no:
        pop dx
        pop ax
        clc
        ret
.si:
        pop dx
        pop ax
        stc
        ret

; Leer una línea del cliente en 'linea' (ASCIIZ, sin CR/LF ni espacios al
; final). AL = 0 línea lista, 1 conexión cerrada, 2 Esc en el teclado.
con_recv_linea:
        mov word [linea_len], 0
.l:
        call red_sondear
        call azar_mezcla
        cmp byte [cod_valido], 0    ; antes de entrar: la cuenta atrás sigue
        je .nocta
        call cuenta_atender
.nocta:
        call rx_getc
        jnc .got
        cmp byte [t_fin], 0
        jne .cerr
        cmp byte [t_rst], 0
        jne .cerr
        call tecla_esc
        jc .esc
        jmp .l
.got:
        cmp al, 13
        je .l                       ; ignora CR
        cmp al, 10
        je .fin                     ; LF termina la línea
        mov bx, [linea_len]
        cmp bx, LINEA_MAX - 1
        jae .l                      ; descarta el exceso
        mov [linea + bx], al
        inc word [linea_len]
        jmp .l
.fin:
        mov bx, [linea_len]
.trim:
        test bx, bx
        jz .wr
        mov al, [linea + bx - 1]
        cmp al, ' '
        je .t1
        cmp al, 9
        je .t1
        jmp .wr
.t1:
        dec bx
        jmp .trim
.wr:
        mov [linea_len], bx
        mov byte [linea + bx], 0
        xor al, al
        ret
.cerr:
        mov al, 1
        ret
.esc:
        mov al, 2
        ret

; =============================================================================
;  Mandar por la conexión
; =============================================================================
; DS:SI = cadena ASCIIZ -> la manda por TCP. CF = 1 si la conexión falló.
manda_sz:
        push si
        push cx
        push di
        mov di, si
        xor cx, cx
.l:
        cmp byte [di], 0
        je .e
        inc di
        inc cx
        jmp .l
.e:
        jcxz .ok
        call tcp_enviar             ; DS:SI, CX
        pop di
        pop cx
        pop si
        ret
.ok:
        clc
        pop di
        pop cx
        pop si
        ret

; El indicador «X:\RUTA>» al cliente. CF = 1 si la conexión falló.
manda_prompt:
        call construye_ruta
        mov di, pbuf
.e:
        cmp byte [di], 0
        je .add
        inc di
        jmp .e
.add:
        mov byte [di], '>'
        mov byte [di + 1], 0
        mov si, pbuf
        call manda_sz
        ret

; pbuf = «X:\RUTA» (ASCIIZ) de la unidad y carpeta actuales
construye_ruta:
        mov ah, 0x19                ; unidad actual (0 = A)
        int 0x21
        add al, 'A'
        mov [pbuf], al
        mov byte [pbuf + 1], ':'
        mov byte [pbuf + 2], '\'
        mov ah, 0x19
        int 0x21
        mov dl, al
        inc dl                      ; 1 = A
        mov si, pbuf + 3
        mov ah, 0x47                ; carpeta actual -> DS:SI (sin barra inicial)
        int 0x21
        ret

; =============================================================================
;  Procesar una línea de órdenes
; =============================================================================
; 'linea' ya está sin CR/LF ni espacios al final.
; AL: 0 seguir, 1 cerrar la sesión (SALIR), 2 la conexión se cayó.
procesa_linea:
        mov si, linea
.sk:
        mov al, [si]
        cmp al, ' '
        je .sk1
        cmp al, 9
        je .sk1
        jmp .have
.sk1:
        inc si
        jmp .sk
.have:
        test al, al
        jnz .word
        xor al, al                  ; línea vacía: solo se reimprime el indicador
        ret
.word:
        mov di, palabra
        mov cx, 15
.cw:
        mov al, [si]
        test al, al
        jz .cwe
        cmp al, ' '
        je .cwe
        cmp al, 9
        je .cwe
        call upcase
        stosb
        inc si
        dec cx
        jnz .cw
.drena:
        mov al, [si]
        test al, al
        jz .cwe
        cmp al, ' '
        je .cwe
        cmp al, 9
        je .cwe
        inc si
        jmp .drena
.cwe:
        mov byte [di], 0
.ask:
        mov al, [si]
        cmp al, ' '
        je .ask1
        cmp al, 9
        je .ask1
        jmp .ahave
.ask1:
        inc si
        jmp .ask
.ahave:
        mov [arg_ptr], si
        ; ¿SALIR / EXIT?
        mov si, palabra
        mov di, s_salir
        call streq
        jc .salir
        mov si, palabra
        mov di, s_exit
        call streq
        jc .salir
        ; ¿CD / CHDIR?
        mov si, palabra
        mov di, s_cd
        call streq
        jc .cd
        mov si, palabra
        mov di, s_chdir
        call streq
        jc .cd
        ; ¿cambio de unidad «X:»?
        cmp byte [palabra + 2], 0
        jne .ejec
        cmp byte [palabra + 1], ':'
        jne .ejec
        call hace_unidad
        xor al, al
        ret
.cd:
        call hace_cd
        xor al, al
        ret
.ejec:
        call ejecuta_orden
        jc .caida
        xor al, al
        ret
.caida:
        mov al, 2
        ret
.salir:
        mov si, m_hasta
        call manda_sz
        mov al, 1
        ret

; CD/CHDIR: lo maneja CONSOLA (así el estado se mantiene entre órdenes)
hace_cd:
        mov si, [arg_ptr]
        cmp byte [si], 0
        je .mostrar
        mov dx, si
        mov ah, 0x3B                ; CHDIR DS:DX
        int 0x21
        jnc .r
        mov si, m_nodir
        call manda_sz
.r:
        ret
.mostrar:
        call construye_ruta
        mov si, pbuf
        call manda_sz
        mov si, s_crlf
        call manda_sz
        ret

; Cambio de unidad «X:»
hace_unidad:
        mov ah, 0x19
        int 0x21
        mov [uni_prev], al
        mov al, [palabra]
        call upcase
        sub al, 'A'
        jc .mal
        cmp al, 25
        ja .mal
        mov dl, al
        mov ah, 0x0E                ; unidad por omisión; AL = nº de unidades
        int 0x21
        cmp dl, al
        jb .r
        mov dl, [uni_prev]          ; inválida: dejar la de antes
        mov ah, 0x0E
        int 0x21
.mal:
        mov si, m_nounidad
        call manda_sz
.r:
        ret

; Ejecutar la orden de 'linea' con %COMSPEC% /C, su salida al archivo temporal,
; y mandar el archivo por TCP. CF = 1 si la conexión se cayó al mandarlo.
ejecuta_orden:
        mov di, ctail + 1
        mov eax, " /C "
        mov [di], eax
        add di, 4
        mov si, linea
.c:
        lodsb
        test al, al
        jz .ce
        stosb
        jmp .c
.ce:
        mov si, s_redir             ; « >X:\CONSOLA.OUT»
.cr:
        lodsb
        test al, al
        jz .cf
        stosb
        jmp .cr
.cf:
        mov byte [di], 13
        mov ax, di
        sub ax, ctail + 1
        mov [ctail], al             ; longitud de la cola
        mov word [pblk], 0          ; entorno: una copia del nuestro
        mov word [pblk + 2], ctail
        mov [pblk + 4], cs
        mov word [pblk + 6], fcb0
        mov [pblk + 8], cs
        mov word [pblk + 10], fcb0
        mov [pblk + 12], cs
        mov [sv_sp], sp
        mov dx, [comspec_p]
        mov bx, pblk
        mov ax, 0x4B00
        int 0x21
        cli
        mov bx, cs
        mov ss, bx
        mov sp, [cs:sv_sp]
        sti
        mov ds, bx
        mov es, bx
        cld
        jnc .leer
        mov si, m_noexec
        call manda_sz
        ret
.leer:
        call manda_archivo
        ret

; Mandar el archivo temporal por TCP y borrarlo. CF = 1 si la conexión se cayó.
manda_archivo:
        mov dx, s_outfile
        mov ax, 0x3D00              ; abrir para lectura
        int 0x21
        jc .nofile
        mov [out_h], ax
.l:
        mov bx, [out_h]
        mov cx, OBUF_SZ
        mov dx, obuf
        mov ah, 0x3F
        int 0x21
        jc .close
        test ax, ax
        jz .close
        mov cx, ax
        mov si, obuf
        call tcp_enviar
        jc .close_err
        jmp .l
.close:
        call .cerrar_borrar
        clc
        ret
.close_err:
        call .cerrar_borrar
        stc
        ret
.cerrar_borrar:
        mov bx, [out_h]
        mov ah, 0x3E
        int 0x21
        mov dx, s_outfile
        mov ah, 0x41
        int 0x21
        ret
.nofile:
        clc                         ; sin salida (orden sin resultado): nada
        ret

; =============================================================================
;  Código de acceso
; =============================================================================
; Desde que arranca CONSOLA hay siempre un código vigente en la pantalla del
; equipo (nunca se manda por la red). Se cambia por otro, y se enseña:
;   - cuando alguien entra con él (un solo uso; el nuevo sale al acabar),
;   - tras 3 intentos fallidos con él (en una conexión o sumando varias),
;   - cuando caduca (10 minutos, o /CADUCA:s), aunque no llegue nadie.
; La línea «Código de acceso: 123456   caduca en 9:59» se repinta en su sitio
; cada segundo (retorno de carro sin salto de línea); linea_cierra la termina
; antes de escribir cualquier otra cosa.

; SI = el porqué (texto) -> código nuevo, vigente, y en la pantalla
nuevo_codigo:
        pushad
        call linea_cierra
        call puts
        call gen_codigo
        call codigo_texto
        call tic
        mov [cod_ini], eax
        mov byte [cod_valido], 1
        mov byte [fallos], 0
        mov dword [cuenta_ult], -1
        call cuenta_atender
        popad
        ret

; Si el código ha caducado, otro; si no, repinta lo que le queda (solo cuando
; cambia el segundo). No hace nada sin código vigente (durante una sesión).
cuenta_atender:
        pushad
        cmp byte [cod_valido], 0
        je .r
        mov si, cod_ini
        call transcurrido
        cmp eax, [cod_dur]
        jb .vig
        mov si, m_cod_cad
        call nuevo_codigo
        jmp .r
.vig:
        mov ecx, [cod_dur]
        sub ecx, eax                ; tics que le quedan
        imul eax, ecx, 10
        add eax, 181
        xor edx, edx
        mov ebx, 182
        div ebx                     ; segundos (redondeando hacia arriba)
        cmp eax, [cuenta_ult]
        je .r
        mov [cuenta_ult], eax
        push eax
        mov si, m_linea1
        call puts
        mov si, cod_txt
        call puts
        mov si, m_linea2
        call puts
        pop eax
        xor edx, edx
        mov ebx, 60
        div ebx                     ; EAX = minutos, EDX = segundos
        push edx
        call putnum
        mov al, ':'
        call putc
        pop eax
        mov bl, 10
        div bl                      ; AL = decenas, AH = unidades
        add ax, '00'
        call putc
        mov al, ah
        call putc
        mov si, m_linea3
        call puts
        mov byte [viva], 1
.r:
        popad
        ret

; termina la línea del código si está a medio escribir
linea_cierra:
        cmp byte [viva], 0
        je .r
        mov byte [viva], 0
        call crlf
.r:
        ret

; SI -> dd con un tic de partida. EAX = tics desde entonces (pasando la
; medianoche, cuando el contador de la BIOS vuelve a cero).
transcurrido:
        call tic
        sub eax, [si]
        jnc .r
        add eax, TICS_DIA
.r:
        ret

; 2 s antes de contestar a un código erróneo (atendiendo a la red)
espera_mal:
        pushad
        call tic
        mov [espera_ini], eax
.l:
        call red_sondear
        call azar_mezcla
        mov si, espera_ini
        call transcurrido
        cmp eax, ESPERA_MAL
        jb .l
        popad
        ret

; ---- azar ----
; Un estado de 64 bits (azar_s0, azar_s1) al que se le va echando lo que no
; se puede adivinar desde fuera: el contador del temporizador (PIT, canal 0,
; que da 1,19 millones de vueltas por segundo), el reloj de la BIOS, el
; contador de ciclos del procesador (RDTSC, si lo tiene: Pentium o más), y en
; cada conexión la IP y el puerto de quien llama. Se mezcla en CADA vuelta de
; los bucles de espera (red, teclado), así que el momento exacto en que se
; pide un código depende de cuándo han llegado las tramas y las teclas. La
; mezcla es la de los finalizadores de MurmurHash3 (no es criptografía, pero
; no hay atajo: el código no sale del reloj sin más).
azar_ini:
        pushad
        mov dword [azar_s0], 0x6A09E667
        mov dword [azar_s1], 0xBB67AE85
        ; ¿CPUID? (si se puede cambiar el bit 21 de EFLAGS)
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd
        xor eax, ecx
        test eax, 0x200000
        jz .sin
        mov eax, 1
        db 0x0F, 0xA2               ; CPUID
        test dl, 0x10               ; TSC
        jz .sin
        mov byte [tsc_hay], 1
.sin:
        mov cx, 8
.m:
        call azar_mezcla
        loop .m
        popad
        ret

; azar_mezcla: echa al estado el PIT, el reloj y el TSC.
; azar_mezcla_eax: lo mismo, más EAX.
azar_mezcla:
        pushad
        xor eax, eax
        jmp short azar_comun
azar_mezcla_eax:
        pushad
azar_comun:
        mov ebx, eax
        pushf
        cli
        xor al, al
        out 0x43, al                ; PIT: congelar la cuenta del canal 0
        in al, 0x40
        mov cl, al
        in al, 0x40
        mov ch, al
        popf
        movzx ecx, cx
        xor ebx, ecx
        call tic
        rol eax, 16
        xor ebx, eax
        cmp byte [tsc_hay], 0
        je .m
        db 0x0F, 0x31               ; RDTSC -> EDX:EAX
        xor ebx, eax
        rol edx, 7
        xor ebx, edx
.m:
        mov eax, [azar_s0]
        add eax, ebx
        xor eax, [azar_s1]
        imul eax, eax, 0x85EBCA6B
        mov edx, eax
        shr edx, 13
        xor eax, edx
        imul eax, eax, 0xC2B2AE35
        mov edx, eax
        shr edx, 16
        xor eax, edx
        mov [azar_s0], eax
        mov edx, [azar_s1]
        rol edx, 5
        add edx, eax
        add edx, 0x9E3779B9
        mov [azar_s1], edx
        popad
        ret

; Un código de 6 cifras (100000..999999) en [codigo], distinto del anterior.
; Sin sesgo: de los 32 bits se descartan los valores por encima del último
; múltiplo entero de 900000 y se vuelve a sacar.
gen_codigo:
        pushad
.otra:
        call azar_mezcla
        call azar_mezcla
        mov eax, [azar_s0]
        mov edx, [azar_s1]
        rol edx, 16
        xor eax, edx
        imul eax, eax, 0x27D4EB2D
        mov edx, eax
        shr edx, 15
        xor eax, edx
        cmp eax, 4772 * 900000
        jae .otra
        xor edx, edx
        mov ecx, 900000
        div ecx                     ; EDX = resto 0..899999
        add edx, 100000
        cmp edx, [codigo]
        je .otra
        mov [codigo], edx
        popad
        ret

; [codigo] -> cod_txt (6 cifras ASCIIZ)
codigo_texto:
        push eax
        push ebx
        push ecx
        push edx
        push di
        mov eax, [codigo]
        mov di, cod_txt + 6
        mov byte [di], 0
        mov ebx, 10
        mov cx, 6
.l:
        xor edx, edx
        div ebx
        add dl, '0'
        dec di
        mov [di], dl
        loop .l
        pop di
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; -> AL = 1 si 'linea' es el código vigente. Sin atajos: mira siempre las 6
; cifras (no se para en la primera distinta) y sin código vigente no vale
; nada.
cmp_codigo:
        push bx
        xor ah, ah
        cmp word [linea_len], 6
        je .l6
        mov ah, 1
.l6:
        cmp byte [cod_valido], 0
        jne .v
        or ah, 1
.v:
        xor bx, bx
.c:
        mov al, [linea + bx]
        xor al, [cod_txt + bx]
        or ah, al
        inc bx
        cmp bx, 6
        jb .c
        xor al, al
        test ah, ah
        jnz .r
        mov al, 1
.r:
        pop bx
        ret

; DS:SI vs DS:DI (ASCIIZ) -> CF = 1 si iguales
streq:
        push si
        push di
.l:
        mov al, [si]
        cmp al, [di]
        jne .no
        test al, al
        jz .si
        inc si
        inc di
        jmp .l
.si:
        pop di
        pop si
        stc
        ret
.no:
        pop di
        pop si
        clc
        ret

; %COMSPEC% del entorno, o «X:\COMMAND.COM» de la unidad de arranque
find_comspec:
        mov word [comspec_p], comspec_def
        push es
        mov es, [0x2C]
        mov ax, es
        test ax, ax
        jz .x
        xor di, di
.l:
        cmp byte [es:di], 0
        je .x
        cmp dword [es:di], 0x534D4F43       ; "COMS"
        jne .n
        cmp dword [es:di + 4], 0x3D434550   ; "PEC="
        jne .n
        add di, 8
        mov si, comspec
        mov cx, 78
.c:
        mov al, [es:di]
        mov [si], al
        inc si
        inc di
        test al, al
        jz .ok
        loop .c
        jmp .x
.ok:
        mov word [comspec_p], comspec
        jmp .x
.n:
        inc di
        cmp byte [es:di - 1], 0
        jne .n
        cmp di, 0x7F00
        jb .l
.x:
        pop es
        ret

%include "util.inc"
%include "red.inc"

; =============================================================================
;  Textos
; =============================================================================
; en la pantalla del equipo
m_necesita  db "Cargue HSPRED y configure la red (CONFRED) antes de usar CONSOLA.", 13, 10, 0
m_banner1   db 13, 10, "Consola remota de Hispa-DOS ACTIVADA.", 13, 10
            db "  IP: ", 0
m_banner2   db "  Puerto: ", 0
m_banner3   db 13, 10
            db "  Abra ese puerto TCP en el router hacia este equipo para entrar desde fuera.", 13, 10
            db "Esperando conexión...  (Esc para cerrar)", 13, 10, 0
m_entrante  db "Conexión entrante desde ", 0
m_entrante2 db ": pide el código de acceso.", 13, 10, 0
m_cod_ini   db "Código de acceso para entrar (de un solo uso; dígaselo a quien se conecta):", 13, 10, 0
m_cod_usado db "El código anterior ya se ha usado. Para la próxima conexión:", 13, 10, 0
m_cod_cad   db "El código anterior ha caducado. El nuevo:", 13, 10, 0
m_cod_fallos db "(!) 3 intentos fallidos con el código anterior: se ha cambiado.", 13, 10
            db "    Durante ", 0
m_cod_fallos2 db " s no se aceptan conexiones. El nuevo:", 13, 10, 0
m_linea1    db 13, "  Código de acceso: ", 0
m_linea2    db "   caduca en ", 0
m_linea3    db "  ", 0
m_fallo     db "(!) Código erróneo desde ", 0
m_fallo2    db " (", 0
m_fallo3    db " de 3 con este código).", 13, 10, 0
m_rechazada db "Conexión de ", 0
m_rechazada2 db " rechazada: pausa tras 3 intentos fallidos.", 13, 10, 0
m_conectado db "CONECTADO desde ", 0
m_conectado2 db ". Esc para cerrar la sesión.", 13, 10, 0
m_cerrada   db "Sesión cerrada. Esperando conexión...  (Esc para cerrar)", 13, 10, 0
m_fin       db "CONSOLA cerrada. Puerto liberado.", 13, 10, 0
; diagnóstico del saludo TCP (en la pantalla del equipo)
m_diag_syn  db "[red] Ha llegado un SYN, contesto SYN+ACK, de ", 0
m_diag_est  db "[red] Conexión TCP establecida.", 13, 10, 0
m_diag_esc  db "[red] (vuelvo a escuchar: el saludo TCP no se completó)", 13, 10, 0
m_fin_linea db 13, 10, 0
; al cliente (por la conexión)
m_pide      db "Hispa-DOS CONSOLA. Introduzca el código de acceso: ", 0
m_malcod    db "Código no válido.", 13, 10, 0
m_demasiados db "Código no válido. Demasiados intentos: el código ha cambiado.", 13, 10, 0
m_espere    db "Demasiados intentos fallidos. Espere y vuelva a probar más tarde.", 13, 10, 0
m_concedido db 13, 10, "Acceso concedido. Escriba SALIR para terminar.", 13, 10, 0
m_hasta     db "Hasta luego.", 13, 10, 0
m_nodir     db "Ruta no encontrada.", 13, 10, 0
m_nounidad  db "Unidad no válida.", 13, 10, 0
m_noexec    db "No se puede ejecutar la orden (no se encuentra COMMAND.COM).", 13, 10, 0
s_crlf      db 13, 10, 0
; comparaciones y rutas
s_salir     db "SALIR", 0
s_exit      db "EXIT", 0
s_cd        db "CD", 0
s_chdir     db "CHDIR", 0
s_redir     db " >A:\CONSOLA.OUT", 0   ; la «A» se cambia por la unidad de arranque
s_outfile   db "A:\CONSOLA.OUT", 0     ; idem
comspec_def db "A:\COMMAND.COM", 0     ; idem (si no hay %COMSPEC%)

m_uso       db "Consola remota de Hispa-DOS: deja que alguien entre por la red a este equipo.", 13, 10
            db 13, 10
            db "CONSOLA [/PUERTO:n] [/CADUCA:s]", 13, 10
            db 13, 10
            db "  /PUERTO:n   puerto TCP donde escuchar (2323 si no se dice)", 13, 10
            db "  /CADUCA:s   segundos que vale cada código (600 = 10 minutos si no se dice)", 13, 10
            db 13, 10
            db "Enseña en ESTA pantalla un código de 6 cifras que hay que decirle a quien", 13, 10
            db "se conecta; sin él no se entra. Es de un solo uso, cambia tras 3 intentos", 13, 10
            db "fallidos y caduca solo (en la pantalla se ve lo que le queda). Dentro,", 13, 10
            db "se ejecutan órdenes (DIR, VER, TYPE...) y se ve su salida. Los programas de", 13, 10
            db "pantalla completa (EDITAR, menús) no van por ahora. SALIR cierra la sesión;", 13, 10
            db "Esc en el teclado del equipo cierra la sesión (o CONSOLA si no hay sesión).", 13, 10
            db "Para entrar desde fuera de la red, abra ese puerto en el router. Necesita", 13, 10
            db "HSPRED y la red configurada (ver CONFRED). Es software abierto y lo manda el", 13, 10
            db "usuario: no arranca solo ni se instala por omisión.", 13, 10, 0

        section .bss
puerto      resw 1
intentos    resb 1
fallos      resb 1                  ; intentos fallidos con el código vigente
cod_valido  resb 1                  ; 0: no hay código (se acaba de usar)
viva        resb 1                  ; la línea del código está a medio escribir
bloqueado   resb 1                  ; pausa tras 3 fallos
tsc_hay     resb 1                  ; el procesador tiene RDTSC
bloq_ini    resd 1
bloq_dur    resd 1                  ; tics de la pausa
bloq_veces  resb 1                  ; códigos quemados seguidos
espera_ini  resd 1
cod_ini     resd 1                  ; tic en que se generó el código
cod_dur     resd 1                  ; tics que vale
cuenta_ult  resd 1                  ; los segundos que se enseñan ahora
azar_s0     resd 1
azar_s1     resd 1
est_ant     resb 1                  ; (diag) estado TCP anterior, para ver los cambios
uni_prev    resb 1
codigo      resd 1
cod_txt     resb 8
arg_ptr     resw 1
linea_len   resw 1
linea       resb LINEA_MAX
palabra     resb 16
pbuf        resb 80
rx_r        resw 1
rx_w        resw 1
rx_buf      resb RX_SZ
out_h       resw 1
sv_sp       resw 1
comspec_p   resw 1
comspec     resb 80
ctail       resb 300
fcb0        resb 40
pblk        resb 14
obuf        resb OBUF_SZ + 4
u_numbuf    resb 16
u_lbuf      resb 84
bss_fin:
            alignb 16
            resb 2048
pila_tope:
prog_end:
