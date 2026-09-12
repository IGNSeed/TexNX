#include "texnx/ui/SettingsView.hpp"

#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace texnx::ui {
namespace {

constexpr std::array<std::string_view, 3> LanguageKeys{
    "settings.system",
    "settings.english",
    "settings.japanese",
};

} // namespace

SettingsView::SettingsView(localization::Localization& localization,
                           const config::LanguageMode selectedLanguage,
                           LanguageChangedCallback languageChanged)
    : brls::Box(brls::Axis::COLUMN),
      localization_(localization),
      selectedLanguage_(selectedLanguage),
      languageChanged_(std::move(languageChanged)) {
    components::configureScreen(*this);
    components::registerBackAction(*this, localization_.text("common.back"));

    title_ = components::makeTitle({});
    addView(title_);

    description_ = components::makeBody({});
    description_->setHeight(54);
    description_->setMarginBottom(16);
    addView(description_);

    auto* panel = components::makePanel();
    languageLabel_ = components::makeSectionLabel({});
    languageLabel_->setMarginBottom(10);
    panel->addView(languageLabel_);

    for (int index = 0; index < static_cast<int>(languageButtons_.size()); ++index) {
        auto* button = components::makeButton({});
        const auto mode = config::fromSelectionIndex(index);
        button->registerClickAction([this, mode](brls::View*) {
            selectLanguage(mode);
            return true;
        });
        languageButtons_[static_cast<std::size_t>(index)] = button;
        panel->addView(button);
    }

    addView(panel);
    backButton_ = components::makeBackButton({});
    addView(backButton_);
    refreshText();
}

void SettingsView::selectLanguage(const config::LanguageMode mode) {
    selectedLanguage_ = mode;
    languageChanged_(mode);
    refreshText();
}

void SettingsView::refreshText() {
    title_->setText(localization_.text("settings.title"));
    description_->setText(localization_.text("settings.description"));
    languageLabel_->setText(localization_.text("settings.language"));
    backButton_->setText(localization_.text("common.back"));

    const int selectedIndex = config::toSelectionIndex(selectedLanguage_);
    for (int index = 0; index < static_cast<int>(languageButtons_.size()); ++index) {
        auto* button = languageButtons_[static_cast<std::size_t>(index)];
        button->setText(localization_.text(LanguageKeys[static_cast<std::size_t>(index)]));
        button->setStyle(index == selectedIndex ? &brls::BUTTONSTYLE_PRIMARY
                                                : &brls::BUTTONSTYLE_DEFAULT);
    }
}

} // namespace texnx::ui
