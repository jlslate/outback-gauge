#include "webconfig.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "settings.h"
#include "webpage.h"

namespace {

const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint32_t WINDOW_MS = WEBCONFIG_MINUTES * 60UL * 1000UL;

WebServer server(80);
DNSServer dns;
TaskHandle_t taskHandle = nullptr;
volatile bool running = false;
volatile bool stopRequested = false;
volatile uint32_t stopAt = 0;

char ssid[32] = WEBCONFIG_SSID;
char password[16] = "";
char url[24] = "";

void keepAlive() { stopAt = millis() + WINDOW_MS; }

// ---- handlers --------------------------------------------------------------

float argFloat(const char *name, float fallback) {
  return server.hasArg(name) ? server.arg(name).toFloat() : fallback;
}

void handleRoot() {
  keepAlive();
  server.send(200, "text/html", webpage_render(settings(), nullptr));
}

void handleSave() {
  keepAlive();
  Settings s = settings();
  s.boostWarnPsi = argFloat("boost", s.boostWarnPsi);
  s.coolantWarnF = argFloat("cool", s.coolantWarnF);
  s.intakeWarnF = argFloat("intake", s.intakeWarnF);
  s.voltsLowWarn = argFloat("vlo", s.voltsLowWarn);
  s.voltsHighWarn = argFloat("vhi", s.voltsHighWarn);
  s.tiltWarnDeg = argFloat("tilt", s.tiltWarnDeg);
  s.backlight = (uint8_t)argFloat("bl", s.backlight);
  // Unchecked boxes aren't posted at all, so presence is the value.
  s.rotate180 = server.hasArg("rot");
  s.touchSwapXY = server.hasArg("swap");
  s.touchInvertX = server.hasArg("invx");
  s.touchInvertY = server.hasArg("invy");
  s.rollSign = server.hasArg("roll") ? -1 : 1;
  s.pitchSign = server.hasArg("pitch") ? -1 : 1;

  settings_stage(s);
  // Rendered from what was just staged, not from settings(): the UI task adopts
  // it a frame or two from now, and re-reading here would show the old values.
  server.send(200, "text/html", webpage_render(s, "Saved."));
}

void handleDefaults() {
  keepAlive();
  const Settings s = settings_defaults();
  settings_stage(s);
  server.send(200, "text/html", webpage_render(s, "Restored the built-in defaults."));
}

void handleReboot() {
  server.send(200, "text/html",
              F("<!doctype html><meta charset=utf-8><body style='background:#111;color:#eee;"
                "font:16px system-ui;padding:2rem'>Rebooting. The Wi-Fi network will disappear; "
                "start it again from the gauge's settings page if you need it.</body>"));
  delay(200);
  ESP.restart();
}

void handleStop() {
  server.send(200, "text/html",
              F("<!doctype html><meta charset=utf-8><body style='background:#111;color:#eee;"
                "font:16px system-ui;padding:2rem'>Wi-Fi off. Press and hold on the gauge's "
                "settings page to bring it back.</body>"));
  delay(200);
  stopRequested = true;
}

// Phones probe a known URL to decide whether they're behind a captive portal;
// sending them to the form makes the settings page pop up on connect.
void handleNotFound() {
  server.sendHeader("Location", String("http://") + AP_IP.toString() + "/");
  server.send(302, "text/plain", "");
}

void task(void *) {
  // Routes live on the server object, which outlives each start/stop cycle;
  // registering them again every time would stack up duplicate handlers.
  static bool routed = false;
  if (!routed) {
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/defaults", HTTP_POST, handleDefaults);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/stop", HTTP_POST, handleStop);
    server.onNotFound(handleNotFound);
    routed = true;
  }
  server.begin();
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", AP_IP);

  while (!stopRequested && (int32_t)(stopAt - millis()) > 0) {
    dns.processNextRequest();
    server.handleClient();
    delay(2);
  }

  dns.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);  // hands the Wi-Fi stack's RAM back to BLE and LVGL
  Serial.println("[WEB] access point stopped");

  running = false;
  taskHandle = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

void webconfig_start() {
  if (running) {
    keepAlive();
    return;
  }

  WiFi.mode(WIFI_AP);
  uint8_t mac[6] = {0};
  WiFi.softAPmacAddress(mac);
  // Per-device but stable, so it can be printed on the gauge and not change.
  snprintf(password, sizeof password, "gauge%02X%02X", mac[4], mac[5]);

  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(ssid, password)) {
    Serial.println("[WEB] access point failed to start");
    WiFi.mode(WIFI_OFF);
    return;
  }
  snprintf(url, sizeof url, "http://%s", AP_IP.toString().c_str());

  stopRequested = false;
  keepAlive();
  running = true;

  // Its own task, not loop(): request handling is a much deeper call path than
  // anything else here, and sharing the loop stack with it is what corrupted
  // the display in sensecap-camera-viewer. It also keeps the UI at 50 Hz while
  // a page loads.
  if (xTaskCreatePinnedToCore(task, "webcfg", 10240, nullptr, 1, &taskHandle, 1) != pdPASS) {
    Serial.println("[WEB] could not start the server task");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    running = false;
    return;
  }
  Serial.printf("[WEB] \"%s\" / %s on %s for %d min\n", ssid, password, url, WEBCONFIG_MINUTES);
}

void webconfig_stop() {
  stopRequested = true;
}

bool webconfig_active() {
  return running;
}

uint32_t webconfig_secondsLeft() {
  if (!running) return 0;
  const int32_t left = (int32_t)(stopAt - millis());
  return left > 0 ? (uint32_t)left / 1000 : 0;
}

const char *webconfig_ssid() { return ssid; }
const char *webconfig_password() { return password; }
const char *webconfig_url() { return url; }
