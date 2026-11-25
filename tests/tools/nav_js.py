"""[JS] Las páginas con JavaScript del servidor de pruebas del Navegador
(tests/tools/nav_servidor.py las sirve en /js/...; las usa
tests/run_navegador_js_tests.sh). Parecidas a lo de ClicGest: un buscador
que sugiere al escribir (fetch y JSON), pestañas, confirm() y alert(),
localStorage, setTimeout, XMLHttpRequest, un error que va al registro y un
bucle sin fin que el Navegador tiene que parar.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import json

NOMBRES = ['Socia 001 Prueba', 'Socia 010 Prueba', 'Socia 017 Prueba', 'María García', 'Antonia Núñez', 'Pepe Formación']

CAB = ('<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8"><title>%s</title>'
       '<style>.tab-pane{display:none}.tab-pane.active{display:block}.op-activa{font-weight:bold}</style></head><body>\n')
PIE = '\n<noscript><p>SIN-JAVASCRIPT</p></noscript></body></html>'

BUSCADOR_JS = r'''
(function () {
  'use strict';
  const q = document.getElementById('q'), lista = document.getElementById('lista'), estado = document.getElementById('estado');
  let visibles = [], activo = -1;
  const pinta = async () => {
    const r = await fetch(`/js/api?q=${encodeURIComponent(q.value)}`, { headers: { 'Accept': 'application/json' } });
    visibles = await r.json();
    lista.replaceChildren();
    activo = -1;
    visibles.forEach((n, i) => {
      const li = document.createElement('li');
      li.className = 'op';
      li.dataset.i = String(i);
      li.textContent = `${n} (nº ${i + 1})`;
      lista.appendChild(li);
    });
    lista.hidden = !visibles.length;
    estado.textContent = visibles.length ? `${visibles.length} coinciden.` : `Nadie coincide con «${q.value}».`;
  };
  const marca = (i) => {
    [...lista.children].forEach((li, k) => { li.classList.toggle('op-activa', k === i); li.setAttribute('aria-selected', k === i ? 'true' : 'false'); });
    activo = i;
  };
  const ir = (i) => { location.href = '/js/ficha?n=' + encodeURIComponent(visibles[i]); };
  q.addEventListener('input', pinta);
  q.addEventListener('keydown', (ev) => {
    if (ev.key === 'ArrowDown') { ev.preventDefault(); if (visibles.length) marca((activo + 1) % visibles.length); }
    else if (ev.key === 'Enter' && activo >= 0) { ev.preventDefault(); ir(activo); }
  });
  lista.addEventListener('click', (ev) => { const li = ev.target.closest('.op'); if (li) ir(+li.dataset.i); });
  console.log('buscador listo');
})();
'''

PAGINAS = {
    '/js/': ('Pruebas de JavaScript',
             '<h1>Pruebas de JavaScript</h1><p id="hola">Sin JavaScript.</p>'
             '<p><a href="/js/buscador">Buscador</a></p><p><a href="/js/pestanas">Pestañas</a></p>'
             '<p><a href="/js/cuadros">Cuadros</a></p><p><a href="/js/bucle">Bucle</a></p>'
             '<script>document.getElementById("hola").textContent = `Con JavaScript: ${[1, 2, 3].map(x => x * 2).join("-")}.`;</script>'),
    '/js/buscador': ('Buscador',
                     '<h1>Buscador</h1><form method="get" action="/js/buscador"><p>Nombre: '
                     '<input id="q" name="q" autocomplete="off"> <button type="submit">Buscar</button></p></form>'
                     '<ul id="lista" hidden></ul><p id="estado">Escriba para buscar.</p>'
                     '<script src="/js/buscador.js"></script>'),
    '/js/pestanas': ('Pestañas',
                     '<h1>Pestañas</h1><ul class="nav nav-tabs">'
                     '<li><button type="button" class="nav-link active" data-bs-toggle="tab" data-bs-target="#p1">Asientos</button></li>'
                     '<li><button type="button" class="nav-link" data-bs-toggle="tab" data-bs-target="#p2">A devolver</button></li>'
                     '<li><button type="button" class="nav-link" data-bs-toggle="tab" data-bs-target="#p3">Espera</button></li></ul>'
                     '<div class="tab-pane active" id="p1"><p>PANEL-ASIENTOS: 12 libres.</p></div>'
                     '<div class="tab-pane" id="p2"><p>PANEL-DEVOLVER: nada.</p></div>'
                     '<div class="tab-pane" id="p3"><p>PANEL-ESPERA: 2 personas.</p></div>'
                     '<details id="mas"><summary>Más</summary><p>DETALLE-ABIERTO</p></details>'
                     '<p><button type="button" id="abre">Abrir el detalle</button></p>'
                     '<script>'
                     'document.querySelectorAll("[data-bs-toggle=tab]").forEach(b => b.addEventListener("click", () => {'
                     '  document.querySelectorAll(".nav-link").forEach(x => x.classList.toggle("active", x === b));'
                     '  document.querySelectorAll(".tab-pane").forEach(p => p.classList.toggle("active", "#" + p.id === b.dataset.bsTarget));'
                     '}));'
                     'document.getElementById("abre").onclick = () => { const d = document.getElementById("mas"); d.open = !d.open; };'
                     '</script>'),
    '/js/cuadros': ('Cuadros',
                    '<h1>Cuadros</h1><p id="visitas">Visitas: ?</p><p id="tiempo">Temporizador: esperando</p>'
                    '<p id="xhr">XHR: ?</p><p id="listo">DOM: ?</p>'
                    '<p><button type="button" id="borra">Borrar</button> <button type="button" onclick="alert(\'Aviso de la página\')">Avisar</button></p>'
                    '<p id="res">Sin decidir.</p>'
                    '<form method="post" action="/js/envio" data-confirmar="¿Enviar el formulario?" onsubmit="return confirm(this.dataset.confirmar)">'
                    '<input type="hidden" name="dato" value="uno"><noscript><input type="hidden" name="_cg_sin_js" value="1"></noscript>'
                    '<button type="submit">Enviar</button></form>'
                    '<script>'
                    'const n = +(localStorage.getItem("visitas") || 0) + 1; localStorage.setItem("visitas", n);'
                    'document.getElementById("visitas").textContent = "Visitas: " + n;'
                    'setTimeout(() => { document.getElementById("tiempo").textContent = "Temporizador: hecho"; }, 1000);'
                    'document.getElementById("borra").addEventListener("click", () => {'
                    '  document.getElementById("res").textContent = confirm("¿Borrar de verdad?") ? "Sí, borrado." : "No, se queda.";'
                    '});'
                    'const x = new XMLHttpRequest(); x.open("GET", "/js/api?q=socia");'
                    'x.onload = () => { document.getElementById("xhr").textContent = "XHR: " + JSON.parse(x.responseText).length + " socias"; }; x.send();'
                    'document.addEventListener("DOMContentLoaded", () => { document.getElementById("listo").textContent = "DOM: cargado"; });'
                    'console.log("cuadros", n);'
                    '</script><script>noExiste.algo();</script>'),
    '/js/bucle': ('Bucle',
                  '<h1>Bucle</h1><p id="d">Sigue: no</p><script>for (;;) {}</script>'
                  '<script>document.getElementById("d").textContent = "Sigue: sí";</script>'),
}

# [CORS] lo que prueba /js/cors (docs/486/NAVEGADOR.md, «Lo que puede pedir una página»)
CORS_IDS = ['abierto', 'cerrado', 'pon', 'sin', 'con', 'conmal', 'json', 'put', 'propia', 'mismo', 'xhr', 'push', 'fin']
CORS_JS = r'''
(async () => {
  const o = 'http://localhost:' + location.port, pon = (id, t) => { document.getElementById(id).textContent = id + ': ' + t; };
  const t = async (id, u, op) => {
    try { const r = await fetch(u.startsWith('/') ? o + u : u, op); pon(id, await r.text() + ' secreto=' + (r.headers.get('x-secreto') || '-') + ' galleta=' + (r.headers.get('set-cookie') || '-')); }
    catch (e) { pon(id, 'ERROR ' + e.message); }
  };
  await t('abierto', '/js/otro?acao=*');
  await t('cerrado', '/js/otro');
  await t('pon', '/js/otro?pon=1&acao=o&acac=1', { credentials: 'include' });
  await t('sin', '/js/otro?acao=*');
  await t('con', '/js/otro?acao=o&acac=1', { credentials: 'include' });
  await t('conmal', '/js/otro?acao=*', { credentials: 'include' });
  await t('json', '/js/otro?acao=*', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
  await t('put', '/js/otro?acao=*', { method: 'PUT' });
  await t('propia', '/js/otro?acao=*', { headers: { 'X-Propia': '1' } });
  await t('mismo', '/js/otro?pon=1'.replace(/^/, location.origin), { headers: { 'X-Propia': 'mia', 'Cookie': 'robada=1' } });
  await new Promise((si) => { const x = new XMLHttpRequest(); x.open('GET', o + '/js/otro'); x.onload = () => { pon('xhr', 'LEIDO ' + x.responseText); si(); };
    x.onerror = () => { pon('xhr', 'ERROR'); si(); }; x.send(); });
  try { history.pushState(null, '', o + '/'); pon('push', 'SI ' + location.href); } catch (e) { pon('push', 'NO ' + e.message + ' ' + location.host); }
  pon('fin', 'FIN');
})();
'''


def atiende(h, u, q):
    """sirve una página de /js/...; False si no es de aquí"""
    if u.path == '/js/buscador.js':
        h.manda(BUSCADOR_JS.encode('utf-8'), tipo='application/javascript; charset=UTF-8')
        return True
    if u.path == '/js/api':
        t = q.get('q', [''])[0].lower()
        h.manda(json.dumps([n for n in NOMBRES if t in n.lower()], ensure_ascii=False).encode('utf-8'), tipo='application/json')
        return True
    if u.path == '/js/galletas':      # document.cookie y fetch con las galletas de http.c
        js = ('<h1>Galletas</h1><p id="c">?</p><p id="e">?</p><script>'
              'document.getElementById("c").textContent = "COOKIE: " + document.cookie;'
              'document.cookie = "sesion=pisada; path=/"; document.cookie = "js=1; path=/";'
              'document.cookie = "oculta=1; path=/; HttpOnly";'
              'fetch("/js/eco").then(r => r.text()).then(t => { document.getElementById("e").textContent = t; console.log(navigator.userAgent); });'
              '</script>')
        h.manda((CAB % 'Galletas' + js + PIE).encode('utf-8'),
                cab=(('Set-Cookie', 'vista=1; Path=/'), ('Set-Cookie', 'sesion=S1; Path=/; HttpOnly')))
        return True
    if u.path == '/js/eco':           # lo que llega: las galletas y el User-Agent
        h.manda(('ECO galletas=[%s] agente=[%s]' % (h.headers.get('Cookie', ''), h.headers.get('User-Agent', ''))).encode('utf-8'),
                tipo='text/plain; charset=UTF-8')
        return True
    if u.path == '/js/cors':          # fetch a otro origen (localhost frente a 127.0.0.1): CORS
        h.manda((CAB % 'CORS' + '<h1>CORS</h1>' + ''.join('<p id="%s">%s: ?</p>' % (i, i) for i in CORS_IDS)
                 + '<script>' + CORS_JS + '</script>' + PIE).encode('utf-8'),
                cab=(('Set-Cookie', 'propia=P1; Path=/'),))
        return True
    if u.path == '/js/otro':          # el «otro origen»: Access-Control-Allow-* según la consulta
        cab = [('X-Secreto', 'S7')]
        a = q.get('acao', [''])[0]
        if a == 'o':
            a = h.headers.get('Origin', '')
        if a:
            cab.append(('Access-Control-Allow-Origin', a))
        if q.get('acac', [''])[0]:
            cab.append(('Access-Control-Allow-Credentials', 'true'))
        if q.get('pon', [''])[0]:
            cab.append(('Set-Cookie', 'otra=O1; Path=/; SameSite=None'))
        h.manda(('OTRO galletas=[%s] origin=[%s] propia=[%s]' % (h.headers.get('Cookie', ''), h.headers.get('Origin', ''),
                                                                 h.headers.get('X-Propia', ''))).encode('utf-8'),
                tipo='text/plain; charset=UTF-8', cab=cab)
        return True
    if u.path == '/js/ficha':
        h.manda((CAB % 'Ficha' + '<h1>Ficha</h1><p>FICHA DE %s</p>' % q.get('n', ['?'])[0] + PIE).encode('utf-8'))
        return True
    if u.path in PAGINAS:
        t, c = PAGINAS[u.path]
        h.manda((CAB % t + c + PIE).encode('utf-8'))
        return True
    return False


def atiende_post(h, ruta, cuerpo):
    if ruta == '/js/envio':
        h.manda((CAB % 'Enviado' + '<h1>Enviado</h1><p>ENVIADO: %s</p>' % cuerpo + PIE).encode('utf-8'))
        return True
    return False
