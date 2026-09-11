#pragma once
#include "../SDK/include/PALMMenuApi.h"
#include <array>
#include <atomic>
#include <mutex>

namespace wheel {
struct SectionSnapshot {
 palm::api::SectionHandle handle{};
 palm::api::Icon icon{};
 char id[64]{},name[48]{},modName[48]{};
 std::array<palm::api::ItemV1,palm::api::kMaxItems> items;
 unsigned count{};
 bool enabled{};
};
struct SectionCatalog {
 std::array<SectionSnapshot,palm::api::kMaxSections> sections;
 unsigned count{};
 std::uint64_t revision{};
};
class SectionRegistry {
public:
 palm::api::Result registerSection(const palm::api::SectionV1*,palm::api::SectionHandle*);
 palm::api::Result unregisterSection(palm::api::SectionHandle);
 palm::api::Result setItems(palm::api::SectionHandle,const palm::api::ItemV1*,std::uint32_t);
 void clearItems();
 // Nonblocking render snapshot; a contended registry is retried on the next draw.
 bool snapshot(SectionCatalog&) const;
 std::uint64_t revision() const noexcept {return _revision.load();}
 // Only the host's validated game task dispatches callbacks.
 palm::api::Result dispatch(palm::api::SectionHandle,std::uint32_t item);
private:
 struct Entry {palm::api::SectionV1 registration;SectionSnapshot snapshot;bool invoking{};};
 Entry* find(palm::api::SectionHandle);
 mutable std::mutex _mutex;
 std::array<Entry,palm::api::kMaxSections> _entries;
 palm::api::SectionHandle _next{1};
 std::atomic_uint64_t _revision{1};
};
SectionRegistry& sectionRegistry();
}
