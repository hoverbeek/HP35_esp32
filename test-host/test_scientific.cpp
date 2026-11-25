#include "scientific_bcd.h"
#include "rpn_bcd.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace hp35;
int main(){
  int checks=0;
  auto test=[&](const std::string& op, const std::string& input, const std::string& expected){
    BCDNumber n=BCDNumber::fromDecimal(input);
    BCDNumber result=(op=="sqrt"?sqrtScientificBCD(n):op=="ln"?lnScientificBCD(n):op=="log"?logScientificBCD(n):expScientificBCD(n));
    std::string got=result.toString();
    std::cout<<op<<"("<<input<<") = "<<got<<"\n";
    if(got!=expected)throw std::runtime_error("Expected "+expected+", got "+got);
    ++checks;
  };
  test("sqrt","4","2.");test("sqrt","2","1.414213562");
  test("sqrt","0.25",".5");test("sqrt","100","10.");
  test("ln","1","0.");test("log","100","2.");test("exp","0","1.");
  test("exp","1","2.718281828");
  test("ln","2.02",".7030975114");
  auto composed=expScientificBCD(lnScientificBCD(BCDNumber::fromDecimal("2.02")));
  std::cout<<"exp(ln(2.02))="<<composed.toString()<<"\n";
  if(composed.toString()!="2.02")throw std::runtime_error("round trip failed");
  ++checks;
  auto result=powScientificBCD(BCDNumber::fromDecimal("2"),BCDNumber::fromDecimal("3"));
  std::cout<<"2^3="<<result.toString()<<"\n";
  if(result.toString()!="8.")throw std::runtime_error("pow failed");
  ++checks;
  RpnBCD r;
  for(auto token:{"2.02","ln","exp"}){if(!r.execute(token))throw std::runtime_error(r.errorMessage);}
  if(r.st[0].toString()!="2.02")throw std::runtime_error("RPN scientific round trip failed");
  ++checks;
  std::cout<<"PASS "<<checks<<" scientific tests\n";
}
