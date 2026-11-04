#pragma once
#include "Abi.h"

// Actions on the shared Developer wheel. Registration survives game loads.
// Borrowed callback/context must remain alive until unregister succeeds;
// CallbackBusy requires a nonblocking retry. CapacityReached preserves every
// existing action; obtain user approval before unregistering an existing action
// to make room. No automatic eviction.
#pragma pack(push, 8)
namespace palm::api::developer
{
    inline constexpr InterfaceId kInterfaceId = InterfaceId::Developer;
    inline constexpr std::uint32_t kMajor = 2;
    inline constexpr std::uint32_t kMinor = 0;
    inline constexpr std::uint32_t kCapacity = 8;

    using Handle = std::uint32_t;

    // Game-task callback: enqueue work to the owning mod's appropriate thread.
    // No exceptions or blocking capture/file operations across this boundary.
    using Callback = void(PALM_CALL*)(void* context) noexcept;

    struct Action
    {
        std::uint32_t size{ sizeof(Action) };
        std::uint32_t version{ kRecordVersion };
        char id[64]{}; // Stable mod-qualified ID. Registration never replaces another ID.
        char label[96]{}; // Include the mod name; all mods share one eight-action wheel.
        Callback invoke{};
        void* context{};
    };

    struct Api
    {
        static constexpr InterfaceId interfaceId = kInterfaceId;
        static constexpr std::uint32_t majorVersion = kMajor;
        static constexpr std::uint32_t minorVersion = kMinor;

        bool(PALM_CALL* enabled)() noexcept {};
        Result(PALM_CALL* registerAction)(const Action* action, Handle* outHandle) noexcept {};
        Result(PALM_CALL* unregisterAction)(Handle handle) noexcept {};
    };

    static_assert(std::is_standard_layout_v<Action> && std::is_trivially_copyable_v<Action> && sizeof(Action) == 184);
    static_assert(std::is_standard_layout_v<Api> && sizeof(Api) == 24);
}
#pragma pack(pop)
