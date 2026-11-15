; =============================================================================
;  TERMINAL.HVP - Terminal, el símbolo del sistema de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Una ventana con un símbolo del sistema donde se escriben órdenes y se ve su
;  salida, mientras se pueden seguir usando las demás ventanas. Es «Hispa-DOS
;  en ventana».
;
;  POR QUÉ NO LANZA COMMAND.COM: DOS es monotarea y sin preferencia, y un
;  accesorio .HVP corre en modo protegido. Poner COMMAND.COM (modo real) a la
;  vez que la GUI es inviable de forma sencilla. POR ESO esta ventana lleva su
;  PROPIO intérprete de órdenes, que usa el puente a DOS de Hispa Ventanas (las
;  funciones dos_* de la interfaz y, para lo que no exporta —buscar, carpetas,
;  renombrar—, rm_interrup, como hace el Administrador de archivos). Los
;  programas externos (.COM/.EXE) NO corren aquí: se siguen abriendo a pantalla
;  completa desde el menú (lo dice la AYUDA).
;
;  La orden se ejecuta con ejecuta_linea(ESI = cadena acabada en 0): parte la
;  línea, despacha la orden interna y ESCRIBE la salida en el área de texto. No
;  depende de QUIÉN trae la cadena: hoy la trae el teclado de la ventana; más
;  adelante la misma función puede alimentarse desde una sesión de red (la
;  consola remota en ventana).
;
;  Órdenes internas (todas con el puente a DOS): DIR, CD/CHDIR, TYPE, DEL/
;  BORRAR, COPY, MD/MKDIR, RD/RMDIR, REN/RENOMBRAR, VER, CLS, ECHO, AYUDA/HELP
;  y el cambio de unidad «X:». La carpeta actual es el estado del propio DOS
;  (como hace CONSOLA), así CD se conserva entre órdenes.
; =============================================================================
%include "hvp.inc"
%include "version.inc"              ; VER_STR, de lib/version.inc

; ---- medidas y topes ----
FILA        equ 16                  ; alto de una fila de texto (la fuente 8x16)
ANCHO_C     equ 8                   ; ancho de un carácter
MARGEN      equ 4                   ; margen izquierdo del texto
COLS        equ 100                 ; columnas que se guardan por línea
TOT         equ 300                 ; líneas de historial (scrollback)
ENT_MAX     equ 255                 ; largo máximo de la línea de entrada
BUFIO       equ 8192                ; búfer de lectura/escritura (TYPE, COPY)
HIST_N      equ 16                  ; órdenes recordadas (flecha arriba/abajo)
HIST_L      equ 128                 ; largo de cada una

ID_BARRA    equ 200                 ; la barra de desplazamiento (hija)
; órdenes del menú
O_LIMPIAR   equ 101
O_SALIR     equ 102
O_ACERCA    equ 121

HVP_CABECERA "Terminal", inicio, 1
        db "................................"
        db "................................"
        db "..7777777777777777777777777777.."
        db "..7000000000000000000000000007.."
        db "..7000000000000000000000000007.."
        db "..7000e00000000000000000000007.."
        db "..7000e00000000000000000000007.."
        db "..70000e0000000000000000000007.."
        db "..70000e0000000000000000000007.."
        db "..700000e000000000000000000007.."
        db "..700000e000000000000000000007.."
        db "..7000000e00000000000000000007.."
        db "..7000000e00000000000000000007.."
        db "..700000e000000000000000000007.."
        db "..700000e000000000000000000007.."
        db "..70000e0000000000000000000007.."
        db "..70000e0000000000000000000007.."
        db "..7000e00000000000000000000007.."
        db "..7000e00000000000000000000007.."
        db "..7000000000999999990000000007.."
        db "..7000000000000000000000000007.."
        db "..7000000000000000000000000007.."
        db "..7000000000000000000000000007.."
        db "..7000000000000000000000000007.."
        db "..7777777777777777777777777777.."
        db ".............777777............."
        db ".............777777............."
        db ".........77777777777777........."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- clase de la ventana y plantillas ----
c_term      dd ter_proc
            db C_TINTA, 0, 0, 0     ; el fondo se borra en negro
term_plant:
        PLANTILLA c_term, t_titulo, E_NORMAL, 100, 60, 560, 360, 0
; la barra de desplazamiento vertical (hija): la clase va como número de la
; interfaz (HV_c_barra); hvl_clases la resuelve
barra_plant:
        PLANTILLA HV_c_barra, 0, E_HIJA | E_OCULTA, 0, 0, BD_AN, 50, ID_BARRA
        dd 0

; ---- la barra de menús ----
term_menu:
        db "&Archivo", 0
        dw O_LIMPIAR
        db "&Limpiar la pantalla    Ctrl+L", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_ACERCA
        db "&Acerca de Terminal...", 0
        dw 0xFFFF
        db 0

; ---- textos (todo en español) ----
t_titulo    db "Terminal", 0
t_acerca_tit db "Acerca de Terminal", 0
t_acerca    db "Terminal", 10
            db "El símbolo del sistema de Hispa Ventanas.", 10
            db "Escriba AYUDA para ver las órdenes.", 0

m_bienvenida  db "Terminal de Hispa Ventanas.", 0
m_bienvenida2 db "Escriba AYUDA para ver las órdenes.", 0

s_estrella  db "*.*", 0
s_dir       db "<DIR>", 0
s_2esp      db "  ", 0
s_arch      db " archivo(s), ", 0
s_carp      db " carpeta(s)", 0

m_no_arch     db "Archivo no encontrado.", 0
m_no_abrir    db "No se puede abrir el archivo.", 0
m_falta_nombre db "Falta el nombre del archivo.", 0
m_faltan      db "Faltan argumentos.", 0
m_copiado     db "        1 archivo(s) copiado(s).", 0
m_no_copia    db "No se ha podido copiar.", 0
m_no_borra    db "No se ha podido borrar el archivo.", 0
m_no_crea_dir db "No se ha podido crear la carpeta.", 0
m_no_quita_dir db "No se ha podido quitar la carpeta.", 0
m_no_renombra db "No se ha podido cambiar el nombre.", 0
m_no_ruta     db "Ruta no encontrada.", 0
m_no_unidad   db "Unidad no válida.", 0
m_desconocido db "Orden o nombre de archivo incorrecto.", 0
m_version     db "Hispa-DOS ", VER_STR, 0

; la AYUDA: líneas separadas por 10 (sb_putc lo toma como salto de línea)
m_ayuda:
        db "Órdenes de Terminal:", 10
        db "  DIR [patrón]      lista la carpeta", 10
        db "  CD [ruta]         cambia de carpeta (sin ruta, la enseña)", 10
        db "  TYPE archivo      vuelca un archivo de texto", 10
        db "  COPY orig dest    copia un archivo", 10
        db "  DEL archivo       borra un archivo (o BORRAR)", 10
        db "  MD carpeta        crea una carpeta (o MKDIR)", 10
        db "  RD carpeta        quita una carpeta (o RMDIR)", 10
        db "  REN viejo nuevo   cambia el nombre (o RENOMBRAR)", 10
        db "  VER               versión del sistema", 10
        db "  CLS               limpia la pantalla", 10
        db "  ECHO texto        escribe el texto", 10
        db "  X:                cambia de unidad", 10
        db "  AYUDA             esta ayuda (o HELP)", 10
        db "Los programas externos (.COM/.EXE) NO se ejecutan aquí: se abren a", 10
        db "pantalla completa desde el menú.", 10, 0

; ---- la tabla de órdenes: dd nombre, dd rutina (acaba en dd 0) ----
n_dir       db "DIR", 0
n_cd        db "CD", 0
n_chdir     db "CHDIR", 0
n_type      db "TYPE", 0
n_del       db "DEL", 0
n_borrar    db "BORRAR", 0
n_copy      db "COPY", 0
n_md        db "MD", 0
n_mkdir     db "MKDIR", 0
n_rd        db "RD", 0
n_rmdir     db "RMDIR", 0
n_ren       db "REN", 0
n_renombrar db "RENOMBRAR", 0
n_ver       db "VER", 0
n_cls       db "CLS", 0
n_echo      db "ECHO", 0
n_ayuda     db "AYUDA", 0
n_help      db "HELP", 0
        align 4
cmd_tabla:
        dd n_dir, cmd_dir
        dd n_cd, cmd_cd
        dd n_chdir, cmd_cd
        dd n_type, cmd_type
        dd n_del, cmd_del
        dd n_borrar, cmd_del
        dd n_copy, cmd_copy
        dd n_md, cmd_md
        dd n_mkdir, cmd_md
        dd n_rd, cmd_rd
        dd n_rmdir, cmd_rd
        dd n_ren, cmd_ren
        dd n_renombrar, cmd_ren
        dd n_ver, cmd_ver
        dd n_cls, cmd_cls
        dd n_echo, cmd_echo
        dd n_ayuda, cmd_ayuda
        dd n_help, cmd_ayuda
        dd 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
inicio:
        cld
        mov [param], edi
        mov eax, [hv_api + HV_dos_buf * 4]
        mov [dbuf], eax             ; el búfer de paso de Hispa Ventanas
        mov esi, barra_plant        ; resolver la clase de la barra
        call hvl_clases
        call sb_init                ; preparar el scrollback (y la bienvenida)
        call actualiza_prompt
        mov byte [cur_on], 1
        mov esi, term_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov ebp, [ven]              ; parpadeo del cursor
        mov ecx, 1
        mov edx, 500
        HV tmp_poner
        HV app_bucle
.x:
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
ter_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_VSCROLL
        je .vscroll
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_GETMINMAXINFO   ; lo más pequeño
        jne .nmin
        mov eax, 240 + 120 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], term_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, barra_plant        ; la barra de desplazamiento (hija)
        HV ven_crear
        mov [barra], eax
        call disponer
        jmp hecho
.tam:
        call disponer
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.vscroll:                           ; la barra se movió: ECX=posición nueva
        mov [desp], ecx
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.rueda:                             ; la rueda: se la pasamos a la barra
        cmp dword [barra_on], 0
        je hecho
        mov ebp, [barra]
        mov eax, WM_MOUSEWHEEL
        imul ecx, ecx, 3
        HV msg_enviar
        jmp hecho
.reloj:                             ; parpadeo del cursor
        xor byte [cur_on], 1
        call inval_entrada
        jmp hecho
.cerrar:
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
        jmp hecho

hecho:
        xor eax, eax
        ret

; ---- las órdenes del menú (WM_COMMAND: ECX=id) ----
orden:
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_LIMPIAR
        je .limpiar
        cmp ecx, O_ACERCA
        je .acerca
        jmp hecho
.limpiar:
        call cmd_cls
        call disponer
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp hecho
.salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp hecho

; ---- teclas que escriben un carácter (WM_CHAR: ECX=carácter) ----
letra:
        mov eax, ecx
        cmp al, 32
        jb hecho
        cmp dword [ent_len], ENT_MAX
        jae hecho
        mov ebx, [ent_len]
        mov [entrada+ebx], al
        inc dword [ent_len]
        mov eax, [ent_len]
        mov byte [entrada+eax], 0
        mov byte [cur_on], 1
        call inval_entrada
        jmp hecho

; ---- teclas de control (WM_KEYDOWN: ECX=código, EDX=estado de mayúsculas) ----
tecla:
        cmp ecx, TC_INTRO
        je .intro
        cmp ecx, TC_RETRO
        je .retro
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_AVPAG
        je .avpag
        cmp ecx, TC_CTRL + 12       ; Ctrl+L: limpiar
        je .cls
        jmp hecho
.intro:
        call al_enter
        jmp hecho
.retro:
        cmp dword [ent_len], 0
        je hecho
        dec dword [ent_len]
        mov eax, [ent_len]
        mov byte [entrada+eax], 0
        mov byte [cur_on], 1
        call inval_entrada
        jmp hecho
.arr:
        call hist_arriba
        mov byte [cur_on], 1
        call inval_entrada
        jmp hecho
.aba:
        call hist_abajo
        mov byte [cur_on], 1
        call inval_entrada
        jmp hecho
.repag:
        mov eax, [vis_rows]
        sub [desp], eax
        call disponer
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.avpag:
        mov eax, [vis_rows]
        add [desp], eax
        call disponer
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho
.cls:
        call cmd_cls
        call disponer
        mov ebp, [ven]
        HV ven_invalidar
        jmp hecho

; =============================================================================
;  Pintar
; =============================================================================
pintar:
        mov ebp, [ven]
        HV dc_pintar                ; borra el fondo en negro (C_TINTA)
        call calcula
        ; ---- el scrollback ----
        COLOR C_PIEDRA
        xor ecx, ecx                ; r = fila visible
.lf:
        cmp ecx, [vis_rows]
        jae .input
        mov eax, [desp]
        add eax, ecx                ; índice de línea
        cmp eax, [nlin]
        jae .nextrow
        imul eax, COLS
        lea esi, [sb+eax]
        push ecx
        mov eax, MARGEN
        mov ebx, ecx
        imul ebx, FILA
        mov ecx, [viscols]
        HV dc_texto_n
        pop ecx
.nextrow:
        inc ecx
        jmp .lf
.input:
        ; ---- la línea de entrada, abajo ----
        mov eax, [vis_rows]
        imul eax, FILA
        mov [in_y], eax
        ; el símbolo del sistema (en turquesa)
        COLOR C_TURQUESA
        mov eax, MARGEN
        mov ebx, [in_y]
        mov esi, promptbuf
        HV dc_texto
        mov esi, promptbuf
        call cad_largo
        mov [promptlen], eax
        ; cuántos caracteres de la entrada caben (dejando sitio al cursor)
        mov eax, [viscols]
        sub eax, [promptlen]
        dec eax
        jns .a1
        xor eax, eax
.a1:
        mov [avail], eax
        ; si no cabe toda la entrada, se enseña su final
        mov eax, [ent_len]
        cmp eax, [avail]
        jbe .s0
        sub eax, [avail]
        mov [instart], eax
        jmp .s1
.s0:
        mov dword [instart], 0
.s1:
        mov eax, [ent_len]
        sub eax, [instart]
        mov [inshow], eax
        mov esi, entrada
        add esi, [instart]
        COLOR C_BLANCO
        mov eax, [promptlen]
        imul eax, ANCHO_C
        add eax, MARGEN
        mov ebx, [in_y]
        mov ecx, [inshow]
        HV dc_texto_n
        ; el cursor (un bloque, si está encendido)
        cmp byte [cur_on], 0
        je .fin
        mov eax, [promptlen]
        add eax, [inshow]
        imul eax, ANCHO_C
        add eax, MARGEN
        mov ebx, [in_y]
        inc ebx
        COLOR C_PIEDRA
        mov ecx, ANCHO_C
        mov edx, FILA - 2
        HV dc_rect
.fin:
        jmp hecho

; (interna) calcula las medidas de la vista (sin tocar la barra)
calcula:
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_cli                  ; ECX=ancho EDX=alto
        mov [cli_an], ecx
        mov [cli_al], edx
        ; filas visibles = (alto - fila de entrada) / FILA
        mov eax, edx
        sub eax, FILA
        jns .v1
        xor eax, eax
.v1:
        xor edx, edx
        mov ebx, FILA
        div ebx
        test eax, eax
        jnz .v2
        mov eax, 1
.v2:
        mov [vis_rows], eax
        ; máximo desplazamiento = nlin - filas visibles (>= 0)
        mov eax, [nlin]
        sub eax, [vis_rows]
        jns .m1
        xor eax, eax
.m1:
        mov [max_scroll], eax
        xor ecx, ecx                ; ¿hace falta barra?
        test eax, eax
        jz .b1
        mov ecx, 1
.b1:
        mov [barra_on], ecx
        ; columnas visibles = (ancho - margen - barra) / 8, tope COLS
        mov eax, [cli_an]
        sub eax, MARGEN
        cmp dword [barra_on], 0
        je .c1
        sub eax, BD_AN
.c1:
        jns .c2
        xor eax, eax
.c2:
        shr eax, 3
        cmp eax, COLS
        jbe .c3
        mov eax, COLS
.c3:
        mov [viscols], eax
        ; acotar el desplazamiento a [0, max_scroll]
        mov eax, [desp]
        cmp eax, [max_scroll]
        jle .d1
        mov eax, [max_scroll]
        mov [desp], eax
.d1:
        cmp dword [desp], 0
        jge .x
        mov dword [desp], 0
.x:
        ret

; (interna) coloca y enseña (o esconde) la barra según calcula
disponer:
        call calcula
        mov ebp, [barra]
        test ebp, ebp
        jz .x
        cmp dword [barra_on], 0
        je .hide
        mov eax, [cli_an]
        sub eax, BD_AN
        xor ebx, ebx
        mov ecx, BD_AN
        mov edx, [vis_rows]
        imul edx, FILA
        HV ven_mover
        mov ebp, [barra]
        mov eax, BDM_PONER
        mov ecx, [max_scroll]
        mov edx, [vis_rows]
        HV msg_enviar
        mov ebp, [barra]
        mov eax, BDM_POS
        mov ecx, [desp]
        HV msg_enviar
        mov ebp, [barra]
        mov eax, 1
        HV ven_mostrar
        jmp .x
.hide:
        xor eax, eax
        HV ven_mostrar
.x:
        ret

; (interna) invalida solo la fila de la línea de entrada (para el parpadeo)
inval_entrada:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_cli                  ; ECX=ancho EDX=alto
        mov ebx, edx
        sub ebx, FILA
        jns .r1
        xor ebx, ebx
.r1:
        xor eax, eax
        mov edx, FILA
        HV ven_invalidar_rect       ; EAX=x EBX=y ECX=ancho EDX=alto
.x:
        popad
        ret

; =============================================================================
;  Ejecutar la línea
; =============================================================================
; Se llama al pulsar Intro: eco del símbolo + la orden, se ejecuta, y se sigue
; el fondo del scrollback.
al_enter:
        mov eax, [ent_len]
        mov byte [entrada+eax], 0
        call actualiza_prompt
        mov esi, promptbuf          ; eco: «C:\RUTA>orden»
        call escribe_sb
        mov esi, entrada
        call escribe_sb
        call sb_newline_i
        cmp dword [ent_len], 0
        je .run
        mov esi, entrada            ; al historial
        call hist_add
.run:
        mov esi, entrada
        call ejecuta_linea
        mov dword [ent_len], 0      ; vaciar la entrada
        mov byte [entrada], 0
        call actualiza_prompt       ; CD pudo cambiar la carpeta
        call calcula
        mov eax, [max_scroll]       ; seguir el fondo
        mov [desp], eax
        call disponer
        mov eax, [hist_cnt]         ; el historial, al final
        mov [hist_nav], eax
        mov byte [cur_on], 1
        mov ebp, [ven]
        HV ven_invalidar
        ret

; ejecuta_linea(ESI = cadena acabada en 0): parte la línea y despacha la orden.
; La salida la escribe en el scrollback. No depende de quién trae la cadena.
ejecuta_linea:
        call salta_esp
        cmp byte [esi], 0
        je .fin                     ; vacía
        mov edi, palabra            ; la primera palabra (en mayúsculas)
        mov ecx, 15
.w:
        mov al, [esi]
        test al, al
        jz .we
        cmp al, ' '
        je .we
        cmp al, 9
        je .we
        call upcase_al
        mov [edi], al
        inc edi
        inc esi
        dec ecx
        jnz .w
.wd:                                ; (drenar si la palabra es más larga)
        mov al, [esi]
        test al, al
        jz .we
        cmp al, ' '
        je .we
        cmp al, 9
        je .we
        inc esi
        jmp .wd
.we:
        mov byte [edi], 0
        call salta_esp              ; ESI en el primer argumento
        mov [arg_ptr], esi
        ; ¿cambio de unidad «X:»?
        cmp byte [palabra+2], 0
        jne .disp
        cmp byte [palabra+1], ':'
        jne .disp
        call cmd_unidad
        ret
.disp:
        mov edi, cmd_tabla
.d:
        mov eax, [edi]
        test eax, eax
        jz .desc
        mov esi, palabra
        push edi
        mov edi, eax
        call cad_igual_cf           ; CF=1 si iguales (ESI intacto)
        pop edi
        jc .hit
        add edi, 8
        jmp .d
.hit:
        call [edi+4]
        ret
.desc:
        mov esi, m_desconocido
        call escribe_linea
.fin:
        ret

; =============================================================================
;  Las órdenes
; =============================================================================
; DIR [patrón]
cmd_dir:
        mov dword [file_cnt], 0
        mov dword [dir_cnt], 0
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        jne .arg
        mov esi, s_estrella
        jmp .go
.arg:
        mov edi, patbuf
        call copia_token
        mov esi, patbuf
.go:
        mov edi, dta
        mov ecx, 0x16               ; también carpetas, ocultos y de sistema
        call d_primero
        jnc .loop
        mov esi, m_no_arch
        call escribe_linea
        ret
.loop:
        mov edi, tmp                ; «NOMBRE        <DIR>» o «NOMBRE          1234»
        mov esi, dta + DTA_NOMBRE
        call hvl_cad
.pad:
        mov eax, edi
        sub eax, tmp
        cmp eax, 14
        jae .padf
        mov byte [edi], ' '
        inc edi
        jmp .pad
.padf:
        test byte [dta + DTA_ATR], 0x10
        jz .size
        mov esi, s_dir
        call hvl_cad
        inc dword [dir_cnt]
        jmp .emit
.size:
        mov eax, [dta + DTA_TAMANO]
        call hvl_num
        inc dword [file_cnt]
.emit:
        mov byte [edi], 0
        mov esi, tmp
        call escribe_linea
        mov edi, dta
        call d_siguiente
        jnc .loop
        ; el pie: «  N archivo(s), M carpeta(s)»
        mov edi, tmp
        mov esi, s_2esp
        call hvl_cad
        mov eax, [file_cnt]
        call hvl_num
        mov esi, s_arch
        call hvl_cad
        mov eax, [dir_cnt]
        call hvl_num
        mov esi, s_carp
        call hvl_cad
        mov byte [edi], 0
        mov esi, tmp
        call escribe_linea
        ret

; CD / CHDIR
cmd_cd:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        jne .ir
        mov edi, promptbuf          ; sin argumento: enseñar la carpeta actual
        HV dos_dir_actual
        mov esi, promptbuf
        call escribe_linea
        ret
.ir:
        mov edi, nombre
        call copia_token
        mov esi, nombre
        HV dos_ir_carpeta
        jnc .x
        mov esi, m_no_ruta
        call escribe_linea
.x:
        ret

; cambio de unidad «X:»
cmd_unidad:
        mov esi, palabra
        HV dos_ir_carpeta
        jnc .x
        mov esi, m_no_unidad
        call escribe_linea
.x:
        ret

; TYPE archivo
cmd_type:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        mov esi, nombre
        xor al, al
        HV dos_abrir
        jc .noabrir
        mov [h_in], eax
.l:
        mov ebx, [h_in]
        mov ecx, BUFIO
        mov edi, iobuf
        HV dos_leer
        test eax, eax
        jz .fin
        mov ecx, eax
        mov esi, iobuf
.pc:
        mov al, [esi]
        push ecx
        push esi
        call sb_putc
        pop esi
        pop ecx
        inc esi
        dec ecx
        jnz .pc
        jmp .l
.fin:
        mov ebx, [h_in]
        HV dos_cerrar
        cmp dword [col], 0          ; terminar la última línea si quedó a medias
        je .x
        call sb_newline_i
.x:
        ret
.noabrir:
        mov esi, m_no_abrir
        call escribe_linea
        ret
.falta:
        mov esi, m_falta_nombre
        call escribe_linea
        ret

; COPY origen destino
cmd_copy:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre2
        call copia_token
        mov esi, nombre
        xor al, al
        HV dos_abrir
        jc .noabrir
        mov [h_in], eax
        mov esi, nombre2
        HV dos_crear
        jc .nocrear
        mov [h_out], eax
.l:
        mov ebx, [h_in]
        mov ecx, BUFIO
        mov edi, iobuf
        HV dos_leer
        test eax, eax
        jz .fin
        mov ecx, eax
        mov ebx, [h_out]
        mov esi, iobuf
        HV dos_escribir
        jmp .l
.fin:
        mov ebx, [h_in]
        HV dos_cerrar
        mov ebx, [h_out]
        HV dos_cerrar
        mov esi, m_copiado
        call escribe_linea
        ret
.nocrear:
        mov ebx, [h_in]
        HV dos_cerrar
.noabrir:
        mov esi, m_no_copia
        call escribe_linea
        ret
.falta:
        mov esi, m_faltan
        call escribe_linea
        ret

; DEL / BORRAR archivo
cmd_del:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        mov esi, nombre
        call d_borrar
        jnc .x
        mov esi, m_no_borra
        call escribe_linea
.x:
        ret
.falta:
        mov esi, m_falta_nombre
        call escribe_linea
        ret

; MD / MKDIR carpeta
cmd_md:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        mov esi, nombre
        call d_crear_dir
        jnc .x
        mov esi, m_no_crea_dir
        call escribe_linea
.x:
        ret
.falta:
        mov esi, m_falta_nombre
        call escribe_linea
        ret

; RD / RMDIR carpeta
cmd_rd:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        mov esi, nombre
        call d_quitar_dir
        jnc .x
        mov esi, m_no_quita_dir
        call escribe_linea
.x:
        ret
.falta:
        mov esi, m_falta_nombre
        call escribe_linea
        ret

; REN / RENOMBRAR viejo nuevo
cmd_ren:
        mov esi, [arg_ptr]
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre
        call copia_token
        call salta_esp
        cmp byte [esi], 0
        je .falta
        mov edi, nombre2
        call copia_token
        mov esi, nombre
        mov edi, nombre2
        call d_renombrar
        jnc .x
        mov esi, m_no_renombra
        call escribe_linea
.x:
        ret
.falta:
        mov esi, m_faltan
        call escribe_linea
        ret

; VER
cmd_ver:
        mov esi, m_version
        call escribe_linea
        ret

; ECHO texto
cmd_echo:
        mov esi, [arg_ptr]
        call escribe_linea
        ret

; AYUDA / HELP
cmd_ayuda:
        mov esi, m_ayuda
        call escribe_sb
        ret

; CLS (también desde el menú y Ctrl+L): vaciar el scrollback
cmd_cls:
        push eax
        push ecx
        push edi
        mov edi, sb
        mov ecx, COLS
        mov al, ' '
        rep stosb
        mov dword [nlin], 1
        mov dword [col], 0
        mov dword [desp], 0
        pop edi
        pop ecx
        pop eax
        ret

; =============================================================================
;  El área de texto (scrollback)
; =============================================================================
; prepara el scrollback entero a espacios y escribe la bienvenida
sb_init:
        push eax
        push ecx
        push edi
        mov edi, sb
        mov ecx, TOT * COLS
        mov al, ' '
        rep stosb
        mov dword [nlin], 1
        mov dword [col], 0
        mov dword [desp], 0
        pop edi
        pop ecx
        pop eax
        mov esi, m_bienvenida
        call escribe_linea
        mov esi, m_bienvenida2
        call escribe_linea
        ret

; escribe ESI (acabada en 0) en el scrollback
escribe_sb:
        push eax
        push esi
.c:
        mov al, [esi]
        test al, al
        jz .x
        call sb_putc
        inc esi
        jmp .c
.x:
        pop esi
        pop eax
        ret

; escribe ESI y salta de línea
escribe_linea:
        call escribe_sb
        call sb_newline_i
        ret

; AL = un carácter al scrollback (10 salta de línea, 13 se ignora, 9 tabula)
sb_putc:
        push eax
        push ebx
        push ecx
        push edi
        mov bl, al
        cmp bl, 13
        je .x
        cmp bl, 10
        je .nl
        cmp bl, 9
        je .tab
        cmp bl, 32
        jae .pon
        mov bl, ' '                 ; otros controles -> espacio
.pon:
        mov eax, [col]
        cmp eax, COLS
        jb .pon1
        call sb_newline_i
        mov eax, [col]
.pon1:
        mov edi, [nlin]
        dec edi
        imul edi, COLS
        add edi, eax
        add edi, sb
        mov [edi], bl
        inc dword [col]
        jmp .x
.tab:
        mov bl, ' '
.tabl:
        mov eax, [col]
        cmp eax, COLS
        jb .tab1
        call sb_newline_i
        mov eax, [col]
.tab1:
        mov edi, [nlin]
        dec edi
        imul edi, COLS
        add edi, eax
        add edi, sb
        mov byte [edi], ' '
        inc dword [col]
        mov eax, [col]
        and eax, 7
        jnz .tabl
        jmp .x
.nl:
        call sb_newline_i
.x:
        pop edi
        pop ecx
        pop ebx
        pop eax
        ret

; salta de línea: rellena el resto con espacios, avanza y limpia la nueva
sb_newline_i:
        push eax
        push ecx
        push edi
        mov edi, [nlin]
        dec edi
        imul edi, COLS
        add edi, sb
        add edi, [col]
        mov ecx, COLS
        sub ecx, [col]
        mov al, ' '
        rep stosb
        mov dword [col], 0
        mov eax, [nlin]
        cmp eax, TOT
        jb .adv
        call sb_shift
        jmp .clr
.adv:
        inc dword [nlin]
.clr:
        mov edi, [nlin]
        dec edi
        imul edi, COLS
        add edi, sb
        mov ecx, COLS
        mov al, ' '
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret

; desplaza todas las líneas una hacia arriba (tira la más vieja); nlin=TOT
sb_shift:
        push esi
        push edi
        push ecx
        mov esi, sb + COLS
        mov edi, sb
        mov ecx, (TOT - 1) * COLS
        rep movsb
        pop ecx
        pop edi
        pop esi
        ret

; =============================================================================
;  El símbolo del sistema y el historial
; =============================================================================
; promptbuf = «C:\RUTA>» de la carpeta actual
actualiza_prompt:
        push esi
        push edi
        mov edi, promptbuf
        HV dos_dir_actual
.e:
        cmp byte [edi], 0
        je .e1
        inc edi
        jmp .e
.e1:
        mov byte [edi], '>'
        mov byte [edi+1], 0
        pop edi
        pop esi
        ret

; guarda ESI (acabada en 0) en el historial
hist_add:
        push eax
        push ecx
        push edi
        push esi
        mov eax, [hist_cnt]
        cmp eax, HIST_N
        jb .ns
        push esi                    ; lleno: tira la más vieja
        mov esi, hist + HIST_L
        mov edi, hist
        mov ecx, (HIST_N - 1) * HIST_L
        rep movsb
        pop esi
        mov dword [hist_cnt], HIST_N - 1
.ns:
        mov eax, [hist_cnt]
        imul eax, HIST_L
        lea edi, [hist + eax]
        mov ecx, HIST_L - 1
.c:
        mov al, [esi]
        test al, al
        jz .f
        mov [edi], al
        inc edi
        inc esi
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        inc dword [hist_cnt]
        pop esi
        pop edi
        pop ecx
        pop eax
        ret

; flecha arriba: la orden anterior
hist_arriba:
        mov eax, [hist_nav]
        test eax, eax
        jle .x
        dec eax
        mov [hist_nav], eax
        call hist_carga
.x:
        ret

; flecha abajo: la siguiente (o la línea vacía)
hist_abajo:
        mov eax, [hist_nav]
        cmp eax, [hist_cnt]
        jge .x
        inc eax
        mov [hist_nav], eax
        cmp eax, [hist_cnt]
        je .vacio
        call hist_carga
        ret
.vacio:
        mov dword [ent_len], 0
        mov byte [entrada], 0
.x:
        ret

; carga hist[hist_nav] en la entrada
hist_carga:
        push eax
        push ecx
        push esi
        push edi
        mov eax, [hist_nav]
        imul eax, HIST_L
        lea esi, [hist + eax]
        mov edi, entrada
        xor ecx, ecx
.c:
        mov al, [esi]
        test al, al
        jz .f
        cmp ecx, ENT_MAX
        jae .f
        mov [edi], al
        inc edi
        inc esi
        inc ecx
        jmp .c
.f:
        mov byte [edi], 0
        mov [ent_len], ecx
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; =============================================================================
;  Utilidades de cadenas
; =============================================================================
; ESI: salta espacios y tabuladores
salta_esp:
        push eax
.l:
        mov al, [esi]
        cmp al, ' '
        je .n
        cmp al, 9
        je .n
        pop eax
        ret
.n:
        inc esi
        jmp .l

; copia un token de [ESI] a [EDI] (0 al final); para en espacio, tabulador o 0
copia_token:
        push eax
.c:
        mov al, [esi]
        test al, al
        jz .f
        cmp al, ' '
        je .f
        cmp al, 9
        je .f
        mov [edi], al
        inc edi
        inc esi
        jmp .c
.f:
        mov byte [edi], 0
        pop eax
        ret

; AL a mayúscula (a-z)
upcase_al:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .x
        sub al, 0x20
.x:
        ret

; ESI, EDI (0-term): CF=1 si son iguales (ESI y EDI intactos)
cad_igual_cf:
        push esi
        push edi
.l:
        mov al, [esi]
        mov ah, [edi]
        cmp al, ah
        jne .no
        test al, al
        jz .si
        inc esi
        inc edi
        jmp .l
.si:
        pop edi
        pop esi
        stc
        ret
.no:
        pop edi
        pop esi
        clc
        ret

; ESI (0-term) -> EAX = largo
cad_largo:
        push esi
        xor eax, eax
.c:
        cmp byte [esi], 0
        je .x
        inc esi
        inc eax
        jmp .c
.x:
        pop esi
        ret

%include "dos.inc"                  ; las llamadas a DOS por rm_interrup
%include "hvplib.inc"

        section .bss
        alignb 4
param       resd 1
dbuf        resd 1                  ; el búfer de paso de Hispa Ventanas
rl          resb RL_TAM             ; los registros de rm_interrup
ven         resd 1                  ; la ventana del Terminal
barra       resd 1                  ; la barra de desplazamiento (hija)
cli_an      resd 1
cli_al      resd 1
vis_rows    resd 1                  ; filas de texto visibles
viscols     resd 1                  ; columnas visibles
max_scroll  resd 1                  ; desplazamiento máximo
barra_on    resd 1                  ; 1 si la barra se enseña
desp        resd 1                  ; línea de arriba del scrollback
nlin        resd 1                  ; líneas usadas del scrollback
col         resd 1                  ; columna actual (en la última línea)
cur_on      resd 1                  ; el cursor está encendido (parpadeo)
ent_len     resd 1                  ; largo de la línea de entrada
hist_cnt    resd 1                  ; órdenes guardadas
hist_nav    resd 1                  ; navegación del historial
file_cnt    resd 1                  ; (DIR) archivos y carpetas contados
dir_cnt     resd 1
h_in        resd 1                  ; manejadores de archivo (TYPE, COPY)
h_out       resd 1
arg_ptr     resd 1                  ; los argumentos de la orden
in_y        resd 1                  ; (pintar) coordenadas de la entrada
promptlen   resd 1
avail       resd 1
instart     resd 1
inshow      resd 1
palabra     resb 20                 ; la orden (primera palabra)
nombre      resb 80
nombre2     resb 80
patbuf      resb 80
promptbuf   resb 96
tmp         resb 160                ; para armar líneas de salida
entrada     resb ENT_MAX + 2        ; la línea que se teclea
dta         resb DTA_TAM            ; la zona de búsqueda (DIR)
iobuf       resb BUFIO              ; lectura/escritura (TYPE, COPY)
hist        resb HIST_N * HIST_L    ; el historial
sb          resb TOT * COLS         ; el scrollback

HVP_FIN
