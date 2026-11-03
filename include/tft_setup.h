#pragma once

// Preserve the effective setup from the original build's Setup21_ILI9488.h.
#define USER_SETUP_LOADED
#define USER_SETUP_INFO "Zalogovniki ILI9488"
#define ILI9488_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
#define SPI_FREQUENCY 27000000
#define SPI_READ_FREQUENCY 16000000
#define SPI_TOUCH_FREQUENCY 2500000

// OneWire 2.3.7 relies on this register declaration with Arduino-ESP32 3.x.
#if defined(ARDUINO_ARCH_ESP32)
#include <soc/gpio_struct.h>
#endif
