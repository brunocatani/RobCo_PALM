#include <RobCoPALM/Client.h>
#include <RobCoPALM/Menu.h>
#include <array>
#include <cstring>
#include <span>
#include <string_view>

// A physical magazine mod's wheel section. Resolve the query with
// GetProcAddress(GetModuleHandleW(palm::api::kModuleName), palm::api::kQueryExportName)
// and integrate these calls with the mod's own F4SE lifecycle. The callback
// resolves a mod-owned object token; magazine items do not need to be TESAmmo.
namespace magazine_example {
using namespace palm::api;
Result registerMagazines(const menu::Api& api,menu::SelectCallback callback,void* context,menu::SectionHandle& handle) noexcept {
 menu::Section section;
 std::strcpy(section.id,"example.physical-magazines"); // Replace with your mod's unique ID.
 std::strcpy(section.name,"Magazines");std::strcpy(section.modName,"Physical Magazine Mod");
 section.icon=Icon::CombatRifle;section.onSelect=callback;section.context=context;
 return api.registerSection(&section,&handle);
}
struct MagazineChoice {
 std::uint32_t id;
 std::uint64_t objectToken;
 std::string_view name;
 Icon weaponIcon;
 std::uint32_t quantity;
 bool available;
};
Result updateMagazines(const menu::Api& api,menu::SectionHandle handle,std::span<const MagazineChoice> choices) noexcept {
 if(choices.size()>menu::kMaxItems)return Result::InvalidArgument;
 std::array<menu::Item,menu::kMaxItems> items;
 for(std::size_t i=0;i<choices.size();++i) {
  const auto& choice=choices[i];auto& item=items[i];
  if(choice.name.empty() || choice.name.size()>=sizeof(item.name))return Result::InvalidArgument;
  item.id=choice.id;item.userData=choice.objectToken;item.icon=choice.weaponIcon;item.quantity=choice.quantity;
  item.flags=static_cast<unsigned>(menu::ItemFlag::ShowQuantity)|(choice.available?0:static_cast<unsigned>(menu::ItemFlag::Disabled));
  std::memcpy(item.name,choice.name.data(),choice.name.size());item.name[choice.name.size()]=0;
 }
 return api.setItems(handle,items.data(),static_cast<std::uint32_t>(choices.size()));
}
// On unload, call api.unregisterSection(handle). Release callback/context state
// only after Ok. If CallbackBusy is returned, retry later while retaining state.
Result connect(QueryInterfaceV1 query,const menu::Api*& api) noexcept {return acquire(query,api);}
}

int main() {}
