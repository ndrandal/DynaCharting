import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    coverage: {
      provider: "v8",
      reporter: ["text", "html"],
      include: ["packages/engine-host/src/**/*.ts", "packages/chart-controller/src/**/*.ts"],
      exclude: ["**/__tests__/**", "**/*.test.ts"],
    },
    // apps/** is included (ENC-1252) so the showcase's pure chrome logic can be
    // tested against the views' real committed captures. React/DOM code stays
    // untested here — these are node-env tests over pure functions + bytes.
    include: [
      "packages/**/__tests__/**/*.test.ts",
      "packages/**/*.test.ts",
      "apps/**/*.test.ts",
      // ENC-1277: the LIMITATIONS.md id guard. `pnpm test` is the only check in
      // this repo that needs no Dawn, no GPU and no build, so it is where a guard
      // that must actually run belongs.
      "scripts/**/*.test.ts",
    ],
    exclude: ["**/node_modules/**", "**/dist/**"],
  },
});
