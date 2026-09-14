// ENC-984: EngineHost.getSceneDocument() — the TS half of the SceneDocument
// export. The C++ side is pinned by dc_enc984_scene_export (round trip) and by
// scripts/validate-node.mjs (the real WASM module); what is left to pin here is
// the wrapper's contract, which is exactly the two rules getBufferBytes follows
// and which nothing else enforces:
//
//   * it must NOT enter the WASM module while a render is in flight — the
//     ASYNCIFY runtime is parked mid-await and any entry aborts it with
//     "multiple async operations in flight";
//   * it must pass the compact flag through, defaulting to false.
//
// A fake core stands in for the module (no WASM, no GPU) so these are pure
// contract assertions.

import { describe, it, expect, vi } from "vitest";
import { EngineHost } from "./EngineHost";

const DOC = '{"version":1,"panes":{"1":{"name":"P"}}}';

function harness() {
  const getSceneDocument = vi.fn((compact: boolean) =>
    compact ? '{"version":1}' : DOC,
  );
  const host = new EngineHost();
  (host as unknown as Record<string, unknown>).core = { getSceneDocument };
  return { host, getSceneDocument };
}

describe("EngineHost.getSceneDocument (ENC-984)", () => {
  it("returns the document the core serializes", () => {
    const { host, getSceneDocument } = harness();
    expect(host.getSceneDocument()).toBe(DOC);
    expect(getSceneDocument).toHaveBeenCalledWith(false);
  });

  it("passes the compact flag through", () => {
    const { host, getSceneDocument } = harness();
    expect(host.getSceneDocument(true)).toBe('{"version":1}');
    expect(getSceneDocument).toHaveBeenCalledWith(true);
  });

  it("returns '' and does NOT enter the core when a render is in flight", () => {
    const { host, getSceneDocument } = harness();
    (host as unknown as Record<string, unknown>).rendering = true;
    expect(host.getSceneDocument()).toBe("");
    // The important half: entering the module here would abort the ASYNCIFY
    // render, so the guard must short-circuit BEFORE the call, not after.
    expect(getSceneDocument).not.toHaveBeenCalled();
  });

  it("returns '' before the core exists", () => {
    const host = new EngineHost();
    expect(host.getSceneDocument()).toBe("");
  });
});
