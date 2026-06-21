// ENC-701 (G5b): applyControl rejections must be SURFACED, not silently
// swallowed. Previously a rejected control (e.g. ID_TAKEN, bad bind) was pushed
// onto lastErrors and dropped by callers. Now it fires onControlRejected, or
// console.warn by default, and is still recorded on getLastErrors().

import { describe, it, expect, vi, afterEach } from "vitest";
import { EngineHost } from "./EngineHost";

/** A fake core whose applyControl rejects with a fixed error. */
function makeRejectingCore(error: string) {
  return {
    applyControl: vi.fn(() => ({ ok: false, error })),
  };
}

/** Put a host into the "ready, direct-apply" state so applyControl is not
 *  buffered (needs ready + core + not rendering + empty pendingControl). */
function readyHost(host: EngineHost, core: unknown) {
  const h = host as unknown as Record<string, unknown>;
  h.ready = true;
  h.core = core;
  h.rendering = false;
  h.pendingControl = [];
}

afterEach(() => {
  vi.restoreAllMocks();
});

describe("EngineHost applyControl rejection surfacing (ENC-701)", () => {
  it("invokes onControlRejected with the failing command + message", () => {
    const onControlRejected = vi.fn();
    const host = new EngineHost({ onControlRejected });
    const core = makeRejectingCore("ID_TAKEN");
    readyHost(host, core);

    const cmd = { cmd: "createPane", id: 1 };
    const res = host.applyControl(cmd);

    expect(res).toEqual({ ok: false, error: "ID_TAKEN" });
    expect(onControlRejected).toHaveBeenCalledTimes(1);
    expect(onControlRejected).toHaveBeenCalledWith({
      code: "CONTROL_REJECTED",
      message: "ID_TAKEN",
      command: JSON.stringify(cmd),
    });
  });

  it("console.warns by default when no callback is provided", () => {
    const warn = vi.spyOn(console, "warn").mockImplementation(() => {});
    const host = new EngineHost();
    readyHost(host, makeRejectingCore("VALIDATION_BAD_VERTEX_COUNT"));

    host.applyControl({ cmd: "createGeometry", id: 9, vertexCount: 0 });

    expect(warn).toHaveBeenCalledTimes(1);
    expect(String(warn.mock.calls[0][0])).toContain("applyControl rejected");
    expect(String(warn.mock.calls[0][0])).toContain("VALIDATION_BAD_VERTEX_COUNT");
  });

  it("still records the rejection on getLastErrors()", () => {
    const host = new EngineHost({ onControlRejected: () => {} });
    readyHost(host, makeRejectingCore("ID_TAKEN"));

    host.applyControl({ cmd: "createLayer", id: 2, paneId: 1 });

    const errs = host.getLastErrors();
    expect(errs.some((e) => e.code === "CONTROL_REJECTED" && e.message === "ID_TAKEN")).toBe(true);
  });

  it("does not warn or call back when the command succeeds", () => {
    const warn = vi.spyOn(console, "warn").mockImplementation(() => {});
    const onControlRejected = vi.fn();
    const host = new EngineHost({ onControlRejected });
    readyHost(host, { applyControl: vi.fn(() => ({ ok: true })) });

    const res = host.applyControl({ cmd: "createPane", id: 1 });

    expect(res).toEqual({ ok: true });
    expect(onControlRejected).not.toHaveBeenCalled();
    expect(warn).not.toHaveBeenCalled();
  });
});
