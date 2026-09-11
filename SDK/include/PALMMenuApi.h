#pragma once
#include "PALMIcons.h"
#include <cstdint>
#include <type_traits>

namespace palm::api {
inline constexpr std::uint32_t kVersion=1;
inline constexpr const char* kExportName="GetPALMMenuApi";
inline constexpr const wchar_t* kModuleName=L"wheelmenu.dll";
inline constexpr std::uint32_t kMaxSections=10;
inline constexpr std::uint32_t kMaxItems=8;
inline constexpr std::uint32_t kMaxVisibleEntries=10; // Includes PALM's Config entry.
using SectionHandle=std::uint32_t;
enum class Result : std::uint32_t { Ok, InvalidArgument, NotFound, DuplicateId, CapacityReached, CallbackBusy, ItemUnavailable, InternalError };
enum class ItemFlag : std::uint32_t { Disabled=1, Equipped=2, ShowQuantity=4 };

// Called on the F4SE game task thread without PALM locks. In press/click mode
// the wheel remains open; consumers must not depend on it closing before dispatch.
// The mod owns the action: vanilla equipment, ROCK requests, or another feature.
// Never throw across this callback. Keep the module and context alive until
// unregisterSection succeeds. CallbackBusy means an invocation is still running;
// retry unregister later, without blocking a game/input/render callback.
using SelectCallback=void (*)(std::uint32_t itemId,std::uint64_t userData,void* context) noexcept;
struct SectionV1 {
 std::uint32_t structSize{sizeof(SectionV1)};
 Icon icon{Icon::Config};
 char id[64]{}; // Stable, globally unique ASCII ID: letters, digits, '_', '-', '.'.
 char name[48]{};
 char modName[48]{};
 SelectCallback onSelect{};
 void* context{};
};
struct ItemV1 {
 std::uint32_t structSize{sizeof(ItemV1)};
 std::uint32_t id{}; // Nonzero, stable within this section. Not necessarily a FormID.
 Icon icon{Icon::Config};
 std::uint32_t quantity{};
 std::uint32_t flags{};
 char name[96]{};
 std::uint64_t userData{};
};

// Thread-safe, bounded calls. Text and items are copied before returning; only
// the callback/context are borrowed. No engine pointers enter the API.
// Register once per plugin lifetime, replace the item snapshot when it changes,
// and unregister before releasing callback state. Handles are never reused.
// New sections start hidden; users enable them in PALM settings. Registration
// survives game loads; PALM empties item snapshots at the start of a game load.
// Republish from a game task queued after a successful PostLoadGame or NewGame.
// Removing a section or item cancels any queued selection that refers to it.
struct ApiV1 {
 std::uint32_t version;
 std::uint32_t byteSize;
 Result (*registerSection)(const SectionV1*,SectionHandle*) noexcept;
 Result (*unregisterSection)(SectionHandle) noexcept;
 Result (*setItems)(SectionHandle,const ItemV1*,std::uint32_t count) noexcept;
};
using GetApi=const ApiV1* (*)(std::uint32_t version) noexcept;
static_assert(sizeof(void*)==8);
static_assert(sizeof(SectionV1)==184 && sizeof(ItemV1)==128 && sizeof(ApiV1)==32);
static_assert(std::is_standard_layout_v<SectionV1> && std::is_trivially_copyable_v<ItemV1>);
}
