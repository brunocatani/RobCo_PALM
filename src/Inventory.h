#pragma once
#include "WheelModel.h"
#include <RE/Bethesda/BGSInventoryItem.h>
namespace wheel {
namespace detail {
// Called only with the current inventory read lock held; returns value-only records.
Item equipmentItem(const RE::BGSInventoryItem& entry, const RE::BGSInventoryItem::Stack& stack, std::uint32_t index);
}
// Game-thread only. Records contain values and FormIDs, never engine pointers.
Model readInventory();
}
