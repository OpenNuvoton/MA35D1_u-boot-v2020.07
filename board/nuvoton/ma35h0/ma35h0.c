// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Nuvoton Technology Corporation.
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <wdt.h>
#include <env.h>

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
