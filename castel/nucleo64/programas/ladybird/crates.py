# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/ladybird/crates.py - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
# sección 23): las crates de crates.io de terceros/rust-crates (los .crate,
# tal cual los da crates.io) en una carpeta que cargo usa como «directory
# source» (lo que hace «cargo vendor»), sin red: cada .crate se comprueba con
# el SHA-256 que dice el Cargo.lock de Ladybird y se abre en DESTINO/nombre-versión,
# con su .cargo-checksum.json.
# Uso: python3 crates.py Cargo.lock CARPETA_DE_LOS_CRATE DESTINO
import hashlib
import json
import os
import sys
import tarfile
import tomllib

lock, carpeta, destino = sys.argv[1:4]
paquetes = tomllib.load(open(lock, "rb"))["package"]
puestos = 0
for p in paquetes:
    if p.get("source", "") != "registry+https://github.com/rust-lang/crates.io-index":
        continue
    nombre = "%s-%s" % (p["name"], p["version"])
    archivo = os.path.join(carpeta, nombre + ".crate")
    if not os.path.exists(archivo):
        continue                        # (las que no hacen falta para lo que se compila)
    datos = open(archivo, "rb").read()
    if hashlib.sha256(datos).hexdigest() != p["checksum"]:
        sys.exit("crates.py: %s no es el que dice el Cargo.lock" % archivo)
    with tarfile.open(archivo, "r:gz") as t:
        for m in t.getmembers():
            if not m.name.startswith(nombre + "/") or ".." in m.name.split("/") or m.issym() or m.islnk():
                sys.exit("crates.py: %s trae algo raro: %s" % (archivo, m.name))
        t.extractall(destino, filter="data")
    json.dump({"files": {}, "package": p["checksum"]}, open(os.path.join(destino, nombre, ".cargo-checksum.json"), "w"))
    puestos += 1
print("crates.py: %d crates en %s" % (puestos, destino))
