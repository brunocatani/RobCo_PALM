#pragma once
#include "../SDK/include/PALMIcons.h"

namespace wheel {
using Icon=palm::api::Icon;
inline constexpr unsigned kIconSheetCount=7;
static_assert(static_cast<unsigned>(Icon::Count)==kIconSheetCount*16);
}
