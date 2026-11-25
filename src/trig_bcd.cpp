// Level 2 v0.8 decimal-radix CORDIC port. Runtime uses BCD digits only.
// All angles are in DEGREES. Original clean-room algorithms, not HP ROM.
#include "trig_bcd.h"
#include <stdexcept>
#include <algorithm>
namespace hp35 {
namespace {
FixedBCD fx(const char* str) { return FixedBCD::parse(str); }
const FixedBCD& zero() { static const auto v=FixedBCD::zero();return v; }
const FixedBCD& one() { static const auto v=FixedBCD::one();return v; }
const FixedBCD& ninety() { static const auto v=fx("90");return v; }
const FixedBCD& oneEighty() { static const auto v=fx("180");return v; }
const FixedBCD& threeSixty() { static const auto v=fx("360");return v; }
const FixedBCD& radPerDegree() { static const auto v=fx("0.01745329251994329576923690768");return v; }
const FixedBCD& degPerRadian() { static const auto v=fx("57.2957795130823208767981548141");return v; }
const FixedBCD& gainInverse() { static const auto v=fx("0.675836158043892966518808871234");return v; }
const char* ATAN[] = {
 "45.000000000000000000000000000000",
 "5.710593137499642512695881348234",
 "0.572938697683485926841422479047",
 "0.057295760414500608997075374019",
 "0.005729577932209639031243933397",
 "0.000572957795111724615598100023",
 "0.000057295779513063222283627139",
 "0.000005729577951308212989086644",
 "0.000000572957795130823189669388",
 "0.000000057295779513082320857700",
 "0.000000005729577951308232087661",
 "0.000000000572957795130823208768"};
struct Quadrant {FixedBCD angle;int sinSign;int cosSign;};
// Long division of integral decimal digits handles input exponents through +99
// without first converting the number into 26-digit fixed scratch space.
FixedBCD modulo360(const BCDNumber& n) {
 if(n.isZero())return zero();
 const int point=n.exponent+1;
 Digits whole, fraction;
 if(point<=0) {
   whole={0};
   int leading=-point;
   if(leading>24)leading=24;
   fraction.insert(fraction.end(),leading,0);
   fraction.insert(fraction.end(),n.mantissa.begin(),n.mantissa.end());
 } else {
   for(int i=0;i<point;i++)whole.push_back(i<10?n.mantissa[i]:0);
   for(int i=point;i<10;i++)fraction.push_back(n.mantissa[i]);
 }
 Digits rem=divideDigits(whole,Digits{3,6,0}).remainder;
 std::string literal;
 for(auto d:rem)literal+=char('0'+d);
 literal+='.';
 for(auto d:fraction)literal+=char('0'+d);
 FixedBCD angle=FixedBCD::parse(literal);
 if(n.negative&&!angle.isZero()) angle=threeSixty().sub(angle);
 return angle;
}
Quadrant quadrant(const BCDNumber& n) {
 FixedBCD a=modulo360(n);int ss=1,cs=1;
 if(a.compare(oneEighty())>0){a=threeSixty().sub(a);ss=-1;}
 if(a.compare(ninety())>0){a=oneEighty().sub(a);cs=-1;}
 return {a,ss,cs};
}
struct FixedSinCos {FixedBCD sine,cosine;};
FixedSinCos rotate(const FixedBCD& angle, CordicTrace* trace=nullptr) {
 FixedBCD x=gainInverse(),y=zero(),z=angle;
 for(int j=0;j<12;j++){
  const auto delta=fx(ATAN[j]);const int repeats=(j==0)?1:9;
  for(int i=0;i<repeats;i++){
   auto sx=x.shiftRight(j),sy=y.shiftRight(j);
   bool positive=z.compare(zero())>=0;
   auto nx=positive?x.sub(sy):x.add(sy);
   auto ny=positive?y.add(sx):y.sub(sx);
   z=positive?z.sub(delta):z.add(delta);
   x=nx;y=ny;
   if (trace) trace->steps.push_back({static_cast<unsigned>(trace->steps.size()+1),
          static_cast<unsigned>(j),static_cast<unsigned>(i+1),positive,x,y,z});
  }
 }
 auto radians=z.multiply(radPerDegree());
 return {y.add(x.multiply(radians)),x.sub(y.multiply(radians))};
}
FixedSinCos smallAngle(const FixedBCD& angle) {
 auto r=angle.multiply(radPerDegree());
 auto r2=r.multiply(r),r3=r.multiply(r2),r4=r2.multiply(r2),r5=r3.multiply(r2);
 auto sine=r.sub(r3.multiply(fx("0.16666666666666666666666667"))).add(r5.multiply(fx("0.00833333333333333333333333")));
 auto cosine=one().sub(r2.multiply(fx("0.5"))).add(r4.multiply(fx("0.04166666666666666666666667")));
 return {sine,cosine};
}
FixedBCD atanUnit(const FixedBCD& ratio, CordicTrace* trace=nullptr) {
 if(ratio.isZero())return zero();
 if(ratio.compare(one())==0)return fx("45");
 if(ratio.compare(fx("0.001"))<=0){
  auto t2=ratio.multiply(ratio),t3=ratio.multiply(t2),t5=t3.multiply(t2),t7=t5.multiply(t2);
  return ratio.sub(t3.multiply(fx("0.33333333333333333333333333")))
   .add(t5.multiply(fx("0.2")))
   .sub(t7.multiply(fx("0.14285714285714285714285714"))).multiply(degPerRadian());
 }
 auto x=one(),y=ratio,angle=zero();
 for(int j=0;j<12;j++){
  auto delta=fx(ATAN[j]);int count=(j==0)?1:9;
  for(int i=0;i<count;i++){
   auto sx=x.shiftRight(j),sy=y.shiftRight(j);
   bool positive=y.compare(zero())>=0;
   if(positive){auto nx=x.add(sy);y=y.sub(sx);x=nx;angle=angle.add(delta);}
   else {auto nx=x.sub(sy);y=y.add(sx);x=nx;angle=angle.sub(delta);}
   if (trace) trace->steps.push_back({static_cast<unsigned>(trace->steps.size()+1),
           static_cast<unsigned>(j),static_cast<unsigned>(i+1),positive,x,y,angle});
  }
 }
 return angle.add(y.divide(x).multiply(degPerRadian()));
}
// Square-root extraction on 4+22 fixed decimal digits, preserving 22 fractional places.
FixedBCD sqrtFixed(const FixedBCD& n) {
 if(n.negative)throw std::domain_error("Square root domain");
 if(n.isZero())return zero();
 Digits rem{0}, root{0};
 // group the 4 integer digits and 22 fractional digits into 13 pairs;
 // append 26 extra zero digits to obtain 13 guard-pair positions.
 for(int place=0;place<26;place++){
  Digits pair={static_cast<uint8_t>(place<13?n.digits[2*place]:0),static_cast<uint8_t>(place<13?n.digits[2*place+1]:0)};
  rem.insert(rem.end(),pair.begin(),pair.end());
  Digits twenty=addDigits(root,root);twenty.push_back(0);
  int selected=0;Digits selectedProduct{0};
  for(int d=1;d<=9;d++){
   auto trial=multiplyDigits(addDigits(twenty,Digits{static_cast<uint8_t>(d)}),Digits{static_cast<uint8_t>(d)});
   if(compareDigits(trial,rem)>0)break;
   selected=d;selectedProduct=trial;
  }
  rem=subDigits(rem,selectedProduct);root.push_back(static_cast<uint8_t>(selected));
 }
 // Input has 2 integer digits' worth of pairs before decimal point;
 // root has 2 integer decimal digits + 24 fractional guard digits.
 // Round to the 4+22 fixed output representation.
 Digits kept={0,0};
 kept.insert(kept.end(),root.begin()+1,root.begin()+25);
 if(root[25]>=5)kept=addDigits(kept,Digits{1});
 FixedBCD out; if(kept.size()>26)throw std::out_of_range("Square root scratch overflow");
 std::copy(kept.begin(),kept.end(),out.digits.begin()+(26-kept.size()));
 return out;
}
FixedBCD asinFixed(const BCDNumber& n){
 BCDNumber mag=n;mag.negative=false;
 if(mag.exponent>0||(mag.exponent==0&&(mag.mantissa[0]>1||
  std::any_of(mag.mantissa.begin()+1,mag.mantissa.end(),[](uint8_t d){return d!=0;}))))
  throw std::domain_error("Inverse trig domain: |X| > 1");
 if(mag.isZero())return zero();
 if(mag.exponent==0&&mag.mantissa[0]==1)return n.negative?ninety().neg():ninety();
 if(mag.exponent<=-11)return FixedBCD::fromBCD(mulBCD(n,BCDNumber::fromDecimal("57.2957795130823208768")));
 auto x=FixedBCD::fromBCD(mag);
 auto other=sqrtFixed(one().sub(x.multiply(x)));
 auto a=x.compare(other)>0?ninety().sub(atanUnit(other.divide(x))):atanUnit(x.divide(other));
 return n.negative?a.neg():a;
}
} // namespace
SinCosBCD sinCosScientificBCD(const BCDNumber& n) {
 if(n.exponent<=-11&&!n.isZero())return {mulBCD(n,BCDNumber::fromDecimal("0.01745329251994329577")),BCDNumber::fromDecimal("1")};
 auto q=quadrant(n);
 if(q.angle.isZero())return {BCDNumber::zero(),BCDNumber::fromDecimal(q.cosSign<0?"-1":"1")};
 if(q.angle.compare(ninety())==0)return {BCDNumber::fromDecimal(q.sinSign<0?"-1":"1"),BCDNumber::zero()};
 auto complement=ninety().sub(q.angle);
 bool nearZero=q.angle.compare(one())<=0, nearNinety=complement.compare(one())<=0;
 FixedSinCos result;
 if(nearZero||nearNinety){
  result=smallAngle(nearZero?q.angle:complement);
  if(nearNinety)std::swap(result.sine,result.cosine);
 }else result=rotate(q.angle);
 return {(q.sinSign<0?result.sine.neg():result.sine).toBCD(),
         (q.cosSign<0?result.cosine.neg():result.cosine).toBCD()};
}
BCDNumber sinScientificBCD(const BCDNumber& n){return sinCosScientificBCD(n).sine;}
BCDNumber cosScientificBCD(const BCDNumber& n){return sinCosScientificBCD(n).cosine;}
BCDNumber tanScientificBCD(const BCDNumber& n){auto v=sinCosScientificBCD(n);if(v.cosine.isZero())throw std::domain_error("Tangent undefined at 90 degrees");return divBCD(v.sine,v.cosine);}
BCDNumber atanScientificBCD(const BCDNumber& n){
 if(n.isZero())return BCDNumber::zero();
 if(n.exponent<=-11)return mulBCD(n,BCDNumber::fromDecimal("57.2957795130823208768"));
 BCDNumber positive=n;positive.negative=false;
 FixedBCD ratio;bool inverted;
 if(positive.exponent>3){ratio=FixedBCD::fromBCD(divBCD(BCDNumber::fromDecimal("1"),positive));inverted=true;}
 else {auto f=FixedBCD::fromBCD(positive);inverted=f.compare(one())>0;ratio=inverted?one().divide(f):f;}
 auto a=atanUnit(ratio);auto adjusted=inverted?ninety().sub(a):a;
 return (n.negative?adjusted.neg():adjusted).toBCD();
}
BCDNumber asinScientificBCD(const BCDNumber& n){
 if(n.exponent<=-11&&!n.isZero())return mulBCD(n,BCDNumber::fromDecimal("57.2957795130823208768"));
 return asinFixed(n).toBCD();
}
BCDNumber acosScientificBCD(const BCDNumber& n){return ninety().sub(asinFixed(n)).toBCD();}
} // namespace hp35

namespace hp35 {
SinCosBCD traceSinCosBCD(const BCDNumber& n, CordicTrace& out) {
 out.steps.clear();out.algorithm="rotation";out.note.clear();
 if(n.exponent<=-11&&!n.isZero()){
  out.note="Tiny-angle direct approximation (no rotations)";
  return sinCosScientificBCD(n);
 }
 auto q=quadrant(n);
 if(q.angle.isZero()||q.angle.compare(ninety())==0){
  out.note="Exact quadrant boundary (no rotations)";
  return sinCosScientificBCD(n);
 }
 auto complement=ninety().sub(q.angle);
 if(q.angle.compare(one())<=0||complement.compare(one())<=0){
  out.note="Small-angle polynomial shortcut (no rotations)";
  return sinCosScientificBCD(n);
 }
 auto result=rotate(q.angle,&out);
 return {(q.sinSign<0?result.sine.neg():result.sine).toBCD(),
         (q.cosSign<0?result.cosine.neg():result.cosine).toBCD()};
}
BCDNumber traceAtanBCD(const BCDNumber& n, CordicTrace& out){
 out.steps.clear();out.algorithm="vectoring";out.note.clear();
 if(n.isZero()||n.exponent<=-11){
  out.note="Zero/tiny argument shortcut (no rotations)";
  return atanScientificBCD(n);
 }
 BCDNumber positive=n;positive.negative=false;
 FixedBCD ratio;bool inverted;
 if(positive.exponent>3){ratio=FixedBCD::fromBCD(divBCD(BCDNumber::fromDecimal("1"),positive));inverted=true;}
 else {auto f=FixedBCD::fromBCD(positive);inverted=f.compare(one())>0;ratio=inverted?one().divide(f):f;}
 if(ratio.isZero()||ratio.compare(one())==0||ratio.compare(fx("0.001"))<=0){
  out.note="Exact value / small-ratio polynomial shortcut (no rotations)";
  return atanScientificBCD(n);
 }
 auto angle=atanUnit(ratio,&out);
 auto adjusted=inverted?ninety().sub(angle):angle;
 return (n.negative?adjusted.neg():adjusted).toBCD();
}
} // namespace hp35
