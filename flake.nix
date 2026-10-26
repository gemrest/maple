{
  description = "A small static Gemini server with optional Titan uploads";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.nixpkgs-intel-darwin.url = "github:NixOS/nixpkgs/nixpkgs-26.05-darwin";

  # Tsutsumi calls these outputs directly with its own arguments, so new inputs
  # need defaults and unknown arguments must be accepted.
  outputs =
    {
      nixpkgs,
      nixpkgs-intel-darwin ? nixpkgs,
      self,
      ...
    }:
    let
      forEachSystem = nixpkgs.lib.genAttrs [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];
      packageSets = forEachSystem (
        system:
        if system == "x86_64-darwin" then
          nixpkgs-intel-darwin.legacyPackages.${system}
        else
          nixpkgs.legacyPackages.${system}
      );
    in
    {
      packages = forEachSystem (
        system:
        let
          packageSet = packageSets.${system};
          maple = packageSet.clangStdenv.mkDerivation {
            pname = "maple";
            version = "0.2.0";
            src = packageSet.lib.fileset.toSource {
              root = ./.;
              fileset = packageSet.lib.fileset.unions [
                ./LICENSE
                ./build.ninja
                ./maple
                ./tests
              ];
            };
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
              install -Dm755 build/maple $out/bin/maple
              runHook postInstall
            '';

            meta = {
              description = "A small static Gemini server with optional Titan uploads";
              homepage = "https://github.com/gemrest/maple";
              license = packageSet.lib.licenses.gpl3Only;
              maintainers = [ packageSet.lib.maintainers.Fuwn ];
              mainProgram = "maple";
              platforms = packageSet.lib.platforms.unix;
            };
          };
        in
        {
          inherit maple;

          default = maple;
        }
      );

      apps = forEachSystem (
        system:
        let
          package = self.packages.${system}.maple;
          maple = {
            inherit (package) meta;

            type = "app";
            program = "${package}/bin/maple";
          };
        in
        {
          inherit maple;

          default = maple;
        }
      );

      checks = forEachSystem (system: {
        inherit (self.packages.${system}) maple;
      });

      devShells = forEachSystem (
        system:
        let
          packageSet = packageSets.${system};
          maple = self.packages.${system}.maple;
        in
        {
          default = (packageSet.mkShell.override { stdenv = packageSet.clangStdenv; }) {
            inputsFrom = [ maple ];
          };
        }
      );

      formatter = forEachSystem (system: packageSets.${system}.nixfmt);
    };
}
