#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "  SETUP    ports/cpython"
MUSL_DIR="../../../libc/musl"
SRC_DIR="$SCRIPT_DIR/cpython"
if [ ! -f "$SRC_DIR/configure" ]; then
	cd "$REPO_ROOT"
	git submodule update --init ports/cpython/cpython
fi
if grep -q 'otsos: all' "$SRC_DIR/Makefile.pre.in" 2>/dev/null; then
	echo "  PATCH    already"
else
	cd "$SRC_DIR"
	patch -p0 < "$SCRIPT_DIR/diff.patch"
	echo "  PATCH    applied"
fi
if [ ! -f "$SRC_DIR/config.site" ]; then
	cp "$SCRIPT_DIR/config.site" "$SRC_DIR/config.site"
	echo "  CONFIG   config.site copied"
fi
if [ ! -f "$SRC_DIR/Makefile" ]; then
	echo "  CONFIG   running configure"
	cd "$SRC_DIR"
	CONFIG_SITE=config.site \
	CFLAGS="-O2 -g -fPIE -nostdinc -I$MUSL_DIR/arch/x86_64 -I$MUSL_DIR/arch/generic -I$MUSL_DIR/obj/include -I$MUSL_DIR/include" \
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
		--with-build-python=$(command -v python3) \
		--with-ensurepip=no \
		--with-system-ffi=no \
		2>&1 | tee configure.log
	echo "  CONFIG   done"
fi
