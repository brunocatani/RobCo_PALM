#pragma once
#include "WheelPreferences.h"
namespace wheel {
// Serialization snapshots UI edits under the same mutex. Restoring a game
// clears its predecessor's inventory and pending refresh, including no-data saves.
Preferences snapshotWheelPreferences();
void restoreWheelPreferences(Preferences prefs);
void publishWheelInventory(const Model& inventory);
Model selectedWheelInventory(const Model& inventory);
bool takeWheelConfigChange();
void refreshWheelSections(Model& model);
void drawWheelConfig();
void drawWheelSettings();
}
