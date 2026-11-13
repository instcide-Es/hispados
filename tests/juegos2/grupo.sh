# El grupo «Juegos» por omisión (tests/run_juegos2_tests.sh): sin CENTRO.INI,
# sale un acceso por cada .HVP de \HISPADOS\JUEGOS (los dos de la primera
# tanda, los cuatro de la segunda e Hispa-Nes), con el nombre y el icono que
# lleva cada módulo;
# como no caben en una fila, la ventana del grupo se alarga sola. Cada juego
# nuevo se abre como una ventana y se cierra; al salir, CENTRO.INI tiene los
# siete accesos y la memoria extendida queda como al entrar.
echo "== Juegos (segunda tanda): el grupo Juegos por omisión, abrir y cerrar cada uno =="
disco
if mdir -i "$IMG" ::HISPADOS/JUEGOS/BLOQUES.HVP ::HISPADOS/JUEGOS/SOLITARI.HVP \
        ::HISPADOS/JUEGOS/TRAGABOL.HVP ::HISPADOS/JUEGOS/ALMACEN.HVP > /dev/null 2>&1; then
  ok "BLOQUES, SOLITARI, TRAGABOL y ALMACEN.HVP están en \\HISPADOS\\JUEGOS del disquete"
else
  ko "faltan .HVP de la segunda tanda en \\HISPADOS\\JUEGOS del disquete"
fi
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/JUEGOS/BLOQUES.HVP ::HISPADOS/JUEGOS/SOLITARI.HVP \
        ::HISPADOS/JUEGOS/TRAGABOL.HVP ::HISPADOS/JUEGOS/ALMACEN.HVP > /dev/null 2>&1; then
    ok "y en \\HISPADOS\\JUEGOS de la imagen USB"
  else
    ko "faltan .HVP de la segunda tanda en \\HISPADOS\\JUEGOS de la imagen USB"
  fi
fi
libre=$(mdir -i "$IMG" :: | sed -n 's/^ *\([0-9 ]*\) bytes free.*/\1/p' | tr -d ' ')
if [ "${libre:-0}" -ge 200000 ]; then ok "caben en el disquete: quedan $libre octetos libres"; else ko "disquete: solo quedan ${libre:-?} octetos libres"; fi
{ entrar
  doble 310 129; sleep 1.5                  # el grupo Juegos
  aparca
  shot g1
  tecla right right ret; sleep 2.5          # Bloques
  aparca
  shot g2
  tecla alt-f4; sleep 1
  tecla right ret; sleep 2.5                # Solitario
  shot g3
  tecla alt-f4; sleep 1
  tecla right ret; sleep 2.5                # Tragabolas (segunda fila)
  shot g4
  tecla alt-f4; sleep 1
  tecla right ret; sleep 2.5                # Almacén
  shot g5
  tecla alt-f4; sleep 1
  shot g6
  salir
} | run 150 -vga std
# -- el grupo: siete accesos en dos filas, con su nombre y su icono; la ventana se ha alargado --
chk $T/g1.ppm tam 800 600 marco 248 188 452 207 activa titulo 248 188 activa "Juegos" \
  texto 286 291 9 12 fina "Culebra" texto 362 291 0 8 fina "Campo de minas" \
  texto 494 291 0 8 fina "Bloques" texto 590 291 0 8 fina "Solitario" \
  texto 274 367 0 8 fina "Tragabolas" texto 390 367 0 8 fina "Almacén" texto 486 367 0 8 fina "Hispa-Nes" \
  px 521 257 0 "icono de Bloques" px 619 265 12 "icono de Solitario: el dorso añil" px 631 265 10 "icono de Solitario: el oro" \
  px 313 341 2 "icono de Tragabolas: el Tragón" px 417 341 10 "icono de Almacén: la caja" \
  px 508 343 15 "icono de Hispa-Nes: el mando" px 519 337 13 "icono de Hispa-Nes: un botón"
# -- cada juego se abre como una ventana --
chk $T/g2.ppm marco 150 60 412 431 activa titulo 150 60 activa "Bloques"
chk $T/g3.ppm marco 90 40 462 429 activa titulo 90 40 activa "Solitario"
chk $T/g4.ppm marco 40 30 520 449 activa titulo 40 30 activa "Tragabolas"
chk $T/g5.ppm marco 140 60 468 397 activa titulo 140 60 activa "Almacén"
chk $T/g6.ppm marco 248 188 452 207 activa titulo 248 188 activa "Juegos" texto 390 367 9 12 fina "Almacén"
ini "Modulos=8"
ini "Programa=A:\\HISPADOS\\JUEGOS\\BLOQUES.HVP"; ini "Programa=A:\\HISPADOS\\JUEGOS\\SOLITARI.HVP"
ini "Programa=A:\\HISPADOS\\JUEGOS\\TRAGABOL.HVP"; ini "Programa=A:\\HISPADOS\\JUEGOS\\ALMACEN.HVP"
if [ "$(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')" = "[Grupo] Nombre=Principal Nombre=Editar Nombre=Gestor Nombre=Información Nombre=Hispa-DOS Nombre=Configuración [Grupo] Nombre=Accesorios Nombre=Bloc de notas Nombre=Calculadora Nombre=Reloj Nombre=Sistema Nombre=Pintura [Grupo] Nombre=Juegos Nombre=Culebra Nombre=Campo de minas Nombre=Bloques Nombre=Solitario Nombre=Tragabolas Nombre=Almacén Nombre=Hispa-Nes " ]; then
  ok "CENTRO.INI: los seis juegos e Hispa-Nes, en el grupo Juegos"
else
  ko "CENTRO.INI, grupos: $(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')"
fi
muestra g1 grupo
salida_bien
