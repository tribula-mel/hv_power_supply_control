# Makefile for the ALE 102 control software
# Copyright (C) 2015 Tribula Mel <tribula.mel@gmail.com> 
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lallegro -lallegro_primitives -lallegro_font -lallegro_ttf -lallegro_color -lm -ldl
SOFLAFS = -fPIC
SHARED = -shared -Wl,-soname,pcidas1602_16.so
LDSHARED = -lcomedi

all: ps_prog pcidas1602_16.so

ps_prog: power_supply_gfx.o
	$(CC) power_supply_gfx.o -o ps_prog $(LDFLAGS)

power_supply_gfx.o: power_supply_gfx.c power_supply_gfx.h types.h
	$(CC) $(CFLAGS) power_supply_gfx.c

pcidas1602_16.so: pcidas1602_16.o
	$(CC) $(SHARED) -o pcidas1602_16.so pcidas1602_16.o $(LDSHARED)

pcidas1602_16.o: pcidas1602_16.c types.h
	$(CC) $(SOFLAGS) $(CFLAGS) pcidas1602_16.c

clean:
	rm -rf core cscope.* *.o ps_prog pcidas1602_16.so

