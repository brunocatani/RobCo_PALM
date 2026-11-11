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
// Game thread. Saves the base item of a loose reference a hand holds into a
// free slot of its aid or food page. outBase is that item's FormID when saved.
const char* addHeldItemToWheel(std::uint32_t referenceId,Category category,unsigned slot,std::uint32_t& outBase) noexcept;
// Game thread, after ROCK released the reference: the player picks it up,
// provided it is still that loose item.
const char* storeReleasedItem(std::uint32_t referenceId,std::uint32_t baseId) noexcept;
}
