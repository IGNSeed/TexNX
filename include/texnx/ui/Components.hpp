#pragma once

#include <string>

#include <borealis.hpp>

namespace texnx::ui::components {

void configureScreen(brls::Box& screen);
[[nodiscard]] brls::Label* makeTitle(const std::string& text);
[[nodiscard]] brls::Label* makeBody(const std::string& text);
[[nodiscard]] brls::Label* makeSectionLabel(const std::string& text);
[[nodiscard]] brls::Button* makeButton(const std::string& text);
[[nodiscard]] brls::Button* makeBackButton(const std::string& text);
[[nodiscard]] brls::Box* makePanel();
void registerBackAction(brls::View& view, const std::string& hint);
void pushResponsiveActivity(brls::View* view);

} // namespace texnx::ui::components
