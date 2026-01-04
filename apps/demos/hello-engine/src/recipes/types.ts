import type { StreamType, WorkerSubscription as ProtoWorkerSubscription } from "../protocol";

export type ControlCmd = any;

export type WorkerSubscription = ProtoWorkerSubscription & {
  stream: StreamType;
};

export type RecipeBuildResult = {
  commands: ControlCmd[];
  dispose: ControlCmd[];
  subscriptions: WorkerSubscription[];
};

export type RecipeConfig = {
  idBase: number; // user provides a stable range for IDs per recipe instance
};
