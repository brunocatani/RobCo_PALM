#include "RockServices.h"
#include "PCH.h"
#include "Equipment.h"
#include "Inventory.h"
#include "RockFrame.h"
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
bool heisenbergTarget(std::uintptr_t address) noexcept {
 MEMORY_BASIC_INFORMATION memory{};
 constexpr DWORD executable=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
 HMODULE owner{};
 const auto heisenberg=GetModuleHandleW(L"Heisenberg_F4VR.dll");
 return heisenberg && VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) &&
  memory.State==MEM_COMMIT && memory.Type==MEM_IMAGE && (memory.Protect&executable) &&
  !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCWSTR>(address),&owner) && owner==heisenberg;
}
std::string hexBytes(std::span<const std::uint8_t> bytes) {
 std::string result;
 for(auto byte:bytes){if(!result.empty())result+=' ';result+=std::format("{:02X}",byte);}
 return result;
}
template<std::size_t N> bool validateEntry(const char* name,std::uintptr_t rva,const std::array<std::uint8_t,N>& expected) {
 std::array<std::uint8_t,N> actual{};
 const auto address=REL::Offset(rva).address();
 if(!readBytes(address,actual.data(),actual.size())) {
  spdlog::warn("PALM native {} unavailable: unreadable entry at RVA {:X}; wheel remains available",name,rva);return false;
 }
 bool accepted=equipment_policy::prefixMatches(actual,expected);
 if constexpr(N==equipment_policy::equipEntry.size()) {
  std::uintptr_t destination{};
  if(equipment_policy::heisenbergEquipJump(actual))std::memcpy(&destination,actual.data()+6,sizeof(destination));
  const auto kind=equipment_policy::classifyEquipEntry(actual,destination && heisenbergTarget(destination));
  accepted=kind!=equipment_policy::EquipEntry::Unavailable;
  if(kind==equipment_policy::EquipEntry::Heisenberg) {
   spdlog::info("PALM native {}: supported Heisenberg_F4VR.dll detour at {:X}; existing hook retained",name,destination);
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
  support.equip=validateEntry("equip",0xe6fea0,equipment_policy::equipEntry);
  support.unequip=validateEntry("unequip",0xe70280,equipment_policy::unequipEntry);
  support.names=validateEntry("display names",0xc15f0,equipment_policy::nameEntry);
  spdlog::info("PALM native capabilities: equip={} unequip={} names={}; menu startup continues",support.equip,support.unequip,support.names);
 }catch(...){spdlog::error("PALM native capability initialization interrupted; unavailable operations stay disabled, wheel startup continues");}
}
EquipmentSupport equipmentSupport() noexcept {return support;}
const char* toggleEquipment(const Item& selected,std::uint64_t owner,std::uint32_t inputThread) noexcept {
 try {
  if(!support.names)return "Equipment names unavailable; saved choices are preserved";
  auto* player=RE::PlayerCharacter::GetSingleton();
  if(!player || !player->inventoryList)return "Inventory unavailable";
  auto* object=RE::TESForm::GetFormByID<RE::TESBoundObject>(selected.id);
  if(!object || (!object->Is(RE::ENUM_FORM_ID::kWEAP) && !object->Is(RE::ENUM_FORM_ID::kARMO)))return "Equipment no longer available";
  if(owner && object->Is(RE::ENUM_FORM_ID::kWEAP)) {

   for(const auto hand:{rock::api::Hand::Left,rock::api::Hand::Right}) {
    rock::api::grab::HandInteractionState interaction{};
    const auto& services=rockServices();
    const auto status=services.client.owner() && services.grab?
     services.grab->getHandInteractionState(owner,hand,&interaction):rock::api::Status::NotReady;
    if(status!=rock::api::Status::Ok ||
       !(interaction.flags&static_cast<std::uint32_t>(rock::api::grab::HandInteractionFlag::Valid))) {
     // One report per explicit equipment action, never per input/render frame.
     spdlog::warn("PALM equipment hand query rejected: weapon={:08X} hand={} status={} flags={:X} owner={:016X} inputThread={} taskThread={} frame={} generations[world,skeleton,provider]=[{},{},{}]",
      selected.id,hand==rock::api::Hand::Left?"left":"right",static_cast<unsigned>(status),interaction.flags,owner,
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
