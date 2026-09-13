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
    {"navigation.textures", "Textures"},
    {"navigation.settings", "Settings"},
    {"navigation.about", "About"},
    {"textures.title", "Textures"},
    {"textures.current_section", "CURRENT TEXTURE"},
    {"textures.current_badge", "CURRENT"},
    {"textures.available_section", "Available Textures"},
    {"textures.current", "Current"},
    {"textures.default", "Default"},
    {"textures.external_unknown", "External / Unknown"},
    {"textures.checking", "Checking..."},
    {"textures.checking_description", "Reading the current texture state..."},
    {"textures.external_description", "This texture is not managed by TexNX."},
    {"textures.unable_identify", "Unable to identify"},
    {"textures.current_error_description", "The current texture could not be checked."},
    {"textures.current_error_short", "Unavailable"},
    {"textures.current_error", "The current texture could not be determined because a fingerprint check failed."},
    {"textures.empty", "No texture packs found."},
    {"textures.directory_error", "The texture pack directory could not be read."},
    {"textures.default_description", "Minecraft default textures"},
    {"textures.apply_question", "Apply \"{pack}\"?"},
    {"textures.apply_warning", "The current texture files will be replaced."},
    {"textures.restore_question", "Restore default textures?"},
    {"textures.restore_warning", "The current LayeredFS Common folder will be removed."},
    {"textures.apply", "Apply"},
    {"textures.restore", "Restore"},
    {"textures.progress_apply_operation", "Applying texture"},
    {"textures.progress_restore_operation", "Restoring default"},
    {"textures.progress_checking", "Checking files..."},
    {"textures.progress_removing", "Removing current files..."},
    {"textures.progress_creating", "Creating folders..."},
    {"textures.progress_copying", "Applying..."},
    {"textures.progress_saving", "Saving changes..."},
    {"textures.progress_verifying", "Verifying..."},
    {"textures.progress_cleanup", "Removing incomplete files..."},
    {"textures.operation_complete", "Texture change completed successfully."},
    {"textures.operation_failed", "Texture change failed."},
    {"textures.error_source_missing", "The selected pack's Common folder is missing."},
    {"textures.error_preflight", "The selected pack could not be fully checked. The current Common folder was not changed."},
    {"textures.error_destination_check", "The current Common folder could not be safely checked."},
    {"textures.error_delete", "The current Common folder could not be completely removed. No new pack was copied."},
    {"textures.error_create", "The destination folders could not be created."},
    {"textures.error_source_read", "A source file could not be read."},
    {"textures.error_copy", "A texture file could not be written."},
    {"textures.error_commit", "The SD card changes could not be committed."},
    {"textures.error_verify", "The copied Common folder did not match the source pack."},
    {"textures.error_cleanup", "The incomplete Common folder could not be removed."},
    {"textures.error_generic", "The operation could not be completed."},
    {"textures.cleanup_warning", "Some incomplete destination files may remain. Check the LayeredFS Common folder before starting the game."},
    {"settings.title", "Settings"},
    {"settings.description", "Choose the language used by TexNX."},
    {"settings.language", "Language"},
    {"settings.system", "System"},
    {"settings.english", "English"},
    {"settings.japanese", "日本語"},
    {"settings.save_failed", "The language changed for this session, but config.json could not be saved."},
    {"about.title", "About"},
    {"about.summary", "A simple texture manager for Minecraft: Nintendo Switch Edition"},
    {"about.version", "Version"},
    {"about.developer", "Developer"},
    {"about.target", "Target"},
    {"about.title_id", "Title ID"},
    {"about.unofficial", "Unofficial community project. Not affiliated with Nintendo, Mojang Studios, or Microsoft."},
    {"common.back", "Back"},
    {"common.cancel", "Cancel"},
    {"common.ok", "OK"},
    {"common.error_title", "Common directory check failed"},
    {"common.error_body", "TexNX could not read the expected Minecraft LayeredFS Common directory."},
    {"hints.open", "Open"},
    {"hints.apply", "Apply"},
    {"hints.tabs", "Tabs"},
    {"hints.select", "Select"},
    {"hints.exit", "Exit"},
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
