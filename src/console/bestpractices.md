# Host console practices

- Resolve output collisions before starting the decoder, playback, or encoder.
  Require an explicit overwrite choice; empty input and EOF cancel the export.
- Use FFmpeg's no-overwrite mode for fresh and numbered outputs. Enable
  overwriting only for the destination the user explicitly confirmed.
- Never overwrite the input file, including symlink and hard-link aliases:
  decoding and encoding run concurrently, so this would corrupt the source.
- Keep output-selection integration tests in `tests/console_output_test.py`.
  Run the `console` CTest label after changing export behavior.
