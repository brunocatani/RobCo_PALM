#include "WheelPreferences.h"
#include "EquipmentIdentity.h"
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
 for(unsigned c=1;c<kCategoryCount;++c)for(unsigned i=0;i<kSlots;++i)
  check(prefs.select(c,{"variant|"+std::to_string(i),"Equipment "+std::to_string(i)},true));
 std::ostringstream full;writePreferences(full,prefs);
 Preferences forty;std::istringstream fullSource(full.str());check(readPreferences(fullSource,forty));
 std::size_t total=0;for(const auto& list:forty.slots)total+=list.size();check(total==40);
 check(!forty.select(4,{"ninth","Ninth armor"},true));
 for(const auto header:{"WheelItems 1","WheelItems 2"}) {
  std::istringstream legacy(std::string(header)+full.str().substr(full.str().find('\n')));
  Preferences rejected;check(!readPreferences(legacy,rejected));
  for(const auto& category:rejected.slots)check(category.empty());
 }
 const std::array<std::string,1> mod{"weapon-mod.esp|001234:0:1:0"};
 check(equipmentKey("fallout4.esm|000001","Rifle",mod)!=equipmentKey("fallout4.esm|000001","Named Rifle",mod));
 check(equipmentKey("fallout4.esm|000001","Rifle",mod)!=equipmentKey("fallout4.esm|000001","Rifle",{}));
 check(equipmentKey("","Rifle",mod).empty());
 Model inventory;inventory.items[0]={{999,"Item 0",4,false,"plugin|0"},{888,"Not selected",3,false,"other"}};
 auto visible=curatedInventory(inventory,loaded);
 check(visible.items[0].size()==1 && visible.items[0][0].id==999);
 inventory.items[0].push_back({0,"Unresolved",2,false,"plugin|1"});
 inventory.items[0].push_back({777,"Empty stack",0,false,"plugin|2"});
 check(curatedInventory(inventory,loaded).items[0].size()==1);
 inventory.items[0].clear();check(curatedInventory(inventory,loaded).items[0].empty());
 check(loaded.slots[0].size()==8); // Hiding is not deletion of a saved choice.
 inventory.items[0]={{555,"Reacquired 2",1,false,"plugin|2"},{999,"Reacquired 0",1,false,"plugin|0"}};
 visible=curatedInventory(inventory,loaded);
 check(visible.items[0].size()==2 && visible.items[0][0].id==999 && visible.items[0][1].id==555);
 check(loaded.select(0,{"plugin|0","Item 0"},false));
 check(loaded.select(0,{"plugin|8","Ninth item"},true));
 std::istringstream invalid("PALMItems 1\ncategory 0 1\nitem 0 \"same\" \"A\"\nitem 0 \"same\" \"B\"\ncategory 1 1\ncategory 2 1\ncategory 3 1\ncategory 4 1\n");
 check(!readPreferences(invalid,loaded));check(loaded.slots[0].size()==8);
 std::cout<<"Slot limit, persistence, stable identity and unavailable-item behavior passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
