#include "PCH.h"
#include "Equipment.h"
#include "Inventory.h"
#include "ROCKProviderApi.h"
#include <array>
#include <cstring>

namespace wheel {
namespace {
bool equipmentReady=false; // Initialized before registering the runtime callback.
template<std::size_t N> bool matches(std::uintptr_t rva,const std::array<unsigned char,N>& bytes) {
 const auto address=REL::Offset(rva).address();
 MEMORY_BASIC_INFORMATION memory{};
 return VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory)) && memory.State==MEM_COMMIT &&
  !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
  address+N<=reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize &&
  std::memcmp(reinterpret_cast<const void*>(address),bytes.data(),N)==0;
}
}
bool validateEquipmentRuntime() noexcept {
 try {
  // VR-only entrypoints; caller/callee disassembly recorded in the workspace audit.
  equipmentReady=REL::Module::IsVR() && REL::Module::get().version()==F4SE::RUNTIME_VR_1_2_72 &&
   matches(0xe6fea0,std::array<unsigned char,8>{0x4c,0x8b,0xdc,0x49,0x89,0x53,0x10,0x55}) &&
   matches(0xe70280,std::array<unsigned char,8>{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55}) &&
   matches(0xc15f0,std::array<unsigned char,15>{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x30});
  spdlog::info("Wheel equipment native entrypoint guards: {}",equipmentReady?"accepted":"rejected");
  return equipmentReady;
 }catch(...){spdlog::error("Wheel equipment entrypoint validation failed");return false;}
}
const char* toggleEquipment(const Item& selected,std::uint64_t owner) noexcept {
 try {
  if(!equipmentReady)return "Equipment support unavailable";
  auto* player=RE::PlayerCharacter::GetSingleton();
  if(!player || !player->inventoryList)return "Inventory unavailable";
  auto* object=RE::TESForm::GetFormByID<RE::TESBoundObject>(selected.id);
  if(!object || (!object->Is(RE::ENUM_FORM_ID::kWEAP) && !object->Is(RE::ENUM_FORM_ID::kARMO)))return "Equipment no longer available";
  if(object->Is(RE::ENUM_FORM_ID::kWEAP)) {
   using namespace rock::provider;
   for(const auto hand:{RockProviderHand::Left,RockProviderHand::Right}) {
    RockProviderHandInteractionStateV1 interaction;
    if(!RockProviderApi::inst || !RockProviderApi::inst->getHandInteractionStateV1 ||
       RockProviderApi::inst->getHandInteractionStateV1(owner,hand,&interaction)!=RockProviderResultV1::Ok ||
       !(interaction.flags&static_cast<std::uint32_t>(RockProviderHandInteractionFlagV1::Valid)))return "Hand state unavailable";
    constexpr auto loose=static_cast<std::uint32_t>(RockProviderHandInteractionFlagV1::LooseObject)|
     static_cast<std::uint32_t>(RockProviderHandInteractionFlagV1::LooseWeapon);
    // LooseObject is also set for a merely highlighted world object. Only an
    // active handoff/hold (or detached part carry) owns the hand against equip.
    const bool holding=interaction.phase!=RockProviderHandInteractionPhaseV1::Idle &&
     interaction.phase!=RockProviderHandInteractionPhaseV1::Touching && interaction.phase!=RockProviderHandInteractionPhaseV1::Selecting;
    if(((interaction.flags&loose) && holding) ||
       (interaction.flags&static_cast<std::uint32_t>(RockProviderHandInteractionFlagV1::PartCarry)))return "Put down the held object before changing weapons";
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
  auto* manager=RE::ActorEquipManager::GetSingleton();
  if(!manager)return "Equipment manager unavailable";
  RE::BGSObjectInstance objectInstance(object,instance.get());
  // The native equipment path also owns armor slot conflicts and updates ROCK's
  // observed equipped weapon. No inventory transfer, reference spawn, or forced drop.
  const bool accepted=equipped?
   manager->UnequipObject(player,&objectInstance,1,nullptr,stackIndex,false,false,true,true,nullptr):
   manager->EquipObject(player,objectInstance,stackIndex,1,nullptr,false,false,true,true,false);
  spdlog::info("Wheel equipment {} {:08X}, current stack {}, accepted {}",equipped?"unequip":"equip",selected.id,stackIndex,accepted);
  if(!accepted)return "The game refused that equipment change";
  return equipped?"Unequip requested":"Equip requested";
 }catch(...){spdlog::error("Wheel equipment change failed");return "Equipment change failed";}
}
}
