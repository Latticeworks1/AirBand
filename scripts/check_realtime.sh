#!/bin/bash
# Runs the DSP tests under clang's RealtimeSanitizer, which aborts on allocation or locking
# inside the audio callback. Needs LLVM 19+ (Homebrew: brew install llvm).
set -euo pipefail
LLVM=${LLVM:-/opt/homebrew/opt/llvm}
cd "$(dirname "$0")/.."
cmake -S . -B build-rtsan -G Ninja -DCMAKE_BUILD_TYPE=Release -DAIRBAND_RTSAN=ON \
    -DCMAKE_C_COMPILER="$LLVM/bin/clang" -DCMAKE_CXX_COMPILER="$LLVM/bin/clang++" \
    -DCMAKE_OBJCXX_COMPILER="$LLVM/bin/clang++" -DAIRBAND_COPY_AFTER_BUILD=OFF \
    -DFETCHCONTENT_SOURCE_DIR_JUCE="${JUCE_SRC:-$PWD/build/_deps/juce-src}" > /dev/null
ninja -C build-rtsan AirBand_Tests
"$(find build-rtsan -type f -perm +111 -name AirBand_Tests | head -1)" | grep -E "Realtime|FAIL|passed|FAILED"
