#pragma once
#include "WheelPreferences.h"
namespace wheel {
// Serialization snapshots UI edits under the same mutex. Restoring a game
// clears its predecessor's inventory and pending refresh, including no-data saves.
Preferences snapshotWheelPreferences();
void restoreWheelPreferences(Preferences prefs);
// Returns the weapon choices removed because they left the inventory.
std::size_t publishWheelInventory(const Model& inventory);
// Adds a carried item at one free ring slot of the category; returns the status text.
const char* addWheelFavorite(Category category,const Item& item,unsigned slot);
Model selectedWheelInventory(const Model& inventory);
bool takeWheelConfigChange();
void refreshWheelSections(Model& model);
void drawWheelConfig();
void drawWheelSettings();
}
