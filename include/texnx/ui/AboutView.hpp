#pragma once

#include <string>

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

class AboutView final : public brls::Box {
public:
    AboutView(localization::Localization& localization, std::string version);
    void refreshText();

private:
    localization::Localization& localization_;
    std::string version_;
    brls::Label* title_{nullptr};
    brls::Label* appName_{nullptr};
    brls::Label* summary_{nullptr};
    brls::Label* versionLabel_{nullptr};
    brls::Label* developerLabel_{nullptr};
    brls::Label* targetLabel_{nullptr};
    brls::Label* titleIdLabel_{nullptr};
    brls::Label* disclaimer_{nullptr};
};

} // namespace texnx::ui
