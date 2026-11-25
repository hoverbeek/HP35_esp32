#include "debug_trace.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>
namespace hp35 {
std::string fixedNibbles(const FixedBCD& v){
  std::string s;
  for (auto d: v.digits) s.push_back(char('0'+d));
  return s;
}
std::string fixedDecimal(const FixedBCD& v){
  std::string s=v.negative?"-":"";
  for(int i=0;i<4;i++)s.push_back(char('0'+v.digits[i]));
  s.push_back('.');
  for(int i=4;i<26;i++)s.push_back(char('0'+v.digits[i]));
  return s;
}
bool DebugTrace::capture(const std::string& op,const BCDNumber& input,std::string& err){
  // Preserve last good trace if the newly requested operation fails.
  try{
    CordicTrace candidate;
    BCDNumber answer;
    if(op=="sin"||op=="cos"||op=="tan") {
      auto sc=traceSinCosBCD(input,candidate);
      if(op=="sin")answer=sc.sine;
      else if(op=="cos")answer=sc.cosine;
      else {
        if(sc.cosine.isZero())throw std::domain_error("tan undefined at 90 degrees");
        answer=divBCD(sc.sine,sc.cosine);
      }
    } else if(op=="atan")answer=traceAtanBCD(input,candidate);
    else if(op=="ln" || op=="exp"){
      ScientificTrace alt;
      answer=op=="ln" ? traceLnScientificBCD(input,alt) : traceExpScientificBCD(input,alt);
      scientific=std::move(alt);scientificMode=true;
    }
    else {err="Trace supports sin, cos, tan, atan, ln, exp";return false;}
    if(op!="ln" && op!="exp") {trace=std::move(candidate);scientificMode=false;}
    argument=input; result=answer; operation=op;
    cursor=count()==0?0:1; active=true;err.clear();return true;
  }catch(const std::exception& ex){err=ex.what();return false;}
}
bool DebugTrace::jump(size_t n){if(!active||n<1||n>count())return false;cursor=n;return true;}
bool DebugTrace::next(){return jump(cursor+1);}
bool DebugTrace::previous(){return cursor>1?jump(cursor-1):false;}
std::string DebugTrace::summary()const{
  if(!active)return "No trace. Use: trace ln 2.02 or trace sin 30\n";
  char buf[80];std::snprintf(buf,sizeof(buf)," | %u %s",(unsigned)count(),scientificMode?"decimal steps":"micro-rotations");
  return operation+"("+argument.toString()+") = "+result.toString()+buf+"\n"+
    (scientificMode?"":(trace.note.empty()?"":trace.note+"\n"));
}
std::string DebugTrace::current()const{
  if(!active)return "No trace.\n";
  if(count()==0)return summary()+"No intermediate steps for this input.\n";
  if(scientificMode){
    const auto& r=scientific.steps[cursor-1];char line[160];
    std::snprintf(line,sizeof(line),"Step %u/%u decade=%u repetition=%u action=%s\n",
      (unsigned)cursor,(unsigned)count(),r.decade,r.repetition,r.action.c_str());
    return std::string(line)+"work.before="+fixedDecimal(r.before)+"\nwork.after="+fixedDecimal(r.after)+
      "\nfactor="+fixedDecimal(r.factor)+"\nacc.before="+fixedDecimal(r.accumulatorBefore)+
      "\nacc.after="+fixedDecimal(r.accumulatorAfter)+"\n";
  }
  const auto& r=trace.steps[cursor-1];char line[128];
  std::snprintf(line,sizeof(line),"Step %u/%u decade=%u repetition=%u direction=%c\n",
    (unsigned)cursor,(unsigned)count(),r.decade,r.repetition,r.positive?'+':'-');
  return std::string(line)+"work.x="+fixedDecimal(r.x)+"\nwork.y="+fixedDecimal(r.y)+
         "\nwork.z="+fixedDecimal(r.z)+"\n";
}
std::string DebugTrace::hidden()const{
  if(!active)return "No scratch state captured; use trace sin 30 first.\n";
  if(count()==0)return scientificMode ? "No decimal scratch steps recorded for this input.\n" : "Algorithm shortcut: no CORDIC scratch rotations recorded.\n";
  if(scientificMode){
    const auto& r=scientific.steps[cursor-1];
    return "Hidden ALGORITHM scratch (not HP-35 CPU regs):\n"
      "work.before sign="+std::string(r.before.negative?"-":"+")+" BCD="+fixedNibbles(r.before)+"\n"
      "work.after  sign="+std::string(r.after.negative?"-":"+")+" BCD="+fixedNibbles(r.after)+"\n"
      "factor      BCD="+fixedNibbles(r.factor)+"\n"
      "acc.before  BCD="+fixedNibbles(r.accumulatorBefore)+"\n"
      "acc.after   BCD="+fixedNibbles(r.accumulatorAfter)+"\n";
  }
  const auto& r=trace.steps[cursor-1];
  return "Hidden ALGORITHM scratch registers (not historical HP-35 CPU regs):\n"
    "work.x sign="+std::string(r.x.negative?"-":"+")+" BCD="+fixedNibbles(r.x)+"\n"
    "work.y sign="+std::string(r.y.negative?"-":"+")+" BCD="+fixedNibbles(r.y)+"\n"
    "work.z sign="+std::string(r.z.negative?"-":"+")+" BCD="+fixedNibbles(r.z)+"\n";
}
}
