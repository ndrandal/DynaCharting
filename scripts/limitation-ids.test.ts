// LIMITATIONS.md id guard, wired into the one gate that always runs (ENC-1277).
//
// `pnpm test` needs no Dawn, no GPU and no build, so it is the only check in this
// repo that every contributor actually runs — which is the whole requirement here.
// The default `ctest` was the alternative and it is disqualified by its own entry:
// DC-L01 is about a green run that proves nothing.
//
// The assertions live in scripts/check-limitation-ids.sh; this file runs it and
// also re-states the duplicate check directly, so that neither copy can rot
// silently into agreeing with a broken file.
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { describe, expect, it } from "vitest";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, "..");
const guard = resolve(here, "check-limitation-ids.sh");

function headingIds(): string[] {
  const text = readFileSync(resolve(repoRoot, "LIMITATIONS.md"), "utf8");
  return [...text.matchAll(/^## (DC-L-?\d+)\b/gm)].map((m) => m[1]);
}

describe("LIMITATIONS.md entry ids (ENC-1277)", () => {
  it("no two entries share an id", () => {
    const ids = headingIds();
    expect(ids.length).toBeGreaterThan(0); // a moved heading pattern is not a pass
    const seen = new Map<string, number>();
    for (const id of ids) seen.set(id, (seen.get(id) ?? 0) + 1);
    const dupes = [...seen].filter(([, n]) => n > 1).map(([id, n]) => `${id} x${n}`);
    // Three collisions in two days, twice AUTO-MERGED with no conflict marker:
    // ENC-1252/1257 -> DC-L12, ENC-1250/1254/1256 -> DC-L14, ENC-1251/1253 -> DC-L17.
    expect(dupes, "two entries are wearing one id").toEqual([]);
  });

  it("every id is a closed-space DC-Lnn or a ticket-derived DC-L-<ticket>", () => {
    // §H device 6: DC-L01..DC-L18 is closed; new entries take their ENC number, and
    // the separator is load-bearing — `DC-L1277` would be matched by `grep DC-L12`.
    const malformed = headingIds().filter((id) => !/^DC-L(\d{2}|-\d{3,})$/.test(id));
    expect(malformed, "id is neither DC-Lnn nor DC-L-<ticket>").toEqual([]);
  });

  it("the shell guard agrees (and still runs)", () => {
    // Exit 2 = could not run. That must fail the test, never pass quietly.
    const out = execFileSync("bash", [guard], { encoding: "utf8", cwd: repoRoot });
    expect(out).toMatch(/^ok\s+LIMITATIONS\.md ids are unique/m);
  });
});
