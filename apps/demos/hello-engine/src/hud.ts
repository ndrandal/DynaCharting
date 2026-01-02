import type { EngineStats } from "@repo/engine-host";

export function makeHud() {
  const fpsEl = document.getElementById("fps")!;
  const glEl = document.getElementById("glver")!;
  const memEl = document.getElementById("mem")!;

  const frameEl = document.getElementById("framems")!;
  const drawEl = document.getElementById("drawcalls")!;
  const upEl = document.getElementById("uploadb")!;
  const bufEl = document.getElementById("bufs")!;
  const dbgB = document.getElementById("dbg_bounds")!;
  const dbgW = document.getElementById("dbg_wire")!;

  return {
    setFps: (fps: number) => (fpsEl.textContent = fps.toFixed(1)),
    setGl: (label: string) => (glEl.textContent = label),
    setMem: (label: string) => (memEl.textContent = label),

    setStats: (s: EngineStats) => {
      frameEl.textContent = s.frameMs.toFixed(3);
      drawEl.textContent = String(s.drawCalls);
      upEl.textContent = String(s.uploadedBytesThisFrame);
      bufEl.textContent = String(s.activeBuffers);
      dbgB.textContent = s.debug.showBounds ? "1" : "0";
      dbgW.textContent = s.debug.wireframe ? "1" : "0";
    }
  };
}
