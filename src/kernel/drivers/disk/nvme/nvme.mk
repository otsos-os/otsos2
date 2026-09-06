TOML_GET = sh ../tools/toml_get.sh

CONFIG_DISK_NVME := $(shell $(TOML_GET) config.toml disk.nvme enabled true)

ifeq ($(CONFIG_DISK_NVME),true)

NVME_DEFINES = -DCONFIG_DISK_NVME=1
NVME_OBJ = ../bin/nvme_ctrl.o ../bin/nvme_queue.o

NVME_DEPS = kernel/drivers/disk/nvme/nvme.h \
    kernel/drivers/disk/nvme/nvmereg.h kernel/drivers/disk/disk.h \
    kernel/drivers/disk/bio.h kernel/mm/dma/dma.h \
    kernel/drivers/newbus/newbus.h kernel/pci/pci.h kernel/sync/sync.h

../bin/nvme_ctrl.o: kernel/drivers/disk/nvme/nvme_ctrl.c $(NVME_DEPS)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/nvme_queue.o: kernel/drivers/disk/nvme/nvme_queue.c $(NVME_DEPS)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

else

NVME_DEFINES =
NVME_OBJ =

endif
