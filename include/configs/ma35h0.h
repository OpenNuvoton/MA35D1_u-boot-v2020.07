/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Configuration for Nuvoton MA35H0
 *
 * Copyright (C) 2020 Nuvoton Technology Corporation
 */

#ifndef __MA35H0_H
#define __MA35H0_H

#include <linux/sizes.h>

#define CONFIG_CPU_ARMV8

#define CONFIG_SYS_LOAD_ADDR			0x81000000

#define CONFIG_SYS_MALLOC_LEN			SZ_2M
#define CONFIG_SYS_BOOTM_LEN			SZ_64M

/* Uboot definition */
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_TEXT_BASE + \
						SZ_2M - \
						GENERATED_GBL_DATA_SIZE)


#define CONFIG_SYS_MMC_ENV_DEV 0

#define CONFIG_ETHPRIME			"eth0"

/*
 * NAND flash configuration
 */
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_MAX_NAND_DEVICE	1

#define CONFIG_DW_ALTDESCRIPTOR

/* LCD */
#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define STDOUT_CFG "vidconsole,serial"
#endif

#define MMCARGS \
	"mmcboot=" \
	"if mmc dev 0; then " \
		"echo \"Booting form mmc ... ...\"; " \
		"setenv bootargs root=/dev/${mmc_block} rootfstype=ext4 rw rootwait console=ttyS0,115200n8 rdinit=/sbin/init mem=${kernelmem}; " \
		"mmc read ${kernel_addr_r} 0x1800 0xf000; " \
		"mmc read ${fdt_addr_r} 0x1600 0x80; " \
		"booti ${kernel_addr_r} - ${fdt_addr_r}; " \
	"fi;\0"

#define BOOTENV_DEV_LEGACY_MMC(devtypeu, devtypel, instance) \
        "bootcmd_" #devtypel #instance "=" \
        "run mmcboot\0"

#define BOOTENV_DEV_NAME_LEGACY_MMC(devtypeu, devtypel, instance) \
        #devtypel #instance " "

#define NANDARGS \
        "nandboot=" \
	"if test ${mtdids} = 'nand0=nand0' ; then " \
		"echo \"Booting from nand ... ...\"; " \
		"setenv bootargs noinitrd ubi.mtd=${nand_ubiblock} root=ubi0:rootfs rootfstype=ubifs rw rootwait=1 console=ttyS0,115200n8 rdinit=/sbin/init mem=${kernelmem}; " \
		"nand read ${kernel_addr_r} kernel; " \
		"nand read ${fdt_addr_r} device-tree; " \
		"booti ${kernel_addr_r} - ${fdt_addr_r} ; " \
	"fi\0"
#define BOOTENV_DEV_NAND(devtypeu, devtypel, instance) \
	"bootcmd_" #devtypel #instance "=" \
	"run nandboot\0"

#define BOOTENV_DEV_NAME_NAND(devtypeu, devtypel, instance) \
	#devtypel #instance " "

#define SPINANDARGS \
	"spinandboot=" \
	"if test ${mtdids} = 'spi-nand0=spi-nand0' ; then " \
		"echo \"Booting from spinand ... ...\"; " \
		"mtd list; " \
		"setenv bootargs noinitrd ubi.mtd=${spinand_ubiblock} root=ubi0:rootfs rootfstype=ubifs rw rootwait=1 console=ttyS0,115200n8 rdinit=/sbin/init mem=${kernelmem}; " \
		"mtd read kernel ${kernel_addr_r}; " \
		"mtd read device-tree ${fdt_addr_r}; " \
		"booti ${kernel_addr_r} - ${fdt_addr_r}; " \
	"fi\0"
#define BOOTENV_DEV_SPINAND(devtypeu, devtypel, instance) \
	"bootcmd_" #devtypel #instance "=" \
	"run spinandboot\0"

#define BOOTENV_DEV_NAME_SPINAND(devtypeu, devtypel, instance) \
	#devtypel #instance " "


#define SPINORARGS \
        "spinorboot=" \
                "echo \"Booting from spinor ... ...\"; " \
                "setenv bootargs noinitrd ${spinor_bootargs} rw rootwait=1 console=ttyS0,115200n8 rdinit=/sbin/init mem=${kernelmem}; " \
                "run spinor_boot; " \
                "booti ${kernel_addr_r} - ${fdt_addr_r};\0"

#define BOOTENV_DEV_SPINOR(devtypeu, devtypel, instance) \
        "bootcmd_" #devtypel #instance "=" \
        "run spinorboot\0"

#define BOOTENV_DEV_NAME_SPINOR(devtypeu, devtypel, instance) \
        #devtypel #instance " "


#ifdef CONFIG_CMD_MMC
#define BOOT_TARGET_MMC0(func)  func(MMC, mmc, 0)
#define BOOT_TARGET_MMC1(func)  func(MMC, mmc, 1)
#define BOOT_TARGET_LEGACY_MMC(func)  func(LEGACY_MMC, legacy_mmc, 0)
#else
#define BOOT_TARGET_MMC0(func)
#define BOOT_TARGET_MMC1(func)
#define BOOT_TARGET_LEGACY_MMC(func)
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_TARGET_UBIFS(func) func(UBIFS, ubifs, 0)
#else
#define BOOT_TARGET_UBIFS(func)
#endif

#ifdef CONFIG_MTD_RAW_NAND
#define BOOT_TARGET_NAND(func) func(NAND, nand, 0)
#else
#define BOOT_TARGET_NAND(func)
#endif

#ifdef CONFIG_CMD_MTD
#define BOOT_TARGET_SPINAND(func) func(SPINAND, mtd, 0)
#else
#define BOOT_TARGET_SPINAND(func)
#endif

#ifdef CONFIG_SPI_FLASH_MTD
#define BOOT_TARGET_SPINOR(func) func(SPINOR, spinor, 0)
#else
#define BOOT_TARGET_SPINOR(func)
#endif


#define BOOT_TARGET_DEVICES(func) \
		BOOT_TARGET_MMC0(func) \
		BOOT_TARGET_MMC1(func) \
		BOOT_TARGET_UBIFS(func) \
		BOOT_TARGET_LEGACY_MMC(func) \
		BOOT_TARGET_NAND(func) \
		BOOT_TARGET_SPINAND(func) \
		BOOT_TARGET_SPINOR(func)

#include <config_distro_bootcmd.h>

/* Extra environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"bootfile=Image\0" \
	"kernel_addr_r=0x80080000\0" \
	"fdt_addr_r=0x85000000\0" \
	"bmp_addr_r=0x8D200000\0" \
	"loadaddr=0x81000000\0" \
	"scriptaddr=0x85010000\0" \
	"pxefile_addr_r=0x85020000\0" \
	"ramdisk_addr_r=0x85030000\0" \
	"bootubipart=rootfs\0" \
	"bootubivol=rootfs\0" \
	"kernelmem=256M\0" \
	"mmc_block=mmcblk1p1\0" \
	"spinand_ubiblock=9\0" \
        "nand_ubiblock=4\0" \
	"spinor_bootargs=root=/dev/mtdblock2 rootfstype=jffs2\0" \
	"spinor_boot=sf probe 0 18000000; sf read ${kernel_addr_r} kernel; sf read ${fdt_addr_r} device-tree;\0" \
	MMCARGS \
	NANDARGS \
	SPINANDARGS \
	SPINORARGS \
	BOOTENV

#endif
