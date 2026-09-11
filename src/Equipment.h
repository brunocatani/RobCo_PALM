#pragma once
#include "WheelModel.h"
namespace wheel {
// The plugin validates native entrypoints before enabling inventory access.
bool validateEquipmentRuntime() noexcept;
// Game-thread only; re-resolves the selected variant and reads its current equip state.
const char* toggleEquipment(const Item& selected, std::uint64_t owner) noexcept;
const char* useInventoryItem(std::uint32_t id) noexcept;
}
