/* apps/showcase/src/views/growthStride.test.ts — ENC-1452
 *
 * NO EXISTING CALLER IS REFUSED.
 *
 * ENC-1452 made `IndexTimeTracker` decline a source whose `stride` cannot hold
 * a 4-byte bar-ordinal lane at `indexOffset` — `stride > 4` and
 * `stride >= indexOffset + 4` (`packages/dc-wasm/src/chart/time.ts`,
 * `refuseReason`). A guard that refuses a legitimate caller is worse than the
 * hole it closes, so this asserts the other direction against the REAL
 * manifests: every view that actually constructs a tracker registers cleanly.
 *
 * The caller being modelled is the only production one,
 * `useViewSwitch.ts`:
 *
 *   new IndexTimeTracker([{ bufferId: g.bufferId, stride: g.stride,
 *                           indexOffset: g.xField }], false)
 *
 * where `g` is the view's `growth: GrowthSync`. Both `stride` and `xField` are
 * required on that interface (`engine/useReplay.ts`), so there is no absent
 * value to default — every `growth` descriptor in the repo supplies both.
 *
 * NOT A HAND-WRITTEN LIST. The views are discovered with the same
 * `import.meta.glob('../../views/* /manifest.ts', { eager: true })` call
 * `registry.ts` uses, so a view added tomorrow is scored by this without being
 * named in it — and a view whose growth buffer goes to stride 4 turns this red
 * rather than quietly losing its axis.
 */
import { describe, it, expect } from 'vitest';
import { IndexTimeTracker } from '@repo/dc-wasm';
import type { GrowthSync } from '../engine/useReplay';

interface ManifestModule {
  growth?: GrowthSync;
}

const manifestModules = import.meta.glob<ManifestModule>('../../views/*/manifest.ts', {
  eager: true,
});

const viewId = (path: string) => path.split('/').slice(-2)[0];

const withGrowth = Object.entries(manifestModules)
  .map(([path, mod]) => ({ id: viewId(path), growth: mod.growth }))
  .filter((v): v is { id: string; growth: GrowthSync } => v.growth !== undefined)
  .sort((a, b) => a.id.localeCompare(b.id));

describe('every view that fits a time basis survives the ENC-1452 stride guard', () => {
  it('discovered a non-trivial set of views, and some of them have growth', () => {
    // The negative control for the whole file: a glob that resolved to nothing
    // would make every assertion below vacuously true.
    expect(Object.keys(manifestModules).length).toBeGreaterThan(15);
    expect(withGrowth.length).toBeGreaterThan(5);
  });

  it.each(withGrowth.map((v) => [v.id, v.growth] as const))(
    '%s registers with no refusal',
    (id, growth) => {
      const tracker = new IndexTimeTracker(
        [{ bufferId: growth.bufferId, stride: growth.stride, indexOffset: growth.xField }],
        false,
      );
      expect(tracker.refusals, `${id}: stride ${growth.stride} @ ${growth.xField}`).toEqual([]);
      expect(tracker.bufferIds).toEqual([growth.bufferId]);
    },
  );

  it('the real stride contract: every growth buffer is a 16- or 24-byte record at xField 0', () => {
    // The measured contract as of ENC-1452, stated so a change to it is a
    // visible diff rather than a silently-widened guard. 16 = rect4,
    // 24 = candle6 / pos2_color4; the engine's smallest declared stride is 8
    // (pos2_clip) and nothing in this repo declares a stride-4 record format.
    const shapes = withGrowth.map((v) => `${v.id}: ${v.growth.stride}@${v.growth.xField}`);
    for (const v of withGrowth) {
      expect([16, 24], shapes.join(', ')).toContain(v.growth.stride);
      expect(v.growth.xField, `${v.id}`).toBe(0);
      // i.e. the two clauses, stated as the data satisfies them.
      expect(v.growth.stride).toBeGreaterThan(4);
      expect(v.growth.stride).toBeGreaterThanOrEqual(v.growth.xField + 4);
    }
  });
});
