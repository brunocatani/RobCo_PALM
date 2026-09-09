#include "PCH.h"
#include "Inventory.h"
#include "WheelConfig.h"
#include "EquipmentIdentity.h"
#include <array>

namespace wheel {
namespace {
std::optional<Category> classify(RE::TESBoundObject* object) {
 if(!object || !object->GetPlayable(object->GetBaseInstanceData())) return {};
 if(object->Is(RE::ENUM_FORM_ID::kALCH)) {
  auto* aid=static_cast<RE::AlchemyItem*>(object);
  if(aid->IsPoison()) return {};
  return aid->IsFood()?Category::Food:Category::Aid;
 }
 if(object->Is(RE::ENUM_FORM_ID::kWEAP)) {
  const auto type=static_cast<RE::TESObjectWEAP*>(object)->weaponData.type;
  // Preserve the throwable types supported by ROCK's replaced quick-draw path.
  if(type==RE::WEAPON_TYPE::kGrenade || type==RE::WEAPON_TYPE::kMine)return Category::Grenades;
  return Category::Weapons;
 }
 if(object->Is(RE::ENUM_FORM_ID::kARMO))return Category::Armor;
 return {};
}
}
namespace detail {
Item equipmentItem(const RE::BGSInventoryItem& entry,const RE::BGSInventoryItem::Stack& stack,std::uint32_t index) {
 Item result;result.id=entry.object->GetFormID();result.count=stack.GetCount();result.equipped=stack.IsEquipped();result.stackIndex=index;
 const auto* file=entry.object->GetFile(0);
 const auto base=file?stableItemKey(file->filename,result.id):std::string{};
 if(base.empty())return result;
 const char* name=nullptr;
 if(stack.extra) {
  if(auto* text=stack.extra->GetByType<RE::ExtraTextDisplayData>()) {
   name=text->GetDisplayName(entry.object).c_str();
  }
 }
 if(!name || !*name)name=RE::TESFullName::GetFullName(*entry.object,false).data();
 if(!name || !*name)return result;
 result.name.assign(name,strnlen_s(name,512));
 std::vector<std::string> modifiers;
 if(stack.extra)if(const auto* extra=stack.extra->GetByType<RE::BGSObjectInstanceExtra>();extra && extra->values) {
  const auto indices=extra->GetIndexData();
  if(indices.size()>128)return result;
  modifiers.reserve(indices.size());
  for(const auto& mod:indices) {
   const auto* form=RE::TESForm::GetFormByID(mod.objectID);
   const auto* owner=form?form->GetFile(0):nullptr;
   const auto modKey=owner?stableItemKey(owner->filename,mod.objectID):std::string{};
   if(modKey.empty())return result; // Do not confuse an unresolved attachment with an unmodified item.
   modifiers.push_back(modKey+":"+std::to_string(mod.index)+":"+std::to_string(mod.rank)+":"+std::to_string(mod.disabled));
  }
 }
 std::sort(modifiers.begin(),modifiers.end());
 result.key=equipmentKey(base,result.name,modifiers);
 return result;
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
   if(isEquipment(*category)) {
    auto& items=result.items[static_cast<unsigned>(*category)];
    std::uint32_t index=0;
    for(auto* stack=entry.stackData.get();stack && index<4096;stack=stack->nextStack.get(),++index) {
     if(!stack->GetCount() || items.size()>=kMaxItems)continue;
     auto item=detail::equipmentItem(entry,*stack,index);
     if(item.key.empty() || item.name.empty())continue;
     const auto duplicate=std::find_if(items.begin(),items.end(),[&](const Item& other){return other.key==item.key;});
     if(duplicate==items.end())items.push_back(std::move(item));
     else {
      duplicate->count=static_cast<std::uint32_t>((std::min)(static_cast<std::uint64_t>(duplicate->count)+item.count,static_cast<std::uint64_t>(UINT32_MAX)));
      if(item.equipped){duplicate->equipped=true;duplicate->stackIndex=item.stackIndex;}
     }
    }
    continue;
   }
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
   const auto* owner=entry.object->GetFile(0);
   const auto key=owner?stableItemKey(owner->filename,entry.object->GetFormID()):std::string{};
   items.push_back({entry.object->GetFormID(),std::string(name),static_cast<std::uint32_t>(std::min<std::uint64_t>(count,UINT32_MAX)),equipped,key});
  }
 }
 for(auto& items:result.items) std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.name==b.name?a.id<b.id:a.name<b.name;});
 publishWheelInventory(result);
 return selectedWheelInventory(result);
}
}
