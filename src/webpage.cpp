#include "webpage.h"

namespace {

void num(String &o, const char *id, const char *label, const char *attrs, float v, int dp, const char *unit) {
  char buf[16];
  snprintf(buf, sizeof buf, "%.*f", dp, v);
  o += "<label><span>";
  o += label;
  o += "</span><input type=number name=";
  o += id;
  o += " ";
  o += attrs;
  o += " value=";
  o += buf;
  o += "><em>";
  o += unit;
  o += "</em></label>";
}

void check(String &o, const char *id, const char *label, bool on) {
  o += "<label class=chk><input type=checkbox name=";
  o += id;
  o += on ? " checked><span>" : "><span>";
  o += label;
  o += "</span></label>";
}

String render(const Settings &s, const char *note) {
  String o;
  o.reserve(6000);
  o += F("<!doctype html><html><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Outback Gauge</title><style>"
         "*{box-sizing:border-box}"
         "body{margin:0;padding:18px;background:#111;color:#eee;"
         "font:16px/1.4 -apple-system,system-ui,Roboto,sans-serif}"
         "main{max-width:30rem;margin:0 auto}"
         "h1{font-size:1.25rem;margin:0}"
         "p.sub{margin:.2rem 0 1.4rem;color:#888;font-size:.85rem}"
         "h2{font-size:.72rem;text-transform:uppercase;letter-spacing:.09em;color:#888;"
         "margin:1.6rem 0 .3rem;border-bottom:1px solid #2a2a2a;padding-bottom:.4rem}"
         "label{display:flex;align-items:center;gap:.75rem;padding:.5rem 0}"
         "label span{flex:1}"
         "label em{color:#777;font-style:normal;font-size:.8rem;min-width:2.4rem}"
         "input[type=number]{width:5.5rem;padding:.5rem;background:#1c1c1c;border:1px solid #333;"
         "border-radius:6px;color:#fff;font-size:1rem;text-align:right}"
         "input[type=range]{flex:1;accent-color:#FF6D00}"
         "input[type=checkbox]{width:1.35rem;height:1.35rem;accent-color:#FF6D00}"
         "button{width:100%;padding:.85rem;margin-top:1.2rem;border:0;border-radius:8px;"
         "background:#FF6D00;color:#111;font-size:1rem;font-weight:600}"
         "button.ghost{background:#1c1c1c;color:#ccc;border:1px solid #333;font-weight:400;margin-top:.5rem}"
         ".note{background:#14301b;border:1px solid #2c5c38;color:#a9e0bb;padding:.6rem .8rem;"
         "border-radius:6px;margin-bottom:1.2rem;font-size:.9rem}"
         ".warn{color:#FB8C00;font-size:.78rem;margin:.5rem 0 0}"
         "</style></head><body><main>"
         "<h1>Outback Gauge</h1><p class=sub>Settings are saved on the gauge and survive a reboot.</p>");

  if (note) {
    o += F("<div class=note>");
    o += note;
    o += F("</div>");
  }

  o += F("<form method=POST action=/save>"
         "<h2>Warnings</h2>");
  num(o, "boost", "Boost", "min=0 max=20 step=0.5", s.boostWarnPsi, 1, "psi");
  num(o, "cool", "Coolant", "min=150 max=260 step=1", s.coolantWarnF, 0, "&deg;F");
  num(o, "intake", "Intake air", "min=60 max=200 step=1", s.intakeWarnF, 0, "&deg;F");
  num(o, "vlo", "Battery low", "min=10 max=14 step=0.1", s.voltsLowWarn, 1, "V");
  num(o, "vhi", "Battery high", "min=13 max=16 step=0.1", s.voltsHighWarn, 1, "V");
  num(o, "tilt", "Tilt", "min=5 max=45 step=1", s.tiltWarnDeg, 0, "&deg;");

  o += F("<h2>Display</h2><label><span>Backlight</span>"
         "<input type=range name=bl min=5 max=100 step=5 value=");
  o += String((int)s.backlight);
  o += F(" oninput=\"this.nextElementSibling.textContent=this.value+'%'\"><em>");
  o += String((int)s.backlight);
  o += F("%</em></label>");

  o += F("<h2>Orientation</h2>");
  check(o, "rot", "Rotate picture 180&deg;", s.rotate180);
  check(o, "swap", "Swap touch X and Y", s.touchSwapXY);
  check(o, "invx", "Invert touch X", s.touchInvertX);
  check(o, "invy", "Invert touch Y", s.touchInvertY);
  check(o, "roll", "Invert tilt roll", s.rollSign < 0);
  check(o, "pitch", "Invert tilt pitch", s.pitchSign < 0);
  o += F("<p class=warn>Rotating the picture needs a reboot. Everything else applies as soon as you save.</p>"
         "<button>Save</button></form>"
         "<form method=POST action=/defaults><button class=ghost>Restore defaults</button></form>"
         "<form method=POST action=/reboot><button class=ghost>Reboot the gauge</button></form>"
         "<form method=POST action=/stop><button class=ghost>Turn Wi-Fi off</button></form>"
         "</main></body></html>");
  return o;
}

}  // namespace

String webpage_render(const Settings &s, const char *note) {
  return render(s, note);
}
