{ pkgs ? import <nixpkgs> { } }:

with pkgs;

mkShell {
  buildInputs = [
    (python3.withPackages
      (p: with p; [ ipython matplotlib jupyter jupyterlab notebook tqdm ]))
    gcc-arm-embedded
    dfu-util
    uucp
    cmake
    gtest
    rapidcheck
  ];
}
