// Decimal scientific operations ported from Level 2 v0.8 fixed-bcd.js / cordic.js.
// Runtime arithmetic uses BCD nibbles only; no floating point or HP ROM data.
#include "scientific_bcd.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace hp35 {
namespace {
Digits vec(const std::array<uint8_t,26>& a){return Digits(a.begin(),a.end());}
std::array<uint8_t,26> pad(const Digits& a) {
  if(a.size()>26)throw std::out_of_range("BCD scratch overflow");
  std::array<uint8_t,26> out{};
  std::copy(a.begin(),a.end(),out.begin()+(26-a.size()));
  return out;
}
FixedBCD makeFixed(const Digits& d,bool neg=false){FixedBCD f;f.digits=pad(d);f.negative=neg&&!f.isZero();return f;}
const char* const FACTORS[] = {
 "0.693147180559945309417232121458", "0.095310179804324860043952123281",
 "0.009950330853168082848215357544", "0.000999500333083533166809398921",
 "0.000099995000333308335333166681", "0.000009999950000333330833353333",
 "0.000000999999500000333333083334", "0.000000099999995000000333333308",
 "0.000000009999999950000000333333", "0.000000000999999999500000000333",
 "0.000000000099999999995000000000", "0.000000000009999999999950000000",
 "0.000000000000999999999999500000"};
FixedBCD ln10(){return FixedBCD::parse("2.302585092994045684017991454684");}
FixedBCD logMantissa(const BCDNumber& v, ScientificTrace* trace=nullptr) {
  bool one=v.mantissa[0]==1;
  for(int i=1;i<10;i++)one=one&&(v.mantissa[i]==0);
  if(one)return FixedBCD::zero();
  std::string text;
  text += char('0'+v.mantissa[0]); text += '.';
  for(int i=1;i<10;i++)text+=char('0'+v.mantissa[i]);
  FixedBCD current=FixedBCD::parse(text), accumulated=FixedBCD::zero(), ten=FixedBCD::parse("10");
  for(int j=0;j<13;j++){
    FixedBCD factor=FixedBCD::parse(FACTORS[j]);
    int count=0;
    while(true){
      FixedBCD candidate=current.add(current.shiftRight(j));
      if(candidate.compare(ten)>0)break;
      if(++count>20)throw std::logic_error("Logarithm pseudo-quotient did not converge");
      auto before=current; auto accBefore=accumulated;
      current=candidate; accumulated=accumulated.add(factor);
      if(trace)trace->steps.push_back({unsigned(trace->steps.size()+1),unsigned(j),unsigned(count),
          "decimal pseudo-division",before,current,factor,accBefore,accumulated});
    }
  }
  // First order residual correction; final residual is below 10^-12.
  return ln10().sub(accumulated).sub(ten.sub(current).shiftRight(1));
}
FixedBCD logarithmFixed(const BCDNumber& v, ScientificTrace* trace=nullptr) {
  if(v.negative||v.isZero())throw std::domain_error("Logarithm domain: X must be positive");
  FixedBCD result=logMantissa(v,trace), factor=ln10();
  for(int e=0;e<v.exponent;e++){
    auto before=result; result=result.add(factor);
    if(trace) trace->steps.push_back({unsigned(trace->steps.size()+1),0,1,
       "add ln(10) for exponent",before,result,factor,before,result});
  }
  for(int e=0;e>v.exponent;e--){
    auto before=result; result=result.sub(factor);
    if(trace) trace->steps.push_back({unsigned(trace->steps.size()+1),0,1,
       "subtract ln(10) for exponent",before,result,factor,before,result});
  }
  return result;
}
struct ExponentialResult {FixedBCD value;int decade;};
ExponentialResult exponentialFixed(FixedBCD residual, ScientificTrace* trace=nullptr) {
  const FixedBCD L=ln10(),zero=FixedBCD::zero();
  int decade=0;
  while(residual.compare(L)>=0){
    auto before=residual; residual=residual.sub(L);
    if(trace)trace->steps.push_back({unsigned(trace->steps.size()+1),0,1,
       "range reduce -ln(10)",before,residual,L,FixedBCD::zero(),FixedBCD::zero()});
    if(++decade>99)throw std::out_of_range("Exponential overflow");
  }
  while(residual.compare(zero)<0){
    auto before=residual; residual=residual.add(L);
    if(trace)trace->steps.push_back({unsigned(trace->steps.size()+1),0,1,
       "range reduce +ln(10)",before,residual,L,FixedBCD::zero(),FixedBCD::zero()});
    if(--decade< -99)throw std::out_of_range("Exponential underflow");
  }
  FixedBCD value=FixedBCD::one();
  for(int j=0;j<13;j++){
    FixedBCD factor=FixedBCD::parse(FACTORS[j]);
    int count=0;
    while(residual.compare(factor)>=0){
      if(++count>20)throw std::logic_error("Exponential pseudo-quotient did not converge");
      auto priorValue=value, priorResidual=residual;
      value=value.add(value.shiftRight(j));
      residual=residual.sub(factor);
      if(trace)trace->steps.push_back({unsigned(trace->steps.size()+1),unsigned(j),unsigned(count),
         "decimal pseudo-multiplication",priorValue,value,factor,priorResidual,residual});
    }
  }
  auto before=value, beforeResidual=residual;
  value=value.add(value.multiply(residual));
  if(trace)trace->steps.push_back({unsigned(trace->steps.size()+1),13,1,
    "first-order residual correction",before,value,FixedBCD::zero(),beforeResidual,residual});
  return {value,decade};
}
BCDNumber adjustDecade(const ExponentialResult& result){
  BCDNumber n=result.value.toBCD();
  return BCDNumber::normalized(Digits(n.mantissa.begin(),n.mantissa.end()),n.exponent+result.decade);
}
}
FixedBCD FixedBCD::zero(){return {};}
FixedBCD FixedBCD::one(){return parse("1");}
bool FixedBCD::isZero()const{return std::all_of(digits.begin(),digits.end(),[](uint8_t v){return v==0;});}
FixedBCD FixedBCD::neg()const{FixedBCD f=*this;if(!f.isZero())f.negative=!f.negative;return f;}
FixedBCD FixedBCD::parse(const std::string& s){
  if(s.empty())throw std::invalid_argument("Empty fixed BCD literal");
  size_t p=0;bool neg=false;
  if(s[p]=='+'||s[p]=='-'){neg=s[p]=='-';p++;}
  if(p>=s.size())throw std::invalid_argument("Fixed BCD literal missing digits");
  std::string whole,frac; bool dot=false;
  for(;p<s.size();p++){
    char c=s[p];if(c=='.'&&!dot){dot=true;continue;}
    if(c<'0'||c>'9')throw std::invalid_argument("Invalid fixed BCD literal");
    (dot?frac:whole).push_back(c);
  }
  if(whole.empty()&&frac.empty())throw std::invalid_argument("Fixed BCD literal missing digits");
  if(whole.empty())whole="0";
  if(whole.size()>4){
    if(whole.substr(0,whole.size()-4).find_first_not_of('0')!=std::string::npos)
      throw std::out_of_range("Fixed BCD integer overflow");
    whole=whole.substr(whole.size()-4);
  }
  std::string fixed=std::string(4-whole.size(),'0')+whole+frac.substr(0,22);
  fixed+=std::string(26-fixed.size(),'0');
  Digits d;for(char c:fixed)d.push_back(uint8_t(c-'0'));
  if(frac.size()>22&&frac[22]>='5')d=addDigits(d,{1});
  return makeFixed(d,neg);
}
FixedBCD FixedBCD::fromBCD(const BCDNumber& n){
  if(n.isZero())return zero();
  int point=n.exponent+1;
  std::string mantissa;
  for(uint8_t digit:n.mantissa)mantissa+=char('0'+digit);
  std::string text;
  if(point<=0){
    if(-point>=FractionDigits)return zero();
    text="0."+std::string(-point,'0')+mantissa;
  }else if(point>=10){
    if(point>IntegerDigits)throw std::out_of_range("CORDIC scratch integer range");
    text=mantissa+std::string(point-10,'0');
  }else text=mantissa.substr(0,point)+"."+mantissa.substr(point);
  return parse(std::string(n.negative?"-":"")+text);
}
int FixedBCD::compare(const FixedBCD& o)const{
  if(isZero()&&o.isZero())return 0;
  if(negative!=o.negative)return negative?-1:1;
  int cmp=compareDigits(vec(digits),vec(o.digits));return negative?-cmp:cmp;
}
FixedBCD FixedBCD::add(const FixedBCD& o)const{
  if(negative==o.negative)return makeFixed(addDigits(vec(digits),vec(o.digits)),negative);
  int cmp=compareDigits(vec(digits),vec(o.digits));
  return cmp>=0 ? makeFixed(subDigits(vec(digits),vec(o.digits)),negative):makeFixed(subDigits(vec(o.digits),vec(digits)),o.negative);
}
FixedBCD FixedBCD::sub(const FixedBCD& o)const{return add(o.neg());}
FixedBCD FixedBCD::shiftRight(int n)const{
  if(n<0)throw std::invalid_argument("Negative BCD shift");
  if(n>=Width)return zero();
  FixedBCD out;out.negative=negative;
  for(int i=n;i<Width;i++)out.digits[i]=digits[i-n];
  if(out.isZero())out.negative=false;
  return out;
}
FixedBCD FixedBCD::multiply(const FixedBCD& o)const{
  Digits prod=multiplyDigits(vec(digits),vec(o.digits));
  if(prod.size()<=FractionDigits)return zero();
  Digits kept(prod.begin(),prod.end()-FractionDigits);
  if(prod[prod.size()-FractionDigits]>=5)kept=addDigits(kept,{1});
  return makeFixed(kept,negative!=o.negative);
}
FixedBCD FixedBCD::divide(const FixedBCD& o)const{
  if(o.isZero())throw std::domain_error("Fixed BCD division by zero");
  if(isZero())return zero();
  Digits top=vec(digits);top.insert(top.end(),FractionDigits,0);
  return makeFixed(divideDigits(top,vec(o.digits)).quotient,negative!=o.negative);
}
BCDNumber FixedBCD::toBCD()const{
  int first=0;while(first<Width&&digits[first]==0)first++;
  if(first==Width)return BCDNumber::zero();
  return BCDNumber::normalized(Digits(digits.begin()+first,digits.end()),IntegerDigits-1-first,negative);
}
BCDNumber sqrtScientificBCD(const BCDNumber& n){
  if(n.negative)throw std::domain_error("Square root requires nonnegative X");
  if(n.isZero())return BCDNumber::zero();
  const int parity=((n.exponent%2)+2)%2;
  const int rootExponent=(n.exponent-parity)/2;
  Digits source(n.mantissa.begin(),n.mantissa.end());
  Digits remainder{0},root{0};int cursor=0;
  for(int place=0;place<12;place++){
    Digits pair;
    for(int j=0;j<(place==0?(parity==0?1:2):2);j++)pair.push_back(cursor<(int)source.size()?source[cursor++]:0);
    if(place==0&&pair.size()==1)pair.insert(pair.begin(),0);
    remainder.insert(remainder.end(),pair.begin(),pair.end());
    Digits twentyRoot=addDigits(Digits(root.begin(),root.end()),Digits(root.begin(),root.end()));
    twentyRoot.push_back(0); // 20 * root
    int selected=0;Digits product{0};
    for(int d=1;d<=9;d++){
      Digits trial=multiplyDigits(addDigits(twentyRoot,{(uint8_t)d}),{(uint8_t)d});
      if(compareDigits(trial,remainder)>0)break;
      selected=d;product=trial;
    }
    remainder=subDigits(remainder,product);
    root.push_back(uint8_t(selected));
  }
  return BCDNumber::normalized(Digits(root.begin()+1,root.end()),rootExponent);
}
BCDNumber lnScientificBCD(const BCDNumber& n){return logarithmFixed(n).toBCD();}
BCDNumber traceLnScientificBCD(const BCDNumber& n, ScientificTrace& out){
  ScientificTrace t; t.algorithm="ln";
  auto answer=logarithmFixed(n,&t).toBCD();out=std::move(t);return answer;
}
BCDNumber traceExpScientificBCD(const BCDNumber& n, ScientificTrace& out){
  ScientificTrace t; t.algorithm="exp";
  BCDNumber answer;
  if(n.isZero())answer=BCDNumber::fromDecimal("1");
  else {
    if(n.exponent>2)throw std::out_of_range("Exponential outside HP-35 range");
    answer=adjustDecade(exponentialFixed(FixedBCD::fromBCD(n),&t));
  }
  out=std::move(t);return answer;
}
BCDNumber logScientificBCD(const BCDNumber& n){
  if(n.negative||n.isZero())throw std::domain_error("Logarithm domain: X must be positive");
  bool power=n.mantissa[0]==1;
  for(int i=1;i<10;i++)power=power&&n.mantissa[i]==0;
  if(power)return BCDNumber::fromDecimal(std::to_string(n.exponent));
  return divBCD(lnScientificBCD(n),BCDNumber::fromDecimal("2.302585092994045684017991454684"));
}
BCDNumber expScientificBCD(const BCDNumber& n){
  if(n.isZero())return BCDNumber::fromDecimal("1");
  if(n.exponent>2)throw std::out_of_range("Exponential outside HP-35 range");
  return adjustDecade(exponentialFixed(FixedBCD::fromBCD(n)));
}
BCDNumber powScientificBCD(const BCDNumber& base,const BCDNumber& exponent){
  if(base.negative||base.isZero())throw std::domain_error("Power requires positive base");
  if(exponent.isZero())return BCDNumber::fromDecimal("1");
  if(exponent.exponent>2)throw std::out_of_range("Power exponent out of working range");
  FixedBCD argument=logarithmFixed(base).multiply(FixedBCD::fromBCD(exponent));
  return adjustDecade(exponentialFixed(argument));
}
BCDNumber piScientificBCD(){return BCDNumber::fromDecimal("3.14159265358979323846264338328");}
} // namespace hp35
