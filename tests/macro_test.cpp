#include <cassert>
#include <cstring>
#include <string>
#include "../firmware/flawless/macro_script.h"
int main(){
 OniMacroAction actions[96];uint8_t count=0;
 const char* script="TEXT hello  \r\nCTRL A\nDELAY 250\nENTER";
 assert(flawless::parseMacro(script,strlen(script),actions,96,count));
 assert(count==4&&!strcmp(actions[0].text,"hello  ")&&actions[1].key=='a'&&actions[2].delayMs==250);
 for(const char* bad:{"DELAY 9","DELAY 2001","CTRL !","TEXT héllo","UNKNOWN","DELAY -10"}){assert(!flawless::parseMacro(bad,strlen(bad),actions,96,count));assert(count==0);}
 std::string longText="TEXT "+std::string(128,'x');assert(!flawless::parseMacro(longText.data(),longText.size(),actions,96,count));
 std::string many;for(int i=0;i<97;++i)many+="ENTER\n";assert(!flawless::parseMacro(many.data(),many.size(),actions,96,count));
 const char* valid="# comment\nTEXT x\nTAB\nESC\nBACKSPACE\n";assert(flawless::parseMacro(valid,strlen(valid),actions,96,count)&&count==4);
}
