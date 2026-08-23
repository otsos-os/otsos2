#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "  SETUP    ports/doom"
SRC_DIR="$SCRIPT_DIR/doomgeneric"

if [ ! -f "$SRC_DIR/doomgeneric.c" ] && [ ! -f "$SRC_DIR/doomgeneric/doomgeneric.c" ]; then
	cd "$REPO_ROOT"
	git submodule update --init --depth 1 ports/doom/doomgeneric
fi

WAD_TARGET="$SCRIPT_DIR/doom1.wad"
if [ ! -f "$WAD_TARGET" ]; then
	for candidate in \
		/usr/share/games/doom/doom1.wad \
		/usr/share/games/doom/doom.wad \
		/usr/share/games/doom/DOOM1.WAD \
		/usr/local/share/games/doom/doom1.wad \
		"$REPO_ROOT/doom1.wad" \
		"$REPO_ROOT/usr/share/games/doom/doom1.wad"; do
		if [ -f "$candidate" ]; then
			echo "  COPY     $candidate -> $WAD_TARGET"
			cp "$candidate" "$WAD_TARGET"
			break
		fi
	done
fi

if [ ! -f "$WAD_TARGET" ]; then
	echo "warning: doom1.wad not found in host paths (/usr/share/games/doom/doom1.wad)" >&2
fi
