import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    coverage: {
      provider: "v8",
      reporter: ["text", "html"],
      include: ["packages/engine-host/src/**/*.ts", "packages/chart-controller/src/**/*.ts"],
      exclude: ["**/__tests__/**", "**/*.test.ts"],
    },
    include: ["packages/**/__tests__/**/*.test.ts", "packages/**/*.test.ts"],
  },
});
