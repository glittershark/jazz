# Fridge Clicking TODOs

This note tracks the clicking/feedback-like issues found while bringing the
fridge effect into the terminal demo.

## Fixed or Mitigated

1. LFO clamped at the range edge.
   - Symptom: the first LFO cycle looked clipped or flat.
   - Cause: the LFO used `std::clamp`, so it hit max range and stayed there
     until the grain ended.
   - Current state: the LFO now wraps modularly through its range.

2. LFO started at the midpoint.
   - Symptom: circular LFO motion became negative deltas for half its range.
   - Cause: the transform initialized LFOs at `range * 0.5`, then computed
     `current - initial`.
   - Result: when the LFO wrapped below midpoint, head position deltas went
     negative and clamped to `0`.
   - Current state: transform and console chart traces now start LFOs at `0`.

3. `erase_amount = 0` did not schedule an erase.
   - Symptom: old audio stayed in the buffer and sounded feedback-like even
     with feedback amount at `0`.
   - Cause: `erase_amount` acts like retained amount, but the scheduler only
     erased when `erase_amount > 0`.
   - Current state: the scheduler erases when `erase_amount < 1`.
   - Current meaning:
     - `0.0` means full erase.
     - `0.5` means keep half.
     - `1.0` means no erase.

## Open Questions and TODOs

1. Hard read-head discontinuities.
   - Symptom: clicks when the LFO wraps or teleports.
   - Likely cause: `Sound::Read(position)` switches buffer index immediately
     with no interpolation, window, or crossfade.
   - Example discontinuity: `23999 -> 0`.
   - TODO: add read-head de-clicking for large position jumps.

2. Reverse grain boundary clicks.
   - Symptom: clicks can remain even with grain size set to half the LFO range.
   - Likely cause: the read head reverses direction instantly. Position can be
     continuous while audio still needs a short boundary window.
   - TODO: add a grain-boundary envelope or crossfade for direction changes.

3. Write/erase ordering may preserve too much energy.
   - Symptom: the buffer can still sound lively or feedback-like with explicit
     feedback amount at `0`.
   - Suspicion: reads, writes, and erases can happen on the same moving head.
     Matured updates currently apply erase first, then write, so erase does not
     clear the newly written sample from that same tick.
   - TODO: decide the intended semantics:
     - erase old content before write,
     - erase old and new content together,
     - or write first and then erase.
   - TODO: update `Sound::DoUpdate()` and tests once semantics are chosen.

4. Head position modulation is not truly modular yet.
   - Symptom: the circular LFO works cleanly only for simple start-at-zero
     position modulation.
   - Cause: the transform still applies position modulation with linear delta
     math and `ClampSize(...)`.
   - TODO: make `TargetParameter::kPosition` use modular arithmetic.

5. Feedback config is parsed but not implemented in sound.
   - Symptom: `feedback_amount = 0` does not control much yet.
   - Cause: `feedback.amount` exists in config/UI/preset parsing, but
     `Sound::ProcessSample()` does not use it.
   - TODO: either implement feedback semantics or keep it out of demos until it
     is real.
