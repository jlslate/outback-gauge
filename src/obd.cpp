#include "obd.h"

#include <Arduino.h>

#include "config.h"

#ifndef OBD_SIMULATE
#define OBD_SIMULATE 0
#endif

#if !OBD_SIMULATE
#include <BLEDevice.h>
#include <freertos/stream_buffer.h>
#endif

namespace {

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Telemetry shared;
volatile Focus focus = Focus::Boost;

template <typename F>
void edit(F f) {
  taskENTER_CRITICAL(&mux);
  f(shared);
  taskEXIT_CRITICAL(&mux);
}

void setState(ObdState s, const char *adapter = nullptr) {
  edit([&](Telemetry &t) {
    t.state = s;
    if (adapter) strlcpy(t.adapter, adapter, sizeof t.adapter);
  });
}

float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }

#if OBD_SIMULATE

// Plausible bench data: throttle swings through vacuum into boost, coolant
// warms up from cold, voltage sits at charging level.
void simulate() {
  setState(ObdState::Simulated, "Simulator");
  const uint32_t t0 = millis();
  for (;;) {
    const uint32_t now = millis();
    const float t = (now - t0) / 1000.0f;
    const float throttle = constrain(0.5f + 0.6f * sinf(t * 0.7f) * sinf(t * 0.23f), 0.0f, 1.0f);
    edit([&](Telemetry &s) {
      s.boostPsi = min(-11.5f + throttle * 29.0f + random(-20, 20) / 100.0f, 17.8f);
      s.coolantF = min(120.0f + t * 0.8f, 205.0f) + 2.0f * sinf(t * 0.1f);
      s.intakeF = 88.0f + 18.0f * throttle;
      s.volts = 14.1f + 0.1f * sinf(t * 1.3f);
      s.rpm = 800.0f + throttle * 4500.0f;
      s.boostAt = s.coolantAt = s.intakeAt = s.voltsAt = s.rpmAt = now;
    });
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

#else

// ---- BLE link to an ELM327-compatible adapter ------------------------------

BLEClient *client = nullptr;
BLERemoteCharacteristic *txChar = nullptr;
bool txNeedsResponse = false;
StreamBufferHandle_t rx = nullptr;
bool countSuffix = true;  // "010B1": tell the ELM to stop after one reply

bool nameMatches(String name) {
  name.toLowerCase();
  for (const char *hint : OBD_NAME_HINTS)
    if (name.indexOf(hint) >= 0) return true;
  return false;
}

bool isStandardService(BLEUUID uuid) {
  static const uint16_t ids[] = {0x1800, 0x1801, 0x180A, 0x180F};  // GAP, GATT, device info, battery
  for (uint16_t id : ids)
    if (uuid.equals(BLEUUID(id))) return true;
  return false;
}

bool scanFor(BLEAdvertisedDevice &out) {
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  BLEScanResults *res = scan->start(4, false);
  bool found = false;
  int bestRssi = -1000;
  for (int i = 0; res && i < res->getCount(); i++) {
    BLEAdvertisedDevice d = res->getDevice(i);
    if (d.getName().isEmpty() || !nameMatches(d.getName())) continue;
    if (d.getRSSI() > bestRssi) {
      bestRssi = d.getRSSI();
      out = d;
      found = true;
    }
  }
  scan->clearResults();
  return found;
}

// Adapters expose a serial-style service: one characteristic we write
// commands to and one that notifies replies (sometimes the same one). UUIDs
// differ by brand, so take the first non-standard service that has both.
bool connectTo(BLEAdvertisedDevice &dev) {
  if (!client) client = BLEDevice::createClient();
  if (!client->connect(&dev)) return false;

  txChar = nullptr;
  BLERemoteCharacteristic *rxChar = nullptr;
  for (auto &svc : *client->getServices()) {
    if (isStandardService(svc.second->getUUID())) continue;
    BLERemoteCharacteristic *n = nullptr, *w = nullptr;
    for (auto &ch : *svc.second->getCharacteristics()) {
      BLERemoteCharacteristic *c = ch.second;
      if (!n && c->canNotify()) n = c;
      if (!w && (c->canWrite() || c->canWriteNoResponse())) w = c;
    }
    if (n && w) {
      Serial.printf("[OBD] using service %s\n", svc.second->getUUID().toString().c_str());
      rxChar = n;
      txChar = w;
      break;
    }
  }
  if (!txChar) {
    Serial.println("[OBD] no serial-style service found");
    client->disconnect();
    return false;
  }
  txNeedsResponse = !txChar->canWriteNoResponse();
  rxChar->registerForNotify([](BLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
    xStreamBufferSend(rx, data, len, 0);
  });
  return true;
}

bool linkUp() { return client && client->isConnected(); }

// Sends one command and collects the reply up to the ELM's '>' prompt.
// The reply comes back uppercased with whitespace and "SEARCHING..." removed.
bool command(const char *cmd, String &resp, uint32_t timeoutMs) {
  resp = "";
  if (!linkUp() || !txChar) return false;
  xStreamBufferReset(rx);
  char line[24];
  const int len = snprintf(line, sizeof line, "%s\r", cmd);
  txChar->writeValue((uint8_t *)line, len, txNeedsResponse);

  String raw;
  const uint32_t start = millis();
  char buf[64];
  while (millis() - start < timeoutMs) {
    const size_t n = xStreamBufferReceive(rx, buf, sizeof buf, pdMS_TO_TICKS(10));
    for (size_t i = 0; i < n; i++) {
      if (buf[i] != '>') {
        raw += buf[i];
        continue;
      }
      raw.replace("SEARCHING...", "");
      for (unsigned j = 0; j < raw.length(); j++)
        if (!isspace((unsigned char)raw[j])) resp += (char)toupper(raw[j]);
      return true;
    }
    if (!linkUp()) return false;
  }
  return false;
}

// Mode 01 request; copies `count` data bytes following "41<pid>".
bool readPid(uint8_t pid, uint8_t *out, int count) {
  char cmd[8];
  snprintf(cmd, sizeof cmd, countSuffix ? "01%02X1" : "01%02X", pid);
  String r;
  if (!command(cmd, r, 600)) return false;
  if (r == "?" && countSuffix) {
    countSuffix = false;  // older clone that doesn't take the reply count
    return readPid(pid, out, count);
  }
  char key[5];
  snprintf(key, sizeof key, "41%02X", pid);
  const int at = r.indexOf(key);
  if (at < 0 || (int)r.length() < at + 4 + count * 2) return false;
  for (int i = 0; i < count; i++) out[i] = strtol(r.substring(at + 4 + i * 2, at + 6 + i * 2).c_str(), nullptr, 16);
  return true;
}

bool initAdapter() {
  String r;
  command("ATZ", r, 3000);
  Serial.printf("[OBD] adapter: %s\n", r.c_str());
  static const char *const setup[] = {
      "ATE0",   // echo off
      "ATL0",   // no linefeeds
      "ATS0",   // no spaces
      "ATH0",   // no headers
      "ATAT1",  // adaptive timing
      "ATSP6",  // ISO 15765-4 CAN 11-bit 500k, what a 2025 Outback uses
  };
  for (const char *c : setup)
    if (!command(c, r, 1000)) return false;
  return true;
}

bool ecuResponds(uint32_t timeoutMs) {
  String r;
  return command("0100", r, timeoutMs) && r.indexOf("4100") >= 0;
}

// ---- polling ---------------------------------------------------------------

enum class Item : uint8_t { Map, Coolant, Intake, Volts, Rpm };

float baroKpa = 101.3f;

bool poll(Item item) {
  uint8_t b[2];
  const uint32_t now = millis();
  switch (item) {
    case Item::Map:
      if (!readPid(0x0B, b, 1)) return false;
      edit([&](Telemetry &t) { t.boostPsi = (b[0] - baroKpa) * 0.1450377f, t.boostAt = now; });
      return true;
    case Item::Coolant:
      if (!readPid(0x05, b, 1)) return false;
      edit([&](Telemetry &t) { t.coolantF = cToF(b[0] - 40), t.coolantAt = now; });
      return true;
    case Item::Intake:
      if (!readPid(0x0F, b, 1)) return false;
      edit([&](Telemetry &t) { t.intakeF = cToF(b[0] - 40), t.intakeAt = now; });
      return true;
    case Item::Rpm:
      if (!readPid(0x0C, b, 2)) return false;
      edit([&](Telemetry &t) { t.rpm = (b[0] * 256 + b[1]) / 4.0f, t.rpmAt = now; });
      return true;
    case Item::Volts: {
      String r;
      if (!command("ATRV", r, 600)) return false;
      const float v = r.toFloat();  // "12.6V"
      if (v <= 0) return false;
      edit([&](Telemetry &t) { t.volts = v, t.voltsAt = now; });
      return true;
    }
  }
  return false;
}

bool readBaro() {
  uint8_t b;
  if (!readPid(0x33, &b, 1)) return false;
  baroKpa = b;
  return true;
}

bool focusItem(Focus f, Item &out) {
  switch (f) {
    case Focus::Boost: out = Item::Map; return true;
    case Focus::Coolant: out = Item::Coolant; return true;
    case Focus::Intake: out = Item::Intake; return true;
    case Focus::Volts: out = Item::Volts; return true;
    default: return false;
  }
}

// Runs until the BLE link drops.
void runSession() {
  bool ecu = ecuResponds(5000);
  if (!ecu) {
    String r;
    command("ATSP0", r, 1000);  // fall back to automatic protocol search
    ecu = ecuResponds(10000);
  }

  static const Item slowItems[] = {Item::Coolant, Item::Intake, Item::Volts, Item::Rpm};
  size_t slowIdx = 0;
  uint32_t lastSlow = 0, lastBaro = 0;
  int failures = 0;

  while (linkUp()) {
    if (!ecu) {
      setState(ObdState::NoEcu);
      poll(Item::Volts);  // the adapter can still read battery voltage
      vTaskDelay(pdMS_TO_TICKS(3000));
      ecu = ecuResponds(3000);
      continue;
    }
    setState(ObdState::Live);

    if (!lastBaro || millis() - lastBaro > 30000) {
      if (readBaro()) lastBaro = millis();
    }

    Item fast;
    const bool haveFast = focusItem(focus, fast);
    bool ok = true;
    if (haveFast) ok = poll(fast);

    if (!haveFast || millis() - lastSlow >= 400) {
      lastSlow = millis();
      Item next = slowItems[slowIdx++ % (sizeof slowItems / sizeof slowItems[0])];
      if (!haveFast || next != fast) ok = poll(next) && ok;
    }

    failures = ok ? 0 : failures + 1;
    if (failures >= 5) {
      Serial.println("[OBD] ECU stopped answering");
      ecu = false;
      failures = 0;
    }
    vTaskDelay(1);
  }
}

#endif  // OBD_SIMULATE

void obdTask(void *) {
#if OBD_SIMULATE
  simulate();
#else
  BLEDevice::init("OutbackGauge");
  rx = xStreamBufferCreate(1024, 1);
  for (;;) {
    setState(ObdState::Scanning, "");
    BLEAdvertisedDevice dev;
    if (!scanFor(dev)) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    Serial.printf("[OBD] found %s (%s)\n", dev.getName().c_str(), dev.getAddress().toString().c_str());
    setState(ObdState::Connecting, dev.getName().c_str());
    if (!connectTo(dev)) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    setState(ObdState::Initializing);
    if (initAdapter()) runSession();
    if (linkUp()) client->disconnect();
    Serial.println("[OBD] link closed");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
#endif
}

}  // namespace

void obd_start() {
  xTaskCreatePinnedToCore(obdTask, "obd", 10240, nullptr, 1, nullptr, 0);
}

Telemetry obd_snapshot() {
  taskENTER_CRITICAL(&mux);
  Telemetry copy = shared;
  taskEXIT_CRITICAL(&mux);
  return copy;
}

void obd_setFocus(Focus f) {
  focus = f;
}
