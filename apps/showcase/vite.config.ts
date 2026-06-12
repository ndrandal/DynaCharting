import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The dc-wasm engine ships as an Emscripten ES module (dc_engine_host.js) that
// dynamically loads dc_engine_host.wasm next to it. Vite must (a) not try to
// pre-bundle/transform the generated glue, and (b) treat the .wasm as a static
// asset so its bytes are served verbatim with the correct mime type.
export default defineConfig({
  plugins: [react()],
  server: { port: 5174 },
  // allowedHosts: true so the static preview can be fronted by any host
  // (e.g. a cloudflared/ngrok demo tunnel) without Vite's DNS-rebinding 403.
  preview: { port: 5174, allowedHosts: true },
  // Emscripten glue references `import.meta.url`, fs/path shims, etc. Keeping it
  // out of optimizeDeps avoids esbuild choking on the generated module.
  optimizeDeps: {
    exclude: ['@repo/dc-wasm'],
  },
  assetsInclude: ['**/*.wasm'],
  build: {
    target: 'es2020',
  },
});
