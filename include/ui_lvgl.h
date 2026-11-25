#pragma once
#include <Arduino_GFX_Library.h>
#include <string>
#include <vector>

namespace hp35ui {
// All LVGL calls must run from the Arduino loop task, not inside an ISR.
void begin(Arduino_GFX *display);
void render(const char *selectedTab, const std::vector<std::string>& lines,
            const char *status, bool error);
void tick();
// Force redraw after ST7789 hardware orientation changes.
void redraw();
}
