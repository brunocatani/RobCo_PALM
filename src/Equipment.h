#pragma once
#include "WheelModel.h"
#include "EquipmentRuntimePolicy.h"
namespace wheel {
// Initialized once before input subscription. Each rejected native operation is
// isolated; menu startup does not depend on equipment capabilities.
void initializeEquipmentRuntime() noexcept;
EquipmentSupport equipmentSupport() noexcept;
// Game-thread only; re-resolves the selected variant and reads its current equip state.
const char* toggleEquipment(const Item& selected, std::uint64_t owner, std::uint32_t inputThread) noexcept;
const char* useInventoryItem(std::uint32_t id) noexcept;
}
