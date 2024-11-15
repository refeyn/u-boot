/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2024 Refeyn
 */

#ifndef _REFEYN_COMMON_H
#define _REFEYN_COMMON_H

int refeyn_setup_carrier(void);

#if defined(CONFIG_OF_LIBFDT)
int refeyn_ft_board_setup(void *blob, struct bd_info *bd);
#endif

#endif /* _REFEYN_COMMON_H */
