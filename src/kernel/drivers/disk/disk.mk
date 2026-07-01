TOML_GET = sh ../tools/toml_get.sh

CONFIG_DISK_PATA := $(shell $(TOML_GET) config.toml disk.pata enabled true)

ifeq ($(CONFIG_DISK_PATA),true)

DISK_DEFINES = -DCONFIG_DISK_PATA=1
DISK_OBJ = ../bin/pata.o

../bin/pata.o: kernel/drivers/disk/pata/pata.c \
    kernel/drivers/disk/pata/pata.h kernel/drivers/disk/disk.h \
    kernel/drivers/fs/chainFS/chainfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

else

DISK_DEFINES =
DISK_OBJ =

endif
