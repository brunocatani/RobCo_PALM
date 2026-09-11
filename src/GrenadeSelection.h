#pragma once
#include <ROCKConfigurationApi.h>
#include <optional>
#include <string_view>

namespace wheel {
// Game task thread only. Read the applied catalog at selection time so Config
// changes take effect without a second INI reader or a cached mode in PALM.
inline std::optional<bool> immersiveGrenadesEnabled(const rock::configuration_api::ApiV1* api) noexcept {
 using namespace rock::configuration_api;
 if(!api || api->version!=kVersion || api->byteSize<sizeof(ApiV1) || !api->visit)return {};
 std::optional<bool> enabled;
 const auto visitor=[](const SettingV1* setting,void* context) noexcept {
  if(!setting || !setting->section || !setting->key || !setting->value || setting->type!=ValueType::Boolean)return;
  if(std::string_view(setting->section)!="RealisticWeapons" || std::string_view(setting->key)!="bImmersiveGrenades")return;
  auto& result=*static_cast<std::optional<bool>*>(context);
  if(std::string_view(setting->value)=="true")result=true;
  else if(std::string_view(setting->value)=="false")result=false;
 };
 if(!api->visit(Group::Consumer,visitor,&enabled))return {};
 return enabled;
}
}
