# Trial 060: Piano Roll

**Date:** 2026-03-12
**Goal:** MIDI piano roll with 187 note events across 4 instrument tracks (drums, bass, chords, melody) over 8 bars of 4/4 time. Tests dense instancedRect@1 at scale with variable-width rectangles, 4-track coloring, musical domain accuracy (drum patterns, chord triads, melodic contour), and non-zero-origin Y-axis transform on a 1200×600 viewport.
**Outcome:** All 187 notes at correct pitch and time positions. All drum patterns verified (16 kicks, 16 snares, 64 hihats). All 16 chord events are triads. No pitch-time overlaps within any track. Zero defects.

---

## What Was Built

A 1200×600 viewport with a single pane (background #0f172a):

**4 instrument tracks (4 instancedRect@1 DrawItems, rect4):**

| Track | Color | # Notes | Pitch Range | Description |
|-------|-------|---------|-------------|-------------|
| Drums | #ef4444 (red) | 96 | MIDI 36–42 | Kick (36), snare (38), hihat (42) |
| Bass | #3b82f6 (blue) | 16 | MIDI 48–55 | Root notes, 2-beat duration |
| Chords | #10b981 (emerald) | 48 | MIDI 60–71 | Major triads (root, 3rd, 5th) |
| Melody | #f59e0b (amber) | 27 | MIDI 72–84 | Single notes, variable rhythm |

Total: 187 notes. Each note: (beatStart, pitch, beatEnd, pitch+0.8). Height 0.8 with 0.2 gap.

**Drum pattern (per bar):**
- Kick (MIDI 36): beats 1 and 3 (0.5 beat duration) — 16 total
- Snare (MIDI 38): beats 2 and 4 (0.5 beat duration) — 16 total
- Hihat (MIDI 42): every 8th note (0.25 beat duration) — 64 total

**Chord structure:** 16 chord events (2 per bar), each a major triad with 3 notes (root + major 3rd + perfect 5th). 1.5-beat duration.

**12 grid lines (1 lineAA@1 DrawItem, rect4, 12 instances):**
- 7 vertical bar boundaries at X=4,8,12,16,20,24,28 plus X=32
- 4 horizontal octave lines at Y=48,60,72,84 (C3, C4, C5, C6)
White, alpha 0.08, lineWidth 1. Layer 10.

Data space: X=[0, 32], Y=[34, 86]. Transform 50: sx=0.059375, sy=0.036538, tx=−0.95, ty=−2.192308.

Layers: Grid (10) → Drums (11) → Bass (12) → Chords (13) → Melody (14).

Total: 22 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 187 notes at correct positions.** Every note's (xMin, yMin, xMax, yMax) verified: time within [0,32], pitch within track range, height exactly 0.8. 187/187 correct.

- **No pitch-time overlaps within any track.** Notes at the same pitch within a track have non-overlapping time ranges. Verified across all 4 tracks.

- **Drum pattern is musically correct.** Kick on beats 1 and 3, snare on 2 and 4, hihat on every 8th note — the standard rock/pop drum pattern. 16+16+64 = 96 drum events verified.

- **All 16 chord events are proper triads.** Each chord event has exactly 3 notes at the same start time. The intervals (root, major 3rd at +4 semitones, perfect 5th at +7 semitones) create major triads.

- **Bass notes track the chord progression.** Bass pitches step through a progression (C3, E3, D3, C3, F3, G3, D3, C3 pattern), each held for 2 beats. These root notes support the chord structure.

- **Melody has variable rhythm and contour.** Notes range from 0.5 to 2 beats with a clear melodic contour rising and falling across the register. The ascending passage to C6 (MIDI 84) at beat 24 creates a natural climax.

- **Non-zero-origin Y transform is correct.** With Y range [34,86]: ty = −0.95 − 34×0.036538 ≈ −2.192308. This correctly maps MIDI 34 to clip −0.95 and MIDI 86 to clip 0.95.

- **Track layering creates correct visual depth.** Drums (11) → Bass (12) → Chords (13) → Melody (14). Higher-register instruments draw in front, matching the vertical position hierarchy.

- **Bar boundary grid lines at 4-beat intervals.** 8 bars × 4 beats = 32 beats. Grid at X=4,8,...,32 divides the timeline into musical measures.

- **Octave lines at C3, C4, C5, C6.** These standard musical reference pitches help read the register of each track.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 5/5 geometries verified.

- **All 22 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Non-zero-origin data ranges require adjusted ty.** When Y doesn't start at 0 (e.g., MIDI 34–86), the transform offset is ty = −0.95 − yMin × sy. This shifts the entire Y range to clip space correctly.

2. **Musical domain data validates structurally.** Drum patterns (kick/snare/hihat positions), chord triads (3 notes per event), and melodic contour provide domain-specific verification beyond just checking positions.

3. **instancedRect@1 handles dense data well.** 187 rectangles across 4 DrawItems renders cleanly. Each track's notes share color, so one DrawItem per track is sufficient.

4. **Variable-width rectangles communicate rhythm.** Short notes (0.25 beats for hihats) vs long notes (2 beats for bass) create immediate visual rhythm differentiation.

5. **Pitch-time overlap checking is the key integrity invariant.** Within a track, no two notes at the same pitch should overlap in time. This is the musical equivalent of the parent-child containment check in the flame chart.
