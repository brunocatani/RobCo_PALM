#include "EquipmentRuntimePolicy.h"
#include "WheelPreferences.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){try {
 using namespace wheel;
 using namespace equipment_policy;
 // Captured from the unpacked FO4VR 1.2.72 image, independently of the guard.
 const std::array<std::uint8_t,32> native{
  0x4c,0x8b,0xdc,0x49,0x89,0x53,0x10,0x55,0x56,0x41,0x54,0x41,0x57,0x49,0x8d,0x6b,
  0xd9,0x48,0x81,0xec,0xa8,0x00,0x00,0x00,0x45,0x8b,0xe1,0x49,0x8b,0xf0,0x48,0x8b};
 require(classifyEquipEntry(native,false)==EquipEntry::Native,"unmodified equip entry rejected");
 require(classifyEquipEntry({},true)==EquipEntry::Unavailable,"unreadable entry accepted");
 auto hooked=native;
 const std::array<std::uint8_t,17> patch{0xff,0x25,0,0,0,0,0x10,0x32,0x54,0x76,0x12,0x7f,0,0,0x90,0x90,0x90};
 std::copy(patch.begin(),patch.end(),hooked.begin());
 require(heisenbergEquipJump(hooked),"recorded 17-byte Heisenberg detour rejected");
 require(classifyEquipEntry(hooked,true)==EquipEntry::Heisenberg,"verified hook unavailable");
 require(classifyEquipEntry(hooked,false)==EquipEntry::Unavailable,"foreign/non-executable hook accepted");
 require(classifyEquipEntry(std::span(hooked).first(17),true)==EquipEntry::Unavailable,"missing native continuation accepted");
 for(const unsigned index:{0u,1u,2u,3u,4u,5u,14u,15u,16u,17u,24u,31u}) {
  auto damaged=hooked;damaged[index]^=1;
  require(classifyEquipEntry(damaged,true)==EquipEntry::Unavailable,"damaged detour or continuation accepted");
 }
 // Unequip entry captured from the same image; ROCK's 15-byte entry detour.
 const std::array<std::uint8_t,32> nativeUnequip{
  0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x57,0x41,0x56,0x48,0x83,0xec,0x70,0x45,
  0x8b,0xf1,0x49,0x8b,0xd8,0x48,0x8b,0xfa,0x48,0x8b,0xe9,0x48,0x85,0xd2,0x0f,0x84};
 require(classifyUnequipEntry(nativeUnequip,false)==UnequipEntry::Native,"unmodified unequip entry rejected");
 auto rockHooked=nativeUnequip;
 const std::array<std::uint8_t,15> rockPatch{0xff,0x25,0,0,0,0,0x10,0x32,0x54,0x76,0xfd,0x7f,0,0,0x90};
 std::copy(rockPatch.begin(),rockPatch.end(),rockHooked.begin());
 require(classifyUnequipEntry(rockHooked,true)==UnequipEntry::Rock,"verified ROCK unequip detour unavailable");
 require(classifyUnequipEntry(rockHooked,false)==UnequipEntry::Unavailable,"foreign unequip detour accepted");
 require(classifyUnequipEntry(std::span(rockHooked).first(15),true)==UnequipEntry::Unavailable,"missing unequip continuation accepted");
 for(const unsigned index:{0u,1u,2u,5u,14u,15u,20u,31u}) {
  auto damaged=rockHooked;damaged[index]^=1;
  require(classifyUnequipEntry(damaged,true)==UnequipEntry::Unavailable,"damaged unequip detour or continuation accepted");
 }
 // Capabilities fail independently. Missing display names blocks variant
 // actions, while native consumable use depends only on equip.
 for(unsigned mask=0;mask<8;++mask) {
  const EquipmentSupport support{bool(mask&1),bool(mask&2),bool(mask&4)};
  require(support.canToggle(false)==((mask&5)==5),"equip acquired an unrelated dependency");
  require(support.canToggle(true)==((mask&6)==6),"unequip acquired an unrelated dependency");
 }
 const EquipmentSupport equipConflict{false,true,true};
 require(!equipConflict.canToggle(false) && equipConflict.canToggle(true),"equip conflict disabled unequip");
 const EquipmentSupport unequipConflict{true,false,true};
 require(unequipConflict.canToggle(false) && !unequipConflict.canToggle(true),"unequip conflict disabled equip");

 Preferences preferences;
 require(preferences.select(0,{"aid","Stimpak"},true),"aid preference failed");
 require(preferences.select(3,{"rifle-variant","Named Rifle"},true),"weapon preference failed");
 Model unavailable;unavailable.equipmentNamesAvailable=false;unavailable.category=Category::Weapons;
 unavailable.items[0].push_back({1,"Stimpak",4,false,"aid"});
 const auto visible=curatedInventory(unavailable,preferences);
 require(!visible.equipmentNamesAvailable && visible.items[3].empty(),"failed names produced an equipment identity");
 require(visible.items[0].size()==1,"equipment failure hid usable aid");
 const auto layout=navigationLayout(visible);
 require(layout.entries[layout.count-1]==kConfigNavigation,"equipment failure hid Config");
 require(hitCenter(0,0,1,layout)==kCancelNavigation,"equipment failure hid Cancel");
 require(preferences.slots[3].size()==1 && preferences.slots[3][0].key=="rifle-variant","fallback rewrote a favorite");
 auto recovered=unavailable;recovered.equipmentNamesAvailable=true;
 recovered.items[3].push_back({2,"Named Rifle",1,false,"rifle-variant"});
 require(curatedInventory(recovered,preferences).items[3].size()==1,"recovery lost the saved variant");
 std::cout<<"Native entry validation, hook rejection and isolated equipment fallback passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
