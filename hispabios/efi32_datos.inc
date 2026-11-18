; =============================================================================
;  HispaBIOS (IA32) - los textos y las variables (parte de efi32.asm)
; =============================================================================
t_titulo        db 'HispaBIOS ', HB_VERSION, ' (IA32)', 13, 10, 0
t_r_fab         db '[registro de HispaBIOS ', HB_VERSION, ' IA32]', 13, 10, 'Firmware: ', 0
t_r_rev         db ', revision ', 0
t_r_uefi        db 'h, UEFI ', 0
t_punto         db '.', 0
t_r_conout      db ', ConOut ', 0
t_p_mapa        db '[paso] GetMemoryMap', 13, 10, 0
t_mapa_mal      db 'No se ha podido leer el mapa de memoria del firmware. Codigo: ', 0
t_desc_mal      db 'El mapa de memoria del firmware no se entiende (entradas de ', 0
t_r_mapa        db 'Mapa de memoria: ', 0
t_r_mapa2       db ' entradas de ', 0
t_r_mapa3       db ' octetos (inicio, paginas, tipo, atributos)', 13, 10, 0
t_mapa          db 'Memoria por debajo de 1 MB (inicio, paginas, tipo):', 13, 10, 0
t_sangria       db '  ', 0
t_paginas       db ' pag. ', 0
t_tipo          db ' tipo ', 0
t_espacio       db ' ', 0
t_atr           db ' atr. ', 0
t_nl            db 13, 10, 0
t_mas           db '  (y ', 0
t_mas2          db ' tramos mas: en el registro)', 13, 10, 0
t_total         db 'Memoria del equipo: ', 0
t_mb            db ' MB', 13, 10, 0
; los tipos de memoria de UEFI (12 octetos cada nombre)
t_tipos         db 'reservada', 0, 0, 0
                db 'cargador c.', 0
                db 'cargador d.', 0
                db 'firmware c.', 0
                db 'firmware d.', 0
                db 'servicios c', 0
                db 'servicios d', 0
                db 'libre', 0, 0, 0, 0, 0, 0, 0
                db 'averiada', 0, 0, 0, 0
                db 'ACPI', 0, 0, 0, 0, 0, 0, 0, 0
                db 'ACPI NVS', 0, 0, 0, 0
                db 'E/S', 0, 0, 0, 0, 0, 0, 0, 0, 0
                db 'E/S puertos', 0
                db 'PAL', 0, 0, 0, 0, 0, 0, 0, 0, 0
                db 'persistente', 0
                db 'sin aceptar', 0
t_tipo_otro     db 'del fabricante', 0
t_pide          db 'Pidiendo al firmware la memoria baja (0 a 9FFFF)...', 13, 10, 0
t_no_dada       db '  no da de ', 0
t_a             db ' a ', 0
t_parentesis    db ' (', 0
t_pag_fin       db ' pag.)', 13, 10, 0
t_dadas         db 'Paginas dadas: ', 0
t_de160         db ' de 160', 13, 10, 0
t_base          db 'La BIOS de modo real va en ', 0
t_base2         db 'h. Memoria convencional para DOS: ', 0
t_kb            db ' KB', 13, 10, 0
t_sin_arranque  db 13, 10, 'El firmware usa la pagina 80000h (la del sector de arranque de Hispa-DOS).', 0
t_sin_base      db 13, 10
                db 'ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA QUE NECESITA HISPA-DOS', 13, 10
                db '(60 KB seguidos entre 384 y 636 KB): la usa su firmware.', 13, 10
                db 'Que hacer: entre en la configuracion del equipo (BIOS/UEFI) y, si tiene', 13, 10
                db '"CSM", "Legacy" o "arranque heredado", activelo y arranque la memoria', 13, 10
                db 'USB en ese modo (sin "UEFI:" delante en el menu de arranque).', 13, 10, 0
t_poca          db 13, 10
                db 'ESTE EQUIPO DEJA MUY POCA MEMORIA CONVENCIONAL (menos de 256 KB):', 13, 10
                db 'su firmware usa la memoria baja y Hispa-DOS no cabe.', 13, 10
                db 'Que hacer: entre en la configuracion del equipo (BIOS/UEFI) y, si tiene', 13, 10
                db '"CSM", "Legacy" o "arranque heredado", activelo y arranque la memoria', 13, 10
                db 'USB en ese modo (sin "UEFI:" delante en el menu de arranque).', 13, 10, 0
t_gop           db 'Pantalla (GOP): ', 0
t_por           db 'x', 0
t_gop_f         db ', formato ', 0
t_gop_l         db ', puntos por linea ', 0
t_gop_d         db ', en ', 0
t_r_ext         db 'Memoria extendida: ', 0
t_presta        db 'Paginas del firmware en la memoria baja que se prestan a DOS (se cambian en cada viaje): ', 0
t_t_acpi        db 'Tablas del equipo: ACPI (RSDP) en ', 0
t_t_rev         db ' (revision ', 0
t_t_xsdt        db '; OJO: sin RSDT de 32 bits, solo XSDT', 0
t_t_smbios      db '; SMBIOS en ', 0
t_parentesis2   db ')', 0
t_t_no          db 'no hay', 0
t_s_equipo      db 'Equipo: ', 0
t_s_placa       db '; placa ', 0
t_s_bios        db '; BIOS ', 0
t_s_nada        db '-', 0
t_seguro_s      db '(Si no arranca o va mal: reinicie y vaya pulsando la tecla S = modo seguro.', 13, 10
                db ' Si la imagen sale girada: vaya pulsando la tecla G al arrancar.)', 13, 10, 0
t_seguro        db 'MODO SEGURO: sin escritura combinada, sin cambiar de modo de pantalla y sin prestar paginas', 13, 10, 0
; el giro de la pantalla
t_gi_vert       db 'Pantalla vertical (mas alta que ancha). ', 0
t_gi            db 'Giro de la imagen: ', 0
t_gi_0          db 'ninguno', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
t_gi_1          db '90 grados a la derecha', 0, 0, 0, 0, 0, 0
                db '180 grados', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                db '90 grados a la izquierda', 0, 0, 0, 0
t_gi_log        db '; DOS ve una pantalla de ', 0
t_gi_fich       db ' (giro guardado en HBGIRO.CFG)', 0
t_gi_tecla      db ' (tecla G: un cuarto de vuelta mas)', 0
t_gi_fin        db '. G al arrancar o Ctrl+Alt+G lo cambian.', 13, 10, 0
t_gi_sin_mem    db 'Giro: no hay memoria para la pantalla girada: sin giro', 13, 10, 0
t_g_modos       db 'Modos de pantalla del firmware (GOP; * el puesto): ', 0
t_g_cambio      db 'Pantalla lenta: se pasa a un modo mas pequeno del firmware: ', 0
t_g_coma        db ', ', 0
t_g_ast         db '*', 0
t_g_mas         db '...', 0
t_w_mtrr        db 'Cache de la pantalla: MTRR por omision ', 0
t_w_mtrr2       db 'h; la pantalla, ', 0
t_w_var         db 'en el MTRR variable ', 0
t_w_tipo        db ' de tipo ', 0
t_w_nomtrr      db 'en ninguno (vale el tipo por omision)', 0
t_w_tipos       db 'UC', 0, 0, 'WC', 0, 0, '??', 0, 0, '??', 0, 0, 'WT', 0, 0, 'WP', 0, 0, 'WB', 0, 0, '??', 0, 0
t_w_pat         db '; PAT ', 0
t_w_fw          db 'Paginacion del firmware: CR0 ', 0
t_w_fw3         db ', CR3 ', 0
t_w_fw4         db ', CR4 ', 0
t_w_fw_sin      db ' (sin paginacion)', 13, 10, 0
t_w_fw_con      db ' (con paginacion)', 13, 10, 0
%ifdef PRUEBA_SIN_PAG
t_w_sinpag      db '(prueba) las tablas propias se ponen y se quitan como en un firmware sin paginacion', 13, 10, 0
%endif
t_w_tablas      db 'Escritura combinada (WC): tablas de paginas propias (PAE, de 2 MB) en ', 0
t_w_tablas2     db '; la pantalla, entradas ', 0
t_w_tablas3     db '; PAT nuevo ', 0
t_w_no_pat      db 'Escritura combinada (WC): no se pone (el procesador no tiene PAT)', 13, 10, 0
t_w_no_pae      db 'Escritura combinada (WC): no se pone (el procesador no tiene PAE)', 13, 10, 0
t_w_no_mem      db 'Escritura combinada (WC): no se pone (sin memoria para las tablas)', 13, 10, 0
t_w_medida      db 'Pintar la pantalla entera: ', 0
t_w_medida2     db ' us escribiendo directamente, ', 0
t_w_medida3     db ' us con las tablas propias (WC). Se pinta ', 0
t_w_directo     db 'directamente', 0
t_w_portablas   db 'con las tablas propias', 0
t_w_esc         db 'Texto: ', 0
t_w_esc_todo    db 'ampliado a toda la pantalla (', 0
t_w_esc_k       db 'la pantalla es lenta: ampliado solo por ', 0
t_w_esc_k2      db ' (', 0
t_w_cierra      db ')', 13, 10, 0
t_v_lfb         db 'VBE: marco lineal para los programas en ', 0
t_v_lfb2        db ' (3 MB, por debajo de 4 GB); modos de 32 bits: 640x480, 800x600 y 1024x768 (los que quepan)', 13, 10, 0
t_v_no          db 'VBE: no hay memoria para el marco lineal: sin modos graficos', 13, 10, 0
t_r_tsc         db ' KB; TSC por tic: ', 0
t_r_vec         db '; vector de la IRQ 0 (8259): ', 0
t_r_discos      db 'h; discos: ', 0
; los discos (discos_buscar)
t_dc            db 'Controladora de disco ', 0
t_dc2           db ':', 0
t_dc3           db ' (clase ', 0
t_dc4           db 'h): se pide al firmware que la ponga en marcha. Resultado ', 0
t_d_tit         db 'Discos del firmware (Block I/O, enteros y con medio):', 13, 10, 0
t_d_disco       db '  ', 0
t_d_sec         db 'h sectores de ', 0
t_d_oct         db ' octetos', 0
t_d_mb          db ' MB)', 0
t_d_fijo        db ', fijo', 0
t_d_extra       db ', extraible', 0
t_d_sololee     db ', solo lectura', 0
t_d_arr         db ', EL DE ARRANQUE', 0
t_d_no512       db ': no se da a DOS (hacen falta sectores de 512)', 13, 10, 0
t_d_chico       db ': no se da a DOS (fijo y de menos de 32 MB: una particion de arranque de eMMC)', 13, 10, 0
t_d_sobra       db ': no cabe (8 discos como mucho)', 13, 10, 0
t_d_es          db ': disco ', 0
t_d_h           db 'h de DOS', 13, 10, 0
t_sin_discos    db 13, 10, 'NO SE ENCUENTRA EL DISCO DE ARRANQUE (la memoria USB) entre los discos', 13, 10
                db 'del firmware (hacen falta sectores de 512 octetos).', 13, 10, 0
t_viaje         db 'Arrancando la BIOS de modo real...', 13, 10, 0
; el reloj (reloj_mirar)
t_k_seguro      db 'Reloj: modo seguro, no se toca el PCH, el HPET ni el IOAPIC; la BIOS solo prueba el 8254 y el reloj CMOS', 13, 10, 0
t_k_pit         db 'Reloj 8254 (PIT), canal 0: ', 0
t_k_pit2        db 'Reloj 8254 (PIT) despues de quitar el bloqueo: ', 0
t_k_si          db 'CUENTA (lecturas ', 0
t_k_no          db 'NO CUENTA: esta parado (lecturas ', 0
t_k_lento       db 'NO CUENTA el tiempo: se mueve, pero muy despacio (bloqueado; lecturas ', 0
t_k_y           db 'h y ', 0
t_k_fin         db 'h con 2 ms entre medias)', 13, 10, 0
t_i_lpc         db 'Bloqueo del 8254 (clock gating) del PCH: el puente 00:1F.0 es ', 0
t_i_no_intel    db ', que no es de Intel: no se sabe quitar', 0
t_i_p2sb        db '; P2SB (00:1F.1): identificador ', 0
t_i_e0          db ', registro E0h ', 0
t_i_mostrado    db '; al mostrarlo, ', 0
t_i_no_hay      db ': no aparece (este puente no lo tiene)', 0
t_i_raro        db ': no es el P2SB de un PCH', 0
t_i_bar         db '; SBREG_BAR ', 0
t_i_sin_bar     db ': sin registros que valgan', 0
t_i_prc         db '; ITSSPRC (puerto C4h, 3300h) = ', 0
t_i_ya          db ': el bit 2 (8254CGE) ya esta a 0, el bloqueo no es este', 0
t_i_quitado     db ' con el bit 2 (8254CGE) puesto: se quita y queda ', 0
t_k_sin_hpet    db 'HPET: la tabla de ACPI no esta (no hay HPET)', 13, 10, 0
t_k_hpet        db 'HPET: en ', 0
t_k_hpet_id     db ', identificador ', 0
t_k_hpet_cap    db '; capacidades ', 0
t_k_hpet_cfg    db ', configuracion ', 0
t_k_hpet_t0     db ', temporizador 0 ', 0
t_k_alto        db ' (por encima de 4 GB: no se usa)', 0
t_k_nomap       db ' (no esta en las tablas de paginas del firmware: no se mira)', 0
t_k_lapic       db 'APIC local: IA32_APIC_BASE ', 0
t_k_x2          db ' (x2APIC)', 0
t_k_svr         db '; SVR ', 0
t_k_lint0       db ', LINT0 ', 0
t_k_lint1       db ', LINT1 ', 0
t_k_lvtt        db ', temporizador ', 0
t_k_sin_madt    db 'Tabla APIC de ACPI: no esta', 13, 10, 0
t_k_madt        db 'Tabla APIC de ACPI: banderas ', 0
t_k_ioapic      db '; IOAPIC en ', 0
t_k_gsi         db ' desde la entrada ', 0
t_k_irq0        db '; la IRQ 0 va a la entrada ', 0
t_k_ioa0        db '; el IOAPIC tiene la entrada 0 = ', 0
t_k_ioa2        db ' y la 2 = ', 0
t_sin_gop       db 13, 10, 'ESTE EQUIPO NO DA UNA PANTALLA GRAFICA (GOP) DE 640x400 O MAS con color', 13, 10
                db 'de 32 bits, que es donde HispaBIOS dibuja la pantalla de DOS.', 13, 10
                db 'Que hacer: si la configuracion del equipo (BIOS/UEFI) tiene "CSM" o', 13, 10
                db '"arranque heredado", activelo y arranque la memoria USB en ese modo.', 13, 10, 0
t_vuelta        db 13, 10, 'La BIOS de modo real ha vuelto al firmware.', 13, 10, 0
t_guardado      db 13, 10, 'Se ha guardado el registro en la memoria USB: HISPABIOS.LOG', 13, 10
                db '(para saber que pasa, hace falta ese archivo).', 13, 10, 0
t_no_guardado   db 13, 10, 'No se ha podido guardar el registro (HISPABIOS.LOG) en la memoria USB:', 13, 10
                db 'haga una foto de esta pantalla.', 13, 10, 0
t_tecla         db 'Pulse una tecla para volver al firmware (o espere 2 minutos).', 13, 10, 0

; dónde puede ir la BIOS de modo real: base, dónde está su copia en esta
; imagen y cuánto ocupa (por orden de preferencia; acaba en 0). En 80000h no:
; ahí se copia el sector de arranque de Hispa-DOS (boot/boot16.asm, RELOC_SEG)
        align 4
bases:
        dd 90000h, bios_9 - pe_inicio, bios_9_fin - bios_9
        dd 70000h, bios_7 - pe_inicio, bios_7_fin - bios_7
        dd 60000h, bios_6 - pe_inicio, bios_6_fin - bios_6
        dd 0
%ifdef PRUEBA_OCUPA
prueba_ocupa    dd PRUEBA_OCUPA, -1
%endif
        align 2
nombre_reg      dw 'H','I','S','P','A','B','I','O','S','.','L','O','G', 0
nombre_giro     dw 'H','B','G','I','R','O','.','C','F','G', 0
        align 4
guid_ficheros   dd 964E5B22h            ; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
                dw 6459h, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_imagen     dd 5B1B31A1h
                dw 9562h, 11D2h
                db 8Eh, 3Fh, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_camino     dd 09576E91h
                dw 6D3Fh, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_bloques    dd 964E5B21h
                dw 6459h, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_8259       dd 38321DBAh
                dw 4FE0h, 4E17h
                db 8Ah, 0ECh, 41h, 30h, 55h, 0EAh, 0EDh, 0C1h
guid_gop        dd 9042A9DEh
                dw 23DCh, 4A38h
                db 96h, 0FBh, 7Ah, 0DEh, 0D0h, 80h, 51h, 6Ah
guid_inex       dd 0DD9E7534h           ; EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL
                dw 7762h, 4698h
                db 8Ch, 14h, 0F5h, 85h, 17h, 0A6h, 25h, 0AAh
guid_pciio      dd 4CF5B200h            ; EFI_PCI_IO_PROTOCOL
                dw 68B8h, 4CA5h
                db 9Eh, 0ECh, 0B2h, 3Eh, 3Fh, 50h, 02h, 9Ah
guid_acpi2      dd 8868E871h
                dw 0E4F1h, 11D3h
                db 0BCh, 22h, 00h, 80h, 0C7h, 3Ch, 88h, 81h
guid_acpi1      dd 0EB9D2D30h
                dw 2D88h, 11D3h
                db 9Ah, 16h, 00h, 90h, 27h, 3Fh, 0C1h, 4Dh
guid_smbios     dd 0EB9D2D31h
                dw 2D88h, 11D3h
                db 9Ah, 16h, 00h, 90h, 27h, 3Fh, 0C1h, 4Dh
guid_smbios3    dd 0F2FD1544h
                dw 9794h, 4A2Ch
                db 99h, 2Eh, 0E5h, 0BBh, 0CFh, 20h, 0E3h, 94h
%ifdef PRUEBA_P2SB
        align 16
sim_prc         dd 00000007h            ; ITSSPRC de mentira: 8254CGE (bit 2) puesto
sim_oculto      db 1
sim_cambios     db 0
t_sim           db '(prueba) P2SB de mentira: oculto = ', 0
t_sim2          db ', cambios de ocultacion = ', 0
t_sim3          db ', ITSSPRC = ', 0
%endif
        align 8
fw_a            times 8 dd 0            ; los argumentos de la llamada al firmware (FW)
imagen          dd 0
sistema         dd 0
conout          dd 0
bootserv        dd 0
bios_base       dd 0                    ; dónde está la BIOS de modo real
bios_cual       dd 0                    ; su entrada en «bases»
bajo_tope       dd 0
mapa_dir        dd 0                    ; el mapa de memoria
mapa_cabe       dd 0
mapa_tam        dd 0
mapa_llave      dd 0
desc_tam        dd 0
desc_ver        dd 0
mapa_min        dd 0, 0, 0, 0
me_fin          dd 0
me_vis          dd 0
me_no           dd 0
me_tot          dd 0, 0
dir_pag         dq 0                    ; EFI_PHYSICAL_ADDRESS (64 bits)
reg_dir         dd 0                    ; el registro (texto) y cuánto lleva
reg_n           dd 0
reg_marca       dd 0
reg_escrito     dd 0
reg_vol         dd 0
reg_raiz        dd 0
reg_fich        dd 0
fg_nom          dd 0
fg_dat          dd 0
fg_n            dd 0
fg_res          dd 0
m_ext           dd 0
m_vec           dd 0
p8259           dd 0
vec0            dd 0
rsdp            dd 0                    ; la RSDP de ACPI (tablas_copiar)
tc_acpi         dd 0
tc_smb          dd 0
tc_smb3         dd 0
sd_fin          dd 0
sd_t0           dd 0                    ; (los tres, seguidos: tipos 0, 1 y 2)
sd_t1           dd 0
sd_t2           dd 0
presta_dir      dd 0, 0, 0, 0           ; las páginas prestadas
presta_guarda   dd 0                    ; dónde se guarda su otra versión
pit_l1          dd 0                    ; las dos lecturas del 8254 (pit_cuenta)
pit_l2          dd 0
rm_a            dd 0
rm_b            dd 0
rm_c            dd 0
rm_d            dd 0
imagen_cargada  dd 0
camino_arranque dd 0
camino          dd 0
bloques         dd 0
n_manejadores   dd 0
manejadores     dd 0
db_pasada       dd 0
db_i            dd 0
db_man          dd 0
db_arr          dd 0
db_sec          dd 0, 0
dp_buf          dd 0
dc_pci          dd 0
dc_cfg          dd 0, 0, 0, 0
tecla_efi       dd 0
coninex         dd 0                    ; SimpleTextInputEx
keydata         times 12 db 0           ; tecla + estado de Ctrl/Alt/...
discos          times 16 dd 0           ; Block I/O y MediaId de cada disco (8)
gop             dd 0
fb              dd 0                    ; la pantalla de verdad: el marco,
fb_alta         dd 0
fb_ancho        dd 0                    ; sus medidas,
fb_alto         dd 0
fb_formato      dd 0
fb_linea        dd 0                    ; y los puntos de cada línea
p_dir           dd 0                    ; dónde se dibuja: la pantalla o, con giro, la
p_lin           dd 0                    ; pantalla lógica (lb); sus puntos por línea
p_an            dd 0                    ; y sus medidas (con giro de un cuarto, cambiadas)
p_al            dd 0
lb              dd 0                    ; la pantalla lógica y las páginas que ocupa
lb_pags         dd 0
lb_quiere       dd 0
s_x0            dd 7FFFFFFFh            ; el rectángulo sucio de la pantalla lógica
s_y0            dd 7FFFFFFFh
s_x1            dd 0
s_y1            dd 0
gr_x0           dd 0                    ; girar_sucio: el rectángulo en la pantalla,
gr_x1           dd 0
gr_y0           dd 0
gr_y1           dd 0
gr_x            dd 0                    ; la tesela
gr_y            dd 0
gr_nc           dd 0
gr_nf           dd 0
gr_paso_f       dd 0                    ; y los pasos por la pantalla lógica
gr_paso_c       dd 0
gm_max          dd 0
gm_tam          dd 0
gm_info         dd 0
gn_el           dd 0
gn_pts          dd 0
gn_tot          dd 0
wc_tablas       dd 0                    ; las cinco páginas de las tablas propias
wc_cr3          dd 0                    ; lo mismo, cuando se pueden usar (0: no hay)
wc_sv0          dd 0                    ; CR0, CR3 y CR4 del firmware mientras se dibuja
wc_sv3          dd 0
wc_sv4          dd 0
wc_svp0         dd 0                    ; (pruebas: PRUEBA_SIN_PAG)
wc_svp4         dd 0
wp_cpu          dd 0
wp_n            dd 0
wp_tipo         dd 0
wp_e0           dd 0
wp_e1           dd 0
wp_t1           dd 0
wp_t2           dd 0
vbe_lfb         dd 0                    ; el marco lineal que ven los programas
vbe_ant         dd 0                    ; lo que hay pintado de él
tsc_tic         dd 0                    ; cuentas del TSC por tic (54925 us)
t_pant          dd 0                    ; microsegundos que cuesta pintar la pantalla entera
esc_k           dd 0                    ; texto: 0 = ampliado a toda la pantalla; k = 640k x 400k
g_an            dd 0                    ; el modo VBE puesto: ancho, alto, ampliación
g_al            dd 0                    ; y dónde cae en la pantalla
g_k             dd 0
g_x0            dd 0
g_y0            dd 0
gp_y            dd 0
gp_xa           dd 0
gp_xb           dd 0
pd_t            dd 0, 0
dib_an          dd 0
dib_al          dd 0
pc_tin          dd 0
pc_fon          dd 0
pc_x            dd 0
pc_y            dd 0
pc_an           dd 0
pc_gx           dd 0
pc_gy           dd 0
; los modos VBE (los mismos de vbe_modos en bios64.asm): ancho, alto
g_modos         dd 640, 480, 800, 600, 1024, 768
G_NMODOS        equ 3
; los 16 colores del modo texto (00RRGGBB)
paleta          dd 000000h, 0000AAh, 00AA00h, 00AAAAh, 0AA0000h, 0AA00AAh, 0AA5500h, 0AAAAAAh
                dd 555555h, 5555FFh, 55FF55h, 55FFFFh, 0FF5555h, 0FF55FFh, 0FFFF55h, 0FFFFFFh
pc_bits         db 0
presta_n        db 0
seguro          db 0                    ; 1: modo seguro (tecla S al arrancar)
giro            db 0                    ; cuartos de vuelta a la derecha de la imagen (0..3)
giro_vert       db 0                    ; 1: la pantalla del firmware es vertical
giro_fich       db 0                    ; 1: el giro viene de \HBGIRO.CFG
giro_tecla      db 0                    ; 1: se ha pulsado G al arrancar
buf_giro        db 0, 0
m_singop        db 0
tc_acpi_es2     db 0
inex_est        db 0                    ; SimpleTextInputEx: 0 sin mirar, 1 hay, 2 no hay
it_oc           db 0
gn_cumple       db 0
wc_usar         db 0                    ; 1: se pinta con las tablas propias
graf            db 0                    ; 0: texto; 1...: modo VBE puesto
g_todo          db 0                    ; 1: hay que pintar el marco entero
ndiscos         db 0
borrada         db 0
callado         db 0                    ; 1: «escribir» solo apunta en el registro
rj_callado      db 0
buf_car         db 0, 0
dada            times NPAG db 0         ; 1: el firmware ha dado esa página baja (2: prestada)
buf_txt         times 64 db 0
fuente:         incbin "FUENTE16.BIN"
                align 8
buf16           times 256 dw 0
sombra          times 2000 dw 0         ; lo que hay pintado (carácter y atributo)
        align 16
linea_buf       times LIN_MAX dd 0      ; una línea de puntos ya ampliada
tesela          times TES_AN * TES_AL dd 0  ; un trozo de pantalla ya girado
tab_x           times 641 dd 0
tab_y           times 401 dd 0
; la BIOS de modo real, ensamblada para cada base posible
bios_9:         incbin "BIOS32.BIN"
bios_9_fin:
bios_7:         incbin "BIOS32_7.BIN"
bios_7_fin:
bios_6:         incbin "BIOS32_6.BIN"
bios_6_fin:
