import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

const canvas = document.getElementById("c") as HTMLCanvasElement;

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);
host.start();

// Optional: expose for manual testing
(globalThis as any).__host = host;
