# Hispa-Pentium: lo que falta para estar a la altura de un sistema operativo

*Lista de trabajo pedida por Carlos el 21/11/2026 («explorador de archivos, dock,
DirectX y lo que necesitemos para estar a la altura de un sistema operativo»).
Se tacha lo que se acaba; el detalle técnico de cada cosa va en docs/PENTIUM.md.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

## En marcha (21/11/2026)
- [ ] **Navegador web** (NetSurf de Hispa-486 con JavaScript, sobre la red y el HTTPS de Pentium) — rama pentium-navegador
- [ ] **CD y DVD** (ATAPI por IDE, SATA y USB; ISO 9660 + Joliet; meter y sacar en marcha) y **montar imágenes .ISO como CD virtual** — rama pentium-cd
- [ ] **Controles de ventana completos** (pestañas, rejillas con columnas, barra de herramientas, recuadros, fechas, listas desplegables) y ClicGest 0.2 — rama controles-erp
- [ ] **Disco SATA del Toshiba NB250** (ICH7) — rama pentium-sata, a falta de la prueba de Carlos
- [x] ~~**Explorador de archivos** y **dock** — rama pentium-explorador~~ (hecho el 21/11: docs/PENTIUM.md, «El explorador de archivos» y «El dock»)
- [x] ~~**Programas de Windows y DirectX** (HW32 de Hispa-486 llevado a Pentium) — rama pentium-win32~~ (21/11/2026: [WIN32.md](WIN32.md))
- [x] ~~**Antivirus «Escudo»** (pedido por el presidente de la asociación): motor propio con las firmas de ClamAV, la ventana, la cuarentena, «¿Revisar la memoria?» al conectar una memoria USB o un CD, las descargas del Navegador y las firmas por Internet desde actualiza.hispados.es~~ — rama pentium-escudo (21/11: ESCUDO.BIN y las firmas en tres paquetes, medidas con las bases de verdad)
- [x] ~~**Cortafuegos de Escudo** («si se llama Escudo, añádele un cortafuegos», Carlos 21/11): nada entra si no lo pide el equipo, reglas por puerto y red, avisar cuando un programa quiera escuchar~~ — rama pentium-escudo (21/11: rcorta.cas en RED.BIN y la pestaña «Cortafuegos»)
- [x] ~~**Administrador de dispositivos** (PCI, USB, ACPI: qué hay, qué controlador lo lleva, recursos y estado)~~ — rama pentium-dispositivos (21/11: DISPOSIT.BIN y la llamada 155; informe en X:\DISPOSIT.TXT)

## El escritorio
- [ ] **Menú Inicio**: «Todos los programas» con todo lo instalado (también lo de los paquetes), y «Apagar equipo» siempre a la vista aunque haya muchos programas (pedido de Carlos 21/11)
- [ ] **Rueda del ratón en todo el entorno**: menús, listas y rejillas, Explorador, Bloc de notas, Consola, cuadros, ClicGest (pedido de Carlos 21/11)
- [x] ~~Explorador de archivos: árbol de carpetas, vistas (iconos, lista, detalles), copiar/cortar/pegar, arrastrar y soltar, papelera, buscar, propiedades, abrir con…, miniaturas de imágenes~~ (21/11; falta escribir nombres largos)
- [x] ~~Dock: barra de lanzamiento con los programas favoritos y los abiertos, que se pueda fijar y quitar~~ (21/11)
- [x] ~~Menú Inicio con todos los programas («Todos los programas» con carpetas, por el tipo de cada paquete), fijados y más usados, y Cerrar sesión / Reiniciar / Apagar equipo siempre a la vista~~ — rama pentium-menu (21/11: docs/PENTIUM.md, «El menú Inicio y los atajos de teclado»)
- [x] ~~La rueda del ratón en todo el entorno: menús y submenús largos, rejillas de controles.cas, Bloc, Consola (historial), Explorador, Administrador de dispositivos, Actualizaciones; Ctrl+rueda, la letra o la vista~~ — ramas pentium-menu y pentium-rueda (21/11; ClicGest, en su versión siguiente)
- [x] ~~Atajos de teclado de todo el sistema: Alt+F4, Alt+Tab, Logo (E, D, R, L), Ctrl+Esc, Alt+Espacio, Ctrl+Alt+Supr (Administrador de tareas), Impr Pant, F1~~ — rama pentium-menu (21/11)
- [ ] Barra de tareas: agrupar ventanas, vista previa, bandeja con más iconos (sonido; el Wi-Fi, hecho)
- [ ] Panel de control completo: fecha y hora (y por Internet), pantalla (hecho), sonido, ratón, teclado, red y Wi-Fi, energía, usuarios, programas (hecho: actualizaciones y «Programas instalados»)
- [x] ~~**Gestor de programas instalados** por tipos para desinstalarlos (como «Agregar o quitar programas»): árbol por tipos, tamaño, fecha, dependencias, desinstalar (con lo que lo necesita), reparar, ver archivos, los programas sueltos con «Borrar»; y en la misma ventana, «Programas y actualizaciones» con pestañas Pendientes, Al día, Instaladas y Sin instalar, con búsqueda~~ — rama pentium-programas (21/11: PROGRAMA.BIN, el campo `tipo=` de los paquetes; docs/PENTIUM.md §13)
- [ ] Temas y fondo de escritorio; letras TrueType del sistema; protector de pantalla
- [ ] Portapapeles con imágenes y archivos; arrastrar y soltar entre programas
- [ ] Ayuda del sistema con buscador

## El sistema
- [ ] Sonido: HD Audio y AC'97, mezclador, sonidos del sistema
- [x] ~~Wi-Fi (el núcleo de C de Hispa-486: Broadcom y Atheros) con su ventanita~~ — rama pentium-wifi (21/11: WIFI.BIN, REDWIFI.BIN, el icono con rayitas, Configuración › Red; a falta de probarlo en el HP Mini)
- [ ] Usuarios con contraseña y carpetas propias; permisos
- [ ] Suspender e hibernar (ACPI S3/S4)
- [ ] Impresoras (USB y de red)
- [ ] Compartir carpetas en red (SMB) y FTP
- [x] ~~**Tailscale** (WireGuard + su red privada: cada equipo localizado por su nombre) y **SSH** (entrar en la terminal de Hispa-Pentium desde otro equipo y desde él en los demás)~~ — rama pentium-tailscale (21/11; lo directo por UDP, más adelante: docs/PENTIUM.md §14 quinquies)
- [ ] Más tarjetas de red y controladoras (las que vaya trayendo el taller)
- [ ] Escudo: RAR y UPX por dentro, el interior de los PDF, revisar en segundo plano, reglas de UDP en el cortafuegos
- [ ] NTP: la hora por Internet (y quitar el riesgo de la hora sin cifrar de TLS)
- [ ] TLS 1.3 y descompresión gzip

## Programas
- [x] ~~Programas de Windows de 32 bits (cargador PE) y **DirectX** (DirectDraw, DirectSound, DirectInput), como en Hispa-486~~ (WIN32.BIN: NASM, TCC, FASMW, Naves; [WIN32.md](WIN32.md)). Queda: hilos de verdad, SEH, Direct3D, el sonido de DirectSound
- [ ] Reproductor de música y vídeo (MP3, OGG, WAV; vídeo sencillo)
- [ ] Visor de imágenes y Pintura
- [ ] Compresor ZIP
- [ ] **Herramienta de PDF propia** (en lugar de PDF24, que es de 64 bits y no libre): unir y separar, girar/ordenar/quitar páginas, comprimir, PDF ↔ imágenes, PDF a texto, poner y quitar contraseña; con piezas libres (qpdf…) y el Lector de la oficina; al ser nuestra, se compila tal cual para Hispa-64 (pedido de Carlos 21/11)
- [ ] Calculadora, Reloj, Mapa de caracteres
- [ ] Juegos: los emuladores (Hispa-Nes, Hispa-System, Hispa-Snes, Hispa-Drive) con sonido en Pentium
- [ ] Correo
- [ ] ClicGest (programa de ClicGest, licencia propia): 0.1 publicado; 0.2 con pestañas en marcha

## Hecho
- [x] Núcleo de 32 bits en Castel, escritorio al estilo de XP, oficina de Hispa-386 dentro (HVPRUN)
- [x] Discos IDE y SATA, USB en caliente, red por cable, HTTPS, apagar y batería (probados en portátiles de verdad)
- [x] Instalador, CD de instalación, actualizaciones firmadas por Internet (actualiza.hispados.es) con vuelta atrás del núcleo
- [x] Cambiar la resolución en marcha; teclado numérico
- [x] CASTEL de 32 bits: Hispa-Pentium se compila a sí mismo
