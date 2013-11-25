/*
* Copyright (c) 2013 Sladeware LLC.
*/
#ifndef __VEGIMETER2_PINS_H
#define __VEGIMETER2_PINS_H

/* Update bits of DIRA register. */
#define propeller_set_dira_bit(bits, v) DIRA = (DIRA &~ (bits)) | (v)
#define propeller_set_dira_bits(bits) DIRA |= (bits)
#define propeller_clr_dira_bits(bits) DIRA &= ~(bits)
/* Update bits of OUTA register. */
#define propeller_set_outa_bit(bit, v) OUTA = (OUTA &~ (bit)) | (v)
#define propeller_set_outa_bits(bits) OUTA |= (bits)
#define propeller_clr_outa_bits(bits) OUTA &= ~(bits)
#define propeller_get_ina_bits() INA

/* Pins start at 0. There are 32 Pins. P0 - P31. */
#define GET_MASK(pin) (1UL << (pin))

#define GET_INPUT(pin) ((propeller_get_ina_bits() >> (pin)) & 1)
#define DIR_OUTPUT(pin) (propeller_set_dira_bits(GET_MASK(pin)))
#define DIR_INPUT(pin) (propeller_clr_dira_bits(GET_MASK(pin)))
#define DIR_TO(pin, state) (propeller_set_dira_bit(GET_MASK(pin), (state)<<(pin)))
#define OUT_LOW(pin) (propeller_clr_outa_bits(GET_MASK(pin)))
#define OUT_HIGH(pin) (propeller_set_outa_bits(GET_MASK(pin)))
#define OUT_TO(pin, state) (propeller_set_outa_bit(GET_MASK(pin), (state)<<(pin)))

#define GET_INPUT_MASK(mask) (propeller_get_ina_bits() & (unsigned)mask)
#define DIR_OUTPUT_MASK(mask) (propeller_set_dira_bits((unsigned)mask))
#define DIR_INPUT_MASK(mask) (propeller_clr_dira_bits((unsigned)mask))
#define OUT_LOW_MASK(mask) (propeller_clr_outa_bits((unsigned)mask))
#define OUT_HIGH_MASK(mask) (propeller_set_outa_bits((unsigned)mask))

#endif /* __VEGIMETER2_PINS_H */
