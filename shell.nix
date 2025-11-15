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
    # Workaround https://github.com/NixOS/nixpkgs/issues/296348
    (rapidcheck.overrideDerivation (oldAttrs: {
      postFixup = ''
        cp -r "$out/share" $dev
      '';
    })).dev
  ];
}
