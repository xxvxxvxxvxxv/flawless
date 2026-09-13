#pragma once
#include <stdint.h>
#include <string.h>
namespace oni {
constexpr uint16_t MAX_FRAMES=600;
inline uint32_t get32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;++i)p[i]=v>>(8*i);}
struct Frame {uint8_t kind;uint32_t session,seq,total,sent;};
inline void encode(uint8_t* b,const Frame& f){memset(b,0,32);memcpy(b,"ONI1",4);b[4]=1;b[5]=f.kind;put32(b+8,f.session);put32(b+12,f.seq);put32(b+16,f.total);put32(b+20,f.sent);}
inline bool decode(const uint8_t* b,Frame& f){
 if(memcmp(b,"ONI1",4)||b[4]!=1||(b[5]!=1&&b[5]!=2)||b[6]||b[7])return false;
 for(int i=24;i<32;++i)if(b[i])return false;
 f={b[5],get32(b+8),get32(b+12),get32(b+16),get32(b+20)};
 return f.session&&f.total&&f.total<=MAX_FRAMES&&f.seq<f.total;
}
struct Tracker {
 uint8_t seen[(MAX_FRAMES+7)/8]={};uint32_t unique=0,duplicates=0,reordered=0,high=0,gapEvents=0;
 bool add(uint32_t s){if(s>=MAX_FRAMES)return false;uint8_t mask=1u<<(s%8);if(seen[s/8]&mask){++duplicates;return false;}
 seen[s/8]|=mask;if(unique&&s<high)++reordered;if(s>high || (!unique&&s>0))++gapEvents;
 if(s+1>high){high=s+1;}
 ++unique;return true;}
 uint32_t missingObserved()const{return high-unique;}
};
}
