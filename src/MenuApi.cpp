#include "SectionRegistry.h"
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
