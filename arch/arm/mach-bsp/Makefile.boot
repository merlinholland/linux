ifeq ($(CONFIG_ARCH_BSP_AMP), y)
zreladdr-$(CONFIG_ARCH_BSP)      := $(CONFIG_AMP_ZRELADDR)
else
zreladdr-$(CONFIG_ARCH_BSP)      := $(CONFIG_BSP_ZRELADDR)
endif
params_phys-$(CONFIG_ARCH_BSP)   := $(CONFIG_BSP_PARAMS_PHYS)
initrd_phys-$(CONFIG_ARCH_BSP)   := $(CONFIG_BSP_INITRD_PHYS)
