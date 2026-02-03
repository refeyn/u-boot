// SPDX-License-Identifier: GPL-2.0-only
/*
 * Board specific initialization for Aquila AM69 SoM
 *
 * Copyright 2024 Toradex - https://www.toradex.com/
 */

#include <asm/arch/k3-common-fdt.h>
#include <asm/arch/hardware.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm.h>
#include <env.h>
#include <fdt_support.h>
#include <i2c.h>
#include <spl.h>

#include "../../toradex/common/tdx-common.h"
#include "../../toradex/aquila-am69/aquila_ddrs_16GB.h"
#include "../../toradex/aquila-am69/aquila_ddrs_8GB.h"
#include "../../toradex/aquila-am69/ddrs_patch.h"
#include "../common/refeyn-common.h"

#define CTRL_MMR_CFG0_MCU_ADC1_CTRL	0x40F040B4
#define CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL	0x40F08010
#define MCU_CLKOUT0_CTRL_CLK_25_MHZ	BIT(0)
#define MCU_CLKOUT0_CTRL_CLK_EN		BIT(4)

#define HW_CFG_MEM_SZ_32GB		0x00
#define HW_CFG_MEM_SZ_16GB		0x01
#define HW_CFG_MEM_SZ_8GB		0x02

#define HW_CFG_MEM_SZ_MASK		0x03

DECLARE_GLOBAL_DATA_PTR;
static u8 hw_cfg;

static u64 aquila_am69_memory_size(void)
{
	switch (hw_cfg & HW_CFG_MEM_SZ_MASK) {
	case HW_CFG_MEM_SZ_32GB:
		return SZ_32G;
	case HW_CFG_MEM_SZ_16GB:
		return SZ_16G;
	case HW_CFG_MEM_SZ_8GB:
		return SZ_8G;
	default:
		puts("Invalid memory size configuration\n");
		return -EINVAL;
	}
}

static void read_hw_cfg(void)
{
	struct gpio_desc gpio_hw_cfg;
	char gpio_name[20];
	int i;

	printf("HW CFG: ");
	for (i = 0; i < 5; i++) {
		sprintf(gpio_name, "gpio@42110000_%d", 82 + i);
		if (dm_gpio_lookup_name(gpio_name, &gpio_hw_cfg) < 0) {
			printf("Lookup named gpio error\n");
			return;
		}

		if (dm_gpio_request(&gpio_hw_cfg, "hw_cfg")) {
			printf("gpio request error\n");
			return;
		}

		if (dm_gpio_get_value(&gpio_hw_cfg) == 1)
			hw_cfg |= BIT(i);

		dm_gpio_free(NULL, &gpio_hw_cfg);
	}
	printf("0x%02x\n", hw_cfg);
}

static void update_ddr_timings(void)
{
	int ret = 0;
	void *fdt = (void *)gd->fdt_blob;

	switch (aquila_am69_memory_size()) {
	case SZ_8G:
		ret = aquila_am69_fdt_apply_ddr_patch(fdt, aquila_am69_ddrss_patch_8GB,
						 MULTI_DDR_CFG_INTRLV_SIZE_8GB);
		break;
	case SZ_16G:
		ret = aquila_am69_fdt_apply_ddr_patch(fdt, aquila_am69_ddrss_patch_16GB,
						 MULTI_DDR_CFG_INTRLV_SIZE_16GB);
		break;
	}

	if (ret)
		printf("Applying DDR patch error: %d\n", ret);
}

static int aquila_am69_fdt_fixup_memory_size(u64 total_sz)
{
	void *blob = (void *)gd->fdt_blob;

	u64 s[CONFIG_NR_DRAM_BANKS] = {
		CFG_SYS_SDRAM_BASE,
		CFG_SYS_SDRAM_BASE1
	};

	u64 e[CONFIG_NR_DRAM_BANKS] = {
		SZ_2G,
		total_sz - SZ_2G
	};

	return fdt_fixup_memory_banks(blob, s, e, CONFIG_NR_DRAM_BANKS);
}

void do_board_detect(void)
{
	/* MCU_ADC1 pins used as General Purpose Inputs */
	writel(readl(CTRL_MMR_CFG0_MCU_ADC1_CTRL) | BIT(16),
	       CTRL_MMR_CFG0_MCU_ADC1_CTRL);

	read_hw_cfg();

	if (IS_ENABLED(CONFIG_K3_DDRSS))
		update_ddr_timings();
}

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
	s32 ret;

	ret = fdtdec_setup_mem_size_base_lowest();
	if (ret)
		printf("Error setting up mem size and base. %d\n", ret);

	return ret;
}

int dram_init_banksize(void)
{
	s32 ret;

	ret = aquila_am69_fdt_fixup_memory_size(aquila_am69_memory_size());
	if (ret)
		printf("Error setting memory size. %d\n", ret);

	ret = fdtdec_setup_memory_banksize();
	if (ret)
		printf("Error setting up memory banksize. %d\n", ret);

	return ret;
}

phys_size_t board_get_usable_ram_top(phys_size_t total_size)
{
#ifdef CONFIG_PHYS_64BIT
	/* Limit RAM used by U-Boot to the DDR low region */
	if (gd->ram_top > 0x100000000)
		return 0x100000000;
#endif

	return gd->ram_top;
}

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;

	ret = fdt_fixup_msmc_ram_k3(blob);
	if (ret)
		return ret;

	ret = refeyn_ft_board_setup(blob, bd);
	if (ret) {
		return ret;
	}

	return ft_common_board_setup(blob, bd);
}
#endif

void spl_board_init(void)
{
	struct udevice *dev;
	int ret;

	if (IS_ENABLED(CONFIG_ESM_K3)) {
		ret = uclass_get_device_by_name(UCLASS_MISC, "esm@700000",
						&dev);
		if (ret)
			printf("MISC init for esm@700000 failed: %d\n", ret);

		ret = uclass_get_device_by_name(UCLASS_MISC, "esm@40800000",
						&dev);
		if (ret)
			printf("MISC init for esm@40800000 failed: %d\n", ret);

		ret = uclass_get_device_by_name(UCLASS_MISC, "esm@42080000",
						&dev);
		if (ret)
			printf("MISC init for esm@42080000 failed: %d\n", ret);
	}

	if (IS_ENABLED(CONFIG_ESM_PMIC)) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_DRIVER_GET(pmic_esm),
						  &dev);
		if (ret)
			printf("ESM PMIC init failed: %d\n", ret);
	}

	if (IS_ENABLED(CONFIG_TARGET_AQUILA_AM69_A72_REFEYN)) {
		writel(readl(CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL) |
		       MCU_CLKOUT0_CTRL_CLK_EN | MCU_CLKOUT0_CTRL_CLK_25_MHZ,
		       CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL);
	} else {
		refeyn_setup_early();
	}
}

#define PMIC_I2C_ADDRESS 0x48

/*
 * Detect PCB revision based on the PMIC configuration in the NVM.
 * If buck5 voltage is 0.85V we are running on a PCB v1.0
 */
static void detect_board_variant(void)
{
	struct udevice *dev;
	uint8_t data;
	int err;

	env_set("variant", "");

	err = i2c_get_chip_for_busnum(0, PMIC_I2C_ADDRESS, 1, &dev);
	if (err) {
		printf("%s: Cannot find PMIC I2C chip\n", __func__);
		return;
	}

	/* BUCK5_VOUT_1 Register (Offset = 16h) */
	err = dm_i2c_read(dev, 0x16, &data, 1);
	if (err) {
		printf("%s: Cannot read from PMIC I2C chip\n", __func__);
		return;
	}

	/* BUCK5_VSET[01] == 0x41 -> 0.85V */
	if (data == 0x41)
		env_set("variant", "-v1.0");
}

int board_late_init(void)
{
	detect_board_variant();

	return 0;
}
