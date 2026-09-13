#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
namespace oni {
enum Layer:uint8_t { SYSTEM=0, NRF=1, BLE=2, WIFI=3, FIELD=4 };
struct Observation {
 uint64_t us=0;uint32_t id=0;int32_t value=0;int16_t rssi=0;
 uint8_t layer=SYSTEM,channel=0,length=0,crc=0,addressType=0;
 uint8_t mac[6]={},peer[6]={},bytes[32]={};char name[33]={},kind[24]={};
};
template<typename T> struct Ring {
 T* data=nullptr;size_t capacity=0,head=0,count=0;uint32_t total=0;
 void init(T* p,size_t n){data=p;capacity=n;head=count=total=0;}
 bool push(T item){if(!data||!capacity)return false;item.id=++total;data[head]=item;head=(head+1)%capacity;if(count<capacity)++count;return true;}
 const T& at(size_t i)const{return data[(head+capacity-count+i)%capacity];}
};
struct Traffic {
 uint64_t firstUs=0,lastUs=0,lastPacketUs=0;uint32_t observations=0,hits=0,packets=0,iatCount=0,bursts=0;
 double meanIat=0,m2=0;uint8_t peak=0,lastOccupancy=0,minLen=255,maxLen=0;
 void sample(uint8_t hit,uint8_t trials,uint64_t us){if(!observations)firstUs=us;lastUs=us;observations+=trials;hits+=hit;uint8_t v=trials?100*hit/trials:0;if(v>peak)peak=v;if(v>=50&&lastOccupancy<50)++bursts;lastOccupancy=v;}
 void packet(uint8_t len,uint64_t us){if(packets&&us>=lastPacketUs){double dt=double(us-lastPacketUs);++iatCount;double d=dt-meanIat;meanIat+=d/iatCount;m2+=d*(dt-meanIat);}lastPacketUs=us;++packets;if(len<minLen)minLen=len;if(len>maxLen)maxLen=len;}
 double occupancy()const{return observations?100.0*hits/observations:0;}
 double cv()const{return iatCount>1&&meanIat>0?sqrt(m2/(iatCount-1))/meanIat:0;}
 const char* profile()const{
  if(observations>=120&&occupancy()>=85)return "Steady activity / possible carrier";
  if(iatCount>=20&&meanIat>=1000&&meanIat<=100000&&cv()<0.2&&maxLen<=32)return "Periodic polling-like";
  if(packets>=5&&iatCount>=4&&(meanIat>100000||cv()>0.8))return "Intermittent burst-like";
  return "Unclassified";
 }
 const char* signature()const{
  // Conservative generic signatures. These are behaviour matches, not vendor/device IDs.
  if(packets>=8&&meanIat>=7000&&meanIat<=13000&&cv()<0.25&&minLen==maxLen)return "10 ms polling-like";
  if(packets>=5&&meanIat>=900000&&meanIat<=1100000&&cv()<0.35)return "1 s beacon-like";
  if(packets>=5&&meanIat>=100000&&meanIat<=500000&&cv()>0.35)return "sensor burst-like";
  return "no signature";
 }
};
// Bounds-checked 802.11 management parsing. len excludes the trailing FCS.
inline bool parseManagement(const uint8_t* p,size_t len,Observation& o){
 if(len<24||(p[0]&0x0f)!=0||p[1]&0x40)return false;
 const uint8_t subtype=p[0]>>4;size_t offset;
 if(subtype==4){strcpy(o.kind,"PROBE_REQUEST");offset=24;}
 else if(subtype==0){strcpy(o.kind,"ASSOC_REQUEST");offset=28;}
 else if(subtype==2){strcpy(o.kind,"REASSOC_REQUEST");offset=34;}
 else if(subtype==1||subtype==3){if(len<30)return false;strcpy(o.kind,"ASSOC_RESPONSE");memcpy(o.mac,p+4,6);memcpy(o.peer,p+10,6);o.value=p[26]|uint16_t(p[27])<<8;return true;}
 else return false;
 if(len<offset)return false;
 memcpy(o.mac,p+10,6);memcpy(o.peer,p+16,6);
 bool found=false;while(offset<len){if(len-offset<2)return false;uint8_t id=p[offset++],n=p[offset++];if(n>len-offset)return false;
  if(id==0){if(n>32||found)return false;found=true;o.length=n;memcpy(o.bytes,p+offset,n);for(uint8_t i=0;i<n;++i)o.name[i]=(p[offset+i]>=32&&p[offset+i]<127)?p[offset+i]:'.';o.name[n]=0;}
  offset+=n;
 }
 return found;
}
inline bool parseAddress(const char* s,uint8_t* out,size_t n){if(strlen(s)!=n*2)return false;for(size_t i=0;i<n;++i){unsigned v=0;for(int j=0;j<2;++j){char c=s[2*i+j];int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(d<0)return false;v=(v<<4)|unsigned(d);}out[i]=uint8_t(v);}return true;}
}
