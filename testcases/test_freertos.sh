#!/bin/sh
# Test 7: FreeRTOS kernel evaluation
set -e

ANALYZER="$HOME/stack-analyzer/build/stack-analyzer"
FREERTOS="$HOME/FreeRTOS-Kernel"
CLANG="/usr/lib/llvm17/bin/clang"
LLVM_LINK="/usr/bin/llvm-link"

echo "=== FreeRTOS Stack Analysis ==="
echo ""

if [ ! -d "$FREERTOS" ]; then
  echo "Cloning FreeRTOS Kernel..."
  git clone --depth=1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git "$FREERTOS"
fi

cd "$FREERTOS"

echo "Compiling FreeRTOS to LLVM bitcode..."
for f in *.c; do
  $CLANG -O0 -emit-llvm -c "$f" \
    -I./include \
    -I./portable/GCC/ARM_CM3 \
    -I./portable/Common \
    -o "${f%.c}.bc" 2>/dev/null
done

echo "Linking bitcode modules..."
$LLVM_LINK *.bc -o freertos.bc

echo ""
echo "--- Threshold: 4096 bytes (typical RTOS task stack) ---"
$ANALYZER freertos.bc --threshold=4096

echo ""
echo "--- Threshold: 256 bytes (tight embedded stack) ---"
$ANALYZER freertos.bc --threshold=256

# Cleanup
rm -f *.bc
