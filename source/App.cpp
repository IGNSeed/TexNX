#include "texnx/App.hpp"

#include "texnx/Paths.hpp"
#include "texnx/config/Config.hpp"
#include "texnx/filesystem/FileSystem.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/ui/AboutView.hpp"
#include "texnx/ui/HomeView.hpp"
#include "texnx/ui/SettingsView.hpp"
#include "texnx/ui/TexturesView.hpp"
#include "texnx/ui/Theme.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include <borealis.hpp>

namespace texnx {
namespace {

using filesystem::DirectoryCheckResult;
using filesystem::DirectoryState;

void logConfigState(const config::LoadResult& result) {
    switch (result.state) {
        case config::LoadState::Loaded:
            brls::Logger::info("TexNX config loaded");
            break;
        case config::LoadState::Missing:
            brls::Logger::info("TexNX config is missing; using System language");
            break;
        case config::LoadState::Invalid:
            brls::Logger::warning("TexNX config is invalid; using System language");
            break;
        case config::LoadState::Error:
            brls::Logger::warning("TexNX config could not be read (errno {})", result.posixError);
            break;
    }
}

std::string commonDialogMessage(localization::Localization& localization,
                                const DirectoryCheckResult& result) {
    std::ostringstream message;
    if (result.state == DirectoryState::NotFound) {
        message << localization.text("common.not_found_title") << "\n\n"
                << localization.text("common.not_found_body");
    } else {
        message << localization.text("common.error_title") << "\n\n"
                << localization.text("common.error_body");
    }

    message << "\n\n" << paths::MinecraftCommon;
    if (result.state == DirectoryState::Error) {
        const char* reason = std::strerror(result.posixError);
        message << "\nerrno: " << result.posixError << " ("
                << (reason != nullptr ? reason : "Unknown error") << ')';
        if (result.nativeResult != 0) {
            message << "\nlibnx Result: 0x" << std::uppercase << std::hex
                    << std::setw(8) << std::setfill('0') << result.nativeResult;
        }
    }

    return message.str();
}

void showCommonDialog(localization::Localization& localization,
                      const DirectoryCheckResult& result) {
    if (result.state == DirectoryState::Found) {
        brls::Logger::info("Minecraft Common directory found: {}",
                           paths::MinecraftCommon);
        return;
    }

    auto* dialog = new brls::Dialog(commonDialogMessage(localization, result));
    dialog->addButton(localization.text("common.ok"), [] {});
    dialog->open();
}

} // namespace

int App::run() const {
    auto configResult = config::ConfigStore::load();
    auto currentConfig = configResult.config;
    const std::string systemLocale = localization::Localization::detectSystemLocale();

    localization::Localization localization;
    const bool localizationResourcesLoaded = localization.loadResources();
    localization.select(currentConfig.language, systemLocale);

    // Borealis 自身の初期 translation も起動時の有効言語へ合わせる。
    brls::Platform::APP_LOCALE_DEFAULT = localization.effectiveLocaleTag();
    if (!brls::Application::init()) {
        return 1;
    }

    ui::applyFixedTheme();
    brls::Application::createWindow("TexNX");

    // deko3d の video context は createWindow() で生成されるため、その後に切り替える。
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(true);

    logConfigState(configResult);
    if (!localizationResourcesLoaded) {
        brls::Logger::warning("One or more TexNX translation resources were unavailable; English fallback is active");
    }

    ui::HomeView* home = nullptr;
    const auto openScreen = [&](const ui::Screen screen) {
        switch (screen) {
            case ui::Screen::Textures:
                brls::Application::pushActivity(
                    new brls::Activity(new ui::TexturesView(localization)));
                break;
            case ui::Screen::Settings: {
                auto* settings = new ui::SettingsView(
                    localization, currentConfig.language,
                    [&](const config::LanguageMode language) {
                        currentConfig.language = language;
                        localization.select(language, systemLocale);
                        home->refreshText();

                        const auto saveResult = config::ConfigStore::save(currentConfig);
                        if (!saveResult.succeeded) {
                            brls::Logger::error(
                                "Unable to save TexNX config (errno {}, libnx Result {:#x})",
                                saveResult.posixError, saveResult.nativeResult);
                            brls::Application::notify(
                                localization.text("settings.save_failed"));
                        }
                    });
                brls::Application::pushActivity(new brls::Activity(settings));
                break;
            }
            case ui::Screen::About:
                brls::Application::pushActivity(new brls::Activity(
                    new ui::AboutView(localization, TEXNX_VERSION)));
                break;
        }
    };

    home = new ui::HomeView(localization, openScreen);
    brls::Application::pushActivity(new brls::Activity(home));

    const auto common =
        filesystem::FileSystem::directoryExists(paths::MinecraftCommon);
    showCommonDialog(localization, common);

    while (brls::Application::mainLoop()) {
    }

    return 0;
}

} // namespace texnx
