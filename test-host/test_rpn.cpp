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
 if(!r.error) {std::cerr<<"Division by zero must report error";return 1;}
 // A failed operation must not block the next valid one or clear the stack.
 if(r.st[0].toString()!="0." || r.st[1].toString()!="1.") {
   std::cerr<<"Invalid division corrupted the stack";return 1;
 }
 if(!r.execute("5") || r.error || r.st[0].toString()!="5.") {
   std::cerr<<"Automatic recovery from division by zero failed";return 1;
 }
 if(r.execute("1*") || r.error) {
   std::cerr<<"Unsupported command must fail without latching an error";return 1;
 }
 if(!r.execute("6") || r.error || r.st[0].toString()!="6.") {
   std::cerr<<"Automatic recovery from unsupported token failed";return 1;
 }
 if(!r.execute("clr") || r.error) {std::cerr<<"CLR must still work";return 1;}
 if(!r.execute("1") || !r.execute("ln") || r.error || r.st[0].toString()!="0.") {std::cerr<<"Scientific ln(1) failed";return 1;}
 if(!r.execute("sin") || r.error || r.st[0].toString()!="0.") {std::cerr<<"Trig sin(0) failed";return 1;}
 std::cout << "PASS error auto-recovery, ln, and trigonometry\n";
}
