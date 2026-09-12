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

private:
    void selectLanguage(config::LanguageMode mode);

    localization::Localization& localization_;
    config::LanguageMode selectedLanguage_;
    LanguageChangedCallback languageChanged_;
    brls::Label* title_{nullptr};
    brls::Label* description_{nullptr};
    brls::Label* languageLabel_{nullptr};
    std::array<brls::Button*, 3> languageButtons_{};
    brls::Button* backButton_{nullptr};
};

} // namespace texnx::ui
