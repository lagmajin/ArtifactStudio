# Environmental Audio Physics Research

**最終更新:** 2026-10-08

## Scope

This note records a focused research pass for the standalone rain, wind, and wave generators in `ArtifactCore`. The cited papers inform model choices; this work does not reproduce their full algorithms or datasets.

## Evidence and implementation

| Source | Reported finding | Model use |
|---|---|---|
| Liu, Cheng, Tong, [Physically-based Statistical Simulation of Rain Sound (SIGGRAPH 2019)](https://haonancheng.cn/attaches/2019%20SIGGRAPH.pdf) | Rain impact is a short transient; bubble oscillation is a distinct event and may be inaudible for solid surfaces. | Existing droplet model keeps randomized short impacts and avoids adding a sustained tonal bubble to every drop. |
| Heutschi et al., [Auralization of Wind Turbine Noise: Propagation Filtering and Vegetation Noise Synthesis (2014)](https://doi.org/10.3813/AAA.918682) | Wind-related sound is shaped by a time-varying wind process and filtered-noise spectra; vegetation contributes a separate, speed-dependent noise source. | Wind now has a slow, asymmetric stochastic gust envelope. The current model remains a generic wind bed and does not claim to synthesize vegetation. |
| van den Doel, [Physically based models for liquid sounds (2005)](https://doi.org/10.1145/1101530.1101554) | Liquid sounds can be described as stochastic acoustic emissions from bubbles across streams, rain, rivers, and breaking waves. | Supports event-based liquid sound as a direction; the current wave break remains a simplified broadband event. |
| Guo, [Sound generation in the ocean by breaking surface waves (1987)](https://www.cambridge.org/core/journals/journal-of-fluid-mechanics/article/abs/sound-generation-in-the-ocean-by-breaking-surface-waves/F183AE007307DFE885A81E45DD1FBCCB) | Spray momentum fluctuations can be an important sound source during wave breaking. | Supports treating a break as a transient/noisy event rather than a continuous pitched tone. |
| Wang and Liu, [Example-based synthesis for sound of ocean waves caused by bubble dynamics (2018)](https://onlinelibrary.wiley.com/doi/full/10.1002/cav.1857) | Wave audio depends on bubble dynamics and wave attributes, with example clustering used for real-time synthesis. | Follow-up direction: model foam/bubble activity separately from the swell bed; not implemented in this slice. |

## Changes in this slice

- Wind gust amplitude follows a bounded, filtered random process with a faster rise and slower fall. Its state uses a fixed PRNG and scalar fields, so `Update` performs no heap allocation.
- Flow synthesis now maps its frequency parameter to a low-pass corner in hertz and computes the one-pole coefficient from sample rate. The previous `frequency / (Q * sampleRate)` coefficient kept low-frequency wind unnaturally muffled. A small unfiltered component preserves broadband air noise.
- Wave cycles vary by a bounded ±8% around the selected period to reduce exact repetition. This is a perceptual approximation, not a claim that real ocean swell follows this distribution.
- Rain continues to use short randomized impact grains and event-level stereo spread, following the SIGGRAPH model's separation of impact and bubble phenomena.

## Limits and next work

The wind paper's vegetation source model is not present, and the wave implementation does not yet simulate foam particles or bubble populations. The standalone WAV checks verify finite, non-silent stereo output and file structure; they do not evaluate perceptual realism. Listening review remains necessary before treating these models as production quality.
