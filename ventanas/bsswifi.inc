; Hispa Ventanas - [WIFI] variables sin valor inicial de wifi.inc (en la parte
; de 32 bits; se ponen a cero al entrar en modo protegido)
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
            alignb 4
wf_ven      resd 1                  ; la ventanita de la red (0: cerrada)
wf_lis      resd 1                  ; su lista de redes
wf_cla      resd 1                  ; su cuadro de la contraseña
wf_tic      resd 1                  ; la última vez que se miró la red
wf_t0       resd 1                  ; cuándo empezó lo que se espera (buscar, conectar)
wf_msg      resd 1                  ; el aviso de la ventanita (0: ninguno)
wf_nredes   resd 1                  ; redes de la lista
wf_sel      resd 1                  ; la elegida (-1: ninguna)
wf_prim     resd 1                  ; la primera a la vista
wf_firma    resd 1                  ; lo que se ve (si cambia, se repinta)
wf_clave_n  resd 1                  ; letras de la contraseña escrita
wf_ini_n    resd 1                  ; octetos de RED.INI leídos
wf_nv       resd 1                  ; (wf_rayitas) cuántas llenas
wf_modo     resb 1                  ; 0 sin red, 1 cable, 2 Wi-Fi (HSPWIFI)
wf_hspred   resb 1                  ; (cable) contesta a INT 2Fh AH=DAh: hay datos
wf_enlace   resb 1                  ; (cable) 0 sin enlace, 1 con enlace, FFh no se sabe
wf_fase     resb 1                  ; WF_F_*: lo que está haciendo la ventanita
wf_con_clave resb 1                 ; la red que se conecta lleva clave (WPA2)
wf_ssid_n   resb 1
wf_c_lleno  resb 1                  ; (wf_rayitas) los colores
wf_c_hueco  resb 1
wf_cfg      resb 32                 ; la configuración IP compartida (INT 2Fh DA00h)
wf_info     resb WI_LEN + 3         ; el estado del Wi-Fi (struct w_info)
wf_redes    resb WF_REDES * WR_LEN  ; las redes (struct ll_red), de más a menos señal
wf_ssid     resb 36                 ; la red que se conecta
wf_clave    resb 68                 ; la contraseña que se escribe (se borra al usarla)
wf_pmk      resb 32                 ; la clave preparada
wf_txt      resb 128
wf_ruta     resb 24                 ; «A:\HISPADOS\RED.INI» (la unidad de arranque)
wf_ini      resb WF_INI
wf_ini2     resb WF_INI + 160
