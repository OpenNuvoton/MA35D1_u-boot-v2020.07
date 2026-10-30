// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Nuvoton Technology Corporation.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <log.h>
#include <wdt.h>
#include <asm/io.h>

#define MA35_PID_REG		0x40460000
#define MA35_PID_MASK		0x00FF0000U
#define MA35_PID_SHIFT		16U

#define PID_MA35H04FJ64C	0xA4U

#define DRAM_BASE		0x80000000ULL
#define DRAM_SIZE_256M		0x10000000ULL
#define DRAM_SIZE_512M		0x20000000ULL

#define OPTEE_CORE_SIZE		0x00800000ULL

DECLARE_GLOBAL_DATA_PTR;

unsigned long tfa_arg1 __attribute__((section(".data"))) = 0;

static void setup_linux_mtdparts(void)
{
	const char *mtdparts = CONFIG_MTDPARTS_DEFAULT;
	char buf[512];

	if (!*mtdparts)
		return;

	if (!strncmp(mtdparts, "mtdparts=", 9))
		mtdparts += 9;

	if (!strncmp(mtdparts, "spi-nand0:", 10)) {
		snprintf(buf, sizeof(buf), "mtdparts=spi0.0:%s", mtdparts + 10);
		env_set("linux_mtdparts", buf);
	} else if (!strncmp(mtdparts, "nand0:", 6)) {
		snprintf(buf, sizeof(buf), "mtdparts=%s", mtdparts);
		env_set("linux_mtdparts", buf);
	}
}

static int ma35_add_optee_core_node(void *fdt, u64 base)
{
	struct fdt_memory core = {
		.start = base,
		.end = base + OPTEE_CORE_SIZE - 1,
	};
	char path[64];
	int node, ret;

	ret = fdt_increase_size(fdt, 768);
	if (ret < 0)
		return ret;

	ret = fdtdec_add_reserved_memory(fdt, "optee_core", &core, NULL);
	if (ret < 0)
		return ret;

	snprintf(path, sizeof(path), "/reserved-memory/optee_core@%llx", base);
	node = fdt_path_offset(fdt, path);
	if (node < 0)
		return node;

	return fdt_setprop(fdt, node, "no-map", NULL, 0);
}

static u64 ma35_dram_size(void)
{
	u32 pid = (readl((void *)MA35_PID_REG) & MA35_PID_MASK) >> MA35_PID_SHIFT;

	switch (pid) {
	case PID_MA35H04FJ64C:
		return DRAM_SIZE_512M;
	default:
		return DRAM_SIZE_256M;
	}
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	const char *sw = env_get("optee_rmem");
	u64 mem_start = DRAM_BASE;
	u64 mem_size = ma35_dram_size();
	int ret;

	if (!sw || strcmp(sw, "1"))
		return 0;

	ret = ma35_add_optee_core_node(blob, mem_start + mem_size - OPTEE_CORE_SIZE);
	if (ret)
		printf("Failed to add optee_core: %s\n", fdt_strerror(ret));

	ret = fdt_fixup_memory_banks(blob, &mem_start, &mem_size, 1);
	if (ret)
		printf("Failed to fixup memory: %s\n", fdt_strerror(ret));

	return 0;
}

int board_late_init(void)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%ld", tfa_arg1);
	env_set("kernel_ver", buf);

	setup_linux_mtdparts();

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = gd->ram_base + 0x100;

	debug("gd->fdt_blob is %p\n", gd->fdt_blob);
	return 0;
}
