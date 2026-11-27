#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/navegador/talones.py - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
# sección 25): los «talones» de la conexión con el Hub. El Hub le manda a la
# interfaz unos 150 mensajes distintos (Services/Hub/HubClient.ipc), y el
# generador de IPC de Ladybird los pone todos como funciones virtuales puras
# de HubClientEndpoint. NAVEGADOR atiende los que le importan (conexion.h);
# los demás, aquí: uno vacío para cada uno, con los tipos tal cual (sin los
# nombres de los parámetros, que no se usan). Así, si Ladybird añade uno, el
# navegador sigue compilando.
# Uso: talones.py HubClient.ipc conexion.h SALIDA.h
import re
import sys


def partes(s):
    """los parámetros, separados por las comas de fuera de < > ( )"""
    r, n, a = [], 0, ""
    for c in s:
        if c in "<(":
            n += 1
        elif c in ">)":
            n -= 1
        if c == "," and n == 0:
            r.append(a.strip())
            a = ""
        else:
            a += c
    if a.strip():
        r.append(a.strip())
    return r


def main():
    ipc, propio, salida = sys.argv[1:4]
    texto = open(ipc, encoding="utf-8").read()
    i = texto.index("endpoint HubClient")
    cuerpo = texto[texto.index("{", i) + 1:]
    cuerpo = cuerpo[:cuerpo.index("\n}")]
    hechos = set(re.findall(r"virtual\s+void\s+(\w+)\s*\(", open(propio, encoding="utf-8").read()))
    líneas = ["// (hecho por talones.py desde Services/Hub/HubClient.ipc: no se toca a mano)"]
    for l in cuerpo.splitlines():
        l = l.split("//")[0].strip()
        m = re.match(r"(\w+)\s*\((.*)\)\s*=\|", l)
        if not m:
            if l and "=>" in l:
                sys.exit("talones.py: un mensaje con respuesta, que no se sabe atender: " + l)
            continue
        nombre, params = m.group(1), m.group(2)
        if nombre in hechos:
            continue
        tipos = []
        for p in partes(params):
            t = re.sub(r"\s*\b\w+\s*$", "", p).strip()
            tipos.append(t)
        líneas.append("virtual void %s(%s) override { }" % (nombre, ", ".join(tipos)))
    open(salida, "w", encoding="utf-8").write("\n".join(líneas) + "\n")


main()
