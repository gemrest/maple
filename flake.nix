{
  description = "A small static Gemini server with optional Titan uploads";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    systems.url = "github:nix-systems/default";
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
        packageSet = import nixpkgs { inherit system; };
        maple = packageSet.clangStdenv.mkDerivation {
          pname = "maple";
          version = "0.1.6";
          src = packageSet.lib.cleanSource ./.;
          nativeBuildInputs = [
            packageSet.ninja
            packageSet.pkg-config
          ];
          buildInputs = [ packageSet.openssl ];
          nativeCheckInputs = [
            packageSet.python3
            packageSet.openssl
          ];
          doCheck = true;
          buildPhase = ''
            runHook preBuild
            ninja
            runHook postBuild
          '';
          checkPhase = ''
            runHook preCheck
            ninja check
            runHook postCheck
          '';
          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin
            cp build/maple $out/bin/maple
            runHook postInstall
          '';
          meta = with packageSet.lib; {
            description = "A small static Gemini server with optional Titan uploads";
            homepage = "https://github.com/gemrest/maple";
            license = licenses.gpl3Only;
            maintainers = [ maintainers.Fuwn ];
            mainProgram = "maple";
            platforms = platforms.unix;
          };
        };
      in
      {
        packages = {
          inherit maple;
          default = maple;
        };
        apps = {
          maple = {
            inherit (maple) meta;
            type = "app";
            program = "${maple}/bin/maple";
          };
          default = self.apps.${system}.maple;
        };
        devShells.default = (packageSet.mkShell.override { stdenv = packageSet.clangStdenv; }) {
          inputsFrom = [ maple ];
        };
      }
    );
}
