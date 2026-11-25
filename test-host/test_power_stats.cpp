#include "power_stats.h"
#include <cassert>
#include <iostream>

int main() {
  hp35power::Stats s{};
  assert(!hp35power::valid(s));
  hp35power::reset(s);
  assert(hp35power::valid(s));
  hp35power::timerLight(s,125,true);
  hp35power::timerLight(s,100,true);
  hp35power::timerLight(s,0,false);
  assert(s.timerLightCount==2 && s.timerLightUs==225000);
  assert(s.sleepFailures==1);
  hp35power::irqLight(s,2133,7,true,7,4);
  hp35power::irqLight(s,15000,4,true,7,4);
  hp35power::irqLight(s,0,0,false,7,4);
  assert(s.irqLightCount==2 && s.irqLightUs==17133000);
  assert(s.gpioWakes==1 && s.timerWakes==1);
  assert(s.sleepFailures==2);
  assert(s.lastSleepMs==15000);
  hp35power::deepStart(s,10);
  hp35power::deepWake(s);
  assert(s.deepEntries==1 && s.deepWakes==1 && s.lastDeepRequestedSec==10);
  hp35power::reset(s);
  assert(s.timerLightCount==0 && s.deepEntries==0);
  std::cout<<"power stats tests OK\n";
}
