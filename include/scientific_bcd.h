#pragma once
#include "bcd.h"
#include <array>
#include <string>
#include <vector>

namespace hp35 {
// Extended decimal scratch precision: 4 integer + 22 fractional BCD digits.
struct FixedBCD {
  static constexpr int IntegerDigits=4, FractionDigits=22, Width=26;
  std::array<uint8_t, Width> digits{};
  bool negative=false;
  static FixedBCD parse(const std::string&);
  static FixedBCD fromBCD(const BCDNumber&);
  static FixedBCD zero();
  static FixedBCD one();
  bool isZero() const;
  FixedBCD neg() const;
  int compare(const FixedBCD&) const;
  FixedBCD add(const FixedBCD&) const;
  FixedBCD sub(const FixedBCD&) const;
  FixedBCD shiftRight(int) const;
  FixedBCD multiply(const FixedBCD&) const;
  FixedBCD divide(const FixedBCD&) const;
  BCDNumber toBCD() const;
};
// Capture real decimal pseudo-division / pseudo-multiplication steps.
// These are our algorithm scratch values, NOT HP-35 ROM registers.
struct ScientificStep {
  unsigned index = 0;
  unsigned decade = 0;
  unsigned repetition = 0;
  std::string action;
  FixedBCD before, after, factor, accumulatorBefore, accumulatorAfter;
};
struct ScientificTrace {
  std::string algorithm;
  std::vector<ScientificStep> steps;
};
BCDNumber traceLnScientificBCD(const BCDNumber&, ScientificTrace&);
BCDNumber traceExpScientificBCD(const BCDNumber&, ScientificTrace&);
BCDNumber sqrtScientificBCD(const BCDNumber&);
BCDNumber lnScientificBCD(const BCDNumber&);
BCDNumber logScientificBCD(const BCDNumber&);
BCDNumber expScientificBCD(const BCDNumber&);
BCDNumber powScientificBCD(const BCDNumber&,const BCDNumber&);
BCDNumber piScientificBCD();
} // namespace hp35
