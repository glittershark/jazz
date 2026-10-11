# La Sort implementation practices

- Keep one DSP implementation in `la_sort.cpp` for firmware, host rendering,
  and tests. Keep Daisy hardware setup in `firmware.cpp` and the Fridge encoder
  hookup in `panel.cpp`, reusing its existing mux and LED drivers.
- Construct the panel after `hw.Init()`. Prime the mux inputs before enabling
  turn handlers so startup pull-up transitions cannot change parameters.
- Queue encoder ticks atomically in the scan interrupt; consume them in the
  main loop. Keep LED I2C and weight preparation out of both interrupts.
- Match Fridge's physical knob and LED mappings. Wet is mux channel 0 but LED
  column 1; dry is mux channel 1 but LED column 0.
- Prepare cached weights outside the audio callback. Treat the active `Parameters`
  as immutable; publish a fully prepared inactive table at a block boundary.
  The two-table handoff relies on the single-core main-loop/audio-ISR model.
- Keep history capacity fixed and private to each effect/channel. Evict one
  occurrence of the oldest sample so duplicate amplitudes remain valid.
- Validate length as an integer in `1..kMaxLength`, defaulting to 64. Shrinks
  retain the newest samples; grows fill with new input and must not resurrect
  discarded history. Rebuild only the retained sorted window on a shrink.
- Store geometric weight coefficients per startup size, not a quadratic table
  of individual rank weights. Cache rows only while center/sharpness match.
- Compute the decay step with `-expm1(-slope)` and subtract `weight * step`
  when advancing ranks; this avoids rounding a near-one decay factor across
  long lists. Sum the normalization in double precision outside the audio
  callback so small tail weights are retained. Keep the prototype comparison
  accurate at maximum length.
- Length is Read Amount / SW6 (mux channel 4, LED column 4). Keep its integer
  tick arithmetic wide before clamping so large signed deltas cannot wrap.
- LFO rate is Erase Amount / SW7 (channel/LED 5), logarithmic from 0.05 to
  20 Hz; depth is Feedback / SW8 (channel/LED 6), linear from 0 to 1. Keep
  the length binding in its existing slot when extending the knob table.
- LFO depth is the total normalized sweep span: radius = depth / 2, midpoint
  = clamp(weight_center, radius, 1 - radius). Shift the midpoint rather than
  clipping the waveform. Share one phase across stereo and preserve phase
  across control changes, including depth zero. Advance using the actual
  sample rate and double-precision phase so very slow rates stay accurate.
- At nonzero depth, calculate only the current window's two exponential
  starting weights, reuse its cached rank decay, and accumulate normalization
  during the rank traversal. Never rebuild all startup rows in the callback.
  Depth zero must keep the original cached path and output exactly.
- Preserve the original partially filled window normalization. Sanitize
  non-finite audio before inserting it into sorted history.
- Compensation is strictly a fixed gain based on list length: `sqrt(n)` at
  full amount, where `n` is the filled sample count. Do not add signal-level
  detection, AGC, compression, limiting, or any other dynamics control.
  Prepare the square root outside the audio callback and apply the current
  compensation amount only to wet. Do not rebuild weights for amount changes.
- Keep prototype oracle tests with compensation disabled, and separately
  measure compensated AC RMS across lengths on deterministic broadband input.
  A length-only gain cannot recover spectral nulls or guarantee loudness for
  every signal; it also increases peaks and any off-center DC bias.
- Read and write host PCM in complete `StereoSample` frames. Test partial
  final chunks as well as full chunks when changing the console audio loop.
- Build both `la_sort` firmware and the host demo. Run the `la_sort` CTest
  label, including the ffmpeg roundtrip when available, after DSP or CLI changes.
