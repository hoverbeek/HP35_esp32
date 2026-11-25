#include "sleep_state.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  hp35::RpnBCD calc;
  assert(calc.execute("2.02"));
  assert(calc.execute("3"));
  assert(calc.execute("*"));
  hp35sleep::Snapshot saved{};
  saved.magic=hp35sleep::MAGIC;
  saved.format=hp35sleep::FORMAT;
  saved.bcdMode=1; saved.prism=1; saved.view=2;
  hp35sleep::captureBCD(saved,calc);
  saved.checksum=hp35sleep::checksum(saved);
  assert(hp35sleep::valid(saved));
  hp35::RpnBCD restored;
  assert(hp35sleep::restoreBCD(saved,restored));
  assert(restored.st[0].toString()==calc.st[0].toString());
  assert(restored.st[0].toString()=="6.06");
  assert(restored.step==calc.step);
  assert(restored.nextNumberReplaces()==calc.nextNumberReplaces());
  assert(restored.execute("4"));
  assert(restored.st[0].toString()=="4.");
  saved.stack[0].digits[2]=8; // intentional mutation invalidates checksum
  assert(!hp35sleep::valid(saved));
  std::cout << "Sleep snapshot: RTC state round-trip and checksum tests OK\n";
}
