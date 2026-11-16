; =============================================================================
;  Hispa-DOS ACTUALIZAR (en el disco ACTUALIZ.COM) - instalador por red
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  ACTUALIZAR [http://servidor[:puerto]/ruta]
;  Mira por la red (HTTP, sin cifrar) si hay una versión más nueva o una edición
;  más rica de Hispa-DOS que la instalada, y, si el equipo la aguanta y el
;  usuario quiere, la DESCARGA y la INSTALA en el sitio (conservando los datos y
;  ajustes del usuario). Sin argumentos usa el flujo nuevo; si se le da una
;  dirección, baja de ahí la lista de versiones.
;
;  El servidor da un archivo de texto (versiones.txt) con una o varias
;  versiones/ediciones y sus requisitos (ver docs/RED.md):
;      [0.9.16]
;      cpu=386            ; nivel mínimo de CPU: 386 / 486 / 586 (Pentium)
;      ram=0              ; KiB mínimos de memoria extendida (0 = no exige)
;      disco=4            ; MiB libres mínimos en el disco de destino
;      nivel=0            ; "riqueza" de la edición (mayor = más completa)
;      edicion=Hispa-386 (con Hispa Ventanas)
;      paquete=http://actualiza.clicgest.es/hispados-0.9.16.paq
;      nota=Novedades...
;  También entiende el formato viejo (solo «version=X»): lo trata como una única
;  versión sin requisitos y sin paquete (solo avisa, como antes).
;
;  La dirección sale, por este orden:
;    1. la que se escriba en la línea de órdenes (si se escribe),
;    2. la clave ACTUALIZAR= de \HISPADOS\RED.INI (si existe),
;    3. la de por omisión compilada (ver s_url_def).
;
;  HTTPS (cifrado) todavía no: se rechaza con un mensaje claro. GitHub es todo
;  HTTPS, así que el archivo y los paquetes tienen que estar en un servidor
;  propio por HTTP. Necesita HSPRED y la red configurada (ver docs/RED.md).
;
;  SEGURIDAD: primero baja el paquete y lo desempaqueta DEL TODO en una carpeta
;  temporal (C:\ACTUALIZA) y solo si todo está bien aplica los ficheros en el
;  sitio; si falla la descarga o el desempaquetado, aborta sin tocar nada.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

%include "version.inc"

HDR_MAX      equ 2048
BODY_MAX     equ 4096
INI_MAX      equ 2048

MAX_VER      equ 8            ; secciones (versiones/ediciones) como mucho
V_STRSZ      equ 16           ; "mayor.menor.parche"
V_EDISZ      equ 40           ; nombre de la edición
V_PAQSZ      equ 160          ; URL del paquete (o del enlace de descarga viejo)
V_NOTSZ      equ 128          ; nota
PAQ_NAME_MAX equ 80           ; longitud de un nombre dentro del paquete
ARB_MAX      equ 6            ; profundidad máxima al borrar el temporal

start:
        cld
        mov sp, pila_tope
        mov di, red_bss
        mov cx, bss_fin - red_bss
        xor al, al
        rep stosb
        ; encoger el bloque de memoria: por si hay que lanzar HSPRED (EXEC)
        mov bx, prog_end + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov word [t_rutina], act_datos
        ; ---- detectar el equipo (antes de la red) ----
        call det_cpu
        mov [det_cpu_lvl], al
        call det_ram
        mov [det_ram_kib], eax
        mov ax, 0x3305
        int 0x21                    ; DL = unidad de arranque (1 = A)
        mov [boot_num], dl
        add dl, 'A' - 1
        mov [boot_letter], dl
        mov dl, [boot_num]
        call det_disco
        mov [det_free_mib], eax
        ; ---- la dirección: ¿viene en la línea de órdenes? ----
        mov si, 0x81
        call blancos
        cmp word [si], '/?'
        je ayuda
        cmp byte [si], 13
        je .sin_arg
        mov di, url
        call palabra
        mov byte [url_dado], 1
.sin_arg:
        cmp byte [url_dado], 0
        jne .con_url
        call ini_actualizar
.con_url:
        cmp byte [url_dado], 0
        jne .ya
        mov si, s_url_def
        mov di, url
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
.ya:
        ; ---- la red ----
        call red_abrir
        jnc .red_ok
        cmp si, m_nopkt
        jne no_red
        call carga_hspred
        call red_abrir
        jc no_red
.red_ok:
        xor al, al
        call red_config
        jc no_red
        cmp dword [my_ip], 0
        je no_red
        ; ---- bajar la lista de versiones ----
        mov si, m_mirando
        call puts
        mov byte [dl_modo], 0
        call http_get
        jc error
        mov byte [body_buf + BODY_MAX - 1], 0
        call parse_versiones
        call decide
        call mostrar_veredicto      ; termina con red_salir salvo si instala
        ; mostrar_veredicto solo vuelve aquí cuando hay que instalar
        call do_install
        jc .inst_fallo
        xor al, al
        jmp red_salir
.inst_fallo:
        mov al, 1
        jmp red_salir

no_red:
        mov si, m_sinred
        call puts
        call crlf
        mov al, 4
        jmp red_salir

error:
        call puts
        call crlf
        mov al, 1
        jmp red_salir

ayuda:
        mov si, m_uso
        call puts
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  La dirección de RED.INI (clave ACTUALIZAR=)
; =============================================================================
ini_actualizar:
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [ini_ruta], dl
        mov dx, ini_ruta
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov dx, ini_buf
        mov cx, INI_MAX - 1
        mov ah, 0x3F
        int 0x21
        pushf
        push ax
        mov ah, 0x3E
        int 0x21
        pop ax
        popf
        jc .r
        mov si, ini_buf
        add si, ax
        mov byte [si], 0
        mov si, ini_buf
.linea:
.bl:
        mov al, [si]
        cmp al, ' '
        je .bi
        cmp al, 9
        je .bi
        cmp al, 13
        je .bi
        cmp al, 10
        je .bi
        jmp .nobl
.bi:
        inc si
        jmp .bl
.nobl:
        test al, al
        jz .r
        cmp al, ';'
        je .salta
        cmp al, '#'
        je .salta
        cmp al, '['
        je .salta
        mov di, i_clave
        mov cx, 15
.k:
        mov al, [si]
        test al, al
        jz .r
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, '='
        je .ke
        inc si
        cmp al, ' '
        je .k
        cmp al, 9
        je .k
        call upcase
        stosb
        loop .k
        jmp .salta
.ke:
        mov byte [di], 0
        inc si
        mov di, i_clave
        mov bp, s_clave_act
.cmp:
        mov al, [bp]
        cmp al, [di]
        jne .salta
        test al, al
        jz .match
        inc di
        inc bp
        jmp .cmp
.match:
.sp:
        mov al, [si]
        cmp al, ' '
        je .spi
        cmp al, 9
        jne .copia
.spi:
        inc si
        jmp .sp
.copia:
        mov di, url
        mov cx, 250
.c:
        mov al, [si]
        cmp al, ' '
        jbe .ce
        inc si
        stosb
        loop .c
.ce:
        mov byte [di], 0
        cmp di, url
        je .salta
        mov byte [url_dado], 1
        ret
.salta:
        mov al, [si]
        test al, al
        jz .r
        inc si
        cmp al, 10
        jne .salta
        jmp .linea
.r:
        ret

; =============================================================================
;  Leer versiones.txt (en body_buf, acaba en 0) a las tablas de secciones.
;  Entiende secciones [x.y.z] con claves cpu/ram/disco/nivel/edicion/paquete/
;  nota, y el formato viejo (version=/descarga=/nota= sin sección).
; =============================================================================
parse_versiones:
        mov byte [nver], 0
        mov word [cur_sec], 0xFFFF
        mov si, body_buf
.linea:
.bl:
        mov al, [si]
        cmp al, ' '
        je .bi
        cmp al, 9
        je .bi
        jmp .nobl
.bi:
        inc si
        jmp .bl
.nobl:
        test al, al
        jz .fin
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, ';'
        je .salta
        cmp al, '#'
        je .salta
        cmp al, '['
        je .seccion
        ; clave = valor
        mov di, p_clave
        mov cx, 14
.k:
        mov al, [si]
        test al, al
        jz .fin
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        cmp al, '='
        je .ke
        inc si
        call upcase
        stosb
        loop .k
        jmp .salta
.ke:
        mov byte [di], 0
        inc si
.spv:
        mov al, [si]
        cmp al, ' '
        je .spvi
        cmp al, 9
        jne .val
.spvi:
        inc si
        jmp .spv
.val:
        mov di, p_valor
        mov cx, 200
.vl:
        mov al, [si]
        test al, al
        jz .vend
        cmp al, 13
        je .vend
        cmp al, 10
        je .vend
        inc si
        stosb
        loop .vl
.vend:
        mov byte [di], 0
        mov [p_si], si
        call recorta
        call aplica_clave
        mov si, [p_si]
        jmp .salta
.seccion:
        inc si                      ; saltar '['
        mov di, p_valor
        mov cx, V_STRSZ - 1
.sl:
        mov al, [si]
        test al, al
        jz .sdone
        cmp al, 13
        je .sdone
        cmp al, 10
        je .sdone
        cmp al, ']'
        je .sdone
        inc si
        stosb
        loop .sl
.sdone:
        mov byte [di], 0
        mov [p_si], si              ; recordar la posición en el cuerpo
        call nueva_seccion          ; (calc_num mueve SI: hay que restaurarlo)
        mov si, [p_si]
        jmp .salta
.salta:
        mov al, [si]
        test al, al
        jz .fin
        inc si
        cmp al, 10
        jne .salta
        jmp .linea
.fin:
        ret

; crear una sección desde p_valor (la cadena de versión)
nueva_seccion:
        mov al, [nver]
        cmp al, MAX_VER
        jae .lleno
        movzx ax, al
        call sel_slot
        ; copiar la versión al hueco
        mov si, p_valor
        mov di, [sp_str]
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        ; número
        mov si, p_valor
        call calc_num
        jc .mal
        mov bx, [cur_i]
        shl bx, 2
        mov [v_num + bx], eax
        ; requisitos y textos a cero/vacío
        mov bx, [cur_i]
        mov byte [v_cpu + bx], 0
        mov byte [v_haspaq + bx], 0
        mov byte [v_cumple + bx], 0
        mov bx, [cur_i]
        shl bx, 1
        mov word [v_disco + bx], 0
        mov word [v_nivel + bx], 0
        mov bx, [cur_i]
        shl bx, 2
        mov dword [v_ram + bx], 0
        mov di, [sp_edi]
        mov byte [di], 0
        mov di, [sp_paq]
        mov byte [di], 0
        mov di, [sp_not]
        mov byte [di], 0
        mov ax, [cur_i]
        mov [cur_sec], ax
        inc byte [nver]
        ret
.mal:
        mov word [cur_sec], 0xFFFF   ; versión inválida: ignorar sus claves
        ret
.lleno:
        ret

; interpretar p_clave=p_valor en la sección en curso (o el formato viejo)
aplica_clave:
        cmp word [cur_sec], 0xFFFF
        jne .ensec
        ; sin sección: solo «version=» (formato viejo)
        mov di, p_clave
        mov bp, k_version
        call kcmp
        jc .r
        call nueva_seccion
        ret
.ensec:
        mov ax, [cur_sec]
        call sel_slot
        mov di, p_clave
        mov bp, k_cpu
        call kcmp
        jnc .cpu
        mov di, p_clave
        mov bp, k_ram
        call kcmp
        jnc .ram
        mov di, p_clave
        mov bp, k_disco
        call kcmp
        jnc .disco
        mov di, p_clave
        mov bp, k_nivel
        call kcmp
        jnc .nivel
        mov di, p_clave
        mov bp, k_paquete
        call kcmp
        jnc .paquete
        mov di, p_clave
        mov bp, k_edicion
        call kcmp
        jnc .edicion
        mov di, p_clave
        mov bp, k_nota
        call kcmp
        jnc .nota
        mov di, p_clave
        mov bp, k_descarga
        call kcmp
        jnc .descarga
.r:
        ret
.cpu:
        mov si, p_valor
        call parse_cpu
        mov bx, [cur_i]
        mov [v_cpu + bx], al
        ret
.ram:
        mov si, p_valor
        call parse_num
        jc .r
        mov bx, [cur_i]
        shl bx, 2
        mov [v_ram + bx], eax
        ret
.disco:
        mov si, p_valor
        call parse_num
        jc .r
        mov bx, [cur_i]
        shl bx, 1
        mov [v_disco + bx], ax
        ret
.nivel:
        mov si, p_valor
        call parse_num
        jc .r
        mov bx, [cur_i]
        shl bx, 1
        mov [v_nivel + bx], ax
        ret
.paquete:
        mov si, p_valor
        mov di, [sp_paq]
        mov cx, V_PAQSZ
        call cp_lim
        mov bx, [cur_i]
        mov byte [v_haspaq + bx], 1
        ret
.edicion:
        mov si, p_valor
        mov di, [sp_edi]
        mov cx, V_EDISZ
        call cp_lim
        ret
.nota:
        mov si, p_valor
        mov di, [sp_not]
        mov cx, V_NOTSZ
        call cp_lim
        ret
.descarga:
        ; formato viejo: solo para enseñar; no es un paquete instalable
        mov bx, [cur_i]
        cmp byte [v_haspaq + bx], 0
        jne .r
        mov si, p_valor
        mov di, [sp_paq]
        mov cx, V_PAQSZ
        call cp_lim
        ret

; AX = índice de sección -> punteros a sus cadenas en sp_str/sp_edi/sp_paq/sp_not
sel_slot:
        mov [cur_i], ax
        mov bx, ax
        imul bx, V_STRSZ
        add bx, v_str
        mov [sp_str], bx
        mov bx, ax
        imul bx, V_EDISZ
        add bx, v_edi
        mov [sp_edi], bx
        mov bx, ax
        imul bx, V_PAQSZ
        add bx, v_paq
        mov [sp_paq], bx
        mov bx, ax
        imul bx, V_NOTSZ
        add bx, v_not
        mov [sp_not], bx
        ret

; SI = valor de cpu= -> AL = nivel (3/4/5), 0 = sin requisito
parse_cpu:
        call parse_num
        jc .cero
        cmp eax, 586
        jae .c5
        cmp eax, 486
        jae .c4
        cmp eax, 386
        jae .c3
        cmp eax, 5
        je .c5
        cmp eax, 4
        je .c4
        cmp eax, 3
        je .c3
.cero:
        xor al, al
        ret
.c5:
        mov al, 5
        ret
.c4:
        mov al, 4
        ret
.c3:
        mov al, 3
        ret

; copiar ASCIIZ SI->DI, como mucho CX-1 bytes, y poner el cero
cp_lim:
        dec cx
.l:
        jcxz .e
        lodsb
        test al, al
        jz .e
        stosb
        dec cx
        jmp .l
.e:
        mov byte [di], 0
        ret

; copiar ASCIIZ SI->DI sin límite (para la url)
cp_asciiz:
        lodsb
        stosb
        test al, al
        jnz cp_asciiz
        ret

; recorta espacios y tabuladores del final de p_valor
recorta:
        mov di, p_valor
.f:
        cmp byte [di], 0
        je .fin
        inc di
        jmp .f
.fin:
        cmp di, p_valor
        je .r
        dec di
        cmp byte [di], ' '
        je .q
        cmp byte [di], 9
        je .q
        ret
.q:
        mov byte [di], 0
        jmp .fin
.r:
        ret

; DI = clave leída (mayúsculas, 0), BP = clave buscada; CF=0 si iguales
kcmp:
        push di
        push bp
.l:
        mov al, [bp]
        cmp al, [di]
        jne .no
        test al, al
        jz .si
        inc di
        inc bp
        jmp .l
.si:
        pop bp
        pop di
        clc
        ret
.no:
        pop bp
        pop di
        stc
        ret

; SI = «mayor.menor[.parche]» -> EAX = mayor*10000 + menor*100 + parche; CF si no
calc_num:
        push ebx
        call parse_num
        jc .mal
        imul ebx, eax, 10000
        cmp byte [si], '.'
        jne .mal
        inc si
        call parse_num
        jc .mal
        imul eax, eax, 100
        add ebx, eax
        cmp byte [si], '.'
        jne .ok
        inc si
        call parse_num
        jc .ok
        add ebx, eax
.ok:
        mov eax, ebx
        pop ebx
        clc
        ret
.mal:
        pop ebx
        stc
        ret

; =============================================================================
;  Decidir: calcular qué cumple el equipo y cuál recomendar
; =============================================================================
decide:
        movzx cx, byte [nver]
        test cx, cx
        jz .vacio
        xor bx, bx
.cl:
        ; cpu
        mov al, [v_cpu + bx]
        test al, al
        jz .cpu_ok
        cmp al, [det_cpu_lvl]
        ja .no
.cpu_ok:
        mov di, bx
        shl di, 2
        mov eax, [v_ram + di]
        cmp eax, [det_ram_kib]
        ja .no
        mov di, bx
        shl di, 1
        movzx eax, word [v_disco + di]
        cmp eax, [det_free_mib]
        ja .no
        mov byte [v_cumple + bx], 1
        jmp .cn
.no:
        mov byte [v_cumple + bx], 0
.cn:
        inc bx
        cmp bx, cx
        jb .cl
        ; buscar los mejores
        mov word [best_inst], 0xFFFF
        mov word [best_notify], 0xFFFF
        mov word [best_reach], 0xFFFF
        mov word [best_unreach], 0xFFFF
        mov byte [any_better], 0
        xor bx, bx
.fl:
        cmp bx, cx
        jae .done
        call es_mejora              ; AL = 1 si mejora lo instalado
        mov dl, al
        test al, al
        jz .nm
        mov byte [any_better], 1
.nm:
        mov al, [v_cumple + bx]
        test al, al
        jz .nocumple
        ; best_reach = mejor por (nivel,num) de los que cumple
        mov [mn_a], bx
        mov ax, [best_reach]
        mov [mn_b], ax
        call mejor_nn
        test al, al
        jz .r1
        mov [best_reach], bx
.r1:
        test dl, dl
        jz .next
        ; best_notify
        mov [mn_a], bx
        mov ax, [best_notify]
        mov [mn_b], ax
        call mejor_nn
        test al, al
        jz .r2
        mov [best_notify], bx
.r2:
        mov al, [v_haspaq + bx]
        test al, al
        jz .next
        mov [mn_a], bx
        mov ax, [best_inst]
        mov [mn_b], ax
        call mejor_nn
        test al, al
        jz .next
        mov [best_inst], bx
        jmp .next
.nocumple:
        test dl, dl
        jz .next
        ; best_unreach = el de MENOR rango (la "siguiente" que no alcanza)
        mov ax, [best_unreach]
        cmp ax, 0xFFFF
        je .setu
        mov [mn_a], ax              ; ¿el actual es de mayor rango que el nuevo?
        mov [mn_b], bx
        call mejor_nn
        test al, al
        jz .next                    ; no: el nuevo no es menor, se queda el actual
.setu:
        mov [best_unreach], bx
.next:
        inc bx
        jmp .fl
.done:
        ret
.vacio:
        ret

; BX = índice -> AL = 1 si v_num>VER_NUM o v_nivel>VER_EDI (mejora lo instalado)
es_mejora:
        push bx
        push ecx
        push edx
        mov cx, bx
        shl bx, 2
        mov edx, [v_num + bx]
        cmp edx, VER_NUM
        ja .si
        mov bx, cx
        shl bx, 1
        movzx edx, word [v_nivel + bx]
        cmp edx, VER_EDI
        ja .si
        xor al, al
        jmp .r
.si:
        mov al, 1
.r:
        pop edx
        pop ecx
        pop bx
        ret

; [mn_a] = candidato, [mn_b] = mejor actual -> AL = 1 si el candidato es mejor
; por (nivel desc, num desc), o si [mn_b] = 0xFFFF (ninguno aún)
mejor_nn:
        cmp word [mn_b], 0xFFFF
        je .si
        push eax
        push ebx
        push ecx
        push edx
        mov bx, [mn_a]
        shl bx, 1
        mov cx, [v_nivel + bx]
        mov bx, [mn_b]
        shl bx, 1
        mov dx, [v_nivel + bx]
        cmp cx, dx
        ja .si2
        jb .no2
        mov bx, [mn_a]
        shl bx, 2
        mov eax, [v_num + bx]
        mov bx, [mn_b]
        shl bx, 2
        cmp eax, [v_num + bx]
        ja .si2
.no2:
        pop edx
        pop ecx
        pop ebx
        pop eax
        xor al, al
        ret
.si2:
        pop edx
        pop ecx
        pop ebx
        pop eax
.si:
        mov al, 1
        ret

; =============================================================================
;  Enseñar el veredicto. Si hay que instalar, VUELVE al llamante; en los demás
;  casos termina con red_salir.
; =============================================================================
mostrar_veredicto:
        call imprime_equipo
        mov si, m_instalada
        call puts
        mov si, s_verstr
        call puts
        mov si, m_inst2
        call puts
        cmp byte [nver], 0
        je .sinver
        cmp word [best_inst], 0xFFFF
        jne .inst
        cmp word [best_notify], 0xFFFF
        jne .notify
        cmp byte [any_better], 0
        jne .limite
        mov si, m_aldia
        call puts
        xor al, al
        jmp red_salir
.sinver:
        mov si, m_sinver
        call puts
        mov al, 1
        jmp red_salir
.inst:
        mov ax, [best_inst]
        call sel_slot
        mov si, m_recom
        call puts
        mov si, [sp_edi]
        cmp byte [si], 0
        jne .re_e
        mov si, m_estaver
        call puts
        jmp .re_v
.re_e:
        call puts
.re_v:
        mov si, m_recom2
        call puts
        mov si, [sp_str]
        call puts
        mov si, m_punto
        call puts
        mov si, [sp_not]
        cmp byte [si], 0
        je .ri
        call puts
        call crlf
.ri:
        mov si, m_pregunta
        call puts
        mov si, [sp_str]
        call puts
        mov si, m_pregunta2
        call puts
        call ask_yes
        jc .no_inst
        ret                         ; -> el llamante hace do_install
.no_inst:
        mov si, m_solomirar
        call puts
        xor al, al
        jmp red_salir
.notify:
        mov ax, [best_notify]
        call sel_slot
        mov si, m_nueva
        call puts
        mov si, [sp_str]
        call puts
        mov si, m_punto
        call puts
        mov si, [sp_not]
        cmp byte [si], 0
        je .nn
        call puts
        call crlf
.nn:
        mov si, [sp_paq]
        cmp byte [si], 0
        je .nd
        mov si, m_desc
        call puts
        mov si, [sp_paq]
        call puts
        call crlf
.nd:
        mov si, m_comoact
        call puts
        xor al, al
        jmp red_salir
.limite:
        cmp word [best_reach], 0xFFFF
        je .l2
        mov ax, [best_reach]
        call sel_slot
        mov si, m_llega
        call puts
        mov si, [sp_str]
        call puts
        mov si, [sp_edi]
        cmp byte [si], 0
        je .l1b
        mov si, m_edi_ab
        call puts
        mov si, [sp_edi]
        call puts
        mov si, m_cierra
        call puts
.l1b:
        mov si, m_punto
        call puts
.l2:
        cmp word [best_unreach], 0xFFFF
        je .lfin
        mov ax, [best_unreach]
        call sel_slot
        mov si, [sp_edi]
        cmp byte [si], 0
        je .ue_ver
        mov si, m_laedi
        call puts
        mov si, [sp_edi]
        call puts
        mov si, m_parsp
        call puts
        mov si, [sp_str]
        call puts
        mov si, m_cierra
        call puts
        jmp .ue_req
.ue_ver:
        mov si, m_laversion
        call puts
        mov si, [sp_str]
        call puts
.ue_req:
        mov si, m_necreq
        call puts
        call imprime_requisitos
        mov si, m_punto2
        call puts
.lfin:
        xor al, al
        jmp red_salir

; "Tu equipo: 486, 32 MB de memoria, 180 MB libres en C:."
imprime_equipo:
        mov si, m_tuequipo
        call puts
        mov al, [det_cpu_lvl]
        cmp al, 5
        jae .c5
        cmp al, 4
        je .c4
        mov si, m_cpu386
        jmp .cp
.c4:
        mov si, m_cpu486
        jmp .cp
.c5:
        mov si, m_cpu586
.cp:
        call puts
        mov si, m_coma
        call puts
        mov eax, [det_ram_kib]
        add eax, 512
        shr eax, 10
        call putnum
        mov si, m_mbmem
        call puts
        mov eax, [det_free_mib]
        call putnum
        mov si, m_mblibre
        call puts
        mov al, [boot_letter]
        call putc
        mov si, m_dp
        call puts
        ret

; requisitos de la sección seleccionada (cur_i): "un 486 y 4 MB de memoria..."
imprime_requisitos:
        mov byte [req_prev], 0
        mov bx, [cur_i]
        mov al, [v_cpu + bx]
        test al, al
        jz .ram
        mov si, m_un
        call puts
        cmp al, 5
        jae .q5
        cmp al, 4
        je .q4
        mov si, m_cpu386
        jmp .qp
.q4:
        mov si, m_cpu486
        jmp .qp
.q5:
        mov si, m_cpu586
.qp:
        call puts
        mov byte [req_prev], 1
.ram:
        mov bx, [cur_i]
        shl bx, 2
        mov eax, [v_ram + bx]
        test eax, eax
        jz .disco
        call .ycond
        add eax, 512
        shr eax, 10
        call putnum
        mov si, m_mbmem2
        call puts
        mov byte [req_prev], 1
.disco:
        mov bx, [cur_i]
        shl bx, 1
        movzx eax, word [v_disco + bx]
        test eax, eax
        jz .r
        call .ycond
        call putnum
        mov si, m_mbdisco
        call puts
        mov byte [req_prev], 1
.r:
        ret
.ycond:
        cmp byte [req_prev], 0
        je .yr
        mov si, m_y
        call puts
.yr:
        ret

; =============================================================================
;  Instalar la versión recomendada (best_inst): bajar, desempaquetar, aplicar
; =============================================================================
do_install:
        mov al, [boot_letter]
        mov [tpaq_d], al
        mov [tdir_d], al
        mov ax, [best_inst]
        call sel_slot
        mov si, [sp_paq]
        mov di, url
        call cp_asciiz
        ; limpiar cualquier temporal anterior y crear la carpeta
        call limpia_temp
        mov dx, tdir_path
        mov ah, 0x39
        int 0x21
        ; bajar el paquete
        mov si, m_descpaq
        call puts
        mov byte [dl_modo], 1
        mov si, tpaq_path
        mov di, archivo
        call cp_asciiz
        call http_get
        jc .falla_desc
        ; desempaquetar
        mov si, m_desempaq
        call puts
        call unpack_paq
        jc .falla_paq
        ; aplicar en el sitio
        mov si, m_aplicando
        call puts
        call apply_update
        call limpia_temp
        mov si, m_actok
        call puts
        mov ax, [best_inst]
        call sel_slot
        mov si, [sp_str]
        call puts
        mov si, m_actok2
        call puts
        clc
        ret
.falla_desc:
        push si
        call limpia_temp
        pop si
        call puts
        call crlf
        mov si, m_abortado
        call puts
        stc
        ret
.falla_paq:
        call limpia_temp
        mov si, m_paqmal
        call puts
        mov si, m_abortado
        call puts
        stc
        ret

; borrar el temporal: el .PAQ y el árbol \ACTUALIZA
limpia_temp:
        mov dx, tpaq_path
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov dx, tpaq_path
        mov ah, 0x41
        int 0x21
        mov si, tdir_path
        call borra_arbol
        ret

; aplicar \ACTUALIZA sobre la unidad de arranque (motor de copiaarbol.inc)
apply_update:
        mov al, [boot_letter]
        mov [src], al
        mov [dst], al
        mov si, s_actpre
        mov di, src_pre
        call cp_asciiz
        mov word [ncopied], 0
        mov word [nfailed], 0
        mov word [nkept], 0
        call backup_cfg
        mov byte [keepdata], 1
        mov byte [keep], 0
        mov byte [fix], 0
        mov si, s_hsp
        mov di, s_hdir
        call make_dir
        call copy_set
        mov si, s_hspj
        mov di, s_hjdir
        call make_dir
        call copy_set
        mov si, s_hspn
        mov di, s_hndir
        call make_dir
        call copy_set
        mov si, s_hsps
        mov di, s_hsdir
        call make_dir
        call copy_set
        mov si, s_hspp
        mov di, s_hpdir
        call make_dir
        call copy_set
        mov byte [keepdata], 0
        mov byte [fix], 0
        mov si, s_kernel
        mov di, s_root
        call copy_set
        mov si, s_command
        mov di, s_root
        call copy_set
        mov si, s_leeme
        mov di, s_root
        call copy_set
        mov byte [fix], 1
        mov si, s_config
        mov di, s_root
        call copy_set
        mov si, s_autoexec
        mov di, s_root
        call copy_set
        mov byte [fix], 0
        ret

; guardar CONFIG.SYS y AUTOEXEC.BAT del usuario como .ANT (unidad de arranque)
backup_cfg:
        mov al, [boot_letter]
        mov [bk_cs], al
        mov [bk_ca], al
        mov [bk_ab], al
        mov [bk_aa], al
        mov si, bk_ab
        mov di, bk_aa
        call backup_one
        mov si, bk_cs
        mov di, bk_ca
        call backup_one
        ret

; renombrar SI (origen) a DI (.ANT); borra antes un .ANT anterior
backup_one:
        push si
        push di
        mov dx, di
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov dx, di
        mov ah, 0x41
        int 0x21
        pop di
        pop si
        push si
        push di
        mov dx, si
        mov ah, 0x56
        int 0x21
        pop di
        pop si
        ret

; =============================================================================
;  Desempaquetar C:\ACTUALIZA.PAQ en C:\ACTUALIZA (formato PAQ1, ver docs)
; =============================================================================
unpack_paq:
        mov dx, tpaq_path
        mov ax, 0x3D00
        int 0x21
        jc .err
        mov [paqfh], ax
        mov dword [suma_cl], 0
        mov di, hdrbuf
        mov cx, 12
        call lee_exacto
        jc .errc
        mov eax, [hdrbuf]
        cmp eax, 'PAQ1'
        jne .errc
        mov eax, [hdrbuf + 4]
        mov [paq_n], eax
        mov eax, [hdrbuf + 8]
        mov [paq_sum], eax
        mov dword [paq_i], 0
.loop:
        mov eax, [paq_i]
        cmp eax, [paq_n]
        jae .fin
        mov di, hdrbuf
        mov cx, 2
        call lee_exacto
        jc .errc
        mov cx, [hdrbuf]
        cmp cx, PAQ_NAME_MAX
        ja .errc
        test cx, cx
        jz .errc
        mov di, nombuf
        call lee_exacto
        jc .errc
        mov bx, cx
        mov byte [nombuf + bx], 0
        mov di, hdrbuf
        mov cx, 4
        call lee_exacto
        jc .errc
        mov eax, [hdrbuf]
        mov [datlen], eax
        call ruta_destino
        call mkpath
        mov dx, destpath
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .errc
        mov [outfh], ax
        call copia_datos
        jc .errc_out
        mov bx, [outfh]
        mov ah, 0x3E
        int 0x21
        inc dword [paq_i]
        jmp .loop
.fin:
        mov bx, [paqfh]
        mov ah, 0x3E
        int 0x21
        mov eax, [suma_cl]
        cmp eax, [paq_sum]
        jne .err
        clc
        ret
.errc_out:
        mov bx, [outfh]
        mov ah, 0x3E
        int 0x21
.errc:
        mov bx, [paqfh]
        mov ah, 0x3E
        int 0x21
.err:
        stc
        ret

; leer exactamente CX bytes del paquete a DI; CF=1 si no se pudo
lee_exacto:
        push bx
        push si
        mov si, cx
.l:
        test si, si
        jz .ok
        mov bx, [paqfh]
        mov cx, si
        mov dx, di
        mov ah, 0x3F
        int 0x21
        jc .err
        test ax, ax
        jz .err
        add di, ax
        sub si, ax
        jmp .l
.ok:
        clc
        jmp .r
.err:
        stc
.r:
        pop si
        pop bx
        ret

; copiar [datlen] bytes del paquete al fichero de salida, acumulando la suma
copia_datos:
.l:
        mov eax, [datlen]
        test eax, eax
        jz .ok
        cmp eax, BUFSZ
        jbe .n
        mov eax, BUFSZ
.n:
        mov [chunk], eax
        mov bx, [paqfh]
        mov cx, [chunk]
        mov dx, buf
        mov ah, 0x3F
        int 0x21
        jc .err
        cmp ax, cx
        jne .err
        call suma_buf
        mov bx, [outfh]
        mov cx, [chunk]
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .err
        cmp ax, cx
        jne .err
        mov eax, [datlen]
        sub eax, [chunk]
        mov [datlen], eax
        jmp .l
.ok:
        clc
        ret
.err:
        stc
        ret

; sumar [chunk] bytes de buf a [suma_cl]
suma_buf:
        push cx
        push si
        push eax
        push edx
        mov cx, [chunk]
        mov si, buf
        mov edx, [suma_cl]
.l:
        jcxz .done
        movzx eax, byte [si]
        add edx, eax
        inc si
        dec cx
        jmp .l
.done:
        mov [suma_cl], edx
        pop edx
        pop eax
        pop si
        pop cx
        ret

; destpath = tdir_path + "\" + nombuf (con '/' -> '\')
ruta_destino:
        mov si, tdir_path
        mov di, destpath
        call strcpy
        mov byte [di], '\'
        inc di
        mov si, nombuf
.l:
        lodsb
        test al, al
        jz .e
        cmp al, '/'
        jne .nn
        mov al, '\'
.nn:
        stosb
        jmp .l
.e:
        mov byte [di], 0
        ret

; crear los directorios intermedios de destpath (ignora los que ya existan)
mkpath:
        push si
        mov si, destpath
        add si, 3                   ; saltar "X:\"
.l:
        mov al, [si]
        test al, al
        jz .done
        cmp al, '\'
        jne .n
        mov byte [si], 0
        push si
        mov dx, destpath
        mov ah, 0x39
        int 0x21
        pop si
        mov byte [si], '\'
.n:
        inc si
        jmp .l
.done:
        pop si
        ret

; =============================================================================
;  Borrar recursivamente un árbol de directorios (el temporal)
;  SI = ruta ASCIIZ del directorio a vaciar y borrar
; =============================================================================
borra_arbol:
        push si
.ff:
        mov dx, arb_dta
        mov ah, 0x1A
        int 0x21
        call arb_patron
        mov dx, arb_pat
        mov cx, 0x06
        mov ah, 0x4E
        int 0x21
        jc .fd
        mov di, arb_full_b
        call _mk_pathname
        mov dx, arb_full_b
        mov ax, 0x4301
        xor cx, cx
        int 0x21
        mov dx, arb_full_b
        mov ah, 0x41
        int 0x21
        jmp .ff
.fd:
        mov dx, arb_dta
        mov ah, 0x1A
        int 0x21
        call arb_patron
        mov dx, arb_pat
        mov cx, 0x10
        mov ah, 0x4E
        int 0x21
.dl:
        jc .rm
        test byte [arb_dta + 0x15], 0x10
        jz .dn
        cmp byte [arb_dta + 0x1E], '.'
        je .dn
        call arb_sub
        jc .dn                      ; demasiada profundidad: dejarlo
        push si
        mov si, di
        inc byte [arb_prof]
        call borra_arbol
        dec byte [arb_prof]
        pop si
        jmp .fd
.dn:
        mov ah, 0x4F
        int 0x21
        jmp .dl
.rm:
        pop si
        mov dx, si
        mov ah, 0x3A
        int 0x21
        ret

; arb_pat = [si] + "\*.*"
arb_patron:
        push si
        push di
        mov di, arb_pat
.c:
        lodsb
        mov [di], al
        inc di
        test al, al
        jnz .c
        dec di
        mov si, s_estrella
.c2:
        lodsb
        mov [di], al
        inc di
        test al, al
        jnz .c2
        pop di
        pop si
        ret

; DI = buffer del nivel actual con [si] + "\" + nombre; CF=1 si no cabe nivel
arb_sub:
        movzx ax, byte [arb_prof]
        cmp al, ARB_MAX
        jae .full
        imul ax, 96
        add ax, arb_pool
        mov di, ax
        push di
        call _mk_pathname
        pop di
        clc
        ret
.full:
        stc
        ret

; DI = destino; construye [si] + "\" + (arb_dta+0x1E); preserva SI
_mk_pathname:
        push si
.a:
        lodsb
        test al, al
        jz .b
        mov [di], al
        inc di
        jmp .a
.b:
        mov byte [di], '\'
        inc di
        mov si, arb_dta + 0x1E
.c:
        lodsb
        mov [di], al
        inc di
        test al, al
        jnz .c
        pop si
        ret

; =============================================================================
;  Utilidades de la línea de órdenes
; =============================================================================
blancos:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        je .s
        ret
.s:
        inc si
        jmp blancos

palabra:
        mov cx, 250
.l:
        lodsb
        cmp al, ' '
        jbe .f
        stosb
        loop .l
.f:
        dec si
        mov byte [di], 0
        ret

; ---- url -> host, puerto, ruta ----
url_partir:
        mov si, url
        mov di, s_https
        call prefijo
        jnc .cifr
        mov di, s_http
        call prefijo
        jc .sin
        add si, 7
.sin:
        mov di, host
        mov cx, 120
.h:
        lodsb
        test al, al
        jz .hf
        cmp al, '/'
        je .hf
        cmp al, ':'
        je .hf
        cmp al, '?'
        je .hf
        stosb
        loop .h
.hf:
        mov byte [di], 0
        cmp di, host
        je .mal
        mov word [puerto], 80
        cmp al, ':'
        jne .np
        call parse_num
        jc .mal
        test eax, eax
        jz .mal
        cmp eax, 65535
        ja .mal
        mov [puerto], ax
        lodsb
.np:
        mov di, ruta
        mov byte [di], '/'
        inc di
        test al, al
        jz .rf
        cmp al, '/'
        je .r
        stosb
.r:
        mov cx, 240
.rl:
        lodsb
        test al, al
        jz .rf
        stosb
        loop .rl
.rf:
        mov byte [di], 0
        clc
        ret
.cifr:
        mov si, m_https
        stc
        ret
.mal:
        mov si, m_urlmal
        stc
        ret

; ¿empieza SI por DI (sin mirar mayúsculas)? CF = 0 si sí
prefijo:
        push si
.l:
        mov ah, [di]
        test ah, ah
        jz .si
        lodsb
        call upcase
        inc di
        cmp al, ah
        je .l
        pop si
        stc
        ret
.si:
        pop si
        clc
        ret

; ---- la petición ----
peticion:
        mov di, peti
        mov si, s_get
        call copia
        mov si, ruta
        call copia
        mov si, s_http10
        call copia
        mov si, host
        call copia
        cmp word [puerto], 80
        je .np
        mov al, ':'
        stosb
        movzx eax, word [puerto]
        call num_a_texto
.np:
        mov si, s_cabeceras
        call copia
        mov cx, di
        sub cx, peti
        ret

copia:
        lodsb
        test al, al
        jz .r
        stosb
        jmp copia
.r:
        ret

; EAX -> texto en DI
num_a_texto:
        push ebx
        push edx
        push cx
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        stosb
        loop .p
        pop cx
        pop edx
        pop ebx
        ret

; =============================================================================
;  Bajar [url] por HTTP. [dl_modo]=0 a body_buf (memoria), =1 al fichero
;  [archivo]. Hace hasta 4 redirecciones. CF=0 ok; CF=1 fallo (SI = mensaje).
; =============================================================================
http_get:
        mov byte [n_redir], 0
.otra_url:
        mov byte [h_estado], 0
        mov word [h_len], 0
        mov dword [recibidos], 0
        mov dword [total], 0
        mov dword [recib_cuerpo], 0
        mov word [body_len], 0
        mov byte [t_fin], 0
        mov byte [t_rst], 0
        mov byte [abortar], 0
        mov byte [creado], 0
        mov word [prog_ult], 0xFFFF
        call url_partir
        jc .err
        mov si, m_buscando
        call puts
        mov si, host
        call puts
        mov si, m_puntos
        call puts
        mov si, host
        call dns_buscar
        jc .err_nl
        mov [srv_ip], eax
        call ip_poner
        mov al, ':'
        call putc
        mov di, num_txt
        movzx eax, word [puerto]
        call num_a_texto
        mov byte [di], 0
        mov si, num_txt
        call puts
        call crlf
        mov eax, [srv_ip]
        mov bx, [puerto]
        call tcp_abrir
        jc .noconn
        call peticion
        mov si, peti
        call tcp_enviar
        jc .err_cierra
        call tic
        mov [t_ult], eax
        mov eax, [recibidos]
        mov [rec_ult], eax
.l:
        call red_sondear
        cmp byte [abortar], 0
        jne .corta
        cmp byte [t_rst], 0
        jne .cortada
        cmp byte [t_fin], 0
        jne .acabo
        call esc_pulsado
        jc .esc
        mov eax, [recibidos]
        cmp eax, [rec_ult]
        je .quieto
        mov [rec_ult], eax
        call tic
        mov [t_ult], eax
        jmp .l
.quieto:
        call tic
        sub eax, [t_ult]
        cmp eax, 30 * 18
        jb .l
        mov si, m_parado
        jmp .falla
.esc:
        mov si, m_esc
        jmp .falla
.cortada:
        mov si, m_tcprst
        jmp .falla
.corta:
        mov si, [abortar_msg]
.falla:
        push si
        call tcp_cerrar
        pop si
        call borrar_arch
        stc
        ret
.acabo:
        call tcp_cerrar
        cmp byte [h_estado], 1
        je .cuerpo
        cmp byte [h_estado], 3
        je .redir
        mov si, m_nohttp
        jmp .falla
.redir:
        inc byte [n_redir]
        cmp byte [n_redir], 4
        ja .muchas
        mov si, m_redir
        call puts
        mov si, url
        call puts
        call crlf
        jmp .otra_url
.muchas:
        mov si, m_muchas
        jmp .falla
.cuerpo:
        cmp byte [dl_modo], 0
        je .mem
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        mov word [fh], 0
        mov byte [creado], 0
        call crlf
        mov eax, [total]
        test eax, eax
        jz .okf
        cmp eax, [recib_cuerpo]
        je .okf
        ; incompleto: borrar el fichero a medias
        mov byte [creado], 1
        call borrar_arch
        mov si, m_incompleto
        stc
        ret
.okf:
        clc
        ret
.mem:
        clc
        ret
.noconn:
        mov si, m_noconn
        stc
        ret
.err_cierra:
        call tcp_cerrar
        mov si, m_noenvio
        stc
        ret
.err_nl:
        call crlf
.err:
        stc
        ret

; progreso sencillo de la descarga del paquete (cada 4 KB)
progreso_simple:
        mov eax, [recib_cuerpo]
        shr eax, 12
        cmp ax, [prog_ult]
        je .r
        mov [prog_ult], ax
        mov al, 13
        call putc
        mov si, m_recib
        call puts
        mov eax, [recib_cuerpo]
        call putnum
        mov si, m_oct
        call puts
.r:
        ret

borrar_arch:
        cmp byte [creado], 0
        je .r
        mov bx, [fh]
        test bx, bx
        jz .b
        mov ah, 0x3E
        int 0x21
.b:
        mov dx, archivo
        mov ah, 0x41
        int 0x21
        mov byte [creado], 0
.r:
        ret

; =============================================================================
;  Lo que llega por la conexión (lo llama tcp_in): SI = datos, CX = octetos.
; =============================================================================
act_datos:
        cmp byte [abortar], 0
        jne .r
        movzx eax, cx
        add [recibidos], eax
        mov al, [h_estado]
        cmp al, 1
        je .cuerpo
        test al, al
        jnz .r
.cab:
        test cx, cx
        jz .r
        lodsb
        dec cx
        mov bx, [h_len]
        cmp bx, HDR_MAX - 1
        jae .larga
        mov [hdr + bx], al
        inc word [h_len]
        cmp al, 10
        jne .cab
        cmp bx, 3
        jb .cab
        cmp dword [hdr + bx - 3], 0x0A0D0A0D
        je .fin_cab
        cmp word [hdr + bx - 1], 0x0A0A
        jne .cab
.fin_cab:
        mov bx, [h_len]
        mov byte [hdr + bx], 0
        push si
        push cx
        call cabeceras
        pop cx
        pop si
        cmp byte [h_estado], 1
        jne .r
.cuerpo:
        cmp byte [dl_modo], 0
        jne .cfile
.cc:
        test cx, cx
        jz .r
        mov bx, [body_len]
        cmp bx, BODY_MAX - 1
        jae .r
        lodsb
        mov [body_buf + bx], al
        inc word [body_len]
        dec cx
        jmp .cc
.cfile:
        test cx, cx
        jz .r
        movzx eax, cx
        add [recib_cuerpo], eax
        mov dx, si
        mov bx, [fh]
        mov ah, 0x40
        int 0x21
        jc .disco
        cmp ax, cx
        jne .disco
        call progreso_simple
.r:
        ret
.disco:
        mov word [abortar_msg], m_disco
        mov byte [abortar], 1
        ret
.larga:
        mov word [abortar_msg], m_nohttp
        mov byte [abortar], 1
        ret

; las cabeceras ya están en hdr (acaban en 0)
cabeceras:
        mov si, hdr
        mov di, s_http_
        call prefijo
        jc .nohttp
.sp:
        lodsb
        test al, al
        jz .nohttp
        cmp al, ' '
        jne .sp
        call parse_num
        jc .nohttp
        mov [h_codigo], ax
        mov si, hdr
.lin:
        lodsb
        test al, al
        jz .hecho
        cmp al, 10
        jne .lin
        mov di, s_clen
        call prefijo
        jc .nl
        add si, 15
        call blancos
        call parse_num
        jc .lin
        mov [total], eax
        jmp .lin
.nl:
        mov di, s_loc
        call prefijo
        jc .lin
        add si, 9
        call blancos
        mov di, nueva_url
        mov cx, 250
.lc:
        lodsb
        cmp al, ' '
        jbe .lf
        stosb
        loop .lc
.lf:
        mov byte [di], 0
        dec si
        jmp .lin
.hecho:
        mov ax, [h_codigo]
        cmp ax, 200
        je .bien
        cmp ax, 301
        jb .mal
        cmp ax, 308
        ja .mal
        cmp ax, 304
        je .mal
        cmp ax, 305
        je .mal
        cmp ax, 306
        je .mal
        cmp byte [nueva_url], 0
        je .mal
        mov si, nueva_url
        mov di, s_https
        call prefijo
        jnc .acifr
        cmp byte [nueva_url], '/'
        jne .abs
        mov di, url
        mov si, s_httpm
        call copia
        mov si, host
        call copia
        cmp word [puerto], 80
        je .rp
        mov al, ':'
        stosb
        movzx eax, word [puerto]
        call num_a_texto
.rp:
        mov si, nueva_url
        call copia
        mov byte [di], 0
        jmp .redir
.abs:
        mov si, nueva_url
        mov di, url
        mov cx, 251
        rep movsb
.redir:
        mov byte [h_estado], 3
        ret
.acifr:
        mov word [abortar_msg], m_redircifr
        mov byte [abortar], 1
        mov byte [h_estado], 2
        ret
.bien:
        mov byte [h_estado], 1
        mov word [body_len], 0
        mov dword [recib_cuerpo], 0
        cmp byte [dl_modo], 0
        je .bret
        mov dx, archivo
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .nocrea
        mov [fh], ax
        mov byte [creado], 1
        mov si, m_guardando
        call puts
        mov si, archivo
        call puts
        mov si, m_puntos2
        call puts
.bret:
        ret
.nocrea:
        mov word [abortar_msg], m_nocrea
        mov byte [abortar], 1
        mov byte [h_estado], 2
        ret
.nohttp:
        mov word [abortar_msg], m_nohttp
        mov byte [abortar], 1
        mov byte [h_estado], 2
        ret
.mal:
        mov byte [h_estado], 2
        mov si, hdr
        mov di, h_linea
        mov cx, 70
.ml:
        lodsb
        cmp al, 13
        je .mf
        cmp al, 10
        je .mf
        test al, al
        jz .mf
        stosb
        loop .ml
.mf:
        mov byte [di], 0
        mov word [abortar_msg], m_servidor
        mov byte [abortar], 1
        ret

; =============================================================================
;  Cargar el controlador de red HSPRED (como ACTUALIZAR original)
; =============================================================================
carga_hspred:
        mov si, m_cargando
        call puts
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [hspred_path], dl
        mov word [pblk], 0
        mov word [pblk + 2], hspred_tail
        mov [pblk + 4], cs
        mov word [pblk + 6], fcb0
        mov [pblk + 8], cs
        mov word [pblk + 10], fcb0
        mov [pblk + 12], cs
        mov [sv_sp], sp
        mov dx, hspred_path
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
        ret

%include "equipodet.inc"
%include "copiaarbol.inc"
%include "util.inc"
%include "red.inc"

m_uso       db "Busca por la red una versión más nueva o una edición más rica de", 13, 10
            db "Hispa-DOS que la instalada (", VER_STR, ") y, si el equipo la aguanta y", 13, 10
            db "quieres, la descarga y la instala en el sitio (conserva tus datos y", 13, 10
            db "ajustes: *.INI, *.REC).", 13, 10
            db 13, 10
            db "ACTUALIZAR [http://servidor[:puerto]/ruta]", 13, 10
            db 13, 10
            db "Sin dirección usa la clave ACTUALIZAR= de \HISPADOS\RED.INI y, si no", 13, 10
            db "está, una de por omisión. El servidor da un archivo de texto con las", 13, 10
            db "versiones y sus requisitos (ver docs/RED.md).", 13, 10
            db 13, 10
            db "Solo HTTP sin cifrar: las direcciones https:// todavía no funcionan.", 13, 10
            db "Necesita HSPRED y la red configurada (ver CONFRED).", 13, 10, 0
m_cargando  db "Cargando el controlador de red (HSPRED)...", 13, 10, 0
m_mirando   db "Buscando actualizaciones de Hispa-DOS ", VER_STR, "...", 13, 10, 0
m_buscando  db "Servidor ", 0
m_puntos    db ": ", 0
m_puntos2   db ":", 13, 10, 0
m_guardando db "Guardando en ", 0
m_tuequipo  db "Tu equipo: ", 0
m_cpu386    db "386", 0
m_cpu486    db "486", 0
m_cpu586    db "Pentium o superior", 0
m_coma      db ", ", 0
m_mbmem     db " MB de memoria, ", 0
m_mblibre   db " MB libres en ", 0
m_dp        db ":.", 13, 10, 0
m_instalada db "Tienes instalado Hispa-DOS ", 0
s_verstr    db VER_STR, 0
m_inst2     db ".", 13, 10, 0
m_aldia     db "Hispa-DOS ", VER_STR, " está al día.", 13, 10, 0
m_recom     db "Tu equipo admite la edición ", 0
m_estaver   db "más reciente", 0
m_recom2    db ": la ", 0
m_pregunta  db "¿Descargar e instalar la ", 0
m_pregunta2 db "? (S/N) ", 0
m_solomirar db "De acuerdo: solo mirar, no se ha cambiado nada.", 13, 10, 0
m_nueva     db "Hay una versión nueva: ", 0
m_punto     db ".", 13, 10, 0
m_punto2    db ".", 13, 10, 0
m_desc      db "Descárgala en: ", 0
m_comoact   db "Para actualizar: arranca desde el USB nuevo y ejecuta INSTALAR.", 13, 10, 0
m_llega     db "Tu equipo llega hasta la ", 0
m_edi_ab    db " (edición ", 0
m_cierra    db ")", 0
m_laedi     db "La edición ", 0
m_laversion db "La versión ", 0
m_parsp     db " (", 0
m_necreq    db " necesita ", 0
m_un        db "un ", 0
m_y         db " y ", 0
m_mbmem2    db " MB de memoria", 0
m_mbdisco   db " MB de disco", 0
m_sinver    db "El servidor contestó, pero el archivo no trae ninguna versión.", 13, 10, 0
m_sinred    db "Cargue HSPRED y configure la red (CONFRED) antes de buscar", 13, 10
            db "actualizaciones.", 0
m_descpaq   db "Descargando el paquete de la nueva versión...", 13, 10, 0
m_desempaq  db "Comprobando y desempaquetando...", 13, 10, 0
m_aplicando db "Aplicando la actualización (se conservan tus datos y ajustes)...", 13, 10, 0
m_actok     db "Actualizado a ", 0
m_actok2    db ". Reinicia para usar la nueva versión.", 13, 10, 0
m_abortado  db "No se ha cambiado nada en el sistema.", 13, 10, 0
m_paqmal    db "El paquete bajado no es válido o está incompleto.", 13, 10, 0
m_redir     db "El servidor manda a otra dirección: ", 0
m_muchas    db "Demasiadas redirecciones.", 0
m_https     db "Las direcciones https:// (cifradas) todavía no funcionan en Hispa-DOS;", 13, 10
            db "pruebe con http://, si el servidor lo admite.", 0
m_redircifr db "El servidor manda a una dirección https:// (cifrada), que todavía no", 13, 10
            db "funciona en Hispa-DOS.", 0
m_urlmal    db "Esa dirección no es válida (ejemplo: http://servidor/versiones.txt).", 0
m_nohttp    db "El servidor no contesta como un servidor web (HTTP).", 0
m_servidor  db "El servidor contesta: "
h_linea     times 72 db 0
m_nocrea    db "No se puede crear el archivo del paquete.", 0
m_disco     db "No se puede escribir en el disco (¿está lleno?).", 0
m_incompleto db "La descarga del paquete ha quedado incompleta.", 0
m_noconn    db "No se ha podido conectar con el servidor.", 0
m_noenvio   db "No se ha podido enviar la petición al servidor.", 0
m_parado    db "El servidor ha dejado de enviar (30 segundos sin nada).", 0
m_esc       db "Búsqueda interrumpida (Esc).", 0
m_recib     db "Recibidos ", 0
m_oct       db " octetos", 0

; --- la dirección de por omisión: se puede cambiar con la clave ACTUALIZAR=
;     de \HISPADOS\RED.INI ---
s_url_def   db "http://actualiza.clicgest.es/versiones.txt", 0

hspred_path db "A:\HISPADOS\HSPRED.COM", 0
hspred_tail db 0, 13
s_clave_act db "ACTUALIZAR", 0
k_version   db "VERSION", 0
k_descarga  db "DESCARGA", 0
k_nota      db "NOTA", 0
k_cpu       db "CPU", 0
k_ram       db "RAM", 0
k_disco     db "DISCO", 0
k_nivel     db "NIVEL", 0
k_paquete   db "PAQUETE", 0
k_edicion   db "EDICION", 0
s_https     db "HTTPS://", 0
s_http      db "HTTP://", 0
s_httpm     db "http://", 0
s_http_     db "HTTP/", 0
s_clen      db "CONTENT-LENGTH:", 0
s_loc       db "LOCATION:", 0
s_get       db "GET ", 0
s_http10    db " HTTP/1.0", 13, 10, "Host: ", 0
s_cabeceras db 13, 10, "User-Agent: Hispa-DOS/", VER_STR, " ACTUALIZAR", 13, 10
            db "Accept: */*", 13, 10
            db "Connection: close", 13, 10, 13, 10, 0

; rutas del temporal y de los conjuntos a copiar (la letra se pone en marcha)
tpaq_path   db "A:\ACTUALIZA.PAQ", 0
tpaq_d      equ tpaq_path
tdir_path   db "A:\ACTUALIZA", 0
tdir_d      equ tdir_path
s_actpre    db "\ACTUALIZA", 0
s_estrella  db "\*.*", 0
s_root      db "\", 0
s_hdir      db "\HISPADOS\", 0
s_hsp       db "\HISPADOS\*.*", 0
s_hjdir     db "\HISPADOS\JUEGOS\", 0
s_hspj      db "\HISPADOS\JUEGOS\*.*", 0
s_hndir     db "\HISPADOS\JUEGOS\NES\", 0
s_hspn      db "\HISPADOS\JUEGOS\NES\*.*", 0
s_hsdir     db "\HISPADOS\JUEGOS\SMS\", 0
s_hsps      db "\HISPADOS\JUEGOS\SMS\*.*", 0
s_hpdir     db "\HISPADOS\PROGS\", 0
s_hspp      db "\HISPADOS\PROGS\*.*", 0
s_kernel    db "\KERNEL.SYS", 0
s_command   db "\COMMAND.COM", 0
s_leeme     db "\LEEME.TXT", 0
s_config    db "\CONFIG.SYS", 0
s_autoexec  db "\AUTOEXEC.BAT", 0
bk_ab       db "A:\AUTOEXEC.BAT", 0
bk_aa       db "A:\AUTOEXEC.ANT", 0
bk_cs       db "A:\CONFIG.SYS", 0
bk_ca       db "A:\CONFIG.ANT", 0

        section .bss
url         resb 256
nueva_url   resb 256
host        resb 128
ruta        resb 256
archivo     resb 260
url_dado    resb 1
puerto      resw 1
srv_ip      resd 1
peti        resb 700
hdr         resb HDR_MAX + 4
h_len       resw 1
h_estado    resb 1
h_codigo    resw 1
n_redir     resb 1
dl_modo     resb 1
fh          resw 1
creado      resb 1
abortar     resb 1
abortar_msg resw 1
recibidos   resd 1
recib_cuerpo resd 1
total       resd 1
rec_ult     resd 1
t_ult       resd 1
prog_ult    resw 1
body_buf    resb BODY_MAX
body_len    resw 1
p_clave     resb 16
p_valor     resb 208
p_si        resw 1
i_clave     resb 16
num_txt     resb 8
u_numbuf    resb 16
u_lbuf      resb 84
; --- detección del equipo ---
det_cpu_lvl resb 1
boot_num    resb 1
boot_letter resb 1
det_ram_kib resd 1
det_free_mib resd 1
; --- tablas de versiones ---
nver        resb 1
cur_sec     resw 1
cur_i       resw 1
sp_str      resw 1
sp_edi      resw 1
sp_paq      resw 1
sp_not      resw 1
v_num       resd MAX_VER
v_ram       resd MAX_VER
v_disco     resw MAX_VER
v_nivel     resw MAX_VER
v_cpu       resb MAX_VER
v_haspaq    resb MAX_VER
v_cumple    resb MAX_VER
v_str       resb MAX_VER * V_STRSZ
v_edi       resb MAX_VER * V_EDISZ
v_paq       resb MAX_VER * V_PAQSZ
v_not       resb MAX_VER * V_NOTSZ
best_inst   resw 1
best_notify resw 1
best_reach  resw 1
best_unreach resw 1
any_better  resb 1
req_prev    resb 1
mn_a        resw 1
mn_b        resw 1
; --- desempaquetado ---
paqfh       resw 1
outfh       resw 1
paq_n       resd 1
paq_sum     resd 1
paq_i       resd 1
suma_cl     resd 1
datlen      resd 1
chunk       resd 1
hdrbuf      resb 16
nombuf      resb PAQ_NAME_MAX + 2
destpath    resb 160
; --- borrado recursivo del temporal ---
arb_prof    resb 1
arb_dta     resb 128
arb_pat     resb 112
arb_full_b  resb 112
arb_pool    resb ARB_MAX * 96
; --- motor de copia (lib/copiaarbol.inc) ---
pat         resb 96
sdir        resb 96
path_s      resb 128
path_d      resb 128
dta         resb 128
dta2        resb 128
buf         resb BUFSZ
; --- EXEC de HSPRED ---
sv_sp       resw 1
fcb0        resb 40
pblk        resb 14
bss_fin:
            alignb 16
            resb 2048
pila_tope:
prog_end:
