// Exercises the input bridge in web/shell.html without a browser, by loading
// the generated page's inline script against a minimal DOM stub and driving
// synthetic events at it.
//
// Two behaviours are covered, neither of which can be checked by reading the
// code:
//
//   touch  a tap becomes SPACE and a press-and-hold becomes SHIFT. The whole
//          thing is a timer, and getting it wrong gives either no push or a
//          brace key that stays stuck down.
//   mouse  a button released OUTSIDE the canvas is re-dispatched to it. The
//          engine polls the button every frame, so a release it never sees
//          leaves the brace on for ever.
//
// Run it against a *built* page:
//
//     node web/test_touch.js build-web/cashyphus.html
//
const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync(process.argv[2], 'utf8');
const blocks = [...html.matchAll(/<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/g)]
  .map((m) => m[1]);
if (blocks.length !== 1) {
  console.error(`expected 1 inline script block, found ${blocks.length}`);
  process.exit(2);
}

// --- minimal DOM -----------------------------------------------------------
function makeEl(id) {
  const classes = new Set();
  return {
    id,
    textContent: '',
    classList: {
      add: (c) => classes.add(c),
      remove: (c) => classes.delete(c),
      contains: (c) => classes.has(c),
    },
    addEventListener() {},
    appendChild() {},
    remove() {},
    querySelector: () => null,
    requestFullscreen: () => Promise.resolve(),
  };
}

const els = {
  panel: makeEl('panel'),
  status: makeEl('status'),
  canvas: makeEl('canvas'),
  start: makeEl('start'),
};
els.status.textContent = 'loading engine...';

const docListeners = {};
global.document = {
  getElementById: (id) => els[id] || null,
  createElement: () => makeEl('div'),
  addEventListener: (t, f) => (docListeners[t] ||= []).push(f),
  documentElement: { requestFullscreen: () => Promise.resolve() },
};

const sent = [];              // every key event the shell dispatches
const winListeners = {};
global.window = {
  addEventListener: (t, f) => (winListeners[t] ||= []).push(f),
  dispatchEvent: (ev) => {
    sent.push(`${ev.type}:${ev.code}`);
    for (const f of winListeners[ev.type] || []) f(ev);
    return true;
  },
};
global.screen = {};
global.KeyboardEvent = class {
  constructor(type, init) { this.type = type; Object.assign(this, init); }
};

// The mouse safety net needs a canvas that can receive events, and a MouseEvent
// to hand to it, so both must exist before the shell script registers anything.
els.canvas.dispatched = [];
els.canvas.dispatchEvent = function (ev) { els.canvas.dispatched.push(ev); return true; };
global.MouseEvent = class {
  constructor(type, init) { this.type = type; Object.assign(this, init); }
};

vm.runInThisContext(blocks[0], { filename: 'shell-inline.js' });

// --- driving ---------------------------------------------------------------
// The touch handlers deliberately ignore touches while the start panel is up,
// so dismiss it first the way the button would.
els.panel.classList.add('hidden');

function fire(type, id) {
  for (const f of docListeners[type] || []) {
    f({ preventDefault() {}, changedTouches: [{ identifier: id }] });
  }
}

// Same, for the plain DOM events the mouse bridge listens for.
function fireDom(type, ev) {
  const fns = docListeners[type] || [];
  for (const f of fns) f(ev);
  return fns.length;
}

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

function check(label, got, want) {
  const ok = JSON.stringify(got) === JSON.stringify(want);
  console.log(`  ${ok ? 'ok  ' : 'FAIL'} ${label}: ${JSON.stringify(got)}`);
  if (!ok) {
    console.log(`       expected ${JSON.stringify(want)}`);
    process.exitCode = 1;
  }
}

(async () => {
  // A quick tap must produce exactly one push and no brace.
  sent.length = 0;
  fire('touchstart', 1);
  await wait(60);                       // shorter than HOLD_MS
  fire('touchend', 1);
  await wait(150);                      // let the push's keyup land
  check('tap -> single push', sent, ['keydown:Space', 'keyup:Space']);

  // A sustained press must brace, and must NOT also push.
  sent.length = 0;
  fire('touchstart', 1);
  await wait(260);                      // longer than HOLD_MS
  check('hold -> brace begins', sent, ['keydown:ShiftLeft']);
  fire('touchend', 1);
  await wait(120);
  check('hold -> brace ends', sent, ['keydown:ShiftLeft', 'keyup:ShiftLeft']);

  // A cancelled touch must not leave the brace key stuck down.
  sent.length = 0;
  fire('touchstart', 1);
  await wait(260);
  fire('touchcancel', 1);
  await wait(120);
  check('cancel -> brace released',
        sent, ['keydown:ShiftLeft', 'keyup:ShiftLeft']);

  // --- mouse: a release outside the canvas still has to reach the engine ----
  els.canvas.dispatched.length = 0;
  const bar = makeEl('letterbox');
  fireDom('mouseup', { target: bar, button: 2, buttons: 0, clientX: 7, clientY: 8 });
  check('release off-canvas is re-dispatched',
        els.canvas.dispatched.map((e) => `${e.type}:${e.button}`), ['mouseup:2']);

  els.canvas.dispatched.length = 0;
  fireDom('mouseup', { target: els.canvas, button: 2, buttons: 0 });
  check('release on-canvas is not duplicated',
        els.canvas.dispatched.map((e) => `${e.type}:${e.button}`), []);

  // --- mouse: the browser menu must not interrupt a held right-click --------
  let prevented = 0;
  const ctx = { preventDefault() { ++prevented; } };
  fireDom('contextmenu', ctx);
  check('context menu suppressed during play', prevented, 1);

  els.panel.classList.remove('hidden');          // loading / failure panel
  prevented = 0;
  fireDom('contextmenu', ctx);
  check('context menu left alone on the panel', prevented, 0);
  els.panel.classList.add('hidden');

  console.log(process.exitCode ? '\ninput bridge: FAILED' : '\ninput bridge: all good');
})();
