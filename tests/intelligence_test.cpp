#include "../firmware/flawless/intelligence_model.h"
#include <cassert>
#include <iostream>
int main(){
 oni::Observation o; uint8_t p[64]={0}; p[0]=0x40;p[1]=0; for(int i=0;i<6;++i){p[10+i]=uint8_t(i);p[16+i]=uint8_t(10+i);} p[24]=0;p[25]=3;p[26]='A';p[27]='B';p[28]='C';assert(oni::parseManagement(p,29,o));assert(std::string(o.kind)=="PROBE_REQUEST");assert(std::string(o.name)=="ABC");
 p[25]=40;assert(!oni::parseManagement(p,64,o)); assert(!oni::parseAddress("0011ZZ",p,3));assert(oni::parseAddress("001122334455",p,6));
 oni::Traffic t; for(int i=0;i<130;++i)t.sample(12,12,i*1000);assert(t.occupancy()>99&&std::string(t.profile())=="Steady activity / possible carrier");
 std::cout<<"Intelligence parser and traffic profile tests passed\n";
}
