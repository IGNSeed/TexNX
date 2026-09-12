#include "texnx/ui/Components.hpp"

namespace texnx::ui::components {

void configureScreen(brls::Box& screen) {
    screen.setAxis(brls::Axis::COLUMN);
    screen.setAlignItems(brls::AlignItems::CENTER);
    screen.setPadding(44, 72, 40, 72);
    screen.setBackgroundColor(brls::Application::getTheme()["texnx/background"]);
}

brls::Label* makeTitle(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(42);
    label->setHeight(64);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setTextColor(brls::Application::getTheme()["texnx/text"]);
    label->setSingleLine(true);
    return label;
}

brls::Label* makeBody(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(23);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setTextColor(brls::Application::getTheme()["texnx/text_secondary"]);
    label->setSingleLine(false);
    return label;
}

brls::Label* makeSectionLabel(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(28);
    label->setHeight(48);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setTextColor(brls::Application::getTheme()["texnx/text"]);
    label->setSingleLine(true);
    return label;
}

brls::Button* makeButton(const std::string& text) {
    auto* button = new brls::Button();
    button->setText(text);
    button->setStyle(&brls::BUTTONSTYLE_DEFAULT);
    button->setFontSize(25);
    button->setHeight(68);
    button->setWidthPercentage(100);
    button->setCornerRadius(8);
    button->setMarginBottom(14);
    return button;
}

brls::Button* makeBackButton(const std::string& text) {
    auto* button = makeButton(text);
    button->setWidthPercentage(44);
    button->setMaxWidth(380);
    button->setMarginTop(24);
    button->registerClickAction([](brls::View*) {
        return brls::Application::popActivity();
    });
    return button;
}

brls::Box* makePanel() {
    auto* panel = new brls::Box(brls::Axis::COLUMN);
    panel->setWidthPercentage(78);
    panel->setMaxWidth(900);
    panel->setPadding(32);
    panel->setCornerRadius(12);
    panel->setBackgroundColor(brls::Application::getTheme()["texnx/panel"]);
    return panel;
}

void registerBackAction(brls::View& view, const std::string& hint) {
    view.registerAction(hint, brls::BUTTON_B,
                        [](brls::View*) {
                            return brls::Application::popActivity();
                        },
                        true, false, brls::SOUND_BACK);
}

void pushResponsiveActivity(brls::View* view) {
    if (view == nullptr) {
        return;
    }

    // 標準のfade中に行われるglobal input blockを避け、同じfadeだけをView側で再生する。
    const float duration =
        view->getShowAnimationDuration(brls::TransitionAnimation::FADE);
    view->hide([] {}, false, 0.0F);
    brls::Application::pushActivity(new brls::Activity(view),
                                    brls::TransitionAnimation::NONE);
    view->show([] {}, duration > 0.0F, duration);
}

} // namespace texnx::ui::components
