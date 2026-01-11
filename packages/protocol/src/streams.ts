export type StreamType = "lineSine" | "pointsCos" | "rectBars" | "candles";

export type WorkerSubscription =
  | { kind: "workerStream"; stream: StreamType; bufferId: number };
