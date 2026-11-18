; =============================================================================
;  Hispa Ventanas - [HISPA-486] variables de GDI32 de la fase 3d (win32g.inc,
;  win32t.inc, win32b.inc, win32r.inc, win32ga.inc)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
            alignb 4
wg_prog     resd 1                  ; el programa que llama (lo pone pe_despacho)
wg_dc       resd 1                  ; el contexto abierto
wg_mapa     resd 1                  ;   (de memoria) la ficha de su mapa
wg_dox      resd 1                  ;   el origen de su zona en la superficie
wg_doy      resd 1
wg_ox       resd 1                  ;   lógicas -> superficie: lo que se suma
wg_oy       resd 1
wg_sptr     resd 1                  ;   la superficie de antes (la pantalla)
wg_san      resd 1
wg_sal      resd 1
wg_txt      resd 1                  ;   el color del texto (en el destino)
wg_bk       resd 1                  ;   el del fondo
wg_rop      resd 1                  ;   la ROP2
wg_c        resd 1                  ; el color con el que pintan las primitivas
wg_bx       resd 1                  ; el origen del pincel (superficie)
wg_by       resd 1
wg_pm       resd 1                  ; el pincel preparado: -1 nada, 0 liso, 1 rayado, 2 dibujo
wg_pt       resd 1                  ;   su color
wg_pf       resd 1                  ;   el fondo del rayado (-1: transparente)
wg_pmask    resb 8                  ;   el rayado
wg_patbuf   resb 64                 ;   el dibujo (ya en el destino)
wg_pa       resd 1                  ; la pluma preparada: su ancho,
wg_pe       resd 1                  ;   su estilo,
wg_pbits    resd 1                  ;   sus puntos,
wg_plen     resd 1                  ;   cuántos son,
wg_ppos     resd 1                  ;   por cuál va
wg_pcol     resd 1                  ;   y su color
wg_fh       resd 1                  ; la letra preparada: alto de la celda,
wg_fw       resd 1                  ;   ancho,
wg_fb       resd 1                  ;   negrita,
wg_ff       resd 1                  ;   cursiva, subrayado, tachado
wg_fasc     resd 1                  ;   la línea de base
wg_fextra   resd 1                  ;   lo de más entre letras
wg_tdx      resd 1                  ;   los avances (o 0)
wg_t0       resd 1                  ; para todo
wg_t1       resd 1
wg_t2       resd 1
wg_t3       resd 1
wg_t4       resd 1
wg_t5       resd 1
wg_t6       resd 1
wg_t7       resd 1
wg_fil_y0   resd 1                  ; las filas de una figura
wg_fil_n    resd 1
wg_rr_l     resd 1                  ; (RoundRect)
wg_rr_t     resd 1
wg_rr_r     resd 1
wg_rr_b     resd 1
wg_cx2      resd 1                  ; (Arc, Pie, Chord) el centro en dobles,
wg_cy2      resd 1
wg_sa       resd 1                  ;   los ejes,
wg_sb       resd 1
wg_sx       resd 1                  ;   las direcciones del inicio y del fin,
wg_sy       resd 1
wg_ex       resd 1
wg_ey       resd 1
wg_cse      resd 1
wg_dse      resd 1
wg_px0      resd 1                  ;   y los puntos de la elipse
wg_py0      resd 1
wg_px1      resd 1
wg_py1      resd 1
wg_pn       resd 1                  ; (polígonos) cuántos,
wg_pcnt     resd 64                 ;   los puntos de cada uno
wg_ncx      resd 1                  ;   y los cortes de una fila
wg_cxs      resd 128 * 2
wg_bz       resd 8                  ; (PolyBezier) los cuatro puntos
wg_de       resd 4                  ; (DrawEdge) el rectángulo
wg_de_bf    resd 1
wg_dt_n     resd 1                  ; (DrawTextA)
wg_dt_nl    resd 1
wg_dt_dc    resd 1
wg_dt_rect  resd 1
wg_dt_fmt   resd 1
wg_dt_tab   resd 1
wg_dt_sub   resd 1
wg_dt_y     resd 1
wg_dt_alto  resd 1
; la copia de bloques (wg_blt)
wg_b_dx     resd 1
wg_b_dy     resd 1
wg_b_dw     resd 1
wg_b_dh     resd 1
wg_b_sx     resd 1
wg_b_sy     resd 1
wg_b_sw     resd 1
wg_b_sh     resd 1
wg_b_src    resd 1
wg_b_ptr    resd 1
wg_b_paso   resd 1
wg_b_an     resd 1
wg_b_al     resd 1
wg_b_dib    resd 1
wg_b_mapa   resd 1
wg_b_sfondo resd 1
wg_b_tmp    resd 1
wg_b_tmpm   resd 1
wg_b_ultf   resd 1
wg_b_usap   resd 1
wg_b_usas   resd 1
wg_b_xid    resd 1
wg_b_c0     resd 1
wg_b_nc     resd 1
wg_b_fp     resd 1
wg_b_fx     resd 1
wg_b_fy     resd 1
wg_b_rop    resb 4
wg_b_xlat   resb 256
wg_b_estira resd 1
wg_mem      resb 1                  ; 1: la superficie es un mapa
wg_mono     resb 1                  ; 1: el destino es monocromo
wg_tmodo    resb 1
wg_ultimo   resb 1                  ; 1: las líneas, con su último punto
wg_filtro   resb 1
wg_con_pluma resb 1
wg_con_pincel resb 1
wg_tabla_ok resb 1
wg_q6       resb 256                ; RGB -> el cubo
wg_gris     resb 256                ;   y los grises
wg_rmap     resb 256                ; (la letra) fila -> fila de la de 8×16
wg_cmap     resb 128                ;   columna -> su bit
            alignb 4
wg_fil_l    resd WG_FILAS           ; las filas de una figura (o las líneas de
wg_fil_r    resd WG_FILAS           ;   DrawTextA)
wg_pts      resd WG_PUNTOS * 2
wg_regtmp   resb REG_MAX * 16
wg_rgt      resb RG_TAM             ; regiones de paso
wg_rgu      resb RG_TAM
wg_rgv      resb RG_TAM
wg_rgw      resb RG_TAM
wg_rgx      resb RG_TAM
wg_rgc      resb RG_TAM
wg_rgd      resb RG_TAM
wg_dibd     resb DB_TAM             ; el DIB de una llamada
wg_fila     resb WG_FILA
wg_tbuf     resb WG_TBUF + 4
wg_tbuf2    resb WG_TBUF + 4
wg_tbuf3    resb WG_TBUF + 8
; ---- USER32, COMDLG32... de la fase 3d (win32x.inc) ----
            alignb 4
w32x_si     resd 5                  ; el SCROLLINFO de un control SCROLLBAR
w32x_hwnd   resd 1
w32x_tpm_ven resd 1                 ; (TrackPopupMenu) la ventana,
w32x_tpm_res resd 1                 ;   la orden elegida,
w32x_tpm_fl resd 1                  ;   TPM_*
w32x_tpm_x  resd 1
w32x_tpm_y  resd 1
w32x_ic_bits resd 1                 ; (iconos y cursores)
w32x_hot    resd 1
w32x_ic_p   resd 1
w32x_ic_masc resd 1
w32x_ic_mpaso resd 1
w32x_ic_w   resd 1
w32x_ic_h   resd 1
w32x_r      resd 4
w32x_guardar resd 1                 ; (GetOpenFileNameA y GetSaveFileNameA)
w32x_ofn    resd 1
w32x_patron resd 1
w32x_lista  resd 1                  ; (ChooseColorA y ChooseFontA)
w32x_lista_n resd 1
w32x_sel    resd 1
w32x_casillas resd 1
w32x_con_casillas resd 1
w32x_tpm_esp resb 1
            alignb 4
w32x_pat    resb 16
w32x_nombre resb 84
w32x_titulo resb 64
w32x_dir0   resb 80
w32x_ruta   resb 160
w32x_bmi    resb 40 + 1024
w32x_buf    resb 2048 + 8
