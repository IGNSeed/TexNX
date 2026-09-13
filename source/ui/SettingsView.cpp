#include "texnx/ui/SettingsView.hpp"

#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"
#include "texnx/ui/UiMetrics.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <borealis/core/touch/tap_gesture.hpp>

namespace texnx::ui {
namespace {

constexpr std::array<std::string_view, 3> LanguageKeys{
    "settings.system",
    "settings.english",
    "settings.japanese",
};

} // namespace

class SettingsView::LanguageRow final : public brls::Box {
public:
    LanguageRow(std::function<void()> selected,
                std::function<void()> focused)
        : brls::Box(brls::Axis::ROW), selected_(std::move(selected)),
          focused_(std::move(focused)) {
        setFocusable(true);
        setHeight(66);
        setWidthPercentage(100);
        setShrink(0);
        setAlignItems(brls::AlignItems::CENTER);
        setPadding(0, 18, 0, 18);
        setHighlightPadding(2);
        setHighlightCornerRadius(9);
        setLineBottom(1);
        setLineColor(brls::Application::getTheme()["texnx/divider"]);

        label_ = new brls::Label();
        label_->setGrow(1);
        label_->setMinWidth(0);
        label_->setHeight(40);
        label_->setFontSize(22);
        label_->setSingleLine(true);
        label_->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        label_->setTextColor(brls::Application::getTheme()["texnx/text"]);
        addView(label_);

        radio_ = new brls::Label();
        radio_->setWidth(38);
        radio_->setHeight(40);
        radio_->setShrink(0);
        radio_->setFontSize(24);
        radio_->setSingleLine(true);
        radio_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        addView(radio_);

        registerAction({}, brls::BUTTON_A,
                       [this](brls::View*) {
                           selected_();
                           return true;
                       },
                       false, false, brls::SOUND_CLICK);
        addGestureRecognizer(new brls::TapGestureRecognizer(this));
    }

    void setText(const std::string& text) {
        label_->setText(text);
    }

    void setSelected(const bool selected) {
        radio_->setText(selected ? "●" : "○");
        radio_->setTextColor(
            brls::Application::getTheme()[selected ? "texnx/text"
                                                    : "texnx/text_secondary"]);
        setBackgroundColor(
            selected ? brls::Application::getTheme()["texnx/row_selected"]
                     : brls::TRANSPARENT);
    }

    void setSelectHint(const std::string& hint) {
        updateActionHint(brls::BUTTON_A, hint);
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        focused_();
    }

private:
    brls::Label* label_{nullptr};
    brls::Label* radio_{nullptr};
    std::function<void()> selected_;
    std::function<void()> focused_;
};

SettingsView::SettingsView(localization::Localization& localization,
                           const config::LanguageMode selectedLanguage,
                           LanguageChangedCallback languageChanged)
    : brls::Box(brls::Axis::COLUMN), localization_(localization),
      selectedLanguage_(selectedLanguage),
      languageChanged_(std::move(languageChanged)) {
    components::configureContentPane(*this);

    title_ = components::makeTitle({});
    addView(title_);

    description_ = components::makeBody({});
    description_->setHeight(38);
    description_->setMarginBottom(18);
    addView(description_);

    languageLabel_ = components::makeSectionLabel({});
    languageLabel_->setMarginBottom(8);
    addView(languageLabel_);

    auto* panel = components::makePanel();
    panel->setWidthPercentage(74);
    panel->setMaxWidth(700);
    panel->setPadding(8);

    for (std::size_t index = 0; index < languageRows_.size(); ++index) {
        const auto mode = config::fromSelectionIndex(static_cast<int>(index));
        auto* row = new LanguageRow(
            [this, mode] { selectLanguage(mode); },
            [this, index] { lastFocusedIndex_ = index; });
        languageRows_[index] = row;
        panel->addView(row);
    }
    languageRows_.back()->setLineBottom(0);
    languageRows_[0]->setCustomNavigationRoute(brls::FocusDirection::UP,
                                                languageRows_[0]);
    languageRows_[0]->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                                                languageRows_[1]);
    languageRows_[1]->setCustomNavigationRoute(brls::FocusDirection::UP,
                                                languageRows_[0]);
    languageRows_[1]->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                                                languageRows_[2]);
    languageRows_[2]->setCustomNavigationRoute(brls::FocusDirection::UP,
                                                languageRows_[1]);
    languageRows_[2]->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                                                languageRows_[2]);
    panel->setDefaultFocusedIndex(0);
    addView(panel);

    registerAction({}, brls::BUTTON_B,
                   [this](brls::View*) {
                       if (returnToSidebar_) {
                           returnToSidebar_();
                       }
                       return true;
                   },
                   false, false, brls::SOUND_BACK);
    registerAction({}, brls::BUTTON_LEFT,
                   [this](brls::View*) {
                       if (returnToSidebar_) {
                           returnToSidebar_();
                       }
                       return true;
                   },
                   true, false, brls::SOUND_FOCUS_CHANGE);
    setDefaultFocusedIndex(3);
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
    updateActionHint(brls::BUTTON_B, localization_.text("hints.tabs"));

    const int selectedIndex = config::toSelectionIndex(selectedLanguage_);
    for (std::size_t index = 0; index < languageRows_.size(); ++index) {
        languageRows_[index]->setText(localization_.text(LanguageKeys[index]));
        languageRows_[index]->setSelected(
            static_cast<int>(index) == selectedIndex);
        languageRows_[index]->setSelectHint(
            localization_.text("hints.select"));
    }
}

void SettingsView::focusContent() {
    const auto index =
        std::min(lastFocusedIndex_, languageRows_.size() - 1U);
    brls::Application::giveFocus(languageRows_[index]);
}

void SettingsView::setReturnToSidebarCallback(
    std::function<void()> callback) {
    returnToSidebar_ = std::move(callback);
}

} // namespace texnx::ui
