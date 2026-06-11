/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_SHOWCASE_AGENT_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
