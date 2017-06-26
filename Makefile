#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of object file images to be generated ()
#   GEN_BINS - list of binaries to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
TARGET = eagle
#FLAVOR = release
FLAVOR = debug

#EXTRA_CCFLAGS += -u

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SPECIAL_MKTARGETS=$(APP_MKTARGETS)
SUBDIRS=    \
	user    \
	platforms/alink-embed    \
	driver

endif # } PDIR

LDDIR = $(SDK_PATH)/ld

CCFLAGS += -Os

# CCFLAGS += -D SAMPLE_JSON_DEBUG


# CCFLAGS += -D CONFIG_ALINK_PASSTHROUGH
CCFLAGS += -D CONFIG_WIFI_WAIT_TIME=60
CCFLAGS += -D CONFIG_ALINK_TASK_PRIOTY=6


CCFLAGS += -D CONFIG_INFO_STORE_MANAGER_ADDR=0x1f8000
CCFLAGS += -D CONFIG_INFO_STORE_KEY_LEN=16


# config LOG_ALINK_LEVEL
# default 0 if LOG_ALINK_LEVEL_NONE
# default 1 if LOG_ALINK_LEVEL_FATAL
# default 2 if LOG_ALINK_LEVEL_ERROR
# default 3 if LOG_ALINK_LEVEL_WARN
# default 4 if LOG_ALINK_LEVEL_INFO
# default 5 if LOG_ALINK_LEVEL_DEBUG
# default 6 if LOG_ALINK_LEVEL_VERBOSE
CCFLAGS += -D CONFIG_LOG_ALINK_LEVEL=4
CCFLAGS += -D CONFIG_ALINK_SDK_LOG_LEVEL=4


# If you have insufficient memory, you can modify the following configuration
CCFLAGS += -D CONFIG_DOWN_CMD_QUEUE_NUM=1
CCFLAGS += -D CONFIG_UP_CMD_QUEUE_NUM=1
CCFLAGS += -D CONFIG_EVENT_HANDLER_CB_STACK=256
# SSL at least 3kB stack space in order to ensure that does not overflow,
# when the stack space is less than 3k, high frequency transceiver data will occur when the stack overflow
CCFLAGS += -D CONFIG_ALINK_POST_DATA_STACK=768
# ALINK single packet data length of up to 512 bytes
CCFLAGS += -D CONFIG_ALINK_DATA_LEN=512

# The following configuration is the largest for the free task
# CCFLAGS += -D CONFIG_DOWN_CMD_QUEUE_NUM=1
# CCFLAGS += -D CONFIG_UP_CMD_QUEUE_NUM=1
# CCFLAGS += -D CONFIG_EVENT_HANDLER_CB_STACK=256
# CCFLAGS += -D CONFIG_ALINK_POST_DATA_STACK=384
# CCFLAGS += -D CONFIG_ALINK_DATA_LEN=256


TARGET_LDFLAGS =		\
	-nostdlib		\
	-Wl,-EL \
	--longcalls \
	--text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -g -O0 -lssl
endif

COMPONENTS_eagle.app.v6 = \
	user/libuser.a    \
	platforms/alink-embed/libesp8266-alink-embed.a    \
	driver/libdriver.a

EXTRA_LIBS := -L$(PDIR)platforms/alink-embed/lib -lalink_agent -ltfspal -lupgrade

LINKFLAGS_eagle.app.v6 =    \
    -L$(SDK_PATH)/lib       \
    -Wl,--gc-sections       \
    -nostdlib               \
    -T$(LD_FILE)            \
    -Wl,--no-check-sections \
    -u call_user_start      \
    -Wl,-static             \
    -Wl,--start-group       \
    -lcirom                 \
    -lgcc                   \
    -lhal                   \
    -lcrypto                \
    -lfreertos              \
    -llwip                  \
    -lmain                  \
    -lnet80211              \
    -lphy                   \
    -lpp                    \
    -lmbedtls               \
    -lopenssl               \
    -lwpa                   \
    -ljson                  \
    -ldriver                \
    -lspiffs                \
    -lnopoll                \
    $(EXTRA_LIBS)           \
    $(DEP_LIBS_eagle.app.v6)\
    -Wl,--end-group

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES =		\

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES =	-DICACHE_FLASH

DEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)

DDEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)


#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I $(PDIR)platforms/alink-embed/include
sinclude $(SDK_PATH)/Makefile

.PHONY: FORCE
FORCE:

