# Fridge implementation practices

- Keep `LFOEngine` a scalar motion generator. Resolve regions and target
  assignments in `Modulator`, and keep physical memory mapping in
  `regions::Memory` / `Sound`. Do not put panel policy in the LFO engine.
- Use `Config::AssignRegion` and `Config::ResizeRegion` for live region edits.
  Both static offsets and ongoing LFO phase must preserve their relative
  position when the assigned range changes.
- Validate a complete proposed region allocation before changing it. Capacity
  uses the sum of rounded page allocations, not just the audible sample counts.
- Keep audio memory fixed and recycle pages. Do not allocate heap memory, move
  recorded buffers, or clear an entire resized region in the audio callback.
  Clear newly accessed samples lazily and release their pending updates.
- Keep frame region references valid through processing. The firmware serializes
  config installation against its audio interrupt; do not publish half-updated
  region definitions or modulation state.
- Keep the general-routing and standalone LFO tests when changing fixed pairing.
  Verify region isolation, retained audio, phase, capacity rejection, and reuse
  of samples with pending updates when changing region storage.
- Build the Daisy target as well as host tests. Its internal flash is only
  128 KiB. The panel translation unit uses size optimization; audio and motion
  translation units retain the toolchain's speed optimization. Check SDRAM fit
  whenever changing sample storage or region bookkeeping.

- Use unsigned 32-bit sample-clock differences; never compare absolute update
  timestamps across rollover. Coalesce erases only when deadlines match.
- Keep stored samples four bytes on both host and target. Do not use inactive
  union members or pointer-sized tags to encode slab indices. Owning buffer
  values and modulators with borrowed frames are not copyable.
- Validate standalone LFO inputs as well as routed inputs: ranges are bounded
  by the sample pool and grains by uint32_t. Avoid signed lround conversions
  for values that can exceed the Seed's signed-long range.
- Callbacks must actually accept their erased void* signature. Cast the context
  inside the callback, never reinterpret-cast a typed function pointer.
- Keep unfinished panel/feedback features in place and record them in the
  README's deferred-functionality list until their behavior is decided.
- Build the `fridge-tests` target and run the `fridge` CTest label. Benchmark
  only optimized builds; host reduced-budget p99 checks are regression proxies,
  while DWT worst-block measurements on a Seed establish hardware headroom.
