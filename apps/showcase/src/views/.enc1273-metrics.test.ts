import { it } from 'vitest';
import { readFileSync } from 'node:fs';
import { DomainTracker, frameSeries, framingMetrics, plotBox, checkTier2Framing, DEFAULT_PLOT_INSETS } from '@repo/dc-wasm';

it('reports the capture-canvas framing metrics', () => {
  const canvas = { width: 900, height: 497 };
  const cap = JSON.parse(readFileSync(new URL('../../views/candles-aapl/records.json', import.meta.url), 'utf8'));
  const t = new DomainTracker([{ bufferId: 10100, format: 'candle6' }]);
  for (const f of cap.frames) {
    const bin = atob(f.b64); const b = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) b[i] = bin.charCodeAt(i);
    t.observe(b.buffer);
  }
  const d = t.domain();
  const box = plotBox(canvas, DEFAULT_PLOT_INSETS);
  const shipped = { sx: 0.011333333, sy: 0.10625, tx: -0.895333333, ty: -43.88125 };
  const before = framingMetrics({ x: d.x!, y: d.y! }, shipped, box, canvas);
  const after = frameSeries(d, canvas);
  const row = (n: string, m: any) => console.log(
    `${n} deadMargin=${(m.deadMarginFrac * 100).toFixed(1)}% fill=${m.fillRatio.toFixed(3)} minEdge=${m.minEdgeClearancePx.toFixed(1)}px L=${m.edgeClearancePx.left.toFixed(1)} R=${m.edgeClearancePx.right.toFixed(1)} T=${m.edgeClearancePx.top.toFixed(1)} B=${m.edgeClearancePx.bottom.toFixed(1)} gutter=${(m.gutterFrac * 100).toFixed(1)}%`);
  console.log('domain', JSON.stringify(d));
  console.log('box', JSON.stringify(box));
  row('BEFORE', before);
  console.log('BEFORE verdict', JSON.stringify(checkTier2Framing(before)));
  console.log('BEFORE ink', JSON.stringify(before.ink));
  row('AFTER ', after.metrics);
  console.log('AFTER verdict', JSON.stringify(checkTier2Framing(after.metrics!)));
  console.log('AFTER ink', JSON.stringify(after.metrics!.ink));
});
