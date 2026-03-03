import { defineConfig } from "vite";

export default defineConfig({
  server: { port: 5173, strictPort: true },
  build: {
    rollupOptions: {
      input: {
        gallery: new URL("./gallery.html", import.meta.url).pathname,
        themes: new URL("./themes.html", import.meta.url).pathname,
      },
    },
  },
});
