#pragma once
#include "SectionRegistry.h"
#include "../SDK/include/PALMDeveloperApi.h"
#include <string_view>

namespace wheel {
inline constexpr char kDeveloperSection[]="palm.developer";
inline bool isDeveloperSection(std::string_view id){return id==kDeveloperSection;}
class DeveloperActions {
public:
 explicit DeveloperActions(SectionRegistry& registry):registry_(registry){}
 bool initialize(bool enabled);
 bool enabled() const noexcept {return enabled_.load(std::memory_order_acquire);}
 bool ownsSection(palm::api::SectionHandle handle) const noexcept {return enabled() && handle && handle==section_;}
 palm::developer::Result add(const palm::developer::ActionV1*,palm::developer::Handle*);
 palm::developer::Result remove(palm::developer::Handle);
 void refresh();
private:
 struct Entry {palm::developer::ActionV1 action; palm::developer::Handle handle{}; bool invoking{};};
 void publish(); // mutex held
 void dispatch(std::uint32_t);
 static void selected(std::uint32_t,std::uint64_t,void*) noexcept;
 SectionRegistry& registry_;
 std::mutex mutex_;
 std::array<Entry,palm::developer::kCapacity> entries_{};
 std::atomic_bool enabled_{};
 palm::api::SectionHandle section_{};
 palm::developer::Handle next_{1};
};
DeveloperActions& developerActions();
}
