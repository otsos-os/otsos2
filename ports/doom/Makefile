TARGET ?= x86_64-elf
CC = clang
LD = ld.lld
SIZE = llvm-size

NATIVE_DIR = ../../libc/native
LIBSRAPI_DIR = ../../libc/LibSRAPI
LIBG_DIR = ../../libc/LibG
LIBELF_DIR = ../../libc/LibELF
LIBEMIT_DIR = ../../libc/LibEmit
LIBAS_DIR = ../../progs/toolchain/as
SPROT_DIR = ../../progs/GUI/sprot
SRC_DIR = doomgeneric/doomgeneric

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS = $(TARGET_CFLAGS) -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
         -fno-builtin -O2 -Wall -Wextra -fPIE -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE \
         -DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=400 \
         -Iinclude -I$(NATIVE_DIR)/include -I$(LIBSRAPI_DIR)/include -I$(LIBG_DIR)/include \
         -I$(SPROT_DIR)/include -I$(SRC_DIR)

LDFLAGS = -m elf_x86_64 -pie --oformat=elf64-x86-64 -nostdlib \
          -dynamic-linker /lib/drld --export-dynamic

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
LIBSPROT = $(SPROT_DIR)/lib/libsprot.a
LIBSRAPI = $(LIBSRAPI_DIR)/lib/libsrapi.a
LIBG = $(LIBG_DIR)/lib/libg.a
LIBAS = $(LIBAS_DIR)/lib/libas.a
LIBELF = $(LIBELF_DIR)/lib/libelf.a
LIBEMIT = $(LIBEMIT_DIR)/lib/libemit.a

LIBS = $(LIBG) $(LIBSPROT) $(LIBSRAPI) $(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO)

OBJDIR = build
OUTPUT = doom

SRC_DOOM = dummy.o am_map.o doomdef.o doomstat.o dstrings.o d_event.o d_items.o \
           d_iwad.o d_loop.o d_main.o d_mode.o d_net.o f_finale.o f_wipe.o \
           g_game.o hu_lib.o hu_stuff.o info.o i_cdmus.o i_endoom.o \
           i_joystick.o i_scale.o i_sound.o i_system.o i_timer.o memio.o \
           m_argv.o m_bbox.o m_cheat.o m_config.o m_controls.o m_fixed.o \
           m_menu.o m_misc.o m_random.o p_ceilng.o p_doors.o p_enemy.o \
           p_floor.o p_inter.o p_lights.o p_map.o p_maputl.o p_mobj.o \
           p_plats.o p_pspr.o p_saveg.o p_setup.o p_sight.o p_spec.o \
           p_switch.o p_telept.o p_tick.o p_user.o r_bsp.o r_data.o \
           r_draw.o r_main.o r_plane.o r_segs.o r_sky.o r_things.o \
           sha1.o sounds.o statdump.o st_lib.o st_stuff.o s_sound.o \
           tables.o v_video.o wi_stuff.o w_checksum.o w_file.o w_main.o \
           w_wad.o z_zone.o w_file_stdc.o i_input.o i_video.o doomgeneric.o

OBJS = $(addprefix $(OBJDIR)/, $(SRC_DOOM)) $(OBJDIR)/glue.o

all: $(OUTPUT)

$(OUTPUT): $(OBJS) $(NATIVE_CRT0) $(LIBSPROT) $(LIBSRAPI) $(LIBG) $(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJS) $(LIBS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRC_DIR)/%.c | $(OBJDIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/glue.o: glue.c | $(OBJDIR)
	@echo "  CC      $< (glue)"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(OUTPUT)

.PHONY: all clean
