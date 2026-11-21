/* surf.js - [JS] el Navegador completo de Hispa-486 (NAVEGA2.EXE): lo que
 * cambia del preludio del Navegador propio (w32/navega/dom.js) cuando la
 * página la maqueta NetSurf: las medidas de verdad de las cajas
 * (getBoundingClientRect, offsetWidth...), getComputedStyle con el estilo de
 * NetSurf, el desplazamiento de la ventana (scrollY, scrollTo). Va dentro de
 * NAVEGA2.EXE como texto, compilado una vez; se ejecuta tras dom.js.
 * docs/486/NAVEGADOR2.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
(function (N) {
'use strict';
const G = globalThis;
const S = (v) => v === undefined || v === null ? '' : String(v);
const E = G.Element.prototype;
const prop = (o, k, d) => Object.defineProperty(o, k, Object.assign({ configurable: true }, d));
const doc = G.document;
const esRaiz = (el) => el === doc.documentElement || el === doc.body;
/* la caja de NetSurf: [x, y, ancho, alto] (borde), [4] y [5] sin el borde, [6] lo de dentro */
const caja = (el) => N.es(el) && N.tipo(el) === 1 ? N.caja(el) : null;
function rect(el) {
  const c = caja(el), d = N.desp();
  const r = c ? { x: c[0] - d[0], y: c[1] - d[1], width: c[2], height: c[3] } : { x: 0, y: 0, width: 0, height: 0 };
  r.left = r.x; r.top = r.y; r.right = r.x + r.width; r.bottom = r.y + r.height;
  r.toJSON = function () { return this; };
  return r;
}
prop(E, 'getBoundingClientRect', { value() { return rect(this); }, writable: true });
prop(E, 'getClientRects', { value() { return caja(this) ? [rect(this)] : []; }, writable: true });
prop(E, 'offsetWidth', { get() { const c = caja(this); return c ? c[2] : 0; } });
prop(E, 'offsetHeight', { get() { const c = caja(this); return c ? c[3] : 0; } });
prop(E, 'clientWidth', { get() { if (this === doc.documentElement) return N.ancho(); const c = caja(this); return c ? c[4] : 0; } });
prop(E, 'clientHeight', { get() { if (this === doc.documentElement) return N.alto(); const c = caja(this); return c ? c[5] : 0; } });
prop(E, 'clientTop', { get() { return 0; } });
prop(E, 'clientLeft', { get() { return 0; } });
prop(E, 'scrollWidth', { get() { if (esRaiz(this)) return N.ancho(); const c = caja(this); return c ? c[4] : 0; } });
prop(E, 'scrollHeight', { get() { if (esRaiz(this)) return Math.max(N.altoPagina(), N.alto()); const c = caja(this); return c ? Math.max(c[5], c[6]) : 0; } });
prop(E, 'scrollTop', { get() { return esRaiz(this) ? N.desp()[1] : 0; }, set(v) { if (esRaiz(this)) N.veA(N.desp()[0], +v || 0); } });
prop(E, 'scrollLeft', { get() { return esRaiz(this) ? N.desp()[0] : 0; }, set(v) { if (esRaiz(this)) N.veA(+v || 0, N.desp()[1]); } });
/* el de arriba con position que no sea static (o body) */
function padreColocado(el) {
  if (!caja(el)) return null;
  for (let p = el.parentElement; p; p = p.parentElement) {
    if (p === doc.body || p === doc.documentElement) return doc.body;
    const pos = N.estilo(p, 'position');
    if (pos && pos !== 'static') return p;
    if (/^(td|th|table)$/.test(N.nombre(p))) return p;
  }
  return doc.body;
}
prop(E, 'offsetParent', { get() { return padreColocado(this); } });
prop(E, 'offsetTop', { get() { const c = caja(this); if (!c) return 0; const p = padreColocado(this), d = p && caja(p); return d ? c[1] - d[1] : c[1]; } });
prop(E, 'offsetLeft', { get() { const c = caja(this); if (!c) return 0; const p = padreColocado(this), d = p && caja(p); return d ? c[0] - d[0] : c[0]; } });
prop(E, 'scrollTo', { value(x, y) { if (typeof x === 'object' && x) { y = x.top; x = x.left; } if (esRaiz(this)) N.veA(+x || 0, +y || 0); }, writable: true });
prop(E, 'scroll', { value(x, y) { this.scrollTo(x, y); }, writable: true });
prop(E, 'scrollBy', { value() {}, writable: true });
// (los comentarios no son el documento)
const nodeName = Object.getOwnPropertyDescriptor(G.Node.prototype, 'nodeName');
prop(G.Node.prototype, 'nodeName', { get() { const t = N.tipo(this); return t === 8 ? '#comment' : t === 10 ? 'html' : nodeName.get.call(this); } });

/* getComputedStyle: lo que sabe NetSurf de la caja; si no, lo del atributo
 * style; si no, lo de siempre (sin animaciones: transiciones de 0 s) */
const FIJO = { 'transition-duration': '0s', 'transition-delay': '0s', 'animation-duration': '0s', 'animation-name': 'none',
  'transition-property': 'none', 'transform': 'none', 'opacity': '1', 'direction': 'ltr', 'box-sizing': 'border-box',
  'line-height': 'normal', 'font-size': '16px', 'cursor': 'auto', 'pointer-events': 'auto', 'content': 'none',
  'will-change': 'auto', 'backdrop-filter': 'none', 'filter': 'none', 'contain': 'none', 'perspective': 'none',
  'border-top-width': '0px', 'border-left-width': '0px', 'border-right-width': '0px', 'border-bottom-width': '0px',
  'padding-right': '0px', 'padding-left': '0px', 'margin-right': '0px', 'margin-left': '0px' };
const aGuion = (p) => p === 'cssFloat' ? 'float' : p.startsWith('--') ? p : p.replace(/[A-Z]/g, m => '-' + m.toLowerCase());
function valor(el, n) {
  n = S(n).toLowerCase();
  if (N.es(el) && N.tipo(el) === 1) {
    const v = N.estilo(el, n);
    if (v !== null && v !== undefined) return S(v);
    const s = el.style.getPropertyValue(n);
    if (s) return s;
    if (n === 'display') return N.oculto(el) ? 'none' : 'block';
    if (n === 'position') return 'static';
    if (n === 'visibility') return 'visible';
  }
  return n in FIJO ? FIJO[n] : '';
}
function getComputedStyle(el) {
  const cosas = {
    getPropertyValue: (n) => valor(el, n),
    getPropertyPriority: () => '',
    item: () => '',
  };
  return new Proxy({}, {
    get(o, p) {
      if (typeof p === 'symbol') return undefined;
      if (p in cosas) return cosas[p];
      if (p === 'length') return 0;
      if (p === 'cssText') return '';
      return valor(el, aGuion(p));
    },
    has() { return true; }
  });
}
// el desplazamiento de la ventana
const desp = { get() { return N.desp()[1]; }, configurable: true };
const despX = { get() { return N.desp()[0]; }, configurable: true };
const def = (k, v) => Object.defineProperty(G, k, { value: v, writable: true, configurable: true, enumerable: false });
Object.defineProperty(G, 'scrollY', desp); Object.defineProperty(G, 'pageYOffset', desp);
Object.defineProperty(G, 'scrollX', despX); Object.defineProperty(G, 'pageXOffset', despX);
def('getComputedStyle', getComputedStyle);
def('scrollTo', (x, y) => { if (typeof x === 'object' && x) { y = x.top; x = x.left; } N.veA(x === undefined ? N.desp()[0] : +x || 0, y === undefined ? N.desp()[1] : +y || 0); });
def('scroll', G.scrollTo);
def('scrollBy', (x, y) => { if (typeof x === 'object' && x) { y = x.top; x = x.left; } N.veA(N.desp()[0] + (+x || 0), N.desp()[1] + (+y || 0)); });
def('visualViewport', { get width() { return N.ancho(); }, get height() { return N.alto(); }, offsetLeft: 0, offsetTop: 0, scale: 1,
  addEventListener() {}, removeEventListener() {} });
})
