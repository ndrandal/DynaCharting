// What the committed dc_engine_host.wasm lets you measure — ENC-1112 / LIMITATIONS.md §C7.
//
// The authoritative census needs emsdk (packages/dc-wasm/scripts/wasm-census.sh, and
// DC-L-1112 for the numbers). This file pins the one thing the census REST ON that
// costs no toolchain at all: the committed module has **no `name` custom section**,
// so it has no symbol table, so `strings | grep` can only ever see string literals.
//
// That is the premise of §C7, and it is the kind of premise that goes stale silently:
// the day someone links with `-g` or `--profiling-funcs`, `strings` starts finding
// C++ symbols, every `strings`-based claim in this repo changes meaning, and nothing
// would have said so. These assertions fail in that case.
//
// `pnpm test` needs no Dawn, no GPU and no build, which is why the guard lives here
// (the same reasoning as scripts/limitation-ids.test.ts).
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { describe, expect, it } from "vitest";

const here = dirname(fileURLToPath(import.meta.url));
const wasmPath = resolve(here, "dc_engine_host.wasm");
const bytes = new Uint8Array(readFileSync(wasmPath));

/** Section id 0 is a custom section, whose payload begins with its own name. */
function sections(buf: Uint8Array): { id: number; name: string; size: number }[] {
  const out: { id: number; name: string; size: number }[] = [];
  let p = 8; // magic (4) + version (4)
  const leb = () => {
    let v = 0;
    let shift = 0;
    for (;;) {
      const b = buf[p++];
      v |= (b & 0x7f) << shift;
      if ((b & 0x80) === 0) return v;
      shift += 7;
    }
  };
  while (p < buf.length) {
    const id = buf[p++];
    const size = leb();
    const end = p + size;
    let name = "";
    if (id === 0) {
      const nameLen = leb();
      name = new TextDecoder().decode(buf.subarray(p, p + nameLen));
    }
    out.push({ id, name, size });
    p = end;
  }
  return out;
}

/** Every printable-ASCII run of >= 4 chars, i.e. what `strings` would report. */
function literals(buf: Uint8Array): string {
  let run = 0;
  const parts: string[] = [];
  for (let i = 0; i < buf.length; i++) {
    const c = buf[i];
    if (c >= 0x20 && c < 0x7f) {
      run++;
    } else {
      if (run >= 4) parts.push(new TextDecoder().decode(buf.subarray(i - run, i)));
      run = 0;
    }
  }
  if (run >= 4) parts.push(new TextDecoder().decode(buf.subarray(buf.length - run)));
  return parts.join("\n");
}

describe("the committed dc_engine_host.wasm — what is measurable from it (ENC-1112)", () => {
  const secs = sections(bytes);
  const text = literals(bytes);

  it("is a wasm module, parsed far enough for the assertions below to mean anything", () => {
    expect([...bytes.subarray(0, 4)]).toEqual([0x00, 0x61, 0x73, 0x6d]); // \0asm
    // Positive control for the parser itself: CODE (10) and DATA (11) must be there.
    const ids = secs.map((s) => s.id);
    expect(ids, "section walk did not reach CODE/DATA — the parser is wrong, not the file").toContain(10);
    expect(ids).toContain(11);
  });

  it("has NO `name` custom section, so it carries no symbol table", () => {
    // If this fails, a debug/profiling link shipped. That is not necessarily wrong —
    // but §C7, DC-L-1112 and DC-L08's `strings` re-check all change meaning, so
    // restamp them in the same PR.
    const custom = secs.filter((s) => s.id === 0).map((s) => s.name);
    expect(custom).not.toContain("name");
    expect(custom, "unexpected custom sections — re-read §C7 before trusting any `strings` claim")
      .toEqual(["target_features"]);
  });

  it("`strings` sees EMBIND NAMES, not code — and is wrong in both directions (§C7)", () => {
    // Bound method names ARE literals: this is all ENC-984's 0 -> 1 ever measured.
    expect(text).toMatch(/\bgetSceneDocument\b/);
    expect(text).toMatch(/\bapplyControl\b/);
    // …while the C++ function getSceneDocument actually calls is in the module and
    // leaves no literal at all. FALSE NEGATIVE.
    expect(text).not.toMatch(/serializeSceneDocument/);
    expect(text).not.toMatch(/sceneToDocument/);
    // And "encode" matches 20+ WebGPU command-encoder import names with no encode
    // pass present anywhere. FALSE POSITIVE.
    const encodeHits = text.split("\n").filter((l) => /encode/i.test(l)).length;
    expect(encodeHits).toBeGreaterThan(10);
    expect(text).not.toMatch(/EncodePass/);
  });

  it("names the things DC-L-1112 says are absent, nowhere in the bytes", () => {
    for (const absent of ["EncodePass", "markSpecOf", "treemap", "LinearScale", "DChartFileIO"]) {
      expect(text, `${absent} appeared as a literal — re-run wasm-census.sh and restamp DC-L-1112`)
        .not.toContain(absent);
    }
  });
});
