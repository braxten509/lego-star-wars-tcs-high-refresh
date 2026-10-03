#!/bin/bash
# Installs the high refresh mod. usage: install.sh [game dir] [compatdata dir]
# Uninstall: delete dinput8.dll and dxvk.conf from the game folder.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${1:-$HOME/.local/share/Steam/steamapps/common/Lego Star Wars Saga}"
PREFIX="${2:-$HOME/.local/share/Steam/steamapps/compatdata/32440}"
REG="$PREFIX/pfx/user.reg"
cp "$HERE/dinput8.dll" "$GAME/dinput8.dll"
# Wine only loads a game-folder dinput8.dll when told to prefer it.
if ! grep -q '^"dinput8"=' "$REG"; then
  cp "$REG" "$REG.before-high-refresh-mod"
  sed -i '/^\[Software\\\\Wine\\\\DllOverrides\]/{n;a "dinput8"="native,builtin"
}' "$REG"
fi
grep -n '^"dinput8"=' "$REG"
