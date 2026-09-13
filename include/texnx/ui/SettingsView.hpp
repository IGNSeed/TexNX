#pragma once

#include "texnx/config/Config.hpp"

#include <array>
#include <functional>

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

class SettingsView final : public brls::Box {
public:
    using LanguageChangedCallback = std::function<void(config::LanguageMode)>;

    SettingsView(localization::Localization& localization,
                 config::LanguageMode selectedLanguage,
                 LanguageChangedCallback languageChanged);
    void refreshText();
    void focusContent();
    void setReturnToSidebarCallback(std::function<void()> callback);

private:
    class LanguageRow;

    void selectLanguage(config::LanguageMode mode);

    localization::Localization& localization_;
    config::LanguageMode selectedLanguage_;
    LanguageChangedCallback languageChanged_;
    brls::Label* title_{nullptr};
    brls::Label* description_{nullptr};
    brls::Label* languageLabel_{nullptr};
    std::array<LanguageRow*, 3> languageRows_{};
    std::function<void()> returnToSidebar_;
    std::size_t lastFocusedIndex_{0};
};

} // namespace texnx::ui
