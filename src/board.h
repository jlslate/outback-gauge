#pragma once

#include <Arduino.h>

// Waveshare ESP32-S3-Touch-LCD-1.46B pin map, from the Waveshare wiki pin table
// and schematic (ESP32-S3-Touch-LCD-1.46.pdf).

// Display: SPD2010 over QSPI
constexpr int PIN_LCD_CS = 21;
constexpr int PIN_LCD_SCK = 40;
constexpr int PIN_LCD_D0 = 46;
constexpr int PIN_LCD_D1 = 45;
constexpr int PIN_LCD_D2 = 42;
constexpr int PIN_LCD_D3 = 41;
constexpr int PIN_LCD_BL = 5;
constexpr int LCD_SIZE = 412;

// One I2C bus for the TCA9554 expander, QMI8658 IMU, PCF85063 RTC and touch.
constexpr int PIN_I2C_SDA = 11;
constexpr int PIN_I2C_SCL = 10;

// TCA9554 outputs; the schematic's EXIO1..EXIO8 are expander pins P0..P7.
constexpr uint8_t EXIO_TP_RST = 0;   // EXIO1
constexpr uint8_t EXIO_LCD_RST = 1;  // EXIO2
constexpr uint8_t EXIO_SD_CS = 2;    // EXIO3

// Power
constexpr int PIN_BAT_CONTROL = 7;  // held high to stay on when running from the LiPo
constexpr int PIN_BAT_ADC = 8;      // battery through a 200K/100K divider
constexpr float BAT_DIVIDER = 3.0f;

constexpr int PIN_BOOT_BUTTON = 0;

void board_init();  // power latch, I2C, expander, panel reset, backlight PWM
void board_setBacklight(uint8_t percent);
float board_batteryVolts();
