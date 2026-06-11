/* apps/showcase/src/engine/useAgentStream.ts
 *
 * Inert WebSocket data-plane hook stub. In a later phase the showcase will
 * connect to an embassy-style agent (VITE_SHOWCASE_AGENT_URL) and feed the
 * binary records it streams straight into host.enqueueData. For the T0.3 render
 * shell this is intentionally a no-op unless the env var is set, so the static
 * sample manifest renders with zero network dependency.
 */

import { useEffect } from 'react';
import type { EngineHost } from '@repo/dc-wasm';

const AGENT_URL = import.meta.env.VITE_SHOWCASE_AGENT_URL as string | undefined;

/**
 * When VITE_SHOWCASE_AGENT_URL is set, open a WS, set binaryType=arraybuffer,
 * and forward each binary message into the engine's data plane. When unset (the
 * default), do nothing. Reconnect logic is deliberately omitted for now.
 */
export function useAgentStream(host: EngineHost | null): void {
  useEffect(() => {
    if (!host || !AGENT_URL) return;

    let ws: WebSocket | null = null;
    try {
      ws = new WebSocket(AGENT_URL);
      ws.binaryType = 'arraybuffer';
      ws.onmessage = (ev: MessageEvent) => {
        if (ev.data instanceof ArrayBuffer) {
          host.enqueueData(ev.data);
        }
      };
      ws.onerror = () => {
        console.warn('[showcase] agent stream error');
      };
    } catch (e) {
      console.warn('[showcase] agent stream failed to open:', (e as Error).message);
    }

    return () => {
      ws?.close();
    };
  }, [host]);
}
