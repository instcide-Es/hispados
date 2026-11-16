; =============================================================================
;  barra.inc - el aspecto común de los programas de «Productividad»: la barra
;  de botones con iconos, el pie (la barra de estado) y los iconos
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Todos los programas de la familia (Pluma, Cuentas, Facturas y los que
;  vengan) pintan su barra y su pie con esto, para que se vean y se manejen
;  igual: botones de 22 puntos de alto sobre arena, el botón puesto en
;  petróleo, los mismos iconos para las mismas órdenes y, al pasar el ratón
;  por un botón, lo que hace escrito en el pie.
;
;  Una fila de botones es una tabla; de cada botón, B_TAM octetos:
;      dd x, ancho, dibujo, orden, nombre, clase
;  y acaba en uno de ancho 0. «dibujo» es, según la clase: BT_TEXTO,
;  BT_NEGRITA y BT_SUBRAYA, un texto; BT_ICONO, un icono (ic_*, de aquí o
;  propio, con la misma forma); BT_COLOR, un color de la paleta (una muestra).
;  «nombre» es lo que sale en el pie. «orden» es del programa.
;
;  El programa pone la macro BARRA_BSS en su «section .bss» y define:
;      barra_estado   EDI=el botón, ESI=su dibujo -> EBP=1 si está puesto
;                     ahora (llega a 0); puede cambiar ESI (un texto que varía)
;
;    br_fondo     ECX=ancho EDX=alto: el fondo de la barra, arriba de la ventana
;    br_fila      EDI=tabla EBX=y: pinta una fila de botones
;    br_boton_en  EAX=x ESI=tabla -> ESI=el botón que hay ahí; CF=1 si ninguno
;    br_sobre_pon EDI=nombre del botón bajo el ratón (0: ninguno) -> ZF=1 si
;                 es el mismo de antes (no hay que repintar)
;    br_pie       ECX=ancho y EDX=alto de la ventana: pinta el pie y, si el
;                 ratón está sobre un botón, su nombre (CF=1); si no, CF=0 y
;                 el programa escribe lo suyo en (8, alto - BR_PIE + 3)
;    br_icono     ESI=icono EAX=x EBX=y, sobre el color [br_fondo_c]
;
;  Un icono son 16 filas de 16 caracteres: «.» deja ver el botón; las cifras y
;  las letras de la «a» a la «f», los colores de la paleta de Hispa Ventanas.
; =============================================================================
BR_AL       equ 22                  ; alto de un botón
BR_PIE      equ 20                  ; alto del pie
B_TAM       equ 24
BT_TEXTO    equ 0
BT_ICONO    equ 1
BT_NEGRITA  equ 2
BT_SUBRAYA  equ 3
BT_COLOR    equ 4

%macro BARRA_BSS 0
br_y        resd 1
br_sobre    resd 1                  ; el nombre del botón que hay bajo el ratón
br_fondo_c  resb 1
            alignb 4
br_buf      resb 256
%endmacro

br_fondo:
        pushad
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        HV dc_rect
        COLOR C_ARENA_OSC
        lea ebx, [edx-1]
        mov edx, 1
        HV dc_rect
        popad
        ret

br_fila:
        pushad
        mov [br_y], ebx
.b:
        cmp dword [edi+4], 0
        je .x
        mov esi, [edi+8]
        xor ebp, ebp
        call barra_estado           ; EBP=1 si está puesto
        cmp dword [edi+20], BT_COLOR
        jne .normal
        ; un color: su muestra, con doble marco si es el de ahora
        mov eax, esi
        HV dc_color
        mov eax, [edi]
        mov ebx, [br_y]
        mov ecx, [edi+4]
        mov edx, BR_AL
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        test ebp, ebp
        jz .sig
        COLOR C_BLANCO
        add eax, 2
        add ebx, 2
        sub ecx, 4
        sub edx, 4
        HV dc_marco
        jmp .sig
.normal:
        mov byte [br_fondo_c], C_CREMA
        test ebp, ebp
        jz .c1
        mov byte [br_fondo_c], C_PETROLEO
.c1:
        movzx eax, byte [br_fondo_c]
        HV dc_color
        mov eax, [edi]
        mov ebx, [br_y]
        mov ecx, [edi+4]
        mov edx, BR_AL
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        cmp dword [edi+20], BT_ICONO
        jne .txt
        mov eax, [edi+4]
        sub eax, 16
        sar eax, 1
        add eax, [edi]
        add ebx, 3
        call br_icono
        jmp .sig
.txt:
        test ebp, ebp
        jz .c2
        COLOR C_CREMA
.c2:
        xor ecx, ecx                ; el texto, centrado
.lg:
        cmp byte [esi+ecx], 0
        je .lg1
        inc ecx
        jmp .lg
.lg1:
        shl ecx, 3
        mov eax, [edi+4]
        sub eax, ecx
        sar eax, 1
        add eax, [edi]
        mov ebx, [br_y]
        add ebx, 3
        cmp dword [edi+20], BT_NEGRITA
        jne .t
        NEGRITA 1
.t:
        HV dc_texto
        NEGRITA 0
        cmp dword [edi+20], BT_SUBRAYA
        jne .sig
        add ebx, 15
        mov edx, 1
        HV dc_rect
.sig:
        add edi, B_TAM
        jmp .b
.x:
        popad
        ret

br_boton_en:
        push ecx
.b:
        cmp dword [esi+4], 0
        je .no
        mov ecx, eax
        sub ecx, [esi]
        cmp ecx, [esi+4]
        jb .es
        add esi, B_TAM
        jmp .b
.es:
        pop ecx
        clc
        ret
.no:
        pop ecx
        stc
        ret

br_sobre_pon:
        cmp edi, [br_sobre]
        mov [br_sobre], edi
        ret

br_pie:
        pushad
        COLOR C_ARENA
        xor eax, eax
        lea ebx, [edx-BR_PIE]
        mov edx, BR_PIE
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        COLOR C_TINTA
        mov esi, [br_sobre]
        test esi, esi
        jz .nada
        mov eax, 8
        add ebx, 3
        HV dc_texto
        popad
        stc
        ret
.nada:
        popad
        clc
        ret

br_icono:
        pushad
        mov edi, br_buf
        mov ecx, 256
.c:
        lodsb
        cmp al, '.'
        jne .c1
        mov al, [br_fondo_c]
        jmp short .c3
.c1:
        cmp al, '9'
        ja .c2
        sub al, '0'
        jmp short .c3
.c2:
        sub al, 'a' - 10
.c3:
        stosb
        dec ecx
        jnz .c
        mov eax, [esp+28]
        mov ebx, [esp+16]
        mov ecx, 16
        mov edx, 16
        mov esi, br_buf
        HV dc_copiar
        popad
        ret

; ---- los iconos comunes (los dibuja tools/iconos.py; el calderón, a mano) ----
ic_nuevo:
        db "................"
        db "...00000000....."
        db "...099999900...."
        db "...0999999000..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0000000000..."
        db "................"
ic_abrir:
        db "................"
        db "................"
        db "................"
        db "..000000........"
        db "..0aaaa0........"
        db ".00000000000000."
        db ".0aaaaaaaaaaaa0."
        db ".09999999999990."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".00000000000000."
        db "................"
        db "................"
        db "................"
ic_guardar:
        db "................"
        db "................"
        db "..000000000000.."
        db "..0c09999990c0.."
        db "..0c09999990c0.."
        db "..0c09999990c0.."
        db "..0c00000000c0.."
        db "..0cccccccccc0.."
        db "..0cccccccccc0.."
        db "..0cc000000cc0.."
        db "..0cc0ff0f0cc0.."
        db "..0cc0ff0f0cc0.."
        db "..0cc0ffff0cc0.."
        db "..000000000000.."
        db "................"
        db "................"
ic_imprimir:
        db "................"
        db "....00000000...."
        db "....09999990...."
        db "....09999990...."
        db "....09999990...."
        db "....09999990...."
        db ".00000000000000."
        db ".0ffffffffffff0."
        db ".0ffffffffffbf0."
        db ".0ffffffffffff0."
        db ".0ff00000000ff0."
        db ".00009999990000."
        db "....09777790...."
        db "....09999990...."
        db "....00000000...."
        db "................"
ic_cortar:
        db "................"
        db "....0......0...."
        db ".....0....0....."
        db "......0...0....."
        db "......0..0......"
        db ".......00......."
        db ".......00......."
        db "......0.0......."
        db "......0..0......"
        db ".....0....0....."
        db "...dd......dd..."
        db "..d..d....d..d.."
        db "..d..d....d..d.."
        db "...dd......dd..."
        db "................"
        db "................"
ic_copiar:
        db "................"
        db "..00000000......"
        db "..09999990......"
        db "..09999990......"
        db "..09999990......"
        db "..099900000000.."
        db "..099909999990.."
        db "..099909999990.."
        db "..099909777790.."
        db "..099909999990.."
        db "..000009777790.."
        db "......09999990.."
        db "......09777790.."
        db "......09999990.."
        db "......00000000.."
        db "................"
ic_pegar:
        db "................"
        db "......0000......"
        db "..00000ff00000.."
        db "..0aaa0000aaa0.."
        db "..0aaaaaaaaaa0.."
        db "..0aaaaaaaaaa0.."
        db "..0aa000000000.."
        db "..0aa099999900.."
        db "..0aa097777900.."
        db "..0aa099999900.."
        db "..0aa097777900.."
        db "..0aa099999900.."
        db "..0aa099999900.."
        db "..0aa000000000.."
        db "..000000000000.."
        db "................"
ic_brocha:
        db "................"
        db ".............1.."
        db "............111."
        db "...........111.."
        db "..........111..."
        db ".........111...."
        db "........111....."
        db "....0000.1......"
        db "...0aaaa0......."
        db "..0aaaaa0......."
        db ".0aaaaa0........"
        db ".0aaaa0........."
        db ".0aaa0.........."
        db "..000..........."
        db "................"
        db "................"
ic_deshacer:
        db "................"
        db "................"
        db ".....00000......"
        db "...00ccccc00...."
        db "..0cc00000cc0..."
        db "..0c0.....0c0..."
        db ".0c0.......0c0.."
        db "00c00......0c0.."
        db "0ccc0......0c0.."
        db ".0c0.......0c0.."
        db "..0.......0c0..."
        db ".........0c0...."
        db ".......00c0....."
        db ".......000......"
        db "................"
        db "................"
ic_rehacer:
        db "................"
        db "................"
        db "......00000....."
        db "....00ccccc00..."
        db "...0cc00000cc0.."
        db "...0c0.....0c0.."
        db "..0c0.......0c0."
        db "..0c0......00c00"
        db "..0c0......0ccc0"
        db "..0c0.......0c0."
        db "...0c0.......0.."
        db "....0c0........."
        db ".....0c00......."
        db "......000......."
        db "................"
        db "................"
ic_izq:
        db "................"
        db "................"
        db "..00000000000..."
        db "................"
        db "..0000000......."
        db "................"
        db "..00000000000..."
        db "................"
        db "..00000........."
        db "................"
        db "..00000000000..."
        db "................"
        db "..00000000......"
        db "................"
        db "................"
        db "................"
ic_cen:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "....00000000...."
        db "................"
        db "..000000000000.."
        db "................"
        db ".....000000....."
        db "................"
        db "..000000000000.."
        db "................"
        db "....00000000...."
        db "................"
        db "................"
        db "................"
ic_der:
        db "................"
        db "................"
        db "...00000000000.."
        db "................"
        db ".......0000000.."
        db "................"
        db "...00000000000.."
        db "................"
        db ".........00000.."
        db "................"
        db "...00000000000.."
        db "................"
        db "......00000000.."
        db "................"
        db "................"
        db "................"
ic_just:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db "................"
ic_numera:
        db "................"
        db "................"
        db "..c............."
        db ".cc...00000000.."
        db "..c............."
        db "................"
        db ".cc............."
        db "..cc..00000000.."
        db ".cc............."
        db "................"
        db ".ccc............"
        db "..cc..00000000.."
        db ".ccc............"
        db "................"
        db "................"
        db "................"
ic_vinetas:
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "................"
        db "................"
ic_sang_mas:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db ".......0000000.."
        db ".c.............."
        db ".cc............."
        db ".cccc..0000000.."
        db ".cc............."
        db ".c.............."
        db ".......0000000.."
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
ic_sang_menos:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db ".......0000000.."
        db "....c..........."
        db "...cc..........."
        db ".cccc..0000000.."
        db "...cc..........."
        db "....c..........."
        db ".......0000000.."
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
ic_borrar:
        db "................"
        db "................"
        db "................"
        db "...dd.......dd.."
        db "....dd.....dd..."
        db ".....dd...dd...."
        db "......dd.dd....."
        db ".......ddd......"
        db ".......ddd......"
        db "......dd.dd....."
        db ".....dd...dd...."
        db "....dd.....dd..."
        db "...dd.......dd.."
        db "................"
        db "................"
        db "................"
ic_mas:
        db "................"
        db "................"
        db "................"
        db ".......bb......."
        db ".......bb......."
        db ".......bb......."
        db ".......bb......."
        db "...bbbbbbbbbb..."
        db "...bbbbbbbbbb..."
        db ".......bb......."
        db ".......bb......."
        db ".......bb......."
        db ".......bb......."
        db "................"
        db "................"
        db "................"
ic_menos:
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
        db "...dddddddddd..."
        db "...dddddddddd..."
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
        db "................"
ic_visto:
        db "................"
        db "................"
        db "................"
        db ".............bb."
        db "............bb.."
        db "...........bb..."
        db "...........bb..."
        db "..........bb...."
        db "..bb.....bb....."
        db "...bb...bb......"
        db "....bb..bb......"
        db ".....bbbb......."
        db "......bb........"
        db "................"
        db "................"
        db "................"
ic_sigma:
        db "................"
        db "................"
        db "................"
        db "...000000000...."
        db "...00.....0....."
        db "....00.........."
        db ".....00........."
        db "......00........"
        db ".....00........."
        db "....00.........."
        db "...00.....0....."
        db "...000000000...."
        db "................"
        db "................"
        db "................"
        db "................"
ic_lapiz:
        db "................"
        db "............d..."
        db "...........a.d.."
        db "..........a.a.d."
        db ".........a.a.a.."
        db "........a.a.a..."
        db ".......a.a.a...."
        db "......a.a.a....."
        db ".....a.a.a......"
        db "....a.a.a......."
        db "...0.a.a........"
        db "...00.a........."
        db "..0.00.........."
        db "..00............"
        db "................"
        db "................"
ic_texto:
        db "................"
        db "...0000000000..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0977777790..."
        db "...0999999990..."
        db "...0977777790..."
        db "...0999999990..."
        db "...0977777790..."
        db "...0999999990..."
        db "...0977779990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0000000000..."
        db "................"
ic_buscar:
        db "................"
        db "................"
        db ".....000........"
        db "....0...0......."
        db "...0.....0......"
        db "...0.....0......"
        db "...0.....0......"
        db "....0...0......."
        db ".....000........"
        db ".........00....."
        db "..........00...."
        db "...........00..."
        db "............00.."
        db ".............00."
        db "................"
        db "................"
ic_volver:
        db "................"
        db "................"
        db "................"
        db "........c......."
        db ".......cc......."
        db "......cc........"
        db ".....cc........."
        db "....cc.........."
        db "...ccccccccccc.."
        db "....cc.........."
        db ".....cc........."
        db "......cc........"
        db ".......cc......."
        db "........c......."
        db "................"
        db "................"
ic_grafico:
        db "................"
        db "................"
        db "................"
        db "............aa.."
        db "............aa.."
        db "......dd....aa.."
        db "......dd....aa.."
        db "......dd.bb.aa.."
        db "......dd.bb.aa.."
        db "...cc.dd.bb.aa.."
        db "...cc.dd.bb.aa.."
        db "...cc.dd.bb.aa.."
        db "...cc.dd.bb.aa.."
        db "..000000000000.."
        db "................"
        db "................"
ic_hoja:
        db "................"
        db "................"
        db ".00000000000000."
        db ".0bbb7bbbb7bbb0."
        db ".0bbb7bbbb7bbb0."
        db ".07777777777770."
        db ".09997999979990."
        db ".09997999979990."
        db ".07777777777770."
        db ".09997999979990."
        db ".09997999979990."
        db ".07777777777770."
        db ".09997999979990."
        db ".00000000000000."
        db "................"
        db "................"
ic_prelim:
        db "................"
        db "..000000000....."
        db "..099999990....."
        db "..099999990....."
        db "..097777790....."
        db "..099999990....."
        db "..097777790....."
        db "..0999999cc....."
        db "..099999c99c...."
        db "..099999c99c...."
        db "..0999999cc....."
        db "..099999990.c..."
        db "..099999990.cc.."
        db "..000000000..cc."
        db "................"
        db "................"
ic_marcas:
        db "................"
        db "................"
        db "....000000000..."
        db "...0000.00......"
        db "..00000.00......"
        db "..00000.00......"
        db "..00000.00......"
        db "...0000.00......"
        db "....000.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "................"
        db "................"
