with import (builtins.fetchTarball {
  name = "25.05";
  url = "https://github.com/NixOS/nixpkgs/archive/refs/tags/25.05.tar.gz";
  sha256 = "sha256:1915r28xc4znrh2vf4rrjnxldw2imysz819gzhk9qlrkqanmfsxd";
}) {};
mkShell {
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
    ];
  shellHook = ''
    export HISTFILE=.history
  '';
}
