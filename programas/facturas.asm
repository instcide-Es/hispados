; =============================================================================
;  FACTURAS.HVP - Facturas, las cuentas de casa y de un pequeño negocio
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Lleva dos listas de facturas: las EMITIDAS (las que hace uno a sus
;  clientes) y las RECIBIDAS (los gastos: la luz, el agua, el taller...). De
;  cada factura guarda el número, la fecha, el cliente o el proveedor, hasta
;  doce líneas (concepto, cantidad y precio), el IVA, la retención y si está
;  pagada. La factura se ve en la ventana como saldrá en el papel, y se puede
;  imprimir (puerto paralelo) o guardar como texto (.TXT, para el Bloc de notas).
;  Abajo, en la lista, el resumen: lo facturado, lo pendiente y el saldo del
;  año; «Resumen» da las bases y el IVA de cada trimestre.
;
;  El dinero se lleva en CÉNTIMOS, con enteros (nada de coma flotante): las
;  cantidades en milésimas y los porcentajes en centésimas. Cada importe se
;  redondea al céntimo (el medio céntimo, hacia arriba) una sola vez:
;       importe de la línea = cantidad x precio
;       base  = suma de los importes
;       IVA   = base x tipo      retención = base x tipo
;       total = base + IVA - retención
;  Si la factura dice que sus precios llevan el IVA incluido (lo normal en los
;  recibos de casa: «la luz, 65,70»), es al revés: la suma de los importes es
;  lo que se paga, y de ella se sacan la base y el IVA:
;       base = suma / (1 + tipo)      IVA = suma - base
;
;  Los datos van en FACTURAS.DAT, al lado del módulo, y se escriben cada vez
;  que cambian. Es un módulo .HVP (ventanas/hvp.inc) del grupo «Productividad».
; =============================================================================
%include "hvp.inc"

MAX_FAC     equ 200                 ; facturas (entre emitidas y recibidas)
MAX_LIN     equ 12                  ; líneas de una factura
PCT_MAX     equ 5000                ; el IVA o la retención más altos: 50 %
IMP_MAX     equ 99999999            ; el importe más alto de una línea: 999.999,99

; ---- la factura (F_TAM octetos) ----
F_TIPO      equ 0                   ; db: 0 emitida, 1 recibida
F_PAGADA    equ 1                   ; db: 1 cobrada o pagada
F_NLIN      equ 2                   ; db: cuántas líneas tiene
F_INCL      equ 3                   ; db: 1 si los precios llevan el IVA incluido
F_IVA       equ 4                   ; dd: IVA, en centésimas de % (2100 = 21 %)
F_RET       equ 8                   ; dd: retención, lo mismo
F_FECHA     equ 12                  ; dd: aaaammdd
F_NUM       equ 16                  ; 16: el número
F_CLI       equ 32                  ; 40: cliente o proveedor
F_NIF       equ 72                  ; 16
F_DIR       equ 88                  ; 56
F_LIN       equ 144                 ; las líneas
L_CON       equ 0                   ; 36: concepto
L_CANT      equ 36                  ; dd: cantidad, en milésimas
L_PRE       equ 40                  ; dd: precio, en céntimos
L_TAM       equ 48
F_TAM       equ F_LIN + MAX_LIN * L_TAM

; ---- la cabecera del archivo (CAB_TAM octetos) ----
CAB_N       equ 8                   ; dd: cuántas facturas
E_NOM       equ 16                  ; 40: mis datos: nombre
E_NIF       equ 56                  ; 16
E_DIR       equ 72                  ; 56
E_DIR2      equ 128                 ; 56: código postal y población
E_NOTA      equ 184                 ; 56: el pie (forma de pago, cuenta...)
CAB_TAM     equ 256

; ---- la ventana ----
TB_AL       equ 30                  ; la barra de botones
LY0         equ TB_AL + 22          ; la primera fila de la lista
FIL_AL      equ 18
PIE_AL      equ 44                  ; el resumen, abajo
FLY0        equ 190                 ; la primera línea de la factura
FLIN_AL     equ 16

; ---- órdenes ----
O_NUEVA     equ 101
O_ABRIR     equ 102
O_BORRAR    equ 103
O_TEXTO     equ 104
O_IMPRIMIR  equ 105
O_SALIR     equ 106
O_DUPLICAR  equ 107
O_CSV       equ 108
O_DATOS     equ 111
O_LIN_MAS   equ 112
O_LIN_CAMB  equ 113
O_LIN_QUITA equ 114
O_PAGADA    equ 115
O_LISTA     equ 116
O_EMITIDAS  equ 121
O_RECIBIDAS equ 122
O_RESUMEN   equ 123
O_MIS       equ 131
O_ACERCA    equ 141

HVP_CABECERA "Facturas", entrada, 4
        db "................................"
        db "................................"
        db "......00000000000000000........."
        db "......099999999999999900........"
        db "......0999999999999999090......."
        db "......09999999999999990990......"
        db "......099999999999999900000....."
        db "......0999999999999999999990...."
        db "......0992222222999999999990...."
        db "......0992222222999999999990...."
        db "......0999999999999999999990...."
        db "......0997777777777997777990...."
        db "......0999999999999999999990...."
        db "......0997777777777997777990...."
        db "......0999999999999999999990...."
        db "......0997777777777997777990...."
        db "......0999999999999999999990...."
        db "......0997777777777997777990...."
        db "......0999999999999999999990...."
        db "......0990000000000000000990...."
        db "......0999999999999999999990...."
        db "......0999999999944444444990...."
        db "......0999999999944444444990...."
        db "......0999999999944444444990...."
        db "......0999999999999999999990...."
        db "......0999999999999999999990...."
        db "......0000000000000000000000...."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_fac       dd fac_proc
            db C_CREMA, 0, 0, 0
fac_plant:
        PLANTILLA c_fac, t_titulo, E_NORMAL, 50, 36, 700, 540, 0

fac_menu:
        db "&Archivo", 0
        dw O_NUEVA
        db "&Nueva factura...        Ins", 0
        dw O_ABRIR
        db "&Abrir                 Intro", 0
        dw O_DUPLICAR
        db "&Duplicar factura", 0
        dw O_BORRAR
        db "&Borrar factura", 0
        dw 0
        db 0
        dw O_TEXTO
        db "&Guardar como texto...", 0
        dw O_CSV
        db "&Exportar la lista a Cuentas...", 0
        dw O_IMPRIMIR
        db "&Imprimir", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Factura", 0
        dw O_DATOS
        db "&Datos de la factura...   F2", 0
        dw O_LIN_MAS
        db "&Añadir línea...         Ins", 0
        dw O_LIN_CAMB
        db "&Cambiar línea...      Intro", 0
        dw O_LIN_QUITA
        db "&Quitar línea           Supr", 0
        dw 0
        db 0
        dw O_PAGADA
        db "&Pagada: sí o no           P", 0
        dw 0
        db 0
        dw O_LISTA
        db "&Volver a la lista       Esc", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_EMITIDAS
        db "Facturas &emitidas", 0
        dw O_RECIBIDAS
        db "Facturas &recibidas (gastos)", 0
        dw 0
        db 0
        dw O_RESUMEN
        db "Re&sumen del año...", 0
        dw 0xFFFF
        db "&Datos", 0
        dw O_MIS
        db "&Mis datos...", 0
        dw 0xFFFF
        db "A&yuda", 0
        dw O_ACERCA
        db "&Acerca de Facturas...", 0
        dw 0xFFFF
        db 0

; ---- los botones de la barra (programas/comun/barra.inc): x, ancho, dibujo,
;      orden, nombre y clase ----
            align 4
bot_lista   dd 8, 24, ic_nuevo, O_NUEVA, n_nueva, BT_ICONO
            dd 34, 24, ic_abrir, O_ABRIR, n_abrir, BT_ICONO
            dd 60, 24, ic_copiar, O_DUPLICAR, n_duplicar, BT_ICONO
            dd 86, 24, ic_borrar, O_BORRAR, n_borrar, BT_ICONO
            dd 112, 24, ic_hoja, O_CSV, n_csv, BT_ICONO
            dd 152, 80, t_b_emit, O_EMITIDAS, n_emit, BT_TEXTO
            dd 236, 88, t_b_recib, O_RECIBIDAS, n_recib, BT_TEXTO
            dd 336, 80, t_b_resumen, O_RESUMEN, n_resumen, BT_TEXTO
            dd 420, 88, t_b_mis, O_MIS, n_mis, BT_TEXTO
            dd 0, 0, 0, 0, 0, 0
bot_fac     dd 8, 24, ic_volver, O_LISTA, n_lista, BT_ICONO
            dd 40, 64, t_b_datos, O_DATOS, n_datos, BT_TEXTO
            dd 108, 80, t_b_mas, O_LIN_MAS, n_mas, BT_TEXTO
            dd 192, 72, t_b_cambiar, O_LIN_CAMB, n_cambiar, BT_TEXTO
            dd 268, 64, t_b_quitar, O_LIN_QUITA, n_quitar, BT_TEXTO
            dd 336, 72, t_b_pagada, O_PAGADA, n_pagada, BT_TEXTO
            dd 420, 24, ic_texto, O_TEXTO, n_texto, BT_ICONO
            dd 446, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, BT_ICONO
            dd 0, 0, 0, 0, 0, 0
n_nueva     db "Nueva factura", 0
n_abrir     db "Abrir la factura elegida", 0
n_borrar    db "Borrar la factura elegida", 0
n_csv       db "Exportar esta lista a un archivo para Cuentas", 0
n_duplicar  db "Duplicar: una factura nueva con los mismos datos y líneas", 0
n_emit      db "Las facturas que hago yo", 0
n_recib     db "Las facturas que me hacen a mí", 0
n_resumen   db "Resumen del trimestre", 0
n_mis       db "Mis datos: los que salen en las facturas", 0
n_lista     db "Volver a la lista de facturas", 0
n_datos     db "Los datos de la factura", 0
n_mas       db "Añadir una línea", 0
n_cambiar   db "Cambiar la línea elegida", 0
n_quitar    db "Quitar la línea elegida", 0
n_pagada    db "Marcar la factura como cobrada o pagada", 0
n_texto     db "Guardar la factura en un archivo de texto", 0
n_imprimir  db "Imprimir la factura", 0

; ---- diálogo: los datos de la factura ----
dlg_fac:
        PLANTILLA HV_c_dialogo, t_d_fac, E_DIALOGO, 0, 0, 440, 446, 0
        PLANTILLA HV_c_estatico, t_l_num, E_HIJA, 16, 18, 110, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 14, 140, 24, 110
        PLANTILLA HV_c_estatico, t_l_fecha, E_HIJA, 16, 50, 110, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 46, 100, 24, 111
        PLANTILLA HV_c_estatico, t_l_ddmm, E_HIJA, 240, 50, 180, 18, 107
df_lab_cli:
        PLANTILLA HV_c_estatico, t_l_cli, E_HIJA, 16, 82, 110, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 78, 254, 24, 112
        PLANTILLA HV_c_boton, t_b_elegir, E_HIJA, 388, 78, 32, 24, 130
        PLANTILLA HV_c_estatico, t_l_nif, E_HIJA, 16, 114, 110, 18, 103
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 110, 140, 24, 113
        PLANTILLA HV_c_estatico, t_l_dir, E_HIJA, 16, 146, 110, 18, 104
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 142, 290, 24, 114
        PLANTILLA HV_c_estatico, t_l_iva, E_HIJA, 16, 178, 110, 18, 105
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 174, 60, 24, 115
        PLANTILLA HV_c_estatico, t_l_ret, E_HIJA, 16, 210, 110, 18, 106
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 206, 60, 24, 116
        PLANTILLA HV_c_estatico, t_l_ret2, E_HIJA, 200, 210, 220, 18, 108
        PLANTILLA HV_c_boton, t_c_incl, E_HIJA | E_TABULA | BS_CASILLA, 130, 238, 306, 20, 118
df_pagada:
        PLANTILLA HV_c_boton, t_c_cobrada, E_HIJA | E_TABULA | BS_CASILLA, 130, 262, 200, 20, 117
        ; y, de paso, el importe: una línea de la factura sin salir de aquí
        PLANTILLA HV_c_estatico, t_l_otra, E_HIJA | SS_NEGRITA, 16, 296, 404, 18, 109
        PLANTILLA HV_c_estatico, t_l_con, E_HIJA, 16, 326, 110, 18, 121
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 322, 290, 24, 119
        PLANTILLA HV_c_estatico, t_l_imp, E_HIJA, 16, 358, 110, 18, 122
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 354, 100, 24, 120
        PLANTILLA HV_c_estatico, t_l_imp2, E_HIJA, 240, 358, 190, 18, 123
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 130, 396, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 230, 396, 90, 26, ID_CANCELAR
        dd 0
cam_fac     dd 110, c_num, 16
            dd 111, c_fecha, 12
            dd 112, c_cli, 40
            dd 113, c_nif, 16
            dd 114, c_dir, 56
            dd 115, c_iva, 8
            dd 116, c_ret, 8
            dd 117, c_pag, 0
            dd 118, c_incl, 0
            dd 119, c_con, 36
            dd 120, c_pre, 12
            dd 0

; ---- diálogo: elegir un cliente (o proveedor) de las facturas ya hechas ----
dlg_cli:
        PLANTILLA HV_c_dialogo, t_d_cli, E_DIALOGO, 0, 0, 340, 300, 0
        PLANTILLA HV_c_lista, 0, E_HIJA | E_TABULA, 16, 12, 304, 204, 60
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 70, 228, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 170, 228, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: una línea ----
dlg_lin:
        PLANTILLA HV_c_dialogo, t_d_lin, E_DIALOGO, 0, 0, 440, 190, 0
        PLANTILLA HV_c_estatico, t_l_con, E_HIJA, 16, 18, 110, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 14, 290, 24, 110
        PLANTILLA HV_c_estatico, t_l_cant, E_HIJA, 16, 50, 110, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 46, 100, 24, 111
        PLANTILLA HV_c_estatico, t_l_pre, E_HIJA, 16, 82, 110, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 130, 78, 100, 24, 112
dl_pre2:
        PLANTILLA HV_c_estatico, t_l_pre2, E_HIJA, 240, 82, 190, 18, 103
        PLANTILLA HV_c_estatico, t_l_cant2, E_HIJA, 240, 50, 190, 18, 104
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 130, 122, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 230, 122, 90, 26, ID_CANCELAR
        dd 0
cam_lin     dd 110, c_con, 36
            dd 111, c_cant, 12
            dd 112, c_pre, 12
            dd 0

; ---- diálogo: mis datos ----
dlg_mis:
        PLANTILLA HV_c_dialogo, t_d_mis, E_DIALOGO, 0, 0, 460, 262, 0
        PLANTILLA HV_c_estatico, t_l_nom, E_HIJA, 16, 18, 126, 18, 100
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 146, 14, 294, 24, 110
        PLANTILLA HV_c_estatico, t_l_nif, E_HIJA, 16, 50, 126, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 146, 46, 140, 24, 111
        PLANTILLA HV_c_estatico, t_l_dir, E_HIJA, 16, 82, 126, 18, 102
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 146, 78, 294, 24, 112
        PLANTILLA HV_c_estatico, t_l_pob, E_HIJA, 16, 114, 126, 18, 103
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 146, 110, 294, 24, 113
        PLANTILLA HV_c_estatico, t_l_nota, E_HIJA, 16, 146, 126, 18, 104
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 146, 142, 294, 24, 114
        PLANTILLA HV_c_estatico, t_l_nota2, E_HIJA, 146, 170, 294, 18, 105
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 146, 198, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 246, 198, 90, 26, ID_CANCELAR
        dd 0
cam_mis     dd 110, tmp_emi + E_NOM - E_NOM, 40
            dd 111, tmp_emi + E_NIF - E_NOM, 16
            dd 112, tmp_emi + E_DIR - E_NOM, 56
            dd 113, tmp_emi + E_DIR2 - E_NOM, 56
            dd 114, tmp_emi + E_NOTA - E_NOM, 56
            dd 0

; ---- diálogo: el resumen del año ----
dlg_res:
        PLANTILLA HV_c_dialogo, t_d_res, E_DIALOGO, 0, 0, 640, 296, 0
        PLANTILLA HV_c_estatico, res_buf, E_HIJA, 16, 14, 608, 206, 100
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 275, 232, 90, 26, ID_ACEPTAR
        dd 0
cam_nada    dd 0

; ---- textos ----
t_titulo    db "Facturas", 0
t_b_emit    db "Emitidas", 0
t_b_recib   db "Recibidas", 0
t_b_resumen db "Resumen", 0
t_b_mis     db "Mis datos", 0
t_b_datos   db "Datos", 0
t_b_mas     db "+ Línea", 0
t_b_cambiar db "Cambiar", 0
t_b_quitar  db "Quitar", 0
t_b_pagada  db "Pagada", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_d_fac     db "Datos de la factura", 0
t_d_lin     db "Línea de la factura", 0
t_d_mis     db "Mis datos", 0
t_d_res     db "Resumen del año", 0
t_l_num     db "Número:", 0
t_l_fecha   db "Fecha:", 0
t_l_ddmm    db "(día/mes/año)", 0
t_l_cli     db "Cliente:", 0
t_l_prov    db "Proveedor:", 0
t_l_nif     db "NIF:", 0
t_l_dir     db "Dirección:", 0
t_l_iva     db "IVA (%):", 0
t_l_ret     db "Retención (%):", 0
t_l_ret2    db "(IRPF; vacío si no lleva)", 0
t_c_cobrada db "Ya está cobrada", 0
t_c_pagada  db "Ya está pagada", 0
t_l_con     db "Concepto:", 0
t_l_cant    db "Cantidad:", 0
t_l_cant2   db "(vacío: una)", 0
t_l_pre     db "Precio:", 0
t_l_pre2    db "(de una unidad, sin IVA)", 0
t_l_pre2i   db "(de una unidad, con IVA)", 0
t_c_incl    db "Los precios llevan el IVA incluido", 0
t_l_otra    db "El importe (una línea más de la factura):", 0
t_l_imp     db "Importe:", 0
t_l_imp2    db "(vacío: no añade nada)", 0
t_con_def   db "Importe", 0
t_incl      db "(precios con el IVA incluido)", 0
t_l_nom     db "Nombre:", 0
t_l_pob     db "C. P. y población:", 0
t_l_nota    db "Pie de la factura:", 0
t_l_nota2   db "(forma de pago, cuenta del banco...)", 0
t_h_num     db "Número", 0
t_h_fecha   db "Fecha", 0
t_h_cli     db "Cliente", 0
t_h_prov    db "Proveedor", 0
t_h_total   db "Total", 0
t_h_estado  db "Estado", 0
t_cobrada   db "Cobrada", 0
t_pagada    db "Pagada", 0
t_pendiente db "Pendiente", 0
t_pend_cobro db "Pendiente de cobro", 0
t_pend_pago db "Pendiente de pago", 0
t_vacia_e   db "No hay facturas emitidas. Pulse «Nueva» para hacer la primera.", 0
t_vacia_r   db "No hay facturas recibidas. Pulse «Nueva» para apuntar la primera.", 0
t_factura   db "FACTURA", 0
t_fac_rec   db "FACTURA RECIBIDA", 0
t_p_num     db "Número: ", 0
t_p_fecha   db "Fecha:  ", 0
t_p_estado  db "Estado: ", 0
t_p_cliente db "Cliente:", 0
t_p_nif     db "NIF:", 0
t_p_con     db "Concepto", 0
t_p_cant    db "Cantidad", 0
t_p_pre     db "Precio", 0
t_p_imp     db "Importe", 0
t_p_base    db "Base imponible:", 0
t_p_iva     db "IVA ", 0
t_p_ret     db "Retención ", 0
t_p_pct     db " %:", 0
t_p_total   db "TOTAL:", 0
t_sin_lin   db "(sin líneas: pulse «+ Línea» para añadir la primera)", 0
t_sin_mis   db "(rellene «Mis datos»)", 0
t_nada      db 0
t_s_fact    db " facturas    Total: ", 0
t_s_fact1   db " factura    Total: ", 0
t_s_pend    db "    Pendiente: ", 0
t_s_ano     db "Año ", 0
t_s_ing     db ":  ingresos ", 0
t_s_gas     db "    gastos ", 0
t_s_saldo   db "    saldo ", 0
t_r_cab     db "            Ingresos      Gastos IVA repercut. IVA soportado   Diferencia", 10, 0
t_r_trim    db "º trim.", 0
t_r_ano     db "Año     ", 0
t_r_nota    db 10, "Ingresos y gastos: las bases, sin IVA. Diferencia: IVA repercutido", 10
            db "menos IVA soportado (lo que sale a ingresar en cada trimestre).", 0
t_ext_txt   db ".TXT", 0
t_b_elegir  db "...", 0
t_d_cli     db "Elegir de las facturas anteriores", 0
t_e_sin_cli db "Todavía no hay ninguno: salen de las facturas ya hechas.", 0
t_ext_csv   db ".CSV", 0
t_csv_emi   db "EMITIDAS.CSV", 0
t_csv_rec   db "RECIBIDA.CSV", 0
t_csv_tit   db "Exportar la lista para Cuentas", 0
t_csv_cab   db "Número;Fecha;Nombre;NIF;Base;IVA;Retención;Total;Pagada", 13, 10, 0
t_csv_si    db "sí", 0
t_csv_no    db "no", 0
t_csv_hecho db "La lista se ha guardado. Se abre con Cuentas.", 0
t_e_csv_nada db "No hay facturas en esta lista.", 0
t_fac_nom   db "FACTURA", 0
t_guardar_como db "Guardar la factura como texto", 0
t_raya      db "------------------------------------------------------------------------", 0
t_e_llena   db "No caben más facturas (el tope son 200).", 10, "Borre alguna antigua.", 0
t_e_lineas  db "Una factura no puede tener más de doce líneas.", 0
t_e_nombre  db "Escriba el nombre del cliente o del proveedor.", 0
t_e_fecha   db "La fecha no vale. Escríbala así: 16/11/2026.", 0
t_e_iva     db "El IVA tiene que ser un porcentaje de 0 a 50 (por ejemplo 21 o 10,5).", 0
t_e_ret     db "La retención tiene que ser un porcentaje de 0 a 50 (o nada).", 0
t_e_con     db "Escriba el concepto de la línea.", 0
t_e_cant    db "La cantidad no vale. Escriba un número, como 2 o 1,5.", 0
t_e_pre     db "El precio no vale. Escriba un número, como 12,50.", 0
t_e_imp     db "El importe de la línea es demasiado grande", 10, "(como mucho, 999.999,99).", 0
t_e_guardar db "No se han podido guardar los datos en el disco.", 0
t_e_texto   db "No se ha podido guardar el archivo de texto.", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_e_ninguna db "No hay ninguna factura elegida.", 0
t_e_linea   db "No hay ninguna línea elegida.", 0
t_impresa   db "La factura se ha enviado a la impresora.", 0
t_guardada  db "La factura se ha guardado como texto.", 0
t_q_borrar  db "¿Borrar esta factura? No se puede deshacer.", 0
t_q_quitar  db "¿Quitar esta línea de la factura?", 0
t_acerca_tit db "Acerca de Facturas", 0
t_acerca    db "Facturas", 10
            db "Las facturas de casa y de un pequeño negocio:", 10
            db "las que se hacen y las que se reciben, con su IVA,", 10
            db "lo que queda por cobrar y el resumen de cada trimestre.", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov edi, ruta_dat
        mov eax, 'DAT'
        call hvl_ruta
        mov esi, dlg_fac
        call hvl_clases
        mov esi, dlg_lin
        call hvl_clases
        mov esi, dlg_cli
        call hvl_clases
        mov esi, dlg_mis
        call hvl_clases
        mov esi, dlg_res
        call hvl_clases
        call hoy_leer
        call cargar
        call ordena
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub eax, 16
        cmp eax, 700
        jae .a
        mov [fac_plant+P_AN], eax
        mov dword [fac_plant+P_X], 8
.a:
        sub ebx, BARRA_H + 12
        cmp ebx, 540
        jae .b
        mov [fac_plant+P_AL], ebx
        mov dword [fac_plant+P_Y], BARRA_H + 4
.b:
        mov esi, fac_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        HV app_bucle
.x:
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
fac_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_LBUTTONDOWN
        je raton
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONDBLCLK
        je doble
        cmp eax, WM_MOUSEWHEEL
        je rueda
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 636 + 300 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], fac_menu
        mov dword [ebp+V_ICONO], hvp_icono
        jmp hecho
.tam:
        call inval
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
hecho:
        xor eax, eax
        ret

; (interna) la ventana, a repintar
inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        popad
        ret

; ---- las órdenes (del menú, de la barra y de las teclas): ECX=número ----
orden:
        call orden_haz
        jmp hecho
orden_haz:
        cmp ecx, O_NUEVA
        je nueva
        cmp ecx, O_ABRIR
        je abrir
        cmp ecx, O_BORRAR
        je borrar
        cmp ecx, O_TEXTO
        je a_texto
        cmp ecx, O_DUPLICAR
        je duplicar
        cmp ecx, O_CSV
        je a_csv
        cmp ecx, O_IMPRIMIR
        je imprimir
        cmp ecx, O_DATOS
        je datos
        cmp ecx, O_LIN_MAS
        je lin_mas
        cmp ecx, O_LIN_CAMB
        je lin_camb
        cmp ecx, O_LIN_QUITA
        je lin_quita
        cmp ecx, O_PAGADA
        je pagada
        cmp ecx, O_LISTA
        je a_lista
        cmp ecx, O_EMITIDAS
        je .emit
        cmp ecx, O_RECIBIDAS
        je .recib
        cmp ecx, O_RESUMEN
        je resumen
        cmp ecx, O_MIS
        je mis_datos
        cmp ecx, O_ACERCA
        je .acerca
        cmp ecx, O_SALIR
        jne .x
        mov ebp, [ven]
        HV ven_destruir
.x:
        ret
.emit:
        xor eax, eax
        jmp .tipo
.recib:
        mov eax, 1
.tipo:
        mov [ver_tipo], eax
        mov byte [modo], 0
        mov dword [sel], 0
        mov dword [top], 0
        call ordena
        jmp inval
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; ---- teclado ----
tecla:
        cmp byte [modo], 0
        jne .fac
        cmp ecx, TC_ABAJO
        je .l_aba
        cmp ecx, TC_ARRIBA
        je .l_arr
        cmp ecx, TC_AVPAG
        je .l_avp
        cmp ecx, TC_REPAG
        je .l_rep
        cmp ecx, TC_INICIO
        je .l_ini
        cmp ecx, TC_FIN
        je .l_fin
        cmp ecx, TC_TAB
        je .l_tab
        mov eax, O_ABRIR
        cmp ecx, TC_INTRO
        je .ord
        mov eax, O_BORRAR
        cmp ecx, TC_SUPR
        je .ord
        mov eax, O_NUEVA
        cmp ecx, TC_INS
        je .ord
        jmp hecho
.l_tab:
        mov ecx, O_EMITIDAS
        cmp dword [ver_tipo], 0
        jne .o2
        mov ecx, O_RECIBIDAS
.o2:
        jmp orden
.l_aba:
        mov eax, 1
        jmp .l_mueve
.l_arr:
        mov eax, -1
        jmp .l_mueve
.l_avp:
        mov eax, [vis]
        jmp .l_mueve
.l_rep:
        mov eax, [vis]
        neg eax
        jmp .l_mueve
.l_ini:
        mov eax, -MAX_FAC
        jmp .l_mueve
.l_fin:
        mov eax, MAX_FAC
.l_mueve:
        call sel_mueve
        jmp hecho
.fac:
        cmp ecx, TC_ABAJO
        je .f_aba
        cmp ecx, TC_ARRIBA
        je .f_arr
        cmp ecx, TC_AVPAG
        je .f_avp
        cmp ecx, TC_REPAG
        je .f_rep
        mov eax, O_LISTA
        cmp ecx, TC_ESC
        je .ord
        mov eax, O_LIN_MAS
        cmp ecx, TC_INS
        je .ord
        mov eax, O_LIN_QUITA
        cmp ecx, TC_SUPR
        je .ord
        mov eax, O_DATOS
        cmp ecx, TC_F1 + 1          ; F2
        je .ord
        cmp ecx, TC_INTRO
        jne hecho
        mov eax, O_LIN_CAMB
        call fac_actual
        cmp byte [ebx+F_NLIN], 0
        jne .ord
        mov eax, O_LIN_MAS
.ord:
        mov ecx, eax
        jmp orden
.f_aba:
        mov eax, 1
        jmp .f_mueve
.f_arr:
        mov eax, -1
.f_mueve:
        call lin_mueve
        jmp hecho
.f_avp:
        mov eax, 96
        jmp .f_desp
.f_rep:
        mov eax, -96
.f_desp:
        call fac_desplaza
        jmp hecho

letra:
        cmp byte [modo], 0
        je hecho
        cmp cl, 'p'
        je .p
        cmp cl, 'P'
        jne hecho
.p:
        mov ecx, O_PAGADA
        jmp orden

; (interna) mueve la elección de la lista EAX filas
sel_mueve:
        pushad
        add eax, [sel]
        mov ecx, [n_ord]
        dec ecx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [sel], eax
        call sel_a_la_vista
        call inval
        popad
        ret
; (interna) que la fila elegida se vea
sel_a_la_vista:
        push eax
        mov eax, [sel]
        cmp eax, [top]
        jae .a
        mov [top], eax
.a:
        sub eax, [vis]
        inc eax
        cmp eax, [top]
        jle .x
        mov [top], eax
.x:
        pop eax
        ret
; (interna) mueve la línea elegida de la factura EAX líneas
lin_mueve:
        pushad
        call fac_actual
        add eax, [lsel]
        movzx ecx, byte [ebx+F_NLIN]
        dec ecx
        cmp eax, ecx
        jle .a
        mov eax, ecx
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [lsel], eax
        call inval
        popad
        ret
; (interna) desplaza la factura EAX puntos
fac_desplaza:
        pushad
        add eax, [fv_y]
        cmp eax, [fv_max]
        jle .a
        mov eax, [fv_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [fv_y], eax
        call inval
        popad
        ret

rueda:
        mov eax, ecx
        cmp byte [modo], 0
        jne .f
        call sel_mueve
        jmp hecho
.f:
        shl eax, 5
        call fac_desplaza
        jmp hecho

; ---- el ratón se mueve: si está sobre un botón de la barra, su nombre ----
raton_mueve:
        xor edi, edi
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jae .n
        test ebx, ebx
        js .n
        movsx eax, dx
        mov esi, bot_lista
        cmp byte [modo], 0
        je .f
        mov esi, bot_fac
.f:
        call br_boton_en
        jc .n
        mov edi, [esi+16]
.n:
        call br_sobre_pon
        je hecho
        call inval
        jmp hecho

; ---- ratón: EDX=x + y*65536 ----
raton:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        cmp byte [modo], 0
        jne .fac
        sub ebx, LY0
        js hecho
        mov eax, ebx
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        cmp eax, [vis]
        jae hecho
        add eax, [top]
        cmp eax, [n_ord]
        jae hecho
        mov [sel], eax
        call inval
        jmp hecho
.fac:
        add ebx, [fv_y]
        sub ebx, FLY0
        js hecho
        mov eax, ebx
        xor edx, edx
        mov ecx, FLIN_AL
        div ecx
        call fac_actual
        movzx ecx, byte [ebx+F_NLIN]
        cmp eax, ecx
        jae hecho
        mov [lsel], eax
        call inval
        jmp hecho
.barra:
        mov esi, bot_lista
        cmp byte [modo], 0
        je .b
        mov esi, bot_fac
.b:
        call br_boton_en
        jc hecho
        mov dword [br_sobre], 0     ; (pulsado: su nombre ya no hace falta)
        mov ecx, [esi+12]
        jmp orden

doble:
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl raton                    ; en la barra: otro clic
        cmp byte [modo], 0
        jne .fac
        sub ebx, LY0
        js hecho
        mov eax, ebx
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        add eax, [top]
        cmp eax, [n_ord]
        jae hecho
        mov [sel], eax
        mov ecx, O_ABRIR
        jmp orden
.fac:
        add ebx, [fv_y]
        mov ecx, O_DATOS            ; arriba: los datos
        cmp ebx, FLY0 - 22
        jl orden
        sub ebx, FLY0
        js hecho
        mov eax, ebx
        xor edx, edx
        mov ecx, FLIN_AL
        div ecx
        call fac_actual
        movzx ecx, byte [ebx+F_NLIN]
        cmp eax, ecx
        mov ecx, O_LIN_MAS          ; debajo de las líneas: otra
        jae orden
        mov [lsel], eax
        mov ecx, O_LIN_CAMB
        jmp orden

; =============================================================================
;  Las facturas
; =============================================================================
; EAX=número de la factura -> EBX=su sitio
ptr_fac:
        imul ebx, eax, F_TAM
        add ebx, facs
        ret
; -> EBX=la factura abierta
fac_actual:
        push eax
        mov eax, [act]
        call ptr_fac
        pop eax
        ret
; -> EBX=la factura elegida en la lista y [act]; CF=1 (y lo dice) si no hay
fac_elegida:
        cmp byte [modo], 0
        jne .ya
        push eax
        mov eax, [sel]
        cmp eax, [n_ord]
        jae .no
        mov eax, [fac_ord+eax*4]
        mov [act], eax
        pop eax
.ya:
        call fac_actual
        clc
        ret
.no:
        pop eax
        push esi
        mov esi, t_e_ninguna
        call aviso
        pop esi
        stc
        ret

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret
; pregunta: ESI=texto -> ZF=1 si se acepta
pregunta:
        pushad
        mov ebp, [ven]
        mov edi, t_titulo
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        popad
        ret

; la lista de la pestaña ([ver_tipo]), de la más nueva a la más antigua
ordena:
        pushad
        xor edi, edi                ; cuántas van
        xor ecx, ecx                ; la factura
.f:
        cmp ecx, [n_fac]
        jae .fin
        mov eax, ecx
        call ptr_fac
        movzx eax, byte [ebx+F_TIPO]
        cmp eax, [ver_tipo]
        jne .sig
        mov edx, [ebx+F_FECHA]
        xor esi, esi                ; su sitio: delante de la primera que no
.b:                                 ; sea posterior
        cmp esi, edi
        jae .pon
        mov eax, [fac_ord+esi*4]
        call ptr_fac
        cmp [ebx+F_FECHA], edx
        jbe .pon
        inc esi
        jmp .b
.pon:
        mov eax, edi
.c:
        cmp eax, esi
        jbe .d
        mov ebx, [fac_ord+eax*4-4]
        mov [fac_ord+eax*4], ebx
        dec eax
        jmp .c
.d:
        mov [fac_ord+esi*4], ecx
        inc edi
.sig:
        inc ecx
        jmp .f
.fin:
        mov [n_ord], edi
        mov eax, [sel]
        cmp eax, edi
        jb .x
        mov eax, edi
        dec eax
        jns .x1
        xor eax, eax
.x1:
        mov [sel], eax
.x:
        popad
        ret

; pone la elección de la lista en la factura [act]
sel_en_act:
        pushad
        xor ecx, ecx
        mov eax, [act]
.b:
        cmp ecx, [n_ord]
        jae .x
        cmp [fac_ord+ecx*4], eax
        je .es
        inc ecx
        jmp .b
.es:
        mov [sel], ecx
        call sel_a_la_vista
.x:
        popad
        ret

; ---- Abrir: la factura elegida, a la vista de factura ----
abrir:
        cmp byte [modo], 0
        jne .x
        call fac_elegida
        jc .x
        mov byte [modo], 1
        mov dword [lsel], 0
        mov dword [fv_y], 0
        call inval
.x:
        ret
a_lista:
        mov byte [modo], 0
        call ordena
        call sel_en_act
        jmp inval

; ---- Nueva ----
nueva:
        cmp dword [n_fac], MAX_FAC
        jb .cabe
        mov esi, t_e_llena
        jmp aviso
.cabe:
        mov edi, tmp_fac
        mov ecx, F_TAM / 4
        xor eax, eax
        rep stosd
        mov eax, [ver_tipo]
        mov [tmp_fac+F_TIPO], al
        mov [tmp_fac+F_INCL], al    ; (en las recibidas, el IVA va incluido)
        mov dword [tmp_fac+F_IVA], 2100
        mov eax, [hoy]
        mov [tmp_fac+F_FECHA], eax
        cmp dword [ver_tipo], 0
        jne .d
        call num_auto
.d:
nueva_pon:
        call edita_datos
        jc .x
        mov eax, [n_fac]
        mov [act], eax
        call ptr_fac
        mov edi, ebx
        mov esi, tmp_fac
        mov ecx, F_TAM / 4
        rep movsd
        inc dword [n_fac]
        call guardar
        call ordena
        call sel_en_act
        mov byte [modo], 1
        mov dword [lsel], 0
        mov dword [fv_y], 0
        call inval
        call fac_actual             ; una factura sin importe no es nada: si
        cmp byte [ebx+F_NLIN], 0    ; no se ha puesto, se pide la primera línea
        jne .x
        call lin_mas
.x:
        ret

; ---- Duplicar: una factura nueva igual que la elegida (a quién, líneas, IVA),
;      con la fecha de hoy, sin cobrar y, si es emitida, con su número nuevo.
;      Se enseñan sus datos por si hay que retocarlos ----
duplicar:
        cmp dword [n_fac], MAX_FAC
        jae nueva                   ; (no cabe: lo dice)
        call fac_elegida
        jc .x
        mov esi, ebx
        mov edi, tmp_fac
        mov ecx, F_TAM / 4
        rep movsd
        mov byte [tmp_fac+F_PAGADA], 0
        mov eax, [hoy]
        mov [tmp_fac+F_FECHA], eax
        cmp byte [tmp_fac+F_TIPO], 0
        jne nueva_pon
        call num_auto
        jmp nueva_pon
.x:
        ret

; el número siguiente de las emitidas del año: «2026-007»
num_auto:
        pushad
        mov eax, [hoy]
        xor edx, edx
        mov ecx, 10000
        div ecx
        mov ebp, eax                ; el año
        xor edi, edi                ; el más alto
        xor ecx, ecx
.f:
        cmp ecx, [n_fac]
        jae .fin
        mov eax, ecx
        call ptr_fac
        cmp byte [ebx+F_TIPO], 0
        jne .s
        lea esi, [ebx+F_NUM]
        call hvl_lee_num
        cmp eax, ebp
        jne .s
        cmp byte [esi], '-'
        jne .s
        inc esi
        call hvl_lee_num
        cmp eax, edi
        jbe .s
        mov edx, eax
        mov edi, edx
.s:
        inc ecx
        jmp .f
.fin:
        lea ebx, [edi+1]
        mov edi, tmp_fac + F_NUM
        mov eax, ebp
        call hvl_num
        mov al, '-'
        stosb
        cmp ebx, 100
        jae .n
        mov al, '0'
        stosb
        cmp ebx, 10
        jae .n
        stosb
.n:
        mov eax, ebx
        call hvl_num
        mov byte [edi], 0
        popad
        ret

; ---- Datos de la factura abierta (o de la elegida) ----
datos:
        call fac_elegida
        jc .x
        mov esi, ebx
        mov edi, tmp_fac
        mov ecx, F_TAM / 4
        rep movsd
        call edita_datos
        jc .x
        call fac_actual
        mov edi, ebx
        mov esi, tmp_fac
        mov ecx, F_TAM / 4
        rep movsd
        call guardar
        call ordena
        call sel_en_act
        call inval
.x:
        ret

; el diálogo de los datos, sobre tmp_fac; CF=1 si se cancela
edita_datos:
        pushad
        ; los textos del diálogo, según sea emitida o recibida
        mov eax, t_l_cli
        mov ebx, t_c_cobrada
        cmp byte [tmp_fac+F_TIPO], 0
        je .t
        mov eax, t_l_prov
        mov ebx, t_c_pagada
.t:
        mov [df_lab_cli+P_TEXTO], eax
        mov [df_pagada+P_TEXTO], ebx
        mov esi, tmp_fac + F_NUM
        mov edi, c_num
        call copia
        mov esi, tmp_fac + F_CLI
        mov edi, c_cli
        call copia
        mov esi, tmp_fac + F_NIF
        mov edi, c_nif
        call copia
        mov esi, tmp_fac + F_DIR
        mov edi, c_dir
        call copia
        mov eax, [tmp_fac+F_FECHA]
        mov edi, c_fecha
        call fmt_fecha
        mov byte [edi], 0
        mov eax, [tmp_fac+F_IVA]
        mov ecx, 2
        mov edi, c_iva
        call fmt_dec
        mov byte [edi], 0
        mov edi, c_ret
        mov eax, [tmp_fac+F_RET]
        test eax, eax
        jz .r0
        call fmt_dec
.r0:
        mov byte [edi], 0
        mov al, [tmp_fac+F_PAGADA]
        mov [c_pag], al
        mov al, [tmp_fac+F_INCL]
        mov [c_incl], al
        mov byte [c_con], 0         ; la línea que se añade de paso: en blanco
        mov byte [c_pre], 0
.otra:
        mov dword [campos], cam_fac
        mov esi, dlg_fac
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .no
        mov esi, c_cli
        call vacio
        mov esi, t_e_nombre
        je .mal
        mov esi, c_fecha
        call lee_fecha
        mov esi, t_e_fecha
        jc .mal
        mov [tmp_fac+F_FECHA], eax
        mov esi, c_iva
        mov ecx, 2
        call lee_dec
        mov esi, t_e_iva
        jc .mal
        cmp eax, PCT_MAX
        ja .mal
        mov [tmp_fac+F_IVA], eax
        xor eax, eax
        mov esi, c_ret
        call vacio
        je .r1
        mov ecx, 2
        call lee_dec
        mov esi, t_e_ret
        jc .mal
        cmp eax, PCT_MAX
        ja .mal
.r1:
        mov [tmp_fac+F_RET], eax
        ; ¿se ha puesto un importe? Es una línea más: una unidad a ese precio
        mov esi, c_pre
        call vacio
        jne .linea
        mov esi, c_con
        call vacio
        je .sin_linea
        mov esi, t_e_pre            ; (concepto sin importe)
        jmp .mal
.linea:
        mov esi, c_pre
        mov ecx, 2
        call lee_dec
        mov esi, t_e_pre
        jc .mal
        mov edx, eax
        movzx eax, byte [tmp_fac+F_NLIN]
        mov esi, t_e_lineas
        cmp eax, MAX_LIN
        jae .mal
        imul edi, eax, L_TAM
        add edi, tmp_fac + F_LIN    ; EDI = la línea nueva
        push edi
        mov ecx, L_TAM / 4
        xor eax, eax
        rep stosd
        pop edi
        mov dword [edi+L_CANT], 1000
        mov [edi+L_PRE], edx
        mov esi, c_con
        call vacio
        jne .lc
        mov esi, t_con_def          ; sin concepto: «Importe»
.lc:
        push edi
        call copia
        pop edi
        mov esi, edi
        call importe_linea
        mov esi, t_e_imp
        jc .mal
        inc byte [tmp_fac+F_NLIN]
        mov byte [c_con], 0         ; (si hay que volver a preguntar, no se
        mov byte [c_pre], 0         ; añade otra vez)
.sin_linea:
        mov al, [c_incl]
        mov [tmp_fac+F_INCL], al
        mov esi, c_num
        mov edi, tmp_fac + F_NUM
        call copia
        mov esi, c_cli
        mov edi, tmp_fac + F_CLI
        call copia
        mov esi, c_nif
        mov edi, tmp_fac + F_NIF
        call copia
        mov esi, c_dir
        mov edi, tmp_fac + F_DIR
        call copia
        mov al, [c_pag]
        mov [tmp_fac+F_PAGADA], al
        popad
        clc
        ret
.mal:
        call aviso
        jmp .otra
.no:
        popad
        stc
        ret

; copia ESI (acabada en 0) a EDI, con su 0
copia:
        push eax
        push esi
        push edi
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop edi
        pop esi
        pop eax
        ret
; ESI=texto -> ZF=1 si está vacío (o solo tiene espacios)
vacio:
        push esi
.b:
        cmp byte [esi], ' '
        jne .f
        inc esi
        jmp .b
.f:
        cmp byte [esi], 0
        pop esi
        ret

; ---- Borrar la factura ----
borrar:
        call fac_elegida
        jc .x
        mov esi, t_q_borrar
        call pregunta
        jne .x
        call fac_actual
        mov edi, ebx                ; las de detrás, un sitio adelante
        lea esi, [ebx+F_TAM]
        mov ecx, [n_fac]
        sub ecx, [act]
        dec ecx
        imul ecx, F_TAM / 4
        rep movsd
        dec dword [n_fac]
        mov byte [modo], 0
        call guardar
        call ordena
        call inval
.x:
        ret

; ---- Pagada: sí o no ----
pagada:
        call fac_elegida
        jc .x
        xor byte [ebx+F_PAGADA], 1
        call guardar
        call inval
.x:
        ret

; ---- las líneas ----
lin_mas:
        call fac_elegida
        jc .x
        cmp byte [modo], 0
        jne .v
        call abrir
.v:
        call fac_actual
        cmp byte [ebx+F_NLIN], MAX_LIN
        jb .cabe
        mov esi, t_e_lineas
        jmp aviso
.cabe:
        mov edi, tmp_lin
        mov ecx, L_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [tmp_lin+L_CANT], 1000
        mov byte [lin_nueva], 1
        call edita_linea
        jc .x
        call fac_actual
        movzx eax, byte [ebx+F_NLIN]
        mov [lsel], eax
        inc byte [ebx+F_NLIN]
        call lin_ptr
        mov esi, tmp_lin
        mov ecx, L_TAM / 4
        rep movsd
        call guardar
        call inval
.x:
        ret
; -> EDI=la línea [lsel] de la factura EBX
lin_ptr:
        imul edi, [lsel], L_TAM
        lea edi, [ebx+edi+F_LIN]
        ret
; -> EBX=la factura abierta, con una línea elegida; CF=1 (y lo dice) si no
lin_elegida:
        cmp byte [modo], 0
        je .no
        call fac_actual
        movzx eax, byte [ebx+F_NLIN]
        cmp [lsel], eax
        jae .no
        clc
        ret
.no:
        mov esi, t_e_linea
        call aviso
        stc
        ret
lin_camb:
        call lin_elegida
        jc .x
        call lin_ptr
        mov esi, edi
        mov edi, tmp_lin
        mov ecx, L_TAM / 4
        rep movsd
        mov byte [lin_nueva], 0
        call edita_linea
        jc .x
        call fac_actual
        call lin_ptr
        mov esi, tmp_lin
        mov ecx, L_TAM / 4
        rep movsd
        call guardar
        call inval
.x:
        ret
lin_quita:
        call lin_elegida
        jc .x
        mov esi, t_q_quitar
        call pregunta
        jne .x
        call fac_actual
        call lin_ptr
        lea esi, [edi+L_TAM]
        movzx ecx, byte [ebx+F_NLIN]
        sub ecx, [lsel]
        dec ecx
        imul ecx, L_TAM / 4
        rep movsd
        dec byte [ebx+F_NLIN]
        movzx eax, byte [ebx+F_NLIN]
        cmp [lsel], eax
        jb .g
        dec eax
        jns .g1
        xor eax, eax
.g1:
        mov [lsel], eax
.g:
        call guardar
        call inval
.x:
        ret

; el diálogo de una línea, sobre tmp_lin; CF=1 si se cancela
edita_linea:
        pushad
        call fac_actual             ; el precio, ¿con el IVA o sin él?
        mov eax, t_l_pre2
        cmp byte [ebx+F_INCL], 0
        je .rot
        mov eax, t_l_pre2i
.rot:
        mov [dl_pre2+P_TEXTO], eax
        mov esi, tmp_lin + L_CON
        mov edi, c_con
        call copia
        mov byte [c_cant], 0        ; (en una línea nueva, los dos en blanco:
        mov edi, c_pre              ; sin cantidad es una)
        cmp byte [lin_nueva], 0
        jne .p0
        mov eax, [tmp_lin+L_CANT]
        mov ecx, 3
        mov edi, c_cant
        call fmt_dec
        mov byte [edi], 0
        mov edi, c_pre
        mov eax, [tmp_lin+L_PRE]
        mov ecx, 2
        call fmt_dec
.p0:
        mov byte [edi], 0
.otra:
        mov dword [campos], cam_lin
        mov esi, dlg_lin
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .no
        mov esi, c_con
        call vacio
        mov esi, t_e_con
        je .mal
        mov eax, 1000               ; sin cantidad: una
        mov esi, c_cant
        call vacio
        je .c1
        mov ecx, 3
        call lee_dec
        mov esi, t_e_cant
        jc .mal
.c1:
        mov [tmp_lin+L_CANT], eax
        mov esi, c_pre
        mov ecx, 2
        call lee_dec
        mov esi, t_e_pre
        jc .mal
        mov [tmp_lin+L_PRE], eax
        mov esi, tmp_lin
        call importe_linea
        mov esi, t_e_imp
        jc .mal
        mov esi, c_con
        mov edi, tmp_lin + L_CON
        call copia
        popad
        clc
        ret
.mal:
        call aviso
        jmp .otra
.no:
        popad
        stc
        ret

; ---- Mis datos ----
mis_datos:
        mov esi, cab + E_NOM
        mov edi, tmp_emi
        mov ecx, (CAB_TAM - E_NOM) / 4
        rep movsd
        mov dword [campos], cam_mis
        mov esi, dlg_mis
        call dialogo
        cmp eax, ID_ACEPTAR
        jne .x
        mov esi, tmp_emi
        mov edi, cab + E_NOM
        mov ecx, (CAB_TAM - E_NOM) / 4
        rep movsd
        call guardar
        call inval
.x:
        ret

; =============================================================================
;  Los diálogos: ESI=plantilla (y detrás, sus controles), [campos]=los campos
;  (dd id, búfer, tamaño; tamaño 0: una casilla y el búfer es un octeto)
;  -> EAX=el botón con el que se cierra
; =============================================================================
dialogo:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov ebp, [ven]
        lea edi, [esi+P_TAM]
        mov ebx, dlg_gancho
        xor edx, edx
        HV dlg_modal
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
dlg_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, 130                ; «...»: elegir el cliente de los de antes
        je elige_cliente
        cmp ecx, ID_ACEPTAR
        jne .no
        mov esi, [campos]
.g:
        mov ecx, [esi]
        test ecx, ecx
        jz .no
        pushad
        HV ven_hija_id
        mov ebp, eax
        cmp dword [esi+8], 0
        je .gm
        mov eax, WM_GETTEXT
        mov ecx, [esi+8]
        mov edx, [esi+4]
        HV msg_enviar
        jmp .gs
.gm:
        push esi
        mov eax, BM_MARCA
        HV msg_enviar
        pop esi
        mov edx, [esi+4]
        mov [edx], al
.gs:
        popad
        add esi, 12
        jmp .g
.ini:
        mov esi, [campos]
.i:
        mov ecx, [esi]
        test ecx, ecx
        jz .no
        pushad
        HV ven_hija_id
        mov ebp, eax
        mov edx, [esi+4]
        cmp dword [esi+8], 0
        je .im
        mov eax, WM_SETTEXT
        HV msg_enviar
        jmp .is
.im:
        movzx ecx, byte [edx]
        mov eax, BM_PONER_MARCA
        HV msg_enviar
.is:
        popad
        add esi, 12
        jmp .i
.no:
        xor eax, eax
        ret

; ---- «...» junto al nombre: una lista con los clientes (o los proveedores,
;      en las recibidas) de las facturas ya hechas, sin repetir; el elegido
;      rellena el nombre, el NIF y la dirección. EBP=el diálogo de la factura ----
elige_cliente:
        pushad
        mov [ec_dlg], ebp
        xor edi, edi                ; EDI=cuántos van
        xor ecx, ecx                ; ECX=la factura
.f:
        cmp ecx, [n_fac]
        jae .ff
        mov eax, ecx
        call ptr_fac
        movzx eax, byte [ebx+F_TIPO]
        cmp eax, [ver_tipo]
        jne .fs
        cmp byte [ebx+F_CLI], 0
        je .fs
        xor edx, edx                ; ¿está ya? (el mismo nombre)
.r:
        cmp edx, edi
        jae .nuevo
        push ecx
        push edi
        mov esi, [ec_tab+edx*4]
        add esi, F_CLI
        lea edi, [ebx+F_CLI]
        mov ecx, 40
        repe cmpsb
        pop edi
        pop ecx
        je .fs
        inc edx
        jmp .r
.nuevo:
        mov [ec_tab+edi*4], ebx
        inc edi
.fs:
        inc ecx
        jmp .f
.ff:
        mov [ec_n], edi
        test edi, edi
        jnz .hay
        mov esi, t_e_sin_cli
        call aviso
        jmp .x
.hay:
        mov dword [ec_sel], 0
        mov esi, dlg_cli
        lea edi, [esi+P_TAM]
        mov ebx, cli_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov ebx, [ec_sel]
        mov ebx, [ec_tab+ebx*4]
        lea edx, [ebx+F_CLI]
        mov ecx, 112
        call .pon
        lea edx, [ebx+F_NIF]
        mov ecx, 113
        call .pon
        lea edx, [ebx+F_DIR]
        mov ecx, 114
        call .pon
.x:
        popad
        xor eax, eax
        ret
; (interna) el texto EDX, al cuadro ECX del diálogo de la factura
.pon:
        pushad
        mov ebp, [ec_dlg]
        HV ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        HV msg_enviar
        popad
        ret
cli_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 60                  ; la lista: doble clic o Intro, aceptar
        jne .boton
        shr ecx, 16
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        HV msg_enviar
        jmp .no
.boton:
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 60
        HV ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        HV msg_enviar
        cmp eax, [ec_n]
        jae .b1
        mov [ec_sel], eax
.b1:
        popad
        jmp .no
.ini:
        pushad
        mov ecx, 60
        HV ven_hija_id
        mov ebp, eax
        mov dword [ec_i], 0
.i1:
        mov edx, [ec_i]
        mov edx, [ec_tab+edx*4]
        add edx, F_CLI
        mov eax, LM_ANADIR
        HV msg_enviar
        inc dword [ec_i]
        mov eax, [ec_i]
        cmp eax, [ec_n]
        jb .i1
        xor ecx, ecx
        mov eax, LM_PONER_SELEC
        HV msg_enviar
        popad
.no:
        xor eax, eax
        ret

; =============================================================================
;  Las cuentas (enteros: céntimos, milésimas y centésimas de %)
; =============================================================================
; ESI=línea -> EAX=su importe en céntimos: cantidad x precio, redondeado al
; céntimo; CF=1 (y EAX=0) si se sale de IMP_MAX
importe_linea:
        push ebx
        push ecx
        push edx
        mov eax, [esi+L_CANT]
        imul dword [esi+L_PRE]      ; EDX:EAX = milésimas de céntimo
        xor ebx, ebx
        test edx, edx
        jns .p
        not edx
        neg eax
        sbb edx, -1
        inc ebx
.p:
        add eax, 500                ; medio céntimo
        adc edx, 0
        cmp edx, 1000               ; (que la división no desborde)
        jae .mal
        mov ecx, 1000
        div ecx
        cmp eax, IMP_MAX
        ja .mal
        test ebx, ebx
        jz .ok
        neg eax
.ok:
        clc
        jmp .x
.mal:
        xor eax, eax
        stc
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=base (céntimos) ECX=porcentaje (centésimas) -> EAX=base x porcentaje,
; redondeado al céntimo
por_ciento:
        push ebx
        push ecx
        push edx
        imul ecx                    ; EDX:EAX = diezmilésimas de céntimo
        xor ebx, ebx
        test edx, edx
        jns .p
        not edx
        neg eax
        sbb edx, -1
        inc ebx
.p:
        add eax, 5000
        adc edx, 0
        mov ecx, 10000
        div ecx
        test ebx, ebx
        jz .x
        neg eax
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=importe con el IVA dentro (céntimos) ECX=porcentaje (centésimas) ->
; EAX=la base: importe x 10000 / (10000 + porcentaje), redondeada al céntimo
quita_iva:
        push ebx
        push ecx
        push edx
        push esi
        xor ebx, ebx
        test eax, eax
        jns .p
        neg eax
        inc ebx
.p:
        lea esi, [ecx+10000]        ; el divisor
        mov ecx, 10000
        mul ecx
        mov ecx, esi
        shr ecx, 1                  ; (la mitad, para redondear)
        add eax, ecx
        adc edx, 0
        div esi
        test ebx, ebx
        jz .x
        neg eax
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; EBX=factura -> [t_base], [t_iva], [t_ret] y [t_total]
totales:
        pushad
        xor edi, edi
        movzx ecx, byte [ebx+F_NLIN]
        lea esi, [ebx+F_LIN]
        jecxz .f
.l:
        call importe_linea
        add edi, eax
        add esi, L_TAM
        loop .l
.f:
        cmp byte [ebx+F_INCL], 0
        je .sin
        ; los precios llevan el IVA: la suma es lo que se paga; la base es la
        ; suma entre (1 + tipo), y el IVA, lo que falta hasta la suma
        mov eax, edi
        mov ecx, [ebx+F_IVA]
        call quita_iva
        mov [t_base], eax
        mov edx, edi
        sub edx, eax
        mov [t_iva], edx
        mov edi, eax
        jmp short .ret
.sin:
        mov [t_base], edi
        mov eax, edi
        mov ecx, [ebx+F_IVA]
        call por_ciento
        mov [t_iva], eax
.ret:
        mov eax, edi
        mov ecx, [ebx+F_RET]
        call por_ciento
        mov [t_ret], eax
        mov eax, edi
        add eax, [t_iva]
        sub eax, [t_ret]
        mov [t_total], eax
        popad
        ret

; suma EAX (con signo) al número de 64 bits de EDI
suma64:
        push eax
        push edx
        cdq
        add [edi], eax
        adc [edi+4], edx
        pop edx
        pop eax
        ret

; las sumas de la pestaña y del año: s_tab_tot, s_tab_pend, s_ing, s_gas
sumas:
        pushad
        mov edi, s_tab_tot
        mov ecx, 8
        xor eax, eax
        rep stosd
        mov eax, [hoy]
        xor edx, edx
        mov ecx, 10000
        div ecx
        mov ebp, eax                ; el año
        xor ecx, ecx
.f:
        cmp ecx, [n_fac]
        jae .x
        mov eax, ecx
        call ptr_fac
        call totales
        movzx eax, byte [ebx+F_TIPO]
        cmp eax, [ver_tipo]
        jne .a
        mov eax, [t_total]
        mov edi, s_tab_tot
        call suma64
        cmp byte [ebx+F_PAGADA], 0
        jne .a
        mov edi, s_tab_pend
        call suma64
.a:
        push ecx
        mov eax, [ebx+F_FECHA]
        xor edx, edx
        mov ecx, 10000
        div ecx
        pop ecx
        cmp eax, ebp
        jne .s
        mov eax, [t_total]
        mov edi, s_ing
        cmp byte [ebx+F_TIPO], 0
        je .b
        mov edi, s_gas
.b:
        call suma64
.s:
        inc ecx
        jmp .f
.x:
        popad
        ret

; =============================================================================
;  Números y fechas, a texto y de texto
; =============================================================================
; ESI=texto -> ECX=su largo
largo:
        push esi
        xor ecx, ecx
.b:
        cmp byte [esi], 0
        je .x
        inc esi
        inc ecx
        jmp .b
.x:
        pop esi
        ret

; EAX=céntimos (con signo) -> «-1.234,56» en EDI, que avanza
fmt_dinero:
        push edx
        cdq
        call fmt_dinero64
        pop edx
        ret
; lo mismo con EDX:EAX
fmt_dinero64:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push ebp
        test edx, edx
        jns .p
        not edx
        neg eax
        sbb edx, -1
        mov byte [edi], '-'
        inc edi
.p:
        mov ebx, 10
        xor ebp, ebp
        sub esp, 24                 ; las cifras, de la última a la primera
.d:
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        div ebx
        mov esi, eax
        mov eax, ecx
        div ebx
        mov [esp+ebp], dl
        inc ebp
        mov edx, esi
        mov ecx, eax
        or ecx, edx
        jnz .d
.pad:
        cmp ebp, 3
        jae .o
        mov byte [esp+ebp], 0
        inc ebp
        jmp .pad
.o:
        mov ecx, ebp
.e:
        dec ecx
        mov al, [esp+ecx]
        add al, '0'
        stosb
        cmp ecx, 2
        jb .e2
        jne .e1
        mov al, ','
        stosb
        jmp .e2
.e1:
        mov eax, ecx                ; ¿toca el punto de los millares?
        sub eax, 2
.m:
        cmp eax, 3
        jb .m1
        sub eax, 3
        jmp .m
.m1:
        test eax, eax
        jnz .e2
        mov al, '.'
        stosb
.e2:
        test ecx, ecx
        jnz .e
        add esp, 24
        pop ebp
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; EDX:EAX=céntimos ECX=ancho -> en EDI, arrimado a la derecha (EDI avanza)
fmt_din_ancho:
        push eax
        push ecx
        push esi
        push ebx
        mov ebx, edi
        push edi
        mov edi, num_tmp
        call fmt_dinero64
        mov byte [edi], 0
        pop edi
        mov esi, num_tmp
        push ecx
        call largo
        mov eax, ecx
        pop ecx
        sub ecx, eax
        jle .c
        mov al, ' '
        rep stosb
.c:
        call hvl_cad
        pop ebx
        pop esi
        pop ecx
        pop eax
        ret

; EAX=número (con signo) con ECX decimales (2 o 3) -> texto en EDI, que
; avanza: sin puntos de millares y sin ceros de más («2», «1,5», «10,25»)
fmt_dec:
        push eax
        push ebx
        push ecx
        push edx
        test eax, eax
        jns .p
        neg eax
        mov byte [edi], '-'
        inc edi
.p:
        mov ebx, 100
        cmp ecx, 2
        je .d
        mov ebx, 1000
.d:
        xor edx, edx
        div ebx
        call hvl_num
        test edx, edx
        jz .x
        mov byte [edi], ','
        inc edi
        mov eax, edx
.f:
        push eax                    ; la cifra siguiente
        mov eax, ebx
        xor edx, edx
        mov ebx, 10
        div ebx
        mov ebx, eax
        pop eax
        xor edx, edx
        div ebx
        add al, '0'
        stosb
        mov eax, edx
        test eax, eax
        jnz .f
.x:
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; ESI=texto ECX=decimales -> EAX=el número (con esos decimales); CF=1 si no
; es un número. Vale la coma o el punto; lo que sobra de decimales se redondea
lee_dec:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        xor eax, eax
        xor edi, edi
        xor ebx, ebx
.sp:
        cmp byte [esi], ' '
        jne .s
        inc esi
        jmp .sp
.s:
        cmp byte [esi], '-'
        jne .il
        inc edi
        inc esi
.il:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .if
        cmp eax, 99999              ; (la parte entera: hasta 999.999)
        ja .err
        imul eax, 10
        add eax, edx
        inc esi
        inc ebx
        jmp .il
.if:
        cmp byte [esi], ','
        je .fr
        cmp byte [esi], '.'
        jne .esc
.fr:
        inc esi
.fl:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .esc
        inc ebx
        inc esi
        jecxz .extra
        imul eax, 10
        add eax, edx
        dec ecx
        jmp .fl
.extra:
        cmp edx, 5                  ; la primera que sobra redondea
        jb .sk
        inc eax
.sk:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .t
        inc esi
        jmp .sk
.esc:
        jecxz .t
.sc:
        imul eax, 10
        loop .sc
.t:
        cmp byte [esi], ' '
        jne .t1
        inc esi
        jmp .t
.t1:
        cmp byte [esi], 0
        jne .err
        test ebx, ebx
        jz .err
        test edi, edi
        jz .ok
        neg eax
.ok:
        clc
        jmp .x
.err:
        stc
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; EAX=aaaammdd -> «dd/mm/aaaa» en EDI, que avanza
fmt_fecha:
        push eax
        push ebx
        push ecx
        push edx
        xor edx, edx
        mov ecx, 100
        div ecx
        mov ebx, edx                ; día
        xor edx, edx
        div ecx                     ; EAX=año EDX=mes
        push eax
        push edx
        mov eax, ebx
        call .dos
        mov al, '/'
        stosb
        pop eax
        call .dos
        mov al, '/'
        stosb
        pop eax
        call hvl_num
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
.dos:
        cmp eax, 10
        jae .d1
        mov byte [edi], '0'
        inc edi
.d1:
        jmp hvl_num

; ESI=«d/m/aaaa» (o con guiones o puntos; el año, también con dos cifras)
; -> EAX=aaaammdd; CF=1 si no es una fecha
lee_fecha:
        push ebx
        push ecx
        push edx
        push esi
        call .num
        jc .err
        mov ebx, eax                ; día
        call .sep
        jc .err
        call .num
        jc .err
        mov ecx, eax                ; mes
        call .sep
        jc .err
        call .num
        jc .err
        cmp eax, 100                ; «26» es 2026
        jae .a
        add eax, 2000
.a:
        cmp eax, 1980
        jb .err
        cmp eax, 2099
        ja .err
        cmp ecx, 1
        jb .err
        cmp ecx, 12
        ja .err
        cmp ebx, 1
        jb .err
        movzx edx, byte [dias_mes+ecx-1]
        cmp ecx, 2                  ; febrero de un año bisiesto
        jne .d
        test al, 3
        jnz .d
        inc edx
.d:
        cmp ebx, edx
        ja .err
.f:
        cmp byte [esi], ' '
        jne .f1
        inc esi
        jmp .f
.f1:
        cmp byte [esi], 0
        jne .err
        imul eax, 100
        add eax, ecx
        imul eax, 100
        add eax, ebx
        clc
        jmp .x
.err:
        stc
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
.num:
        cmp byte [esi], ' '
        jne .n0
        inc esi
        jmp .num
.n0:
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .nerr
        call hvl_lee_num
        cmp eax, 9999
        ja .nerr
        clc
        ret
.nerr:
        stc
        ret
.sep:
        mov al, [esi]
        inc esi
        cmp al, '/'
        je .sok
        cmp al, '-'
        je .sok
        cmp al, '.'
        je .sok
        stc
        ret
.sok:
        clc
        ret
dias_mes    db 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31

; la fecha de hoy (INT 21h, 2Ah) -> [hoy]
hoy_leer:
        pushad
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [rl+RL_EAX], 0x2A00
        mov byte [rl+RL_INT], 0x21
        mov esi, rl
        HV rm_interrup
        movzx eax, word [rl+RL_ECX]
        imul eax, 100
        movzx ecx, byte [rl+RL_EDX+1]
        add eax, ecx
        imul eax, 100
        movzx ecx, byte [rl+RL_EDX]
        add eax, ecx
        mov [hoy], eax
        popad
        ret

; =============================================================================
;  El archivo de datos
; =============================================================================
guardar:
        pushad
        mov dword [cab], 'HFAC'
        mov dword [cab+4], 1
        mov eax, [n_fac]
        mov [cab+CAB_N], eax
        imul ecx, eax, F_TAM
        add ecx, CAB_TAM
        mov esi, ruta_dat
        mov edx, cab
        call hvl_escribir
        jnc .x
        mov esi, t_e_guardar
        call aviso
.x:
        popad
        ret

cargar:
        pushad
        mov esi, ruta_dat
        mov edi, cab
        mov ecx, CAB_TAM + MAX_FAC * F_TAM + 1
        call hvl_leer
        cmp eax, CAB_TAM
        jb .nada
        cmp dword [cab], 'HFAC'
        jne .nada
        mov ecx, [cab+CAB_N]
        cmp ecx, MAX_FAC
        ja .nada
        imul edx, ecx, F_TAM
        add edx, CAB_TAM
        cmp edx, eax
        ja .nada
        mov [n_fac], ecx
        ; que nada de lo leído se salga de sus límites
        mov byte [cab+E_NIF-1], 0
        mov byte [cab+E_DIR-1], 0
        mov byte [cab+E_DIR2-1], 0
        mov byte [cab+E_NOTA-1], 0
        mov byte [cab+CAB_TAM-1], 0
        mov ebx, facs
        jecxz .x
.f:
        and byte [ebx+F_TIPO], 1
        and byte [ebx+F_INCL], 1
        cmp byte [ebx+F_NLIN], MAX_LIN
        jbe .f1
        mov byte [ebx+F_NLIN], MAX_LIN
.f1:
        cmp dword [ebx+F_IVA], PCT_MAX
        jbe .f2
        mov dword [ebx+F_IVA], 0
.f2:
        cmp dword [ebx+F_RET], PCT_MAX
        jbe .f3
        mov dword [ebx+F_RET], 0
.f3:
        mov byte [ebx+F_CLI-1], 0
        mov byte [ebx+F_NIF-1], 0
        mov byte [ebx+F_DIR-1], 0
        mov byte [ebx+F_LIN-1], 0
        lea esi, [ebx+F_LIN]
        mov edx, MAX_LIN
.l:
        mov byte [esi+L_CANT-1], 0
        add esi, L_TAM
        dec edx
        jnz .l
        add ebx, F_TAM
        loop .f
        jmp .x
.nada:
        mov edi, cab
        mov ecx, CAB_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [n_fac], 0
.x:
        popad
        ret

; =============================================================================
;  Pintar
; =============================================================================
; texto en EAX=x EBX=y (menos el desplazamiento) ESI=texto
tx:
        pushad
        sub ebx, [desp]
        HV dc_texto
        popad
        ret
; lo mismo, acabando en EAX=x
txd:
        pushad
        call largo
        shl ecx, 3
        sub eax, ecx
        sub ebx, [desp]
        HV dc_texto
        popad
        ret
; rectángulo EAX, EBX, ECX, EDX (menos el desplazamiento)
rc:
        pushad
        sub ebx, [desp]
        HV dc_rect
        popad
        ret

%macro TX 3
        mov eax, %1
        mov ebx, %2
        mov esi, %3
        call tx
%endmacro
%macro TXD 3
        mov eax, %1
        mov ebx, %2
        mov esi, %3
        call txd
%endmacro

pintar:
        HV dc_pintar
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        cmp byte [modo], 0
        jne .fac
        mov dword [desp], 0
        call pinta_lista
        jmp .barra
.fac:
        call pinta_factura
.barra:
        mov dword [desp], 0
        call pinta_barra
        cmp dword [br_sobre], 0     ; con el ratón sobre un botón: abajo, lo
        je .cierra                  ; que hace (como en toda la familia)
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie
.cierra:
        HV dc_cerrar
        jmp hecho

; ---- la barra de botones ----
pinta_barra:
        pushad
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, bot_lista
        cmp byte [modo], 0
        je .b
        mov edi, bot_fac
.b:
        mov ebx, 4
        call br_fila
        popad
        ret
; lo que pregunta la barra común: EBP=1 en la pestaña que se ve
barra_estado:
        mov eax, [edi+12]
        sub eax, O_EMITIDAS
        cmp eax, 1
        ja .x
        cmp eax, [ver_tipo]
        jne .x
        inc ebp
.x:
        ret

; ---- la lista ----
pinta_lista:
        pushad
        call sumas
        ; cuántas filas caben
        mov eax, [cli_al]
        sub eax, LY0 + PIE_AL
        jns .v0
        xor eax, eax
.v0:
        xor edx, edx
        mov ecx, FIL_AL
        div ecx
        test eax, eax
        jnz .v1
        inc eax
.v1:
        mov [vis], eax
        call sel_a_la_vista
        mov eax, [n_ord]            ; (y que no quede hueco abajo)
        sub eax, [vis]
        jns .v2
        xor eax, eax
.v2:
        cmp [top], eax
        jbe .v3
        mov [top], eax
.v3:
        ; la cabecera
        mov eax, [cli_an]
        sub eax, 104
        mov [x_tot], eax
        add eax, 8
        mov [x_est], eax
        NEGRITA 1
        COLOR C_TINTA
        TX 8, TB_AL + 3, t_h_num
        TX 136, TB_AL + 3, t_h_fecha
        mov esi, t_h_cli
        cmp dword [ver_tipo], 0
        je .h
        mov esi, t_h_prov
.h:
        mov eax, 228
        call tx
        TXD [x_tot], TB_AL + 3, t_h_total
        TX [x_est], TB_AL + 3, t_h_estado
        NEGRITA 0
        COLOR C_ARENA_OSC
        xor eax, eax
        mov ebx, LY0 - 2
        mov ecx, [cli_an]
        mov edx, 1
        HV dc_rect
        ; las filas
        cmp dword [n_ord], 0
        jne .hay
        COLOR C_MARRON
        mov esi, t_vacia_e
        cmp dword [ver_tipo], 0
        je .va
        mov esi, t_vacia_r
.va:
        mov eax, 16
        mov ebx, LY0 + 12
        call tx
        jmp .pie
.hay:
        xor edi, edi                ; la fila
.f:
        cmp edi, [vis]
        jae .pie
        mov eax, edi
        add eax, [top]
        cmp eax, [n_ord]
        jae .pie
        mov ebp, eax
        mov eax, [fac_ord+eax*4]
        call ptr_fac
        imul edx, edi, FIL_AL
        add edx, LY0                ; EDX=y de la fila
        mov [fila_y], edx
        COLOR C_TINTA
        cmp ebp, [sel]
        jne .n
        COLOR C_PETROLEO
        push ebx
        xor eax, eax
        mov ebx, edx
        mov ecx, [cli_an]
        mov edx, FIL_AL
        HV dc_rect
        pop ebx
        COLOR C_CREMA
.n:
        inc dword [fila_y]          ; el texto, un punto más abajo
        lea esi, [ebx+F_NUM]
        mov eax, 8
        push ebx
        mov ebx, [fila_y]
        call tx
        pop ebx
        push edi
        mov edi, num_buf
        mov eax, [ebx+F_FECHA]
        call fmt_fecha
        mov byte [edi], 0
        pop edi
        push ebx
        TX 136, [fila_y], num_buf
        pop ebx
        ; el cliente, hasta donde quepa
        lea esi, [ebx+F_CLI]
        call largo
        mov eax, [x_tot]
        sub eax, 228 + 96
        sar eax, 3
        cmp ecx, eax
        jbe .cl
        mov ecx, eax
.cl:
        push ebx
        mov eax, 228
        mov ebx, [fila_y]
        cmp ecx, 0
        jle .cl2
        HV dc_texto_n
.cl2:
        pop ebx
        call totales
        push edi
        mov edi, num_buf
        mov eax, [t_total]
        call fmt_dinero
        mov byte [edi], 0
        pop edi
        push ebx
        mov ebx, [fila_y]
        mov eax, [x_tot]
        mov esi, num_buf
        call txd
        pop ebx
        mov esi, t_pendiente
        cmp byte [ebx+F_PAGADA], 0
        jne .pg
        cmp ebp, [sel]
        je .es
        COLOR C_GRANATE
        jmp .es
.pg:
        mov esi, t_cobrada
        cmp byte [ebx+F_TIPO], 0
        je .es
        mov esi, t_pagada
.es:
        mov eax, [x_est]
        mov ebx, [fila_y]
        call tx
        inc edi
        jmp .f
.pie:
        ; el resumen
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, PIE_AL
        mov ecx, [cli_an]
        mov edx, PIE_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        COLOR C_TINTA
        mov edi, linea
        mov eax, [n_ord]
        call hvl_num
        mov esi, t_s_fact
        cmp eax, 1
        jne .pl
        mov esi, t_s_fact1
.pl:
        call hvl_cad
        mov eax, [s_tab_tot]
        mov edx, [s_tab_tot+4]
        call fmt_dinero64
        mov esi, t_s_pend
        call hvl_cad
        mov eax, [s_tab_pend]
        mov edx, [s_tab_pend+4]
        call fmt_dinero64
        mov byte [edi], 0
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, PIE_AL - 5
        mov esi, linea
        call tx
        mov edi, linea
        mov esi, t_s_ano
        call hvl_cad
        mov eax, [hoy]
        xor edx, edx
        mov ecx, 10000
        div ecx
        call hvl_num
        mov esi, t_s_ing
        call hvl_cad
        mov eax, [s_ing]
        mov edx, [s_ing+4]
        call fmt_dinero64
        mov esi, t_s_gas
        call hvl_cad
        mov eax, [s_gas]
        mov edx, [s_gas+4]
        call fmt_dinero64
        mov esi, t_s_saldo
        call hvl_cad
        mov eax, [s_ing]
        mov edx, [s_ing+4]
        sub eax, [s_gas]
        sbb edx, [s_gas+4]
        call fmt_dinero64
        mov byte [edi], 0
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, PIE_AL - 23
        mov esi, linea
        call tx
        popad
        ret

; quién hace la factura EBX y para quién: [pa_*] arriba, [pb_*] el cliente
bloques:
        push eax
        push ecx
        lea eax, [ebx+F_CLI]
        mov [pb_nom], eax
        lea eax, [ebx+F_NIF]
        mov [pb_nif], eax
        lea eax, [ebx+F_DIR]
        mov [pb_dir], eax
        mov dword [pa_nom], cab + E_NOM
        mov dword [pa_nif], cab + E_NIF
        mov dword [pa_dir], cab + E_DIR
        mov dword [pa_dir2], cab + E_DIR2
        cmp byte [cab+E_NOM], 0
        jne .m
        mov dword [pa_nom], t_sin_mis
.m:
        cmp byte [ebx+F_TIPO], 0
        je .x
        mov eax, [pa_nom]           ; recibida: la hace el proveedor, para mí
        xchg eax, [pb_nom]
        mov [pa_nom], eax
        mov eax, [pa_nif]
        xchg eax, [pb_nif]
        mov [pa_nif], eax
        mov eax, [pa_dir]
        xchg eax, [pb_dir]
        mov [pa_dir], eax
        mov dword [pa_dir2], t_nada
.x:
        pop ecx
        pop eax
        ret

; el estado de la factura EBX -> ESI=texto, ZF=1 si está pendiente
estado_txt:
        mov esi, t_pend_cobro
        cmp byte [ebx+F_TIPO], 0
        je .a
        mov esi, t_pend_pago
.a:
        cmp byte [ebx+F_PAGADA], 0
        je .x
        mov esi, t_cobrada
        cmp byte [ebx+F_TIPO], 0
        je .b
        mov esi, t_pagada
.b:
        test esi, esi               ; ZF=0
.x:
        ret

; «IVA 21 %:» o «Retención 15 %:» en linea: ESI=el nombre EAX=el porcentaje
rotulo_pct:
        push ecx
        push edi
        mov edi, linea
        call hvl_cad
        mov ecx, 2
        call fmt_dec
        mov esi, t_p_pct
        call hvl_cad
        mov byte [edi], 0
        pop edi
        pop ecx
        ret

; ---- la factura, como en el papel ----
pinta_factura:
        pushad
        call fac_actual
        mov [pf], ebx
        call bloques
        call totales
        ; las medidas: dónde van los totales y el pie, y lo que ocupa todo
        movzx eax, byte [ebx+F_NLIN]
        test eax, eax
        jnz .m1
        inc eax
.m1:
        imul eax, FLIN_AL
        add eax, FLY0 + 10
        mov [y_tot], eax
        add eax, 3 * 18 + 10
        cmp dword [ebx+F_RET], 0
        je .m2
        add eax, 18
.m2:
        mov [y_pie], eax
        add eax, 26 + 8
        sub eax, [cli_al]
        jns .m3
        xor eax, eax
.m3:
        mov [fv_max], eax
        cmp [fv_y], eax
        jle .m4
        mov [fv_y], eax
.m4:
        mov eax, [fv_y]
        mov [desp], eax
        ; el papel
        COLOR C_BLANCO
        mov eax, 8
        mov ebx, TB_AL + 6
        mov ecx, [cli_an]
        sub ecx, 16
        mov edx, [y_pie]
        sub edx, TB_AL + 6 - 26
        call rc
        mov eax, [cli_an]
        sub eax, 272
        mov [x_der], eax
        ; quién la hace y los datos de la factura
        mov ebp, [pf]
        COLOR C_TINTA
        NEGRITA 1
        TX 24, 44, [pa_nom]
        mov esi, t_factura
        cmp byte [ebp+F_TIPO], 0
        je .t1
        mov esi, t_fac_rec
.t1:
        mov eax, [x_der]
        call tx
        NEGRITA 0
        TX 24, 62, [pa_nif]
        TX 24, 78, [pa_dir]
        TX 24, 94, [pa_dir2]
        mov edi, linea
        mov esi, t_p_num
        call hvl_cad
        lea esi, [ebp+F_NUM]
        call hvl_cad
        mov byte [edi], 0
        TX [x_der], 62, linea
        mov edi, linea
        mov esi, t_p_fecha
        call hvl_cad
        mov eax, [ebp+F_FECHA]
        call fmt_fecha
        mov byte [edi], 0
        TX [x_der], 78, linea
        TX [x_der], 94, t_p_estado
        mov ebx, ebp
        call estado_txt
        jnz .t2
        COLOR C_GRANATE
.t2:
        mov eax, [x_der]
        add eax, 64
        mov ebx, 94
        call tx
        COLOR C_TINTA
        ; el cliente
        NEGRITA 1
        TX 24, 116, t_p_cliente
        NEGRITA 0
        TX 104, 116, [pb_nom]
        TX 24, 132, t_p_nif
        TX 104, 132, [pb_nif]
        TX 104, 148, [pb_dir]
        ; la cabecera de las líneas
        COLOR C_ARENA
        mov eax, 16
        mov ebx, 168
        mov ecx, [cli_an]
        sub ecx, 32
        mov edx, 20
        call rc
        COLOR C_TINTA
        mov eax, [cli_an]
        sub eax, 24
        mov [x_imp], eax
        sub eax, 104
        mov [x_pre], eax
        sub eax, 104
        mov [x_cant], eax
        NEGRITA 1
        TX 24, 170, t_p_con
        TXD [x_cant], 170, t_p_cant
        TXD [x_pre], 170, t_p_pre
        TXD [x_imp], 170, t_p_imp
        NEGRITA 0
        ; las líneas
        cmp byte [ebp+F_NLIN], 0
        jne .hay
        COLOR C_MARRON
        TX 24, FLY0, t_sin_lin
        COLOR C_TINTA
        jmp .tot
.hay:
        xor edi, edi
        lea esi, [ebp+F_LIN]
.l:
        movzx eax, byte [ebp+F_NLIN]
        cmp edi, eax
        jae .tot
        imul edx, edi, FLIN_AL
        add edx, FLY0
        mov [fila_y], edx
        cmp edi, [lsel]
        jne .l1
        COLOR C_ARENA
        mov eax, 16
        mov ebx, edx
        mov ecx, [cli_an]
        sub ecx, 32
        mov edx, FLIN_AL
        call rc
        COLOR C_TINTA
.l1:
        mov eax, 24
        mov ebx, [fila_y]
        call tx                     ; el concepto (ESI=la línea: empieza por él)
        push edi
        mov edi, num_buf
        mov eax, [esi+L_CANT]
        mov ecx, 3
        call fmt_dec
        mov byte [edi], 0
        mov edi, num_buf + 16
        mov eax, [esi+L_PRE]
        call fmt_dinero
        mov byte [edi], 0
        mov edi, num_buf + 32
        call importe_linea
        call fmt_dinero
        mov byte [edi], 0
        pop edi
        push esi
        TXD [x_cant], [fila_y], num_buf
        TXD [x_pre], [fila_y], num_buf + 16
        TXD [x_imp], [fila_y], num_buf + 32
        pop esi
        add esi, L_TAM
        inc edi
        jmp .l
.tot:
        cmp byte [ebp+F_INCL], 0    ; (a la izquierda de los totales)
        je .tot1
        COLOR C_MARRON
        TX 24, [y_tot], t_incl
        COLOR C_TINTA
.tot1:
        ; los totales
        mov eax, 16
        mov ebx, [y_tot]
        sub ebx, 5
        mov ecx, [cli_an]
        sub ecx, 32
        mov edx, 1
        call rc
        mov edx, [y_tot]
        mov esi, t_p_base
        mov eax, [t_base]
        call .total
        mov esi, t_p_iva
        mov eax, [ebp+F_IVA]
        call rotulo_pct
        mov esi, linea
        mov eax, [t_iva]
        call .total
        cmp dword [ebp+F_RET], 0
        je .sr
        mov esi, t_p_ret
        mov eax, [ebp+F_RET]
        call rotulo_pct
        mov esi, linea
        mov eax, [t_ret]
        neg eax
        call .total
.sr:
        NEGRITA 1
        mov esi, t_p_total
        mov eax, [t_total]
        call .total
        NEGRITA 0
        ; el pie (solo en las que hace uno)
        cmp byte [ebp+F_TIPO], 0
        jne .x
        TX 24, [y_pie], cab + E_NOTA
.x:
        popad
        ret
; (interna) una fila de los totales: ESI=rótulo EAX=céntimos EDX=y (avanza)
.total:
        push eax
        mov eax, [x_pre]
        mov ebx, edx
        call txd
        pop eax
        mov edi, num_buf
        call fmt_dinero
        mov byte [edi], 0
        mov eax, [x_imp]
        mov esi, num_buf
        call txd
        add edx, 18
        ret

; =============================================================================
;  El resumen del año: bases e IVA de cada trimestre
; =============================================================================
resumen:
        pushad
        mov edi, q_sumas
        mov ecx, 5 * 4 * 2
        xor eax, eax
        rep stosd
        mov eax, [hoy]
        xor edx, edx
        mov ecx, 10000
        div ecx
        mov ebp, eax                ; el año
        xor ecx, ecx
.f:
        cmp ecx, [n_fac]
        jae .txt
        mov eax, ecx
        call ptr_fac
        push ecx
        mov eax, [ebx+F_FECHA]
        xor edx, edx
        mov ecx, 100
        div ecx
        xor edx, edx
        div ecx                     ; EAX=año EDX=mes
        pop ecx
        cmp eax, ebp
        jne .s
        lea eax, [edx-1]            ; el trimestre, de 0 a 3
        xor edx, edx
        mov esi, 3
        div esi
        call totales
        mov esi, eax
        call .apunta
        mov esi, 4                  ; y el año entero
        call .apunta
.s:
        inc ecx
        jmp .f
; (interna) apunta la factura EBX en la fila ESI: base e IVA, en su columna
.apunta:
        shl esi, 5                  ; 32 octetos por fila: ingresos, gastos,
        lea edi, [q_sumas+esi]      ; IVA repercutido e IVA soportado
        cmp byte [ebx+F_TIPO], 0
        je .a1
        add edi, 8
.a1:
        mov eax, [t_base]
        call suma64
        add edi, 16
        mov eax, [t_iva]
        call suma64
        ret
.txt:
        mov edi, res_buf
        mov esi, t_s_ano
        call hvl_cad
        mov eax, ebp
        call hvl_num
        mov al, 10
        stosb
        mov esi, t_r_cab
        call hvl_cad
        xor ebx, ebx                ; la fila
.r:
        cmp ebx, 4
        je .ra
        lea eax, [ebx+'1']
        stosb
        mov esi, t_r_trim
        jmp .rb
.ra:
        mov al, 10
        stosb
        mov esi, t_r_ano
.rb:
        call hvl_cad
        mov esi, ebx
        shl esi, 5
        add esi, q_sumas
        mov ecx, 12
        mov eax, [esi]
        mov edx, [esi+4]
        call fmt_din_ancho
        mov eax, [esi+8]
        mov edx, [esi+12]
        call fmt_din_ancho
        mov ecx, 14
        mov eax, [esi+16]
        mov edx, [esi+20]
        call fmt_din_ancho
        mov eax, [esi+24]
        mov edx, [esi+28]
        call fmt_din_ancho
        mov ecx, 13
        mov eax, [esi+16]
        mov edx, [esi+20]
        sub eax, [esi+24]
        sbb edx, [esi+28]
        call fmt_din_ancho
        mov al, 10
        stosb
        inc ebx
        cmp ebx, 5
        jb .r
        mov esi, t_r_nota
        call hvl_cad
        mov byte [edi], 0
        mov dword [campos], cam_nada
        mov esi, dlg_res
        call dialogo
        popad
        ret

; =============================================================================
;  La factura en texto: para guardarla y para imprimirla
; =============================================================================
ANCHO_TXT   equ 72
; la línea de texto, en blanco
ln_ini:
        push eax
        push ecx
        push edi
        mov edi, linea
        mov ecx, ANCHO_TXT + 8
        mov al, ' '
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret
; pone ESI en la columna EAX de la línea
ln_pon:
        pushad
        lea edi, [linea+eax]
.c:
        lodsb
        test al, al
        jz .x
        cmp edi, linea + ANCHO_TXT + 8
        jae .x
        stosb
        jmp .c
.x:
        popad
        ret
; pone ESI acabando en la columna EAX
ln_pon_der:
        push eax
        push ecx
        call largo
        sub eax, ecx
        jns .a
        xor eax, eax
.a:
        call ln_pon
        pop ecx
        pop eax
        ret
; la línea, al texto (sin los espacios del final, con CR y LF)
ln_fin:
        pushad
        mov ecx, ANCHO_TXT + 8
.q:
        cmp byte [linea+ecx-1], ' '
        jne .c
        dec ecx
        jnz .q
.c:
        mov esi, linea
        mov edi, [txt_p]
        rep movsb
        mov ax, 0x0A0D
        stosw
        mov [txt_p], edi
        popad
        jmp ln_ini

%macro LN 2
        mov eax, %1
        mov esi, %2
        call ln_pon
%endmacro

; hace el texto de la factura abierta en txt_buf -> ECX=octetos
texto_haz:
        push eax
        push ebx
        push edx
        push esi
        push edi
        push ebp
        call fac_actual
        mov ebp, ebx
        call bloques
        call totales
        mov dword [txt_p], txt_buf
        call ln_ini
        LN 0, [pa_nom]
        mov esi, t_factura
        cmp byte [ebp+F_TIPO], 0
        je .t1
        mov esi, t_fac_rec
.t1:
        mov eax, 46
        call ln_pon
        call ln_fin
        LN 0, [pa_nif]
        LN 46, t_p_num
        lea esi, [ebp+F_NUM]
        mov eax, 54
        call ln_pon
        call ln_fin
        LN 0, [pa_dir]
        LN 46, t_p_fecha
        mov edi, num_buf
        mov eax, [ebp+F_FECHA]
        call fmt_fecha
        mov byte [edi], 0
        LN 54, num_buf
        call ln_fin
        LN 0, [pa_dir2]
        call ln_fin
        call ln_fin
        LN 0, t_p_cliente
        LN 9, [pb_nom]
        call ln_fin
        LN 0, t_p_nif
        LN 9, [pb_nif]
        call ln_fin
        LN 9, [pb_dir]
        call ln_fin
        call ln_fin
        LN 0, t_p_con
        mov eax, 46
        mov esi, t_p_cant
        call ln_pon_der
        mov eax, 59
        mov esi, t_p_pre
        call ln_pon_der
        mov eax, ANCHO_TXT
        mov esi, t_p_imp
        call ln_pon_der
        call ln_fin
        LN 0, t_raya
        call ln_fin
        movzx ecx, byte [ebp+F_NLIN]
        lea esi, [ebp+F_LIN]
        jecxz .tot
.l:
        xor eax, eax
        call ln_pon                 ; el concepto
        mov edi, num_buf
        mov eax, [esi+L_CANT]
        push ecx
        mov ecx, 3
        call fmt_dec
        pop ecx
        mov byte [edi], 0
        mov edi, num_buf + 16
        mov eax, [esi+L_PRE]
        call fmt_dinero
        mov byte [edi], 0
        mov edi, num_buf + 32
        call importe_linea
        call fmt_dinero
        mov byte [edi], 0
        push esi
        mov eax, 46
        mov esi, num_buf
        call ln_pon_der
        mov eax, 59
        mov esi, num_buf + 16
        call ln_pon_der
        mov eax, ANCHO_TXT
        mov esi, num_buf + 32
        call ln_pon_der
        pop esi
        call ln_fin
        add esi, L_TAM
        loop .l
.tot:
        LN 0, t_raya
        call ln_fin
        cmp byte [ebp+F_INCL], 0
        je .tot1
        LN 0, t_incl
.tot1:
        mov esi, t_p_base
        mov eax, [t_base]
        call .total
        mov esi, t_p_iva
        mov eax, [ebp+F_IVA]
        call .rotulo
        mov eax, [t_iva]
        call .total
        cmp dword [ebp+F_RET], 0
        je .sr
        mov esi, t_p_ret
        mov eax, [ebp+F_RET]
        call .rotulo
        mov eax, [t_ret]
        neg eax
        call .total
.sr:
        mov esi, t_p_total
        mov eax, [t_total]
        call .total
        cmp byte [ebp+F_TIPO], 0
        jne .fin
        cmp byte [cab+E_NOTA], 0
        je .fin
        call ln_fin
        LN 0, cab + E_NOTA
        call ln_fin
.fin:
        mov ecx, [txt_p]
        sub ecx, txt_buf
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ebx
        pop eax
        ret
; (interna) una fila de totales: ESI=rótulo EAX=céntimos
.total:
        push eax
        mov eax, 59
        call ln_pon_der
        pop eax
        mov edi, num_buf
        call fmt_dinero
        mov byte [edi], 0
        mov eax, ANCHO_TXT
        mov esi, num_buf
        call ln_pon_der
        jmp ln_fin
; (interna) el rótulo con porcentaje, en rot_buf -> ESI
.rotulo:
        mov edi, rot_buf
        call hvl_cad
        mov ecx, 2
        call fmt_dec
        mov esi, t_p_pct
        call hvl_cad
        mov byte [edi], 0
        mov esi, rot_buf
        ret

; ---- Guardar como texto ----
a_texto:
        call fac_elegida
        jc .x
        ; el nombre que se propone: el número de la factura (sus letras y
        ; cifras, hasta ocho) y .TXT
        mov edi, nom_tmp
        lea esi, [ebx+F_NUM]
        xor ecx, ecx
.n:
        lodsb
        test al, al
        jz .n2
        cmp al, '0'
        jb .n
        cmp al, '9'
        jbe .n1
        and al, 0xDF
        cmp al, 'A'
        jb .n
        cmp al, 'Z'
        ja .n
.n1:
        stosb
        inc ecx
        cmp ecx, 8
        jb .n
.n2:
        test ecx, ecx
        jnz .n3
        mov esi, t_fac_nom
        call hvl_cad
.n3:
        mov esi, t_ext_txt
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, t_ext_txt          ; sin extensión, .TXT
        call pon_ext
        call texto_haz
        mov esi, nom_tmp
        mov edx, txt_buf
        call hvl_escribir
        mov esi, t_guardada
        jnc .d
        mov esi, t_e_texto
.d:
        call aviso
.x:
        ret

; ---- Exportar la lista que se ve (emitidas o recibidas) a un .CSV, para
;      abrirla con Cuentas: una fila por factura, con «;» entre celdas y los
;      importes con coma y sin puntos de millares, para que sean números ----
a_csv:
        cmp dword [n_ord], 0
        jne .hay
        mov esi, t_e_csv_nada
        jmp aviso
.hay:
        mov edi, nom_tmp
        mov esi, t_csv_emi
        cmp dword [ver_tipo], 0
        je .n
        mov esi, t_csv_rec
.n:
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_csv_tit
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, t_ext_csv
        call pon_ext
        mov edi, csv_buf
        mov esi, t_csv_cab
        call hvl_cad
        xor ebp, ebp                ; EBP=la fila
.f:
        mov eax, [fac_ord+ebp*4]
        call ptr_fac
        call totales
        lea esi, [ebx+F_NUM]
        call csv_txt
        mov eax, [ebx+F_FECHA]
        call fmt_fecha
        mov al, ';'
        stosb
        lea esi, [ebx+F_CLI]
        call csv_txt
        lea esi, [ebx+F_NIF]
        call csv_txt
        mov eax, [t_base]
        call csv_dinero
        mov eax, [t_iva]
        call csv_dinero
        mov eax, [t_ret]
        call csv_dinero
        mov eax, [t_total]
        call csv_dinero
        mov esi, t_csv_si
        cmp byte [ebx+F_PAGADA], 0
        jne .p
        mov esi, t_csv_no
.p:
        call hvl_cad
        mov ax, 0x0A0D
        stosw
        inc ebp
        cmp ebp, [n_ord]
        jb .f
        mov ecx, edi
        sub ecx, csv_buf
        mov esi, nom_tmp
        mov edx, csv_buf
        call hvl_escribir
        mov esi, t_csv_hecho
        jnc .d
        mov esi, t_e_texto
.d:
        call aviso
.x:
        ret
; (internas) una celda de texto (ESI) y su «;»: los «;» y las comillas del
; texto se cambian por una coma y un apóstrofo, y así no hacen falta comillas
csv_txt:
        lodsb
        test al, al
        jz .f
        cmp al, ';'
        jne .c1
        mov al, ','
.c1:
        cmp al, '"'
        jne .c2
        mov al, "'"
.c2:
        stosb
        jmp csv_txt
.f:
        mov al, ';'
        stosb
        ret
; EAX=céntimos (con signo) -> «-1234,56;» en EDI
csv_dinero:
        push eax
        push ebx
        push edx
        test eax, eax
        jns .p
        neg eax
        mov byte [edi], '-'
        inc edi
.p:
        xor edx, edx
        mov ebx, 100
        div ebx
        push edx
        call hvl_num
        mov al, ','
        stosb
        pop eax
        xor edx, edx
        mov ebx, 10
        div ebx
        add al, '0'
        stosb
        mov al, dl
        add al, '0'
        stosb
        mov al, ';'
        stosb
        pop edx
        pop ebx
        pop eax
        ret
; pone la extensión ESI a nom_tmp si no lleva ninguna
pon_ext:
        push eax
        push edx
        push edi
        push esi
        mov esi, nom_tmp
        xor edx, edx
.e:
        lodsb
        test al, al
        jz .e2
        cmp al, '.'
        jne .e1
        inc edx
.e1:
        cmp al, '\'
        jne .e
        xor edx, edx
        jmp .e
.e2:
        lea edi, [esi-1]
        pop esi
        test edx, edx
        jnz .x
        call hvl_cad
        mov byte [edi], 0
.x:
        pop edi
        pop edx
        pop eax
        ret

; ---- Imprimir: el texto y un salto de página, a la impresora del puerto
; paralelo (LPT1) con la BIOS, INT 17h. (El PRN de Hispa-DOS todavía no
; imprime: lo que se le escribe se pierde sin dar error.) ----
imprimir:
        call fac_elegida
        jc .x
        call texto_haz
        mov byte [txt_buf+ecx], 12
        inc ecx
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h: los dos
        mov bl, 0x11                ; bits de arriba, cuántos)
        call .bios
        test ah, 0xC0
        jz .mal
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call .bios
        test ah, 0x28               ; (error o sin papel)
        jnz .mal
        test ah, 0x10               ; (en línea)
        jz .mal
        mov esi, txt_buf
.c:
        call .lista                 ; esperar a que admita otro carácter
        jc .mal
        movzx eax, byte [esi]       ; AH=0: imprimir el carácter AL
        mov bl, 0x17
        call .bios
        test ah, 0x28
        jnz .mal
        inc esi
        loop .c
        call .lista                 ; (y a que acabe con el último)
        jc .mal
        mov esi, t_impresa
        jmp aviso
; (interna) espera a que la impresora no esté ocupada, hasta veinte segundos
; (una impresora de verdad se para a ratos, con la memoria llena); CF=1 si no
; contesta o da error
.lista:
        push eax
        push ebx
        push edx
        HV tics
        mov edx, eax
.l:
        mov eax, 0x0200
        mov bl, 0x17
        call .bios
        test ah, 0x28
        jnz .lno
        test ah, 0x80               ; no está ocupada
        jnz .lsi
        HV tics
        sub eax, edx
        cmp eax, 20 * 18
        jb .l
.lno:
        stc
        jmp .lx
.lsi:
        clc
.lx:
        pop edx
        pop ebx
        pop eax
        ret
.mal:
        mov esi, t_e_imprimir
        call aviso
.x:
        ret
; (interna) la interrupción BL de la BIOS con EAX y DX=0 (LPT1) -> EAX
.bios:
        push ecx
        push esi
        push edi
        push eax
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov [rl+RL_EAX], eax
        mov [rl+RL_INT], bl
        mov esi, rl
        HV rm_interrup
        mov eax, [rl+RL_EAX]
        pop edi
        pop esi
        pop ecx
        ret

%include "barra.inc"
%include "hvplib.inc"

        section .bss
            BARRA_BSS
ven         resd 1
modo        resb 1                  ; 0: la lista; 1: una factura
lin_nueva   resb 1
c_pag       resb 1
c_incl      resb 1
            alignb 4
ver_tipo    resd 1                  ; la pestaña: 0 emitidas, 1 recibidas
sel         resd 1                  ; la fila elegida de la lista
top         resd 1                  ; la primera fila que se ve
vis         resd 1                  ; cuántas filas caben
act         resd 1                  ; la factura abierta
lsel        resd 1                  ; su línea elegida
fv_y        resd 1                  ; lo que está desplazada la factura
fv_max      resd 1
desp        resd 1
hoy         resd 1                  ; aaaammdd
n_fac       resd 1
n_ord       resd 1
cli_an      resd 1
cli_al      resd 1
x_tot       resd 1
x_est       resd 1
x_der       resd 1
x_cant      resd 1
x_pre       resd 1
x_imp       resd 1
y_tot       resd 1
y_pie       resd 1
fila_y      resd 1
pf          resd 1
pa_nom      resd 1
pa_nif      resd 1
pa_dir      resd 1
pa_dir2     resd 1
pb_nom      resd 1
pb_nif      resd 1
pb_dir      resd 1
t_base      resd 1
t_iva       resd 1
t_ret       resd 1
t_total     resd 1
campos      resd 1
txt_p       resd 1
s_tab_tot   resd 2                  ; (las cuatro sumas, seguidas)
s_tab_pend  resd 2
s_ing       resd 2
s_gas       resd 2
q_sumas     resd 5 * 4 * 2          ; 4 trimestres y el año x 4 columnas de 64 bits
rl          resb RL_TAM
fac_ord     resd MAX_FAC
c_num       resb 16
c_fecha     resb 16
c_cli       resb 40
c_nif       resb 16
c_dir       resb 56
c_iva       resb 16
c_ret       resb 16
c_con       resb 36
c_cant      resb 16
c_pre       resb 16
num_buf     resb 64
num_tmp     resb 32
rot_buf     resb 40
linea       resb 160
nom_tmp     resb 96
ruta_dat    resb 96
res_buf     resb 800
            alignb 4
tmp_emi     resb CAB_TAM - E_NOM
tmp_lin     resb L_TAM
tmp_fac     resb F_TAM
txt_buf     resb 4096
ec_dlg      resd 1                  ; elegir cliente: el diálogo de la factura
ec_n        resd 1
ec_sel      resd 1
ec_i        resd 1
ec_tab      resd MAX_FAC
csv_buf     resb MAX_FAC * 160 + 128
            alignb 4
cab         resb CAB_TAM            ; el archivo: la cabecera y, detrás, las
facs        resb MAX_FAC * F_TAM    ; facturas
            resb 4
HVP_FIN
