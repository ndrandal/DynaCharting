import { it } from "vitest";
import { readFileSync } from "node:fs";
import { parsesAsTimestamp, classifyTimestampLabel } from "./time";
it("ENC-1403 probe dump", () => {
  const lines = readFileSync("/tmp/ENC-1403-AlwAzj/probe.txt", "utf8").split("\n").filter((s) => s.length > 0);
  const out = lines.map((l) => `${l}\t${parsesAsTimestamp(l)}\t${classifyTimestampLabel(l)}`).join("\n");
  console.log("PROBE_BEGIN\n" + out + "\nPROBE_END");
});
