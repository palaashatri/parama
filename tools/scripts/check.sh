#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
: "${CC:=clang}"
mkdir -p build/tests
"$CC" -std=c2x -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Ibrahma/include -Ithird_party tools/tests/test_bhumi.c brahma/core/bhumi/boot_alloc.c \
    -o build/tests/test_bhumi
"$CC" -std=c2x -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Ibrahma/include -Ithird_party tools/tests/test_display.c brahma/core/display/boot_display.c \
    -o build/tests/test_display
./build/tests/test_bhumi
"$CC" -std=c2x -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Iinclude tools/tests/test_intelligence_abi.c \
    -o build/tests/test_intelligence_abi
./build/tests/test_display
./build/tests/test_intelligence_abi
python3 tools/tests/check_elf.py build/brahma.elf
