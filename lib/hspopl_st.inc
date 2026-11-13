; hspopl_st.inc - estado de la FM (dentro del bloque de estado de hspsb.inc)

; un operador
O_PHASE     equ 0                   ; dd: los 10 bits altos, el punto de la onda
O_INC       equ 4                   ; dd: avance por muestra
O_ARI       equ 8                   ; dd 16.16: pasos de envolvente por muestra
O_DRI       equ 12                  ;    (ataque, caida y suelta)
O_RRI       equ 16
O_ENV       equ 20                  ; dw: atenuacion (0 = maximo, 511 = nada)
O_EGACC     equ 22                  ; dw: fraccion de paso acumulada
O_TLL       equ 24                  ; dw: nivel (TL) + atenuacion por altura
O_SL        equ 26                  ; dw: nivel de sostenido
O_OUT       equ 28                  ; dw: las dos ultimas salidas (para la
O_OUT2      equ 30                  ;     realimentacion)
O_STATE     equ 32                  ; db: 0 callado, 1 ataque, 2 caida,
                                    ;     3 sostenido, 4 suelta
O_R20       equ 33                  ; registros 20h, 40h, 60h, 80h y E0h
O_R40       equ 34
O_R60       equ 35
O_R80       equ 36
O_WS        equ 37
O_CH        equ 38                  ; db: su canal
O_SIZE      equ 40
; un canal
C_FNUM      equ 0                   ; dw: frecuencia (10 bits)
C_B0        equ 2                   ; db: registro B0h (tecla y octava)
C_C0        equ 3                   ; db: registro C0h
C_SIZE      equ 4

fm_addr     resb 1
fm_stat     resb 1
fm_r01      resb 1                  ; bit 5: se pueden elegir formas de onda
fm_rbd      resb 1                  ; tremolo, vibrato y percusion
fm_trem     resb 1                  ; tremolo de ahora (0..26)
fm_vibs     resb 1                  ; paso del vibrato (0..7)
fm_any      resb 1                  ; suena algo
fm_tstep    resb 1                  ; paso del tremolo (0..209)
fm_vcnt     resb 1
            alignb 2
fm_lfo      resw 1
fm_nact     resw 1
fm_k        resd 1                  ; 49716 Hz / frecuencia de salida (16.16)
fm_sum      resd 1
fm_noise    resd 1
fm_ch       resb 9 * C_SIZE
fm_op       resb 18 * O_SIZE
