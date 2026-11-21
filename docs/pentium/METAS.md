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
- [x] ~~**Administrador de dispositivos** (PCI, USB, ACPI: qué hay, qué controlador lo lleva, recursos y estado)~~ — rama pentium-dispositivos (21/11: DISPOSIT.BIN y la llamada 155; informe en X:\DISPOSIT.TXT)

## El escritorio
- [ ] **Menú Inicio**: «Todos los programas» con todo lo instalado (también lo de los paquetes), y «Apagar equipo» siempre a la vista aunque haya muchos programas (pedido de Carlos 21/11)
- [ ] **Rueda del ratón en todo el entorno**: menús, listas y rejillas, Explorador, Bloc de notas, Consola, cuadros, ClicGest (pedido de Carlos 21/11)
- [x] ~~Explorador de archivos: árbol de carpetas, vistas (iconos, lista, detalles), copiar/cortar/pegar, arrastrar y soltar, papelera, buscar, propiedades, abrir con…, miniaturas de imágenes~~ (21/11; falta escribir nombres largos)
- [x] ~~Dock: barra de lanzamiento con los programas favoritos y los abiertos, que se pueda fijar y quitar~~ (21/11)
- [ ] Barra de tareas: agrupar ventanas, vista previa, bandeja con más iconos (sonido, Wi-Fi)
- [ ] Panel de control completo: fecha y hora (y por Internet), pantalla (hecho), sonido, ratón, teclado, red y Wi-Fi, energía, usuarios, programas (hecho: actualizaciones)
- [ ] Temas y fondo de escritorio; letras TrueType del sistema; protector de pantalla
- [ ] Portapapeles con imágenes y archivos; arrastrar y soltar entre programas
- [ ] Ayuda del sistema con buscador

## El sistema
- [ ] Sonido: HD Audio y AC'97, mezclador, sonidos del sistema
- [ ] Wi-Fi (el núcleo de C de Hispa-486: Broadcom y Atheros) con su ventanita
- [ ] Usuarios con contraseña y carpetas propias; permisos
- [ ] Suspender e hibernar (ACPI S3/S4)
- [ ] Impresoras (USB y de red)
- [ ] Compartir carpetas en red (SMB) y FTP
- [x] ~~**Tailscale** (WireGuard + su red privada: cada equipo localizado por su nombre) y **SSH** (entrar en la terminal de Hispa-Pentium desde otro equipo y desde él en los demás)~~ — rama pentium-tailscale (21/11; lo directo por UDP, más adelante: docs/PENTIUM.md §14 quinquies)
- [ ] Más tarjetas de red y controladoras (las que vaya trayendo el taller)
- [ ] NTP: la hora por Internet (y quitar el riesgo de la hora sin cifrar de TLS)
- [ ] TLS 1.3 y descompresión gzip

## Programas
- [x] ~~Programas de Windows de 32 bits (cargador PE) y **DirectX** (DirectDraw, DirectSound, DirectInput), como en Hispa-486~~ (WIN32.BIN: NASM, TCC, FASMW, Naves; [WIN32.md](WIN32.md)). Queda: hilos de verdad, SEH, Direct3D, el sonido de DirectSound
- [ ] Reproductor de música y vídeo (MP3, OGG, WAV; vídeo sencillo)
- [ ] Visor de imágenes y Pintura
- [ ] Compresor ZIP
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
