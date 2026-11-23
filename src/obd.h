#pragma once

#include <stdint.h>

enum class ObdState : uint8_t {
  Scanning,      // looking for the adapter over BLE
  Connecting,
  Initializing,  // sending AT setup commands
  NoEcu,         // adapter is up but the car isn't answering (ignition off)
  Live,
  Simulated,
};

// Which reading to poll as fast as possible; the rest are polled in rotation.
enum class Focus : uint8_t { Boost, Coolant, Oil, Intake, Volts, Speed, Rpm, Fuel, Economy, None };

// Timestamps are millis() of the last good reading, 0 if never read.
struct Telemetry {
  ObdState state = ObdState::Scanning;
  char adapter[24] = "";
  float boostPsi = 0;  // manifold pressure minus barometric; negative = vacuum
  float coolantF = 0;
  float oilF = 0;
  float intakeF = 0;
  float volts = 0;     // measured by the adapter at the OBD port
  float rpm = 0;
  float speedMph = 0;
  float fuelPct = 0;   // tank level, smoothed
  float fuelLph = 0;      // fuel burned, litres per hour (from the fuel rate, or worked out from air flow)
  bool economyOk = true;  // false once the car turns out to report neither
  float altitudeFt = 0;   // worked out from the barometric pressure, which the car updates slowly
  uint32_t boostAt = 0, coolantAt = 0, oilAt = 0, intakeAt = 0, voltsAt = 0, rpmAt = 0, speedAt = 0, fuelAt = 0, altitudeAt = 0, fuelRateAt = 0;
};

void obd_start();  // runs the BLE/ELM327 client on its own task (core 0)
Telemetry obd_snapshot();
void obd_setFocus(Focus f);
