#!/bin/sh
# Build & run the osx-arm64 platform-port unit tests.
#
# These exercise the Apple Silicon (arm64) implementations of the replicant
# foundation/nx primitives: types, atomics, the lock-free LIFO and NXOnce.
# Everything is compiled with `-arch arm64` so the macro dispatch in the
# foundation/nx headers selects the osx-arm64 sources.
#
# Usage: sh run_tests.sh
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
# replicant root is three levels up: .../replicant/tests/osx-arm64
ROOT=$(cd "$HERE/../.." && pwd)

CC=${CC:-clang}
ARCH_FLAGS="-arch arm64"
CFLAGS="$ARCH_FLAGS -I$ROOT -Wall -Wextra -O2 -pthread"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fail=0
run() {
  name=$1
  shift
  bin="$TMP/$name"
  if ! $CC $CFLAGS "$@" -o "$bin"; then
    echo "BUILD FAILED: $name"
    fail=1
    return
  fi
  if ! "$bin"; then
    fail=1
  fi
}

echo "== osx-arm64 platform-port tests =="
run test_types   "$HERE/test_types.c"
run test_atomics "$HERE/test_atomics.c"
run test_lifo    "$HERE/test_lifo.c"
run test_nxonce  "$HERE/test_nxonce.c" "$ROOT/nx/osx-arm64/nxonce.c"

if [ "$fail" -eq 0 ]; then
  echo "== ALL TESTS PASSED =="
  exit 0
fi
echo "== TESTS FAILED =="
exit 1
