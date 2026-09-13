#include "gesture.h"
#include <cstdio>
#include <vector>
#include <string>

const char *name(TouchEvent e) {
  switch (e) { case TouchEvent::Tap: return "Tap"; case TouchEvent::LongPress: return "LongPress";
    case TouchEvent::SwipeLeft: return "SwipeLeft"; case TouchEvent::SwipeRight: return "SwipeRight"; default: return "None"; }
}
struct Sample { bool report, down; int x, y; };
// Polls every 20 ms; returns all non-None events in order.
std::string run(const std::vector<Sample> &s, int tailPolls = 20) {
  GestureDetector g; std::string out; uint32_t t = 1000;
  auto feed = [&](Sample v) { TouchEvent e = g.step(v.report, v.down, v.x, v.y, t); if (e != TouchEvent::None) out += std::string(out.empty() ? "" : ",") + name(e); t += 20; };
  for (auto v : s) feed(v);
  for (int i = 0; i < tailPolls; i++) feed({false, false, 0, 0});
  return out.empty() ? "None" : out;
}
std::vector<Sample> hold(int n, int x, int y) { return std::vector<Sample>(n, {true, true, x, y}); }
std::vector<Sample> operator+(std::vector<Sample> a, const std::vector<Sample> &b) { a.insert(a.end(), b.begin(), b.end()); return a; }
std::vector<Sample> lift(int x, int y) { return {{true, false, x, y}}; }
std::vector<Sample> drag(int x0, int y0, int x1, int y1, int n) { std::vector<Sample> v; for (int i = 0; i <= n; i++) v.push_back({true, true, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n}); return v; }
std::vector<Sample> sparse(int n, int x, int y, int every) { std::vector<Sample> v; for (int i = 0; i < n; i++) v.push_back(i % every == 0 ? Sample{true, true, x, y} : Sample{false, false, 0, 0}); return v; }

int main() {
  struct Case { const char *what; std::vector<Sample> s; const char *expect; } cases[] = {
    {"tap with lift report", hold(5, 200, 200) + lift(200, 200), "Tap"},
    {"tap ended by report gap", hold(5, 200, 200), "Tap"},
    {"tap with jitter", drag(200, 200, 212, 190, 5), "Tap"},
    {"long press", hold(60, 200, 200) + lift(200, 200), "LongPress"},
    {"long press then gap", hold(60, 200, 200), "LongPress"},
    {"swipe left", drag(320, 200, 100, 210, 10) + lift(100, 210), "SwipeLeft"},
    {"swipe right", drag(100, 200, 320, 190, 10), "SwipeRight"},
    {"vertical drag", drag(200, 80, 220, 330, 10), "None"},
    {"hold 800ms (too long for tap, too short for long)", hold(40, 200, 200) + lift(200, 200), "None"},
    {"drag then hold still (no long press)", drag(100, 200, 300, 200, 10) + hold(60, 300, 200), "SwipeRight"},
    {"intermittent reports while held", sparse(6, 200, 200, 3) + lift(200, 200), "Tap"},
    {"two taps", hold(4, 200, 200) + lift(200, 200) + std::vector<Sample>(5, {false,false,0,0}) + hold(4, 150, 150) + lift(150, 150), "Tap,Tap"},
  };
  int fails = 0;
  for (auto &c : cases) { std::string got = run(c.s); bool ok = got == c.expect; fails += !ok;
    printf("%s  %-52s got %-12s expect %s\n", ok ? "PASS" : "FAIL", c.what, got.c_str(), c.expect); }
  printf("%d failed\n", fails); return fails;
}
