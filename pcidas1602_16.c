/*
 * Support code for PCI-DAS1602/16 IO card
 * Copyright (C) 2015 Tribula Mel <tribula.mel@gmail.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

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
#include <stdlib.h>
#include <stdbool.h>
#include <comedilib.h>
#include <ctype.h>
#include <math.h>

#include "types.h"

/* enable for debugging */
#undef DEBUG

/* supported interfaces by the card */
enum {
   ANALOG_INPUT = 0,
   ANALOG_OUTPUT,
   DIGITAL_IO,
};
 
/* we have 16 analog input single-ended channels */
enum {
   AI_CHANNEL_0 = 0,
   AI_CHANNEL_1,
   AI_CHANNEL_2,
   AI_CHANNEL_3,
   AI_CHANNEL_4,
   AI_CHANNEL_5,
   AI_CHANNEL_6,
   AI_CHANNEL_7,
   AI_CHANNEL_8,
   AI_CHANNEL_9,
   AI_CHANNEL_10 = 10,
   AI_CHANNEL_11,
   AI_CHANNEL_12,
   AI_CHANNEL_13,
   AI_CHANNEL_14,
   AI_CHANNEL_15,
};

/* we have 2 analog output channels */
enum {
   AO_CHANNEL_0 = 0,
   AO_CHANNEL_1,
};

/* we have 24 digital IO channels */
enum {
   DIO_CHANNEL_0 = 0,
   DIO_CHANNEL_1,
   DIO_CHANNEL_2,
   DIO_CHANNEL_3,
   DIO_CHANNEL_4,
   DIO_CHANNEL_5,
   DIO_CHANNEL_6,
   DIO_CHANNEL_7,
   DIO_CHANNEL_8,
   DIO_CHANNEL_9,
   DIO_CHANNEL_10 = 10,
   DIO_CHANNEL_11,
   DIO_CHANNEL_12,
   DIO_CHANNEL_13,
   DIO_CHANNEL_14,
   DIO_CHANNEL_15,
   DIO_CHANNEL_16,
   DIO_CHANNEL_17,
   DIO_CHANNEL_18,
   DIO_CHANNEL_19,
   DIO_CHANNEL_20 = 20,
   DIO_CHANNEL_21,
   DIO_CHANNEL_22,
   DIO_CHANNEL_23,
};

/* lets pick -10..+10V input range */
#define ANALOG_INPUT_RANGE_10_10V 0
/* lets pick -10..+10V output range */
#define ANALOG_OUTPUT_RANGE_10_10V 1
#define ANALOG_OUTPUT_RANGE_0_10V 3

static const char filename[] = "/dev/comedi0";

#define BIT_0 0
#define BIT_1 1

typedef struct pcidas1602_16 {
   comedi_t *device;
} pcidas1602_16_t;
  
bool io_plugin_initialized = false;

static pcidas1602_16_t das_io_card;

bool analog_channel_input(uint32_t channel, double *value)
{
   comedi_t *device = das_io_card.device;
   lsampl_t data;
   comedi_range *range_info;
   lsampl_t maxdata;
   int retval;

   if (channel > AI_CHANNEL_15) {
      fprintf(stderr, "channel[%d] out or range\n", channel);
      return false;
   }

   comedi_set_global_oor_behavior(COMEDI_OOR_NAN);

   /* read input channel */
   retval = comedi_data_read(device, ANALOG_INPUT, channel,
                             ANALOG_INPUT_RANGE_10_10V, AREF_GROUND, &data);
   if (retval < 0) {
      fprintf(stderr, "error reading channel[%d]\n", channel);
      return false;
   }

   range_info = comedi_get_range(device, ANALOG_INPUT, channel,
                                 ANALOG_INPUT_RANGE_10_10V);
   maxdata = comedi_get_maxdata(device, ANALOG_INPUT, channel);
   *value = comedi_to_phys(data, range_info, maxdata);
   if (isnan(*value)) {
      fprintf(stderr, "out of range [%g,%g]\n",
              range_info->min, range_info->max);
      return false;
   } else {
#ifdef DEBUG
      printf("%g", *value);
      switch(range_info->unit) {
      case UNIT_volt: printf(" V"); break;
      case UNIT_mA: printf(" mA"); break;
      case UNIT_none: break;
      default: printf(" (unknown unit %d)", range_info->unit);
      }
      printf(" (%lu in raw units)\n", (unsigned long)data);
#endif
   }

   return true;
}

bool analog_channel_output(uint32_t channel, double value, double v_max, double v_min)
{
   comedi_t *device = das_io_card.device;
   comedi_range *range_info;
   lsampl_t data;
   lsampl_t maxdata;
   int retval;

   if (channel > AO_CHANNEL_1) {
      fprintf(stderr, "channel[%d] out or range\n", channel);
      return false;
   }

   if ((value > v_max) || (value < v_min)) {
      fprintf(stderr, "voltage[%g] out or range\n", value);
      return false;
   }

   range_info = comedi_get_range(device, ANALOG_OUTPUT, channel, ANALOG_OUTPUT_RANGE_0_10V);
   maxdata = comedi_get_maxdata(device, ANALOG_OUTPUT, channel);
#ifdef DEBUG
   printf("output [0,%d] -> [%g,%g]\n", maxdata,
          range_info->min, range_info->max);
#endif
   data = comedi_from_phys(value, range_info, maxdata);
   retval = comedi_data_write(device, ANALOG_OUTPUT, channel, ANALOG_OUTPUT_RANGE_0_10V, AREF_GROUND, data);
   if ( retval == -1) {
      fprintf(stderr, "error setting %gV on output channel[%d]\n", value, channel);
      return false;
   }

   return true;
}

bool digital_channel_output_high(uint32_t channel)
{
   comedi_t *device = das_io_card.device;
   int retval = 0;

   if (channel > DIO_CHANNEL_23) {
      fprintf(stderr, "channel[%d] out or range\n", channel);
      return false;
   }

   retval = comedi_dio_config(device, DIGITAL_IO, channel, COMEDI_OUTPUT);
   if ( retval == -1) {
      fprintf(stderr, "error setting output direction on channel[0]\n");
      return false;
   }
   retval = comedi_dio_write(device, DIGITAL_IO, channel, BIT_1);
   if ( retval == -1) {
      fprintf(stderr, "error setting high output channel[%d]\n", channel);
      return false;
   }

   return true;
}

bool digital_channel_output_low(uint32_t channel)
{
   comedi_t *device = das_io_card.device;
   int retval;

   if (channel > DIO_CHANNEL_23) {
      fprintf(stderr, "channel[%d] out or range\n", channel);
      return false;
   }

   retval = comedi_dio_config(device, DIGITAL_IO, channel, COMEDI_OUTPUT);
   if ( retval == -1) {
      fprintf(stderr, "error setting output direction on channel[0]\n");
      return false;
   }
   retval = comedi_dio_write(device, DIGITAL_IO, channel, BIT_0);
   if ( retval == -1) {
      fprintf(stderr, "error setting low on output channel[%d]\n", channel);
      return false;
   }

   return true;
}

void __attribute__ ((constructor)) init_pcidas1602_16(void)
{
   comedi_t *device;
   bool rc;

   device = comedi_open(filename);
   if (device == NULL) {
      comedi_perror(filename);
      goto err_init;
   }
   das_io_card.device = device;

   rc = digital_channel_output_low(DIO_CHANNEL_0);
   if (rc == false) {
      fprintf(stderr, "writing to digital channel[%d] failed\n", DIO_CHANNEL_0);
      goto err_init;
   }
   rc = digital_channel_output_low(DIO_CHANNEL_2);
   if (rc == false) {
      fprintf(stderr, "writing to digital channel[%d] failed\n", DIO_CHANNEL_2);
      goto err_init;
   }
   rc = digital_channel_output_low(DIO_CHANNEL_4);
   if (rc == false) {
      fprintf(stderr, "writing to digital channel[%d] failed\n", DIO_CHANNEL_4);
      goto err_init;
   }

   io_plugin_initialized = true;

err_init:
   return;
}

void __attribute__ ((destructor)) fini_pcidas1602_16(void)
{
   if (das_io_card.device != NULL) {
      comedi_close(das_io_card.device);
   }
   io_plugin_initialized = false;
}

/* prototype board interface in between ale102 power supply
 * pcidas1602 board
 */

/* convert knob number into pcidas1602 analog output channel
 * number
 */
int convert_knob_to_channel(uint32_t knob)
{
   switch(knob) {
   case voltage_program_knob:
      return AO_CHANNEL_0;
   default:
      fprintf(stderr, "knob out of bounds[%d]\n", knob);
      return -1;
   }

   return 0;
}

/* convert button number into pcidas1602 digital output channel
 * number
 */
int convert_button_to_channel(uint32_t button)
{
   switch(button) {
   case enable_key:
      return DIO_CHANNEL_4;
   case inhibit_key:
      return DIO_CHANNEL_0;
   case interlock_key:
      return DIO_CHANNEL_2;
   default:
      fprintf(stderr, "button out of bounds[%d]\n", button);
      return -1;
   }

   return 0;
}

