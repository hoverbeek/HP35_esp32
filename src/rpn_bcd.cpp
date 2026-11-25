#include "rpn_bcd.h"
#include "scientific_bcd.h"
#include "trig_bcd.h"
#include <algorithm>
#include <cctype>
#include <exception>

namespace hp35 {
RpnBCD::RpnBCD() { reset(); }
void RpnBCD::reset() {
  for (auto &v : st) v = BCDNumber::zero();
  error = false; errorMessage.clear(); step = 0;
  nextNumberReplacesX = true;
}
void RpnBCD::lift() {
  st[3] = st[2]; st[2] = st[1]; st[1] = st[0];
}
void RpnBCD::drop(const BCDNumber &result) {
  st[0] = result; st[1] = st[2]; st[2] = st[3]; // T replicates
}
bool RpnBCD::execute(const std::string &token) {
  std::string cmd = token;
  std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  if (cmd == "reset" || cmd == "clr") { reset(); return true; }
  if (cmd == "stack" || cmd == "regs" || cmd == "help") return true;
  // Errors are reported for the failed operation only. A subsequent
  // command starts with a clean error status while preserving the stack.
  // Invalid operations will still report their own errors below.
  error = false;
  errorMessage.clear();
  try {
    if (!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])) ||
        token[0] == '.' || token[0] == '-' || token[0] == '+')) {
      // '+' and '-' by themselves are operators, not literals.
      if (cmd != "+" && cmd != "-") {
        auto number = BCDNumber::fromDecimal(token);
        if (!nextNumberReplacesX) lift();
        st[0] = number;
        nextNumberReplacesX = false;
        ++step;
        return true;
      }
    }
    if (cmd == "enter") { lift(); nextNumberReplacesX = true; }
    else if (cmd == "swap" || cmd == "x<>y") { std::swap(st[0], st[1]); nextNumberReplacesX = false; }
    else if (cmd == "rdn") {
      auto oldX = st[0]; st[0] = st[1]; st[1] = st[2]; st[2] = st[3]; st[3] = oldX;
      nextNumberReplacesX = false;
    }
    else if (cmd == "chs") { st[0] = st[0].negate(); nextNumberReplacesX = false; }
    else if (cmd == "clx") { st[0] = BCDNumber::zero(); nextNumberReplacesX = true; }
    else if (cmd == "pi") {
      if (!nextNumberReplacesX) lift();
      st[0] = piScientificBCD(); nextNumberReplacesX = false;
    }
    else if (cmd == "sqrt" || cmd == "ln" || cmd == "log" || cmd == "exp" || cmd == "sin" || cmd == "cos" || cmd == "tan" || cmd == "asin" || cmd == "acos" || cmd == "atan") {
      if (cmd == "sqrt") st[0] = sqrtScientificBCD(st[0]);
      else if (cmd == "ln") st[0] = lnScientificBCD(st[0]);
      else if (cmd == "log") st[0] = logScientificBCD(st[0]);
      else if (cmd == "exp") st[0] = expScientificBCD(st[0]);
      else if (cmd == "sin") st[0] = sinScientificBCD(st[0]);
      else if (cmd == "cos") st[0] = cosScientificBCD(st[0]);
      else if (cmd == "tan") st[0] = tanScientificBCD(st[0]);
      else if (cmd == "asin") st[0] = asinScientificBCD(st[0]);
      else if (cmd == "acos") st[0] = acosScientificBCD(st[0]);
      else st[0] = atanScientificBCD(st[0]);
      nextNumberReplacesX = false;
    }
    else if (cmd == "+" || cmd == "-" || cmd == "*" || cmd == "x" || cmd == "/" || cmd == "pow") {
      BCDNumber result;
      if (cmd == "+") result = addBCD(st[1], st[0]);
      else if (cmd == "-") result = subBCD(st[1], st[0]);
      else if (cmd == "*" || cmd == "x") result = mulBCD(st[1], st[0]);
      else if (cmd == "/") result = divBCD(st[1], st[0]);
      else result = powScientificBCD(st[1], st[0]);
      drop(result); nextNumberReplacesX = false;
    } else {
      errorMessage = "Unsupported command in BCD phase 4: " + token;
      return false; // Unsupported scientific functions don't silently use float
    }
    ++step; return true;
  } catch (const std::exception &e) {
    error = true;
    errorMessage = e.what();
    return false;
  }
}
std::string RpnBCD::registerDump() const {
  std::string text;
  const char* names = "XYZT";
  for (int i=0; i<4; ++i) {
    text += names[i]; text += "=";
    for (auto d : st[i].nibbles()) text += static_cast<char>('0' + d);
    text += " "; text += st[i].toString(); text += "\n";
  }
  return text;
}
}
