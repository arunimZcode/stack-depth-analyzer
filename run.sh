#!/bin/sh
# run.sh — Compile a C file and run the stack analyzer on it
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ANALYZER="$SCRIPT_DIR/build/stack-analyzer"
CLANG="/usr/lib/llvm17/bin/clang"

# Check analyzer is built
if [ ! -f "$ANALYZER" ]; then
  echo "Analyzer not built. Running build.sh first..."
  "$SCRIPT_DIR/build.sh"
fi

# --all flag: run all test cases
if [ "$1" = "--all" ]; then
  echo "=== Running all test cases ==="
  echo ""

  run_test() {
    FILE="$1"
    THRESHOLD="${2:-4096}"
    NAME=$(basename "$FILE" .c)
    echo "────────────────────────────────────────"
    echo "TEST: $NAME  (threshold=${THRESHOLD}B)"
    echo "────────────────────────────────────────"
    BC="/tmp/${NAME}.bc"
    $CLANG -O0 -emit-llvm -c "$FILE" -o "$BC" 2>/dev/null
    $ANALYZER "$BC" --threshold="$THRESHOLD"
    rm -f "$BC"
    echo ""
  }

  run_test "$SCRIPT_DIR/testcases/test_simple.c"          512
  run_test "$SCRIPT_DIR/testcases/test_deep_chain.c"      512
  run_test "$SCRIPT_DIR/testcases/test_recursion.c"       4096
  run_test "$SCRIPT_DIR/testcases/test_multiple_paths.c"  512
  run_test "$SCRIPT_DIR/testcases/test_large_buffers.c"   1024
  run_test "$SCRIPT_DIR/testcases/test_no_overflow.c"     8192

  echo "=== All test cases complete ==="
  exit 0
fi

# Single file mode
if [ -z "$1" ]; then
  echo "Usage:"
  echo "  ./run.sh <file.c> [--threshold=N]"
  echo "  ./run.sh <file.bc> [--threshold=N]"
  echo "  ./run.sh --all"
  exit 1
fi

INPUT="$1"
shift
THRESHOLD_ARG="${1:---threshold=4096}"

# If .c file, compile to .bc first
if echo "$INPUT" | grep -q "\.c$"; then
  echo "Compiling $INPUT to LLVM bitcode..."
  BC="/tmp/$(basename "$INPUT" .c).bc"
  $CLANG -O0 -emit-llvm -c "$INPUT" -o "$BC" 2>/dev/null
  echo "Running stack analysis..."
  echo ""
  $ANALYZER "$BC" "$THRESHOLD_ARG"
  rm -f "$BC"
else
  # Already a .bc file
  echo "Running stack analysis on $INPUT..."
  echo ""
  $ANALYZER "$INPUT" "$THRESHOLD_ARG"
fi
