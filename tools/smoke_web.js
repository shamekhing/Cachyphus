// End-to-end check of a staged web build, run in CI so a broken page cannot be
// published. It serves the directory, loads it in Chromium, and fails on the
// three ways a wasm build actually goes wrong:
//
//   1. an exception (GLFW init, missing Module.canvas, async support, ...)
//   2. the canvas never changing -- a black, hung or never-started screen
//   3. the start flow not dismissing the panel
//
// Liveness is checked by comparing two screenshots rather than by decoding
// pixels: a running game always changes frame to frame (the clouds drift), and
// a dead canvas returns byte-identical images. No PNG decoder needed.
//
//   node tools/smoke_web.js <staged-dir> [extra path to visit]
//
const fs = require('fs');
const http = require('http');
const path = require('path');

let chromium;
try {
  ({ chromium } = require('playwright'));
} catch (e) {
  console.error('smoke: playwright is not installed (npm i playwright)');
  process.exit(2);
}

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.nojekyll': 'text/plain',
};

function serve(dir) {
  return new Promise((resolve, reject) => {
    const server = http.createServer((req, res) => {
      const url = decodeURIComponent((req.url || '/').split('?')[0]);
      const file = path.join(dir, url === '/' ? 'index.html' : url);
      if (!file.startsWith(path.resolve(dir))) { res.writeHead(403).end(); return; }
      fs.readFile(file, (err, data) => {
        if (err) { res.writeHead(404).end('not found'); return; }
        res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
        res.end(data);
      });
    });
    server.on('error', reject);
    server.listen(0, '127.0.0.1', () => resolve(server));
  });
}

function fail(msg) { console.error('smoke FAIL: ' + msg); process.exit(1); }

(async () => {
  const dir = process.argv[2];
  if (!dir || !fs.existsSync(path.join(dir, 'index.html'))) {
    fail(`no staged site at ${dir || '(missing argument)'}`);
  }

  const server = await serve(path.resolve(dir));
  const url = `http://127.0.0.1:${server.address().port}/?smoke=1`;
  console.log('smoke: serving ' + dir);
  console.log('smoke: loading ' + url);

  const browser = await chromium.launch({
    headless: true,
    args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader',
           '--autoplay-policy=no-user-gesture-required'],
  });

  const ctx = await browser.newContext({ viewport: { width: 412, height: 915 }, hasTouch: true, isMobile: true });
  const page = await ctx.newPage();

  const errors = [];
  page.on('pageerror', (e) => errors.push('pageerror: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  page.on('requestfailed', (r) =>
    errors.push(`request failed: ${r.url()} (${(r.failure() || {}).errorText})`));

  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(9000);

  const status = await page.textContent('#status').catch(() => '(no #status)');
  const panelErrs = await page.$$eval('.err', (els) => els.map((e) => e.textContent)).catch(() => []);
  console.log('smoke: status = ' + JSON.stringify((status || '').trim()));

  if (errors.length) fail('errors during load:\n  ' + errors.join('\n  '));
  if (panelErrs.length) fail('error panel shown:\n  ' + panelErrs.join('\n  '));
  if (!/ready/i.test(status || '')) fail(`engine never reported ready (status=${status})`);

  // The start panel is opaque and completely covers the canvas, so it has to be
  // dismissed before anything about liveness can be measured -- otherwise this
  // only ever compares two pictures of the same static card.
  const start = page.locator('#start');
  if (!(await start.count())) fail('no start button');
  if (!(await start.isEnabled())) fail('start button never became enabled (engine not ready)');
  const bb = await start.boundingBox();
  await page.touchscreen.tap(bb.x + bb.width / 2, bb.y + bb.height / 2);
  await page.waitForTimeout(1500);
  if (await page.isVisible('#panel')) fail('start button did not dismiss the panel');
  console.log('smoke: start panel dismissed');
  // The preloaded WAV/OGG package and user-gesture audio context must be live.
  const audio = await page.evaluate(() => ({
    packageLoaded: typeof FS !== 'undefined' &&
      FS.analyzePath('/audio/music/money_loop_young.ogg').exists &&
      FS.analyzePath('/audio/sfx/push_01.wav').exists &&
      FS.analyzePath('/audio/ambience/freedom.ogg').exists,
    devices: (window.miniaudio && window.miniaudio.devices || []).length,
    running: (window.miniaudio && window.miniaudio.devices || []).some(
      (d) => d.webaudio && d.webaudio.state === 'running'),
  }));
  if (!audio.packageLoaded || !audio.devices || !audio.running) {
    fail('audio package or Web Audio context unavailable: ' + JSON.stringify(audio));
  }
  console.log('smoke: audio package loaded and Web Audio running');
  const sound = page.locator('#sound');
  if (!(await sound.count())) fail('web mute control missing');
  const sb = await sound.boundingBox();
  await page.touchscreen.tap(sb.x + sb.width / 2, sb.y + sb.height / 2);
  await page.waitForTimeout(120);
  if ((await sound.textContent()).trim() !== 'SOUND OFF') fail('web mute control did not toggle off');
  await page.touchscreen.tap(sb.x + sb.width / 2, sb.y + sb.height / 2);
  await page.waitForTimeout(120);
  if ((await sound.textContent()).trim() !== 'SOUND ON') fail('web mute control did not toggle on');
  console.log('smoke: touch mute control toggled');

  // Liveness: the frame must change over time. A running game always does -- the
  // clouds drift and the title blinks -- while a blank or hung canvas returns
  // byte-identical images.
  const a = await page.screenshot();
  await page.waitForTimeout(900);
  const b = await page.screenshot();
  console.log(`smoke: frame bytes ${a.length} -> ${b.length}`);
  if (a.equals(b)) fail('screen never changed after start: canvas is blank or the main loop is not running');

  if (errors.length) fail('errors after interaction:\n  ' + errors.join('\n  '));

  // --- the page must not stretch the canvas --------------------------------
  // raylib resizes the canvas ELEMENT to the whole window on every resize, so a
  // CSS box of a different shape makes the browser squash one into the other.
  // On a phone-shaped viewport the two shapes are nearly the same and the bug is
  // invisible -- which is how the game shipped crushed to a third of its width
  // on a desktop monitor while every check above still passed. So: measure it on
  // a desktop viewport, and require the display box to have the same shape as
  // the pixels in it.
  const measure = (p) => p.evaluate(() => new Promise((res) => {
    const c = document.getElementById('canvas');
    requestAnimationFrame(() => requestAnimationFrame(() => {
      const r = c.getBoundingClientRect();
      res({ bw: c.width, bh: c.height,
            cw: Math.round(r.width), ch: Math.round(r.height),
            lw: document.documentElement.clientWidth,
            lh: document.documentElement.clientHeight,
            bw2: document.body.clientWidth,
            bh2: document.body.clientHeight });
    }));
  }));

  const checkFit = (tag, g) => {
    console.log(`smoke: ${tag} canvas is ${g.bw}x${g.bh} pixels in a ${g.cw}x${g.ch} box`
                + ` (layout ${g.lw}x${g.lh}, body ${g.bw2}x${g.bh2})`);
    if (!g.bw || !g.bh || !g.cw || !g.ch) fail(`canvas has no size (${tag})`);
    const want = g.bw / g.bh;
    if (Math.abs(g.cw / g.ch - want) / want > 0.02) {
      fail(`canvas is being stretched: ${g.bw}x${g.bh} shown in ${g.cw}x${g.ch} (${tag})`);
    }
    // A dimension of 0 means "unknown" (a stub, not a real layout), so skip it.
    const overflows = (box, layout) => layout > 0 && box > layout;
    if (overflows(g.cw, g.lw) || overflows(g.ch, g.lh) ||
        overflows(g.cw, g.bw2) || overflows(g.ch, g.bh2)) {
      fail(`canvas overflows its box: ${g.cw}x${g.ch} in ${g.lw}x${g.lh} (${tag})`);
    }
  };

  const deskCtx = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  const desk = await deskCtx.newPage();
  await desk.goto(url, { waitUntil: 'load' });
  await desk.waitForTimeout(9000);
  checkFit('desktop', await measure(desk));
  await deskCtx.close();

  // --- a phone-shaped viewport, including the URL bar ----------------------
  // A phone is where the layout viewport moves on its own: showing the URL bar
  // shortens the layout box while the canvas keeps its own pixel size. Sizing the
  // canvas against the visual viewport overflowed that box, and a single-axis
  // clamp in the stylesheet turned the overflow into an 18% squash that nothing
  // corrected. Both halves are checked here, because this is the only place that
  // runs a real browser, and it is the only place that can emulate a phone.
  const phoneCtx = await browser.newContext({
    viewport: { width: 390, height: 844 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true });
  const phone = await phoneCtx.newPage();
  await phone.goto(url, { waitUntil: 'load' });
  await phone.waitForTimeout(9000);
  checkFit('phone', await measure(phone));

  await phone.evaluate(() => {
    document.documentElement.style.height = Math.round(window.innerHeight * 0.85) + 'px';
  });
  checkFit('phone, URL bar showing', await measure(phone));
  await phoneCtx.close();

  console.log('smoke OK');
  await browser.close();
  server.close();
  process.exit(0);
})().catch((e) => { console.error('smoke runner failed: ' + (e && e.stack ? e.stack : e)); process.exit(1); });
