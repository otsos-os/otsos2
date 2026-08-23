TARGET ?= x86_64-elf
SHELL ?= sh

MUSL_DIR = ../libc/musl
MUSL_LIBC = $(MUSL_DIR)/lib/libc.a
MUSL_LDSO = $(MUSL_DIR)/lib/libc.so
MUSL_CRT1 = $(MUSL_DIR)/lib/crt1.o
MUSL_SCRT1 = $(MUSL_DIR)/lib/Scrt1.o
LDSO_BIN = ld-musl-x86_64.so.1
LUA_BIN = lua/lua/lua
LUA_DIR = lua/lua
LUA_SETUP = lua/setup.sh
CPYTHON_BIN = cpython/cpython/python
CPYTHON_DIR = cpython/cpython
CPYTHON_SETUP = cpython/setup.sh
CPYTHON_SETUP_LOCAL = cpython/Setup.local
DOOM_BIN = doom/doom
DOOM_DIR = doom
DOOM_SETUP = doom/setup.sh
LIBC_ENABLED ?= true
CPYTHON_ENABLED ?= true
DOOM_ENABLED ?= $(shell sh ../tools/toml_get.sh ../src/config.toml doom enabled true)

ifeq ($(CPYTHON_ENABLED),false)
CPYTHON_BIN =
endif

ifeq ($(DOOM_ENABLED),false)
DOOM_BIN =
endif

ifeq ($(LIBC_ENABLED),true)
ALL_TARGETS = $(LUA_BIN) $(CPYTHON_BIN) $(DOOM_BIN) $(LDSO_BIN)
else
ALL_TARGETS =
endif

all: $(ALL_TARGETS)

$(MUSL_LIBC) $(MUSL_LDSO):
	@echo "  MAKE    musl libc"
	@$(MAKE) -C $(MUSL_DIR)

$(MUSL_CRT1) $(MUSL_SCRT1): $(MUSL_LIBC)
	@true

$(LDSO_BIN): $(MUSL_LDSO)
	@echo "  CP      $@"
	@cp $(MUSL_LDSO) $@

lua: $(LUA_BIN)

$(LUA_BIN): $(MUSL_SCRT1) $(MUSL_LDSO)
	@echo "  SETUP   ports/lua"
	@$(SHELL) $(LUA_SETUP)
	@echo "  MAKE    ports/lua"
	@$(MAKE) -C $(LUA_DIR)

.PHONY: cpython
cpython: $(CPYTHON_BIN)

ifneq ($(CPYTHON_BIN),)
$(CPYTHON_BIN): $(MUSL_SCRT1) $(MUSL_LDSO) $(CPYTHON_SETUP) \
    $(CPYTHON_SETUP_LOCAL)
	@echo "  SETUP   ports/cpython"
	@$(SHELL) $(CPYTHON_SETUP)
	@echo "  MAKE    ports/cpython"
	@$(MAKE) -C $(CPYTHON_DIR)
endif

.PHONY: doom
doom: $(DOOM_BIN)

ifneq ($(DOOM_BIN),)
$(DOOM_BIN): $(MUSL_SCRT1) $(MUSL_LDSO) $(DOOM_SETUP)
	@echo "  SETUP   ports/doom"
	@$(SHELL) $(DOOM_SETUP)
	@echo "  MAKE    ports/doom"
	@$(MAKE) -C $(DOOM_DIR)
endif

clean:
	rm -f $(LDSO_BIN)
	$(MAKE) -C $(LUA_DIR) clean
	-$(MAKE) -C $(CPYTHON_DIR) clean 2>/dev/null || true
	-$(MAKE) -C $(DOOM_DIR) clean 2>/dev/null || true

.PHONY: all clean lua cpython doom
