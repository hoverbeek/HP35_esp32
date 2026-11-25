#include "debug_trace.h"
#include <cassert>
#include <iostream>
using namespace hp35;
int main(){
  DebugTrace t;std::string err;
  auto x=BCDNumber::fromDecimal("2.02");
  assert(t.capture("ln",x,err));
  assert(t.scientificMode && t.count()>0 && t.result.toString()==lnScientificBCD(x).toString());
  assert(t.current().find("pseudo-division")!=std::string::npos);
  assert(t.hidden().find("work.before")!=std::string::npos);
  assert(t.jump(t.count()));assert(!t.next());
  auto logResult=t.result;
  assert(t.capture("exp",logResult,err));
  assert(t.scientificMode && t.count()>0);
  assert(t.result.toString()==expScientificBCD(logResult).toString());
  assert(t.result.toString()=="2.02");
  assert(t.current().find("pseudo-multiplication")!=std::string::npos || t.current().find("range reduce")!=std::string::npos);
  assert(t.capture("sin",BCDNumber::fromDecimal("30"),err));
  assert(!t.scientificMode && t.count()==100);
  std::string old=t.summary();
  assert(!t.capture("ln",BCDNumber::zero(),err));
  assert(t.summary()==old);
  assert(t.capture("exp",BCDNumber::zero(),err));
  assert(t.count()==0 && t.result.toString()=="1.");
  std::cout<<"Phase 6 scientific trace tests passed.\n";
}
