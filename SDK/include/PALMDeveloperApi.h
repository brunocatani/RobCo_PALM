#pragma once
#include <cstdint>
#include <type_traits>

namespace palm::developer {
inline constexpr std::uint32_t kVersion=1;
inline constexpr std::uint32_t kCapacity=8;
inline constexpr char kExportName[]="GetPALMDeveloperApi";
inline constexpr wchar_t kModuleName[]=L"wheelmenu.dll";
using Handle=std::uint32_t;
enum class Result:std::uint32_t {Ok,Disabled,InvalidArgument,DuplicateId,CapacityReached,NotFound,CallbackBusy,InternalError};
// Game-task callback: enqueue work to the owning mod's appropriate thread.
// No exceptions or blocking capture/file operations across this boundary.
using Callback=void(*)(void*) noexcept;
struct ActionV1 {
 std::uint32_t byteSize{sizeof(ActionV1)};
 char id[64]{}; // Stable mod-qualified ID. Registration never replaces another ID.
 char label[96]{}; // Include the mod name; all mods share one eight-action wheel.
 Callback invoke{};
 void* context{};
};
// Independent interface: the released PALM Menu API remains unchanged.
// Registration survives game loads. Borrowed callback/context must remain
// alive until unregister succeeds; CallbackBusy requires a nonblocking retry.
// CapacityReached preserves every existing action; obtain user approval before
// unregistering an existing action to make room. No automatic eviction.
struct ApiV1 {
 std::uint32_t version;
 std::uint32_t byteSize;
 bool(*enabled)() noexcept;
 Result(*registerAction)(const ActionV1*,Handle*) noexcept;
 Result(*unregisterAction)(Handle) noexcept;
};
using GetApi=const ApiV1*(*)(std::uint32_t) noexcept;
static_assert(sizeof(ActionV1)==184 && sizeof(ApiV1)==32);
static_assert(std::is_standard_layout_v<ActionV1> && std::is_trivially_copyable_v<ActionV1>);
}
