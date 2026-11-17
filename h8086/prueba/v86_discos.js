// Prueba de dos disqueteras en el emulador v86 (el de la web): arranca
// Hispa-8086, mete en B: la imagen que se le dé, teclea las órdenes (con la
// distribución de EE. UU.), escribe las últimas líneas de la pantalla tras
// cada una y, al final, guarda el disquete de B: tal como ha quedado.
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//   uso: node v86_discos.js URL playwright chrome imagenB salidaB orden...
const [url, pw, chrome, imgB, salB, ...ordenes] = process.argv.slice(2);
const { chromium } = require(pw);
const fs = require('fs');
async function teclea(p, t) {
  const m = { ' ': 'Space', '.': 'Period', '\n': 'Enter', '\\': 'Backslash', '-': 'Minus', '/': 'Slash' };
  for (const ch of t) {
    if (ch === ':') { await p.keyboard.down('ShiftLeft'); await p.keyboard.press('Semicolon'); await p.keyboard.up('ShiftLeft'); }
    else if (ch === '*') { await p.keyboard.down('ShiftLeft'); await p.keyboard.press('Digit8'); await p.keyboard.up('ShiftLeft'); }
    else if (m[ch]) await p.keyboard.press(m[ch]);
    else if (/[0-9]/.test(ch)) await p.keyboard.press('Digit' + ch);
    else await p.keyboard.press('Key' + ch.toUpperCase());
    await p.waitForTimeout(80);
  }
}
(async () => {
  const b = await chromium.launch({ executablePath: chrome, args: ['--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1280, height: 950 } });
  await p.goto(url + '?x=' + Date.now(), { waitUntil: 'domcontentloaded', timeout: 120000 });
  // (la página carga cosas de fuera que pueden tardar: basta con que esté el emulador)
  await p.waitForFunction(() => typeof window.V86 !== 'undefined' || typeof window.V86Starter !== 'undefined', null, { timeout: 120000 }).catch(() => {});
  await p.waitForTimeout(1500);
  await p.click('.emu-ir[data-sis="8086"]');
  const pantalla = () => p.evaluate(() => document.querySelector('#v86screen .v86txt').innerText);
  for (let i = 0; i < 90; i++) { await p.waitForTimeout(1000); if ((await pantalla()).includes('A:\\>')) break; }
  await p.setInputFiles('#dq-archivo', imgB);
  await p.waitForTimeout(1500);
  await p.click('#v86screen');
  await p.waitForTimeout(500);
  for (const o of ordenes) {
    await teclea(p, o + '\n');
    let antes = '';
    for (let i = 0; i < 40; i++) {            // hasta que la pantalla se quede quieta
      await p.waitForTimeout(1500);
      const t = await pantalla();
      if (t === antes && t.trimEnd().endsWith('>')) break;
      antes = t;
    }
    const l = (await pantalla()).split('\n').map(x => x.trimEnd()).filter(x => x.trim());
    console.log('>> ' + o + ' :: ' + l.slice(-4).join(' | '));
  }
  const datos = await p.evaluate(() => Array.from(window.hispa_emu.get_disk_fdb()));
  fs.writeFileSync(salB, Buffer.from(datos));
  await b.close();
})().catch(e => { console.log('ERROR: ' + e.message.split('\n')[0]); process.exit(1); });
