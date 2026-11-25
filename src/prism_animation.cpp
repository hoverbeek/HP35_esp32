#include "prism_animation.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#ifndef HP35_USE_LVGL
#define HP35_USE_LVGL 0
#endif
#if HP35_USE_LVGL
#include <lvgl.h>

// The prism is now an independent LVGL screen. All pixels reach the ST7789
// solely via ui_lvgl.cpp's LVGL flush callback: never through direct GFX writes.
namespace hp35prism {
namespace {
constexpr int W=240,H=240;
constexpr uint32_t FRAME_MS=40;
uint32_t originMs=0, previousFrame=0;
lv_obj_t* normalScreen=nullptr;
lv_obj_t* prismScreen=nullptr;
lv_obj_t* prismDrawing=nullptr;
char displayedValue[64]="0";
bool hasError=false;
struct Glyph { char ch; uint8_t col[5]; };
const Glyph GLYPHS[] = {
  {'0',{0x3E,0x51,0x49,0x45,0x3E}},
  {'1',{0x00,0x42,0x7F,0x40,0x00}},
  {'2',{0x42,0x61,0x51,0x49,0x46}},
  {'3',{0x21,0x41,0x45,0x4B,0x31}},
  {'4',{0x18,0x14,0x12,0x7F,0x10}},
  {'5',{0x27,0x45,0x45,0x45,0x39}},
  {'6',{0x3C,0x4A,0x49,0x49,0x30}},
  {'7',{0x01,0x71,0x09,0x05,0x03}},
  {'8',{0x36,0x49,0x49,0x49,0x36}},
  {'9',{0x06,0x49,0x49,0x29,0x1E}},
  {'X',{0x63,0x14,0x08,0x14,0x63}},
  {'-',{0x08,0x08,0x08,0x08,0x08}},
  {'+',{0x08,0x08,0x3E,0x08,0x08}},
  {'.',{0x00,0x60,0x60,0x00,0x00}},
  {'e',{0x38,0x54,0x54,0x54,0x18}},
  {'E',{0x7F,0x49,0x49,0x49,0x41}},
  {' ',{0,0,0,0,0}},
  {'?',{0x02,0x01,0x51,0x09,0x06}}
};
const uint8_t* bitmap(char ch) {
  for(const auto& glyph : GLYPHS) if(glyph.ch == ch) return glyph.col;
  return GLYPHS[sizeof(GLYPHS)/sizeof(GLYPHS[0])-1].col;
}

void rect(lv_layer_t* layer,int x,int y,int width,int height,lv_color_t ink) {
  if(x<0){width+=x;x=0;}
  if(y<0){height+=y;y=0;}
  if(x+width>W)width=W-x;
  if(y+height>H)height=H-y;
  if(width<=0||height<=0)return;
  lv_draw_rect_dsc_t desc;
  lv_draw_rect_dsc_init(&desc);
  desc.bg_color=ink;
  desc.bg_opa=LV_OPA_COVER;
  desc.border_width=0;
  desc.radius=0;
  lv_area_t a={x,y,x+width-1,y+height-1};
  lv_draw_rect(layer,&desc,&a);
}
void drawPrism(lv_event_t* e) {
  lv_layer_t* layer=lv_event_get_layer(e);
  if(!layer)return;
  char buffer[80];
  snprintf(buffer,sizeof(buffer),"X  %s",displayedValue);
  const size_t len=strlen(buffer);
  const int cell=(len>19)?1:((len>12)?2:((len>9)?3:4));
  const int logicalWidth=int(len)*6*cell;
  const int top=(H-7*cell)/2;
  const uint32_t now=millis();
  const float phase=((now-originMs)%4000UL)*(6.28318530718f/4000.0f);
  const float squeeze=cosf(phase);
  const float light=0.35f+0.65f*fabsf(squeeze);
  const uint8_t red=uint8_t(255*light);
  const uint8_t green=uint8_t((hasError?75:213)*light);
  const uint8_t blue=uint8_t((hasError?75:79)*light);
  const lv_color_t ink=lv_color_make(red,green,blue);
  for(size_t i=0;i<len;++i) {
    const uint8_t* glyph=bitmap(buffer[i]);
    for(int col=0;col<5;++col) {
      const float left=(int(i)*6+col)*cell-logicalWidth*0.5f;
      const float right=left+cell;
      int x1=int(roundf(W/2.0f+left*squeeze));
      int x2=int(roundf(W/2.0f+right*squeeze));
      if(x2<x1){int tmp=x1;x1=x2;x2=tmp;}
      if(x2==x1)x2=x1+1;
      // The 180-degree ST7789 orientation and prism reflection need this
      // horizontal pre-mirror, preserving the working Phase 7.13 optics.
      const int mirroredLeft=W-x2;
      const int mirroredRight=W-x1;
      x1=mirroredLeft;x2=mirroredRight;
      for(int bit=0;bit<7;++bit) if(glyph[col]&(1U<<bit))
        rect(layer,x1,top+bit*cell,x2-x1,cell,ink);
    }
  }
}
}
void begin(Arduino_GFX*) {
  normalScreen=lv_screen_active();
  prismScreen=lv_obj_create(nullptr);
  lv_obj_remove_style_all(prismScreen);
  lv_obj_set_style_bg_color(prismScreen,lv_color_black(),0);
  lv_obj_set_style_bg_opa(prismScreen,LV_OPA_COVER,0);
  lv_obj_set_size(prismScreen,W,H);
  lv_obj_clear_flag(prismScreen,LV_OBJ_FLAG_SCROLLABLE);
  prismDrawing=lv_obj_create(prismScreen);
  lv_obj_remove_style_all(prismDrawing);
  lv_obj_set_pos(prismDrawing,0,0);
  lv_obj_set_size(prismDrawing,W,H);
  lv_obj_set_style_bg_opa(prismDrawing,LV_OPA_TRANSP,0);
  lv_obj_clear_flag(prismDrawing,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(prismDrawing,drawPrism,LV_EVENT_DRAW_MAIN,nullptr);
  reset();
}
void reset() {
  previousFrame=0;
  originMs=millis();
  if(prismDrawing)lv_obj_invalidate(prismDrawing);
}
void setActive(bool active) {
  lv_obj_t* next=active?prismScreen:normalScreen;
  if(next) {
    lv_screen_load(next);
    lv_obj_invalidate(next);
    if(active)reset();
    lv_refr_now(lv_display_get_default());
  }
}
void tick(const char* value,bool error) {
  if(!prismDrawing)return;
  const uint32_t now=millis();
  if(previousFrame && now-previousFrame<FRAME_MS)return;
  previousFrame=now;
  const char* source=(value&&*value)?value:"0";
  strncpy(displayedValue,source,sizeof(displayedValue)-1);
  displayedValue[sizeof(displayedValue)-1]=0;
  hasError=error;
  lv_obj_invalidate(prismDrawing); // LVGL paints black + glyphs in one pass
}
}
#else
// Fallback for HP35_USE_LVGL=0: the original direct-drawing Phase 7.13 path.
#include "prism_animation.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace hp35prism {
namespace {
Arduino_GFX* screen = nullptr;
uint32_t previousFrame = 0;
uint32_t originMs = 0;
constexpr uint32_t FRAME_MS = 40; // 25 fps; LCD SPI is the limiting factor
constexpr int W = 240, H = 240;
// Standard five-column, seven-row bitmap font. The least significant bit
// denotes the top pixel. Only digits and calculator notation are needed.
struct Glyph { char ch; uint8_t col[5]; };
const Glyph GLYPHS[] = {
  {'0',{0x3E,0x51,0x49,0x45,0x3E}},
  {'1',{0x00,0x42,0x7F,0x40,0x00}},
  {'2',{0x42,0x61,0x51,0x49,0x46}},
  {'3',{0x21,0x41,0x45,0x4B,0x31}},
  {'4',{0x18,0x14,0x12,0x7F,0x10}},
  {'5',{0x27,0x45,0x45,0x45,0x39}},
  {'6',{0x3C,0x4A,0x49,0x49,0x30}},
  {'7',{0x01,0x71,0x09,0x05,0x03}},
  {'8',{0x36,0x49,0x49,0x49,0x36}},
  {'9',{0x06,0x49,0x49,0x29,0x1E}},
  {'X',{0x63,0x14,0x08,0x14,0x63}},
  {'-',{0x08,0x08,0x08,0x08,0x08}},
  {'+',{0x08,0x08,0x3E,0x08,0x08}},
  {'.',{0x00,0x60,0x60,0x00,0x00}},
  {'e',{0x38,0x54,0x54,0x54,0x18}},
  {'E',{0x7F,0x49,0x49,0x49,0x41}},
  {' ',{0,0,0,0,0}},
  {'?',{0x02,0x01,0x51,0x09,0x06}}
};
const uint8_t* bitmap(char ch) {
  for(const auto& glyph : GLYPHS) if(glyph.ch == ch) return glyph.col;
  return GLYPHS[sizeof(GLYPHS)/sizeof(GLYPHS[0])-1].col;
}
void frame(const char* value, bool error, uint32_t now) {
  // Clear to true black. The prism reflects the bright glyphs but the black
  // LCD pixels are only *approximately* transparent in the optical illusion.
  screen->fillScreen(0x0000);
  char text[80];
  snprintf(text,sizeof(text),"X  %s", value && *value ? value : "0");
  const size_t len = strlen(text);
  // Fit long scientific-notation values before rotation; never truncate.
  const int cell = (len > 19) ? 1 : ((len > 12) ? 2 : ((len > 9) ? 3 : 4));
  const int logicalWidth = (int)len * 6 * cell;
  const int logicalHeight = 7 * cell;
  const int top = (H - logicalHeight) / 2;
  // ~4 seconds per revolution; signed cosine makes the back face mirrored.
  const float phase = ((now - originMs) % 4000UL) * (6.28318530718f / 4000.0f);
  const float squeeze = cosf(phase);
  // Fade the edge-on frame and back face slightly for visual depth.
  const float light = 0.35f + 0.65f * fabsf(squeeze);
  const uint8_t r = error ? (uint8_t)(255*light) : (uint8_t)(255*light);
  const uint8_t g = error ? (uint8_t)(75*light) : (uint8_t)(213*light);
  const uint8_t b = error ? (uint8_t)(75*light) : (uint8_t)(79*light);
  const uint16_t ink = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  // Each glyph column is projected independently. This creates a horizontal
  // flip at the halfway point, unlike a simple pulsating text-width animation.
  for(size_t i=0; i<len; ++i) {
    const uint8_t* glyph = bitmap(text[i]);
    for(int col=0; col<5; ++col) {
      const float left = ((int)i*6 + col)*cell - logicalWidth*0.5f;
      const float right = left + cell;
      int x1 = (int)roundf(W/2.0f + left*squeeze);
      int x2 = (int)roundf(W/2.0f + right*squeeze);
      if(x2 < x1) { const int tmp=x1; x1=x2; x2=tmp; }
      if(x2 == x1) x2 = x1+1;
      // Pre-mirror the image horizontally for the prism's reflection.
      // This is applied *after* the rotating-plane projection so that
      // both the front and reverse faces read correctly in reflection.
      // Normal LVGL views are deliberately unaffected.
      const int mirroredLeft = W - x2;
      const int mirroredRight = W - x1;
      x1 = mirroredLeft;
      x2 = mirroredRight;
      if(x1 < 0 || x2 > W) continue;
      for(int bit=0; bit<7; ++bit) if(glyph[col] & (1U<<bit)) {
        screen->fillRect(x1, top + bit*cell, x2-x1, cell, ink);
      }
    }
  }
}
}
void begin(Arduino_GFX* lcd) {screen=lcd;reset();}
void reset() {previousFrame=0;originMs=millis();if(screen)screen->fillScreen(0x0000);}
void tick(const char* result, bool error) {
  if(!screen)return;
  const uint32_t now=millis();
  if(previousFrame && now-previousFrame<FRAME_MS)return;
  previousFrame=now;
  frame(result,error,now);
}
}

void hp35prism::setActive(bool) {} // no LVGL screen in fallback mode
#endif
