#pragma once
#include "trig_bcd.h"
#include <string>
namespace hp35 {
// Inspectable algorithm scratch state, not historical HP-35 ROM registers.
class DebugTrace {
 public:
  CordicTrace trace;
  ScientificTrace scientific;
  bool scientificMode = false;
  BCDNumber argument, result;
  std::string operation;
  size_t cursor = 0;
  bool active = false;
  // Never modifies the calculator RPN stack.
  bool capture(const std::string& operation, const BCDNumber& input, std::string& error);
  bool jump(size_t oneBasedStep);
  bool next();
  bool previous();
  size_t count() const { return scientificMode ? scientific.steps.size() : trace.steps.size(); }
  std::string summary() const;
  std::string current() const;
  std::string hidden() const;
};
std::string fixedDecimal(const FixedBCD& v);
std::string fixedNibbles(const FixedBCD& v);
}
