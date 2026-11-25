#include "rpn_bcd.h"
#include <cstdlib>
#include <iostream>
#include <string>
using hp35::RpnBCD;
void test(const char *program,const char *expected) {
 RpnBCD r; std::string text(program); size_t start=0;
 while (start < text.size()) {
   auto end=text.find(' ',start); auto token=text.substr(start,end==std::string::npos?end:end-start);
   if(!r.execute(token)) { std::cerr << "Failed token: " << token << " " << r.errorMessage << "\n"; std::exit(1); }
   if(end==std::string::npos) break;
   start=end+1;
 }
 if(r.st[0].toString()!=expected) { std::cerr<<program<<" expected "<<expected<<" got "<<r.st[0].toString()<<"\n";std::exit(1); }
 std::cout<<"PASS "<<program<<" -> "<<expected<<"\n";
}
int main(){
 test("2 3 +","5.");
 test("2 enter 3 +","5.");
 test("2.02 3 *","6.06");
 test("9 4 -","5.");
 test("1 8 /",".125");
 test("3 chs","-3.");
 test("5 enter 2 *","10.");
 test("1 2 3 4 +","7.");
 RpnBCD r; r.execute("1");r.execute("0"); // literal 0 is valid
 if(r.execute("/")) {std::cerr<<"Expected division by zero";return 1;}
 if(!r.error || r.execute("5") || !r.execute("clr") || r.error) {std::cerr<<"Error latch failure";return 1;}
 if(!r.execute("1") || !r.execute("ln") || r.error || r.st[0].toString()!="0.") {std::cerr<<"Scientific ln(1) failed";return 1;}
 if(!r.execute("sin") || r.error || r.st[0].toString()!="0.") {std::cerr<<"Trig sin(0) failed";return 1;}
 std::cout << "PASS error latch, ln, and trigonometry\n";
}
