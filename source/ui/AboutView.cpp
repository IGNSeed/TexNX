#include "texnx/ui/AboutView.hpp"

#include "texnx/Paths.hpp"
#include "texnx/localization/Localization.hpp"
#include "texnx/ui/Components.hpp"
#include "texnx/ui/UiMetrics.hpp"

#include <utility>

namespace texnx::ui {
namespace {

brls::Label* addInformationRow(brls::Box& panel, const std::string& value,
                               const bool addDivider = true) {
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setWidthPercentage(100);
    row->setHeight(52);
    row->setShrink(0);
    row->setAlignItems(brls::AlignItems::CENTER);
    if (addDivider) {
        row->setLineBottom(1);
        row->setLineColor(
            brls::Application::getTheme()["texnx/divider"]);
    }

    auto* heading = new brls::Label();
    heading->setWidthPercentage(32);
    heading->setHeight(36);
    heading->setShrink(0);
    heading->setFontSize(18);
    heading->setSingleLine(true);
    heading->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    heading->setTextColor(
        brls::Application::getTheme()["texnx/text_secondary"]);
    row->addView(heading);

    auto* detail = new brls::Label();
    detail->setGrow(1);
    detail->setMinWidth(0);
    detail->setHeight(36);
    detail->setFontSize(20);
    detail->setSingleLine(true);
    detail->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    detail->setTextColor(brls::Application::getTheme()["texnx/text"]);
    detail->setText(value);
    row->addView(detail);
    panel.addView(row);
    return heading;
}

} // namespace

AboutView::AboutView(localization::Localization& localization,
                     std::string version)
    : brls::Box(brls::Axis::COLUMN), localization_(localization),
      version_(std::move(version)) {
    components::configureContentPane(*this);

    title_ = components::makeTitle({});
    addView(title_);

    auto* identity = components::makePanel();
    identity->setHeight(120);
    identity->setShrink(0);
    identity->setJustifyContent(brls::JustifyContent::CENTER);
    identity->setMarginBottom(metrics::SpaceMedium);

    appName_ = new brls::Label();
    appName_->setWidthPercentage(100);
    appName_->setHeight(48);
    appName_->setFontSize(34);
    appName_->setSingleLine(true);
    appName_->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    appName_->setTextColor(brls::Application::getTheme()["texnx/text"]);
    identity->addView(appName_);

    summary_ = components::makeBody({});
    summary_->setHeight(34);
    identity->addView(summary_);
    addView(identity);

    auto* information = components::makePanel();
    information->setPadding(10, 18, 10, 18);
    information->setMaxWidth(820);
    versionLabel_ = addInformationRow(*information, "TexNX v" + version_);
    developerLabel_ = addInformationRow(*information, "IGNSeed");
    targetLabel_ = addInformationRow(
        *information, "Minecraft: Nintendo Switch Edition");
    titleIdLabel_ =
        addInformationRow(*information, paths::MinecraftTitleId, false);
    addView(information);

    disclaimer_ = components::makeBody({});
    disclaimer_->setHeight(68);
    disclaimer_->setMarginTop(metrics::SpaceMedium);
    disclaimer_->setFontSize(18);
    addView(disclaimer_);

    refreshText();
}

void AboutView::refreshText() {
    title_->setText(localization_.text("about.title"));
    appName_->setText("TexNX");
    summary_->setText(localization_.text("about.summary"));
    versionLabel_->setText(localization_.text("about.version"));
    developerLabel_->setText(localization_.text("about.developer"));
    targetLabel_->setText(localization_.text("about.target"));
    titleIdLabel_->setText(localization_.text("about.title_id"));
    disclaimer_->setText(localization_.text("about.unofficial"));
}

} // namespace texnx::ui
