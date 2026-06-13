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
      pkgs.clang-tools
      pkgs.git
      pkgs.jq
      pkgs.ffmpeg
      # Workaround https://github.com/NixOS/nixpkgs/issues/296348
      (pkgs.rapidcheck.overrideDerivation (oldAttrs: {
        postFixup = ''
          cp -r "$out/share" $dev
        '';
      })).dev
      pkgs.llvmPackages.compiler-rt.dev
      pkgs.screen
      pkgs.niv
    ]
    ++ lib.optionals (!pkgs.stdenv.isDarwin) [
      pkgs.stlink
      pkgs.pkgsi686Linux.glibc.dev  # 32-bit headers for clang-tidy with ARM
    ];
}
