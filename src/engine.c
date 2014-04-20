/*
 * Vegimeter 2 Engine
 *
 * Copyright (c) 2013 Sladeware LLC
 */

#include <bb/os.h>
#include <propeller.h>
#include <stdio.h>
#include "bb/os/drivers/onewire/onewire_bus.h"
#include "pins.h"

#define HEAT_PUMP_ACTIVATION 2100 // Centi-Celsius ;)
#define HEATER 15
#define HEATER_DEACTIVATION 4200 // Centi-Celsius ;)
#define POLLING_PERIOD 60000 // Milliseconds
#define PUMP 26
#define STR_SIZE 64
#define TEMP_SIZE 16

/* Max values */
#define MAX_HEATER_PERIODS 80 // One hour with 60s polling periods
#define MAX_AIR_TEMP 5000 // Centi-Celcius ;)

/* Error codes for system halt conditions */
#define ERROR_HIGH_AIR_TEMP 3
#define ERROR_BAD_TEMP 2
#define ERROR_MAX_HEAT 1

/* Read scratch pad. */
#define DS18B20_READ_SCRATCHPAD 0xBE
/* Write scratch pad. */
#define DS18B20_WRITE_SCRATCHPAD 0x4E
/* Start temperature conversion. */
#define DS18B20_CONVERT_TEMPERATURE 0x44
/* Read power status. */
#define DS18B20_READ_POWER 0xB4
/* Scratch pad size in bytes. */
#define DS18B20_SCRATCHPAD_SIZE 9

#define DEFAULT_TEMP_READING 54321
#define DS18B20_1_100TH_CELCIUS(value) (100 * (value))

HUBDATA char is_initialized = 0;
HUBDATA char str[STR_SIZE];
HUBDATA char temp[TEMP_SIZE];
HUBDATA int water_temp = 0, soil_temp = 0, soil_a = 0, soil_b = 0, soil_c = 0;
HUBDATA int soil_d = 0, water_a = 0, water_b = 0, air_temp = 0;
HUBDATA FILE* xbee;
HUBDATA char digit[] = "0123456789";
HUBDATA char* p;
HUBDATA int8_t led = 0;
HUBDATA int8_t heater_periods = 0;
HUBDATA int8_t halt = 0;

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

int get_temp(int16_t pin) {
  uint8_t i;
  uint8_t sp[DS18B20_SCRATCHPAD_SIZE];
  int sign;
  int temp_data;

  ow_reset(pin);
  /* Start measurements... */
  if (ow_reset(pin)) {
    fputs("Failed to reset 1-wire BUS before reading sensor's ROM.\r\n", xbee);
    return DEFAULT_TEMP_READING;
  }
  /*
   * Now we need to read the state from the input pin to define
   * whether the bus is "idle".
   */
  if (!ow_input_pin_state(pin)) {
    return DEFAULT_TEMP_READING;
  }
  ow_command(DS18B20_CONVERT_TEMPERATURE, pin);
  if (ow_reset(pin)) {
    fputs("Failed to reset 1-wire BUS before reading sensor's ROM.\r\n", xbee);
    return DEFAULT_TEMP_READING;
  }
  ow_command(DS18B20_READ_SCRATCHPAD, pin);
  for (i=0; i<DS18B20_SCRATCHPAD_SIZE; i++) {
    sp[i] = ow_read_byte(pin);
  }
  /* Process measurements... */
  sign = sp[1] & 0xF0 ? -1 : 1; /* sign */
  temp_data = ((unsigned)(sp[1] & 0x07) << 8) | sp[0];
  return DS18B20_1_100TH_CELCIUS((temp_data & 0xFFFF) * sign) >> 4;
}

void pump_init() {
  DIR_OUTPUT(PUMP);
}

unsigned int pump_on() {
  return OUT_HIGH(PUMP);
}

unsigned int pump_off() {
  return OUT_LOW(PUMP);
}

void heater_init() {
  DIR_OUTPUT(HEATER);
}

unsigned int heater_on() {
  heater_periods++;
  return OUT_HIGH(HEATER);
}

unsigned int heater_off() {
  heater_periods = 0;
  return OUT_LOW(HEATER);
}

void validate_temp(int temp) {
  if (temp == DEFAULT_TEMP_READING) {
    fputs("Bad temperature reading. Error. Halting.", xbee);
    halt = ERROR_BAD_TEMP;
  }
}

int get_soil_a_temp() {
  int t = get_temp(10);
  validate_temp(t);
  return t;
}

int get_soil_b_temp() {
  int t = get_temp(13);
  validate_temp(t);
  return t;  
}

int get_soil_c_temp() {
  int t = get_temp(14);
  validate_temp(t);
  return t;
}

int get_soil_d_temp() {
  int t = get_temp(12);
  validate_temp(t);
  return t;
}

int get_water_a_temp() {
  int t = get_temp(11);
  validate_temp(t);
  return t;
}

int get_water_b_temp() {
  int t = get_temp(9);
  validate_temp(t);
  return t;
}

int get_air_temp() {
  int t = get_temp(8);
  validate_temp(t);
  return t;
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

void led_init() {
  DIR_OUTPUT(23);
  DIR_OUTPUT(22);
  DIR_OUTPUT(21);
  DIR_OUTPUT(20);
  DIR_OUTPUT(19);
  DIR_OUTPUT(18);
  DIR_OUTPUT(17);
  DIR_OUTPUT(16);
}

void led_on(int pin) {
  OUT_HIGH(pin);
  led = 1;
}

void led_off(int pin) {
  OUT_LOW(pin);
  led = 0;
}

void error_leds() {
  int i;

  for (i = 16; i <= 23; i++) {
    led_on(i);
  }
}

void strobe_leds() {
  int wait = 200;
  int i;

  for (i = 16; i <= 23; i++) {
    led_on(i);
    engine_wait_ms(wait);
    led_off(i);
  }
}

void blink_led() {
  int pin = 20;

  led_on(pin);
  engine_wait_ms(500);
  led_off(pin);  
}

void engine_xbee_init() {
  xbee = fopen("SSER:9600,24,25", "w"); // p24 out, p25 in
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

    led_init();
    pump_init();
    heater_init();
    engine_xbee_init();

    heater_off();
    pump_off();

    fputs("Engine initialized.\n", xbee);
  }
}

void itoa(int i, char b[]) {
  p = b;
  memset(b, 0, TEMP_SIZE);

  if (i < 0) {
    *p++ = '-';
    i *= -1;
  }

  int shifter = i, ctr=TEMP_SIZE;
  do {
    ++p;
    shifter /= 10;
  } while (shifter && --ctr > 0);
  *p = '\0';

  ctr = TEMP_SIZE;
  do {
    *--p = digit[i % 10];
    i /= 10;
  } while (i && --ctr > 0);
}

void engine_runnerT() {
  int i = -13;

  while (1) {
    i++;
    engine_init();
    blink_led();
    itoa(i, temp);
    fputs(temp, xbee);
    fputs("\n", xbee);
    engine_wait_ms(500);
  }
}

int halt_on_error() {
  if (halt > 0) {
    heater_off();
    pump_off();

    strcpy(str, "System error. Halted. Code: ");
    itoa(halt, temp);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);      
    memset(str, 0, STR_SIZE);

    error_leds();
    engine_wait_ms(POLLING_PERIOD);

    return 1;
  } else {
    return 0;
  }
}

void engine_runner() {
  while (1) {
    if (halt_on_error()) {
      continue;
    }

    engine_init();

    blink_led();

    strcpy(str, "A: ");
    air_temp = get_air_temp();
    itoa(air_temp, temp);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);
    if (air_temp >= MAX_AIR_TEMP) {
      fputs("Max air temperature reached. Error. Halting.\n", xbee);
      halt = ERROR_HIGH_AIR_TEMP;
      heater_off();
      pump_off();
      continue;
    }

    soil_a = get_soil_a_temp();
    soil_b = get_soil_b_temp();
    soil_c = get_soil_c_temp();
    soil_d = get_soil_d_temp();
    soil_temp = soil_a + soil_b + soil_c + soil_d;

    strcpy(str, "S: ");
    itoa(soil_a, temp);
    strcat(str, temp);
    strcat(str, ",");
    itoa(soil_b, temp);
    strcat(str, temp);
    strcat(str, ",");
    itoa(soil_c, temp);
    strcat(str, temp);
    strcat(str, ",");
    itoa(soil_d, temp);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);

    water_a = get_water_a_temp();
    water_b = get_water_b_temp();
    water_temp = water_a + water_b;

    strcpy(str, "W: ");
    itoa(water_a, temp);
    strcat(str, temp);
    strcat(str, ",");
    itoa(water_b, temp);
    strcat(str, temp);
    strcat(str, "\n");
    fputs(str, xbee);
    memset(str, 0, STR_SIZE);

    if (soil_temp < HEAT_PUMP_ACTIVATION * 4) {
      if (water_temp > HEATER_DEACTIVATION * 2 ||
	  water_a >> 1 > water_b ||
	  water_b >> 1 > water_a) {
	heater_off();
	fputs("Heat off.\n", xbee);
      } else {
	heater_on();
	strcpy(str, "Heater On: ");
	itoa(heater_periods, temp);
	strcat(str, temp);
	strcat(str, "\n");
	fputs(str, xbee);
	memset(str, 0, STR_SIZE);
      }
      pump_on();
      fputs("Pump on.\n", xbee);
    } else {
      fputs("Heat pump deactivated\n", xbee);
      heater_off();
      pump_off();
    }

    if (heater_periods > MAX_HEATER_PERIODS) {
      fputs("Max heater periods reached. Error. Halting.\n", xbee);
      halt = ERROR_MAX_HEAT;
      heater_off();
      pump_off();
      continue;
    }

    strobe_leds();
    
    engine_wait_ms(POLLING_PERIOD);
  }
}
