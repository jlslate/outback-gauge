#include "board.h"

#include <Wire.h>

namespace {

constexpr uint8_t TCA_REG_OUTPUT = 0x01;
constexpr uint8_t TCA_REG_CONFIG = 0x03;  // 1 = input

int tcaAddr = -1;
uint8_t tcaOut = 0xFF;

bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

void setExio(uint8_t pin, bool high) {
  if (tcaAddr < 0) return;
  if (high) tcaOut |= (1 << pin);
  else tcaOut &= ~(1 << pin);
  writeReg(tcaAddr, TCA_REG_OUTPUT, tcaOut);
}

}  // namespace

void board_init() {
  // Latch power first: on battery, the board shuts off as soon as PWR is
  // released unless this pin is held high.
  pinMode(PIN_BAT_CONTROL, OUTPUT);
  digitalWrite(PIN_BAT_CONTROL, HIGH);

  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  // TCA9554 (not the A variant) answers somewhere in 0x20-0x27; nothing else
  // on this bus uses that range.
  for (uint8_t a = 0x20; a <= 0x27; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      tcaAddr = a;
      break;
    }
  }

  if (tcaAddr >= 0) {
    Serial.printf("[BOARD] TCA9554 at 0x%02X\n", tcaAddr);
    // Drive outputs high before switching them to outputs so nothing glitches low.
    writeReg(tcaAddr, TCA_REG_OUTPUT, tcaOut);
    writeReg(tcaAddr, TCA_REG_CONFIG, 0xFF & ~((1 << EXIO_TP_RST) | (1 << EXIO_LCD_RST) | (1 << EXIO_SD_CS)));
    setExio(EXIO_LCD_RST, false);
    setExio(EXIO_TP_RST, false);
    delay(20);
    setExio(EXIO_LCD_RST, true);
    setExio(EXIO_TP_RST, true);
    delay(120);
  } else {
    Serial.println("[BOARD] TCA9554 not found; panel reset skipped");
  }

  ledcAttach(PIN_LCD_BL, 20000, 8);
  board_setBacklight(0);
}

void board_setBacklight(uint8_t percent) {
  ledcWrite(PIN_LCD_BL, (uint32_t)min<uint8_t>(percent, 100) * 255 / 100);
}

float board_batteryVolts() {
  return analogReadMilliVolts(PIN_BAT_ADC) * BAT_DIVIDER / 1000.0f;
}
