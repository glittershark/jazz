{ pkgs ? import <nixpkgs> { } }:

with pkgs;

mkShell {
  buildInputs = [ python3 gcc-arm-embedded dfu-util bear uucp cmake gtest ];
}
