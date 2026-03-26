{ pkgs ? import (import ./nix/sources.nix { }).nixpkgs { } }:

let
  lib = pkgs.lib;
in
pkgs.mkShell {
  buildInputs =
    [
      (pkgs.python3.withPackages
        (p: with p; [ ipython matplotlib jupyter jupyterlab notebook tqdm ]))
      pkgs.gcc-arm-embedded
      pkgs.dfu-util
      pkgs.uucp
      pkgs.cmake
      pkgs.gtest
      pkgs.git
      # Workaround https://github.com/NixOS/nixpkgs/issues/296348
      (pkgs.rapidcheck.overrideDerivation (oldAttrs: {
        postFixup = ''
          cp -r "$out/share" $dev
        '';
      })).dev
      pkgs.screen
      pkgs.niv
    ]
    ++ lib.optionals (!pkgs.stdenv.isDarwin) [
      pkgs.stlink
    ];
}
