{
  description = "Dev shell with GCC 16, Clang 22, CMake 4.3.3";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Override CMake – same version, same source as your devenv
        cmake = pkgs.cmake.overrideAttrs (old: rec {
          version = "4.3.3";
          src = pkgs.fetchurl {
            url = "https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}.tar.gz";
            hash = "sha256-y6S7ekTt8od7tvBZkyiWODur5DWzqMO130i0qkHJu4U=";
          };
          patches = [ ];
          meta = old.meta // {
            priority = 0;
          };
        });

        # Compiler toolchains
        gcc = pkgs.gcc16;
        gccUnwrapped = gcc.cc;

        llvm = pkgs.llvmPackages_22;
        clang = llvm.libstdcxxClang;
        llvmTools = llvm.llvm;
        lld = llvm.lld;

        glibcDev = pkgs.glibc.dev;

        # Include flags from your devenv
        flags =
          with builtins;
          concatStringsSep " " [
            "-isystem ${gccUnwrapped}/include/c++/${gccUnwrapped.version}"
            "-isystem ${gccUnwrapped}/include/c++/${gccUnwrapped.version}/x86_64-unknown-linux-gnu"
            "-isystem ${gccUnwrapped}/include/c++/${gccUnwrapped.version}/backward"
            "-isystem ${glibcDev}/include"

            # XCB / Wayland / xkbcommon headers (needed by Vulkan & GLFW)
            "-isystem ${pkgs.libxcb.dev}/include"
            "-isystem ${pkgs.wayland.dev}/include"
            "-isystem ${pkgs.libxkbcommon.dev}/include"
          ];

        # Hardening: disable only "fortify" (keep everything else that nixpkgs enables)
        hardeningDisableFortify = "stackprotector pie pic strictoverflow format relro bindnow";

        # Extra CMake flags for this environment (Clang + LTO)
        extraFlags = builtins.concatStringsSep " " [
          "-DCMAKE_CXX_COMPILER_AR=${llvmTools}/bin/llvm-ar"
          "-DCMAKE_CXX_COMPILER_RANLIB=${llvmTools}/bin/llvm-ranlib"
          "-DCMAKE_LINKER_TYPE=LLD"
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          # Packages available in the shell
          nativeBuildInputs = [
            pkgs.git
            gcc
            clang
            lld
            cmake
            pkgs.ninja
            glibcDev
            llvmTools
            pkgs.perf

            pkgs.tbb.dev
            pkgs.vulkan-loader
            pkgs.vulkan-validation-layers

            pkgs.python3

            pkgs.pkg-config

            pkgs.libx11
            pkgs.libxrandr
            pkgs.libxinerama
            pkgs.libxcursor
            pkgs.libxi
            pkgs.libxext
            pkgs.libxxf86vm
            pkgs.libxdamage
            pkgs.libxfixes

            pkgs.libxcb

            pkgs.wayland
            pkgs.wayland-scanner
            pkgs.wayland-protocols
            pkgs.libxkbcommon

            pkgs.libxscrnsaver
            pkgs.libXtst
            pkgs.libglvnd
            pkgs.libpulseaudio
            pkgs.alsa-lib
            pkgs.libjack2
            pkgs.pipewire

            pkgs.freetype
            pkgs.harfbuzz
            pkgs.libpng
            pkgs.zlib
            pkgs.libtiff
            pkgs.libffi

            pkgs.dbus
            pkgs.ibus
            pkgs.libusb1
            pkgs.libunwind
            pkgs.liburing
            pkgs.sysprof

            pkgs.sndio
            pkgs.libdrm
            pkgs.mesa
            pkgs.libdecor
          ];

          env = {
            CXXFLAGS = flags;
            CFLAGS = flags;

            NIX_LDFLAGS =
              with builtins;
              concatStringsSep " " [
                "-L${gccUnwrapped}/lib"
                "-L${gccUnwrapped}/lib64"
              ];

            LD_LIBRARY_PATH =
              with pkgs;
              lib.makeLibraryPath ([
                vulkan-loader
                libx11
                libxrandr
                libxinerama
                libxcursor
                libxi
                libxext
                libxxf86vm
                libxdamage
                libxfixes
                libxcb
                wayland
                wayland-protocols
                libxkbcommon
                libxscrnsaver
                libXtst
                libglvnd
                libpulseaudio
                alsa-lib
                libjack2
                pipewire
                freetype
                harfbuzz
                libpng
                zlib
                libtiff
                libffi
                dbus
                ibus
                libusb1
                libunwind
                liburing
                sysprof
              ])
              + ":/run/opengl-driver/lib";

            PKG_CONFIG_PATH =
              with pkgs;
              lib.makeSearchPath "lib/pkgconfig" [
                wayland.dev
                wayland-protocols
                libxkbcommon.dev
                libxcb.dev
                libx11.dev
                libxrandr.dev
                libxinerama.dev
                libxcursor.dev
                libxi.dev
                libxext.dev
                libxxf86vm.dev
                libxdamage.dev
                libxfixes.dev
                libxscrnsaver
                libXtst
                libglvnd.dev
                libpulseaudio.dev
                alsa-lib.dev
                libjack2.dev
                pipewire.dev
                freetype.dev
                harfbuzz.dev
                libpng.dev
                zlib.dev
                libtiff.dev
                libffi.dev
                dbus.dev
                ibus.dev
                libusb1.dev
                sysprof.dev
              ];

            VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

            TBB_DIR = "${pkgs.tbb.dev}/lib/cmake/TBB";
            CXX_MODULES_JSON = "${gccUnwrapped}/lib/libstdc++.modules.json";

            USE_LLVM_LTO = "1";

            # Disable fortify hardening only
            NIX_HARDENING_ENABLE = hardeningDisableFortify;
          };

          # Shell hook: runs after entering the shell (enterShell equivalent)
          shellHook = ''
            export PROJECT_ROOT="$PWD"
            export CC="${clang}/bin/clang"
            export CXX="${clang}/bin/clang++"

            echo "C compiler:   $CC   ($( $CC   --version | head -n1 ))"
            echo "C++ compiler: $CXX ($( $CXX --version | head -n1 ))"

            settings --cmake-extra-flags "${extraFlags}"
          '';
        };
      }
    );
}
