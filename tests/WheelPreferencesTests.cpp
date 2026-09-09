#include "WheelPreferences.h"
#include <iostream>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("Wheel preferences assertion failed");}
int main(){try {
 using namespace wheel;
 check(stableItemKey("Example.esp",0x02001234)==stableItemKey("example.esp",0x09001234));
 check(stableItemKey("Light.esl",0xfe001abc)==stableItemKey("Light.esl",0xfe987abc));
 check(stableItemKey("",123).empty());check(stableItemKey("Example.esp",0xff000123).empty());
 Preferences prefs;
 for(unsigned i=0;i<8;++i)check(prefs.select(0,{"plugin|"+std::to_string(i),"Item "+std::to_string(i)},true));
 check(!prefs.select(0,{"plugin|8","Ninth item"},true));
 check(!prefs.select(0,{"plugin|0","Duplicate"},true));
 prefs.enabled[1]=false;
 std::ostringstream saved;writePreferences(saved,prefs);
 Preferences loaded;std::istringstream source(saved.str());check(readPreferences(source,loaded));
 check(loaded.slots[0].size()==8 && !loaded.enabled[1]);
 Model inventory;inventory.items[0]={{999,"Item 0",4,false,"plugin|0"},{888,"Not selected",3,false,"other"}};
 auto visible=curatedInventory(inventory,loaded);
 check(visible.items[0].size()==8 && visible.items[0][0].id==999);
 check(visible.items[0][1].id==0 && visible.items[0][1].count==0); // No silent replacement.
 check(loaded.select(0,{"plugin|0","Item 0"},false));
 check(loaded.select(0,{"plugin|8","Ninth item"},true));
 std::istringstream invalid("WheelItems 1\ncategory 0 1\nitem 0 \"same\" \"A\"\nitem 0 \"same\" \"B\"\n");
 check(!readPreferences(invalid,loaded));check(loaded.slots[0].size()==8);
 std::cout<<"Slot limit, persistence, stable identity and unavailable-item behavior passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
