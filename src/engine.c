/*
 * Vegimeter 2 Engine
 *
 * Copyright (c) 2013 Sladeware LLC
 */

#include <bb/os.h>
#include <bb/os/drivers/onewire/slaves/ds18b20.h>
#include <propeller.h>
#include <stdio.h>
#include "pins.h"

#define HEAT_PUMP_ACTIVATION 2100 // Centi-Celsius ;)
#define HEATER 6
#define HEATER_DEACTIVATION 4300 // Centi-Celsius ;)
#define POLLING_PERIOD 1000 // Milliseconds
#define PUMP 7
#define STR_SIZE 64

HUBDATA char is_initialized = 0;
HUBDATA char str[STR_SIZE];
HUBDATA int water_temp, soil_temp, soil_a, soil_b, soil_c, soil_d;
HUBDATA int water_a, water_b;
HUBDATA FILE* xbee;

extern _Driver _SimpleSerialDriver;
extern _Driver _FileDriver;

/* This is a list of all drivers we can use in the
 * program. The default _InitIO function opens stdin,
 * stdout, and stderr based on the first driver in
 * the list (the serial driver, for us)
 */
_Driver *_driverlist[] = {
  &_SimpleSerialDriver,
  &_FileDriver,
  NULL
};

int get_temp(unsigned int pin) {
  struct ds18b20_input input;
  struct ds18b20_output output;

  *(int *)input.pin = pin;

  ds18b20_read_temperature(&input, &output);

  return *(int *)output.value;
}

unsigned int pump_on() {
  return OUT_HIGH(PUMP);
}

unsigned int pump_off() {
  return OUT_LOW(PUMP);
}

unsigned int heater_on() {
  return OUT_HIGH(HEATER);
}

unsigned int heater_off() {
  return OUT_LOW(HEATER);
}

int get_soil_a_temp() {
  return get_temp(2);
}

int get_soil_b_temp() {
  return get_temp(3);
}

int get_soil_c_temp() {
  return get_temp(4);
}

int get_soil_d_temp() {
  return get_temp(5);
}

int get_water_a_temp() {
  return get_temp(1);
}

int get_water_b_temp() {
  return get_temp(10);
}

int get_air_temp() {
  return get_temp(0);
}

void engine_wait_ms(unsigned int ms) {
  unsigned waitcycles;
  unsigned millisecond = _clkfreq / 1000;

  waitcycles = CNT;
  while (ms > 0) {
    waitcycles += millisecond;
    __napuntil(waitcycles);
    --ms;
  }
}

void engine_xbee_init() {
  xbee = fopen("SSER:9600,9,8", "w"); // p9 out, p8 in
  if (xbee == NULL) {
    puts("ERROR: Cannot open the XBee");
    return;
  }
  setbuf(xbee, 0);
  fputs("XBee initialized.\n", xbee);
}

void engine_init() {
  if (is_initialized != 1) {
    is_initialized = 1;

    engine_xbee_init();

    heater_off();
    pump_off();

    fputs("Engine initialized.\n", xbee);
  }
}

// From http://code.google.com/p/propsideworkspace/source/browse/Learn/Simple+Libraries/Utility/libsimpletools/itoa.c?spec=svn2c2224c84beb4a7e5569b00754883f84eaae08ac&r=ff7cfd8e63cf9940bbaa2311d82da626e8d7aebf
char* itoa(int i, char b[], int base) {
  char const digit[] = "0123456789ABCDEF";
  char* p = b;
  if (i < 0) {
    *p++ = '-';
    i = -1;
  }

  int shifter = i;
  do { // Move to where representation ends
    ++p;
    shifter = shifter / base;
  } while (shifter);
  *p = '\0';
  do{ // Move back, inserting digits as u go
    *--p = digit[i % base];
    i = i / base;
  } while (i);

  return b;
}

void engine_runner() {
  char temp[16];

  while (1) {
    engine_init();

    strcpy(str, "Air temperature: ");
    itoa(get_air_temp(), temp, 10);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);
    fputs("7\n", xbee);
    soil_a = get_soil_a_temp();
    soil_b = get_soil_b_temp();
    soil_c = get_soil_c_temp();
    soil_d = get_soil_d_temp();
    soil_temp = soil_a + soil_b + soil_c + soil_d;

    strcpy(str, "Soil A,B,C,D,+: ");
    itoa(soil_a, temp, 10);
    strcat(str, temp);
    itoa(soil_b, temp, 10);
    strcat(str, temp);
    itoa(soil_c, temp, 10);
    strcat(str, temp);
    itoa(soil_d, temp, 10);
    strcat(str, temp);
    itoa(soil_temp, temp, 10);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);

    water_a = get_water_a_temp();
    water_b = get_water_b_temp();
    water_temp = water_a + water_b;

    strcpy(str, "Water A,B,+: ");
    itoa(water_a, temp, 10);
    strcat(str, temp);
    itoa(water_b, temp, 10);
    strcat(str, temp);
    itoa(water_temp, temp, 10);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);

    if (soil_temp < HEAT_PUMP_ACTIVATION * 4) {
      if (water_temp > HEATER_DEACTIVATION * 2 ||
	  water_a >> 1 > water_b ||
	  water_b >> 1 > water_a) {
	heater_off();
	fputs("Heater off.\n", xbee);
      } else {
	heater_on();
	fputs("Heater on.\n", xbee);
      }
      pump_on();
      fputs("Pump on.\n", xbee);
    } else {
      fputs("Heat pump deactivated\n", xbee);
      heater_off();
      pump_off();
    }

    engine_wait_ms(POLLING_PERIOD);
  }
}
