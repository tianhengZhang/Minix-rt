/*
 *  linux/drivers/char/serial_core.h
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __SEL4M_CONSOLE_H_
#define __SEL4M_CONSOLE_H_

#include <of/of.h>

extern int of_setup_console(const struct of_device_id *match,
			     u64 node,
			     const char *options);

#define CONSOLE_DECLARE(name, compat, fn) OF_DECLARE_1(console, name, compat, fn)

#endif /* !__SEL4M_CONSOLE_H_ */