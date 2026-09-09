#include "PCH.h"
#include "Inventory.h"

namespace wheel {
namespace {
std::optional<Category> classify(RE::TESBoundObject* object) {
 if(!object || !object->GetPlayable(object->GetBaseInstanceData())) return {};
 if(object->Is(RE::ENUM_FORM_ID::kALCH)) {
  auto* aid=static_cast<RE::AlchemyItem*>(object);
  if(aid->IsPoison()) return {};
  return aid->IsFood()?Category::Food:Category::Aid;
 }
 if(object->Is(RE::ENUM_FORM_ID::kWEAP) && static_cast<RE::TESObjectWEAP*>(object)->weaponData.type==RE::WEAPON_TYPE::kGrenade)
  return Category::Grenades;
 return {};
}
}
Model readInventory() {
 Model result;
 auto* player=RE::PlayerCharacter::GetSingleton();
 if(!player || !player->inventoryList) {result.status="Inventory unavailable";return result;}
 {
  const RE::BSAutoReadLock lock{player->inventoryList->rwLock};
  std::size_t scanned=0;
  for(auto& entry:player->inventoryList->data) {
   if(++scanned>16384) {result.status="Inventory scan limit reached";break;}
   const auto category=classify(entry.object);
   if(!category) continue;
   std::uint64_t count=0; bool equipped=false; std::size_t stacks=0;
   for(auto* stack=entry.stackData.get();stack;stack=stack->nextStack.get()) {
    if(++stacks>4096) {result.status="Inventory stack limit reached";count=0;break;}
    count+=stack->GetCount(); equipped|=stack->IsEquipped();
   }
   if(count==0) continue;
   auto& items=result.items[static_cast<unsigned>(*category)];
   if(items.size()>=kMaxItems) {result.status="Category item limit reached";continue;}
   const auto name=RE::TESFullName::GetFullName(*entry.object,false);
   if(name.empty()) continue;
   items.push_back({entry.object->GetFormID(),std::string(name),static_cast<std::uint32_t>(std::min<std::uint64_t>(count,UINT32_MAX)),equipped});
  }
 }
 for(auto& items:result.items) std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.name==b.name?a.id<b.id:a.name<b.name;});
 return result;
}
bool useInventoryItem(std::uint32_t id,std::string& message) {
 auto* player=RE::PlayerCharacter::GetSingleton();
 auto* manager=RE::ActorEquipManager::GetSingleton();
 auto* form=RE::TESForm::GetFormByID(id);
 auto* object=form?form->As<RE::TESBoundObject>():nullptr;
 const auto category=classify(object);
 if(!player || !manager || !player->inventoryList || !category) {message="Item unavailable";return false;}
 std::uint32_t chosenStack=UINT32_MAX;
 RE::BSTSmartPointer<RE::TBO_InstanceData> instance;
 {
  const RE::BSAutoReadLock lock{player->inventoryList->rwLock};
  std::size_t scanned=0;
  for(auto& entry:player->inventoryList->data) {
   if(++scanned>16384)break;
   if(entry.object!=object)continue;
   std::uint32_t stackID=0;
   for(auto* stack=entry.stackData.get();stack && stackID<4096;stack=stack->nextStack.get(),++stackID) {
    if(!stack->GetCount())continue;
    chosenStack=stackID;
    if(stack->extra)if(auto* extra=stack->extra->GetByType<RE::ExtraInstanceData>())instance=extra->data;
    break;
   }
   break;
  }
 }
 if(chosenStack==UINT32_MAX) {message="You no longer carry this item";return false;}
 const RE::BGSObjectInstance objectInstance(object,instance.get());
 // Uses ROCK's existing native inventory-use dispatch. The consumable path can
 // return false after dispatch; it is not an acknowledgement of consumption.
 const bool equipped=manager->EquipObject(player,objectInstance,chosenStack,1,nullptr,false,false,true,false,false);
 if(*category==Category::Grenades) {
  message=equipped?"Grenade equipped — use your normal quick draw":"Grenade equip was rejected";
  return equipped;
 }
 message="Item use requested";
 return true;
}
}
