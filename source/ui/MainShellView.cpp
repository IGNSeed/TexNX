#include "texnx/ui/MainShellView.hpp"

#include "texnx/localization/Localization.hpp"
#include "texnx/ui/AboutView.hpp"
#include "texnx/ui/SettingsView.hpp"
#include "texnx/ui/TexturesView.hpp"
#include "texnx/ui/UiMetrics.hpp"

#include <array>
#include <utility>

#include <borealis/core/touch/tap_gesture.hpp>

namespace texnx::ui {

class MainShellView::SidebarTab final : public brls::Box {
public:
    SidebarTab(std::string icon, std::function<void()> selected,
               std::function<void()> enterContent, const bool canEnter)
        : brls::Box(brls::Axis::ROW), selected_(std::move(selected)),
          enterContent_(std::move(enterContent)), canEnter_(canEnter) {
        setFocusable(true);
        setHeight(metrics::SidebarTabHeight);
        setWidthPercentage(100);
        setShrink(0);
        setAlignItems(brls::AlignItems::CENTER);
        setPadding(0, 14, 0, 8);
        setCornerRadius(metrics::SidebarTabRadius);
        setClipsToBounds(true);
        setHighlightPadding(2);
        setHighlightCornerRadius(metrics::SidebarTabRadius + 2.0F);

        accent_ = new brls::Box(brls::Axis::ROW);
        accent_->setWidth(4);
        accent_->setHeight(34);
        accent_->setShrink(0);
        accent_->setCornerRadius(2);
        accent_->setMarginRight(14);
        addView(accent_);

        icon_ = new brls::Label();
        icon_->setText(icon);
        icon_->setWidth(34);
        icon_->setHeight(42);
        icon_->setShrink(0);
        icon_->setFontSize(27);
        icon_->setSingleLine(true);
        icon_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        addView(icon_);

        label_ = new brls::Label();
        label_->setGrow(1);
        label_->setMinWidth(0);
        label_->setHeight(42);
        label_->setMarginLeft(12);
        label_->setFontSize(23);
        label_->setSingleLine(true);
        label_->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        addView(label_);

        if (canEnter_) {
            registerAction({}, brls::BUTTON_A,
                           [this](brls::View*) {
                               enterContent_();
                               return true;
                           },
                           false, false, brls::SOUND_CLICK);
            registerAction({}, brls::BUTTON_RIGHT,
                           [this](brls::View*) {
                               enterContent_();
                               return true;
                           },
                           true, false, brls::SOUND_FOCUS_CHANGE);
        }

        addGestureRecognizer(new brls::TapGestureRecognizer(
            this, [this] { selected_(); }));
        updateVisual();
    }

    void setText(const std::string& text) {
        label_->setText(text);
    }

    void setSelected(const bool selected) {
        selectedTab_ = selected;
        updateVisual();
    }

    void setOpenHint(const std::string& hint) {
        if (canEnter_) {
            updateActionHint(brls::BUTTON_A, hint);
        }
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        selected_();
        updateVisual();
    }

    void onFocusLost() override {
        brls::Box::onFocusLost();
        updateVisual();
    }

private:
    void updateVisual() {
        auto theme = brls::Application::getTheme();
        setBackgroundColor(selectedTab_ ? theme["texnx/sidebar_selected"]
                                        : brls::TRANSPARENT);
        accent_->setBackgroundColor(selectedTab_ ? theme["texnx/sidebar_accent"]
                                                 : brls::TRANSPARENT);
        const auto textColor =
            selectedTab_ ? theme["texnx/text"]
                         : theme["texnx/text_secondary"];
        icon_->setTextColor(textColor);
        label_->setTextColor(textColor);
    }

    brls::Box* accent_{nullptr};
    brls::Label* icon_{nullptr};
    brls::Label* label_{nullptr};
    std::function<void()> selected_;
    std::function<void()> enterContent_;
    bool selectedTab_{false};
    bool canEnter_{false};
};

MainShellView::MainShellView(localization::Localization& localization,
                             const config::LanguageMode selectedLanguage,
                             std::string version,
                             LanguageChangedCallback languageChanged)
    : brls::Box(brls::Axis::ROW), localization_(localization),
      version_(std::move(version)) {
    setWidthPercentage(100);
    setHeightPercentage(100);
    setBackgroundColor(
        brls::Application::getTheme()["texnx/content_background"]);
    setDefaultFocusedIndex(0);

    auto* sidebar = new brls::Box(brls::Axis::COLUMN);
    sidebar->setWidthPercentage(metrics::SidebarWidthPercent);
    sidebar->setHeightPercentage(100);
    sidebar->setShrink(0);
    sidebar->setPadding(metrics::SidebarPaddingTop,
                        metrics::SidebarPaddingHorizontal, 0,
                        metrics::SidebarPaddingHorizontal);
    sidebar->setBackgroundColor(
        brls::Application::getTheme()["texnx/sidebar_background"]);

    // 将来このbrandHostだけを画像Logoへ置き換えられる構造にする。
    auto* brandHost = new brls::Box(brls::Axis::COLUMN);
    brandHost->setHeight(112);
    brandHost->setShrink(0);

    auto* brand = new brls::Label();
    brand->setText("TexNX");
    brand->setWidthPercentage(100);
    brand->setHeight(54);
    brand->setFontSize(40);
    brand->setSingleLine(true);
    brand->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    brand->setTextColor(brls::Application::getTheme()["texnx/text"]);
    brandHost->addView(brand);

    versionLabel_ = new brls::Label();
    versionLabel_->setWidthPercentage(100);
    versionLabel_->setHeight(30);
    versionLabel_->setFontSize(18);
    versionLabel_->setSingleLine(true);
    versionLabel_->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    versionLabel_->setTextColor(
        brls::Application::getTheme()["texnx/text_secondary"]);
    brandHost->addView(versionLabel_);
    sidebar->addView(brandHost);

    const auto selectTextures = [this] { selectTab(Tab::Textures); };
    const auto selectSettings = [this] { selectTab(Tab::Settings); };
    const auto selectAbout = [this] { selectTab(Tab::About); };
    const auto enter = [this] { enterSelectedContent(); };

    tabs_[0] = new SidebarTab("\uE421", selectTextures, enter, true);
    tabs_[1] = new SidebarTab("\uE8B8", selectSettings, enter, true);
    tabs_[2] = new SidebarTab("\uE88E", selectAbout, enter, false);
    for (auto* tab : tabs_) {
        sidebar->addView(tab);
    }
    sidebar->setDefaultFocusedIndex(1);

    tabs_[0]->setCustomNavigationRoute(brls::FocusDirection::UP, tabs_[0]);
    tabs_[0]->setCustomNavigationRoute(brls::FocusDirection::DOWN, tabs_[1]);
    tabs_[1]->setCustomNavigationRoute(brls::FocusDirection::UP, tabs_[0]);
    tabs_[1]->setCustomNavigationRoute(brls::FocusDirection::DOWN, tabs_[2]);
    tabs_[2]->setCustomNavigationRoute(brls::FocusDirection::UP, tabs_[1]);
    tabs_[2]->setCustomNavigationRoute(brls::FocusDirection::DOWN, tabs_[2]);
    addView(sidebar);

    auto* verticalDivider = new brls::Rectangle(
        brls::Application::getTheme()["texnx/divider"]);
    verticalDivider->setWidth(metrics::SidebarSeparatorWidth);
    verticalDivider->setHeightPercentage(100);
    verticalDivider->setShrink(0);
    addView(verticalDivider);

    auto* rightColumn = new brls::Box(brls::Axis::COLUMN);
    rightColumn->setGrow(1);
    rightColumn->setShrink(1);
    rightColumn->setMinWidth(0);
    rightColumn->setHeightPercentage(100);

    contentHost_ = new brls::Box(brls::Axis::COLUMN);
    contentHost_->setGrow(1);
    contentHost_->setShrink(1);
    contentHost_->setMinHeight(0);
    contentHost_->setWidthPercentage(100);
    contentHost_->setClipsToBounds(true);

    texturesView_ = new TexturesView(localization_);
    settingsView_ = new SettingsView(localization_, selectedLanguage,
                                     std::move(languageChanged));
    aboutView_ = new AboutView(localization_, version_);

    const auto returnToSidebar = [this] { focusSidebar(); };
    texturesView_->setReturnToSidebarCallback(returnToSidebar);
    texturesView_->setApplyStateCallback(
        [this](const bool applying) { setApplyInProgress(applying); });
    settingsView_->setReturnToSidebarCallback(returnToSidebar);

    contentHost_->addView(texturesView_);
    contentHost_->addView(settingsView_);
    contentHost_->addView(aboutView_);
    rightColumn->addView(contentHost_);

    auto* footer = new brls::Box(brls::Axis::ROW);
    footer->setWidthPercentage(100);
    footer->setHeight(metrics::FooterHeight);
    footer->setShrink(0);
    footer->setAlignItems(brls::AlignItems::CENTER);
    footer->setPadding(0, 26, 0, 20);
    footer->setLineTop(1);
    footer->setLineColor(
        brls::Application::getTheme()["texnx/divider"]);
    footer->setBackgroundColor(
        brls::Application::getTheme()["texnx/footer_background"]);

    footerHints_ = new brls::Hints();
    footerHints_->setAddUnableAButtonAction(false);
    footerHints_->setGrow(1);
    footerHints_->setJustifyContent(brls::JustifyContent::FLEX_END);
    footer->addView(footerHints_);
    rightColumn->addView(footer);
    addView(rightColumn);

    registerAction({}, brls::BUTTON_B,
                   [](brls::View*) { return true; }, true, false,
                   brls::SOUND_FOCUS_ERROR);

    settingsView_->setVisibility(brls::Visibility::GONE);
    aboutView_->setVisibility(brls::Visibility::GONE);
    tabs_[0]->setSelected(true);
    texturesView_->onTabActivated();
    refreshText();
}

void MainShellView::refreshText() {
    versionLabel_->setText("v" + version_);
    tabs_[0]->setText(localization_.text("navigation.textures"));
    tabs_[1]->setText(localization_.text("navigation.settings"));
    tabs_[2]->setText(localization_.text("navigation.about"));
    tabs_[0]->setOpenHint(localization_.text("hints.open"));
    tabs_[1]->setOpenHint(localization_.text("hints.open"));

    // global quitはActivity push後にrootへ登録されるため、存在時だけ更新される。
    updateActionHint(brls::BUTTON_START, localization_.text("hints.exit"));
    texturesView_->refreshText();
    settingsView_->refreshText();
    aboutView_->refreshText();
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void MainShellView::selectTab(const Tab tab, const bool animate) {
    if (applying_ || tab == selectedTab_) {
        return;
    }

    if (selectedTab_ == Tab::Textures) {
        texturesView_->onTabDeactivated();
    }

    auto* oldPane = paneFor(selectedTab_);
    oldPane->alpha.stop();
    oldPane->setAlpha(1.0F);
    oldPane->setVisibility(brls::Visibility::GONE);

    selectedTab_ = tab;
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        tabs_[index]->setSelected(index == static_cast<std::size_t>(tab));
    }

    auto* newPane = paneFor(tab);
    newPane->alpha.stop();
    newPane->setVisibility(brls::Visibility::VISIBLE);
    if (animate) {
        // View単体のfadeだけを差し替えるためglobal input blockは発生しない。
        newPane->hide([] {}, false, 0.0F);
        newPane->show([] {}, true, metrics::TabFadeDuration);
    } else {
        newPane->setAlpha(1.0F);
    }

    if (tab == Tab::Textures) {
        texturesView_->onTabActivated();
    }
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void MainShellView::enterSelectedContent() {
    if (applying_) {
        return;
    }
    switch (selectedTab_) {
        case Tab::Textures:
            texturesView_->focusContent();
            break;
        case Tab::Settings:
            settingsView_->focusContent();
            break;
        case Tab::About:
            // Aboutは閲覧専用で、不可視のfocus targetは作らない。
            break;
    }
}

void MainShellView::focusSidebar() {
    if (!applying_) {
        brls::Application::giveFocus(
            tabs_[static_cast<std::size_t>(selectedTab_)]);
    }
}

void MainShellView::setApplyInProgress(const bool applying) {
    applying_ = applying;
    footerHints_->setVisibility(applying ? brls::Visibility::INVISIBLE
                                         : brls::Visibility::VISIBLE);
    if (!applying) {
        brls::Application::getGlobalHintsUpdateEvent()->fire();
    }
}

brls::View* MainShellView::paneFor(const Tab tab) const noexcept {
    switch (tab) {
        case Tab::Textures:
            return texturesView_;
        case Tab::Settings:
            return settingsView_;
        case Tab::About:
            return aboutView_;
    }
    return texturesView_;
}

} // namespace texnx::ui
