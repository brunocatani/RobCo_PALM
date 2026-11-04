#pragma once
#include "SectionRegistry.h"
#include <RobCoPALM/Developer.h>
#include <string_view>

namespace wheel {
inline constexpr char kDeveloperSection[]="palm.developer";
inline bool isDeveloperSection(std::string_view id){return id==kDeveloperSection;}
class DeveloperActions {
public:
 explicit DeveloperActions(SectionRegistry& registry):registry_(registry){}
 bool initialize(bool enabled);
 bool enabled() const noexcept {return enabled_.load(std::memory_order_acquire);}
 bool ownsSection(palm::api::menu::SectionHandle handle) const noexcept {return enabled() && handle && handle==section_;}
 palm::api::Result add(const palm::api::developer::Action*,palm::api::developer::Handle*);
 palm::api::Result remove(palm::api::developer::Handle);
 void refresh();
private:
 struct Entry {palm::api::developer::Action action; palm::api::developer::Handle handle{}; bool invoking{};};
 void publish(); // mutex held
 void dispatch(std::uint32_t);
 static void selected(std::uint32_t,std::uint64_t,void*) noexcept;
 SectionRegistry& registry_;
 std::mutex mutex_;
 std::array<Entry,palm::api::developer::kCapacity> entries_{};
 std::atomic_bool enabled_{};
 palm::api::menu::SectionHandle section_{};
 palm::api::developer::Handle next_{1};
};
DeveloperActions& developerActions();
}
