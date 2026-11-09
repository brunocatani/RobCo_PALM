#include "PalmTheme.h"
#include <RPS_UI_Framework/ThemeDefaults.h>

namespace palm::theme {
namespace {
// Render thread only: the callbacks that bind it run there.
const rpsui::api::framework::Theme*& bound() noexcept {
    static const rpsui::api::framework::Theme* value{};
    return value;
}
}
void bind(const rpsui::api::framework::Theme* value) noexcept {bound()=value;}
const rpsui::api::framework::Theme& current() noexcept {
    static const auto builtIn=rpsui::theme_defaults::robco();
    return bound() ? *bound() : builtIn;
}
}
