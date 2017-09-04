#pragma once

#include "Arduino.h"

const uint8_t encoder_switch = 21, encoder_1 = 5, encoder_2 = 18;

const byte spi_clk = 2, spi_miso = 4, spi_mosi = 16, sd_spi_ss = 17;

const byte lcd_pins[] = {25, 26, 32, 33};
const byte lcd_clk = 27;
const byte lcd_rs = 14;