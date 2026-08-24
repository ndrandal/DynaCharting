/* Minimal ambient declarations for the Node built-ins used by the ENC-715
 * real-wasm integration test (text.test.ts). The package tsconfig sets
 * `"types": []` and pulls in no `@types/node`, so `node:fs`/`node:url`/`node:path`
 * do not resolve under `tsc --noEmit`. Rather than add a whole `@types/node`
 * dependency (and lockfile churn) for three functions, we declare exactly those
 * three, accurately typed for how the test uses them. Runtime is real Node (the
 * test runs under Vitest); these are type-only shims.
 */
declare module "node:fs" {
  /** Read a file's bytes. The test wraps the result in `new Uint8Array(...)`. */
  export function readFileSync(path: string): Uint8Array;
}
declare module "node:url" {
  export function fileURLToPath(url: string | URL): string;
}
declare module "node:path" {
  export function dirname(p: string): string;
  export function resolve(...segments: string[]): string;
}
