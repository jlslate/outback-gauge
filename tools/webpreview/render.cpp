// Writes the settings form to a file so it can be opened in a browser without
// an ESP32. Uses the same src/webpage.cpp the firmware serves.

#include <stdio.h>

#include "settings.h"
#include "webpage.h"

int main() {
  settings_load();  // the shim has nothing saved, so this is the factory state
  const String page = webpage_render(settings(), "Saved.");
  FILE *f = fopen("settings-page.html", "wb");
  fwrite(page.c_str(), 1, page.size(), f);
  fclose(f);
  printf("%zu bytes\n", page.size());
  return 0;
}
