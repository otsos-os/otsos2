TOML_GET = sh ../tools/toml_get.sh

CONFIG_AUDIO_HDA := $(shell $(TOML_GET) config.toml audio.hda enabled true)

ifeq ($(CONFIG_AUDIO_HDA),true)

AUDIO_DEFINES = -DCONFIG_AUDIO_HDA=1

AUDIO_OBJ = ../bin/audio_kstypes.o \
	../bin/audio_ksobj.o \
	../bin/audio_ksstream.o \
	../bin/audio_mixgain.o \
	../bin/audio_mixcore.o \
	../bin/audio_pcminiport.o \
	../bin/audio_pcport.o \
	../bin/audio_hdabus.o \
	../bin/audio_hdawidget.o \
	../bin/audio_hdacodec.o \
	../bin/audio_sysaudio.o \
	../bin/api_audio.o \
	../bin/audio_core.o \
	../bin/audio_hda.o

../bin/audio_kstypes.o: kernel/audio/ks/kstypes.c kernel/audio/ks/kstypes.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_ksobj.o: kernel/audio/ks/ksobj.c kernel/audio/ks/ksobj.h kernel/audio/ks/kstypes.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_ksstream.o: kernel/audio/ks/ksstream.c kernel/audio/ks/ksstream.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_mixgain.o: kernel/audio/mix/mix_gain.c kernel/audio/mix/mix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_mixcore.o: kernel/audio/mix/mix_core.c kernel/audio/mix/mix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_pcminiport.o: kernel/audio/portcls/pcminiport.c kernel/audio/portcls/pcminiport.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_pcport.o: kernel/audio/portcls/pcport.c kernel/audio/portcls/pcport.h kernel/audio/portcls/pcminiport.h kernel/audio/ks/ksstream.h kernel/audio/mix/mix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_hdabus.o: kernel/audio/hdaudio/hdabus.c kernel/audio/hdaudio/hdabus.h kernel/drivers/audio/intel-hda/hdac_reg.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_hdawidget.o: kernel/audio/hdaudio/hdawidget.c kernel/audio/hdaudio/hdawidget.h kernel/audio/hdaudio/hdabus.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_hdacodec.o: kernel/audio/hdaudio/hdacodec.c kernel/audio/hdaudio/hdacodec.h kernel/audio/hdaudio/hdawidget.h kernel/audio/ks/ksobj.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_sysaudio.o: kernel/audio/sysaudio/sysaudio.c kernel/audio/sysaudio/sysaudio.h kernel/audio/api/api_audio.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/api_audio.o: kernel/audio/api/api_audio.c kernel/audio/api/api_audio.h kernel/audio/portcls/pcport.h kernel/audio/mix/mix.h kernel/entity/entity.h kernel/api/api.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_core.o: kernel/audio/audio.c kernel/audio/audio.h kernel/audio/sysaudio/sysaudio.h kernel/audio/api/api_audio.h kernel/drivers/newbus/newbus.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

../bin/audio_hda.o: kernel/drivers/audio/intel-hda/hda.c kernel/drivers/audio/intel-hda/hda.h kernel/drivers/audio/intel-hda/hdac_reg.h kernel/audio/hdaudio/hdabus.h kernel/audio/hdaudio/hdacodec.h kernel/audio/portcls/pcminiport.h kernel/audio/portcls/pcport.h kernel/audio/mix/mix.h kernel/audio/sysaudio/sysaudio.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

else

AUDIO_DEFINES =
AUDIO_OBJ =

endif
