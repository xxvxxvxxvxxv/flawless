#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>
#include "../firmware/flawless/fat12_snapshot.h"
static unsigned entry(const uint8_t* p,unsigned c){unsigned i=512+c*3/2;unsigned v=p[i]|unsigned(p[i+1])<<8;return c%2?v>>4:v&4095;}
int main(){
 std::vector<uint8_t> memory(flawless::Fat12Snapshot::SIZE+16,0xaa);
 flawless::Fat12Snapshot disk(memory.data());disk.begin();
 std::string content(1400,'Q');content[600]='Z';
 assert(disk.add("FRAMES  CSV",content.data(),content.size()));
 assert(entry(memory.data(),2)==3&&entry(memory.data(),3)==4&&entry(memory.data(),4)==4095);
 assert(!memcmp(memory.data()+2048,content.data(),1400));
 assert(memory[510]==0x55&&memory[511]==0xaa);
 assert(disk.add("EVENTS  JSN","{}",2));
 unsigned next=disk.next;assert(!disk.add("TOOBIG  TXT",content.data(),flawless::Fat12Snapshot::SIZE));assert(next==disk.next);
 for(size_t i=flawless::Fat12Snapshot::SIZE;i<memory.size();++i)assert(memory[i]==0xaa);
 std::ofstream("/tmp/flawless-fat12.img",std::ios::binary).write((char*)memory.data(),flawless::Fat12Snapshot::SIZE);
 disk.begin();assert(disk.next==2&&disk.entries==0);assert(disk.add("EMPTY   TXT","",0));
}
