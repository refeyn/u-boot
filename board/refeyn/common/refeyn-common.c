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

	ret = uclass_get_device_by_name(UCLASS_I2C, "i2c@5a840000", &ibus);
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

static int config_gpio_nums[] = {14, 13, 12, 10, 11, 17};

static int read_carrier_switches(u32* carrier_switches) {
	struct gpio_desc desc[6];
	char gpio_name[10];
	int ret;

	for (int i = 0; i < 6; ++i) {
		sprintf(gpio_name, "GPIO6_%d", config_gpio_nums[i]);
		ret = dm_gpio_lookup_name(gpio_name, &desc[i]);
		if (ret) {
			printf("%s lookup %s failed ret = %d\n", __func__, gpio_name, ret);
			return 1;
		}

		sprintf(gpio_name, "switch_%d", i);
		ret = dm_gpio_request(&desc[i], gpio_name);
		if (ret) {
			printf("%s request %s failed ret = %d\n", __func__, gpio_name, ret);
			return 1;
		}

		dm_gpio_set_dir_flags(&desc[i], GPIOD_IS_IN);
	}
	ret = dm_gpio_get_values_as_int(desc, 6);
	if (ret < 0) {
		return 1;
	}
	*carrier_switches = ret;
	return 0;
}

static void lowercaseify(char* s) {
	for(; *s; s++) *s = tolower(*s);
}

static int read_eeprom_data(char* ident, char* serial, char* i2c_bus) {
	struct udevice *idev, *ibus;
	char buf[0x30];
	int ret;

	ret = uclass_get_device_by_name(UCLASS_I2C, i2c_bus, &ibus);
	if (ret) {
		printf("%s bus get failed!\n", i2c_bus);
		return ret;
	}

	ret = dm_i2c_probe(ibus, 0x50, 0, &idev);
	if (ret) {
		printf("%s EEPROM probe failed!\n", i2c_bus);
		return ret;
	}

	for (int chip_addr_len = 2; chip_addr_len > 0; --chip_addr_len) {
		ret = i2c_set_chip_offset_len(idev, chip_addr_len);
		if (ret) {
			printf("%s EEPROM offset len failed!\n", i2c_bus);
			return ret;
		}

		ret = dm_i2c_read(idev, 0, buf, 0x30);
		if (ret) {
			printf("%s EEPROM read failed!\n", i2c_bus);
			return ret;
		}

		if (buf[0] != 0xab) {
			printf("Invalid %s EEPROM mmap %d with chip offset len %d\n", i2c_bus, buf[0], chip_addr_len);
			continue;
		}
		break;
	}
	if (buf[0] != 0xab) {
		return -EILSEQ;
	}

	strncpy(ident, &buf[0x10], 8);
	strncpy(&ident[strlen(ident)], &buf[0x18], 8);
	snprintf(&ident[strlen(ident)], 4, "-%02d", buf[0x1]);
	strncpy(serial, &buf[0x20], 16);
	lowercaseify(ident);
	return 0;
}

static u32 carrier_switches = 0;
static char carrier_ident[32] = {0};
static char carrier_serial[32] = {0};
static char aux_ident[32] = {0};
static char aux_serial[32] = {0};

int refeyn_setup_carrier(void) {
	int ret;

	ret = read_carrier_switches(&carrier_switches);
	if (ret) {
		return ret;
	}
	read_eeprom_data(carrier_ident, carrier_serial, "i2c@5a820000"); // I2C 1
	ret = read_eeprom_data(aux_ident, aux_serial, "i2c@56246000"); // I2C 6 (LVDS0 I2C0)
	if (ret) {
		read_eeprom_data(aux_ident, aux_serial, "i2c@57246000"); // I2C 8 (LVDS1 I2C0)
	}

	if (strcmp(carrier_ident, "") == 0) {
		strcpy(carrier_ident, "generic-carrier");
		printf("No carrier_board_ident readable\n");
		if (strcmp(aux_ident, "") != 0) {
			strcpy(aux_ident, "generic-aux");
			printf("Aux ident cleared due to carrier\n");
		}
	}
	if (strcmp(aux_ident, "") == 0) {
		strcpy(aux_ident, "generic-aux");
		printf("No aux_board_ident readable\n");
	}

	env_set("carrier_board_ident", carrier_ident);
	env_set("aux_board_ident", aux_ident);

	printf("Carrier switches (1-6): ");
	for (int i = 0; i < 6; ++i) {
		printf("%d", (carrier_switches & (1<<i)) != 0);
	}
	printf("\n");

	printf("Carrier: %s %s\n", carrier_ident, carrier_serial);
	printf("Aux: %s %s\n", aux_ident, aux_serial);

	return 0;
}

int refeyn_setup_early(void) {
	setup_power_button();
	return 0;
}

#if defined(CONFIG_OF_LIBFDT)
int refeyn_ft_board_setup(void *blob, struct bd_info *bd) {
	fdt_setprop(blob, 0, "refeyn,carrier-identifier", carrier_ident, strlen(carrier_ident) + 1);
	fdt_setprop(blob, 0, "refeyn,carrier-serial-number", carrier_serial, strlen(carrier_serial) + 1);
	fdt_setprop(blob, 0, "refeyn,aux-identifier", aux_ident, strlen(aux_ident) + 1);
	fdt_setprop(blob, 0, "refeyn,aux-serial-number", aux_serial, strlen(aux_serial) + 1);
	fdt_setprop(blob, 0, "refeyn,carrier-switches", &carrier_switches, sizeof(carrier_switches));
	return 0;
}
#endif
