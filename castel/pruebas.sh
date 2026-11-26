#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas.sh - las pruebas de Castel. Cuatro partes:
#   1. Los errores: cada ejemplos/errores/*.cas tiene que dar, al compilar, el
#      mensaje (con su línea y su columna) que lleva escrito en «; ERROR: ».
#   2. Los ejemplos: se compilan todos, se ejecutan dentro de Hispa-8086 (en
#      QEMU) con la salida mandada a un archivo, y cada una se compara con
#      ejemplos/esperado/NOMBRE.txt. Entre ellos, los de la Fase 4: los
#      números de 32 bits (largos), la biblioteca escrita en Castel (biblio,
#      ficheros) y los programas en varios archivos (modulos, con los módulos
#      de ejemplos/partes).
#   3. MAS: la orden MAS escrita en Castel (ejemplos/mas.cas) frente a la de
#      ensamblador (h8086/ordenes/mas.asm), con las mismas entradas y las
#      mismas teclas: archivo, «<», tubería, archivo que no está, pausas,
#      Esc. Tienen que escribir lo mismo, octeto a octeto, en la salida y en
#      la pantalla.
#   4. CONTAR y NUMERA (ejemplos/contar.cas y numera.cas): los programas de
#      verdad de la Fase 4, con archivos, «<», tubería, uno que no está y uno
#      de más de 65535 octetos; lo que escriben se compara con lo que sale de
#      hacer la misma cuenta en Python.
#   5. El ensamblador propio: ensambla.py (la referencia, en Python) y ENSAMBLA
#      (ensamblador/ensambla.cas, escrito en Castel, corriendo en Hispa-8086)
#      ensamblan todos los ejemplos, la biblioteca entera, las formas de
#      ensamblador/formas.asm y el propio ENSAMBLA, y cada archivo tiene que
#      salir igual, octeto a octeto, que el de NASM.
#   6. El compilador escrito en Castel (compilador/castel.cas -> CASTEL.EXE),
#      dentro de Hispa-8086: saca el mismo ensamblador que ccastel.py para
#      todos los ejemplos, la biblioteca entera y ENSAMBLA, y dice los mismos
#      errores. Y EL ARRANQUE EN TRES PASOS: (1) ccastel.py compila el
#      compilador; (2) ese CASTEL.EXE, dentro de Hispa-8086, se compila a sí
#      mismo y ENSAMBLA lo ensambla; (3) el resultado se compila otra vez.
#      Los pasos 2 y 3 tienen que dar el mismo ensamblador y el mismo .EXE
#      (que son, además, los del paso 1 con NASM).
#   8. Castel de 32 bits (CASTEL32.md): los ejemplos de 16 bits que no tocan
#      el DOS, compilados como núcleo de 32 bits, escriben lo mismo; los de
#      ejemplos32 (núcleos en QEMU y programas Hispa32 con HW32, si está el
#      disquete de Hispa-DOS), con la biblioteca de 32 bits (biblio32);
#      cuentas al azar con las reglas de 32 bits; y la semilla de
#      Hispa-Pentium (nucleo32): IDT, reloj, teclado, excepciones, la
#      paginación con su fallo de página atendido, el montón (sin fugas, y
#      viendo lo que lo estropea), las tareas que se alternan con el reloj,
#      dormir y esperar, y los programas del anillo 3 con su llamada al
#      sistema (y los que hacen lo que no deben, que se acaban). Y con un
#      disco duro IDE de imagen (nucleo32/disco.sh; FAT12, FAT16, FAT32 y con
#      tabla de particiones): el controlador ATA, la FAT, cargar programas
#      del disco, las llamadas al sistema nuevas (teclado, archivos, lanzar y
#      esperar, memoria que crece), la pila que crece sola y el intérprete
#      de órdenes del anillo 3. Y escribiendo en el disco (en copias de esas
#      imágenes, que luego miran fsck.fat y mtools): crear, escribir, alargar,
#      cortar y borrar archivos, las carpetas, COPY, DEL, REN, MD, RD y la
#      redirección; CD y la carpeta de trabajo, PATH, los archivos .BAT, la
#      historia de la línea y Ctrl+C; el teclado español y los nombres largos.
#   9. Hispa-Pentium 0.6: los discos SATA (AHCI: leer y escribir por DMA,
#      con FAT32, particiones MBR y GPT), dos discos a la vez con su letra
#      (C:, D:) y las rutas con letra, el permiso de escritura por disco
#      («escritura», «escritura=usb»); y el USB (xHCI, EHCI y UHCI) sin teclado ni
#      ratón PS/2: el teclado USB (el español), el ratón USB en el
#      escritorio, un concentrador, la memoria USB como unidad X: (leer,
#      escribir, fsck.fat), «sinusb», Mi equipo con sus unidades, y la
#      memoria USB con GRUB arrancada como tal (el núcleo la lee él). Y la
#      oficina de Hispa-386 dentro de Hispa-Pentium (HVPRUN.BIN, los programas
#      de Hispa Ventanas): el grupo «Oficina» del menú Inicio, Facturas (hacer
#      una factura, que queda en el disco, y volver a abrirla), Pluma (escribir
#      y guardar), el portapapeles hasta el Bloc de notas, Cuentas, y Mi equipo
#      abriendo un documento de Pluma y una imagen con Mirador.
#  10. Hispa-Pentium 0.7: instalarlo en el disco duro. El programa INSTALAR
#      (pantalla azul, en modo texto) con discos SATA e IDE en blanco, con
#      una partición de otro sistema y espacio sin usar (no se toca lo que
#      hay), llenos (borrar pide dos confirmaciones), con tabla GPT y de los
#      de antes (la partición en el sector 63); la FAT32 que escribe (la mira
#      fsck.fat) y lo que copia; que sin «instalar» el núcleo no deja; y, de
#      verdad: arrancar la memoria USB (xHCI y EHCI), instalar, arrancar
#      SOLO del disco (GRUB en el MBR), el asistente del primer arranque,
#      HISPA.INI y el saludo del menú Inicio; la memoria USB, intacta.
#      Y la oficina en el disco instalado (C:\OFICINA y C:\HISPA\HVPRUN.BIN):
#      desde el menú Inicio y desde Mi equipo.
#  11. El USB en caliente (usbcal.cas): con el monitor de QEMU (device_add y
#      device_del), en xHCI y EHCI (y un teclado por las UHCI compañeras):
#      una memoria que se enchufa sale como W: y se lee; escribir y «quitar
#      con seguridad» la deja sana (fsck.fat); quitarla a lo bruto con un
#      archivo abierto no cuelga nada (el programa ve errores y sigue);
#      teclados, ratones y un concentrador enchufados y quitados en marcha;
#      la memoria del sistema no se deja quitar; y en el escritorio, el
#      globo de los avisos, el icono de la bandeja con su menú y Mi equipo,
#      que se pone al día solo y tiene «Quitar con seguridad».
#  12. Hispa-Pentium 0.8: los paquetes. tools/hpq.py (paquetes .HPQ, su
#      catálogo y los registros); el programa PAQUETES en un disco
#      «instalado», con una memoria USB de paquetes: instalar con
#      dependencias (de un archivo y por su nombre), actualizar, una suma mala,
#      un conflicto, quitar lo que otro necesita (el disco, idéntico), quitar,
#      COMPRUEBA, un corte a mitad que se deshace al volver, el gancho de la red
#      (con un DESCARGA de mentira) y la ventana «Programas y actualizaciones»;
#      fsck.fat y mtools miran el disco después de cada paso.
#  13. Hispa-Pentium 0.8: la red. El servicio de red (RED.BIN) con la e1000
#      y la RTL8139 de QEMU (-netdev user): DHCP, IPCONFIG, PING, el DNS,
#      DESCARGAR contra un servidor HTTP de aquí (nucleo32/servidor_prueba.py:
#      con largo, en trozos, hasta que se cierra, redirección, 404) con lo
#      descargado igual octeto a octeto, Ctrl+C, sin tarjeta, «sinred», y el
#      icono de la red en el escritorio.
#  13 bis. (TLS) Hispa-Pentium 0.8: HTTPS. El servicio TLS (TLS.BIN,
#      nucleo32/tls/: BearSSL en el anillo 3) contra los servidores HTTPS de
#      nucleo32/tls/servidor_https.py (con su raíz de prueba en \RAICES):
#      DESCARGAR https:// (ECDSA y RSA, con largo y en trozos, igual octeto
#      a octeto), la orden HTTPS con la biblioteca Https (galletas,
#      keep-alive, POST con cabeceras, redirecciones entre servidores), los
#      certificados que no valen (otro nombre, raíz desconocida, caducado),
#      la memoria de cadenas (TLS.CCH) y la hora: el reloj del PC malo, la
#      de la cabecera Date (TLS.INI) y sin hora. Si esta máquina sale a
#      internet, también https://clicgest.es/ y https://hispados.es/ frente
#      a curl, y la hora de actualiza.hispados.es.
#  14. Hispa-Pentium 0.8: apagar de verdad y la batería. El servicio de ACPI
#      (ACPI.BIN, acpi/pentium/: HSPACPI con ACPICA en el anillo 3) con la
#      batería de mentira de las pruebas de Hispa-DOS (-acpitable): la pila
#      de la bandeja y su recuadro de ayuda, apagar (S5) y reiniciar en
#      orden con PIIX4 y con ICH9, «sinacpi» y las llamadas 150 a 153.
#  15. CASTEL32 (pruebas32.sh): el compilador escrito en Castel, de 32 bits.
#      Con corre32, en Linux: el mismo ensamblador que ccastel.py en todos los
#      programas (y los mismos errores), ENSAMB32 igual que NASM y el arranque
#      en tres pasos. Dentro de Hispa-Pentium (QEMU): el núcleo entero y sus
#      programas compilados y ensamblados ahí, idénticos, con las tablas lejos
#      del tope (--gasto), y CASTEL32 compilándose a sí mismo hasta el punto
#      fijo. Y con HW32 en Hispa-DOS (Hispa-386/486), CASTELW, también hasta
#      el punto fijo. (Desde el 20/11 el núcleo de Hispa-Pentium ya no lo
#      compila CASTEL de 16 bits en las partes 5 y 6: lo hacen CASTEL32 y
#      ENSAMB32 aquí; CASTEL.md y CASTEL32.md §11.)
#  16. Hispa-Pentium 0.8: el CD y las actualizaciones por Internet. El CD
#      (nucleo32/cd.sh: ISO con El Torito sin emulación y GRUB), en QEMU
#      como lo pone VirtualBox: «pc» con IDE (PIIX), USB OHCI y la e1000
#      82540EM, y «q35» con SATA (AHCI, el CD también); arrancar sin
#      instalar, instalar desde él y arrancar del disco. Y en el sistema
#      instalado, por la red (un servidor HTTP de aquí con un catálogo
#      firmado): el aviso de la bandeja al arrancar, la ventana, actualizar
#      el núcleo, reiniciar, que arranque el nuevo; y con un núcleo que se
#      cuelga, que GRUB vuelva solo al de antes.
#  17. Hispa-Pentium 0.8: la medida de la pantalla, desde Configuración
#      (Cambiar...). Con la VBE de Bochs (QEMU -vga std; y el CD con el modo
#      de GRUB, en un «pc» como VirtualBox, con -vga std y con -vga vmware,
#      la VMSVGA): la lista (lo que da la tarjeta y lo que dice la pantalla,
#      su EDID), «Probar...» en el momento, con una ventana maximizada que
#      se pone a la medida nueva, «Mantener», y la vuelta sola a los 15 s.
#      Sin ella (-vga cirrus, como un PC de verdad), en un sistema instalado
#      desde el CD: \HISPA\PANTALLA.ENV y GRUB.CFG, «Reiniciar ahora», el
#      arranque a prueba y «Mantener»; y si se reinicia sin darla por buena,
#      GRUB vuelve solo a la de antes. Y, con la VBE de Bochs, la medida
#      guardada que la BIOS no tiene (1200 x 800) la pone el núcleo al arrancar.
#  18. El Navegador (NAVEGA.BIN, nucleo32/navega/: NetSurf con QuickJS en el
#      anillo 3) contra el sitio de prueba de navega/servidor_pruebas.py
#      (HTTP y HTTPS con su raíz en \RAICES): CSS de fuera con variables,
#      PNG y JPEG, JavaScript (DOM, setTimeout, localStorage, fetch con
#      galletas), un formulario POST, la rueda, Fin, Atrás, el menú del botón
#      derecho y las pestañas, la dirección, el portapapeles, una descarga a
#      C:\DESCARGA (igual octeto a octeto) y las galletas al cerrar; y el x87
#      de cada programa (X87.BIN).
#  19. Hispa-Pentium 0.8.3: el Administrador de dispositivos (DISPOSIT.BIN y
#      la llamada 155, dispositivos.cas; lo de ACPI lo apunta ACPI.BIN con la
#      156): en un «pc» con e1000, EHCI (teclado, ratón, tableta y memoria),
#      SATA y la batería de mentira, en el escritorio (Mi equipo > Propiedades,
#      Configuración, las pestañas de las propiedades, copiar y guardar el
#      informe en X:); en un «q35» (RTL8139, xHCI con concentrador) el
#      informe en texto; y con «sinusb» y «sinacpi», lo desactivado.
#  19 bis. (INFORME-DISP) Hispa-Pentium 0.8.12: «Enviar informe de los
#      dispositivos que fallan» (DISPOSIT.BIN; pruebas_informe.sh): el
#      receptor (tools/informe-disp/informe.php) con curl y su cron hasta el
#      FTP de prueba; en QEMU, como un usuario normal, por HTTPS: ENVIADO, el
#      tope (429) y demasiado grande (413), sin números de serie, ni UUID, ni
#      claves, ni redes, ni nombres (se buscan a propósito); sin red; y en el
#      escritorio, el botón, la vista, «Cancelar» y «Enviar».
#  20. SSH, WireGuard y Tailscale (SOLO=ssh): SSH.BIN y SSHD.BIN (en C, con
#      Monocypher) contra OpenSSH de aquí; WG.BIN contra el WireGuard del
#      núcleo Linux y TAILSCAL.BIN contra Headscale y un tailscaled (estas
#      dos, con root e ip netns; Tailscale, con HEADSCALE=binario).
#  21. (CD, SOLO=lectores) Hispa-Pentium: los lectores de CD y DVD y las imágenes de disco.
#      Una ISO de prueba con Joliet (genisoimage o xorriso): por IDE (como
#      VirtualBox: el lector en el secundario maestro, el disco en SATA), por
#      AHCI (el q35) y por USB (usb-storage con la ISO, también enchufado en
#      marcha); sus archivos, copiados al disco y comparados octeto a octeto;
#      sacar, cambiar y meter el disco con el monitor de QEMU (eject, change)
#      y EXPULSAR; MONTAR la misma ISO (y una .IMG de disquete y una
#      .CUE/.BIN en crudo) desde C:, desde el CD y desde una memoria USB que
#      se quita (la imagen se desmonta sola); y Mi equipo, con el CD y el CD
#      virtual, montado con dos clics.
#  22. [WIN32, SOLO=win32] Los programas de Windows de 32 bits y DirectX (WIN32.BIN,
#      nucleo32/win32/, docs/pentium/WIN32.md): los de consola de las pruebas
#      de HW32 (PEHOLA, HOLAC, K32PRUE con lo que da Windows 98, una DLL),
#      NASM, TCC (compila un «hola» que luego corre) y FASM, de otros
#      (tests/thirdparty/fetch_win32.sh; si no están, se saltan); los de
#      ventanas de \HISPA32 (HOLA32, AVISO32 con sus MessageBox, CALC32 con
#      el ratón y su menú, MENU32, CTL32) como ventanas de Hispa-Pentium; Naves
#      (DirectDraw a pantalla completa, DirectInput) jugando; el ejemplo DDRAW
#      de FASM (ensamblado ahí); FASMW abriendo y ensamblando HELLO.ASM, y
#      ese HELLO.EXE; Mi equipo abriendo un .EXE; y WSOCK32 (REDPRUE) contra
#      el servidor HTTP de aquí.
#  23. (MENÚ, SOLO=menu) El menú Inicio: su forma (fijados, más usados,
#      Todos los programas, la columna de la derecha y el pie siempre a la
#      vista, a 800x600, 640x480 y 1024x600), las carpetas por el «tipo» de
#      los accesos, un submenú más alto que la pantalla (flechas, rueda),
#      la flecha en diagonal, el teclado (logo, letra inicial, flechas,
#      Esc), Mis documentos; un menú emergente de 30 opciones; y los atajos
#      de todo el sistema: Alt+Tab, Alt+Espacio (Tamaño con las flechas),
#      Ctrl+Alt+Supr, Logo+R, Logo+D, Impr Pant (el BMP en \CAPTURAS),
#      Alt+F4 (en el tapiz: Apagar, con Cerrar sesión), F1 y Logo+E.
#  23 bis. (RUEDA, SOLO=rueda) La rueda del ratón en los programas: la
#      Consola (el historial de lo que ha subido, y vuelve abajo al
#      teclear), el Bloc de notas (sin mover el cursor), Mi equipo (iconos,
#      lista y detalles; Ctrl+rueda cambia la vista), el Administrador de
#      dispositivos y MUESTRA (las rejillas de controles.cas: sin mover lo
#      elegido; Ctrl+rueda, la letra).
#  24. (PROGRAMAS, SOLO=programas) «Programas instalados»: los tipos de los
#      paquetes (tipo=, en la cabecera y en el catálogo), PAQUETES LISTA /TIPO,
#      los programas sueltos, lo imprescindible, QUITA /DEPENDIENTES, REPARA,
#      un corte a mitad de quitar, y la ventana PROGRAMA.BIN (desde
#      Configuración y desde las actualizaciones; desinstalar con su barra,
#      los archivos, reparar y borrar un suelto a la Papelera), con capturas.
#  25. (PROTEGE, SOLO=protege) La zona protegida (permisos.cas): \HISPA solo
#      la escribe el sistema, los secretos solo los lee su dueño, los
#      servicios por su sello (no por su nombre) y el modo administrador.
#  26. (ESCUDO, SOLO=escudo) Escudo, el antivirus (ESCUDO.BIN, nucleo32/escudo/): el motor en
#      Linux con la base pequeña de prueba (escudo/prueba/hazprueba.py: sumas,
#      patrones con comodines y desplazamientos, PE, lógicas, ZIP dentro de
#      ZIP, limpias e ignoradas), lo mismo desde FIRMAS.ESC, y la firma
#      digital de un .cvd de verdad (el de las pruebas de ClamAV, si hay red:
#      buena, y mala si se toca un octeto); en Hispa-Pentium, desde el
#      intérprete (ESCUDO /LISTA, la cuarentena y vuelta), y en el escritorio
#      una memoria USB enchufada en caliente con EICAR (suelto, en un ZIP y un
#      AUTORUN.INF): «¿Revisar la memoria?», la revisión, la cuarentena;
#      «ESCUDO /DESCARGA» (lo que lanza el Navegador) y lo que tarda (MB/s).
#  27. (WIFI, SOLO=wifi) El Wi-Fi (WIFI.BIN, nucleo32/wifi/: el núcleo Wi-Fi de Hispa-486
#      en el anillo 3; la tarjeta Wi-Fi de RED.BIN, rwifi.cas; REDWIFI.BIN).
#      QEMU no tiene tarjetas Wi-Fi: los vectores de las normas y la gestión
#      802.11/WPA2 en el anfitrión (los de Hispa-486); la radio de mentira
#      (RED.INI: WifiSimulada=SI) sin cable: buscar, conectar con WPA2, DHCP y
#      PING por RED.BIN, WIFI.INI sin la contraseña, la contraseña equivocada,
#      y al arrancar otra vez, se conecta sola; con la e1000: Automático,
#      Wi-Fi y Cable (RED.INI); el controlador de verdad (brcmsmac y bcma)
#      contra el modelo de la tarjeta (WIFISIM.BIN) con el microcódigo del
#      disco, la BCM43224 y (BCM4313) la BCM4313 con su PHY LCN; y el escritorio: el icono con rayitas, la ventana del Wi-Fi
#      (la lista, la contraseña con asteriscos, conectar) y Configuración.
#  55. (MÓVIL, SOLO=movil) El móvil por USB, el «Anclaje de USB» de Android
#      (pruebas_movil.sh): RNDIS y CDC-NCM (usbmovil.cas y rmovil.cas) en QEMU,
#      con el móvil de mentira (tests/movil_ncm.py): DHCP, PING, la prioridad
#      cable > móvil > Wi-Fi, enchufar y desenchufar.
#  56. (BLUETOOTH, SOLO=bt) El Bluetooth (pruebas_bt.sh): la pila de BT.BIN contra
#      el modelo (build/bt_prueba) y en QEMU con el adaptador de mentira por
#      usb-redir (emparejar, teclear, reconectar, la ventana, el Intel 0AAA).
#  57. (RTL8723BS, SOLO=rtl) El Wi-Fi por SDIO de las tabletas Bay Trail (pruebas_rtl.sh):
#      el SDIO y la Realtek RTL8723BS contra un modelo (tests/wifi/rtl_sim.c), en el
#      anfitrión (build/rtl_prueba) y en QEMU (WIFISIM.BIN «chiprtl»: WPA2, DHCP, PING);
#      sin firmware; y ACPI.BIN, que aprende la SDIO y su _PS0 (las patillas GPIO) en
#      \SDIO.TXT con una SSDT de tableta, y WIFI.BIN, que lo usa sin ACPI.
#  54. (INTEL, SOLO=intel) El Wi-Fi de Intel (wifi/nucleo/radio_iwl.c): el lector del
#      firmware (.ucode) y el controlador de la 7265 contra el modelo de la tarjeta
#      (tests/wifi/iwl_modelo.c); en QEMU, la e1000 no pone en marcha WIFI.BIN.
#  28. (ACCESORIOS, SOLO=accesorios) Los accesorios: la Calculadora (CALC.BIN,
#      con las cuentas decimales de decimal.cas), el Reloj (RELOJ.BIN) y el
#      Mapa de caracteres (CARACTER.BIN). Con corre32: las cuentas con lo que
#      tienen que dar justo, el calendario y los festivos, cada carácter; en
#      el escritorio: los tres modos de la Calculadora, la cinta y el IVA, la
#      ñ copiada y pegada en el Bloc, la alarma, la cuenta atrás y el calendario.
#  29. (SONIDO, SOLO=sonido) El sonido: SONIDO.BIN con las tres tarjetas de
#      QEMU (HD Audio, AC'97 y Sound Blaster 16), grabando lo que suena
#      (-audiodev wav) y mirando cada sonido (nucleo32/sonido/oye.py): los
#      altavoces izquierdo y derecho, dos programas a la vez, un WAV de 8
#      bits, el volumen y el silencio; el Administrador de dispositivos; el
#      altavoz de la bandeja con la rueda, Configuración > Sonido, los
#      sonidos de inicio y de apagar; DirectSound (WINDX.EXE) y Hispa-Snes en
#      HVPRUN.
#  31. (ZIP, SOLO=zip) Comprimidos (COMPRIM.BIN, ZIP y UNZIP; pruebas_zipvisor.sh):
#      el motor en el anfitrión (comprim/prueba/prueba.sh: con el zip y el
#      unzip de Linux octeto a octeto, ZipCrypto, AES, ZIP64, los ZIP malos y
#      el fuzz con ASan; FUZZ_ZIP=vueltas, SIN_GRANDE=1 sin el de 4 GB); las
#      órdenes en el intérprete; y la ventana desde el explorador (abrir,
#      entrar, Extraer todo, Quitar, Añadir, Propiedades, arrastrar al tapiz,
#      contraseña, «Comprimir en ZIP»), mirando el disco con mtools y unzip.
#  49. (COMPAT, SOLO=compat) El Asistente de compatibilidad (COMPAT.BIN; pruebas_compat.sh): el
#      juego de mentira agrupado por DLL, el informe (sin rutas) y su receptor (php -S, HTTPS de
#      prueba), el aviso al cambiar win32 (PAQUETES ACTUALIZA con un catálogo de prueba), los
#      componentes de Microsoft con un servidor de mentira (CAB y LZX propios) y la ventana.
#  42. (CORREO, SOLO=correo) Correo, el cliente de correo (CORREO.BIN; pruebas_correo.sh): el
#      núcleo MIME/HTML/vCard y el IMAP con ASan y fuzz, el servidor de pruebas propio
#      (IMAP, SMTP, POP3 con TLS), las órdenes y la ventana en QEMU, y lo que tarda en un Atom.
#  48. (UEFI, SOLO=uefi) Arrancar por UEFI (pruebas_uefi.sh): la memoria USB con
#      BOOTX64.EFI (GRUB EFI, multiboot2) en OVMF de 64 bits (y de 32, si hay su
#      GRUB): el escritorio a la medida del GOP, ACPI.BIN con la RSDP de GRUB y la
#      batería de prueba, los discos SATA y USB, la consola de texto pintada en la
#      pantalla del firmware; la misma memoria por la BIOS; y el CD por UEFI.
#  51. (PUNTEROS, SOLO=punteros) Los punteros del ratón (pruebas_punteros.sh): la flecha de
#      siempre, la «I» del Bloc, los bordes; las 13 formas en 6 juegos, 4 tamaños y 11
#      colores sobre claro y oscuro (mira_punteros.py), para zurdos, los anillos, el
#      rastro, sin restos; el Panel > Ratón > Puntero y PUNTERO.INI.
#  47. (ICONOS, SOLO=iconos) Los iconos del tapiz (pruebas_iconos.sh): 30 a 1024 x 600 y
#      a 800 x 600, ninguno en la barra ni en el dock (mira_iconos.py); organizar por
#      nombre, tipo, tamaño y fecha; arrastrar; la cuadrícula, la automática, el teclado,
#      ICONOS.INI; cambiar la medida en marcha; 47 a 640 x 480, en dos páginas; sin dock.
#  53. (SDHCI, SOLO=sdhci) Las tarjetas SD y el eMMC (pruebas_sdhci.sh): la sdhci-pci de
#      QEMU con su tarjeta: leer y escribir por ADMA2 y por PIO, SDSC y SDHC, GPT, la
#      norma 2.00 y la 3.00, sin tarjeta, sin permiso; el modo ACPI con una SSDT de
#      prueba (ACPI.BIN, la llamada 160), dada a mano y aprendida (\SDHCI.TXT); e
#      INSTALAR en la tarjeta SD. (El eMMC no: la QEMU de aquí no lo tiene.)
#  50. (TOUCHPAD, SOLO=touchpad) El touchpad (pruebas_touchpad.sh): touchpad.cas con
#      paquetes Synaptics hechos a mano (RATON /ANALIZA con corre32: mover, tocar, doble
#      toque, arrastrar, el borde derecho, dos dedos, la palma, al escribir, descolocados);
#      el ratón PS/2 de QEMU sigue igual (y RATON /TOUCHPAD lo graba); con «tp_simulado»,
#      RATON /INYECTA baja y sube el Bloc y hace clic; Panel de control > Ratón > Touchpad.
#  55. (BLUETOOTH, SOLO=bt) El Bluetooth (pruebas_bt.sh): la pila de BT.BIN contra su
#      modelo en el anfitrión, y en QEMU un adaptador USB de mentira por usb-redir:
#      buscar, emparejar el teclado y el ratón, lo tecleado, reconectar, el Intel 0AAA.
#  52. (I2CHID, SOLO=i2chid) Los touchpads HID sobre I2C (pruebas_i2chid.sh): hiddesc.cas
#      con descriptores de touchpads de verdad (HIDPRUEB con corre32); lo de ACPI.BIN con
#      acpi_sim (la SSDT de prueba y las tablas del ASUS GL553VD); en QEMU, sin controladora
#      I2C, con la SSDT de prueba (la llamada 159) y con «sini2c».
#  32. (MIRADOR, SOLO=mirador) El Mirador (MIRADOR.BIN): PNG, BMP, JPG, GIF
#  41. (PLUMA32, SOLO=pluma) Pluma 2 (PLUMA.BIN, el procesador de textos de 32 bits en C
#      portable: nucleo32/oficina; pruebas_pluma.sh): formatos, PDF, RTF, corrector, la
#      ventana con un guion, en QEMU (escribir, tabla, imagen, guardar, PDF) y en un Atom.
#  44. (CUENTAS32, SOLO=cuentas) Cuentas 2 (CUENTAS.BIN, la hoja de cálculo de 32 bits en C
#      portable: nucleo32/oficina/cuentas; pruebas_cuentas.sh): las fórmulas (a mano, con
#      Python y fuzz), los formatos (.HOJ, .XLSX, .ODS, .CSV), el PDF, la ventana con un
#      guion, en QEMU (escribir, gráfico, guardar, PDF, volver a abrir) y en un Atom.
#  45. (PINTURA32, SOLO=pintura) Pintura 2 (PINTURA.BIN, el programa de dibujo de 32 bits en C
#      portable: nucleo32/oficina/pintura; pruebas_pintura.sh): formatos y su fuzz, cada herramienta
#      con un guion contra su imagen esperada, en QEMU (pintar, texto, guardar PNG, fondo de
#      escritorio, volver a abrir) y el pincel en un Atom.
#      animado e ICO desde el explorador, las flechas, la rueda, girar,
#      pantalla completa, diapositivas y «Usar como fondo de escritorio» (la
#      llamada 172: FONDO.HPM), que sigue al arrancar otra vez.
#  34. (PANEL, SOLO=panel; pruebas_panel.sh) El Panel de control y la hora
#      por Internet: HORARED.BIN con servidores NTP de mentira (de acuerdo,
#      del 2001, dos días por delante, en desacuerdo, callado), el sello y la
#      llamada 175 (no atrasar más de 2 h), HTTPS aunque NTP mienta y TLS.HOR
#      como suelo; Configuración con sus iconos y el teclado; Fecha y hora,
#      Ratón (velocidad, doble clic, zurdos, rueda), Teclado (el
#      latinoamericano), Energía (apagar la pantalla, el botón de encendido,
#      la batería baja), Personalización (el tema, el tapiz), PANEL.INI y al
#      arrancar otra vez; y el contraste de los temas. (La 33 es la de
#      win32-juegos; la 35, la del Bloc, y la 36,
#      la del Reproductor.)
#  35. (BLOC, SOLO=bloc) El Bloc de notas 1.0 (BLOC.BIN, ubloc.cas y sus
#      módulos bloc*.cas): en Linux, con corre32, la memoria y el texto
#      (búfer con hueco, deshacer sin límite) al azar, las expresiones
#      regulares contra el «re» de Python (fuzz), las páginas (UTF-8, la 437,
#      la 1252; fuzz de ida y vuelta), comparar (Myers, el mínimo), los
#      colores de los diez lenguajes, el Bloc sin ventana con sus guiones
#      (escribir, buscar, reemplazar, páginas, líneas, código, pestañas) y
#      un archivo de 11 MB; y en el escritorio, con capturas en
#      docs/pentium/bloc/: los colores y la negrita, los números y plegar,
#      buscar y reemplazar (Ctrl+H, con las teclas de cambio del núcleo), la
#      letra grande con Ctrl+rueda, Abrir, las pestañas con « *», comparar,
#      buscar en una carpeta y saltar, la ayuda, en columna (Alt+Mayús+flechas),
#      la pregunta al salir, la sesión otra vez, soltar un archivo arrastrado
#      desde Mi equipo y abrir 11 MB.
#  36. (REPRODUCTOR, SOLO=reproductor) El Reproductor (REPRODUC.BIN y la orden
#      REPRODUCE; pruebas_reproductor.sh): en el anfitrión, con ASan, cada
#      formato frente a ffmpeg, sin huecos, las imágenes de CD con su .CUE,
#      buscar, el ecualizador, el remuestreador, la velocidad, las etiquetas,
#      las listas y el fuzz; en QEMU, lo que sale por las tres tarjetas es lo
#      del anfitrión sin un corte, lo que cuesta en un Atom N270, el CD (la
#      llamada 134) por IDE y por SATA, y en el escritorio la ventana, la
#      bandeja (la 135) y que no se corte con el procesador ocupado.
#  33. (JUEGOS, SOLO=win32juegos) Lo que pide un juego de Windows de 1998-2000
#      (Half-Life: Win32, VC6, OpenGL o DirectDraw), con los programas de
#      tests/juegos/: el diagnóstico de lo que falta (el aviso, WIN32.LOG y
#      WIN32 COMPRUEBA), los hilos de verdad y lo que los sincroniza, SEH (el
#      __try/__except de VC6), el registro (en \USUARIOS), MSVCRT de VC6, la
#      ventana a pantalla completa con el ratón de los juegos y DirectInput,
#      UDP de cliente y de servidor, y el sonido (la mezcla en su hilo,
#      waveOut, el CD de sonido por MCI).
#  46. (TERMO, SOLO=termo) El Termómetro (TERMO.BIN, nucleo32/termo/; pruebas_termo.sh): en el
#      anfitrión, el veredicto con los registros de equipos de verdad (HP Mini, Toshiba, ThinkPad),
#      la TjMax, el terminal de SSH.BIN (vt.c, con htop, vim y less de verdad) y el receptor de
#      «Enviar a Carlos» (subir.php); en QEMU, la memoria USB que lo abre al arrancar, la zona
#      térmica de mentira de ACPI, la parada de seguridad, el registro cada 2 s, lo ocupado, sin
#      repintar por el ratón, el apagón a mitad de la carga (FAT sana, .LOG y .CSV al día), el aviso
#      al volver, «Enviar a Carlos» por HTTPS y «Terminal» (SSH con la llave de la memoria contra un
#      sshd propio: xterm-256color, htop, vim, less, las teclas y window-change).
#  39. (JUEGOS, SOLO=juegos) Los juegos (el paquete «juegos», nucleo32/juegos/;
#      pruebas_juegos.sh): la física del pinball en el anfitrión (no atraviesa,
#      conserva, el flipper golpea), los niveles de Almacén, el paquete, el
#      menú Inicio > Juegos y cada juego en QEMU con teclas grabadas, sus
#      récords, el sonido, y lo que gastan en un Atom N270 de QEMU.
#  40. (OFICINA2, SOLO=oficina) La oficina 1.1 en HVPRUN (pruebas_oficina.sh):
#      Datos, el Lector, Publicaciones, Presentaciones y Pintura (abrir,
#      escribir con eñe, tildes y euro, guardar, cerrar y volver a abrir; y lo
#      guardado, mirado en el disco), el menú Inicio (Oficina y Multimedia, sin
#      dos Miradores), las clases de Mi equipo (.BDD, .PUB, .PRE, .PDF y
#      «Editar con Pintura» en BMP y PCX), la rueda, Alt+letra, el
#      portapapeles, maximizar, una ruta larga, \DOCUMENT y la zona protegida.
# Necesita python3, nasm, qemu-system-i386, socat, mtools, mkfs.fat y fsck.fat
#  37. (CORTES, SOLO=cortes) Los cortes de la red (pruebas_cortes.sh): DESCARGAR
#      contra un servidor que cierra, corta, calla o no deja seguir, y con el
#      enlace caído a mitad (sigue desde donde iba, con Range); TCP con el
#      camino muerto (keepalive); PAQUETES con el enlace caído; «Cancelar» en
#      menos de 2 s; «Reiniciar» en plena descarga; «Forzar el cierre» de un
#      programa colgado; y el Wi-Fi simulado que se va y vuelve con otra IP.
#  38. (USUARIOS, SOLO=usuarios) Los usuarios (pruebas_usuarios.sh): ARGON2
#      con ASan en Linux, la serie de tools/hpq.py; un sistema de antes que
#      da de alta usuarios con contraseña (Argon2id) y sus carpetas; el usuario
#      normal que no lee la carpeta de otro, ni instala, ni pide el modo
#      administrador; la entrada en texto y con el escritorio; el catálogo con
#      la serie vieja rechazado; el azar del núcleo, distinto en cada
#      arranque; SSH con los usuarios (OpenSSH de aquí: contraseña buena y
#      mala, sin contraseña, la llave de su carpeta)
# (y, para corre32, gcc y ld con elf_i386; para la parte 16, grub-mkrescue,
# xorriso y python3-cryptography). El disquete de
# Hispa-8086 se monta solo (make en h8086) si no está hecho.
#  58. (REMOTA, SOLO=remota) La consola remota «Dejar entrar a Claude» (SSHD
#      /REMOTA, nucleo32/ssh/remota.c; pruebas_remota.sh) con el cliente
#      tools/remota_pentium.py, por QEMU: sin la ventana, el puerto cerrado;
#      el código (leído de la captura) malo no entra y el bueno sí; dentro,
#      una orden, leer un registro, llevar y traer un archivo y «registros»;
#      uno por conexión; caduca a los 60 s; «Echar»; tres malos, otro código y
#      la pausa; Esc la cierra; y REMOTA.LOG (sin ningún código en él).
#  43. (AGENDA, SOLO=agenda) La Agenda (AGENDA.BIN; pruebas_agenda.sh): las
#      repeticiones frente a python-dateutil, el cambio de hora, un .ICS de
#      Google de ida y vuelta, los cumpleaños de una vCard, los festivos (la
#      misma tabla en el Reloj) y el fuzz del lector .ICS, con corre32; y en
#      QEMU: importar, una cita nueva que avisa con el reloj adelantado,
#      posponer, la repetición semanal, el Reloj y la bandeja.
#   SOLO=errores ./pruebas.sh     solo la primera parte (no arranca QEMU)
#   SOLO=32 ./pruebas.sh          la primera, la octava y la novena (lo de 32 bits)
#   SOLO=06 ./pruebas.sh          la primera y la novena (lo nuevo de Hispa-Pentium 0.6)
#   SOLO=07 ./pruebas.sh          la primera y la décima (instalar en el disco: la 0.7)
#   SOLO=08 ./pruebas.sh          la primera y las de la 0.8 (de la undécima a la decimocuarta)
#   SOLO=caliente ./pruebas.sh    la primera y la undécima (el USB en caliente)
#   SOLO=paquetes ./pruebas.sh    la primera y la duodécima (los paquetes: PAQUETES y tools/hpq.py)
#   SOLO=red ./pruebas.sh         la primera y la decimotercera (la red)
#   SOLO=acpi ./pruebas.sh        la primera y la decimocuarta (apagar de verdad y la batería: ACPI.BIN)
#   SOLO=tls ./pruebas.sh         la primera y la 13 bis (HTTPS: el servicio TLS, TLS.BIN)
#   SOLO=cd ./pruebas.sh          la primera y la decimosexta (el CD y las actualizaciones por Internet)
#   SOLO=pantalla ./pruebas.sh    la primera y la decimoséptima (la medida de la pantalla)
#   SOLO=navegador ./pruebas.sh   la primera y la decimoctava (el Navegador: NAVEGA.BIN)
#   SOLO=dispositivos ./pruebas.sh  la primera y la 19 (el Administrador de dispositivos) y la 19 bis
#   SOLO=informe ./pruebas.sh     la primera y la 19 bis (INFORME-DISP: «Enviar informe de los dispositivos que fallan»)
#   SOLO=ssh ./pruebas.sh         la primera y la 20 (SSH, WireGuard y Tailscale; con HEADSCALE=binario, también Tailscale)
#   SOLO=lectores ./pruebas.sh    la primera y la 21 (los CD y DVD, y las imágenes de disco)
#   SOLO=win32 ./pruebas.sh       la primera y la 22 (los programas de Windows y DirectX)
#   SOLO=rueda ./pruebas.sh       la primera y la 23 bis (la rueda del ratón en los programas)
#   SOLO=menu ./pruebas.sh        la primera y la 23 (el menú Inicio, los menús largos y los atajos)
#   SOLO=programas ./pruebas.sh   la primera y la 24 (PROGRAMAS: «Programas instalados», los tipos, reparar)
#   SOLO=protege ./pruebas.sh     la primera y la 25 (PROTEGE: la zona protegida, los secretos y los servicios por su sello)
#   SOLO=escudo ./pruebas.sh      la primera y la 26 (Escudo, el antivirus)
#   SOLO=wifi ./pruebas.sh        la primera y la 27 (el Wi-Fi: WIFI.BIN)
#   SOLO=intel ./pruebas.sh       la primera y la 54 ((INTEL) el Wi-Fi de Intel: el firmware y la 7265 contra su modelo; IWL_FW=archivo.ucode)
#   SOLO=accesorios ./pruebas.sh  la primera y la 28 (ACCESORIOS: la Calculadora, el Reloj y el Mapa de caracteres)
#   SOLO=sonido ./pruebas.sh      la primera y la 29 (el sonido: SONIDO.BIN, la bandeja, DirectSound, HVPRUN)
#   SOLO=pdf ./pruebas.sh         la primera y la 30 (Hispa-PDF: PDF.BIN y su ventana)
#   SOLO=zip ./pruebas.sh         la primera y la 31 (Comprimidos: ZIP y UNZIP; pruebas_zipvisor.sh)
#   SOLO=mirador ./pruebas.sh     la primera y la 32 (el Mirador y la imagen de tapiz; pruebas_zipvisor.sh)
#   SOLO=cortes ./pruebas.sh      la primera y la 37 ((CORTES) los cortes de la red; pruebas_cortes.sh)
#   SOLO=remota ./pruebas.sh      la primera y la 58 (REMOTA: «Dejar entrar a Claude», la consola remota; pruebas_remota.sh)
#   SOLO=usuarios ./pruebas.sh    la primera y la 38 (USUARIOS: los usuarios, la serie del catálogo, el azar; pruebas_usuarios.sh)
#   SOLO=panel ./pruebas.sh       la primera y la 34 ((PANEL) el Panel de control y la hora por Internet; pruebas_panel.sh)
#   SOLO=bloc ./pruebas.sh        la primera y la 35 (BLOC: el Bloc de notas 1.0)
#   SOLO=reproductor ./pruebas.sh la primera y la 36 (el Reproductor; pruebas_reproductor.sh; FUZZ_REPRO=n casos de fuzz)
#   SOLO=win32juegos ./pruebas.sh la primera y la 33 ((JUEGOS) lo que pide un juego de Windows: hilos, SEH, registro, VC6, ratón, UDP, sonido y CD)
#   SOLO=juegos ./pruebas.sh      la primera y la 39 (JUEGOS: los juegos; pruebas_juegos.sh)
#   SOLO=pluma ./pruebas.sh       la primera y la 41 (PLUMA32: Pluma 2; pruebas_pluma.sh)
#   SOLO=oficina ./pruebas.sh     la primera y la 40 (OFICINA2: la oficina 1.1 en HVPRUN; pruebas_oficina.sh)
#   SOLO=correo ./pruebas.sh      la primera y la 42 (CORREO: el cliente de correo; pruebas_correo.sh)
#   SOLO=termo ./pruebas.sh       la primera y la 46 (TERMO: el Termómetro y su memoria USB; pruebas_termo.sh)
#   SOLO=agenda ./pruebas.sh      la primera y la 43 (AGENDA: la Agenda; pruebas_agenda.sh)
#   SOLO=cuentas ./pruebas.sh     la primera y la 44 (CUENTAS32: Cuentas 2; pruebas_cuentas.sh)
#   SOLO=pintura ./pruebas.sh     la primera y la 45 (PINTURA32: Pintura 2; pruebas_pintura.sh)
#   SOLO=iconos ./pruebas.sh      la primera y la 47 (ICONOS: los iconos del tapiz; pruebas_iconos.sh)
#   SOLO=compat ./pruebas.sh      la primera y la 49 ((COMPAT) el Asistente de compatibilidad; pruebas_compat.sh)
#   SOLO=touchpad ./pruebas.sh    la primera y la 50 (TOUCHPAD: el touchpad; pruebas_touchpad.sh)
#   SOLO=sdhci ./pruebas.sh       la primera y la 53 (SDHCI: las tarjetas SD y el eMMC; pruebas_sdhci.sh)
#   SOLO=movil ./pruebas.sh       la primera y la 55 (MÓVIL: el móvil por USB, el anclaje de USB de Android; pruebas_movil.sh)
#   SOLO=rtl ./pruebas.sh         la primera y la 57 (RTL8723BS: el Wi-Fi por SDIO de las tabletas; pruebas_rtl.sh; RTL_FW=rtl8723bs_nic.bin)
#   SOLO=punteros ./pruebas.sh    la primera y la 51 (PUNTEROS: los punteros del ratón; pruebas_punteros.sh)
#   SOLO=bt ./pruebas.sh          la primera y la 56 (BLUETOOTH: pruebas_bt.sh)
#   SOLO=i2chid ./pruebas.sh      la primera y la 52 (I2CHID: los touchpads HID sobre I2C; pruebas_i2chid.sh)
#   SOLO=16 ./pruebas.sh          de la primera a la séptima (sin arrancar el núcleo de Hispa-Pentium)
#   SOLO=castel32 ./pruebas.sh    la primera y la decimoquinta (CASTEL32: corre32, Hispa-Pentium y HW32)
#   SOLO=castel64 ./pruebas.sh    la primera y la 23 (CASTEL64: Castel para x86-64, en Linux, y Hispa-64; pruebas64.sh)
#   SIN_ARRANQUE=1 ./pruebas.sh   sin el arranque en tres pasos
#   AZAR=3 ./pruebas.sh           además, tres programas de cuentas al azar
#                                 de 16 bits (azar.py) y otros tres de 32
#                                 (azar32.py): lo que calculan frente a lo
#                                 que tiene que dar. Unos 10 s más cada uno.
#   VER=1 ./pruebas.sh            deja lo que sale en /tmp/castel-pruebas
#   CAPTURAS_A=carpeta ./pruebas.sh   deja ahí las capturas del escritorio (PPM)
set -u
AQUI=$(dirname "$(readlink -f "$0")")
H=$AQUI/../h8086
# los programas de usuario de Hispa-Pentium (usuario.cas y llamadas.cas son módulos)
USU="$AQUI/nucleo32/uinstala.cas $AQUI/nucleo32/uasiste.cas $AQUI/nucleo32/uhola.cas $AQUI/nucleo32/umalo.cas $AQUI/nucleo32/ucli.cas $AQUI/nucleo32/ueco.cas $AQUI/nucleo32/uordenes.cas $AQUI/nucleo32/uprueba.cas $AQUI/nucleo32/uescribe.cas $AQUI/nucleo32/ubucle.cas $AQUI/nucleo32/ubloc.cas $AQUI/nucleo32/uconsola.cas $AQUI/nucleo32/uequipo.cas $AQUI/nucleo32/udisposi.cas $AQUI/nucleo32/upaquete.cas $AQUI/nucleo32/uactual.cas $AQUI/nucleo32/ucuentas.cas"
# y los de la red (la 0.8): el servicio, RED.BIN, y sus programas (con sus
# módulos: rbase, rtarjeta, ryukon, rpila, rtcp y redes); van en su tanda
RED_USU="$AQUI/nucleo32/ured.cas $AQUI/nucleo32/uipconf.cas $AQUI/nucleo32/uping.cas $AQUI/nucleo32/udescarg.cas $AQUI/nucleo32/uredwifi.cas"
T=$(mktemp -d)
QPID=
acaba() { [ -n "$QPID" ] && kill $QPID 2>/dev/null; [ -n "${VER:-}" ] && { rm -rf /tmp/castel-pruebas; cp -r $T /tmp/castel-pruebas; }; rm -rf $T; }
trap acaba EXIT
fail=0
nok=0
ok() { echo "  OK   $1"; nok=$((nok + 1)); }
ko() { echo "  FAIL $1"; fail=$((fail + 1)); }
# (los ayudantes de las partes 8 y 9, aquí arriba: valen para las dos)
mira() {       # lo que se prueba, y las líneas que tienen que salir (expresiones de grep)
  local que=$1 l; shift
  for l in "$@"; do
    if ! grep -q -- "$l" $S; then ko "$que: falta «$l»"; sed 's/^/        /' $S | grep -v '^ *$' | head -60 | tail -n 20; return; fi
  done
  ok "$que"
}
# ¿está sano el volumen de esa imagen (según fsck.fat)? Con un segundo argumento, el volumen empieza en ese sector
sano() {
  local v=$1
  if [ -n "${2:-}" ]; then v=$P/vol.img; dd if=$1 of=$v bs=512 skip=$2 status=none; fi
  fsck.fat -n $v > $P/fsck.txt 2>&1
}
# un archivo de la imagen, a un archivo de aquí (falla si no está)
saca() { rm -f $3; mcopy -n -i $1 "::$2" $3 > /dev/null 2>&1; }
cap() { python3 -I $AQUI/nucleo32/mira_captura.py "$@" > $G/cap.txt 2>&1; }
miracap() {    # lo que se prueba, la captura y sus comprobaciones
  local que=$1 c=$2; shift 2
  if [ -f $G/$c.ppm ] && cap $G/$c.ppm "$@"; then ok "$que"; else ko "$que:"; sed 's/^/        /' $G/cap.txt | head -6; [ -f $G/$c.ppm ] || echo "        (no hay captura $c)"; fi
}

. $AQUI/pruebas32.sh               # la parte 15 (CASTEL32), como la función parte15
. $AQUI/pruebas_zipvisor.sh        # (ZIP) (MIRADOR) las partes 31 y 32, como parte_zip y parte_mirador
. $AQUI/pruebas_usuarios.sh        # (USUARIOS) la parte 38, como parte_usuarios
. $AQUI/pruebas_remota.sh          # (REMOTA) la parte 58, como parte_remota
. $AQUI/pruebas_panel.sh           # (PANEL) la parte 34, como parte_panel
. $AQUI/pruebas_reproductor.sh     # (REPRODUCTOR) la parte 36, como parte_reproductor
. $AQUI/pruebas_juegos.sh          # (JUEGOS2) la parte 39, como parte_juegos
. $AQUI/pruebas_pluma.sh           # (PLUMA32) la parte 41, como parte_pluma
. $AQUI/pruebas_oficina.sh         # (OFICINA2) la parte 40, como parte_oficina
. $AQUI/pruebas_correo.sh          # (CORREO) la parte 42, como parte_correo
. $AQUI/pruebas_termo.sh           # (TERMO) la parte 46, como parte_termo
. $AQUI/pruebas_informe.sh         # (INFORME-DISP) la parte 19 bis, como informe_disp
. $AQUI/pruebas_uefi.sh            # (UEFI) la parte 48, como parte_uefi
. $AQUI/pruebas_agenda.sh          # (AGENDA) la parte 43, como parte_agenda
. $AQUI/pruebas_cuentas.sh         # (CUENTAS32) la parte 44, como parte_cuentas
. $AQUI/pruebas_pintura.sh         # (PINTURA32) la parte 45, como parte_pintura
. $AQUI/pruebas_cortes.sh          # (CORTES) la parte 37, como parte_cortes
. $AQUI/pruebas_iconos.sh          # (ICONOS) la parte 47, como parte_iconos
. $AQUI/pruebas_compat.sh          # (COMPAT) la parte 49, como parte_compat
. $AQUI/pruebas_touchpad.sh        # (TOUCHPAD) la parte 50, como parte_touchpad
. $AQUI/pruebas_sdhci.sh           # (SDHCI) la parte 53, como parte_sdhci
. $AQUI/pruebas_rtl.sh             # (RTL8723BS) la parte 57, como parte_rtl
. $AQUI/pruebas_punteros.sh        # (PUNTEROS) la parte 51, como parte_punteros
. $AQUI/pruebas_i2chid.sh          # (I2CHID) la parte 52, como parte_i2chid
. $AQUI/pruebas_movil.sh           # (MÓVIL) la parte 55, como parte_movil
. $AQUI/pruebas_bt.sh              # (BLUETOOTH) la parte 56, como parte_bt
. $AQUI/pruebas64.sh               # [CASTEL64] la parte 23 (CASTEL64), como la función parte23

# ---------------------------------------------------------------- 1. errores
echo "== Castel: los errores se dicen en español, con línea y columna =="
for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas $AQUI/ejemplos64/errores/*.cas; do
  n=$(basename $f .cas)
  esperado=$(sed -n 's/^; ERROR: //p' $f | head -1)
  if python3 -I $AQUI/ccastel.py $f -o $T/err.asm -I $AQUI/ejemplos/partes > $T/err.txt 2>&1; then
    ko "$n: compila, y tenía que dar un error"
    continue
  fi
  dicho=$(head -1 $T/err.txt | sed 's/^.*\.cas://')
  if [ "$dicho" = "$esperado" ]; then ok "$n: $dicho"; else ko "$n: dice «$dicho» y se esperaba «$esperado»"; fi
done
# la versión que dicen el menú Inicio, Configuración y «Acerca de»
# (usuario.cas, VERSIÓN_TEXTO) es la del paquete «base» de tools/hpq.py
VERSION_BASE=$(python3 -I -c "import sys; sys.path.insert(0, '$AQUI/../tools'); import hpq; print([v for n, v, *_ in hpq.SISTEMA if n == 'base'][0])")
if grep -q "^    público constante VERSIÓN_TEXTO = \"$VERSION_BASE\"$" $AQUI/nucleo32/usuario.cas; then
  ok "la versión del escritorio (VERSIÓN_TEXTO) es la de la base de hpq.py: $VERSION_BASE"
else ko "VERSIÓN_TEXTO de usuario.cas no es la $VERSION_BASE de la base de hpq.py"; fi
# SOLO=caliente, paquetes, red o acpi: la primera parte y solo esa de la 0.8;
# SOLO=08, la primera y todas las de la 0.8 (como SOLO=32, que las lleva también)
case "${SOLO:-}" in caliente|paquetes|red|acpi|tls|cd|pantalla|navegador|dispositivos|informe|ssh|lectores|win32|rueda|menu|programas|protege|escudo|wifi|intel|accesorios|sonido|pdf|zip|mirador|usuarios|remota|panel|bloc|reproductor|win32juegos|juegos|pluma|oficina|correo|agenda|cuentas|pintura|cortes|iconos|uefi|termo|compat|touchpad|punteros|i2chid|sdhci|movil|bt|rtl) SOLO08=$SOLO; SOLO=08 ;; *) SOLO08= ;; esac   # (TERMO) (UEFI) (COMPAT) (TOUCHPAD) (PUNTEROS) (I2CHID) (INTELWIFI) (SDHCI)
parte08() { case "${SOLO:-}" in ""|32) return 0 ;; 08) [ -z "$SOLO08" ] || [ "$SOLO08" = $1 ] ;; *) return 1 ;; esac; }

if [ "${SOLO:-}" = errores ]; then
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi
if [ "${SOLO:-}" = castel32 ]; then
  parte15
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi
if [ "${SOLO:-}" = castel64 ]; then
  parte23
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi

# SOLO=32: solo la primera parte, la octava y la novena (los 32 bits);
# SOLO=06: solo la primera y la novena (lo nuevo de Hispa-Pentium 0.6)
if [ "${SOLO:-}" != 32 ] && [ "${SOLO:-}" != 06 ] && [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
# -------------------------------------------------------- 2. compilar todo
echo "== Castel: los ejemplos compilan =="
EJEMPLOS=
for f in $AQUI/ejemplos/*.cas; do
  n=$(basename $f .cas)
  N=$(echo $n | tr a-z A-Z)
  [ $n = mas ] && N=MASC               # MAS.COM es la de ensamblador
  X=COM
  grep -q '^ *modelo grande' $f && X=EXE   # el «modelo grande» da un .EXE
  if $AQUI/compilar.sh $f $T/$N.$X -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    ok "$n.cas -> $N.$X ($(stat -c %s $T/$N.$X) octetos)"
    # MAS, CONTAR y NUMERA llevan argumentos y sus pruebas aparte
    [ $n = mas ] || [ $n = contar ] || [ $n = numera ] || EJEMPLOS="$EJEMPLOS $n"
  else
    ko "$n.cas no compila: $(head -1 $T/comp.txt)"
  fi
done

if [ -f $T/modulos.asm ] && ! grep -q "F_Figuras__sin_usar" $T/modulos.asm && grep -q "F_Figuras__cuadrado" $T/modulos.asm; then
  ok "de un módulo solo va al programa lo que se usa"
else ko "de un módulo solo va al programa lo que se usa"; fi

# el disquete: el de Hispa-8086 con los programas, los textos y un AUTOEXEC
# que lo ejecuta todo con la salida en archivos .SAL
[ -f $H/build/hispa8086-1440.img ] || make -C $H > /dev/null 2>&1
[ -f $H/build/hispa8086-1440.img ] || { echo "No hay disquete de Hispa-8086 (make en h8086)"; exit 2; }
cp $H/build/hispa8086-1440.img $T/a.img
python3 - $T $EJEMPLOS <<'PY'
import sys
t = sys.argv[1]
# CORTO.TXT: cabe en una pantalla. Tabuladores, acentos (página 437), una
# línea sin retorno de carro y otra sin fin de línea al final
corto = ("Primera línea\r\n\tcon un tabulador\r\nuno\tdos\ttres\tcuatro\r\n"
         "1234567\ta\r\n12345678\tb\r\nsolo salto de línea\nñandú, pingüino, ¿qué?\r\n"
         "sin fin de línea").encode('cp437')
open(t + '/CORTO.TXT', 'wb').write(corto)
# LARGO.TXT: tres pausas con 24 líneas de pantalla. Líneas de justo 80 letras
# y de más, tabuladores que llegan al borde, más de un trozo de 512 octetos
# y, al final, el fin de archivo de los textos (1Ah) con algo detrás
ln = []
for i in range(1, 76):
    if i == 7:
        ln.append("x" * 80)
    elif i == 13:
        ln.append("".join(str(j % 10) for j in range(100)))
    elif i == 19:
        ln.append("a\tb\t\tc" + "\t" * 9 + "tras el borde")
    elif i == 31:
        ln.append("y" * 79)
    elif i == 40:
        ln.append("z" * 73 + "\ttab al borde")
    elif i == 45:
        ln.append("")
    else:
        ln.append(f"Línea {i:02d} de la prueba de MAS:\t{'=' * (i % 30)}")
largo = ("\r\n".join(ln) + "\r\nfin del texto\r\n").encode('cp437') + b"\x1aNO DEBE SALIR\r\n"
open(t + '/LARGO.TXT', 'wb').write(largo)
# GRANDE.TXT: más de 65535 octetos, para las cuentas de 32 bits de CONTAR
grande = "".join(f"renglón {i} del archivo grande, que pasa de 65535 octetos\r\n"
                 for i in range(1, 1501)).encode('cp437')
open(t + '/GRANDE.TXT', 'wb').write(grande)
# lo que tiene que decir CONTAR de cada uno (y el total): líneas, palabras y
# octetos, en columnas de 8
def cuenta(d):
    return (d.count(b'\n'), len(d.split()), len(d))
def renglon(c, nombre):
    return "%8d%8d%8d" % c + (" " + nombre if nombre else "") + "\n"
cl, cc, cg = cuenta(largo), cuenta(corto), cuenta(grande)
assert cg[2] > 65535
open(t + '/contar1.esp', 'w').write(
    renglon(cl, 'LARGO.TXT') + renglon(cc, 'CORTO.TXT') + "CONTAR: no puedo abrir NOHAY.TXT\n"
    + renglon(cg, 'GRANDE.TXT') + renglon(tuple(a + b + c for a, b, c in zip(cl, cc, cg)), 'total'))
open(t + '/contar2.esp', 'w').write(renglon(cc, 'CORTO.TXT'))
open(t + '/contar3.esp', 'w').write(renglon(cc, ''))
# (por la tubería llega lo que escribe TYPE, que acaba siempre con un fin de línea suyo)
open(t + '/contar4.esp', 'w').write(renglon(cuenta(grande + b'\r\n'), ''))
# y lo que tiene que escribir NUMERA: cada línea con su número delante, hasta
# el fin de archivo de los textos (1Ah)
def numera(d):
    d = d.split(b'\x1a')[0]
    sal = b''
    for i, ln in enumerate(d.splitlines(keepends=True)):
        sal += b"%5d: " % (i + 1) + ln
    return sal
assert b'\r' not in corto.replace(b'\r\n', b'') and b'\r' not in largo.replace(b'\r\n', b'')
open(t + '/numera1.esp', 'wb').write(numera(largo))
open(t + '/numera2.esp', 'wb').write(numera(corto))
# cuántas pausas tiene que haber (la misma cuenta que hace MAS)
col = lin = pausas = 0
for c in largo:
    if c == 0x1A:
        break
    nueva = False
    if c == 9:
        while True:
            col += 1
            if col >= 80:
                col = 0
                nueva = True
                break
            if col % 8 == 0:
                break
    elif c == 10:
        nueva = True
    elif c == 13:
        col = 0
    elif c >= 32:
        col += 1
        if col >= 80:
            col = 0
            nueva = True
    if nueva:
        lin += 1
        if lin >= 24:
            lin = 0
            pausas += 1
assert pausas == 3, pausas
lote = ["@ECO OFF"]
for n in sys.argv[2:]:
    N = n.upper()
    lote.append(f"{N} > {N}.SAL")
    if N == "NIVEL":
        lote += ["IF ERRORLEVEL 7 ECO siete > NIV7.SAL", "IF ERRORLEVEL 8 ECO ocho > NIV8.SAL"]
lote += [
    "CONTAR LARGO.TXT CORTO.TXT NOHAY.TXT GRANDE.TXT > CONTAR1.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR1N.SAL",
    "CONTAR CORTO.TXT > CONTAR2.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR2N.SAL",
    "CONTAR < CORTO.TXT > CONTAR3.SAL",
    "TYPE GRANDE.TXT | CONTAR > CONTAR4.SAL",
    "NUMERA LARGO.TXT > NUMERA1.SAL",
    "NUMERA < CORTO.TXT > NUMERA2.SAL",
    "NUMERA NOHAY.TXT > NUMERA3.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > NUMERA3N.SAL",
]
for prog, l in (("MAS", "M"), ("MASC", "C")):
    lote += [
        "LIMPIAR",
        f"{prog} CORTO.TXT > {l}1.SAL",
        f"{prog} < CORTO.TXT > {l}2.SAL",
        f"TYPE CORTO.TXT | {prog} > {l}3.SAL",
        f"{prog} NOHAY.TXT > {l}4.SAL",
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}4N.SAL",
        f"{prog}    CORTO.TXT   > {l}5.SAL",
        f"{prog} LARGO.TXT > {l}6.SAL",           # pausas 1 a 3
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausas 4 a 6, en la pantalla
        "LIMPIAR",
        f"TYPE LARGO.TXT | {prog}",               # pausa 7 y pausa 8: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}7N.SAL",
        f"{prog} CORTO.TXT",
        f"{prog} < LARGO.TXT > {l}8.SAL",         # pausas 9 a 11
        f"{prog} NOHAY.TXT",
        f"{prog} LARGO.TXT > {l}9.SAL",           # pausas 12 a 14
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausa 15: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}10N.SAL",
    ]
lote += ["LIMPIAR", "ECO [castel-fin]"]
open(t + '/AUTOEXEC.BAT', 'wb').write(("\r\n".join(lote) + "\r\n").encode('cp437'))
PY
[ $? = 0 ] || { echo "No se han podido preparar los textos de prueba"; exit 2; }
mcopy -o -i $T/a.img $T/*.COM $T/*.EXE $T/CORTO.TXT $T/LARGO.TXT $T/GRANDE.TXT $T/AUTOEXEC.BAT ::
POR_PROG=15         # pausas de cada una de las dos MAS
ESC="8 15"          # las que se contestan con Esc

# ------------------------------------------------------------ QEMU
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.3
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
echo "== Castel: los ejemplos, dentro de Hispa-8086 (QEMU) =="
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
np=0
hecho=0
for ((i = 0; i < 400; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  if grep -qF -- '-- Más --' $T/p.txt; then
    np=$((np + 1))
    cp $T/p.txt $T/pausa_$np.txt
    tecla=spc
    for e in $ESC; do
      if [ $np = $e ] || [ $np = $((e + POR_PROG)) ]; then tecla=esc; fi
    done
    mon "sendkey $tecla" 0.6
  else
    sleep 0.4
  fi
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
if [ $hecho = 0 ]; then
  ko "el lote no ha llegado al final (pantalla en $T/p.txt):"
  sed 's/^/        /' $T/p.txt | grep -v '^ *$' | tail -n 8
fi
mkdir $T/sal
mcopy -n -i $T/a.img '::*.SAL' $T/sal/ > /dev/null 2>&1

for n in $EJEMPLOS; do
  N=$(echo $n | tr a-z A-Z)
  if [ ! -f $T/sal/$N.SAL ]; then ko "$n: no ha dejado salida"; continue; fi
  tr -d '\r' < $T/sal/$N.SAL | iconv -f CP437 -t UTF-8 > $T/sal/$n.txt
  if cmp -s $T/sal/$n.txt $AQUI/ejemplos/esperado/$n.txt; then
    ok "$n: la salida es la esperada"
  else
    ko "$n: la salida no es la esperada (< esperada, > la que sale):"
    diff $AQUI/ejemplos/esperado/$n.txt $T/sal/$n.txt | sed 's/^/        /' | head -12
  fi
done

if [ -s $T/sal/NIV7.SAL ] && [ ! -s $T/sal/NIV8.SAL ]; then ok "nivel: lo que devuelve principal() es el nivel de error (7)"
else ko "nivel: el programa no ha acabado con el nivel de error 7"; fi

# ---------------------------------------------------------------- 3. MAS
echo "== MAS en Castel frente a MAS en ensamblador =="
echo "  (MAS.COM en ensamblador: $(stat -c %s $H/build/MAS.COM) octetos; en Castel: $(stat -c %s $T/MASC.COM))"
compara() {    # número de la salida y lo que se prueba
  if [ ! -s $T/sal/M$1.SAL ]; then ko "$2: la de ensamblador no ha escrito nada"
  elif cmp -s $T/sal/M$1.SAL $T/sal/C$1.SAL; then ok "$2: la misma salida ($(stat -c %s $T/sal/C$1.SAL) octetos)"
  else ko "$2: la salida es distinta"; cmp $T/sal/M$1.SAL $T/sal/C$1.SAL 2>&1 | sed 's/^/        /'; fi
}
compara 1 "MAS archivo"
compara 2 "MAS < archivo"
compara 3 "orden | MAS"
compara 4 "MAS con un archivo que no está («No existe ese archivo.»)"
compara 4N "y acaba con el nivel de error 1"
compara 5 "MAS con espacios antes y después del nombre"
compara 6 "MAS archivo largo > salida (tres pausas)"
compara 8 "MAS < archivo largo > salida"
compara 9 "MAS archivo largo > salida, tras el aviso de que no está"
if [ -s $T/sal/M10N.SAL ] || [ -s $T/sal/C10N.SAL ]; then ko "tras Esc, el nivel de error es 0"; else ok "tras Esc, las dos acaban con el nivel de error 0"; fi
if [ -s $T/sal/M7N.SAL ] && [ -s $T/sal/C7N.SAL ]; then ok "tras Esc en una tubería, el mismo nivel de error (1 o más)"
elif [ -s $T/sal/M7N.SAL ] || [ -s $T/sal/C7N.SAL ]; then ko "tras Esc en una tubería, el nivel de error es distinto"
else ok "tras Esc en una tubería, el mismo nivel de error (0)"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q "NO DEBE SALIR"; then ko "lo que hay tras el 1Ah no se escribe"
else ok "lo que hay tras el fin de archivo (1Ah) no se escribe"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q -- "-- Más --"; then ko "el aviso no va a la salida"
else ok "el aviso «-- Más --» no acaba en el archivo de salida"; fi
if [ $np = $((2 * POR_PROG)) ]; then ok "las mismas pausas: $POR_PROG cada una"; else ko "pausas: $np, y tenían que ser $((2 * POR_PROG))"; fi
iguales=1
for ((i = 1; i <= POR_PROG; i++)); do
  if ! cmp -s $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt; then
    iguales=0
    ko "la pantalla en la pausa $i es distinta:"
    diff $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt 2>&1 | sed 's/^/        /' | head -8
  fi
done
[ $iguales = 1 ] && ok "la pantalla es la misma en las $POR_PROG pausas (también tras Esc)"
if [ -f $T/pausa_4.txt ] && grep -q "Línea 1[0-9] de la prueba" $T/pausa_4.txt && grep -qF -- "-- Más --" $T/pausa_4.txt; then
  ok "en la pantalla: el texto y, debajo, «-- Más --»"
else ko "en la pantalla no está el texto con el aviso"; fi

# ---------------------------------------------------------------- 4. CONTAR
echo "== CONTAR y NUMERA: programas con módulos, biblioteca y 32 bits =="
contar() {     # número de la salida, la esperada y lo que se prueba
  if [ ! -f $T/sal/CONTAR$1.SAL ]; then ko "$3: no ha dejado salida"; return; fi
  tr -d '\r' < $T/sal/CONTAR$1.SAL | iconv -f CP437 -t UTF-8 > $T/sal/contar$1.txt
  if cmp -s $T/sal/contar$1.txt $T/contar$2.esp; then ok "$3"
  else ko "$3 (< esperado, > lo que sale):"; diff $T/contar$2.esp $T/sal/contar$1.txt | sed 's/^/        /' | head -8; fi
}
contar 1 1 "CONTAR de cuatro archivos (uno no está; otro pasa de 65535 octetos) y el total"
if [ -s $T/sal/CONTAR1N.SAL ]; then ok "y acaba con el nivel de error 1"; else ko "CONTAR no ha acabado con el nivel de error 1"; fi
contar 2 2 "CONTAR de un archivo"
if [ -s $T/sal/CONTAR2N.SAL ]; then ko "CONTAR de un archivo que está acaba con nivel de error"; else ok "y acaba con el nivel de error 0"; fi
contar 3 3 "CONTAR < archivo"
contar 4 4 "orden | CONTAR (con el archivo de más de 65535 octetos)"
numera() {     # número de la salida y lo que se prueba
  if [ ! -f $T/sal/NUMERA$1.SAL ]; then ko "$2: no ha dejado salida"
  elif cmp -s $T/sal/NUMERA$1.SAL $T/numera$1.esp; then ok "$2 ($(stat -c %s $T/sal/NUMERA$1.SAL) octetos, los mismos)"
  else ko "$2: no sale lo esperado"; cmp $T/numera$1.esp $T/sal/NUMERA$1.SAL 2>&1 | sed 's/^/        /'; fi
}
numera 1 "NUMERA de un archivo de 76 líneas (para en el 1Ah)"
numera 2 "NUMERA < archivo"
if [ -s $T/sal/NUMERA3N.SAL ] && grep -q "no puedo abrir NOHAY.TXT" $T/sal/NUMERA3.SAL; then ok "NUMERA de uno que no está: lo dice y acaba con el nivel de error 1"
else ko "NUMERA de uno que no está: no lo dice o no acaba con el nivel de error 1"; fi

# ---------------------------------------------------- 5. el ensamblador propio
echo "== El ensamblador propio: los mismos octetos que NASM =="
mkdir $T/ens $T/ens/nasm
cp $T/*.asm $T/ens/
cp $AQUI/ensamblador/formas.asm $T/ens/formas.asm
# y los de 32 bits: las formas de 32, los ejemplos de ejemplos32, la semilla
# de Hispa-Pentium y unas cuentas al azar (CASTEL32.md)
cp $AQUI/ensamblador/formas32.asm $T/ens/formas32.asm
python3 -I $AQUI/azar32.py 9 150 $T/ens/az32p.cas $T/ens/az32p.esp plano
for f in $AQUI/ejemplos32/*.cas $AQUI/nucleo32/semilla.cas $USU $RED_USU $T/ens/az32p.cas; do
  python3 -I $AQUI/ccastel.py $f -o $T/ens/$(basename $f .cas).asm > /dev/null 2>&1 || ko "$(basename $f) no compila"
done
rm -f $T/ens/az32p.cas $T/ens/az32p.esp
# la biblioteca entera (también lo que nadie usa) y el propio ensamblador
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $T/ens/toda.cas
python3 -I $AQUI/ccastel.py --todo $T/ens/toda.cas -o $T/ens/toda.asm > /dev/null 2>&1 || ko "la biblioteca entera no compila"
python3 -I $AQUI/ccastel.py $AQUI/ensamblador/ensambla.cas -o $T/ens/ensambla.asm > /dev/null 2>&1 || ko "ensambla.cas no compila"
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  nasm -f bin $f -o $T/ens/nasm/$n.bin 2> /dev/null
  python3 -I $AQUI/ensambla.py $f -o $T/ens/$n.py.bin 2> /dev/null
  if cmp -s $T/ens/nasm/$n.bin $T/ens/$n.py.bin; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ensambla.py: los $iguales archivos (ejemplos de 16 y de 32 bits, biblioteca entera, formas y ENSAMBLA), iguales que con NASM"
else ko "ensambla.py no da lo mismo que NASM en:$distintos"; fi
# ENSAMBLA, dentro de Hispa-8086: los ensambla todos (también a sí mismo),
# menos el núcleo de Hispa-Pentium y sus programas: esos, desde el 20/11, los
# ensambla ENSAMB32 dentro de Hispa-Pentium (la parte 15); el ENSAMBLA de 16
# bits ya no manda en el núcleo (CASTEL32.md §11)
NUC=" $(for f in $USU; do basename $f .cas; done | tr '\n' ' ')"
NRED=" $(for f in $RED_USU; do basename $f .cas; done | tr '\n' ' ')"
# en qué tanda va cada uno: 2, la semilla; 3, sus programas; 4, los de la red; 1, lo demás
tanda_de() { case "$1" in semilla) echo 2 ;; *) case "$NUC" in *" $1 "*) echo 3 ;; *) case "$NRED" in *" $1 "*) echo 4 ;; *) echo 1 ;; esac ;; esac ;; esac; }
mkdir $T/ens/sal
for tanda in 1; do
cp $H/build/hispa8086-1440.img $T/e.img
{ printf '@ECO OFF\r\n'
  for f in $T/ens/*.asm; do
    n=$(basename $f .asm)
    [ "$(tanda_de $n)" = $tanda ] || continue
    N=$(echo $n | tr a-z A-Z)
    mcopy -o -i $T/e.img $f ::$N.ASM || ko "ENSAMBLA: $N.ASM no cabe en el disquete $tanda"
    printf 'ENSAMBLA %s.ASM %s.BIN > %s.SAL\r\n' $N $N $N
  done
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/e.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
mcopy -o -i $T/e.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/e.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "ENSAMBLA: el lote $tanda no ha llegado al final"
mcopy -n -i $T/e.img '::*.BIN' $T/ens/sal/ > /dev/null 2>&1
done
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  N=$(echo $n | tr a-z A-Z)
  [ "$(tanda_de $n)" = 1 ] || continue
  if cmp -s $T/ens/nasm/$n.bin $T/ens/sal/$N.BIN; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ENSAMBLA (en Castel, dentro de Hispa-8086): los $iguales archivos, iguales que con NASM (él mismo incluido)"
else ko "ENSAMBLA no da lo mismo que NASM en:$distintos"; fi

# ------------------------------------------------ 6. el compilador en Castel
echo "== El compilador escrito en Castel, dentro de Hispa-8086 =="
A=$T/aut
mkdir $A $A/ref $A/sal
# paso 1: ccastel.py compila el compilador, y NASM lo ensambla
if python3 -I $AQUI/ccastel.py $AQUI/compilador/castel.cas -o $A/c1.asm > $T/comp.txt 2>&1 && nasm -f bin $A/c1.asm -o $A/CASTEL.EXE 2>> $T/comp.txt; then
  ok "paso 1: ccastel.py compila el compilador (castel.cas -> CASTEL.EXE, $(stat -c %s $A/CASTEL.EXE) octetos)"
else ko "el compilador en Castel no compila: $(head -1 $T/comp.txt)"; fi
# dos disquetes: en A: la biblioteca en \BIB (la de 32 bits en \BIB32), los
# módulos de ejemplo en \PARTES, los de la semilla de Hispa-Pentium,
# los ejemplos, la biblioteca entera (--todo), ENSAMBLA, los errores y el
# temporal de CASTEL; en B:, lo que sale (el ensamblador de cada uno son, todos
# juntos, más de un disquete si va con los fuentes: el de ENSAMBLA solo pasa de
# 250 K)
python3 -I $AQUI/azar.py 7 45 $A/azar16.cas $A/azar16.esp
python3 -I $AQUI/azar32.py 7 28 $A/azar32.cas $A/azar32.esp
python3 -I $AQUI/azar32.py 8 40 $A/azar32p.cas $A/azar32p.esp plano
# (en tres tandas, cada una con sus disquetes: en la primera, los ejemplos, la
# biblioteca, ENSAMBLA, las cuentas al azar y los errores; en la segunda y la
# tercera, lo de Hispa-Pentium, que con el entorno gráfico ya no cabe con lo
# demás: los fuentes del núcleo en A: y, en B:, el ensamblador de la semilla,
# de más de 600 K (la segunda), o el de sus programas de usuario (la tercera))
# (el núcleo de Hispa-Pentium y sus programas, desde el 20/11, los compila
# CASTEL32 en la parte 15: las tablas del CASTEL de 16 bits, en 640 K, ya no
# dan para él; las tandas 2, 3 y 4 de antes eran eso)
for tanda in 1; do
cp $H/build/hispa8086-1440.img $T/c.img
mmd -i $T/c.img ::BIB ::PARTES ::BIB32
for f in $AQUI/biblioteca32/*.cas; do mcopy -i $T/c.img $f ::BIB32/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
if [ $tanda = 1 ]; then
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/c.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/ejemplos/partes/*.cas; do mcopy -i $T/c.img $f ::PARTES/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  mcopy -i $T/c.img $AQUI/ensamblador/montador.cas ::MONTADOR.CAS
  LOTE="$(echo $AQUI/ejemplos/*.cas) $AQUI/ensamblador/ensambla.cas $T/ens/toda.cas $A/azar16.cas $A/azar32.cas $(echo $AQUI/ejemplos32/*.cas) $A/azar32p.cas"
else
  # (en la segunda, los módulos del núcleo; en la tercera, los programas y
  # los dos módulos que usan: todos juntos ya no caben con lo demás)
  for f in $AQUI/nucleo32/*.cas; do
    case "$(basename $f .cas)" in
      ured|uipconf|uping|udescarg|rbase|rtarjeta|ryukon|rpila|rtcp|redes) [ $tanda = 4 ] || continue ;;
      llamadas) [ $tanda = 3 ] || [ $tanda = 4 ] || continue ;;
      uhci|unidades|usb|usbalm|usbbase|usbcal|usbhid|usuario) [ $tanda = 2 ] || continue ;;
      u[a-z]*|barramenu|sha256|firma|clave) [ $tanda = 3 ] || continue ;;
      *) [ $tanda = 2 ] || continue ;;
    esac
    mcopy -i $T/c.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS
  done
  if [ $tanda = 2 ]; then LOTE="$AQUI/nucleo32/semilla.cas"; elif [ $tanda = 3 ]; then LOTE="$USU"; else LOTE="$RED_USU"; fi
fi
mcopy -i $T/c.img $A/CASTEL.EXE ::
# (lo de Hispa-Pentium, con B: de 2,88 MB y desde B:: ahí deja CASTEL su
# archivo temporal, que con el núcleo de la 0.6 —900 K de ensamblador— ya no
# cabe en A: con los fuentes, ni con la salida en un disquete de 1,44)
if [ $tanda = 1 ]; then mformat -C -f 1440 -i $T/cb.img ::; else mformat -C -f 2880 -i $T/cb.img ::; fi
{ printf '@ECO OFF\r\n'
  [ $tanda = 1 ] || printf 'B:\r\n'
  for f in $LOTE; do
    n=$(basename $f .cas); N=$(echo $n | tr a-z A-Z)
    todo=; [ $n = toda ] && todo=--todo
    mcopy -o -i $T/c.img $f ::$N.CAS
    python3 -I $AQUI/ccastel.py $todo $f -o $A/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
    if [ $tanda = 1 ]; then printf 'CASTEL %s.CAS -o B:%s.ASM -I BIB -I PARTES -I BIB32 %s > B:%s.SAL\r\n' $N $N "$todo" $N
    else printf 'A:\\CASTEL A:\\%s.CAS -o B:%s.ASM -I A:\\BIB32 > B:%s.SAL\r\n' $N $N $N; fi
  done
  [ $tanda = 1 ] || printf 'A:\r\n'
  if [ $tanda = 1 ]; then
    i=0
    for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
      i=$((i + 1)); N=$(printf 'E%02d' $i)
      mcopy -i $T/c.img $f ::$N.CAS
      echo "$N $(basename $f .cas)" >> $A/errores.txt
      printf 'CASTEL %s.CAS -I BIB -I PARTES > B:%s.SAL\r\n' $N $N
    done
  fi
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/c.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/c.img,format=raw,if=floppy,index=0 -drive file=$T/cb.img,format=raw,if=floppy,index=1 \
  -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "CASTEL: el lote $tanda no ha llegado al final"
mcopy -n -i $T/cb.img '::*.ASM' '::*.SAL' $A/sal/ > /dev/null 2>&1
done
iguales=0
distintos=
for f in $A/ref/*.ASM; do
  N=$(basename $f .ASM)
  if cmp -s $f $A/sal/$N.ASM; then iguales=$((iguales + 1)); else distintos="$distintos $N"; fi
done
if [ -z "$distintos" ]; then ok "CASTEL: el mismo ensamblador que ccastel.py en los $iguales programas (ejemplos de 16 y de 32 bits, biblioteca entera, ENSAMBLA y tres de cuentas al azar)"
else ko "CASTEL no saca el mismo ensamblador que ccastel.py en:$distintos"; fi
iguales=0
distintos=
while read N n; do
  esperado=$(sed -n 's/^; ERROR: //p' $AQUI/ejemplos/errores/$n.cas $AQUI/ejemplos32/errores/$n.cas 2> /dev/null | head -1)
  dicho=$(tr -d '\r' < $A/sal/$N.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
  if [ "$dicho" = "$esperado" ]; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done < $A/errores.txt
if [ -z "$distintos" ]; then ok "CASTEL: los $iguales errores, dichos igual que ccastel.py (con su línea y su columna)"
else ko "CASTEL no dice igual que ccastel.py los errores de:$distintos"; fi

# El arranque en tres pasos. Cada paso, en un Hispa-8086 con dos disquetes: en
# A: el sistema, el compilador que toca, ENSAMBLA y los fuentes (y ahí deja
# CASTEL su archivo temporal); en B:, lo que sale (el ensamblador del
# compilador son 700 K: no cabe todo en uno). Compilar y ensamblar van en dos
# arranques seguidos con el mismo B: (en Hispa-8086, después de que CASTEL
# compile, leer archivos grandes en ese mismo arranque va decenas de veces
# más despacio: el ensamblado tardaría cuatro minutos en vez de tres segundos).
arranca_ab() {     # las órdenes del AUTOEXEC (con A: en $T/d.img y B: en $T/b.img)
  printf '@ECO OFF\r\n%s\r\nECO [castel-fin]\r\n' "$1" > $T/AUTOEXEC.BAT
  mcopy -o -i $T/d.img $T/AUTOEXEC.BAT ::
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
    -drive file=$T/d.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
    -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 4
  hecho=0
  for ((i = 0; i < 600; i++)); do
    pantalla p
    if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
    sleep 1
  done
  mon quit 0.5
  wait $QPID 2>/dev/null
  QPID=
  [ $hecho = 1 ]
}
paso() {       # número del paso, el compilador (un .EXE) y cómo se llama en el disquete
  cp $H/build/hispa8086-1440.img $T/d.img
  mmd -i $T/d.img ::BIB
  mcopy -i $T/d.img $2 ::$3.EXE
  mcopy -i $T/d.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
  for f in $AQUI/compilador/*.cas; do mcopy -i $T/d.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/d.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  # [CASTEL64] B: de 2,88 MB, y se compila desde él: ahí deja CASTEL su archivo
  # temporal (con la parte de atrás de 64 bits, los fuentes del compilador y el
  # temporal ya no caben juntos en A:)
  rm -f $T/b.img; mformat -C -f 2880 -i $T/b.img ::
  arranca_ab "B:"$'\r\n'"A:\\$3 A:\\CASTEL.CAS -o B:C$1.ASM -I A:\\BIB > B:P$1.SAL"$'\r\n'"A:" || ko "el paso $1 del arranque (compilar) no ha llegado al final"
  arranca_ab "ENSAMBLA B:C$1.ASM B:C$1.EXE > B:E$1.SAL" || ko "el paso $1 del arranque (ensamblar) no ha llegado al final"
  mcopy -n -i $T/b.img "::C$1.*" '::*.SAL' $A/arr/ > /dev/null 2>&1
}
if [ -z "${SIN_ARRANQUE:-}" ]; then
  echo "== El arranque en tres pasos: el compilador se compila a sí mismo =="
  mkdir $A/arr
  paso 2 $A/CASTEL.EXE CASTEL
  if [ -s $A/arr/C2.EXE ]; then paso 3 $A/arr/C2.EXE C2; else ko "el paso 2 no ha dejado C2.EXE: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.ASM ] && cmp -s $A/c1.asm $A/arr/C2.ASM; then ok "paso 2: CASTEL se compila a sí mismo dentro de Hispa-8086: el mismo ensamblador que el paso 1 ($(wc -l < $A/arr/C2.ASM) líneas)"
  else ko "paso 2: el ensamblador no es el del paso 1: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.EXE ] && cmp -s $A/CASTEL.EXE $A/arr/C2.EXE; then ok "paso 2: ENSAMBLA lo ensambla: el mismo CASTEL.EXE que el paso 1 con NASM ($(stat -c %s $A/arr/C2.EXE) octetos)"
  else ko "paso 2: el .EXE no es el del paso 1: $(tr -d '\r' < $A/arr/E2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.ASM ] && cmp -s $A/arr/C2.ASM $A/arr/C3.ASM; then ok "paso 3: el resultado se compila otra vez: el mismo ensamblador que el paso 2"
  else ko "paso 3: el ensamblador no es el del paso 2: $(tr -d '\r' < $A/arr/P3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.EXE ] && cmp -s $A/arr/C2.EXE $A/arr/C3.EXE; then ok "paso 3: y el mismo .EXE: el compilador se sostiene solo"
  else ko "paso 3: el .EXE no es el del paso 2: $(tr -d '\r' < $A/arr/E3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
fi

# ------------------------------------------------- 7. cuentas al azar (AZAR=n)
if [ -n "${AZAR:-}" ]; then
  echo "== Castel: cuentas al azar =="
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar32.py $s 120 $T/azar32_$s.cas $T/azar32_$s.esp
    if ! $AQUI/compilar.sh $T/azar32_$s.cas $T/AZ32_$s.COM > $T/comp.txt 2>&1; then
      ko "azar de 32 bits $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZ32_$s.COM > $T/azar32_$s.sal 2>&1
    if cmp -s $T/azar32_$s.esp $T/azar32_$s.sal; then ok "azar de 32 bits $s: $(wc -l < $T/azar32_$s.esp) resultados, todos bien"
    else ko "azar de 32 bits $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar32_$s.esp $T/azar32_$s.sal | head -6 | sed 's/^/        /'; fi
  done
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar.py $s 250 $T/azar$s.cas $T/azar$s.esp
    if ! $AQUI/compilar.sh $T/azar$s.cas $T/AZAR$s.COM > $T/comp.txt 2>&1; then
      ko "azar $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZAR$s.COM > $T/azar$s.sal 2>&1
    if cmp -s $T/azar$s.esp $T/azar$s.sal; then ok "azar $s: $(wc -l < $T/azar$s.esp) resultados, todos bien"
    else ko "azar $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar$s.esp $T/azar$s.sal | head -6 | sed 's/^/        /'; fi
  done
fi

fi   # SOLO=32

# SOLO=16: hasta aquí (lo de 16 bits, los dos compiladores y los dos ensambladores)
if [ "${SOLO:-}" = 16 ]; then
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi

# ------------------------------------------------- 8. Castel de 32 bits
echo "== Castel de 32 bits: «modelo núcleo» (QEMU -kernel) y «modelo hispa32» (HW32) =="
P=$T/p32
mkdir $P
if [ "${SOLO:-}" != 06 ] && [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
# los ejemplos de 16 bits que no tocan el DOS, compilados como núcleo de 32
# bits: tienen que escribir lo mismo
MISMOS="arreglos bucles cuenta estruct mezcla nivel pares pruebas refs saltos saludo textos modulos"
iguales=0
distintos=
for n in $MISMOS; do
  python3 - $AQUI/ejemplos/$n.cas $P/$n.cas <<'PY'
import sys, re
s = open(sys.argv[1], encoding='utf-8').read().split('\n')
for i, l in enumerate(s):
    if re.match(r'^módulo \w+', l):
        s.insert(i + 1, '    modelo núcleo')
        break
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(s))
PY
  if ! $AQUI/compilar.sh $P/$n.cas $P/$n.bin -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    distintos="$distintos $n(no compila)"; continue
  fi
  $AQUI/probar32.sh $P/$n.bin > $P/$n.sal 2>&1
  cod=$(tail -n 1 $P/$n.sal); sed -i '$d' $P/$n.sal
  esperado_c="[código 0]"; [ $n = nivel ] && esperado_c="[código 7]"
  if cmp -s $P/$n.sal $AQUI/ejemplos/esperado/$n.txt && [ "$cod" = "$esperado_c" ]; then iguales=$((iguales + 1))
  else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "los $iguales ejemplos de 16 bits sin DOS, como núcleos de 32 bits: escriben lo mismo (y nivel acaba con 7)"
else ko "como núcleos de 32 bits, no escriben lo mismo:$distintos"; fi
# los de ejemplos32: los núcleos, en QEMU; los de hispa32, con HW32 (si está el
# disquete de Hispa-DOS: make y make nucprogs en la raíz)
for f in $AQUI/ejemplos32/*.cas; do
  n=$(basename $f .cas)
  if grep -q '^ *modelo hispa32' $f; then X=EXE; else X=BIN; fi
  if ! $AQUI/compilar.sh $f $P/$n.$X > $T/comp.txt 2>&1; then ko "$n.cas no compila: $(head -1 $T/comp.txt)"; continue; fi
  if [ $X = BIN ]; then
    $AQUI/probar32.sh $P/$n.$X > $P/$n.sal 2>&1
    cod=$(tail -n 1 $P/$n.sal); sed -i '$d' $P/$n.sal
  elif [ -f $AQUI/../build/hispados.img ] && [ -f $AQUI/../build/HW32.COM ]; then
    NIVEL=42 $AQUI/probar32.sh $P/$n.$X > $P/$n.log 2>&1
    sed '/^A:\\>/,$d' $P/$n.log | sed '/^$/d' > $P/$n.sal
    cod="[código ?]"
    grep -qx '\[nivel-42\]' $P/$n.log && ! grep -qx '\[nivel-43\]' $P/$n.log && cod="[código 42]"
  else
    echo "  (sin el disquete de Hispa-DOS no se prueba $n.EXE: make y make nucprogs en la raíz)"
    continue
  fi
  esp_cod="[código $(sed -n 's/^; código de salida: //p' $f)]"
  if cmp -s $P/$n.sal $AQUI/ejemplos32/esperado/$n.txt && [ "$cod" = "$esp_cod" ]; then
    ok "$n.$X ($(stat -c %s $P/$n.$X) octetos): la salida es la esperada y acaba con el código que toca ($cod)"
  else
    ko "$n.$X: la salida no es la esperada o acaba con otro código ($cod y no $esp_cod; < esperada, > la que sale):"
    diff $AQUI/ejemplos32/esperado/$n.txt $P/$n.sal | sed 's/^/        /' | head -12
  fi
done
# cuentas al azar, con las reglas de 32 bits
for s in 1 2; do
  python3 -I $AQUI/azar32.py $s 150 $P/az$s.cas $P/az$s.esp plano
  $AQUI/compilar.sh $P/az$s.cas $P/az$s.bin > /dev/null 2>&1
  $AQUI/probar32.sh $P/az$s.bin > $P/az$s.sal 2>&1
  sed -i '$d' $P/az$s.sal
  if cmp -s $P/az$s.esp $P/az$s.sal; then ok "azar de 32 bits en el modelo plano $s: $(wc -l < $P/az$s.esp) resultados, todos bien"
  else ko "azar plano $s: hay cuentas que no salen"; diff $P/az$s.esp $P/az$s.sal | head -6 | sed 's/^/        /'; fi
done
# la semilla de Hispa-Pentium: la IDT, el reloj, el teclado y las excepciones
$AQUI/nucleo32/probar.sh "" ayuda hora :espera hora Hola teclas apagar > $P/semilla.sal 2>&1
if grep -q "^órdenes: ayuda hora memoria mapa paginas monton tareas programas ejecutar N" $P/semilla.sal && grep -q "^no conozco «Hola»" $P/semilla.sal \
    && grep -q "^\[código 5\]" $P/semilla.sal; then ok "la semilla de Hispa-Pentium arranca, lee el teclado y entiende sus órdenes"
else ko "la semilla de Hispa-Pentium no hace lo que debe:"; sed 's/^/        /' $P/semilla.sal | head -12; fi
s1=$(grep "^llevo" $P/semilla.sal | head -1 | awk '{print $2 * 100 + $5}')
s2=$(grep "^llevo" $P/semilla.sal | tail -n 1 | awk '{print $2 * 100 + $5}')
if [ -n "$s1" ] && [ -n "$s2" ] && [ $((s2 - s1)) -ge 100 ]; then ok "el reloj (IRQ 0) corre: $s1 y luego $s2 centésimas"
else ko "el reloj no corre: «$s1» y «$s2»"; fi
if grep -q "^28 teclas pulsadas" $P/semilla.sal; then ok "el teclado (IRQ 1) cuenta las teclas, mayúsculas incluidas"
else ko "el teclado no cuenta bien las teclas: $(grep teclas $P/semilla.sal | tail -n 1)"; fi
$AQUI/nucleo32/probar.sh "" cero > $P/cero.sal 2>&1
if grep -q "excepción 0: división por cero" $P/cero.sal && grep -q "^\[código 9\]" $P/cero.sal; then ok "una división por cero llega a su excepción (la 0, por la IDT)"
else ko "la división por cero no llega a la excepción"; sed 's/^/        /' $P/cero.sal | head -6; fi
# la memoria, las tareas y el anillo 3 (con los programas uhola, umalo y ucli
# como módulos de multiboot)
$AQUI/nucleo32/probar.sh "" memoria paginas toca monton estropear turnos dormir tareas programas \
  "ejecutar 0" "ejecutar 1" "ejecutar 2" memoria apagar > $P/pentium.sal 2>&1
S=$P/pentium.sal
mira "la memoria: el mapa de multiboot da los marcos de 4 K (mapa de bits)" "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 3 programas de usuario" \
  "^marcos: [0-9]* libres de 8160 "
mira "la paginación está encendida (CR0.PG), con la identidad para el núcleo" "^paginación: activa (CR0 = 0x8" "^  identidad: de 0 a 0x01FDFFFF, 8160 páginas en 8 tablas"
mira "un fallo de página en la zona a la demanda se atiende (un marco nuevo) y la escritura llega" \
  "^escribo en 0xD0000123 (fallos de página atendidos: 1) y leo 0xCAFE0001" "^otra vez (fallos de página atendidos: 0) y leo 0xCAFE0002"
mira "el montón: 3000 vueltas de reservar y liberar, sin fugas (ni de bloques ni de marcos)" "^montón: 3000 vueltas de reservar y liberar (y [0-9]* al final), 0 fallos" "^montón sin fugas"
mira "el montón ve lo que lo estropea: escribir de más, soltar dos veces, soltar lo que no es suyo" \
  "se ha escrito más allá de los 10 octetos del bloque" "^falso" "^lo arreglo y suelto: cierto" "ya estaba libre (¿se suelta dos veces?)" \
  "no es de un bloque" "^suelto el otro: cierto" "^el montón ha visto 3 fallos; revisado, 0"
t=$(grep "^turnos:" $S)
a=$(echo "$t" | awk '{print $5}'); b=$(echo "$t" | awk '{print $9}' | tr -d ';'); n=$(echo "$t" | awk '{print $13}')
if [ -n "$t" ] && [ "${a:-0}" -gt 0 ] && [ "${b:-0}" -gt 0 ] && [ "${n:-0}" -ge 50 ]; then ok "dos tareas que no ceden nunca se alternan con el reloj (multitarea apropiativa): $n veces"
else ko "las tareas no se alternan: «$t»"; fi
if [ "$(grep '^la dormilona' $S | awk '{print $3}' | tr -d '\n')" = 123 ] && grep -q "^la dormilona 2 se despierta: ha dormido 4[0-9] tics (pidió 40)" $S \
    && grep -q "^las tres han acabado" $S; then ok "dormir y esperar: tres dormilonas despiertan cada una a su hora, y se las espera"
else ko "las dormilonas no despiertan en orden:"; grep dormilona $S | sed 's/^/        /'; fi
mira "las tareas, con sus turnos" "^   1  consola       corriendo       0" "^   2  ociosa  " "cambios de tarea en"
m1=$(grep "^uhola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3, en su espacio de direcciones, escribe por la int 0x80, duerme y acaba con su código" \
  "^ejecuto uhola.bin (la tarea [0-9]*, en el anillo 3)" "^hola desde el anillo 3: soy la tarea" "^1 + 2 + ... + 1000 = 500500" \
  "^he dormido 10 tics: cierto" "^uhola.bin ha acabado con el código 3; marcos libres"
mira "la llamada al sistema no deja escribir desde la memoria del núcleo ni desde lo que no es del programa" \
  "^el núcleo no me deja enseñar su memoria: cierto" "^ni lo que no tengo: cierto"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa, su espacio de direcciones vuelve entero: los mismos marcos libres ($m1)"
else ko "el programa de usuario se deja marcos: «$m1»"; fi
mira "un programa que lee la memoria del núcleo: fallo de página de protección, se acaba él y el núcleo sigue" \
  "fallo de página en 0x00100000 (lectura, anillo 3, protección)" "^\*\*\* se acaba la tarea [0-9]* (umalo.bin)" "^umalo.bin ha acabado con el código 255"
mira "un programa que hace «cli» en el anillo 3: protección general, y se acaba" \
  "excepción 13: protección general (código 0)" "^ucli.bin ha acabado con el código 255" "^\[código 5\]"
if grep -q "no tenía que salir" $S; then ko "un programa del anillo 3 ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer, no lo han hecho"; fi
$AQUI/nucleo32/probar.sh "" fallo > $P/fallo.sal 2>&1
if grep -q "fallo de página en 0xDEAD0000 (lectura, núcleo, no presente)" $P/fallo.sal && grep -q "^\[código 14\]" $P/fallo.sal; then ok "un fallo de página del núcleo fuera de la zona a la demanda: lo dice con la dirección y se para (código 14)"
else ko "el fallo de página del núcleo no se dice bien"; sed 's/^/        /' $P/fallo.sal | head -6; fi

# ---- el disco, la FAT, las llamadas nuevas y el intérprete de órdenes ----
# (un disco duro IDE de imagen, hecho con mkfs.fat y mtools: nucleo32/disco.sh)
echo "== Hispa-Pentium con disco: ATA, FAT, programas del disco y el intérprete de órdenes del anillo 3 =="
hechos=1
for c in 12 16 32 mbr; do
  $AQUI/nucleo32/disco.sh $P/d$c.img $c > $P/disco$c.txt 2>&1 || { hechos=0; ko "no se puede hacer el disco de prueba ($c): $(tail -n 1 $P/disco$c.txt)"; }
done
if [ $hechos = 1 ]; then
# lo que tiene que sumar TEXTOS\LARGO.TXT (el mismo que escribe disco.sh)
suma=$(python3 -c "print(sum(sum(('linea %03d de un archivo de varios clusters\r\n' % i).encode()) for i in range(1, 401)))")
# 1. desde la consola del núcleo, con FAT16 (en la raíz, también AZAR.SEM: la
# semilla del azar, que el núcleo escribe en el primer arranque desde la 0.8.7)
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" disco dir "dir textos" "dir nada" "lanzar hola.bin" "lanzar eco.bin uno dos tres" \
  "lanzar noprog.bin" "lanzar nada.bin" disco apagar > $P/disco.sal 2>&1
S=$P/disco.sal
mira "el controlador ATA (PIO): IDENTIFY dice qué disco hay en el canal primario, y cuántos sectores" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16\.$" "^disco 0: QEMU HARDDISK, 32768 sectores (16 MB), LBA" "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$"
mira "la FAT16 se monta y se lee su raíz (los nombres largos se saltan; queda el corto)" \
  "^sistema de archivos: FAT16, desde el sector 0, [0-9]* clusters de 2048 octetos, [0-9]* libres" \
  "^ORDENES.BIN  *[0-9]*$" "^LEEME.TXT  *118$" "^NOMBRE~1.TXT  *32$" "^TEXTOS  *<carpeta>" "^AZAR.SEM  *32$" "^21 entradas"
mira "una carpeta dentro de otra, y una que no está" "^LARGO.TXT  *17600$" "^PIRATA.TXT  *110$" "^5 entradas" "^no puedo abrir «nada»: no existe"
m1=$(grep "^hola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3 cargado del disco (no de multiboot): corre y acaba con su código" \
  "^lanzo hola.bin del disco (la tarea [0-9]*, en el anillo 3)" "^1 + 2 + ... + 1000 = 500500" "^hola.bin ha acabado con el código 3; marcos libres"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa del disco, la memoria vuelve entera: los mismos marcos libres ($m1)"
else ko "el programa del disco se deja marcos: «$m1»"; fi
mira "un programa del disco recibe sus argumentos" "^uno dos tres$" "^eco.bin ha acabado con el código 3"
mira "lo que no es un programa, o no está, no se lanza (y se dice por qué)" "^no puedo lanzar noprog.bin: no es un programa de Hispa-Pentium" \
  "^no puedo lanzar nada.bin: no existe" "; archivos abiertos: 0$" "^\[código 5\]"
# 2. el intérprete de órdenes del anillo 3 (ORDENES.BIN, del disco)
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes ayuda ver mem dir "dir textos" "dir nada" "type leeme.txt" "type textos\\pirata.txt" \
  "type nada.txt" "TYPE textos" "eco hola y adios" "textos\\eco2 desde una carpeta" hola malo nada noprog prueba :espera "prueba fuera" \
  "prueba pozo" :espera salir tareas memoria disco apagar > $P/ordenes.sal 2>&1
S=$P/ordenes.sal
mira "el intérprete de órdenes es un programa del anillo 3 cargado del disco, y lee el teclado por la llamada al sistema" \
  "^lanzo ORDENES.BIN del disco (la tarea [0-9]*, en el anillo 3)" "^Hispa-Pentium: intérprete de órdenes (un programa del anillo 3" \
  "^C:.>ayuda$" "^Las órdenes de Hispa-Pentium:" "^  TYPE archivo "
mira "VER y MEM" "^Hispa-Pentium ${VERSION_BASE//./\\.}: un núcleo de 32 bits escrito en Castel" "^Memoria: 32640 K en total, [0-9]* K libres ([0-9]* marcos de 4 K)" \
  "^Este intérprete ocupa [0-9]* K. Tareas en marcha: 4\.$"
mira "DIR: la raíz y una carpeta, con sus tamaños, sus fechas y lo que queda libre" "^ Directorio de C:.$" "^LEEME    TXT  *118  [0-9/]*  [0-9:]*$" \
  "^TEXTOS  *<DIR>  " "^ *18 archivos  *[0-9]* octetos$" "^ *3 carpetas  *[0-9]* octetos libres$" "^ Directorio de C:.TEXTOS$" \
  "^LARGO    TXT  *17600  " "^ *3 archivos  *18[0-9]* octetos$" "^No puedo abrir nada: no existe\.$"
mira "TYPE: un archivo de la raíz y uno de una carpeta (abrir, leer y cerrar por la llamada al sistema)" \
  "^Este archivo está en un disco FAT y lo enseña la orden TYPE\.$" "^Con diez cañones por banda,$" "^un velero bergantín\.$" \
  "^No puedo abrir nada.txt: no existe\.$" "^textos es una carpeta"
mira "un programa se lanza por su nombre, con sus argumentos, y se le espera (también de una carpeta)" "^hola y adios$" \
  "^(ECO.BIN ha acabado con el código 3)$" "^desde una carpeta$" "^(TEXTOS.ECO2.BIN ha acabado con el código 3)$" "^(HOLA.BIN ha acabado con el código 3)$"
mira "un programa que hace lo que no debe se acaba, y el intérprete sigue; lo desconocido se dice" \
  "^\*\*\* se acaba la tarea [0-9]* (MALO.BIN)" "^(MALO.BIN ha acabado con el código 255)$" \
  "^Orden o nombre de programa desconocido: nada (escriba AYUDA)\.$" "^No puedo lanzar NOPROG.BIN: no es un programa de Hispa-Pentium\.$"
mira "la pila de un programa crece sola: el fallo de página le pone páginas según baja (y el núcleo, si va a escribir en ella)" \
  "^pila: leo 17600 octetos del disco a un arreglo local sin estrenar ([4-6] páginas más)$" \
  "^pila: 200 llamadas, una dentro de otra, con 4000 octetos cada una: 200; la pila ha crecido 19[0-9] páginas$"
mira "una pila que se desboca (una función que se llama sin fin) acaba en su tope de 4 MB: se acaba el programa, no el núcleo" \
  "^pozo: una función que se llama a sí misma sin fin\.\.\.$" "fallo de página en 0xBFC00[0-9A-F]* (escritura, anillo 3, no presente)"
mira "la memoria del programa crece y se devuelve (sbrk): páginas nuevas a cero, y vuelven" \
  "^memoria: pido 100000 octetos: 25 páginas más, a cero: cierto, y la suma de lo escrito es 312487500$" \
  "^memoria: la devuelvo: cierto, y quedan 0 páginas de más$" "^memoria: no me dan 3 GB: cierto; ni devuelvo lo que no pedí: cierto$"
mira "leer un archivo de varios clusters a trozos de 777 octetos: lo mismo que hay en el disco" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
  "^archivo: al final ya no da más: cierto$" "^archivo: cerrado, ya no se deja leer: cierto$"
mira "los archivos abiertos tienen un tope, y las direcciones que no son del programa no se tocan" \
  "^abiertos: caben 16 a la vez; el siguiente: hay demasiados archivos abiertos$" "^abiertos: cerrados todos, se puede abrir otra vez: cierto$" \
  "^punteros: leer a la memoria del núcleo: cierto; a donde no tengo nada: cierto; un nombre que no es mío: cierto$" \
  "^punteros: un manejador que no existe: cierto; esperar a una tarea que no es mía: cierto$"
mira "un programa lanza otros dos a la vez y los espera" "^soy el hijo uno$" "^y yo soy el dos$" \
  "^hijos: los dos han acabado, con los códigos 4 y 5; ya no se les puede esperar: cierto$" "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$"
ra=$(grep -c "^ruido A [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rb=$(grep -c "^ruido B [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rm_=$(grep -a "ruido [AB]" $S | grep -vc "^ruido [AB] [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$")
alt=$(grep "^ruido [AB] " $S | awk '{print $2}' | uniq | wc -l)
if [ "$ra" = 120 ] && [ "$rb" = 120 ] && [ "$rm_" = 0 ] && [ "$alt" -gt 2 ]; then ok "la consola con su cerrojo: dos programas que escriben a la vez (240 líneas, $alt turnos) no se mezclan ni una letra"
else ko "las líneas de dos programas que escriben a la vez se mezclan o se pierden: $ra de A, $rb de B, $rm_ rotas, $alt turnos"; fi
mira "la memoria devuelta ya no se puede tocar: fallo de página, y se acaba el programa" "^fuera: escribo en la memoria que me han dado: cierto$" \
  "fallo de página en 0x4000[0-9A-F]*000 (escritura, anillo 3, no presente)" "^(PRUEBA.BIN ha acabado con el código 255)$"
if grep -q "no tenía que salir" $S; then ko "un programa ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer (leer el núcleo, tocar lo devuelto, salir del pozo), no lo han hecho"; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al salir, el núcleo ha recogido lo que dejó PRUEBA sin cerrar ni esperar (un archivo abierto y un programa)" \
  "^prueba: acabo con el código 7, un archivo abierto y un programa sin esperar$" "^(PRUEBA.BIN ha acabado con el código 7)$" \
  "soy el que nadie espera: he dormido y acabo solo$" \
  "^Hasta luego\.$" "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 2 bloques ocupados con 16384 octetos"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 3 ]; then
  ok "y no queda nada: los mismos marcos libres que antes de lanzar el intérprete ($m1) y solo las tres tareas del núcleo (con cd-cambios, la del lector de CD de QEMU)"
else ko "tras el intérprete queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi
# CLS: se mira la pantalla (lo que hay justo antes de la última orden)
VER=1 DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes dir cls apagar > $P/cls.sal 2>&1
if [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed -n '2p')" = '|C:\>' ] && [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed '1,2d' | grep -vc '^|$')" = 0 ]; then
  ok "CLS deja la pantalla en blanco, con la petición de órdenes arriba"
else ko "CLS no borra la pantalla:"; sed -n '/^\[pantalla\]/,$p' $P/cls.sal | head -5 | sed 's/^/        /'; fi
# 3. las otras FAT, y el disco con tabla de particiones
for c in 12 32 mbr; do
  case $c in 12) que="FAT12"; f=FAT12; s0=0 ;; 32) que="FAT32"; f=FAT32; s0=0 ;; mbr) que="un disco con tabla de particiones (la FAT16, en la primera)"; f=FAT16; s0=63 ;; esac
  DISCO=$P/d$c.img $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar prueba.bin" :espera "lanzar textos\\eco2.bin hola" apagar > $P/fat$c.sal 2>&1
  S=$P/fat$c.sal
  mira "$que: se monta, se lee una carpeta, un archivo de varios clusters y se cargan programas" \
    "^sistema de archivos: $f, desde el sector $s0, " "^LARGO.TXT  *17600$" \
    "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
    "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$" "^prueba.bin ha acabado con el código 7" "^hola$" "^eco2.bin ha acabado con el código 1"
done
# 4. el intérprete, él solo al arrancar (si el cargador dice «ordenes»); y sin disco
DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ver salir apagar > $P/arranque.sal 2>&1
S=$P/arranque.sal
mira "si el cargador se lo dice (-append ordenes), el núcleo arranca con el intérprete de órdenes (y sin módulos de multiboot: todo del disco)" \
  "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 0 programas de usuario" \
  "^C:.>ver$" "^Hispa-Pentium 0\.8" "^ORDENES.BIN ha acabado con el código 0" "^\[código 5\]"
MODULOS= $AQUI/nucleo32/probar.sh "" disco dir ordenes apagar > $P/sindisco.sal 2>&1
S=$P/sindisco.sal
mira "sin disco (y sin nada que traiga el cargador), el núcleo lo dice y sigue" "^disco 0: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$" "^sistema de archivos: no hay" "^no puedo lanzar ORDENES.BIN: no hay disco" "^\[código 5\]"

# ---- escribir en el disco: ATA, la FAT (12, 16, 32 y con particiones) y las órdenes ----
# (cada prueba escribe en una copia del disco, con ESCRIBE=1; luego la miran fsck.fat y mtools)
echo "== Hispa-Pentium escribe en el disco: ATA, FAT12/16/32, COPY, DEL, REN, MD, RD y la redirección =="
export MTOOLS_SKIP_CHECK=1
for c in 16 12 32 mbr; do
  case $c in 12) que="FAT12" ;; 16) que="FAT16" ;; 32) que="FAT32" ;; mbr) que="FAT16 en una partición" ;; esac
  I=$P/w$c.img; cp $P/d$c.img $I
  M=$I; s0=; [ $c = mbr ] && { M=$I@@32256; s0=63; }
  lleno=(); [ $c = 12 ] && lleno=("escribe lleno" :espera :espera :espera)
  ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes escribe "echo primera línea > nuevo.txt" "echo y la segunda >> nuevo.txt" "type nuevo.txt" \
    "copy textos\\largo.txt copia.txt" "md obra" "cd obra" "copy \\textos\\*.txt" "copy \\prueba.bin ." "md sub" "ren pirata.txt poema.txt" \
    "del largo.txt" "rd sub" "cd \\" "rd obra" "eco hola mundo > eco.txt" "dir obra >> eco.txt" "ren leeme.txt hola.bin" "del nada.txt" \
    "copy nada.txt x.txt" "md textos" "${lleno[@]}" salir disco apagar > $P/escribe$c.sal 2>&1
  S=$P/escribe$c.sal
  mira "$que: un programa crea un archivo, lo escribe a trozos, lo lee de vuelta, escribe en medio, lo corta, lo alarga y le añade" \
    "^escribir: un archivo nuevo, 10000 octetos a trozos de 777; mide 10000; ocupa los clusters que tocan: cierto$" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^en medio: en 5000 escribo HOLA y leo HOLA; sigue midiendo 10000$" "^cortar: en 3000: 0; mide 3000; quedan libres los clusters que sobran: cierto$" \
    "^hueco: escribo en 20000: mide 20001, y lo de en medio está a cero (0 octetos que no)$" "^añadir: tres octetos al final: mide 20004 y acaba en FIN$" \
    "^crear encima: mide 0; borrado: 0; ya no está: cierto$"
  mira "$que: lo que no se puede (abrir dos veces para escribir, borrar lo abierto, nombres que no valen...) se dice" \
    "^no se puede: abrirlo otra vez para escribir: cierto; ni para leer: cierto; ni borrarlo: cierto; ni cambiarle el nombre: cierto$" \
    "^no se puede: crear en una carpeta que no está: cierto; un nombre de más de 8+3: cierto; ni con letras raras: cierto; ni el de un dispositivo: cierto; escribir en una carpeta: cierto$" \
    "^no se puede: borrar lo que no está: cierto; borrar una carpeta como archivo: cierto; abrir para escribir lo que no está: cierto$"
  mira "$que: carpetas: hacerlas, entrar, 70 archivos (la carpeta crece), mover y cambiar nombres, vaciarlas y borrarlas; todo queda como estaba" \
    "^carpetas: hago PD: 0; otra vez no: cierto; entro: 0; estoy en .PD$" "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" \
    "^carpetas: F42.TXT dice «soy el archivo 42»; y «...PD...F07.TXT» se abre: cierto$" \
    "^mover: F00.TXT a DENTRO.CERO.TXT: 0; está allí: cierto y no aquí: cierto; la carpeta pasa a llamarse FUERA: 0 y lleva lo suyo: cierto$" \
    "^no se puede: llamarlo como otro que ya está: cierto; borrar una carpeta con algo: cierto; ni la carpeta donde estoy: cierto; ni un archivo como carpeta: cierto$" \
    "^carpetas: borrados 70 archivos; FUERA: 0; PD: 0; ya no se puede entrar: cierto$" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$"
  mira "$que: las órdenes dicen lo que no puede ser (y el programa redirigido, su código, por la pantalla)" \
    "^No puedo borrar la carpeta obra: la carpeta no está vacía\.$" "^No puedo llamar hola.bin a leeme.txt: ya existe\.$" "^No puedo borrar nada.txt: no existe\.$" \
    "^No puedo abrir nada.txt: no existe\.$" "^No puedo hacer la carpeta textos: ya existe\.$" "^(ECO.BIN ha acabado con el código 2)$" \
    "^ *2 archivo(s) copiado(s)\.$" "^primera línea$" "^y la segunda$" "; archivos abiertos: 0$" "^\[código 5\]"
  if [ $c = 12 ]; then
    mira "$que: el disco se llena (se escribe hasta el último cluster), se dice, y al borrar vuelve todo" \
      "^lleno: escribo hasta que no cabe: cierto; quedan 0 clusters libres; el siguiente: no queda sitio en el disco$" \
      "^lleno: ni otro archivo con algo: no queda sitio en el disco; ni una carpeta: no queda sitio en el disco$" \
      "^lleno: lo borro: 0; vuelven a estar libres los de antes: cierto$"
  fi
  # lo que ha quedado en el disco, visto desde fuera
  if sano $I $s0; then ok "$que: después de todo eso, fsck.fat da el disco por sano ($(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "$que: fsck.fat encuentra el disco mal:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  bien=1
  saca $M NUEVO.TXT $P/x.txt && printf 'primera l\xa1nea\r\ny la segunda\r\n' | cmp -s - $P/x.txt || bien=0
  saca $M COPIA.TXT $P/x.txt && saca $P/d16.img TEXTOS/LARGO.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/POEMA.TXT $P/x.txt && saca $P/d16.img TEXTOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/PRUEBA.BIN $P/x.txt && saca $P/d16.img PRUEBA.BIN $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M ECO.TXT $P/x.txt && [ "$(head -1 $P/x.txt)" = $'hola mundo\r' ] && grep -a -q '^ Directorio de C:.OBRA' $P/x.txt && grep -a -q '^POEMA    TXT  *110  ' $P/x.txt \
    && ! grep -a -q -v $'\r$' $P/x.txt || bien=0
  saca $M OBRA/LARGO.TXT $P/x.txt && bien=0
  saca $M OBRA/PIRATA.TXT $P/x.txt && bien=0
  mdir -i $M ::OBRA 2> /dev/null | grep -q '^SUB ' && bien=0
  mdir -i $M :: 2> /dev/null | grep -q -i '^PD \|^P1 \|^LLENO ' && bien=0
  if [ $bien = 1 ]; then ok "$que: y mtools ve lo mismo: lo creado con «>» y «>>» (con sus finales de línea), lo copiado (igual que el original), lo renombrado y lo borrado"
  else ko "$que: lo que hay en el disco no es lo que tenía que ser:"; mdir -i $M :: ::OBRA 2>&1 | sed 's/^/        /' | head -40; fi
  # y otra vez encendido, el núcleo lee lo que escribió
  DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "type nuevo.txt" "dir obra" "type eco.txt" salir apagar > $P/relee$c.sal 2>&1
  S=$P/relee$c.sal
  mira "$que: al volver a arrancar, lo escrito sigue ahí" "^primera línea$" "^y la segunda$" "^POEMA    TXT  *110  " "^PRUEBA   BIN  *[0-9]*  " \
    "^ *2 archivos  *[0-9]* octetos$" "^hola mundo$" "^ Directorio de C:.OBRA$"
done

# ---- el intérprete: carpetas de trabajo, PATH, archivos de órdenes, la historia, Ctrl+C ----
echo "== Hispa-Pentium: CD y la carpeta de trabajo, PATH, archivos .BAT, la historia de la línea y Ctrl+C =="
I=$P/int.img; cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "cd textos" cd "dir" "type pirata.txt" "eco desde textos" "cd .." "cd textos\\.." cd "cd nada" "cd leeme.txt" \
  "md uno" "cd uno" "md dos" "cd dos" "echo hondo > aqui.txt" "type \\uno\\dos\\aqui.txt" "cd..\\.." "type uno\\dos\\aqui.txt" "rd uno\\dos" \
  "del uno\\dos\\aqui.txt" "rd uno\\dos" "rd uno" path "cd textos" "path ;" path "eco sin path" "eco2 con el de aquí" "path c:\\nada;c:\\" "eco con path" "cd \\" \
  "lote uno dos" :espera ":tecla spc" visto "eco primera" "eco segunda" ":tecla up up ret" ":tecla up left left left backspace x home delete z end y ret" \
  ":tecla e c o esc v e r ret" fecha salir apagar > $P/interprete.sal 2>&1
S=$P/interprete.sal
mira "CD: la carpeta de trabajo sale en la petición de órdenes, y las rutas se entienden desde ella («..» sube)" \
  "^C:.>cd textos$" "^C:.TEXTOS>cd$" "^C:.TEXTOS$" "^ Directorio de C:.TEXTOS$" "^un velero bergantín\.$" "^C:.TEXTOS>cd \.\.$" "^C:.>cd textos...$" \
  "^No puedo entrar en nada: no existe\.$" "^No puedo entrar en leeme.txt: no es una carpeta\.$"
mira "carpetas dentro de carpetas: se crean, se entra, se escribe en ellas, y no se borran con algo dentro" \
  "^C:.UNO.DOS>echo hondo > aqui.txt$" "^C:.UNO.DOS>cd.....$" "^C:.>type uno.dos.aqui.txt$" "^No puedo borrar la carpeta uno.dos: la carpeta no está vacía\.$" \
  "^C:.>rd uno$"
if [ "$(grep -c '^hondo$' $S)" = 2 ] && ! grep -q "^No puedo borrar la carpeta uno:" $S; then ok "y la carpeta de trabajo vale igual para crear que para leer (con la ruta entera se lee lo mismo)"
else ko "lo escrito en la carpeta de trabajo no se lee por su ruta entera"; fi
mira "la carpeta de trabajo la hereda el programa que se lanza, y el PATH dice dónde se buscan los programas" \
  "^PATH=C:.$" "^desde textos$" "^(C:.ECO.BIN ha acabado con el código 2)$" "^No hay PATH\.$" "^Orden o nombre de programa desconocido: eco (escriba AYUDA)\.$" \
  "^con el de aquí$" "^(ECO2.BIN ha acabado con el código 4)$" "^con path$"
mira "un archivo de órdenes (.BAT): sus líneas, ECHO, REM, PAUSE, sus argumentos (%1), programas y redirección dentro" \
  "^C:.>lote uno dos$" "^lote: empiezo con uno y dos (y el 100%)$" "^desde el lote$" "^(ECO.BIN ha acabado con el código 3)$" "^lote: una línea$" "^lote: y otra$" \
  "^Pulse una tecla para seguir \. \. \. $" "^lote: fin$"
if ! grep -q "^C:.>ECHO lote\|REM un archivo\|^C:.>PAUSE" $S && grep -q "^C:.>REM con ECHO activado se ve cada orden$" $S && grep -q "^C:.>ECO visto$" $S \
    && grep -q "^esta no se ve$" $S && ! grep -q "ECHO esta no se ve" $S; then ok "ECHO OFF y la @ callan las órdenes del archivo; sin ellos, se ven antes de hacerse"
else ko "ECHO OFF o la @ no callan lo que deben (o callan de más)"; grep -n "lote\|visto\|REM\|se ve" $S | sed 's/^/        /' | head; fi
# la historia: lo que queda en la pantalla, quitando lo que borra cada retroceso
python3 -I $AQUI/nucleo32/pantalla.py < $S > $P/historia.txt
if [ "$(grep -c '^C:.>eco primera$' $P/historia.txt)" = 2 ] && [ "$(grep -c '^primera$' $S)" = 2 ] && grep -q '^C:.>zco prixeray$' $P/historia.txt \
    && grep -q "^Orden o nombre de programa desconocido: zco " $S && grep -q '^C:.>ver$' $P/historia.txt && grep -q "^Hispa-Pentium 0\.8" $S; then
  ok "la historia de la línea: la flecha de arriba trae las órdenes de antes; se corrigen con las flechas, Retroceso, Supr, Inicio y Fin; Esc borra la línea"
else ko "la historia o la corrección de la línea no van:"; grep -n "primera\|zco\|ver" $P/historia.txt | sed 's/^/        /' | head; fi
mira "FECHA: la fecha y la hora del reloj del PC" "^Hoy es $(date +%d/%m/%Y) y son las [0-2][0-9]:[0-5][0-9]:[0-5][0-9]\.$"
if sano $I && ! mdir -i $I :: 2> /dev/null | grep -q '^UNO \|^LOTE *TXT'; then ok "y el disco queda sano, sin las carpetas ni los archivos de paso"
else ko "tras las pruebas del intérprete el disco no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
# Ctrl+C
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes bucle :espera ":tecla ctrl-c" "bucle duerme" ":tecla ctrl-c" "bucle tecla" ":tecla a ctrl-c" \
  sinfin :espera ":tecla ctrl-c" "bucle escribe" :espera ":tecla ctrl-c" "dir bucle.dat" ":tecla e c o ctrl-c" "echo sigo aquí" "prueba ruido Z > ruido.txt" ":tecla ctrl-c" \
  "lote a b" :espera ":tecla ctrl-c" salir tareas memoria disco "lanzar bucle.bin" :espera ":tecla ctrl-c" tareas apagar > $P/ctrlc.sal 2>&1
S=$P/ctrlc.sal
mira "Ctrl+C acaba el programa que corre: uno que da vueltas sin llamar al sistema, uno que duerme y uno que espera teclas" \
  "^bucle: doy vueltas sin fin" "^bucle: duermo sin fin" "^bucle: espero teclas sin fin" "^tecla 97$" "^C:.>bucle duerme$" "^C:.>bucle tecla$" "^C:.>sinfin$"
if [ "$(grep -c '^\^C$' $S)" -ge 7 ] && ! grep -q "ha acabado con el código 25[45])" $S; then ok "cada vez sale ^C y vuelve la petición de órdenes (sin más avisos)"
else ko "Ctrl+C no deja las cosas como debe: $(grep -c '^\^C$' $S) veces ^C"; fi
mira "Ctrl+C en un archivo de órdenes: acaba el programa y el archivo (no sigue con la línea siguiente); y en PAUSE" "^sinfin: lanzo BUCLE$" "^C:.>lote a b$" \
  "^Pulse una tecla para seguir"
if grep -q "no tenía que salir\|^lote: fin" $S; then ko "tras Ctrl+C, el archivo de órdenes ha seguido"; else ok "y lo que venía detrás en el archivo no se hace"; fi
mira "Ctrl+C sin programa en marcha: deja la línea que se estaba escribiendo, y el intérprete sigue" "^C:.>eco^C$" "^sigo aquí$"
t=$(sed -n 's/^bucle.dat es un archivo de \([0-9]*\) octetos\.$/\1/p' $S)
if [ -n "$t" ] && [ "$t" -gt 0 ] && [ $((t % 1000)) = 0 ] && sano $I && saca $I BUCLE.DAT $P/x.txt && [ "$(wc -c < $P/x.txt)" = "$t" ]; then
  ok "un programa cortado mientras escribía: su archivo queda cerrado, con lo que llevaba ($t octetos), y el disco sano"
else ko "el archivo del programa cortado no queda bien: «$t»"; sed 's/^/        /' $P/fsck.txt | head -8; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al cortar, el núcleo lo recoge todo: ni archivos abiertos ni tareas de más; y Ctrl+C también desde la consola del núcleo" \
  "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 2 bloques ocupados con 16384 octetos" \
  "^bucle.bin ha acabado con el código 254; marcos libres" "^\[código 5\]"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 6 ]; then
  ok "y la memoria vuelve entera ($m1 marcos libres antes y después)"
else ko "tras Ctrl+C queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi

# ---- el teclado español y los nombres largos ----
echo "== Hispa-Pentium: el teclado español (acentos con tecla muerta, ñ, ç, ¿ ¡) y los nombres largos (VFAT) =="
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "echo ¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï > letras.txt" \
  "echo 1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ ÁÍÓÚ > signos.txt" "type letras.txt" teclado "teclado us" ":tecla e c h o spc bracket_left bracket_right semicolon apostrophe backslash slash ret" "teclado es" \
  ":tecla caps_lock" "echo bloqueo con ñ y ç" ":tecla caps_lock" "dir largos" "type largos\\cancion_del_pirata.txt" "type largos\\AÑO" \
  "copy largos\\cancion_del_pirata.txt cancion.txt" "cd largos" "ren cancion_del_pirata.txt pirata.txt" "del añonue~1.txt" "dir" salir teclas apagar > $P/teclado.sal 2>&1
S=$P/teclado.sal
bien=1
saca $I LETRAS.TXT $P/x.txt && printf '¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï\r\n' | iconv -f UTF-8 -t CP437 | cmp -s - $P/x.txt || bien=0
# (0.8.5) las mayúsculas con tilde, en la 437 de Hispa (tools/hispa437.py): Á B6h, Í CFh, Ó D3h, Ú D7h
saca $I SIGNOS.TXT $P/y.txt && { printf '%s' "1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ " | iconv -f UTF-8 -t CP437; printf '\266\317\323\327\r\n'; } | cmp -s - $P/y.txt || bien=0
if [ $bien = 1 ]; then ok "el teclado español: ñ, ç, ¿ ¡, º ª, los acentos con su tecla muerta (´ \` ^ ¨; Á Í Ó Ú en la 437 de Hispa), Alt Gr ([ ] { } @ # | \\ ~) y los signos, octeto a octeto en la página 437"
else ko "el teclado español no da las letras que debe:"; od -c $P/x.txt | head -8 | sed 's/^/        /'; od -c $P/y.txt | head -6 | sed 's/^/        /'; fi
mira "lo tecleado se ve en la pantalla; TECLADO cambia al de EE. UU. y vuelve; Bloq Mayús" "^¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï$" \
  "^Teclado: español (ES)\.$" "^Teclado: de EE\. UU\. (US)\.$" "^\[\];'./$" "^BLOQUEO CON Ñ Y Ç$"
mira "los nombres largos (VFAT) se leen de las carpetas, con sus letras de fuera del ASCII, y se abre por ellos" \
  "^CANCIO~1 TXT  *57  [0-9/]*  [0-9:]*  cancion_del_pirata.txt$" "^AÑONUE~1 TXT  *18  [0-9/]*  [0-9:]*  Año nuevo.txt$" "^que es mi dios la libertad\.$" \
  "^No puedo abrir largos.AÑO: no existe\.$" "^ *1 archivo(s) copiado(s)\.$" "^PIRATA   TXT  *57  [0-9/]*  [0-9:]*$"
if sano $I && saca $I CANCION.TXT $P/x.txt && [ "$(wc -c < $P/x.txt)" = 57 ] && saca $I LARGOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt \
    && ! mdir -i $I ::LARGOS 2> /dev/null | grep -q -i 'cancion_del\|nuevo'; then
  ok "al cambiar el nombre o borrar un archivo con nombre largo, sus trozos se van con él (fsck.fat no ve nombres largos sueltos)"
else ko "los nombres largos no quedan bien al renombrar o borrar:"; sed 's/^/        /' $P/fsck.txt | head -8; mdir -i $I ::LARGOS 2>&1 | sed 's/^/        /'; fi
fi

# ---- sin disco (la unidad en memoria), el permiso de escritura y el escritorio ----
echo "== Hispa-Pentium: sin disco, los módulos del cargador hacen de unidad M:; en el disco no se escribe sin permiso =="
G=$P/graf
mkdir -p $G $G/mod
for p in uordenes:ORDENES uhola:HOLA ueco:ECO; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${p%%:*}.cas -o $G/mod/x.asm > /dev/null 2>&1 && nasm -f bin $G/mod/x.asm -o $G/mod/${p##*:}.BIN
done
printf 'un texto que ha venido con el cargador\r\n' > $G/mod/LEEME.TXT
MODULOS="$G/mod/ORDENES.BIN,$G/mod/HOLA.BIN,$G/mod/ECO.BIN,$G/mod/LEEME.TXT" ARRANQUE=ordenes $AQUI/nucleo32/probar.sh "" dir "type leeme.txt" "eco uno dos" hola \
  "echo x > a.txt" "del leeme.txt" "md c" "cd nada" nada salir "lanzar eco.bin desde la consola" apagar > $G/mem.sal 2>&1
S=$G/mem.sal
mira "sin disco, lo que trae el cargador (los módulos de multiboot) hace de unidad M: en memoria: «ordenes» lanza el ORDENES.BIN que venga" \
  "^sin disco: los 4 archivos que ha traído el cargador hacen de unidad M: (en memoria, de solo lectura)" "^Hispa-Pentium: intérprete de órdenes"
mira "la unidad en memoria: DIR enseña los módulos, TYPE los lee y los programas se lanzan por su nombre" \
  "^ORDENES  BIN  *[0-9]* " "^LEEME    TXT  *40 " "^ *4 archivos  *[0-9]* octetos$" "^un texto que ha venido con el cargador$" "^uno dos$" \
  "^1 + 2 + ... + 1000 = 500500" "^(HOLA.BIN ha acabado con el código 3)" "^Orden o nombre de programa desconocido: nada "
mira "la unidad en memoria es de solo lectura: ni crear, ni borrar, ni carpetas (y se dice)" "^No puedo escribir en a.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^No puedo hacer la carpeta c: es de solo lectura\.$"
mira "y desde la consola del núcleo también se lanzan por su nombre" "^desde la consola$" \
  "^ECO.BIN ha acabado con el código 3; marcos libres" "^\[código 5\]"
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "sin disco, al salir del intérprete quedan los mismos marcos libres ($m1)"
else ko "sin disco, el intérprete se deja marcos: «$m1»"; fi
# el disco, sin permiso para escribir (como en un PC de verdad: hay que decir «escritura»)
cp $P/d16.img $G/sl.img
SOLO_LECTURA=1 ESCRIBE=1 DISCO=$G/sl.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "echo hola > nuevo.txt" "del leeme.txt" "md cosas" \
  "ren leeme.txt otro.txt" "copy leeme.txt copia.txt" "type leeme.txt" hola salir apagar > $G/sl.sal 2>&1
S=$G/sl.sal
mira "sin «escritura» en la línea del cargador, el disco es de solo lectura: se lee y se lanzan sus programas, pero no se crea, borra ni cambia nada" \
  "^el disco es de solo lectura (para escribir en él: «escritura» en la línea del cargador)" "^No puedo escribir en nuevo.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^No puedo hacer la carpeta cosas: es de solo lectura\.$" \
  "^Este archivo está en un disco FAT" "^1 + 2 + ... + 1000 = 500500" "^\[código 5\]"
if cmp -s $P/d16.img $G/sl.img; then ok "y la imagen del disco queda idéntica, octeto a octeto: no se ha escrito ni un sector"
else ko "sin permiso de escritura, el disco ha cambiado"; fi
DISCO=$P/d16.img ARRANQUE="ordenes sindisco" MODULOS="$G/mod/ORDENES.BIN,$G/mod/ECO.BIN" $AQUI/nucleo32/probar.sh "" dir salir disco apagar > $G/sin.sal 2>&1
S=$G/sin.sal
if ! grep -q "^disco 0: QEMU" $S && grep -q "^disco 0: no hay" $S && grep -q "^ *2 archivos " $S && grep -q "^sin disco: los 2 archivos" $S; then
  ok "con «sindisco» el núcleo ni mira los discos del PC: la unidad es la de los módulos"
else ko "«sindisco» no deja los discos sin mirar:"; sed 's/^/        /' $S | head -12; fi

echo "== Hispa-Pentium: el escritorio (modo gráfico, ratón, ventanas, barra de tareas, menú Inicio, iconos, Bloc de notas, Consola y Mi equipo) =="
cp $P/d16.img $G/e.img
hora1=$(date +%H:%M); antes=$(date +%s)
CAPTURAS=$G ESCRIBE=1 DISCO=$G/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":captura e1" \
  ":clic 40 585" ":captura e2" ":clic 80 176" ":pausa 2" ":escribe Querida señora: ¿qué tal está?" ":tecla ret" ":escribe Adiós." ":captura e3" \
  ":tecla f2" ":pausa 1" ":captura e4" ":arrastra 300 75 400 175" ":raton 600 450" ":captura e5" ":doble 400 175" ":pausa 1" ":captura e6" \
  ":doble 400 14" ":pausa 1" ":clic 630 176" ":captura e7" ":clic 150 585" ":captura e8" \
  ":clic 40 585" ":clic 80 216" ":pausa 3" "eco hola desde la ventana" "bucle" ":pausa 1" ":tecla ctrl-c" ":pausa 1" "ver" ":captura e9" \
  "salir" ":pausa 1" ":captura e10" ":clic 40 585" ":clic 300 238" ":raton 700 530" ":captura e11" ":clic 620 486" ":pausa 1" \
  ":clicd 720 120" ":clic 700 160" ":pausa 1" ":clicd 720 120" ":clic 700 160" ":pausa 2" ":captura e12" \
  ":clic 678 176" ":pausa 1" ":captura e13" ":clic 40 585" ":clic 360 545" ":captura e14" ":clic 275 327" > $G/escritorio.sal 2>&1
S=$G/escritorio.sal
mira "«escritorio» en la línea del cargador: el núcleo pone el modo gráfico (VBE de Bochs/QEMU, 800x600 con 32 bits) y arranca el escritorio" \
  "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha puesto el núcleo, por la VBE de Bochs/QEMU), tapiz 0"
miracap "el escritorio: el tapiz con el disquete y su nombre, la barra de tareas con «Inicio» y el reloj en la bandeja" e1 tam 800 600 \
  negrita 30 570 46 580 Inicio negrita 630 505 645 520 Hispa-Pentium rgb 400 2 154C9C rgb 300 598 1D5DBA 10 rgb 795 597 3393E0 12
# (0.8.9) la hora puede ser cualquiera de las de entre que se empieza y que se
# acaba: probar.sh compila antes la semilla y sus programas (con el PC cargado,
# más de un minuto, y la hora de antes ya no valía)
reloj_bien=0
for ((t = antes; t <= $(date +%s) + 60; t += 60)); do
  cap $G/e1.ppm negrita 740 570 752 580 $(date -d @$t +%H:%M) && { reloj_bien=1; break; }
done
if [ $reloj_bien = 1 ]; then ok "el reloj de la bandeja da la hora del PC (la de la CMOS: $hora1)"
else ko "el reloj de la bandeja no da la hora ($hora1)"; fi
miracap "la flecha del ratón está pintada donde está el ratón (y solo ahí)" e2 rgb 40 585 10233F rgb 42 589 FFFFFF nrgb 400 300 10233F
miracap "el menú Inicio (MENÚ): los fijados a la izquierda, «Todos los programas» abajo, Mis documentos y Configuración a la derecha, y Cerrar sesión, Reiniciar y Apagar equipo en el pie" e2 \
  negrita 54 104 62 112 Hispa-Pentium negrita 46 155 54 163 "Bloc de notas" negrita 46 195 54 203 Consola negrita 58 492 66 500 "Todos los programas" \
  negrita 256 159 264 167 "Mis documentos" negrita 256 226 264 234 "Configuración" negrita 68 533 76 541 "Cerrar sesión" negrita 210 533 222 541 Reiniciar \
  negrita 320 533 328 541 "Apagar equipo" rgb 100 400 FFFFFF rgb 300 450 DCEAFB
mira "el menú lanza el Bloc de notas (un programa del anillo 3, del disco)" "escritorio: lanzo .BLOC.BIN (la tarea [0-9]*)"
miracap "el Bloc de notas en su ventana: el título con su nombre, su botón en la barra de tareas, y lo tecleado (con eñe, acentos y ¿?) dentro" e3 \
  negrita 112 64 124 72 "NOTAS.TXT - Bloc de notas" texto 130 572 140 580 "NOTAS.TXT - Blo" texto 94 146 102 156 "Querida señora: ¿qué tal está?" \
  texto 94 162 102 172 "Adiós." negrita 110 116 130 126 Nuevo negrita 190 116 220 126 Guardar texto 100 88 108 98 Archivo texto 168 88 178 98 "Edición" \
  notexto 0 180 330 560 Programas
mira "F2 guarda el texto en el disco (lo dice el propio programa)" "^bloc: guardado NOTAS.TXT, 38 octetos"
miracap "y lo dice abajo, en la ventana" e4 texto 94 428 104 448 "Guardado en NOTAS.TXT (38 octetos)."
miracap "arrastrar la ventana por su título la mueve (y donde estaba queda el tapiz)" e5 negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas" \
  texto 194 246 202 256 "Querida señora: ¿qué tal está?" notexto 100 60 140 80 "NOTAS.TXT - Bloc de notas"
miracap "dos clics en el título la maximizan (de lado a lado, hasta la barra de tareas), y el programa vuelve a pintar lo suyo" e6 \
  negrita 24 2 36 10 "NOTAS.TXT - Bloc de notas" texto 6 84 14 94 "Querida señora: ¿qué tal está?" rgb 797 300 1A5AB6 negrita 30 570 46 580 Inicio
miracap "otros dos la devuelven a su tamaño, y el botón de minimizar la recoge en la barra de tareas" e7 notexto 150 150 300 180 "NOTAS.TXT - Bloc de notas" \
  notexto 0 0 100 20 "NOTAS.TXT - Bloc de notas" texto 130 572 140 580 "NOTAS.TXT - Blo"
miracap "su botón de la barra de tareas la recupera, con lo que tenía" e8 negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas" texto 194 262 202 272 "Adiós."
mira "el menú lanza la Consola" "^escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "la Consola: el intérprete de órdenes dentro de una ventana (un terminal): órdenes, un programa, Ctrl+C y la versión" e9 \
  negrita 140 90 152 98 Consola texto 120 116 132 520 "C:\\>eco hola desde la ventana" texto 120 116 132 520 "hola desde la ventana" \
  texto 120 116 132 520 "bucle: doy vueltas sin fin (Ctrl+C para acabar)" texto 120 116 132 520 "^C" \
  texto 120 116 132 520 "Hispa-Pentium $VERSION_BASE: un núcleo de 32 bits escrito en Castel." texto 290 572 300 580 Consola
miracap "SALIR acaba el intérprete y la Consola se cierra (su ventana y su botón)" e10 notexto 130 80 160 100 Consola notexto 280 570 310 582 Consola \
  negrita 212 164 224 172 "NOTAS.TXT - Bloc de notas"
miracap "Configuración (PANEL): el Panel de control, con lo que hay (pantalla, memoria) y un icono por sección" e11 negrita 140 80 160 95 "Configuración" \
  negrita 190 115 205 125 "Hispa-Pentium $VERSION_BASE: Panel de control" texto 190 135 205 145 "Pantalla:  800 x 600, 32 bits por punto" negrita 155 245 165 255 "Fecha y hora" \
  negrita 140 437 150 447 "Personalización" rgb 10 10 154C9C
miracap "el tapiz se cambia desde su menú («Cambiar el tapiz», dos veces: «Verde»), y se repinta todo el escritorio" e12 rgb 10 10 1F6B52 negrita 30 570 46 580 Inicio
miracap "el botón de cerrar acaba el Bloc de notas (sin cambios pendientes): ni ventana ni botón en la barra" e13 \
  notexto 180 150 260 180 "NOTAS.TXT - Bloc de notas" notexto 100 570 200 582 "NOTAS.TXT - Blo" notexto 180 120 220 140 "Configuración"
miracap "«Apagar el equipo»: el cuadro con Apagar, Reiniciar, Cerrar sesión y Cancelar (MENÚ)" e14 negrita 170 190 180 200 "Apagar el equipo" \
  negrita 196 315 204 323 Apagar negrita 304 315 312 323 Reiniciar negrita 548 315 556 323 Cancelar
mira "«Apagar» apaga (el núcleo acaba con su código)" "^escritorio: apagar" "^\[código 5\]"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda la sesión del escritorio, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado durante la sesión del escritorio:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
fsck.fat -n $G/e.img > $G/fsck.txt 2>&1
MTOOLS_SKIP_CHECK=1 mtype -i $G/e.img ::NOTAS.TXT > $G/notas.bin 2> /dev/null
printf 'Querida se\xa4ora: \xa8qu\x82 tal est\xa0?\r\nAdi\xa2s.' > $G/notas.esp
if cmp -s $G/notas.bin $G/notas.esp && ! grep -qi "differ\|error\|unused\|orphan" $G/fsck.txt; then
  ok "NOTAS.TXT está en el disco con lo tecleado, octeto a octeto (página 437, finales de línea del DOS), y fsck.fat da el disco por sano"
else ko "lo que guarda el Bloc de notas no está bien en el disco:"; od -c $G/notas.bin | head -4 | sed 's/^/        /'; head -5 $G/fsck.txt | sed 's/^/        /'; fi
# ---- la 0.5: el botón secundario y sus menús, seleccionar, el portapapeles, los iconos, cambiar el tamaño ----
echo "== Hispa-Pentium 0.5: los menús del botón secundario, seleccionar texto, cortar, copiar y pegar, los iconos, las ventanas que se estiran y Mi equipo =="
RES=2F74D0     # el azul de lo seleccionado
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clicd 400 300" ":captura a1" ":clic 440 345" ":pausa 1" ":captura a2" \
  ":doble 60 110" ":pausa 2" ":escribe Hola mundo cruel" ":tecla ret" ":escribe segunda linea" ":doble 150 158" ":raton 300 300" ":captura a3" ":clicd 155 158" ":captura a4" ":clic 175 175" \
  ":captura a5" ":tecla end" ":tecla ctrl-v" ":tecla shift-left" ":tecla shift-left" ":tecla shift-left" ":captura a6" ":tecla ctrl-c" ":tecla right" ":tecla ctrl-v" \
  ":triple 150 174" ":captura a7" ":escribe X" ":clic 98 158" ":mayusclic 138 158" ":captura a8" ":tecla delete" ":arrastra 98 158 122 160" ":clic 200 100" ":captura a9" ":clic 220 152" \
  ":arrastra 595 455 695 505" ":pausa 1" ":captura a10" ":clic 40 585" ":clicd 80 216" ":captura a11" ":clic 100 228" ":pausa 3" ":clicd 400 400" ":captura a12" ":clic 420 440" \
  ":pausa 1" ":captura a13" > $G/menus.sal 2>&1
S=$G/menus.sal
miracap "los iconos del tapiz (Mi equipo, Bloc de notas, Consola), con su rótulo en letra normal" a1 texto 24 50 32 58 "Mi equipo" texto 8 126 16 134 "Bloc de notas" \
  texto 32 202 40 210 Consola
miracap "el botón secundario en el tapiz saca su menú: Configuración, Cambiar el tapiz, Organizar iconos y Acerca de Hispa-Pentium" a1 texto 410 304 418 314 "Configuración" \
  texto 410 328 418 338 "Cambiar el tapiz" texto 410 352 418 362 "Organizar iconos" texto 410 385 418 395 "Acerca de Hispa-Pentium" rgb 500 302 FFFFFF 12
miracap "«Cambiar el tapiz» pasa al siguiente (de «Olas» a «Azul»), y el menú se cierra" a2 rgb 300 4 16468E 8 notexto 400 300 430 400 "Organizar iconos" \
  texto 24 50 32 58 "Mi equipo"
mira "dos clics en el icono «Bloc de notas» lo abren" "escritorio: lanzo .BLOC.BIN (la tarea [0-9]*)"
miracap "en el Bloc de notas, dos clics seleccionan la palabra, que se ve en blanco sobre azul" a3 texto 134 146 142 154 mundo rgb 139 151 $RES rgb 176 164 $RES \
  nrgb 135 151 $RES nrgb 181 151 $RES texto 94 162 102 170 "segunda linea"
miracap "el botón secundario en el Bloc saca Cortar, Copiar, Pegar y Seleccionar todo" a4 texto 165 163 173 171 Cortar texto 165 187 173 195 Copiar texto 165 211 173 219 Pegar \
  texto 165 244 173 252 "Seleccionar todo"
miracap "«Cortar» quita lo seleccionado" a5 texto 94 146 102 154 "Hola  cruel" notexto 90 140 200 160 mundo
miracap "Ctrl+V lo pega donde está el cursor, y Mayús con la flecha selecciona hacia atrás, letra a letra" a6 texto 94 146 102 154 "Hola  cruelmu" rgb 203 151 $RES \
  rgb 225 164 $RES nrgb 199 151 $RES
miracap "Ctrl+C copia lo seleccionado y Ctrl+V lo pega; tres clics seleccionan la línea entera" a7 texto 94 146 102 154 "Hola  cruelmundondo" rgb 99 167 $RES rgb 200 180 $RES \
  nrgb 99 151 $RES
miracap "lo que se teclea sustituye a lo seleccionado; y un clic y Mayús con otro clic seleccionan de uno a otro" a8 texto 94 162 102 170 X notexto 90 160 200 176 segunda \
  rgb 99 151 $RES rgb 120 164 $RES nrgb 141 151 $RES       # ((PUNTEROS) 136,164 queda debajo de la flecha nueva, en 138,158)
miracap "la barra de menús del Bloc (Archivo, Edición, Ayuda): «Edición» abre Cortar, Copiar, Pegar y Seleccionar todo, con sus teclas" a9 \
  texto 178 116 186 124 "Cortar            Ctrl+X" texto 178 140 186 148 "Copiar            Ctrl+C" texto 178 197 186 205 "Seleccionar todo  Ctrl+A" rgb 99 151 $RES
miracap "arrastrar la esquina de la ventana la cambia de tamaño, y el programa vuelve a pintar lo suyo (el pie, más abajo; el papel, más ancho)" a10 \
  texto 94 480 102 488 "Copiado (3 letras)." texto 118 146 126 154 uelmundondo rgb 650 300 FFFFFF rgb 693 300 1A5AB6 10 notexto 90 430 110 446 NOTAS.TXT
miracap "el botón secundario en un programa del menú Inicio: «Abrir» y «Quitar del menú Inicio» (MENÚ)" a11 texto 90 220 98 228 Abrir texto 90 244 98 252 "Quitar del menú Inicio" negrita 46 155 54 163 "Bloc de notas"
mira "y «Abrir» lo lanza" "^escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "el botón secundario en la Consola: Copiar (en gris: no hay nada seleccionado) y Pegar" a12 texto 410 405 418 413 Copiar texto 410 429 418 437 Pegar
miracap "el portapapeles es del sistema: lo copiado en el Bloc de notas (con el menú Edición) se pega en la Consola" a13 texto 120 140 132 300 "C:\\> cr"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# la Consola: seleccionar y copiar; el menú de una ventana (en el título y en la barra); Mi equipo; Acerca de
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":doble 60 186" ":pausa 4" ":doble 150 140" ":raton 400 300" ":captura b1" ":tecla ctrl-c" \
  ":clicd 300 75" ":captura b2" ":clic 320 188" ":pausa 1" ":captura b3" ":clicd 180 585" ":captura b4" ":clic 200 421" ":pausa 1" ":captura b5" ":arrastra 743 523 643 423" \
  ":pausa 1" ":captura b6" ":clic 40 585" ":clic 80 176" ":pausa 2" ":tecla ctrl-v" ":captura b7" ":clicd 300 75" ":clic 320 221" ":pausa 1" ":captura b8" ":doble 60 40" ":pausa 2" \
  ":captura b9" ":doble 330 160" ":pausa 1" ":raton 600 440" ":captura b10" ":doble 356 310" ":pausa 2" ":captura b11" ":clicd 150 540" ":clic 200 523" ":pausa 1" ":captura b12" > $G/menus2.sal 2>&1
S=$G/menus2.sal
mira "dos clics en el icono «Consola» la abren" "escritorio: lanzo .CONSOLA.BIN (la tarea [0-9]*)"
miracap "en la Consola, dos clics seleccionan una palabra de lo que hay en la pantalla" b1 rgb 97 133 $RES rgb 206 146 $RES nrgb 213 133 $RES texto 92 128 100 136 "Hispa-Pentium:"
miracap "el botón secundario en el título de una ventana: Restaurar, Mover, Tamaño (MENÚ), Minimizar, Maximizar y Cerrar" b2 texto 310 80 318 88 Restaurar texto 310 104 318 112 Mover \
  texto 310 128 318 136 "Tamaño" texto 310 152 318 160 Minimizar texto 310 176 318 184 Maximizar texto 310 209 318 217 Cerrar
miracap "«Maximizar», desde ese menú, la maximiza" b3 negrita 24 2 36 10 Consola rgb 797 300 1A5AB6 notexto 300 70 330 200 Minimizar
miracap "el botón secundario en su botón de la barra de tareas saca el mismo menú (hacia arriba)" b4 texto 190 409 198 417 Restaurar texto 190 542 198 550 Cerrar
miracap "y «Restaurar» la devuelve a su tamaño" b5 negrita 112 64 124 72 Consola rgb 780 300 154C9C 60 nrgb 780 300 0C1626
miracap "la Consola también cambia de tamaño por la esquina (su pantalla sigue siendo de 80 x 25: lo que no cabe no se ve)" b6 rgb 600 300 0C1626 nrgb 700 300 0C1626 \
  nrgb 400 480 0C1626 negrita 112 64 124 72 Consola
miracap "lo copiado en la Consola (Ctrl+C con algo seleccionado) se pega en el Bloc de notas" b7 texto 122 172 130 180 "Hispa-Pentium:"
miracap "«Cerrar», en el menú de la ventana, la cierra: ni ventana ni botón en la barra" b8 notexto 100 60 200 80 Consola notexto 120 570 300 582 Consola \
  negrita 140 90 152 98 "NOTAS.TXT - Bloc de notas"
miracap "dos clics en «Mi equipo»: el explorador, con la unidad C: (el disco FAT16) en su mosaico y lo que tiene libre" b9 negrita 30 10 40 20 "Mi equipo" \
  negrita 265 133 275 143 "HISPA (C:)" texto 313 151 323 161 "libres de"
miracap "dos clics en la unidad: sus archivos y sus carpetas" b10 negrita 30 10 40 20 "C:\\" texto 316 322 326 332 LEEME.TXT texto 428 166 438 176 VACIA
miracap "y dos clics en un texto lo abren con el Bloc de notas" b11 negrita 198 142 206 150 "\\LEEME.TXT - Bloc de notas" \
  texto 178 224 186 232 "Hispa-Pentium: un núcleo de 32 bits escrito en Castel."
miracap "«Acerca de Hispa-Pentium», desde el menú del tapiz: la versión y la licencia" b12 negrita 206 158 214 166 "Acerca de Hispa-Pentium" negrita 276 196 284 204 "Hispa-Pentium $VERSION_BASE" \
  texto 196 294 204 302 "Copyright (C) 2026 Carlos Sanz y contribuidores."
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# a la medida de un netbook (1024 x 600); llevar un icono a otro sitio y organizarlos; mover una ventana desde su menú
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":captura c1" ":arrastra 60 190 500 300" \
  ":raton 700 100" ":captura c2" ":clicd 600 200" ":clic 620 265" ":pausa 1" ":clic 620 313" ":captura c3" ":clicd 600 200" ":clic 620 215" ":pausa 1" ":captura c4" ":clicd 400 95" ":clic 420 131" \
  ":raton 460 233" ":clic 460 233" ":raton 700 400" ":captura c5" > $G/medida.sal 2>&1
S=$G/medida.sal
mira "con «modo=1024x600» en la línea del cargador, el escritorio arranca a esa medida" "^escritorio: en marcha, 1024 x 600 con 32 bits por punto"
miracap "a 1024 x 600 (la pantalla de un netbook): la barra de tareas abajo, de lado a lado, el reloj a la derecha y los iconos" c1 tam 1024 600 negrita 30 570 46 580 Inicio \
  rgb 1019 597 3393E0 12 rgb 600 598 1D5DBA 10 texto 24 50 32 58 "Mi equipo" negrita 854 505 869 520 Hispa-Pentium
# (ICONOS) arrastrado, va a la celda más cercana de la rejilla; «Organizar iconos...» es un submenú: «Como al principio»
miracap "un icono del tapiz se lleva a otro sitio arrastrándolo (a la celda de la rejilla más cercana)" c2 texto 480 278 488 286 Consola notexto 20 196 60 214 Consola texto 24 50 32 58 "Mi equipo"
miracap "«Organizar iconos...» > «Como al principio» los devuelve a su sitio" c3 texto 32 202 40 210 Consola notexto 470 270 500 300 Consola
miracap "Configuración dice la medida de la pantalla" c4 texto 300 135 320 145 "Pantalla:  1024 x 600, 32 bits por punto"
miracap "«Mover», en el menú de una ventana: la flecha va a su título y la ventana la sigue hasta el clic siguiente" c5 negrita 298 185 306 193 "Configuración" \
  notexto 255 80 270 95 "Configuración"
# (explorador, 21/11) el explorador de archivos (EQUIPO.BIN: el árbol, las
# vistas, las miniaturas con MINIAT.BIN, copiar y pegar, cambiar el nombre,
# carpeta nueva, la Papelera, el menú del botón secundario, elegir varios,
# buscar) y el dock (soltar en el tapiz y en el dock, el punto de los
# abiertos, su menú, ocultarlo); el disco lo miran luego mtools y fsck.fat
echo "== Hispa-Pentium: el explorador de archivos y el dock =="
cp $P/d16.img $G/ex.img
export MTOOLS_SKIP_CHECK=1
mmd -i $G/ex.img ::FOTOS
python3 - $G <<'PY'
import struct, sys, zlib, base64
g = sys.argv[1]
def png(w, h, f):
    raw = b''.join(b'\0' + b''.join(bytes(f(x, y)) for x in range(w)) for y in range(h))
    ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
open(g + '/FLOR.PNG', 'wb').write(png(160, 120, lambda x, y: (255, 122, 51) if (x - 80) ** 2 + (y - 60) ** 2 < 35 ** 2 else (47, 116, 208)))
w, h = 64, 96
filas = b''.join(bytes((255, 255, 255) if 10 <= x < 54 and 56 <= y < 86 else (0x4A, 0x29, 0x14)) * 1 for y in range(h) for x in range(w))
bmp = b'BM' + struct.pack('<IHHI', 54 + len(filas), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(filas), 2835, 2835, 0, 0) + filas
open(g + '/CASA.BMP', 'wb').write(bmp)
open(g + '/PAISAJE.JPG', 'wb').write(base64.b64decode('/9j/4AAQSkZJRgABAQAAAAAAAAD/2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKSj/2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCj/wAARCAAgADADASIAAhEBAxEB/8QAFgABAQEAAAAAAAAAAAAAAAAAAAYH/8QAFhAAAwAAAAAAAAAAAAAAAAAAABRh/8QAFwEBAQEBAAAAAAAAAAAAAAAAAAYFB//EABcRAQEBAQAAAAAAAAAAAAAAAAATFGH/2gAMAwEAAhEDEQA/ANQfofpKP0P04rgWkVW/Q/SUfofowEVW/Q/SUfofowEUo/Q/SUfofpa4ONWKrfofpKP0P0YOEVW/Q/SUfofowcIv/9k='))
PY
mcopy -i $G/ex.img $G/CASA.BMP $G/FLOR.PNG $G/PAISAJE.JPG ::FOTOS/
miniat=no
if bash $AQUI/nucleo32/miniat/hacer.sh $G/MINIAT.BIN > $G/miniat.txt 2>&1; then mcopy -i $G/ex.img $G/MINIAT.BIN ::; miniat=si; fi
SES=(":pausa 2" ":captura ex1" ":doble 60 40" ":pausa 3" ":captura ex2" ":doble 330 160" ":pausa 2" ":raton 600 440" ":captura ex3"
 ":tecla alt-v" ":tecla down down down down ret" ":pausa 1" ":captura ex4"
 ":tecla ret" ":pausa 1" ":tecla alt-v" ":tecla down ret" ":pausa 6" ":captura ex5"
 ":tecla alt-v" ":tecla down down ret" ":tecla backspace" ":pausa 1"
 ":clic 452 310" ":tecla ctrl-c" ":doble 548 155" ":pausa 1" ":tecla ctrl-v" ":pausa 2" ":raton 600 440" ":captura ex6"
 ":clic 260 160" ":tecla f2" ":pausa 1" ":captura ex7" ":escribe NUEVO.TXT" ":tecla ret" ":pausa 1" ":tecla f7" ":pausa 1" ":captura ex8"
 ":tecla ret" ":pausa 1" ":raton 600 440" ":captura ex9" ":clic 260 160" ":tecla delete" ":pausa 2" ":raton 600 440" ":captura ex10"
 ":clic 84 280" ":pausa 2" ":raton 600 440" ":captura ex11" ":clic 260 160" ":tecla alt-h" ":tecla down down ret" ":pausa 2" ":captura ex12"
 ":tecla f4" ":escribe C:\\" ":tecla ret" ":pausa 1" ":clicd 452 310" ":pausa 1" ":captura ex13" ":tecla esc"
 ":clic 260 260" ":ctrlclic 356 260" ":pausa 1" ":captura ex14" ":arrastra 600 455 380 290" ":pausa 1" ":captura ex15"
 ":clic 452 310" ":arrastra 452 310 120 540" ":pausa 2" ":clic 260 410" ":arrastra 260 410 400 535" ":pausa 2" ":captura ex16"
 ":tecla f3" ":escribe pir" ":tecla ret" ":pausa 3" ":captura ex17" ":tecla meta_l-d" ":pausa 1" ":doble 176 420" ":pausa 3" ":captura ex18" ":arrastra 292 530 400 530" ":pausa 1"
 ":clicd 320 530" ":pausa 1" ":captura ex19" ":clic 380 513" ":pausa 1" ":captura ex20")
CAPTURAS=$G ESCRIBE=1 DISCO=$G/ex.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" "${SES[@]}" > $G/explorador.sal 2>&1
S=$G/explorador.sal
miracap "el dock: centrado abajo, encima de la barra de tareas, con el borde del panel" ex1 rgb 400 504 0F3F8A 20 negrita 30 570 46 580 Inicio
miracap "«Mi equipo» es el explorador: el árbol (Mi equipo), la unidad C: en su mosaico, con lo libre, y la Papelera" ex2 negrita 30 10 40 20 "Mi equipo" \
  negrita 265 133 275 143 "HISPA (C:)" negrita 515 133 525 143 "CD (D:)" texto 515 151 525 161 "sin disco" negrita 265 195 275 205 Papelera texto 313 151 323 161 "libres de" texto 48 127 58 137 "Mi equipo"
miracap "dos clics en la unidad: sus carpetas y sus archivos, en iconos, y lo libre en la barra de estado" ex3 texto 236 166 246 176 FOTOS \
  texto 412 322 422 332 LEEME.TXT texto 687 475 697 485 "Libre:"
miracap "la vista de detalles: la rejilla de los controles, con su nombre, su tamaño, su tipo y su fecha (las carpetas, delante)" ex4 \
  negrita 221 127 231 137 Nombre negrita 629 127 639 137 Modificado texto 477 177 487 187 Carpeta texto 477 249 487 259 Programa
if [ $miniat = si ]; then
  miracap "las miniaturas (MINIAT.BIN, con stb_image): un BMP, un PNG y un JPG en pequeño, con su nombre" ex5 texto 344 204 354 214 FLOR.PNG \
    texto 444 204 454 214 PAISAJE.JPG rgb 381 170 FF7A33 24 rgb 270 150 FFFFFF 10 nrgb 493 170 FFFFFF 30
else ko "no se ha podido hacer MINIAT.BIN (¿gcc con -m32?): $(tail -n 1 $G/miniat.txt)"; fi
miracap "copiar (Ctrl+C) en una carpeta y pegar (Ctrl+V) en otra: el archivo, y «Hecho» en la barra de estado" ex6 texto 220 166 230 176 LEEME.TXT \
  texto 159 475 169 485 "copiados 1"
miracap "F2: el cuadro para cambiar el nombre, con el que tiene" ex7 negrita 187 170 197 180 Cambiar texto 194 225 204 235 LEEME.TXT texto 191 253 201 263 "Hasta 8 letras"
miracap "F7: la carpeta nueva, con su nombre (CARPETA)" ex8 texto 194 225 204 235 CARPETA
miracap "el nombre cambiado y la carpeta nueva, en la carpeta" ex9 texto 228 166 238 176 CARPETA texto 316 166 326 176 NUEVO.TXT
miracap "Supr manda lo elegido a la Papelera (y lo dice)" ex10 texto 220 166 230 176 NUEVO.TXT notexto 210 160 400 180 CARPETA texto 207 475 217 485 Papelera
miracap "la Papelera, en el árbol: lo que se ha tirado" ex11 negrita 30 10 40 20 Papelera texto 228 166 238 176 CARPETA
miracap "Herramientas > Restaurar lo elegido: lo devuelve a su sitio, y la Papelera queda vacía" ex12 texto 217 135 227 145 "La Papelera est"
miracap "el botón secundario en un archivo: Abrir, Abrir con..., Mandar a la Papelera, Propiedades..." ex13 texto 461 338 471 348 "Abrir con..." \
  texto 461 452 471 462 "Mandar a la Papelera" texto 461 509 471 519 Propiedades
miracap "Control y un clic eligen dos" ex14 texto 111 475 121 485 "2 elegidos"
miracap "y un recuadro con el ratón, los que toca" ex15 texto 111 475 121 485 "7 elegidos"
mira "arrastrar un archivo al tapiz deja en él un acceso" "^escritorio: un acceso nuevo en el tapiz: C:.LEEME.TXT"
# (ICONOS) soltado debajo de la zona de los iconos (a la altura del dock), el acceso va a la celda libre más cercana (120, 392), detrás de la ventana
mira "el acceso, en la zona de los iconos: a la celda libre más cercana (no a la altura del dock)" "escritorio: iconos (acceso nuevo): .* LEEME.TXT@120,392/1;"
miracap "un programa soltado en el dock queda fijado" ex16 texto 615 529 625 539 "Fijado en el dock."
miracap "buscar (F3) «pir» en la carpeta y las de dentro: lo encontrado, con su carpeta" ex17 negrita 236 172 256 192 PIRATA.TXT texto 290 172 560 192 "C:\\TEXTOS" \
  texto 103 475 113 485 "Encontrados: 2."
miracap "dos clics en el acceso del tapiz (con Logo+D, que enseña el escritorio) lo abren con lo suyo (EQUIPO /ABRIR: el Bloc de notas)" ex18 negrita 149 89 159 99 "LEEME.TXT"
miracap "el botón secundario en el dock: Abrir, Abrir otra ventana, Fijar, Quitar, Cerrar y Ocultar el dock" ex19 texto 329 501 339 511 "Ocultar el dock"
miracap "«Ocultar el dock» lo quita (y lo de debajo se ve)" ex20 nrgb 400 504 0F3F8A 20
saca $G/ex.img VACIA/NUEVO.TXT $G/nuevo.txt; saca $G/ex.img LEEME.TXT $G/leeme.txt
if cmp -s $G/nuevo.txt $G/leeme.txt && mdir -i $G/ex.img ::VACIA/CARPETA > /dev/null 2>&1; then ok "en el disco: la copia, con su nombre nuevo, igual que el original, y la carpeta restaurada en su sitio"
else ko "en el disco no está lo que se ha hecho en el explorador:"; mdir -i $G/ex.img ::VACIA 2>&1 | head -6 | sed 's/^/        /'; fi
saca $G/ex.img PAPELERA/INDICE.TXT $G/indice.txt
if [ -f $G/indice.txt ] && [ ! -s $G/indice.txt ] && mattrib -i $G/ex.img ::PAPELERA 2>/dev/null | grep -q "H"; then ok "la Papelera (\\PAPELERA, oculta) queda con su índice vacío"
else ko "la Papelera no ha quedado bien: $(cat $G/indice.txt 2>/dev/null | head -2)"; fi
saca $G/ex.img ATAJOS.INI $G/atajos.ini; saca $G/ex.img DOCK.INI $G/dock.ini
if grep -q "^atajo=C:.LEEME.TXT|LEEME.TXT|" $G/atajos.ini 2>/dev/null && grep -q "^fijado=.MINIAT.BIN||MINIAT.BIN|" $G/dock.ini 2>/dev/null && grep -q "^visible=no" $G/dock.ini \
  && [ "$(grep "^fijado=" $G/dock.ini | sed -n 4p | cut -d'|' -f1)" = 'fijado=\BLOC.BIN' ]; then
  ok "ATAJOS.INI guarda el acceso del tapiz, y DOCK.INI el orden (el Bloc, arrastrado al cuarto sitio), el programa fijado y que está oculto"
else ko "ATAJOS.INI o DOCK.INI no guardan lo que tocaba:"; cat $G/atajos.ini $G/dock.ini 2>&1 | head -12 | sed 's/^/        /'; fi
if sano $G/ex.img; then ok "y el disco, sano (fsck.fat)"; else ko "el disco ha quedado mal:"; sed 's/^/        /' $P/fsck.txt | head -5; fi
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# la orden ESCRITORIO del intérprete, y abrir un documento desde el menú ((PUNTEROS)
# la flecha se aparta antes de la captura: en el Bloc es la «I», que tapaba el texto)
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" escritorio ":pausa 2" ":clic 40 585" ":clic 300 171" ":pausa 1" ":clic 500 171" ":pausa 2" \
  ":raton 700 530" ":captura o1" ":clic 40 585" ":clic 360 545" ":clic 533 327" ":captura o2" > $G/orden.sal 2>&1
S=$G/orden.sal
mira "la orden ESCRITORIO del intérprete de órdenes (por la llamada al sistema) pone en marcha el escritorio, y ese intérprete acaba" \
  "^C:.>escritorio$" "^escritorio: en marcha, 800 x 600" "^El escritorio está en marcha\.$" "^ORDENES.BIN ha acabado con el código 0"
miracap "un documento del menú Inicio se abre con el Bloc de notas (LEEME.TXT, leído del disco)" o1 negrita 112 64 124 72 "LEEME.TXT - Bloc de notas" \
  texto 94 146 102 156 "Hispa-Pentium: un núcleo de 32 bits escrito en Castel."
miracap "«Cancelar» cierra el cuadro de apagar y todo sigue" o2 notexto 200 180 240 210 "Apagar el equipo" negrita 112 64 124 72 "LEEME.TXT - Bloc de notas"
# otros modos: 1024x768, y una pantalla de 24 bits por punto (se dibuja en 32 y se vuelca)
CAPTURAS=$G DISCO=$P/d16.img ARRANQUE="escritorio modo=1024x768 bits=24 tapiz=1" PANTALLA=1024x768 MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  ":clic 40 753" ":raton 600 300" ":captura m1" > $G/modo.sal 2>&1
S=$G/modo.sal
mira "con «modo=1024x768 bits=24 tapiz=1» en la línea del cargador, ese modo y ese tapiz" "^escritorio: en marcha, 1024 x 768 con 24 bits por punto .*, tapiz 1"
miracap "a 1024x768 y con 24 bits por punto se ve igual: la barra abajo, el menú Inicio y el tapiz «Azul»" m1 tam 1024 768 negrita 30 738 46 748 Inicio \
  negrita 46 323 54 331 "Bloc de notas" rgb 500 3 16468E 8
# sin tarjeta gráfica que valga, se dice y la consola sigue
VGA=none $AQUI/nucleo32/probar.sh "" escritorio hora apagar > $G/sinvga.sal 2>&1
S=$G/sinvga.sal
mira "sin modo gráfico (ni del cargador ni VBE de Bochs), «escritorio» lo dice y la consola del núcleo sigue" \
  "^pantalla: no hay modo gráfico" "^no se puede poner en marcha el escritorio" "^llevo [0-9]* segundos" "^\[código 5\]"
# el núcleo que le pide el modo gráfico al cargador (para GRUB, en los PC de verdad)
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $G/semilla.asm > /dev/null 2>&1 && nasm -f bin $G/semilla.asm -o $G/SEMILLA.BIN
python3 -I $AQUI/nucleo32/modo_grafico.py $G/SEMILLA.BIN $G/GRAFICO.BIN 1024 768 32 > $G/mg.txt 2>&1
if python3 -I $AQUI/nucleo32/modo_grafico.py --mira $G/SEMILLA.BIN $G/GRAFICO.BIN 1024 768 32 >> $G/mg.txt 2>&1; then
  ok "modo_grafico.py: la copia del núcleo pide al cargador 1024x768x32 en su cabecera multiboot (bandera de vídeo y suma al día); lo demás, igual"
else ko "modo_grafico.py no deja bien la cabecera multiboot: $(tail -n 1 $G/mg.txt)"; fi
# la memoria USB con GRUB (hace falta ser root, y grub-install para PC)
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
  if $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1; then
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$G/usb.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    monu() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$G/monu > /dev/null 2>&1; sleep "${2:-0.3}"; }
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; monu "sendkey ret" 12
    monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8; monu "mouse_move 300 -200" 0.6
    monu "screendump $G/u1.ppm" 0.6
    monu "mouse_move -130 -105" 0.2; monu "mouse_move -130 -104" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5; monu "mouse_move 400 200" 0.6
    monu "screendump $G/u2.ppm" 0.6
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $G/usb.sal | iconv -f CP437 -t UTF-8 > $G/usb.txt2
    S=$G/usb.txt2
    # (SSH, TAILSCALE: SSH, SSHD, WG y TAILSCAL también van como módulos; (SONIDO) y SONIDO.BIN con sus cuatro .WAV: 31;
    # (PANEL) y PANEL.BIN y HORARED.BIN: 41; USUARIOS, ARGON2 y RATON: 44; (BLUETOOTH) y BT.BIN: 45,
    # los mismos que la entrada «sin tocar el USB» de la 06)
    mira "la memoria USB con GRUB (usb.sh): arranca el núcleo que pide el modo gráfico, con los programas también como módulos (M:) y la memoria como unidad X:" \
      "^memoria: 64 MB, .* 45 programas de usuario" "^unidades: M: (en memoria) X: (disco 0, USB, FAT16); la del sistema, X:" \
      "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
    miracap "y el escritorio sale en el modo que ha puesto GRUB: la barra, el menú Inicio y el documento que ha venido como módulo" u1 tam 800 600 \
      negrita 30 570 46 580 Inicio negrita 46 155 54 163 "Bloc de notas" negrita 256 159 264 167 "Mis documentos"
    if grep -q "escritorio: lanzo .BLOC.BIN" $S && cap $G/u2.ppm negrita 112 64 124 72 "NOTAS.TXT - Bloc de notas"; then
      ok "y desde su menú se lanza el Bloc de notas (de la memoria USB, la unidad del sistema)"
    else ko "en la memoria USB, el Bloc de notas no se lanza desde el menú:"; sed 's/^/        /' $G/cap.txt | head -3; fi
    # la entrada de 1024 x 600 (la tercera): el modo lo pide la cabecera de ESCRIT6.BIN
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$G/usb6.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; monu "sendkey down"; monu "sendkey down"; monu "sendkey ret" 12
    monu "screendump $G/u3.ppm" 0.6
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $G/usb6.sal | iconv -f CP437 -t UTF-8 > $G/usb6.txt
    S=$G/usb6.txt
    mira "la memoria USB, con la entrada de 1024 x 600: GRUB pone ese modo (el de la cabecera de esa copia del núcleo) y el escritorio sale a esa medida" \
      "^escritorio: en marcha, 1024 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
    miracap "y se ve entero: la barra abajo y los iconos" u3 tam 1024 600 negrita 30 570 46 580 Inicio texto 24 50 32 58 "Mi equipo"
    MTOOLS_SKIP_CHECK=1 mtype -i $G/usb.img@@1M ::/boot/grub/grub.cfg > $G/grub.cfg 2> /dev/null
    if grep -q "a la medida de la pantalla" $G/grub.cfg && grep -q "^  set gfxpayload=auto$" $G/grub.cfg && grep -q "^  videoinfo$" $G/grub.cfg \
      && [ "$(grep -c "^  multiboot /ESCRIT" $G/grub.cfg)" = 6 ] && grep -q "multiboot /ESCRIT13.BIN" $G/grub.cfg \
      && [ "$(grep -c "escritura=usb unidad=X registro" $G/grub.cfg)" = 7 ] && grep -q "multiboot /ESCRIT8.BIN escritorio sindisco sinusb sinred sinacpi$" $G/grub.cfg; then
      ok "el menú de GRUB: el escritorio a 800 x 600, «a la medida de la pantalla» (gfxpayload=auto), a 1024 x 600, 1366 x 768 y 1024 x 768, uno sin tocar el USB ni los discos, y los modos de vídeo de la BIOS (videoinfo); las demás, con «escritura=usb unidad=X registro»"
    else ko "el menú de GRUB de la memoria USB no trae las entradas nuevas"; fi
  else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la memoria USB con GRUB)"
fi
fi   # SOLO=06, SOLO=07

# ------------------------------------------------- 9. Hispa-Pentium 0.6
# Los discos SATA (AHCI), las unidades con letra (varias a la vez) y el USB
# (xHCI, EHCI y UHCI): teclado, ratón, concentradores y memorias. QEMU da la
# controladora AHCI (-device ahci), las de USB (qemu-xhci, usb-ehci, las UHCI) y sus
# aparatos; con -machine pc,i8042=off no hay teclado ni ratón PS/2, como en
# un Mac, y lo que se teclea por el monitor llega por el teclado USB.
G=$P/graf
N=$P/n06
mkdir -p $G $N
if [ "${SOLO:-}" != 07 ] && [ "${SOLO:-}" != 08 ]; then
echo "== Hispa-Pentium 0.6: los discos SATA (AHCI), las unidades con letra, el permiso de escritura por disco =="
hechos=1
for c in 16 32 mbr; do
  [ -f $P/d$c.img ] || $AQUI/nucleo32/disco.sh $P/d$c.img $c > $P/disco$c.txt 2>&1 || { hechos=0; ko "no se puede hacer el disco de prueba ($c)"; }
done
if [ $hechos = 1 ]; then
suma=$(python3 -c "print(sum(sum(('linea %03d de un archivo de varios clusters\r\n' % i).encode()) for i in range(1, 401)))")
# 1. un disco SATA: leer
SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar prueba.bin" :espera "lanzar textos\\eco2.bin hola" apagar > $N/sata.sal 2>&1
S=$N/sata.sal
mira "AHCI: la controladora SATA se encuentra en el bus PCI, IDENTIFY dice el disco, y la FAT se monta encima (C:)" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16 (SATA)\.$" "^disco 0: QEMU HARDDISK, 32768 sectores (16 MB), LBA, SATA" \
  "^sistema de archivos: FAT16, desde el sector 0, " "^unidades: C: (disco 0, SATA, FAT16); la del sistema, C:"
mira "AHCI: se leen sectores por DMA: una carpeta, un archivo de varios clusters (su suma, la de verdad) y programas" "^LARGO.TXT  *17600$" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
  "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$" "^prueba.bin ha acabado con el código 7" "^hola$" "^eco2.bin ha acabado con el código 1"
# 1b. lo que ahci.cas apunta para un PC de verdad (la orden «disco» y ARRANQUE.TXT), y
# los caminos de las averías que en QEMU no se dan: PI a 0 (se miran los puertos de CAP),
# unos registros que no empiezan en una página (la ABAR del ICH7 va a 1 KB) y la segunda
# vuelta, reiniciando la controladora y cada puerto (COMRESET), si a la primera no sale el disco
S=$N/sata.sal
mira "AHCI: la orden «disco» dice lo que se ha visto: la controladora, CAP/PI/VS y cada puerto (SSTS, TFD, firma)" \
  "^ahci: controladora 00:0[0-9A-F]\.0, 8086:2922, orden 0107, ABAR " "^ahci: CAP [0-9A-F]*, GHC 80000000, PI 0000003F, VS 00010000" \
  "^ahci: puerto 0: SSTS 00000113, CMD [0-9A-F]*, TFD 00000050, SIG 00000101$" "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$"
ARRANQUE="ahci=pi0 ahci=desalineada ahci=reinicio" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataav.sal 2>&1
S=$N/sataav.sal
mira "AHCI, averías: con PI a 0, con los registros a media página y reiniciando la controladora (GHC.HR y COMRESET), el disco sale igual" \
  "^ahci: prueba de la base desalineada ([0-9A-F]*100): bien$" "^ahci: PI vale 0: se miran los puertos que dice CAP (0000003F)$" \
  "^ahci: ningún disco: se reinicia la controladora$" "^ahci: puerto 0: SSTS 00000113, CMD [0-9A-F]*, TFD 0000007F, SIG FFFFFFFF$" \
  "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" "^disco 0: QEMU HARDDISK, 16 MB, FAT16 (SATA)\.$" "^LEEME.TXT  *118$"
# 1c. el orden de la norma (AHCI 1.3, 10.1.2), lo que no le salía al ICH7 del
# Toshiba: FRE = 1 antes de mirar TFD y, si el disco sigue ocupado, el COMRESET
# con FRE = 1 y luego ST = 1 («ahci=ocupado»: como si TFD siguiera en 80h); y la
# segunda vuelta del ICH6/ICH7 («ahci=ich7»), sin GHC.HR: solo el COMRESET de cada puerto
ARRANQUE="ahci=ocupado" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataoc.sal 2>&1
S=$N/sataoc.sal
l1=$(grep -n "^ahci: puerto 0: tras FRE=1: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000, CMD [0-9A-F]*, SIG 00000101$" $S | head -1 | cut -d: -f1)
l2=$(grep -n "^ahci: puerto 0: tras COMRESET: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000" $S | head -1 | cut -d: -f1)
l3=$(grep -n "^ahci: puerto 0: tras ST=1: SSTS 00000113, TFD [0-9A-F]*, SERR 00000000, CMD [0-9A-F]*[13579BDF], SIG 00000101$" $S | head -1 | cut -d: -f1)
if [ -n "$l1" ] && [ -n "$l2" ] && [ -n "$l3" ] && [ "$l1" -lt "$l2" ] && [ "$l2" -lt "$l3" ] && grep -q "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" $S \
  && grep -q "^LEEME.TXT  *118$" $S && ! grep -q "se reinicia la controladora" $S; then
  ok "AHCI, el orden de la norma: FRE=1 antes de mirar TFD; con el disco ocupado, COMRESET (con FRE=1) y luego ST=1; cada paso, apuntado (SSTS, TFD, SERR); el disco sale y se lee"
else ko "AHCI: el orden FRE=1, COMRESET, ST=1 no sale así ($l1, $l2, $l3):"; grep "^ahci: puerto 0" $S | sed 's/^/        /' | head -6; fi
ARRANQUE="ahci=reinicio ahci=ich7" SATA=$P/d16.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir" apagar > $N/sataich7.sal 2>&1
S=$N/sataich7.sal
if grep -q "^ahci: ningún disco: COMRESET de cada puerto (en el ICH6/ICH7, sin reiniciar la controladora)$" $S && ! grep -q "se reinicia la controladora\|^ahci: reiniciada" $S \
  && grep -q "^ahci: puerto 0: tras COMRESET: SSTS 00000113" $S && grep -q "^ahci: puerto 0: disco QEMU HARDDISK, 16 MB$" $S && grep -q "^LEEME.TXT  *118$" $S; then
  ok "AHCI, la segunda vuelta en un ICH6/ICH7: sin GHC.HR (que en el Toshiba deja los puertos sin enlace), solo el COMRESET de cada puerto; el disco sale"
else ko "AHCI: la segunda vuelta del ICH7 no va como debe:"; grep "^ahci:" $S | sed 's/^/        /' | head -8; fi
# 2. escribir en él (FAT32 y la FAT16 de una partición), y que quede sano
for c in 32 mbr; do
  case $c in 32) que="FAT32" ;; mbr) que="FAT16 en una partición" ;; esac
  I=$N/s$c.img; cp $P/d$c.img $I
  M=$I; s0=; [ $c = mbr ] && { M=$I@@32256; s0=63; }
  ESCRIBE=1 SATA=$I MODULOS= $AQUI/nucleo32/probar.sh "" ordenes escribe "echo Querida señora: ¿qué tal está? > maría.txt" "copy textos\\largo.txt copia.txt" \
    "md obra" "copy *.bat obra" "del obra\\lote.bat" salir disco apagar > $N/sataw$c.sal 2>&1
  S=$N/sataw$c.sal
  mira "AHCI, $que: se escribe por DMA: un programa crea, escribe, corta, alarga y borra archivos y carpetas, y todo queda como estaba" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" \
    "^ *3 archivo(s) copiado(s)\.$" "; archivos abiertos: 0$" "^\[código 5\]"
  if sano $I $s0; then ok "AHCI, $que: fsck.fat da el disco por sano ($(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "AHCI, $que: fsck.fat encuentra el disco mal:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  saca $M TEXTOS/LARGO.TXT $N/largo.txt; saca $M COPIA.TXT $N/copia.txt; saca $M MARIA.TXT $N/maria.txt
  if cmp -s $N/largo.txt $N/copia.txt && [ "$(iconv -f CP437 -t UTF-8 $N/maria.txt | tr -d '\r')" = "Querida señora: ¿qué tal está?" ] \
    && MTOOLS_SKIP_CHECK=1 mdir -i $M ::OBRA 2> /dev/null | grep -q "^SINFIN  *BAT" && ! MTOOLS_SKIP_CHECK=1 mdir -i $M ::OBRA 2> /dev/null | grep -q "^LOTE "; then
    ok "AHCI, $que: mtools ve lo escrito: lo copiado, igual que el original; lo tecleado, con sus letras; lo borrado, borrado"
  else ko "AHCI, $que: lo escrito no está bien en el disco"; fi
done
# 3. sin permiso no se escribe ni un sector
cp $P/d16.img $N/sl.img
SOLO_LECTURA=1 ESCRIBE=1 SATA=$N/sl.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "echo hola > nuevo.txt" "del leeme.txt" "md cosas" \
  "type leeme.txt" hola salir apagar > $N/sl.sal 2>&1
S=$N/sl.sal
mira "AHCI: sin «escritura» en la línea del cargador, el disco SATA es de solo lectura" \
  "^el disco es de solo lectura (para escribir en él: «escritura» en la línea del cargador)" "^No puedo escribir en nuevo.txt: es de solo lectura\.$" \
  "^No puedo borrar leeme.txt: es de solo lectura\.$" "^Este archivo está en un disco FAT" "^1 + 2 + ... + 1000 = 500500"
if cmp -s $P/d16.img $N/sl.img; then ok "AHCI: y la imagen queda idéntica, octeto a octeto: no se ha escrito ni un sector"
else ko "AHCI: sin permiso de escritura, el disco ha cambiado"; fi
# 4. un disco con tabla de particiones GPT (la de los discos de ahora)
if command -v parted > /dev/null; then
  truncate -s 20M $N/gpt.img
  parted -s $N/gpt.img mklabel gpt mkpart datos 2048s 34815s > /dev/null 2>&1
  dd if=$P/d16.img of=$N/gpt.img bs=512 seek=2048 conv=notrunc status=none
  SATA=$N/gpt.img MODULOS= $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar hola.bin" apagar > $N/gpt.sal 2>&1
  S=$N/gpt.sal
  mira "un disco con tabla de particiones GPT: se encuentra su partición y la FAT se monta en ella" \
    "^disco 0: QEMU HARDDISK, 20 MB, FAT16 (SATA)\.$" "^sistema de archivos: FAT16, desde el sector 2048, " "^PIRATA.TXT  *110$" "^hola.bin ha acabado con el código 3"
else
  echo "  (sin parted no se prueba la tabla de particiones GPT)"
fi
# 5. dos discos a la vez: el IDE (C:) y el SATA (D:)
cp $P/d16.img $N/c.img; cp $P/d32.img $N/d.img
ESCRIBE=1 DISCO=$N/c.img SATA=$N/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "dir d:\\textos" "copy c:\\textos\\pirata.txt d:\\poema.txt" \
  "d:" "type poema.txt" "md nueva" "cd nueva" "copy c:\\leeme.txt" "dir" "c:" "type d:\\nueva\\leeme.txt" "z:" "ren leeme.txt d:\\otro.txt" \
  "d:\\textos\\eco2 desde la otra" salir disco apagar > $N/dos.sal 2>&1
S=$N/dos.sal
mira "dos discos a la vez, cada uno con su letra: C: (el IDE) y D: (el SATA), y la FAT con dos volúmenes montados" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16\.$" "^disco 1: QEMU HARDDISK, 40 MB, FAT32 (SATA)\.$" \
  "^unidades: C: (disco 0, IDE, FAT16) D: (disco 1, SATA, FAT32); la del sistema, C:"
mira "las rutas con la letra de la unidad: DIR, COPY de una a otra, TYPE y lanzar un programa de la otra unidad" \
  "^ Directorio de D:.TEXTOS$" "^ *1 archivo(s) copiado(s)\.$" "^un velero bergantín\.$" "^Este archivo está en un disco FAT" "^desde la otra$"
mira "«D:» cambia de unidad (la petición lo dice), con su carpeta de trabajo; una unidad que no hay, se dice; y de una a otra no se cambia de nombre" \
  "^D:.>type poema.txt$" "^D:.NUEVA>dir$" "^ Directorio de D:.NUEVA$" "^C:.>type d:.nueva.leeme.txt$" "^No hay unidad Z:\.$" \
  "^No puedo llamar d:.otro.txt a leeme.txt: "
if sano $N/c.img && sano $N/d.img; then ok "los dos discos quedan sanos (fsck.fat)"; else ko "con dos discos, alguno queda mal:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
saca $N/d.img POEMA.TXT $N/poema.txt; saca $N/c.img TEXTOS/PIRATA.TXT $N/pirata.txt; saca $N/d.img NUEVA/LEEME.TXT $N/leeme2.txt; saca $N/c.img LEEME.TXT $N/leeme1.txt
if cmp -s $N/poema.txt $N/pirata.txt && cmp -s $N/leeme1.txt $N/leeme2.txt; then ok "lo copiado de C: a D: es igual que el original, octeto a octeto"
else ko "lo copiado de un disco a otro no es igual"; fi

echo "== Hispa-Pentium 0.6: el USB (xHCI, EHCI y UHCI): teclado, ratón, concentradores y memorias; sin teclado ni ratón PS/2 =="
# 6. xHCI y EHCI, sin 8042: el teclado USB (el español, con sus acentos) y la memoria USB (X:), en la que se escribe
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $P/d16.img $N/u$hc.img
  ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off ARRANQUE="ordenes escritura" QEMU_MAS="-device $dev -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 -drive id=mu,file=$N/u$hc.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh "" "dir textos" "echo Querida señora: ¿qué tal está? El pingüino > maría.txt" "type maría.txt" \
    bucle :espera ":tecla ctrl-c" ":tecla up ret" :espera ":tecla ctrl-c" "copy textos\\largo.txt copia.txt" prueba :espera escribe salir disco apagar > $N/usb$hc.sal 2>&1
  S=$N/usb$hc.sal
  mira "$que: la controladora se toma, se enumeran los aparatos (teclado, ratón y memoria) y la memoria es la unidad X:" \
    "^usb: controladora $que " "($que); teclado$" "($que); ratón$" "($que); almacenamiento$" "^usb: 1 teclado, 1 ratón, 1 memoria\.$" \
    "^disco 0: QEMU    QEMU HARDDISK, 16 MB, FAT16 (USB)\.$" "^unidades: X: (disco 0, USB, FAT16); la del sistema, X:"
  mira "$que, sin teclado PS/2: el teclado USB es el español (eñe, acentos y diéresis con tecla muerta, ¿?), con Ctrl+C y las flechas" \
    "^X:.>echo Querida señora: ¿qué tal está? El pingüino > maría.txt$" "^Querida señora: ¿qué tal está? El pingüino$" "^X:.>prueba$" \
    "^bucle: doy vueltas sin fin" "^\^C$"
  if [ "$(grep -c '^bucle: doy vueltas sin fin' $S)" = 2 ]; then ok "$que: las flechas del teclado USB traen la orden de antes (la historia)"
  else ko "$que: las flechas del teclado USB no traen la orden de antes"; fi
  mira "$que: la memoria USB se lee (un archivo de varios clusters, programas) y se escribe (almacenamiento: solo a granel, SCSI)" \
    "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" \
    "; archivos abiertos: 0$" "^\[código 5\]"
  mira "$que: (0.8.10) el teclado USB empieza con Bloq Num apagado: SET_REPORT de las luces a 0 al ponerse en marcha (el Acer One 10: su firmware lo deja encendido)" \
    "^teclado: la BIOS deja Bloq Num apagado$" "^usb: teclado (aparato [0-9]*): Bloq Num apagado (SET_REPORT de las luces: bien)$"
  if sano $N/u$hc.img; then ok "$que: la memoria USB queda sana (fsck.fat: $(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "$que: fsck.fat encuentra mal la memoria USB:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  saca $N/u$hc.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/u$hc.img COPIA.TXT $N/copia.txt; saca $N/u$hc.img MARIA.TXT $N/maria.txt
  if cmp -s $N/largo.txt $N/copia.txt && [ "$(iconv -f CP437 -t UTF-8 $N/maria.txt | tr -d '\r')" = "Querida señora: ¿qué tal está? El pingüino" ]; then
    ok "$que: mtools ve en la memoria lo copiado (igual que el original) y lo tecleado (con sus letras)"
  else ko "$que: lo escrito en la memoria USB no está bien"; fi
done
# 6 bis. (0.8.10) las luces del teclado USB siguen a Bloq Num del sistema:
# con Bloq Num, SET_REPORT con el bit 0 (y el teclado numérico da números);
# sin él, a 0 otra vez
MODULOS= MAQUINA=pc,i8042=off QEMU_MAS="-device qemu-xhci,id=u -device usb-kbd,bus=u.0" \
  $AQUI/nucleo32/probar.sh "" ":pausa 1" ":tecla num_lock" ":pausa 1" ":tecla kp_4 kp_2 ret" ":tecla num_lock" ":pausa 1" apagar > $N/luces.sal 2>&1
S=$N/luces.sal
mira "(0.8.10) las luces del teclado USB siguen a Bloq Num: encendido, SET_REPORT con 01h (y 4 y 2 del teclado numérico dan «42»); apagado, 00h" \
  "^usb: teclado (aparato 0): Bloq Num apagado (SET_REPORT de las luces: bien)$" "usb: teclado (aparato 0): luces 01h " "^42$" "usb: teclado (aparato 0): luces 00h "
# 7. detrás de un concentrador (aparatos de USB 1.1), con el xHCI
ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off QEMU_MAS="-device qemu-xhci,id=u -device usb-hub,bus=u.0,port=1 -device usb-kbd,bus=u.0,port=1.1 -device usb-mouse,bus=u.0,port=1.2 -drive id=mu,file=$P/d16.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,port=1.3,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "lanzar prueba.bin" :espera "lanzar hola.bin" apagar > $N/hub.sal 2>&1
S=$N/hub.sal
mira "un concentrador USB: se enciende, se reinician sus puertos y se enumera lo que cuelga de él (teclado, ratón y memoria, a velocidad plena)" \
  ": concentrador de 8 puertos$" "velocidad plena, en el puerto 1 del concentrador 0; teclado$" "en el puerto 2 del concentrador 0; ratón$" \
  "en el puerto 3 del concentrador 0; almacenamiento$" "^usb: 1 teclado, 1 ratón, 1 memoria\.$"
mira "y detrás del concentrador van el teclado (se teclea por él) y la memoria (se leen sus archivos y sus programas)" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" "^hola.bin ha acabado con el código 3" "^\[código 5\]"
# 7 bis. un EHCI con sus compañeras UHCI (los PC de 2002 a 2010): el teclado y el ratón, de USB 1.1, se ceden a ellas; la memoria va por el EHCI
cp $P/d16.img $N/comp.img
ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off ARRANQUE="ordenes escritura" QEMU_MAS="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2 -device usb-kbd,bus=u.0,port=1,usb_version=1 -device usb-mouse,bus=u.0,port=3,usb_version=1 -drive id=mu,file=$N/comp.img,format=raw,if=none -device usb-storage,bus=u.0,port=2,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "copy textos\\largo.txt copia.txt" "echo El pingüino de la señora > a.txt" "type a.txt" prueba :espera salir disco apagar > $N/comp.sal 2>&1
S=$N/comp.sal
mira "un EHCI con dos UHCI compañeras: lo que no es de alta velocidad (el teclado y el ratón) se cede a ellas y se enumera allí; la memoria, por el EHCI" \
  "^usb: controladora EHCI 8086:293A, 6 puertos (2 cedidos a las compañeras: aparatos de USB 1.x)\(; traspaso de la BIOS.*\)\?$" "^usb: controladora UHCI 8086:2934, 2 puertos$" \
  "velocidad alta, en el puerto 2 (EHCI); almacenamiento$" "velocidad plena, en el puerto 1 (UHCI); teclado$" "velocidad plena, en el puerto 1 (UHCI); ratón$" \
  "^usb: 1 teclado, 1 ratón, 1 memoria\.$"
mira "UHCI: el teclado USB (se teclea por él, sin teclado PS/2), con sus letras" "^X:.>echo El pingüino de la señora > a.txt$" "^El pingüino de la señora$" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" "; archivos abiertos: 0$" "^\[código 5\]"
saca $N/comp.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/comp.img COPIA.TXT $N/copia.txt
if sano $N/comp.img && cmp -s $N/largo.txt $N/copia.txt; then ok "y la memoria queda sana, con lo copiado igual que el original"
else ko "con el EHCI y sus compañeras, la memoria no queda bien"; fi
# 7 ter. una UHCI sola (un PC de antes), con la memoria en ella (a velocidad plena): leer y escribir
cp $P/d16.img $N/uhci.img
ESCRIBE=1 MODULOS= ARRANQUE="ordenes escritura" QEMU_MAS="-device piix3-usb-uhci,id=u -drive id=mu,file=$N/uhci.img,format=raw,if=none -device usb-storage,bus=u.0,port=1,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "copy textos\\largo.txt copia.txt" escribe ":hasta 180 1 todo queda como estaba" ":pausa 1" salir disco apagar > $N/uhci.sal 2>&1
S=$N/uhci.sal
mira "UHCI: una memoria USB a velocidad plena (cada paquete, un TD; la alternancia, llevada aquí): se lee y se escribe" \
  "^usb: controladora UHCI 8086:7020, 2 puertos$" "velocidad plena, en el puerto 1 (UHCI); almacenamiento$" "^ *1 archivo(s) copiado(s)\.$" \
  "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
  "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" "; archivos abiertos: 0$" "^\[código 5\]"
saca $N/uhci.img TEXTOS/LARGO.TXT $N/largo.txt; saca $N/uhci.img COPIA.TXT $N/copia.txt
if sano $N/uhci.img && cmp -s $N/largo.txt $N/copia.txt; then ok "UHCI: la memoria queda sana (fsck.fat), con lo copiado igual que el original"
else ko "UHCI: la memoria no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
# 8. el permiso solo para las memorias USB: en el disco de dentro no se escribe
cp $P/d16.img $N/pc.img; cp $P/d16.img $N/pu.img
SOLO_LECTURA=1 ESCRIBE=1 DISCO=$N/pc.img MODULOS= ARRANQUE="ordenes escritura=usb unidad=X" QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$N/pu.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh "" "echo en la memoria > nuevo.txt" "type nuevo.txt" "echo en el disco > c:\\nuevo.txt" "del c:\\leeme.txt" "copy c:\\leeme.txt" \
  "dir c:\\textos" salir disco apagar > $N/perm.sal 2>&1
S=$N/perm.sal
mira "«escritura=usb unidad=X»: la unidad del sistema es la memoria USB y en ella se escribe; el disco de dentro se ve (C:), pero es de solo lectura" \
  "^los discos de dentro son de solo lectura" "^unidades: C: (disco 0, IDE, FAT16) X: (disco 1, USB, FAT16); la del sistema, X:" \
  "^X:.>type nuevo.txt$" "^en la memoria$" "^No puedo escribir en c:.nuevo.txt: es de solo lectura\.$" "^No puedo borrar c:.leeme.txt: es de solo lectura\.$" \
  "^ *1 archivo(s) copiado(s)\.$" "^ Directorio de C:.TEXTOS$"
saca $N/pu.img NUEVO.TXT $N/nuevo.txt; saca $N/pu.img LEEME.TXT $N/leeme2.txt; saca $N/pc.img LEEME.TXT $N/leeme1.txt
if cmp -s $P/d16.img $N/pc.img && [ "$(tr -d '\r' < $N/nuevo.txt)" = "en la memoria" ] && cmp -s $N/leeme1.txt $N/leeme2.txt && sano $N/pu.img; then
  ok "y así es: la imagen del disco de dentro queda idéntica octeto a octeto, y lo escrito y lo copiado están en la memoria USB (sana)"
else ko "con «escritura=usb», o el disco de dentro ha cambiado o la memoria no tiene lo escrito"; fi
# 9. «sinusb»: el USB ni se toca
MODULOS= QEMU_MAS="-device qemu-xhci,id=u -drive id=mu,file=$P/d16.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" ARRANQUE=sinusb \
  $AQUI/nucleo32/probar.sh "" disco apagar > $N/sinusb.sal 2>&1
S=$N/sinusb.sal
if ! grep -q "^usb:" $S && grep -q "^disco 0: QEMU DVD-ROM, lector de CD o DVD (IDE), sin disco$" $S && grep -q "^\[código 5\]" $S; then ok "con «sinusb» el núcleo ni toca el USB (se teclea por el teclado PS/2, y la memoria no se ve)"
else ko "«sinusb» no deja el USB sin tocar:"; sed 's/^/        /' $S | head -8; fi
# 10. el escritorio, sin teclado ni ratón PS/2: el ratón y el teclado USB, guardar en la memoria, y Mi equipo con sus unidades
cp $P/d16.img $N/eu.img
CAPTURAS=$G ESCRIBE=1 MODULOS= MAQUINA=pc,i8042=off SOLO_LECTURA=1 SATA=$P/d32.img ARRANQUE="escritorio escritura=usb unidad=X" \
  QEMU_MAS="-device usb-ehci,id=u -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 -drive id=mu,file=$N/eu.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clic 40 585" ":captura m1" ":clic 80 176" ":pausa 2" ":escribe Guardado en la memoria USB: ¡señor!" ":tecla f2" ":pausa 1" ":captura m2" \
  ":clic 578 76" ":pausa 1" ":doble 62 30" ":pausa 2" ":captura m3" ":doble 580 160" ":pausa 1" ":captura m4" > $N/escusb.sal 2>&1
S=$N/escusb.sal
mira "el escritorio en un PC sin teclado ni ratón PS/2 (como un Mac): arranca con el ratón y el teclado USB" \
  "^usb: 1 teclado, 1 ratón, 1 memoria\.$" "^escritorio: en marcha, 800 x 600" "escritorio: lanzo .BLOC.BIN" "^bloc: guardado NOTAS.TXT, 35 octetos"
miracap "el ratón USB mueve la flecha y pulsa: el menú Inicio se abre con un clic" m1 negrita 46 155 54 163 "Bloc de notas" negrita 320 533 328 541 "Apagar equipo"
miracap "el teclado USB escribe en el Bloc de notas (con eñe y «¡»), y F2 guarda" m2 texto 94 146 102 156 "Guardado en la memoria USB: ¡señor!" \
  texto 94 428 104 448 "Guardado en NOTAS.TXT (35 octetos)."
saca $N/eu.img NOTAS.TXT $N/notas.txt
if [ "$(iconv -f CP437 -t UTF-8 $N/notas.txt 2> /dev/null)" = "Guardado en la memoria USB: ¡señor!" ] && sano $N/eu.img; then ok "y lo guardado está en la memoria USB (la unidad del sistema, X:), que queda sana"
else ko "lo guardado con el Bloc de notas no está en la memoria USB"; fi
miracap "Mi equipo enseña las unidades con su letra: C: (el disco SATA de dentro, de solo lectura) y X: (la memoria USB)" m3 negrita 20 5 60 25 "Mi equipo" \
  negrita 313 133 323 143 "(C:)" texto 313 151 323 161 "libres de" texto 264 169 274 179 "(de solo lectura)" negrita 563 133 573 143 "(X:)"
# (0.8.7) LEEME.TXT y NOTAS.TXT, en cualquier sitio de la lista: con más programas en la memoria (USUARIOS, ARGON2, PANEL, HORARED...) cambian de sitio
miracap "y dos clics en X: enseñan lo que hay en la memoria (con lo recién guardado)" m4 negrita 20 5 60 25 "X:\\" texto 20 60 780 560 "LEEME.TXT" texto 20 60 780 560 "NOTAS.TXT"
fi   # los discos de prueba

# 11. la memoria USB con GRUB, como la arranca un PC: el núcleo la lee él (X:)
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
  [ -f $G/usb.img ] || $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1
  if [ -f $G/usb.img ]; then
    for hc in xhci ehci; do
      case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
      cp $G/usb.img $N/g$hc.img
      rm -f $G/monu
      qemu-system-i386 -nic none -m 64 -machine pc,i8042=off -display none -monitor unix:$G/monu,server,nowait -serial file:$N/g$hc.sal -no-reboot -device $dev \
        -drive file=$N/g$hc.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1 -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 > /dev/null 2>&1 &
      QPID=$!
      monu() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$G/monu > /dev/null 2>&1; sleep "${2:-0.3}"; }
      for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
      sleep 6; monu "sendkey ret" 14
      monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8
      monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -103" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5
      for k in h o l a spc u s b; do monu "sendkey $k" 0.1; done
      monu "sendkey f2" 1.5
      monu "screendump $G/g$hc.ppm" 0.6
      monu quit 0.5; wait $QPID 2> /dev/null; QPID=
      tr -d '\r' < $N/g$hc.sal | iconv -f CP437 -t UTF-8 > $N/g$hc.txt
      S=$N/g$hc.txt
      mira "la memoria USB con GRUB, arrancada de un $que sin teclado PS/2: el núcleo le quita el USB a la BIOS y lee él la memoria (X:, la del sistema)" \
        "^usb: controladora $que " "^usb: 1 teclado, 1 ratón, 1 memoria\.$" "^disco 0: QEMU    QEMU HARDDISK, 64 MB, FAT16 (USB)\.$" \
        "^unidades: M: (en memoria) X: (disco 0, USB, FAT16); la del sistema, X:" "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" \
        "escritorio: lanzo .BLOC.BIN" "^bloc: guardado NOTAS.TXT, 8 octetos"
      dd if=$N/g$hc.img of=$N/g$hc.vol bs=1M skip=1 status=none
      saca $N/g$hc.vol NOTAS.TXT $N/notas.txt; saca $N/g$hc.vol ARRANQUE.TXT $N/arranque.txt
      if cap $G/g$hc.ppm texto 94 146 102 156 "hola usb" && [ "$(cat $N/notas.txt 2> /dev/null)" = "hola usb" ] && LC_ALL=C grep -q "^usb: 1 teclado, 1 rat.n, 1 memoria" $N/arranque.txt && grep -q "la del sistema, X:" $N/arranque.txt \
        && fsck.fat -n $N/g$hc.vol > $P/fsck.txt 2>&1; then
        ok "$que: lo tecleado con el teclado USB y guardado con el Bloc de notas está en la memoria (sana), con el registro del arranque (ARRANQUE.TXT)"
      else ko "$que: en la memoria USB de arranque no queda lo guardado, o no queda sana:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
    done
    # la sexta entrada, «sin tocar el USB ni los discos»: como la 0.5 (con el
    # teclado y el ratón PS/2, y los programas de la unidad en memoria)
    rm -f $G/monu
    qemu-system-i386 -nic none -m 64 -display none -monitor unix:$G/monu,server,nowait -serial file:$N/gsin.sal -no-reboot -device qemu-xhci \
      -drive file=$G/usb.img,format=raw,if=none,id=u,snapshot=on -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
    QPID=$!
    for ((i = 0; i < 40; i++)); do [ -S $G/monu ] && break; sleep 0.1; done
    sleep 6; for i in 1 2 3 4 5; do monu "sendkey down"; done; monu "sendkey ret" 12
    monu "mouse_move -360 285" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 0.8
    monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -102" 0.1; monu "mouse_move 10 -103" 0.4; monu "mouse_button 1" 0.2; monu "mouse_button 0" 2.5
    monu quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $N/gsin.sal | iconv -f CP437 -t UTF-8 > $N/gsin.txt
    S=$N/gsin.txt
    # (0.8.7) 43: los de la 0.8.6 y USUARIOS, ARGON2, PANEL y HORARED; (TOUCHPAD) 44, con RATON; (BLUETOOTH) 45, con BT
    if ! grep -q "^usb:" $S && ! grep -q "^disco " $S && grep -q "^sin disco: los 45 archivos que ha traído el cargador hacen de unidad M:" $S \
      && grep -q "escritorio: lanzo .BLOC.BIN" $S; then
      ok "la entrada «sin tocar el USB, los discos ni la red»: ni una línea de USB ni de discos, la unidad del sistema es M: (en memoria) y de ella se lanza el Bloc de notas"
    else ko "la entrada «sin tocar el USB ni los discos» no arranca como la 0.5:"; sed 's/^/        /' $S | head -10; fi
  else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la memoria USB con GRUB)"
fi

# 12. la oficina de Hispa-386 (programas de Hispa Ventanas, .HVP) con HVPRUN.BIN
echo "== Hispa-Pentium: la oficina de Hispa-386 dentro (HVPRUN: Facturas, Pluma, Cuentas y Mirador) =="
if $AQUI/nucleo32/disco.sh $N/of.img 16 oficina > $N/discoof.txt 2>&1; then
  CAPTURAS=$G ESCRIBE=1 DISCO=$N/of.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":clic 40 585" ":raton 150 470" ":captura of1" \
    ":clic 80 258" ":pausa 4" ":captura of2" ":clic 70 83" ":pausa 2" ":clic 440 190" ":escribe Taller Peña" ":clic 456 434" ":escribe Revisión del año" \
    ":clic 360 466" ":escribe 121,50" ":pausa 1" ":captura of3" ":clic 356 509" ":pausa 2" ":captura of4" ":clic 734 34" ":pausa 2" ":captura of5" \
    ":clic 40 585" ":clic 80 258" ":pausa 4" ":captura of6" ":clic 734 34" ":pausa 2" \
    ":clic 40 585" ":clic 80 288" ":pausa 4" ":escribe Hola, señor Pérez." ":tecla ret" ":escribe Segunda línea." ":clic 186 92" ":pausa 2" ":escribe carta" \
    ":tecla ret" ":pausa 2" ":captura of7" ":tecla ctrl-e" ":tecla ctrl-c" ":clic 695 44" ":pausa 2" ":doble 64 110" ":pausa 3" ":tecla ctrl-v" ":pausa 1" ":captura of8" \
    ":clic 40 585" ":clic 80 318" ":pausa 4" ":escribe 12" ":tecla ret" ":escribe 30" ":tecla ret" ":escribe =A1+A2" ":tecla ret" ":pausa 1" ":captura of9" > $N/oficina.sal 2>&1
  S=$N/oficina.sal
  mira "el menú Inicio lanza los programas de la oficina con HVPRUN.BIN" "escritorio: lanzo .HVPRUN.BIN (la tarea [0-9]*)"
  miracap "el menú Inicio tiene la oficina a mano (MENÚ: entre los más usados; y en Todos los programas > Oficina): Facturas, Pluma, Cuentas y Mirador" of1 \
    texto 38 246 46 254 Facturas texto 38 276 46 284 Pluma texto 38 306 46 314 Cuentas texto 38 336 46 344 Mirador negrita 46 155 54 163 "Bloc de notas" negrita 320 533 328 541 "Apagar equipo"
  miracap "Facturas (FACTURAS.HVP, de Hispa-386) en una ventana de Hispa-Pentium: su barra de menús, sus botones y su lista" of2 negrita 72 22 84 30 Facturas \
    texto 62 126 74 138 "No hay facturas emitidas." texto 206 70 218 80 Emitidas
  miracap "sus cuadros de diálogo salen dentro, con la fecha de hoy (INT 21h), y se escribe en ellos (con eñe y acentos)" of3 negrita 202 76 214 86 "Datos de la factura" \
    texto 310 178 322 188 "Taller Peña"
  miracap "la factura nueva: base, IVA y total, como saldrá en el papel" of4 texto 150 178 162 190 "Taller Peña" texto 70 252 82 264 "Revisión del año" negrita 670 314 682 326 "147,02"
  miracap "el botón de cerrar de la ventana acaba Facturas (ni ventana ni botón en la barra de tareas)" of5 notexto 50 20 200 40 Facturas notexto 100 570 400 595 Facturas
  miracap "y al volver a abrirla, la factura está (FACTURAS.DAT, en el disco FAT)" of6 texto 274 115 286 127 "Taller Peña" texto 590 115 602 127 "147,02" texto 54 497 66 509 "1 factura"
  miracap "Pluma (el procesador de textos): lo tecleado, y «Guardar» con su cuadro de elegir archivo; el título lo dice" of7 negrita 110 30 122 42 "Pluma - carta.PLU" \
    texto 98 142 110 154 "Hola, señor Pérez." texto 98 158 110 170 "Segunda línea."
  miracap "el portapapeles: lo copiado en Pluma (Ctrl+E, Ctrl+C) se pega en el Bloc de notas" of8 texto 140 180 260 260 "Hola, señor Pérez."
  miracap "Cuentas (la hoja de cálculo): dos números y su suma con una fórmula" of9 negrita 92 20 104 32 "Cuentas - (sin nombre)" texto 170 196 184 210 42
  saca $N/of.img OFICINA/FACTURAS.DAT $N/facturas.dat; saca $N/of.img CARTA.PLU $N/carta.plu
  if [ -s $N/facturas.dat ] && LC_ALL=C grep -qa "Taller Pe.a" $N/facturas.dat && LC_ALL=C grep -qa "Hola, se.or P.rez" $N/carta.plu && sano $N/of.img; then
    ok "lo guardado está en el disco: OFICINA\\FACTURAS.DAT (con la factura) y CARTA.PLU (lo de Pluma), y el disco queda sano"
  else ko "lo guardado por Facturas o Pluma no está en el disco, o no queda sano:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
  # Mi equipo: un documento de Pluma y una imagen (Mirador), con el teclado:
  # Intro entra en C:, una letra va al primero que empieza por ella, Intro abre
  python3 - $N/FOTO.BMP <<'PY'
import struct, sys
an, al = 160, 100
filas = []
for y in range(al):
    f = bytearray()
    for x in range(an):
        f += bytes([255 * x // an, 255 * y // al, 200 if (x // 20 + y // 20) % 2 else 60])
    filas.append(bytes(f))
datos = b''.join(reversed(filas))
cab = b'BM' + struct.pack('<IHHI', 54 + len(datos), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, an, al, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
open(sys.argv[1], 'wb').write(cab + datos)
PY
  MTOOLS_SKIP_CHECK=1 mcopy -i $N/of.img $N/FOTO.BMP ::
  CAPTURAS=$G DISCO=$N/of.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 2" ":doble 62 30" ":pausa 3" ":tecla ret" ":pausa 2" \
    ":tecla f" ":pausa 1" ":tecla ret" ":pausa 5" ":raton 400 450" ":captura of10" ":clic 180 582" ":pausa 1" ":tecla c" ":tecla ret" ":pausa 5" ":captura of11" > $N/oficina2.sal 2>&1
  miracap "Mi equipo abre una imagen con Mirador (MIRADOR.HVP): su nombre, lo que mide y la imagen" of10 negrita 30 20 42 32 "Mirador - FOTO.BMP" \
    texto 10 520 20 535 "FOTO.BMP" texto 108 519 120 531 "160 x 100"
  miracap "y un documento de Pluma (.PLU) con Pluma: lo guardado antes, con sus acentos" of11 negrita 110 30 122 42 "Pluma - \\CARTA.PLU" \
    texto 146 142 158 154 "señor Pérez." texto 98 158 110 170 "Segunda línea."
else ko "disco.sh no puede hacer el disco con la oficina: $(tail -n 1 $N/discoof.txt)"; fi

fi   # SOLO=07

# ------------------------------------------------- 10. Hispa-Pentium 0.7
# Instalar en el disco duro: el programa INSTALAR (uinstala.cas), el arranque
# del disco (GRUB en el MBR y en el hueco de delante de la partición) y el
# asistente del primer arranque (uasiste.cas). Lo que se instala (el núcleo,
# los programas, BOOT.IMG y CORE.IMG) sale de la memoria USB que hace usb.sh.
if [ "${SOLO:-}" != 08 ]; then
echo "== Hispa-Pentium 0.7: instalar en el disco duro (INSTALAR), arrancar de él y el asistente del primer arranque =="
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ] && command -v parted > /dev/null; then
Z=$P/n07
K=$Z/kit
mkdir -p $Z $K
[ -f $G/usb.img ] || $AQUI/nucleo32/usb.sh $G/usb.img > $G/usb.txt 2>&1
if [ -f $G/usb.img ]; then
INST=
for f in ESCRIT8.BIN:NUCLEO.BIN INSTALAR.BIN ASISTE.BIN ORDENES.BIN BLOC.BIN CONSOLA.BIN EQUIPO.BIN HOLA.BIN INSTALA/LEEME.TXT:LEEME.TXT INSTALA/MANUAL.TXT:MANUAL.TXT INSTALA/BOOT.IMG:BOOT.IMG INSTALA/CORE.IMG:CORE.IMG; do
  mcopy -n -i $G/usb.img@@1M ::${f%%:*} $K/${f##*:} > /dev/null 2>&1
  INST=$INST${INST:+,}$K/${f##*:}
done
mcopy -n -i $G/usb.img@@1M ::SEMILLA.BIN $K/SEMILLA.BIN > /dev/null 2>&1
# (y dos archivos para otra carpeta, como los de la oficina: CARPETAS.TXT
# dice adónde va cada uno, y él no se instala)
head -c 3000 /dev/urandom > $K/UNO.DAT; printf 'el dos\r\n' > $K/DOS.TXT; printf 'UNO.DAT=COSAS\r\ndos.txt=cosas\r\nBLOC.PQI=HISPA\\PAQUETES\r\n' > $K/CARPETAS.TXT
# (y el registro de un paquete, que va dentro de \HISPA: a \HISPA\PAQUETES)
mcopy -n -i $G/usb.img@@1M ::INSTALA/PAQUETES/BLOC.PQI $K/BLOC.PQI > /dev/null 2>&1
INST=$INST,$K/UNO.DAT,$K/DOS.TXT,$K/BLOC.PQI,$K/CARPETAS.TXT
# instala con el núcleo arrancado por QEMU (-kernel) y lo que se instala como
# módulos (la unidad M:, como con la entrada de la memoria USB): el disco
# (SATA; con IDE=1, IDE), el nombre de lo que sale y las teclas
instala() {
  local d=$1 n=$2; shift 2
  if [ -n "${IDE:-}" ]; then
    CAPTURAS=$Z ESCRIBE=1 DISCO=$d MODULOS=$INST ARRANQUE="instalar unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" "$@" > $Z/$n.sal 2>&1
  else
    CAPTURAS=$Z ESCRIBE=1 SATA=$d MODULOS=$INST ARRANQUE="instalar unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" "$@" > $Z/$n.sal 2>&1
  fi
  S=$Z/$n.sal
}
# la partición (desde ese sector) de un disco, a $Z/vol.img, y ¿está sana?
volumen() { dd if=$1 of=$Z/vol.img bs=512 skip=$2 conv=sparse status=none; fsck.fat -n $Z/vol.img > $Z/fsck.txt 2>&1; }
# ¿está en el volumen ($Z/vol.img) el sistema, igual que el que se instala, y el disco arrancable?
instalado() {
  local f bien=1
  for f in NUCLEO.BIN INSTALAR.BIN ASISTE.BIN ORDENES.BIN BLOC.BIN CONSOLA.BIN EQUIPO.BIN HOLA.BIN LEEME.TXT MANUAL.TXT BOOT.IMG CORE.IMG; do
    rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/$f $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/$f || { bien=0; echo "distinto: $f" >> $Z/fsck.txt; }
  done
  rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::LEEME.TXT $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/LEEME.TXT || { bien=0; echo "distinto: LEEME.TXT de la raíz" >> $Z/fsck.txt; }
  if [ "${2:-}" = usb ]; then
    # (instalado desde la memoria USB: si lleva la oficina, HVPRUN.BIN en \HISPA y lo suyo en \OFICINA)
    if mcopy -n -i $G/usb.img@@1M ::OFICINA/PLUMA.HVP $Z/o.bin > /dev/null 2>&1; then
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::OFICINA/PLUMA.HVP $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $Z/o.bin || { bien=0; echo "distinto: OFICINA/PLUMA.HVP" >> $Z/fsck.txt; }
      mcopy -n -i $G/usb.img@@1M ::HVPRUN.BIN $Z/o.bin > /dev/null 2>&1
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/HVPRUN.BIN $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $Z/o.bin || { bien=0; echo "distinto: HVPRUN.BIN" >> $Z/fsck.txt; }
      rm -f $Z/o.bin
    fi
  else
    for f in UNO.DAT DOS.TXT; do
      rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::COSAS/$f $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/$f || { bien=0; echo "distinto: COSAS/$f" >> $Z/fsck.txt; }
    done
    mdir -i $Z/vol.img ::HISPA 2> /dev/null | grep -q "^UNO \|^DOS \|^CARPETAS\|^BLOC  *PQI" && { bien=0; echo "en HISPA hay lo que no toca" >> $Z/fsck.txt; }
    rm -f $Z/f.bin; mcopy -n -i $Z/vol.img ::HISPA/PAQUETES/BLOC.PQI $Z/f.bin > /dev/null 2>&1; cmp -s $Z/f.bin $K/BLOC.PQI || { bien=0; echo "distinto: HISPA/PAQUETES/BLOC.PQI" >> $Z/fsck.txt; }
  fi
  # el arranque: el primer sector de GRUB en el MBR (sus 440 octetos) y el resto desde el sector 1
  cmp -s -n 440 $1 $K/BOOT.IMG || { bien=0; echo "el MBR no lleva BOOT.IMG" >> $Z/fsck.txt; }
  cmp -s <(dd if=$1 bs=512 skip=1 status=none | head -c $(stat -c %s $K/CORE.IMG)) $K/CORE.IMG || { bien=0; echo "CORE.IMG no está desde el sector 1" >> $Z/fsck.txt; }
  [ $bien = 1 ]
}
tabla() { parted -s $1 unit s print 2> /dev/null | sed -n '/^Partition Table/p; /^ *[0-9]/p' | tr -s ' ' | sed 's/ *$//' > $Z/tabla.txt; }

# 1. un disco SATA en blanco: la bienvenida, la ayuda, elegir el disco, instalar
truncate -s 300M $Z/blanco.img
instala $Z/blanco.img blanco ":texto i1" ":tecla f1" ":texto i2" ":tecla ret" ":tecla ret" ":texto i3" ":tecla ret" ":texto i4" ":tecla ret" ":pausa 5" ":texto i5" ":tecla ret"
if grep -q "Instalación de Hispa-Pentium 0.8" $Z/i1.txt && grep -q "Bienvenido a la instalación de Hispa-Pentium" $Z/i1.txt && grep -q "INTRO=Continuar   F1=Ayuda   F3=Salir" $Z/i1.txt \
  && grep -q "La memoria USB de arranque no se toca" $Z/i1.txt && grep -q "^|   Ayuda$" $Z/i2.txt && grep -q "hay que confirmarlo dos veces" $Z/i2.txt; then
  ok "INSTALAR: la pantalla azul de bienvenida (el título, el pie con INTRO=Continuar, F1=Ayuda y F3=Salir) y la ayuda (F1)"
else ko "INSTALAR: la bienvenida o la ayuda no salen como deben:"; sed 's/^/        /' $Z/i1.txt | head -8; fi
if grep -q "Elija el disco donde instalar Hispa-Pentium" $Z/i3.txt && grep -q "Disco 1   SATA  QEMU HARDDISK  *300 MB" $Z/i3.txt && grep -q "(en blanco: sin particiones)" $Z/i3.txt \
  && grep -q "Todo listo para instalar" $Z/i4.txt && grep -q "Partición:  una nueva de 299 MB (el disco está en blanco)" $Z/i4.txt && grep -q "INTRO=Instalar   ESC=Volver   F3=Salir" $Z/i4.txt \
  && grep -q "Hispa-Pentium está instalado" $Z/i5.txt && grep -q "Quite ahora la memoria USB y pulse INTRO para reiniciar" $Z/i5.txt; then
  ok "INSTALAR: elegir el disco (con su tamaño y lo que tiene), la pantalla de «todo listo» y la del final"
else ko "INSTALAR: las pantallas de elegir el disco, «todo listo» o el final no salen como deben:"; sed 's/^/        /' $Z/i3.txt | head -10; fi
mira "INSTALAR en un disco SATA en blanco: la partición, el formato FAT32, la copia a \\HISPA y el arranque; y reinicia" \
  "^instalar: disco 0, SATA, 300 MB, se puede: 1$" "^instalar: partición 1, desde el sector 2048, 612352 sectores$" \
  "^instalar: FAT32 con clusters de 4096 octetos, [0-9]* clusters, [0-9]* sectores por FAT$" "^instalar: copiado NUCLEO.BIN ([0-9]* octetos)$" \
  "^instalar: copiado LEEME.TXT a la raíz" "^instalar: Dejando el disco arrancable\.\.\.$" "^instalar: terminado$" "^reiniciar$"
tabla $Z/blanco.img
if volumen $Z/blanco.img 2048 && instalado $Z/blanco.img && grep -q "^Partition Table: msdos" $Z/tabla.txt && [ "$(grep -c '^ [0-9]' $Z/tabla.txt)" = 1 ] \
  && grep -q "^ 1 2048s 614399s 612352s primary fat32 boot, lba" $Z/tabla.txt; then
  ok "el disco instalado: una partición FAT32 arrancable en el MBR, sana para fsck.fat, con el sistema en \\HISPA igual que el original y GRUB en el MBR y en el hueco de delante"
else ko "el disco instalado no queda bien:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; fi
if grep -q "^instalar: copiado UNO.DAT (3000 octetos) a .COSAS$" $S && grep -q "^instalar: copiado DOS.TXT (8 octetos) a .COSAS$" $S && grep -q "^instalar: 15 archivos que instalar$" $S \
  && [ "$(mdir -b -i $Z/vol.img ::COSAS 2> /dev/null | tr '\n' ' ')" = "::/COSAS/UNO.DAT ::/COSAS/DOS.TXT " ] \
  && grep -q "^instalar: copiado BLOC.PQI ([0-9]* octetos) a .HISPA.PAQUETES$" $S && [ "$(mdir -b -i $Z/vol.img ::HISPA/PAQUETES 2> /dev/null | tr '\n' ' ')" = "::/HISPA/PAQUETES/BLOC.PQI " ]; then
  ok "lo que dice CARPETAS.TXT («ARCHIVO.EXT=CARPETA») va a su carpeta de la raíz (C:\\COSAS: así irá la oficina a C:\\OFICINA) y no a \\HISPA, y «HISPA\\PAQUETES», dentro de \\HISPA (los registros de lo instalado); CARPETAS.TXT no se instala"
else ko "lo de CARPETAS.TXT no va a su carpeta:"; mdir -i $Z/vol.img :: 2>&1 | sed 's/^/        /' | head -8; grep "archivos que\|COSAS" $S | sed 's/^/        /' | head -4; fi
rm -f $Z/cfg.txt; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1
if grep -q "^set modo=800x600x32,800x600,auto$" $Z/cfg.txt && grep -q "multiboot /HISPA/NUCLEO.BIN escritorio escritura$" $Z/cfg.txt && grep -q "module /HISPA/ORDENES.BIN ORDENES.BIN" $Z/cfg.txt \
  && ! grep -q chainloader $Z/cfg.txt && ! grep -q "INSTALAR.BIN" $Z/cfg.txt; then
  ok "el menú de arranque del disco (\\HISPA\\GRUB.CFG): el escritorio, el modo texto y el de emergencia con los programas como módulos"
else ko "\\HISPA\\GRUB.CFG no es el que tiene que ser:"; sed 's/^/        /' $Z/cfg.txt | head -8; fi

# (y otra vez encima: el disco ya tiene su FAT32, que el núcleo ha montado
# como C:; al escribir en él por debajo deja de estarlo)
instala $Z/blanco.img otravez ":tecla ret" ":texto r1" ":tecla ret" ":tecla ret" ":tecla s" ":escribe BORRAR" ":tecla ret" ":tecla ret" ":pausa 5" ":texto r2" ":tecla ret"
if grep -q "^disco 0: QEMU HARDDISK, 300 MB, FAT32 (SATA)\.$" $S && grep -q "partición 1: FAT32  *299 MB" $Z/r1.txt && grep -q "^instalar: terminado$" $S \
  && grep -q "Hispa-Pentium está instalado" $Z/r2.txt && volumen $Z/blanco.img 2048 && instalado $Z/blanco.img; then
  ok "instalar otra vez encima (el disco ya llevaba Hispa-Pentium, montado como C:): borrándolo con sus dos confirmaciones, queda instalado y sano"
else ko "instalar encima de un Hispa-Pentium ya instalado no va:"; sed 's/^/        /' $Z/fsck.txt | head -6; tail -n 4 $S | sed 's/^/        /'; fi

# 2. un disco con la partición de otro sistema y espacio sin usar: salir con
# F3 y no confirmar el borrado no tocan nada; se instala en lo libre
truncate -s 300M $Z/otro.img
parted -s $Z/otro.img mklabel msdos mkpart primary ntfs 1MiB 101MiB set 1 boot on
dd if=/dev/urandom of=$Z/otro.img bs=1M seek=1 count=100 conv=notrunc status=none
cp $Z/otro.img $Z/otro0.img
instala $Z/otro.img otro1 ":tecla ret" ":texto o1" ":tecla ret" ":texto o2" ":tecla down" ":tecla ret" ":texto o3" ":tecla s" ":texto o4" ":escribe BORRA" ":tecla ret" ":texto o5" \
  ":tecla down" ":tecla ret" ":tecla n" ":tecla f3" ":texto o6" ":tecla n" ":tecla f3" ":tecla s"
if grep -q "partición 1: NTFS o exFAT (Windows)  *100 MB" $Z/o1.txt && grep -q "espacio sin usar:  *199 MB" $Z/o1.txt \
  && grep -q "Tiene 199 MB sin usar: se puede instalar ahí sin tocar lo que hay" $Z/o2.txt && grep -q "Instalar en el espacio sin usar (no se toca lo que hay)" $Z/o2.txt \
  && grep -q "Borrar el disco entero e instalar (se pierde TODO lo que tiene)" $Z/o2.txt; then
  ok "un disco con otro sistema: se enseñan su partición (NTFS) y el espacio sin usar, y se ofrece instalar ahí o borrar el disco"
else ko "un disco con otro sistema: no se enseña bien lo que tiene:"; sed 's/^/        /' $Z/o1.txt | sed -n 5,12p; fi
if grep -q "ATENCION: se va a BORRAR TODO lo que hay en este disco" $Z/o3.txt && grep -q "S=Sí, borrar el disco   ESC=Volver (no se toca nada)" $Z/o3.txt \
  && grep -q "Borrar el disco entero: segunda confirmación" $Z/o4.txt && grep -q "escriba la palabra BORRAR y pulse INTRO" $Z/o4.txt \
  && grep -q "Dónde instalar Hispa-Pentium en este disco" $Z/o5.txt && grep -q "^instalar: no se borra$" $S && grep -q "¿Salir de la instalación y reiniciar el equipo?  (S/N)" $Z/o6.txt \
  && grep -q "^instalar: se sale$" $S && ! grep -q "^instalar: partición" $S && cmp -s $Z/otro.img $Z/otro0.img; then
  ok "borrar el disco pide dos confirmaciones (una S y escribir BORRAR): con la palabra mal escrita, con N y saliendo con F3, el disco queda idéntico octeto a octeto"
else ko "las confirmaciones de borrar no van (o el disco ha cambiado sin confirmarlo):"; sed 's/^/        /' $Z/o5.txt | sed -n 4,10p; fi
instala $Z/otro.img otro2 ":tecla ret" ":tecla ret" ":tecla ret" ":texto o7" ":tecla ret" ":pausa 5" ":texto o8" ":tecla ret"
tabla $Z/otro.img
if grep -q "una nueva de 199 MB en el espacio sin usar; lo demás no se toca" $Z/o7.txt && grep -q "hay en el disco (su partición 1)" $Z/o7.txt && grep -q "^instalar: partición 2, desde el sector 206848, 407552 sectores$" $S \
  && grep -q "El sistema que ya había en el disco sigue en su sitio" $Z/o8.txt && cmp -s <(dd if=$Z/otro.img bs=1M skip=1 count=100 status=none) <(dd if=$Z/otro0.img bs=1M skip=1 count=100 status=none) \
  && grep -q "^ 1 2048s 206847s 204800s primary$" $Z/tabla.txt && grep -q "^ 2 206848s 614399s 407552s primary fat32 boot, lba" $Z/tabla.txt \
  && volumen $Z/otro.img 206848 && instalado $Z/otro.img; then
  ok "instalar en el espacio sin usar: la partición del otro sistema queda idéntica octeto a octeto, la nueva (la 2) es la arrancable, y está sana y con el sistema"
else ko "instalar en el espacio sin usar no va (o toca lo que había):"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; fi
rm -f $Z/cfg.txt $Z/mbr.ant; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1; mcopy -n -i $Z/vol.img ::HISPA/MBR.ANT $Z/mbr.ant > /dev/null 2>&1
if grep -q "^set timeout=8$" $Z/cfg.txt && grep -q 'El sistema que ya estaba en el disco (particion 1)' $Z/cfg.txt && grep -q "set root=(hd0,msdos1)" $Z/cfg.txt && grep -q "chainloader +1" $Z/cfg.txt \
  && cmp -s -n 512 $Z/mbr.ant $Z/otro0.img; then
  ok "el menú de arranque lleva una entrada para el sistema que ya había, y el primer sector de antes queda guardado (\\HISPA\\MBR.ANT)"
else ko "el menú de arranque no lleva el otro sistema, o no se guarda el primer sector de antes:"; sed 's/^/        /' $Z/cfg.txt | tail -n 6; fi

# 3. un disco lleno (sin espacio sin usar): solo se puede borrar, con sus dos confirmaciones
truncate -s 200M $Z/lleno.img
parted -s $Z/lleno.img mklabel msdos mkpart primary ext2 1MiB 100%
dd if=/dev/urandom of=$Z/lleno.img bs=1M seek=1 count=20 conv=notrunc status=none
instala $Z/lleno.img lleno ":tecla ret" ":tecla ret" ":texto l1" ":tecla ret" ":tecla s" ":escribe borrar" ":tecla ret" ":texto l2" ":tecla ret" ":pausa 5" ":texto l3" ":tecla ret"
tabla $Z/lleno.img
if grep -q "No tiene bastante espacio sin usar" $Z/l1.txt && ! grep -q "Instalar en el espacio sin usar" $Z/l1.txt && grep -q "una nueva de 199 MB, BORRANDO todo lo que tiene el disco" $Z/l2.txt \
  && grep -q "^instalar: confirmar borrar (2)$" $S && grep -q "^instalar: FAT32 con clusters de 512 octetos, " $S && grep -q "Hispa-Pentium está instalado" $Z/l3.txt && [ "$(grep -c '^ [0-9]' $Z/tabla.txt)" = 1 ] && grep -q "^ 1 2048s 409599s 407552s primary fat32 boot, lba" $Z/tabla.txt \
  && volumen $Z/lleno.img 2048 && instalado $Z/lleno.img; then
  ok "un disco lleno: no se ofrece el espacio sin usar; confirmado dos veces (S y BORRAR), se borra entero y queda instalado y sano (199 MB: FAT32 con clusters de 512 octetos)"
else ko "borrar un disco lleno e instalar no va:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; sed 's/^/        /' $Z/l1.txt | sed -n 6,14p; fi

# 4. un disco de los de antes (la partición empieza en el sector 63: no hay
# hueco para el arranque) y uno con tabla GPT: solo borrándolos
truncate -s 200M $Z/viejo.img
printf 'label: dos\n63,80M,7\n' | sfdisk -q $Z/viejo.img > /dev/null 2>&1
cp $Z/viejo.img $Z/viejo0.img
instala $Z/viejo.img viejo ":tecla ret" ":tecla ret" ":texto v1" ":tecla f3" ":tecla s"
if grep -q "su primera partición empieza demasiado" $Z/v1.txt && ! grep -q "Instalar en el espacio sin usar" $Z/v1.txt && cmp -s $Z/viejo.img $Z/viejo0.img; then
  ok "un disco de los de antes (la partición en el sector 63): se dice que no hay sitio para el arranque sin borrar, y no se toca"
else ko "el disco con la partición en el sector 63 no se trata bien:"; sed 's/^/        /' $Z/v1.txt | sed -n 6,14p; fi
truncate -s 200M $Z/gpt.img
parted -s $Z/gpt.img mklabel gpt mkpart datos ntfs 1MiB 60MiB mkpart linux ext4 60MiB 120MiB
instala $Z/gpt.img gpt ":tecla ret" ":texto g1" ":tecla ret" ":texto g2" ":tecla ret" ":tecla s" ":escribe BORRAR" ":tecla ret" ":tecla ret" ":pausa 5" ":texto g3" ":tecla ret"
tabla $Z/gpt.img
if grep -q "partición 1: NTFS o exFAT (Windows)  *59 MB" $Z/g1.txt && grep -q "partición 2: Linux  *60 MB" $Z/g1.txt && grep -q "Usa una tabla de particiones GPT" $Z/g2.txt \
  && ! grep -q "Instalar en el espacio sin usar" $Z/g2.txt && grep -q "Hispa-Pentium está instalado" $Z/g3.txt && grep -q "^Partition Table: msdos" $Z/tabla.txt \
  && volumen $Z/gpt.img 2048 && instalado $Z/gpt.img; then
  ok "un disco con tabla GPT: se leen sus particiones; solo se instala borrándolo entero (queda con tabla MBR, sin rastro de la GPT), sano y con el sistema"
else ko "el disco con tabla GPT no se trata bien:"; cat $Z/tabla.txt $Z/fsck.txt | sed 's/^/        /' | head -8; sed 's/^/        /' $Z/g2.txt | sed -n 6,14p; fi

# 5. un disco IDE (el canal de siempre), de 2 GB: la FAT32 con clusters de 4 K
truncate -s 2G $Z/ide.img
IDE=1 instala $Z/ide.img ide ":tecla ret" ":texto d1" ":tecla ret" ":tecla ret" ":pausa 9" ":texto d2" ":tecla ret"
if grep -q "Disco 1   IDE   QEMU HARDDISK  *2048 MB" $Z/d1.txt && grep -q "^instalar: FAT32 con clusters de 4096 octetos, [0-9]* clusters, [0-9]* sectores por FAT$" $S \
  && grep -q "Hispa-Pentium está instalado" $Z/d2.txt && volumen $Z/ide.img 2048 && instalado $Z/ide.img; then
  ok "un disco IDE de 2 GB: instalado, con su FAT32 de clusters de 4 K sana y el sistema dentro"
else ko "instalar en un disco IDE de 2 GB no va:"; sed 's/^/        /' $Z/fsck.txt | head -6; tail -n 4 $S | sed 's/^/        /'; fi

# 6. sin «instalar» en la línea del cargador, el núcleo no deja escribir en
# los discos sector a sector: INSTALAR no encuentra dónde, aunque haya «escritura»
truncate -s 100M $Z/sin.img
dd if=/dev/urandom of=$Z/sin.img bs=1M count=1 conv=notrunc status=none
cp $Z/sin.img $Z/sin0.img
CAPTURAS=$Z ESCRIBE=1 SATA=$Z/sin.img MODULOS=$INST ARRANQUE="ordenes escritura unidad=M" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" instalar ":tecla ret" ":texto n1" ":tecla f3" > $Z/sin.sal 2>&1
S=$Z/sin.sal
if grep -q "^instalar: disco 0, SATA, 100 MB, se puede: 0$" $S && grep -q "No se ha encontrado ningún disco duro en este equipo" $Z/n1.txt && cmp -s $Z/sin.img $Z/sin0.img; then
  ok "sin «instalar» en la línea del cargador (aunque haya «escritura»), INSTALAR no puede tocar ningún disco: el núcleo no le deja"
else ko "sin «instalar» en la línea del cargador, INSTALAR ve discos donde escribir:"; sed 's/^/        /' $Z/n1.txt | sed -n 4,12p; fi

# 7. de verdad: la memoria USB arrancada como tal (GRUB), su entrada «Instalar
# Hispa-Pentium en el disco duro», un disco SATA en blanco de 2 GB; luego, SOLO
# el disco: GRUB desde su MBR, el escritorio y el asistente del primer arranque
monz() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$Z/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
teclas() { local k; for k in "$@"; do monz "sendkey $k" 0.12; done; }
textoz() {     # la pantalla de texto, a $Z/$1.txt
  monz "pmemsave 0xb8000 4000 \"$Z/scr.bin\"" 0.3
  python3 -I -c "
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())" $Z/scr.bin > $Z/$1.txt 2> /dev/null
}
espera_a() {   # espera (hasta $2 segundos) a que salga esa línea por el puerto serie ($3)
  local i
  for ((i = 0; i < $2 * 2; i++)); do grep -qa -- "$1" $3 2> /dev/null && return 0; sleep 0.5; done
  return 1
}
espera_menu() {    # espera (hasta 15 s) a que el menú de GRUB esté en la pantalla, con ese texto; queda en $Z/$2.txt
  local i
  for ((i = 0; i < 40; i++)); do
    textoz $2
    # (ya está: un momento más, que acabe de pintarse, y otra vez)
    if grep -q -- "$1" $Z/$2.txt; then sleep 0.6; textoz $2; return 0; fi
    sleep 0.1
  done
  return 1
}
desde_disco() {    # arranca solo del disco $1 (SATA); lo que sale, a $2
  rm -f $Z/mon
  qemu-system-i386 -nic none -m 64 -display none -monitor unix:$Z/mon,server,nowait -serial file:$2 -no-reboot -rtc base=localtime \
    -device ahci,id=sata -drive id=dsata,file=$1,format=raw,if=none -device ide-hd,drive=dsata,bus=sata.0,bootindex=1 > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $Z/mon ] && break; sleep 0.1; done
}
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $G/usb.img $Z/u$hc.img
  rm -f $Z/d$hc.img; truncate -s 2G $Z/d$hc.img
  rm -f $Z/mon
  qemu-system-i386 -nic none -m 64 -display none -monitor unix:$Z/mon,server,nowait -serial file:$Z/i$hc.sal -no-reboot -rtc base=localtime -device $dev \
    -drive file=$Z/u$hc.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1 \
    -device ahci,id=sata -drive id=dsata,file=$Z/d$hc.img,format=raw,if=none -device ide-hd,drive=dsata,bus=sata.0,bootindex=2 > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $Z/mon ] && break; sleep 0.1; done
  espera_menu "Instalar Hispa-Pentium" m$hc; teclas down down down down down down down down down; textoz m2$hc; teclas ret
  espera_a "^instalar: bienvenida" 40 $Z/i$hc.sal; sleep 0.5
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i1-bienvenida.ppm" 0.5
  teclas ret; sleep 1; textoz u1$hc
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i2-discos.ppm" 0.5
  teclas ret; sleep 1
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i3-listo.ppm" 0.5
  teclas ret
  espera_a "^instalar: terminado" 60 $Z/i$hc.sal; sleep 0.5; textoz u2$hc
  [ $hc = xhci ] && [ -n "${CAPTURAS_A:-}" ] && monz "screendump $G/i4-final.ppm" 0.5
  teclas ret; sleep 1.5
  kill -0 $QPID 2> /dev/null && monz quit 0.5
  wait $QPID 2> /dev/null; QPID=
  tr -d '\r' < $Z/i$hc.sal | iconv -f CP437 -t UTF-8 > $Z/i$hc.txt
  S=$Z/i$hc.txt
  if grep -q "Instalar Hispa-Pentium 0.8 en el disco duro" $Z/m$hc.txt && grep -q "^| │\*Instalar Hispa-Pentium 0.8 en el disco duro" $Z/m2$hc.txt; then
    ok "$que: el menú de la memoria USB trae la entrada «Instalar Hispa-Pentium 0.8 en el disco duro» (la última)"
  else ko "$que: el menú de la memoria USB no trae la entrada de instalar:"; sed 's/^/        /' $Z/m2$hc.txt | head -14; fi
  mira "$que: arrancando de la memoria USB con «instalar», el núcleo lanza INSTALAR con lo que trae el cargador (la unidad M:) y se instala en el disco SATA" \
    "^usb: controladora $que " "^disco 0: QEMU HARDDISK, 2048 MB, sin FAT (SATA)\.$" "la del sistema, M:$" "^lanzo INSTALAR.BIN del disco" \
    "^instalar: disco 0, SATA, 2048 MB, se puede: 1$" "^instalar: disco 1, USB, 64 MB, se puede: 0$" "^instalar: partición 1, desde el sector 2048, 4192256 sectores$" \
    "^instalar: copiado CORE.IMG" "^instalar: terminado$" "^reiniciar$"
  if grep -q "Disco 1   SATA  QEMU HARDDISK  *2048 MB" $Z/u1$hc.txt && grep -q "Memoria USB  QEMU    QEMU HARDDISK  *64 MB  (no se toca)" $Z/u1$hc.txt \
    && [ "$(grep -c '^|    Disco ' $Z/u1$hc.txt)" = 1 ] && grep -q "Hispa-Pentium está instalado" $Z/u2$hc.txt && cmp -s $Z/u$hc.img $G/usb.img; then
    ok "$que: la memoria USB de arranque no se ofrece como disco (se enseña: «no se toca») y, después de instalar, está idéntica octeto a octeto"
  else ko "$que: la memoria USB de arranque se ofrece como disco, o ha cambiado al instalar:"; sed 's/^/        /' $Z/u1$hc.txt | sed -n 5,12p; fi
  if volumen $Z/d$hc.img 2048 && instalado $Z/d$hc.img usb; then ok "$que: el disco de 2 GB queda con su FAT32 sana (fsck.fat), el sistema en \\HISPA y el arranque puesto"
  else ko "$que: el disco instalado desde la memoria USB no queda bien:"; sed 's/^/        /' $Z/fsck.txt | head -6; fi
  # lo instalado, apuntado para PAQUETES: los registros de los paquetes del sistema, en \HISPA\PAQUETES
  rm -rf $Z/pqi $Z/pqu; mkdir $Z/pqi $Z/pqu
  mcopy -n -i $Z/vol.img '::HISPA/PAQUETES/*' $Z/pqi/ > /dev/null 2>&1; mcopy -n -i $G/usb.img@@1M '::INSTALA/PAQUETES/*' $Z/pqu/ > /dev/null 2>&1
  if [ "$(ls $Z/pqi | tr '\n' ' ')" = "ACCESORI.PQI BASE.PQI BLOC.PQI BLUETOOT.PQI CONSOLA.PQI EJEMPLOS.PQI EQUIPO.PQI HVPRUN.PQI OFICINA.PQI PAQUETES.PQI PINTURA.PQI RED.PQI SONIDO.PQI SSH.PQI TAILSCAL.PQI TLS.PQI WIFI.PQI WIN32.PQI " ] && diff -r $Z/pqi $Z/pqu > /dev/null \
    && mcopy -n -i $Z/vol.img ::HISPA/PAQUETES.BIN $Z/f.bin > /dev/null 2>&1; then
    ok "$que: INSTALAR apunta lo que instala: los registros de los 18 paquetes del sistema (OFICINA2: con pintura; BLUETOOTH: con bluetoot), en C:\\HISPA\\PAQUETES (y PAQUETES.BIN en \\HISPA)"
  else ko "$que: lo instalado no queda apuntado en C:\\HISPA\\PAQUETES:"; ls $Z/pqi | sed 's/^/        /' | head -10; fi
  # SOLO el disco: el primer arranque
  desde_disco $Z/d$hc.img $Z/a$hc.sal
  espera_menu "Hispa-Pentium: el interprete" a0$hc
  espera_a "asistente: el primer arranque" 50 $Z/a$hc.sal; sleep 1.5
  monz "screendump $G/a1$hc.ppm" 0.6
  if [ $hc = xhci ]; then
    # el nombre, el del equipo, la hora (las 07:45), el teclado como está y la pantalla a 1024 x 600
    teclas shift-c a r l o s tab backspace backspace backspace backspace backspace shift-t o s h i b a tab tab backspace backspace backspace backspace backspace 0 7 shift-dot 4 5 tab tab right
    monz "screendump $G/a2$hc.ppm" 0.6
    teclas ret; sleep 2
    monz "screendump $G/a3$hc.ppm" 0.6
    teclas ret; sleep 1
    monz "mouse_move -360 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/a4$hc.ppm" 0.6
  else
    # con el ratón: clic en «de EE. UU.», se teclea el nombre (con el teclado de EE. UU. ya puesto) y clic en Aceptar
    monz "mouse_move 30 12" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 0.6
    monz "mouse_move -100 -136" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 0.6
    teclas shift-a n a shift-semicolon
    monz "screendump $G/a2$hc.ppm" 0.6
    monz "mouse_move 150 266" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 2
    monz "screendump $G/a3$hc.ppm" 0.6
  fi
  monz quit 0.5; wait $QPID 2> /dev/null; QPID=
  tr -d '\r' < $Z/a$hc.sal | iconv -f CP437 -t UTF-8 > $Z/a$hc.txt
  S=$Z/a$hc.txt
  if grep -q "GNU GRUB" $Z/a0$hc.txt && grep -q "^| │\*Hispa-Pentium  " $Z/a0$hc.txt && grep -q "Hispa-Pentium: el interprete de ordenes (modo texto)" $Z/a0$hc.txt; then
    ok "$que: SOLO con el disco (sin la memoria USB), la BIOS arranca GRUB desde su MBR y sale el menú de \\HISPA\\GRUB.CFG"
  else ko "$que: el disco instalado no arranca solo (no sale su menú):"; sed 's/^/        /' $Z/a0$hc.txt | head -8; fi
  mira "$que: el primer arranque desde el disco: el núcleo lo lee (C:, FAT32), el escritorio a 800 x 600 y el asistente (de \\HISPA)" \
    "^disco 0: QEMU HARDDISK, 2048 MB, FAT32 (SATA)\.$" "^escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" \
    "^escritorio: lanzo .HISPA.ASISTE.BIN" "asistente: el primer arranque$"
  miracap "$que: el asistente del primer arranque, en su ventana: el nombre, el equipo, la fecha y la hora, el teclado y la pantalla" a1$hc \
    negrita 114 64 122 72 "Bienvenido a Hispa-Pentium" texto 112 164 120 172 "Su nombre:" texto 290 198 298 206 "HISPA" texto 112 266 120 274 "Hora (horas:minutos):" \
    texto 292 334 300 342 "800 x 600" negrita 454 427 462 435 "Aceptar"
  volumen $Z/d$hc.img 2048; sano=$?
  rm -f $Z/ini.txt $Z/cfg.txt; mcopy -n -i $Z/vol.img ::HISPA/HISPA.INI $Z/ini.txt > /dev/null 2>&1; mcopy -n -i $Z/vol.img ::HISPA/GRUB.CFG $Z/cfg.txt > /dev/null 2>&1
  if [ $hc = xhci ]; then
    miracap "el asistente, con el teclado: el nombre, el equipo, la hora y la pantalla (Tab de casilla en casilla; la flecha cambia la medida)" a2$hc \
      texto 290 164 298 172 "Carlos" texto 290 198 298 206 "Toshiba" texto 290 266 298 274 "07:45" texto 292 334 300 342 "1024 x 600"
    miracap "al aceptar: «Todo listo», con el saludo; y el reloj de la barra ya marca la hora puesta (el reloj del PC)" a3$hc \
      negrita 154 98 162 106 "Todo listo" negrita 112 176 120 184 "¡Hola, Carlos!" negrita 454 427 462 435 "Empezar" negrita 740 570 760 584 "07:45"
    miracap "el menú Inicio saluda con el nombre («Hola, Carlos») y dice la versión; LEEME.TXT, en los documentos" a4$hc \
      negrita 54 104 62 112 "Hola, Carlos" texto 54 122 62 130 "Hispa-Pentium, versión $VERSION_BASE" negrita 256 159 264 167 "Mis documentos" texto 38 246 46 254 Naves
    if grep -q "asistente: guardado HISPA.INI (Carlos, Toshiba)$" $S && [ "$(tr -d '\r' < $Z/ini.txt | tr '\n' '|')" = "[hispa]|usuario=Carlos|equipo=Toshiba|teclado=es|modo=1024x600|" ] \
      && grep -q "^set modo=1024x600x32,1024x600,auto$" $Z/cfg.txt && [ "$(grep -c '^set modo=' $Z/cfg.txt)" = 1 ] && [ $sano = 0 ]; then
      ok "lo guardado: C:\\HISPA\\HISPA.INI (usuario, equipo, teclado y modo) y la medida de la pantalla en el menú de arranque (GRUB.CFG); el disco, sano"
    else ko "el asistente no deja bien HISPA.INI o GRUB.CFG:"; cat $Z/ini.txt | sed 's/^/        /'; grep "set modo" $Z/cfg.txt | sed 's/^/        /'; sed 's/^/        /' $Z/fsck.txt | head -4; grep "asistente:" $S | sed 's/^/        /' | head -4; echo "        (fsck: $sano)"; fi
    # el segundo arranque: ya sin asistente, a 1024 x 600 y con el saludo
    desde_disco $Z/d$hc.img $Z/b$hc.sal
    espera_a "escritorio: en marcha" 50 $Z/b$hc.sal; sleep 3
    monz "mouse_move -472 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/b1$hc.ppm" 0.6
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $Z/b$hc.sal | iconv -f CP437 -t UTF-8 > $Z/b$hc.txt
    S=$Z/b$hc.txt
    if grep -q "^escritorio: en marcha, 1024 x 600 con 32 bits por punto" $S && ! grep -q "ASISTE.BIN" $S && cap $G/b1$hc.ppm tam 1024 600 negrita 54 104 62 112 "Hola, Carlos"; then
      ok "el segundo arranque: sin asistente, el escritorio a 1024 x 600 (la medida elegida, puesta por GRUB) y el menú Inicio con el saludo"
    else ko "el segundo arranque no sale como se dejó:"; grep "escritorio" $S | sed 's/^/        /' | head -4; sed 's/^/        /' $G/cap.txt | head -4; fi
  else
    miracap "el asistente, con el ratón: clic en «de EE. UU.» cambia el teclado en el momento (la tecla de la eñe da «:»), y clic en Aceptar" a2$hc \
      texto 290 164 298 172 "Ana:"
    miracap "y queda «Todo listo» con su saludo" a3$hc negrita 154 98 162 106 "Todo listo" negrita 112 176 120 184 "¡Hola, Ana:!"
    if grep -q "asistente: guardado HISPA.INI (Ana:, HISPA)$" $S && grep -q "^teclado=us" $Z/ini.txt && grep -q "^modo=800x600" $Z/ini.txt && grep -q "^set modo=800x600x32,800x600,auto$" $Z/cfg.txt; then
      ok "lo guardado con el ratón: el teclado de EE. UU. y la pantalla como estaba"
    else ko "el asistente, con el ratón, no guarda lo elegido:"; cat $Z/ini.txt | sed 's/^/        /'; fi
    # la tercera entrada del menú del disco (desde la 0.8.13 la segunda es la «SIN ACPI»): el intérprete de órdenes, en modo texto, con el teclado guardado
    desde_disco $Z/d$hc.img $Z/b$hc.sal
    espera_menu "Hispa-Pentium: el interprete" b0$hc; teclas down down ret
    espera_a "lanzo ORDENES.BIN" 40 $Z/b$hc.sal; sleep 1
    teclas v e r ret d i r spc backslash h i s p a backslash h i s p a dot i n i ret; sleep 1.5; textoz b1$hc
    # y PAQUETES, en el sistema instalado: lo instalado, y que sus archivos son los que se instalaron
    teclas c l s ret p a q u e t e s spc l i s t a ret p a q u e t e s spc c o m p r u e b a ret
    espera_a "^Los archivos de los\|no est.n como se instalaron" 60 $Z/b$hc.sal; sleep 0.5; textoz b2$hc
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    if grep -q "Hispa-Pentium $VERSION_BASE: un núcleo de 32 bits escrito en Castel" $Z/b1$hc.txt && grep -q "^|C:\\\\>dir \\\\hispa\\\\hispa.ini" $Z/b1$hc.txt && grep -q "^|.hispa.hispa.ini es un archivo de [0-9]* octetos" $Z/b1$hc.txt; then
      ok "la entrada de modo texto del disco: el intérprete de órdenes (de C:\\HISPA), en C:, con el teclado guardado en HISPA.INI (el de EE. UU.: su tecla «\\» da la barra)"
    else ko "la entrada de modo texto del disco no arranca el intérprete:"; sed 's/^/        /' $Z/b1$hc.txt | grep -v '^ *|$' | tail -n 8; fi
    if grep -q "^|  win32     1\.[0-9]     Programas de Windows de 32 bits" $Z/b2$hc.txt && grep -q "^|  tailscal  0\.8\.[0-9]   Tailscale y WireGuard" $Z/b2$hc.txt && grep -q "^|18 paquetes\.$" $Z/b2$hc.txt \
      && grep -q "^|Los archivos de los 18 paquetes instalados están como se instalaron\.$" $Z/b2$hc.txt; then
      ok "PAQUETES en el sistema recién instalado: LISTA dice los 18 paquetes del sistema (..., ssh, tailscal, sonido, tls, wifi y win32: con 17, base ya no cabe en la pantalla; (OFICINA2) con pintura; (BLUETOOTH) con bluetoot) y COMPRUEBA, que sus archivos son los instalados (SHA-256)"
    else ko "PAQUETES en el sistema recién instalado no ve bien lo instalado:"; sed 's/^/        /' $Z/b2$hc.txt | grep -v '^ *|$' | tail -n 12; fi
    # la oficina, en el disco instalado: el grupo «Oficina» del menú Inicio
    # (Facturas, con C:\\HISPA\\HVPRUN.BIN) y Mi equipo abriendo Cuentas de C:\\OFICINA
    desde_disco $Z/d$hc.img $Z/c$hc.sal
    espera_a "escritorio: en marcha" 50 $Z/c$hc.sal; sleep 3
    monz "mouse_move -360 285" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 1
    monz "screendump $G/c1$hc.ppm" 0.6
    monz "mouse_move 20 -150" 0.2; monz "mouse_move 20 -147" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 5
    monz "screendump $G/c2$hc.ppm" 0.6
    # (los saltos grandes, en dos: con la rueda, el ratón PS/2 manda cuatro
    # octetos y en la cola de QEMU, de 16, caben 4 paquetes de 127 puntos por
    # suceso; lo que sobra llegaría con el clic siguiente)
    monz "mouse_move 327 -127" 0.2; monz "mouse_move 327 -127" 0.4; monz "mouse_button 1" 0.2; monz "mouse_button 0" 2
    monz "mouse_move -336 -2" 0.2; monz "mouse_move -336 -2" 0.4; monz "mouse_button 1" 0.1; monz "mouse_button 0" 0.1; monz "mouse_button 1" 0.1; monz "mouse_button 0" 3
    teclas ret; sleep 2; teclas o ret; sleep 2
    monz "screendump $G/c3$hc.ppm" 0.6
    teclas c ret; sleep 5
    monz "screendump $G/c4$hc.ppm" 0.6
    monz quit 0.5; wait $QPID 2> /dev/null; QPID=
    tr -d '\r' < $Z/c$hc.sal | iconv -f CP437 -t UTF-8 > $Z/c$hc.txt
    S=$Z/c$hc.txt
    volumen $Z/d$hc.img 2048; sano=$?
    if grep -q "escritorio: lanzo .HVPRUN.BIN (la tarea [0-9]*)" $S && ! grep -q "ASISTE.BIN" $S \
      && cap $G/c1$hc.ppm texto 38 276 46 284 Facturas texto 38 246 46 254 Naves negrita 54 104 62 112 "Hola, Ana:" \
      && cap $G/c2$hc.ppm negrita 72 22 84 30 Facturas texto 62 126 74 138 "No hay facturas emitidas."; then
      ok "la oficina en el disco instalado: el menú Inicio (con su saludo) tiene el grupo «Oficina», y Facturas se abre con C:\\HISPA\\HVPRUN.BIN (FACTURAS.HVP, de C:\\OFICINA)"
    else ko "la oficina no se abre desde el menú Inicio del disco instalado:"; sed 's/^/        /' $G/cap.txt | head -4; grep "lanzo" $S | sed 's/^/        /' | head -4; fi
    if cap $G/c3$hc.ppm negrita 29 9 39 19 "C:\\OFICINA" texto 213 166 223 176 "CUENTAS.HV" texto 311 166 321 176 "DATOS.HVP" \
      && cap $G/c4$hc.ppm negrita 80 16 110 36 "Cuentas - (sin nombre)" && [ $sano = 0 ]; then
      ok "y Mi equipo entra en C:\\OFICINA y abre CUENTAS.HVP (Cuentas, con HVPRUN); el disco queda sano"
    else ko "Mi equipo no abre la oficina de C:\\OFICINA en el disco instalado:"; sed 's/^/        /' $G/cap.txt | head -4; sed 's/^/        /' $Z/fsck.txt | head -3; fi
  fi
done
else ko "usb.sh no puede hacer la imagen: $(tail -n 1 $G/usb.txt)"; fi
else
  echo "  (sin root o sin grub-install para PC no se prueba la instalación en el disco)"
fi
fi   # SOLO=08

# ------------------------------------------------- 11. El USB en caliente
# Lo que se enchufa y se desenchufa con el equipo en marcha (usbcal.cas),
# por el monitor de QEMU: «:monitor drive_add ...» y «:monitor device_add
# usb-storage...» enchufan una memoria; «:monitor device_del ...», la quitan.
if parte08 caliente; then
echo "== Hispa-Pentium 0.8: el USB en caliente (enchufar y quitar memorias, teclados, ratones y concentradores en marcha) =="
Q=$P/n08
mkdir -p $Q $G
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null 2>&1 && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN
# una memoria con un texto, y otras vacías (FAT16, 16 MB)
memoria() { rm -f $1; dd if=/dev/zero of=$1 bs=1M count=16 status=none; mkfs.fat -F 16 -n ${2:-MEMORIA} $1 > /dev/null; }
memoria $Q/m1.img MEMORIA1
printf 'hola desde la memoria enchufada\r\n' > $Q/HOLA.TXT
MTOOLS_SKIP_CHECK=1 mcopy -i $Q/m1.img $Q/HOLA.TXT ::HOLA.TXT
# 1 y 2. en xHCI y en EHCI, con el intérprete de órdenes (del disco IDE) y sin
# teclado ni ratón PS/2: un teclado que llega (y se va, y vuelve); una
# memoria que se lee y que se quita a lo bruto con un archivo abierto (PRUEBA
# abierto); otra, que se escribe y se quita con seguridad; y en el xHCI, un
# concentrador con un teclado y un ratón detrás, que se quita entero
for hc in xhci ehci; do
  case $hc in xhci) dev="qemu-xhci,id=u"; que="xHCI" ;; ehci) dev="usb-ehci,id=u"; que="EHCI" ;; esac
  cp $P/d16.img $Q/c$hc.img; cp $Q/m1.img $Q/a$hc.img; memoria $Q/b$hc.img MEMORIA2
  CONC=(":pausa 0.1")
  [ $hc = xhci ] && CONC=(":monitor device_del k1" ":pausa 2" ":monitor device_add usb-hub,bus=u.0,port=3,id=h1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,port=3.1,id=k2" \
    ":monitor device_add usb-mouse,bus=u.0,port=3.2,id=r2" ":pausa 3" "eco dos" ":monitor device_del h1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 2")
  ESCRIBE=1 DISCO=$Q/c$hc.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE=ordenes QEMU_MAS="-device $dev" \
    $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 2" "eco uno" \
    ":monitor drive_add 0 if=none,id=ma,file=$Q/a$hc.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=ma,id=mema" ":pausa 3" \
    "type w:\\hola.txt" "prueba abierto w:\\hola.txt" ":pausa 1" ":monitor device_del mema" ":pausa 2" ":tecla ret" ":pausa 1" "dir w:" \
    "${CONC[@]}" ":monitor drive_add 0 if=none,id=mb,file=$Q/b$hc.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=mb,id=memb" ":pausa 3" \
    "copy textos\\largo.txt w:\\largo.txt" "md w:\\nueva" "copy leeme.txt w:\\nueva\\leeme.txt" "eco tres" salir "quitar w" "disco" \
    ":monitor device_del memb" ":pausa 2" usb apagar > $Q/$hc.sal 2>&1
  S=$Q/$hc.sal
  mira "$que: un teclado USB enchufado con el intérprete en marcha teclea al momento" \
    "usb: se enchufa el aparato 0, 0627:0001, velocidad [a-z]*, en el puerto [0-9] ($que); teclado$" "eco uno$" "^uno$"
  mira "$que: una memoria enchufada en marcha sale como W: (y el aviso) y se lee" \
    "($que); almacenamiento$" "^disco 2: QEMU    QEMU HARDDISK, 16 MB (USB), recién enchufado, W: (FAT16)\.$" "usb: aviso: Memoria USB conectada: W:$" \
    "^hola desde la memoria enchufada$"
  mira "$que: quitada a lo bruto con un archivo abierto: el programa ve errores y sigue, y la unidad ya no está" \
    "^abierto: leídos 16 octetos; pulse una tecla$" "usb: aviso: Memoria USB desconectada sin «quitar con seguridad»: W:$" "^abierto: al leer otra vez: el disco falla$" \
    "^abierto: al escribir: el disco falla$" "^abierto: al abrirlo otra vez: no hay disco" "^abierto: sigo en marcha, y acabo$" "^No puedo abrir w:: no hay disco"
  if [ $hc = xhci ]; then
    mira "$que: un concentrador enchufado en marcha, con un teclado y un ratón detrás: el teclado teclea; y al quitarlo se va todo, y otro teclado vuelve a ir" \
      "usb: se desenchufa el aparato 0$" ": concentrador de 8 puertos$" "en el puerto 1 del concentrador [0-9]*; teclado$" "en el puerto 2 del concentrador [0-9]*; ratón$" \
      "usb: 1 teclado, 1 ratón\.$" "^dos$" "usb: 1 teclado, 0 ratones\.$" "^tres$"
  else
    mira "$que: el teclado sigue tecleando después de quitar y enchufar memorias" "^tres$"
  fi
  mira "$que: la otra memoria (W: otra vez) se escribe y se quita con seguridad: «Ya puede quitar la memoria W:»" \
    "^ *1 archivo(s) copiado(s)\.$" "^> quitar w$" "^disco 2: quitado con seguridad\.$" "usb: aviso: Ya puede quitar la memoria W:$"
  saca $Q/b$hc.img LARGO.TXT $Q/largo$hc.txt; saca $Q/b$hc.img NUEVA/LEEME.TXT $Q/leeme$hc.txt
  if sano $Q/b$hc.img && sano $Q/a$hc.img && [ "$(wc -c < $Q/largo$hc.txt 2> /dev/null)" = 17600 ] && [ -s $Q/leeme$hc.txt ] && [ "$(grep -c "sin «quitar con seguridad»" $S)" = 1 ]; then
    ok "$que: lo escrito está en la memoria quitada con seguridad, y las dos quedan sanas (fsck.fat)"
  else ko "$que: la memoria quitada con seguridad no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -6; fi
done
# 3. un teclado de USB 1.1 enchufado en un EHCI con compañeras: va a la UHCI
cp $P/d16.img $Q/cu.img
ESCRIBE=1 DISCO=$Q/cu.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE=ordenes QEMU_MAS="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,port=1,usb_version=1,id=k1" ":pausa 3" "eco uno" \
  ":monitor device_del k1" ":pausa 2" ":monitor device_add usb-kbd,bus=u.0,port=3,usb_version=1,id=k2" ":pausa 3" "eco dos" salir apagar > $Q/uhci.sal 2>&1
S=$Q/uhci.sal
mira "UHCI: un teclado de USB 1.1 enchufado en marcha en un puerto del EHCI pasa a su compañera y teclea; quitado y enchufado en otro, también" \
  "usb: se enchufa el aparato 0, 0627:0001, velocidad plena, en el puerto 1 (UHCI); teclado$" "^uno$" "usb: se desenchufa el aparato 0$" "^dos$"
# 4. la memoria del sistema (X:, de la que se ha arrancado) no se deja quitar
cp $P/d16.img $Q/x.img
ESCRIBE=1 MODULOS= ARRANQUE="escritura=usb unidad=X" QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/x.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN "quitar x" "quitar c" "disco" apagar > $Q/x.sal 2>&1
S=$Q/x.sal
mira "la memoria del sistema (X:) no se deja quitar con seguridad: lo dice, y sigue montada" \
  "usb: aviso: X: es la memoria del sistema: no se puede quitar$" "usb: aviso: No hay unidad C:$" "^unidades: X: (disco 0, USB, FAT16); la del sistema, X:$"
# 5. en el escritorio (EHCI, con el teclado USB y sin ratón): un ratón que
# llega abre Mi equipo; una memoria que llega sale en Mi equipo (que se pone
# al día solo), en el globo de los avisos y en la bandeja; «Quitar con
# seguridad» desde Mi equipo (botón secundario en la unidad) y desde la bandeja
cp $P/d16.img $Q/ce.img; cp $Q/m1.img $Q/ea.img; memoria $Q/eb.img MEMORIA3
CAPTURAS=$G ESCRIBE=1 DISCO=$Q/ce.img MODULOS= MAQUINA=pc,i8042=off ARRANQUE="escritorio escritura" QEMU_MAS="-device usb-ehci,id=u -device usb-kbd,bus=u.0" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":monitor device_add usb-mouse,bus=u.0,id=r1" ":pausa 2" ":doble 62 30" ":pausa 2" \
  ":monitor drive_add 0 if=none,id=ma,file=$Q/ea.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=ma,id=mema" ":pausa 2.5" ":captura u1" \
  ":clicd 330 222" ":pausa 0.5" ":captura u2" ":clic 370 257" ":pausa 1.5" ":captura u3" ":monitor device_del mema" ":pausa 2" \
  ":monitor drive_add 0 if=none,id=mb,file=$Q/eb.img,format=raw" ":monitor device_add usb-storage,bus=u.0,drive=mb,id=memb" ":pausa 2.5" ":raton 600 450" ":pausa 6" \
  ":clic 720 583" ":pausa 0.5" ":captura u4" ":clic 680 549" ":pausa 1.5" ":captura u5" ":monitor device_del memb" ":pausa 2" > $Q/escritorio.sal 2>&1
S=$Q/escritorio.sal
mira "escritorio: un ratón USB enchufado en marcha mueve la flecha y abre Mi equipo con dos clics" \
  "usb: se enchufa el aparato 1, 0627:0001, velocidad alta, en el puerto 2 (EHCI); ratón$" "^escritorio: lanzo .EQUIPO.BIN"
miracap "escritorio: la memoria que llega sale en el globo de los avisos, en la bandeja (su icono) y en Mi equipo, que se pone al día solo" u1 \
  texto 570 532 590 548 "Memoria USB conectada: W:" negrita 570 510 590 526 "USB" negrita 216 190 460 212 "(W:)" rgb 721 585 2463C2 40
miracap "escritorio: Mi equipo, botón secundario en la memoria: «Abrir» y «Quitar con seguridad»" u2 texto 335 242 355 267 "Quitar con seguridad"
miracap "escritorio: quitada con seguridad desde Mi equipo: el globo «Ya puede quitar la memoria W:», y en Mi equipo ya no está" u3 \
  texto 540 532 600 548 "Ya puede quitar la memoria W:" texto 100 474 130 490 "Ya puede quitar la memoria W:." notexto 216 190 460 212 "(W:)"
miracap "escritorio: el icono de la bandeja saca «Quitar con seguridad W:» de la otra memoria" u4 texto 580 535 600 560 "Quitar con seguridad W:"
mira "escritorio: y desde la bandeja también se quita con seguridad; las dos memorias quedan sanas" \
  "^escritorio: globo: Ya puede quitar la memoria W:$" "^disco 2: quitado con seguridad\.$"
if [ "$(grep -c "^escritorio: globo: Ya puede quitar la memoria W:$" $S)" = 2 ] && ! grep -q "sin «quitar con seguridad»" $S && sano $Q/ea.img && sano $Q/eb.img; then
  ok "escritorio: las dos memorias se han quitado con seguridad (ninguna «a lo bruto») y quedan sanas (fsck.fat)"
else ko "escritorio: las memorias no se han quitado con seguridad, o no quedan sanas:"; grep "globo\|aviso" $S | sed 's/^/        /' | head -8; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el USB en caliente)"
fi   # el USB en caliente

# ------------------------------------------------- 12. Hispa-Pentium 0.8: los paquetes
# El programa PAQUETES (upaquete.cas) en un sistema «instalado» (un disco SATA
# con C:\HISPA y su base de lo instalado, C:\HISPA\PAQUETES) y una memoria USB
# (X:) con paquetes .HPQ y su CATALOGO.TXT, hechos con tools/hpq.py: instalar
# con dependencias (de un archivo y por su nombre), actualizar, lo que no
# vale (una suma mala, un conflicto, quitar lo que otro necesita), quitar,
# comprobar lo instalado, un corte a mitad (se deshace al volver) y el gancho
# de la red (con un DESCARGA de mentira). Después de cada paso, fsck.fat y
# mtools miran el disco desde fuera.
if parte08 paquetes; then
echo "== Hispa-Pentium 0.8: los paquetes (PAQUETES, .HPQ): instalar con dependencias, actualizar, quitar, deshacer un corte =="
Q=$P/n08p
mkdir -p $Q/f/paq
HPQ="python3 -I $AQUI/../tools/hpq.py"
export MTOOLS_SKIP_CHECK=1
hechos=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null 2>&1 && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN || hechos=0
# (PAQUETES, con una clave de prueba: la pública, en un clave.cas al lado de
# una copia de upaquete.cas, que el compilador encuentra antes que el de
# nucleo32; la privada, solo aquí: firma los catálogos de prueba)
$HPQ clave $Q/prueba.key > /dev/null && $HPQ publica $Q/prueba.key $Q/clave.cas > /dev/null || hechos=0
cp $AQUI/nucleo32/upaquete.cas $Q/upaquete.cas
for p in uordenes:ORDENES upaquete:PAQUETES uactual:ACTUALIZ uhola:HOLA; do
  fu=$AQUI/nucleo32/${p%%:*}.cas; [ ${p%%:*} = upaquete ] && fu=$Q/upaquete.cas
  python3 -I $AQUI/ccastel.py $fu -o $Q/${p%%:*}.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/${p%%:*}.asm -o $Q/f/${p##*:}.BIN || hechos=0
done
# un DESCARGA de mentira, para el gancho de la red: «DESCARGA http://prueba/ARCHIVO
# DESTINO» copia X:\PAQUETES\ARCHIVO a DESTINO (el de verdad lo trae por HTTP)
cat > $Q/udescar.cas <<'CAS'
módulo UDescar
    modelo usuario
    importar Llamadas, Textos32
    variable a: arreglo [400] de byte
    variable url: arreglo [200] de byte
    variable destino: arreglo [200] de byte
    variable origen: arreglo [200] de byte
    variable trozo: arreglo [4096] de byte
    función principal(): byte
        variable m, s, n: nat32
        argumentos(a, 400)
        si no palabra(a, 0, url, 200) o no palabra(a, 1, destino, 200) entonces devolver 2 fin
        si no empieza_por(url, "http://prueba/") entonces devolver 3 fin
        copiar(origen, "X:\\PAQUETES\\")
        concatenar(origen, dirección(url[14]))
        m := abrir(origen)
        si es_error(m) entonces devolver 4 fin
        s := crear(destino)
        si es_error(s) entonces devolver 5 fin
        n := leer(m, trozo, 4096)
        mientras no es_error(n) y n > 0 hacer
            escribir_archivo(s, trozo, n)
            n := leer(m, trozo, 4096)
        fin
        cerrar(s)
        cerrar(m)
        escribir("descarga (de prueba): ", url, " -> ", destino, "\n")
        devolver 0
    fin
fin UDescar.
CAS
python3 -I $AQUI/ccastel.py $Q/udescar.cas -o $Q/udescar.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/udescar.asm -o $Q/f/DESCARGA.BIN || hechos=0
# los paquetes de prueba: letras (1.0 y 1.1), saludo (1.0, que necesita
# letras >= 1.0, base y el núcleo; 2.0, que necesita letras >= 1.1, cambia
# un archivo, trae otro y deja de traer el grande), choque (no puede estar con
# saludo) y malo (con un octeto cambiado: su suma no es la suya)
F=$Q/f
printf 'letras A\r\n' > $F/A.TXT; printf 'letras B (1.0)\r\n' > $F/B.TXT; printf 'letras B (1.1)\r\n' > $F/B11.TXT
printf 'saludo, la 2\r\n' > $F/S2.TXT; head -c 70000 /dev/urandom > $F/GRANDE.DAT
( cd $F
  $HPQ crea BASE.HPQ --nombre base --version 0.7 --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' DESCARGA.BIN='\HISPA\DESCARGA.BIN'
  $HPQ registro BASE.HPQ BASE.PQI
  $HPQ crea paq/LETRAS1.HPQ --nombre letras --version 1.0 --descripcion 'Unas letras' A.TXT='\DOCS\LETRAS\A.TXT' B.TXT='\DOCS\LETRAS\B.TXT'
  $HPQ crea paq/LETRAS11.HPQ --nombre letras --version 1.1 --descripcion 'Unas letras, mejores' A.TXT='\DOCS\LETRAS\A.TXT' B11.TXT='\DOCS\LETRAS\B.TXT'
  $HPQ crea paq/SALUDO1.HPQ --nombre saludo --version 1.0 --descripcion 'Un programa que saluda' --necesita 'letras >= 1.0, base >= 0.7, nucleo >= 0.7' HOLA.BIN='\HISPA\SALUDO.BIN' GRANDE.DAT='\HISPA\SALUDO.DAT'
  $HPQ crea paq/SALUDO2.HPQ --nombre saludo --version 2.0 --descripcion 'Un programa que saluda (la 2)' --necesita 'letras >= 1.1, nucleo >= 0.7' HOLA.BIN='\HISPA\SALUDO.BIN' S2.TXT='\HISPA\SALUDO.TXT'
  $HPQ crea paq/CHOQUE.HPQ --nombre choque --version 1.0 --descripcion 'No puede estar con saludo' --choca 'saludo' A.TXT='\CHOQUE.TXT'
  $HPQ catalogo paq --clave $Q/prueba.key
  $HPQ crea paq/MALO.HPQ --nombre malo --version 1.0 --descripcion 'Estropeado' A.TXT='\MALO1.TXT' GRANDE.DAT='\MALO2.DAT'
  python3 -I -c "
d = bytearray(open('paq/MALO.HPQ', 'rb').read()); d[-5000] ^= 1; open('paq/MALO.HPQ', 'wb').write(d)"
  $HPQ registro paq/SALUDO1.HPQ SALUDO1.PQI; $HPQ registro paq/SALUDO2.HPQ SALUDO2.PQI ) > $Q/hpq.txt 2>&1 || hechos=0
# el disco del sistema (FAT32, con C:\HISPA y lo instalado: base 0.7) y la memoria USB
mkfs.fat -F 32 -s 1 -n HISPA -C $Q/sis.img 65536 > /dev/null && mmd -i $Q/sis.img ::HISPA ::HISPA/PAQUETES \
  && mcopy -i $Q/sis.img $F/ORDENES.BIN $F/PAQUETES.BIN $F/DESCARGA.BIN ::HISPA/ && mcopy -i $Q/sis.img $F/BASE.PQI ::HISPA/PAQUETES/ || hechos=0
mkfs.fat -F 16 -n MEMORIA -C $Q/usb.img 16384 > /dev/null && mmd -i $Q/usb.img ::PAQUETES && mcopy -i $Q/usb.img $F/paq/* ::PAQUETES/ || hechos=0
# (y, para la parte 10, un «base» 0.9 y 0.9.1 que cambian el núcleo:
# \HISPA\NUCLEO.BIN; van en la memoria, fuera del catálogo)
head -c 5000 /dev/urandom > $F/NUC07.BIN; head -c 6000 /dev/urandom > $F/NUC09.BIN; head -c 7000 /dev/urandom > $F/NUC091.BIN
( cd $F
  for v in 09:0.9 091:0.9.1; do
    $HPQ crea BASE${v%%:*}.HPQ --nombre base --version ${v##*:} --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' DESCARGA.BIN='\HISPA\DESCARGA.BIN' NUC${v%%:*}.BIN='\HISPA\NUCLEO.BIN'
  done ) >> $Q/hpq.txt 2>&1 || hechos=0
mcopy -i $Q/usb.img $F/BASE09.HPQ $F/BASE091.HPQ ::PAQUETES/ || hechos=0
if [ $hechos = 1 ] && $HPQ mira $F/paq/SALUDO1.HPQ > $Q/mira.txt && ! $HPQ mira $F/paq/MALO.HPQ > $Q/mira2.txt && grep -q '^paquete=saludo 2.0 SALUDO2.HPQ 1[0-9]* [0-9a-f]\{64\} Un programa que saluda (la 2)' $F/paq/CATALOGO.TXT \
  && grep -q 'MALO2.DAT  *70000  MAL' $Q/mira2.txt && head -1 $F/BASE.PQI | grep -q '^HPQ 1'; then
  ok "tools/hpq.py: hace los paquetes (crea), su catálogo (CATALOGO.TXT, con el tamaño y el SHA-256 de cada .HPQ), los registros (.PQI) y mira las sumas (la de MALO.HPQ, mal)"
else ko "tools/hpq.py no hace bien los paquetes de prueba:"; tail -n 4 $Q/hpq.txt | sed 's/^/        /'; fi
# PAQUETES en el sistema del disco $1, con la memoria USB: lo que sale, a $Q/$2.sal (y en $S)
paq() {
  local d=$1 n=$2; shift 2
  ESCRIBE=1 SATA=$d MEMORIA=64 MODULOS= ARRANQUE=ordenes QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=${USB_IMG:-$Q/usb.img},format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 2" "$@" ":pausa 1" > $Q/$n.sal 2>&1
  S=$Q/$n.sal
}
# lo que hay en el disco: la lista entera de mdir y fsck.fat; sin restos de una instalación
lista_de() { mdir -/ -b -i $1 :: 2> /dev/null | sort; }
sin_restos() { ! lista_de $1 | grep -q 'HPQ[0-9]*\.\(NUE\|VIE\)\|DIARIO\.TXT\|HECHO\.TXT'; }
igual_en() { rm -f $Q/x.bin; mcopy -n -i $1 "::$2" $Q/x.bin > /dev/null 2>&1 && cmp -s $Q/x.bin $3; }

# 1. lo instalado, el catálogo de la memoria USB (lo encuentra solo: X:\PAQUETES)
# e instalar un archivo de paquete de la memoria, con lo que necesita (del catálogo)
cp $Q/sis.img $Q/s1.img
paq $Q/s1.img uno "paquetes lista" "paquetes catalogo" 'paquetes instala x:\paquetes\saludo1.hpq' ":pausa 3" "paquetes lista" saludo
mira "PAQUETES LISTA: lo apuntado en C:\\HISPA\\PAQUETES (base 0.7); CATALOGO: lo de la memoria USB (X:\\PAQUETES, que encuentra solo), cada uno con su versión más nueva" \
  "^  base      0.7     El sistema base (de prueba)$" "^1 paquetes\.$" "^Paquetes que hay en X:.PAQUETES.:$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^  saludo    2\.0  *Un programa que saluda (la 2)$"
mira "PAQUETES INSTALA X:\\PAQUETES\\SALUDO1.HPQ: antes, lo que necesita (letras, la más nueva del catálogo; base y el núcleo ya están), comprobando las sumas de todo; y el programa nuevo se lanza por su nombre (PATH con C:\\HISPA)" \
  "^Se va a instalar: letras 1\.1, saludo 1\.0$" "^Las sumas (SHA-256) de todos los archivos están bien\.$" "^  instalado letras 1\.1$" "^  instalado saludo 1\.0$" "^Hecho\.$" \
  "^  saludo    1\.0     Un programa que saluda$" "^3 paquetes\.$" "^hola desde el anillo 3" "^(C:.HISPA.SALUDO.BIN ha acabado con el código 3)$"
if sano $Q/s1.img && sin_restos $Q/s1.img && igual_en $Q/s1.img HISPA/SALUDO.BIN $F/HOLA.BIN && igual_en $Q/s1.img HISPA/SALUDO.DAT $F/GRANDE.DAT \
  && igual_en $Q/s1.img DOCS/LETRAS/A.TXT $F/A.TXT && igual_en $Q/s1.img DOCS/LETRAS/B.TXT $F/B11.TXT && igual_en $Q/s1.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO1.PQI; then
  ok "lo instalado, desde fuera: fsck.fat da el disco por sano, los archivos son los del paquete octeto a octeto (también en carpetas nuevas: C:\\DOCS\\LETRAS), el registro es la cabecera del paquete y no quedan restos (ni HPQ*.NUE, ni .VIE, ni el diario)"
else ko "lo instalado no queda bien en el disco:"; lista_de $Q/s1.img | sed 's/^/        /' | head -14; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 2. por su nombre, del catálogo (la versión más nueva, con lo que necesita)
cp $Q/sis.img $Q/s5.img
paq $Q/s5.img nombre "paquetes instala saludo" ":pausa 2" "paquetes instala saludo"
mira "PAQUETES INSTALA saludo: del catálogo, la versión más nueva (2.0), con lo que necesita delante (letras 1.1); y otra vez: ya está" \
  "^Se va a instalar: letras 1\.1, saludo 2\.0$" "^  instalado saludo 2\.0$" "^paquetes: saludo ya está instalado (la 2\.0); para una más nueva: PAQUETES ACTUALIZA$"
if sano $Q/s5.img && sin_restos $Q/s5.img && igual_en $Q/s5.img HISPA/SALUDO.TXT $F/S2.TXT && igual_en $Q/s5.img DOCS/LETRAS/B.TXT $F/B11.TXT; then
  ok "y en el disco: sano, con los dos paquetes octeto a octeto y sin restos"
else ko "instalar por su nombre no deja bien el disco:"; lista_de $Q/s5.img | sed 's/^/        /' | head -14; fi

# 3. actualizar: saludo 1.0 -> 2.0 (cambia un archivo, trae otro, deja de traer el grande)
cp $Q/s1.img $Q/s2.img
paq $Q/s2.img actualiza "paquetes catalogo" "paquetes actualiza" ":pausa 2" "paquetes actualiza" "paquetes comprueba"
mira "PAQUETES ACTUALIZA: el catálogo dice lo que es más nuevo; se pone saludo 2.0 en lugar de la 1.0, y luego todo está al día; COMPRUEBA: los archivos instalados siguen iguales" \
  "^  saludo    2\.0     (nuevo; tiene la 1\.0)    Un programa que saluda (la 2)$" "^  letras    1\.1     (instalado)              Unas letras, mejores$" \
  "^Se va a instalar: saludo 2\.0 (en lugar de la 1\.0)$" "^  instalado saludo 2\.0$" "^Todo está al día\.$" "^Los archivos de los 3 paquetes instalados están como se instalaron\.$"
if sano $Q/s2.img && sin_restos $Q/s2.img && igual_en $Q/s2.img HISPA/SALUDO.TXT $F/S2.TXT && ! igual_en $Q/s2.img HISPA/SALUDO.DAT $F/GRANDE.DAT \
  && igual_en $Q/s2.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO2.PQI && igual_en $Q/s2.img HISPA/SALUDO.BIN $F/HOLA.BIN; then
  ok "lo actualizado, desde fuera: el disco sano, lo de la 2.0 en su sitio, el archivo que la 2.0 ya no trae (SALUDO.DAT) quitado y el registro, el de la 2.0"
else ko "lo actualizado no queda bien en el disco:"; lista_de $Q/s2.img | sed 's/^/        /' | head -14; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 3 bis. (SISTEMA, 0.8.6) los paquetes nuevos del sistema: el catálogo marca
# «sistema=nuevo» (y «sistema=letras», que ya está); ACTUALIZA pone «nuevo»
# aunque no estuviera (como accesori, sonido, wifi o escudo en un sistema que
# viene de una versión de antes), pero no «extra», que no es del sistema; lo
# quitado a propósito no lo vuelve a poner (QUITADOS.TXT) e INSTALA lo olvida
mkdir -p $F/paqsis
cp $F/paq/LETRAS1.HPQ $F/paq/LETRAS11.HPQ $F/paq/SALUDO1.HPQ $F/paq/SALUDO2.HPQ $F/paqsis/
( cd $F
  $HPQ crea paqsis/NUEVO.HPQ --nombre nuevo --version 1.0 --descripcion 'Nuevo del sistema (de prueba)' A.TXT='\HISPA\NUEVO.TXT'
  $HPQ crea paqsis/EXTRA.HPQ --nombre extra --version 1.0 --descripcion 'Opcional (de prueba)' A.TXT='\HISPA\EXTRA.TXT'
  $HPQ catalogo paqsis && printf 'sistema=letras\r\nsistema=nuevo\r\n' >> paqsis/CATALOGO.TXT && $HPQ firma paqsis $Q/prueba.key ) > $Q/hpqsis.txt 2>&1
mkfs.fat -F 16 -n MEMORIA -C $Q/usbsis.img 16384 > /dev/null && mmd -i $Q/usbsis.img ::PAQUETES && mcopy -i $Q/usbsis.img $F/paqsis/* ::PAQUETES/
cp $Q/s1.img $Q/ssis.img
# (y lo que dejaba en BAJADAS un PAQUETES de antes de la 0.8.6: se borra)
mmd -i $Q/ssis.img ::HISPA/PAQUETES/BAJADAS 2> /dev/null; mcopy -i $Q/ssis.img $F/paq/LETRAS1.HPQ ::HISPA/PAQUETES/BAJADAS/VIEJO.HPQ
USB_IMG=$Q/usbsis.img paq $Q/ssis.img sistema "paquetes estado" "paquetes actualiza" ":pausa 2" "paquetes lista" "paquetes quita nuevo" "paquetes estado" "paquetes actualiza" ":pausa 1"
mira "PAQUETES ESTADO dice cuál es nuevo del sistema («|nuevo»: la bandeja lo cuenta); ACTUALIZA pone también el paquete nuevo del sistema (no estaba), no el opcional; y quitado a propósito, ACTUALIZA ya no lo pone" \
  "^p|nuevo||1\.0|Nuevo del sistema (de prueba)|otros|nuevo$" "^p|extra||1\.0|Opcional (de prueba)|otros$" "^Nuevo del sistema: nuevo$" \
  "^Se va a instalar: saludo 2\.0 (en lugar de la 1\.0), nuevo 1\.0$" "^  instalado nuevo 1\.0$" "^  nuevo     1\.0     Nuevo del sistema (de prueba)$" \
  "^  quitado nuevo 1\.0$" "^p|nuevo||1\.0|Nuevo del sistema (de prueba)|otros$" "^Todo está al día\.$"
if grep -q "instalado extra\|^p|letras||" $S; then ko "ACTUALIZA ha puesto el opcional (extra) o da letras por no instalado"; else ok "el opcional (extra) se queda sin instalar"; fi
if sano $Q/ssis.img && sin_restos $Q/ssis.img && saca $Q/ssis.img HISPA/PAQUETES/QUITADOS.TXT $Q/quitados.txt && [ "$(cat $Q/quitados.txt)" = "$(printf 'nuevo\r')" ] \
  && ! igual_en $Q/ssis.img HISPA/NUEVO.TXT $F/A.TXT && ! igual_en $Q/ssis.img HISPA/EXTRA.TXT $F/A.TXT && ! saca $Q/ssis.img HISPA/PAQUETES/BAJADAS/VIEJO.HPQ $Q/viejo.hpq; then
  ok "en el disco: sano, sin «nuevo» (quitado) ni «extra», QUITADOS.TXT dice «nuevo» y lo viejo de BAJADAS (de un PAQUETES de antes), borrado"
else ko "lo de los nuevos del sistema no queda bien en el disco:"; lista_de $Q/ssis.img | sed 's/^/        /' | head -14; fi
USB_IMG=$Q/usbsis.img paq $Q/ssis.img sistema2 "paquetes instala nuevo" ":pausa 1" "paquetes actualiza" ":pausa 1"
mira "y PAQUETES INSTALA nuevo lo pone otra vez (y lo olvida de QUITADOS.TXT)" "^  instalado nuevo 1\.0$" "^Todo está al día\.$"
if sano $Q/ssis.img && igual_en $Q/ssis.img HISPA/NUEVO.TXT $F/A.TXT && ! saca $Q/ssis.img HISPA/PAQUETES/QUITADOS.TXT $Q/quitados.txt; then
  ok "en el disco: «nuevo» puesto y QUITADOS.TXT, borrado (ya no queda nada quitado)"
else ko "INSTALA no olvida lo quitado:"; cat $Q/quitados.txt 2> /dev/null | sed 's/^/        /'; fi

# 4. lo que no vale no toca nada: una suma mala, un conflicto, quitar lo que
# otro necesita; y COMPRUEBA ve un archivo cambiado (la imagen, idéntica)
cp $Q/s1.img $Q/s4.img
printf 'cambiado a mano\r\n' > $Q/cambiado.txt; mcopy -o -i $Q/s4.img $Q/cambiado.txt ::DOCS/LETRAS/A.TXT
cp $Q/s4.img $Q/s40.img
paq $Q/s4.img malo 'paquetes instala x:\paquetes\malo.hpq' "paquetes instala choque" "paquetes quita letras" 'paquetes mira x:\paquetes\malo.hpq' "paquetes comprueba" "paquetes quita nadie" "paquetes instala nadie"
mira "una suma mala (MALO.HPQ, un octeto cambiado): se ve antes de copiar nada, y no se instala nada; MIRA dice lo mismo" \
  "^paquetes: X:.paquetes.malo\.hpq: la suma de .MALO2\.DAT no es la suya: el paquete está estropeado$" "^No se instala nada: el disco queda como estaba\.$" \
  "^  .MALO2\.DAT  *70000 octetos$"
mira "lo que choca (choque no puede estar con saludo), quitar lo que otro necesita (letras, que necesita saludo) y lo que no hay: se dice, y no se hace" \
  "^paquetes: choque no puede estar con saludo, que está instalado: quítelo antes (PAQUETES QUITA saludo)$" "^paquetes: letras no se quita: lo necesita saludo (quite antes saludo, o /FORZAR)$" \
  "^paquetes: nadie no está instalado$" "^paquetes: hace falta nadie y no está en el catálogo$"
if grep -q "^  letras: C:.DOCS.LETRAS.A\.TXT ha cambiado$" $S && grep -q "^1 archivos no están como se instalaron" $S && cmp -s $Q/s4.img $Q/s40.img; then
  ok "COMPRUEBA ve el archivo cambiado a mano (su SHA-256 no es el apuntado); y con todo lo que no valía, el disco queda idéntico octeto a octeto"
else ko "COMPRUEBA no ve el archivo cambiado, o lo que no valía ha tocado el disco:"; grep "cambiado\|como se instalaron" $S | sed 's/^/        /'; fi

# 5. quitar (saludo, y luego letras, que ya nadie necesita): el disco, como al principio
cp $Q/s2.img $Q/s3.img
paq $Q/s3.img quita "paquetes quita saludo letras" "paquetes lista" "dir \\docs"
mira "PAQUETES QUITA saludo letras: quita sus archivos, sus registros y las carpetas que se quedan vacías (C:\\DOCS\\LETRAS y C:\\DOCS)" \
  "^  quitado saludo 2\.0$" "^  quitado letras 1\.1$" "^Hecho\.$" "^1 paquetes\.$" "^No puedo abrir .docs: no existe\.$"
sano $Q/sis.img; antes=$(grep -o '[0-9]*/[0-9]* clusters' $P/fsck.txt)
# ((SISTEMA, 0.8.6) lo quitado a propósito queda apuntado: QUITADOS.TXT; fuera, para comparar)
if saca $Q/s3.img HISPA/PAQUETES/QUITADOS.TXT $Q/quitados.txt && [ "$(tr -d '\r' < $Q/quitados.txt | tr '\n' ' ')" = "saludo letras " ]; then
  ok "lo quitado a propósito queda en C:\\HISPA\\PAQUETES\\QUITADOS.TXT (saludo y letras: ACTUALIZA no los pondrá si fueran del sistema)"
else ko "QUITADOS.TXT no dice lo quitado: $(cat $Q/quitados.txt 2> /dev/null)"; fi
mdel -i $Q/s3.img ::HISPA/PAQUETES/QUITADOS.TXT 2> /dev/null
# ((USUARIOS) menos la semilla del azar, AZAR.SEM, que se hace en el primer
# arranque, y la serie del último catálogo firmado, SERIE.TXT)
mdel -i $Q/s3.img ::HISPA/AZAR.SEM ::HISPA/PAQUETES/SERIE.TXT 2> /dev/null
if sano $Q/s3.img && [ "$(grep -o '[0-9]*/[0-9]* clusters' $P/fsck.txt)" = "$antes" ] && [ "$(lista_de $Q/s3.img)" = "$(lista_de $Q/sis.img)" ]; then
  ok "lo quitado, desde fuera: el disco sano, con los mismos archivos y carpetas que antes de instalar nada y los mismos clusters ocupados ($antes)"
else ko "después de quitarlo todo el disco no queda como al principio:"; diff <(lista_de $Q/s3.img) <(lista_de $Q/sis.img) | sed 's/^/        /' | head -8; fi

# 6. un corte a mitad (como si se fuera la luz): actualizar saludo y pararse
# tras el cuarto cambio (/CORTE=4: lo de antes apartado, lo nuevo a medio
# poner); QEMU se apaga ahí. Al volver, PAQUETES lo deshace
cp $Q/s1.img $Q/s6.img
paq $Q/s6.img corte "paquetes actualiza /corte=4" ":pausa 2"
cp $Q/s6.img $Q/s6corte.img
if grep -q "^paquetes: corte de prueba" $S && lista_de $Q/s6.img | grep -q "DIARIO\.TXT" && lista_de $Q/s6.img | grep -q "HPQ00000\.VIE" && ! lista_de $Q/s6.img | grep -q "SALUDO\.PQI"; then
  ok "un corte a mitad de actualizar (QEMU apagado tras el cuarto cambio): quedan el diario (C:\\HISPA\\PAQUETES\\DIARIO.TXT), lo de antes apartado (HPQ*.VIE) y lo nuevo a medias"
else ko "el corte a mitad no deja lo que se esperaba:"; lista_de $Q/s6.img | sed 's/^/        /' | head -14; fi
paq $Q/s6.img vuelta "paquetes lista"
mira "al volver, PAQUETES ve el diario y lo deshace: saludo sigue en la 1.0" \
  "^paquetes: saludo 2\.0 se quedó a medias (¿se fue la luz?): se ha deshecho y todo está como antes$" "^  saludo    1\.0     Un programa que saluda$"
if sano $Q/s6.img && [ "$(lista_de $Q/s6.img)" = "$(lista_de $Q/s1.img)" ] && igual_en $Q/s6.img HISPA/SALUDO.DAT $F/GRANDE.DAT && igual_en $Q/s6.img HISPA/SALUDO.BIN $F/HOLA.BIN \
  && igual_en $Q/s6.img HISPA/PAQUETES/SALUDO.PQI $F/SALUDO1.PQI && ! lista_de $Q/s6.img | grep -q "SALUDO\.TXT"; then
  ok "lo deshecho, desde fuera: fsck.fat da el disco por sano y está como antes del corte (los mismos archivos, iguales octeto a octeto; sin restos)"
else ko "después del corte el disco no vuelve a como estaba:"; diff <(lista_de $Q/s6.img) <(lista_de $Q/s1.img) | sed 's/^/        /' | head -8; tail -n 3 $P/fsck.txt | sed 's/^/        /'; fi

# 7. el gancho de la red: con /DESDE=http://..., PAQUETES lanza DESCARGA
# (aquí, uno de mentira) para el catálogo y cada paquete; sin DESCARGA, lo dice
cp $Q/sis.img $Q/s7.img
# (PROTEGE: borrar DESCARGA.BIN de C:\HISPA es tocar el sistema: en modo administrador, que se confirma con la S)
paq $Q/s7.img red "paquetes instala letras /desde=http://prueba/" ":pausa 2" "admin" ":pausa 1" "s" "del \\hispa\\descarga.bin" "admin no" "paquetes catalogo /desde=http://actualiza.hispados.es/pentium/"
mira "el gancho de la red: con /DESDE=http://..., el catálogo y los paquetes los trae DESCARGA (DESCARGA.BIN, la orden de la red) a C:\\HISPA\\PAQUETES\\BAJADAS, y se instalan igual" \
  "^Trayendo http://prueba/CATALOGO\.TXT\.\.\.$" "^descarga (de prueba): http://prueba/LETRAS11\.HPQ -> C:.HISPA.PAQUETES.BAJADAS.LETRAS11\.HPQ$" "^  instalado letras 1\.1$" \
  "^paquetes: para traer http://actualiza\.hispados\.es/pentium/CATALOGO\.TXT de Internet hace falta la red (el programa DESCARGA),$"
if sano $Q/s7.img && sin_restos $Q/s7.img && igual_en $Q/s7.img DOCS/LETRAS/B.TXT $F/B11.TXT && ! lista_de $Q/s7.img | grep -q "BAJADAS/LETRAS11\.HPQ"; then
  ok "y en el disco: sano, con lo instalado igual que lo de la memoria octeto a octeto; lo traído a BAJADAS, ya borrado (RÁPIDO: en una memoria pequeña son megas)"
else ko "lo traído por el gancho de la red no queda bien en el disco:"; lista_de $Q/s7.img | sed 's/^/        /' | head -14; fi

# 7 bis. (GRUPOS, 25/11/2026) los grupos del catálogo: «grupo=código
# p1,p2,... Nombre|Descripción», firmados con lo demás. textos (letras y
# saludo), mixto (letras y «nadie», que no está en el catálogo) y conbase
# (base, imprescindible, y letras): PAQUETES GRUPOS; INSTALA GRUPO= (entero,
# otra vez: ya estaba; con un fallo, con su motivo; lo que falta); QUITA
# GRUPO= (lo de otro grupo entero, lo imprescindible y lo que otro necesita
# se queda); un PAQUETES sin grupos (como los de antes de la 1.9) no se
# entera de las líneas «grupo=» y sigue instalando; y un usuario normal, no
( cd $F && rm -rf paqg && mkdir paqg && cp paq/*.HPQ paqg/ && rm -f paqg/MALO.HPQ && $HPQ catalogo paqg \
  && printf 'grupo=textos letras,saludo Los textos|Unas letras y un programa que saluda\r\ngrupo=mixto letras,nadie Mixto|Con uno que no está en el catálogo\r\ngrupo=conbase base,letras Con la base|La base (imprescindible) y las letras\r\n' >> paqg/CATALOGO.TXT \
  && $HPQ firma paqg $Q/prueba.key ) >> $Q/hpq.txt 2>&1
mkfs.fat -F 16 -n MEMORIA -C $Q/usbg.img 16384 > /dev/null && mmd -i $Q/usbg.img ::PAQUETES && mcopy -i $Q/usbg.img $F/paqg/* ::PAQUETES/
if $HPQ comprueba $F/paqg $Q/clave.cas > /dev/null && grep -q $'^grupo=textos letras,saludo Los textos|Unas letras y un programa que saluda\r$' $F/paqg/CATALOGO.TXT \
  && [ "$($HPQ grupos | grep -c '^grupo=')" = 6 ] && $HPQ grupos | grep -q '^grupo=oficina pluma,cuentas,oficina,pdf,agenda,bloc,pintura,accesori Toda la Oficina|' \
  && [ -z "$($HPQ grupos $F/paq)" ]; then
  ok "tools/hpq.py: los grupos (GRUPOS: oficina, internet, juegos, multimedia, herramientas y seguridad), en el catálogo solo con los paquetes que hay; el catálogo de prueba, con los suyos y firmado (la firma, buena)"
else ko "tools/hpq.py no hace bien los grupos:"; $HPQ grupos | head -3 | sed 's/^/        /'; fi
cp $Q/sis.img $Q/sg.img
USB_IMG=$Q/usbg.img paq $Q/sg.img grupos "paquetes grupos" "paquetes instala grupo=textos" ":pausa 3" "paquetes instala grupo=textos" ":pausa 1" "paquetes instala grupo=mixto" ":pausa 1" \
  "paquetes instala grupo=nada" "paquetes quita grupo=textos" ":pausa 2" "paquetes grupos" "paquetes instala grupo=textos" ":pausa 3" "paquetes quita grupo=conbase" ":pausa 1" "paquetes lista"
tr -d '\r' < $Q/grupos.sal > $Q/grupos.txt
S=$Q/grupos.txt
mira "PAQUETES GRUPOS: cada grupo, con lo que lleva y cómo está (nadie: no está en el catálogo)" \
  "^Grupos de paquetes de X:.PAQUETES.:$" "^  textos        Los textos: 0 de 2 instalados (faltan [0-9]* K)$" "^      letras, saludo$" \
  "^      letras, nadie (no está en el catálogo)$" "^  conbase       Con la base: 1 de 2 instalados"
mira "INSTALA GRUPO=textos: todo, en orden (letras antes, que saludo lo necesita), con las sumas; y el resumen" \
  "^Grupo «Los textos» (textos): letras saludo$" "^Se va a instalar: letras 1\.1, saludo 2\.0$" "^  instalado saludo 2\.0$" "^Grupo textos: 2 instalados, 0 ya estaban, 0 fallos\.$"
mira "otra vez: ya estaba todo; mixto: lo que no se puede, con su motivo (y lo demás, sí); un grupo que no está, se dice" \
  "^Grupo textos: 0 instalados, 2 ya estaban, 0 fallos\.$" "^  fallo nadie: no está en el catálogo$" "^Grupo mixto: 0 instalados, 1 ya estaban, 1 fallos\.$" \
  "^paquetes: el catálogo no trae el grupo «nada» (PAQUETES GRUPOS los dice)$"
mira "QUITA GRUPO=textos: letras se queda (es de mixto, que está entero); saludo, se quita" \
  "^Quitar el grupo «Los textos»:$" "^  se queda letras: es también de «Mixto», que está entero$" "^Se va a quitar: saludo 2\.0$" "^  quitado saludo 2\.0$" \
  "^  textos        Los textos: 1 de 2 instalados"
mira "lo que falta: INSTALA GRUPO=textos pone solo saludo; QUITA GRUPO=conbase: base es imprescindible y letras lo necesita saludo: nada que quitar" \
  "^Se va a instalar: saludo 2\.0$" "^Grupo textos: 1 instalados, 1 ya estaban, 0 fallos\.$" "^  se queda base: es imprescindible" \
  "^No hay nada del grupo que se pueda quitar\.$" "^3 paquetes\.$"
if sano $Q/sg.img && sin_restos $Q/sg.img && igual_en $Q/sg.img HISPA/SALUDO.TXT $F/S2.TXT && igual_en $Q/sg.img DOCS/LETRAS/B.TXT $F/B11.TXT; then
  ok "y en el disco: sano, sin restos, con letras y saludo (los del grupo) octeto a octeto"
else ko "instalar y quitar grupos no deja bien el disco:"; lista_de $Q/sg.img | sed 's/^/        /' | head -12; fi
# un PAQUETES sin grupos (el de la 1.8, si está el repositorio; si no, este
# mismo sin leer «grupo=»): instala del catálogo con grupos sin enterarse
mkdir -p $Q/viejo && cp $Q/clave.cas $Q/viejo/
if git -C $AQUI log -1 > /dev/null 2>&1 && c=$(git -C $AQUI log --format=%H -S'VERSIÓN_PAQUETES = "1.9"' -- nucleo32/upaquete.cas | tail -1) && [ -n "$c" ] \
  && git -C $AQUI show "$c^:castel/nucleo32/upaquete.cas" > $Q/viejo/upaquete.cas 2> /dev/null && grep -q 'VERSIÓN_PAQUETES = "1.8"' $Q/viejo/upaquete.cas; then
  viejo="el de la 1.8"
else
  sed 's/^                lee_grupo(l + 6)$/                gr_n := 0/' $AQUI/nucleo32/upaquete.cas > $Q/viejo/upaquete.cas; viejo="uno sin grupos"
fi
cp $Q/sis.img $Q/sv.img
if python3 -I $AQUI/ccastel.py $Q/viejo/upaquete.cas -o $Q/viejo/u.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/viejo/u.asm -o $Q/viejo/PAQUETES.BIN \
  && mcopy -o -i $Q/sv.img $Q/viejo/PAQUETES.BIN ::HISPA/PAQUETES.BIN; then
  USB_IMG=$Q/usbg.img paq $Q/sv.img viejo "paquetes catalogo" "paquetes instala saludo" ":pausa 3"
  tr -d '\r' < $Q/viejo.sal > $Q/viejo.txt
  S=$Q/viejo.txt
  mira "un PAQUETES sin grupos ($viejo) con el catálogo con grupos: la firma, buena; las líneas «grupo=» no las mira; instala igual" \
    "^La firma del catálogo es buena" "^  saludo    2\.0  *Un programa que saluda (la 2)$" "^  instalado saludo 2\.0$" "^Hecho\.$"
  ! grep -q "Grupo \|grupo «\|textos" $S && ok "(y no dice nada de los grupos)" || ko "el PAQUETES sin grupos habla de ellos: $(grep -m1 'Grupo \|grupo «\|textos' $S)"
else ko "no se ha podido hacer el PAQUETES sin grupos ($viejo)"; fi
# un usuario normal (Biblio2, que entra sola; el administrador es Carlos): no instala grupos
cp $Q/sis.img $Q/sn.img
printf 'HISPA-USUARIOS 1\r\nu=1|N|0|1|BIBLIO2|Biblio2|\r\nu=2|A|0|0|CARLOS|Carlos|\r\n' > $Q/UN.TXT
mcopy -i $Q/sn.img $Q/UN.TXT ::HISPA/USUARIOS.TXT
USB_IMG=$Q/usbg.img paq $Q/sn.img normal ":pausa 2" "paquetes grupos" "paquetes instala grupo=textos" "paquetes quita grupo=conbase" "paquetes lista"
tr -d '\r' < $Q/normal.sal > $Q/normal.txt
S=$Q/normal.txt
mira "un usuario normal ve los grupos, pero no los instala ni los quita (solo un administrador)" \
  "^  textos        Los textos: 0 de 2 instalados" "^paquetes: solo un administrador puede instalar, reparar o quitar programas" "^1 paquetes\.$"
[ "$(grep -c '^paquetes: solo un administrador puede instalar' $S)" = 2 ] && ok "(las dos veces: INSTALA GRUPO= y QUITA GRUPO=)" || ko "el usuario normal: $(grep -c '^paquetes: solo un administrador' $S) rechazos (tenían que ser 2)"

# 8. la ventana «Programas y actualizaciones» (ACTUALIZ.BIN), desde el menú
# Inicio («Actualizar»): la lista, instalar (con lo que necesita) y quitar con
# sus botones; por debajo, PAQUETES
cp $Q/sis.img $Q/s8.img
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $Q/HISPA.INI
mcopy -i $Q/s8.img $F/ACTUALIZ.BIN $Q/HISPA.INI ::HISPA/
CAPTURAS=$G ESCRIBE=1 SATA=$Q/s8.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":clic 40 585" ":captura pq0" ":clic 300 338" ":pausa 4" ":captura pq1" ":clic 200 236" ":clic 199 471" ":pausa 5" ":captura pq2" \
  ":clic 200 216" ":clic 310 471" ":pausa 4" ":captura pq3" ":clic 600 471" ":pausa 4" ":captura pq4" ":clic 661 71" ":pausa 3" ":captura pq5" > $Q/ventana.sal 2>&1
miracap "el menú Inicio trae «Actualizar» (en la columna de la derecha): abre la ventana «Programas y actualizaciones»" pq0 negrita 256 330 264 338 Actualizar
miracap "la ventana: lo instalado (base 0.7) y lo del catálogo de la memoria USB, con su firma buena (choque, letras, saludo: «(no)» instalados), cada uno con su versión y lo que es; y el botón «Buscar novedades»" pq1 \
  negrita 150 90 154 94 "Programas y actualizaciones" texto 150 109 154 113 'Los paquetes, de X:\PAQUETES\' negrita 112 163 116 167 base \
  texto 208 183 212 187 "(no)" negrita 112 223 116 227 saludo texto 304 223 308 227 "2.0" negrita 132 458 136 462 Actualizarlo \
  negrita 536 456 540 462 "Buscar novedades" negrita 576 107 584 113 "(firma buena)"
miracap "con el botón «Instalarlo», saludo (y antes letras, que lo necesita): lo que dice PAQUETES sale abajo" pq2 \
  texto 112 397 116 401 "  instalado saludo 2.0" texto 112 415 116 419 "Hecho." negrita 112 183 116 187 letras texto 208 183 212 187 "1.1"
miracap "y «Quitarlo», con saludo elegido (sigue elegido aunque la lista cambie de orden): se quita" pq3 \
  texto 112 397 116 401 "  quitado saludo 2.0" negrita 112 203 116 207 choque
miracap "«Buscar novedades» trae el catálogo otra vez (y su firma, buena)" pq4 \
  texto 112 415 116 419 "La firma del catálogo es buena" negrita 112 203 116 207 choque
miracap "maximizada, la ventana se pone a su medida: la lista, los mensajes y los botones, al ancho y al alto de la pantalla, y lo que no cabe, cortado" pq5 \
  negrita 52 32 60 38 "Programas y actualizaciones" negrita 660 50 700 58 "(firma buena)" negrita 182 512 190 522 Quitarlo
if sano $Q/s8.img && sin_restos $Q/s8.img && [ "$(mdir -/ -b -i $Q/s8.img ::HISPA/PAQUETES 2> /dev/null | grep -v '/SERIE.TXT$' | sort | tr '\n' ' ')" = "::/HISPA/PAQUETES/BASE.PQI ::/HISPA/PAQUETES/LETRAS.PQI ::/HISPA/PAQUETES/QUITADOS.TXT " ]; then    # ((USUARIOS) y SERIE.TXT)
  ok "lo hecho desde la ventana, desde fuera: el disco sano, letras instalado, saludo quitado y sin restos (tampoco VENTANA.TXT)"
else ko "lo hecho desde la ventana no queda bien en el disco:"; lista_de $Q/s8.img | sed 's/^/        /' | head -12; fi

# 9. los accesos de los paquetes (accesos.cas; clicgest-pentium, 20/11): un
# .MNU en \HISPA\MENU con su icono .ICP (tools/hpicono.py, de un PNG) sale en
# el menú Inicio, debajo de Mi equipo, y en el tapiz; elegirlo lanza el programa
A9=$Q/acc
mkdir -p $A9
python3 - $A9/rojo.png <<'PY'
import struct, sys, zlib
filas = b''.join(b'\0' + b''.join(bytes((200, 40, 40, 255)) if 4 <= x < 28 and 4 <= y < 28 else bytes(4) for x in range(32)) for y in range(32))
def tr(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + tr(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) + tr(b'IDAT', zlib.compress(filas)) + tr(b'IEND', b''))
PY
if python3 -I $AQUI/../tools/hpicono.py $A9/rojo.png $A9/SALUDO.ICP && [ "$(stat -c %s $A9/SALUDO.ICP)" = 4108 ]; then
  ok "tools/hpicono.py: de un PNG de 32 x 32 (RGBA), el .ICP (HPIC, 32, 32 y los puntos)"
else ko "tools/hpicono.py no hace el .ICP"; fi
$AQUI/nucleo32/disco.sh $A9/d.img 16 > /dev/null 2>&1
printf 'nombre=Saludo\r\nprograma=\\HOLA.BIN\r\nicono=\\HISPA\\MENU\\SALUDO.ICP\r\nescritorio=si\r\n' > $A9/SALUDO.MNU
mmd -i $A9/d.img ::HISPA ::HISPA/MENU 2> /dev/null
mcopy -i $A9/d.img $A9/SALUDO.MNU $A9/SALUDO.ICP ::HISPA/MENU/
CAPTURAS=$G DISCO=$A9/d.img MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":captura ac0" ":clic 40 585" ":pausa 1" ":captura ac1" ":clic 80 258" ":pausa 2" > $A9/sal.txt 2>&1
miracap "un acceso de un paquete (\\HISPA\\MENU\\SALUDO.MNU): su icono (el .ICP, rojo) y su nombre en el tapiz, debajo de los de siempre" ac0 texto 30 270 60 295 Saludo rgb 64 259 C82828
miracap "y en el menú Inicio, el primero de los más usados (MENÚ: mientras no hay cuentas, los de los paquetes)" ac1 texto 38 246 46 254 Saludo negrita 46 155 54 163 "Bloc de notas"
S=$A9/sal.txt
mira "elegirlo en el menú lanza su programa (programa=\\HOLA.BIN)" "escritorio: lanzo .HOLA.BIN"

# 10. los controles de las ventanas (controles.cas; controles-erp, 21/11):
# MUESTRA.BIN, maximizada y con la letra a vez y media: la barra de menús
# (Alt+V), la de herramientas, las pestañas (Ctrl+Tab), una rejilla que se
# ordena (un clic en un rótulo) y se filtra (sin tildes), recuadros, un
# campo de fecha, una desplegable y la barra de estado
C10=$Q/ctl
mkdir -p $C10
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/umuestra.cas -o $C10/m.asm > /dev/null 2>&1 && nasm -f bin $C10/m.asm -o $C10/MUESTRA.BIN
$AQUI/nucleo32/disco.sh $C10/d.img 16 > /dev/null 2>&1
mcopy -i $C10/d.img $C10/MUESTRA.BIN ::
CAPTURAS=$G DISCO=$C10/d.img MEMORIA=64 MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":doble 40 190" ":pausa 2" ":escribe muestra" ":tecla ret" ":pausa 4" \
  ":raton 620 590" ":captura ct1" ":clic 60 217" ":pausa 1" ":raton 620 590" ":captura ct2" ":clic 30 178" ":escribe angel" ":raton 620 590" ":pausa 1" ":captura ct3" \
  ":tecla ctrl-tab" ":pausa 1" ":captura ct4" ":tecla alt-v" ":pausa 1" ":captura ct5" ":tecla esc" ":pausa 1" ":clic 60 112" ":pausa 1" ":raton 620 590" ":captura ct6" \
  ":clic 300 178" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":tecla backspace" ":pausa 1" \
  ":raton 620 590" ":captura cr0" ":arrastra 653 217 713 217" ":pausa 1" ":raton 620 590" ":captura cr1" ":doble 713 217" ":pausa 1" ":raton 620 590" ":captura cr2" \
  ":arrastra 72 217 120 217" ":pausa 1" ":raton 620 590" ":captura cr3" > $C10/sal.txt 2>&1
miracap "MUESTRA.BIN: maximizada (800 x 600), la barra de menús, la de herramientas, las pestañas, la rejilla con sus rótulos y la barra de estado, a vez y media" ct1 \
  tam 800 600 texto15 10 30 140 60 rchivo texto 20 60 120 90 Nuevo texto 90 60 300 90 Guardar negrita15 10 95 120 130 Lista texto15 90 95 300 130 Ficha \
  negrita15 60 195 200 235 Nombre negrita15 400 195 660 235 Importe texto15 0 525 300 560 "Ana (titular)"
miracap "un clic en el rótulo «Nº» ordena la rejilla por él (de menos a más: la 1 arriba)" ct2 texto15 60 230 420 270 "Persona de ejemplo 1"
miracap "filtrar sin mirar tildes ni mayúsculas: «angel» encuentra «Àngel álvarez»" ct3 negrita15 90 230 420 270 "ngel álvarez" texto15 200 525 800 560 "1 renglones"
miracap "Ctrl+Tab: la pestaña Ficha, con su recuadro «Datos generales», los campos, la fecha, la desplegable y la casilla" ct4 \
  negrita15 10 125 300 160 "Datos generales" texto15 20 180 300 215 "Ana Ejemplo" texto15 20 245 300 280 "21/11/2026" texto15 200 245 600 280 "Socio" texto15 40 280 400 320 "Al corriente de pago"
miracap "Alt+V abre el menú «Ver» de la barra (un menú del escritorio)" ct5 texto 100 40 330 160 "Letra muy grande"
miracap "y un clic en la pestaña «Lista» vuelve a ella (pestaña_en: la fila de pestañas que hay en ese punto)" ct6 negrita15 60 195 200 235 Nombre
# los anchos de las columnas (pentium-letras): cada una, lo que mide lo más
# largo (Nombre, la del texto largo, lo que sobra); se arrastra el borde de
# un rótulo (va con el ratón: Importe gana lo que pierde Estado) y dos clics
# en él la ajustan a lo más largo que se ve
miracap "la rejilla reparte el ancho por lo que mide cada columna (Fecha e Importe, lo justo; Nombre, lo que sobra)" cr0 \
  negrita15 560 200 580 235 Importe negrita15 70 200 90 235 Nombre negrita15 425 200 445 235 Fecha
miracap "arrastrar el borde entre Importe y Estado: Importe más ancha (60 puntos), Fecha donde estaba" cr1 negrita15 620 200 640 235 Importe negrita15 425 200 445 235 Fecha distinto 0 200 800 100 $G/cr0.ppm
miracap "dos clics en ese borde: Importe, a lo que mide lo más largo que se ve (lo que sobra, a Nombre)" cr2 negrita15 620 200 640 235 Importe negrita15 485 200 505 235 Fecha
miracap "y el borde entre Nº y Nombre: Nº más ancha" cr3 negrita15 117 200 137 235 Nombre

# 9. la firma (Ed25519, firma.cas): el catálogo de prueba va firmado con la
# clave de prueba (la que lleva este PAQUETES); PAQUETES FIRMA la comprueba;
# lo de Internet sin firma, o con el catálogo cambiado, no se usa
if $HPQ comprueba $F/paq $Q/clave.cas > $Q/firma.txt 2>&1 && ! $HPQ comprueba $F/paq $AQUI/nucleo32/clave.cas >> $Q/firma.txt 2>&1 \
  && grep -q '^ed25519 [0-9a-f]\{128\}' $F/paq/CATALOGO.FIR; then
  ok "tools/hpq.py: hace la clave (la privada, con permisos 600), firma el catálogo (CATALOGO.FIR, Ed25519) y comprueba la firma (buena con su clave pública; mala con la de Hispa-Pentium)"
else ko "tools/hpq.py no firma bien el catálogo:"; sed 's/^/        /' $Q/firma.txt; fi
# una memoria con el catálogo cambiado (una letra de una descripción) y la
# misma firma; y otra sin CATALOGO.FIR
mkdir -p $Q/u2 $Q/u3
cp $F/paq/* $Q/u2/; sed -i 's/Unas letras, mejores/Unas letras, peores/' $Q/u2/CATALOGO.TXT
cp $F/paq/* $Q/u3/; rm $Q/u3/CATALOGO.FIR
for u in 2 3; do mkfs.fat -F 16 -n MEMORIA -C $Q/usb$u.img 16384 > /dev/null && mmd -i $Q/usb$u.img ::PAQUETES && mcopy -i $Q/usb$u.img $Q/u$u/* ::PAQUETES/; done
cp $Q/sis.img $Q/s9.img
paq $Q/s9.img firma 'paquetes firma x:\paquetes\catalogo.txt' ":pausa 3" 'paquetes firma x:\paquetes\letras1.hpq' "paquetes catalogo /desde=http://prueba/" ":pausa 3" "paquetes catalogo /desde=http://prueba/nada/" ":pausa 2"
USB_IMG=$Q/usb2.img paq $Q/s9.img firma2 'paquetes firma x:\paquetes\catalogo.txt' ":pausa 3" "paquetes instala letras /desde=http://prueba/" ":pausa 3" "paquetes catalogo" ":pausa 3"
USB_IMG=$Q/usb3.img paq $Q/s9.img firma3 "paquetes instala letras /desde=http://prueba/" ":pausa 2" "paquetes catalogo" ":pausa 2"
cat $Q/firma2.sal $Q/firma3.sal >> $Q/firma.sal; S=$Q/firma.sal
mira "la firma (Ed25519, en Castel): PAQUETES FIRMA dice si es buena; el catálogo de Internet firmado se usa (y lo dice); cambiado una letra, o sin su CATALOGO.FIR, no se usa nada de él; de una carpeta, sin firma, sí; y si el servidor aún no tiene catálogo (404), lo dice en llano" \
  "^La firma de X:.paquetes.catalogo\.txt es buena (la clave [0-9a-f]\{16\}; [0-9]* tics)\.$" "^paquetes: no está su firma (X:.paquetes.letras1\.FIR), o no se entiende$" \
  "^La firma del catálogo es buena (la clave [0-9a-f]\{16\}; [0-9]* tics)\.$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^La firma de X:.paquetes.catalogo\.txt NO es buena\.$" "^paquetes: la firma del catálogo NO es buena: no se usa nada de él$" \
  "^paquetes: el catálogo de Internet no trae su firma (CATALOGO.FIR): no se usa$" "^  letras    1\.1  *Unas letras, mejores$" \
  "^Todavía no hay actualizaciones publicadas en el servidor\.$"
if [ "$(grep -c '^paquetes: la firma del catálogo NO es buena' $S)" = 2 ] && ! grep -q '^  instalado' $S && sano $Q/s9.img && [ "$(mdir -/ -b -i $Q/s9.img ::HISPA/PAQUETES 2> /dev/null | grep -v 'BAJADAS\|/SERIE.TXT$' | tr '\n' ' ')" = "::/HISPA/PAQUETES/BASE.PQI " ]; then    # ((USUARIOS) SERIE.TXT: la del catálogo bueno)
  ok "y con la firma mala, nada: ni de Internet ni de la carpeta (que trae CATALOGO.FIR); el disco sano, con lo de antes"
else ko "con la firma mala se ha usado algo:"; lista_de $Q/s9.img | sed 's/^/        /' | head -10; fi

# 10. el núcleo: un «base» que cambia \HISPA\NUCLEO.BIN deja el de antes en
# NUCLEO.ANT, GRUBENV con «nuevo=1» (1.024 octetos, como lo quiere GRUB) y
# la entrada del de antes en GRUB.CFG, con lo que hace que GRUB vuelva solo a
# él (una vez, aunque se actualice otra vez); y pide reiniciar
cp $Q/sis.img $Q/s10.img
printf 'set modo=800x600x32,800x600,auto\nset default=0\nmenuentry "Hispa-Pentium" {\n  multiboot /HISPA/NUCLEO.BIN escritorio escritura\n}\n' > $Q/GRUB.CFG
cp $F/NUC07.BIN $Q/NUCLEO.BIN
mcopy -i $Q/s10.img $Q/GRUB.CFG $Q/NUCLEO.BIN ::HISPA/
paq $Q/s10.img nucleo 'paquetes instala x:\paquetes\base09.hpq' ":pausa 3" "paquetes lista" 'paquetes instala x:\paquetes\base091.hpq' ":pausa 3"
mira "PAQUETES con un núcleo nuevo (\\HISPA\\NUCLEO.BIN): lo instala y pide reiniciar; si no arrancara, vuelve el de antes" \
  "^  instalado base 0\.9$" "^reiniciar|Reinicie para usar el núcleo nuevo; si no arranca, vuelve el de antes\.$" "^  base      0\.9 " "^  instalado base 0\.9\.1$"
saca $Q/s10.img HISPA/GRUB.CFG $Q/grub10.cfg; saca $Q/s10.img HISPA/GRUBENV $Q/grubenv10
if sano $Q/s10.img && sin_restos $Q/s10.img && igual_en $Q/s10.img HISPA/NUCLEO.BIN $F/NUC091.BIN && igual_en $Q/s10.img HISPA/NUCLEO.ANT $F/NUC09.BIN \
  && [ "$(stat -c %s $Q/grubenv10)" = 1024 ] && head -c 33 $Q/grubenv10 | grep -q '^nuevo=1$' && [ "$(grep -c -- '--id anterior' $Q/grub10.cfg)" = 1 ] \
  && grep -q '^  multiboot /HISPA/NUCLEO.ANT escritorio escritura$' $Q/grub10.cfg && grep -q '^  load_env -f /HISPA/GRUBENV nuevo$' $Q/grub10.cfg && head -1 $Q/grub10.cfg | grep -q '^set modo='; then
  ok "y en el disco: el núcleo nuevo en NUCLEO.BIN y el de justo antes en NUCLEO.ANT; GRUBENV (1.024 octetos) dice nuevo=1; GRUB.CFG, con la entrada del de antes una sola vez"
else ko "el núcleo nuevo no deja bien el arranque:"; lista_de $Q/s10.img | sed 's/^/        /' | head -12; tail -n 5 $Q/grub10.cfg | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los paquetes)"
fi

# ------------------------------------------------- 24. (PROGRAMAS) Programas instalados
# Los tipos de los paquetes (tipo=, en la cabecera y en el catálogo: si no lo
# dicen, por su nombre, u «otros»), PAQUETES LISTA /TIPO, los programas
# sueltos (sin paquete), lo imprescindible (base y red no se quitan), quitar
# con lo que lo necesita (/DEPENDIENTES, en su orden), REPARA (solo lo
# dañado: un archivo cambiado y otro que falta), un corte a mitad de quitar
# (se deshace al volver) y la ventana «Programas instalados» (PROGRAMA.BIN):
# desde Configuración y desde «Programas y actualizaciones», el árbol por
# tipos, lo que no se puede quitar, quitar con lo que lo necesita (con la
# barra), los archivos, reparar y borrar un programa suelto (a la Papelera).
if parte08 programas; then
echo "== (PROGRAMAS) Programas instalados: los tipos, lo suelto, lo imprescindible, quitar con lo que lo necesita, reparar y la ventana =="
Q=$P/n08g
mkdir -p $Q/f/paq $G
HPQ="python3 -I $AQUI/../tools/hpq.py"
export MTOOLS_SKIP_CHECK=1
hechos=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null 2>&1 && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN || hechos=0
# (PAQUETES con la clave de prueba, como en la parte 12)
$HPQ clave $Q/prueba.key > /dev/null 2>&1; $HPQ publica $Q/prueba.key $Q/clave.cas > /dev/null || hechos=0
cp $AQUI/nucleo32/upaquete.cas $Q/upaquete.cas
for p in uordenes:ORDENES upaquete:PAQUETES uactual:ACTUALIZ uprogram:PROGRAMA uhola:HOLA uequipo:EQUIPO; do
  fu=$AQUI/nucleo32/${p%%:*}.cas; [ ${p%%:*} = upaquete ] && fu=$Q/upaquete.cas
  python3 -I $AQUI/ccastel.py $fu -o $Q/${p%%:*}.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $Q/${p%%:*}.asm -o $Q/f/${p##*:}.BIN || hechos=0
done
F=$Q/f
printf 'letras A\r\n' > $F/A.TXT; printf 'letras B\r\n' > $F/B.TXT; printf 'extra\r\n' > $F/E.TXT; printf 'el juego\r\n' > $F/J.TXT
printf 'la red (de prueba)\r\n' > $F/R.TXT; head -c 70000 /dev/urandom > $F/GRANDE.DAT
( cd $F
  $HPQ crea BASE.HPQ --nombre base --version 0.8.4 --tipo sistema --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' \
    ACTUALIZ.BIN='\HISPA\ACTUALIZ.BIN' PROGRAMA.BIN='\HISPA\PROGRAMA.BIN' EQUIPO.BIN='\HISPA\EQUIPO.BIN'
  $HPQ registro BASE.HPQ BASE.PQI
  $HPQ crea RED.HPQ --nombre red --version 0.8.4 --descripcion 'La red (de prueba)' --necesita 'base' R.TXT='\HISPA\RED.TXT'
  $HPQ registro RED.HPQ RED.PQI
  $HPQ crea paq/LETRAS.HPQ --nombre letras --version 1.0 --tipo oficina --descripcion 'Unas letras' A.TXT='\DOCS\LETRAS\A.TXT' B.TXT='\DOCS\LETRAS\B.TXT'
  $HPQ crea paq/SALUDO.HPQ --nombre saludo --version 1.0 --tipo 'Juegos y emuladores' --descripcion 'Un programa que saluda' --necesita 'letras >= 1.0' HOLA.BIN='\HISPA\SALUDO.BIN' GRANDE.DAT='\HISPA\SALUDO.DAT'
  $HPQ crea paq/EXTRA.HPQ --nombre extra --version 1.0 --descripcion 'Lo que necesita a saludo' --necesita 'saludo' E.TXT='\EXTRA\E.TXT'
  $HPQ crea paq/JUEGO.HPQ --nombre juego --version 1.0 --descripcion 'Un juego (su tipo, solo en el catálogo)' J.TXT='\JUEGOS\J.TXT'
  $HPQ catalogo paq
  printf 'tipo=juego juegos\r\n' >> paq/CATALOGO.TXT
  $HPQ firma paq $Q/prueba.key
  $HPQ registro paq/LETRAS.HPQ LETRAS.PQI ) > $Q/hpq.txt 2>&1 || hechos=0
# el disco: base y red instalados, y los sueltos (un .EXE en \HISPA32, un .BIN
# en \HISPA y otro en una subcarpeta de \PROGRAMS; un .TXT, que no es programa)
printf 'no es un programa\r\n' > $F/LEEME.TXT
mkfs.fat -F 32 -s 1 -n HISPA -C $Q/sis.img 65536 > /dev/null && mmd -i $Q/sis.img ::HISPA ::HISPA/PAQUETES ::HISPA/MENU ::HISPA32 ::PROGRAMS ::PROGRAMS/COSAS \
  && mcopy -i $Q/sis.img $F/ORDENES.BIN $F/PAQUETES.BIN $F/ACTUALIZ.BIN $F/PROGRAMA.BIN $F/EQUIPO.BIN $F/R.TXT ::HISPA/ && mren -i $Q/sis.img ::HISPA/R.TXT ::HISPA/RED.TXT \
  && mcopy -i $Q/sis.img $F/BASE.PQI $F/RED.PQI ::HISPA/PAQUETES/ && mcopy -i $Q/sis.img $F/HOLA.BIN ::HISPA32/MIJUEGO.EXE && mcopy -i $Q/sis.img $F/LEEME.TXT ::HISPA32/ \
  && mcopy -i $Q/sis.img $F/HOLA.BIN ::HISPA/SUELTO.BIN && mcopy -i $Q/sis.img $F/HOLA.BIN ::PROGRAMS/COSAS/OTRO.BIN || hechos=0
mkfs.fat -F 16 -n MEMORIA -C $Q/usb.img 16384 > /dev/null && mmd -i $Q/usb.img ::PAQUETES && mcopy -i $Q/usb.img $F/paq/* ::PAQUETES/ || hechos=0
if [ $hechos = 1 ] && grep -q $'^tipo=sistema\r$' $F/BASE.PQI && grep -q '^tipo=juegos' $F/paq/SALUDO.HPQ && ! grep -aq '^tipo=' $F/paq/JUEGO.HPQ \
  && grep -q '^tipo=letras oficina' $F/paq/CATALOGO.TXT && ! grep -q '^tipo=extra' $F/paq/CATALOGO.TXT && grep -q '^tipo=juego juegos' $F/paq/CATALOGO.TXT \
  && $HPQ comprueba $F/paq $Q/clave.cas > /dev/null && [ "$($HPQ tipos | wc -l)" = 11 ]; then
  ok "tools/hpq.py: el tipo en la cabecera (--tipo, por su código o por su nombre: «Juegos y emuladores» es juegos) y en el catálogo, una línea «tipo=» por paquete (el que no lo dice ni se sabe, sin línea: «otros»)"
else ko "tools/hpq.py no pone bien los tipos:"; tail -4 $Q/hpq.txt | sed 's/^/        /'; grep '^tipo' $F/paq/CATALOGO.TXT | sed 's/^/        /'; fi
paq() {
  local d=$1 n=$2; shift 2
  ESCRIBE=1 SATA=$d MEMORIA=64 MODULOS= ARRANQUE=ordenes QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 2" "$@" ":pausa 1" > $Q/$n.sal 2>&1
  S=$Q/$n.sal
}
lista_de() { mdir -/ -b -i $1 :: 2> /dev/null | sort; }
sin_restos() { ! lista_de $1 | grep -q 'HPQ[0-9]*\.\(NUE\|VIE\)\|DIARIO\.TXT\|HECHO\.TXT'; }
igual_en() { rm -f $Q/x.bin; mcopy -n -i $1 "::$2" $Q/x.bin > /dev/null 2>&1 && cmp -s $Q/x.bin $3; }

# 1. instalar (con lo que necesitan), la lista por tipos, lo suelto, lo que no se quita
cp $Q/sis.img $Q/s1.img
paq $Q/s1.img uno "paquetes instala extra juego" ":pausa 3" "paquetes lista /tipo" "paquetes sueltos" "paquetes quita base" "paquetes quita red /forzar" "paquetes quita letras" "paquetes fichas"
mira "PAQUETES INSTALA: extra con lo que necesita (saludo, y antes letras) y juego" "^Se va a instalar: letras 1\.0, saludo 1\.0, extra 1\.0, juego 1\.0$" "^  instalado juego 1\.0$"
mira "PAQUETES LISTA /TIPO: por tipos, en su orden (base por su cabecera, red por su nombre, juego por el catálogo, extra «Otros»), y al final los programas sueltos" \
  "^Paquetes instalados, por tipos" "^Sistema:$" "^  base      0\.8\.4   El sistema base (de prueba)$" "^Oficina:$" "^  letras    1\.0     Unas letras$" "^Internet y red:$" "^  red       0\.8\.4 " \
  "^Juegos y emuladores:$" "^  saludo    1\.0 " "^  juego     1\.0 " "^Otros:$" "^  extra     1\.0 " "^6 paquetes\.$" "^Programas de terceros (sin paquete):$"
mira "PAQUETES SUELTOS: los .EXE y .BIN que no son de ningún paquete (en \\HISPA, \\HISPA32 y las subcarpetas de \\PROGRAMS), con su tamaño y su fecha" \
  "^  C:.HISPA32.MIJUEGO\.EXE  *[0-9]* octetos  [0-9][0-9]/[0-9][0-9]/20[0-9][0-9]$" "^  C:.HISPA.SUELTO\.BIN " "^  C:.PROGRAMS.COSAS.OTRO\.BIN " "^3 programas sueltos\.$"
mira "lo imprescindible no se quita (ni con /FORZAR), y se dice por qué; lo que otro necesita, tampoco (y se dice cómo)" \
  "^paquetes: base no se puede quitar: es imprescindible (es el sistema mismo; sin él, el equipo no arranca)\.$" \
  "^paquetes: red no se puede quitar: es imprescindible (sin ella no llegan las actualizaciones ni se pueden traer programas)\.$" \
  "^paquetes: letras no se quita: lo necesita saludo (quite antes saludo, o /FORZAR)$" "^  (o con /DEPENDIENTES, que lo quita también)$"
mira "PAQUETES FICHAS (para la ventana): el disco, y de cada uno su tipo, lo que ocupa, sus archivos, la fecha, si es imprescindible, lo que necesita y lo que lo necesita; y lo suelto" \
  "^u|C|[0-9]*|[0-9]*$" "^f|base|0\.8\.4|sistema|[0-9]*|5|[0-9/]*|es el sistema mismo; sin él, el equipo no arranca||red|El sistema base (de prueba)$" \
  "^f|saludo|1\.0|juegos|70[0-9][0-9][0-9]|2|[0-9][0-9]/[0-9][0-9]/20[0-9][0-9]||letras >= 1\.0|extra|Un programa que saluda$" "^f|juego|1\.0|juegos|" "^f|extra|1\.0|otros|" \
  "^s|C:.HISPA32.MIJUEGO\.EXE|[0-9]*|[0-9/]*$"
saca $Q/s1.img HISPA/PAQUETES/JUEGO.PQI $Q/juego.pqi
if sano $Q/s1.img && sin_restos $Q/s1.img && ! grep -q 'LEEME' $S && grep -aq '^tipo=juegos' $Q/juego.pqi && [ "$(grep -c '^  quitado' $S)" = 0 ] && igual_en $Q/s1.img HISPA/PAQUETES/LETRAS.PQI $F/LETRAS.PQI; then
  ok "en el disco: sano, sin restos; el registro de juego lleva el tipo que dice el catálogo (tipo=juegos); el de letras, su cabecera tal cual; el .TXT no sale como suelto; y no se ha quitado nada"
else ko "lo instalado con los tipos no queda bien:"; lista_de $Q/s1.img | sed 's/^/        /' | head -14; sed 's/^/        /' $Q/juego.pqi | head -10; fi

# 2. QUITA letras /DEPENDIENTES: también saludo y extra, cada uno antes de lo que necesita
cp $Q/s1.img $Q/s2.img
paq $Q/s2.img dep "paquetes quita letras /dependientes" ":pausa 2" "paquetes lista"
mira "PAQUETES QUITA letras /DEPENDIENTES: lo que lo necesita (saludo y extra) sale antes que él" "^Se va a quitar: .*letras 1\.0" "^  quitado extra 1\.0$" "^  quitado saludo 1\.0$" "^  quitado letras 1\.0$" "^3 paquetes\.$"
l1=$(grep -n '^  quitado extra' $S | cut -d: -f1); l2=$(grep -n '^  quitado saludo' $S | cut -d: -f1); l3=$(grep -n '^  quitado letras' $S | cut -d: -f1)
if [ -n "$l1" ] && [ -n "$l3" ] && [ "$l1" -lt "$l2" ] && [ "$l2" -lt "$l3" ] && sano $Q/s2.img && sin_restos $Q/s2.img && ! lista_de $Q/s2.img | grep -q 'DOCS\|EXTRA\|SALUDO' && lista_de $Q/s2.img | grep -q 'JUEGOS/J.TXT'; then
  ok "en su orden (extra, saludo, letras) y, en el disco, sano, sin sus archivos ni sus carpetas (\\DOCS, \\EXTRA) y con lo demás (juego)"
else ko "quitar con lo que lo necesita no queda bien:"; grep '^  quitado' $S | sed 's/^/        /'; lista_de $Q/s2.img | sed 's/^/        /' | head -12; fi

# 3. REPARA: un archivo cambiado (letras) y otro que falta (saludo); solo eso se vuelve a poner
cp $Q/s1.img $Q/s3.img
printf 'cambiado a mano\r\n' > $Q/cambiado.txt; mcopy -o -i $Q/s3.img $Q/cambiado.txt ::DOCS/LETRAS/A.TXT; mdel -i $Q/s3.img ::HISPA/SALUDO.DAT
paq $Q/s3.img repara "paquetes archivos letras" "paquetes repara" ":pausa 3" "paquetes comprueba"
mira "PAQUETES ARCHIVOS letras: cada archivo, lo que mide y si está como se instaló" "^Archivos de letras 1\.0 (2):$" "^  CAMBIADO  *10  C:.DOCS.LETRAS.A\.TXT$" "^  bien  *10  C:.DOCS.LETRAS.B\.TXT$" \
  "^1 no están como se instalaron: PAQUETES REPARA letras los vuelve a poner\.$"
mira "PAQUETES REPARA: comprueba todos; lo dañado (cambiado o que falta) se vuelve a poner del paquete de la misma versión; luego COMPRUEBA, todo bien" \
  "^base 0\.8\.4 está bien (sus 5 archivos, como se instalaron)\.$" "^letras: .DOCS.LETRAS.A\.TXT (dañados)$" "^saludo: .HISPA.SALUDO\.DAT (dañados)$" "^Se va a reparar (lo dañado, del paquete de la misma versión): letras 1\.0, saludo 1\.0$" \
  "^  reparado letras 1\.0$" "^  reparado saludo 1\.0$" "^Los archivos de los 6 paquetes instalados están como se instalaron\.$"
if sano $Q/s3.img && sin_restos $Q/s3.img && igual_en $Q/s3.img DOCS/LETRAS/A.TXT $F/A.TXT && igual_en $Q/s3.img HISPA/SALUDO.DAT $F/GRANDE.DAT && igual_en $Q/s3.img DOCS/LETRAS/B.TXT $F/B.TXT; then
  ok "lo reparado, desde fuera: sano, sin restos y los archivos otra vez como los del paquete, octeto a octeto"
else ko "lo reparado no queda bien:"; lista_de $Q/s3.img | sed 's/^/        /' | head -12; fi

# 4. un corte a mitad de quitar (tras el primer cambio): al volver, se deshace
cp $Q/s1.img $Q/s4.img
paq $Q/s4.img corte "paquetes quita juego /corte=1" ":pausa 2"
paq $Q/s4.img vuelta "paquetes lista"
mira "un corte a mitad de quitar juego: la próxima vez, PAQUETES lo deshace y juego sigue instalado" \
  "^paquetes: juego 1\.0 se quedó a medias (¿se fue la luz?): se ha deshecho y todo está como antes$" "^  juego     1\.0 "
if sano $Q/s4.img && [ "$(lista_de $Q/s4.img)" = "$(lista_de $Q/s1.img)" ] && igual_en $Q/s4.img JUEGOS/J.TXT $F/J.TXT; then
  ok "y en el disco, como antes del corte: los mismos archivos, iguales"
else ko "después del corte el disco no vuelve a como estaba:"; diff <(lista_de $Q/s4.img) <(lista_de $Q/s1.img) | sed 's/^/        /' | head -8; fi

# 5. la ventana «Programas y actualizaciones» (PROGRAMA.BIN, una sola, con
# pestañas: Pendientes, Al día, Instaladas y Sin instalar, y la búsqueda), en
# el escritorio, con otra memoria (su catálogo trae juego 1.1, visor, que no
# está instalado, y red, para repararlo). Primero: desde Configuración, en
# «Instaladas» (el árbol por tipos; base, que no se quita; letras, que
# necesitan otros); desde «Actualizar» del menú, en «Pendientes» (juego 1.1);
# «Sin instalar» e Instalar (visor); buscar «juego» (en todas las pestañas) y
# Actualizar. Luego, con un acceso del menú con /LENTO (para ver la barra):
# quitar letras con lo que lo necesita, los archivos de red (uno cambiado a
# mano), repararlo, y borrar un programa suelto (a la Papelera)
( cd $F && printf 'el juego, la 1.1\r\n' > J11.TXT && printf 'un visor\r\n' > V.TXT && mkdir -p paq2 && cp paq/*.HPQ RED.HPQ paq2/ \
  && $HPQ crea paq2/JUEGO11.HPQ --nombre juego --version 1.1 --tipo juegos --descripcion 'Un juego (la 1.1)' J11.TXT='\JUEGOS\J.TXT' \
  && $HPQ crea paq2/VISOR.HPQ --nombre visor --version 1.0 --tipo multimedia --descripcion 'Un visor de imágenes' V.TXT='\VISOR\V.TXT' \
  && $HPQ catalogo paq2 --clave $Q/prueba.key ) >> $Q/hpq.txt 2>&1
mkfs.fat -F 16 -n MEMORIA -C $Q/usb2.img 16384 > /dev/null && mmd -i $Q/usb2.img ::PAQUETES && mcopy -i $Q/usb2.img $F/paq2/* ::PAQUETES/
cp $Q/s1.img $Q/s5.img
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $Q/HISPA.INI
printf 'nombre=Programas (prueba)\r\nprograma=\\HISPA\\PROGRAMA.BIN\r\nargumentos=/LENTO /INSTALADAS\r\n' > $Q/PRUEBA.MNU
printf 'red cambiada\r\n' > $Q/rc.txt
mcopy -i $Q/s5.img $Q/HISPA.INI ::HISPA/ && mcopy -i $Q/s5.img $Q/PRUEBA.MNU ::HISPA/MENU/ && mcopy -o -i $Q/s5.img $Q/rc.txt ::HISPA/RED.TXT
CAPTURAS=$G ESCRIBE=1 SATA=$Q/s5.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/usb2.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":clic 40 585" ":pausa 1" ":clic 300 238" ":pausa 1" ":captura pg0" ":clic 592 326" ":pausa 5" ":raton 790 300" ":captura pg1" \
  ":tecla down" ":tecla delete" ":pausa 1" ":captura pg2" ":tecla esc" ":tecla down" ":tecla down" ":tecla delete" ":pausa 1" ":captura pg3" ":tecla esc" ":tecla esc" ":pausa 1" \
  ":clic 40 585" ":pausa 1" ":clic 290 338" ":pausa 5" ":captura pg4" ":tecla ctrl-tab" ":tecla ctrl-tab" ":tecla ctrl-tab" ":pausa 1" ":captura pg5" ":tecla down" ":tecla ret" ":pausa 5" ":captura pg6" \
  ":tecla ctrl-tab" ":tecla ctrl-tab" ":pausa 1" ":escribe juego" ":pausa 1" ":captura pg7" ":tecla ret" ":tecla down" ":clic 720 374" ":pausa 5" ":captura pg8" ":tecla esc" ":tecla esc" ":pausa 1" \
  ":clic 40 585" ":pausa 1" ":captura pgm" ":clic 330 308" ":pausa 5" ":captura pgi" ":tecla esc" ":pausa 1" ":clic 40 585" ":pausa 1" ":captura pgm2" ":clic 100 348" ":pausa 5" ":tecla down" ":tecla down" ":tecla down" ":tecla delete" ":pausa 1" ":captura pg9" ":tecla tab" ":tecla ret" ":pausa 1" ":captura pg10" ":pausa 10" ":captura pg11" \
  ":tecla home" ":tecla down" ":tecla down" ":tecla down" ":tecla ret" ":pausa 3" ":captura pg12" ":clic 720 403" ":pausa 5" ":captura pg13" \
  ":tecla end" ":tecla up" ":tecla delete" ":pausa 1" ":captura pg14" ":tecla tab" ":tecla ret" ":pausa 4" ":captura pg15" > $Q/ventana.sal 2>&1
miracap "Configuración (el Panel de control) trae «Programas»" pg0 negrita 140 80 160 95 "Configuración" negrita 550 341 560 351 "Programas"
miracap "«Programas...» abre la ventana en «Instaladas (9)» (con las otras pestañas y su cuenta), la búsqueda con su lupa, el árbol por tipos, la ficha, los botones y lo libre en el disco" pg1 \
  negrita 0 0 400 30 "Programas y actualizaciones" negrita 240 120 380 140 "Instaladas (9)" texto 10 120 150 140 "Pendientes (1)" texto 140 120 250 140 "Al día (5)" texto 370 120 520 140 "Sin instalar (1)" \
  texto 30 88 320 110 "Buscar por nombre, qué es o tipo" negrita 20 170 130 195 "[-] Sistema" negrita 40 195 120 220 base negrita 20 265 220 290 "[-] Internet y red" \
  negrita 10 340 120 360 Detalles negrita 640 360 790 390 "Desinstalar..." negrita 640 390 790 415 Reparar negrita 640 420 790 445 "Ver archivos" \
  texto 630 465 790 490 "Libre en el disco:" texto 5 538 200 560 "6 paquetes"
miracap "base no se puede desinstalar: lo dice, y por qué" pg2 texto 140 220 660 320 "base no se puede desinstalar: es imprescindible (es el" negrita 400 260 660 360 Entendido \
  negrita 10 360 640 530 "Lo necesitan:"
miracap "letras: hay otros que lo necesitan (saludo y extra): se dice, y se ofrece quitarlos también" pg3 texto 140 220 660 320 "Hay programas que necesitan letras: saludo, extra. Sin" \
  negrita 380 270 660 370 "Quitar los 3" negrita 380 270 660 370 Cancelar
miracap "«Actualizar» del menú abre la misma ventana en «Pendientes (1)»: juego, de la 1.0 a la 1.1, con «Actualizar» y «Actualizar todo»" pg4 \
  negrita 10 120 150 140 "Pendientes (1)" negrita 20 170 330 195 "[-] Juegos y emuladores" negrita 50 195 120 220 juego texto 200 195 290 220 "1.0" texto 290 195 340 220 "1.1" \
  negrita 640 360 790 390 Actualizar negrita 640 390 790 415 "Actualizar todo"
miracap "Ctrl+Tab hasta «Sin instalar (1)»: visor (Multimedia), con «Instalar»" pg5 negrita 370 120 530 140 "Sin instalar (1)" negrita 20 170 250 195 "[-] Multimedia" negrita 50 195 120 220 visor \
  negrita 640 360 790 390 Instalar
miracap "Instalar visor: hecho, y ya no queda nada sin instalar" pg6 texto 400 538 795 560 "visor instalado." negrita 370 120 530 140 "Sin instalar (0)" texto 10 170 400 200 "Todo lo del catálogo está instalado."
miracap "buscar «juego» (sin tildes ni mayúsculas) filtra todas las pestañas: Pendientes (1: juego), Al día (1: saludo, por su tipo, Juegos), Instaladas (3: y MIJUEGO.EXE)" pg7 \
  texto 30 88 320 110 juego negrita 10 120 150 140 "Pendientes (1)" texto 140 120 250 140 "Al día (1)" texto 220 120 380 140 "Instaladas (3)"
miracap "y «Actualizar» con juego elegido: juego 1.1, y ya no hay pendientes" pg8 texto 300 538 795 560 "juego actualizado a la 1.1." negrita 10 120 150 140 "Pendientes (0)"
miracap "el menú Inicio nuevo: «Programas instalados» y «Actualizar» en la columna de la derecha" pgm negrita 250 295 430 320 "Programas instalados" negrita 250 325 400 350 Actualizar
# (los más usados del menú suben: «Programas (prueba)», el cuarto de ellos)
miracap "«Programas instalados» del menú abre la ventana en «Instaladas»" pgi negrita 240 120 380 140 "Instaladas (10)" negrita 20 170 130 195 "[-] Sistema"
miracap "desde el acceso con /LENTO (en Instaladas): lo mismo con letras" pg9 negrita 380 270 660 370 "Quitar los 3"
miracap "al decir que sí, la barra de avance mientras se quita (archivo a archivo)" pg10 negrita 150 230 650 300 "Quitando" rgb 200 301 166534 40
miracap "y luego, la lista sin letras, saludo ni extra; «Desinstalado.»" pg11 texto 500 538 795 560 "Desinstalado." notexto 20 160 630 330 letras notexto 20 160 630 330 saludo \
  negrita 20 160 400 330 "[-] Juegos y emuladores" negrita 20 160 400 330 "[-] Multimedia"
miracap "Ver archivos (Intro en red): su archivo, CAMBIADO (en naranja y con su palabra)" pg12 negrita 90 335 200 360 Archivos negrita 15 380 200 420 CAMBIADO negrita 100 380 420 420 'C:\HISPA\RED.TXT' \
  texto 250 538 795 560 "Un archivo dañado"
miracap "Reparar: lo vuelve a poner, y luego está bien" pg13 negrita 15 380 200 420 bien texto 250 538 795 560 "red reparado"
miracap "un programa suelto (sin paquete): «Borrar...» pregunta, y dice que va a la Papelera" pg14 texto 140 220 660 320 'C:\HISPA32\MIJUEGO.EXE (760 octetos)? Va a la' \
  negrita 640 360 790 390 "Borrar..."
miracap "borrado: ya no sale, y «Está en la Papelera.»" pg15 texto 500 538 795 560 "Está en la Papelera." notexto 20 160 630 330 MIJUEGO
if sano $Q/s5.img && sin_restos $Q/s5.img && ! lista_de $Q/s5.img | grep -q 'DOCS\|EXTRA\|HISPA/SALUDO\|HISPA32/MIJUEGO' && igual_en $Q/s5.img JUEGOS/J.TXT $F/J11.TXT \
  && igual_en $Q/s5.img HISPA/RED.TXT $F/R.TXT && igual_en $Q/s5.img VISOR/V.TXT $F/V.TXT \
  && lista_de $Q/s5.img | grep -qi 'PAPELERA/' && ! lista_de $Q/s5.img | grep -q 'PROGRAMA\.TXT\|AVANCE\.TXT'; then
  ok "lo hecho desde la ventana, desde fuera: el disco sano, visor instalado, juego en la 1.1, sin letras, saludo ni extra, red reparado (octeto a octeto), MIJUEGO.EXE en la Papelera, y sin restos (ni PROGRAMA.TXT ni AVANCE.TXT)"
else ko "lo hecho desde la ventana no queda bien en el disco:"; lista_de $Q/s5.img | sed 's/^/        /' | head -16; fi
# (GUARDA_CAPTURAS=carpeta: las capturas de la ventana se quedan ahí)
[ -n "${GUARDA_CAPTURAS:-}" ] && mkdir -p "$GUARDA_CAPTURAS" && cp $G/pg*.ppm "$GUARDA_CAPTURAS/"
# (GRUPOS y RUEDA, 25/11/2026) la pestaña «Grupos» y la rueda: con lo de
# antes instalado (letras, saludo, extra y juego) y un catálogo con grupos
# (todo: letras, saludo, extra y visor; juegos: juego y saludo; red: la red,
# imprescindible; y, de GRUPOS de hpq.py, «Todo Internet y redes», con red). La rueda sobre el árbol de «Instaladas» lo baja tres
# renglones (antes no hacía nada); en Grupos, el grupo con lo que lleva
# ([+]), su botón «Faltan 1 de 4: instalar lo que falta», la barra y el
# resumen; luego «Ya está todo» y «Quitar grupo...» (se quedan letras,
# que lo necesita saludo, y saludo, que es de juegos, que está entero)
( cd $F && rm -rf paq3 && mkdir paq3 && cp paq2/*.HPQ paq3/ && $HPQ catalogo paq3 \
  && printf 'grupo=todo letras,saludo,extra,visor Todo junto|Las letras, el saludo, lo extra y el visor\r\ngrupo=juegos juego,saludo Los juegos|El juego y el saludo\r\ngrupo=red red Solo la red|La red (imprescindible)\r\n' >> paq3/CATALOGO.TXT \
  && $HPQ firma paq3 $Q/prueba.key ) >> $Q/hpq.txt 2>&1
mkfs.fat -F 16 -n MEMORIA -C $Q/usb3.img 16384 > /dev/null && mmd -i $Q/usb3.img ::PAQUETES && mcopy -i $Q/usb3.img $F/paq3/* ::PAQUETES/
cp $Q/s1.img $Q/sg.img
mcopy -i $Q/sg.img $Q/HISPA.INI ::HISPA/
CAPTURAS=$G ESCRIBE=1 SATA=$Q/sg.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$Q/usb3.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 3" ":clic 40 585" ":pausa 1" ":clic 300 238" ":pausa 1" ":clic 592 326" ":pausa 5" ":raton 300 260" ":pausa 1" ":captura gr0" \
  ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura gr1" ":monitor mouse_move 0 0 1" ":pausa 1" ":captura gr1b" \
  ":tecla ctrl-tab" ":tecla ctrl-tab" ":pausa 1" ":tecla down" ":raton 790 300" ":pausa 1" ":captura gr2" ":tecla right" ":pausa 1" ":captura gr3" \
  ":clic 720 374" ":pausa 6" ":captura gr4" ":tecla esc" ":raton 790 300" ":pausa 1" ":captura gr5" ":clic 720 403" ":pausa 1" ":captura gr6" ":tecla tab" ":tecla ret" ":pausa 6" ":captura gr7" > $Q/grupos.sal 2>&1
miracap "la rueda en «Programas y actualizaciones» (no la atendía): sobre el árbol de «Instaladas», un paso abajo, tres renglones (Sistema ya no se ve arriba)" gr1 \
  notexto 20 160 630 185 "Sistema" notexto 20 160 630 185 "base"
miracap "y un paso arriba, otra vez como estaba" gr1b negrita 20 160 200 195 "[-] Sistema"
miracap "la pestaña «Grupos (4)» (con «Todo Internet y redes» de GRUPOS, que lleva red): cada grupo, con cuántos tiene instalados y su botón: «Faltan 1 de 4: instalar lo que falta», «Ya está todo»" gr2 \
  negrita 500 115 640 145 "Grupos (4)" negrita 20 160 330 330 "[+] Todo junto" negrita 400 160 790 330 "Faltan 1 de 4" texto 400 160 790 330 "Ya est" negrita 20 160 330 330 "[+] Todo Internet y redes"
miracap "abierto (flecha derecha): lo que lleva debajo (visor, sin instalar) y, en la ficha, qué es, lo que falta y el botón grande" gr3 \
  negrita 20 160 330 330 "[-] Todo junto" texto 20 160 630 330 visor negrita 10 350 630 530 "Falta:" texto 150 350 630 530 "visor (209 octetos)" negrita 10 350 630 530 "instalar lo que falta"
miracap "«Completar grupo» (instalar lo que falta): el resumen (1 instalados, 3 ya estaban, 0 fallos)" gr4 texto 120 200 680 400 "Todo junto: 1 instalados, 3 ya estaban, 0 fallos"
miracap "y luego, el grupo entero: «4 de 4», visor (sí)" gr5 negrita 230 160 330 330 "4 de 4" texto 150 400 630 420 "visor (s"
miracap "«Quitar grupo...»: pregunta, con lo que se quita (extra y visor; letras y saludo se quedan)" gr6 texto 120 200 680 400 "extra,visor" negrita 120 200 680 420 "Quitar el grupo"
miracap "y después: extra y visor quitados; letras y saludo, no (el grupo, «2 de 4»)" gr7 negrita 20 160 400 330 "[-] Todo junto" negrita 230 160 330 330 "2 de 4" texto 150 350 630 530 "extra, visor (407 octetos)"
if ! grep -q "excepción\|\*\*\*" $Q/grupos.sal; then ok "(con los grupos, ni una excepción del núcleo)"; else ko "hay excepciones con los grupos:"; grep "excepción\|\*\*\*" $Q/grupos.sal | head -3 | sed 's/^/        /'; fi
if sano $Q/sg.img && sin_restos $Q/sg.img && ! lista_de $Q/sg.img | grep -q 'EXTRA/\|VISOR/' && igual_en $Q/sg.img HISPA/SALUDO.DAT $F/GRANDE.DAT && igual_en $Q/sg.img DOCS/LETRAS/A.TXT $F/A.TXT; then
  ok "los grupos desde la ventana, desde fuera: el disco sano y sin restos; sin extra ni visor; con letras y saludo"
else ko "los grupos desde la ventana no dejan bien el disco:"; lista_de $Q/sg.img | sed 's/^/        /' | head -16; fi
[ -n "${GUARDA_CAPTURAS:-}" ] && mkdir -p "$GUARDA_CAPTURAS" && cp $G/gr*.ppm "$GUARDA_CAPTURAS/"
if ! grep -q "excepción\|\*\*\*" $Q/ventana.sal; then ok "ni una excepción del núcleo"; else ko "hay excepciones:"; grep "excepción\|\*\*\*" $Q/ventana.sal | head -3 | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los programas instalados)"
fi   # (PROGRAMAS)

# ------------------------------------------------- 13. Hispa-Pentium 0.8: la red
# La red: el servicio de red (RED.BIN, en el anillo 3) con las tarjetas que
# da QEMU (-netdev user: DHCP, la puerta 10.0.2.2, que es esta máquina, y el
# DNS 10.0.2.3), IPCONFIG, PING y DESCARGAR contra un servidor HTTP de aquí
# (nucleo32/servidor_prueba.py), y el icono de la red en el escritorio.
if parte08 red; then
echo "== Hispa-Pentium 0.8: la red (el servicio de red, IPCONFIG, PING, DESCARGAR y el icono) =="
R=$P/n08r
mkdir -p $R/srv $G
python3 -I $AQUI/nucleo32/servidor_prueba.py $R/srv > $R/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $R/srv/puerto ] && break; sleep 0.1; done
U=http://10.0.2.2:$(cat $R/srv/puerto 2> /dev/null)
if $AQUI/nucleo32/disco.sh $R/d.img 16 red > $R/disco.txt 2>&1; then
# la e1000 (la de QEMU por omisión, la 82540EM)
cp $R/d.img $R/e.img
ESCRIBE=1 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" "ping -n 3 10.0.2.2" ":pausa 3" \
  "descargar $U/grande.bin" ":pausa 5" "descargar $U/trozos.bin" ":pausa 4" "descargar $U/cierra.bin" ":pausa 3" "descargar $U/redirige -" ":pausa 3" \
  "descargar $U/nohay" ":pausa 2" "descargar $U/ultima.txt c:\\textos\\otra.txt" ":pausa 2" "descargar https://10.0.2.2/" "descargar" \
  "descargar $U/lento.bin" ":pausa 3" ":tecla ctrl-c" ":pausa 2" "ipconfig /todo" ":pausa 2" "mem" "apagar" > $R/e1000.sal 2>&1
tr -d '\r' < $R/e1000.sal > $R/e1000.txt
S=$R/e1000.txt
mira "el servicio de red se pone en marcha solo (la primera vez que se pide algo), encuentra la e1000 y pide la IP por DHCP" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^red: tarjeta 8086:100E en el bus PCI 00:03$" "^red: enlace: sí (1000 Mbit/s)$" \
  "^red: DHCP: IP 10.0.2.15, máscara 255.255.255.0, puerta 10.0.2.2, DNS 10.0.2.3$"
mira "IPCONFIG: la tarjeta, su MAC, el enlace, la IP, la máscara, la puerta de enlace, el DNS y el servidor de DHCP" \
  "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000) (8086:100E, IRQ [0-9]*)$" "^  MAC . . . . . . . . . : 52:54:00:12:34:56$" "^  Enlace  . . . . . . . : sí, a 1000 Mbit/s$" \
  "^  Estado  . . . . . . . : conectada$" "^  Dirección IP  . . . . : 10.0.2.15$" "^  Máscara de subred . . : 255.255.255.0$" "^  Puerta de enlace  . . : 10.0.2.2$" \
  "^  Servidor DNS  . . . . : 10.0.2.3$" "^  DHCP  . . . . . . . . : de 10.0.2.2, 86400 s"
mira "PING a la puerta de enlace: tres respuestas y ninguna perdida" "^Haciendo ping a 10.0.2.2 con 32 octetos de datos:$" \
  "^Respuesta desde 10.0.2.2: octetos=32 tiempo[<=][0-9]* ms TTL=[0-9]*$" "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$"
[ "$(grep -c "^Respuesta desde 10.0.2.2" $S)" = 3 ] || ko "PING: no son tres respuestas"
mira "DESCARGAR: con Content-Length, en trozos (chunked), hasta que se cierra, una redirección (302), un 404 y a otra carpeta" \
  "^Guardando GRANDE.BIN (409600 octetos)\.*$" "^Hecho: GRANDE.BIN, 409600 octetos en [0-9]*,[0-9] s" "^Hecho: TROZOS.BIN, 150001 octetos" "^Hecho: CIERRA.BIN, 70000 octetos" \
  "^Redirigido a /ultima.txt$" "^version=0.8$" "^DESCARGAR: no se ha bajado nada (el servidor ha dicho 404)\.$" "^Hecho: c:.textos.otra.txt, 41 octetos" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: no hay servicio TLS (falta TLS.BIN): no se pueden hacer conexiones cifradas\.$" "^Uso: DESCARGAR http\[s\]://servidor\[:puerto\]/camino \[archivo\]$"
mira "Ctrl+C en medio de una descarga que no llega: el programa acaba, y la red sigue (IPCONFIG /TODO, con lo que ha apuntado el servicio)" \
  "^Guardando LENTO.BIN (1000000 octetos)^C$" "^Lo que ha apuntado el servicio de red:$" "^red: DHCP: IP 10.0.2.15" "^Este intérprete ocupa [0-9]* K. Tareas en marcha: 5\.$"
iguales=1
for f in GRANDE.BIN:grande.bin TROZOS.BIN:trozos.bin CIERRA.BIN:cierra.bin TEXTOS/OTRA.TXT:ultima.txt; do
  saca $R/e.img ${f%%:*} $R/x.bin && cmp -s $R/x.bin $R/srv/${f##*:} || { iguales=0; echo "        ${f%%:*} no es igual"; }
done
sano $R/e.img
if [ $iguales = 1 ] && grep -q "^$R/e.img: [0-9]* files" $P/fsck.txt; then ok "lo descargado, en el disco, es igual octeto a octeto que lo del servidor (los cuatro archivos), y el disco queda sano"
else ko "lo descargado no es igual que lo del servidor, o el disco no queda sano"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# la RTL8139, y el DNS de QEMU (que pregunta al de esta máquina)
cp $R/d.img $R/r.img
ESCRIBE=1 DISCO=$R/r.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0 -device rtl8139,netdev=n0" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" \
  "descargar $U/grande.bin" ":pausa 5" "ping -n 1 nombre.que.no.existe.invalid" ":pausa 4" "ping -n 1 actualiza.hispados.es" ":pausa 4" "apagar" > $R/rtl.sal 2>&1
tr -d '\r' < $R/rtl.sal > $R/rtl.txt
S=$R/rtl.txt
mira "la Realtek RTL8139: IPCONFIG y DESCARGAR" "^  Tarjeta . . . . . . . : Realtek RTL8139 (10EC:8139, IRQ [0-9]*)$" "^  Enlace  . . . . . . . : sí, a 100 Mbit/s$" \
  "^  Dirección IP  . . . . : 10.0.2.15$" "^Hecho: GRANDE.BIN, 409600 octetos"
if saca $R/r.img GRANDE.BIN $R/x.bin && cmp -s $R/x.bin $R/srv/grande.bin; then ok "con la RTL8139, lo descargado es igual octeto a octeto"; else ko "con la RTL8139, lo descargado no es igual"; fi
# (RTL8168) el controlador de la RTL8111/8168 (rrtl.cas) contra la RTL8139 de
# QEMU en modo C+ (los mismos descriptores y los mismos registros de los
# anillos: RealtekEnsayo=SI en RED.INI): por la memoria (BAR1) y por los
# puertos (RealtekPuertos=SI). Los 400 K de GRANDE.BIN dan muchas vueltas a
# los dos anillos (16 de recepción, 8 de envío). Lo propio de la 8168 (el XID,
# el PHY por OCP, RXDV, ASPM) solo se ve en el equipo (el ASUS GL553VD).
for modo in memoria puertos; do
  cp $R/d.img $R/k.img
  if [ $modo = memoria ]; then printf 'RealtekEnsayo=SI\r\n' > $R/red.ini; else printf 'RealtekEnsayo=SI\r\nRealtekPuertos=SI\r\n' > $R/red.ini; fi
  mcopy -o -i $R/k.img $R/red.ini ::RED.INI
  ESCRIBE=1 DISCO=$R/k.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0 -device rtl8139,netdev=n0" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" \
    "ping -n 4 10.0.2.2" ":pausa 4" "descargar $U/grande.bin" ":pausa 5" "descargar $U/trozos.bin" ":pausa 4" "ipconfig" "apagar" > $R/r68$modo.sal 2>&1
  tr -d '\r' < $R/r68$modo.sal > $R/r68$modo.txt
  S=$R/r68$modo.txt
  if [ $modo = memoria ]; then m1="^red: realtek: registros en la memoria, [0-9A-F]* (BAR1)$"; else m1="^red: realtek: registros por los puertos, [0-9A-F]*$"; fi
  mira "(RTL8168) la RTL8111/8168 (rrtl.cas) en ensayo, por $modo: la encuentra, se pone en marcha, DHCP, PING sin perder ninguno y DESCARGAR" \
    "^red: RED.INI: RealtekEnsayo=SI: la RTL8139 de QEMU, con el controlador de la RTL8168 (modo C+)$" "$m1" \
    "^red: realtek: revisión PCI 20, TxConfig [0-9A-F]*, XID [0-9A-F]*: RTL8139C+ en modo C+ (ENSAYO)$" "^red: realtek: en marcha (RxConfig 0000E70E, TxConfig [0-9A-F]*, CPlusCmd 0003)$" \
    "^red: enlace: sí (100 Mbit/s)$" "^red: DHCP: IP 10.0.2.15, máscara 255.255.255.0, puerta 10.0.2.2, DNS 10.0.2.3$" \
    "^  Tarjeta . . . . . . . : Realtek RTL8139C+ en modo C+ (ENSAYO) (10EC:8139, IRQ [0-9]*)$" "^  MAC . . . . . . . . . : 52:54:00:12:34:56$" \
    "^    Paquetes: enviados = 4, recibidos = 4, perdidos = 0 (0% perdidos)$" "^Hecho: GRANDE.BIN, 409600 octetos" "^Hecho: TROZOS.BIN, 150001 octetos"
  if grep -q "^  Tramas  . . . . . . . : [0-9]* recibidas, [0-9]* enviadas, 0 con error$" $S && [ "$(grep "^  Tramas" $S | tail -1 | sed 's/^.*: \([0-9]*\) recibidas.*$/\1/')" -gt 300 ]; then
    ok "(RTL8168) por $modo: $(grep "^  Tramas" $S | tail -1 | sed 's/^  Tramas[ .]*: //') (los anillos, muchas vueltas)"
  else ko "(RTL8168) por $modo: las cuentas de tramas no son las que deben:"; grep "^  Tramas" $S | sed 's/^/        /'; fi
  iguales=1
  for f in GRANDE.BIN:grande.bin TROZOS.BIN:trozos.bin; do saca $R/k.img ${f%%:*} $R/x.bin && cmp -s $R/x.bin $R/srv/${f##*:} || iguales=0; done
  if [ $iguales = 1 ]; then ok "(RTL8168) por $modo: lo descargado es igual octeto a octeto"; else ko "(RTL8168) por $modo: lo descargado no es igual"; fi
done
S=$R/rtl.txt
# (el DNS de QEMU le pregunta al de esta máquina: si esta no sale a la red, no se prueba)
if getent hosts actualiza.hispados.es > /dev/null 2>&1; then
  mira "el DNS (el de QEMU, que pregunta al de esta máquina): un nombre que no existe se dice; uno que sí, se resuelve" \
    "^PING: no se encuentra nombre.que.no.existe.invalid: ese nombre no existe\.$" "^Haciendo ping a actualiza.hispados.es \[$(getent hosts actualiza.hispados.es | awk '{print $1}' | head -1)\] con 32 octetos de datos:$"
else echo "  (esta máquina no resuelve nombres: no se prueba el DNS)"; fi
# sin tarjeta, y con «sinred»
DISCO=$R/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 2" "ping 10.0.2.2" ":pausa 2" "apagar" > $R/sin.sal 2>&1
tr -d '\r' < $R/sin.sal > $R/sin.txt
S=$R/sin.txt
mira "sin tarjeta de red: el servicio lo dice, y PING también" "^red: no hay ninguna tarjeta de red que se sepa llevar$" \
  "^  No hay ninguna tarjeta de red que Hispa-Pentium sepa llevar$" "^PING: no se encuentra\|^Error: no hay ninguna tarjeta de red que Hispa-Pentium sepa llevar\.$"
DISCO=$R/d.img ARRANQUE="ordenes sinred" MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 1" "apagar" > $R/sinred.sal 2>&1
tr -d '\r' < $R/sinred.sal > $R/sinred.txt
S=$R/sinred.txt
if grep -q "^  no hay servicio de red (falta RED.BIN o se arrancó con «sinred»)\.$" $S && ! grep -q "^red:" $S; then ok "con «sinred», ni se pone en marcha el servicio de red (y IPCONFIG lo dice)"
else ko "con «sinred» se pone en marcha la red:"; sed 's/^/        /' $S | tail -n 6; fi
# el escritorio: el servicio, en marcha al arrancar (hay tarjeta), y su icono
CAPTURAS=$G DISCO=$R/d.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 5" ":captura r1" ":clic 720 583" ":pausa 1" ":captura r2" > $R/esc.sal 2>&1
S=$R/esc.sal
mira "el escritorio pone en marcha el servicio de red (hay una tarjeta) y, con un clic en su icono, dice cómo está" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^escritorio: aviso: La red: conectada, con la IP 10.0.2.15\.$"
miracap "el icono de la red en la bandeja, con la luz verde (conectada), a la izquierda del reloj" r1 rgb 729 587 2ECC40 rgb 725 582 8FDCFF
miracap "y el aviso de la red, al pulsarlo" r2 texto 230 255 260 280 "La red: conectada, con la IP 10.0.2.15."
else ko "no se puede hacer el disco de prueba de la red: $(tail -n 1 $R/disco.txt)"; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la red)"
fi   # la red

# ------------------------------------------- 13 bis. (TLS) Hispa-Pentium 0.8: HTTPS
# El servicio TLS (TLS.BIN: nucleo32/tls/, BearSSL en C en el anillo 3, como
# ACPI.BIN) con los servidores HTTPS de nucleo32/tls/servidor_https.py: su
# raíz de prueba va al disco (\RAICES\PRUEBA.CER) y sus certificados son
# para «10.0.2.2» (el bueno ECDSA, el bueno RSA, uno de otro nombre y uno de
# una raíz que no conoce nadie); y un HTTP para la cabecera Date.
if parte08 tls; then
echo "== Hispa-Pentium 0.8: HTTPS (el servicio TLS, TLS.BIN: DESCARGAR https://, la orden HTTPS y la biblioteca Https) =="
R=$P/n08t
mkdir -p $R/srv
if bash $AQUI/nucleo32/tls/hacer.sh $R/TLS.BIN > $R/hacer.txt 2>&1; then
  ok "nucleo32/tls/hacer.sh: TLS.BIN, BearSSL 0.6 (MIT) y tls.c como programa del anillo 3 ($(stat -c %s $R/TLS.BIN) octetos)"
else ko "nucleo32/tls/hacer.sh no hace TLS.BIN: $(tail -n 1 $R/hacer.txt)"; fi
python3 -I $AQUI/nucleo32/tls/servidor_https.py $R/srv > $R/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 100); do [ -s $R/srv/puertos ] && break; sleep 0.1; done
pu() { grep "^$1=" $R/srv/puertos | cut -d= -f2; }
B=https://10.0.2.2:$(pu bueno); RS=https://10.0.2.2:$(pu rsa); OT=https://10.0.2.2:$(pu otro); EX=https://10.0.2.2:$(pu extrano)
export MTOOLS_SKIP_CHECK=1
if [ -s $R/srv/puertos ] && $AQUI/nucleo32/disco.sh $R/d.img 16 red tls > $R/disco.txt 2>&1 && mmd -i $R/d.img ::RAICES && mcopy -i $R/d.img $R/srv/RAIZ.CER ::RAICES/PRUEBA.CER; then
cp $R/d.img $R/e.img
ESCRIBE=1 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/grande.bin" ":pausa 6" "descargar $RS/trozos.bin" ":pausa 5" "https -i $B/galleta $B/mira" ":pausa 4" \
  "https -d hola=mundo -H X-Uno:dos $RS/eco" ":pausa 3" "https $B/redirige $RS/lejos" ":pausa 4" \
  "descargar $OT/ultima.txt" ":pausa 3" "descargar $EX/ultima.txt" ":pausa 3" "https /estado" ":pausa 2" > $R/e.sal 2>&1
tr -d '\r' < $R/e.sal > $R/e.txt
S=$R/e.txt
mira "el servicio TLS se pone en marcha solo (la primera conexión cifrada), con las autoridades de HSPTLS y la raíz de prueba de \\RAICES, y la hora del reloj del PC" \
  "tls: Hispa-Pentium 0.8: el servicio TLS (TLS.BIN, BearSSL 0.6, TLS 1.2)$" "tls: autoridad de más: .RAICES.PRUEBA.CER$" "tls: 21 autoridades raíz$" \
  "tls: la hora, del reloj del PC: [0-9][0-9]/[0-9][0-9]/20[0-9][0-9]$"
mira "DESCARGAR https://: el certificado ECDSA y el RSA comprobados (con la raíz de prueba), con largo y en trozos" \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*; apretón de manos en [0-9]* ms (BearSSL: [0-9,]* M ciclos), la cadena comprobada$" \
  "^Conectando con 10.0.2.2 (10.0.2.2), puerto $(pu bueno)\.\.\. " "conectado (TLS 1.2, ECDHE-ECDSA, " "^Hecho: GRANDE.BIN, 409600 octetos en " \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-RSA, .*la cadena comprobada$" "^Hecho: TROZOS.BIN, 150001 octetos en "
mira "la orden HTTPS (la biblioteca Https): las galletas que pone el servidor vuelven en el pedido siguiente, por la misma conexión (keep-alive)" \
  "^== $B/galleta: 200, 18 octetos (redirecciones: 0)$" "^Set-Cookie: sesion=abc123; Path=/; HttpOnly$" "^== $B/mira: 200, 35 octetos" "^galletas=sesion=abc123; idioma=es$"
if [ "$(sed -n "/^C:.>https -i /,/^C:.>https -d /p" $S | grep -c "apretón de manos")" = 1 ]; then ok "las dos direcciones del mismo servidor van por una sola conexión cifrada (un solo apretón de manos)"
else ko "HTTPS no vuelve a usar la conexión (keep-alive):"; sed -n "/^C:.>https -i /,/^C:.>https -d /p" $S | sed 's/^/        /' | head -12; fi
mira "HTTPS: un POST con su cuerpo y una cabecera propia; una redirección en el mismo servidor y otra (301) a otro servidor" \
  "^metodo=POST$" "^largo=10$" "^x-uno=dos$" "^tipo=application/x-www-form-urlencoded$" "^cuerpo=hola=mundo$" \
  "^== $B/ultima.txt: 200, 50 octetos (redirecciones: 1)$" "^Hispa-Pentium 0.8: la red, cifrada.$"
[ "$(grep -c "^== $B/ultima.txt: 200, 50 octetos (redirecciones: 1)$" $S)" = 2 ] || ko "HTTPS: la redirección a otro servidor no llega"
mira "los certificados que no valen no se aceptan, y se dice por qué en español: de otro nombre y de una raíz que no conoce nadie" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado es de otro sitio, no de 10.0.2.2\.$" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado de 10.0.2.2 no lo firma ninguna autoridad conocida\.$"
mira "HTTPS /ESTADO: la fecha que usa (y de dónde), las autoridades, las conexiones y la memoria" \
  "^  Servicio  . . . . . . : TLS.BIN (BearSSL 0.6), TLS 1.2$" "^  Fecha . . . . . . . . : .* (del reloj del PC)$" "^  Autoridades raíz  . . : 21$" \
  "^  Conexiones  . . . . . : 0 abiertas, [5-9] apretones de manos$" "^  Memoria . . . . . . . : 37 K de conexiones$"
iguales=1
for f in GRANDE.BIN:grande.bin TROZOS.BIN:trozos.bin; do
  saca $R/e.img ${f%%:*} $R/x.bin && cmp -s $R/x.bin $R/srv/${f##*:} || { iguales=0; echo "        ${f%%:*} no es igual"; }
done
sano $R/e.img
if [ $iguales = 1 ] && grep -q "^$R/e.img: [0-9]* files" $P/fsck.txt && saca $R/e.img TLS.CCH $R/x.bin && [ "$(stat -c %s $R/x.bin)" = 576 ]; then
  ok "lo descargado por HTTPS es igual octeto a octeto que lo del servidor, el disco queda sano y la memoria de cadenas (TLS.CCH) está guardada"
else ko "lo descargado por HTTPS no es igual, el disco no queda sano o no se ha guardado TLS.CCH"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# otra vez (la cadena ya está en TLS.CCH), con el reloj del PC en el 2001 y la hora de la cabecera Date del servidor de aquí (TLS.INI)
printf 'hora=10.0.2.2:%s\r\n' "$(pu hora)" > $R/TLS.INI
mcopy -o -i $R/e.img $R/TLS.INI ::
cp $R/d.img $R/s.img
printf 'hora=10.0.2.2:1\r\n' > $R/TLS.INI
mcopy -o -i $R/s.img $R/TLS.INI ::
RELOJ=2001-01-01 DISCO=$R/e.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" "https /estado" ":pausa 2" > $R/h.sal 2>&1 &
H1=$!
RELOJ=2001-01-01 DISCO=$R/s.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" > $R/s.sal 2>&1 &
H2=$!
RELOJ=2050-06-01 DISCO=$R/d.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
  "descargar $B/ultima.txt -" ":pausa 4" > $R/c.sal 2>&1 &
H3=$!
wait $H1 $H2 $H3
tr -d '\r' < $R/h.sal > $R/h.txt
S=$R/h.txt
mira "con el reloj del PC en el 2001, la hora se pide por HTTP (la cabecera Date: avisado como riesgo) y la cadena ya comprobada sale de TLS.CCH" \
  "tls: el reloj del PC dice 01/01/2001: no puede ser (este TLS.BIN es del .*RIESGO" "tls: la hora, de la cabecera Date de 10.0.2.2: .* (UTC)$" \
  "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*la cadena ya comprobada (de memoria)$" "^version=0.8$" "^  Fecha . . . . . . . . : .* (de la cabecera Date: el reloj del PC no está en hora)$"
tr -d '\r' < $R/s.sal > $R/s.txt
S=$R/s.txt
mira "con el reloj del PC mal y sin servidor de la hora: no hay conexión, y se dice por qué" "tls: la hora: no se conecta con 10.0.2.2:1 " \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el reloj del equipo no está en hora y no se ha podido preguntar la hora a 10.0.2.2: sin la fecha no se pueden comprobar los certificados\.$"
tr -d '\r' < $R/c.sal > $R/c.txt
S=$R/c.txt
mira "con el reloj del PC en el 2050, el certificado ha caducado: no se acepta" "tls: la hora, del reloj del PC: 01/06/2050$" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el certificado de 10.0.2.2 ha caducado o aún no vale (con la fecha 01/06/2050, del reloj del PC); compruebe la fecha del equipo\.$"
# con internet: clicgest.es y hispados.es, frente a curl (y la hora de actualiza.hispados.es)
if curl -s -m 10 -o $R/clic.htm https://clicgest.es/ && curl -s -m 10 -o $R/hispa.htm https://hispados.es/ && [ -s $R/clic.htm ]; then
  cp $R/d.img $R/i.img
  ESCRIBE=1 RELOJ=2001-01-01 DISCO=$R/i.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 2" \
    "descargar https://clicgest.es/ clic.htm" ":pausa 8" "descargar https://hispados.es/ hispa.htm" ":pausa 8" > $R/i.sal 2>&1
  tr -d '\r' < $R/i.sal > $R/i.txt
  S=$R/i.txt
  mira "(con internet) la hora, de actualiza.hispados.es; https://clicgest.es/ y https://hispados.es/ con sus cadenas de Let's Encrypt (ISRG Root YE)" \
    "tls: la hora, de la cabecera Date de actualiza.hispados.es: " "tls: clicgest.es: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$" \
    "tls: hispados.es: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$"
  if saca $R/i.img CLIC.HTM $R/x.htm && curl -s -m 10 -o $R/clic.htm https://clicgest.es/ && cmp -s $R/x.htm $R/clic.htm \
     && saca $R/i.img HISPA.HTM $R/y.htm && curl -s -m 10 -o $R/hispa.htm https://hispados.es/ && cmp -s $R/y.htm $R/hispa.htm; then
    ok "(con internet) lo bajado de https://clicgest.es/ y https://hispados.es/ es igual octeto a octeto que lo que baja curl ($(stat -c %s $R/x.htm) y $(stat -c %s $R/y.htm) octetos)"
  else ko "(con internet) lo bajado de clicgest.es o hispados.es no es igual que con curl"; fi
else echo "  (esta máquina no llega a https://clicgest.es/: no se prueba contra internet)"; fi
else ko "no se pueden poner en marcha los servidores HTTPS de prueba o hacer el disco: $(tail -n 1 $R/srv.txt) $(tail -n 1 $R/disco.txt)"; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin HTTPS)"
fi   # HTTPS

# ------------------------------------- 14. Hispa-Pentium 0.8: apagar de verdad y la batería
# El servicio de ACPI, ACPI.BIN (acpi/pentium/: el núcleo de C de HSPACPI
# de Hispa-DOS, con ACPICA, como programa del anillo 3), y energia.cas. En
# QEMU, que tiene ACPI (PIIX4 con «pc», ICH9 con «q35») pero no batería, la
# SSDT de las pruebas de Hispa-DOS (tests/acpi/bateria-qemu.aml, con
# -acpitable) declara una batería que lee su nivel de la CMOS: 60h el nivel
# (de 0 a 100), 61h bit 0 con cable y bit 1 puesta, 62h la salud en %. La
# CMOS se escribe por el monitor de QEMU («o /b 0x70 0x60»), con la máquina
# parada (-S) antes de arrancar y luego en marcha. Apagar de verdad: QEMU
# termina (ACPI S5) sin pasar por su puerto de pruebas (el código 5).
if parte08 acpi; then
echo "== Hispa-Pentium 0.8: apagar de verdad y la batería (el servicio de ACPI, ACPI.BIN) =="
Y=$P/acpi
mkdir -p $Y $G
if bash $AQUI/../acpi/pentium/hacer.sh $Y/ACPI.BIN > $Y/hacer.txt 2>&1; then
  ok "acpi/pentium/hacer.sh: ACPI.BIN, el núcleo de HSPACPI (ACPICA) como programa del anillo 3 ($(stat -c %s $Y/ACPI.BIN) octetos)"
else ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Y/hacer.txt)"; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN
for u in ubloc:BLOC uconsola:CONSOLA uordenes:ORDENES uhola:HOLA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN
done
SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
# la CMOS de la batería: cmosq REGISTRO VALOR (órdenes del monitor para probar.sh)
cmosq() { echo ":monitor o /b 0x70 0x$1"; echo ":monitor o /b 0x71 0x$2"; }
mapfile -t PUESTA < <(cmosq 60 37; cmosq 61 02; cmosq 62 00)
# (con la máquina en marcha, parada un momento: que ni el núcleo, que lee
# su reloj de la CMOS, ni el servicio cambien el registro entre las dos)
mapfile -t ENCHUFA < <(echo ":monitor stop"; cmosq 61 03; echo ":monitor cont")
mapfile -t GASTADA < <(echo ":monitor stop"; cmosq 62 28; echo ":monitor cont")
# 1. el escritorio con la batería al 55 %, sin cable: la pila y su recuadro;
# luego enchufada (cargando) y gastada; y «Apagar» apaga (QEMU termina)
CAPTURAS=$G MODULOS=$Y/ACPI.BIN,$Y/BLOC.BIN ARRANQUE="escritorio acpicada=1" QEMU_MAS="-S $SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN \
  "${PUESTA[@]}" ":monitor cont" ":pausa 5" ":captura a1" ":raton 690 585" ":pausa 1.5" ":captura a2" \
  "${ENCHUFA[@]}" ":pausa 3" ":captura a3" ":clic 690 585" ":pausa 1" ":captura a4" ":raton 400 300" \
  "${GASTADA[@]}" ":pausa 3" ":raton 690 585" ":pausa 1.5" ":captura a5" ":raton 400 300" \
  ":clic 40 585" ":clic 80 176" ":pausa 2" ":escribe sin guardar" ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 5" ":captura a6" \
  ":tecla ret" ":pausa 4" > $Y/apagar.sal 2>&1
S=$Y/apagar.sal
mira "el núcleo lanza el servicio de ACPI al arrancar, y este lee las tablas de QEMU y la batería de la SSDT de prueba (_BIF, _BIX, _BST)" \
  "^acpi: el servicio de ACPI (ACPI.BIN) en marcha, la tarea [0-9]*" "^ACPI: SSDT .*HISPA  BATPRUEB" "^Batería 1: ._SB_.HBAT" \
  "^Lectura 1: 55 % (2750 de 5000 mAh), sin cable, descargando, quedan 165 min"
mira "y le da al núcleo lo que hace falta para apagar (PM1a_CNT de PIIX4 y \\_S5) y reiniciar" \
  "^ACPI: para apagar: PM1a_CNT 604h, PM1b_CNT 0h, ._S5 0, 0; SMI_CMD B2h, ACPI_ENABLE F1h"
miracap "la bandeja enseña la pila con lo que queda (verde) y el porcentaje, al lado del reloj" a1 negrita 700 570 712 580 "55%" \
  rgb 680 580 3FAE49 rgb 695 580 EAF3FD
miracap "con el puntero quieto medio segundo encima, el recuadro de ayuda: lo que queda y los minutos, la salud y los ciclos de carga" a2 \
  texto 300 500 790 560 "Batería: 55 % · quedan 2 h 45 min" texto 300 500 790 560 "Salud: 68 % (guarda 5.000 de 7.350 mAh de cuando era nueva)" \
  texto 300 500 790 560 "312 ciclos de carga" rgb 790 520 FFF8E1
miracap "al enchufar el cargador (con el recuadro abierto), se pone al día solo: «cargando» y el rayo en la pila" a3 \
  texto 300 500 790 560 "Enchufado · cargando: 55 %" rgb 690 578 FFD43B
miracap "un clic en la pila quita el recuadro" a4 notexto 300 500 790 560 "Enchufado"
miracap "con la batería gastada (la salud al 40 %), el aviso en terracota" a5 texto 300 480 790 560 "Salud: 40 % (guarda 5.000 de 12.500 mAh de cuando era nueva)" \
  texto 300 480 790 560 "La batería está gastada: dura menos de la mitad que nueva"
# (CORTES) ya no es «Hay un programa sin cerrar» y apagar otra vez: el cuadro
# «Estos programas no se han cerrado», con «Forzar el cierre» (Intro)
miracap "«Apagar» con el Bloc de notas sin guardar: el Bloc pregunta y el escritorio, «Estos programas no se han cerrado» (el Bloc espera una respuesta), con «Forzar el cierre»" a6 \
  negrita 100 100 700 500 "Estos programas no se han cerrado:" texto 100 100 700 500 "(espera una respuesta)" negrita 100 100 700 500 "Forzar el cierre"
mira "«Forzar el cierre»: se cierran los programas, se vacían los discos y el servicio apaga por ACPI (S5: _PTS, el modo ACPI, SLP_EN) y QEMU termina" \
  "^escritorio: se fuerza el cierre de la tarea [0-9]* (.*Bloc de notas" \
  "^escritorio: apagar" "^energía: apagar (S5) por el servicio de ACPI" "^ACPI: preparar S5 (._S5, _PTS, _SST): AE_OK" \
  "^ACPI: modo ACPI (SMI_CMD B2h <- F1h): AE_OK" "^\[código 0\]"
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S; then ok "ni una excepción del núcleo ni un error de ACPICA en toda la sesión"
else ko "hay excepciones o errores de ACPICA:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S | head -4 | sed 's/^/        /'; fi
# 2. reiniciar (con -no-reboot, QEMU termina al reiniciarse): con PIIX4 la
# FADT no trae registro de reinicio: el teclado
MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="$SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":clic 40 585" ":clic 360 545" \
  ":clic 340 327" ":pausa 3" > $Y/reiniciar.sal 2>&1
S=$Y/reiniciar.sal
mira "«Reiniciar»: el servicio avisa al controlador embebido (_REG 3, 0) y el núcleo reinicia (PIIX4: por el teclado)" \
  "^\(> \)\?escritorio: reiniciar" "^ACPI: el núcleo va a reiniciar el equipo" "^energía: reiniciar por el teclado (8042)" "^\[código 0\]"
# 3. con ICH9 (q35): apagar y reiniciar (aquí la FADT sí trae el registro de reinicio: CF9h)
for q in 275:apagar 340:reiniciar; do
  MAQUINA=q35 MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="$SSDT" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":clic 40 585" \
    ":clic 360 545" ":clic ${q%%:*} 327" ":pausa 3" > $Y/q35${q##*:}.sal 2>&1
done
S=$Y/q35apagar.sal
mira "ICH9 (QEMU q35): «Apagar» apaga por ACPI (SMI_CMD B2h <- 02h) y QEMU termina" "^ACPI: modo ACPI (SMI_CMD B2h <- 2h): AE_OK" "^\[código 0\]"
S=$Y/q35reiniciar.sal
mira "ICH9: «Reiniciar» por el registro de reinicio de la FADT (E/S CF9h <- 0Fh)" \
  "^ACPI: para reiniciar: el registro de la FADT (E/S CF9h) <- Fh" "^energía: reiniciar por el registro de la FADT" "^\[código 0\]"
# 4. sin ACPI (-machine acpi=off; -no-acpi ya no está en QEMU 10): el servicio lo dice, no hay pila y «Apagar» es lo de antes (en QEMU, su puerto de pruebas)
CAPTURAS=$G MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" QEMU_MAS="-machine acpi=off" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":captura a7" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 2" > $Y/sinacpi.sal 2>&1
S=$Y/sinacpi.sal
mira "sin ACPI: el servicio lo dice y «Apagar» hace lo de antes (en QEMU, el código 5 de su puerto de pruebas)" \
  "^ACPI: el equipo no tiene ACPI" "escritorio: apagar" "^energía: el equipo no se ha apagado" "^\[código 5\]"
miracap "sin ACPI no hay pila en la bandeja (solo el reloj)" a7 notexto 670 570 740 590 "%" nrgb 695 580 EAF3FD
# 5. un sobremesa (ACPI sin batería): ni pila; apagar, por ACPI
CAPTURAS=$G MODULOS=$Y/ACPI.BIN ARRANQUE="escritorio" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":captura a8" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 3" > $Y/sobremesa.sal 2>&1
S=$Y/sobremesa.sal
mira "un sobremesa (ACPI sin batería): el servicio lo dice, y apaga por ACPI" "^El equipo no declara baterías" "^energía: apagar (S5) por el servicio de ACPI" "^\[código 0\]"
miracap "y en la bandeja no hay pila" a8 notexto 670 570 740 590 "%"
# 6. las llamadas del servicio no son para cualquiera: el mismo ACPI.BIN,
# lanzado a mano desde el intérprete, no puede (y lo dice)
MODULOS=$Y/ACPI.BIN,$Y/ORDENES.BIN ARRANQUE="ordenes sinacpi" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 2" "acpi" ":pausa 3" "salir" apagar > $Y/mano.sal 2>&1
S=$Y/mano.sal
mira "un programa cualquiera no puede usar las llamadas del servicio (150 a 153): ACPI.BIN lanzado a mano lo dice y acaba" \
  "^ACPI.BIN: es el servicio de ACPI; lo lanza el núcleo al arrancar"
if ! grep -q "^acpi: el servicio" $S; then ok "«sinacpi» en la línea del cargador: el núcleo no lanza el servicio"
else ko "con «sinacpi» el núcleo lanza el servicio"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin apagar de verdad ni la batería)"
fi   # apagar de verdad y la batería

# una máquina: maquina NOMBRE "opciones de QEMU" PASOS... (:pausa S, :tecla
# K..., :texto N, :captura N, :clic X Y, :monitor ORDEN, :espera S REGEX:
# hasta que salga REGEX por el puerto serie, como mucho S segundos; :centro X Y:
# la flecha está ahí, al arrancar con otra medida que 800 x 600); lo que
# sale por el puerto serie, en $V/NOMBRE.sal (lo usan las partes 16 y 17)
maquina() {
  local n=$1 o=$2 p x y s rx=400 ry=300 sx sy dx dy
  shift 2
  rm -f $V/mon
  # shellcheck disable=SC2086
  qemu-system-i386 $o -display none -monitor unix:$V/mon,server,nowait -serial file:$V/$n.sal -rtc base=localtime > /dev/null 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $V/mon ] && break; sleep 0.1; done
  for p in "$@"; do
    case "$p" in
      ":pausa "*) sleep ${p:7} ;;
      ":centro "*) set -- ${p:8}; rx=$1; ry=$2 ;;
      ":tecla "*) for k in ${p:7}; do printf 'sendkey %s\n' $k | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.15; done ;;
      ":monitor "*) printf '%s\n' "${p:9}" | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.3 ;;
      ":captura "*) printf 'screendump %s\n' $G/${p:9}.ppm | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.6 ;;
      ":texto "*) printf 'pmemsave 0xb8000 4000 "%s"\n' $V/p.bin | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4
        python3 -I -c "
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25): print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())" $V/p.bin > $V/${p:7}.txt ;;
      ":espera "*) s=${p:8}; s=${s%% *}; x=${p#:espera $s }
        for ((i = 0; i < s * 2; i++)); do tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 2> /dev/null | grep -q -- "$x" && break; sleep 0.5; done ;;
      ":clic "*) set -- ${p:6}; dx=$(($1 - rx)); dy=$(($2 - ry))
        while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
          sx=$dx; [ $sx -gt 50 ] && sx=50; [ $sx -lt -50 ] && sx=-50
          sy=$dy; [ $sy -gt 50 ] && sy=50; [ $sy -lt -50 ] && sy=-50
          printf 'mouse_move %s %s\n' $sx $sy | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.05
          dx=$((dx - sx)); dy=$((dy - sy))
        done
        rx=$1; ry=$2; sleep 0.25
        printf 'mouse_button 1\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.2
        printf 'mouse_button 0\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4 ;;
    esac
  done
  printf 'quit\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1
  wait $QPID 2> /dev/null
  QPID=
  tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 > $V/$n.txt 2> /dev/null
  S=$V/$n.txt
}
# ------------------------------- 16. Hispa-Pentium 0.8: el CD y las actualizaciones por Internet
# El CD (cd.sh) en QEMU como lo pone VirtualBox, instalar desde él y, en el
# sistema instalado, actualizar por la red (el núcleo también) con un
# servidor HTTP de aquí (servidor_prueba.py con RAIZ) y un catálogo firmado
# con una clave de prueba (en el disco instalado se pone un PAQUETES.BIN
# hecho con ella, como en la parte 12).
if parte08 cd; then
echo "== Hispa-Pentium 0.8: el CD (VirtualBox) y las actualizaciones por Internet =="
V=$P/cd
mkdir -p $V/srv $V/raiz/pentium $G
export MTOOLS_SKIP_CHECK=1
if ! command -v grub-mkrescue > /dev/null || ! command -v xorriso > /dev/null; then
  echo "  (sin grub-mkrescue o xorriso no se prueba el CD)"
else
HPQ="python3 -I $AQUI/../tools/hpq.py"
# 1. el CD
if GUARDA_PAQ=$V/paq $AQUI/nucleo32/cd.sh $V/cd.iso > $V/cd.txt 2>&1 && xorriso -indev $V/cd.iso -report_el_torito plain > $V/torito.txt 2>&1 \
  && grep -q "^El Torito boot img : *1 *BIOS *y *none " $V/torito.txt && [ $(stat -c %s $V/cd.iso) -lt $((650 * 1048576)) ]; then
  ok "cd.sh hace el CD: una ISO de $(($(stat -c %s $V/cd.iso) / 1048576)) MB que arranca por la BIOS con El Torito sin emulación (vale en CD y en DVD), con GRUB"
else ko "cd.sh no hace bien el CD:"; tail -n 3 $V/cd.txt $V/torito.txt | sed 's/^/        /'; fi
VBOX_PC="-machine pc -m 256 -nic user,model=e1000 -device pci-ohci,id=o -device usb-tablet,bus=o.0"
VBOX_Q35="-machine q35 -m 256 -nic user,model=e1000"
# 2. arrancar sin instalar (la primera entrada), en un «pc» con IDE, USB OHCI (que no se toca) y la e1000
truncate -s 2G $V/ide.img
maquina vivo "$VBOX_PC -cdrom $V/cd.iso -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d" ":espera 40 ^escritorio: en marcha" ":espera 20 ^red: DHCP: IP" ":pausa 2" ":captura cd1"
# (TAILSCALE: sin «^» en la del DHCP: con más módulos, puede salir detrás del «> » de la consola)
mira "el CD en un «pc» como el de VirtualBox (IDE, USB OHCI, la e1000 82540EM): GRUB arranca solo la primera entrada, el escritorio sin instalar, con la red" \
  "escritorio: en marcha, 800 x 600" "red: tarjeta 8086:100E" "red: DHCP: IP 10\.0\.2\.15"
miracap "y el escritorio, desde la memoria (la unidad M:)" cd1 texto 20 50 110 70 "Mi equipo" negrita 30 570 80 590 Inicio
mira "(CD) y el núcleo lee el propio CD: el lector del IDE secundario maestro (el de VirtualBox) es D:, con su ISO 9660" \
  "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE)\.$" "cd: D: CD de datos: «HISPA_PENTI[A-Z_0-9]*», ISO 9660"
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción del núcleo (con el controlador OHCI y su tableta USB, que no se usan)"; else ko "hay excepciones:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
# 3. instalar desde el CD: en el «pc» (IDE) y en el «q35» (SATA: AHCI, con el CD en AHCI también)
truncate -s 2G $V/sata.img
INSTALA_CD=(":pausa 3" ":tecla down ret" ":espera 40 ^instalar: bienvenida" ":texto c1" ":tecla ret" ":pausa 2" ":tecla ret" ":pausa 2" ":texto c3" ":tecla ret" ":espera 120 ^instalar: terminado" ":texto c4" ":tecla ret" ":pausa 2")
maquina inside "$VBOX_PC -cdrom $V/cd.iso -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d -no-reboot" "${INSTALA_CD[@]}"
mira "instalar desde el CD en el «pc» (el disco IDE): INSTALAR sabe que es el CD (MEDIO.TXT) y acaba" "^instalar: desde el CD$" "^instalar: terminado$"
if grep -q "para que arranque de él, sin el CD\." $V/c1.txt && grep -q "Disco: *IDE *QEMU HARDDISK, 2048 MB" $V/c3.txt && grep -q "Saque ahora el CD (o el DVD) y pulse INTRO" $V/c4.txt; then
  ok "y lo dice por el CD: «sin el CD», el disco IDE de 2 GB, «Saque ahora el CD (o el DVD)»"
else ko "INSTALAR, desde el CD, no dice lo suyo:"; cat $V/c3.txt $V/c4.txt | grep -v '^|$' | sed 's/^/        /' | head -20; fi
maquina insq35 "$VBOX_Q35 -cdrom $V/cd.iso -drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0 -boot d -no-reboot" "${INSTALA_CD[@]}"
mira "y en el «q35» (SATA, AHCI; el CD, en AHCI): el disco SATA, instalado" "^instalar: desde el CD$" "^instalar: terminado$"
grep -q "Disco: *SATA *QEMU HARDDISK, 2048 MB" $V/c3.txt || ko "en el q35, el disco no sale como SATA: $(grep Disco: $V/c3.txt)"
# 4. arrancar del disco (con el CD aún puesto: «-boot c»): el asistente del primer arranque
for m in ide:pc sata:q35; do
  if [ ${m%%:*} = ide ]; then o="$VBOX_PC -drive file=$V/ide.img,format=raw,if=ide,index=0"; else o="$VBOX_Q35 -drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0"; fi
  maquina arr${m%%:*} "$o -cdrom $V/cd.iso -boot c" ":espera 50 lanzo .HISPA.ASISTE.BIN"
  mira "el disco instalado desde el CD arranca solo (el «${m##*:}»): el escritorio y el asistente del primer arranque" "escritorio: en marcha" "lanzo .HISPA.ASISTE.BIN"
done
# 5. actualizar por Internet, en el instalado del q35: sus ajustes (sin
# asistente), de dónde (ORIGEN.TXT: el servidor de aquí) y PAQUETES con la
# clave de prueba
$HPQ clave $V/prueba.key > /dev/null && $HPQ publica $V/prueba.key $V/clave.cas > /dev/null
cp $AQUI/nucleo32/upaquete.cas $V/upaquete.cas
python3 -I $AQUI/ccastel.py $V/upaquete.cas -o $V/upaquete.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $V/upaquete.asm -o $V/PAQUETES.BIN
python3 -I $AQUI/nucleo32/servidor_prueba.py $V/srv $V/raiz > $V/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $V/srv/puerto ] && break; sleep 0.1; done
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $V/HISPA.INI
printf 'http://10.0.2.2:%s/pentium/\r\n' "$(cat $V/srv/puerto)" > $V/ORIGEN.TXT
mcopy -o -i $V/sata.img@@1M $V/HISPA.INI $V/PAQUETES.BIN ::HISPA/ && mcopy -o -i $V/sata.img@@1M $V/ORIGEN.TXT ::HISPA/PAQUETES/
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/NUCLEO08.BIN
# el servidor: los paquetes del CD, con un «base» más nuevo (sus mismos
# archivos; con NUCLEO, otro núcleo), el catálogo y su firma
sube_base() {   # VERSION [NÚCLEO]
  rm -f $V/raiz/pentium/*
  cp $V/paq/*.HPQ $V/raiz/pentium/
  python3 -I - $V/raiz/pentium "$@" <<'PY'
import os, sys
sys.path.insert(0, os.path.join(os.environ['AQUI'], '..', 'tools'))
import hpq
c, v = sys.argv[1], sys.argv[2]
campos, archivos, largo, datos = hpq.lee_paquete(os.path.join(c, 'BASE.HPQ'))
lista, p = [], largo
for d, t, s in archivos:
    b = datos[p:p + t]
    p += t
    if d == '\\HISPA\\NUCLEO.BIN' and len(sys.argv) > 3:
        b = open(sys.argv[3], 'rb').read()
    lista.append((d, b))
hpq.haz_paquete(os.path.join(c, 'BASE.HPQ'), 'base', v, campos['descripcion'], campos['necesita'], campos['choca'], lista)
PY
  $HPQ catalogo $V/raiz/pentium --clave $V/prueba.key > /dev/null
}
AQUI=$AQUI sube_base 0.9
DISCO="-drive file=$V/sata.img,format=raw,if=none,id=d -device ide-hd,drive=d,bus=ide.0 -boot c"
# (PROGRAMAS) «Verlas» abre la ventana de PROGRAMA.BIN en «Pendientes»;
# «Actualizar todo» es el segundo botón de la derecha; «Reiniciar ahora», Intro
maquina act1 "$VBOX_Q35 $DISCO" ":espera 60 aviso en la bandeja: Hay una actualización" ":pausa 1" ":captura u1" ":clic 478 526" ":pausa 8" ":captura u2" \
  ":clic 720 403" ":pausa 12" ":captura u3" ":tecla ret" ":espera 90 aviso en la bandeja: El núcleo de Hispa-Pentium El núcleo nuevo ha arrancado bien" ":pausa 1" ":captura u4"
mira "al arrancar, con red, ACTUALIZ /BANDEJA mira solo si hay actualizaciones (el catálogo firmado del servidor) y lo dice; tras actualizar el núcleo y reiniciar, que el nuevo ha arrancado bien" \
  "^actualiz: aviso en la bandeja: Hay una actualización\.$" "^reiniciar$" "^actualiz: aviso en la bandeja: El núcleo de Hispa-Pentium El núcleo nuevo ha arrancado bien\.$"
miracap "el aviso de la bandeja: una ventanita abajo a la derecha, «Hay una actualización», con «Verlas» y «Ahora no»" u1 negrita 400 460 480 480 "Hay una actualización." negrita 420 510 520 530 Verlas
[ -n "${GUARDA_CAPTURAS:-}" ] && mkdir -p "$GUARDA_CAPTURAS" && cp $G/u[23].ppm "$GUARDA_CAPTURAS/"
miracap "«Verlas» abre «Programas y actualizaciones» en «Pendientes (1)»: base, de la $VERSION_BASE a la 0.9, y el catálogo de Internet con su firma buena" u2 \
  negrita 10 120 150 140 "Pendientes (1)" negrita 40 190 120 230 base texto 110 190 200 230 "$VERSION_BASE" texto 195 190 245 230 "0.9" texto 300 88 790 110 "Paquetes de http://10.0.2.2:" \
  texto 300 88 790 110 "(firma buena)"
miracap "«Actualizar todo»: con el núcleo nuevo, ofrece «Reiniciar ahora» o «Más tarde»" u3 negrita 150 200 660 400 "Reiniciar ahora" negrita 150 200 660 400 "Más tarde" \
  texto 140 200 660 320 "Se ha puesto un núcleo nuevo: se usa al reiniciar"
miracap "tras reiniciar, el aviso: el núcleo nuevo ha arrancado bien" u4 texto 400 480 600 500 "El núcleo nuevo ha arrancado bien."
mcopy -n -i $V/sata.img@@1M ::HISPA/GRUBENV $V/grubenv1 > /dev/null 2>&1; mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.ANT $V/ant1 > /dev/null 2>&1
mcopy -n -i $V/sata.img@@1M ::HISPA/PAQUETES/BASE.PQI $V/base1.pqi > /dev/null 2>&1
if head -c 33 $V/grubenv1 | grep -q '^nuevo=0$' && cmp -s $V/ant1 $V/NUCLEO08.BIN && grep -q '^version=0\.9' $V/base1.pqi && grep -q '^pentium/BASE\.HPQ$' $V/srv/pedidos && sano $V/sata.img 2048; then
  ok "y en el disco: base 0.9 apuntado, el núcleo de antes en NUCLEO.ANT y GRUBENV a 0 (el nuevo vale); el paquete, traído del servidor"
else ko "la actualización por Internet no deja bien el disco:"; head -c 40 $V/grubenv1 | sed 's/^/        /'; fi
# 6. un núcleo que se cuelga (multiboot, «hlt» para siempre): al
# reiniciar a mano, GRUB vuelve solo al de antes, y el sistema lo dice
printf 'bits 32\norg 0x100000\ncab: dd 0x1BADB002, 0x00010000, -(0x1BADB002 + 0x00010000), cab, cab, fin, fin, ent\nent: cli\n.l: hlt\njmp .l\nfin:\n' > $V/colgado.asm
nasm -f bin $V/colgado.asm -o $V/COLGADO.BIN
AQUI=$AQUI sube_base 0.9.1 $V/COLGADO.BIN
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/NUCLEO09.BIN
maquina act2 "$VBOX_Q35 $DISCO" ":espera 60 aviso en la bandeja: Hay una actualización" ":pausa 1" ":clic 478 526" ":pausa 8" ":clic 720 403" ":pausa 12" ":tecla ret" \
  ":espera 30 ^reiniciar$" ":pausa 25" ":monitor system_reset" ":pausa 4" ":texto g5" ":espera 90 aviso en la bandeja: El núcleo de Hispa-Pentium El nuevo no" ":pausa 1" ":captura u6"
mira "con un núcleo nuevo que se cuelga, al reiniciar a mano GRUB arranca solo el de antes, y el sistema lo dice" \
  "^actualiz: aviso en la bandeja: El núcleo de Hispa-Pentium El nuevo no llegó a arrancar: se usa el de antes\.$"
if grep -q '\*Hispa-Pentium con el nucleo de antes (si el nuevo no arranca)' $V/g5.txt; then ok "GRUB, en ese arranque: la entrada del núcleo de antes, elegida sola"
else ko "GRUB no elige solo el núcleo de antes:"; grep -v '^|$' $V/g5.txt | sed 's/^/        /' | head -8; fi
mcopy -n -i $V/sata.img@@1M ::HISPA/GRUBENV $V/grubenv2 > /dev/null 2>&1; mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.ANT $V/ant2 > /dev/null 2>&1
mcopy -n -i $V/sata.img@@1M ::HISPA/NUCLEO.BIN $V/nuc2 > /dev/null 2>&1
if head -c 33 $V/grubenv2 | grep -q '^nuevo=4$' && cmp -s $V/ant2 $V/NUCLEO09.BIN && cmp -s $V/nuc2 $V/COLGADO.BIN && sano $V/sata.img 2048; then
  ok "y en el disco: GRUBENV con nuevo=4 (se sigue arrancando el de antes), NUCLEO.ANT el que valía y el disco sano"
else ko "tras el núcleo colgado, el disco no queda bien:"; head -c 40 $V/grubenv2 | sed 's/^/        /'; fi
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el CD)"
fi   # el CD

# ------------------------------- 17. Hispa-Pentium 0.8: la medida de la pantalla
# Configuración > Cambiar...: con la VBE de Bochs, en el momento (y a prueba
# 15 s); sin ella (cirrus, como un PC de verdad), guardada para GRUB
# (\HISPA\PANTALLA.ENV) y a prueba al arrancar.
if parte08 pantalla; then
echo "== Hispa-Pentium 0.8: la medida de la pantalla (Configuración > Cambiar...: en el momento con la VBE de Bochs; al arrancar, con GRUB) =="
V=$P/pan
mkdir -p $V $G
export MTOOLS_SKIP_CHECK=1
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null 2>&1 && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN
# 1. en QEMU con -kernel (el modo lo pone el núcleo): el Bloc de notas
# maximizado; Configuración > Cambiar...; 1024 x 768 a prueba y «Mantener»;
# luego 1280 x 800 a prueba, sin tocar nada: a los 15 s vuelve sola
CAPTURAS=$G MEMORIA=128 DISCO=$P/d16.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 110" ":pausa 2" ":clic 553 76" ":pausa 1" \
  ":clic 40 585" ":clic 300 238" ":raton 700 530" ":captura m0" ":clic 336 230" ":pausa 1" ":captura m1" ":clic 565 229" ":clic 229 441" ":pausa 2" ":captura m2" ":clic 431 418" ":pausa 1" ":captura m3" \
  ":clic 495 347" ":clic 341 525" ":pausa 2" ":captura m4" ":pausa 17" ":captura m5" > $V/marcha.sal 2>&1
S=$V/marcha.sal
mira "con la VBE de Bochs (QEMU -vga std), «Probar...» pone la medida en el momento; «Mantener» la deja; sin tocar nada, a los 15 s vuelve sola la de antes" \
  "^pantalla: 1024x768 con 32 bits por punto (cambiado en marcha, por la VBE de Bochs)" "^escritorio: la pantalla, a 1024 x 768 (en marcha)" \
  "^escritorio: Se queda a 1024 x 768 (hasta que se apague)\." "^escritorio: la pantalla, a 1280 x 800 (en marcha)" \
  "^escritorio: no se ha dado por buena en 15 s" "^escritorio: Ha vuelto la de antes\."
miracap "Configuración: la medida de la pantalla arriba y el icono «Pantalla» (PANEL)" m0 texto 190 135 205 145 "Pantalla:  800 x 600, 32 bits por punto" negrita 300 245 310 255 "Pantalla"
miracap "la lista: los modos que da la tarjeta y caben en la pantalla (su EDID: 1280 x 800, recomendada), el de ahora elegido, «Probar...» y «Cerrar»" m1 \
  negrita 200 120 208 126 "La medida de la pantalla" texto 200 140 208 146 "Ahora: 800 x 600, 32 bits por punto." negrita 200 218 210 224 "800 x 600" \
  negrita 520 218 530 224 "1024 x 768" negrita 350 250 360 258 "1280 x 800 *" texto 160 283 168 291 "* La de la pantalla (recomendada): 1280 x 800." notexto 0 0 800 600 "1920 x 1080" \
  negrita 188 430 198 436 "Probar..." negrita 565 430 575 436 Cerrar rgb 170 228 5CC45E 40
miracap "a prueba: la pantalla ya a 1024 x 768, «¿Se ve bien?» con la cuenta atrás, y el Bloc de notas maximizado, a la medida nueva" m2 tam 1024 768 \
  negrita 334 305 342 313 "¿Se ve bien la pantalla?" texto 334 327 342 335 "Está a 1024 x 768." negrita 393 405 405 415 Mantener negrita 571 405 583 415 Volver \
  negrita 24 2 36 10 "NOTAS.TXT - Bloc de notas" rgb 1000 719 D7E5F8 10 texto 4 708 14 716 "NOTAS.TXT" negrita 30 742 46 752 Inicio
miracap "«Mantener»: la lista otra vez, con la de ahora (1024 x 768) y lo que ha pasado" m3 tam 1024 768 texto 312 223 320 231 "Ahora: 1024 x 768, 32 bits por punto." \
  texto 272 383 280 391 "Se queda a 1024 x 768 (hasta que se apague)."
miracap "1280 x 800 a prueba" m4 tam 1280 800
miracap "y a los 15 s, sin tocar nada, la de antes (1024 x 768): «Ha vuelto la de antes.»" m5 tam 1024 768 texto 272 383 280 391 "Ha vuelto la de antes."
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo al cambiar la pantalla en marcha"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
# 2. el CD, con el modo que ha puesto GRUB (por la VBE de la BIOS), en un
# «pc» como el de VirtualBox: con -vga std (como VBoxVGA) y con -vga vmware
# (la VMSVGA, 15AD:0405: la memoria, en su segunda zona PCI)
CD=$P/cd/cd.iso
[ -f $CD ] || { CD=$V/cd.iso; $AQUI/nucleo32/cd.sh $CD > $V/cd.txt 2>&1; }
VBOX_PC="-machine pc -m 256 -nic user,model=e1000 -device pci-ohci,id=o -device usb-tablet,bus=o.0"
truncate -s 2G $V/ide.img
for vga in std vmware; do
  # (EDID) con -vga std, la lista llega hasta lo que dice su EDID (1280 x 800, la quinta); con vmware, sin EDID, la sexta
  if [ $vga = std ]; then x=403; else x=565; fi
  maquina cd$vga "$VBOX_PC -vga $vga -cdrom $CD -drive file=$V/ide.img,format=raw,if=ide,index=0 -boot d" ":espera 40 ^escritorio: en marcha" ":pausa 3" \
    ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 1" ":captura cd$vga" ":clic $x 263" ":clic 229 441" ":pausa 2" ":captura cd${vga}2" ":clic 559 434" ":pausa 1"
  mira "el CD en un «pc» como VirtualBox con -vga $vga: el modo de GRUB (800 x 600) se cambia en marcha (1280 x 800) y se mantiene" \
    "escritorio: en marcha, 800 x 600 con 32 bits por punto (el modo lo ha dejado puesto el cargador)" "^pantalla: 1280x800 con 32 bits por punto (cambiado en marcha" \
    "^escritorio: Se queda a 1280 x 800 (hasta que se apague)\."
  miracap "y se ve: a 1280 x 800, con «¿Se ve bien la pantalla?» (-vga $vga)" cd${vga}2 tam 1280 800 negrita 462 321 470 329 "¿Se ve bien la pantalla?" negrita 30 770 46 780 Inicio
done
# 3. instalado desde el CD, con una tarjeta sin la VBE de Bochs (-vga
# cirrus: como un PC de verdad): «Guardar y reiniciar...»
truncate -s 2G $V/inst.img
maquina inst "$VBOX_PC -cdrom $CD -drive file=$V/inst.img,format=raw,if=ide,index=0 -boot d -no-reboot" ":pausa 3" ":tecla down ret" ":espera 40 ^instalar: bienvenida" \
  ":tecla ret" ":pausa 2" ":tecla ret" ":pausa 2" ":tecla ret" ":espera 120 ^instalar: terminado" ":tecla ret" ":pausa 2"
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $V/HISPA.INI
mcopy -o -i $V/inst.img@@1M $V/HISPA.INI ::HISPA/
cp $V/inst.img $V/inst0.img
# (EDID) cirrus no da EDID: la nativa (1024 x 768), como si se hubiera
# sabido otro día arrancando «a la medida»; sin ella, nada mayor que la de ahora
python3 -I -c "import sys; t='# GRUB Environment Block\npantalla=\nantes=\nprueba=0\nnativa=1024x768\n'; open(sys.argv[1],'w').write(t+'#'*(1024-len(t)))" $V/envn
mcopy -o -i $V/inst.img@@1M $V/envn ::HISPA/PANTALLA.ENV
CIRRUS="$VBOX_PC -vga cirrus -drive file=$V/inst.img,format=raw,if=ide,index=0"
maquina pc1 "$CIRRUS -no-reboot" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 1" ":captura pc1" \
  ":clic 565 229" ":clic 274 441" ":pausa 1" ":captura pc2" ":clic 314 336" ":pausa 6"
mira "sin la VBE de Bochs (cirrus), instalado: la medida se guarda en \\HISPA\\PANTALLA.ENV (a prueba), GRUB.CFG la lee, y «Reiniciar ahora» reinicia" \
  "^escritorio: GRUB.CFG lee ahora PANTALLA.ENV" "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=1" "^escritorio: reiniciar"
miracap "la lista, sin cambio en marcha: «se pone al arrancar», «A la medida» y «Guardar y reiniciar...»" pc1 texto 160 167 168 175 "Esta tarjeta no deja cambiarla en marcha: se guarda y" \
  negrita 190 252 200 260 "A la medida" negrita 512 216 522 224 "1024 x 768 *" notexto 0 0 800 600 "1280 x 800" negrita 180 428 190 436 "Guardar y reiniciar..."
miracap "«Al arrancar otra vez, a 1024 x 768», con «Reiniciar ahora» y «Más tarde»" pc2 negrita 224 216 234 224 "Al arrancar otra vez, a 1024 x 768." \
  negrita 248 324 258 332 "Reiniciar ahora" negrita 448 324 458 332 "Más tarde"
mcopy -n -i $V/inst.img@@1M ::HISPA/GRUB.CFG $V/grub.cfg; mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env1
if [ $(stat -c %s $V/env1) = 1024 ] && head -c 25 $V/env1 | grep -q '^# GRUB Environment Block' && grep -q '^  load_env -f /HISPA/PANTALLA.ENV pantalla antes prueba$' $V/grub.cfg \
  && grep -q 'set modo="${pantalla}x32,${pantalla},${modo}"' $V/grub.cfg && sano $V/inst.img 2048; then
  ok "PANTALLA.ENV es un bloque de GRUB de 1024 octetos (GRUB escribe en él: save_env), GRUB.CFG lo lee y el disco está sano"
else ko "PANTALLA.ENV o GRUB.CFG no quedan bien:"; head -c 80 $V/env1 | sed 's/^/        /'; fi
# al arrancar: GRUB pone 1024 x 768 (y la apunta «probándose»); «Mantener»
maquina pc2 "$CIRRUS -no-reboot" ":espera 40 está a prueba" ":pausa 2" ":captura pc3" ":centro 512 384" ":clic 431 418" ":pausa 2"
mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env2
mira "al arrancar, GRUB la pone (1024 x 768) y el escritorio pregunta; «Mantener» la deja para siempre" \
  "escritorio: en marcha, 1024 x 768 con [0-9]* bits por punto (el modo lo ha dejado puesto el cargador)" "pantalla: está a prueba" \
  "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=0" "^escritorio: Se queda a 1024 x 768\."
miracap "«¿Se ve bien la pantalla?», a 1024 x 768, con la cuenta atrás «vuelve la de antes»" pc3 negrita 334 305 342 313 "¿Se ve bien la pantalla?" \
  texto 334 347 342 355 "Si no la da por buena, vuelve la de antes en 1"
if head -c 100 $V/env2 | grep -q '^prueba=0$' && head -c 100 $V/env2 | grep -q '^pantalla=1024x768$'; then ok "y PANTALLA.ENV: pantalla=1024x768, prueba=0"
else ko "PANTALLA.ENV no queda bien tras «Mantener»:"; head -c 70 $V/env2 | sed 's/^/        /'; fi
# otra, 1024 x 600: se reinicia y, en la prueba, se reinicia otra vez a lo
# bruto (como con la pantalla a oscuras): GRUB vuelve sola a 1024 x 768
maquina pc3 "$CIRRUS -no-reboot" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":centro 512 384" ":clic 40 753" ":clic 300 406" ":clic 448 314" ":pausa 1" \
  ":clic 515 313" ":clic 386 525" ":pausa 1" ":clic 426 420" ":pausa 6"
mira "desde 1024 x 768, 1024 x 600 guardada a prueba (la de antes, apuntada)" "^escritorio: .*PANTALLA.ENV: pantalla=1024x600 antes=1024x768 prueba=1" "^escritorio: reiniciar"
maquina pc4 "$CIRRUS" ":espera 40 está a prueba" ":pausa 2" ":monitor system_reset" ":pausa 3" ":espera 40 en marcha, 1024 x 768" ":pausa 4"
mcopy -n -i $V/inst.img@@1M ::HISPA/PANTALLA.ENV $V/env3
mira "si se reinicia sin darla por buena, GRUB vuelve solo a la de antes (1024 x 768), sin preguntar" "escritorio: en marcha, 1024 x 600" "pantalla: está a prueba" \
  "escritorio: en marcha, 1024 x 768"
if [ $(grep -c "pantalla: está a prueba" $S) = 1 ] && head -c 100 $V/env3 | grep -q '^pantalla=1024x768$' && head -c 100 $V/env3 | grep -q '^antes=$' && head -c 100 $V/env3 | grep -q '^prueba=0$' && sano $V/inst.img 2048; then
  ok "y PANTALLA.ENV lo dice (GRUB lo ha escrito con save_env): pantalla=1024x768, antes vacío, prueba=0; el disco, sano"
else ko "tras reiniciar sin darla por buena, PANTALLA.ENV no queda bien:"; head -c 70 $V/env3 | sed 's/^/        /'; fi
# 4. instalado, con la VBE de Bochs (-vga std, como VirtualBox): «Mantener»
# la guarda para los próximos arranques; y una medida que la BIOS no da
# (1200 x 800; con -vga vmware, la VMSVGA: GRUB no tiene controlador
# para ella y va por la VBE de la BIOS, que no la da) la pone el núcleo al arrancar
cp $V/inst0.img $V/inst.img
STD="$VBOX_PC -vga std -drive file=$V/inst.img,format=raw,if=ide,index=0"
maquina vb1 "$STD" ":espera 40 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 1" ":clic 565 229" ":clic 229 441" ":pausa 2" ":clic 431 418" ":pausa 2"
mira "instalado, con la VBE de Bochs: «Mantener» la guarda también para los próximos arranques" "^escritorio: .*PANTALLA.ENV: pantalla=1024x768 antes= prueba=0" \
  "^escritorio: Se queda a 1024 x 768, también al arrancar\."
maquina vb2 "$STD" ":espera 40 ^escritorio: en marcha" ":pausa 3"
mira "y al arrancar otra vez, GRUB ya la pone (1024 x 768)" "escritorio: en marcha, 1024 x 768 con 32 bits por punto (el modo lo ha dejado puesto el cargador)"
printf '# GRUB Environment Block\npantalla=1200x800\nantes=\nprueba=0\n' > $V/env4
head -c 1024 /dev/zero | tr '\0' '#' | dd of=$V/env4 bs=1 seek=$(stat -c %s $V/env4) count=$((1024 - $(stat -c %s $V/env4))) status=none 2> /dev/null
mcopy -o -i $V/inst.img@@1M $V/env4 ::HISPA/PANTALLA.ENV
maquina vb3 "${STD/-vga std/-vga vmware}" ":espera 40 la pantalla, a 1200" ":pausa 2" ":captura vb3"
mira "con la VMSVGA, 1200 x 800 (que su BIOS no da por VBE: GRUB pone otra) la pone el núcleo al arrancar, por la VBE de Bochs" \
  "escritorio: en marcha, 800 x 600" "^escritorio: la pantalla, a 1200 x 800 (en marcha)"
miracap "y el escritorio, a 1200 x 800" vb3 tam 1200 800 negrita 30 770 46 780 Inicio
# 5. (EDID) lo que dice la pantalla de sí misma: con QEMU -device VGA y un
# EDID de 1024 x 600 (un netbook), uno de 1366 x 768 y sin EDID; el
# Administrador de dispositivos dice el monitor ((0.8.12) con «*»: ahora se abre
# con lo que funciona recogido)
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/udisposi.cas -o $V/udisposi.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $V/udisposi.asm -o $V/DISPOSIT.BIN
cp $P/d16.img $V/de.img; mcopy -o -i $V/de.img $V/DISPOSIT.BIN ::DISPOSIT.BIN
for e in 1024:600 1366:768 no; do
  if [ $e = no ]; then o="edid=off"; else o="xres=${e%:*},yres=${e#*:}"; fi
  CAPTURAS=$G MEMORIA=128 DISCO=$V/de.img ARRANQUE=escritorio MODULOS= VGA=none QEMU_MAS="-device VGA,$o" $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" \
    ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 1" ":captura ed${e%:*}" ":clic 594 441" ":clic 336 422" ":pausa 3" ":tecla shift-bracket_right" ":pausa 1" ":captura dv${e%:*}" > $V/edid$e.sal 2>&1
done
S=$V/edid1024:600.sal
mira "con un EDID de 1024 x 600 (un netbook), el núcleo lo lee y lo apunta en el registro del arranque" "pantalla: EDID: «QEMU Monitor», hecha para 1024 x 600 (la tarjeta de QEMU)"
miracap "y la lista ofrece solo lo que cabe en la pantalla: 800 x 600 y 1024 x 600, la recomendada" ed1024 negrita 200 218 210 224 "800 x 600" negrita 355 218 365 224 "1024 x 600 *" \
  texto 160 249 168 257 "* La de la pantalla (recomendada): 1024 x 600." notexto 0 0 800 600 "1024 x 768"
miracap "el Administrador de dispositivos dice el monitor en Pantalla, colgado de la tarjeta (lo lleva edid.cas)" dv1024 texto 520 178 560 194 "edid.cas"
S=$V/edid1366:768.sal
mira "con un EDID de 1366 x 768" "pantalla: EDID: «QEMU Monitor», hecha para 1366 x 768"
miracap "nada mayor que 1366 x 768 (y 1366 no, que la VBE de QEMU no la da: la deja en 1360)" ed1366 negrita 0 0 800 600 "1280 x 720" texto 160 200 168 400 "* La de la pantalla (recomendada): 1366 x 768." \
  notexto 0 0 800 600 "1440 x 900" notexto 0 0 800 600 "1366 x 768 *"
S=$V/edidno.sal
mira "sin EDID, lo dice" "la pantalla no dice su medida"
miracap "y en una máquina virtual (su pantalla es una ventana) la lista sigue sin tope" edno negrita 0 0 800 600 "1920 x 1080"
# 6. (EDID) la memoria USB (usb.sh, como root), en un PC sin la VBE de
# Bochs ni EDID (-vga cirrus, como el HP Mini): se guarda en X:\PANTALLA.ENV
# y la usa la primera entrada de su menú; lo que la BIOS no da se dice
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && command -v parted > /dev/null; then
  $AQUI/nucleo32/usb.sh $V/usb.img > $V/usb.txt 2>&1
  USBQ="-machine pc -m 256 -vga cirrus -nic none -device qemu-xhci,id=u -drive file=$V/usb.img,format=raw,if=none,id=m -device usb-storage,bus=u.0,drive=m,bootindex=1"
  maquina us1 "$USBQ -no-reboot" ":espera 60 ^escritorio: en marcha" ":pausa 3" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 1" ":captura us1" \
    ":clic 403 229" ":clic 274 441" ":pausa 1" ":clic 314 336" ":pausa 6"
  mira "desde la memoria USB (sin EDID): nada mayor que la de ahora, «A la medida», y se guarda en X:\\PANTALLA.ENV" \
    "la pantalla no dice su medida" "^escritorio: \\\\PANTALLA.ENV: pantalla=auto antes= prueba=1" "^escritorio: reiniciar"
  miracap "«No se sabe su medida: nada mayor que la de ahora.»" us1 texto 160 249 168 257 "No se sabe su medida: nada mayor que la de ahora." negrita 350 218 370 226 "A la medida"
  maquina us2 "$USBQ -no-reboot" ":espera 60 está a prueba" ":pausa 2" ":clic 431 418" ":pausa 2"
  mira "y al arrancar, GRUB la pone (a la medida) desde la primera entrada y pregunta" "PANTALLA.ENV pide «auto» (prueba=2)" "pantalla: está a prueba"
  python3 -I -c "import sys; t='# GRUB Environment Block\npantalla=1600x900\nantes=\nprueba=0\nnativa=\n'; open(sys.argv[1],'w').write(t+'#'*(1024-len(t)))" $V/envu
  mcopy -o -i $V/usb.img@@1M $V/envu ::PANTALLA.ENV
  maquina us3 "$USBQ -no-reboot" ":espera 60 no permite" ":pausa 3" ":captura us3"
  mcopy -n -i $V/usb.img@@1M ::ARRANQUE.TXT $V/arranque.txt; mcopy -n -i $V/usb.img@@1M ::PANTALLA.ENV $V/envu2
  mira "si la BIOS no da la pedida (1600 x 900 en cirrus), GRUB pone otra y el núcleo lo dice" "Este equipo no permite 1600 x 900: se queda en 800 x 600\."
  miracap "con un aviso" us3 texto 0 0 800 600 "Este equipo no permite 1600 x 900: se queda en 800 x 600."
  if tr -d '\r' < $V/arranque.txt | iconv -f CP437 -t UTF-8 | grep -q "Este equipo no permite 1600 x 900" && head -c 60 $V/envu2 | grep -q '^pantalla=800x600$'; then
    ok "y queda en X:\\ARRANQUE.TXT (para mandarlo), y PANTALLA.ENV pasa a la que hay"
  else ko "ARRANQUE.TXT o PANTALLA.ENV no lo dicen:"; head -c 60 $V/envu2 | sed 's/^/        /'; fi
else
  echo "  (sin ser root no se hace la memoria USB: sin la parte de la memoria USB)"
fi
if ! cat $V/pc*.txt $V/vb*.txt $V/cd*.txt | grep -q "excepción\|\*\*\*"; then ok "en todos esos arranques, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $V/pc*.txt $V/vb*.txt $V/cd*.txt | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la medida de la pantalla)"
fi   # la pantalla
# ------------------------------- 19. Hispa-Pentium 0.8.3: el Administrador de dispositivos
# (dispositivos) DISPOSIT.BIN (udisposi.cas) y la llamada 155 del núcleo
# (dispositivos.cas): todo lo del PCI, lo USB, lo de ACPI (que apunta
# ACPI.BIN con la 156), los discos, lo de siempre y el procesador, con el
# controlador que lleva cada cosa. En un «pc» (PIIX) con la e1000, EHCI con
# teclado, ratón, tableta (sin controlador: no es de arranque) y una memoria,
# un disco SATA (AHCI) y la batería de mentira de la parte 14; y en un «q35»
# (ICH9) con la RTL8139, xHCI, el disco en el AHCI de la placa y la batería.
if parte08 dispositivos; then
echo "== Hispa-Pentium 0.8.3: el Administrador de dispositivos (DISPOSIT.BIN y la llamada 155) =="
Y=$P/disp
mkdir -p $Y $G
bash $AQUI/../acpi/pentium/hacer.sh $Y/ACPI.BIN > $Y/hacer.txt 2>&1 || ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Y/hacer.txt)"
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN
for u in udisposi:DISPOSIT uequipo:EQUIPO uordenes:ORDENES ured:RED; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN \
    || ko "${u%%:*}.cas no compila"
done
SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
cmosq() { echo ":monitor o /b 0x70 0x$1"; echo ":monitor o /b 0x71 0x$2"; }
mapfile -t PUESTA < <(cmosq 60 37; cmosq 61 03; cmosq 62 00)
rm -f $Y/mem.img $Y/sata.img
mkfs.fat -C -F 16 -n MEMORIA $Y/mem.img 32768 > /dev/null
mkfs.fat -C -F 16 -n SATA $Y/sata.img 65536 > /dev/null
# 1. «pc», en el escritorio: Mi equipo > Propiedades; guardar el informe en la
# memoria (X:); las propiedades (General, Controlador, Recursos); copiar el
# informe; e Inicio > Configuración > «Administrador de dispositivos...»
USBS="-device usb-ehci,id=e -device usb-kbd,bus=e.0 -device usb-mouse,bus=e.0 -device usb-tablet,bus=e.0 -drive id=mu,file=$Y/mem.img,format=raw,if=none -device usb-storage,bus=e.0,drive=mu"
CAPTURAS=$G ESCRIBE=1 SATA=$Y/sata.img SOLO_LECTURA=1 MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/EQUIPO.BIN,$Y/RED.BIN ARRANQUE="escritorio escritura=usb unidad=M acpicada=1" \
  QEMU_MAS="-S $SSDT -nic user,model=e1000 $USBS" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${PUESTA[@]}" ":monitor cont" ":pausa 7" \
  ":clicd 63 40" ":pausa 1" ":captura dv1" ":clic 100 70" ":pausa 4" ":captura dv2a" ":tecla shift-bracket_right" ":pausa 1" ":captura dv2" ":clic 395 81" ":pausa 3" ":captura dv3" \
  ":tecla down" ":tecla ret" ":pausa 4" ":captura dv4" ":clic 228 140" ":raton 400 450" ":pausa 1" ":captura dv5" ":clic 323 140" ":raton 400 450" ":pausa 1" ":captura dv6" \
  ":tecla esc" ":pausa 1" ":clic 245 81" ":pausa 2" ":captura dv7" \
  ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":tecla down" ":tecla left" ":pausa 1" ":captura dv10" ":clic 742 31" ":pausa 1" \
  ":clic 40 585" ":pausa 1" ":clic 300 238" ":raton 700 530" ":pausa 1" ":captura dv8" ":clic 336 422" ":pausa 4" ":captura dv9" > $Y/pc.sal 2>&1
S=$Y/pc.sal
mira "ACPI.BIN apunta lo que hay en el espacio de nombres de ACPI (la llamada 156)" "^ACPI: [0-9]* dispositivos del espacio de nombres, apuntados para el Administrador de dispositivos"
mira "Mi equipo > Propiedades, Configuración > «Administrador de dispositivos...» y las propiedades lanzan DISPOSIT.BIN" \
  "^escritorio: lanzo .DISPOSIT.BIN (la tarea [0-9]*)$"
[ "$(grep -c '^escritorio: lanzo .DISPOSIT.BIN' $S)" = 2 ] && ok "(dos veces: desde Mi equipo y desde Configuración)" || ko "DISPOSIT.BIN no se lanza dos veces: $(grep -c '^escritorio: lanzo .DISPOSIT' $S)"
miracap "el botón secundario en Mi equipo: «Abrir» y «Propiedades»" dv1 texto 60 30 200 100 Abrir texto 60 30 200 100 Propiedades
miracap "(RECOGIDO) al abrirla: lo que funciona entero, recogido y dicho en palabra; lo que tiene algo que no va, abierto, con lo que falla a la vista" dv2a \
  negrita 40 120 400 520 "[+] Pantalla (2)" negrita 40 120 400 520 "[+] Discos (2)" negrita 40 120 400 520 "[+] Red (1)" texto 380 120 620 160 "todo funciona" \
  negrita 40 120 400 520 "[-] Teclado y ratón" texto 380 120 620 520 "1 NO VA" negrita 40 120 760 520 "! QEMU USB Tablet" texto 300 120 720 520 "SIN CONTROLADOR"
miracap "con «*», desplegado todo: el árbol por categorías, con el controlador de cada cosa" dv2 \
  negrita 30 15 400 40 "Administrador de dispositivos" negrita 40 120 400 520 "[-] Pantalla (2)" negrita 40 120 500 520 "Intel PRO/1000 MT (82540EM)" \
  texto 400 120 760 520 RED.BIN texto 400 120 760 520 ahci.cas texto 30 60 700 100 "Guardar informe"
miracap "las categorías se cierran con la flecha izquierda; la tableta USB, «SIN CONTROLADOR» con su «!», fondo naranja y su franja" dv10 \
  negrita 40 120 400 520 "[+] Discos (2)" negrita 40 120 400 520 "[+] Red (1)" negrita 40 120 760 520 "! QEMU USB Tablet" texto 300 120 720 520 "SIN CONTROLADOR"
miracap "«Guardar informe»: dónde lo ha dejado (la memoria USB, X:)" dv3 texto 30 520 760 545 "Informe guardado en X:\\DISPOSIT.TXT"
mcopy -n -i $Y/mem.img ::DISPOSIT.TXT $Y/disposit.437 > /dev/null 2>&1
iconv -f CP437 -t UTF-8 < $Y/disposit.437 2> /dev/null | tr -d '\r' > $Y/disposit.txt
S=$Y/disposit.txt
mira "el informe (X:\\DISPOSIT.TXT): el equipo, el resumen y, lo primero, lo que no tiene controlador" \
  "^Administrador de dispositivos de Hispa-Pentium$" "^Resumen: [0-9]* dispositivos; [0-9]* funcionan, [1-9][0-9]* SIN CONTROLADOR" \
  "^Lo que falta (sin controlador o con error):$" "^  \[!\]  QEMU USB Tablet  \[0627:0001; Teclado y ratón\]$"
mira "el PCI: cada aparato con su nombre, su identificador, su controlador, sus zonas medidas y su IRQ" \
  "^  \[OK\] Intel 440FX: puente al procesador  \[8086:1237\]$" "^  \[OK\] QEMU/Bochs: tarjeta gráfica (VBE de Bochs)  \[1234:1111\]$" \
  "^       Memoria: FD000000-FDFFFFFF (16 MB)$" "^       Controlador: video.cas (en el núcleo)$" "^  \[OK\] Intel ICH4: USB 2.0 (EHCI)  \[8086:24CD\]$" \
  "^       Controlador: ehci.cas (en el núcleo)$" "^       Clase: 010601 (Controladora SATA (AHCI))$" "^       Controlador: ahci.cas (en el núcleo)$" \
  "^       Interrupción (IRQ): [0-9]*$"
mira "la red: la e1000 la lleva RED.BIN, con su MAC, el enlace y la IP" "^  \[OK\] Intel PRO/1000 MT (82540EM)  \[8086:100E\]$" \
  "^       Controlador: RED.BIN (un servicio del anillo 3)$" "^       MAC: 52:54:00:12:34:56$" "^       Enlace: sí, a 1000 Mbit/s$" "^       Detalle: conectada: IP 10.0.2.15$" \
  "^       Puertos: C0[0-9A-F]*-C0[0-9A-F]* (64 octetos)$"
mira "el USB: el teclado y el ratón (usbhid.cas), la memoria (usbalm.cas), con su nombre de verdad (sus descriptores de cadena) y de qué controladora cuelgan" \
  "^  \[OK\] QEMU USB Keyboard  \[0627:0001\]$" "^  \[OK\] QEMU USB Mouse  \[0627:0001\]$" "^       Controlador: usbhid.cas (en el núcleo)$" \
  "^  \[OK\] QEMU USB HARDDRIVE  \[46F4:0001\]$" "^       Controlador: usbalm.cas (en el núcleo)$" "^       Cuelga de: Intel ICH4: USB 2.0 (EHCI)$"
mira "los discos: el SATA y la memoria, con su tamaño y sus unidades" "^  \[OK\] QEMU HARDDISK (64 MB)$" "^       Detalle: 64 MB; unidades C:; de solo lectura$" \
  "^       Detalle: 32 MB; unidades X:; se puede escribir$" "^       Cuelga de: QEMU USB HARDDRIVE$"
mira "lo de siempre: el teclado y el ratón PS/2, el reloj, el 8253, los 8259 y COM1, con lo que dice ACPI de ellos" \
  "^  \[OK\] Teclado PS/2  \[PNP0303\]$" "^       En ACPI: .*KBD_$" "^  \[OK\] Ratón PS/2  \[PNP0F13\]$" "^  \[OK\] Reloj de la CMOS (fecha y hora)  \[PNP0B00\]$" \
  "^  \[OK\] Puerto serie COM1  \[PNP0501\]$" "^       Puertos: 03F8-03FF (8 octetos)$"
mira "ACPI: el sistema (ACPI.BIN), la batería con su carga y su salud, el adaptador, y lo que no tiene controlador (la disquetera, el puerto paralelo, el HPET)" \
  "^  \[OK\] Sistema ACPI (apagar, reiniciar y la batería)  \[ACPI\]$" "^  \[OK\] Batería  \[PNP0C0A\]$" "^       Carga: 55 %, cargando$" \
  "^       Salud: 68 % (guarda 5000 de 7350 mAh" "^  \[OK\] Adaptador de corriente  \[ACPI0003\]$" "^       Corriente: enchufado$" "^  \[!\]  Controladora de disquetes  \[PNP0700\]$" \
  "^  \[OK\] Bus PCI  \[PNP0A03\]$"
mira "el procesador (CPUID)" "^== Procesador ==$" "^  \[OK\] QEMU Virtual CPU" "^       Firma: [0-9A-F]* (CPUID 1)$"
miracap "las propiedades (Intro en la tarjeta gráfica): General, con su signo y su estado" dv4 negrita 100 60 700 100 "Propiedades: QEMU/Bochs" \
  negrita 100 120 400 160 General texto 100 120 400 160 Controlador texto 100 120 400 160 Recursos texto 100 150 700 400 "Funciona: lo lleva video.cas." \
  texto 100 150 700 400 "800 x 600, 32 bits"
miracap "la pestaña Controlador: el controlador, el identificador, el subsistema y la clase" dv5 negrita 100 120 400 160 Controlador \
  texto 100 150 700 400 "video.cas (en el núcleo)" texto 100 150 700 400 "PCI 1234:1111" texto 100 150 700 400 "030000 (Tarjeta gráfica (VGA))"
miracap "la pestaña Recursos: sus zonas de memoria, con lo que miden" dv6 negrita 100 120 400 160 Recursos texto 100 150 700 400 "FD000000-FDFFFFFF (16 MB)"
miracap "«Copiar informe»: al portapapeles" dv7 texto 30 520 760 545 "Informe copiado al portapapeles"
miracap "Configuración (el Panel de control), con «Dispositivos»" dv8 negrita 140 80 160 95 "Configuración" negrita 280 437 290 447 "Dispositivos"
miracap "y desde ahí, la ventana otra vez" dv9 negrita 30 15 400 40 "Administrador de dispositivos"
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $Y/pc.sal; then ok "ni una excepción del núcleo ni un error de ACPICA (recorriendo el espacio de nombres y los _CRS)"
else ko "hay excepciones o errores:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $Y/pc.sal | head -4 | sed 's/^/        /'; fi
# 2. «q35» (ICH9), sin escritorio: el informe, en texto, por la salida
QXHCI="-device qemu-xhci,id=x -device usb-kbd,bus=x.0 -device usb-hub,bus=x.0,id=h,port=2 -device usb-mouse,bus=x.0,port=2.1"
MAQUINA=q35 MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/ORDENES.BIN,$Y/RED.BIN ARRANQUE="ordenes conred acpicada=1" \
  QEMU_MAS="-S $SSDT -netdev user,id=n0 -device rtl8139,netdev=n0 -drive id=d0,file=$Y/sata.img,format=raw,if=none,snapshot=on -device ide-hd,drive=d0,bus=ide.0 $QXHCI" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${PUESTA[@]}" ":monitor cont" ":pausa 8" "disposit /informe" ":pausa 6" > $Y/q35.sal 2>&1
tr -d '\r' < $Y/q35.sal > $Y/q35.txt
S=$Y/q35.txt
mira "q35: el informe en texto (sin escritorio), con la ICH9: el puente al procesador, el LPC, el AHCI de la placa con su disco y el SMBus sin controlador" \
  "^  \[OK\] Intel Q35: puente al procesador  \[8086:29C0\]$" "^  \[OK\] Intel ICH9: puente LPC  \[8086:2918\]$" "^  \[OK\] Intel ICH9: SATA (AHCI)  \[8086:2922\]$" \
  "^       Detalle: 1 discos SATA" "^  \[!\]  Intel ICH9: SMBus  \[8086:2930\]$" "^  \[OK\] QEMU HARDDISK (64 MB)$"
mira "q35: la RTL8139 (RED.BIN), la xHCI con el teclado y, detrás de un concentrador, el ratón" "^  \[OK\] Realtek RTL8139 (Fast Ethernet)  \[10EC:8139\]$" \
  "^       Controlador: RED.BIN (un servicio del anillo 3)$" "^  \[OK\] QEMU: controladora USB 3 (xHCI)  \[1B36:000D\]$" "^       Controlador: xhci.cas (en el núcleo)$" \
  "^  \[OK\] QEMU USB Hub  \[0409:55AA\]$" "^       Controlador: usb.cas (en el núcleo)$" "^       Cuelga de: QEMU USB Hub$" "^  \[OK\] QEMU USB Mouse  \[0627:0001\]$"
mira "q35: y la batería de ACPI" "^  \[OK\] Batería  \[PNP0C0A\]$" "^       Carga: 55 %, cargando$"
# 3. con «sinusb» y «sinacpi»: lo que no se usa, desactivado (y dicho así)
MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/ORDENES.BIN ARRANQUE="ordenes sinusb sinacpi" QEMU_MAS="-device usb-ehci,id=e" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 3" "disposit /informe" ":pausa 5" > $Y/sin.sal 2>&1
tr -d '\r' < $Y/sin.sal > $Y/sin.txt
S=$Y/sin.txt
mira "con «sinusb» y «sinacpi»: la controladora USB y ACPI, desactivadas (con el porqué)" "^  \[--\] Intel ICH4: USB 2.0 (EHCI)  \[8086:24CD\]$" \
  "^       Detalle: «sinusb» en la línea del cargador$" "^  \[--\] Sistema ACPI (apagar, reiniciar y la batería)  \[ACPI\]$" "^       Detalle: «sinacpi» en la línea del cargador$"
if ! cat $Y/q35.sal $Y/sin.sal | grep -q "excepción\|\*\*\*"; then ok "y ni una excepción del núcleo en el q35 ni sin USB"
else ko "el núcleo se ha quejado:"; cat $Y/q35.sal $Y/sin.sal | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Administrador de dispositivos)"
fi   # el Administrador de dispositivos
# ------------------------- 19 bis. (INFORME-DISP) Hispa-Pentium 0.8.12: el informe de lo que falla
# «Enviar informe de los dispositivos que fallan» (DISPOSIT.BIN) y su receptor
# (tools/informe-disp): pruebas_informe.sh, informe_disp
if parte08 dispositivos || parte08 informe; then
  informe_disp
fi
# ------------------------------------------- 20. SSH, WireGuard y Tailscale
# (ssh; docs/PENTIUM.md §14 quinquies) Los programas en C de
# nucleo32/hispac/ (hacer.sh): SSHD contra el cliente OpenSSH de aquí (QEMU
# con hostfwd), SSH contra un sshd de OpenSSH propio de la prueba (puerto alto,
# ForceCommand: no da una shell); y, con root, ip netns y wg, WireGuard contra
# el del núcleo Linux en un espacio de nombres propio; con HEADSCALE= (el
# binario), Tailscale contra Headscale y un tailscaled de pares, en otro. Sin
# eso, esas dos se saltan (y se dice).
if parte08 ssh; then
echo "== Hispa-Pentium: SSH (SSH.BIN, SSHD.BIN), WireGuard (WG.BIN) y Tailscale (TAILSCAL.BIN) =="
R=$P/ssh
rm -rf $R; mkdir -p $R
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/hispac/hacer.sh $R/bin > $R/hacer.txt 2>&1; then
  ok "nucleo32/hispac/hacer.sh: SSH.BIN, SSHD.BIN, WG.BIN y TAILSCAL.BIN con Monocypher ($(stat -c %s $R/bin/SSH.BIN), $(stat -c %s $R/bin/SSHD.BIN), $(stat -c %s $R/bin/WG.BIN) y $(stat -c %s $R/bin/TAILSCAL.BIN) octetos)"
else ko "nucleo32/hispac/hacer.sh no los hace: $(tail -n 1 $R/hacer.txt)"; fi
ssh-keygen -q -t ed25519 -N '' -f $R/clave -C prueba
PS=$((30000 + RANDOM % 10000)); PH=$((PS + 1))
bash $AQUI/nucleo32/hispac/prueba_red.sh sshd $R/sshd $PS
if $AQUI/nucleo32/disco.sh $R/d.img 16 red tls ssh > $R/disco.txt 2>&1 && mmd -i $R/d.img ::SSH && cp $R/clave.pub $R/AUT.TXT && mcopy -i $R/d.img $R/AUT.TXT ::SSH/AUTORIZA.TXT \
   && mcopy -i $R/d.img $R/sshd/ID_ED.SSH ::SSH/ID_ED.SSH && [ -s $R/sshd/sshd.pid ]; then
# 1. SSHD, con el ssh de aquí: una orden y una sesión con terminal
cp $R/d.img $R/a.img
# (0.8.12) el puerto lo abre QEMU al arrancar, y antes probar.sh compila la
# semilla (más de 2 min con la máquina cargada): hasta 600 s, no 120 (con 120
# el ssh iba antes de que hubiera QEMU: «Connection refused», desde la 0.8.11)
( for _ in $(seq 600); do sleep 1; timeout 2 bash -c "</dev/tcp/127.0.0.1/$PH" 2> /dev/null && break; done
  sleep 20
  timeout 60 ssh -p $PH -o StrictHostKeyChecking=no -o UserKnownHostsFile=$R/conocidos -o UpdateHostKeys=no -o IdentitiesOnly=yes -o BatchMode=yes -i $R/clave prueba@127.0.0.1 \
    "echo hola desde hispa" < /dev/null > $R/orden.txt 2>&1; echo "código $?" >> $R/orden.txt
  printf 'echo dentro de la consola\rver\rsalir\r' > $R/teclas.txt
  setsid timeout 60 ssh -tt -p $PH -o StrictHostKeyChecking=no -o UserKnownHostsFile=$R/conocidos -o UpdateHostKeys=no -o IdentitiesOnly=yes -o BatchMode=yes -i $R/clave \
    prueba@127.0.0.1 < $R/teclas.txt > $R/tty.txt 2>&1
  echo "código $?" >> $R/tty.txt ) &
FUERA=$!
ESCRIBE=1 DISCO=$R/a.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PH-:22 -device e1000,netdev=n0" \
  $AQUI/nucleo32/probar.sh "" ":pausa 3" "sshd /encender" ":pausa 75" "sshd" ":pausa 2" > $R/a.sal 2>&1
wait $FUERA
tr -d '\r' < $R/a.sal > $R/a.txt
S=$R/a.txt
mira "SSHD: apagado hasta que se enciende; con /ENCENDER escucha en el puerto 22, con su llave del equipo (ssh-ed25519) hecha la primera vez" \
  "^Servidor SSH encendido (puerto 22)" "sshd: llave del equipo nueva (ssh-ed25519)$" "red: tcp: escuchando en el puerto 22$" \
  "sshd: Hispa-Pentium: el servidor SSH, en el puerto 22 (llave SHA256:"
mira "SSHD con el cliente OpenSSH: curve25519-sha256, chacha20-poly1305 y kex-strict; entra con la llave de AUTORIZA.TXT" \
  "sshd: la otra punta: SSH-2.0-OpenSSH" "sshd: kex-strict: sí (contra Terrapin)$" "sshd: cifrado: curve25519-sha256, ssh-ed25519, chacha20-poly1305@openssh.com$" \
  "sshd: entra prueba con su llave$" "sshd: orden: echo hola desde hispa$" "^El servidor SSH de Hispa-Pentium: encendido (puerto 22)"
S=$R/orden.txt
mira "ssh equipo orden: la orden se hace en Hispa-Pentium (ORDENES /C) y vuelve lo que escribe, con el código 0" "^hola desde hispa$" "^código 0$"
if grep -q "hola desde hispa.$(printf '\r')" $R/orden.txt; then ko "sin terminal, los finales de línea tienen que ir como en Unix (sin CR)"; else ok "sin terminal (una orden), los finales de línea van como en Unix"; fi
tr -d '\r' < $R/tty.txt > $R/tty2.txt
S=$R/tty2.txt
mira "una sesión con terminal: la Consola de Hispa-Pentium (en UTF-8), las órdenes que se teclean al otro lado y SALIR" \
  "^Hispa-Pentium: intérprete de órdenes" "^dentro de la consola$" "^Intérprete de órdenes del anillo 3 (ORDENES.BIN).$" "^Hasta luego.$" "^código 0$"
# 2. SSH, con un sshd de OpenSSH: la huella, la llave propia, el terminal y una orden
cp $R/d.img $R/b.img
CAPTURAS=$R ESCRIBE=1 DISCO=$R/b.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 3" \
  "ssh -p $PS root@10.0.2.2" ":pausa 10" ":escribe s" ":pausa 6" ":texto b1" "hola mundo" ":pausa 2" ":texto b2" ":pausa 4" \
  "ssh -p $PS root@10.0.2.2 una orden" ":pausa 10" ":texto b3" > $R/b.sal 2>&1
tr -d '\r' < $R/b.sal > $R/b.txt
S=$R/b.txt
mira "SSH con OpenSSH: la primera vez enseña la huella y pregunta; entra con la llave propia (ID_ED.SSH)" \
  "ssh: la otra punta: SSH-2.0-OpenSSH" "ssh: kex-strict: sí (contra Terrapin)$" "ssh: dentro, con la llave propia$" "ssh: sesión acabada (código 0)$" "ssh: sesión acabada (código 3)$"
S=$R/b1.txt
mira "el terminal de SSH (en la pantalla de texto): borrar, colores, mover el cursor y el UTF-8 (a la página 437)" \
  "^|HOLA DESDE OPENSSH TERM=xterm" "^|          caja azul  ─ñ" "^|Escriba algo:"
S=$R/b2.txt
mira "lo que se teclea va al otro lado (y vuelve: «leido»)" "^|leido: \[hola mundo\]"
S=$R/b3.txt
mira "SSH con una orden: sin terminal, lo que sale, aquí mismo, y el código de salida de allí" "^|orden: una orden$" "^|(SSH.BIN ha acabado con el código 3)"
S=$R/b.txt
if grep -q "^\[10.0.2.2\]:$PS ssh-ed25519 " <(mtype -i $R/b.img ::SSH/CONOCIDO.TXT 2> /dev/null); then ok "la llave del servidor queda en \\HISPA\\SSH\\CONOCIDO.TXT (la segunda vez no se pregunta)"
else ko "no está la llave del servidor en CONOCIDO.TXT"; fi
# 3. WireGuard contra el del núcleo Linux (root, ip netns y wg)
if [ "$(id -u)" = 0 ] && command -v wg > /dev/null && ip netns list > /dev/null 2>&1; then
  cp $R/d.img $R/w.img
  bash $AQUI/nucleo32/hispac/prueba_red.sh wg $R/wg $R/w.img
  tr -d '\r' < $R/wg/q.sal > $R/w.txt
  S=$R/w.txt
  mira "WireGuard (WG /ENCENDER, el archivo de wg-quick): el apretón de manos con el WireGuard de Linux y la interfaz del túnel en RED.BIN" \
    "red: túnel: la interfaz, con la IP 10.9.0.2 y 1 redes$" "wg: Hispa-Pentium: WireGuard en marcha (10.9.0.2, clave pública " \
    "wg: 10.0.2.2: túnel hecho (claves nuevas)$" "^Respuesta desde 10.9.0.1: octetos=32 " "^  último apretón de manos: "
  S=$R/wg/fuera.txt
  mira "por el túnel, desde Linux: ping a Hispa-Pentium y SSH a su SSHD" "^64 bytes from 10.9.0.2" "latest handshake" "^hola por wireguard$" "^código 0$"
else echo "  (WireGuard: hace falta root, ip netns y wg: se salta)"; fi
# 4. Tailscale contra Headscale (HEADSCALE=...) y un tailscaled de pares
if [ "$(id -u)" = 0 ] && [ -x "${HEADSCALE:-}" ] && command -v tailscaled > /dev/null && ip netns list > /dev/null 2>&1; then
  cp $R/d.img $R/t.img
  bash $AQUI/nucleo32/hispac/prueba_red.sh tailscale $R/ts $R/t.img $PS
  tr -d '\r' < $R/ts/q.sal > $R/t.txt
  S=$R/t.txt
  mira "Tailscale: ts2021 con Headscale, la dirección de alta (que aprueba la prueba) y el registro" \
    "tailscale: control: conectado con 10.0.2.2 (ts2021)$" "tailscale: dar de alta este equipo: abra https://10.0.2.2:8443/register/" \
    "^  https://10.0.2.2:8443/register/" "tailscale: registrado en la red de prueba$"
  mira "el mapa de la red: la IP 100.x, el relevo DERP de casa, las reglas, la interfaz del túnel y los nombres (MagicDNS)" \
    "tailscale: relevo de casa: derp-999 (Relevo de prueba)$" "tailscale: reglas de la red: [1-9]" "red: túnel: la interfaz, con la IP 100.64.0." \
    "tailscale: en la red: hispa-pentium.prueba.hispa., 100.64.0." "tailscale: derp-999: conectado con 10.0.2.2$" \
    "^Este equipo: 100.64.0.[0-9]*  hispa-pentium.prueba.hispa." "^  100.64.0.[0-9]* *pares-vps *en línea"
  mira "con el otro equipo, por DERP: el apretón de WireGuard, ping por el nombre y SSH a su sshd" \
    "tailscale: pares-vps: túnel hecho" "^Haciendo ping a pares-vps \[100.64.0." "^Respuesta desde 100.64.0." "ssh: conectando con pares-vps (100.64.0." "ssh: dentro, con la llave propia$"
  S=$R/ts/fuera.txt
  mira "y desde el otro equipo (tailscaled), SSH al SSHD de Hispa-Pentium por la red de Tailscale" "hispa-pentium" "^hola por tailscale$" "^código 0$"
  mira "lo que el servidor de control guarda del equipo (la actualización ligera del MapRequest): la clave del disco, y el otro equipo la ve con el relevo de casa" "^nodo hispa-pentium disco sí" "^par hispa-pentium disco sí derp 999$"
else echo "  (Tailscale: hace falta root, ip netns, tailscaled y HEADSCALE=el binario de Headscale: se salta)"; fi
if ! cat $R/*.txt | grep -q "excepción\|\*\*\*"; then ok "en todos esos arranques, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $R/*.txt | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else ko "no se ha podido hacer el disco de las pruebas de SSH: $(tail -n 1 $R/disco.txt)"; fi
[ -s $R/sshd/sshd.pid ] && kill "$(cat $R/sshd/sshd.pid)" 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin SSH, WireGuard ni Tailscale)"
fi   # ssh

# --------------------------------- 21. (CD) los lectores de CD y DVD y las imágenes
if parte08 lectores; then
echo "== Hispa-Pentium: los CD y DVD (IDE, AHCI y USB; ISO 9660 con Joliet), meter y sacar el disco, y las imágenes de disco =="
L=$P/lectores
mkdir -p $L $G
export MTOOLS_SKIP_CHECK=1
if command -v genisoimage > /dev/null; then HAZISO="genisoimage -quiet"; else HAZISO="xorriso -as mkisofs -quiet"; fi
[ -f $P/d16.img ] || $AQUI/nucleo32/disco.sh $P/d16.img 16 > $P/disco16.txt 2>&1
[ -f $P/d32.img ] || $AQUI/nucleo32/disco.sh $P/d32.img 32 > $P/disco32.txt 2>&1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $L/semilla.asm > /dev/null 2>&1 && nasm -f bin $L/semilla.asm -o $L/SEMILLA.BIN
# la ISO de prueba: nombres largos (Joliet, y Rock Ridge), una carpeta dentro de otra y un archivo de 300 K
rm -rf $L/iso $L/iso2; mkdir -p "$L/iso/Carpeta con nombre largo/sub" $L/iso2
printf 'Hola desde el CD de prueba\r\n' > $L/iso/LEEME.TXT
printf 'Texto con nombre largo, en Joliet.\r\n' > "$L/iso/Carpeta con nombre largo/documento-de-prueba-largo.txt"
printf 'dentro\r\n' > "$L/iso/Carpeta con nombre largo/sub/hondo.txt"
python3 -I -c "import random; random.seed(18); open('$L/iso/grande.bin', 'wb').write(bytes(random.randrange(256) for _ in range(300000)))"
$HAZISO -J -r -V PRUEBA_CD -o $L/joliet.iso $L/iso
$HAZISO -J -V OTRO -o $L/otro.iso $L/iso/LEEME.TXT
cp $L/joliet.iso $L/iso2/dentro.iso; $HAZISO -J -V CONTIENE -o $L/contiene.iso $L/iso2
LARGO="$L/iso/Carpeta con nombre largo/documento-de-prueba-largo.txt"
# ¿lo copiado al disco es igual que lo del CD? (la imagen y los archivos de ella)
igual_cd() { local img=$1 f; shift; for f in "$@"; do rm -f $L/sacado; mcopy -n -i $img ::$f $L/sacado 2> /dev/null || return 1
  case $f in DOC*) cmp -s $L/sacado "$LARGO" ;; *) cmp -s $L/sacado $L/iso/grande.bin ;; esac || return 1; done; }
if [ ! -f $L/joliet.iso ] || [ ! -f $L/SEMILLA.BIN ]; then ko "no se puede hacer la ISO de prueba (genisoimage o xorriso) o el núcleo"; else
# 1. como VirtualBox: un «pc» con el disco en SATA (AHCI) y el lector en el IDE secundario maestro
cp $P/d32.img $L/c.img
ESCRIBE=1 SATA=$L/c.img MAQUINA=pc ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "dir d:\\" "type d:\\leeme.txt" "dir d:\\carpet~1" "type d:\\carpet~1\\sub\\hondo.txt" "copy d:\\grande.bin c:\\grande.bin" \
  "copy d:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor eject -f cd" ":pausa 3" "dir d:\\" ":monitor change cd $L/otro.iso" ":pausa 3" "dir d:\\" \
  ":monitor change cd $L/joliet.iso" ":pausa 3" "type d:\\leeme.txt" "expulsar d:" "montar" salir disco apagar > $L/vbox.sal 2>&1
S=$L/vbox.sal
mira "como VirtualBox (el disco en SATA, el lector en el IDE secundario maestro): ATAPI por PIO, el lector es D: y su ISO 9660 se monta" \
  "^disco 0: QEMU HARDDISK, 40 MB, FAT32 (SATA)\.$" "^disco 1: QEMU DVD-ROM, lector de CD o DVD (IDE)\.$" "cd: D: el lector QEMU DVD-ROM (IDE)$" \
  "cd: D: CD de datos: «PRUEBA_CD», ISO 9660 Joliet, 1 MB\.$" "^unidades: C: (disco 0, SATA, FAT32) D: (disco 1, CD, ISO 9660); la del sistema, C:"
mira "DIR y TYPE en el CD: los nombres largos de Joliet (y el corto que les toca, CARPET~1), carpetas dentro de carpetas" \
  "^ Directorio de D:.$" "^CARPET~1 *<DIR> .*Carpeta con nombre largo$" "^GRANDE   BIN      300000 " "^Hola desde el CD de prueba$" \
  "^DOCUME~1 TXT          36 .*documento-de-prueba-largo\.txt$" "^dentro$"
if igual_cd $L/c.img GRANDE.BIN DOC.TXT; then ok "lo copiado del CD al disco es igual que lo de la ISO, octeto a octeto (300 K y el del nombre largo)"
else ko "lo copiado del CD (IDE) no es igual que lo de la ISO"; fi
mira "sacar el disco, meter otro y volver al primero (el monitor de QEMU: eject y change): la unidad se va y vuelve sola, con aviso" \
  "cd: D: sin disco\.$" "cd: aviso: CD retirado: D:$" "^No puedo abrir d:.: no hay disco" "cd: D: CD de datos: «OTRO», ISO 9660 Joliet" "cd: aviso: CD insertado: D:$"
mira "EXPULSAR D: abre la bandeja; MONTAR dice lo que hay en el lector" "cd: D: expulsado\.$" "^Ya puede sacar el disco de D:\.$" "^  D:  lector de CD o DVD QEMU DVD-ROM: sin disco$"
# 2. por AHCI: el q35, con el lector en un puerto SATA (la firma EB140101)
cp $P/d16.img $L/q.img
ESCRIBE=1 DISCO=$L/q.img MAQUINA=q35 ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.2" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "copy d:\\grande.bin c:\\grande.bin" "copy d:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor change cd $L/otro.iso" ":pausa 3" "dir d:\\" salir apagar > $L/ahci.sal 2>&1
S=$L/ahci.sal
mira "por AHCI (el q35): el puerto con la firma EB140101 es un lector, ATAPI por DMA (PACKET con el bit A y el paquete en ACMD)" \
  "^disco 1: QEMU DVD-ROM, lector de CD o DVD (SATA)\.$" "cd: D: CD de datos: «PRUEBA_CD», ISO 9660 Joliet" "^ *1 archivo(s) copiado(s)\.$" "cd: D: CD de datos: «OTRO»"
if igual_cd $L/q.img GRANDE.BIN DOC.TXT; then ok "y lo copiado del CD por AHCI es igual que lo de la ISO"; else ko "lo copiado del CD (AHCI) no es igual que lo de la ISO"; fi
# 3. por USB: un lector de CD (usb-storage con la ISO: SCSI, clase 5) al arrancar, y otro enchufado en marcha
cp $P/d16.img $L/u.img
ESCRIBE=1 DISCO=$L/u.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u -drive id=ucd,file=$L/joliet.iso,media=cdrom,if=none -device usb-storage,bus=u.0,drive=ucd,id=ulector -drive id=ucd2,file=$L/otro.iso,media=cdrom,if=none" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "copy e:\\grande.bin c:\\grande.bin" "copy e:\\carpet~1\\docume~1.txt c:\\doc.txt" ":monitor eject -f ucd" ":pausa 3" \
  ":monitor device_add usb-storage,bus=u.0,drive=ucd2,id=ulector2" ":pausa 4" "type f:\\leeme.txt" ":monitor device_del ulector" ":pausa 3" "montar" salir apagar > $L/usb.sal 2>&1
S=$L/usb.sal
mira "por USB: un lector de CD (almacenamiento masivo, SCSI de la clase 5) es una unidad de CD más (E:; D: es el lector vacío de QEMU)" \
  "^disco 2: QEMU    QEMU CD-ROM, lector de CD o DVD (USB)\.$" "cd: E: CD de datos: «PRUEBA_CD», ISO 9660 Joliet" "cd: aviso: CD retirado: E:$"
mira "y enchufado en marcha, se lee al momento; desenchufado, se va (y su letra)" "cd: aviso: Lector de CD conectado: F:$" "^Hola desde el CD de prueba$" \
  "cd: aviso: Lector de CD desconectado: E:$" "^  F:  lector de CD o DVD QEMU    QEMU CD-ROM: «OTRO»$"
if igual_cd $L/u.img GRANDE.BIN DOC.TXT; then ok "y lo copiado del CD por USB es igual que lo de la ISO"; else ko "lo copiado del CD (USB) no es igual que lo de la ISO"; fi
# 4. las imágenes de disco: la misma ISO, un disquete (.IMG, FAT12) y la ISO en crudo (.CUE y .BIN, MODE1/2352), desde C:
cp $P/d16.img $L/i.img
mmd -i $L/i.img ::DISCOS; mcopy -i $L/i.img $L/joliet.iso ::DISCOS/PRUEBA.ISO
rm -f $L/flop.img; mformat -C -f 1440 -i $L/flop.img ::; printf 'en el disquete\r\n' > $L/disq.txt; mcopy -i $L/flop.img $L/disq.txt ::DISQUETE.TXT; mcopy -i $L/i.img $L/flop.img ::DISCOS/FLOP.IMG
python3 -I - $L <<'PY'
import sys
L = sys.argv[1]
d = open(L + '/joliet.iso', 'rb').read()
o = bytearray()
for i in range(len(d) // 2048):
    o += b'\x00' + b'\xff' * 10 + b'\x00' + bytes(3) + b'\x01' + d[i * 2048:(i + 1) * 2048] + bytes(288)
open(L + '/crudo.bin', 'wb').write(o)
open(L + '/crudo.cue', 'w').write('FILE "CRUDO.BIN" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n')
PY
mcopy -i $L/i.img $L/crudo.bin ::DISCOS/CRUDO.BIN; mcopy -i $L/i.img $L/crudo.cue ::DISCOS/CRUDO.CUE
ESCRIBE=1 DISCO=$L/i.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "montar c:\\discos\\prueba.iso" "montar c:\\discos\\flop.img" "montar c:\\discos\\crudo.cue" "montar" "type f:\\disquete.txt" \
  "copy e:\\grande.bin c:\\grande.bin" "copy g:\\carpet~1\\docume~1.txt c:\\doc.txt" "copy g:\\grande.bin c:\\crudo.bin" "type e:\\carpet~1\\sub\\hondo.txt" \
  "desmontar e:" "desmontar f:" "montar" "montar c:\\leeme.txt" "desmontar c:" salir apagar > $L/imagen.sal 2>&1
S=$L/imagen.sal
mira "MONTAR una imagen de disco: la .ISO (la misma del lector), un disquete .IMG (FAT12) y una .CUE con su .BIN en crudo, cada una con la primera letra libre" \
  "^c:.discos.prueba\.iso está montada como la unidad E:\.$" "^c:.discos.flop\.img está montada como la unidad F:\.$" "^c:.discos.crudo\.cue está montada como la unidad G:\.$" \
  "^  E:  imagen C:.discos.prueba\.iso  «PRUEBA_CD»$" "^  F:  imagen C:.discos.flop\.img$" "^en el disquete$" "^dentro$"
if igual_cd $L/i.img GRANDE.BIN DOC.TXT CRUDO.BIN; then ok "lo leído de las imágenes (la .ISO y la .BIN en crudo) es igual que lo de la ISO del lector"; else ko "lo leído de una imagen montada no es igual que lo de la ISO"; fi
mira "DESMONTAR la quita; lo que no es una imagen, o no es una imagen montada, se dice" "^La unidad E: está desmontada\.$" "^La unidad F: está desmontada\.$" \
  "^No puedo montar c:.leeme\.txt: no es una imagen de disco que se pueda leer" "^C: no es un CD ni una imagen montada\.$"
# 5. una imagen dentro del CD y otra en una memoria USB que se desenchufa: se desmontan solas
cp $P/d16.img $L/m.img; mcopy -i $L/m.img $L/joliet.iso ::PRUEBA.ISO
DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-drive id=cd,file=$L/contiene.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0 -device qemu-xhci,id=u -drive id=mu,file=$L/m.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu,id=memo" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN "montar d:\\dentro.iso" "montar x:\\prueba.iso" "type e:\\leeme.txt" "type f:\\leeme.txt" ":monitor device_del memo" ":pausa 3" "montar" \
  ":monitor eject -f cd" ":pausa 3" "montar" salir apagar > $L/base.sal 2>&1
S=$L/base.sal
mira "una imagen dentro del CD y otra en la memoria USB: si se va la memoria, o se saca el CD, su imagen se desmonta sola (sin colgarse)" \
  "^d:.dentro\.iso está montada como la unidad E:\.$" "^x:.prueba\.iso está montada como la unidad F:\.$" "cd: aviso: Imagen desmontada: F: (su disco se ha ido)$" \
  "^  E:  imagen D:.dentro\.iso  «PRUEBA_CD»$" "cd: aviso: Imagen desmontada: E: (su disco se ha ido)$" "^  D:  lector de CD o DVD QEMU DVD-ROM: sin disco$"
if ! cat $L/*.sal | grep -q "excepción\|\*\*\*"; then ok "en todo eso, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $L/*.sal | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
# 6. el escritorio: el explorador (Mi equipo) con el CD (D:), y una .ISO
# montada al abrirla (el CD virtual, E:), que «Expulsar» desmonta
cp $P/d16.img $L/e.img; mmd -i $L/e.img ::DISCOS; mcopy -i $L/e.img $L/joliet.iso ::DISCOS/PRUEBA.ISO
CAPTURAS=$G ESCRIBE=1 DISCO=$L/e.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-drive id=cd,file=$L/joliet.iso,media=cdrom,if=none -device ide-cd,drive=cd,id=lector,bus=ide.1,unit=0" \
  $AQUI/nucleo32/probar.sh $L/SEMILLA.BIN ":pausa 3" ":doble 62 30" ":pausa 3" ":captura cd18a" ":tecla f4" ":escribe C:\\DISCOS" ":tecla ret" ":pausa 2" \
  ":tecla ret" ":pausa 3" ":raton 600 440" ":captura cd18b" ":tecla backspace" ":pausa 2" ":raton 600 440" ":captura cd18e" ":clicd 330 222" ":pausa 1" ":captura cd18c" ":clic 370 257" ":pausa 2" \
  ":raton 600 440" ":captura cd18d" > $L/escritorio.sal 2>&1
S=$L/escritorio.sal
miracap "Mi equipo (el explorador) enseña el CD con su nombre y su letra, lo que tiene y que es de solo lectura" cd18a negrita 265 133 275 143 "HISPA (C:)" \
  negrita 515 133 525 143 "PRUEBA_CD (D:)" texto 515 151 525 161 "670 K, ISO 9660" texto 515 167 525 177 "(de solo lectura)"
miracap "abrir una .ISO la monta (E:) y entra en ella; el aviso de la bandeja es de los CD y DVD" cd18b texto 100 98 130 112 "E:\\" texto 200 130 700 400 "grande.bin" \
  texto 10 474 700 490 "PRUEBA.ISO, montada como E:" negrita 570 505 640 520 "CD y DVD"
miracap "el CD virtual sale en Mi equipo («CD virtual», con lo que tiene)" cd18e negrita 265 195 275 205 "PRUEBA_CD (E:)" texto 265 213 275 223 "CD virtual: 670 K"
miracap "y su menú (el botón secundario): «Expulsar»" cd18c texto 335 242 355 267 "Expulsar"
miracap "y «Expulsar» la quita de Mi equipo" cd18d notexto 216 190 460 212 "(E:)" texto 10 474 700 490 "desmontada"
mira "y «Expulsar» la desmonta" "cd: E: la imagen C:.DISCOS.PRUEBA\.ISO (sectores de 2048)$" "cd: E: imagen desmontada\.$" "escritorio: globo: Imagen desmontada: E:"
fi   # la ISO de prueba
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los CD y DVD)"
fi   # los lectores

# ------------------------------- 22. [WIN32] Los programas de Windows y DirectX
# WIN32.BIN (nucleo32/win32/): el cargador de PE y KERNEL32, USER32, GDI32...
# en C, con las DLL de Hispa-486 (MSVCRT, COMCTL32, DDRAW, DSOUND, DINPUT,
# WINMM); el disco, de nucleo32/win32/disco.sh (FAT32, con lo de las pruebas)
if parte08 win32; then
echo "== Hispa-Pentium: los programas de Windows de 32 bits y DirectX (WIN32.BIN: consola, ventanas, Naves, FASMW, red) =="
V=$P/w32
mkdir -p $V $G
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/win32/disco.sh $V/d.img > $V/disco.txt 2>&1; then
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null 2>&1 && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN
HAY_TP=0; mdir -i $V/d.img ::NASM > /dev/null 2>&1 && HAY_TP=1
# 1. los de consola, en el intérprete de órdenes (modo texto)
O=("cd \\PRUEBAS" "PEHOLA uno dos" ":pausa 2" "HOLAC tres" ":pausa 3" "K32PRUE" ":pausa 6" "USADLL" ":pausa 2")
[ $HAY_TP = 1 ] && O+=("cd \\NASM" "nasm -v" ":pausa 4" "nasm -f win32 hola.asm -o hola.obj" ":pausa 8" "dir hola.obj" ":pausa 1" \
  "cd \\TCC" "tcc hola.c -o hola.exe" ":pausa 12" "hola" ":pausa 3" "cd \\FASM\\DDRAW" "\\FASM\\FASM DDRAW.ASM" ":pausa 12" "dir ddraw.exe" ":pausa 1")
ESCRIBE=1 DISCO=$V/d.img ARRANQUE=ordenes MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN "${O[@]}" > $V/consola.sal 2>&1
tr -d '\r' < $V/consola.sal > $V/consola.txt
S=$V/consola.txt
mira "un .EXE se lanza por su nombre: el núcleo ve «MZ» y lo corre WIN32.BIN (PEHOLA: KERNEL32 a mano, su línea de órdenes y su código de salida)" \
  "^Hola desde Win32$" "^Linea: C:.PRUEBAS.PEHOLA.EXE uno dos$" "(PEHOLA.EXE ha acabado con el código 42)"
mira "HOLAC, en C (gcc y tools/elf2pe.py), con MSVCRT.DLL de Hispa-486" "^Hola desde C (gcc) en Hispa-486$" "^Primo 100: 541$" "^Suma: 704982704$" "(HOLAC.EXE ha acabado con el código 7)"
mira "K32PRUE: KERNEL32 de consola da lo que da Windows 98 (versión, archivos, búsquedas, carpetas, la hora, el montón, el entorno, TLS, los módulos)" \
  "^version: C0000A04$" "^versionex: 4.10 plataforma 1 build 1998$" "^escritos: 22$" "^leer3: uno$" "^crear_nuevo_existe: si error 80$" "^fin_busqueda: 18$" \
  "^ft_1970: 019DB1DED53E8000$" "^ida_y_vuelta: 2026-11-17 12:34:56.789 semana 2$" "^realloc: conservado 5000 0$" "^20000_vueltas: si$" "^tls: si 1234$" \
  "^getprocaddress: si 6$" "^modulo_propio: 00400000$" "^FIN K32PRUE$"
mira "una DLL propia (DLLPRUE.DLL, con su DllMain y sus datos exportados)" "^Doble(20) = 41$" "^contador = 42$"
if [ $HAY_TP = 1 ]; then
  mira "NASM 2.16 para Win32 (MinGW) dice su versión y ensambla un .ASM a un .OBJ de Win32" "^NASM version 2.16.03" "^hola.obj es un archivo de 196 octetos\.$"
  mira "TCC 0.9.27, el compilador de C de Windows, compila un «hola» (con su libtcc.dll) y el .EXE que hace corre" "^Hola desde TCC en Hispa-Pentium$" "(HOLA.EXE ha acabado con el código 7)"
  mira "FASM (el de consola) ensambla el ejemplo DDRAW de DirectDraw" "^flat assembler  version 1.73.32" "passes, .* seconds, 4608 bytes\.$" "^ddraw.exe es un archivo de 4608 octetos\.$"
else
  echo "  (sin NASM, TCC ni FASM: tests/thirdparty/fetch_win32.sh)"
fi
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 2. los de ventanas, desde la Consola del escritorio: cada uno, su ventana
CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
  ":escribe \\HISPA32\\HOLA32" ":tecla ret" ":pausa 5" ":captura w1" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\AVISO32" ":tecla ret" ":pausa 4" ":captura w2" ":tecla ret" ":pausa 2" ":captura w3" ":tecla ret" ":pausa 2" \
  ":escribe \\HISPA32\\CALC32" ":tecla ret" ":pausa 5" ":clic 99 152" ":clic 156 152" ":clic 270 252" ":clic 156 186" ":clic 213 252" ":pausa 1" ":captura w4" \
  ":clic 175 88" ":pausa 1" ":captura w5" ":tecla esc" ":pausa 1" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\MENU32" ":tecla ret" ":pausa 5" ":captura w6" ":tecla alt-f4" ":pausa 2" \
  ":escribe \\HISPA32\\CTL32" ":tecla ret" ":pausa 5" ":captura w7" ":tecla alt-f4" ":pausa 2" > $V/ventanas.sal 2>&1
S=$V/ventanas.sal
miracap "HOLA32: un programa de Windows de ventanas es una ventana de Hispa-Pentium (su clase, su WM_PAINT, el texto y los dibujos de GDI, un temporizador)" w1 \
  negrita 100 60 140 90 "Hola desde Win32" texto 90 90 130 115 "¡Hola, mundo de Win32!" texto 90 150 130 175 "Temporizador: bien" texto 90 190 130 215 "Todo bien."
miracap "AVISO32: MessageBoxA es un cuadro de Hispa-Pentium, con sus botones en español" w2 \
  negrita 270 210 320 240 "Prueba de Win32" texto 280 250 320 275 "¿Le gusta Hispa-486?" texto 280 300 360 340 "Sí" texto 400 300 470 340 "No"
miracap "Intro: «Sí» (el botón de antes) y el cuadro siguiente con la respuesta" w3 texto 280 250 320 280 "Ha dicho que sí." texto 280 280 320 310 "Todo bien."
miracap "CALC32: los botones con el ratón (78 + 5 = 83) y la caja (EDIT)" w4 negrita 60 50 100 75 "Calculadora" texto 60 100 120 130 "83"
miracap "su menú (de los recursos), con el menú emergente del núcleo" w5 texto 60 80 200 100 "Archivo" texto 100 100 220 140 "Raíz cuadrada"
miracap "MENU32: los menús y los recursos (textos, iconos, menús de los recursos)" w6 negrita 40 40 100 70 "Menús y recursos" texto 40 60 120 90 "Archivo"
miracap "CTL32: los controles de serie (botones, casillas, opciones, una lista con sus cuatro colores, una lista desplegable, una caja de varias líneas)" w7 \
  negrita 40 40 100 70 "Controles de Win32" texto 40 70 120 100 "Nombre:" texto 260 70 340 100 "Saludar" texto 40 130 100 160 "Rojo" texto 40 160 100 190 "Azul" texto 40 170 100 200 "Amarillo"
if ! grep -q "excepción\|\*\*\*" $S; then ok "con todos esos, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 3. Naves (DirectDraw a pantalla completa, DirectInput) desde Mi equipo: se
# juega; y el ejemplo DDRAW de FASM; y FASMW
# (en el explorador: la dirección, C:\\HISPA32, y la N lleva a NAVES.EXE)
CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 40" ":pausa 3" ":tecla f4" ":escribe C:\\HISPA32" ":tecla ret" ":pausa 2" \
  ":raton 600 440" ":captura n0" ":tecla n" ":tecla ret" ":pausa 9" ":captura n1" ":tecla spc" ":pausa 3" ":captura n2" \
  ":monitor sendkey left 1000" ":pausa 1" ":captura n3" ":monitor sendkey ctrl 200" ":pausa 0.3" ":captura n4" ":tecla esc" ":pausa 3" ":captura n5" > $V/naves.sal 2>&1
S=$V/naves.sal
miracap "Mi equipo ve los .EXE como programas y los abre (los corre WIN32.BIN)" n0 texto 100 98 220 112 "C:\\HISPA32" texto 210 130 790 470 "NAVES.EXE"
miracap "Naves: DirectDraw a pantalla completa (su modo, 320 x 240 a 8 bits, al doble y en el centro, sin la barra de tareas)" n1 \
  rgb 10 10 000000 rgb 790 590 000000 nrgb 40 585 37A53A 60
miracap "Espacio (DirectInput): empieza la partida, con las naves enemigas" n2 rgb 10 10 000000 distinto 80 60 640 200 $G/n1.ppm
miracap "la nave se mueve con las flechas (mantenidas) y dispara con Control" n3 distinto 80 380 640 100 $G/n2.ppm
miracap "Esc acaba el juego y vuelve el escritorio" n5 negrita 30 570 70 590 Inicio
if ! grep -q "excepción\|\*\*\*" $S; then ok "Naves: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if [ $HAY_TP = 1 ]; then
  CAPTURAS=$G DISCO=$V/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
    ":escribe cd \\FASM\\DDRAW" ":tecla ret" ":escribe \\FASM\\FASM DDRAW.ASM" ":tecla ret" ":pausa 12" ":escribe DDRAW" ":tecla ret" ":pausa 10" ":captura d1" ":tecla esc" ":pausa 3" \
    ":escribe cd \\FASM" ":tecla ret" ":escribe FASMW" ":tecla ret" ":pausa 12" ":captura f1" ":tecla ctrl-o" ":pausa 3" ":captura f2" ":escribe hello.asm" ":tecla ret" ":pausa 4" \
    ":captura f3" ":tecla ctrl-f9" ":pausa 10" ":captura f4" ":tecla ret" ":pausa 2" ":tecla alt-f4" ":pausa 3" ":escribe hello" ":tecla ret" ":pausa 4" ":captura f5" > $V/fasm.sal 2>&1
  S=$V/fasm.sal
  miracap "el ejemplo DDRAW de FASM (ensamblado en Hispa-Pentium): DirectDraw a 640 x 480 con su paleta, en el centro" d1 rgb 10 10 000000 rgb 790 10 000000 nrgb 400 500 000000
  miracap "FASMW, el flat assembler de Windows: su menú (MENUEX), su editor propio, las pestañas y la barra de estado (COMCTL32)" f1 \
    negrita 100 60 200 90 "flat assembler 1.73.32" texto 100 90 140 110 "File" texto 140 90 200 110 "Edit" texto 280 90 400 110 "Options" texto 100 330 200 360 "Untitled"
  miracap "Ctrl+O (su acelerador): el cuadro de abrir de COMDLG32, con la carpeta y sus archivos" f2 negrita 150 90 260 125 "Abrir" texto 150 125 400 160 "C:\\FASM" texto 150 150 300 190 "HELLO.ASM"
  miracap "abre HELLO.ASM (con sus colores) y lo ensambla en otro hilo (Ctrl+F9): «4 passes»" f4 texto 90 320 220 370 "hello.asm" texto 150 100 600 260 "4 passes"
  miracap "y el HELLO.EXE que ha hecho FASMW corre: su MessageBox" f5 texto 250 240 400 290 "May I introduce myself?"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "DDRAW y FASMW: ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
fi
# [GL] 5. OpenGL (HispaGL: OPENGL32.DLL y GLU32.DLL de Hispa-486, las mismas):
# GLPRUEBA en su ventana y a pantalla completa (ChangeDisplaySettingsA y el
# DirectDraw de HW32), los engranajes y la sala de medir; los cuadros por
# segundo, por la traza de HispaGL (HISPAGL=TRAZA en \WIN32\ENTORNO.TXT, en
# una copia del disco); los colores, por clases (tests/gl/compara.py --cuenta)
cp $V/d.img $V/gl.img
printf 'HISPA32=TRAZA\r\nHISPAGL=TRAZA\r\n' > $V/ENTORNO.TXT
mcopy -o -i $V/gl.img $V/ENTORNO.TXT ::WIN32/ENTORNO.TXT
CAPTURAS=$G DISCO=$V/gl.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
  ":escribe \\HISPA32\\GLPRUEBA /PRUEBA" ":tecla ret" ":pausa 25" ":captura g1" ":tecla esc" ":pausa 3" \
  ":escribe \\HISPA32\\GLPRUEBA /COMPLETA /PRUEBA" ":tecla ret" ":pausa 25" ":captura g2" ":tecla esc" ":pausa 3" ":captura g3" \
  ":escribe \\HISPA32\\ENGRANA" ":tecla ret" ":pausa 30" ":captura g4" ":tecla esc" ":pausa 3" \
  ":escribe \\HISPA32\\GLPRUEBA /SALA /ANCHO 320 /ALTO 240" ":tecla ret" ":pausa 30" ":captura g5" ":tecla esc" ":pausa 3" \
  ":escribe \\PRUEBAS\\WGLGEARS" ":tecla ret" ":pausa 30" ":captura g6" ":tecla esc" ":pausa 3" > $V/gl.sal 2>&1
S=$V/gl.sal
glcuenta() { local f=$1; shift; if python3 -I $AQUI/../tests/gl/compara.py --cuenta $G/$f.ppm "$@" > $V/glc.txt; then ok "$(sed 's/^ *OK *//' $V/glc.txt)"; else ko "$(sed 's/^ *FAIL *//' $V/glc.txt)"; fi; }
glcps() { awk -v p="$1" '/wglCreateContext/{n++} n==p && /cuadros por 10 s/{v=$NF} END{print v}' $S | tr -d '\r'; }
glcuenta g1 0 0 800 600 rojo 300 8000 "GLPRUEBA en su ventana: la esfera roja (luz, brillo; gluSphere de GLU32)"
glcuenta g1 0 0 800 600 amarillo 300 8000 "la estrella amarilla (prueba de alfa) y el texto (wglUseFontBitmapsA)"
glcuenta g1 0 0 800 600 baldosa 15000 200000 "el suelo de baldosas (gluBuild2DMipmaps, niebla)"
glcuenta g2 80 60 640 480 rojo 1000 20000 "GLPRUEBA a pantalla completa (ChangeDisplaySettingsA + DirectDraw): la esfera, al doble"
glcuenta g2 0 0 800 60 negro 40000 48000 "y el borde negro de arriba (320×240 al doble, en el centro)"
glcuenta g4 0 0 800 600 rojo 6000 60000 "ENGRANA: el engranaje rojo (listas, luz, sombreado plano y suave)"
glcuenta g4 0 0 800 600 verde 1500 30000 "el verde"
glcuenta g5 0 0 800 600 ladrillo 15000 120000 "la sala de medir: las paredes de ladrillo (multitextura: la pared por su mapa de luz)"
GLP=(3:ENGRANA:300×300 4:"la sala":320×240)
if mdir -i $V/gl.img ::PRUEBAS/WGLGEARS.EXE > /dev/null 2>&1; then
  glcuenta g6 0 0 800 600 verde 2000 40000 "WGLGEARS (los engranajes de los demos de Mesa, MIT: un programa de otros, con WGL)"
  GLP+=(5:WGLGEARS:300×300)
else echo "  (sin wglgears: tests/thirdparty/fetch_gl.sh)"; fi
for p in "${GLP[@]}"; do
  c=$(glcps ${p%%:*}); q=${p#*:}
  if [ -n "$c" ]; then ok "${q%%:*}: $(( 16#$c / 10 )),$(( 16#$c % 10 )) cuadros por segundo en QEMU (${q#*:}, calidad normal)"
  else ko "${q%%:*}: la traza no dice los cuadros por segundo"; fi
done
if ! grep -q "excepción\|\*\*\*" $S; then ok "OpenGL: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 4. la red de los programas de Windows: WSOCK32 (REDPRUE) contra el servidor de aquí
mkdir -p $V/srv
python3 -I $AQUI/nucleo32/servidor_prueba.py $V/srv > $V/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $V/srv/puerto ] && break; sleep 0.1; done
PU=$(cat $V/srv/puerto 2> /dev/null)
DISCO=$V/d.img ARRANQUE=ordenes MODULOS= MEMORIA=64 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN "cd \\PRUEBAS" ":pausa 3" \
  "REDPRUE 10.0.2.2 $PU /grande.bin" ":pausa 20" "REDPRUE 10.0.2.2 $PU /trozos.bin P" ":pausa 20" > $V/red.sal 2>&1
kill $SPID 2> /dev/null
tr -d '\r' < $V/red.sal > $V/red.txt
S=$V/red.txt
SUMA=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(len(d), sum(d))" $V/srv/grande.bin 2> /dev/null)
mira "WSOCK32 sobre el servicio de red: la IP (HispaRedConfig), el DNS, una descarga de 400 KB por TCP con su suma (la del archivo: $SUMA)" \
  "^IP 10.0.2.15 puerta 10.0.2.2 MAC 52:54:00:12:34:56$" "^Respuesta: HTTP/1.1 200 OK$" "^Cuerpo: ${SUMA% *} octetos, suma ${SUMA#* }$"
mira "y sin bloqueo: un puerto cerrado (WSAECONNREFUSED), connect y select, y otra descarga" "^Puerto cerrado: -1, error 10061$" "^select: 1 (escribir 1, fallo 0)$" "^Cuerpo: 150508 octetos"
else
  ko "nucleo32/win32/disco.sh no puede hacer el disco: $(tail -n 2 $V/disco.txt)"
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los programas de Windows)"
fi   # [WIN32] los programas de Windows
# ------------------------------------------------- 33. (JUEGOS) lo que pide un juego de Windows
# (JUEGOS, SOLO=win32juegos) Lo que necesita un juego de Windows de 1998-2000
# (Half-Life y los de su tiempo: Win32, Visual C++ 6, OpenGL o DirectDraw),
# con los programas de prueba de tests/juegos/ (gcc y tools/elf2pe.py:
# castel/nucleo32/win32/juegos.sh): el diagnóstico (FALTA, SINDLL y WIN32
# COMPRUEBA, con el registro WIN32.LOG), los hilos de verdad (HILOS), SEH
# (SEH), el registro (REG, y que siga al volver), MSVCRT de VC6 (CRT), la
# ventana a pantalla completa con el ratón de los juegos y DirectInput
# (JUEGOV, moviendo el ratón de QEMU), UDP en red local como cliente y como
# servidor (REDUDP, con tests/juegos/eco_udp.py) y el sonido (SONJ: la
# mezcla en su hilo, waveOut con su evento y el CD de sonido por MCI). (HL)
# Lo que pedía Half-Life 1: HLK32, HLORD (Winsock por número), HLMM (mmio y
# el mezclador), HLNET (WININET, sin red y contra el servidor HTTP de aquí),
# HLVEN (en el escritorio) y el juego de mentira JUEGO99 (WIN32 COMPRUEBA de
# su carpeta, /CON y /MAQUINA: «está todo»).
if parte08 win32juegos; then
echo "== (JUEGOS) Hispa-Pentium: lo que pide un juego de Windows (diagnóstico, hilos, SEH, registro, MSVCRT de VC6, ventanas y ratón, UDP, sonido y CD) =="
J=$P/jue
mkdir -p $J $G
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/win32/disco.sh $J/d.img > $J/disco.txt 2>&1 && bash $AQUI/nucleo32/win32/juegos.sh $J/jp > $J/jp.txt 2>&1; then
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $J/semilla.asm > /dev/null 2>&1 && nasm -f bin $J/semilla.asm -o $J/SEMILLA.BIN
mmd -i $J/d.img ::JUEGOS
mcopy -s -i $J/d.img $J/jp/* ::JUEGOS/
ok "nucleo32/win32/juegos.sh: $(ls $J/jp/*.EXE | wc -l) programas de prueba (gcc -m32 -march=i586 y elf2pe.py, con MSVCRT.DLL)"
# 1. en el intérprete de órdenes: el diagnóstico, los hilos, SEH, el registro y la biblioteca de C
cp $J/d.img $J/c.img
O=("cd \\JUEGOS" "FALTA" ":pausa 3" "SINDLL" ":pausa 3" "WIN32 COMPRUEBA FALTA.EXE SINDLL.EXE" ":pausa 4" "WIN32 COMPRUEBA C:\\JUEGOS" ":pausa 6"
   "HILOS" ":pausa 15" "HILOS /SALIR" ":pausa 4" "HILOS /DEJA" ":pausa 4" "SEH" ":pausa 6" "SEH /SOLO" ":pausa 3" "SEH /FILTRO" ":pausa 3"
   "REG" ":pausa 4" "REG /LEE" ":pausa 3" "CRT uno \"dos tres\" cuatro" ":pausa 6"
   "WIN32 COMPRUEBA FALTA.EXE SINDLL.EXE /MAQUINA" ":pausa 4" "HLK32" ":pausa 6" "HLK32 /FATAL" ":pausa 3" "HLORD" ":pausa 4" "HLMM" ":pausa 5"
   "HLNET /SINRED" ":pausa 5" "cd \\JUEGOS\\JUEGO99" "WIN32 COMPRUEBA C:\\JUEGOS\\JUEGO99" ":pausa 5" "WIN32 COMPRUEBA JUEGO.EXE /CON C:\\JUEGOS\\JUEGO99" ":pausa 5"
   "WIN32 COMPRUEBA JUEGO.EXE /MAQUINA" ":pausa 4" "JUEGO" ":pausa 5"
   "cd \\PRUEBAS" "K32PRUE" ":pausa 5" "HOLAC" ":pausa 2")
ESCRIBE=1 DISCO=$J/c.img ARRANQUE=ordenes MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $J/SEMILLA.BIN "${O[@]}" > $J/consola.sal 2>&1
tr -d '\r' < $J/consola.sal | grep -v '^\[w32\]' > $J/consola.txt
S=$J/consola.txt
mira "una función que falta, llamada: el aviso dice cuál (DLL!función) y desde dónde, y dónde queda apuntado; lo que no está por GetProcAddress, nulo" \
  "^getprocaddress_nohay: nulo$" "^Falta la función KERNEL32.DLL!FuncionQueNoHay$" "FALTA.EXE la ha llamado (desde FALTA.EXE+0x" "Queda apuntado en C:.USUARIOS.USUARIO.WIN32.LOG" "(FALTA.EXE ha acabado con el código 255)"
mira "una DLL que no está: lo dice («No se encuentra NOHAY.DLL, que necesita SINDLL.EXE»)" "^No se encuentra NOHAY.DLL, que necesita SINDLL.EXE.$" "(SINDLL.EXE ha acabado con el código 255)"
mira "WIN32 COMPRUEBA: las importaciones que faltan, sin ejecutar (las de una DLL de dentro, las de una de fuera y la DLL que no hay)" \
  "^FALTA.EXE: programa de consola$" "^  falta WINMM.DLL!mixerNoHay$" "^  falta KERNEL32.DLL!FuncionQueNoHay$" "^  FALTA LA DLL NOHAY.DLL (1 función)$" "^En total: 2 archivos; faltan 3 funciones"
mira "y de una carpeta entera (sus .EXE y sus .DLL, también los de dentro): HILOS, SEH, CRT, REG... sin nada que falte" \
  "^HILOS.EXE: programa de consola$" "^JDLL.DLL: DLL$" "^SEH.EXE: programa de consola$"
if [ "$(sed -n '/COMPRUEBA C:.JUEGOS/,/^En total/p' $S | grep -c '^  falta ')" = 2 ]; then ok "en C:\\JUEGOS, solo faltan las dos de FALTA.EXE (y la DLL de SINDLL)"; else ko "WIN32 COMPRUEBA C:\\JUEGOS: $(sed -n '/COMPRUEBA C:.JUEGOS/,/^En total/p' $S | grep '^  falta ' | head -5 | tr '\n' ' ')"; fi
if mtype -i $J/c.img ::USUARIOS/USUARIO/WIN32.LOG > $J/win32.log 2>/dev/null && LC_ALL=C grep -q "FALTA.EXE: LLAMA A UNA QUE FALTA: KERNEL32.DLL!FuncionQueNoHay" $J/win32.log \
   && LC_ALL=C grep -q "FALTA.EXE: GetProcAddress(KERNEL32.DLL, OtraQueNoHay)" $J/win32.log && LC_ALL=C grep -q "no se encuentra NOHAY.DLL, que necesita SINDLL.EXE" $J/win32.log \
   && LC_ALL=C grep -q "EXCEPCI.N SIN MANEJADOR: 0xc0000005" $J/win32.log; then
  ok "\\USUARIOS\\USUARIO\\WIN32.LOG apunta, con la fecha y el programa, lo que falta, lo que falló y las excepciones sin manejador"
else ko "WIN32.LOG no tiene lo que debe: $(head -c 300 $J/win32.log 2>/dev/null)"; fi
mira "hilos de verdad: cuatro que suman a la vez (Interlocked y CRITICAL_SECTION), esperar a todos, sus códigos y números, TLS de cada uno" \
  "^esperar_a_todos: 0$" "^interlocked: 80000$" "^critical_section: 80000$" "^codigos: 46$" "^ids_distintos: si$" "^tls_principal: 7$"
mira "un hilo que no llama a nada corre a la vez que el principal (con el de HW32, que corre los hilos enteros, se quedaría colgado)" "^sin_fin_sigue: si$" "^sin_fin_acaba: 0 5$"
mira "semáforos, eventos automáticos y manuales, esperar con tiempo" "^semaforo: 0 10$" "^evento_auto_se_apaga: 258$" "^semaforo_demasiado: 0 298$" "^manual_antes: 0$" "^manual_despues: 0 3$" "^manual_sigue_puesto: 0$" "^espera_60ms: 258 bien$"
mira "mutex: con dueño, recursivo, ERROR_NOT_OWNER, por su nombre (ERROR_ALREADY_EXISTS) y abandonado (WAIT_ABANDONED)" \
  "^mutex_nuevo: 0$" "^mutex_recursivo: 0$" "^mutex_suelta: 1 1 0 288$" "^mutex_mismo_nombre: 183 el mismo$" "^mutex_abre: si$" "^mutex_abandonado: 80$"
mira "Sleep(0) cede, las prioridades se guardan, CREATE_SUSPENDED y ResumeThread, ExitThread con su código" "^sleep0: 0 200 200$" "^prioridad: 1 15$" "^dormido: 258$" "^exitthread: 0 77$"
mira "__declspec(thread) (TLS de la imagen): cada hilo, su copia; y sus funciones (DLL_THREAD_ATTACH y DETACH)" "^tls_imagen: 0 100 plantilla$" "^tls_imagen_hilo: 100 200 5$" "^tls_funciones: 1 1 1$" "^FIN HILOS$"
mira "un hilo acaba el proceso (ExitProcess(33)) con el principal esperando; y el principal acaba con un hilo que no acaba nunca" \
  "^hilo: ExitProcess(33)$" "(HILOS.EXE ha acabado con el código 33)" "^deja: el principal acaba con un hilo que no acaba$" "(HILOS.EXE ha acabado con el código 4)"
mira "SEH: el puntero nulo (escribir y leer), la memoria que no hay, dividir por cero (seguir con otro divisor), ud2, int 3, cli, RaiseException con sus datos" \
  "^nulo_escritura: c0000005 1 00000000 55$" "^nulo_lectura: c0000005 0 00000400$" "^sin_memoria: c0000005 6fff0000$" "^division: c0000094 25$" "^ud2: c000001d$" "^int3: 80000003$" "^cli: c0000096$" "^raise: e0001234 2 11 22$" "^cadena_vacia: si$"
mira "SEH: dos marcos con RtlUnwind; el __try/__except y el __try/__finally de VC6 (_except_handler3 de MSVCRT); en otro hilo; longjmp repone la cadena; IsBadReadPtr" \
  "^dos_marcos: c0000005 1 bien$" "^vc6_except: 1 42$" "^vc6_finally: 43 1$" "^en_otro_hilo: 42$" "^longjmp_cadena: bien$" "^isbad: 1 0 1 1$" "^FIN SEH$"
mira "una excepción sin manejador: el aviso claro (qué y dónde); con SetUnhandledExceptionFilter, su filtro" \
  "^SEH.EXE ha fallado: EXCEPTION_ACCESS_VIOLATION (un puntero que no vale) en SEH.EXE+0x.*, al escribir en 0x00000024.$" "^filtro_final: c0000005 en su sitio$" "(SEH.EXE ha acabado con el código 3221225477)"
mira "el registro: crear (REG_CREATED_NEW_KEY), poner y leer DWORD, texto y binario, el búfer corto (ERROR_MORE_DATA), el valor sin nombre, borrar, enumerar, Unicode, HKLM y HKCR" \
  "^abrir_nohay: 2$" "^crear: 0 1$" "^tam_sin_bufer: 0 1 7$" "^bufer_corto: 234 7$" "^texto: 0 hw.dll 7$" "^binario: 0 3 4 0102feff$" "^sin_nombre: 0 por defecto$" "^borrar_valor: 0 2$" \
  "^info: 1 5 12 21$" "^unicode: 0 14 hw\.$" "^enum_claves: 0 Half-Life 259$" "^directx: 0 4.07.00.0700$" "^hkcr: 0 HispaJuego$" "^borrar_clave: 0 2$"
mira "y al correr otra vez, sigue ahí (REG_OPENED_EXISTING_KEY)" "^lee_otra_vez: 0 4 640$" "^lee_texto: 0 hw.dll 7$" "^lee_borrado: 2$" "^crear_existente: 0 2$"
if mtype -i $J/c.img ::USUARIOS/USUARIO/REGISTRO.REG > $J/registro.reg 2>/dev/null && grep -q '^"ScreenWidth"=dword:00000280' $J/registro.reg && grep -q '^\[HKEY_CURRENT_USER.Software.Valve.Half-Life.Settings\]' $J/registro.reg \
   && grep -q '^"Clave"=hex:01,02,fe,ff' $J/registro.reg && mtype -i $J/c.img ::USUARIOS/EQUIPO.REG 2>/dev/null | grep -q '^\[HKEY_LOCAL_MACHINE.Software.Classes..hlj\]'; then
  ok "en el disco: \\USUARIOS\\USUARIO\\REGISTRO.REG (HKCU) y \\USUARIOS\\EQUIPO.REG (HKLM), de texto como los de REGEDIT4"
else ko "REGISTRO.REG o EQUIPO.REG no están como deben: $(head -c 300 $J/registro.reg 2>/dev/null)"; fi
mira "MSVCRT de VC6: __getmainargs con comillas, _initterm, WinMain con su lpCmdLine (como lo saca el arranque de VC6)" "^argc: 4$" "^argv2: \[dos tres\]$" "^initterm: 11$" "^winmain: \[uno \"dos tres\" cuatro\] hinstance 10$"
mira "stdio en texto (\\n como \\r\\n en el disco) y en binario, sprintf con coma flotante, sscanf, _findfirst, la hora" \
  "^texto_en_disco: 10 0d0a$" "^texto_leido: 8 0a$" "^binario: 5 b$" "^sprintf: 3.141593|-2.500|  7.3|1.234568e+04|0.0001|1e+20|3|1.00    |+3.00$" "^sscanf: 2 12 3.5$" "^findfirst: varios CRT.EXE$" "^findfirst_nada: -1$" "^time: 2026 o despues fecha$" "^strdate: 8$"
mira "qsort, bsearch, setjmp y longjmp, _beginthreadex y _beginthread, _ftol y _CIpow/_CIsqrt del compilador, new de C++, _splitpath, _ecvt, Unicode; atexit al revés" \
  "^qsort: 12345789 kiwi uva 5$" "^longjmp: 5$" "^beginthreadex: 40 9 con id$" "^beginthread: 42$" "^ftol: -3 00000002540be400$" "^cipow: 1024 141421$" "^new: si 3$" \
  "^splitpath: C:|.JUEGOS.VALVE.|hl|.exe D:.OTRA.hl.dll$" "^ecvt: 12346 3 1$" "^unicode: 8 vida=100$" "^FIN CRT$" "^atexit: segundo puesto, el primero$" "^atexit: primero puesto, el ultimo$"
# (HL) lo que pedía Half-Life 1
TB=$'\t'
mira "(HL) WIN32 COMPRUEBA /MAQUINA: una línea por hecho, con tabuladores (FALTA, FALTADLL, RESUMEN, TOTAL)" \
  "^ARCHIVO${TB}C:.JUEGOS.FALTA.EXE${TB}consola${TB}[0-9]*${TB}[0-9a-f]\{64\}$" "^FALTA${TB}C:.JUEGOS.FALTA.EXE${TB}WINMM.DLL${TB}mixerNoHay$" \
  "^FALTADLL${TB}C:.JUEGOS.SINDLL.EXE${TB}NOHAY.DLL${TB}1$" "^RESUMEN${TB}C:.JUEGOS.FALTA.EXE${TB}[0-9]*${TB}[0-9]*${TB}2$" "^TOTAL${TB}2${TB}3${TB}1$"
mira "(HL) KERNEL32: _lcreat, _lwrite, _llseek, _lopen y _lread (solo para leer: no escribe), OpenFile (OF_EXIST con la ruta entera, el error, leer, OF_DELETE)" \
  "^lcreat: si 14$" "^llseek: 5 14$" "^lopen: 4 \[Life\] 1$" "^lopen_nohay: -1$" "^openfile_existe: 1 ruta entera$" "^openfile_nohay: -1 2$" "^openfile_lee: 4 \[Half\]$" "^openfile_borra: 1 -1$"
mira "(HL) KERNEL32: OpenFileMapping por su nombre (el mismo trozo; sigue al cerrar el primero; ERROR_ALREADY_EXISTS), SetEnvironmentVariableW, SetVolumeLabel, GetProcessVersion, EnumSystemLocales" \
  "^openfilemapping: si \[visto por los dos\]$" "^mapa_sigue: \[visto por los dos\]$" "^mapa_ya_existe: 183$" "^openfilemapping_nohay: nulo 2$" "^setenvw: 1 \[valve\]$" \
  "^setvolumelabel: 0 50$" "^getprocessversion: 00040000 00040000$" "^enumsystemlocales: 1 4 con el de Espa"
mira "(HL) USER32: WaitForInputIdle, ExitWindowsEx (no: ERROR_PRIVILEGE_NOT_HELD), DDE que falla limpio (DMLERR_NO_CONV_ESTABLISHED) con sus cadenas y datos, los lParam de DDE" \
  "^waitforinputidle: 0$" "^exitwindowsex: 0 1314$" "^ddeinitialize: 0 con id$" "^ddestring: 8 \[Servidor\] distintas$" "^ddeconnect: nulo 400a$" "^ddetransaction: nulo$" \
  "^ddedata: 13 \[datos de dde\]$" "^ddeuninitialize: 1$" "^unpackddelparam: 1 1234 56789abc$" "^reuseddelparam: 7 11223344 1$" "^getlastactivepopup_nohay: nulo$"
mira "(HL) GDI32 (ScaleWindowExtEx y ScaleViewportExtEx en MM_TEXT, Escape: no, GetBoundsRect) y WINSPOOL.DRV (DocumentProperties: no hay impresoras)" \
  "^scalewindowextex: 1 1 1$" "^scaleviewportextex: 1 1 1$" "^escape: 0 0$" "^getboundsrect: 9 0 0$" "^documentproperties: -1 1801$"
mira "(HL) ADVAPI32, la seguridad: descriptores sin dueño ni DACL (todo permitido), SID y ACL de verdad, la de los objetos y archivos, sin testigos, los privilegios" \
  "^initializesd: 1 20 1$" "^sd_owner: 1 nulo 0$" "^sd_group: 1 nulo$" "^sd_dacl: 1 0 nulo$" "^sd_sacl: 1 0$" "^sd_control: 1 0 1$" "^sd_pon_dacl: 1$" "^sid: 1 1 16$" \
  "^equalsid: 1$" "^acl: 1 1 1 1$" "^acl_mala: 0$" "^kernelobjectsecurity: 0 122 20 1 1$" "^filesecurity: 1 1$" "^openprocesstoken: 0 120$" "^privilegios: 1 19 1 0$" "^privilegio_nohay: 0 1313$" "^FIN HLK32$"
mira "(HL) FatalAppExit: su aviso y acaba (255)" "^HLK32.EXE ha tenido que acabar:$" "(HLK32.EXE ha acabado con el código 255)"
mira "(HL) Winsock por número (WSOCK32 y WS2_32, que tienen otros del 10 al 12) y GetProcAddress con un número" \
  "^wsastartup: 0 101$" "^htons: 3412 1234$" "^htonl: 04030201$" "^inet_addr: 0f02000a 192.168.1.20$" "^wsalasterror: 10035$" "^gethostname: 0 con nombre$" \
  "^getprotobynumber: nulo$" "^socket_udp: si$" "^ioctlsocket: 0$" "^wsafdisset: 1 0$" "^ws2_32: 0202000a 10.0.2.15 01020304$" \
  "^getprocaddress_numero: inet_addr gethostbyname nulo$" "^getprocaddress_ws2: otra inet_addr$" "^wsacleanup: 0$" "^FIN HLORD$"
mira "(HL) WINMM, mmio: un WAV hecho con mmioCreateChunk y mmioAscend (el tamaño y el relleno) y leído con mmioDescend (FINDRIFF, FINDCHUNK), mmioRead, mmioSeek, el acceso directo y en memoria" \
  "^mmio_escribe: si 0 0 0 0 201$" "^mmio_cierra: 0$" "^mmio_riff: 0 238 8$" "^mmio_fmt: 0 16 11025 16 1$" "^mmio_data: 0 201 8 100 300$" "^mmio_seek: 8 246$" "^mmio_nohay: 265$" \
  "^mmio_getinfo: RIFF 246$" "^mmio_setinfo: 8$" "^mmio_advance: 0$" "^mmio_nohay_archivo: nulo$" "^mmio_existe: 1 0$" "^mmio_memoria: si 0 0$" "^fourcc: 45564157$"
mira "(HL) WINMM, el mezclador (los altavoces y la onda, sus controles, poner y leer el volumen) y lo que no hay (waveIn, midiOut y aux: limpio)" \
  "^mixer: 1 0 0 1 Mezclador de Hispa$" "^mixer_linea: 0 0 2 1 Altavoces$" "^mixer_onda: 0 10000 80000001$" "^mixer_micro: 1024$" "^mixer_control: 0 1 0 65535 Volumen general$" \
  "^mixer_volumen: 0 0 la mitad$" "^mixer_control_nohay: 1025$" "^mixer_cierra: 0$" "^mcigetdeviceid: 0$" "^wavein: 0 6 nulo 6$" "^wavein_texto: No hay dispositivo (en Hispa a.*n no hay micr.*fono ni MIDI)\.$" \
  "^midiout: 0 6 nulo 6$" "^aux: 0 2 2$" "^FIN HLMM$"
mira "(HL) WININET sin red: InternetGetConnectedState dice que no (INTERNET_CONNECTION_OFFLINE), InternetCrackUrl, y bajar algo: ERROR_INTERNET_DISCONNECTED" \
  "^conectado: 0 20$" "^internetopen: si$" "^crackurl: 1 3 servidores.ejemplo.es 27950 /lista.txt$" "^sin_red: -12163$" "^FIN HLNET$"
mira "(HL) el juego de mentira de 1999 (el .EXE y su VGUI.DLL en la raíz, las del mod en MOD\\CL_DLLS y MOD\\DLLS, Winsock por número): WIN32 COMPRUEBA de su carpeta, está todo" \
  "^(las DLL, desde la carpeta del programa: C:.JUEGOS.JUEGO99.)$" "^CLIENTE.DLL: DLL$" "^SERVIDOR.DLL: DLL$" "^VGUI.DLL: DLL$" "^JUEGO.EXE: programa de consola$" "^En total: 4 archivos; está todo (no falta nada).$"
if [ "$(grep -c '^En total: 4 archivos; está todo (no falta nada).$' $S)" = 2 ] && ! sed -n '/COMPRUEBA C:.JUEGOS.JUEGO99/,/^FIN JUEGO/p' $S | grep -q '^  falta\|FALTA'; then
  ok "(HL) y con «JUEGO.EXE /CON C:\\JUEGOS\\JUEGO99» (la carpeta del .EXE para las de las subcarpetas): está todo; VGUI.DLL ya no sale como falta"
else ko "(HL) WIN32 COMPRUEBA del juego de mentira: $(sed -n '/COMPRUEBA C:.JUEGOS.JUEGO99/,/^FIN JUEGO/p' $S | grep '^  falta\|FALTA\|En total' | head -5 | tr '\n' ' ')"; fi
SH=$(sha256sum $J/jp/JUEGO99/JUEGO.EXE | cut -d' ' -f1)
mira "(HL) /MAQUINA del juego de mentira: su SHA-256 (el de aquí: ${SH:0:16}...), de dónde sale cada DLL (VGUI.DLL de la carpeta del .EXE) y nada que falte" \
  "^PROGRAMA${TB}C:.JUEGOS.JUEGO99.$" "^ARCHIVO${TB}C:.JUEGOS.JUEGO99.JUEGO.EXE${TB}consola${TB}$(stat -c %s $J/jp/JUEGO99/JUEGO.EXE)${TB}$SH$" \
  "^USA${TB}C:.JUEGOS.JUEGO99.JUEGO.EXE${TB}VGUI.DLL${TB}C:.JUEGOS.JUEGO99.VGUI.DLL$" "^USA${TB}C:.JUEGOS.JUEGO99.JUEGO.EXE${TB}WSOCK32.DLL${TB}WIN32.BIN$" \
  "^RESUMEN${TB}C:.JUEGOS.JUEGO99.JUEGO.EXE${TB}9${TB}[0-9]*${TB}0$" "^TOTAL${TB}2${TB}0${TB}0$"
mira "(HL) y al correr: carga las del mod por su ruta (desde otra carpeta de trabajo); la del cliente encuentra VGUI.DLL junto al .EXE, la del servidor usa Winsock por número" \
  "^juego_wsastartup: 0 6c1b 0202000a$" "^juego_vgui: 41$" "^juego_openfile: -1$" "^juego_mixer: 0$" "^juego_cliente: cargada 1011$" "^juego_servidor: cargada 1111$" "^FIN JUEGO$"
mira "y lo de antes sigue igual (K32PRUE y HOLAC, con los hilos y el cerrojo grande)" "^tls: si 1234$" "^interlocked: 6 6 10 20$" "^FIN K32PRUE$" "^Primo 100: 541$"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en toda esa sesión, ni una excepción ni una queja del núcleo (las de SEH las recoge el programa)"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 2. en el escritorio: la ventana del juego a pantalla completa y el ratón
cp $J/d.img $J/v.img
M=(); for _ in 1 2 3 4 5 6 7 8 9; do M+=(":monitor mouse_move 100 0"); done
CAPTURAS=$G ESCRIBE=1 DISCO=$J/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $J/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" \
  ":escribe \\JUEGOS\\JUEGOV" ":tecla ret" ":pausa 5" "${M[@]}" ":pausa 3" "${M[@]}" ":pausa 6" \
  ":escribe \\JUEGOS\\HLVEN" ":tecla ret" ":pausa 8" > $J/juegov.sal 2>&1
mtype -i $J/v.img ::JUEGOS/JUEGOV.TXT 2>/dev/null | tr -d '\r' > $J/juegov.txt
S=$J/juegov.txt
mira "GetVersionEx (4.10), QueryPerformanceCounter y timeGetTime a su paso (Sleep(100): 100 ms), timeBeginPeriod" "^version: 4.10 1$" "^timebeginperiod: 0$" "^qpc: mhz sube$" "^sleep100: bien bien$"
mira "la DLL del juego por su ruta relativa a la carpeta del .EXE (con otra carpeta de trabajo), GetProcAddress y GetModuleFileName" "^loadlibrary_relativa: si 5 su ruta$" "^getprocaddress_nohay: nulo$"
mira "una ventana WS_POPUP a pantalla completa (DirectDraw a 640 x 480): GetSystemMetrics y GetClientRect, en el modo del juego" "^ddraw: si 0 0$" "^pantalla: 640 480$" "^cliente: 0 0 640 480$"
mira "SetCursorPos y GetCursorPos (en el modo del juego), ShowCursor, ClipCursor" "^cursor_centro: 320 240$" "^showcursor: -1 -2$" "^clipcursor: 100 100$"
mira "mirar con el ratón como un motor de juegos (volviendo al centro en cada vuelta): más de lo que mide la pantalla" "^raton_al_centro: mas que la pantalla"
mira "y DirectInput (el ratón relativo, lo movido de verdad, sin topar con el borde)" "^dinput_raton: 0 0 0$" "^dinput_relativo: mas que la pantalla" "^FIN JUEGOV$"
mtype -i $J/v.img ::JUEGOS/HLVEN.TXT 2>/dev/null | tr -d '\r' > $J/hlven.txt
S=$J/hlven.txt
mira "(HL) GetLastActivePopup (la ventanita de encima del juego), ExcludeUpdateRgn (sin lo que está por repintar) y WSAAsyncGetHostByName (el aviso a la ventana, con su hostent)" \
  "^ventana: si$" "^lastactivepopup_sola: ella$" "^lastactivepopup: la de encima$" "^excludeupdatergn: [23] sin lo de repintar$" "^excludeupdatergn_nada: [123]$" \
  "^asyncgethostbyname: 0 [0-9]* 10.0.2.15 10.0.2.15$" "^FIN HLVEN$"
S=$J/juegov.sal
if ! grep -q "excepción\|\*\*\*" $S; then ok "JUEGOV: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 3. UDP: cliente (contra el eco de aquí) y servidor (en el 27015, llevado desde fuera)
mkdir -p $J/eco
FU=$((20000 + RANDOM % 20000))
python3 -I $AQUI/../tests/juegos/eco_udp.py $J/eco $FU > $J/eco.txt 2>&1 &
EPID=$!
for _ in $(seq 50); do [ -s $J/eco/puerto ] && break; sleep 0.1; done
PE=$(cat $J/eco/puerto 2> /dev/null)
mkdir -p $J/srv                     # (HL) y el servidor HTTP de aquí, para WININET
python3 -I $AQUI/nucleo32/servidor_prueba.py $J/srv > $J/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $J/srv/puerto ] && break; sleep 0.1; done
PU=$(cat $J/srv/puerto 2> /dev/null)
DISCO=$J/d.img ARRANQUE=ordenes MODULOS= MEMORIA=64 QEMU_MAS="-nic user,model=e1000,hostfwd=udp::$FU-:27015" $AQUI/nucleo32/probar.sh $J/SEMILLA.BIN \
  "cd \\JUEGOS" ":pausa 3" "REDUDP $PE" ":pausa 15" "HLNET 10.0.2.2 $PU" ":pausa 25" > $J/red.sal 2>&1
kill $EPID $SPID 2> /dev/null
tr -d '\r' < $J/red.sal | grep -v '^\[w32\]' > $J/red.txt
S=$J/red.txt
mira "UDP de cliente: sendto y recvfrom con el eco, gethostbyname del propio equipo, getsockname" "^eco: 13 \[eco: hola eco\] de 10.0.2.2:$PE$" "^mi_ip: 10.0.2.15$" "^getsockname: puerto propio$"
mira "sin bloqueo (FIONBIO): WSAEWOULDBLOCK al momento (100 veces, deprisa); select y FIONREAD con UDP; la difusión" "^sin_bloqueo: -1 10035 deprisa$" "^select: 1 13$" "^tras_select: 13 \[eco: otra vez\]$" "^difusion: 9$"
mira "de servidor: bind al 27015 (y otro al mismo, WSAEADDRINUSE), y contesta al que llega sin haberlo pedido" "^bind: 0$" "^bind_ocupado: -1 10048$" "^servidor_recibe: 13 \[hola servidor\]$" "^servidor_contesta: 22$" "^FIN REDUDP$"
if [ "$(cat $J/eco/respuesta 2> /dev/null)" = "respuesta del servidor" ]; then ok "y la respuesta llega al otro equipo (aquí, por el puerto de QEMU)"; else ko "la respuesta del servidor no llega: $(cat $J/eco.txt)"; fi
SG=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(len(d), sum(d))" $J/srv/grande.bin 2> /dev/null)
ST=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(len(d), sum(d))" $J/srv/trozos.bin 2> /dev/null)
mira "(HL) WININET con red: InternetOpenUrl de 400 KB con Content-Length ($SG), en trozos ($ST), la redirección (302) y un 404" \
  "^conectado: 1 2$" "^grande: $SG 200$" "^trozos: $ST 200$" "^redirige: 41 200$" "^no_esta: 404$"
mira "(HL) InternetConnect, HttpOpenRequest, HttpSendRequest, HttpQueryInfo (el texto, una cabecera por su nombre); HTTPS y FTP, aún no (ERROR_INTERNET_CANNOT_CONNECT)" \
  "^httpsendrequest: 1 \[OK\] 41 con su texto$" "^httpqueryinfo_custom: 1 41$" "^https: -12029$" "^ftp: -12029$" "^FIN HLNET$"
# 4. el sonido: DirectSound en bucle sin que el programa haga nada, waveOut, el CD
if bash $AQUI/nucleo32/sonido/hacer.sh $J/son > $J/son.txt 2>&1; then
  cp $J/d.img $J/s.img
  mcopy -o -i $J/s.img $J/son/SONIDO.BIN $J/son/INICIO.WAV $J/son/APAGAR.WAV $J/son/AVISO.WAV $J/son/ERROR.WAV ::
  rm -f "${J:?}/son.wav"
  CAPTURAS=$G ESCRIBE=1 DISCO=$J/s.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-audiodev wav,id=snd0,path=$J/son.wav -device intel-hda -device hda-duplex,audiodev=snd0" \
    $AQUI/nucleo32/probar.sh $J/SEMILLA.BIN ":pausa 6" ":doble 60 186" ":pausa 4" ":escribe \\JUEGOS\\SONJ" ":tecla ret" ":pausa 14" > $J/sonj.sal 2>&1
  mtype -i $J/s.img ::JUEGOS/SONJ.TXT 2>/dev/null | tr -d '\r' > $J/sonj.txt
  python3 -I $AQUI/nucleo32/sonido/oye.py $J/son.wav 440,660 > $J/son.oye 2>&1
  S=$J/sonj.txt
  mira "DirectSound: un búfer de 1 s en bucle, GetCurrentPosition (da la vuelta, el de escribir va delante), GetStatus (sonando, en bucle)" "^crear_bufer: 0$" "^play: 0$" "^vueltas_en_1500ms: bien" "^escribir_delante: si$" "^estado: 5$"
  mira "waveOut con CALLBACK_EVENT: los cuatro trozos vuelven hechos, a su tiempo" "^waveout: 1 0$" "^waveout_hechos: 4 a su tiempo$"
  mira "el CD de sonido por MCI (sin disco en QEMU): open, «media present», el error de «number of tracks» con su texto, play y mode; y por mciSendCommand" \
    "^cd_open: 0$" "^cd_disco: 0 false$" "^cd_pistas: 276 \[\] No hay disco en el lector" "^cd_play: 276$" "^cd_modo: 0 not ready$" "^cd_mci_open: 0 con numero$" "^cd_mci_status: 0 0$" "^cd_mci_close: 0$" "^FIN SONJ$"
  r=$(awk '$6 >= 400 && $6 <= 480 && $5 >= 3300 && $9 == 0' $J/son.oye | head -1)
  if [ -n "$r" ]; then ok "suena el la de DirectSound 3,5 s seguidos, sin un corte, aunque el programa pase 2 s sin llamar a nada (la mezcla, en su hilo): $r"
  else ko "el la de DirectSound no suena seguido: $(cat $J/son.oye | head -5 | tr '\n' ' ')"; fi
  r=$(awk '$6 >= 600 && $6 <= 700 && $5 >= 850 && $5 <= 1300 && $9 == 0' $J/son.oye | head -1)
  if [ -n "$r" ]; then ok "y el mi de waveOut, 1 s: $r"; else ko "el mi de waveOut no suena: $(cat $J/son.oye | head -5 | tr '\n' ' ')"; fi
  S=$J/sonj.sal
  if ! grep -q "excepción\|\*\*\*" $S; then ok "SONJ: ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
else ko "nucleo32/sonido/hacer.sh no hace SONIDO.BIN: $(tail -n 2 $J/son.txt)"; fi
else
  ko "no se pueden hacer el disco de los programas de Windows o los de JUEGOS: $(tail -n 2 $J/disco.txt) $(tail -n 2 $J/jp.txt)"
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin lo que pide un juego de Windows)"
fi   # (JUEGOS)
# ------------------------------------------------- 28. (ACCESORIOS) los accesorios
# (ACCESORIOS, SOLO=accesorios) La Calculadora (CALC.BIN, ucalc.cas, con
# las cuentas decimales de decimal.cas), el Reloj (RELOJ.BIN, ureloj.cas) y
# el Mapa de caracteres (CARACTER.BIN, ucaract.cas, con caracnom.cas). Con
# corre32 (en Linux, deprisa): las cuentas, con lo que tienen que dar justo
# (0,1 + 0,2 = 0,3; el IVA; los céntimos redondeados a la mitad hacia
# fuera; la trigonometría en grados; los errores), el calendario y los
# festivos, y lo de cada carácter. Y en el escritorio (sus accesos de
# nucleo32/accesorios.py, con el icono en el tapiz): las tres ventanas, los
# modos de la Calculadora (por el menú Ver), la cinta y el IVA, copiar el
# resultado; la ñ copiada en el Mapa y pegada en el Bloc de notas; la alarma
# (RELOJ /ALARMA +0:40) con su aviso, la cuenta atrás y el calendario.
if parte08 accesorios; then
echo "== Hispa-Pentium: los accesorios (la Calculadora, el Reloj y el Mapa de caracteres) =="
A=$P/acc
rm -rf $A; mkdir -p $A $G
for u in ucalc:CALC ureloj:RELOJ ucaract:CARACTER; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $A/${u%%:*}.asm > /dev/null && nasm -f bin $A/${u%%:*}.asm -o $A/${u##*:}.BIN \
    || ko "${u%%:*}.cas no compila"
done
R=$AQUI/corre32/corre32.sh
# lo que sale, de la página de Hispa (la 437 con Á, €...) a UTF-8
hispa() { python3 -I -c "import sys; sys.path.insert(0, '$AQUI/../tools'); import hispa437; sys.stdout.write(''.join(hispa437.letra(b) for b in sys.stdin.buffer.read() if b != 13))"; }
# las cuentas: una por línea, «teclas|opciones|lo que tiene que salir»
cuentas() {
  local que=$1 t op esp r mal= n=0
  while IFS='|' read -r t op esp; do
    [ -z "$t" ] && continue
    # shellcheck disable=SC2086
    r=$($R $A/CALC.BIN /TECLAS "$t" $op 2>&1 | hispa | sed -n 's/^calc: visor: //p')
    n=$((n + 1))
    [ "$r" = "$esp" ] || mal="$mal «$t» $op dio «$r» (y es «$esp»);"
  done
  if [ -z "$mal" ]; then ok "$que ($n cuentas)"; else ko "$que:$mal"; fi
}
cuentas "la Calculadora estándar: decimal (0,1 + 0,2 = 0,3), de izquierda a derecha, el tanto por ciento, la memoria, «=» otra vez, los miles y los errores" <<'L'
0,1+0,2=||0,3
0,1*3=||0,3
1-0,9=||0,1
1/3=||0,3333333333333333
2+3*4=||20
10/4=||2,5
100+21%=||121
200-15%=||170
50*10%=||5
2{raiz}||1,414213562373095
1,21{cuad}||1,4641
4{inv}||0,25
1234567,5*2=||2.469.135
99999999*99999999=||9.999.999.800.000.001
2+3===||11
5{ms}3{m+}{mr}||8
7{mas}||-7
1/0=||No se puede dividir entre cero
4{mas}{raiz}||No existe (entrada no válida)
L
cuentas "la científica: la prioridad y los paréntesis, potencias y raíces, la trigonometría justa en grados (sen 30 = 0,5; tan 90 no existe) y en radianes, logaritmos, e^x, n! y el exponente" <<'L'
2+3*4=|/CIENTIFICA|14
2*(3+4)=|/CIENTIFICA|14
2(3+1)=|/CIENTIFICA|8
2^10=|/CIENTIFICA|1.024
2^3^2=|/CIENTIFICA|512
27{raizn}3=|/CIENTIFICA|3
30{sen}|/CIENTIFICA|0,5
60{cos}|/CIENTIFICA|0,5
45{tan}|/CIENTIFICA|1
390{sen}|/CIENTIFICA|0,5
30{mas}{sen}|/CIENTIFICA|-0,5
90{tan}|/CIENTIFICA|No existe (entrada no válida)
0,5{asen}|/CIENTIFICA|30
1{atan}|/CIENTIFICA|45
1{mas}{acos}|/CIENTIFICA|180
{pi}{sen}|/CIENTIFICA /RAD|0
{pi}{cos}|/CIENTIFICA /RAD|-1
1000{log}|/CIENTIFICA|3
0,001{log}|/CIENTIFICA|-3
2{log}|/CIENTIFICA|0,3010299956639812
2{ln}|/CIENTIFICA|0,6931471805599453
{e}{ln}|/CIENTIFICA|1
1{exp}|/CIENTIFICA|2,718281828459045
2^0,5=|/CIENTIFICA|1,414213562373095
20{fact}|/CIENTIFICA|2,43290200817664E+18
1,5{exp10}3{mas}*2=|/CIENTIFICA|0,003
0{ln}|/CIENTIFICA|No existe (entrada no válida)
L
cuentas "la de oficina: el IVA (21, 10 y 4 %, ponerlo y sacar la base) y los euros redondeados a la mitad hacia fuera (2,675 da 2,68; 1,005, 1,01)" <<'L'
100{iva+}|/OFICINA|121,00
121{iva-}|/OFICINA|100,00
{iva10}50{iva+}|/OFICINA|55,00
{iva4}10,4{iva-}|/OFICINA|10,00
99,99{iva+}|/OFICINA|120,99
2,675*1=|/OFICINA|2,68
1,005*1=|/OFICINA|1,01
3*1,255=|/OFICINA|3,77
12,5+7,25-3=|/OFICINA|16,75
0,1+0,2=|/OFICINA|0,30
L
$R $A/CALC.BIN /TECLAS "3*1,255+12,5={iva21}{iva+}" /OFICINA 2>&1 | hispa > $A/cinta.txt
S=$A/cinta.txt
mira "la cinta de la de oficina: cada número con su operación (el precio con sus tres decimales), el total y el desglose del IVA" \
  "^calc: cinta: 3,00 x$" "^calc: cinta: 1,255 +$" "^calc: cinta: 12,50 =$" "^calc: cinta: 16,27 TOTAL$" "^calc: cinta: 16,27 BASE$" \
  "^calc: cinta: 3,42 IVA 21 %$" "^calc: cinta: 19,69 TOTAL$" "^calc: Base 16,27 + IVA 21 % 3,42$" "^calc: visor: 19,69$"
{ $R $A/RELOJ.BIN /CALENDARIO 11/2026; $R $A/RELOJ.BIN /CALENDARIO 02/2028; $R $A/RELOJ.BIN /FESTIVOS 2027; } 2>&1 | hispa > $A/reloj.txt
S=$A/reloj.txt
mira "el Reloj en texto: el calendario (de lunes a domingo; 2028, bisiesto) y los festivos nacionales, con el Viernes Santo calculado" \
  "^reloj: noviembre de 2026$" "^reloj: lu ma mi ju vi sá do$" "^reloj:                    1$" "^reloj:  2  3  4  5  6  7  8$" "^reloj: 30$" \
  "^reloj: festivo el 1: Todos los Santos$" "^reloj:     1  2  3  4  5  6$" "^reloj: 28 29$" \
  "^reloj: Viernes, 26 de marzo de 2027: Viernes Santo$" "^reloj: Miércoles, 6 de enero de 2027: Epifanía del Señor (Reyes)$" \
  "^reloj: Martes, 12 de octubre de 2027: Fiesta Nacional de España$" "^reloj: Sábado, 25 de diciembre de 2027: Navidad$"
{ for c in 164 130 182 244 64 94; do $R $A/CARACTER.BIN /INFO $c; done; } 2>&1 | hispa > $A/caract.txt
n=$($R $A/CARACTER.BIN /LISTA 2>&1 | grep -c '^caracter: ')
S=$A/caract.txt
mira "el Mapa de caracteres en texto: el código, el Unicode, el nombre y cómo se escribe con el teclado español del núcleo" \
  "^caracter: 164 A4h U+00F1 ñ Eñe minúscula | tecla ñ$" "^caracter: 130 82h U+00E9 é E con tilde (minúscula) | tecla del acento y luego e$" \
  "^caracter: 182 B6h U+00C1 Á A con tilde (mayúscula) | (no se puede escribir)$" "^caracter: 244 F4h U+20AC € Euro | (no se puede escribir)$" \
  "^caracter: 64 40h U+0040 @ Arroba | Alt Gr + tecla 2$" "^caracter: 94 5Eh U+005E ^ Circunflejo | Mayús + tecla \` y luego espacio$"
[ "$n" = 166 ] && ok "y las 166 letras con dibujo de la letra del sistema (CARACTER /LISTA)" || ko "CARACTER /LISTA da $n letras (y son 166)"
# en el escritorio
export MTOOLS_SKIP_CHECK=1
if $AQUI/nucleo32/disco.sh $A/d.img 32 > $A/disco.txt 2>&1 && python3 -I $AQUI/nucleo32/accesorios.py $A/menu --raiz --escritorio; then
  printf 'argumentos=/ALARMA +0:40 Reuni\xa2n de la junta\r\n' >> $A/menu/RELOJ.MNU
  mmd -i $A/d.img ::MENU && mcopy -i $A/d.img $A/menu/* ::MENU/ && mcopy -i $A/d.img $A/CALC.BIN $A/RELOJ.BIN $A/CARACTER.BIN ::
  CAPTURAS=$G ESCRIBE=1 DISCO=$A/d.img ARRANQUE=escritorio MODULOS= RELOJ=2026-11-21T10:00:00 $AQUI/nucleo32/probar.sh "" \
    ":pausa 3" ":captura ac0" ":doble 63 258" ":pausa 3" ":raton 790 590" ":escribe 0,1+0,2=" ":pausa 1" ":captura ac1" \
    ":tecla alt-v" ":pausa 1" ":tecla down" ":tecla down" ":pausa 1" ":tecla ret" ":pausa 1" ":escribe 2+3*4=" ":escribe 30s" ":pausa 2" ":captura ac2" \
    ":tecla alt-v" ":pausa 1" ":tecla down" ":tecla down" ":tecla down" ":pausa 1" ":tecla ret" ":pausa 1" ":escribe 100i" ":escribe 3*1,255+12,5=" \
    ":tecla ctrl-c" ":pausa 2" ":captura ac3" ":clic 778 16" ":pausa 1" \
    ":doble 63 334" ":pausa 3" ":escribe ñ" ":tecla ctrl-c" ":tecla ret" ":pausa 1" ":captura ac4" ":clic 778 16" ":pausa 1" \
    ":doble 63 106" ":pausa 3" ":tecla ctrl-v" ":pausa 1" ":captura ac5" \
    ":doble 63 410" ":pausa 3" ":captura ac6" ":pausa 42" ":captura ac7" ":tecla ret" ":pausa 1" \
    ":tecla ctrl-tab" ":tecla ctrl-tab" ":pausa 1" ":clic 65 277" ":tecla backspace" ":escribe 0" ":clic 175 277" ":tecla backspace" ":escribe 4" \
    ":tecla ret" ":pausa 2" ":captura ac8" ":pausa 5" ":captura ac9" ":tecla ret" ":pausa 1" ":tecla ctrl-tab" ":pausa 1" ":captura ac10" > $A/e.sal 2>&1
  # (lo del puerto serie ya llega en UTF-8: probar.sh lo pasa de la 437)
  tr -d '\r' < $A/e.sal > $A/e.txt
  S=$A/e.txt
  mira "los accesos (\\MENU\\CALC.MNU, CARACTER.MNU y RELOJ.MNU) los lanzan" "escritorio: lanzo .CALC.BIN (la tarea [0-9]*)" \
    "escritorio: lanzo .CARACTER.BIN (la tarea [0-9]*)" "escritorio: lanzo .RELOJ.BIN (la tarea [0-9]*)"
  miracap "los tres en el tapiz, con su nombre" ac0 texto 10 270 120 300 Calculadora texto 0 420 120 450 Reloj
  mira "la Calculadora: 0,1 + 0,2 = 0,3 con el teclado; en la científica, 2 + 3 x 4 = 14; en la de oficina, el IVA, 3 x 1,255 + 12,5 = 16,27 (a céntimos) y Ctrl+C" \
    "^calc: = 0,3$" "^calc: = 14$" "^calc: Base 100,00 + IVA 21 % 21,00$" "^calc: = 16,27$" "^calc: copiado 16,27$"
  miracap "la estándar: la cuenta encima del número, y sus teclas con su nombre" ac1 texto15 300 60 780 100 "0,1 + 0,2 =" \
    negrita15 20 170 800 240 MC negrita15 20 230 800 300 Borrar
  miracap "la científica: GRADOS (con su palabra) y sen, cos, tan" ac2 negrita15 20 60 300 100 GRADOS negrita15 20 230 300 300 sen \
    negrita15 20 230 300 300 cos negrita15 20 170 300 240 Inv
  miracap "la de oficina: el tipo de IVA elegido (21 %, con su marca), +IVA y -IVA, y la cinta con la base, el IVA y el total" ac3 \
    negrita15 20 170 470 230 "21 %" negrita15 20 170 470 230 "+IVA" negrita15 470 60 800 100 "La cinta" texto15 480 120 800 480 BASE \
    texto15 480 120 800 480 "IVA 21 %" texto15 480 120 800 480 TOTAL texto15 20 60 470 100 "1,255 + 12,50 ="
  mira "el Mapa de caracteres: la ñ, tecleada, copiada al portapapeles" "^caracter: copiado 164 (ñ, U+00F1)$"
  miracap "el Mapa: la ñ en grande con su nombre, su código y su tecla" ac4 negrita15 470 200 800 300 "Eñe" texto15 470 240 800 320 "164" \
    texto15 470 300 800 380 "tecla ñ" texto15 10 480 470 530 "ñ"
  miracap "y pegada (Ctrl+V) en el Bloc de notas: el portapapeles de todo el sistema" ac5 texto 100 160 400 260 "ñ"
  mira "el Reloj: la alarma de RELOJ /ALARMA +0:40, a su hora, con su mensaje; y la cuenta atrás de 4 segundos" \
    "^reloj: alarma puesta a las 10:0[0-9]:[0-9][0-9]$" "^reloj: ALARMA: 10:0[0-9]:[0-9][0-9]  Reunión de la junta$" \
    "^reloj: cuenta atrás de 4 segundos$" "^reloj: SE ACAB. EL TIEMPO: La cuenta atrás ha llegado a cero$"
  miracap "la pestaña Alarma: puesta, con su hora y su mensaje" ac6 negrita15 20 100 780 150 "ALARMA PUESTA" texto15 140 160 780 220 "Reuni"
  miracap "el aviso de la alarma: su mensaje y los botones Parar y Posponer" ac7 negrita15 40 260 780 330 "junta" negrita15 40 300 780 380 Parar \
    negrita15 40 300 780 380 "Posponer 5 minutos"
  miracap "la cuenta atrás en marcha: Pausa y Cancelar" ac8 negrita15 10 280 400 380 Pausa negrita15 10 280 400 380 Cancelar
  miracap "y su aviso al llegar a cero" ac9 negrita15 40 260 780 340 "cuenta" negrita15 40 260 780 340 "cero"
  miracap "el calendario del mes, con hoy (HOY, en negro) y el festivo del 1 (con *)" ac10 negrita15 360 100 790 140 "Noviembre de 2026" \
    negrita 640 300 720 370 HOY negrita15 700 160 790 200 "1*" texto15 360 450 790 490 "21 de noviembre de 2026"
  if ! grep -q "excepción\|\*\*\*" $A/e.sal; then ok "y ni una excepción del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $A/e.sal | head -3 | sed 's/^/        /'; fi
else ko "no se puede hacer el disco de prueba: $(tail -n 2 $A/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin los accesorios)"
fi   # (ACCESORIOS) los accesorios
# ------------------------------------------------- 29. (SONIDO) el sonido
# (SONIDO, SOLO=sonido) El servicio de sonido (SONIDO.BIN, nucleo32/sonido/:
# HD Audio, AC'97 y la Sound Blaster 16, en C en el anillo 3) y sonido.cas
# (las llamadas 190 a 199). QEMU graba lo que sale por la tarjeta
# (-audiodev wav) y nucleo32/sonido/oye.py lo parte en ráfagas: lo que dura
# cada sonido, su frecuencia, su nivel por cada lado, los cortes y el nivel
# de las frecuencias que se le piden (Goertzel). Con las tres tarjetas de
# QEMU (intel-hda con hda-duplex, AC97 y sb16), desde la orden SONIDO: el
# estado; un tono solo por la izquierda y otro solo por la derecha; dos
# programas sonando a la vez (la mezcla); un WAV de 8 bits, mono, a
# 11.025 Hz; el volumen al 40 % (lo que baja); el silencio (nada); y un WAV
# que toca el servicio solo (la llamada 194). Con HD Audio, el
# Administrador de dispositivos (SONIDO.BIN lleva la tarjeta). En el
# escritorio: el sonido de inicio, el altavoz de la bandeja (la rueda sube
# el volumen, con su «clic»; un clic, la ventanita), Configuración > Sonido
# (la tarjeta, probar el altavoz izquierdo y el derecho) y el sonido de
# apagar. DirectSound de WIN32.BIN (WINDX.EXE, la prueba de DirectX de
# Hispa-486) y Hispa-Snes en HVPRUN (PRUEBA.SFC, con su música).
if parte08 sonido; then
echo "== (SONIDO) Hispa-Pentium: el sonido (SONIDO.BIN, las llamadas 190 a 199, la bandeja, DirectSound y HVPRUN) =="
Y=$P/son
mkdir -p $Y $G
export MTOOLS_SKIP_CHECK=1
if bash $AQUI/nucleo32/sonido/hacer.sh $Y > $Y/hacer.txt 2>&1; then
  ok "nucleo32/sonido/hacer.sh: SONIDO.BIN ($(stat -c %s $Y/SONIDO.BIN) octetos) y los sonidos del sistema (INICIO, APAGAR, AVISO y ERROR.WAV, hechos por hazwav.py)"
else ko "nucleo32/sonido/hacer.sh no hace SONIDO.BIN: $(tail -n 2 $Y/hacer.txt)"; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN
for u in uordenes:ORDENES uconsola:CONSOLA udisposi:DISPOSIT; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN
done
python3 -I $AQUI/../tests/tools/audio.py make $Y/P8.WAV 750 1000 11025 8 1
SIS=$Y/SONIDO.BIN,$Y/INICIO.WAV,$Y/APAGAR.WAV,$Y/AVISO.WAV,$Y/ERROR.WAV
# cumple ARCHIVO N 'condición de awk' qué [-v variable=valor...]: la ráfaga N
# de oye.py cumple la condición (con ini, fin, dura, hz, izq, der, cortes y
# f[1], f[2]...: el nivel de las frecuencias pedidas)
cumple() {
  local a=$1 n=$2 e=$3 q=$4 l; shift 4
  l=$(grep "^ráfaga $n " $a)
  if [ -n "$l" ] && echo "$l" | awk "$@" "{ ini=\$3; fin=\$4; dura=\$5; hz=\$6; izq=\$7; der=\$8; cortes=\$9; for (i = 10; i <= NF; i++) { split(\$i, x, \"=\"); f[i - 9] = x[2] + 0 } } END { exit !($e) }"
  then ok "$q"; else ko "$q: ${l:-no hay esa ráfaga} (se esperaba $e)"; sed 's/^/        /' $a | head -10; fi
}
campo() { grep "^ráfaga $2 " $1 | awk -v c=$3 '{ print $c }'; }
for tarjeta in "hda:-device intel-hda -device hda-duplex,audiodev=snd0" "ac97:-device AC97,audiodev=snd0" "sb16:-device sb16,audiodev=snd0"; do
  n=${tarjeta%%:*}; dev=${tarjeta#*:}
  O=("sonido" ":pausa 2" "sonido /tono 440 1200 I" ":pausa 1" "sonido /tono 660 1200 D" ":pausa 1" "sonido /tono 523 6000 /suelto" ":pausa 1" "sonido /tono 1000 1500" ":pausa 6"
     "sonido M:\\P8.WAV" ":pausa 2" "sonido /volumen 40" "sonido /tono 440 800" ":pausa 1" "sonido /silencio si" "sonido /tono 440 800" ":pausa 1" "sonido /silencio no"
     "sonido /volumen 80" "sonido /fondo M:\\AVISO.WAV" ":pausa 2")
  [ $n = hda ] && O+=("disposit /informe" ":pausa 6")
  rm -f "${Y:?}/$n.wav"
  MODULOS=$Y/ORDENES.BIN,$Y/DISPOSIT.BIN,$Y/P8.WAV,$SIS ARRANQUE=ordenes QEMU_MAS="-audiodev wav,id=snd0,path=$Y/$n.wav $dev" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${O[@]}" > $Y/$n.sal 2>&1
  tr -d '\r' < $Y/$n.sal > $Y/$n.txt
  S=$Y/$n.txt
  case $n in
    hda) mira "HD Audio (intel-hda con hda-duplex): SONIDO dice la tarjeta, el códec, la salida y el volumen" "^Tarjeta:   Intel HD Audio (ICH6) (HD Audio)$" \
           "^Códec:     QEMU HDA (dúplex)$" "^Salida:    línea; 48000 Hz, 16 bits, estéreo$" "^Volumen:   80 %$" "^Sonidos del sistema: sí$" "^sonido: HD Audio: el pin 3 (línea) suena, desde el DAC 2$" ;;
    ac97) mira "AC'97 (la ICH de Intel con el SigmaTel STAC9700 de QEMU): la tarjeta y el códec" "^Tarjeta:   Intel AC'97 (ICH) (AC'97)$" "^Códec:     SigmaTel STAC9700$" \
           "^Salida:    altavoces; 48000 Hz, 16 bits, estéreo$" ;;
    sb16) mira "la Sound Blaster 16 (en 220h, DSP 4.05; la DMA por el canal 5, por debajo de los 16 MB)" "^Tarjeta:   Sound Blaster 16 (220) (Sound Blaster 16)$" \
           "^Códec:     DSP 4.05$" "^Salida:    altavoces; 44100 Hz, 16 bits, estéreo$" "^sonido: Sound Blaster 16 en 220: DSP 4.5; la DMA, en el canal 5 ([0-9a-f]*)$" ;;
  esac
  python3 -I $AQUI/nucleo32/sonido/oye.py $Y/$n.wav 523,1000 > $Y/$n.oye 2>&1
  r0=$(campo $Y/$n.oye 0 7); r0=${r0:-1}
  cumple $Y/$n.oye 0 'hz >= 420 && hz <= 460 && dura >= 1100 && dura <= 1400 && izq > 3000 && der < 200 && cortes == 0' "$n: «SONIDO /TONO 440 1200 I»: 440 Hz, 1,2 s, solo por la izquierda y sin cortes"
  cumple $Y/$n.oye 1 'hz >= 630 && hz <= 690 && dura >= 1100 && dura <= 1400 && der > 3000 && izq < 200 && cortes == 0' "$n: «/TONO 660 1200 D»: 660 Hz, solo por la derecha"
  # (el de 523 Hz dura 6 s y P8.WAV va 6 s después del de 1.000 Hz: con 9 s y
  # 4 s, según lo que se tardara en teclear, P8.WAV empezaba justo al acabar
  # el de 523 Hz y oye.py los juntaba en una ráfaga, con «un corte» entre los
  # dos; y faltaba una ráfaga: así salían los 5 fallos de cada tarjeta)
  cumple $Y/$n.oye 2 'dura >= 5500 && dura <= 6800 && f[1] > 0.6 * r0 && f[2] > 0.6 * r0 && izq > 1.6 * r0 && cortes == 0' "$n: dos programas a la vez (523 Hz 6 s, suelto, y 1.000 Hz encima): los dos se oyen y se suman, sin cortes" -v r0=$r0
  cumple $Y/$n.oye 3 'hz >= 700 && hz <= 800 && dura >= 900 && dura <= 1200 && izq > 1000 && der > 1000' "$n: un WAV de 8 bits, mono, a 11.025 Hz (SONIDO M:\\P8.WAV), a su frecuencia y lo que dura"
  cumple $Y/$n.oye 4 'hz >= 420 && hz <= 460 && izq < 0.2 * r0 && izq > 0.05 * r0' "$n: con el volumen al 40 %, unos 19 dB más bajo (la curva de HSPSND: 0,47 dB por punto)" -v r0=$r0
  cumple $Y/$n.oye 5 'hz >= 780 && hz <= 980 && dura >= 400 && dura <= 800' "$n: en silencio no suena nada; y AVISO.WAV, que toca el servicio solo (la llamada 194, «SONIDO /FONDO»)"
  if [ "$(grep -c '^ráfaga' $Y/$n.oye)" = 6 ]; then ok "$n: ni una ráfaga de más (el tono en silencio no ha sonado)"; else ko "$n: $(grep -c '^ráfaga' $Y/$n.oye) ráfagas (se esperaban 6)"; fi
  if [ $n = hda ]; then
    mira "el Administrador de dispositivos: la tarjeta de sonido, con controlador (SONIDO.BIN, un servicio del anillo 3), el códec y la salida" \
      "^  \[OK\] Intel ICH6: sonido HD Audio  \[8086:2668\]$" "^       Estado: Funciona: lo lleva SONIDO.BIN.$" "^       Detalle: QEMU HDA (dúplex); por la línea, a 48000 Hz$" \
      "^       Controlador: SONIDO.BIN (un servicio del anillo 3)$"
  fi
  if ! grep -q "excepción\|\*\*\*" $S; then ok "$n: ni una excepción ni una queja del núcleo"
  else ko "$n: el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
done
# 2. el escritorio (HD Audio): el sonido de inicio, el altavoz de la bandeja (la
# rueda, dos pasos arriba: 80 % -> 90 %, con su «clic» cada vez; un clic: la
# ventanita del volumen), Configuración > Sonido y sus botones de probar, y apagar
rm -f "${Y:?}/esc.wav"
CAPTURAS=$G MODULOS=$Y/ORDENES.BIN,$Y/CONSOLA.BIN,$SIS ARRANQUE=escritorio QEMU_MAS="-audiodev wav,id=snd0,path=$Y/esc.wav -device intel-hda -device hda-duplex,audiodev=snd0" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 6" ":captura so1" ":raton 720 583" ":monitor mouse_move 0 0 1" ":pausa 1" ":monitor mouse_move 0 0 1" ":pausa 1" \
  ":clic 720 583" ":pausa 3" ":captura so2" ":tecla esc" ":pausa 1" ":clicd 720 583" ":pausa 1" ":captura so3" ":tecla esc" ":pausa 1" \
  ":clic 40 585" ":pausa 1" ":clic 310 237" ":pausa 1" ":clic 464 230" ":pausa 4" ":captura so4" ":clic 355 330" ":pausa 2" ":clic 515 330" ":pausa 2" \
  ":clic 40 585" ":clic 360 545" ":clic 275 327" ":pausa 6" > $Y/esc.sal 2>&1
S=$Y/esc.sal
python3 -I $AQUI/nucleo32/sonido/oye.py $Y/esc.wav > $Y/esc.oye 2>&1
cumple $Y/esc.oye 0 'hz >= 480 && hz <= 570 && dura >= 1200 && dura <= 1700' "el escritorio arranca el servicio («/ARRANQUE») y suena INICIO.WAV (do, mi, sol, do: empieza en 523 Hz)"
miracap "el altavoz en la bandeja, a la izquierda del reloj (con el volumen al 80 %: tres ondas)" so1 rgb 715 582 F4F8FE rgb 731 582 FFFFFF
cumple $Y/esc.oye 1 'hz >= 820 && hz <= 940 && dura >= 30 && dura <= 150' "la rueda sobre el altavoz sube el volumen: su «clic» (880 Hz)"
cumple $Y/esc.oye 2 'hz >= 820 && hz <= 940 && izq > c1 * 1.05' "y otra vez, más fuerte (85 % -> 90 %)" -v c1=$(campo $Y/esc.oye 1 7)
miracap "un clic en el altavoz: la ventanita del volumen, encima de la bandeja (90 %, el silencio, «Ajustes...»)" so2 negrita 500 380 800 520 Volumen texto 500 400 800 520 "90 %" \
  texto 500 420 800 540 Silencio texto 500 420 800 540 "Ajustes..."
miracap "el botón secundario en el altavoz: «Silenciar» y «Configuración de sonido...»" so3 texto 500 450 800 580 Silenciar texto 500 450 800 580 "Configuración de sonido..."
miracap "Configuración > «Sonido...»: la tarjeta, el códec, el volumen, probar y los sonidos del sistema" so4 negrita 150 60 480 90 Sonido texto 230 150 580 190 "Intel HD Audio (ICH6)" \
  texto 230 170 580 210 "QEMU HDA (dúplex)" texto 480 240 640 280 "90 %" negrita 260 300 440 340 Izquierdo negrita 420 300 600 340 Derecho texto 180 330 640 380 "Sonidos del sistema"
cumple $Y/esc.oye 3 'hz >= 490 && hz <= 560 && izq > 3000 && der < 200' "«Izquierdo»: el altavoz izquierdo (un do, solo por ese lado)"
cumple $Y/esc.oye 4 'hz >= 620 && hz <= 700 && der > 3000 && izq < 200' "«Derecho»: el derecho (un mi)"
cumple $Y/esc.oye 5 'hz >= 700 && hz <= 870 && dura >= 800 && dura <= 1500' "Apagar: suena APAGAR.WAV (sol, mi, do: empieza en 784 Hz) antes de apagar"
mira "y el escritorio espera a que acabe, para la tarjeta sin ruido y apaga" "^escritorio: el sonido de apagar, tocado$" "^escritorio: apagar$"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en el escritorio, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado en el escritorio:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
# 3. DirectSound de WIN32.BIN: WINDX.EXE (la prueba de DirectX de Hispa-486)
# toca un la (440 Hz) en bucle con un búfer de DirectSound y luego cambia su
# frecuencia (SetFrequency: 660 Hz); WINMM lo mezcla y lo da a un canal
if bash $AQUI/nucleo32/win32/disco.sh $Y/w32.img > $Y/w32disco.txt 2>&1 && make -s -C $AQUI/.. build/WINDX.EXE > /dev/null 2>&1; then
  mcopy -o -i $Y/w32.img $AQUI/../build/WINDX.EXE ::PRUEBAS/ && mcopy -o -i $Y/w32.img $Y/SONIDO.BIN $Y/INICIO.WAV $Y/APAGAR.WAV $Y/AVISO.WAV $Y/ERROR.WAV ::
  rm -f "${Y:?}/dx.wav"
  CAPTURAS=$G DISCO=$Y/w32.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-audiodev wav,id=snd0,path=$Y/dx.wav -device intel-hda -device hda-duplex,audiodev=snd0" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 6" ":doble 60 186" ":pausa 4" ":escribe \\PRUEBAS\\WINDX" ":tecla ret" ":pausa 8" ":tecla spc" ":pausa 5" ":tecla spc" ":pausa 3" \
    ":tecla esc" ":pausa 2" > $Y/dx.sal 2>&1
  python3 -I $AQUI/nucleo32/sonido/oye.py $Y/dx.wav 440,660 > $Y/dx.oye 2>&1
  cumple $Y/dx.oye 1 'dura >= 4000 && f[1] > 1500 && f[2] > 1500 && izq > 2000' "DirectSound (WINDX.EXE): el la en bucle de su búfer y, con SetFrequency, a 660 Hz, por la tarjeta"
  S=$Y/dx.sal
  if ! grep -q "excepción\|\*\*\*" $S; then ok "WINDX: ni una excepción ni una queja del núcleo"
  else ko "WINDX: el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
else
  ko "no se puede hacer el disco de los programas de Windows o WINDX.EXE: $(tail -n 2 $Y/w32disco.txt)"
fi
# 4. Hispa-Snes en HVPRUN: PRUEBA.SFC con su música, por la tarjeta (HVPRUN
# pasa su snd_pcm2 y su snd_orden al sonido de Hispa-Pentium)
if bash $AQUI/nucleo32/hvprun/hacer.sh $Y/of > /dev/null 2>&1 && make -s -C $AQUI/.. build/HISPASNS.HVP build/PRUEBA.SFC > /dev/null 2>&1; then
  rm -f "${Y:?}/snes.wav"
  CAPTURAS=$G MODULOS=$Y/ORDENES.BIN,$Y/CONSOLA.BIN,$Y/of/HVPRUN.BIN,$AQUI/../build/HISPASNS.HVP,$AQUI/../build/PRUEBA.SFC,$SIS ARRANQUE=escritorio MEMORIA=64 \
    QEMU_MAS="-audiodev wav,id=snd0,path=$Y/snes.wav -device intel-hda -device hda-duplex,audiodev=snd0" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 6" ":doble 60 186" ":pausa 4" ":escribe hvprun M:\\HISPASNS.HVP M:\\PRUEBA.SFC" ":tecla ret" ":pausa 12" \
    ":clic 400 300" ":pausa 12" ":captura so5" > $Y/snes.sal 2>&1
  python3 -I $AQUI/nucleo32/sonido/oye.py $Y/snes.wav > $Y/snes.oye 2>&1
  cumple $Y/snes.oye 1 'dura >= 5000 && izq > 1500' "Hispa-Snes en HVPRUN: la música de PRUEBA.SFC suena por la tarjeta (snd_pcm2 y snd_orden, sobre las llamadas del sonido)"
  S=$Y/snes.sal
  if ! grep -q "excepción\|\*\*\*" $S; then ok "Hispa-Snes en HVPRUN: ni una excepción ni una queja del núcleo"
  else ko "Hispa-Snes en HVPRUN: el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
else
  ko "no se pueden hacer HVPRUN.BIN, HISPASNS.HVP o PRUEBA.SFC"
fi
# 5. Hispa-Nes (HISPA1.NES) e Hispa-System (BUZO.SMS) en HVPRUN: desde el 21/11
# corren allí como Hispa-Snes (sin DPMI ni puertos: el reloj fino de HVPRUN) y
# suenan (la «fin de la partida» de BUZO tiene su música; HISPA1, sus golpes)
if [ -f $Y/of/HVPRUN.BIN ] && make -s -C $AQUI/.. build/HISPANES.HVP build/HISPA1.NES build/HISPASMS.HVP build/BUZO.SMS > /dev/null 2>&1; then
  for emu in HISPANES:HISPA1.NES:Hispa-Nes HISPASMS:BUZO.SMS:Hispa-System; do
    h=${emu%%:*}; r=${emu#*:}; r=${r%%:*}; nom=${emu##*:}
    rm -f "${Y:?}/$h.wav"
    CAPTURAS=$G MODULOS=$Y/ORDENES.BIN,$Y/CONSOLA.BIN,$Y/of/HVPRUN.BIN,$AQUI/../build/$h.HVP,$AQUI/../build/$r,$SIS ARRANQUE=escritorio MEMORIA=64 \
      QEMU_MAS="-audiodev wav,id=snd0,path=$Y/$h.wav -device intel-hda -device hda-duplex,audiodev=snd0" \
      $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 6" ":doble 60 186" ":pausa 4" ":escribe hvprun M:\\$h.HVP M:\\$r" ":tecla ret" ":pausa 10" \
      ":clic 400 300" ":tecla ret" ":pausa 10" ":captura so-$h" > $Y/$h.sal 2>&1
    python3 -I $AQUI/nucleo32/sonido/oye.py $Y/$h.wav > $Y/$h.oye 2>&1
    cumple $Y/$h.oye 1 'izq > 1000' "$nom en HVPRUN: corre (sin DPMI ni puertos) y suena por la tarjeta"
    miracap "$nom en HVPRUN: su ventana (el título, y en la barra de abajo, «c/s»: los cuadros por segundo)" so-$h negrita 280 120 460 190 "$nom" texto 270 380 530 430 "c/s"
    S=$Y/$h.sal
    if ! grep -q "excepción\|\*\*\*" $S; then ok "$nom en HVPRUN: ni una excepción ni una queja del núcleo"
    else ko "$nom en HVPRUN: el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
  done
else
  ko "no se pueden hacer HISPANES.HVP, HISPA1.NES, HISPASMS.HVP o BUZO.SMS"
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el sonido)"
fi   # (SONIDO) el sonido
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }

# ---------------------------------------------- 18. el Navegador (NAVEGA.BIN)
# NetSurf con QuickJS en un programa del anillo 3 (nucleo32/navega/: el
# Navegador completo de Hispa-486 con un frontal nuevo; docs/PENTIUM.md, «El
# Navegador»), contra el sitio de prueba de navega/servidor_pruebas.py (HTTP
# y HTTPS, con su raíz de prueba en \RAICES): una hoja de fuera con variables
# de CSS, un PNG y un JPEG, JavaScript (el DOM, setTimeout, localStorage,
# fetch con galletas y una cabecera), un formulario POST con eñe, la rueda del
# ratón, Fin, Atrás, el menú del botón derecho (abrir el enlace en otra
# pestaña), la dirección (F6) por HTTP, el portapapeles del sistema (Ctrl+L,
# Ctrl+C y Ctrl+V en un campo), una descarga a C:\DESCARGA con su nombre en
# 8+3 (igual octeto a octeto), las galletas que se guardan al cerrar; y el
# x87 de cada programa (X87.BIN, a la vez con su copia); (TERMO-FUERTE) el SSE
# de cada programa (XMM.BIN) y, con -cpu pentium, el x87 sin FXSAVE. Las posiciones, las de
# la ventana en una pantalla de 800 x 600 (la página empieza en (20, 108)).
if parte08 navegador; then
echo "== Hispa-Pentium: el Navegador (NetSurf con QuickJS, NAVEGA.BIN) =="
Q=$P/nav
rm -rf $Q; mkdir -p $Q $G
python3 -I $AQUI/nucleo32/navega/servidor_pruebas.py $Q/srv > $Q/srv.txt 2>&1 &
NPID=$!
for _ in $(seq 100); do [ -s $Q/srv/puertos ] && break; sleep 0.1; done
HP=$(sed -n 's/^http=//p' $Q/srv/puertos); HS=$(sed -n 's/^https=//p' $Q/srv/puertos)
export MTOOLS_SKIP_CHECK=1
if [ -n "$HS" ] && $AQUI/nucleo32/disco.sh $Q/d.img 32 red tls navegador > $Q/disco.txt 2>&1 && mmd -i $Q/d.img ::RAICES \
   && mcopy -i $Q/d.img $Q/srv/RAIZ.CER ::RAICES/PRUEBA.CER; then
  ok "nucleo32/navega/hacer.sh: NAVEGA.BIN, NetSurf 3.11 con QuickJS y la biblioteca de C de Hispa-486 en un programa del anillo 3 ($(mdir -i $Q/d.img ::NAVEGA/NAVEGA.BIN | sed -n 's/^NAVEGA *BIN *\([0-9]*\).*/\1/p') octetos)"
  printf 'Inicio=https://10.0.2.2:%s/\r\n' $HS > $Q/NAVEGA.INI
  mcopy -o -i $Q/d.img $Q/NAVEGA.INI ::NAVEGA/
  : > $Q/SURF.LOG; mcopy -i $Q/d.img $Q/SURF.LOG ::NAVEGA/
  cp $Q/d.img $Q/e.img
  CAPTURAS=$G ESCRIBE=1 DISCO=$Q/e.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" \
    ":pausa 3" ":captura nv0" ":doble 62 260" ":pausa 15" ":captura nv1" \
    ":raton 400 400" ":monitor mouse_move 0 0 -3" ":pausa 2" ":captura nv2" ":tecla end" ":pausa 2" ":captura nv3" ":tecla home" ":pausa 2" \
    ":clic 130 267" ":escribe Peña" ":tecla ret" ":pausa 5" ":captura nv4" ":clic 41 89" ":pausa 5" ":captura nv5" \
    ":clicd 95 309" ":pausa 1" ":captura nv6" ":clic 150 326" ":pausa 5" ":captura nv7" \
    ":tecla f6" ":escribe 10.0.2.2:$HP/pagina2.html" ":tecla ret" ":pausa 4" ":captura nv8" \
    ":tecla ctrl-l" ":tecla ctrl-c" ":tecla esc" ":clic 482 57" ":pausa 6" ":clic 130 267" ":tecla ctrl-v" ":tecla ret" ":pausa 5" \
    ":tecla f6" ":escribe https://10.0.2.2:$HS/descarga" ":tecla ret" ":pausa 6" ":captura nv9" ":clic 765 26" ":pausa 4" > $Q/e.sal 2>&1
  tr -d '\r' < $Q/e.sal > $Q/e.txt
  S=$Q/e.txt
  mira "el acceso del menú (\\MENU\\NAVEGA.MNU) lo pone en el tapiz y lo lanza; arranca con la página de NAVEGA.INI" \
    "escritorio: lanzo .NAVEGA.NAVEGA.BIN (la tarea [0-9]*)" "^navega: Navegador [0-9.]*, en C:.NAVEGA$" "^navega: primera https://10.0.2.2:$HS/$"
  miracap "el icono «Navegador» en el tapiz" nv0 texto 20 276 40 292 Navegador
  mira "la página por HTTPS (TLS.BIN, con la raíz de prueba) y su hoja, sus imágenes y su fetch, por la misma conexión cifrada" \
    "tls: 10.0.2.2: TLS 1.2, ECDHE-ECDSA, .*la cadena comprobada$" "^navega: hecho https://10.0.2.2:$HS/ \[[0-9]* KB en uso"
  S=$Q/srv/pedidos.txt
  mira "lo que pide: el User-Agent propio (HispaSurf, sin «Hispa32» ni «HispaNavegador»: ClicGest da la web normal), las galletas y la cabecera de fetch" \
    "^GET / | HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium) |" "^GET /estilo.css | HispaSurf.* | sesion=abc; recuerda=1; desdejs=1 |" \
    "^GET /rojo.png |" "^GET /azul.jpg |" "^GET /api?visitas=1 | .* | sesion=abc; recuerda=1; desdejs=1 | fetch |"
  miracap "la página pintada: la hoja de fuera con variables de CSS (la cabecera verde), el JavaScript (la caja magenta), setTimeout (azul), fetch con las dos galletas (naranja), el PNG y el JPEG" nv1 \
    rgb 300 140 2e8b57 rgb 77 222 c71585 rgb 181 222 1e90ff rgb 285 222 ff8c00 rgb 382 208 d02020 rgb 472 208 2040d0 14
  miracap "la rueda del ratón mueve la página (60 puntos por paso)" nv2 nrgb 300 112 2e8b57 rgb 77 160 c71585 rgb 285 160 ff8c00
  miracap "Fin: al final de la página (el pie, morado)" nv3 rgb 300 500 8a2be2
  S=$Q/srv/pedidos.txt
  mira "el formulario: lo tecleado (con eñe) va por POST" "^POST /eco | .* | nombre=Pe%C3%B1a$"
  miracap "y la respuesta del servidor, pintada" nv4 rgb 300 150 daa520
  miracap "Atrás: la página de antes, con su JavaScript otra vez" nv5 rgb 300 140 2e8b57 rgb 77 222 c71585
  miracap "el botón derecho sobre un enlace: su menú (abrirlo en otra pestaña, copiar su dirección, descargarlo)" nv6 texto 100 310 140 340 Abrir
  S=$Q/e.txt
  mira "«Abrir el enlace en otra pestaña»: la segunda página, en una pestaña nueva" "^navega: hecho https://10.0.2.2:$HS/pagina2.html "
  mira "F6 y una dirección sin http:// (por HTTP, sin cifrar)" "^navega: ir a http://10.0.2.2:$HP/pagina2.html$" "^navega: hecho http://10.0.2.2:$HP/pagina2.html "
  miracap "la segunda página, por HTTP, en la segunda pestaña" nv8 rgb 300 150 008b8b
  S=$Q/srv/pedidos.txt
  mira "el portapapeles del sistema: Ctrl+L y Ctrl+C copian la dirección; Ctrl+V la pega en un campo de otra pestaña (+), y va con el formulario" \
    "^POST /eco | .* | nombre=http%3A%2F%2F10.0.2.2%3A$HP%2Fpagina2.html$"
  S=$Q/e.txt
  mira "una descarga (Content-Disposition: attachment): a C:\\DESCARGA, con su nombre en 8+3" \
    "^navega: descarga: Informe anual 2026.zip -> C:.DESCARGA.INFORMEA.ZIP$" "^navega: descarga: hecha C:.DESCARGA.INFORMEA.ZIP, 50000 octetos$"
  mira "y al cerrar la ventana, se acaba" "^navega: adiós$"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
  sano $Q/e.img
  if saca $Q/e.img DESCARGA/INFORMEA.ZIP $Q/x.zip && cmp -s $Q/x.zip $Q/srv/descarga.bin && grep -q "^$Q/e.img: [0-9]* files" $P/fsck.txt; then
    ok "lo descargado es igual octeto a octeto que lo del servidor, y el disco queda sano"
  else ko "lo descargado no está, no es igual o el disco no queda sano"; sed 's/^/        /' $P/fsck.txt | head -4; fi
  if saca $Q/e.img NAVEGA/SURFGAL.TXT $Q/gal.txt && grep -q "recuerda" $Q/gal.txt && ! grep -q "sesion" $Q/gal.txt && saca $Q/e.img NAVEGA/NAVEGA2.ALM $Q/alm.txt; then
    ok "las galletas que dicen hasta cuándo valen se guardan al cerrar (SURFGAL.TXT; las de sesión, no), y lo de localStorage (NAVEGA2.ALM)"
  else ko "no se guardan las galletas o localStorage"; fi
  # position: fixed y sticky (navega 0.2: la barra lateral de ClicGest se
  # cortaba al desplazar): la rueda, y la cabecera fija, la barra lateral fija
  # (entera, hasta abajo) y el bloque sticky siguen en su sitio; y el enlace de
  # abajo de la barra lateral, ya desplazada, se puede pulsar
  printf 'Inicio=http://10.0.2.2:%s/fijo.html\r\n' $HP > $Q/NAVEGA.INI
  cp $Q/d.img $Q/f.img
  mcopy -o -i $Q/f.img $Q/NAVEGA.INI ::NAVEGA/
  CAPTURAS=$G DISCO=$Q/f.img ARRANQUE=escritorio MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" \
    ":pausa 3" ":doble 62 260" ":pausa 12" ":captura nvf0" ":raton 400 400" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" \
    ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 2" ":captura nvf1" \
    ":clic 45 500" ":pausa 3" > $Q/f.sal 2>&1
  miracap "position: fixed y sticky, sin desplazar: la cabecera fija (roja), la barra lateral fija (azul, de height: 100vh: hasta abajo) y el sticky (verde) en su sitio" nvf0 \
    rgb 300 128 e03030 rgb 60 130 305080 rgb 60 524 305080 rgb 300 360 20a060
  miracap "y desplazada 300 puntos con la rueda: la cabecera y la barra lateral, donde estaban y enteras; el sticky, pegado bajo la cabecera" nvf1 \
    rgb 300 128 e03030 rgb 60 130 305080 rgb 60 524 305080 rgb 300 163 20a060 nrgb 300 360 20a060
  S=$Q/srv/pedidos.txt
  mira "y el enlace de abajo de la barra lateral fija, con la página desplazada, se pulsa (donde se ve)" "^GET /eco?lateral |"
  # (navega 0.2.2) en la pantalla del HP Mini (1024x600), lo que Carlos no
  # encontraba en clicgest.es: «Acceder» (el menú plegado de la portada, que
  # se abre con el botón de las tres rayas, un SVG en un data:) y, en el modo
  # teléfono, «Ver versión de ordenador» (en la hoja «Más»: un <dialog> fijo
  # abajo, con inset, 86dvh, overflow-y: auto y un <details> cerrado; la
  # barra de abajo, fija con bottom: 0); y la versión para móvil o de
  # escritorio del propio Navegador (el botón derecho), que se recuerda
  printf 'Inicio=https://10.0.2.2:%s/portada\r\n' $HS > $Q/NAVEGA.INI
  cp $Q/d.img $Q/m.img
  mcopy -o -i $Q/m.img $Q/NAVEGA.INI ::NAVEGA/
  : > $Q/srv/pedidos.txt
  RU=":monitor mouse_move 0 0 -1"
  CAPTURAS=$G ESCRIBE=1 DISCO=$Q/m.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" \
    $AQUI/nucleo32/probar.sh "" ":pausa 3" ":doble 62 260" ":pausa 14" ":captura nvm0" \
    ":clic 955 133" ":pausa 3" ":captura nvm1" ":clic 88 243" ":pausa 4" ":captura nvm2" \
    ":tecla f6" ":escribe https://10.0.2.2:$HS/panel" ":tecla ret" ":pausa 4" ":clic 100 190" ":pausa 5" ":captura nvm3" \
    ":clic 828 502" ":pausa 3" ":captura nvm4" ":raton 400 400" "$RU" ":pausa 1" "$RU" ":pausa 1" "$RU" ":pausa 1" "$RU" ":pausa 1" "$RU" ":pausa 2" \
    ":captura nvm5" ":clic 400 497" ":pausa 5" ":captura nvm6" \
    ":tecla f6" ":escribe https://10.0.2.2:$HS/ua" ":tecla ret" ":pausa 4" ":clicd 300 250" ":pausa 1" ":captura nvm7" ":clic 340 429" ":pausa 4" ":captura nvm8" \
    ":clicd 300 250" ":pausa 1" ":clic 340 429" ":pausa 4" ":captura nvm9" ":clicd 300 250" ":pausa 1" ":clic 340 429" ":pausa 4" \
    ":clic 990 26" ":pausa 3" ":doble 62 260" ":pausa 12" ":tecla f6" ":escribe https://10.0.2.2:$HS/ua" ":tecla ret" ":pausa 4" ":captura nvm10" \
    ":clic 990 26" ":pausa 3" > $Q/m.sal 2>&1
  miracap "la portada a 1024x600: el botón de las tres rayas se ve (navbar-toggler-icon: un SVG con trazos en un data:, con NanoSVG)" nvm0 \
    rgb 950 125 212529 24 rgb 950 133 212529 24 rgb 950 141 212529 24 rgb 950 129 ffffff 8
  miracap "al pulsarlo se abre el menú (JavaScript: .collapse.show) con «Acceder»" nvm1 rgb 45 232 1f6feb 8
  S=$Q/srv/pedidos.txt
  mira "«Acceder» lleva a /login" "^GET /login |"
  miracap "y se ve la página de entrar" nvm2 rgb 300 200 6a5acd 8
  miracap "el modo teléfono (galleta vista=movil): la barra de abajo, fija con bottom: 0, se ve abajo de la ventana (antes, al final de la página: nunca)" nvm3 \
    rgb 300 490 0b6e4f 8 rgb 900 490 f59e0b 8
  miracap "«Más» abre la hoja (showModal), pegada abajo (inset: auto 0 0 0) y de 86dvh, con el <details> cerrado (lo de dentro no se ve)" nvm4 \
    rgb 300 200 fff3c4 8 rgb 300 480 ffd0d0 8 nrgb 300 130 fff3c4 8
  miracap "la rueda sobre la hoja la desplaza a ella (y no la página de debajo): «Ver versión de ordenador», al final, a la vista" nvm5 \
    rgb 300 500 c0306a 8 rgb 300 125 0f172a 8
  S=$Q/srv/pedidos.txt
  mira "y se pulsa (el clic es para la hoja, que está encima, y no para lo de debajo): la galleta vista=pc y la versión de ordenador otra vez" \
    "^GET /vista?modo=movil&volver=/panel |" "^GET /panel | .* | vista=movil |" "^GET /vista?modo=pc&volver=/panel | .* | vista=movil |" "^GET /panel | .* | vista=pc |"
  miracap "la versión de ordenador" nvm6 rgb 300 130 1f4e79 8
  miracap "el botón derecho ofrece «Ver la versión para móvil»" nvm7 texto 300 400 500 440 "Ver la versi"
  mira "el Navegador pide la versión para móvil (el User-Agent con «Mobile»), vuelve a la de escritorio y otra vez la de móvil; y la recuerda al abrirlo otra vez" \
    "^GET /ua | HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium) |" "^GET /ua | HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium; Mobile) |"
  if [ "$(grep -c '^GET /ua |' $S)" -ge 5 ] && grep '^GET /ua |' $S | sed -n '2p;4p;5p' | grep -c 'Mobile' | grep -q '^3$' \
     && grep '^GET /ua |' $S | sed -n '3p' | grep -vq 'Mobile'; then
    ok "en orden: escritorio, móvil, escritorio, móvil y, al abrirlo otra vez, móvil"
  else ko "el orden de las versiones no es el esperado:"; grep '^GET /ua |' $S | cut -c1-80 | sed 's/^/        /'; fi
  miracap "con la versión para móvil, la página y su JavaScript (navigator.userAgent) lo saben" nvm8 rgb 300 150 daa520 8 rgb 300 250 c71585 8
  miracap "y con la de escritorio, también" nvm9 rgb 300 150 008b8b 8 rgb 300 250 1e90ff 8
  if saca $Q/m.img NAVEGA/NAVEGA2.INI $Q/n2.ini && [ "$(tr -d '\r' < $Q/n2.ini)" = "Movil=10.0.2.2" ]; then
    ok "lo elegido se guarda por sitio en NAVEGA2.INI («Movil=10.0.2.2»)"
  else ko "NAVEGA2.INI no tiene «Movil=10.0.2.2»: $(cat -A $Q/n2.ini 2> /dev/null | head -3)"; fi
  # el x87: dos programas con coma flotante a la vez
  DISCO=$Q/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "x87" ":pausa 6" > $Q/x.sal 2>&1
  tr -d '\r' < $Q/x.sal > $Q/x.txt
  S=$Q/x.txt
  mira "el x87 de cada programa: X87.BIN y sus dos copias, a la vez, cada una con su coma flotante (el núcleo la guarda y la repone)" \
    "^x87: bien (dos programas a la vez, cada uno con su x87)$"
  # (TERMO-FUERTE, 0.8.12) el SSE de cada programa (FXSAVE/FXRSTOR al cambiar
  # de tarea): XMM.BIN y sus dos copias, a la vez, cada una con sus XMM, su
  # MXCSR y su x87; limpio al empezar; y sin FXSAVE (-cpu pentium), el x87
  # con fnsave como antes y XMM.BIN dice que no hay SSE2
  DISCO=$Q/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "xmm" ":pausa 8" "x87" ":pausa 6" > $Q/xm.sal 2>&1
  tr -d '\r' < $Q/xm.sal > $Q/xm.txt
  S=$Q/xm.txt
  mira "(TERMO-FUERTE) el SSE de cada programa: XMM.BIN y sus dos copias, a la vez, cada una con sus XMM, su MXCSR y su x87 (FXSAVE al salir de cada tarea), limpios al empezar; y el x87 sigue bien" \
    "^xmm: bien (tres programas a la vez, cada uno con sus XMM, su MXCSR y su x87)$" "^x87: bien (dos programas a la vez, cada uno con su x87)$"
  if grep -q '^xmm: MAL' $S; then ko "(TERMO-FUERTE) XMM.BIN: $(grep '^xmm: MAL' $S | head -n 2 | tr '\n' ' ')"; fi
  DISCO=$Q/d.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-cpu pentium" $AQUI/nucleo32/probar.sh "" "x87" ":pausa 6" "xmm" ":pausa 3" > $Q/xp.sal 2>&1
  tr -d '\r' < $Q/xp.sal > $Q/xp.txt
  S=$Q/xp.txt
  mira "(TERMO-FUERTE) con un Pentium (sin FXSAVE ni SSE): el x87 de cada programa, con fnsave como antes; XMM.BIN dice que no hay SSE2" \
    "^x87: bien (dos programas a la vez, cada uno con su x87)$" "^xmm: sin SSE2"
else ko "no se puede poner en marcha el sitio de prueba o hacer el disco: $(tail -n 1 $Q/srv.txt) $(tail -n 3 $Q/disco.txt)"; fi
kill $NPID 2> /dev/null; wait $NPID 2> /dev/null
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Navegador)"
fi   # el Navegador

# ------------------------------------- 23 bis. (RUEDA) la rueda en los programas
if parte08 rueda; then
echo "== Hispa-Pentium: la rueda del ratón en los programas (RUEDA) =="
W=$P/rueda
rm -rf $W; mkdir -p $W $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN || ko "semilla.cas no compila"
for u in udisposi:DISPOSIT umuestra:MUESTRA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/${u##*:}.BIN || ko "${u%%:*}.cas no compila"
done
# (y Programas y actualizaciones, que solo se compila: su rueda se ve con un catálogo)
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uactual.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/ACTUALIZ.BIN \
  && ok "ACTUALIZ.BIN (Programas y actualizaciones, con la rueda en su lista) compila" || ko "uactual.cas no compila"
if $AQUI/nucleo32/disco.sh $W/d.img 32 > $W/disco.txt 2>&1; then
  python3 -c "
import sys
for i in range(1, 401): sys.stdout.write('linea %03d de un texto largo\r\n' % i)" > $W/LARGO.TXT
  mmd -i $W/d.img ::MUCHOS
  for i in $(seq -w 1 80); do printf 'archivo %s\r\n' $i > $W/A$i.TXT; done
  mcopy -i $W/d.img $W/A*.TXT ::MUCHOS/
  mcopy -i $W/d.img $W/LARGO.TXT $W/DISPOSIT.BIN $W/MUESTRA.BIN ::/
  # la Consola (dos clics en su icono): 400 líneas, y la rueda hacia arriba
  # (cada «mouse_move 0 0 N» de QEMU es un paso: tres líneas); luego el
  # Bloc, Mi equipo, el Administrador de dispositivos y MUESTRA, lanzados desde ella
  CAPTURAS=$G DISCO=$W/d.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "type largo.txt" ":pausa 3" ":raton 300 300" \
    ":monitor mouse_move 0 0 1" ":pausa 1" ":monitor mouse_move 0 0 1" ":pausa 1" ":captura ru1" \
    "bloc largo.txt" ":pausa 3" ":captura ru2" ":raton 300 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru3" \
    ":tecla right" ":pausa 1" ":captura ru4" ":clic 606 102" ":pausa 1" \
    "equipo c:\\muchos" ":pausa 4" ":raton 400 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru5" \
    ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 -1" ":pausa 2" ":captura ru6" \
    ":tecla f8" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru7" ":clic 775 21" ":pausa 1" \
    "disposit" ":pausa 4" ":tecla shift-bracket_right" ":pausa 1" ":raton 400 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru8" ":clic 745 31" ":pausa 1" \
    "muestra normal" ":pausa 4" ":raton 400 350" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":captura ru9" \
    ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 1" ":pausa 2" ":captura ru10" ":tecla down" ":pausa 1" ":tecla down" ":pausa 1" ":captura ru11" > $W/r.sal 2>&1
  tr -d '\r' < $W/r.sal > $W/r.txt
  S=$W/r.txt
  mira "la Consola, el Bloc, Mi equipo y el Administrador de dispositivos, lanzados" \
    "escritorio: lanzo .CONSOLA.BIN" 
  miracap "la Consola: la rueda hacia arriba enseña lo que había subido (dos pasos, seis líneas: la 371 arriba, en otro azul) y el título lo dice" ru1 \
    negrita 90 60 600 90 "Consola - historial: 6 l" texto 90 110 400 130 "linea 371 de un texto largo"
  miracap "el Bloc de notas con LARGO.TXT (y la Consola, al escribir, otra vez abajo)" ru2 texto 110 170 400 200 "linea 001 de un texto largo" notexto 80 60 600 90 "historial"
  miracap "el Bloc: dos pasos de rueda hacia abajo, seis líneas (la 007 arriba), y el cursor donde estaba (Lín 1)" ru3 \
    texto 110 170 400 200 "linea 007 de un texto largo" texto 110 455 400 480 "Lín 1, Col 1"
  miracap "y con una tecla (la flecha), otra vez al cursor" ru4 texto 110 170 400 200 "linea 001 de un texto largo" texto 110 455 400 480 "Lín 1, Col 2"
  miracap "Mi equipo: un paso de rueda, una fila de iconos (A06 arriba), sin elegir nada" ru5 texto 210 160 300 200 A06.TXT notexto 210 160 300 200 A01.TXT texto 10 478 300 500 "Nada elegido"
  miracap "Ctrl+rueda hacia abajo: la vista, más pequeña (de los iconos a la lista)" ru6 texto 210 130 320 160 A01.TXT texto 440 130 560 160 A16.TXT
  miracap "en los detalles (F8), la rueda baja seis renglones de la rejilla" ru7 negrita 220 150 320 180 A07.TXT notexto 210 130 320 470 A01.TXT
  # (0.8.12, RECOGIDO) se abre con lo que funciona recogido: desplegado todo con
  # «*» (21 renglones; ya no hay «Procesador»), la rueda baja hasta el final
  miracap "el Administrador de dispositivos: la rueda baja la lista y la elegida («Pantalla», arriba) se queda elegida, aunque ya no se vea" ru8 \
    notexto 40 120 400 520 "[-] Pantalla (2)" negrita 40 120 400 520 "[-] Sistema y ACPI (8)" texto 40 520 300 545 "16 dispositivos"
  miracap "MUESTRA: la rueda baja la rejilla seis renglones (el 20, arriba) sin mover la elegida (la 8, que ya no se ve)" ru9 \
    negrita 0 0 400 30 "Muestra de controles" texto 40 195 300 220 "Persona de ejemplo 20" notexto 0 190 800 520 "Persona de ejemplo 8"
  miracap "MUESTRA: Ctrl+rueda hacia arriba, la letra más grande (vez y media)" ru10 negrita15 0 150 800 300 "Nombre"
  miracap "y con la flecha abajo (la primera lleva el teclado a la rejilla), la elegida pasa a la siguiente (la 15) y se ve otra vez" ru11 negrita15 0 150 800 400 "Persona de ejemplo 15"
  [ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/ru*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
else ko "no se puede hacer el disco: $(tail -2 $W/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la rueda)"
fi   # la rueda

# ---------------------------------------------- 23. (MENÚ) el menú Inicio y los atajos
if parte08 menu; then
echo "== Hispa-Pentium: el menú Inicio, los menús largos y los atajos de teclado de todo el sistema (MENÚ) =="
W=$P/menu
rm -rf $W; mkdir -p $W/mnu $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN || ko "semilla.cas no compila"
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/ularga.cas -o $W/x.asm > /dev/null && nasm -f bin $W/x.asm -o $W/LARGA.BIN || ko "ularga.cas no compila"
if $AQUI/nucleo32/disco.sh $W/d.img 32 oficina > $W/disco.txt 2>&1; then
  # 24 juegos (más de los que caben a lo alto: el submenú se desplaza), un
  # acceso de «Herramientas» (a Accesorios) y uno sin tipo (el Navegador:
  # a Internet, por su programa); 8 documentos más en la raíz
  for i in $(seq -w 1 24); do printf 'nombre=Juego %s\r\nprograma=\\HOLA.BIN\r\ntipo=Juegos y emuladores\r\n' $i > $W/mnu/J$i.MNU; done
  printf 'nombre=Saludo\r\nprograma=\\HOLA.BIN\r\ntipo=Herramientas\r\n' > $W/mnu/SALUDO.MNU
  printf 'nombre=Navegador\r\nprograma=\\HISPA\\NAVEGA\\NAVEGA.BIN\r\n' > $W/mnu/NAVEGA.MNU
  for i in 1 2 3 4 5 6 7 8; do printf 'documento %s\r\n' $i > $W/DOC$i.TXT; done
  mmd -i $W/d.img ::MENU
  mcopy -i $W/d.img $W/mnu/*.MNU ::MENU/
  mcopy -i $W/d.img $W/DOC*.TXT $W/LARGA.BIN ::/
  # 1. el menú: su forma, Todos los programas, una carpeta larga (con la
  # flecha en diagonal: no se cierra), la rueda, el teclado y Mis documentos
  CAPTURAS=$G DISCO=$W/d.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":clic 40 585" ":pausa 1" ":captura mn1" ":raton 100 502" ":pausa 1" ":captura mn2" ":raton 300 428" ":pausa 1" ":captura mn3" \
    ":raton 560 300" ":monitor mouse_move 0 0 -4" ":pausa 0.5" ":monitor mouse_move 0 0 -4" ":pausa 1" ":captura mn4" ":tecla esc" ":tecla esc" ":tecla esc" ":pausa 1" ":captura mn5" \
    ":tecla meta_l" ":pausa 1" ":tecla t" ":tecla right" ":tecla a" ":tecla right" ":pausa 1" ":captura mn6" ":tecla s" ":tecla ret" ":pausa 2" \
    ":clic 40 585" ":clic 300 171" ":pausa 1" ":captura mn7" ":clic 500 171" ":pausa 2" ":captura mn8" > $W/m.sal 2>&1
  tr -d '\r' < $W/m.sal > $W/m.txt
  S=$W/m.txt
  miracap "el menú Inicio: los fijados (Bloc de notas y Consola, con su explicación), los más usados (los de los paquetes), «Todos los programas», la columna de la derecha y el pie con Cerrar sesión, Reiniciar y Apagar equipo" mn1 \
    negrita 46 155 54 163 "Bloc de notas" texto 46 175 54 183 "Escribir un texto" texto 38 246 46 254 "Juego 01" negrita 58 492 66 500 "Todos los programas" \
    negrita 256 159 264 167 "Mis documentos" negrita 256 192 264 200 "Mi equipo" negrita 256 226 264 234 "Configuración" negrita 256 256 264 264 "Administrador de" \
    negrita 256 296 264 304 "Programas instalados" negrita 256 330 264 338 "Actualizar" negrita 256 363 264 371 "Ayuda" negrita 256 393 264 401 "Ejecutar..." \
    negrita 68 533 76 541 "Cerrar sesión" negrita 210 533 222 541 Reiniciar negrita 320 533 328 541 "Apagar equipo"
  # ((OFICINA2) con Pintura, también Multimedia: las demás, una más arriba)
  miracap "«Todos los programas» (con la flecha encima) abre sus carpetas: Accesorios, Internet (el Navegador, por su programa), Juegos (por su «tipo»), Multimedia (Pintura), Oficina y Sistema" mn2 \
    texto 250 364 258 372 Accesorios texto 250 390 258 398 Internet texto 250 416 258 424 Juegos texto 250 442 258 450 Multimedia texto 250 468 258 476 Oficina texto 250 494 258 502 Sistema
  miracap "una carpeta con más de los que caben a lo alto: el submenú no crece más que la pantalla y lleva sus flechas (el 01 arriba, el 24 no se ve)" mn3 \
    texto 486 30 494 40 "Juego 01" notexto 460 0 700 570 "Juego 24" negrita 320 533 328 541 "Apagar equipo"
  miracap "la flecha, en diagonal hasta él, no lo cierra; y la rueda lo baja: el 24, a la vista" mn4 texto 460 0 700 570 "Juego 24" notexto 460 0 700 570 "Juego 01"
  miracap "Esc cierra un submenú cada vez, y luego el menú" mn5 notexto 0 90 440 570 "Todos los programas"
  miracap "con el teclado: el logo, la «t» (Todos los programas), la flecha a la derecha, la «a» (Accesorios) y otra vez a la derecha: su submenú, la primera elegida" mn6 \
    texto 486 364 494 372 "Bloc de notas" texto 486 442 494 450 "Saludo"
  mira "y la «s» e Intro lanzan «Saludo» (un acceso de un paquete)" "escritorio: lanzo .HOLA.BIN"
  miracap "«Mis documentos» abre su submenú con los .TXT de la raíz" mn7 texto 460 160 480 182 "LEEME.TXT" texto 460 0 700 570 "DOC8.TXT"
  mira "y uno de ellos se abre con el Bloc de notas" "escritorio: lanzo .BLOC.BIN"
  # 2. a 640 x 480, a 1024 x 600 y a 1280 x 724 (0.8.5: la pantalla de Carlos
  # en VirtualBox, 1282 x 724, donde el menú de la 0.8.4 tapaba la barra y
  # «Apagar»): el pie, siempre a la vista, y la barra de tareas con «Inicio»
  # sin tapar; también con una carpeta de más programas de los que caben
  # (los 24 juegos, y 30 y tantos en «Todos los programas»)
  for m in 640x480 1024x600 1280x724; do
    al=${m#*x}
    CAPTURAS=$G DISCO=$W/d.img ARRANQUE="escritorio modo=$m" PANTALLA=$m MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN ":pausa 3" ":clic 40 $((al - 15))" ":pausa 1" ":raton 470 $((al - 200))" ":pausa 1" ":captura mp$m" \
      ":raton 100 $((al - 104))" ":pausa 1" ":raton 300 $((al - 104))" ":pausa 1" ":raton 300 $((al - 156))" ":pausa 1" ":raton 450 $((al - 156))" ":pausa 1" ":captura mq$m" > $W/p$m.sal 2>&1
    miracap "a $m: el menú cabe encima de la barra, con «Todos los programas» y «Apagar equipo» a la vista, y la barra con «Inicio» sin tapar" mp$m \
      negrita 58 $((al - 108)) 66 $((al - 100)) "Todos los programas" negrita 320 $((al - 67)) 328 $((al - 59)) "Apagar equipo" negrita 46 0 54 $((al - 120)) "Bloc de notas" \
      negrita 30 $((al - 30)) 80 $((al - 5)) Inicio
    miracap "a $m, con la carpeta de los 24 juegos abierta: no baja de la barra; «Apagar equipo» e «Inicio», a la vista" mq$m \
      texto 0 0 ${m%x*} $((al - 28)) "Juego 01" negrita 320 $((al - 67)) 328 $((al - 59)) "Apagar equipo" negrita 30 $((al - 30)) 80 $((al - 5)) Inicio
  done
  # 3. los atajos de teclado de todo el sistema
  cp $W/d.img $W/e.img
  CAPTURAS=$G ESCRIBE=1 DISCO=$W/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 60 110" ":pausa 2" ":doble 60 190" ":pausa 2" ":tecla alt-tab" ":pausa 1" ":captura at1" \
    ":tecla alt-spc" ":pausa 1" ":captura at2" ":tecla down" ":tecla down" ":tecla ret" ":tecla right" ":tecla right" ":tecla right" ":tecla down" ":tecla ret" ":pausa 1" ":captura at3" \
    ":tecla ctrl-alt-delete" ":pausa 1" ":captura at4" ":tecla esc" ":tecla meta_l-r" ":pausa 1" ":escribe larga" ":captura at5" ":tecla ret" ":pausa 2" \
    ":clic 300 200" ":pausa 1" ":captura at6" ":raton 380 300" ":monitor mouse_move 0 0 -5" ":pausa 1" ":captura at7" ":tecla end" ":tecla ret" ":pausa 1" \
    ":tecla print" ":pausa 5" ":captura at8" ":tecla meta_l-d" ":pausa 1" ":captura at9" ":tecla meta_l-d" ":pausa 1" ":captura at10" \
    ":tecla alt-f4" ":pausa 2" ":clic 792 300" ":tecla alt-f4" ":pausa 1" ":captura at11" ":tecla right" ":tecla right" ":pausa 1" ":captura at12" ":tecla ret" ":pausa 3" ":captura at13" \
    ":tecla ret" ":pausa 1" ":clic 792 300" ":tecla f1" ":pausa 1" ":tecla ret" ":tecla meta_l-e" ":pausa 3" ":captura at14" > $W/a.sal 2>&1
  tr -d '\r' < $W/a.sal > $W/a.txt
  S=$W/a.txt
  mira "Alt+Tab: la franja con las dos ventanas, y al soltar Alt se queda la de detrás (el Bloc de notas)" "^escritorio: Alt+Tab, con 2 ventanas$" "^escritorio: Alt+Tab, a la 1$"
  miracap "Alt+Tab trae delante el Bloc de notas (estaba detrás de la Consola)" at1 negrita 100 60 130 80 "NOTAS.TXT - Bloc de notas"
  miracap "Alt+Espacio: el menú de la ventana (Restaurar, Mover, Tamaño, Minimizar, Maximizar y Cerrar)" at2 texto 90 80 140 110 Restaurar texto 90 120 140 160 "Tamaño" texto 90 210 140 240 Cerrar
  miracap "«Tamaño» con las flechas: la ventana, más ancha y 16 puntos más alta (la barra de estado del Bloc, más abajo)" at3 negrita 100 60 130 80 "NOTAS.TXT - Bloc de notas" texto 94 446 102 470 NOTAS.TXT
  miracap "Ctrl+Alt+Supr: el Administrador de tareas, con los programas abiertos" at4 negrita 160 60 300 200 "Los programas abiertos" texto 180 180 300 220 "NOTAS.TXT - Bloc de notas" texto 180 200 300 240 Consola
  miracap "Logo+R: «Ejecutar», con lo que se escribe" at5 negrita 140 200 260 300 "Abrir:" texto 200 250 300 300 larga
  mira "y lanza LARGA.BIN (sin .BIN: se pone solo)" "escritorio: lanzo .LARGA.BIN"
  miracap "un menú emergente de 30 opciones (más de las que caben): se ve lo que cabe, con sus flechas" at6 texto 250 0 450 60 "Opción número 01" notexto 250 0 450 600 "Opción número 30"
  miracap "y la rueda lo desplaza" at7 notexto 250 0 450 60 "Opción número 01" texto 250 0 450 80 "Opción número 0"
  mira "Fin e Intro eligen la última (la 30)" "^larga: elegida 30$"
  mira "Impr Pant: una captura de toda la pantalla en \\CAPTURAS (un BMP)" "^escritorio: captura de 800 puntos de ancho en .CAPTURAS.CAPT0001.BMP$"
  miracap "y lo dice en un globo encima de la bandeja" at8 negrita 450 500 800 570 "Captura de la pantalla"
  rm -f $W/capt.bmp; mcopy -n -i $W/e.img ::CAPTURAS/CAPT0001.BMP $W/capt.bmp > /dev/null 2>&1
  if [ "$(head -c 2 $W/capt.bmp 2> /dev/null)" = BM ] && [ "$(stat -c %s $W/capt.bmp 2> /dev/null)" = 1440054 ] && fsck.fat -n $W/e.img > /dev/null 2>&1; then
    ok "el BMP está en el disco: 800 x 600 con 24 bits (1.440.054 octetos), y el disco, sano (fsck.fat)"
  else ko "la captura no está bien en el disco ($(stat -c %s $W/capt.bmp 2> /dev/null) octetos)"; fi
  miracap "Logo+D enseña el escritorio (lo minimiza todo)" at9 notexto 80 60 700 100 "NOTAS.TXT - Bloc de notas" notexto 80 60 700 100 "Menú largo"
  miracap "y otra vez Logo+D lo devuelve" at10 negrita 80 60 700 120 "NOTAS.TXT - Bloc de notas"
  miracap "Alt+F4 con el tapiz elegido: «Apagar el equipo», con Apagar elegido (Apagar, Reiniciar, Cerrar sesión y Cancelar)" at11 \
    negrita 192 312 208 326 Apagar negrita 300 312 316 326 Reiniciar negrita 404 312 420 326 "Cerrar sesión" negrita 544 312 560 326 Cancelar rgb 166 306 14294A 30
  miracap "las flechas llevan lo elegido a «Cerrar sesión» (el borde oscuro, alrededor)" at12 negrita 404 312 420 326 "Cerrar sesión" rgb 406 306 14294A 30 nrgb 166 306 14294A 30
  mira "Intro cierra la sesión (se cierran los programas) y vuelve con «Entrar»" "^escritorio: sesión cerrada$" "^escritorio: sesión abierta$"
  miracap "el cuadro de la sesión cerrada" at13 negrita 200 200 600 300 "La sesión se ha cerrado." negrita 200 200 600 400 "Entrar"
  mira "F1 en el tapiz: la Ayuda (en este disco no hay MANUAL.TXT: lo dice)" "^escritorio: aviso: La Ayuda (MANUAL.TXT) no está en este sistema.$"
  mira "Logo+E: el explorador (Mi equipo)" "escritorio: lanzo .EQUIPO.BIN"
  if ! grep -q "excepción\|\*\*\*" $W/m.txt $W/a.txt; then ok "en esas sesiones, ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep -h "excepción\|\*\*\*" $W/m.txt $W/a.txt | head -4 | sed 's/^/        /'; fi
  [ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/mn*.ppm $G/mp*.ppm $G/at*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
else ko "no se puede hacer el disco: $(tail -2 $W/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el menú Inicio)"
fi   # el menú Inicio
# ------------------------------------------------- 25. (PROTEGE) la zona protegida
# La revisión de seguridad del 21/11/2026 (nucleo32/permisos.cas): en un
# sistema instalado (C:\HISPA en un disco SATA), un programa normal no puede
# escribir, borrar ni cambiar de nombre los archivos del sistema, ni con otras
# formas de escribir su nombre («\HISPA.», la «á» que la FAT lleva a la A);
# no lee los secretos de los servicios; y, aunque se llame RED.BIN o TLS.BIN,
# lanzado de otra carpeta no es un servicio (ni IOPL 3, ni memoria de DMA,
# ni el túnel, ni ACPI). PAQUETES (el de C:\HISPA) instala bien; una copia
# suya en otra carpeta, no. Los servicios siguen: la red (con un RED.BIN de
# mentira puesto en la raíz, que no se usa) y SSHD con sus llaves. Y el modo
# administrador (ADMIN), que el núcleo pregunta en la pantalla: en modo texto
# y con el escritorio (una ventanita suya).
if parte08 protege; then
echo "== Hispa-Pentium: la zona protegida (\\HISPA), los secretos, los servicios por su sello y el modo administrador =="
X=$P/protege
rm -rf $X; mkdir -p $X/f/paq $X/v
export MTOOLS_SKIP_CHECK=1
HPQ="python3 -I $AQUI/../tools/hpq.py"
hechos=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null 2>&1 && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || hechos=0
# PAQUETES con una clave de prueba (como en la parte de los paquetes)
$HPQ clave $X/prueba.key > /dev/null && $HPQ publica $X/prueba.key $X/clave.cas > /dev/null || hechos=0
cp $AQUI/nucleo32/upaquete.cas $X/upaquete.cas
python3 -I $AQUI/ccastel.py $X/upaquete.cas -o $X/upaquete.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $X/upaquete.asm -o $X/f/PAQUETES.BIN || hechos=0
# el programa «malo»: lo que no debe poder (se pone como RED.BIN y TLS.BIN en C:\OTRA)
cat > $X/ufalso.cas <<'CAS'
módulo UFalso
    modelo usuario
    importar Llamadas, Textos32
    variable dma: arreglo [4] de nat32
    variable pedido: arreglo [3] de nat32
    variable sector: arreglo [512] de byte
    función sc(n: nat32, a: nat32, b: nat32, c: nat32): nat32
        ensamblador
            mov eax, {n}
            mov ebx, {a}
            mov ecx, {b}
            mov edx, {c}
            int 0x80
        fin
    fin
    función dice(qué: puntero a byte, r: nat32)
        si es_error(r) entonces escribir("falso: ", qué, ": no (", texto_error(r), ")\n") sino escribir("falso: ", qué, ": SE PUEDE\n") fin
    fin
    función abre(qué: puntero a byte, r: nat32)
        dice(qué, r)
        si no es_error(r) entonces cerrar(r) fin
    fin
    función principal(): byte
        dice("apuntarse como servicio (la 91)", sc(91, 0, 0, 0))
        dice("memoria de DMA (la 94)", sc(94, 1, nat32(dirección(dma[0])), 0))
        dice("apuntar un dispositivo (la 156)", sc(156, 0, 1, 0))
        dice("la memoria física de ACPI (la 151)", sc(151, 0, 4096, 0))
        dice("poner el reloj del sistema (la 67)", sc(67, (20 << 9) ob (1 << 5) ob 1, 0, 0))   ; (0.8.5) atrasarlo al 1/1/2000
        dice("el sello de otra tarea (la 158)", sc(158, 1, 0, 0))
        pedido[0] := 0
        pedido[1] := 1
        pedido[2] := nat32(dirección(sector[0]))
        dice("leer el disco en crudo (la 64)", sc(64, 0, nat32(dirección(pedido[0])), 0))
        abre("escribir \\HISPA\\NUCLEO.BIN", crear("\\HISPA\\NUCLEO.BIN"))
        abre("escribir \\HISPA.\\NUCLEO.BIN", crear("\\HISPA.\\NUCLEO.BIN"))
        abre("escribir \\hisp\xA0\\nucleo.bin", crear("\\hisp\xA0\\nucleo.bin"))
        abre("abrir \\HISPA\\RED.BIN para escribir", abrir_para("\\HISPA\\RED.BIN", PARA_ESCRIBIR))
        dice("borrar \\HISPA\\RED.BIN", borrar("\\HISPA\\RED.BIN"))
        dice("cambiar el nombre de \\HISPA\\RED.BIN", renombrar("\\HISPA\\RED.BIN", "\\HISPA\\RED.ANT"))
        dice("llevarse \\HISPA\\RED.BIN a la raíz", renombrar("\\HISPA\\RED.BIN", "\\R.BIN"))
        dice("meter un archivo en \\HISPA", renombrar("\\DOCUMENT\\LEEME.TXT", "\\HISPA\\LEEME.TXT"))
        dice("hacer la carpeta \\HISPA\\NUEVA", crear_carpeta("\\HISPA\\NUEVA"))
        dice("borrar la carpeta \\HISPA\\PAQUETES", borrar_carpeta("\\HISPA\\PAQUETES"))
        dice("ocultar \\HISPA\\RED.BIN", poner_atributos("\\HISPA\\RED.BIN", 2))
        abre("poner un ORDENES.BIN en la raíz", crear("\\ORDENES.BIN"))
        abre("leer \\HISPA\\WIFI.INI", abrir("\\HISPA\\WIFI.INI"))
        abre("leer \\HISPA\\TLS.SEM", abrir("\\HISPA\\TLS.SEM"))
        abre("leer \\HISPA\\HISPA.INI", abrir("\\HISPA\\HISPA.INI"))
        abre("escribir \\DOCUMENT\\NOTA.TXT", crear("\\DOCUMENT\\NOTA.TXT"))
        devolver 0
    fin
fin UFalso.
CAS
python3 -I $AQUI/ccastel.py $X/ufalso.cas -o $X/ufalso.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $X/ufalso.asm -o $X/FALSO.BIN || hechos=0
# el disco «de arranque» (los programas en la raíz: disco.sh, con la red y SSH)
$AQUI/nucleo32/disco.sh $X/vivo.img 32 red ssh > $X/disco.txt 2>&1 || hechos=0
# un paquete de prueba (NOTA: \HISPA\NOTA.TXT) en una memoria USB con su catálogo firmado
printf 'una nota del paquete\r\n' > $X/f/NOTA.TXT
( cd $X/f && $HPQ crea paq/NOTA.HPQ --nombre nota --version 1.0 --descripcion 'Una nota' NOTA.TXT='\HISPA\NOTA.TXT' && $HPQ catalogo paq --clave $X/prueba.key \
  && $HPQ crea BASE.HPQ --nombre base --version 0.8 --descripcion 'El sistema base (de prueba)' PAQUETES.BIN='\HISPA\PAQUETES.BIN' && $HPQ registro BASE.HPQ BASE.PQI ) > $X/hpq.txt 2>&1 || hechos=0
mkfs.fat -F 16 -n MEMORIA -C $X/usb.img 16384 > /dev/null && mmd -i $X/usb.img ::PAQUETES && mcopy -i $X/usb.img $X/f/paq/* ::PAQUETES/ || hechos=0
# el sistema instalado: los programas de la memoria de arranque, en C:\HISPA
mkdir -p $X/v && mcopy -n -i $X/vivo.img '::*.BIN' $X/v/ > /dev/null 2>&1 || hechos=0
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\n' > $X/HISPA.INI
printf 'Red=AUTO,4361736100,secreto-de-la-wifi\r\n' > $X/WIFI.INI
printf 'semilla secreta de TLS\r\n' > $X/TLS.SEM
printf 'un documento\r\n' > $X/LEEME.TXT
mkfs.fat -F 32 -s 1 -n HISPA -C $X/sis.img 65536 > /dev/null && mmd -i $X/sis.img ::HISPA ::HISPA/PAQUETES ::OTRA ::DOCUMENT \
  && mcopy -i $X/sis.img $X/v/ORDENES.BIN $X/v/RED.BIN $X/v/IPCONFIG.BIN $X/v/PING.BIN $X/v/SSHD.BIN $X/v/SSH.BIN $X/f/PAQUETES.BIN $X/HISPA.INI $X/WIFI.INI $X/TLS.SEM ::HISPA/ \
  && mcopy -i $X/sis.img $X/f/BASE.PQI ::HISPA/PAQUETES/ && mcopy -i $X/sis.img $X/FALSO.BIN ::OTRA/RED.BIN && mcopy -i $X/sis.img $X/FALSO.BIN ::OTRA/TLS.BIN \
  && mcopy -i $X/sis.img $X/f/PAQUETES.BIN ::OTRA/PAQUETES.BIN && mcopy -i $X/sis.img $X/FALSO.BIN ::RED.BIN && mcopy -i $X/sis.img $X/LEEME.TXT ::DOCUMENT/ || hechos=0
cp $X/v/NUCLEO.BIN $X/NUCLEO.BIN 2> /dev/null || cp $X/SEMILLA.BIN $X/NUCLEO.BIN
mcopy -i $X/sis.img $X/NUCLEO.BIN ::HISPA/NUCLEO.BIN || hechos=0
if [ $hechos = 1 ]; then
  ok "el disco instalado de las pruebas: C:\\HISPA con los programas, sus secretos (WIFI.INI, TLS.SEM) y PAQUETES; C:\\OTRA con un RED.BIN, un TLS.BIN y un PAQUETES.BIN que no son del sistema"
  # 1. en modo texto (ORDENES), con la red y la memoria USB de los paquetes
  cp $X/sis.img $X/s1.img
  ESCRIBE=1 SATA=$X/s1.img MEMORIA=64 MODULOS= ARRANQUE=ordenes QEMU_MAS="-nic user,model=e1000 -device usb-ehci,id=u -drive id=mu,file=$X/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
    $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" "\\otra\\red" ":pausa 2" "\\otra\\tls" ":pausa 2" \
    "copy \\document\\leeme.txt \\hispa\\nucleo.bin" "del \\hispa\\red.bin" "type \\hispa\\wifi.ini" "md \\hispa\\otra" "type \\hispa\\hispa.ini" \
    "\\otra\\paquetes instala x:\\paquetes\\nota.hpq" ":pausa 3" "paquetes instala x:\\paquetes\\nota.hpq" ":pausa 4" \
    "ipconfig" ":pausa 8" "sshd /encender" ":pausa 25" "type \\hispa\\ssh\\llave.ssh" "type \\hispa\\ssh\\sshd.ini" \
    "admin" ":pausa 1" "s" "copy \\document\\leeme.txt \\hispa\\admin.txt" "type \\hispa\\wifi.ini" "admin no" "del \\hispa\\admin.txt" ":pausa 1" > $X/t.sal 2>&1
  tr -d '\r' < $X/t.sal > $X/t.txt
  S=$X/t.txt
  mira "un programa que se llama RED.BIN (o TLS.BIN), lanzado de C:\\OTRA, no es un servicio: ni se apunta (91), ni memoria de DMA (94), ni dispositivos (156), ni ACPI (151), ni el sello de otro (158)" \
    "^falso: apuntarse como servicio (la 91): no (" "^falso: memoria de DMA (la 94): no (" "^falso: apuntar un dispositivo (la 156): no (" \
    "^falso: la memoria física de ACPI (la 151): no (" "^falso: el sello de otra tarea (la 158): no ("
  mira "(0.8.5) ni poner el reloj del sistema (la 67: atrasarlo haría pasar certificados caducados): acceso denegado" \
    "^falso: poner el reloj del sistema (la 67): no (acceso denegado" "permisos: acceso denegado a poner el reloj del sistema"
  if [ "$(grep -c '^falso: apuntarse como servicio (la 91): no' $S)" = 2 ] && ! grep -q "SE PUEDE.*la 9[14]\|la 91.*SE PUEDE" $S; then ok "ni el RED.BIN de mentira ni el TLS.BIN de mentira (los dos se apuntarían por su nombre antes de la 0.8.4)"
  else ko "un programa de otra carpeta se ha podido apuntar como servicio:"; grep "^falso" $S | head -4 | sed 's/^/        /'; fi
  mira "un programa normal no puede escribir NUCLEO.BIN (ni como «\\HISPA.\\» ni con la «á» que la FAT lleva a la A), ni abrir RED.BIN para escribir, ni borrarlo, ni cambiarle el nombre, ni llevárselo, ni meter nada en \\HISPA, ni hacer o borrar carpetas, ni ocultar archivos, ni poner un ORDENES.BIN en la raíz: acceso denegado" \
    "^falso: escribir \\\\HISPA\\\\NUCLEO.BIN: no (acceso denegado" "^falso: escribir \\\\HISPA.\\\\NUCLEO.BIN: no (acceso denegado" "^falso: escribir .hisp.\\\\nucleo.bin: no (acceso denegado" \
    "^falso: abrir \\\\HISPA\\\\RED.BIN para escribir: no (acceso denegado" "^falso: borrar \\\\HISPA\\\\RED.BIN: no (acceso denegado" "^falso: cambiar el nombre de \\\\HISPA\\\\RED.BIN: no (acceso denegado" \
    "^falso: llevarse \\\\HISPA\\\\RED.BIN a la raíz: no (acceso denegado" "^falso: meter un archivo en \\\\HISPA: no (acceso denegado" "^falso: hacer la carpeta \\\\HISPA\\\\NUEVA: no (acceso denegado" \
    "^falso: borrar la carpeta \\\\HISPA\\\\PAQUETES: no (acceso denegado" "^falso: ocultar \\\\HISPA\\\\RED.BIN: no (acceso denegado" "^falso: poner un ORDENES.BIN en la raíz: no (acceso denegado"
  mira "ni leer los secretos (WIFI.INI, TLS.SEM) ni el disco en crudo (64); lo que no es secreto (HISPA.INI) sí se lee, y los datos del usuario (C:\\DOCUMENT) se escriben" \
    "^falso: leer \\\\HISPA\\\\WIFI.INI: no (acceso denegado" "^falso: leer \\\\HISPA\\\\TLS.SEM: no (acceso denegado" "^falso: leer el disco en crudo (la 64): no (acceso denegado" \
    "^falso: leer \\\\HISPA\\\\HISPA.INI: SE PUEDE$" "^falso: escribir \\\\DOCUMENT\\\\NOTA.TXT: SE PUEDE$"
  mira "y desde el intérprete de órdenes (COPY, DEL, TYPE, MD): el mismo «acceso denegado», con lo que hay que hacer (ADMIN)" \
    "^No puedo .*nucleo.bin: acceso denegado: es del sistema (para cambiarlo a mano: ADMIN)" "^No puedo borrar .*red.bin: acceso denegado" \
    "acceso denegado" "^usuario=Ana$"
  mira "una copia de PAQUETES en C:\\OTRA no instala nada (acceso denegado); el de C:\\HISPA, sí (NOTA, en \\HISPA)" \
    "^paquetes: acceso denegado a C:.HISPA.PAQUETES: solo instala el PAQUETES del sistema" "^  instalado nota 1\.0$"
  mira "los servicios siguen: la red (el RED.BIN de C:\\HISPA, aunque haya otro en la raíz) y SSHD (su llave, nueva, en C:\\HISPA\\SSH; escucha en el 22)" \
    "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^  Dirección IP  . . . . : 10.0.2.15$" "^Servidor SSH encendido (puerto 22)" "sshd: llave del equipo nueva (ssh-ed25519)$" "red: tcp: escuchando en el puerto 22$"
  if ! grep -q "^falso: apuntarse" <(sed -n '/^C:.>ipconfig/,$p' $S); then ok "el RED.BIN de mentira de la raíz no se lanza: el servicio sale de C:\\HISPA"
  else ko "el núcleo ha lanzado el RED.BIN de la raíz"; fi
  mira "la llave del servidor SSH no la lee nadie más (TYPE: acceso denegado); su SSHD.INI, sí" "^No puedo .*llave.ssh: acceso denegado" "^encendido=1$"
  mira "ADMIN: el núcleo pregunta (S o N) y, con S, se pueden cambiar los archivos del sistema y leer los secretos; ADMIN NO lo deja" \
    "El núcleo pregunta: ORDENES.BIN pide el MODO ADMINISTRADOR" "^permisos: modo administrador para ORDENES.BIN" "^Modo administrador: puede cambiar los archivos del sistema" \
    "^Red=AUTO,4361736100,secreto-de-la-wifi$" "^Modo administrador: dejado" "^No puedo borrar .*admin.txt: acceso denegado"
  if sano $X/s1.img && saca $X/s1.img HISPA/NUCLEO.BIN $X/n.bin && cmp -s $X/n.bin $X/NUCLEO.BIN && saca $X/s1.img HISPA/RED.BIN $X/r.bin && cmp -s $X/r.bin $X/v/RED.BIN \
    && saca $X/s1.img HISPA/NOTA.TXT $X/nota.txt && cmp -s $X/nota.txt $X/f/NOTA.TXT && saca $X/s1.img HISPA/ADMIN.TXT $X/a.txt && saca $X/s1.img DOCUMENT/NOTA.TXT $X/d.txt \
    && saca $X/s1.img HISPA/SSH/LLAVE.SSH $X/l.bin && ! mdir -i $X/s1.img ::HISPA/NUEVA > /dev/null 2>&1 && ! mdir -i $X/s1.img ::HISPA/OTRA > /dev/null 2>&1 && ! saca $X/s1.img ORDENES.BIN $X/o.bin; then
    ok "y en el disco, desde fuera: sano; NUCLEO.BIN y RED.BIN, intactos; la nota de PAQUETES, el archivo del administrador, la nota del usuario y la llave de SSHD, puestos; nada más en \\HISPA"
  else ko "el disco no queda como tiene que quedar:"; mdir -/ -b -i $X/s1.img :: 2> /dev/null | head -20 | sed 's/^/        /'; fi
  # 2. con el escritorio: ADMIN en la Consola saca la pregunta del núcleo (una ventanita suya, no del programa)
  cp $X/vivo.img $X/e.img
  CAPTURAS=$X ESCRIBE=1 DISCO=$X/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 2" ":doble 60 186" ":pausa 4" "md \\hispa" ":pausa 1" \
    "admin" ":pausa 2" ":captura ad1" ":tecla down" ":tecla ret" ":pausa 1" "md \\hispa" ":pausa 1" ":captura ad2" \
    "admin no" ":doble 60 40" ":pausa 3" ":tecla f4" ":escribe C:\\HISPA" ":tecla ret" ":pausa 2" ":captura ad3" > $X/e.sal 2>&1
  tr -d '\r' < $X/e.sal > $X/e.txt
  S=$X/e.txt
  G=$X
  miracap "con el escritorio, ADMIN saca la pregunta del núcleo en medio de la pantalla: quién lo pide, para qué, «Sí, dárselo» y «No»" ad1 \
    texto 200 220 320 260 "ORDENES.BIN pide el modo administrador" texto 200 280 320 320 "Sí, dárselo" texto 200 300 320 340 "No"
  mira "elegido «Sí», ORDENES (en la Consola) tiene el modo administrador" "^permisos: modo administrador para ORDENES.BIN"
  miracap "Mi equipo, en C:\\HISPA: avisa de que la carpeta del sistema solo se mira (y de cómo cambiarla: Herramientas > Modo administrador)" ad3 \
    texto 0 300 800 600 "Carpeta del sistema: solo se puede mirar"
  if mdir -i $X/e.img ::HISPA > /dev/null 2>&1; then ok "y entonces, solo entonces, puede hacer C:\\HISPA (antes: acceso denegado)"
  else ko "con el modo administrador no se ha hecho \\HISPA"; fi
  if ! cat $X/t.txt $X/e.txt | grep -q "excepción\|\*\*\*"; then ok "en esos arranques, ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; cat $X/t.txt $X/e.txt | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
else ko "no se han podido hacer los programas o los discos de las pruebas de la zona protegida: $(tail -n 2 $X/disco.txt) $(tail -n 2 $X/hpq.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin la zona protegida)"
fi   # la zona protegida
# ------------------------------------------------- 26. (ESCUDO) Escudo, el antivirus
# ESCUDO.BIN (nucleo32/escudo/): el motor propio con las firmas de ClamAV. La
# base pequeña de prueba (escudo/prueba/hazprueba.py) y sus archivos, con lo
# que tiene que decir de cada uno (ESPERADO.TXT; escudo/prueba/compara.py).
if parte08 escudo; then
echo "== Hispa-Pentium: Escudo, el antivirus (ESCUDO.BIN, con las firmas de ClamAV) =="
E=$P/escudo
rm -rf $E; mkdir -p $E $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/nucleo32/escudo/prueba/hazprueba.py $E/p > /dev/null
CMP="python3 -I $AQUI/nucleo32/escudo/prueba/compara.py $E/p/ESPERADO.TXT"
FIRMAS=$(for f in $E/p/FIRMAS/*; do echo "-f $f"; done)
if bash $AQUI/nucleo32/escudo/hacer.sh --anfitrion $E/anf > $E/hacer.txt 2>&1; then
  # 1. el motor, en Linux
  # shellcheck disable=SC2086
  $E/anf $FIRMAS $E/p/ARCHIVOS > $E/l1.txt 2> $E/l1.err
  if $CMP $E/l1.txt > $E/c1.txt; then ok "el motor (en Linux): $(tail -1 $E/c1.txt | sed 's/^compara.py: //'): sumas MD5/SHA-1/SHA-256 (y de cualquier tamaño), de sección PE, comodines, desplazamientos, punto de entrada, lógicas con cuentas y ::iwaf, ZIP dentro de ZIP, limpias (.fp) e ignoradas (.ign2)"
  else ko "el motor (en Linux) no dice lo que tiene que decir:"; sed 's/^/        /' $E/c1.txt | head -8; fi
  S=$E/l1.err
  mira "lo que no sabe mirar se cuenta y se deja (el objetivo HTML, la PCRE); y la del nivel 240 no se carga" "^escudo: firmas: 4 sumas, 1 limpias, 8 del cuerpo, 4 lógicas .*; 2 no soportadas"
  # shellcheck disable=SC2086
  $E/anf -q $FIRMAS -o $E/PRUEBA.ESC && $E/anf -q -f $E/PRUEBA.ESC $E/p/ARCHIVOS > $E/l2.txt 2>&1
  if $CMP $E/l2.txt > $E/c2.txt; then ok "y lo mismo desde FIRMAS.ESC (el índice propio: las sumas en el disco por tramos; el cuerpo en bloques comprimidos y, en memoria, sus disparadores)"
  else ko "desde FIRMAS.ESC no dice lo mismo:"; sed 's/^/        /' $E/c2.txt | head -6; fi
  # (0.8.6) un .ESC sin nada (el DIA.ESC de un día sin firmas nuevas), cargado el
  # primero (Escudo carga primero DIA.ESC): antes salía «no hay memoria» y no se usaba
  $E/anf -q -o $E/VACIO.ESC > /dev/null 2>&1 && $E/anf -f $E/VACIO.ESC -f $E/PRUEBA.ESC $E/p/ARCHIVOS > $E/l3.txt 2> $E/l3.err
  if ! grep -q "no hay memoria" $E/l3.err && $CMP $E/l3.txt > $E/c3.txt; then ok "un .ESC vacío (un DIA.ESC sin firmas) cargado el primero: se usa, sin «no hay memoria», y lo demás dice lo mismo"
  else ko "un .ESC vacío cargado el primero no va:"; cat $E/l3.err $E/c3.txt 2> /dev/null | sed 's/^/        /' | head -6; fi
  # lo de la revisión de seguridad (21/11): los casos que encontró (FIRMAS.ESC
  # con la cabecera mentirosa, una tabla de bloques al revés, paréntesis sin
  # fin, variantes que piden 1 GB, una bomba ZIP de miembros solapados, un PE
  # con el punto de entrada que da la vuelta) y otros 2.000 estropeados al azar,
  # con el motor compilado con ASan y UBSan: ni un fallo, ni colgarse
  if bash $AQUI/nucleo32/escudo/hacer.sh --asan $E/asan > $E/asan.txt 2>&1; then
    C=$AQUI/nucleo32/escudo/prueba/casos
    malos=0
    for f in $C/*.ESC $C/*.ndb $C/*.ldb; do
      ASAN_OPTIONS=detect_leaks=0 timeout 60 $E/asan -q -f $f $E/p/ARCHIVOS/EICAR.COM > /dev/null 2> $E/caso.txt
      rc=$?; { [ $rc -ge 2 ] || grep -q 'ERROR: AddressSanitizer\|runtime error' $E/caso.txt; } && { malos=$((malos + 1)); echo "        $(basename $f): $rc $(tail -n 1 $E/caso.txt)"; }
    done
    for f in $C/*.zip $C/*.exe; do
      ASAN_OPTIONS=detect_leaks=0 timeout 60 $E/asan -q $FIRMAS $f > /dev/null 2> $E/caso.txt
      rc=$?; { [ $rc -ge 2 ] || grep -q 'ERROR: AddressSanitizer\|runtime error' $E/caso.txt; } && { malos=$((malos + 1)); echo "        $(basename $f): $rc $(tail -n 1 $E/caso.txt)"; }
    done
    [ $malos = 0 ] && ok "los $(ls $C | wc -l) casos de la revisión de seguridad (con ASan y UBSan): ni un fallo, ni colgarse" || ko "$malos casos de la revisión fallan"
    cp $E/PRUEBA.ESC $E/p/PRUEBA.ESC
    if python3 -I $AQUI/nucleo32/escudo/prueba/fuzz.py $E/asan $E/p 2000 > $E/fuzz.txt 2>&1; then ok "y 2.000 FIRMAS.ESC, firmas, ZIP y PE estropeados al azar: $(tail -n 1 $E/fuzz.txt | sed 's/^fuzz.py: //')"
    else ko "estropeados al azar:"; tail -n 4 $E/fuzz.txt | sed 's/^/        /'; fi
  else ko "hacer.sh --asan: $(tail -n 2 $E/asan.txt)"; fi
  # un .cvd de verdad, firmado con la clave de ClamAV (el de sus pruebas): si hay red
  if curl -sf --max-time 20 -o $E/test-3.cvd https://raw.githubusercontent.com/Cisco-Talos/clamav/main/unit_tests/input/freshclam_testfiles/test-3.cvd; then
    cp $E/test-3.cvd $E/toca.cvd
    python3 -I -c "import sys; d=bytearray(open(sys.argv[1],'rb').read()); d[3000]^=0x5A; open(sys.argv[1],'wb').write(d)" $E/toca.cvd
    if $E/anf -c $E/test-3.cvd | grep -q "firma digital BUENA" && ! $E/anf -c $E/toca.cvd > $E/toca.txt && grep -q "firma digital MALA" $E/toca.txt; then
      ok "un .cvd de ClamAV (test-3.cvd, de sus pruebas): su firma digital (RSA de la clave de ClamAV sobre la MD5) es buena; tocado un octeto, mala"
    else ko "la firma digital del .cvd: $($E/anf -c $E/test-3.cvd) / $(cat $E/toca.txt)"; fi
    $E/anf -f $E/test-3.cvd $E/p/ARCHIVOS/EICAR.COM > $E/l3.txt 2>&1
    $E/anf -f $E/toca.cvd $E/p/ARCHIVOS/EICAR.COM > $E/l4.txt 2>&1
    S=$E/l3.txt
    mira "y se descomprime (tar.gz) y se carga: sus sumas, las limpias y las de PUA (que no se cargan)" "firma digital de ClamAV buena (versión 3, 11 firmas, de raynman)" "^escudo: firmas: 5 sumas, 2 limpias, .* 4 PUA sin cargar"
    S=$E/l4.txt
    mira "el tocado no se usa" "la firma digital NO es buena"
  else echo "  (sin red: sin el .cvd de las pruebas de ClamAV)"; fi
else ko "escudo/hacer.sh --anfitrion: $(tail -3 $E/hacer.txt)"; fi
# 2. en Hispa-Pentium, desde el intérprete de órdenes: ESCUDO /LISTA, la
# cuarentena (/CUARENTENA), /ESTADO, /RESTAURA (igual octeto a octeto)
if $AQUI/nucleo32/disco.sh $E/d.img 32 escudo > $E/disco.txt 2>&1 && mmd -i $E/d.img ::ARCH && mcopy -i $E/d.img $E/p/ARCHIVOS/* ::ARCH/ \
   && mcopy -i $E/d.img $E/p/FIRMAS/* ::ESCUDO/FIRMAS/; then
  cp $E/d.img $E/o.img
  ESCRIBE=1 DISCO=$E/o.img ARRANQUE="ordenes escritura" MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 3" "escudo /lista C:\\ARCH" ":pausa 10" \
    "escudo /cuarentena C:\\ARCH\\EICAR.COM" ":pausa 3" "escudo /estado" ":pausa 3" "dir C:\\ARCH\\EICAR.COM" ":pausa 1" \
    "escudo /restaura 1" ":pausa 3" "escudo /estado" ":pausa 3" > $E/o.sal 2>&1
  tr -d '\r' < $E/o.sal > $E/o.txt
  if $CMP $E/o.txt > $E/c3.txt; then ok "en Hispa-Pentium (ESCUDO /LISTA C:\\ARCH): $(tail -1 $E/c3.txt | sed 's/^compara.py: //'), lo mismo que en Linux"
  else ko "ESCUDO /LISTA en Hispa-Pentium:"; sed 's/^/        /' $E/c3.txt | head -8; fi
  S=$E/o.txt
  mira "el resumen y el código de salida (1: hay algo)" "^Revisados: 35 archivos" "Encontrados: 21\.$" "(ESCUDO.BIN ha acabado con el código 1)"
  mira "ESCUDO /CUARENTENA: el archivo sale de su sitio y va a la cuarentena (CUARENTE: la FAT escribe 8+3)" \
    "^escudo: en cuarentena: C:.ARCH.EICAR.COM (Eicar-Test-Signature) -> Q0000001.CUA$" "^En cuarentena (C:.CUARENTE): 1\.$" \
    "^  1\. 20[0-9-]* [0-9:]*  Eicar-Test-Signature  (de C:.ARCH.EICAR.COM)$"
  mira "ESCUDO /RESTAURA 1: vuelve a su sitio" "^Devuelto a C:.ARCH.EICAR.COM\.$" "^En cuarentena (C:.CUARENTE): 0\.$"
  if saca $E/o.img ARCH/EICAR.COM $E/eicar.vuelto && cmp -s $E/eicar.vuelto $E/p/ARCHIVOS/EICAR.COM && saca $E/o.img ESCUDO/REGISTRO.TXT $E/reg.txt \
     && grep -q "restaurado de la cuarentena: C:.ARCH.EICAR.COM" $E/reg.txt; then
    ok "lo devuelto es igual octeto a octeto, y queda apuntado en el registro (\\ESCUDO\\REGISTRO.TXT)"
  else ko "lo devuelto no está, no es igual o no está en el registro"; fi
  sano $E/o.img
  grep -q "^$E/o.img: [0-9]* files" $P/fsck.txt && ok "y el disco queda sano (fsck.fat)" || { ko "el disco no queda sano:"; head -4 $P/fsck.txt | sed 's/^/        /'; }
  # 3. en el escritorio: una memoria USB en caliente (EHCI) con EICAR suelto,
  # en un ZIP y un AUTORUN.INF; «¿Revisar la memoria?», Intro, la revisión;
  # a la cuarentena los dos; y la pestaña Cuarentena. Y ESCUDO /DESCARGA (lo
  # que lanza el Navegador al acabar una descarga), desde la Consola.
  cp $E/d.img $E/e.img
  rm -f $E/m.img; dd if=/dev/zero of=$E/m.img bs=1M count=16 status=none; mkfs.fat -F 16 -n MEMORIA $E/m.img > /dev/null
  printf '[autorun]\r\nopen=EICAR.COM\r\n' > $E/AUTORUN.INF
  mcopy -i $E/m.img $E/p/ARCHIVOS/EICAR.COM $E/p/ARCHIVOS/EICAR.ZIP $E/p/ARCHIVOS/LIMPIO.TXT $E/AUTORUN.INF ::
  CAPTURAS=$G ESCRIBE=1 DISCO=$E/e.img ARRANQUE="escritorio escritura" MODULOS= QEMU_MAS="-device usb-ehci,id=u" \
    $AQUI/nucleo32/probar.sh "" ":pausa 4" ":captura es0" ":monitor drive_add 0 if=none,id=ma,file=$E/m.img,format=raw" \
    ":monitor device_add usb-storage,bus=u.0,drive=ma,id=mema" ":pausa 5" ":captura es1" ":tecla ret" ":pausa 6" ":captura es2" \
    ":tecla ret" ":pausa 2" ":tecla ret" ":pausa 2" ":clic 300 146" ":pausa 2" ":captura es3" ":clic 710 40" ":pausa 2" \
    ":doble 62 185" ":pausa 3" "escudo /descarga C:\\ARCH\\SUMAZIP.ZIP" ":pausa 4" ":captura es4" ":tecla ret" ":pausa 3" > $E/e.sal 2>&1
  tr -d '\r' < $E/e.sal > $E/e.txt
  S=$E/e.txt
  mira "el escritorio lanza ESCUDO /VIGILAR, que ve la memoria enchufada y pregunta (ESCUDO /PREGUNTA W:)" \
    "escudo: vigilando las unidades (memorias USB y CD)$" "^escudo: unidad nueva W: (MEMORIA, medio 3)$" "^escudo: pregunta W: revisar$"
  miracap "el escudo en la bandeja (a la derecha, junto al reloj)" es0 rgb 721 576 3b82f6 rgb 721 581 ffffff
  miracap "«¿Revisar la memoria USB W: (MEMORIA)?», con «Revisar» y «Ahora no»" es1 negrita 150 180 700 240 "¿Revisar la memoria USB W: (MEMORIA)?" \
    negrita 300 340 700 400 "Revisar" texto 300 340 700 400 "Ahora no"
  mira "la revisión de la memoria: EICAR suelto y en el ZIP, y el aviso de AUTORUN.INF" "^escudo: encontrado: W:.EICAR.COM: Eicar-Test-Signature$" \
    "^escudo: encontrado: W:.EICAR.ZIP > prueba/eicar.com: Eicar-Test-Signature$" "^escudo: ventana: revisión de W: acabada: 4 archivos, 3 encontrados$"
  miracap "lo encontrado, con su signo y su palabra ([!], [i]), y qué hacer con cada uno" es2 negrita 80 180 720 260 "[!] Encontrados: 3 (2 con firmas de virus)." \
    texto 80 230 720 320 "[!] W:\\EICAR.COM: Eicar-Test-Signature" texto 80 230 720 320 "[i] W:\\AUTORUN.INF: aviso: AUTORUN.INF" \
    negrita 80 450 300 500 "A cuarentena" texto 80 450 720 500 "Dejarlo"
  mira "Intro, Intro: los dos a la cuarentena" "^escudo: en cuarentena: W:.EICAR.COM (Eicar-Test-Signature) -> Q0000001.CUA$" \
    "^escudo: en cuarentena: W:.EICAR.ZIP (Eicar-Test-Signature) -> Q0000002.CUA$"
  miracap "la pestaña Cuarentena, con lo que ha entrado" es3 negrita 80 130 400 160 "Cuarentena (2)" texto 80 190 720 260 "Eicar-Test-Signature  (de W:\\EICAR.COM)"
  mira "ESCUDO /DESCARGA (lo que lanza el Navegador): encontrado, se avisa en su cuadro y va a la cuarentena con Intro" \
    "^escudo: descarga C:.ARCH.SUMAZIP.ZIP: Hispa.Prueba.SumaMD5-1$" "^escudo: aviso: C:.ARCH.SUMAZIP.ZIP: Hispa.Prueba.SumaMD5-1$" \
    "^escudo: en cuarentena: C:.ARCH.SUMAZIP.ZIP (Hispa.Prueba.SumaMD5-1) -> Q0000003.CUA$"
  miracap "el cuadro del aviso: dónde, qué firma, y «A cuarentena», «Borrarlo», «Dejarlo»" es4 negrita 100 150 720 240 "[!] Escudo ha encontrado algo en un archivo." \
    texto 100 150 720 320 "Hispa.Prueba.SumaMD5-1" negrita 100 300 720 460 "A cuarentena"
  if mdir -i $E/m.img ::EICAR.COM > /dev/null 2>&1 || mdir -i $E/m.img ::EICAR.ZIP > /dev/null 2>&1 || ! mdir -i $E/m.img ::LIMPIO.TXT > /dev/null 2>&1; then
    ko "en la memoria siguen EICAR.COM o EICAR.ZIP (o falta LIMPIO.TXT)"
  elif saca $E/e.img CUARENTE/Q0000001.CUA $E/q1 && python3 -I -c "import sys; d=open(sys.argv[1],'rb').read(); e=open(sys.argv[2],'rb').read(); sys.exit(bytes(b^0xA5 for b in d)!=e)" $E/q1 $E/p/ARCHIVOS/EICAR.COM; then
    ok "en la memoria ya no están (LIMPIO.TXT, sí); en la cuarentena del disco, con sus octetos cambiados (no se puede abrir ni lo encuentra otro antivirus)"
  else ko "la cuarentena no tiene lo que tiene que tener"; fi
  sano $E/m.img
  grep -q "^$E/m.img: [0-9]* files" $P/fsck.txt && ok "y la memoria queda sana" || { ko "la memoria no queda sana:"; head -4 $P/fsck.txt | sed 's/^/        /'; }
  if ! grep -q "excepción\|\*\*\*" $E/e.txt $E/o.txt; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep -h "excepción\|\*\*\*" $E/e.txt $E/o.txt | head -3 | sed 's/^/        /'; fi
  # 4. lo que tarda: 16 MB al azar con 20.000 patrones, 5.000 lógicas y 100.000 sumas
  if [ -x $E/anf ]; then
    python3 -I $AQUI/nucleo32/escudo/prueba/hazcarga.py $E/c 16 > /dev/null
    $E/anf -q -f $E/c/CARGA.NDB -f $E/c/CARGA.LDB -f $E/c/CARGA.HDB -o $E/c/CARGA.ESC -F
    LIN=$($E/anf -f $E/c/CARGA.ESC -t $E/c/GRANDE.BIN 2>&1 | sed -n 's/.* \([0-9.]* MB\/s\).*/\1/p')
    $AQUI/nucleo32/disco.sh $E/r.img 32 escudo > /dev/null 2>&1 && mcopy -i $E/r.img $E/c/CARGA.ESC ::ESCUDO/FIRMAS/ && mcopy -i $E/r.img $E/c/GRANDE.BIN ::
    DISCO=$E/r.img ARRANQUE=ordenes MODULOS= MEMORIA=160 $AQUI/nucleo32/probar.sh "" ":pausa 3" "escudo C:\\GRANDE.BIN" ":pausa 60" > $E/r.sal 2>&1
    MS=$(tr -d '\r' < $E/r.sal | sed -n 's/^escudo: 16777216 octetos en \([0-9]*\) ms$/\1/p')
    if [ -n "$MS" ]; then ok "lo que tarda, 16 MB al azar con 20.000 patrones, 5.000 lógicas y 100.000 sumas: en Linux, $LIN; en QEMU (sin KVM), $(python3 -c "print('%.1f MB/s' % (16000/$MS))") (leyendo del disco IDE)"
    else ko "no se ha podido medir en QEMU"; fi
    # (RÁPIDO) lo que pasó en la 0.8.5 (VirtualBox: «145 archivos en 504 s»;
    # el HP Mini, congelado instalando la base de firmas): fat.cas leía y
    # escribía de sector en sector (una orden al disco por cada 512 octetos)
    # y la memoria USB de 4 K en 4 K. Se mide en un «pc» con el disco IDE, una
    # memoria USB (EHCI, como el HP Mini) y un CD con 300 archivos (ide-cd,
    # como VirtualBox): copiar 16 MB en la memoria, revisarlos, y «Revisar
    # todo el equipo» desde la ventana
    rm -rf $E/cdr; mkdir -p $E/cdr/UNO $E/cdr/DOS
    for k in $(seq 150); do printf 'archivo %d del CD de prueba\r\n' $k > $E/cdr/UNO/A$k.TXT; head -c $((k * 97)) $E/c/GRANDE.BIN > $E/cdr/DOS/B$k.BIN; done
    printf '[autorun]\r\nopen=NADA.EXE\r\n' > $E/cdr/AUTORUN.INF
    rm -f $E/u.img; truncate -s 64M $E/u.img; mkfs.fat -F 32 -n MEMORIA $E/u.img > /dev/null && mcopy -i $E/u.img $E/c/GRANDE.BIN ::
    if xorriso -as mkisofs -quiet -J -V PRUEBA_CD -o $E/cd.iso $E/cdr > $E/iso.txt 2>&1; then
      rm -f $E/rel.txt
      RELOJ_A=$E/rel.txt CAPTURAS=$G DISCO=$E/r.img MAQUINA=pc ARRANQUE=ordenes MODULOS= MEMORIA=160 \
        QEMU_MAS="-device usb-ehci,id=u -drive id=mu,file=$E/u.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu -drive id=cd,file=$E/cd.iso,media=cdrom,if=none,readonly=on -device ide-cd,drive=cd,bus=ide.1,unit=0" \
        $AQUI/nucleo32/probar.sh "" ":pausa 3" ":reloj copia" "copy X:\\GRANDE.BIN X:\\COPIA.BIN" ":hasta 400 2 C:.>" "escudo X:\\COPIA.BIN" ":hasta 200 3 C:.>" \
        "escudo D:" ":hasta 200 4 C:.>" > $E/rap.sal 2>&1
      tr -d '\r' < $E/rap.sal > $E/rap.txt
      T1=$(sed -n 's/^copia \([0-9.]*\)$/\1/p' $E/rel.txt); T2=$(sed -n 's/^C:\.> 2 \([0-9.]*\)$/\1/p' $E/rel.txt)
      MSU=$(sed -n 's/^escudo: 16777216 octetos en \([0-9]*\) ms$/\1/p' $E/rap.txt | head -n 1)
      MSD=$(sed -n 's/^escudo: \([0-9]*\) octetos en \([0-9]*\) ms$/\2/p' $E/rap.txt | tail -n 1)
      NCD=$(sed -n 's/^escudo: revisado D: \([0-9]*\) archivos.*/\1/p' $E/rap.txt)
      if [ -n "$T1" ] && [ -n "$T2" ] && [ -n "$MSU" ] && python3 -c "import sys; sys.exit(not (16 / ($T2 - $T1) >= 0.4 and 16000 / $MSU >= 1.5))"; then
        ok "(RÁPIDO) la memoria USB (EHCI): copiar 16 MB, $(python3 -c "print('%.1f s' % ($T2 - $T1))") (antes, unos 260); revisarlos, $(python3 -c "print('%.1f MB/s' % (16000 / $MSU))") (antes, 0,2)"
      else ko "(RÁPIDO) la memoria USB va lenta: copiar 16 MB ${T1:+$(python3 -c "print('%.1f s' % (${T2:-0} - $T1))")}, revisar ${MSU:-?} ms"; fi
      if [ "$NCD" = 301 ] && [ -n "$MSD" ] && [ "$MSD" -lt 15000 ]; then ok "(RÁPIDO) el CD (ide-cd, como VirtualBox): sus 301 archivos, en $(python3 -c "print('%.1f s' % ($MSD / 1000))")"
      else ko "(RÁPIDO) el CD: ${NCD:-?} archivos en ${MSD:-?} ms"; fi
      # y en el escritorio, la ventana: «Revisar todo el equipo» (Intro, el botón de fábrica)
      CAPTURAS=$G DISCO=$E/r.img MAQUINA=pc ARRANQUE=escritorio MODULOS= MEMORIA=160 \
        QEMU_MAS="-drive id=cd,file=$E/cd.iso,media=cdrom,if=none,readonly=on -device ide-cd,drive=cd,bus=ide.1,unit=0" \
        $AQUI/nucleo32/probar.sh "" ":pausa 6" ":doble 62 185" ":pausa 3" "escudo /ventana" ":pausa 4" ":tecla ret" \
        ":hasta 120 1 revisión de \* acabada" ":pausa 2" ":captura rp1" ":clic 400 300" ":pausa 1" > $E/rv.sal 2>&1
      tr -d '\r' < $E/rv.sal > $E/rv.txt
      MST=$(sed -n 's/^escudo: \([0-9]*\) octetos en \([0-9]*\) ms$/\2/p' $E/rv.txt | tail -n 1)
      NT=$(sed -n 's/^escudo: ventana: revisión de \* acabada: \([0-9]*\) archivos.*/\1/p' $E/rv.txt)
      if [ -n "$MST" ] && [ -n "$NT" ] && [ "$NT" -gt 300 ] && [ "$MST" -lt 30000 ]; then
        ok "(RÁPIDO) «Revisar todo el equipo» en la ventana (el disco y el CD): $NT archivos en $(python3 -c "print('%.1f s' % ($MST / 1000))")"
      else ko "(RÁPIDO) «Revisar todo el equipo»: ${NT:-?} archivos en ${MST:-?} ms"; fi
      # el AUTORUN.INF del CD: un aviso que no se puede mover: «Dejarlo» por omisión; y el texto, en dos líneas
      miracap "(RÁPIDO) el AUTORUN.INF de un CD: «A cuarentena» y «Borrarlo» apagados, «Dejarlo» por omisión; la explicación, entera" rp1 \
        negrita 80 450 720 510 "Dejarlo" texto 80 360 720 480 "al meter el disco o la memoria."
    else ko "(RÁPIDO) no se ha podido hacer el CD de prueba: $(tail -n 1 $E/iso.txt)"; fi
  fi
  # 5. (CORTAFUEGOS) el cortafuegos de RED.BIN (rcorta.cas), con el servidor
  # SSH de verdad (SSHD.BIN) y una conexión desde aquí por QEMU (hostfwd: la
  # conexión llega desde 10.0.2.2, la red local). Lo que se mira: si llega su
  # saludo («SSH-2.0»), del lado de aquí; y lo que apunta RED.BIN
  if $AQUI/nucleo32/disco.sh $E/cf.img 32 red tls ssh escudo > $E/cfdisco.txt 2>&1; then
    saludo() {   # intenta cada 2 s durante $1 s: «sí» si alguna vez llega el saludo de SSH
      local k r=no
      for k in $(seq $(($1 / 2))); do
        sleep 2
        if timeout 3 bash -c "exec 3<>/dev/tcp/127.0.0.1/$2; head -c 7 <&3" 2> /dev/null | grep -q 'SSH-2.0'; then r=sí; break; fi
      done
      echo $r
    }
    # a) sin CORTAFUE.INI: lo de fábrica deja entrar a SSH desde la red local
    PH=$((40000 + RANDOM % 20000))
    ( saludo 240 $PH > $E/cfa.host ) &
    FUERA=$!
    cp $E/cf.img $E/cfa.img
    DISCO=$E/cfa.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PH-:22 -device e1000,netdev=n0" \
      $AQUI/nucleo32/probar.sh "" ":pausa 3" "sshd /encender" ":pausa 70" "escudo /cortafuegos" ":pausa 3" > $E/cfa.sal 2>&1
    wait $FUERA
    tr -d '\r' < $E/cfa.sal > $E/cfa.txt
    S=$E/cfa.txt
    mira "(CORTAFUEGOS) sin CORTAFUE.INI, lo de fábrica: SSH, desde la red local y Tailscale" \
      "red: cortafuegos: sin CORTAFUE.INI: lo de fábrica" "^  regla: TCP 22 (SSH): desde la red local y Tailscale$" "^  escucha: TCP 22 (la tarea [0-9]*)$"
    [ "$(cat $E/cfa.host)" = sí ] && ok "y la conexión desde la red local entra (llega el saludo de SSHD)" || ko "con lo de fábrica, la conexión a SSH no entra"
    # b) con «regla=tcp 22 nadie»: no entra, sin contestar, y se apunta
    PH=$((40000 + RANDOM % 20000))
    ( saludo 240 $PH > $E/cfb.host ) &
    FUERA=$!
    cp $E/cf.img $E/cfb.img
    printf 'activo=si\r\nping=red\r\navisar=si\r\nregla=tcp 22 nadie\r\n' > $E/CORTAFUE.INI
    mcopy -o -i $E/cfb.img $E/CORTAFUE.INI ::ESCUDO/CORTAFUE.INI
    DISCO=$E/cfb.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PH-:22 -device e1000,netdev=n0" \
      $AQUI/nucleo32/probar.sh "" ":pausa 3" "sshd /encender" ":pausa 70" "escudo /cortafuegos" ":pausa 3" > $E/cfb.sal 2>&1
    wait $FUERA
    tr -d '\r' < $E/cfb.sal > $E/cfb.txt
    S=$E/cfb.txt
    mira "(CORTAFUEGOS) con «regla=tcp 22 nadie»: la conexión se tira sin contestar y se apunta, y ESCUDO /CORTAFUEGOS lo dice" \
      "red: cortafuegos: activo, con 1 reglas$" "red: cortafuegos: bloqueada una conexión TCP desde 10.0.2.2 al puerto 22 (1 en total)$" \
      "^  regla: TCP 22 (SSH): no entra$" "^  bloqueado: TCP desde 10.0.2.2$"
    [ "$(cat $E/cfb.host)" = no ] && ok "y desde aquí no llega nada (ni el saludo ni un «no hay nadie»: no se ve el equipo)" || ko "con la regla «nadie», la conexión ha entrado"
    # c) en el escritorio, sin regla para el 22 y «avisar=si»: SSHD se pone a
    # escuchar (desde la Consola) y Escudo pregunta; «Desde la red» (Intro) deja
    # la regla en CORTAFUE.INI y RED.BIN la lee; y la pestaña Cortafuegos
    cp $E/cf.img $E/cfc.img
    printf 'activo=si\r\nping=red\r\navisar=si\r\n' > $E/CORTAFUE.INI
    mcopy -o -i $E/cfc.img $E/CORTAFUE.INI ::ESCUDO/CORTAFUE.INI
    CAPTURAS=$G ESCRIBE=1 DISCO=$E/cfc.img ARRANQUE="escritorio escritura" MODULOS= QEMU_MAS="-nic user,model=e1000" \
      $AQUI/nucleo32/probar.sh "" ":pausa 5" ":doble 62 185" ":pausa 3" "sshd /encender" ":pausa 12" ":captura ef1" ":tecla ret" ":pausa 6" \
      ":clic 720 585" ":pausa 3" ":clic 420 146" ":pausa 3" ":captura ef2" > $E/cfc.sal 2>&1
    tr -d '\r' < $E/cfc.sal > $E/cfc.txt
    S=$E/cfc.txt
    mira "(CORTAFUEGOS) un programa se pone a escuchar sin regla: RED.BIN lo deja pendiente y Escudo pregunta (ESCUDO /PERMITIR)" \
      "red: cortafuegos: la tarea [0-9]* quiere recibir conexiones en el puerto TCP 22, y no hay regla" "escudo: cortafuegos: pregunta por el puerto 22$"
    miracap "«¿Dejar que un programa reciba conexiones?», con «Desde la red», «Desde todos» y «No»" ef1 negrita 120 120 720 240 "¿Dejar que un programa reciba conexiones?" \
      negrita 120 320 720 480 "Desde la red" texto 120 320 720 480 "Desde todos" texto 120 160 720 400 "Wi-Fi"
    mira "«Desde la red»: la regla, a CORTAFUE.INI, y RED.BIN la lee" "escudo: cortafuegos: puerto 22: desde la red$" "red: cortafuegos: activo, con 1 reglas$"
    if saca $E/cfc.img ESCUDO/CORTAFUE.INI $E/cf.ini && grep -q '^regla=tcp 22 red' $E/cf.ini; then ok "CORTAFUE.INI lleva «regla=tcp 22 red»"
    else ko "CORTAFUE.INI no lleva la regla"; fi
    miracap "la pestaña «Cortafuegos»: encendido, lo que escucha con su regla y los botones" ef2 negrita 400 130 560 160 "Cortafuegos" \
      texto 90 170 720 220 "[OK] Cortafuegos encendido" texto 90 250 720 300 "TCP 22 (SSH)" texto 90 380 720 420 "Que no entre"
    if ! grep -q "excepción\|\*\*\*" $E/cfa.txt $E/cfb.txt $E/cfc.txt; then ok "(CORTAFUEGOS) ni una excepción del núcleo"
    else ko "el núcleo se ha quejado:"; grep -h "excepción\|\*\*\*" $E/cfa.txt $E/cfb.txt $E/cfc.txt | head -3 | sed 's/^/        /'; fi
  else ko "no se puede hacer el disco con la red y SSH: $(tail -3 $E/cfdisco.txt)"; fi
  # 6. con las bases de verdad de ClamAV (si está el espejo de cvdupdate del
  # VPS): los paquetes (tools/escudo_firmas.py), 10 muestras hechas de firmas
  # de verdad (encontradas) y los programas de Windows de Open Watcom, el
  # Python y el gcc de aquí (ningún aviso)
  ESPEJO=${ESCUDO_ESPEJO:-/var/lib/escudo-espejo}
  if [ -s $ESPEJO/main.cvd ] && [ -s $ESPEJO/daily.cvd ] && [ -d /opt/watcom/binnt64 ]; then
    if python3 -I $AQUI/../tools/escudo_firmas.py --cvd $ESPEJO --sal $E/real > $E/real.txt 2>&1; then
      for x in BASE:ESCBASE MES:ESCMES DIA:ESCDIA; do
        python3 -I -c "
import re, sys
d = open(sys.argv[1], 'rb').read(); i = d.index(b'fin\r\n') + 5
n = int(re.search(rb'%s.ESC (\d+)' % sys.argv[2].encode(), d).group(1)); open(sys.argv[3], 'wb').write(d[i:i + n])" \
          $E/real/${x##*:}.HPQ ${x%%:*} $E/real/${x%%:*}.ESC
      done
      python3 -I - $ESPEJO $E/sint > /dev/null 2>&1 <<'PY'
import io, os, random, re, sys, tarfile
def saca(r):
    d = open(r, 'rb').read()[512:]
    t = tarfile.open(fileobj=io.BytesIO(d), mode='r:gz')
    return {m.name: t.extractfile(m).read().decode('latin-1') for m in t.getmembers() if m.isfile()}
m, d = saca(sys.argv[1] + '/main.cvd'), saca(sys.argv[1] + '/daily.cvd')
os.makedirs(sys.argv[2], exist_ok=True)
random.seed(21)
ldb = [l for l in d['daily.ldb'].split('\n') if re.match(r'^Win\.[^;]*;Engine:\d+-255,Target:1;0&1&2&3&4;([0-9a-f]+;){4}[0-9a-f]+$', l)]
ndb = [l for l in m['main.ndb'].split('\n') if re.match(r'^Win\.[^:]*:0:\*:[0-9a-f]+$', l) and len(l) < 200]
pe = open('/opt/watcom/binnt/wcl386.exe', 'rb').read()
for k, l in enumerate(random.sample(ldb, 5)):
    p = l.split(';')
    open('%s/L%d.EXE' % (sys.argv[2], k), 'wb').write(pe + b''.join(b'\0' * 37 + bytes.fromhex(x) for x in p[3:]))
for k, l in enumerate(random.sample(ndb, 5)):
    p = l.split(':')
    open('%s/N%d.BIN' % (sys.argv[2], k), 'wb').write(os.urandom(3000) + bytes.fromhex(p[3]) + os.urandom(100))
PY
      $E/anf -f $E/real/BASE.ESC -f $E/real/MES.ESC -f $E/real/DIA.ESC $E/sint > $E/sint.txt 2>&1
      $E/anf -f $E/real/BASE.ESC -f $E/real/MES.ESC -f $E/real/DIA.ESC -t /opt/watcom/binnt64 /opt/watcom/binp /usr/bin/python3 /usr/bin/gcc > $E/limpios.txt 2>&1
      S=$E/real.txt
      mira "con las bases de verdad: escbase, escmes y escdia, con la firma de ClamAV de cada .cvd comprobada" "firma digital BUENA" \
        "^escudo_firmas: escbase " "^escudo_firmas: escmes " "^escudo_firmas: escdia "
      [ "$(grep -c ENCONTRADO $E/sint.txt)" = 10 ] && ok "las 10 muestras hechas de firmas de verdad (5 lógicas de daily, 5 del cuerpo de main), encontradas" \
        || { ko "de las 10 muestras, encontradas $(grep -c ENCONTRADO $E/sint.txt)"; grep -v ENCONTRADO $E/sint.txt | tail -3 | sed 's/^/        /'; }
      if ! grep -q ENCONTRADO $E/limpios.txt; then ok "y ningún aviso en los programas limpios ($(grep -c ': limpio' $E/limpios.txt) archivos: los de Windows de Open Watcom, python y gcc; $(sed -n 's/.* \([0-9.]* MB\/s\).*/\1/p' $E/limpios.txt) en Linux)"
      else ko "avisos falsos:"; grep ENCONTRADO $E/limpios.txt | head -3 | sed 's/^/        /'; fi
    else ko "tools/escudo_firmas.py: $(tail -2 $E/real.txt)"; fi
  else echo "  (sin el espejo de las bases de ClamAV: sin la prueba con las de verdad)"; fi
else ko "no se puede hacer el disco: $(tail -3 $E/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin Escudo)"
fi   # Escudo
# ------------------------------------------------- 27. (WIFI) el Wi-Fi
# WIFI.BIN (nucleo32/wifi/servicio.c con wifi/ de Hispa-486), su ventana
# (REDWIFI.BIN, uredwifi.cas) y la tarjeta Wi-Fi de RED.BIN (rwifi.cas). Sin
# tarjeta Wi-Fi en QEMU: la radio de mentira de wifi/nucleo/sim.c (una red
# «HispaPrueba», WPA2 con «clave de prueba», y detrás DHCP, ARP y eco en
# 192.168.77.1) y el modelo de la BCM43224 de tests/wifi/tarjeta_sim.c.
if parte08 wifi; then
echo "== (WIFI) Hispa-Pentium: el Wi-Fi (WIFI.BIN, REDWIFI.BIN y la tarjeta Wi-Fi de RED.BIN) =="
Wf=$P/wifi
mkdir -p $Wf $G
# 0. en el anfitrión, lo de Hispa-486: las normas (SHA-1, HMAC, PBKDF2, la PMK,
# PRF, AES, el envoltorio de claves y CCMP con el vector de IEEE 802.11) y la
# gestión contra el punto de acceso de mentira; y brcmsmac contra el modelo
if make -s -C $AQUI/.. build/prueba_wifi build/chip_sim > $Wf/make.txt 2>&1; then
  if $AQUI/../build/prueba_wifi > $Wf/anf.txt 2>&1 && grep -q ", 0 fallos" $Wf/anf.txt; then ok "en el anfitrión, las normas y la gestión 802.11 y WPA2: $(tail -1 $Wf/anf.txt)"
  else ko "build/prueba_wifi falla:"; tail -5 $Wf/anf.txt | sed 's/^/        /'; fi
  if $AQUI/../build/chip_sim > $Wf/chip.txt 2>&1 && grep -q ", 0 fallos" $Wf/chip.txt; then ok "en el anfitrión, brcmsmac contra el modelo de la tarjeta: $(tail -1 $Wf/chip.txt | iconv -f CP437 -t UTF-8)"
  else ko "build/chip_sim falla:"; tail -5 $Wf/chip.txt | iconv -f CP437 -t UTF-8 | sed 's/^/        /'; fi
  # (BCM4313) el PHY LCN (phy_lcn.c de Linux) contra el modelo de la BCM4313:
  # la placa sin Bluetooth (iPA) y la de amplificador de fuera con Bluetooth
  for v in 4313 4313bt; do
    if $AQUI/../build/chip_sim $v > $Wf/chip$v.txt 2>&1 && grep -q ", 0 fallos" $Wf/chip$v.txt; then ok "(BCM4313) en el anfitrión, brcmsmac con el PHY LCN: $(tail -1 $Wf/chip$v.txt | iconv -f CP437 -t UTF-8)"
    else ko "(BCM4313) build/chip_sim $v falla:"; grep -a "FALLA" $Wf/chip$v.txt | tail -5 | iconv -f CP437 -t UTF-8 | sed 's/^/        /'; tail -1 $Wf/chip$v.txt | iconv -f CP437 -t UTF-8 | sed 's/^/        /'; fi
  done
else ko "no se hacen build/prueba_wifi y build/chip_sim: $(tail -2 $Wf/make.txt)"; fi
if $AQUI/nucleo32/disco.sh $Wf/d.img 16 red wifisim > $Wf/disco.txt 2>&1 && bash $AQUI/nucleo32/wifi/hacer.sh $Wf/WIFISIM.BIN chip > $Wf/chip_hacer.txt 2>&1 \
   && bash $AQUI/nucleo32/wifi/hacer.sh $Wf/WIFISIM4313.BIN chip4313 > $Wf/chip4313_hacer.txt 2>&1; then
# 1. sin cable, la radio de mentira: buscar, conectar (WPA2), DHCP y PING por RED.BIN
cp $Wf/d.img $Wf/e.img
MEMORIA=64 ESCRIBE=1 DISCO=$Wf/e.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "redwifi /estado" ":pausa 6" "redwifi /redes" ":pausa 2" \
  "redwifi /conectar HispaPrueba" "otra_clave" ":pausa 2" "redwifi /conectar 2 /auto" "clave de prueba" ":pausa 2" "ipconfig" ":pausa 2" "ping -n 3 192.168.77.1" ":pausa 8" \
  "redwifi /estado" "redwifi /registro" > $Wf/sim.sal 2>&1
tr -d '\r' < $Wf/sim.sal > $Wf/sim.txt
S=$Wf/sim.txt
mira "RED.BIN pone en marcha WIFI.BIN (WifiSimulada=SI en RED.INI), que se apunta como tarjeta Wi-Fi: la red va por él" \
  "^red: Wi-Fi: WIFI.BIN en marcha, con la radio simulada$" "wifi: Hispa-Pentium: el servicio Wi-Fi (WIFI.BIN" "wifi: Memoria para la tarjeta: 2048 KB en " \
  "^red: Wi-Fi simulada (pruebas)$" "^red: la red va por el Wi-Fi$"
mira "REDWIFI /REDES: busca y dice las redes, con la señal en rayitas y la seguridad" "^N.º  Red  " \
  "^  [123]  HispaPrueba  *6  ████ -47 dBm  WPA2$" "^  [123]  Vecina abierta  *11  ████ -47 dBm  abierta$" "^  [123]  Vecina vieja  *3  ████ -47 dBm  WPA (no admitida)$"
mira "la contraseña se pregunta sin enseñarla (asteriscos; no va en la orden, así no queda en ninguna lista)" "^Contraseña de la red: \*\*\*\*\*\*\*\*\*\*$" \
  "^Contraseña de la red: \*\*\*\*\*\*\*\*\*\*\*\*\*\*\*$"
if grep -q "clave de prueba\|otra_clave" $S; then ko "la contraseña sale escrita en la pantalla"; else ok "y la contraseña no sale escrita en ningún sitio"; fi
mira "la contraseña equivocada: no conecta, lo dice y no la recuerda" "^No se conecta a «HispaPrueba»: la contraseña no es la de esta red\.$" "^wifi: No se recuerda «HispaPrueba»: la contraseña no es la de esta red$"
mira "con la buena (por su número en la lista): WPA2 (los 4 pasos), y la recuerda en WIFI.INI; RED.BIN ve el enlace y pide la IP (DHCP)" \
  "^wifi: WPA: paso 3 de 4 correcto; se manda el 4\. Claves puestas" "^wifi: 802\.11: CONECTADO a «HispaPrueba»$" "^Conectado a «HispaPrueba»\.$" \
  "^wifi: La red «HispaPrueba» queda recordada en WIFI.INI (se conectará sola)$" "^red: Wi-Fi: enlace (asociada, -47 dBm)$" \
  "red: DHCP: IP 192\.168\.77\.10, máscara 255\.255\.255\.0, puerta 192\.168\.77\.1"
mira "IPCONFIG y PING, por el Wi-Fi (la pila TCP/IP de siempre)" "^  Tarjeta . . . . . . . : Wi-Fi simulada (pruebas)" "^  Enlace  . . . . . . . : sí, a 54 Mbit/s$" \
  "^  Dirección IP  . . . . : 192\.168\.77\.10$" "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$"
mira "REDWIFI /ESTADO y /REGISTRO: conectada, con la señal, la IP y lo que se prefiere" "^Conectado a «HispaPrueba»$" "^  Señal -47 dBm, canal 6; IP 192\.168\.77\.10$" \
  "^  La red: por el cable si está enchufado; si no, por el Wi-Fi (Usar: Automático)$" "^WIFI.BIN 0\.1: Wi-Fi simulada (pruebas); radio en marcha; estado 5"
mira "las claves de WPA2 se renuevan sin cortar (el punto de acceso de mentira: la de pares cada 6 s, con un acuerdo de 4 pasos nuevo; la de grupo cada 9 s, con el KeyID 2): una línea por renovación" \
  "wifi: WPA2: clave por pares renovada (n\.º 1)$" "wifi: WPA2: clave de grupo renovada (n\.º [0-9]*, KeyID [12])$"
if grep -q "conexión cortada\|el acuerdo de claves con el punto de acceso no sale bien\|se ha perdido la red" $S; then ko "con las renovaciones de las claves se ha cortado el Wi-Fi:"; grep "802\.11\|WPA" $S | tail -8 | sed 's/^/        /'
else ok "y con las renovaciones no se corta ni se reconecta"; fi
saca $Wf/e.img WIFI.INI $Wf/wifi.ini; saca $Wf/e.img WIFI.LOG $Wf/wifi.log; saca $Wf/e.img WIFI.LLV $Wf/wifi.llv
if [ -f $Wf/wifi.log ] && LC_ALL=C grep -a -q "WPA2: clave por pares renovada (n\.. 1)" $Wf/wifi.log && LC_ALL=C grep -a -q "WPA2: clave de grupo renovada (n\.. [0-9]*, KeyID [12])" $Wf/wifi.log; then
  ok "WIFI.LOG lleva las renovaciones (sin claves ni nonces: solo el número)"
else ko "WIFI.LOG no lleva las renovaciones:"; grep -a "WPA" $Wf/wifi.log 2> /dev/null | tail -5 | sed 's/^/        /'; fi
if [ -f $Wf/wifi.ini ] && ! grep -q "clave de prueba" $Wf/wifi.ini && grep -q "^Red=1,4869737061507275656261,2:[0-9A-F]\{112\}.$" $Wf/wifi.ini && grep -q "ofuscada" $Wf/wifi.ini \
  && [ "$(stat -c %s $Wf/wifi.llv 2> /dev/null)" = 16 ] && grep -q "CONECTADO a" $Wf/wifi.log; then
  ok "WIFI.INI: la red (en hexadecimal) y su clave preparada, cifrada y firmada (AES-CTR y HMAC-SHA1; y dice que es una ofuscación): la contraseña no está escrita; WIFI.LLV (la llave, 16 octetos) y WIFI.LOG en el disco"
else ko "WIFI.INI, WIFI.LLV o WIFI.LOG no son como deben:"; cat $Wf/wifi.ini 2> /dev/null | sed 's/^/        /'; fi
# 2. otra vez (el mismo disco): se conecta sola a la red recordada
MEMORIA=64 DISCO=$Wf/e.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "ipconfig" ":pausa 8" "redwifi /estado" > $Wf/otra.sal 2>&1
tr -d '\r' < $Wf/otra.sal > $Wf/otra.txt
S=$Wf/otra.txt
mira "al arrancar otra vez, se conecta sola a la red recordada (WIFI.INI) y RED.BIN pide la IP" "wifi: WIFI.INI: 1 redes recordadas$" \
  "wifi: Se conecta sola a la red recordada «HispaPrueba» (-47 dBm)$" "red: DHCP: IP 192\.168\.77\.10" "^Conectado a «HispaPrueba»$"
# 2 bis. WIFI.INI cambiado a mano (una cifra de la clave): la firma no cuadra y no se usa
cp $Wf/e.img $Wf/t.img
python3 -I -c 'import sys; t=open(sys.argv[1],"rb").read(); i=t.index(b",2:")+20; c=b"0" if t[i:i+1]!=b"0" else b"1"; open(sys.argv[2],"wb").write(t[:i]+c+t[i+1:])' $Wf/wifi.ini $Wf/wifi2.ini
mcopy -o -i $Wf/t.img $Wf/wifi2.ini ::WIFI.INI
MEMORIA=64 DISCO=$Wf/t.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "ipconfig" ":pausa 6" > $Wf/t.sal 2>&1
tr -d '\r' < $Wf/t.sal > $Wf/t.txt
S=$Wf/t.txt
mira "WIFI.INI cambiado a mano: la clave no lleva la firma de este equipo y no se usa (ni se conecta sola con ella)" \
  "WIFI.INI: la clave de «HispaPrueba» no lleva la firma de este equipo" "WIFI.INI: 0 redes recordadas"
if grep -q "Se conecta sola" $S; then ko "con WIFI.INI cambiado se conecta sola"; else ok "y no se conecta sola"; fi
# 3. con la e1000 (el cable) y el Wi-Fi: Automático (el cable), Wi-Fi y Cable (RED.INI)
cp $Wf/e.img $Wf/c.img
MEMORIA=64 ESCRIBE=1 DISCO=$Wf/c.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 8" "redwifi /estado" \
  "redwifi /usar wifi" ":pausa 4" "ipconfig" "ping -n 2 192.168.77.1" ":pausa 3" "redwifi /usar auto" ":pausa 4" "ipconfig" "redwifi /usar cable" ":pausa 3" "redwifi /estado" ":hasta 30 1 WIFI.BIN ya no está" ":pausa 1" > $Wf/cable.sal 2>&1
tr -d '\r' < $Wf/cable.sal > $Wf/cable.txt
S=$Wf/cable.txt
mira "con cable y Wi-Fi, en Automático: la red va por el cable (el Wi-Fi conecta, pero no se usa)" "red: DHCP: IP 10\.0\.2\.15" \
  "^  Señal -47 dBm, canal 6; la red va por el cable (IP 10\.0\.2\.15)$"
mira "«Usar: Wi-Fi»: RED.BIN cambia a la tarjeta Wi-Fi, pide la IP otra vez y el PING va por ella" "^red: Wi-Fi: se usa el Wi-Fi (Preferir=WIFI)$" "^red: la red va por el Wi-Fi$" \
  "^  Tarjeta . . . . . . . : Wi-Fi simulada (pruebas)" "^  Dirección IP  . . . . : 192\.168\.77\.10$" "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
mira "«Usar: Automático»: otra vez el cable; «Usar: Cable»: WIFI.BIN se para" "^red: la red va por el cable$" "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000)" \
  "^red: Wi-Fi: se usa solo el cable (Preferir=CABLE)$" "^wifi: RED.BIN usa el cable: el Wi-Fi se para$" "^red: Wi-Fi: WIFI.BIN ya no está$" "^  La red: solo por el cable (Usar: Cable)$"
if saca $Wf/c.img RED.INI $Wf/red.ini && grep -q "^WifiSimulada=SI.$" $Wf/red.ini && grep -q "^Preferir=CABLE.$" $Wf/red.ini; then ok "RED.INI guarda lo elegido (Preferir=CABLE) y deja lo demás"
else ko "RED.INI no queda bien:"; sed 's/^/        /' $Wf/red.ini 2> /dev/null; fi
MEMORIA=64 DISCO=$Wf/c.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 3" > $Wf/cable2.sal 2>&1
if grep -q "red: DHCP: IP 10.0.2.15" $Wf/cable2.sal && ! grep -q "^red: Wi-Fi: WIFI.BIN en marcha" $Wf/cable2.sal; then ok "con «Preferir=CABLE», al arrancar el Wi-Fi ni se pone en marcha"
else ko "con «Preferir=CABLE» se pone en marcha el Wi-Fi"; fi
# 4. el controlador de verdad (brcmsmac y bcma) contra el modelo de la tarjeta, con el microcódigo del disco
cp $Wf/d.img $Wf/k.img
printf 'WifiSimulada=CHIP\r\n' > $Wf/RED.INI
mcopy -o -i $Wf/k.img $Wf/WIFISIM.BIN ::WIFI.BIN && mcopy -o -i $Wf/k.img $Wf/RED.INI ::
MEMORIA=64 DISCO=$Wf/k.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "redwifi /estado" ":pausa 8" "redwifi /conectar HispaPrueba" "clave de prueba" ":pausa 2" \
  "ping -n 5 192.168.77.1" ":pausa 5" > $Wf/k.sal 2>&1
tr -d '\r' < $Wf/k.sal > $Wf/k.txt
S=$Wf/k.txt
mira "WIFISIM.BIN: brcmsmac y bcma de Linux en el anillo 3, con los anillos de DMA en la memoria del núcleo (la 94) y los registros a la vista (la 95)" \
  "^wifi: Tarjeta Wi-Fi: 14E4:4353 (subsistema 103C:1510)" "^wifi: Chip BCM43224 (0xA8D8), revisión 1" "^wifi: SPROM: revisión 8, placa 103C:1510" \
  "BCM43XX\.FW: 96224 octetos$" "^wifi: PASO 3 de 3 hecho: radio en marcha" "^red: Wi-Fi Broadcom BCM43224$"
mira "y con él: WPA2, DHCP y cinco PING sin perder ninguno" "^wifi: brcmsmac: brcms_ops_bss_info_changed: associated$" "^wifi: 802\.11: CONECTADO a «HispaPrueba»$" \
  "red: DHCP: IP 192\.168\.77\.10" "^    Paquetes: enviados = 5, recibidos = 5, perdidos = 0"
mira "y la clave por pares se renueva por él sin cortar (el punto de acceso de mentira, cada 6 s)" "wifi: WPA2: clave por pares renovada (n\.º 1)$"
# 4 ter. (BCM4313) lo mismo con el modelo de la BCM4313: el PHY LCN y la radio
# 2064 (phy_lcn.c de Linux) se montan, se encienden con sus calibraciones,
# buscan (el modelo oye en el canal que programa el PHY) y conectan
cp $Wf/d.img $Wf/k4.img
mcopy -o -i $Wf/k4.img $Wf/WIFISIM4313.BIN ::WIFI.BIN && mcopy -o -i $Wf/k4.img $Wf/RED.INI ::
MEMORIA=64 DISCO=$Wf/k4.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "redwifi /estado" ":pausa 8" "redwifi /conectar HispaPrueba" "clave de prueba" ":pausa 2" \
  "ping -n 5 192.168.77.1" ":pausa 5" > $Wf/k4.sal 2>&1
tr -d '\r' < $Wf/k4.sal > $Wf/k4.txt
S=$Wf/k4.txt
mira "(BCM4313) WIFISIM.BIN con el modelo de la BCM4313: se encuentra (14E4:4727), chip 0x4313, PHY LCN rev. 1 con la radio 2064, sin Bluetooth (tabla de antena «4313»)" \
  "^wifi: Tarjeta Wi-Fi: 14E4:4727 (subsistema 103C:1483)" "^wifi: Chip BCM4313 (0x4313), revisión 1" "^wifi: Encendiendo la radio (microcódigo, PHY LCN, calibración)\.\.\.$" \
  "^wifi: PHY LCN rev\. 1, radio 2064 rev\. 0; amplificador de dentro (iPA); sin Bluetooth al lado; tabla de la antena: 4313$" \
  "^wifi: PASO 3 de 3 hecho: radio en marcha" "^red: Wi-Fi Broadcom BCM4313$"
mira "(BCM4313) y con ella: WPA2, DHCP y cinco PING sin perder ninguno" "^wifi: 802\.11: CONECTADO a «HispaPrueba»$" \
  "red: DHCP: IP 192\.168\.77\.10" "^    Paquetes: enviados = 5, recibidos = 5, perdidos = 0"
# 4 quater. (0.8.11, SIN-ADMIN) la BCM4313 que no se pone en marcha (aquí, sin
# su microcódigo): WIFI.LOG lleva el DIAGNÓSTICO sin «Registro=SI» y la
# ventana dice qué tarjeta y dónde está el diagnóstico
cp $Wf/d.img $Wf/k5.img
mcopy -o -i $Wf/k5.img $Wf/WIFISIM4313.BIN ::WIFI.BIN && mcopy -o -i $Wf/k5.img $Wf/RED.INI :: && mdel -i $Wf/k5.img ::BCM43XX.FW
MEMORIA=64 ESCRIBE=1 DISCO=$Wf/k5.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 3" "ipconfig" ":pausa 12" "redwifi /estado" ":pausa 3" > $Wf/k5.sal 2>&1
tr -d '\r' < $Wf/k5.sal > $Wf/k5.txt
S=$Wf/k5.txt
mira "(SIN-ADMIN) la BCM4313 sin poder ponerse en marcha: el DIAGNÓSTICO (la tarjeta, dónde está en el PCI, el controlador) sin Registro=SI" \
  "^wifi: ==== DIAGN.STICO: la tarjeta está, pero no se ha podido poner en marcha ====$" "^wifi: Tarjeta: Broadcom 14E4:4727 en el PCI [0-9a-f:.]*; controlador: Broadcom" \
  "^wifi: Este archivo está en .WIFI.LOG: cópielo a una memoria USB y páseselo a quien le ayude\.$" "^wifi: ==== FIN DEL DIAGN.STICO ====$"
mira "(SIN-ADMIN) y REDWIFI /ESTADO (lo mismo que la ventana): qué tarjeta no se ha podido usar y dónde está el diagnóstico" \
  "^No se ha podido usar la tarjeta Broadcom 14E4:4727\.$" "^  El diagnóstico está en [A-Z]:.WIFI.LOG: páseselo a quien le ayude\.$"
if saca $Wf/k5.img WIFI.LOG $Wf/k5.log && grep -aq "FIN DEL DIAGN" $Wf/k5.log && ! grep -aq "clave de prueba" $Wf/k5.log; then ok "(SIN-ADMIN) y WIFI.LOG, en el disco, con el diagnóstico (y sin contraseñas)"
else ko "(SIN-ADMIN) WIFI.LOG no lleva el diagnóstico"; fi
# 4 quinquies. (HPRADIO) el HP Mini 5103 n.º 2: la BCM4313 con la radio APAGADA
# por la patilla W_DISABLE# de la tarjeta (la tecla del Wi-Fi; WIFISIM.TXT
# «RFKILL=14»: las 14 primeras miradas al bit RFD de phydebug), con ACPI.BIN y
# la WMI de HP de mentira (tests/acpi/hpwmi-qemu.aml: el Wi-Fi apagado «por
# programa»). ACPI.BIN pide a la BIOS que lo encienda (consulta 1Bh, como
# hp-wmi de Linux); WIFI.BIN lo copia a WIFI.LOG («HP:»), dice en la ventana
# que se pulse la tecla, mira la patilla cada segundo y, al soltarse, enciende
# la radio sin reiniciar ni volver a cargar nada, y conecta
if bash $AQUI/../acpi/pentium/hacer.sh $Wf/ACPI.BIN > $Wf/acpi_hacer.txt 2>&1; then
cp $Wf/d.img $Wf/k6.img
printf 'RFKILL=14\r\n' > $Wf/WIFISIM.TXT
mcopy -o -i $Wf/k6.img $Wf/WIFISIM4313.BIN ::WIFI.BIN && mcopy -o -i $Wf/k6.img $Wf/RED.INI :: && mcopy -o -i $Wf/k6.img $Wf/WIFISIM.TXT :: \
  && mcopy -o -i $Wf/k6.img $Wf/ACPI.BIN ::    # (con disco, el núcleo lanza el ACPI.BIN de C:)
MEMORIA=64 ESCRIBE=1 DISCO=$Wf/k6.img ARRANQUE=ordenes MODULOS=$Wf/ACPI.BIN QEMU_MAS="-acpitable file=$AQUI/../tests/acpi/hpwmi-qemu.aml" \
  $AQUI/nucleo32/probar.sh "" ":pausa 4" "redwifi /estado" ":pausa 3" "redwifi /estado" ":hasta 40 1 se enciende la radio" ":pausa 6" "redwifi /estado" \
  "redwifi /conectar HispaPrueba" "clave de prueba" ":pausa 2" "ping -n 3 192.168.77.1" ":pausa 4" > $Wf/k6.sal 2>&1
tr -d '\r' < $Wf/k6.sal > $Wf/k6.txt
S=$Wf/k6.txt
mira "(HPRADIO) ACPI.BIN encuentra la WMI de HP (PNP0C14, el GUID de la BIOS de HP, WMAA) y lee la tabla de las radios: el Wi-Fi (14E4:4727) apagado por programa" \
  "^ACPI: (HPRADIO) WMI de HP (GUID 5FB7F034-...-3D44E2C707E4) en WMAA" "^ACPI: (HPRADIO) al arrancar: la BIOS dice 2 radios" \
  "^ACPI: (HPRADIO)   radio 0: Wi-Fi, bus 1, 14E4:4727 (subsistema 103C:1483), numero 0, estado 0Ch: apagada, APAGADA POR PROGRAMA, la BIOS la deja, la tecla la deja$" \
  "^ACPI: (HPRADIO)   radio 1: Bluetooth, .*estado 0Fh: ENCENDIDA"
mira "(HPRADIO) y le pide a la BIOS que lo encienda (1Bh, escribir 01 00 00 01), como «rfkill unblock» con hp-wmi: la BIOS lo enciende" \
  "^ACPI: (HPRADIO) el Wi-Fi esta apagado por programa (le falta el 02h): se pide a la BIOS que lo encienda (consulta 1Bh, escribir 01 00 00 01)$" \
  "^ACPI: (HPRADIO)   radio 0: Wi-Fi, .*estado 0Fh: ENCENDIDA, el sistema la deja" "^ACPI: (HPRADIO) la BIOS lo ha encendido (por programa)$"
mira "(HPRADIO) WIFI.BIN: la radio, apagada por la patilla W_DISABLE#: no se rinde (el controlador se queda montado) y dice lo que intenta" \
  "DE PRUEBAS: la patilla W_DISABLE# de la tarjeta, activa" "^wifi: Radio (al encender): bit RFD de phydebug = 1" \
  "apagada por la patilla W_DISABLE# de la tarjeta (-10)$" "^wifi: Lo que se intenta: (1) en los HP, ACPI.BIN pide a la BIOS"
mira "(HPRADIO) y copia lo de ACPI.BIN (RADIOHP.TXT) a WIFI.LOG, con «HP:» delante" "^wifi: HP: WMI de HP (GUID 5FB7F034" "^wifi: HP: la BIOS lo ha encendido (por programa)$"
mira "(HPRADIO) la ventana (REDWIFI /ESTADO) dice que se pulse la tecla del Wi-Fi, no «No se ha podido usar la tarjeta»" \
  "pulse la tecla del Wi-Fi del equipo (la de la antena)\.$"
mira "(HPRADIO) al soltarse la patilla (la tecla), la radio se enciende sola, sin reiniciar: PHY LCN, calibraciones, y busca" \
  "wifi: La patilla W_DISABLE# dice ahora ENCENDIDA (a los [0-9]* s): se enciende la radio$" "^wifi: Radio (al soltarse): bit RFD de phydebug = 0 (encendida)" \
  "^wifi: PASO 3 de 3 hecho: radio en marcha" "^wifi: El Wi-Fi, en marcha despu"
mira "(HPRADIO) y con ella: WPA2, DHCP y tres PING sin perder ninguno" "^wifi: 802\.11: CONECTADO a «HispaPrueba»$" \
  "red: DHCP: IP 192\.168\.77\.10" "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
if grep -q "No se ha podido usar la tarjeta" $S; then ko "(HPRADIO) la ventana ha dicho «No se ha podido usar la tarjeta» con la radio solo apagada"
else ok "(HPRADIO) ni una vez «No se ha podido usar la tarjeta» (la radio solo estaba apagada)"; fi
if saca $Wf/k6.img WIFI.LOG $Wf/k6.log && grep -aq "HP: WMI de HP" $Wf/k6.log && grep -aq "dice ahora ENCENDIDA" $Wf/k6.log && grep -aq "patilla W_DISABLE# de la tarjeta" $Wf/k6.log; then
  ok "(HPRADIO) y WIFI.LOG, en el disco, con lo de la BIOS de HP, la patilla y cuándo se ha encendido"
else ko "(HPRADIO) WIFI.LOG no lleva lo de la radio apagada"; fi
else ko "(HPRADIO) acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Wf/acpi_hacer.txt)"; fi
# 4 sexies. (HPRADIO) lo mismo de ACPI.BIN contra el AML de VERDAD del HP Mini
# 5103 (sus tablas, tests/acpi/hp-mini-5103/; build/acpi_sim): \_SB.WMID.WMAA
# -> WHCM -> WGWS (la tabla de las radios en la memoria de la BIOS, WLDA:
# aquí, la que se pone con -m) y, para encender, WSWS (la llamada a la BIOS:
# SMI EA75h por el puerto B2h, que en el simulado no hace nada)
H=$AQUI/../tests/acpi/hp-mini-5103
WL=7D8F4E1B   # WLDA (HPD2 + 3)
pk() { local d=$1; shift; for v in "$@"; do printf -- '-m %X %s ' $((0x$WL + d)) $v; d=$((d + 1)); done; }
if make -s -C $AQUI/.. build/acpi_sim > $Wf/acpisim_make.txt 2>&1; then
  # shellcheck disable=SC2046
  ( cd $AQUI/.. && timeout 120 build/acpi_sim -n 1 -o 38 -osi 0 $(for f in $H/*.BIN; do printf -- '-t %s ' $f; done) -e $H/escenario.txt \
      $(pk 7 01) $(pk 16 00 01 E4 14 27 47 3C 10 83 14 00 0C) -hpradio ) 2>&1 | tr -d '\r' > $Wf/hpsim.txt
  S=$Wf/hpsim.txt
  mira "(HPRADIO) con el AML de verdad del HP Mini 5103: la WMI de HP es \\_SB.WMID, WMAA, y la tabla de las radios (WGWS/WLDA) se lee" \
    "^ACPI: (HPRADIO) WMI de HP (GUID 5FB7F034-...-3D44E2C707E4) en WMAA" "^ACPI: (HPRADIO) al arrancar: la BIOS dice 1 radios (128 octetos)$" \
    "^ACPI: (HPRADIO)   radio 0: Wi-Fi, bus 1, 14E4:4727 (subsistema 103C:1483), numero 0, estado 0Ch"
  # (la BIOS simulada no tiene SMM: la SMI EA75h no cambia EBX, que WFDA
  # devuelve como código: 2, el que le puso SSMI; en el equipo lo pone la BIOS)
  mira "(HPRADIO) y pedir que encienda el Wi-Fi pasa por WSWS (SSMI EA75h, WFDA) sin errores de ACPICA; la BIOS simulada (sin SMM) no hace nada" \
    "^ACPI: (HPRADIO) el Wi-Fi esta apagado por programa" "^ACPI: (HPRADIO) WMAA (consulta 1Bh, orden 2): la BIOS dice 2 " \
    "^ACPI: (HPRADIO) despues de pedirlo: la BIOS dice 1 radios" "^ACPI: (HPRADIO) la BIOS NO lo ha encendido$" "ACPICA: 0 avisos o errores"
  if grep -q "AE_\(AML\|NOT\|BAD\|ERROR\|TYPE\)\|ACPI Error\|ACPI Exception\|orden 1): la BIOS dice" $S; then ko "(HPRADIO) ACPICA se queja con el AML del HP Mini:"; grep "AE_\|ACPI Error\|ACPI Exception\|la BIOS dice [0-9]* (" $S | head -5 | sed 's/^/        /'
  else ok "(HPRADIO) ni un error de ACPICA ni de la BIOS con el AML del HP Mini"; fi
else ko "(HPRADIO) no se hace build/acpi_sim: $(tail -n 2 $Wf/acpisim_make.txt)"; fi
# 4 bis. (seguridad) una red ABIERTA falsa con el nombre de la WPA2 (WifiSimulada=GEMELA:
# en el canal 1, se ve antes): se conecta a la WPA2, la lista enseña las dos
# por separado, la abierta no se usa ni rebaja la recordada, y al arrancar
# otra vez, se conecta sola a la WPA2
cp $Wf/d.img $Wf/g.img
printf 'WifiSimulada=GEMELA\r\n' > $Wf/RED.INI
mcopy -o -i $Wf/g.img $Wf/RED.INI ::
MEMORIA=64 ESCRIBE=1 DISCO=$Wf/g.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "ipconfig" ":pausa 6" "redwifi /conectar HispaPrueba /auto" "clave de prueba" ":pausa 2" \
  "redwifi /redes" ":pausa 2" "redwifi /conectar HispaPrueba /abierta" ":pausa 2" "redwifi /estado" > $Wf/g.sal 2>&1
tr -d '\r' < $Wf/g.sal > $Wf/g.txt
S=$Wf/g.txt
mira "una «HispaPrueba» ABIERTA falsa al lado: se conecta a la de WPA2 (...:00:01: el BSSID, recortado), no a la abierta (:04)" \
  "802.11: conectando con «HispaPrueba» (\.\.\.:00:01, canal 6" "^Conectado a «HispaPrueba»\.$"
mira "la lista enseña las dos por separado: la WPA2, recordada; la abierta, sin candado ni «recordada»" \
  "^  [1-4]  HispaPrueba  *6  ████ -47 dBm  WPA2  (conectada)$" "^  [1-4]  HispaPrueba  *1  ████ -47 dBm  abierta$"
mira "pedir la abierta a propósito: no se usa (puede ser falsa) y lo dice; sigue conectada a la WPA2" \
  "^Hay una red ABIERTA que se llama «HispaPrueba», como una que se recordó con contraseña: puede ser falsa y no se usa" \
  "«HispaPrueba» está recordada con contraseña (WPA2) y esta es abierta: no se conecta" "^Conectado a «HispaPrueba»$"
if grep -q "conectando con «HispaPrueba» (\.\.\.:00:04" $S; then ko "se ha conectado a la abierta falsa"; else ok "ni un intento con la abierta falsa"; fi
saca $Wf/g.img WIFI.INI $Wf/g.ini
if grep -q "^Red=1,4869737061507275656261,2:[0-9A-F]\{112\}.$" $Wf/g.ini; then ok "WIFI.INI sigue con la clave de la WPA2 (no se ha rebajado a abierta)"
else ko "WIFI.INI ha cambiado la red recordada:"; sed 's/^/        /' $Wf/g.ini; fi
MEMORIA=64 DISCO=$Wf/g.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "ipconfig" ":pausa 8" "redwifi /estado" > $Wf/g2.sal 2>&1
tr -d '\r' < $Wf/g2.sal > $Wf/g2.txt
S=$Wf/g2.txt
mira "al arrancar otra vez, con la falsa al lado, se conecta sola a la WPA2" "Se conecta sola a la red recordada «HispaPrueba»" \
  "802.11: conectando con «HispaPrueba» (\.\.\.:00:01" "^Conectado a «HispaPrueba»$"
if grep -q "conectando con «HispaPrueba» (\.\.\.:00:04" $S; then ko "al arrancar, se ha ido a la abierta falsa"; else ok "y ni lo intenta con la falsa"; fi
# 5. el escritorio: el icono (con el aspa, sin conectar), la ventana del Wi-Fi, la contraseña con asteriscos, conectar; Configuración
# ((0.8.12) los 8 s, desde que el escritorio está en marcha, no desde que
# arranca QEMU: con la máquina cargada, a los 8 s aún no estaba y el clic iba a nadie)
cp $Wf/d.img $Wf/v.img
CAPTURAS=$G MEMORIA=64 DISCO=$Wf/v.img ARRANQUE="escritorio conred" MODULOS= $AQUI/nucleo32/probar.sh "" ":hasta 120 1 escritorio: paso: en marcha" ":pausa 8" ":captura wf1" ":clic 720 583" ":pausa 4" ":captura wf2" \
  ":clic 300 187" ":pausa 1" ":clic 227 347" ":escribe clave de prueba" ":pausa 1" ":captura wf3" ":clic 150 397" ":pausa 8" ":captura wf4" > $Wf/esc.sal 2>&1
S=$Wf/esc.sal
mira "el escritorio: RED.BIN y WIFI.BIN en marcha; un clic en el icono de la red abre la ventana del Wi-Fi (REDWIFI.BIN), y se conecta desde ella" \
  "^red: la red va por el Wi-Fi$" "^escritorio: lanzo .REDWIFI.BIN" "^wifi: 802.11: CONECTADO a «HispaPrueba»$" "red: DHCP: IP 192.168.77.10"
miracap "el icono del Wi-Fi sin conectar: las cuatro rayitas huecas y un aspa (la forma, no solo el color)" wf1 rgb 727 584 4F9BE6 rgb 712 586 4F9BE6 rgb 711 574 14336A
miracap "la ventana del Wi-Fi: cómo está, las redes (con la señal y la seguridad) y «Usar»" wf2 negrita 135 85 150 100 "Wi-Fi: sin conectar" \
  negrita 100 125 120 145 "Redes a la vista" texto 160 155 175 170 "Vecina vieja" texto 160 178 175 195 "HispaPrueba" texto 530 178 560 195 "WPA2" negrita 100 438 120 455 "Usar:"
miracap "la contraseña, con asteriscos (y la red elegida, en azul con su flecha)" wf3 texto 108 336 120 355 "************" notexto 100 330 350 360 "clave" rgb 400 190 1D4ED8
miracap "conectada: arriba «Conectado a», la señal, el canal y la IP; en la lista, «conectada»; y en la bandeja, las cuatro rayitas llenas" wf4 negrita 135 85 150 100 "Conectado a" \
  texto 210 103 230 118 "dBm, canal 6; IP 192.168.77.10" texto 580 178 600 195 "conectada" rgb 727 584 FFFFFF rgb 712 586 FFFFFF
# Configuración (el menú del tapiz) > Red y Wi-Fi (PANEL: el Panel de control,
# PANEL.BIN /RED): «Por dónde va la red: Automático, Cable, Wi-Fi»; Wi-Fi y luego Cable
cp $Wf/d.img $Wf/v2.img
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/upanel.cas -o $Wf/upanel.asm > /dev/null && nasm -f bin $Wf/upanel.asm -o $Wf/PANEL.BIN && mcopy -i $Wf/v2.img $Wf/PANEL.BIN ::
CAPTURAS=$G MEMORIA=64 ESCRIBE=1 DISCO=$Wf/v2.img ARRANQUE="escritorio conred" MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 8" ":clicd 400 300" ":pausa 1" ":clic 420 310" ":pausa 2" \
  ":clic 336 326" ":pausa 4" ":raton 400 450" ":captura wf5" ":clic 632 139" ":raton 400 450" ":pausa 2" ":captura wf6" ":clic 440 139" ":pausa 3" ":captura wf7" > $Wf/conf.sal 2>&1
S=$Wf/conf.sal
mira "Configuración, «Red»: Wi-Fi y luego Cable llegan a RED.BIN (un aviso del núcleo), que lo guarda; con Cable, WIFI.BIN se para" \
  "^red: Wi-Fi: se usa el Wi-Fi (Preferir=WIFI)$" "^red: Wi-Fi: se usa solo el cable (Preferir=CABLE)$" "^wifi: RED.BIN usa el cable: el Wi-Fi se para$"
miracap "el Panel de control > Red y Wi-Fi: «Por dónde va la red» con Automático (elegido), Cable y Wi-Fi" wf5 negrita15 230 90 300 120 "Por dónde va la red" \
  negrita15 240 120 300 150 "Automático (elegido)" negrita15 480 120 520 150 "Cable"
miracap "y elegido Wi-Fi, los botones cambian («Wi-Fi (elegido)»), lo de al lado no" wf6 distinto 236 123 440 32 $G/wf5.ppm igual 10 300 200 60 $G/wf5.ppm \
  negrita15 500 120 520 135 "Wi-Fi (elegido)"
miracap "y con Cable, el icono de la bandeja deja de ser el del Wi-Fi" wf7 distinto 700 570 40 25 $G/wf6.ppm
if saca $Wf/v2.img RED.INI $Wf/red2.ini && grep -q "^Preferir=CABLE.$" $Wf/red2.ini; then ok "y RED.INI queda con Preferir=CABLE"; else ko "RED.INI no guarda lo de Configuración"; fi
else ko "no se pueden hacer el disco o WIFISIM.BIN: $(tail -2 $Wf/disco.txt) $(tail -2 $Wf/chip_hacer.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Wi-Fi)"
fi   # el Wi-Fi
# ------------------------------------------ 54. (INTEL) el Wi-Fi de Intel
# El controlador de las Intel 7265/3165/3168 (wifi/nucleo/radio_iwl.c, como
# iwlwifi/MVM) y el lector de su firmware (iwl_fw.c). El firmware de Intel no
# es libre y no está en el repositorio: en el anfitrión, el lector se prueba
# con firmwares hechos a mano y el controlador entero contra el modelo de la
# tarjeta (tests/wifi/iwl_modelo.c: ALIVE, NVM, PHY DB, DQA, LAR y un
# escaneo con tres redes, hasta la lista de w80211.c). Con IWL_FW=archivo
# (un iwlwifi-7265D-29.ucode bajado aparte), además con sus secciones de
# verdad. En QEMU (sin la tarjeta): con la e1000, que también es de Intel
# (clase 0200h), WIFI.BIN no se pone en marcha y la red va como siempre.
if parte08 intel; then
echo "== (INTEL) el Wi-Fi de Intel: el lector del firmware y el controlador de la 7265 contra su modelo =="
Wi=$P/intel
mkdir -p $Wi
if make -s -C $AQUI/.. build/iwl_analiza build/iwl_modelo > $Wi/make.txt 2>&1; then
  if $AQUI/../build/iwl_analiza > $Wi/analiza.txt 2>&1 && grep -q ", 0 fallos$" $Wi/analiza.txt; then ok "(INTEL) $(tail -1 $Wi/analiza.txt)"
  else ko "(INTEL) el lector del firmware falla:"; tail -5 $Wi/analiza.txt | sed 's/^/        /'; fi
  if $AQUI/../build/iwl_modelo ${IWL_FW:-} > $Wi/modelo.txt 2>&1 && grep -q ", 0 fallos$" $Wi/modelo.txt; then ok "(INTEL) $(tail -1 $Wi/modelo.txt)"
  else ko "(INTEL) el controlador contra el modelo falla:"; grep "FALLO\|MODELO" $Wi/modelo.txt | head -8 | sed 's/^/        /'; fi
  S=$Wi/modelo.txt
  mira "(INTEL) lo que dice en WIFI.LOG: la 7265D, las dos imágenes, la MAC, la calibración, el país y las redes" \
    "iwl: Intel Wireless-AC 7265, CSR_HW_REV 00000218 (tipo 021: 7265D, paso 2)$" "iwl: imagen INIT cargada: " "iwl: imagen RT cargada: " \
    "iwl: MAC 34:e6:ad:12:34:56 (del NVM)$" "iwl: calibrada: 5 trozos del PHY DB" "iwl: país «ES» (estado 0, 51 canales)$" \
    "iwl: escaneando 13 canales de 2,4 GHz$" "red: canal  6,  -47 dBm, «HispaPrueba», seguridad 3$"
  if [ -n "${IWL_FW:-}" ]; then
    $AQUI/../build/iwl_analiza "$IWL_FW" > $Wi/real.txt 2>&1
    S=$Wi/real.txt
    mira "(INTEL) el firmware de verdad ($(basename $IWL_FW)): sus imágenes, y escaneo LMAC" "^Imagen RT (la de trabajo): " "^Imagen INIT (la de calibrar): " "^Escaneo: LMAC"
  else echo "  (sin IWL_FW=iwlwifi-7265D-29.ucode: solo con el firmware de mentira)"; fi
else ko "(INTEL) no se hacen build/iwl_analiza y build/iwl_modelo: $(tail -3 $Wi/make.txt)"; fi
if $AQUI/nucleo32/disco.sh $Wi/d.img 16 red > $Wi/disco.txt 2>&1; then
  DISCO=$Wi/d.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ipconfig ":pausa 3" "apagar" > $Wi/e1000.sal 2>&1
  tr -d '\r' < $Wi/e1000.sal > $Wi/e1000.txt
  S=$Wi/e1000.txt
  mira "(INTEL) en QEMU, con la e1000 (de Intel, pero de cable): la red como siempre" "^red: tarjeta 8086:100E en el bus PCI 00:03$" "^red: DHCP: IP 10.0.2.15"
  if grep -q "Wi-Fi: tarjeta\|WIFI.BIN en marcha" $S; then ko "(INTEL) con la e1000 se pone en marcha WIFI.BIN"; else ok "(INTEL) y WIFI.BIN no se pone en marcha (la e1000 no es Wi-Fi)"; fi
else ko "(INTEL) no se hace el disco: $(tail -2 $Wi/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Wi-Fi de Intel)"
fi   # el Wi-Fi de Intel
# ---------------------------------------- 30. (PDF) Hispa-PDF: PDF.BIN y su ventana
# El núcleo en C de castel/nucleo32/pdf, en el anfitrión (con ASan y UBSan:
# unir, separar, girar, quitar, comprimir con flujos de objetos, cifrar y
# descifrar con RC4, AES-128 y AES-256, texto, imágenes, crear con fotos; y,
# si están qpdf y pdftotext, lo que escribe, mirado por ellos), y PDF.BIN en
# QEMU: la orden PDF en el intérprete de órdenes (lo que deja, mirado luego
# desde fuera) y la ventana, HISPAPDF.BIN, en el escritorio: las miniaturas,
# girar con R, arrastrar una página, añadir otro PDF por su ruta, uno con
# contraseña (la pide), guardar todo junto y verlo en grande. Los PDF de la
# prueba los hace pdf/pruebas/haz_pdf.py (cada página dice cuál es).
if parte08 pdf; then
echo "== Hispa-PDF (PDF.BIN, en C, y su ventana HISPAPDF.BIN) =="
V=$P/pdf
mkdir -p $V/a $V/q $G
export MTOOLS_SKIP_CHECK=1
HP=$V/pdf-anfitrion
if bash $AQUI/nucleo32/pdf/anfitrion.sh $HP asan > $V/anf.txt 2>&1 && bash $AQUI/nucleo32/pdf/hacer.sh $V/PDF.BIN > $V/hacer.txt 2>&1; then
  ok "PDF.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $V/PDF.BIN) octetos) y Hispa-PDF en el anfitrión, con ASan y UBSan"
  python3 -I $AQUI/nucleo32/pdf/pruebas/haz_pdf.py $V/q/A.PDF A 10
  python3 -I $AQUI/nucleo32/pdf/pruebas/haz_pdf.py $V/q/B.PDF B 6 objstm
  python3 -I $AQUI/nucleo32/pdf/pruebas/haz_pdf.py $V/a/c0.pdf C 4
  python3 -I - $V/q/FOTO.PPM <<'PY'
import sys
w, h = 160, 120
d = bytearray()
for y in range(h):
    for x in range(w):
        d += bytes([x * 255 // w, y * 255 // h, 128])
open(sys.argv[1], 'wb').write(b'P6 %d %d 255\n' % (w, h) + d)
PY
  # 1. en el anfitrión
  ASAN_OPTIONS=detect_leaks=1 $HP CIFRA $V/a/c0.pdf /CLAVE=hola /SALIDA=$V/q/C.PDF > $V/a/o.txt 2>&1
  bien=1; por=
  ord_() { if ! ASAN_OPTIONS=detect_leaks=1 $HP "$@" > $V/a/o.txt 2>&1 || grep -q "Sanitizer\|runtime error" $V/a/o.txt; then bien=0; por="$por | $* -> $(tail -n 2 $V/a/o.txt | tr '\n' ' ')"; fi; }
  ord_ UNE $V/q/A.PDF $V/q/B.PDF /SALIDA=$V/a/u.pdf
  ord_ SEPARA $V/q/B.PDF /SALIDA=$V/a/s.pdf
  ord_ EXTRAE $V/q/A.PDF 3,1,8-10 /SALIDA=$V/a/e.pdf
  ord_ QUITA $V/q/A.PDF 2-9 /SALIDA=$V/a/qu.pdf
  ord_ GIRA $V/q/A.PDF 90 2 /SALIDA=$V/a/g.pdf
  ord_ COMPRIME $V/a/u.pdf /SALIDA=$V/a/co.pdf
  for m in RC4 AES128 AES256; do ord_ CIFRA $V/q/B.PDF /CLAVE=uno /DUENO=dos /CIFRADO=$m /SALIDA=$V/a/c$m.pdf; ord_ DESCIFRA $V/a/c$m.pdf /CLAVE=dos /SALIDA=$V/a/d$m.pdf; done
  ord_ CREA $V/q/FOTO.PPM /SALIDA=$V/a/cr.pdf
  ord_ IMAGENES $V/a/e.pdf /PAGINAS=1 /PUNTOS=50 /SALIDA=$V/a/p.png
  ord_ TEXTO $V/a/e.pdf /SALIDA=$V/a/e.txt
  [ $bien = 1 ] && ok "en el anfitrión: unir, separar, sacar, quitar, girar, comprimir, cifrar y descifrar (RC4, AES-128, AES-256), crear, imágenes y texto, sin quejas de ASan" || ko "en el anfitrión:$por"
  t=$(grep -o "Documento [AB], p.gina [0-9]*" $V/a/e.txt | tr '\n' ';')
  [ "$t" = "Documento A, página 3;Documento A, página 1;Documento A, página 8;Documento A, página 9;Documento A, página 10;" ] && ok "EXTRAE 3,1,8-10 deja esas páginas en ese orden (su texto)" || ko "EXTRAE: el texto da «$t»"
  if [ "$(head -c 8 $V/a/p.png | od -An -tx1 | tr -d ' \n')" = 89504e470d0a1a0a ]; then ok "IMAGENES escribe un PNG"; else ko "IMAGENES: no es un PNG"; fi
  for f in u co cRC4 cAES128 cAES256 dAES256 cr g; do
    case $f in u) n=16 ;; co) n=16 ;; cr) n=1 ;; g) n=10 ;; *) n=6 ;; esac
    r=$($HP INFO $V/a/$f.pdf /CLAVE=uno 2> /dev/null | head -1)
    case "$r" in *", $n páginas"*) ;; *) ko "$f.pdf: «$r» (tenían que ser $n páginas)"; bien=0 ;; esac
  done
  if command -v qpdf > /dev/null; then
    m=0
    for f in $V/a/*.pdf; do qpdf --check --password=uno $f > /dev/null 2>&1 || { m=1; ko "qpdf no da por bueno $(basename $f)"; }; done
    [ $m = 0 ] && ok "qpdf --check da por buenos los $(ls $V/a/*.pdf | wc -l) PDF que ha hecho (también los cifrados, con su contraseña)"
    r=$(qpdf --show-npages $V/a/s16.pdf 2> /dev/null; qpdf --show-npages $V/a/s1.pdf 2> /dev/null)
    [ -f $V/a/s6.pdf ] && ok "SEPARA: un archivo por página (S1.PDF ... S6.PDF)" || ko "SEPARA: no están sus archivos"
  fi
  if command -v pdftotext > /dev/null; then
    t=$(pdftotext -upw uno -f 1 -l 1 $V/a/cAES256.pdf - 2> /dev/null | head -1)
    [ "$t" = "Documento B, página 1" ] && ok "pdftotext lee el cifrado con AES-256 (con su contraseña)" || ko "pdftotext con AES-256: «$t»"
  fi
  # 2. la orden, en QEMU
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null 2>&1 && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uhpdf.cas -o $V/uhpdf.asm > /dev/null && nasm -f bin $V/uhpdf.asm -o $V/HISPAPDF.BIN
  bash $AQUI/nucleo32/disco.sh $V/d.img 32 > /dev/null
  mcopy -i $V/d.img $V/PDF.BIN $V/HISPAPDF.BIN $V/q/A.PDF $V/q/B.PDF $V/q/C.PDF $V/q/FOTO.PPM ::
  ESCRIBE=1 DISCO=$V/d.img ARRANQUE=ordenes MODULOS= MEMORIA=64 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN "PDF INFO B.PDF" ":pausa 3" \
    "PDF UNE A.PDF B.PDF /SALIDA=U.PDF" ":pausa 12" "PDF GIRA B.PDF 90 1-2 /SALIDA=G.PDF" ":pausa 6" "PDF INFO C.PDF" ":pausa 3" \
    "PDF TEXTO C.PDF /CLAVE=hola /PAGINAS=2" ":pausa 4" "PDF COMPRIME U.PDF /SALIDA=CO.PDF" ":pausa 15" \
    "PDF CIFRA B.PDF /CLAVE=uno /CIFRADO=AES256 /SALIDA=CI.PDF" ":pausa 8" "PDF CREA FOTO.PPM /SALIDA=CR.PDF" ":pausa 4" \
    "PDF IMAGENES A.PDF /PAGINAS=1-2 /PUNTOS=40 /SALIDA=P.PNG" ":pausa 15" > $V/orden.sal 2>&1
  tr -d '\r' < $V/orden.sal > $V/orden.txt
  S=$V/orden.txt
  mira "PDF INFO (un PDF 1.5 con flujos de objetos y la tabla en un flujo)" "^B.PDF: PDF 1.5, 6 p.ginas$" "^  p.gina 1: 216 x 279 mm$"
  mira "PDF UNE, GIRA, COMPRIME, CIFRA (AES-256), CREA e IMAGENES en Hispa-Pentium" "^Hecho: U.PDF, con 16 p.ginas de 2 archivos\.$" \
    "^Hecho: G.PDF (2 p.ginas giradas 90 grados)\.$" "^Hecho: CO.PDF: de " "^Hecho: CI.PDF, con contrase.a (AES de 256 bits)\.$" \
    "^Hecho: CR.PDF, con 1 p.gina\.$" "^  P01.PNG: 340 x 440 puntos$" "^  P02.PNG: 340 x 440 puntos$"
  mira "un PDF con contraseña: sin ella lo dice; con /CLAVE= se lee (AES-128, el texto de su página 2, con su tilde)" \
    "^C.PDF: lleva contrase.a: p.ngala con /CLAVE=\.\.\.$" "^Documento C, p.gina 2"
  m=0
  for f in U G CO CI CR; do saca $V/d.img $f.PDF $V/a/qemu_$f.pdf || m=1; done
  for f in U CO; do r=$($HP INFO $V/a/qemu_$f.pdf 2> /dev/null | head -1); case "$r" in *", 16 páginas"*) ;; *) m=1 ;; esac; done
  r=$($HP INFO $V/a/qemu_CI.pdf /CLAVE=uno 2> /dev/null | sed -n 2p); case "$r" in *AES*256*) ;; *) m=1 ;; esac
  [ $m = 0 ] && ok "lo que ha escrito en el disco de QEMU se lee bien desde fuera (16 páginas; el cifrado, con AES-256)" || ko "lo escrito en QEMU no está bien"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "la orden PDF: ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
  # 3. la ventana (800 x 600, maximizada: las miniaturas, de 4 en 4 desde (250, 89), cada 112 x 128)
  bash $AQUI/nucleo32/disco.sh $V/v.img 32 > /dev/null
  mcopy -i $V/v.img $V/PDF.BIN $V/HISPAPDF.BIN $V/q/A.PDF $V/q/B.PDF $V/q/C.PDF ::
  CAPTURAS=$G ESCRIBE=1 DISCO=$V/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
    ":escribe HISPAPDF A.PDF" ":tecla ret" ":pausa 30" ":captura hp1" ":clic 418 139" ":tecla r" ":pausa 1" ":captura hp2" \
    ":arrastra 530 139 262 139" ":pausa 1" ":captura hp3" ":clic 62 64" ":pausa 2" ":escribe B.PDF" ":tecla ret" ":pausa 2" \
    ":escribe C.PDF" ":tecla ret" ":pausa 25" ":captura hp4" ":escribe hola" ":tecla ret" ":pausa 30" \
    ":captura hp6" ":clic 165 64" ":pausa 2" ":captura hp7" ":tecla ret" ":pausa 40" ":captura hp8" ":clic 306 139" ":tecla ret" ":pausa 20" ":captura hp9" \
    ":tecla esc" ":pausa 2" > $V/ventana.sal 2>&1
  S=$V/ventana.sal
  miracap "la ventana: A.PDF con sus 10 páginas en miniatura (las pinta PDF.BIN, de seis en seis)" hp1 \
    negrita 0 0 160 30 "Hispa-PDF" texto 0 100 120 140 "A A.PDF" texto 20 120 120 150 "10 páginas" texto 250 170 380 200 "1 (A1)" texto 250 420 400 460 "9 (A9)"
  miracap "R gira la página elegida (su miniatura, apaisada; y la marca *)" hp2 texto 360 170 500 200 "2 (A2)*" distinto 362 89 112 100 $G/hp1.ppm
  miracap "arrastrar la tercera al principio: ahora va la primera" hp3 texto 250 170 380 200 "1 (A3)" texto 360 170 500 200 "2 (A1)" texto 470 170 620 200 "3 (A2)*"
  miracap "en el cuadro de añadir se escribe la ruta (B.PDF, C.PDF) y se añaden; C.PDF lleva contraseña: la pide" hp4 \
    texto 0 520 800 600 "Añadido" negrita 0 90 600 130 "C.PDF lleva contraseña" negrita 20 100 400 200 "La contraseña"
  miracap "con la contraseña, las páginas de los tres (10 + 6 + 4)" hp6 texto 0 520 400 600 "20 páginas" texto 0 100 240 300 "C C.PDF" texto 0 100 240 300 "4 páginas"
  miracap "Guardar: dónde (al lado del primero: NUEVO.PDF), comprimir, contraseña, uno por página" hp7 \
    negrita 0 80 300 130 "Guardar como" texto 20 100 600 200 "NUEVO.PDF" negrita 20 150 700 300 "Comprimir" negrita 20 200 700 500 "Cancelar"
  miracap "guardado" hp8 texto 0 530 800 600 "Guardado: NUEVO.PDF"
  miracap "Intro: la página, en grande" hp9 texto 0 80 300 110 "Esc o un clic: volver" distinto 300 200 200 200 $G/hp8.ppm
  if saca $V/v.img NUEVO.PDF $V/a/nuevo.pdf; then
    t=$($HP TEXTO $V/a/nuevo.pdf /PAGINAS=1-3,11,17,20 2> /dev/null | grep -o "Documento [ABC], p.gina [0-9]*" | tr '\n' ';')
    g=$($HP INFO $V/a/nuevo.pdf 2> /dev/null | grep -c "girada 90")
    if [ "$t" = "Documento A, página 3;Documento A, página 1;Documento A, página 2;Documento B, página 1;Documento C, página 1;Documento C, página 4;" ] && [ "$g" = 1 ]; then
      ok "NUEVO.PDF (hecho en QEMU): 20 páginas en el orden de la ventana (A3, A1, A2 girada, ..., B, C sin contraseña)"
    else ko "NUEVO.PDF: «$t», giradas $g"; fi
  else ko "no está NUEVO.PDF"; fi
  if ! grep -q "excepción\|\*\*\*" $S; then ok "la ventana: ni una excepción ni una queja del núcleo"
  else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
else
  ko "no se puede compilar Hispa-PDF: $(tail -n 3 $V/anf.txt $V/hacer.txt)"
fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin Hispa-PDF)"
fi   # (PDF) Hispa-PDF
# ------------------------- 31 y 32. (ZIP) Comprimidos y (MIRADOR) el Mirador
if parte08 zip; then parte_zip; else echo "  (SOLO=${SOLO08:-$SOLO}: sin Comprimidos)"; fi
if parte08 mirador; then parte_mirador; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Mirador)"; fi
# ------------------------- 38. (USUARIOS) los usuarios, la serie del catálogo y el azar
if parte08 usuarios; then parte_usuarios; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los usuarios)"; fi
# ------------------------- 58. (REMOTA) la consola remota «Dejar entrar a Claude»
if parte08 remota; then parte_remota; else echo "  (SOLO=${SOLO08:-$SOLO}: sin la consola remota)"; fi
# ------------- 34. (PANEL) el Panel de control y la hora por Internet (pruebas_panel.sh)
if parte08 panel; then parte_panel; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Panel de control)"; fi
# ------------------------- 36. (REPRODUCTOR) el Reproductor (pruebas_reproductor.sh)
if parte08 reproductor; then parte_reproductor; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Reproductor)"; fi     # (REPRODUCTOR)
if parte08 juegos; then parte_juegos; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los juegos)"; fi     # (JUEGOS2)
if parte08 pluma; then parte_pluma; else echo "  (SOLO=${SOLO08:-$SOLO}: sin Pluma 2)"; fi   # (PLUMA32) la 41
# ---------------------------------------- 40. (OFICINA2) la oficina 1.1 en HVPRUN
if parte08 oficina; then parte_oficina; else echo "  (SOLO=${SOLO08:-$SOLO}: sin la oficina 1.1)"; fi
if parte08 correo; then parte_correo; else echo "  (SOLO=${SOLO08:-$SOLO}: sin Correo)"; fi   # (CORREO)
if parte08 termo; then parte_termo; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Termómetro)"; fi   # (TERMO) la 46
if parte08 uefi; then parte_uefi; else echo "  (SOLO=${SOLO08:-$SOLO}: sin UEFI)"; fi   # (UEFI) la 48
if parte08 agenda; then parte_agenda; else echo "  (SOLO=${SOLO08:-$SOLO}: sin la Agenda)"; fi   # (AGENDA) la parte 43
if parte08 cuentas; then parte_cuentas; else echo "  (SOLO=${SOLO08:-$SOLO}: sin Cuentas 2)"; fi   # (CUENTAS32) la 44
if parte08 pintura; then parte_pintura; else echo "  (SOLO=${SOLO08:-$SOLO}: sin Pintura 2)"; fi   # (PINTURA32) la 45
if parte08 cortes; then parte_cortes; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los cortes de la red)"; fi   # (CORTES)
# ------------------------- 47. (ICONOS) los iconos del tapiz (pruebas_iconos.sh)
if parte08 iconos; then parte_iconos; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los iconos del tapiz)"; fi
if parte08 compat; then parte_compat; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Asistente de compatibilidad)"; fi   # (COMPAT) la 49
# ------------------------- 50. (TOUCHPAD) el touchpad (pruebas_touchpad.sh)
if parte08 touchpad; then parte_touchpad; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el touchpad)"; fi
# ------------------------- 53. (SDHCI) las tarjetas SD y el eMMC (pruebas_sdhci.sh)
if parte08 sdhci; then parte_sdhci; else echo "  (SOLO=${SOLO08:-$SOLO}: sin las tarjetas SD)"; fi
# ------------------------- 57. (RTL8723BS) el Wi-Fi por SDIO de las tabletas (pruebas_rtl.sh)
if parte08 rtl; then parte_rtl; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Wi-Fi por SDIO)"; fi
# ------------------------- 51. (PUNTEROS) los punteros del ratón (pruebas_punteros.sh)
if parte08 punteros; then parte_punteros; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los punteros del ratón)"; fi
# ------------------------- 52. (I2CHID) los touchpads HID sobre I2C (pruebas_i2chid.sh)
if parte08 i2chid; then parte_i2chid; else echo "  (SOLO=${SOLO08:-$SOLO}: sin los touchpads HID sobre I2C)"; fi
# ------------------------- 55. (MÓVIL) el móvil por USB (pruebas_movil.sh)
if parte08 movil; then parte_movil; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el móvil por USB)"; fi
# ------------------------- 56. (BLUETOOTH) el Bluetooth (pruebas_bt.sh)
if parte08 bt; then parte_bt; else echo "  (SOLO=${SOLO08:-$SOLO}: sin el Bluetooth)"; fi
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }

# ------------------------------------- 35. (BLOC) el Bloc de notas 1.0
if parte08 bloc; then
echo "== Hispa-Pentium: el Bloc de notas 1.0 (BLOC) =="
W=$P/bloc
rm -rf $W; mkdir -p $W $G
export MTOOLS_SKIP_CHECK=1
B=$AQUI/nucleo32/blocprueba
# 1. en Linux, con corre32: cada guion dice «bien: ...» o «mal: ...»
for s in prueba_bloc prueba_exp prueba_pag prueba_dif prueba_len; do
  BLOC_TMP=$W/t $B/$s.sh > $W/$s.txt 2>&1; rc=$?
  hubo=0
  while IFS= read -r l; do
    case "$l" in bien:*) ok "BLOC, $s: ${l#bien: }"; hubo=1 ;; mal:*) ko "BLOC, $s: ${l#mal: }"; hubo=1 ;; esac
  done < <(iconv -f UTF-8 -t UTF-8 -c $W/$s.txt)
  [ $rc != 0 ] && [ $hubo = 0 ] && ko "BLOC, $s acaba mal: $(tail -3 $W/$s.txt)"
done
# 2. en el escritorio
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN || ko "semilla.cas no compila"
if $AQUI/nucleo32/disco.sh $W/d.img 32 > $W/disco.txt 2>&1; then
  cp $B/muestras/MUESTRA.CAS $B/guiones/datos/A1.TXT $B/guiones/datos/A2.TXT $B/guiones/datos/P.C $W/
  mkdir -p $W/docs
  printf 'una carta para Juan\r\nsaludos\r\n' > $W/docs/CARTA.TXT
  printf 'la lista de la compra\r\npan, leche y una carta\r\n' > $W/docs/LISTA.TXT
  printf 'nada que ver\r\n' > $W/docs/OTRO.TXT
  python3 -I -c "
import sys
sys.stdout.buffer.write(''.join('linea %06d con algo de texto para llenar\r\n' % i for i in range(250000)).encode())" > $W/GRANDE.TXT
  mmd -i $W/d.img ::DOCS
  mcopy -i $W/d.img $W/docs/*.TXT ::DOCS/
  mcopy -i $W/d.img $W/MUESTRA.CAS $W/A1.TXT $W/A2.TXT $W/P.C $W/GRANDE.TXT ::/
  cp $W/d.img $W/s.img
  # 2a. MUESTRA.CAS (con su lenguaje: Castel), buscar, reemplazar, la letra
  # grande, Abrir, las pestañas, comparar, buscar en una carpeta, la ayuda,
  # en columna y la pregunta al salir (sin guardar: la sesión, sí)
  CAPTURAS=$G ESCRIBE=1 DISCO=$W/s.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "bloc muestra.cas" ":pausa 4" ":captura bl1" \
    ":tecla ctrl-f" ":escribe fin" ":tecla ret" ":pausa 1" ":captura bl2" ":tecla esc" ":tecla ctrl-h" ":pausa 1" ":captura bl3" ":tecla esc" \
    ":raton 400 300" ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 1" ":pausa 2" ":captura bl4" ":monitor sendkey ctrl 1500" ":monitor mouse_move 0 0 -1" ":pausa 2" \
    ":clic 157 199" ":pausa 1" ":captura bl5" ":clic 157 199" ":pausa 1" \
    ":tecla ctrl-o" ":pausa 2" ":captura bl6" ":escribe A1.TXT" ":tecla ret" ":pausa 2" ":escribe zz" ":raton 400 300" ":pausa 1" ":captura bl7" \
    ":tecla ctrl-o" ":pausa 1" ":escribe A2.TXT" ":tecla ret" ":pausa 2" ":clic 340 126" ":pausa 1" ":clic 400 395" ":pausa 2" ":captura bl8" ":tecla esc" ":pausa 1" \
    ":tecla ctrl-shift-f" ":pausa 1" ":escribe carta" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla end" ":escribe DOCS" ":tecla ret" ":pausa 3" ":captura bl9" \
    ":doble 200 254" ":pausa 2" ":captura bl10" ":tecla f1" ":pausa 2" ":captura bl11" \
    ":tecla ctrl-tab" ":pausa 1" ":tecla ctrl-home" ":tecla alt-shift-down" ":tecla alt-shift-down" ":escribe #" ":pausa 1" ":captura bl12" \
    ":clic 604 101" ":pausa 1" ":captura bl13" ":clic 380 440" ":pausa 3" ":captura bl14" > $W/a.sal 2>&1
  S=$W/a.sal
  mira "el Bloc de notas se lanza desde la Consola con MUESTRA.CAS" "escritorio: lanzo .CONSOLA.BIN"
  miracap "MUESTRA.CAS en Castel (por la extensión): las palabras clave en negrita y de color, los números de línea, el lenguaje y la página abajo" bl1 \
    negrita 140 88 152 100 "MUESTRA.CAS - Bloc de notas" negrita 166 186 178 198 "módulo" texto 222 186 234 198 "Muestra" negrita 318 250 330 262 "entonces" \
    texto 166 170 178 182 "; un comentario: si mientras fin" negrita 120 170 150 190 "1" texto 120 454 132 466 "MUESTRA.CAS   Lín 1, Col 1" texto 440 454 480 466 "Castel | UTF-8 | LF" \
    rgb 160 199 FFFFFF rgb 158 205 5A7396 8
  miracap "Ctrl+F: el panel de buscar (con sus casillas), y lo encontrado elegido (en blanco sobre azul)" bl2 texto 122 398 134 410 "Buscar:" texto 354 400 366 412 "Siguiente" \
    texto 142 426 154 438 "Mayúsculas" rgb 410 183 2F74D0
  miracap "Ctrl+H (Control y la H: el núcleo dice ya las teclas de cambio, y no es Retroceso): reemplazar, con «Por:»" bl3 texto 122 398 134 410 "Por:" texto 454 400 466 412 "Reemplazar"
  miracap "Ctrl+rueda: la letra de ancho fijo suavizada, más grande (el renglón de 20)" bl4 distinto 170 176 300 60 $G/bl1.ppm texto 120 454 132 466 "MUESTRA.CAS   Lín 1, Col 33"
  miracap "un clic en el [-] del margen pliega el módulo entero: se ve «...» y el número de la línea de después es el 20" bl5 negrita 166 186 178 198 "módulo" \
    texto 124 202 136 214 "20" notexto 160 200 400 300 "función"
  miracap "Ctrl+O: Abrir, con la carpeta (las carpetas primero) y la ruta abajo" bl6 negrita 122 170 134 182 'C:\' negrita 200 186 400 200 "DOCS" texto 122 426 134 438 "Abrir:"
  # (PUNTEROS) para bl7 el ratón, abajo en el texto: la «I» en (157, 199) tapaba el pie de las pestañas
  miracap "las pestañas: MUESTRA.CAS y A1.TXT (con cambios: « *», no solo un color)" bl7 texto 126 171 138 183 "MUESTRA.CAS" negrita 266 171 278 183 "A1.TXT *"
  miracap "Ver > Comparar con la de al lado: A1 y A2 lado a lado, con «*» (cambiado), «-» (quitado) y «+» (añadido) en el margen, el hueco rayado y el resumen" bl8 \
    negrita 128 194 140 206 "*" negrita 128 242 140 254 "-" negrita 380 274 392 286 "+" texto 120 454 132 466 "3 trozos distintos" rgb 200 248 FDE2E2 rgb 450 280 DCFCE7
  miracap "Ctrl+Mayús+F: buscar «carta» en C:\\DOCS\\: la pestaña de lo encontrado, con cada archivo y su línea" bl9 texto 120 194 132 206 'Buscar «carta» en C:\DOCS\' \
    texto 120 226 132 238 'C:\DOCS\CARTA.TXT' texto 120 242 132 254 "  1: una carta para Juan" texto 120 454 132 466 "Encontradas 2 líneas en 2 archivos"
  miracap "dos clics en una línea de lo encontrado: el archivo abierto, en esa línea y con lo buscado elegido" bl10 negrita 140 88 152 100 'C:\DOCS\CARTA.TXT - Bloc de notas' rgb 168 207 2F74D0
  miracap "F1: la ayuda, en una pestaña para leer" bl11 texto 120 194 132 206 "Bloc de notas de Hispa-Pentium: la ayuda" negrita 140 88 152 100 "Ayuda - Bloc de notas"
  miracap "en columna (Alt+Mayús+flechas abajo, con las teclas de cambio del núcleo): «#» en las tres primeras líneas de MUESTRA.CAS a la vez" bl12 \
    texto 166 194 178 206 "#" texto 174 194 186 206 "; un comentario: si mientras fin" texto 166 210 178 222 "#" texto 166 226 178 238 "#" texto 340 454 380 466 "(en bloque)"
  miracap "cerrar con cambios: la pregunta, con Guardar todo y salir, Salir sin guardar y Cancelar" bl13 texto 122 398 134 410 "Hay 2 documentos con cambios sin guardar." texto 312 427 324 439 "Salir sin guardar"
  miracap "«Salir sin guardar»: se cierra (ni ventana ni botón en la barra)" bl14 notexto 100 80 700 120 "Bloc de notas" notexto 260 570 500 590 "MUESTRA"
  if saca $W/s.img USUARIOS/USUARIO/BLOC.INI $W/bloc.ini && grep -q '^abierto=C:\\MUESTRA.CAS|[0-9]*.$' $W/bloc.ini && grep -q '^abierto=C:\\A1.TXT|' $W/bloc.ini && grep -q '^abierto=C:\\A2.TXT|' $W/bloc.ini \
    && grep -q '^abierto=C:\\DOCS\\CARTA.TXT|' $W/bloc.ini && grep -q '^activo=0.$' $W/bloc.ini && ! grep -qi 'Ayuda\|Encontrado' $W/bloc.ini && saca $W/s.img A1.TXT $W/a1 && cmp -s $W/a1 $W/A1.TXT; then
    ok "la sesión, en \\USUARIOS\\USUARIO\\BLOC.INI (fuera de la zona protegida): los cuatro archivos con su ruta entera y su cursor, y cuál estaba delante (no la ayuda ni lo encontrado); A1.TXT, sin guardar"
  else ko "BLOC.INI no recuerda lo abierto:"; sed 's/^/        /' $W/bloc.ini 2> /dev/null | head -12; fi
  if ! grep -q "excepción\|\*\*\*" $S; then ok "BLOC: en toda la sesión, ni una excepción ni una queja del núcleo"; else ko "BLOC: el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
  # 2b. otra vez, sin decirle nada: lo de la última vez; y 11 MB
  CAPTURAS=$G ESCRIBE=1 DISCO=$W/s.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "bloc" ":pausa 4" ":captura bl15" ":clic 604 101" ":pausa 2" \
    "bloc grande.txt" ":pausa 8" ":tecla ctrl-end" ":pausa 2" ":captura bl16" > $W/b.sal 2>&1
  S=$W/b.sal
  miracap "BLOC sin nada: abre lo de la última vez (MUESTRA.CAS, A1.TXT, A2.TXT y CARTA.TXT) y deja MUESTRA.CAS delante, con el cursor donde estaba" bl15 \
    negrita 126 171 138 183 "MUESTRA.CAS" texto 200 171 460 183 "A2.TXT" texto 120 454 132 466 'C:\MUESTRA.CAS   Lín 3, Col'
  t=$(sed -n 's/^.*bloc: abierto GRANDE.TXT, 10750000 octetos, en \([0-9]*\) ms.*/\1/p' $S | head -1)
  if [ -n "$t" ] && [ "$t" -lt 30000 ]; then ok "11 MB (250.000 líneas) abiertos en $t ms (QEMU sin aceleración)"; else ko "GRANDE.TXT no se abre (o tarda: ${t:-?} ms)"; fi
  miracap "y Ctrl+Fin va a la última línea (la 250.001)" bl16 texto 100 100 700 500 "linea 249999 con algo de texto para llenar" texto 100 100 700 500 "GRANDE.TXT   Lín 250001, Col 1"
  # 2c. soltar en el Bloc un archivo arrastrado desde Mi equipo (EV_SOLTADO: la llamada 173)
  cp $W/d.img $W/m.img
  CAPTURAS=$G DISCO=$W/m.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "bloc muestra.cas" ":pausa 4" ":doble 60 40" ":pausa 4" ":arrastra 795 497 470 430" ":pausa 2" ":doble 300 160" ":pausa 3" \
    ":arrastra 356 310 550 300" ":pausa 3" ":captura bl17" > $W/c.sal 2>&1
  miracap "un archivo (A2.TXT) arrastrado desde Mi equipo y soltado en el Bloc se abre en otra pestaña" bl17 texto 120 454 132 466 'C:\A2.TXT   Lín 1, Col 1'
  # 2d. Ver: las líneas largas partidas (como siempre) y sin partir; los
  # espacios, los tabuladores y los finales de línea, con su dibujo
  printf '\tuno dos\r\nuna l\xa1nea muy larga que no cabe en lo ancho de la ventana del Bloc de notas y que sigue y sigue hasta el final\r\n' > $W/VER.TXT
  cp $W/d.img $W/v.img
  mcopy -i $W/v.img $W/VER.TXT ::/
  CAPTURAS=$G DISCO=$W/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $W/SEMILLA.BIN \
    ":pausa 3" ":doble 62 185" ":pausa 3" "bloc ver.txt" ":pausa 4" ":captura bl18" ":tecla alt-v" ":pausa 1" ":clic 420 200" ":pausa 1" \
    ":tecla alt-v" ":pausa 1" ":clic 420 224" ":pausa 1" ":tecla alt-v" ":pausa 1" ":clic 420 176" ":pausa 1" ":captura bl19" > $W/v.sal 2>&1
  miracap "un .TXT se ve como siempre: sin números de línea y con las líneas largas partidas al ancho de la ventana" bl18 \
    texto 120 186 132 198 "una línea muy larga que no cabe en lo ancho de la ventana del" texto 120 202 132 214 " Bloc de notas y que sigue y sigue hasta el final" \
    texto 120 454 132 466 "VER.TXT   Lín 1, Col 1" texto 380 454 620 466 "Texto | 437 | CR LF"
  miracap "Ver > Espacios y tabuladores (· y »), Finales de línea (¬) y sin Ajuste de línea: la larga, en una fila (lo demás, a la derecha)" bl19 \
    texto 132 170 144 182 "»" texto 152 170 164 182 "uno" texto 176 170 188 182 "·" texto 184 170 196 182 "dos" texto 209 170 221 182 "¬" \
    texto 120 186 132 198 "una" texto 144 186 156 198 "·" notexto 100 200 640 260 "sigue"
  # las capturas, para la documentación (docs/pentium/bloc/)
  if [ -n "${CAPTURAS_A:-}" ]; then mkdir -p "$CAPTURAS_A"; cp $G/bl*.ppm "$CAPTURAS_A"/ 2> /dev/null; fi
else ko "no se puede hacer el disco: $(tail -2 $W/disco.txt)"; fi
else
  echo "  (SOLO=${SOLO08:-$SOLO}: sin el Bloc de notas)"
fi   # el Bloc de notas

# ------------------------------------------------- 15. CASTEL32 (pruebas32.sh)
[ -z "${SOLO:-}" ] && parte15
[ -z "${SOLO:-}" ] && parte23

echo "== $nok bien, $fail mal =="
exit $((fail > 0))
