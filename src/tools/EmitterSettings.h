#pragma once
#include <array>
#include <string_view>
#include "RockSettingControlPolicy.h"

namespace rock_configurator::emitter_settings
{
    enum class Type { Toggle, Number, Color };
    struct Setting
    {
        const char* key;
        const char* label;
        Type type;
        double minimum{}, maximum{}, step{};
    };
    // Bounds match Emitters' Main-section parser; values always come from the
    // user's INI. Opening this page never replaces tuned appearance settings.
    inline constexpr std::array settings{
        Setting{"bEnabled","Enable Emitters",Type::Toggle},
        Setting{"bLaserEnabled","Enable laser",Type::Toggle},
        Setting{"bWeaponEmitterControl","Attachment-controlled laser / flashlight",Type::Toggle},
        Setting{"bFlashlightEnabled","Enable weapon flashlight",Type::Toggle},
        Setting{"fFlashlightIntensity","Flashlight intensity",Type::Number,0.1,20,0.05},
        Setting{"bCorrectedReticles","Correct sight aiming and parallax",Type::Toggle},
        Setting{"bReticleVisibilityAssist","Improve reticle visibility",Type::Toggle},
        Setting{"fReticleBrightness","Reticle brightness",Type::Number,0.25,16,0.05},
        Setting{"bDotEnabled","Show laser impact dot",Type::Toggle},
        Setting{"bHitLightEnabled","Illuminate laser impact surroundings",Type::Toggle},
        Setting{"bDebugVisualizer","Show laser debug ray",Type::Toggle},
        Setting{"bStereoDiagnostics","Log stereo sight diagnostics",Type::Toggle},
        Setting{"fDotRadius","Laser dot radius",Type::Number,0.05,4,0.05},
        Setting{"fDotEmissionScale","Laser dot emission at distance",Type::Number,1,128,0.25},
        Setting{"fDotNearEmissionScale","Laser dot emission nearby",Type::Number,1,128,0.25},
        Setting{"fDotSurfaceOffset","Laser dot surface offset",Type::Number,0.001,0.5,0.001},
        Setting{"fHitLightIntensity","Laser spill intensity",Type::Number,0.05,10,0.05},
        Setting{"fHitLightRadius","Laser spill radius",Type::Number,1,512,1},
        Setting{"sLaserColor","Laser color",Type::Color},
        Setting{"fHitLightSurfaceOffset","Laser spill surface offset",Type::Number,0.05,4,0.05}
    };
    inline const Setting* find(std::string_view key) noexcept
    {
        for (const auto& entry : settings)
            if (setting_control::equalsIgnoreCase(entry.key,key)) return &entry;
        return nullptr;
    }
}
