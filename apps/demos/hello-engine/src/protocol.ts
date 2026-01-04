// apps/demos/hello-engine/src/protocol.ts

export type StreamType = "lineSine" | "pointsCos" | "rectBars" | "candles";
export type PolicyMode = "raw" | "agg";

export type WorkerSubscription = {
  kind: "workerStream";
  stream: StreamType;
  bufferId: number;
};
