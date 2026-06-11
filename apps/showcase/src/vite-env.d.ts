/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_SHOWCASE_AGENT_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}

/** Explainer markdown imported as a raw string (Vite ?raw). */
declare module '*.md?raw' {
  const content: string;
  export default content;
}
