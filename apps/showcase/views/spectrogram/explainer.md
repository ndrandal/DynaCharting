---
title: Spectrogram — frequency × time
referenceTool: Audio spectrogram
tier: walled
---

An audio spectrogram — a **frequency × time** magnitude grid computed by a short-time FFT (STFT) of a synthesized signal (a rising chirp + stepped tones + a steady low harmonic, plus noise), rasterized to a viridis-like colormap and drawn as a single textured quad. Time runs left→right, frequency bottom→top; the bright diagonal is the chirp sweeping up, the bright steps are the tone that jumps each second. **Shown precomputed** — the FFT runs once on the CPU at build time; doing it live on the GPU is the frontier.

| | |
|---|---|
| **DATA** | synthesized 4 s signal @ 4096 Hz → STFT (512-pt window, 128 hop, 125 frames) |
| **PIPELINE** | `texturedQuad@1` (one full-pane quad, `pos2_uv4`) |
| **WRITE MODE** | static — RGBA8 colormap uploaded once via `setTexturePixels` (ENC-532) |
| **TEXTURE** | `90` · 256×256 RGBA8 viridis colormap (log-magnitude, high freq at top) |
| **THE WALL** | **live GPU FFT + interpolation** — the STFT computed per-frame in a compute shader, streamed into the texture |

**What's going on.** A spectrogram slides a window across a signal and takes the **Fourier transform** of each window — turning a 1D waveform into a 2D frequency-vs-time image where brightness is magnitude. Here the engine renders that image faithfully, but as a **precomputed RGBA8 texture**: the signal is synthesized, windowed (Hann), FFT'd frame by frame on the CPU at build time, converted to log-magnitude, normalized, mapped through a viridis ramp, and uploaded via the `setTexturePixels` escape hatch. The manifest only declares a pane, a unit quad, and `texturedQuad@1` — the engine blits the colormap; it runs no transform.

> **THE WALL (the frontier this view documents).** The live version computes the **FFT on the GPU**: a compute shader runs the STFT every frame on the incoming audio (or signal) buffer, interpolates the magnitude grid, and streams it into the texture in real time — no CPU prepass, no precomputed image. That live GPU FFT + interpolation is the frontier tier; this card proves the *output*, the wall is the *live transform*.
