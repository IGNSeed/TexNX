#include "texnx/ui/Theme.hpp"

#include <borealis.hpp>

namespace texnx::ui {

void applyFixedTheme() {
    auto& theme = brls::Theme::getDarkTheme();

    const auto black = nvgRGB(10, 10, 10);
    const auto darkGray = nvgRGB(28, 28, 28);
    const auto gray = nvgRGB(72, 72, 72);
    const auto lightGray = nvgRGB(190, 190, 190);
    const auto white = nvgRGB(245, 245, 245);

    theme.addColor("brls/background", black);
    theme.addColor("brls/text", white);
    theme.addColor("brls/text_disabled", gray);
    theme.addColor("brls/backdrop", nvgRGBA(0, 0, 0, 205));
    theme.addColor("brls/click_pulse", nvgRGBA(255, 255, 255, 36));
    theme.addColor("brls/accent", white);
    theme.addColor("brls/highlight/background", darkGray);
    theme.addColor("brls/highlight/color1", white);
    theme.addColor("brls/highlight/color2", lightGray);
    theme.addColor("brls/applet_frame/separator", gray);
    theme.addColor("brls/sidebar/background", darkGray);
    theme.addColor("brls/sidebar/active_item", white);
    theme.addColor("brls/sidebar/separator", gray);
    theme.addColor("brls/header/border", gray);
    theme.addColor("brls/header/rectangle", lightGray);
    theme.addColor("brls/header/subtitle", lightGray);
    theme.addColor("brls/button/primary_enabled_background", white);
    theme.addColor("brls/button/primary_disabled_background", gray);
    theme.addColor("brls/button/primary_enabled_text", black);
    theme.addColor("brls/button/primary_disabled_text", darkGray);
    theme.addColor("brls/button/default_enabled_background", gray);
    theme.addColor("brls/button/default_disabled_background", darkGray);
    theme.addColor("brls/button/default_enabled_text", white);
    theme.addColor("brls/button/default_disabled_text", lightGray);
    theme.addColor("brls/button/highlight_enabled_text", white);
    theme.addColor("brls/button/highlight_disabled_text", lightGray);
    theme.addColor("brls/button/enabled_border_color", white);
    theme.addColor("brls/button/disabled_border_color", gray);
    theme.addColor("brls/list/listItem_value_color", lightGray);

    theme.addColor("texnx/background", black);
    theme.addColor("texnx/panel", darkGray);
    theme.addColor("texnx/content_background", black);
    theme.addColor("texnx/sidebar_background", nvgRGB(20, 20, 20));
    theme.addColor("texnx/sidebar_selected", nvgRGB(40, 40, 40));
    theme.addColor("texnx/sidebar_accent", white);
    theme.addColor("texnx/card", darkGray);
    theme.addColor("texnx/icon_background", black);
    theme.addColor("texnx/row_selected", nvgRGB(38, 38, 38));
    theme.addColor("texnx/divider", nvgRGB(62, 62, 62));
    theme.addColor("texnx/footer_background", nvgRGB(16, 16, 16));
    theme.addColor("texnx/progress_track", gray);
    theme.addColor("texnx/text", white);
    theme.addColor("texnx/text_secondary", lightGray);
}

} // namespace texnx::ui
