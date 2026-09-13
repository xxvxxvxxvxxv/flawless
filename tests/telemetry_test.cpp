#include "../firmware/flawless/telemetry.h"
#include <cassert>
#include <iostream>
int main(){
 uint8_t b[32];oni::Frame f={1,123,599,600,0xfffffff0},out;
 oni::encode(b,f);assert(oni::decode(b,out));assert(out.session==123&&out.seq==599&&out.sent==0xfffffff0);
 for(int i: {0,4,6,24}){uint8_t saved=b[i];b[i]=255;assert(!oni::decode(b,out));b[i]=saved;}
 b[5]=3;assert(!oni::decode(b,out));b[5]=2;assert(oni::decode(b,out));
 oni::put32(b+16,601);assert(!oni::decode(b,out));oni::put32(b+16,599);assert(!oni::decode(b,out));
 oni::Tracker t;assert(t.add(3));assert(t.missingObserved()==3&&t.gapEvents==1);
 assert(!t.add(3)&&t.duplicates==1);assert(t.add(0));assert(t.add(2));assert(t.add(1));
 assert(t.unique==4&&t.missingObserved()==0&&t.reordered==3);assert(!t.add(600));
 t=oni::Tracker();for(unsigned i=0;i<600;++i)assert(t.add(i));assert(t.unique==600&&t.gapEvents==0);
 for(unsigned i=0;i<600;++i){assert(!t.add(i));}
 assert(t.duplicates==600);
 std::cout<<"Telemetry protocol and sequence accounting passed\n";
}
