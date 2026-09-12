#include "texnx/localization/Localization.hpp"

#include "texnx/Paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

#include <switch.h>

namespace texnx::localization {
namespace {

constexpr std::size_t EnglishIndex = 0;
constexpr std::size_t JapaneseIndex = 1;

const std::unordered_map<std::string_view, std::string_view> EnglishFallback{
    {"home.tagline", "A focused texture manager for Minecraft: Nintendo Switch Edition"},
    {"home.textures", "Textures"},
    {"home.settings", "Settings"},
    {"home.about", "About"},
    {"textures.title", "Textures"},
    {"textures.placeholder_title", "Texture library"},
    {"textures.placeholder_body", "This screen is ready for a future texture list. No files are scanned or changed."},
    {"settings.title", "Settings"},
    {"settings.description", "Choose the language used by TexNX."},
    {"settings.language", "Language"},
    {"settings.system", "System"},
    {"settings.english", "English"},
    {"settings.japanese", "日本語"},
    {"settings.save_failed", "The language changed for this session, but config.json could not be saved."},
    {"about.title", "About"},
    {"about.version", "Version"},
    {"about.developer", "Developer"},
    {"about.target", "Target"},
    {"about.title_id", "Title ID"},
    {"about.unofficial", "Unofficial community project. Not affiliated with Nintendo, Mojang Studios, or Microsoft."},
    {"common.back", "Back"},
    {"common.ok", "OK"},
    {"common.not_found_title", "Common directory not found"},
    {"common.not_found_body", "TexNX will remain available, but the expected Minecraft LayeredFS Common directory is missing."},
    {"common.error_title", "Common directory check failed"},
    {"common.error_body", "TexNX could not read the expected Minecraft LayeredFS Common directory."},
};

EffectiveLanguage resolveLanguage(const config::LanguageMode mode,
                                  std::string_view systemLocale) noexcept {
    if (mode == config::LanguageMode::English) {
        return EffectiveLanguage::English;
    }
    if (mode == config::LanguageMode::Japanese) {
        return EffectiveLanguage::Japanese;
    }

    std::string normalized(systemLocale);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (normalized.rfind("ja", 0) == 0) {
        return EffectiveLanguage::Japanese;
    }

    // 英語以外の未対応 locale も安全な English fallback に統一する。
    return EffectiveLanguage::English;
}

} // namespace

bool Localization::loadResourceFile(const char* path,
                                    nlohmann::json& destination) noexcept {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    auto parsed = nlohmann::json::parse(input, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return false;
    }

    destination = std::move(parsed);
    return true;
}

bool Localization::loadResources() noexcept {
    const bool englishLoaded = loadResourceFile(paths::EnglishTranslations,
                                                resources_[EnglishIndex]);
    const bool japaneseLoaded = loadResourceFile(paths::JapaneseTranslations,
                                                 resources_[JapaneseIndex]);
    return englishLoaded && japaneseLoaded;
}

void Localization::select(const config::LanguageMode mode,
                          const std::string_view systemLocale) noexcept {
    effectiveLanguage_ = resolveLanguage(mode, systemLocale);
}

const nlohmann::json& Localization::selectedResource() const noexcept {
    return resources_[effectiveLanguage_ == EffectiveLanguage::Japanese
                          ? JapaneseIndex
                          : EnglishIndex];
}

std::string Localization::text(const std::string_view key) const {
    const auto lookup = [key](const nlohmann::json& resource) -> std::string {
        if (!resource.is_object()) {
            return {};
        }
        const auto entry = resource.find(std::string(key));
        if (entry == resource.end() || !entry->is_string()) {
            return {};
        }
        return entry->get<std::string>();
    };

    if (auto translated = lookup(selectedResource()); !translated.empty()) {
        return translated;
    }
    if (auto english = lookup(resources_[EnglishIndex]); !english.empty()) {
        return english;
    }
    if (const auto fallback = EnglishFallback.find(key);
        fallback != EnglishFallback.end()) {
        return std::string(fallback->second);
    }

    return std::string(key);
}

EffectiveLanguage Localization::effectiveLanguage() const noexcept {
    return effectiveLanguage_;
}

const char* Localization::effectiveLocaleTag() const noexcept {
    return effectiveLanguage_ == EffectiveLanguage::Japanese ? "ja-JP" : "en-US";
}

std::string Localization::detectSystemLocale() noexcept {
    std::uint64_t languageCode = 0;
    if (R_FAILED(setGetSystemLanguage(&languageCode))) {
        return {};
    }

    char locale[sizeof(languageCode) + 1]{};
    std::memcpy(locale, &languageCode, sizeof(languageCode));
    return std::string(locale);
}

} // namespace texnx::localization
