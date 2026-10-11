"""Check export prompts and preservation of existing files with real FFmpeg."""

from pathlib import Path
import subprocess
import sys
import tempfile


def render(console, source, output, directory, choice=""):
    result = subprocess.run(
        [console, str(source), "--effect", "bypass", "--no-play", "--output", output],
        cwd=directory, input=choice, capture_output=True, text=True, timeout=10)
    if result.returncode not in (0, 1):
        raise AssertionError(f"Unexpected exit {result.returncode}: {result.stderr}")
    return result


def check_collisions(console, source, directory):
    requested = "take with 'quotes'.mp3"
    output = directory / "mp3s" / requested
    result = render(console, source, requested, directory)
    assert result.returncode == 0, result.stderr
    assert output.stat().st_size > 0
    assert "already exists" not in result.stderr
    assert not (directory / requested).exists()
    original = output.read_bytes()

    # Skip occupied numbered names, including dangling links and directories.
    numbered = output.with_stem(output.stem + "_1")
    numbered.write_bytes(b"keep this previous export")
    dangling = output.with_stem(output.stem + "_2")
    dangling.symlink_to(directory / "missing.mp3")
    occupied_directory = output.with_stem(output.stem + "_3")
    occupied_directory.mkdir()
    result = render(console, source, requested, directory, "invalid\nn\n")
    assert result.returncode == 0, result.stderr
    assert "Choose n, o, or c." in result.stderr
    assert output.with_stem(output.stem + "_4").read_bytes() == original
    assert output.read_bytes() == original
    assert numbered.read_bytes() == b"keep this previous export"
    assert dangling.is_symlink() and not dangling.exists()
    assert occupied_directory.is_dir()

    for choice in ("c\n", "\n", "", "yes\nc\n"):
        result = render(console, source, requested, directory, choice)
        assert result.returncode == 1, result.stderr
        assert "already exists" in result.stderr
        assert "Export cancelled." in result.stderr
        assert output.read_bytes() == original

    output.write_bytes(b"replace this file only after confirmation")
    result = render(console, source, requested, directory, "o\n")
    assert result.returncode == 0, result.stderr
    assert output.read_bytes() == original

    # Explicit paths use the same collision policy without moving into mp3s/.
    custom = directory / "custom"
    custom.mkdir()
    for requested in ("custom/relative.mp3", str(custom / "absolute.mp3")):
        result = render(console, source, requested, directory)
        assert result.returncode == 0, result.stderr
        result = render(console, source, requested, directory, "n\n")
        assert result.returncode == 0, result.stderr
        path = directory / requested
        assert path.read_bytes() == original
        assert path.with_stem(path.stem + "_1").read_bytes() == original


def check_input_protection(console, source, directory):
    original = source.read_bytes()
    symlink = directory / "source-link.mp3"
    symlink.symlink_to(source)
    hardlink = directory / "source-hardlink.mp3"
    hardlink.hardlink_to(source)
    for output in (source, symlink, hardlink):
        result = render(console, source, str(output), directory, "o\n")
        assert result.returncode == 1, result.stderr
        assert "cannot overwrite the input file" in result.stderr
        assert source.read_bytes() == original

    result = render(console, source, str(source), directory, "n\n")
    assert result.returncode == 0, result.stderr
    assert source.with_stem(source.stem + "_1").stat().st_size > 0
    assert source.read_bytes() == original


def main():
    console, ffmpeg = sys.argv[1:]
    console = str(Path(console).resolve())
    with tempfile.TemporaryDirectory(prefix="jazz-output-") as temp:
        directory = Path(temp)
        source = directory / "source.mp3"
        subprocess.run(
            [ffmpeg, "-v", "error", "-f", "lavfi", "-i",
             "sine=frequency=440:duration=0.1", str(source)],
            check=True, capture_output=True, timeout=10)
        check_collisions(console, source, directory)
        check_input_protection(console, source, directory)
    print("Output numbering, overwrite, cancel, explicit paths, and input protection passed")


if __name__ == "__main__":
    main()
