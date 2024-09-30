{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  buildInputs = with pkgs; [
    libressl_3_9
    ninja
    clang_19
    (llvmPackages_19.clang-tools.override { enableLibcxx = true; })
  ];
}
