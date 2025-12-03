{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    parentFlake.url = "..";
  };

  outputs = { self, nixpkgs, flake-utils, parentFlake }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        libreactor = parentFlake.packages.${system}.libreactor;
        depsEdn = pkgs.writeText "deps.edn" ''
          {:paths []

           :deps {org.clojure/clojure {:mvn/version "1.12.3"}

                  reactor/reactor {:local/root "${libreactor}"}}

           :aliases
           {:test
            {:jvm-opts      ["-Djava.library.path=${libreactor}/lib"]
             :extra-deps    {lambdaisland/kaocha {:mvn/version "1.91.1392"}}
             :extra-paths   ["test"]
             :exec-fn       kaocha.runner/exec-fn
             :main-opts     ["-m" "kaocha.runner"]}}}
        '';
        testScript = pkgs.writeShellScriptBin "tests" ''
          cp ${depsEdn} ./deps.edn
          ${pkgs.clojure}/bin/clojure -M:test
        '';
      in
        {
          apps.default = {
            type = "app";
            buildInputs = with pkgs;[ clojure ];
            program = "${testScript}/bin/tests";
          };

          devShell = pkgs.mkShell {
            buildInputs = with pkgs; [ clojure ];
          };
        }
    );
}
