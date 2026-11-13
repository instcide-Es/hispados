; =============================================================================
;  MEZCLAD.HVP - el Mezclador de Hispa Ventanas (grupo «Sonido»)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  El volumen de cada fuente de sonido, con el mezclador por fuentes de
;  HSPSND (sus funciones 09h-0Fh, versión 4; docs/MEZCLADOR.md):
;    - una columna por fuente que tenga la tarjeta (General, Sonidos,
;      Música, CD, Línea, Micrófono, Altavoz del PC) con su deslizador
;      vertical, el tanto por ciento y, si es estéreo, su balance;
;    - debajo, las casillas con texto: «Silencio» en todas y, en CD, Línea y
;      Micrófono, «Sonar» (pasa a los altavoces) y «Grabar», si la tarjeta
;      las tiene (nada se dice solo con colores);
;    - los graves y los agudos, en una fila, solo si la tarjeta los tiene; y
;      una frase que dice qué falta y por qué;
;    - vista Sencilla (solo General) y Completa; «Avanzado...» con las
;      ganancias, el ajuste automático y el refuerzo del micrófono (lo que
;      tenga la tarjeta); «De fábrica»;
;    - Ayuda > «Cómo se usa»: un recorrido de ocho pasos que marca en la
;      ventana el control del que habla.
;  Teclado: Tab pasa por las columnas y sus casillas; en una columna, las
;  flechas arriba y abajo (de 5 en 5), RePág y AvPág (de 10 en 10), Inicio y
;  Fin cambian el volumen, izquierda y derecha el balance; Espacio marca la
;  casilla que tiene el foco; M (silencio), S (sonar) y G (grabar) valen para
;  la columna en la que se está. Esc cierra.
;  Se guarda solo, sin preguntar, en la sección [Mezclador] de
;  \HISPADOS\SONIDO.INI (ventanas/hvpmez.inc): un segundo después de cada
;  cambio y al cerrar. HSPSND la pone al arrancar.
;  Con el parámetro /C se abre en la vista Completa (lo usa el botón
;  «Mezclador...» del altavoz de la barra); si no, en la última que se usó.
;  Todo el diseño es propio: la ventana, las columnas, el icono, los textos y
;  el recorrido de ayuda. Es un módulo .HVP con la interfaz de 84 entradas.
; =============================================================================
%include "hvp.inc"

COL_AN      equ 88                  ; una columna
COL_AL      equ 200
SL_Y        equ 46                  ; la guía del volumen, dentro de la columna
SL_L        equ 112
BAL_Y       equ 184                 ; la del balance
BAL_M       equ 18                  ; su margen a cada lado
CAS_AL      equ 22
CAS_AN      equ 84
CAS_Y       equ 8 + COL_AL + 6      ; la primera fila de casillas
ID_COL      equ 100                 ; + fuente
ID_SIL      equ 200
ID_SON      equ 210
ID_GRA      equ 220
ID_TONO     equ 300                 ; 300 graves, 301 agudos
ID_TONO_T   equ 310                 ; su valor
ID_TONO_N   equ 320                 ; su nombre
ID_FRASE    equ 400
ID_VISTA    equ 500
ID_AVZ      equ 501
ID_FAB      equ 502
ID_CERRAR   equ 503
O_SENC      equ 601                 ; el menú
O_COMP      equ 602
O_AVZ       equ 603
O_FAB       equ 604
O_AYUDA     equ 611
O_ACERCA    equ 612
ID_A_ENT    equ 701                 ; Avanzado
ID_A_SAL    equ 702
ID_A_AGC    equ 703
ID_A_REF    equ 704
ID_A_TXT    equ 705
ID_A_CERRAR equ 706
ID_T_TIT    equ 801                 ; Cómo se usa
ID_T_TXT    equ 802
ID_T_ANT    equ 803
ID_T_SIG    equ 804
ID_T_CERRAR equ 805
MAX_PL      equ 48
TUT_N       equ 8
FRASE_AN    equ 70                  ; letras por línea de la frase

HVP_CABECERA "Mezclador", entrada, 2
        db "................................"
        db "................................"
        db ".11111111111111111111111111111.."
        db "1777777777777777777777777777771."
        db "17888888888888888888888888888710"
        db "17888888888888888888888888888710"
        db "17888881888888818888888188888710"
        db "17888884888888818888888188888710"
        db "17888844488888818888888188888710"
        db "17888445448888818888888188888710"
        db "17884455544888818888888c88888710"
        db "1788844544888881888888ccc8888710"
        db "178888444888888188888cc5cc888710"
        db "17888884888888818888cc555cc88710"
        db "178888818888888488888cc5cc888710"
        db "1788888188888844488888ccc8888710"
        db "17888881888884454488888c88888710"
        db "17888881888844555448888188888710"
        db "17888881888884454488888188888710"
        db "17888881888888444888888188888710"
        db "17888881888888848888888188888710"
        db "17888881888888818888888188888710"
        db "17888881888888818888888188888710"
        db "17888888888888888888888888888710"
        db "17889988899899888998998889988710"
        db "17888888888888888888888888888710"
        db "17777777777777777777777777777710"
        db ".11111111111111111111111111111.0"
        db "...000000000000000000000000000.."
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- clases ----
c_mez       dd mez_proc
            db C_CREMA, 0, 0, 0
c_col       dd col_proc
            db C_CREMA, CB_FOCO, 0, 0
c_hdes      dd hdes_proc
            db C_CREMA, CB_FOCO, 0, 0
c_avz       dd avz_proc
            db C_CREMA, 0, 0, 0
c_tut       dd tut_proc
            db C_CREMA, 0, 0, 0

%define EST HV_c_estatico
%define BOT HV_c_boton
H           equ E_HIJA
HT          equ E_HIJA | E_TABULA

mez_menu:
        db "&Ver", 0
        dw O_SENC
        db "Vista &sencilla", 0
        dw O_COMP
        db "Vista &completa", 0
        dw 0
        db 0
        dw O_AVZ
        db "&Avanzado...", 0
        dw O_FAB
        db "Valores de &fábrica...", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_AYUDA
        db "&Cómo se usa...", 0
        dw O_ACERCA
        db "&Acerca del Mezclador...", 0
        dw 0xFFFF
        db 0

; ---- textos ----
t_titulo    db "Mezclador", 0
nombres     dd tn0, tn1, tn2, tn3, tn4, tn5, tn6
tn0         db "General", 0
tn1         db "Sonidos", 0
tn2         db "Música", 0
tn3         db "CD", 0
tn4         db "Línea", 0
tn5         db "Micrófono", 0
tn6         db "Altavoz", 0
t_sil       db "Silencio", 0
t_son       db "Sonar", 0
t_gra       db "Grabar", 0
t_graves    db "Graves:", 0
t_agudos    db "Agudos:", 0
t_v_senc    db "Vista sencilla", 0
t_v_comp    db "Vista completa", 0
t_avz       db "Avanzado...", 0
t_fab       db "De fábrica", 0
t_cerrar    db "Cerrar", 0
t_pct       db " %", 0
t_db        db " dB", 0
t_izq       db "I", 0
t_der       db "D", 0
t_no_hsp    db "No hay mezclador: HSPSND no está cargado, no ha", 10
            db "encontrado tarjeta o es de una versión anterior.", 10
            db "Salga de Hispa Ventanas, escriba HSPSND y vuelva", 10
            db "a entrar con VENTANAS.", 0
t_f_senc    db "Vista sencilla: solo el", 10
            db "volumen de todo. «Vista", 10
            db "completa» enseña cada", 10
            db "fuente por separado.", 0
t_f_ent     db "CD, Línea y Micrófono no salen: la tarjeta no tiene esas entradas.", 0
t_f_ent2    db "Solo salen las entradas que tiene la tarjeta (CD, Línea, Micrófono).", 0
t_f_mus     db "Música (FM) solo sale con la Sound Blaster para juegos (HSPSND /SB).", 0
t_f_mus2    db "Esta Sound Blaster no tiene mezclador para la Música ni para el CD.", 0
t_f_ton     db "Graves y agudos no salen: esta tarjeta no los tiene.", 0
t_f_alt     db "El altavoz del PC no pasa por esta tarjeta: no se regula aquí.", 0
t_f_gra     db "«Grabar» no sale: con esta tarjeta no se elige de dónde se graba.", 0
t_f_una     db "Esta tarjeta graba de una sola fuente: marcar «Grabar» quita la otra.", 0
t_f_todo    db "Esta tarjeta lo tiene todo: volumen, balance y casillas por fuente.", 0
t_fab_preg  db "¿Volver a los valores de fábrica?", 10, 10
            db "General 80 %, Sonidos 90 %, Música, CD, Línea y", 10
            db "Micrófono 85 %, Altavoz 50 %; la línea y el micrófono", 10
            db "sin sonar y se graba del micrófono.", 0
t_acerca    db "Mezclador de Hispa Ventanas", 10, 10
            db "El volumen de cada fuente de sonido, con HSPSND.", 10
            db "Se guarda solo en \HISPADOS\SONIDO.INI.", 0
t_sin_avz   db "Esta tarjeta no tiene ajustes avanzados", 10
            db "(ganancias ni ajuste del micrófono).", 0
t_avz_tit   db "Avanzado", 0
t_a_ent     db "Ganancia de entrada: x", 0
t_a_sal     db "Ganancia de salida: x", 0
t_a_agc     db "Ajuste automático del micrófono", 0
t_a_ref     db "Refuerzo del micrófono (+20 dB)", 0
t_a_txt     db "La ganancia sube la señal: la de entrada,", 10
            db "antes de grabar; la de salida, al sacarla", 10
            db "por los altavoces. Un clic en el botón la", 10
            db "cambia (x1, x2, x4, x8). Súbala solo si se", 10
            db "oye muy flojo: si no, suena rota.", 0
gan_txt     db "1248"
t_tut_tit   db "Cómo se usa el Mezclador", 0
t_paso      db "Paso ", 0
t_de        db " de 8: ", 0
t_ant       db "< Anterior", 0
t_sig       db "Siguiente >", 0
; el recorrido: título, texto y qué se marca (M_*)
M_NADA      equ 0
M_COLS      equ 1                   ; todas las columnas
M_COL0      equ 2                   ; la columna General
M_GUIA0     equ 3                   ; su guía
M_BAL0      equ 4                   ; su balance
M_SIL0      equ 5                   ; su «Silencio»
M_SONAR     equ 6                   ; la primera columna con «Sonar»
M_FRASE     equ 7
M_BOTONES   equ 8
tut_pasos   dd tt1, tx1, M_COLS, tt2, tx2, M_COL0, tt3, tx3, M_GUIA0, tt4, tx4, M_BAL0
            dd tt5, tx5, M_SIL0, tt6, tx6, M_SONAR, tt7, tx7, M_FRASE, tt8, tx8, M_BOTONES
tt1         db "Qué es", 0
tx1         db "El Mezclador reparte el volumen entre las", 10
            db "fuentes de sonido del equipo: General (todo),", 10
            db "Sonidos (los programas y los juegos), Música,", 10
            db "CD, Línea, Micrófono y Altavoz del PC. Solo", 10
            db "salen las que tiene su tarjeta: están", 10
            db "marcadas en la ventana con un recuadro.", 0
tt2         db "La columna General", 0
tx2         db "La primera columna es el volumen de todo: lo", 10
            db "mismo que el altavoz de la barra de arriba.", 10
            db "Las demás suben o bajan cada fuente por", 10
            db "separado, dentro de lo que diga General.", 10
            db "Encima de la guía está el tanto por ciento.", 0
tt3         db "Mover el volumen", 0
tx3         db "Con el ratón: arrastre el rombo de la guía o", 10
            db "haga clic en ella. Con el teclado: Tab hasta", 10
            db "la columna (se le pone un marco) y las", 10
            db "flechas arriba y abajo, de 5 en 5; RePág y", 10
            db "AvPág, de 10 en 10; Inicio, todo; Fin, nada.", 0
tt4         db "El balance", 0
tx4         db "La raya de abajo (I izquierda, D derecha)", 10
            db "reparte el sonido entre los dos altavoces.", 10
            db "Con el teclado, flechas izquierda y derecha;", 10
            db "con el ratón, arrastre su rombo. Un doble", 10
            db "clic lo deja en medio. El micrófono y el", 10
            db "altavoz del PC son de un canal: no tienen.", 0
tt5         db "Silencio", 0
tx5         db "La casilla «Silencio» calla esa fuente sin", 10
            db "perder el volumen puesto. Con el teclado:", 10
            db "Tab hasta la casilla y Espacio, o la tecla M", 10
            db "en cualquier sitio de la columna.", 0
tt6         db "Sonar y Grabar", 0
tx6         db "CD, Línea y Micrófono tienen dos casillas", 10
            db "más: «Sonar» los pasa a los altavoces y", 10
            db "«Grabar» los elige para grabar (teclas S y", 10
            db "G). La línea y el micrófono vienen sin", 10
            db "sonar, para que no se acoplen. Si su tarjeta", 10
            db "no tiene esas entradas, no salen.", 0
tt7         db "Lo que no sale", 0
tx7         db "Debajo, una frase dice lo que su tarjeta no", 10
            db "tiene (por ejemplo, graves y agudos) y por", 10
            db "qué no sale. Si los tiene, los graves y los", 10
            db "agudos van en una fila aparte: 50 es el", 10
            db "sonido tal cual.", 0
tt8         db "Vistas, Avanzado y guardar", 0
tx8         db "«Vista sencilla» deja solo General. En", 10
            db "«Avanzado...» están las ganancias y el ajuste", 10
            db "del micrófono, si su tarjeta los tiene. No", 10
            db "hay que guardar: todo queda solo en", 10
            db "\HISPADOS\SONIDO.INI y HSPSND lo pone al", 10
            db "arrancar el equipo.", 0

; =============================================================================
;  La entrada
; =============================================================================
entrada:                                ; EBX=base ESI=ruta EDI=parámetros
        call mz_ruta
        mov al, 'C'                     ; /C: la vista Completa
        mov esi, edi
        call hvl_opcion
        jc .p
        mov byte [mzs_vista], 'C'
.p:
        mov eax, [hv_api + HV_c_boton * 4]
        mov [k_bot], eax
        mov eax, [hv_api + HV_c_estatico * 4]
        mov [k_est], eax
        xor edx, edx
        HV snd_info                     ; -> EAX=tarjeta, EDX=versión
        mov [tarjeta], eax
        mov byte [hay], 0
        test eax, eax
        jle .nh
        cmp edx, 4
        jb .nh
        call mz_leer
        jc .nh
        mov byte [hay], 1
.nh:
        ; la última vista, de SONIDO.INI (si no se ha dicho /C)
        cmp byte [mzs_vista], 0
        jne .v
        mov esi, mzs_ruta
        mov edi, mzb_ent
        mov ecx, MZ_FMAX + 1
        call hvl_leer
        mov [mzb_n], eax
        call mz_buscar
.v:
        mov al, [mzs_vista]
        cmp al, 'S'
        je .vs
        mov al, 'C'
.vs:
        mov [vista], al
        call disponer                   ; medidas y plantillas
        HV pantalla
        mov ecx, [v_an]
        sub eax, ecx
        sar eax, 1
        jns .x
        xor eax, eax
.x:
        mov [plant+P_X], eax
        sub ebx, BARRA_H + SOMBRA
        sub ebx, [v_al]
        sar ebx, 1
        jns .y
        xor ebx, ebx
.y:
        add ebx, BARRA_H
        mov [plant+P_Y], ebx
        mov dword [plant+P_CLASE], c_mez
        mov dword [plant+P_TEXTO], t_titulo
        mov dword [plant+P_ESTILO], E_TITULO | E_CERRAR | E_MINIM
        mov eax, [v_an]
        mov [plant+P_AN], eax
        mov eax, [v_al]
        mov [plant+P_AL], eax
        mov dword [plant+P_ID], 0
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .fin
        mov ebp, eax
        call montar
        mov ecx, 1
        mov edx, 1000
        HV tmp_poner
        HV app_bucle
.fin:
        ret

; =============================================================================
;  Medidas y controles de la vista (vista = 'S' o 'C')
; =============================================================================
; -> cols/ncol, v_an, v_al (la ventana entera) y pl_din (las hijas)
disponer:
        pushad
        ; las columnas
        xor ecx, ecx
        cmp byte [hay], 0
        je .c9
        cmp byte [vista], 'S'
        jne .c0
        mov byte [cols], 0
        mov ecx, 1
        jmp .c9
.c0:
        xor ebx, ebx
.c1:
        movzx eax, byte [mzs_src]
        bt eax, ebx
        jnc .c2
        mov [cols+ecx], bl
        inc ecx
.c2:
        inc ebx
        cmp ebx, MZ_N
        jb .c1
.c9:
        mov [ncol], ecx
        ; filas de casillas: Silencio siempre; Sonar y Grabar, si alguna
        ; columna las tiene
        mov dword [f_son], -1
        mov dword [f_gra], -1
        mov edx, 1
        cmp byte [vista], 'S'
        je .f9
        movzx eax, byte [mzs_src]
        test [mzs_son], al
        jz .f1
        mov [f_son], edx
        inc edx
.f1:
        test [mzs_rec], al
        jz .f9
        mov [f_gra], edx
        inc edx
.f9:
        mov [nfil], edx
        ; tonos
        mov byte [tonos], 0
        cmp byte [vista], 'S'
        je .t0
        test dword [mzs_ajs], MZC_GRAVES | MZC_AGUDOS
        setnz byte [tonos]
.t0:
        ; las medidas de la zona
        cmp byte [hay], 0
        jne .m1
        mov dword [c_an], 420
        mov dword [c_al], 120
        mov dword [y_frase], 12
        mov dword [y_bot], 84
        jmp .m9
.m1:
        cmp byte [vista], 'S'
        jne .m2
        mov dword [c_an], 8 + COL_AN + 8 + 200 + 8
        mov dword [c_al], CAS_Y + CAS_AL + 8
        mov dword [y_frase], 12
        mov dword [y_bot], CAS_Y - 30 - 34
        jmp .m9
.m2:
        mov eax, [ncol]
        imul eax, COL_AN
        add eax, 8
        cmp eax, 584
        jge .m3
        mov eax, 584
.m3:
        mov [c_an], eax
        mov eax, [nfil]
        imul eax, CAS_AL
        add eax, CAS_Y + 8
        mov [y_tono], eax
        cmp byte [tonos], 0
        je .m4
        add eax, 32
.m4:
        mov [y_frase], eax
        add eax, 60
        mov [y_bot], eax
        add eax, 26 + 8
        mov [c_al], eax
.m9:
        mov eax, [c_an]
        add eax, 2 * BORDE
        mov [v_an], eax
        mov eax, [c_al]
        add eax, 2 * BORDE + TIT_H + 1 + MENU_H
        mov [v_al], eax
        ; las plantillas
        mov edi, pl_din
        cmp byte [hay], 0
        jne .h
        mov eax, [k_est]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], t_no_hsp
        mov dword [edi+P_ESTILO], H
        mov dword [edi+P_X], 12
        mov dword [edi+P_Y], 12
        mov dword [edi+P_AN], 400
        mov dword [edi+P_AL], 66
        mov dword [edi+P_ID], ID_FRASE
        add edi, P_TAM
        jmp .botones
.h:
        xor esi, esi                    ; cada columna y sus casillas
.k:
        cmp esi, [ncol]
        jae .tn
        movzx ebx, byte [cols+esi]
        mov eax, esi
        imul eax, COL_AN
        add eax, 8
        mov [q_x], eax
        mov dword [edi+P_CLASE], c_col
        mov dword [edi+P_TEXTO], 0
        mov dword [edi+P_ESTILO], HT
        mov [edi+P_X], eax
        mov dword [edi+P_Y], 8
        mov dword [edi+P_AN], COL_AN - 4
        mov dword [edi+P_AL], COL_AL
        lea eax, [ebx+ID_COL]
        mov [edi+P_ID], eax
        add edi, P_TAM
        mov eax, CAS_Y
        mov edx, t_sil
        lea ecx, [ebx+ID_SIL]
        call .casilla
        mov eax, [f_son]
        test eax, eax
        js .k2
        bt dword [mzs_son], ebx         ; (los 4 octetos: mzs_son y siguientes;
        jnc .k2                         ;  bt solo mira el bit 0..6)
        imul eax, CAS_AL
        add eax, CAS_Y
        mov edx, t_son
        lea ecx, [ebx+ID_SON]
        call .casilla
.k2:
        mov eax, [f_gra]
        test eax, eax
        js .k3
        bt dword [mzs_rec], ebx
        jnc .k3
        imul eax, CAS_AL
        add eax, CAS_Y
        mov edx, t_gra
        lea ecx, [ebx+ID_GRA]
        call .casilla
.k3:
        inc esi
        jmp .k
.tn:
        ; los tonos: «Graves: [---] +2 dB   Agudos: [---] 0 dB»
        cmp byte [tonos], 0
        je .fr
        xor esi, esi
.tl:
        mov eax, MZC_GRAVES
        test esi, esi
        jz .t1
        mov eax, MZC_AGUDOS
.t1:
        test [mzs_ajs], eax
        jz .tx
        mov eax, esi
        imul eax, 288
        add eax, 8
        mov [q_x], eax
        mov ecx, [k_est]
        mov [edi+P_CLASE], ecx
        mov ecx, t_graves
        test esi, esi
        jz .t2
        mov ecx, t_agudos
.t2:
        mov [edi+P_TEXTO], ecx
        mov dword [edi+P_ESTILO], H
        mov [edi+P_X], eax
        mov ecx, [y_tono]
        add ecx, 3
        mov [edi+P_Y], ecx
        mov dword [edi+P_AN], 64
        mov dword [edi+P_AL], 18
        lea ecx, [esi+ID_TONO_N]
        mov [edi+P_ID], ecx
        add edi, P_TAM
        mov dword [edi+P_CLASE], c_hdes
        mov dword [edi+P_TEXTO], 0
        mov dword [edi+P_ESTILO], HT
        mov eax, [q_x]
        add eax, 66
        mov [edi+P_X], eax
        mov ecx, [y_tono]
        mov [edi+P_Y], ecx
        mov dword [edi+P_AN], 140
        mov dword [edi+P_AL], 21
        lea ecx, [esi+ID_TONO]
        mov [edi+P_ID], ecx
        add edi, P_TAM
        mov ecx, [k_est]
        mov [edi+P_CLASE], ecx
        lea ecx, [esi*8+ton_txt]
        mov [edi+P_TEXTO], ecx
        mov dword [edi+P_ESTILO], H
        mov eax, [q_x]
        add eax, 212
        mov [edi+P_X], eax
        mov ecx, [y_tono]
        add ecx, 3
        mov [edi+P_Y], ecx
        mov dword [edi+P_AN], 64
        mov dword [edi+P_AL], 18
        lea ecx, [esi+ID_TONO_T]
        mov [edi+P_ID], ecx
        add edi, P_TAM
.tx:
        inc esi
        cmp esi, 2
        jb .tl
.fr:
        ; la frase de lo que falta
        call frase_hacer
        mov eax, [k_est]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], frase_txt
        mov dword [edi+P_ESTILO], H
        mov eax, 8
        mov ecx, [c_an]
        sub ecx, 16
        cmp byte [vista], 'S'
        jne .fr1
        mov eax, 8 + COL_AN + 8
        mov ecx, 200
.fr1:
        mov [edi+P_X], eax
        mov [edi+P_AN], ecx
        mov eax, [y_frase]
        mov [edi+P_Y], eax
        mov dword [edi+P_AL], 54
        cmp byte [vista], 'S'
        jne .fr2
        mov dword [edi+P_AL], 72
.fr2:
        mov dword [edi+P_ID], ID_FRASE
        add edi, P_TAM
.botones:
        cmp byte [vista], 'S'
        jne .b1
        cmp byte [hay], 0
        je .b1
        ; sencilla: los dos botones, uno encima de otro, a la derecha
        mov eax, 8 + COL_AN + 8
        mov ebx, [y_bot]
        mov ecx, 200
        mov edx, t_v_comp
        mov esi, ID_VISTA
        call .boton
        mov eax, 8 + COL_AN + 8
        mov ebx, [y_bot]
        add ebx, 34
        mov ecx, 200
        mov edx, t_cerrar
        mov esi, ID_CERRAR
        call .boton
        jmp .fin
.b1:
        ; completa: abajo, de izquierda a derecha
        mov eax, 8
        mov ebx, [y_bot]
        cmp byte [hay], 0
        je .b4
        mov ecx, 138
        mov edx, t_v_senc
        mov esi, ID_VISTA
        call .boton
        mov eax, 154
        test dword [mzs_ajs], MZC_GENT | MZC_GSAL | MZC_AGC | MZC_REFUERZO
        jz .b2
        mov ecx, 120
        mov edx, t_avz
        mov esi, ID_AVZ
        call .boton
        mov eax, 282
.b2:
        mov ecx, 120
        mov edx, t_fab
        mov esi, ID_FAB
        call .boton
.b4:
        mov eax, [c_an]
        sub eax, 8 + 96
        mov ecx, 96
        mov edx, t_cerrar
        mov esi, ID_CERRAR
        call .boton
.fin:
        mov dword [edi], 0
        popad
        ret
; EAX=y EDX=texto ECX=id (en la columna q_x)
.casilla:
        push eax
        mov eax, [k_bot]
        mov [edi+P_CLASE], eax
        pop eax
        mov [edi+P_TEXTO], edx
        mov dword [edi+P_ESTILO], HT | BS_CASILLA
        mov edx, [q_x]
        mov [edi+P_X], edx
        mov [edi+P_Y], eax
        mov dword [edi+P_AN], CAS_AN
        mov dword [edi+P_AL], 20
        mov [edi+P_ID], ecx
        add edi, P_TAM
        ret
; EAX=x EBX=y ECX=ancho EDX=texto ESI=id
.boton:
        push eax
        mov eax, [k_bot]
        mov [edi+P_CLASE], eax
        pop eax
        mov [edi+P_TEXTO], edx
        mov dword [edi+P_ESTILO], HT
        mov [edi+P_X], eax
        mov [edi+P_Y], ebx
        mov [edi+P_AN], ecx
        mov dword [edi+P_AL], 26
        mov [edi+P_ID], esi
        add edi, P_TAM
        ret

; la frase de lo que falta (hasta tres líneas) -> frase_txt
frase_hacer:
        pushad
        mov edi, frase_txt
        mov dword [nlin], 0
        cmp byte [vista], 'S'
        jne .c
        mov esi, t_f_senc
        call hvl_cad
        jmp .x
.c:
        movzx ebx, byte [mzs_src]
        ; las entradas
        mov eax, ebx
        and eax, 0x38
        jnz .e1
        mov esi, t_f_ent
        call .linea
        jmp .e2
.e1:
        cmp eax, 0x38
        je .e2
        mov esi, t_f_ent2
        call .linea
.e2:
        test ebx, 0x04                  ; Música
        jnz .e3
        mov esi, t_f_mus
        cmp dword [tarjeta], 3
        jae .e2b
        mov esi, t_f_mus2
.e2b:
        call .linea
.e3:
        test dword [mzs_ajs], MZC_GRAVES
        jnz .e4
        mov esi, t_f_ton
        call .linea
.e4:
        test ebx, 0x40
        jnz .e5
        mov esi, t_f_alt
        call .linea
.e5:
        test ebx, 0x38                  ; con entradas, pero sin «Grabar»
        jz .e6
        cmp byte [mzs_rec], 0
        jne .e5b
        mov esi, t_f_gra
        call .linea
        jmp .e6
.e5b:
        test dword [mzs_ajs], MZC_UNA
        jz .e6
        mov esi, t_f_una
        call .linea
.e6:
        cmp dword [nlin], 0
        jne .x
        mov esi, t_f_todo
        call .linea
.x:
        mov byte [edi], 0
        popad
        ret
.linea:
        cmp dword [nlin], 3
        jae .lr
        cmp dword [nlin], 0
        je .l1
        mov al, 10
        stosb
.l1:
        call hvl_cad
        inc dword [nlin]
.lr:
        ret

; crea las hijas de la ventana [ven] y les pone los valores
montar:
        pushad
        mov ebp, [ven]
        mov esi, pl_din
        HV ven_lista
        call sincronizar
        ; el foco, a la primera columna (o al primer botón)
        mov ecx, ID_COL
        call hija
        test eax, eax
        jnz .f
        mov ecx, ID_CERRAR
        call hija
        test eax, eax
        jz .x
.f:
        mov ebp, eax
        HV ven_enfocar
.x:
        popad
        ret

; quita las hijas que puso montar
desmontar:
        pushad
        mov esi, pl_din
.l:
        cmp dword [esi], 0
        je .x
        mov ecx, [esi+P_ID]
        call hija
        test eax, eax
        jz .s
        push ebp
        mov ebp, eax
        HV ven_destruir
        pop ebp
.s:
        add esi, P_TAM
        jmp .l
.x:
        popad
        ret

; AL = 'S' o 'C': cambia de vista (y la guarda)
vista_poner:
        pushad
        cmp al, [vista]
        je .x
        mov [vista], al
        mov [mzs_vista], al
        call desmontar
        call disponer
        mov ebp, [ven]
        mov eax, [ebp+V_X]
        mov ebx, [ebp+V_Y]
        mov ecx, [v_an]
        mov edx, [v_al]
        HV ven_mover
        HV ven_invalidar
        call montar
        call guardar_ya                 ; (la vista, al momento)
.x:
        popad
        ret

; lo de HSPSND y la vista, a SONIDO.INI ya (y HSPSND lo da por guardado)
guardar_ya:
        pushad
        cmp byte [hay], 0
        je .x
        mov al, [vista]
        mov [mzs_vista], al
        call mz_leer
        jc .x
        call mz_guardar
        jc .x
        mov eax, 0x0F01
        HV snd_mezcla
.x:
        popad
        ret

; ECX=id -> EAX=la hija de la ventana principal (0 si no está)
hija:
        push ebp
        mov ebp, [ven]
        HV ven_hija_id
        pop ebp
        ret

; las casillas, los tonos y sus textos, como dice mzs_*; y las columnas, a
; repintar
sincronizar:
        pushad
        xor ebx, ebx
.f:
        movzx edx, byte [mzs_flg+ebx]
        lea ecx, [ebx+ID_SIL]
        mov eax, edx
        and eax, MZF_MUDO
        call .marca
        lea ecx, [ebx+ID_SON]
        mov eax, edx
        shr eax, 1
        and eax, 1
        call .marca
        lea ecx, [ebx+ID_GRA]
        mov eax, edx
        shr eax, 2
        and eax, 1
        call .marca
        lea ecx, [ebx+ID_COL]
        call hija
        test eax, eax
        jz .n
        push ebp
        mov ebp, eax
        HV ven_invalidar
        pop ebp
.n:
        inc ebx
        cmp ebx, MZ_N
        jb .f
        ; los tonos
        xor ebx, ebx
.t:
        lea ecx, [ebx+ID_TONO]
        call hija
        test eax, eax
        jz .tn
        push ebp
        mov ebp, eax
        movzx eax, byte [mzs_aj+ebx]
        cmp dword [ebp+V_D3], 0         ; (arrastrando, no)
        jne .t1
        cmp eax, [ebp+V_D0]
        je .t1
        mov [ebp+V_D0], eax
        HV ven_invalidar
.t1:
        pop ebp
        call tono_texto
.tn:
        inc ebx
        cmp ebx, 2
        jb .t
        popad
        ret
; EAX = 0/1, ECX = id: la casilla, si está y si ha cambiado
.marca:
        push eax
        call hija
        mov esi, eax
        pop ecx
        test esi, esi
        jz .mr
        push ebp
        mov ebp, esi
        push ecx
        mov eax, BM_MARCA
        HV msg_enviar
        pop ecx
        cmp eax, ecx
        je .mp
        mov eax, BM_PONER_MARCA
        HV msg_enviar
.mp:
        pop ebp
.mr:
        ret

; EBX = 0 graves, 1 agudos: el texto de su valor («+4 dB»)
tono_texto:
        pushad
        lea edi, [ebx*8+ton_txt]
        movzx eax, byte [mzs_aj+ebx]
        sub eax, 50                     ; SB16: de -14 a +14 dB; AC'97: de
        mov ecx, 14                     ; -10,5 a +10,5
        cmp dword [tarjeta], 3
        jne .k
        mov ecx, 10
.k:
        imul eax, ecx
        cdq
        mov ecx, 50
        idiv ecx
        test eax, eax
        jz .p
        js .n
        mov byte [edi], '+'
        inc edi
        jmp .p
.n:
        mov byte [edi], '-'
        inc edi
        neg eax
.p:
        call hvl_num
        mov esi, t_db
        call hvl_cad
        mov byte [edi], 0
        lea ecx, [ebx+ID_TONO_T]
        call hija
        test eax, eax
        jz .x
        mov ebp, eax
        lea esi, [ebx*8+ton_txt]
        HV ven_texto
.x:
        popad
        ret

; =============================================================================
;  La ventana principal
; =============================================================================
mez_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_COMMAND
        je ordenes
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_CHAR
        je .car
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
        mov dword [ebp+V_MENU], mez_menu
        jmp .hecho
.destruir:
        mov ecx, 1
        HV tmp_quitar
        call guardar_ya                 ; todo, con la vista, a SONIDO.INI

        mov dword [ven], 0
        HV msg_salir
        jmp .hecho
.reloj:
        cmp byte [hay], 0
        je .hecho
        call mz_vigila
        call refrescar
        jmp .hecho
.tecla:
        cmp ecx, TC_ESC
        jne .hecho
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.car:
        mov eax, ecx                    ; M, S y G: en la columna en la que
        and eax, 0xDF                   ; se está
        mov dl, MZF_MUDO
        cmp al, 'M'
        je .c1
        mov dl, MZF_SONAR
        cmp al, 'S'
        je .c1
        mov dl, MZF_GRABAR
        cmp al, 'G'
        jne .hecho
.c1:
        movzx eax, byte [col_act]
        call casilla_cambiar
        jmp .hecho
.pintar:
        HV dc_pintar
        cmp byte [hay], 0
        je .pf
        cmp byte [vista], 'C'
        jne .pm
        COLOR C_ARENA_OSC               ; una raya encima de la frase
        mov eax, 8
        mov ebx, [y_frase]
        sub ebx, 6
        mov ecx, [c_an]
        sub ecx, 16
        mov edx, 1
        HV dc_rect
.pm:
        ; el recorrido de ayuda: un marco en lo que explica
        cmp dword [tut_ven], 0
        je .pf
        call tut_rect                   ; -> EAX EBX ECX EDX (CF: nada)
        jc .pf
        COLOR C_TERRACOTA
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.pf:
        HV dc_cerrar
.hecho:
        xor eax, eax
        ret

; vuelve a leer HSPSND (por si lo cambian el altavoz de la barra, las teclas
; o Configuración) y, si algo ha cambiado, lo enseña
refrescar:
        pushad
        mov esi, mzs_vol
        mov edi, viejo
        mov ecx, MZ_N * 3 + 6
        rep movsb
        call mz_leer
        jc .x
        mov esi, mzs_vol
        mov edi, viejo
        mov ecx, MZ_N * 3 + 6
        repe cmpsb
        je .x
        call sincronizar
.x:
        popad
        ret

; EAX = fuente, DL = casilla (MZF_*): la cambia (si la fuente la tiene)
casilla_cambiar:
        pushad
        cmp eax, MZ_N
        jae .x
        cmp dl, MZF_SONAR
        jne .g
        bt dword [mzs_son], eax
        jnc .x
.g:
        cmp dl, MZF_GRABAR
        jne .p
        bt dword [mzs_rec], eax
        jnc .x
.p:
        movzx ecx, byte [mzs_src]
        bt ecx, eax
        jnc .x
        mov bh, [mzs_flg+eax]
        xor bh, dl
        mov bl, 0xFF
        mov cl, 0xFF
        push eax
        call mz_fuente
        pop eax
        call sincronizar
        cmp eax, 1                      ; al quitar el silencio de General o
        ja .x                           ; Sonidos, la campanita
        test byte [mzs_flg+eax], MZF_MUDO
        jnz .x
        cmp dl, MZF_MUDO
        jne .x
        HV snd_campana
.x:
        popad
        ret

; WM_COMMAND: ECX = id (+ aviso*65536), EDX = el control
ordenes:
        mov ebx, ecx
        shr ebx, 16                     ; EBX = aviso
        movzx ecx, cx
        cmp ecx, ID_COL
        jb .o1
        cmp ecx, ID_COL + MZ_N
        jae .o1
        ; una columna ha cambiado (ella misma se lo ha dicho a HSPSND)
        call sincronizar
        test ebx, ebx                   ; al soltarla, con General o Sonidos,
        jz .hecho                       ; la campanita
        sub ecx, ID_COL
        cmp ecx, 1
        ja .hecho
        HV snd_campana
        jmp .hecho
.o1:
        cmp ecx, ID_SIL
        jb .o4
        cmp ecx, ID_GRA + MZ_N
        jae .o4
        ; una casilla: la marca nueva
        mov eax, ecx
        mov dl, MZF_MUDO
        sub eax, ID_SIL
        cmp eax, 10
        jb .c
        mov dl, MZF_SONAR
        sub eax, 10
        cmp eax, 10
        jb .c
        mov dl, MZF_GRABAR
        sub eax, 10
.c:
        mov [col_act], al
        call casilla_cambiar
        jmp .hecho
.o4:
        cmp ecx, ID_TONO
        jb .o5
        cmp ecx, ID_TONO + 1
        ja .o5
        sub ecx, ID_TONO
        mov eax, ecx
        mov ebx, [edx+V_D0]
        call mz_ajuste
        mov ebx, ecx
        call tono_texto
        jmp .hecho
.o5:
        cmp ecx, ID_VISTA
        jne .o6
        mov al, 'S'
        cmp byte [vista], 'S'
        jne .vp
        mov al, 'C'
.vp:
        call vista_poner
        jmp .hecho
.o6:
        cmp ecx, O_SENC
        jne .o7
        mov al, 'S'
        call vista_poner
        jmp .hecho
.o7:
        cmp ecx, O_COMP
        jne .o8
        mov al, 'C'
        call vista_poner
        jmp .hecho
.o8:
        cmp ecx, ID_AVZ
        je .avz
        cmp ecx, O_AVZ
        jne .o9
.avz:
        call avz_abrir
        jmp .hecho
.o9:
        cmp ecx, ID_FAB
        je .fab
        cmp ecx, O_FAB
        jne .o10
.fab:
        cmp byte [hay], 0
        je .hecho
        mov ebp, [ven]
        mov esi, t_fab_preg
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne .hecho
        mov eax, 0x0E00
        HV snd_mezcla
        call mz_leer
        call sincronizar
        HV snd_campana
        jmp .hecho
.o10:
        cmp ecx, ID_CERRAR
        jne .o11
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.o11:
        cmp ecx, O_AYUDA
        jne .o12
        call tut_abrir
        jmp .hecho
.o12:
        cmp ecx, O_ACERCA
        jne .hecho
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
.hecho:
        xor eax, eax
        ret

; =============================================================================
;  Una columna: el nombre, el tanto por ciento, la guía del volumen y el
;  balance. V_D0 = la fuente, V_D3 = arrastrando (1 volumen, 2 balance),
;  V_D6 = tiene el foco
; =============================================================================
col_proc:
        cmp eax, WM_PAINT
        je col_pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_KEYDOWN
        je .tecla
        cmp eax, WM_CHAR
        je .car
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sinfoco
        HVJ ven_defecto
.crear:
        mov eax, [ebp+V_ID]
        sub eax, ID_COL
        mov [ebp+V_D0], eax
        jmp .nada
.foco:
        mov dword [ebp+V_D6], 1
        mov eax, [ebp+V_D0]
        mov [col_act], al
        jmp .inv
.sinfoco:
        mov dword [ebp+V_D6], 0
        mov dword [ebp+V_D3], 0
.inv:
        HV ven_invalidar
.nada:
        xor eax, eax
        ret
.doble:
        call .bal_aqui                  ; doble clic en el balance: en medio
        jnc .pulsa
        mov eax, 50
        call col_bal
        jmp .nada
.pulsa:
        call .bal_aqui
        jc .pb
        mov dword [ebp+V_D3], 1
        jmp .pv
.pb:
        mov dword [ebp+V_D3], 2
        jmp .px
.mueve:
        cmp dword [ebp+V_D3], 1
        je .pv
        cmp dword [ebp+V_D3], 2
        jne .nada
.px:
        movsx eax, dx                   ; x -> balance
        sub eax, BAL_M
        imul eax, 100
        mov ecx, [ebp+V_AN]
        sub ecx, 2 * BAL_M
        cdq
        idiv ecx
        call col_bal
        jmp .nada
.pv:
        mov eax, edx                    ; y -> volumen
        sar eax, 16
        sub eax, SL_Y
        imul eax, 100
        cdq
        mov ecx, SL_L
        idiv ecx
        neg eax
        add eax, 100
        call col_vol
        jmp .nada
.suelta:
        cmp dword [ebp+V_D3], 0
        je .nada
        mov dword [ebp+V_D3], 0
        mov ebx, 1                      ; (aviso 1: se ha soltado)
        call col_aviso
        jmp .nada
.tecla:
        mov ebx, [ebp+V_D0]
        movzx eax, byte [mzs_vol+ebx]
        cmp ecx, TC_ARRIBA
        jne .k1
        add eax, 5
        jmp .kv
.k1:
        cmp ecx, TC_ABAJO
        jne .k2
        sub eax, 5
        jmp .kv
.k2:
        cmp ecx, TC_REPAG
        jne .k3
        add eax, 10
        jmp .kv
.k3:
        cmp ecx, TC_AVPAG
        jne .k4
        sub eax, 10
        jmp .kv
.k4:
        cmp ecx, TC_INICIO
        jne .k5
        mov eax, 100
        jmp .kv
.k5:
        cmp ecx, TC_FIN
        jne .k6
        xor eax, eax
.kv:
        call col_vol
        mov ebx, 1
        call col_aviso                  ; (con el teclado, cada vez se oye)
        jmp .nada
.k6:
        movzx eax, byte [mzs_bal+ebx]
        cmp ecx, TC_IZQ
        jne .k7
        sub eax, 5
        call col_bal
        jmp .nada
.k7:
        cmp ecx, TC_DER
        jne .k8
        add eax, 5
        call col_bal
        jmp .nada
.k8:
        HV tec_estado                   ; (EDX: las mayúsculas, otra vez)
        mov edx, eax
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.car:
        cmp ecx, ' '                    ; Espacio en la columna: su silencio
        jne .c2
        mov ecx, 'M'
.c2:
        mov eax, WM_CHAR
        HVJ ven_defecto                 ; (sube a la ventana: M, S, G)
; EDX = punto -> CF=1 si está en la zona del balance (y la fuente lo tiene)
.bal_aqui:
        call col_tiene_bal
        jnc .bn
        mov eax, edx
        sar eax, 16
        cmp eax, BAL_Y - 12
        jl .bn
        stc
        ret
.bn:
        clc
        ret

; -> CF=1 si la columna EBP tiene balance (fuente estéreo y la tarjeta lo tiene)
col_tiene_bal:
        cmp dword [ebp+V_D0], MZ_N - 2  ; (micrófono y altavoz: un canal)
        jae .no
        test dword [mzs_ajs], MZC_BAL
        jz .no
        stc
        ret
.no:
        clc
        ret

; EAX = volumen nuevo para la columna EBP
col_vol:
        pushad
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp eax, 100
        jbe .b
        mov eax, 100
.b:
        mov ebx, [ebp+V_D0]
        cmp al, [mzs_vol+ebx]
        je .x
        mov ecx, ebx                    ; AL = fuente, BL = volumen
        mov bl, al
        mov al, cl
        mov bh, 0xFF
        mov cl, 0xFF
        call mz_fuente
        HV ven_invalidar
        xor ebx, ebx
        call col_aviso
.x:
        popad
        ret
; EAX = balance nuevo
col_bal:
        pushad
        call col_tiene_bal
        jnc .x
        test eax, eax
        jns .a
        xor eax, eax
.a:
        cmp eax, 100
        jbe .b
        mov eax, 100
.b:
        mov ebx, [ebp+V_D0]
        cmp al, [mzs_bal+ebx]
        je .x
        mov cl, al
        mov al, bl
        mov bx, 0xFFFF
        call mz_fuente
        HV ven_invalidar
        xor ebx, ebx
        call col_aviso
.x:
        popad
        ret
; EBX = aviso: WM_COMMAND a la ventana principal
col_aviso:
        pushad
        mov ecx, ebx
        shl ecx, 16
        or ecx, [ebp+V_ID]
        mov edx, ebp
        mov ebp, [ebp+V_PADRE]
        mov eax, WM_COMMAND
        HV msg_enviar
        popad
        ret

col_pintar:
        HV dc_pintar
        mov ebx, [ebp+V_D0]
        mov [p_f], ebx
        mov eax, [ebp+V_AN]
        mov [p_an], eax
        ; el nombre, en negrita y centrado
        COLOR C_TINTA
        NEGRITA 1
        mov esi, [nombres+ebx*4]
        mov ebx, 2
        call texto_centro
        NEGRITA 0
        ; el tanto por ciento (en silencio, en gris y dicho)
        mov ebx, [p_f]
        mov edi, col_txt
        movzx eax, byte [mzs_vol+ebx]
        call hvl_num
        mov esi, t_pct
        call hvl_cad
        mov byte [edi], 0
        COLOR C_TINTA
        test byte [mzs_flg+ebx], MZF_MUDO
        jz .t
        COLOR C_ARENA_OSC
.t:
        mov esi, col_txt
        mov ebx, 22
        call texto_centro
        ; la guía: rayitas cada 25 %, el carril y lo puesto
        mov edi, [p_an]
        shr edi, 1                      ; EDI = el centro
        COLOR C_ARENA_OSC
        xor esi, esi
.r:
        lea eax, [edi-20]
        mov ebx, esi
        imul ebx, SL_L / 4
        add ebx, SL_Y
        mov ecx, 8
        mov edx, 1
        HV dc_rect
        lea eax, [edi+12]
        HV dc_rect
        inc esi
        cmp esi, 4
        jbe .r
        lea eax, [edi-3]
        mov ebx, SL_Y
        mov ecx, 6
        mov edx, SL_L + 1
        HV dc_rect
        mov ebx, [p_f]
        movzx eax, byte [mzs_vol+ebx]
        mov ecx, 100
        sub ecx, eax
        imul ecx, SL_L
        mov eax, ecx
        xor edx, edx
        mov ecx, 100
        div ecx
        add eax, SL_Y
        mov [p_y], eax                  ; donde va el rombo
        mov edx, SL_Y + SL_L + 1
        sub edx, eax
        jz .r1
        COLOR C_ANIL
        test byte [mzs_flg+ebx], MZF_MUDO
        jz .r0
        COLOR C_PIEDRA
.r0:
        mov ebx, eax
        lea eax, [edi-3]
        mov ecx, 6
        HV dc_rect
.r1:
        COLOR C_MARRON
        lea eax, [edi-3]
        mov ebx, SL_Y
        mov ecx, 6
        mov edx, SL_L + 1
        HV dc_marco
        ; el rombo
        mov eax, edi
        mov ebx, [p_y]
        COLOR C_MARRON
        mov ecx, 10
        HV dc_rombo
        COLOR C_TERRACOTA
        cmp dword [ebp+V_D6], 0
        je .r2
        COLOR C_ANIL
.r2:
        mov ecx, 8
        HV dc_rombo
        COLOR C_CREMA
        mov ecx, 2
        HV dc_rombo
        ; el balance
        call col_tiene_bal
        jnc .foco
        COLOR C_TINTA
        mov eax, 3
        mov ebx, BAL_Y - 8
        mov esi, t_izq
        HV dc_texto
        mov eax, [p_an]
        sub eax, 11
        mov esi, t_der
        HV dc_texto
        COLOR C_ARENA_OSC
        mov eax, BAL_M
        mov ebx, BAL_Y - 1
        mov ecx, [p_an]
        sub ecx, 2 * BAL_M
        mov edx, 3
        HV dc_rect
        mov eax, edi                    ; la marca de en medio
        mov ebx, BAL_Y - 5
        mov ecx, 1
        mov edx, 11
        HV dc_rect
        mov ebx, [p_f]
        movzx eax, byte [mzs_bal+ebx]
        mov ecx, [p_an]
        sub ecx, 2 * BAL_M
        imul eax, ecx
        xor edx, edx
        mov ecx, 100
        div ecx
        add eax, BAL_M
        mov ebx, BAL_Y
        COLOR C_MARRON
        mov ecx, 7
        HV dc_rombo
        COLOR C_PETROLEO
        mov ecx, 5
        HV dc_rombo
.foco:
        cmp dword [ebp+V_D6], 0         ; con el foco, un marco
        je .tut
        COLOR C_TERRACOTA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [p_an]
        mov edx, [ebp+V_AL]
        HV dc_marco
.tut:
        ; el recorrido de ayuda, si habla de la guía o del balance de la
        ; primera columna: el marco, aquí dentro
        cmp dword [tut_ven], 0
        je .x
        movzx eax, byte [cols]
        cmp eax, [ebp+V_D0]
        jne .x
        mov eax, [tut_paso]
        imul eax, 12
        mov eax, [tut_pasos+eax+8]
        cmp eax, M_GUIA0
        jne .tb
        mov eax, [p_an]
        shr eax, 1
        sub eax, 24
        mov ebx, SL_Y - 8
        mov ecx, 48
        mov edx, SL_L + 16
        jmp .tm
.tb:
        cmp eax, M_BAL0
        jne .x
        mov eax, 1
        mov ebx, BAL_Y - 12
        mov ecx, [p_an]
        sub ecx, 2
        mov edx, 24
.tm:
        COLOR C_TERRACOTA
        HV dc_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        HV dc_marco
.x:
        HV dc_cerrar
        xor eax, eax
        ret

; las columnas, a repintar
cols_inv:
        pushad
        xor ebx, ebx
.l:
        lea ecx, [ebx+ID_COL]
        call hija
        test eax, eax
        jz .n
        mov ebp, eax
        HV ven_invalidar
.n:
        inc ebx
        cmp ebx, MZ_N
        jb .l
        popad
        ret

; ESI = texto, EBX = y: centrado en p_an
texto_centro:
        push eax
        push ecx
        push esi
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .m
        inc ecx
        jmp .l
.m:
        mov eax, [p_an]
        shl ecx, 3
        sub eax, ecx
        sar eax, 1
        HV dc_texto
        pop esi
        pop ecx
        pop eax
        ret

; =============================================================================
;  Deslizador horizontal (los graves y los agudos): V_D0 = valor (0..100),
;  V_D3 = arrastrando, V_D6 = tiene el foco. Avisa a su madre con WM_COMMAND
; =============================================================================
DES_M       equ 10
hdes_proc:
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .doble
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
.suelta:
        mov dword [ebp+V_D3], 0
.inv:
        HV ven_invalidar
.nada:
        xor eax, eax
        ret
.doble:
        mov eax, 50                     ; doble clic: tal cual
        jmp .pon
.pulsa:
        mov dword [ebp+V_D3], 1
        jmp .porx
.mueve:
        cmp dword [ebp+V_D3], 0
        je .nada
.porx:
        movsx eax, dx
        sub eax, DES_M
        imul eax, 100
        mov ecx, [ebp+V_AN]
        sub ecx, 2 * DES_M
        cdq
        idiv ecx
.pon:
        test eax, eax
        jns .p1
        xor eax, eax
.p1:
        cmp eax, 100
        jle .p2
        mov eax, 100
.p2:
        cmp eax, [ebp+V_D0]
        je .nada
        mov [ebp+V_D0], eax
        HV ven_invalidar
        pushad
        mov ecx, [ebp+V_ID]
        mov edx, ebp
        mov ebp, [ebp+V_PADRE]
        mov eax, WM_COMMAND
        HV msg_enviar
        popad
        jmp .nada
.tecla:
        mov eax, [ebp+V_D0]
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
        cmp ecx, TC_ARRIBA
        jne .k3
        add eax, 5
        jmp .pon
.k3:
        cmp ecx, TC_ABAJO
        jne .k4
        sub eax, 5
        jmp .pon
.k4:
        cmp ecx, TC_INICIO
        jne .k5
        mov eax, 50
        jmp .pon
.k5:
        HV tec_estado
        mov edx, eax
        mov eax, WM_KEYDOWN
        HVJ ven_defecto
.pintar:
        HV dc_pintar
        mov edi, [ebp+V_AN]
        sub edi, 2 * DES_M
        COLOR C_ARENA_OSC
        mov eax, DES_M
        mov ebx, 9
        mov ecx, edi
        mov edx, 3
        HV dc_rect
        mov eax, edi                    ; la marca de en medio (tal cual)
        shr eax, 1
        add eax, DES_M
        mov ebx, 4
        mov ecx, 1
        mov edx, 13
        HV dc_rect
        mov eax, [ebp+V_D0]
        imul eax, edi
        xor edx, edx
        mov ecx, 100
        div ecx
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

; =============================================================================
;  Avanzado: las ganancias y el micrófono (lo que tenga la tarjeta). Se aplica
;  al momento, como todo
; =============================================================================
avz_abrir:
        pushad
        cmp dword [avz_ven], 0
        je .n
        mov ebp, [avz_ven]
        HV ven_activar
        jmp .x
.n:
        test dword [mzs_ajs], MZC_GENT | MZC_GSAL | MZC_AGC | MZC_REFUERZO
        jnz .si
        mov ebp, [ven]
        mov esi, t_sin_avz
        mov edi, t_avz_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        jmp .x
.si:
        ; las hijas
        mov edi, pl_avz
        mov ebx, 10                     ; y
        test dword [mzs_ajs], MZC_GENT
        jz .a1
        mov edx, txt_ent
        mov esi, ID_A_ENT
        xor ecx, ecx
        call .fila
.a1:
        test dword [mzs_ajs], MZC_GSAL
        jz .a2
        mov edx, txt_sal
        mov esi, ID_A_SAL
        xor ecx, ecx
        call .fila
.a2:
        test dword [mzs_ajs], MZC_AGC
        jz .a3
        mov edx, t_a_agc
        mov esi, ID_A_AGC
        mov ecx, BS_CASILLA
        call .fila
.a3:
        test dword [mzs_ajs], MZC_REFUERZO
        jz .a4
        mov edx, t_a_ref
        mov esi, ID_A_REF
        mov ecx, BS_CASILLA
        call .fila
.a4:
        add ebx, 4
        mov eax, [k_est]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], t_a_txt
        mov dword [edi+P_ESTILO], H
        mov dword [edi+P_X], 12
        mov [edi+P_Y], ebx
        mov dword [edi+P_AN], 340
        mov dword [edi+P_AL], 92
        mov dword [edi+P_ID], ID_A_TXT
        add edi, P_TAM
        add ebx, 98
        mov eax, [k_bot]
        mov [edi+P_CLASE], eax
        mov dword [edi+P_TEXTO], t_cerrar
        mov dword [edi+P_ESTILO], HT
        mov dword [edi+P_X], 258
        mov [edi+P_Y], ebx
        mov dword [edi+P_AN], 96
        mov dword [edi+P_AL], 26
        mov dword [edi+P_ID], ID_A_CERRAR
        add edi, P_TAM
        mov dword [edi], 0
        add ebx, 26 + 10
        call avz_textos
        ; la ventana, al lado de la principal
        mov ebp, [ven]
        mov eax, [ebp+V_X]
        add eax, 40
        mov [plant+P_X], eax
        mov eax, [ebp+V_Y]
        add eax, 60
        mov [plant+P_Y], eax
        mov dword [plant+P_CLASE], c_avz
        mov dword [plant+P_TEXTO], t_avz_tit
        mov dword [plant+P_ESTILO], E_TITULO | E_CERRAR
        mov dword [plant+P_AN], 366 + 2 * BORDE
        add ebx, 2 * BORDE + TIT_H + 1
        mov [plant+P_AL], ebx
        mov dword [plant+P_ID], 0
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov ebp, eax
        mov esi, pl_avz
        HV ven_lista
        call avz_marcas
        mov esi, pl_avz                 ; el foco, en el primer control
        mov ecx, [esi+P_ID]
        HV ven_hija_id
        test eax, eax
        jz .x
        mov ebp, eax
        HV ven_enfocar
.x:
        popad
        ret
; EBX = y (avanza), EDX = texto, ESI = id, ECX = estilo de más
.fila:
        mov eax, [k_bot]
        mov [edi+P_CLASE], eax
        mov [edi+P_TEXTO], edx
        or ecx, HT
        mov [edi+P_ESTILO], ecx
        mov dword [edi+P_X], 12
        mov [edi+P_Y], ebx
        mov dword [edi+P_AN], 300
        mov dword [edi+P_AL], 26
        mov [edi+P_ID], esi
        add edi, P_TAM
        add ebx, 32
        ret

; los textos de los botones de ganancia («Ganancia de entrada: x2»)
avz_textos:
        pushad
        mov edi, txt_ent
        mov esi, t_a_ent
        call hvl_cad
        movzx eax, byte [mzs_aj+2]
        mov al, [gan_txt+eax]
        stosb
        mov byte [edi], 0
        mov edi, txt_sal
        mov esi, t_a_sal
        call hvl_cad
        movzx eax, byte [mzs_aj+3]
        mov al, [gan_txt+eax]
        stosb
        mov byte [edi], 0
        popad
        ret
; las casillas y los textos de la ventana de Avanzado, como mzs_aj
avz_marcas:
        pushad
        mov ebp, [avz_ven]
        test ebp, ebp
        jz .x
        call avz_textos
        mov ecx, ID_A_ENT
        mov esi, txt_ent
        call .texto
        mov ecx, ID_A_SAL
        mov esi, txt_sal
        call .texto
        mov ecx, ID_A_AGC
        movzx edx, byte [mzs_aj+4]
        call .marca
        mov ecx, ID_A_REF
        movzx edx, byte [mzs_aj+5]
        call .marca
.x:
        popad
        ret
.texto:
        HV ven_hija_id
        test eax, eax
        jz .r
        push ebp
        mov ebp, eax
        HV ven_texto
        HV ven_invalidar
        pop ebp
.r:
        ret
.marca:
        HV ven_hija_id
        test eax, eax
        jz .r
        push ebp
        mov ebp, eax
        mov ecx, edx
        mov eax, BM_PONER_MARCA
        HV msg_enviar
        pop ebp
        ret

avz_proc:
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, WM_KEYDOWN
        je .tecla
        HVJ ven_defecto
.crear:
        mov [avz_ven], ebp
        jmp .hecho
.destruir:
        mov dword [avz_ven], 0
        jmp .hecho
.tecla:
        cmp ecx, TC_ESC
        jne .hecho
.cierra:
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.orden:
        movzx ecx, cx
        cmp ecx, ID_A_CERRAR
        je .cierra
        mov eax, 2
        cmp ecx, ID_A_ENT
        je .gan
        mov eax, 3
        cmp ecx, ID_A_SAL
        jne .o1
.gan:
        movzx ebx, byte [mzs_aj+eax]    ; x1 -> x2 -> x4 -> x8 -> x1
        inc ebx
        and ebx, 3
        call mz_ajuste
        call avz_marcas
        jmp .hecho
.o1:
        mov eax, 4
        cmp ecx, ID_A_AGC
        je .cas
        mov eax, 5
        cmp ecx, ID_A_REF
        jne .hecho
.cas:
        push eax
        push ebp
        mov ebp, edx
        mov eax, BM_MARCA
        HV msg_enviar
        pop ebp
        mov ebx, eax
        pop eax
        call mz_ajuste
.hecho:
        xor eax, eax
        ret

; =============================================================================
;  Cómo se usa: el recorrido de ocho pasos. Una ventana pequeña con el paso,
;  «< Anterior», «Siguiente >» y «Cerrar»; en la ventana del Mezclador, un
;  marco en lo que explica (tut_rect)
; =============================================================================
pl_tut:
        PLANTILLA EST, tut_tit, H | SS_NEGRITA, 12, 10, 380, 18, ID_T_TIT
        PLANTILLA EST, 0, H, 12, 34, 380, 114, ID_T_TXT
        PLANTILLA BOT, t_ant, HT, 12, 156, 120, 26, ID_T_ANT
        PLANTILLA BOT, t_sig, HT | BS_DEFECTO, 142, 156, 130, 26, ID_T_SIG
        PLANTILLA BOT, t_cerrar, HT, 296, 156, 96, 26, ID_T_CERRAR
        dd 0
TUT_AN      equ 404
TUT_AL      equ 192

tut_abrir:
        pushad
        mov al, 'C'                     ; el recorrido, sobre la vista Completa
        call vista_poner
        mov dword [tut_paso], 0
        cmp dword [tut_ven], 0
        je .n
        mov ebp, [tut_ven]
        HV ven_activar
        call tut_poner
        jmp .x
.n:
        cmp byte [pl_tut_ok], 0
        jne .k
        mov esi, pl_tut
        call hvl_clases
        mov byte [pl_tut_ok], 1
.k:
        ; a la derecha de la ventana, si cabe; si no, abajo a la derecha
        mov ebp, [ven]
        HV pantalla
        mov ecx, [ebp+V_X]
        add ecx, [ebp+V_AN]
        add ecx, 8
        lea edx, [ecx + TUT_AN + 2 * BORDE]
        cmp edx, eax
        jbe .px
        sub eax, TUT_AN + 2 * BORDE + 4
        mov ecx, eax
.px:
        mov [plant+P_X], ecx
        mov eax, [ebp+V_Y]
        add eax, 30
        mov [plant+P_Y], eax
        mov dword [plant+P_CLASE], c_tut
        mov dword [plant+P_TEXTO], t_tut_tit
        mov dword [plant+P_ESTILO], E_TITULO | E_CERRAR
        mov dword [plant+P_AN], TUT_AN + 2 * BORDE
        mov dword [plant+P_AL], TUT_AL + 2 * BORDE + TIT_H + 1
        mov dword [plant+P_ID], 0
        mov esi, plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov ebp, eax
        mov esi, pl_tut
        HV ven_lista
        mov ecx, ID_T_SIG               ; el foco, en «Siguiente >»
        HV ven_hija_id
        test eax, eax
        jz .t
        push ebp
        mov ebp, eax
        HV ven_enfocar
        pop ebp
.t:
        call tut_poner
.x:
        popad
        ret

; el paso tut_paso: sus textos y el marco en la ventana del Mezclador
tut_poner:
        pushad
        mov ebp, [tut_ven]
        test ebp, ebp
        jz .x
        mov ebx, [tut_paso]
        imul ebx, 12
        mov edi, tut_tit
        mov esi, t_paso
        call hvl_cad
        mov eax, [tut_paso]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov esi, [tut_pasos+ebx]
        call hvl_cad
        mov byte [edi], 0
        mov ecx, ID_T_TIT
        mov esi, tut_tit
        call .texto
        mov ecx, ID_T_TXT
        mov esi, [tut_pasos+ebx+4]
        call .texto
        mov ebp, [ven]                  ; el marco: toda la ventana, a repintar
        HV ven_invalidar
        call cols_inv
.x:
        popad
        ret
.texto:
        HV ven_hija_id
        test eax, eax
        jz .r
        push ebp
        mov ebp, eax
        HV ven_texto
        HV ven_invalidar
        pop ebp
.r:
        ret

tut_proc:
        cmp eax, WM_COMMAND
        je .orden
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        cmp eax, WM_KEYDOWN
        je .tecla
        HVJ ven_defecto
.crear:
        mov [tut_ven], ebp
        jmp .hecho
.destruir:
        mov dword [tut_ven], 0
        push ebp
        mov ebp, [ven]                  ; el marco se va
        test ebp, ebp
        jz .d
        HV ven_invalidar
        call cols_inv
.d:
        pop ebp
        jmp .hecho
.tecla:
        cmp ecx, TC_INTRO               ; Intro: el paso siguiente
        je .sig
        cmp ecx, TC_ESC
        jne .hecho
.cierra:
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        jmp .hecho
.orden:
        movzx ecx, cx
        cmp ecx, ID_T_CERRAR
        je .cierra
        cmp ecx, ID_T_SIG
        jne .o1
.sig:
        cmp dword [tut_paso], TUT_N - 1
        jae .cierra                     ; (en el último, «Siguiente» cierra)
        inc dword [tut_paso]
        call tut_poner
        jmp .hecho
.o1:
        cmp ecx, ID_T_ANT
        jne .hecho
        cmp dword [tut_paso], 0
        je .hecho
        dec dword [tut_paso]
        call tut_poner
.hecho:
        xor eax, eax
        ret

; -> EAX EBX ECX EDX = el rectángulo del marco del paso (en la zona de la
; ventana del Mezclador); CF=1 si no hay que marcar nada
tut_rect:
        mov eax, [tut_paso]
        imul eax, 12
        mov eax, [tut_pasos+eax+8]
        cmp eax, M_COLS
        jne .r1
        mov eax, 3
        mov ebx, 3
        mov ecx, [ncol]
        imul ecx, COL_AN
        add ecx, 2
        mov edx, COL_AL + 10
        jmp .si
.r1:
        cmp eax, M_COL0
        jne .r2
        mov eax, 3
        mov ebx, 3
        mov ecx, COL_AN + 2
        mov edx, COL_AL + 10
        jmp .si
.r2:
        cmp eax, M_GUIA0                ; (la guía y el balance, los pinta
        je .no                          ; la columna: col_pintar)
        cmp eax, M_BAL0
        je .no
        cmp eax, M_SIL0
        jne .r5
.r4:
        cmp eax, M_SIL0
        jne .r5
        mov eax, 4
        mov ebx, CAS_Y - 4
        mov ecx, CAS_AN + 8
        mov edx, 28
        jmp .si
.r5:
        cmp eax, M_SONAR
        jne .r6
        ; la primera columna con «Sonar»; si no hay, la frase
        xor ecx, ecx
.s:
        cmp ecx, [ncol]
        jae .r6f
        movzx eax, byte [cols+ecx]
        bt dword [mzs_son], eax
        jc .sc
        inc ecx
        jmp .s
.sc:
        imul ecx, COL_AN
        lea eax, [ecx+3]
        mov ebx, CAS_Y - 4
        mov ecx, COL_AN + 2
        mov edx, [nfil]
        imul edx, CAS_AL
        add edx, 8
        jmp .si
.r6:
        cmp eax, M_FRASE
        jne .r7
.r6f:
        mov eax, 3
        mov ebx, [y_frase]
        sub ebx, 4
        mov ecx, [c_an]
        sub ecx, 6
        mov edx, 58
        jmp .si
.r7:
        cmp eax, M_BOTONES
        jne .no
        mov eax, 3
        mov ebx, [y_bot]
        sub ebx, 5
        mov ecx, [c_an]
        sub ecx, 6
        mov edx, 36
.si:
        clc
        ret
.no:
        stc
        ret

%include "hvplib.inc"
%include "hvpmez.inc"

        section .bss
            alignb 4
ven         resd 1
avz_ven     resd 1
tut_ven     resd 1
tut_paso    resd 1
tarjeta     resd 1
k_bot       resd 1                      ; las clases de los controles de Hispa Ventanas
k_est       resd 1
ncol        resd 1
nfil        resd 1
f_son       resd 1                      ; la fila de «Sonar» (-1: no hay)
f_gra       resd 1                      ; la de «Grabar»
c_an        resd 1                      ; la zona de la ventana
c_al        resd 1
v_an        resd 1                      ; la ventana entera
v_al        resd 1
y_tono      resd 1
y_frase     resd 1
y_bot       resd 1
q_x         resd 1
nlin        resd 1
p_f         resd 1                      ; (al pintar una columna)
p_an        resd 1
p_y         resd 1
hay         resb 1                      ; HSPSND con mezclador
vista       resb 1                      ; 'S' o 'C'
tonos       resb 1
col_act     resb 1                      ; la columna en la que se está
pl_tut_ok   resb 1
cols        resb 8                      ; las fuentes de las columnas
viejo       resb 32
col_txt     resb 16
ton_txt     resb 16
txt_ent     resb 32
txt_sal     resb 32
tut_tit     resb 64
frase_txt   resb 256
            alignb 4
plant       resb P_TAM
pl_din      resb P_TAM * MAX_PL + 4
pl_avz      resb P_TAM * 8 + 4
HVP_FIN
