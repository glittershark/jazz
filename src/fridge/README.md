# Fridge

Fridge has 10 heads, 10 independent granular LFOs, and 6 shared regions. Each
region is its own circular stereo buffer. All six draw memory from one pool
of approximately 165 seconds at the firmware’s explicit 48 kHz rate.
The fixed 7,938,000-sample capacity preserves the existing SDRAM budget.

## Panel

The first 10 selection buttons select head/LFO pairs. The remaining 6 assign
the selected pair to a region. Selecting another head loads that head's LFO
settings and shows its current region; it does not change the assignment.

The physical order follows the existing button banks: SW0 channels 0–7 are
pairs 0–7, SW1 channels 0–1 are pairs 8–9, and SW1 channels 2–7 are regions 0–5.
The corresponding LEDs follow the same mapping.

- **Position:** one encoder revolution sweeps the selected head around its
  region. It wraps in either direction and offsets the ongoing LFO motion.
- **Range:** changes the size of the selected head's region. Every head
  assigned to that region uses the new size. It does not edit LFO parameters.
- **Region buttons:** preserve the moving head's percentage through its
  region. A head 35% through a 100-sample region moves to sample 70 in a
  200-sample region. Its LFO keeps its direction, speed, grain timing, and
  random sequence.
- **Head controls:** write, read, erase, feedback, and pan remain per head.
- **LFO controls:** grain sizes and reverse, teleport, pitch shift, and octave
  probabilities remain independent for each pair.
- **Dry/wet:** global mixer gains.

Changing a region's range also preserves the relative positions of all its
heads. Playback speed is unchanged, so a larger region takes longer to
traverse. Position edits and region changes use the existing short head fades.
The old and new contributions retain their own region assignments during a
reassignment fade.

On startup, each region is one second long. Hardware heads start inert and
are distributed across the six regions in order, repeating for heads 6–9.
Host configs assign heads to region 0 unless the preset says otherwise;
the console demo default makes region 0 24000 samples long. A region's minimum range is one sample.

## Region memory

Regions grow and shrink independently. Resizing one does not move or overwrite
recordings in another. The retained prefix survives a resize; space removed
by shrinking is discarded, and newly added space starts silent.

`regions::Memory` maps local positions onto 1024-sample pages in a fixed stereo
sample pool. Lengths remain exact in samples; each region's memory allocation
is rounded up to whole pages. `config::RegionsFit` checks the total page budget
before accepting a change. No heap allocation or copying of recorded audio is
needed to resize. A validity bitmap clears newly used samples lazily, including
old pending writes and erases, so recycled memory cannot leak another region's
audio. This bookkeeping and sample storage fit in the Seed's 64 MiB SDRAM.

If a Range change would exceed the budget, the entire requested change is
rejected. The knob retains its old value and flashes red in 100 ms intervals
for 600 ms. Further rejected turns restart the flash. Turning back down frees
space for other regions. Host presets that exceed the combined budget fail
validation before processing audio.

## LFO and routing boundaries

`LFOEngine` only consumes scalar `LfoParams` and produces values and transition
events. It has no region, head, button, or assignment logic. Its range can be
changed while preserving relative phase without resetting the grain or RNG.

`Modulator` resolves routing:

- `config::Routing::kPairedRegions` is the default. LFO N drives head N's local
  position, using that head's assigned region range. Stored generic targets
  and the LFO config's standalone range are retained but inactive.
- `config::Routing::kAssignable` retains the original target-list behavior:
  LFOs can target heads, other LFOs, and mixer parameters. Positions address
  the original shared tape in this mode. This path remains covered by the
  general-routing tests so flexible assignment can be brought back later.

The current hardware panel exposes fixed pairing. Holding buttons or pressing
encoder switches no longer enters target assignment.

The audio path is `Modulator::TickSample()` → `Frame` → `Sound::ProcessSample()`
on both firmware and host. Region-backed frames contain local sample positions;
`Sound` resolves their physical storage. A frame borrows its region definitions
from its modulator and must be consumed before the next config update.

## Signal and motion controls

For each head, reads contribute stored audio times `read_amount` to the wet
mix. Writes add input times `write_amount` to the region. Erase multiplies the
stored audio by `erase_amount`: **1 means no erasure; 0 clears it**. Writes and
erases fade over `kFadeTime` samples. Pan applies to reads, writes, and erase
strength. The output is `input * dry + sum(head reads) * wet`; heads are not
normalized by count. Feedback is retained in config and UI but is not yet
applied by `Sound`.

An LFO advances in samples at unity speed by default, wrapping within its
range. At each grain boundary it can reverse, teleport to a random position,
and choose a new speed. Grain length is sampled between the minimum and
maximum grain size. Reverse and teleport probabilities are independent.

If the pitch-shift roll succeeds, low/high octave weights select 0.5× or 2×
speed. If it fails, or both octave weights are zero, speed is 1×. Range changes
never scale this playback speed.

## Host presets

Indices are zero-based: heads/LFOs 0–9, regions 0–5. Region ranges and head
positions are measured in samples. Head position is local to its region and
wraps there. Use `Config::AssignRegion` and `Config::ResizeRegion` for live
changes that need to preserve relative position.

```toml
fridge_routing = "paired_regions"
fridge_region_0_range = 24000
fridge_head_0_region = 0
fridge_head_0_position = 0
fridge_head_0_write_amount = 1.0
fridge_head_0_read_amount = 1.0
fridge_head_0_erase_amount = 0.0001
fridge_lfo_0_min_grain_size = 12000
fridge_lfo_0_max_grain_size = 12000
fridge_lfo_0_reverse_chance = 1.0
```

The complete example is `presets/host/fridge_demo.toml`:

```sh
./build-test/src/console/jazz-console input.mp3 \
  --preset presets/host/fridge_demo.toml --output fridge_demo.mp3
```

LFO charts also resolve the paired region's range. To run an existing preset
with general target assignment, explicitly set `fridge_routing = "assignable"`;
its `fridge_lfo_N_range` and `fridge_lfo_N_target_M_*` fields then apply as before.
