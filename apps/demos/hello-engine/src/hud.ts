export function makeHud() {
  const fpsEl = document.getElementById("fps")!;
  const glEl = document.getElementById("glver")!;
  const memEl = document.getElementById("mem")!;

  return {
    setFps: (fps: number) => (fpsEl.textContent = fps.toFixed(1)),
    setGl: (label: string) => (glEl.textContent = label),
    setMem: (label: string) => (memEl.textContent = label)
  };
}
