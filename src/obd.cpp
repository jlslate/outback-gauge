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
      s.oilF = min(110.0f + t * 0.5f, 215.0f) + 1.5f * sinf(t * 0.08f);
      s.intakeF = 88.0f + 18.0f * throttle;
      s.volts = 14.1f + 0.1f * sinf(t * 1.3f);
      s.rpm = 800.0f + throttle * 4500.0f;
      s.boostAt = s.coolantAt = s.oilAt = s.intakeAt = s.voltsAt = s.rpmAt = now;
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
volatile uint32_t notifyCount = 0;  // notifications received, for the log
bool countSuffix = true;  // "010B1": tell the ELM to stop after one reply

#if OBD_BONDING
// Pairing is just-works (the adapter has no display or keypad); these only log
// how it went so a failed bond shows up in the serial output.
volatile int pairResult = 0;  // 0 pending, 1 bonded, -1 failed

class PairingLog : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    Serial.println("[OBD] adapter asked for a passkey, answering 000000");
    return 0;
  }
  void onPassKeyNotify(uint32_t pass) override { Serial.printf("[OBD] passkey %06u\n", (unsigned)pass); }
  bool onConfirmPIN(uint32_t pass) override {
    Serial.printf("[OBD] confirm pin %06u: yes\n", (unsigned)pass);
    return true;
  }
  bool onSecurityRequest() override {
    Serial.println("[OBD] adapter sent a security request");
    return true;
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t c) override {
    pairResult = c.success ? 1 : -1;
    Serial.printf("[OBD] pairing %s (reason 0x%02x)\n", c.success ? "ok" : "FAILED", c.fail_reason);
  }
};
#endif

bool nameMatches(String name) {
  name.toLowerCase();
  for (const char *hint : OBD_NAME_HINTS)
    if (name.indexOf(hint) >= 0) return true;
  return false;
}

bool isStandardService(BLEUUID uuid) {
  // GAP, GATT, device info, battery, and 0xFEF5 (Dialog's firmware-update
  // service, which the OBDLink CX exposes alongside its serial one)
  static const uint16_t ids[] = {0x1800, 0x1801, 0x180A, 0x180F, 0xFEF5};
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
    Serial.printf("[OBD] service %s\n", svc.second->getUUID().toString().c_str());
    if (isStandardService(svc.second->getUUID())) continue;
    BLERemoteCharacteristic *n = nullptr, *w = nullptr;
    for (auto &ch : *svc.second->getCharacteristics()) {
      BLERemoteCharacteristic *c = ch.second;
      Serial.printf("[OBD]   char %s handle %u notify=%d write=%d writeNR=%d cccd=%d\n", c->getUUID().toString().c_str(),
                    c->getHandle(), c->canNotify(), c->canWrite(), c->canWriteNoResponse(),
                    c->getDescriptor(BLEUUID((uint16_t)0x2902)) != nullptr);
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
#if OBD_BONDING
  // The CX refuses to enable notifications on an unencrypted link ("encryption
  // insufficient"), and asking only at connect time gets dropped, so start the
  // bond ourselves and wait for it before subscribing.
  pairResult = 0;
  esp_err_t er = esp_ble_set_encryption(*client->getPeerAddress().getNative(), ESP_BLE_SEC_ENCRYPT);
  Serial.printf("[OBD] requested encryption (rc=%d)\n", (int)er);
  for (int i = 0; i < 100 && pairResult == 0 && client->isConnected(); i++) delay(100);
  Serial.printf("[OBD] pairing state %d\n", (int)pairResult);
#endif
  txNeedsResponse = txChar->canWrite();  // acknowledged writes where offered; some adapters drop the unacknowledged kind
  rxChar->registerForNotify([](BLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
    notifyCount++;
    xStreamBufferSend(rx, data, len, 0);
  });
  if (!rxChar->getDescriptor(BLEUUID((uint16_t)0x2902))) {
    // The library couldn't read this characteristic's descriptors, so it never
    // turned notifications on. The CCCD sits right after the value handle.
    Serial.println("[OBD] no CCCD found, enabling notifications by handle");
    uint8_t on[2] = {0x01, 0x00};
    esp_ble_gattc_write_char_descr(client->getGattcIf(), client->getConnId(), rxChar->getHandle() + 1, sizeof on, on,
                                   ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    delay(300);
  }
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
    if (!linkUp()) {
      Serial.printf("[OBD] link dropped waiting for %s (got \"%s\")\n", cmd, raw.c_str());
      return false;
    }
  }
  Serial.printf("[OBD] no reply to %s in %u ms (got \"%s\", %u notifications so far)\n", cmd, (unsigned)timeoutMs,
                raw.c_str(), (unsigned)notifyCount);
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
  if (at < 0 || (int)r.length() < at + 4 + count * 2) {
    Serial.printf("[OBD] %s -> \"%s\" (not the %s reply expected)\n", cmd, r.c_str(), key);
    return false;
  }
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
      "ATAT0",  // fixed timing: adaptive tunes itself to the quickest module on the bus
      "ATST7D", // and wait up to ~500 ms for the engine ECU
      "ATSP6",  // ISO 15765-4 CAN 11-bit 500k, what a 2025 Outback uses
      "ATSH7E0",   // ask the engine ECU only...
      "ATCRA7E8",  // ...and listen only to its reply, not the transmission's
  };
  for (const char *c : setup) {
    if (!command(c, r, 1000)) {
      Serial.printf("[OBD] setup failed at %s\n", c);
      return false;
    }
  }
  Serial.println("[OBD] adapter setup ok");
  return true;
}

bool ecuResponds(uint32_t timeoutMs) {
  String r;
  const bool ok = command("0100", r, timeoutMs) && r.indexOf("4100") >= 0;
  Serial.printf("[OBD] 0100 -> \"%s\" (%s)\n", r.c_str(), ok ? "ECU up" : "no ECU");
  return ok;
}

// ---- polling ---------------------------------------------------------------

enum class Item : uint8_t { Map, Coolant, Oil, Intake, Volts, Rpm };

float baroKpa = 101.3f;

// Which mode 01 PIDs the engine ECU says it supports. Until discovery has run
// (or if it fails) everything counts as supported, so polling still works.
bool pidKnown[256];
bool supportKnown = false;

bool pidSupported(uint8_t pid) { return !supportKnown || pidKnown[pid]; }

bool itemSupported(Item item) {
  switch (item) {
    case Item::Map: return pidSupported(0x0B) || pidSupported(0x87);
    case Item::Coolant: return pidSupported(0x05);
    case Item::Oil: return pidSupported(0x5C);
    case Item::Intake: return pidSupported(0x0F) || pidSupported(0x68);
    case Item::Rpm: return pidSupported(0x0C);
    case Item::Volts: return true;  // the adapter measures this itself
  }
  return true;
}

// Reads the 32-bit "supported PIDs" bitmap that starts at `base` (00, 20, 40...).
bool readSupport(uint8_t base, uint32_t &bits) {
  char cmd[8];
  snprintf(cmd, sizeof cmd, countSuffix ? "01%02X1" : "01%02X", base);
  String r;
  if (!command(cmd, r, 1500)) return false;
  char key[5];
  snprintf(key, sizeof key, "41%02X", base);
  const int at = r.indexOf(key);
  if (at < 0 || (int)r.length() < at + 12) return false;
  bits = strtoul(r.substring(at + 4, at + 12).c_str(), nullptr, 16);
  return true;
}

void discoverPids() {
  memset(pidKnown, 0, sizeof pidKnown);
  String list;
  bool any = false;
  for (int base = 0; base <= 0xC0; base += 0x20) {
    uint32_t bits;
    if (!readSupport(base, bits)) break;
    any = true;
    for (int i = 0; i < 32; i++) {
      if (!(bits & (1u << (31 - i)))) continue;
      const int pid = base + 1 + i;
      pidKnown[pid] = true;
      char one[6];
      snprintf(one, sizeof one, "%02X ", pid);
      list += one;
    }
    if (!pidKnown[base + 0x20]) break;  // no further range
  }
  supportKnown = any;
  Serial.printf("[OBD] ECU supports PIDs: %s\n", any ? list.c_str() : "(could not read)");
}

bool poll(Item item) {
  if (!itemSupported(item)) return true;  // nothing to ask; not a failure
  uint8_t b[5];
  const uint32_t now = millis();
  switch (item) {
    case Item::Map: {
      float kpa;
      if (pidSupported(0x0B)) {
        if (!readPid(0x0B, b, 1)) return false;
        kpa = b[0];
      } else {
        // The 2025 Outback's ECU has no 0B. It reports manifold pressure in 87:
        // a support byte, then sensor A in 1/32 kPa.
        if (!readPid(0x87, b, 5)) return false;
        kpa = ((b[1] << 8) | b[2]) / 32.0f;
      }
      edit([&](Telemetry &t) { t.boostPsi = (kpa - baroKpa) * 0.1450377f, t.boostAt = now; });
      return true;
    }
    case Item::Coolant:
      if (!readPid(0x05, b, 1)) return false;
      edit([&](Telemetry &t) { t.coolantF = cToF(b[0] - 40), t.coolantAt = now; });
      return true;
    case Item::Oil:
      if (!readPid(0x5C, b, 1)) return false;
      edit([&](Telemetry &t) { t.oilF = cToF(b[0] - 40), t.oilAt = now; });
      return true;
    case Item::Intake: {
      float c;
      if (pidSupported(0x0F)) {
        if (!readPid(0x0F, b, 1)) return false;
        c = b[0] - 40;
      } else {
        // No 0F either; 68 carries up to two intake air sensors after a support byte.
        if (!readPid(0x68, b, 3)) return false;
        c = b[1] - 40;
      }
      edit([&](Telemetry &t) { t.intakeF = cToF(c), t.intakeAt = now; });
      return true;
    }
    case Item::Rpm:
      if (!readPid(0x0C, b, 2)) return false;
      edit([&](Telemetry &t) { t.rpm = (b[0] * 256 + b[1]) / 4.0f, t.rpmAt = now; });
      return true;
    case Item::Volts: {
      String r;
      if (!command("ATRV", r, 600)) return false;
      const float v = r.toFloat();  // "12.6V"
      if (v <= 0) {
        Serial.printf("[OBD] ATRV -> \"%s\"\n", r.c_str());
        return false;
      }
      edit([&](Telemetry &t) { t.volts = v, t.voltsAt = now; });
      return true;
    }
  }
  return false;
}

bool readBaro() {
  if (!pidSupported(0x33)) return true;  // keep the standard-atmosphere default
  uint8_t b;
  if (!readPid(0x33, &b, 1)) return false;
  baroKpa = b;
  return true;
}

bool focusItem(Focus f, Item &out) {
  switch (f) {
    case Focus::Boost: out = Item::Map; return true;
    case Focus::Coolant: out = Item::Coolant; return true;
    case Focus::Oil: out = Item::Oil; return true;
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
    command("ATSH7E0", r, 1000);
    command("ATCRA7E8", r, 1000);
    ecu = ecuResponds(10000);
  }
  if (ecu) discoverPids();

  static const Item slowItems[] = {Item::Coolant, Item::Oil, Item::Intake, Item::Volts, Item::Rpm};
  size_t slowIdx = 0;
  uint32_t lastSlow = 0, lastBaro = 0;
  int failures = 0;

  while (linkUp()) {
    if (!ecu) {
      setState(ObdState::NoEcu);
      poll(Item::Volts);  // the adapter can still read battery voltage
      vTaskDelay(pdMS_TO_TICKS(3000));
      // Protocol search takes several seconds; a shorter wait aborts it every time.
      ecu = ecuResponds(10000);
      if (ecu) discoverPids();
      continue;
    }
    setState(ObdState::Live);

    if (!lastBaro || millis() - lastBaro > 30000) {
      if (readBaro()) lastBaro = millis();
    }

    Item fast;
    const bool haveFast = focusItem(focus, fast);
    bool ok = true, asked = false;
    if (haveFast && itemSupported(fast)) ok = poll(fast), asked = true;

    if (!haveFast || millis() - lastSlow >= 400) {
      lastSlow = millis();
      Item next = slowItems[slowIdx++ % (sizeof slowItems / sizeof slowItems[0])];
      if ((!haveFast || next != fast) && itemSupported(next)) ok = poll(next) && ok, asked = true;
    }

    // Only requests actually sent count; skipped (unsupported) items say nothing about the ECU.
    if (asked) failures = ok ? 0 : failures + 1;
    else vTaskDelay(pdMS_TO_TICKS(20));
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
#if OBD_BONDING
  // Bond on connect: some adapters (the OBDLink CX included, as far as we know)
  // accept the link but ignore commands until it is encrypted.
  BLEDevice::setSecurityCallbacks(new PairingLog());
  auto *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_BOND);
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#endif
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
