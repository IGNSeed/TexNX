#pragma once

#include <borealis.hpp>

namespace texnx::localization {
class Localization;
}

namespace texnx::ui {

class TexturesView final : public brls::Box {
public:
    explicit TexturesView(localization::Localization& localization);

private:
    localization::Localization& localization_;
};

} // namespace texnx::ui
