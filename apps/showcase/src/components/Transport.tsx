/* apps/showcase/src/components/Transport.tsx
 *
 * The replay transport (DESIGN §3.6 / T5.6): play/pause, restart, a scrubber
 * bound to the replay timeline, elapsed/total (mono tabular), loop toggle.
 * Accent fill on the played portion. Two sizes — `compact` for the app bar,
 * the full inline scrubber for single-view mode. Keyboard space=play/pause and
 * r=restart are wired at the App level (T5.7); the buttons are also clickable.
 */

import type { CSSProperties } from 'react';

function fmt(fraction: number, durationMs: number): string {
  const ms = Math.max(0, fraction) * durationMs;
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  return `${m}:${String(s % 60).padStart(2, '0')}`;
}

export interface TransportProps {
  progress: number;
  playing: boolean;
  onPlayToggle: () => void;
  onRestart: () => void;
  durationMs: number;
  loop?: boolean;
  onLoopToggle?: () => void;
  onScrub?: (fraction: number) => void;
  variant?: 'compact' | 'full';
}

export function Transport({
  progress,
  playing,
  onPlayToggle,
  onRestart,
  durationMs,
  loop = true,
  onLoopToggle,
  onScrub,
  variant = 'compact',
}: TransportProps) {
  const fillStyle: CSSProperties = { width: `${Math.round(Math.min(1, Math.max(0, progress)) * 100)}%` };

  const handleScrub = (e: React.MouseEvent<HTMLDivElement>) => {
    if (!onScrub) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const f = (e.clientX - rect.left) / rect.width;
    onScrub(Math.min(1, Math.max(0, f)));
  };

  return (
    <div className="transport" role="group" aria-label="Replay transport">
      <button type="button" onClick={onRestart} aria-label="Restart replay" title="Restart (r)">
        ⏮
      </button>
      <button
        type="button"
        onClick={onPlayToggle}
        aria-label={playing ? 'Pause replay' : 'Play replay'}
        aria-pressed={playing}
        title="Play / pause (space)"
      >
        {playing ? '⏸' : '▶'}
      </button>
      <div
        className={variant === 'full' ? 'scrubber wide' : 'scrubber'}
        onClick={handleScrub}
        role="slider"
        aria-label="Replay position"
        aria-valuemin={0}
        aria-valuemax={100}
        aria-valuenow={Math.round(progress * 100)}
        tabIndex={onScrub ? 0 : -1}
      >
        <span className="scrubber-fill" style={fillStyle} aria-hidden />
      </div>
      <span className="time" aria-hidden>
        {fmt(progress, durationMs)} / {fmt(1, durationMs)}
      </span>
      {onLoopToggle && (
        <button
          type="button"
          className={loop ? 'toggle-on' : ''}
          onClick={onLoopToggle}
          aria-label="Loop replay"
          aria-pressed={loop}
          title="Loop"
        >
          ↻
        </button>
      )}
    </div>
  );
}
