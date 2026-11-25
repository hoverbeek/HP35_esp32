#pragma once
#include <Arduino.h>
#include <Wire.h>
namespace hp35motion {
struct Axes { int16_t x=0,y=0,z=0; };
inline uint32_t delta(const Axes& a,const Axes& b) {
 uint32_t x=abs(int32_t(a.x)-b.x),y=abs(int32_t(a.y)-b.y),z=abs(int32_t(a.z)-b.z);
 return max(x,max(y,z));
}
class QMI8658 {
 TwoWire* wire_=&Wire;
 uint8_t addr_=0,id_=0;bool ok_=false;
 bool readReg(uint8_t reg,uint8_t *dst,uint8_t n) {
  if(!addr_)return false;
  wire_->beginTransmission(addr_);wire_->write(reg);
  if(wire_->endTransmission(false)!=0)return false;
  if(wire_->requestFrom(addr_,n)!=n)return false;
  for(uint8_t i=0;i<n;i++)dst[i]=wire_->read();
  return true;
 }
 bool writeReg(uint8_t reg,uint8_t val) {
  wire_->beginTransmission(addr_);wire_->write(reg);wire_->write(val);
  return wire_->endTransmission()==0;
 }
public:
 bool begin() {
  wire_->begin(47,48,100000);
  for(uint8_t a:{uint8_t(0x6B),uint8_t(0x6A)}) {
   addr_=a;uint8_t id=0;
   if(readReg(0,&id,1)&&id==5){id_=id;break;}
   addr_=0;
  }
  if(!addr_)return false;
  ok_=writeReg(0x08,0) && writeReg(0x02,0x60)
   && writeReg(0x03,0x06) && writeReg(0x08,0x01);
  if(ok_)delay(35);
  return ok_;
 }
 bool available()const{return ok_;}
 uint8_t address()const{return addr_;}
 uint8_t identity()const{return id_;}
 bool read(Axes& a) {
  if(!ok_)return false;
  uint8_t b[6];if(!readReg(0x35,b,6))return false;
  a.x=int16_t(uint16_t(b[0])|(uint16_t(b[1])<<8));
  a.y=int16_t(uint16_t(b[2])|(uint16_t(b[3])<<8));
  a.z=int16_t(uint16_t(b[4])|(uint16_t(b[5])<<8));
  return true;
 }
};
}
