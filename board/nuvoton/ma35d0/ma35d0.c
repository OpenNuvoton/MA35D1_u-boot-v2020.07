// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Nuvoton Technology Corporation.
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <wdt.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long tfa_arg1 __attribute__((section(".data"))) = 0;

int board_late_init(void)
{
	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = gd->ram_base + 0x100;

	debug("gd->fdt_blob is %p\n", gd->fdt_blob);
	return 0;
}
