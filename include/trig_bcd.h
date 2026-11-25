#pragma once
#include "scientific_bcd.h"
#include <vector>
namespace hp35 {
struct SinCosBCD { BCDNumber sine, cosine; };
// Records one state AFTER each decimal micro-rotation. Values are full 26-nibble
// fixed-point BCD, not floating-point approximations.
struct CordicStep {
  unsigned index = 0;
  unsigned decade = 0;
  unsigned repetition = 0;
  bool positive = false;
  FixedBCD x, y, z;
};
struct CordicTrace {
  std::vector<CordicStep> steps;
  std::string algorithm;
  std::string note;
};
// Diagnostic entry points do not alter any RPN stack registers.
// Forward traces follow the standard rotation branch (angles >1 degree
// and more than 1 degree from 90). Other angles use a shortcut; no rotations.
SinCosBCD traceSinCosBCD(const BCDNumber& degrees, CordicTrace& out);
BCDNumber traceAtanBCD(const BCDNumber& input, CordicTrace& out);
SinCosBCD sinCosScientificBCD(const BCDNumber& degrees);
BCDNumber sinScientificBCD(const BCDNumber& degrees);
BCDNumber cosScientificBCD(const BCDNumber& degrees);
BCDNumber tanScientificBCD(const BCDNumber& degrees);
BCDNumber atanScientificBCD(const BCDNumber& ratio);
BCDNumber asinScientificBCD(const BCDNumber& ratio);
BCDNumber acosScientificBCD(const BCDNumber& ratio);
}
