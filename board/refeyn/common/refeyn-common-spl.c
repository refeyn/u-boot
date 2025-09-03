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
	setup_power_button();
	return 0;
}

