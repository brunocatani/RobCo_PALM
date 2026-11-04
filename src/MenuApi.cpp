#include "SectionRegistry.h"
#include "DeveloperActions.h"
#include "InterfaceNegotiation.h"
#include "RequiredUpdates.h"
#include <spdlog/spdlog.h>
#include <array>
#include <intrin.h>

namespace {
using namespace palm::api;
Result PALM_CALL registerSection(const menu::Section* section,menu::SectionHandle* out) noexcept {
 try{return wheel::sectionRegistry().registerSection(section,out);}
 catch(...){if(out)*out=0;spdlog::error("PALM section registration failed");return Result::InternalError;}
}
Result PALM_CALL unregisterSection(menu::SectionHandle section) noexcept {
 try{return wheel::sectionRegistry().unregisterSection(section);}
 catch(...){spdlog::error("PALM section removal failed");return Result::InternalError;}
}
Result PALM_CALL setItems(menu::SectionHandle section,const menu::Item* items,std::uint32_t count) noexcept {
 try{return wheel::sectionRegistry().setItems(section,items,count);}
 catch(...){spdlog::error("PALM section item update failed");return Result::InternalError;}
}
const menu::Api menuApi{registerSection,unregisterSection,setItems};

bool PALM_CALL developerEnabled() noexcept {return wheel::developerActions().enabled();}
Result PALM_CALL registerDeveloper(const developer::Action* action,developer::Handle* out) noexcept {
 try {
  const auto result=wheel::developerActions().add(action,out);
  if(result==Result::CapacityReached)spdlog::warn("Developer wheel full (8 actions); existing actions preserved. Ask the user which action may be removed before replacing one.");
  return result;
 }catch(...){if(out)*out=0;spdlog::error("Developer action registration failed");return Result::InternalError;}
}
Result PALM_CALL removeDeveloper(developer::Handle handle) noexcept {
 try{return wheel::developerActions().remove(handle);}catch(...){spdlog::error("Developer action removal failed");return Result::InternalError;}
}
const developer::Api developerApi{developerEnabled,registerDeveloper,removeDeveloper};

const std::array interfaces{wheel::discovery::describe(menuApi),wheel::discovery::describe(developerApi)};
}

extern "C" __declspec(dllexport) palm::api::Result PALM_CALL PALMAPI_QueryInterfaceV1(const palm::api::InterfaceId id,
 const std::uint32_t exactMajor,const std::uint32_t minimumMinor,const std::uint32_t minimumTableBytes,
 const palm::api::InterfaceDescriptorV1** output) noexcept {
 const auto result=wheel::discovery::query(interfaces,id,exactMajor,minimumMinor,minimumTableBytes,output);
 if(result!=palm::api::Result::Ok)wheel::required_updates::noteDiscoveryRefusal(result,id,exactMajor,_ReturnAddress());
 return result;
}

// Retired pre-2.0 exports. They stay exported so a mod still built against them
// is named in the required-update stop instead of failing on a missing symbol;
// each returns no table. Their names are never reused.
extern "C" __declspec(dllexport) const void* PALM_CALL GetPALMMenuApi(std::uint32_t) noexcept {
 wheel::required_updates::noteRetiredExport("GetPALMMenuApi",_ReturnAddress());
 return nullptr;
}
extern "C" __declspec(dllexport) const void* PALM_CALL GetPALMDeveloperApi(std::uint32_t) noexcept {
 wheel::required_updates::noteRetiredExport("GetPALMDeveloperApi",_ReturnAddress());
 return nullptr;
}
