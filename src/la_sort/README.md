# La Sort

La Sort sorts the most recent `length` samples by amplitude and takes a weighted
average around a chosen rank. It preserves the original prototype's exponential
weighting and startup behavior: the first output uses only the first input,
then the window grows to the configured length. Length defaults to 64 samples
(1.33 ms at 48 kHz) and accepts integers from 1 to 1024. A length of 1 leaves
the wet signal unchanged; longer lists generally smooth a longer slice of audio.

The host demo processes each stereo channel independently. The Daisy firmware
uses the original left/mono input and sends the result to both outputs.

## MP3 demo

From the repository root, with the dependencies from `nix-shell` available:

```sh
make console
./build-test/src/console/jazz-console mp3s/beautiful_piano.mp3 \
  --preset presets/host/la_sort_demo.toml
```

Playback and MP3 export use the same workflow as Fridge:

```sh
./build-test/src/console/jazz-console input.mp3 \
  --preset presets/host/la_sort_demo.toml --output la_sort_demo.mp3
```

Add `--no-play` for offline export. `ffmpeg` is required for rendering;
`ffplay` is also required for playback. CLI flags override TOML or JSON presets.
The existing `clip_lasort.toml` chain continues to work.

| Preset key / CLI flag | Range | Default | Meaning |
| --- | --- | --- | --- |
| `la_sort_length` / `--la-sort-length` | 1–1024, integer | 64 | Number of recent samples in the sorted list. |
| `la_sort_volume_compensation` / `--la-sort-volume-compensation` | 0–1 | 1 | Fixed wet makeup gain based only on length: 0 disables it, 1 applies `sqrt(N)`. |
| `la_sort_weight_center` / `--la-sort-weight-center` | 0–1 | 0.5 | 0 favors minimum amplitudes, 0.5 the median, 1 maximum amplitudes. |
| `la_sort_weight_sharpness` / `--la-sort-weight-sharpness` | 0–32 | 2 | 0 gives a moving average; larger values concentrate on the selected rank. |
| `la_sort_lfo_rate` / `--la-sort-lfo-rate` | 0.05–20 Hz | 1 | Sine sweep frequency. |
| `la_sort_lfo_depth` / `--la-sort-lfo-depth` | 0–1 | 0 | Total sweep span as a fraction of the list; 0 disables modulation, 1 sweeps end to end. |
| `la_sort_dry` / `--la-sort-dry` | 0–1 | 0 | Original signal level. |
| `la_sort_wet` / `--la-sort-wet` | 0–1 | 1 | Sorted signal level. |

The demo uses length 64, center 0.5, sharpness 12, dry 0.15, and wet 0.85 for a softened
signal with some original attack. Off-center settings can introduce a DC bias.
Dry and wet are independent gains; their sum can exceed one, with final output
clipping handled by the host runner.

```sh
./build-test/src/console/jazz-console input.mp3 \
  --preset presets/host/la_sort_demo.toml \
  --la-sort-length 128 \
  --la-sort-weight-center 0.65 --la-sort-weight-sharpness 24 \
  --la-sort-dry 0 --la-sort-wet 1
```

## Sine modulation

Rate controls cycles per second, and depth controls the whole sweep width.
The sweep stays centered on the Weight Center setting whenever it fits. If it
would cross an edge, only its midpoint moves inward; the sine shape and sweep
width stay intact. With normalized list positions from 0 to 1:

```text
radius = depth / 2
midpoint = clamp(weight_center, radius, 1 - radius)
effective_center = midpoint + radius * sin(phase)
```

For example, center 0.1 and depth 0.4 sweep from 0 to 0.4 around midpoint 0.2.
Center 0.9 with the same depth sweeps from 0.6 to 1 around midpoint 0.8.
Depth 1 always sweeps from 0 to 1. Depth 0 reproduces the unmodulated effect.

The LFO starts at the midpoint moving upward, runs at the audio sample rate,
and uses one phase for both channels. Rate, depth, center, and length changes
preserve phase; it keeps running with depth or Wet at zero. The host uses the
selected `--sample-rate`, and the pedal runs at 48 kHz. Modulation changes rank
selection only; volume compensation remains the fixed length-based gain.

```sh
./build-test/src/console/jazz-console input.mp3 \
  --preset presets/host/la_sort_demo.toml \
  --la-sort-lfo-rate 2 --la-sort-lfo-depth 1
```

## Length and volume

The normalized weights sum to one, which preserves constant input but does not
preserve audio energy. With sharpness zero, sorting has no effect on the sum:
the wet signal is a moving average, `y = (x[0] + ... + x[N-1]) / N`.
For independent, zero-mean samples of variance `sigma²`,
`Var(y) = N * sigma² / N² = sigma² / N`. Thus RMS falls as `1 / sqrt(N)`:
quadrupling the list loses about 6.02 dB, and 64 → 1024 loses about 12.04 dB.

At nonzero sharpness the amplitudes are sorted before weighting, so their
covariances matter: `Var(y) = sum(i,j, a[i] * a[j] * Cov(X[i], X[j]))`, where
`X` are the sorted samples and `a` are normalized weights. Treating the ranks
as independent and normalizing by `sqrt(sum(a²))` would be incorrect. The
length law is an approximation here, supported by the broadband measurements
below, rather than a promise of constant perceived loudness for every sound.

Compensation targets the input level using this averaging model. It is strictly
a fixed cut/boost based on list length, with no envelope follower, signal-level
measurement, AGC, compressor, limiter, or gain smoothing. For the number of
samples currently stored, `n`, and compensation amount `c`, it applies:

```text
gain = 1 + c * (sqrt(n) - 1)
output = dry * input + wet * gain * sorted_average
```

| Filled list length | Gain at amount 1 | Makeup level |
| --- | --- | --- |
| 1 | 1× | 0 dB |
| 16 | 4× | +12.04 dB |
| 64 | 8× | +18.06 dB |
| 256 | 16× | +24.08 dB |
| 1024 | 32× | +30.10 dB |

Both stereo channels receive the same gain, and the dry path is unchanged.
Startup uses the filled count so a long configured list does not boost its
first sample. Changing length updates the gain with the retained window.
The pedal enables this fixed compensation by default; Wet still controls its output
level. There is no additional knob assignment. In the demo, use
`--la-sort-volume-compensation 0` for the original behavior, or a value between
0 and 1 for partial compensation.

On 262,144 deterministic uniform-noise samples (after startup), input RMS was
0.28866. At sharpness zero, raw wet AC RMS was 0.03591 at length 64 and 0.00877
at length 1024. Fixed compensation brings these to approximately 0.2873 and
0.2807, both within 0.25 dB of the input. At the default center 0.5/sharpness 2,
compensated RMS is approximately 0.3216 and 0.3146; rank weighting makes these
about 0.9 and 0.7 dB louder than the input. This is why a length-only correction
can approximate input loudness but cannot guarantee an exact match.

Actual music can differ: a moving average has magnitude response
`abs(sin(N*w/2) / (N*sin(w/2)))`, with frequency-dependent cancellation and
zeros that no finite makeup gain can undo. This compensation is a fixed
length-based gain. It also boosts peaks and off-center DC bias; lower Wet or
compensation if the output clips. There is no dynamics control in the effect
to prevent clipping; the host runner's existing final output clamp is unchanged.

## Integration and firmware

`la_sort.hpp` / `la_sort.cpp` contain the shared DSP. `Config` holds the eight
controls; `Parameters::SetConfig` validates them and prepares normalized weights
for every window size. `Effect::ProcessSample` maintains independent channel
histories and applies those weights. Separate instances have separate histories.
Changing parameters keeps recorded samples, including while the wet gain is zero.
Shrinking length retains only the newest samples. Growing fills with new input
and does not bring back discarded samples. Resizing takes effect on the next
processed sample without clearing the retained audio.
Non-finite input samples become silence so they cannot corrupt sorted history.

`firmware.cpp` handles the Daisy hardware, and `panel.cpp` connects seven
parameters to the existing Fridge encoder/mux panel:

| Fridge knob | Schematic encoder | La Sort parameter | Startup value |
| --- | --- | --- | --- |
| Dry | SW2 | Dry level, 0–1 | 0 |
| Wet | SW3 | Wet level, 0–1 | 1 |
| Position | SW4 | Weight center, 0–1 | 0.5 |
| Write Amount | SW5 | Weight sharpness, 0–32 | 2 |
| Read Amount | SW6 | List length, 1–1024 samples | 64 |
| Erase Amount | SW7 | LFO rate, 0.05–20 Hz | 1 Hz |
| Feedback | SW8 | LFO depth, 0–1 | 0 (off) |

One full encoder revolution spans each continuous parameter's range, with
clamping at both ends. Rate uses a logarithmic scale: half a turn from 0.05 Hz
reaches 1 Hz, and the other half reaches 20 Hz. Length changes by one sample per quadrature tick
(96 samples per revolution, normally four samples per detent).
Each knob's LED uses Fridge's blue-to-green value scale.
The remaining encoders, selection buttons, and encoder presses have no La Sort
action. Settings return to the startup values on reboot. Host presets configure
the MP3 runner and are not loaded by the pedal.

The panel uses Fridge's address pins D0–D2 and encoder bank inputs D5/D6,
with the existing LED controller on D11–D14. It replaces the prototype's
standalone ADC knob on pin 21. This firmware expects the Fridge panel wiring.
Mux scanning uses TIM3 at 100 microseconds per channel. The scan interrupt only
queues encoder ticks; the main loop updates settings, LEDs, and an inactive
parameter table, then publishes the table for the next audio block. Processing
uses fixed storage without heap allocations or rebuilding parameter tables in
the callback. With depth zero it uses the cached weights. Modulation evaluates
one sine and at most two exponential starting weights per sample, reuses the
cached rank decay, and normalizes during the same traversal of sorted samples.
There are no exponentials per rank or square roots in the callback.
Each startup window stores two starting
weights, a decay step, and a makeup gain, so table memory grows linearly with
maximum length. The two firmware tables use about 40 KiB. Length, mix, and
compensation, rate, and depth changes reuse prepared weights when the weighting curve is unchanged.

```sh
cmake --preset arm
cmake --build --preset arm --target la_sort
# To install on a connected Seed:
make flash DIR=la_sort
```

## Verification

```sh
cmake --preset host
cmake --build --preset host --target la_sort_gtest jazz-console
ctest --test-dir build-test -L la_sort --output-on-failure
```

The DSP tests compare against an independent full-sort reference through startup,
duplicate values, wraparound, parameter changes, and invalid inputs. Panel tests
cover all seven knob mappings, travel, end stops, and rejection of invalid updates.
DSP tests also cover minimum/maximum lengths and live shrink/grow changes.
Compensation tests check wet-only gain, startup, live controls, and AC RMS
across lengths using deterministic noise at several sharpness settings. They
also verify that gain stays fixed through level changes and silence, and that
the DSP does not limit signals above full scale.
LFO tests cover edge-constrained sine sweeps, zero/full depth, both rate limits,
sample-rate timing, phase continuity, and modulated audio against the sorting oracle.
When Python and ffmpeg are available at configure time, CTest renders MP3s through
the demo and JSON presets, checks CLI overrides, and compares stereo output with
the reference across complete and partial processing chunks at several lengths,
including modulation at 44.1 and 48 kHz.
