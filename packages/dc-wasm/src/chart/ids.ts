/**
 * Resource-ID allocator for the DynaCharting client builder.
 *
 * The engine uses **one** id namespace shared by all six resource kinds
 * (pane, layer, drawItem, buffer, geometry, transform). A `create` against an
 * id that is already taken fails silently with `ID_TAKEN` (see GAPS.md G5 /
 * SPEC §5.5). The only safe contract is therefore: **never hand out the same
 * id twice in a session, regardless of kind.**
 *
 * Design choice — a single monotonic counter (not per-type ranges):
 *   - The engine's namespace is unified, so the *only* invariant that matters
 *     is global uniqueness. A single monotonic counter satisfies that by
 *     construction, with no per-type bookkeeping, no range sizing, and no risk
 *     of one type exhausting its slice while others sit idle.
 *   - Per-type ranges would re-introduce exactly the "hand-manage
 *     non-overlapping ranges" footgun this ticket exists to remove, and add a
 *     failure mode (range overflow) for zero benefit in a shared namespace.
 *   - `nextFor(kind)` exists purely for call-site readability; it returns an id
 *     from the same monotonic sequence as `next()`. The `kind` does not change
 *     which id you get — it documents intent at the call site.
 *
 * Ids start at 1. Id 0 is never handed out (the engine treats 0 as a reserved
 * / "no resource" sentinel and several stores start their counters at 1).
 *
 * Pure and framework-agnostic: no DOM, no engine, no globals. One allocator
 * instance per builder session.
 */

/** The six resource kinds that share the engine's single id namespace. */
export type ResourceKind =
  | "pane"
  | "layer"
  | "drawItem"
  | "buffer"
  | "geometry"
  | "transform";

/**
 * A monotonic id allocator over the engine's unified resource namespace.
 *
 * Every call to {@link IdAllocator.next} or {@link IdAllocator.nextFor} returns
 * a strictly increasing id that has not been returned before in this session,
 * guaranteeing no `ID_TAKEN` collisions across any of the six resource kinds.
 */
export interface IdAllocator {
  /** Allocate the next id from the shared namespace. */
  next(): number;
  /**
   * Allocate the next id, naming the resource kind at the call site for
   * readability. The returned id is drawn from the same monotonic sequence as
   * {@link next}; `kind` does not affect the value.
   */
  nextFor(kind: ResourceKind): number;
  /** The id that will be returned by the next allocation (does not consume it). */
  peek(): number;
  /**
   * Reset the allocator back to its starting id. Only safe when every resource
   * previously allocated has been disposed; otherwise reuse can collide.
   */
  reset(): void;
}

/** First id handed out (and the value the counter resets to). */
const FIRST_ID = 1;

/**
 * Create a fresh id allocator. Each call returns an independent allocator with
 * its own counter starting at {@link FIRST_ID} (1).
 *
 * @param start - Optional first id to hand out. Must be a positive,
 *   non-fractional integer. Useful when coexisting with externally-allocated
 *   ids (e.g. a reserved low range): pass a `start` above that range.
 */
export function createIdAllocator(start: number = FIRST_ID): IdAllocator {
  if (!Number.isInteger(start) || start < 1) {
    throw new RangeError(
      `createIdAllocator: start must be a positive integer, got ${start}`,
    );
  }

  const base = start;
  let nextId = base;

  function alloc(): number {
    if (!Number.isSafeInteger(nextId)) {
      // Astronomically unlikely in any real session, but failing loud beats
      // silently overflowing into non-unique / unsafe-integer territory.
      throw new RangeError(
        "createIdAllocator: id space exhausted (exceeded Number.MAX_SAFE_INTEGER)",
      );
    }
    return nextId++;
  }

  return {
    next: alloc,
    nextFor: (_kind: ResourceKind): number => alloc(),
    peek: (): number => nextId,
    reset: (): void => {
      nextId = base;
    },
  };
}
