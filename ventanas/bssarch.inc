; Hispa Ventanas - [ARCHIVOS] variables sin valor inicial de archapi.inc (en
; la parte de 32 bits; se ponen a cero al entrar en modo protegido)
            alignb 4
arch_reab_p resd 1                  ; (prog_abrir) qué abrir al volver
arch_acc    resb AC_TAM             ; el acceso directo de paso de prog_abrir
cen_reabrir resb 120                ; CENTRO.INI, «Reabrir=»: el módulo que se abre al volver
int_param   resb 80                 ; los parámetros para un programa interno (el Bloc)
