import { spawn } from 'child_process';
import { createServer } from 'http';
import { readFileSync, existsSync } from 'fs';
import { WebSocketServer } from 'ws';
import { fileURLToPath } from 'url';
import { dirname, join, resolve } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(__dirname, '../..');

// ---------- Find the C++ binary ----------

const cppBinary = process.env.DC_BINARY
  ? resolve(ROOT, process.env.DC_BINARY)
  : join(ROOT, 'build/core/dc_live_server');
if (!existsSync(cppBinary)) {
  console.error('C++ binary not found at', cppBinary);
  console.error('Run: cmake --build build');
  process.exit(1);
}

// ---------- Spawn C++ renderer ----------

const cpp = spawn(cppBinary, [], {
  cwd: ROOT, // so it can find third_party/test_font.ttf
  stdio: ['pipe', 'pipe', 'inherit'], // stdin=pipe, stdout=pipe, stderr=inherit
});

console.log('C++ renderer started (PID:', cpp.pid, ')');

// ---------- Frame reading state machine ----------

let frameBuffer = Buffer.alloc(0);
let state = 'sync'; // 'sync' | 'frame' | 'text'
let currentWidth = 0;
let currentHeight = 0;
let expectedBytes = 0;
let textLength = 0;
let latestTextJson = null;
const clients = new Set();

function processStdout(chunk) {
  frameBuffer = Buffer.concat([frameBuffer, chunk]);

  while (true) {
    if (state === 'sync') {
      // Need at least 4 bytes to check magic
      if (frameBuffer.length < 4) break;

      const magic = frameBuffer.toString('ascii', 0, 4);

      if (magic === 'TEXT') {
        if (frameBuffer.length < 8) break;
        textLength = frameBuffer.readUInt32LE(4);
        frameBuffer = frameBuffer.subarray(8);
        state = 'text';
      } else if (magic === 'FRME') {
        if (frameBuffer.length < 12) break;
        currentWidth = frameBuffer.readUInt32LE(4);
        currentHeight = frameBuffer.readUInt32LE(8);
        expectedBytes = currentWidth * currentHeight * 4;
        frameBuffer = frameBuffer.subarray(12);
        state = 'frame';
      } else {
        // Skip one byte and try again (resync)
        frameBuffer = frameBuffer.subarray(1);
        continue;
      }
    }

    if (state === 'text') {
      if (frameBuffer.length < textLength) break;
      latestTextJson = frameBuffer.subarray(0, textLength).toString('utf8');
      frameBuffer = frameBuffer.subarray(textLength);
      state = 'sync';
      continue; // Don't break — check for FRME right after
    }

    if (state === 'frame') {
      if (frameBuffer.length < expectedBytes) break;

      // Extract frame data
      const pixelData = frameBuffer.subarray(0, expectedBytes);
      frameBuffer = frameBuffer.subarray(expectedBytes);
      state = 'sync';

      // Send to all connected clients
      const header = JSON.stringify({
        type: 'frame',
        width: currentWidth,
        height: currentHeight,
      });

      // Send text overlay first (if available), then frame
      for (const ws of clients) {
        if (ws.readyState === 1) {
          if (latestTextJson) {
            ws.send(JSON.stringify({ type: 'text', data: JSON.parse(latestTextJson) }));
          }
          ws.send(header);
          ws.send(pixelData);
        }
      }
      latestTextJson = null;
    }
  }
}

cpp.stdout.on('data', processStdout);
cpp.on('exit', (code) => {
  console.log('C++ renderer exited with code', code);
  process.exit(code || 0);
});

// ---------- HTTP server for static files ----------

const DIST = join(__dirname, 'dist');
const mimeTypes = {
  '.html': 'text/html',
  '.js': 'application/javascript',
  '.css': 'text/css',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.json': 'application/json',
};

const httpServer = createServer((req, res) => {
  let filePath = req.url === '/' ? '/index.html' : req.url;
  const fullPath = join(DIST, filePath);

  if (existsSync(fullPath)) {
    const ext = filePath.substring(filePath.lastIndexOf('.'));
    res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'text/plain' });
    res.end(readFileSync(fullPath));
  } else {
    // Fallback: serve a self-contained HTML page
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(getFallbackHTML());
  }
});

// ---------- WebSocket server ----------

const wss = new WebSocketServer({ server: httpServer });

wss.on('connection', (ws) => {
  console.log('Browser connected');
  clients.add(ws);

  // Request a fresh frame for the new client
  cpp.stdin.write('{"cmd":"render"}\n');

  ws.on('message', (data) => {
    const msg = data.toString();
    // Forward input to C++
    cpp.stdin.write(msg + '\n');
  });

  ws.on('close', () => {
    clients.delete(ws);
    console.log('Browser disconnected');
  });
});

const PORT = process.env.PORT || 3000;
httpServer.listen(PORT, () => {
  console.log(`Live viewer: http://localhost:${PORT}`);
});

// ---------- Graceful shutdown ----------

process.on('SIGINT', () => {
  cpp.kill();
  process.exit(0);
});

process.on('SIGTERM', () => {
  cpp.kill();
  process.exit(0);
});

// ---------- Fallback HTML (self-contained, no build step) ----------

function getFallbackHTML() {
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DynaCharting Live Viewer</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  html, body {
    width: 100%; height: 100%; overflow: hidden;
    background: #1a1a1e; color: #ccc;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, monospace;
  }
  .container {
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    width: 100%; height: 100%; position: relative;
  }
  .status-bar {
    position: absolute; top: 8px; left: 12px;
    font-size: 13px; display: flex; gap: 16px;
    z-index: 10; user-select: none;
  }
  .status-indicator { display: inline-flex; align-items: center; gap: 6px; }
  .dot {
    width: 8px; height: 8px; border-radius: 50%;
    display: inline-block;
  }
  .dot.connecting { background: #f0ad4e; }
  .dot.connected  { background: #5cb85c; }
  .dot.disconnected { background: #d9534f; }
  .fps { color: #888; }
  .chart-wrapper { position: relative; display: inline-block; }
  canvas { cursor: crosshair; display: block; }
  #textOverlay {
    position: absolute; top: 0; left: 0; width: 100%; height: 100%;
    pointer-events: none; overflow: hidden;
  }
  #textOverlay div {
    position: absolute; white-space: nowrap; line-height: 1;
  }
</style>
</head>
<body>
<div class="container">
  <div class="status-bar">
    <span class="status-indicator">
      <span class="dot connecting" id="statusDot"></span>
      <span id="statusText">Connecting...</span>
    </span>
    <span class="fps" id="fpsCounter"></span>
  </div>
  <div class="chart-wrapper">
    <canvas id="canvas" width="900" height="600" tabindex="0"></canvas>
    <div id="textOverlay"></div>
  </div>
</div>

<script>
(function() {
  var canvas = document.getElementById('canvas');
  var ctx2d = canvas.getContext('2d');
  var statusDot = document.getElementById('statusDot');
  var statusText = document.getElementById('statusText');
  var fpsCounter = document.getElementById('fpsCounter');
  var textOverlay = document.getElementById('textOverlay');

  var ws = null;
  var dragging = false;
  var dpr = window.devicePixelRatio || 1;
  var cssW = 900, cssH = 600;

  // Frame protocol state
  var expectingPixels = false;
  var frameWidth = 0;
  var frameHeight = 0;

  // FPS tracking
  var frameCount = 0;
  var lastFpsTime = performance.now();

  // Text label DOM pool
  var textPool = [];

  function setStatus(status) {
    statusDot.className = 'dot ' + status;
    if (status === 'connecting') statusText.textContent = 'Connecting...';
    else if (status === 'connected') statusText.textContent = 'Connected';
    else statusText.textContent = 'Disconnected';
  }

  function renderTextOverlay(data) {
    var labels = data.labels;
    var fontSize = data.fontSize || 12;
    var color = data.color || '#b2b5bc';
    // Scale font size and positions from physical pixels to CSS pixels
    var scaledFontSize = Math.round(fontSize / dpr);
    var fontStr = scaledFontSize + 'px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';

    // Expand pool if needed
    while (textPool.length < labels.length) {
      var el = document.createElement('div');
      textOverlay.appendChild(el);
      textPool.push(el);
    }

    // Update visible labels
    for (var i = 0; i < labels.length; i++) {
      var label = labels[i];
      var el = textPool[i];
      el.textContent = label.t;
      el.style.left = (label.x / dpr) + 'px';
      el.style.top = (label.y / dpr) + 'px';
      el.style.font = label.fs ? (Math.round(label.fs / dpr) + 'px ' + fontStr.split(' ').slice(1).join(' ')) : fontStr;
      el.style.color = label.c || color;
      el.style.display = '';

      if (label.a === 'r') {
        el.style.transform = 'translateX(-100%)';
      } else if (label.a === 'c') {
        el.style.transform = 'translateX(-50%)';
      } else {
        el.style.transform = '';
      }
    }

    // Hide unused labels
    for (var j = labels.length; j < textPool.length; j++) {
      textPool[j].style.display = 'none';
    }
  }

  function connect() {
    setStatus('connecting');
    var protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(protocol + '//' + location.host);
    ws.binaryType = 'arraybuffer';

    ws.onopen = function() {
      setStatus('connected');
      // Request rendering at device pixel resolution for crisp output
      var physW = Math.round(cssW * dpr);
      var physH = Math.round(cssH * dpr);
      ws.send(JSON.stringify({ cmd: 'resize', w: physW, h: physH }));
    };

    ws.onclose = function() {
      setStatus('disconnected');
      fpsCounter.textContent = '';
      ws = null;
      setTimeout(connect, 2000);
    };

    ws.onerror = function() {
      ws.close();
    };

    ws.onmessage = function(event) {
      if (typeof event.data === 'string') {
        var msg = JSON.parse(event.data);
        if (msg.type === 'frame') {
          frameWidth = msg.width;
          frameHeight = msg.height;
          expectingPixels = true;
          if (canvas.width !== frameWidth || canvas.height !== frameHeight) {
            canvas.width = frameWidth;
            canvas.height = frameHeight;
            // CSS size = physical / dpr so the browser doesn't upscale
            canvas.style.width = Math.round(frameWidth / dpr) + 'px';
            canvas.style.height = Math.round(frameHeight / dpr) + 'px';
          }
        } else if (msg.type === 'text') {
          renderTextOverlay(msg.data);
        }
      } else if (expectingPixels) {
        expectingPixels = false;

        var handler = function(arrayBuf) {
          var rgba = new Uint8ClampedArray(arrayBuf);
          var imageData = new ImageData(rgba, frameWidth, frameHeight);
          ctx2d.putImageData(imageData, 0, 0);

          // FPS counting
          frameCount++;
          var now = performance.now();
          var elapsed = now - lastFpsTime;
          if (elapsed >= 1000) {
            fpsCounter.textContent = Math.round((frameCount * 1000) / elapsed) + ' FPS';
            frameCount = 0;
            lastFpsTime = now;
          }
        };

        if (event.data instanceof Blob) {
          event.data.arrayBuffer().then(handler);
        } else {
          handler(event.data);
        }
      }
    };
  }

  // ---- Input events ----

  canvas.addEventListener('mousedown', function(e) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.offsetX * dpr, y: e.offsetY * dpr,
      buttons: e.buttons, type: 'down'
    }));
    dragging = true;
  });

  canvas.addEventListener('mousemove', function(e) {
    if (!dragging) return;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.offsetX * dpr, y: e.offsetY * dpr,
      buttons: e.buttons, type: 'move'
    }));
  });

  canvas.addEventListener('mouseup', function(e) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'mouse', x: e.offsetX * dpr, y: e.offsetY * dpr,
      buttons: 0, type: 'up'
    }));
    dragging = false;
  });

  // Also handle mouseup on window in case mouse leaves canvas while dragging
  window.addEventListener('mouseup', function() {
    dragging = false;
  });

  canvas.addEventListener('wheel', function(e) {
    e.preventDefault();
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({
      cmd: 'scroll', x: e.offsetX * dpr, y: e.offsetY * dpr,
      dy: e.deltaY > 0 ? 3 : -3
    }));
  }, { passive: false });

  document.addEventListener('keydown', function(e) {
    if (['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Home','End'].includes(e.key)) {
      e.preventDefault();
      if (!ws || ws.readyState !== WebSocket.OPEN) return;
      ws.send(JSON.stringify({ cmd: 'key', code: e.key }));
    }
  });

  // Start connection
  connect();
})();
</script>
</body>
</html>`;
}
