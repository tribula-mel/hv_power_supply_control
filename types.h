/*
 * Supporting header file - types
 * Copyright (C) 2015 Tribula Mel <tribula.mel@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __TYPES_H
#define __TYPES_H

typedef unsigned int uint32_t;

/* available controls in the system */
enum {
   enable_key = 0,
   inhibit_key,
   interlock_key,
};

/* available knobs in the system */
enum {
   voltage_program_knob = 0,
};

#endif /* __TYPES_H */
