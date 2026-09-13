#include "texnx/ui/Components.hpp"

#include "texnx/ui/UiMetrics.hpp"

namespace texnx::ui::components {

void configureContentPane(brls::Box& pane) {
    pane.setAxis(brls::Axis::COLUMN);
    pane.setGrow(1);
    pane.setShrink(1);
    pane.setMinWidth(0);
    pane.setMinHeight(0);
    pane.setPadding(metrics::ContentPaddingTop,
                    metrics::ContentPaddingHorizontal,
                    metrics::ContentPaddingBottom,
                    metrics::ContentPaddingHorizontal);
    pane.setBackgroundColor(
        brls::Application::getTheme()["texnx/content_background"]);
}

brls::Label* makeTitle(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(metrics::PageTitleSize);
    label->setHeight(metrics::PageTitleHeight);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setTextColor(brls::Application::getTheme()["texnx/text"]);
    label->setSingleLine(true);
    return label;
}

brls::Label* makeBody(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(metrics::BodySize);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setTextColor(
        brls::Application::getTheme()["texnx/text_secondary"]);
    label->setSingleLine(false);
    return label;
}

brls::Label* makeSectionLabel(const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(metrics::SectionTitleSize);
    label->setHeight(metrics::SectionTitleHeight);
    label->setWidthPercentage(100);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setTextColor(brls::Application::getTheme()["texnx/text"]);
    label->setSingleLine(true);
    return label;
}

brls::Box* makePanel() {
    auto* panel = new brls::Box(brls::Axis::COLUMN);
    panel->setWidthPercentage(100);
    panel->setPadding(metrics::CardPadding);
    panel->setCornerRadius(metrics::CardRadius);
    panel->setClipsToBounds(true);
    panel->setBackgroundColor(
        brls::Application::getTheme()["texnx/card"]);
    return panel;
}

brls::Rectangle* makeDivider() {
    auto* divider = new brls::Rectangle(
        brls::Application::getTheme()["texnx/divider"]);
    divider->setWidthPercentage(100);
    divider->setHeight(metrics::DividerSize);
    divider->setShrink(0);
    return divider;
}

} // namespace texnx::ui::components
