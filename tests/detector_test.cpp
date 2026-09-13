#include "../firmware/flawless/analysis.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
  Detector d; uint8_t values[126]={}; int bursts=0, sustained=0;
  auto emit=[&](const char* name,int ch,uint8_t v){assert(ch==40);assert(v>=75);if(!strcmp(name,"BURST"))++bursts;else ++sustained;};
  for(int i=0;i<10;++i)d.update(values,100+i*100,emit);
  values[40]=90;d.update(values,1200,emit);assert(bursts==1);
  d.update(values,6199,emit);assert(sustained==0);
  d.update(values,6200,emit);assert(sustained==1);
  d.update(values,7200,emit);assert(sustained==1);
  values[40]=0;d.update(values,7300,emit);
  values[40]=80;d.update(values,7400,emit);assert(bursts==2);
  d.update(values,12400,emit);assert(sustained==2);
  std::cout<<"PASS: warmup, burst, duration threshold, latch, reset\n";
}
