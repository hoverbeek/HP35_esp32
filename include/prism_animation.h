#pragma once
#include <Arduino_GFX_Library.h>

namespace hp35prism {
// Full-black prism view. A minimal 5x7 bitmap font is projected onto a
// rotating vertical plane (perspective-like horizontal compression).
void begin(Arduino_GFX* lcd);
void tick(const char* result, bool error);
void reset();
// Switch between dedicated LVGL screens (noop in non-LVGL fallback).
void setActive(bool active);
}
