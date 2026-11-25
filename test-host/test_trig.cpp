#include "trig_bcd.h"
#include "rpn_bcd.h"
#include <iostream>
#include <stdexcept>
using namespace hp35;
int main(){
  int count=0;
  auto check=[&](const char* name, BCDNumber got, const char* expected){
    std::cout<<name<<" = "<<got.toString()<<"\n";
    if(got.toString()!=expected)throw std::runtime_error(std::string(name)+": expected "+expected+", got "+got.toString());
    ++count;
  };
  auto v=[](const char* s){return BCDNumber::fromDecimal(s);};
  check("sin(0)",sinScientificBCD(v("0")),"0.");
  check("sin(30)",sinScientificBCD(v("30")),".5");
  check("cos(60)",cosScientificBCD(v("60")),".5");
  check("sin(90)",sinScientificBCD(v("90")),"1.");
  check("cos(90)",cosScientificBCD(v("90")),"0.");
  check("sin(180)",sinScientificBCD(v("180")),"0.");
  check("cos(180)",cosScientificBCD(v("180")),"-1.");
  check("sin(270)",sinScientificBCD(v("270")),"-1.");
  check("sin(390)",sinScientificBCD(v("390")),".5");
  check("tan(45)",tanScientificBCD(v("45")),"1.");
  check("atan(1)",atanScientificBCD(v("1")),"45.");
  check("asin(.5)",asinScientificBCD(v("0.5")),"30.");
  check("acos(.5)",acosScientificBCD(v("0.5")),"60.");
  check("asin(-1)",asinScientificBCD(v("-1")),"-90.");
  check("acos(-1)",acosScientificBCD(v("-1")),"180.");
  RpnBCD r;
  for(auto token:{"30","sin"}) if(!r.execute(token))throw std::runtime_error(r.errorMessage);
  check("RPN 30 sin",r.st[0],".5");
  std::cout<<"PASS "<<count<<" trig checks\n";
}
