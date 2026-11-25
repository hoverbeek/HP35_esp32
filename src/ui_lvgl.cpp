#ifndef HP35_USE_LVGL
#define HP35_USE_LVGL 0
#endif
#if HP35_USE_LVGL
#include "ui_lvgl.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstring>

namespace hp35ui {
namespace {
constexpr int WIDTH = 240, HEIGHT = 240, BUF_LINES = 20;
Arduino_GFX *tft = nullptr;
lv_display_t *display = nullptr;
static uint16_t drawBuffer[WIDTH * BUF_LINES]; // 9,600 bytes internal RAM
lv_obj_t *tabs[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *body[10] = {nullptr};
lv_obj_t *stackValues[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *footer = nullptr;
lv_obj_t *rootScreen = nullptr;
lv_obj_t *mainPanel = nullptr;
lv_obj_t *stackDivider = nullptr;
lv_obj_t *titleLabel = nullptr;
lv_obj_t *versionLabel = nullptr;
const char *tabNames[] = {"STACK", "BCD", "HIDDEN", "TRACE"};
const uint32_t BG=0x1B263B, PANEL=0x111B2A, DIM=0x94A3B8, ACCENT=0xF59E0B,
               TEXT=0xFFFFFF, GREEN=0x86EFAC, RED=0xFF7777, YELLOW=0xFFD54F;
const uint32_t VIEW_BG[4] = {0x8CD5FF,0x91E0BB,0xD6A8EE,0xF4C68B};
const uint32_t VIEW_PANEL[4] = {0x287EB8,0x267C58,0x775092,0x98602E};

lv_color_t color(uint32_t rgb) {return lv_color_hex(rgb);}
void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels) {
  if (!tft) {lv_display_flush_ready(d);return;}
  int w = a->x2 - a->x1 + 1;
  int h = a->y2 - a->y1 + 1;
  // RGB565 is rendered into native-endian uint16_t pixels (no swap here).
  tft->draw16bitRGBBitmap(a->x1, a->y1,
                           reinterpret_cast<uint16_t*>(pixels), w, h);
  lv_display_flush_ready(d);
}
// Fit the *rendered glyph width*, not a guessed number of characters.
// The 179px field is fixed and right-aligned. Avoid truncating exponent digits.
const lv_font_t* chooseStackFont(const char* value, int32_t maxWidth) {
  const lv_font_t* choices[] = {
    &lv_font_montserrat_20,
    &lv_font_montserrat_16,
    &lv_font_montserrat_14,
    &lv_font_montserrat_12
  };
  for(const lv_font_t* font : choices) {
    const int32_t width = lv_text_get_width(value, strlen(value), font, 0);
    if(width <= maxWidth) return font;
  }
  return &lv_font_montserrat_12;
}
void setFittedStackValue(lv_obj_t* target, const char* value) {
  const int32_t available = lv_obj_get_width(target);
  const lv_font_t* font = chooseStackFont(value, available);
  lv_obj_set_style_text_font(target, font, 0);
  // Rare values still wider than the smallest font scroll instead of clipping.
  const bool tooWide = lv_text_get_width(value, strlen(value), font, 0) > available;
  lv_label_set_long_mode(target, tooWide ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP);
  lv_label_set_text(target, value);
}
void style(lv_obj_t *o, uint32_t fg, uint32_t bg, bool transparent=true) {
  lv_obj_set_style_text_color(o, color(fg), 0);
  if (transparent) lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  else {lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);lv_obj_set_style_bg_color(o,color(bg),0);}
  lv_obj_set_style_border_width(o,0,0);
  lv_obj_set_style_pad_all(o,0,0);
  lv_obj_set_style_radius(o,0,0);
}
lv_obj_t *label(lv_obj_t *root, const char *str, int x, int y, int w,
                int h, uint32_t fg) {
  lv_obj_t *o=lv_label_create(root);
  lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
  lv_label_set_text(o,str);
  style(o,fg,BG);
  return o;
}
}
void begin(Arduino_GFX *physical) {
  tft=physical;
  lv_init();
  lv_tick_set_cb([]() -> uint32_t {return millis();});
  display=lv_display_create(WIDTH,HEIGHT);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display,flush);
  lv_display_set_buffers(display,drawBuffer,nullptr,sizeof(drawBuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_obj_t *root=lv_screen_active();
  rootScreen=root;
  lv_obj_set_style_bg_color(root,color(BG),0);
  lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
  lv_obj_set_style_pad_all(root,0,0);
  lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
  titleLabel=label(root,"HP-35 DEBUGGER",8,8,180,20,0x173D60);
  versionLabel=label(root,"P7.18",202,8,38,20,0x173D60);
  for(int i=0;i<4;i++) {
    tabs[i]=lv_obj_create(root);
    lv_obj_set_pos(tabs[i],5+i*58,32);
    lv_obj_set_size(tabs[i],55,23);
    style(tabs[i],DIM,PANEL,false);
    lv_obj_set_style_radius(tabs[i],4,0);
    lv_obj_clear_flag(tabs[i],LV_OBJ_FLAG_SCROLLABLE);
    label(tabs[i],tabNames[i],4,4,48,18,DIM);
  }
  lv_obj_t *panel=lv_obj_create(root);
  mainPanel=panel;
  lv_obj_set_pos(panel,5,60);lv_obj_set_size(panel,230,153);
  style(panel,TEXT,PANEL,false);
  lv_obj_set_style_radius(panel,5,0);
  lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
  for(int i=0;i<10;i++) body[i]=label(panel,"",7,5+i*14,214,15,TEXT);
  // Large 20px stack labels and right-aligned values; other views retain default font.
  for(int i=1;i<=4;i++) {
    lv_obj_set_style_text_font(body[i],&lv_font_montserrat_20,0);
    lv_obj_set_height(body[i],27);
  }
  // Separate right-aligned numeric values for the four stack rows.
  for(int i=0;i<4;i++) {
    stackValues[i]=label(panel,"",35,25+i*32,179,27,i==3?YELLOW:TEXT);
    lv_obj_set_style_text_font(stackValues[i],&lv_font_montserrat_20,0);
    lv_obj_set_style_text_align(stackValues[i],LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_add_flag(stackValues[i],LV_OBJ_FLAG_HIDDEN);
  }
  // Stack-only separator, hidden in all other views.
  stackDivider=lv_obj_create(panel);
  lv_obj_set_pos(stackDivider,8,13);
  lv_obj_set_size(stackDivider,214,1);
  style(stackDivider,TEXT,TEXT,false);
  lv_obj_add_flag(stackDivider,LV_OBJ_FLAG_HIDDEN);
  footer=label(root,"UART0 | 115200",8,219,225,19,GREEN);
  lv_timer_handler();
}
void render(const char *selectedTab,const std::vector<std::string>& lines,
            const char *status,bool error) {
  if(!display)return;
  int tabIndex=0;
  for(int i=0;i<4;i++)if(strcmp(selectedTab,tabNames[i])==0){tabIndex=i;break;}
  lv_obj_set_style_bg_color(rootScreen,color(VIEW_BG[tabIndex]),0);
  lv_obj_set_style_bg_color(mainPanel,color(VIEW_PANEL[tabIndex]),0);
  // Dark header ink is readable against all four pastel backgrounds.
  lv_obj_set_style_text_color(titleLabel,color(0x173D60),0);
  lv_obj_set_style_text_color(versionLabel,color(0x173D60),0);
  const bool isStack=tabIndex==0;
  // Stack view uses the space formerly occupied by its redundant subheading
  // and UART status footer. Other three views retain their original geometry.
  lv_obj_set_height(mainPanel,isStack?175:153);
  if(isStack)lv_obj_add_flag(footer,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(footer,LV_OBJ_FLAG_HIDDEN);
  if(isStack)lv_obj_remove_flag(stackDivider,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(stackDivider,LV_OBJ_FLAG_HIDDEN);
  for(int i=0;i<4;i++) {
    if(isStack)lv_obj_remove_flag(stackValues[i],LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(stackValues[i],LV_OBJ_FLAG_HIDDEN);
  }
  for(int i=0;i<4;i++) {
    bool active=strcmp(selectedTab,tabNames[i])==0;
    lv_obj_set_style_bg_color(tabs[i],color(active?VIEW_PANEL[tabIndex]:0x101820),0);
    lv_obj_t *text=lv_obj_get_child(tabs[i],0);
    if(text)lv_obj_set_style_text_color(text,color(active?ACCENT:DIM),0);
  }
  for(int i=0;i<10;i++) {
    const char *str=i<(int)lines.size()?lines[i].c_str():"";
    // STACK: header + white rule + four register rows. Yellow X and its value.
    if(isStack) {
      const int y = (i==0)?5 : (i>=1&&i<=4 ? 25+(i-1)*32 : 150+(i-5)*14);
      lv_obj_set_y(body[i],y);
      if(i>=1 && i<=4) {
        // Input format from main.cpp: "T  0." etc.
        // Keep the register identifier left and the value flush-right.
        const char* value = strstr(str,"  ");
        if(value) value += 2;
        else value = str[0] ? str+1 : str;
        while(*value==' ') ++value;
        char name[2] = {str[0] ? str[0] : ' ', 0};
        lv_label_set_text(body[i],name);
        setFittedStackValue(stackValues[i-1],value);
        lv_obj_set_style_text_color(stackValues[i-1],color(i==4?YELLOW:TEXT),0);
      } else lv_label_set_text(body[i],i==0?"":str);
      lv_obj_set_style_text_color(body[i],color(i==4?YELLOW:TEXT),0);
    } else {
      lv_obj_set_y(body[i],5+i*14);
      lv_label_set_text(body[i],str);
      lv_obj_set_style_text_color(body[i],color(i==0?ACCENT:TEXT),0);
    }
  }
  std::string foot="UART0 | ";foot += status;
  lv_label_set_text(footer,foot.c_str());
  lv_obj_set_style_text_color(footer,color(error?RED:GREEN),0);
  lv_timer_handler();
}
void redraw(){if(display) {lv_obj_invalidate(lv_screen_active());lv_refr_now(display);}}
void tick(){if(display)lv_timer_handler();}
}

#endif // HP35_USE_LVGL
