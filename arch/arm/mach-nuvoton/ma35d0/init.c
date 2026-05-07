// SPDX-License-Identifier: GPL-2.0
/*
 * Configuration for Nuvoton ma35d0
 *
 * Copyright (C) 2020 Nuvoton Technology Corporation.
 */

#include <clk.h>
#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <fdtdec.h>
#include <init.h>
#include <ram.h>
#include <asm/armv8/mmu.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <dm/uclass.h>
#include <asm/io.h>
#include <asm/system.h>

DECLARE_GLOBAL_DATA_PTR;

#define MA35D0_REGION_DDR	0
#define MA35D0_REGION_REG	1

static struct mm_region ma35d0_mem_map[] = {
	{
		.virt = 0x80000000UL,
		.phys = 0x80000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 0x10000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		0
	}
};

struct mm_region *mem_map = ma35d0_mem_map;


int timer_init(void)
{
	unsigned long freq = 12000000;
	unsigned int el = current_el();

	/* Set clock frequency if uboot is executing in EL3 */
	if (el == 3)
		asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");

	gd->arch.tbl = 0;
	gd->arch.tbu = 0;

	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	mem_map[MA35D0_REGION_DDR].virt = gd->ram_base;
	mem_map[MA35D0_REGION_DDR].phys = gd->ram_base;
	mem_map[MA35D0_REGION_DDR].size = gd->ram_size;

	return 0;
}

void reset_cpu(ulong addr)
{
	psci_system_reset();
}

int print_cpuinfo(void)
{
#ifdef CONFIG_DEFAULT_DEVICE_TREE
	const char *dt = CONFIG_DEFAULT_DEVICE_TREE;
	const char *dash = strchr(dt, '-');
	int len = dash ? (int)(dash - dt) : (int)strlen(dt);

	printf("CPU:   Nuvoton %.*s\n", len, dt);
#else
	printf("CPU:   Nuvoton ma35d0\n");
#endif
	return 0;
}
