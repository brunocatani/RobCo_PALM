#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#define PALM_CALL __cdecl

// RobCo PALM's shared ABI vocabulary. Each interface header carries its own
// kMajor and kMinor; records and tables belong to their interface headers.
#pragma pack(push, 8)
namespace palm::api
{
    // Layout version of every size/version-tagged record. Interface versions
    // are separate and negotiated through discovery.
    inline constexpr std::uint32_t kRecordVersion = 1;

    enum class Result : std::uint32_t
    {
        Ok = 0,
        InvalidArgument = 1,
        NotFound = 2,
        DuplicateId = 3,
        CapacityReached = 4,
        CallbackBusy = 5,
        ItemUnavailable = 6,
        InternalError = 7,
        Disabled = 8,
        // Discovery.
        UnknownInterface = 100,
        UnsupportedMajor = 101,
        UnsupportedMinor = 102,
        TableTooSmall = 103,
    };

    // Interface IDs are permanent and never reused.
    enum class InterfaceId : std::uint32_t
    {
        Menu = 1,
        Developer = 2,
    };

    static_assert(sizeof(void*) == 8, "RobCo PALM requires a 64-bit consumer");
    static_assert(sizeof(Result) == 4 && sizeof(InterfaceId) == 4);
}
#pragma pack(pop)
