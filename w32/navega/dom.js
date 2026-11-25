/* dom.js - [JS] el Navegador de Hispa-486: el DOM y lo del navegador que ve
 * JavaScript (document, window, eventos, fetch, localStorage...), hecho en
 * JavaScript sobre lo poco que da script.c (N: el árbol de html.c y dom.c, la
 * red de http.c, los cuadros de Hispa Ventanas). Se compila una vez al abrir
 * el Navegador y se ejecuta al empezar cada página.
 * Escrito desde la descripción pública del DOM y de HTML (WHATWG).
 * docs/486/NAVEGADOR.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
(function (N) {
'use strict';
const G = globalThis;
const def = (o, props) => { for (const k of Object.keys(props)) Object.defineProperty(o, k, { value: props[k], writable: true, configurable: true }); };
const oculto = (o, k, v) => Object.defineProperty(o, k, { value: v, writable: true, configurable: true, enumerable: false });
const S = (v) => v === undefined || v === null ? '' : String(v);

/* =============================================================================
 *  Eventos
 * ============================================================================= */
class Event {
  constructor(type, init) {
    init = init || {};
    this.type = String(type);
    this.bubbles = !!init.bubbles;
    this.cancelable = !!init.cancelable;
    this.composed = !!init.composed;
    this.defaultPrevented = false;
    this.target = null;
    this.currentTarget = null;
    this.eventPhase = 0;
    this.timeStamp = N.ahora();
    this.isTrusted = false;
    oculto(this, '__para', false);
    oculto(this, '__paraYa', false);
  }
  preventDefault() { if (this.cancelable) this.defaultPrevented = true; }
  stopPropagation() { this.__para = true; }
  stopImmediatePropagation() { this.__para = true; this.__paraYa = true; }
  composedPath() { const r = []; for (let n = this.target; n; n = n.parentNode) r.push(n); r.push(G); return r; }
  get returnValue() { return !this.defaultPrevented; }
  set returnValue(v) { if (!v) this.preventDefault(); }
  get srcElement() { return this.target; }
  initEvent(t, b, c) { this.type = t; this.bubbles = !!b; this.cancelable = !!c; }
}
Event.NONE = 0; Event.CAPTURING_PHASE = 1; Event.AT_TARGET = 2; Event.BUBBLING_PHASE = 3;
class CustomEvent extends Event { constructor(t, i) { super(t, i); this.detail = i && 'detail' in i ? i.detail : null; } }
class UIEvent extends Event { constructor(t, i) { super(t, i); this.view = G; this.detail = (i && i.detail) || 0; } }
class MouseEvent extends UIEvent {
  constructor(t, i) {
    super(t, i); i = i || {};
    this.button = i.button || 0; this.buttons = i.buttons || 0;
    this.clientX = i.clientX || 0; this.clientY = i.clientY || 0; this.pageX = this.clientX; this.pageY = this.clientY;
    this.screenX = 0; this.screenY = 0; this.offsetX = 0; this.offsetY = 0;
    this.ctrlKey = !!i.ctrlKey; this.shiftKey = !!i.shiftKey; this.altKey = !!i.altKey; this.metaKey = !!i.metaKey;
    this.relatedTarget = i.relatedTarget || null;
  }
}
class PointerEvent extends MouseEvent { constructor(t, i) { super(t, i); this.pointerType = 'mouse'; this.pointerId = 1; this.isPrimary = true; } }
class KeyboardEvent extends UIEvent {
  constructor(t, i) {
    super(t, i); i = i || {};
    this.key = i.key || ''; this.code = i.code || ''; this.keyCode = i.keyCode || 0; this.which = this.keyCode;
    this.charCode = i.charCode || 0; this.location = 0; this.repeat = false; this.isComposing = false;
    this.ctrlKey = !!i.ctrlKey; this.shiftKey = !!i.shiftKey; this.altKey = !!i.altKey; this.metaKey = !!i.metaKey;
  }
  getModifierState() { return false; }
}
class FocusEvent extends UIEvent { constructor(t, i) { super(t, i); this.relatedTarget = (i && i.relatedTarget) || null; } }
class InputEvent extends UIEvent { constructor(t, i) { super(t, i); this.data = (i && i.data) || null; this.inputType = (i && i.inputType) || 'insertText'; this.isComposing = false; } }
class SubmitEvent extends Event { constructor(t, i) { super(t, i); this.submitter = (i && i.submitter) || null; } }
class ErrorEvent extends Event { constructor(t, i) { super(t, i); i = i || {}; this.message = i.message || ''; this.filename = i.filename || ''; this.lineno = i.lineno || 0; this.colno = 0; this.error = i.error || null; } }
class PopStateEvent extends Event { constructor(t, i) { super(t, i); this.state = (i && i.state) || null; } }
class HashChangeEvent extends Event { constructor(t, i) { super(t, i); this.oldURL = (i && i.oldURL) || ''; this.newURL = (i && i.newURL) || ''; } }

const escuchas = (o) => { if (!Object.prototype.hasOwnProperty.call(o, '__esc')) oculto(o, '__esc', Object.create(null)); return o.__esc; };
const DE_CLIC = { click: 1, mousedown: 1, mouseup: 1, pointerdown: 1, pointerup: 1, touchstart: 1, dblclick: 1 };
class EventTarget {
  addEventListener(type, f, op) {
    if (!f) return;
    const cap = typeof op === 'boolean' ? op : !!(op && op.capture);
    const l = escuchas(this)[type] || (escuchas(this)[type] = []);
    for (const x of l) if (x.f === f && x.cap === cap) return;
    l.push({ f, cap, una: !!(op && op.once), signal: op && op.signal });
    if (op && op.signal) op.signal.addEventListener('abort', () => this.removeEventListener(type, f, op));
    if (DE_CLIC[type] && N.es(this)) N.escucha(this, 1);
  }
  removeEventListener(type, f, op) {
    const cap = typeof op === 'boolean' ? op : !!(op && op.capture);
    const l = this.__esc && this.__esc[type];
    if (!l) return;
    const k = l.findIndex(x => x.f === f && x.cap === cap);
    if (k >= 0) l.splice(k, 1);
  }
  dispatchEvent(ev) {
    if (!(ev instanceof Event)) throw new TypeError('dispatchEvent: no es un Event');
    ev.target = this;
    const camino = [];
    if (N.es(this)) { for (let n = this.parentNode; n; n = n.parentNode) camino.push(n); camino.push(G); }
    else if (this !== G && this === documento) camino.push(G);
    // captura, en el objetivo y burbuja
    for (let k = camino.length - 1; k >= 0 && !ev.__para; k--) llama(camino[k], ev, 1);
    if (!ev.__para) llama(this, ev, 2);
    if (ev.bubbles) for (let k = 0; k < camino.length && !ev.__para; k++) llama(camino[k], ev, 3);
    ev.currentTarget = null;
    ev.eventPhase = 0;
    return !ev.defaultPrevented;
  }
}
/* las funciones de un objeto para un evento (fase 1 captura, 2 objetivo, 3 burbuja) */
function llama(o, ev, fase) {
  ev.currentTarget = o;
  ev.eventPhase = fase;
  const l = o.__esc && o.__esc[ev.type];
  if (l && l.length) {
    for (const x of l.slice()) {
      if (fase === 1 && !x.cap) continue;
      if (fase === 3 && x.cap) continue;
      if (x.una) o.removeEventListener(ev.type, x.f, x.cap);
      try {
        if (typeof x.f === 'function') x.f.call(o, ev);
        else if (x.f && typeof x.f.handleEvent === 'function') x.f.handleEvent(ev);
      } catch (e) { informa(e); }
      if (ev.__paraYa) return;
    }
  }
  if (fase !== 1) {
    // onclick = function..., o el atributo onclick="..."
    let h = o['on' + ev.type];
    if (typeof h !== 'function' && N.es(o) && o.nodeType === 1) h = enLinea(o, ev.type);
    if (typeof h === 'function') {
      try {
        const r = h.call(o, ev);
        if (r === false && ev.type !== 'mouseover') ev.preventDefault();
      } catch (e) { informa(e); }
    }
  }
}
/* onclick="..." del HTML: se compila una vez (y otra si cambia el atributo) */
function enLinea(el, type) {
  const a = N.atr(el, 'on' + type);
  if (a === null) return null;
  const c = el.__enl || (oculto(el, '__enl', Object.create(null)), el.__enl);
  if (c[type] && c[type].src === a) return c[type].f;
  let f = null;
  try { f = new Function('event', a); } catch (e) { informa(e); }
  c[type] = { src: a, f };
  return f;
}
function informa(e) {
  try {
    const t = (e && e.stack) ? String(e) + '\n' + e.stack : String(e);
    N.registro(3, t);
    const ev = new ErrorEvent('error', { message: String(e && e.message || e), error: e, cancelable: true });
    if (typeof G.onerror === 'function') G.onerror(ev.message, '', 0, 0, e);
    llama(G, ev, 2);
  } catch (x) { N.registro(3, 'error al informar de un error: ' + x); }
}

/* =============================================================================
 *  Nodos
 * ============================================================================= */
const FUERA = { script: 1, style: 1, head: 1, title: 1, meta: 1, link: 1, template: 1 };
const VACIOS = { area: 1, base: 1, br: 1, col: 1, embed: 1, hr: 1, img: 1, input: 1, link: 1, meta: 1, source: 1, track: 1, wbr: 1 };
class Node extends EventTarget {
  get nodeType() { return N.tipo(this); }
  get nodeName() { const t = N.tipo(this); return t === 1 ? this.tagName : t === 3 ? '#text' : '#document'; }
  get parentNode() { return N.padre(this); }
  get parentElement() { const p = N.padre(this); return p && p.nodeType === 1 ? p : null; }
  get firstChild() { return N.hijo(this); }
  get lastChild() { return N.ult(this); }
  get nextSibling() { return N.sig(this); }
  get previousSibling() { return N.ant(this); }
  get childNodes() { const r = []; for (let n = N.hijo(this); n; n = N.sig(n)) r.push(n); return lista(r); }
  get ownerDocument() { return N.tipo(this) === 9 ? null : documento; }
  get isConnected() { return N.conectado(this); }
  get textContent() { return N.tipo(this) === 9 ? null : N.contenido(this); }
  set textContent(v) { N.ponContenido(this, S(v)); }
  get nodeValue() { return N.tipo(this) === 3 ? N.texto(this) : null; }
  set nodeValue(v) { if (N.tipo(this) === 3) N.ponTexto(this, S(v)); }
  hasChildNodes() { return !!N.hijo(this); }
  getRootNode() { let n = this; while (N.padre(n)) n = N.padre(n); return n; }
  contains(o) { return !!o && (o === this || (N.es(o) && N.dentro(this, o))); }
  appendChild(n) { return this.insertBefore(n, null); }
  insertBefore(n, ref) {
    if (n instanceof DocumentFragment) { for (const h of n.__hijos.splice(0)) N.inserta(this, h, ref || null); return n; }
    if (!N.es(n)) throw new TypeError('insertBefore: no es un nodo');
    N.inserta(this, n, ref || null);
    return n;
  }
  removeChild(n) { if (N.padre(n) !== this) throw new Error('NotFoundError: no es un hijo'); N.quita(n); return n; }
  replaceChild(nuevo, viejo) { this.insertBefore(nuevo, viejo); if (nuevo !== viejo) N.quita(viejo); return viejo; }
  cloneNode(hondo) { return N.clona(this, !!hondo); }
  isSameNode(o) { return o === this; }
  isEqualNode(o) { return !!o && o.outerHTML === this.outerHTML; }
  compareDocumentPosition(o) { return this.contains(o) ? 20 : o.contains(this) ? 10 : 4; }
  normalize() {}
}
for (const [k, v] of Object.entries({ ELEMENT_NODE: 1, TEXT_NODE: 3, COMMENT_NODE: 8, DOCUMENT_NODE: 9, DOCUMENT_FRAGMENT_NODE: 11,
  DOCUMENT_POSITION_CONTAINED_BY: 16, DOCUMENT_POSITION_CONTAINS: 8, DOCUMENT_POSITION_FOLLOWING: 4, DOCUMENT_POSITION_PRECEDING: 2 })) {
  Node[k] = v; Node.prototype[k] = v;
}
/* las listas (NodeList, HTMLCollection): un Array con item() */
function lista(a) { oculto(a, 'item', (i) => a[i] || null); oculto(a, 'namedItem', (n) => a.find(e => e.id === n || e.getAttribute('name') === n) || null); return a; }

class CharacterData extends Node {
  get data() { return N.texto(this); }
  set data(v) { N.ponTexto(this, S(v)); }
  get length() { return N.texto(this).length; }
  get wholeText() { return N.texto(this); }
  appendData(t) { this.data += t; }
  remove() { N.quita(this); }
  before(...a) { antes(this, a); }
  after(...a) { despues(this, a); }
  replaceWith(...a) { despues(this, a); N.quita(this); }
}
class Text extends CharacterData {
  constructor(t) { super(); return N.creaTexto(S(t)); }
  splitText(i) { const t = this.data; this.data = t.slice(0, i); const n = N.creaTexto(t.slice(i)); this.after(n); return n; }
}
class Comment extends CharacterData {}
const aNodos = (a) => a.map(x => N.es(x) || x instanceof DocumentFragment ? x : N.creaTexto(S(x)));
function antes(n, a) { const p = N.padre(n); if (p) for (const x of aNodos(a)) p.insertBefore(x, n); }
function despues(n, a) { const p = N.padre(n); if (!p) return; const s = N.sig(n); for (const x of aNodos(a)) p.insertBefore(x, s); }

class DocumentFragment extends Node {
  constructor() { super(); oculto(this, '__hijos', []); }
  get nodeType() { return 11; }
  get nodeName() { return '#document-fragment'; }
  get childNodes() { return lista(this.__hijos.slice()); }
  get children() { return lista(this.__hijos.filter(x => x.nodeType === 1)); }
  get firstChild() { return this.__hijos[0] || null; }
  get lastChild() { return this.__hijos[this.__hijos.length - 1] || null; }
  get firstElementChild() { return this.children[0] || null; }
  get childElementCount() { return this.children.length; }
  get parentNode() { return null; }
  get textContent() { return this.__hijos.map(x => x.textContent).join(''); }
  set textContent(v) { this.__hijos.length = 0; if (S(v)) this.__hijos.push(N.creaTexto(S(v))); }
  insertBefore(n, ref) {
    const add = n instanceof DocumentFragment ? n.__hijos.splice(0) : [n];
    for (const x of add) { if (N.es(x)) N.quita(x); const i = ref ? this.__hijos.indexOf(ref) : -1; if (i >= 0) this.__hijos.splice(i, 0, x); else this.__hijos.push(x); }
    return n;
  }
  appendChild(n) { return this.insertBefore(n, null); }
  append(...a) { for (const x of aNodos(a)) this.appendChild(x); }
  prepend(...a) { const p = this.__hijos[0] || null; for (const x of aNodos(a)) this.insertBefore(x, p); }
  removeChild(n) { const i = this.__hijos.indexOf(n); if (i >= 0) this.__hijos.splice(i, 1); return n; }
  replaceChildren(...a) { this.__hijos.length = 0; this.append(...a); }
  querySelector(s) { for (const h of this.__hijos) if (h.nodeType === 1) { if (h.matches(s)) return h; const r = h.querySelector(s); if (r) return r; } return null; }
  querySelectorAll(s) { const r = []; for (const h of this.__hijos) if (h.nodeType === 1) { if (h.matches(s)) r.push(h); r.push(...h.querySelectorAll(s)); } return lista(r); }
  getElementById(id) { return this.querySelector('#' + CSS.escape(id)); }
  cloneNode(hondo) { const f = new DocumentFragment(); if (hondo) for (const h of this.__hijos) f.__hijos.push(h.cloneNode(true)); return f; }
}

/* ---- classList ---- */
class DOMTokenList {
  constructor(el, atr) { oculto(this, '__el', el); oculto(this, '__a', atr); }
  __lee() { return S(N.atr(this.__el, this.__a)).split(/[ \t\n\r\f]+/).filter(Boolean); }
  __pon(l) { const v = l.join(' '); if (v !== S(N.atr(this.__el, this.__a))) N.ponAtr(this.__el, this.__a, v); }
  get length() { return this.__lee().length; }
  get value() { return S(N.atr(this.__el, this.__a)); }
  set value(v) { N.ponAtr(this.__el, this.__a, S(v)); }
  item(i) { return this.__lee()[i] || null; }
  contains(c) { return this.__lee().includes(S(c)); }
  add(...c) { const l = this.__lee(); for (const x of c) if (!l.includes(S(x))) l.push(S(x)); this.__pon(l); }
  remove(...c) { const l = this.__lee().filter(x => !c.map(S).includes(x)); if (l.length !== this.__lee().length) this.__pon(l); }
  toggle(c, forzar) {
    c = S(c);
    const tiene = this.contains(c), quiere = forzar === undefined ? !tiene : !!forzar;
    if (quiere && !tiene) this.add(c); else if (!quiere && tiene) this.remove(c);
    return quiere;
  }
  replace(a, b) { const l = this.__lee(); const i = l.indexOf(S(a)); if (i < 0) return false; l[i] = S(b); this.__pon(l); return true; }
  supports() { return true; }
  forEach(f, t) { this.__lee().forEach((x, i) => f.call(t, x, i, this)); }
  entries() { return this.__lee().entries(); }
  keys() { return this.__lee().keys(); }
  values() { return this.__lee().values(); }
  [Symbol.iterator]() { return this.__lee()[Symbol.iterator](); }
  toString() { return this.value; }
}
/* ---- style: lo que se escribe va al atributo style (y la maqueta solo mira
 * display, visibility, color, fondo, negrita, alineación) ---- */
const aGuion = (p) => p === 'cssFloat' ? 'float' : p.startsWith('--') ? p : p.replace(/[A-Z]/g, m => '-' + m.toLowerCase());
const MAQUETA = { display: 1, visibility: 1, color: 1, 'background-color': 1, background: 1, 'font-weight': 1, 'text-align': 1, 'text-decoration': 1 };
function leeEstilo(t) {
  const m = new Map();
  for (const d of S(t).split(';')) {
    const i = d.indexOf(':');
    if (i > 0) m.set(d.slice(0, i).trim().toLowerCase(), d.slice(i + 1).trim());
  }
  return m;
}
function estilo(el) {
  if (el.__est) return el.__est;
  const pon = (m, cambia) => {
    let t = '';
    for (const [k, v] of m) t += k + ': ' + v + '; ';
    N.ponAtr(el, 'style', t.trim(), cambia ? 0 : 1);
  };
  const cosas = {
    getPropertyValue: (p) => leeEstilo(N.atr(el, 'style')).get(S(p).toLowerCase()) || '',
    setProperty: (p, v) => { p = S(p).toLowerCase(); const m = leeEstilo(N.atr(el, 'style')); if (v === null || v === '' || v === undefined) m.delete(p); else m.set(p, S(v).replace(/\s*!important\s*$/, '')); pon(m, MAQUETA[p]); },
    removeProperty: (p) => { p = S(p).toLowerCase(); const m = leeEstilo(N.atr(el, 'style')); const v = m.get(p) || ''; if (m.delete(p)) pon(m, MAQUETA[p]); return v; },
    getPropertyPriority: () => '',
    item: (i) => [...leeEstilo(N.atr(el, 'style')).keys()][i] || '',
  };
  const px = new Proxy({}, {
    get(o, p) {
      if (typeof p === 'symbol') return undefined;
      if (p in cosas) return cosas[p];
      if (p === 'cssText') return S(N.atr(el, 'style'));
      if (p === 'length') return leeEstilo(N.atr(el, 'style')).size;
      return cosas.getPropertyValue(aGuion(p));
    },
    set(o, p, v) {
      if (p === 'cssText') { N.ponAtr(el, 'style', S(v), 0); return true; }
      cosas.setProperty(aGuion(String(p)), v);
      return true;
    },
    has(o, p) { return true; }
  });
  oculto(el, '__est', px);
  return px;
}
/* ---- dataset: los atributos data-* ---- */
function datos(el) {
  if (el.__dat) return el.__dat;
  const aAtr = (p) => 'data-' + String(p).replace(/[A-Z]/g, m => '-' + m.toLowerCase());
  const px = new Proxy({}, {
    get(o, p) { if (typeof p === 'symbol') return undefined; const v = N.atr(el, aAtr(p)); return v === null ? undefined : v; },
    set(o, p, v) { N.ponAtr(el, aAtr(p), S(v)); return true; },
    has(o, p) { return N.atr(el, aAtr(p)) !== null; },
    deleteProperty(o, p) { N.quitaAtr(el, aAtr(p)); return true; },
    ownKeys() { return N.atrs(el).filter(n => n.startsWith('data-')).map(n => n.slice(5).replace(/-([a-z])/g, (m, l) => l.toUpperCase())); },
    getOwnPropertyDescriptor(o, p) { const v = N.atr(el, aAtr(p)); return v === null ? undefined : { value: v, writable: true, enumerable: true, configurable: true }; }
  });
  oculto(el, '__dat', px);
  return px;
}
const atrLogico = (n) => ({ get() { return N.atr(this, n) !== null; }, set(v) { if (v) N.ponAtr(this, n, ''); else N.quitaAtr(this, n); }, configurable: true });
const atrTexto = (n) => ({ get() { return S(N.atr(this, n)); }, set(v) { N.ponAtr(this, n, S(v)); }, configurable: true });
const atrURL = (n) => ({ get() { const v = N.atr(this, n); return v === null ? '' : N.absoluta(v); }, set(v) { N.ponAtr(this, n, S(v)); }, configurable: true });

class Element extends Node {
  get tagName() { return N.nombre(this).toUpperCase(); }
  get localName() { return N.nombre(this); }
  get namespaceURI() { return 'http://www.w3.org/1999/xhtml'; }
  get id() { return S(N.atr(this, 'id')); }
  set id(v) { N.ponAtr(this, 'id', S(v)); }
  get className() { return S(N.atr(this, 'class')); }
  set className(v) { N.ponAtr(this, 'class', S(v)); }
  get classList() { return this.__cl || (oculto(this, '__cl', new DOMTokenList(this, 'class')), this.__cl); }
  get style() { return estilo(this); }
  set style(v) { N.ponAtr(this, 'style', S(v), 0); }
  get dataset() { return datos(this); }
  getAttribute(n) { return N.atr(this, S(n).toLowerCase()); }
  setAttribute(n, v) { N.ponAtr(this, S(n).toLowerCase(), S(v)); }
  removeAttribute(n) { N.quitaAtr(this, S(n).toLowerCase()); }
  hasAttribute(n) { return N.atr(this, S(n).toLowerCase()) !== null; }
  hasAttributes() { return N.atrs(this).length > 0; }
  toggleAttribute(n, f) { const t = this.hasAttribute(n), q = f === undefined ? !t : !!f; if (q) { if (!t) this.setAttribute(n, ''); } else this.removeAttribute(n); return q; }
  getAttributeNames() { return N.atrs(this); }
  getAttributeNS(ns, n) { return this.getAttribute(n); }
  setAttributeNS(ns, n, v) { this.setAttribute(n, v); }
  get attributes() { const el = this; return lista(N.atrs(this).map(n => ({ name: n, localName: n, nodeName: n, get value() { return N.atr(el, n); }, set value(v) { N.ponAtr(el, n, S(v)); } }))); }
  get children() { const r = []; for (let n = N.hijo(this); n; n = N.sig(n)) if (N.tipo(n) === 1) r.push(n); return lista(r); }
  get childElementCount() { return this.children.length; }
  get firstElementChild() { for (let n = N.hijo(this); n; n = N.sig(n)) if (N.tipo(n) === 1) return n; return null; }
  get lastElementChild() { for (let n = N.ult(this); n; n = N.ant(n)) if (N.tipo(n) === 1) return n; return null; }
  get nextElementSibling() { for (let n = N.sig(this); n; n = N.sig(n)) if (N.tipo(n) === 1) return n; return null; }
  get previousElementSibling() { for (let n = N.ant(this); n; n = N.ant(n)) if (N.tipo(n) === 1) return n; return null; }
  get innerHTML() { return N.html(this, 0); }
  set innerHTML(v) { N.ponHtml(this, S(v)); }
  get outerHTML() { return N.html(this, 1); }
  set outerHTML(v) { const p = N.padre(this); if (!p) return; const t = documento.createElement('div'); t.innerHTML = S(v); while (t.firstChild) p.insertBefore(t.firstChild, this); N.quita(this); }
  get innerText() { return this.textContent; }
  set innerText(v) { this.textContent = v; }
  get outerText() { return this.textContent; }
  insertAdjacentHTML(donde, html) {
    const t = documento.createElement(N.nombre(this) === 'tr' || N.nombre(this) === 'tbody' ? 'tbody' : 'div');
    t.innerHTML = S(html);
    const f = new DocumentFragment();
    while (t.firstChild) f.appendChild(t.firstChild);
    this.insertAdjacentElement(donde, f);
  }
  insertAdjacentElement(donde, el) {
    switch (S(donde).toLowerCase()) {
      case 'beforebegin': if (N.padre(this)) N.padre(this).insertBefore(el, this); break;
      case 'afterbegin': this.insertBefore(el, N.hijo(this)); break;
      case 'beforeend': this.appendChild(el); break;
      case 'afterend': if (N.padre(this)) N.padre(this).insertBefore(el, N.sig(this)); break;
    }
    return el;
  }
  insertAdjacentText(donde, t) { this.insertAdjacentElement(donde, N.creaTexto(S(t))); }
  querySelector(s) { return N.busca(this, S(s), 1); }
  querySelectorAll(s) { return lista(N.busca(this, S(s), 0)); }
  getElementsByTagName(t) { return lista(t === '*' ? N.busca(this, '*', 0) : N.busca(this, S(t).replace(/[^\w-]/g, ''), 0)); }
  getElementsByClassName(c) { return lista(N.busca(this, S(c).trim().split(/\s+/).map(x => '.' + CSS.escape(x)).join(''), 0)); }
  matches(s) { return N.casa(this, S(s)); }
  webkitMatchesSelector(s) { return this.matches(s); }
  closest(s) { s = S(s); for (let n = this; n && N.tipo(n) === 1; n = N.padre(n)) if (N.casa(n, s)) return n; return null; }
  append(...a) { for (const x of aNodos(a)) this.appendChild(x); }
  prepend(...a) { const p = N.hijo(this); for (const x of aNodos(a)) this.insertBefore(x, p); }
  before(...a) { antes(this, a); }
  after(...a) { despues(this, a); }
  remove() { N.quita(this); }
  replaceWith(...a) { despues(this, a); N.quita(this); }
  replaceChildren(...a) { while (N.hijo(this)) N.quita(N.hijo(this)); this.append(...a); }
  click() {
    if (this.disabled) return;
    const nom = N.nombre(this), t = this.type;
    const caja = nom === 'input' && (t === 'checkbox' || t === 'radio');
    const antes = caja ? this.checked : false;
    if (caja) this.checked = t === 'radio' ? true : !antes;
    const ok = this.dispatchEvent(new PointerEvent('click', { bubbles: true, cancelable: true, detail: 1 }));
    if (caja) {
      if (!ok) this.checked = antes;
      else if (antes !== this.checked) { this.dispatchEvent(new InputEvent('input', { bubbles: true })); this.dispatchEvent(new Event('change', { bubbles: true })); }
      return;
    }
    if (!ok) return;
    if (nom === 'a' && N.atr(this, 'href') !== null) {
      const h = N.atr(this, 'href');
      if (/^javascript:/i.test(h)) (0, eval)(decodeURIComponent(h.slice(11)));
      else if (h[0] === '#') location.hash = h;
      else location.href = this.href;
    } else if ((nom === 'button' && this.type === 'submit') || (nom === 'input' && (t === 'submit' || t === 'image'))) {
      const f = this.form;
      if (f) f.requestSubmit(this);
    } else if (nom === 'label') {
      const c = this.htmlFor ? documento.getElementById(this.htmlFor) : this.querySelector('input,select,textarea');
      if (c) c.click();
    } else if (nom === 'summary') {
      const d = N.padre(this);
      if (d && N.nombre(d) === 'details') d.open = !d.open;
    }
  }
  focus() { N.foco(this); }
  blur() { if (documento.activeElement === this) N.foco(null); }
  scrollIntoView() { N.aLaVista(this); }
  scrollTo() {} scrollBy() {} scroll() {}
  getBoundingClientRect() { const v = !N.oculto(this); return { x: 0, y: 0, top: 0, left: 0, right: v ? 100 : 0, bottom: v ? 20 : 0, width: v ? 100 : 0, height: v ? 20 : 0, toJSON() { return this; } }; }
  getClientRects() { return N.oculto(this) ? [] : [this.getBoundingClientRect()]; }
  get offsetWidth() { return N.oculto(this) ? 0 : 100; }
  get offsetHeight() { return N.oculto(this) ? 0 : 20; }
  get offsetTop() { return 0; } get offsetLeft() { return 0; }
  get offsetParent() { return N.oculto(this) ? null : documento.body; }
  get clientWidth() { return N.oculto(this) ? 0 : 100; } get clientHeight() { return N.oculto(this) ? 0 : 20; }
  get scrollWidth() { return this.clientWidth; } get scrollHeight() { return this.clientHeight; }
  get scrollTop() { return 0; } set scrollTop(v) {} get scrollLeft() { return 0; } set scrollLeft(v) {}
  animate() { return { finished: Promise.resolve(), cancel() {}, play() {}, pause() {}, onfinish: null }; }
  getAnimations() { return []; }
  attachShadow() { throw new Error('NotSupportedError: sin Shadow DOM'); }
  setPointerCapture() {} releasePointerCapture() {} hasPointerCapture() { return false; }
  requestFullscreen() { return Promise.reject(new Error('sin pantalla completa')); }
}
class HTMLElement extends Element {}
Object.defineProperties(HTMLElement.prototype, {
  hidden: atrLogico('hidden'), title: atrTexto('title'), lang: atrTexto('lang'), dir: atrTexto('dir'),
  accessKey: atrTexto('accesskey'), draggable: atrLogico('draggable'), translate: atrLogico('translate'),
  tabIndex: { get() { const v = N.atr(this, 'tabindex'); return v === null ? (/^(a|button|input|select|textarea)$/.test(N.nombre(this)) ? 0 : -1) : parseInt(v, 10) || 0; }, set(v) { N.ponAtr(this, 'tabindex', S(v)); }, configurable: true },
  contentEditable: { get() { return S(N.atr(this, 'contenteditable')) || 'inherit'; }, set(v) { N.ponAtr(this, 'contenteditable', S(v)); }, configurable: true },
  isContentEditable: { get() { return false; }, configurable: true },
  // formularios y controles
  value: { get() { return N.valor(this); }, set(v) { N.ponValor(this, S(v)); }, configurable: true },
  defaultValue: { get() { return N.nombre(this) === 'textarea' ? this.textContent : S(N.atr(this, 'value')); }, set(v) { if (N.nombre(this) === 'textarea') this.textContent = v; else N.ponAtr(this, 'value', S(v)); }, configurable: true },
  checked: { get() { return N.marcado(this); }, set(v) { N.ponMarcado(this, !!v); }, configurable: true },
  defaultChecked: atrLogico('checked'),
  selected: { get() { return N.marcado(this); }, set(v) { N.ponMarcado(this, !!v); }, configurable: true },
  defaultSelected: atrLogico('selected'),
  disabled: atrLogico('disabled'), readOnly: atrLogico('readonly'), required: atrLogico('required'),
  multiple: atrLogico('multiple'), autofocus: atrLogico('autofocus'), noValidate: atrLogico('novalidate'),
  indeterminate: { get() { return !!this.__indet; }, set(v) { oculto(this, '__indet', !!v); }, configurable: true },
  name: atrTexto('name'), placeholder: atrTexto('placeholder'), autocomplete: atrTexto('autocomplete'),
  min: atrTexto('min'), max: atrTexto('max'), step: atrTexto('step'), pattern: atrTexto('pattern'),
  maxLength: { get() { const v = N.atr(this, 'maxlength'); return v === null ? -1 : parseInt(v, 10); }, set(v) { N.ponAtr(this, 'maxlength', S(v)); }, configurable: true },
  size: { get() { return parseInt(S(N.atr(this, 'size')), 10) || 20; }, set(v) { N.ponAtr(this, 'size', S(v)); }, configurable: true },
  rows: { get() { return parseInt(S(N.atr(this, 'rows')), 10) || 2; }, set(v) { N.ponAtr(this, 'rows', S(v)); }, configurable: true },
  cols: { get() { return parseInt(S(N.atr(this, 'cols')), 10) || 20; }, set(v) { N.ponAtr(this, 'cols', S(v)); }, configurable: true },
  type: {
    get() {
      const n = N.nombre(this), t = S(N.atr(this, 'type')).toLowerCase();
      if (n === 'input') return t || 'text';
      if (n === 'button') return t || 'submit';
      if (n === 'select') return N.atr(this, 'multiple') !== null ? 'select-multiple' : 'select-one';
      if (n === 'textarea') return 'textarea';
      return t;
    }, set(v) { N.ponAtr(this, 'type', S(v)); }, configurable: true
  },
  form: { get() { const f = N.atr(this, 'form'); if (f) return documento.getElementById(f); return this.closest('form'); }, configurable: true },
  labels: { get() { const id = this.id; return lista([...(id ? documento.querySelectorAll('label[for="' + CSS.escape(id) + '"]') : []), ...(this.closest('label') ? [this.closest('label')] : [])]); }, configurable: true },
  validity: { get() { const v = !(this.required && !this.value && /^(input|select|textarea)$/.test(N.nombre(this))); return { valid: v, valueMissing: !v, typeMismatch: false, patternMismatch: false, tooLong: false, tooShort: false, rangeUnderflow: false, rangeOverflow: false, stepMismatch: false, badInput: false, customError: !!this.__msj }; }, configurable: true },
  validationMessage: { get() { return this.__msj || (this.validity.valid ? '' : 'Rellene este campo.'); }, configurable: true },
  willValidate: { get() { return true; }, configurable: true },
  valueAsNumber: { get() { return parseFloat(this.value); }, set(v) { this.value = S(v); }, configurable: true },
  selectionStart: { get() { return this.value.length; }, set(v) {}, configurable: true },
  selectionEnd: { get() { return this.value.length; }, set(v) {}, configurable: true },
  // listas
  options: { get() { return lista(N.busca(this, 'option', 0)); }, configurable: true },
  selectedOptions: { get() { return lista(N.busca(this, 'option', 0).filter(o => o.selected)); }, configurable: true },
  selectedIndex: { get() { return N.elegida(this); }, set(v) { N.ponElegida(this, v | 0); }, configurable: true },
  length: { get() { return N.nombre(this) === 'select' ? this.options.length : N.nombre(this) === 'form' ? this.elements.length : undefined; }, configurable: true },
  index: { get() { const s = this.closest('select'); return s ? [...s.options].indexOf(this) : 0; }, configurable: true },
  label: { get() { return S(N.atr(this, 'label')) || this.textContent.trim(); }, set(v) { N.ponAtr(this, 'label', S(v)); }, configurable: true },
  text: { get() { return this.textContent; }, set(v) { this.textContent = v; }, configurable: true },
  // formulario
  elements: { get() { return lista(N.busca(this, 'input,select,textarea,button,fieldset,output', 0)); }, configurable: true },
  action: { get() { const v = N.atr(this, 'action'); return N.absoluta(v === null || v === '' ? N.url() : v); }, set(v) { N.ponAtr(this, 'action', S(v)); }, configurable: true },
  method: { get() { return S(N.atr(this, 'method')).toLowerCase() === 'post' ? 'post' : 'get'; }, set(v) { N.ponAtr(this, 'method', S(v)); }, configurable: true },
  enctype: atrTexto('enctype'), target: atrTexto('target'), acceptCharset: atrTexto('accept-charset'),
  // enlaces, imágenes...
  href: atrURL('href'), src: atrURL('src'), alt: atrTexto('alt'), rel: atrTexto('rel'), download: atrTexto('download'),
  htmlFor: atrTexto('for'), colSpan: { get() { return parseInt(S(N.atr(this, 'colspan')), 10) || 1; }, set(v) { N.ponAtr(this, 'colspan', S(v)); }, configurable: true },
  rowSpan: { get() { return parseInt(S(N.atr(this, 'rowspan')), 10) || 1; }, set(v) { N.ponAtr(this, 'rowspan', S(v)); }, configurable: true },
  width: { get() { return parseInt(S(N.atr(this, 'width')), 10) || 0; }, set(v) { N.ponAtr(this, 'width', S(v)); }, configurable: true },
  height: { get() { return parseInt(S(N.atr(this, 'height')), 10) || 0; }, set(v) { N.ponAtr(this, 'height', S(v)); }, configurable: true },
  complete: { get() { return true; }, configurable: true },
  naturalWidth: { get() { return 0; }, configurable: true }, naturalHeight: { get() { return 0; }, configurable: true },
  open: {
    get() { return N.abierto(this); },
    set(v) { const a = N.abierto(this); N.ponAbierto(this, !!v); if (a !== !!v && N.nombre(this) === 'details') setTimeout(() => this.dispatchEvent(new Event('toggle')), 0); },
    configurable: true
  },
  returnValue: { get() { return this.__rv || ''; }, set(v) { oculto(this, '__rv', S(v)); }, configurable: true },
  content: { get() { if (!this.__cont) { const f = new DocumentFragment(); while (N.hijo(this)) f.appendChild(N.hijo(this)); oculto(this, '__cont', f); } return this.__cont; }, configurable: true },
  async: atrLogico('async'), defer: atrLogico('defer'), charset: atrTexto('charset'),
  media: atrTexto('media'), sizes: atrTexto('sizes'), srcset: atrTexto('srcset'), loading: atrTexto('loading'),
  // las partes de la dirección de un <a>
  protocol: { get() { return new URL(this.href).protocol; }, configurable: true },
  host: { get() { return new URL(this.href).host; }, configurable: true },
  hostname: { get() { return new URL(this.href).hostname; }, configurable: true },
  pathname: { get() { return new URL(this.href).pathname; }, configurable: true },
  search: { get() { return new URL(this.href).search; }, configurable: true },
  hash: { get() { try { return new URL(this.href).hash; } catch (e) { return ''; } }, configurable: true },
  origin: { get() { return new URL(this.href).origin; }, configurable: true },
});
def(HTMLElement.prototype, {
  submit() { N.envia(this, null); },
  requestSubmit(b) {
    if (!this.checkValidity()) return;
    const ev = new SubmitEvent('submit', { bubbles: true, cancelable: true, submitter: b || null });
    if (this.dispatchEvent(ev)) N.envia(this, b || null);
  },
  reset() { if (this.dispatchEvent(new Event('reset', { bubbles: true, cancelable: true }))) for (const e of this.elements) { if ('defaultValue' in e && /^(input|textarea)$/.test(N.nombre(e)) && !/^(checkbox|radio|submit|button|reset)$/.test(e.type)) e.value = e.defaultValue; else if (/^(checkbox|radio)$/.test(e.type)) e.checked = e.defaultChecked; } },
  checkValidity() {
    if (N.nombre(this) === 'form') return [...this.elements].every(e => e.checkValidity());
    const v = this.validity.valid && !this.__msj;
    if (!v) this.dispatchEvent(new Event('invalid', { cancelable: true }));
    return v;
  },
  reportValidity() { const v = this.checkValidity(); if (!v) N.cuadro(0, this.validationMessage); return v; },
  setCustomValidity(m) { oculto(this, '__msj', S(m)); },
  select() {}, setSelectionRange() {}, setRangeText() {},
  stepUp() { this.value = S((parseFloat(this.value) || 0) + 1); }, stepDown() { this.value = S((parseFloat(this.value) || 0) - 1); },
  showModal() { N.ponAtr(this, 'open', ''); }, show() { N.ponAtr(this, 'open', ''); },
  close(rv) { if (N.atr(this, 'open') === null) return; if (rv !== undefined) this.returnValue = rv; N.quitaAtr(this, 'open'); this.dispatchEvent(new Event('close')); },
  add(o, antes) { this.insertBefore(o, typeof antes === 'number' ? this.options[antes] || null : antes || null); },
  play() { return Promise.resolve(); }, pause() {}, load() {}, canPlayType() { return ''; },
  getContext() { return null; }, toDataURL() { return 'data:,'; },
  decode() { return Promise.resolve(); },
  showPicker() {},
});
// los nombres de los constructores, para instanceof y para quien los busque
const HTML = {};
for (const n of ['HTMLDivElement', 'HTMLSpanElement', 'HTMLAnchorElement', 'HTMLButtonElement', 'HTMLInputElement', 'HTMLFormElement',
  'HTMLSelectElement', 'HTMLOptionElement', 'HTMLTextAreaElement', 'HTMLLabelElement', 'HTMLImageElement', 'HTMLTableElement',
  'HTMLTableRowElement', 'HTMLTableCellElement', 'HTMLUListElement', 'HTMLLIElement', 'HTMLParagraphElement', 'HTMLHeadingElement',
  'HTMLScriptElement', 'HTMLStyleElement', 'HTMLLinkElement', 'HTMLMetaElement', 'HTMLBodyElement', 'HTMLHtmlElement',
  'HTMLHeadElement', 'HTMLDetailsElement', 'HTMLDialogElement', 'HTMLTemplateElement', 'HTMLCanvasElement', 'HTMLIFrameElement',
  'HTMLFieldSetElement', 'HTMLLegendElement', 'HTMLOptGroupElement', 'HTMLTableSectionElement', 'HTMLPreElement', 'HTMLHRElement',
  'HTMLBRElement', 'HTMLOListElement', 'HTMLDListElement', 'HTMLVideoElement', 'HTMLAudioElement', 'HTMLMediaElement',
  'HTMLUnknownElement', 'HTMLOutputElement', 'HTMLProgressElement', 'HTMLDataListElement', 'HTMLTimeElement', 'HTMLPictureElement',
  'HTMLSourceElement', 'SVGElement', 'SVGSVGElement']) {
  const C = function () { throw new TypeError('Illegal constructor'); };
  C.prototype = HTMLElement.prototype;
  Object.defineProperty(C, Symbol.hasInstance, { value: (x) => N.es(x) && N.tipo(x) === 1 && esDe(n, N.nombre(x)) });
  Object.defineProperty(C, 'name', { value: n });
  HTML[n] = C;
}
function esDe(c, t) {
  const m = { HTMLDivElement: 'div', HTMLSpanElement: 'span', HTMLAnchorElement: 'a', HTMLButtonElement: 'button', HTMLInputElement: 'input',
    HTMLFormElement: 'form', HTMLSelectElement: 'select', HTMLOptionElement: 'option', HTMLTextAreaElement: 'textarea', HTMLLabelElement: 'label',
    HTMLImageElement: 'img', HTMLTableElement: 'table', HTMLTableRowElement: 'tr', HTMLUListElement: 'ul', HTMLLIElement: 'li',
    HTMLParagraphElement: 'p', HTMLScriptElement: 'script', HTMLStyleElement: 'style', HTMLLinkElement: 'link', HTMLMetaElement: 'meta',
    HTMLBodyElement: 'body', HTMLHtmlElement: 'html', HTMLHeadElement: 'head', HTMLDetailsElement: 'details', HTMLDialogElement: 'dialog',
    HTMLTemplateElement: 'template', HTMLCanvasElement: 'canvas', HTMLIFrameElement: 'iframe', HTMLFieldSetElement: 'fieldset',
    HTMLLegendElement: 'legend', HTMLOptGroupElement: 'optgroup', HTMLPreElement: 'pre', HTMLHRElement: 'hr', HTMLBRElement: 'br',
    HTMLOListElement: 'ol', HTMLDListElement: 'dl', HTMLVideoElement: 'video', HTMLAudioElement: 'audio', HTMLDataListElement: 'datalist',
    HTMLTimeElement: 'time', HTMLPictureElement: 'picture', HTMLSourceElement: 'source', SVGSVGElement: 'svg' };
  if (c === 'HTMLHeadingElement') return /^h[1-6]$/.test(t);
  if (c === 'HTMLTableCellElement') return t === 'td' || t === 'th';
  if (c === 'HTMLTableSectionElement') return t === 'thead' || t === 'tbody' || t === 'tfoot';
  if (c === 'HTMLMediaElement') return t === 'video' || t === 'audio';
  if (c === 'SVGElement') return t === 'svg';
  if (c === 'HTMLUnknownElement') return t === '';
  return m[c] === t;
}

/* =============================================================================
 *  document
 * ============================================================================= */
let estadoDoc = 'loading';
let actual = null;          // document.currentScript
// document.implementation.createHTMLDocument(): un documento aparte (un <html>
// suelto, con su <head> y su <body>) para leer HTML sin tocar la página. Antes
// daba el de la página, y jQuery, al arrancar («body.innerHTML = '<form></form>
// <form></form>'», para ver si sabe), se llevaba el cuerpo entero de la página:
// Wikipedia salía en blanco (21/11/2026, el Navegador de Hispa-Pentium)
function docAparte(titulo) {
  const html = documento.createElement('html'), head = documento.createElement('head'), body = documento.createElement('body');
  html.appendChild(head); html.appendChild(body);
  if (titulo !== undefined) { const t = documento.createElement('title'); t.textContent = S(titulo); head.appendChild(t); }
  return {
    nodeType: 9, nodeName: '#document', documentElement: html, head, body, defaultView: null, location: null, readyState: 'complete',
    get title() { const t = head.querySelector('title'); return t ? t.textContent : ''; },
    createElement: (n) => documento.createElement(n), createElementNS: (ns, n) => documento.createElementNS(ns, n),
    createTextNode: (t) => documento.createTextNode(t), createComment: (t) => documento.createComment(t),
    createDocumentFragment: () => documento.createDocumentFragment(),
    get implementation() { return documento.implementation; },
    querySelector: (s) => html.querySelector(s), querySelectorAll: (s) => html.querySelectorAll(s),
    getElementById: (id) => html.querySelector('#' + CSS.escape(S(id))),
    getElementsByTagName: (t) => html.getElementsByTagName(t), getElementsByClassName: (c) => html.getElementsByClassName(c),
    importNode: (n, h) => n.cloneNode(h), adoptNode: (n) => n,
    addEventListener() {}, removeEventListener() {},
  };
}
class Document extends Node {
  get documentElement() { for (let n = N.hijo(this); n; n = N.sig(n)) if (N.tipo(n) === 1) return n; return null; }
  get head() { return N.busca(this, 'head', 1); }
  get body() { return N.cuerpo(); }
  set body(v) {}
  get title() { const t = N.busca(this, 'title', 1); return t ? t.textContent.trim() : ''; }
  set title(v) { let t = N.busca(this, 'title', 1); if (!t && this.head) { t = this.createElement('title'); this.head.appendChild(t); } if (t) t.textContent = S(v); N.titulo(S(v)); }
  get readyState() { return estadoDoc; }
  get currentScript() { return actual; }
  get URL() { return N.url(); }
  get documentURI() { return N.url(); }
  get baseURI() { return N.absoluta(''); }
  get referrer() { return ''; }
  get domain() { return location.hostname; }
  get location() { return location; }
  set location(v) { location.href = v; }
  get defaultView() { return G; }
  get cookie() { return N.galletas(); }
  set cookie(v) { N.ponGalleta(S(v)); }
  get activeElement() { return enfocado || this.body; }
  get hidden() { return false; }
  get visibilityState() { return 'visible'; }
  get characterSet() { return 'UTF-8'; }
  get charset() { return 'UTF-8'; }
  get contentType() { return 'text/html'; }
  get compatMode() { return 'CSS1Compat'; }
  get dir() { return ''; }
  get forms() { return lista(N.busca(this, 'form', 0)); }
  get images() { return lista(N.busca(this, 'img', 0)); }
  get links() { return lista(N.busca(this, 'a[href],area[href]', 0)); }
  get scripts() { return lista(N.busca(this, 'script', 0)); }
  get styleSheets() { return []; }
  get fonts() { return { ready: Promise.resolve(), check() { return true; }, load() { return Promise.resolve([]); }, addEventListener() {} }; }
  get scrollingElement() { return this.documentElement; }
  get fullscreenElement() { return null; }
  get implementation() { return { createHTMLDocument: (t) => docAparte(t), hasFeature: () => true }; }
  getElementById(id) { return N.porId(S(id)); }
  getElementsByName(n) { return lista(N.busca(this, '[name="' + CSS.escape(S(n)) + '"]', 0)); }
  getElementsByTagName(t) { return Element.prototype.getElementsByTagName.call(this, t); }
  getElementsByClassName(c) { return Element.prototype.getElementsByClassName.call(this, c); }
  querySelector(s) { return N.busca(this, S(s), 1); }
  querySelectorAll(s) { return lista(N.busca(this, S(s), 0)); }
  createElement(t) { return N.crea(S(t).toLowerCase()); }
  createElementNS(ns, t) { return N.crea(S(t).toLowerCase()); }
  createTextNode(t) { return N.creaTexto(S(t)); }
  createComment(t) { return N.creaTexto(''); }
  createDocumentFragment() { return new DocumentFragment(); }
  createEvent(t) { return /key/i.test(t) ? new KeyboardEvent('') : /mouse/i.test(t) ? new MouseEvent('') : new CustomEvent(''); }
  createRange() { return { selectNodeContents() {}, setStart() {}, setEnd() {}, collapse() {}, getBoundingClientRect() { return { top: 0, left: 0, width: 0, height: 0 }; },
    createContextualFragment: (h) => { const t = documento.createElement('div'); t.innerHTML = S(h); const f = new DocumentFragment(); while (t.firstChild) f.appendChild(t.firstChild); return f; } }; }
  createTreeWalker(raiz, que) {
    const todos = [raiz, ...N.busca(raiz, '*', 0)];
    let i = 0;
    return { currentNode: raiz, nextNode() { i++; this.currentNode = todos[i] || null; return this.currentNode; } };
  }
  importNode(n, hondo) { return n.cloneNode(hondo); }
  adoptNode(n) { return n; }
  hasFocus() { return true; }
  execCommand() { return false; }
  queryCommandSupported() { return false; }
  getSelection() { return G.getSelection(); }
  elementFromPoint() { return null; }
  open() { return this; } close() {}
  write(t) { const b = this.body; if (b) b.insertAdjacentHTML('beforeend', S(t)); }
  writeln(t) { this.write(S(t) + '\n'); }
  exitFullscreen() { return Promise.resolve(); }
  startViewTransition(f) { if (f) f(); return { finished: Promise.resolve(), ready: Promise.resolve(), updateCallbackDone: Promise.resolve() }; }
}
N.prototipos(HTMLElement.prototype, Text.prototype, Document.prototype);
const documento = N.doc();

/* =============================================================================
 *  window: location, history, temporizadores, cuadros, almacén...
 * ============================================================================= */
function partes(u) {
  const m = /^([a-z][a-z0-9+.-]*:)(?:\/\/(?:[^@\/?#]*@)?(\[[^\]]*\]|[^:\/?#]*)(?::(\d*))?)?([^?#]*)(\?[^#]*)?(#.*)?$/i.exec(u);
  if (!m) throw new TypeError('URL no válida: ' + u);
  return { protocol: m[1].toLowerCase(), hostname: (m[2] || '').toLowerCase(), port: m[3] || '', pathname: m[4] || (m[2] !== undefined ? '/' : ''), search: m[5] && m[5] !== '?' ? m[5] : '', hash: m[6] && m[6] !== '#' ? m[6] : '' };
}
class URLSearchParams {
  constructor(i) {
    oculto(this, '__l', []);
    if (i === undefined || i === null) return;
    if (typeof i === 'string') { this.__lee(i); return; }
    if (i instanceof URLSearchParams || i instanceof FormData || (typeof i === 'object' && typeof i[Symbol.iterator] === 'function')) { for (const [k, v] of i) this.__l.push([S(k), S(v)]); return; }
    for (const k of Object.keys(i)) this.__l.push([k, S(i[k])]);
  }
  __lee(t) {
    this.__l.length = 0;
    t = S(t);
    if (t[0] === '?') t = t.slice(1);
    const des = (x) => { try { return decodeURIComponent(x.replace(/\+/g, ' ')); } catch (e) { return x; } };
    for (const p of t.split('&')) { if (!p) continue; const i = p.indexOf('='); this.__l.push(i < 0 ? [des(p), ''] : [des(p.slice(0, i)), des(p.slice(i + 1))]); }
  }
  get size() { return this.__l.length; }
  append(k, v) { this.__l.push([S(k), S(v)]); this.__cambia(); }
  set(k, v) { k = S(k); const i = this.__l.findIndex(x => x[0] === k); if (i < 0) this.__l.push([k, S(v)]); else { this.__l[i][1] = S(v); this.__l = this.__l.filter((x, j) => j <= i || x[0] !== k); } this.__cambia(); }
  get(k) { const x = this.__l.find(x => x[0] === S(k)); return x ? x[1] : null; }
  getAll(k) { return this.__l.filter(x => x[0] === S(k)).map(x => x[1]); }
  has(k) { return this.__l.some(x => x[0] === S(k)); }
  delete(k) { const n = this.__l.length; const q = this.__l.filter(x => x[0] !== S(k)); this.__l.length = 0; this.__l.push(...q); if (n !== q.length) this.__cambia(); }
  sort() { this.__l.sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0); this.__cambia(); }
  forEach(f, t) { for (const [k, v] of this.__l) f.call(t, v, k, this); }
  keys() { return this.__l.map(x => x[0])[Symbol.iterator](); }
  values() { return this.__l.map(x => x[1])[Symbol.iterator](); }
  entries() { return this.__l.map(x => [x[0], x[1]])[Symbol.iterator](); }
  [Symbol.iterator]() { return this.entries(); }
  toString() {
    const c = (x) => encodeURIComponent(x).replace(/%20/g, '+').replace(/[!'()~]/g, m => '%' + m.charCodeAt(0).toString(16).toUpperCase());
    return this.__l.map(([k, v]) => c(k) + '=' + c(v)).join('&');
  }
  __cambia() { if (this.__url) this.__url.__p.search = this.__l.length ? '?' + this.toString() : ''; }
}
class URL {
  constructor(u, base) {
    u = S(u);
    let abs;
    if (base !== undefined) abs = N.absolutaDe(S(base instanceof URL ? base.href : base), u);
    else abs = /^[a-z][a-z0-9+.-]*:/i.test(u) ? u : null;
    if (abs === null) throw new TypeError('URL no válida: ' + u);
    oculto(this, '__p', partes(abs));
  }
  get protocol() { return this.__p.protocol; } set protocol(v) { this.__p.protocol = S(v).replace(/:?$/, ':'); }
  get hostname() { return this.__p.hostname; } set hostname(v) { this.__p.hostname = S(v); }
  get port() { return this.__p.port; } set port(v) { this.__p.port = S(v); }
  get host() { return this.__p.hostname + (this.__p.port ? ':' + this.__p.port : ''); }
  set host(v) { const [h, p] = S(v).split(':'); this.__p.hostname = h; this.__p.port = p || ''; }
  get origin() { return /^https?:$/.test(this.__p.protocol) ? this.__p.protocol + '//' + this.host : 'null'; }
  get pathname() { return this.__p.pathname; } set pathname(v) { v = S(v); this.__p.pathname = v[0] === '/' ? v : '/' + v; }
  get search() { return this.__p.search; } set search(v) { v = S(v); this.__p.search = v && v !== '?' ? (v[0] === '?' ? v : '?' + v) : ''; if (this.__sp) this.__sp.__lee(this.__p.search); }
  get hash() { return this.__p.hash; } set hash(v) { v = S(v); this.__p.hash = v && v !== '#' ? (v[0] === '#' ? v : '#' + v) : ''; }
  get username() { return ''; } get password() { return ''; }
  get searchParams() { if (!this.__sp) { const s = new URLSearchParams(this.__p.search); oculto(s, '__url', this); oculto(this, '__sp', s); } return this.__sp; }
  get href() { const p = this.__p; return p.protocol + (p.hostname || /^(https?|file):$/.test(p.protocol) ? '//' + this.host : '') + p.pathname + p.search + p.hash; }
  set href(v) { oculto(this, '__p', partes(new URL(v).href)); if (this.__sp) this.__sp.__lee(this.__p.search); }
  toString() { return this.href; }
  toJSON() { return this.href; }
  static canParse(u, b) { try { new URL(u, b); return true; } catch (e) { return false; } }
  static createObjectURL() { return 'blob:nada'; }
  static revokeObjectURL() {}
}
const location = {
  get href() { return N.url(); },
  set href(v) { N.ir(N.absoluta(S(v)), 0); },
  assign(v) { N.ir(N.absoluta(S(v)), 0); },
  replace(v) { N.ir(N.absoluta(S(v)), 1); },
  reload() { N.historia(0); },
  toString() { return N.url(); },
  get ancestorOrigins() { return []; },
};
for (const k of ['protocol', 'host', 'hostname', 'port', 'pathname', 'search', 'hash', 'origin']) {
  Object.defineProperty(location, k, {
    get() { return new URL(N.url())[k]; },
    set(v) {
      const u = new URL(N.url());
      const viejo = u.href;
      u[k] = v;
      if (k === 'hash') { N.ponUrl(u.href); G.dispatchEvent(new HashChangeEvent('hashchange', { oldURL: viejo, newURL: u.href })); return; }
      N.ir(u.href, 0);
    }, enumerable: true
  });
}
const estados = [null];
const history = {
  get length() { return estados.length; },
  get state() { return estados[estados.length - 1]; },
  scrollRestoration: 'auto',
  back() { N.historia(-1); }, forward() { N.historia(1); }, go(n) { N.historia(n | 0); },
  pushState(st, t, u) { estados.push(st === undefined ? null : st); if (u !== undefined && u !== null) N.ponUrl(N.absoluta(S(u))); },
  replaceState(st, t, u) { estados[estados.length - 1] = st === undefined ? null : st; if (u !== undefined && u !== null) N.ponUrl(N.absoluta(S(u))); },
};
// temporizadores: script.c avisa con __tic(id)
const temps = new Map();
let nTemp = 0;
function pide(f, ms, args, cada) {
  const id = ++nTemp;
  ms = Math.max(0, +ms || 0);
  temps.set(id, { f, args, cada: cada ? Math.max(ms, 10) : 0 });
  N.temporiza(id, ms);
  return id;
}
function quita(id) { if (temps.delete(id)) N.desTemporiza(id); }
oculto(G, '__tic', function (id) {
  const t = temps.get(id);
  if (!t) return;
  if (t.cada) N.temporiza(id, t.cada); else temps.delete(id);
  try {
    if (typeof t.f === 'function') t.f.apply(G, t.args);
    else (0, eval)(S(t.f));
  } catch (e) { informa(e); }
});
// el almacén (localStorage, en el disco; sessionStorage, mientras dure el Navegador).
// El origen lo pone C, con la dirección de la página (no location.origin: la
// página podría cambiar URL.prototype y leer el almacén de otro sitio)
class Storage {
  constructor(local) { oculto(this, '__local', local); oculto(this, '__d', null); }
  __datos() {
    if (!this.__d) { let d = {}; try { d = JSON.parse(N.almacen(this.__local) || '{}'); } catch (e) {} oculto(this, '__d', d); }
    return this.__d;
  }
  __guarda() { N.guardaAlmacen(this.__local, JSON.stringify(this.__d)); }
  get length() { return Object.keys(this.__datos()).length; }
  key(i) { return Object.keys(this.__datos())[i] ?? null; }
  getItem(k) { const d = this.__datos(); return Object.prototype.hasOwnProperty.call(d, k) ? d[k] : null; }
  setItem(k, v) { const d = this.__datos(); if (d[S(k)] === S(v)) return; d[S(k)] = S(v); this.__guarda(); }
  removeItem(k) { const d = this.__datos(); if (!(S(k) in d)) return; delete d[S(k)]; this.__guarda(); }
  clear() { oculto(this, '__d', {}); this.__guarda(); }
}
function almacen(local) {
  const s = new Storage(local);
  return new Proxy(s, {
    get(o, p) { if (typeof p === 'symbol' || p in o) { const v = o[p]; return typeof v === 'function' ? v.bind(o) : v; } return o.getItem(p) ?? undefined; },
    set(o, p, v) { o.setItem(p, v); return true; },
    deleteProperty(o, p) { o.removeItem(p); return true; },
    has(o, p) { return p in o || o.getItem(p) !== null; },
    ownKeys(o) { return Object.keys(o.__datos()); },
    getOwnPropertyDescriptor(o, p) { const v = o.getItem(p); return v === null ? undefined : { value: v, writable: true, enumerable: true, configurable: true }; }
  });
}
const localStorage = almacen(1), sessionStorage = almacen(0);
// matchMedia: la ventana del Navegador es una pantalla de ordenador sin ratón
// que pase por encima (sin globos de ayuda), sin animaciones y clara
function casaMedia(q) {
  q = S(q).toLowerCase();
  const w = N.ancho(), h = N.alto();
  return q.split(',').some(parte => {
    parte = parte.trim();
    let no = false;
    if (parte.startsWith('not ')) { no = true; parte = parte.slice(4); }
    const ok = parte.split(/\s+and\s+/).every(c => {
      c = c.trim().replace(/^only\s+/, '');
      if (c === 'screen' || c === 'all' || c === '') return true;
      if (c === 'print' || c === 'speech') return false;
      const m = /^\(\s*([a-z-]+)\s*(?::\s*([^)]*?))?\s*\)$/.exec(c);
      if (!m) return false;
      const v = (m[2] || '').trim(), num = parseFloat(v) * (/em$/.test(v) ? 16 : 1);
      switch (m[1]) {
        case 'min-width': return w >= num;
        case 'max-width': return w <= num;
        case 'min-height': return h >= num;
        case 'max-height': return h <= num;
        case 'orientation': return v === (w >= h ? 'landscape' : 'portrait');
        case 'hover': return v === 'none';
        case 'any-hover': return v === 'none';
        case 'pointer': case 'any-pointer': return v === 'fine';
        case 'prefers-color-scheme': return v === 'light';
        case 'prefers-reduced-motion': return v === 'reduce';
        case 'prefers-contrast': return v === 'no-preference';
        case 'display-mode': return v === 'browser';
        case 'color': return true;
        case 'min-resolution': case 'max-resolution': case '-webkit-min-device-pixel-ratio': return parseFloat(v) <= 1;
        default: return false;
      }
    });
    return no ? !ok : ok;
  });
}
function matchMedia(q) {
  return { matches: casaMedia(q), media: S(q), onchange: null, addListener() {}, removeListener() {}, addEventListener() {}, removeEventListener() {}, dispatchEvent() { return true; } };
}
function getComputedStyle(el) {
  const vis = N.es(el) && N.tipo(el) === 1 ? !N.oculto(el) : true;
  const bloque = vis && N.es(el) && /^(div|p|ul|ol|li|table|form|section|article|header|footer|main|nav|aside|h[1-6]|details|dialog|fieldset|pre|blockquote|body|html|hr)$/.test(N.nombre(el));
  const fijo = { display: vis ? (bloque ? 'block' : 'inline') : 'none', visibility: 'visible', opacity: '1', position: 'static',
    transitionDuration: '0s', transitionDelay: '0s', animationDuration: '0s', animationName: 'none', fontSize: '16px', lineHeight: 'normal',
    width: vis ? '100px' : 'auto', height: vis ? '20px' : 'auto', overflow: 'visible', cursor: 'auto', zIndex: 'auto', direction: 'ltr' };
  return new Proxy({}, {
    get(o, p) {
      if (typeof p === 'symbol') return undefined;
      if (p === 'getPropertyValue') return (n) => { const c = S(n).replace(/-([a-z])/g, (m, l) => l.toUpperCase()); return c in fijo ? fijo[c] : (N.es(el) && N.tipo(el) === 1 ? el.style.getPropertyValue(n) : ''); };
      if (p in fijo) return fijo[p];
      return N.es(el) && N.tipo(el) === 1 ? el.style[p] : '';
    }
  });
}
// la consola, al registro (NAVEGA.LOG)
const formato = (a) => a.map(x => {
  if (typeof x === 'string') return x;
  if (x instanceof Error) return String(x) + (x.stack ? '\n' + x.stack : '');
  if (N.es(x)) return '<' + (N.tipo(x) === 1 ? N.nombre(x) + (x.id ? '#' + x.id : '') : N.tipo(x) === 3 ? '#text' : '#document') + '>';
  try { return typeof x === 'object' && x !== null ? JSON.stringify(x) : String(x); } catch (e) { return String(x); }
}).join(' ');
const cuentas = {}, relojes = {};
const consola = {
  log: (...a) => N.registro(0, formato(a)), info: (...a) => N.registro(0, formato(a)), debug: (...a) => N.registro(0, formato(a)),
  warn: (...a) => N.registro(2, formato(a)), error: (...a) => N.registro(3, formato(a)), trace: (...a) => N.registro(0, formato(a)),
  assert: (c, ...a) => { if (!c) N.registro(3, 'Falla: ' + formato(a)); },
  table: (x) => N.registro(0, formato([x])), dir: (x) => N.registro(0, formato([x])),
  group: () => {}, groupCollapsed: () => {}, groupEnd: () => {}, clear: () => {},
  count: (n = 'default') => { cuentas[n] = (cuentas[n] || 0) + 1; N.registro(0, n + ': ' + cuentas[n]); },
  countReset: (n = 'default') => { cuentas[n] = 0; },
  time: (n = 'default') => { relojes[n] = N.ahora(); },
  timeEnd: (n = 'default') => { N.registro(0, n + ': ' + (N.ahora() - (relojes[n] || 0)) + ' ms'); delete relojes[n]; },
  timeLog: (n = 'default') => { N.registro(0, n + ': ' + (N.ahora() - (relojes[n] || 0)) + ' ms'); },
};

/* =============================================================================
 *  La red: fetch y XMLHttpRequest (sobre http.c, con las galletas)
 * ============================================================================= */
class Headers {
  constructor(i) {
    oculto(this, '__m', new Map());
    if (!i) return;
    if (i instanceof Headers) { for (const [k, v] of i) this.append(k, v); return; }
    if (Array.isArray(i)) { for (const [k, v] of i) this.append(k, v); return; }
    for (const k of Object.keys(i)) this.append(k, i[k]);
  }
  append(k, v) { k = S(k).toLowerCase(); const a = this.__m.get(k); this.__m.set(k, a !== undefined ? a + ', ' + S(v) : S(v)); }
  set(k, v) { this.__m.set(S(k).toLowerCase(), S(v)); }
  get(k) { const v = this.__m.get(S(k).toLowerCase()); return v === undefined ? null : v; }
  has(k) { return this.__m.has(S(k).toLowerCase()); }
  delete(k) { this.__m.delete(S(k).toLowerCase()); }
  forEach(f, t) { for (const [k, v] of this.__m) f.call(t, v, k, this); }
  keys() { return this.__m.keys(); } values() { return this.__m.values(); } entries() { return this.__m.entries(); }
  [Symbol.iterator]() { return this.__m.entries(); }
}
function cabecerasDe(texto) {
  const h = new Headers();
  for (const l of S(texto).split(/\r?\n/).slice(1)) { const i = l.indexOf(':'); if (i > 0) h.append(l.slice(0, i).trim(), l.slice(i + 1).trim()); }
  return h;
}
class Blob {
  constructor(partes, op) { oculto(this, '__t', (partes || []).map(x => typeof x === 'string' ? x : x instanceof Blob ? x.__t : S(x)).join('')); this.type = (op && op.type) || ''; }
  get size() { return this.__t.length; }
  text() { return Promise.resolve(this.__t); }
  slice(a, b, t) { return new Blob([this.__t.slice(a, b)], { type: t }); }
}
class File extends Blob { constructor(p, n, op) { super(p, op); this.name = S(n); this.lastModified = Date.now(); } }
class Response {
  constructor(cuerpo, init) {
    init = init || {};
    this.status = init.status === undefined ? 200 : init.status;
    this.statusText = init.statusText || '';
    this.ok = this.status >= 200 && this.status < 300;
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.url = init.url || '';
    this.redirected = !!init.redirected;
    this.type = 'basic';
    oculto(this, '__c', cuerpo === undefined || cuerpo === null ? '' : S(cuerpo));
    this.bodyUsed = false;
  }
  text() { this.bodyUsed = true; return Promise.resolve(this.__c); }
  json() { this.bodyUsed = true; return new Promise((si, no) => { try { si(JSON.parse(this.__c)); } catch (e) { no(e); } }); }
  blob() { this.bodyUsed = true; return Promise.resolve(new Blob([this.__c], { type: this.headers.get('content-type') || '' })); }
  arrayBuffer() { this.bodyUsed = true; const b = new Uint8Array(this.__c.length); for (let i = 0; i < b.length; i++) b[i] = this.__c.charCodeAt(i) & 255; return Promise.resolve(b.buffer); }
  formData() { return this.text().then(t => { const f = new FormData(); for (const [k, v] of new URLSearchParams(t)) f.append(k, v); return f; }); }
  clone() { return new Response(this.__c, { status: this.status, statusText: this.statusText, headers: this.headers, url: this.url }); }
  static json(d, i) { return new Response(JSON.stringify(d), Object.assign({ headers: { 'content-type': 'application/json' } }, i)); }
  static error() { return new Response('', { status: 0 }); }
}
class Request {
  constructor(u, op) {
    op = op || {};
    if (u instanceof Request) { op = Object.assign({ method: u.method, headers: u.headers, body: u.__b }, op); u = u.url; }
    this.url = N.absoluta(S(u));
    this.method = S(op.method || 'GET').toUpperCase();
    this.headers = new Headers(op.headers);
    this.credentials = op.credentials || 'same-origin';
    this.signal = op.signal || null;
    oculto(this, '__b', op.body);
  }
  clone() { return new Request(this); }
}
class AbortSignal extends EventTarget {
  constructor() { super(); this.aborted = false; this.reason = undefined; this.onabort = null; }
  throwIfAborted() { if (this.aborted) throw this.reason; }
  static timeout(ms) { const c = new AbortController(); setTimeout(() => c.abort(new Error('TimeoutError')), ms); return c.signal; }
  static abort(r) { const c = new AbortController(); c.abort(r); return c.signal; }
}
class AbortController {
  constructor() { this.signal = new AbortSignal(); }
  abort(r) { const s = this.signal; if (s.aborted) return; s.aborted = true; s.reason = r === undefined ? new Error('AbortError') : r; if (s.reason instanceof Error) s.reason.name = 'AbortError'; s.dispatchEvent(new Event('abort')); }
}
class FormData {
  constructor(form, quien) {
    oculto(this, '__l', []);
    if (!form) return;
    for (const e of form.elements) {
      const n = e.name, t = e.type;
      if (!n || e.disabled || N.nombre(e) === 'fieldset' || N.nombre(e) === 'output') continue;
      if ((t === 'submit' || t === 'button' || t === 'image' || t === 'reset') && e !== quien) continue;
      if ((t === 'checkbox' || t === 'radio') && !e.checked) continue;
      if (t === 'file') { this.__l.push([n, new File([], '')]); continue; }
      if (N.nombre(e) === 'select') { for (const o of e.options) if (o.selected) this.__l.push([n, o.value]); continue; }
      this.__l.push([n, e.value]);
    }
  }
  append(k, v, nom) { this.__l.push([S(k), v instanceof Blob ? v : S(v)]); }
  set(k, v) { this.delete(k); this.append(k, v); }
  get(k) { const x = this.__l.find(x => x[0] === S(k)); return x ? x[1] : null; }
  getAll(k) { return this.__l.filter(x => x[0] === S(k)).map(x => x[1]); }
  has(k) { return this.__l.some(x => x[0] === S(k)); }
  delete(k) { const q = this.__l.filter(x => x[0] !== S(k)); this.__l.length = 0; this.__l.push(...q); }
  forEach(f, t) { for (const [k, v] of this.__l) f.call(t, v, k, this); }
  keys() { return this.__l.map(x => x[0])[Symbol.iterator](); }
  values() { return this.__l.map(x => x[1])[Symbol.iterator](); }
  entries() { return this.__l.map(x => [x[0], x[1]])[Symbol.iterator](); }
  [Symbol.iterator]() { return this.entries(); }
}
/* el cuerpo de una petición: texto y su tipo */
function cuerpoDe(b, h) {
  if (b === undefined || b === null) return null;
  if (b instanceof URLSearchParams) { if (!h.has('content-type')) h.set('content-type', 'application/x-www-form-urlencoded;charset=UTF-8'); return b.toString(); }
  if (b instanceof FormData) {
    // (PHP lee igual $_POST de un formulario «urlencoded»; los archivos no se suben)
    if (!h.has('content-type')) h.set('content-type', 'application/x-www-form-urlencoded;charset=UTF-8');
    return new URLSearchParams([...b].map(([k, v]) => [k, v instanceof Blob ? '' : v])).toString();
  }
  if (b instanceof Blob) { if (!h.has('content-type') && b.type) h.set('content-type', b.type); return b.__t; }
  if (!h.has('content-type')) h.set('content-type', 'text/plain;charset=UTF-8');
  return S(b);
}
const pendientes = new Map();
let nPet = 0;
// cred: las galletas (0 omit, 1 same-origin, 2 include). Lo que se deja pedir a
// otro origen y leer de él (CORS) lo decide C (script.c y guion.c, con cors.h)
function pidePet(metodo, url, h, cuerpo, llega, cred) {
  const id = ++nPet;
  let cab = '';
  for (const [k, v] of h) cab += k.replace(/(^|-)([a-z])/g, (m, a, b) => a + b.toUpperCase()) + ': ' + v + '\r\n';
  pendientes.set(id, llega);
  N.pide(id, metodo, url, cab, cuerpo === null ? null : cuerpo, cred === undefined ? 1 : cred);
  return id;
}
oculto(G, '__llega', function (id, estado, url, cab, cuerpo) {
  const f = pendientes.get(id);
  if (!f) return;
  pendientes.delete(id);
  try { f(estado, url, cab, cuerpo); } catch (e) { informa(e); }
});
function fetch(u, op) {
  const r = new Request(u, op);
  return new Promise((si, no) => {
    if (r.signal && r.signal.aborted) { no(r.signal.reason); return; }
    const cuerpo = r.method === 'GET' || r.method === 'HEAD' ? null : cuerpoDe(r.__b, r.headers);
    if (!r.headers.has('accept')) r.headers.set('accept', '*/*');
    const id = pidePet(r.method, r.url, r.headers, cuerpo, (estado, url, cab, texto) => {
      if (!estado) { no(new TypeError('Failed to fetch: ' + texto)); return; }
      const st = /^HTTP\/[\d.]+ \d+ ?(.*)$/m.exec(S(cab));
      si(new Response(texto, { status: estado, statusText: st ? st[1] : '', headers: cabecerasDe(cab), url, redirected: url !== r.url }));
    }, r.credentials === 'include' ? 2 : r.credentials === 'omit' ? 0 : 1);
    if (r.signal) r.signal.addEventListener('abort', () => { if (pendientes.delete(id)) { N.cancela(id); no(r.signal.reason); } });
  });
}
class XMLHttpRequest extends EventTarget {
  constructor() {
    super();
    this.readyState = 0; this.status = 0; this.statusText = ''; this.responseText = ''; this.responseURL = '';
    this.responseType = ''; this.timeout = 0; this.withCredentials = false; this.upload = new EventTarget();
    this.onreadystatechange = null; this.onload = null; this.onerror = null; this.onloadend = null; this.onabort = null; this.ontimeout = null;
    oculto(this, '__h', new Headers()); oculto(this, '__cab', ''); oculto(this, '__id', 0);
  }
  open(m, u) { this.__m = S(m).toUpperCase(); this.__u = N.absoluta(S(u)); this.readyState = 1; this.__ev('readystatechange'); }
  setRequestHeader(k, v) { this.__h.append(k, v); }
  overrideMimeType() {}
  send(b) {
    const cuerpo = this.__m === 'GET' || this.__m === 'HEAD' ? null : cuerpoDe(b, this.__h);
    if (!this.__h.has('accept')) this.__h.set('accept', '*/*');
    this.__id = pidePet(this.__m, this.__u, this.__h, cuerpo, (estado, url, cab, texto) => {
      if (!estado) { this.readyState = 4; this.__ev('readystatechange'); this.__ev('error'); this.__ev('loadend'); return; }
      const st = /^HTTP\/[\d.]+ \d+ ?(.*)$/m.exec(S(cab));
      oculto(this, '__cab', S(cab));
      this.status = estado; this.statusText = st ? st[1] : ''; this.responseURL = url; this.responseText = texto;
      this.readyState = 2; this.__ev('readystatechange');
      this.readyState = 3; this.__ev('readystatechange');
      this.readyState = 4; this.__ev('readystatechange');
      this.__ev('load'); this.__ev('loadend');
    }, this.withCredentials ? 2 : 1);
  }
  abort() { if (this.__id && pendientes.delete(this.__id)) { N.cancela(this.__id); this.readyState = 0; this.__ev('abort'); } }
  get response() {
    if (this.responseType === 'json') { try { return JSON.parse(this.responseText); } catch (e) { return null; } }
    if (this.responseType === 'document') { return null; }
    return this.responseText;
  }
  get responseXML() { return null; }
  getResponseHeader(k) { return cabecerasDe(this.__cab).get(k); }
  getAllResponseHeaders() { return this.__cab.split(/\r?\n/).slice(1).filter(Boolean).join('\r\n') + '\r\n'; }
  __ev(t) { this.dispatchEvent(new ProgressEvent(t)); }
}
for (const [k, v] of Object.entries({ UNSENT: 0, OPENED: 1, HEADERS_RECEIVED: 2, LOADING: 3, DONE: 4 })) { XMLHttpRequest[k] = v; XMLHttpRequest.prototype[k] = v; }
class ProgressEvent extends Event { constructor(t, i) { super(t, i); this.lengthComputable = false; this.loaded = 0; this.total = 0; } }

/* =============================================================================
 *  Lo demás de window
 * ============================================================================= */
const b64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
function btoa(t) {
  t = S(t);
  let r = '';
  for (let i = 0; i < t.length; i += 3) {
    const a = t.charCodeAt(i), b = t.charCodeAt(i + 1), c = t.charCodeAt(i + 2);
    if (a > 255 || b > 255 || c > 255) throw new Error('InvalidCharacterError');
    const n = a << 16 | (b || 0) << 8 | (c || 0);
    r += b64[n >> 18 & 63] + b64[n >> 12 & 63] + (i + 1 < t.length ? b64[n >> 6 & 63] : '=') + (i + 2 < t.length ? b64[n & 63] : '=');
  }
  return r;
}
function atob(t) {
  t = S(t).replace(/[\s=]/g, '');
  let r = '', n = 0, k = 0;
  for (const c of t) {
    const v = b64.indexOf(c);
    if (v < 0) throw new Error('InvalidCharacterError');
    n = n << 6 | v; k += 6;
    if (k >= 8) { k -= 8; r += String.fromCharCode(n >> k & 255); }
  }
  return r;
}
class TextEncoder {
  get encoding() { return 'utf-8'; }
  encode(t) { const s = unescape(encodeURIComponent(S(t))); const b = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) b[i] = s.charCodeAt(i); return b; }
}
class TextDecoder {
  get encoding() { return 'utf-8'; }
  decode(b) { if (!b) return ''; const u = new Uint8Array(b.buffer || b); let s = ''; for (const x of u) s += String.fromCharCode(x); try { return decodeURIComponent(escape(s)); } catch (e) { return s; } }
}
class MutationObserver { constructor(f) {} observe() {} disconnect() {} takeRecords() { return []; } }
class IntersectionObserver {
  constructor(f) { this.__f = f; }
  observe(el) { const f = this.__f; setTimeout(() => f([{ target: el, isIntersecting: true, intersectionRatio: 1, boundingClientRect: el.getBoundingClientRect() }], this), 0); }
  unobserve() {} disconnect() {} takeRecords() { return []; }
}
const CSS = {
  escape: (t) => S(t).replace(/[^\w- -￿]/g, m => '\\' + m).replace(/^(\d)/, '\\3$1 '),
  supports: () => false,
};
const navigator = {
  userAgent: 'HispaNavegador/0.2 (Hispa-486)',
  appName: 'Netscape', appVersion: '5.0 (Hispa-486)', appCodeName: 'Mozilla', product: 'Gecko', vendor: '',
  platform: 'Hispa-486', language: 'es-ES', languages: ['es-ES', 'es'], onLine: true, cookieEnabled: true,
  maxTouchPoints: 0, hardwareConcurrency: 1, deviceMemory: 0.016, doNotTrack: null, webdriver: false, pdfViewerEnabled: false,
  javaEnabled: () => false, sendBeacon: () => false, vibrate: () => false,
  get userAgentData() { return { mobile: false, platform: 'Hispa-486', brands: [] }; },
};
N.agente && (navigator.userAgent = N.agente());
N.idioma && (navigator.language = N.idioma(), navigator.languages = [navigator.language]);
const performance = { now: () => N.ahora(), timeOrigin: Date.now(), mark() {}, measure() {}, getEntriesByType: () => [], getEntriesByName: () => [], timing: {}, navigation: { type: 0 } };
let raf = 0;
const crypto = {
  getRandomValues(a) { for (let i = 0; i < a.length; i++) a[i] = Math.floor(Math.random() * 4294967296); return a; },
  randomUUID() { const h = () => Math.floor(Math.random() * 65536).toString(16).padStart(4, '0'); return h() + h() + '-' + h() + '-4' + h().slice(1) + '-a' + h().slice(1) + '-' + h() + h() + h(); },
};
// Intl: lo poco que hace falta para escribir números y fechas en español
const ES_MES = ['enero', 'febrero', 'marzo', 'abril', 'mayo', 'junio', 'julio', 'agosto', 'septiembre', 'octubre', 'noviembre', 'diciembre'];
const ES_DIA = ['domingo', 'lunes', 'martes', 'miércoles', 'jueves', 'viernes', 'sábado'];
function numeroEs(n, op) {
  op = op || {};
  n = Number(n);
  if (!isFinite(n)) return String(n);
  const moneda = op.style === 'currency', pc = op.style === 'percent';
  if (pc) n *= 100;
  const min = op.minimumFractionDigits ?? (moneda ? 2 : 0), max = Math.max(min, op.maximumFractionDigits ?? (moneda ? 2 : pc ? 0 : 3));
  let t = Math.abs(n).toFixed(max);
  if (max > min) t = t.replace(new RegExp('0{1,' + (max - min) + '}$'), '');
  t = t.replace(/\.$/, '');
  let [e, d] = t.split('.');
  if (op.useGrouping !== false && e.length > 4) e = e.replace(/\B(?=(\d{3})+(?!\d))/g, '.');
  t = (n < 0 ? '-' : '') + e + (d ? ',' + d : '');
  if (moneda) t += ' ' + (op.currency === 'EUR' || !op.currency ? '€' : op.currency);
  if (pc) t += ' %';
  return t;
}
function fechaEs(d, op, hora) {
  op = op || {};
  const dos = (x) => String(x).padStart(2, '0');
  if (isNaN(d)) return 'Invalid Date';
  const quiereF = hora !== 2 && (!op || !(op.hour || op.minute) || op.year || op.month || op.day || op.weekday);
  const quiereH = hora === 2 || (hora === 1 && !(op.year || op.month || op.day || op.weekday)) || op.hour || op.minute;
  let f = '';
  if (quiereF) {
    if (op.month === 'long' || op.month === 'short') {
      const m = op.month === 'long' ? ES_MES[d.getMonth()] : ES_MES[d.getMonth()].slice(0, 3);
      f = (op.weekday ? (op.weekday === 'long' ? ES_DIA[d.getDay()] : ES_DIA[d.getDay()].slice(0, 3)) + ', ' : '') + (op.day ? d.getDate() + ' de ' : '') + m + (op.year ? ' de ' + d.getFullYear() : '');
    } else {
      const day = op.day === '2-digit' ? dos(d.getDate()) : d.getDate(), mon = op.month === '2-digit' ? dos(d.getMonth() + 1) : d.getMonth() + 1;
      f = (op.weekday ? ES_DIA[d.getDay()] + ', ' : '') + (op.day || !Object.keys(op).length ? day + '/' : '') + (op.month || !Object.keys(op).length ? mon : '') + (op.year || !Object.keys(op).length ? '/' + (op.year === '2-digit' ? dos(d.getFullYear() % 100) : d.getFullYear()) : '');
    }
  }
  if (quiereH) f += (f ? ', ' : '') + d.getHours() + ':' + dos(d.getMinutes()) + (op.second || (!op.hour && !op.minute) ? ':' + dos(d.getSeconds()) : '');
  return f;
}
const Intl = {
  NumberFormat: function (l, op) { if (!(this instanceof Intl.NumberFormat)) return new Intl.NumberFormat(l, op); this.format = (n) => numeroEs(n, op); this.formatToParts = (n) => [{ type: 'literal', value: numeroEs(n, op) }]; this.resolvedOptions = () => Object.assign({ locale: 'es-ES' }, op); },
  DateTimeFormat: function (l, op) { if (!(this instanceof Intl.DateTimeFormat)) return new Intl.DateTimeFormat(l, op); this.format = (d) => fechaEs(d === undefined ? new Date() : new Date(d), op, 0); this.formatToParts = (d) => [{ type: 'literal', value: this.format(d) }]; this.resolvedOptions = () => ({ locale: 'es-ES', timeZone: 'Europe/Madrid' }); },
  Collator: function () { if (!(this instanceof Intl.Collator)) return new Intl.Collator(); this.compare = (a, b) => S(a).localeCompare(S(b)); },
  PluralRules: function () { if (!(this instanceof Intl.PluralRules)) return new Intl.PluralRules(); this.select = (n) => n === 1 ? 'one' : 'other'; },
  RelativeTimeFormat: function () { if (!(this instanceof Intl.RelativeTimeFormat)) return new Intl.RelativeTimeFormat(); this.format = (n, u) => (n < 0 ? 'hace ' + -n : 'dentro de ' + n) + ' ' + u; },
  getCanonicalLocales: (l) => [].concat(l || []),
};
def(Number.prototype, { toLocaleString(l, op) { return numeroEs(this, op); } });
def(Date.prototype, {
  toLocaleDateString(l, op) { return fechaEs(this, op, 0); },
  toLocaleTimeString(l, op) { return fechaEs(this, op, 2); },
  toLocaleString(l, op) { return fechaEs(this, op, 1); },
});
def(String.prototype, { localeCompare(o) { const a = S(this).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase(), b = S(o).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase(); return a < b ? -1 : a > b ? 1 : 0; } });

const ventana = {
  window: G, self: G, top: G, parent: G, globalThis: G, frames: G, opener: null, closed: false, frameElement: null, length: 0,
  name: '', status: '', origin: '',
  document: documento, location, history, navigator, localStorage, sessionStorage, performance, crypto, console: consola, CSS, Intl,
  screen: { width: N.ancho(), height: N.alto(), availWidth: N.ancho(), availHeight: N.alto(), colorDepth: 8, pixelDepth: 8, orientation: { type: 'landscape-primary', angle: 0, addEventListener() {} } },
  get innerWidth() { return N.ancho(); }, get innerHeight() { return N.alto(); },
  get outerWidth() { return N.ancho(); }, get outerHeight() { return N.alto(); },
  devicePixelRatio: 1, scrollX: 0, scrollY: 0, pageXOffset: 0, pageYOffset: 0, screenX: 0, screenY: 0, screenLeft: 0, screenTop: 0,
  isSecureContext: true, crossOriginIsolated: false,
  setTimeout: (f, ms, ...a) => pide(f, ms, a, false),
  setInterval: (f, ms, ...a) => pide(f, ms, a, true),
  clearTimeout: quita, clearInterval: quita,
  requestAnimationFrame: (f) => pide(() => f(N.ahora()), 16, [], false),
  cancelAnimationFrame: quita,
  requestIdleCallback: (f) => pide(() => f({ didTimeout: false, timeRemaining: () => 10 }), 1, [], false),
  cancelIdleCallback: quita,
  queueMicrotask: (f) => { Promise.resolve().then(f); },
  alert: (t) => { N.cuadro(0, t === undefined ? '' : S(t)); },
  confirm: (t) => N.cuadro(1, t === undefined ? '' : S(t)),
  prompt: (t, d) => N.pregunta(t === undefined ? '' : S(t), d === undefined ? '' : S(d)),
  print: () => { N.cuadro(0, 'El Navegador no puede imprimir.'); },
  open: (u) => { if (u) location.href = u; return null; },
  close: () => {}, focus: () => {}, blur: () => {}, stop: () => {},
  scrollTo: () => {}, scrollBy: () => {}, scroll: () => {}, moveTo: () => {}, resizeTo: () => {},
  getComputedStyle, matchMedia,
  getSelection: () => ({ toString: () => '', rangeCount: 0, removeAllRanges() {}, addRange() {}, getRangeAt() { return documento.createRange(); }, collapse() {}, selectAllChildren() {} }),
  postMessage: () => {},
  structuredClone: (x) => x === undefined ? undefined : JSON.parse(JSON.stringify(x)),
  btoa, atob, fetch, Headers, Request, Response, FormData, URL, URLSearchParams, Blob, File, AbortController, AbortSignal,
  XMLHttpRequest, TextEncoder, TextDecoder, MutationObserver, IntersectionObserver,
  EventTarget, Event, CustomEvent, UIEvent, MouseEvent, PointerEvent, KeyboardEvent, FocusEvent, InputEvent, SubmitEvent, ErrorEvent,
  PopStateEvent, HashChangeEvent, ProgressEvent,
  Node, Element, HTMLElement, CharacterData, Text, Comment, Document, HTMLDocument: Document, DocumentFragment, DOMTokenList,
  NodeList: Array, HTMLCollection: Array, Storage,
  Image: function (w, h) { const i = documento.createElement('img'); if (w) i.width = w; if (h) i.height = h; return i; },
  Option: function (t, v, d, s) { const o = documento.createElement('option'); if (t !== undefined) o.textContent = t; if (v !== undefined) o.value = v; if (s) o.selected = true; return o; },
  Audio: function () { return documento.createElement('audio'); },
  onerror: null, onload: null, onbeforeunload: null, onunload: null, onpopstate: null, onhashchange: null, onresize: null,
};
for (const k of Object.keys(HTML)) ventana[k] = HTML[k];
for (const k of Object.keys(ventana)) {
  const d = Object.getOwnPropertyDescriptor(ventana, k);
  Object.defineProperty(G, k, Object.assign(d, { configurable: true, enumerable: false }));
}
// window también es un EventTarget
for (const k of ['addEventListener', 'removeEventListener', 'dispatchEvent']) oculto(G, k, EventTarget.prototype[k]);

/* =============================================================================
 *  Lo que llama script.c
 * ============================================================================= */
const TECLAS = { 8: 'Backspace', 9: 'Tab', 13: 'Enter', 27: 'Escape', 32: ' ', 33: 'PageUp', 34: 'PageDown', 35: 'End', 36: 'Home',
  37: 'ArrowLeft', 38: 'ArrowUp', 39: 'ArrowRight', 40: 'ArrowDown', 45: 'Insert', 46: 'Delete',
  112: 'F1', 113: 'F2', 114: 'F3', 115: 'F4', 116: 'F5', 117: 'F6', 118: 'F7', 119: 'F8', 120: 'F9', 121: 'F10', 122: 'F11', 123: 'F12' };
const CODIGOS = { Backspace: 'Backspace', Tab: 'Tab', Enter: 'Enter', Escape: 'Escape', ' ': 'Space' };
let enfocado = null, valorAlEnfocar = '';
/* un evento de la persona (un clic, una tecla, lo escrito): true si la página
 * dice preventDefault */
oculto(G, '__evento', function (el, tipo, vk, ch) {
  let ev;
  const base = { bubbles: true, cancelable: true };
  if (tipo === 'keydown' || tipo === 'keyup' || tipo === 'keypress') {
    const key = ch ? String.fromCharCode(ch) : (TECLAS[vk] || 'Unidentified');
    const code = CODIGOS[key] || (/^[a-z]$/i.test(key) ? 'Key' + key.toUpperCase() : /^\d$/.test(key) ? 'Digit' + key : key);
    ev = new KeyboardEvent(tipo, Object.assign(base, { key, code, keyCode: vk || (ch >= 97 && ch <= 122 ? ch - 32 : ch), charCode: tipo === 'keypress' ? ch : 0 }));
  } else if (tipo === 'click' || tipo === 'mousedown' || tipo === 'mouseup' || tipo === 'dblclick') {
    ev = new (tipo === 'click' ? PointerEvent : MouseEvent)(tipo, Object.assign(base, { detail: 1, buttons: tipo === 'mousedown' ? 1 : 0 }));
  } else if (tipo === 'input') ev = new InputEvent('input', { bubbles: true, data: ch ? String.fromCharCode(ch) : null });
  else if (tipo === 'change') ev = new Event('change', { bubbles: true });
  else if (tipo === 'submit') ev = new SubmitEvent('submit', Object.assign(base, { submitter: vk || null }));
  else if (tipo === 'reset') ev = new Event('reset', base);
  else if (tipo === 'toggle') ev = new Event('toggle');
  else ev = new Event(tipo, base);
  ev.isTrusted = true;
  el = el || documento.body || documento;
  el.dispatchEvent(ev);
  return ev.defaultPrevented;
});
/* el foco va a otro sitio: blur y focusout del de antes (y change si se
 * escribió algo), focus y focusin del nuevo */
oculto(G, '__foco', function (el) {
  if (el === enfocado) return;
  const viejo = enfocado;
  enfocado = el;
  if (viejo) {
    if (/^(input|textarea)$/.test(N.nombre(viejo)) && viejo.value !== valorAlEnfocar && !/^(checkbox|radio)$/.test(viejo.type)) viejo.dispatchEvent(new Event('change', { bubbles: true }));
    viejo.dispatchEvent(new FocusEvent('blur', { relatedTarget: el }));
    viejo.dispatchEvent(new FocusEvent('focusout', { bubbles: true, relatedTarget: el }));
  }
  if (el) {
    valorAlEnfocar = /^(input|textarea|select)$/.test(N.nombre(el)) ? el.value : '';
    el.dispatchEvent(new FocusEvent('focus', { relatedTarget: viejo }));
    el.dispatchEvent(new FocusEvent('focusin', { bubbles: true, relatedTarget: viejo }));
  }
});
oculto(G, '__activo', () => enfocado);
/* los momentos de la carga */
oculto(G, '__guion', function (el) { actual = el; });
oculto(G, '__cargado', function (fase) {
  if (fase === 1) {
    estadoDoc = 'interactive';
    documento.dispatchEvent(new Event('readystatechange'));
    documento.dispatchEvent(new Event('DOMContentLoaded', { bubbles: true }));
  } else if (fase === 2) {
    estadoDoc = 'complete';
    documento.dispatchEvent(new Event('readystatechange'));
    llama(G, new Event('load'), 2);
    llama(G, Object.assign(new Event('pageshow'), { persisted: false }), 2);
  } else if (fase === 3) {
    llama(G, new Event('resize'), 2);
  }
});
oculto(G, '__informa', informa);
oculto(G, '__ev', function (el, tipo) { el.dispatchEvent(new Event(tipo)); });
})
