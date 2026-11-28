#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/imprimir/ipp_prueba.py - Hispa-64, etapa 11 a
# (docs/64/HISPA-64.md, sección 27): una impresora IPP de mentira para las
# pruebas de IMPRIMIR (pruebas.sh), en el anfitrión, sin nada instalado:
#   - contesta a Get-Printer-Attributes como una «sin controlador» (IPP
#     Everywhere): con PDF y PWG raster o, con --sinpdf, solo PWG raster
#     (300 y 600 ppp, srgb_8 y sgray_8);
#   - recibe Print-Job, guarda el documento (trabajo-N.pdf o .pwg) y lo
#     comprueba: el PDF, entero (la cabecera, la tabla xref con cada objeto
#     donde dice, los flujos de Flate que se descomprimen, las páginas, el
#     texto por su ToUnicode y las imágenes, con los puntos de un color); el
#     PWG raster, cada página (su cabecera, las filas descomprimidas, el
#     tamaño A4 a su resolución, lo que no es blanco);
#   - dice cada trabajo en una línea: «trabajo N: PDF bien: ...» o «... MAL: ...».
# Uso: ipp_prueba.py CARPETA [--sinpdf] [--color RRGGBB]   (dice «listo PUERTO»)
#      ipp_prueba.py --mira ARCHIVO [--color RRGGBB]       (comprueba un archivo)
import re
import socket
import struct
import sys
import threading
import zlib

COLOR = None


# ---- el PDF ----
def objetos(d):
    """los objetos «N G obj ... endobj»: {N: (posición, cuerpo)}"""
    r = {}
    for m in re.finditer(rb"(?<![0-9])(\d+) (\d+) obj\b", d):
        n = int(m.group(1))
        fin = d.find(b"endobj", m.end())
        if fin < 0:
            continue
        r[n] = (m.start(), d[m.end():fin])
    return r


def flujo(cuerpo, objs):
    """(diccionario, datos ya descomprimidos) de un objeto con «stream»"""
    k = cuerpo.find(b"stream")
    if k < 0:
        return cuerpo, None
    dic = cuerpo[:k]
    m = re.search(rb"/Length\s+(\d+)(\s+\d+\s+R)?", dic)
    if not m:
        raise ValueError("un flujo sin /Length")
    if m.group(2):
        largo = int(re.search(rb"\d+", objs[int(m.group(1))][1]).group(0))
    else:
        largo = int(m.group(1))
    i = k + 6
    if cuerpo[i:i + 2] == b"\r\n":
        i += 2
    elif cuerpo[i:i + 1] in (b"\n", b"\r"):
        i += 1
    datos = cuerpo[i:i + largo]
    if len(datos) != largo or b"endstream" not in cuerpo[i + largo:i + largo + 20]:
        raise ValueError("un flujo que no mide lo que dice su /Length")
    if b"/FlateDecode" in dic:
        datos = zlib.decompress(datos)
    return dic, datos


def cmap(datos):
    """lo de un ToUnicode: {código: texto} y cuántos octetos por código"""
    r = {}
    ancho = 2
    m = re.search(rb"begincodespacerange\s*<([0-9A-Fa-f]+)>", datos)
    if m:
        ancho = len(m.group(1)) // 2
    for b in re.finditer(rb"beginbfchar(.*?)endbfchar", datos, re.S):
        for c, u in re.findall(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", b.group(1)):
            r[int(c, 16)] = bytes.fromhex(u.decode()).decode("utf-16-be", "replace")
    for b in re.finditer(rb"beginbfrange(.*?)endbfrange", datos, re.S):
        for a, z, u in re.findall(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", b.group(1)):
            u0 = int(u, 16)
            for c in range(int(a, 16), int(z, 16) + 1):
                r[c] = chr(u0 + c - int(a, 16))
    return r, ancho


def mira_pdf(d):
    if not d.startswith(b"%PDF-1."):
        return "MAL: no empieza por %PDF-1."
    if b"%EOF" not in d[-64:]:
        return "MAL: no acaba en %EOF"
    m = re.findall(rb"startxref\s+(\d+)", d[-200:])
    if not m:
        return "MAL: sin startxref"
    x = int(m[-1])
    objs = objetos(d)
    if d[x:x + 4] == b"xref":
        # la tabla: cada objeto, donde dice
        p = x + 4
        filas = re.match(rb"\s+(\d+) (\d+)\s+", d[p:])
        ini, n = int(filas.group(1)), int(filas.group(2))
        p += filas.end()
        for i in range(n):
            e = d[p + 20 * i:p + 20 * i + 20]
            if e[17:18] == b"n":
                pos = int(e[:10])
                if not re.match(rb"%d \d+ obj" % (ini + i), d[pos:pos + 20]):
                    return "MAL: el objeto %d no está donde dice la tabla xref (%d)" % (ini + i, pos)
    elif not re.match(rb"\d+ \d+ obj", d[x:x + 20]):
        return "MAL: startxref no lleva a la tabla xref"
    páginas = 0
    textos = []
    mapas = {}
    imágenes = []
    contenidos = []
    try:
        for n, (pos, cuerpo) in objs.items():
            dic, datos = flujo(cuerpo, objs)
            if re.search(rb"/Type\s*/Page(?![s\w])", dic):
                páginas += 1
            if datos is None:
                continue
            if b"begincmap" in datos:
                mapas[n] = cmap(datos)
            elif b"/Subtype/Image" in dic.replace(b" ", b""):
                w = int(re.search(rb"/Width\s+(\d+)", dic).group(1))
                h = int(re.search(rb"/Height\s+(\d+)", dic).group(1))
                comp = 1 if b"/DeviceGray" in dic else 3
                if len(datos) != w * h * comp and b"/DCTDecode" not in dic:
                    return "MAL: una imagen de %dx%d con %d octetos" % (w, h, len(datos))
                imágenes.append((w, h, comp, datos))
            elif b"/Length1" not in dic and b"/FontFile" not in dic:
                contenidos.append(datos)
    except Exception as e:
        return "MAL: %s" % e
    if páginas == 0:
        return "MAL: sin páginas"
    # el texto: las cadenas de los contenidos, con todos los ToUnicode juntos
    todo = {}
    ancho = 2
    for m_, a in mapas.values():
        todo.update(m_)
        ancho = a
    for c in contenidos:
        for h in re.findall(rb"<([0-9A-Fa-f]+)>", c):
            b = bytes.fromhex(h.decode()) if len(h) % 2 == 0 else b""
            for i in range(0, len(b) - ancho + 1, ancho):
                textos.append(todo.get(int.from_bytes(b[i:i + ancho], "big"), ""))
        textos.append(" ")
    texto = re.sub(r"\s+", " ", "".join(textos)).strip()
    r = "PDF bien: %d página%s" % (páginas, "" if páginas == 1 else "s")
    if imágenes:
        r += ", %d imagen%s (%s)" % (len(imágenes), "" if len(imágenes) == 1 else "es", " ".join("%dx%d" % (w, h) for w, h, _, _ in imágenes[:6]))
        if COLOR is not None:
            c = bytes(COLOR)
            r += ", %d puntos del color %02X%02X%02X" % ((sum(datos.count(c) for w, h, comp, datos in imágenes if comp == 3),) + tuple(COLOR))
    r += ", texto «%s»" % texto[:300]
    return r


# ---- PWG raster ----
def mira_pwg(d):
    if d[:4] != b"RaS2":
        return "MAL: no empieza por RaS2"
    p = 4
    págs = []
    while p < len(d):
        h = d[p:p + 1796]
        if len(h) < 1796 or not h.startswith(b"PwgRaster\0"):
            return "MAL: la cabecera de la página %d" % (len(págs) + 1)
        u = lambda o: struct.unpack(">I", h[o:o + 4])[0]
        ppp, w, al, bpp, bpl, cs, tam = u(276), u(372), u(376), u(388), u(392), u(400), h[1732:1796].split(b"\0")[0].decode()
        if bpp not in (8, 24) or bpl != w * bpp // 8 or cs not in (18, 19):
            return "MAL: la página %d: %d bits por punto, %d por fila, color %d" % (len(págs) + 1, bpp, bpl, cs)
        if abs(w - round(210 / 25.4 * ppp)) > 1 or abs(al - round(297 / 25.4 * ppp)) > 1 or tam != "iso_a4_210x297mm":
            return "MAL: la página %d no es A4 a %d ppp: %dx%d (%s)" % (len(págs) + 1, ppp, w, al, tam)
        p += 1796
        bp = bpp // 8
        filas = 0
        oscuros = 0
        color = 0
        while filas < al:
            if p >= len(d):
                return "MAL: la página %d se acaba en la fila %d" % (len(págs) + 1, filas)
            rep = d[p] + 1
            p += 1
            f = bytearray()
            while len(f) < bpl:
                c = d[p]
                p += 1
                if c <= 127:
                    f += d[p:p + bp] * (c + 1)
                    p += bp
                else:
                    k = 257 - c
                    f += d[p:p + bp * k]
                    p += bp * k
            if len(f) != bpl:
                return "MAL: la fila %d de la página %d mide %d" % (filas, len(págs) + 1, len(f))
            no_blancos = sum(1 for i in range(0, bpl, bp) if f[i] < 128)
            oscuros += no_blancos * rep
            if COLOR is not None and bp == 3:
                color += f.count(bytes(COLOR)) * rep
            filas += rep
        if filas != al:
            return "MAL: la página %d tiene %d filas (no %d)" % (len(págs) + 1, filas, al)
        págs.append((w, al, ppp, "sRGB" if cs == 19 else "gris", oscuros, color))
    if not págs:
        return "MAL: sin páginas"
    r = "PWG bien: %d página%s, %dx%d a %d ppp, en %s; puntos oscuros: %s" % (
        len(págs), "" if len(págs) == 1 else "s", págs[0][0], págs[0][1], págs[0][2], págs[0][3], " ".join(str(x[4]) for x in págs))
    if COLOR is not None:
        r += "; del color %02X%02X%02X: %s" % (tuple(COLOR) + (" ".join(str(x[5]) for x in págs),))
    return r


# ---- IPP ----
def atributos(d, i):
    """lee los atributos desde i: [(grupo, nombre, etiqueta, valor)] y el resto (el documento)"""
    r = []
    grupo = 0
    nombre = None
    while i < len(d):
        t = d[i]
        i += 1
        if t == 3:
            return r, d[i:]
        if t < 0x10:
            grupo = t
            continue
        nl = struct.unpack(">H", d[i:i + 2])[0]
        i += 2
        if nl:
            nombre = d[i:i + nl].decode()
        i += nl
        vl = struct.unpack(">H", d[i:i + 2])[0]
        i += 2
        r.append((grupo, nombre, t, d[i:i + vl]))
        i += vl
    raise ValueError("sin el fin de los atributos")


def a(t, nombre, v):
    if isinstance(v, int):
        v = struct.pack(">i", v)
    elif isinstance(v, str):
        v = v.encode()
    return bytes([t]) + struct.pack(">H", len(nombre)) + nombre.encode() + struct.pack(">H", len(v)) + v


def varios(t, nombre, vs):
    return b"".join(a(t, nombre if i == 0 else "", v) for i, v in enumerate(vs))


class Impresora:
    def __init__(self, carpeta, sinpdf):
        self.carpeta = carpeta
        self.sinpdf = sinpdf
        self.n = 0
        self.cerrojo = threading.Lock()

    def respuesta(self, pet, estado, más=b""):
        return struct.pack(">BBHI", 1, 1, estado, pet) + b"\x01" + a(0x47, "attributes-charset", "utf-8") + \
            a(0x48, "attributes-natural-language", "es") + más + b"\x03"

    def atiende(self, cuerpo):
        ver, op, pet = struct.unpack(">BxHI", cuerpo[:8])
        attrs, doc = atributos(cuerpo, 8)
        nombres = {n: v for g, n, t, v in attrs}
        if ver != 1 or b"printer-uri" not in b"".join(n.encode() for g, n, t, v in attrs if n) or "attributes-charset" not in nombres:
            print("petición mal hecha (sin printer-uri o attributes-charset)", flush=True)
            return self.respuesta(pet, 0x0400)
        if op == 0x000B:
            fmts = ["image/pwg-raster"] if self.sinpdf else ["application/pdf", "image/pwg-raster", "application/octet-stream"]
            res = struct.pack(">iib", 300, 300, 3) + b"", struct.pack(">iib", 600, 600, 3)
            más = b"\x04" + a(0x42, "printer-name", "Prueba") + a(0x41, "printer-make-and-model", "Impresora de prueba de Hispa-64") + \
                a(0x23, "printer-state", 3) + a(0x44, "printer-state-reasons", "none") + a(0x22, "printer-is-accepting-jobs", b"\x01") + \
                varios(0x49, "document-format-supported", fmts) + varios(0x32, "pwg-raster-document-resolution-supported", res) + \
                varios(0x44, "pwg-raster-document-type-supported", ["sgray_8", "srgb_8"]) + a(0x44, "media-default", "iso_a4_210x297mm")
            print("Get-Printer-Attributes", flush=True)
            return self.respuesta(pet, 0, más)
        if op == 0x0002:
            fmt = nombres.get("document-format", b"application/octet-stream").decode()
            with self.cerrojo:
                self.n += 1
                n = self.n
            nombre = nombres.get("job-name", b"").decode("utf-8", "replace")
            if fmt == "application/pdf" and not self.sinpdf:
                ext, juicio = "pdf", mira_pdf(doc)
            elif fmt == "image/pwg-raster":
                ext, juicio = "pwg", mira_pwg(doc)
            else:
                print("trabajo %d: MAL: el formato %s no se admite" % (n, fmt), flush=True)
                return self.respuesta(pet, 0x040A)
            with open("%s/trabajo-%d.%s" % (self.carpeta, n, ext), "wb") as f:
                f.write(doc)
            print("trabajo %d («%s», %s, %d octetos): %s" % (n, nombre, fmt, len(doc), juicio), flush=True)
            return self.respuesta(pet, 0, b"\x02" + a(0x21, "job-id", n) + a(0x45, "job-uri", "ipp://prueba/trabajos/%d" % n) + a(0x23, "job-state", 3))
        return self.respuesta(pet, 0x0501)


def conexión(s, imp):
    try:
        s.settimeout(60)
        d = b""
        while b"\r\n\r\n" not in d:
            x = s.recv(65536)
            if not x:
                return
            d += x
        cab, cuerpo = d.split(b"\r\n\r\n", 1)
        m = re.search(rb"(?i)content-length:\s*(\d+)", cab)
        if not cab.startswith(b"POST ") or not m or b"application/ipp" not in cab.lower():
            s.sendall(b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            return
        largo = int(m.group(1))
        while len(cuerpo) < largo:
            x = s.recv(1 << 20)
            if not x:
                break
            cuerpo += x
        try:
            r = imp.atiende(cuerpo[:largo])
        except Exception as e:
            print("petición mal hecha: %s" % e, flush=True)
            r = struct.pack(">BBHI", 1, 1, 0x0400, 0) + b"\x03"
        s.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/ipp\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(r) + r)
    finally:
        s.close()


def main():
    global COLOR
    args = sys.argv[1:]
    if "--color" in args:
        k = args.index("--color")
        COLOR = list(bytes.fromhex(args[k + 1]))
        del args[k:k + 2]
    if args and args[0] == "--mira":
        d = open(args[1], "rb").read()
        print(mira_pwg(d) if d[:4] == b"RaS2" else mira_pdf(d))
        return
    imp = Impresora(args[0], "--sinpdf" in args)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(8)
    print("listo %d" % srv.getsockname()[1], flush=True)
    while True:
        s, _ = srv.accept()
        threading.Thread(target=conexión, args=(s, imp), daemon=True).start()


if __name__ == "__main__":
    main()
