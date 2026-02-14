import { useEffect, useRef, useState, useCallback } from 'react';

type ConnectionStatus = 'connecting' | 'connected' | 'disconnected';

export default function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const [status, setStatus] = useState<ConnectionStatus>('connecting');
  const [fps, setFps] = useState(0);

  // FPS tracking refs
  const frameCountRef = useRef(0);
  const lastFpsTimeRef = useRef(performance.now());

  // Frame protocol state
  const expectingPixelsRef = useRef(false);
  const frameWidthRef = useRef(0);
  const frameHeightRef = useRef(0);

  const connect = useCallback(() => {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${protocol}//${window.location.host}`);
    ws.binaryType = 'arraybuffer';
    wsRef.current = ws;

    ws.onopen = () => {
      setStatus('connected');
    };

    ws.onclose = () => {
      setStatus('disconnected');
      wsRef.current = null;
      // Reconnect after 2 seconds
      setTimeout(connect, 2000);
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (event: MessageEvent) => {
      if (typeof event.data === 'string') {
        const msg = JSON.parse(event.data);
        if (msg.type === 'frame') {
          frameWidthRef.current = msg.width;
          frameHeightRef.current = msg.height;
          expectingPixelsRef.current = true;

          const canvas = canvasRef.current;
          if (canvas) {
            if (canvas.width !== msg.width || canvas.height !== msg.height) {
              canvas.width = msg.width;
              canvas.height = msg.height;
            }
          }
        }
      } else if (expectingPixelsRef.current) {
        expectingPixelsRef.current = false;
        const w = frameWidthRef.current;
        const h = frameHeightRef.current;

        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const arrayBuf = event.data as ArrayBuffer;
        const rgba = new Uint8ClampedArray(arrayBuf);
        const imageData = new ImageData(rgba, w, h);
        ctx.putImageData(imageData, 0, 0);

        // FPS counting
        frameCountRef.current++;
        const now = performance.now();
        const elapsed = now - lastFpsTimeRef.current;
        if (elapsed >= 1000) {
          setFps(Math.round((frameCountRef.current * 1000) / elapsed));
          frameCountRef.current = 0;
          lastFpsTimeRef.current = now;
        }
      }
    };
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (wsRef.current) {
        wsRef.current.onclose = null; // prevent reconnect on unmount
        wsRef.current.close();
      }
    };
  }, [connect]);

  // Mouse and keyboard event handlers
  const draggingRef = useRef(false);

  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.nativeEvent.offsetX, y: e.nativeEvent.offsetY,
      buttons: e.buttons, type: 'down',
    }));
    draggingRef.current = true;
  }, []);

  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!draggingRef.current) return;
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.nativeEvent.offsetX, y: e.nativeEvent.offsetY,
      buttons: e.buttons, type: 'move',
    }));
  }, []);

  const handleMouseUp = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.nativeEvent.offsetX, y: e.nativeEvent.offsetY,
      buttons: 0, type: 'up',
    }));
    draggingRef.current = false;
  }, []);

  const handleWheel = useCallback((e: React.WheelEvent<HTMLCanvasElement>) => {
    e.preventDefault();
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'scroll', x: e.nativeEvent.offsetX, y: e.nativeEvent.offsetY,
      dy: e.deltaY > 0 ? 3 : -3,
    }));
  }, []);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End'].includes(e.key)) {
        e.preventDefault();
        const ws = wsRef.current;
        if (!ws || ws.readyState !== WebSocket.OPEN) return;
        ws.send(JSON.stringify({ cmd: 'key', code: e.key }));
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, []);

  const statusLabel = status === 'connecting' ? 'Connecting...'
    : status === 'connected' ? 'Connected'
    : 'Disconnected';

  return (
    <div className="viewer-container">
      <div className="status-bar">
        <span className="status-indicator">
          <span className={`status-dot ${status}`} />
          {statusLabel}
        </span>
        {status === 'connected' && (
          <span className="fps-counter">{fps} FPS</span>
        )}
      </div>
      <canvas
        ref={canvasRef}
        width={900}
        height={600}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onWheel={handleWheel}
        tabIndex={0}
      />
    </div>
  );
}
