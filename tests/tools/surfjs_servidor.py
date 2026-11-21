#!/usr/bin/env python3
"""Servidor de pruebas del JavaScript del Navegador completo de Hispa-486
(NAVEGA2.EXE: tests/run_navegador2_js_tests.sh, docs/486/NAVEGADOR2.md,
«JavaScript»), por HTTP: una página con guiones de hoy (clases, funciones
flecha, plantillas), un menú desplegable a lo Bootstrap (z-index), un botón
que enseña una caja, un campo con el evento input, setTimeout, localStorage,
fetch con su galleta y un formulario que JavaScript manda con fetch
(preventDefault en submit).

uso: surfjs_servidor.py CARPETA PUERTO   (CARPETA: deja pedidos.log)
Solo escucha en 127.0.0.1 (QEMU lo ve como 10.0.2.2).
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import os, sys, json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

D, P = sys.argv[1], int(sys.argv[2])

PAGINA = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>Prueba de JavaScript</title>
<style>
body { font-family: sans-serif; margin: 12px; background: #fff; }
.oculto { display: none; }
#cab { background: #888; color: #fff; padding: 6px; }
#cab.hecho { background: #306767; }
.menu { position: relative; display: inline-block; }
.menu ul { position: absolute; top: 100%; left: 0; z-index: 1000; display: none; background: #ffffcf;
           border: 1px solid #000; margin: 0; padding: 4px; list-style: none; width: 200px; }
.menu ul.show { display: block; }
.tapa { background: #cf3030; color: #fff; padding: 30px; }
#caja { background: #309830; color: #fff; padding: 8px; }
#eco, #red, #tiempo, #alm, #envio { padding: 2px; }
</style></head><body>
<div id="cab">Sin JavaScript</div>
<div class="menu"><button type="button" id="abre">Menú</button>
<ul id="lista"><li><a href="#" id="op1">Opción uno</a></li><li><a href="#" id="op2">Opción dos</a></li></ul></div>
<button type="button" id="ver">Ver la caja</button>
<div class="tapa">Esto queda debajo del menú</div>
<div id="caja" class="oculto">La caja</div>
<p><input id="campo" type="text" value="">  <span id="eco">eco</span></p>
<div id="tiempo">sin setTimeout</div>
<div id="alm">sin localStorage</div>
<div id="red">sin fetch</div>
<form id="forma" method="post" action="/no"><input type="hidden" name="a" value="1">
<button type="submit" id="manda">Mandar</button></form><div id="envio">sin enviar</div>
<script>
'use strict';
class Cuenta { constructor() { this.n = 0; } mas() { return ++this.n; } }
const c = new Cuenta();
const $ = (s) => document.querySelector(s);
$('#cab').textContent = `Con JavaScript ${[1, 2, 3].map(x => x * 2).join('-')}`;
$('#cab').classList.add('hecho');
$('#abre').addEventListener('click', (e) => { $('#lista').classList.toggle('show'); });
document.addEventListener('click', (e) => {
  const a = e.target.closest('#lista a');
  if (!a) return;
  e.preventDefault();
  $('#lista').classList.remove('show');
  $('#cab').textContent = 'Elegida: ' + a.textContent;
});
$('#ver').addEventListener('click', () => { $('#caja').classList.toggle('oculto'); $('#ver').textContent = 'Pulsado ' + c.mas(); });
$('#campo').addEventListener('input', (e) => { $('#eco').textContent = 'eco: ' + e.target.value; });
setTimeout(() => { $('#tiempo').textContent = 'setTimeout hecho'; }, 300);
const v = +(localStorage.getItem('veces') || 0) + 1;
localStorage.setItem('veces', String(v));
$('#alm').textContent = 'localStorage ' + v;
fetch('/datos.json', { headers: { 'X-Prueba': 'si' } }).then(r => r.json())
  .then(d => { $('#red').textContent = 'fetch ' + d.libro + ' ' + document.cookie; })
  .catch(e => { $('#red').textContent = 'fetch fallo ' + e; });
$('#forma').addEventListener('submit', async (e) => {
  e.preventDefault();
  const r = await fetch('/guarda', { method: 'POST', body: new URLSearchParams(new FormData(e.target)) });
  $('#envio').textContent = 'enviado: ' + (await r.text());
});
console.log('pagina lista', document.querySelectorAll('li').length);
</script></body></html>"""

class H(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *a): pass
    def apunta(self, cuerpo=b''):
        with open(os.path.join(D, 'pedidos.log'), 'a') as f:
            f.write('%s %s %s | %s | %s\n' % (self.command, self.path, self.headers.get('X-Prueba', ''),
                    self.headers.get('Cookie', ''), cuerpo.decode('utf-8', 'replace')))
    def manda(self, cod, tipo, datos, extra=()):
        self.send_response(cod)
        self.send_header('Content-Type', tipo)
        self.send_header('Content-Length', str(len(datos)))
        for k, v in extra: self.send_header(k, v)
        self.end_headers()
        self.wfile.write(datos)
    def do_GET(self):
        self.apunta()
        if self.path == '/':
            self.manda(200, 'text/html; charset=utf-8', PAGINA.encode('utf-8'), [('Set-Cookie', 'sesion=abc123; Path=/')])
        elif self.path == '/datos.json':
            self.manda(200, 'application/json', json.dumps({'libro': 'Quijote'}).encode())
        else:
            self.manda(404, 'text/plain', b'no')
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0) or 0)
        b = self.rfile.read(n) if n else b''
        self.apunta(b)
        if self.path == '/guarda':
            self.manda(200, 'text/plain; charset=utf-8', b'vale ' + b)
        else:
            self.manda(404, 'text/plain', b'no')

srv = ThreadingHTTPServer(('127.0.0.1', P), H)
print('listo', flush=True)
srv.serve_forever()
