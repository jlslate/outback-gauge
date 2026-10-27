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

// A limit as a bare input with its unit, for a table cell.
void limit(String &o, const char *id, const char *attrs, float v, int dp, const char *unit) {
  char buf[16];
  snprintf(buf, sizeof buf, "%.*f", dp, v);
  o += "<span class=lim><input type=number name=";
  o += id;
  o += " ";
  o += attrs;
  o += " value=";
  o += buf;
  o += "><em>";
  o += unit;
  o += "</em></span>";
}

void check(String &o, const char *id, const char *label, bool on) {
  o += "<label class=chk><input type=checkbox name=";
  o += id;
  o += on ? " checked><span>" : "><span>";
  o += label;
  o += "</span></label>";
}

// In the order of the pages in ui.cpp.
const char *const PAGE_NAMES[GAUGE_PAGES] = {"Boost", "Engine load", "Coolant", "Oil temperature", "Intake air", "Battery"};

String render(const Settings &s, const char *note) {
  String o;
  o.reserve(9500);
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
         "select{padding:.5rem;background:#1c1c1c;border:1px solid #333;border-radius:6px;color:#fff;font-size:1rem}"
         "input[type=range]{flex:1;accent-color:#FF6D00}"
         "table{width:100%;border-collapse:collapse;margin-top:.3rem}"
         "th{font-size:.7rem;font-weight:400;text-transform:uppercase;letter-spacing:.06em;color:#888;"
         "text-align:center;padding:.3rem .25rem}"
         "th:first-child,td:first-child{text-align:left}"
         "td{padding:.45rem .25rem;border-top:1px solid #222;text-align:center;vertical-align:middle}"
         "td input[type=number]{width:4.6rem;padding:.4rem}"
         "td input[type=radio]{width:1.35rem;height:1.35rem;accent-color:#FF6D00}"
         ".lim{display:flex;align-items:center;justify-content:center;gap:.3rem;margin:.15rem 0}"
         ".lim em{min-width:1.6rem;text-align:left;white-space:nowrap}"
         "td input[type=number]{width:4.2rem}"
         ".none{color:#555}"
         "td input:disabled{opacity:.25}"
         "#toast{position:fixed;left:50%;bottom:1.2rem;transform:translateX(-50%);padding:.7rem 1.3rem;"
         "border-radius:8px;background:#14301b;border:1px solid #2c5c38;color:#a9e0bb;display:none;z-index:9;"
         "box-shadow:0 4px 18px #000a}"
         "#toast.bad{background:#3a1616;border-color:#7a2c2c;color:#f2b2b2}"
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

  o += F("<form method=POST action=/save data-toast=\"Saved\"><h2>Gauges</h2><table>"
         "<tr><th>Gauge<th>Warns at<th>Show<th>Start on</tr>");
  for (int i = 0; i < GAUGE_PAGES; i++) {
    o += "<tr data-g><td>";
    o += PAGE_NAMES[i];
    o += "<td>";
    switch (i) {
      case 0: limit(o, "boost", "min=0 max=20 step=0.5", s.boostWarnPsi, 1, "psi"); break;
      case 2: limit(o, "cool", "min=150 max=260 step=1", s.coolantWarnF, 0, "&deg;F"); break;
      case 3: limit(o, "oil", "min=180 max=300 step=1", s.oilWarnF, 0, "&deg;F"); break;
      case 4: limit(o, "intake", "min=60 max=200 step=1", s.intakeWarnF, 0, "&deg;F"); break;
      case 5:
        limit(o, "vlo", "min=10 max=14 step=0.1", s.voltsLowWarn, 1, "V lo");
        limit(o, "vhi", "min=13 max=16 step=0.1", s.voltsHighWarn, 1, "V hi");
        break;
      default: o += "<span class=none>none</span>";
    }
    o += "<td><input type=checkbox name=pg";
    o += String(i);
    o += s.pageMask & (1u << i) ? " checked>" : ">";
    o += "<td><input type=radio name=start value=";
    o += String(i);
    o += s.startPage == i ? " checked>" : ">";
    o += "</tr>";
  }
  o += F("<tr><td colspan=3>The last one I tapped to<td><input type=radio name=start value=");
  o += String((int)START_LAST);
  o += s.startPage == START_LAST ? F(" checked>") : F(">");
  o += F("</tr></table>"
         "<p class=warn>Unticked gauges are never shown, not even for a warning. "
         "The start gauge falls back to the first one shown if you hide it.</p>");

  o += F("<h2>Rotation</h2>");
  check(o, "auto", "Cycle through the gauges", s.autoRotate);
  num(o, "secs", "Seconds per gauge", "min=1 max=60 step=0.5", s.autoRotateSecs, 1, "s");
  o += F("<p class=warn>Tap the screen to move on early. Hold to open this page.</p>");

  o += F("<h2>Display</h2><label><span>Backlight</span>"
         "<input type=range name=bl min=5 max=100 step=5 value=");
  o += String((int)s.backlight);
  o += F(" oninput=\"this.nextElementSibling.textContent=this.value+'%'\"><em>");
  o += String((int)s.backlight);
  o += F("%</em></label>");

  o += F("<button>Save</button></form>"
         "<form method=POST action=/defaults data-toast=\"Defaults restored\" data-confirm=\"Put every setting back to its default?\">"
         "<button class=ghost>Restore defaults</button></form>"
         "<form method=POST action=/reboot data-confirm=\"Reboot the gauge? The Wi-Fi drops, so hold the screen to bring it back.\">"
         "<button class=ghost>Reboot the gauge</button></form>"
         "<form method=POST action=/stop data-confirm=\"Turn Wi-Fi off? Hold the screen to bring it back.\">"
         "<button class=ghost>Turn Wi-Fi off</button></form>"
         "</main><div id=toast></div><script>"
         "function sync(){var rows=document.querySelectorAll('tr[data-g]'),on=0,pick=null;"
         "rows.forEach(function(r){var c=r.querySelector('input[type=checkbox]'),d=r.querySelector('input[type=radio]');"
         "d.disabled=!c.checked;if(c.checked){on++;if(!pick)pick=d}"
         "if(d.disabled&&d.checked){d.checked=false;if(pick)pick.checked=true}});"
         "var sel=document.querySelector('input[name=start]:checked');"
         "if(!sel){(pick||document.querySelector('input[name=start]:not(:disabled)')).checked=true}}"
         "function toast(t,bad){var e=document.getElementById('toast');e.textContent=t;e.className=bad?'bad':'';"
         "e.style.display='block';clearTimeout(toast.t);toast.t=setTimeout(function(){e.style.display='none'},3000)}"
         "document.addEventListener('change',function(e){if(e.target.type=='checkbox'&&e.target.name&&e.target.name.indexOf('pg')==0){"
         "if(!document.querySelector('input[name^=pg]:checked'))e.target.checked=true;sync()}});"
         "document.addEventListener('submit',function(e){var f=e.target,c=f.getAttribute('data-confirm');"
         "if(c&&!confirm(c)){e.preventDefault();return}"
         "var t=f.getAttribute('data-toast');if(!t)return;e.preventDefault();"
         "fetch(f.action,{method:'POST',body:new URLSearchParams(new FormData(f))}).then(function(r){return r.text()})"
         ".then(function(h){var y=scrollY;document.body.innerHTML=new DOMParser().parseFromString(h,'text/html').body.innerHTML;"
         "var n=document.querySelector('.note');if(n)n.remove();sync();scrollTo(0,y);toast(t)})"
         ".catch(function(){toast('Could not reach the gauge',1)})});"
         "sync();</script></body></html>");
  return o;
}

}  // namespace

String webpage_render(const Settings &s, const char *note) {
  return render(s, note);
}
