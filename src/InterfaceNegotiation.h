#pragma once
#include <RobCoPALM/Discovery.h>
#include <span>

namespace wheel::discovery
{
    template <class Table>
    constexpr palm::api::InterfaceDescriptorV1 describe(const Table& table) noexcept
    {
        return { sizeof(palm::api::InterfaceDescriptorV1), Table::interfaceId, Table::majorVersion, Table::minorVersion,
            sizeof(Table), 0, &table };
    }

    // Exact major, minimum minor and minimum table size. A known interface
    // asked for a major PALM does not serve is refused with UnsupportedMajor
    // and no table; the caller logs that refusal.
    inline palm::api::Result query(std::span<const palm::api::InterfaceDescriptorV1> entries,
        palm::api::InterfaceId id, std::uint32_t major, std::uint32_t minor,
        std::uint32_t bytes, const palm::api::InterfaceDescriptorV1** output) noexcept
    {
        using palm::api::Result;
        if (!output) return Result::InvalidArgument;
        *output = nullptr;
        if (!major || !bytes) return Result::InvalidArgument;
        bool knownId = false;
        for (const auto& descriptor : entries) {
            if (descriptor.interfaceId != id) continue;
            knownId = true;
            if (descriptor.major != major) continue;
            if (descriptor.minor < minor) return Result::UnsupportedMinor;
            if (descriptor.tableByteSize < bytes) return Result::TableTooSmall;
            if (!descriptor.table || descriptor.size != sizeof(palm::api::InterfaceDescriptorV1)) return Result::InternalError;
            *output = &descriptor;
            return Result::Ok;
        }
        return knownId ? Result::UnsupportedMajor : Result::UnknownInterface;
    }
}
