// Prueba de las teclas de cursor en el emulador v86 (el de la web), con un
// teclado de PC de ahora (las flechas grises, con su prefijo E0h, y las del
// teclado numérico) y con TECLADO cargado: corrige órdenes con las flechas,
// trae las de antes, y mueve el cursor en EDITAR. Escribe «SI texto» o «NO
// texto» por cada renglón que tiene que salir en la pantalla.
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//   uso: node v86_flechas.js URL [ruta de playwright-core] [ruta de Chrome]
const url = process.argv[2];
const { chromium } = require(process.argv[3] || 'playwright-core');
(async () => {
  const b = await chromium.launch({ executablePath: process.argv[4] || '/usr/bin/google-chrome', args: ['--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1280, height: 900 } });
  await p.goto(url + '?x=' + Date.now(), { waitUntil: 'domcontentloaded', timeout: 120000 });
  await p.waitForFunction(() => typeof window.V86 !== 'undefined' || typeof window.V86Starter !== 'undefined', null, { timeout: 120000 }).catch(() => {});
  await p.waitForTimeout(1500);
  await p.click('.emu-ir[data-sis="8086"]');
  const pantalla = () => p.evaluate(() => document.querySelector('#v86screen .v86txt').innerText);
  for (let i = 0; i < 60; i++) { await p.waitForTimeout(1000); if ((await pantalla()).includes('Teclado espa')) break; }
  await p.click('#v86screen');
  await p.waitForTimeout(1500);
  // una tira de teclas: las letras y cifras tal cual; lo demás, por su nombre
  const NOMBRES = { ' ': 'Space', '.': 'Period' };
  const pulsa = async (...teclas) => {
    for (const t of teclas) {
      if (t.length > 1 && !/^[a-z0-9 .]+$/.test(t)) { await p.keyboard.press(t, { delay: 40 }); await p.waitForTimeout(60); continue; }
      for (const c of t) {
        const k = NOMBRES[c] || (/[0-9]/.test(c) ? 'Digit' + c : 'Key' + c.toUpperCase());
        await p.keyboard.press(k, { delay: 40 });
        await p.waitForTimeout(60);
      }
    }
  };
  const mira = async (que, veces, ...renglones) => {
    await p.waitForTimeout(1500);
    const l = (await pantalla()).split('\n').map(x => x.trimEnd());
    for (const r of renglones) console.log((l.filter(x => x === r).length === veces ? 'SI ' : 'NO ') + que + ': ' + r);
    if (process.env.VER_PANTALLA) console.log(l.filter(x => x).map(x => '   | ' + x).join('\n'));   // (para ver qué pasa)
  };
  for (const [que, L, R, U, D, H, E, S] of [
    ['grises', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End', 'Delete'],
    ['numerico', 'Numpad4', 'Numpad6', 'Numpad8', 'Numpad2', 'Numpad7', 'Numpad1', 'NumpadDecimal']]) {
    await pulsa('limpiar', 'Enter');
    await pulsa('eco acd', L, L, 'b', 'Enter');
    await pulsa('co hola', H, 'e', E, 's', 'Enter');
    await pulsa('eco muxndo', L, L, L, L, S, 'Enter');
    await pulsa('eco sl', H, R, R, R, R, R, 'a', 'Enter');
    await mira(que, 1, 'abcd', 'holas', 'mundo', 'sal');
    await pulsa('limpiar', 'Enter', 'eco uno', 'Enter', 'eco dos', 'Enter', U, U, 'Enter', U, U, U, D, 'Enter');
    await mira(que, 2, 'uno', 'dos');
    await pulsa('limpiar', 'Enter', 'editar f.txt', 'Enter');
    await p.waitForTimeout(1500);
    await pulsa('abc', 'Enter', 'def', U, H, 'x', D, E, 'y', L, L, S, H);   // (y al principio: la página parte el renglón por donde está el cursor)
    await mira(que, 1, 'xabc', 'dey');
    await pulsa('F10');
    await p.waitForTimeout(500);
    await pulsa('n');
    await p.waitForTimeout(1500);
  }
  await b.close();
})().catch(e => { console.log('ERROR: ' + e.message); process.exit(1); });
