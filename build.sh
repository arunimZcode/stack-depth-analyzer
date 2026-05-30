#!/bin/sh
# build.sh — Build the Stack Depth Analyzer
set -e

echo "=== Stack Depth Analyzer — Build ==="

# Check dependencies
echo "Checking dependencies..."
command -v cmake  >/dev/null 2>&1 || { echo "ERROR: cmake not found. Run: apk add cmake"; exit 1; }
command -v ninja  >/dev/null 2>&1 || { echo "ERROR: ninja not found. Run: apk add ninja"; exit 1; }

if [ ! -f "/usr/lib/llvm17/lib/cmake/llvm/LLVMConfig.cmake" ]; then
  echo "ERROR: LLVM17 dev not found. Run: apk add llvm17-dev llvm17-static clang17-dev"
  exit 1
fi

# Create missing dummy libs if needed (Alpine LLVM17 package quirk)
for lib in LLVMTestingAnnotations LLVMTestingSupport LLVMBenchmarkSupport llvm_gtest llvm_gtest_main; do
  [ ! -f "/usr/lib/llvm17/lib/lib${lib}.a" ] && ar rcs "/usr/lib/llvm17/lib/lib${lib}.a"
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p build
cd build

echo "Running CMake..."
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  2>&1 | grep -E "Found LLVM|Error|error" || true

echo "Building..."
ninja

echo ""
echo "Build complete! Binary: $SCRIPT_DIR/build/stack-analyzer"
echo ""
echo "Usage:"
echo "  ./run.sh testcases/test_simple.c"
echo "  ./run.sh testcases/test_deep_chain.c --threshold=512"
echo "  ./run.sh --all"
