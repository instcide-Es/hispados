#!/bin/bash
# Prueba de Datos (DATOS.HVP), la base de datos del grupo «Productividad».
#
# Abre Hispa Ventanas con Datos (clave «Reabrir=» de CENTRO.INI) y, en varias
# partes (cada una arranca de nuevo sobre el mismo disquete, así que también
# se prueba que lo guardado sigue ahí):
#  1. una tabla de la plantilla «Socios»: añadir registros, una fecha que no
#     existe (lo dice y no la acepta), ordenar por un campo, la ficha, cambiar
#     y borrar; y lo que queda en DATOS.BDD (tamaño, suma y registros);
#  2. otra tabla («Inventario») en la misma base: números con decimales, el
#     resumen (suma, media, mayor, menor), filtrar, buscar, exportar a .CSV,
#     imprimir el listado y pasar de una tabla a otra;
#  3. diseñar: añadir, cambiar (con cambio de tipo), mover y quitar campos,
#     una tabla en blanco, un campo que viene de otra tabla, un texto largo,
#     el ancho de una columna arrastrando, cambiar el nombre y borrar una tabla;
#  4. importar un .CSV (adivina los tipos), el diálogo de un registro de más
#     de catorce campos, las etiquetas, 3.000 registros de una vez, base
#     nueva, abrir otra base y guardar una copia;
#  5. un corte de luz (un .BDD a medias y la copia anterior entera), la base
#     que se abrió la última vez, sin impresora, los límites, duplicar,
#     buscar lo que no está y filtrar un campo de sí o no.
#
# Con SOLO=n se repite solo la parte n, sobre el disquete que dejó la
# anterior (build/gen/datos_pN.img). Necesita qemu-system-i386, mtools, socat
# e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/DATOS.HVP
NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/datos.asm $HVP > /dev/null || { echo "  FAIL DATOS.HVP no se ensambla"; exit 1; }
SOLO=${SOLO:-}
mkdir -p $B/gen
parte() { [ -z "$SOLO" ] || [ "$SOLO" = "$1" ]; }
empieza() {     # el disquete de la parte $1: el que dejó la anterior
  if [ -n "$SOLO" ] && [ "$1" -gt 1 ]; then cp $B/gen/datos_p$(( $1 - 1 )).img $T/f.img; mcopy -o -i $T/f.img $HVP ::HISPADOS/PROGS/DATOS.HVP; fi
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
acaba() { cp $T/f.img $B/gen/datos_p$1.img; }

# lo que hay en un .BDD, en texto: comprueba el tamaño y la suma de la
# cabecera y escribe una línea por tabla, por campo y por registro
cat > $T/vuelca.py <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
firma, ver, tam, suma, ntab, act = struct.unpack_from('<4sIIIII', d, 0)
s = 0
for b in d[32:]:
    s = (s + b) & 0xFFFFFFFF
    s = ((s << 1) | (s >> 31)) & 0xFFFFFFFF
print('BASE firma=%s version=%d tablas=%d tamano=%s suma=%s' % (firma.decode(), ver, ntab,
      'bien' if tam == len(d) else 'MAL', 'bien' if s == suma else 'MAL'))
p = 32
for t in range(ntab):
    ttam, nreg, ncam, ordc, ordd, vista = struct.unpack_from('<IIBBBB', d, p)
    nom = d[p+16:p+40].split(b'\0')[0].decode('cp437')
    print('TABLA %s registros=%d campos=%d orden=%d%s' % (nom, nreg, ncam, ordc, ' al reves' if ordd else ''))
    campos = []
    q = p + 40
    for c in range(ncam):
        cn = d[q:q+20].split(b'\0')[0].decode('cp437')
        tipo, dec, ancho, reft, refc = struct.unpack_from('<BBHBB', d, q + 20)
        campos.append((tipo, dec))
        print('CAMPO %s tipo=%d decimales=%d ancho=%d viene=%d.%d' % (cn, tipo, dec, ancho, reft, refc))
        q += 32
    for r in range(nreg):
        rl = struct.unpack_from('<H', d, q)[0]
        v, vals = q + 2, []
        for tipo, dec in campos:
            if tipo in (0, 4):
                n = struct.unpack_from('<H', d, v)[0]
                vals.append(d[v+2:v+2+n].decode('cp437').replace('\n', '\\n')); v += 2 + n
            elif tipo == 1:
                n = struct.unpack_from('<i', d, v)[0]; v += 4
                vals.append('' if n == -0x80000000 else ('%.*f' % (dec, n / 10 ** dec)).replace('.', ','))
            elif tipo == 2:
                n = struct.unpack_from('<I', d, v)[0]; v += 4
                vals.append('' if n == 0 else '%02d/%02d/%04d' % (n % 100, n // 100 % 100, n // 10000))
            else:
                vals.append('si' if d[v] else 'no'); v += 1
        assert v == q + rl, 'un registro no mide lo que dice'
        print('REG ' + '|'.join(vals))
        q += rl
    assert q == p + ttam, 'una tabla no mide lo que dice'
    p += ttam
assert p == len(d), 'sobra o falta archivo'
PY
# saca del disquete la base $1 (ruta en el disquete) y la vuelca en $T/bdd.txt
vuelca() {
  rm -f $T/b.bdd; mcopy -n -i $T/f.img "::$1" $T/b.bdd 2>/dev/null
  python3 $T/vuelca.py $T/b.bdd > $T/bdd.txt 2>&1 || echo "VOLCADO ROTO" >> $T/bdd.txt
}
cb() { if grep -qxE -- "$1" $T/bdd.txt; then ok "archivo: $1"; else ko "archivo: falta «$1»"; fi; }
ncb() { if grep -qxE -- "$1" $T/bdd.txt; then ko "archivo: sobra «$1»"; else ok "archivo: no está $1"; fi; }
esta() { if mdir -i $T/f.img "::$1" > /dev/null 2>&1; then ok "en el disco: $1"; else ko "falta en el disco: $1"; fi; }
noesta() { if mdir -i $T/f.img "::$1" > /dev/null 2>&1; then ko "sobra en el disco: $1"; else ok "no queda en el disco: $1"; fi; }
D=HISPADOS/PROGS
# un socio: nombre, apellidos, población, fecha de alta y, si hay un quinto dato, la cuota pagada
# una orden de un menú: la x de su título, y la x y la y de la orden
menu() { clic $1 66; sleep 0.8; clic $2 $3; sleep ${4:-1.3}; }
socio() { tecla insert; sleep 1.2; escribe "$1"; tecla tab; escribe "$2"; tecla tab tab tab tab; escribe "$3"; tecla tab; escribe "$4"
          [ -n "${5:-}" ] && { tecla tab; tecla spc; }; tecla ret; sleep 1.5; }

prepara $HVP
mdel -i $T/f.img ::$D/TIPOS.DAT ::$D/TIPOS12.DAT ::$D/TIPOS24.DAT ::$D/TIPOS32.DAT 2>/dev/null  # (Datos no los usa: sitio para las bases)

if parte 1; then
echo "== Datos 1: una tabla de socios; registros, validar, ordenar, la ficha =="
empieza 1
{ arranca
  shot v0
  ir 204 91; shot v1                      # el ratón sobre «Filtrar»: abajo, lo que hace
  clic 578 91; sleep 1.5; shot t0         # «Tablas...»: como no hay ninguna, la nueva
  tecla ret; sleep 2; aparca; shot t1     # la plantilla «Socios»
  socio "Marta" "Vidal" "Albinyana" "1/2/2020" x
  socio "berta" "Soler" "Valls" "15/6/2019"
  tecla insert; sleep 1.2; shot r0; tecla esc; sleep 1
  socio "Carles" "Roig" "Albinyana" "3/3/2021" x
  # una fecha que no existe: lo dice, deja el cursor en ella y sigue abierto
  tecla insert; sleep 1.2; escribe "Zoe"; tecla tab tab tab tab tab tab; escribe "31/2/2020"; tecla ret; sleep 1.5; shot e1
  tecla ret; sleep 1; tecla backspace backspace backspace backspace backspace backspace backspace backspace backspace
  escribe "28/2/2020"; shot e2; tecla ret; sleep 1.5; aparca; shot r1
  clic 60 116; sleep 1.2; aparca; shot o1          # clic en el encabezado «Nombre»: de menor a mayor
  clic 60 116; sleep 1.2; aparca; shot o2          # otra vez: al revés
  tecla right right right right right right; sleep 0.5
  clic 230 91; sleep 1.2; aparca; shot o3          # el botón «Ordenar», por la fecha de alta
  tecla f4; sleep 1.2; shot f1                     # la ficha
  tecla pgup; sleep 1; shot f2                     # el registro anterior
  tecla up; sleep 0.5                              # el campo «Población»
  tecla ret; sleep 1.2; shot c0                    # cambiarlo: el cursor ya está en ese campo
  tecla end; escribe " Nou"; tecla ret; sleep 1.5; shot c1
  tecla f4; sleep 1; tecla ctrl-home; sleep 0.5
  tecla delete; sleep 1.2; shot d0; tecla ret; sleep 1.5; aparca; shot d1     # borrar el primero
  sal; } | run 240 -vga std
chk $T/v0.ppm titulo 10 32 activa "Datos - DATOS.BDD"
chk $T/v0.ppm busca 100 150 700 200 0 8 "Esta base todavía no tiene tablas."
chk $T/v0.ppm busca 12 566 400 590 0 6 "Sin tablas"
chk $T/v1.ppm busca 12 566 700 590 0 6 "Filtrar: ver solo los registros que cumplen una condición"
chk $T/t0.ppm busca 200 180 600 210 0 8 "¿De qué va a ser la tabla?"
chk $T/t0.ppm busca 220 220 500 320 0 9 "Biblioteca"
chk $T/t1.ppm busca 150 106 330 126 0 6 "Apellidos"            # los campos de la plantilla
chk $T/t1.ppm busca 500 106 700 126 0 6 "Dirección"
chk $T/t1.ppm busca 12 566 400 590 0 6 "0 registros"
chk $T/r0.ppm titulo 142 152 activa "Registro nuevo"
chk $T/r0.ppm busca 140 150 600 500 0 8 "Fecha de alta:"        # el diálogo: un cuadro por campo
chk $T/r0.ppm busca 140 150 600 500 0 8 "Cuota pagada:"
chk $T/e1.ppm busca 60 270 760 310 0 8 "«Fecha de alta» tiene que ser una fecha que exista, así: 16/11/2026."
chk $T/e2.ppm busca 200 150 600 500 0 9 "Zoe"                   # lo escrito sigue ahí
chk $T/e2.ppm busca 200 150 600 500 0 9 "28/2/202"
chk $T/r1.ppm busca 12 126 148 146 0 9 "Marta"                  # como se escribieron
chk $T/r1.ppm busca 148 144 324 164 0 9 "Soler"
chk $T/r1.ppm busca 12 566 400 590 0 6 "4 registros"
chk $T/o1.ppm busca 12 126 148 146 0 9 "berta"                  # sin mirar las mayúsculas
chk $T/o1.ppm busca 12 144 148 164 0 9 "Carles"
chk $T/o1.ppm busca 12 162 148 182 0 9 "Marta"
chk $T/o1.ppm busca 12 566 600 590 0 6 "4 registros   Orden: Nombre"
chk $T/o2.ppm busca 12 126 148 146 0 10 "Zoe"                   # (el elegido, que sigue elegido)
chk $T/o2.ppm busca 12 180 148 200 0 9 "berta"
chk $T/o2.ppm busca 12 566 600 590 0 6 "Orden: Nombre (de mayor a menor)"
chk $T/o3.ppm busca 400 126 772 146 0 9 "15/06/2019"            # por fecha, no por letras
chk $T/o3.ppm busca 400 180 772 200 0 9 "03/03/2021"
chk $T/o3.ppm busca 12 566 600 590 0 6 "Orden: Fecha de alta"
chk $T/f1.ppm busca 500 120 720 144 0 9 "Registro 3 de 4"
chk $T/f1.ppm busca 100 155 400 180 0 9 "Nombre:"
chk $T/f1.ppm busca 200 155 500 180 0 9 "Zoe"
chk $T/f1.ppm busca 200 270 500 300 8 2 "28/02/2020"            # el campo elegido
chk $T/f2.ppm busca 500 120 720 144 0 9 "Registro 2 de 4"
chk $T/f2.ppm busca 200 155 500 180 0 9 "Marta"
chk $T/f2.ppm busca 200 290 500 320 0 9 "Sí"
chk $T/c0.ppm titulo 142 152 activa "Cambiar el registro"
chk $T/c1.ppm busca 200 250 500 280 8 2 "Albinyana Nou"
chk $T/d0.ppm busca 150 250 650 310 0 8 "¿Borrar este registro? No se puede deshacer."
chk $T/d1.ppm busca 12 566 600 590 0 6 "3 registros   Orden: Fecha de alta"
chk $T/d1.ppm nobusca 12 126 772 300 0 9 "berta"
muestra v0 datos_vacia; muestra t0 datos_tabla_nueva; muestra r0 datos_registro; muestra e1 datos_fecha_mala
muestra o3 datos_ordenada; muestra f1 datos_ficha; muestra d1 datos_socios
vuelca $D/DATOS.BDD
cb 'BASE firma=HBDD version=1 tablas=1 tamano=bien suma=bien'
cb 'TABLA Socios registros=3 campos=8 orden=6'
cb 'CAMPO Fecha de alta tipo=2 decimales=0 ancho=112 viene=0.0'
cb 'CAMPO Cuota pagada tipo=3 decimales=0 ancho=104 viene=0.0'
cb 'REG Marta\|Vidal\|\|\|\|Albinyana Nou\|01/02/2020\|si'
cb 'REG Carles\|Roig\|\|\|\|Albinyana\|03/03/2021\|si'
cb 'REG Zoe\|\|\|\|\|\|28/02/2020\|no'
ncb 'REG berta.*'
esta $D/DATOS.ANT                 # la copia de antes del último cambio
noesta "$D/DATOS.\$\$\$"          # y nada a medias
acaba 1
fi

if parte 2; then
echo "== Datos 2: otra tabla (inventario); números, resumen, filtrar, buscar, .CSV, imprimir =="
empieza 2
rm -f $T/prn.txt
art() { tecla insert; sleep 1.2; escribe "$1"; tecla tab; escribe "$2"; tecla tab; escribe "$3"; tecla tab; escribe "$4"; tecla ret; sleep 1.5; }
{ arranca
  shot i0                                          # lo de la parte 1 sigue ahí
  tecla f6; sleep 1.2; shot i1                     # las tablas de la base
  clic 527 242; sleep 1.5                          # «Nueva tabla...»
  tecla down down ret; sleep 2                     # la plantilla «Inventario»
  art "Tornillos" "200" "0,05" "Cajon 1"
  art "Taladro" "2" "1.249,5" "Armario"
  art "Lija" "12" "1,2" "Cajon 2"
  tecla insert; sleep 1.2; escribe "Cola"; tecla tab; escribe "dos"; tecla ret; sleep 1.5; shot n1   # no es un número
  tecla ret; sleep 0.8; tecla esc; sleep 1
  tecla right right; sleep 0.8; aparca; shot i2    # en «Precio»: abajo, su suma
  tecla f5; sleep 1.2; shot s1; tecla ret; sleep 1 # el resumen
  tecla left; sleep 0.5
  tecla ctrl-f; sleep 1.2; escribe "5"; tecla tab tab down down; shot q1; tecla ret; sleep 1.5; aparca; shot q2   # Cantidad es mayor que 5
  clic 376 91; sleep 1.5; shot x0; tecla ret; sleep 2.5; shot x1; tecla ret; sleep 1    # exportar lo que se ve
  tecla ctrl-f; sleep 1.2; clic 493 425; sleep 1.5; aparca; shot q3                     # «Quitar el filtro»
  tecla ctrl-b; sleep 1.2; escribe "armario"; tecla ret; sleep 1.5; aparca; shot b1     # buscar
  tecla ctrl-p; sleep 5; shot p1; tecla ret; sleep 1                                    # imprimir el listado
  tecla f6; sleep 1.2; shot l1; tecla up ret; sleep 1.5; shot l2                        # a la otra tabla
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
chk $T/i0.ppm busca 12 566 600 590 0 6 "3 registros   Orden: Fecha de alta"
chk $T/i0.ppm busca 12 126 148 146 0 10 "Marta"
chk $T/i1.ppm busca 150 150 600 420 9 12 "Socios (3)"
chk $T/n1.ppm busca 60 250 760 320 0 8 "«Cantidad» tiene que ser un número, como 12 o 3,50"
chk $T/i2.ppm busca 12 126 252 146 0 9 "Tornillos"
chk $T/i2.ppm busca 252 126 332 146 0 9 "200"
chk $T/i2.ppm busca 332 126 428 146 0 9 "0,05"
chk $T/i2.ppm busca 332 144 428 164 0 9 "1.249,50"              # con su punto de millares y sus dos decimales
chk $T/i2.ppm busca 332 162 428 182 0 10 "1,20"                 # (el elegido)
chk $T/i2.ppm busca 12 566 400 590 0 6 "3 registros"
chk $T/i2.ppm busca 400 566 790 590 0 6 "Suma de Precio: 1.250,75"
chk $T/s1.ppm busca 100 200 700 420 0 8 "Campo «Precio» (Número), en 3 registros a la vista:"
chk $T/s1.ppm busca 100 200 700 420 0 8 "Suma:        1.250,75"
chk $T/s1.ppm busca 100 200 700 420 0 8 "Media:       416,92"
chk $T/s1.ppm busca 100 200 700 420 0 8 "Mayor:       1.249,50"
chk $T/s1.ppm busca 100 200 700 420 0 8 "Menor:       0,05"
chk $T/q1.ppm busca 150 150 650 450 9 12 "es mayor que"
chk $T/q2.ppm busca 12 566 790 590 0 6 "2 de 3 registros   Filtro: Cantidad es mayor que «5»"
chk $T/q2.ppm busca 12 126 252 164 8 2 "Lija"
chk $T/q2.ppm nobusca 12 126 252 200 0 9 "Taladro"
chk $T/x1.ppm busca 100 250 700 310 0 8 "La tabla se ha exportado. El archivo se abre con Cuentas."
chk $T/q3.ppm busca 12 566 400 590 0 6 "3 registros"
chk $T/q3.ppm busca 12 126 252 200 0 9 "Taladro"
chk $T/b1.ppm busca 428 144 588 164 0 10 "Armario"               # lo encontrado, elegido
chk $T/p1.ppm busca 150 250 650 310 0 8 "El listado se ha enviado a la impresora."
chk $T/l1.ppm busca 150 150 600 420 9 12 "Inventario (3)"
chk $T/l2.ppm busca 150 106 330 126 0 6 "Apellidos"
muestra i2 datos_inventario; muestra s1 datos_resumen; muestra q1 datos_filtro; muestra q2 datos_filtrada; muestra l1 datos_tablas
mtype -i $T/f.img ::INVENTAR.CSV 2>/dev/null | iconv -f CP437 -t UTF-8 > $T/inv.csv
if [ "$(cat $T/inv.csv)" = "$(printf 'Artículo;Cantidad;Precio;Lugar\r\nTornillos;200;0,05;Cajon 1\r\nLija;12;1,20;Cajon 2\r\n')" ]; then
  ok "INVENTAR.CSV: lo filtrado, con números que Cuentas entiende"
else
  ko "INVENTAR.CSV no es lo esperado"; cat -A $T/inv.csv
fi
tr -d '\r\f' < $T/prn.txt 2>/dev/null | iconv -f CP437 -t UTF-8 > $T/prn.u
ci() { if grep -qE -- "$1" $T/prn.u; then ok "impresora: $1"; else ko "impresora: falta «$1»"; fi; }
ci '^Inventario +[0-9]{2}/[0-9]{2}/[0-9]{4}   Página 1$'
ci '^Artículo   Cantidad    Precio  Lugar$'
ci '^---------  --------  --------  -------$'
ci '^Tornillos       200      0,05  Cajon 1$'
ci '^Taladro           2  1.249,50  Armario$'
ci '^3 registros$'
if tail -c 1 $T/prn.txt 2>/dev/null | od -An -tu1 | grep -qw 12; then ok "impresora: salto de página al final"; else ko "impresora: falta el salto de página"; fi
vuelca $D/DATOS.BDD
cb 'BASE firma=HBDD version=1 tablas=2 tamano=bien suma=bien'
cb 'TABLA Socios registros=3 campos=8 orden=6'
cb 'TABLA Inventario registros=3 campos=4 orden=255'
cb 'CAMPO Precio tipo=1 decimales=2 ancho=96 viene=0.0'
cb 'REG Taladro\|2\|1249,50\|Armario'
cb 'REG Lija\|12\|1,20\|Cajon 2'
acaba 2
fi

if parte 3; then
echo "== Datos 3: diseñar la tabla: campos, texto largo, tabla en blanco, un campo de otra tabla =="
empieza 3
{ arranca
  shot g0
  menu 228 280 177; escribe "Notas"; clic 312 305; shot g1; tecla ret; sleep 1.5      # Tabla > Añadir un campo: texto largo
  tecla ret; sleep 1.2; shot g2                    # cambiar el registro: el cursor, en «Escribir...»
  tecla spc; sleep 1.2; escribe "linea uno"; tecla ret; escribe "dos"; shot g3
  clic 347 434; sleep 1; clic 365 378; sleep 1.5; aparca; shot g4    # Aceptar el texto y el registro
  tecla left left left left right; sleep 0.5       # el campo «Cantidad»: ahora, con un decimal
  menu 228 280 197; clic 330 337; tecla end backspace; escribe "1"; shot h1; tecla ret; sleep 1.2; shot h2
  tecla ret; sleep 1.5; aparca; shot h3
  menu 228 280 257 1.5; aparca; shot h4            # Mover el campo hacia detrás
  tecla right; sleep 0.5; menu 228 280 217; shot h5; tecla ret; sleep 1.5; aparca; shot h6   # Quitar el campo «Lugar»
  ir 252 116; shot w0; mon "mouse_button 1" 0.3; ir 300 118; ir 332 116; mon "mouse_button 0" 1.5; aparca; shot w1   # ensanchar «Artículo»
  menu 228 280 129; shot n0; escribe "Almacen"; tecla ret; sleep 1.5; shot n1         # Cambiar el nombre de la tabla
  menu 228 280 109; tecla down down down down down tab; escribe "Prestamos"; tecla ret; sleep 1.5   # Nueva tabla, en blanco
  escribe "Socio"; clic 556 370; sleep 1.2; shot k0; tecla down ret; sleep 1.2; shot k1; tecla ret; sleep 1.2; shot k2
  tecla ret; sleep 2; aparca; shot k3
  tecla insert; sleep 1.2; clic 633 290; sleep 1.2; shot k4; tecla down ret; sleep 1.2; shot k5; tecla ret; sleep 1.5; aparca; shot k6
  tecla f6; sleep 1.2; tecla up ret; sleep 1.5     # a «Almacen», para borrarla
  menu 228 280 149; shot z0; tecla ret; sleep 1.5; aparca; shot z1
  sal; } | run 300 -vga std
chk $T/g0.ppm busca 100 82 700 100 0 8 "Tabla: Inventario"
chk $T/g1.ppm titulo 180 135 activa "Campo"
chk $T/g1.ppm busca 190 150 620 470 0 8 "Texto largo (notas, varias líneas)"
chk $T/g2.ppm busca 140 190 660 420 0 8 "Notas:"
chk $T/g2.ppm busca 140 190 660 420 0 8 "Lugar:"
chk $T/g3.ppm titulo 150 135 activa "Texto largo"
chk $T/g3.ppm busca 150 160 650 420 0 9 "linea uno"
chk $T/g4.ppm busca 560 126 772 146 0 10 "linea uno dos"        # en la rejilla, en un renglón
chk $T/h1.ppm busca 190 150 620 470 0 9 "Cantidad"
chk $T/h2.ppm busca 100 250 700 320 0 8 "pasar al tipo nuevo se quedarán vacíos. ¿Seguir?"
chk $T/h3.ppm busca 252 126 332 146 0 10 "200,0"                # los valores, pasados al tipo nuevo
chk $T/h3.ppm busca 252 162 332 182 0 9 "12,0"
chk $T/h4.ppm busca 252 106 348 126 0 6 "Precio"                # los dos campos, cambiados de sitio
chk $T/h4.ppm busca 252 126 348 146 8 2 "0,05"
chk $T/h4.ppm busca 348 162 428 182 0 9 "12,0"
chk $T/h5.ppm busca 100 250 700 320 0 8 "¿Quitar este campo? Se borra lo que haya escrito en él"
chk $T/h6.ppm nobusca 12 106 772 126 0 6 "Lugar"
chk $T/h6.ppm busca 428 126 772 146 0 10 "linea uno dos"
chk $T/w1.ppm busca 332 106 428 126 0 6 "Precio"                # la columna de antes, 80 puntos más ancha
chk $T/w1.ppm nobusca 252 106 331 126 0 6 "Precio"
chk $T/n1.ppm busca 100 82 700 100 0 8 "Tabla: Almacen"
chk $T/k0.ppm busca 210 160 590 420 9 12 "(de ninguna: se escribe a mano)"
chk $T/k0.ppm busca 210 160 590 420 0 9 "Socios"
chk $T/k0.ppm busca 210 160 590 420 0 9 "Almacen"
chk $T/k1.ppm busca 210 160 590 420 0 9 "Fecha de alta"         # los campos de «Socios»
chk $T/k2.ppm busca 190 150 620 470 0 9 "Socios > Nombre"
chk $T/k3.ppm busca 100 82 700 100 0 8 "Tabla: Prestamos"
chk $T/k4.ppm busca 210 160 590 420 9 12 "Marta"                # los valores, sacados de la otra tabla
chk $T/k4.ppm busca 210 160 590 420 0 9 "Carles"
chk $T/k4.ppm busca 210 160 590 420 0 9 "Zoe"
chk $T/k5.ppm busca 140 240 660 360 0 9 "arles"                 # el elegido, en su cuadro
chk $T/k6.ppm busca 12 566 400 590 0 6 "1 registro"
chk $T/z0.ppm busca 100 250 700 320 0 8 "¿Borrar la tabla entera, con todos sus registros?"
chk $T/z1.ppm busca 100 82 700 100 0 8 "Tabla: Prestamos"
muestra g1 datos_campo; muestra g3 datos_texto_largo; muestra w1 datos_almacen; muestra k1 datos_viene_de; muestra k4 datos_elegir_valor
vuelca $D/DATOS.BDD
cb 'BASE firma=HBDD version=1 tablas=2 tamano=bien suma=bien'
cb 'TABLA Socios registros=3 campos=8 orden=6'
cb 'TABLA Prestamos registros=1 campos=1 orden=255'
cb 'CAMPO Socio tipo=0 decimales=0 ancho=160 viene=1.0'
cb 'REG Carles'
ncb 'TABLA Almacen.*'
# la tabla borrada, como quedó justo antes (en la copia anterior)
vuelca $D/DATOS.ANT
cb 'TABLA Almacen registros=3 campos=4 orden=255'
cb 'CAMPO Artículo tipo=0 decimales=0 ancho=320 viene=0.0'
cb 'CAMPO Precio tipo=1 decimales=2 ancho=96 viene=0.0'
cb 'CAMPO Cantidad tipo=1 decimales=1 ancho=80 viene=0.0'
cb 'CAMPO Notas tipo=4 decimales=0 ancho=200 viene=0.0'
cb 'REG Tornillos\|0,05\|200,0\|linea uno\\ndos'
cb 'REG Lija\|1,20\|12,0\|'
cb 'CAMPO Socio tipo=0 decimales=0 ancho=160 viene=1.0'
acaba 3
fi

if parte 4; then
echo "== Datos 4: base nueva, importar .CSV, etiquetas, 3.000 registros, copia y abrir =="
empieza 4
# tres .CSV en la raíz del disquete: uno con de todo (para que adivine los
# tipos), uno de 36 columnas (solo caben 32) y uno de 3.000 filas
printf 'Nombre;Apellidos;Alta;Cuota;Paga;CP;Notas\r\nAna;Soler Pi;01/02/2020;12,50;sí;08001;"Calle; con ""comillas"""\r\nPere;Vidal;15/06/2019;30;no;43700;\r\nNúria;Roig;;7,5;sí;43700;x\r\n' | iconv -f UTF-8 -t CP437 > $T/SOCIOS.CSV
python3 - > $T/ANCHA.CSV <<'PY'
print(';'.join('C%d' % i for i in range(1, 37)), end='\r\n')
print(';'.join('%d' % i for i in range(1, 37)), end='\r\n')
PY
python3 - > $T/MUCHOS.CSV <<'PY'
print('N;S;Valor', end='\r\n')
for i in range(1, 3001):
    print('%d;S %d;%d' % (i, i, 3001 - i), end='\r\n')
PY
mcopy -o -i $T/f.img $T/SOCIOS.CSV $T/ANCHA.CSV $T/MUCHOS.CSV ::
rm -f $T/prn.txt
{ arranca
  menu 52 100 89; escribe "club"; tecla ret; sleep 2.5; shot b0; tecla esc; sleep 1; shot b1   # Archivo > Nueva base
  menu 52 100 157; escribe "socios.csv"; tecla ret; sleep 3; shot m1; tecla ret; sleep 1; aparca; shot m2   # Importar
  menu 52 100 225; shot q0; tecla ret; sleep 5; shot q1; tecla ret; sleep 1                    # Imprimir etiquetas
  menu 52 100 157; escribe "ancha.csv"; tecla ret; sleep 3; shot a1; tecla ret; sleep 1
  tecla ret; sleep 1.5; shot a2; tecla esc; sleep 1                                            # el registro: 32 campos en dos columnas
  menu 52 100 157; escribe "muchos.csv"; tecla ret; sleep 6; shot g1; tecla ret; sleep 1
  tecla right right; sleep 1; tecla ctrl-end; sleep 1; aparca; shot g2
  clic 40 116; sleep 1.5; clic 40 116; sleep 1.5; aparca; shot g3                               # por N, de mayor a menor
  tecla right; sleep 0.5; tecla ctrl-f; sleep 1.2; escribe "s 29"; tecla ret; sleep 1.5; aparca; shot g4    # S contiene «s 29»
  tecla ctrl-p; sleep 14; shot g5; tecla ret; sleep 1                                          # el listado: 111 filas, tres páginas
  menu 52 100 129; escribe "copia"; tecla ret; sleep 3; shot c1; tecla ret; sleep 1            # Guardar una copia como
  menu 52 100 109; escribe "copia"; tecla ret; sleep 3; aparca; shot c2                        # Abrir base
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
chk $T/b0.ppm titulo 210 150 activa "Nueva tabla"               # la base nueva pide su primera tabla
chk $T/b1.ppm titulo 10 32 activa "Datos - CLUB.BDD"
chk $T/b1.ppm busca 100 150 700 200 0 8 "Esta base todavía no tiene tablas."
chk $T/m1.ppm busca 100 250 700 320 0 8 "3 registros importados."
chk $T/m2.ppm busca 100 82 700 100 0 8 "Tabla: socios"
chk $T/m2.ppm busca 172 126 332 146 8 2 "Soler Pi"
chk $T/m2.ppm busca 332 126 428 146 8 2 "01/02/2020"            # una fecha
chk $T/m2.ppm busca 428 144 516 164 0 9 "30,00"                 # un número, con los decimales de la columna
chk $T/m2.ppm busca 516 126 580 146 8 2 "Sí"                    # sí o no
chk $T/m2.ppm busca 580 126 740 146 8 2 "08001"                 # el código postal, texto: no pierde el cero
chk $T/m2.ppm busca 12 162 172 182 0 9 "Núria"
chk $T/q0.ppm titulo 180 135 activa "Imprimir etiquetas"
chk $T/q0.ppm busca 190 150 620 470 0 8 "4 Cuota  5 Paga  6 CP  7 Notas"
chk $T/q1.ppm busca 100 250 700 320 0 8 "Las etiquetas se han enviado a la impresora."
chk $T/a1.ppm busca 100 250 700 320 0 8 "1 registro importado."
chk $T/a2.ppm busca 16 100 400 560 0 8 "C16:"                   # dos columnas de dieciséis
chk $T/a2.ppm busca 400 60 790 560 0 8 "C17:"
chk $T/a2.ppm busca 400 100 790 560 0 8 "C32:"
chk $T/a2.ppm nobusca 16 100 790 560 0 8 "C33:"
chk $T/g1.ppm busca 100 250 700 320 0 8 "3000 registros importados."
chk $T/g2.ppm busca 12 566 400 590 0 6 "3000 registros"
chk $T/g2.ppm busca 400 566 790 590 0 6 "Suma de Valor: 4.501.500"
chk $T/g2.ppm busca 100 120 260 560 8 2 "S 3000"                # el último, a la vista y elegido
chk $T/g3.ppm busca 100 126 260 146 8 2 "S 3000"                # el orden es por número: 3000, 2999...
chk $T/g3.ppm busca 100 144 260 164 0 9 "S 2999"
chk $T/g3.ppm busca 12 566 600 590 0 6 "3000 registros   Orden: N (de mayor a menor)"
chk $T/g4.ppm busca 12 566 790 590 0 6 "111 de 3000 registros   Filtro: S contiene «s 29»"
chk $T/g4.ppm busca 100 126 260 146 0 10 "S 2999"
chk $T/g5.ppm busca 100 250 700 320 0 8 "El listado se ha enviado a la impresora."
chk $T/c1.ppm busca 100 250 700 320 0 8 "La copia se ha guardado."
chk $T/c2.ppm titulo 10 32 activa "Datos - COPIA.BDD"
chk $T/c2.ppm busca 12 566 400 590 0 6 "3000 registros"
muestra m2 datos_importada; muestra q0 datos_etiquetas; muestra a2 datos_registro_32; muestra g4 datos_3000
tr -d '\r\f' < $T/prn.txt 2>/dev/null | iconv -f CP437 -t UTF-8 > $T/prn.u
ci() { if grep -qE -- "$1" $T/prn.u; then ok "impresora: $1"; else ko "impresora: falta «$1»"; fi; }
ci '^Ana Soler Pi {28}Pere Vidal$'
ci '^01/02/2020 {30}15/06/2019$'
ci '^12,50 {35}30,00$'
ci '^Núria Roig$'
ci '^muchos +[0-9]{2}/[0-9]{2}/[0-9]{4}   Página 1$'
ci '^Filtro: S contiene «s 29»$'
ci '^   N  S       Valor$'
ci '^2999  S 2999      2$'
ci '^muchos +[0-9]{2}/[0-9]{2}/[0-9]{4}   Página 3$'
ci '^111 registros$'
if [ "$(tr -cd '\f' < $T/prn.txt | wc -c)" = 4 ]; then ok "impresora: una página de etiquetas y tres de listado"; else ko "impresora: no salen cuatro páginas"; fi
vuelca CLUB.BDD
cb 'BASE firma=HBDD version=1 tablas=3 tamano=bien suma=bien'
cb 'TABLA socios registros=3 campos=7 orden=255'
cb 'CAMPO Alta tipo=2 decimales=0 ancho=96 viene=0.0'
cb 'CAMPO Cuota tipo=1 decimales=2 ancho=88 viene=0.0'
cb 'CAMPO Paga tipo=3 decimales=0 ancho=64 viene=0.0'
cb 'CAMPO CP tipo=0 decimales=0 ancho=160 viene=0.0'
cb 'REG Ana\|Soler Pi\|01/02/2020\|12,50\|si\|08001\|Calle; con "comillas"'
cb 'REG Núria\|Roig\|\|7,50\|si\|43700\|x'
cb 'TABLA ancha registros=1 campos=32 orden=255'
cb 'CAMPO C32 tipo=1 decimales=0 ancho=88 viene=0.0'
ncb 'CAMPO C33 .*'
cb 'TABLA muchos registros=3000 campos=3 orden=0 al reves'
cb 'REG 3000\|S 3000\|1'
esta COPIA.BDD
if [ "$(mtype -i $T/f.img ::$D/DATOS.INI 2>/dev/null)" = 'A:\COPIA.BDD' ]; then ok "DATOS.INI apunta la última base abierta"; else ko "DATOS.INI no apunta la última base"; fi
acaba 4
fi

if parte 5; then
echo "== Datos 5: un corte de luz a medio guardar; sin impresora; acerca de =="
empieza 5
# la base, a medias (la mitad del archivo); el temporal, basura; y la copia
# anterior, entera: al abrir se recupera la copia y se dice
mcopy -n -i $T/f.img ::COPIA.BDD $T/buena.bdd
head -c $(( $(stat -c %s $T/buena.bdd) / 2 )) $T/buena.bdd > $T/media.bdd
head -c 3000 /dev/urandom > $T/basura.bdd
mcopy -o -i $T/f.img $T/media.bdd ::COPIA.BDD; mcopy -o -i $T/f.img $T/basura.bdd '::COPIA.$$$'; mcopy -o -i $T/f.img $T/buena.bdd ::COPIA.ANT
{ arranca
  shot r0; tecla ret; sleep 1.5; shot r1
  tecla ctrl-p; sleep 3; shot p0; tecla ret; sleep 1                         # sin impresora: lo dice
  menu 312 360 109; shot i0; tecla ret; sleep 1                              # Herramientas > Datos de la base
  menu 380 420 89; shot h0; tecla ret; sleep 1                               # ? > Acerca de Datos
  tecla f6; sleep 1.2; tecla up ret; sleep 1.5                               # a la tabla «ancha», de 32 campos
  menu 228 280 177; shot x0; tecla ret; sleep 1                              # Tabla > Añadir un campo: no caben más
  menu 124 200 129; tecla ret; sleep 2; aparca; shot x1                      # Edición > Duplicar el registro
  tecla ctrl-b; sleep 1.2; escribe "zzz"; tecla ret; sleep 1.5; shot x2; tecla ret; sleep 1   # buscar lo que no está
  tecla f6; sleep 1.2; tecla up ret; sleep 1.5                               # a «socios»: los que pagan
  tecla right right right right; tecla ctrl-f; sleep 1.2; escribe "si"; tecla tab tab down ret; sleep 1.5; aparca; shot x3
  sal; } | run 200 -vga std
chk $T/r0.ppm busca 100 250 700 320 0 8 "Se ha recuperado la última copia entera."
chk $T/r1.ppm titulo 10 32 activa "Datos - COPIA.BDD"                        # la última que se abrió
chk $T/r1.ppm busca 12 566 400 590 0 6 "3000 registros"
chk $T/p0.ppm busca 100 250 700 320 0 8 "No se ha podido imprimir."
chk $T/i0.ppm busca 100 150 700 450 0 8 "Archivo: A:\\COPIA.BDD"
chk $T/i0.ppm busca 100 150 700 450 0 8 "muchos: 3000 registros, 3 campos"
chk $T/h0.ppm busca 100 150 700 450 0 8 "Copyright (C) 2026 Carlos Sanz y contribuidores."
chk $T/h0.ppm busca 100 150 700 450 0 8 "GPL-2.0 o posterior."
chk $T/x0.ppm busca 100 250 700 320 0 8 "Una tabla no puede tener más de 32 campos."
chk $T/x1.ppm busca 12 566 400 590 0 6 "2 registros"
chk $T/x2.ppm busca 100 250 700 320 0 8 "No se ha encontrado ese texto."
chk $T/x3.ppm busca 12 566 790 590 0 6 "2 de 3 registros   Filtro: Paga es igual a «si»"
chk $T/x3.ppm nobusca 12 126 172 200 0 9 "Pere"
muestra r0 datos_recuperada; muestra i0 datos_base; muestra h0 datos_acerca
vuelca COPIA.BDD                 # y el archivo vuelve a estar entero
cb 'TABLA ancha registros=2 campos=32 orden=255'
cb 'BASE firma=HBDD version=1 tablas=3 tamano=bien suma=bien'
cb 'TABLA muchos registros=3000 campos=3 orden=0 al reves'
acaba 5
fi

if [ -n "${VERBOSE:-}" ]; then cat $T/bdd.txt 2>/dev/null; fi
if [ -n "${CAPTURAS:-}" ]; then for f in $T/*.ppm; do convert $f $B/dx_$(basename $f .ppm).png; done; fi   # todas, a build/dx_*.png
rm -rf $T
exit $fail
