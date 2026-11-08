#!/usr/bin/env bash
# Сборка P.A.S.S. + прогон самопроверки
set -e
DR_DIR="${DYNAMORIO_DIR:-/opt/dynamorio/cmake}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build && cd build
cmake .. -DDynamoRIO_DIR="$DR_DIR"
make -j"$(nproc)"
echo "── libpass_client.so собран: $ROOT/build/libpass_client.so"

# самопроверка на изолированных asm-пробах (без glibc-стартапа)
if [ -d /tmp/passtests ] && [ -x "$DR_DIR/../bin64/drrun" ]; then
    DRRUN="$DR_DIR/../bin64/drrun"
    pass=0; fail=0
    for t in /tmp/passtests/t_*; do
        [ -x "$t" ] || continue
        # нативный результат
        "$t" > /tmp/pass_nat.txt 2>/dev/null || true
        # под профилем core2 (форсированная эмуляция)
        "$DRRUN" -c "$ROOT/build/libpass_client.so" -target core2 -- "$t" \
            > /tmp/pass_emu.txt 2>/dev/null || true
        if diff -q <(grep -v '^\[' /tmp/pass_nat.txt) \
                   <(grep -v '^\[' /tmp/pass_emu.txt) >/dev/null 2>&1; then
            echo "PASS $(basename "$t")"; pass=$((pass+1))
        else
            echo "FAIL $(basename "$t")"; fail=$((fail+1))
        fi
    done
    echo "── самопроверка: pass=$pass fail=$fail"
fi
