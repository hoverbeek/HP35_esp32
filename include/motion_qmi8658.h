#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

namespace hp35motion {
struct Axes { int16_t x=0,y=0,z=0; };
inline uint32_t delta(const Axes& a,const Axes& b) {
 const uint32_t x=abs(int32_t(a.x)-b.x),y=abs(int32_t(a.y)-b.y),z=abs(int32_t(a.z)-b.z);
 return max(x,max(y,z));
}
class QMI8658 {
 TwoWire* wire_=&Wire;
 uint8_t addr_=0,id_=0;
 uint8_t sda_=47,scl_=48;
 bool ok_=false;
 uint8_t lastError_=0;
 bool readReg(uint8_t reg,uint8_t *dst,uint8_t n,bool repeatedStart=true) {
  if(!addr_)return false;
  wire_->beginTransmission(addr_);wire_->write(reg);
  lastError_=wire_->endTransmission(!repeatedStart);
  if(lastError_!=0)return false;
  const uint8_t count=wire_->requestFrom(addr_,n);
  if(count!=n) {lastError_=0xFE;return false;}
  for(uint8_t i=0;i<n;i++)dst[i]=wire_->read();
  return true;
 }
 bool writeReg(uint8_t reg,uint8_t value) {
  wire_->beginTransmission(addr_);wire_->write(reg);wire_->write(value);
  lastError_=wire_->endTransmission();
  return lastError_==0;
 }
 void configureBus(uint8_t sda,uint8_t scl) {
  wire_->end();
  delay(5);
  wire_->begin(sda,scl,100000);
  wire_->setTimeOut(50);
  sda_=sda;scl_=scl;
  delay(10);
 }
 bool locate(bool allowReverse) {
  const uint8_t options[2][2]={{47,48},{48,47}};
  for(int option=0;option<(allowReverse?2:1);++option) {
   configureBus(options[option][0],options[option][1]);
   for(uint8_t address : {uint8_t(0x6B),uint8_t(0x6A)}) {
    addr_=address;
    // Some I2C implementations need STOP after writing register pointer.
    for(bool repeatedStart : {true,false}) {
     uint8_t candidate=0;
     if(readReg(0x00,&candidate,1,repeatedStart) && candidate==0x05) {
      id_=candidate;return true;
     }
    }
   }
  }
  addr_=0;id_=0;return false;
 }
public:
 bool begin() {
  ok_=false;addr_=0;id_=0;
  // First use the schematic's GPIO47 SDA / GPIO48 SCL.
  // Reverse is probed only if normal direction does not find WHO_AM_I=0x05.
  if(!locate(true))return false;
  ok_=writeReg(0x08,0x00) && writeReg(0x02,0x60)
    && writeReg(0x03,0x06) && writeReg(0x08,0x01);
  if(ok_)delay(35);
  return ok_;
 }
 void scan(Print& output) {
  output.println("I2C scan: testing SDA47/SCL48 and reversed SDA48/SCL47.");
  const uint8_t pinsets[2][2]={{47,48},{48,47}};
  for(auto &pins : pinsets) {
   configureBus(pins[0],pins[1]);
   output.printf("I2C SDA=%u SCL=%u:",(unsigned)pins[0],(unsigned)pins[1]);
   bool found=false;
   for(uint8_t address=0x08;address<=0x77;++address) {
    wire_->beginTransmission(address);
    const uint8_t error=wire_->endTransmission();
    if(error!=0)continue;
    found=true;
    output.printf(" ACK 0x%02X",address);
    if(address==0x6A || address==0x6B) {
     addr_=address;
     for(bool repeatedStart : {true,false}) {
      uint8_t ident=0xFF;
      const bool readOK=readReg(0x00,&ident,1,repeatedStart);
      output.printf(" WHOAMI[%s]=%s0x%02X",repeatedStart?"restart":"stop",
        readOK?"":"ERR/",ident);
     }
    }
   }
   if(!found)output.print(" no devices ACK");
   output.println();
  }
  // Restore normal bus arrangement or recognized alternate configuration.
  const bool found=locate(true);
  if(found) {
   ok_=writeReg(0x08,0x00) && writeReg(0x02,0x60)
     && writeReg(0x03,0x06) && writeReg(0x08,0x01);
   if(ok_)delay(35);
  } else {ok_=false;}
  output.printf("QMI8658 result: %s, SDA=%u SCL=%u, address=0x%02X ID=0x%02X, last I2C code=%u\r\n",
   ok_?"READY":"NOT DETECTED",(unsigned)sda_,(unsigned)scl_,addr_,id_,lastError_);
 }
 bool available()const{return ok_;}
 uint8_t address()const{return addr_;}
 uint8_t identity()const{return id_;}
 int sda()const{return sda_;}
 int scl()const{return scl_;}
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