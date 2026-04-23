#!/usr/bin/env python3

import argparse
import csv
import pathlib
import subprocess
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_args():
    parser = argparse.ArgumentParser(
        description="Render a fridge preset LFO trace with matplotlib."
    )
    parser.add_argument(
        "--preset",
        default="presets/host/fridge_demo.toml",
        help="Fridge host preset to inspect.",
    )
    parser.add_argument(
        "--console",
        default="./build-test/src/console/jazz-console",
        help="Path to the built jazz-console binary.",
    )
    parser.add_argument("--lfo-index", type=int, default=0)
    parser.add_argument(
        "--duration",
        type=float,
        default=48000.0,
        help="Trace duration in samples.",
    )
    parser.add_argument("--points", type=int, default=1000)
    parser.add_argument(
        "--output",
        default="fridge_lfo.png",
        help="Output image path, usually .png or .svg.",
    )
    return parser.parse_args()


def load_trace(args):
    command = [
        args.console,
        "--preset",
        args.preset,
        "--fridge-lfo-chart-csv",
        "--fridge-lfo-index",
        str(args.lfo_index),
        "--fridge-lfo-chart-duration",
        str(args.duration),
        "--fridge-lfo-chart-points",
        str(args.points),
    ]

    result = subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    reader = csv.DictReader(result.stdout.splitlines())
    times = []
    values = []
    for row in reader:
        times.append(float(row["time"]))
        values.append(float(row["value"]))

    return times, values


def plot_trace(args, times, values):
    output = pathlib.Path(args.output)

    fig, ax = plt.subplots(figsize=(11, 4.8))
    ax.plot(times, values, linewidth=2.0, color="#1f77b4")
    ax.set_title(f"Fridge LFO {args.lfo_index}")
    ax.set_xlabel("Time (samples)")
    ax.set_ylabel("Value")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output, dpi=160)
    return output


def main():
    args = parse_args()
    try:
        times, values = load_trace(args)
        output = plot_trace(args, times, values)
    except subprocess.CalledProcessError as err:
        sys.stderr.write(err.stderr)
        return err.returncode

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
