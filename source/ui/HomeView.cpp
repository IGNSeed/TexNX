#include "texnx/ui/HomeView.hpp"

#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"

#include <utility>

namespace texnx::ui {

HomeView::HomeView(localization::Localization& localization,
                   OpenScreenCallback openScreen)
    : brls::Box(brls::Axis::COLUMN), localization_(localization) {
    components::configureScreen(*this);
    setJustifyContent(brls::JustifyContent::CENTER);

    auto* logo = components::makeTitle("TexNX");
    logo->setFontSize(64);
    logo->setHeight(88);
    addView(logo);

    tagline_ = components::makeBody({});
    tagline_->setHeight(74);
    tagline_->setWidthPercentage(74);
    tagline_->setMaxWidth(900);
    tagline_->setMarginBottom(30);
    addView(tagline_);

    auto* menu = new brls::Box(brls::Axis::COLUMN);
    menu->setWidthPercentage(60);
    menu->setMaxWidth(720);

    texturesButton_ = components::makeButton({});
    settingsButton_ = components::makeButton({});
    aboutButton_ = components::makeButton({});

    texturesButton_->registerClickAction([openScreen](brls::View*) {
        openScreen(Screen::Textures);
        return true;
    });
    settingsButton_->registerClickAction([openScreen](brls::View*) {
        openScreen(Screen::Settings);
        return true;
    });
    aboutButton_->registerClickAction([openScreen = std::move(openScreen)](brls::View*) {
        openScreen(Screen::About);
        return true;
    });

    menu->addView(texturesButton_);
    menu->addView(settingsButton_);
    menu->addView(aboutButton_);
    addView(menu);

    refreshText();
}

void HomeView::refreshText() {
    tagline_->setText(localization_.text("home.tagline"));
    texturesButton_->setText(localization_.text("home.textures"));
    settingsButton_->setText(localization_.text("home.settings"));
    aboutButton_->setText(localization_.text("home.about"));
}

} // namespace texnx::ui
