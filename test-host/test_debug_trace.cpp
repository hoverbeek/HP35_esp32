#include "debug_trace.h"
#include <cassert>
#include <iostream>
int main(){
 hp35::DebugTrace t;std::string err;
 assert(t.capture("sin",hp35::BCDNumber::fromDecimal("30"),err));
 assert(t.count()==100);assert(t.cursor==1);
 assert(t.current().find("work.x")!=std::string::npos);
 assert(t.hidden().find("work.z")!=std::string::npos);
 assert(t.hidden().find("BCD=")!=std::string::npos);
 assert(t.next()&&t.cursor==2);assert(t.previous()&&t.cursor==1);
 assert(t.jump(100));assert(!t.next());assert(!t.jump(101));
 auto result=t.result.toString();
 assert(t.capture("cos",hp35::BCDNumber::fromDecimal("60"),err));
 assert(t.result.toString()==result);
 assert(t.capture("sin",hp35::BCDNumber::fromDecimal("90"),err));
 assert(t.count()==0 && t.hidden().find("no CORDIC scratch")!=std::string::npos);
 assert(!t.capture("bad",hp35::BCDNumber::fromDecimal("1"),err));
 std::cout<<"Phase 5 debugger tests passed.\n";
}
