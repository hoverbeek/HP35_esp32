/* HP-35 on ESP32-S3 / Waveshare LCD 1.3: hardware and serial debugger bootstrap.
 * NOTE: mode bcd uses the Level 2 clean-room C++ digit-array BCD engine.
 * Only the optional mode float uses native double as a hardware fallback.
 */
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#ifndef HP35_USE_LVGL
#define HP35_USE_LVGL 0
#endif
#if HP35_USE_LVGL
#include "ui_lvgl.h"
#include <vector>
#endif
#include <math.h>
#include "bcd.h"
#include "rpn_bcd.h"
#include "debug_trace.h"
#include "prism_animation.h"
#include <esp_timer.h>
#include <Wire.h>
#include "motion_qmi8658.h"
#include <esp_sleep.h>
#include <esp_system.h>
#include "sleep_state.h"
#include <stdlib.h>
#include <string.h>
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED   0xF800
#define GREEN 0x07E0

// Waveshare ESP32-S3-LCD-1.3, ST7789 240x240
static constexpr int LCD_SCK=40, LCD_MOSI=41, LCD_CS=39;
static constexpr int LCD_DC=38, LCD_RST=42, LCD_BL=20;
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *lcd = new Arduino_ST7789(bus, LCD_RST, 0, true, 240, 240);

struct Calc {
  double st[4] = {0,0,0,0}; // X, Y, Z, T
  bool error = false;
  bool early = false; // experimental original-ROM bug example
  unsigned step = 0;
} calc;

hp35::RpnBCD bcdCalc;
bool bcdMode = true; // BCD mode uses only digit arithmetic
hp35::DebugTrace debugTrace;
bool traceScreen = false;
enum class View {Stack, Regs, Hidden, Trace};
View currentView = View::Stack;
// Crystal prism mode rotates the physical LCD by 180 degrees.
bool prismMode = false;
// The snapshot has a plain POD layout: dynamic std::string/vector pointers
// cannot safely survive deep sleep.
RTC_DATA_ATTR hp35sleep::Snapshot retainedState{};
static bool autoSleep=false;
static uint32_t lastInputMs=0;
static constexpr uint32_t AUTO_LIGHT_IDLE_MS=100;
static constexpr uint32_t AUTO_DEEP_IDLE_MS=30000;
static constexpr uint32_t AUTO_LIGHT_SLICE_MS=100;
static constexpr uint32_t AUTO_DEEP_WAKE_SEC=10;
void show();
static bool lightSleepMs(uint32_t ms, bool restoreBacklight=true);
static hp35motion::QMI8658 motion;
static constexpr uint32_t MOTION_THRESHOLD=1200;
static constexpr uint32_t MOTION_INTERVAL_MS=125;

// Poll the accelerometer between timer-based LIGHT sleeps.
// GPIO45/46 (QMI8658 INT2/INT1) cannot wake S3 from deep sleep.
static void motionSample() {
  hp35motion::Axes a{};
  if(!motion.read(a)) {Serial0.println("QMI8658 read failed");return;}
  Serial0.printf("acc raw x=%d y=%d z=%d; g approx x=%.3f y=%.3f z=%.3f\r\n",
    a.x,a.y,a.z,a.x/16384.0,a.y/16384.0,a.z/16384.0);
}
static void lightSleepMotion(uint32_t seconds) {
  if(!motion.available()) {
    Serial0.println("Motion sensor not initialized: attempting QMI8658 initialization...");
    if(!motion.begin()) {Serial0.println("QMI8658 initialization failed. Run motion scan.");return;}
  }
  hp35motion::Axes baseline{};
  if(!motion.read(baseline)){Serial0.println("Motion baseline failed");return;}
  Serial0.printf("Light sleep motion watch %lu s, threshold=%lu raw axis counts\r\n",
     (unsigned long)seconds,(unsigned long)MOTION_THRESHOLD);
  Serial0.flush();
  digitalWrite(LCD_BL,LOW); // Stay dark across ALL timer wakeups.
  const uint32_t started=millis();
  bool moved=false,failed=false,serialCancel=false;
  uint32_t samples=0,peakDelta=0;
  // Timer-driven light sleep: sample accelerometer after each 125ms wake.
  while(uint32_t(millis()-started)<seconds*1000UL) {
    if(!lightSleepMs(MOTION_INTERVAL_MS,false)){failed=true;break;}
    hp35motion::Axes a{};
    if(!motion.read(a)){failed=true;break;}
    ++samples;
    const uint32_t change=hp35motion::delta(a,baseline);
    if(change>peakDelta)peakDelta=change;
    if(change>=MOTION_THRESHOLD){moved=true;break;}
    // The serial monitor may leave LF pending after CR ran the command.
    // Ignore CR/LF; only non-newline input should cancel this watch.
    while(Serial0.available() && (Serial0.peek()=='\r'||Serial0.peek()=='\n')) {
      Serial0.read();
    }
    if(Serial0.available()){serialCancel=true;break;}
  }
  Serial0.printf("Motion watch: elapsed=%lu ms, samples=%lu, peak delta=%lu (trigger=%lu)\r\n",
    (unsigned long)(millis()-started),(unsigned long)samples,
    (unsigned long)peakDelta,(unsigned long)MOTION_THRESHOLD);
  if(moved)Serial0.println("Movement detected: calculator awake");
  else if(failed)Serial0.println("Sleep or QMI8658 read error");
  else if(serialCancel)Serial0.println("Motion watch cancelled by serial input");
  else Serial0.println("Motion watch timed out: no threshold crossing");
  digitalWrite(LCD_BL,HIGH); // Restore only after movement, timeout or error.
  lastInputMs=millis();
  show();
}


static void saveRetainedState() {
  auto &s=retainedState;
  memset(&s,0,sizeof(s));
  s.magic=hp35sleep::MAGIC;s.format=hp35sleep::FORMAT;
  hp35sleep::captureBCD(s,bcdCalc);
  s.bcdMode=bcdMode?1:0;s.early=calc.early?1:0;
  s.prism=prismMode?1:0;s.view=static_cast<uint8_t>(currentView);
  s.autoSleep=autoSleep?1:0;
  for(int i=0;i<4;++i)s.floatStack[i]=calc.st[i];
  s.floatError=calc.error?1:0;s.floatSteps=calc.step;
  s.checksum=hp35sleep::checksum(s);
}
static bool restoreRetainedState() {
  if(!hp35sleep::restoreBCD(retainedState,bcdCalc))return false;
  const auto &s=retainedState;
  bcdMode=s.bcdMode!=0;calc.early=s.early!=0;
  calc.error=s.floatError!=0;calc.step=s.floatSteps;
  for(int i=0;i<4;++i)calc.st[i]=s.floatStack[i];
  prismMode=s.prism!=0;currentView=static_cast<View>(s.view);
  traceScreen=(currentView==View::Trace);
  autoSleep=s.autoSleep!=0;
  // Algorithm tracing uses dynamic storage and is deliberately not retained.
  return true;
}
static bool lightSleepMs(uint32_t ms, bool restoreBacklight) {
  Serial0.flush();
  digitalWrite(LCD_BL,LOW);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  const esp_err_t configured=esp_sleep_enable_timer_wakeup(uint64_t(ms)*1000ULL);
  if(configured!=ESP_OK) {
    if(restoreBacklight) digitalWrite(LCD_BL,HIGH);
    Serial0.printf("Light-sleep timer configuration failed: %d\r\n",(int)configured);
    return false;
  }
  const esp_err_t result=esp_light_sleep_start();
  if(restoreBacklight) digitalWrite(LCD_BL,HIGH);
  if(result!=ESP_OK)Serial0.printf("Light sleep failed: %d\r\n",(int)result);
  return result==ESP_OK;
}
static void deepSleepSeconds(uint32_t secs) {
  saveRetainedState();
  Serial0.printf("Saving RTC state; deep sleep for %lu seconds. USB UART cannot wake deep sleep.\r\n",(unsigned long)secs);
  Serial0.flush();
  digitalWrite(LCD_BL,LOW);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  const esp_err_t configured=esp_sleep_enable_timer_wakeup(uint64_t(secs)*1000000ULL);
  if(configured!=ESP_OK) {
    digitalWrite(LCD_BL,HIGH);
    Serial0.printf("Deep-sleep timer configuration failed: %d\r\n",(int)configured);
    return;
  }
  esp_deep_sleep_start(); // boot setup() restores retained snapshot
}


char input[256];
size_t inputLen = 0;

#if HP35_USE_LVGL
static std::string splitLabel(const std::string& value, size_t start, size_t maxChars=27) {
  if(start >= value.size())return "";
  return value.substr(start,maxChars);
}
static void showLvgl() {
  std::vector<std::string> rows;
  const char* tab="STACK";
  if(currentView==View::Stack) {
    rows.push_back(bcdMode ? "RPN STACK | BCD" : "RPN STACK | FLOAT");
    const char *names[]={"T", "Z", "Y", "X"};
    for(int i=3;i>=0;i--) {
      std::string v;
      if(bcdMode)v=bcdCalc.st[i].toString();
      else {char tmp[32];snprintf(tmp,sizeof(tmp),"%.10g",calc.st[i]);v=tmp;}
      // UI only: suppress a trailing decimal point on whole-number values.
      // Embedded decimal points (e.g. 0.123456) and BCD values stay intact.
      if(!v.empty() && v.back()=='.') v.pop_back();
      rows.push_back(std::string(names[3-i])+"  "+v);
    }
    rows.push_back(std::string("operations: ")+std::to_string(bcdMode?bcdCalc.step:calc.step));
  } else if(currentView==View::Regs) {
    tab="BCD";
    rows.push_back("14-NIBBLE REGISTERS");
    for(int i=0;i<4;i++) {
      if(bcdMode) {
        auto digits=bcdCalc.st[i].nibbles();
        std::string line(1,"XYZT"[i]);line+=" ";
        for(auto d:digits)line+=char('0'+d); // valid decimal nibbles
        rows.push_back(line);
      } else rows.push_back("Use mode bcd");
    }
    rows.push_back("X,Y,Z,T | decimal digits");
  } else if(currentView==View::Hidden) {
    tab="HIDDEN";
    rows.push_back("26-DIGIT SCRATCH");
    if(!debugTrace.active||!debugTrace.cursor)rows.push_back("No trace: trace ln 2.02");
    else {
      const hp35::FixedBCD* work[3];
      const char* names[3];
      if(debugTrace.scientificMode){
        const auto& r=debugTrace.scientific.steps[debugTrace.cursor-1];
        work[0]=&r.before;work[1]=&r.after;work[2]=&r.accumulatorAfter;
        names[0]="BEFORE";names[1]="AFTER";names[2]="ACCUM";
      }else{
        const auto& r=debugTrace.trace.steps[debugTrace.cursor-1];
        work[0]=&r.x;work[1]=&r.y;work[2]=&r.z;
        names[0]="work.x";names[1]="work.y";names[2]="work.z";
      }
      for(int i=0;i<3;i++){
        std::string d=hp35::fixedNibbles(*work[i]);
        rows.push_back(std::string(names[i])+(work[i]->negative?" -":" +"));
        rows.push_back(splitLabel(d,0,13));
        rows.push_back(splitLabel(d,13,13));
      }
    }
  } else {
    tab="TRACE";
    rows.push_back("DECIMAL ALGORITHM TRACE");
    if(!debugTrace.active||!debugTrace.cursor)rows.push_back("trace sin 30 / trace ln 2.02");
    else {
      rows.push_back(debugTrace.operation+"  "+std::to_string(debugTrace.cursor)+"/"+std::to_string(debugTrace.count()));
      if(debugTrace.scientificMode){
        const auto& r=debugTrace.scientific.steps[debugTrace.cursor-1];
        rows.push_back("decade "+std::to_string(r.decade)+" repeat "+std::to_string(r.repetition));
        rows.push_back(splitLabel(r.action,0,27));
        rows.push_back("BEFORE:");rows.push_back(splitLabel(hp35::fixedDecimal(r.before),0));
        rows.push_back("AFTER:");rows.push_back(splitLabel(hp35::fixedDecimal(r.after),0));
      } else {
        const auto& r=debugTrace.trace.steps[debugTrace.cursor-1];
        rows.push_back("decade "+std::to_string(r.decade)+" rep "+std::to_string(r.repetition));
        rows.push_back("work.x "+splitLabel(hp35::fixedDecimal(r.x),0,19));
        rows.push_back("work.y "+splitLabel(hp35::fixedDecimal(r.y),0,19));
        rows.push_back("work.z "+splitLabel(hp35::fixedDecimal(r.z),0,19));
      }
    }
  }
  hp35ui::render(tab,rows,bcdCalc.error?"ERROR":"READY",bcdCalc.error);
}
#endif

void show() {
  if(prismMode) return; // prism animation owns the display
#if HP35_USE_LVGL
  showLvgl();
  return;
#endif
  if(traceScreen && debugTrace.active) {
    lcd->fillScreen(BLACK);
    lcd->setTextColor(0x07FF);lcd->setTextSize(2);lcd->setCursor(8,8);lcd->println("BCD TRACE");
    lcd->setTextSize(1);lcd->setTextColor(WHITE);lcd->setCursor(8,38);
    if(debugTrace.count()) {
      if(debugTrace.scientificMode){
        const auto &r=debugTrace.scientific.steps[debugTrace.cursor-1];
        lcd->printf("%s step %u/%u\n",debugTrace.operation.c_str(),(unsigned)debugTrace.cursor,(unsigned)debugTrace.count());
        lcd->printf("decade %u repeat %u\n",r.decade,r.repetition);
        lcd->println(r.action.c_str());lcd->println();
        const hp35::FixedBCD* work[3]={&r.before,&r.after,&r.accumulatorAfter};
        const char* names[3]={"BEFORE","AFTER","ACCUM"};
        for(int i=0;i<3;i++){
          lcd->printf("%s %c\n",names[i],work[i]->negative?'-':'+');
          auto digits=hp35::fixedNibbles(*work[i]);
          lcd->println(digits.substr(0,13).c_str());
          lcd->println(digits.substr(13).c_str());
        }
      } else {
      const auto &r=debugTrace.trace.steps[debugTrace.cursor-1];
      lcd->printf("%s step %u/%u\n",debugTrace.operation.c_str(),(unsigned)debugTrace.cursor,(unsigned)debugTrace.count());
      lcd->printf("decade=%u repeat=%u dir=%c\n\n",r.decade,r.repetition,r.positive?'+':'-');
      const hp35::FixedBCD* work[3]={&r.x,&r.y,&r.z};
      for(int i=0;i<3;i++) {
        lcd->printf("%c  %c\n", "xyz"[i],work[i]->negative?'-':'+');
        auto digits=hp35::fixedNibbles(*work[i]);
        lcd->println(digits.substr(0,13).c_str());
        lcd->println(digits.substr(13).c_str());
        lcd->println();
      }
      }
    } else lcd->println(debugTrace.scientificMode?"No decimal steps":debugTrace.trace.note.c_str());
    lcd->setTextColor(GREEN);lcd->setCursor(8,220);lcd->print("trace next / prev");
    return;
  }
  lcd->fillScreen(BLACK);
  lcd->setTextColor(0x07FF); lcd->setTextSize(2); lcd->setCursor(8, 8);
  lcd->println("HP-35 DEBUG");
  lcd->setTextSize(1); lcd->setTextColor(0xAD55);
  lcd->setCursor(8,35); lcd->println(bcdMode ? "ESP32-S3 | BCD PHASE 6" : "ESP32-S3 | FLOAT TEST");
  lcd->setTextColor(WHITE); lcd->setTextSize(2);
  const char *labels[4] = {"X", "Y", "Z", "T"};
  for(int i=0; i<4; i++) {
    char row[38];
    if (bcdMode) snprintf(row,sizeof(row),"%s %s",labels[i],bcdCalc.st[i].toString().c_str());
    else snprintf(row,sizeof(row),"%s % .9g", labels[i],calc.st[i]);
    lcd->setCursor(8, 57 + i*29); lcd->println(row);
  }
  lcd->drawFastHLine(8, 181, 224, 0x7BEF);
  lcd->setTextSize(1); lcd->setTextColor((bcdMode ? bcdCalc.error : calc.error) ? RED : GREEN);
  lcd->setCursor(8, 193); lcd->printf("%s   step:%lu\n",(bcdMode ? bcdCalc.error : calc.error)?"ERROR":"READY",(unsigned long)(bcdMode ? bcdCalc.step : calc.step));
  lcd->setTextColor(0xFFE0);
  lcd->setCursor(8,213); lcd->print("USB serial: 115200 baud");
}
void status() {
  if (bcdMode) {
    Serial0.printf("X=%s Y=%s Z=%s T=%s | step=%lu%s\r\n",
      bcdCalc.st[0].toString().c_str(), bcdCalc.st[1].toString().c_str(),
      bcdCalc.st[2].toString().c_str(), bcdCalc.st[3].toString().c_str(),
      bcdCalc.step, bcdCalc.error ? " ERROR (CLR required)" : "");
    show(); return;
  }
  Serial0.printf("X=% .12g Y=% .12g Z=% .12g T=% .12g | step=%lu%s\r\n",
   calc.st[0],calc.st[1],calc.st[2],calc.st[3],(unsigned long)calc.step,
   calc.error?" ERROR":"");
  show();
}
void resetCalc() {calc = Calc{}; bcdCalc.reset(); show();}
void push(double value) {
  calc.st[3]=calc.st[2]; calc.st[2]=calc.st[1]; calc.st[1]=calc.st[0]; calc.st[0]=value;
}
void binary(double result) {
  calc.st[0]=result; calc.st[1]=calc.st[2]; calc.st[2]=calc.st[3];
}
void help() {
  Serial0.println("Sleep: sleep status | sleep light [SECONDS] | sleep deep [SECONDS] | sleep auto on/off");
  Serial0.println("Motion: motion status | motion scan | motion sample | sleep motion [SECONDS]");
  Serial0.println("Auto sleep is OFF by default: 100ms light-sleep slices after 100ms idle, deep sleep after 30s.");
  Serial0.println("Phase 7.14: dedicated black LVGL prism screen, BCD default.");
  Serial0.println("Trace: trace sin/cos/tan/atan/ln/exp VALUE");
  Serial0.println("Trace navigation: trace next | trace prev | trace jump N | trace show");
  Serial0.println("Hidden scratch: hidden or allregs, view stack | view regs | view hidden | view trace");
  Serial0.println("Timing: bench OP VALUE [COUNT] (no LCD writes in timed loop)");
  Serial0.println("Display: display prism (black animated X) | display normal (four LVGL tabs)");
  Serial0.println("BCD mode: numeric literals, + - * / enter swap rdn chs clx clr regs stack.");
  Serial0.println("BCD scientific: sqrt ln log exp pow pi sin cos tan asin acos atan (degrees).");
  Serial0.println("HP-35 v0.1 serial debugger (temporary native-double math)");
  Serial0.println("Examples: 2.02 ln exp | 2 3 * | 2 enter 3 +");
  Serial0.println("Commands: + - * / ln log exp sqrt sin cos tan pow pi enter swap rdn chs clx clr");
  Serial0.println("          stack regs bcdtest step reset help early on / early off");
  Serial0.println("Numbers separated by whitespace automatically lift the stack.");
  Serial0.println("BCD-only decimal CORDIC trigonometry and inverse functions active.");
}
// For instrumentation: display the mantissa's digits as BCD nibbles.
// This is a display inspection aid, NOT actual internal BCD arithmetic.
void regs() {
  if (bcdMode) { Serial0.print(bcdCalc.registerDump().c_str()); return; }
  for(int i=0;i<4;i++) {
    char str[48]; snprintf(str,sizeof(str),"%.9e",fabs(calc.st[i]));
    Serial0.printf("%c sign=%c decimal=%s   mantissa BCD: ","XYZT"[i],
      signbit(calc.st[i])?'-':'+',str);
    int n=0;
    for(char *p=str;*p && *p!='e';p++) if(*p>='0' && *p<='9') {
      Serial0.printf("%X",*p-'0'); n++;
    }
    Serial0.printf(" (%d digits, diagnostic only)\r\n",n);
  }
}
void execute(const char* tok) {
  if(!tok || !*tok) return;
  if (bcdMode) {
    String cmd(tok); cmd.toLowerCase();
    if (cmd=="help" || cmd=="?") { help(); return; }
    if (cmd=="regs") { regs(); return; }
    if (cmd=="stack") { status(); return; }
    if (cmd=="bcdtest") {
      Serial0.printf("BCD 2.02 + 3 = %s\r\n",hp35::addBCD(hp35::BCDNumber::fromDecimal("2.02"),hp35::BCDNumber::fromDecimal("3")).toString().c_str());
      return;
    }
    if (!bcdCalc.execute(tok)) Serial0.printf("BCD: %s\r\n",bcdCalc.errorMessage.c_str());
    status(); return;
  }
  char* end=nullptr;
  double v=strtod(tok,&end);
  if(end!=tok && *end=='\0' && isfinite(v)) {push(v); calc.step++; status(); return;}
  String cmd(tok); cmd.toLowerCase();
  if(cmd=="help" || cmd=="?") {help();return;}
  if(cmd=="stack") {status();return;}
  if(cmd=="bcdtest") {
    auto a=hp35::BCDNumber::fromDecimal("2.02");
    auto b=hp35::BCDNumber::fromDecimal("3");
    auto sum=hp35::addBCD(a,b);
    auto product=hp35::mulBCD(a,b);
    Serial0.printf("BCD 2.02 + 3 = %s\r\n",sum.toString().c_str());
    Serial0.printf("BCD 2.02 * 3 = %s\r\n",product.toString().c_str());
    Serial0.printf("BCD nibbles for 2.02: ");
    for(auto nibble:a.nibbles()) Serial0.print(nibble,HEX);
    Serial0.println();
    Serial0.println("BCD test separate from floating-point debugger");
    return;
  }
  if(cmd=="regs") {regs();return;}
  if(cmd=="step") {Serial0.println("Step: each token is executed immediately in v0.1.");status();return;}
  if(cmd=="reset" || cmd=="clr") {resetCalc();status();return;}
  if(cmd=="enter") {push(calc.st[0]);}
  else if(cmd=="swap") {double t=calc.st[0];calc.st[0]=calc.st[1];calc.st[1]=t;}
  else if(cmd=="rdn") {double t=calc.st[0]; calc.st[0]=calc.st[1]; calc.st[1]=calc.st[2];calc.st[2]=calc.st[3];calc.st[3]=t;}
  else if(cmd=="clx") {calc.st[0]=0;}
  else if(cmd=="chs") {calc.st[0]=-calc.st[0];}
  else if(cmd=="pi") {push(3.14159265358979323846);}
  else if(cmd=="+" || cmd=="-" || cmd=="*" || cmd=="/" || cmd=="pow") {
    double x=calc.st[0],y=calc.st[1]; double result=0;
    if(cmd=="+")result=y+x;
    else if(cmd=="-")result=y-x;
    else if(cmd=="*")result=y*x;
    else if(cmd=="/")result=y/x;
    else result=pow(y,x);
    if(isfinite(result))binary(result); else calc.error=true;
  }
  else if(cmd=="ln" || cmd=="log" || cmd=="exp" || cmd=="sqrt" ||
          cmd=="sin" || cmd=="cos" || cmd=="tan") {
    double x=calc.st[0], result=0;
    if(cmd=="ln")result=log(x);
    else if(cmd=="log")result=log10(x);
    else if(cmd=="exp")result=calc.early && fabs(x-log(2.02))<1e-12 ? 2.0 : exp(x);
    else if(cmd=="sqrt")result=sqrt(x);
    else if(cmd=="sin")result=sin(x*3.14159265358979323846/180.0);
    else if(cmd=="cos")result=cos(x*3.14159265358979323846/180.0);
    else result=tan(x*3.14159265358979323846/180.0);
    if(isfinite(result))calc.st[0]=result; else calc.error=true;
  }
  else {Serial0.printf("Unknown command: %s\r\n",tok);return;}
  calc.step++;status();
}
void runLine(char *line) {
  if(strcmp(line,"motion")==0 || strcmp(line,"motion status")==0) {
    Serial0.printf("QMI8658 %s, I2C=0x%02X, ID=0x%02X, SDA%d SCL%d, INT1 GPIO46 / INT2 GPIO45\r\n",
       motion.available()?"READY":"NOT DETECTED",motion.address(),motion.identity(),motion.sda(),motion.scl());
    Serial0.println("ESP32-S3 deep-sleep wake requires RTC GPIO0..21: onboard IMU interrupt pins are NOT eligible.");
    return;
  }
  if(strcmp(line,"motion scan")==0){motion.scan(Serial0);return;}
  if(strcmp(line,"motion sample")==0){motionSample();return;}
  if(strncmp(line,"sleep motion",12)==0 && (line[12]=='\0'||line[12]==' ')) {
    unsigned seconds=15;char extra=0;
    if(line[12]==' ' && (sscanf(line+13,"%u %c",&seconds,&extra)!=1 || seconds<1 || seconds>120)) {
      Serial0.println("Usage: sleep motion [1..120 seconds]");return;
    }
    lightSleepMotion(seconds);return;
  }

  if(strncmp(line,"sleep",5)==0 && (line[5]=='\0'||line[5]==' ')) {
    if(strcmp(line,"sleep")==0 || strcmp(line,"sleep status")==0) {
      Serial0.printf("Sleep: auto=%s, light idle=%lu ms, deep idle=%lu ms, deep timer=%lu s\r\n",
        autoSleep?"ON":"OFF",(unsigned long)AUTO_LIGHT_IDLE_MS,
        (unsigned long)AUTO_DEEP_IDLE_MS,(unsigned long)AUTO_DEEP_WAKE_SEC);
      Serial0.println("Light sleep: timer wake, RAM retained. Deep sleep: RTC snapshot + timer reboot.");
      Serial0.println("No USB wake from deep sleep. UART bytes can be lost during light sleep.");
      return;
    }
    if(strcmp(line,"sleep auto on")==0) { autoSleep=true;lastInputMs=millis();Serial0.println("Auto sleep ON (experimental; UART bytes may be lost)");return; }
    if(strcmp(line,"sleep auto off")==0) { autoSleep=false;lastInputMs=millis();Serial0.println("Auto sleep OFF");return; }
    if(strncmp(line,"sleep light",11)==0 && (line[11]=='\0'||line[11]==' ')) {
      const char* p=line+11;while(*p==' ')++p;
      char* end=nullptr;
      unsigned long sec=*p?strtoul(p,&end,10):2UL;
      if((*p && (end==p || *end)) || sec<1 || sec>3600) {Serial0.println("Usage: sleep light [1..3600 seconds]");return;}
      Serial0.printf("Light sleep %lu s; waking by timer...\r\n",sec);
      if(lightSleepMs(uint32_t(sec)*1000U))Serial0.println("Woke from light sleep (RAM unchanged)");
      lastInputMs=millis();show();return;
    }
    if(strncmp(line,"sleep deep",10)==0 && (line[10]=='\0'||line[10]==' ')) {
      const char* p=line+10;while(*p==' ')++p;
      char* end=nullptr;
      unsigned long sec=*p?strtoul(p,&end,10):10UL;
      if((*p && (end==p || *end)) || sec<1 || sec>86400) {Serial0.println("Usage: sleep deep [1..86400 seconds]");return;}
      deepSleepSeconds(uint32_t(sec));return;
    }
    Serial0.println("Usage: sleep status | sleep light [seconds] | sleep deep [seconds] | sleep auto on/off");return;
  }

  // Prism is a separate, intentionally black graphical mode: no LVGL draws
  // while the rotating X result is visible. Core RPN state is unaffected.
  if (strcmp(line,"display prism")==0 || strcmp(line,"display normal")==0) {
    const bool wanted = strcmp(line,"display prism")==0;
    if(prismMode != wanted) {
      prismMode = wanted;
      lcd->setRotation(prismMode ? 2 : 0);
#if HP35_USE_LVGL
      hp35prism::setActive(prismMode);
#else
      if(prismMode) hp35prism::reset();
#endif
    }
    Serial0.printf("Display mode: %s\r\n",prismMode ? "PRISM | black + rotating X | 180 degrees + horizontal mirror" : "NORMAL | four tabs | 0 degrees");
    if(!prismMode) show();
    return;
  }
  if (bcdMode && strcmp(line,"allregs")==0) {
    Serial0.print(bcdCalc.registerDump().c_str());
    Serial0.print(debugTrace.hidden().c_str());return;
  }
  if (bcdMode && (strcmp(line,"hidden")==0 || strcmp(line,"trace hidden")==0)) {
    Serial0.print(debugTrace.hidden().c_str());return;
  }
  if (strcmp(line,"view trace")==0) {currentView=View::Trace;traceScreen=true;show();return;}
  if (strcmp(line,"view stack")==0) {currentView=View::Stack;traceScreen=false;show();return;}
  if (strcmp(line,"view regs")==0) {currentView=View::Regs;traceScreen=false;show();return;}
  if (strcmp(line,"view hidden")==0) {currentView=View::Hidden;traceScreen=false;show();return;}
  if (bcdMode && strncmp(line,"trace",5)==0 && (line[5]==0||line[5]==' ')) {
    std::string arg(line);std::vector<std::string> words;
    size_t pos=0;
    while(pos<arg.size()){
      while(pos<arg.size() && arg[pos]==' ')pos++;
      size_t end=arg.find(' ',pos);if(end==std::string::npos)end=arg.size();
      if(end>pos)words.emplace_back(arg.substr(pos,end-pos));pos=end;
    }
    if(words.size()==1||words[1]=="show") {}
    else if(words[1]=="next") {if(!debugTrace.next())Serial0.println("End of trace");}
    else if(words[1]=="prev") {if(!debugTrace.previous())Serial0.println("Start of trace");}
    else if(words[1]=="jump" && words.size()==3) {
      auto n=strtoul(words[2].c_str(),nullptr,10);
      if(!debugTrace.jump(n))Serial0.println("Invalid trace step");
    } else {
      try {
        const auto val=words.size()>2?hp35::BCDNumber::fromDecimal(words[2]):bcdCalc.st[0];
        std::string error;
        if(!debugTrace.capture(words[1],val,error))Serial0.printf("Trace: %s\r\n",error.c_str());
        else Serial0.print(debugTrace.summary().c_str());
      }catch(const std::exception& e){Serial0.printf("Trace: %s\r\n",e.what());}
    }
    Serial0.print(debugTrace.current().c_str());
    if(currentView==View::Trace || currentView==View::Hidden)show();return;
  }
  if (bcdMode && strncmp(line,"bench ",6)==0) {
    std::string arg(line+6);std::vector<std::string> w;
    size_t pos=0;while(pos<arg.size()) {while(pos<arg.size()&&arg[pos]==' ')pos++;
      size_t end=arg.find(' ',pos);if(end==std::string::npos)end=arg.size();
      if(end>pos)w.push_back(arg.substr(pos,end-pos));pos=end;}
    if(w.size()<2||w.size()>3){Serial0.println("Usage: bench OP VALUE [COUNT]");return;}
    unsigned count=w.size()==3?strtoul(w[2].c_str(),nullptr,10):10;
    if(count==0||count>100){Serial0.println("COUNT must be 1..100");return;}
    try {
      hp35::BCDNumber v=hp35::BCDNumber::fromDecimal(w[1]);
      volatile unsigned checksum=0;
      int64_t start=esp_timer_get_time();
      for(unsigned i=0;i<count;i++){
        hp35::BCDNumber result;
        if(w[0]=="sin")result=hp35::sinScientificBCD(v);
        else if(w[0]=="cos")result=hp35::cosScientificBCD(v);
        else if(w[0]=="atan")result=hp35::atanScientificBCD(v);
        else if(w[0]=="ln")result=hp35::lnScientificBCD(v);
        else if(w[0]=="exp")result=hp35::expScientificBCD(v);
        else if(w[0]=="sqrt")result=hp35::sqrtScientificBCD(v);
        else {Serial0.println("Bench supports sin cos atan ln exp sqrt");return;}
        checksum+=result.mantissa[0];
      }
      int64_t elapsed=esp_timer_get_time()-start;
      Serial0.printf("%s: %u runs, %lld us total, %lld us/run (checksum %u)\r\n",w[0].c_str(),
        count,(long long)elapsed,(long long)(elapsed/count),checksum);
    }catch(const std::exception& e){Serial0.printf("Bench: %s\r\n",e.what());}
    return;
  }

  if(strcmp(line,"mode bcd")==0) { traceScreen=false; currentView=View::Stack; bcdMode=true; bcdCalc.reset(); Serial0.println("BCD RPN mode ON"); status(); return; }
  if(strcmp(line,"mode float")==0) { traceScreen=false; currentView=View::Stack; bcdMode=false; calc = Calc{}; Serial0.println("Legacy floating-point bootstrap ON"); status(); return; }
  // Special two-word setting; otherwise every token is an immediate command.
  if(strcmp(line,"early on")==0) {if(bcdMode){Serial0.println("Early-ROM shim only available in legacy float mode");return;}calc.early=true;Serial0.println("Early example shim ON");return;}
  if(strcmp(line,"early off")==0) {if(bcdMode){Serial0.println("Early-ROM shim only available in legacy float mode");return;}calc.early=false;Serial0.println("Early example shim OFF");return;}
  char *save=nullptr;
  for(char *tok=strtok_r(line," \t",&save);tok;tok=strtok_r(nullptr," \t",&save))execute(tok);
}
void setup() {
  Serial0.begin(115200);
  const bool wokeDeep=(esp_reset_reason()==ESP_RST_DEEPSLEEP);
  const bool restored=wokeDeep && restoreRetainedState();
  lastInputMs=millis();
  delay(2000);
  Serial0.println("HP-35 UART0 TEST");
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, HIGH);
  lcd->begin(40000000);
  lcd->setRotation(prismMode ? 2 : 0);
  if(motion.begin()) Serial0.printf("QMI8658 accelerometer ready (0x%02X)\r\n",motion.address());
  else Serial0.println("QMI8658 not detected at boot. Try motion scan.");
#if HP35_USE_LVGL
  hp35ui::begin(lcd);
#endif
  hp35prism::begin(lcd);
#if HP35_USE_LVGL
  if(prismMode)hp35prism::setActive(true);
#endif
  show();
  delay(300);
  Serial0.println("\r\n=== HP-35 ESP32-S3 phase 7.16 | MOTION SLEEP ===");
  Serial0.printf("Boot: %s; RTC snapshot: %s\r\n",wokeDeep?"deep-sleep wake":"power/reset",restored?"RESTORED":"not restored");
  if(restored)Serial0.println("BCD stack, entry mode, error, view and prism restored; trace history not retained.");
  help();status();
  Serial0.print("> ");
}
void loop() {
  if(prismMode) {
    std::string value;
    if(bcdMode) value=bcdCalc.st[0].toString();
    else {char buffer[40];snprintf(buffer,sizeof(buffer),"%.10g",calc.st[0]);value=buffer;}
    if(!value.empty() && value.back()=='.') value.pop_back();
    hp35prism::tick(value.c_str(), bcdMode?bcdCalc.error:calc.error);
  }
#if HP35_USE_LVGL
  // LVGL always owns the LCD, including while prism animation is active.
  hp35ui::tick();
#endif
  while(Serial0.available()) {
    char c=(char)Serial0.read();
    if(c=='\r'||c=='\n') {
      if(inputLen) {input[inputLen]='\0'; Serial0.println();runLine(input);inputLen=0;}
      Serial0.print("> ");
    } else if(c=='\b'||c==127) {if(inputLen)inputLen--;}
    else if(c>=32 && inputLen<sizeof(input)-1) {input[inputLen++]=c;Serial0.write(c);}
    lastInputMs=millis();
  }
  if(autoSleep && inputLen==0 && !Serial0.available()) {
    const uint32_t idle=millis()-lastInputMs;
    if(idle>=AUTO_DEEP_IDLE_MS)deepSleepSeconds(AUTO_DEEP_WAKE_SEC);
    else if(idle>=AUTO_LIGHT_IDLE_MS)lightSleepMs(AUTO_LIGHT_SLICE_MS);
  }
}
