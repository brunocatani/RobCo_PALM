#include "PCH.h"
#include "Inventory.h"
#include "WheelConfig.h"
#include "EquipmentIdentity.h"
#include "Equipment.h"
#include "ItemIconPolicy.h"
#include <array>

namespace wheel {
namespace {
Icon inventoryIcon(RE::TESBoundObject* object,const RE::TBO_InstanceData* instance=nullptr) {
 const auto* file=object->GetFile(0);
 const auto known=knownItemIcon(file && _stricmp(file->filename,"Fallout4.esm")==0,object->GetFormID()&0xffffffu);
 if(object->Is(RE::ENUM_FORM_ID::kWEAP)) {
  const auto* weapon=static_cast<RE::TESObjectWEAP*>(object);
  return weaponIcon(known,static_cast<std::uint8_t>(*weapon->weaponData.type),[&](std::uint32_t id) {
   const auto* keyword=RE::TESForm::GetFormByID<RE::BGSKeyword>(id);
   return keyword && weapon->HasKeyword(keyword,instance);
  });
 }
 if(object->Is(RE::ENUM_FORM_ID::kARMO)) {
  const auto* armor=static_cast<RE::TESObjectARMO*>(object);
  const auto* power=RE::TESForm::GetFormByID<RE::BGSKeyword>(0x0004D8A1);
  return armorIcon(armor->GetSlotMask(),power && armor->HasKeyword(power,instance));
 }
 if(known!=Icon::Automatic)return known;
 return static_cast<RE::AlchemyItem*>(object)->IsFood()?Icon::Food:Icon::Chems;
}
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
// A loose reference that is still in the world and not owned by a container.
RE::TESObjectREFR* looseReference(std::uint32_t referenceId) {
 auto* ref=RE::TESForm::GetFormByID<RE::TESObjectREFR>(referenceId);
 return ref && !ref->IsDeleted() && !ref->IsDisabled() && ref->GetObjectReference()?ref:nullptr;
}
// ExtraCount's count, the layout ROCK's shoulder stash reads (ShoulderStashTransfer.cpp).
std::int32_t referenceCount(RE::TESObjectREFR* ref) {
 auto* extra=ref->extraList?ref->extraList->GetByType(RE::EXTRA_DATA_TYPE::kCount):nullptr;
 if(!extra)return 1;
 std::uint16_t count{};std::memcpy(&count,reinterpret_cast<const std::uint8_t*>(extra)+0x18,sizeof(count));
 return (std::max<std::int32_t>)(1,count);
}
}
const char* addHeldItemToWheel(std::uint32_t referenceId,Category category,unsigned slot,std::uint32_t& outBase) noexcept {
 outBase=0;
 try {
  auto* ref=looseReference(referenceId);
  if(!ref)return "The held item is gone";
  auto* base=ref->GetObjectReference();
  const auto kind=classify(base);
  if(kind!=category)return category==Category::Food?"Hold a food item to add it here":"Hold an aid item to add it here";
  const auto name=RE::TESFullName::GetFullName(*base,false);
  const auto* file=base->GetFile(0);
  const auto key=file?stableItemKey(file->filename,base->GetFormID()):std::string{};
  if(name.empty() || key.empty())return "That item cannot be identified for the wheel";
  const char* message{};
  const bool added=addWheelFavorite(category,{base->GetFormID(),std::string(name),1,false,key},slot,message);
  spdlog::info("PALM add held item {:08X} (reference {:08X}) '{}' to {} slot {}: {}",base->GetFormID(),referenceId,name,categoryName(category),slot,message);
  if(added)outBase=base->GetFormID();
  return message;
 }catch(...){spdlog::error("PALM add held item failed");return "Adding the item failed";}
}
const char* storeReleasedItem(std::uint32_t referenceId,std::uint32_t baseId) noexcept {
 try {
  auto* player=RE::PlayerCharacter::GetSingleton();
  auto* ref=looseReference(referenceId);
  if(!player || !ref || ref->GetObjectReference()->GetFormID()!=baseId)return "Added to the wheel; the item was gone before it could be stored";
  // The native pickup, as ROCK's own consume and stash paths take a released reference.
  const bool taken=ref->ActivateRef(player,nullptr,referenceCount(ref),false,false,false);
  spdlog::info("PALM store held item {:08X} (reference {:08X}): {}",baseId,referenceId,taken);
  return taken?"Added to the wheel and stored":"Added to the wheel; it could not be stored";
 }catch(...){spdlog::error("PALM store held item failed");return "Added to the wheel; storing it failed";}
}
namespace detail {
Item equipmentItem(const RE::BGSInventoryItem& entry,const RE::BGSInventoryItem::Stack& stack,std::uint32_t index) {
 // No base-name substitute: it could rebind two distinct saved variants.
 if(!equipmentSupport().names)return {};
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
 result.equipmentNamesAvailable=equipmentSupport().names;
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
    if(!result.equipmentNamesAvailable)continue;
    auto& items=result.items[static_cast<unsigned>(*category)];
    std::uint32_t index=0;
    for(auto* stack=entry.stackData.get();stack && index<4096;stack=stack->nextStack.get(),++index) {
     if(!stack->GetCount() || items.size()>=kMaxItems)continue;
     auto item=detail::equipmentItem(entry,*stack,index);
     if(item.key.empty() || item.name.empty())continue;
     const auto* extra=stack->extra?stack->extra->GetByType<RE::ExtraInstanceData>():nullptr;
     item.icon=inventoryIcon(entry.object,extra?extra->data.get():nullptr);
     const auto duplicate=std::find_if(items.begin(),items.end(),[&](const Item& other){return other.key==item.key;});
     if(duplicate==items.end())items.push_back(std::move(item));
     else {
      duplicate->count=static_cast<std::uint32_t>((std::min)(static_cast<std::uint64_t>(duplicate->count)+item.count,static_cast<std::uint64_t>(UINT32_MAX)));
      if(item.equipped){duplicate->equipped=true;duplicate->stackIndex=item.stackIndex;duplicate->icon=item.icon;}
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
   items.push_back({entry.object->GetFormID(),std::string(name),static_cast<std::uint32_t>(std::min<std::uint64_t>(count,UINT32_MAX)),equipped,key,0,inventoryIcon(entry.object)});
  }
 }
 for(auto& items:result.items) std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.name==b.name?a.id<b.id:a.name<b.name;});
 if(const auto removed=publishWheelInventory(result))spdlog::info("PALM removed {} weapon choice(s) no longer carried",removed);
 return selectedWheelInventory(result);
}
}
