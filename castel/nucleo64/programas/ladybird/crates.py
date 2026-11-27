# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/ladybird/crates.py - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
# sección 23): las crates de crates.io de terceros/rust-crates (los .crate,
# tal cual los da crates.io) en una carpeta que cargo usa como «directory
# source» (lo que hace «cargo vendor»), sin red: cada .crate se comprueba con
# el SHA-256 que dice el Cargo.lock de Ladybird y se abre en DESTINO/nombre-versión,
# con su .cargo-checksum.json.
# [etapa 10 g] Y las que cargo solo necesita para resolver el «workspace»
# entero de Ladybird (no se compilan): de ellas va solo su Cargo.toml, en
# CARPETA/resolver/nombre-versión.toml.
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
    solo = os.path.join(carpeta, "resolver", nombre + ".toml")
    if not os.path.exists(archivo) and os.path.exists(solo):
        # [etapa 10 g] una crate que cargo solo necesita para resolver (no se
        # compila: las de otras plataformas, las de WebRTC...): su Cargo.toml
        # tal cual, y vacíos los archivos que nombra (lib.rs, build.rs)
        d = os.path.join(destino, nombre)
        os.makedirs(os.path.join(d, "src"), exist_ok=True)
        datos = open(solo, "rb").read()
        open(os.path.join(d, "Cargo.toml"), "wb").write(datos)
        m = tomllib.loads(datos.decode())
        vacíos = ["src/lib.rs"]
        if isinstance(m.get("package", {}).get("build"), str):
            vacíos.append(m["package"]["build"])
        if isinstance(m.get("lib", {}).get("path"), str):
            vacíos.append(m["lib"]["path"])
        for v in vacíos:
            if ".." in v.split("/") or v.startswith("/"):
                sys.exit("crates.py: %s nombra algo raro: %s" % (solo, v))
            os.makedirs(os.path.dirname(os.path.join(d, v)), exist_ok=True)
            open(os.path.join(d, v), "w").write("fn main() {}\n" if v == m.get("package", {}).get("build") else "")
        json.dump({"files": {}, "package": p["checksum"]}, open(os.path.join(d, ".cargo-checksum.json"), "w"))
        puestos += 1
        continue
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
