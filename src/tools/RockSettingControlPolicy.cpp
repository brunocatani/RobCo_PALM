#include "RockSettingControlPolicy.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <optional>

namespace rock_configurator::setting_control
{
    namespace
    {
        struct NumericRange
        {
            std::string_view key;
            double minimum;
            double maximum;
        };

        constexpr auto kNumericRanges = std::to_array<NumericRange>({
            { "fFiringGripProximitySupportRadius", 0.25, 30.0 },
            { "fForceGrabAttachSettleSeconds", 0.0, 1.0 },
            { "fGrabEffectiveMotorMassFloor", 0.0, 100.0 },
            { "fGrabHandLerpMinDistance", 0.0, 80.0 },
            { "fGrabHandLerpTimeMin", 0.0, 1.0 },
            { "fGrabHandReturnMinAngleDegrees", 0.0, 180.0 },
            { "fGrabHandReturnMinDistance", 0.0, 80.0 },
            { "fGrabHandReturnTimeMin", 0.0, 1.0 },
            { "fGrabHapticBaseIntensity", 0.0, 1.0 },
            { "fGrabHapticDurationSeconds", 0.0, 0.2 },
            { "fGrabHapticMassExponent", 0.0, 2.0 },
            { "fGrabHapticMassScale", 0.0, 1.0 },
            { "fGrabHeldMassMovementFadeOutSeconds", 0.0, 60.0 },
            { "fGrabHeldMassMovementMassExponent", 0.0, 4.0 },
            { "fGrabHeldMassMovementMassProportion", 0.0, 10.0 },
            { "fGrabHeldMassMovementMaxReduction", 0.0, 99.0 },
            { "fGrabLooseWeaponSharedConstraintAngularDampingMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintAngularForceMultiplier", 0.05, 8.0 },
            { "fGrabLooseWeaponSharedConstraintAngularRecoveryMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintAngularTauMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintCollisionTauMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintLinearDampingMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintLinearRecoveryMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintLinearTauMultiplier", 0.05, 4.0 },
            { "fGrabLooseWeaponSharedConstraintMaxForceMultiplier", 0.05, 8.0 },
            { "fGrabLowContactSupportAngularScale", 0.05, 1.0 },
            { "fGrabMinAngularAuthorityScale", 0.05, 1.0 },
            { "fGrabMinInertia", 0.0001, 100.0 },
            { "fGrabPhysicsRateForceScaleExponent", 0.0, 2.0 },
            { "fGrabPhysicsRateMinForceScale", 0.1, 2.0 },
            { "fGrabPhysicsRateReferenceHz", 1.0, 240.0 },
            { "fGrabPinchDetectionAxisBlend", 0.0, 1.0 },
            { "fGrabPinchMaxFingerGapGameUnits", 0.1, 80.0 },
            { "fGrabPinchMaxPocketDistanceGameUnits", 0.1, 80.0 },
            { "fGrabPinchMaxVolumeCubicGameUnits", 0.001, 1000000.0 },
            { "fGrabPinchMinFingerGapGameUnits", 0.0, 40.0 },
            { "fGrabPinchOtherFingerCurlValue", 0.0, 1.0 },
            { "fGrabPinchThumbIndexMaxOpenValue", 0.0, 1.0 },
            { "fGrabPositionOnlyAngularScale", 0.05, 1.0 },
            { "fGrabSmallObjectAngularScale", 0.05, 1.0 },
            { "fGrabSmallObjectReferenceLeverGameUnits", 1.0, 120.0 },
            { "fGrabWeakPivotTwistScale", 0.0, 1.0 },
            { "fHeldImpactHapticBaseIntensity", 0.0, 1.0 },
            { "fHeldImpactHapticCooldownSeconds", 0.0, 1.0 },
            { "fHeldImpactHapticDampedMultiplier", 0.0, 1.0 },
            { "fHeldImpactHapticDurationSeconds", 0.0, 0.2 },
            { "fHeldImpactHapticMassExponent", 0.0, 2.0 },
            { "fHeldImpactHapticMassScale", 0.0, 1.0 },
            { "fHeldImpactHapticMinSpeedGameUnits", 0.0, 1000.0 },
            { "fHeldImpactHapticSpeedScale", 0.0, 1.0 },
            { "fManualScopeHoldSeconds", 0.05, 2.0 },
            { "fMouthConsumeCandidateHapticIntervalSeconds", 0.0, 2.0 },
            { "fMouthConsumeCommitHapticDurationSeconds", 0.0, 0.2 },
            { "fMouthConsumeCommitHapticIntensity", 0.0, 1.0 },
            { "fMouthConsumeEnterPaddingGameUnits", 0.0, 40.0 },
            { "fMouthConsumeExitPaddingGameUnits", 0.0, 60.0 },
            { "fMouthConsumeMaxSpeedGameUnitsPerSecond", 0.0, 1000.0 },
            { "fMouthConsumeMinDwellSeconds", 0.0, 1.0 },
            { "fMouthConsumeRadiusGameUnits", 1.0, 80.0 },
            { "fNativeScopeFiringGripFallbackOffsetXGameUnits", -100.0, 100.0 },
            { "fNativeScopeFiringGripFallbackOffsetYGameUnits", -100.0, 100.0 },
            { "fNativeScopeFiringGripFallbackOffsetZGameUnits", -100.0, 100.0 },
            { "fNativeScopeFiringGripFallbackPitchDegrees", -180.0, 180.0 },
            { "fNativeScopeFiringGripFallbackRollDegrees", -180.0, 180.0 },
            { "fNativeScopeFiringGripFallbackYawDegrees", -180.0, 180.0 },
            { "fNativeScopeOverlayOffsetXGameUnits", -100.0, 100.0 },
            { "fNativeScopeOverlayOffsetYGameUnits", -100.0, 100.0 },
            { "fNativeScopeOverlayOffsetZGameUnits", -100.0, 100.0 },
            { "fNativeScopeOverlayPitchDegrees", -180.0, 180.0 },
            { "fNativeScopeOverlayRollDegrees", -180.0, 180.0 },
            { "fNativeScopeOverlayYawDegrees", -180.0, 180.0 },
            { "fPullCatchHapticIntensity", 0.0, 1.0 },
            { "fPullStartHapticIntensity", 0.0, 1.0 },
            { "fRealisticGrenadeFuseSeconds", 0.0, 30.0 },
            { "fSelectionLockHapticIntensity", 0.0, 1.0 },
            { "fSelectionLockReleaseHapticDurationSeconds", 0.0, 0.2 },
            { "fSelectionLockReleaseHapticIntensity", 0.0, 1.0 },
            { "fShoulderStashCandidateHapticIntervalSeconds", 0.0, 2.0 },
            { "fShoulderStashCommitHapticIntensity", 0.0, 1.0 },
            { "fShoulderStashEnterPaddingGameUnits", 0.0, 40.0 },
            { "fShoulderStashExitPaddingGameUnits", 0.0, 60.0 },
            { "fShoulderStashHmdBackRadiusGameUnits", 1.0, 80.0 },
            { "fShoulderStashMaxSpeedGameUnitsPerSecond", 0.0, 1000.0 },
            { "fShoulderStashMinDwellSeconds", 0.0, 1.0 },
            { "fSurfaceGrabHapticDurationSeconds", 0.0, 0.2 },
            { "fSurfaceGrabHapticIntensity", 0.0, 1.0 },
            { "fWeaponInteractionTouchRadius", 0.25, 6.0 },
            { "fWeaponSupportGripHandLerpMinDistance", 0.0, 80.0 },
            { "fWeaponSupportGripHandLerpTimeMin", 0.0, 1.0 },
        });

        constexpr auto kIntegerRanges = std::to_array<NumericRange>({
            { "iHavokTimingFixMaxSubsteps", 1.0, 6.0 },
            { "iSurfaceMeshGrabMaxTriangles", 256.0, 100000.0 },
            { "iSurfaceMeshGrabMaxPatchTriangles", 64.0, 2048.0 },
            { "iGrabMinFingerContactGroups", 1.0, 5.0 },
            { "iGrabContactPatchProbeCount", 1.0, 9.0 },
            { "iDebugMaxHandBoneBodiesDrawn", 0.0, 48.0 },
            { "iDebugMaxBodyBoneBodiesDrawn", 0.0, 64.0 },
            { "iDebugMaxShapeCapturesPerFrame", 0.0, 32.0 },
            { "iDebugMaxConvexSupportVertices", 1.0, 32.0 },
            { "iDebugMaxCompoundChildren", 1.0, 1024.0 },
            { "iDebugMaxCompoundDepth", 1.0, 8.0 },
            { "iDebugMaxShapeQueuedJobs", 0.0, 256.0 },
            { "iDebugMaxShapeCompletedJobs", 0.0, 256.0 },
            { "iDebugMaxShapeUploadsPerFrame", 0.0, 16.0 },
            { "iDebugMaxShapeCacheEntries", 16.0, 4096.0 },
            { "iDebugMaxShapeCacheBytes", 1048576.0, 536870912.0 },
            { "iDebugMaxBodyInstances", 0.0, 171.0 },
            { "iDebugMaxLineVertices", 0.0, 65536.0 },
            { "iDebugMaxTextVertices", 0.0, 131072.0 },
            { "iDebugMaxSkeletonBonesDrawn", 0.0, 768.0 },
            { "iDebugMaxSkeletonBoneAxesDrawn", 0.0, 768.0 },
            { "iLogSampleMilliseconds", 0.0, 60000.0 },
            { "iPerformanceProfilerLogIntervalFrames", 30.0, 54000.0 },
            { "iPerformanceProfilerWarmupFrames", 0.0, 54000.0 },
            { "iRightWeaponReadyButtonID", 0.0, 64.0 },
            { "iWeaponCollisionGroupingMode", 0.0, 3.0 },
            { "iWeaponCollisionVisualStabilizationFrames", 0.0, 60.0 },
            { "iWeaponCollisionSupportFitTargetPoints", 4.0, 252.0 },
            { "iSoftContactVisualPriority", 0.0, 255.0 },
        });

        [[nodiscard]] bool contains(std::string_view text, std::string_view token) noexcept
        {
            return text.find(token) != std::string_view::npos;
        }

        [[nodiscard]] double parseNumber(std::string_view value) noexcept
        {
            std::string owned(value);
            char* end = nullptr;
            const double parsed = std::strtod(owned.c_str(), &end);
            return end && *end == '\0' && std::isfinite(parsed) ? parsed : 0.0;
        }

        [[nodiscard]] double fineStep(
            ValueType type,
            std::string_view key,
            double current,
            double span) noexcept
        {
            if (type == ValueType::Integer) {
                if (contains(key, "CacheBytes")) {
                    return 1048576.0;
                }
                if (contains(key, "Triangles") && span > 10000.0) {
                    return 64.0;
                }
                return 1.0;
            }
            if (contains(key, "Seconds") || contains(key, "Intensity") ||
                contains(key, "Alpha") || contains(key, "Blend") ||
                contains(key, "Weight") || contains(key, "Strength") ||
                contains(key, "Multiplier") || contains(key, "Scale") ||
                contains(key, "Exponent") || std::fabs(current) < 0.1) {
                return std::fabs(current) < 0.01 ? 0.001 : 0.01;
            }
            if (contains(key, "Degrees")) {
                return 0.5;
            }
            if (contains(key, "GameUnits")) {
                return span <= 20.0 ? 0.1 : 0.25;
            }
            if (span <= 5.0 || std::fabs(current) < 10.0) {
                return 0.05;
            }
            if (span <= 100.0 || std::fabs(current) < 100.0) {
                return 0.5;
            }
            if (span <= 1000.0 || std::fabs(current) < 1000.0) {
                return 1.0;
            }
            return 10.0;
        }

        [[nodiscard]] Spec dropdown(std::initializer_list<Option> options)
        {
            Spec spec{};
            spec.kind = Kind::Dropdown;
            spec.options.assign(options.begin(), options.end());
            return spec;
        }

        [[nodiscard]] std::vector<Option> describedIntegerOptions(
            std::string_view description,
            std::string_view currentValue)
        {
            std::vector<Option> options;
            std::size_t cursor = 0;
            while (cursor < description.size()) {
                const bool negative = description[cursor] == '-' &&
                    cursor + 1 < description.size() &&
                    std::isdigit(static_cast<unsigned char>(description[cursor + 1]));
                const bool digit = std::isdigit(static_cast<unsigned char>(description[cursor]));
                const bool boundary = cursor == 0 ||
                    std::isspace(static_cast<unsigned char>(description[cursor - 1])) ||
                    description[cursor - 1] == ',' || description[cursor - 1] == ';' ||
                    description[cursor - 1] == ':' || description[cursor - 1] == '.';
                if ((!digit && !negative) || !boundary) {
                    ++cursor;
                    continue;
                }

                const std::size_t valueBegin = cursor;
                cursor += negative ? 1 : 0;
                while (cursor < description.size() &&
                       std::isdigit(static_cast<unsigned char>(description[cursor]))) {
                    ++cursor;
                }
                const std::size_t valueEnd = cursor;
                while (cursor < description.size() &&
                       std::isspace(static_cast<unsigned char>(description[cursor]))) {
                    ++cursor;
                }
                if (cursor >= description.size() || description[cursor] != '=') {
                    continue;
                }
                ++cursor;
                while (cursor < description.size() &&
                       std::isspace(static_cast<unsigned char>(description[cursor]))) {
                    ++cursor;
                }
                const std::size_t labelBegin = cursor;
                while (cursor < description.size() && description[cursor] != ',' &&
                       description[cursor] != ';' && description[cursor] != '.') {
                    ++cursor;
                }
                std::string value(description.substr(valueBegin, valueEnd - valueBegin));
                std::string label(description.substr(labelBegin, cursor - labelBegin));
                while (!label.empty() &&
                       std::isspace(static_cast<unsigned char>(label.back()))) {
                    label.pop_back();
                }
                if (!label.empty() && std::none_of(
                        options.begin(), options.end(), [&](const Option& option) {
                            return option.value == value;
                        })) {
                    options.push_back({ value, value + " — " + label });
                }
            }
            if (options.size() < 2 || std::none_of(
                    options.begin(), options.end(), [&](const Option& option) {
                        return option.value == currentValue;
                    })) {
                options.clear();
            }
            return options;
        }

        [[nodiscard]] std::optional<Spec> exactDropdown(
            std::string_view key,
            std::string_view currentValue,
            std::string_view description)
        {
            if (key == "iLogLevel") {
                return dropdown({
                    { "0", "0 — Trace" },
                    { "1", "1 — Debug" },
                    { "2", "2 — Info" },
                    { "3", "3 — Warn" },
                    { "4", "4 — Error" },
                    { "5", "5 — Critical" },
                    { "6", "6 — Off" },
                });
            }
            if (key == "iWeaponGrabMode") {
                return dropdown({
                    { "1", "1 — Toggle both grips" },
                    { "2", "2 — Toggle firing grip only" },
                    { "3", "3 — Hold both grips (full immersive)" },
                });
            }
            if (key == "iHighlightIntensityMode") {
                return dropdown({
                    { "1", "1 — 40%" },
                    { "2", "2 — 60%" },
                    { "3", "3 — 80%" },
                    { "4", "4 — 100%" },
                });
            }
            if (key == "sHighlightColor") {
                return dropdown({
                    { "red", "Red" },
                    { "blue", "Blue" },
                    { "orange", "Orange" },
                    { "white", "White" },
                });
            }
            if (key == "iDebugSkeletonBoneMode") {
                return dropdown({
                    { "0", "Mode 0" },
                    { "1", "Mode 1" },
                    { "2", "Mode 2" },
                    { "3", "Mode 3" },
                });
            }
            if (key == "iDebugSkeletonBoneSource") {
                return dropdown({
                    { "1", "Source 1" },
                    { "2", "Source 2" },
                });
            }
            auto described = describedIntegerOptions(description, currentValue);
            if (!described.empty()) {
                Spec spec{};
                spec.kind = Kind::Dropdown;
                spec.options = std::move(described);
                return spec;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<NumericRange> exactRange(
            ValueType type,
            std::string_view key) noexcept
        {
            if (type == ValueType::Integer) {
                const auto found = std::find_if(
                    kIntegerRanges.begin(), kIntegerRanges.end(), [&](const auto& range) {
                        return range.key == key;
                    });
                return found == kIntegerRanges.end() ?
                    std::nullopt : std::optional<NumericRange>(*found);
            }
            const auto found = std::find_if(
                kNumericRanges.begin(), kNumericRanges.end(), [&](const auto& range) {
                    return range.key == key;
                });
            return found == kNumericRanges.end() ?
                std::nullopt : std::optional<NumericRange>(*found);
        }
    }

    bool equalsIgnoreCase(std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index) {
            char a = left[index];
            char b = right[index];
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = static_cast<char>(b - 'A' + 'a');
            }
            if (a != b) {
                return false;
            }
        }
        return true;
    }

    Spec build(
        ValueType type,
        std::string_view key,
        std::string_view value,
        std::string_view description,
        RpsMod mod)
    {
        mod = configurationFamily(mod);
        if (mod != RpsMod::Rock && mod != RpsMod::RockDeveloper) {
            if (type == ValueType::Boolean) return { .kind = Kind::Checkbox };
            if (mod == RpsMod::Paper && key == "sMaximumMode")
                return { .kind = Kind::Dropdown, .options = {{"User","User"},{"Observe","Observe"},{"Harvest","Harvest"},{"Capture","Capture"}} };
            if (mod == RpsMod::Paper && key == "sAccess")
                return { .kind = Kind::Dropdown, .options = {{"Off","Off"},{"ReadOnly","Read only"},{"ReadWrite","Read and write"}} };
            if (type == ValueType::String) return { .kind = Kind::Text };
            // ROCK's keyed ranges and naming heuristics are not other mods' contracts.
            return { .kind = Kind::Numeric, .step = fineStep(type, key, parseNumber(value), 0.0) };
        }
        if (type == ValueType::Boolean) {
            return { .kind = Kind::Checkbox };
        }
        if (type == ValueType::String) {
            if (const auto exact = exactDropdown(key, value, description)) {
                return *exact;
            }
            return { .kind = Kind::Text };
        }
        if (type == ValueType::Integer) {
            if (const auto exact = exactDropdown(key, value, description)) {
                return *exact;
            }
        }

        const double current = parseNumber(value);
        if (const auto range = exactRange(type, key)) {
            const double span = range->maximum - range->minimum;
            return {
                .kind = Kind::Numeric,
                .bounded = true,
                .minimum = range->minimum,
                .maximum = range->maximum,
                .step = fineStep(type, key, current, span),
            };
        }

        if (type == ValueType::Float &&
            (contains(key, "Intensity") || contains(key, "Alpha") ||
             contains(key, "Blend") || contains(key, "Weight") ||
             contains(key, "Strength")) &&
            current >= 0.0 && current <= 1.0) {
            return {
                .kind = Kind::Numeric,
                .bounded = true,
                .minimum = 0.0,
                .maximum = 1.0,
                .step = 0.01,
            };
        }
        if (type == ValueType::Float && contains(key, "MaxOpenValue")) {
            return {
                .kind = Kind::Numeric,
                .bounded = true,
                .minimum = 1.0,
                .maximum = 2.0,
                .step = 0.01,
            };
        }
        if (type == ValueType::Float && contains(key, "Degrees")) {
            const bool signedAngle = current < 0.0 || contains(key, "Yaw") ||
                contains(key, "Pitch") || contains(key, "Roll") || contains(key, "Offset");
            return {
                .kind = Kind::Numeric,
                .bounded = true,
                .minimum = signedAngle ? -180.0 : 0.0,
                .maximum = 180.0,
                .step = 0.5,
            };
        }

        return {
            .kind = Kind::Numeric,
            .bounded = false,
            .step = fineStep(type, key, current, 0.0),
        };
    }

    double snapNumeric(const Spec& spec, double value) noexcept
    {
        if (!std::isfinite(value)) {
            return spec.bounded ? spec.minimum : 0.0;
        }
        const double step = std::isfinite(spec.step) && spec.step > 0.0 ? spec.step : 1.0;
        value = std::round(value / step) * step;
        if (spec.bounded && std::isfinite(spec.minimum) && std::isfinite(spec.maximum) &&
            spec.minimum <= spec.maximum) {
            value = std::clamp(value, spec.minimum, spec.maximum);
        }
        return value;
    }
}
