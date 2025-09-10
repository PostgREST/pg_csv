with import (builtins.fetchTarball {
  name = "2025-06-16";
  url = "https://github.com/NixOS/nixpkgs/archive/e6f23dc08d3624daab7094b701aa3954923c6bbb.tar.gz";
  sha256 = "sha256:0m0xmk8sjb5gv2pq7s8w7qxf7qggqsd3rxzv3xrqkhfimy2x7bnx";
}) {};
mkShellNoCC {
  buildInputs =
    let
    xpg = import (fetchFromGitHub { owner  = "steve-chavez";
      repo   = "xpg";
      rev    = "v1.5.2";
      sha256 = "sha256-NwhOi/BAZX0JdtFhtV3wgjagNTO5Kmq2Oy3sa+GyDv8=";
    });
    style =
      writeShellScriptBin "pg_csv-style" ''
        ${clang-tools}/bin/clang-format -i src/*
      '';
    styleCheck =
      writeShellScriptBin "pg_csv-style-check" ''
        ${clang-tools}/bin/clang-format -i src/*
        ${git}/bin/git diff-index --exit-code HEAD -- '*.c'
      '';
    loadtest =
      writeShellScriptBin "pg_csv-loadtest" ''
        set -euo pipefail

        file=./bench/$1.sql

        cat <<EOF
        pgbench running for:

        \`\`\`sql
        $(< $file)
        \`\`\`

        results:

        \`\`\`
        $(${xpg.xpg}/bin/xpg pgbench -n -c 1 -T 30 -M prepared -f $file)
        \`\`\`

        EOF
      '';
    in [
      xpg.xpg
      style
      styleCheck
      loadtest
      gcc15
    ];
  shellHook = ''
    export HISTFILE=.history
  '';
}
