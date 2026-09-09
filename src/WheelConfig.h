#pragma once
#include "WheelPreferences.h"
#include <filesystem>
namespace wheel {
// Persistence and inventory publication are game-thread operations. UI edits
// only mutate the model and request a game-thread save/refresh.
void loadWheelConfig(const std::filesystem::path& path);
void flushWheelConfig();
void publishWheelInventory(const Model& inventory);
Model selectedWheelInventory(const Model& inventory);
bool takeWheelConfigChange();
void drawWheelConfig();
}
