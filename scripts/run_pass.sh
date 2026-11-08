#!/usr/bin/env bash
# Запуск программы под P.A.S.S.
#   ./run_pass.sh [-target core2] -- программа [аргументы]
DR_DIR="${DYNAMORIO_DIR:-/opt/dynamorio}"
LIB="$(cd "$(dirname "$0")/.." && pwd)/build/libpass_client.so"
exec "$DR_DIR/bin64/drrun" -c "$LIB" "$@"
