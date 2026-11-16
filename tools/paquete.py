#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  Hispa-DOS - tools/paquete.py: construir un paquete .PAQ para ACTUALIZAR
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  Empaqueta una carpeta (con la MISMA estructura que tendra el sistema en el
#  disco: KERNEL.SYS, COMMAND.COM, CONFIG.SYS, AUTOEXEC.BAT en la raiz y
#  HISPADOS\... con sus subcarpetas) en un unico fichero .PAQ que la orden
#  ACTUALIZAR baja por HTTP, desempaqueta y aplica en el sitio.
#
#  Uso:
#      python3 -I tools/paquete.py <carpeta_origen> <salida.paq>
#
#  SIN compresion en esta primera version.
#
#  FORMATO DEL .PAQ (todos los enteros en little-endian):
#    Cabecera (12 octetos):
#       0  magia      4 octetos  = "PAQ1"
#       4  nfich      uint32     numero de ficheros
#       8  suma       uint32     suma de control: suma de TODOS los octetos de
#                                datos de todos los ficheros, modulo 2^32
#    Por cada fichero, en orden:
#       uint16   long_nombre     longitud del nombre (1..80)
#       bytes    nombre          ruta relativa, separador '\' (sin cero final)
#       uint32   long_datos      longitud del contenido
#       bytes    datos           el contenido del fichero
#    (nada mas al final; ACTUALIZAR comprueba la magia, el numero de ficheros
#    y que la suma de control cuadra antes de aplicar nada).
# =============================================================================

import os
import struct
import sys

MAGIA = b"PAQ1"
NOMBRE_MAX = 80


def recoger(carpeta):
    """Devuelve [(ruta_relativa_con_backslash, ruta_en_disco), ...] ordenada."""
    ficheros = []
    carpeta = os.path.abspath(carpeta)
    for raiz, dirs, nombres in os.walk(carpeta):
        dirs.sort()
        for n in sorted(nombres):
            completo = os.path.join(raiz, n)
            if not os.path.isfile(completo):
                continue
            rel = os.path.relpath(completo, carpeta)
            rel = rel.replace(os.sep, "\\").replace("/", "\\")
            ficheros.append((rel, completo))
    return ficheros


def construir(carpeta, salida):
    ficheros = recoger(carpeta)
    if not ficheros:
        sys.stderr.write("paquete: la carpeta '%s' no tiene ficheros\n" % carpeta)
        return 1

    cuerpo = bytearray()
    suma = 0
    for rel, completo in ficheros:
        nombre = rel.encode("cp437", "replace")
        if not (1 <= len(nombre) <= NOMBRE_MAX):
            sys.stderr.write(
                "paquete: nombre demasiado largo o vacio: %r (max %d)\n"
                % (rel, NOMBRE_MAX)
            )
            return 1
        with open(completo, "rb") as f:
            datos = f.read()
        suma = (suma + sum(datos)) & 0xFFFFFFFF
        cuerpo += struct.pack("<H", len(nombre))
        cuerpo += nombre
        cuerpo += struct.pack("<I", len(datos))
        cuerpo += datos

    cabecera = MAGIA + struct.pack("<II", len(ficheros), suma)
    with open(salida, "wb") as f:
        f.write(cabecera)
        f.write(cuerpo)

    total = len(cabecera) + len(cuerpo)
    sys.stderr.write(
        "paquete: %d ficheros, suma=0x%08X, %d octetos -> %s\n"
        % (len(ficheros), suma, total, salida)
    )
    for rel, _ in ficheros:
        sys.stderr.write("  %s\n" % rel)
    return 0


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("Uso: python3 -I tools/paquete.py <carpeta> <salida.paq>\n")
        return 2
    return construir(argv[1], argv[2])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
