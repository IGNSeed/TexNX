#pragma once

#include "texnx/config/Config.hpp"

#include <array>
#include <string>
#include <string_view>

#include <borealis/extern/nlohmann/json.hpp>

namespace texnx::localization {

enum class EffectiveLanguage {
    English,
    Japanese,
};

class Localization final {
public:
    [[nodiscard]] bool loadResources() noexcept;
    void select(config::LanguageMode mode, std::string_view systemLocale) noexcept;

    [[nodiscard]] std::string text(std::string_view key) const;
    [[nodiscard]] EffectiveLanguage effectiveLanguage() const noexcept;
    [[nodiscard]] const char* effectiveLocaleTag() const noexcept;

    [[nodiscard]] static std::string detectSystemLocale() noexcept;

private:
    [[nodiscard]] static bool loadResourceFile(
        const char* path, nlohmann::json& destination) noexcept;
    [[nodiscard]] const nlohmann::json& selectedResource() const noexcept;

    std::array<nlohmann::json, 2> resources_{};
    EffectiveLanguage effectiveLanguage_{EffectiveLanguage::English};
};

} // namespace texnx::localization
