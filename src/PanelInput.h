#pragma once

#include <imgui.h>

namespace wheel {
inline void configurePanelInput(ImGuiIO& io) noexcept {
    // RPS UI supplies one input snapshot per rendered frame, including at most
    // one button transition. ImGui's event trickling splits pointer motion and
    // wheel input across frames, building a backlog while a VR hand moves.
    // Consume this snapshot together; desktop event backends keep trickling.
    io.ConfigInputTrickleEventQueue = false;
}
}
