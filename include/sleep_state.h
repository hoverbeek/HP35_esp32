#pragma once
#include "rpn_bcd.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hp35sleep {
constexpr uint32_t MAGIC=0x48503335u; // HP35
constexpr uint32_t FORMAT=1u;
struct Number {
  uint8_t digits[10];
  int32_t exponent;
  uint8_t negative;
  uint8_t reserved[3];
};
struct Snapshot {
  uint32_t magic, format, checksum;
  Number stack[4];
  uint32_t steps;
  uint8_t error, nextReplace, bcdMode, early;
  uint8_t prism, view, autoSleep, reserved;
  double floatStack[4];
  uint32_t floatSteps;
  uint8_t floatError;
  char message[95];
};
inline uint32_t checksum(const Snapshot& s) {
  // Stable FNV-1a over every byte except the checksum field itself.
  const auto* p=reinterpret_cast<const uint8_t*>(&s);
  uint32_t hash=2166136261u;
  for(size_t i=0;i<sizeof(Snapshot);++i) {
    if(i>=offsetof(Snapshot,checksum) && i<offsetof(Snapshot,checksum)+sizeof(s.checksum))continue;
    hash=(hash^p[i])*16777619u;
  }
  return hash;
}
inline void captureBCD(Snapshot& s,const hp35::RpnBCD& r) {
  for(size_t i=0;i<4;++i) {
    for(size_t j=0;j<10;++j)s.stack[i].digits[j]=r.st[i].mantissa[j];
    s.stack[i].exponent=r.st[i].exponent;
    s.stack[i].negative=r.st[i].negative?1:0;
  }
  s.steps=static_cast<uint32_t>(r.step);
  s.error=r.error?1:0;
  s.nextReplace=r.nextNumberReplaces()?1:0;
  std::strncpy(s.message,r.errorMessage.c_str(),sizeof(s.message)-1);
  s.message[sizeof(s.message)-1]=0;
}
inline bool valid(const Snapshot& s) {
  if(s.magic!=MAGIC || s.format!=FORMAT || checksum(s)!=s.checksum)return false;
  if(s.bcdMode>1 || s.error>1 || s.nextReplace>1 || s.prism>1 || s.view>3 || s.autoSleep>1 || s.floatError>1)return false;
  for(const auto& n:s.stack) {
    if(n.exponent < -99 || n.exponent > 99 || n.negative>1)return false;
    for(uint8_t d:n.digits)if(d>9)return false;
  }
  return true;
}
inline bool restoreBCD(const Snapshot& s,hp35::RpnBCD& r) {
  if(!valid(s))return false;
  r.reset();
  for(size_t i=0;i<4;++i) {
    for(size_t j=0;j<10;++j)r.st[i].mantissa[j]=s.stack[i].digits[j];
    r.st[i].exponent=s.stack[i].exponent;
    r.st[i].negative=s.stack[i].negative!=0;
  }
  r.step=s.steps; r.error=s.error!=0;
  r.setNextNumberReplaces(s.nextReplace!=0);
  r.errorMessage=s.message;
  return true;
}
}
