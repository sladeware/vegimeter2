# -*- mode: python; -*-
#
# Copyright (c) 2013 Sladeware LLC.

from vegimeter2 import vegimeter2

propeller_binary(name="vegimeter2",
                 srcs=["src/engine.c",
                       "../bbos/src/main/c/bb/os/drivers/onewire/slaves/ds18b20.c",
                       "../bbos/src/main/c/bb/os/drivers/onewire/onewire_bus.c",
                       vegimeter2])

propeller_load(name="vegimeter2-load",
               binary="vegimeter2",
               deps=[":vegimeter2"],
               baudrate=115200,
               terminal_mode=False,
               port="/dev/ttyUSB0",
               eeprom=True)
