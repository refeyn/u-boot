// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 NXP
 */

#include <common.h>
#include <i2c.h>
#include <cpu_func.h>
#include <env.h>
#include <errno.h>
#include <init.h>
#include <ctype.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/clock.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/arch/snvs_security_sc.h>
#include <usb.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include "../../freescale/common/tcpc.h"
#include "command.h"

DECLARE_GLOBAL_DATA_PTR;

#define ENET_INPUT_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ENET_NORMAL_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))


#define GPIO_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))


#define UART_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static iomux_cfg_t uart1_pads[] = {
	SC_P_UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	SC_P_UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static void setup_iomux_uart(void)
{
	imx8_iomux_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}

int board_early_init_f(void)
{
	int ret;

	/* Set UART1 clock root to 80 MHz */
	ret = sc_pm_setup_uart(SC_R_UART_1, SC_80MHZ);
	if (ret)
		return ret;

	setup_iomux_uart();

	// Power on aux GPIOs
	sc_pm_set_resource_power_mode(-1, SC_R_BOARD_R2, SC_PM_PW_MODE_ON);

	// Power on USDHC2
	sc_pm_set_resource_power_mode(-1, SC_R_BOARD_R3, SC_PM_PW_MODE_ON);

	return 0;
}


#if IS_ENABLED(CONFIG_FEC_MXC)
#include <miiphy.h>

#ifndef CONFIG_DM_ETH
static iomux_cfg_t pad_enet0[] = {
	SC_P_ENET0_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD0 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD1 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD2 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD3 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXC | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD0 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD1 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD2 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD3 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXC | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),

	/* Shared MDIO */
	SC_P_ENET0_MDC | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_MDIO | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx8_iomux_setup_multiple_pads(pad_enet0, ARRAY_SIZE(pad_enet0));
}

int board_eth_init(bd_t *bis)
{
	int ret;
	struct power_domain pd;

	printf("[%s] %d\n", __func__, __LINE__);

	if (!power_domain_lookup_name("conn_enet0", &pd))
		power_domain_on(&pd);

	setup_iomux_fec();

	ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV,
		CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	if (ret)
		printf("FEC1 MXC: %s:failed\n", __func__);

	return ret;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif
#endif

static struct gpio_desc green_led_desc;
static struct gpio_desc red_led_desc;

static void board_gpio_init(void)
{
	int ret;
	struct gpio_desc desc;

	ret = dm_gpio_lookup_name("GPIO0_20", &green_led_desc);
	if (ret) {
		printf("%s lookup GPIO@0_20 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&green_led_desc, "green_led");
	if (ret) {
		printf("%s request green_led failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&green_led_desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = dm_gpio_lookup_name("GPIO0_21", &red_led_desc);
	if (ret) {
		printf("%s lookup GPIO@0_20 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&red_led_desc, "red_led");
	if (ret) {
		printf("%s request red_led failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&red_led_desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = dm_gpio_lookup_name("GPIO4_20", &desc);
	if (ret) {
		printf("%s lookup GPIO@4_20 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&desc, "bb_3v3_1");
	if (ret) {
		printf("%s request bb_3v3_1 failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = dm_gpio_lookup_name("GPIO4_24", &desc);
	if (ret) {
		printf("%s lookup GPIO@4_24 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&desc, "bb_3v3_2");
	if (ret) {
		printf("%s request bb_3v3_2 failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = dm_gpio_lookup_name("GPIO4_23", &desc);
	if (ret) {
		printf("%s lookup GPIO@4_23 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&desc, "bb_3v3_3");
	if (ret) {
		printf("%s request bb_3v3_3 failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
}
int checkboard(void)
{
	print_bootinfo();

	return 0;
}

#ifdef CONFIG_USB

#ifdef CONFIG_USB_TCPC
struct gpio_desc type_sel_desc;

static iomux_cfg_t ss_mux_gpio[] = {
	SC_P_USB_SS3_TC3 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
	SC_P_QSPI1A_SS0_B | MUX_MODE_ALT(3) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
};

struct tcpc_port port;
struct tcpc_port_config port_config = {
	.i2c_bus = 0,
	.addr = 0x51,
	.port_type = TYPEC_PORT_DFP,
};

void ss_mux_select(enum typec_cc_polarity pol)
{
	if (pol == TYPEC_POLARITY_CC1)
		dm_gpio_set_value(&type_sel_desc, 0);
	else
		dm_gpio_set_value(&type_sel_desc, 1);
}

static void setup_typec(void)
{
	int ret;
	struct gpio_desc typec_en_desc;

	imx8_iomux_setup_multiple_pads(ss_mux_gpio, ARRAY_SIZE(ss_mux_gpio));
	ret = dm_gpio_lookup_name("GPIO4_6", &type_sel_desc);
	if (ret) {
		printf("%s lookup GPIO4_6 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&type_sel_desc, "typec_sel");
	if (ret) {
		printf("%s request typec_sel failed ret = %d\n", __func__, ret);
		return;
	}

	dm_gpio_set_dir_flags(&type_sel_desc, GPIOD_IS_OUT);

	ret = dm_gpio_lookup_name("GPIO4_19", &typec_en_desc);
	if (ret) {
		printf("%s lookup GPIO4_19 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&typec_en_desc, "typec_en");
	if (ret) {
		printf("%s request typec_en failed ret = %d\n", __func__, ret);
		return;
	}

	/* Enable SS MUX */
	dm_gpio_set_dir_flags(&typec_en_desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = tcpc_init(&port, port_config, &ss_mux_select);
	if (ret) {
		printf("%s: tcpc init failed, err=%d\n", __func__, ret);
		return;
	}
}
#endif

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_dfp_mode(&port);
#endif
#ifdef CONFIG_USB_CDNS3_GADGET
		} else {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_ufp_mode(&port);
			printf("%d setufp mode %d\n", index, ret);
#endif
#endif
		}
	}

	return ret;

}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_disable_src_vbus(&port);
#endif
		}
	}

	return ret;
}
#endif

int board_init(void)
{
	if (IS_ENABLED(CONFIG_XEN))
		return 0;

	board_gpio_init();


#if defined(CONFIG_USB) && defined(CONFIG_USB_TCPC)
	setup_typec();
#endif

#ifdef CONFIG_IMX_SNVS_SEC_SC_AUTO
	{
		int ret = snvs_security_sc_init();

		if (ret)
			return ret;
	}
#endif

	return 0;
}

void board_quiesce_devices(void)
{
	dm_gpio_set_value(&red_led_desc, 0);

	const char *power_on_devices[] = {
		"dma_lpuart1",
		"PD_UART1_TX",
		"PD_UART1_RX",
	};

	if (IS_ENABLED(CONFIG_XEN)) {
		/* Clear magic number to let xen know uboot is over */
		writel(0x0, (void __iomem *)0x80000000);
		return;
	}

	imx8_power_off_pd_devices(power_on_devices, ARRAY_SIZE(power_on_devices));
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(void)
{
	sc_pm_reboot(-1, SC_PM_RESET_TYPE_COLD);
	while(1);
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}
#endif

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

int read_debug_switches(int* debug_gpios)
{
	struct gpio_desc desc[6];
	char gpio_name[10];
	int ret;

	for (int i = 0; i < 6; ++i) {
		sprintf(gpio_name, "GPIO1_%d", i + 3);
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
	*debug_gpios = ret;
	return 0;
}

void lowercaseify(char* s) {
	for(; *s; s++) *s=tolower(*s);
}

void read_eeprom_data(char* sbc_ident, char* sbc_serial, char* aux_ident, char* aux_serial)
{
	struct udevice *idev, *ibus;
	char buf[0x30];
	int ret;

	// SBC

	ret = uclass_get_device_by_name(UCLASS_I2C, "i2c@5a820000", &ibus);
	if (ret) {
		printf("\nSBC bus get failed!\n");
		goto aux;
	}

	ret = dm_i2c_probe(ibus, 0x50, 0, &idev);
	if (ret) {
		printf("\nSBC EEPROM probe failed!\n");
		goto aux;
	}

	ret = i2c_set_chip_offset_len(idev, 2);
	if (ret) {
		printf("\nSBC EEPROM offset len failed!\n");
		goto aux;
	}

	if (dm_i2c_read(idev, 0, buf, 0x30)) {
		printf("\nSBC EEPROM read failed!\n");
		goto aux;
	}

	if (buf[0] != 0xab) {
		printf("\nInvalid SBC EEPROM mmap %d\n", buf[0]);
		goto aux;
	}

	strncpy(sbc_ident, &buf[0x10], 8);
	strncpy(&sbc_ident[strlen(sbc_ident)], &buf[0x18], 8);
	snprintf(&sbc_ident[strlen(sbc_ident)], 4, "-%02d", buf[0x1]);
	strncpy(sbc_serial, &buf[0x20], 16);
	lowercaseify(sbc_ident);


	// Aux board
aux:
	ret = uclass_get_device_by_name(UCLASS_I2C, "i2c@37230000", &ibus);
	if (ret) {
		printf("\nAux bus get failed!\n");
		return;
	}

	ret = dm_i2c_probe(ibus, 0x50, 0, &idev);
	if (ret) {
		printf("\nAux EEPROM probe failed!\n");
		return;
	}

	ret = i2c_set_chip_offset_len(idev, 2);
	if (ret) {
		printf("\nAux EEPROM offset len failed!\n");
		goto aux;
	}

	if (dm_i2c_read(idev, 0, buf, 0x30)) {
		printf("\nAux EEPROM read failed!\n");
		return;
	}

	if (buf[0] != 0xab) {
		printf("\nInvalid Aux EEPROM mmap %d\n", buf[0]);
		return;
	}

	strncpy(aux_ident, &buf[0x10], 8);
	strncpy(&aux_ident[strlen(aux_ident)], &buf[0x18], 8);
	snprintf(&aux_ident[strlen(aux_ident)], 4, "-%02d", buf[0x1]);
	strncpy(aux_serial, &buf[0x0020], 16);
	lowercaseify(aux_ident);

	return;
}

void setup_env(int debug_gpios, char* sbc_ident, char* sbc_serial, char* aux_ident, char* aux_serial)
{
	int override = debug_gpios & 3;
	const char* const overrides[] = {0, "ec-asm-00027-01", 0, "ec-asm-00025-02"};
	char buf[256];

	if (!strcmp(sbc_ident, "")) {
		strcpy(sbc_ident, "ec-asm-00014-01");
		printf("No sbc_board_ident readable, defaulting to %s\n", sbc_ident);
	}
	if (!strcmp(aux_ident, "")) {
		strcpy(aux_ident, "ec-asm-00025-02");
		printf("No aux_board_ident readable, defaulting to %s\n", aux_ident);
	}
	if (overrides[override]) {
		printf("Override set to %d, forcing aux_board_ident to %s\n", override, overrides[override]);
		strcpy(aux_ident, overrides[override]);
	}

	snprintf(buf, sizeof(buf), "imx8qm-%s-with-%s.dtb", sbc_ident, aux_ident);
	env_set("fdt_file", buf);
	snprintf(buf, sizeof(buf), "sbc_board_ident=%s aux_board_ident=%s sbc_board_serial=%s aux_board_serial=%s debug_switches=%d", sbc_ident, aux_ident, sbc_serial, aux_serial, debug_gpios);
	env_set("kernelparams", buf);
	printf("FDT file: %s\n", env_get("fdt_file"));
}

extern uint32_t _end_ofs;

int board_late_init(void)
{
	bool m4_booted;
	int ret;
	int debug_gpios;
	char sbc_ident[21] = {0};
	char sbc_serial[17] = {0};
	char aux_ident[21] = {0};
	char aux_serial[17] = {0};

	build_info();

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "Refeyn EC-ASM-00014-01");
	env_set("board_rev", "iMX8QM");
#endif

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	m4_booted = m4_parts_booted();
	printf("M4 booted: %s\n", m4_booted ? "true" : "false");

	ret = read_debug_switches(&debug_gpios);
	if (ret) {
		return ret;
	}
	read_eeprom_data(sbc_ident, sbc_serial, aux_ident, aux_serial);
	setup_env(debug_gpios, sbc_ident, sbc_serial, aux_ident, aux_serial);

	printf("Debug switches (0-5): ");
	for (int i = 0; i < 6; ++i) {
		printf("%d", (debug_gpios & (1<<i)) != 0);
	}
	printf("\n");

	printf("SBC: %s %s\n", sbc_ident, sbc_serial);
	printf("Aux: %s %s\n", aux_ident, aux_serial);


#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

#if defined(CONFIG_IMX_LOAD_HDMI_FIMRWARE_RX) || defined(CONFIG_IMX_LOAD_HDMI_FIMRWARE_TX)
	char *end_of_uboot;
	char command[256];
	end_of_uboot = (char *)(ulong)(CONFIG_SYS_TEXT_BASE + _end_ofs + fdt_totalsize(gd->fdt_blob));
	end_of_uboot += 9;

	/* load hdmitxfw.bin and hdmirxfw.bin*/
	memcpy((void *)IMX_HDMI_FIRMWARE_LOAD_ADDR, end_of_uboot,
			IMX_HDMITX_FIRMWARE_SIZE + IMX_HDMIRX_FIRMWARE_SIZE);

#ifdef CONFIG_IMX_LOAD_HDMI_FIMRWARE_TX
	sprintf(command, "hdp load 0x%x", IMX_HDMI_FIRMWARE_LOAD_ADDR);
	run_command(command, 0);
#endif
#ifdef CONFIG_IMX_LOAD_HDMI_FIMRWARE_RX
	sprintf(command, "hdprx load 0x%x",
			IMX_HDMI_FIRMWARE_LOAD_ADDR + IMX_HDMITX_FIRMWARE_SIZE);
	run_command(command, 0);
#endif
#endif /* CONFIG_IMX_LOAD_HDMI_FIMRWARE_RX || CONFIG_IMX_LOAD_HDMI_FIMRWARE_TX */

	return 0;
}
