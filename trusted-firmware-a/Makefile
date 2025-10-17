BUILD_DIR := $(shell pwd)

CPU_TYPE ?=
ifeq ($(CHIP), ss928v100)
LINUX_SUFFIX := _$(CPU_TYPE)
endif

MAKE=make
KERNEL_VER ?=linux-4.19.y

CHIP ?= ss928v100

ifneq ($(filter $(CHIP), ss928v100), )
BUILD_CHIP = ss928v100
else ifneq ($(filter $(CHIP), ss927v100), )
BUILD_CHIP = ss927v100
else ifneq ($(filter $(CHIP), ss528v100 ss625v100), )
BUILD_CHIP = ss528v100
else
$(warning "warning: "$(CHIP)" is invalid")
endif

ATF_VER:=trusted-firmware-a-2.2
ATF_TAR:=trusted-firmware-a-2.2.tar.gz

OSDRV_CROSS ?= aarch64-mix210-linux
OSDRV_CROSS_CFLAGS := -fstack-protector-strong -fPIC
OSDRV_CROSS_LDFLAGS := -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

GREEN = "\e[32;1m"
DONE    = "\033[0m"

BOOT_MEDIA ?= spi
ifeq ($(BOOT_MEDIA), spi)
MEDIUM_FLAG = _
endif

ifeq ($(BOOT_MEDIA), nand)
MEDIUM_FLAG = _nand_
endif

ifeq ($(BOOT_MEDIA), emmc)
MEDIUM_FLAG = _emmc_
endif

LIB_TYPE ?=glibc
ARCH_TYPE ?= arm64

PUB_IMAGE ?=$(CHIP)$(MEDIUM_FLAG)image_$(LIB_TYPE)
UIMAGE ?=uImage_$(CHIP)
BUILD ?= part

all:
ifneq ($(BUILD_DIR)/$(ATF_VER), $(wildcard $(BUILD_DIR)/$(ATF_VER)))
	pushd $(BUILD_DIR);tar xzf $(ATF_TAR) -C .;popd
	pushd $(BUILD_DIR)/$(ATF_VER);patch -p1 < ../trusted-firmware-a-2.2.patch;popd
endif
	find $(BUILD_DIR)/$(ATF_VER) | xargs touch
	cd $(BUILD_DIR)/$(ATF_VER) && \
	$(MAKE) clean PLAT=$(BUILD_CHIP) DEBUG=1 CROSS_COMPILE=$(OSDRV_CROSS)-  && \
		$(MAKE) distclean PLAT=$(BUILD_CHIP) DEBUG=1 CROSS_COMPILE=$(OSDRV_CROSS)-  && \
		$(MAKE) PLAT=$(BUILD_CHIP) SPD=none BL33=$(BUILD_DIR)/../arch/arm64/boot/uImage \
		CCI_UP=0 DEBUG=0 BL33_SEC=0 fip CROSS_COMPILE=$(OSDRV_CROSS)-;

ifeq ($(BUILD), all)
	cp $(BUILD_DIR)/$(ATF_VER)/build/$(BUILD_CHIP)/release/fip.bin ../$(UIMAGE)
endif

clean:
	rm $(BUILD_DIR)/$(ATF_VER) -rf;

distclean: clean
