// Prueba de TECLADO en el emulador v86 (el de la web), que lee el puerto del
// teclado de otra manera que QEMU: teclea una frase por los códigos físicos
// de las teclas, deprisa y despacio, y escribe lo que sale en la pantalla.
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//   uso: node v86_teclado.js URL [ruta de playwright-core] [ruta de Chrome]
const url = process.argv[2];
const { chromium } = require(process.argv[3] || 'playwright-core');
// [tecla, modificador]: «Hola ñÑ áé ¿¡ /-():; @/» en un teclado español
const FRASE = [['KeyE'], ['KeyC'], ['KeyO'], ['Space'], ['KeyH', 'ShiftLeft'], ['KeyO'], ['KeyL'], ['KeyA'], ['Space'],
  ['Semicolon'], ['Semicolon', 'ShiftLeft'], ['Space'], ['Quote'], ['KeyA'], ['Quote'], ['KeyE'], ['Space'],
  ['Equal', 'ShiftLeft'], ['Equal'], ['Space'], ['Digit7', 'ShiftLeft'], ['Slash'], ['Digit8', 'ShiftLeft'],
  ['Digit9', 'ShiftLeft'], ['Period', 'ShiftLeft'], ['Comma', 'ShiftLeft'], ['Space'], ['Digit2', 'AltRight'],
  ['NumpadDivide'], ['Enter']];
(async () => {
  const b = await chromium.launch({ executablePath: process.argv[4] || '/usr/bin/google-chrome', args: ['--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1280, height: 900 } });
  await p.goto(url + '?x=' + Date.now(), { waitUntil: 'domcontentloaded', timeout: 120000 });
  // (la página carga cosas de fuera que pueden tardar: basta con que esté el emulador)
  await p.waitForFunction(() => typeof window.V86 !== 'undefined' || typeof window.V86Starter !== 'undefined', null, { timeout: 120000 }).catch(() => {});
  await p.waitForTimeout(1500);
  await p.click('.emu-ir[data-sis="8086"]');
  const pantalla = () => p.evaluate(() => document.querySelector('#v86screen .v86txt').innerText);
  for (let i = 0; i < 60; i++) { await p.waitForTimeout(1000); if ((await pantalla()).includes('Teclado espa')) break; }
  await p.click('#v86screen');
  await p.waitForTimeout(1500);
  for (const [nombre, ms] of [['DEPRISA', 30], ['DESPACIO', 150]]) {
    for (const [k, m] of FRASE) {
      if (m) await p.keyboard.down(m);
      await p.keyboard.press(k, { delay: ms });
      if (m) await p.keyboard.up(m);
      await p.waitForTimeout(ms);
    }
    await p.waitForTimeout(1500);
    const l = (await pantalla()).split('\n').map(x => x.trimEnd()).filter(x => x.startsWith('Hola'));
    console.log(nombre + ': ' + (l.length ? l[l.length - 1] : '(nada)'));
  }
  await b.close();
})().catch(e => { console.log('ERROR: ' + e.message); process.exit(1); });
