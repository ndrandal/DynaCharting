/* apps/showcase/src/node-builtins.d.ts — ENC-1252
 *
 * Minimal ambient declaration for the one Node built-in the chrome tests use
 * (`deriveAxes.test.ts` reads the views' committed `records.json` captures under
 * Vitest). The showcase's tsconfig pulls in no `@types/node`; rather than add
 * that dependency and its lockfile churn for a single function, we declare
 * exactly the call the test makes. Same approach, and same reasoning, as
 * `packages/dc-wasm/src/chart/node-builtins.d.ts`. Type-only — the runtime is
 * real Node.
 */
declare module 'node:fs' {
  /** Read a file as text. The tests pass a file:// URL from `import.meta.url`. */
  export function readFileSync(path: string | URL, encoding: 'utf8'): string;
}
