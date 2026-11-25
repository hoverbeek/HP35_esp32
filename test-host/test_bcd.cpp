#include "bcd.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace hp35;
static void eq(const BCDNumber& a,const char* expected){
 if(a.toString()!=expected){std::cerr<<"Expected "<<expected<<" got "<<a.toString()<<'\n';std::abort();}
}
int main(){
 int checks=0;
 for(int a=0;a<=9;a++)for(int b=0;b<=9;b++)for(int c=0;c<=1;c++){
   auto add=addNibble(a,b,c);assert(add.digit==(a+b+c)%10 && add.carry==(a+b+c>=10));
   int s=a-b-c;auto sub=subNibble(a,b,c);assert(sub.digit==(s+10)%10 && sub.carry==(s<0)); checks+=2;
 }
 assert((addDigits({9,9},{1})==Digits{1,0,0}));checks++;
 assert((subDigits({1,0,0},{1})==Digits{0,9,9}));checks++;
 assert((multiplyDigits({1,2,3},{4,5})==Digits{5,5,3,5}));checks++;
 auto d=divideDigits({1,2,3,4},{1,2});assert((d.quotient==Digits{1,0,2})&&(d.remainder==Digits{1,0}));checks++;
 eq(BCDNumber::fromDecimal("2.02"),"2.02");checks++;
 eq(BCDNumber::fromDecimal("-.00012"),"-.00012");checks++;
 eq(BCDNumber::fromDecimal("9.9999999995"),"10.");checks++;
 auto a=BCDNumber::fromDecimal("2.02"),b=BCDNumber::fromDecimal("3");
 eq(addBCD(a,b),"5.02");checks++;
 eq(subBCD(a,b),"-.98");checks++;
 eq(mulBCD(a,b),"6.06");checks++;
 eq(divBCD(b,b),"1.");checks++;
 eq(divBCD(BCDNumber::fromDecimal("1"),BCDNumber::fromDecimal("8")),".125");checks++;
 eq(addBCD(BCDNumber::fromDecimal("9999999999"),BCDNumber::fromDecimal("1")),"1.E+10");checks++;
 auto n=a.nibbles();assert(n[0]==0&&n[1]==2&&n[2]==0&&n[3]==2&&n.size()==14);checks++;
 bool error=false;try{divideDigits({1},{0});}catch(const std::domain_error&){error=true;}assert(error);checks++;
 std::cout<<"PASS: "<<checks<<" BCD checks (including 400 nibble cases)\n";
}
