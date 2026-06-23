// SPDX-License-Identifier: GPL-2.0+
/*
 * MA35D1 FDT memory fixup command.
 *
 * Some MA35D1 boot images and device trees may be built with a default
 * 256MB memory configuration. On MA35D1 devices with 512MB DDR, this
 * command detects the chip type through PID register 0x40460000. If the
 * FDT is still configured for 256MB and the PID indicates a 512MB device,
 * the command updates the FDT /memory@80000000/reg property to describe
 * 512MB. It also adds an OP-TEE shared/secure memory reserved-memory node
 * at 0x8f800000 with size 8MB.
 *
 * If the FDT is already configured for 512MB, the command intentionally
 * does nothing. If the PID does not indicate a 512MB model, the command
 * keeps the FDT as 256MB. This avoids relying on fragile U-Boot script
 * logic such as setexpr, itest, and shell quoting.
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <mapmem.h>
#include <asm/io.h>
#include <linux/libfdt.h>

#define MA35D1_PID_REG		0x40460000
#define MA35D1_PID_MASK		0x00ff0000

#define PID_MA35D16AJ87C	0x00160000
#define PID_MA35D16F987C	0x00240000
#define PID_MA35D16FJ87C	0x002C0000
#define PID_MA35D14F984		0x00310000
#define PID_MA35D03FJ64C	0x00850000
#define PID_MA35H04FJ64C	0x00A40000

#define MA35D1_DDR_BASE		0x80000000
#define MA35D1_DDR_256M_SIZE	0x10000000
#define MA35D1_DDR_512M_SIZE	0x20000000

#define OPTEE_SHM_BASE		0x8f800000
#define OPTEE_SHM_SIZE		0x00800000

static int ma35_fixmem_set_u32_prop(void *fdt, int node, const char *name,
				    u32 val)
{
	int ret;

	ret = fdt_setprop_u32(fdt, node, name, val);
	if (ret)
		printf("Failed to set %s: %s\n", name, fdt_strerror(ret));

	return ret;
}

static int ma35_fixmem_set_ranges(void *fdt, int node)
{
	int ret;

	ret = fdt_setprop(fdt, node, "ranges", NULL, 0);
	if (ret)
		printf("Failed to set ranges: %s\n", fdt_strerror(ret));

	return ret;
}

static int ma35_fixmem_set_reg(void *fdt, int node, u32 base, u32 size)
{
	fdt32_t reg[4];
	int ret;

	reg[0] = cpu_to_fdt32(0);
	reg[1] = cpu_to_fdt32(base);
	reg[2] = cpu_to_fdt32(0);
	reg[3] = cpu_to_fdt32(size);

	ret = fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
	if (ret)
		printf("Failed to set reg: %s\n", fdt_strerror(ret));

	return ret;
}

static int ma35_fixmem_get_size(const void *fdt, int node, u32 *size)
{
	const fdt32_t *reg;
	int len;

	reg = fdt_getprop(fdt, node, "reg", &len);
	if (!reg) {
		printf("Failed to read /memory@80000000/reg: %s\n",
		       fdt_strerror(len));
		return len;
	}

	if (len < sizeof(fdt32_t) * 4) {
		printf("Warning: unexpected /memory@80000000/reg length: %d\n",
		       len);
		return -FDT_ERR_BADVALUE;
	}

	*size = fdt32_to_cpu(reg[3]);
	return 0;
}

static int ma35_fixmem_reserved_memory(void *fdt)
{
	int root, node, ret;

	root = fdt_path_offset(fdt, "/");
	if (root < 0)
		return root;

	node = fdt_path_offset(fdt, "/reserved-memory");
	if (node == -FDT_ERR_NOTFOUND) {
		node = fdt_add_subnode(fdt, root, "reserved-memory");
		if (node < 0) {
			printf("Failed to add /reserved-memory: %s\n",
			       fdt_strerror(node));
			return node;
		}
	} else if (node < 0) {
		printf("Failed to find /reserved-memory: %s\n",
		       fdt_strerror(node));
		return node;
	}

	ret = ma35_fixmem_set_u32_prop(fdt, node, "#address-cells", 2);
	if (ret)
		return ret;

	ret = ma35_fixmem_set_u32_prop(fdt, node, "#size-cells", 2);
	if (ret)
		return ret;

	ret = ma35_fixmem_set_ranges(fdt, node);
	if (ret)
		return ret;

	node = fdt_path_offset(fdt, "/reserved-memory");
	if (node < 0) {
		printf("Failed to re-find /reserved-memory: %s\n",
		fdt_strerror(node));
		return node;
	}

	return node;
}

static int ma35_fixmem_add_optee_shm(void *fdt)
{
	int rmem, node, ret;

	rmem = ma35_fixmem_reserved_memory(fdt);
	if (rmem < 0)
		return rmem;

	node = fdt_path_offset(fdt, "/reserved-memory/optee_shm@8f800000");
	if (node == -FDT_ERR_NOTFOUND) {
		node = fdt_add_subnode(fdt, rmem, "optee_shm@8f800000");
		if (node < 0) {
			printf("Failed to add /reserved-memory/optee_shm@8f800000: %s\n",
			       fdt_strerror(node));
			return node;
		}
		printf("Inserted OP-TEE reserved memory node\n");
	} else if (node < 0) {
		printf("Failed to find /reserved-memory/optee_shm@8f800000: %s\n",
		       fdt_strerror(node));
		return node;
	} else {
		printf("Updated OP-TEE reserved memory node\n");
	}

	ret = ma35_fixmem_set_reg(fdt, node, OPTEE_SHM_BASE, OPTEE_SHM_SIZE);
	if (ret)
		return ret;

	ret = fdt_setprop(fdt, node, "no-map", NULL, 0);
	if (ret)
		printf("Failed to set no-map: %s\n", fdt_strerror(ret));

	return ret;
}

static int do_ma35_fixmem(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	u32 pid, mem_size = 0;
	bool pid_is_512m = 0;
	int memory, ret;
	ulong fdt_addr;
	void *fdt;

	if (argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2)
		fdt_addr = simple_strtoul(argv[1], NULL, 16);
	else
		fdt_addr = env_get_hex("fdt_addr_r", 0);

	if (!fdt_addr) {
		printf("No FDT address provided and fdt_addr_r is not set\n");
		return CMD_RET_FAILURE;
	}

	fdt = map_sysmem(fdt_addr, 0);

	if (!fdt || fdt_check_header(fdt)) {
		printf("No valid FDT available\n");
		return CMD_RET_FAILURE;
	}

	ret = fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + 0x2000);
	if (ret) {
		printf("Failed to resize FDT: %s\n", fdt_strerror(ret));
		return CMD_RET_FAILURE;
	}

	memory = fdt_path_offset(fdt, "/memory@80000000");
	if (memory < 0) {
		printf("Failed to find /memory@80000000: %s\n",
		       fdt_strerror(memory));
		return CMD_RET_FAILURE;
	}

	ret = ma35_fixmem_get_size(fdt, memory, &mem_size);
	if (ret)
		return CMD_RET_FAILURE;

	printf("FDT memory node size: %uMB\n", mem_size >> 20);

	pid = readl((void *)MA35D1_PID_REG) & MA35D1_PID_MASK;

	if ((pid == PID_MA35D16AJ87C) || (pid == PID_MA35D16F987C) ||
	    (pid == PID_MA35D16FJ87C) || (pid == PID_MA35D14F984) ||
	    (pid == PID_MA35D03FJ64C) || (pid == PID_MA35H04FJ64C)) {
		pid_is_512m = 1;
	}

	printf("MA35D1 PID register: 0x%08x\n", pid);
	printf("PID indicates 512MB model: %s\n", pid_is_512m ? "yes" : "no");

	if (mem_size == MA35D1_DDR_512M_SIZE) {
		printf("Device tree is already configured for 512MB\n");
		printf("No change needed\n");
		return CMD_RET_SUCCESS;
	}

	if (mem_size != MA35D1_DDR_256M_SIZE) {
		printf("Warning: unexpected FDT memory node size: %uMB\n", mem_size >> 20);
		printf("No change needed\n");
		return CMD_RET_FAILURE;
	}

	if (!pid_is_512m) {
		//printf("No change needed\n");
		return CMD_RET_SUCCESS;
	}

	ret = env_set("kernelmem", "512M");
	if (ret) {
		printf("Failed to set kernelmem=512M\n");
		return CMD_RET_FAILURE;
	}

	ret = ma35_fixmem_set_reg(fdt, memory, MA35D1_DDR_BASE,
				    MA35D1_DDR_512M_SIZE);
	if (ret)
		return CMD_RET_FAILURE;

	printf("Updated FDT memory node to 512MB\n");

	ret = ma35_fixmem_add_optee_shm(fdt);
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	ma35_fixmem, 2, 0, do_ma35_fixmem,
	"fix MA35D1 FDT memory size and OP-TEE shared memory",
	"[fdt_addr]\n"
	"    - inspect /memory@80000000/reg and, when a 256MB FDT is used\n"
	"      on a 512MB MA35D1, update the FDT and OP-TEE shared memory"
);
