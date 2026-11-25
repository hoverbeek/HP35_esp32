#pragma once
#include <stdint.h>

namespace hp35power {
constexpr uint32_t MAGIC=0x48503550u; // "HP5P"
constexpr uint32_t FORMAT=1;
struct Stats {
  uint32_t magic=0,format=0;
  uint32_t timerLightCount=0, irqLightCount=0;
  uint32_t gpioWakes=0,timerWakes=0,otherWakes=0;
  uint32_t sleepFailures=0,deepEntries=0,deepWakes=0;
  uint64_t timerLightUs=0, irqLightUs=0;
  uint32_t lastWakeCause=0;
  uint32_t lastSleepMs=0;
  uint32_t lastDeepRequestedSec=0;
};
inline void reset(Stats& s) {
 s=Stats{};s.magic=MAGIC;s.format=FORMAT;
}
inline bool valid(const Stats& s) {return s.magic==MAGIC && s.format==FORMAT;}
inline void timerLight(Stats& s,uint32_t elapsedMs,bool ok) {
 if(!ok){++s.sleepFailures;return;}
 ++s.timerLightCount;
 s.timerLightUs+=uint64_t(elapsedMs)*1000u;
 s.lastSleepMs=elapsedMs;
}
inline void irqLight(Stats& s,uint32_t elapsedMs,uint32_t cause,bool ok,uint32_t gpioCause,uint32_t timerCause) {
 if(!ok){++s.sleepFailures;return;}
 ++s.irqLightCount;s.irqLightUs+=uint64_t(elapsedMs)*1000u;
 s.lastSleepMs=elapsedMs;s.lastWakeCause=cause;
 if(cause==gpioCause)++s.gpioWakes;
 else if(cause==timerCause)++s.timerWakes;
 else ++s.otherWakes;
}
inline void deepStart(Stats& s,uint32_t requestedSeconds) {
 ++s.deepEntries;s.lastDeepRequestedSec=requestedSeconds;
}
inline void deepWake(Stats& s) {++s.deepWakes;}
}