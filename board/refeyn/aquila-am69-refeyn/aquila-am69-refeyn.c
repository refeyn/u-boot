// SPDX-License-Identifier: GPL-2.0-only
/*
 * Board specific initialization for Aquila AM69 SoM
 *
 * Copyright 2024 Toradex - https://www.toradex.com/
 */

#include <asm/arch/hardware.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm.h>
#include <env.h>
#include <spl.h>

#include "../../../arch/arm/mach-k3/common_fdt.h"
#include "../../toradex/common/tdx-common.h"

#define CTRL_MMR_CFG0_MCU_ADC1_CTRL	0x40F040B4
#define CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL	0x40F08010
#define MCU_CLKOUT0_CTRL_CLK_EN		BIT(4)

DECLARE_GLOBAL_DATA_PTR;
static u8 hw_cfg;

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
	printf("0x%2x\n", hw_cfg);
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

	/* MCU_ADC1 pins used as General Purpose Inputs */
	writel(readl(CTRL_MMR_CFG0_MCU_ADC1_CTRL) | BIT(16),
	       CTRL_MMR_CFG0_MCU_ADC1_CTRL);

	if (IS_ENABLED(CONFIG_TARGET_AQUILA_AM69_R5_REFEYN))
		writel(readl(CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL) |
		       MCU_CLKOUT0_CTRL_CLK_EN,
		       CTRL_MMR_CFG0_MCU_CLKOUT0_CTRL);

	read_hw_cfg();
}
