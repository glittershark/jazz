"""Exercise real MP3 decoding, presets, CLI overrides, and stereo DSP together."""

import array
from collections import deque
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave


def run(command, data=None):
    result = subprocess.run(command, input=data, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{command} failed:\n{result.stderr.decode()}")
    return result.stdout


def decode(ffmpeg, path, sample_rate=48000):
    pcm = run([ffmpeg, "-v", "error", "-i", str(path), "-f", "f32le",
               "-ac", "2", "-ar", str(sample_rate), "pipe:1"])
    samples = array.array("f")
    samples.frombytes(pcm)
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def reference(samples, config, sample_rate=48000):
    length = config.get("la_sort_length", 64)
    histories = [deque(maxlen=length), deque(maxlen=length)]
    tables = {}
    output = []
    for frame in range(len(samples) // 2):
        count = min(frame + 1, length)
        depth = config.get("la_sort_lfo_depth", 0)
        radius = depth / 2
        center = config["la_sort_weight_center"]
        if depth:
            center = max(radius, min(1 - radius, center)) + radius * math.sin(
                2 * math.pi * frame * config.get("la_sort_lfo_rate", 1) / sample_rate)
        makeup = 1 + config.get("la_sort_volume_compensation", 1) * (
            math.sqrt(count) - 1)
        if depth or count not in tables:
            weights = [math.exp(-config["la_sort_weight_sharpness"] *
                               abs((rank / (count - 1) if count > 1 else 0) -
                                   center))
                       for rank in range(count)]
            total = sum(weights)
            tables[count] = [weight / total for weight in weights]
        for channel in range(2):
            sample = samples[2 * frame + channel]
            histories[channel].append(sample)
            wet = sum(value * weight for value, weight in
                      zip(sorted(histories[channel]), tables[count]))
            output.append(max(-1.0, min(1.0, sample * config["la_sort_dry"] +
                                       wet * config["la_sort_wet"] * makeup)))
    return struct.pack(f"<{len(output)}f", *output)


def check_render(console, ffmpeg, input_path, output_path, preset, config, flags,
                 sample_rate=48000):
    run([console, str(input_path), "--preset", str(preset), "--no-play",
         "--output", str(output_path), *flags])
    source = decode(ffmpeg, input_path, sample_rate)
    expected_path = output_path.with_name("reference.mp3")
    run([ffmpeg, "-v", "error", "-y", "-f", "f32le", "-ac", "2",
         "-ar", str(sample_rate), "-i", "pipe:0", "-codec:a", "libmp3lame", "-q:a",
         "2", str(expected_path)], reference(source, config, sample_rate))
    actual = decode(ffmpeg, output_path, sample_rate)
    expected = decode(ffmpeg, expected_path, sample_rate)
    # Compare identical codec roundtrips: libmp3lame can add tail padding at
    # 44.1 kHz, even when encoding unprocessed PCM of exactly this frame count.
    assert len(actual) == len(expected), (
        f"frame count changed at {sample_rate} Hz: "
        f"actual={len(actual) // 2}, source={len(source) // 2}, expected={len(expected) // 2}")
    if sample_rate == 48000:
        assert len(actual) == len(source), "48 kHz fixture lost its exact frame count"
    rms = math.sqrt(sum((a - b) ** 2 for a, b in zip(actual, expected)) /
                    len(actual))
    assert rms < 0.0005, f"stereo render differs from sorting oracle: RMS={rms}"


def main():
    console, ffmpeg, demo = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="jazz-la-sort-") as temp:
        directory = Path(temp)
        wav = directory / "source.wav"
        # Cross several CLI chunks, ending with a partial chunk of odd length.
        with wave.open(str(wav), "wb") as stream:
            stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            pcm = bytearray()
            for frame in range(6 * 1024 + 137):
                left = 0.5 * math.sin(frame * 0.03) + 0.2 * math.sin(frame * 0.17)
                right = 0.4 * math.cos(frame * 0.07)
                pcm.extend(struct.pack("<hh", int(left * 32767), int(right * 32767)))
            stream.writeframes(pcm)
        source = directory / "source with spaces.mp3"
        run([ffmpeg, "-v", "error", "-i", str(wav), "-codec:a", "libmp3lame",
             "-q:a", "2", str(source)])

        # Read numeric values from this deliberately flat demo TOML.
        config = {}
        for line in Path(demo).read_text().splitlines():
            if line.startswith("la_sort_"):
                key, value = line.split("=", 1)
                config[key.strip()] = (int(value.strip()) if key.strip() == "la_sort_length"
                                       else float(value.strip()))
        output = directory / "processed.mp3"
        check_render(console, ffmpeg, source, output, demo, config, [])

        json_preset = directory / "preset.json"
        json_preset.write_text(json.dumps({"effect_chain": ["la_sort"], **config}))
        overrides = {"la_sort_weight_center": 0.8, "la_sort_weight_sharpness": 32.0,
                     "la_sort_dry": 0.35, "la_sort_wet": 0.65,
                     "la_sort_length": 128, "la_sort_volume_compensation": 0.5}
        flags = [item for key, value in overrides.items()
                 for item in ("--" + key.replace("_", "-"), str(value))]
        check_render(console, ffmpeg, source, output, json_preset,
                     {**config, **overrides}, flags)

        # Fully dry processing must still pass every stereo frame intact.
        dry = {"la_sort_dry": 1.0, "la_sort_wet": 0.0}
        check_render(console, ffmpeg, source, output, demo, {**config, **dry},
                     ["--la-sort-dry", "1", "--la-sort-wet", "0"])

        for length in (1, 17, 1024):
            check_render(console, ffmpeg, source, output, demo,
                         {**config, "la_sort_length": length},
                         ["--la-sort-length", str(length)])

        check_render(console, ffmpeg, source, output, demo,
                     {**config, "la_sort_length": 1024, "la_sort_volume_compensation": 0},
                     ["--la-sort-length", "1024", "--la-sort-volume-compensation", "0"])

        for sample_rate, center, depth in ((48000, 0.1, 1.0), (44100, 0.9, 0.4)):
            modulation = {"la_sort_lfo_rate": 20.0, "la_sort_lfo_depth": depth,
                          "la_sort_weight_center": center, "la_sort_length": 17,
                          "la_sort_volume_compensation": 0.0}
            flags = ["--sample-rate", str(sample_rate)] + [
                item for key, value in modulation.items()
                for item in ("--" + key.replace("_", "-"), str(value))]
            check_render(console, ffmpeg, source, output, demo,
                         {**config, **modulation}, flags, sample_rate)

        # A preset that omits length preserves the original 64-sample behavior.
        legacy = {key: value for key, value in config.items()
                  if key not in ("la_sort_length", "la_sort_lfo_rate", "la_sort_lfo_depth")}
        json_preset.write_text(json.dumps({"effect_chain": ["la_sort"], **legacy}))
        check_render(console, ffmpeg, source, output, json_preset, legacy, [])

        # Exercise non-default lengths loaded from both preset formats.
        for suffix in ("json", "toml"):
            preset = directory / ("length." + suffix)
            values = {**config, "la_sort_length": 128, "la_sort_volume_compensation": 0.5,
                      "la_sort_lfo_rate": 7.5, "la_sort_lfo_depth": 0.4}
            if suffix == "json":
                preset.write_text(json.dumps({"effect_chain": ["la_sort"], **values}))
            else:
                preset.write_text('effect_chain = ["la_sort"]\n' +
                                  "\n".join(f"{key} = {value}" for key, value in values.items()))
            check_render(console, ffmpeg, source, output, preset, values, [])

        for flag, value in [("--la-sort-dry", "-1"), ("--la-sort-wet", "1.1"),
                            ("--la-sort-weight-center", "nan"),
                            ("--la-sort-weight-sharpness", "33"),
                            ("--la-sort-volume-compensation", "-0.1"),
                            ("--la-sort-volume-compensation", "1.1"),
                            ("--la-sort-volume-compensation", "nan"),
                            ("--la-sort-lfo-rate", "0.049"),
                            ("--la-sort-lfo-rate", "20.01"),
                            ("--la-sort-lfo-rate", "nan"),
                            ("--la-sort-lfo-depth", "-0.01"),
                            ("--la-sort-lfo-depth", "1.01"),
                            ("--la-sort-lfo-depth", "inf")]:
            result = subprocess.run([console, "missing.mp3", flag, value],
                                    capture_output=True)
            assert result.returncode == 2, f"accepted invalid {flag} {value}"

        for value in ("0", "1025", "-1", "17.5", "nan", "inf", "999999999999999999999999"):
            for flags in (["--la-sort-length", value], ["--preset", str(json_preset)]):
                json_preset.write_text('{"effect_chain": ["la_sort"], "la_sort_length": "' + value + '"}')
                result = subprocess.run([console, "missing.mp3", *flags], capture_output=True)
                assert result.returncode == 2, f"accepted invalid length {value}"
        result = subprocess.run([console, "missing.mp3", "--la-sort-length"], capture_output=True)
        assert result.returncode == 2, "accepted length without a value"
    print("La Sort MP3 demo, length, sine modulation, presets, overrides, dry stereo, and validation passed")


if __name__ == "__main__":
    main()
