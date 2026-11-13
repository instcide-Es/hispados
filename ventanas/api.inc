; =============================================================================
;  Hispa Ventanas - la interfaz para los programas aparte (módulos .HVP)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  La lista de lo que Hispa Ventanas ofrece a un módulo, en orden: cada línea
;  es una entrada de la tabla que el cargador (modulos.inc) copia dentro del
;  módulo al abrirlo. La usan los dos lados: VENTANAS.COM hace con ella la
;  tabla de direcciones (api_tabla) y ventanas/hvp.inc los números HV_nombre
;  con los que el módulo llama (macro HV).
;
;  REGLA: solo se añade al final. Nunca se quita, se cambia de sitio ni se
;  cambia lo que hace una entrada: así un módulo ya compilado sigue
;  funcionando con un Hispa Ventanas más nuevo. Un módulo dice en su
;  cabecera cuántas entradas conoce; si son más de las que hay, no se carga.
;  Los registros de cada función, en docs/VENTANAS.md.
; =============================================================================
%macro HV_LISTA 0
        ; ---- versión 1 (63 entradas) ----
        HVF ven_crear               ;  0  ventanas
        HVF ven_lista
        HVF ven_destruir
        HVF ven_mover
        HVF ven_mostrar
        HVF ven_activar
        HVF ven_enfocar
        HVF ven_capturar
        HVF ven_soltar
        HVF ven_invalidar
        HVF ven_invalidar_rect      ; 10
        HVF ven_texto
        HVF ven_cli
        HVF ven_hija_id
        HVF ven_defecto
        HVF msg_tomar               ; 15  mensajes
        HVF msg_mirar
        HVF msg_despachar
        HVF msg_poner
        HVF msg_enviar
        HVF msg_salir               ; 20
        HVF app_bucle
        HVF tmp_poner               ; 22  temporizadores y reloj
        HVF tmp_quitar
        HVF tics
        HVF mem_pedir               ; 25  memoria
        HVF mem_soltar
        HVF dc_pintar               ; 27  dibujo
        HVF dc_abrir
        HVF dc_cerrar
        HVF dc_cortar               ; 30
        HVF dc_color
        HVF dc_negrita
        HVF dc_rect
        HVF dc_marco
        HVF dc_linea                ; 35
        HVF dc_punto
        HVF dc_texto
        HVF dc_texto_n
        HVF dc_texto_m
        HVF dc_icono                ; 40
        HVF dc_rombo
        HVF dc_copiar_clave
        HVF dc_mosaico
        HVF dlg_mensaje             ; 44  diálogos
        HVF dlg_pedir
        HVF dlg_modal
        HVF dos_abrir               ; 47  archivos
        HVF dos_crear
        HVF dos_cerrar
        HVF dos_leer                ; 50
        HVF dos_escribir
        HVF dos_puntero
        HVF dos_borrar
        HVF azar                    ; 54  números aleatorios
        HVF azar_semilla
        HVF tec_estado              ; 56  teclado: mayúsculas, Ctrl y Alt
        HVF pantalla                ; 57  tamaño de la pantalla
        HVF c_boton                 ; 58  (datos) las clases de los controles
        HVF c_estatico
        HVF c_edit                  ; 60
        HVF c_lista
        HVF c_dialogo               ; 62
        ; ---- [Configuración] añadidas para CONFIG.HVP (66 entradas) ----
        HVF aj_leer                 ; 63  ajustes: EAX=número (AJ_*) -> EAX
        HVF aj_poner                ; 64  EAX=número EDX=valor: lo pone y lo aplica
        HVF esc_fondo               ; 65  ESI=mosaico de 16x16 (256 octetos): fondo nuevo
        ; ---- [Pintura] mapas de bits, archivos y memoria grande (72 entradas) ----
        HVF dc_copiar               ; 66  dibuja un mapa de bits (un octeto por punto)
        HVF dlg_archivo             ; 67  diálogo de elegir un archivo
        HVF mem_bloque              ; 68  memoria extendida aparte del montón
        HVF mem_bloque_soltar       ; 69
        HVF pal_rgb                 ; 70  (datos) la paleta: 256 de rojo, verde, azul, 0
        HVF fuente8x16              ; 71  (datos) la fuente: 256 caracteres de 16 filas
        ; ---- [Hispa-Nes] turno, teclas mantenidas, sonido seguido y carpeta (76 entradas) ----
        HVF ceder                   ; 72  da el turno a los demás programas y vuelve
        HVF tec_mapa                ; 73  (datos) 128 octetos: 1 = esa tecla está pulsada
        HVF snd_pcm                 ; 74  un trozo de sonido para HSPSND
        HVF dos_dir_actual          ; 75  la carpeta de trabajo: «A:\CARPETA»
        ; ---- [CAMPANITA] [Reproductor de sonido] la tarjeta, trozos grandes y la campanita (80 entradas) ----
        HVF snd_info                ; 76  -> EAX=tarjeta de HSPSND (-1: no está) ESI=su nombre
        HVF snd_pcm2                ; 77  como snd_pcm, con trozos de hasta 8.192 octetos
        HVF snd_orden               ; 78  AH=función de HSPSND (03h parar...) AL=dato -> EAX
        HVF snd_campana             ; 79  la campanita del volumen (si no hay silencio)
        ; ---- [TAMAÑO] barras de desplazamiento y forma del cursor (82 entradas) ----
        HVF c_barra                 ; 80  (datos) la clase de la barra de desplazamiento
        HVF cur_forma               ; 81  EAX=CUR_* : la forma del cursor sobre la ventana EBP
        ; ---- [MEZCLADOR] el mezclador de HSPSND (84 entradas) ----
        HVF snd_mezcla              ; 82  EAX EBX ECX EDX a HSPSND (AH=función) -> los cuatro; CF=1 si no hay sonido
        HVF mez_abrir               ; 83  abre el Mezclador (MEZCLAD.HVP); EAX=1: en la vista Completa
%endmacro
