/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <base/init.h>

#include <of/of.h>

#include <minix_rt/clocksource.h>

static const struct of_device_id __timer_of_table_sentinel
	__used __section(__timer_of_table_end);

extern struct of_device_id __timer_of_table_start[];

void __init timer_probe(void)
{
	struct device_node *np;
	const struct of_device_id *match;
	of_init_fn_1_ret init_func_ret;
	unsigned timers = 0;
	int ret;

	for_each_matching_node_and_match(np, __timer_of_table_start, &match) {
		if (!of_device_is_available(np))
			continue;

		init_func_ret = match->data;

		ret = init_func_ret(np);
		if (ret) {
			printf("Failed to initialize '%pOF': %d\n", np, ret);
			continue;
		}

		timers++;
	}

	if (!timers)
		printf("%s: no matching timers found\n", __func__);
}
