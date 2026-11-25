#pragma once
#include "bcd.h"
#include <array>
#include <string>

namespace hp35 {
// Text-debugger RPN semantics based on Level 2 v0.8:
// numbers are standalone operands; adjacent numbers imply ENTER.
// Stack X,Y,Z,T and T replication after binary operations.
// This is not yet the full HP-35 keystroke entry state machine.
class RpnBCD {
public:
  std::array<BCDNumber, 4> st{};
  bool error = false;
  std::string errorMessage;
  unsigned long step = 0;
  RpnBCD();
  void reset();
  bool execute(const std::string &token);
  std::string registerDump() const;
private:
  bool nextNumberReplacesX = true;
  void lift();
  void drop(const BCDNumber& result);
};
}
