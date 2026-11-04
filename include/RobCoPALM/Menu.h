#pragma once
#include "Abi.h"
#include "Icons.h"

// A mod's own section on the wheel: a named group of up to kMaxItems items.
#pragma pack(push, 8)
namespace palm::api::menu
{
    inline constexpr InterfaceId kInterfaceId = InterfaceId::Menu;
    inline constexpr std::uint32_t kMajor = 2;
    inline constexpr std::uint32_t kMinor = 0;
    inline constexpr std::uint32_t kMaxSections = 10;
    inline constexpr std::uint32_t kMaxItems = 8;
    inline constexpr std::uint32_t kMaxVisibleEntries = 10; // Includes PALM's Config entry.

    using SectionHandle = std::uint32_t;
    enum class ItemFlag : std::uint32_t { Disabled = 1, Equipped = 2, ShowQuantity = 4 };

    // Called on the F4SE game task thread without PALM locks. In press/click mode
    // the wheel remains open; consumers must not depend on it closing before dispatch.
    // The mod owns the action: vanilla equipment, ROCK requests, or another feature.
    // Never throw across this callback. Keep the module and context alive until
    // unregisterSection succeeds. CallbackBusy means an invocation is still running;
    // retry unregister later, without blocking a game/input/render callback.
    using SelectCallback = void(PALM_CALL*)(std::uint32_t itemId, std::uint64_t userData, void* context) noexcept;

    struct Section
    {
        std::uint32_t size{ sizeof(Section) };
        std::uint32_t version{ kRecordVersion };
        Icon icon{ Icon::Config };
        char id[64]{}; // Stable, globally unique ASCII ID: letters, digits, '_', '-', '.'.
        char name[48]{};
        char modName[48]{};
        SelectCallback onSelect{};
        void* context{};
    };

    struct Item
    {
        std::uint32_t size{ sizeof(Item) };
        std::uint32_t version{ kRecordVersion };
        std::uint32_t id{}; // Nonzero, stable within this section. Not necessarily a FormID.
        Icon icon{ Icon::Config };
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
    struct Api
    {
        static constexpr InterfaceId interfaceId = kInterfaceId;
        static constexpr std::uint32_t majorVersion = kMajor;
        static constexpr std::uint32_t minorVersion = kMinor;

        Result(PALM_CALL* registerSection)(const Section* section, SectionHandle* outHandle) noexcept {};
        Result(PALM_CALL* unregisterSection)(SectionHandle handle) noexcept {};
        Result(PALM_CALL* setItems)(SectionHandle handle, const Item* items, std::uint32_t count) noexcept {};
    };

    static_assert(std::is_standard_layout_v<Section> && sizeof(Section) == 192);
    static_assert(std::is_standard_layout_v<Item> && sizeof(Item) == 128);
    static_assert(std::is_standard_layout_v<Api> && sizeof(Api) == 24);
}
#pragma pack(pop)
