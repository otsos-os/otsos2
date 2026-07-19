TOML_GET = sh ../tools/toml_get.sh
CONFIG_KSHELL := $(shell $(TOML_GET) config.toml kshell enabled true)
CONFIG_KSHELL_CMD_HELP      := $(shell $(TOML_GET) config.toml kshell.commands.help enabled true)
CONFIG_KSHELL_CMD_CLEAR     := $(shell $(TOML_GET) config.toml kshell.commands.clear enabled true)
CONFIG_KSHELL_CMD_ECHO      := $(shell $(TOML_GET) config.toml kshell.commands.echo enabled true)
CONFIG_KSHELL_CMD_DRM_SWITCH := $(shell $(TOML_GET) config.toml kshell.commands.drm_switch enabled true)
CONFIG_KSHELL_CMD_EXIT      := $(shell $(TOML_GET) config.toml kshell.commands.exit enabled true)

ifeq ($(CONFIG_KSHELL),true)

KSHELL_DEFINES = -DCONFIG_KSHELL=1

ifeq ($(CONFIG_KSHELL_CMD_HELP),true)
KSHELL_DEFINES += -DCONFIG_KSHELL_CMD_HELP=1
endif
ifeq ($(CONFIG_KSHELL_CMD_CLEAR),true)
KSHELL_DEFINES += -DCONFIG_KSHELL_CMD_CLEAR=1
endif
ifeq ($(CONFIG_KSHELL_CMD_ECHO),true)
KSHELL_DEFINES += -DCONFIG_KSHELL_CMD_ECHO=1
endif
ifeq ($(CONFIG_KSHELL_CMD_DRM_SWITCH),true)
KSHELL_DEFINES += -DCONFIG_KSHELL_CMD_DRM_SWITCH=1
endif
ifeq ($(CONFIG_KSHELL_CMD_EXIT),true)
KSHELL_DEFINES += -DCONFIG_KSHELL_CMD_EXIT=1
endif

KSHELL_OBJ = ../bin/kshell.o ../bin/kshell_parser.o

ifeq ($(CONFIG_KSHELL_CMD_ECHO),true)
KSHELL_OBJ += ../bin/kshell_echo.o
endif
ifeq ($(CONFIG_KSHELL_CMD_DRM_SWITCH),true)
KSHELL_OBJ += ../bin/kshell_drm.o
endif

../bin/kshell.o: kernel/kshell/kshell.c kernel/kshell/kshell.h \
    kernel/drivers/console/kms_console.h kernel/cm/cm.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/kshell_parser.o: kernel/kshell/parser.c kernel/kshell/kshell.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/kshell_echo.o: kernel/kshell/commands/echo.c kernel/kshell/kshell.h \
    kernel/process.h kernel/api/api.h kernel/drivers/eventtimer.h \
    kernel/drivers/timer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/kshell_drm.o: kernel/kshell/commands/drm.c kernel/kshell/kshell.h \
    kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

else

KSHELL_OBJ = ../bin/kshell_stub.o
KSHELL_DEFINES =

../bin/kshell_stub.o: kernel/kshell/kshell_stub.c kernel/kshell/kshell.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

endif
