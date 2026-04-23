# Fridge

`fridge` is a multi-head tape/freezer buffer. Each head points at one sample
position in a long circular-ish audio buffer, can read from that position into
the wet signal, can write the current input sample into that position, and can
erase that position over time.

The implementation currently has:

- 8 heads
- 8 LFOs
- a 6 minute buffer, `BUFFER_LEN = 44100 * 60 * 6`
- 128 sample fades for pending writes, erases, and head discontinuities

The host console uses presets with keys like `fridge_head_0_position` and
`fridge_lfo_0_range`. The hardware UI edits one selected head and one selected
LFO through the same underlying config fields. Selection controls are still a
TODO in `ui.hpp`, so the firmware defaults to selected head 0 and selected LFO 0
unless code changes those indices.

The host processor applies the LFO transform and the head transition mixer
before audio processing. The firmware path currently calls the transform from
`Engine::Tick`, but the audio callback still passes the raw `config` directly
to `Sound`, so LFO modulation is effectively a host-console behavior until the
firmware path feeds the transformed config or transition frame into audio.

## Signal Model

For each input sample `x[n]`, every active head contributes to the wet signal:

```text
wet_head = read(buffer[position]) * read_amount
```

Then the head may schedule two buffer updates at the same `position`:

```text
write: buffer[position] += x[n] * write_amount
erase: buffer[position] *= erase_amount
```

Writes and erases are not applied as hard instantaneous changes. They are
scheduled over `FADE_TIME` samples, which is currently 128, so reads see a
short fade instead of an immediate step.

The final sample is:

```text
y[n] = x[n] * dry + sum(wet_head) * wet
```

There is no normalization by head count in `Sound`, so multiple heads can add
gain quickly.

## Head Knobs

These knobs edit `config::Head` for the selected head.

### Position

`position` is the buffer index the head reads, writes, and erases.

- Host preset key: `fridge_head_N_position`
- Host range: `0 .. BUFFER_LEN - 1`
- Hardware UI value type: encoder ticks, clamped to roughly the buffer range

If an LFO targets head position, the LFO delta is added to this static knob
position. A head position jump caused by LFO reversal or teleport can be
crossfaded by `HeadTransitionMixer`.

### Write Amount

`write_amount` controls how much of the current input sample is written into
the buffer at `position`.

- Host preset key: `fridge_head_N_write_amount`
- Range: `0.0 .. 1.0`
- Formula: scheduled write value is `input * write_amount`

At `0.0`, the head does not write. At `1.0`, the full input sample is written.
Because writes add into the stored sample, repeated writes can build up level.

### Read Amount

`read_amount` controls how much of the stored buffer sample is added to the wet
signal.

- Host preset key: `fridge_head_N_read_amount`
- Range: `0.0 .. 1.0`
- Formula: wet contribution is `buffer[position] * read_amount`

At `0.0`, the head is silent. At `1.0`, it contributes the full stored sample
to the wet bus.

### Erase Amount

`erase_amount` controls how much of the stored value remains after an erase.
This is easy to misread: it is a residual multiplier, not an "amount erased"
control.

- Host preset key: `fridge_head_N_erase_amount`
- Range: `0.0 .. 1.0`
- Formula: scheduled erase is `buffer[position] *= erase_amount`

At `1.0`, nothing is erased. At `0.5`, the stored value is halved. At `0.0`,
the stored value is cleared. The sound code only schedules an erase when
`erase_amount < 1.0`.

### Feedback

The config has a signed feedback-style control split into `kind` and `amount`.
In the UI model, positive turns become read feedback and negative turns become
erase feedback:

```text
feedback >= 0: kind = read,  amount = feedback
feedback <  0: kind = erase, amount = -feedback
```

- Host preset keys: `fridge_head_N_feedback_kind`,
  `fridge_head_N_feedback_amount`
- `feedback_kind`: `read` or `erase`
- `feedback_amount`: `0.0 .. 1.0`

Current implementation note: `feedback` is parsed and can be modulated, but
`sound.cpp` does not currently use it in the audio path.

## Mixer Knobs

### Dry

`dry` is the linear gain applied to the unprocessed input sample.

- Host preset key: `fridge_dry`
- Host range: `0.0 .. 4.0`
- Hardware UI range: `0.0 .. 1.0`
- Formula: dry contribution is `input * dry`

### Wet

`wet` is the linear gain applied to the summed head output.

- Host preset key: `fridge_wet`
- Host range: `0.0 .. 4.0`
- Hardware UI range: `0.0 .. 1.0`
- Formula: wet contribution is `sum(head outputs) * wet`

## LFO Knobs

Each LFO is a random-grain ramp generator. Its value moves linearly inside
`0 .. range`, wrapping around the range. At each grain boundary it can reverse,
teleport, and/or choose a new speed. LFOs do not affect audio by themselves;
they add their current delta to all configured targets.

The effective modulation is:

```text
target_value = static_knob_value + (lfo_value - lfo_initial_value)
```

For example, if a head starts at position `12000` and its LFO moves from `0`
to `300`, the transformed head position is `12300`.

### Range

`range` is the size of the LFO's wrapped value space.

- Host preset key: `fridge_lfo_N_range`
- Host range: `0 .. BUFFER_LEN - 1`
- Hardware UI range: `0 .. 255`

With `range = 0`, the LFO value wraps to `0` and produces no useful motion.
For position modulation, `range` is measured in samples. For other targets, it
is still added as a raw delta, so large ranges can push those targets hard into
their clamps.

### Min Grain Size

`min_grain_size` is the lower bound, in samples/ticks, for the length of each
LFO grain.

- Host preset key: `fridge_lfo_N_min_grain_size`
- Host range: `1 .. BUFFER_LEN`
- Hardware UI range: `0 .. 255`, sanitized to at least `1` in the transform

At the start of each grain, the engine samples an integer grain length from the
inclusive range between the effective min and max grain sizes.

### Max Grain Size

`max_grain_size` is the upper bound, in samples/ticks, for the length of each
LFO grain.

- Host preset key: `fridge_lfo_N_max_grain_size`
- Host range: `1 .. BUFFER_LEN`
- Hardware UI range: `0 .. 255`, sanitized to at least `1` in the transform

If `min_grain_size` is greater than `max_grain_size`, the implementation still
builds a valid range by using the smaller value as the lower bound and the
larger value as the upper bound.

### Reverse Chance

`reverse_chance` is the probability that the LFO flips direction at a grain
boundary.

- Host preset key: `fridge_lfo_N_reverse_chance`
- Range: `0.0 .. 1.0`

At `0.0`, the LFO keeps its current direction. At `1.0`, it reverses at every
grain boundary. A reversal on a head-position target starts a short
old-motion/new-motion crossfade.

### Teleport Chance

`teleport_chance` is the probability that the LFO jumps to a new random value
at a grain boundary.

- Host preset key: `fridge_lfo_N_teleport_chance`
- Range: `0.0 .. 1.0`
- New value distribution: uniform over `0.0 .. range`

A teleport on a head-position target starts a short crossfade from the old
motion to the new motion.

### Pitch Shift Chance

`pitch_shift_chance` is the probability that a new grain uses an octave-shifted
speed instead of normal speed.

- Host preset key: `fridge_lfo_N_pitch_shift_chance`
- Range: `0.0 .. 1.0`

If this roll fails, speed is `1.0`. If it succeeds, the LFO chooses either
`0.5` speed or `2.0` speed using the low/high octave weights below.

### Low Octave Chance

`low_octave_chance` is the relative weight for choosing `0.5` speed after a
successful pitch-shift roll.

- Host preset key: `fridge_lfo_N_low_octave_chance`
- Range: `0.0 .. 1.0`

This is not rolled independently. Once pitch shift is active:

```text
P(low octave) = low_octave_chance /
                (low_octave_chance + high_octave_chance)
```

If both low and high weights are `0.0`, speed falls back to `1.0`.

### High Octave Chance

`high_octave_chance` is the relative weight for choosing `2.0` speed after a
successful pitch-shift roll.

- Host preset key: `fridge_lfo_N_high_octave_chance`
- Range: `0.0 .. 1.0`

Once pitch shift is active:

```text
P(high octave) = high_octave_chance /
                 (low_octave_chance + high_octave_chance)
```

## LFO Targets

Host presets can wire each LFO to one or more targets:

```toml
fridge_lfo_0_target_0_object = "head"
fridge_lfo_0_target_0_parameter = "position"
fridge_lfo_0_target_0_index = 0
```

Objects:

- `head`
- `lfo`
- `mixer`

Supported target parameters:

- Head: `position`, `write_amount`, `read_amount`, `erase_amount`,
  `feedback_amount`
- LFO: `range`, `max_grain_size`, `min_grain_size`, `reverse_chance`,
  `teleport_chance`, `pitch_shift_chance`, `low_octave_chance`,
  `high_octave_chance`
- Mixer: `dry`, `wet`

Target indices are zero-based. There are 8 heads and 8 LFOs.

## Current Implementation Notes

- `feedback` exists in config/UI/preset parsing, but it is not used by
  `Sound::ApplyHead`.
- The host processor applies LFO modulation and head-transition crossfades.
  The firmware audio callback currently processes the raw config directly.
- The hardware `feedback` encoder is declared, but `engine.cpp` currently
  registers the position callback twice instead of registering the feedback
  callback.
- The hardware `wet` encoder currently registers the dry callback, so it edits
  dry instead of wet. Host presets still set `fridge_wet` correctly.
- Host preset ranges and hardware UI ranges are not identical. In particular,
  LFO size knobs are limited to `0 .. 255` in the hardware UI type, while host
  presets can set sample-scale values up to the buffer length.

## Example

`presets/host/fridge_demo.toml` sets head 0 to read, write, and slowly erase
one moving tap. LFO 0 targets head 0 position, scans over 24000 samples, and
reverses every 12000 sample grain:

```toml
fridge_head_0_position = 0
fridge_head_0_write_amount = 1.0
fridge_head_0_read_amount = 1.0
fridge_head_0_erase_amount = 0.0001

fridge_lfo_0_range = 24000
fridge_lfo_0_min_grain_size = 12000
fridge_lfo_0_max_grain_size = 12000
fridge_lfo_0_reverse_chance = 1.0

fridge_lfo_0_target_0_object = "head"
fridge_lfo_0_target_0_parameter = "position"
fridge_lfo_0_target_0_index = 0
```

To render it through the host CLI:

```shell
./build-test/src/console/jazz-console input.mp3 \
  --preset presets/host/fridge_demo.toml \
  --output fridge_demo.mp3
```
