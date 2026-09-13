#pragma once

#include "texnx/config/Config.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <string>

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

class AboutView;
class SettingsView;
class TexturesView;

class MainShellView final : public brls::Box {
public:
    using LanguageChangedCallback =
        std::function<void(config::LanguageMode)>;

    MainShellView(localization::Localization& localization,
                  config::LanguageMode selectedLanguage,
                  std::string version,
                  LanguageChangedCallback languageChanged);

    void refreshText();

private:
    enum class Tab : std::size_t {
        Textures = 0,
        Settings = 1,
        About = 2,
    };

    class SidebarTab;

    void selectTab(Tab tab, bool animate = true);
    void enterSelectedContent();
    void focusSidebar();
    void setApplyInProgress(bool applying);
    [[nodiscard]] brls::View* paneFor(Tab tab) const noexcept;

    localization::Localization& localization_;
    std::string version_;
    std::array<SidebarTab*, 3> tabs_{};
    TexturesView* texturesView_{nullptr};
    SettingsView* settingsView_{nullptr};
    AboutView* aboutView_{nullptr};
    brls::Box* contentHost_{nullptr};
    brls::Hints* footerHints_{nullptr};
    brls::Label* versionLabel_{nullptr};
    Tab selectedTab_{Tab::Textures};
    bool applying_{false};
};

} // namespace texnx::ui
