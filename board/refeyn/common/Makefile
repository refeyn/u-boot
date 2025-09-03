# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2024 Refeyn

ifeq ($(CONFIG_SPL_BUILD),y)
obj-y += refeyn-common-spl.o
else
obj-$(CONFIG_TDX_CFG_BLOCK) += refeyn-common.o
endif
