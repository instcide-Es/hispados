; =============================================================================
;  jpeg.inc - descodificador de imágenes JPEG (ITU T.81 / JFIF), para cualquier
;  programa de Hispa Ventanas
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Las fotos de las cámaras, casi todas las de la web y los escaneos metidos
;  en un PDF (/DCTDecode) son JPEG. Aquí están:
;    - el JPEG corriente («baseline», SOF0) y el secuencial extendido de 8
;      bits (SOF1), con códigos de Huffman;
;    - el progresivo (SOF2): barridos por bandas de frecuencias y por
;      aproximaciones sucesivas, con sus tiradas de bloques acabados (EOB);
;    - de uno (gris), tres (YCbCr o RGB) o cuatro componentes (CMYK o YCCK),
;      con cualquier submuestreo (factores de 1 a 4 a lo ancho y a lo alto:
;      4:4:4, 4:2:2, 4:2:0, 4:4:0, 4:1:1...); al ampliar un componente
;      submuestreado se repiten sus puntos (no se suaviza);
;    - tablas de cuantización de 8 y de 16 bits, intervalos de reinicio (DRI y
;      RSTn), el relleno FF 00, y los APPn de JFIF y de Adobe (el APP14 dice
;      si los colores van girados a YCbCr/YCCK, y que el CMYK va al revés).
;  Lo que no: codificación aritmética, sin pérdidas, jerárquico y 12 bits por
;  muestra. Se reconocen y se rechazan cada uno con su error.
;
;  No depende de nada: ni de Hispa Ventanas ni de quien lo incluye. Se pone
;  dentro del código (%include "jpeg.inc") y, en la «section .bss», la macro
;  JPEG_BSS (unos 14 K: tablas de Huffman y de cuantización, un bloque y las
;  tablas de color). Lo demás (una tira de la imagen, y en los progresivos
;  sus coeficientes) va en una memoria de trabajo que da quien llama; cuánta
;  hace falta lo dice jpeg_medidas. No es reentrante: una imagen cada vez.
;  Es para un 386 a secas: todo con enteros.
;
;    jpeg_medidas   ESI=el JPEG  ECX=cuántos octetos  EAX=opciones (JPEG_*)
;                   -> CF=0: EAX=ancho EBX=alto ECX=componentes (1, 3 o 4)
;                      EDX=banderas (JPEG_B_*); y en [jp_pide] los octetos de
;                      memoria de trabajo que pedirá jpeg_abre con esas
;                      opciones, y en [jp_sal_an], [jp_sal_al] y [jp_sal_nc]
;                      lo que medirá lo que salga (puntos y octetos por punto)
;                      CF=1: EAX=el porqué (JPEG_E_*). Conserva lo demás.
;                   (Echa a perder lo que hubiera abierto.)
;    jpeg_abre      ESI, ECX y EAX como antes; EDI=memoria de trabajo EDX=cuánta
;                   -> CF=0: abierto. CF=1: EAX=JPEG_E_*; si es
;                      JPEG_E_MEMORIA, en [jp_pide] lo que hacía falta.
;                   Conserva los registros. Los datos y la memoria de trabajo
;                   tienen que seguir en su sitio mientras se piden filas.
;                   Un progresivo (o uno con los componentes en barridos
;                   aparte) se descodifica entero aquí, a coeficientes.
;    jpeg_fila      -> CF=0: ESI=la fila siguiente, de arriba abajo:
;                      [jp_sal_an] puntos de [jp_sal_nc] octetos (1: gris;
;                      3: rojo, verde y azul; 4, solo con JPEG_COMP: cian,
;                      magenta, amarillo y negro como vienen). Vale hasta la
;                      llamada siguiente.
;                      CF=1: no hay más. EAX=0 si se ha acabado la imagen, o
;                      JPEG_E_* si los datos están rotos o cortados: las filas
;                      entregadas hasta ahí son buenas (la última tira puede
;                      llevar a la derecha un trozo gris, y en un progresivo
;                      cortado se entrega la imagen entera, con el detalle
;                      que hubiera llegado). Conserva los demás registros.
;
;  Opciones (se suman):
;    JPEG_R1, JPEG_R2, JPEG_R4, JPEG_R8   el tamaño: entero, o reducido al
;                   vuelo a la mitad, un cuarto o un octavo de lado (redondeando
;                   hacia arriba). Cada punto es la media de su cuadro de 2x2,
;                   4x4 u 8x8: sale de la propia transformada, sin hacer la
;                   imagen grande (a 1/8 ni siquiera se hace transformada).
;    JPEG_COMP      los componentes como son, sin pasar a rojo, verde y azul
;                   lo que venga en CMYK (para quien tiene su propia idea de
;                   los colores, como un PDF con /ColorSpace y /Decode): salen
;                   1, 3 o 4 octetos por punto. Lo girado (YCbCr, YCCK) sí se
;                   deshace.
;    JPEG_TR_NO, JPEG_TR_SI   si el JPEG no lleva el APP14 de Adobe, que mande
;                   esto: los componentes no van girados (RGB, CMYK) o sí
;                   (YCbCr, YCCK). Sin ninguna de las dos: tres componentes
;                   son YCbCr (salvo que se llamen «R», «G», «B» y no haya
;                   JFIF) y cuatro, CMYK.
;
;  Cómo está hecho. Los bits se sacan por arriba de un registro de 32 que se
;  rellena por octetos (quitando el 00 de detrás de cada FF); al llegar a un
;  marcador o al final de los datos se rellena con ceros de mentira, que se
;  cuentan: si un bloque llega a gastar uno, los datos están rotos (así no hay
;  forma de quedarse dando vueltas ni de leer fuera). Cada código de Huffman
;  se busca en una tabla de 256 entradas con los ocho bits que vienen; los más
;  largos, comparando con el mayor código de cada largo. La transformada
;  inversa es el producto directo por la matriz de cosenos (en coma fija de 14
;  bits), primero por columnas y luego por filas, saltándose los coeficientes
;  a cero, que son casi todos; para las reducciones la matriz es la media de
;  filas vecinas de la entera, así que lo que sale es la media exacta de los
;  puntos que se habrían calculado. Difiere de libjpeg en un nivel por
;  componente, a lo más (hasta tres después de pasar a rojo, verde y azul).
;  Una imagen corriente se hace tira a tira (una fila de MCU: 8 o 16 filas de
;  puntos): en memoria solo hay una tira de cada componente. De un progresivo
;  se guardan los coeficientes de toda la imagen (136 octetos por bloque; con
;  reducción, solo los de frecuencias bajas que se van a usar y un mapa de
;  bits de cuáles no son cero, que hace falta para leer los refinamientos:
;  40, 16 o 10 octetos por bloque) y las tiras salen de ahí.
;  Todo índice y todo largo se comprueba: con datos rotos o cortados da
;  error, nunca lee fuera de los datos ni escribe fuera de lo suyo.
; =============================================================================
JPEG_E_NOJPEG       equ 1           ; no empieza como un JPEG (o no hay nada abierto)
JPEG_E_CORTO        equ 2           ; se acaba antes de tiempo (archivo cortado)
JPEG_E_DATOS        equ 3           ; los datos están rotos o no tienen sentido
JPEG_E_MEMORIA      equ 4           ; la memoria de trabajo no llega ([jp_pide])
JPEG_E_PRECISION    equ 5           ; 12 o 16 bits por muestra
JPEG_E_ARITMETICO   equ 6           ; codificación aritmética
JPEG_E_SINPERDIDAS  equ 7           ; JPEG sin pérdidas
JPEG_E_JERARQUICO   equ 8           ; JPEG jerárquico (diferencial)

JPEG_R1             equ 0
JPEG_R2             equ 1
JPEG_R4             equ 2
JPEG_R8             equ 3
JPEG_COMP           equ 4
JPEG_TR_NO          equ 8
JPEG_TR_SI          equ 16

JPEG_B_PROG         equ 1           ; es progresivo
JPEG_B_COEF         equ 2           ; hay que guardar sus coeficientes (progresivo, o
                                    ; con los componentes en barridos aparte)
JPEG_B_ADOBE        equ 4           ; lleva el APP14 de Adobe (su CMYK va al revés)
JPEG_B_JFIF         equ 8           ; lleva el APP0 de JFIF
JPEG_B_GIRADO       equ 16          ; los componentes van girados (YCbCr o YCCK)

JPEG_BARRIDOS       equ 100         ; barridos, a lo más (uno normal lleva unos diez)

; de cada componente
JC_ID       equ 0
JC_H        equ 4                   ; sus factores de muestreo
JC_V        equ 8
JC_TQ       equ 12
JC_PRED     equ 16                  ; el último DC (se codifica la diferencia)
JC_DCP      equ 20                  ; sus tablas, para el barrido de ahora
JC_ACP      equ 24
JC_QP       equ 28
JC_PASO     equ 32                  ; su plano de la tira: octetos por fila
JC_PLANO    equ 36
JC_EXP      equ 40                  ; una fila suya ampliada al ancho de la imagen
JC_COEF     equ 44                  ; sus coeficientes (progresivo)
JC_BW       equ 48                  ; bloques por fila y filas de bloques, con el relleno
JC_BH       equ 52                  ; hasta MCU enteros
JC_BWR      equ 56                  ; y los que de verdad cubren la imagen (para los
JC_BHR      equ 60                  ; barridos de un solo componente)
JC_FILA     equ 64                  ; la fila que se está entregando
JC_TD       equ 68
JC_TA       equ 72
JC_N        equ 76                  ; puntos de lado que salen de cada bloque suyo
JC_MAT      equ 80                  ; y la matriz con la que se hacen
JC_MAPA     equ 84                  ; de cada coeficiente (en zigzag), dónde se guarda
JC_BTAM     equ 88                  ; octetos de un bloque suyo guardado
JC_TAM      equ 96

; de cada tabla de Huffman
JH_RAP      equ 0                   ; con los 8 bits que vienen: largo (0: es más largo) y símbolo
JH_MAX      equ 512                 ; el mayor código de cada largo (-1: no hay)
JH_DELTA    equ 580                 ; lo que se le suma a un código para dar con su símbolo
JH_VAL      equ 648                 ; los símbolos, en orden
JH_TAM      equ 904

%macro JPEG_BSS 0
            alignb 4
jp_error    resd 1
jp_pide     resd 1
jp_sal_an   resd 1
jp_sal_al   resd 1
jp_sal_nc   resd 1
jp_ancho    resd 1
jp_alto     resd 1
jp_nc       resd 1
jp_band     resd 1
jp_esp      resd 1                  ; la pila y la salida, para dejarlo desde cualquier
jp_salida   resd 1                  ; profundidad al dar con un error
jp_ini      resd 1
jp_fin      resd 1
jp_p        resd 1
jp_bits     resd 1
jp_nbits    resd 1
jp_marca    resd 1                  ; el marcador con el que se ha topado al leer bits
jp_falsos   resd 1                  ; (0x100: el final de los datos) y los ceros de mentira
jp_opc      resd 1
jp_red      resd 1
jp_n        resd 1                  ; puntos de lado de un bloque sin submuestrear: 8, 4, 2 o 1
jp_mat      resd 1
jp_den      resd 1
jp_sof      resd 1
jp_prog     resd 1
jp_jfif     resd 1
jp_adobe    resd 1
jp_adobe_tr resd 1
jp_tr       resd 1
jp_hmax     resd 1
jp_vmax     resd 1
jp_mcux     resd 1
jp_mcuy     resd 1
jp_ri       resd 1
jp_faltan   resd 1
jp_ns       resd 1                  ; el barrido: cuántos componentes, cuáles, la banda
jp_sc       resd 4                  ; y la aproximación
jp_b_ss     resd 1
jp_b_se     resd 1
jp_b_ah     resd 1
jp_b_al     resd 1
jp_p1       resd 1
jp_eob      resd 1
jp_nbarr    resd 1
jp_blq_fn   resd 1
jp_modo     resd 1                  ; 0: tira a tira; 1: con los coeficientes guardados
jp_abierto  resd 1
jp_pend     resd 1                  ; el error que se dirá al acabar de entregar filas
jp_y        resd 1
jp_fy       resd 1
jp_tfilas   resd 1
jp_tira_n   resd 1
jp_fila_p   resd 1
jp_kp       resd 1
jp_k        resd 1
jp_r        resd 1
jp_nv       resd 1
jp_tmp      resd 1
jp_t0       resd 1
jp_j        resd 1
jp_mx       resd 1
jp_my       resd 1
jp_bh       resd 1
jp_bv       resd 1
jp_bx       resd 1
jp_by       resd 1
jp_c        resb 4 * JC_TAM
jp_blk      resd 64                 ; un bloque: los coeficientes ya multiplicados
jp_ws       resd 64
jp_cr_r     resd 256                ; de YCbCr a rojo, verde y azul (como libjpeg)
jp_cb_b     resd 256
jp_cr_g     resd 256
jp_cb_g     resd 256
jp_q        resw 4 * 64             ; cuantización, en el orden en zigzag en que viene
jp_h        resb 8 * JH_TAM         ; Huffman: las 4 de DC y las 4 de AC
jp_tope     resb 1024               ; de -256 a 767, recortado a 0-255
jp_mapa     resb 4 * 64
jp_q_hay    resb 4
jp_h_hay    resb 8
%endmacro

; el orden en zigzag: de cada puesto, qué coeficiente es (fila * 8 + columna)
jp_zz       db  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5
            db 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28
            db 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51
            db 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
            align 4
jp_unos     times 64 dw 1           ; una «cuantización» que no cambia nada
; La transformada inversa en una dimensión, por 16.384: fila x, columna u =
; c(u)/2 * cos((2x + 1) u pi / 16). Las de 4 y de 2 son la media de cada dos
; y de cada cuatro filas de la de 8 (por eso se les anulan columnas).
jp_m8       dd   5793,   8035,   7568,   6811,   5793,   4551,   3135,   1598
            dd   5793,   6811,   3135,  -1598,  -5793,  -8035,  -7568,  -4551
            dd   5793,   4551,  -3135,  -8035,  -5793,   1598,   7568,   6811
            dd   5793,   1598,  -7568,  -4551,   5793,   6811,  -3135,  -8035
            dd   5793,  -1598,  -7568,   4551,   5793,  -6811,  -3135,   8035
            dd   5793,  -4551,  -3135,   8035,  -5793,  -1598,   7568,  -6811
            dd   5793,  -6811,   3135,   1598,  -5793,   8035,  -7568,   4551
            dd   5793,  -8035,   7568,  -6811,   5793,  -4551,   3135,  -1598
jp_m4       dd   5793,   7423,   5352,   2607,      0,  -1742,  -2217,  -1477
            dd   5793,   3075,  -5352,  -6293,      0,   4205,   2217,   -612
            dd   5793,  -3075,  -5352,   6293,      0,  -4205,   2217,    612
            dd   5793,  -7423,   5352,  -2607,      0,   1742,  -2217,   1477
jp_m2       dd   5793,   5249,      0,  -1843,      0,   1232,      0,  -1044
            dd   5793,  -5249,      0,   1843,      0,  -1232,      0,   1044

; EDI += [ESI + %1] * [EDX + %2], si no es cero (casi todos lo son)
%macro JP_TERM 2
        mov eax, [esi+%1]
        test eax, eax
        jz %%cero
        imul eax, [edx+%2]
        add edi, eax
%%cero:
%endmacro

; [ESI]=Y [EBX]=Cb [EDX]=Cr -> rojo, verde y azul en EDI (que avanza 3). Usa EAX y EBP
%macro JP_YCC 0
        push ecx
        movzx ecx, byte [esi]
        movzx ebp, byte [edx]
        mov eax, [jp_cr_r+ebp*4]
        add eax, ecx
        mov al, [jp_tope+256+eax]
        stosb
        movzx eax, byte [ebx]
        mov ebp, [jp_cr_g+ebp*4]
        add ebp, [jp_cb_g+eax*4]
        sar ebp, 16
        add ebp, ecx
        mov eax, [jp_cb_b+eax*4]
        add eax, ecx
        mov ah, [jp_tope+256+eax]
        mov al, [jp_tope+256+ebp]
        stosw
        pop ecx
%endmacro

; -----------------------------------------------------------------------------
;  Las salidas con error, desde cualquier profundidad
; -----------------------------------------------------------------------------
jp_mal_nojpeg:
        mov eax, JPEG_E_NOJPEG
        jmp jp_mal
jp_mal_corto:
        mov eax, JPEG_E_CORTO
        jmp jp_mal
jp_mal_datos:
        mov eax, JPEG_E_DATOS
jp_mal:
        mov [jp_error], eax
        mov esp, [jp_esp]
        jmp [jp_salida]
; la salida corriente: la rutina pública devuelve CF=1 y el error en EAX
jp_sale_mal:
        mov eax, [jp_error]
        mov [esp+28], eax
        popad
        stc
        ret

; -----------------------------------------------------------------------------
jpeg_medidas:
        pushad
        mov [jp_esp], esp
        mov dword [jp_salida], jp_sale_mal
        call jp_empieza
        mov eax, [jp_ancho]
        mov [esp+28], eax
        mov eax, [jp_alto]
        mov [esp+16], eax
        mov eax, [jp_nc]
        mov [esp+24], eax
        mov eax, [jp_band]
        mov [esp+20], eax
        popad
        clc
        ret

jpeg_abre:
        pushad
        mov [jp_esp], esp
        mov dword [jp_salida], jp_sale_mal
        call jp_empieza
        mov eax, JPEG_E_MEMORIA
        mov edx, [esp+20]           ; (EDX de la entrada: lo que hay)
        cmp edx, [jp_pide]
        jb jp_mal
        ; el reparto de la memoria de trabajo: los planos de la tira, las
        ; filas ampliadas, la fila que se entrega y, si hacen falta, los
        ; coeficientes
        mov edi, [esp]              ; (EDI de la entrada)
        mov ebp, jp_c
        mov ecx, [jp_nc]
.plano:
        mov [ebp+JC_PLANO], edi
        mov eax, [ebp+JC_V]
        imul eax, [ebp+JC_N]
        imul eax, [ebp+JC_PASO]
        add edi, eax
        mov [ebp+JC_EXP], edi
        mov eax, [jp_mcux]
        imul eax, [jp_hmax]
        imul eax, [jp_n]
        add edi, eax
        add ebp, JC_TAM
        dec ecx
        jnz .plano
        mov [jp_fila_p], edi
        mov eax, [jp_sal_an]
        imul eax, [jp_sal_nc]
        add edi, eax
        xor eax, eax
        mov [jp_y], eax
        mov [jp_tira_n], eax
        mov [jp_pend], eax
        mov eax, [jp_tfilas]        ; (la primera fila pide la primera tira)
        mov [jp_fy], eax
        cmp dword [jp_modo], 0
        jne .coef
        ; tira a tira: el único barrido lleva todos los componentes
        call jp_pon_barrido
        call jp_pon_q
        mov esi, [jp_p]
        call jp_barrido_ini
        mov [jp_bits], ebx
        mov [jp_nbits], edx
        jmp .abierto
.coef:
        mov ebp, jp_c
        mov ecx, [jp_nc]
.c_sitio:
        mov [ebp+JC_COEF], edi
        mov eax, [ebp+JC_BW]
        imul eax, [ebp+JC_BH]
        imul eax, [ebp+JC_BTAM]
        push ecx
        mov ecx, eax
        xor eax, eax
        rep stosb                   ; todos a cero
        pop ecx
        add ebp, JC_TAM
        dec ecx
        jnz .c_sitio
        ; todos los barridos, hasta el final de la imagen o hasta el primer
        ; error (que se dirá después de entregar lo que haya)
        mov dword [jp_salida], .roto
        mov dword [jp_nbarr], 0
        mov esi, [jp_p]
.barrido:
        inc dword [jp_nbarr]
        cmp dword [jp_nbarr], JPEG_BARRIDOS
        ja jp_mal_datos
        call jp_barrido
        call jp_cabeceras
        cmp al, 0xDA
        je .barrido
        jmp .barridos_fin
.roto:
        mov eax, [jp_error]
        mov [jp_pend], eax
.barridos_fin:
        mov dword [jp_salida], jp_sale_mal
        call jp_pon_q
.abierto:
        mov dword [jp_error], 0
        mov dword [jp_abierto], 1
        popad
        clc
        ret

jpeg_fila:
        pushad
        mov [jp_esp], esp
        mov dword [jp_salida], jp_sale_mal
        cmp dword [jp_abierto], 0
        je jp_mal_nojpeg
        mov eax, [jp_y]
        cmp eax, [jp_sal_al]
        jae .fin
        mov eax, [jp_fy]
        cmp eax, [jp_tfilas]
        jb .hay
        cmp dword [jp_modo], 0      ; hace falta otra tira
        jne .coef
        cmp dword [jp_pend], 0
        jne .fin
        mov dword [jp_salida], .rota
        call jp_tira_flujo
        jmp .tira
.rota:
        ; la tira se ha quedado a medias: lo que falta, en gris, y se entrega;
        ; el error se dirá al pedir la tira siguiente. Si no ha salido ni un
        ; MCU, se dice ya
        mov eax, [jp_error]
        mov [jp_pend], eax
        cmp dword [jp_mx], 0
        je .fin
        call jp_gris
        jmp .tira
.coef:
        call jp_tira_coef
.tira:
        mov dword [jp_salida], jp_sale_mal
        inc dword [jp_tira_n]
        mov dword [jp_fy], 0
.hay:
        call jp_haz_fila
        mov [esp+4], esi
        inc dword [jp_fy]
        inc dword [jp_y]
        popad
        clc
        ret
.fin:
        mov eax, [jp_pend]
        mov [jp_error], eax
        mov [esp+28], eax
        popad
        stc
        ret

; -----------------------------------------------------------------------------
;  Las cabeceras
; -----------------------------------------------------------------------------
; (interna) ESI=el JPEG ECX=cuántos EAX=opciones: lee hasta el primer barrido y
; echa las cuentas de lo que mide todo
jp_empieza:
        mov [jp_opc], eax
        and eax, 3
        mov [jp_red], eax
        mov [jp_ini], esi
        add ecx, esi
        jnc .e1
        mov ecx, 0xFFFFFFFF         ; (no puede dar la vuelta a la memoria)
.e1:
        mov [jp_fin], ecx
        xor eax, eax
        mov [jp_abierto], eax
        mov [jp_error], eax
        mov [jp_pend], eax
        mov [jp_sof], eax
        mov [jp_prog], eax
        mov [jp_jfif], eax
        mov [jp_adobe], eax
        mov [jp_ri], eax
        mov [jp_q_hay], eax
        mov [jp_h_hay], eax
        mov [jp_h_hay+4], eax
        mov [jp_pide], eax
        sub ecx, esi
        cmp ecx, 2
        jb jp_mal_nojpeg
        cmp word [esi], 0xD8FF      ; SOI
        jne jp_mal_nojpeg
        add esi, 2
        call jp_cabeceras
        cmp al, 0xDA
        jne jp_mal_datos            ; se acaba sin imagen
        mov [jp_p], esi
        ; ---- las cuentas ----
        mov ecx, [jp_red]
        mov eax, 8
        shr eax, cl
        mov [jp_n], eax
        mov eax, 1
        shl eax, cl
        dec eax
        mov ebx, eax
        add eax, [jp_ancho]
        shr eax, cl
        mov [jp_sal_an], eax
        add ebx, [jp_alto]
        shr ebx, cl
        mov [jp_sal_al], ebx
        ; los mayores factores de muestreo y los MCU que hay
        mov ebp, jp_c
        mov ecx, [jp_nc]
        xor eax, eax
        xor ebx, ebx
.max:
        cmp eax, [ebp+JC_H]
        jae .max1
        mov eax, [ebp+JC_H]
.max1:
        cmp ebx, [ebp+JC_V]
        jae .max2
        mov ebx, [ebp+JC_V]
.max2:
        add ebp, JC_TAM
        dec ecx
        jnz .max
        mov [jp_hmax], eax
        mov [jp_vmax], ebx
        lea ecx, [eax*8]
        mov eax, [jp_ancho]
        lea eax, [eax+ecx-1]
        xor edx, edx
        div ecx
        mov [jp_mcux], eax
        lea ecx, [ebx*8]
        mov eax, [jp_alto]
        lea eax, [eax+ecx-1]
        xor edx, edx
        div ecx
        mov [jp_mcuy], eax
        mov eax, [jp_vmax]
        imul eax, [jp_n]
        mov [jp_tfilas], eax
        ; ¿van girados los colores? Manda el APP14 de Adobe; si no lo hay, lo
        ; que diga quien llama; si no dice nada, lo que hace todo el mundo
        mov eax, [jp_nc]
        xor ebx, ebx
        cmp eax, 1
        je .tr_ya
        cmp eax, 4
        je .tr4
        cmp dword [jp_adobe], 0
        je .tr3_opc
        cmp dword [jp_adobe_tr], 0
        setne bl
        jmp .tr_ya
.tr3_opc:
        test dword [jp_opc], JPEG_TR_NO
        jnz .tr_ya
        inc ebx
        test dword [jp_opc], JPEG_TR_SI
        jnz .tr_ya
        cmp dword [jp_jfif], 0
        jne .tr_ya
        cmp dword [jp_c+JC_ID], 'R'
        jne .tr_ya
        cmp dword [jp_c+JC_TAM+JC_ID], 'G'
        jne .tr_ya
        cmp dword [jp_c+2*JC_TAM+JC_ID], 'B'
        jne .tr_ya
        dec ebx
        jmp .tr_ya
.tr4:
        cmp dword [jp_adobe], 0
        je .tr4_opc
        cmp dword [jp_adobe_tr], 2
        sete bl
        jmp .tr_ya
.tr4_opc:
        test dword [jp_opc], JPEG_TR_SI
        setnz bl
.tr_ya:
        mov [jp_tr], ebx
        ; los octetos por punto de lo que sale
        mov eax, [jp_nc]
        cmp eax, 4
        jne .snc
        test dword [jp_opc], JPEG_COMP
        jnz .snc
        mov eax, 3
.snc:
        mov [jp_sal_nc], eax
        ; ¿tira a tira, o guardando los coeficientes?
        xor eax, eax
        cmp dword [jp_prog], 0
        jne .m1
        mov ecx, [jp_ns]
        cmp ecx, [jp_nc]
        je .m0
.m1:
        inc eax
.m0:
        mov [jp_modo], eax
        ; las banderas
        mov ebx, [jp_prog]
        lea ebx, [ebx+eax*2]
        cmp dword [jp_adobe], 0
        je .b1
        or ebx, JPEG_B_ADOBE
.b1:
        cmp dword [jp_jfif], 0
        je .b2
        or ebx, JPEG_B_JFIF
.b2:
        cmp dword [jp_tr], 0
        je .b3
        or ebx, JPEG_B_GIRADO
.b3:
        mov [jp_band], ebx
        ; de cada componente: su plano y sus bloques; y la memoria que hace falta
        mov ebp, jp_c
        mov ecx, [jp_nc]
        xor edi, edi                ; EDI=la suma
.comp:
        push ecx
        ; Cuánto se reduce este componente. Uno submuestreado no hace falta
        ; reducirlo tanto: si va a la mitad y la imagen se pide a la mitad, se
        ; hace entero y ya no hay que ampliarlo (así lo hace libjpeg): se
        ; dobla su lado mientras siga cabiendo un número entero de veces, a lo
        ; ancho y a lo alto, en el del componente más fino
        mov ebx, [jp_n]
.n1:
        cmp ebx, 8
        jae .n2
        mov eax, [jp_hmax]
        imul eax, [jp_n]
        mov ecx, [ebp+JC_H]
        imul ecx, ebx
        add ecx, ecx
        xor edx, edx
        div ecx
        test edx, edx
        jnz .n2
        mov eax, [jp_vmax]
        imul eax, [jp_n]
        mov ecx, [ebp+JC_V]
        imul ecx, ebx
        add ecx, ecx
        xor edx, edx
        div ecx
        test edx, edx
        jnz .n2
        add ebx, ebx
        jmp .n1
.n2:
        mov [ebp+JC_N], ebx
        mov eax, jp_m8
        cmp ebx, 8
        je .mat
        mov eax, jp_m4
        cmp ebx, 4
        je .mat
        mov eax, jp_m2
.mat:
        mov [ebp+JC_MAT], eax
        mov eax, ebx
        imul eax, eax
        lea eax, [eax*2+8]
        mov [ebp+JC_BTAM], eax
        ; dónde se guarda cada coeficiente: los de sus N primeras filas y
        ; columnas, seguidos; los demás (0xFF), en ningún sitio
        mov eax, [esp]
        neg eax
        add eax, [jp_nc]
        shl eax, 6
        add eax, jp_mapa
        mov [ebp+JC_MAPA], eax
        mov esi, eax
        xor ecx, ecx
.mapa:
        movzx eax, byte [jp_zz+ecx]
        mov edx, eax
        shr eax, 3                  ; fila
        and edx, 7                  ; columna
        cmp eax, ebx
        jae .mapa0
        cmp edx, ebx
        jae .mapa0
        imul eax, ebx
        add eax, edx
        jmp .mapa1
.mapa0:
        mov al, 0xFF
.mapa1:
        mov [esi+ecx], al
        inc ecx
        cmp ecx, 64
        jb .mapa
        mov eax, [jp_mcux]
        imul eax, [ebp+JC_H]
        mov [ebp+JC_BW], eax
        imul eax, [ebp+JC_N]
        mov [ebp+JC_PASO], eax
        mov ebx, [ebp+JC_V]
        imul ebx, [ebp+JC_N]
        imul eax, ebx
        add edi, eax                ; su plano (no pasa de 8 megas)
        mov eax, [jp_mcux]
        imul eax, [jp_hmax]
        imul eax, [jp_n]
        add edi, eax                ; y su fila ampliada
        mov eax, [jp_mcuy]
        imul eax, [ebp+JC_V]
        mov [ebp+JC_BH], eax
        mov eax, [jp_ancho]         ; los bloques que de verdad cubren la imagen
        mul dword [ebp+JC_H]
        add eax, [jp_hmax]
        dec eax
        xor edx, edx
        div dword [jp_hmax]
        add eax, 7
        shr eax, 3
        mov [ebp+JC_BWR], eax
        mov eax, [jp_alto]
        mul dword [ebp+JC_V]
        add eax, [jp_vmax]
        dec eax
        xor edx, edx
        div dword [jp_vmax]
        add eax, 7
        shr eax, 3
        mov [ebp+JC_BHR], eax
        cmp dword [jp_modo], 0
        je .comp1
        mov eax, [ebp+JC_BW]        ; sus coeficientes: puede no caber en 32 bits
        mul dword [ebp+JC_BH]
        test edx, edx
        jnz .mucho
        mul dword [ebp+JC_BTAM]
        test edx, edx
        jnz .mucho
        add edi, eax
        jc .mucho
.comp1:
        pop ecx
        add ebp, JC_TAM
        dec ecx
        jnz .comp
        mov eax, [jp_sal_an]
        imul eax, [jp_sal_nc]
        add edi, eax
        jc .mucho0
        mov [jp_pide], edi
        ret
.mucho:
        pop ecx
.mucho0:
        mov dword [jp_pide], 0xFFFFFFFF
        ret

; (interna) ESI=el cursor -> AL=el marcador siguiente, ESI detrás
jp_marcador:
.busca:
        cmp esi, [jp_fin]
        jae jp_mal_corto
        lodsb
        cmp al, 0xFF
        jne .busca
.ff:
        cmp esi, [jp_fin]
        jae jp_mal_corto
        lodsb
        cmp al, 0xFF                ; (pueden venir varios FF de relleno)
        je .ff
        test al, al
        jz .busca
        ret

; (interna) ESI=el cursor: lee segmentos hasta un barrido (-> AL=0xDA, con su
; cabecera leída y ESI en sus datos) o hasta el final de la imagen (-> AL=0xD9)
jp_cabeceras:
.sig:
        call jp_marcador
        cmp al, 0xD9
        je .x
        cmp al, 0xD8                ; sin largo: SOI, TEM y los RSTn sueltos
        je .sig
        cmp al, 0x01
        je .sig
        mov bl, al
        and al, 0xF8
        cmp al, 0xD0
        je .sig
        ; el largo del segmento (se cuenta a sí mismo); tiene que caber entero
        mov eax, [jp_fin]
        sub eax, esi
        cmp eax, 2
        jb jp_mal_corto
        movzx ecx, word [esi]
        xchg cl, ch
        add esi, 2
        sub ecx, 2
        jb jp_mal_datos
        sub eax, 2
        cmp eax, ecx
        jb jp_mal_corto
        lea ebp, [esi+ecx]          ; EBP=el final del segmento
        cmp bl, 0xC4
        je .dht
        cmp bl, 0xDB
        je .dqt
        cmp bl, 0xDA
        je .sos
        cmp bl, 0xDD
        je .dri
        cmp bl, 0xE0
        je .app0
        cmp bl, 0xEE
        je .app14
        cmp bl, 0xDE                ; DHP: jerárquico
        je .jer
        mov al, bl
        and al, 0xF0
        cmp al, 0xC0
        jne .salta                  ; COM, los otros APPn y lo que no se conoce
        cmp bl, 0xC2
        jbe .sof
        mov eax, JPEG_E_SINPERDIDAS
        cmp bl, 0xC3
        je jp_mal
        cmp bl, 0xC8
        je jp_mal_datos
        mov eax, JPEG_E_ARITMETICO  ; SOF9 a SOF11 y DAC
        jb .jer
        cmp bl, 0xCC
        jbe jp_mal
.jer:
        mov eax, JPEG_E_JERARQUICO  ; SOF5 a SOF7 y SOF13 a SOF15
        jmp jp_mal
.salta:
        mov esi, ebp
        jmp .sig
.x:
        ret

.dri:
        cmp ecx, 2
        jb jp_mal_datos
        movzx eax, word [esi]
        xchg al, ah
        mov [jp_ri], eax
        jmp .salta
.app0:
        cmp ecx, 5
        jb .salta
        cmp dword [esi], 'JFIF'
        jne .salta
        cmp byte [esi+4], 0
        jne .salta
        mov dword [jp_jfif], 1
        jmp .salta
.app14:
        cmp ecx, 12
        jb .salta
        cmp dword [esi], 'Adob'
        jne .salta
        cmp byte [esi+4], 'e'
        jne .salta
        movzx eax, byte [esi+11]
        mov [jp_adobe_tr], eax
        mov dword [jp_adobe], 1
        jmp .salta

.dqt:
        ; una o varias tablas de cuantización: clase (8 o 16 bits) y número, y
        ; sus 64 valores en zigzag
        cmp esi, ebp
        jae .salta
        lodsb
        movzx edx, al
        mov ebx, edx
        shr ebx, 4
        and edx, 15
        cmp edx, 3
        ja jp_mal_datos
        cmp ebx, 1
        ja jp_mal_datos
        mov ecx, 64
        test ebx, ebx
        jz .q0
        add ecx, ecx
.q0:
        mov eax, ebp
        sub eax, esi
        cmp eax, ecx
        jb jp_mal_datos
        mov byte [jp_q_hay+edx], 1
        shl edx, 7
        lea edi, [jp_q+edx]
        mov ecx, 64
.q1:
        xor eax, eax
        lodsb
        test ebx, ebx
        jz .q2
        mov ah, al
        lodsb
.q2:
        stosw
        dec ecx
        jnz .q1
        jmp .dqt

.dht:
        ; una o varias tablas de Huffman: clase (DC o AC) y número, cuántos
        ; códigos hay de cada largo (de 1 a 16) y los símbolos
        cmp esi, ebp
        jae .salta
        mov eax, ebp
        sub eax, esi
        cmp eax, 17
        jb jp_mal_datos
        lodsb
        movzx edx, al
        mov ebx, edx
        shr ebx, 4
        and edx, 15
        cmp ebx, 1
        ja jp_mal_datos
        cmp edx, 3
        ja jp_mal_datos
        lea edx, [edx+ebx*4]
        xor eax, eax
        xor ecx, ecx
.h1:
        movzx edi, byte [esi+ecx]
        add eax, edi
        inc ecx
        cmp ecx, 16
        jb .h1
        cmp eax, 256
        ja jp_mal_datos
        mov ecx, ebp
        sub ecx, esi
        sub ecx, 16
        cmp ecx, eax
        jb jp_mal_datos
        call jp_haz_huff
        lea esi, [esi+eax+16]
        jmp .dht

.sof:
        ; la imagen: precisión, alto, ancho y, de cada componente, su nombre,
        ; sus factores de muestreo y su tabla de cuantización
        cmp dword [jp_sof], 0
        jne jp_mal_datos
        cmp ecx, 6
        jb jp_mal_datos
        movzx eax, byte [esi]
        cmp eax, 8
        je .s1
        cmp eax, 12
        je .s_prec
        cmp eax, 16
        jne jp_mal_datos
.s_prec:
        mov eax, JPEG_E_PRECISION
        jmp jp_mal
.s1:
        movzx eax, word [esi+1]
        xchg al, ah
        test eax, eax
        jz jp_mal_datos
        mov [jp_alto], eax
        movzx eax, word [esi+3]
        xchg al, ah
        test eax, eax
        jz jp_mal_datos
        mov [jp_ancho], eax
        movzx eax, byte [esi+5]
        cmp eax, 1
        je .s2
        cmp eax, 3
        je .s2
        cmp eax, 4
        jne jp_mal_datos
.s2:
        mov [jp_nc], eax
        lea eax, [eax+eax*2+6]
        cmp ecx, eax
        jb jp_mal_datos
        add esi, 6
        mov edi, jp_c
        mov ecx, [jp_nc]
.s3:
        movzx eax, byte [esi]
        mov [edi+JC_ID], eax
        movzx eax, byte [esi+1]
        mov edx, eax
        shr eax, 4
        and edx, 15
        cmp dword [jp_nc], 1        ; (uno solo: sus factores no pintan nada)
        jne .s4
        mov eax, 1
        mov edx, 1
.s4:
        dec eax
        cmp eax, 3
        ja jp_mal_datos
        inc eax
        dec edx
        cmp edx, 3
        ja jp_mal_datos
        inc edx
        mov [edi+JC_H], eax
        mov [edi+JC_V], edx
        movzx eax, byte [esi+2]
        cmp eax, 3
        ja jp_mal_datos
        mov [edi+JC_TQ], eax
        add esi, 3
        add edi, JC_TAM
        dec ecx
        jnz .s3
        movzx eax, bl
        mov [jp_sof], eax
        xor eax, eax
        cmp bl, 0xC2
        sete al
        mov [jp_prog], eax
        jmp .salta

.sos:
        ; un barrido: qué componentes lleva y con qué tablas; y, en los
        ; progresivos, qué banda de coeficientes y qué bits
        cmp dword [jp_sof], 0
        je jp_mal_datos
        cmp ecx, 1
        jb jp_mal_datos
        movzx eax, byte [esi]
        dec eax
        cmp eax, 3
        ja jp_mal_datos
        inc eax
        cmp eax, [jp_nc]
        ja jp_mal_datos
        mov [jp_ns], eax
        lea eax, [eax*2+4]
        cmp ecx, eax
        jb jp_mal_datos
        inc esi
        xor ecx, ecx
.o1:
        movzx eax, byte [esi+ecx*2] ; el nombre: ¿cuál de los de la imagen es?
        mov edi, jp_c
        mov edx, [jp_nc]
.o2:
        cmp eax, [edi+JC_ID]
        je .o3
        add edi, JC_TAM
        dec edx
        jnz .o2
        jmp jp_mal_datos
.o3:
        xor edx, edx                ; no puede venir dos veces
.o4:
        cmp edx, ecx
        jae .o5
        cmp edi, [jp_sc+edx*4]
        je jp_mal_datos
        inc edx
        jmp .o4
.o5:
        mov [jp_sc+ecx*4], edi
        movzx eax, byte [esi+ecx*2+1]
        mov edx, eax
        shr eax, 4
        and edx, 15
        cmp eax, 3
        ja jp_mal_datos
        cmp edx, 3
        ja jp_mal_datos
        mov [edi+JC_TD], eax
        mov [edi+JC_TA], edx
        inc ecx
        cmp ecx, [jp_ns]
        jb .o1
        lea esi, [esi+ecx*2]
        movzx eax, byte [esi]
        mov [jp_b_ss], eax
        movzx eax, byte [esi+1]
        mov [jp_b_se], eax
        movzx eax, byte [esi+2]
        mov edx, eax
        shr eax, 4
        and edx, 15
        mov [jp_b_ah], eax
        mov [jp_b_al], edx
        mov esi, ebp
        mov al, 0xDA
        ret

; (interna) hace una tabla de Huffman: ESI=las 16 cuentas y, detrás, los
; símbolos; EAX=cuántos símbolos; EDX=cuál (0-3 las de DC, 4-7 las de AC)
jp_haz_huff:
        pushad
        mov byte [jp_h_hay+edx], 0
        imul edi, edx, JH_TAM
        add edi, jp_h
        mov [jp_t0], edi
        push esi
        mov ecx, eax
        add esi, 16
        add edi, JH_VAL
        rep movsb
        pop esi
        mov edi, [jp_t0]
        mov ecx, 256
        xor eax, eax
        rep stosw
        mov edi, [jp_t0]
        xor ebx, ebx                ; EBX=el código que toca (son consecutivos)
        xor ebp, ebp                ; EBP=cuántos símbolos van
        mov ecx, 1                  ; ECX=el largo
.largo:
        mov eax, ebp
        sub eax, ebx
        mov [edi+JH_DELTA+ecx*4], eax
        movzx edx, byte [esi+ecx-1]
.cod:
        test edx, edx
        jz .hecho
        cmp ecx, 8
        ja .lento
        ; hasta 8 bits: todas las entradas que empiezan por este código
        push ecx
        push edx
        push ebx
        movzx eax, byte [edi+JH_VAL+ebp]
        mov ah, al
        mov al, cl
        mov edx, ebx
        neg ecx
        add ecx, 8
        shl edx, cl
        mov ebx, 1
        shl ebx, cl
.r:
        cmp edx, 256                ; (una tabla con códigos de más)
        jae .r1
        mov [edi+JH_RAP+edx*2], ax
        inc edx
        dec ebx
        jnz .r
.r1:
        pop ebx
        pop edx
        pop ecx
.lento:
        inc ebx
        inc ebp
        dec edx
        jmp .cod
.hecho:
        mov eax, 1                  ; no puede haber más códigos de los que caben
        shl eax, cl
        cmp ebx, eax
        ja jp_mal_datos
        lea eax, [ebx-1]
        cmp byte [esi+ecx-1], 0
        jne .m
        mov eax, -1
.m:
        mov [edi+JH_MAX+ecx*4], eax
        shl ebx, 1
        inc ecx
        cmp ecx, 16
        jbe .largo
        mov edx, [esp+20]
        mov byte [jp_h_hay+edx], 1
        popad
        ret

; (interna) las tablas de cuantización de cada componente: tienen que estar
jp_pon_q:
        mov ebp, jp_c
        mov ecx, [jp_nc]
.c:
        mov eax, [ebp+JC_TQ]
        cmp byte [jp_q_hay+eax], 0
        je jp_mal_datos
        shl eax, 7
        add eax, jp_q
        mov [ebp+JC_QP], eax
        add ebp, JC_TAM
        dec ecx
        jnz .c
        ; y, ya que se pasa por aquí al abrir, las tablas de color
        xor ecx, ecx
.t:
        lea eax, [ecx-128]
        imul edx, eax, 91881        ; 1,40200
        add edx, 32768
        sar edx, 16
        mov [jp_cr_r+ecx*4], edx
        imul edx, eax, 116130       ; 1,77200
        add edx, 32768
        sar edx, 16
        mov [jp_cb_b+ecx*4], edx
        imul edx, eax, -46802       ; 0,71414
        mov [jp_cr_g+ecx*4], edx
        imul edx, eax, -22554       ; 0,34414
        add edx, 32768
        mov [jp_cb_g+ecx*4], edx
        inc ecx
        cmp ecx, 256
        jb .t
        mov edi, jp_tope
        xor eax, eax
        mov ecx, 256
        rep stosb
.t1:
        stosb
        inc al
        jnz .t1
        dec al
        mov ecx, 512
        rep stosb
        ret

; (interna) prepara el barrido que se acaba de leer: comprueba que tiene
; sentido y que están las tablas que usa, y elige cómo se lee cada bloque
jp_pon_barrido:
        mov ebx, 3                  ; EBX=qué tablas hacen falta: 1 DC, 2 AC
        mov dword [jp_blq_fn], jp_b_sec
        cmp dword [jp_prog], 0
        je .tablas
        mov eax, [jp_b_se]
        cmp eax, 63
        ja jp_mal_datos
        cmp [jp_b_ss], eax
        ja jp_mal_datos
        mov ecx, [jp_b_al]
        cmp ecx, 13
        ja jp_mal_datos
        mov eax, 1
        shl eax, cl
        mov [jp_p1], eax
        cmp dword [jp_b_ss], 0
        jne .ac
        cmp dword [jp_b_se], 0      ; el DC va solo, en barridos suyos
        jne jp_mal_datos
        mov ebx, 1
        mov dword [jp_blq_fn], jp_b_dc0
        cmp dword [jp_b_ah], 0
        je .tablas
        xor ebx, ebx
        mov dword [jp_blq_fn], jp_b_dc1
        jmp .tablas
.ac:
        cmp dword [jp_ns], 1        ; los AC, de un componente cada vez
        jne jp_mal_datos
        mov ebx, 2
        mov dword [jp_blq_fn], jp_b_ac0
        cmp dword [jp_b_ah], 0
        je .tablas
        mov dword [jp_blq_fn], jp_b_ac1
.tablas:
        xor ecx, ecx
.c:
        mov ebp, [jp_sc+ecx*4]
        mov eax, [ebp+JC_TD]
        test bl, 1
        jz .c1
        cmp byte [jp_h_hay+eax], 0
        je jp_mal_datos
.c1:
        imul eax, eax, JH_TAM
        add eax, jp_h
        mov [ebp+JC_DCP], eax
        mov eax, [ebp+JC_TA]
        add eax, 4
        test bl, 2
        jz .c2
        cmp byte [jp_h_hay+eax], 0
        je jp_mal_datos
.c2:
        imul eax, eax, JH_TAM
        add eax, jp_h
        mov [ebp+JC_ACP], eax
        inc ecx
        cmp ecx, [jp_ns]
        jb .c
        ret

; (interna) al empezar un barrido y en cada reinicio: sin bits y sin memoria
; de lo anterior -> EBX=0 EDX=0
jp_barrido_ini:
        xor eax, eax
        mov [jp_falsos], eax
        mov [jp_marca], eax
        mov [jp_eob], eax
        mov [jp_c+JC_PRED], eax
        mov [jp_c+JC_TAM+JC_PRED], eax
        mov [jp_c+2*JC_TAM+JC_PRED], eax
        mov [jp_c+3*JC_TAM+JC_PRED], eax
        mov eax, [jp_ri]
        mov [jp_faltan], eax
        xor ebx, ebx
        xor edx, edx
        ret

; -----------------------------------------------------------------------------
;  Los bits. En todo lo que sigue: ESI=por dónde van los datos, EBX=los bits
;  ya leídos (arriba, los primeros) y EDX=cuántos son
; -----------------------------------------------------------------------------
; (interna) rellena hasta tener más de 24 bits. Conserva ECX; usa EAX
jp_llena:
        push ecx
.mas:
        cmp edx, 24
        ja .x
        cmp dword [jp_marca], 0
        jne .falso
        cmp esi, [jp_fin]
        jae .final
        movzx eax, byte [esi]
        inc esi
        cmp al, 0xFF
        je .ff
.pon:
        mov ecx, 24
        sub ecx, edx
        shl eax, cl
        or ebx, eax
        add edx, 8
        jmp .mas
.ff:
        cmp esi, [jp_fin]
        jae .final
        mov cl, [esi]
        test cl, cl
        jnz .ff1
        inc esi                     ; FF 00: un FF de los datos
        jmp .pon
.ff1:
        cmp cl, 0xFF
        jne .marcador
        inc esi                     ; FF FF: relleno; se mira el siguiente
        jmp .ff
.marcador:
        dec esi                     ; un marcador: se deja el cursor en él
        movzx ecx, cl
        mov [jp_marca], ecx
        jmp .falso
.final:
        mov dword [jp_marca], 0x100
.falso:
        add edx, 8                  ; ceros de mentira
        add dword [jp_falsos], 8
        jmp .mas
.x:
        pop ecx
        ret

; (interna) después de cada bloque: ¿se ha gastado algún bit de mentira?
jp_mira_falsos:
        cmp dword [jp_falsos], 0
        je .x
        cmp edx, [jp_falsos]
        jae .x
        cmp dword [jp_marca], 0x100
        je jp_mal_corto
        jmp jp_mal_datos
.x:
        ret

; (interna) antes de cada MCU: si toca, el marcador de reinicio
jp_mira_reinicio:
        cmp dword [jp_ri], 0
        je .x
        cmp dword [jp_faltan], 0
        jne .f
        cmp dword [jp_marca], 0x100
        je jp_mal_corto
        lea eax, [esi+2]            ; tiene que venir ya: FF (y más FF de
        cmp eax, [jp_fin]           ; relleno, si acaso) y RSTn
        ja jp_mal_corto
        cmp byte [esi], 0xFF
        jne jp_mal_datos
.ff:
        inc esi
        cmp esi, [jp_fin]
        jae jp_mal_corto
        mov al, [esi]
        cmp al, 0xFF
        je .ff
        and al, 0xF8
        cmp al, 0xD0
        jne jp_mal_datos
        inc esi
        call jp_barrido_ini
.f:
        dec dword [jp_faltan]
.x:
        ret

; (interna) EAX=una tabla de Huffman -> EAX=el símbolo que viene. Usa ECX
jp_simbolo:
        cmp edx, 16
        jae .hay
        push eax
        call jp_llena
        pop eax
.hay:
        mov ecx, ebx
        shr ecx, 24
        movzx ecx, word [eax+JH_RAP+ecx*2]
        test cl, cl
        jz .largo
        movzx eax, ch
        shl ebx, cl
        and ecx, 0xFF
        sub edx, ecx
        ret
.largo:
        ; de 9 a 16 bits: el primer largo en el que lo leído no pasa del mayor
        ; código que hay de ese largo
        push edi
        push ebp
        mov ebp, eax
        mov edi, ebx
        shr edi, 16
        mov ecx, 7                  ; (16 - largo)
.l:
        mov eax, edi
        shr eax, cl
        push ecx
        neg ecx
        add ecx, 16
        cmp eax, [ebp+JH_MAX+ecx*4]
        jle .es
        pop ecx
        dec ecx
        jns .l
        jmp jp_mal_datos            ; no es ningún código
.es:
        add eax, [ebp+JH_DELTA+ecx*4]
        and eax, 0xFF
        movzx eax, byte [ebp+JH_VAL+eax]
        shl ebx, cl
        sub edx, ecx
        pop ecx
        pop ebp
        pop edi
        ret

; (interna) EAX=cuántos bits (0-15) -> EAX=el número que dicen: si el primero
; es 1, tal cual; si es 0, negativo (lo leído menos 2^n - 1). Usa ECX
jp_recibe:
        test eax, eax
        jz .x
        mov ecx, eax
        cmp edx, ecx
        jae .hay
        call jp_llena
.hay:
        mov eax, ebx
        shl ebx, cl
        sub edx, ecx
        neg ecx
        add ecx, 32
        test eax, eax
        js .mas
        not eax
        shr eax, cl
        neg eax
        ret
.mas:
        shr eax, cl
.x:
        ret

; (interna) -> CF=el bit que viene. Usa EAX
jp_bit:
        test edx, edx
        jnz .hay
        call jp_llena
.hay:
        dec edx
        shl ebx, 1
        ret

; (interna) ECX=cuántos bits (0-15) -> EAX=su valor. Usa ECX
jp_bitsn:
        xor eax, eax
        test ecx, ecx
        jz .x
        cmp edx, ecx
        jae .hay
        call jp_llena
.hay:
        mov eax, ebx
        shl ebx, cl
        sub edx, ecx
        neg ecx
        add ecx, 32
        shr eax, cl
.x:
        ret

; -----------------------------------------------------------------------------
;  Los bloques
; -----------------------------------------------------------------------------
; (interna) un bloque secuencial entero: EBP=su componente, EDI=su tabla de
; cuantización -> jp_blk, ya multiplicado y en su orden (fila * 8 + columna)
jp_bloque_base:
        push edi
        mov edi, jp_blk
        mov ecx, 64
        xor eax, eax
        rep stosd
        pop edi
        mov eax, [ebp+JC_DCP]       ; el DC: la diferencia con el del bloque anterior
        call jp_simbolo
        cmp eax, 15
        ja jp_mal_datos
        call jp_recibe
        add eax, [ebp+JC_PRED]
        mov [ebp+JC_PRED], eax
        movzx ecx, word [edi]
        imul eax, ecx
        mov [jp_blk], eax
        mov dword [jp_k], 1
.ac:
        mov eax, [ebp+JC_ACP]       ; los AC: cuántos ceros hay delante y cuántos
        call jp_simbolo             ; bits tiene el que viene
        mov ecx, eax
        shr ecx, 4
        and eax, 15
        jz .cero
        add ecx, [jp_k]
        cmp ecx, 63
        ja jp_mal_datos
        mov [jp_k], ecx
        call jp_recibe
        mov ecx, [jp_k]
        push edx
        movzx edx, word [edi+ecx*2]
        imul eax, edx
        movzx edx, byte [jp_zz+ecx]
        mov [jp_blk+edx*4], eax
        pop edx
        inc ecx
        mov [jp_k], ecx
        cmp ecx, 64
        jb .ac
        ret
.cero:
        cmp ecx, 15                 ; 16 ceros seguidos; si no, el bloque se acaba
        jne .fin
        mov ecx, [jp_k]
        add ecx, 16
        mov [jp_k], ecx
        cmp ecx, 64
        jb .ac
.fin:
        ret

; Los bloques guardados (EDI=el bloque: 8 octetos de mapa de bits y sus
; coeficientes; EBP=su componente).
; (interna) secuencial, en un barrido de los que se guardan
jp_b_sec:
        push edi
        mov edi, jp_unos
        call jp_bloque_base
        pop edi
        push edx
        xor ecx, ecx
.k:
        mov edx, [ebp+JC_MAPA]
        movzx edx, byte [edx+ecx]
        cmp dl, 0xFF
        je .k1
        movzx eax, byte [jp_zz+ecx]
        mov eax, [jp_blk+eax*4]
        mov [edi+8+edx*2], ax
.k1:
        inc ecx
        cmp ecx, 64
        jb .k
        pop edx
        ret

; (interna) progresivo, el DC por primera vez: como siempre, pero sin sus
; últimos bits
jp_b_dc0:
        mov eax, [ebp+JC_DCP]
        call jp_simbolo
        cmp eax, 15
        ja jp_mal_datos
        call jp_recibe
        add eax, [ebp+JC_PRED]
        mov [ebp+JC_PRED], eax
        mov ecx, [jp_b_al]
        shl eax, cl
        mov [edi+8], ax
        ret

; (interna) progresivo, un bit más del DC
jp_b_dc1:
        call jp_bit
        jnc .x
        mov eax, [jp_p1]
        or [edi+8], ax
.x:
        ret

; (interna) progresivo, una banda de AC por primera vez. Como en el
; secuencial, salvo que un bloque puede acabar diciendo que los n siguientes
; tampoco tienen nada en esta banda ([jp_eob])
jp_b_ac0:
        cmp dword [jp_eob], 0
        je .dec
        dec dword [jp_eob]
        ret
.dec:
        mov eax, [jp_b_ss]
        mov [jp_k], eax
.l:
        mov eax, [ebp+JC_ACP]
        call jp_simbolo
        mov ecx, eax
        shr ecx, 4
        and eax, 15
        jz .cero
        add ecx, [jp_k]
        cmp ecx, [jp_b_se]
        ja jp_mal_datos
        mov [jp_k], ecx
        call jp_recibe
        mov ecx, [jp_b_al]
        shl eax, cl
        mov ecx, [jp_k]
        bts [edi], ecx
        add ecx, [ebp+JC_MAPA]
        movzx ecx, byte [ecx]
        cmp cl, 0xFF
        je .sig
        mov [edi+8+ecx*2], ax
.sig:
        mov ecx, [jp_k]
        inc ecx
        mov [jp_k], ecx
        cmp ecx, [jp_b_se]
        jbe .l
        ret
.cero:
        cmp ecx, 15
        je .zrl
        call jp_tirada
        ret
.zrl:
        mov ecx, [jp_k]
        add ecx, 16
        mov [jp_k], ecx
        cmp ecx, [jp_b_se]
        jbe .l
        ret

; (interna) ECX=r (0-14): este bloque se acaba aquí y, con él, 2^r - 1 más los
; r bits que vienen -> [jp_eob]
jp_tirada:
        push ecx
        call jp_bitsn
        pop ecx
        mov [jp_tmp], eax
        mov eax, 1
        shl eax, cl
        dec eax
        add eax, [jp_tmp]
        mov [jp_eob], eax
        ret

; (interna) progresivo, un bit más de una banda de AC. De los coeficientes que
; ya no eran cero viene un bit de corrección de cada uno, mezclados con los
; nuevos (que solo pueden valer +1 o -1 en este bit): cada código dice cuántos
; coeficientes A CERO hay que saltar antes de poner el nuevo, y por el camino
; se van corrigiendo los que no lo eran
jp_b_ac1:
        mov eax, [jp_b_ss]
        mov [jp_k], eax
        cmp dword [jp_eob], 0
        je .l
        dec dword [jp_eob]
        jmp .resto
.l:
        mov eax, [ebp+JC_ACP]
        call jp_simbolo
        mov ecx, eax
        shr ecx, 4
        and eax, 15
        jnz .nuevo
        cmp ecx, 15
        je .salta                   ; 16 ceros (EAX=0: no se pone nada)
        call jp_tirada
        jmp .resto
.nuevo:
        cmp eax, 1
        jne jp_mal_datos
        call jp_bit                 ; su signo
        mov eax, [jp_p1]
        jc .salta
        neg eax
.salta:
        mov [jp_nv], eax
        mov [jp_r], ecx
.av:
        mov ecx, [jp_k]
        cmp ecx, [jp_b_se]
        ja .fin
        inc dword [jp_k]
        bt [edi], ecx
        jnc .a_cero
        call jp_refina
        jmp .av
.a_cero:
        cmp dword [jp_r], 0
        je .pon
        dec dword [jp_r]
        jmp .av
.pon:
        mov eax, [jp_nv]
        test eax, eax
        jz .sig
        bts [edi], ecx
        add ecx, [ebp+JC_MAPA]
        movzx ecx, byte [ecx]
        cmp cl, 0xFF
        je .sig
        mov [edi+8+ecx*2], ax
.sig:
        mov ecx, [jp_k]
        cmp ecx, [jp_b_se]
        jbe .l
.fin:
        ret
.resto:
        ; el bloque no trae nada nuevo: solo las correcciones de los que hay
        mov ecx, [jp_k]
        cmp ecx, [jp_b_se]
        ja .fin
        inc dword [jp_k]
        bt [edi], ecx
        jnc .resto
        call jp_refina
        jmp .resto

; (interna) ECX=un coeficiente que no es cero: su bit de corrección
jp_refina:
        call jp_bit
        jnc .x
        add ecx, [ebp+JC_MAPA]
        movzx ecx, byte [ecx]
        cmp cl, 0xFF
        je .x                       ; (no se guarda: solo había que leer el bit)
        movsx eax, word [edi+8+ecx*2]
        test eax, [jp_p1]
        jnz .x
        test eax, eax
        js .menos
        add eax, [jp_p1]
        jmp .g
.menos:
        sub eax, [jp_p1]
.g:
        mov [edi+8+ecx*2], ax
.x:
        ret

; -----------------------------------------------------------------------------
;  Los barridos
; -----------------------------------------------------------------------------
; (interna) una tira (una fila de MCU) de una imagen de un solo barrido: de
; los datos a los planos
jp_tira_flujo:
        mov esi, [jp_p]
        mov ebx, [jp_bits]
        mov edx, [jp_nbits]
        mov dword [jp_mx], 0
.mcu:
        call jp_mira_reinicio
        mov dword [jp_j], 0
.comp:
        mov eax, [jp_j]
        mov ebp, [jp_sc+eax*4]
        mov dword [jp_bv], 0
.v:
        mov dword [jp_bh], 0
.h:
        mov edi, [ebp+JC_QP]
        call jp_bloque_base
        call jp_mira_falsos
        mov eax, [jp_bv]
        imul eax, [ebp+JC_N]
        imul eax, [ebp+JC_PASO]
        mov ecx, [jp_mx]
        imul ecx, [ebp+JC_H]
        add ecx, [jp_bh]
        imul ecx, [ebp+JC_N]
        add eax, ecx
        add eax, [ebp+JC_PLANO]
        mov edi, eax
        mov ecx, [ebp+JC_PASO]
        call jp_idct
        inc dword [jp_bh]
        mov eax, [jp_bh]
        cmp eax, [ebp+JC_H]
        jb .h
        inc dword [jp_bv]
        mov eax, [jp_bv]
        cmp eax, [ebp+JC_V]
        jb .v
        inc dword [jp_j]
        mov eax, [jp_j]
        cmp eax, [jp_ns]
        jb .comp
        inc dword [jp_mx]
        mov eax, [jp_mx]
        cmp eax, [jp_mcux]
        jb .mcu
        mov [jp_p], esi
        mov [jp_bits], ebx
        mov [jp_nbits], edx
        ret

; (interna) lo que falta de una tira que se ha quedado a medias, en gris
jp_gris:
        mov ebp, jp_c
        mov ecx, [jp_nc]
.c:
        push ecx
        mov eax, [jp_mx]
        imul eax, [ebp+JC_H]
        imul eax, [ebp+JC_N]
        mov edx, [ebp+JC_PASO]
        sub edx, eax
        jbe .sig
        mov ebx, [ebp+JC_V]
        imul ebx, [ebp+JC_N]
        mov edi, [ebp+JC_PLANO]
        add edi, eax
.f:
        push edi
        mov ecx, edx
        mov al, 128
        rep stosb
        pop edi
        add edi, [ebp+JC_PASO]
        dec ebx
        jnz .f
.sig:
        add ebp, JC_TAM
        pop ecx
        dec ecx
        jnz .c
        ret

; (interna) un barrido entero, a los coeficientes guardados. ESI=sus datos
; -> ESI detrás de lo que ha gastado
jp_barrido:
        call jp_pon_barrido
        call jp_barrido_ini
        cmp dword [jp_ns], 1
        jne .varios
        ; un componente solo: sus bloques seguidos, y solo los que cubren la imagen
        mov ebp, [jp_sc]
        mov dword [jp_by], 0
.y:
        mov dword [jp_bx], 0
.x:
        call jp_mira_reinicio
        mov eax, [jp_by]
        imul eax, [ebp+JC_BW]
        add eax, [jp_bx]
        imul eax, [ebp+JC_BTAM]
        add eax, [ebp+JC_COEF]
        mov edi, eax
        call [jp_blq_fn]
        call jp_mira_falsos
        inc dword [jp_bx]
        mov eax, [jp_bx]
        cmp eax, [ebp+JC_BWR]
        jb .x
        inc dword [jp_by]
        mov eax, [jp_by]
        cmp eax, [ebp+JC_BHR]
        jb .y
        ret
.varios:
        ; varios: por MCU, de cada componente sus H x V bloques
        mov dword [jp_my], 0
.my:
        mov dword [jp_mx], 0
.mx:
        call jp_mira_reinicio
        mov dword [jp_j], 0
.comp:
        mov eax, [jp_j]
        mov ebp, [jp_sc+eax*4]
        mov dword [jp_bv], 0
.v:
        mov dword [jp_bh], 0
.h:
        mov eax, [jp_my]
        imul eax, [ebp+JC_V]
        add eax, [jp_bv]
        imul eax, [ebp+JC_BW]
        mov ecx, [jp_mx]
        imul ecx, [ebp+JC_H]
        add eax, ecx
        add eax, [jp_bh]
        imul eax, [ebp+JC_BTAM]
        add eax, [ebp+JC_COEF]
        mov edi, eax
        call [jp_blq_fn]
        call jp_mira_falsos
        inc dword [jp_bh]
        mov eax, [jp_bh]
        cmp eax, [ebp+JC_H]
        jb .h
        inc dword [jp_bv]
        mov eax, [jp_bv]
        cmp eax, [ebp+JC_V]
        jb .v
        inc dword [jp_j]
        mov eax, [jp_j]
        cmp eax, [jp_ns]
        jb .comp
        inc dword [jp_mx]
        mov eax, [jp_mx]
        cmp eax, [jp_mcux]
        jb .mx
        inc dword [jp_my]
        mov eax, [jp_my]
        cmp eax, [jp_mcuy]
        jb .my
        ret

; (interna) una tira, de los coeficientes guardados a los planos
jp_tira_coef:
        mov ebp, jp_c
        mov ecx, [jp_nc]
.c:
        push ecx
        mov dword [jp_bv], 0
.v:
        mov dword [jp_bh], 0
.h:
        mov eax, [jp_tira_n]
        imul eax, [ebp+JC_V]
        add eax, [jp_bv]
        imul eax, [ebp+JC_BW]
        add eax, [jp_bh]
        imul eax, [ebp+JC_BTAM]
        add eax, [ebp+JC_COEF]
        mov esi, eax
        mov edi, jp_blk
        mov ecx, 64
        xor eax, eax
        rep stosd
        mov edi, [ebp+JC_QP]
        xor ecx, ecx
.k:
        mov edx, [ebp+JC_MAPA]
        movzx edx, byte [edx+ecx]
        cmp dl, 0xFF
        je .k1
        movsx eax, word [esi+8+edx*2]
        test eax, eax
        jz .k1
        movzx edx, word [edi+ecx*2]
        imul eax, edx
        movzx edx, byte [jp_zz+ecx]
        mov [jp_blk+edx*4], eax
.k1:
        inc ecx
        cmp ecx, 64
        jb .k
        mov eax, [jp_bv]
        imul eax, [ebp+JC_N]
        imul eax, [ebp+JC_PASO]
        mov ecx, [jp_bh]
        imul ecx, [ebp+JC_N]
        add eax, ecx
        add eax, [ebp+JC_PLANO]
        mov edi, eax
        mov ecx, [ebp+JC_PASO]
        call jp_idct
        inc dword [jp_bh]
        mov eax, [jp_bh]
        cmp eax, [ebp+JC_BW]
        jb .h
        inc dword [jp_bv]
        mov eax, [jp_bv]
        cmp eax, [ebp+JC_V]
        jb .v
        add ebp, JC_TAM
        pop ecx
        dec ecx
        jnz .c
        ret

; -----------------------------------------------------------------------------
;  La transformada inversa: de jp_blk a los N x N puntos del componente EBP,
;  en EDI (ECX=lo que mide una fila del plano). Conserva todo
; -----------------------------------------------------------------------------
jp_idct:
        pushad
        mov eax, [ebp+JC_MAT]
        mov [jp_mat], eax
        mov ebp, [ebp+JC_N]
        cmp ebp, 1
        je .dc
        push edi
        mov edi, jp_blk+4
        mov ecx, 63
        xor eax, eax
        repe scasd
        pop edi
        jne .entera
.dc:
        ; solo el DC (o reducido a un punto): todo el bloque vale su media
        mov eax, [jp_blk]
        add eax, 4
        sar eax, 3
        add eax, 128
        test eax, 0xFFFFFF00
        jz .dc1
        sar eax, 31
        not eax
.dc1:
        mov edx, [esp+24]
        mov ebx, ebp
.dc2:
        mov ecx, ebp
        push edi
        rep stosb
        pop edi
        add edi, edx
        dec ebx
        jnz .dc2
        popad
        ret
.entera:
        ; por columnas: de cada una salen N valores (con 2 bits de más)
        xor ebx, ebx
.col:
        lea esi, [jp_blk+ebx*4]
        lea ecx, [jp_ws+ebx*4]
        mov eax, [esi]
        or eax, [esi+32]
        or eax, [esi+64]
        or eax, [esi+96]
        or eax, [esi+128]
        or eax, [esi+160]
        or eax, [esi+192]
        or eax, [esi+224]
        jnz .c_hay
        mov edx, ebp
.c0:
        mov dword [ecx], 0
        add ecx, 32
        dec edx
        jnz .c0
        jmp .c_sig
.c_hay:
        mov edx, [jp_mat]
        push ebp
.cy:
        mov edi, 1 << 11
        JP_TERM 0, 0
        JP_TERM 32, 4
        JP_TERM 64, 8
        JP_TERM 96, 12
        JP_TERM 128, 16
        JP_TERM 160, 20
        JP_TERM 192, 24
        JP_TERM 224, 28
        sar edi, 12
        mov [ecx], edi
        add ecx, 32
        add edx, 32
        dec ebp
        jnz .cy
        pop ebp
.c_sig:
        inc ebx
        cmp ebx, 8
        jb .col
        ; por filas, y a su sitio, entre 0 y 255
        mov ebx, [esp]
        mov esi, jp_ws
        mov ecx, ebp
.fy:
        push ecx
        mov edx, [jp_mat]
        xor ecx, ecx
.fx:
        mov edi, 1 << 15
        JP_TERM 0, 0
        JP_TERM 4, 4
        JP_TERM 8, 8
        JP_TERM 12, 12
        JP_TERM 16, 16
        JP_TERM 20, 20
        JP_TERM 24, 24
        JP_TERM 28, 28
        sar edi, 16
        add edi, 128
        test edi, 0xFFFFFF00
        jz .ok
        sar edi, 31
        not edi
.ok:
        mov eax, edi
        mov [ebx+ecx], al
        add edx, 32
        inc ecx
        cmp ecx, ebp
        jb .fx
        pop ecx
        add ebx, [esp+24]
        add esi, 32
        dec ecx
        jnz .fy
        popad
        ret

; -----------------------------------------------------------------------------
;  De los planos de la tira a una fila de puntos -> ESI=la fila
; -----------------------------------------------------------------------------
jp_haz_fila:
        ; de cada componente, su fila; si va submuestreado a lo ancho, ampliada
        ; repitiendo puntos (a lo alto, se repite la fila)
        mov ebp, jp_c
        mov ecx, [jp_nc]
.c:
        push ecx
        mov eax, [jp_fy]            ; su fila: fy * (V * N) / (Vmax * N de la imagen)
        imul eax, [ebp+JC_V]
        imul eax, [ebp+JC_N]
        mov ecx, [jp_vmax]
        imul ecx, [jp_n]
        xor edx, edx
        div ecx
        imul eax, [ebp+JC_PASO]
        add eax, [ebp+JC_PLANO]
        mov esi, eax
        mov eax, [jp_hmax]          ; a lo ancho, lo mismo: de cada H * N puntos
        imul eax, [jp_n]            ; suyos salen Hmax * N de la imagen
        mov [jp_den], eax
        mov eax, [ebp+JC_H]
        imul eax, [ebp+JC_N]
        cmp eax, [jp_den]
        je .igual
        mov edi, [ebp+JC_EXP]
        mov [ebp+JC_FILA], edi
        mov ecx, [jp_sal_an]
        mov ebx, eax
        add eax, eax
        cmp eax, [jp_den]
        jne .dda
        inc ecx                     ; lo corriente: al doble
        shr ecx, 1
.dos:
        lodsb
        mov ah, al
        stosw
        dec ecx
        jnz .dos
        jmp .sig
.dda:
        xor edx, edx                ; cualquier otra proporción
.d1:
        mov al, [esi]
        stosb
        add edx, ebx
        cmp edx, [jp_den]
        jb .d2
        sub edx, [jp_den]
        inc esi
.d2:
        dec ecx
        jnz .d1
        jmp .sig
.igual:
        mov [ebp+JC_FILA], esi
.sig:
        add ebp, JC_TAM
        pop ecx
        dec ecx
        jnz .c
        ; y juntos
        mov esi, [jp_c+JC_FILA]
        mov eax, [jp_nc]
        cmp eax, 1
        je .x                       ; gris: tal cual está
        mov edi, [jp_fila_p]
        mov ecx, [jp_sal_an]
        mov ebx, [jp_c+JC_TAM+JC_FILA]
        mov edx, [jp_c+2*JC_TAM+JC_FILA]
        cmp eax, 4
        je .cuatro
        cmp dword [jp_tr], 0
        je .rgb
.ycc:
        JP_YCC
        inc esi
        inc ebx
        inc edx
        dec ecx
        jnz .ycc
        jmp .hecha
.rgb:
        mov al, [esi]
        stosb
        mov al, [ebx]
        stosb
        mov al, [edx]
        stosb
        inc esi
        inc ebx
        inc edx
        dec ecx
        jnz .rgb
        jmp .hecha
.cuatro:
        mov eax, [jp_c+3*JC_TAM+JC_FILA]
        mov [jp_kp], eax
.c4:
        cmp dword [jp_tr], 0
        je .c4_tal
        JP_YCC                      ; YCCK: lo girado es el CMY al revés
        not byte [edi-3]
        not byte [edi-2]
        not byte [edi-1]
        jmp .c4_k
.c4_tal:
        mov al, [esi]
        stosb
        mov al, [ebx]
        stosb
        mov al, [edx]
        stosb
.c4_k:
        mov ebp, [jp_kp]
        mov al, [ebp]
        inc ebp
        mov [jp_kp], ebp
        test dword [jp_opc], JPEG_COMP
        jz .c4_rgb
        stosb
        jmp .c4_sig
.c4_rgb:
        call jp_cmyk
.c4_sig:
        inc esi
        inc ebx
        inc edx
        dec ecx
        jnz .c4
.hecha:
        mov esi, [jp_fila_p]
.x:
        ret

; (interna) AL=negro, y en [EDI-3] cian, magenta y amarillo: los cambia por
; rojo, verde y azul. Los de Adobe (los que hay) vienen al revés: 255 es sin
; tinta, y entonces cada color es su componente por el negro
jp_cmyk:
        push ecx
        push ebx
        movzx ebx, al
        mov ecx, -3
.c:
        movzx eax, byte [edi+ecx]
        cmp dword [jp_adobe], 0
        jne .c1
        xor al, 0xFF
.c1:
        push ebx
        cmp dword [jp_adobe], 0
        jne .c2
        xor bl, 0xFF
.c2:
        imul eax, ebx
        pop ebx
        add eax, 128                ; entre 255, redondeando
        push ebx
        mov ebx, eax
        shr ebx, 8
        add eax, ebx
        shr eax, 8
        pop ebx
        mov [edi+ecx], al
        inc ecx
        jnz .c
        pop ebx
        pop ecx
        ret
