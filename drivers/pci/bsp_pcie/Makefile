
obj-$(CONFIG_BSP_PCIE) += bsp_pcie.o

bsp_pcie-objs	:= pcie.o

ifeq ($(CONFIG_PCI_DEBUG),y)
	        EXTRA_CFLAGS += -DPCIE_DEBUG
endif
