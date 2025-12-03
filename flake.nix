{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
        {
          packages.libreactor = pkgs.stdenv.mkDerivation {
            pname = "libreactor";
            version = "1.0";
            buildInputs = with pkgs;[ gcc openjdk21 liburing.dev ];
            src = ./.;
            buildPhase = ''
                ${pkgs.openjdk21}/bin/javac -h . Reactor.java
                ${pkgs.gcc}/bin/gcc -shared -fpic \
                -o libreactor.so \
                -Wall -Wextra \
                -I"${pkgs.openjdk21}/include" \
                -I"${pkgs.openjdk21}/include/linux" \
                -luring \
                Reactor.c Reactor.h
              '';
            installPhase = ''
              mkdir -p $out/classes $out/lib
              mv *.class $out/classes
              mv *.so $out/lib
              echo '{:paths ["classes"]}' > $out/deps.edn
            '';
            postBuild = "rm -f Reactor.h";
            JAVA_HOME = pkgs.openjdk21;
            NIX_LD_LIBRARY_PATH = [ "$out/lib" ];
          };
        }
    );
}
