#include "texnx/ui/AboutView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"

#include <utility>

namespace texnx::ui {
namespace {

void addInformation(brls::Box& panel, const std::string& label,
                    const std::string& value) {
    auto* heading = components::makeSectionLabel(label);
    heading->setHeight(26);
    heading->setFontSize(19);
    heading->setTextColor(brls::Application::getTheme()["texnx/text_secondary"]);
    panel.addView(heading);

    auto* detail = components::makeSectionLabel(value);
    detail->setHeight(32);
    detail->setFontSize(23);
    detail->setMarginBottom(4);
    panel.addView(detail);
}

} // namespace

AboutView::AboutView(localization::Localization& localization, std::string version)
    : brls::Box(brls::Axis::COLUMN),
      localization_(localization),
      version_(std::move(version)) {
    components::configureScreen(*this);
    components::registerBackAction(*this, localization_.text("common.back"));

    addView(components::makeTitle(localization_.text("about.title")));

    auto* panel = components::makePanel();
    panel->setGrow(1);
    addInformation(*panel, localization_.text("about.version"),
                   "TexNX v" + version_);
    addInformation(*panel, localization_.text("about.developer"), "IGNSeed");
    addInformation(*panel, localization_.text("about.target"),
                   "Minecraft: Nintendo Switch Edition");
    addInformation(*panel, localization_.text("about.title_id"),
                   paths::MinecraftTitleId);

    auto* disclaimer = components::makeBody(localization_.text("about.unofficial"));
    disclaimer->setHeight(72);
    disclaimer->setMarginTop(10);
    panel->addView(disclaimer);

    addView(panel);
    addView(components::makeBackButton(localization_.text("common.back")));
}

} // namespace texnx::ui
