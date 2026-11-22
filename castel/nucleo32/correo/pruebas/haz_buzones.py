#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/correo/pruebas/haz_buzones.py [CARPETA]

Los mensajes de prueba del servidor de correo de pruebas (servidor_correo.py),
para probar Correo, el cliente de correo de Hispa-Pentium. Todo determinista:
fechas fijas de 2026, octetos «al azar» con semilla fija, renglones con CRLF.

Como módulo (lo importa servidor_correo.py):
  buzones()            {usuario: [(carpeta, uso_especial, [(octetos, banderas, fecha)])]}
                       (la carpeta en Unicode, con «/» de separador; el orden
                       de la lista es el orden de los UID)
  ASUNTOS              [(carpeta, asunto legible)] de ana, en orden
  mensaje_grande(i)    el mensaje i (1..N) del INBOX de grande@prueba.test,
                       hecho al vuelo (no se guarda: con 50000 va igual)
  banderas_grande(i), fecha_grande(i)

Como programa: «haz_buzones.py CARPETA» vuelca los .eml en
CARPETA/<usuario>/<carpeta>/0001.eml... (y los 20 primeros de grande) para
mirarlos con cualquier lector.

Cada caso del INBOX de ana lleva en el asunto «Caso NN: qué prueba», para que
las pruebas lo encuentren con SEARCH SUBJECT "Caso NN".
"""
import base64
import datetime as dt
import io
import os
import random
import re
import struct
import sys
import zipfile
import zlib

UNA = dt.timezone(dt.timedelta(hours=1))
BASE = dt.datetime(2026, 3, 2, 9, 0, 0, tzinfo=UNA)
VISTO = '\\Seen'
IMPORTANTE = '\\Flagged'


def crlf(b):
    """Todos los finales de renglón a CRLF."""
    if isinstance(b, str):
        b = b.encode('utf-8')
    return re.sub(rb'\r?\n', b'\r\n', b)


def b64(d):
    """base64 en renglones de 76 (con CRLF)."""
    return crlf(base64.encodebytes(d))


def q(texto, juego='utf-8'):
    """Una palabra codificada RFC 2047 en Q."""
    s = ''
    for c in texto.encode(juego):
        ch = chr(c)
        if ch.isascii() and (ch.isalnum() or ch in '!*+-/'):
            s += ch
        elif ch == ' ':
            s += '_'
        else:
            s += '=%02X' % c
    return '=?%s?Q?%s?=' % (juego.upper(), s)


def bw(texto, juego='utf-8'):
    """Una palabra codificada RFC 2047 en B."""
    return '=?%s?B?%s?=' % (juego.upper(), base64.b64encode(texto.encode(juego)).decode())


def qp(texto, juego='utf-8'):
    """quoted-printable sencillo (renglones cortos, como los de las pruebas)."""
    out = []
    for renglon in texto.split('\n'):
        s = ''
        for c in renglon.encode(juego):
            if (33 <= c <= 126 and c != 61) or c == 32:
                s += chr(c)
            else:
                s += '=%02X' % c
            if len(s) > 70:
                out.append(s + '=')
                s = ''
        if s.endswith(' '):
            s = s[:-1] + '=20'
        out.append(s)
    return '\n'.join(out).encode('ascii')


# ---- archivos de verdad, pequeños ----
def png(ancho, alto, color):
    crudo = b''.join(b'\x00' + bytes(color) * ancho for _ in range(alto))

    def trozo(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', ancho, alto, 8, 2, 0, 0, 0))
            + trozo(b'IDAT', zlib.compress(crudo, 9)) + trozo(b'IEND', b''))


def pdf(texto):
    flujo = b'BT /F1 18 Tf 20 70 Td (' + texto.encode('latin-1') + b') Tj ET'
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>',
            b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 144] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>',
            b'<< /Length %d >>\nstream\n' % len(flujo) + flujo + b'\nendstream',
            b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
    d = b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n'
    pos = []
    for i, o in enumerate(objs):
        pos.append(len(d))
        d += b'%d 0 obj\n' % (i + 1) + o + b'\nendobj\n'
    x = len(d)
    d += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1)
    d += b''.join(b'%010d 00000 n \n' % p for p in pos)
    d += b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, x)
    return d


def zipeado():
    f = io.BytesIO()
    with zipfile.ZipFile(f, 'w') as z:
        for nombre, texto in (('LEEME.TXT', 'Esto es un zip de prueba.\r\n'), ('datos/cuotas.csv', 'socio;cuota\r\n1;20\r\n2;20\r\n')):
            zi = zipfile.ZipInfo(nombre, date_time=(2026, 3, 1, 9, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o644 << 16
            z.writestr(zi, texto)
    return f.getvalue()


EICAR = ('X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR' + '-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*').encode('ascii')
PNG_PEQUENO = png(16, 16, (200, 30, 40))
PNG_LOGO = png(32, 12, (20, 90, 200))
PDF_PEQUENO = pdf('Factura de prueba 2026')
ZIP_PEQUENO = zipeado()


# ---- cómo se hace un mensaje ----
def cabeceras(n, asunto, de='Marta Gil <marta@ejemplo.es>', para='Ana Garcia <ana@prueba.test>', fecha=None,
              mid=None, mas='', tipo='text/plain; charset=us-ascii', cte='7bit', mime=True):
    f = fecha if fecha is not None else 'Date: ' + formatea(BASE + dt.timedelta(hours=n * 5))
    s = 'From: %s\nTo: %s\nSubject: %s\n%s\nMessage-ID: %s\n' % (de, para, asunto, f, mid or '<caso%02d@ejemplo.es>' % n)
    if mime:
        s += 'MIME-Version: 1.0\n'
        if tipo:
            s += 'Content-Type: %s\n' % tipo
        if cte:
            s += 'Content-Transfer-Encoding: %s\n' % cte
    return s + mas


def formatea(f):
    dias = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
    meses = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']
    z = f.utcoffset().total_seconds() // 60
    return '%s, %d %s %d %02d:%02d:%02d %s%02d%02d' % (dias[f.weekday()], f.day, meses[f.month - 1], f.year, f.hour, f.minute,
                                                       f.second, '+' if z >= 0 else '-', abs(z) // 60, abs(z) % 60)


def junta(cab, cuerpo):
    """cabeceras (str) + renglón vacío + cuerpo (bytes o str); todo con CRLF."""
    if isinstance(cuerpo, str):
        cuerpo = cuerpo.encode('utf-8')
    return crlf(cab.encode('utf-8')) + b'\r\n' + crlf(cuerpo)


def parte(cab, cuerpo):
    if isinstance(cuerpo, str):
        cuerpo = cuerpo.encode('utf-8')
    return crlf(cab.encode('utf-8')) + b'\r\n' + crlf(cuerpo)


def multi(frontera, partes, final=True, preambulo=b'Esto es un mensaje MIME de varias partes.\r\n'):
    d = preambulo
    for p in partes:
        d += b'--' + frontera.encode() + b'\r\n' + p + (b'' if p.endswith(b'\r\n') else b'\r\n')
    if final:
        d += b'--' + frontera.encode() + b'--\r\n'
    return d


def adjunto(tipo, nombre, datos, disp=None):
    disp = disp or 'attachment; filename="%s"' % nombre
    return parte('Content-Type: %s; name="%s"\nContent-Disposition: %s\nContent-Transfer-Encoding: base64\n' % (tipo, nombre, disp), b64(datos))


TEXTO = 'Hola, Ana:\n\nEsto es un mensaje de prueba del servidor de correo de Hispa-Pentium.\n\nUn saludo,\nMarta\n'


def casos_ana():
    """Los casos del INBOX de ana: [(asunto legible, octetos, banderas)]."""
    C = []

    def pon(n, legible, datos, banderas=()):
        C.append((n, legible, datos, set(banderas)))

    # -- texto y juegos de caracteres --
    a = 'Caso 01: texto ASCII sencillo'
    pon(1, a, junta(cabeceras(1, a), TEXTO), [VISTO])
    a = 'Caso 02: UTF-8 quoted-printable (año, ñandú, €)'
    pon(2, a, junta(cabeceras(2, q(a), tipo='text/plain; charset=utf-8', cte='quoted-printable'),
                    qp('El año que viene veremos un ñandú.\nCuesta 20 € (veinte euros).\nPingüino, corazón, acción.\n')), [VISTO])
    a = 'Caso 03: ISO-8859-1 en 8bit (año, corazón)'
    pon(3, a, junta(cabeceras(3, q(a, 'iso-8859-1'), tipo='text/plain; charset=ISO-8859-1', cte='8bit'),
                    'Este año el corazón manda. Señora Muñoz, ¿qué tal?\n'.encode('latin-1')))
    a = 'Caso 04: windows-1252 con € y comillas “tipográficas”'
    pon(4, a, junta(cabeceras(4, q(a, 'windows-1252'), tipo='text/plain; charset=windows-1252', cte='8bit'),
                    'Precio: 15 € — dijo “vale” y ‘ya está’… así de fácil.\n'.encode('cp1252')))
    a = 'Caso 05: ISO-8859-15 con €'
    pon(5, a, junta(cabeceras(5, q(a, 'iso-8859-15'), tipo='text/plain; charset=iso-8859-15', cte='8bit'),
                    'La cuota del año es de 30 € en ISO-8859-15.\n'.encode('iso-8859-15')))
    a = 'Caso 06: KOI8-R (Привет, мир)'
    pon(6, a, junta(cabeceras(6, bw(a, 'koi8-r'), tipo='text/plain; charset=koi8-r', cte='8bit'),
                    'Привет, мир! Это проверка KOI8-R.\n'.encode('koi8-r')))
    # -- asuntos codificados --
    a = 'Caso 07: asunto RFC 2047 B (año)'
    pon(7, a, junta(cabeceras(7, bw(a)), TEXTO), [VISTO])
    a = 'Caso 08: asunto RFC 2047 Q (ñandú)'
    pon(8, a, junta(cabeceras(8, q(a)), TEXTO))
    a = 'Caso 09: asunto partido en varias palabras codificadas (año ñandú)'
    pon(9, a, junta(cabeceras(9, q('Caso 09: asunto partido ') + '\n ' + q('en varias palabras ') + ' ' + bw('codificadas (año') + '\n\t' + q(' ñandú)', 'iso-8859-1')), TEXTO))
    a = 'Caso 10: asunto con un trozo mal hecho'
    pon(10, a, junta(cabeceras(10, 'Caso 10: asunto con un trozo mal hecho ' + q('bien hecho') + ' =?UTF-8?B?$$$roto?= =?UTF-8?X?raro?= =?UTF-8?Q?sin_cerrar'), TEXTO))
    a = 'Caso 11: asunto en UTF-8 crudo sin codificar (año)'
    pon(11, a, junta(cabeceras(11, a, tipo='text/plain; charset=utf-8', cte='8bit'), 'Asunto y cuerpo en UTF-8 crudo: año, señal.\n'))
    a = 'Caso 12: remitente con nombre codificado (José María)'
    pon(12, a, junta(cabeceras(12, q(a), de='%s <jose@ejemplo.es>' % q('José María Pérez', 'iso-8859-1'),
                               mas='Cc: %s <luis@prueba.test>\n' % bw('Luis Ñúñez')), TEXTO))
    # -- HTML --
    a = 'Caso 13: multipart/alternative texto y HTML'
    pon(13, a, junta(cabeceras(13, a, tipo='multipart/alternative; boundary="alt13"', cte=None), multi('alt13', [
        parte('Content-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: quoted-printable\n', qp('Versión en texto: la reunión es el jueves.\n')),
        parte('Content-Type: text/html; charset=utf-8\nContent-Transfer-Encoding: quoted-printable\n',
              qp('<html><body><p>Versión en <b>HTML</b>: la reunión es el <i>jueves</i>.</p></body></html>\n'))])), [VISTO])
    a = 'Caso 14: solo HTML con imagen remota y píxel de rastreo'
    pon(14, a, junta(cabeceras(14, a, tipo='text/html; charset=utf-8', cte='8bit'),
                     '<html><body><h1>Ofertas</h1><p><img src="https://imagenes.ejemplo/oferta.jpg" alt="oferta"></p>\n'
                     '<p>Texto de la oferta.</p><img src="http://rastreo.ejemplo/p.gif" width="1" height="1" alt="">\n</body></html>\n'))
    a = 'Caso 15: multipart/related con imagen cid'
    pon(15, a, junta(cabeceras(15, a, tipo='multipart/related; boundary="rel15"; type="text/html"', cte=None), multi('rel15', [
        parte('Content-Type: text/html; charset=us-ascii\n', '<html><body><p>El logo:</p><img src="cid:logo15@ejemplo.es" alt="logo"></body></html>\n'),
        parte('Content-Type: image/png; name="logo.png"\nContent-ID: <logo15@ejemplo.es>\nContent-Disposition: inline; filename="logo.png"\nContent-Transfer-Encoding: base64\n', b64(PNG_LOGO))])))
    a = 'Caso 16: HTML peligroso (script, onload, javascript)'
    pon(16, a, junta(cabeceras(16, a, tipo='text/html; charset=us-ascii'),
                     '<html><head><script>alert("hola")</script></head>\n<body onload="robar()">\n'
                     '<p>Pulsa <a href="javascript:robar()">aqui</a>.</p><img src="x" onerror="robar()">\n'
                     '<iframe src="http://malo.ejemplo/"></iframe></body></html>\n'))
    a = 'Caso 17: enlace engañoso (banco.es a malo.ejemplo)'
    pon(17, a, junta(cabeceras(17, q(a), tipo='text/html; charset=utf-8', cte='8bit'),
                     '<html><body><p>Entre en su banco: <a href="http://malo.ejemplo/entrar">https://www.banco.es</a></p></body></html>\n'))
    a = 'Caso 18: suplantación (Banco Santander) con Reply-To de otro dominio'
    pon(18, a, junta(cabeceras(18, q(a), de='"Banco Santander <soporte@bancosantander.es>" <alerta@malo.ejemplo>',
                               mas='Reply-To: Soporte <cobros@otro-dominio.ejemplo>\n'),
                     'Su cuenta ha sido bloqueada. Responda a este mensaje con su clave.\n'))
    # -- adjuntos --
    a = 'Caso 19: adjunto PDF'
    pon(19, a, junta(cabeceras(19, a, tipo='multipart/mixed; boundary="mix19"', cte=None), multi('mix19', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Te mando la factura en PDF.\n'),
        adjunto('application/pdf', 'factura.pdf', PDF_PEQUENO)])), [VISTO])
    a = 'Caso 20: adjunto TXT'
    pon(20, a, junta(cabeceras(20, a, tipo='multipart/mixed; boundary="mix20"', cte=None), multi('mix20', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Va una nota.\n'),
        parte('Content-Type: text/plain; charset=utf-8; name="nota.txt"\nContent-Disposition: attachment; filename="nota.txt"\nContent-Transfer-Encoding: quoted-printable\n',
              qp('Nota adjunta: comprar café y azúcar.\n'))])))
    a = 'Caso 21: adjunto PNG'
    pon(21, a, junta(cabeceras(21, a, tipo='multipart/mixed; boundary="mix21"', cte=None), multi('mix21', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Una imagen.\n'), adjunto('image/png', 'cuadro.png', PNG_PEQUENO)])))
    a = 'Caso 22: adjunto ZIP'
    pon(22, a, junta(cabeceras(22, a, tipo='multipart/mixed; boundary="mix22"', cte=None), multi('mix22', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Los datos van en el zip.\n'), adjunto('application/zip', 'datos.zip', ZIP_PEQUENO)])))
    a = 'Caso 23: adjunto EICAR (factura.com)'
    pon(23, a, junta(cabeceras(23, a, de='Facturas <facturas@malo.ejemplo>', tipo='multipart/mixed; boundary="mix23"', cte=None), multi('mix23', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Abra la factura adjunta.\n'), adjunto('application/octet-stream', 'factura.com', EICAR)])))
    a = 'Caso 24: adjunto con nombre RFC 2231 (Presupuesto año 2026.pdf)'
    pon(24, a, junta(cabeceras(24, q(a), tipo='multipart/mixed; boundary="mix24"', cte=None), multi('mix24', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'El presupuesto.\n'),
        parte("Content-Type: application/pdf;\n name*0*=utf-8''Presupuesto%20a;\n name*1*=%C3%B1o%202026.pdf\n"
              "Content-Disposition: attachment;\n filename*0*=utf-8''Presupuesto%20a;\n filename*1*=%C3%B1o%202026.pdf\n"
              'Content-Transfer-Encoding: base64\n', b64(PDF_PEQUENO))])))
    a = 'Caso 25: adjunto con nombre RFC 2047 solo en name= (Señales.txt)'
    pon(25, a, junta(cabeceras(25, q(a), tipo='multipart/mixed; boundary="mix25"', cte=None), multi('mix25', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Adjunto con el nombre en RFC 2047.\n'),
        parte('Content-Type: text/plain; charset=utf-8; name="%s"\nContent-Transfer-Encoding: base64\n' % bw('Señales.txt'),
              b64('Señales de tráfico.\r\n'.encode()))])))
    a = 'Caso 26: mensaje reenviado adjunto (message/rfc822)'
    interno = junta(cabeceras(90, 'Acta de la asamblea', de='Pedro Ruiz <pedro@ejemplo.es>', para='Marta Gil <marta@ejemplo.es>',
                              mid='<acta@ejemplo.es>', tipo='text/plain; charset=utf-8', cte='8bit'),
                    'Acta: se aprueba la subida de la cuota a 25 €.\n')
    pon(26, a, junta(cabeceras(26, a, tipo='multipart/mixed; boundary="mix26"', cte=None), multi('mix26', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Te reenvio el acta.\n'),
        parte('Content-Type: message/rfc822\nContent-Disposition: attachment; filename="acta.eml"\n', interno)])))
    a = 'Caso 27: multipart anidado (mixed > alternative > related)'
    rel = multi('rel27', [
        parte('Content-Type: text/html; charset=us-ascii\n', '<html><body><p>Con logo: <img src="cid:logo27@ejemplo.es"></p></body></html>\n'),
        parte('Content-Type: image/png\nContent-ID: <logo27@ejemplo.es>\nContent-Transfer-Encoding: base64\n', b64(PNG_LOGO))], preambulo=b'')
    alt = multi('alt27', [parte('Content-Type: text/plain; charset=us-ascii\n', 'Con logo (en texto).\n'),
                          parte('Content-Type: multipart/related; boundary="rel27"\n', rel)], preambulo=b'')
    pon(27, a, junta(cabeceras(27, a, tipo='multipart/mixed; boundary="mix27"', cte=None), multi('mix27', [
        parte('Content-Type: multipart/alternative; boundary="alt27"\n', alt), adjunto('application/pdf', 'anexo.pdf', PDF_PEQUENO)])))
    a = 'Caso 28: format=flowed con DelSp'
    pon(28, a, junta(cabeceras(28, a, tipo='text/plain; charset=utf-8; format=flowed; delsp=yes', cte='8bit'),
                     'Este parrafo largo esta partido en varios renglones con espacio al fi \nnal y DelSp, asi que hay que juntarlo sin el espacio del co \nrte.\n\n> Una cita que \n> sigue aqui.\n\n-- \nFirma\n'))
    # -- un hilo de 4 --
    hilo = 'Caso 29: hilo de 4 mensajes (Comida de Navidad)'
    refs = []
    for k, (pre, quien) in enumerate((('', 'Marta Gil <marta@ejemplo.es>'), ('Re: ', 'Ana Garcia <ana@prueba.test>'),
                                      ('RE: ', 'Pedro Ruiz <pedro@ejemplo.es>'), ('Re: Re: ', 'Marta Gil <marta@ejemplo.es>'))):
        mid = '<hilo%d@ejemplo.es>' % (k + 1)
        mas = ''
        if refs:
            mas = 'In-Reply-To: %s\nReferences: %s\n' % (refs[-1], ' '.join(refs))
        pon(29, pre + hilo + ' %d de 4' % (k + 1), junta(cabeceras(29 + k * 0, pre + hilo + ' %d de 4' % (k + 1), de=quien, mid=mid, mas=mas,
                                                              fecha='Date: ' + formatea(BASE + dt.timedelta(days=10, hours=k))),
                                                    'Mensaje %d del hilo.\n' % (k + 1)), [VISTO] if k < 2 else [])
        refs.append(mid)
    # -- fechas --
    for n, f, que in ((33, 'Tue, 3 Mar 2026 10:15:00 +0100', 'fecha con zona +0100'),
                      (34, 'Tue, 3 Mar 2026 08:15:00 -0500', 'fecha con zona -0500'),
                      (35, 'Tue, 3 Mar 2026 13:15:00 GMT', 'fecha con zona GMT'),
                      (36, 'Tue, 3 Mar 2026 08:15:00 EST', 'fecha con zona EST'),
                      (37, '3 Mar 2026 14:15:00 +0100', 'fecha sin día de la semana'),
                      (38, 'Tue, 3 Mar 26 14:15:00 +0100', 'fecha con el año de dos cifras'),
                      (39, 'ayer por la tarde, mas o menos', 'fecha inválida')):
        a = 'Caso %02d: %s' % (n, que)
        pon(n, a, junta(cabeceras(n, q(a), fecha='Date: ' + f), 'La fecha de este mensaje: %s\n' % f))
    a = 'Caso 40: marcado como importante (Flagged)'
    pon(40, a, junta(cabeceras(40, a), TEXTO), [IMPORTANTE, VISTO])
    # -- malformados --
    a = 'Caso 41: malformado, multipart sin frontera final'
    pon(41, a, junta(cabeceras(41, a, tipo='multipart/mixed; boundary="mix41"', cte=None), multi('mix41', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Primera parte.\n'),
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Segunda parte, sin frontera final.\n')], final=False)))
    a = 'Caso 42: malformado, frontera que no aparece nunca'
    pon(42, a, junta(cabeceras(42, a, tipo='multipart/mixed; boundary="nunca42"', cte=None),
                     '--otra\nContent-Type: text/plain\n\nEsta frontera no es la anunciada.\n--otra--\n'))
    a = 'Caso 43: malformado, base64 roto'
    pon(43, a, junta(cabeceras(43, a, tipo='text/plain; charset=utf-8', cte='base64'),
                     base64.b64encode('Texto en base64 que luego se rompe.'.encode()).decode()[:30] + '!!**@@\nSG9sYQ\n=Zm9v=\n'))
    a = 'Caso 44: malformado, cabecera sin dos puntos'
    pon(44, a, junta(cabeceras(44, a, mas='Esto no es una cabecera\nX-Normal: si\n'), TEXTO))
    a = 'Caso 45: malformado, renglón de 20000 octetos'
    pon(45, a, junta(cabeceras(45, q(a)), 'Principio.\n' + ('abcdefghij' * 2000) + '\nFin.\n'))
    a = 'Caso 46: malformado, octetos NUL'
    pon(46, a, junta(cabeceras(46, a, tipo='text/plain; charset=us-ascii', cte='8bit'), b'Antes del NUL\x00\x00despues del NUL.\nOtro\x00renglon.\n'))
    a = 'Caso 47: malformado, Content-Type sin charset en 8 bits'
    pon(47, a, junta(cabeceras(47, a, tipo='text/plain', cte='8bit'), 'Sin juego de caracteres: año, acción, €.\n'.encode('cp1252')))
    a = 'Caso 48: malformado, Content-Transfer-Encoding desconocido'
    pon(48, a, junta(cabeceras(48, a, tipo='text/plain; charset=us-ascii', cte='x-uuencode-raro'), 'Codificacion desconocida: se ensena tal cual.\n'))
    a = 'Caso 49: adjunto grande de 2 MB'
    grande = random.Random(20261122).randbytes(2 * 1024 * 1024)
    pon(49, a, junta(cabeceras(49, a, tipo='multipart/mixed; boundary="mix49"', cte=None), multi('mix49', [
        parte('Content-Type: text/plain; charset=us-ascii\n', 'Un adjunto de 2 MB.\n'), adjunto('application/octet-stream', 'grande.bin', grande)])))
    return C


def otra(n, carpeta, asunto, cuerpo='Mensaje de prueba.\n', de='Ana Garcia <ana@prueba.test>', para='Marta Gil <marta@ejemplo.es>', mas=''):
    return junta(cabeceras(n, q(asunto) if not asunto.isascii() else asunto, de=de, para=para, mid='<%s%d@prueba.test>' % (carpeta.lower()[:4], n), mas=mas), cuerpo)


def fecha_de(n):
    return BASE + dt.timedelta(hours=n * 5)


ASUNTOS = []


def buzones():
    """Los buzones al arrancar, de los tres usuarios."""
    del ASUNTOS[:]
    inbox = []
    for i, (n, legible, datos, banderas) in enumerate(casos_ana()):
        inbox.append((datos, banderas, fecha_de(n) + dt.timedelta(minutes=i)))
        ASUNTOS.append(('INBOX', legible))

    def lista(carpeta, cosas):
        r = []
        for k, (asunto, datos, banderas) in enumerate(cosas):
            r.append((datos, set(banderas), fecha_de(60 + k)))
            ASUNTOS.append((carpeta, asunto))
        return r
    enviados = lista('Sent', [
        ('Re: Caso 02: UTF-8 quoted-printable (año, ñandú, €)',
         otra(60, 'Sent', 'Re: Caso 02: UTF-8 quoted-printable (año, ñandú, €)', '¡Qué bien lo del ñandú!\n\n> El año que viene veremos un ñandú.\n',
              mas='In-Reply-To: <caso02@ejemplo.es>\nReferences: <caso02@ejemplo.es>\n'), [VISTO]),
        ('Enviado: convocatoria de la junta', otra(61, 'Sent', 'Enviado: convocatoria de la junta', 'Junta el lunes a las 18:00.\n'), [VISTO])])
    borradores = lista('Drafts', [('Borrador: carta sin terminar', otra(62, 'Drafts', 'Borrador: carta sin terminar', 'Querida Marta:\n\nTe escribo para\n'), [VISTO, '\\Draft'])])
    papelera = lista('Trash', [('Papelera: publicidad vieja', otra(63, 'Trash', 'Papelera: publicidad vieja', 'Oferta caducada.\n', de='Tienda <ofertas@tienda.ejemplo>', para='ana@prueba.test'), [VISTO]),
                               ('Papelera: aviso borrado', otra(64, 'Trash', 'Papelera: aviso borrado', 'Ya no vale.\n'), [VISTO])])
    spam = lista('Junk', [('Spam: ha ganado un premio', otra(65, 'Junk', 'Spam: ha ganado un premio', 'Pulse aqui.\n', de='Premios <premio@malo.ejemplo>', para='ana@prueba.test'), [])])
    archivo = lista('Archive', [('Archivo: memoria de 2025', otra(66, 'Archive', 'Archivo: memoria de 2025', 'La memoria del año pasado.\n', de='Pedro Ruiz <pedro@ejemplo.es>', para='ana@prueba.test'), [VISTO]),
                                ('Archivo: fotos de la excursión', otra(67, 'Archive', 'Archivo: fotos de la excursión', 'Las fotos.\n', de='Marta Gil <marta@ejemplo.es>', para='ana@prueba.test'), [VISTO])])
    cuotas = lista('Asociación/Cuotas', [('Cuotas: recibo de enero', otra(68, 'Cuot', 'Cuotas: recibo de enero', 'Recibo de la cuota de enero: 20 €.\n', de='Tesorería <tesoreria@ejemplo.es>', para='ana@prueba.test'), [VISTO]),
                                         ('Cuotas: recibo de febrero', otra(69, 'Cuot', 'Cuotas: recibo de febrero', 'Recibo de la cuota de febrero: 20 €.\n', de='Tesorería <tesoreria@ejemplo.es>', para='ana@prueba.test'), [])])
    vacias = [('Sent', '\\Sent', []), ('Drafts', '\\Drafts', []), ('Trash', '\\Trash', []), ('Junk', '\\Junk', []), ('Archive', '\\Archive', [])]
    bienvenida = junta(cabeceras(70, 'Bienvenida a la cuenta de pruebas', de='Administrador <admin@prueba.test>', para='luis@prueba.test', mid='<bienvenida@prueba.test>'),
                       'Hola, Luis. Esta cuenta recibe lo que se le envia.\n')
    return {
        'ana@prueba.test': [('INBOX', None, inbox), ('Sent', '\\Sent', enviados), ('Drafts', '\\Drafts', borradores),
                            ('Trash', '\\Trash', papelera), ('Junk', '\\Junk', spam), ('Archive', '\\Archive', archivo),
                            ('Asociación', None, []), ('Asociación/Cuotas', None, cuotas)],
        'luis@prueba.test': [('INBOX', None, [(bienvenida, set(), fecha_de(70))])] + vacias,
        'grande@prueba.test': [('INBOX', None, None)] + vacias,   # None: lo hace el servidor con mensaje_grande()
    }


# ---- el relleno de grande@prueba.test ----
NOMBRES = ['Marta Gil', 'Pedro Ruiz', 'Lucia Moreno', 'Jordi Puig', 'Carmen Vidal', 'Antonio Sanchez', 'Rosa Marin', 'Pablo Ortega',
           'Elena Castro', 'Miguel Navarro', 'Nuria Ferrer', 'Javier Romero', 'Isabel Torres', 'Andreu Serra', 'Laura Blanco']
DOMINIOS = ['ejemplo.es', 'correo.ejemplo', 'asociacion.ejemplo', 'ayuntamiento.ejemplo', 'prueba.test']
PALABRAS = ('reunion junta cuota socio excursion viaje autobus comida fiesta recibo factura banco cuenta pago presupuesto '
            'memoria actividad taller curso horario lunes martes jueves sabado domingo mañana tarde noche lista asamblea '
            'votacion acta informe resumen fotos programa bingo loteria biblioteca libro prestamo devolucion aviso '
            'cambio fecha lugar plaza salon local llaves material compra tienda mercado ayuda gracias saludo').split()


def fecha_grande(i):
    return dt.datetime(2026, 1, 1, 8, 0, 0, tzinfo=UNA) + dt.timedelta(minutes=9 * i)


def banderas_grande(i):
    h = (i * 2654435761) & 0xffffffff
    b = set()
    if h % 100 < 70:
        b.add(VISTO)
    if (h >> 8) % 100 < 3:
        b.add(IMPORTANTE)
    return b


def mensaje_grande(i):
    r = random.Random(7700000 + i)
    nombre = r.choice(NOMBRES)
    correo = nombre.lower().replace(' ', '.') + '@' + r.choice(DOMINIOS)
    asunto = 'Relleno %05d: ' % i + ' '.join(r.choice(PALABRAS) for _ in range(r.randint(2, 6)))
    largo = r.randint(300, 2000)
    palabras = r.choices(PALABRAS, k=largo // 6 + 2)
    renglones, renglon, ancho, total = [], [], 0, 0
    for p in palabras:
        renglon.append(p)
        ancho += len(p) + 1
        total += len(p) + 1
        if ancho > 70:
            renglones.append(' '.join(renglon))
            renglon, ancho = [], 0
        if total >= largo:
            break
    renglones.append(' '.join(renglon))
    texto = '\n'.join(renglones) + '\n'
    cab = 'From: %s <%s>\nTo: grande@prueba.test\nSubject: %s\nDate: %s\nMessage-ID: <relleno%d@prueba.test>\nMIME-Version: 1.0\n' % (
        nombre, correo, q(asunto) if not asunto.isascii() else asunto, formatea(fecha_grande(i)), i)
    if r.random() < 0.05:
        f = 'r%d' % i
        cab += 'Content-Type: multipart/mixed; boundary="%s"\n' % f
        cuerpo = multi(f, [parte('Content-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: 8bit\n', texto),
                           adjunto('application/octet-stream', 'datos%d.bin' % i, r.randbytes(r.randint(100, 900)))])
        return junta(cab, cuerpo)
    cab += 'Content-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: 8bit\n'
    return junta(cab, texto)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    destino = sys.argv[1]
    for usuario, carpetas in buzones().items():
        for nombre, _uso, mensajes in carpetas:
            sitio = os.path.join(destino, usuario, nombre)
            os.makedirs(sitio, exist_ok=True)
            if mensajes is None:
                mensajes = [(mensaje_grande(i), None, None) for i in range(1, 21)]
            for k, (datos, _b, _f) in enumerate(mensajes):
                with open(os.path.join(sitio, '%04d.eml' % (k + 1)), 'wb') as f:
                    f.write(datos)
    for carpeta, asunto in ASUNTOS:
        print('%-20s %s' % (carpeta, asunto))
    return 0


if __name__ == '__main__':
    sys.exit(main())
