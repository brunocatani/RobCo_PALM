#include "RockServices.h"
#include "PCH.h"
#include "Equipment.h"
#include "Inventory.h"
#include "RockFrame.h"
#include "WheelConfig.h"
#include "tools/LoadedRpsMods.h"
#include <array>
#include <cstring>

namespace wheel {
namespace {
EquipmentSupport support; // Immutable after startup, before any input/task readers.
bool readBytes(std::uintptr_t address,void* out,std::size_t size) noexcept {
 MEMORY_BASIC_INFORMATION memory{};
 if(!VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
  (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
  size>memory.RegionSize-(address-reinterpret_cast<std::uintptr_t>(memory.BaseAddress)))return false;
 __try {std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// The detour target must be executable image code of that loaded module.
bool moduleTarget(std::uintptr_t address,HMODULE module) noexcept {
 MEMORY_BASIC_INFORMATION memory{};
 constexpr DWORD executable=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
 HMODULE owner{};
 return module && VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) &&
  memory.State==MEM_COMMIT && memory.Type==MEM_IMAGE && (memory.Protect&executable) &&
  !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCWSTR>(address),&owner) && owner==module;
}
std::uintptr_t jumpDestination(std::span<const std::uint8_t> actual) noexcept {
 std::uintptr_t destination{};
 if(actual.size()>=6+sizeof(destination))std::memcpy(&destination,actual.data()+6,sizeof(destination));
 return destination;
}
HMODULE rockModule() noexcept {
 const auto rock=rock_configurator::loadedRockMod();
 return rock?GetModuleHandleW(rock_configurator::modInfo(*rock).module):nullptr;
}
std::string hexBytes(std::span<const std::uint8_t> bytes) {
 std::string result;
 for(auto byte:bytes){if(!result.empty())result+=' ';result+=std::format("{:02X}",byte);}
 return result;
}
// The one supported foreign detour of each entry; any other modification fails closed.
enum class KnownDetour {None,Heisenberg,Rock};
template<std::size_t N> bool validateEntry(const char* name,std::uintptr_t rva,const std::array<std::uint8_t,N>& expected,
 KnownDetour detour=KnownDetour::None) {
 std::array<std::uint8_t,N> actual{};
 const auto address=REL::Offset(rva).address();
 if(!readBytes(address,actual.data(),actual.size())) {
  spdlog::warn("PALM native {} unavailable: unreadable entry at RVA {:X}; wheel remains available",name,rva);return false;
 }
 bool accepted=equipment_policy::prefixMatches(actual,expected);
 if(detour==KnownDetour::Heisenberg) {
  const auto destination=equipment_policy::heisenbergEquipJump(actual)?jumpDestination(actual):0;
  const auto kind=equipment_policy::classifyEquipEntry(actual,destination &&
   moduleTarget(destination,GetModuleHandleW(L"Heisenberg_F4VR.dll")));
  accepted=kind!=equipment_policy::EquipEntry::Unavailable;
  if(kind==equipment_policy::EquipEntry::Heisenberg) {
   spdlog::info("PALM native {}: supported Heisenberg_F4VR.dll detour at {:X}; existing hook retained",name,destination);
   return true;
  }
 }else if(detour==KnownDetour::Rock) {
  // Calls through ROCK's detour reach the native function inside ROCK's
  // player inventory scope, which its hand-slot mapping requires.
  const auto destination=equipment_policy::rockUnequipJump(actual)?jumpDestination(actual):0;
  const auto kind=equipment_policy::classifyUnequipEntry(actual,destination && moduleTarget(destination,rockModule()));
  accepted=kind!=equipment_policy::UnequipEntry::Unavailable;
  if(kind==equipment_policy::UnequipEntry::Rock) {
   spdlog::info("PALM native {}: supported ROCK detour at {:X}; existing hook retained",name,destination);
   return true;
  }
 }
 if(accepted)spdlog::info("PALM native {} available at RVA {:X}",name,rva);
 else spdlog::warn("PALM native {} unavailable at RVA {:X}: expected=[{}] actual=[{}]; wheel remains available",
  name,rva,hexBytes(expected),hexBytes(actual));
 return accepted;
}
}
void initializeEquipmentRuntime() noexcept {
 support={};
 try {
  if(!REL::Module::IsVR() || REL::Module::get().version()!=F4SE::RUNTIME_VR_1_2_72) {
   spdlog::error("PALM native inventory operations unavailable: unsupported runtime");return;
  }
  // Do not short-circuit: an equip conflict must not disable unequip or names.
  support.equip=validateEntry("equip",0xe6fea0,equipment_policy::equipEntry,KnownDetour::Heisenberg);
  support.unequip=validateEntry("unequip",0xe70280,equipment_policy::unequipEntry,KnownDetour::Rock);
  support.names=validateEntry("display names",0xc15f0,equipment_policy::nameEntry);
  spdlog::info("PALM native capabilities: equip={} unequip={} names={}; menu startup continues",support.equip,support.unequip,support.names);
 }catch(...){spdlog::error("PALM native capability initialization interrupted; unavailable operations stay disabled, wheel startup continues");}
}
EquipmentSupport equipmentSupport() noexcept {return support;}
const char* submitRockEquip(std::uint64_t owner,const RockEquip& equip,std::uint64_t& outCommand) noexcept {
 outCommand=0;
 try {
  const auto& services=rockServices();
  if(!owner || !services.client.owner() || !services.weapon)return "ROCK weapon interface unavailable";
  rock::api::weapon::EquipRequest request;
  request.hand=equip.hand==PointerHand::Left?rock::api::Hand::Left:rock::api::Hand::Right;
  using rock::api::Status;
  auto status=services.weapon->captureInventoryWeapon(owner,equip.form,equip.stackIndex,&request.item);
  if(status==Status::Ok)status=services.weapon->requestInventoryEquip(owner,&request,&outCommand);
  spdlog::info("PALM inventory equip {:08X} stack {} hand {}: status {} command {}",equip.form,equip.stackIndex,
   equip.hand==PointerHand::Left?"left":"right",static_cast<unsigned>(status),outCommand);
  if(status==Status::RequestQueued)return equip.hand==PointerHand::Left?"Equipping into left hand":"Equipping into right hand";
  outCommand=0;
  return status==Status::Busy?"Another weapon change is in progress":
   status==Status::TargetUnavailable?"That equipment variant is no longer carried; update its Config selection":
   status==Status::NotReady?"Weapon change unavailable right now":"ROCK refused that weapon change";
 }catch(...){spdlog::error("PALM ROCK equip request failed");outCommand=0;return "Equipment change failed";}
}
const char* toggleEquipment(const Item& selected,std::uint64_t owner,PointerHand hand,std::uint32_t inputThread,RockEquip& outRock) noexcept {
 outRock={};
 try {
  if(!support.names)return "Equipment names unavailable; saved choices are preserved";
  auto* player=RE::PlayerCharacter::GetSingleton();
  if(!player || !player->inventoryList)return "Inventory unavailable";
  auto* object=RE::TESForm::GetFormByID<RE::TESBoundObject>(selected.id);
  if(!object || (!object->Is(RE::ENUM_FORM_ID::kWEAP) && !object->Is(RE::ENUM_FORM_ID::kARMO)))return "Equipment no longer available";
  std::uint32_t stackIndex=0;bool found=false,equipped=false;
  RE::BSTSmartPointer<RE::TBO_InstanceData> instance;
  {
   const RE::BSAutoReadLock lock{player->inventoryList->rwLock};
   for(const auto& entry:player->inventoryList->data) {
    if(entry.object!=object)continue;
    std::uint32_t index=0;
    for(auto* stack=entry.stackData.get();stack && index<4096;stack=stack->nextStack.get(),++index) {
     if(!stack->GetCount())continue;
     const auto current=detail::equipmentItem(entry,*stack,index);
     if(current.key!=selected.key || current.key.empty())continue;
     // Identical variants may be split by ammo or health: prefer the equipped
     // stack so choosing the same favorite again always means unequip.
     if(!found || current.equipped) {
      found=true;stackIndex=index;equipped=current.equipped;instance.reset();
      if(stack->extra)if(const auto* extra=stack->extra->GetByType<RE::ExtraInstanceData>())instance=extra->data;
     }
     if(equipped)break;
    }
    break;
   }
  } // Never call the equipment manager while holding the inventory lock.
  if(!found)return "That equipment variant is no longer carried; update its Config selection";
  if(owner && !equipped && object->Is(RE::ENUM_FORM_ID::kWEAP)) {
   // ROCK owns hand placement: the weapon goes into the selecting hand and
   // replaces the weapon that hand holds; the other hand's weapon stays.
   // ROCK's weapon calls run on its owner thread, so only the stack is chosen here.
   if(hand==PointerHand::None)return "Selecting hand unknown; point at the weapon and select again";
   outRock={selected.id,stackIndex,hand};
   return hand==PointerHand::Left?"Equipping into left hand":"Equipping into right hand";
  }
  if(owner && equipped && object->Is(RE::ENUM_FORM_ID::kWEAP)) {
   for(const auto side:{rock::api::Hand::Left,rock::api::Hand::Right}) {
    rock::api::grab::HandInteractionState interaction{};
    const auto& services=rockServices();
    const auto status=services.client.owner() && services.grab?
     services.grab->getHandInteractionState(owner,side,&interaction):rock::api::Status::NotReady;
    if(status!=rock::api::Status::Ok ||
       !(interaction.flags&static_cast<std::uint32_t>(rock::api::grab::HandInteractionFlag::Valid))) {
     // One report per explicit equipment action, never per input/render frame.
     spdlog::warn("PALM equipment hand query rejected: weapon={:08X} hand={} status={} flags={:X} owner={:016X} inputThread={} taskThread={} frame={} generations[world,skeleton,provider]=[{},{},{}]",
      selected.id,side==rock::api::Hand::Left?"left":"right",static_cast<unsigned>(status),interaction.flags,owner,
      inputThread,GetCurrentThreadId(),interaction.frameIndex,interaction.worldGeneration,interaction.skeletonGeneration,interaction.providerGeneration);
     return "Hand state unavailable";
    }
    constexpr auto loose=static_cast<std::uint32_t>(rock::api::grab::HandInteractionFlag::LooseObject)|
     static_cast<std::uint32_t>(rock::api::grab::HandInteractionFlag::LooseWeapon);
    // LooseObject is also set for a merely highlighted world object. Only an
    // active handoff/hold (or detached part carry) owns the hand against equip.
    const bool holding=interaction.phase!=rock::api::grab::HandInteractionPhase::Idle &&
     interaction.phase!=rock::api::grab::HandInteractionPhase::Touching && interaction.phase!=rock::api::grab::HandInteractionPhase::Selecting;
    if(((interaction.flags&loose) && holding) ||
       (interaction.flags&static_cast<std::uint32_t>(rock::api::grab::HandInteractionFlag::PartCarry)))return "Put down the held object before changing weapons";
   }
  }
  if(!support.canToggle(equipped))return equipped?"Unequip unavailable: native function conflict":"Equip unavailable: native function conflict";
  auto* manager=RE::ActorEquipManager::GetSingleton();
  if(!manager)return "Equipment manager unavailable";
  RE::BGSObjectInstance objectInstance(object,instance.get());
  // The native equipment path also owns armor slot conflicts and updates ROCK's
  // observed equipped weapon. No inventory transfer, reference spawn, or forced drop.
  const bool accepted=equipped?
   manager->UnequipObject(player,&objectInstance,1,nullptr,stackIndex,false,false,true,true,nullptr):
   manager->EquipObject(player,objectInstance,stackIndex,1,nullptr,false,false,true,true,false);
  spdlog::info("PALM equipment {} {:08X}, current stack {}, accepted {}",equipped?"unequip":"equip",selected.id,stackIndex,accepted);
  if(!accepted)return "The game refused that equipment change";
  return equipped?"Unequip requested":"Equip requested";
 }catch(...){spdlog::error("PALM equipment change failed");return "Equipment change failed";}
}
const char* addHeldWeaponToWheel(std::uint32_t equipIndex,unsigned slot) noexcept {
 try {
  if(!support.names)return "Equipment names unavailable";
  auto* player=RE::PlayerCharacter::GetSingleton();
  auto* middle=player && player->currentProcess?player->currentProcess->middleHigh:nullptr;
  if(!middle || !player->inventoryList)return "Equipped weapon unavailable";
  // Identity comparison witnesses only; never dereferenced after the lock.
  const RE::TESForm* weapon{};const RE::TBO_InstanceData* instance{};
  {
   RE::BSAutoLock<RE::BSSpinLock> lock{middle->equippedItemsLock};
   if(middle->equippedItems.size()<=32)for(const auto& entry:middle->equippedItems)
    if(entry.equipIndex.index==equipIndex && entry.item.object && entry.item.object->IsWeapon()) {
     const auto* data=static_cast<const RE::TESObjectWEAP*>(entry.item.object);
     const auto type=entry.item.instanceData?static_cast<const RE::TESObjectWEAP::InstanceData*>(entry.item.instanceData.get())->type:data->weaponData.type;
     if(type==RE::WEAPON_TYPE::kGrenade || type==RE::WEAPON_TYPE::kMine)break;
     weapon=entry.item.object;instance=entry.item.instanceData.get();break;
    }
  }
  if(!weapon)return "No weapon held in that hand";
  // The equipped stack holding this exact instance; a native equip may clone
  // the instance payload, so a single equipped stack of the form also names it.
  Item exact,only;unsigned equippedStacks{};
  {
   const RE::BSAutoReadLock lock{player->inventoryList->rwLock};
   for(const auto& entry:player->inventoryList->data) {
    if(entry.object!=weapon)continue;
    std::uint32_t index=0;
    for(auto* stack=entry.stackData.get();stack && index<4096;stack=stack->nextStack.get(),++index) {
     if(!stack->GetCount() || !stack->IsEquipped())continue;
     const auto* extra=stack->extra?stack->extra->GetByType<RE::ExtraInstanceData>():nullptr;
     auto item=detail::equipmentItem(entry,*stack,index);
     if((extra?extra->data.get():nullptr)==instance)exact=item;
     ++equippedStacks;only=std::move(item);
    }
    break;
   }
  }
  const auto& item=!exact.key.empty()?exact:equippedStacks==1?only:exact;
  if(item.key.empty() || item.name.empty())return "That weapon cannot be identified for the wheel";
  const auto* message=addWheelFavorite(Category::Weapons,item,slot);
  spdlog::info("PALM add held weapon index {} {:08X} '{}' to slot {}: {}",equipIndex,item.id,item.name,slot,message);
  return message;
 }catch(...){spdlog::error("PALM add held weapon failed");return "Adding the weapon failed";}
}
const char* useInventoryItem(std::uint32_t id) noexcept {
 try {
  if(!support.equip)return "Item use unavailable: native equip conflict";
  auto* player=RE::PlayerCharacter::GetSingleton();
  auto* object=RE::TESForm::GetFormByID<RE::TESBoundObject>(id);
  if(!player || !player->inventoryList || !object ||
     (!object->Is(RE::ENUM_FORM_ID::kALCH) && !object->Is(RE::ENUM_FORM_ID::kWEAP)))return "Item no longer available";
  std::uint32_t index=0;bool found=false;
  RE::BSTSmartPointer<RE::TBO_InstanceData> instance;
  {
   const RE::BSAutoReadLock lock{player->inventoryList->rwLock};
   for(const auto& entry:player->inventoryList->data)if(entry.object==object) {
    for(auto* stack=entry.stackData.get();stack && index<4096;stack=stack->nextStack.get(),++index)if(stack->GetCount()) {
     found=true;if(stack->extra)if(auto* extra=stack->extra->GetByType<RE::ExtraInstanceData>())instance=extra->data;break;
    }
    break;
   }
  }
  if(!found)return "Item is no longer carried";
  auto* manager=RE::ActorEquipManager::GetSingleton();if(!manager)return "Equipment manager unavailable";
  RE::BGSObjectInstance item(object,instance.get());
  // Existing native inventory-use boundary, also used by ROCK's mouth consume.
  const bool accepted=manager->EquipObject(player,item,index,1,nullptr,false,false,true,false,false);
  return accepted?(object->Is(RE::ENUM_FORM_ID::kALCH)?"Use requested":"Grenade equip requested"):"The game refused that item action";
 }catch(...){spdlog::error("PALM native item action failed");return "Item action failed";}
}
}
