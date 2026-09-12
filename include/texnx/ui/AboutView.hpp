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

private:
    localization::Localization& localization_;
    std::string version_;
};

} // namespace texnx::ui
