#include "texnx/ui/TexturesView.hpp"

#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"

namespace texnx::ui {

TexturesView::TexturesView(localization::Localization& localization)
    : brls::Box(brls::Axis::COLUMN), localization_(localization) {
    components::configureScreen(*this);
    components::registerBackAction(*this, localization_.text("common.back"));

    addView(components::makeTitle(localization_.text("textures.title")));

    auto* panel = components::makePanel();
    panel->setGrow(1);
    panel->setJustifyContent(brls::JustifyContent::CENTER);
    panel->setAlignItems(brls::AlignItems::CENTER);

    auto* placeholderTitle =
        components::makeSectionLabel(localization_.text("textures.placeholder_title"));
    placeholderTitle->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    panel->addView(placeholderTitle);

    auto* placeholderBody =
        components::makeBody(localization_.text("textures.placeholder_body"));
    placeholderBody->setWidthPercentage(82);
    placeholderBody->setHeight(110);
    panel->addView(placeholderBody);

    addView(panel);
    addView(components::makeBackButton(localization_.text("common.back")));
}

} // namespace texnx::ui
