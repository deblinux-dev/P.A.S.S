#!/usr/bin/env bash
# ============================================================
# P.A.S.S. — Linux-лаунчер
# ------------------------------------------------------------
# Простой запуск программ под P.A.S.S.:
#
#     ./pass [ключи] -- программа [аргументы...]
#
#   -target <имя>     профиль CPU: core2 (по умолчанию),
#                     nehalem, westmere, sandybridge, haswell, native
#   -verbose <N>      диагностика 0..4
#   -no-download      не скачивать DynamoRIO автоматически
#   -h                справка
#   прочие -ключи     передаются клиенту P.A.S.S. как есть
#
# Лаунчер сам находит drrun (рядом с собой, в bin64/, в
# $DYNAMORIO_HOME, в ~/.local/share/PASS/dynamorio или в PATH)
# и при отсутствии скачивает релиз DynamoRIO с GitHub.
# ============================================================
set -u

DR_VERSION="11.91.20763"
DR_URL="https://github.com/DynamoRIO/dynamorio/releases/download/cronbuild-${DR_VERSION}/DynamoRIO-Linux-${DR_VERSION}.tar.gz"

DIR="$(cd "$(dirname "$0")" && pwd)"
CLIENT="$DIR/libpass_client.so"
ALLOW_DL=1

msg()  { printf '%s\n' "$*"; }
fail() { printf 'pass: ошибка: %s\n' "$*" >&2; exit 2; }

help() {
cat <<'EOF'
P.A.S.S. — запуск программ с эмуляцией новых инструкций
на старых CPU (по умолчанию профиль Core 2).

  ./pass [ключи] -- программа [args...]

Ключи:
  -target <имя>   core2 | nehalem | westmere | sandybridge | haswell | native
  -verbose <N>    диагностика 0..4
  -no-download    не скачивать DynamoRIO автоматически
  -h              эта справка

Остальные -ключи передаются клиенту P.A.S.S. без изменений.
EOF
}

# разбор ключей: всё до "--" уходит клиенту
CLIENT_ARGS=()
APP_ARGS=()
HAVE_APP=0
while [ $# -gt 0 ]; do
    if [ "$1" = "--" ]; then
        HAVE_APP=1
        shift
        while [ $# -gt 0 ]; do APP_ARGS+=("$1"); shift; done
        break
    fi
    case "$1" in
        -h|-help|--help)  help; exit 0 ;;
        -no-download)     ALLOW_DL=0 ;;
    esac
    CLIENT_ARGS+=("$1")
    shift
done

[ -f "$CLIENT" ] || fail "libpass_client.so не найден рядом с pass ($CLIENT)"
[ "$HAVE_APP" = 1 ] || fail "не указана программа: pass [ключи] -- программа [args]"

# поиск drrun
DRRUN=""
for c in "$DIR/drrun" "$DIR/bin64/drrun" \
         "${DYNAMORIO_HOME:-/nonexistent}/bin64/drrun" \
         "${XDG_DATA_HOME:-$HOME/.local/share}/PASS/dynamorio/bin64/drrun" \
         "/opt/dynamorio/bin64/drrun"; do
    if [ -x "$c" ]; then DRRUN="$c"; break; fi
done
if [ -z "$DRRUN" ]; then
    DRRUN="$(command -v drrun 2>/dev/null || true)"
fi

# авто-скачивание DynamoRIO
if { [ -z "$DRRUN" ] || [ ! -x "$DRRUN" ]; } && [ "$ALLOW_DL" = 1 ]; then
    DEST="${XDG_DATA_HOME:-$HOME/.local/share}/PASS"
    msg "pass: DynamoRIO не найден — скачиваю релиз $DR_VERSION..."
    mkdir -p "$DEST" || fail "не создать каталог $DEST"
    T="$(mktemp -d)"
    trap 'rm -rf "$T"' EXIT
    curl -L --fail -o "$T/dr.tar.gz" "$DR_URL" \
        || fail "не удалось скачать DynamoRIO (нужен интернет или ручная установка: положите drrun рядом с pass)"
    tar xzf "$T/dr.tar.gz" -C "$T" || fail "не удалось распаковать DynamoRIO"
    rm -rf "$DEST/dynamorio"
    mv "$T/DynamoRIO-Linux-$DR_VERSION" "$DEST/dynamorio" || fail "не установить DynamoRIO в $DEST"
    DRRUN="$DEST/dynamorio/bin64/drrun"
fi

[ -n "$DRRUN" ] && [ -x "$DRRUN" ] \
    || fail "drrun не найден: положите его рядом с pass, задайте DYNAMORIO_HOME или уберите -no-download"

if [ -n "${PASS_LAUNCHER_DEBUG:-}" ]; then
    msg "drrun: $DRRUN"
    msg "client: $CLIENT"
fi

exec "$DRRUN" -c "$CLIENT" ${CLIENT_ARGS[@]+"${CLIENT_ARGS[@]}"} -- \
     ${APP_ARGS[@]+"${APP_ARGS[@]}"}
