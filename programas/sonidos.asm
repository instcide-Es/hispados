; =============================================================================
;  SONIDOS.HVP - Reproductor de sonido de Hispa Ventanas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Para saber enseguida si un equipo suena. Arriba dice qué tarjeta lleva el
;  driver HSPSND (o avisa de que no está cargado o de que no ha encontrado
;  ninguna). Debajo, los botones:
;    Sonido de prueba   toca HISPA.WAV (\HISPADOS; una melodía corta hecha
;                       por tools/mksample.py)
;    Izquierda, Derecha el mismo sonido por un solo altavoz (con una tarjeta
;                       mono lo dice y suena igual por los dos)
;    Tono 440 Hz        dos segundos de un «la», hecho aquí (no necesita
;                       ningún archivo)
;    Abrir WAV...       cualquier WAV PCM de 8 o 16 bits, mono o estéreo, de
;                       8.000 a 48.000 Hz; si no lo admite, dice por qué
;    Parar
;  y el volumen (el mismo del altavoz de la barra: aj_leer y aj_poner) con su
;  casilla de silencio. Abajo, por dónde va el sonido y la barra de estado.
;
;  Cómo suena sin que la ventana se quede esperando: HSPSND toca trozos uno
;  detrás de otro (tiene sitio para cuatro) y, si no le cabe otro, hace
;  esperar a quien se lo manda. Aquí el sonido entero está en memoria (un
;  bloque de memoria extendida; el archivo se lee a trozos, sin parar la
;  ventana) y se manda por trozos de la medida exacta que la tarjeta toca de
;  una vez, ya en su formato: 2.048 tramas de 16 bits en estéreo a 48.000 Hz
;  con HD Audio y AC'97, 1.024 con una Sound Blaster 16 y 4.096 muestras de 8
;  bits con las Sound Blaster anteriores (así HSPSND no mete silencios ni
;  cambia la frecuencia: el paso a 48.000 Hz se hace aquí, interpolando).
;  Antes de mandar un trozo se pregunta a HSPSND si cabe (su función 08h, de
;  la versión 3): si no, se deja para la vuelta siguiente; nunca se espera.
;  Con un HSPSND anterior se lleva la cuenta con el reloj (el contador 8254)
;  de cuánto queda en la cola y solo se manda otro cuando quedan como mucho
;  tres. El mismo reloj dice cuándo ha acabado de sonar: detrás del sonido
;  va un tercio de segundo de silencio y entonces se manda parar a HSPSND
;  (HD Audio da vueltas a sus cuatro trozos: si no, repetiría los últimos). Mientras suena o carga, el bucle mira los mensajes y cede el turno
;  (msg_mirar y ceder); parado, espera un mensaje.
;
;  Todo el diseño es propio: la ventana, el deslizador, el icono, los textos
;  y los sonidos. Es un módulo .HVP (ventanas/hvp.inc) con la interfaz de 80
;  entradas: snd_info, snd_pcm2, snd_orden y snd_campana (docs/VENTANAS.md).
; =============================================================================
%include "hvp.inc"

CLI_AN      equ 480                 ; la zona de la ventana
CLI_AL      equ 326
ID_PRUEBA   equ 10
ID_IZQ      equ 11
ID_DER      equ 12
ID_TONO     equ 13
ID_PARAR    equ 14
ID_ABRIR    equ 15
ID_TARJETA  equ 20
ID_DETALLE  equ 21
ID_VOL      equ 30
ID_VOL_TXT  equ 31
ID_MUDO     equ 32
ID_ESTADO   equ 40
ID_FIJO     equ 99
PROG_X      equ 14                  ; la barra de por dónde va
PROG_Y      equ 282
PROG_AN     equ 452
PROG_AL     equ 9
TICS_SEG    equ 1193182             ; tics del contador 8254 por segundo
CARGA_PASO  equ 16384               ; octetos del archivo que se leen de una vez
TONO_HZ     equ 22000               ; el tono: 50 muestras por ciclo de 440 Hz
TONO_N      equ 2 * TONO_HZ         ; dos segundos
TONO_RAMPA  equ 220                 ; entra y sale en 10 ms, sin chasquido
E_NADA      equ 0                   ; [estado]
E_CARGA     equ 1                   ; leyendo el archivo
E_SUENA     equ 2                   ; mandando trozos
E_ACABA     equ 3                   ; mandado: falta el silencio de detrás y que acabe

HVP_CABECERA "Reproductor de sonido", entrada, 2  ; [MEZCLADOR] grupo «Sonido»
        db "................................"
        db "................1..............."
        db "................1..............."
        db "...............11..............."
        db "..............1a1..............."
        db ".............1aa1........2......"
        db ".............1aa1........22....."
        db "............1aaa1.........22...."
        db "...........1aaaa1.....cc..22...."
        db "...........aaaaa1.....cc...22..."
        db "..........1aaaaa1.cc...cc..22..."
        db "...1111111aaaaaa1..cc..cc...2..."
        db "...144444aaaaaaa1...c...c...2..."
        db "...144444aaaaaaa1...c...cc..22.."
        db "...144444aaaaaaa1...cc..cc..22.."
        db "...144444aaaaaaa1...cc..cc..22.."
        db "...144444aaaaaaa1...cc..cc..22.."
        db "...144444aaaaaaa1...cc..cc..22.."
        db "...144444aaaaaaa1...c...cc..22.."
        db "...144444aaaaaaa1...c...c...2..."
        db "...1111111aaaaaa1..cc..cc...2..."
        db "..........1aaaaa1.cc...cc..22..."
        db "...........aaaaa1.....cc...22..."
        db "...........1aaaa1.....cc..22...."
        db "............1aaa1.........22...."
        db ".............1aa1........22....."
        db ".............1aa1........2......"
        db "..............1a1..............."
        db "...............11..............."
        db "................1..............."
        db "................1..............."
        db "................................"
HVP_CODIGO

; ---- clases ----
c_sonidos   dd son_proc
            db C_CREMA, 0, 0, 0
c_desliza   dd des_proc
            db C_CREMA, CB_FOCO, 0, 0

%define EST HV_c_estatico
%define BOT HV_c_boton
H           equ E_HIJA
HT          equ E_HIJA | E_TABULA

son_plant:
        PLANTILLA c_sonidos, t_titulo, E_TITULO | E_CERRAR | E_MINIM, 0, 0, \
                  CLI_AN + 2*BORDE, CLI_AL + 2*BORDE + TIT_H + 1, 0
pl_hijos:
        PLANTILLA EST, tarj_txt, H | SS_NEGRITA, 14, 8, 452, 18, ID_TARJETA
pl_det:
        PLANTILLA EST, 0, H, 14, 28, 452, 36, ID_DETALLE
        PLANTILLA BOT, t_b_prueba, HT | BS_DEFECTO, 14, 78, 452, 40, ID_PRUEBA
        PLANTILLA BOT, t_b_izq, HT, 14, 126, 220, 32, ID_IZQ
        PLANTILLA BOT, t_b_der, HT, 246, 126, 220, 32, ID_DER
        PLANTILLA BOT, t_b_tono, HT, 14, 164, 220, 32, ID_TONO
        PLANTILLA BOT, t_b_abrir, HT, 246, 164, 220, 32, ID_ABRIR
        PLANTILLA BOT, t_b_parar, HT, 14, 202, 452, 32, ID_PARAR
        PLANTILLA EST, t_vol, H, 14, 252, 72, 18, ID_FIJO
        PLANTILLA c_desliza, 0, HT, 88, 250, 220, 21, ID_VOL
        PLANTILLA EST, vol_txt, H, 314, 252, 56, 18, ID_VOL_TXT
        PLANTILLA BOT, t_mudo, HT | BS_CASILLA, 376, 251, 92, 20, ID_MUDO
        PLANTILLA EST, est_txt, H | SS_HUNDIDO, 8, 298, 464, 22, ID_ESTADO
        dd 0

; ---- textos ----
t_titulo    db "Reproductor de sonido", 0
t_b_prueba  db "Sonido de prueba", 0
t_b_izq     db "Izquierda", 0
t_b_der     db "Derecha", 0
t_b_tono    db "Tono 440 Hz", 0
t_b_abrir   db "Abrir WAV...", 0
t_b_parar   db "Parar", 0
t_vol       db "Volumen:", 0
t_mudo      db "Silencio", 0
t_abrir     db "Abrir un sonido WAV", 0
n_hispa     db "HISPA.WAV", 0
t_tarjeta   db "Tarjeta: ", 0
t_no_hsp    db "HSPSND no está cargado: no hay sonido.", 0
t_no_tarj   db "HSPSND no ha encontrado ninguna tarjeta de sonido.", 0
; lo que se dice de cada tarjeta (por su número, de -1 a 4)
detalles    dd t_d_nohsp, t_d_notarj, t_d_sb, t_d_sb16, t_d_48, t_d_48
t_d_nohsp   db "Cargue HSPSND antes de abrir Hispa Ventanas: salga,", 10
            db "escriba HSPSND y vuelva a entrar con VENTANAS.", 0
t_d_notarj  db "La orden HSPSND /INFO (fuera de Hispa Ventanas) dice", 10
            db "qué ha buscado y qué ha visto.", 0
t_d_sb      db "Mono, de 8 bits, hasta 22.050 Hz: no distingue", 10
            db "izquierda y derecha.", 0
t_d_sb16    db "Estéreo, de 16 bits, hasta 44.100 Hz.", 0
t_d_48      db "Estéreo, de 16 bits. Todo se le da a 48.000 Hz.", 0
t_listo     db "Listo.", 0
t_sonando   db "Sonando... ", 0
t_terminado db "Terminado.", 0
t_parado    db "Parado.", 0
t_nada      db "No hay nada sonando.", 0
t_cargando  db "Cargando... ", 0
t_pct       db " %", 0
t_q_prueba  db "sonido de prueba", 0
t_q_izq     db "solo por la izquierda", 0
t_q_der     db "solo por la derecha", 0
t_q_tono    db "tono de 440 Hz (2 segundos)", 0
t_q_mono    db "la tarjeta es mono: igual por los dos lados", 0
t_hz        db " Hz, ", 0
t_b8        db "8 bits, ", 0
t_b16       db "16 bits, ", 0
t_mono      db "mono)", 0
t_estereo   db "estéreo)", 0
t_sin_hsp   db "No hay sonido: HSPSND no está cargado.", 0
t_sin_tarj  db "No hay sonido: HSPSND no ha encontrado tarjeta.", 0
t_e_abrir   db "No se puede abrir el archivo.", 0
t_e_hispa   db "No está HISPA.WAV: pruebe con el tono de 440 Hz.", 0
t_e_nowav   db "No es un archivo WAV.", 0
t_e_noadm   db "No se admite: solo WAV PCM de 8 o 16 bits, 8000-48000 Hz", 0
t_e_vacio   db "El archivo WAV no tiene sonido.", 0
t_e_mem     db "No hay memoria para un sonido tan largo.", 0
; un ciclo del tono: 50 muestras de un seno (a 22.000 Hz, 440 Hz justos)
seno        dw 0, 2464, 4889, 7237, 9471, 11556, 13458, 15148, 16599, 17789
            dw 18698, 19312, 19621, 19621, 19312, 18698, 17789, 16599, 15148, 13458
            dw 11556, 9471, 7237, 4889, 2464, 0, -2464, -4889, -7237, -9471
            dw -11556, -13458, -15148, -16599, -17789, -18698, -19312, -19621, -19621, -19312
            dw -18698, -17789, -16599, -15148, -13458, -11556, -9471, -7237, -4889, -2464

; =============================================================================
;  La entrada y el bucle
; =============================================================================
entrada:                                ; EBX=base ESI=ruta EDI=parámetros
        call rutas
        call cero_busca
        mov esi, pl_hijos
        call hvl_clases
        call tarjeta_mira
        mov esi, son_plant              ; la ventana, centrada
        mov edi, plant
        mov ecx, P_TAM / 4
        rep movsd
        HV pantalla
        sub eax, [plant+P_AN]
        sar eax, 1
        mov [plant+P_X], eax
        sub ebx, BARRA_H + SOMBRA
        sub ebx, [plant+P_AL]
        sar ebx, 1
        jns .y
        xor ebx, ebx
.y:
        add ebx, BARRA_H
        mov [plant+P_Y], ebx
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz near .fin
        mov ebp, eax
        mov esi, pl_hijos
        HV ven_lista
        or dword [v_pct], -1            ; (para que vol_mira lo ponga todo)
        call vol_mira
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
bucle:
        mov edi, msg
        cmp dword [estado], E_NADA
        jne .activo
        HV msg_tomar                    ; parado: a esperar un mensaje
        test eax, eax
        jz .x
        HV msg_despachar
        jmp bucle
.activo:
        HV msg_mirar
        jc .paso
        cmp dword [msg+M_MSG], WM_QUIT
        je .x
        HV msg_despachar
        jmp bucle
.paso:
        call paso
        HV ceder
        jmp bucle
.x:
        call parar
        call src_soltar
        mov edx, [tono_mango]
        cmp dword [tono_ptr], 0
        je entrada.fin
        HV mem_bloque_soltar
entrada.fin:
        ret

; ESI=la ruta del módulo: de ella salen las dos de HISPA.WAV: en la carpeta de
; encima (\HISPADOS; ruta_a) y al lado del módulo (ruta_b)
rutas:
        pushad
        mov edi, ruta_b
        xor ebx, ebx                    ; detrás de la última «\»
        xor edx, edx                    ; y de la anterior
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c1
        mov edx, ebx
        mov ebx, edi
.c1:
        test al, al
        jnz .c
        test ebx, ebx
        jnz .b
        mov ebx, ruta_b
.b:
        test edx, edx
        jnz .a
        mov edx, ebx
.a:
        mov esi, ruta_b
        mov edi, ruta_a
        mov ecx, edx
        sub ecx, esi
        rep movsb
        mov esi, n_hispa
        call hvl_cad
        mov byte [edi], 0
        mov edi, ebx
        mov esi, n_hispa
        call hvl_cad
        mov byte [edi], 0
        popad
        ret

; el puntero a la dirección lineal 0 (para el reloj de la BIOS)
cero_busca:
        pushad
        mov ebx, ds
        mov edi, desc
        mov eax, 0x000B
        int 0x31
        mov eax, [desc+2]
        and eax, 0x00FFFFFF
        movzx ebx, byte [desc+7]
        shl ebx, 24
        or eax, ebx
        neg eax
        mov [cero], eax
        popad
        ret

; ¿qué tarjeta lleva HSPSND? Deja los dos textos de arriba y el del estado
tarjeta_mira:
        pushad
        xor edx, edx
        HV snd_info                     ; -> EAX=tarjeta (-1: no está) ESI=nombre
        mov [hsp_ver], edx              ;    EDX=versión de la interfaz de HSPSND
        cmp eax, 4
        jle .t
        mov eax, 4                      ; (una que no se conoce: como la mejor)
.t:
        mov [tarjeta], eax
        mov ebx, [detalles+eax*4+4]
        mov [pl_det+P_TEXTO], ebx
        mov edi, tarj_txt
        test eax, eax
        jg .hay
        mov esi, t_no_tarj
        mov ebx, t_sin_tarj
        jz .no
        mov esi, t_no_hsp
        mov ebx, t_sin_hsp
.no:
        call hvl_cad
        mov byte [edi], 0
        mov esi, ebx
        jmp .est
.hay:
        push esi
        mov esi, t_tarjeta
        call hvl_cad
        pop esi
        mov ecx, 40
.n:
        lodsb
        test al, al
        jz .nf
        stosb
        dec ecx
        jnz .n
.nf:
        mov byte [edi], 0
        mov esi, t_listo
.est:
        mov edi, est_txt
        call hvl_cad
        mov byte [edi], 0
        popad
        ret

; =============================================================================
;  La ventana
; =============================================================================
son_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_COMMAND
        je ordenes
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_ICONO], hvp_icono
        jmp .hecho
.destruir:
        call parar
        mov dword [ven], 0
        HV msg_salir
        jmp .hecho
.reloj:
        call vol_mira                   ; (por si lo cambian en la barra)
        jmp .hecho
.tecla:
        cmp ecx, TC_ESC
        je .esc
        HVJ ven_defecto
.esc:
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.pintar:
        HV dc_pintar
        COLOR C_ARENA_OSC               ; dos rayas que separan las tres partes
        mov eax, 8
        mov ebx, 70
        mov ecx, CLI_AN - 16
        mov edx, 1
        HV dc_rect
        mov ebx, 242
        HV dc_rect
        COLOR C_BLANCO                  ; por dónde va: lo tocado, en añil
        mov eax, PROG_X
        mov ebx, PROG_Y
        mov ecx, PROG_AN
        mov edx, PROG_AL
        HV dc_rect
        mov ecx, [prog_px]
        test ecx, ecx
        jz .p1
        COLOR C_ANIL
        HV dc_rect
.p1:
        COLOR C_MARRON
        mov ecx, PROG_AN
        HV dc_marco
        HV dc_cerrar
.hecho:
        xor eax, eax
        ret

; WM_COMMAND: ECX = id (+ aviso*65536), EDX = el control
ordenes:
        movzx ecx, cx
        cmp ecx, ID_PRUEBA
        jne .o1
        xor al, al
        mov esi, t_q_prueba
        jmp .prueba
.o1:
        cmp ecx, ID_IZQ
        jne .o2
        mov al, 1
        mov esi, t_q_izq
        jmp .prueba
.o2:
        cmp ecx, ID_DER
        jne .o3
        mov al, 2
        mov esi, t_q_der
.prueba:
        call prueba
        jmp .hecho
.o3:
        cmp ecx, ID_TONO
        jne .o4
        call tono
        jmp .hecho
.o4:
        cmp ecx, ID_PARAR
        jne .o5
        mov esi, t_nada
        cmp dword [estado], E_NADA
        je .p
        mov esi, t_parado
.p:
        call parar
        call estado_pon
        jmp .hecho
.o5:
        cmp ecx, ID_ABRIR
        jne .o6
        call abrir
        jmp .hecho
.o6:
        cmp ecx, ID_VOL
        jne .o7
        push dword [edx+V_D3]
        mov edx, [edx+V_D0]
        mov [v_pct], edx
        mov eax, AJ_VOLUMEN
        HV aj_poner
        call vol_texto
        pop eax
        test eax, eax                   ; arrastrando: la campanita, al soltar
        jnz .hecho
        call campana
        jmp .hecho
.o7:
        cmp ecx, ID_MUDO
        jne .hecho
        call hija
        push ebp
        mov ebp, eax
        mov eax, BM_MARCA
        HV msg_enviar
        pop ebp
        mov edx, eax
        mov [v_mudo], edx
        mov eax, AJ_SILENCIO
        HV aj_poner
        call campana
.hecho:
        xor eax, eax
        ret

; la campanita del volumen, si no está sonando otra cosa (que ya se oye)
campana:
        cmp dword [estado], E_NADA
        jne .x
        HV snd_campana
.x:
        ret

; ECX=id -> EAX=el control (0 si no está)
hija:
        push ebp
        mov ebp, [ven]
        HV ven_hija_id
        pop ebp
        ret

; ESI=texto: a la barra de estado (se copia: puede ser un búfer que cambia)
estado_pon:
        pushad
        cmp esi, est_txt
        je .p
        mov edi, est_txt
        call hvl_cad
        mov byte [edi], 0
.p:
        mov ecx, ID_ESTADO
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        mov esi, est_txt
        HV ven_texto
.x:
        popad
        ret

; la barra de por dónde va: EAX=puntos en añil (de 0 a PROG_AN)
prog_pon:
        cmp eax, [prog_px]
        je .x
        pushad
        mov [prog_px], eax
        mov ebp, [ven]
        test ebp, ebp
        jz .f
        mov eax, PROG_X
        mov ebx, PROG_Y
        mov ecx, PROG_AN
        mov edx, PROG_AL
        HV ven_invalidar_rect
.f:
        popad
.x:
        ret

; ---- el volumen: el de Hispa Ventanas (el altavoz de la barra) ----
; mira el volumen y el silencio que hay y, si han cambiado, los enseña
vol_mira:
        pushad
        mov eax, AJ_VOLUMEN
        HV aj_leer
        test eax, eax
        jns .v
        mov eax, 100                    ; (no se sabe: como lo deja HSPSND)
.v:
        cmp eax, [v_pct]
        je .m
        mov [v_pct], eax
        mov ecx, ID_VOL
        call hija
        test eax, eax
        jz .m
        mov ebp, eax
        cmp dword [ebp+V_D3], 0         ; (mientras se arrastra, no)
        jne .m
        mov dword [ebp+V_D1], 0
        mov dword [ebp+V_D2], 100
        mov eax, [v_pct]
        mov [ebp+V_D0], eax
        HV ven_invalidar
        call vol_texto
.m:
        mov eax, AJ_SILENCIO
        HV aj_leer
        cmp eax, [v_mudo]
        je .x
        mov [v_mudo], eax
        mov ecx, ID_MUDO
        call hija
        mov ebp, eax
        mov ecx, [v_mudo]
        mov eax, BM_PONER_MARCA
        HV msg_enviar
.x:
        popad
        ret

; el tanto por ciento, al lado del deslizador
vol_texto:
        pushad
        mov edi, vol_txt
        mov eax, [v_pct]
        call hvl_num
        mov esi, t_pct
        call hvl_cad
        mov byte [edi], 0
        mov ecx, ID_VOL_TXT
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        mov esi, vol_txt
        HV ven_texto
.x:
        popad
        ret

; =============================================================================
;  Los botones
; =============================================================================
; -> CF=1 (y lo dice) si no hay sonido
hay_sonido:
        cmp dword [tarjeta], 0
        jg .si
        push esi
        mov esi, t_sin_tarj
        je .d
        mov esi, t_sin_hsp
.d:
        call estado_pon
        pop esi
        stc
        ret
.si:
        clc
        ret

; AL=canales (0 los dos, 1 izquierda, 2 derecha) ESI=cómo se llama: el sonido
; de prueba. Si ya está en memoria, suena ya; si no, se lee antes
prueba:
        pushad
        call hay_sonido
        jc .x
        mov [c_canal], al
        mov [c_que], esi
        cmp dword [src_id], 1
        jne .leer
        call parar
        call sonar_src
        jmp .x
.leer:
        mov dword [c_id], 1
        mov esi, ruta_a                 ; en \HISPADOS o, si no, al lado
        xor al, al
        HV dos_abrir
        jnc .esta
        mov esi, ruta_b
        xor al, al
        HV dos_abrir
        jnc .esta
        call parar
        mov esi, t_e_hispa
        call estado_pon
        jmp .x
.esta:
        mov ebx, eax
        HV dos_cerrar
        call abrir_wav
.x:
        popad
        ret

; el tono de 440 Hz: se hace la primera vez (16 bits, un canal)
tono:
        pushad
        call hay_sonido
        jc near .x
        call parar
        cmp dword [tono_ptr], 0
        jne .ya
        mov ecx, TONO_N * 2
        HV mem_bloque
        jnc .m
        mov esi, t_e_mem
        call estado_pon
        jmp .x
.m:
        mov [tono_ptr], eax
        mov [tono_mango], edx
        mov edi, eax
        xor ecx, ecx
.l:
        mov eax, ecx
        xor edx, edx
        mov ebx, 50
        div ebx
        movsx eax, word [seno+edx*2]
        mov ebx, ecx                    ; lo que falta para una punta
        mov edx, TONO_N - 1
        sub edx, ecx
        cmp ebx, edx
        jbe .r
        mov ebx, edx
.r:
        cmp ebx, TONO_RAMPA
        jae .p
        imul eax, ebx
        cdq
        mov ebx, TONO_RAMPA
        idiv ebx
.p:
        stosw
        inc ecx
        cmp ecx, TONO_N
        jb .l
.ya:
        mov eax, [tono_ptr]
        mov [p_ptr], eax
        mov dword [p_n], TONO_N
        mov dword [p_hz], TONO_HZ
        mov byte [p_fmt], 1
        mov dword [c_que], t_q_tono
        mov byte [c_canal], 0
        call sonar
.x:
        popad
        ret

; Abrir WAV...: el diálogo de elegir un archivo
abrir:
        pushad
        call hay_sonido
        jc .x
        call parar                      ; (con el diálogo abierto no se mandan trozos)
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .x
        mov dword [c_id], 2
        mov byte [c_canal], 0
        mov dword [c_que], nom_tmp
        mov esi, nom_tmp
        call abrir_wav
.x:
        popad
        ret

; =============================================================================
;  Leer un WAV: la cabecera aquí y los datos, a trozos, en carga_paso
; =============================================================================
; ESI=nombre ([c_id] qué es, [c_canal] y [c_que] cómo sonará luego). Lo que
; hubiera en memoria se suelta. Si no vale, lo dice en la barra de estado
abrir_wav:
        pushad
        call parar
        call src_soltar
        xor al, al
        HV dos_abrir
        mov edi, t_e_abrir
        jc near .dice
        mov [c_man], eax
        mov ebx, eax
        mov al, 2                       ; lo que mide el archivo
        xor edx, edx
        HV dos_puntero
        mov [c_tam], eax
        xor al, al
        xor edx, edx
        HV dos_puntero
        mov edi, cab
        mov ecx, 12
        HV dos_leer
        jc near .nowav
        cmp eax, 12
        jne near .nowav
        cmp dword [cab], 'RIFF'
        jne near .nowav
        cmp dword [cab+8], 'WAVE'
        jne near .nowav
        mov dword [c_pos], 12
        mov byte [c_hayfmt], 0
.trozo:                                 ; cada parte: cuatro letras y su tamaño
        mov edi, cab
        mov ecx, 8
        HV dos_leer
        jc near .nowav
        cmp eax, 8
        jne near .nowav
        add dword [c_pos], 8
        mov edx, [cab+4]
        cmp dword [cab], 'data'
        je .datos
        cmp dword [cab], 'fmt '
        jne .salta
        cmp edx, 16
        jb near .nowav
        push edx
        mov edi, wfmt
        mov ecx, 16
        HV dos_leer
        pop edx
        jc near .nowav
        cmp eax, 16
        jne near .nowav
        mov byte [c_hayfmt], 1
.salta:
        inc edx                         ; (las partes ocupan un número par)
        and edx, ~1
        add edx, [c_pos]
        jc near .nowav
        cmp edx, [c_tam]
        ja near .nowav
        mov [c_pos], edx
        xor al, al
        HV dos_puntero
        jmp .trozo
.datos:
        cmp byte [c_hayfmt], 0
        je near .nowav
        mov edi, t_e_noadm              ; ¿es de los que se admiten?
        movzx eax, word [wfmt]          ; 1 PCM (o FFFEh: el mismo, «extendido»)
        cmp eax, 1
        je .pcm
        cmp eax, 0xFFFE
        jne near .cierra
.pcm:
        xor ecx, ecx                    ; CL = el formato: bit 0, 16 bits; bit 1, estéreo
        movzx eax, word [wfmt+2]        ; canales
        cmp eax, 1
        je .c1
        cmp eax, 2
        jne near .cierra
        or cl, 2
.c1:
        movzx eax, word [wfmt+14]       ; bits
        cmp eax, 8
        je .b1
        cmp eax, 16
        jne near .cierra
        or cl, 1
.b1:
        mov eax, [wfmt+4]               ; Hz
        cmp eax, 8000
        jb near .cierra
        cmp eax, 48000
        ja near .cierra
        mov [src_hz], eax
        mov [src_fmt], cl
        mov eax, [c_tam]                ; los datos: como mucho, hasta el final
        sub eax, [c_pos]
        cmp edx, eax
        jbe .d1
        mov edx, eax
.d1:
        mov eax, 1                      ; octetos por trama: 1, 2 o 4
        test cl, 1
        jz .d2
        shl eax, 1
.d2:
        test cl, 2
        jz .d3
        shl eax, 1
.d3:
        mov [src_bpt], eax
        dec eax
        not eax
        and edx, eax
        mov edi, t_e_vacio
        jz .cierra
        mov [c_falta], edx
        mov [c_total], edx
        mov ecx, edx
        HV mem_bloque
        mov edi, t_e_mem
        jc .cierra
        mov [src_ptr], eax
        mov [src_mango], edx
        mov [c_dst], eax
        mov dword [estado], E_CARGA
        mov dword [c_visto], -1
        call carga_texto
        popad
        ret
.nowav:
        mov edi, t_e_nowav
.cierra:
        mov ebx, [c_man]
        HV dos_cerrar
.dice:
        mov esi, edi
        call estado_pon
        popad
        ret

; «Cargando... 40 %» (solo cuando cambia)
carga_texto:
        pushad
        mov eax, [c_total]
        sub eax, [c_falta]
        mov ebx, 100
        mul ebx
        div dword [c_total]
        cmp eax, [c_visto]
        je .x
        mov [c_visto], eax
        mov edi, est_txt
        mov esi, t_cargando
        call hvl_cad
        call hvl_num
        mov esi, t_pct
        call hvl_cad
        mov byte [edi], 0
        mov esi, est_txt
        call estado_pon
.x:
        popad
        ret

; (bucle) otro trozo del archivo; con el último, a sonar
carga_paso:
        pushad
        mov ebx, [c_man]
        mov edi, [c_dst]
        mov ecx, [c_falta]
        cmp ecx, CARGA_PASO
        jbe .c
        mov ecx, CARGA_PASO
.c:
        HV dos_leer
        jnc .l
        xor eax, eax
.l:
        add [c_dst], eax
        sub [c_falta], eax
        test eax, eax
        jz .fin                         ; (más corto de lo que dice: lo que haya)
        cmp dword [c_falta], 0
        je .fin
        call carga_texto
        jmp .x
.fin:
        mov ebx, [c_man]
        HV dos_cerrar
        mov dword [estado], E_NADA
        mov eax, [c_total]
        sub eax, [c_falta]
        xor edx, edx
        div dword [src_bpt]
        mov [src_n], eax
        test eax, eax
        jnz .vale
        call src_soltar                 ; (no se ha podido leer nada)
        mov esi, t_e_vacio
        call estado_pon
        jmp .x
.vale:
        mov eax, [c_id]
        mov [src_id], eax
        call sonar_src
.x:
        popad
        ret

; suelta el sonido leído
src_soltar:
        cmp dword [src_ptr], 0
        je .x
        push edx
        mov edx, [src_mango]
        HV mem_bloque_soltar
        pop edx
        mov dword [src_ptr], 0
        mov dword [src_id], 0
.x:
        ret

; =============================================================================
;  Sonar
; =============================================================================
; el sonido leído, con [c_canal] y [c_que]
sonar_src:
        push eax
        mov eax, [src_ptr]
        mov [p_ptr], eax
        mov eax, [src_n]
        mov [p_n], eax
        mov eax, [src_hz]
        mov [p_hz], eax
        mov al, [src_fmt]
        mov [p_fmt], al
        pop eax
; empieza a sonar p_ptr (p_n tramas a p_hz, formato p_fmt) por [c_canal]
sonar:
        pushad
        call parar
        mov eax, 0x0300                 ; (por si suena la campanita del volumen:
        HV snd_orden                    ; fuera, que no haga esperar a este)
        mov eax, [tarjeta]
        mov ebx, 48000                  ; HD Audio y AC'97: lo suyo
        mov ecx, 2048
        mov dl, 3
        cmp eax, 3
        jae .t
        mov ebx, [p_hz]                 ; Sound Blaster 16: a la frecuencia del
        mov ecx, 1024                   ; sonido, hasta 44.100 Hz
        cmp eax, 2
        jne .sb
        cmp ebx, 44100
        jbe .t
        mov ebx, 44100
        jmp .t
.sb:
        mov ecx, 4096                   ; las de 8 bits: un canal, hasta 22.050 Hz
        xor dl, dl
        cmp ebx, 22050
        jbe .t
        mov ebx, 22050
.t:
        mov [o_hz], ebx
        mov [o_blq], ecx
        mov [o_fmt], dl
        mov eax, ecx                    ; lo que dura un trozo, en tics del reloj
        mov edx, TICS_SEG
        mul edx
        div ebx
        mov [o_tics], eax
        mov ecx, eax                    ; el silencio de detrás: un tercio de
        mov eax, TICS_SEG / 3           ; segundo, en trozos
        xor edx, edx
        div ecx
        inc eax
        mov [cola], eax
        xor eax, eax
        mov [p_pos], eax
        mov [p_acc], eax
        call reloj
        mov [t_fin], eax
        mov dword [estado], E_SUENA
        ; «Sonando... qué (22050 Hz, 16 bits, estéreo)»
        mov edi, est_txt
        mov esi, t_sonando
        call hvl_cad
        mov esi, [c_que]
        cmp byte [c_canal], 0
        je .q
        cmp dword [tarjeta], 1
        jne .q
        mov esi, t_q_mono
.q:
        mov ebx, esi
        call hvl_cad
        cmp ebx, t_q_prueba             ; el de prueba y los WAV, con su formato
        je .fmt
        cmp ebx, nom_tmp
        jne .dice
.fmt:
        mov ax, ' ('
        stosw
        mov eax, [p_hz]
        call hvl_num
        mov esi, t_hz
        call hvl_cad
        mov esi, t_b8
        test byte [p_fmt], 1
        jz .f1
        mov esi, t_b16
.f1:
        call hvl_cad
        mov esi, t_mono
        test byte [p_fmt], 2
        jz .f2
        mov esi, t_estereo
.f2:
        call hvl_cad
.dice:
        mov byte [edi], 0
        mov esi, est_txt
        call estado_pon
        popad
        ret

; para lo que suene o se esté cargando (sin decir nada)
parar:
        pushad
        mov eax, [estado]
        mov dword [estado], E_NADA
        cmp eax, E_NADA
        je .x
        cmp eax, E_CARGA
        jne .s
        mov ebx, [c_man]                ; a medio leer: no vale
        HV dos_cerrar
        call src_soltar
        jmp .x
.s:
        mov eax, 0x0300                 ; HSPSND, función 03h: parar
        HV snd_orden
.x:
        xor eax, eax
        call prog_pon
        popad
        ret

; (bucle) lo que toca mientras [estado] no es E_NADA
; Detrás del sonido van unos trozos de silencio ([cola]: un tercio de
; segundo) y al final se le dice a HSPSND que pare: HD Audio da vueltas a
; sus cuatro trozos y, si no, repetiría los últimos sin parar.
paso:
        cmp dword [estado], E_CARGA
        je carga_paso
        pushad
        call reloj
        mov edx, [t_fin]
        sub edx, eax                    ; lo que queda en la cola de HSPSND
        mov ecx, [o_tics]
        mov ebx, ecx
        shl ebx, 4
        cmp edx, ebx                    ; (nada, o el reloj ha dado un salto)
        jbe .q
        mov [t_fin], eax
        xor edx, edx
.q:
        cmp dword [estado], E_ACABA
        jne .cabe
        cmp dword [cola], 0
        jne .cabe
        test edx, edx                   ; todo mandado: cuando acabe de sonar
        jnz .x                          ; (lo último es silencio), se para
        call parar
        mov esi, t_terminado
        call estado_pon
        jmp .x
.cabe:
        cmp dword [hsp_ver], 3
        jb .cuenta
        mov eax, 0x0800                 ; HSPSND, función 08h: ¿cuántos trozos
        HV snd_orden                    ; caben sin esperar?
        test al, al
        jz .x
        jmp .manda
.cuenta:                                ; un HSPSND anterior: por el reloj
        lea ecx, [ecx+ecx*2]
        cmp edx, ecx                    ; con más de tres trozos en cola, nada:
        ja .x                           ; cabe otro sin que HSPSND haga esperar
.manda:
        cmp dword [estado], E_ACABA
        jne .m1
        dec dword [cola]
.m1:
        call trozo
        mov eax, [o_tics]
        add [t_fin], eax
        mov eax, [p_pos]                ; por dónde va
        cmp eax, [p_n]
        jbe .pr
        mov eax, [p_n]
.pr:
        mov ebx, PROG_AN
        mul ebx
        div dword [p_n]
        call prog_pon
.x:
        popad
        ret

; hace un trozo de [o_blq] tramas en el formato de la tarjeta y se lo da a
; HSPSND. De p_hz a o_hz, interpolando entre cada trama y la siguiente
; (p_acc: lo que se lleva andado entre las dos, en «o_hz avos»)
trozo:
        pushad
        mov edi, blq_buf
        mov ecx, [o_blq]
.l:
        mov eax, [p_pos]
        call trama                      ; -> EBX=izquierda EDX=derecha
        cmp dword [p_acc], 0
        je .pon
        mov [t_i], ebx
        mov [t_d], edx
        inc eax
        call trama
        mov [t_d1], edx
        mov eax, ebx
        sub eax, [t_i]
        imul dword [p_acc]
        idiv dword [o_hz]
        add eax, [t_i]
        mov ebx, eax
        mov eax, [t_d1]
        sub eax, [t_d]
        imul dword [p_acc]
        idiv dword [o_hz]
        add eax, [t_d]
        mov edx, eax
.pon:
        cmp byte [o_fmt], 0
        je .m8
        mov eax, ebx
        stosw
        mov eax, edx
        stosw
        jmp .av
.m8:
        lea eax, [ebx+edx]              ; un canal de 8 bits: la media
        sar eax, 9
        add al, 0x80
        stosb
.av:
        mov eax, [p_acc]
        add eax, [p_hz]
.a:
        cmp eax, [o_hz]
        jb .a1
        sub eax, [o_hz]
        inc dword [p_pos]
        jmp .a
.a1:
        mov [p_acc], eax
        dec ecx
        jnz .l
        mov esi, blq_buf
        mov ecx, [o_blq]
        mov al, [o_fmt]
        test al, al
        jz .e
        shl ecx, 2
.e:
        mov edx, [o_hz]
        HV snd_pcm2
        mov eax, [p_pos]
        cmp eax, [p_n]
        jb .x
        mov dword [estado], E_ACABA
.x:
        popad
        ret

; EAX=trama -> EBX=izquierda EDX=derecha (16 bits con signo; detrás del
; final, silencio), ya con el canal que no toca callado
trama:
        xor ebx, ebx
        xor edx, edx
        cmp eax, [p_n]
        jae .x
        push esi
        mov esi, [p_ptr]
        test byte [p_fmt], 1
        jnz .w
        test byte [p_fmt], 2
        jnz .b2
        movzx ebx, byte [esi+eax]
        mov edx, ebx
        jmp .b
.b2:
        movzx ebx, byte [esi+eax*2]
        movzx edx, byte [esi+eax*2+1]
.b:
        sub ebx, 128
        shl ebx, 8
        sub edx, 128
        shl edx, 8
        jmp .c
.w:
        test byte [p_fmt], 2
        jnz .w2
        movsx ebx, word [esi+eax*2]
        mov edx, ebx
        jmp .c
.w2:
        movsx ebx, word [esi+eax*4]
        movsx edx, word [esi+eax*4+2]
.c:
        pop esi
        cmp byte [c_canal], 1
        jb .x
        jne .der
        xor edx, edx                    ; solo la izquierda
        ret
.der:
        xor ebx, ebx
.x:
        ret

; El reloj: los tics del 8254 (1.193.182 por segundo). El tic de la BIOS
; (18,2 por segundo) se afina con lo que lleva contado el contador 0, que
; puede estar en modo 3 (baja de dos en dos y da dos vueltas por tic) o en
; modo 2. -> EAX (da la vuelta cada hora: solo valen las diferencias)
reloj:
        push ebx
        push ecx
        push edx
.otra:
        mov ebx, [cero]
        mov ecx, [ebx+0x46C]
        mov al, 0xC2                    ; estado y cuenta del contador 0
        out 0x43, al
        in al, 0x40
        mov dl, al                      ; bit 7 la salida, bits 1-3 el modo
        in al, 0x40
        mov ah, al
        in al, 0x40
        xchg al, ah
        cmp ecx, [ebx+0x46C]
        jne .otra
        movzx eax, ax
        neg eax
        add eax, 0x10000                ; lo que lleva contado
        and eax, 0xFFFF
        mov dh, dl
        and dh, 0x06
        cmp dh, 0x06
        jne .m2
        shr eax, 1                      ; modo 3: media vuelta por cada mitad
        test dl, 0x80
        jnz .m2
        add eax, 0x8000
.m2:
        shl ecx, 16
        add eax, ecx
        mov edx, eax                    ; nunca hacia atrás (el tic de la BIOS
        sub edx, [reloj_ult]            ; llega un pelo después de la vuelta)
        jns .ok
        cmp edx, -0x20000
        jl .ok
        mov eax, [reloj_ult]
.ok:
        mov [reloj_ult], eax
        pop edx
        pop ecx
        pop ebx
        ret

; =============================================================================
;  El deslizador: una guía y un rombo que se lleva con el ratón o con las
;  flechas. V_D0 el valor, V_D1 el mínimo, V_D2 el máximo, V_D3 se está
;  arrastrando, V_D6 tiene el foco. Avisa a la ventana (WM_COMMAND) con cada
;  cambio y, al soltarlo, suena la campanita con el volumen nuevo
; =============================================================================
DES_M       equ 10                      ; margen de la guía a cada lado
des_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        HVJ ven_defecto
.foco:
        mov dword [ebp+V_D6], 1
        jmp .inv
.sinfoco:
        mov dword [ebp+V_D6], 0
        jmp .s0
.suelta:
        cmp dword [ebp+V_D3], 0
        je .inv
        call campana
.s0:
        mov dword [ebp+V_D3], 0
.inv:
        HV ven_invalidar
.nada:
        xor eax, eax
        ret
.pulsa:
        mov dword [ebp+V_D3], 1
        jmp .porx
.mueve:
        cmp dword [ebp+V_D3], 0
        je .nada
.porx:
        movsx eax, dx                   ; del sitio del ratón al valor
        sub eax, DES_M
        jns .x1
        xor eax, eax
.x1:
        mov ecx, [ebp+V_AN]
        sub ecx, 2*DES_M
        cmp eax, ecx
        jle .x2
        mov eax, ecx
.x2:
        mov ebx, [ebp+V_D2]
        sub ebx, [ebp+V_D1]
        imul eax, ebx
        mov edx, ecx
        shr edx, 1
        add eax, edx
        xor edx, edx
        div ecx
        add eax, [ebp+V_D1]
.pon:
        cmp eax, [ebp+V_D1]
        jge .p1
        mov eax, [ebp+V_D1]
.p1:
        cmp eax, [ebp+V_D2]
        jle .p2
        mov eax, [ebp+V_D2]
.p2:
        cmp eax, [ebp+V_D0]
        je .nada
        mov [ebp+V_D0], eax
        HV ven_invalidar
        pushad                          ; el aviso a la ventana
        mov ecx, [ebp+V_ID]
        mov edx, ebp
        mov ebp, [ebp+V_PADRE]
        mov eax, WM_COMMAND
        HV msg_enviar
        popad
        jmp .nada
.tecla:
        mov eax, [ebp+V_D0]             ; de cinco en cinco
        cmp ecx, TC_DER
        jne .k1
        add eax, 5
        jmp .pon
.k1:
        cmp ecx, TC_IZQ
        jne .k2
        sub eax, 5
        jmp .pon
.k2:
        cmp ecx, TC_INICIO
        jne .k3
        mov eax, [ebp+V_D1]
        jmp .pon
.k3:
        cmp ecx, TC_FIN
        jne .k4
        mov eax, [ebp+V_D2]
        jmp .pon
.k4:
        HV tec_estado                   ; (EDX se ha perdido: las mayúsculas, otra vez)
        mov edx, eax
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.pintar:
        HV dc_pintar
        mov edi, [ebp+V_AN]
        sub edi, 2*DES_M                ; largo de la guía
        COLOR C_ARENA_OSC
        mov eax, DES_M
        mov ebx, 9
        mov ecx, edi
        mov edx, 3
        HV dc_rect
        mov ebx, 5                      ; los topes
        mov ecx, 1
        mov edx, 11
        HV dc_rect
        add eax, edi
        HV dc_rect
        mov eax, [ebp+V_D0]             ; el rombo, donde toca
        sub eax, [ebp+V_D1]
        imul eax, edi
        mov ebx, [ebp+V_D2]
        sub ebx, [ebp+V_D1]
        jnz .r
        inc ebx
.r:
        xor edx, edx
        div ebx
        add eax, DES_M
        mov ebx, 10
        COLOR C_MARRON
        mov ecx, 9
        HV dc_rombo
        COLOR C_PETROLEO
        cmp dword [ebp+V_D6], 0
        je .r2
        COLOR C_ANIL
.r2:
        mov ecx, 7
        HV dc_rombo
        COLOR C_CREMA
        mov ecx, 2
        HV dc_rombo
        HV dc_cerrar
        xor eax, eax
        ret

%include "hvplib.inc"

        section .bss
; (las variables, a más de una página del código: se escriben en cada trama,
; y escribir en la página del código que corre es lentísimo: el procesador
; cree que el programa se cambia a sí mismo; en QEMU, más todavía)
            resb 4096
            alignb 4
ven         resd 1
cero        resd 1
reloj_ult   resd 1
hsp_ver     resd 1                      ; versión de la interfaz de HSPSND
tarjeta     resd 1                      ; -1 sin HSPSND, 0 sin tarjeta, 1 SB, 2 SB16, 3 AC'97, 4 HD Audio
estado      resd 1
prog_px     resd 1
v_pct       resd 1
v_mudo      resd 1
; el sonido leído (HISPA.WAV o el que se ha abierto)
src_ptr     resd 1
src_mango   resd 1
src_n       resd 1                      ; tramas
src_hz      resd 1
src_bpt     resd 1                      ; octetos por trama
src_id      resd 1                      ; 0 nada, 1 HISPA.WAV, 2 otro
tono_ptr    resd 1
tono_mango  resd 1
; la carga
c_man       resd 1
c_tam       resd 1
c_pos       resd 1
c_falta     resd 1
c_total     resd 1
c_dst       resd 1
c_id        resd 1
c_que       resd 1                      ; cómo se llama lo que va a sonar
c_visto     resd 1
; lo que suena
p_ptr       resd 1
p_n         resd 1
p_hz        resd 1
p_pos       resd 1
p_acc       resd 1
o_hz        resd 1                      ; lo que se le da a la tarjeta
o_blq       resd 1                      ; tramas de un trozo
o_tics      resd 1                      ; y lo que dura, en tics del reloj
t_fin       resd 1                      ; cuándo acaba lo que hay en la cola
cola        resd 1                      ; trozos de silencio que faltan, al final
t_i         resd 1
t_d         resd 1
t_d1        resd 1
src_fmt     resb 1                      ; bit 0: 16 bits; bit 1: estéreo
p_fmt       resb 1
o_fmt       resb 1                      ; 0: 8 bits y un canal; 3: 16 bits y estéreo
c_canal     resb 1                      ; 0 los dos, 1 izquierda, 2 derecha
c_hayfmt    resb 1
            alignb 4
msg         resb M_TAM
plant       resb P_TAM
desc        resb 8
cab         resb 16
wfmt        resb 16
vol_txt     resb 12
tarj_txt    resb 64
est_txt     resb 100
nom_tmp     resb 80
ruta_a      resb 96
ruta_b      resb 96
blq_buf     resb 8192
HVP_FIN
