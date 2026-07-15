#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "  SETUP    ports/lua"
if [ ! -f "$SCRIPT_DIR/lua/makefile" ]; then
	cd "$REPO_ROOT"
	git submodule update --init --depth 1 ports/lua/lua
fi
if [ -f "$SCRIPT_DIR/lua/makefile" ] && \
   grep -q 'clang --target=x86_64-linux-musl' "$SCRIPT_DIR/lua/makefile" 2>/dev/null; then
	echo "  PATCH    arleady"
else
	cd "$SCRIPT_DIR/lua"
	patch -p0 < "$SCRIPT_DIR/diff.patch"
	echo "  PATCH    applied"
fi
