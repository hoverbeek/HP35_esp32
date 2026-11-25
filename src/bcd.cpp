#include "bcd.h"
#include <algorithm>
#include <stdexcept>

namespace hp35 {
namespace {
void digit(uint8_t value) { if(value > 9) throw std::out_of_range("Invalid BCD nibble"); }
void validate(const Digits& a) { for (auto d : a) digit(d); }
Digits trim(Digits d) {
  if(d.empty()) return {0};
  auto first = std::find_if(d.begin(), d.end(), [](uint8_t n){return n != 0;});
  return first == d.end() ? Digits{0} : Digits(first,d.end());
}
int lz(const Digits& d) {
  size_t n=0; while(n<d.size() && d[n]==0) ++n; return static_cast<int>(n);
}
Digits aligned(const BCDNumber& value, int highExponent) {
  Digits result(MANTISSA_DIGITS+2,0);
  const int offset=highExponent-value.exponent;
  for(int i=0;i<MANTISSA_DIGITS;i++) {
    int idx=i+offset;
    if(idx>=0 && idx<static_cast<int>(result.size())) result[idx]=value.mantissa[i];
  }
  return result;
}
}
NibbleResult addNibble(uint8_t a,uint8_t b,uint8_t carryIn) {
  digit(a);digit(b);if(carryIn>1) throw std::out_of_range("Invalid carry");
  unsigned sum=a+b+carryIn;
  return {static_cast<uint8_t>(sum>=10 ? sum-10:sum),static_cast<uint8_t>(sum>=10)};
}
NibbleResult subNibble(uint8_t a,uint8_t b,uint8_t borrowIn) {
  digit(a);digit(b);if(borrowIn>1) throw std::out_of_range("Invalid borrow");
  int diff=static_cast<int>(a)-b-borrowIn;
  return {static_cast<uint8_t>(diff<0 ? diff+10:diff),static_cast<uint8_t>(diff<0)};
}
int compareDigits(const Digits& a,const Digits& b) {
  validate(a);validate(b);
  Digits x=trim(a),y=trim(b);
  if(x.size()!=y.size())return x.size()<y.size()?-1:1;
  for(size_t i=0;i<x.size();i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;
  return 0;
}
Digits addDigits(const Digits& a,const Digits& b) {
  validate(a);validate(b);
  size_t size=std::max(a.size(),b.size());
  Digits result(size,0);uint8_t carry=0;
  for(size_t i=0;i<size;i++) {
    auto da=i<a.size()?a[a.size()-1-i]:0;
    auto db=i<b.size()?b[b.size()-1-i]:0;
    auto step=addNibble(da,db,carry);
    result[size-1-i]=step.digit;carry=step.carry;
  }
  if(carry)result.insert(result.begin(),carry);
  return result;
}
Digits subDigits(const Digits& a,const Digits& b) {
  if(compareDigits(a,b)<0)throw std::out_of_range("Unsigned BCD subtraction would be negative");
  size_t size=std::max(a.size(),b.size());
  Digits result(size,0);uint8_t borrow=0;
  for(size_t i=0;i<size;i++) {
    auto da=i<a.size()?a[a.size()-1-i]:0;
    auto db=i<b.size()?b[b.size()-1-i]:0;
    auto step=subNibble(da,db,borrow);
    result[size-1-i]=step.digit;borrow=step.carry;
  }
  return result;
}
Digits multiplyDigits(const Digits& a,const Digits& b) {
  validate(a);validate(b);Digits x=trim(a),y=trim(b),result{0};
  for(size_t pos=0;pos<y.size();pos++) {
    uint8_t d=y[y.size()-1-pos];Digits partial{0};
    for(unsigned repeat=0;repeat<d;repeat++)partial=addDigits(partial,x);
    if(d) {partial.insert(partial.end(),pos,0);result=addDigits(result,partial);}
  }
  return trim(result);
}
Division divideDigits(const Digits& numerator,const Digits& denominator) {
  validate(numerator);validate(denominator);
  auto n=trim(numerator),d=trim(denominator);
  if(d==Digits{0})throw std::domain_error("Division by zero");
  Digits q,remainder{0};
  for(uint8_t next:n) {
    remainder.push_back(next);remainder=trim(remainder);
    uint8_t qdigit=0;
    while(compareDigits(remainder,d)>=0) {
      remainder=trim(subDigits(remainder,d));
      if(++qdigit>9)throw std::logic_error("BCD long division invariant");
    }
    q.push_back(qdigit);
  }
  return {trim(q),remainder};
}
BCDNumber BCDNumber::zero(){return normalized({0},0);}
BCDNumber BCDNumber::normalized(const Digits& digits,int firstDigitExponent,bool isNegative) {
  validate(digits);
  auto first=lz(digits);
  BCDNumber result;
  if(first==static_cast<int>(digits.size()))return result;
  result.exponent=firstDigitExponent-first;
  Digits significant(digits.begin()+first,digits.end());
  for(int i=0;i<MANTISSA_DIGITS;i++)result.mantissa[i]=i<static_cast<int>(significant.size())?significant[i]:0;
  if(significant.size()>MANTISSA_DIGITS && significant[MANTISSA_DIGITS]>=5) {
    uint8_t carry=1;
    for(int i=MANTISSA_DIGITS-1;i>=0 && carry;i--) {
      auto n=addNibble(result.mantissa[i],0,carry);
      result.mantissa[i]=n.digit;carry=n.carry;
    }
    if(carry){result.mantissa.fill(0);result.mantissa[0]=1;++result.exponent;}
  }
  if(result.exponent>99)throw std::out_of_range("BCD overflow");
  if(result.exponent< -99)throw std::out_of_range("BCD underflow");
  result.negative=isNegative;
  return result;
}
BCDNumber BCDNumber::fromDecimal(const std::string& source) {
  if(source.empty())throw std::invalid_argument("Empty decimal literal");
  size_t index=0;bool neg=false;
  if(source[index]=='+' || source[index]=='-'){neg=source[index]=='-';++index;}
  if(index==source.size())throw std::invalid_argument("Invalid decimal literal");
  Digits digits;int integerDigits=0;bool dot=false;
  for(;index<source.size();index++){
    char c=source[index];
    if(c=='.' && !dot){dot=true;continue;}
    if(c<'0'||c>'9')throw std::invalid_argument("Invalid decimal literal");
    digits.push_back(static_cast<uint8_t>(c-'0'));
    if(!dot)++integerDigits;
  }
  if(digits.empty())throw std::invalid_argument("Invalid decimal literal");
  return normalized(digits,integerDigits-1,neg);
}
bool BCDNumber::isZero()const {
  return std::all_of(mantissa.begin(),mantissa.end(),[](uint8_t d){return d==0;});
}
BCDNumber BCDNumber::negate()const {BCDNumber r=*this;if(!r.isZero())r.negative=!r.negative;return r;}
std::array<uint8_t,REGISTER_NIBBLES> BCDNumber::nibbles()const {
  std::array<uint8_t,REGISTER_NIBBLES> r{};
  r[0]=negative?9:0;
  for(int i=0;i<MANTISSA_DIGITS;i++)r[i+1]=mantissa[i];
  r[11]=exponent<0?9:0;
  int e=exponent<0?-exponent:exponent;
  r[12]=static_cast<uint8_t>(e/10);r[13]=static_cast<uint8_t>(e%10);
  return r;
}
std::string BCDNumber::toString()const {
  if(isZero())return "0.";
  std::string m;for(auto d:mantissa)m.push_back(static_cast<char>('0'+d));
  std::string prefix=negative?"-":"";
  if(exponent>9||exponent< -4){
    auto frac=m.substr(1);while(!frac.empty()&&frac.back()=='0')frac.pop_back();
    int e=exponent<0?-exponent:exponent;
    return prefix+m.substr(0,1)+"."+frac+"E"+(exponent>=0?"+":"-")+(e<10?"0":"")+std::to_string(e);
  }
  const int cutoff=exponent+1;std::string value;
  if(cutoff<=0)value="."+std::string(-cutoff,'0')+m;
  else if(cutoff>=10)value=m+std::string(cutoff-10,'0')+".";
  else value=m.substr(0,cutoff)+"."+m.substr(cutoff);
  while(!value.empty()&&value.back()=='0')value.pop_back();
  if(value.find('.')==std::string::npos)value+='.';
  return prefix+value;
}
BCDNumber addBCD(const BCDNumber& a,const BCDNumber& b){
  int e=std::max(a.exponent,b.exponent);
  Digits left=aligned(a,e),right=aligned(b,e),result;
  bool negative;
  if(a.negative==b.negative){result=addDigits(left,right);negative=a.negative;}
  else if(compareDigits(left,right)>=0){result=subDigits(left,right);negative=a.negative;}
  else {result=subDigits(right,left);negative=b.negative;}
  return BCDNumber::normalized(result,e+static_cast<int>(result.size())-(MANTISSA_DIGITS+2),negative);
}
BCDNumber subBCD(const BCDNumber& a,const BCDNumber& b){return addBCD(a,b.negate());}
BCDNumber mulBCD(const BCDNumber& a,const BCDNumber& b){
  if(a.isZero()||b.isZero())return BCDNumber::zero();
  Digits product=multiplyDigits(Digits(a.mantissa.begin(),a.mantissa.end()),Digits(b.mantissa.begin(),b.mantissa.end()));
  int exponent=a.exponent+b.exponent+static_cast<int>(product.size())-19;
  return BCDNumber::normalized(product,exponent,a.negative!=b.negative);
}
BCDNumber divBCD(const BCDNumber& a,const BCDNumber& b){
  if(b.isZero())throw std::domain_error("Division by zero");
  if(a.isZero())return BCDNumber::zero();
  Digits denominator(b.mantissa.begin(),b.mantissa.end());
  Digits remainder(a.mantissa.begin(),a.mantissa.end()),quotient;
  int exponent=a.exponent-b.exponent;
  if(compareDigits(remainder,denominator)<0){remainder.push_back(0);remainder=trim(remainder);--exponent;}
  for(int place=0;place<=MANTISSA_DIGITS;place++){
    uint8_t qdigit=0;
    while(compareDigits(remainder,denominator)>=0){
      remainder=trim(subDigits(remainder,denominator));
      if(++qdigit>9)throw std::logic_error("BCD quotient digit exceeded nine");
    }
    quotient.push_back(qdigit);remainder.push_back(0);remainder=trim(remainder);
  }
  return BCDNumber::normalized(quotient,exponent,a.negative!=b.negative);
}
} // namespace hp35
