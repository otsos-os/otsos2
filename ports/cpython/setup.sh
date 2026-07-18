#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "  SETUP    ports/cpython"
MUSL_DIR="../../../libc/musl"
SRC_DIR="$SCRIPT_DIR/cpython"
SUBMODULE_PATH="ports/cpython/cpython"
SUBMODULE_URL="https://github.com/python/cpython.git"
SUBMODULE_BRANCH="3.14"
find_compiler_headers() {
	resource_dir="$(clang --target=x86_64-linux-musl -print-resource-dir 2>/dev/null || true)"
	if [ -z "$resource_dir" ] || [ ! -d "$resource_dir/include" ]; then
		echo "error: clang compiler headers not found" >&2
		return 1
	fi
	printf '%s\n' "$resource_dir/include"
}

find_build_python() {
	for py in "${CPYTHON_BUILD_PYTHON:-}" python3.14 /home/linuxbrew/.linuxbrew/bin/python3.14 /home/linuxbrew/.linuxbrew/bin/python3 python3; do
		[ -n "$py" ] || continue
		if [ -x "$py" ]; then
			py_path="$py"
		else
			py_path="$(command -v "$py" 2>/dev/null || true)"
		fi
		[ -n "$py_path" ] || continue
		if "$py_path" -c 'import sys; raise SystemExit(sys.version_info[:2] != (3, 14))'; then
			printf '%s\n' "$py_path"
			return 0
		fi
	done
	return 1
}

if [ ! -f "$SRC_DIR/configure" ]; then
	cd "$REPO_ROOT"
	if git ls-files --error-unmatch "$SUBMODULE_PATH" >/dev/null 2>&1; then
		git submodule update --init --depth 1 --remote "$SUBMODULE_PATH"
	else
		git submodule add --depth 1 --branch "$SUBMODULE_BRANCH" "$SUBMODULE_URL" "$SUBMODULE_PATH"
	fi
fi
if grep -q 'otsos: all' "$SRC_DIR/Makefile.pre.in" 2>/dev/null; then
	echo "  PATCH    already"
else
	cd "$SRC_DIR"
	patch -p1 < "$SCRIPT_DIR/diff.patch"
	echo "  PATCH    applied"
fi
if [ ! -f "$SRC_DIR/config.site" ] || ! cmp -s "$SCRIPT_DIR/config.site" "$SRC_DIR/config.site"; then
	cp "$SCRIPT_DIR/config.site" "$SRC_DIR/config.site"
	echo "  CONFIG   config.site copied"
fi
if [ ! -f "$SRC_DIR/Makefile" ]; then
	echo "  CONFIG   running configure"
	cd "$SRC_DIR"
	BUILD_PYTHON="$(find_build_python)" || {
		echo "error: Python 3.14 is required for CPython 3.14 build"
		exit 1
	}
	COMPILER_HEADERS="$(find_compiler_headers)"
	echo "  CONFIG   compiler headers: $COMPILER_HEADERS"
	CONFIG_SITE=config.site \
	PKG_CONFIG="${PKG_CONFIG:-false}" \
	CFLAGS="-O2 -g -fPIE -nostdinc -isystem $COMPILER_HEADERS -I$MUSL_DIR/arch/x86_64 -I$MUSL_DIR/arch/generic -I$MUSL_DIR/obj/include -I$MUSL_DIR/include" \
	CPPFLAGS="-isystem $COMPILER_HEADERS" \
	LDFLAGS_NODIST="-L$MUSL_DIR/lib -Wl,-m,elf_x86_64" \
	LINKFORSHARED="-Wl,-pie -Wl,--oformat=elf64-x86-64 -Wl,-dynamic-linker,/lib/ld-musl-x86_64.so.1" \
	./configure \
		--host=x86_64-linux-musl \
		--build=x86_64-linux-gnu \
		CC="clang --target=x86_64-linux-musl" \
		AR="llvm-ar" \
		--disable-ipv6 \
		--without-ensurepip \
		--disable-test-modules \
		--with-build-python="$BUILD_PYTHON" \
		--with-ensurepip=no \
		--with-system-ffi=no \
		>configure.log 2>&1 || {
			cat configure.log
			exit 1
		}
	cat configure.log
	echo "  CONFIG   done"
fi
