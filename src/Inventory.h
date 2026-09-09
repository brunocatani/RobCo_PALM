#pragma once
#include "WheelModel.h"
namespace wheel {
// Game-thread only. Records contain values and FormIDs, never engine pointers.
Model readInventory();
bool useInventoryItem(std::uint32_t id, std::string& message);
}
