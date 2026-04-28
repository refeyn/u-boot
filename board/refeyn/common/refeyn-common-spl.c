// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 Refeyn
 */

#include <common.h>
#include <env.h>
#include <i2c.h>
#include <errno.h>
#include <dm/uclass.h>
#include <asm/gpio.h>
#include <ctype.h>
#include <linux/delay.h>

const char* power_control_gpios[] = {"gpio@600000_48", "gpio@600000_22"};
const static int power_control_gpios_size = ARRAY_SIZE(power_control_gpios);

static void power_control_enable(void) {
	// Would be better as a regulator, but we don't know which GPIO to use
	struct gpio_desc descs[power_control_gpios_size];
	int ret;

	for (int i = 0; i < power_control_gpios_size; ++i) {
		ret = dm_gpio_lookup_name(power_control_gpios[i], &descs[i]);
		if (ret) {
			printf("%s lookup %s failed ret = %d\n", __func__, power_control_gpios[i], ret);
			return;
		}

		ret = dm_gpio_request(&descs[i], "POWER_CONTROL");
		if (ret) {
			printf("%s request %s (%d) failed ret = %d\n", __func__, "POWER_CONTROL", i, ret);
			return;
		}

		ret = dm_gpio_set_dir_flags(&descs[i], GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		if (ret) {
			printf("%s set %s (%d) failed ret = %d\n", __func__, "POWER_CONTROL", i, ret);
			return;
		}
	}
}

static void setup_power_button(void) {
	struct udevice *idev, *ibus;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_I2C, "i2c@2050000", &ibus);
	if (ret) {
		printf("\nPower button bus get failed!\n");
		return;
	}

	ret = dm_i2c_probe(ibus, 0x6c, 0, &idev);
	if (ret) {
		printf("\nPower button LED controller probe failed!\n");
		return;
	}

	// Assume the rest succeed
	// ret = dm_i2c_reg_write(idev, 0x23, 0x66); // Chip reset (disabled as it causes an IO error)
	ret = ret || dm_i2c_reg_write(idev, 0x0, 0x1); // Chip on
	ret = ret || dm_i2c_reg_write(idev, 0x1, 0x6); // Set voltage and max current
	ret = ret || dm_i2c_reg_write(idev, 0x4, 0x0); // Manual control
	ret = ret || dm_i2c_reg_write(idev, 0x10, 0x55); // Commit update
	ret = ret || dm_i2c_reg_write(idev, 0x20, 0x07); // LEDs on
	ret = ret || dm_i2c_write(idev, 0x30, "\x7f\x7f\x7f", 3); // Set LED current
	ret = ret || dm_i2c_write(idev, 0x40, "\x7f\x7f\x7f", 3); // Set LED PWM

	if (ret) {
		printf("\nPower button LED write failed!\n");
		return;
	}
}

int refeyn_setup_early(void) {
	power_control_enable();
	mdelay(20);
	setup_power_button();
	return 0;
}

