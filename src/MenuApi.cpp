#include "SectionRegistry.h"
#include "DeveloperActions.h"
#include <spdlog/spdlog.h>

namespace {
using namespace palm::api;
Result registerSection(const SectionV1* section,SectionHandle* out) noexcept {
 try{return wheel::sectionRegistry().registerSection(section,out);}
 catch(...){if(out)*out=0;spdlog::error("PALM section registration failed");return Result::InternalError;}
}
Result unregisterSection(SectionHandle section) noexcept {
 try{return wheel::sectionRegistry().unregisterSection(section);}
 catch(...){spdlog::error("PALM section removal failed");return Result::InternalError;}
}
Result setItems(SectionHandle section,const ItemV1* items,std::uint32_t count) noexcept {
 try{return wheel::sectionRegistry().setItems(section,items,count);}
 catch(...){spdlog::error("PALM section item update failed");return Result::InternalError;}
}
const ApiV1 api{kVersion,sizeof(ApiV1),registerSection,unregisterSection,setItems};
}
extern "C" __declspec(dllexport) const palm::api::ApiV1* GetPALMMenuApi(std::uint32_t version) noexcept {
 return version==palm::api::kVersion?&api:nullptr;
}

namespace {
bool developerEnabled() noexcept {return wheel::developerActions().enabled();}
palm::developer::Result registerDeveloper(const palm::developer::ActionV1* action,palm::developer::Handle* out) noexcept {
 try {
  const auto result=wheel::developerActions().add(action,out);
  if(result==palm::developer::Result::CapacityReached)spdlog::warn("Developer wheel full (8 actions); existing actions preserved. Ask the user which action may be removed before replacing one.");
  return result;
 }catch(...){if(out)*out=0;spdlog::error("Developer action registration failed");return palm::developer::Result::InternalError;}
}
palm::developer::Result removeDeveloper(palm::developer::Handle handle) noexcept {
 try{return wheel::developerActions().remove(handle);}catch(...){spdlog::error("Developer action removal failed");return palm::developer::Result::InternalError;}
}
const palm::developer::ApiV1 developerApi{palm::developer::kVersion,sizeof(palm::developer::ApiV1),developerEnabled,registerDeveloper,removeDeveloper};
}
extern "C" __declspec(dllexport) const palm::developer::ApiV1* GetPALMDeveloperApi(std::uint32_t version) noexcept {
 return version==palm::developer::kVersion?&developerApi:nullptr;
}
