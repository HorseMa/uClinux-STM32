# Put definitions for platforms and boards in here.
# Microblaze uses PLATFORM, not BOARD

# old targets (mbvanilla, suzaku) must not define PLATFORM
ifdef CONFIG_MBVANILLA
endif

ifdef CONFIG_SUZAKU
PLATFORM := suzaku
endif

ifdef CONFIG_ML401
PLATFORM := ml401
endif

# new targets must define platform
ifdef CONFIG_UCLINUX_AUTO
PLATFORM := uclinux-auto
endif

export PLATFORM
