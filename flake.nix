{
  description = "A very simple static Gemini server, now with Titan support!";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    systems.url = "github:nix-systems/default";

    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };

    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      self,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };

        meta = with pkgs.lib; {
          description = "A very simple static Gemini server, now with Titan support!";
          homepage = "https://github.com/gemrest/maple";
          license = licenses.gpl3Only;
          maintainers = [ maintainers.Fuwn ];
          mainPackage = "maple";
          platforms = platforms.linux;
        };

        maple = (pkgs.stdenvAdapters.useMoldLinker pkgs.clangStdenv).mkDerivation {
          inherit meta;

          name = "maple";
          version = "0.1.6";
          src = pkgs.lib.cleanSource ./.;

          nativeBuildInputs = with pkgs; [
            ninja
            clang
          ];

          buildInputs = [
            pkgs.libressl.dev
          ];

          buildPhase = ''
            mkdir -p $out/bin
            ninja
          '';

          installPhase = ''
            cp build/maple $out/bin/maple
          '';
        };
      in
      {
        packages = {
          inherit maple;

          default = maple;
        };

        apps = {
          maple = {
            inherit meta;

            type = "app";
            program = "${maple}/bin/maple";
          };

          default = self.apps.${system}.maple;
        };

        devShells.default = import ./shell.nix {
          inherit pkgs;
        };
      }
    );
}
