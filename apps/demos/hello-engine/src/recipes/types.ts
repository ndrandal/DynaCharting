export type ControlCmd = any;

export type WorkerSubscription = {
  kind: "workerStream";
  stream: "lineSine";
  bufferId: number;
};

export type RecipeBuildResult = {
  commands: ControlCmd[];
  dispose: ControlCmd[];
  subscriptions: WorkerSubscription[];
};

export type RecipeConfig = {
  idBase: number; // user provides a stable range for IDs per recipe instance
};
