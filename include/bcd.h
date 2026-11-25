#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Clean-room port of HP-35 Level 2 v0.8 src/bcd.js.
// The digit arrays contain decimal digits in the range 0..9, MS digit first.
// No host floating-point arithmetic is used here.
namespace hp35 {
using Digits = std::vector<uint8_t>;
struct NibbleResult { uint8_t digit, carry; }; // carry = borrow on subtraction
NibbleResult addNibble(uint8_t a, uint8_t b, uint8_t carryIn = 0);
NibbleResult subNibble(uint8_t a, uint8_t b, uint8_t borrowIn = 0);
int compareDigits(const Digits& a, const Digits& b);
Digits addDigits(const Digits& a, const Digits& b);
Digits subDigits(const Digits& a, const Digits& b); // unsigned, a >= b
Digits multiplyDigits(const Digits& a, const Digits& b);
struct Division { Digits quotient, remainder; };
Division divideDigits(const Digits& numerator, const Digits& denominator);

constexpr int MANTISSA_DIGITS = 10;
constexpr int REGISTER_NIBBLES = 14;
struct BCDNumber {
  std::array<uint8_t, MANTISSA_DIGITS> mantissa{};
  int exponent = 0;
  bool negative = false;
  static BCDNumber zero();
  static BCDNumber fromDecimal(const std::string& text);
  static BCDNumber normalized(const Digits& digits, int firstDigitExponent, bool negative=false);
  bool isZero() const;
  BCDNumber negate() const;
  std::array<uint8_t, REGISTER_NIBBLES> nibbles() const;
  std::string toString() const;
};
BCDNumber addBCD(const BCDNumber& a, const BCDNumber& b);
BCDNumber subBCD(const BCDNumber& a, const BCDNumber& b);
BCDNumber mulBCD(const BCDNumber& a, const BCDNumber& b);
BCDNumber divBCD(const BCDNumber& a, const BCDNumber& b);
} // namespace hp35
