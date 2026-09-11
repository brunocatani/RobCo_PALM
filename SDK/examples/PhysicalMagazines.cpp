#include <Windows.h>
#include <PALMMenuApi.h>
#include <array>
#include <cstring>
#include <span>
#include <string_view>

// Integrate these calls with the magazine mod's own F4SE lifecycle. The callback
// resolves a mod-owned object token; magazine items do not need to be TESAmmo.
namespace magazine_example {
using namespace palm::api;
const ApiV1* findPALM() noexcept {
 const auto module=GetModuleHandleW(kModuleName);
 const auto get=module?reinterpret_cast<GetApi>(GetProcAddress(module,kExportName)):nullptr;
 const auto* api=get?get(kVersion):nullptr;
 return api && api->version==kVersion && api->byteSize>=sizeof(ApiV1)?api:nullptr;
}
Result registerMagazines(const ApiV1& api,SelectCallback callback,void* context,SectionHandle& handle) noexcept {
 SectionV1 section;
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
Result updateMagazines(const ApiV1& api,SectionHandle handle,std::span<const MagazineChoice> choices) noexcept {
 if(choices.size()>kMaxItems)return Result::InvalidArgument;
 std::array<ItemV1,kMaxItems> items;
 for(std::size_t i=0;i<choices.size();++i) {
  const auto& choice=choices[i];auto& item=items[i];
  if(choice.name.empty() || choice.name.size()>=sizeof(item.name))return Result::InvalidArgument;
  item.id=choice.id;item.userData=choice.objectToken;item.icon=choice.weaponIcon;item.quantity=choice.quantity;
  item.flags=static_cast<unsigned>(ItemFlag::ShowQuantity)|(choice.available?0:static_cast<unsigned>(ItemFlag::Disabled));
  std::memcpy(item.name,choice.name.data(),choice.name.size());item.name[choice.name.size()]=0;
 }
 return api.setItems(handle,items.data(),static_cast<std::uint32_t>(choices.size()));
}
// On unload, call api.unregisterSection(handle). Release callback/context state
// only after Ok. If CallbackBusy is returned, retry later while retaining state.
}
