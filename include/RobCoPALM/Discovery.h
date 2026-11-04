#pragma once
#include "Abi.h"

// Resolve kQueryExportName from the loaded kModuleName and ask for an interface
// by ID, exact major, minimum minor and minimum table size. A major this PALM
// does not serve returns UnsupportedMajor and no table; PALM logs the refusal
// and names the calling DLL in an error box at the main menu.
// The pre-2.0 exports (GetPALMMenuApi and GetPALMDeveloperApi) only refuse
// outdated consumers; their names are never reused.
#pragma pack(push, 8)
namespace palm::api
{
    struct InterfaceDescriptorV1
    {
        std::uint32_t size{ sizeof(InterfaceDescriptorV1) };
        InterfaceId interfaceId{};
        std::uint32_t major{};
        std::uint32_t minor{};
        std::uint32_t tableByteSize{};
        std::uint32_t reserved{};
        const void* table{};
    };

    // Any thread. The descriptor and table stay valid for the process lifetime.
    using QueryInterfaceV1 = Result(PALM_CALL*)(InterfaceId interfaceId,
        std::uint32_t exactMajor, std::uint32_t minimumMinor,
        std::uint32_t minimumTableBytes,
        const InterfaceDescriptorV1** outDescriptor) noexcept;
    inline constexpr char kQueryExportName[] = "PALMAPI_QueryInterfaceV1";
    inline constexpr wchar_t kModuleName[] = L"wheelmenu.dll";

    static_assert(sizeof(InterfaceDescriptorV1) == 32);
}
#pragma pack(pop)
