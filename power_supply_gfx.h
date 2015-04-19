/*
 * Header file for graphics UI code
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

#ifndef __POWER_SUPPLY_GFX_H
#define __POWER_SUPPLY_GFX_H

/* types */

typedef struct title {
   char title[256];
   ALLEGRO_FONT *font;
} title_t;

typedef struct circle {
   float x;
   float y;
   float r;
} circle_t;

typedef struct led {
   circle_t gfx;
   uint32_t state;
   char title[256];
   ALLEGRO_FONT *font;
   ALLEGRO_COLOR gfx_color;
   ALLEGRO_COLOR state_color;
} led_t;

typedef struct knob {
   circle_t gfx;
   float knob_r;
   float angle;
   float clock_wise_limit;
   float counter_clock_wise_limit;
   double voltage_setting;
   char title[256];
   ALLEGRO_FONT *font;
   ALLEGRO_COLOR gfx_color;
} knob_t;

typedef struct rectangle {
   float x1;
   float y1;
   float x2;
   float y2;
} rectangle_t;

typedef struct control {
   rectangle_t gfx;
   uint32_t state;
   char title[256];
   ALLEGRO_FONT *font;
   ALLEGRO_COLOR gfx_color;
   ALLEGRO_COLOR state_color;
} control_t;

typedef struct power_supply {
   ALLEGRO_CONFIG *cfg;
   uint32_t voltage_full_output;
   uint32_t LEDS_N;
   uint32_t KNOBS_N;
   uint32_t CONTROLS_N;
   led_t *leds;
   knob_t *knobs;
   control_t *controls;
   double v_program_max;
   double v_program_min;
} power_supply_t;

/* enums */

enum {
   led_on = 0,
   led_off,
};

/* different leds in the system */
enum {
   overload = 0,
   thermal_overload,
   interlock,
   overvoltage,
   end_of_charge,
   inhibit,
};

enum {
   key_on = 0,
   key_off,
};

/* different knobs in the system */
enum {
   output_voltage_selector = 0,
};

/* different controls in the system */
enum {
   enable_power_supply = 0,
   inhibit_power_supply,
   interlock_power_supply,
};

typedef struct power_supply_handler {
   void *handle;
   bool (*init_pcidas1602_16)(void);
   bool (*analog_channel_input)(uint32_t channel, double *value);
   bool (*digital_channel_output_high)(uint32_t channel);
   bool (*digital_channel_output_low)(uint32_t channel);
   bool (*analog_channel_output)(uint32_t channel, double value, double v_max, double v_min);
   int (*convert_button_to_channel)(uint32_t button);
   int (*convert_knob_to_channel)(uint32_t knob);
   bool *io_plugin_initialized;
} ps_handler_t;

#endif /* __POWER_SUPPLY_GFX_H */
