// motor.js - prueba de HSPJS.DLL (QuickJS en Hispa-486) con JS.EXE: lo que
// usa el JavaScript de ClicGest (ES2020) y las cuentas de Math del x87.
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"use strict";
let fallos = 0;
function igual(a, b, que) {
  if (Object.is(a, b) || (typeof a === 'number' && typeof b === 'number' && Math.abs(a - b) <= Math.abs(b) * 1e-15)) return;
  fallos++;
  print('FALLA ' + que + ': ' + a + ' (debia ser ' + b + ')');
}
// el lenguaje
const f = (a, b = 2) => a * b;
igual(f(3), 6, 'flecha y valor por omision');
class Socio { #n; constructor(n) { this.#n = n; } get nombre() { return `Socia ${this.#n}`; } static de(n) { return new Socio(n); } }
igual(Socio.de('001').nombre, 'Socia 001', 'clase, campo privado y plantilla');
const { a, ...resto } = { a: 1, b: 2, c: 3 };
igual(a + Object.keys(resto).join(''), '1bc', 'desestructurar y resto');
igual([1, [2, [3]]].flat(Infinity).join(), '1,2,3', 'flat');
igual(null ?? 'x', 'x', '??');
igual(({ p: { q: 5 } }).p?.q, 5, '?.');
igual([...new Set([1, 1, 2])].length, 2, 'Set y propagar');
igual(new Map([[1, 'uno']]).get(1), 'uno', 'Map');
igual('Préstamo'.normalize('NFD').length, 9, 'normalize');
igual('ÁRBOL ñandú'.toLowerCase(), 'árbol ñandú', 'minusculas');
igual('socia 017'.replace(/(\d+)/, '[$1]'), 'socia [017]', 'expresion regular');
igual(/^(?<dia>\d\d)\/(?<mes>\d\d)$/.exec('18/11').groups.mes, '11', 'grupos con nombre');
igual(JSON.stringify({ n: 'Ñ', l: [1, 2.5, null] }), '{"n":"Ñ","l":[1,2.5,null]}', 'JSON');
igual(JSON.parse('{"u":"\\/biblioteca\\/prestar?socio=150"}').u, '/biblioteca/prestar?socio=150', 'JSON.parse');
igual(10n ** 20n + 1n, 100000000000000000001n, 'BigInt');
igual((1234.5).toFixed(2), '1234.50', 'toFixed');
igual((0.1 + 0.2).toString(), '0.30000000000000004', '0.1 + 0.2');
igual(String(1e21), '1e+21', 'numero grande');
igual(parseFloat('3.14159abc'), 3.14159, 'parseFloat');
igual((25).toString(36), 'p', 'base 36');
// Math
igual(Math.sqrt(2), 1.4142135623730951, 'sqrt');
igual(Math.sin(Math.PI / 6), 0.49999999999999994, 'sin');
igual(Math.cos(0), 1, 'cos');
igual(Math.tan(Math.PI / 4), 0.9999999999999999, 'tan');
igual(Math.atan2(1, 1), Math.PI / 4, 'atan2');
igual(Math.exp(1), Math.E, 'exp');
igual(Math.log(Math.E), 1, 'log');
igual(Math.log10(1000), 3, 'log10');
igual(Math.log2(8), 3, 'log2');
igual(Math.pow(2, 0.5), Math.SQRT2, 'pow');
igual(2 ** 10, 1024, '**');
igual(Math.pow(-8, 1 / 3), NaN, 'pow de negativo');
igual(Math.cbrt(-27), -3, 'cbrt');
igual(Math.hypot(3, 4), 5, 'hypot');
igual(Math.round(2.5), 3, 'round 2.5');
igual(Math.round(-2.5), -2, 'round -2.5');
igual(Math.round(0.49999999999999994), 0, 'round casi medio');
igual(Math.floor(-1.5), -2, 'floor');
igual(Math.ceil(1.2), 2, 'ceil');
igual(Math.trunc(-4.7), -4, 'trunc');
igual(Math.sign(-3), -1, 'sign');
igual(Math.max(1, 5, 3), 5, 'max');
igual(Object.is(Math.min(0, -0), -0), true, 'min con -0');
igual(Math.asin(1), Math.PI / 2, 'asin');
igual(Math.acos(-1), Math.PI, 'acos');
igual(Math.sinh(1), 1.1752011936438014, 'sinh');
igual(Math.tanh(0.5), 0.46211715726000974, 'tanh');
igual(Math.expm1(1e-10), 1.00000000005e-10, 'expm1');
igual(Math.log1p(1e-10), 9.9999999995e-11, 'log1p');
igual(5 % 3, 2, '%');
igual(-5.5 % 2, -1.5, '% con decimales');
igual(Math.floor(Number.MAX_SAFE_INTEGER / 7), 1286742750677284, 'division');
igual(2 ** 53 % 1000, 992, 'resto grande');
// fechas
const d = new Date(2026, 10, 18, 9, 30);
igual(d.getDate() + '/' + (d.getMonth() + 1) + '/' + d.getFullYear(), '18/11/2026', 'Date');
igual(new Date('2026-11-18T09:30:00Z').getTime(), 1794994200000, 'Date.parse');
igual(d.getDay(), 3, 'dia de la semana');
igual(typeof Date.now(), 'number', 'Date.now');
// promesas y async
let orden = [];
(async () => { orden.push(1); await null; orden.push(3); })();
orden.push(2);
Promise.resolve().then(() => { orden.push(4); });
os.setTimeout(() => {
  igual(orden.join(''), '1234', 'async y then');
  print(fallos ? 'MOTOR-FALLA ' + fallos : 'MOTOR-OK');
}, 10);
