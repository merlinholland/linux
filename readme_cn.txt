    a.进入linux目录，执行以下操作
	    cp arch/arm64/configs/ss928v100_defconfig .config
	    (emmc启动时执行如下操作: cp arch/arm64/configs/ss928v100_emmc_defconfig .config)
	    (并口 nand 启动时执行如下操作：cp arch/arm64/configs/ss928v100_nand_defconfig .config)
	    make ARCH=arm64 CROSS_COMPILE=aarch64-mix210-linux- menuconfig
	    make ARCH=arm64 CROSS_COMPILE=aarch64-mix210-linux- uImage -j 20

	b.进入trusted-firmware-a目录，
        make CHIP=ss928v100 BOOT_MEDIA=emmc LIB_TYPE=glibc ARCH_TYPE=arm64 all
        make all 与上面功能一样，默认EMMC启动
	    在trusted-firmware-a/trusted-firmware-a-2.2/build/ss928v100/release目录下，生成的fip.bin文件就是ATF+kernle的镜像.