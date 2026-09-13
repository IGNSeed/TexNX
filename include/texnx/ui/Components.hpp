#pragma once

#include <string>

#include <borealis.hpp>

namespace texnx::ui::components {

void configureContentPane(brls::Box& pane);
[[nodiscard]] brls::Label* makeTitle(const std::string& text);
[[nodiscard]] brls::Label* makeBody(const std::string& text);
[[nodiscard]] brls::Label* makeSectionLabel(const std::string& text);
[[nodiscard]] brls::Box* makePanel();
[[nodiscard]] brls::Rectangle* makeDivider();

} // namespace texnx::ui::components
