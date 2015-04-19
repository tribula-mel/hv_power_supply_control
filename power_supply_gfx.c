/*
 * Graphical UI code
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

#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_color.h>
 
#include "types.h"
#include "power_supply_gfx.h"

/* enable for debugging */
#undef DEBUG

#define DISPLAY_X 640
#define DISPLAY_Y 480

#define LINE_THIKNESS 2.0f

#define FONT_SIZE_12 12
#define FONT_SIZE_24 24

#define START_ANGLE 2*ALLEGRO_PI/3
#define COUNTER_CW_LIMIT 2*ALLEGRO_PI/3
#define CW_LIMIT 7*ALLEGRO_PI/3

#define VOLTAGE_UPPER_THRESHOLD 0.7f
#define VOLTAGE_LOWER_THRESHOLD 0.2f

#define INPUT_CHANNEL_SHIFT 8

#define CFG_FILE "data/power_supply.cfg"

static double convert_to_ps_voltage(power_supply_t *ps, double voltage);
static bool load_io_plugin(power_supply_t *ps);
static bool read_ale_config(ALLEGRO_CONFIG *cfg, char *section,
                            char *key, char *value, uint32_t len);
static void draw_circle(led_t *led);
static void draw_filled_circle(led_t *leds, ALLEGRO_COLOR color);
static void draw_knob(knob_t *knob);
static void draw_rectangle(control_t *controls);
static void draw_filled_rectangle(control_t *controls, ALLEGRO_COLOR color);
static bool init_allegro(void);
static bool init_fonts(void *obj, uint32_t obj_size,
                       uint32_t n_elem, unsigned short font_size);
static void init_colors(void *obj, uint32_t obj_size,
                        uint32_t n_elem);
static bool init_plugin_file(ALLEGRO_CONFIG *cfg);
static bool init_ps_config(power_supply_t *ps);
static bool init_title_gfx(ALLEGRO_CONFIG *cfg);
static bool init_leds_gfx(power_supply_t *ps);
static bool init_knobs_gfx(power_supply_t *ps);
static bool init_controls_gfx(power_supply_t *ps);
static bool init_title(ALLEGRO_CONFIG *cfg);
static bool init_leds(power_supply_t *ps);
static bool init_knobs(power_supply_t *ps);
static bool init_controls(power_supply_t *ps);
static void draw_display(power_supply_t *ps);
static bool check_button(power_supply_t *ps, ALLEGRO_EVENT *event, int *button);
static bool check_knob(power_supply_t *ps, ALLEGRO_EVENT *event, int *knob);
static void check_leds(power_supply_t *ps);
static void process_event_mouse_axes(power_supply_t *ps, ALLEGRO_EVENT *event);
static void process_event_mouse_button_up(power_supply_t *ps, ALLEGRO_EVENT *event);
static void process_event_timer(power_supply_t *ps, ALLEGRO_EVENT *event);
static void process_events(power_supply_t *ps, ALLEGRO_DISPLAY *display);
static bool init_elements(power_supply_t *ps);
static bool save_voltage_setting(power_supply_t *ps);
static bool load_config_file(power_supply_t *ps);
static power_supply_t *allocate_main_object();

/* title text */

static title_t title[] = {
   {
      .font = NULL,
   },
};
#define TITLE_N sizeof(title)/sizeof(title_t)

static ps_handler_t handler;
static char plugin_file[256];

static double convert_to_ps_voltage(power_supply_t *ps, double voltage)
{
   return ((ps->v_program_max * voltage) / ps->voltage_full_output);
}

static bool load_io_plugin(power_supply_t *ps)
{
   char *error;
   bool rc;

   rc = init_plugin_file(ps->cfg);
   if (rc == false)
      return false;

   handler.handle = dlopen(plugin_file, RTLD_NOW);
   if (!handler.handle) {
      fprintf(stderr, "problem loading io handler plugin: %s\n", dlerror());
      return false;
   }

   handler.analog_channel_input = dlsym(handler.handle, "analog_channel_input");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.digital_channel_output_high = dlsym(handler.handle, "digital_channel_output_high");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.digital_channel_output_low = dlsym(handler.handle, "digital_channel_output_low");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.analog_channel_output = dlsym(handler.handle, "analog_channel_output");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.convert_button_to_channel = dlsym(handler.handle, "convert_button_to_channel");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.convert_knob_to_channel = dlsym(handler.handle, "convert_knob_to_channel");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }
   handler.io_plugin_initialized = dlsym(handler.handle, "io_plugin_initialized");
   if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "dlsym problem: %s\n", error);
      return false;
   }

   return true;
}

static bool read_ale_config(ALLEGRO_CONFIG *cfg, char *section,
                            char *key, char *value, uint32_t len)
{
   const char *str;
   size_t str_len;

   str = al_get_config_value(cfg, section, key);
   if (str == NULL) {
      fprintf(stderr, "failed to read key[%s] in section[%s]!\n", key, section);
      return false;
   }
   str_len = strlen(str);
   if (str_len > len) {
      fprintf(stderr, "requested value (key[%s] in section[%s]) too long!\n",
              key, section);
      return false;
   }
   memset(value, 0, len);
   memcpy(value, str, str_len);

   return true;
}

static void draw_circle(led_t *led)
{
   ALLEGRO_COLOR color = led->gfx_color;
   float text_width;
   float text_x, text_y;

   al_draw_circle(led->gfx.x, led->gfx.y, led->gfx.r, color, LINE_THIKNESS);

   /* draw text centered bellow the circle */
   text_width = al_get_text_width(led->font, led->title);
   text_x = (led->gfx.x - led->gfx.r) + (led->gfx.r - text_width/2);
   text_y = (led->gfx.y + led->gfx.r) + 5/100*led->gfx.r;
   al_draw_textf(led->font, color, text_x, text_y, 0, "%s", led->title);
}

static void draw_filled_circle(led_t *led, ALLEGRO_COLOR color)
{
   al_draw_filled_circle(led->gfx.x, led->gfx.y, led->gfx.r, color);
}

static void draw_knob(knob_t *knob)
{
   ALLEGRO_COLOR color = knob->gfx_color;
   circle_t gfx = knob->gfx;
   float text_width;
   float text_x, text_y;
   float knob_r, knob_x, knob_y;

   al_draw_circle(gfx.x, gfx.y, gfx.r, color, LINE_THIKNESS);
   knob_r = knob->knob_r * gfx.r;
   knob_x = knob_r * cosf(knob->angle) + gfx.x;
   knob_y = knob_r * sinf(knob->angle) + gfx.y;
   al_draw_circle(knob_x, knob_y, 4, color, LINE_THIKNESS);

   /* draw text centered bellow the outer circle */
   text_width = al_get_text_width(knob->font, knob->title);
   text_x = (gfx.x - gfx.r) + (gfx.r - text_width/2);
   text_y = (gfx.y + gfx.r) + 5/100*gfx.r;
   al_draw_textf(knob->font, color, text_x, text_y, 0,
                 "%s %f V", knob->title, knob->voltage_setting);
}

static void draw_rectangle(control_t *control)
{
   ALLEGRO_COLOR color = control->gfx_color;
   float text_width, rec_width, rec_height;
   float text_x, text_y;

   al_draw_rectangle(control->gfx.x1, control->gfx.y1,
                     control->gfx.x2, control->gfx.y2, color, LINE_THIKNESS);

   /* draw text centered bellow the rectangle */
   text_width = al_get_text_width(control->font, control->title);
   rec_width = control->gfx.x2 - control->gfx.x1;
   rec_height = control->gfx.y2 - control->gfx.y1;
   text_x = control->gfx.x1 + (rec_width - text_width)/2;
   text_y = control->gfx.y2 + 5/100*rec_height;
   al_draw_textf(control->font, color, text_x, text_y, 0, "%s", control->title);
}

static void draw_filled_rectangle(control_t *controls, ALLEGRO_COLOR color)
{
   al_draw_filled_rectangle(controls->gfx.x1, controls->gfx.y1,
                            controls->gfx.x2, controls->gfx.y2, color);
}

static bool init_allegro(void)
{
   bool rc = false;

   if (!al_init()) {
      fprintf(stderr, "failed to initialize allegro!\n");
      return false;
   }
 
   rc = al_install_mouse();
   if (rc == false) {
      fprintf(stderr, "failed to install mouse!\n");
      return false;
   }
 
   /* circles, rectangles, arcs ... */
   rc = al_init_primitives_addon();
   if (rc == false) {
      fprintf(stderr, "failed to init primitives addon!\n");
      return false;
   }

   /* font add-on is needed for ttf add-on */
   al_init_font_addon();
   al_init_ttf_addon();
 
   return true;
}

static bool init_fonts(void *obj, uint32_t obj_size,
                       uint32_t n_elem, unsigned short font_size)
{
   ALLEGRO_FONT **font;
   const char *font_file = "data/DejaVuSans.ttf";
   uint32_t i = 0;

   for (i = 0; i < n_elem; i++) {
      font = obj;
      *font = al_load_font(font_file, font_size, 0);
      if (*font == NULL) {
         fprintf(stderr, "failed to load title font size[%d]!\n", font_size);
         return false; 
      }
      obj += obj_size;
   }

   return true;
}

static void init_colors(void *obj, uint32_t obj_size,
                        uint32_t n_elem)
{
   ALLEGRO_COLOR *color;
   uint32_t i = 0;

   for (i = 0; i < n_elem; i++) {
      color = obj;
      *color = al_map_rgb(32, 92, 46); /* green */
      obj += obj_size;
   }
}

static bool init_plugin_file(ALLEGRO_CONFIG *cfg)
{
   bool rc;
   char value[256];

   rc = read_ale_config(cfg, "plugin", "file", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read file value from section[plugin]!\n");
      return false;
   }
   memcpy(plugin_file, value, sizeof(value));

   return true;
}

static bool init_ps_config(power_supply_t *ps)
{
   ALLEGRO_CONFIG *cfg = ps->cfg;
   bool rc;
   char value[256];
   uint32_t l_value;

   rc = read_ale_config(cfg, "power_supply", "voltage_full_output", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for voltage_full_output!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert voltage_full_output value from section[power_supply]!\n");
      return false;
   }
   ps->voltage_full_output = l_value;

   memset(&value[0], 0, 255);
   rc = read_ale_config(cfg, "power_supply", "v_program_max", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for v_program_max!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert v_program_max value from section[power_supply]!\n");
      return false;
   }
   ps->v_program_max = l_value;

   memset(&value[0], 0, 255);
   rc = read_ale_config(cfg, "power_supply", "v_program_min", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for v_program_min!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert v_program_min value from section[power_supply]!\n");
      return false;
   }
   ps->v_program_min = l_value;

   memset(&value[0], 0, 255);
   rc = read_ale_config(cfg, "power_supply", "leds", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for number of leds!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert leds value from section[power_supply]!\n");
      return false;
   }
   ps->LEDS_N = l_value;
   ps->leds = malloc(ps->LEDS_N * sizeof(led_t));
   if (ps->leds == NULL) {
      fprintf(stderr, "failed to allocate memory for leds!\n");
      return false;
   }
   memset(ps->leds, ps->LEDS_N * sizeof(led_t), 0);

   memset(&value[0], 0, 255);
   rc = read_ale_config(cfg, "power_supply", "knobs", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for number of knobs!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert knobs value from section[power_supply]!\n");
      return false;
   }
   ps->KNOBS_N = l_value;
   ps->knobs = malloc(ps->KNOBS_N * sizeof(knob_t));
   if (ps->knobs == NULL) {
      fprintf(stderr, "failed to allocate memory for knobs!\n");
      return false;
   }
   memset(ps->knobs, ps->KNOBS_N * sizeof(knob_t), 0);

   memset(&value[0], 0, 255);
   rc = read_ale_config(cfg, "power_supply", "controls", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for number of controls!\n");
      return false;
   }
   l_value = strtol(value, NULL, 10);
   if ((l_value == LONG_MIN) || (l_value == LONG_MAX)) {
      fprintf(stderr, "failed to convert controls value from section[power_supply]!\n");
      return false;
   }
   ps->CONTROLS_N = l_value;
   ps->controls = malloc(ps->CONTROLS_N * sizeof(control_t));
   if (ps->controls == NULL) {
      fprintf(stderr, "failed to allocate memory for controls!\n");
      return false;
   }
   memset(ps->controls, ps->CONTROLS_N * sizeof(control_t), 0);

   return rc;
}

static bool init_title_gfx(ALLEGRO_CONFIG *cfg)
{
   bool rc;
   char value[256];

   rc = read_ale_config(cfg, "power_supply", "title", &value[0], 255);
   if (rc == false) {
      fprintf(stderr, "failed to read configuration for title!\n");
      return false;
   }
   memcpy(&title[0].title, value, strlen(value));

   return rc;
}

static bool init_leds_gfx(power_supply_t *ps)
{
   ALLEGRO_CONFIG *cfg = ps->cfg;
   int i;
   bool rc;
   char section[256] = {0};
   char value[256];
   float f_value;

   for (i = 0; i < ps->LEDS_N; i++) {
      switch(i) {
      case overload:
         memcpy(section, "overload_led", sizeof("overload_led"));
         break;
      case thermal_overload:
         memcpy(section, "thermal_overload_led", sizeof("thermal_overload_led"));
         break;
      case interlock:
         memcpy(section, "interlock_led", sizeof("interlock_led"));
         break;
      case overvoltage:
         memcpy(section, "overvoltage_led", sizeof("overvoltage_led"));
         break;
      case end_of_charge:
         memcpy(section, "end_of_charge_led", sizeof("end_of_charge_led"));
         break;
      case inhibit:
         memcpy(section, "inhibit_led", sizeof("inhibit_led"));
         break;
      default:
         fprintf(stderr, "unexpected led switch case!\n");
         return false;
      }

      /* read x coordinate */
      rc = read_ale_config(cfg, section, "x", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read x value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert x value from section[%s]!\n", section);
         return false;
      }
      ps->leds[i].gfx.x = f_value;

      /* read y coordinate */
      rc = read_ale_config(cfg, section, "y", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read y value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert y value from section[%s]!\n", section);
         return false;
      }
      ps->leds[i].gfx.y = f_value;

      /* read r radius */
      rc = read_ale_config(cfg, section, "r", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read r value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert r value from section[%s]!\n", section);
         return false;
      }
      ps->leds[i].gfx.r = f_value;

      /* read state */
      rc = read_ale_config(cfg, section, "state", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read title value from section[%s]!\n", section);
         return false;
      }
      ps->leds[i].state = (!strcmp(value, "on") ? led_on : led_off);

      /* read title */
      rc = read_ale_config(cfg, section, "title", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read title value from section[%s]!\n", section);
         return false;
      }
      memcpy(&ps->leds[i].title, value, strlen(value));
   }

   return rc;
}

static bool init_knobs_gfx(power_supply_t *ps)
{
   ALLEGRO_CONFIG *cfg = ps->cfg;
   int i;
   bool rc;
   char section[256] = {0};
   char value[256];
   float f_value;

   for (i = 0; i < ps->KNOBS_N; i++) {
      switch(i) {
      case output_voltage_selector:
         memcpy(section, "output_voltage_selector", sizeof("output_voltage_selector"));
         break;
      default:
         fprintf(stderr, "unexpected knob switch case!\n");
         return false;
      }

      /* read x coordinate */
      rc = read_ale_config(cfg, section, "x", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read x value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert x value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].gfx.x = f_value;

      /* read y coordinate */
      rc = read_ale_config(cfg, section, "y", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read y value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert y value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].gfx.y = f_value;

      /* read r radius */
      rc = read_ale_config(cfg, section, "r", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read r value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert r value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].gfx.r = f_value;

      /* read knob radius */
      rc = read_ale_config(cfg, section, "knob_r", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read knob_r value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert knob_r value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].knob_r = f_value;

      /* read angle */
      rc = read_ale_config(cfg, section, "angle", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read angle value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert angle value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].angle = f_value;

      /* read clock_wise limit */
      rc = read_ale_config(cfg, section, "clock_wise_limit", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read clock_wise_limit value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert clock_wise_limit value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].clock_wise_limit = f_value;

      /* read counter_clock_wise limit */
      rc = read_ale_config(cfg, section, "counter_clock_wise_limit", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read counter_clock_wise_limit value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert counter_clock_wise_limit value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].counter_clock_wise_limit = f_value;

      /* read voltage setting */
      rc = read_ale_config(cfg, section, "voltage_setting", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read voltage_setting value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert voltage_setting value from section[%s]!\n", section);
         return false;
      }
      ps->knobs[i].voltage_setting = f_value;

      /* read title */
      rc = read_ale_config(cfg, section, "title", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read title value from section[%s]!\n", section);
         return false;
      }
      memcpy(&ps->knobs[i].title, value, strlen(value));
   }

   return rc;
}

static bool init_controls_gfx(power_supply_t *ps)
{
   ALLEGRO_CONFIG *cfg = ps->cfg;
   int i;
   bool rc;
   char section[256] = {0};
   char value[256];
   float f_value;

   for (i = 0; i < ps->CONTROLS_N; i++) {
      switch(i) {
      case enable_power_supply:
         memcpy(section, "enable_power_supply", sizeof("enable_power_supply"));
         break;
      case inhibit_power_supply:
         memcpy(section, "inhibit_power_supply", sizeof("inhibit_power_supply"));
         break;
      case interlock_power_supply:
         memcpy(section, "interlock_power_supply", sizeof("interlock_power_supply"));
         break;
      default:
         fprintf(stderr, "unexpected control switch case!\n");
         return false;
      }

      /* read x1 coordinate */
      rc = read_ale_config(cfg, section, "x1", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read x1 value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert x1 value from section[%s]!\n", section);
         return false;
      }
      ps->controls[i].gfx.x1 = f_value;

      /* read y1 coordinate */
      rc = read_ale_config(cfg, section, "y1", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read y1 value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert y1 value from section[%s]!\n", section);
         return false;
      }
      ps->controls[i].gfx.y1 = f_value;

      /* read x2 coordinate */
      rc = read_ale_config(cfg, section, "x2", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read x2 value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert x2 value from section[%s]!\n", section);
         return false;
      }
      ps->controls[i].gfx.x2 = f_value;

      /* read y2 coordinate */
      rc = read_ale_config(cfg, section, "y2", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read y2 value from section[%s]!\n", section);
         return false;
      }
      f_value = strtof(value, NULL);
      if (errno == ERANGE) {
         fprintf(stderr, "failed to convert y2 value from section[%s]!\n", section);
         return false;
      }
      ps->controls[i].gfx.y2 = f_value;

      /* read state */
      rc = read_ale_config(cfg, section, "state", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read title value from section[%s]!\n", section);
         return false;
      }
      ps->controls[i].state = (!strcmp(value, "on") ? key_on : key_off);

      /* read title */
      rc = read_ale_config(cfg, section, "title", &value[0], 255);
      if (rc == false) {
         fprintf(stderr, "failed to read title value from section[%s]!\n", section);
         return false;
      }
      memcpy(&ps->controls[i].title, value, strlen(value));
   }

   return rc;
}

static bool init_title(ALLEGRO_CONFIG *cfg)
{
   bool rc;

   rc = init_fonts(&title[0].font, sizeof(title_t), TITLE_N, 24);
   if (rc == false)
      return false;

   rc = init_title_gfx(cfg);
   if (rc == false)
      return false;

   return true;
}

static bool init_leds(power_supply_t *ps)
{
   bool rc;

   init_colors(&ps->leds[0].gfx_color, sizeof(led_t), ps->LEDS_N);
   rc = init_fonts(&ps->leds[0].font, sizeof(led_t), ps->LEDS_N, 12);
   if (rc == false)
      return false;

   rc = init_leds_gfx(ps);
   if (rc == false)
      return false;

   return true;
}

static bool init_controls(power_supply_t *ps)
{
   bool rc;

   init_colors(&ps->controls[0].gfx_color, sizeof(control_t), ps->CONTROLS_N);
   rc = init_fonts(&ps->controls[0].font, sizeof(control_t), ps->CONTROLS_N, 12);
   if (rc == false)
      return false;

   rc = init_controls_gfx(ps);
   if (rc == false)
      return false;

   return true;
}

static bool init_knobs(power_supply_t *ps)
{
   bool rc;

   init_colors(&ps->knobs[0].gfx_color, sizeof(knob_t), ps->KNOBS_N);
   rc = init_fonts(&ps->knobs[0].font, sizeof(knob_t), ps->KNOBS_N, 12);
   if (rc == false)
      return false;

   rc = init_knobs_gfx(ps);
   if (rc == false)
      return false;

   return true;
}

static void draw_display(power_supply_t *ps)
{
   ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);
   ALLEGRO_COLOR black = al_map_rgb(0, 0, 0);
   ALLEGRO_COLOR red = al_color_name("red");
   ALLEGRO_COLOR yellow = al_color_name("yellow");
   float x = 0;
   int i = 0;

   al_clear_to_color(black);
   for (i = 0; i < ps->LEDS_N; i++) {
      if (ps->leds[i].state == led_off)
         draw_filled_circle(&ps->leds[i], black);
      else
         draw_filled_circle(&ps->leds[i], yellow);
      draw_circle(&ps->leds[i]);
   }

   for (i = 0; i < ps->KNOBS_N; i++)
      draw_knob(&ps->knobs[i]);

   for (i = 0; i < ps->CONTROLS_N; i++) {
      if (ps->controls[i].state == key_off)
         draw_filled_rectangle(&ps->controls[i], black);
      else
         draw_filled_rectangle(&ps->controls[i], red);
      draw_rectangle(&ps->controls[i]);
   }

   x = (DISPLAY_X - al_get_text_width(title[0].font, title[0].title)) / 2;
   al_draw_textf(title[0].font, white, x, 20, 0, "%s", title[0].title);

   al_flip_display();
}

static bool check_button(power_supply_t *ps, ALLEGRO_EVENT *event, int *button)
{
   int i = 0;

   for (i = 0; i < ps->CONTROLS_N; i++) {
      if ((ps->controls[i].gfx.x2 >= event->mouse.x && ps->controls[i].gfx.x1 <= event->mouse.x) &&
          (ps->controls[i].gfx.y2 >= event->mouse.y && ps->controls[i].gfx.y1 <= event->mouse.y)) {
         *button = i;
         return true;
      }
   }

   return false;
}

static bool check_knob(power_supply_t *ps, ALLEGRO_EVENT *event, int *knob)
{
   int i = 0;
   circle_t gfx;
   float dx, dy;

   for (i = 0; i < ps->KNOBS_N; i++) {
      gfx = ps->knobs[i].gfx;
      dx = powf(event->mouse.x - gfx.x, 2);
      dy = powf(event->mouse.y - gfx.y, 2);
      if ((dx + dy) <= pow(gfx.r, 2)) {
         *knob = i;
         return true;
      }
   }

   return false;
}

static void check_leds(power_supply_t *ps)
{
   int i = 0;
   double voltage = 0.0f;
   bool rc;

   for (i = 0; i < ps->LEDS_N; i++) {
      rc = handler.analog_channel_input(i + INPUT_CHANNEL_SHIFT, &voltage);
      if (rc == false) {
         fprintf(stderr, "analog channel input failed\n");
         return;
      }
      if ((voltage < VOLTAGE_UPPER_THRESHOLD) &&
          (voltage > VOLTAGE_LOWER_THRESHOLD))
         ps->leds[i].state = led_on;
      else
         ps->leds[i].state = led_off;
   }
}

static void process_event_mouse_axes(power_supply_t *ps, ALLEGRO_EVENT *event)
{
   double voltage;
   float angle = 0;
   float angle_delta = 0;
   int knob = -1;
   int channel = -1;
   bool rc = false;

#ifdef DEBUG
   printf("event type[ALLEGRO_EVENT_MOUSE_AXES]\n");
   printf("event->mouse.x[%d]\n", event->mouse.x);
   printf("event->mouse.y[%d]\n", event->mouse.y);
#endif
   rc = check_knob(ps, event, &knob);
   if (rc == true) {
      angle_delta = ALLEGRO_PI * event->mouse.dz / 256;
      angle = ps->knobs[knob].angle + angle_delta;
      if (angle < ps->knobs[knob].counter_clock_wise_limit) {
         angle = ps->knobs[knob].counter_clock_wise_limit;
         angle_delta = 0;
         ps->knobs[knob].voltage_setting = 0;
      } else if (angle > ps->knobs[knob].clock_wise_limit) {
         angle = ps->knobs[knob].clock_wise_limit;
         angle_delta = 0;
         ps->knobs[knob].voltage_setting = ps->voltage_full_output;
      }

      ps->knobs[knob].angle = angle;
      ps->knobs[knob].voltage_setting += ps->voltage_full_output*angle_delta/(CW_LIMIT - COUNTER_CW_LIMIT);
      if (ps->knobs[knob].voltage_setting < 0)
         ps->knobs[knob].voltage_setting = 0;
      if (ps->knobs[knob].voltage_setting > ps->voltage_full_output)
         ps->knobs[knob].voltage_setting = ps->voltage_full_output;
      voltage = convert_to_ps_voltage(ps, ps->knobs[knob].voltage_setting);
#ifdef DEBUG
      printf("voltage for ale102 [%g]\n", voltage);
#endif
      channel = handler.convert_knob_to_channel(knob);
      if (channel == -1) {
         fprintf(stderr, "conversion for knob[%d] failed\n", knob);
         return;
      }
      rc = handler.analog_channel_output(channel, voltage, ps->v_program_max, ps->v_program_min);
      if (rc == false) {
         fprintf(stderr, "output to pcidas1602/16 analog channel[%d] failed\n",
                 channel);
      }
      draw_display(ps);
   }
}

static void process_event_mouse_button_up(power_supply_t *ps, ALLEGRO_EVENT *event)
{
   ALLEGRO_COLOR yellow = al_color_name("yellow");
   ALLEGRO_COLOR green1 = al_map_rgb(75, 105, 47);

   int button = -1;
   int channel = -1;
   bool rc = false;

#ifdef DEBUG
   printf("event type[ALLEGRO_EVENT_MOUSE_BUTTON_UP]\n");
#endif
   rc = check_button(ps, event, &button);
   if (rc == true) {
#ifdef DEBUG
      printf("button[%d] state[%d]\n", button, controls[button].state);
#endif
      channel = handler.convert_button_to_channel(button);
      if (channel == -1) {
         fprintf(stderr, "conversion for button[%d] failed\n", button);
         return;
      }
      if (ps->controls[button].state == key_on) {
         ps->controls[button].state = key_off;
         draw_filled_rectangle(&ps->controls[button], yellow);
         rc = handler.digital_channel_output_low(channel);
         if (rc == false)
            fprintf(stderr,
                    "digital_channel_output_low for channel[%d] failed\n",
                    channel);
      } else {
         ps->controls[button].state = key_on;
         draw_filled_rectangle(&ps->controls[button], green1);
         rc = handler.digital_channel_output_high(channel);
         if (rc == false)
            fprintf(stderr,
                    "digital_channel_output_high for channel[%d] failed\n",
                    channel);
      }
      draw_display(ps);
   }
}

static void process_event_timer(power_supply_t *ps, ALLEGRO_EVENT *event)
{
   check_leds(ps);
}

static void process_events(power_supply_t *ps, ALLEGRO_DISPLAY *display)
{
   ALLEGRO_EVENT_QUEUE *queue;
   ALLEGRO_EVENT event;
   ALLEGRO_TIMER *timer;

   timer = al_create_timer(1.0 / 30);
   if (timer == NULL) {
      fprintf(stderr, "al_create_timer failed\n");
      return;
   }
   queue = al_create_event_queue();
   al_register_event_source(queue, al_get_mouse_event_source());
   al_register_event_source(queue, al_get_display_event_source(display));
   al_register_event_source(queue, al_get_timer_event_source(timer));
   al_start_timer(timer);

   while (true) {
      al_wait_for_event(queue, &event);
      switch(event.type) {
      case ALLEGRO_EVENT_DISPLAY_CLOSE:
          return;
      case ALLEGRO_EVENT_MOUSE_AXES:
         process_event_mouse_axes(ps, &event);
         break;
      case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
#ifdef DEBUG
         printf("event.type[ALLEGRO_EVENT_MOUSE_BUTTON_DOWN]\n");
#endif
         break;
      case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
         process_event_mouse_button_up(ps, &event);
         break;
      case ALLEGRO_EVENT_MOUSE_WARPED:
#ifdef DEBUG
         printf("event.type[ALLEGRO_EVENT_MOUSE_WARPED]\n");
#endif
         break;
      case ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY:
#ifdef DEBUG
         printf("event.type[ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY]\n");
#endif
         break;
      case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
#ifdef DEBUG
         printf("event.type[ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY]\n");
#endif
         break;
      case ALLEGRO_EVENT_TIMER:
         process_event_timer(ps, &event);
         draw_display(ps);
         break;
      }
   }
}

static bool init_elements(power_supply_t *ps)
{
   ALLEGRO_CONFIG* cfg = ps->cfg;
   bool rc = false;

   rc = init_ps_config(ps);
   if (rc == false)
      return false;

#ifdef DEBUG
   printf("leds[%d]\n", ps->LEDS_N);
   printf("knobs[%d]\n", ps->KNOBS_N);
   printf("controls[%d]\n", ps->CONTROLS_N);
#endif

   rc = init_leds(ps);
   if (rc == false)
      return false;

   rc = init_controls(ps);
   if (rc == false)
      return false;

   rc = init_knobs(ps);
   if (rc == false)
      return false;

   rc = init_title(cfg);
   if (rc == false)
      return false;

   al_destroy_config(cfg);

   return true;
}

static bool save_voltage_setting(power_supply_t *ps)
{
   char angle[256];
   char voltage[256];
   bool rc;

   ps->cfg = al_load_config_file(CFG_FILE);
   if (ps->cfg == NULL) {
      fprintf(stderr, "failed to load %s file!\n", CFG_FILE);
      return false;
   }

   snprintf(angle, sizeof(angle), "%f", ps->knobs[0].angle);
   al_set_config_value(ps->cfg, "output_voltage_selector", "angle", angle);
   snprintf(voltage, sizeof(voltage), "%f", ps->knobs[0].voltage_setting);
   al_set_config_value(ps->cfg, "output_voltage_selector", "voltage_setting", voltage);

   rc = al_save_config_file(CFG_FILE, ps->cfg);
   if (rc == false) {
      fprintf(stderr, "failed to save %s file!\n", CFG_FILE);
      return false;
   }

   al_destroy_config(ps->cfg);

   return true;
}

static bool load_config_file(power_supply_t *ps)
{
   ps->cfg = al_load_config_file(CFG_FILE);
   if (ps->cfg == NULL) {
      fprintf(stderr, "failed to load %s file!\n", CFG_FILE);
      return false;
   }

   return true;
}

static power_supply_t *allocate_main_object()
{
   power_supply_t *ps = NULL;

   ps = malloc(sizeof(power_supply_t));
   if (ps == NULL) {
      perror("malloc error");
   }

   return ps;
}

int main(int argc, char **argv)
{
   power_supply_t *ps = NULL;
   ALLEGRO_DISPLAY *display = NULL;
   bool rc = false;

   ps = allocate_main_object();
   if (ps == NULL)
      return EXIT_FAILURE;

   rc = load_config_file(ps);
   if (rc == false)
      return EXIT_FAILURE;

   rc = load_io_plugin(ps);
   if (rc == false)
      return EXIT_FAILURE;
   if (*handler.io_plugin_initialized != true) {
      fprintf(stderr, "failed to initialize comedi!\n");
      return EXIT_FAILURE;
   }

   rc = init_allegro();
   if (rc == false)
      return EXIT_FAILURE;

   rc = init_elements(ps);
   if (rc == false)
      return EXIT_FAILURE;

   display = al_create_display(DISPLAY_X, DISPLAY_Y);
   if (display == NULL) {
      fprintf(stderr, "failed to create display!\n");
      return EXIT_FAILURE; 
   }
   draw_display(ps);
 
   process_events(ps, display);

#if 0
   al_rest(5.0);
#endif

   rc = save_voltage_setting(ps);
   if (rc == false)
      fprintf(stderr, "failed to save voltage!\n");

   al_destroy_display(display);

   if (handler.handle != NULL)
      dlclose(handler.handle);
 
   return EXIT_SUCCESS;
}
