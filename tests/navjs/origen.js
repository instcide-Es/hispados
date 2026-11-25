/* tests/navjs/origen.js - [JS] lo de dom.js (el preludio de los dos
 * Navegadores de Hispa-486, tal cual) que toca al origen, en node con un N de
 * mentira (las funciones de script.c y guion.c):
 *  - localStorage y sessionStorage no le dan a C ningún origen (lo saca C de
 *    la dirección de la página): cambiar URL.prototype.origin no sirve;
 *  - fetch y XMLHttpRequest le dicen a C las galletas que piden (credentials,
 *    withCredentials), que C usa con CORS.
 * Lo corre tests/run_navegador_cors_tests.sh: node origen.js w32/navega/dom.js
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
'use strict';
const fs = require('fs'), vm = require('vm');
const src = fs.readFileSync(process.argv[2], 'utf8');
let fallos = 0;
const mira = (si, que) => { console.log('  ' + (si ? 'OK  ' : 'FAIL') + ' ' + que); if (!si) fallos = 1; };
const llamadas = [];
const almacenes = { 'https://banco.es': '{"sesion":"SECRETO-DEL-BANCO"}' };
const propias = {
  url: () => 'https://malo.es/pagina',
  absoluta: (s) => new URL(s, 'https://malo.es/pagina').href,
  ahora: () => 0,
  // (como el C de antes: si llega un origen, se usa; el bueno no lo recibe)
  almacen: (...a) => { llamadas.push(['almacen', ...a]); return a.length > 1 ? (almacenes[a[0]] || '') : ''; },
  guardaAlmacen: (...a) => { llamadas.push(['guardaAlmacen', ...a]); },
  pide: (...a) => { llamadas.push(['pide', ...a]); },
};
const N = new Proxy({}, { get(o, k) { return k in propias ? propias[k] : () => 0; } });
const ctx = vm.createContext({ console });
vm.runInContext(src, ctx)(N);
const corre = (t) => vm.runInContext(t, ctx);

corre(`Object.defineProperty(URL.prototype, 'origin', { get() { return 'https://banco.es'; } });`);
const leido = corre(`localStorage.getItem('sesion')`);
mira(leido === null, 'localStorage: con URL.prototype.origin cambiado, no lee lo de banco.es (' + leido + ')');
corre(`localStorage.setItem('sesion', 'CAMBIADO-POR-MALO'); sessionStorage.setItem('s', '1')`);
const alm = llamadas.filter(l => l[0] === 'almacen' || l[0] === 'guardaAlmacen');
mira(alm.length >= 3, 'localStorage y sessionStorage llaman a C (' + alm.length + ' veces)');
mira(alm.every(l => !l.some(x => typeof x === 'string' && /banco|malo|https?:/.test(x))), 'a N.almacen y N.guardaAlmacen no les llega ningún origen');
mira(alm.filter(l => l[0] === 'almacen').every(l => l.length === 2), 'N.almacen(local): un argumento');
mira(alm.filter(l => l[0] === 'guardaAlmacen').every(l => l.length === 3), 'N.guardaAlmacen(local, datos): dos argumentos');

llamadas.length = 0;
corre(`fetch('https://banco.es/c', { credentials: 'include' }); fetch('/x'); fetch('https://banco.es/o', { credentials: 'omit' });
  const x = new XMLHttpRequest(); x.open('GET', 'https://banco.es/y'); x.withCredentials = true; x.send();
  const y = new XMLHttpRequest(); y.open('POST', 'https://banco.es/z'); y.send('a=1');`);
const p = llamadas.filter(l => l[0] === 'pide');
const cred = (u) => { const l = p.find(l => l[3] === u); return l ? l[6] : 'ninguna'; };
mira(cred('https://banco.es/c') === 2, "fetch con credentials: 'include' -> 2");
mira(cred('https://malo.es/x') === 1, "fetch sin decir nada (same-origin) -> 1");
mira(cred('https://banco.es/o') === 0, "fetch con credentials: 'omit' -> 0");
mira(cred('https://banco.es/y') === 2, 'XMLHttpRequest con withCredentials -> 2');
mira(cred('https://banco.es/z') === 1, 'XMLHttpRequest sin withCredentials -> 1');
console.log(fallos ? '  HAY FALLOS' : '  todo bien');
process.exit(fallos);
