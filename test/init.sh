set -euo pipefail

mkdir -p "$TMPDIR/data"

ln bench/data/*.csv "$TMPDIR/data"
