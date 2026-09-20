#!/usr/bin/env node
/* apps/showcase/tools/check-chart-chrome-guards.mjs — ENC-1313
 *
 * THE TWO GUARDS ENC-1313 ADDED, DRIVEN AGAINST THE RUNNING APP.
 *
 * `specs/2026-09-19-chart-quality-bar/harness/deeplink-crash.mjs` already asks
 * the one question that names the regression — *is the app still there?* — and
 * it goes from 11 crashing views to 0. But it is a NEGATIVE test, and a negative
 * test cannot distinguish "the guard works" from "the guard was never reached".
 * A change that merely delayed publishing the 1px bootstrap size would pass it
 * with the bug still latent underneath. So this asks the positive questions:
 *
 *   A. THE GUARD IS REACHED. A cold deep link really does hit `plotBox()` on a
 *      1x1 canvas — and now declines by name instead of throwing. If this ever
 *      starts failing, the harness's green has become a timing accident.
 *   B. …AND THE AXIS IS STILL DRAWN, one commit later, when layout lands. The
 *      fix is "decline this commit", not "never draw".
 *   C. A SUSTAINED degenerate canvas is handled, not just a transient one, and
 *      the decline is VISIBLE — `data-dc-engine-axis-refusal` in the DOM and
 *      `refusal` in `window.__dcEngineAxis`. A silent null would be how §1.3's
 *      caption axes happened again.
 *   D. WIDENING IT BRINGS THE AXIS BACK. The guard is a bound, not a blanket.
 *   E. THE ERROR BOUNDARY CATCHES. `?chromeFault=effect` throws from a passive
 *      effect — the exact shape of the ENC-1313 bug — and the app, the shell and
 *      the canvas all survive while the failure is announced. An error boundary
 *      that has never been SEEN to catch anything is a hope, not a guarantee.
 *   F. A CONTROL with no fault flag: no badge, no boundary record. Otherwise E
 *      is a property of the page rather than of the drill.
 *
 * C squeezes the canvas's own CSS box rather than shrinking the viewport: the
 * showcase's layout has a min-width, so a narrow window leaves the canvas at
 * full size and the check would pass vacuously (measured — it did, first try).
 * Squeezing the element drives the app's REAL `ResizeObserver` → `sizeCanvas` →
 * `setCanvasSize` path.
 *
 * Usage — needs a showcase dev server and a WebGPU Chrome with CDP open; see
 * `specs/2026-09-19-chart-quality-bar/harness/README.md` for the exact flags:
 *
 *   pnpm --filter @repo/showcase dev --port 5954 --strictPort
 *   chrome --headless=new --remote-debugging-port=9786 --enable-unsafe-webgpu \
 *          --enable-features=Vulkan --ignore-gpu-blocklist --use-angle=vulkan \
 *          --user-data-dir="$(mktemp -d)" about:blank
 *   node apps/showcase/tools/check-chart-chrome-guards.mjs --port 9786 --vite-port 5954
 *
 * Exit 0 all green, 1 on a failure, 2 when it could not run — a `could not run`
 * is never reported as a pass (DC-L01's lesson).
 */

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[++i];
}
const PORT = args.port ?? '9222';
const VITE = args['vite-port'] ?? '5173';
/* A view whose axes resolve on the FIRST commit — a literal min/max, no
 * TimeBasis. One of the eleven. NOT `candles-aapl`: the reference chart is a
 * `timestamp` view and is structurally in the surviving half, which is exactly
 * why verifying ENC-1253/ENC-1273 on it could not find this (SPEC §5 Q4). */
const VIEW = args.view ?? 'scatter';
const SETTLE = parseInt(args.settle ?? '9000', 10);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let ws;
try {
  const targets = await (await fetch(`http://localhost:${PORT}/json`)).json();
  const page =
    targets.find((t) => t.type === 'page') ??
    (await (await fetch(`http://localhost:${PORT}/json/new`)).json());
  ws = new WebSocket(page.webSocketDebuggerUrl);
} catch (e) {
  console.error(`CANNOT RUN: no CDP endpoint on localhost:${PORT} — ${e.message}`);
  process.exit(2);
}

let id = 0;
const pending = new Map();
let consoleLines = [];
let exceptions = [];
ws.addEventListener('message', (e) => {
  const m = JSON.parse(e.data);
  if (m.method === 'Runtime.consoleAPICalled') {
    consoleLines.push(m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  }
  if (m.method === 'Runtime.exceptionThrown') {
    exceptions.push(
      String(m.params.exceptionDetails?.exception?.description || '').split('\n')[0],
    );
  }
  if (m.id && pending.has(m.id)) {
    const { r, j } = pending.get(m.id);
    pending.delete(m.id);
    m.error ? j(new Error(JSON.stringify(m.error))) : r(m.result);
  }
});
await new Promise((r, j) => {
  ws.addEventListener('open', r);
  ws.addEventListener('error', () => j(new Error('websocket')));
}).catch(() => {
  console.error('CANNOT RUN: the CDP websocket would not open');
  process.exit(2);
});

const send = (method, params = {}) =>
  new Promise((r, j) => {
    const i = ++id;
    pending.set(i, { r, j });
    ws.send(JSON.stringify({ id: i, method, params }));
  });
const ev = async (x) =>
  (await send('Runtime.evaluate', { expression: x, returnByValue: true, awaitPromise: true }))
    .result?.value;

await send('Page.enable');
await send('Runtime.enable');

let fails = 0;
const check = (name, ok, detail) => {
  console.log(`${ok ? 'ok  ' : 'FAIL'}  ${name}`);
  if (detail) console.log(`        ${detail}`);
  if (!ok) fails++;
};

const open = async (query = '') => {
  await send('Page.navigate', { url: 'about:blank' });
  await sleep(400);
  consoleLines = [];
  exceptions = [];
  await send('Page.navigate', {
    url: `http://localhost:${VITE}/?svgAxis=0${query}#/view/${VIEW}`,
  });
  await sleep(SETTLE);
};

const state = () =>
  ev(
    `(() => { const c = document.querySelector('canvas.engine-canvas');
       const o = document.querySelector('.chrome-overlay');
       const r = (window.__dcEngineAxis||{})[${JSON.stringify(VIEW)}];
       return { root: document.getElementById('root')?.children.length || 0,
                shell: !!document.querySelector('.showcase-root'),
                canvas: c ? c.width + 'x' + c.height : 'none',
                canvasW: c ? c.width : -1,
                domRefusal: o ? o.getAttribute('data-dc-engine-axis-refusal') : null,
                reportRefusal: r?.refusal ? r.refusal.reason + ' / ' + r.refusal.detail : null,
                gridlines: r?.plan?.gridLines?.length ?? -1,
                labels: r?.plan?.labels?.length ?? -1,
                badge: document.querySelector('.chrome-error-badge')?.textContent ?? null,
                appError: document.querySelector('[data-dc-app-error]') ? 'yes' : null,
                boundary: JSON.stringify(window.__dcBoundaryErrors ?? []) }; })()`,
  );

// ── A / B ──────────────────────────────────────────────────────────────────
await open();
const declined = consoleLines.filter((l) => l.includes('engine axis declined'));
check(
  `A  the 1px bootstrap canvas IS reached on a cold deep link to #/view/${VIEW}`,
  declined.some((l) => l.includes('no plot box in 1px')),
  declined[0] ?? '(no "engine axis declined" warning — the guard was not exercised)',
);

const laidOut = await state();
check(
  'B  …and the axis IS drawn once layout lands (refusal cleared, plan populated)',
  laidOut.reportRefusal === null && laidOut.gridlines > 0 && laidOut.labels > 0,
  JSON.stringify(laidOut),
);
check(
  '   no uncaught exception, no error boundary — it did not merely SURVIVE',
  exceptions.length === 0 && laidOut.boundary === '[]',
  `exceptions=${exceptions.join(' | ') || 'none'} boundary=${laidOut.boundary}`,
);

// ── C / D: a SUSTAINED degenerate canvas ───────────────────────────────────
await ev(
  `(() => { const c = document.querySelector('canvas.engine-canvas');
     c.style.width = '40px'; c.style.minWidth = '40px'; c.style.flex = '0 0 40px'; })()`,
);
await sleep(4000);
const squeezed = await state();
check(
  'C  a 40px canvas is REACHED and stays there (not a one-commit transient)',
  squeezed.canvasW > 0 && squeezed.canvasW < 80,
  `canvas = ${squeezed.canvas}`,
);
check(
  '   the app survives it, with no exception and no error boundary',
  squeezed.root > 0 && squeezed.shell && exceptions.length === 0 && squeezed.boundary === '[]',
  `root=${squeezed.root} exceptions=${exceptions.join(' | ') || 'none'} boundary=${squeezed.boundary}`,
);
check(
  '   …and it DECLINES VISIBLY — in the DOM and in the published report',
  !!squeezed.domRefusal &&
    squeezed.domRefusal.includes('plot-box-refused') &&
    (squeezed.reportRefusal ?? '').includes('leave no plot box'),
  `data-dc-engine-axis-refusal = ${squeezed.domRefusal}`,
);

await ev(
  `(() => { const c = document.querySelector('canvas.engine-canvas');
     c.style.width = ''; c.style.minWidth = ''; c.style.flex = ''; })()`,
);
await sleep(4000);
const restored = await state();
check(
  'D  widening it brings the axis back — the guard is a BOUND, not a blanket',
  restored.gridlines > 0 && !restored.domRefusal && restored.reportRefusal === null,
  JSON.stringify({ canvas: restored.canvas, gridlines: restored.gridlines }),
);

// ── E: the error boundary, seen to catch ───────────────────────────────────
for (const mode of ['effect', 'render']) {
  await open(`&chromeFault=${mode}`);
  const o = await state();
  check(
    `E  chromeFault=${mode}: the APP and the SHELL survive a chrome throw`,
    o.root > 0 && o.shell && o.appError === null,
    JSON.stringify({ root: o.root, shell: o.shell, appError: o.appError }),
  );
  check(
    `   chromeFault=${mode}: the CANVAS survives — a chrome failure is not a chart failure`,
    o.canvas !== 'none',
    `canvas = ${o.canvas}`,
  );
  check(
    `   chromeFault=${mode}: caught by the CHROME boundary, recorded, and announced on screen`,
    o.boundary.includes('"boundary":"chrome"') &&
      o.boundary.includes('ENC-1313 drill') &&
      (o.badge ?? '').includes('chart chrome declined'),
    o.boundary.slice(0, 180),
  );
}

// ── F: the control ─────────────────────────────────────────────────────────
await open();
const clean = await state();
check(
  'F  control, no fault flag: no badge and no boundary record',
  clean.badge === null && clean.boundary === '[]',
  JSON.stringify({ badge: clean.badge, boundary: clean.boundary }),
);

console.log(
  `\n${fails === 0 ? 'ALL GUARDS VERIFIED' : `${fails} CHECK(S) FAILED`} ` +
    `(view ${VIEW}, vite ${VITE}, cdp ${PORT})`,
);
ws.close();
process.exit(fails ? 1 : 0);
