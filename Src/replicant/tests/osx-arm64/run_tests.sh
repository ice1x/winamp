#!/bin/sh
# Build & run the osx-arm64 platform-port unit tests.
#
# These exercise the Apple Silicon (arm64) implementations of the replicant
# foundation/nx primitives: types, atomics, the lock-free LIFO, NXOnce, the
# threading/synchronization/sleep primitives (thread, semaphore, condition,
# sleep), and the string layer (nxstring UTF-8/16/32 transcoding + charset
# conversion, nxmutablestring).
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
run test_sleep     "$HERE/test_sleep.c"     "$ROOT/nx/osx/nxsleep.c"
run test_semaphore "$HERE/test_semaphore.c" "$ROOT/nx/osx/nxsemaphore.c"
run test_condition "$HERE/test_condition.c" "$ROOT/nx/osx/nxcondition.c"
run test_thread    "$HERE/test_thread.c"    "$ROOT/nx/osx/nxthread.c"
run test_string        "$HERE/test_string.c"        "$ROOT/nx/osx/nxstring.c"
run test_mutablestring "$HERE/test_mutablestring.c" "$ROOT/nx/osx/nxstring.c" "$ROOT/nx/osx/nxmutablestring.c"

if [ "$fail" -eq 0 ]; then
  echo "== ALL TESTS PASSED =="
  exit 0
fi
echo "== TESTS FAILED =="
exit 1
