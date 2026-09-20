/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_SHOWCASE_AGENT_URL?: string;
  /** Dataplane session to subscribe to on that socket. Default 'showcase'. */
  readonly VITE_SHOWCASE_AGENT_SESSION?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}

/** Explainer markdown imported as a raw string (Vite ?raw). */
declare module '*.md?raw' {
  const content: string;
  export default content;
}
