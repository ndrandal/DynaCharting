/* Minimal ambient declarations for the Node built-ins used by the real-wasm
 * integration tests (ENC-715 text.test.ts, ENC-1253 axis.test.ts) and by the
 * ENC-1253 C++/TS theme-parity test, which reads the C++ theme sources as TEXT.
 * The package tsconfig sets
 * `"types": []` and pulls in no `@types/node`, so `node:fs`/`node:url`/`node:path`
 * do not resolve under `tsc --noEmit`. Rather than add a whole `@types/node`
 * dependency (and lockfile churn) for three functions, we declare exactly those
 * three, accurately typed for how the test uses them. Runtime is real Node (the
 * test runs under Vitest); these are type-only shims.
 */
declare module "node:fs" {
  /** Read a file's bytes. The test wraps the result in `new Uint8Array(...)`. */
  export function readFileSync(path: string): Uint8Array;
  /** Read a file as text — `theme.test.ts` parses the C++ theme sources. */
  export function readFileSync(path: string, encoding: "utf8"): string;
}
declare module "node:url" {
  export function fileURLToPath(url: string | URL): string;
}
declare module "node:path" {
  export function dirname(p: string): string;
  export function resolve(...segments: string[]): string;
}
