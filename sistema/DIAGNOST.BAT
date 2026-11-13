@ECHO OFF
REM DIAGNOSTICO (DIAGNOST.BAT): guarda el informe completo del equipo en una
REM carpeta nueva de la unidad de arranque, \INFORMES\EQUIPO01 (EQUIPO02 la
REM vez siguiente...): INFORME.TXT (versión, fecha, EQUIPO, MEM /C, SET,
REM HSPSND /INFO, HSPRATON /INFO, MANDO, CONFIG.SYS y AUTOEXEC.BAT) y los
REM volcados en binario (PCI, ACPI, SMBIOS, USB, vídeo, CMOS, discos...).
REM Lo hace EQUIPO /INFORME. Suba la carpeta al FTP, a HispaDOS/informes.
REM   DIAGNOSTICO /NOISA      sin sondear los puertos ISA (y /NOPCI, /NOVESA...)
REM   DIAGNOSTICO /NOSONIDO   sin ofrecer la prueba de sonido (AUDICION)
REM   DIAGNOSTICO /?          ayuda
EQUIPO /INFORME %1 %2 %3 %4
